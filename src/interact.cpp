#include "interact.h"

#include <cstdio>
#include <cstring>

#include "config.h"
#include "log.h"
#include "memory.h"
#include "network.h"
#include "protocol.h"
#include "replication.h"

using namespace oni;

namespace interact {

static const double kRequestInterval = 0.4;  // d'une demande à la suivante, touche martelée
static const float kTalkRange = 60.0f;       // client : un PNJ à cette distance, sinon rien à demander
static const float kGiftRadius = 80.0f;      // arme donnée que le PNJ a posée à ses pieds, faute de main libre
static const double kGiftWindow = 3.0;       // après la fin d'une conversation, ce qui arrive encore au pantin en vient

typedef uint8_t(ONICALL* NeutralActionFunction)(Character* who, uint32_t activate);
static NeutralActionFunction g_neutralAction = nullptr;  // fonction d'origine, une fois l'appel redirigé

static double g_now = 0.0;
static double g_nextRequest = 0.0;       // client
static unsigned g_requested = 0;         // client : demandes faites à l'hôte
static unsigned g_received = 0;          // client : dons reçus
static unsigned g_started = 0;           // hôte : conversations engagées pour un client
static unsigned g_forwarded = 0;         // hôte : dons envoyés

// Hôte : ce qui est suivi pour le pantin de chaque client.
struct Watch {
    bool pending = false;          // sa touche d'action attend d'être jouée ici
    Character* talker = nullptr;   // PNJ en conversation avec lui
    uint16_t talkerHandle = 0;
    bool gave = false;             // ce PNJ a remis ce qu'il avait à donner
    double talkedUntil = 0.0;      // jusqu'à quand ce qui arrive au pantin est tenu pour un don
    void* hand = nullptr;          // arme qu'il avait en main à l'image précédente
    bool lsi = false;              // l'objet du niveau lui a déjà été compté
    bool strayNoted = false;       // réserves apparues hors conversation : dit une fois
};
static Watch g_watch[proto::kMaxPlayers];

static bool Host() { return net::CurrentMode() == cfg::Mode::Host && cfg::Get().npcs; }
// Client dont les PNJ sont des pantins de ceux de l'hôte : c'est chez lui que la conversation a lieu.
static bool Proxied() { return net::CurrentMode() == cfg::Mode::Client && cfg::Get().npcs && net::Connected(); }

static const char* PowerupName(uint32_t type) {
    static const char* const kNames[] = {"chargeur(s) de balles", "cellule(s) d'energie", "hypo(s)",
                                         "bouclier de force",     "invisibilite",         "objet du niveau"};
    return type < off::kPuTypeCount ? kNames[type] : "?";
}

// Nom de classe d'arme : il finit dans une ligne de script.
static bool ValidWeaponName(const char* name) {
    if (!name[0] || strcmp(name, "none") == 0) return false;
    for (const char* c = name; *c; c++) {
        bool valid = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_';
        if (!valid) return false;
    }
    return true;
}

static float DistanceSquared(const Vector3& a, const Vector3& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

// État de conversation d'un PNJ, ou nullptr s'il n'est pas en train de parler à quelqu'un.
static uint8_t* NeutralState(Character* npc) {
    if (!npc || !mem::InUse(npc) || !mem::IsAI(npc) || mem::IsDead(npc)) return nullptr;
    if (field<int32_t>(npc, off::kChAiGoal) != off::kAiGoalNeutral) return nullptr;
    uint8_t* state = field<uint8_t*>(npc, off::kChAiState);
    return state ? state + off::kNsBase : nullptr;
}

// PNJ en conversation avec ce personnage, ou nullptr.
static Character* Talker(Character* who) {
    int count = mem::CharacterCount();
    for (int i = 0; i < count; i++) {
        Character* npc = mem::CharacterAt(i);
        if (!npc || npc == who) continue;
        uint8_t* state = NeutralState(npc);
        if (state && field<Character*>(state, off::kNsTarget) == who) return npc;
    }
    return nullptr;
}

// -------------------------------------------------------------------------------------------------
// Client : la touche d'action part vers l'hôte
// -------------------------------------------------------------------------------------------------
static bool NpcNearby(Character* player) {
    Vector3 here = mem::Location(player);
    int count = mem::CharacterCount();
    for (int i = 0; i < count; i++) {
        Character* chr = mem::CharacterAt(i);
        if (!chr || chr == player || !mem::InUse(chr) || !mem::IsAI(chr) || mem::IsDead(chr)) continue;
        if (rep::IsPlayerCharacter(chr)) continue;
        if (DistanceSquared(mem::Location(chr), here) <= kTalkRange * kTalkRange) return true;
    }
    return false;
}

// Appelé par le moteur quand la touche d'action d'un personnage n'a trouvé ni porte ni console :
// reste le PNJ en face de lui. activate n'a de sens que dans son octet bas.
static uint8_t ONICALL ONIMP_ENTRY NeutralActionHook(Character* who, uint32_t activate) {
    if (!Proxied() || who != mem::Player()) return g_neutralAction(who, activate);
    // Nos PNJ sont des pantins : la conversation se tient chez l'hôte, avec notre pantin à nous.
    if (g_now >= g_nextRequest && NpcNearby(who)) {
        g_nextRequest = g_now + kRequestInterval;
        uint8_t packet[1 + sizeof(uint32_t)];
        uint32_t level = mem::LevelHash();
        packet[0] = proto::kReliableInteract;
        memcpy(packet + 1, &level, sizeof(level));
        if (net::SendReliable(packet, sizeof(packet))) g_requested++;
    }
    return 0;
}

bool Install() {
    if (mem::CallTarget(addr::kNeutralActionCallSite) != reinterpret_cast<void*>(addr::kNeutralActionTarget)) return false;
    void* previous = nullptr;
    if (!mem::PatchCall(addr::kNeutralActionCallSite, reinterpret_cast<void*>(&NeutralActionHook), &previous)) return false;
    g_neutralAction = reinterpret_cast<NeutralActionFunction>(previous);
    return true;
}

void OnGift(uint8_t from, const uint8_t* data, int size) {
    proto::Gift gift;
    if (net::CurrentMode() != cfg::Mode::Client || from != proto::kHostPlayerId) return;
    if (size < static_cast<int>(sizeof(gift))) return;
    memcpy(&gift, data, sizeof(gift));
    Character* player = mem::Player();
    if (gift.levelHash != mem::LevelHash() || !player || mem::IsDead(player)) return;
    g_received++;

    if (gift.kind == proto::kGiftWeapon) {
        char name[sizeof(gift.weapon) + 1] = {};
        memcpy(name, gift.weapon, sizeof(gift.weapon));
        const char* self = mem::Name(player);
        if (!ValidWeaponName(name) || !self[0]) return;
        // Comme toute arme reçue d'un script : notre joueur la prend en main, celle qu'il tenait
        // tombe à ses pieds. Les autres machines l'apprennent par les annonces habituelles.
        mem::Exec("chr_giveweapon %s %s", self, name);
        LOG("civil : arme '%s' recue d'un PNJ de l'hote", name);
        mem::ConsolePrint("OniMP : un civil vous donne une arme (%s)", name);
        return;
    }
    if (gift.kind != proto::kGiftPowerup || gift.type >= off::kPuTypeCount || !gift.amount) return;
    uint16_t taken = mem::GivePowerup(player, gift.type, gift.amount);
    LOG("civil : %u %s recu(s) d'un PNJ de l'hote, %u pris", static_cast<unsigned>(gift.amount), PowerupName(gift.type),
        static_cast<unsigned>(taken));
    const char* full = taken ? "" : " (vous n'avez plus de place)";
    if (gift.type <= 2) {
        mem::ConsolePrint("OniMP : un civil vous donne %u %s%s", static_cast<unsigned>(gift.amount),
                          PowerupName(gift.type), full);
    } else {
        mem::ConsolePrint("OniMP : un civil vous donne : %s%s", PowerupName(gift.type), full);
    }
}

// -------------------------------------------------------------------------------------------------
// Hôte : la conversation se tient ici, avec le pantin du client
// -------------------------------------------------------------------------------------------------
void OnInteract(uint8_t from, const uint8_t* data, int size) {
    uint32_t level;
    if (!Host() || from == proto::kHostPlayerId || from >= proto::kMaxPlayers || size < static_cast<int>(sizeof(level))) return;
    memcpy(&level, data, sizeof(level));
    // Jouée depuis notre boucle, pas depuis la lecture du réseau.
    if (level == mem::LevelHash()) g_watch[from].pending = true;
}

static bool SendGift(int player, const proto::Gift& gift) {
    uint8_t packet[1 + sizeof(gift)];
    packet[0] = proto::kReliableGift;
    memcpy(packet + 1, &gift, sizeof(gift));
    bool sent = player < 0 ? net::SendReliable(packet, sizeof(packet))
                           : net::SendReliableTo(static_cast<uint8_t>(player), packet, sizeof(packet));
    if (sent) g_forwarded++;
    return sent;
}

// everyone : à tous les clients, et non au seul joueur dont le pantin l'a reçu.
static void ForwardPowerup(uint8_t player, uint32_t type, uint16_t amount, bool everyone = false) {
    proto::Gift gift = {};
    gift.levelHash = mem::LevelHash();
    gift.kind = proto::kGiftPowerup;
    gift.type = static_cast<uint8_t>(type);
    gift.amount = amount;
    bool sent = SendGift(everyone ? -1 : player, gift);
    LOG("civil : %u %s donne(s) au pantin du joueur %u : %s", static_cast<unsigned>(amount), PowerupName(type), player,
        !sent ? "envoi refuse par le reseau" : everyone ? "envoye(s) a tous les clients" : "envoye(s) a son joueur");
}

void WatchGifts(uint8_t player, Character* puppet, uint16_t shieldSet, uint16_t cloakSet) {
    if (!Host() || player == proto::kHostPlayerId || player >= proto::kMaxPlayers || !puppet) return;
    Watch& watch = g_watch[player];
    bool talking = watch.talker != nullptr || g_now < watch.talkedUntil;

    // Les réserves d'un pantin sont vides : il tire sans chargeur. Ce qui s'y trouve vient d'y être mis.
    mem::Reserves reserves = mem::ReadReserves(puppet);
    if (reserves.clips || reserves.cells || reserves.hypos) {
        if (talking) {
            if (reserves.clips) ForwardPowerup(player, 0, reserves.clips);
            if (reserves.cells) ForwardPowerup(player, 1, reserves.cells);
            if (reserves.hypos) ForwardPowerup(player, 2, reserves.hypos);
        } else if (!watch.strayNoted) {
            watch.strayNoted = true;
            LOG("civil : reserves du pantin du joueur %u garnies hors conversation (%u, %u, %u) : videes sans les envoyer",
                player, reserves.clips, reserves.cells, reserves.hypos);
        }
        mem::ClearReserves(puppet);
    }
    // Bouclier et invisibilité : le moteur ne fait que les user. Plus qu'à l'image précédente : donnés.
    uint16_t shield = mem::Shield(puppet), cloak = mem::Cloak(puppet);
    if (talking && shield > shieldSet) ForwardPowerup(player, 3, shield);
    if (talking && cloak > cloakSet) ForwardPowerup(player, 4, cloak);

    // L'objet du niveau est celui de toute l'équipe (voir world.cpp) : à notre joueur aussi, et à
    // tous les clients.
    bool lsi = mem::HasLsi(puppet);
    if (lsi && !watch.lsi) {
        ForwardPowerup(player, off::kPuTypeLsi, 1, true);
        Character* own = mem::Player();
        if (own && !mem::HasLsi(own)) {
            mem::GiveLsi(own);
            mem::ConsolePrint("OniMP : le joueur %u a recu l'objet du niveau", player);
        }
    }
    watch.lsi = lsi;
}

// Le PNJ vient de remettre ce qu'il avait à donner. Bonus et réserves sont repris au pantin par
// WatchGifts ; reste l'arme, que le moteur a mise dans sa main si elle était libre, et posée aux
// pieds du PNJ sinon (0x4ADA0B). Elle est retirée d'ici et envoyée au vrai joueur, qui la prend en
// main sur sa machine : les autres l'apprennent alors par les annonces d'armes habituelles.
static void ForwardWeapon(uint8_t player, Character* puppet, Character* talker, const uint8_t* state, void* handBefore) {
    char name[33] = {};
    memcpy(name, state + off::kNsGiveWeapon, 32);
    if (!ValidWeaponName(name)) return;

    void* inHand = field<void*>(puppet, off::kChWeaponInHand);
    void* weapon = nullptr;
    if (!handBefore && inHand && strcmp(mem::WeaponClassName(inHand), name) == 0) {
        weapon = mem::DropWeapon(puppet);
    } else {
        weapon = mem::FindGroundWeapon(name, mem::Location(talker), kGiftRadius);
    }
    if (!weapon) {
        LOG("civil : arme '%s' donnee par '%s' au pantin du joueur %u : introuvable ici, rien d'envoye", name,
            mem::Name(talker), player);
        return;
    }
    proto::Gift gift = {};
    gift.levelHash = mem::LevelHash();
    gift.kind = proto::kGiftWeapon;
    gift.amount = mem::WeaponAmmo(weapon);
    size_t length = strlen(name);
    memcpy(gift.weapon, name, length < sizeof(gift.weapon) ? length : sizeof(gift.weapon));
    mem::DeleteWeapon(weapon);
    bool sent = SendGift(player, gift);
    LOG("civil : arme '%s' donnee par '%s' au pantin du joueur %u : %s", name, mem::Name(talker), player,
        sent ? "envoyee a son joueur" : "envoi refuse par le reseau");
}

static void UpdateHost(uint8_t player) {
    Watch& watch = g_watch[player];
    Character* puppet = rep::RemotePlayerCharacter(player);
    if (!puppet) {
        watch = Watch();
        return;
    }
    if (watch.pending) {
        watch.pending = false;
        if (!mem::IsDead(puppet)) {
            Character* before = Talker(puppet);
            if (mem::NeutralAction(puppet)) {
                Character* talker = Talker(puppet);
                if (talker && talker != before) {
                    g_started++;
                    LOG("civil : '%s' engage la conversation avec le joueur %u, a sa touche d'action", mem::Name(talker), player);
                }
            }
        }
    }

    Character* talker = Talker(puppet);
    uint16_t handle = talker ? mem::Handle(talker) : 0;
    if (talker != watch.talker || handle != watch.talkerHandle) {
        if (watch.talker && !talker) LOG("civil : conversation avec le joueur %u terminee", player);
        watch.talker = talker;
        watch.talkerHandle = handle;
        watch.gave = false;
    }
    if (talker) {
        watch.talkedUntil = g_now + kGiftWindow;
        const uint8_t* state = NeutralState(talker);
        bool gave = state && field<uint8_t>(const_cast<uint8_t*>(state), off::kNsGave) != 0;
        if (gave && !watch.gave) ForwardWeapon(player, puppet, talker, state, watch.hand);
        watch.gave = gave;
    }
    watch.hand = field<void*>(puppet, off::kChWeaponInHand);
}

void OnLevelChanged() {
    for (Watch& watch : g_watch) watch = Watch();
    g_nextRequest = 0.0;
}

void Update(double now) {
    g_now = now;
    if (!Host() || !mem::Player()) return;
    for (int player = 1; player < proto::kMaxPlayers; player++) UpdateHost(static_cast<uint8_t>(player));
}

void Describe(char* out, int capacity) {
    if (net::CurrentMode() == cfg::Mode::Client) {
        snprintf(out, static_cast<size_t>(capacity), "civils : %u demande(s) a l'hote, %u don(s) recu(s)%s", g_requested,
                 g_received, g_neutralAction ? "" : " (touche d'action non relayee)");
    } else {
        snprintf(out, static_cast<size_t>(capacity), "civils : %u conversation(s) engagee(s) pour un client, %u don(s) envoye(s)",
                 g_started, g_forwarded);
    }
}

}  // namespace interact
