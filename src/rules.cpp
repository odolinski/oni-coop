#include "rules.h"

#include <cstdio>
#include <cstring>

namespace rules {

// -------------------------------------------------------------------------------------------------
// Modes de difficulté
// -------------------------------------------------------------------------------------------------
// Survie part du mode difficile du jeu d'origine et y ajoute moitié plus de vie et de dégâts, et
// des ennemis qui se soignent.
static const ModeRules kModes[kModeCount] = {
    {"Easy", "easy", 35.0, 45, 0, 100, 100, 0, 0},
    {"Normal", "normal", 25.0, 40, 1, 100, 100, 0, 0},
    {"Hard", "hard", 10.0, 35, 2, 100, 100, 0, 0},
    {"Survival", "survival", 5.0, 25, 2, 150, 150, 1, 2},
};

const ModeRules& Of(uint8_t mode) { return kModes[mode < kModeCount ? mode : static_cast<uint8_t>(kNormal)]; }

uint8_t ModeFromKey(const char* key, uint8_t fallback) {
    for (uint8_t mode = 0; mode < kModeCount; mode++) {
        if (key && strcmp(key, kModes[mode].key) == 0) return mode;
    }
    return fallback;
}

uint8_t NextMode(uint8_t mode) { return static_cast<uint8_t>(mode + 1 < kModeCount ? mode + 1 : kEasy); }

uint32_t RevivedHealth(uint32_t maxHealth, uint32_t percent) {
    uint64_t health = static_cast<uint64_t>(maxHealth) * percent / 100;
    if (health < 2) return 2;
    return health > 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32_t>(health);
}

void Bleed::Start(double seconds) {
    total_ = seconds > 0.0 ? seconds : 0.0;
    remaining_ = total_;
}

bool Bleed::Tick(double elapsed, bool paused) {
    if (!paused && elapsed > 0.0) remaining_ -= elapsed;
    if (remaining_ > 0.0) return false;
    remaining_ = 0.0;
    return true;
}

int16_t BleedToWire(double seconds) {
    if (seconds <= 0.0) return 0;
    double tenths = seconds * 10.0 + 0.999;  // arrondi vers le haut : zéro veut dire épuisé
    return tenths > 32767.0 ? 32767 : static_cast<int16_t>(tenths);
}

double BleedFromWire(int16_t tenths) { return tenths > 0 ? tenths / 10.0 : 0.0; }

// -------------------------------------------------------------------------------------------------
// Multiplicateur d'ennemis
// -------------------------------------------------------------------------------------------------
static int Clamp(int value, int low, int high) { return value < low ? low : (value > high ? high : value); }

int MinEnemies(int players) { return Clamp(players, kMinEnemies, kMaxEnemies); }

int EffectiveEnemies(int chosen, int players) {
    int minimum = MinEnemies(players);
    chosen = Clamp(chosen, kMinEnemies, kMaxEnemies);
    return chosen < minimum ? minimum : chosen;
}

int NextEnemies(int chosen, int players) {
    int next = EffectiveEnemies(chosen, players) + 1;
    return next > kMaxEnemies ? MinEnemies(players) : next;
}

// -------------------------------------------------------------------------------------------------
// Personnages ajoutés par le mod
// -------------------------------------------------------------------------------------------------
bool CloneName(const char* original, int rank, char* out, size_t size) {
    if (!original || !*original || rank < 2 || rank > kMaxEnemies) return false;
    size_t length = strlen(original);
    if (length + 3 >= size || length + 3 >= kNameSize) return false;
    snprintf(out, size, "%s_x%d", original, rank);
    return true;
}

bool ParseCloneName(const char* name, char* original, size_t size, int* rank) {
    size_t length = name ? strlen(name) : 0;
    if (length < 4 || length >= kNameSize) return false;
    const char* suffix = name + length - 3;
    if (suffix[0] != '_' || suffix[1] != 'x' || suffix[2] < '2' || suffix[2] > '0' + kMaxEnemies) return false;
    if (length - 3 >= size) return false;
    memcpy(original, name, length - 3);
    original[length - 3] = '\0';
    if (rank) *rank = suffix[2] - '0';
    return true;
}

bool WaveName(const char* original, int index, char* out, size_t size) {
    if (!original || !*original || index < 0 || index > 99) return false;
    size_t length = strlen(original);
    if (length + 4 >= size || length + 4 >= kNameSize) return false;
    snprintf(out, size, "%s_w%02d", original, index);
    return true;
}

bool ParseWaveName(const char* name, char* original, size_t size) {
    size_t length = name ? strlen(name) : 0;
    if (length < 5 || length >= kNameSize) return false;
    const char* suffix = name + length - 4;
    if (suffix[0] != '_' || suffix[1] != 'w' || suffix[2] < '0' || suffix[2] > '9' || suffix[3] < '0' || suffix[3] > '9') {
        return false;
    }
    if (original) {
        if (length - 4 >= size) return false;
        memcpy(original, name, length - 4);
        original[length - 4] = '\0';
    }
    return true;
}

int WaveSize(int players) {
    if (players < 1) players = 1;
    if (players > 4) players = 4;
    return kWavePerPlayer * players;
}

float RayGap(float a, float b, int railFrames, bool loops) {
    float gap = a > b ? a - b : b - a;
    float rail = static_cast<float>(railFrames);
    if (loops && railFrames > 0 && gap > rail * 0.5f && gap <= rail) gap = rail - gap;
    return gap;
}

float RayTolerance(float speed) {
    if (speed < 0.0f) speed = -speed;
    return 6.0f * speed + 1.5f;
}

void AdvanceRay(float* frame, int* direction, float speed, int ticks, int railFrames, bool loops) {
    float rail = static_cast<float>(railFrames);
    if (railFrames < 2 || !(*frame >= 0.0f && *frame <= rail)) return;
    for (int i = 0; i < ticks; i++) {
        *frame += static_cast<float>(*direction) * speed;
        if (*frame >= rail) {
            *frame = loops ? 0.0f : rail - 1.0f;
            if (!loops) *direction = -1;
        }
        if (*frame < 0.0f) {
            *frame = loops ? rail - 1.0f : 0.0f;
            if (!loops) *direction = 1;
        }
    }
}

int ChooseTurretTarget(const TurretCandidate* list, int count, int current, float keepRatio, TurretChoice* why) {
    static const TurretChoice kWhy[3] = {kTurretTripper, kTurretReachable, kTurretNearest};
    int best[3] = {-1, -1, -1};
    for (int i = 0; i < count; i++) {
        const bool in[3] = {list[i].reachable && list[i].tripped, list[i].reachable, true};
        for (int rank = 0; rank < 3; rank++) {
            if (in[rank] && (best[rank] < 0 || list[i].distance < list[best[rank]].distance)) best[rank] = i;
        }
    }
    for (int rank = 0; rank < 3; rank++) {
        if (best[rank] < 0) continue;
        if (why) *why = kWhy[rank];
        bool keep = current >= 0 && current < count && (rank == 2 || list[current].reachable) &&
                    (rank != 0 || list[current].tripped) && list[current].distance <= list[best[rank]].distance * keepRatio;
        return keep ? current : best[rank];
    }
    if (why) *why = kTurretNobody;
    return -1;
}

static const char kCivilianPrefix[] = "mpc";

bool CivilianName(int index, const char* className, char* out, size_t size) {
    if (index < 0 || index > 99 || !className || !*className) return false;
    if (strlen(className) + 6 >= kNameSize || strlen(className) + 6 >= size) return false;
    snprintf(out, size, "%s%02d_%s", kCivilianPrefix, index, className);
    return true;
}

bool ParseCivilianName(const char* name, char* className, size_t size) {
    if (!name || strncmp(name, kCivilianPrefix, 3) != 0) return false;
    if (name[3] < '0' || name[3] > '9' || name[4] < '0' || name[4] > '9' || name[5] != '_' || !name[6]) return false;
    const char* found = name + 6;
    // Le nom finit dans une recherche de classe : lettres, chiffres et tirets bas seulement.
    for (const char* c = found; *c; c++) {
        bool valid = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_';
        if (!valid) return false;
    }
    if (strlen(found) >= size) return false;
    if (className) strcpy(className, found);
    return true;
}

bool IsBossClass(const char* className) {
    static const char* const kBosses[] = {"barabus", "muro_generic", "mutantmuro", "super_ninja_1"};
    if (!className) return false;
    for (const char* boss : kBosses) {
        if (strcmp(className, boss) == 0) return true;
    }
    return false;
}

// Passants et employés de bureau, puis techniciens et chercheurs : ceux que le jeu d'origine place
// lui-même dans ces niveaux.
static const char* const kCitizens[] = {
    "generic_male_1",   "generic_male_2",   "generic_male_3",   "generic_male_4",   "generic_male_5",
    "generic_male_6",   "generic_female_1", "generic_female_2", "generic_female_3", "generic_female_4",
    "generic_female_5", "generic_female_6", "generic_female_7", "generic_female_8", "generic_female_9",
};
static const char* const kStaff[] = {
    "bd_1", "bd_2", "bd_3", "bd_4", "bd_5", "bdfem_1", "bdfem_2", "bdfem_3", "bdfem_4",
};

struct CivilianLevel {
    const char* level;
    const char* const* classes;
    int count;
};
#define ONIMP_CLASSES(list) list, static_cast<int>(sizeof(list) / sizeof(list[0]))
static const CivilianLevel kCivilianLevels[] = {
    {"Airport", ONIMP_CLASSES(kCitizens)},      // aérogare
    {"Airport_III", ONIMP_CLASSES(kCitizens)},  // hangars et pistes
    {"tctf", ONIMP_CLASSES(kCitizens)},         // quartier général du TCTF : bureaux
    {"state", ONIMP_CLASSES(kCitizens)},        // bâtiment administratif
    {"manplant", ONIMP_CLASSES(kStaff)},        // usine
    {"lab", ONIMP_CLASSES(kStaff)},             // laboratoire
    {"neuro", ONIMP_CLASSES(kStaff)},           // centre de recherche
    {"power_II", ONIMP_CLASSES(kStaff)},        // centrale : salles de contrôle
    {"tctf_II", ONIMP_CLASSES(kStaff)},         // quartier général du TCTF, sous-sols scientifiques
};
#undef ONIMP_CLASSES

// Le nom d'un niveau, sans tenir compte de la casse : « Airport » dans les données du jeu.
static bool SameLevel(const char* a, const char* b) {
    for (;; a++, b++) {
        char x = *a >= 'A' && *a <= 'Z' ? static_cast<char>(*a + 32) : *a;
        char y = *b >= 'A' && *b <= 'Z' ? static_cast<char>(*b + 32) : *b;
        if (x != y) return false;
        if (!x) return true;
    }
}

const char* const* CivilianClasses(const char* levelName, int* count) {
    if (count) *count = 0;
    if (!levelName) return nullptr;
    for (const CivilianLevel& level : kCivilianLevels) {
        if (!SameLevel(levelName, level.level)) continue;
        if (count) *count = level.count;
        return level.classes;
    }
    return nullptr;
}

static uint32_t Mix(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;
    return value;
}

bool BringsCivilian(uint32_t levelHash, uint32_t nameHash) { return Mix(levelHash ^ Mix(nameHash)) % 6 == 0; }

const char* CivilianClassFor(const char* levelName, uint32_t nameHash) {
    int count = 0;
    const char* const* classes = CivilianClasses(levelName, &count);
    return classes && count > 0 ? classes[Mix(nameHash + 0x9E3779B9u) % static_cast<uint32_t>(count)] : nullptr;
}

}  // namespace rules
