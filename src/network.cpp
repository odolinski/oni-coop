#include "network.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <iphlpapi.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "lang.h"
#include "log.h"

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
#ifndef MIB_IPADDR_DISCONNECTED
#define MIB_IPADDR_DISCONNECTED 0x0008
#endif
#ifndef MIB_IPADDR_DELETED
#define MIB_IPADDR_DELETED 0x0040
#endif

namespace net {

static const double kTimeout = 5.0;          // silence d'un pair avant déconnexion
static const double kBeaconInterval = 2.0;   // entre deux annonces de l'hôte
static const double kInterfaceScan = 5.0;    // entre deux relevés des interfaces réseau
static const double kJoinTimeout = 6.0;      // client : sans réponse de l'hôte trouvé, il cherche de nouveau
static const double kRejectedPause = 10.0;   // client : un hôte qui l'a refusé est laissé de côté ce temps-là
// Hôte : une liaison par client. Client : une seule, la première, vers l'hôte.
static const int kMaxPeers = proto::kMaxPlayers - 1;
static const double kPlayerSeqMemory = 1.0;  // au-delà, le dernier état reçu d'un joueur ne départage plus rien
static const double kHelloInterval = 0.5;
static const double kResendInterval = 0.2;
static const int kMaxResends = 25;
static const int kMaxPacketsPerPoll = 256;
static const int kDatagramSize = 1400;

struct PendingEvent {
    proto::Event event;
    uint32_t levelHash;
    double nextSend;
    int tries;
};

// Flux fiable et ordonné, un par liaison.
struct StreamMessage {
    uint32_t seq;
    uint8_t origin;              // joueur auteur du message
    std::string data;
    double nextSend;
};
static const double kStreamResend = 0.15;
static const int kStreamBurst = 24;          // messages (ré)émis au plus par liaison et par Poll

struct Peer {
    sockaddr_in address = {};
    bool known = false;          // hôte : place prise par un client ; client : adresse de l'hôte connue
    bool connected = false;
    // Connexion en cours (proto::Hello). Client : celle qu'il demande ; hôte : celle qu'il a acceptée.
    uint32_t session = 0;
    double lastReceive = 0.0;
    // Dernier état reçu par cette liaison, pour chaque joueur dont elle apporte l'état.
    uint16_t lastPlayerSeq[proto::kMaxPlayers] = {};
    double lastPlayerTime[proto::kMaxPlayers] = {};
    bool havePlayerSeq[proto::kMaxPlayers] = {};
    std::vector<PendingEvent> pending;
    uint16_t seenEvents[64] = {};
    int seenEventCount = 0;
    std::deque<StreamMessage> stream;   // émis, pas encore accusés
    uint32_t streamNextSeq = 0;
    uint32_t streamExpected = 0;        // prochain numéro attendu en réception
    // Latence : aller-retour d'un kPing, lissé, en millisecondes ; négatif tant qu'aucun n'est revenu.
    double rtt = -1.0;
    double nextPing = 0.0;

    // Hôte en pause (voir PumpRelay) : ce qui est arrivé de ce client pendant ce temps, à remettre
    // à nos fonctions au prochain Poll. De ses états, seul le dernier compte.
    struct Held {
        bool isEvent;
        uint32_t levelHash;
        proto::Event event;
        std::string data;               // message du flux fiable
    };
    std::deque<Held> held;
    bool heldPlayer = false;
    proto::EntityState heldState = {};
    uint32_t heldLevel = 0;
};

static bool g_wsaStarted = false;
static SOCKET g_socket = INVALID_SOCKET;
static cfg::Mode g_mode = cfg::Mode::Off;
static Peer g_peers[kMaxPeers];
static Peer& g_host = g_peers[0];   // client : la liaison vers l'hôte
static uint8_t g_localId = 1;       // client : numéro donné par l'hôte
static double g_nextHello = 0.0;
static uint16_t g_sequence = 0;
static uint16_t g_nextEventId = 1;
static uint32_t g_lastLevelHash = 0;
static bool g_reconnectRequested = false;
static double g_now = 0.0;          // instant du Poll en cours
static bool (*g_shared)(uint8_t kind) = nullptr;  // Handlers::shared du dernier Poll
static bool g_deferring = false;    // dans PumpRelay : rien n'est remis à nos fonctions, tout est gardé

// Ce que l'hôte annonce, et ce que le client cherche.
static std::string g_gameName = "Oni";
static std::string g_joinName;
static std::string g_modVersion = "?";
static std::string g_levelName;

// Réseau où l'hôte s'annonce et d'où chacun accepte l'autre (voir Configure).
enum class Scope { Auto, Radmin, Any, Address };
static Scope g_scope = Scope::Auto;
static uint32_t g_scopeAddress = 0;  // ordre réseau, comme toutes les adresses ici

struct Interface {
    uint32_t address;
    uint32_t mask;
};
// Hôte : une prise par interface retenue, liée à son adresse. C'est ce lien qui fait sortir une
// diffusion par cette interface-là, et non par celle que Windows préfère.
struct BeaconSocket {
    SOCKET socket;
    Interface on;
};
static std::vector<Interface> g_interfaces;      // interfaces retenues au dernier relevé
static std::vector<BeaconSocket> g_beacons;
static double g_nextInterfaceScan = 0.0;
static double g_nextBeacon = 0.0;
static uint32_t g_hostId = 0;
static bool g_scanned = false;                   // au moins un relevé fait depuis Start
static bool g_hadRadmin = false;                 // Radmin VPN présent au dernier relevé
static bool g_outOfScopeLogged = false;

// Client en recherche automatique : il écoute les annonces et vise l'hôte qu'il trouve.
static SOCKET g_discovery = INVALID_SOCKET;
static bool g_discover = false;
static uint16_t g_port = 0;
static uint32_t g_targetHost = 0;                // hostId de l'hôte visé
static double g_targetSince = 0.0;
static uint32_t g_ignoredHost = 0;               // hôte laissé de côté jusqu'à g_ignoreUntil
static double g_ignoreUntil = 0.0;
static uint32_t g_noticedHost = 0;               // dernier hôte écarté dont on a dit pourquoi
static uint8_t g_noticedReason = 0;
static uint8_t g_rejectNoticed = 0;              // dernier refus signalé pour l'hôte visé

static void ResetStream(Peer& peer) {
    peer.stream.clear();
    peer.streamNextSeq = 0;
    peer.streamExpected = 0;
}

// Le client change d'identifiant à chaque tentative : jamais nul, jamais le précédent.
static void NewSession() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    uint32_t session = static_cast<uint32_t>(counter.LowPart) ^ (static_cast<uint32_t>(GetCurrentProcessId()) << 16);
    if (session == 0 || session == g_host.session) session = g_host.session + 1;
    if (session == 0) session = 1;
    g_host.session = session;
}

// Horloge en millisecondes, ramenée sur 32 bits.
static uint32_t MillisecondStamp(double now) {
    return static_cast<uint32_t>(static_cast<uint64_t>(now * 1000.0) & 0xFFFFFFFFu);
}

static bool SameAddress(const sockaddr_in& a, const sockaddr_in& b) {
    return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
}

// Numéro du joueur au bout de cette liaison.
static uint8_t PlayerOf(const Peer& peer) {
    return g_mode == cfg::Mode::Host ? static_cast<uint8_t>(&peer - g_peers + 1) : proto::kHostPlayerId;
}

static Peer* PeerOf(uint8_t player) {
    if (g_mode == cfg::Mode::Host) return player >= 1 && player <= kMaxPeers ? &g_peers[player - 1] : nullptr;
    return player == proto::kHostPlayerId ? &g_host : nullptr;
}

static Peer* FindPeer(const sockaddr_in& from) {
    for (Peer& peer : g_peers) {
        if (peer.known && SameAddress(from, peer.address)) return &peer;
    }
    return nullptr;
}

static int CountConnected() {
    int count = 0;
    for (const Peer& peer : g_peers) count += peer.connected ? 1 : 0;
    return count;
}

static void AddressText(const sockaddr_in& address, char* out, int capacity) {
    const uint8_t* ip = reinterpret_cast<const uint8_t*>(&address.sin_addr.s_addr);
    snprintf(out, capacity, "%u.%u.%u.%u:%u", ip[0], ip[1], ip[2], ip[3], ntohs(address.sin_port));
}

static void IpText(uint32_t address, char* out, int capacity) {
    const uint8_t* ip = reinterpret_cast<const uint8_t*>(&address);
    snprintf(out, capacity, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

// Au journal, et au joueur si l'appelant a de quoi le lui dire.
static void Notice(const Handlers* handlers, const char* format, ...) ONIMP_PRINTF(2, 3);
static void Notice(const Handlers* handlers, const char* format, ...) {
    char text[200];
    va_list args;
    va_start(args, format);
    loc::FormatV(text, sizeof(text), format, args);  // dans la langue du joueur : il le lira à l'écran
    va_end(args);
    LOG("reseau : %s", text);
    if (handlers && handlers->onNotice) handlers->onNotice(text);
}

// -------------------------------------------------------------------------------------------------
// Interfaces réseau
// -------------------------------------------------------------------------------------------------
// Radmin VPN donne à chaque machine une adresse en 26.x.x.x sur une carte réseau virtuelle, à côté
// de l'Ethernet ou du Wi-Fi. Une diffusion envoyée sans précaution part par la carte que Windows
// préfère, qui n'est pas forcément celle-là : l'autre joueur ne la reçoit jamais.
static bool IsRadmin(uint32_t address) { return (ntohl(address) >> 24) == 26; }

static std::vector<Interface> ListInterfaces() {
    std::vector<Interface> found;
    ULONG size = 0;
    GetIpAddrTable(nullptr, &size, FALSE);
    if (size == 0) return found;
    std::vector<uint8_t> buffer(size);
    MIB_IPADDRTABLE* table = reinterpret_cast<MIB_IPADDRTABLE*>(buffer.data());
    if (GetIpAddrTable(table, &size, FALSE) != NO_ERROR) return found;
    for (DWORD i = 0; i < table->dwNumEntries; i++) {
        const MIB_IPADDRROW& row = table->table[i];
        if (row.dwAddr == 0 || row.dwMask == 0) continue;
        if (row.wType & (MIB_IPADDR_DISCONNECTED | MIB_IPADDR_DELETED)) continue;
        if ((ntohl(row.dwAddr) >> 24) == 127) continue;  // la boucle locale a son annonce à part
        found.push_back({row.dwAddr, row.dwMask});
    }
    return found;
}

static std::vector<Interface> SelectInterfaces(const std::vector<Interface>& all) {
    if (g_scope == Scope::Any) return all;
    std::vector<Interface> selected;
    for (const Interface& candidate : all) {
        bool wanted = g_scope == Scope::Address ? candidate.address == g_scopeAddress : IsRadmin(candidate.address);
        if (wanted) selected.push_back(candidate);
    }
    if (selected.empty() && g_scope == Scope::Auto) return all;  // pas de Radmin VPN : réseau local
    return selected;
}

// Un paquet venu de cette adresse est-il du réseau choisi ? En mode automatique tout est accepté :
// Radmin VPN n'y est qu'une préférence pour s'annoncer.
static bool InScope(uint32_t source) {
    if (g_scope == Scope::Auto || g_scope == Scope::Any) return true;
    if (g_scope == Scope::Radmin) return IsRadmin(source);
    for (const Interface& chosen : g_interfaces) {
        if ((source & chosen.mask) == (chosen.address & chosen.mask)) return true;
    }
    return false;
}

static uint16_t DiscoveryPort(uint16_t port) { return port == 65535 ? 65534 : static_cast<uint16_t>(port + 1); }

static void CloseBeacons() {
    for (BeaconSocket& beacon : g_beacons) closesocket(beacon.socket);
    g_beacons.clear();
}

static void OpenBeacon(const Interface& on) {
    SOCKET beacon = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (beacon == INVALID_SOCKET) return;
    u_long nonBlocking = 1;
    ioctlsocket(beacon, FIONBIO, &nonBlocking);
    BOOL broadcast = TRUE;
    setsockopt(beacon, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast), sizeof(broadcast));
    sockaddr_in local = {};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = on.address;
    if (bind(beacon, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR) {
        closesocket(beacon);
        return;
    }
    g_beacons.push_back({beacon, on});
}

// Hôte : relève les interfaces et rouvre les prises d'annonce si elles ont changé. Radmin VPN peut
// être lancé, coupé ou reconnecté en cours de partie.
static void ScanInterfaces(double now, const Handlers* handlers) {
    if (now < g_nextInterfaceScan) return;
    g_nextInterfaceScan = now + kInterfaceScan;

    std::vector<Interface> selected = SelectInterfaces(ListInterfaces());
    bool hasRadmin = false;
    for (const Interface& chosen : selected) hasRadmin = hasRadmin || IsRadmin(chosen.address);
    bool changed = !g_scanned || hasRadmin != g_hadRadmin;
    g_hadRadmin = hasRadmin;
    bool concerned = g_mode == cfg::Mode::Host || g_discover;
    if (changed && concerned && !hasRadmin && g_scope == Scope::Radmin) {
        Notice(handlers, g_mode == cfg::Mode::Host
                             ? "Radmin VPN introuvable : la partie n'est pas annoncee tant qu'il n'est pas connecte"
                             : "Radmin VPN introuvable : aucun hote ne sera trouve tant qu'il n'est pas connecte");
    } else if (changed && concerned && g_scanned && (g_scope == Scope::Radmin || g_scope == Scope::Auto)) {
        Notice(handlers, hasRadmin ? "Radmin VPN connecte" : "Radmin VPN deconnecte");
    }

    bool same = g_scanned && selected.size() == g_interfaces.size();
    g_scanned = true;
    for (size_t i = 0; same && i < selected.size(); i++) {
        same = selected[i].address == g_interfaces[i].address && selected[i].mask == g_interfaces[i].mask;
    }
    if (same) return;

    g_interfaces = selected;
    if (g_mode != cfg::Mode::Host) return;
    CloseBeacons();
    std::string list;
    for (const Interface& chosen : selected) {
        OpenBeacon(chosen);
        char text[24];
        IpText(chosen.address, text, sizeof(text));
        list += std::string(list.empty() ? "" : ", ") + text + (IsRadmin(chosen.address) ? " (Radmin VPN)" : "");
    }
    // Sur la même machine, un client trouve aussi l'hôte : utile pour essayer sans réseau.
    if (g_scope == Scope::Auto || g_scope == Scope::Any) OpenBeacon({htonl(INADDR_LOOPBACK), htonl(0xFF000000)});
    LOG("reseau : partie annoncee sur %s", list.empty() ? "aucune interface" : list.c_str());
}

static void SendOn(SOCKET from, const sockaddr_in& to, uint8_t type, uint32_t levelHash, const void* payload,
                   int payloadSize);

// Hôte : dit au réseau qu'une partie existe. Sur chaque interface retenue, vers l'adresse de
// diffusion de son sous-réseau (26.255.255.255 pour Radmin VPN) et vers 255.255.255.255.
static void SendBeacons(double now, uint32_t levelHash) {
    if (now < g_nextBeacon || g_beacons.empty()) return;
    g_nextBeacon = now + kBeaconInterval;

    proto::Beacon beacon = {};
    int clients = CountConnected();
    beacon.hostId = g_hostId;
    beacon.port = g_port;
    beacon.state = clients >= kMaxPeers ? proto::kBeaconPlaying : proto::kBeaconWaiting;
    beacon.players = static_cast<uint8_t>(1 + clients);
    beacon.maxPlayers = proto::kMaxPlayers;
    strncpy(beacon.name, g_gameName.c_str(), sizeof(beacon.name) - 1);
    strncpy(beacon.modVersion, g_modVersion.c_str(), sizeof(beacon.modVersion) - 1);
    strncpy(beacon.level, g_levelName.c_str(), sizeof(beacon.level) - 1);

    sockaddr_in to = {};
    to.sin_family = AF_INET;
    to.sin_port = htons(DiscoveryPort(g_port));
    for (const BeaconSocket& out : g_beacons) {
        bool loopback = (ntohl(out.on.address) >> 24) == 127;
        to.sin_addr.s_addr = loopback ? out.on.address : (out.on.address | ~out.on.mask);
        SendOn(out.socket, to, proto::kBeacon, levelHash, &beacon, sizeof(beacon));
        if (loopback) continue;
        to.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        SendOn(out.socket, to, proto::kBeacon, levelHash, &beacon, sizeof(beacon));
    }
}

static void SendOn(SOCKET from, const sockaddr_in& to, uint8_t type, uint32_t levelHash, const void* payload,
                   int payloadSize) {
    if (from == INVALID_SOCKET) return;
    char buffer[kDatagramSize];
    if (payloadSize < 0 || static_cast<int>(sizeof(proto::Header)) + payloadSize > kDatagramSize) return;

    proto::Header header;
    header.magic = proto::kMagic;
    header.version = proto::kVersion;
    header.type = type;
    header.seq = ++g_sequence;
    header.levelHash = levelHash;
    memcpy(buffer, &header, sizeof(header));
    if (payloadSize > 0) memcpy(buffer + sizeof(header), payload, payloadSize);

    sendto(from, buffer, static_cast<int>(sizeof(header)) + payloadSize, 0,
           reinterpret_cast<const sockaddr*>(&to), sizeof(to));
}

static void SendRaw(const sockaddr_in& to, uint8_t type, uint32_t levelHash, const void* payload,
                    int payloadSize) {
    SendOn(g_socket, to, type, levelHash, payload, payloadSize);
}

static void SendTo(const Peer& peer, uint8_t type, uint32_t levelHash, const void* payload, int payloadSize) {
    if (peer.known) SendRaw(peer.address, type, levelHash, payload, payloadSize);
}

static void SendToAll(uint8_t type, uint32_t levelHash, const void* payload, int payloadSize) {
    for (const Peer& peer : g_peers) {
        if (peer.connected) SendTo(peer, type, levelHash, payload, payloadSize);
    }
}

static void SetConnected(Peer& peer, bool connected, const Handlers* handlers) {
    if (peer.connected == connected) return;
    char text[32] = "-";
    if (peer.known) AddressText(peer.address, text, sizeof(text));
    uint8_t player = PlayerOf(peer);
    if (g_mode == cfg::Mode::Host) {
        LOG("reseau : %s (%s), joueur %u", connected ? "pair connecte" : "pair perdu", text, player);
    } else {
        LOG("reseau : %s (%s), l'hote ; ici joueur %u", connected ? "pair connecte" : "pair perdu", text, g_localId);
    }

    peer.connected = connected;
    memset(peer.havePlayerSeq, 0, sizeof(peer.havePlayerSeq));
    g_rejectNoticed = 0;
    ResetStream(peer);  // chaque connexion repart d'un flux vierge des deux côtés
    peer.held.clear();
    peer.heldPlayer = false;
    if (!connected) {
        peer.pending.clear();
        peer.seenEventCount = 0;
        if (g_mode == cfg::Mode::Host) peer.known = false;  // la place est libre pour un autre client
        else NewSession();                                  // la prochaine tentative sera une autre connexion
        // Recherche automatique : l'hôte perdu est rappelé un moment (un niveau qui se recharge),
        // puis on écoute de nouveau les annonces.
        g_targetSince = g_now;
    }
    g_nextBeacon = 0.0;  // l'annonce dit tout de suite s'il reste une place
    if (handlers && handlers->onConnection) handlers->onConnection(player, connected);
}

static bool ResolveHost(const char* host, uint16_t port, sockaddr_in& out) {
    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(host, nullptr, &hints, &result) != 0 || !result) return false;
    out = *reinterpret_cast<sockaddr_in*>(result->ai_addr);
    out.sin_port = htons(port);
    freeaddrinfo(result);
    return true;
}

void Configure(const char* gameName, const char* joinName, const char* modVersion, const char* interfaceSpec) {
    g_gameName = gameName && *gameName ? gameName : "Oni";
    g_joinName = joinName ? joinName : "";
    g_modVersion = modVersion ? modVersion : "?";

    std::string spec = interfaceSpec ? interfaceSpec : "";
    g_scopeAddress = 0;
    if (spec.empty() || spec == "auto") {
        g_scope = Scope::Auto;
    } else if (spec == "radmin") {
        g_scope = Scope::Radmin;
    } else if (spec == "any") {
        g_scope = Scope::Any;
    } else {
        g_scopeAddress = inet_addr(spec.c_str());
        g_scope = g_scopeAddress == INADDR_NONE ? Scope::Auto : Scope::Address;
        if (g_scope == Scope::Auto) LOG("reseau : interface '%s' inconnue, choix automatique", spec.c_str());
    }
}

void SetLevelName(const char* name) { g_levelName = name ? name : ""; }

static bool OpenDiscovery(uint16_t port) {
    g_discovery = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_discovery == INVALID_SOCKET) return false;
    u_long nonBlocking = 1;
    ioctlsocket(g_discovery, FIONBIO, &nonBlocking);
    // Le port des annonces est le même pour tous : plusieurs jeux sur une machine doivent pouvoir
    // l'écouter ensemble.
    BOOL reuse = TRUE;
    setsockopt(g_discovery, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    // Écoute sur toutes les interfaces ; le tri par réseau se fait à la réception (voir InScope) :
    // une prise liée à une seule adresse ne reçoit pas partout les diffusions.
    sockaddr_in local = {};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(DiscoveryPort(port));
    if (bind(g_discovery, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR) {
        LOG("reseau : bind() a echoue sur le port d'annonce %u (%d)", DiscoveryPort(port), WSAGetLastError());
        closesocket(g_discovery);
        g_discovery = INVALID_SOCKET;
        return false;
    }
    return true;
}

bool Start(cfg::Mode mode, const char* host, uint16_t port) {
    Stop();
    if (mode == cfg::Mode::Off) return true;

    if (!g_wsaStarted) {
        WSADATA data;
        int error = WSAStartup(MAKEWORD(2, 2), &data);
        if (error != 0) {
            LOG("reseau : WSAStartup a echoue (%d)", error);
            return false;
        }
        g_wsaStarted = true;
    }

    g_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_socket == INVALID_SOCKET) {
        LOG("reseau : socket() a echoue (%d)", WSAGetLastError());
        return false;
    }

    u_long nonBlocking = 1;
    ioctlsocket(g_socket, FIONBIO, &nonBlocking);
    // Sans ceci, un ICMP "port injoignable" (hôte pas encore lancé) fait échouer recvfrom en 10054.
    BOOL reportReset = FALSE;
    DWORD returned = 0;
    WSAIoctl(g_socket, SIO_UDP_CONNRESET, &reportReset, sizeof(reportReset), nullptr, 0, &returned, nullptr,
             nullptr);

    sockaddr_in local = {};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(mode == cfg::Mode::Host ? port : 0);
    if (bind(g_socket, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR) {
        LOG("reseau : bind() a echoue sur le port %u (%d)", port, WSAGetLastError());
        Stop();
        return false;
    }

    g_mode = mode;
    g_port = port;
    for (Peer& peer : g_peers) peer = Peer();
    g_localId = 1;
    g_nextHello = 0.0;
    g_interfaces.clear();
    g_scanned = false;
    g_nextInterfaceScan = 0.0;
    g_nextBeacon = 0.0;
    g_hadRadmin = false;
    g_outOfScopeLogged = false;
    g_targetHost = 0;
    g_ignoredHost = 0;
    g_noticedHost = 0;
    g_rejectNoticed = 0;

    if (mode == cfg::Mode::Client) {
        NewSession();
        if (host && strcmp(host, kAutoHost) == 0) {
            if (!OpenDiscovery(port)) {
                Stop();
                return false;
            }
            g_discover = true;
            LOG("reseau : client, recherche d'un hote (annonces sur le port UDP %u)", DiscoveryPort(port));
            return true;
        }
        if (!ResolveHost(host, port, g_host.address)) {
            LOG("reseau : adresse '%s' invalide", host);
            Stop();
            return false;
        }
        g_host.known = true;
        LOG("reseau : client vers %s:%u", host, port);
    } else {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        g_hostId = (static_cast<uint32_t>(counter.LowPart) ^ (static_cast<uint32_t>(GetCurrentProcessId()) << 16)) | 1u;
        LOG("reseau : hote sur le port UDP %u, %d joueurs au plus, partie '%s' annoncee sur le port %u", port,
            proto::kMaxPlayers, g_gameName.c_str(), DiscoveryPort(port));
    }
    return true;
}

void Stop() {
    if (g_socket != INVALID_SOCKET) {
        SendToAll(proto::kBye, g_lastLevelHash, nullptr, 0);
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
        LOG("reseau : session fermee");
    }
    if (g_discovery != INVALID_SOCKET) closesocket(g_discovery);
    g_discovery = INVALID_SOCKET;
    g_discover = false;
    CloseBeacons();
    g_mode = cfg::Mode::Off;
    for (Peer& peer : g_peers) peer = Peer();
}

bool Running() { return g_socket != INVALID_SOCKET; }
bool Connected() { return CountConnected() > 0; }
bool Connected(uint8_t player) {
    const Peer* peer = PeerOf(player);
    return peer && peer->connected;
}
int ConnectedCount() { return CountConnected(); }
bool Searching() { return g_discover && !g_host.known; }
cfg::Mode CurrentMode() { return g_mode; }
uint8_t LocalPlayerId() { return g_mode == cfg::Mode::Host ? proto::kHostPlayerId : g_localId; }

void Status(char* out, int capacity) {
    if (g_mode == cfg::Mode::Host) {
        char clients[16] = "";
        for (const Peer& peer : g_peers) {
            if (!peer.connected) continue;
            size_t length = strlen(clients);
            snprintf(clients + length, sizeof(clients) - length, " %u", PlayerOf(peer));
        }
        snprintf(out, capacity, "mode=host joueurs=%d/%d clients=%s", 1 + CountConnected(), proto::kMaxPlayers,
                 clients[0] ? clients + 1 : "aucun");
        return;
    }
    char host[32] = "-";
    if (g_host.known) AddressText(g_host.address, host, sizeof(host));
    snprintf(out, capacity, "mode=%s joueur=%u hote=%s %s", cfg::ModeName(g_mode), g_localId, host,
             g_host.connected ? "connecte" : (Searching() ? "recherche d'un hote" : (Running() ? "en attente" : "arrete")));
}

// -------------------------------------------------------------------------------------------------
// Recherche automatique et refus
// -------------------------------------------------------------------------------------------------
// Client : l'hôte visé ne répond plus, ou nous a refusés. On écoute de nouveau les annonces, en
// laissant cet hôte de côté un moment s'il a dit non.
static void DropTarget(double pause) {
    if (!g_discover) return;
    g_ignoredHost = pause > 0.0 ? g_targetHost : 0;
    g_ignoreUntil = g_now + pause;
    g_host.known = false;
    g_targetHost = 0;
}

static void RejectHello(const sockaddr_in& from, const char* data, int size, uint8_t reason) {
    proto::Reject reject = {0, reason, proto::kVersion};
    proto::Hello hello;
    if (size >= static_cast<int>(sizeof(proto::Header) + sizeof(hello))) {
        memcpy(&hello, data + sizeof(proto::Header), sizeof(hello));
        reject.session = hello.session;
    }
    SendRaw(from, proto::kReject, g_lastLevelHash, &reject, sizeof(reject));
}

// Client : l'hôte que nous visons dit pourquoi il ne nous prend pas.
static void HandleReject(const sockaddr_in& from, const char* data, int size, const Handlers& handlers) {
    proto::Reject reject;
    if (g_host.connected || !g_host.known || !SameAddress(from, g_host.address)) return;
    if (size < static_cast<int>(sizeof(proto::Header) + sizeof(reject))) return;
    memcpy(&reject, data + sizeof(proto::Header), sizeof(reject));
    if (reject.session != g_host.session) return;

    if (g_rejectNoticed != reject.reason) {
        g_rejectNoticed = reject.reason;
        if (reject.reason == proto::kRejectFull) {
            Notice(&handlers, "refuse par l'hote : la partie est complete");
        } else {
            Notice(&handlers, "refuse par l'hote : version differente (protocole %u chez lui, %u ici)",
                   reject.hostVersion, proto::kVersion);
        }
    }
    DropTarget(kRejectedPause);
}

// Client en recherche : annonce d'un hôte. Le premier dont la partie nous est ouverte devient
// celui que nous appelons.
static void HandleBeacon(const sockaddr_in& from, const char* data, int size, double now, const Handlers& handlers) {
    proto::Header header;
    proto::Beacon beacon;
    if (size < static_cast<int>(sizeof(header) + sizeof(beacon))) return;
    memcpy(&header, data, sizeof(header));
    if (header.magic != proto::kMagic || header.type != proto::kBeacon) return;
    if (!InScope(from.sin_addr.s_addr)) return;
    memcpy(&beacon, data + sizeof(header), sizeof(beacon));
    beacon.name[sizeof(beacon.name) - 1] = '\0';
    beacon.modVersion[sizeof(beacon.modVersion) - 1] = '\0';
    beacon.level[sizeof(beacon.level) - 1] = '\0';

    if (g_host.known) return;  // un hôte à la fois : celui que nous appelons, ou avec qui nous jouons
    if (beacon.hostId == g_ignoredHost && now < g_ignoreUntil) return;
    if (!g_joinName.empty() && g_joinName != beacon.name) return;

    char address[24];
    IpText(from.sin_addr.s_addr, address, sizeof(address));
    uint8_t obstacle = header.version != proto::kVersion ? proto::kRejectVersion
                       : (beacon.state != proto::kBeaconWaiting ? proto::kRejectFull : 0);
    if (obstacle) {
        // Une fois par hôte et par raison : l'annonce revient toutes les deux secondes.
        if (g_noticedHost == beacon.hostId && g_noticedReason == obstacle) return;
        g_noticedHost = beacon.hostId;
        g_noticedReason = obstacle;
        if (obstacle == proto::kRejectFull) {
            Notice(&handlers, "partie '%s' trouvee a %s, mais complete (%u joueurs)", beacon.name, address,
                   beacon.players);
        } else {
            Notice(&handlers, "partie '%s' trouvee a %s, mais en OniMP %s (protocole %u, ici %u)", beacon.name, address,
                   beacon.modVersion, header.version, proto::kVersion);
        }
        return;
    }

    g_host.address = from;
    g_host.address.sin_port = htons(beacon.port);
    g_host.known = true;
    g_targetHost = beacon.hostId;
    g_targetSince = now;
    g_noticedHost = 0;
    g_rejectNoticed = 0;
    g_nextHello = 0.0;
    NewSession();
    Notice(&handlers, "partie '%s' trouvee a %s:%u (niveau '%s', %u joueur(s) sur %u) : connexion", beacon.name,
           address, beacon.port, beacon.level, beacon.players, beacon.maxPlayers);
}

static bool EventAlreadySeen(Peer& peer, uint16_t eventId) {
    int count = peer.seenEventCount < 64 ? peer.seenEventCount : 64;
    for (int i = 0; i < count; i++) {
        if (peer.seenEvents[i] == eventId) return true;
    }
    peer.seenEvents[peer.seenEventCount % 64] = eventId;
    peer.seenEventCount++;
    return false;
}

// Paquets non fiables : on écarte ceux arrivés en retard. Vrai si cet état du joueur source est
// plus récent que le dernier que cette liaison a apporté.
static bool NewerPlayerState(Peer& peer, uint32_t source, uint16_t seq, double now) {
    // Le numéro est celui du datagramme : chez un client c'est le compteur de l'hôte, qui avance
    // pour tout ce qu'il envoie, à tous. Il ne départage donc que des états rapprochés. Après un
    // silence de ce joueur (parti puis revenu, ou un autre à sa place), l'ancien numéro ne veut plus
    // rien dire, et le garder ferait écarter ses états le temps d'un demi-tour du compteur.
    bool recent = peer.havePlayerSeq[source] && now - peer.lastPlayerTime[source] < kPlayerSeqMemory;
    if (recent && static_cast<int16_t>(seq - peer.lastPlayerSeq[source]) <= 0) return false;
    peer.lastPlayerSeq[source] = seq;
    peer.lastPlayerTime[source] = now;
    peer.havePlayerSeq[source] = true;
    return true;
}

// Hôte : l'état d'un client part vers les autres clients, avec le niveau où ce client se trouve.
static void RelayPlayer(const Peer& from, const proto::EntityState& state, uint32_t levelHash) {
    for (const Peer& peer : g_peers) {
        if (&peer != &from && peer.connected) SendTo(peer, proto::kPlayer, levelHash, &state, sizeof(state));
    }
}

static bool Enqueue(Peer& peer, uint8_t origin, const void* payload, int size);

// Hôte : ce message d'un client concerne les autres joueurs ; il part vers les autres clients, et
// ce client en reste l'auteur.
static void RelayReliable(const Peer& from, const void* payload, int size) {
    for (Peer& peer : g_peers) {
        if (&peer != &from) Enqueue(peer, PlayerOf(from), payload, size);
    }
}

// Hôte : un client appelle. Une place lui est donnée s'il en reste une.
static void HandleHello(const sockaddr_in& from, const char* data, int size, double now, const Handlers& handlers) {
    proto::Hello hello;
    if (size < static_cast<int>(sizeof(proto::Header) + sizeof(hello))) return;
    memcpy(&hello, data + sizeof(proto::Header), sizeof(hello));
    if (!InScope(from.sin_addr.s_addr)) {
        if (!g_outOfScopeLogged) {
            char text[32];
            AddressText(from, text, sizeof(text));
            LOG("reseau : demande de %s ignoree, hors du reseau choisi", text);
        }
        g_outOfScopeLogged = true;
        return;
    }

    Peer* peer = FindPeer(from);
    if (peer) {
        // Même client, autre connexion : il nous a perdus (chargement de niveau trop long) sans
        // que nous le perdions. Il est reparti de zéro, nous devons en faire autant, sinon les
        // numéros du flux fiable ne correspondent plus et tout s'y perd. Il garde sa place.
        if (peer->connected && hello.session != peer->session) SetConnected(*peer, false, &handlers);
    } else {
        for (Peer& candidate : g_peers) {
            if (!candidate.known) {
                peer = &candidate;
                break;
            }
        }
        if (!peer) {
            RejectHello(from, data, size, proto::kRejectFull);
            return;
        }
    }
    peer->address = from;
    peer->known = true;
    peer->session = hello.session;
    peer->lastReceive = now;
    proto::Welcome welcome = {hello.session, PlayerOf(*peer), static_cast<uint8_t>(proto::kMaxPlayers)};
    SendTo(*peer, proto::kWelcome, g_lastLevelHash, &welcome, sizeof(welcome));
    SetConnected(*peer, true, &handlers);
}

static void HandlePacket(const sockaddr_in& from, const char* data, int size, double now,
                         const Handlers& handlers) {
    proto::Header header;
    memcpy(&header, data, sizeof(header));
    const char* payload = data + sizeof(header);
    int payloadSize = size - static_cast<int>(sizeof(header));
    bool host = g_mode == cfg::Mode::Host;

    if (host && header.type == proto::kHello) {
        // En pause, qui arrive attend : le client rappelle toutes les demi-secondes.
        if (!g_deferring) HandleHello(from, data, size, now, handlers);
        return;
    }
    Peer* peer = FindPeer(from);
    if (!peer) return;
    if (!host) {
        if (header.type == proto::kReject) {
            HandleReject(from, data, size, handlers);
            return;
        }
        if (header.type == proto::kWelcome) {
            // Une réponse à une tentative antérieure ne vaut plus rien.
            proto::Welcome welcome = {};
            if (payloadSize >= static_cast<int>(sizeof(welcome))) memcpy(&welcome, payload, sizeof(welcome));
            if (welcome.session != peer->session) return;
            if (welcome.player < 1 || welcome.player >= proto::kMaxPlayers) return;
            g_localId = welcome.player;
            peer->lastReceive = now;
            SetConnected(*peer, true, &handlers);
            return;
        }
    }
    if (!peer->connected) return;
    peer->lastReceive = now;

    switch (header.type) {
        case proto::kPlayer: {
            if (payloadSize < static_cast<int>(sizeof(proto::EntityState))) return;
            proto::EntityState state;
            memcpy(&state, payload, sizeof(state));
            // Un client ne parle que de son joueur ; l'hôte apporte le sien et ceux des autres clients.
            uint32_t source = host ? PlayerOf(*peer) : state.characterID;
            if (source >= static_cast<uint32_t>(proto::kMaxPlayers) || source == LocalPlayerId()) return;
            if (!NewerPlayerState(*peer, source, header.seq, now)) return;
            state.characterID = source;
            if (g_deferring) {
                peer->heldPlayer = true;
                peer->heldState = state;
                peer->heldLevel = header.levelHash;
            } else if (handlers.onPlayer) {
                handlers.onPlayer(state, header.levelHash);
            }
            if (host) RelayPlayer(*peer, state, header.levelHash);
            break;
        }
        case proto::kNpcSnapshot: {
            int count = payloadSize / static_cast<int>(sizeof(proto::EntityState));
            if (host || count < 0 || count > proto::kMaxEntitiesPerPacket) return;
            proto::EntityState states[proto::kMaxEntitiesPerPacket];
            if (count > 0) memcpy(states, payload, count * sizeof(proto::EntityState));
            if (handlers.onNpcSnapshot) handlers.onNpcSnapshot(states, count, header.levelHash);
            break;
        }
        case proto::kNpcInfo: {
            int count = payloadSize / static_cast<int>(sizeof(proto::NpcInfo));
            if (host || count <= 0 || count > proto::kMaxInfosPerPacket) return;
            proto::NpcInfo infos[proto::kMaxInfosPerPacket];
            memcpy(infos, payload, count * sizeof(proto::NpcInfo));
            for (int i = 0; i < count; i++) infos[i].name[sizeof(infos[i].name) - 1] = '\0';
            if (handlers.onNpcInfo) handlers.onNpcInfo(infos, count);
            break;
        }
        case proto::kEvent: {
            if (payloadSize < static_cast<int>(sizeof(proto::Event))) return;
            proto::Event event;
            memcpy(&event, payload, sizeof(event));
            proto::Ack ack = {event.eventId};
            SendTo(*peer, proto::kAck, g_lastLevelHash, &ack, sizeof(ack));
            if (EventAlreadySeen(*peer, event.eventId)) break;
            if (g_deferring) peer->held.push_back({true, header.levelHash, event, std::string()});
            else if (handlers.onEvent) handlers.onEvent(PlayerOf(*peer), event, header.levelHash);
            break;
        }
        case proto::kAck: {
            if (payloadSize < static_cast<int>(sizeof(proto::Ack))) return;
            proto::Ack ack;
            memcpy(&ack, payload, sizeof(ack));
            for (size_t i = 0; i < peer->pending.size(); i++) {
                if (peer->pending[i].event.eventId == ack.eventId) {
                    peer->pending.erase(peer->pending.begin() + static_cast<long>(i));
                    break;
                }
            }
            break;
        }
        case proto::kBye:
            // En pause, un départ se constatera au silence du client, une fois Poll revenu.
            if (!g_deferring) SetConnected(*peer, false, &handlers);
            break;
        case proto::kStream: {
            if (payloadSize < static_cast<int>(sizeof(proto::StreamHeader))) return;
            proto::StreamHeader stream;
            memcpy(&stream, payload, sizeof(stream));
            // Seul le message attendu est livré ; les autres seront renvoyés par l'émetteur.
            if (stream.seq == peer->streamExpected) {
                peer->streamExpected++;
                // Un client est l'auteur de ce qu'il envoie ; l'hôte dit de qui vient ce qu'il remet.
                uint8_t origin = host ? PlayerOf(*peer) : stream.origin;
                const uint8_t* message = reinterpret_cast<const uint8_t*>(payload) + sizeof(stream);
                int messageSize = payloadSize - static_cast<int>(sizeof(stream));
                // Un message trop long ou d'auteur impossible ne vient pas d'un OniMP : accusé, pour
                // que son émetteur passe à la suite, mais écarté.
                if (origin < proto::kMaxPlayers && messageSize <= proto::kMaxStreamPayload) {
                    // Aux autres clients d'abord : ce que l'hôte en fera lui-même, les commandes du
                    // script d'une console par exemple, doit leur arriver après sa cause.
                    if (host && messageSize > 0 && handlers.shared && handlers.shared(message[0])) {
                        RelayReliable(*peer, message, messageSize);
                    }
                    if (g_deferring) {
                        peer->held.push_back({false, header.levelHash, proto::Event(),
                                              std::string(reinterpret_cast<const char*>(message),
                                                          static_cast<size_t>(messageSize))});
                    } else if (handlers.onReliable) {
                        handlers.onReliable(origin, message, messageSize);
                    }
                }
            }
            proto::StreamAck ack = {peer->streamExpected};
            SendTo(*peer, proto::kStreamAck, g_lastLevelHash, &ack, sizeof(ack));
            break;
        }
        case proto::kStreamAck: {
            if (payloadSize < static_cast<int>(sizeof(proto::StreamAck))) return;
            proto::StreamAck ack;
            memcpy(&ack, payload, sizeof(ack));
            while (!peer->stream.empty() && peer->stream.front().seq < ack.next) peer->stream.pop_front();
            break;
        }
        case proto::kPing:
            // Renvoyé tel quel : c'est l'émetteur qui compte le temps.
            if (payloadSize >= static_cast<int>(sizeof(proto::Ping))) {
                SendTo(*peer, proto::kPong, g_lastLevelHash, payload, sizeof(proto::Ping));
            }
            break;
        case proto::kPong: {
            if (payloadSize < static_cast<int>(sizeof(proto::Ping))) return;
            proto::Ping ping;
            memcpy(&ping, payload, sizeof(ping));
            // Différence de deux compteurs de 32 bits : juste même quand l'horloge a fait un tour.
            double sample = static_cast<double>(static_cast<uint32_t>(MillisecondStamp(now) - ping.stamp));
            if (sample > 5000.0) break;  // réponse d'une autre époque
            peer->rtt = peer->rtt < 0.0 ? sample : peer->rtt * 0.7 + sample * 0.3;
            break;
        }
        default:
            break;
    }
}

void RequestReconnect() { g_reconnectRequested = true; }

int PingMs(uint8_t player) {
    const Peer* peer = PeerOf(player);
    if (!peer || !peer->connected || peer->rtt < 0.0) return -1;
    return static_cast<int>(peer->rtt + 0.5);
}

// Flux ordonné de chaque liaison : les plus anciens messages d'abord, par petites rafales.
static void FlushStreams(double now, uint32_t levelHash) {
    for (Peer& peer : g_peers) {
        if (!peer.connected) continue;
        int sent = 0;
        for (StreamMessage& message : peer.stream) {
            if (sent >= kStreamBurst) break;
            if (now < message.nextSend) continue;
            message.nextSend = now + kStreamResend;
            char packet[sizeof(proto::StreamHeader) + proto::kMaxStreamPayload];
            proto::StreamHeader stream = {message.seq, message.origin};
            memcpy(packet, &stream, sizeof(stream));
            memcpy(packet + sizeof(stream), message.data.data(), message.data.size());
            SendTo(peer, proto::kStream, levelHash, packet, static_cast<int>(sizeof(stream) + message.data.size()));
            sent++;
        }
    }
}

// Hôte, au sortir d'une pause : ce que PumpRelay a gardé de chaque client est remis à nos
// fonctions, dans l'ordre d'arrivée. La redistribution aux autres clients est déjà faite.
static void DeliverHeld(const Handlers& handlers) {
    for (Peer& peer : g_peers) {
        if (peer.heldPlayer && handlers.onPlayer) handlers.onPlayer(peer.heldState, peer.heldLevel);
        peer.heldPlayer = false;
        while (!peer.held.empty()) {
            Peer::Held held = peer.held.front();
            peer.held.pop_front();
            if (held.isEvent) {
                if (handlers.onEvent) handlers.onEvent(PlayerOf(peer), held.event, held.levelHash);
            } else if (handlers.onReliable) {
                handlers.onReliable(PlayerOf(peer), reinterpret_cast<const uint8_t*>(held.data.data()),
                                    static_cast<int>(held.data.size()));
            }
        }
    }
}

void Poll(double now, uint32_t levelHash, const Handlers& handlers) {
    if (!Running()) return;
    g_lastLevelHash = levelHash;
    g_now = now;
    g_shared = handlers.shared;

    ScanInterfaces(now, &handlers);
    DeliverHeld(handlers);

    if (g_reconnectRequested) {
        g_reconnectRequested = false;
        if (g_mode == cfg::Mode::Client && g_host.connected) {
            SetConnected(g_host, false, &handlers);
            g_nextHello = 0.0;
        }
    }

    char buffer[kDatagramSize + 64];
    for (int i = 0; i < kMaxPacketsPerPoll; i++) {
        sockaddr_in from = {};
        int fromSize = sizeof(from);
        int size = recvfrom(g_socket, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &fromSize);
        if (size == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAECONNRESET || error == WSAEMSGSIZE) continue;
            break;  // WSAEWOULDBLOCK : plus rien à lire
        }
        if (size < static_cast<int>(sizeof(proto::Header))) continue;
        proto::Header header;
        memcpy(&header, buffer, sizeof(header));
        if (header.magic != proto::kMagic) continue;
        if (header.version != proto::kVersion) {
            // Une autre version n'est pas écoutée, sauf pour se le dire : l'hôte répond non à
            // qui l'appelle, et le client entend ce non.
            if (g_mode == cfg::Mode::Host && header.type == proto::kHello) {
                if (InScope(from.sin_addr.s_addr)) RejectHello(from, buffer, size, proto::kRejectVersion);
            } else if (g_mode == cfg::Mode::Client && header.type == proto::kReject) {
                HandleReject(from, buffer, size, handlers);
            }
            continue;
        }
        HandlePacket(from, buffer, size, now, handlers);
    }

    for (Peer& peer : g_peers) {
        if (peer.connected && now - peer.lastReceive > kTimeout) SetConnected(peer, false, &handlers);
    }
    // Latence : un aller-retour par seconde et par liaison. L'horloge part de zéro au lancement : en
    // millisecondes elle tient largement sur 32 bits.
    for (Peer& peer : g_peers) {
        if (!peer.connected) {
            peer.rtt = -1.0;
            continue;
        }
        if (now < peer.nextPing) continue;
        peer.nextPing = now + 1.0;
        proto::Ping ping = {MillisecondStamp(now)};
        SendTo(peer, proto::kPing, levelHash, &ping, sizeof(ping));
    }
    if (g_mode == cfg::Mode::Host) SendBeacons(now, levelHash);

    // Recherche automatique : annonces des hôtes, puis abandon de celui qui ne répond pas.
    if (g_discover) {
        for (int i = 0; i < kMaxPacketsPerPoll; i++) {
            sockaddr_in from = {};
            int fromSize = sizeof(from);
            int size = recvfrom(g_discovery, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &fromSize);
            if (size == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error == WSAECONNRESET || error == WSAEMSGSIZE) continue;
                break;
            }
            HandleBeacon(from, buffer, size, now, handlers);
        }
        if (g_host.known && !g_host.connected && now - g_targetSince > kJoinTimeout) {
            Notice(&handlers, "l'hote ne repond plus : nouvelle recherche");
            DropTarget(0.0);
        }
    }

    if (g_mode == cfg::Mode::Client && !g_host.connected && now >= g_nextHello) {
        g_nextHello = now + kHelloInterval;
        proto::Hello hello = {g_host.session};
        SendTo(g_host, proto::kHello, levelHash, &hello, sizeof(hello));
    }

    for (Peer& peer : g_peers) {
        for (size_t i = 0; i < peer.pending.size();) {
            PendingEvent& pending = peer.pending[i];
            if (now >= pending.nextSend) {
                if (pending.tries >= kMaxResends) {
                    peer.pending.erase(peer.pending.begin() + static_cast<long>(i));
                    continue;
                }
                pending.tries++;
                pending.nextSend = now + kResendInterval;
                SendTo(peer, proto::kEvent, pending.levelHash, &pending.event, sizeof(pending.event));
            }
            i++;
        }
    }

    FlushStreams(now, levelHash);
}

void PumpRelay(double now) {
    if (g_mode != cfg::Mode::Host || !Running()) return;
    // Mêmes traitements que Poll, sauf qu'aucune de nos fonctions n'est appelée : le jeu est
    // arrêté dans son menu, ce n'est pas le moment d'y toucher.
    Handlers relayOnly = {};
    relayOnly.shared = g_shared;
    g_deferring = true;
    char buffer[kDatagramSize + 64];
    for (int i = 0; i < kMaxPacketsPerPoll; i++) {
        sockaddr_in from = {};
        int fromSize = sizeof(from);
        int size = recvfrom(g_socket, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &fromSize);
        if (size == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAECONNRESET || error == WSAEMSGSIZE) continue;
            break;
        }
        proto::Header header;
        if (size < static_cast<int>(sizeof(header))) continue;
        memcpy(&header, buffer, sizeof(header));
        if (header.magic != proto::kMagic || header.version != proto::kVersion) continue;
        HandlePacket(from, buffer, size, now, relayOnly);
    }
    g_deferring = false;

    for (Peer& peer : g_peers) {
        for (PendingEvent& pending : peer.pending) {
            if (now < pending.nextSend || pending.tries >= kMaxResends) continue;
            pending.tries++;
            pending.nextSend = now + kResendInterval;
            SendTo(peer, proto::kEvent, pending.levelHash, &pending.event, sizeof(pending.event));
        }
    }
    FlushStreams(now, g_lastLevelHash);
}

static bool Enqueue(Peer& peer, uint8_t origin, const void* payload, int size) {
    if (!peer.connected || size <= 0 || size > proto::kMaxStreamPayload) return false;
    StreamMessage message;
    message.seq = peer.streamNextSeq++;
    message.origin = origin;
    message.data.assign(static_cast<const char*>(payload), static_cast<size_t>(size));
    message.nextSend = 0.0;  // premier envoi au prochain Poll
    peer.stream.push_back(message);
    return true;
}

bool SendReliable(const void* payload, int size) {
    if (size <= 0 || size > proto::kMaxStreamPayload) return false;
    bool queued = false;
    for (Peer& peer : g_peers) queued = Enqueue(peer, LocalPlayerId(), payload, size) || queued;
    return queued;
}

bool SendReliableTo(uint8_t player, const void* payload, int size) {
    Peer* peer = PeerOf(player);
    if (!peer || size <= 0 || size > proto::kMaxStreamPayload) return false;
    return Enqueue(*peer, LocalPlayerId(), payload, size);
}

int ReliableBacklog() {
    size_t backlog = 0;
    for (const Peer& peer : g_peers) backlog += peer.stream.size();
    return static_cast<int>(backlog);
}

void SendPlayer(const proto::EntityState& state, uint32_t levelHash) {
    SendToAll(proto::kPlayer, levelHash, &state, sizeof(state));
}

void SendNpcSnapshot(const proto::EntityState* states, int count, uint32_t levelHash) {
    // Aucun PNJ : un relevé vide, qui le dit. Sans lui un client ne distinguait pas « l'hôte n'a
    // plus de PNJ » de « plus de nouvelles de l'hôte », et gardait ceux que l'hôte venait de retirer.
    if (count <= 0) SendToAll(proto::kNpcSnapshot, levelHash, nullptr, 0);
    for (int offset = 0; offset < count; offset += proto::kMaxEntitiesPerPacket) {
        int chunk = count - offset;
        if (chunk > proto::kMaxEntitiesPerPacket) chunk = proto::kMaxEntitiesPerPacket;
        SendToAll(proto::kNpcSnapshot, levelHash, states + offset, chunk * static_cast<int>(sizeof(proto::EntityState)));
    }
}

void SendNpcInfo(const proto::NpcInfo* infos, int count, uint32_t levelHash) {
    for (int offset = 0; offset < count; offset += proto::kMaxInfosPerPacket) {
        int chunk = count - offset;
        if (chunk > proto::kMaxInfosPerPacket) chunk = proto::kMaxInfosPerPacket;
        SendToAll(proto::kNpcInfo, levelHash, infos + offset, chunk * static_cast<int>(sizeof(proto::NpcInfo)));
    }
}

void SendEvent(uint8_t kind, uint32_t characterID, int32_t value, uint32_t levelHash) {
    PendingEvent pending = {};
    pending.event.kind = kind;
    pending.event.characterID = characterID;
    pending.event.value = value;
    pending.levelHash = levelHash;
    pending.nextSend = 0.0;  // premier envoi au prochain Poll
    pending.tries = 0;
    for (Peer& peer : g_peers) {
        if (!peer.connected) continue;
        pending.event.eventId = g_nextEventId++;
        if (g_nextEventId == 0) g_nextEventId = 1;
        peer.pending.push_back(pending);
    }
}

}  // namespace net
