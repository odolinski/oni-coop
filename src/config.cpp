#include "config.h"

#include <windows.h>

#include <cstdlib>
#include <cstring>

#include "rules.h"

namespace cfg {

static Config g_config;
static std::string g_gameDir;

Config& Get() { return g_config; }

const std::string& GameDir() {
    if (g_gameDir.empty()) {
        char path[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        char* slash = strrchr(path, '\\');
        if (slash) slash[1] = '\0';
        g_gameDir = path;
    }
    return g_gameDir;
}

const char* ModeName(Mode mode) {
    switch (mode) {
        case Mode::Host: return "host";
        case Mode::Client: return "client";
        default: return "off";
    }
}

static bool ParsePort(const char* text, uint16_t& port) {
    char* end = nullptr;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < 1 || value > 65535) return false;
    port = static_cast<uint16_t>(value);
    return true;
}

bool ParseSpec(const char* spec, Config& out) {
    std::string s = spec ? spec : "";
    if (s.empty()) return false;

    if (s == "off") {
        out.mode = Mode::Off;
        return true;
    }
    if (s.rfind("host", 0) == 0) {
        if (s.size() > 4) {
            if (s[4] != ':' || !ParsePort(s.c_str() + 5, out.port)) return false;
        }
        out.mode = Mode::Host;
        return true;
    }
    if (s.rfind("join", 0) == 0) {
        if (s.size() > 4) {
            if (s[4] != ':' || !ParsePort(s.c_str() + 5, out.port)) return false;
        }
        out.host = "auto";
        out.mode = Mode::Client;
        return true;
    }
    if (s.rfind("connect:", 0) == 0) {
        std::string rest = s.substr(8);
        size_t colon = rest.rfind(':');
        if (colon != std::string::npos) {
            if (!ParsePort(rest.c_str() + colon + 1, out.port)) return false;
            rest.resize(colon);
        }
        if (rest.empty()) return false;
        out.host = rest;
        out.mode = Mode::Client;
        return true;
    }
    return false;
}

static std::string IniString(const char* ini, const char* section, const char* key, const char* fallback) {
    char buffer[256] = {};
    GetPrivateProfileStringA(section, key, fallback, buffer, sizeof(buffer), ini);
    return buffer;
}

static bool IniBool(const char* ini, const char* section, const char* key, bool fallback) {
    std::string value = IniString(ini, section, key, fallback ? "true" : "false");
    return value == "1" || value == "true" || value == "yes" || value == "on";
}

// Réglages propres à chaque joueur (affichage, langue, volume). L'hôte et le client d'une même
// machine partagent onimp.ini : l'hôte a la section telle quelle, un client la même suivie de
// « _client ». Ce qu'un client n'a pas encore réglé lui-même vaut ce que dit la section de l'hôte.
static std::string OwnSection(const char* section) {
    return g_config.mode == Mode::Client ? std::string(section) + "_client" : section;
}

static bool OwnBool(const char* ini, const char* section, const char* key, bool fallback) {
    bool common = IniBool(ini, section, key, fallback);
    return g_config.mode == Mode::Client ? IniBool(ini, OwnSection(section).c_str(), key, common) : common;
}

static std::string OwnString(const char* ini, const char* section, const char* key, const char* fallback) {
    std::string common = IniString(ini, section, key, fallback);
    return g_config.mode == Mode::Client ? IniString(ini, OwnSection(section).c_str(), key, common.c_str()) : common;
}

void Load() {
    std::string ini = GameDir() + "onimp.ini";
    Config& c = g_config;

    std::string mode = IniString(ini.c_str(), "net", "mode", "off");
    if (mode == "host") c.mode = Mode::Host;
    else if (mode == "client") c.mode = Mode::Client;
    else c.mode = Mode::Off;

    c.host = IniString(ini.c_str(), "net", "host", c.host.c_str());
    int port = static_cast<int>(GetPrivateProfileIntA("net", "port", c.port, ini.c_str()));
    if (port >= 1 && port <= 65535) c.port = static_cast<uint16_t>(port);
    int rate = static_cast<int>(GetPrivateProfileIntA("net", "rate", c.rate, ini.c_str()));
    if (rate >= 5 && rate <= 120) c.rate = rate;

    c.gameName = IniString(ini.c_str(), "net", "name", c.gameName.c_str());
    c.joinName = IniString(ini.c_str(), "net", "join", c.joinName.c_str());
    c.netInterface = IniString(ini.c_str(), "net", "interface", c.netInterface.c_str());

    c.playerClass = IniString(ini.c_str(), "player", "class", c.playerClass.c_str());
    c.npcs = IniBool(ini.c_str(), "sync", "npcs", c.npcs);
    c.quests = IniBool(ini.c_str(), "sync", "quests", c.quests);
    c.deleteOrphans = IniBool(ini.c_str(), "sync", "delete_orphans", c.deleteOrphans);
    c.debug = IniBool(ini.c_str(), "debug", "trace", c.debug);
    c.multiInstance = IniBool(ini.c_str(), "game", "allow_multiple_instances", c.multiInstance);
    c.onlineDamage = IniBool(ini.c_str(), "game", "online_damage", c.onlineDamage);
    c.difficulty = rules::ModeFromKey(IniString(ini.c_str(), "game", "difficulty", "").c_str(), c.difficulty);
    int enemies = static_cast<int>(GetPrivateProfileIntA("game", "enemies", c.enemies, ini.c_str()));
    if (enemies >= rules::kMinEnemies && enemies <= rules::kMaxEnemies) c.enemies = enemies;
    c.civilians = IniBool(ini.c_str(), "game", "civilians", c.civilians);
    c.runInMenus = IniBool(ini.c_str(), "game", "run_in_menus", c.runInMenus);
    c.joinReload = IniBool(ini.c_str(), "game", "join_reload", c.joinReload);
    c.readyCheck = IniBool(ini.c_str(), "game", "ready_check", c.readyCheck);
    c.teamKill = IniBool(ini.c_str(), "game", "team_kill", c.teamKill);
    c.cheats = IniBool(ini.c_str(), "game", "cheats", c.cheats);
    int access = static_cast<int>(GetPrivateProfileIntA("game", "cheat_access", c.cheatAccess, ini.c_str()));
    if (access >= 0 && access <= 0xE) c.cheatAccess = access & 0xE;  // jamais le bit de l'hôte : il a tout

    // Prioritaire sur l'ini : permet deux instances dans le même dossier.
    char spec[128] = {};
    if (GetEnvironmentVariableA("ONIMP", spec, sizeof(spec)) > 0) ParseSpec(spec, c);
    char flag[8] = {};
    if (GetEnvironmentVariableA("ONIMP_NPCS", flag, sizeof(flag)) > 0) c.npcs = flag[0] == '1';
    if (GetEnvironmentVariableA("ONIMP_QUESTS", flag, sizeof(flag)) > 0) c.quests = flag[0] == '1';
    if (GetEnvironmentVariableA("ONIMP_INTERFACE", spec, sizeof(spec)) > 0) c.netInterface = spec;

    // Les scripts de l'hôte font apparaître les PNJ chez le client : sans pilotage par l'hôte,
    // ils y vivraient leur propre vie.
    if (c.quests) c.npcs = true;

    // Titre de la fenêtre, donné par les scripts de lancement pour distinguer deux fenêtres du jeu.
    char title[64] = {};
    if (GetEnvironmentVariableA("ONIMP_TITLE", title, sizeof(title)) > 0) c.windowTitle = title;

    // Pseudo et tenue : une fois le rôle connu, chacun les siens.
    const char* role = c.mode == Mode::Client ? "client" : "host";
    c.playerName = IniString(ini.c_str(), "player", (std::string("name_") + role).c_str(), "");
    if (c.playerName.size() > 15) c.playerName.resize(15);
    c.playerSkin = static_cast<int>(GetPrivateProfileIntA("player", (std::string("skin_") + role).c_str(), -1, ini.c_str()));

    // Affichage, langue et volume : à chacun les siens aussi (voir OwnSection).
    c.showPlayerHealth = OwnBool(ini.c_str(), "display", "player_health", c.showPlayerHealth);
    c.showNpcHealth = OwnBool(ini.c_str(), "display", "npc_health", c.showNpcHealth);
    c.showMessages = OwnBool(ini.c_str(), "display", "messages", c.showMessages);
    c.showNpcIds = OwnBool(ini.c_str(), "display", "npc_ids", c.showNpcIds);
    c.showWeaponIds = OwnBool(ini.c_str(), "display", "weapon_ids", c.showWeaponIds);
    c.showPlayerAmmo = OwnBool(ini.c_str(), "display", "player_ammo", c.showPlayerAmmo);
    c.showPing = OwnBool(ini.c_str(), "display", "ping", c.showPing);
    c.showThroughWalls = OwnBool(ini.c_str(), "display", "wallhack", c.showThroughWalls);
    c.lowPortraits = OwnBool(ini.c_str(), "display", "portraits_bottom", c.lowPortraits);
    c.autoPlay = c.mode == Mode::Client && IniBool(ini.c_str(), "game_client", "autoplay", false);
    c.language = OwnString(ini.c_str(), "display", "language", c.language.c_str());
    int volume = atoi(OwnString(ini.c_str(), "audio", "alert_volume", std::to_string(c.alertVolume).c_str()).c_str());
    if (volume >= 0 && volume <= 100) c.alertVolume = volume;

    // Cheats : ceux de l'hôte et ceux d'un client ne se mêlent pas, même dans un fichier partagé.
    std::string cheats = OwnSection("cheats");
    c.cheatGod = IniBool(ini.c_str(), cheats.c_str(), "god_mode", c.cheatGod);
    c.cheatAmmo = IniBool(ini.c_str(), cheats.c_str(), "unlimited_ammo", c.cheatAmmo);
    c.cheatOneHit = IniBool(ini.c_str(), cheats.c_str(), "one_hit_kill", c.cheatOneHit);
    c.cheatAura = IniBool(ini.c_str(), cheats.c_str(), "permanent_aura", c.cheatAura);
    const struct {
        const char* key;
        int* value;
    } speeds[] = {{"attack_speed", &c.cheatAttackSpeed}, {"move_speed", &c.cheatMoveSpeed}, {"jump", &c.cheatJump}};
    for (const auto& entry : speeds) {
        int percent = static_cast<int>(GetPrivateProfileIntA(cheats.c_str(), entry.key, *entry.value, ini.c_str()));
        if (percent >= 100 && percent <= 400) *entry.value = percent;
    }
    c.character = IniString(ini.c_str(), cheats.c_str(), "character", c.character.c_str());
    c.characterKey = IniString(ini.c_str(), cheats.c_str(), "character_key", c.characterKey.c_str());
}

void SaveCheats() {
    std::string ini = GameDir() + "onimp.ini";
    std::string section = OwnSection("cheats");
    const Config& c = g_config;
    const struct {
        const char* key;
        bool value;
    } toggles[] = {{"god_mode", c.cheatGod}, {"unlimited_ammo", c.cheatAmmo}, {"one_hit_kill", c.cheatOneHit},
                   {"permanent_aura", c.cheatAura}};
    for (const auto& entry : toggles) {
        WritePrivateProfileStringA(section.c_str(), entry.key, entry.value ? "true" : "false", ini.c_str());
    }
    WritePrivateProfileStringA(section.c_str(), "attack_speed", std::to_string(c.cheatAttackSpeed).c_str(), ini.c_str());
    WritePrivateProfileStringA(section.c_str(), "move_speed", std::to_string(c.cheatMoveSpeed).c_str(), ini.c_str());
    WritePrivateProfileStringA(section.c_str(), "jump", std::to_string(c.cheatJump).c_str(), ini.c_str());
    WritePrivateProfileStringA(section.c_str(), "character", c.character.c_str(), ini.c_str());
    WritePrivateProfileStringA(section.c_str(), "character_key", c.characterKey.c_str(), ini.c_str());
}

void SavePlayer() {
    std::string ini = GameDir() + "onimp.ini";
    const Config& c = g_config;
    const char* role = c.mode == Mode::Client ? "client" : "host";
    WritePrivateProfileStringA("player", (std::string("name_") + role).c_str(), c.playerName.c_str(), ini.c_str());
    WritePrivateProfileStringA("player", (std::string("skin_") + role).c_str(), std::to_string(c.playerSkin).c_str(),
                               ini.c_str());
}

void SaveDisplay() {
    std::string ini = GameDir() + "onimp.ini";
    std::string section = OwnSection("display");
    const Config& c = g_config;
    const struct {
        const char* key;
        bool value;
    } shown[] = {
        {"player_health", c.showPlayerHealth}, {"npc_health", c.showNpcHealth},   {"messages", c.showMessages},
        {"npc_ids", c.showNpcIds},             {"weapon_ids", c.showWeaponIds},   {"player_ammo", c.showPlayerAmmo},
        {"ping", c.showPing},                  {"wallhack", c.showThroughWalls},  {"portraits_bottom", c.lowPortraits},
    };
    for (const auto& entry : shown) {
        WritePrivateProfileStringA(section.c_str(), entry.key, entry.value ? "true" : "false", ini.c_str());
    }
    WritePrivateProfileStringA(section.c_str(), "language", c.language.c_str(), ini.c_str());
}

void SaveAudio() {
    std::string ini = GameDir() + "onimp.ini";
    WritePrivateProfileStringA(OwnSection("audio").c_str(), "alert_volume", std::to_string(g_config.alertVolume).c_str(),
                               ini.c_str());
}

std::string DifficultyPath() { return GameDir() + "onimp_difficulty.ini"; }

// Un chemin complet : sans dossier, Windows irait chercher le fichier dans le sien.
static std::string FullPath(const char* path) {
    char full[MAX_PATH] = {};
    if (!GetFullPathNameA(path, MAX_PATH, full, nullptr)) strncpy(full, path, MAX_PATH - 1);
    return full;
}

difficulty::Table LoadDifficulty(const char* path, bool* found) {
    std::string ini = FullPath(path);
    if (found) *found = GetFileAttributesA(ini.c_str()) != INVALID_FILE_ATTRIBUTES;
    difficulty::Table table = difficulty::DefaultTable();
    for (int clients = 1; clients <= difficulty::kMaxClients; clients++) {
        std::string key = "clients_" + std::to_string(clients);
        table.health[clients] = GetPrivateProfileIntA("health", key.c_str(), static_cast<int>(table.health[clients]),
                                                      ini.c_str());
        table.damage[clients] = GetPrivateProfileIntA("damage", key.c_str(), static_cast<int>(table.damage[clients]),
                                                      ini.c_str());
    }
    return difficulty::Sanitized(table);
}

uint64_t DifficultyStamp(const char* path) {
    WIN32_FILE_ATTRIBUTE_DATA data = {};
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) return 0;
    return (static_cast<uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
}

void SaveDifficulty(const char* path, const difficulty::Table& table) {
    std::string ini = FullPath(path);
    for (int clients = 1; clients <= difficulty::kMaxClients; clients++) {
        std::string key = "clients_" + std::to_string(clients);
        WritePrivateProfileStringA("health", key.c_str(), std::to_string(table.health[clients]).c_str(), ini.c_str());
        WritePrivateProfileStringA("damage", key.c_str(), std::to_string(table.damage[clients]).c_str(), ini.c_str());
    }
}

void SaveDifficultyMode() {
    std::string ini = GameDir() + "onimp.ini";
    WritePrivateProfileStringA("game", "difficulty", rules::Of(g_config.difficulty).key, ini.c_str());
}

void SaveAutoPlay() {
    std::string ini = GameDir() + "onimp.ini";
    WritePrivateProfileStringA("game_client", "autoplay", g_config.autoPlay ? "true" : "false", ini.c_str());
}

void SaveRules() {
    std::string ini = GameDir() + "onimp.ini";
    WritePrivateProfileStringA("game", "online_damage", g_config.onlineDamage ? "true" : "false", ini.c_str());
    WritePrivateProfileStringA("game", "difficulty", rules::Of(g_config.difficulty).key, ini.c_str());
    WritePrivateProfileStringA("game", "enemies", std::to_string(g_config.enemies).c_str(), ini.c_str());
    WritePrivateProfileStringA("game", "civilians", g_config.civilians ? "true" : "false", ini.c_str());
    WritePrivateProfileStringA("game", "team_kill", g_config.teamKill ? "true" : "false", ini.c_str());
    WritePrivateProfileStringA("game", "cheats", g_config.cheats ? "true" : "false", ini.c_str());
    WritePrivateProfileStringA("game", "cheat_access", std::to_string(g_config.cheatAccess).c_str(), ini.c_str());
}

}  // namespace cfg
