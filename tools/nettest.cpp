// Test du module réseau sans le jeu : plusieurs processus sur la boucle locale.
//   onimp_nettest host [port]            puis            onimp_nettest client [ip[:port]]
// Chaque côté envoie un joueur qui tourne en rond, l'hôte ajoute 3 PNJ, le client envoie un
// événement de dégâts fiable. Se termine après quelques secondes avec le code 0 si tout a circulé.
//   onimp_nettest host 27801 stall       puis            onimp_nettest client 127.0.0.1:27801 stall
// Même chose, mais l'hôte se fige plusieurs secondes, comme pendant un chargement de niveau : le
// client le croit parti et se reconnecte, alors que l'hôte n'a rien vu. Le flux fiable doit
// repartir de zéro des deux côtés.
//   onimp_nettest host 27801 resync      puis            onimp_nettest client 127.0.0.1:27801 resync
// Le client demande lui-même une nouvelle connexion, comme après avoir rechargé son niveau :
// l'hôte doit le traiter en nouveau venu et lui renvoyer sa série.
//   onimp_nettest host 27801             puis            onimp_nettest client auto:27801
// Le client ne connaît pas l'adresse de l'hôte : il attend son annonce et le rejoint.
//   onimp_nettest host 27801 radmin      puis            onimp_nettest client auto:27801 radmin
// Même chose par Radmin VPN seulement (26.x.x.x) ; échoue s'il n'est pas connecté.
//   onimp_nettest host 27801 multi3      puis, trois fois, onimp_nettest client 127.0.0.1:27801 multi3
// Partie à quatre (multi2 : à trois). Chaque client doit recevoir de l'hôte l'état des autres
// clients et leurs messages fiables, avec le numéro de leur auteur. L'hôte cesse un moment
// d'interroger le réseau, comme dans son menu de pause : les clients doivent continuer de se voir
// et de recevoir les messages les uns des autres, et l'hôte retrouver à sa reprise ce qui lui
// était destiné.
//   ... pendant que les trois clients sont là :          onimp_nettest client 127.0.0.1:27801 full
// Un client de plus doit être refusé, et apprendre que la partie est complète.
#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "../src/config.h"
#include "../src/network.h"

static const uint32_t kLevel = 0xD3CAB9F7;
static const int kHostReliableCount = 300;
static const int kClientReliableCount = 20;
static const char* const kPauseText = "pause";
static const char* const kResumeText = "reprise";
static const char* const kLateText = "tardif";   // envoyé par chaque client pendant la pause de l'hôte

static int g_players = 0;
static int g_snapshots = 0;
static int g_infos = 0;
static int g_events = 0;
static int g_connections = 0;
static proto::EntityState g_lastPlayer = {};

// Par numéro de joueur : ce qui est venu de lui.
struct Source {
    int states = 0;
    double firstState = 0.0;
    double lastState = 0.0;
    double longestGap = 0.0;
    int reliable = 0;             // messages de sa série reçus
    bool inOrder = true;
    int events = 0;
    int late = 0;                 // messages kLateText reçus de lui
    double lateAt = 0.0;
};
static Source g_sources[proto::kMaxPlayers];
static double g_pauseAt = 0.0, g_resumeAt = 0.0;  // client : arrivée des deux messages de l'hôte
static bool g_deferSeries = false;                // client d'une partie à plusieurs : série envoyée plus tard
static bool g_refusedFull = false;
static double g_clock = 0.0;

static double Now() {
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart) / static_cast<double>(frequency.QuadPart);
}

static void SendSeries(int target) {
    int count = net::CurrentMode() == cfg::Mode::Host ? kHostReliableCount : kClientReliableCount;
    for (int i = 0; i < count; i++) {
        char text[32];
        int length = snprintf(text, sizeof(text), "message %d", i);
        if (target < 0) net::SendReliable(text, length);
        else net::SendReliableTo(static_cast<uint8_t>(target), text, length);
    }
}

static void OnPlayer(const proto::EntityState& state, uint32_t) {
    g_players++;
    g_lastPlayer = state;
    if (state.characterID >= static_cast<uint32_t>(proto::kMaxPlayers)) return;
    Source& source = g_sources[state.characterID];
    if (source.states == 0) source.firstState = g_clock;
    else if (g_clock - source.lastState > source.longestGap) source.longestGap = g_clock - source.lastState;
    source.lastState = g_clock;
    source.states++;
}
static void OnSnapshot(const proto::EntityState*, int count, uint32_t) { g_snapshots += count; }
static void OnInfo(const proto::NpcInfo*, int count) { g_infos += count; }
static void OnEvent(uint8_t from, const proto::Event& event, uint32_t) {
    g_events++;
    if (from < proto::kMaxPlayers) g_sources[from].events++;
    printf("evenement %u du joueur %u : type %u, personnage %08X, valeur %d\n", event.eventId, from, event.kind,
           static_cast<unsigned>(event.characterID), event.value);
}

static void OnConnection(uint8_t player, bool connected) {
    printf("joueur %u : %s\n", player, connected ? "pair connecte" : "pair perdu");
    if (!connected || player >= proto::kMaxPlayers) return;
    g_connections++;
    // Chaque connexion repart d'un flux vierge : la série attendue de ce pair recommence. Chez un
    // client, tout ce que l'hôte apportait des autres aussi.
    if (net::CurrentMode() == cfg::Mode::Host) {
        g_sources[player].reliable = 0;
        g_sources[player].inOrder = true;
    } else {
        for (Source& source : g_sources) {
            source.reliable = 0;
            source.inOrder = true;
        }
    }
    // L'hôte pousse d'un coup une longue série au client qui arrive, comme un journal de niveau,
    // et à lui seul ; le client une courte, comme ses annonces d'armes ou de projections.
    if (net::CurrentMode() == cfg::Mode::Host) SendSeries(player);
    else if (!g_deferSeries) SendSeries(-1);
}

static void OnNotice(const char* text) {
    printf("avis : %s\n", text);
    if (strstr(text, "complete")) g_refusedFull = true;
}

static bool Is(const uint8_t* payload, int size, const char* text) {
    return size == static_cast<int>(strlen(text)) && memcmp(payload, text, static_cast<size_t>(size)) == 0;
}

static void OnReliable(uint8_t from, const uint8_t* payload, int size) {
    if (from >= proto::kMaxPlayers) return;
    Source& source = g_sources[from];
    if (net::CurrentMode() == cfg::Mode::Client && from == proto::kHostPlayerId) {
        if (Is(payload, size, kPauseText)) {
            g_pauseAt = g_clock;
            return;
        }
        if (Is(payload, size, kResumeText)) {
            g_resumeAt = g_clock;
            return;
        }
    }
    if (Is(payload, size, kLateText)) {
        source.late++;
        source.lateAt = g_clock;
        return;
    }
    char expected[32];
    int length = snprintf(expected, sizeof(expected), "message %d", source.reliable);
    if (size != length || memcmp(payload, expected, static_cast<size_t>(length)) != 0) source.inOrder = false;
    source.reliable++;
}

// Comme le jeu pour ce qu'un joueur adresse aux autres : l'hôte redistribue tout ce qu'un client
// lui envoie par le flux fiable.
static bool Shared(uint8_t) { return true; }

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc < 2) {
        printf("usage : onimp_nettest host [port] [mode] | client [ip[:port]] [mode]\n");
        return 2;
    }

    cfg::Config config;
    std::string spec = argv[1];
    if (spec == "client") spec = std::string("connect:") + (argc > 2 ? argv[2] : "127.0.0.1:27777");
    else if (argc > 2) spec += std::string(":") + argv[2];
    if (!cfg::ParseSpec(spec.c_str(), config) || config.mode == cfg::Mode::Off) {
        printf("argument invalide : %s\n", spec.c_str());
        return 2;
    }
    bool isHost = config.mode == cfg::Mode::Host;
    const char* mode = argc > 3 ? argv[3] : "";
    bool stall = strcmp(mode, "stall") == 0;
    bool resync = strcmp(mode, "resync") == 0;
    bool full = strcmp(mode, "full") == 0;
    bool radmin = strcmp(mode, "radmin") == 0;
    int clients = strcmp(mode, "multi2") == 0 ? 2 : (strcmp(mode, "multi3") == 0 ? 3 : 1);  // clients de la partie
    bool multi = clients > 1;
    bool search = config.host == net::kAutoHost;
    g_deferSeries = multi && !isHost;
    net::Configure("nettest", "", "test", radmin ? "radmin" : "auto");
    net::SetLevelName("niveau_test");
    if (!net::Start(config.mode, config.host.c_str(), config.port)) {
        printf("demarrage reseau impossible\n");
        return 1;
    }

    const net::Handlers handlers = {OnPlayer, OnSnapshot, OnInfo, OnEvent, OnConnection, OnReliable, OnNotice, Shared};
    double start = Now();
    double duration = stall ? (isHost ? 16.0 : 13.0) : (isHost ? 11.0 : (resync ? 6.0 : (search ? 7.0 : 4.0)));
    if (multi) duration = isHost ? 15.0 : 12.0;
    if (full) duration = 2.0;  // doit finir avant les clients en place : une place libre, il serait accepte
    bool eventSent = false;
    bool stalled = false;
    bool resynced = false;
    bool seriesSent = false;
    bool lateSent = false;
    double pauseUntil = 0.0;   // hôte d'une partie à plusieurs : fin de sa pause, 0 avant
    bool paused = false;
    int mostClients = 0;
    int tick = 0;

    while (Now() - start < duration) {
        double now = Now();
        g_clock = now;
        if (stall && isHost && !stalled && net::Connected() && now - start > 3.0) {
            stalled = true;
            printf("hote fige 6,5 s\n");
            Sleep(6500);
            now = Now();
        }
        if (resync && !isHost && !resynced && net::Connected() && now - start > 2.0) {
            resynced = true;
            printf("client : nouvelle connexion demandee\n");
            net::RequestReconnect();
        }
        if (multi && isHost && pauseUntil == 0.0 && net::ConnectedCount() == clients && now - start > 6.0) {
            printf("hote : pause de 3 s, le reseau n'est plus interroge\n");
            net::SendReliable(kPauseText, static_cast<int>(strlen(kPauseText)));
            pauseUntil = now + 3.0;
            paused = true;
        }
        if (paused && now >= pauseUntil) {
            paused = false;
            net::SendReliable(kResumeText, static_cast<int>(strlen(kResumeText)));
            printf("hote : reprise\n");
        }
        // En pause, comme le menu du jeu : plus de Poll, seulement le relais et, plus bas, notre joueur.
        if (paused) net::PumpRelay(now);
        else net::Poll(now, kLevel, handlers);
        if (net::ConnectedCount() > mostClients) mostClients = net::ConnectedCount();

        float angle = static_cast<float>(now - start);
        proto::EntityState player = {};
        player.x = 100.0f * std::cos(angle);
        player.z = 100.0f * std::sin(angle);
        player.rotation = angle;
        player.characterID = net::LocalPlayerId();
        player.health = 200;
        player.flags = proto::kEntityActive;
        player.tick = static_cast<uint16_t>(tick * 2);
        net::SendPlayer(player, kLevel);

        if (isHost && !paused && tick % 2 == 0) {
            proto::EntityState npcs[3] = {};
            proto::NpcInfo infos[3] = {};
            for (int i = 0; i < 3; i++) {
                npcs[i].characterID = 0x1000u + static_cast<uint32_t>(i);
                npcs[i].health = 100;
                infos[i].characterID = npcs[i].characterID;
                snprintf(infos[i].name, sizeof(infos[i].name), "pnj_%d", i);
            }
            if (tick % 60 == 0) net::SendNpcInfo(infos, 3, kLevel);
            net::SendNpcSnapshot(npcs, 3, kLevel);
        }
        if (!isHost && net::Connected() && !eventSent && now - start > 1.0) {
            eventSent = true;
            net::SendEvent(proto::kEventDamage, 0x1001, 25, kLevel);
        }
        // Partie à plusieurs : la série part quand tous les clients sont là, pour que chacun la reçoive.
        if (g_deferSeries && !seriesSent && net::Connected() && now - start > 3.0) {
            seriesSent = true;
            SendSeries(-1);
        }
        // Et pendant la pause de l'hôte, un message pour les autres et un second événement pour lui.
        if (g_deferSeries && !lateSent && net::Connected() && now - start > 7.0) {
            lateSent = true;
            net::SendReliable(kLateText, static_cast<int>(strlen(kLateText)));
            net::SendEvent(proto::kEventDamage, 0x1002, 10, kLevel);
        }

        tick++;
        Sleep(33);
    }

    char status[96];
    net::Status(status, sizeof(status));
    printf("%s | connexions=%d joueurs=%d pnj=%d noms=%d evenements=%d reste=%d | dernier joueur id=%u (%.1f, %.1f)\n",
           status, g_connections, g_players, g_snapshots, g_infos, g_events, net::ReliableBacklog(),
           static_cast<unsigned>(g_lastPlayer.characterID), g_lastPlayer.x, g_lastPlayer.z);
    for (int id = 0; id < proto::kMaxPlayers; id++) {
        const Source& source = g_sources[id];
        if (!source.states && !source.reliable) continue;
        printf("  joueur %d : %d etats (trou le plus long %.2f s), %d fiables (ordre %s), %d evenement(s), %d tardif(s)\n",
               id, source.states, source.longestGap, source.reliable, source.inOrder ? "ok" : "FAUX", source.events,
               source.late);
    }
    uint8_t local = net::LocalPlayerId();
    bool emptied = net::ReliableBacklog() == 0;
    net::Stop();

    bool ok = emptied && g_players > 30;
    if (isHost) {
        // Chaque client : une place, ses états, sa série entière et dans l'ordre, son événement.
        // Partie à plusieurs : en plus, ce qu'il a envoyé pendant notre pause, retrouvé à la reprise.
        int perClient = multi ? 2 : 1;
        ok = ok && mostClients == clients && g_connections == (stall || resync ? 2 : clients);
        ok = ok && g_events == clients * perClient;
        for (int id = 1; id <= clients; id++) {
            const Source& source = g_sources[id];
            ok = ok && source.states > 30 && source.reliable == kClientReliableCount && source.inOrder;
            ok = ok && source.events == perClient && source.late == (multi ? 1 : 0);
        }
    } else {
        const Source& host = g_sources[proto::kHostPlayerId];
        ok = ok && g_connections == (stall || resync ? 2 : 1) && g_snapshots > 30 && g_infos >= 3;
        ok = ok && host.states > 30 && host.reliable == kHostReliableCount && host.inOrder;
        ok = ok && local >= 1 && local <= clients;
        // Partie à plusieurs : chaque autre client vu sans interruption, pause de l'hôte comprise,
        // et sa série reçue par l'hôte, à son nom. Les messages envoyés par l'hôte pendant sa pause
        // sont arrivés avant qu'elle ne finisse.
        for (int id = 1; multi && id <= clients; id++) {
            if (id == local) continue;
            const Source& other = g_sources[id];
            ok = ok && other.states > 100 && other.longestGap < 1.0;
            ok = ok && other.reliable == kClientReliableCount && other.inOrder;
            // Son message envoyé pendant la pause de l'hôte nous est parvenu avant qu'elle ne finisse.
            ok = ok && other.late == 1 && other.lateAt > g_pauseAt && other.lateAt < g_resumeAt;
        }
        if (multi) {
            printf("  messages de pause : %.2f s entre les deux\n", g_resumeAt - g_pauseAt);
            ok = ok && g_pauseAt > 0.0 && g_resumeAt - g_pauseAt > 2.0;
        }
    }
    // Client de trop : il doit avoir ete refuse, et le savoir.
    if (full) ok = g_connections == 0 && g_refusedFull;
    printf("%s\n", ok ? "OK" : "ECHEC");
    return ok ? 0 : 1;
}
