#include "replication.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "cheat.h"
#include "config.h"
#include "coop.h"
#include "difficulty.h"
#include "gauge.h"
#include "interact.h"
#include "lang.h"
#include "log.h"
#include "memory.h"
#include "network.h"
#include "revive.h"
#include "roster.h"
#include "rules.h"
#include "spawns.h"
#include "timeline.h"

using namespace oni;

namespace rep {

// Le pantin d'un autre joueur s'appelle mp_remote suivi du numéro de ce joueur.
static const char* const kRemotePrefix = "mp_remote";

static const double kStateTimeout = 5.0;      // sans nouvel état : le pantin est retiré
// Les pantins sont affichés avec un léger retard : on connaît ainsi l'état suivant et l'on
// interpole exactement entre deux paquets au lieu de courir après le dernier reçu.
static const double kPlayerDelayTicks = 6.0;  // 100 ms, soit 3 paquets à 30 Hz
static const double kNpcDelayTicks = 10.0;    // les PNJ sont envoyés deux fois moins souvent
static const int kFrameTolerance = 1;         // dérive de frame d'animation tolérée
static const int32_t kSpawnDelayTicks = 60;   // ne rien créer pendant la première seconde du niveau
static const double kSpawnRetry = 2.0;
static const int kMaxNpcSpawnTries = 3;
static const double kSnapshotFresh = 1.0;     // un PNJ absent des snapshots depuis ce délai est "inconnu"
static const double kOrphanDelay = 3.0;       // client : PNJ local inconnu de l'hôte -> supprimé
static const double kPendingDamageTime = 0.5; // temps laissé à l'hôte pour renvoyer la santé à jour
static const double kPassiveInterval = 5.0;   // les scripts de niveau peuvent réactiver l'IA
static const double kThrowPendingTime = 1.0;  // entre l'annonce d'une projection et l'animation du pantin
static const double kFallCatchUp = 1.5;       // chute jouée ici : temps laissé à l'émetteur pour la montrer
static const double kReactionWindow = 0.25;   // entre un coup sur le pantin du joueur et l'animation qu'il lui impose
static const int kReactionInterp = 4;         // fondu vers une réaction imposée par l'autre machine
static const double kReloadRetry = 0.6;       // temps laissé au pantin pour avoir en main l'arme qu'il recharge
// Joueur à terre : les PNJ restés autour de lui sont dirigés vers un joueur debout.
static const double kRetargetInterval = 0.5;
static const float kDownedRadius = 600.0f;    // PNJ concernés : à cette distance d'un joueur à terre, tireurs compris
static const float kRetargetRange = 900.0f;   // joueur debout qu'on leur désigne : à cette distance d'eux
static const float kLabelRange = 300.0f;      // au-delà, pas d'identifiant sur une arme au sol
// Arme des PNJ : l'hôte dit ce que chacun tient, le client y ramène les siens.
static const double kNpcWeaponInterval = 0.25;  // l'hôte regarde à ce rythme si la liste a changé
static const double kNpcWeaponRefresh = 3.0;    // et la renvoie même inchangée
static const int kMaxNpcWeapons = 64;           // PNJ armés par message
static const double kNpcWeaponRetry = 0.4;      // entre deux essais pour donner à un PNJ l'arme annoncée
static const int kMaxNpcWeaponTries = 5;
static const float kNpcDropRadius = 40.0f;      // arme que le PNJ vient de laisser tomber à ses pieds
static const float kWeaponLabelHeight = 4.0f; // au-dessus de l'arme
static const float kWeaponLabelScale = 0.8f;

struct Puppet {
    Character* chr = nullptr;
    uint16_t handle = 0;

    timeline::Buffer timeline;     // états reçus
    proto::EntityState latest = {};
    bool hasState = false;
    double lastSeen = 0.0;

    void* appliedAnim = nullptr;   // ce que le moteur a réellement posé après notre demande
    uint32_t appliedHash = 0;

    // Projection annoncée par l'émetteur : la cible à donner au pantin quand il en pose l'animation.
    Character* throwTarget = nullptr;
    uint16_t throwTargetHandle = 0;
    void* throwTargetAnim = nullptr;
    uint32_t throwAnim = 0;
    double throwExpires = 0.0;

    // Animation subie par le pantin et décidée ici (chute d'une projection, réaction à un coup) :
    // le moteur local la joue déjà.
    uint32_t fallAnim = 0;         // 0 : aucune
    bool fallSeen = false;         // l'émetteur l'a montrée à son tour
    double fallUntil = 0.0;

    // Gâchettes de l'entité distante telles qu'appliquées au pantin.
    bool fire = false;
    bool altFire = false;
    // Rechargement de l'entité distante : vu à l'image précédente, et jusqu'à quand le jouer.
    bool reloading = false;
    double reloadUntil = 0.0;
    // Aura, bouclier et invisibilité de l'entité distante, tels que posés sur le pantin à l'image
    // précédente : ce que le moteur y ajoute ensuite de lui-même est un don fait au pantin.
    uint8_t auraShown = 0;
    uint16_t shieldSet = 0;
    uint16_t cloakSet = 0;
    double auraCheckAt = 0.0;      // quand regarder si le moteur montre bien l'aura demandée

    // Pantin d'un joueur uniquement : dernier coup qui lui a été porté ici, et par qui : notre
    // joueur (tir ami), ou chez l'hôte un PNJ.
    double hitAt = -1000.0;
    bool hitByPlayer = false;

    // PNJ uniquement
    // Arme qu'il tient chez l'hôte : son nom, vide s'il n'en a pas ; rangée ou en main.
    char wantedWeapon[12] = {};
    bool wantedHolstered = false;
    void* ownWeapon = nullptr;     // dernière arme vue dans ses mains : la sienne, où qu'elle tombe
    // Un joueur vient de lui prendre cette arme : on ne lui en redonne pas tant que l'hôte ne l'a
    // pas dit à son tour.
    char disarmed[12] = {};
    double disarmedUntil = 0.0;
    bool ghost = false;            // pantin d'un joueur : ne heurte plus rien (à terre, ou cinématique)
    bool weaponKnown = false;
    double nextWeaponFix = 0.0;
    int weaponTries = 0;
    int32_t pendingDamage = 0;     // coups de notre joueur annoncés à l'hôte, pas encore dans sa santé
    double pendingUntil = 0.0;
    double nextSpawnTry = 0.0;
    int spawnTries = 0;
    bool killed = false;
    double lostAt = 0.0;           // son personnage d'ici a disparu à cet instant ; 0 : il n'en a jamais eu
};

// Pantins des autres joueurs, par numéro de joueur. La case de notre propre numéro reste vide.
static Puppet g_remotes[proto::kMaxPlayers];
static unsigned g_frame = 0;            // image en cours : ce qui se trace une fois par image s'y repère

static float DistanceSquared(const Vector3& a, const Vector3& b);

// Hôte : dernière liste des armes des PNJ envoyée, pour ne la renvoyer que si elle change.
static std::string g_sentNpcWeapons;
static double g_nextNpcWeapons = 0.0;
static double g_nextNpcWeaponsRefresh = 0.0;
static double g_nextRemoteSpawn[proto::kMaxPlayers] = {};
static bool g_levelMismatchReported[proto::kMaxPlayers] = {};
static double g_now = 0.0;              // instant de la frame en cours, pour le filtre de dégâts

// Vie de chaque PNJ, par numéro de case : son handle, le maximum posé, celui qu'il avait à sa
// création, et ce que la difficulté du jeu d'origine y multipliait alors.
struct ScaledHealth {
    uint16_t handle;
    uint32_t maxHealth;
    uint32_t baseMax;
    float spawnFactor;
};
static ScaledHealth g_scaled[off::kMaxCharacters];

// Client : coups et réactions décidés par l'hôte pour notre joueur. Ils attendent que le PNJ qui
// les porte, affiché ici avec retard, en soit au même point.
struct Incoming {
    double at;
    bool isHit;
    proto::Hit hit;
    uint32_t animID;
};
static std::vector<Incoming> g_incoming;

static std::unordered_map<uint32_t, Puppet> g_npcs;            // par characterID
static std::unordered_map<uint32_t, std::string> g_npcNames;
static std::unordered_map<uint16_t, double> g_orphanSince;     // par handle
// Machine qui simule les PNJ : l'arme de chacun, pour qu'il n'en ait jamais deux (voir
// GuardNpcWeapons).
struct NpcArm {
    void* held = nullptr;      // arme en main ou rangée à l'image précédente
    void* dropped = nullptr;   // celle qu'il a lâchée, tant qu'elle est au sol
};
static std::unordered_map<uint16_t, NpcArm> g_npcArms;         // par handle
// Qui tenait chaque arme de la table à l'image précédente, et ce que notre joueur avait en main :
// de quoi voir une arme passer des mains d'un PNJ aux siennes (voir WatchDisarm).
static Character* g_weaponOwner[addr::kMaxWeapons];
static void* g_handBefore = nullptr;
static bool g_ownersSeen = false;
static const double kDisarmHold = 3.0;         // temps laissé à l'hôte pour dire le PNJ désarmé
static bool g_weaponExisted[addr::kMaxWeapons];                // cases de la table occupées à l'image précédente
static bool g_weaponsSeen = false;                             // ce relevé a été fait au moins une fois
// Cinématique du niveau en cours : les pantins des autres joueurs sont cachés et ne heurtent rien.
static bool g_cutscene = false;
// Client : PNJ d'ici inconnus de l'hôte alors qu'il en connaît un du même nom. Des doubles : ils
// ne sont pas tracés, et sont retirés sans attendre le délai des autres.
static std::unordered_map<uint16_t, char> g_doubles;           // par handle
// Hôte : PNJ gardés actifs par nous, parce qu'un autre joueur est près d'eux.
static std::unordered_map<uint16_t, char> g_activeLocked;      // par handle
static const float kActiveRange = 600.0f;      // PNJ à cette distance d'un autre joueur : gardé actif
static const float kActiveRelease = 800.0f;    // plus loin que cela de tous : rendu au moteur
static const double kTwinDelay = 0.4;          // client : délai avant de retirer un double
static const double kRespawnProof = 0.5;       // client : PNJ disparu d'ici, recréé si l'hôte le donne encore après ce délai
static const double kControlGrace = 5.0;       // client : joueur resté aux mains de l'IA de déplacement hors cinématique
static double g_lastSnapshot = -1000.0;
static double g_nextPassive = 0.0;
static bool g_passiveApplied = false;

// Projection en cours de chaque personnage simulé ici, par handle : une annonce par projection.
struct ThrowWatch {
    void* anim;
    Character* target;
};
static std::unordered_map<uint16_t, ThrowWatch> g_throwWatch;

static void* g_potentiallyHostile = nullptr;  // fonction d'origine du moteur, voir InstallAiHook

// -------------------------------------------------------------------------------------------------
// Outils
// -------------------------------------------------------------------------------------------------
static bool Valid(const Puppet& puppet) {
    return puppet.chr && mem::InUse(puppet.chr) && mem::Handle(puppet.chr) == puppet.handle;
}

static void Bind(Puppet& puppet, Character* chr) {
    puppet.chr = chr;
    puppet.handle = mem::Handle(chr);
    puppet.appliedAnim = nullptr;
    puppet.appliedHash = 0;
    puppet.throwTarget = nullptr;
    puppet.fallAnim = 0;
    puppet.fire = false;
    puppet.altFire = false;
    puppet.pendingDamage = 0;
    puppet.killed = false;
    puppet.lostAt = 0.0;
    puppet.ownWeapon = nullptr;
    puppet.disarmedUntil = 0.0;
    puppet.ghost = false;
    puppet.auraShown = 0;
    puppet.shieldSet = 0;
    puppet.cloakSet = 0;
    puppet.auraCheckAt = 0.0;
}

static bool SendReliableMessage(uint8_t kind, const void* data, size_t size) {
    uint8_t packet[64];
    if (size > sizeof(packet) - 1) return false;
    packet[0] = kind;
    memcpy(packet + 1, data, size);
    return net::SendReliable(packet, static_cast<int>(1 + size));
}

static uint32_t LocalPlayerId() { return net::LocalPlayerId(); }

// Numéro du joueur que ce pantin représente, -1 pour celui d'un PNJ.
static int PlayerOf(const Puppet& puppet) {
    return &puppet >= g_remotes && &puppet < g_remotes + proto::kMaxPlayers ? static_cast<int>(&puppet - g_remotes) : -1;
}

// Pantin de joueur qui tient ce personnage, ou nullptr.
static Puppet* RemoteOf(Character* chr) {
    if (!chr) return nullptr;
    for (Puppet& remote : g_remotes) {
        if (remote.chr == chr && Valid(remote)) return &remote;
    }
    return nullptr;
}

// Un état dont la position ou l'orientation n'est pas un nombre n'est jamais donné à un pantin : il
// en ferait autant du personnage, et de proche en proche de notre joueur.
static bool SoundState(const proto::EntityState& state) {
    return std::isfinite(state.x) && std::isfinite(state.y) && std::isfinite(state.z) && std::isfinite(state.rotation);
}

static void PushState(Puppet& puppet, const proto::EntityState& state, double now) {
    if (!SoundState(state)) {
        static double nextLog = 0.0;
        if (now >= nextLog) {
            nextLog = now + 2.0;
            LOG("NAN : etat recu avec une position invalide (personnage %08X), ecarte", state.characterID);
        }
        return;
    }
    puppet.latest = state;
    puppet.hasState = true;
    puppet.lastSeen = now;
    timeline::Push(puppet.timeline, state, now);
}

static bool IsRemotePlayer(Character* chr) { return RemoteOf(chr) != nullptr; }

bool IsPlayerCharacter(Character* chr) { return chr && (chr == mem::Player() || IsRemotePlayer(chr)); }

// Joueur à terre, le nôtre ou celui d'une autre machine : il attend d'être ranimé.
static bool DownedPlayer(Character* chr) {
    if (!chr) return false;
    if (chr == mem::Player()) return revive::Downed();
    const Puppet* remote = RemoteOf(chr);
    return remote && remote->hasState && (remote->latest.flags & proto::kEntityDowned) != 0;
}

// Machine qui simule les PNJ : l'hôte, ou chacun pour soi sans PNJ partagés.
static bool SimulatesNpcs() {
    cfg::Mode mode = net::CurrentMode();
    return mode == cfg::Mode::Host || (mode == cfg::Mode::Client && !cfg::Get().npcs);
}

// Ennemi des joueurs, par son équipe.
static bool HostileToPlayers(Character* chr) {
    Character* player = mem::Player();
    return player && fn::FriendOrFoe(field<uint16_t>(chr, off::kChTeam), field<uint16_t>(player, off::kChTeam)) == 2;
}

static bool AnyRemotePlayer() {
    for (const Puppet& remote : g_remotes) {
        if (Valid(remote)) return true;
    }
    return false;
}

// PNJ répliqués : personnages IA nommés, hors pantins des autres joueurs.
static bool IsReplicatedNpc(Character* chr) {
    if (!mem::InUse(chr) || !mem::IsAI(chr)) return false;
    if (chr == mem::Player() || IsRemotePlayer(chr)) return false;
    const char* name = mem::Name(chr);
    return name[0] != '\0' && strncmp(name, kRemotePrefix, strlen(kRemotePrefix)) != 0;
}

struct LocalNpc {
    Character* chr;
    uint32_t nameHash;
    uint32_t id;
};

// Identifiant stable entre machines : hash du nom + rang parmi les homonymes.
static void ListLocalNpcs(std::vector<LocalNpc>& out) {
    out.clear();
    int count = mem::CharacterCount();
    for (int i = 0; i < count; i++) {
        Character* chr = mem::CharacterAt(i);
        if (!chr || !IsReplicatedNpc(chr)) continue;
        uint32_t hash = mem::Fnv1a(mem::Name(chr), 32);
        uint32_t rank = 0;
        for (const LocalNpc& other : out) {
            if (other.nameHash == hash) rank++;
        }
        out.push_back({chr, hash, hash + rank});
    }
}

// Écart entre deux frames d'une animation qui boucle.
static int FrameDistance(int a, int b, int duration) {
    int distance = std::abs(a - b);
    return duration > 0 && duration - distance < distance ? duration - distance : distance;
}

static Puppet* FindNpcPuppet(Character* chr, uint32_t* id) {
    for (auto& entry : g_npcs) {
        if (!Valid(entry.second) || entry.second.chr != chr) continue;
        if (id) *id = entry.first;
        return &entry.second;
    }
    return nullptr;
}

static bool LocalNpcId(Character* chr, uint32_t& id) {
    static std::vector<LocalNpc> locals;
    ListLocalNpcs(locals);
    for (const LocalNpc& local : locals) {
        if (local.chr != chr) continue;
        id = local.id;
        return true;
    }
    return false;
}

static void ReleaseTriggers(Puppet& puppet) {
    if (puppet.fire) mem::Trigger(puppet.chr, mem::TriggerEvent::Released);
    if (puppet.altFire) mem::Trigger(puppet.chr, mem::TriggerEvent::AltReleased);
    puppet.fire = false;
    puppet.altFire = false;
}

// Ce qui se voit sur l'entité distante sans être dans son animation : l'aura d'un personnage
// surpuissant, la lueur de son bouclier de force, son invisibilité. Le moteur les tire de la santé
// et de l'inventaire du personnage, et un pantin n'a pas ceux de l'entité qu'il représente : on lui
// donne ce que l'autre machine montre. Il en tire aussi le reste : un bouclier arrête les tirs, les
// IA ne voient pas un personnage invisible. Jusqu'à la 0.23 seule l'aura voyageait, en huit
// niveaux, et rien au journal ne disait ce qu'il en advenait ici.
static void ApplyEffects(Puppet& puppet, const timeline::Pose& pose) {
    Character* chr = puppet.chr;
    int player = PlayerOf(puppet);
    bool aura = pose.super != 0, shield = pose.shield != 0, cloak = pose.cloak != 0;
    if (player >= 0) {
        if (aura != (puppet.auraShown != 0)) {
            LOG("aura : %s sur '%s' d'apres sa machine (%u sur 255)", aura ? "allumee" : "eteinte", mem::Name(chr),
                static_cast<unsigned>(pose.super));
            puppet.auraCheckAt = aura ? g_now + 1.0 : 0.0;
        }
        if (shield != (puppet.shieldSet != 0)) {
            LOG("bouclier : '%s' %s d'apres sa machine (%u)", mem::Name(chr), shield ? "en porte un" : "n'en a plus",
                static_cast<unsigned>(pose.shield));
        }
        if (cloak != (puppet.cloakSet != 0)) {
            LOG("invisibilite : '%s' %s d'apres sa machine", mem::Name(chr), cloak ? "est invisible" : "est de nouveau visible");
        }
        // Une seconde après : le moteur la montre-t-il ? Sinon c'est son tracé qui manque, pas la
        // transmission.
        if (puppet.auraCheckAt != 0.0 && g_now >= puppet.auraCheckAt) {
            puppet.auraCheckAt = 0.0;
            LOG("aura : '%s' : le moteur la montre ici a %.2f (demandee %.2f)%s", mem::Name(chr), mem::SuperShown(chr),
                pose.super / 255.0f, mem::Active(chr) ? "" : ", personnage inactif");
        }
        // Avant d'y récrire : ce qu'un PNJ d'ici vient de donner à ce pantin revient à son joueur.
        interact::WatchGifts(static_cast<uint8_t>(player), chr, puppet.shieldSet, puppet.cloakSet);
    }
    mem::SetSuper(chr, pose.super / 255.0f);
    uint16_t shieldNow = pose.shield;
    uint16_t cloakNow = static_cast<uint16_t>(pose.cloak * proto::kCloakStep);
    mem::SetShield(chr, shieldNow);
    mem::SetCloak(chr, cloakNow);
    puppet.auraShown = pose.super;
    puppet.shieldSet = shieldNow;
    puppet.cloakSet = cloakNow;
}

// Reproduit sur le pantin la visée et la gâchette de l'entité distante. Le pantin tire alors de
// vrais projectiles : c'est ce qui donne les tirs à voir et à entendre. Ce qu'ils blessent est
// décidé ailleurs (voir FilterDamage).
static void DriveWeapon(Puppet& puppet, const timeline::Pose& pose) {
    Character* chr = puppet.chr;
    bool player = PlayerOf(puppet) >= 0;
    mem::SyncWeaponVariant(chr);
    if (pose.hasAim) mem::SetAim(chr, pose.aimYaw, pose.aimPitch);
    else if (!player) mem::ClearAim(chr);  // PNJ dont l'IA ne vise plus

    bool fire = (pose.flags & proto::kEntityFire) != 0;
    bool altFire = (pose.flags & proto::kEntityAltFire) != 0;

    // Joueur : appui, maintien, relâchement, dans l'ordre où le traitement des touches les pose.
    // PNJ : son IA redemande le tir à chaque tick tant qu'elle tire (0x490AD9).
    if (fire && (!puppet.fire || !player)) mem::Trigger(chr, mem::TriggerEvent::Pressed);
    else if (fire) mem::Trigger(chr, mem::TriggerEvent::Held);
    else if (puppet.fire) mem::Trigger(chr, mem::TriggerEvent::Released);

    if (altFire && !puppet.altFire) mem::Trigger(chr, mem::TriggerEvent::AltPressed);
    else if (!altFire && puppet.altFire) mem::Trigger(chr, mem::TriggerEvent::AltReleased);

    puppet.fire = fire;
    puppet.altFire = altFire;

    // Rechargement : une animation jouée par-dessus celle du corps, que l'état ne porte que par ce
    // drapeau. Elle est posée quand il se lève ; si le pantin n'a pas encore l'arme en main (elle
    // lui est donnée à part), on réessaie un instant. Le journal dit ce qu'il en a été, une ligne
    // par rechargement.
    bool reloading = (pose.flags & proto::kEntityReloading) != 0;
    if (reloading && !puppet.reloading) puppet.reloadUntil = g_now + kReloadRetry;
    puppet.reloading = reloading;
    if (puppet.reloadUntil != 0.0) {
        mem::ReloadResult result = mem::PlayReload(chr);
        bool done = result == mem::ReloadResult::Played || result == mem::ReloadResult::Already;
        if (done) {
            LOG("rechargement : '%s' recharge '%s' comme sur l'autre machine", mem::Name(chr), mem::WeaponInHand(chr));
            puppet.reloadUntil = 0.0;
        } else if (g_now > puppet.reloadUntil) {
            const char* why = result == mem::ReloadResult::NoWeapon      ? "pas d'arme en main"
                              : result == mem::ReloadResult::NoAnimation ? "pas d'animation pour cette arme"
                                                                         : "personnage inactif";
            LOG("rechargement : '%s' ne peut pas le montrer : %s", mem::Name(chr), why);
            puppet.reloadUntil = 0.0;
        }
    }

    ApplyEffects(puppet, pose);
}

// Le moteur applique une projection à l'instant où l'animation du lanceur est posée, en lisant la
// cible dans le lanceur. Sur un pantin, rien ne l'a choisie : on y écrit celle que l'émetteur a
// annoncée, sinon il projette dans le vide et sa cible ne bouge pas.
static void ArmThrow(Puppet& puppet, uint32_t animHash, void* tram, double now) {
    if (!mem::IsThrowAnimation(tram)) return;
    Character* target = puppet.throwTarget;
    bool announced = target && puppet.throwAnim == animHash && now < puppet.throwExpires && mem::InUse(target) &&
                     mem::Handle(target) == puppet.throwTargetHandle && !mem::IsDead(target);
    // Sans annonce, la cible est effacée : reposer la même animation ne doit pas projeter deux fois.
    mem::SetThrowTarget(puppet.chr, announced ? target : nullptr, puppet.throwTargetAnim);
    if (!announced) return;
    LOG("projection : '%s' projette '%s'", mem::Name(puppet.chr), mem::Name(target));
    puppet.throwTarget = nullptr;
}

// Le moteur vient d'imposer cette animation au pantin, par une interaction décidée ici. On le
// laisse la dérouler (voir Falling).
static void StartFall(Puppet& puppet, void* anim, double now) {
    puppet.fallAnim = mem::AnimHash(anim);
    puppet.fallSeen = false;
    puppet.fallUntil = now + mem::AnimDuration(anim) / timeline::kTicksPerSecond + kFallCatchUp;
}

// Vrai tant que le pantin déroule une animation décidée ici (voir WatchThrow et React).
// L'émetteur ne la montrera que plus tard : le piloter d'ici là le ramènerait debout.
static bool Falling(Puppet& puppet, const timeline::Pose& pose, double now) {
    if (!puppet.fallAnim) return false;
    bool shown = pose.hasAnimation && pose.animID == puppet.fallAnim;
    if (shown) puppet.fallSeen = true;
    // Fini quand l'émetteur l'a montrée puis est passé à la suite ; le délai couvre une annonce
    // perdue.
    if ((puppet.fallSeen && !shown) || now >= puppet.fallUntil) {
        puppet.fallAnim = 0;
        return false;
    }
    ReleaseTriggers(puppet);
    mem::LockAnimation(puppet.chr, false);  // le moteur enchaîne lui-même jusqu'au sol
    puppet.appliedAnim = nullptr;           // à la reprise, l'animation reçue sera reposée
    puppet.appliedHash = 0;
    return true;
}

static void NoteRemoteHit(Puppet& remote, bool byPlayer) {
    remote.hitAt = g_now;
    remote.hitByPlayer = byPlayer;
}

// Un personnage simulé ici vient de frapper le pantin d'un autre joueur (un PNJ chez l'hôte, ou
// notre joueur quand le tir ami est activé) et le moteur lui a imposé une animation : recul,
// chute, parade. C'est au vrai joueur de la subir : on la lui annonce, et le pantin la joue en
// attendant que son état vienne la montrer à son tour.
static bool React(Puppet& puppet, void* imposed, double now) {
    int target = PlayerOf(puppet);
    if (target < 0 || now - puppet.hitAt > kReactionWindow) return false;
    // Joueur à terre : seul un coup reçu au sol se montre, par une animation qui part du sol et y
    // revient ; son joueur la joue aussi (revive::OnGroundReaction). Toute autre réaction le
    // relèverait ici alors qu'il reste couché chez lui : elle n'est ni annoncée ni jouée, et le
    // pantin reprend aussitôt la pose que dit sa machine.
    if (puppet.hasState && (puppet.latest.flags & proto::kEntityDowned) && !mem::GroundAnimation(imposed)) {
        puppet.appliedAnim = nullptr;
        puppet.appliedHash = 0;
        return false;
    }
    proto::Impose message = {mem::LevelHash(), mem::AnimHash(imposed), static_cast<uint8_t>(puppet.hitByPlayer),
                             static_cast<uint8_t>(target)};
    if (!message.animID || !SendReliableMessage(proto::kReliableImpose, &message, sizeof(message))) return false;
    LOG("reaction : '%s' imposee au joueur %d, annoncee", mem::AnimName(imposed), target);
    StartFall(puppet, imposed, now);
    return true;
}

static void Drive(Puppet& puppet, double now, double delayTicks) {
    timeline::Pose pose;
    if (!timeline::Evaluate(puppet.timeline, timeline::RenderTick(puppet.timeline, now, delayTicks), pose)) return;
    if (Falling(puppet, pose, now)) return;

    mem::WriteTransform(puppet.chr, pose.position, pose.facing);
    DriveWeapon(puppet, pose);

    if (!pose.hasAnimation) {
        mem::LockAnimation(puppet.chr, false);  // entité inactive chez l'émetteur : rien à imiter
        return;
    }
    void* current = mem::CurrentAnimation(puppet.chr);
    if (!current) return;  // personnage inactif localement
    void* tram = mem::AnimFromHash(pose.animID);
    if (!tram) return;

    // Verrouillé, le moteur boucle l'animation en fin de course : on raisonne modulo sa durée.
    int duration = mem::AnimDuration(tram);
    int frame = static_cast<int>(pose.frame + 0.5);
    if (frame < 0) frame = 0;
    if (duration > 0) frame %= duration;

    if (pose.animID != puppet.appliedHash || current != puppet.appliedAnim) {
        // Verrouillé, le pantin ne change d'animation que si on la lui impose : nous, ou le
        // moteur après un coup reçu ici.
        bool imposed = puppet.appliedAnim && current != puppet.appliedAnim;
        if (imposed && React(puppet, current, now)) return;
        // Même animation qu'avant : simple déviation, recalée sans fondu.
        int interp = pose.animID == puppet.appliedHash ? 0 : pose.interpFrames;
        ArmThrow(puppet, pose.animID, tram, now);
        if (mem::SetAnimation(puppet.chr, tram, interp)) {
            puppet.appliedAnim = mem::CurrentAnimation(puppet.chr);
            puppet.appliedHash = pose.animID;
            mem::SetAnimFrame(puppet.chr, frame);
        }
    } else if (FrameDistance(mem::CurrentFrame(puppet.chr), frame, duration) > kFrameTolerance) {
        mem::SetAnimFrame(puppet.chr, frame);
    }
    // Pas de verrou sur une animation de lanceur. Verrouillée, le moteur la rejoue à sa fin si
    // l'état suivant n'est pas encore arrivé, et il applique alors la projection une seconde
    // fois, la cible étant toujours inscrite dans le lanceur : la victime retombait et reperdait
    // de la vie. Sans verrou il passe de lui-même à la suite, ce qui efface la cible.
    mem::LockAnimation(puppet.chr, !mem::IsThrowAnimation(tram));
}

// -------------------------------------------------------------------------------------------------
// Cycle de vie
// -------------------------------------------------------------------------------------------------
void OnLevelChanged() {
    // Les slots ont été réinitialisés par le moteur : on oublie sans rien supprimer.
    for (int i = 0; i < proto::kMaxPlayers; i++) {
        g_remotes[i] = Puppet();
        g_nextRemoteSpawn[i] = 0.0;
        g_levelMismatchReported[i] = false;
    }
    g_incoming.clear();
    g_npcs.clear();
    g_npcArms.clear();
    g_weaponsSeen = false;
    g_doubles.clear();
    g_activeLocked.clear();
    g_cutscene = false;
    g_npcNames.clear();
    g_orphanSince.clear();
    g_lastSnapshot = -1000.0;
    g_nextPassive = 0.0;
    g_passiveApplied = false;
    g_throwWatch.clear();
    g_sentNpcWeapons.clear();
    g_nextNpcWeaponsRefresh = 0.0;
    g_ownersSeen = false;
    g_handBefore = nullptr;
    memset(g_scaled, 0, sizeof(g_scaled));
    mem::ForgetAims();
    gauge::OnLevelChanged();
}

static void RemoveRemotePlayer(Puppet& remote) {
    if (Valid(remote)) {
        LOG("pantin joueur %d : suppression", PlayerOf(remote));
        mem::DeleteCharacter(remote.chr);
    }
    remote = Puppet();
}

void ReleaseNpcs() {
    for (auto& entry : g_npcs) {
        if (!Valid(entry.second)) continue;
        ReleaseTriggers(entry.second);
        mem::ClearAim(entry.second.chr);
        mem::LockAnimation(entry.second.chr, false);
    }
    g_npcs.clear();
    g_orphanSince.clear();
    g_lastSnapshot = -1000.0;
    if (g_passiveApplied && mem::Player()) {
        mem::Exec("ai2_allpassive 0");
        g_passiveApplied = false;
    }
}

void OnSessionClosed() {
    for (Puppet& remote : g_remotes) RemoveRemotePlayer(remote);
    ReleaseNpcs();
    g_incoming.clear();
}

void OnDisconnected(uint8_t player) {
    // Chez l'hôte, un client s'en va : son pantin seulement. Chez un client c'est l'hôte qui est
    // perdu, et avec lui tous les autres joueurs.
    if (net::CurrentMode() != cfg::Mode::Host) OnSessionClosed();
    else if (player < proto::kMaxPlayers) RemoveRemotePlayer(g_remotes[player]);
}

// -------------------------------------------------------------------------------------------------
// Réception
// -------------------------------------------------------------------------------------------------
void OnPlayerState(const proto::EntityState& state, uint32_t levelHash, double now) {
    uint32_t id = state.characterID;
    if (id >= static_cast<uint32_t>(proto::kMaxPlayers) || id == LocalPlayerId()) return;
    Puppet& remote = g_remotes[id];
    if (levelHash != mem::LevelHash()) {
        if (!g_levelMismatchReported[id]) {
            g_levelMismatchReported[id] = true;
            mem::ConsolePrint("OniMP : le joueur %u n'est pas dans le meme niveau", static_cast<unsigned>(id));
        }
        remote.hasState = false;
        remote.timeline.count = 0;
        return;
    }
    g_levelMismatchReported[id] = false;
    PushState(remote, state, now);
}

void OnNpcSnapshot(const proto::EntityState* states, int count, uint32_t levelHash, double now) {
    if (!cfg::Get().npcs || levelHash != mem::LevelHash()) return;
    g_lastSnapshot = now;
    for (int i = 0; i < count; i++) PushState(g_npcs[states[i].characterID], states[i], now);
}

void OnNpcInfo(const proto::NpcInfo* infos, int count) {
    for (int i = 0; i < count; i++) g_npcNames[infos[i].characterID] = infos[i].name;
}

void OnEvent(uint8_t from, const proto::Event& event, uint32_t levelHash) {
    if (levelHash != mem::LevelHash()) return;
    if (event.kind != proto::kEventDamage || net::CurrentMode() != cfg::Mode::Host) return;
    if (event.value <= 0 || event.value > 10000) return;

    std::vector<LocalNpc> locals;
    ListLocalNpcs(locals);
    for (const LocalNpc& local : locals) {
        if (local.id != event.characterID) continue;
        if (!mem::IsDead(local.chr)) {
            LOG("degats client %u : %d sur '%s'", from, event.value, mem::Name(local.chr));
            spawn::OnPlayerDamage(local.chr, from);
            // Au nom du pantin de ce client : l'IA du PNJ sait ainsi qui l'a blessé.
            mem::Damage(local.chr, static_cast<uint32_t>(event.value), RemotePlayerCharacter(from));
        }
        return;
    }
}

// Coup reçu par un joueur et jugé par une autre machine : celui d'un PNJ, décidé par l'hôte, ou
// celui d'un autre joueur quand le tir ami est activé. Il ne nous concerne que si c'est notre
// joueur qui encaisse. Appliqué quand le pantin de l'attaquant, affiché ici avec retard, en est au
// même point.
void OnHit(uint8_t from, const uint8_t* data, int size, double now) {
    Incoming incoming = {};
    if (size < static_cast<int>(sizeof(incoming.hit))) return;
    memcpy(&incoming.hit, data, sizeof(incoming.hit));
    if (incoming.hit.levelHash != mem::LevelHash() || incoming.hit.targetID != LocalPlayerId()) return;
    if (incoming.hit.attackerIsPlayer) {
        if (!coop::TeamKill() || incoming.hit.attackerID != from) return;
        incoming.at = now + kPlayerDelayTicks / timeline::kTicksPerSecond;
    } else {
        if (net::CurrentMode() != cfg::Mode::Client || from != proto::kHostPlayerId) return;
        incoming.at = now + kNpcDelayTicks / timeline::kTicksPerSecond;
    }
    incoming.isHit = true;
    g_incoming.push_back(incoming);
}

// Réaction à un coup, décidée comme lui par une autre machine : mêmes conditions, même retard.
void OnImpose(uint8_t from, const uint8_t* data, int size, double now) {
    proto::Impose message;
    if (size < static_cast<int>(sizeof(message))) return;
    memcpy(&message, data, sizeof(message));
    if (message.levelHash != mem::LevelHash() || message.targetID != LocalPlayerId()) return;
    Incoming incoming = {};
    if (message.byPlayer) {
        if (!coop::TeamKill()) return;
        incoming.at = now + kPlayerDelayTicks / timeline::kTicksPerSecond;
    } else {
        if (net::CurrentMode() != cfg::Mode::Client || from != proto::kHostPlayerId) return;
        incoming.at = now + kNpcDelayTicks / timeline::kTicksPerSecond;
    }
    incoming.animID = message.animID;
    g_incoming.push_back(incoming);
}

static void ApplyIncoming(double now) {
    Character* player = mem::Player();
    size_t kept = 0;
    for (size_t i = 0; i < g_incoming.size(); i++) {
        const Incoming& incoming = g_incoming[i];
        if (now < incoming.at) {
            g_incoming[kept++] = incoming;
            continue;
        }
        if (mem::IsDead(player)) continue;
        if (revive::Shielded()) {
            // À terre : plus rien ne le blesse ni ne le relève. Un coup reçu au sol le fait tout de
            // même réagir, sans rien changer à son état.
            if (!incoming.isHit) revive::OnGroundReaction(mem::AnimFromHash(incoming.animID), now);
            continue;
        }
        if (!incoming.isHit) {
            void* tram = mem::AnimFromHash(incoming.animID);
            LOG("reaction : '%s' imposee par une autre machine", mem::AnimName(tram));
            mem::SetAnimation(player, tram, kReactionInterp);
            continue;
        }
        const proto::Hit& hit = incoming.hit;
        if (cheat::GodMode()) continue;
        Character* attacker = nullptr;
        if (hit.attackerIsPlayer) {
            attacker = RemotePlayerCharacter(hit.attackerID);
        } else {
            auto found = g_npcs.find(hit.attackerID);
            if (found != g_npcs.end() && Valid(found->second)) attacker = found->second.chr;
        }
        Vector3 direction = {hit.direction[0], hit.direction[1], hit.direction[2]};
        // Une poussée qui n'est pas un nombre ne s'applique pas : le coup compte, sans elle.
        bool pushed = hit.hasDirection && std::isfinite(direction.x) && std::isfinite(direction.y) &&
                      std::isfinite(direction.z) && std::isfinite(hit.knockback);
        LOG("coup recu : %u de '%s'", hit.damage, attacker ? mem::Name(attacker) : "?");
        if (!hit.attackerIsPlayer) revive::OnLocalHit();
        mem::Damage(player, hit.damage, attacker, hit.ownerType == 1 ? fn::kOwnerWeapon : fn::kOwnerMelee,
                    pushed ? &direction : nullptr, pushed ? hit.knockback : 0.0f, hit.impact);
    }
    g_incoming.resize(kept);
}

void OnThrow(uint8_t from, const uint8_t* data, int size, double now) {
    proto::Throw message;
    if (size < static_cast<int>(sizeof(message))) return;
    memcpy(&message, data, sizeof(message));
    if (message.levelHash != mem::LevelHash()) return;

    // Le lanceur est un pantin ici : celui du joueur qui l'annonce, ou un PNJ de l'hôte...
    Puppet* source = nullptr;
    if (message.sourceIsPlayer) {
        if (message.sourceID == from && from < proto::kMaxPlayers && from != LocalPlayerId()) source = &g_remotes[from];
    } else if (from != proto::kHostPlayerId) {
        return;
    } else {
        auto found = g_npcs.find(message.sourceID);
        if (found != g_npcs.end()) source = &found->second;
    }
    if (!source || !Valid(*source)) return;

    // ... et la cible un personnage simulé ici : notre joueur, ou chez l'hôte un PNJ. Sinon
    // l'annonce est pour une autre machine.
    Character* target = nullptr;
    if (message.targetIsPlayer) {
        if (message.targetID == LocalPlayerId()) target = mem::Player();
    } else if (net::CurrentMode() == cfg::Mode::Host) {
        std::vector<LocalNpc> locals;
        ListLocalNpcs(locals);
        for (const LocalNpc& local : locals) {
            if (local.id == message.targetID) target = local.chr;
        }
    }
    void* targetAnim = mem::AnimFromHash(message.targetAnim);
    if (!target || mem::IsDead(target) || !targetAnim) return;

    source->throwTarget = target;
    source->throwTargetHandle = mem::Handle(target);
    source->throwTargetAnim = targetAnim;
    source->throwAnim = message.throwAnim;
    source->throwExpires = now + kThrowPendingTime;

    // L'annonce précède normalement l'animation, affichée avec retard. Arrivée après, le pantin
    // projette déjà dans le vide : on repose son animation, cette fois avec sa cible.
    void* tram = mem::AnimFromHash(message.throwAnim);
    bool playing = source->appliedHash == message.throwAnim && source->appliedAnim &&
                   mem::CurrentAnimation(source->chr) == source->appliedAnim;
    if (!tram || !playing) return;
    int frame = mem::CurrentFrame(source->chr);
    ArmThrow(*source, message.throwAnim, tram, now);
    if (mem::SetAnimation(source->chr, tram, 0)) {
        source->appliedAnim = mem::CurrentAnimation(source->chr);
        mem::SetAnimFrame(source->chr, frame);
    }
}

// -------------------------------------------------------------------------------------------------
// Émission
// -------------------------------------------------------------------------------------------------
bool CollectLocalPlayer(proto::EntityState& out) {
    Character* player = mem::Player();
    if (!player) return false;
    mem::ReadEntity(player, LocalPlayerId(), out);
    if (!SoundState(out)) return false;  // rien ne part d'ici qui ne soit un nombre
    revive::Stamp(out);

    // Au journal, ce qui part d'ici et ne se voit pas dans l'animation : rechargement, aura,
    // bouclier, invisibilité.
    static bool reloading = false, aura = false, shield = false, cloak = false;
    bool reloadingNow = (out.flags & proto::kEntityReloading) != 0;
    if (reloadingNow && !reloading) LOG("rechargement : notre joueur recharge '%s', annonce dans son etat", mem::WeaponInHand(player));
    reloading = reloadingNow;
    if ((out.super != 0) != aura) LOG("aura : %s chez notre joueur, annoncee dans son etat", out.super ? "allumee" : "eteinte");
    aura = out.super != 0;
    if ((out.shield != 0) != shield) {
        LOG("bouclier : notre joueur %s, annonce dans son etat", out.shield ? "en porte un" : "n'en a plus");
    }
    shield = out.shield != 0;
    if ((out.cloak != 0) != cloak) {
        LOG("invisibilite : notre joueur %s, annonce dans son etat", out.cloak ? "est invisible" : "est de nouveau visible");
    }
    cloak = out.cloak != 0;
    return true;
}

Character* NearestHostile(const Vector3& from, float range, float* distance) {
    Character* best = nullptr;
    float bestSquared = range * range;
    int count = mem::CharacterCount();
    for (int i = 0; i < count; i++) {
        Character* chr = mem::CharacterAt(i);
        if (!chr || !IsReplicatedNpc(chr) || mem::IsDead(chr) || !HostileToPlayers(chr)) continue;
        float squared = DistanceSquared(from, mem::Location(chr));
        if (squared > bestSquared) continue;
        best = chr;
        bestSquared = squared;
    }
    if (best && distance) *distance = std::sqrt(bestSquared);
    return best;
}

int Hostiles(const Vector3& from, float range, Character** out, int capacity) {
    struct Found {
        Character* chr;
        float squared;
    };
    static std::vector<Found> found;
    found.clear();
    int count = mem::CharacterCount();
    for (int i = 0; i < count; i++) {
        Character* chr = mem::CharacterAt(i);
        if (!chr || !IsReplicatedNpc(chr) || mem::IsDead(chr) || !HostileToPlayers(chr)) continue;
        float squared = DistanceSquared(from, mem::Location(chr));
        if (squared <= range * range) found.push_back({chr, squared});
    }
    std::sort(found.begin(), found.end(), [](const Found& a, const Found& b) { return a.squared < b.squared; });
    int kept = 0;
    for (const Found& entry : found) {
        if (kept >= capacity) break;
        out[kept++] = entry.chr;
    }
    return kept;
}

// Ce que le moteur ne doit pas tracer : le pantin d'un autre joueur pendant une cinématique (le
// niveau y met en scène un seul personnage principal, le nôtre ; les autres joueurs, eux aussi
// Konoko, s'y voyaient en double ou en triple), et un PNJ en double chez un client.
static bool Hidden(Character* chr) {
    if (g_cutscene && IsRemotePlayer(chr)) return true;
    return !g_doubles.empty() && g_doubles.count(mem::Handle(chr)) != 0;
}

bool InstallVisibilityHook() { return mem::InstallVisibilityHook(&Hidden); }

// Début et fin d'une cinématique, sur chaque machine (le client reçoit begin_cutscene de l'hôte).
// Cachés, les pantins ne doivent pas non plus gêner la mise en scène, où ils sont souvent à
// l'endroit même que le script donne au personnage principal : voir UpdateRemotePlayer.
// Le personnage du joueur est aux mains de l'IA de déplacement (ai2_takecontrol 1) : une
// cinématique le fait marcher. Chez un client la commande suit maintenant celle de l'hôte ; reste
// le cas où elle ne lui serait jamais rendue (reprise au milieu, hôte parti). Hors cinématique et
// touches rendues, cinq secondes de suite, la main lui est rendue d'office.
static void GuardControl(double now) {
    static double since = 0.0;
    Character* player = mem::Player();
    bool held = net::CurrentMode() == cfg::Mode::Client && player && !mem::IsDead(player) && !mem::InCutscene() &&
                mem::InputEnabled() && (field<uint32_t>(player, off::kChFlags) & off::kChFlagAiMovement) != 0;
    if (!held) {
        since = 0.0;
        return;
    }
    if (since == 0.0) since = now;
    if (now - since < kControlGrace) return;
    field<uint32_t>(player, off::kChFlags) &= ~off::kChFlagAiMovement;
    since = 0.0;
    LOG("cinematique : notre joueur etait reste aux mains de l'IA de deplacement, la main lui est rendue");
}

static void TrackCutscene() {
    // Le script suit un autre personnage que notre joueur (chr_focus) : au journal, une fois.
    static Character* followed = nullptr;
    Character* focused = mem::FocusedCharacter();
    if (focused != followed) {
        followed = focused;
        if (focused && focused != mem::Player()) {
            LOG("cinematique : le script suit '%s' (chr_focus) ; notre joueur reste '%s'", mem::Name(focused),
                mem::Name(mem::Player()));
        }
    }
    bool cutscene = net::Running() && mem::InCutscene();
    if (cutscene == g_cutscene) return;
    g_cutscene = cutscene;
    int count = 0;
    for (const Puppet& remote : g_remotes) {
        if (Valid(remote)) count++;
    }
    LOG("cinematique : %s, %d autre(s) joueur(s) %s", cutscene ? "commence" : "finie", count,
        cutscene ? "caches" : "remontres");
}

// Le moteur met en sommeil un PNJ loin de son joueur : plus de physique ni d'animation, il avance
// par à-coups le long de son chemin, sans suivre le sol. Chez l'hôte, un PNJ loin de lui mais près
// d'un autre joueur était dans cet état, et c'est ce que cet autre joueur voyait : des PNJ qui
// flottent, glissent et tremblent. L'hôte garde donc actif tout PNJ à portée d'un autre joueur, et
// le rend au moteur quand plus aucun n'est près de lui.
static void KeepNpcsActive() {
    bool wanted = cfg::Get().npcs && net::CurrentMode() == cfg::Mode::Host && net::Connected();
    if (!wanted && g_activeLocked.empty()) return;
    static std::vector<LocalNpc> locals;
    ListLocalNpcs(locals);
    std::unordered_map<uint16_t, char> kept;
    size_t before = g_activeLocked.size();
    for (const LocalNpc& npc : locals) {
        uint16_t handle = mem::Handle(npc.chr);
        bool ours = g_activeLocked.count(handle) != 0;
        float nearest = kActiveRelease * kActiveRelease * 4.0f;
        if (wanted && !mem::IsDead(npc.chr)) {
            Vector3 here = mem::Location(npc.chr);
            for (const Puppet& remote : g_remotes) {
                if (!Valid(remote)) continue;
                float squared = DistanceSquared(here, mem::Location(remote.chr));
                if (squared < nearest) nearest = squared;
            }
        }
        if (ours) {
            if (nearest > kActiveRelease * kActiveRelease) mem::LockActive(npc.chr, false);
            else kept[handle] = 1;
        } else if (nearest < kActiveRange * kActiveRange &&
                   !(field<uint32_t>(npc.chr, off::kChFlags) & off::kChFlagActiveLock) &&  // le niveau l'a déjà voulu
                   mem::LockActive(npc.chr, true)) {
            kept[handle] = 1;
        }
    }
    g_activeLocked.swap(kept);
    if (g_activeLocked.size() != before) {
        LOG("pnj : %u garde(s) actif(s) pres d'un autre joueur, loin du notre", static_cast<unsigned>(g_activeLocked.size()));
    }
}

Character* RemotePlayerCharacter(uint32_t player) {
    return player < static_cast<uint32_t>(proto::kMaxPlayers) && Valid(g_remotes[player]) ? g_remotes[player].chr
                                                                                          : nullptr;
}

bool DownedPlayerAt(uint32_t player, Vector3& where) {
    if (player >= static_cast<uint32_t>(proto::kMaxPlayers)) return false;
    const Puppet& remote = g_remotes[player];
    if (!Valid(remote) || !remote.hasState) return false;
    if (!(remote.latest.flags & proto::kEntityDowned) || (remote.latest.flags & proto::kEntityDead)) return false;
    where = mem::Location(remote.chr);
    return true;
}

bool RemotePlayerState(uint32_t player, proto::EntityState& out) {
    if (player >= static_cast<uint32_t>(proto::kMaxPlayers)) return false;
    const Puppet& remote = g_remotes[player];
    if (!Valid(remote) || !remote.hasState) return false;
    out = remote.latest;
    return true;
}

// Une projection décidée ici sur un personnage qu'une autre machine simule (un autre joueur, et
// pour un client les PNJ) n'existe que chez nous : le moteur a déjà imposé la chute à notre pantin,
// mais là-bas la cible n'en sait rien. On l'annonce, et on laisse le pantin finir sa chute.
static void WatchThrow(Character* source, uint32_t sourceID, bool sourceIsPlayer, double now) {
    uint16_t handle = mem::Handle(source);
    Character* target = nullptr;
    void* targetAnim = nullptr;
    if (!mem::ThrowInProgress(source, target, targetAnim)) {
        g_throwWatch.erase(handle);
        return;
    }
    void* anim = mem::CurrentAnimation(source);
    auto known = g_throwWatch.find(handle);
    if (known != g_throwWatch.end() && known->second.anim == anim && known->second.target == target) return;
    g_throwWatch[handle] = {anim, target};

    proto::Throw message = {};
    Puppet* puppet = RemoteOf(target);
    if (puppet) {
        message.targetID = static_cast<uint32_t>(PlayerOf(*puppet));
        message.targetIsPlayer = 1;
    } else {
        puppet = FindNpcPuppet(target, &message.targetID);
    }
    if (!puppet) return;  // cible simulée ici : le moteur a tout fait

    message.levelHash = mem::LevelHash();
    message.sourceID = sourceID;
    message.sourceIsPlayer = sourceIsPlayer ? 1 : 0;
    message.throwAnim = mem::AnimHash(anim);
    message.targetAnim = mem::AnimHash(targetAnim);
    if (!SendReliableMessage(proto::kReliableThrow, &message, sizeof(message))) return;
    LOG("projection : '%s' projette '%s', annoncee", mem::Name(source), mem::Name(target));

    StartFall(*puppet, mem::CurrentAnimation(target), now);  // ce que le moteur a réellement posé sur la cible
}

static void WatchLocalThrows(double now) {
    if (!net::Connected()) {
        g_throwWatch.clear();
        return;
    }
    WatchThrow(mem::Player(), LocalPlayerId(), true, now);

    // Les PNJ sont simulés chez l'hôte : leurs prises sur le joueur d'un client partent d'ici.
    if (!cfg::Get().npcs || net::CurrentMode() != cfg::Mode::Host || !AnyRemotePlayer()) return;
    static std::vector<LocalNpc> locals;
    ListLocalNpcs(locals);
    for (const LocalNpc& local : locals) WatchThrow(local.chr, local.id, false, now);
}

int CollectNpcs(proto::EntityState* states, proto::NpcInfo* infos, int capacity) {
    std::vector<LocalNpc> locals;
    ListLocalNpcs(locals);
    int count = 0;
    for (const LocalNpc& local : locals) {
        if (count >= capacity) break;
        mem::ReadEntity(local.chr, local.id, states[count]);
        if (!SoundState(states[count])) continue;
        if (infos) {
            infos[count].characterID = local.id;
            memset(infos[count].name, 0, sizeof(infos[count].name));
            strncpy(infos[count].name, mem::Name(local.chr), sizeof(infos[count].name) - 1);
        }
        count++;
    }
    return count;
}

// -------------------------------------------------------------------------------------------------
// Chaque frame
// -------------------------------------------------------------------------------------------------
static void UpdateRemotePlayer(int id, double now) {
    Puppet& remote = g_remotes[id];
    bool wanted = remote.hasState && remote.timeline.count > 0 && now - remote.lastSeen < kStateTimeout;
    if (!wanted) {
        if (Valid(remote)) RemoveRemotePlayer(remote);
        return;
    }

    if (!Valid(remote)) {
        if (now < g_nextRemoteSpawn[id] || mem::GameTime() < kSpawnDelayTicks) return;
        g_nextRemoteSpawn[id] = now + kSpawnRetry;

        char name[16];
        snprintf(name, sizeof(name), "%s%d", kRemotePrefix, id);
        Character* player = mem::Player();
        const proto::EntityState& latest = remote.latest;
        Vector3 position = {latest.x, latest.y, latest.z};
        // Dans la tenue que l'hôte lui a attribuée, s'il l'a déjà dite.
        Character* chr = mem::SpawnPuppet(roster::ClassFor(static_cast<uint8_t>(id)), name, position, latest.rotation,
                                          field<uint16_t>(player, off::kChTeam));
        if (!chr) return;

        Bind(remote, chr);
        // Toujours actif (animations calculées même loin), sans IA, insensible aux dégâts locaux :
        // la santé d'un joueur n'est décidée que sur sa propre machine.
        mem::Exec("chr_lock_active %s", name);
        mem::Exec("ai2_passive %s 1", name);
        mem::Exec("chr_invincible %s 1", name);
        mem::ConsolePrint("OniMP : joueur %d apparu", id);
    }

    Drive(remote, now, kPlayerDelayTicks);

    // Un pantin ne heurte plus rien dans deux cas. Pendant une cinématique, où il est caché. Et
    // quand son joueur est à terre : sa place est remise à chaque image à celle que dit sa
    // machine, et tout personnage d'ici qui marchait dessus le repoussait entre-temps ; le corps
    // tressautait et glissait sous celui qui venait le ranimer.
    // Pas pendant sa chute, que le moteur joue lui-même sans qu'on le replace : sans rien pour le
    // retenir, il passerait à travers le sol.
    bool ghost = g_cutscene || ((remote.latest.flags & proto::kEntityDowned) != 0 && remote.fallAnim == 0);
    if (ghost != remote.ghost) {
        remote.ghost = ghost;
        mem::Exec("chr_nocollision %s %d", mem::Name(remote.chr), ghost ? 1 : 0);
    }
}

static void UpdateRemotePlayers(double now) {
    for (int id = 0; id < proto::kMaxPlayers; id++) {
        if (static_cast<uint32_t>(id) != LocalPlayerId()) UpdateRemotePlayer(id, now);
    }
}

static void SyncNpcHealth(Puppet& puppet, double now) {
    Character* chr = puppet.chr;
    int32_t local = mem::Health(chr);
    bool hostDead = (puppet.latest.flags & proto::kEntityDead) != 0 || puppet.latest.health <= 0;

    // Les coups de notre joueur sont annoncés à l'hôte par FilterDamage. D'ici sa réponse, on ne
    // rend pas au PNJ la vie qu'ils lui ont ôtée.
    if (now > puppet.pendingUntil) puppet.pendingDamage = 0;

    if (hostDead) {
        if (!mem::IsDead(chr) && !puppet.killed) {
            puppet.killed = true;
            mem::LockAnimation(chr, false);  // laisse jouer l'animation de mort
            mem::ClearDrops(chr);            // ce qu'il lâche vient de l'hôte (voir world.cpp)
            mem::Kill(chr);
        }
    } else {
        int32_t wanted = puppet.latest.health - puppet.pendingDamage;
        if (wanted < 1) wanted = 1;
        if (local != wanted) mem::SetHealth(chr, static_cast<uint32_t>(wanted));
        puppet.killed = false;
    }
}

// -------------------------------------------------------------------------------------------------
// Arme des PNJ
// -------------------------------------------------------------------------------------------------
// Chez le client un PNJ est un pantin : sa mêlée et ses tirs viennent de l'hôte, mais son arme
// restait l'affaire du moteur local. Un coup de notre joueur la lui faisait lâcher ici alors qu'il
// la tenait toujours chez l'hôte, ou l'inverse : le boss tirait avec une arme qu'on ne voyait pas,
// elle traînait au sol d'un côté et pas de l'autre, et le joueur pouvait la ramasser d'un côté
// seulement. L'hôte dit donc ce que tient chaque PNJ, et le client y ramène les siens : l'arme
// tombée ici seulement est reprise, celle que l'hôte a vue tomber tombe ici aussi.
static void SendNpcWeapons(double now) {
    if (now < g_nextNpcWeapons) return;
    g_nextNpcWeapons = now + kNpcWeaponInterval;
    bool refresh = now >= g_nextNpcWeaponsRefresh;
    if (refresh) g_nextNpcWeaponsRefresh = now + kNpcWeaponRefresh;

    static std::vector<LocalNpc> locals;
    ListLocalNpcs(locals);
    std::string entries;
    int count = 0;
    bool truncated = false;
    for (const LocalNpc& npc : locals) {
        if (mem::IsDead(npc.chr)) continue;
        const char* hand = mem::WeaponInHand(npc.chr);
        const char* stored = mem::WeaponStored(npc.chr);
        bool holstered = !hand[0] && stored[0];
        const char* name = holstered ? stored : hand;
        proto::NpcWeapon entry = {};
        if (!name[0] || strlen(name) >= sizeof(entry.name)) continue;
        if (count >= kMaxNpcWeapons) {
            truncated = true;
            break;
        }
        entry.characterID = npc.id;
        strncpy(entry.name, name, sizeof(entry.name) - 1);
        entry.holstered = holstered ? 1 : 0;
        entries.append(reinterpret_cast<const char*>(&entry), sizeof(entry));
        count++;
    }
    if (!refresh && entries == g_sentNpcWeapons) return;
    uint32_t level = mem::LevelHash();
    std::string message(1, static_cast<char>(proto::kReliableNpcWeapons));
    message.append(reinterpret_cast<const char*>(&level), sizeof(level));
    // Bit 7 : la liste est tronquée, un PNJ qui n'y figure pas n'est pas forcément désarmé.
    message.push_back(static_cast<char>(count | (truncated ? 0x80 : 0)));
    message += entries;
    if (net::SendReliable(message.data(), static_cast<int>(message.size()))) g_sentNpcWeapons = entries;
}

void OnNpcWeapons(uint8_t from, const uint8_t* data, int size) {
    if (from != proto::kHostPlayerId || net::CurrentMode() != cfg::Mode::Client || !cfg::Get().npcs) return;
    uint32_t level;
    if (size < 5) return;
    memcpy(&level, data, 4);
    if (level != mem::LevelHash()) return;
    int count = data[4] & 0x7F;
    bool truncated = (data[4] & 0x80) != 0;
    if (size < 5 + count * static_cast<int>(sizeof(proto::NpcWeapon))) return;

    static std::unordered_map<uint32_t, proto::NpcWeapon> listed;
    listed.clear();
    for (int i = 0; i < count; i++) {
        proto::NpcWeapon entry;
        memcpy(&entry, data + 5 + i * sizeof(entry), sizeof(entry));
        entry.name[sizeof(entry.name) - 1] = '\0';
        // Le nom finit dans une ligne de script : on n'accepte que des noms de classe d'arme.
        bool valid = entry.name[0] != '\0';
        for (const char* c = entry.name; *c && valid; c++) {
            valid = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_';
        }
        if (valid) listed[entry.characterID] = entry;
    }
    for (auto& npc : g_npcs) {
        Puppet& puppet = npc.second;
        auto found = listed.find(npc.first);
        if (found == listed.end() && truncated) continue;
        const char* name = found == listed.end() ? "" : found->second.name;
        bool holstered = found != listed.end() && found->second.holstered != 0;
        if (!puppet.weaponKnown || strcmp(puppet.wantedWeapon, name) != 0 || puppet.wantedHolstered != holstered) {
            puppet.nextWeaponFix = 0.0;  // changement : appliqué sans attendre
            puppet.weaponTries = 0;
        }
        strncpy(puppet.wantedWeapon, name, sizeof(puppet.wantedWeapon) - 1);
        puppet.wantedWeapon[sizeof(puppet.wantedWeapon) - 1] = '\0';
        puppet.wantedHolstered = holstered;
        puppet.weaponKnown = true;
    }
}

static void FixNpcWeapon(Puppet& puppet, double now) {
    if (!puppet.weaponKnown) return;
    Character* chr = puppet.chr;
    const char* wanted = puppet.wantedWeapon;
    const char* held = mem::WeaponInHand(chr);
    bool inHand = wanted[0] && !puppet.wantedHolstered;
    // Un joueur vient de lui prendre cette arme (voir OnDisarm) : la liste de l'hôte qui la lui
    // prête encore date d'avant. Lui en redonner une la mettait en double, une au joueur, une au
    // PNJ : c'était l'arme de Barabas de la 0.28.
    if (puppet.disarmedUntil != 0.0) {
        if (!wanted[0] || now >= puppet.disarmedUntil) puppet.disarmedUntil = 0.0;
        else if (!held[0] && strcmp(wanted, puppet.disarmed) == 0) return;
    }
    if (inHand ? strcmp(held, wanted) == 0 : !held[0]) {
        puppet.weaponTries = 0;
        return;
    }
    // Il la tient chez l'hôte et vient de la lâcher ici (jeté au sol par notre joueur) : il la
    // reprend aussitôt, avant que quiconque la ramasse de ce côté seulement. La sienne d'abord, où
    // qu'elle soit tombée : un coup peut l'envoyer loin, et jusqu'à la 0.25, faute de la retrouver à
    // ses pieds, on lui en créait une autre en laissant la première au sol. C'était l'arme de
    // Barabas en double.
    if (inHand && !held[0]) {
        void* own = puppet.ownWeapon;
        if (own && mem::WeaponOnGround(own) && strcmp(mem::WeaponClassName(own), wanted) == 0) {
            mem::TakeWeapon(chr, own);
            if (puppet.weaponTries++ == 0) LOG("arme du PNJ '%s' : '%s' lachee ici, la sienne, reprise ou qu'elle soit", mem::Name(chr), wanted);
            return;
        }
        if (void* dropped = mem::FindGroundWeapon(wanted, mem::Location(chr), kNpcDropRadius)) {
            mem::TakeWeapon(chr, dropped);
            if (puppet.weaponTries++ == 0) LOG("arme du PNJ '%s' : '%s' lachee ici seulement, reprise", mem::Name(chr), wanted);
            return;
        }
    }
    if (now < puppet.nextWeaponFix || puppet.weaponTries >= kMaxNpcWeaponTries) return;
    puppet.nextWeaponFix = now + kNpcWeaponRetry;
    puppet.weaponTries++;
    const char* name = mem::Name(chr);
    LOG("arme du PNJ '%s' : tient '%s', l'hote dit '%s'%s", name, held, wanted,
        wanted[0] && puppet.wantedHolstered ? " rangee" : "");
    if (!inHand) {
        // Rangée chez l'hôte, ou tombée : elle tombe ici aussi, et se ramasse des deux côtés.
        if (wanted[0] && strcmp(held, wanted) == 0) mem::StoreWeapon(chr, true);
        else mem::DropWeapon(chr);
    } else if (!held[0] && strcmp(mem::WeaponStored(chr), wanted) == 0) {
        mem::StoreWeapon(chr, false);  // ressortie
    } else {
        if (held[0]) mem::Exec("chr_inv_reset %s", name);  // sinon l'arme précédente tombe au sol
        LOG("arme du PNJ '%s' : aucune '%s' a reprendre ici, une neuve lui est donnee", name, wanted);
        mem::Exec("chr_giveweapon %s %s", name, wanted);
    }
}

// -------------------------------------------------------------------------------------------------
// Arme prise à un PNJ
// -------------------------------------------------------------------------------------------------
// Une prise sur un PNJ armé peut le désarmer : son arme passe de ses mains à celles du joueur sans
// toucher le sol, et aucune annonce d'arme lâchée ou ramassée n'en parlait. Désarmé par un client,
// le PNJ gardait la sienne chez l'hôte, qui en donnait en plus une neuve au pantin de ce client :
// deux armes pour une (journaux de la 0.28, Barabas : `arme du pantin 1 : attendu 'w11_ba1'`, puis
// `arme du PNJ 'Barabus' : 'w11_ba1' lachee` chez l'hôte), et chez le client le PNJ en recevait
// aussitôt une neuve puisque l'hôte la lui prêtait toujours. La machine du joueur qui désarme
// l'annonce donc ; l'hôte, seul à décider de ce que tiennent les PNJ, retire l'arme au sien sans la
// poser au sol, et chaque autre client à son pantin. Cela vaut pour tous les PNJ, boss ou non.
static bool WeaponClassValid(const char* name) {
    if (!name[0]) return false;
    for (const char* c = name; *c; c++) {
        bool valid = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_';
        if (!valid) return false;
    }
    return true;
}

// Retire au personnage l'arme de cette classe qu'il tient ou a rangée, sans la laisser au sol.
static bool StripWeapon(Character* chr, const char* className) {
    if (strcmp(mem::WeaponInHand(chr), className) != 0) {
        if (mem::WeaponInHand(chr)[0] || strcmp(mem::WeaponStored(chr), className) != 0) return false;
        mem::StoreWeapon(chr, false);  // rangée : ressortie, pour la lui prendre
        if (strcmp(mem::WeaponInHand(chr), className) != 0) return false;
    }
    void* weapon = mem::DropWeapon(chr);
    if (!weapon) return false;
    mem::DeleteWeapon(weapon);
    return true;
}

static void HoldDisarmed(Puppet& puppet, const char* className, double now) {
    strncpy(puppet.disarmed, className, sizeof(puppet.disarmed) - 1);
    puppet.disarmed[sizeof(puppet.disarmed) - 1] = '\0';
    puppet.disarmedUntil = now + kDisarmHold;
    puppet.ownWeapon = nullptr;  // elle est au joueur : le pantin ne la reprendra pas s'il la pose
    puppet.weaponTries = 0;
}

static void AnnounceDisarm(Character* npc, void* weapon, double now) {
    proto::Disarm message = {};
    const char* name = mem::WeaponClassName(weapon);
    if (!WeaponClassValid(name) || strlen(name) >= sizeof(message.name)) return;
    message.levelHash = mem::LevelHash();
    message.ammo = mem::WeaponAmmo(weapon);
    strncpy(message.name, name, sizeof(message.name) - 1);
    if (net::CurrentMode() == cfg::Mode::Host) {
        if (!LocalNpcId(npc, message.characterID)) return;
    } else {
        Puppet* puppet = FindNpcPuppet(npc, &message.characterID);
        if (!puppet) return;
        HoldDisarmed(*puppet, name, now);
    }
    if (!SendReliableMessage(proto::kReliableDisarm, &message, sizeof(message))) return;
    LOG("desarmement : notre joueur prend '%s' a '%s', annonce", name, mem::Name(npc));
}

// L'arme que notre joueur a en main était, à l'image précédente, dans celles d'un PNJ.
static void WatchDisarm(double now) {
    Character* player = mem::Player();
    void* hand = field<void*>(player, off::kChWeaponInHand);
    if (g_ownersSeen && hand && hand != g_handBefore && net::Connected() && cfg::Get().npcs) {
        int slot = mem::WeaponIndex(hand);
        Character* before = slot >= 0 ? g_weaponOwner[slot] : nullptr;
        if (before && before != player && IsReplicatedNpc(before)) AnnounceDisarm(before, hand, now);
    }
    g_handBefore = hand;
    for (int i = 0; i < addr::kMaxWeapons; i++) g_weaponOwner[i] = mem::WeaponOwner(mem::WeaponAt(i));
    g_ownersSeen = true;
}

void OnDisarm(uint8_t from, const uint8_t* data, int size, double now) {
    proto::Disarm message;
    if (size < static_cast<int>(sizeof(message)) || !cfg::Get().npcs || !mem::Player()) return;
    memcpy(&message, data, sizeof(message));
    message.name[sizeof(message.name) - 1] = '\0';
    if (message.levelHash != mem::LevelHash() || from == LocalPlayerId() || !WeaponClassValid(message.name)) return;

    if (net::CurrentMode() == cfg::Mode::Host) {
        static std::vector<LocalNpc> locals;
        ListLocalNpcs(locals);
        for (const LocalNpc& local : locals) {
            if (local.id != message.characterID) continue;
            NpcArm& arm = g_npcArms[mem::Handle(local.chr)];
            const char* name = mem::Name(local.chr);
            if (StripWeapon(local.chr, message.name)) {
                LOG("desarmement : le joueur %u a pris '%s' a '%s' : retiree au PNJ, une seule de chaque", from,
                    message.name, name);
            } else if (arm.dropped && mem::WeaponOnGround(arm.dropped) &&
                       strcmp(mem::WeaponClassName(arm.dropped), message.name) == 0) {
                // Il venait de la lâcher ici : c'est celle-là que ce joueur a en main.
                mem::DeleteWeapon(arm.dropped);
                arm.dropped = nullptr;
                LOG("desarmement : le joueur %u a pris '%s' a '%s' : elle etait tombee ici, retiree du sol", from,
                    message.name, name);
            } else {
                LOG("desarmement : le joueur %u a pris '%s' a '%s', qui n'en tenait pas ici : rien a retirer", from,
                    message.name, name);
            }
            void* held = field<void*>(local.chr, off::kChWeaponInHand);
            arm.held = held ? held : field<void*>(local.chr, off::kChWeaponStored);
            g_nextNpcWeapons = 0.0;  // la liste des armes des PNJ part sans attendre son tour
            return;
        }
        return;
    }
    auto found = g_npcs.find(message.characterID);
    if (found == g_npcs.end() || !Valid(found->second)) return;
    Puppet& puppet = found->second;
    bool stripped = StripWeapon(puppet.chr, message.name);
    HoldDisarmed(puppet, message.name, now);
    LOG("desarmement : le joueur %u a pris '%s' a '%s'%s", from, message.name, mem::Name(puppet.chr),
        stripped ? " : retiree ici aussi" : "");
}

// Machine qui simule les PNJ : une seule arme par PNJ. Si un PNJ reçoit une arme neuve (du moteur,
// d'un script) alors que celle qu'il avait lâchée, de la même classe, est encore par terre, c'est
// celle-là qui est retirée : il n'y en a jamais deux. Neuve : sa case de la table des armes était
// libre à l'image précédente ; une arme qui existait déjà, il l'a ramassée, et rien n'est en
// double. Une arme qu'un joueur lui a prise n'est plus au sol, et reste au joueur. Le journal note
// chaque arme perdue, reprise ou reçue.
static void GuardNpcWeapons() {
    if (!cfg::Get().npcs || !SimulatesNpcs() || !net::Running()) {
        g_npcArms.clear();
        g_weaponsSeen = false;
        return;
    }
    static std::vector<LocalNpc> locals;
    ListLocalNpcs(locals);
    for (const LocalNpc& npc : locals) {
        uint16_t handle = mem::Handle(npc.chr);
        if (mem::IsDead(npc.chr)) {
            g_npcArms.erase(handle);  // son arme est à qui la ramasse
            continue;
        }
        NpcArm& arm = g_npcArms[handle];
        void* now = field<void*>(npc.chr, off::kChWeaponInHand);
        if (!now) now = field<void*>(npc.chr, off::kChWeaponStored);
        if (arm.dropped && !mem::WeaponOnGround(arm.dropped)) arm.dropped = nullptr;  // ramassée par quelqu'un
        if (now == arm.held) continue;
        const char* name = mem::Name(npc.chr);
        if (arm.held && !now && mem::WeaponOnGround(arm.held)) {
            arm.dropped = arm.held;
            LOG("arme du PNJ '%s' : '%s' lachee", name, mem::WeaponClassName(arm.held));
        } else if (now && now == arm.dropped) {
            arm.dropped = nullptr;
            LOG("arme du PNJ '%s' : '%s' reprise", name, mem::WeaponClassName(now));
        } else if (now) {
            int slot = mem::WeaponIndex(now);
            bool fresh = g_weaponsSeen && slot >= 0 && !g_weaponExisted[slot];
            if (arm.held && mem::WeaponOnGround(arm.held)) arm.dropped = arm.held;
            if (fresh && arm.dropped && strcmp(mem::WeaponClassName(arm.dropped), mem::WeaponClassName(now)) == 0) {
                LOG("arme du PNJ '%s' : il recoit une '%s' neuve alors que la sienne est au sol : celle du sol est retiree",
                    name, mem::WeaponClassName(now));
                mem::DeleteWeapon(arm.dropped);
                arm.dropped = nullptr;
            } else if (arm.held || arm.dropped) {
                LOG("arme du PNJ '%s' : '%s' %s", name, mem::WeaponClassName(now), fresh ? "recue, neuve" : "ramassee");
            }
        }
        arm.held = now;
    }
    for (int i = 0; i < addr::kMaxWeapons; i++) g_weaponExisted[i] = mem::WeaponInUse(mem::WeaponAt(i));
    g_weaponsSeen = true;
}

static void UpdateNpcs(double now) {
    const cfg::Config& config = cfg::Get();
    g_doubles.clear();
    if (config.npcs && net::CurrentMode() == cfg::Mode::Host && net::Connected()) SendNpcWeapons(now);
    if (!config.npcs || net::CurrentMode() != cfg::Mode::Client || !net::Connected()) return;
    if (mem::GameTime() < kSpawnDelayTicks) return;

    if (now >= g_nextPassive) {
        g_nextPassive = now + kPassiveInterval;
        mem::Exec("ai2_allpassive 1");
        g_passiveApplied = true;
    }
    if (now - g_lastSnapshot > kSnapshotFresh) {
        g_orphanSince.clear();  // pas de données fraîches : on ne touche à rien
        for (auto& entry : g_npcs) {
            if (Valid(entry.second)) ReleaseTriggers(entry.second);  // sauf aux armes, qui tireraient sans fin
        }
        return;
    }

    std::vector<LocalNpc> locals;
    ListLocalNpcs(locals);

    // Noms des PNJ d'ici que l'hôte connaît : un inconnu qui porte l'un d'eux est un double.
    std::vector<uint32_t> knownNames;
    for (const LocalNpc& local : locals) {
        auto found = g_npcs.find(local.id);
        if (found != g_npcs.end() && now - found->second.lastSeen < kSnapshotFresh) knownNames.push_back(local.nameHash);
    }

    for (const LocalNpc& local : locals) {
        auto found = g_npcs.find(local.id);
        bool known = found != g_npcs.end() && now - found->second.lastSeen < kSnapshotFresh;
        uint16_t handle = mem::Handle(local.chr);

        if (!known) {
            if (found != g_npcs.end() && Valid(found->second)) ReleaseTriggers(found->second);
            if (mem::IsDead(local.chr)) continue;  // cadavre local : sans conséquence
            // L'hôte ne connaît pas ce personnage : il n'existe pas dans la partie de référence.
            // S'il en connaît un du même nom, celui-ci est un double (le même PNJ créé deux fois
            // ici) : caché tout de suite, retiré presque aussitôt.
            bool twin = std::find(knownNames.begin(), knownNames.end(), local.nameHash) != knownNames.end();
            if (twin) g_doubles[handle] = 1;
            auto since = g_orphanSince.emplace(handle, now).first;
            if (config.deleteOrphans && now - since->second > (twin ? kTwinDelay : kOrphanDelay)) {
                LOG("pnj orphelin : suppression de '%s'%s", mem::Name(local.chr),
                    twin ? ", en double d'un PNJ que l'hote connait" : "");
                g_orphanSince.erase(since);
                g_doubles.erase(handle);
                mem::DeleteCharacter(local.chr);
            }
            continue;
        }
        g_orphanSince.erase(handle);

        Puppet& puppet = found->second;
        if (!Valid(puppet) || puppet.chr != local.chr) {
            Bind(puppet, local.chr);
            // Il tire quand l'hôte le dit, sans IA pour recharger.
            mem::SetInfiniteAmmo(local.chr);
            // Et reste actif où que soit notre joueur : mis en sommeil par le moteur d'ici, il ne
            // jouerait plus les animations que l'hôte lui donne.
            bool awake = mem::LockActive(local.chr, true);
            LOG("pnj pilote : '%s'%s", mem::Name(local.chr), awake ? "" : " (plus de place pour le garder actif)");
        }

        SyncNpcHealth(puppet, now);
        if (!mem::IsDead(local.chr)) {
            Drive(puppet, now, kNpcDelayTicks);
            if (void* inHand = field<void*>(local.chr, off::kChWeaponInHand)) puppet.ownWeapon = inHand;
            FixNpcWeapon(puppet, now);
        }
    }

    // PNJ présents chez l'hôte mais pas ici : demander au niveau de les créer.
    for (auto it = g_npcs.begin(); it != g_npcs.end();) {
        Puppet& puppet = it->second;
        if (now - puppet.lastSeen > kStateTimeout) {
            it = g_npcs.erase(it);
            continue;
        }
        bool fresh = now - puppet.lastSeen < kSnapshotFresh;
        bool alive = !(puppet.latest.flags & proto::kEntityDead);
        // Son personnage d'ici vient de disparaître : retiré par un script du niveau que l'hôte
        // nous fait rejouer (chr_delete), le plus souvent. Les derniers états reçus de l'hôte le
        // disent encore en vie, et le recréer sur leur foi le faisait revenir pour de bon alors que
        // l'hôte ne l'a plus : Barabas, retiré à la fin de sa cinématique, réapparaissait en l'air,
        // retombait et restait planté là (journal du client de la 0.29 : `ai2_spawn Barabus force`
        // à l'instant de `cinematique : finie`). Il n'est recréé que si l'hôte le donne encore
        // une demi-seconde après sa disparition.
        if (puppet.chr && !Valid(puppet)) {
            puppet.chr = nullptr;
            puppet.lostAt = now;
        }
        bool proven = puppet.lostAt == 0.0 || puppet.lastSeen > puppet.lostAt + kRespawnProof;
        if (fresh && alive && proven && !Valid(puppet) && puppet.spawnTries < kMaxNpcSpawnTries &&
            now >= puppet.nextSpawnTry) {
            auto name = g_npcNames.find(it->first);
            if (name != g_npcNames.end()) {
                puppet.nextSpawnTry = now + kSpawnRetry;
                puppet.spawnTries++;
                // Renfort ou civil ajouté par le mod chez l'hôte : le niveau ne le connaît pas, il
                // est créé ici au même endroit.
                Vector3 position = {puppet.latest.x, puppet.latest.y, puppet.latest.z};
                if (!spawn::Create(name->second.c_str(), position, puppet.latest.rotation)) {
                    mem::Exec("ai2_spawn %s force", name->second.c_str());
                }
            }
        }
        ++it;
    }
}

// -------------------------------------------------------------------------------------------------
// À plusieurs, des PNJ plus résistants
// -------------------------------------------------------------------------------------------------
// Tant que des clients partagent les PNJ de l'hôte, chacun d'eux, boss compris, a plus de vie : ce
// que le fichier de difficulté de l'hôte donne pour ce nombre de clients, deux fois la vie
// d'origine avec un client et quatre avec trois s'il n'y a pas touché. Son maximum et sa santé du
// moment suivent, à la hausse quand un client arrive comme à la baisse quand il s'en va. Chez
// l'hôte, qui décide de leur santé, c'est ce qui compte. Chez un client la santé vient de l'hôte ;
// le maximum y sert à la jauge. Le pourcentage est tenu par coop::Update, qui y compte ce que le
// mode survie ajoute.
//
// La difficulté du jeu d'origine entre aussi dans la vie d'un ennemi, mais le moteur ne l'applique
// qu'à sa création : quand l'hôte change de mode en cours de niveau, les ennemis déjà là passent
// ici de ce que valait l'ancienne difficulté à ce que vaut la nouvelle.
static void ScaleNpcHealth() {
    uint32_t percent = coop::NpcHealthPercent();
    float factor = mem::EnemyHealthFactor(mem::Difficulty());

    int changed = 0, restored = 0;
    unsigned shown = 100;
    int count = mem::CharacterCount();
    for (int i = 0; i < count; i++) {
        Character* chr = mem::CharacterAt(i);
        ScaledHealth& record = g_scaled[i];
        // Un civil garde sa vie d'origine : c'est sa fragilité qui gêne les joueurs.
        if (!chr || !IsReplicatedNpc(chr) || spawn::IsCivilian(chr)) {
            record = {};
            continue;
        }
        uint32_t& maxHealth = field<uint32_t>(chr, off::kChMaxHealth);
        uint32_t& health = field<uint32_t>(chr, off::kChHealth);
        // Un autre personnage a pu prendre la case, ou un script lui donner une autre classe : le
        // maximum n'est alors plus celui que nous avions posé, et sa vie est celle d'origine.
        bool known = record.baseMax != 0 && record.handle == mem::Handle(chr) && record.maxHealth == maxHealth;
        if (!known) {
            if (maxHealth == 0 || maxHealth > 10000) continue;
            record = {mem::Handle(chr), maxHealth, maxHealth, factor};
        }
        uint32_t wanted = percent;
        if (HostileToPlayers(chr) && record.spawnFactor != factor) {
            wanted = static_cast<uint32_t>(static_cast<float>(percent) * factor / record.spawnFactor + 0.5f);
        }
        if (difficulty::ScaledMax(record.baseMax, wanted) == maxHealth) continue;

        difficulty::Rescale(maxHealth, health, record.baseMax, wanted, mem::IsDead(chr));
        record.maxHealth = maxHealth;
        if (wanted != 100) {
            changed++;
            shown = wanted;
        } else {
            restored++;
        }
    }
    if (changed) {
        LOG("pnj : vie portee a %u pour cent de l'origine pour %d personnage(s)", shown, changed);
    }
    if (restored) LOG("pnj : vie d'origine rendue a %d personnage(s)", restored);
}

// Mode survie : les ennemis se soignent d'eux-mêmes, un point par seconde, deux pour un boss. Sur la
// machine qui les simule ; les autres tiennent leur santé d'elle.
static void RegenerateNpcs(double now) {
    static double next = 0.0;
    if (now < next) return;
    next = now + 1.0;
    const rules::ModeRules& mode = rules::Of(coop::Difficulty());
    if ((!mode.regenEnemy && !mode.regenBoss) || !SimulatesNpcs()) return;

    int count = mem::CharacterCount();
    for (int i = 0; i < count; i++) {
        Character* chr = mem::CharacterAt(i);
        if (!chr || !IsReplicatedNpc(chr) || mem::IsDead(chr) || !HostileToPlayers(chr)) continue;
        uint32_t& health = field<uint32_t>(chr, off::kChHealth);
        uint32_t maxHealth = field<uint32_t>(chr, off::kChMaxHealth);
        if (health == 0 || health >= maxHealth) continue;
        uint32_t gain = rules::IsBossClass(mem::ClassName(chr)) ? mode.regenBoss : mode.regenEnemy;
        health = health + gain > maxHealth ? maxHealth : health + gain;
    }
}

// Un joueur à terre n'est plus une menace, et entre deux joueurs un PNJ préfère celui qui est
// debout (voir ClosestPlayerFirst). Mais le moteur ne revoit la cible d'une IA que lorsqu'un contact
// lui arrive ou change (0x490745) : les PNJ qui se battaient avec le joueur tombé restaient plantés
// autour de lui, sans le frapper puisqu'il est au sol, et ne se tournaient vers un autre joueur que
// s'il venait se faire toucher. Deux fois par seconde, la machine qui simule les PNJ donne donc à
// ceux qui entourent un joueur à terre la connaissance du joueur debout le plus proche d'eux,
// comme le ferait la commande de script ai2_makeaware : le moteur revoit alors leur cible, et
// choisit celui qui est debout.
static void RetargetFromDowned(double now) {
    static double next = 0.0;
    static double nextLog = 0.0;
    if (now < next) return;
    next = now + kRetargetInterval;
    if (!SimulatesNpcs() || !net::Connected()) return;

    Character* standing[proto::kMaxPlayers];
    Character* down[proto::kMaxPlayers];
    int standingCount = 0, downCount = 0;
    Character* local = mem::Player();
    if (local && !mem::IsDead(local)) (revive::Downed() ? down[downCount++] : standing[standingCount++]) = local;
    for (const Puppet& remote : g_remotes) {
        if (!Valid(remote) || !remote.hasState || (remote.latest.flags & proto::kEntityDead)) continue;
        bool downed = (remote.latest.flags & proto::kEntityDowned) != 0;
        (downed ? down[downCount++] : standing[standingCount++]) = remote.chr;
    }
    if (!downCount || !standingCount) return;

    static std::vector<LocalNpc> locals;
    ListLocalNpcs(locals);
    int redirected = 0;
    for (const LocalNpc& npc : locals) {
        Character* chr = npc.chr;
        if (mem::IsDead(chr) || !HostileToPlayers(chr)) continue;
        if (field<uint32_t>(chr, off::kChAiFlags) & (off::kAiFlagPassive | off::kAiFlagNonCombatant)) continue;
        Vector3 origin = mem::Location(chr);
        bool around = false;
        for (int i = 0; i < downCount && !around; i++) {
            around = DistanceSquared(origin, mem::Location(down[i])) < kDownedRadius * kDownedRadius;
        }
        if (!around) continue;
        Character* best = nullptr;
        float bestDistance = kRetargetRange * kRetargetRange;
        for (int i = 0; i < standingCount; i++) {
            float distance = DistanceSquared(origin, mem::Location(standing[i]));
            if (distance >= bestDistance) continue;
            bestDistance = distance;
            best = standing[i];
        }
        if (!best) continue;
        fn::AiMakeAware(chr, best, 0, 0);
        redirected++;
    }
    if (redirected && now >= nextLog) {
        nextLog = now + 2.0;
        LOG("cible : %d PNJ autour d'un joueur a terre diriges vers un joueur debout", redirected);
    }
}

void Update(double now) {
    g_now = now;
    g_frame++;
    if (!mem::Player()) return;
    ScaleNpcHealth();       // avant la santé reçue de l'hôte, déjà multipliée
    RegenerateNpcs(now);
    ApplyIncoming(now);
    WatchLocalThrows(now);  // avant de piloter : un pantin qui vient d'être projeté ne l'est plus
    TrackCutscene();        // avant les pantins : cachés dès la première image de la cinématique
    GuardControl(now);
    UpdateRemotePlayers(now);
    WatchDisarm(now);       // avant le relevé des armes des PNJ : celle qu'on vient de lui prendre n'est pas « lâchée »
    GuardNpcWeapons();
    KeepNpcsActive();
    UpdateNpcs(now);
    RetargetFromDowned(now);
}

// -------------------------------------------------------------------------------------------------
// Dégâts : qui décide de quoi
// -------------------------------------------------------------------------------------------------
// Un personnage simulé ici a touché le pantin d'un autre joueur : un PNJ chez l'hôte, ou notre
// joueur quand le tir ami est activé. Le pantin est invincible ; c'est le vrai joueur, sur sa
// machine, qui encaisse.
static void ForwardHit(const mem::DamageInfo& info, const Puppet& victim) {
    proto::Hit hit = {};
    hit.levelHash = mem::LevelHash();
    hit.targetID = static_cast<uint8_t>(PlayerOf(victim));
    if (info.attacker == mem::Player()) {
        hit.attackerID = LocalPlayerId();
        hit.attackerIsPlayer = 1;
    } else {
        LocalNpcId(info.attacker, hit.attackerID);
    }
    hit.impact = info.impact;
    hit.knockback = info.knockback;
    if (info.direction) {
        hit.hasDirection = 1;
        hit.direction[0] = info.direction->x;
        hit.direction[1] = info.direction->y;
        hit.direction[2] = info.direction->z;
    }
    hit.damage = static_cast<uint16_t>(info.damage > 0xFFFF ? 0xFFFF : info.damage);
    hit.ownerType = info.ownerType == fn::kOwnerWeapon ? 1 : 2;
    SendReliableMessage(proto::kReliableHit, &hit, sizeof(hit));
}

// Dégâts en ligne : un PNJ frappe plus ou moins fort selon le nombre de clients (réglage et fichier
// de difficulté de l'hôte) et le mode de difficulté. Le coup est changé sur la machine qui simule le
// PNJ, qu'il touche son joueur ou celui d'un client. Ce que l'arrondi laisse de côté est gardé, par
// joueur frappé, pour le coup suivant.
static uint32_t ScaleNpcDamage(uint32_t damage, int victim) {
    static uint32_t carry[proto::kMaxPlayers] = {};
    if (victim < 0 || victim >= proto::kMaxPlayers) return damage;
    return difficulty::ScaleDamage(damage, coop::NpcDamagePercent(), carry[victim]);
}

// Coup d'un PNJ simulé ici sur notre joueur : grossi selon la difficulté. S'il porte, il interrompt
// la réanimation que notre joueur menait.
static void NpcHitsLocalPlayer(mem::DamageInfo& info) {
    info.damage = ScaleNpcDamage(info.damage, static_cast<int>(LocalPlayerId()));
    if (info.damage > 0) revive::OnLocalHit();
}

// Appelé par le moteur pour chaque dégât, pendant sa propre mise à jour. Toutes les machines
// simulent les mêmes combats avec un léger décalage : sans arbitrage, un même coup compterait
// plusieurs fois, ou pas du tout. La règle : la santé d'un personnage ne change que sur la machine qui
// le simule (l'hôte pour les PNJ, chacun pour son joueur), et un coup est jugé par la machine qui
// simule l'attaquant. Un coup porté par un pantin passe donc ici avec des dégâts nuls : il garde
// ses effets (recul, bruit, alerte de l'IA) sans toucher à la santé.
static void FilterDamage(mem::DamageInfo& info) {
    Character* player = mem::Player();
    // Invincibilité de notre joueur (son cheat, tant que l'hôte laisse les cheats ouverts), joueur à
    // terre ou tout juste relevé : rien ne le blesse. Seule passe la mise à mort directe du moteur
    // (AI2rSmite, celle de ses commandes de script), qui n'est pas un coup.
    if ((cheat::GodMode() || revive::Shielded()) && player && info.victim == player &&
        info.ownerType != fn::kOwnerActOfGod) {
        info.damage = 0;
        return;
    }
    // Saut réglé au-dessus de la normale : retomber de plus haut ne blesse pas. Le moteur compte les
    // dégâts d'une chute à l'atterrissage (0x4EC98A), et tue qui reste cinq secondes en l'air
    // (0x4DEB65) : les deux sont des dégâts « de chute ».
    if (player && info.victim == player && info.ownerType == fn::kOwnerFall && cheat::NoFallDamage()) {
        info.damage = 0;
        return;
    }
    // Coup fatal : un coup ou un tir de notre joueur achève l'ennemi qu'il touche. Jamais un autre
    // joueur : le tir ami garde ses dégâts.
    if (player && info.attacker == player && info.damage > 0 && !RemoteOf(info.victim)) {
        info.damage = cheat::StrikeDamage(player, info.victim, info.damage);
    }
    // Notre joueur blesse quelqu'un : si c'est un civil et qu'il en meurt, la mission échoue.
    if (player && info.attacker == player && info.damage > 0) spawn::OnPlayerDamage(info.victim, LocalPlayerId());
    bool npcOnPlayer = player && info.victim == player && info.attacker && info.attacker != player &&
                       !IsRemotePlayer(info.attacker);
    if (!net::Connected()) {
        // Seul en session : nos PNJ, notre joueur, rien à arbitrer. Reste le mode de difficulté.
        if (npcOnPlayer && net::Running()) NpcHitsLocalPlayer(info);
        return;
    }
    Puppet* victim = RemoteOf(info.victim);  // pantin d'un autre joueur, si c'est lui qui est touché
    bool host = net::CurrentMode() == cfg::Mode::Host;

    // Pantin d'un autre joueur : sa machine juge ses coups et les annonce. Seule exception, la
    // chute d'une projection qu'elle a annoncée et que le moteur rejoue ici. Contre notre joueur,
    // rien ne passe tant que le tir ami est coupé.
    if (const Puppet* attacker = RemoteOf(info.attacker)) {
        if (info.hit || (info.victim == player && !coop::TeamKill())) info.damage = 0;
        // La chute d'une projection compte ici : si c'est un civil qui la subit, ce joueur en répond.
        if (info.damage > 0) spawn::OnPlayerDamage(info.victim, static_cast<uint8_t>(PlayerOf(*attacker)));
        return;
    }
    // Tir ami : notre joueur a touché le pantin d'un autre. Annoncé à sa machine, où le vrai
    // joueur encaisse. Une projection ne l'est pas : elle a déjà son annonce, rejouée là-bas.
    if (victim && info.attacker == player) {
        if (coop::TeamKill() && info.hit && info.damage > 0) {
            LOG("tir ami : %u sur le joueur %d, annonce", info.damage, PlayerOf(*victim));
            ForwardHit(info, *victim);
        }
        return;
    }
    if (!cfg::Get().npcs) {  // chacun ses PNJ : rien d'autre à arbitrer
        if (npcOnPlayer) NpcHitsLocalPlayer(info);
        return;
    }

    if (victim) {
        if (host && info.attacker && info.attacker != player) {
            NoteRemoteHit(*victim, false);
            // Les dégâts portés par une animation (chute d'une projection) ne comptent que si le
            // moteur d'ici la joue de lui-même. Quand le pantin ne fait que répéter l'animation
            // du vrai joueur, ils ont déjà été annoncés.
            if (info.damage > 0 && (info.hit || victim->fallAnim)) {
                mem::DamageInfo scaled = info;
                scaled.damage = ScaleNpcDamage(info.damage, PlayerOf(*victim));
                ForwardHit(scaled, *victim);
            }
        }
        return;
    }
    if (host) {
        // PNJ et joueur de l'hôte sont simulés ici : rien à arbitrer. Reste à grossir le coup d'un
        // PNJ sur notre joueur.
        if (npcOnPlayer) NpcHitsLocalPlayer(info);
        return;
    }

    if (info.victim == player) {
        // Les PNJ d'ici sont des pantins : l'hôte nous annonce ceux de leurs coups qui portent.
        // Il en va de même d'une chute qu'il nous impose : faute de lanceur connu ici, notre
        // joueur s'en retrouve l'auteur (voir mem::SetAnimation).
        if (info.attacker && (info.attacker != player || !info.hit)) info.damage = 0;
        return;
    }
    uint32_t id = 0;
    Puppet* puppet = FindNpcPuppet(info.victim, &id);
    if (!puppet) return;
    if (info.attacker != player) {
        info.damage = 0;  // entre pantins, ou par le décor : l'hôte seul en décide
        return;
    }
    // Notre joueur frappe un PNJ : appliqué ici tout de suite, annoncé à l'hôte qui tranche. La
    // chute d'une projection n'est pas annoncée, l'hôte la rejoue lui-même.
    if (mem::IsDead(info.victim)) return;
    if (info.hit && info.damage > 0) {
        int32_t damage = static_cast<int32_t>(info.damage > 10000 ? 10000 : info.damage);
        net::SendEvent(proto::kEventDamage, id, damage, mem::LevelHash());
        puppet->pendingDamage += damage;
        puppet->pendingUntil = g_now + kPendingDamageTime;
    }
    // Le coup ne l'achève pas ici : sa mort, c'est l'hôte qui l'annonce. Tué sur place, il était
    // ramené à la vie par l'état suivant de l'hôte puis tué de nouveau, et laissait tomber deux
    // fois ses chargeurs et ses hypos.
    int32_t health = mem::Health(info.victim);
    if (health > 0 && info.damage >= static_cast<uint32_t>(health)) info.damage = static_cast<uint32_t>(health - 1);
}

bool InstallDamageFilter() { return mem::InstallDamageHook(&FilterDamage); }

// Personnage piloté par le réseau : son IA est à l'arrêt.
static bool IsPuppet(Character* chr) { return IsRemotePlayer(chr) || FindNpcPuppet(chr, nullptr) != nullptr; }

bool InstallShooterHook() { return mem::InstallShooterHook(&IsPuppet); }

// -------------------------------------------------------------------------------------------------
// Zones de déclenchement : les joueurs des clients comptent
// -------------------------------------------------------------------------------------------------
// Les scripts du niveau (apparition des ennemis, objectifs, sauvegardes) sont lancés par des
// zones que le joueur traverse. Presque toutes sont réservées au joueur : le moteur y écarte
// tout personnage d'un autre type, donc le pantin d'un client. Quand un client prenait de l'avance
// sur l'hôte, rien ne se déclenchait. Chez l'hôte, le temps de cette vérification, les pantins des
// clients se présentent comme des joueurs.
static fn::ObjectCallback g_triggerUpdate = nullptr;

static uint8_t ONICALL ONIMP_ENTRY TriggerUpdateHook(void* volume, void* userData) {
    if (net::CurrentMode() != cfg::Mode::Host) return g_triggerUpdate(volume, userData);

    Character* remotes[proto::kMaxPlayers] = {};
    int32_t saved[proto::kMaxPlayers] = {};
    for (int i = 0; i < proto::kMaxPlayers; i++) {
        // Une place qui n'est pas un nombre passe pour être dans toutes les zones à la fois : en
        // 0.28 le pantin d'un client dans cet état a déclenché d'un coup celles de tout le niveau.
        if (!Valid(g_remotes[i]) || !mem::TransformValid(g_remotes[i].chr)) continue;
        remotes[i] = g_remotes[i].chr;
        int32_t& type = field<int32_t>(remotes[i], off::kChType);
        saved[i] = type;
        type = off::kChTypePlayer;
    }
    uint8_t result = g_triggerUpdate(volume, userData);
    for (int i = 0; i < proto::kMaxPlayers; i++) {
        if (remotes[i]) field<int32_t>(remotes[i], off::kChType) = saved[i];
    }
    return result;
}

bool InstallTriggerHook() {
    void* original = reinterpret_cast<void*>(addr::kTriggerCallback);
    g_triggerUpdate = reinterpret_cast<fn::ObjectCallback>(original);
    return mem::PatchPointer(addr::kTriggerCallbackOperand, original, reinterpret_cast<void*>(&TriggerUpdateHook));
}

// -------------------------------------------------------------------------------------------------
// Zones de déclenchement : tous les joueurs ne comptent que pour un
// -------------------------------------------------------------------------------------------------
// Des scripts de niveau demandent combien de personnages se trouvent dans une zone, et n'agissent
// que s'il y en a « exactement un », le joueur : `if (trigvolume_count (63) eq 1) ai2_spawn A_t48`
// à la fin de la cinématique d'ouverture de l'entrepôt. Le moteur compte tout personnage d'une
// équipe que la zone retient ; le pantin d'un autre joueur est de l'équipe du joueur. À deux le
// compte valait deux : le premier ennemi n'apparaissait pas, donc ni la porte ni l'objectif que sa
// mort commande (journal de l'hôte de la 0.30 : plus rien après `cinematique : finie`). Les
// joueurs présents dans la zone, le nôtre et les pantins des autres, comptent donc ensemble pour un.
typedef uint32_t(ONICALL* TriggerCountFunction)(uint32_t id);
static void* g_triggerCount = nullptr;

struct PlayersInVolume {
    uint32_t id;
    int extra;   // joueurs en trop dans le compte du moteur
};

static uint8_t ONICALL ONIMP_ENTRY CountPlayersInVolume(void* volume, void* userData) {
    PlayersInVolume* search = static_cast<PlayersInVolume*>(userData);
    if (field<uint32_t>(volume, off::kTrgvId) != search->id) return 1;
    uint32_t teams = field<uint32_t>(volume, off::kTrgvTeams);
    int players = 0;
    Character* local = mem::Player();
    if (local && !mem::IsDead(local) && fn::VolumeContains(volume, teams, local)) players++;
    for (const Puppet& remote : g_remotes) {
        if (Valid(remote) && !mem::IsDead(remote.chr) && fn::VolumeContains(volume, teams, remote.chr)) players++;
    }
    if (players > 1) search->extra += players - 1;
    return 1;  // continuer : plusieurs zones peuvent porter le même numéro
}

static uint32_t ONICALL ONIMP_ENTRY TriggerCountHook(uint32_t id) {
    uint32_t count = reinterpret_cast<TriggerCountFunction>(g_triggerCount)(id);
    if (count < 2 || !AnyRemotePlayer()) return count;
    PlayersInVolume search = {id, 0};
    fn::EnumerateObjects(Tag('T', 'R', 'G', 'V'), CountPlayersInVolume, &search);
    if (search.extra <= 0 || static_cast<uint32_t>(search.extra) >= count) return count;
    static double nextLog = 0.0;
    if (g_now >= nextLog) {
        nextLog = g_now + 5.0;
        LOG("zones : zone %u, %u personnage(s) dont %d joueur(s) en plus du premier : comptes pour un, soit %u",
            static_cast<unsigned>(id), static_cast<unsigned>(count), search.extra,
            static_cast<unsigned>(count) - static_cast<unsigned>(search.extra));
    }
    return count - static_cast<uint32_t>(search.extra);
}

bool InstallTriggerCountHook() {
    if (mem::CallTarget(addr::kTriggerCountCallSite) != reinterpret_cast<void*>(addr::kTriggerCountTarget)) return false;
    return mem::PatchCall(addr::kTriggerCountCallSite, reinterpret_cast<void*>(&TriggerCountHook), &g_triggerCount);
}

// -------------------------------------------------------------------------------------------------
// Bonus : le pantin d'un autre joueur ne ramasse rien par lui-même
// -------------------------------------------------------------------------------------------------
// Un joueur qui ramasse un bonus joue une animation, et c'est à la frame d'action de celle-ci que
// le moteur lui donne le bonus le plus proche de lui. Son pantin, sur les autres machines, rejoue
// la même animation : le moteur de ces machines lui donnait donc, à lui aussi, le bonus le plus
// proche. Or le vrai ramassage y était déjà arrivé par son annonce, qui avait retiré le bon bonus :
// le plus proche était alors le voisin. Cette machine, voyant disparaître un bonus près de son
// propre joueur, l'annonçait comme ramassé par lui, et les autres le retiraient. Un bonus perdu
// pour tout le monde chaque fois qu'un joueur en ramassait un à côté d'un autre (journaux de la
// 0.32, entrepôt : l'hôte annonce un chargeur à 01:38:36.081, le client annonce un hypo à
// 01:38:36.190, que personne n'a reçu ; de même à 01:32:08). Seule l'annonce du joueur qui ramasse
// fait foi : pour un pantin, la recherche du moteur ne trouve rien.
typedef void*(ONICALL* PickupSearchFunction)(Character* chr, const Vector3* position, float low, float high, void* only,
                                             float reachSquared, uint8_t* full);
static void* g_pickupSearch = nullptr;

static void* ONICALL ONIMP_ENTRY PickupSearchHook(Character* chr, const Vector3* position, float low, float high,
                                                  void* only, float reachSquared, uint8_t* full) {
    if (IsRemotePlayer(chr)) {
        if (full) *full = 0;
        return nullptr;
    }
    return reinterpret_cast<PickupSearchFunction>(g_pickupSearch)(chr, position, low, high, only, reachSquared, full);
}

bool InstallPickupHook() {
    if (mem::CallTarget(addr::kPickupSearchCallSite) != reinterpret_cast<void*>(addr::kPickupSearchTarget)) return false;
    return mem::PatchCall(addr::kPickupSearchCallSite, reinterpret_cast<void*>(&PickupSearchHook), &g_pickupSearch);
}

// -------------------------------------------------------------------------------------------------
// IA : le pantin d'un autre joueur est une menace
// -------------------------------------------------------------------------------------------------
// Le pantin est en ai2_passive pour que son IA ne le pilote pas. Or le moteur range un personnage
// passif parmi les hostiles sans menace, comme un civil : un PNJ qui connaît aussi une vraie
// menace, le joueur local, n'attaque qu'elle. Pour cette seule question, le pantin répond comme
// un personnage actif de son équipe.
typedef uint8_t(ONICALL* PotentiallyHostileFunction)(Character* a, Character* b, uint32_t viaKnowledge);

static uint8_t ONICALL ONIMP_ENTRY PotentiallyHostileHook(Character* a, Character* b, uint32_t viaKnowledge) {
    auto original = reinterpret_cast<PotentiallyHostileFunction>(g_potentiallyHostile);
    // Un joueur à terre n'est plus une menace : les PNJ s'en prennent d'abord à ceux qui sont debout.
    if (net::Connected() && DownedPlayer(a)) return 0;
    if (!IsRemotePlayer(a)) return original(a, b, viaKnowledge);

    uint32_t& flags = field<uint32_t>(a, off::kChAiFlags);
    uint32_t passive = flags & off::kAiFlagPassive;
    flags &= ~off::kAiFlagPassive;
    uint8_t hostile = original(a, b, viaKnowledge);
    flags |= passive;
    return hostile;
}

bool InstallAiHook() {
    return mem::HookEntry(addr::kPotentiallyHostile, addr::kPotentiallyHostileEntry,
                          sizeof(addr::kPotentiallyHostileEntry), reinterpret_cast<void*>(&PotentiallyHostileHook),
                          &g_potentiallyHostile);
}

// -------------------------------------------------------------------------------------------------
// IA : entre deux joueurs, le plus proche
// -------------------------------------------------------------------------------------------------
// Entre deux adversaires qu'il voit aussi bien, le moteur garde pour cible celui qui a blessé le
// PNJ, et ne regarde les distances qu'ensuite : un PNJ frappé une fois par l'hôte ne le lâchait
// plus, même avec le joueur d'un client collé à lui. Chez l'hôte, quand la comparaison porte sur
// deux joueurs, quels qu'ils soient, c'est maintenant la distance qui décide.
typedef int32_t(ONICALL* CompareContactsFunction)(void* a, void* b, uint32_t keepCurrent, Character* owner);
static void* g_compareContacts = nullptr;  // fonction d'origine du moteur

// Pour changer de cible, l'autre joueur doit être nettement plus près : à distances voisines le
// PNJ hésiterait d'un tick à l'autre. C'est le rapport que le moteur applique lui-même (0x490806).
static const float kRetargetRatio = 0.8f;

static float DistanceSquared(const Vector3& a, const Vector3& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

static bool ClosestPlayerFirst(void* a, void* b, bool keepCurrent, Character* owner, int32_t& order) {
    if (!a || !b || !net::Connected() || !SimulatesNpcs()) return false;
    Character* player = mem::Player();
    Character* first = field<Character*>(a, off::kCtEntity);
    Character* second = field<Character*>(b, off::kCtEntity);
    if (!player || !first || !second || first == second) return false;
    if ((first != player && !IsRemotePlayer(first)) || (second != player && !IsRemotePlayer(second))) return false;
    if (mem::IsDead(first) || mem::IsDead(second)) return false;

    // Entre un joueur à terre et un joueur debout, c'est celui qui est debout.
    bool firstDown = DownedPlayer(first), secondDown = DownedPlayer(second);
    if (firstDown != secondDown) {
        order = firstDown ? 1 : -1;
        return true;
    }

    // Le reste ne vaut que pour des PNJ partagés, chez l'hôte : ailleurs chacun a les siens.
    if (net::CurrentMode() != cfg::Mode::Host || !cfg::Get().npcs) return false;

    // Contacts qui ne se valent pas : le moteur tranche. Cible imposée par un script, joueur
    // perdu de vue pendant que l'autre est en face.
    if (field<int32_t>(a, off::kCtPriority) != field<int32_t>(b, off::kCtPriority)) return false;
    int32_t strength = field<int32_t>(a, off::kCtStrength);
    if (strength != field<int32_t>(b, off::kCtStrength) || strength < off::kCtStrengthStrong) return false;

    Character* npc = owner ? owner : field<Character*>(a, off::kCtOwner);
    if (!npc) return false;
    Vector3 origin = mem::Location(npc);
    float toFirst = DistanceSquared(origin, mem::Location(first));
    float toSecond = DistanceSquared(origin, mem::Location(second));
    if (keepCurrent) {
        // a est la cible actuelle : seul un résultat positif en fait changer.
        order = toSecond < toFirst * kRetargetRatio * kRetargetRatio ? 1 : -1;
        // Au journal, une ligne par seconde et par PNJ au plus : si le moteur refusait le
        // changement, la question reviendrait à chaque tick.
        static int32_t lastLogged[off::kMaxCharacters];
        int32_t& logged = lastLogged[mem::Handle(npc) & (off::kMaxCharacters - 1)];
        int32_t time = mem::GameTime();
        if (order > 0 && (time < logged || time - logged >= 60)) {
            logged = time;
            LOG("cible : '%s' quitte '%s' pour '%s', plus proche (%.0f contre %.0f)", mem::Name(npc), mem::Name(first),
                mem::Name(second), std::sqrt(toSecond), std::sqrt(toFirst));
        }
    } else {
        order = toFirst <= toSecond ? -1 : 1;
    }
    return true;
}

static int32_t ONICALL ONIMP_ENTRY CompareContactsHook(void* a, void* b, uint32_t keepCurrent, Character* owner) {
    int32_t order = 0;
    if (ClosestPlayerFirst(a, b, (keepCurrent & 0xFF) != 0, owner, order)) return order;
    return reinterpret_cast<CompareContactsFunction>(g_compareContacts)(a, b, keepCurrent, owner);
}

bool InstallTargetHook() {
    return mem::HookEntry(addr::kCompareContacts, addr::kCompareContactsEntry, sizeof(addr::kCompareContactsEntry),
                          reinterpret_cast<void*>(&CompareContactsHook), &g_compareContacts);
}

// -------------------------------------------------------------------------------------------------
// Tir ami : mêlée et prises entre joueurs
// -------------------------------------------------------------------------------------------------
// Le moteur ignore un coup de mêlée porté à un personnage d'une équipe amie, et une prise ne
// choisit ni un ami ni un personnage invincible. Le pantin d'un autre joueur est les deux : nos
// coups le traversaient, et seuls les tirs passaient. Quand le tir ami est activé, le temps que
// le moteur juge un coup ou cherche la cible d'une prise de notre joueur, les pantins se
// présentent comme des adversaires. Dans l'autre sens rien ne change : les coups d'un pantin sont
// jugés par la machine de son joueur, qui nous les annonce.
class Opponent {
public:
    // only : le seul pantin concerné ; nullptr : tous, pour la recherche d'une cible.
    Opponent(Character* attacker, Character* only, bool vulnerable) {
        Character* player = mem::Player();
        if (!player || attacker != player || !coop::TeamKill() || !net::Connected()) return;

        uint16_t own = field<uint16_t>(player, off::kChTeam);
        const uint16_t candidates[] = {2, 0, 5, 3, 1, 4, 7};  // Syndicat d'abord : hostile à Konoko
        for (uint16_t team : candidates) {
            if (!fn::FriendOrFoe(own, team) || !fn::FriendOrFoe(team, own)) continue;
            for (Puppet& remote : g_remotes) {
                if (!Valid(remote) || (only && remote.chr != only)) continue;
                Saved& saved = saved_[count_++];
                saved.chr = remote.chr;
                saved.team = field<uint16_t>(remote.chr, off::kChTeam);
                field<uint16_t>(remote.chr, off::kChTeam) = team;
                if (vulnerable) {
                    uint32_t& flags = field<uint32_t>(remote.chr, off::kChFlags);
                    saved.invincible = flags & off::kChFlagInvincible;
                    flags &= ~off::kChFlagInvincible;
                }
            }
            return;
        }
    }
    ~Opponent() {
        for (int i = 0; i < count_; i++) {
            field<uint16_t>(saved_[i].chr, off::kChTeam) = saved_[i].team;
            field<uint32_t>(saved_[i].chr, off::kChFlags) |= saved_[i].invincible;
        }
    }
    Opponent(const Opponent&) = delete;
    Opponent& operator=(const Opponent&) = delete;

    bool active() const { return count_ > 0; }

private:
    struct Saved {
        Character* chr;
        uint16_t team;
        uint32_t invincible;
    };
    Saved saved_[proto::kMaxPlayers] = {};
    int count_ = 0;
};

typedef void(ONICALL* MeleeHitFunction)(Character* attacker, Character* target, uint32_t, uint32_t, uint32_t,
                                        uint32_t, uint32_t, uint32_t);
static void* g_meleeHit = nullptr;  // fonctions d'origine du moteur
static void* g_findTarget = nullptr;

static void ONICALL ONIMP_ENTRY MeleeHitHook(Character* attacker, Character* target, uint32_t a, uint32_t b,
                                             uint32_t c, uint32_t d, uint32_t e, uint32_t f) {
    auto original = reinterpret_cast<MeleeHitFunction>(g_meleeHit);
    Puppet* remote = RemoteOf(target);
    if (!remote) {
        original(attacker, target, a, b, c, d, e, f);
        return;
    }
    Opponent opponent(attacker, target, false);
    // Même parée, sans dégâts, l'attaque impose une animation au pantin : à annoncer (voir React).
    if (opponent.active()) NoteRemoteHit(*remote, true);
    original(attacker, target, a, b, c, d, e, f);
}

typedef Character*(ONICALL* FindTargetFunction)(Character* attacker, void* out, uint32_t, uint32_t, uint32_t,
                                                uint32_t);

static Character* ONICALL ONIMP_ENTRY FindTargetHook(Character* attacker, void* out, uint32_t a, uint32_t b,
                                                     uint32_t c, uint32_t d) {
    Opponent opponent(attacker, nullptr, true);
    return reinterpret_cast<FindTargetFunction>(g_findTarget)(attacker, out, a, b, c, d);
}

bool InstallMeleeHook() {
    bool melee = mem::HookEntry(addr::kMeleeHit, addr::kMeleeHitEntry, sizeof(addr::kMeleeHitEntry),
                                reinterpret_cast<void*>(&MeleeHitHook), &g_meleeHit);
    bool throws = mem::HookEntry(addr::kFindTarget, addr::kFindTargetEntry, sizeof(addr::kFindTargetEntry),
                                 reinterpret_cast<void*>(&FindTargetHook), &g_findTarget);
    return melee && throws;
}

// -------------------------------------------------------------------------------------------------
// Santé au-dessus des personnages
// -------------------------------------------------------------------------------------------------
// Le moteur a un affichage de mise au point, appelé pendant le rendu de la scène pour chaque
// personnage présent et vivant. Son appel est détourné : on y trace nos jauges. Pour un autre
// joueur, une barre seule, de la couleur de son numéro, et la santé est celle que sa machine
// transmet : son pantin, invincible ici, garde toujours la sienne. Pour un PNJ c'est la sienne, que
// le client tient de l'hôte, avec les chiffres : rouge pour un ennemi, violette pour un boss. Un
// joueur à terre, le nôtre compris, porte son compte à rebours à la place.
typedef void(ONICALL* OverheadFunction)(Character* chr);
static OverheadFunction g_overhead = nullptr;

static const float kNpcGaugeScale = 0.8f;
static const float kNpcGaugeRange = 600.0f;  // au-delà, la jauge d'un PNJ n'est plus qu'un point

// Vrai si ce joueur est à terre : son panneau est tracé à la place de sa jauge (voir
// gauge::DrawDowned). Celui d'un autre joueur est un repère : il se voit à travers les murs, garde
// sa taille au loin, et dit la touche à tenir quand on est à portée de main. Dans la langue du
// joueur.
static bool DrawCountdown(Character* chr, uint32_t id) {
    revive::Countdown countdown;
    if (!revive::GetCountdown(id, countdown)) return false;
    bool other = id != LocalPlayerId();
    gauge::Downed panel = {};
    panel.seconds = countdown.seconds;
    panel.remaining = countdown.remaining;
    panel.progress = countdown.progress;  // négatif : personne ne le ranime
    panel.reviving = countdown.reviving;
    panel.pulse = countdown.pulse;
    panel.title = loc::Tr(countdown.reviving ? "REANIMATION" : "A TERRE");
    panel.name = other ? roster::Name(static_cast<uint8_t>(id)) : nullptr;
    panel.hint = countdown.canRevive ? loc::Tr("CTRL POUR RANIMER") : nullptr;
    panel.beacon = other && coop::ShowThroughWalls();
    gauge::DrawDowned(chr, panel, gauge::PlayerColor(static_cast<int>(id)));
    return true;
}

static uint32_t NpcColor(Character* chr) {
    if (!HostileToPlayers(chr)) return gauge::kNeutralColor;
    return rules::IsBossClass(mem::ClassName(chr)) ? gauge::kBossColor : gauge::kEnemyColor;
}

// Identifiant de chaque arme au sol : le nom de sa classe, celui qu'attendent les commandes de
// script (chr_giveweapon, weapon_spawn). Tracé une fois par image, avec le premier personnage.
static void DrawWeaponLabels(Character* player) {
    Vector3 origin = mem::Location(player);
    for (int i = 0; i < addr::kMaxWeapons; i++) {
        void* weapon = mem::WeaponAt(i);
        if (!weapon || !mem::WeaponOnGround(weapon)) continue;
        Vector3 position = mem::WeaponPosition(weapon);
        if (DistanceSquared(origin, position) > kLabelRange * kLabelRange) continue;
        position.y += kWeaponLabelHeight;
        gauge::DrawLabelAt(position, mem::WeaponClassName(weapon), kWeaponLabelScale, gauge::kWeaponLabelColor);
    }
}

// Ce qui se lit au-dessus d'un autre joueur : sa jauge, ses réserves, son pseudo et sa latence, ou
// son panneau s'il est à terre.
static void DrawRemotePlayer(int id) {
    if (g_cutscene) return;  // pantins cachés : rien au-dessus d'eux non plus
    const Puppet& remote = g_remotes[id];
    if (static_cast<uint32_t>(id) == LocalPlayerId() || !Valid(remote) || !remote.hasState) return;
    if (remote.latest.flags & proto::kEntityDead) return;
    Character* chr = remote.chr;
    if (DrawCountdown(chr, static_cast<uint32_t>(id))) return;
    if (coop::ShowPlayerHealth()) {
        gauge::Draw(chr, remote.latest.health, static_cast<int32_t>(field<uint32_t>(chr, off::kChMaxHealth)), 1.0f,
                    false, gauge::PlayerColor(id));
    }
    // Ses réserves, telles que sa machine les donne : celles du pantin ne veulent rien dire.
    bool ammo = coop::ShowPlayerAmmo();
    if (ammo) {
        gauge::DrawReserves(chr, remote.latest.ammo & proto::kAmmoMask, remote.latest.ammo >> proto::kCellShift, 1.0f);
    }
    char plate[40];
    int ping = coop::ShowPing() ? roster::PingMs(static_cast<uint8_t>(id)) : -1;
    if (ping >= 0) snprintf(plate, sizeof(plate), "%s %dMS", roster::Name(static_cast<uint8_t>(id)), ping);
    else snprintf(plate, sizeof(plate), "%s", roster::Name(static_cast<uint8_t>(id)));
    gauge::DrawNameplate(chr, plate, 1.0f, gauge::PlayerColor(id), ammo ? gauge::ReservesHeight() : 0.0f);
}

// Vue à travers les murs (réglage de chaque joueur, onglet Display) : tous les autres joueurs sont
// tracés une fois par image, avec le premier personnage, et non quand le moteur trace le leur. Un
// joueur caché par un mur n'est pas tracé du tout par le moteur, et c'est justement là qu'on veut
// savoir où il est. Tout cela est rapproché de la caméra (gauge::SetOverlay), donc par-dessus le
// décor. Réglage coupé : chacun est tracé avec son personnage (voir OverheadHook), à sa vraie
// profondeur, et ne se voit que lorsque le joueur lui-même est en vue.
static void DrawRemotePlayers() {
    if (!net::Connected()) return;
    gauge::SetOverlay(true);
    for (int id = 0; id < proto::kMaxPlayers; id++) DrawRemotePlayer(id);
    gauge::SetOverlay(false);
}

static void ONICALL ONIMP_ENTRY OverheadHook(Character* chr) {
    g_overhead(chr);
    Character* player = mem::Player();
    if (!chr || !player || !net::Running()) return;
    static unsigned labelled = 0;
    if (labelled != g_frame) {
        labelled = g_frame;
        if (coop::ShowWeaponIds()) DrawWeaponLabels(player);
        if (coop::ShowThroughWalls()) DrawRemotePlayers();
    }
    if (chr == player) {
        // Le moteur ne montre au joueur que six chargeurs et six cellules, et il peut en porter
        // douze : au-delà de six, ses réserves se lisent au-dessus de lui, comme celles des autres.
        if (!DrawCountdown(chr, LocalPlayerId()) && coop::ShowPlayerAmmo() && !g_cutscene) {
            mem::Reserves own = mem::ReadReserves(chr);
            if (own.clips > 6 || own.cells > 6) gauge::DrawReserves(chr, own.clips, own.cells, 1.0f);
        }
        return;
    }

    if (const Puppet* remote = RemoteOf(chr)) {
        // Déjà tracé avec les autres joueurs, visible ou non ; sinon ici, parce qu'il est en vue.
        if (!coop::ShowThroughWalls() && net::Connected()) DrawRemotePlayer(static_cast<int>(remote - &g_remotes[0]));
        return;
    }
    // PNJ : sa santé, et son identifiant, le nom que lui donne le niveau et le même sur toutes les
    // machines. Chacun des deux se montre ou se masque à part.
    bool health = coop::ShowNpcHealth(), ids = coop::ShowNpcIds();
    if ((!health && !ids) || !IsReplicatedNpc(chr) || mem::IsDead(chr)) return;
    if (DistanceSquared(mem::Location(chr), mem::Location(player)) > kNpcGaugeRange * kNpcGaugeRange) return;
    if (health) {
        gauge::Draw(chr, mem::Health(chr), static_cast<int32_t>(field<uint32_t>(chr, off::kChMaxHealth)),
                    kNpcGaugeScale, true, NpcColor(chr), ids ? mem::Name(chr) : nullptr);
    } else {
        gauge::DrawLabel(chr, mem::Name(chr), kNpcGaugeScale, gauge::kLabelColor);
    }
}

bool InstallOverheadHook() {
    if (mem::CallTarget(addr::kOverheadCallSite) != reinterpret_cast<void*>(addr::kOverheadTarget)) return false;
    void* previous = nullptr;
    if (!mem::PatchCall(addr::kOverheadCallSite, reinterpret_cast<void*>(&OverheadHook), &previous)) return false;
    g_overhead = reinterpret_cast<OverheadFunction>(previous);
    return true;
}

// -------------------------------------------------------------------------------------------------
// Comparaison de l'hôte et d'un client (voir desync.cpp)
// -------------------------------------------------------------------------------------------------
static bool g_namesRequested = false;

// Empreinte des PNJ vivants : la même chez l'hôte et chez le client quand ils ont les mêmes.
uint32_t NpcDigest() {
    static std::vector<LocalNpc> locals;
    ListLocalNpcs(locals);
    uint32_t sum = 0, mix = 0, count = 0;
    for (const LocalNpc& local : locals) {
        if (mem::IsDead(local.chr)) continue;
        sum += local.id;
        mix ^= local.id * 2654435761u;
        count++;
    }
    return sum * 31u + mix + count;
}

void ResendNames() { g_namesRequested = true; }

bool TakeNamesRequest() {
    bool requested = g_namesRequested;
    g_namesRequested = false;
    return requested;
}

void RetrySpawns() {
    for (auto& entry : g_npcs) {
        entry.second.spawnTries = 0;
        entry.second.nextSpawnTry = 0.0;
    }
}

int NpcPuppetCount() {
    int count = 0;
    for (const auto& entry : g_npcs) {
        if (Valid(entry.second)) count++;
    }
    return count;
}

int RemotePlayerCount() {
    int count = 0;
    for (const Puppet& remote : g_remotes) count += Valid(remote) ? 1 : 0;
    return count;
}

void DescribeRemotePlayers(char* out, int capacity) {
    int length = 0;
    out[0] = '\0';
    for (const Puppet& remote : g_remotes) {
        if (!Valid(remote) || length >= capacity - 1) continue;
        Vector3 location = mem::Location(remote.chr);
        int written = snprintf(out + length, static_cast<size_t>(capacity - length),
                               "%sj%d (%.1f %.1f %.1f) anim='%s' frame=%d etats=%d", length ? " ; " : "",
                               PlayerOf(remote), location.x, location.y, location.z,
                               mem::AnimName(mem::CurrentAnimation(remote.chr)), mem::CurrentFrame(remote.chr),
                               remote.timeline.count);
        if (written < 0) break;
        length += written < capacity - length ? written : capacity - length - 1;
    }
    if (!length) snprintf(out, static_cast<size_t>(capacity), "absents");
}

}  // namespace rep
