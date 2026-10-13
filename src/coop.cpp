#include "coop.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#include "alert.h"
#include "autoplay.h"
#include "cheat.h"
#include "clock.h"
#include "config.h"
#include "difficulty.h"
#include "lang.h"
#include "log.h"
#include "memory.h"
#include "network.h"
#include "oni.h"
#include "protocol.h"
#include "replication.h"
#include "roster.h"
#include "rules.h"

using namespace oni;

namespace coop {

static bool g_teamKill = false;

// Difficulté : plus il y a de clients, plus les PNJ ont de vie et, si l'hôte le veut, frappent
// fort. Les valeurs sont celles du fichier onimp_difficulty.ini de l'hôte (voir difficulty.h).
static difficulty::ClientCount g_clients;  // clients comptés ici pour la difficulté
static difficulty::Table g_table = difficulty::DefaultTable();  // hôte : son fichier
static bool g_tableLoaded = false;
// Hôte : le fichier se modifie en cours de partie. Sa date est regardée une fois par seconde, et il
// est relu dès qu'elle change.
static uint64_t g_tableStamp = 0;          // date du fichier à sa dernière lecture
static double g_nextTableCheck = 0.0;
static const double kTableCheckInterval = 1.0;
// Hôte : la table vient d'être réglée par un curseur du menu F1 (voir OnSliderMoved). Un curseur
// que l'on fait glisser bouge à chaque image : les clients sont prévenus dix fois par seconde au
// plus, et le fichier n'est écrit qu'une fois le curseur au repos, ou le menu fermé.
static bool g_tableDirty = false;          // pas encore écrite dans le fichier
static bool g_tableUnsent = false;         // pas encore dite aux clients
static double g_tableChangedAt = 0.0;
static double g_nextTableSend = 0.0;
static const double kTableSettle = 0.5;
static const double kTableSendInterval = 0.1;
static uint32_t g_hostClients = 0;         // client : dernier compte reçu de l'hôte...
static uint32_t g_hostHealth = 100;        // ... et ce qu'il vaut chez lui
static uint32_t g_hostDamage = 100;
static bool g_hostOnlineDamage = false;    // client : réglage de l'hôte
static bool g_menusServed = false;         // voir SetMenusServed

// Règles de la partie : mode de difficulté, multiplicateur d'ennemis, civils. Celles de l'hôte sont
// dans sa configuration, gardées dans onimp.ini. Un client tient celles-ci de l'hôte ; tant qu'il
// n'en a rien reçu, le mode de sa dernière partie vaut.
static difficulty::ClientCount g_peers;    // hôte : clients comptés pour le minimum du multiplicateur
static bool g_hostKnown = false;           // client : l'hôte a dit ses règles
static uint8_t g_hostMode = rules::kNormal;
static int g_hostEnemies = rules::kMinEnemies;
static bool g_hostCivilians = false;
static int g_hostPlayers = 1;
static uint32_t g_revision = 0;            // change avec chaque réglage : le menu ouvert se met à jour

bool TeamKill() { return g_teamKill; }

// L'hôte en décide, et son choix est gardé dans onimp.ini ; un client suit ce qu'il en reçoit.
bool OnlineDamage() {
    return net::CurrentMode() == cfg::Mode::Client ? g_hostOnlineDamage : cfg::Get().onlineDamage;
}
static bool Client() { return net::CurrentMode() == cfg::Mode::Client; }
static bool Host() { return net::CurrentMode() == cfg::Mode::Host; }

uint8_t Difficulty() { return Client() && g_hostKnown ? g_hostMode : cfg::Get().difficulty; }
int Players() { return Client() ? g_hostPlayers : 1 + static_cast<int>(g_peers.value()); }
int EnemyMultiplier() {
    return Client() ? g_hostEnemies : rules::EffectiveEnemies(cfg::Get().enemies, Players());
}
bool Civilians() { return Client() ? g_hostCivilians : cfg::Get().civilians; }

// Ce que le mode de difficulté ajoute à un pourcentage : rien, sauf en survie.
static uint32_t WithMode(uint32_t percent, uint32_t modePercent) {
    return static_cast<uint32_t>(static_cast<uint64_t>(percent) * modePercent / 100);
}

uint32_t NpcHealthPercent() {
    if (Client()) return net::Connected() ? g_hostHealth : 100;
    if (!net::Running()) return 100;
    uint32_t percent = g_clients.value() ? difficulty::HealthPercent(g_table, g_clients.value()) : 100;
    return WithMode(percent, rules::Of(Difficulty()).healthPercent);
}
uint32_t NpcDamagePercent() {
    if (Client()) return net::Connected() ? g_hostDamage : 100;
    if (!net::Running()) return 100;
    uint32_t percent = g_clients.value() && OnlineDamage() ? difficulty::DamagePercent(g_table, g_clients.value()) : 100;
    return WithMode(percent, rules::Of(Difficulty()).damagePercent);
}

void SetMenusServed(bool served) { g_menusServed = served; }

// Difficulté que le moteur doit lire à la place de celle des options du jeu : celle du mode choisi
// par l'hôte. Dès le lancement, avant même que la session s'ouvre : le premier niveau crée ses
// personnages avant notre première image. Négatif : hors session, les options du jeu valent.
static int EngineLevel() {
    if (cfg::Get().mode == cfg::Mode::Off && !net::Running()) return -1;
    return rules::Of(Difficulty()).engineLevel;
}

bool InstallDifficultyHook() { return mem::InstallDifficultyHook(&EngineLevel); }

// -------------------------------------------------------------------------------------------------
// Réglages
// -------------------------------------------------------------------------------------------------
static void Announce() {
    if (!Host() || !net::Connected()) return;
    proto::Settings settings = {};
    settings.teamKill = g_teamKill ? 1 : 0;
    settings.cheats = cheat::Master() ? 1 : 0;
    settings.onlineDamage = OnlineDamage() ? 1 : 0;
    settings.clients = static_cast<uint8_t>(g_clients.value());
    settings.npcHealthPercent = static_cast<uint16_t>(NpcHealthPercent());
    settings.npcDamagePercent = static_cast<uint16_t>(NpcDamagePercent());
    settings.difficulty = Difficulty();
    settings.enemies = static_cast<uint8_t>(EnemyMultiplier());
    settings.civilians = Civilians() ? 1 : 0;
    settings.players = static_cast<uint8_t>(Players());
    settings.cheatAccess = cheat::AccessMask();
    uint8_t message[1 + sizeof(settings)];
    message[0] = proto::kReliableSettings;
    memcpy(message + 1, &settings, sizeof(settings));
    net::SendReliable(message, sizeof(message));
}

// Le même texte deux fois : en français pour le journal, dans la langue du joueur pour l'écran.
static void Both(char* log, char* shown, size_t size, const char* format, ...) {
    va_list args, copy;
    va_start(args, format);
    va_copy(copy, args);
    vsnprintf(log, size, format, args);
    loc::FormatV(shown, size, format, copy);
    va_end(copy);
    va_end(args);
}

// Ce que les PNJ ont de plus en ce moment, dit au joueur et au journal.
static void ReportDifficulty() {
    char text[96], shown[96];
    unsigned clients = static_cast<unsigned>(g_clients.value());
    unsigned health = static_cast<unsigned>(NpcHealthPercent()), damage = static_cast<unsigned>(NpcDamagePercent());
    if (health == 100 && damage == 100 && (clients == 0 || OnlineDamage())) {
        Both(text, shown, sizeof(text), "PNJ : vie et degats d'origine");
    } else if (OnlineDamage() || clients == 0 || damage != 100) {
        Both(text, shown, sizeof(text), "PNJ : vie x%u.%02u, degats x%u.%02u (%u client(s))", health / 100, health % 100,
             damage / 100, damage % 100, clients);
    } else {
        // Dit pourquoi : les dégâts du fichier ne comptent pas tant que ce réglage est coupé.
        Both(text, shown, sizeof(text), "PNJ : vie x%u.%02u, degats normaux, Online damage sur OFF (%u client(s))",
             health / 100, health % 100, clients);
    }
    LOG("difficulte : %s", text);
    mem::ConsolePrint("OniMP : %s", shown);
}

// Hôte : son fichier peut avoir changé depuis le lancement. Vrai si les valeurs ne sont plus les
// mêmes.
static bool ReadTable() {
    bool found = false;
    std::string path = cfg::DifficultyPath();
    g_tableStamp = cfg::DifficultyStamp(path.c_str());
    difficulty::Table table = cfg::LoadDifficulty(path.c_str(), &found);
    bool changed = !g_tableLoaded || memcmp(&table, &g_table, sizeof(table)) != 0;
    bool differs = g_tableLoaded && changed;
    if (changed) {
        LOG("difficulte : onimp_difficulty.ini %s ; vie %u / %u / %u, degats %u / %u / %u pour cent avec 1, 2, 3 "
            "client(s)", found ? "lu" : "absent, valeurs d'origine", static_cast<unsigned>(table.health[1]),
            static_cast<unsigned>(table.health[2]), static_cast<unsigned>(table.health[3]),
            static_cast<unsigned>(table.damage[1]), static_cast<unsigned>(table.damage[2]),
            static_cast<unsigned>(table.damage[3]));
    }
    // Modifié en cours de partie : celui qui vient de l'enregistrer le voit pris en compte, même
    // sans client pour en montrer l'effet.
    if (differs) {
        mem::ConsolePrint("OniMP : difficulte relue : vie %u / %u / %u, degats %u / %u / %u pour cent",
                          static_cast<unsigned>(table.health[1]), static_cast<unsigned>(table.health[2]),
                          static_cast<unsigned>(table.health[3]), static_cast<unsigned>(table.damage[1]),
                          static_cast<unsigned>(table.damage[2]), static_cast<unsigned>(table.damage[3]));
    }
    g_table = table;
    g_tableLoaded = true;
    return differs;
}

// Hôte : la table réglée dans le menu va dans le fichier, d'où elle sera relue au prochain
// lancement, et les clients en reçoivent le dernier état.
static void CommitTable() {
    if (!g_tableDirty) return;
    g_tableDirty = false;
    g_tableUnsent = false;
    std::string path = cfg::DifficultyPath();
    cfg::SaveDifficulty(path.c_str(), g_table);
    g_tableStamp = cfg::DifficultyStamp(path.c_str());  // notre propre écriture n'est pas à relire
    LOG("difficulte : reglee dans le menu, ecrite dans onimp_difficulty.ini ; vie %u / %u / %u, degats %u / %u / %u "
        "pour cent avec 1, 2, 3 client(s)", static_cast<unsigned>(g_table.health[1]),
        static_cast<unsigned>(g_table.health[2]), static_cast<unsigned>(g_table.health[3]),
        static_cast<unsigned>(g_table.damage[1]), static_cast<unsigned>(g_table.damage[2]),
        static_cast<unsigned>(g_table.damage[3]));
    if (g_clients.value()) ReportDifficulty();
    Announce();
}

void ReloadDifficulty() {
    if (Client()) return;
    CommitTable();  // ce qui vient d'être réglé dans le menu passe avant ce que dit encore le fichier
    if (ReadTable() && g_clients.value()) {
        ReportDifficulty();
        Announce();
    }
}

// Le multiplicateur d'ennemis en vigueur, et ce qui le tient au-dessus du choix de l'hôte.
static void ReportEnemies() {
    int players = Players(), enemies = EnemyMultiplier();
    if (rules::MinEnemies(players) > rules::kMinEnemies && enemies == rules::MinEnemies(players)) {
        LOG("reglage : ennemis x%d, minimum a %d joueurs", enemies, players);
        mem::ConsolePrint("OniMP : ennemis x%d, minimum a %d joueurs", enemies, players);
    } else {
        LOG("reglage : ennemis x%d", enemies);
        mem::ConsolePrint("OniMP : ennemis x%d", enemies);
    }
}

void Update(double now) {
    cfg::Mode mode = net::CurrentMode();
    // Joueurs présents, PNJ partagés ou non : le minimum du multiplicateur d'ennemis en dépend. Un
    // client parti compte encore un moment, comme pour la vie des PNJ.
    int enemies = EnemyMultiplier();
    bool hosting = mode == cfg::Mode::Host;
    if (g_peers.Update(hosting ? static_cast<uint32_t>(net::ConnectedCount()) : 0, !hosting, now)) {
        g_revision++;
        if (hosting && EnemyMultiplier() != enemies) ReportEnemies();
        if (hosting) Announce();
    }

    uint32_t wanted = 0;
    bool immediate = false;  // une baisse s'applique sans attendre
    if (!cfg::Get().npcs) {
        immediate = true;    // chacun ses PNJ : rien à durcir
    } else if (mode == cfg::Mode::Host) {
        wanted = static_cast<uint32_t>(net::ConnectedCount());
    } else if (mode == cfg::Mode::Client && net::Connected()) {
        wanted = g_hostClients;  // l'hôte a déjà attendu ce qu'il fallait
        immediate = true;
    }
    if (!g_tableLoaded && !Client()) ReadTable();
    if (g_tableDirty) {
        if (g_tableUnsent && now >= g_nextTableSend) {
            g_tableUnsent = false;
            g_nextTableSend = now + kTableSendInterval;
            Announce();
        }
        if (now - g_tableChangedAt >= kTableSettle) CommitTable();
    } else if (mode == cfg::Mode::Host && now >= g_nextTableCheck) {
        g_nextTableCheck = now + kTableCheckInterval;
        if (cfg::DifficultyStamp(cfg::DifficultyPath().c_str()) != g_tableStamp) ReloadDifficulty();
    }
    if (!g_clients.Update(wanted, immediate, now)) return;
    // Un client arrive ou s'en va : l'occasion de relire le fichier, sauf réglage en cours au menu.
    if (!Client() && !g_tableDirty) ReadTable();
    g_revision++;
    ReportDifficulty();
    Announce();
}

static void Apply(bool teamKill) {
    if (teamKill == g_teamKill) return;
    g_revision++;
    g_teamKill = teamKill;
    LOG("reglage : tir ami %s", teamKill ? "active" : "desactive");
    mem::ConsolePrint("OniMP : tir ami %s", teamKill ? "active" : "desactive");
}

static bool HostOnly() {
    if (net::CurrentMode() != cfg::Mode::Client) return true;
    mem::ConsolePrint("OniMP : reglage reserve a l'hote");
    return false;
}

void Touch() { g_revision++; }

// Hôte : le tir ami est gardé dans onimp.ini comme les autres règles, et repris au lancement
// suivant. Un client ne l'écrit jamais : c'est celui de l'hôte, qu'il ne fait que suivre.
// L'invincibilité et les munitions illimitées, décidées par l'hôte pour tous jusqu'à la 0.34, sont
// depuis la 0.35 des cheats que chaque joueur règle pour lui-même (voir cheat.h).
void OnSessionStarted(bool hosting) {
    g_teamKill = hosting && cfg::Get().teamKill;
    g_revision++;
    if (hosting) LOG("reglages de l'hote repris d'onimp.ini : tir ami %d", g_teamKill ? 1 : 0);
}

void SetTeamKill(bool enabled) {
    if (!HostOnly()) return;
    Apply(enabled);
    if (cfg::Get().teamKill != g_teamKill) {
        cfg::Get().teamKill = g_teamKill;
        cfg::SaveRules();
    }
    Announce();
}

void SetDifficulty(uint8_t mode) {
    if (!HostOnly() || mode >= rules::kModeCount || cfg::Get().difficulty == mode) return;
    uint32_t health = NpcHealthPercent(), damage = NpcDamagePercent();
    cfg::Get().difficulty = mode;
    cfg::SaveRules();
    g_revision++;
    const rules::ModeRules& chosen = rules::Of(mode);
    LOG("reglage : difficulte %s ; %d s pour ranimer un joueur a terre, releve a %u pour cent de sa sante",
        chosen.name, static_cast<int>(chosen.bleedSeconds), static_cast<unsigned>(chosen.revivePercent));
    mem::ConsolePrint("OniMP : difficulte %s : %d s pour ranimer, releve a %u %% de sante", chosen.name,
                      static_cast<int>(chosen.bleedSeconds), static_cast<unsigned>(chosen.revivePercent));
    if (health != NpcHealthPercent() || damage != NpcDamagePercent()) ReportDifficulty();
    Announce();
}

void SetEnemyMultiplier(int chosen) {
    if (!HostOnly()) return;
    if (chosen < rules::kMinEnemies) chosen = rules::kMinEnemies;
    if (chosen > rules::kMaxEnemies) chosen = rules::kMaxEnemies;
    int before = EnemyMultiplier();
    if (cfg::Get().enemies != chosen) {
        cfg::Get().enemies = chosen;
        cfg::SaveRules();
        g_revision++;
    }
    // Dit aussi quand rien ne change : le choix est sous le minimum du nombre de joueurs.
    if (EnemyMultiplier() != before || EnemyMultiplier() != chosen) ReportEnemies();
    if (EnemyMultiplier() != before) Announce();
}

void SetCivilians(bool enabled) {
    if (!HostOnly() || cfg::Get().civilians == enabled) return;
    cfg::Get().civilians = enabled;
    cfg::SaveRules();
    g_revision++;
    LOG("reglage : civils %s", enabled ? "presents" : "absents");
    mem::ConsolePrint("OniMP : civils %s", enabled ? "presents : en tuer un fait echouer la mission" : "absents");
    Announce();
}

void SetOnlineDamage(bool enabled) {
    if (!HostOnly() || cfg::Get().onlineDamage == enabled) return;
    cfg::Get().onlineDamage = enabled;
    cfg::SaveRules();
    g_revision++;
    LOG("reglage : degats en ligne %s", enabled ? "actives" : "desactives");
    mem::ConsolePrint("OniMP : degats des PNJ %s", enabled ? "en ligne, plus forts a chaque client" : "normaux");
    if (g_clients.value()) ReportDifficulty();
    Announce();
}

void Resend() { Announce(); }

void OnConnection(uint8_t, bool connected) {
    if (net::CurrentMode() == cfg::Mode::Host) {
        if (connected) Announce();  // à tous : sans effet chez ceux qui les ont déjà
    } else if (!connected) {
        Apply(false);  // sans hôte, plus personne pour en décider
    }
}

void OnReliable(uint8_t from, const uint8_t* data, int size) {
    proto::Settings settings;
    if (from != proto::kHostPlayerId) return;
    if (size < static_cast<int>(sizeof(settings)) || net::CurrentMode() != cfg::Mode::Client) return;
    memcpy(&settings, data, sizeof(settings));
    Apply(settings.teamKill != 0);
    cheat::OnHostSettings(settings.cheats != 0, settings.cheatAccess);
    // La difficulté : Update s'aligne sur ce compte. Ce qui change sans que le compte bouge (le
    // réglage de l'hôte, son fichier relu) se dit ici.
    bool onlineDamage = settings.onlineDamage != 0;
    uint32_t clients = g_clients.value();
    bool told = settings.clients == clients && clients != 0 &&
                (onlineDamage != g_hostOnlineDamage || settings.npcHealthPercent != g_hostHealth ||
                 settings.npcDamagePercent != g_hostDamage);
    g_hostOnlineDamage = onlineDamage;
    g_hostClients = settings.clients <= difficulty::kMaxClients ? settings.clients : difficulty::kMaxClients;
    g_hostHealth = settings.npcHealthPercent;
    g_hostDamage = settings.npcDamagePercent;
    // Le mode survie ajoute sa part à ce que donne le fichier de l'hôte.
    if (g_hostHealth < difficulty::kMinHealthPercent) g_hostHealth = difficulty::kMinHealthPercent;
    if (g_hostHealth > 2 * difficulty::kMaxHealthPercent) g_hostHealth = 2 * difficulty::kMaxHealthPercent;
    if (told) ReportDifficulty();

    // Les règles de la partie : dites quand elles changent, et la première fois.
    uint8_t mode = settings.difficulty < rules::kModeCount ? settings.difficulty : static_cast<uint8_t>(rules::kNormal);
    int enemies = rules::EffectiveEnemies(settings.enemies, 1);
    bool civilians = settings.civilians != 0;
    if (!g_hostKnown || mode != g_hostMode) {
        const rules::ModeRules& chosen = rules::Of(mode);
        LOG("reglage : difficulte de l'hote %s", chosen.name);
        mem::ConsolePrint("OniMP : difficulte %s : %d s pour ranimer, releve a %u %% de sante", chosen.name,
                          static_cast<int>(chosen.bleedSeconds), static_cast<unsigned>(chosen.revivePercent));
    }
    if (!g_hostKnown || enemies != g_hostEnemies) {
        LOG("reglage : ennemis x%d d'apres l'hote", enemies);
        mem::ConsolePrint("OniMP : ennemis x%d", enemies);
    }
    if (!g_hostKnown || civilians != g_hostCivilians) {
        LOG("reglage : civils %s d'apres l'hote", civilians ? "presents" : "absents");
        if (civilians) mem::ConsolePrint("OniMP : civils presents : en tuer un fait echouer la mission");
    }
    g_hostKnown = true;
    g_hostMode = mode;
    g_hostEnemies = enemies;
    g_hostCivilians = civilians;
    g_hostPlayers = settings.players >= 1 && settings.players <= proto::kMaxPlayers ? settings.players : 1;
    g_revision++;
    // Gardé pour le prochain lancement : le premier niveau se charge avant que l'hôte ait parlé.
    if (cfg::Get().difficulty != mode) {
        cfg::Get().difficulty = mode;
        cfg::SaveDifficultyMode();
    }
}

// -------------------------------------------------------------------------------------------------
// Affichage
// -------------------------------------------------------------------------------------------------
// Chaque joueur choisit pour lui-même ce qu'il voit au-dessus des personnages. Le choix est gardé
// dans onimp.ini.
bool ShowPlayerHealth() { return cfg::Get().showPlayerHealth; }
bool ShowNpcHealth() { return cfg::Get().showNpcHealth; }

static void SetDisplay(bool& option, bool shown, const char* what) {
    if (option == shown) return;
    option = shown;
    cfg::SaveDisplay();
    mem::ConsolePrint("OniMP : %s %s", what, shown ? "affiches" : "masques");
}

void SetShowPlayerHealth(bool shown) { SetDisplay(cfg::Get().showPlayerHealth, shown, "points de vie des joueurs"); }
void SetShowNpcHealth(bool shown) { SetDisplay(cfg::Get().showNpcHealth, shown, "points de vie des PNJ"); }

bool ShowNpcIds() { return cfg::Get().showNpcIds; }
bool ShowWeaponIds() { return cfg::Get().showWeaponIds; }
void SetShowNpcIds(bool shown) { SetDisplay(cfg::Get().showNpcIds, shown, "identifiants des PNJ"); }
void SetShowWeaponIds(bool shown) { SetDisplay(cfg::Get().showWeaponIds, shown, "identifiants des armes"); }
bool ShowPlayerAmmo() { return cfg::Get().showPlayerAmmo; }
void SetShowPlayerAmmo(bool shown) { SetDisplay(cfg::Get().showPlayerAmmo, shown, "munitions des joueurs"); }
bool ShowPing() { return cfg::Get().showPing; }
void SetShowPing(bool shown) { SetDisplay(cfg::Get().showPing, shown, "latences des joueurs"); }
// Portraits des dialogues (« cinematic insert » des scripts du niveau). Le jeu les fait entrer en
// haut de l'écran, à gauche ou à droite selon qui parle ; ils vont ici dans la rangée du bas, du
// même côté, juste au-dessus des sous-titres. Seule la place demandée change : la rangée du haut
// (1, 2, 3) devient celle du bas (7, 8, 9), et les places hors de l'écran d'où ils arrivent et par
// où ils repartent suivent. En session seulement : hors session, rien ne change.
bool LowPortraits() { return cfg::Get().lowPortraits; }

uint8_t InsertPlace(uint8_t code) {
    static const uint8_t kLow[26] = {0,  7,  8,  9,  4,  5,  6,  7,  8,  9,  21, 22, 23,
                                     24, 25, 19, 20, 17, 18, 19, 20, 21, 22, 23, 24, 25};
    return LowPortraits() && net::Running() && code < 26 ? kLow[code] : code;
}

void SetLowPortraits(bool low) {
    if (cfg::Get().lowPortraits == low) return;
    cfg::Get().lowPortraits = low;
    cfg::SaveDisplay();
    mem::ConsolePrint("OniMP : portraits des dialogues %s", low ? "en bas" : "en haut");
}

bool ShowThroughWalls() { return cfg::Get().showThroughWalls; }
void SetShowThroughWalls(bool shown) {
    SetDisplay(cfg::Get().showThroughWalls, shown, "joueurs a travers les murs");
}

// Messages du mod à l'écran. Masqués, ils ne vont plus qu'au journal : celui qui annonce le
// changement s'écrit donc avant de les couper, ou après les avoir remis.
bool ShowMessages() { return cfg::Get().showMessages; }

void SetShowMessages(bool shown) {
    if (cfg::Get().showMessages == shown) return;
    if (!shown) mem::ConsolePrint("OniMP : messages masques (le journal les garde)");
    cfg::Get().showMessages = shown;
    mem::ShowMessages(shown);
    cfg::SaveDisplay();
    if (shown) mem::ConsolePrint("OniMP : messages affiches");
}

// Langue des textes du mod : messages à l'écran, menu F1, panneau d'un joueur à terre. Chaque
// joueur a la sienne, gardée dans onimp.ini, et le changement se voit aussitôt : chaque texte est
// traduit au moment où il s'affiche (voir lang.h), et le menu ouvert est récrit. Le journal, lui,
// reste en français.
static void SetLanguage(loc::Language language) {
    if (language == loc::Current()) return;
    loc::Set(language);
    cfg::Get().language = loc::Key(language);
    cfg::SaveDisplay();
    g_revision++;
    LOG("reglage : langue %s", loc::Label(language));
    mem::ConsolePrint("OniMP : langue : francais");  // dit dans la nouvelle langue : « language : English »
}

// -------------------------------------------------------------------------------------------------
// Onglets « Coop » et « Display » du menu de pause
// -------------------------------------------------------------------------------------------------
// Le menu de pause est un dialogue du moteur : cinq boutons pour onglets, et des zones où il
// dessine la page choisie. On y ajoute nos onglets sous les cinq autres, et pour chacun des
// boutons de réglage qui prennent la place de la page quand il est ouvert. Ils sont créés par la
// fonction du moteur qui crée les autres, à partir des mêmes données : le gabarit du dialogue, où
// l'on recopie type, style et police d'un bouton existant.
struct Option {
    uint16_t id;               // identifiants libres dans ce dialogue
    const char* label;
    bool (*get)();             // réglage à deux états ; nul : réglage à plusieurs valeurs
    void (*set)(bool);
    void (*value)(char* out, size_t size);  // réglage à plusieurs valeurs : celle du moment, en clair
    void (*next)();                         // et le passage à la suivante
    // Ligne dont le titre entier est composé ailleurs (une entrée de liste, l'accès d'un client) :
    // title l'écrit, click est ce qu'un clic y fait, index dit laquelle.
    void (*title)(int index, char* out, size_t size) = nullptr;
    void (*click)(int index) = nullptr;
    int index = 0;
    uint8_t place = 0;         // kWhole : toute la ligne ; kLeftHalf, kRightHalf : deux boutons côte à côte
    bool hostOnly = false;     // pas créé chez un client
};
enum : uint8_t { kWhole, kLeftHalf, kRightHalf };

// Curseur de difficulté, sous les boutons de l'onglet Coop : une étiquette, et dessous le curseur
// du moteur, celui de son dialogue Options. Il règle ce que chaque client ajoute à la vie ou aux
// dégâts des PNJ, en pour cent de l'origine : à +100, la vie double avec un client et triple avec
// deux. C'est la table du fichier onimp_difficulty.ini qu'il change, et le fichier est récrit.
// Un curseur sans table règle le volume de l'alerte, de 0 à 100 : celui-là est à chaque joueur,
// hôte ou client, et gardé dans son onimp.ini.
struct Slider {
    uint16_t labelId, id;      // identifiants libres dans ce dialogue
    const char* label;
    uint32_t (difficulty::Table::*values)[difficulty::kMaxClients + 1];  // nul : le volume de l'alerte, ou get et set
    int step;                  // pour cent par cran
    int steps;                 // crans, de 0 à steps
    bool damage;               // sans effet tant que « Online damage » est coupé
    // Curseur d'un réglage propre au joueur (onglet Cheat) : sa valeur en pour cent, de minimum à
    // minimum + step * steps.
    int (*get)() = nullptr;
    void (*set)(int) = nullptr;
    int minimum = 0;
};

struct Tab {
    uint16_t id;
    const char* title;
    // Réglages de partie : le client les voit et les suit sans pouvoir les changer, et n'a pas les
    // curseurs qui règlent le fichier de l'hôte.
    bool hostRules;
    bool nameField;            // sous ses boutons : le champ où saisir son pseudo
    const Option* options;
    int count;
    const Slider* sliders;
    int sliderCount;
    // Onglet réservé : montré seulement quand allowed répond vrai (l'hôte, ou un client à qui il a
    // ouvert l'accès). Ces onglets-là vont dans la colonne de droite, sous le bouton Close : celle
    // de gauche n'a plus de place que pour un.
    bool (*allowed)() = nullptr;
    bool rightColumn = false;
};

static void DifficultyValue(char* out, size_t size) { snprintf(out, size, "%s", rules::Of(Difficulty()).name); }
static void NextDifficulty() { SetDifficulty(rules::NextMode(Difficulty())); }

// « x2 », avec ce qui borne le choix quand d'autres joueurs sont là : le minimum, ou le verrou à
// quatre joueurs.
static void EnemiesValue(char* out, size_t size) {
    int minimum = rules::MinEnemies(Players());
    if (minimum >= rules::kMaxEnemies) loc::Format(out, size, "x%d (locked)", EnemyMultiplier());
    else if (minimum > rules::kMinEnemies) snprintf(out, size, "x%d (min x%d)", EnemyMultiplier(), minimum);
    else snprintf(out, size, "x%d", EnemyMultiplier());
}
static void NextEnemies() {
    if (!Client() && rules::MinEnemies(Players()) >= rules::kMaxEnemies) {
        mem::ConsolePrint("OniMP : ennemis x%d, verrouille a %d joueurs", rules::kMaxEnemies, Players());
        return;  // le choix de l'hôte reste ce qu'il était pour une partie à moins de joueurs
    }
    SetEnemyMultiplier(rules::NextEnemies(cfg::Get().enemies, Players()));
}

// Jeu automatique : réservé aux clients. Chez l'hôte le bouton le dit, et un clic l'explique.
static void AutoPlayValue(char* out, size_t size) {
    snprintf(out, size, "%s", Client() ? (bot::Enabled() ? "ON" : "OFF") : "(client)");
}
static void NextAutoPlay() {
    bot::SetEnabled(!bot::Enabled());
    g_revision++;
}

static void LanguageValue(char* out, size_t size) { snprintf(out, size, "%s", loc::Label(loc::Current())); }
static void NextLanguage() {
    SetLanguage(loc::Current() == loc::Language::French ? loc::Language::English : loc::Language::French);
}

// Les libellés sont écrits ici en anglais, leur langue d'origine ; ils s'affichent dans celle du
// joueur (voir OptionTitle).
static const Option kCoopOptions[] = {
    {0x85, "Difficulty", nullptr, nullptr, DifficultyValue, NextDifficulty},
    {0x86, "Enemies", nullptr, nullptr, EnemiesValue, NextEnemies},
    {0x87, "Civilians", Civilians, SetCivilians, nullptr, nullptr},
    {0x7B, "Team kill", TeamKill, SetTeamKill, nullptr, nullptr},
    {0x80, "Online damage", OnlineDamage, SetOnlineDamage, nullptr, nullptr},  // OFF : dégâts normaux des PNJ
    // Le seul réglage de l'onglet qui soit au client : son personnage suit l'hôte et se bat seul.
    {0x95, "Auto-play", nullptr, nullptr, AutoPlayValue, NextAutoPlay},
};
static const Slider kCoopSliders[] = {
    {0x81, 0x82, "NPC health", &difficulty::Table::health, 10, 50, false},  // jusqu'à +500 % par client
    {0x83, 0x84, "NPC damage", &difficulty::Table::damage, 5, 40, true},    // jusqu'à +200 %
    {0x93, 0x94, "Alert volume", nullptr, 5, 20, false},                    // de 0 à 100 %, chez chaque joueur
};
static const Option kDisplayOptions[] = {
    {0x7E, "Player health", ShowPlayerHealth, SetShowPlayerHealth, nullptr, nullptr},
    {0x7F, "NPC health", ShowNpcHealth, SetShowNpcHealth, nullptr, nullptr},
    {0x88, "Messages", ShowMessages, SetShowMessages, nullptr, nullptr},
    {0x89, "NPC IDs", ShowNpcIds, SetShowNpcIds, nullptr, nullptr},
    {0x8A, "Weapon IDs", ShowWeaponIds, SetShowWeaponIds, nullptr, nullptr},
    {0x8B, "Player ammo", ShowPlayerAmmo, SetShowPlayerAmmo, nullptr, nullptr},
    {0x8D, "Ping", ShowPing, SetShowPing, nullptr, nullptr},
    // La tenue du moment ; chaque clic passe à la suivante que personne d'autre ne porte.
    {0x8E, "Skin", nullptr, nullptr, roster::SkinValue, roster::NextSkin},
    // Français ou anglais ; chaque clic passe à l'autre, et tout ce qui est affiché suit aussitôt.
    {0x91, "Language", nullptr, nullptr, LanguageValue, NextLanguage},
    // Les autres joueurs vus à travers les murs, ou seulement quand ils sont en vue.
    {0x92, "Wallhack (ESP)", ShowThroughWalls, SetShowThroughWalls, nullptr, nullptr},
    // Portraits des dialogues en bas de l'écran, au-dessus des sous-titres.
    {0x96, "Low portraits", LowPortraits, SetLowPortraits, nullptr, nullptr},
};
// Onglet Cheat : l'interrupteur général de l'hôte, les cheats de ce joueur, et chez l'hôte l'accès
// de chaque client. God mode et Unlimited ammo y gardent les identifiants qu'ils avaient dans
// l'onglet Coop.
static const Option kCheatOptions[] = {
    {0x97, "Cheats", cheat::Master, cheat::SetMaster, nullptr, nullptr},
    {0x7C, "God mode", cheat::God, cheat::SetGod, nullptr, nullptr},
    {0x8C, "Unlimited ammo", cheat::Ammo, cheat::SetAmmo, nullptr, nullptr},
    {0x98, "One-hit kill", cheat::OneHit, cheat::SetOneHit, nullptr, nullptr},
    {0x99, "Permanent aura", cheat::Aura, cheat::SetAura, nullptr, nullptr},
    {0x9A, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::AccessTitle, cheat::AccessClick, 1, kWhole, true},
    {0x9B, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::AccessTitle, cheat::AccessClick, 2, kWhole, true},
    {0x9C, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::AccessTitle, cheat::AccessClick, 3, kWhole, true},
};
static const Slider kCheatSliders[] = {
    {0x9D, 0x9E, "Attack speed", nullptr, 10, 20, false, cheat::AttackSpeed, cheat::SetAttackSpeed, 100},  // 100 à 300 %
    {0x9F, 0xA0, "Move speed", nullptr, 10, 20, false, cheat::MoveSpeed, cheat::SetMoveSpeed, 100},
    {0xA1, 0xA2, "Jump", nullptr, 20, 15, false, cheat::Jump, cheat::SetJump, 100},                        // 100 à 400 %
};
// Onglet Character : le personnage incarné (un clic rend Konoko), le filtre de la liste, la touche
// « personnage suivant », puis une page de la liste des classes chargées et de quoi la tourner.
static const Option kCharacterOptions[] = {
    {0xA3, "Character", nullptr, nullptr, cheat::CharacterValue, cheat::ResetCharacter},
    {0xA4, "Filter", nullptr, nullptr, cheat::CharacterFilterValue, cheat::NextCharacterFilter},
    {0xA5, "Hotkey", nullptr, nullptr, cheat::KeyValue, cheat::NextKey},
    {0xA6, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::CharacterRowTitle, cheat::CharacterRowClick, 0},
    {0xA7, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::CharacterRowTitle, cheat::CharacterRowClick, 1},
    {0xA8, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::CharacterRowTitle, cheat::CharacterRowClick, 2},
    {0xA9, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::CharacterRowTitle, cheat::CharacterRowClick, 3},
    {0xAA, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::CharacterRowTitle, cheat::CharacterRowClick, 4},
    {0xAB, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::CharacterRowTitle, cheat::CharacterRowClick, 5},
    {0xAC, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::CharacterRowTitle, cheat::CharacterRowClick, 6},
    {0xAD, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::CharacterRowTitle, cheat::CharacterRowClick, 7},
    {0xAE, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::CharacterPageTitle, cheat::CharacterPageClick, -1, kLeftHalf},
    {0xAF, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::CharacterPageTitle, cheat::CharacterPageClick, 1, kRightHalf},
};
// Onglet Spawn : le genre (objets, armes, PNJ), son réglage (nombre, munitions, camp), le filtre
// des classes de PNJ, puis une page de ce qui peut apparaître : un clic le pose devant le joueur.
static const Option kSpawnOptions[] = {
    {0xB0, "Kind", nullptr, nullptr, cheat::SpawnKindValue, cheat::NextSpawnKind},
    {0xB1, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::SpawnOptionTitle, cheat::SpawnOptionClick, 0},
    {0xB2, "Filter", nullptr, nullptr, cheat::SpawnFilterValue, cheat::NextSpawnFilter},
    {0xB3, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::SpawnRowTitle, cheat::SpawnRowClick, 0},
    {0xB4, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::SpawnRowTitle, cheat::SpawnRowClick, 1},
    {0xB5, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::SpawnRowTitle, cheat::SpawnRowClick, 2},
    {0xB6, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::SpawnRowTitle, cheat::SpawnRowClick, 3},
    {0xB7, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::SpawnRowTitle, cheat::SpawnRowClick, 4},
    {0xB8, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::SpawnRowTitle, cheat::SpawnRowClick, 5},
    {0xB9, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::SpawnRowTitle, cheat::SpawnRowClick, 6},
    {0xBA, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::SpawnRowTitle, cheat::SpawnRowClick, 7},
    {0xBB, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::SpawnPageTitle, cheat::SpawnPageClick, -1, kLeftHalf},
    {0xBC, nullptr, nullptr, nullptr, nullptr, nullptr, cheat::SpawnPageTitle, cheat::SpawnPageClick, 1, kRightHalf},
};
static const Tab kTabs[] = {
    {0x7A, "Coop", true, false, kCoopOptions, 6, kCoopSliders, 3},
    {0x7D, "Display", false, true, kDisplayOptions, 11, nullptr, 0},
    {0xBD, "Cheat", false, false, kCheatOptions, 8, kCheatSliders, 3, cheat::Access, true},
    {0xBE, "Character", false, false, kCharacterOptions, 13, nullptr, 0, cheat::Access, true},
    {0xBF, "Spawn", false, false, kSpawnOptions, 13, nullptr, 0, cheat::Access, true},
};
static const uint16_t kPauseClose = 2;     // bouton Close du dialogue : il donne sa place à la colonne de droite
// Pseudo : une étiquette et un champ de saisie du moteur, sous les boutons de l'onglet Display.
static const uint16_t kNameLabelId = 0x8F, kNameFieldId = 0x90;
static const int kNameLabelWidth = 70, kNameGap = 5, kNameHeight = 20;
static const double kNameCommitDelay = 0.8;  // sans frappe depuis ce délai, le pseudo saisi est pris
static const int kTabCount = 5;
static const int kOptionMargin = 30;       // entre le bord de la zone de page et nos boutons
// La page du moteur est cachée quand un de nos onglets est ouvert : nos boutons partent de la
// hauteur du premier onglet, au-dessus de la zone de page. Moins hauts que les onglets (le moteur
// a des boutons de 20 à 40 points), pour tenir à huit au-dessus de trois curseurs, ou à onze
// au-dessus du champ du pseudo, sans descendre plus bas que les boutons de page du moteur.
static const int kOptionHeight = 32;
static const int kOptionGap = 4;
static const int kSliderLabelHeight = 20;  // hauteurs des étiquettes et des curseurs du dialogue Options
static const int kSliderHeight = 20;
static const int kSliderGap = 8;           // d'un curseur à l'étiquette du suivant

static fn::DialogCallback g_pauseCallback = nullptr;  // fonction d'origine du moteur
static void* g_dialog = nullptr;                      // dialogue de pause qui porte nos boutons
static bool g_built[kTabCount] = {};                  // onglets présents dans ce dialogue
static bool g_sliders[kTabCount] = {};                // et leurs curseurs
static const Tab* g_open = nullptr;                   // celui des nôtres qui est ouvert
static void* g_nameField = nullptr;                   // champ du pseudo, nul s'il n'a pas pu être créé
static std::string g_nameTyped;                       // ce qu'il contenait au dernier relevé
static double g_nameChangedAt = 0.0;                  // dernière frappe, 0 : rien à prendre
static uint32_t g_shownRevision = 0;                  // réglages tels que l'onglet ouvert les montre
static uint32_t g_accessRevision = 0;                 // et tels que les onglets réservés les suivent
// Le curseur du volume vient de bouger : appliqué tout de suite, il est gardé dans onimp.ini et
// donné à entendre une fois au repos.
static double g_volumeChangedAt = 0.0;                // 0 : rien à garder
static const double kVolumeSettle = 0.35;

static uint8_t* TemplateItem(void* dialogTemplate, uint16_t id) {
    uint32_t count = field<uint32_t>(dialogTemplate, off::kDlgItemCount);
    if (count > 64) return nullptr;
    for (uint32_t i = 0; i < count; i++) {
        uint8_t* item = static_cast<uint8_t*>(dialogTemplate) + off::kDlgItems + i * off::kDlgItemSize;
        if (field<uint16_t>(item, off::kItId) == id) return item;
    }
    return nullptr;
}

// Contrôle du type et du style voulus, avec les drapeaux et la police d'un contrôle du gabarit.
static void* AddControl(void* dialog, uint16_t type, uint32_t style, uint8_t* model, const char* title, uint16_t id,
                        int x, int y, int width, int height) {
    void* control = fn::WindowNew(type, title, field<uint32_t>(model, off::kItFlags) | off::kWnFlagChild, style, id, x,
                                  y, width, height, dialog, 0);
    if (control) fn::WindowSetFont(control, model + off::kItFont);
    return control;
}

static void* AddButton(void* dialog, uint8_t* model, const char* title, uint16_t id, int x, int y, int width) {
    return AddControl(dialog, field<uint16_t>(model, off::kItType), field<uint32_t>(model, off::kItStyle), model, title,
                      id, x, y, width, field<int16_t>(model, off::kItHeight));
}

static void Show(void* dialog, uint16_t id, bool visible) {
    if (void* item = fn::DialogItem(dialog, id)) fn::WindowSetVisible(item, visible ? 1 : 0);
}

static void OptionTitle(char* out, size_t size, const Option& option) {
    if (option.title) {
        option.title(option.index, out, size);
        return;
    }
    char value[32] = "";
    if (option.get) snprintf(value, sizeof(value), "%s", option.get() ? "ON" : "OFF");
    else option.value(value, sizeof(value));
    // Libellé et valeur dans la langue du joueur : « Difficulty : Hard », « Difficulte : Difficile ».
    loc::Format(out, size, "%s : %s", option.label, value);
}

static int TabIndex(const Tab& tab) { return static_cast<int>(&tab - kTabs); }

// Ce que la table du moment vaut pour ce curseur : son bonus par client, ou à défaut, pour une
// ligne écrite à la main dans le fichier, celui du premier client.
static int SliderBonus(const Slider& slider) {
    const uint32_t* values = g_table.*slider.values;
    int bonus = difficulty::PerClientBonus(values);
    if (bonus < 0) bonus = values[1] > 100 ? static_cast<int>(values[1] - 100) : 0;
    return bonus;
}

static int SliderPosition(const Slider& slider) {
    int value = slider.values ? SliderBonus(slider) : (slider.get ? slider.get() - slider.minimum : alert::Volume());
    int position = (value + slider.step / 2) / slider.step;
    return position > slider.steps ? slider.steps : position;
}

// « NPC health : +100 % per client », ou les trois valeurs d'une ligne qui ne suit pas cette règle.
static void SliderTitle(char* out, size_t size, const Slider& slider) {
    if (!slider.values) {
        // « Alert volume : 50 % », « Move speed : 150 % »
        loc::Format(out, size, "%s : %d %%", slider.label, slider.get ? slider.get() : alert::Volume());
        return;
    }
    const uint32_t* values = g_table.*slider.values;
    int bonus = difficulty::PerClientBonus(values);
    const char* ignored = slider.damage && !OnlineDamage() ? " (off)" : "";
    if (bonus >= 0) {
        loc::Format(out, size, "%s : +%d %% per client%s", slider.label, bonus, ignored);
    } else {
        loc::Format(out, size, "%s : %u / %u / %u %%%s", slider.label, static_cast<unsigned>(values[1]),
                    static_cast<unsigned>(values[2]), static_cast<unsigned>(values[3]), ignored);
    }
}

static void SetTitle(void* dialog, uint16_t id, const char* title) {
    if (void* item = fn::DialogItem(dialog, id)) snprintf(&field<char>(item, off::kWnTitle), 64, "%s", title);
}

// Un réglage actif se lit deux fois : dans le titre du bouton, et à son état enfoncé, celui de
// l'onglet choisi. Un curseur, à sa position et dans son étiquette.
static void RefreshOptions(void* dialog, const Tab& tab) {
    // Les titres des onglets et l'étiquette du pseudo ne changent qu'avec la langue ; le moteur
    // centre chaque titre au moment où il le trace.
    for (int t = 0; t < kTabCount; t++) {
        if (g_built[t]) SetTitle(dialog, kTabs[t].id, loc::Tr(kTabs[t].title));
    }
    if (g_nameField) SetTitle(dialog, kNameLabelId, loc::Tr("Name"));
    for (int i = 0; i < tab.count; i++) {
        const Option& option = tab.options[i];
        void* button = fn::DialogItem(dialog, option.id);
        if (!button) continue;
        OptionTitle(&field<char>(button, off::kWnTitle), 64, option);
        fn::WindowSend(button, off::kBtSetCheck, option.get && option.get() ? 1 : 0, 0);
    }
    g_shownRevision = g_revision;
    if (!g_sliders[TabIndex(tab)]) return;
    for (int i = 0; i < tab.sliderCount; i++) {
        const Slider& slider = tab.sliders[i];
        if (void* label = fn::DialogItem(dialog, slider.labelId)) {
            SliderTitle(&field<char>(label, off::kWnTitle), 64, slider);
        }
        if (void* control = fn::DialogItem(dialog, slider.id)) fn::SliderSetPosition(control, SliderPosition(slider));
    }
}

static void ShowOptions(void* dialog, const Tab& tab, bool visible) {
    for (int i = 0; i < tab.count; i++) Show(dialog, tab.options[i].id, visible);
    if (tab.nameField && g_nameField) {
        Show(dialog, kNameLabelId, visible);
        Show(dialog, kNameFieldId, visible);
    }
    if (!g_sliders[TabIndex(tab)]) return;
    for (int i = 0; i < tab.sliderCount; i++) {
        Show(dialog, tab.sliders[i].labelId, visible);
        Show(dialog, tab.sliders[i].id, visible);
    }
}

// Champ du pseudo : le contrôle de saisie du moteur, tel que le décrit le dialogue « setup player »
// resté dans les données du jeu (type, style, police). Créé caché, garni du pseudo du moment. Sans
// ce gabarit, pas de champ : la commande mp_name reste.
static void BuildNameField(void* dialog, uint8_t* labelModel, int x, int y, int width) {
    g_nameField = nullptr;
    void* setup = nullptr;
    uint8_t* model = nullptr;
    if (fn::GetDataPtr(Tag('W', 'M', 'D', 'D'), "dialog_setup_player", &setup) == 0 && setup) model = TemplateItem(setup, 100);
    if (!model || field<uint16_t>(model, off::kItType) != off::kWnTypeEditField ||
        fn::DialogItem(dialog, kNameLabelId) || fn::DialogItem(dialog, kNameFieldId)) {
        LOG("menu : champ de saisie du pseudo indisponible (gabarit absent ou identifiants pris) : mp_name le remplace");
        return;
    }
    void* label = AddControl(dialog, off::kWnTypeText, off::kTxStyleCentered, labelModel, loc::Tr("Name"), kNameLabelId,
                             x, y, kNameLabelWidth, kNameHeight);
    void* control = AddControl(dialog, off::kWnTypeEditField, field<uint32_t>(model, off::kItStyle), model, "",
                               kNameFieldId, x + kNameLabelWidth + kNameGap, y, width - kNameLabelWidth - kNameGap,
                               kNameHeight);
    if (label) fn::WindowSetVisible(label, 0);
    if (!label || !control) {
        if (control) fn::WindowSetVisible(control, 0);
        LOG("menu : champ de saisie du pseudo refuse par le moteur : mp_name le remplace");
        return;
    }
    fn::WindowSetVisible(control, 0);
    fn::WindowSend(control, off::kEfSetMaxChars, proto::kNameLength - 1, 0);
    g_nameTyped = roster::LocalName();
    fn::WindowSend(control, off::kEfSetText, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_nameTyped.c_str())), 0);
    g_nameChangedAt = 0.0;
    g_nameField = control;
}

// Relève le champ. Le pseudo est pris quand la frappe s'est arrêtée, ou tout de suite si commit.
static void PollNameField(bool commit) {
    if (!g_nameField) return;
    char text[proto::kNameLength + 16] = {};
    fn::WindowSend(g_nameField, off::kEfGetText, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(text)),
                   proto::kNameLength);
    text[proto::kNameLength - 1] = '\0';
    double now = clk::Seconds();
    if (g_nameTyped != text) {
        g_nameTyped = text;
        g_nameChangedAt = now;
    }
    if (g_nameChangedAt != 0.0 && (commit || now - g_nameChangedAt > kNameCommitDelay)) {
        g_nameChangedAt = 0.0;
        roster::SetLocalName(g_nameTyped.c_str());
    }
}

// Un autre mod a pu prendre ces identifiants.
static bool IdsFree(void* dialog, const Tab& tab) {
    if (fn::DialogItem(dialog, tab.id)) return false;
    for (int i = 0; i < tab.count; i++) {
        if (fn::DialogItem(dialog, tab.options[i].id)) return false;
    }
    for (int i = 0; i < tab.sliderCount; i++) {
        if (fn::DialogItem(dialog, tab.sliders[i].labelId) || fn::DialogItem(dialog, tab.sliders[i].id)) return false;
    }
    return true;
}

// Sous les boutons de l'onglet : une étiquette sur toute la largeur de la page, et dessous un
// curseur de la largeur des boutons. Créés cachés. Faux si l'un d'eux a été refusé : l'onglet
// garde alors ses boutons, sans curseurs. Chez un client, ceux qui règlent le fichier de l'hôte ne
// sont pas créés : il ne lui reste que les siens.
static bool BuildSliders(void* dialog, const Tab& tab, uint8_t* model, int areaX, int areaWidth, int top, int bottom,
                         bool host) {
    bool complete = true;
    int row = 0;
    for (int i = 0; i < tab.sliderCount; i++) {
        const Slider& slider = tab.sliders[i];
        if (slider.values && tab.hostRules && !host) continue;
        int y = top + row++ * (kSliderLabelHeight + kSliderHeight + kSliderGap);
        if (y + kSliderLabelHeight + kSliderHeight > bottom) return false;
        char title[64];
        SliderTitle(title, sizeof(title), slider);
        void* label = AddControl(dialog, off::kWnTypeText, off::kTxStyleCentered, model, title, slider.labelId, areaX,
                                 y, areaWidth, kSliderLabelHeight);
        void* control = AddControl(dialog, off::kWnTypeSlider, 0, model, "", slider.id, areaX + kOptionMargin,
                                   y + kSliderLabelHeight, areaWidth - 2 * kOptionMargin, kSliderHeight);
        if (label) fn::WindowSetVisible(label, 0);
        if (control) {
            fn::WindowSetVisible(control, 0);
            fn::SliderSetRange(control, 0, slider.steps);
        }
        if (!label || !control) complete = false;
    }
    return complete;
}

static bool BuildTabs(void* dialog) {
    void* dialogTemplate = nullptr;
    if (fn::GetDataPtr(Tag('W', 'M', 'D', 'D'), "pause_screen", &dialogTemplate) != 0 || !dialogTemplate) return false;
    uint8_t* help = TemplateItem(dialogTemplate, off::kPauseTabLast);
    uint8_t* diary = TemplateItem(dialogTemplate, off::kPauseTabLast - 1);
    uint8_t* area = TemplateItem(dialogTemplate, off::kPauseMainArea);
    if (!help || !diary || !area) return false;
    if (field<uint16_t>(help, off::kItType) != off::kWnTypeButton) return false;

    // Les onglets : à la suite des cinq autres, au même pas. Les réglages : à la place de la page,
    // l'un sous l'autre depuis la hauteur du premier onglet. Même sorte de bouton que les onglets,
    // qui reste enfoncé.
    uint8_t* first = TemplateItem(dialogTemplate, off::kPauseTabFirst);
    int x = field<int16_t>(help, off::kItX), y = field<int16_t>(help, off::kItY);
    int stepX = x - field<int16_t>(diary, off::kItX), stepY = y - field<int16_t>(diary, off::kItY);
    int height = field<int16_t>(help, off::kItHeight);
    int areaX = field<int16_t>(area, off::kItX), areaY = field<int16_t>(area, off::kItY);
    int width = field<int16_t>(area, off::kItWidth) - 2 * kOptionMargin;
    if ((stepX == 0 && stepY == 0) || width < 100) return false;
    int top = first ? field<int16_t>(first, off::kItY) : areaY;
    if (top < 0 || top > areaY) top = areaY;

    bool host = net::CurrentMode() == cfg::Mode::Host;
    int row = 0, rightRow = 0;
    uint8_t* close = TemplateItem(dialogTemplate, kPauseClose);
    int closeX = close ? field<int16_t>(close, off::kItX) : 0, closeY = close ? field<int16_t>(close, off::kItY) : 0;
    for (int t = 0; t < kTabCount; t++) {
        const Tab& tab = kTabs[t];
        if (!IdsFree(dialog, tab) || (tab.rightColumn && !close)) continue;
        // À gauche, à la suite des onglets du moteur ; à droite, sous le bouton Close, au même pas.
        int& placed = tab.rightColumn ? rightRow : row;
        int tabX = (tab.rightColumn ? closeX : x) + stepX * (placed + 1);
        int tabY = (tab.rightColumn ? closeY : y) + stepY * (placed + 1);
        if (tabX < 0 || tabY < 0 || tabY + height > field<int16_t>(dialogTemplate, off::kDlgHeight)) continue;
        if (!AddButton(dialog, help, loc::Tr(tab.title), tab.id, tabX, tabY, field<int16_t>(help, off::kItWidth))) continue;
        placed++;

        // Le titre est complet dès la création : le moteur en mesure alors la largeur pour le
        // centrer.
        bool complete = true;
        const int pitch = kOptionHeight + kOptionGap;
        // Une ligne par bouton, sauf deux moitiés, qui en partagent une. Un bouton réservé à l'hôte
        // n'est pas créé chez un client, et ne lui laisse pas de ligne vide.
        const int half = (width - kOptionGap) / 2;
        int rows = 0;
        for (int i = 0; i < tab.count; i++) {
            const Option& option = tab.options[i];
            if (option.hostOnly && !host) continue;
            char title[64];
            OptionTitle(title, sizeof(title), option);
            void* button = AddControl(dialog, field<uint16_t>(help, off::kItType), field<uint32_t>(help, off::kItStyle),
                                      help, title, option.id,
                                      areaX + kOptionMargin + (option.place == kRightHalf ? width - half : 0),
                                      top + rows * pitch, option.place == kWhole ? width : half, kOptionHeight);
            if (button) fn::WindowSetVisible(button, 0);
            else complete = false;
            if (option.place != kLeftHalf) rows++;
        }
        g_built[t] = complete;
        if (complete && tab.sliderCount) {
            // La page du moteur est cachée quand l'onglet est ouvert : la place va jusqu'au bas du
            // dialogue.
            g_sliders[t] = BuildSliders(dialog, tab, help, areaX, field<int16_t>(area, off::kItWidth),
                                        top + rows * pitch + kSliderGap,
                                        field<int16_t>(dialogTemplate, off::kDlgHeight), host);
            if (!g_sliders[t]) LOG("menu : curseurs de l'onglet %s refuses, il garde ses boutons", tab.title);
        }
        if (complete && tab.nameField) {
            BuildNameField(dialog, help, areaX + kOptionMargin, top + rows * pitch, width);
        }
        if (complete) RefreshOptions(dialog, tab);
        if (!complete || (tab.allowed && !tab.allowed())) Show(dialog, tab.id, false);
    }
    g_accessRevision = g_revision;
    // Une fois par forme du menu : ce que le moteur a accepté de créer, et ce qui est montré.
    static std::string said;
    std::string state;
    for (int t = 0; t < kTabCount; t++) {
        const Tab& tab = kTabs[t];
        const char* shown = !g_built[t] ? "refuse par le moteur" : (tab.allowed && !tab.allowed() ? "cache, acces non donne" : "montre");
        state += std::string(t ? ", " : "") + tab.title + " (" + shown + (g_built[t] && tab.sliderCount && !g_sliders[t] ? ", sans curseurs" : "") + ")";
    }
    if (state != said) {
        said = state;
        LOG("menu : onglets %s", state.c_str());
    }
    for (bool built : g_built) {
        if (built) return true;
    }
    return false;
}

// La page du moteur disparaît : ses zones de dessin, leurs cadres, ses boutons de page. C'est la
// disposition qu'il prend lui-même pour l'onglet Help (0x501EF0), zone principale en moins.
static void OpenTab(void* dialog, const Tab& tab) {
    fn::DialogRadioCheck(dialog, off::kPauseTabFirst, off::kPauseTabLast, tab.id);
    for (int t = 0; t < kTabCount; t++) {
        if (!g_built[t] || &kTabs[t] == &tab) continue;
        ShowOptions(dialog, kTabs[t], false);
        if (void* other = fn::DialogItem(dialog, kTabs[t].id)) fn::WindowSend(other, off::kBtSetCheck, 0, 0);
    }
    const uint16_t hidden[] = {off::kPauseMainArea, off::kPauseMainOutline, off::kPauseSubArea, off::kPauseSubOutline,
                               off::kPausePrevious,  off::kPauseNext,        off::kPausePreviousAlt,
                               off::kPauseNextAlt};
    for (uint16_t id : hidden) Show(dialog, id, false);
    RefreshOptions(dialog, tab);
    ShowOptions(dialog, tab, true);
    g_open = &tab;
}

// Un onglet du moteur vient d'être choisi : il remet lui-même ses boutons de page et sa zone
// secondaire (0x501EF0), pas la zone principale, qu'il ne cache jamais.
static void CloseTab(void* dialog) {
    ShowOptions(dialog, *g_open, false);
    if (void* tab = fn::DialogItem(dialog, g_open->id)) fn::WindowSend(tab, off::kBtSetCheck, 0, 0);
    Show(dialog, off::kPauseMainArea, true);
    Show(dialog, off::kPauseMainOutline, true);
    g_open = nullptr;
}

// Les onglets réservés suivent l'accès que donne l'hôte, menu ouvert compris : un client à qui il
// vient de l'ouvrir les voit paraître, celui à qui il le retire les voit partir, et l'onglet qu'il
// avait ouvert se referme.
static void SyncAccess(void* dialog) {
    g_accessRevision = g_revision;
    for (int t = 0; t < kTabCount; t++) {
        const Tab& tab = kTabs[t];
        if (!g_built[t] || !tab.allowed) continue;
        bool allowed = tab.allowed();
        Show(dialog, tab.id, allowed);
        if (allowed || g_open != &tab) continue;
        if (g_built[0]) OpenTab(dialog, kTabs[0]);
        else CloseTab(dialog);
    }
}

// Le curseur vient de bouger. La table change tout de suite, et la vie des PNJ de l'hôte avec
// elle ; le fichier et les clients suivent (voir coop::Update et CommitTable).
static void OnSliderMoved(const Slider& slider, void* control) {
    int position = fn::SliderGetPosition(control);
    if (position < 0) position = 0;
    if (position > slider.steps) position = slider.steps;
    // Un clic sans déplacement ne touche à rien, surtout pas à une ligne écrite à la main.
    if (position == SliderPosition(slider)) return;
    if (slider.set) {
        slider.set(slider.minimum + position * slider.step);
        return;
    }
    if (!slider.values) {
        alert::SetVolume(position * slider.step);
        g_volumeChangedAt = clk::Seconds();
        return;
    }

    difficulty::Table table = g_table;
    difficulty::SetPerClientBonus(table.*slider.values, static_cast<uint32_t>(position * slider.step));
    g_table = difficulty::Sanitized(table);
    g_tableLoaded = true;
    g_tableDirty = true;
    g_tableUnsent = true;
    g_tableChangedAt = clk::Seconds();
    // Régler les dégâts, c'est les vouloir : le réglage qui les annule est levé.
    if (slider.damage && !OnlineDamage()) SetOnlineDamage(true);
}

// Vrai : le message est traité, le moteur n'a pas à le voir. code : ce que le contrôle annonce.
static bool OnCommand(void* dialog, uint16_t id, uint32_t code, void* control) {
    if (id == kNameFieldId || id == kNameLabelId) return true;  // le champ se relève tout seul
    for (int t = 0; t < kTabCount; t++) {
        const Tab& tab = kTabs[t];
        if (!g_built[t]) continue;
        bool allowed = !tab.allowed || tab.allowed();
        if (id == tab.id) {
            if (allowed) OpenTab(dialog, tab);
            return true;
        }
        for (int i = 0; i < tab.count; i++) {
            const Option& option = tab.options[i];
            if (id != option.id) continue;
            if (!allowed) return true;
            // Chez un client, un réglage de partie se refuse de lui-même et garde la valeur de l'hôte.
            if (option.click) option.click(option.index);
            else if (option.get) option.set(!option.get());
            else option.next();
            RefreshOptions(dialog, tab);
            return true;
        }
        for (int i = 0; g_sliders[t] && i < tab.sliderCount; i++) {
            const Slider& slider = tab.sliders[i];
            if (id != slider.id) continue;
            if (code == off::kSlNotifyMoved && control) {
                OnSliderMoved(slider, control);
                RefreshOptions(dialog, tab);
            }
            return true;
        }
    }
    if (!g_open) return false;
    if (id >= off::kPauseTabFirst && id <= off::kPauseTabLast) {
        CloseTab(dialog);
        return false;
    }
    // Boutons de page du moteur, cachés mais encore joignables au clavier.
    return id == off::kPausePrevious || id == off::kPauseNext || id == off::kPausePreviousAlt ||
           id == off::kPauseNextAlt;
}

// Tant que le menu est ouvert, le jeu ne tourne plus et notre boucle non plus : sans nouvelles
// pendant cinq secondes, les autres machines nous croiraient partis. Le dialogue reçoit des
// messages en continu ; on en profite pour redire où est notre joueur. L'hôte continue aussi de
// passer à chaque client l'état des autres, et ses réglages, changés dans ce menu, partent sans
// attendre qu'il le referme.
static void KeepAlive() {
    static DWORD lastSent = 0;
    DWORD time = GetTickCount();
    if (!net::Running() || g_menusServed) return;  // servi : notre boucle tourne, elle s'en charge
    net::PumpRelay(clk::Seconds());
    if (time - lastSent < 100) return;
    lastSent = time;
    proto::EntityState player;
    if (rep::CollectLocalPlayer(player)) net::SendPlayer(player, mem::LevelHash());
}

// Volume réglé au curseur : gardé une fois le curseur au repos, et le son joué une fois pour en
// juger ; menu fermé avant, il est gardé sans être joué.
static void PollVolume(bool closing) {
    if (g_volumeChangedAt == 0.0) return;
    if (!closing && clk::Seconds() - g_volumeChangedAt < kVolumeSettle) return;
    g_volumeChangedAt = 0.0;
    alert::Save();
    if (!closing) alert::Preview();
}

static uint8_t ONICALL ONIMP_ENTRY PauseHook(void* dialog, uint32_t message, uint32_t param1, uint32_t param2) {
    KeepAlive();
    if (message == off::kWmInitDialog) {
        uint8_t result = g_pauseCallback(dialog, message, param1, param2);
        g_open = nullptr;
        g_dialog = nullptr;
        g_nameField = nullptr;
        memset(g_built, 0, sizeof(g_built));
        memset(g_sliders, 0, sizeof(g_sliders));
        // Hors session le mod ne montre rien : pas d'onglet non plus.
        if (net::CurrentMode() != cfg::Mode::Off) {
            if (BuildTabs(dialog)) g_dialog = dialog;
            else LOG("menu : dialogue de pause inattendu, pas d'onglets Coop et Display (les commandes mp_* restent)");
        }
        return result;
    }
    if (dialog == g_dialog) {
        // Un réglage a changé pendant que l'onglet est ouvert : l'hôte vient de le dire à ce client,
        // ou un joueur est arrivé et le minimum du multiplicateur d'ennemis a monté.
        if (g_accessRevision != g_revision && message != off::kWmDestroy) SyncAccess(dialog);
        if (g_open && g_shownRevision != g_revision && message != off::kWmDestroy) RefreshOptions(dialog, *g_open);
        if (g_open && g_open->nameField && message != off::kWmDestroy) PollNameField(false);
        PollVolume(message == off::kWmDestroy);
        if (message == off::kWmDestroy) {
            PollNameField(true);  // menu fermé sitôt le pseudo saisi
            g_nameField = nullptr;
            g_dialog = nullptr;
            g_open = nullptr;
            CommitTable();  // menu fermé avant que le curseur ait reposé une demi-seconde
        } else if (message == off::kWmCommand) {
            if (OnCommand(dialog, static_cast<uint16_t>(param1 & 0xFFFF), param1 >> 16,
                          reinterpret_cast<void*>(static_cast<uintptr_t>(param2)))) {
                return 1;
            }
        } else if (message == off::kWmDrawItem && g_open) {
            return 1;  // pas de page du moteur sous nos boutons
        }
    }
    return g_pauseCallback(dialog, message, param1, param2);
}

bool InstallMenuHook() {
    void* original = reinterpret_cast<void*>(addr::kPauseCallback);
    g_pauseCallback = reinterpret_cast<fn::DialogCallback>(original);
    return mem::PatchPointer(addr::kPauseCallbackOperand, original, reinterpret_cast<void*>(&PauseHook));
}

// -------------------------------------------------------------------------------------------------
// Console
// -------------------------------------------------------------------------------------------------
static uint16_t ONICALL ONIMP_ENTRY BslTeamKill(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) SetTeamKill(args[0].val.value_int32 != 0);
    else mem::ConsolePrint("OniMP : tir ami %s", g_teamKill ? "active" : "desactive");
    return 0;
}


static uint16_t ONICALL ONIMP_ENTRY BslOnlineDamage(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*,
                                                    sl_arg*) {
    if (numargs >= 1) SetOnlineDamage(args[0].val.value_int32 != 0);
    else mem::ConsolePrint("OniMP : degats des PNJ %s", OnlineDamage() ? "en ligne, plus forts a chaque client" : "normaux");
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslDifficulty(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1 && args[0].val.value_int32 >= 0 && args[0].val.value_int32 < rules::kModeCount) {
        SetDifficulty(static_cast<uint8_t>(args[0].val.value_int32));
    } else {
        mem::ConsolePrint("OniMP : difficulte %s (0 facile, 1 normal, 2 difficile, 3 survie)",
                          rules::Of(Difficulty()).name);
    }
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslEnemies(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) SetEnemyMultiplier(args[0].val.value_int32);
    else mem::ConsolePrint("OniMP : ennemis x%d (%d joueur(s))", EnemyMultiplier(), Players());
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslCivilians(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) SetCivilians(args[0].val.value_int32 != 0);
    else mem::ConsolePrint("OniMP : civils %s", Civilians() ? "presents" : "absents");
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslPlayerHealth(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*,
                                                    sl_arg*) {
    if (numargs >= 1) SetShowPlayerHealth(args[0].val.value_int32 != 0);
    else mem::ConsolePrint("OniMP : sante des joueurs %s", ShowPlayerHealth() ? "affichee" : "masquee");
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslNpcHealth(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) SetShowNpcHealth(args[0].val.value_int32 != 0);
    else mem::ConsolePrint("OniMP : sante des PNJ %s", ShowNpcHealth() ? "affichee" : "masquee");
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslMessages(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) {
        SetShowMessages(args[0].val.value_int32 != 0);
    } else {
        // La réponse à cette question doit se lire même quand les messages sont masqués.
        bool shown = ShowMessages();
        mem::ShowMessages(true);
        mem::ConsolePrint("OniMP : messages %s", shown ? "affiches" : "masques (le journal les garde)");
        mem::ShowMessages(shown);
    }
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslNpcIds(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) SetShowNpcIds(args[0].val.value_int32 != 0);
    else mem::ConsolePrint("OniMP : identifiants des PNJ %s", ShowNpcIds() ? "affiches" : "masques");
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslWeaponIds(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) SetShowWeaponIds(args[0].val.value_int32 != 0);
    else mem::ConsolePrint("OniMP : identifiants des armes %s", ShowWeaponIds() ? "affiches" : "masques");
    return 0;
}


static uint16_t ONICALL ONIMP_ENTRY BslPing(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) SetShowPing(args[0].val.value_int32 != 0);
    else mem::ConsolePrint("OniMP : latences des joueurs %s", ShowPing() ? "affichees" : "masquees");
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslName(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1 && args[0].val.value_str32) roster::SetLocalName(args[0].val.value_str32);
    else mem::ConsolePrint("OniMP : pseudo '%s'", roster::LocalName());
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslSkin(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) {
        roster::SetSkin(args[0].val.value_int32);
    } else {
        char skin[32];
        roster::SkinValue(skin, sizeof(skin));
        mem::ConsolePrint("OniMP : tenue %s (mp_skin 0 a %d pour en changer)", skin, roster::kSkinCount - 1);
    }
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslLanguage(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1 && args[0].val.value_str32) SetLanguage(loc::FromKey(args[0].val.value_str32, loc::Current()));
    else mem::ConsolePrint("OniMP : langue %s (mp_language \"fr\" ou \"en\")", loc::Label(loc::Current()));
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslAlertVolume(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) {
        alert::SetVolume(args[0].val.value_int32);
        alert::Save();
        g_revision++;  // le curseur du menu, s'il est ouvert, suit
    }
    mem::ConsolePrint("OniMP : volume de l'alerte %d %%", alert::Volume());
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslAutoPlay(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) {
        bot::SetEnabled(args[0].val.value_int32 != 0);
        g_revision++;
    } else {
        mem::ConsolePrint("OniMP : jeu automatique %s", bot::Enabled() ? "active" : "desactive");
    }
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslPortraits(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) {
        SetLowPortraits(args[0].val.value_int32 != 0);
        g_revision++;
    } else {
        mem::ConsolePrint("OniMP : portraits des dialogues %s", LowPortraits() ? "en bas" : "en haut");
    }
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslWallhack(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) {
        SetShowThroughWalls(args[0].val.value_int32 != 0);
        g_revision++;
    } else {
        mem::ConsolePrint("OniMP : %s %s", "joueurs a travers les murs", ShowThroughWalls() ? "affiches" : "masques");
    }
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslPlayerAmmo(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) SetShowPlayerAmmo(args[0].val.value_int32 != 0);
    else mem::ConsolePrint("OniMP : munitions des joueurs %s", ShowPlayerAmmo() ? "affichees" : "masquees");
    return 0;
}

void RegisterCommands() {
    fn::RegisterCommandVoid("mp_teamkill", "OniMP : les joueurs peuvent se blesser (hote)", "[enable:int | ]",
                            BslTeamKill);

    fn::RegisterCommandVoid("mp_onlinedamage", "OniMP : PNJ qui frappent plus fort par client (hote)",
                            "[enable:int | ]", BslOnlineDamage);
    fn::RegisterCommandVoid("mp_difficulty", "OniMP : difficulte, 0 facile, 1 normal, 2 difficile, 3 survie (hote)",
                            "[mode:int | ]", BslDifficulty);
    fn::RegisterCommandVoid("mp_enemies", "OniMP : multiplicateur d'ennemis, de 1 a 4 (hote)", "[count:int | ]",
                            BslEnemies);
    fn::RegisterCommandVoid("mp_civilians", "OniMP : civils meles aux ennemis (hote)", "[enable:int | ]",
                            BslCivilians);
    fn::RegisterCommandVoid("mp_playerhealth", "OniMP : sante des autres joueurs au-dessus d'eux", "[show:int | ]",
                            BslPlayerHealth);
    fn::RegisterCommandVoid("mp_npchealth", "OniMP : sante des PNJ au-dessus d'eux", "[show:int | ]", BslNpcHealth);
    fn::RegisterCommandVoid("mp_messages", "OniMP : messages du mod a l'ecran", "[show:int | ]", BslMessages);
    fn::RegisterCommandVoid("mp_npcids", "OniMP : nom de chaque PNJ au-dessus de lui", "[show:int | ]", BslNpcIds);
    fn::RegisterCommandVoid("mp_weaponids", "OniMP : nom de chaque arme au sol au-dessus d'elle", "[show:int | ]",
                            BslWeaponIds);
    fn::RegisterCommandVoid("mp_playerammo", "OniMP : chargeurs et cellules des autres joueurs au-dessus d'eux",
                            "[show:int | ]", BslPlayerAmmo);

    fn::RegisterCommandVoid("mp_ping", "OniMP : latence des autres joueurs a cote de leur pseudo", "[show:int | ]",
                            BslPing);
    fn::RegisterCommandVoid("mp_name", "OniMP : notre pseudo, quinze signes au plus", "[name:string | ]", BslName);
    fn::RegisterCommandVoid("mp_skin", "OniMP : notre tenue, par son numero", "[skin:int | ]", BslSkin);
    fn::RegisterCommandVoid("mp_language", "OniMP : langue des textes du mod, fr ou en", "[language:string | ]",
                            BslLanguage);
    fn::RegisterCommandVoid("mp_alertvolume", "OniMP : volume du son joue quand un joueur tombe, de 0 a 100",
                            "[percent:int | ]", BslAlertVolume);
    fn::RegisterCommandVoid("mp_wallhack", "OniMP : les autres joueurs vus a travers les murs", "[show:int | ]",
                            BslWallhack);
    fn::RegisterCommandVoid("mp_autoplay", "OniMP : notre personnage suit l'hote et se bat seul (client)",
                            "[enable:int | ]", BslAutoPlay);
    fn::RegisterCommandVoid("mp_portraits", "OniMP : portraits des dialogues en bas de l'ecran", "[low:int | ]",
                            BslPortraits);
}

}  // namespace coop
