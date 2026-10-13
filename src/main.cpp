// OniMP : coop en réseau expérimental pour Oni (PC, 2001), jusqu'à quatre joueurs.
// Cette DLL se fait passer pour realbink.dll : Daodan la charge juste avant ONiMain, et tous les
// exports Bink sont renvoyés vers realbink_orig.dll (voir exports.def).
#include <windows.h>

#include <cstring>
#include <string>

#include "alert.h"
#include "autoplay.h"
#include "cheat.h"
#include "clock.h"
#include "config.h"
#include "coop.h"
#include "defense.h"
#include "desync.h"
#include "interact.h"
#include "lang.h"
#include "levelsync.h"
#include "log.h"
#include "memory.h"
#include "mods.h"
#include "network.h"
#include "oni.h"
#include "replication.h"
#include "revive.h"
#include "roster.h"
#include "scriptsync.h"
#include "spawns.h"
#include "textconsole.h"
#include "world.h"

using namespace oni;

static const char* const kVersion = "0.39";

static void(__cdecl* g_previousTick)() = nullptr;
static void(__cdecl* g_previousModalUpdate)() = nullptr;
static uint32_t(__cdecl* g_previousGameInit)() = nullptr;

static bool g_sessionInitialised = false;
static uint32_t g_levelHash = 0;
static int32_t g_lastGameTime = 0;
static double g_nextSend = 0.0;
static double g_nextTrace = 0.0;
static unsigned g_sendCounter = 0;
static unsigned g_playerPacketsReceived = 0;
static double g_now = 0.0;

// -------------------------------------------------------------------------------------------------
// Session
// -------------------------------------------------------------------------------------------------
static bool StartSession(cfg::Mode mode, const char* host, uint16_t port) {
    rep::OnSessionClosed();
    const cfg::Config& config = cfg::Get();
    net::Configure(config.gameName.c_str(), config.joinName.c_str(), kVersion, config.netInterface.c_str());
    bool ok = net::Start(mode, host, port);
    coop::OnSessionStarted(ok && mode == cfg::Mode::Host);
    cheat::OnSessionStarted(ok && mode == cfg::Mode::Host);
    if (mode == cfg::Mode::Off) {
        mem::ConsolePrint("OniMP : session arretee");
    } else if (!ok) {
        mem::ConsolePrint("OniMP : echec du demarrage reseau (voir le journal)");
    } else if (mode == cfg::Mode::Host) {
        mem::ConsolePrint("OniMP : hote sur le port %u, partie '%s', %d joueurs au plus", port,
                          config.gameName.c_str(), proto::kMaxPlayers);
    } else if (net::Searching()) {
        mem::ConsolePrint("OniMP : recherche d'un hote sur le reseau");
    } else {
        mem::ConsolePrint("OniMP : connexion a %s:%u", host, port);
    }
    return ok;
}

static void OnPlayerPacket(const proto::EntityState& state, uint32_t levelHash) {
    g_playerPacketsReceived++;
    rep::OnPlayerState(state, levelHash, g_now);
}

static void OnNpcSnapshotPacket(const proto::EntityState* states, int count, uint32_t levelHash) {
    rep::OnNpcSnapshot(states, count, levelHash, g_now);
}

// Hôte : le client player arrive ou s'en va. Client : player vaut 0, c'est la liaison avec l'hôte.
static void OnConnectionChanged(uint8_t player, bool connected) {
    bool host = net::CurrentMode() == cfg::Mode::Host;
    if (host) {
        mem::ConsolePrint("OniMP : joueur %u %s (%d sur %d)", player, connected ? "connecte" : "deconnecte",
                          1 + net::ConnectedCount(), proto::kMaxPlayers);
    } else if (connected) {
        mem::ConsolePrint("OniMP : connecte a l'hote, vous etes le joueur %u", net::LocalPlayerId());
    } else {
        mem::ConsolePrint("OniMP : hote perdu");
    }
    if (!connected) rep::OnDisconnected(player);
    level::OnConnection(player, connected);  // avant le journal des scripts : le client sait d'abord où en est l'hôte
    script::OnConnection(player, connected);
    world::OnConnection(player, connected);
    coop::OnConnection(player, connected);
    cheat::OnConnection(player, connected);  // après les règles de l'hôte : l'accès d'abord, les personnages ensuite
    defense::OnConnection(player, connected);
    desync::OnConnection(player, connected);
    mods::OnConnection(player, connected);
    roster::OnConnection(player, connected);
}

static void OnNotice(const char* text) { mem::ConsolePrint("OniMP : %s", text); }

// Ce qu'un joueur adresse aux autres. L'hôte, par qui tout passe, le redistribue aux autres
// clients (voir net::Handlers::shared) ; chacun y lit s'il est concerné.
static bool SharedBetweenPlayers(uint8_t kind) {
    switch (kind) {
        case proto::kReliableDoor:
        case proto::kReliableConsole:
        case proto::kReliableWeapon:
        case proto::kReliableThrow:
        case proto::kReliableHit:
        case proto::kReliableImpose:
        case proto::kReliableWeaponEvent:
        case proto::kReliablePowerup:
        case proto::kReliableReload:
        case proto::kReliableRevive:
        case proto::kReliableDisarm:
        case proto::kReliableLaserTrip:
            return true;
        default:
            return false;
    }
}

// from : le joueur auteur du message.
static void OnReliableMessage(uint8_t from, const uint8_t* payload, int size) {
    if (size < 1) return;
    switch (payload[0]) {
        case proto::kReliableScript: script::OnReliable(from, payload + 1, size - 1); break;
        case proto::kReliableThrow: rep::OnThrow(from, payload + 1, size - 1, g_now); break;
        case proto::kReliableHit: rep::OnHit(from, payload + 1, size - 1, g_now); break;
        case proto::kReliableImpose: rep::OnImpose(from, payload + 1, size - 1, g_now); break;
        case proto::kReliableLevelInstance: level::OnInstance(from, payload + 1, size - 1); break;
        case proto::kReliableRestartRequest: level::OnRestartRequest(from, payload + 1, size - 1, g_now); break;
        case proto::kReliableSettings: coop::OnReliable(from, payload + 1, size - 1); break;
        case proto::kReliableDigest: desync::OnDigest(from, payload + 1, size - 1, g_now); break;
        case proto::kReliableResync: desync::OnResync(from, payload + 1, size - 1); break;
        case proto::kReliableMods: mods::OnReliable(from, payload + 1, size - 1); break;
        case proto::kReliableRevive: revive::OnReliable(from, payload + 1, size - 1, g_now); break;
        case proto::kReliableAlert: level::OnAlert(from, payload + 1, size - 1); break;
        case proto::kReliableJoin: level::OnJoin(from, payload + 1, size - 1, g_now); break;
        case proto::kReliableReady: level::OnReady(from, payload + 1, size - 1); break;
        case proto::kReliableGo: level::OnGo(from, payload + 1, size - 1); break;
        case proto::kReliableNpcWeapons: rep::OnNpcWeapons(from, payload + 1, size - 1); break;
        case proto::kReliableInteract: interact::OnInteract(from, payload + 1, size - 1); break;
        case proto::kReliableGift: interact::OnGift(from, payload + 1, size - 1); break;
        case proto::kReliableSavePoint: level::OnSavePoint(from, payload + 1, size - 1); break;
        case proto::kReliableProfile: roster::OnProfile(from, payload + 1, size - 1); break;
        case proto::kReliableRoster: roster::OnRoster(from, payload + 1, size - 1); break;
        case proto::kReliableDisarm: rep::OnDisarm(from, payload + 1, size - 1, g_now); break;
        case proto::kReliableMorph:
        case proto::kReliableMorphs:
        case proto::kReliableSpawn:
        case proto::kReliableSpawned: cheat::OnReliable(payload[0], from, payload + 1, size - 1); break;
        case proto::kReliableDefenses: defense::OnReliable(from, payload + 1, size - 1); break;
        case proto::kReliableTurretTargets: defense::OnTargets(from, payload + 1, size - 1); break;
        case proto::kReliableLaserTrip: defense::OnTrip(from, payload + 1, size - 1); break;
        case proto::kReliableLaserRays: defense::OnRays(from, payload + 1, size - 1); break;
        default: world::OnReliable(payload[0], from, payload + 1, size - 1, g_now); break;
    }
}

static const net::Handlers kHandlers = {
    OnPlayerPacket, OnNpcSnapshotPacket, rep::OnNpcInfo, rep::OnEvent, OnConnectionChanged, OnReliableMessage,
    OnNotice,       SharedBetweenPlayers,
};

static void SendLocalState() {
    proto::EntityState player;
    if (rep::CollectLocalPlayer(player)) net::SendPlayer(player, g_levelHash);

    const cfg::Config& config = cfg::Get();
    if (!config.npcs || net::CurrentMode() != cfg::Mode::Host || !net::Connected()) return;

    // PNJ une émission sur deux ; noms une fois par seconde environ.
    g_sendCounter++;
    if (g_sendCounter % 2 != 0) return;
    static proto::EntityState states[off::kMaxCharacters];
    static proto::NpcInfo infos[off::kMaxCharacters];
    bool withNames = g_sendCounter % (2 * static_cast<unsigned>(config.rate)) < 2 || rep::TakeNamesRequest();
    int count = rep::CollectNpcs(states, withNames ? infos : nullptr, off::kMaxCharacters);
    if (withNames) net::SendNpcInfo(infos, count, g_levelHash);
    net::SendNpcSnapshot(states, count, g_levelHash);
}

static void Trace() {
    Character* player = mem::Player();
    Vector3 location = mem::Location(player);
    Vector3 actual = field<Vector3>(player, off::kChPosition);
    Vector3 physics = {};
    if (ActiveCharacter* active = mem::Active(player)) {
        if (void* context = field<void*>(active, off::kAcPhysics)) physics = field<Vector3>(context, off::kPhPosition);
    }
    char status[96];
    net::Status(status, sizeof(status));
    char remote[320];
    rep::DescribeRemotePlayers(remote, sizeof(remote));
    char quests[128];
    script::Describe(quests, sizeof(quests));
    char objects[200];
    world::Describe(objects, sizeof(objects));
    LOG("trace : niveau='%s' t=%d loc=(%.1f %.1f %.1f) pos=(%.1f %.1f %.1f) phy=(%.1f %.1f %.1f) "
        "cap=%.2f anim='%s' frame=%d pv=%d | %s recus=%u pnj=%d | distants %s | %s | %s",
        mem::LevelName(), mem::GameTime(), location.x, location.y, location.z, actual.x, actual.y, actual.z,
        physics.x, physics.y, physics.z, field<float>(player, off::kChFacing),
        mem::AnimName(mem::CurrentAnimation(player)), mem::CurrentFrame(player), mem::Health(player), status,
        g_playerPacketsReceived, rep::NpcPuppetCount(), remote, quests, objects);
}

// -------------------------------------------------------------------------------------------------
// Boucle : une fois par frame, sur le thread du jeu
// -------------------------------------------------------------------------------------------------
// Titre de la fenêtre du jeu, donné par le script de lancement (variable ONIMP_TITLE) : « Host -
// Oni », « Client - Oni ». Deux fenêtres du jeu sur une même machine portent sinon le même. Posé
// dès que la fenêtre existe, et reposé si le jeu le change.
static BOOL CALLBACK FindOwnWindow(HWND window, LPARAM result) {
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (process != GetCurrentProcessId() || !IsWindowVisible(window) || GetWindow(window, GW_OWNER)) return TRUE;
    // La fenêtre du jeu, pas une autre du même processus : sa classe est « ONI ».
    char windowClass[16] = "";
    GetClassNameA(window, windowClass, sizeof(windowClass));
    if (strncmp(windowClass, "ONI", 3) != 0) return TRUE;
    *reinterpret_cast<HWND*>(result) = window;
    return FALSE;
}

static void ApplyWindowTitle() {
    static DWORD nextCheck = 0;
    static bool said = false;
    const std::string& title = cfg::Get().windowTitle;
    DWORD tick = GetTickCount();
    if (title.empty() || static_cast<int32_t>(tick - nextCheck) < 0) return;
    nextCheck = tick + 1000;
    HWND window = nullptr;
    EnumWindows(FindOwnWindow, reinterpret_cast<LPARAM>(&window));
    if (!window) return;
    char current[128] = "";
    GetWindowTextA(window, current, sizeof(current));
    if (title == current) return;
    SetWindowTextA(window, title.c_str());
    if (!said) LOG("fenetre : titre '%s' (c'etait '%s')", title.c_str(), current);
    said = true;
}

static void OnFrame() {
    ApplyWindowTitle();
    mem::GuardTextAreas();
    mem::UpdateMessages();
    if (!mem::Game() || !mem::Player()) return;
    // Après la simulation, avant tout le reste et avant le tracé : aucun personnage ne garde une
    // position qui n'est pas un nombre, ni ne la donne aux autres machines.
    mem::GuardTransforms("a l'image");

    g_now = clk::Seconds();
    const cfg::Config& config = cfg::Get();
    if (!g_sessionInitialised) {
        g_sessionInitialised = true;
        if (config.mode != cfg::Mode::Off) StartSession(config.mode, config.host.c_str(), config.port);
    }

    // Changement ou rechargement de niveau : les pointeurs mémorisés ne valent plus rien.
    uint32_t levelHash = mem::LevelHash();
    int32_t gameTime = mem::GameTime();
    if (levelHash != g_levelHash || gameTime < g_lastGameTime) {
        g_levelHash = levelHash;
        LOG("niveau : '%s' (hash %08X)", mem::LevelName(), levelHash);
        net::SetLevelName(mem::LevelName());
        rep::OnLevelChanged();
        revive::OnLevelChanged();
        bot::OnLevelChanged();
        cheat::OnLevelChanged();
        defense::OnLevelChanged();
        spawn::OnLevelChanged();
        world::OnLevelChanged();
        interact::OnLevelChanged();
        roster::OnLevelChanged();
        textcon::OnLevelChanged();
        mem::RebuildAnimTable();
        level::OnLevelLoaded();
        coop::ReloadDifficulty();
    }
    g_lastGameTime = gameTime;

    net::Poll(g_now, g_levelHash, kHandlers);
    script::Update(g_now);  // avant les PNJ : les spawns décidés par l'hôte créent ceux qu'on pilote
    coop::Update(g_now);    // avant les PNJ aussi : leur vie dépend du nombre de clients
    rep::Update(g_now);
    cheat::Update(g_now);   // avant les tenues : la touche « personnage suivant » se voit à cette image
    roster::Update(g_now);  // après les pantins : chacun dans la tenue de son joueur
    revive::Update(g_now);  // après les états des autres : qui est debout, qui est à terre
    bot::Update();          // le jeu automatique suspendu ne garde aucune touche enfoncée
    alert::Update();        // le son d'alerte, prêt avant de servir
    spawn::Update(g_now);   // après les PNJ : renforts et civils autour de ceux qui sont là
    defense::Update(g_now); // après les renforts : lasers et tourelles de l'hôte, vague d'une alarme donnée au tick
    interact::Update(g_now);  // avant l'arme des pantins : celle qu'un civil vient de donner à l'un d'eux part à son joueur
    world::Update(g_now);   // après : l'arme annoncée se donne au pantin une fois qu'il existe
    textcon::Resolve(g_now);  // après le relevé des consoles : pour qui est l'écran qu'un script vient de demander
    level::Update(g_now);
    desync::Update(g_now);

    if (net::Running() && g_now >= g_nextSend) {
        double interval = 1.0 / config.rate;
        g_nextSend = g_nextSend + interval > g_now ? g_nextSend + interval : g_now + interval;
        SendLocalState();
    }

    if (config.debug && g_now >= g_nextTrace) {
        g_nextTrace = g_now + 1.0;
        Trace();
    }
}

// -------------------------------------------------------------------------------------------------
// Menus : en session, le jeu continue derrière
// -------------------------------------------------------------------------------------------------
// Le menu de pause (F1), le menu principal ouvert en jeu (Échap) et l'écran d'une console de
// données sont des dialogues modaux : la boucle du jeu attend qu'ils se ferment, et d'ici là le
// monde est figé. Seul, c'est une pause. À plusieurs, c'est une désynchronisation : les PNJ, que
// l'hôte simule, se figent chez tout le monde dès qu'il ouvre un menu, et le joueur d'un client
// dans un menu ne bouge plus ni n'encaisse plus rien pendant que la partie continue chez les
// autres. Tant qu'un autre joueur est relié, le jeu tourne donc depuis la boucle des dialogues,
// une fois par image, par les deux appels qu'y consacre la boucle du jeu : l'horloge, puis la
// simulation. Le moteur retrace déjà la scène derrière ses dialogues. Les menus, eux, ne changent
// pas ; le joueur qui en a un ouvert ne reçoit simplement aucune commande.
static bool g_inMenuStep = false;
static unsigned g_menuFrames = 0;   // images passées dans un menu depuis la dernière de la boucle du jeu
static unsigned g_menuTicks = 0;    // ticks qui y ont été simulés

static void StepGameInMenu() {
    // Des entrées vides et non une absence d'entrées : faute d'entrées le moteur reconduit les
    // touches du tick précédent, et un joueur qui ouvrait son menu en courant courrait toujours.
    static uint8_t actions[off::kMaxTicksPerFrame * off::kActionSize];
    memset(actions, 0, sizeof(actions));
    fn::UpdateServerTime(mem::Game());
    uint32_t ticks = 0;
    fn::GameUpdate(off::kMaxTicksPerFrame, actions, &ticks);
    g_menuTicks += ticks;
}

static void __cdecl ONIMP_ENTRY ModalUpdateHook() {
    g_previousModalUpdate();
    ApplyWindowTitle();  // le menu principal est un dialogue : la fenêtre a son titre avant la première partie
    // g_inMenuStep : dialogue ouvert par la simulation que nous faisons tourner ici même. Pas de
    // simulation dans la simulation : celui-là fige. Les écrans de texte des scripts, eux, ne
    // s'ouvrent plus de là en session (voir textconsole.h).
    if (g_inMenuStep || !cfg::Get().runInMenus || !net::Running() || !mem::Game() || !mem::Player()) return;
    g_inMenuStep = true;
    static bool running = false;  // le jeu tournait derrière le menu à l'image précédente
    bool others = net::Connected();
    if (g_menuFrames++ == 0) {
        LOG("menu : ouvert, %s", others ? "le jeu continue derriere" : "personne d'autre, le jeu est en pause");
    } else if (others != running) {
        LOG("menu : %s", others ? "un joueur est relie, le jeu reprend derriere" : "plus personne, le jeu se met en pause");
    }
    running = others;
    if (others) StepGameInMenu();
    OnFrame();  // et le réseau dans tous les cas : un client peut arriver pendant que l'hôte est dans son menu
    g_inMenuStep = false;
}

static void __cdecl ONIMP_ENTRY TickHook() {
    g_previousTick();
    if (g_menuFrames) {
        LOG("menu : ferme apres %u image(s), %u tick(s) simule(s)", g_menuFrames, g_menuTicks);
        g_menuFrames = 0;
        g_menuTicks = 0;
    }
    OnFrame();
    // Hors de OnFrame, qui tourne à chaque image pendant que l'écran est ouvert.
    textcon::Update();
    // En dernier : la boucle du jeu regarde juste après nous s'il y a un écran d'accueil à montrer.
    level::DropSplash();
}

static bool InstallMenuStep() {
    if (mem::CallTarget(addr::kModalUpdateCallSite) != reinterpret_cast<void*>(addr::kModalUpdateTarget)) return false;
    void* previous = nullptr;
    if (!mem::PatchCall(addr::kModalUpdateCallSite, reinterpret_cast<void*>(&ModalUpdateHook), &previous)) return false;
    g_previousModalUpdate = reinterpret_cast<void(__cdecl*)()>(previous);
    return true;
}

// -------------------------------------------------------------------------------------------------
// Commandes de console (BSL)
// -------------------------------------------------------------------------------------------------
static uint16_t ONICALL ONIMP_ENTRY BslHost(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    cfg::Config& config = cfg::Get();
    if (numargs >= 1 && args[0].val.value_int32 > 0 && args[0].val.value_int32 < 65536) {
        config.port = static_cast<uint16_t>(args[0].val.value_int32);
    }
    config.mode = cfg::Mode::Host;
    StartSession(config.mode, nullptr, config.port);
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslConnect(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    cfg::Config& config = cfg::Get();
    if (numargs < 1 || !args[0].val.value_str32) return 0;
    cfg::Config parsed = config;
    std::string spec = std::string("connect:") + args[0].val.value_str32;
    if (!cfg::ParseSpec(spec.c_str(), parsed)) {
        mem::ConsolePrint("OniMP : adresse invalide (exemple : mp_connect \"192.168.1.10:27777\")");
        return 0;
    }
    if (numargs >= 2 && args[1].val.value_int32 > 0 && args[1].val.value_int32 < 65536) {
        parsed.port = static_cast<uint16_t>(args[1].val.value_int32);
    }
    config = parsed;
    StartSession(config.mode, config.host.c_str(), config.port);
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslJoin(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    cfg::Config& config = cfg::Get();
    if (numargs >= 1 && args[0].val.value_int32 > 0 && args[0].val.value_int32 < 65536) {
        config.port = static_cast<uint16_t>(args[0].val.value_int32);
    }
    config.mode = cfg::Mode::Client;
    config.host = net::kAutoHost;
    StartSession(config.mode, config.host.c_str(), config.port);
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslDisconnect(sl_callinfo*, uint32_t, sl_arg*, int*, int*, sl_arg*) {
    cfg::Get().mode = cfg::Mode::Off;
    StartSession(cfg::Mode::Off, nullptr, 0);
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslStatus(sl_callinfo*, uint32_t, sl_arg*, int*, int*, sl_arg*) {
    char status[96];
    net::Status(status, sizeof(status));
    mem::ConsolePrint("OniMP %s : %s, niveau '%s', %d autre(s) joueur(s) ici, pnj=%s (%d pilotes)", kVersion, status,
                      mem::LevelName(), rep::RemotePlayerCount(), cfg::Get().npcs ? "on" : "off",
                      rep::NpcPuppetCount());
    char quests[128];
    script::Describe(quests, sizeof(quests));
    mem::ConsolePrint("OniMP : %s", quests);
    char objects[200];
    world::Describe(objects, sizeof(objects));
    mem::ConsolePrint("OniMP : %s", objects);
    char loading[64];
    level::Describe(loading, sizeof(loading));
    mem::ConsolePrint("OniMP : %s", loading);
    char checks[64];
    desync::Describe(checks, sizeof(checks));
    mem::ConsolePrint("OniMP : %s", checks);
    unsigned health = static_cast<unsigned>(coop::NpcHealthPercent());
    unsigned damage = static_cast<unsigned>(coop::NpcDamagePercent());
    mem::ConsolePrint("OniMP : PNJ : vie x%u.%02u, degats x%u.%02u (%s), jeu %s dans les menus", health / 100,
                      health % 100, damage / 100, damage % 100,
                      coop::OnlineDamage() ? "degats en ligne" : "degats normaux",
                      g_previousModalUpdate && cfg::Get().runInMenus ? "en marche" : "en pause");
    char packages[200];
    mods::Describe(packages, sizeof(packages));
    mem::ConsolePrint("OniMP : %s", packages);
    char enemies[160];
    spawn::Describe(enemies, sizeof(enemies));
    mem::ConsolePrint("OniMP : %s", enemies);
    char downed[128];
    revive::Describe(downed, sizeof(downed));
    mem::ConsolePrint("OniMP : %s", downed);
    char screens[96];
    textcon::Describe(screens, sizeof(screens));
    mem::ConsolePrint("OniMP : %s", screens);
    char talks[128];
    interact::Describe(talks, sizeof(talks));
    mem::ConsolePrint("OniMP : %s", talks);
    char players[240];
    roster::Describe(players, sizeof(players));
    mem::ConsolePrint("OniMP : %s", players);
    char sound[128];
    alert::Describe(sound, sizeof(sound));
    mem::ConsolePrint("OniMP : %s", sound);
    char autoplay[256];
    bot::Describe(autoplay, sizeof(autoplay));
    mem::ConsolePrint("OniMP : %s", autoplay);
    bot::DescribeCombat(autoplay, sizeof(autoplay));
    if (autoplay[0]) mem::ConsolePrint("OniMP : %s", autoplay);
    char cheats[320];
    cheat::Describe(cheats, sizeof(cheats));
    mem::ConsolePrint("OniMP : %s", cheats);
    char defenses[240];
    defense::Describe(defenses, sizeof(defenses));
    mem::ConsolePrint("OniMP : %s", defenses);
    defense::DescribeTurrets(defenses, sizeof(defenses));
    mem::ConsolePrint("OniMP : %s", defenses);
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslNpcs(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    cfg::Config& config = cfg::Get();
    if (numargs >= 1) {
        bool enable = args[0].val.value_int32 != 0;
        if (config.npcs && !enable) rep::ReleaseNpcs();
        config.npcs = enable;
    }
    mem::ConsolePrint("OniMP : synchronisation des PNJ %s", config.npcs ? "activee" : "desactivee");
    return 0;
}

static void RegisterCommands() {
    fn::RegisterCommandVoid("mp_host", "OniMP : heberge une partie", "[port:int | ]", BslHost);
    fn::RegisterCommandVoid("mp_connect", "OniMP : rejoint un hote", "address:string [port:int | ]", BslConnect);
    fn::RegisterCommandVoid("mp_join", "OniMP : cherche un hote sur le reseau et le rejoint", "[port:int | ]", BslJoin);
    fn::RegisterCommandVoid("mp_disconnect", "OniMP : ferme la session", "", BslDisconnect);
    fn::RegisterCommandVoid("mp_status", "OniMP : affiche l'etat de la session", "", BslStatus);
    fn::RegisterCommandVoid("mp_npcs", "OniMP : PNJ pilotes par l'hote", "[enable:int | ]", BslNpcs);
    coop::RegisterCommands();
    cheat::RegisterCommands();
    LOG("commandes de console enregistrees");
}

// Appelée à la place de l'initialisation du jeu par Daodan (DDrGame_Init), une seule fois, après
// le démarrage du système de script.
static uint32_t __cdecl ONIMP_ENTRY GameInitHook() {
    uint32_t result = g_previousGameInit();
    RegisterCommands();
    script::DumpCommands();
    return result;
}

// -------------------------------------------------------------------------------------------------
// Chargement
// -------------------------------------------------------------------------------------------------
static HMODULE g_self = nullptr;

// Registres, et ce qui dans la pile ressemble à une adresse de retour : dans le code d'Oni.exe
// (donnée telle quelle) ou dans le nôtre (« +décalage » depuis le début de la DLL). Sans cela une
// faute dans une fonction du moteur appelée de vingt endroits ne dit pas d'où elle vient.
static void LogStack(const CONTEXT* context) {
    LOG("  eax=%08lX ebx=%08lX ecx=%08lX edx=%08lX esi=%08lX edi=%08lX ebp=%08lX esp=%08lX", context->Eax,
        context->Ebx, context->Ecx, context->Edx, context->Esi, context->Edi, context->Ebp, context->Esp);
    MEMORY_BASIC_INFORMATION region;
    auto* stack = reinterpret_cast<const uint32_t*>(context->Esp);
    if ((context->Esp & 3) || !VirtualQuery(stack, &region, sizeof(region)) || region.State != MEM_COMMIT) return;
    auto* end = reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(region.BaseAddress) + region.RegionSize);

    uintptr_t self = reinterpret_cast<uintptr_t>(g_self), selfSize = 0;
    if (g_self) {
        auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_self);
        selfSize = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const uint8_t*>(g_self) + dos->e_lfanew)
                       ->OptionalHeader.SizeOfImage;
    }
    char line[400];
    int length = 0, found = 0;
    for (const uint32_t* slot = stack; slot < end && slot < stack + 1024 && found < 28; slot++) {
        uintptr_t value = *slot;
        int written = 0;
        if (value >= addr::kCodeBegin && value < addr::kCodeEnd) {
            written = snprintf(line + length, sizeof(line) - static_cast<size_t>(length), " %08X", static_cast<unsigned>(value));
        } else if (selfSize && value >= self && value < self + selfSize) {
            written = snprintf(line + length, sizeof(line) - static_cast<size_t>(length), " +%X", static_cast<unsigned>(value - self));
        } else {
            continue;
        }
        if (written <= 0 || written >= static_cast<int>(sizeof(line)) - length) break;
        length += written;
        found++;
    }
    if (found) LOG("  pile :%s", line);
}

static LONG CALLBACK CrashLogger(EXCEPTION_POINTERS* info) {
    static int logged = 0;
    DWORD code = info->ExceptionRecord->ExceptionCode;
    if ((code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION ||
         code == EXCEPTION_STACK_OVERFLOW) && logged < 8) {
        logged++;
        HMODULE module = nullptr;
        char name[MAX_PATH] = "?";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               static_cast<LPCSTR>(info->ExceptionRecord->ExceptionAddress), &module)) {
            GetModuleFileNameA(module, name, MAX_PATH);
        }
        const char* base = strrchr(name, '\\');
        LOG("EXCEPTION %08lX a %p (%s), acces %p", code, info->ExceptionRecord->ExceptionAddress,
            base ? base + 1 : name,
            info->ExceptionRecord->NumberParameters >= 2
                ? reinterpret_cast<void*>(info->ExceptionRecord->ExceptionInformation[1])
                : nullptr);
        if (info->ContextRecord) LogStack(info->ContextRecord);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// Oni (et la réécriture de Daodan) quitte si FindWindow("ONI ", "ONI ") trouve une fenêtre.
// Deux instances sur une même machine sont nécessaires pour tester : on masque les fenêtres d'Oni
// à ce seul appel.
static HWND WINAPI FindWindowHook(LPCSTR className, LPCSTR windowName) {
    if (className && strcmp(className, "ONI ") == 0) return nullptr;
    return FindWindowA(className, windowName);
}

static void AllowMultipleInstances() {
    void* original = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("user32.dll"), "FindWindowA"));
    int patched = mem::PatchImports(GetModuleHandleA(nullptr), original, reinterpret_cast<void*>(&FindWindowHook));
    patched += mem::PatchImports(GetModuleHandleA("binkw32.dll"), original, reinterpret_cast<void*>(&FindWindowHook));
    LOG("instances multiples : %d import(s) FindWindowA redirige(s)", patched);
}

static void Install() {
    cfg::Load();
    const cfg::Config& config = cfg::Get();
    loc::Set(loc::FromKey(config.language.c_str(), loc::Language::French));

    // Plusieurs clients peuvent tourner sur une même machine, dans le même dossier : le premier
    // écrit onimp_client.log, les suivants onimp_client_2.log, onimp_client_3.log.
    std::string logBase = cfg::GameDir() + "onimp";
    if (config.mode != cfg::Mode::Off) logBase += std::string("_") + cfg::ModeName(config.mode);
    for (int instance = 1; instance <= 8; instance++) {
        std::string suffix = instance > 1 ? "_" + std::to_string(instance) : "";
        if (logx::Open((logBase + suffix + ".log").c_str())) break;
    }
    LOG("OniMP %s : mode=%s hote=%s port=%u cadence=%d pnj=%d quetes=%d interface=%s", kVersion,
        cfg::ModeName(config.mode), config.host.c_str(), config.port, config.rate, config.npcs ? 1 : 0,
        config.quests ? 1 : 0, config.netInterface.c_str());
    if (*logx::Archived()) {
        LOG("journal : celui du lancement precedent est garde, %s\\%s (et le debugger.txt du moteur a cote)",
            logx::kArchiveFolder, logx::Archived());
    }
    LOG("langue des textes a l'ecran : %s (le journal reste en francais)", loc::Label(loc::Current()));
    mods::Load();
    mem::ShowMessages(config.showMessages);

    if (!mem::ExeSupported()) {
        LOG("Oni.exe non reconnu : aucun patch pose, le jeu tourne sans OniMP");
        return;
    }
    void* tickTarget = mem::CallTarget(addr::kTickCallSite);
    if (!tickTarget) {
        LOG("site %08X inattendu : aucun patch pose", static_cast<unsigned>(addr::kTickCallSite));
        return;
    }
    if (tickTarget != reinterpret_cast<void*>(addr::kTickCallTarget)) {
        LOG("site %08X deja redirige vers %p : chainage", static_cast<unsigned>(addr::kTickCallSite), tickTarget);
    }

    void* previous = nullptr;
    if (!mem::PatchCall(addr::kTickCallSite, reinterpret_cast<void*>(&TickHook), &previous)) {
        LOG("echec du patch de la boucle de jeu");
        return;
    }
    g_previousTick = reinterpret_cast<void(__cdecl*)()>(previous);

    // Doit précéder l'initialisation du moteur : c'est elle qui enregistre les commandes de script.
    // Posée seulement si l'option est active au lancement : sans elle, aucune commande du jeu
    // ne passe par OniMP.
    if (config.quests && !script::Install()) LOG("quetes : interception des commandes de script impossible");

    if (mem::PatchCall(addr::kGameInitCallSite, reinterpret_cast<void*>(&GameInitHook), &previous)) {
        g_previousGameInit = reinterpret_cast<uint32_t(__cdecl*)()>(previous);
    } else {
        LOG("site d'initialisation inattendu : pas de commandes de console (onimp.ini reste utilisable)");
    }

    if (!rep::InstallAiHook()) LOG("IA : code inattendu, les PNJ ne viseront que le joueur local");
    if (!rep::InstallDamageFilter()) LOG("degats : code inattendu, les coups ne seront pas arbitres entre joueurs");
    if (!rep::InstallShooterHook()) LOG("tirs : code inattendu, un PNJ qui tire chez le client peut faire planter le jeu");

    if (!level::InstallSplashHook()) LOG("reprise : code inattendu, l'ecran d'echec restera affiche");
    if (!level::InstallIntroSplashHook()) {
        LOG("niveau : code inattendu, l'ecran d'accueil attendra une touche apres chaque chargement");
    }
    if (!rep::InstallTriggerHook()) LOG("zones : code inattendu, le client ne declenchera pas les zones du niveau");
    if (!rep::InstallTriggerCountHook()) LOG("zones : code inattendu, plusieurs joueurs dans une zone y compteront chacun pour un");
    if (!rep::InstallPickupHook()) LOG("bonus : code inattendu, le pantin d'un autre joueur ramassera encore le bonus voisin du sien");
    if (!mem::InstallAimHook()) LOG("visee : code inattendu, le bras et l'arme de l'autre joueur ne suivront pas sa visee");
    if (!rep::InstallTargetHook()) LOG("cibles : code inattendu, les PNJ ne choisiront pas le joueur le plus proche");
    if (!rep::InstallOverheadHook()) LOG("sante : code inattendu, pas de jauges au-dessus des personnages");
    if (!rep::InstallMeleeHook()) LOG("melee : code inattendu, avec le tir ami les coups ou les prises ne toucheront pas l'autre joueur");
    if (!coop::InstallMenuHook()) LOG("menu : code inattendu, pas d'onglets Coop et Display (les commandes mp_* restent)");
    if (!mem::InstallMessageHook()) {
        LOG("messages : code inattendu, ils s'ecriront dans la console du jeu, en bas de l'ecran");
    }
    if (!interact::Install()) LOG("civils : code inattendu, ceux de l'hote ne parleront pas aux joueurs des clients");
    if (mem::FixCommandHistory()) {
        LOG("historique des commandes : debordement du moteur corrige (256e ligne ecrite hors du tableau)");
    } else {
        LOG("historique des commandes : code inattendu, le debordement du moteur n'est pas corrige ; les commandes "
            "du mod restent hors de l'historique");
    }
    if (!rep::InstallVisibilityHook()) {
        LOG("cinematiques : code inattendu, les autres joueurs y resteront visibles, et un PNJ en double aussi");
    }
    if (!mem::InstallInsertHook(&coop::InsertPlace)) {
        LOG("portraits : code inattendu, ceux des dialogues restent en haut de l'ecran");
    }
    if (!bot::Install()) LOG("jeu automatique : code inattendu, le bouton Auto-play restera sans effet");
    if (!defense::InstallLaserHook()) {
        LOG("lasers : code inattendu, ceux de l'hote ne testeront que son joueur : un client ne sera detecte que par les siens");
    }
    if (!defense::InstallTripHook()) {
        LOG("lasers : code inattendu, un laser coupe sur une machine ne declenchera rien sur les autres : seules les "
            "tourelles que l'hote allume suivront");
    }
    if (!defense::InstallTurretHook()) {
        LOG("tourelles : code inattendu, chaque machine choisira seule la cible des siennes : un joueur vise chez "
            "l'hote peut ne pas l'etre chez lui");
    }
    if (!defense::InstallFireHook()) {
        LOG("tourelles : code inattendu, celles d'un client ne tireront pas : il tient ses IA passives, et le moteur "
            "interdit alors le tir aux tourelles");
    }
    if (!defense::InstallAlarmHook()) {
        LOG("alarmes : code inattendu, pas de vague sur une alarme du moteur (commande ai2_tripalarm, evenement d'un "
            "laser ou d'une console) ; les consoles dont un ennemi se sert restent");
    }
    if (!cheat::Install()) {
        LOG("cheats : code inattendu, les curseurs de vitesse d'attaque, de deplacement et de saut resteront sans effet");
    }
    // Hors session, rien ne change : l'inventaire du jeu d'origine garde ses six places.
    if (config.mode == cfg::Mode::Off) {
        LOG("inventaire : hors session, six chargeurs et six cellules comme dans le jeu d'origine");
    } else if (mem::RaiseReserveCapacity()) {
        LOG("inventaire : douze chargeurs et douze cellules au lieu de six (les hypos restent a six)");
    } else {
        LOG("inventaire : code inattendu, six chargeurs et six cellules comme dans le jeu d'origine");
    }
    if (!world::InstallConsoleHook()) {
        LOG("consoles : code inattendu, celui qui active une console n'est connu qu'au releve suivant ; l'ecran "
            "d'une console qui se remet a zero peut partir chez tous les joueurs");
    }
    if (!mem::InstallExitGuard()) LOG("fermeture : code inattendu, pas de garde sur les zones de texte du moteur");
    if (!mem::InstallTextAreaGuard()) {
        LOG("zones de texte : code inattendu, elles ne sont controlees qu'une fois par image, pas juste avant leur trace");
    }
    if (!coop::InstallDifficultyHook()) {
        LOG("difficulte : code inattendu, les ennemis gardent la difficulte des options du jeu quel que soit le mode");
    }
    // Les écrans de texte d'abord : tant qu'un script peut en ouvrir un de l'intérieur de la
    // simulation, la faire tourner derrière les dialogues la relancerait dans elle-même.
    bool screens = textcon::Install();
    if (!config.runInMenus) {
        LOG("menu : run_in_menus = false, le jeu se met en pause derriere les menus meme en session");
    } else if (!screens) {
        LOG("ecrans de texte : code inattendu, le jeu se mettra en pause derriere les menus meme en session");
    } else if (InstallMenuStep()) {
        coop::SetMenusServed(true);
    } else {
        LOG("menu : code inattendu, le jeu se mettra en pause derriere les menus meme en session");
    }

    if (config.multiInstance) AllowMultipleInstances();
    AddVectoredExceptionHandler(1, CrashLogger);
    fn::StartupMessage("OniMP %s loaded (mode %s)", kVersion, cfg::ModeName(config.mode));
    LOG("patchs poses (boucle %p, init %p)", reinterpret_cast<void*>(g_previousTick),
        reinterpret_cast<void*>(g_previousGameInit));
}

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    // Verrou du chargeur : uniquement lecture de config, journal et patchs mémoire. Le réseau
    // démarre plus tard, depuis la boucle de jeu.
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        g_self = instance;
        Install();
    }
    return TRUE;
}
