#include "cheat.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "config.h"
#include "coop.h"
#include "lang.h"
#include "log.h"
#include "memory.h"
#include "network.h"
#include "protocol.h"
#include "roster.h"

using namespace oni;

namespace cheat {

static bool Host() { return net::CurrentMode() == cfg::Mode::Host; }
static bool Client() { return net::CurrentMode() == cfg::Mode::Client; }
static uint8_t Local() { return net::LocalPlayerId(); }

// -------------------------------------------------------------------------------------------------
// Accès
// -------------------------------------------------------------------------------------------------
// L'hôte garde l'interrupteur général et les accès dans son onimp.ini ; un client tient les deux de
// lui, et n'a plus rien quand il s'en va.
static bool g_hostMaster = false;       // client : interrupteur de l'hôte
static uint8_t g_hostMask = 0;          // client : accès donnés par l'hôte
static void ClearMorph(uint8_t player);
static void SendMorphs();
static void RequestMorph(const char* className);

bool Master() { return Client() ? g_hostMaster : cfg::Get().cheats; }
uint8_t AccessMask() { return Client() ? g_hostMask : static_cast<uint8_t>((cfg::Get().cheatAccess & 0xE) | 1); }
bool Allowed(uint8_t player) { return player < proto::kMaxPlayers && (AccessMask() >> player) & 1; }

bool Access() {
    if (!net::Running()) return false;
    return Host() || (net::Connected() && Allowed(Local()));
}

bool Active() { return Access() && Master(); }

static bool HostOnly() {
    if (!Client()) return true;
    mem::ConsolePrint("OniMP : reglage reserve a l'hote");
    return false;
}

void SetMaster(bool enabled) {
    if (!HostOnly() || cfg::Get().cheats == enabled) return;
    cfg::Get().cheats = enabled;
    cfg::SaveRules();
    LOG("cheats : interrupteur general %s", enabled ? "ouvert" : "coupe");
    mem::ConsolePrint(enabled ? "OniMP : cheats actives par l'hote" : "OniMP : cheats coupes par l'hote");
    coop::Resend();
    coop::Touch();
}

void SetAllowed(uint8_t player, bool allowed) {
    if (!HostOnly() || player == proto::kHostPlayerId || player >= proto::kMaxPlayers || Allowed(player) == allowed) return;
    int& mask = cfg::Get().cheatAccess;
    mask = allowed ? mask | (1 << player) : mask & ~(1 << player);
    cfg::SaveRules();
    LOG("cheats : joueur %u %s", player, allowed ? "autorise" : "interdit");
    mem::ConsolePrint(allowed ? "OniMP : joueur %u autorise aux cheats" : "OniMP : joueur %u interdit de cheats", player);
    // Sans accès, plus de personnage incarné non plus : il redevient Konoko chez tous.
    if (!allowed) ClearMorph(player);
    coop::Resend();
    coop::Touch();
}

void OnHostSettings(bool master, uint8_t accessMask) {
    if (!Client()) return;
    bool had = Allowed(Local()), wasOn = g_hostMaster;
    g_hostMaster = master;
    g_hostMask = accessMask;
    bool has = Allowed(Local());
    if (has != had) {
        LOG("cheats : l'hote nous %s les onglets Cheat, Character et Spawn", has ? "ouvre" : "retire");
        mem::ConsolePrint(has ? "OniMP : l'hote vous ouvre les onglets Cheat, Character et Spawn"
                              : "OniMP : l'hote vous retire les onglets Cheat, Character et Spawn");
        // Le personnage gardé du lancement précédent est redemandé dès que l'hôte le permet.
        if (has && !cfg::Get().character.empty()) RequestMorph(cfg::Get().character.c_str());
    }
    if (master != wasOn && has) {
        LOG("cheats : interrupteur general de l'hote %s", master ? "ouvert" : "coupe");
        mem::ConsolePrint(master ? "OniMP : cheats actives par l'hote" : "OniMP : cheats coupes par l'hote");
    }
}

// -------------------------------------------------------------------------------------------------
// Cheats de ce joueur
// -------------------------------------------------------------------------------------------------
static double g_dirtyAt = 0.0;          // un curseur a bougé : gardé dans onimp.ini une fois au repos
static double g_now = 0.0;
static bool g_ammoOurs = false;         // le drapeau « munitions infinies » de notre joueur vient de nous
static bool g_auraOurs = false;         // l'aura demandée à notre joueur aussi
static int g_attackCarry = 0;           // centièmes d'image d'avance pas encore rendus
static void* g_jumpClass = nullptr;     // classe dont le saut est modifié le temps d'un appel du moteur
static float g_jumpVelocity = 0.0f, g_jumpPush = 0.0f;
static const double kSaveDelay = 0.5;

bool God() { return cfg::Get().cheatGod; }
bool Ammo() { return cfg::Get().cheatAmmo; }
bool OneHit() { return cfg::Get().cheatOneHit; }
bool Aura() { return cfg::Get().cheatAura; }
int AttackSpeed() { return cfg::Get().cheatAttackSpeed; }
int MoveSpeed() { return cfg::Get().cheatMoveSpeed; }
int Jump() { return cfg::Get().cheatJump; }

// message prend on ou off : « activee », « desactives », accordés à ce qu'il nomme.
static void Toggle(bool& option, bool enabled, const char* log, const char* message, const char* on, const char* off) {
    if (option == enabled) return;
    option = enabled;
    cfg::SaveCheats();
    LOG("cheats : %s %s%s", log, enabled ? "oui" : "non", Active() ? "" : " (sans effet : cheats coupes ou acces refuse)");
    mem::ConsolePrint(message, enabled ? on : off);
    // Réglé, mais sans effet pour l'instant : le dire, plutôt que de laisser croire à une panne.
    if (enabled && Access() && !Master()) mem::ConsolePrint("OniMP : sans effet tant que l'hote laisse les cheats coupes");
    coop::Touch();
}

void SetGod(bool enabled) { Toggle(cfg::Get().cheatGod, enabled, "invincibilite", "OniMP : invincibilite %s", "activee", "desactivee"); }
void SetAmmo(bool enabled) {
    Toggle(cfg::Get().cheatAmmo, enabled, "munitions illimitees", "OniMP : munitions illimitees %s", "activees", "desactivees");
}
void SetOneHit(bool enabled) { Toggle(cfg::Get().cheatOneHit, enabled, "coup fatal", "OniMP : coup fatal %s", "active", "desactive"); }
void SetAura(bool enabled) { Toggle(cfg::Get().cheatAura, enabled, "aura permanente", "OniMP : aura permanente %s", "activee", "desactivee"); }

static void SetPercent(int& option, int percent, int most) {
    percent = percent < 100 ? 100 : (percent > most ? most : percent);
    if (option == percent) return;
    option = percent;
    g_dirtyAt = g_now > 0.0 ? g_now : 0.001;
    coop::Touch();
}

void SetAttackSpeed(int percent) { SetPercent(cfg::Get().cheatAttackSpeed, percent, kMaxSpeed); }
void SetMoveSpeed(int percent) { SetPercent(cfg::Get().cheatMoveSpeed, percent, kMaxSpeed); }
void SetJump(int percent) { SetPercent(cfg::Get().cheatJump, percent, kMaxJump); }

bool GodMode() { return cfg::Get().cheatGod && Active(); }
bool NoFallDamage() { return cfg::Get().cheatJump > 100 && Active(); }

uint32_t StrikeDamage(Character* player, Character* victim, uint32_t damage) {
    if (!cfg::Get().cheatOneHit || !Active() || !player || !victim || victim == player || mem::IsDead(victim)) return damage;
    // Un ennemi seulement, c'est-à-dire d'une équipe en guerre avec la nôtre : ni un allié, ni un
    // civil, dont la mort ferait échouer la mission.
    if (fn::FriendOrFoe(mem::Team(victim), mem::Team(player)) != 2) return damage;
    int32_t health = mem::Health(victim);
    return health > 0 && damage < static_cast<uint32_t>(health) ? static_cast<uint32_t>(health) : damage;
}

// Le saut de la classe, le temps d'un appel du moteur pour notre joueur : vitesse de départ et
// poussée de la touche tenue. La classe retrouve ses valeurs aussitôt après : un PNJ de la même
// classe (quand on incarne un ennemi) saute comme avant.
static void RaiseJump(Character* chr) {
    void* characterClass = field<void*>(chr, off::kChClass);
    if (!characterClass || g_jumpClass) return;
    float factor = static_cast<float>(cfg::Get().cheatJump) / 100.0f;
    g_jumpClass = characterClass;
    g_jumpVelocity = field<float>(characterClass, off::kClassJumpVelocity);
    g_jumpPush = field<float>(characterClass, off::kClassJumpPush);
    if (!std::isfinite(g_jumpVelocity) || !std::isfinite(g_jumpPush)) return;
    field<float>(characterClass, off::kClassJumpVelocity) = g_jumpVelocity * factor;
    field<float>(characterClass, off::kClassJumpPush) = g_jumpPush * factor;
}

static void RestoreJump() {
    if (!g_jumpClass) return;
    field<float>(g_jumpClass, off::kClassJumpVelocity) = g_jumpVelocity;
    field<float>(g_jumpClass, off::kClassJumpPush) = g_jumpPush;
    g_jumpClass = nullptr;
}

// Animations d'attaque plus rapides : le moteur avance d'une image par tick, on en saute ici de
// quoi tenir la cadence voulue, sans toucher à la dernière, où il enchaîne l'animation suivante.
// Le temps de récupération entre deux attaques s'use du même pas. Une prise garde sa cadence : la
// victime joue la sienne en même temps.
static void HurryAttack(Character* chr) {
    int speed = cfg::Get().cheatAttackSpeed;
    ActiveCharacter* active = mem::Active(chr);
    if (speed <= 100 || !active) {
        g_attackCarry = 0;
        return;
    }
    g_attackCarry += speed - 100;
    int extra = g_attackCarry / 100;
    g_attackCarry %= 100;
    if (extra <= 0) return;
    for (size_t pause : {off::kAcSoftPause, off::kAcHardPause}) {
        uint16_t& left = field<uint16_t>(active, pause);
        left = left > extra ? static_cast<uint16_t>(left - extra) : 0;
    }
    void* tram = field<void*>(active, off::kAcAnimation);
    if (!mem::AnimAttacks(tram)) return;
    if (field<uint32_t>(tram, off::kTramFlags) & (off::kTramFlagThrowSource | off::kTramFlagThrowTarget)) return;
    int16_t& frame = field<int16_t>(active, off::kAcAnimFrame);
    int last = static_cast<int>(field<uint16_t>(tram, off::kTramDuration)) - 1;
    if (frame < 0 || frame + extra > last) extra = last - frame;
    if (extra > 0) frame = static_cast<int16_t>(frame + extra);
}

// Déplacements plus rapides : le pas du tick, à plat, est allongé avant que la physique ne
// l'applique ; c'est elle qui arrête le personnage contre le décor, comme d'ordinaire. Un coup
// garde son élan, et un joueur qui encaisse ou qui est au sol n'est pas poussé plus loin.
static void StretchStep(Character* chr) {
    int speed = cfg::Get().cheatMoveSpeed;
    ActiveCharacter* active = mem::Active(chr);
    if (speed <= 100 || !active) return;
    if (mem::AnimAttacks(field<void*>(active, off::kAcAnimation)) || mem::Fallen(chr)) return;
    if (field<uint16_t>(active, off::kAcHitStun) || field<uint16_t>(active, off::kAcBlockStun)) return;
    Vector3& step = field<Vector3>(active, off::kAcStep);
    if (!std::isfinite(step.x) || !std::isfinite(step.z)) return;
    float factor = static_cast<float>(speed) / 100.0f;
    step.x *= factor;
    step.z *= factor;
}

static void OnStep(Character* chr, mem::StepPhase phase) {
    if (phase == mem::StepPhase::AfterInput || phase == mem::StepPhase::AfterStep) RestoreJump();
    if (chr != mem::Player() || !Active()) return;
    switch (phase) {
        case mem::StepPhase::BeforeInput:
            if (cfg::Get().cheatJump > 100) RaiseJump(chr);
            break;
        case mem::StepPhase::BeforeStep:
            if (cfg::Get().cheatJump > 100) RaiseJump(chr);
            HurryAttack(chr);
            break;
        case mem::StepPhase::AfterStep:
            StretchStep(chr);
            break;
        default:
            break;
    }
}

bool Install() { return mem::InstallStepHook(&OnStep); }

// -------------------------------------------------------------------------------------------------
// Classes de personnages
// -------------------------------------------------------------------------------------------------
enum Filter { kFilterAll, kFilterKonoko, kFilterTctf, kFilterSyndicate, kFilterBoss, kFilterCivilian, kFilterOther, kFilterCount };
static const char* const kFilterNames[kFilterCount] = {"All", "Konoko", "TCTF", "Syndicate", "Bosses", "Civilians", "Other"};

static bool Starts(const char* name, const char* prefix) { return _strnicmp(name, prefix, strlen(prefix)) == 0; }

// D'après les noms des classes de l'Anniversary Edition. Ce qui ne ressemble à rien de connu (une
// classe ajoutée par un mod) va dans « Other ».
static int Category(const char* name) {
    for (const char* boss : {"barabus", "muro", "mutantmuro", "super_ninja", "mad_bomber", "griffin", "SH_", "Shinzom", "ninjabot"}) {
        if (Starts(name, boss)) return kFilterBoss;
    }
    if (Starts(name, "konoko") || _stricmp(name, "k3") == 0 || Starts(name, "k4_")) return kFilterKonoko;
    for (const char* tctf : {"TCTF", "Cop", "security_guard"}) {
        if (Starts(name, tctf)) return kFilterTctf;
    }
    for (const char* syndicate : {"striker", "thug", "comguy", "elite", "red_", "ninja", "tanker", "sniper"}) {
        if (Starts(name, syndicate)) return kFilterSyndicate;
    }
    return Starts(name, "generic_") ? kFilterCivilian : kFilterOther;
}

// Toutes les classes chargées, par ordre alphabétique. Relevées à chaque niveau : un niveau peut en
// apporter que les données communes n'ont pas.
static std::vector<std::string> g_classes;
static uint32_t g_classesLevel = 0;

static const std::vector<std::string>& Classes() {
    if (g_classesLevel == mem::LevelHash() && !g_classes.empty()) return g_classes;
    g_classes.clear();
    g_classesLevel = mem::LevelHash();
    int count = mem::CharacterClassCount();
    for (int i = 0; i < count && i < 1024; i++) {
        const char* name = mem::CharacterClassAt(i);
        if (name[0] && strlen(name) < proto::kClassLength) g_classes.push_back(name);
    }
    std::sort(g_classes.begin(), g_classes.end(),
              [](const std::string& a, const std::string& b) { return _stricmp(a.c_str(), b.c_str()) < 0; });
    LOG("classes : %u classe(s) de personnage dans les donnees chargees", static_cast<unsigned>(g_classes.size()));
    return g_classes;
}

static std::vector<const char*> Filtered(int filter) {
    std::vector<const char*> out;
    for (const std::string& name : Classes()) {
        if (filter == kFilterAll || Category(name.c_str()) == filter) out.push_back(name.c_str());
    }
    return out;
}

static bool ClassKnown(const char* name) {
    void* instance = nullptr;
    return name && name[0] && fn::GetDataPtr(Tag('O', 'N', 'C', 'C'), name, &instance) == 0 && instance;
}

// Un nom de classe venu du réseau ou du fichier : lettres, chiffres, « _ » et « - », rien d'autre.
static bool CleanName(const char* name, char* out) {
    size_t length = 0;
    for (; name[length] && length < proto::kClassLength; length++) {
        char c = name[length];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    if (length >= proto::kClassLength) return false;
    memset(out, 0, proto::kClassLength);
    memcpy(out, name, length);
    return true;
}

// -------------------------------------------------------------------------------------------------
// Personnage incarné
// -------------------------------------------------------------------------------------------------
static char g_morphs[proto::kMaxPlayers][proto::kClassLength] = {};  // hôte : ce qu'il décide ; client : ce qu'il a dit
static int g_characterFilter = kFilterAll, g_characterPage = 0;

const char* CharacterOf(uint8_t player) {
    return net::Running() && player < proto::kMaxPlayers ? g_morphs[player] : "";
}

static void SendMorphs() {
    if (!Host() || !net::Connected()) return;
    uint8_t packet[1 + sizeof(g_morphs)];
    packet[0] = proto::kReliableMorphs;
    memcpy(packet + 1, g_morphs, sizeof(g_morphs));
    net::SendReliable(packet, sizeof(packet));
}

// Hôte : ce joueur incarne désormais cette classe ("" : Konoko). Dit à tous.
static void StoreMorph(uint8_t player, const char* className) {
    if (strncmp(g_morphs[player], className, proto::kClassLength) == 0) return;
    memset(g_morphs[player], 0, proto::kClassLength);
    strncpy(g_morphs[player], className, proto::kClassLength - 1);
    LOG("personnage : joueur %u incarne %s", player, className[0] ? className : "Konoko, comme a l'origine");
    roster::Redress(player);
    SendMorphs();
    coop::Touch();
}

static void ClearMorph(uint8_t player) {
    if (Host() && player < proto::kMaxPlayers) StoreMorph(player, "");
}

static void RequestMorph(const char* className) {
    proto::Morph morph = {};
    if (!Client() || !net::Connected() || !CleanName(className, morph.className)) return;
    uint8_t packet[1 + sizeof(morph)];
    packet[0] = proto::kReliableMorph;
    memcpy(packet + 1, &morph, sizeof(morph));
    net::SendReliable(packet, sizeof(packet));
}

void SetCharacter(const char* className) {
    char clean[proto::kClassLength];
    if (!Access()) {
        mem::ConsolePrint("OniMP : personnage : acces non donne par l'hote");
        return;
    }
    if (!className || !CleanName(className, clean) || (clean[0] && !ClassKnown(clean))) {
        mem::ConsolePrint("OniMP : personnage inconnu dans ce niveau");
        return;
    }
    cfg::Get().character = clean;
    cfg::SaveCheats();
    mem::ConsolePrint("OniMP : personnage %s", clean[0] ? clean : "Konoko");
    if (Host()) StoreMorph(proto::kHostPlayerId, clean);
    else RequestMorph(clean);
}

void ResetCharacter() { SetCharacter(""); }

// Konoko, puis chaque classe de la liste filtrée, puis Konoko de nouveau.
void NextCharacter() {
    std::vector<const char*> list = Filtered(g_characterFilter);
    const char* current = cfg::Get().character.c_str();
    size_t next = 0;
    if (current[0]) {
        next = list.size();  // classe hors de la liste filtrée : retour à Konoko
        for (size_t i = 0; i < list.size(); i++) {
            if (_stricmp(list[i], current) == 0) next = i + 1;
        }
    }
    SetCharacter(next < list.size() ? list[next] : "");
}

// Touche « personnage suivant » : F5 à F12, au choix du joueur.
static const char* const kKeys[] = {"F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12"};
static const int kKeyCount = 8;

static int KeyIndex() {
    for (int i = 0; i < kKeyCount; i++) {
        if (_stricmp(cfg::Get().characterKey.c_str(), kKeys[i]) == 0) return i;
    }
    return 0;
}

// La fenêtre du jeu est au premier plan : la touche n'est pas pour un autre programme, ni pour
// l'autre instance du jeu sur la même machine.
static bool OwnWindowActive() {
    DWORD process = 0;
    HWND window = GetForegroundWindow();
    return window && GetWindowThreadProcessId(window, &process) && process == GetCurrentProcessId();
}

static void PollKey() {
    static bool held = false;
    bool down = (GetAsyncKeyState(VK_F5 + KeyIndex()) & 0x8000) != 0 && OwnWindowActive();
    if (down && !held && Access()) NextCharacter();
    held = down;
}

// -------------------------------------------------------------------------------------------------
// Apparitions
// -------------------------------------------------------------------------------------------------
struct Item {
    const char* label;
    uint16_t type;       // type du bonus dans le moteur
    uint16_t amount;     // 1, ou la charge d'un bouclier ou d'une invisibilité neufs
};
static const Item kItems[] = {
    {"Hypo spray", 2, 1}, {"Ammo clip", 0, 1}, {"Energy cell", 1, 1}, {"Force shield", 3, 100}, {"Phase cloak", 4, 1800},
};
static const int kItemCount = 5;
// Noms des armes du jeu ; une classe absente de cette table (les armes de boss, celles d'un mod)
// est montrée sous son nom de classe.
static const struct {
    const char* className;
    const char* label;
} kWeaponLabels[] = {
    {"w1_tap", "Campbell Equalizer"}, {"w2_sap", "Black Adder SMG"}, {"w3_phr", "Plasma Rifle"},
    {"w4_psm", "Phase Stream"},       {"w5_sbg", "Super Ball Gun"},  {"w6_vdg", "Van de Graaff"},
    {"w7_scc", "Scram Cannon"},       {"w8_mbo", "Mercury Bow"},     {"w9_scr", "Screaming Cannon"},
    {"w11_ba1", "Wave Motion Cannon"},
};
static const float kItemAhead = 9.0f, kWeaponAhead = 10.0f, kNpcAhead = 22.0f;  // devant le joueur, en unités
static const float kPi = 3.14159265f;
static const int kMaxSpawned = 24;      // PNJ apparus encore en vie : le moteur n'a que 128 personnages
static const char* const kSpawnPrefix = "mpx";

static int g_spawnKind = proto::kSpawnNpc, g_spawnFilter = kFilterSyndicate, g_spawnPage = 0;
static int g_itemCount = 1;             // bonus posés d'un clic : 1, 3 ou 6
static int g_ammoPercent = 100;         // cartouches d'une arme posée
static bool g_spawnAlly = false;        // camp d'un PNJ : allié, sinon ennemi
static unsigned g_spawnSequence = 0, g_spawnsMade = 0;

static const char* WeaponLabel(const char* className) {
    for (const auto& entry : kWeaponLabels) {
        if (_stricmp(entry.className, className) == 0) return entry.label;
    }
    return className;
}

// Devant ce personnage, à cette distance au plus : jamais derrière ce qui lui barre la route.
static Vector3 Ahead(Character* chr, float distance) {
    Vector3 here = mem::Location(chr);
    float facing = field<float>(chr, off::kChFacing);
    if (!std::isfinite(facing)) facing = 0.0f;
    float dx = std::sin(facing), dz = std::cos(facing);
    Vector3 origin = {here.x, here.y + 8.0f, here.z};
    Vector3 target = {here.x + dx * distance, here.y + 8.0f, here.z + dz * distance};
    mem::Ray ray;
    if (mem::CastRay(chr, origin, target, ray) && ray.kind != mem::RayKind::Nothing && ray.distance < distance + 2.0f) {
        distance = ray.distance > 5.0f ? ray.distance - 3.0f : 2.0f;
    }
    return {here.x + dx * distance, here.y, here.z + dz * distance};
}

// Une équipe en guerre avec celle-ci, dans les deux sens.
static uint16_t HostileTeam(uint16_t own) {
    for (uint16_t team : {2, 1, 4, 7, 5, 6, 3, 0}) {
        if (team != own && fn::FriendOrFoe(team, own) == 2 && fn::FriendOrFoe(own, team) == 2) return team;
    }
    return 2;
}

static int SpawnedAlive() {
    int alive = 0, count = mem::CharacterCount();
    for (int i = 0; i < count; i++) {
        Character* chr = mem::CharacterAt(i);
        if (chr && mem::InUse(chr) && !mem::IsDead(chr) && strncmp(mem::Name(chr), kSpawnPrefix, 3) == 0) alive++;
    }
    return alive;
}

// Crée ici ce que décrit la demande. by : le joueur qui l'a faite, pour le journal.
static void DoSpawn(const proto::Spawn& spawn, uint8_t by) {
    Character* player = mem::Player();
    Vector3 at = {spawn.x, spawn.y, spawn.z};
    char name[proto::kClassLength] = {};
    if (!player || !std::isfinite(at.x) || !std::isfinite(at.y) || !std::isfinite(at.z) || !std::isfinite(spawn.facing)) return;
    if (!CleanName(spawn.name, name)) return;

    if (spawn.kind == proto::kSpawnItem) {
        if (spawn.type >= off::kPuTypeLsi) return;  // l'objet du niveau reste aux scripts
        int wanted = spawn.amount < 1 ? 1 : (spawn.amount > 6 ? 6 : spawn.amount), placed = 0;
        uint16_t charge = spawn.type == 3 ? 100 : (spawn.type == 4 ? 1800 : 1);
        for (int i = 0; i < wanted; i++) {
            Vector3 place = {at.x + static_cast<float>(i % 3 - 1) * 3.0f, at.y, at.z + static_cast<float>(i / 3) * 3.0f};
            if (mem::NewPowerup(spawn.type, charge, place)) placed++;
        }
        g_spawnsMade += static_cast<unsigned>(placed);
        LOG("apparition : %d bonus de type %u pose(s) pour le joueur %u en (%.0f %.0f %.0f)", placed, spawn.type, by, at.x,
            at.y, at.z);
    } else if (spawn.kind == proto::kSpawnWeapon) {
        void* weapon = mem::SpawnWeapon(name, at, spawn.facing);
        if (!weapon) {
            LOG("apparition : arme '%s' refusee par le moteur (classe inconnue, ou plus de place)", name);
            return;
        }
        unsigned percent = spawn.amount > 100 ? 100 : spawn.amount;
        mem::SetWeaponAmmo(weapon, static_cast<uint16_t>(mem::WeaponCapacity(weapon) * percent / 100));
        g_spawnsMade++;
        LOG("apparition : arme '%s' posee pour le joueur %u, %u cartouche(s)", name, by,
            static_cast<unsigned>(mem::WeaponAmmo(weapon)));
        // Rien ne porte une arme du sol de l'hôte jusqu'aux clients : elle leur est annoncée.
        if (Host() && net::Connected() && cfg::Get().npcs) {
            uint8_t packet[1 + sizeof(spawn)];
            packet[0] = proto::kReliableSpawned;
            memcpy(packet + 1, &spawn, sizeof(spawn));
            net::SendReliable(packet, sizeof(packet));
        }
    } else if (spawn.kind == proto::kSpawnNpc) {
        if (!ClassKnown(name)) return;
        if (SpawnedAlive() >= kMaxSpawned) {
            LOG("apparition : PNJ refuse, %d sont deja la", kMaxSpawned);
            mem::ConsolePrint("OniMP : apparition refusee (%d PNJ deja la)", kMaxSpawned);
            return;
        }
        uint16_t own = mem::Team(player);
        uint16_t team = spawn.type ? own : HostileTeam(own);
        // Son nom porte son équipe et sa classe : c'est ce qui le fait recréer chez les clients.
        char npc[32];
        snprintf(npc, sizeof(npc), "%s%u_%02u_%s", kSpawnPrefix, team, g_spawnSequence++ % 100, name);
        if (!mem::SpawnNpc(name, npc, at, spawn.facing, team)) return;
        g_spawnsMade++;
        LOG("apparition : PNJ '%s' cree pour le joueur %u, equipe %u (%s)", npc, by, team, spawn.type ? "allie" : "ennemi");
    }
}

bool CreateSpawned(const char* name, const Vector3& position, float facing) {
    unsigned team = 0, sequence = 0;
    char className[proto::kClassLength] = {};
    if (!name || strncmp(name, kSpawnPrefix, 3) != 0 || sscanf(name + 3, "%u_%u_%23s", &team, &sequence, className) != 3) {
        return false;
    }
    // Un pantin : son IA reste à l'arrêt ici, l'hôte le pilote. Ni increvable ni armé d'office.
    if (ClassKnown(className) && mem::SpawnPuppet(className, name, position, facing, static_cast<uint16_t>(team & 7), 0)) {
        LOG("apparition : PNJ '%s' cree d'apres l'hote", name);
    }
    return true;
}

// Notre joueur demande une apparition devant lui. Avec des PNJ partagés c'est l'hôte qui la crée ;
// sinon chacun a son propre monde, et elle n'existe qu'ici.
static void RequestSpawn(uint8_t kind, uint8_t type, uint16_t amount, const char* name, float ahead, const char* label) {
    Character* player = mem::Player();
    if (!player || !Access()) return;
    proto::Spawn spawn = {};
    Vector3 at = Ahead(player, ahead);
    float facing = field<float>(player, off::kChFacing);
    spawn.levelHash = mem::LevelHash();
    spawn.x = at.x;
    spawn.y = at.y;
    spawn.z = at.z;
    // Un PNJ arrive face au joueur.
    spawn.facing = kind == proto::kSpawnNpc ? std::fmod(facing + kPi, 2.0f * kPi) : facing;
    spawn.kind = kind;
    spawn.type = type;
    spawn.amount = amount;
    strncpy(spawn.name, name, proto::kClassLength - 1);
    mem::ConsolePrint("OniMP : apparition : %s", label);
    if (Client() && cfg::Get().npcs) {
        uint8_t packet[1 + sizeof(spawn)];
        packet[0] = proto::kReliableSpawn;
        memcpy(packet + 1, &spawn, sizeof(spawn));
        net::SendReliable(packet, sizeof(packet));
    } else {
        DoSpawn(spawn, Local());
    }
}

// -------------------------------------------------------------------------------------------------
// Réseau
// -------------------------------------------------------------------------------------------------
void OnReliable(uint8_t kind, uint8_t from, const uint8_t* data, int size) {
    if (kind == proto::kReliableMorph) {
        proto::Morph morph;
        char clean[proto::kClassLength];
        if (!Host() || from == proto::kHostPlayerId || from >= proto::kMaxPlayers || size < static_cast<int>(sizeof(morph))) return;
        memcpy(&morph, data, sizeof(morph));
        morph.className[proto::kClassLength - 1] = '\0';
        if (!Allowed(from) || !CleanName(morph.className, clean) || (clean[0] && !ClassKnown(clean))) {
            LOG("personnage : demande du joueur %u refusee ('%s' : %s)", from, morph.className,
                Allowed(from) ? "classe inconnue ici" : "acces non donne");
            SendMorphs();  // il retrouve ce qui vaut
            return;
        }
        StoreMorph(from, clean);
    } else if (kind == proto::kReliableMorphs) {
        proto::Morphs morphs;
        if (!Client() || from != proto::kHostPlayerId || size < static_cast<int>(sizeof(morphs))) return;
        memcpy(&morphs, data, sizeof(morphs));
        for (uint8_t player = 0; player < proto::kMaxPlayers; player++) {
            char clean[proto::kClassLength];
            morphs.className[player][proto::kClassLength - 1] = '\0';
            if (!CleanName(morphs.className[player], clean) || strcmp(clean, g_morphs[player]) == 0) continue;
            memcpy(g_morphs[player], clean, proto::kClassLength);
            LOG("personnage : joueur %u incarne %s, d'apres l'hote", player, clean[0] ? clean : "Konoko");
            roster::Redress(player);
        }
        coop::Touch();
    } else if (kind == proto::kReliableSpawn || kind == proto::kReliableSpawned) {
        proto::Spawn spawn;
        if (size < static_cast<int>(sizeof(spawn))) return;
        memcpy(&spawn, data, sizeof(spawn));
        spawn.name[proto::kClassLength - 1] = '\0';
        if (spawn.levelHash != mem::LevelHash()) return;
        if (kind == proto::kReliableSpawn) {
            // Un client demande : seul l'hôte crée, et seulement pour qui il a autorisé.
            if (!Host() || from == proto::kHostPlayerId) return;
            if (!Allowed(from)) {
                LOG("apparition : demande du joueur %u refusee, acces non donne", from);
                return;
            }
            DoSpawn(spawn, from);
        } else if (Client() && from == proto::kHostPlayerId && spawn.kind == proto::kSpawnWeapon) {
            DoSpawn(spawn, from);
        }
    }
}

void OnSessionStarted(bool hosting) {
    memset(g_morphs, 0, sizeof(g_morphs));
    g_hostMaster = false;
    g_hostMask = 0;
    if (!hosting) return;
    char clean[proto::kClassLength];
    if (CleanName(cfg::Get().character.c_str(), clean)) memcpy(g_morphs[proto::kHostPlayerId], clean, proto::kClassLength);
    LOG("cheats de l'hote repris d'onimp.ini : interrupteur general %d, acces 0x%X, personnage '%s'", Master() ? 1 : 0,
        AccessMask(), g_morphs[proto::kHostPlayerId]);
}

void OnConnection(uint8_t player, bool connected) {
    if (Host()) {
        if (player == proto::kHostPlayerId || player >= proto::kMaxPlayers) return;
        if (connected) SendMorphs();  // à tous : sans effet chez ceux qui les ont déjà
        else ClearMorph(player);
        return;
    }
    if (connected) return;
    // Sans hôte, plus d'accès ni de personnage décidé par lui.
    g_hostMaster = false;
    g_hostMask = 0;
    for (uint8_t other = 0; other < proto::kMaxPlayers; other++) {
        if (!g_morphs[other][0]) continue;
        memset(g_morphs[other], 0, proto::kClassLength);
        roster::Redress(other);
    }
    coop::Touch();
}

void OnLevelChanged() {
    // Le niveau a refait notre joueur : ni drapeau ni aura ne lui restent de nous.
    g_ammoOurs = false;
    g_auraOurs = false;
    g_attackCarry = 0;
    g_characterPage = 0;
    g_spawnPage = 0;
}

void Update(double now) {
    g_now = now;
    if (g_dirtyAt != 0.0 && now - g_dirtyAt >= kSaveDelay) {
        g_dirtyAt = 0.0;
        cfg::SaveCheats();
    }
    Character* player = mem::Player();
    if (!player) return;
    bool active = Active();
    // Munitions illimitées : le drapeau des personnages qui tirent sans compter (0x51A99B : l'arme
    // en main est regarnie à chaque tir), posé tant que le cheat agit et retiré ensuite si c'est
    // nous qui l'avions posé.
    if (active && cfg::Get().cheatAmmo) {
        if (!mem::HasInfiniteAmmo(player)) {
            mem::SetInfiniteAmmo(player);
            g_ammoOurs = true;
        }
    } else if (g_ammoOurs) {
        mem::ClearInfiniteAmmo(player);
        g_ammoOurs = false;
    }
    // Aura permanente : la valeur que pose la commande de script chr_super. Le moteur en tire la
    // lueur et les coups plus forts, et l'état transmis aux autres machines la montre chez elles.
    if (active && cfg::Get().cheatAura) {
        mem::SetSuper(player, 1.0f);
        g_auraOurs = true;
    } else if (g_auraOurs) {
        mem::SetSuper(player, 0.0f);
        g_auraOurs = false;
    }
    PollKey();
}

void Describe(char* out, int capacity) {
    const cfg::Config& c = cfg::Get();
    snprintf(out, static_cast<size_t>(capacity),
             "cheats : interrupteur general %s, acces 0x%X, ici %s ; invincible %d, munitions %d, coup fatal %d, aura %d, "
             "attaque %d %%, deplacement %d %%, saut %d %% ; personnage '%s' ; %u apparition(s)",
             Master() ? "ouvert" : "coupe", AccessMask(), Active() ? "actifs" : (Access() ? "coupes" : "sans acces"),
             c.cheatGod ? 1 : 0, c.cheatAmmo ? 1 : 0, c.cheatOneHit ? 1 : 0, c.cheatAura ? 1 : 0, c.cheatAttackSpeed,
             c.cheatMoveSpeed, c.cheatJump, CharacterOf(Local()), g_spawnsMade);
}

// -------------------------------------------------------------------------------------------------
// Menu F1
// -------------------------------------------------------------------------------------------------
void AccessTitle(int player, char* out, size_t size) {
    uint8_t id = static_cast<uint8_t>(player);
    const char* state = Allowed(id) ? "allowed" : "blocked";
    if (net::Connected(id)) loc::Format(out, size, "%s : %s", roster::Name(id), state);
    else loc::Format(out, size, "P%d (absent) : %s", player, state);
}

void AccessClick(int player) { SetAllowed(static_cast<uint8_t>(player), !Allowed(static_cast<uint8_t>(player))); }

void CharacterValue(char* out, size_t size) {
    const char* current = CharacterOf(Local());
    snprintf(out, size, "%s", current[0] ? current : "Konoko");
}

void CharacterFilterValue(char* out, size_t size) { snprintf(out, size, "%s", kFilterNames[g_characterFilter]); }

void NextCharacterFilter() {
    g_characterFilter = (g_characterFilter + 1) % kFilterCount;
    g_characterPage = 0;
}

void KeyValue(char* out, size_t size) { snprintf(out, size, "%s", kKeys[KeyIndex()]); }

void NextKey() {
    cfg::Get().characterKey = kKeys[(KeyIndex() + 1) % kKeyCount];
    cfg::SaveCheats();
}

static int Pages(int entries) { return entries > 0 ? (entries + kListRows - 1) / kListRows : 1; }

void CharacterRowTitle(int row, char* out, size_t size) {
    std::vector<const char*> list = Filtered(g_characterFilter);
    size_t index = static_cast<size_t>(g_characterPage * kListRows + row);
    if (index >= list.size()) snprintf(out, size, "-");
    else snprintf(out, size, _stricmp(list[index], CharacterOf(Local())) == 0 ? "> %s <" : "%s", list[index]);
}

void CharacterRowClick(int row) {
    std::vector<const char*> list = Filtered(g_characterFilter);
    size_t index = static_cast<size_t>(g_characterPage * kListRows + row);
    if (index < list.size()) SetCharacter(list[index]);
}

static void PageTitle(int direction, int page, int entries, char* out, size_t size) {
    if (direction < 0) snprintf(out, size, "%s", loc::Tr("Previous"));
    else loc::Format(out, size, "Next (%d/%d)", page + 1, Pages(entries));
}

static void Turn(int& page, int direction, int entries) {
    int pages = Pages(entries);
    page = (page + direction + pages) % pages;
}

void CharacterPageTitle(int direction, char* out, size_t size) {
    PageTitle(direction, g_characterPage, static_cast<int>(Filtered(g_characterFilter).size()), out, size);
}

void CharacterPageClick(int direction) {
    Turn(g_characterPage, direction, static_cast<int>(Filtered(g_characterFilter).size()));
}

static int SpawnEntries() {
    if (g_spawnKind == proto::kSpawnItem) return kItemCount;
    if (g_spawnKind == proto::kSpawnWeapon) return mem::WeaponClassCount();
    return static_cast<int>(Filtered(g_spawnFilter).size());
}

void SpawnKindValue(char* out, size_t size) {
    static const char* const kKinds[] = {"Items", "Weapons", "NPCs"};
    snprintf(out, size, "%s", kKinds[g_spawnKind]);
}

void NextSpawnKind() {
    g_spawnKind = (g_spawnKind + 1) % 3;
    g_spawnPage = 0;
}

void SpawnOptionTitle(int, char* out, size_t size) {
    if (g_spawnKind == proto::kSpawnItem) loc::Format(out, size, "Count : x%d", g_itemCount);
    else if (g_spawnKind == proto::kSpawnWeapon) loc::Format(out, size, "Ammo : %d %%", g_ammoPercent);
    else loc::Format(out, size, "Team : %s", g_spawnAlly ? "Ally" : "Enemy");
}

void SpawnOptionClick(int) {
    if (g_spawnKind == proto::kSpawnItem) g_itemCount = g_itemCount == 1 ? 3 : (g_itemCount == 3 ? 6 : 1);
    else if (g_spawnKind == proto::kSpawnWeapon) g_ammoPercent = g_ammoPercent >= 25 ? g_ammoPercent - 25 : 100;
    else g_spawnAlly = !g_spawnAlly;
}

void SpawnFilterValue(char* out, size_t size) {
    snprintf(out, size, "%s", g_spawnKind == proto::kSpawnNpc ? kFilterNames[g_spawnFilter] : "-");
}

void NextSpawnFilter() {
    if (g_spawnKind != proto::kSpawnNpc) return;
    g_spawnFilter = (g_spawnFilter + 1) % kFilterCount;
    g_spawnPage = 0;
}

void SpawnRowTitle(int row, char* out, size_t size) {
    int index = g_spawnPage * kListRows + row;
    if (index >= SpawnEntries()) snprintf(out, size, "-");
    else if (g_spawnKind == proto::kSpawnItem) snprintf(out, size, "%s", loc::Tr(kItems[index].label));
    else if (g_spawnKind == proto::kSpawnWeapon) snprintf(out, size, "%s", WeaponLabel(mem::WeaponClassAt(index)));
    else snprintf(out, size, "%s", Filtered(g_spawnFilter)[static_cast<size_t>(index)]);
}

void SpawnRowClick(int row) {
    int index = g_spawnPage * kListRows + row;
    if (index >= SpawnEntries()) return;
    if (g_spawnKind == proto::kSpawnItem) {
        const Item& item = kItems[index];
        RequestSpawn(proto::kSpawnItem, static_cast<uint8_t>(item.type), static_cast<uint16_t>(g_itemCount), "", kItemAhead,
                     loc::Tr(item.label));
    } else if (g_spawnKind == proto::kSpawnWeapon) {
        const char* className = mem::WeaponClassAt(index);
        RequestSpawn(proto::kSpawnWeapon, 0, static_cast<uint16_t>(g_ammoPercent), className, kWeaponAhead,
                     WeaponLabel(className));
    } else {
        const char* className = Filtered(g_spawnFilter)[static_cast<size_t>(index)];
        RequestSpawn(proto::kSpawnNpc, g_spawnAlly ? 1 : 0, 1, className, kNpcAhead, className);
    }
}

void SpawnPageTitle(int direction, char* out, size_t size) { PageTitle(direction, g_spawnPage, SpawnEntries(), out, size); }
void SpawnPageClick(int direction) { Turn(g_spawnPage, direction, SpawnEntries()); }

// -------------------------------------------------------------------------------------------------
// Console
// -------------------------------------------------------------------------------------------------
static const char* OnOff(bool on) { return on ? "ON" : "OFF"; }

static uint16_t ONICALL ONIMP_ENTRY BslCheats(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) SetMaster(args[0].val.value_int32 != 0);
    else mem::ConsolePrint("OniMP : cheats %s, acces 0x%X", OnOff(Master()), AccessMask());
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslAllow(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 2 && args[0].val.value_int32 >= 1 && args[0].val.value_int32 < proto::kMaxPlayers) {
        SetAllowed(static_cast<uint8_t>(args[0].val.value_int32), args[1].val.value_int32 != 0);
    } else {
        mem::ConsolePrint("OniMP : cheats %s, acces 0x%X", OnOff(Master()), AccessMask());
    }
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslGodMode(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) SetGod(args[0].val.value_int32 != 0);
    else mem::ConsolePrint("OniMP : invincibilite %s", God() ? "activee" : "desactivee");
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslUnlimitedAmmo(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) SetAmmo(args[0].val.value_int32 != 0);
    else mem::ConsolePrint("OniMP : munitions illimitees %s", Ammo() ? "activees" : "desactivees");
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslOneHit(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) SetOneHit(args[0].val.value_int32 != 0);
    else mem::ConsolePrint("OniMP : coup fatal %s", OneHit() ? "active" : "desactive");
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslAura(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) SetAura(args[0].val.value_int32 != 0);
    else mem::ConsolePrint("OniMP : aura permanente %s", Aura() ? "activee" : "desactivee");
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslSpeeds(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1) SetAttackSpeed(args[0].val.value_int32);
    if (numargs >= 2) SetMoveSpeed(args[1].val.value_int32);
    if (numargs >= 3) SetJump(args[2].val.value_int32);
    mem::ConsolePrint("OniMP : attaque %d %%, deplacement %d %%, saut %d %%", AttackSpeed(), MoveSpeed(), Jump());
    return 0;
}

static uint16_t ONICALL ONIMP_ENTRY BslCharacter(sl_callinfo*, uint32_t numargs, sl_arg* args, int*, int*, sl_arg*) {
    if (numargs >= 1 && args[0].val.value_str32) SetCharacter(_stricmp(args[0].val.value_str32, "konoko") == 0 ? "" : args[0].val.value_str32);
    else NextCharacter();
    return 0;
}

void RegisterCommands() {
    fn::RegisterCommandVoid("mp_cheats", "OniMP : interrupteur general des cheats (hote)", "[enable:int | ]", BslCheats);
    fn::RegisterCommandVoid("mp_allow", "OniMP : ouvre ou retire les onglets Cheat, Character et Spawn a un client (hote)",
                            "[player:int allow:int | ]", BslAllow);
    fn::RegisterCommandVoid("mp_godmode", "OniMP : notre joueur est invincible", "[enable:int | ]", BslGodMode);
    fn::RegisterCommandVoid("mp_unlimitedammo", "OniMP : munitions illimitees pour notre joueur", "[enable:int | ]",
                            BslUnlimitedAmmo);
    fn::RegisterCommandVoid("mp_onehit", "OniMP : nos coups tuent un ennemi d'un coup", "[enable:int | ]", BslOneHit);
    fn::RegisterCommandVoid("mp_aura", "OniMP : aura permanente pour notre joueur", "[enable:int | ]", BslAura);
    fn::RegisterCommandVoid("mp_speeds", "OniMP : vitesse d'attaque, de deplacement et force du saut, en pour cent",
                            "[attack:int | ] [move:int | ] [jump:int | ]", BslSpeeds);
    fn::RegisterCommandVoid("mp_character", "OniMP : classe a incarner (konoko pour revenir) ; sans argument, la suivante",
                            "[class:string | ]", BslCharacter);
}

}  // namespace cheat
