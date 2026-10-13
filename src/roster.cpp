#include "roster.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "cheat.h"
#include "config.h"
#include "coop.h"
#include "log.h"
#include "memory.h"
#include "network.h"
#include "protocol.h"
#include "replication.h"

using namespace oni;

namespace roster {

// La première est celle du chapitre : le niveau la donne à Konoko, chacun des siens. Les autres
// sont les classes de Konoko rangées dans les données communes (level0), donc chargées partout.
struct SkinInfo {
    const char* className;   // nul : la classe que le niveau a donnée à notre joueur
    const char* label;
};
static const SkinInfo kSkins[kSkinCount] = {
    {nullptr, "Chapter outfit"}, {"k3", "K3"},     {"k4_A", "K4 A"}, {"k4_F", "K4 F"},
    {"k4_G", "K4 G"},            {"k4_K", "K4 K"}, {"k4_L", "K4 L"},
};

static const double kPingRefresh = 2.0;      // hôte : latences redites aux clients à ce rythme, si elles ont bougé
static const double kClassRetry = 1.0;       // d'un essai au suivant pour donner sa tenue à un personnage
static const int kMaxClassTries = 4;
static const int32_t kClassDelayTicks = 90;  // pas pendant la première seconde et demie d'un niveau

static proto::Roster g_roster = {};          // hôte : ce qu'il décide ; client : ce que l'hôte a dit
static bool g_rosterKnown = false;           // client : reçu depuis la connexion
static std::string g_levelClass;             // classe que le niveau a donnée à notre joueur
static uint32_t g_levelHash = 0;
static double g_now = 0.0;
static double g_nextPings = 0.0;
static bool g_dirty = false;                 // hôte : la liste a changé, à redire
// Tenue donnée à chaque personnage : essais, et quand recommencer. La case de notre numéro est
// celle de notre joueur.
struct Applied {
    Character* chr = nullptr;
    int tries = 0;
    double next = 0.0;
    bool refused = false;
};
static Applied g_applied[proto::kMaxPlayers];

static bool Host() { return net::CurrentMode() == cfg::Mode::Host; }
static bool Client() { return net::CurrentMode() == cfg::Mode::Client; }
static uint8_t Local() { return net::LocalPlayerId(); }
static bool Present(uint8_t player) { return player < proto::kMaxPlayers && (g_roster.present >> player) & 1; }

const char* SkinLabel(int skin) { return skin >= 0 && skin < kSkinCount ? kSkins[skin].label : "?"; }

// Lettres, chiffres et quelques signes : ce que la police des jauges sait écrire. Quinze au plus.
static std::string Sanitized(const char* name) {
    std::string out;
    for (const char* c = name; c && *c && out.size() < proto::kNameLength - 1; c++) {
        bool valid = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_' ||
                     *c == '-' || *c == '.' || *c == ' ';
        if (valid && !(*c == ' ' && out.empty())) out.push_back(*c);
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

static void StoreName(uint8_t player, const char* name) {
    std::string clean = Sanitized(name);
    memset(g_roster.name[player], 0, proto::kNameLength);
    memcpy(g_roster.name[player], clean.data(), clean.size());
}

const char* Name(uint8_t player) {
    static const char* const kDefaults[proto::kMaxPlayers] = {"HOST", "P1", "P2", "P3"};
    if (player >= proto::kMaxPlayers) return "?";
    // Le nôtre vaut tout de suite, sans attendre que l'hôte le redise.
    if (player == Local() && !cfg::Get().playerName.empty()) return cfg::Get().playerName.c_str();
    static char name[proto::kMaxPlayers][proto::kNameLength + 1];
    memcpy(name[player], g_roster.name[player], proto::kNameLength);
    name[player][proto::kNameLength] = '\0';
    return name[player][0] ? name[player] : kDefaults[player];
}

int PingMs(uint8_t player) {
    if (player >= proto::kMaxPlayers) return -1;
    if (Host()) return net::PingMs(player);
    if (player == proto::kHostPlayerId) return net::PingMs(proto::kHostPlayerId);
    return g_rosterKnown && Present(player) ? g_roster.ping[player] : -1;
}

const char* LocalName() { return cfg::Get().playerName.c_str(); }

// -------------------------------------------------------------------------------------------------
// Liste des joueurs
// -------------------------------------------------------------------------------------------------
static bool SkinTaken(int skin, uint8_t except) {
    for (uint8_t player = 0; player < proto::kMaxPlayers; player++) {
        if (player != except && Present(player) && g_roster.skin[player] == skin) return true;
    }
    return false;
}

// Hôte : la tenue demandée si elle est libre, sinon la première qui l'est. Il y en a plus que de joueurs.
static uint8_t Grant(uint8_t player, uint8_t wanted) {
    if (wanted < kSkinCount && !SkinTaken(wanted, player)) return wanted;
    for (int skin = 0; skin < kSkinCount; skin++) {
        if (!SkinTaken(skin, player)) return static_cast<uint8_t>(skin);
    }
    return 0;
}

static void SendRoster() {
    g_dirty = false;
    if (!Host() || !net::Connected()) return;
    uint8_t packet[1 + sizeof(g_roster)];
    packet[0] = proto::kReliableRoster;
    memcpy(packet + 1, &g_roster, sizeof(g_roster));
    net::SendReliable(packet, sizeof(packet));
}

static void SendProfile() {
    if (!Client() || !net::Connected()) return;
    proto::Profile profile = {};
    std::string name = Sanitized(cfg::Get().playerName.c_str());
    memcpy(profile.name, name.data(), name.size());
    int skin = cfg::Get().playerSkin;
    profile.skin = skin >= 0 && skin < kSkinCount ? static_cast<uint8_t>(skin) : proto::kAnySkin;
    uint8_t packet[1 + sizeof(profile)];
    packet[0] = proto::kReliableProfile;
    memcpy(packet + 1, &profile, sizeof(profile));
    net::SendReliable(packet, sizeof(packet));
}

// Hôte : sa propre ligne, d'après son réglage.
static void SeedHost() {
    if (!Host()) return;
    bool was = Present(proto::kHostPlayerId);
    g_roster.present |= 1;
    StoreName(proto::kHostPlayerId, cfg::Get().playerName.c_str());
    int wanted = cfg::Get().playerSkin;
    if (!was) g_roster.skin[0] = Grant(0, wanted >= 0 && wanted < kSkinCount ? static_cast<uint8_t>(wanted) : 0);
}

void OnConnection(uint8_t player, bool connected) {
    if (Host()) {
        SeedHost();
        if (player == proto::kHostPlayerId || player >= proto::kMaxPlayers) return;
        if (connected) {
            // Une tenue libre tout de suite, pour qu'il se distingue des autres dès son apparition ;
            // son profil, qui suit, dira celle qu'il préfère.
            g_roster.present |= static_cast<uint8_t>(1u << player);
            g_roster.skin[player] = Grant(player, proto::kAnySkin);
            memset(g_roster.name[player], 0, proto::kNameLength);
            g_roster.ping[player] = 0;
            LOG("joueurs : joueur %u arrive, tenue '%s' en attendant son choix", player, SkinLabel(g_roster.skin[player]));
        } else {
            g_roster.present &= static_cast<uint8_t>(~(1u << player));
            g_applied[player] = Applied();
        }
        g_dirty = true;
        coop::Touch();
        return;
    }
    if (connected) {
        SendProfile();
    } else {
        g_rosterKnown = false;
    }
}

void OnProfile(uint8_t from, const uint8_t* data, int size) {
    proto::Profile profile;
    if (!Host() || from == proto::kHostPlayerId || from >= proto::kMaxPlayers || size < static_cast<int>(sizeof(profile))) return;
    memcpy(&profile, data, sizeof(profile));
    profile.name[proto::kNameLength - 1] = '\0';
    g_roster.present |= static_cast<uint8_t>(1u << from);
    StoreName(from, profile.name);
    uint8_t granted = Grant(from, profile.skin);
    if (granted != g_roster.skin[from] || profile.skin != granted) {
        LOG("joueurs : joueur %u ('%s') demande la tenue %s : %s", from, Name(from),
            profile.skin < kSkinCount ? SkinLabel(profile.skin) : "libre",
            granted == profile.skin ? "accordee" : SkinLabel(granted));
    }
    g_roster.skin[from] = granted;
    g_dirty = true;
    coop::Touch();
}

void OnRoster(uint8_t from, const uint8_t* data, int size) {
    proto::Roster received;
    if (!Client() || from != proto::kHostPlayerId || size < static_cast<int>(sizeof(received))) return;
    memcpy(&received, data, sizeof(received));
    for (uint8_t player = 0; player < proto::kMaxPlayers; player++) {
        received.name[player][proto::kNameLength - 1] = '\0';
        if (received.skin[player] >= kSkinCount) received.skin[player] = 0;
    }
    bool changed = !g_rosterKnown || received.present != g_roster.present ||
                   memcmp(received.skin, g_roster.skin, sizeof(received.skin)) != 0 ||
                   memcmp(received.name, g_roster.name, sizeof(received.name)) != 0;
    g_roster = received;
    g_rosterKnown = true;
    if (!changed) return;  // les latences seules
    uint8_t self = Local();
    if (Present(self)) {
        // La tenue que l'hôte nous donne est celle que nous redemanderons.
        if (cfg::Get().playerSkin != g_roster.skin[self]) {
            LOG("joueurs : l'hote nous donne la tenue %s", SkinLabel(g_roster.skin[self]));
            cfg::Get().playerSkin = g_roster.skin[self];
            cfg::SavePlayer();
        }
    }
    coop::Touch();
}

void SetLocalName(const char* name) {
    std::string clean = Sanitized(name);
    if (clean == cfg::Get().playerName) return;
    cfg::Get().playerName = clean;
    cfg::SavePlayer();
    LOG("joueurs : pseudo '%s' enregistre dans onimp.ini", clean.c_str());
    mem::ConsolePrint("OniMP : pseudo %s", clean.empty() ? "retire" : clean.c_str());
    if (Host()) {
        SeedHost();
        g_dirty = true;
    } else {
        SendProfile();
    }
    coop::Touch();
}

static int LocalSkin() {
    uint8_t self = Local();
    if ((Host() || g_rosterKnown) && Present(self)) return g_roster.skin[self];
    int skin = cfg::Get().playerSkin;
    return skin >= 0 && skin < kSkinCount ? skin : 0;
}

bool SetSkin(int skin) {
    if (skin < 0 || skin >= kSkinCount) return false;
    uint8_t self = Local();
    if (SkinTaken(skin, self)) {
        mem::ConsolePrint("OniMP : tenue %s deja prise par un autre joueur", SkinLabel(skin));
        return false;
    }
    cfg::Get().playerSkin = skin;
    cfg::SavePlayer();
    if (Client()) {
        SendProfile();  // l'hôte tranche, et le redit à tous
    } else {
        g_roster.present |= 1;
        g_roster.skin[0] = static_cast<uint8_t>(skin);
        g_dirty = true;
    }
    g_applied[self] = Applied();
    LOG("joueurs : tenue %s choisie", SkinLabel(skin));
    coop::Touch();
    return true;
}

void SkinValue(char* out, size_t size) { snprintf(out, size, "%s", SkinLabel(LocalSkin())); }

void NextSkin() {
    int current = LocalSkin();
    for (int step = 1; step < kSkinCount; step++) {
        int skin = (current + step) % kSkinCount;
        if (SkinTaken(skin, Local())) continue;  // grisée : on passe
        SetSkin(skin);
        return;
    }
}

// -------------------------------------------------------------------------------------------------
// Tenues : la classe de personnage de chacun
// -------------------------------------------------------------------------------------------------
static bool ClassExists(const char* name) {
    void* instance = nullptr;
    return fn::GetDataPtr(Tag('O', 'N', 'C', 'C'), name, &instance) == 0 && instance;
}

static const char* ClassOfSkin(int skin) {
    const char* name = skin > 0 && skin < kSkinCount ? kSkins[skin].className : nullptr;
    if (name) return name;
    return g_levelClass.empty() ? cfg::Get().playerClass.c_str() : g_levelClass.c_str();
}

// Le personnage que ce joueur incarne (onglet Character) passe avant sa tenue de Konoko, tant que
// sa classe est dans les données de ce niveau.
static const char* WantedClass(uint8_t player, int skin) {
    const char* morph = cheat::CharacterOf(player);
    return morph[0] && ClassExists(morph) ? morph : ClassOfSkin(skin);
}

const char* ClassFor(uint8_t player) {
    if (player >= proto::kMaxPlayers || !Present(player)) return cfg::Get().playerClass.c_str();
    const char* name = WantedClass(player, g_roster.skin[player]);
    return ClassExists(name) ? name : cfg::Get().playerClass.c_str();
}

void Redress(uint8_t player) {
    if (player < proto::kMaxPlayers) g_applied[player] = Applied();
}

// Donne au personnage la classe voulue, par la commande de script du moteur.
static void Dress(uint8_t player, Character* chr, const char* wanted) {
    Applied& applied = g_applied[player];
    if (applied.chr != chr) applied = Applied();
    applied.chr = chr;
    if (!wanted[0] || strcmp(mem::ClassName(chr), wanted) == 0) {
        applied.tries = 0;
        return;
    }
    if (applied.refused || g_now < applied.next || mem::IsDead(chr)) return;
    applied.next = g_now + kClassRetry;
    const char* name = mem::Name(chr);
    if (!ClassExists(wanted) || applied.tries >= kMaxClassTries) {
        applied.refused = true;
        LOG("tenue : '%s' ne peut pas prendre la classe '%s' (%s)", name, wanted,
            applied.tries ? "le moteur ne la lui garde pas" : "absente des donnees du jeu");
        return;
    }
    applied.tries++;
    LOG("tenue : '%s' passe de '%s' a '%s' (joueur %u)", name, mem::ClassName(chr), wanted, player);
    mem::SetClass(chr, wanted);
}

void OnLevelChanged() {
    for (Applied& applied : g_applied) applied = Applied();
    g_levelClass.clear();
    g_levelHash = 0;
}

void Update(double now) {
    g_now = now;
    Character* player = mem::Player();
    if (!player || !net::Running()) return;
    if (Host()) SeedHost();

    // La classe que le niveau donne à Konoko, relevée avant d'y toucher : c'est la tenue du chapitre.
    if (g_levelHash != mem::LevelHash()) {
        g_levelHash = mem::LevelHash();
        g_levelClass = mem::ClassName(player);
    }
    if (mem::GameTime() >= kClassDelayTicks) {
        uint8_t self = Local();
        Dress(self, player, WantedClass(self, LocalSkin()));
        for (uint8_t other = 0; other < proto::kMaxPlayers; other++) {
            if (other == self || !Present(other)) continue;
            if (Character* puppet = rep::RemotePlayerCharacter(other)) Dress(other, puppet, WantedClass(other, g_roster.skin[other]));
        }
    }

    if (!Host()) return;
    // Latences : celles que l'hôte mesure, redites aux clients, qui ne se voient pas entre eux.
    if (now >= g_nextPings) {
        g_nextPings = now + kPingRefresh;
        for (uint8_t client = 1; client < proto::kMaxPlayers; client++) {
            int ping = Present(client) ? net::PingMs(client) : -1;
            uint16_t value = static_cast<uint16_t>(ping < 0 ? 0 : (ping > 9999 ? 9999 : ping));
            if (value != g_roster.ping[client]) {
                g_roster.ping[client] = value;
                g_dirty = true;
            }
        }
    }
    if (g_dirty) SendRoster();
}

void Describe(char* out, int capacity) {
    int length = snprintf(out, static_cast<size_t>(capacity), "joueurs :");
    for (uint8_t player = 0; player < proto::kMaxPlayers && length > 0 && length < capacity; player++) {
        if (player != Local() && !Present(player)) continue;
        int skin = player == Local() ? LocalSkin() : g_roster.skin[player];
        int ping = player == Local() ? 0 : PingMs(player);
        length += snprintf(out + length, static_cast<size_t>(capacity - length), " %u '%s' %s %d ms,", player, Name(player),
                           SkinLabel(skin), ping);
    }
}

}  // namespace roster
