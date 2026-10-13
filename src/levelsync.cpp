#include "levelsync.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "clock.h"
#include "config.h"
#include "log.h"
#include "memory.h"
#include "network.h"
#include "oni.h"
#include "protocol.h"

using namespace oni;

namespace level {

static const double kRestartDelay = 2.5;  // laisse voir sa mort au joueur du client avant de recharger
static const double kFailDelay = 1.5;     // mission échouée : le temps de lire pourquoi
static const double kJoinDelay = 5.0;     // un joueur arrive : compte à rebours avant la reprise commune
// Niveau « tout juste chargé » : dix secondes de jeu. Deux machines qui en sont là, parties du même
// point de sauvegarde, sont au même endroit du niveau sans rien avoir à recharger.
static const int32_t kFreshTicks = 600;
static const int32_t kWipeTicks = 120;    // le script de reprise a rendu son inventaire au joueur
static const double kWipeGrace = 2.0;     // sans reprise annoncée d'ici là, le nouveau venu garde son niveau
static const double kWaitTimeout = 25.0;  // attente des autres après un chargement, au plus
static const double kGoneTimeout = 10.0;  // un joueur attendu resté injoignable ce temps-là ne l'est plus
static const double kWaitNote = 5.0;      // d'un rappel à l'écran au suivant

// Hôte : numéro du chargement de niveau en cours. La base change d'un lancement à l'autre, pour
// qu'un hôte relancé ne soit pas pris pour l'ancien.
static uint32_t g_instance = 0;
static double g_restartAt = 0.0;          // 0 : aucune reprise demandée
// Compte à rebours dit à l'écran avant une reprise (arrivée d'un joueur), chez tous.
static double g_countdownEnd = 0.0;
static int g_countdownSaid = 0;           // dernière seconde annoncée, 0 : pas de compte à rebours

// Client
static uint32_t g_synced = 0;             // chargement de l'hôte auquel notre niveau correspond, 0 : inconnu
static uint32_t g_pending = 0;            // chargement de l'hôte que nous sommes en train de rejoindre
static bool g_deathReported = false;
static bool g_joined = false;             // notre arrivée dans la partie est dite à l'hôte
static bool g_wipe = false;               // nous venons d'arriver : notre inventaire est à vider
static double g_wipeNotBefore = 0.0;

// Attente après un chargement : le jeu est en pause tant que tous n'ont pas chargé.
static bool g_waiting = false;
static double g_waitUntil = 0.0;
static double g_nextWaitNote = 0.0;
static uint8_t g_expected = 0;            // hôte : clients attendus, un bit par numéro de joueur
static uint8_t g_ready = 0;               // hôte : clients qui ont chargé ce niveau-ci
static double g_goneSince[proto::kMaxPlayers] = {};  // hôte : depuis quand un client attendu ne répond plus
static double g_hostSilentSince = 0.0;    // client : depuis quand l'hôte ne répond plus

// Écran d'échec affiché par la boucle de jeu avant de recharger un niveau perdu. Il dure jusqu'à
// vingt secondes si personne n'appuie sur une touche, et rien ne tourne pendant ce temps : le
// réseau se tait, les autres joueurs attendent. À plusieurs, on s'en passe.
typedef void(ONICALL* SplashFunction)(const char* splash, const char* music, uint32_t stopSounds);
static SplashFunction g_previousSplash = nullptr;

static void ONICALL ONIMP_ENTRY LoseSplashHook(const char* splash, const char* music, uint32_t stopSounds) {
    // Un nom sans image : le moteur prend alors lui-même son chemin « pas d'écran ».
    if (net::Connected()) g_previousSplash("", music, stopSounds);
    else g_previousSplash(splash, music, stopSounds);
}

// Deux autres écrans du même genre : celui que le moteur montre à la fin de chaque chargement de
// niveau, sauvegarde reprise comprise (« intro_splash_screen », 0x5068BE), et celui de la fin d'un
// niveau gagné (0x4D4914). Chaque joueur devait y presser une touche, chacun à son heure, pendant
// que sa machine ne répondait plus à personne. En session ils ne sont pas montrés : l'attente
// commune des joueurs les remplace (voir BeginWait).
// Session ouverte, ou qui va l'être : elle ne s'ouvre qu'à la première image du premier niveau,
// après son écran d'accueil.
static bool InSession() { return net::Running() || cfg::Get().mode != cfg::Mode::Off; }

static void ONICALL ONIMP_ENTRY SessionSplashHook(const char* splash, const char* music, uint32_t stopSounds) {
    if (InSession()) {
        LOG("niveau : ecran '%s' saute, session en cours", splash ? splash : "");
        g_previousSplash("", music, stopSounds);
    } else {
        g_previousSplash(splash, music, stopSounds);
    }
}

bool InstallSplashHook() {
    if (mem::CallTarget(addr::kLoseSplashCallSite) != reinterpret_cast<void*>(addr::kSplashTarget)) return false;
    void* previous = nullptr;
    if (!mem::PatchCall(addr::kLoseSplashCallSite, reinterpret_cast<void*>(&LoseSplashHook), &previous)) return false;
    g_previousSplash = reinterpret_cast<SplashFunction>(previous);
    return true;
}

bool InstallIntroSplashHook() {
    if (!g_previousSplash) return false;  // l'écran d'échec d'abord : c'est lui qui donne la fonction d'origine
    const uintptr_t sites[] = {addr::kIntroSplashCallSite, addr::kWinSplashCallSite};
    for (uintptr_t site : sites) {
        if (mem::CallTarget(site) != reinterpret_cast<void*>(addr::kSplashTarget)) return false;
    }
    for (uintptr_t site : sites) {
        void* previous = nullptr;
        if (!mem::PatchCall(site, reinterpret_cast<void*>(&SessionSplashHook), &previous)) return false;
    }
    return true;
}

static bool Host() { return net::CurrentMode() == cfg::Mode::Host; }
static bool Client() { return net::CurrentMode() == cfg::Mode::Client; }

static void NextInstance() {
    if (g_instance == 0) {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        g_instance = (static_cast<uint32_t>(counter.LowPart) & 0x7FFFFF00u) | 1u;
    }
    g_instance++;
}

// Client : l'hôte est dans un autre niveau que nous. D'ordinaire sa commande de victoire nous y
// mène au même instant ; s'il y est allé autrement (une sauvegarde chargée depuis son menu), ou si
// nous arrivons d'ailleurs, nous chargeons son niveau de nous-mêmes, passé ce délai de grâce.
static proto::LevelInstance g_follow = {};
static double g_followAt = 0.0;                // 0 : rien à suivre
static const double kFollowDelay = 2.5;

static void Follow(const proto::LevelInstance& message) {
    if (!message.levelNumber || message.levelHash == mem::LevelHash()) return;
    g_follow = message;
    g_followAt = clk::Seconds() + kFollowDelay;
}

static bool Send(uint8_t kind, const void* data, size_t size, int player = -1) {
    uint8_t packet[1 + sizeof(proto::LevelInstance)];
    if (size > sizeof(packet) - 1) return false;
    packet[0] = kind;
    memcpy(packet + 1, data, size);
    if (player >= 0) return net::SendReliableTo(static_cast<uint8_t>(player), packet, static_cast<int>(1 + size));
    return net::SendReliable(packet, static_cast<int>(1 + size));
}

// Point de sauvegarde d'où notre niveau est parti.
static int32_t SavePoint() {
    int32_t savePoint = 0;
    static uint8_t saved[addr::kContinueSize];
    mem::ReadContinue(savePoint, saved);
    return savePoint;
}

// Hôte : dit à quel chargement nous en sommes, et de quoi le reprendre comme nous. À tous les
// clients, ou à un seul.
static void SendInstance(int player = -1) {
    if (!Host() || !net::Connected() || !mem::Player()) return;
    if (g_instance == 0) NextInstance();  // session ouverte en cours de niveau
    proto::LevelInstance message = {};
    message.levelHash = mem::LevelHash();
    message.instance = g_instance;
    message.levelNumber = mem::LevelNumber();
    mem::ReadContinue(message.savePoint, message.data);
    Send(proto::kReliableLevelInstance, &message, sizeof(message), player);
}

uint32_t Instance() { return Host() ? g_instance : g_synced; }
bool Waiting() { return g_waiting; }

// -------------------------------------------------------------------------------------------------
// Attente : tout le monde démarre ensemble
// -------------------------------------------------------------------------------------------------
// Après un chargement, chaque joueur voyait l'écran d'accueil du niveau et devait presser une
// touche pour entrer en jeu, chacun à son heure : pendant ce temps sa machine ne répondait plus au
// réseau, et les autres jouaient déjà. En session cet écran n'est plus montré. À la place, le jeu
// reste en pause après le chargement, chez l'hôte comme chez les clients, jusqu'à ce que tous
// l'aient chargé : chaque client le dit à l'hôte, qui donne le départ à tous en même temps.
static void BeginWait(const char* what) {
    if (!cfg::Get().readyCheck || !mem::Game()) return;
    double now = clk::Seconds();
    g_waiting = true;
    g_waitUntil = now + kWaitTimeout;
    g_nextWaitNote = now + kWaitNote;
    g_hostSilentSince = 0.0;
    memset(g_goneSince, 0, sizeof(g_goneSince));
    mem::SetPaused(true);
    LOG("attente : niveau charge, jeu en pause %s", what);
}

static void EndWait(const char* why) {
    if (!g_waiting) return;
    g_waiting = false;
    mem::SetPaused(false);
    LOG("attente : terminee, %s", why);
}

static void SendReady() {
    if (!Client() || !net::Connected() || !g_synced) return;
    proto::Ready ready = {mem::LevelHash(), g_synced};
    if (Send(proto::kReliableReady, &ready, sizeof(ready))) LOG("attente : niveau charge, dit a l'hote");
}

static int Count(uint8_t mask) {
    int count = 0;
    for (int player = 1; player < proto::kMaxPlayers; player++) count += (mask >> player) & 1;
    return count;
}

// Hôte : tous ceux qu'on attendait ont chargé, ou l'on n'attend plus.
static void Release(const char* why, bool everyone) {
    if (!g_waiting) return;
    EndWait(why);
    proto::Go go = {g_instance};
    Send(proto::kReliableGo, &go, sizeof(go));
    if (everyone) mem::ConsolePrint("OniMP : tous les joueurs ont charge : c'est parti");
    else mem::ConsolePrint("OniMP : attente terminee (%s) : c'est parti", why);
}

void OnReady(uint8_t from, const uint8_t* data, int size) {
    proto::Ready ready;
    if (!Host() || from == 0 || from >= proto::kMaxPlayers || size < static_cast<int>(sizeof(ready))) return;
    memcpy(&ready, data, sizeof(ready));
    if (ready.levelHash != mem::LevelHash() || ready.instance != g_instance) return;
    g_ready |= static_cast<uint8_t>(1u << from);
    LOG("attente : le joueur %u a charge le niveau", from);
    if (!g_waiting) {
        // Nous jouons déjà : il n'a pas à nous attendre.
        proto::Go go = {g_instance};
        Send(proto::kReliableGo, &go, sizeof(go), from);
        return;
    }
    if ((g_ready & g_expected) == g_expected) Release("tous les joueurs ont charge", true);
}

void OnGo(uint8_t from, const uint8_t* data, int size) {
    proto::Go go;
    if (!Client() || from != proto::kHostPlayerId || size < static_cast<int>(sizeof(go))) return;
    memcpy(&go, data, sizeof(go));
    if (!g_waiting || go.instance != g_synced) return;
    EndWait("depart donne par l'hote");
    mem::ConsolePrint("OniMP : tous les joueurs ont charge : c'est parti");
}

static void UpdateWait(double now) {
    if (!g_waiting) return;
    if (!net::Running()) {
        EndWait("session fermee");
        return;
    }
    if (Host()) {
        // Un client attendu qui ne répond plus depuis longtemps est parti : on ne l'attend plus.
        for (int player = 1; player < proto::kMaxPlayers; player++) {
            uint8_t bit = static_cast<uint8_t>(1u << player);
            if (!(g_expected & bit) || (g_ready & bit)) continue;
            if (net::Connected(static_cast<uint8_t>(player))) {
                g_goneSince[player] = 0.0;
            } else if (g_goneSince[player] == 0.0) {
                g_goneSince[player] = now;
            } else if (now - g_goneSince[player] > kGoneTimeout) {
                LOG("attente : le joueur %d ne revient pas, on ne l'attend plus", player);
                g_expected &= static_cast<uint8_t>(~bit);
            }
        }
        if ((g_ready & g_expected) == g_expected) {
            Release(g_expected ? "tous les joueurs ont charge" : "plus personne a attendre", g_expected != 0);
        } else if (now >= g_waitUntil) {
            Release("delai depasse", false);
        } else if (now >= g_nextWaitNote) {
            g_nextWaitNote = now + kWaitNote;
            mem::ConsolePrint("OniMP : en attente de %d joueur(s) qui charge(nt) le niveau...",
                              Count(static_cast<uint8_t>(g_expected & ~g_ready)));
        }
        return;
    }
    // Client : le départ vient de l'hôte. S'il se tait trop longtemps, nous repartons seuls.
    if (net::Connected()) g_hostSilentSince = 0.0;
    else if (g_hostSilentSince == 0.0) g_hostSilentSince = now;
    bool hostGone = g_hostSilentSince != 0.0 && now - g_hostSilentSince > kGoneTimeout;
    if (hostGone || now >= g_waitUntil) {
        EndWait(hostGone ? "l'hote ne repond plus" : "delai depasse");
        mem::ConsolePrint("OniMP : pas de depart donne par l'hote : la partie reprend");
    } else if (now >= g_nextWaitNote) {
        g_nextWaitNote = now + kWaitNote;
        mem::ConsolePrint("OniMP : en attente du depart donne par l'hote...");
    }
}

void DropSplash() {
    if (!InSession()) return;
    if (mem::DropSplash()) LOG("niveau : ecran d'accueil saute, session en cours");
}

// -------------------------------------------------------------------------------------------------
// Reprise commune
// -------------------------------------------------------------------------------------------------
// Pourquoi la partie reprend, dit à l'écran. player : le joueur en cause.
static void Tell(uint8_t reason, uint8_t player) {
    if (reason == proto::kRestartBledOut) {
        mem::ConsolePrint("OniMP : le joueur %u n'a pas ete ranime a temps : mission echouee", player);
    } else if (reason == proto::kRestartCivilian) {
        mem::ConsolePrint("OniMP : un civil a ete tue par le joueur %u : mission echouee", player);
    } else if (reason == proto::kRestartJoin) {
        mem::ConsolePrint("OniMP : le joueur %u arrive : reprise au dernier point de sauvegarde dans %d s", player,
                          static_cast<int>(kJoinDelay));
        g_countdownEnd = clk::Seconds() + kJoinDelay;
        g_countdownSaid = static_cast<int>(kJoinDelay);
    } else {
        mem::ConsolePrint("OniMP : le joueur %u est mort, reprise a la derniere sauvegarde", player);
    }
}

// La machine qui décide du niveau reprend à sa dernière sauvegarde, après l'avoir dit aux clients.
static void ScheduleRestart(uint8_t reason, uint8_t player, double now) {
    if (g_restartAt != 0.0) return;  // une reprise est déjà en route
    LOG("reprise : %s (joueur %u), rechargement de la derniere sauvegarde",
        reason == proto::kRestartBledOut   ? "joueur a terre pas ranime a temps"
        : reason == proto::kRestartCivilian ? "civil tue"
        : reason == proto::kRestartJoin     ? "arrivee d'un joueur"
                                            : "joueur mort",
        player);
    Tell(reason, player);
    proto::Alert alert = {reason, player};
    Send(proto::kReliableAlert, &alert, sizeof(alert));
    double delay = reason == proto::kRestartDeath ? kRestartDelay : (reason == proto::kRestartJoin ? kJoinDelay : kFailDelay);
    g_restartAt = now + delay;
}

void Fail(uint8_t reason, uint8_t player, double now) {
    if (net::CurrentMode() == cfg::Mode::Off) return;
    if (!Client() || !net::Connected()) {
        ScheduleRestart(reason, player, now);  // l'hôte, ou un client coupé de lui : son propre niveau
        return;
    }
    // C'est à l'hôte de recharger, nous suivrons.
    proto::RestartRequest message = {mem::LevelHash(), g_synced, reason};
    if (!Send(proto::kReliableRestartRequest, &message, sizeof(message))) return;
    g_deathReported = true;  // si notre joueur en est mort, sa mort est dite
    LOG("reprise : mission echouee (raison %u), demandee a l'hote", reason);
}

void OnAlert(uint8_t from, const uint8_t* data, int size) {
    proto::Alert alert;
    if (!Client() || from != proto::kHostPlayerId || size < static_cast<int>(sizeof(alert))) return;
    memcpy(&alert, data, sizeof(alert));
    Tell(alert.reason, alert.player);
    // C'est pour nous que l'hôte recharge : notre inventaire sera vidé une fois le niveau repris,
    // pas avant, le script de reprise nous rendant celui de la sauvegarde.
    if (alert.reason == proto::kRestartJoin && alert.player == net::LocalPlayerId() && g_wipe) g_wipeNotBefore = 1e30;
}

void Resend(uint8_t player) { SendInstance(player); }

// -------------------------------------------------------------------------------------------------
// Points de sauvegarde
// -------------------------------------------------------------------------------------------------
// À un point de sauvegarde le script du niveau appelle save_game : le moteur note où en est son
// joueur, récrit persist.dat en entier et affiche « autosave ». Chez un client cela ne servait à
// rien, et pouvait nuire : sa reprise est celle de l'hôte, qui lui envoie son propre point avec son
// état (LevelInstance) ; et sur une seule machine les instances se partagent persist.dat, que le
// client récrivait avec sa copie en mémoire, périmée, par-dessus la progression de l'hôte. Signalé
// en jeu en 0.23 : le client plantait à l'arrivée sur un point de sauvegarde. En session un client
// saute donc sa propre commande, et l'hôte lui dit quand la sienne a eu lieu.
void OnHostSaved() {
    if (!Host()) return;
    proto::SavePoint message = {mem::LevelHash(), SavePoint()};
    LOG("sauvegarde : point %d atteint%s", message.savePoint, net::Connected() ? ", dit aux clients" : "");
    if (net::Connected()) Send(proto::kReliableSavePoint, &message, sizeof(message));
}

void OnLocalSaveSkipped(int32_t savePoint) {
    static int32_t last = -2;
    static uint32_t level = 0;
    if (savePoint == last && level == mem::LevelHash()) return;  // le script y repasse : dit une fois
    last = savePoint;
    level = mem::LevelHash();
    LOG("sauvegarde : point %d rencontre par notre script, laisse a l'hote (rien d'ecrit ici)", savePoint);
}

void OnSavePoint(uint8_t from, const uint8_t* data, int size) {
    proto::SavePoint message;
    if (!Client() || from != proto::kHostPlayerId || size < static_cast<int>(sizeof(message))) return;
    memcpy(&message, data, sizeof(message));
    if (message.levelHash != mem::LevelHash()) return;
    LOG("sauvegarde : l'hote a atteint le point %d", message.savePoint);
    mem::ConsolePrint("OniMP : point de sauvegarde %d atteint, garde par l'hote", message.savePoint);
}

// -------------------------------------------------------------------------------------------------
// Arrivée d'un joueur
// -------------------------------------------------------------------------------------------------
// Un client qui arrive a chargé son propre niveau, à son propre point de sauvegarde : ni les
// ennemis, ni les portes, ni les objectifs n'y sont ceux de l'hôte. Jusqu'à la 0.21 il le gardait,
// et l'hôte rechargeait à la main pour que tous repartent du même point. Maintenant le client
// s'annonce, et l'hôte reprend de lui-même à son dernier point de sauvegarde après un compte à
// rebours : les clients suivent, comme pour toute reprise, et retrouvent l'état du niveau et les
// objectifs de l'hôte. Le nouveau venu repart sans armes : celles de la sauvegarde sont à l'hôte.
// Rien de tout cela si les deux niveaux viennent d'être chargés au même point.
static void Arrive() {
    g_joined = true;
    proto::Join join = {mem::LevelHash(), SavePoint(), static_cast<uint8_t>(mem::GameTime() < kFreshTicks ? 1 : 0)};
    if (!Send(proto::kReliableJoin, &join, sizeof(join))) {
        g_joined = false;
        return;
    }
    g_wipe = true;
    g_wipeNotBefore = clk::Seconds() + kWipeGrace;
    LOG("arrivee : annoncee a l'hote (point de sauvegarde %d, niveau charge depuis %d s)", join.savePoint,
        mem::GameTime() / 60);
}

void OnJoin(uint8_t from, const uint8_t* data, int size, double now) {
    proto::Join join;
    if (!Host() || size < static_cast<int>(sizeof(join)) || !mem::Player()) return;
    memcpy(&join, data, sizeof(join));
    if (join.levelHash != mem::LevelHash()) return;
    int32_t savePoint = SavePoint();
    if (!cfg::Get().joinReload) {
        LOG("arrivee : joueur %u ; join_reload = false, il garde le niveau qu'il a charge", from);
        return;
    }
    if (join.fresh && mem::GameTime() < kFreshTicks && join.savePoint == savePoint) {
        LOG("arrivee : joueur %u ; nos deux niveaux viennent d'etre charges au point %d, rien a reprendre", from,
            savePoint);
        return;
    }
    LOG("arrivee : joueur %u (parti du point %d, niveau %s ; ici point %d, %d s de jeu)", from, join.savePoint,
        join.fresh ? "tout juste charge" : "deja joue", savePoint, mem::GameTime() / 60);
    ScheduleRestart(proto::kRestartJoin, from, now);
}

// -------------------------------------------------------------------------------------------------
// Chargements
// -------------------------------------------------------------------------------------------------
void OnLevelLoaded() {
    g_restartAt = 0.0;
    g_countdownSaid = 0;
    g_deathReported = false;
    if (g_waiting) EndWait("nouveau chargement");
    if (Host()) {
        NextInstance();
        SendInstance();
        // Ceux qui étaient là vont recharger à leur tour : on les attend.
        g_ready = 0;
        g_expected = 0;
        for (int player = 1; player < proto::kMaxPlayers; player++) {
            if (net::Connected(static_cast<uint8_t>(player))) g_expected |= static_cast<uint8_t>(1u << player);
        }
        if (g_expected) {
            BeginWait("jusqu'a ce que les autres joueurs l'aient charge");
            if (g_waiting) mem::ConsolePrint("OniMP : niveau charge : en attente de %d joueur(s)...", Count(g_expected));
        }
        return;
    }
    if (g_pending) {
        g_synced = g_pending;
        g_pending = 0;
    }
    if (g_wipe) g_wipeNotBefore = 0.0;  // niveau repris : l'inventaire se vide dès que le joueur l'a reçu
    // Notre niveau repart de son état initial : une nouvelle connexion nous vaut de l'hôte tout
    // ce que ses scripts ont fait depuis son propre chargement.
    net::RequestReconnect();
    // Un hôte est connu : c'est lui qui donne le départ. Au premier niveau du lancement, non.
    if (Client() && g_synced) {
        BeginWait("jusqu'au depart donne par l'hote");
        if (g_waiting) mem::ConsolePrint("OniMP : niveau charge : en attente du depart donne par l'hote...");
    }
}

void OnConnection(uint8_t player, bool connected) {
    if (!connected) return;
    SendInstance(player);
    if (Client() && g_waiting) SendReady();
}

void OnInstance(uint8_t from, const uint8_t* data, int size) {
    proto::LevelInstance message;
    if (!Client() || from != proto::kHostPlayerId) return;
    if (size < static_cast<int>(sizeof(message)) || !mem::Player()) return;
    memcpy(&message, data, sizeof(message));
    bool sameLevel = message.levelHash == mem::LevelHash();

    if (!g_joined && sameLevel) {
        // Première rencontre dans ce niveau : nous gardons celui que nous avons chargé, et l'hôte
        // dira s'il faut repartir ensemble.
        g_synced = message.instance;
        Arrive();
        SendReady();  // l'hôte attend peut-être encore d'autres joueurs : il nous compte
        return;
    }
    if (g_synced == 0) {
        g_synced = message.instance;  // premier contact, dans un autre niveau : nous l'y rejoignons
        Follow(message);
        return;
    }
    if (message.instance == g_synced || message.instance == g_pending) return;
    if (!sameLevel) {
        // Autre niveau : la commande de victoire de l'hôte nous y mène, ou à défaut Follow.
        g_synced = message.instance;
        Follow(message);
        return;
    }
    // Notre niveau vient lui aussi d'être chargé, et du même point : c'est déjà celui de l'hôte
    // (niveau suivant atteint chacun de son côté, reprise faite avant d'en recevoir l'ordre). Le
    // recharger encore ne ferait que doubler l'attente : c'était le double chargement du client à
    // chaque changement de niveau, jusqu'à la 0.21.
    if (mem::GameTime() < kFreshTicks && SavePoint() == message.savePoint) {
        LOG("reprise : l'hote a charge ce niveau au point %d, nous aussi a l'instant : rien a recharger",
            message.savePoint);
        g_synced = message.instance;
        if (g_wipe) g_wipeNotBefore = 0.0;
        // Le départ reste donné par l'hôte, pour tous en même temps ; s'il joue déjà, il le donne
        // aussitôt.
        if (!g_waiting) BeginWait("jusqu'au depart donne par l'hote");
        SendReady();
        return;
    }
    // L'hôte a repris à sa dernière sauvegarde : nous reprenons à la même, avec l'état qu'il y
    // avait enregistré (position, santé, armes).
    LOG("reprise : l'hote a recharge le niveau, point de sauvegarde %d", message.savePoint);
    mem::ConsolePrint("OniMP : reprise a la derniere sauvegarde de l'hote");
    g_pending = message.instance;
    mem::WriteContinue(message.savePoint, message.data);
    if (g_waiting) EndWait("l'hote a recharge, nous le suivons");
    mem::RestartLevel();
}

void OnRestartRequest(uint8_t from, const uint8_t* data, int size, double now) {
    proto::RestartRequest message;
    if (!Host() || size < static_cast<int>(sizeof(message))) return;
    memcpy(&message, data, sizeof(message));
    if (message.levelHash != mem::LevelHash() || g_restartAt != 0.0) return;
    // Mort pendant un chargement que nous avons refait depuis : ce client va de toute façon nous
    // suivre. Sans cela, deux clients morts à quelques secondes d'écart valaient deux reprises.
    if (message.instance != 0 && message.instance != g_instance) return;
    ScheduleRestart(message.reason, from, now);
}

// Le nouveau venu repart sans armes ni réserves : celles que la sauvegarde de l'hôte lui a rendues
// sont à l'hôte, et le même inventaire chez deux joueurs faisait des armes en double.
static void WipeInventory(Character* player) {
    g_wipe = false;
    const char* name = mem::Name(player);
    if (!name[0]) {
        LOG("arrivee : notre joueur n'a pas de nom, inventaire laisse tel quel");
        return;
    }
    mem::Exec("chr_inv_reset %s", name);
    LOG("arrivee : inventaire de notre joueur vide");
    mem::ConsolePrint("OniMP : vous arrivez dans la partie sans armes");
}

void Update(double now) {
    Character* player = mem::Player();
    if (!player) return;
    UpdateWait(now);

    // Compte à rebours avant une reprise annoncée : une ligne par seconde.
    if (g_countdownSaid > 0) {
        int left = static_cast<int>(std::ceil(g_countdownEnd - now));
        if (left < 1) {
            g_countdownSaid = 0;
        } else if (left < g_countdownSaid) {
            g_countdownSaid = left;
            mem::ConsolePrint("OniMP : reprise dans %d s", left);
        }
    }

    if (g_restartAt != 0.0 && now >= g_restartAt) {
        g_restartAt = 0.0;
        EndWait("reprise");  // le moteur recharge depuis sa boucle, qu'il tourne ou non
        mem::RestartLevel();
        return;
    }
    if (!Client() || !net::Connected()) return;

    // L'hôte est dans un autre niveau et rien ne nous y a menés : nous le chargeons, au point de
    // sauvegarde et avec l'état du joueur qu'il nous a donnés. C'est la reprise après un échec du
    // moteur, faite dans un autre niveau que le nôtre.
    if (g_followAt != 0.0 && now >= g_followAt) {
        g_followAt = 0.0;
        if (g_follow.levelHash != mem::LevelHash() && g_follow.levelNumber != 0) {
            LOG("reprise : l'hote est dans un autre niveau (numero %u, point de sauvegarde %d), nous sommes dans le %u : "
                "nous le chargeons", g_follow.levelNumber, g_follow.savePoint, mem::LevelNumber());
            mem::ConsolePrint("OniMP : l'hote a charge un autre niveau : nous le suivons");
            mem::WriteContinue(g_follow.savePoint, g_follow.data);
            if (g_waiting) EndWait("l'hote est dans un autre niveau, nous le suivons");
            mem::LoadLevel(g_follow.levelNumber);
            return;
        }
    }

    if (g_wipe && !g_waiting && now >= g_wipeNotBefore && mem::GameTime() >= kWipeTicks && !mem::IsDead(player)) {
        WipeInventory(player);
    }

    // Notre joueur vient de mourir : c'est à l'hôte de recharger, nous suivrons.
    bool dead = mem::IsDead(player);
    if (dead && !g_deathReported) {
        proto::RestartRequest message = {mem::LevelHash(), g_synced, proto::kRestartDeath};
        if (Send(proto::kReliableRestartRequest, &message, sizeof(message))) {
            g_deathReported = true;
            LOG("reprise : notre joueur est mort, demandee a l'hote");
        }
    } else if (!dead) {
        g_deathReported = false;
    }
}

void Describe(char* out, int capacity) {
    const char* waiting = g_waiting ? ", en attente des autres joueurs" : "";
    if (Host()) snprintf(out, static_cast<size_t>(capacity), "chargement %u%s", g_instance & 0xFF, waiting);
    else snprintf(out, static_cast<size_t>(capacity), "chargement suivi %u%s", g_synced & 0xFF, waiting);
}

}  // namespace level
