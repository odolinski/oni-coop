#include "defense.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "clock.h"
#include "config.h"
#include "log.h"
#include "memory.h"
#include "network.h"
#include "protocol.h"
#include "replication.h"
#include "revive.h"
#include "roster.h"
#include "rules.h"
#include "spawns.h"

using namespace oni;

namespace defense {

static const double kScanInterval = 0.2;      // lasers et tourelles relevés cinq fois par seconde
static const double kAlignInterval = 1.0;     // client : état de l'hôte réappliqué s'il s'en est écarté
static const float kAlertRadius = 220.0f;     // ennemis prévenus autour du joueur qu'un laser détecte
static const double kTripNote = 3.0;          // un même laser n'est pas annoncé plus souvent

static bool Host() { return net::CurrentMode() == cfg::Mode::Host; }
static bool Client() { return net::CurrentMode() == cfg::Mode::Client; }
// Les défenses sont communes : seulement quand les PNJ sont partagés, et qu'il y a quelqu'un avec
// qui les partager.
static bool Shared() { return cfg::Get().npcs && net::Connected(); }

static mem::Defense g_known[proto::kMaxDefenses];   // hôte : dernier relevé ; client : ce que l'hôte a dit
static int g_knownCount = 0;
static bool g_knownValid = false;
static uint32_t g_knownLevel = 0;
static double g_nextScan = 0.0, g_nextAlign = 0.0;
static bool g_unsent = false;                       // hôte : le relevé a changé depuis le dernier envoi
static double g_trippedAt[proto::kMaxDefenses] = {};
static unsigned g_trips = 0, g_tripsHeard = 0, g_sent = 0, g_aligned = 0, g_alarms = 0;
// Ce que les tourelles lisent à la place du drapeau « toutes les IA passives » du moteur : non nul,
// elles ne tirent pas. Partout ailleurs que chez un client c'est ce drapeau lui-même. Un client le
// tient posé en permanence pour ses copies des PNJ : ses tourelles suivent donc celui de l'hôte,
// qui ne l'est que le temps d'une cinématique. Sans cela elles s'allumaient, visaient le joueur,
// et ne tiraient jamais : « côté client il ne se passe absolument rien » (signalé en 0.36 et 0.37).
static uint8_t g_fireHeld = 0;
static bool g_fireHooked = false;
static bool g_hostHeld = false;          // client : drapeau de l'hôte
static bool g_heldSaid = false;          // hôte : dernier état dit aux clients

// -------------------------------------------------------------------------------------------------
// Lasers du niveau
// -------------------------------------------------------------------------------------------------
// Dans l'ordre du niveau : le rang d'un laser est le même sur toutes les machines.
static void* g_lasers[proto::kMaxDefenses];
static int g_laserCount = 0;
static uint32_t g_lasersLevel = 0;

static void ListLasers() {
    g_lasersLevel = mem::LevelHash();
    g_laserCount = mem::Lasers(g_lasers, proto::kMaxDefenses);
}

static int LaserIndex(void* laser) {
    for (int pass = 0; pass < 2; pass++) {
        if (g_lasersLevel == mem::LevelHash()) {
            for (int i = 0; i < g_laserCount; i++) {
                if (g_lasers[i] == laser) return i;
            }
        }
        if (pass) break;
        ListLasers();  // niveau chargé, ou rechargé sous le même nom
    }
    return -1;
}

// Numéro du joueur que représente ce personnage ici : le nôtre, ou le pantin d'un autre.
static int PlayerOf(Character* chr) {
    if (chr && chr == mem::Player()) return net::LocalPlayerId();
    for (uint32_t id = 0; id < static_cast<uint32_t>(proto::kMaxPlayers); id++) {
        if (chr && rep::RemotePlayerCharacter(id) == chr) return static_cast<int>(id);
    }
    return -1;
}

static float Distance(const Vector3& a, const Vector3& b) {
    float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Le joueur détecté est signalé aux ennemis des environs, là où il se trouve : ils vont à lui par
// leurs propres chemins, comme après une alarme. Rend combien ont été prévenus.
static int AlertEnemies(Character* who) {
    Character* player = mem::Player();
    if (!player || !who) return 0;
    Vector3 at = mem::Location(who);
    int alerted = 0, count = mem::CharacterCount();
    for (int i = 0; i < count; i++) {
        Character* chr = mem::CharacterAt(i);
        if (!chr || !mem::InUse(chr) || mem::IsDead(chr) || !mem::IsAI(chr) || rep::IsPlayerCharacter(chr)) continue;
        if (field<uint32_t>(chr, off::kChAiFlags) & (off::kAiFlagPassive | off::kAiFlagNonCombatant)) continue;
        if (fn::FriendOrFoe(mem::Team(chr), mem::Team(player)) != 2) continue;
        if (Distance(mem::Location(chr), at) > kAlertRadius) continue;
        fn::AiMakeAware(chr, who, 0, 0);
        alerted++;
    }
    return alerted;
}

// -------------------------------------------------------------------------------------------------
// Tourelles : qui elles visent
// -------------------------------------------------------------------------------------------------
// Le moteur fait viser à une tourelle le plus proche des personnages vivants des équipes qu'elle
// vise, sur chaque machine pour son compte. Or un autre joueur n'y est qu'un pantin, que rien ne
// blesse. Vu dans la mémoire d'une partie de la 0.36, salle du Deadly Brain de l'usine : chez
// l'hôte, à terre, la tourelle visait le pantin du client ; chez le client, elle visait le pantin
// de l'hôte, plus proche d'elle que lui. Deux cibles, aucun dégât. C'est donc l'hôte qui choisit,
// parmi les joueurs debout, et qui dit son choix : chaque machine fait viser à sa tourelle le même
// joueur, et sur la machine de ce joueur c'est le vrai qui est visé, donc touché.
static void* g_turrets[proto::kMaxTurrets];
static int g_turretCount = 0;
static uint32_t g_turretsLevel = 0;
static uint8_t g_targets[proto::kMaxTurrets] = {};  // hôte : ses choix ; client : ceux que l'hôte a dits
static int g_targetCount = 0;                       // client : tourelles dont l'hôte a parlé
static bool g_targetsValid = false;
static uint32_t g_targetsLevel = 0;
static bool g_targetsUnsent = false;
static unsigned g_targetSends = 0, g_forced = 0, g_guessed = 0;

static void ListTurrets() {
    g_turretsLevel = mem::LevelHash();
    g_turretCount = mem::Turrets(g_turrets, proto::kMaxTurrets);
}

// Rang de la tourelle dans le niveau, le même sur toutes les machines ; -1 si elle n'y est pas.
static int TurretIndex(void* turret) {
    for (int pass = 0; pass < 2; pass++) {
        if (g_turretsLevel == mem::LevelHash()) {
            for (int i = 0; i < g_turretCount; i++) {
                if (g_turrets[i] == turret) return i;
            }
        }
        if (pass) break;
        ListTurrets();  // niveau chargé, ou rechargé sous le même nom : ses objets ont changé de place en mémoire
    }
    return -1;
}

// Joueur à terre ou mort : une tourelle ne s'acharne pas sur lui, et son corps ne déclenche rien.
static bool OutOfFight(Character* chr) {
    if (chr == mem::Player()) return revive::Downed() || mem::IsDead(chr);
    Vector3 where;
    int player = PlayerOf(chr);
    return player >= 0 && rep::DownedPlayerAt(static_cast<uint32_t>(player), where);
}

// Qui vient de couper un laser de chaque tourelle, et quand. Sert trois fois. Chez l'hôte, la
// tourelle prend d'abord ceux-là : le piège s'en prend à qui l'a déclenché. Chez un client, si son
// propre joueur en est, la sienne le prend sans attendre le choix de l'hôte. Et partout, une
// tourelle qu'un laser vient d'allumer ici n'est pas éteinte aussitôt au nom d'un état de l'hôte
// parti avant qu'il ne l'apprenne.
static const double kNever = -1.0e9;
struct Tripped {
    bool used = false;
    uint16_t turret = 0;                // numéro de la tourelle que le laser allume
    double at[proto::kMaxPlayers];      // dernier passage de chaque joueur
    Tripped() {
        for (double& when : at) when = kNever;
    }
    double latest() const {
        double last = kNever;
        for (double when : at) last = when > last ? when : last;
        return last;
    }
};
static Tripped g_tripped[16];
static const double kTripperHold = 5.0;   // hôte : la tourelle le prend d'abord pendant ce temps
static const double kGuessWindow = 1.5;   // client : la sienne le prend d'elle-même pendant ce temps
static const double kAlignGrace = 1.5;    // client : et n'est pas éteinte au nom de l'hôte pendant ce temps
// La cible du moment est gardée tant qu'une autre n'est pas plus près d'elle dans ce rapport : une
// tourelle n'hésite pas entre deux joueurs à même distance.
static const float kKeepTarget = 1.5f;

static Tripped* TrippedSlot(uint16_t turretId) {
    for (Tripped& tripped : g_tripped) {
        if (tripped.used && tripped.turret == turretId) return &tripped;
    }
    return nullptr;
}

// Ce joueur a-t-il coupé un laser de cette tourelle depuis moins de window secondes ?
static bool TrippedBy(uint16_t turretId, int player, double now, double window) {
    const Tripped* slot = TrippedSlot(turretId);
    return slot && player >= 0 && player < proto::kMaxPlayers && now - slot->at[player] < window;
}

// Quelqu'un l'a-t-il fait ?
static bool TrippedLately(uint16_t turretId, double now, double window) {
    const Tripped* slot = TrippedSlot(turretId);
    return slot && now - slot->latest() < window;
}

static void NoteTripper(uint16_t turretId, int player, double now) {
    if (player < 0 || player >= proto::kMaxPlayers) return;
    Tripped* slot = TrippedSlot(turretId);
    if (!slot) {
        slot = &g_tripped[0];
        for (Tripped& tripped : g_tripped) {
            if (!tripped.used) {
                slot = &tripped;
                break;
            }
            if (tripped.latest() < slot->latest()) slot = &tripped;  // la plus ancienne
        }
        *slot = Tripped();
        slot->used = true;
        slot->turret = turretId;
    }
    slot->at[player] = now;
}

// Les tourelles que ce laser allume retiennent quel joueur vient de le couper.
static void NoteTrip(void* laser, int player, double now) {
    uint16_t turrets[8];
    int count = mem::LaserTurrets(laser, turrets, 8);
    for (int i = 0; i < count; i++) NoteTripper(turrets[i], player, now);
}

static const char kByHost[] = ", d'apres l'hote";
static const char kByGuess[] = ", en attendant le choix de l'hote";
// Dans l'ordre de rules::TurretChoice.
static const char* const kByChoice[] = {
    "",
    ", qui vient de couper son laser",
    ", le plus proche de ceux qu'elle peut toucher",
    ", le plus proche : elle ne peut toucher personne",
};

// Le choix de l'hôte, parmi les personnages vivants des équipes visées, joueurs à terre exceptés.
// Le moteur prenait le plus proche, qu'il puisse le toucher ou non : à plusieurs, une tourelle
// suivait ainsi un joueur resté derrière un mur sans jamais tirer, et ignorait celui qui venait de
// couper son laser sous son nez (vu dans les journaux de la 0.38, usine, tourelle 1 : quinze
// secondes à viser l'hôte sans un tir). L'ordre du choix est dans rules::ChooseTurretTarget ; ici,
// qui la tourelle peut viser, à quelle distance, si elle pourrait lui tirer dessus d'après le test
// du moteur, et s'il vient de couper un de ses lasers.
static Character* HostTarget(void* turret, rules::TurretChoice* why) {
    static Character* list[off::kMaxCharacters];
    static Character* players[off::kMaxCharacters];
    static rules::TurretCandidate candidates[off::kMaxCharacters];
    int count = mem::LivingCharacters(list, off::kMaxCharacters), kept = 0, current = -1;
    uint32_t teams = field<uint32_t>(turret, off::kTurretTeams);
    uint16_t id = field<uint16_t>(turret, off::kTurretId);
    Vector3 at = field<Vector3>(turret, off::kObjPosition);
    Character* target = field<Character*>(turret, off::kTurretTarget);
    double now = clk::Seconds();
    for (int i = 0; i < count; i++) {
        Character* chr = list[i];
        if (!mem::InUse(chr) || !((teams >> (mem::Team(chr) & 31)) & 1) || OutOfFight(chr)) continue;
        float distance = Distance(field<Vector3>(chr, off::kChPosition), at);
        if (!std::isfinite(distance)) continue;
        if (chr == target) current = kept;
        players[kept] = chr;
        candidates[kept++] = {distance, mem::TurretCanShoot(turret, chr), TrippedBy(id, PlayerOf(chr), now, kTripperHold)};
    }
    int chosen = rules::ChooseTurretTarget(candidates, kept, current, kKeepTarget, why);
    return chosen >= 0 ? players[chosen] : nullptr;
}

// Une ligne au journal quand la cible d'une tourelle change ; quand seule la raison change, une par
// seconde au plus.
static uint8_t g_targetsSaid[proto::kMaxTurrets] = {};
static const char* g_targetsSaidBy[proto::kMaxTurrets] = {};
static double g_targetsSaidAt[proto::kMaxTurrets] = {};

static void NoteTarget(int index, void* turret, uint8_t code, const char* by) {
    double now = clk::Seconds();
    if (g_targetsSaid[index] == code && (g_targetsSaidBy[index] == by || now - g_targetsSaidAt[index] < 1.0)) return;
    g_targetsSaid[index] = code;
    g_targetsSaidBy[index] = by;
    g_targetsSaidAt[index] = now;
    unsigned id = field<uint16_t>(turret, off::kTurretId);
    if (code == proto::kTurretNoTarget) LOG("defenses : tourelle %u (rang %d) sans cible%s", id, index, by);
    else if (code == proto::kTurretOther) LOG("defenses : tourelle %u (rang %d) vise un personnage du niveau%s", id, index, by);
    else LOG("defenses : tourelle %u (rang %d) vise le joueur %u%s%s", id, index, code - proto::kTurretPlayer, by,
             code - proto::kTurretPlayer == net::LocalPlayerId() ? " : c'est le notre" : "");
}

// Appelé par le moteur chaque fois qu'une tourelle allumée cherche sa cible.
static Character* ChooseTarget(void* turret, Character* engineChoice) {
    if (!Shared()) return engineChoice;
    int index = TurretIndex(turret);
    if (index < 0) return engineChoice;
    if (Host()) {
        rules::TurretChoice choice = rules::kTurretNobody;
        Character* target = HostTarget(turret, &choice);
        int player = PlayerOf(target);
        uint8_t code = !target ? static_cast<uint8_t>(proto::kTurretNoTarget)
                               : player >= 0 ? static_cast<uint8_t>(proto::kTurretPlayer + player)
                                             : static_cast<uint8_t>(proto::kTurretOther);
        if (g_targets[index] != code) {
            g_targets[index] = code;
            g_targetsUnsent = true;
        }
        NoteTarget(index, turret, code, kByChoice[choice]);
        return target;
    }
    bool known = g_targetsValid && g_targetsLevel == mem::LevelHash() && index < g_targetCount;
    uint8_t code = known ? g_targets[index] : static_cast<uint8_t>(proto::kTurretNoTarget);
    if (code == proto::kTurretNoTarget) {
        // L'hôte n'a encore rien dit de cette tourelle, que le laser coupé par notre joueur vient
        // d'allumer ici : c'est lui qu'elle prendra chez l'hôte, elle le prend sans attendre. Une
        // tourelle ne reste allumée que deux ou trois secondes pour certaines.
        Character* player = mem::Player();
        bool ours = TrippedBy(field<uint16_t>(turret, off::kTurretId), net::LocalPlayerId(), clk::Seconds(), kGuessWindow);
        if (ours && player && !OutOfFight(player)) {
            NoteTarget(index, turret, static_cast<uint8_t>(proto::kTurretPlayer + net::LocalPlayerId()), kByGuess);
            g_guessed++;
            return player;
        }
        if (!known) return engineChoice;
    }
    if (code == proto::kTurretOther) return engineChoice;
    NoteTarget(index, turret, code, kByHost);
    if (code == proto::kTurretNoTarget || code > proto::kMaxPlayers) return nullptr;
    uint8_t player = static_cast<uint8_t>(code - proto::kTurretPlayer);
    Character* chr = player == net::LocalPlayerId() ? mem::Player() : rep::RemotePlayerCharacter(player);
    g_forced++;
    return chr && mem::InUse(chr) ? chr : nullptr;
}

bool InstallTurretHook() { return mem::InstallTurretTargetHook(&ChooseTarget); }

bool InstallFireHook() {
    g_fireHooked = mem::InstallTurretFireFlag(&g_fireHeld);
    return g_fireHooked;
}

// Pour le journal : sur cette machine, quelle tourelle se met à tirer et sur qui, et notre joueur
// perd-il de la vie pendant qu'une tourelle lui tire dessus. C'est ce qui dit, après une partie, si
// les tirs ont bien eu lieu chez le joueur visé, et s'ils l'ont touché.
static bool g_firing[proto::kMaxTurrets] = {};
static double g_firingSaidAt[proto::kMaxTurrets] = {};
static double g_nextFiringCheck = 0.0, g_hurtSaidAt = 0.0;
static int32_t g_healthSeen = 0;
static unsigned g_bursts = 0, g_hurts = 0;

static void NoteFiring(double now) {
    static void* list[proto::kMaxTurrets];
    if (now < g_nextFiringCheck) return;
    g_nextFiringCheck = now + kScanInterval;
    Character* player = mem::Player();
    int count = mem::Turrets(list, proto::kMaxTurrets);
    bool deciding = Host() && Shared() && g_turretsLevel == mem::LevelHash();
    bool atUs = false;
    for (int i = 0; i < count; i++) {
        void* turret = list[i];
        bool on = field<uint16_t>(turret, off::kTurretState) != 0;
        bool firing = on && (field<uint16_t>(turret, off::kTurretFlags) & off::kTurretFlagFiring) != 0;
        Character* target = field<Character*>(turret, off::kTurretTarget);
        if (firing && target && target == player) atUs = true;
        if (firing && !g_firing[i]) {
            g_bursts++;
            if (now - g_firingSaidAt[i] >= 2.0) {
                g_firingSaidAt[i] = now;
                LOG("defenses : tourelle %u (rang %d) tire ici, sur %s", field<uint16_t>(turret, off::kTurretId), i,
                    !target ? "personne" : target == player ? "notre joueur" : mem::Name(target));
            }
        }
        g_firing[i] = firing;
        // Hôte : une tourelle éteinte n'a plus de cible, et le dit. À son prochain réveil celle
        // d'un client n'ira pas d'abord à sa cible d'avant, et le journal redira qui elle vise.
        if (deciding && !on && i < g_turretCount && g_turrets[i] == turret && g_targets[i] != proto::kTurretNoTarget) {
            g_targets[i] = proto::kTurretNoTarget;
            g_targetsSaid[i] = proto::kTurretNoTarget;
            g_targetsSaidBy[i] = kByChoice[0];
            g_targetsUnsent = true;
        }
    }
    int32_t health = player ? mem::Health(player) : 0;
    if (atUs && health < g_healthSeen && now - g_hurtSaidAt >= 1.0) {
        g_hurtSaidAt = now;
        g_hurts++;
        LOG("defenses : notre joueur perd de la vie sous le tir d'une tourelle (%d, il en avait %d)", static_cast<int>(health),
            static_cast<int>(g_healthSeen));
    }
    g_healthSeen = health;
}

static void SendTargets() {
    g_targetsUnsent = false;
    if (!Host() || !Shared()) return;
    if (g_turretsLevel != mem::LevelHash()) ListTurrets();  // aucune n'a encore cherché sa cible dans ce niveau
    if (g_turretCount <= 0) return;
    uint8_t packet[1 + 4 + 1 + proto::kMaxTurrets];
    uint32_t level = mem::LevelHash();
    packet[0] = proto::kReliableTurretTargets;
    memcpy(packet + 1, &level, 4);
    packet[5] = static_cast<uint8_t>(g_turretCount);
    memcpy(packet + 6, g_targets, static_cast<size_t>(g_turretCount));
    net::SendReliable(packet, 6 + g_turretCount);
    g_targetSends++;
}

void OnTargets(uint8_t from, const uint8_t* data, int size) {
    if (!Client() || from != proto::kHostPlayerId || size < 5) return;
    int count = data[4];
    if (count > proto::kMaxTurrets || size < 5 + count) return;
    memcpy(&g_targetsLevel, data, 4);
    memcpy(g_targets, data + 5, static_cast<size_t>(count));
    g_targetCount = count;
    g_targetsValid = true;
}

// -------------------------------------------------------------------------------------------------
// État des lasers et des tourelles, de l'hôte vers les clients
// -------------------------------------------------------------------------------------------------
static void Send() {
    g_unsent = false;
    if (!Host() || !Shared() || !g_knownValid) return;
    uint8_t packet[1 + 4 + 2 + proto::kMaxDefenses * sizeof(proto::DefenseState)];
    uint32_t level = mem::LevelHash();
    int count = g_knownCount > 255 ? 255 : g_knownCount;
    packet[0] = proto::kReliableDefenses;
    memcpy(packet + 1, &level, 4);
    packet[5] = g_heldSaid ? proto::kDefensesHeld : 0;
    packet[6] = static_cast<uint8_t>(count);
    for (int i = 0; i < count; i++) {
        proto::DefenseState state;
        state.id = g_known[i].id;
        state.bits = static_cast<uint8_t>((g_known[i].kind == mem::DefenseKind::Turret ? proto::kDefenseTurret : 0) |
                                          (g_known[i].active ? proto::kDefenseActive : 0) |
                                          (g_known[i].hit ? proto::kDefenseHit : 0));
        memcpy(packet + 7 + i * sizeof(state), &state, sizeof(state));
    }
    net::SendReliable(packet, static_cast<int>(7 + count * sizeof(proto::DefenseState)));
    g_sent++;
}

void Resend() {
    if (Host()) g_unsent = true;
}

// Hôte : relève ses défenses, allumées ou éteintes. Ce qui a changé part vers les clients. Le
// laser qui vient d'être coupé, lui, est annoncé à l'instant même par le moteur (OnLaserTripped) :
// un relevé cinq fois par seconde manquerait le passage d'un joueur qui court.
static void Scan() {
    static mem::Defense seen[proto::kMaxDefenses];
    int count = mem::ReadDefenses(seen, proto::kMaxDefenses);
    bool fresh = !g_knownValid || g_knownLevel != mem::LevelHash() || count != g_knownCount;
    for (int i = 0; i < count; i++) {
        const mem::Defense& was = g_known[i];
        const mem::Defense& is = seen[i];
        bool same = !fresh && was.kind == is.kind && was.id == is.id;
        if (!same || was.active != is.active || was.hit != is.hit) g_unsent = true;
        if (same && is.active != was.active) {
            LOG("defenses : %s %u %s", is.kind == mem::DefenseKind::Turret ? "tourelle" : "laser", is.id,
                is.active ? "allume" : "eteint");
        }
    }
    memcpy(g_known, seen, sizeof(mem::Defense) * static_cast<size_t>(count));
    g_knownCount = count;
    g_knownValid = true;
    g_knownLevel = mem::LevelHash();
    if (fresh) memset(g_trippedAt, 0, sizeof(g_trippedAt));
}

// -------------------------------------------------------------------------------------------------
// Lasers : qui les coupe
// -------------------------------------------------------------------------------------------------
// Chaque machine teste ses lasers contre son propre joueur, là où elle les affiche, comme le fait
// le moteur ; elle dit aux autres lequel il vient de couper, et chacune fait alors chez elle ce
// que ce laser déclenche. De la 0.36 à la 0.38 seul l'hôte détectait, en testant ses lasers contre
// le pantin de chaque client ; il le fait toujours, en plus. Or presque tous les lasers du jeu
// glissent le long d'un rail (288 sur 291), et le moteur ne règle leur position sur aucune
// horloge : un rayon avance d'un pas par tick tant que son laser est allumé, sur chaque machine
// pour son compte. Un client qui recharge le niveau seul, ou arrive en cours de partie, n'a donc
// pas ses rayons où l'hôte a les siens : avec la seule détection de l'hôte il traverse le rayon
// qu'il voit sans rien déclencher (déduit du moteur, pas vu en jeu). Sa propre machine le voit,
// elle ; et ses rayons sont recalés sur ceux de l'hôte (plus bas).
//
// Un laser à plusieurs rayons se redéclenche à chaque image tant qu'un joueur en coupe un sans les
// couper tous : le moteur lève son drapeau « coupé » au rayon suivant (0x4CBB19), et le repose à
// l'image d'après. C'est le bourdonnement continu d'un laser où l'on reste, et c'était 500 annonces
// en six minutes dans les journaux de la 0.38. Les autres machines ne sont donc prévenues que
// quatre fois par seconde et par laser, le journal toutes les deux secondes, les ennemis aussi.
static const double kTripResend = 0.25;
static const double kTripLog = 2.0;
static const double kTripEcho = 0.5;      // même passage vu ici et annoncé par une autre machine : un seul son

// L'hôte teste aussi ses lasers contre le pantin de chaque client, comme de la 0.36 à la 0.38 : vu
// en jeu, salle du Deadly Brain en 0.38 et couloir des lasers 2 à 4 de l'usine dans le premier jet
// de la 0.39. Les deux détections valent ; la première qui voit un joueur couper un laser
// l'annonce. Un client garde le test du moteur, contre son seul joueur.
static int Candidates(Character* player, Character** out, int capacity) {
    if (!Shared() || !Host()) return -1;
    int count = 0;
    if (player && count < capacity) out[count++] = player;
    for (uint32_t id = 0; id < static_cast<uint32_t>(proto::kMaxPlayers) && count < capacity; id++) {
        if (Character* other = rep::RemotePlayerCharacter(id)) out[count++] = other;
    }
    return count;
}

bool InstallLaserHook() { return mem::InstallLaserHook(&Candidates); }

// Le personnage qui vient de couper le laser dont le moteur lance les événements. Le moteur les
// lance toujours au nom du joueur de la partie ; chez l'hôte ce peut être le pantin d'un client.
static Character* Cutter(Character* cause) {
    int32_t hitAt = 0;
    Character* who = Host() && Shared() ? mem::LastLaserHit(&hitAt) : nullptr;
    return who && mem::GameTime() - hitAt <= 2 ? who : cause;
}
static double g_tripSentAt[proto::kMaxDefenses] = {};     // dernier envoi aux autres, par rang de laser
static double g_tripSaidAt[proto::kMaxDefenses] = {};     // dernière ligne au journal, pour notre joueur
static unsigned g_tripsUnsaid[proto::kMaxDefenses] = {};  // fois où il l'a coupé depuis cette ligne
static double g_heardSaidAt[proto::kMaxDefenses] = {};    // les mêmes, pour les joueurs des autres machines
static unsigned g_heardUnsaid[proto::kMaxDefenses] = {};

// Appelé par le moteur quand un joueur coupe un laser ici, avant qu'il n'en lance les événements.
// Un joueur à terre ne déclenche plus rien : son corps resterait dans le rayon, et les tourelles
// allumées sur ses coéquipiers.
static bool AllowTrip(void*, Character* cause) {
    Character* who = Cutter(cause);
    return !Shared() || PlayerOf(who) < 0 || !OutOfFight(who);
}

// Appelé par le moteur aussitôt après : un joueur a coupé ce laser ici, ses événements sont lancés
// ici. Les autres machines sont prévenues ; chez l'hôte, les ennemis des environs aussi.
static void OnLaserTripped(void* laser, Character* cause) {
    if (!net::Running()) return;
    int index = LaserIndex(laser);
    if (index < 0) return;
    unsigned id = field<uint16_t>(laser, off::kTrigId);
    double now = clk::Seconds();
    Character* who = Cutter(cause);
    // -1 : pendant une cinématique le moteur suit parfois un autre personnage que notre joueur
    // (chr_focus) ; ce qu'il coupe alors ne regarde que cette machine.
    int player = PlayerOf(who);
    bool sharing = player >= 0 && Shared();
    g_trips++;
    g_tripsUnsaid[index]++;
    if (sharing) {
        NoteTrip(laser, player, now);
        if (now - g_tripSentAt[index] >= kTripResend) {
            g_tripSentAt[index] = now;
            proto::LaserTrip trip = {mem::LevelHash(), static_cast<uint16_t>(index), static_cast<uint8_t>(player)};
            uint8_t packet[1 + sizeof(trip)];
            packet[0] = proto::kReliableLaserTrip;
            memcpy(packet + 1, &trip, sizeof(trip));
            net::SendReliable(packet, sizeof(packet));
        }
    }
    if (now - g_tripSaidAt[index] < kTripLog) return;
    g_tripSaidAt[index] = now;
    int alerted = sharing && Host() ? AlertEnemies(who) : 0;
    LOG("defenses : laser %u (rang %d) coupe ici par %s (joueur %d), %u fois depuis la ligne precedente : %s ; %d ennemi(s) "
        "des environs prevenu(s)", id, index, !who ? "personne" : who == mem::Player() ? "notre joueur" : mem::Name(who), player,
        g_tripsUnsaid[index], !sharing ? "rien de dit aux autres" : Host() ? "dit aux clients" : "dit a l'hote", alerted);
    g_tripsUnsaid[index] = 0;
    if (player >= 0 && now - g_trippedAt[index] >= kTripNote) {
        g_trippedAt[index] = now;
        mem::ConsolePrint("OniMP : laser %u : %s detecte", id, roster::Name(static_cast<uint8_t>(player)));
    }
}

bool InstallTripHook() { return mem::InstallLaserTripHook(&AllowTrip, &OnLaserTripped); }

// Une autre machine a vu un joueur couper ce laser : la sienne, pour son propre joueur, ou l'hôte
// pour n'importe lequel, le nôtre compris. Ce que le moteur a fait là-bas est fait ici : le son du
// laser et ses événements (tourelles allumées, alarme, script, portes, consoles), au nom de ce
// joueur. Chez l'hôte, dont l'état fait foi, un laser éteint reste sans suite, et les ennemis des
// environs sont prévenus.
void OnTrip(uint8_t from, const uint8_t* data, int size) {
    proto::LaserTrip trip;
    if (from >= proto::kMaxPlayers || from == net::LocalPlayerId() || size < static_cast<int>(sizeof(trip))) return;
    memcpy(&trip, data, sizeof(trip));
    Character* player = mem::Player();
    if (!net::Running() || !player || !cfg::Get().npcs || trip.levelHash != mem::LevelHash()) return;
    // Un client ne parle que de son joueur ; l'hôte dit lequel il a vu.
    uint8_t who = from == proto::kHostPlayerId ? trip.player : from;
    if (who >= proto::kMaxPlayers) return;
    if (g_lasersLevel != mem::LevelHash() || trip.index >= g_laserCount) ListLasers();
    if (trip.index >= g_laserCount) {
        LOG("defenses : laser de rang %u coupe par le joueur %u d'apres le joueur %u, inconnu ici (%d laser(s))", trip.index, who,
            from, g_laserCount);
        return;
    }
    void* laser = g_lasers[trip.index];
    unsigned id = field<uint16_t>(laser, off::kTrigId);
    double now = clk::Seconds();
    bool said = now - g_heardSaidAt[trip.index] < kTripLog;
    g_tripsHeard++;
    g_heardUnsaid[trip.index]++;
    if (Host() && !(field<uint16_t>(laser, off::kTrigFlags) & off::kTrigFlagActive)) {
        if (said) return;
        g_heardSaidAt[trip.index] = now;
        LOG("defenses : laser %u (rang %u) coupe chez le joueur %u, %u annonce(s) depuis la ligne precedente, mais il est "
            "eteint ici : sans suite", id, trip.index, who, g_heardUnsaid[trip.index]);
        g_heardUnsaid[trip.index] = 0;
        return;
    }
    bool us = who == net::LocalPlayerId();  // l'hôte a vu notre pantin le couper
    Character* chr = us ? player : rep::RemotePlayerCharacter(who);
    // Le même passage, vu ici il y a un instant : le moteur a déjà joué le son.
    bool sound = now - g_tripSentAt[trip.index] >= kTripEcho && mem::PlayLaserSound(laser);
    mem::RunLaserEvents(laser, chr ? chr : player);
    NoteTrip(laser, who, now);
    if (!said) {
        g_heardSaidAt[trip.index] = now;
        int alerted = Host() ? AlertEnemies(chr) : 0;
        LOG("defenses : laser %u (rang %u) coupe par le joueur %u (%s) d'apres %s, %u annonce(s) depuis la ligne precedente : "
            "%sses evenements lances ici ; %d ennemi(s) des environs prevenu(s)", id, trip.index, who,
            us ? "le notre" : chr ? mem::Name(chr) : "pas de pantin ici", from == proto::kHostPlayerId ? "l'hote" : "sa machine",
            g_heardUnsaid[trip.index], sound ? "son joue, " : "", alerted);
        g_heardUnsaid[trip.index] = 0;
    }
    if (now - g_trippedAt[trip.index] >= kTripNote) {
        g_trippedAt[trip.index] = now;
        mem::ConsolePrint("OniMP : laser %u : %s detecte", id, roster::Name(who));
    }
}

// -------------------------------------------------------------------------------------------------
// Rayons des lasers mobiles, de l'hôte vers les clients
// -------------------------------------------------------------------------------------------------
// Pour que chacun voie les rayons au même endroit : un joueur qui regarde l'autre passer le voit
// alors couper le laser là où il le coupe chez lui. L'hôte dit où en sont les siens ; un client
// ramène ces positions à l'instant où il les reçoit (elles ont un demi-aller-retour de retard),
// y recale ceux de ses rayons qui s'en sont écartés de plus que ce qu'ils parcourent en un
// dixième de seconde, et les laisse courir entre deux relevés.
static const double kRaysInterval = 2.0;
static double g_nextRays = 0.0;
static int g_raysCursor = 0;             // hôte : premier laser du prochain envoi, s'ils ne tiennent pas tous dans un
static double g_raysSaidAt = 0.0;
static unsigned g_raysSent = 0, g_raysFixed = 0;

static void SendRays() {
    if (g_lasersLevel != mem::LevelHash()) ListLasers();
    if (g_laserCount <= 0) return;
    uint8_t packet[proto::kMaxStreamPayload];
    uint32_t level = mem::LevelHash();
    packet[0] = proto::kReliableLaserRays;
    memcpy(packet + 1, &level, 4);
    int size = 5, seen = 0;
    for (; seen < g_laserCount; seen++) {
        int index = (g_raysCursor + seen) % g_laserCount;
        float frames[proto::kMaxLaserRays];
        int8_t directions[proto::kMaxLaserRays];
        int count = mem::LaserRays(g_lasers[index], frames, directions, proto::kMaxLaserRays);
        if (count <= 0) continue;  // laser fixe
        if (size + 2 + count * 5 > static_cast<int>(sizeof(packet))) break;
        packet[size++] = static_cast<uint8_t>(index);
        packet[size++] = static_cast<uint8_t>(count);
        for (int i = 0; i < count; i++) {
            memcpy(packet + size, &frames[i], 4);
            packet[size + 4] = static_cast<uint8_t>(directions[i]);
            size += 5;
        }
    }
    g_raysCursor = (g_raysCursor + seen) % g_laserCount;
    if (size <= 5) return;
    net::SendReliable(packet, size);
    g_raysSent++;
}

void OnRays(uint8_t from, const uint8_t* data, int size) {
    uint32_t level = 0;
    if (!Client() || from != proto::kHostPlayerId || size < 4) return;
    memcpy(&level, data, 4);
    if (!cfg::Get().npcs || !mem::Player() || level != mem::LevelHash()) return;
    if (g_lasersLevel != level) ListLasers();
    int ping = net::PingMs(proto::kHostPlayerId);
    int late = ping > 0 ? (ping * 60 + 1000) / 2000 : 0;  // ticks de jeu écoulés depuis le relevé de l'hôte
    if (late > 30) late = 30;
    int fixed = 0;
    float worst = 0.0f;
    for (int at = 4; at + 2 <= size;) {
        int index = data[at], count = data[at + 1];
        at += 2;
        if (count <= 0 || count > proto::kMaxLaserRays || at + count * 5 > size) break;
        float theirs[proto::kMaxLaserRays], ours[proto::kMaxLaserRays];
        int8_t theirDirections[proto::kMaxLaserRays], ourDirections[proto::kMaxLaserRays];
        for (int i = 0; i < count; i++, at += 5) {
            memcpy(&theirs[i], data + at, 4);
            theirDirections[i] = static_cast<int8_t>(data[at + 4]);
        }
        if (index >= g_laserCount) continue;
        void* laser = g_lasers[index];
        if (mem::LaserRays(laser, ours, ourDirections, proto::kMaxLaserRays) != count) continue;
        int rail = mem::LaserRailFrames(laser);
        float speed = mem::LaserSpeed(laser);
        bool loops = mem::LaserLoops(laser);
        // Un laser éteint n'avance pas : sa position chez l'hôte n'a pas bougé depuis le relevé.
        bool lit = (field<uint16_t>(laser, off::kTrigFlags) & off::kTrigFlagActive) != 0;
        float gap = 0.0f;
        for (int i = 0; i < count; i++) {
            int direction = theirDirections[i] < 0 ? -1 : 1;
            if (lit) rules::AdvanceRay(&theirs[i], &direction, speed, late, rail, loops);
            theirDirections[i] = static_cast<int8_t>(direction);
            float one = rules::RayGap(ours[i], theirs[i], rail, loops);
            if (!(one <= gap)) gap = one;  // une position illisible compte pour un écart
        }
        if (gap <= rules::RayTolerance(speed)) continue;
        mem::SetLaserRays(laser, theirs, theirDirections, count);
        fixed++;
        if (std::isfinite(gap) && gap > worst) worst = gap;
    }
    if (!fixed) return;
    g_raysFixed += static_cast<unsigned>(fixed);
    double now = clk::Seconds();
    if (now - g_raysSaidAt < 5.0) return;
    g_raysSaidAt = now;
    LOG("defenses : rayons de %d laser(s) recales sur ceux de l'hote, ecart jusqu'a %.0f image(s) de rail ; %u depuis le "
        "debut", fixed, worst, g_raysFixed);
}

// Client : aligne ses défenses sur ce que l'hôte a dit. Rend combien il a fallu en changer.
static int Align() {
    static mem::Defense local[proto::kMaxDefenses];
    if (!g_knownValid || g_knownLevel != mem::LevelHash()) return 0;
    double now = clk::Seconds();
    int count = mem::ReadDefenses(local, proto::kMaxDefenses), changed = 0;
    for (int i = 0; i < g_knownCount; i++) {
        const mem::Defense& wanted = g_known[i];
        // Une tourelle qu'un laser vient d'allumer ici : l'hôte l'allume aussi dès qu'il l'apprend,
        // et ce qu'il a dit avant ne vaut pas ordre de l'éteindre.
        if (!wanted.active && wanted.kind == mem::DefenseKind::Turret && TrippedLately(wanted.id, now, kAlignGrace)) continue;
        for (int j = 0; j < count; j++) {
            if (local[j].kind != wanted.kind || local[j].id != wanted.id || local[j].active == wanted.active) continue;
            mem::SetDefense(wanted.kind, wanted.id, wanted.active);
            LOG("defenses : %s %u %s d'apres l'hote", wanted.kind == mem::DefenseKind::Turret ? "tourelle" : "laser",
                wanted.id, wanted.active ? "allume" : "eteint");
            changed++;
        }
    }
    g_aligned += static_cast<unsigned>(changed);
    return changed;
}

void OnReliable(uint8_t from, const uint8_t* data, int size) {
    uint32_t level = 0;
    if (!Client() || from != proto::kHostPlayerId || size < 6) return;
    memcpy(&level, data, 4);
    bool held = (data[4] & proto::kDefensesHeld) != 0;
    int count = data[5];
    if (count > proto::kMaxDefenses || size < 6 + count * static_cast<int>(sizeof(proto::DefenseState))) return;

    for (int i = 0; i < count; i++) {
        proto::DefenseState state;
        memcpy(&state, data + 6 + i * sizeof(state), sizeof(state));
        g_known[i].kind = state.bits & proto::kDefenseTurret ? mem::DefenseKind::Turret : mem::DefenseKind::Laser;
        g_known[i].id = state.id;
        g_known[i].active = (state.bits & proto::kDefenseActive) != 0;
        g_known[i].hit = (state.bits & proto::kDefenseHit) != 0;
    }
    g_knownCount = count;
    g_knownLevel = level;
    g_knownValid = true;
    if (held != g_hostHeld) {
        g_hostHeld = held;
        LOG("defenses : chez l'hote les IA sont %s : nos tourelles %s", held ? "tenues passives par un script" : "libres",
            held ? "ne tirent pas" : "peuvent tirer");
    }
    if (cfg::Get().npcs && level == mem::LevelHash()) Align();
}

// Une alarme est donnée du milieu de la simulation du moteur (un script en cours, l'IA d'un ennemi
// devant sa console) : la vague qu'elle amène est créée à l'image suivante (voir Update), quand le
// moteur a fini son tour des personnages.
struct PendingAlarm {
    bool waiting = false;
    Character* cause = nullptr;
    uint16_t handle = 0;
    char why[48] = "";
};
static PendingAlarm g_alarm;

void OnConnection(uint8_t, bool connected) {
    if (Host()) {
        if (connected) {
            g_unsent = true;             // à tous : sans effet chez ceux qui sont déjà alignés
            g_targetsUnsent = true;
            g_nextRays = 0.0;            // celui qui arrive a ses rayons là où son niveau les a posés
        }
    } else if (!connected) {
        g_knownValid = false;            // sans hôte, nos défenses redeviennent les nôtres
        g_targetsValid = false;
        g_hostHeld = false;
    }
}

void OnLevelChanged() {
    g_alarm = PendingAlarm();
    g_lasersLevel = 0;
    g_laserCount = 0;
    memset(g_tripSentAt, 0, sizeof(g_tripSentAt));
    memset(g_tripSaidAt, 0, sizeof(g_tripSaidAt));
    memset(g_tripsUnsaid, 0, sizeof(g_tripsUnsaid));
    memset(g_heardSaidAt, 0, sizeof(g_heardSaidAt));
    memset(g_heardUnsaid, 0, sizeof(g_heardUnsaid));
    for (Tripped& tripped : g_tripped) tripped = Tripped();
    g_raysCursor = 0;
    g_nextRays = 0.0;
    memset(g_firing, 0, sizeof(g_firing));
    memset(g_firingSaidAt, 0, sizeof(g_firingSaidAt));
    g_nextFiringCheck = 0.0;
    g_turretsLevel = 0;
    g_turretCount = 0;
    g_targetsValid = false;
    memset(g_targets, 0, sizeof(g_targets));
    memset(g_targetsSaid, 0, sizeof(g_targetsSaid));
    memset(g_targetsSaidBy, 0, sizeof(g_targetsSaidBy));
    memset(g_targetsSaidAt, 0, sizeof(g_targetsSaidAt));
    g_knownValid = false;
    g_knownCount = 0;
    g_nextScan = 0.0;
    g_nextAlign = 0.0;
    memset(g_trippedAt, 0, sizeof(g_trippedAt));
}

static void NoteAlarm(Character* cause, const char* why) {
    g_alarms++;
    g_alarm.waiting = true;
    g_alarm.cause = cause;
    g_alarm.handle = cause ? mem::Handle(cause) : 0;
    snprintf(g_alarm.why, sizeof(g_alarm.why), "%s", why);
}

void Update(double now) {
    // À chaque image, session ou non : ce que les tourelles lisent avant de tirer. Hors session, ou
    // chez l'hôte, le drapeau du moteur ; chez un client qui partage les PNJ de l'hôte, celui de l'hôte.
    bool follows = Client() && Shared() && g_knownValid;
    g_fireHeld = (follows ? g_hostHeld : mem::AiAllPassive()) ? 1 : 0;
    if (!net::Running() || !mem::LevelHash()) return;
    NoteFiring(now);
    if (g_alarm.waiting) {
        g_alarm.waiting = false;
        Character* cause = g_alarm.cause;
        if (cause && (!mem::InUse(cause) || mem::Handle(cause) != g_alarm.handle)) cause = nullptr;
        spawn::Wave(cause, g_alarm.why);
    }
    if (Client()) {
        if (Shared() && now >= g_nextAlign) {
            g_nextAlign = now + kAlignInterval;
            Align();
        }
        return;
    }
    // Un script de l'hôte pose ou lève « toutes les IA passives » : ses tourelles se taisent ou
    // reprennent, et celles des clients doivent en faire autant.
    bool held = mem::AiAllPassive();
    if (held != g_heldSaid) {
        g_heldSaid = held;
        g_unsent = true;
        LOG("defenses : IA %s par un script : les tourelles %s, dit aux clients", held ? "tenues passives" : "liberees",
            held ? "ne tirent plus" : "peuvent tirer");
    }
    if (g_targetsUnsent) SendTargets();  // sans attendre le relevé : une cible change en un quart de seconde
    if (Shared() && now >= g_nextRays) {
        g_nextRays = now + kRaysInterval;
        SendRays();
    }
    if (now < g_nextScan) return;
    g_nextScan = now + kScanInterval;
    Scan();
    if (g_unsent) Send();
}

// -------------------------------------------------------------------------------------------------
// Alarmes
// -------------------------------------------------------------------------------------------------
static void OnAlarm(uint8_t alarm, Character* cause) {
    char why[48];
    snprintf(why, sizeof(why), "ai2_tripalarm %u", alarm);
    NoteAlarm(cause, why);
}

bool InstallAlarmHook() { return mem::InstallAlarmHook(&OnAlarm); }

void OnConsoleAlarm(uint16_t console, Character* user) {
    char why[48];
    // Un allié qui se sert d'une console ne donne pas l'alarme contre nous.
    Character* player = mem::Player();
    if (!player || !user || !mem::IsAI(user) || fn::FriendOrFoe(mem::Team(user), mem::Team(player)) != 2) return;
    snprintf(why, sizeof(why), "console %u, par '%.20s'", console, mem::Name(user));
    NoteAlarm(user, why);
}

void Describe(char* out, int capacity) {
    int lasers = 0, turrets = 0, on = 0;
    for (int i = 0; g_knownValid && i < g_knownCount; i++) {
        if (g_known[i].kind == mem::DefenseKind::Turret) turrets++;
        else lasers++;
        if (g_known[i].active) on++;
    }
    snprintf(out, static_cast<size_t>(capacity),
             "defenses : %d laser(s), %d tourelle(s), %d allume(s) ; lasers coupes : %u ici, %u chez les autres ; rayons : "
             "%u envoi(s), %u recale(s) ; etat : %u envoi(s), %u alignement(s) ; %u alarme(s) ; %s",
             lasers, turrets, on, g_trips, g_tripsHeard, g_raysSent, g_raysFixed, g_sent, g_aligned, g_alarms,
             !cfg::Get().npcs ? "chacun les siennes (PNJ non partages)" : "chacun detecte son joueur, l'hote decide du reste");
}

void DescribeTurrets(char* out, int capacity) {
    snprintf(out, static_cast<size_t>(capacity),
             "tourelles : cibles : %u envoi(s), %u imposee(s), %u prise(s) sans attendre l'hote ; tir %s%s, %u salve(s) ici, "
             "notre joueur touche %u fois",
             g_targetSends, g_forced, g_guessed, g_fireHeld ? "retenu" : "libre",
             g_fireHooked ? "" : " (drapeau du moteur, non detourne)", g_bursts, g_hurts);
}

}  // namespace defense
