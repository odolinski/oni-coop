#include "textconsole.h"

#include <cstdio>
#include <deque>
#include <string>

#include "clock.h"
#include "config.h"
#include "log.h"
#include "memory.h"
#include "network.h"
#include "oni.h"
#include "scriptsync.h"
#include "world.h"

using namespace oni;

namespace textcon {

// Un script n'en demande qu'un à la fois ; plusieurs peuvent s'accumuler derrière un menu ouvert.
static const size_t kMaxPending = 8;

// Hôte : une console de données activée dans ce délai avant l'écran qu'un script demande en est
// la cause, et l'écran n'est que pour le joueur qui l'a activée.
static const double kConsoleWindow = 1.5;

static void* g_original = nullptr;         // commande du moteur, une fois détournée
static std::deque<std::string> g_pending;  // écrans demandés, pas encore ouverts
static std::string g_open;                 // celui qui est à l'écran
static unsigned g_shown = 0;
// Hôte : écrans que ses scripts viennent de demander, pas encore attribués à leurs lecteurs.
struct Request {
    std::string name;
    double at;
};
static std::deque<Request> g_requests;
static unsigned g_private = 0;             // hôte : écrans montrés à un seul joueur

static bool Wanted(const char* name) {
    if (g_open == name) return true;
    for (const std::string& pending : g_pending) {
        if (pending == name) return true;
    }
    return false;
}

static uint16_t ONICALL ONIMP_ENTRY TextConsoleHook(sl_callinfo* info, uint32_t numargs, sl_arg* args, int* ticks,
                                                    int* stall, sl_arg* result) {
    const char* name = numargs == 1 && args ? args[0].val.value_str32 : nullptr;
    // Hors session rien ne tourne derrière un dialogue : l'écran s'ouvre sur place, comme dans le
    // jeu d'origine, et le script attend sa fermeture.
    if (!net::Running() || !name || !*name) {
        return reinterpret_cast<sl_func>(g_original)(info, numargs, args, ticks, stall, result);
    }
    // Hôte, d'autres joueurs étant là : à qui le montrer se décide une fois connu ce qui l'a
    // demandé (voir Resolve). Chez un client, l'écran vient de l'hôte, qui a déjà choisi.
    if (net::CurrentMode() == cfg::Mode::Host && net::Connected()) {
        for (const Request& request : g_requests) {
            if (request.name == name) return 0;
        }
        if (g_requests.size() < kMaxPending) g_requests.push_back({name, clk::Seconds()});
        return 0;
    }
    if (Wanted(name)) return 0;  // déjà demandé, ou déjà sous les yeux du joueur
    if (g_pending.size() >= kMaxPending) {
        LOG("ecran de texte : '%s' abandonne, %u en attente", name, static_cast<unsigned>(g_pending.size()));
        return 0;
    }
    g_pending.push_back(name);
    LOG("ecran de texte : '%s' demande, le script continue", name);
    return 0;
}

bool Install() {
    return mem::HookEntry(addr::kTextConsole, addr::kTextConsoleEntry, sizeof(addr::kTextConsoleEntry),
                          reinterpret_cast<void*>(&TextConsoleHook), &g_original);
}

void OnLevelChanged() {
    g_pending.clear();
    g_requests.clear();
}

static void Queue(const std::string& name) {
    if (Wanted(name.c_str()) || g_pending.size() >= kMaxPending) return;
    g_pending.push_back(name);
}

// Les écrans de texte sont de deux sortes. Ceux qu'un script du niveau montre de lui-même
// (consignes, messages de l'état-major) regardent toute l'équipe. Ceux d'une console de données
// qu'un joueur vient d'utiliser ne regardent que lui : jusqu'à la 0.24 ils s'ouvraient chez tous,
// et coupaient les trois autres dans ce qu'ils faisaient. L'hôte, dont les scripts décident, sait
// qui a activé quelle console (world.cpp) : l'écran demandé dans la seconde qui suit n'est ouvert
// que chez ce joueur. Le reste du script de la console (objectif, porte) vaut toujours pour tous.
void Resolve(double now) {
    while (!g_requests.empty()) {
        Request request = g_requests.front();
        g_requests.pop_front();
        int reader = world::TakeConsoleUser(request.at - kConsoleWindow);
        if (reader < 0) {
            LOG("ecran de texte : '%s' demande par un script, pour tous les joueurs", request.name.c_str());
            Queue(request.name);
            script::SendScreen(request.name.c_str(), -1);
        } else if (reader == net::LocalPlayerId()) {
            LOG("ecran de texte : '%s' demande par la console que notre joueur vient d'utiliser, pour lui seul",
                request.name.c_str());
            Queue(request.name);
            g_private++;
        } else {
            LOG("ecran de texte : '%s' demande par la console que le joueur %d vient d'utiliser, pour lui seul",
                request.name.c_str(), reader);
            script::SendScreen(request.name.c_str(), reader);
            g_private++;
        }
    }
    (void)now;
}

void Update() {
    if (!g_original || !g_open.empty() || g_pending.empty()) return;
    if (!mem::Game() || !mem::Player()) {
        g_pending.clear();
        return;
    }
    g_open = g_pending.front();
    g_pending.pop_front();

    // La chaîne doit rester valide et modifiable pendant l'appel.
    std::string text = g_open;
    text.push_back('\0');
    sl_arg args[1] = {};
    args[0].type = sl_str32;
    args[0].val.value_str32 = &text[0];
    static char command[] = "text_console";
    static char location[] = "OniMP";
    struct {
        sl_callinfo info;
        uint32_t padding[16];
    } context = {};
    context.info.name = command;
    context.info.calllocation = location;
    int ticks = 0;
    int stall = 0;
    sl_arg result = {};
    result.type = sl_void;

    // Le tremplin, et non l'adresse détournée : c'est la commande d'origine, qui ouvre le dialogue
    // et ne revient qu'à sa fermeture. Le jeu et notre boucle tournent pendant ce temps depuis la
    // boucle des dialogues ; un écran demandé d'ici là attend son tour.
    LOG("ecran de texte : '%s' ouvert", g_open.c_str());
    uint16_t error = reinterpret_cast<sl_func>(g_original)(&context.info, 1, args, &ticks, &stall, &result);
    if (error) LOG("ecran de texte : '%s' refuse par le moteur (%u)", g_open.c_str(), error);
    else g_shown++;
    g_open.clear();
}

void Describe(char* out, int capacity) {
    snprintf(out, static_cast<size_t>(capacity), "ecrans de texte : %u ouvert(s), %u en attente, %u pour un seul joueur%s",
             g_shown, static_cast<unsigned>(g_pending.size()), g_private,
             g_original ? "" : " (ouverts par les scripts, code inattendu)");
}

}  // namespace textcon
