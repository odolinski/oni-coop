#include "scriptsync.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "levelsync.h"
#include "log.h"
#include "memory.h"
#include "network.h"
#include "oni.h"
#include "protocol.h"
#include "scriptcodec.h"
#include "stub.h"

using namespace oni;

namespace script {

// Les quatre appels du moteur à la routine commune d'enregistrement d'une commande (0x478950).
static const uintptr_t kRegisterCallSites[] = {0x004777EA, 0x00477B2F, 0x00477B4C, 0x0047824A};
static const uintptr_t kRegisterTarget = 0x00478950;
// L'interpréteur rappelle une commande bloquante à chaque tick depuis ce second site (0x47B647) :
// ces rappels ne sont pas de nouvelles exécutions.
static const uintptr_t kRepollReturnAddress = 0x0047B64A;

static const int kMaxCommands = 4096;
static const size_t kMaxLogEntries = 6000;
static const double kDeferredTimeout = 60.0;   // commande d'un autre niveau : abandonnée après ce délai

typedef uint16_t(ONICALL* RegisterFunction)(const char* name, const char* description, const char* argFormat,
                                            int32_t returnType, sl_func callback);

struct Entry {
    std::string name;
    sl_func original;
    codec::Class cls;
    bool screen;   // text_console : montré à qui de droit, voir SendScreen
};

struct Deferred {
    codec::Command command;
    double received;
};

static RegisterFunction g_register = nullptr;
static std::vector<Entry> g_entries;
static std::unordered_map<std::string, int> g_byName;
static uint8_t* g_stubs = nullptr;
// Écrit par le stub propre à chaque commande, lu en entrée de CommandWrapper.
static volatile int g_currentCommand = 0;

static bool g_replaying = false;  // commande reçue de l'hôte en cours d'exécution

// Hôte : commandes d'état exécutées depuis le début du niveau, déjà sérialisées.
static std::vector<std::string> g_log;
static uint32_t g_logLevel = 0;
static int32_t g_logTime = 0;      // horloge de jeu à la dernière commande enregistrée
static int32_t g_seenTime = 0;     // horloge de jeu la plus récente vue, à une image ou à une commande
static bool g_logFull = false;
static unsigned g_forwarded = 0;

// Client
static std::deque<Deferred> g_deferred;
static unsigned g_executed = 0;
static unsigned g_suppressed = 0;
static double g_now = 0.0;

// Commandes passées sur chaque connexion en cours. Hôte : envoyées à ce client, par numéro de
// joueur. Client : reçues de l'hôte, à la case 0.
static uint32_t g_connectionCount[proto::kMaxPlayers] = {};

static bool Enabled() { return cfg::Get().quests && cfg::Get().mode != cfg::Mode::Off; }

// Le flux fiable est partagé : chaque message y commence par son genre. player : le seul client
// destinataire ; négatif : tous.
static bool SendCommand(const std::string& payload, int player = -1) {
    std::string message(1, static_cast<char>(proto::kReliableScript));
    message += payload;
    bool sent = false;
    for (int client = 1; client < proto::kMaxPlayers; client++) {
        if (player >= 0 && client != player) continue;
        if (!net::SendReliableTo(static_cast<uint8_t>(client), message.data(), static_cast<int>(message.size()))) continue;
        g_connectionCount[client]++;
        sent = true;
    }
    return sent;
}

// -------------------------------------------------------------------------------------------------
// Hôte : enregistrement et envoi
// -------------------------------------------------------------------------------------------------
static void Record(const Entry& entry, uint32_t numargs, const sl_arg* args) {
    uint32_t level = mem::LevelHash();
    if (level == 0) return;  // moteur en cours d'initialisation
    // Nouveau niveau, ou le même rechargé (son horloge est repartie de zéro) : les premières
    // commandes arrivent pendant le chargement, avant toute frame.
    // L'horloge se compare aussi à ce qu'elle valait à la dernière image : un point de sauvegarde
    // qui ne rejoue ses commandes qu'au chargement les enregistre toutes à l'instant zéro, et d'un
    // rechargement au suivant cet instant n'était jamais « avant » le précédent. Le journal gardait
    // alors les commandes de chaque reprise, et un client les rejouait toutes : un Barabas de plus
    // par rechargement (niveau lab, 0.27 : 16, 32, 48... jusqu'à 176 commandes rejouées).
    int32_t time = mem::GameTime();
    if (level != g_logLevel || time < g_logTime || time < g_seenTime) {
        if (!g_log.empty()) {
            LOG("quetes : niveau recharge, journal des %u commande(s) d'etat vide avant de reprendre",
                static_cast<unsigned>(g_log.size()));
        }
        g_log.clear();
        g_logLevel = level;
        g_logFull = false;
        g_seenTime = time;
    }
    g_logTime = time;

    codec::Command command;
    command.levelHash = level;
    command.name = entry.name;
    if (numargs > codec::kMaxArgs) return;
    for (uint32_t i = 0; i < numargs; i++) {
        codec::Arg arg;
        switch (args[i].type) {
            case sl_int32:
                arg.type = codec::kInt;
                arg.integer = args[i].val.value_int32;
                break;
            case sl_float:
                arg.type = codec::kFloat;
                arg.real = args[i].val.value_float;
                break;
            case sl_bool:
                arg.type = codec::kBool;
                arg.integer = args[i].val.value_bool ? 1 : 0;
                break;
            case sl_str32:
                arg.type = codec::kString;
                arg.text = args[i].val.value_str32 ? args[i].val.value_str32 : "";
                break;
            default:
                return;  // type inconnu : on ne transmet pas une commande qu'on ne sait pas rejouer
        }
        command.args.push_back(arg);
    }

    std::string payload;
    if (!codec::Encode(command, payload)) return;

    if (entry.cls == codec::Class::State) {
        if (g_log.size() < kMaxLogEntries) {
            g_log.push_back(payload);
        } else if (!g_logFull) {
            g_logFull = true;
            LOG("quetes : journal de niveau plein, un client arrive plus tard sera incomplet");
        }
    }
    if (net::Connected() && SendCommand(payload)) g_forwarded++;
}

void SendScreen(const char* name, int player) {
    if (!Enabled() || cfg::Get().mode != cfg::Mode::Host || !net::Connected()) return;
    codec::Command command;
    command.levelHash = mem::LevelHash();
    command.name = "text_console";
    codec::Arg arg;
    arg.type = codec::kString;
    arg.text = name;
    command.args.push_back(arg);
    std::string payload;
    if (codec::Encode(command, payload) && SendCommand(payload, player)) g_forwarded++;
}

uint32_t StreamCount(uint8_t player) {
    return Enabled() && player < proto::kMaxPlayers ? g_connectionCount[player] : 0;
}

void OnConnection(uint8_t player, bool connected) {
    if (player < proto::kMaxPlayers) g_connectionCount[player] = 0;
    if (!Enabled()) return;
    if (cfg::Get().mode == cfg::Mode::Host) {
        if (!connected) return;
        // Ce client vient d'arriver : il lui manque tout ce que les scripts ont déjà fait ici. Les
        // autres l'ont déjà : à lui seul.
        int sent = 0;
        if (g_logLevel == mem::LevelHash()) {
            for (const std::string& payload : g_log) {
                if (SendCommand(payload, player)) sent++;
            }
        }
        LOG("quetes : %d commande(s) d'etat rejouee(s) au client %u", sent, player);
    } else {
        g_deferred.clear();
    }
}

// -------------------------------------------------------------------------------------------------
// Client : réception et exécution
// -------------------------------------------------------------------------------------------------
void OnReliable(uint8_t from, const uint8_t* payload, int size) {
    if (!Enabled() || cfg::Get().mode != cfg::Mode::Client || from != proto::kHostPlayerId) return;
    g_connectionCount[proto::kHostPlayerId]++;
    Deferred deferred;
    if (!codec::Decode(payload, size, deferred.command)) {
        LOG("quetes : commande illisible (%d octets)", size);
        return;
    }
    deferred.received = g_now;
    g_deferred.push_back(deferred);
}

// Ordre donné à un personnage nommé (voir scriptcodec.cpp) : ici il ne vaut que pour notre joueur,
// que les scripts appellent 0 ou par son nom.
static bool OrderForOthers(const codec::Command& command) {
    const std::string& name = command.name;
    if (name != "playback" && name != "ai2_movetoflag" && name != "ai2_lookatchar" && name != "ai2_setmovementmode") {
        return false;
    }
    if (command.args.empty()) return true;
    const codec::Arg& who = command.args[0];
    if (who.type == codec::kInt) return who.integer != 0;
    if (who.type != codec::kString) return true;
    Character* player = mem::Player();
    return who.text != "0" && !(player && who.text == mem::Name(player));
}

static void Execute(const codec::Command& command) {
    auto found = g_byName.find(command.name);
    if (found == g_byName.end()) {
        LOG("quetes : commande inconnue '%s'", command.name.c_str());
        return;
    }
    const Entry& entry = g_entries[static_cast<size_t>(found->second)];
    // Un hôte d'une autre version ne décide pas de nos règles.
    if (entry.cls == codec::Class::None || entry.cls == codec::Class::HostOnly) return;
    if (OrderForOthers(command)) return;

    // Les chaînes doivent rester valides et modifiables pendant l'appel.
    std::vector<std::string> texts(command.args.size());
    sl_arg args[codec::kMaxArgs] = {};
    std::string line = command.name;
    for (size_t i = 0; i < command.args.size(); i++) {
        const codec::Arg& arg = command.args[i];
        char text[48];
        switch (arg.type) {
            case codec::kString:
                texts[i] = arg.text;
                texts[i].push_back('\0');
                args[i].type = sl_str32;
                args[i].val.value_str32 = &texts[i][0];
                line += " " + arg.text;
                break;
            case codec::kFloat:
                args[i].type = sl_float;
                args[i].val.value_float = arg.real;
                snprintf(text, sizeof(text), " %g", arg.real);
                line += text;
                break;
            case codec::kBool:
                args[i].type = sl_bool;
                args[i].val.value_int32 = 0;
                args[i].val.value_bool = arg.integer != 0;
                line += arg.integer ? " 1" : " 0";
                break;
            default:
                args[i].type = sl_int32;
                args[i].val.value_int32 = arg.integer;
                snprintf(text, sizeof(text), " %d", arg.integer);
                line += text;
                break;
        }
    }
    if (cfg::Get().debug) LOG("quetes : << %s", line.c_str());

    // Contexte d'appel minimal ; les sorties (durée, blocage, valeur de retour) sont ignorées :
    // une commande bloquante démarre son effet sans rien bloquer ici.
    static char location[] = "OniMP";
    struct {
        sl_callinfo info;
        uint32_t padding[16];
    } context = {};
    context.info.name = const_cast<char*>(entry.name.c_str());
    context.info.calllocation = location;
    int ticks = 0;
    int stall = 0;
    sl_arg result = {};
    result.type = sl_void;

    g_replaying = true;
    entry.original(&context.info, static_cast<uint32_t>(command.args.size()), args, &ticks, &stall, &result);
    g_replaying = false;
    g_executed++;
}

void Update(double now) {
    g_now = now;
    // Hôte : niveau rechargé sans qu'aucune commande d'état n'ait encore été enregistrée depuis.
    // Ce que le journal garde est d'avant le rechargement : un client qui arrive ne doit pas le
    // rejouer.
    int32_t time = mem::GameTime();
    if (time < g_seenTime && !g_log.empty()) {
        LOG("quetes : niveau recharge, journal des %u commande(s) d'etat vide", static_cast<unsigned>(g_log.size()));
        g_log.clear();
        g_logFull = false;
        g_logTime = time;
    }
    g_seenTime = time;
    if (g_deferred.empty()) return;
    uint32_t level = mem::LevelHash();
    while (!g_deferred.empty()) {
        Deferred& front = g_deferred.front();
        if (front.command.levelHash != level) {
            // L'hôte est dans un autre niveau (il a de l'avance, ou nous en changeons) : attendre.
            if (now - front.received < kDeferredTimeout) break;
            g_deferred.pop_front();
            continue;
        }
        codec::Command command = front.command;
        g_deferred.pop_front();
        Execute(command);
    }
}

// -------------------------------------------------------------------------------------------------
// Interception des commandes
// -------------------------------------------------------------------------------------------------
static uint16_t ONICALL ONIMP_ENTRY CommandWrapper(sl_callinfo* info, uint32_t numargs, sl_arg* args, int* ticks,
                                                   int* stall, sl_arg* result) {
    const Entry& entry = g_entries[static_cast<size_t>(g_currentCommand)];
    bool firstCall = reinterpret_cast<uintptr_t>(__builtin_return_address(0)) != kRepollReturnAddress;

    // mem::InsideExec : commande lancée par OniMP lui-même, ni transmise ni bloquée.
    bool ours = firstCall && entry.cls != codec::Class::None && !mem::InsideExec() && !g_replaying && Enabled();
    bool hostOnly = entry.cls == codec::Class::HostOnly;
    if (ours && cfg::Get().mode == cfg::Mode::Client) {
        // Point de sauvegarde : celui de l'hôte vaut pour tous. Un client relié ne sauvegarde rien de
        // lui-même ; coupé de l'hôte, il garde les siens.
        if (hostOnly) {
            if (!net::Connected()) return entry.original(info, numargs, args, ticks, stall, result);
            level::OnLocalSaveSkipped(numargs >= 1 && args[0].type == sl_int32 ? args[0].val.value_int32 : -1);
        }
        // Les scripts locaux continuent de tourner, mais leurs effets viennent de l'hôte.
        g_suppressed++;
        return 0;
    }
    // Un écran de texte n'est pas transmis d'office : celui d'une console de données n'est que pour
    // le joueur qui l'a utilisée (voir textconsole.cpp).
    if (ours && !hostOnly && !entry.screen) Record(entry, numargs, args);
    uint16_t status = entry.original(info, numargs, args, ticks, stall, result);
    if (ours && hostOnly) level::OnHostSaved();
    return status;
}

// Un stub par commande : note son numéro puis saute dans CommandWrapper (voir stub.h).
static sl_func MakeStub(int index) {
    uint8_t* code = g_stubs + index * stub::kSize;
    stub::Write(code, index, &g_currentCommand, reinterpret_cast<void*>(&CommandWrapper));
    return reinterpret_cast<sl_func>(code);
}

static uint16_t ONICALL ONIMP_ENTRY RegisterHook(const char* name, const char* description, const char* argFormat,
                                                 int32_t returnType, sl_func callback) {
    if (!name || !callback || static_cast<int>(g_entries.size()) >= kMaxCommands) {
        return g_register(name, description, argFormat, returnType, callback);
    }
    int index = static_cast<int>(g_entries.size());
    // Les commandes d'OniMP et de Daodan (mp_*, d_*) ne sont dans aucune règle : jamais transmises.
    g_entries.push_back({name, callback, codec::Classify(name), strcmp(name, "text_console") == 0});
    g_byName[name] = index;
    return g_register(name, description, argFormat, returnType, MakeStub(index));
}

bool Install() {
    for (uintptr_t site : kRegisterCallSites) {
        if (mem::CallTarget(site) != reinterpret_cast<void*>(kRegisterTarget)) {
            LOG("quetes : site %08X inattendu, synchronisation des scripts indisponible",
                static_cast<unsigned>(site));
            return false;
        }
    }
    g_stubs = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, kMaxCommands * stub::kSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!g_stubs) return false;
    g_entries.reserve(kMaxCommands);

    g_register = reinterpret_cast<RegisterFunction>(kRegisterTarget);
    for (uintptr_t site : kRegisterCallSites) {
        if (!mem::PatchCall(site, reinterpret_cast<void*>(&RegisterHook), nullptr)) return false;
    }
    return true;
}

void Describe(char* out, int capacity) {
    if (!Enabled()) {
        snprintf(out, static_cast<size_t>(capacity), "quetes=off");
    } else if (cfg::Get().mode == cfg::Mode::Host) {
        snprintf(out, static_cast<size_t>(capacity), "quetes=hote journal=%u transmises=%u en attente=%d",
                 static_cast<unsigned>(g_log.size()), g_forwarded, net::ReliableBacklog());
    } else {
        snprintf(out, static_cast<size_t>(capacity), "quetes=client executees=%u bloquees=%u en attente=%u",
                 g_executed, g_suppressed, static_cast<unsigned>(g_deferred.size()));
    }
}

void DumpCommands() {
    if (g_entries.empty()) return;  // interception non installée
    int state = 0, transient = 0;
    std::string line;
    for (const Entry& entry : g_entries) {
        if (entry.cls == codec::Class::None) continue;
        if (entry.cls == codec::Class::State) state++;
        else if (entry.cls == codec::Class::Transient) transient++;
    }
    LOG("quetes : %u commandes de script interceptees (%d d'etat, %d ponctuelles)",
        static_cast<unsigned>(g_entries.size()), state, transient);
    if (!cfg::Get().debug) return;
    for (int pass = 0; pass < 3; pass++) {
        codec::Class wanted = pass == 0 ? codec::Class::State : (pass == 1 ? codec::Class::Transient : codec::Class::None);
        line.clear();
        for (const Entry& entry : g_entries) {
            if (entry.cls != wanted) continue;
            line += entry.name + " ";
            if (line.size() > 900) {
                LOG("quetes : [%s] %s", pass == 0 ? "etat" : (pass == 1 ? "ponctuel" : "local"), line.c_str());
                line.clear();
            }
        }
        if (!line.empty()) LOG("quetes : [%s] %s", pass == 0 ? "etat" : (pass == 1 ? "ponctuel" : "local"), line.c_str());
    }
}

}  // namespace script
