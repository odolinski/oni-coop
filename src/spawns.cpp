#include "spawns.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

#include "cheat.h"
#include "config.h"
#include "coop.h"
#include "levelsync.h"
#include "log.h"
#include "memory.h"
#include "network.h"
#include "protocol.h"
#include "replication.h"
#include "rules.h"

using namespace oni;

namespace spawn {

static const int32_t kStartTicks = 180;        // rien pendant les trois premières secondes du niveau
// Un ennemi doit avoir vécu ce temps-là, hors cinématique : le script qui l'a fait apparaître a eu
// le temps de le régler (équipe, passivité, figurant d'une scène).
static const double kSettle = 2.0;
static const double kFollowInterval = 0.5;     // d'un alignement des renforts sur leur original au suivant
static const double kStagger = 0.4;            // d'une création à la suivante autour du même ennemi
static const float kRing = 3.0f;               // du renfort à son original : assez pour ne pas naître l'un dans l'autre
// Le moteur n'a que 128 personnages, et les scripts du niveau doivent toujours pouvoir créer les
// leurs : au-delà de cette population, plus rien n'est ajouté.
static const int kMaxPopulation = 96;
static const size_t kMaxClones = 40;           // renforts présents
static const int kMaxCivilians = 6;            // civils présents
static const int kMaxCiviliansMade = 14;       // civils créés dans le niveau
static const double kBlameWindow = 3.0;        // du coup d'un joueur à la mort du civil

// Ce que l'on sait de chaque personnage du niveau, par numéro de case.
struct Seen {
    uint16_t handle;
    double since;          // premier passage
    double nextAt;         // pas de création autour de lui avant cet instant
    int made;              // renforts déjà créés pour lui
    bool civilianDone;     // son tirage au sort a eu lieu
    bool refused;          // le niveau n'a pas de quoi le copier
};
static Seen g_seen[off::kMaxCharacters];

struct Clone {
    Character* chr;
    uint16_t handle;
    int original;          // case de l'ennemi copié, -1 : inconnu (créé d'après l'hôte)
    uint16_t originalHandle;
    bool originalDied;
};
static std::vector<Clone> g_clones;

struct Civilian {
    Character* chr;
    uint16_t handle;
    bool hit;              // un joueur l'a blessé
    double hitAt;
    uint8_t hitBy;
};
static std::vector<Civilian> g_civilians;

static int g_civiliansMade = 0;
static unsigned g_clonesMade = 0;
static double g_calmSince = 0.0;               // fin de la dernière cinématique
static double g_now = 0.0;

static bool Present(Character* chr, uint16_t handle) { return mem::InUse(chr) && mem::Handle(chr) == handle; }

// Machine qui simule les PNJ : l'hôte, ou chacun pour soi sans PNJ partagés.
static bool Simulates() {
    cfg::Mode mode = net::CurrentMode();
    return mode == cfg::Mode::Host || (mode == cfg::Mode::Client && !cfg::Get().npcs);
}

static bool IsClone(Character* chr) {
    for (const Clone& clone : g_clones) {
        if (clone.chr == chr && Present(chr, clone.handle)) return true;
    }
    return false;
}

bool IsCivilian(Character* chr) { return rules::ParseCivilianName(mem::Name(chr), nullptr, rules::kNameSize); }

// Personnage de ce nom présent dans le niveau, et sa case ; nullptr, et la case à -1, s'il n'y est pas.
static Character* FindByName(const char* name, int& slot) {
    slot = -1;
    int count = mem::CharacterCount();
    for (int i = 0; i < count; i++) {
        Character* chr = mem::CharacterAt(i);
        if (!chr || !mem::InUse(chr) || strcmp(mem::Name(chr), name) != 0) continue;
        slot = i;
        return chr;
    }
    return nullptr;
}

// Vagues d'alarme : ennemis amenés par une alarme du niveau, trois par joueur présent.
struct WaveMember {
    Character* chr;
    uint16_t handle;
};
static std::vector<WaveMember> g_wave;
static unsigned g_waveIndex = 0;               // numéro du prochain ennemi de vague, dans son nom
static unsigned g_wavesMade = 0, g_waveEnemies = 0;
static double g_lastWaveAt = -1000.0;
static const double kWaveCooldown = 8.0;       // d'une vague à la suivante : une alarme sonne souvent plusieurs fois
static const float kWaveClear = 45.0f;         // pas d'apparition plus près que cela d'un joueur, s'il y a le choix
static const int kWaveSpots = 4;               // endroits d'où une vague arrive, au plus

void OnLevelChanged() {
    // Les personnages ont disparu avec l'ancien niveau.
    memset(g_seen, 0, sizeof(g_seen));
    g_wave.clear();
    g_waveIndex = 0;
    g_wavesMade = 0;
    g_waveEnemies = 0;
    g_lastWaveAt = -1000.0;
    g_clones.clear();
    g_civilians.clear();
    g_civiliansMade = 0;
    g_clonesMade = 0;
    g_calmSince = 0.0;
}

// -------------------------------------------------------------------------------------------------
// Qui se multiplie
// -------------------------------------------------------------------------------------------------
static bool Eligible(Character* chr, Character* player) {
    if (chr == player || mem::IsDead(chr) || !mem::IsAI(chr)) return false;
    const char* name = mem::Name(chr);
    if (!name[0] || IsClone(chr) || IsCivilian(chr)) return false;
    // Ceux que le niveau tient à part : increvables, invincibles, marqués comme boss.
    if (field<uint32_t>(chr, off::kChFlags) & (off::kChFlagUnkillable | off::kChFlagInvincible | off::kChFlagBoss)) {
        return false;
    }
    // Figurants d'une scène, civils : leur IA ne se bat pas.
    if (field<uint32_t>(chr, off::kChAiFlags) & (off::kAiFlagPassive | off::kAiFlagNonCombatant)) return false;
    // Seuls les ennemis des joueurs, par leur équipe : le pantin d'un autre joueur est de la nôtre.
    if (fn::FriendOrFoe(field<uint16_t>(chr, off::kChTeam), field<uint16_t>(player, off::kChTeam)) != 2) return false;
    if (rules::IsBossClass(mem::ClassName(chr))) return false;
    // Un script du niveau lui est attaché (à sa mort, à sa défaite, quand il donne l'alarme...) : la
    // progression du niveau passe par lui.
    return !mem::HasScripts(chr);
}

// Point à côté de location, un par rang, autour de lui.
static Vector3 Beside(const Vector3& location, int rank) {
    float angle = 2.0943951f * static_cast<float>(rank);  // un tiers de tour par rang
    return {location.x + std::cos(angle) * kRing, location.y, location.z + std::sin(angle) * kRing};
}

static bool SpawnClone(Character* original, int slot, int rank) {
    char name[rules::kNameSize];
    if (!rules::CloneName(mem::Name(original), rank, name, sizeof(name))) return false;
    void* object = mem::FindCharacterObject(mem::Name(original));
    // Pas d'objet : personnage créé par un script hors des objets du niveau. Un objet au nom du
    // renfort : le niveau a déjà un personnage qui s'appelle ainsi.
    if (!object || mem::FindCharacterObject(name)) return false;
    // L'équipe de l'original à cet instant, pas celle de l'objet du niveau : un script peut l'avoir
    // changée depuis (le garde de la fin de l'entrepôt naît dans l'équipe de Konoko, puis passe au
    // Syndicat). Sa copie restait une alliée.
    Character* clone = mem::SpawnCopy(object, name, Beside(mem::Location(original), rank),
                                      field<float>(original, off::kChFacing), mem::Team(original));
    if (!clone) return false;
    g_clones.push_back({clone, mem::Handle(clone), slot, mem::Handle(original), false});
    g_clonesMade++;
    LOG("renfort : '%s' aux cotes de '%s' (ennemis x%d)", name, mem::Name(original), coop::EnemyMultiplier());
    return true;
}

// -------------------------------------------------------------------------------------------------
// Civils
// -------------------------------------------------------------------------------------------------
// Équipe que ni les ennemis ni les joueurs ne prennent pour cible : les neutres du jeu d'origine,
// à défaut l'équipe sans ennemis.
static bool NeutralTeam(uint16_t playerTeam, uint16_t enemyTeam, uint16_t& team) {
    const uint16_t candidates[] = {3, 6};
    for (uint16_t candidate : candidates) {
        if (fn::FriendOrFoe(enemyTeam, candidate) == 2 || fn::FriendOrFoe(candidate, enemyTeam) == 2) continue;
        if (fn::FriendOrFoe(playerTeam, candidate) == 2 || fn::FriendOrFoe(candidate, playerTeam) == 2) continue;
        team = candidate;
        return true;
    }
    return false;
}

static int CiviliansPresent() {
    int present = 0;
    for (const Civilian& civilian : g_civilians) present += mem::IsDead(civilian.chr) ? 0 : 1;
    return present;
}

static Character* NewCivilian(const char* name, const char* className, const Vector3& position, float facing,
                              uint16_t team) {
    Character* chr = mem::SpawnPuppet(className, name, position, facing, team, off::kOsdFlagNonCombatant);
    if (chr) g_civilians.push_back({chr, mem::Handle(chr), false, 0.0, 0});
    return chr;
}

static bool SpawnCivilian(Character* enemy, Character* player, const char* level, uint32_t nameHash) {
    const char* className = rules::CivilianClassFor(level, nameHash);
    char name[rules::kNameSize];
    uint16_t team = 0;
    // Le numéro est pris même si la création échoue : pas de nouvel essai à chaque image.
    int index = g_civiliansMade++;
    if (!className || !rules::CivilianName(index, className, name, sizeof(name))) return false;
    if (!NeutralTeam(field<uint16_t>(player, off::kChTeam), field<uint16_t>(enemy, off::kChTeam), team)) return false;
    Character* chr = NewCivilian(name, className, Beside(mem::Location(enemy), 5), field<float>(enemy, off::kChFacing),
                                 team);
    if (!chr) return false;
    LOG("civil : '%s' pres de '%s'", name, mem::Name(enemy));
    return true;
}

void OnPlayerDamage(Character* victim, uint8_t player) {
    for (Civilian& civilian : g_civilians) {
        if (civilian.chr != victim || !Present(victim, civilian.handle)) continue;
        civilian.hit = true;
        civilian.hitAt = g_now;
        civilian.hitBy = player;
        return;
    }
}

// Un civil mort sous les coups d'un joueur : la mission échoue. Mort autrement (balle perdue d'un
// ennemi, explosion du décor), il est seulement oublié.
static void JudgeCivilians(double now) {
    for (size_t i = 0; i < g_civilians.size();) {
        const Civilian& civilian = g_civilians[i];
        if (!mem::IsDead(civilian.chr)) {
            i++;
            continue;
        }
        bool blamed = civilian.hit && now - civilian.hitAt <= kBlameWindow;
        LOG("civil : '%s' est mort%s", mem::Name(civilian.chr), blamed ? ", tue par un joueur" : "");
        uint8_t by = civilian.hitBy;
        g_civilians.erase(g_civilians.begin() + static_cast<long>(i));
        if (blamed) level::Fail(proto::kRestartCivilian, by, now);
    }
}

// -------------------------------------------------------------------------------------------------
// Chaque frame
// -------------------------------------------------------------------------------------------------
// Oublie ceux que le moteur a retirés. Un renfort dont l'original a été retiré vivant par le niveau
// (fin d'une scène, chr_delete) s'en va avec lui : il n'a plus rien à faire là.
static void Prune(bool simulates) {
    for (size_t i = 0; i < g_clones.size();) {
        Clone& clone = g_clones[i];
        bool keep = Present(clone.chr, clone.handle);
        if (keep && clone.original >= 0 && !clone.originalDied) {
            Character* original = mem::CharacterAt(clone.original);
            if (original && Present(original, clone.originalHandle)) {
                clone.originalDied = mem::IsDead(original);
            } else if (simulates && !mem::IsDead(clone.chr)) {
                LOG("renfort : '%s' retire, le niveau a retire son original", mem::Name(clone.chr));
                mem::DeleteCharacter(clone.chr);
                keep = false;
            } else {
                clone.originalDied = true;  // plus rien à suivre
            }
        }
        if (keep) i++;
        else g_clones.erase(g_clones.begin() + static_cast<long>(i));
    }
    for (size_t i = 0; i < g_civilians.size();) {
        if (Present(g_civilians[i].chr, g_civilians[i].handle)) i++;
        else g_civilians.erase(g_civilians.begin() + static_cast<long>(i));
    }
}

// Un renfort naît de l'objet du niveau, tel que le niveau l'a écrit : il n'a rien de ce que les
// scripts ont fait de son original depuis, ni de ce que celui-ci a appris. Il patrouillait de son
// côté pendant que l'original se battait, restait un allié quand l'original avait changé de camp,
// attaquait pendant une scène où l'original était tenu passif. Deux fois par seconde, chaque
// renfort est donc aligné sur son original tant que celui-ci est là : même équipe, même passivité,
// même consigne d'ignorer le joueur, et la connaissance des joueurs que l'original tient pour des
// menaces.
static void Follow(bool simulates, double now) {
    static double next = 0.0;
    if (now < next) return;
    next = now + kFollowInterval;
    const uint32_t mirrored = off::kAiFlagPassive | off::kAiFlagIgnorePlayer;
    for (Clone& clone : g_clones) {
        if (clone.originalDied || mem::IsDead(clone.chr)) continue;
        if (clone.original < 0) {
            // Créé d'après l'hôte avant que son original soit ici : on le cherche par son nom.
            char parsed[rules::kNameSize];
            int slot = -1;
            if (!rules::ParseCloneName(mem::Name(clone.chr), parsed, sizeof(parsed), nullptr)) continue;
            Character* found = FindByName(parsed, slot);
            if (!found) continue;
            clone.original = slot;
            clone.originalHandle = mem::Handle(found);
        }
        Character* original = mem::CharacterAt(clone.original);
        if (!original || !Present(original, clone.originalHandle) || mem::IsDead(original)) continue;
        if (mem::Team(clone.chr) != mem::Team(original)) {
            LOG("renfort : '%s' passe dans l'equipe %u, comme '%s'", mem::Name(clone.chr),
                static_cast<unsigned>(mem::Team(original)), mem::Name(original));
            mem::SetTeam(clone.chr, mem::Team(original));
        }
        if (!simulates) continue;  // chez un client les PNJ n'ont pas d'IA en marche
        uint32_t wanted = field<uint32_t>(original, off::kChAiFlags) & mirrored;
        uint32_t& flags = field<uint32_t>(clone.chr, off::kChAiFlags);
        if ((flags & mirrored) != wanted) {
            if ((flags ^ wanted) & off::kAiFlagPassive) {
                mem::Exec("ai2_passive %s %d", mem::Name(clone.chr), wanted & off::kAiFlagPassive ? 1 : 0);
            }
            flags = (flags & ~mirrored) | wanted;
        }
        if (wanted & off::kAiFlagPassive) continue;
        Character* threats[8];
        int count = mem::Threats(original, threats, 8);
        for (int i = 0; i < count; i++) {
            if (!rep::IsPlayerCharacter(threats[i]) || mem::IsDead(threats[i]) || mem::Knows(clone.chr, threats[i])) continue;
            fn::AiMakeAware(clone.chr, threats[i], 0, 0);
        }
    }
}

void Update(double now) {
    g_now = now;
    Character* player = mem::Player();
    if (!player) return;
    bool simulates = Simulates();
    Prune(simulates);
    Follow(simulates, now);
    if (!simulates) return;
    JudgeCivilians(now);

    // Pendant une cinématique les touches du joueur sont coupées : rien n'apparaît alors, ni juste
    // après, le temps que le niveau retire ses figurants.
    if (!mem::InputEnabled()) g_calmSince = now;
    bool calm = mem::GameTime() >= kStartTicks && now - g_calmSince >= kSettle && !mem::IsDead(player);

    int multiplier = coop::EnemyMultiplier();
    const char* level = mem::LevelName();
    bool civilians = coop::Civilians() && rules::CivilianClasses(level, nullptr) != nullptr;
    int count = mem::CharacterCount(), population = 0;
    for (int i = 0; i < count; i++) {
        Character* chr = mem::CharacterAt(i);
        if (chr && mem::InUse(chr)) population++;
    }

    bool created = false;  // une création par image au plus
    for (int i = 0; i < count; i++) {
        Character* chr = mem::CharacterAt(i);
        Seen& seen = g_seen[i];
        if (!chr || !mem::InUse(chr)) {
            seen = {};
            continue;
        }
        if (seen.handle != mem::Handle(chr)) {
            seen = {};
            seen.handle = mem::Handle(chr);
            seen.since = now;
        }
        if (created || !calm || seen.refused || now - seen.since < kSettle || now < seen.nextAt) continue;
        bool wantsClone = seen.made < multiplier - 1;
        bool wantsCivilian = civilians && !seen.civilianDone;
        if ((!wantsClone && !wantsCivilian) || population >= kMaxPopulation || !Eligible(chr, player)) continue;

        if (wantsCivilian) {
            seen.civilianDone = true;
            uint32_t nameHash = mem::Fnv1a(mem::Name(chr), rules::kNameSize);
            if (rules::BringsCivilian(mem::LevelHash(), nameHash) && CiviliansPresent() < kMaxCivilians &&
                g_civiliansMade < kMaxCiviliansMade) {
                created = SpawnCivilian(chr, player, level, nameHash);
                seen.nextAt = now + kStagger;
                continue;
            }
        }
        if (!wantsClone || g_clones.size() >= kMaxClones) continue;
        if (SpawnClone(chr, i, seen.made + 2)) {
            seen.made++;
            seen.nextAt = now + kStagger;
            created = true;
        } else {
            seen.refused = true;
        }
    }
}

// -------------------------------------------------------------------------------------------------
// Vagues d'alarme
// -------------------------------------------------------------------------------------------------
// Les ennemis d'une vague sont ceux du niveau : des copies de ses objets CHAR, comme les renforts,
// donc avec la classe, l'arme et les réglages de combat que ses concepteurs leur ont donnés. Ils
// naissent là où le niveau place leur original, c'est-à-dire à un endroit d'où l'IA sait partir,
// et reçoivent la place du joueur : c'est le calcul de chemin du moteur qui les y mène.
struct WaveSpot {
    void* object;
    char name[rules::kNameSize];
    Vector3 position;
    float distance;      // au personnage qui a causé l'alarme
    bool clear;          // aucun joueur tout près
};
struct WaveScan {
    WaveSpot spots[64];
    int count;
    uint16_t playerTeam;
    Vector3 cause;
};

static float Distance(const Vector3& a, const Vector3& b) {
    float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Aucun joueur, le nôtre ou le pantin d'un autre, à moins de kWaveClear de ce point.
static bool ClearOfPlayers(const Vector3& at) {
    Character* player = mem::Player();
    if (player && Distance(at, mem::Location(player)) < kWaveClear) return false;
    for (uint32_t id = 0; id < static_cast<uint32_t>(proto::kMaxPlayers); id++) {
        Character* other = rep::RemotePlayerCharacter(id);
        if (other && Distance(at, mem::Location(other)) < kWaveClear) return false;
    }
    return true;
}

// Un ennemi ordinaire du niveau : ni joueur, ni civil, ni boss, ni increvable, sans script attaché,
// d'une équipe en guerre avec celle des joueurs, et au nom assez court pour porter le suffixe.
static uint8_t ONICALL ONIMP_ENTRY CollectWaveSpot(void* object, void* userData) {
    WaveScan* scan = static_cast<WaveScan*>(userData);
    uint8_t* osd = static_cast<uint8_t*>(object) + off::kObjOsd;
    uint32_t flags = field<uint32_t>(osd, off::kOsdFlags);
    if (flags & (off::kOsdFlagPlayer | off::kOsdFlagNonCombatant | off::kOsdFlagUnkillable | off::kOsdFlagBoss)) return 1;
    const char* name = &field<char>(osd, off::kOsdName);
    char wave[rules::kNameSize];
    if (!rules::WaveName(name, 0, wave, sizeof(wave)) || rules::ParseWaveName(name, nullptr, 0)) return 1;
    for (int i = 0; i < off::kChScriptCount; i++) {
        if (field<char>(osd, off::kOsdScripts + static_cast<size_t>(i) * off::kChScriptSize)) return 1;
    }
    if (fn::FriendOrFoe(field<uint16_t>(osd, off::kOsdTeam), scan->playerTeam) != 2) return 1;
    if (rules::IsBossClass(&field<char>(osd, off::kOsdClass))) return 1;
    Vector3 position = field<Vector3>(object, off::kObjPosition);
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) return 1;
    if (scan->count >= 64) return 0;
    WaveSpot& spot = scan->spots[scan->count++];
    spot.object = object;
    strncpy(spot.name, name, sizeof(spot.name) - 1);
    spot.name[sizeof(spot.name) - 1] = '\0';
    spot.position = position;
    spot.distance = Distance(position, scan->cause);
    spot.clear = ClearOfPlayers(position);
    return 1;
}

static int WaveAlive() {
    int alive = 0;
    for (size_t i = 0; i < g_wave.size();) {
        if (!Present(g_wave[i].chr, g_wave[i].handle)) {
            g_wave.erase(g_wave.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        if (!mem::IsDead(g_wave[i].chr)) alive++;
        i++;
    }
    return alive;
}

int Wave(Character* cause, const char* why) {
    Character* player = mem::Player();
    if (!player || !Simulates() || !net::Running() || mem::InCutscene()) return 0;
    if (g_now - g_lastWaveAt < kWaveCooldown) return 0;
    // Trois par joueur présent, moins ceux d'une vague précédente qui sont encore là : jamais plus
    // de trois ennemis de vague par joueur à la fois.
    int players = coop::Players(), alive = WaveAlive();
    int wanted = rules::WaveSize(players) - alive;
    int population = 0, count = mem::CharacterCount();
    for (int i = 0; i < count; i++) {
        Character* chr = mem::CharacterAt(i);
        if (chr && mem::InUse(chr)) population++;
    }
    if (wanted > kMaxPopulation - population) wanted = kMaxPopulation - population;
    if (wanted <= 0) {
        LOG("vague : alarme (%s) sans renfort : %d ennemi(s) de vague encore la pour %d joueur(s), %d personnage(s) "
            "dans le niveau", why, alive, players, population);
        return 0;
    }
    // Celui que la vague vient chercher : le joueur qui a causé l'alarme, ou à défaut le plus proche
    // de celui qui l'a donnée.
    Character* target = cause && rep::IsPlayerCharacter(cause) ? cause : player;
    Vector3 around = mem::Location(cause ? cause : player);
    if (target == player && cause && cause != player) {
        float best = Distance(around, mem::Location(player));
        for (uint32_t id = 0; id < static_cast<uint32_t>(proto::kMaxPlayers); id++) {
            Character* other = rep::RemotePlayerCharacter(id);
            if (!other || Distance(around, mem::Location(other)) >= best) continue;
            best = Distance(around, mem::Location(other));
            target = other;
        }
    }

    static WaveScan scan;
    scan.count = 0;
    scan.playerTeam = mem::Team(player);
    scan.cause = around;
    fn::EnumerateObjects(Tag('C', 'H', 'A', 'R'), CollectWaveSpot, &scan);
    if (scan.count == 0) {
        LOG("vague : alarme (%s), mais ce niveau n'a aucun ennemi ordinaire a copier", why);
        return 0;
    }
    // Les plus proches de l'alarme d'abord, ceux qui ne sont pas sous le nez d'un joueur avant les
    // autres ; la vague arrive de quelques endroits, pas de tout le niveau.
    std::sort(scan.spots, scan.spots + scan.count, [](const WaveSpot& a, const WaveSpot& b) {
        return a.clear != b.clear ? a.clear : a.distance < b.distance;
    });
    int spots = scan.count < kWaveSpots ? scan.count : kWaveSpots;

    int made = 0;
    for (int i = 0; i < wanted; i++) {
        const WaveSpot& spot = scan.spots[i % spots];
        char name[rules::kNameSize];
        int slot = -1;
        // Un numéro que personne ne porte déjà dans ce niveau.
        bool named = false;
        for (int attempt = 0; attempt < 100 && !named; attempt++) {
            named = rules::WaveName(spot.name, static_cast<int>(g_waveIndex++ % 100), name, sizeof(name)) &&
                    !FindByName(name, slot);
        }
        if (!named) continue;
        float facing = field<float>(spot.object, off::kObjOsd + off::kOsdFacing);
        Character* enemy = mem::SpawnCopy(spot.object, name, Beside(spot.position, i / spots), std::isfinite(facing) ? facing : 0.0f);
        if (!enemy) continue;
        fn::AiMakeAware(enemy, target, 0, 0);
        g_wave.push_back({enemy, mem::Handle(enemy)});
        made++;
    }
    if (!made) return 0;
    g_lastWaveAt = g_now;
    g_wavesMade++;
    g_waveEnemies += static_cast<unsigned>(made);
    LOG("vague : alarme (%s) : %d ennemi(s) en renfort pour %d joueur(s), depuis %d endroit(s) du niveau, vers '%s'", why,
        made, players, spots, mem::Name(target));
    mem::ConsolePrint("OniMP : alarme : %d ennemi(s) en renfort (%d joueur(s))", made, players);
    return made;
}

bool Create(const char* name, const Vector3& position, float facing) {
    Character* player = mem::Player();
    char parsed[rules::kNameSize];
    int rank = 0;
    if (!player || !name) return false;
    // PNJ qu'un joueur a fait apparaître chez l'hôte (onglet Spawn) : son nom dit sa classe.
    if (cheat::CreateSpawned(name, position, facing)) return true;
    if (rules::ParseCivilianName(name, parsed, sizeof(parsed))) {
        // Chez l'hôte il est d'une équipe neutre ; ici son IA est à l'arrêt, seule compte qu'il ne
        // soit l'ennemi de personne.
        uint16_t own = field<uint16_t>(player, off::kChTeam), team = 3;
        if (!NeutralTeam(own, own, team)) team = own;
        if (NewCivilian(name, parsed, position, facing, team)) LOG("civil : '%s' cree d'apres l'hote", name);
        return true;
    }
    if (rules::ParseWaveName(name, parsed, sizeof(parsed))) {
        // Ennemi d'une vague d'alarme de l'hôte : la même copie du même ennemi du niveau.
        void* original = mem::FindCharacterObject(parsed);
        if (!original || mem::FindCharacterObject(name)) return false;
        if (mem::SpawnCopy(original, name, position, facing)) LOG("vague : '%s' cree d'apres l'hote", name);
        return true;
    }
    if (!rules::ParseCloneName(name, parsed, sizeof(parsed), &rank)) return false;
    void* object = mem::FindCharacterObject(parsed);
    if (!object || mem::FindCharacterObject(name)) return false;  // un vrai personnage du niveau porte ce nom
    // L'original s'il est ici : sa copie le suivra (équipe), comme chez l'hôte.
    int slot = -1;
    Character* original = FindByName(parsed, slot);
    Character* clone = mem::SpawnCopy(object, name, position, facing, original ? mem::Team(original) : -1);
    if (clone) {
        g_clones.push_back({clone, mem::Handle(clone), slot,
                            static_cast<uint16_t>(original ? mem::Handle(original) : 0), false});
        g_clonesMade++;
        LOG("renfort : '%s' cree d'apres l'hote", name);
    }
    return true;
}

void Describe(char* out, int capacity) {
    snprintf(out, static_cast<size_t>(capacity),
             "ennemis x%d : %u renfort(s) cree(s), %u present(s) ; civils %s : %d present(s) ; alarmes : %u vague(s), %u "
             "ennemi(s), %d en vie",
             coop::EnemyMultiplier(), g_clonesMade, static_cast<unsigned>(g_clones.size()),
             coop::Civilians() ? "on" : "off", CiviliansPresent(), g_wavesMade, g_waveEnemies, WaveAlive());
}

}  // namespace spawn
