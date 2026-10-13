#include "world.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "clock.h"
#include "config.h"
#include "defense.h"
#include "groundmatch.h"
#include "log.h"
#include "memory.h"
#include "network.h"
#include "oni.h"
#include "protocol.h"
#include "replication.h"

using namespace oni;

namespace world {

static const int kMaxObjects = 512;
static const double kWeaponResend = 2.0;   // l'arme en main est réannoncée : un pantin recréé la retrouve
static const double kWeaponRetry = 0.75;   // entre deux essais pour donner au pantin l'arme annoncée
static const int kMaxWeaponTries = 5;      // au-delà, on renonce jusqu'au prochain changement d'arme
// Un même objet au sol n'est pas exactement au même endroit chez les deux joueurs (il a roulé,
// le pantin qui l'a lâché était affiché avec retard) : on prend le plus proche dans ce rayon.
static const float kGroundMatchRadius = 60.0f;
static const float kPowerupReach = 25.0f;  // un bonus disparu plus près que cela de notre joueur : il l'a ramassé
static const float kPowerupSame = 10.0f;   // déplacement d'un bonus encore en chute, d'une frame à l'autre
static const float kOwnDropRadius = 30.0f; // arme que le pantin vient de laisser tomber à ses pieds
static const double kPuppetDelay = 0.1;    // retard d'affichage du pantin d'un autre joueur
static const double kGroundInterval = 0.25;  // l'hôte regarde à ce rythme si le sol a changé
static const double kGroundRefresh = 3.0;    // et en renvoie l'état même inchangé
static const double kPickupMemory = 2.0;     // l'hôte liste encore un moment le bonus que notre joueur vient de ramasser
static const float kPowerupSettle = 2.0f;    // écart au-delà duquel un bonus posé rejoint celui de l'hôte
// Client : pendant ce temps la liste de l'hôte ne touche pas à l'arme que notre joueur vient de
// lâcher. Elle peut dater d'avant notre annonce, et c'est notre machine qui sait ce qu'il a tiré.
static const double kOwnDropGrace = 1.5;

struct ObjectList {
    void* objects[kMaxObjects];
    int count;
};

// Dernier état connu de chaque porte et console. Un identifiant ne suffit pas à les désigner :
// plusieurs objets d'un niveau peuvent porter le même (les deux consoles d'alarme d'une salle ont
// toutes deux le 7 dans l'entrepôt). Suivies par leur seul identifiant, l'état de l'une écrasait
// celui de l'autre à chaque image, et la première passait pour « tout juste activée » sans fin :
// 99 000 annonces en dix minutes dans une partie de la 0.20. Chacune a donc pour clé son
// identifiant et son rang parmi ses homonymes, le même sur toutes les machines puisque le niveau y
// est le même.
static std::unordered_map<uint32_t, uint16_t> g_doorState;
static std::unordered_map<uint32_t, uint16_t> g_consoleFlags;
static std::unordered_map<uint32_t, double> g_announced;  // dernière annonce de chaque objet d'ici
static const double kAnnounceInterval = 0.5;              // jamais deux annonces du même objet plus près
static bool g_seeded = false;       // premier relevé du niveau fait : pas d'annonce pour l'état initial
// Dernière console activée ici, et par quel joueur.
static double g_consoleUsedAt = -1000.0;
static uint8_t g_consoleUser = 0;
static bool g_consoleHooked = false;  // les activations nous sont dites par le moteur lui-même
// Client : depuis quand chaque arme au sol d'ici manque au sol de l'hôte, 0 : elle y est.
static double g_extraSince[addr::kMaxWeapons];
// Le temps qu'une arme tout juste tombée ici soit dans sa liste : un peu moins que le délai entre
// deux listes inchangées (kGroundRefresh), pour qu'elle parte à la suivante.
static const double kExtraWeaponGrace = 2.5;

static std::string g_localWeapon;
static bool g_localWeaponKnown = false;
static double g_nextWeaponSend = 0.0;

// Ce que chaque autre joueur a annoncé de ses mains, par numéro de joueur.
struct RemoteHand {
    std::string weapon;          // arme annoncée
    bool known = false;
    void* appliedTo = nullptr;   // pantin auquel elle a été donnée
    double nextFix = 0.0;
    int tries = 0;
    double reloadAt = 0.0;       // 0 : aucun rechargement à jouer
    uint16_t dropped = 0;        // numéro de la dernière arme qu'il a lâchée, 0 : aucune
};
static RemoteHand g_hands[proto::kMaxPlayers];

// Relevé de la frame précédente : arme en main de notre joueur, armes et bonus au sol.
static bool g_groundTracked = false;
static void* g_localWeaponObject = nullptr;
static bool g_groundWeapon[addr::kMaxWeapons];
static Vector3 g_groundPosition[addr::kMaxWeapons];
// Numéro de chaque arme de la table d'ici, le même sur toutes les machines (voir groundmatch.h).
static ground::Ids g_weaponIds;
static double g_ownDrop[addr::kMaxWeapons];  // client : jusqu'à quand la liste de l'hôte n'y touche pas
static_assert(ground::kSlots == addr::kMaxWeapons, "table des armes");
struct Powerup {
    uint16_t type;
    uint16_t amount;
    Vector3 position;
};
static Powerup g_powerups[addr::kMaxPowerups];
static int g_powerupCount = 0;

// Hôte : dernier état du sol envoyé (bonus, puis armes), pour ne le renvoyer que s'il change.
static std::string g_sentGround[2];
static double g_nextGroundCheck = 0.0;
static double g_nextGroundRefresh = 0.0;
// Client : bonus que notre joueur vient de ramasser, à ne pas reposer d'après une liste en retard.
struct Pickup {
    uint16_t type;
    Vector3 position;
    double until;
};
static std::vector<Pickup> g_pickups;

static unsigned g_doorsSent = 0, g_doorsReceived = 0, g_consolesSent = 0, g_consolesReceived = 0;
static unsigned g_groundSent = 0, g_groundReceived = 0;

// Portes et consoles n'ont de sens partagé que si les scripts sont ceux de l'hôte.
static bool SharedWorld() { return cfg::Get().quests && net::Connected(); }

static uint8_t ONICALL ONIMP_ENTRY Collect(void* object, void* userData) {
    ObjectList* list = static_cast<ObjectList*>(userData);
    if (list->count < kMaxObjects) list->objects[list->count++] = object;
    return 1;  // continuer
}

static void List(uint32_t type, ObjectList& list) {
    list.count = 0;
    fn::EnumerateObjects(type, Collect, &list);
}

static void Send(uint8_t kind, const void* data, size_t size) {
    uint8_t message[64];
    uint32_t level = mem::LevelHash();
    if (size > sizeof(message) - 5) return;
    message[0] = kind;
    memcpy(message + 1, &level, 4);
    memcpy(message + 5, data, size);
    net::SendReliable(message, static_cast<int>(5 + size));
}

// -------------------------------------------------------------------------------------------------
// Portes et consoles
// -------------------------------------------------------------------------------------------------
static uint32_t ObjectKey(uint16_t id, uint8_t rank) { return id | (static_cast<uint32_t>(rank) << 16); }

// Rang de l'objet de cette case parmi ceux de la liste qui portent le même identifiant.
static uint8_t RankOf(const ObjectList& list, int index, size_t idOffset) {
    uint16_t id = field<uint16_t>(list.objects[index], idOffset);
    int rank = 0;
    for (int i = 0; i < index; i++) {
        if (field<uint16_t>(list.objects[i], idOffset) == id) rank++;
    }
    return static_cast<uint8_t>(rank > 255 ? 255 : rank);
}

// Annonce d'un objet du niveau : son identifiant, puis son rang. Une version d'avant la 0.21 ne lit
// que l'identifiant. Faux si le même objet vient déjà d'être annoncé : quoi qu'il arrive à l'état
// d'un objet, il ne part pas d'ici une annonce par image.
static bool Announce(uint8_t kind, uint16_t id, uint8_t rank, double now) {
    double& last = g_announced[(static_cast<uint32_t>(kind) << 24) | ObjectKey(id, rank)];
    if (last != 0.0 && now - last < kAnnounceInterval) return false;
    last = now;
    uint8_t data[3];
    memcpy(data, &id, 2);
    data[2] = rank;
    Send(kind, data, sizeof(data));
    return true;
}

static void UpdateDoors(bool announce, double now) {
    static ObjectList doors;
    List(Tag('D', 'O', 'O', 'R'), doors);
    for (int i = 0; i < doors.count; i++) {
        uint16_t id = field<uint16_t>(doors.objects[i], off::kDoorId);
        uint16_t state = field<uint16_t>(doors.objects[i], off::kDoorState);
        uint8_t rank = RankOf(doors, i, off::kDoorId);
        uint32_t key = ObjectKey(id, rank);
        auto known = g_doorState.find(key);
        bool opened = state == 1 && (known == g_doorState.end() || known->second != 1);
        g_doorState[key] = state;
        if (opened && announce && g_seeded && Announce(proto::kReliableDoor, id, rank, now)) g_doorsSent++;
    }
}

static void UpdateConsoles(bool announce, double now) {
    static ObjectList consoles;
    List(Tag('C', 'O', 'N', 'S'), consoles);
    for (int i = 0; i < consoles.count; i++) {
        uint16_t id = field<uint16_t>(consoles.objects[i], off::kConsoleId);
        uint16_t flags = field<uint16_t>(consoles.objects[i], off::kConsoleFlags);
        uint8_t rank = RankOf(consoles, i, off::kConsoleId);
        uint32_t key = ObjectKey(id, rank);
        auto known = g_consoleFlags.find(key);
        bool triggered = (flags & off::kConsoleTriggered) != 0 &&
                         (known == g_consoleFlags.end() || !(known->second & off::kConsoleTriggered));
        g_consoleFlags[key] = flags;
        // Sans le détour : relevé après coup, qui manque une console que son script remet à zéro
        // dans le même tick.
        if (triggered && g_seeded && !g_consoleHooked) {
            g_consoleUsedAt = now;  // par notre joueur : celles des autres sont notées à leur annonce
            g_consoleUser = net::LocalPlayerId();
        }
        if (triggered && announce && g_seeded && Announce(proto::kReliableConsole, id, rank, now)) {
            g_consolesSent++;
            LOG("console %u (rang %u) activee ici : annoncee", id, rank);
        }
    }
}

// L'objet de cet identifiant et de ce rang.
static void* Find(uint32_t type, size_t idOffset, uint16_t id, uint8_t rank) {
    static ObjectList list;
    List(type, list);
    int seen = 0;
    for (int i = 0; i < list.count; i++) {
        if (field<uint16_t>(list.objects[i], idOffset) != id) continue;
        if (seen++ == rank) return list.objects[i];
    }
    return nullptr;
}

static void OpenDoor(uint16_t id, uint8_t rank) {
    void* door = Find(Tag('D', 'O', 'O', 'R'), off::kDoorId, id, rank);
    if (!door) return;
    g_doorsReceived++;
    if (field<uint16_t>(door, off::kDoorState) != 1) fn::DoorOpen(door, nullptr);
    // Noté ouvert tout de suite : ce changement ne doit pas repartir d'ici comme le nôtre.
    g_doorState[ObjectKey(id, rank)] = 1;
}

// Appelé par le moteur à chaque activation d'une console par un personnage, avant que le script de
// la console ne tourne : la nôtre, quand notre joueur presse la touche d'action devant elle ; celle
// d'un autre joueur, quand ActivateConsole la rejoue ici au nom de son pantin. Une console de
// données se remet à zéro dans le tick où elle sert, pour resservir : le relevé des consoles, fait
// une fois par image, ne la voyait jamais activée chez l'hôte (journaux de la 0.25 : l'écran de
// l'hôte partait chez tous).
static void OnConsoleActivated(void* console, Character* user) {
    // Éteinte ou déjà servie, le moteur la refuse, et son script ne tourne pas.
    uint16_t flags = field<uint16_t>(console, off::kConsoleFlags);
    if (!(flags & off::kConsoleActive) || (flags & off::kConsoleTriggered)) return;
    int player = -1;
    if (user && user == mem::Player()) {
        player = net::LocalPlayerId();
    } else if (user) {
        for (uint32_t other = 0; other < static_cast<uint32_t>(proto::kMaxPlayers); other++) {
            if (rep::RemotePlayerCharacter(other) == user) player = static_cast<int>(other);
        }
    }
    if (player < 0) {
        // Un script, ou un PNJ : s'il s'en sert, c'est une alarme qu'il donne (ai2_doalarm).
        if (user) defense::OnConsoleAlarm(field<uint16_t>(console, off::kConsoleId), user);
        return;
    }
    g_consoleUsedAt = clk::Seconds();
    g_consoleUser = static_cast<uint8_t>(player);
}

bool InstallConsoleHook() {
    g_consoleHooked = mem::InstallConsoleHook(&OnConsoleActivated);
    return g_consoleHooked;
}

int TakeConsoleUser(double since) {
    if (g_consoleUsedAt < since) return -1;
    g_consoleUsedAt = -1000.0;
    return g_consoleUser;
}

static void ActivateConsole(uint16_t id, uint8_t rank, uint8_t from, double now) {
    void* console = Find(Tag('C', 'O', 'N', 'S'), off::kConsoleId, id, rank);
    if (!console) return;
    g_consolesReceived++;
    LOG("console %u (rang %u) activee par le joueur %u", id, rank, from);
    if (!(field<uint16_t>(console, off::kConsoleFlags) & off::kConsoleTriggered)) {
        // Noté avant de l'activer : son script peut demander son écran de texte dans la foulée.
        g_consoleUsedAt = now;
        g_consoleUser = from;
        // Activée au nom du personnage qui représente ce joueur : chez l'hôte, le script de la
        // console s'exécute comme s'il l'avait utilisée lui-même.
        Character* user = rep::RemotePlayerCharacter(from);
        if (!user) user = mem::Player();
        fn::ConsoleActivate(console, user);
        // Le détour vient de la noter au nom de ce personnage : sans pantin, c'était le nôtre.
        g_consoleUsedAt = now;
        g_consoleUser = from;
    }
    // Notée telle qu'elle est tout de suite : ce changement ne doit pas repartir d'ici.
    g_consoleFlags[ObjectKey(id, rank)] = field<uint16_t>(console, off::kConsoleFlags);
}

// -------------------------------------------------------------------------------------------------
// Arme en main
// -------------------------------------------------------------------------------------------------
static void SendLocalWeapon(double now) {
    Character* player = mem::Player();
    std::string weapon = mem::WeaponInHand(player);
    if (g_localWeaponKnown && weapon == g_localWeapon && now < g_nextWeaponSend) return;
    g_localWeapon = weapon;
    g_localWeaponKnown = true;
    g_nextWeaponSend = now + kWeaponResend;

    uint8_t data[40];
    size_t length = weapon.size() > 32 ? 32 : weapon.size();
    data[0] = static_cast<uint8_t>(length);
    memcpy(data + 1, weapon.data(), length);
    Send(proto::kReliableWeapon, data, 1 + length);
}

// L'arme que ce joueur a lâchée en dernier, si elle est encore connue ici.
static void* LastDropped(const RemoteHand& hand) {
    int slot = ground::SlotOf(g_weaponIds, hand.dropped);
    return slot < 0 ? nullptr : mem::WeaponAt(slot);
}

// L'arme en main annoncée par un autre joueur est un état : on y ramène son pantin tant qu'il
// tient autre chose. Les armes lâchées ou ramassées sont annoncées à part (WeaponEvent) et déjà
// traitées quand l'état arrive ; restent ici l'arme rangée, ressortie, ou reçue d'un script.
static void ApplyRemoteWeapon(uint32_t player, double now) {
    RemoteHand& hand = g_hands[player];
    Character* remote = rep::RemotePlayerCharacter(player);
    if (!remote || !hand.known) return;
    if (remote != hand.appliedTo) {
        hand.appliedTo = remote;
        hand.nextFix = 0.0;
        hand.tries = 0;
    }
    std::string held = mem::WeaponInHand(remote);
    if (hand.weapon == held) {
        hand.tries = 0;
        return;
    }
    if (now < hand.nextFix || hand.tries >= kMaxWeaponTries) return;
    hand.nextFix = now + kWeaponRetry;
    int attempt = hand.tries++;

    const char* name = mem::Name(remote);
    if (hand.weapon.empty()) {
        // Rangée. Sans place libre le moteur la ferait tomber au sol : on la retire alors au
        // pantin, qui ne doit pas garder l'arme au poing.
        if (attempt == 0 && !*mem::WeaponStored(remote)) mem::StoreWeapon(remote, true);
        else mem::Exec("chr_inv_reset %s", name);
    } else if (attempt == 0 && held.empty() && hand.weapon == mem::WeaponStored(remote)) {
        mem::StoreWeapon(remote, false);  // ressortie
    } else {
        LOG("arme du pantin %u : tient '%s', range '%s', attendu '%s'", static_cast<unsigned>(player), held.c_str(),
            mem::WeaponStored(remote), hand.weapon.c_str());
        // Si le moteur la lui a fait lâcher (animation de chute), elle est à ses pieds : il la
        // reprend, plutôt que d'en recevoir une seconde qui laisserait la première au sol. Jamais
        // celle que son joueur a lâchée pour de bon, qui peut se trouver à ses pieds elle aussi.
        void* dropped = held.empty() ? mem::FindGroundWeapon(hand.weapon.c_str(), mem::Location(remote), kOwnDropRadius,
                                                             LastDropped(hand))
                                     : nullptr;
        if (dropped) {
            mem::TakeWeapon(remote, dropped);
        } else {
            mem::Exec("chr_inv_reset %s", name);  // sinon l'arme précédente tombe au sol à chaque changement
            mem::Exec("chr_giveweapon %s %s", name, hand.weapon.c_str());
        }
    }
}

// -------------------------------------------------------------------------------------------------
// Rechargement
// -------------------------------------------------------------------------------------------------
// Le rechargement est une animation jouée par-dessus celle du corps. Depuis la 0.21 il voyage dans
// l'état de chaque personnage, joueur ou PNJ (proto::kEntityReloading, voir replication.cpp) :
// jusque-là seul son début était annoncé, ici, pour les joueurs, et rien ne disait s'il était joué.
// L'annonce d'une version plus ancienne est encore honorée.
static void ApplyRemoteReload(uint32_t player, double now) {
    RemoteHand& hand = g_hands[player];
    if (hand.reloadAt == 0.0 || now < hand.reloadAt) return;
    hand.reloadAt = 0.0;
    if (Character* remote = rep::RemotePlayerCharacter(player)) mem::PlayReload(remote);
}

// -------------------------------------------------------------------------------------------------
// Armes et bonus au sol
// -------------------------------------------------------------------------------------------------
static float DistanceSquared(const Vector3& a, const Vector3& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

static int ReadPowerups(Powerup* out) {
    int count = mem::PowerupCount();
    for (int i = 0; i < count; i++) mem::ReadPowerup(i, out[i].type, out[i].amount, out[i].position);
    return count;
}

static void SnapshotPowerups() { g_powerupCount = ReadPowerups(g_powerups); }

static void SnapshotGround() {
    g_localWeaponObject = field<void*>(mem::Player(), off::kChWeaponInHand);
    for (int i = 0; i < addr::kMaxWeapons; i++) {
        void* weapon = mem::WeaponAt(i);
        g_groundWeapon[i] = mem::WeaponOnGround(weapon);
        if (g_groundWeapon[i]) g_groundPosition[i] = mem::WeaponPosition(weapon);
    }
    SnapshotPowerups();
    g_groundTracked = true;
}

static void SendWeaponEvent(uint8_t kind, void* weapon, const Vector3& position) {
    const char* name = mem::WeaponClassName(weapon);
    int slot = mem::WeaponIndex(weapon);
    proto::WeaponEvent event = {};
    event.x = position.x;
    event.y = position.y;
    event.z = position.z;
    event.kind = kind;
    event.ammo = mem::WeaponAmmo(weapon);
    // Lâchée, elle reçoit un numéro si elle n'en avait pas : c'est sous ce numéro que les autres
    // machines la connaîtront. Ramassée, on donne celui qu'on lui connaît.
    if (kind == proto::kWeaponDropped) event.id = ground::Allocate(g_weaponIds, slot, net::LocalPlayerId());
    else if (slot >= 0) event.id = g_weaponIds.of[slot];
    strncpy(event.name, name, sizeof(event.name) - 1);
    Send(proto::kReliableWeaponEvent, &event, sizeof(event));
    g_groundSent++;
    if (kind == proto::kWeaponDropped) {
        LOG("arme '%s' #%u lachee avec %u cartouche(s) : annoncee", name, event.id, event.ammo);
    } else {
        LOG("arme '%s' #%u ramassee : annoncee", name, event.id);
    }
}

// Ce que notre joueur a fait des objets au sol depuis la frame précédente. Une arme rangée ou
// ressortie ne change pas de mains : seule l'annonce d'état en parle.
static void TrackGround(double time) {
    if (!g_groundTracked) {
        SnapshotGround();
        return;
    }
    Character* player = mem::Player();

    void* inHand = field<void*>(player, off::kChWeaponInHand);
    if (inHand != g_localWeaponObject) {
        void* old = g_localWeaponObject;
        if (old && mem::WeaponOnGround(old)) {
            SendWeaponEvent(proto::kWeaponDropped, old, mem::WeaponPosition(old));
            g_ownDrop[mem::WeaponIndex(old)] = time + kOwnDropGrace;
        }
        int index = mem::WeaponIndex(inHand);
        if (index >= 0 && g_groundWeapon[index]) {
            SendWeaponEvent(proto::kWeaponPicked, inHand, g_groundPosition[index]);
        }
    }

    // Un bonus qui était à portée de notre joueur et n'y est plus : il l'a ramassé.
    Vector3 here = mem::Location(player);
    static Powerup now[addr::kMaxPowerups];
    int count = ReadPowerups(now);
    for (int i = 0; i < g_powerupCount; i++) {
        const Powerup& before = g_powerups[i];
        if (DistanceSquared(before.position, here) > kPowerupReach * kPowerupReach) continue;
        bool still = false;
        for (int j = 0; j < count && !still; j++) {
            still = now[j].type == before.type &&
                    DistanceSquared(now[j].position, before.position) < kPowerupSame * kPowerupSame;
        }
        if (still) continue;
        proto::PowerupEvent event = {before.position.x, before.position.y, before.position.z, before.type};
        Send(proto::kReliablePowerup, &event, sizeof(event));
        g_groundSent++;
        if (net::CurrentMode() == cfg::Mode::Client) {
            g_pickups.push_back({before.type, before.position, time + kPickupMemory});
        }
        LOG("bonus de type %u ramasse : annonce", before.type);
    }
    SnapshotGround();
}

// -------------------------------------------------------------------------------------------------
// Sol : celui de l'hôte fait référence
// -------------------------------------------------------------------------------------------------
// Avec des PNJ partagés, ce qui traîne au sol doit être le même des deux côtés : un PNJ tué laisse
// ses chargeurs et ses hypos, et son arme avec les cartouches qui lui restent. Chez le client le
// PNJ est un pantin aux munitions infinies, dont la mort est rejouée : son arme tombait pleine, et
// rien ne garantissait les mêmes bonus. L'hôte envoie donc l'état de son sol, et le client s'y
// conforme : bonus posés, retirés ou déplacés, cartouches des armes corrigées. Chaque arme de la
// liste porte son numéro : le client sait de laquelle des siennes il s'agit.
static void Rounded(std::string& signature, const Vector3& position) {
    const float values[3] = {position.x, position.y, position.z};
    for (float value : values) {
        int32_t rounded = static_cast<int32_t>(value * 2.0f);  // un bonus posé peut frémir sur place
        signature.append(reinterpret_cast<const char*>(&rounded), sizeof(rounded));
    }
}

static void SendGroundList(int slot, uint8_t kind, const std::string& entries, int count,
                           const std::string& signature, bool refresh) {
    if (!refresh && signature == g_sentGround[slot]) return;
    uint32_t level = mem::LevelHash();
    std::string message(1, static_cast<char>(kind));
    message.append(reinterpret_cast<const char*>(&level), sizeof(level));
    message.push_back(static_cast<char>(count));
    message += entries;
    if (net::SendReliable(message.data(), static_cast<int>(message.size()))) g_sentGround[slot] = signature;
}

static void SendGround(double time) {
    if (time < g_nextGroundCheck) return;
    g_nextGroundCheck = time + kGroundInterval;
    bool refresh = time >= g_nextGroundRefresh;
    if (refresh) g_nextGroundRefresh = time + kGroundRefresh;

    std::string entries, signature;
    static Powerup powerups[addr::kMaxPowerups];
    int count = ReadPowerups(powerups);
    for (int i = 0; i < count; i++) {
        const Powerup& powerup = powerups[i];
        proto::GroundPowerup entry = {powerup.position.x, powerup.position.y, powerup.position.z,
                                      static_cast<uint8_t>(powerup.type), powerup.amount};
        entries.append(reinterpret_cast<const char*>(&entry), sizeof(entry));
        signature.append(reinterpret_cast<const char*>(&entry.type), sizeof(entry.type) + sizeof(entry.amount));
        Rounded(signature, powerup.position);
    }
    SendGroundList(0, proto::kReliableGroundPowerups, entries, count, signature, refresh);

    entries.clear();
    signature.clear();
    count = 0;
    for (int i = 0; i < addr::kMaxWeapons; i++) {
        void* weapon = mem::WeaponAt(i);
        if (!mem::WeaponOnGround(weapon)) continue;
        Vector3 position = mem::WeaponPosition(weapon);
        proto::GroundWeapon entry = {position.x, position.y, position.z, mem::Fnv1a(mem::WeaponClassName(weapon)),
                                     mem::WeaponAmmo(weapon), ground::Allocate(g_weaponIds, i, net::LocalPlayerId())};
        entries.append(reinterpret_cast<const char*>(&entry), sizeof(entry));
        signature.append(reinterpret_cast<const char*>(&entry.classHash),
                         sizeof(entry.classHash) + sizeof(entry.ammo) + sizeof(entry.id));
        Rounded(signature, position);
        count++;
    }
    SendGroundList(1, proto::kReliableGroundWeapons, entries, count, signature, refresh);
}

static bool JustPicked(uint16_t type, const Vector3& position, double time) {
    size_t kept = 0;
    bool found = false;
    for (const Pickup& pickup : g_pickups) {
        if (time >= pickup.until) continue;
        g_pickups[kept++] = pickup;
        found = found || (pickup.type == type &&
                          DistanceSquared(pickup.position, position) < kGroundMatchRadius * kGroundMatchRadius);
    }
    g_pickups.resize(kept);
    return found;
}

static void OnGroundPowerups(const uint8_t* data, int size, double time) {
    int hostCount = size >= 1 ? data[0] : 0;
    if (size < 1 || hostCount > addr::kMaxPowerups ||
        size < 1 + hostCount * static_cast<int>(sizeof(proto::GroundPowerup))) {
        return;
    }
    static proto::GroundPowerup host[addr::kMaxPowerups];
    memcpy(host, data + 1, static_cast<size_t>(hostCount) * sizeof(proto::GroundPowerup));

    // Chaque bonus de l'hôte prend ici le plus proche du même type. Un bonus posé (créé par nous,
    // il ne tombe pas) suit celui de l'hôte jusqu'à l'endroit où il s'arrête.
    static Powerup local[addr::kMaxPowerups];
    int localCount = ReadPowerups(local);
    bool used[addr::kMaxPowerups] = {};
    bool missing[addr::kMaxPowerups] = {};
    for (int h = 0; h < hostCount; h++) {
        Vector3 position = {host[h].x, host[h].y, host[h].z};
        int best = -1;
        float bestDistance = kGroundMatchRadius * kGroundMatchRadius;
        for (int j = 0; j < localCount; j++) {
            if (used[j] || local[j].type != host[h].type) continue;
            float distance = DistanceSquared(local[j].position, position);
            if (distance > bestDistance) continue;
            best = j;
            bestDistance = distance;
        }
        if (best < 0) {
            missing[h] = true;
            continue;
        }
        used[best] = true;
        if (bestDistance > kPowerupSettle * kPowerupSettle) mem::MovePowerup(best, position);
    }

    // Retirer avant de poser : dans une table pleine, le moteur fait de la place en supprimant le
    // plus ancien.
    int removed = 0, added = 0;
    for (int j = 0; j < localCount; j++) {
        if (!used[j] && mem::DeletePowerupNear(local[j].type, local[j].position, 1.0f)) removed++;
    }
    for (int h = 0; h < hostCount; h++) {
        Vector3 position = {host[h].x, host[h].y, host[h].z};
        if (!missing[h] || JustPicked(host[h].type, position, time)) continue;
        if (mem::NewPowerup(host[h].type, host[h].amount, position)) added++;
    }
    if (removed || added) {
        g_groundReceived++;
        LOG("sol : %d bonus pose(s), %d retire(s) d'apres l'hote", added, removed);
    }
    SnapshotPowerups();  // ces changements ne sont pas des ramassages de notre joueur
}

// Relevé de la table des armes d'ici, pour les rapprochements de groundmatch.h.
static void ReadWeapons(ground::Weapon* out) {
    Character* player = mem::Player();
    for (int i = 0; i < addr::kMaxWeapons; i++) {
        void* weapon = mem::WeaponAt(i);
        out[i] = ground::Weapon();
        if (!mem::WeaponInUse(weapon)) continue;
        Character* owner = mem::WeaponOwner(weapon);
        if (!owner) out[i].state = ground::kOnGround;
        else if (owner == player) out[i].state = ground::kOurs;
        else out[i].state = rep::IsPlayerCharacter(owner) ? ground::kPuppet : ground::kNpc;
        out[i].classHash = mem::Fnv1a(mem::WeaponClassName(weapon));
        Vector3 position = mem::WeaponPosition(weapon);
        out[i].x = position.x;
        out[i].y = position.y;
        out[i].z = position.z;
    }
}

// Les armes au sol ne sont ni créées ni retirées ici : celles des joueurs ont leurs annonces, et
// un PNJ lâche la sienne des deux côtés. Seules leurs cartouches suivent l'hôte. Chaque arme de sa
// liste est rapprochée de la même arme d'ici par son numéro, ou par l'endroit tant qu'elle n'en a
// pas ici (ground::Match).
static void OnGroundWeapons(const uint8_t* data, int size, double time) {
    int hostCount = size >= 1 ? data[0] : 0;
    if (size < 1 || hostCount > addr::kMaxWeapons ||
        size < 1 + hostCount * static_cast<int>(sizeof(proto::GroundWeapon))) {
        return;
    }
    static proto::GroundWeapon host[addr::kMaxWeapons];
    static ground::Weapon weapons[addr::kMaxWeapons];
    int slots[addr::kMaxWeapons];
    memcpy(host, data + 1, static_cast<size_t>(hostCount) * sizeof(proto::GroundWeapon));
    ReadWeapons(weapons);
    ground::Match(g_weaponIds, weapons, host, hostCount, kGroundMatchRadius, slots);

    // Une seule de chaque : le sol de l'hôte dit quelles armes existent. Une arme de sa liste qui
    // n'a pas été retrouvée ici à son endroit est la même qu'une arme d'ici de la même classe restée
    // sans pareille (tombée ailleurs : chaque machine fait tomber les siennes) : elles font
    // connaissance. Ce qui reste au sol d'ici sans pareille chez l'hôte est en trop, et retiré après
    // un délai : c'était la source des armes en double (celle d'un PNJ recréée ici alors que la
    // première était restée par terre, puis ramassée par le joueur et répandue chez l'hôte).
    bool matched[addr::kMaxWeapons] = {};
    for (int h = 0; h < hostCount; h++) {
        if (slots[h] >= 0) matched[slots[h]] = true;
    }
    for (int h = 0; h < hostCount; h++) {
        if (slots[h] >= 0) continue;
        for (int i = 0; i < addr::kMaxWeapons; i++) {
            if (weapons[i].state != ground::kOnGround || matched[i] || weapons[i].classHash != host[h].classHash) continue;
            ground::Bind(g_weaponIds, i, host[h].id);
            slots[h] = i;
            matched[i] = true;
            LOG("sol : arme '%s' #%u de l'hote reconnue ici par sa classe, ailleurs qu'a son endroit",
                mem::WeaponClassName(mem::WeaponAt(i)), host[h].id);
            break;
        }
    }
    for (int i = 0; i < addr::kMaxWeapons; i++) {
        if (weapons[i].state != ground::kOnGround || matched[i]) {
            g_extraSince[i] = 0.0;
        } else if (g_extraSince[i] == 0.0) {
            g_extraSince[i] = time;
        } else if (time - g_extraSince[i] > kExtraWeaponGrace && time >= g_ownDrop[i]) {
            void* extra = mem::WeaponAt(i);
            LOG("sol : arme '%s' absente du sol de l'hote depuis %.0f s : retiree d'ici, une seule de chaque",
                mem::WeaponClassName(extra), time - g_extraSince[i]);
            mem::DeleteWeapon(extra);
            g_weaponIds.of[i] = 0;
            g_extraSince[i] = 0.0;
        }
    }

    for (int h = 0; h < hostCount; h++) {
        void* weapon = slots[h] < 0 ? nullptr : mem::WeaponAt(slots[h]);
        uint16_t ammo = host[h].ammo, id = host[h].id;
        if (!weapon || mem::WeaponAmmo(weapon) == ammo) continue;
        const char* name = mem::WeaponClassName(weapon);
        if (time < g_ownDrop[slots[h]]) {
            LOG("sol : arme '%s' #%u tout juste lachee par notre joueur, ses %u cartouche(s) restent (l'hote en dit %u)",
                name, id, mem::WeaponAmmo(weapon), ammo);
            continue;
        }
        LOG("sol : arme '%s' #%u, %u cartouche(s) au lieu de %u d'apres l'hote", name, id, ammo, mem::WeaponAmmo(weapon));
        mem::SetWeaponAmmo(weapon, ammo);
    }
}

static void OnWeaponEvent(uint8_t from, const proto::WeaponEvent& event) {
    char name[sizeof(event.name)];
    memcpy(name, event.name, sizeof(name));
    name[sizeof(name) - 1] = '\0';
    // Le nom peut finir dans une ligne de script : on n'accepte que des noms de classe d'arme.
    for (const char* c = name; *c; c++) {
        bool valid = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_';
        if (!valid) return;
    }
    if (!name[0]) return;
    g_groundReceived++;

    // L'événement dit aussi ce que ce joueur a désormais en main, sans attendre l'annonce d'état
    // qui le suit : entre les deux, son pantin ne doit pas être ramené à l'ancienne arme.
    RemoteHand& hand = g_hands[from];
    Character* remote = rep::RemotePlayerCharacter(from);
    if (event.kind == proto::kWeaponDropped) {
        hand.weapon.clear();
        hand.known = true;
        hand.dropped = 0;
        if (!remote) return;
        // Le pantin la lâche à son tour : elle tombe à ses pieds, et notre joueur peut la prendre.
        if (strcmp(mem::WeaponInHand(remote), name) != 0) {
            mem::Exec("chr_inv_reset %s", mem::Name(remote));
            mem::Exec("chr_giveweapon %s %s", mem::Name(remote), name);
        }
        // Le pantin a des munitions infinies : sans cela l'arme tomberait pleine, quoi qu'ait
        // tiré l'autre joueur.
        void* dropped = mem::DropWeapon(remote);
        mem::SetWeaponAmmo(dropped, event.ammo);
        // Elle porte ici le numéro que ce joueur lui donne. S'il ramasse dans la foulée une arme
        // de la même classe (échange), ce n'est pas celle-ci que son pantin doit reprendre.
        if (dropped && event.id) {
            ground::Bind(g_weaponIds, mem::WeaponIndex(dropped), event.id);
            hand.dropped = event.id;
        }
        LOG("arme '%s' #%u lachee par le joueur %u, %u cartouche(s)", name, event.id, from, event.ammo);
    } else if (event.kind == proto::kWeaponPicked) {
        hand.weapon = name;
        hand.known = true;
        if (event.id && event.id == hand.dropped) hand.dropped = 0;  // il reprend celle qu'il avait lâchée
        // La même arme existe au sol ici : c'est elle que le pantin prend, pour qu'elle n'y reste
        // pas. Par son numéro, ou à l'endroit si elle n'en a pas encore ici.
        static ground::Weapon weapons[addr::kMaxWeapons];
        ReadWeapons(weapons);
        bool exact = false;
        int slot = ground::FindPicked(g_weaponIds, weapons, mem::Fnv1a(name), event.x, event.y, event.z,
                                      kGroundMatchRadius, event.id, hand.dropped, &exact);
        void* weapon = slot < 0 ? nullptr : mem::WeaponAt(slot);
        LOG("arme '%s' #%u ramassee par le joueur %u%s", name, event.id, from,
            !weapon ? " : introuvable au sol ici" : exact ? "" : " : reconnue a l'endroit");
        if (!weapon) return;
        if (remote) mem::TakeWeapon(remote, weapon);
        else mem::DeleteWeapon(weapon);
    }
}

static void OnPowerupEvent(uint8_t from, const proto::PowerupEvent& event) {
    Vector3 position = {event.x, event.y, event.z};
    bool removed = mem::DeletePowerupNear(event.type, position, kGroundMatchRadius);
    g_groundReceived++;
    LOG("bonus de type %u ramasse par le joueur %u%s", event.type, from, removed ? "" : " : introuvable au sol ici");
    // L'objet du niveau (LSI) est celui de toute l'équipe. Les scripts ne le cherchent que sur le
    // joueur de la machine où ils tournent (chr_has_lsi 0) : ramassé par un client, il manquait à
    // l'hôte, la porte qui l'attend ne s'ouvrait pas, et le niveau restait bloqué. Chaque machine
    // le donne donc à son propre joueur dès qu'un autre l'a ramassé.
    if (event.type == off::kPuTypeLsi) {
        Character* player = mem::Player();
        if (player && !mem::HasLsi(player)) {
            mem::GiveLsi(player);
            LOG("objet du niveau : ramasse par le joueur %u, donne aussi a notre joueur", from);
            mem::ConsolePrint("OniMP : le joueur %u a ramasse l'objet du niveau", from);
        }
    }
    SnapshotPowerups();  // sa disparition n'est pas un ramassage de notre joueur
}

// -------------------------------------------------------------------------------------------------
// Cycle de vie
// -------------------------------------------------------------------------------------------------
void OnLevelChanged() {
    g_doorState.clear();
    g_consoleFlags.clear();
    g_announced.clear();
    g_seeded = false;
    g_localWeaponKnown = false;
    for (RemoteHand& hand : g_hands) hand = RemoteHand();
    g_groundTracked = false;
    g_sentGround[0].clear();
    g_sentGround[1].clear();
    g_pickups.clear();
    memset(g_weaponIds.of, 0, sizeof(g_weaponIds.of));  // le compteur continue : jamais deux fois le même numéro
    memset(g_ownDrop, 0, sizeof(g_ownDrop));
    memset(g_extraSince, 0, sizeof(g_extraSince));
}

void OnConnection(uint8_t player, bool connected) {
    g_localWeaponKnown = false;   // à réannoncer au nouveau venu
    // Chez un client, c'est la liaison avec l'hôte qui change : tout ce qu'il apportait des autres
    // joueurs est à réapprendre.
    if (net::CurrentMode() != cfg::Mode::Host) {
        for (RemoteHand& hand : g_hands) hand = RemoteHand();
        memset(g_weaponIds.of, 0, sizeof(g_weaponIds.of));  // les numéros sont ceux de l'hôte qu'on quitte
    } else if (player < proto::kMaxPlayers) {
        g_hands[player] = RemoteHand();
    }
    memset(g_extraSince, 0, sizeof(g_extraSince));
    g_groundTracked = false;
    g_sentGround[0].clear();      // l'état du sol aussi
    g_sentGround[1].clear();
    g_nextGroundRefresh = 0.0;
    g_pickups.clear();
    (void)connected;
}

void OnReliable(uint8_t kind, uint8_t from, const uint8_t* data, int size, double now) {
    if (size < 4 || from >= proto::kMaxPlayers) return;
    uint32_t level;
    memcpy(&level, data, 4);
    if (level != mem::LevelHash()) return;  // autre niveau : sans objet ici
    data += 4;
    size -= 4;

    if (kind == proto::kReliableReload) {
        g_hands[from].reloadAt = now + kPuppetDelay;
        return;
    }

    if (kind == proto::kReliableWeapon) {
        if (size < 1 || size < 1 + data[0] || data[0] > 32) return;
        std::string weapon(reinterpret_cast<const char*>(data + 1), data[0]);
        // Le nom finit dans une ligne de script : on n'accepte que des noms de classe d'arme.
        for (char c : weapon) {
            bool valid = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
            if (!valid) return;
        }
        RemoteHand& hand = g_hands[from];
        if (!hand.known || weapon != hand.weapon) {
            hand.nextFix = 0.0;  // changement d'arme : appliqué sans attendre
            hand.tries = 0;
        }
        hand.weapon = weapon;
        hand.known = true;
        return;
    }
    if (kind == proto::kReliableWeaponEvent) {
        proto::WeaponEvent event;
        if (size < static_cast<int>(sizeof(event)) || !mem::Player()) return;
        memcpy(&event, data, sizeof(event));
        OnWeaponEvent(from, event);
        return;
    }
    if (kind == proto::kReliablePowerup) {
        proto::PowerupEvent event;
        if (size < static_cast<int>(sizeof(event)) || !mem::Player()) return;
        memcpy(&event, data, sizeof(event));
        OnPowerupEvent(from, event);
        return;
    }
    if (kind == proto::kReliableGroundPowerups || kind == proto::kReliableGroundWeapons) {
        if (from != proto::kHostPlayerId) return;
        if (!cfg::Get().npcs || net::CurrentMode() != cfg::Mode::Client || !mem::Player()) return;
        if (kind == proto::kReliableGroundPowerups) OnGroundPowerups(data, size, now);
        else OnGroundWeapons(data, size, now);
        return;
    }

    if (size < 2 || !cfg::Get().quests) return;
    uint16_t id;
    memcpy(&id, data, 2);
    uint8_t rank = size >= 3 ? data[2] : 0;  // absent avant la 0.21
    if (kind == proto::kReliableDoor) OpenDoor(id, rank);
    else if (kind == proto::kReliableConsole) ActivateConsole(id, rank, from, now);
}

void Update(double now) {
    if (!mem::Player()) return;
    // Une case libérée perd son numéro : l'arme qui la reprendra en sera une autre.
    for (int i = 0; i < addr::kMaxWeapons; i++) {
        if (g_weaponIds.of[i] && !mem::WeaponInUse(mem::WeaponAt(i))) g_weaponIds.of[i] = 0;
    }

    // L'état est relevé même hors connexion : à l'arrivée de l'autre joueur, seuls les
    // changements postérieurs sont annoncés.
    bool announce = SharedWorld();
    UpdateDoors(announce, now);
    UpdateConsoles(announce, now);
    g_seeded = true;

    if (net::Connected()) {
        TrackGround(now);  // avant l'état : l'autre joueur traite l'arme lâchée avant de voir la main vide
        SendLocalWeapon(now);
        for (uint32_t player = 0; player < static_cast<uint32_t>(proto::kMaxPlayers); player++) {
            ApplyRemoteWeapon(player, now);
            ApplyRemoteReload(player, now);
        }
        // Après les ramassages de cette frame : le client les traite avant la liste qui en tient compte.
        if (cfg::Get().npcs && net::CurrentMode() == cfg::Mode::Host) SendGround(now);
    }
}

// -------------------------------------------------------------------------------------------------
// Comparaison de l'hôte et d'un client (voir desync.cpp)
// -------------------------------------------------------------------------------------------------
// Empreinte des consoles activées.
uint32_t ConsoleDigest() {
    uint32_t mix = 0, count = 0;
    for (const auto& entry : g_consoleFlags) {
        if (!(entry.second & off::kConsoleTriggered)) continue;
        mix ^= (static_cast<uint32_t>(entry.first) + 1u) * 2654435761u;
        count++;
    }
    return mix + count;
}

// Empreinte des bonus au sol : combien de chaque type. Leur place exacte diffère toujours un peu.
uint32_t GroundDigest() {
    static Powerup powerups[addr::kMaxPowerups];
    uint32_t counts[off::kPuTypeCount] = {};
    int count = ReadPowerups(powerups);
    for (int i = 0; i < count; i++) {
        if (powerups[i].type < off::kPuTypeCount) counts[powerups[i].type]++;
    }
    uint32_t digest = 0;
    for (uint32_t typeCount : counts) digest = digest * 67u + typeCount;
    return digest;
}

void ResendConsoles() {
    for (const auto& entry : g_consoleFlags) {
        if (!(entry.second & off::kConsoleTriggered)) continue;
        uint8_t data[3];
        uint16_t id = static_cast<uint16_t>(entry.first & 0xFFFF);
        memcpy(data, &id, 2);
        data[2] = static_cast<uint8_t>(entry.first >> 16);
        Send(proto::kReliableConsole, data, sizeof(data));
    }
}

void ResendGround() {
    g_sentGround[0].clear();
    g_sentGround[1].clear();
    g_nextGroundCheck = 0.0;
}

void Describe(char* out, int capacity) {
    std::string others;
    for (int player = 0; player < proto::kMaxPlayers; player++) {
        if (!g_hands[player].known) continue;
        others += " j" + std::to_string(player) + " '" + g_hands[player].weapon + "'";
    }
    int numbered = 0;
    for (uint16_t id : g_weaponIds.of) numbered += id ? 1 : 0;
    snprintf(out, static_cast<size_t>(capacity),
             "portes %u envoyees/%u recues, consoles %u/%u, objets au sol %u/%u, %d arme(s) numerotee(s), "
             "arme locale '%s', autres :%s",
             g_doorsSent, g_doorsReceived, g_consolesSent, g_consolesReceived, g_groundSent, g_groundReceived, numbered,
             g_localWeapon.c_str(), others.empty() ? " aucune" : others.c_str());
}

}  // namespace world
