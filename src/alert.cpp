#include "alert.h"

#include <windows.h>

#include <mmsystem.h>

#include <cstdio>
#include <string>

#include "config.h"
#include "log.h"
#include "memory.h"
#include "network.h"

namespace alert {

static const char* const kAlias = "onimp_alert";

static bool g_opened = false;       // le fichier est ouvert par Windows, prêt à jouer
static bool g_failed = false;       // introuvable ou refusé : on n'insiste pas
static std::string g_path;
static int g_played = 0;

// Commande MCI ; faux si Windows la refuse, et le journal dit pourquoi.
static bool Command(const char* command) {
    MCIERROR error = mciSendStringA(command, nullptr, 0, nullptr);
    if (!error) return true;
    char text[160] = "";
    mciGetErrorStringA(error, text, sizeof(text));
    LOG("son : commande '%s' refusee par Windows : %s", command, text);
    return false;
}

static std::string Find() {
    const std::string& game = cfg::GameDir();
    // Le dossier OniMP est à côté du jeu, ou dedans quand le jeu est à la racine de l'AE.
    const std::string candidates[] = {game + "onimp_alert.mp3", game + "OniMP\\sounds\\AlertFall.mp3",
                                      game + "..\\OniMP\\sounds\\AlertFall.mp3"};
    for (const std::string& candidate : candidates) {
        if (GetFileAttributesA(candidate.c_str()) == INVALID_FILE_ATTRIBUTES) continue;
        char full[MAX_PATH] = {};
        if (!GetFullPathNameA(candidate.c_str(), MAX_PATH, full, nullptr)) return candidate;
        return full;
    }
    return "";
}

static void ApplyVolume() {
    if (!g_opened) return;
    char command[64];
    snprintf(command, sizeof(command), "setaudio %s volume to %d", kAlias, cfg::Get().alertVolume * 10);  // de 0 à 1000
    Command(command);
}

// Ouvert une fois, dès que la partie tourne : Windows met un instant à préparer la lecture, et cet
// instant ne doit pas tomber quand un joueur tombe.
static bool Open() {
    if (g_opened) return true;
    if (g_failed) return false;
    g_path = Find();
    if (g_path.empty()) {
        g_failed = true;
        LOG("son : alerte introuvable (onimp_alert.mp3 a cote d'Oni.exe, ou OniMP\\sounds\\AlertFall.mp3) : pas de son");
        return false;
    }
    char command[MAX_PATH + 64];
    snprintf(command, sizeof(command), "open \"%s\" type mpegvideo alias %s", g_path.c_str(), kAlias);
    if (!Command(command)) {
        g_failed = true;
        return false;
    }
    g_opened = true;
    ApplyVolume();
    LOG("son : alerte prete (%s, volume %d pour cent)", g_path.c_str(), cfg::Get().alertVolume);
    return true;
}

// Plusieurs fenêtres du jeu peuvent tourner sur une même machine, et chacune voit le joueur
// tomber : seule celle qui a le clavier le fait entendre, une fois et non deux.
static bool Foreground() {
    HWND window = GetForegroundWindow();
    DWORD process = 0;
    if (!window) return false;
    GetWindowThreadProcessId(window, &process);
    return process == GetCurrentProcessId();
}

// Une fois, depuis le début, sans boucle : il s'arrête de lui-même à la fin du fichier. Relancé
// pendant qu'il joue (un second joueur tombe), il repart du début.
static void Play(const char* why) {
    int volume = cfg::Get().alertVolume;
    if (volume <= 0) {
        LOG("son : alerte muette, volume a zero (%s)", why);
        return;
    }
    if (!Foreground()) {
        LOG("son : alerte laissee a la fenetre du jeu qui a le clavier (%s)", why);
        return;
    }
    if (!Open()) return;
    char command[64];
    snprintf(command, sizeof(command), "play %s from 0", kAlias);
    if (!Command(command)) {
        g_failed = true;  // un fichier que Windows ne sait pas lire : une ligne au journal, pas une par joueur
        return;
    }
    g_played++;
    LOG("son : alerte jouee une fois, volume %d pour cent (%s)", volume, why);
}

void Update() {
    if (!g_opened && !g_failed && net::Running() && mem::Player()) Open();
}

void PlayerDown(uint32_t player) {
    char why[40];
    snprintf(why, sizeof(why), "joueur %u a terre", static_cast<unsigned>(player));
    Play(why);
}

void Preview() { Play("reglage du volume"); }

int Volume() { return cfg::Get().alertVolume; }

void SetVolume(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    if (percent == cfg::Get().alertVolume) return;
    cfg::Get().alertVolume = percent;
    ApplyVolume();
}

void Save() {
    cfg::SaveAudio();
    LOG("reglage : volume de l'alerte %d pour cent, garde dans onimp.ini", cfg::Get().alertVolume);
}

void Describe(char* out, int capacity) {
    snprintf(out, static_cast<size_t>(capacity), "alerte : %s, volume %d %%, jouee %d fois",
             g_failed ? "fichier introuvable ou refuse" : (g_opened ? "prete" : "pas encore ouverte"),
             cfg::Get().alertVolume, g_played);
}

}  // namespace alert
