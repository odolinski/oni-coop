#include "memory.h"

#include <windows.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <unordered_map>

#include "clock.h"
#include "lang.h"
#include "log.h"

using namespace oni;

namespace mem {

// -------------------------------------------------------------------------------------------------
// Patch
// -------------------------------------------------------------------------------------------------
bool ExeSupported() {
    MEMORY_BASIC_INFORMATION info;
    if (!VirtualQuery(reinterpret_cast<void*>(addr::kExeCheck), &info, sizeof(info))) return false;
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    return *reinterpret_cast<uint32_t*>(addr::kExeCheck) == addr::kExeCheckValue;
}

void* CallTarget(uintptr_t site) {
    auto* code = reinterpret_cast<uint8_t*>(site);
    if (code[0] != 0xE8) return nullptr;
    int32_t rel;
    memcpy(&rel, code + 1, sizeof(rel));
    return reinterpret_cast<void*>(site + 5 + rel);
}

bool PatchCall(uintptr_t site, void* target, void** previous) {
    void* old = CallTarget(site);
    if (!old) return false;

    auto* code = reinterpret_cast<uint8_t*>(site);
    DWORD protection;
    if (!VirtualProtect(code, 5, PAGE_EXECUTE_READWRITE, &protection)) return false;
    int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(target) - (site + 5));
    memcpy(code + 1, &rel, sizeof(rel));
    VirtualProtect(code, 5, protection, &protection);
    FlushInstructionCache(GetCurrentProcess(), code, 5);

    if (previous) *previous = old;
    return true;
}

bool HookEntry(uintptr_t address, const uint8_t* expected, size_t length, void* hook, void** original) {
    auto* code = reinterpret_cast<uint8_t*>(address);
    if (length < 5 || memcmp(code, expected, length) != 0) return false;

    // Tremplin : les instructions écrasées, puis un saut vers la suite de la fonction.
    auto* trampoline = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, length + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!trampoline) return false;
    memcpy(trampoline, code, length);
    trampoline[length] = 0xE9;
    int32_t back = static_cast<int32_t>((address + length) - (reinterpret_cast<uintptr_t>(trampoline) + length + 5));
    memcpy(trampoline + length + 1, &back, sizeof(back));

    DWORD protection;
    if (!VirtualProtect(code, length, PAGE_EXECUTE_READWRITE, &protection)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return false;
    }
    *original = trampoline;  // avant le saut : le détour peut servir dès qu'il est posé
    int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(hook) - (address + 5));
    code[0] = 0xE9;
    memcpy(code + 1, &rel, sizeof(rel));
    memset(code + 5, 0x90, length - 5);
    VirtualProtect(code, length, protection, &protection);
    FlushInstructionCache(GetCurrentProcess(), code, length);
    return true;
}

bool PatchPointer(uintptr_t site, void* expected, void* replacement) {
    auto* operand = reinterpret_cast<uint8_t*>(site);
    void* current;
    memcpy(&current, operand, sizeof(current));
    if (current != expected) return false;
    DWORD protection;
    if (!VirtualProtect(operand, sizeof(current), PAGE_EXECUTE_READWRITE, &protection)) return false;
    memcpy(operand, &replacement, sizeof(replacement));
    VirtualProtect(operand, sizeof(current), protection, &protection);
    FlushInstructionCache(GetCurrentProcess(), operand, sizeof(current));
    return true;
}

int PatchImports(void* module, void* original, void* replacement) {
    if (!module || !original) return 0;
    auto* base = static_cast<uint8_t*>(module);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!directory.VirtualAddress) return 0;

    // Les imports sont déjà résolus : on cherche l'adresse réelle plutôt que le nom.
    int patched = 0;
    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress);
    for (; descriptor->Name; descriptor++) {
        auto* thunk = reinterpret_cast<void**>(base + descriptor->FirstThunk);
        for (; *thunk; thunk++) {
            if (*thunk != original) continue;
            DWORD protection;
            if (!VirtualProtect(thunk, sizeof(*thunk), PAGE_READWRITE, &protection)) continue;
            *thunk = replacement;
            VirtualProtect(thunk, sizeof(*thunk), protection, &protection);
            patched++;
        }
    }
    return patched;
}

// -------------------------------------------------------------------------------------------------
// État global
// -------------------------------------------------------------------------------------------------
GameState* Game() { return *reinterpret_cast<GameState**>(addr::kGameStatePtr); }

Character* FocusedCharacter() {
    GameState* gs = Game();
    return gs ? field<Character*>(gs, off::kGsPlayerCharacter) : nullptr;
}

Character* Player() {
    Character* focused = FocusedCharacter();
    if (!focused) return nullptr;
    // Le joueur du niveau est le premier personnage créé : c'est lui que les scripts appellent 0,
    // et à lui que chr_focus 0 revient. Quand le moteur en suit un autre qui n'est pas un joueur,
    // le nôtre reste celui-là.
    Character* first = reinterpret_cast<Character*>(reinterpret_cast<uint8_t*>(Game()) + off::kGsCharacterStorage);
    if (focused != first && (field<uint32_t>(first, off::kChFlags) & off::kChFlagInUse) &&
        field<int32_t>(first, off::kChType) == off::kChTypePlayer &&
        field<int32_t>(focused, off::kChType) != off::kChTypePlayer) {
        return first;
    }
    return focused;
}

int32_t GameTime() {
    GameState* gs = Game();
    return gs ? field<int32_t>(gs, off::kGsGameTime) : 0;
}

bool Paused() {
    GameState* gs = Game();
    return gs && field<char>(gs, off::kGsIsGamePaused) != 0;
}

void SetPaused(bool paused) {
    GameState* gs = Game();
    if (!gs) return;
    // La boucle du jeu saute l'horloge et la simulation tant que ce drapeau est posé (0x4D4692). À
    // la relance, l'horloge repart de l'instant présent : sans cela tout le temps de l'arrêt serait
    // simulé d'un trait.
    if (!paused && field<char>(gs, off::kGsIsGamePaused)) field<uint32_t>(gs, off::kGsLastMachineTime) = fn::MachineTime();
    field<char>(gs, off::kGsIsGamePaused) = paused ? 1 : 0;
}

bool DropSplash() {
    GameState* gs = Game();
    if (!gs || !field<char>(gs, off::kGsSplashName)) return false;
    field<char>(gs, off::kGsSplashName) = '\0';
    return true;
}

bool HasLsi(Character* chr) { return chr && field<uint8_t>(chr, off::kChHasLsi) != 0; }
void GiveLsi(Character* chr) {
    if (chr) field<uint8_t>(chr, off::kChHasLsi) = 1;
}

uint16_t Team(Character* chr) { return field<uint16_t>(chr, off::kChTeam); }
void SetTeam(Character* chr, uint16_t team) { field<uint16_t>(chr, off::kChTeam) = team; }

// La liste des contacts d'une IA, bornée : une liste abîmée ne doit pas nous faire tourner sans fin.
template <typename Visit>
static void EachContact(Character* ai, Visit visit) {
    void* contact = field<void*>(ai, off::kChContacts);
    for (int guard = 0; contact && guard < 64; guard++) {
        visit(contact);
        contact = field<void*>(contact, off::kCtNext);
    }
}

bool Knows(Character* ai, Character* other) {
    bool known = false;
    EachContact(ai, [&](void* contact) {
        known = known || (field<Character*>(contact, off::kCtEntity) == other &&
                          field<int32_t>(contact, off::kCtStrength) >= off::kCtStrengthStrong);
    });
    return known;
}

int Threats(Character* ai, Character** out, int capacity) {
    int count = 0;
    EachContact(ai, [&](void* contact) {
        Character* entity = field<Character*>(contact, off::kCtEntity);
        if (!entity || count >= capacity || field<int32_t>(contact, off::kCtStrength) < off::kCtStrengthStrong) return;
        if (field<int32_t>(contact, off::kCtPriority) >= 2) out[count++] = entity;
    });
    return count;
}

const char* LevelName() {
    GameState* gs = Game();
    if (!gs) return "";
    const char* level = field<const char*>(gs, off::kGsLevel);
    return level ? level : "";
}

uint32_t Fnv1a(const char* text, size_t maxLength) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < maxLength && text[i]; i++) {
        hash ^= static_cast<uint8_t>(text[i]);
        hash *= 16777619u;
    }
    return hash;
}

uint32_t LevelHash() {
    const char* name = LevelName();
    return *name ? Fnv1a(name, 64) : 0;
}

bool InputEnabled() {
    GameState* gs = Game();
    return !gs || field<char>(gs, off::kGsInputEnabled) != 0;
}

void SetInputEnabled(bool enabled) {
    if (GameState* gs = Game()) field<char>(gs, off::kGsInputEnabled) = enabled ? 1 : 0;
}

// -------------------------------------------------------------------------------------------------
// Difficulté du jeu d'origine
// -------------------------------------------------------------------------------------------------
static void* g_getDifficulty = nullptr;  // fonction d'origine, une fois détournée
static int (*g_difficultyLevel)() = nullptr;

static int32_t ONIMP_ENTRY GetDifficultyHook() {
    int level = g_difficultyLevel ? g_difficultyLevel() : -1;
    if (level < 0) return reinterpret_cast<int32_t (*)()>(g_getDifficulty)();
    return level > 2 ? 2 : level;
}

bool InstallDifficultyHook(int (*level)()) {
    g_difficultyLevel = level;
    return HookEntry(addr::kGetDifficulty, addr::kGetDifficultyEntry, sizeof(addr::kGetDifficultyEntry),
                     reinterpret_cast<void*>(&GetDifficultyHook), &g_getDifficulty);
}

int Difficulty() {
    int level = fn::GetDifficulty();
    return level < 0 ? 0 : (level > 2 ? 2 : level);
}

float EnemyHealthFactor(int difficulty) {
    // Même lecture que la création d'un personnage (0x4DB38F) : trois nombres, un par difficulté.
    static const size_t kEnemyHealth = 0xAE0;
    void* settings = *reinterpret_cast<void**>(addr::kGameSettingsPtr);
    if (!settings || difficulty < 0 || difficulty > 2) return 1.0f;
    float factor = field<float>(settings, kEnemyHealth + 4 * static_cast<size_t>(difficulty));
    return std::isfinite(factor) && factor > 0.01f && factor < 100.0f ? factor : 1.0f;
}

// -------------------------------------------------------------------------------------------------
// Personnages
// -------------------------------------------------------------------------------------------------
int CharacterCount() {
    GameState* gs = Game();
    if (!gs) return 0;
    int count = field<int16_t>(gs, off::kGsCharacterCount);
    if (count < 0) return 0;
    return count > off::kMaxCharacters ? off::kMaxCharacters : count;
}

Character* CharacterAt(int index) {
    GameState* gs = Game();
    if (!gs || index < 0 || index >= off::kMaxCharacters) return nullptr;
    return reinterpret_cast<Character*>(reinterpret_cast<uint8_t*>(gs) + off::kGsCharacterStorage +
                                        static_cast<size_t>(index) * off::kCharacterSize);
}

bool InUse(Character* chr) { return (field<uint32_t>(chr, off::kChFlags) & off::kChFlagInUse) != 0; }
bool IsDead(Character* chr) { return (field<uint32_t>(chr, off::kChFlags) & off::kChFlagDead) != 0; }
bool IsAI(Character* chr) { return field<int32_t>(chr, off::kChType) == off::kChTypeAI2; }
uint16_t Handle(Character* chr) { return field<uint16_t>(chr, off::kChHandle); }
const char* Name(Character* chr) { return &field<char>(chr, off::kChName); }
int32_t Health(Character* chr) { return field<int32_t>(chr, off::kChHealth); }

const char* ClassName(Character* chr) {
    void* characterClass = field<void*>(chr, off::kChClass);
    const char* name = characterClass ? fn::GetInstanceName(characterClass) : nullptr;
    return name ? name : "";
}

bool SetClass(Character* chr, const char* className) {
    void* characterClass = nullptr;
    if (!chr || fn::GetDataPtr(Tag('O', 'N', 'C', 'C'), className, &characterClass) != 0 || !characterClass) return false;
    if (field<void*>(chr, off::kChClass) != characterClass) fn::SetCharacterClass(chr, characterClass);
    return true;
}

bool HasScripts(Character* chr) {
    for (int i = 0; i < off::kChScriptCount; i++) {
        if (field<char>(chr, off::kChScripts + static_cast<size_t>(i) * off::kChScriptSize)) return true;
    }
    return false;
}
Vector3 Location(Character* chr) { return field<Vector3>(chr, off::kChLocation); }
ActiveCharacter* Active(Character* chr) { return fn::GetActiveCharacter(chr); }

// -------------------------------------------------------------------------------------------------
// Visée
// -------------------------------------------------------------------------------------------------
static const float kPi = 3.14159265f;
static const float kAimHeight = 15.0f;     // la visée part de ce point au-dessus de la position (0x4E01B7)
static const float kAimDistance = 100.0f;  // point visé posé à cette distance, comme le moteur (0x499181)

// Ramène un angle entre -pi et pi.
static float WrapAngle(float angle) {
    if (!std::isfinite(angle)) return 0.0f;
    angle = std::fmod(angle + kPi, 2.0f * kPi);
    if (angle < 0.0f) angle += 2.0f * kPi;
    return angle - kPi;
}

static Vector3 AimDirection(float yaw, float pitch) {
    return {std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw)};
}

// Ce que le moteur ajoute à kAcHeadFacing avant d'orienter le buste, le bras et l'arme (0x4E5612,
// 0x4E5C73) : le chemin qu'il reste au corps pour se tourner, moins le décalage cosmétique.
static float BodyTurn(Character* chr) {
    float facing = field<float>(chr, off::kChFacing);
    float desired = field<float>(chr, off::kChDesiredFacing);
    float cosmetic = field<float>(chr, off::kChCosmeticFacing);
    float offset = WrapAngle(cosmetic);
    float remaining = WrapAngle(desired - cosmetic - facing);
    float turn = desired - facing;
    // Corps qui tourne du côté de son décalage : le moteur ne prend pas le plus court chemin.
    if (offset > 0.0f && remaining > 0.0f) {
        if (turn < 0.0f) turn += 2.0f * kPi;
    } else if (offset < 0.0f && remaining < 0.0f) {
        if (turn > 0.0f) turn -= 2.0f * kPi;
    } else {
        turn = WrapAngle(turn);
    }
    return turn - cosmetic;
}

// Direction réelle de l'arme d'un personnage, dans le monde. Ce n'est pas sa seule visée : le
// moteur y ajoute la visée automatique du joueur, qui dévie l'arme vers l'ennemi proche du viseur,
// et le recul. Transmettre la visée brute faisait tirer le pantin à côté.
static void WeaponDirection(Character* chr, ActiveCharacter* active, float& yaw, float& pitch) {
    yaw = WrapAngle(field<float>(chr, off::kChFacing) + BodyTurn(chr) + field<float>(active, off::kAcHeadFacing) +
                    field<float>(active, off::kAcAutoAimFacing));
    pitch = field<float>(active, off::kAcHeadPitch) + field<float>(chr, off::kChRecoil);
    if (!std::isfinite(pitch)) pitch = 0.0f;
    if (pitch > kPi / 2.0f) pitch = kPi / 2.0f;    // mêmes bornes que le moteur (0x4E5772)
    if (pitch < -kPi / 2.0f) pitch = -kPi / 2.0f;
    float autoAim = field<float>(active, off::kAcAutoAimPitch);
    if (std::isfinite(autoAim)) pitch += autoAim;
}

void ReadEntity(Character* chr, uint32_t characterID, proto::EntityState& out) {
    out = {};
    Vector3 location = Location(chr);
    out.x = location.x;
    out.y = location.y;
    out.z = location.z;
    out.rotation = field<float>(chr, off::kChFacing);
    out.characterID = characterID;

    int32_t health = Health(chr);
    out.health = static_cast<int16_t>(health < 0 ? 0 : (health > 32767 ? 32767 : health));

    out.tick = static_cast<uint16_t>(GameTime());

    uint16_t flags = 0;
    if (IsDead(chr)) flags |= proto::kEntityDead;
    if (Reloading(chr)) flags |= proto::kEntityReloading;
    // Ce qui se voit sur lui sans être dans son animation. L'aura, telle que le moteur la montre
    // ici : jusqu'à la 0.23 elle partait en huit niveaux, et une aura faible, arrondie à zéro, ne
    // se voyait pas chez les autres.
    float aura = SuperShown(chr);
    if (aura > 0.0f) {
        int level = static_cast<int>(aura * 255.0f + 0.5f);
        out.super = static_cast<uint8_t>(level < 1 ? 1 : (level > 255 ? 255 : level));
    }
    uint16_t shield = Shield(chr);
    out.shield = static_cast<uint8_t>(shield > 255 ? 255 : shield);
    if (uint16_t cloak = Cloak(chr)) {
        int steps = (cloak + proto::kCloakStep - 1) / proto::kCloakStep;
        out.cloak = static_cast<uint8_t>(steps > 255 ? 255 : steps);
    }
    Reserves reserves = ReadReserves(chr);
    out.ammo = static_cast<uint8_t>((reserves.clips > proto::kAmmoMask ? proto::kAmmoMask : reserves.clips) |
                                    ((reserves.cells > proto::kAmmoMask ? proto::kAmmoMask : reserves.cells)
                                     << proto::kCellShift));
    if (ActiveCharacter* active = Active(chr)) {
        void* weapon = field<void*>(chr, off::kChWeaponInHand);
        flags |= proto::kEntityActive;
        out.animID = AnimHash(field<void*>(active, off::kAcAnimation));
        out.animFrame = static_cast<uint16_t>(field<int16_t>(active, off::kAcAnimFrame));
        // Le récepteur reproduit le même fondu en posant l'animation.
        if (field<char>(active, off::kAcStitching)) {
            int length = field<int16_t>(active, off::kAcInterpLength);
            if (length < 0) length = 0;
            if (length > proto::kEntityInterpMask) length = proto::kEntityInterpMask;
            flags |= static_cast<uint16_t>(length << proto::kEntityInterpShift);
        }
        if (chr == Player()) {
            // Les touches de tir servent aussi à frapper : sans arme en main, ou avec une arme
            // vide. On ne transmet que les appuis que le moteur transmet lui-même à l'arme, sinon
            // le pantin, aux munitions infinies, tire pendant que le joueur donne des coups. De
            // même pendant un rechargement, où le moteur relâche la gâchette (0x4F6CDB).
            // Ni quand ses touches ne comptent pas : cinématique, touches coupées, personnage aux
            // mains de l'IA de déplacement. Le moteur n'y relit pas les touches, et la dernière
            // gâchette enfoncée restait notée : le pantin, chez les autres, tirait sans fin.
            bool shooting = weapon && !(field<uint32_t>(chr, off::kChFlags2) & off::kChFlag2FireIsMelee) &&
                            !field<void*>(active, off::kAcOverlay) && InputEnabled() && !InCutscene() &&
                            !(field<uint32_t>(chr, off::kChFlags) & off::kChFlagAiMovement);
            uint32_t actions = shooting ? field<uint32_t>(active, off::kAcActionsDownHigh) : 0;
            if (actions & off::kActionFire) flags |= proto::kEntityFire;
            if (actions & off::kActionAltFire) flags |= proto::kEntityAltFire;
        } else if (weapon && (field<uint16_t>(weapon, off::kWpFlags) & off::kWpFlagFiring)) {
            // L'IA n'a pas de touches : elle presse la gâchette de l'arme, qui en garde la trace.
            flags |= proto::kEntityFire;
        }
        // Visée : le joueur vise toujours, par sa caméra ; un PNJ seulement quand son IA vise.
        if (chr == Player() || field<char>(active, off::kAcIsAiming)) {
            float yaw, pitch;
            WeaponDirection(chr, active, yaw, pitch);
            out.aimYaw = static_cast<int16_t>(yaw * proto::kAimScale);
            out.aimPitch = static_cast<int16_t>(pitch * proto::kAimScale);
            if (!out.aimYaw && !out.aimPitch) out.aimYaw = 1;  // 0, 0 : « pas de visée »
        }
    }
    out.flags = flags;
}

// Visée imposée à un pantin, par numéro de personnage. Elle est appliquée par ApplyAim, à
// chaque tick du moteur.
struct AimOverride {
    bool active;
    bool logged;    // première application déjà notée au journal
    uint16_t handle;
    int32_t tick;   // dernier rappel de SetAim
    float yaw;
    float pitch;
};
static AimOverride g_aims[off::kMaxCharacters];
static const int32_t kAimHoldTicks = 30;  // sans rappel, le pantin retrouve la visée du moteur

static AimOverride* AimSlot(Character* chr) {
    int index = Handle(chr) & 0xFF;
    return index < off::kMaxCharacters ? &g_aims[index] : nullptr;
}

void SetAim(Character* chr, float yaw, float pitch) {
    ActiveCharacter* active = Active(chr);
    AimOverride* aim = AimSlot(chr);
    if (!active || !aim) return;
    bool logged = aim->logged && aim->handle == Handle(chr);
    *aim = {true, logged, Handle(chr), GameTime(), yaw, pitch};

    // Un pantin est un personnage d'IA : sa visée passe par l'exécuteur, qui la réécrit chaque
    // tick et la coupe quand rien ne la demande. On lui donne donc aussi une demande, comme le
    // ferait son IA : le moteur garde ainsi le pantin « en train de viser » d'un tick à l'autre.
    field<Vector3>(active, off::kAcExecAim) = AimDirection(yaw, pitch);
    field<char>(active, off::kAcExecAiming) = 1;
    field<char>(active, off::kAcExecStopAiming) = 0;
    field<char>(active, off::kAcExecAimAtPoint) = 0;
    field<char>(active, off::kAcExecAimWeaponSpeed) = 0;
    field<float>(active, off::kAcExecAimSpeed) = 100.0f;  // d'un coup : la direction reçue est déjà lissée
}

void ClearAim(Character* chr) {
    if (AimOverride* aim = AimSlot(chr)) aim->active = false;
    ActiveCharacter* active = Active(chr);
    if (active && field<char>(active, off::kAcExecAiming)) field<char>(active, off::kAcExecStopAiming) = 1;
}

void ForgetAims() { memset(g_aims, 0, sizeof(g_aims)); }

// Le moteur va poser le corps du personnage. Pour un pantin, on remplace ici la visée qu'il vient
// de calculer par celle reçue du réseau, de sorte que le bras et l'arme prennent exactement la
// direction qu'ils ont sur l'autre machine. L'écrire plus tôt ne tient pas : entre notre passage,
// une fois par image, et cet instant, le moteur la recalcule à chaque tick.
static void ApplyAim(Character* chr, ActiveCharacter* active) {
    AimOverride* aim = AimSlot(chr);
    if (!aim || !aim->active || aim->handle != Handle(chr)) return;
    // Pendant un rechargement le bras n'est plus à la visée : c'est l'animation de l'arme qui le
    // tient, et l'imposer ici la masquerait.
    if (Reloading(chr)) return;
    int32_t age = GameTime() - aim->tick;
    if (age < 0 || age > kAimHoldTicks) {
        aim->active = false;
        return;
    }
    // Ce que le moteur ajoutera de lui-même (BodyTurn, visée automatique, recul) est retranché.
    field<float>(active, off::kAcHeadFacing) = WrapAngle(aim->yaw - field<float>(chr, off::kChFacing)) -
                                               BodyTurn(chr) - field<float>(active, off::kAcAutoAimFacing);
    field<float>(active, off::kAcHeadPitch) =
        aim->pitch - field<float>(chr, off::kChRecoil) - field<float>(active, off::kAcAutoAimPitch);

    // Le reste de ce que le moteur tient à jour en visant, par cohérence.
    Vector3 direction = AimDirection(aim->yaw, aim->pitch);
    Vector3 origin = field<Vector3>(chr, off::kChPosition);
    field<float>(active, off::kAcAimFacing) = aim->yaw < 0.0f ? aim->yaw + 2.0f * kPi : aim->yaw;
    field<float>(active, off::kAcAimPitch) = aim->pitch < 0.0f ? aim->pitch + 2.0f * kPi : aim->pitch;
    field<Vector3>(active, off::kAcAimVector) = direction;
    field<Vector3>(active, off::kAcCamVector) = direction;
    field<Vector3>(active, off::kAcAimTarget) = {origin.x + direction.x * kAimDistance,
                                                 origin.y + kAimHeight + direction.y * kAimDistance,
                                                 origin.z + direction.z * kAimDistance};
    // Arme orientée exactement selon la visée, et non selon la seule pose de la main (0x4E5BE4).
    field<char>(active, off::kAcIsAiming) = 1;

    if (aim->logged) return;
    aim->logged = true;  // une ligne par personnage et par niveau : la preuve que le détour passe
    LOG("visee : '%s' suit la direction recue (%.2f, %.2f), corps a %.2f", Name(chr), aim->yaw, aim->pitch,
        field<float>(chr, off::kChFacing));
}

typedef void(ONICALL* BuildPoseFunction)(Character*, ActiveCharacter*);
static void* g_buildPose = nullptr;  // fonction d'origine, une fois détournée

static void ONICALL ONIMP_ENTRY BuildPoseHook(Character* chr, ActiveCharacter* active) {
    if (chr && active) ApplyAim(chr, active);
    reinterpret_cast<BuildPoseFunction>(g_buildPose)(chr, active);
}

bool InstallAimHook() {
    return HookEntry(addr::kBuildPose, addr::kBuildPoseEntry, sizeof(addr::kBuildPoseEntry),
                     reinterpret_cast<void*>(&BuildPoseHook), &g_buildPose);
}

bool Reloading(Character* chr) {
    ActiveCharacter* active = Active(chr);
    void* overlay = active ? field<void*>(active, off::kAcOverlay) : nullptr;
    if (!overlay) return false;
    uint16_t type = field<uint16_t>(overlay, off::kTramType);
    return type >= off::kTramTypeReloadFirst && type <= off::kTramTypeReloadLast;
}

// Variante d'animation que l'arme en main donne au personnage : une ou deux mains, droitier ou
// gaucher. Zéro sans arme. Le calcul du moteur (0x4DFC6F).
static uint16_t WeaponVariant(Character* chr) {
    void* weapon = field<void*>(chr, off::kChWeaponInHand);
    void* weaponClass = weapon ? field<void*>(weapon, off::kWpClass) : nullptr;
    void* characterClass = field<void*>(chr, off::kChClass);
    if (!weaponClass || !characterClass) return 0;
    bool twoHanded = (field<uint32_t>(weaponClass, off::kWcFlags) & off::kWcFlagTwoHanded) != 0;
    bool leftHanded = field<uint8_t>(characterClass, off::kClassLeftHanded) != 0;
    return twoHanded ? (leftHanded ? off::kVariantRifleLeft : off::kVariantRifleRight)
                     : (leftHanded ? off::kVariantPistolLeft : off::kVariantPistolRight);
}

// Le moteur ne pose cette variante qu'au moment où le personnage sort son arme (0x4DFC6F). Un
// pantin la reçoit toute sortie et ne l'avait donc jamais : toute recherche d'une animation propre
// à l'arme échouait pour lui, celle du rechargement d'abord (journaux de la 0.21 : « pas
// d'animation pour cette arme » à chaque rechargement).
void SyncWeaponVariant(Character* chr) {
    ActiveCharacter* active = Active(chr);
    if (!active) return;
    uint16_t& flags = field<uint16_t>(active, off::kAcVariantFlags);
    uint16_t wanted = static_cast<uint16_t>((flags & ~off::kVariantWeaponMask) | WeaponVariant(chr));
    if (flags != wanted) flags = wanted;
}

ReloadResult PlayReload(Character* chr) {
    ActiveCharacter* active = Active(chr);
    if (!active) return ReloadResult::Inactive;
    if (Reloading(chr)) return ReloadResult::Already;
    void* weapon = field<void*>(chr, off::kChWeaponInHand);
    void* weaponClass = weapon ? field<void*>(weapon, off::kWpClass) : nullptr;
    void* characterClass = field<void*>(chr, off::kChClass);
    if (!weaponClass || !characterClass) return ReloadResult::NoWeapon;
    // Même recherche que le rechargement du moteur (0x51C696) : l'animation dépend de l'arme, et de
    // la variante que l'arme donne au personnage, sans laquelle la recherche ne trouve rien (elle
    // écarte toute animation d'une variante que le personnage n'a pas, 0x428063).
    uint16_t variant = static_cast<uint16_t>((field<uint16_t>(active, off::kAcVariantFlags) & ~off::kVariantWeaponMask) |
                                             WeaponVariant(chr));
    void* tram = fn::LookupAnimation(field<void*>(characterClass, off::kClassAnimations),
                                     field<uint16_t>(weaponClass, off::kWcReloadAnimType), fn::kAnimStateOverlay, variant);
    if (!tram) return ReloadResult::NoAnimation;
    // Le moteur attend que l'emplacement soit libre (0x51C65E) ; ici le rechargement a déjà commencé
    // sur l'autre machine, et prend la place d'un geste d'arme qui s'y trouverait encore.
    fn::SetOverlay(&field<void*>(active, off::kAcOverlay), tram, 0);
    return ReloadResult::Played;
}

float SuperShown(Character* chr) {
    float shown = field<float>(chr, off::kChSuperShown);
    return shown > 0.0f && shown <= 1.0f ? shown : (shown > 1.0f ? 1.0f : 0.0f);
}

void SetSuper(Character* chr, float amount) {
    float& wanted = field<float>(chr, off::kChSuper);
    if (wanted != amount) wanted = amount;
}

uint16_t Shield(Character* chr) { return field<uint16_t>(chr, off::kChShield); }

void SetShield(Character* chr, uint16_t amount) {
    uint16_t& shield = field<uint16_t>(chr, off::kChShield);
    if (shield != amount) shield = amount;
}

uint16_t Cloak(Character* chr) { return field<uint16_t>(chr, off::kChCloak); }

void SetCloak(Character* chr, uint16_t ticks) {
    uint16_t& cloak = field<uint16_t>(chr, off::kChCloak);
    if (cloak != ticks) cloak = ticks;
}

Reserves ReadReserves(Character* chr) {
    return {field<uint16_t>(chr, off::kChAmmoClips), field<uint16_t>(chr, off::kChCells), field<uint16_t>(chr, off::kChHypos)};
}

void ClearReserves(Character* chr) {
    field<uint16_t>(chr, off::kChAmmoClips) = 0;
    field<uint16_t>(chr, off::kChCells) = 0;
    field<uint16_t>(chr, off::kChHypos) = 0;
}

uint16_t GivePowerup(Character* chr, uint16_t type, uint16_t amount) {
    if (!chr || type >= off::kPuTypeCount || !amount) return 0;
    return fn::GivePowerup(chr, type, amount, 0, 1);
}

Character* NeutralPartner(Character* chr) { return field<Character*>(chr, off::kChNeutralPartner); }

bool NeutralAction(Character* who) {
    GameState* gs = Game();
    if (!gs || !who) return false;
    // Le PNJ prend pour interlocuteur « le joueur de la partie », lu au moment où il entre dans cet
    // état (0x4ACE7F) : le temps de l'appel, c'est who.
    Character*& player = field<Character*>(gs, off::kGsPlayerCharacter);
    Character* saved = player;
    player = who;
    bool found = fn::NeutralAction(who, 1) != 0;
    player = saved;
    return found;
}

void SetInfiniteAmmo(Character* chr) { field<uint32_t>(chr, off::kChFlags) |= off::kChFlagInfiniteAmmo; }
bool HasInfiniteAmmo(Character* chr) { return (field<uint32_t>(chr, off::kChFlags) & off::kChFlagInfiniteAmmo) != 0; }
void ClearInfiniteAmmo(Character* chr) { field<uint32_t>(chr, off::kChFlags) &= ~off::kChFlagInfiniteAmmo; }

void ClearDrops(Character* chr) { memset(&field<uint8_t>(chr, off::kChDrops), 0, off::kChDropsSize); }

const char* WeaponClassName(void* weapon) {
    void* weaponClass = weapon ? field<void*>(weapon, off::kWpClass) : nullptr;
    if (!weaponClass) return "";
    const char* name = fn::GetInstanceName(weaponClass);
    return name ? name : "";
}

const char* WeaponInHand(Character* chr) { return WeaponClassName(field<void*>(chr, off::kChWeaponInHand)); }
const char* WeaponStored(Character* chr) { return WeaponClassName(field<void*>(chr, off::kChWeaponStored)); }

void StoreWeapon(Character* chr, bool store) { fn::UseWeapon(chr, nullptr, store ? 1 : 0); }

void TakeWeapon(Character* chr, void* weapon) {
    if (WeaponOnGround(weapon)) fn::UseWeapon(chr, weapon, 0);
}

void* DropWeapon(Character* chr) {
    void** inventory = &field<void*>(chr, off::kChWeaponInHand);
    void* weapon = *inventory;
    if (!weapon) return nullptr;
    // Mêmes gestes que la touche « lâcher » du joueur (0x4EDA8A).
    fn::DropSlot(inventory, 0, 0, 0);
    *inventory = nullptr;
    ActiveCharacter* active = Active(chr);
    if (!active) return weapon;
    if (void* extraBody = field<void*>(active, off::kAcExtraBody)) field<void*>(extraBody, 4) = nullptr;
    field<uint16_t>(active, off::kAcVariantFlags) &= 0x87FF;
    memset(&field<char>(active, off::kAcTriggerRequests), 0, 4);
    return weapon;
}

uint16_t WeaponAmmo(void* weapon) { return WeaponIndex(weapon) >= 0 ? field<uint16_t>(weapon, off::kWpAmmo) : 0; }

static uint32_t WeaponClassFlags(void* weapon) {
    void* weaponClass = WeaponIndex(weapon) >= 0 ? field<void*>(weapon, off::kWpClass) : nullptr;
    return weaponClass ? field<uint32_t>(weaponClass, off::kWcFlags) : 0;
}

bool WeaponUsesCells(void* weapon) { return (WeaponClassFlags(weapon) & off::kWcFlagUsesCells) != 0; }
bool WeaponExplosive(void* weapon) { return (WeaponClassFlags(weapon) & off::kWcFlagExplosive) != 0; }
bool WeaponHolsterable(void* weapon) { return weapon && !(WeaponClassFlags(weapon) & off::kWcFlagNoHolster); }

bool HolsterWeapon(Character* chr, bool store) {
    ActiveCharacter* active = Active(chr);
    if (!active || field<void*>(active, off::kAcOverlay)) return false;
    void* held = field<void*>(chr, off::kChWeaponInHand);
    void* stored = field<void*>(chr, off::kChWeaponStored);
    if (store) {
        if (!held || stored || !WeaponHolsterable(held)) return false;
        fn::HolsterWeapon(chr, active, 0);
    } else {
        if (held || !stored) return false;
        fn::DrawWeapon(chr, active, nullptr);
    }
    return field<void*>(active, off::kAcOverlay) != nullptr;
}

void SetPlayerPitch(Character* chr, float pitch) {
    ActiveCharacter* active = Active(chr);
    if (!active || !std::isfinite(pitch)) return;
    const float kLimit = 1.0f;  // le moteur borne à un quart de tour (0x4E5772) ; on reste en deçà
    if (pitch > kLimit) pitch = kLimit;
    if (pitch < -kLimit) pitch = -kLimit;
    field<float>(active, off::kAcHeadPitch) = pitch;
    field<float>(active, off::kAcAimPitch) = pitch < 0.0f ? pitch + 2.0f * 3.14159265f : pitch;
}

void SetWeaponAmmo(void* weapon, uint16_t ammo) {
    void* weaponClass = WeaponIndex(weapon) >= 0 ? field<void*>(weapon, off::kWpClass) : nullptr;
    if (!weaponClass) return;
    uint16_t capacity = field<uint16_t>(weaponClass, off::kWcMaxAmmo);
    field<uint16_t>(weapon, off::kWpAmmo) = ammo > capacity ? capacity : ammo;
}

// -------------------------------------------------------------------------------------------------
// Objets au sol
// -------------------------------------------------------------------------------------------------
static float DistanceSquared(const Vector3& a, const Vector3& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

void* WeaponAt(int index) {
    if (index < 0 || index >= addr::kMaxWeapons) return nullptr;
    return reinterpret_cast<void*>(addr::kWeapons + static_cast<uintptr_t>(index) * addr::kWeaponSize);
}

int WeaponIndex(void* weapon) {
    uintptr_t address = reinterpret_cast<uintptr_t>(weapon);
    if (address < addr::kWeapons) return -1;
    uintptr_t offset = address - addr::kWeapons;
    if (offset % addr::kWeaponSize != 0 || offset / addr::kWeaponSize >= static_cast<uintptr_t>(addr::kMaxWeapons)) {
        return -1;
    }
    return static_cast<int>(offset / addr::kWeaponSize);
}

bool WeaponInUse(void* weapon) {
    return WeaponIndex(weapon) >= 0 && (field<uint16_t>(weapon, off::kWpFlags) & off::kWpFlagInUse) != 0;
}

bool WeaponOnGround(void* weapon) { return WeaponInUse(weapon) && field<void*>(weapon, off::kWpOwner) == nullptr; }

Character* WeaponOwner(void* weapon) {
    return WeaponInUse(weapon) ? field<Character*>(weapon, off::kWpOwner) : nullptr;
}

Vector3 WeaponPosition(void* weapon) { return field<Vector3>(weapon, off::kWpPosition); }

void* FindGroundWeapon(const char* className, const Vector3& position, float radius, void* except) {
    void* best = nullptr;
    float bestDistance = radius * radius;
    for (int i = 0; i < addr::kMaxWeapons; i++) {
        void* weapon = WeaponAt(i);
        if (weapon == except || !WeaponOnGround(weapon) || strcmp(WeaponClassName(weapon), className) != 0) continue;
        float distance = DistanceSquared(WeaponPosition(weapon), position);
        if (distance > bestDistance) continue;
        best = weapon;
        bestDistance = distance;
    }
    return best;
}

void DeleteWeapon(void* weapon) {
    if (WeaponOnGround(weapon)) fn::DeleteWeapon(weapon);
}

static void* PowerupAt(int index) {
    return reinterpret_cast<void*>(addr::kPowerups + static_cast<uintptr_t>(index) * addr::kPowerupSize);
}

int PowerupCount() {
    int count = *reinterpret_cast<uint16_t*>(addr::kPowerupCount);
    return count > addr::kMaxPowerups ? addr::kMaxPowerups : count;
}

void ReadPowerup(int index, uint16_t& type, uint16_t& amount, Vector3& position) {
    void* powerup = PowerupAt(index);
    type = static_cast<uint16_t>(field<uint32_t>(powerup, off::kPuType));
    amount = field<uint16_t>(powerup, off::kPuAmount);
    position = field<Vector3>(powerup, off::kPuPosition);
}

bool NewPowerup(uint16_t type, uint16_t amount, const Vector3& position) {
    if (type >= off::kPuTypeCount || amount == 0) return false;
    return fn::NewPowerup(type, amount, &position, 0.0f) != nullptr;
}

void MovePowerup(int index, const Vector3& position) {
    void* powerup = PowerupAt(index);
    // Un bonus qui tombe est placé par le moteur à chaque tick.
    if (!field<void*>(powerup, off::kPuPhysics)) field<Vector3>(powerup, off::kPuPosition) = position;
}

bool DeletePowerupNear(uint16_t type, const Vector3& position, float radius) {
    void* best = nullptr;
    float bestDistance = radius * radius;
    int count = PowerupCount();
    for (int i = 0; i < count; i++) {
        void* powerup = PowerupAt(i);
        if (field<uint32_t>(powerup, off::kPuType) != type) continue;
        float distance = DistanceSquared(field<Vector3>(powerup, off::kPuPosition), position);
        if (distance > bestDistance) continue;
        best = powerup;
        bestDistance = distance;
    }
    if (best) fn::DeletePowerup(best);
    return best != nullptr;
}

bool TakePowerup(Character* chr, uint16_t type, const Vector3& position) {
    if (!chr) return false;
    void* best = nullptr;
    float bestDistance = 2.0f * 2.0f;
    int count = PowerupCount();
    for (int i = 0; i < count; i++) {
        void* powerup = PowerupAt(i);
        if (field<uint32_t>(powerup, off::kPuType) != type) continue;
        float distance = DistanceSquared(field<Vector3>(powerup, off::kPuPosition), position);
        if (distance > bestDistance) continue;
        best = powerup;
        bestDistance = distance;
    }
    if (!best) return false;
    Vector3 at = field<Vector3>(best, off::kPuPosition), here = Location(chr);
    float dx = at.x - here.x, dy = at.y - here.y, dz = at.z - here.z;
    if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz)) return false;
    if (dx * dx + dz * dz >= fn::kPickupReach * fn::kPickupReach || dy < -fn::kPickupBelow || dy > fn::kPickupAbove) {
        return false;
    }
    fn::PickupPowerup(chr, best);
    return PowerupCount() < count;
}

void Trigger(Character* chr, TriggerEvent event) {
    ActiveCharacter* active = Active(chr);
    if (!active) return;
    switch (event) {
        case TriggerEvent::Pressed: fn::TriggerPressed(chr, active); break;
        case TriggerEvent::Held: fn::TriggerHeld(chr, active); break;
        case TriggerEvent::Released: fn::TriggerReleased(chr, active); break;
        case TriggerEvent::AltPressed: fn::AltTriggerPressed(chr, active); break;
        case TriggerEvent::AltReleased: fn::AltTriggerReleased(chr, active); break;
    }
}

// -------------------------------------------------------------------------------------------------
// Nombres qui n'en sont pas
// -------------------------------------------------------------------------------------------------
// Une position ou une orientation « NaN » dans un personnage ne se répare jamais d'elle-même : tout
// ce que le moteur en tire l'est à son tour, et ses boucles « tant que l'angle est négatif, ajouter
// un tour » ne finissent plus (boussole du HUD, 0x500860 : c'était le jeu figé des clients de la
// 0.28). Rien de tel n'entre donc dans le moteur par nous, et ce qui y serait né est remis d'aplomb.
static bool Finite(const Vector3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

// Remet à zéro les flottants qui ne sont pas des nombres, de from à to (multiples de quatre).
static int ZeroBadFloats(void* base, size_t from, size_t to) {
    int fixed = 0;
    for (size_t at = from; at < to; at += sizeof(float)) {
        float& value = field<float>(base, at);
        if (std::isfinite(value)) continue;
        value = 0.0f;
        fixed++;
    }
    return fixed;
}

bool TransformValid(Character* chr) {
    if (!Finite(field<Vector3>(chr, off::kChPosition)) || !Finite(field<Vector3>(chr, off::kChLocation)) ||
        !std::isfinite(field<float>(chr, off::kChFacing)) || !std::isfinite(field<float>(chr, off::kChDesiredFacing))) {
        return false;
    }
    ActiveCharacter* active = Active(chr);
    void* physics = active ? field<void*>(active, off::kAcPhysics) : nullptr;
    return !physics || (Finite(field<Vector3>(physics, off::kPhPosition)) && Finite(field<Vector3>(physics, off::kPhVelocity)));
}

void WriteTransform(Character* chr, const Vector3& position, float facing) {
    if (!Finite(position) || !std::isfinite(facing)) return;
    // Position (+0x148) et la position physique suivent Location avec un décalage propre au
    // moteur : on leur applique le même déplacement plutôt qu'une valeur absolue. Sauf si l'une
    // d'elles n'est plus un nombre : un déplacement n'y changerait rien, elle est alors écrite
    // telle quelle, et le moteur retrouve son décalage au tick suivant.
    Vector3& location = field<Vector3>(chr, off::kChLocation);
    Vector3& actual = field<Vector3>(chr, off::kChPosition);
    bool sound = Finite(location) && Finite(actual);
    Vector3 delta = {position.x - location.x, position.y - location.y, position.z - location.z};

    if (sound) {
        actual.x += delta.x;
        actual.y += delta.y;
        actual.z += delta.z;
    } else {
        actual = position;
    }
    location = position;
    field<Vector3>(chr, off::kChLastPosition) = position;

    field<float>(chr, off::kChFacing) = facing;
    field<float>(chr, off::kChDesiredFacing) = facing;
    ZeroBadFloats(chr, off::kChCosmeticFacing, off::kChCosmeticFacing + sizeof(float));
    ZeroBadFloats(chr, off::kChVelocity, off::kChVelocity + sizeof(Vector3));

    if (ActiveCharacter* active = Active(chr)) {
        // Élans que le moteur ajoute à la position chaque tick : poussée d'un coup, pas de
        // l'animation, chute, saut. Les entiers qui les séparent ne sont pas touchés.
        for (const auto& range : off::kAcMotion) ZeroBadFloats(active, range[0], range[1]);
        if (void* physics = field<void*>(active, off::kAcPhysics)) {
            Vector3& physicsPosition = field<Vector3>(physics, off::kPhPosition);
            if (sound && Finite(physicsPosition)) {
                physicsPosition.x += delta.x;
                physicsPosition.y += delta.y;
                physicsPosition.z += delta.z;
            } else {
                physicsPosition = position;
            }
            field<Vector3>(physics, off::kPhVelocity) = {0.0f, 0.0f, 0.0f};
        }
    }
}

// -------------------------------------------------------------------------------------------------
// Ligne de vue, sprint
// -------------------------------------------------------------------------------------------------
bool CastRay(Character* from, const Vector3& origin, const Vector3& target, Ray& out) {
    out = {RayKind::Nothing, nullptr, target, 0.0f};
    GameState* gs = Game();
    if (!gs || !Finite(origin) || !Finite(target)) return false;
    Vector3 direction = {target.x - origin.x, target.y - origin.y, target.z - origin.z};
    float length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (!(length > 1.0f)) return false;
    direction = {direction.x / length, direction.y / length, direction.z / length};

    fn::RayResult result = {};
    fn::RayToEverything(from ? static_cast<uint32_t>(Handle(from) & 0xFF) : 0xFFFEu, &origin, &direction, &result);
    if (result.type == fn::kRayNothing || !Finite(result.point)) return true;
    float dx = result.point.x - origin.x, dy = result.point.y - origin.y, dz = result.point.z - origin.z;
    out.point = result.point;
    out.distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (result.type == fn::kRayCharacter) {
        out.kind = RayKind::Character;
        // Le moteur rend un personnage : on ne s'en sert que si c'est bien une case de sa table.
        uintptr_t first = reinterpret_cast<uintptr_t>(gs) + off::kGsCharacterStorage;
        uintptr_t hit = reinterpret_cast<uintptr_t>(result.hit);
        if (hit >= first && (hit - first) % off::kCharacterSize == 0 &&
            (hit - first) / off::kCharacterSize < static_cast<uintptr_t>(off::kMaxCharacters)) {
            out.chr = reinterpret_cast<Character*>(result.hit);
        }
    } else {
        out.kind = result.type == fn::kRayObject ? RayKind::Object : RayKind::Environment;
    }
    return true;
}

void Sprint(Character* chr, bool on) {
    ActiveCharacter* active = Active(chr);
    if (!active) return;
    int32_t& timer = field<int32_t>(active, off::kAcSprintTimer);
    if (on) timer = -1;
    else if (timer == -1) timer = 0;  // le moteur retire alors la variante « sprint » (0x4DDF7D)
}

// Dernière place saine de chaque personnage, par case : relevée à chaque image, et à chaque tick
// pour notre joueur.
struct SoundPlace {
    Vector3 location;
    float facing;
    uint16_t handle;
    uint32_t level;
    int32_t time;      // GameTime du relevé : un niveau rechargé repart de zéro
    bool known;
};
static SoundPlace g_sound[off::kMaxCharacters];
static unsigned g_repairs = 0;

bool GuardTransform(Character* chr, const char* when) {
    if (!chr || !InUse(chr)) return false;
    SoundPlace& sound = g_sound[Handle(chr) & (off::kMaxCharacters - 1)];
    uint32_t level = LevelHash();
    int32_t time = GameTime();
    if (TransformValid(chr)) {
        sound = {Location(chr), field<float>(chr, off::kChFacing), Handle(chr), level, time, true};
        return false;
    }
    Vector3 location = Location(chr), actual = field<Vector3>(chr, off::kChPosition);
    float facing = field<float>(chr, off::kChFacing);
    bool known = sound.known && sound.handle == Handle(chr) && sound.level == level && time >= sound.time;
    // Sans place saine connue : là où il est, pour ce qui en reste de lisible.
    Vector3 back = {0.0f, 0.0f, 0.0f};
    if (known) back = sound.location;
    else if (Finite(location)) back = location;
    else if (Finite(actual)) back = actual;
    else if (Character* player = Player()) {
        if (player != chr && Finite(Location(player))) back = Location(player);
    }
    float turn = known ? sound.facing : (std::isfinite(facing) ? facing : 0.0f);
    WriteTransform(chr, back, turn);
    if (ActiveCharacter* active = Active(chr)) {
        // Visée : recalculée par le moteur d'après l'orientation, mais à partir de ce qu'elle était.
        ZeroBadFloats(active, off::kAcAimTarget, off::kAcAutoAimPitch + sizeof(float));
    }
    // Les premières fois seulement : si la cause dure, c'est à chaque tick.
    if (g_repairs++ < 40) {
        LOG("NAN : position de '%s' invalide %s (place %.1f %.1f %.1f, position %.1f %.1f %.1f, cap %.2f) : "
            "remis en (%.1f, %.1f, %.1f), cap %.2f, anim '%s'%s",
            Name(chr), when, location.x, location.y, location.z, actual.x, actual.y, actual.z, facing, back.x, back.y,
            back.z, turn, AnimName(CurrentAnimation(chr)), known ? "" : ", faute de place saine connue");
    }
    return true;
}

bool GuardPlayerTransform(const char* when) { return GuardTransform(Player(), when); }

int GuardTransforms(const char* when) {
    int repaired = 0, count = CharacterCount();
    for (int i = 0; i < count; i++) {
        if (GuardTransform(CharacterAt(i), when)) repaired++;
    }
    return repaired;
}

// -------------------------------------------------------------------------------------------------
// Animations
// -------------------------------------------------------------------------------------------------
static std::unordered_map<uint32_t, void*> g_animByHash;
static std::unordered_map<void*, uint32_t> g_hashByAnim;

void RebuildAnimTable() {
    g_animByHash.clear();
    g_hashByAnim.clear();

    const uint32_t tag = Tag('T', 'R', 'A', 'M');
    uint32_t count = fn::GetTagCount(tag);
    for (uint32_t i = 0; i < count; i++) {
        void* tram = nullptr;
        if (fn::GetDataPtrByNumber(tag, i, &tram) != 0 || !tram) continue;
        const char* name = fn::GetInstanceName(tram);
        if (!name || !*name) continue;
        uint32_t hash = Fnv1a(name);
        g_animByHash.emplace(hash, tram);
        g_hashByAnim[tram] = hash;
    }
    LOG("animations : %u instances TRAM, %u indexees", count, static_cast<unsigned>(g_animByHash.size()));
}

const char* AnimName(void* tram) {
    if (!tram) return "";
    const char* name = fn::GetInstanceName(tram);
    return name ? name : "";
}

uint32_t AnimHash(void* tram) {
    if (!tram) return 0;
    auto found = g_hashByAnim.find(tram);
    if (found != g_hashByAnim.end()) return found->second;

    const char* name = AnimName(tram);
    if (!*name) return 0;
    uint32_t hash = Fnv1a(name);
    g_hashByAnim[tram] = hash;
    g_animByHash.emplace(hash, tram);
    return hash;
}

void* AnimFromHash(uint32_t hash) {
    auto found = g_animByHash.find(hash);
    return found == g_animByHash.end() ? nullptr : found->second;
}

void* CurrentAnimation(Character* chr) {
    ActiveCharacter* active = Active(chr);
    return active ? field<void*>(active, off::kAcAnimation) : nullptr;
}

int CurrentFrame(Character* chr) {
    ActiveCharacter* active = Active(chr);
    return active ? field<int16_t>(active, off::kAcAnimFrame) : 0;
}

bool SetAnimation(Character* chr, void* tram, int interpFrames) {
    ActiveCharacter* active = Active(chr);
    if (!active || !tram) return false;
    // Pour une animation de cible projetée, le moteur lit le numéro du lanceur sans le contrôler
    // et s'en sert d'indice de table : il doit désigner un personnage. À défaut, la cible même.
    if (field<uint32_t>(tram, off::kTramFlags) & off::kTramFlagThrowTarget) {
        int16_t& thrownBy = field<int16_t>(active, off::kAcThrownBy);
        Character* thrower = thrownBy >= 0 && thrownBy < off::kMaxCharacters ? CharacterAt(thrownBy) : nullptr;
        if (!thrower || !InUse(thrower)) thrownBy = static_cast<int16_t>(Handle(chr) & 0xFF);
    }
    // Même appel que la commande BSL chr_animate.
    fn::SetAnimationExternal(chr, field<int16_t>(active, off::kAcAnimFromState), tram, interpFrames);
    return true;
}

int AnimDuration(void* tram) { return tram ? field<uint16_t>(tram, off::kTramDuration) : 0; }

void SetAnimFrame(Character* chr, int frame) {
    ActiveCharacter* active = Active(chr);
    if (!active) return;
    int duration = AnimDuration(field<void*>(active, off::kAcAnimation));
    if (duration <= 0) return;
    if (frame >= duration) frame = duration - 1;
    if (frame < 0) frame = 0;
    field<int16_t>(active, off::kAcAnimFrame) = static_cast<int16_t>(frame);
}

void LockAnimation(Character* chr, bool locked) {
    ActiveCharacter* active = Active(chr);
    if (!active) return;
    // Même état que pendant un chr_animate : le moteur boucle l'animation courante et n'en
    // choisit plus aucune. Le compteur descend d'un par tick, d'où la valeur large.
    if (locked) {
        field<uint32_t>(chr, off::kChFlags) |= off::kChFlagScriptedAnim;
        field<int32_t>(active, off::kAcForcedAnimFrames) = 600;
    } else {
        field<uint32_t>(chr, off::kChFlags) &= ~off::kChFlagScriptedAnim;
        field<int32_t>(active, off::kAcForcedAnimFrames) = 0;
    }
}

bool Fallen(Character* chr, bool* resting) {
    if (resting) *resting = false;
    ActiveCharacter* active = Active(chr);
    void* tram = active ? field<void*>(active, off::kAcAnimation) : nullptr;
    if (!tram) return false;
    int state = field<uint16_t>(tram, off::kTramToState);
    if (state != off::kAnimStateFallenBack && state != off::kAnimStateFallenFront) return false;
    if (resting) *resting = field<uint16_t>(tram, off::kTramFromState) == state;
    return true;
}

bool GroundAnimation(void* tram) {
    if (!tram) return false;
    int from = field<uint16_t>(tram, off::kTramFromState), to = field<uint16_t>(tram, off::kTramToState);
    bool fromGround = from == off::kAnimStateFallenBack || from == off::kAnimStateFallenFront;
    bool toGround = to == off::kAnimStateFallenBack || to == off::kAnimStateFallenFront;
    return fromGround && toGround;
}

bool LayDown(Character* chr) {
    ActiveCharacter* active = Active(chr);
    void* characterClass = field<void*>(chr, off::kChClass);
    if (!active || !characterClass) return false;
    // L'animation de repos de l'état « au sol, sur le dos », pour la classe du personnage.
    void* tram = fn::LookupAnimation(field<void*>(characterClass, off::kClassAnimations), off::kAnimTypeStand,
                                     static_cast<uint32_t>(off::kAnimStateFallenBack),
                                     field<uint16_t>(active, off::kAcVariantFlags));
    if (!tram) tram = AnimFromHash(Fnv1a("KONCOMfallen_back"));
    return SetAnimation(chr, tram, 8);
}

// -------------------------------------------------------------------------------------------------
// Projections
// -------------------------------------------------------------------------------------------------
bool IsThrowAnimation(void* tram) {
    return tram && (field<uint32_t>(tram, off::kTramFlags) & off::kTramFlagThrowSource) != 0;
}

bool ThrowInProgress(Character* chr, Character*& target, void*& targetAnim) {
    ActiveCharacter* active = Active(chr);
    // Le numéro de la cible n'est noté qu'une fois la projection appliquée, et effacé à
    // l'animation suivante.
    if (!active || field<int16_t>(active, off::kAcThrowing) < 0) return false;
    if (!IsThrowAnimation(field<void*>(active, off::kAcAnimation))) return false;
    target = field<Character*>(active, off::kAcThrowTarget);
    targetAnim = field<void*>(active, off::kAcTargetThrow);
    return target && targetAnim;
}

void SetThrowTarget(Character* chr, Character* target, void* targetAnim) {
    ActiveCharacter* active = Active(chr);
    if (!active) return;
    field<Character*>(active, off::kAcThrowTarget) = nullptr;
    field<void*>(active, off::kAcTargetThrow) = nullptr;
    if (!target || !targetAnim) return;

    // Pendant sa chute, la cible désigne son lanceur par son numéro (0x4DD83A), sans contrôle :
    // il doit être écrit, donc elle doit avoir son ActiveCharacter dès maintenant.
    if (!Active(target)) fn::MakeActive(target, 1);
    ActiveCharacter* targetActive = Active(target);
    if (!targetActive) return;
    field<int16_t>(targetActive, off::kAcThrownBy) = static_cast<int16_t>(Handle(chr) & 0xFF);
    field<Character*>(active, off::kAcThrowTarget) = target;
    field<void*>(active, off::kAcTargetThrow) = targetAnim;
}

// -------------------------------------------------------------------------------------------------
// Actions
// -------------------------------------------------------------------------------------------------
// Crée le personnage décrit par cet objet CHAR, comme le fait ai2_spawn (0x48A4E2).
static Character* SpawnObject(void* object, const char* name) {
    uint32_t index = 0;
    int16_t error = fn::NewCharacter(object, nullptr, nullptr, &index);
    index &= 0xFFFF;
    if (error != 0 || index >= static_cast<uint32_t>(off::kMaxCharacters)) {
        LOG("spawn : NewCharacter a echoue pour '%s' (erreur %d, index %u)", name, error, index);
        return nullptr;
    }
    Character* chr = CharacterAt(static_cast<int>(index));
    if (!chr || !InUse(chr)) {
        LOG("spawn : slot %u inutilisable pour '%s'", index, name);
        return nullptr;
    }
    LOG("spawn : '%s' (%s) slot %u handle %04X", name, ClassName(chr), index, Handle(chr));
    return chr;
}

static uint8_t g_spawnObject[off::kFakeObjectSize];

struct ObjectSearch {
    const char* name;
    void* found;
};

static uint8_t ONICALL ONIMP_ENTRY MatchCharacterObject(void* object, void* userData) {
    ObjectSearch* search = static_cast<ObjectSearch*>(userData);
    const char* name = &field<char>(object, off::kObjOsd + off::kOsdName);
    if (strncmp(name, search->name, 32) != 0) return 1;  // continuer
    search->found = object;
    return 0;
}

void* FindCharacterObject(const char* name) {
    ObjectSearch search = {name, nullptr};
    if (name && *name) fn::EnumerateObjects(Tag('C', 'H', 'A', 'R'), MatchCharacterObject, &search);
    return search.found;
}

Character* SpawnCopy(void* object, const char* name, const Vector3& position, float facing, int team) {
    if (!object || field<uint32_t>(object, off::kObjType) != Tag('C', 'H', 'A', 'R')) return nullptr;
    uint8_t* copy = g_spawnObject;
    memset(copy, 0, sizeof(g_spawnObject));
    memcpy(copy, object, off::kObjOsd + off::kOsdSize);
    field<Vector3>(copy, off::kObjPosition) = position;

    uint8_t* osd = copy + off::kObjOsd;
    // Ni joueur, ni increvable, ni boss, et aucun script du niveau : la copie ne compte pour rien
    // dans ce que les scripts attendent de l'original.
    field<uint32_t>(osd, off::kOsdFlags) &= ~(off::kOsdFlagPlayer | off::kOsdFlagNotPresent | off::kOsdFlagSpawned |
                                              off::kOsdFlagUnkillable | off::kOsdFlagBoss);
    memset(osd + off::kOsdName, 0, 32);
    strncpy(&field<char>(osd, off::kOsdName), name, 31);
    memset(osd + off::kOsdScripts, 0, off::kOsdScriptsSize);
    field<float>(osd, off::kOsdFacing) = facing;
    if (team >= 0) field<uint16_t>(osd, off::kOsdTeam) = static_cast<uint16_t>(team);
    return SpawnObject(copy, name);
}

Character* SpawnPuppet(const char* className, const char* name, const Vector3& position, float facing,
                       uint16_t team, uint32_t osdFlags) {
    void* characterClass = nullptr;
    if (fn::GetDataPtr(Tag('O', 'N', 'C', 'C'), className, &characterClass) != 0 || !characterClass) {
        LOG("spawn : classe '%s' introuvable", className);
        return nullptr;
    }

    // Objet 'CHAR' minimal, tel que le lit ONrGameState_NewCharacter.
    uint8_t* object = g_spawnObject;
    memset(object, 0, sizeof(g_spawnObject));
    field<uint32_t>(object, off::kObjType) = Tag('C', 'H', 'A', 'R');
    field<Vector3>(object, off::kObjPosition) = position;

    uint8_t* osd = object + off::kObjOsd;
    // Pantin : munitions infinies, il tire quand l'autre joueur tire, sans gérer de chargeur.
    field<uint32_t>(osd, off::kOsdFlags) = osdFlags;
    strncpy(&field<char>(osd, off::kOsdClass), className, 63);
    strncpy(&field<char>(osd, off::kOsdName), name, 31);
    field<uint16_t>(osd, off::kOsdTeam) = team;
    field<uint16_t>(osd, off::kOsdMeleeId) = field<uint16_t>(characterClass, off::kClassDefaultMeleeId);
    field<float>(osd, off::kOsdFacing) = facing;

    return SpawnObject(object, name);
}

// Le premier objet CHAR du niveau qui se bat : ni le joueur, ni un civil.
static uint8_t ONICALL ONIMP_ENTRY MatchFighterObject(void* object, void* userData) {
    uint32_t flags = field<uint32_t>(object, off::kObjOsd + off::kOsdFlags);
    if (flags & (off::kOsdFlagPlayer | off::kOsdFlagNonCombatant)) return 1;  // continuer
    *static_cast<void**>(userData) = object;
    return 0;
}

Character* SpawnNpc(const char* className, const char* name, const Vector3& position, float facing, uint16_t team) {
    void* characterClass = nullptr;
    if (fn::GetDataPtr(Tag('O', 'N', 'C', 'C'), className, &characterClass) != 0 || !characterClass) {
        LOG("spawn : classe '%s' introuvable", className);
        return nullptr;
    }
    void* model = nullptr;
    fn::EnumerateObjects(Tag('C', 'H', 'A', 'R'), MatchFighterObject, &model);
    if (!model) return SpawnPuppet(className, name, position, facing, team, 0);

    uint8_t* copy = g_spawnObject;
    memset(copy, 0, sizeof(g_spawnObject));
    memcpy(copy, model, off::kObjOsd + off::kOsdSize);
    field<Vector3>(copy, off::kObjPosition) = position;

    uint8_t* osd = copy + off::kObjOsd;
    field<uint32_t>(osd, off::kOsdFlags) &= ~(off::kOsdFlagPlayer | off::kOsdFlagNotPresent | off::kOsdFlagSpawned |
                                              off::kOsdFlagUnkillable | off::kOsdFlagInfiniteAmmo | off::kOsdFlagBoss);
    memset(osd + off::kOsdClass, 0, 64);
    strncpy(&field<char>(osd, off::kOsdClass), className, 63);
    memset(osd + off::kOsdName, 0, 32);
    strncpy(&field<char>(osd, off::kOsdName), name, 31);
    memset(osd + off::kOsdWeapon, 0, 64);
    memset(osd + off::kOsdScripts, 0, off::kOsdScriptsSize);
    field<uint16_t>(osd, off::kOsdTeam) = team;
    // Les coups de sa propre classe, et non ceux du modèle.
    field<uint16_t>(osd, off::kOsdMeleeId) = field<uint16_t>(characterClass, off::kClassDefaultMeleeId);
    field<float>(osd, off::kOsdFacing) = facing;
    return SpawnObject(copy, name);
}

void* SpawnWeapon(const char* className, const Vector3& position, float facing) {
    void* weaponClass = nullptr;
    if (!Finite(position) || fn::GetDataPtr(Tag('O', 'N', 'W', 'C'), className, &weaponClass) != 0 || !weaponClass) {
        return nullptr;
    }
    uint8_t place[fn::kPlaceSize] = {};
    field<Vector3>(place, fn::kPlacePosition) = position;
    field<float>(place, fn::kPlaceFacing) = facing;
    return fn::WeaponSpawn(weaponClass, place);
}

uint16_t WeaponCapacity(void* weapon) {
    void* weaponClass = weapon ? field<void*>(weapon, off::kWpClass) : nullptr;
    return weaponClass ? field<uint16_t>(weaponClass, off::kWcMaxAmmo) : 0;
}

static const char* InstanceNameAt(uint32_t tag, int index) {
    void* instance = nullptr;
    if (index < 0 || fn::GetDataPtrByNumber(tag, static_cast<uint32_t>(index), &instance) != 0 || !instance) return "";
    const char* name = fn::GetInstanceName(instance);
    return name ? name : "";
}

int CharacterClassCount() { return static_cast<int>(fn::GetTagCount(Tag('O', 'N', 'C', 'C'))); }
const char* CharacterClassAt(int index) { return InstanceNameAt(Tag('O', 'N', 'C', 'C'), index); }
int WeaponClassCount() { return static_cast<int>(fn::GetTagCount(Tag('O', 'N', 'W', 'C'))); }
const char* WeaponClassAt(int index) { return InstanceNameAt(Tag('O', 'N', 'W', 'C'), index); }

void DeleteCharacter(Character* chr) { fn::DeleteCharacter(chr); }

void SetHealth(Character* chr, uint32_t hitPoints) { fn::SetHitPoints(chr, hitPoints); }

// -------------------------------------------------------------------------------------------------
// Dégâts
// -------------------------------------------------------------------------------------------------
static void* g_takeDamage = nullptr;  // fonction d'origine, une fois détournée
static DamageFilter g_damageFilter = nullptr;

static uint32_t ONICALL ONIMP_ENTRY TakeDamageHook(Character* victim, uint32_t damage, uint32_t knockback,
                                                   const Vector3* location, const Vector3* direction, uint32_t owner,
                                                   uint32_t impact) {
    DamageInfo info;
    info.victim = victim;
    info.attacker = fn::OwnerCharacter(owner);
    info.damage = damage;
    info.ownerType = owner & fn::kOwnerTypeMask;
    info.impact = impact;
    memcpy(&info.knockback, &knockback, sizeof(info.knockback));
    // Une poussée qui n'est pas un nombre finirait dans la position de la victime (voir
    // GuardPlayerTransform) : le coup passe, sans elle.
    if (!std::isfinite(info.knockback) || (direction && !Finite(*direction)) || (location && !Finite(*location))) {
        static unsigned said = 0;
        if (said++ < 10) LOG("NAN : poussee invalide d'un coup sur '%s', retiree", Name(victim));
        info.knockback = 0.0f;
        knockback = 0;
        direction = nullptr;
        location = nullptr;
    }
    info.direction = direction;
    info.hit = info.ownerType != fn::kOwnerMelee || location || direction;
    if (g_damageFilter) g_damageFilter(info);
    return reinterpret_cast<fn::TakeDamageFunction>(g_takeDamage)(victim, info.damage, knockback, location, direction,
                                                                  owner, impact);
}

bool InstallDamageHook(DamageFilter filter) {
    g_damageFilter = filter;
    return HookEntry(addr::kTakeDamage, addr::kTakeDamageEntry, sizeof(addr::kTakeDamageEntry),
                     reinterpret_cast<void*>(&TakeDamageHook), &g_takeDamage);
}

typedef float(ONICALL* InaccuracyFunction)(uint32_t owner);
static void* g_inaccuracy = nullptr;  // fonction d'origine, une fois détournée
static bool (*g_isPuppet)(Character*) = nullptr;

static float ONICALL ONIMP_ENTRY InaccuracyHook(uint32_t owner) {
    Character* shooter = fn::OwnerCharacter(owner);
    if (shooter && g_isPuppet && g_isPuppet(shooter)) return 0.0f;
    return reinterpret_cast<InaccuracyFunction>(g_inaccuracy)(owner);
}

bool InstallShooterHook(bool (*isPuppet)(Character*)) {
    g_isPuppet = isPuppet;
    return HookEntry(addr::kShooterInaccuracy, addr::kShooterInaccuracyEntry, sizeof(addr::kShooterInaccuracyEntry),
                     reinterpret_cast<void*>(&InaccuracyHook), &g_inaccuracy);
}

// -------------------------------------------------------------------------------------------------
// Sauvegarde et reprise
// -------------------------------------------------------------------------------------------------
void ReadContinue(int32_t& savePoint, uint8_t* data) {
    savePoint = *reinterpret_cast<int32_t*>(addr::kContinueSavePoint);
    memcpy(data, reinterpret_cast<const void*>(addr::kContinueData), addr::kContinueSize);
}

void WriteContinue(int32_t savePoint, const uint8_t* data) {
    *reinterpret_cast<int32_t*>(addr::kContinueSavePoint) = savePoint;
    memcpy(reinterpret_cast<void*>(addr::kContinueData), data, addr::kContinueSize);
}

void RestartLevel() {
    if (GameState* gs = Game()) field<int32_t>(gs, off::kGsOutcome) = off::kOutcomeLost;
}

// -------------------------------------------------------------------------------------------------
// Garde des zones de texte du moteur
// -------------------------------------------------------------------------------------------------
// Le client de la 0.24 a planté en pleine partie dans le tracé d'une zone de texte du moteur
// (« EXCEPTION C0000005 a 0043102e, acces 0000020c » : 0x43102E lit la première ligne de la zone,
// dont le pointeur des lignes était nul) ; celui de la 0.23, à la fermeture, en libérant la même
// zone, lignes et contexte nuls. Le moteur ne contrôle rien : ni en traçant (0x431021), ni en
// écrivant (0x4313D6), ni en libérant (0x42FAF6). Ce qui vide cette zone chez le client n'est pas
// établi ; aucun des appels du moteur ni de Daodan qui la redimensionnent ne le fait. D'ici là
// chaque zone est contrôlée à chaque image : on en garde la forme tant qu'elle est saine, et dès
// qu'elle ne l'est plus on en crée une neuve de la même forme, que l'on met à sa place, sans rien
// écrire dans l'ancienne (si sa mémoire a été reprise par autre chose, elle n'est plus à nous). Le
// journal garde ses octets tels qu'ils ont été trouvés : c'est eux qui diront ce qui l'a touchée.
struct AreaShape {
    bool known;
    void* area;
    void* family;
    uint16_t alignment, fontSize, maxEntries;
    uint8_t formatting, scaleFont, bottomJustify, displayCompletion, fadeOverBounds;
    int16_t bounds[4];
};
static const char* const kAreaNames[] = {"commande", "console", "messages", "sous-titres"};
static const int kAreaCount = static_cast<int>(sizeof(addr::kTextAreas) / sizeof(addr::kTextAreas[0]));
static AreaShape g_shapes[kAreaCount];
static int g_areaRepairs = 0;
static int g_areaFailures[kAreaCount] = {};  // zones refaites qui ne valaient pas mieux : on n'insiste pas
static const int kMaxAreaFailures = 3;

// Pourquoi cette zone ne peut plus servir, ou nullptr si elle est saine.
static const char* AreaFault(void* area, const AreaShape& shape) {
    if (IsBadReadPtr(area, off::kTaSize)) return "memoire illisible";
    if (!field<void*>(area, off::kTaEntries)) return "plus de lignes";
    if (!field<void*>(area, off::kTaContext)) return "plus de contexte de texte";
    if (!field<uint16_t>(area, off::kTaMaxEntries)) return "aucune ligne prevue";
    if (field<uint16_t>(area, off::kTaEntryCount) > field<uint16_t>(area, off::kTaMaxEntries)) return "plus de lignes ecrites que prevues";
    if (shape.known && shape.area == area && field<void*>(area, off::kTaFontFamily) != shape.family) return "police changee";
    return nullptr;
}

void GuardTextAreas() {
    for (int i = 0; i < kAreaCount; i++) {
        void*& area = *reinterpret_cast<void**>(addr::kTextAreas[i]);
        AreaShape& shape = g_shapes[i];
        if (!area) continue;
        const char* fault = AreaFault(area, shape);
        if (!fault) {
            shape.known = true;
            shape.area = area;
            shape.family = field<void*>(area, off::kTaFontFamily);
            shape.alignment = field<uint16_t>(area, off::kTaAlignment);
            shape.fontSize = field<uint16_t>(area, off::kTaFontSize);
            shape.maxEntries = field<uint16_t>(area, off::kTaMaxEntries);
            shape.formatting = field<uint8_t>(area, off::kTaFormatting);
            shape.scaleFont = field<uint8_t>(area, off::kTaScaleFont);
            shape.bottomJustify = field<uint8_t>(area, off::kTaBottomJustify);
            shape.displayCompletion = field<uint8_t>(area, off::kTaDisplayCompletion);
            shape.fadeOverBounds = field<uint8_t>(area, off::kTaFadeOverBounds);
            memcpy(shape.bounds, &field<int16_t>(area, off::kTaBounds), sizeof(shape.bounds));
            continue;
        }

        bool readable = !IsBadReadPtr(area, off::kTaSize);
        if (g_areaRepairs < 24) {
            char bytes[3 * off::kTaSize + 1] = "";
            for (size_t b = 0; readable && b < off::kTaSize; b++) {
                snprintf(bytes + 3 * b, 4, "%02X ", static_cast<unsigned>(field<uint8_t>(area, b)));
            }
            LOG("zone de texte '%s' du moteur abimee (%s) a %p, %s : octets %s", kAreaNames[i], fault, area,
                shape.known && shape.area == area ? "saine a l'image precedente" : "jamais vue saine", bytes);
        }
        g_areaRepairs++;

        void* fresh = nullptr;
        if (shape.known && shape.family && shape.maxEntries && g_areaFailures[i] < kMaxAreaFailures) {
            fn::TextAreaNew(&fresh, shape.bounds, 0, shape.maxEntries, shape.family, shape.alignment, shape.formatting,
                            shape.fontSize, shape.scaleFont, shape.bottomJustify, shape.displayCompletion,
                            shape.fadeOverBounds);
        }
        if (fresh && !AreaFault(fresh, AreaShape())) {
            area = fresh;
            shape.area = fresh;
            if (g_areaRepairs <= 24) LOG("zone de texte '%s' refaite a neuf a %p (%u ligne(s))", kAreaNames[i], fresh,
                                         static_cast<unsigned>(shape.maxEntries));
        } else {
            if (fresh) g_areaFailures[i]++;
            // Rien pour la refaire : le moteur saute une zone absente quand il écrit et quand il
            // libère, mais pas partout quand il trace. Vide, au moins, elle ne trace rien.
            if (readable) {
                field<uint16_t>(area, off::kTaEntryCount) = 0;
                field<uint16_t>(area, off::kTaMaxEntries) = 0;
            }
            if (g_areaRepairs <= 24) LOG("zone de texte '%s' : impossible de la refaire, videe", kAreaNames[i]);
        }
    }
}

static void(__cdecl* g_levelMessages)() = nullptr;

static void __cdecl ONIMP_ENTRY LevelMessagesHook() {
    GuardTextAreas();
    g_levelMessages();
}

bool InstallTextAreaGuard() {
    if (CallTarget(addr::kLevelMessagesCallSite) != reinterpret_cast<void*>(addr::kLevelMessagesTarget)) return false;
    void* previous = nullptr;
    if (!PatchCall(addr::kLevelMessagesCallSite, reinterpret_cast<void*>(&LevelMessagesHook), &previous)) return false;
    g_levelMessages = reinterpret_cast<void(__cdecl*)()>(previous);
    return true;
}

// -------------------------------------------------------------------------------------------------
// Fermeture du jeu
// -------------------------------------------------------------------------------------------------
// À sa fermeture le moteur libère ses zones de texte une à une, et pour chacune ses lignes, sans
// regarder s'il y en a. Or sa libération lit quatre octets avant le pointeur : une zone restée sans
// lignes faisait planter le jeu en se fermant (journal d'un client de la 0.23 : « EXCEPTION C0000005
// a 00424b30, acces fffffffc », appelée de 0x42FAF9 pour la zone des messages du niveau). Une telle
// zone est écartée avant que le moteur n'y passe, et le journal dit dans quel état étaient les
// quatre.
static void(__cdecl* g_consoleTerminate)() = nullptr;

static void __cdecl ONIMP_ENTRY ConsoleTerminateHook() {
    static const char* const kNames[] = {"commande", "console", "messages", "sous-titres"};
    char line[200] = "";
    int length = 0;
    for (size_t i = 0; i < sizeof(addr::kTextAreas) / sizeof(addr::kTextAreas[0]); i++) {
        void*& area = *reinterpret_cast<void**>(addr::kTextAreas[i]);
        int written = 0;
        if (!area) {
            written = snprintf(line + length, sizeof(line) - static_cast<size_t>(length), " %s absente,", kNames[i]);
        } else if (IsBadReadPtr(area, off::kTaEntries + sizeof(void*)) || !field<void*>(area, off::kTaEntries)) {
            written = snprintf(line + length, sizeof(line) - static_cast<size_t>(length), " %s SANS LIGNES (ecartee),", kNames[i]);
            area = nullptr;  // perdue : le jeu se ferme
        } else {
            written = snprintf(line + length, sizeof(line) - static_cast<size_t>(length), " %s %u/%u,", kNames[i],
                               static_cast<unsigned>(field<uint16_t>(area, off::kTaEntryCount)),
                               static_cast<unsigned>(field<uint16_t>(area, off::kTaMaxEntries)));
        }
        if (written > 0 && written < static_cast<int>(sizeof(line)) - length) length += written;
    }
    LOG("fermeture : le jeu se ferme ; zones de texte :%s", line);
    g_consoleTerminate();
}

bool InstallExitGuard() {
    if (CallTarget(addr::kConsoleTerminateCallSite) != reinterpret_cast<void*>(addr::kConsoleTerminateTarget)) return false;
    void* previous = nullptr;
    if (!PatchCall(addr::kConsoleTerminateCallSite, reinterpret_cast<void*>(&ConsoleTerminateHook), &previous)) return false;
    g_consoleTerminate = reinterpret_cast<void(__cdecl*)()>(previous);
    return true;
}

void Damage(Character* chr, uint32_t amount, Character* attacker, uint32_t ownerType, const Vector3* direction,
            float knockback, uint32_t impact) {
    auto takeDamage = g_takeDamage ? reinterpret_cast<fn::TakeDamageFunction>(g_takeDamage) : fn::TakeDamage;
    // Sans attaquant : mêmes arguments qu'AI2rSmite, qui passe par cette fonction pour tuer.
    uint32_t owner = attacker ? (ownerType | Handle(attacker)) : fn::kOwnerActOfGod;
    uint32_t knockbackBits;
    memcpy(&knockbackBits, &knockback, sizeof(knockbackBits));
    // Le moteur ne donne jamais de poussée sans point d'impact : on lui en fournit un.
    Vector3 location = Location(chr);
    location.y += 10.0f;
    takeDamage(chr, amount, knockbackBits, direction ? &location : nullptr, direction, owner, impact);
}

void Kill(Character* chr) {
    int32_t health = Health(chr);
    Damage(chr, health > 0 ? static_cast<uint32_t>(health) : 1u);
}

static int g_execDepth = 0;

bool InsideExec() { return g_execDepth > 0; }

bool Exec(const char* format, ...) {
    char line[256];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    LOG("bsl : %s", line);

    // Le moteur range chaque ligne exécutée dans l'historique de sa console, et cet historique
    // déborde d'un cran une fois plein (voir addr::kCommandHistory). Or un client exécute une ligne
    // toutes les cinq secondes : au bout de vingt minutes, chacune écrasait la zone des messages du
    // niveau. Nos lignes n'ont rien à faire dans l'historique du joueur : la case où celle-ci
    // s'écrit est sauvée puis rendue, les compteurs aussi, et elle s'écrit toujours dans le tableau.
    int32_t& count = *reinterpret_cast<int32_t*>(addr::kHistoryCount);
    int32_t& last = *reinterpret_cast<int32_t*>(addr::kHistoryLast);
    int32_t savedCount = count, savedLast = last;
    int32_t slot = savedCount < 0 ? 0 : (savedCount >= addr::kHistoryCapacity ? addr::kHistoryCapacity - 1 : savedCount);
    uint8_t* entry = reinterpret_cast<uint8_t*>(addr::kCommandHistory) + static_cast<size_t>(slot) * addr::kHistoryLineSize;
    static uint8_t saved[addr::kHistoryLineSize];
    if (g_execDepth == 0) {
        memcpy(saved, entry, sizeof(saved));
        count = slot;
    }
    g_execDepth++;
    bool ok = fn::CommandExecute(line) != 0;
    g_execDepth--;
    if (g_execDepth == 0) {
        memcpy(entry, saved, sizeof(saved));
        count = savedCount;
        last = savedLast;
    }
    return ok;
}

bool FixCommandHistory() {
    void* limit = reinterpret_cast<void*>(static_cast<uintptr_t>(addr::kHistoryCapacity));
    void* fixed = reinterpret_cast<void*>(static_cast<uintptr_t>(addr::kHistoryCapacity - 1));
    // Les deux ou aucune : l'une sans l'autre décalerait l'historique sans l'arrêter au bon rang.
    if (*reinterpret_cast<void**>(addr::kHistoryLimitA) != limit || *reinterpret_cast<void**>(addr::kHistoryLimitB) != limit) {
        return false;
    }
    return PatchPointer(addr::kHistoryLimitA, limit, fixed) && PatchPointer(addr::kHistoryLimitB, limit, fixed);
}

typedef int16_t(ONICALL* ConsoleActivateFunction)(void* console, Character* user);
static void* g_consoleActivate = nullptr;  // fonction d'origine, une fois détournée
static void (*g_consoleNotify)(void* console, Character* user) = nullptr;

static int16_t ONICALL ONIMP_ENTRY ConsoleActivateHook(void* console, Character* user) {
    if (g_consoleNotify && console) g_consoleNotify(console, user);
    return reinterpret_cast<ConsoleActivateFunction>(g_consoleActivate)(console, user);
}

bool InstallConsoleHook(void (*notify)(void* console, Character* user)) {
    g_consoleNotify = notify;
    return HookEntry(addr::kConsoleActivate, addr::kConsoleActivateEntry, sizeof(addr::kConsoleActivateEntry),
                     reinterpret_cast<void*>(&ConsoleActivateHook), &g_consoleActivate);
}

bool InCutscene() {
    void* state = Game();
    return state && field<uint8_t>(state, off::kGsInCutscene) != 0;
}

bool LockActive(Character* chr, bool locked) {
    uint32_t& flags = field<uint32_t>(chr, off::kChFlags);
    if (!locked) {
        flags &= ~off::kChFlagActiveLock;
        return true;
    }
    if (flags & off::kChFlagActiveLock) return true;
    if (!fn::ForceActive(chr)) return false;
    flags |= off::kChFlagActiveLock;
    return true;
}

typedef uint8_t(ONICALL* VisibleFunction)(Character* chr, void* view);
static void* g_visible = nullptr;  // fonction d'origine, une fois détournée
static bool (*g_hidden)(Character* chr) = nullptr;

static uint8_t ONICALL ONIMP_ENTRY VisibleHook(Character* chr, void* view) {
    if (g_hidden && chr && g_hidden(chr)) return 0;
    return reinterpret_cast<VisibleFunction>(g_visible)(chr, view);
}

bool InstallVisibilityHook(bool (*hidden)(Character* chr)) {
    if (CallTarget(addr::kVisibleCallSite) != reinterpret_cast<void*>(addr::kVisibleTarget)) return false;
    g_hidden = hidden;
    return PatchCall(addr::kVisibleCallSite, reinterpret_cast<void*>(&VisibleHook), &g_visible);
}

// Les arguments passent tels que le moteur les empile : chacun sur quatre octets, dont seul le
// premier (ou les deux premiers) compte pour lui. Seul l'octet de la place est changé.
typedef uint32_t(ONICALL* InsertStartFunction)(const char* name, uint32_t width, uint32_t height, uint32_t start,
                                               uint32_t end, float velocity, uint32_t mirror);
typedef uint32_t(ONICALL* InsertStopFunction)(const char* name, uint32_t end, float velocity);
static void* g_insertStart = nullptr;
static void* g_insertStop = nullptr;
static uint8_t (*g_insertPlace)(uint8_t code) = nullptr;

static uint32_t Placed(uint32_t code) {
    return g_insertPlace ? (code & ~0xFFu) | g_insertPlace(static_cast<uint8_t>(code & 0xFF)) : code;
}

static uint32_t ONICALL ONIMP_ENTRY InsertStartHook(const char* name, uint32_t width, uint32_t height, uint32_t start,
                                                    uint32_t end, float velocity, uint32_t mirror) {
    return reinterpret_cast<InsertStartFunction>(g_insertStart)(name, width, height, Placed(start), Placed(end),
                                                                 velocity, mirror);
}

static uint32_t ONICALL ONIMP_ENTRY InsertStopHook(const char* name, uint32_t end, float velocity) {
    return reinterpret_cast<InsertStopFunction>(g_insertStop)(name, Placed(end), velocity);
}

bool InstallInsertHook(uint8_t (*place)(uint8_t code)) {
    // Les deux ou aucun : un portrait arrivé en bas doit repartir par le bas.
    if (CallTarget(addr::kInsertStartCallSite) != reinterpret_cast<void*>(addr::kInsertStartTarget) ||
        CallTarget(addr::kInsertStopCallSite) != reinterpret_cast<void*>(addr::kInsertStopTarget)) {
        return false;
    }
    g_insertPlace = place;
    return PatchCall(addr::kInsertStartCallSite, reinterpret_cast<void*>(&InsertStartHook), &g_insertStart) &&
           PatchCall(addr::kInsertStopCallSite, reinterpret_cast<void*>(&InsertStopHook), &g_insertStop);
}

typedef void(ONICALL* PlayerInputFunction)(Character* player, GameInput* input);
static void* g_playerInput = nullptr;
static void (*g_inputFilter)(Character* player, GameInput& input) = nullptr;

static void ONICALL ONIMP_ENTRY PlayerInputHook(Character* player, GameInput* input) {
    // Chaque tick, avant que le moteur ne déplace le joueur : une position invalide née au tick
    // précédent n'en traverse pas un second.
    if (player && player == Player()) GuardPlayerTransform("au tick");
    if (g_inputFilter && player && input) g_inputFilter(player, *input);
    reinterpret_cast<PlayerInputFunction>(g_playerInput)(player, input);
}

bool InstallPlayerInputHook(void (*filter)(Character* player, GameInput& input)) {
    if (CallTarget(addr::kPlayerInputCallSite) != reinterpret_cast<void*>(addr::kPlayerInputTarget)) return false;
    g_inputFilter = filter;
    return PatchCall(addr::kPlayerInputCallSite, reinterpret_cast<void*>(&PlayerInputHook), &g_playerInput);
}

typedef void(ONICALL* CharacterInputFunction)(Character* chr, ActiveCharacter* active);
typedef void(ONICALL* CharacterStepFunction)(Character* chr);
static void* g_characterInput = nullptr;
static void* g_characterStep = nullptr;
static void (*g_stepFilter)(Character* chr, StepPhase phase) = nullptr;

static void ONICALL ONIMP_ENTRY CharacterInputHook(Character* chr, ActiveCharacter* active) {
    if (g_stepFilter && chr) g_stepFilter(chr, StepPhase::BeforeInput);
    reinterpret_cast<CharacterInputFunction>(g_characterInput)(chr, active);
    if (g_stepFilter && chr) g_stepFilter(chr, StepPhase::AfterInput);
}

static void ONICALL ONIMP_ENTRY CharacterStepHook(Character* chr) {
    if (g_stepFilter && chr) g_stepFilter(chr, StepPhase::BeforeStep);
    reinterpret_cast<CharacterStepFunction>(g_characterStep)(chr);
    if (g_stepFilter && chr) g_stepFilter(chr, StepPhase::AfterStep);
}

bool InstallStepHook(void (*filter)(Character* chr, StepPhase phase)) {
    // Les deux ou aucun : un saut réglé au départ doit l'être aussi dans la poussée tenue.
    if (CallTarget(addr::kCharacterInputCallSite) != reinterpret_cast<void*>(addr::kCharacterInputTarget) ||
        CallTarget(addr::kCharacterStepCallSite) != reinterpret_cast<void*>(addr::kCharacterStepTarget)) {
        return false;
    }
    g_stepFilter = filter;
    return PatchCall(addr::kCharacterInputCallSite, reinterpret_cast<void*>(&CharacterInputHook), &g_characterInput) &&
           PatchCall(addr::kCharacterStepCallSite, reinterpret_cast<void*>(&CharacterStepHook), &g_characterStep);
}

int AnimAttacks(void* tram) { return tram ? field<uint8_t>(tram, off::kTramAttackCount) : 0; }

// -------------------------------------------------------------------------------------------------
// Lasers, tourelles, alarmes
// -------------------------------------------------------------------------------------------------
typedef uint8_t(ONICALL* LaserTestFunction)(Character* chr, void* origin, uint32_t a, uint32_t b, uint32_t c,
                                            uint32_t d, uint32_t e, uint32_t f);
static void* g_laserTest = nullptr;
static int (*g_laserCandidates)(Character* player, Character** out, int capacity) = nullptr;
static Character* g_laserHit = nullptr;
static int32_t g_laserHitAt = 0;

// Le moteur demande si le rayon coupe le joueur de la partie. La même question est posée, avec les
// mêmes arguments, pour chacun des personnages que l'on nous désigne, jusqu'au premier que le rayon
// coupe.
static uint8_t ONICALL ONIMP_ENTRY LaserTestHook(Character* chr, void* origin, uint32_t a, uint32_t b, uint32_t c,
                                                 uint32_t d, uint32_t e, uint32_t f) {
    auto test = reinterpret_cast<LaserTestFunction>(g_laserTest);
    Character* list[8];
    int count = g_laserCandidates ? g_laserCandidates(chr, list, 8) : -1;
    if (count < 0) return test(chr, origin, a, b, c, d, e, f);
    for (int i = 0; i < count && i < 8; i++) {
        if (!list[i] || !InUse(list[i]) || !test(list[i], origin, a, b, c, d, e, f)) continue;
        g_laserHit = list[i];
        g_laserHitAt = GameTime();
        return 1;
    }
    return 0;
}

bool InstallLaserHook(int (*candidates)(Character* player, Character** out, int capacity)) {
    if (CallTarget(addr::kLaserTestCallSite) != reinterpret_cast<void*>(addr::kLaserTestTarget)) return false;
    g_laserCandidates = candidates;
    return PatchCall(addr::kLaserTestCallSite, reinterpret_cast<void*>(&LaserTestHook), &g_laserTest);
}

Character* LastLaserHit(int32_t* gameTime) {
    if (gameTime) *gameTime = g_laserHitAt;
    return g_laserHit && InUse(g_laserHit) ? g_laserHit : nullptr;
}

typedef uint32_t(ONICALL* AlarmFunction)(uint32_t alarm, Character* cause);
static void* g_alarm = nullptr;
static void (*g_alarmTripped)(uint8_t alarm, Character* cause) = nullptr;

static uint32_t ONICALL ONIMP_ENTRY AlarmHook(uint32_t alarm, Character* cause) {
    uint32_t result = reinterpret_cast<AlarmFunction>(g_alarm)(alarm, cause);
    if (g_alarmTripped) g_alarmTripped(static_cast<uint8_t>(alarm & 0xFF), cause);
    return result;
}

bool InstallAlarmHook(void (*tripped)(uint8_t alarm, Character* cause)) {
    g_alarmTripped = tripped;
    return HookEntry(addr::kAlarmTarget, addr::kAlarmEntry, sizeof(addr::kAlarmEntry), reinterpret_cast<void*>(&AlarmHook),
                     &g_alarm);
}

typedef uint16_t(ONICALL* EventsFunction)(void* events, Character* cause);
static void* g_laserEvents = nullptr;
static bool (*g_laserAllowed)(void* laser, Character* cause) = nullptr;
static void (*g_laserTripped)(void* laser, Character* cause) = nullptr;

// Le moteur ne lit pas ce que rendent les événements d'un laser (0x4CBB0F).
static uint16_t ONICALL ONIMP_ENTRY LaserEventsHook(void* events, Character* cause) {
    void* laser = events ? static_cast<uint8_t*>(events) - off::kTrigEvents : nullptr;
    if (laser && g_laserAllowed && !g_laserAllowed(laser, cause)) return 0;
    uint16_t result = reinterpret_cast<EventsFunction>(g_laserEvents)(events, cause);
    if (laser && g_laserTripped) g_laserTripped(laser, cause);
    return result;
}

bool InstallLaserTripHook(bool (*allowed)(void* laser, Character* cause), void (*tripped)(void* laser, Character* cause)) {
    if (CallTarget(addr::kLaserEventsCallSite) != reinterpret_cast<void*>(addr::kEventsRun)) return false;
    g_laserAllowed = allowed;
    g_laserTripped = tripped;
    return PatchCall(addr::kLaserEventsCallSite, reinterpret_cast<void*>(&LaserEventsHook), &g_laserEvents);
}

bool PlayLaserSound(void* laser) {
    if (!laser) return false;
    void* laserClass = field<void*>(laser, off::kTrigClass);
    void* sound = laserClass ? field<void*>(laserClass, off::kTrigClassHitSound) : nullptr;
    Vector3 position = field<Vector3>(laser, off::kObjPosition);
    if (!sound || !Finite(position)) return false;
    fn::SoundPlayAt(sound, &position, 0, 0, 0);
    return true;
}

void RunLaserEvents(void* laser, Character* cause) {
    if (laser) fn::RunEvents(static_cast<uint8_t*>(laser) + off::kTrigEvents, cause);
}

int LaserTurrets(void* laser, uint16_t* out, int capacity) {
    if (!laser) return 0;
    uint8_t* events = static_cast<uint8_t*>(laser) + off::kTrigEvents;
    unsigned count = field<uint16_t>(events, off::kEventCount);
    uint8_t* items = field<uint8_t*>(events, off::kEventItems);
    int found = 0;
    for (unsigned i = 0; items && i < count && i < 32 && found < capacity; i++) {
        uint8_t* event = items + i * off::kEventSize;
        if (field<uint32_t>(event, off::kEventType) == off::kEventActivateTurret) out[found++] = field<uint16_t>(event, off::kEventId);
    }
    return found;
}

int LaserRailFrames(void* laser) {
    void* laserClass = laser ? field<void*>(laser, off::kTrigClass) : nullptr;
    void* rail = laserClass ? field<void*>(laserClass, off::kTrigClassRail) : nullptr;
    return rail ? field<uint16_t>(rail, off::kRailFrames) : 0;
}

float LaserSpeed(void* laser) { return laser ? field<float>(laser, off::kTrigSpeed) : 0.0f; }

bool LaserLoops(void* laser) { return laser && !(field<uint16_t>(laser, off::kTrigFlags) & off::kTrigFlagBounce); }

// Les rayons d'un laser qui bouge : il lui faut un rail, une vitesse, et sa table de rayons.
static uint8_t* MovingRays(void* laser, int* count) {
    if (!laser || LaserRailFrames(laser) < 2) return nullptr;
    float speed = LaserSpeed(laser);
    if (!std::isfinite(speed) || speed == 0.0f) return nullptr;
    *count = field<uint16_t>(laser, off::kTrigRayCount);
    return *count > 0 ? field<uint8_t*>(laser, off::kTrigRays) : nullptr;
}

int LaserRays(void* laser, float* frames, int8_t* directions, int capacity) {
    int count = 0;
    uint8_t* rays = MovingRays(laser, &count);
    if (!rays || count > capacity) return 0;
    float rail = static_cast<float>(LaserRailFrames(laser));
    for (int i = 0; i < count; i++) {
        uint8_t* ray = rays + i * off::kRaySize;
        float frame = field<float>(ray, off::kRayFrame);
        int16_t direction = field<int16_t>(ray, off::kRayDirection);
        // Ce que le moteur y tient toujours : une image du rail, et un sens. Autre chose, et ce
        // laser n'est pas fait comme attendu : on n'y touche pas.
        if (!(frame >= 0.0f && frame <= rail) || (direction != 1 && direction != -1)) return 0;
        frames[i] = frame;
        directions[i] = static_cast<int8_t>(direction);
    }
    return count;
}

void SetLaserRays(void* laser, const float* frames, const int8_t* directions, int count) {
    int own = 0;
    uint8_t* rays = MovingRays(laser, &own);
    if (!rays || own != count) return;
    float last = static_cast<float>(LaserRailFrames(laser) - 1);
    for (int i = 0; i < count; i++) {
        if (!std::isfinite(frames[i])) continue;
        uint8_t* ray = rays + i * off::kRaySize;
        field<float>(ray, off::kRayFrame) = frames[i] < 0.0f ? 0.0f : frames[i] > last ? last : frames[i];
        field<int16_t>(ray, off::kRayDirection) = directions[i] < 0 ? -1 : 1;
    }
}

bool TurretCanShoot(void* turret, Character* target) {
    if (!turret || !target) return false;
    Character*& slot = field<Character*>(turret, off::kTurretTarget);
    Character* kept = slot;
    slot = target;
    bool can = fn::TurretCanShoot(turret) != 0;
    slot = kept;
    return can;
}

struct DefenseScan {
    Defense* out;
    int capacity;
    int count;
    DefenseKind kind;
};

static uint8_t ONICALL ONIMP_ENTRY ScanDefense(void* object, void* userData) {
    DefenseScan* scan = static_cast<DefenseScan*>(userData);
    bool laser = scan->kind == DefenseKind::Laser;
    uint16_t id = field<uint16_t>(object, laser ? off::kTrigId : off::kTurretId);
    uint16_t flags = laser ? field<uint16_t>(object, off::kTrigFlags) : 0;
    bool active = laser ? (flags & off::kTrigFlagActive) != 0 : field<uint16_t>(object, off::kTurretState) != 0;
    bool hit = laser && (flags & off::kTrigFlagHit) != 0;
    for (int i = 0; i < scan->count; i++) {
        Defense& known = scan->out[i];
        if (known.kind != scan->kind || known.id != id) continue;
        known.active = known.active || active;
        known.hit = known.hit || hit;
        return 1;
    }
    if (scan->count < scan->capacity) scan->out[scan->count++] = {scan->kind, id, active, hit};
    return 1;  // continuer
}

int ReadDefenses(Defense* out, int capacity) {
    DefenseScan scan = {out, capacity, 0, DefenseKind::Laser};
    if (!Game() || !LevelHash()) return 0;
    fn::EnumerateObjects(Tag('T', 'R', 'I', 'G'), ScanDefense, &scan);
    scan.kind = DefenseKind::Turret;
    fn::EnumerateObjects(Tag('T', 'U', 'R', 'R'), ScanDefense, &scan);
    return scan.count;
}

void SetDefense(DefenseKind kind, uint16_t id, bool active) {
    if (id == 0xFFFF) return;  // ce numéro désigne « tous » pour le moteur
    if (kind == DefenseKind::Laser) {
        if (active) fn::TriggerActivate(id);
        else fn::TriggerDeactivate(id);
    } else if (active) {
        fn::TurretActivate(id);
    } else {
        fn::TurretDeactivate(id);
    }
}

typedef Character*(ONICALL* TurretTargetFunction)(void* turret);
static void* g_turretTarget = nullptr;
static Character* (*g_turretChoose)(void* turret, Character* engineChoice) = nullptr;

static Character* ONICALL ONIMP_ENTRY TurretTargetHook(void* turret) {
    Character* chosen = reinterpret_cast<TurretTargetFunction>(g_turretTarget)(turret);
    return g_turretChoose && turret ? g_turretChoose(turret, chosen) : chosen;
}

bool InstallTurretTargetHook(Character* (*choose)(void* turret, Character* engineChoice)) {
    if (CallTarget(addr::kTurretTargetCallSite) != reinterpret_cast<void*>(addr::kTurretTargetTarget)) return false;
    g_turretChoose = choose;
    return PatchCall(addr::kTurretTargetCallSite, reinterpret_cast<void*>(&TurretTargetHook), &g_turretTarget);
}

bool InstallTurretFireFlag(uint8_t* flag) {
    if (!flag || *reinterpret_cast<const uint8_t*>(addr::kTurretPassiveRead) != 0xA0) return false;
    return PatchPointer(addr::kTurretPassiveOperand, reinterpret_cast<void*>(addr::kAiAllPassive), flag);
}

bool AiAllPassive() { return *reinterpret_cast<const uint8_t*>(addr::kAiAllPassive) != 0; }


struct ObjectList {
    void** out;
    int capacity;
    int count;
};

static uint8_t ONICALL ONIMP_ENTRY ListObject(void* object, void* userData) {
    ObjectList* list = static_cast<ObjectList*>(userData);
    if (list->count < list->capacity) list->out[list->count++] = object;
    return 1;  // continuer
}

int Lasers(void** out, int capacity) {
    ObjectList list = {out, capacity, 0};
    if (!Game() || !LevelHash()) return 0;
    fn::EnumerateObjects(Tag('T', 'R', 'I', 'G'), ListObject, &list);
    return list.count;
}

int Turrets(void** out, int capacity) {
    ObjectList list = {out, capacity, 0};
    if (!Game() || !LevelHash()) return 0;
    fn::EnumerateObjects(Tag('T', 'U', 'R', 'R'), ListObject, &list);
    return list.count;
}

int LivingCharacters(Character** out, int capacity) {
    GameState* gs = Game();
    if (!gs) return 0;
    uint32_t count = field<uint32_t>(gs, off::kGsLivingCount);
    if (count > static_cast<uint32_t>(off::kMaxCharacters)) return 0;
    int kept = 0;
    for (uint32_t i = 0; i < count && kept < capacity; i++) {
        Character* chr = field<Character*>(gs, off::kGsLiving + i * sizeof(Character*));
        if (chr) out[kept++] = chr;
    }
    return kept;
}

uint16_t LevelNumber() {
    GameState* gs = Game();
    return gs ? field<uint16_t>(gs, off::kGsLevelNumber) : 0;
}

void LoadLevel(uint16_t number) {
    GameState* gs = Game();
    if (!gs || !number) return;
    field<uint16_t>(gs, off::kGsLevelNumber) = number;
    RestartLevel();
}

// La borne de six devient douze aux six endroits où le moteur l'a écrite. Chargeurs, cellules et
// hypos y passent par le même code : pour que les hypos restent à six, leur case de la table de
// sauts mène à quelques octets de code à nous, qui lisent leur compte, lui ajoutent six, et
// rejoignent le code commun. Six hypos y comptent alors pour douze : inventaire plein.
//   66 8B 86 A4 01 00 00    mov ax, [esi+0x1A4]      (66 8B B1 ... : mov si, [ecx+0x1A4])
//   66 83 C0 06             add ax, 6                (66 83 C6 06 : add si, 6)
//   E9 xx xx xx xx          jmp au code commun
static bool g_reservesRaised = false;

int ReserveCapacity() { return g_reservesRaised ? 12 : 6; }

bool RaiseReserveCapacity() {
    bool& raised = g_reservesRaised;
    if (raised) return true;
    for (int i = 0; i < 6; i++) {
        if (*reinterpret_cast<uint32_t*>(addr::kReserveLimits[i]) != addr::kReserveLimitCode[i]) return false;
    }
    if (*reinterpret_cast<uint32_t*>(addr::kGiveHypoEntry) != addr::kGiveHypoCase ||
        *reinterpret_cast<uint32_t*>(addr::kSearchHypoEntry) != addr::kSearchHypoCase) {
        return false;
    }
    static const uint8_t kGiveStub[11] = {0x66, 0x8B, 0x86, 0xA4, 0x01, 0x00, 0x00, 0x66, 0x83, 0xC0, 0x06};
    static const uint8_t kSearchStub[11] = {0x66, 0x8B, 0xB1, 0xA4, 0x01, 0x00, 0x00, 0x66, 0x83, 0xC6, 0x06};
    const size_t kStubSize = sizeof(kGiveStub) + 5;
    auto* stubs = static_cast<uint8_t*>(VirtualAlloc(nullptr, 2 * kStubSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!stubs) return false;
    const uint8_t* bodies[2] = {kGiveStub, kSearchStub};
    const uintptr_t shared[2] = {addr::kGiveShared, addr::kSearchShared};
    for (int i = 0; i < 2; i++) {
        uint8_t* stub = stubs + i * kStubSize;
        memcpy(stub, bodies[i], sizeof(kGiveStub));
        stub[sizeof(kGiveStub)] = 0xE9;
        int32_t rel = static_cast<int32_t>(shared[i] - (reinterpret_cast<uintptr_t>(stub) + kStubSize));
        memcpy(stub + sizeof(kGiveStub) + 1, &rel, sizeof(rel));
    }
    FlushInstructionCache(GetCurrentProcess(), stubs, 2 * kStubSize);

    // Les hypos d'abord : tant que leur case ne mène pas chez nous, la borne ne doit pas bouger.
    if (!PatchPointer(addr::kGiveHypoEntry, reinterpret_cast<void*>(addr::kGiveHypoCase), stubs) ||
        !PatchPointer(addr::kSearchHypoEntry, reinterpret_cast<void*>(addr::kSearchHypoCase), stubs + kStubSize)) {
        return false;
    }
    for (int i = 0; i < 6; i++) {
        uint32_t code = addr::kReserveLimitCode[i];
        PatchPointer(addr::kReserveLimits[i], reinterpret_cast<void*>(static_cast<uintptr_t>(code)),
                     reinterpret_cast<void*>(static_cast<uintptr_t>((code & ~0xFFu) | 12)));
    }
    raised = true;
    return true;
}

// -------------------------------------------------------------------------------------------------
// Messages à l'écran
// -------------------------------------------------------------------------------------------------
// Jusqu'à la 0.19 les messages du mod s'écrivaient dans les lignes de la console du moteur, en bas
// de l'écran, où passent aussi les sous-titres des dialogues du niveau. La 0.20 avait déplacé
// cette console entière en haut à droite, sous-titres compris. Ils ont maintenant leur propre zone
// de texte, du même genre que celle du moteur et tracée juste après elle : la console du jeu reste
// où elle est, et telle qu'elle est.
static const uint32_t kMessageColor = 0xFF40FF40;     // vert, opaque
static const uint32_t kMessageShadow = 0xFF062006;
static const uint32_t kMessagePriority = 0x3FFFFFFF;  // au-dessus de tout seuil de la console (0x431349)
static const int kMinWidth = 320, kMinHeight = 240;   // en dessous, pas de zone
static const double kTicksPerSecond = 60.0;
static bool g_messagesShown = true;
static void* g_messageArea = nullptr;                 // jamais libérée : elle vit autant que le jeu
static double g_messageClock = 0.0;                   // dernier passage de l'usure
static double g_messageDebt = 0.0;                    // fraction de tick pas encore comptée
static void(__cdecl* g_consoleDisplay)() = nullptr;   // affichage d'origine, une fois le détour posé

void ShowMessages(bool shown) { g_messagesShown = shown; }
bool MessagesShown() { return g_messagesShown; }

// Le tracé du moteur lit le contexte de texte de la zone, et la police de ce contexte, sans les
// contrôler (0x4310A7, 0x42E244) : rien n'est tracé ni écrit tant qu'ils manquent.
static bool MessageAreaUsable() {
    void* area = g_messageArea;
    if (!area || !field<void*>(area, off::kTaEntries) || !field<uint16_t>(area, off::kTaMaxEntries)) return false;
    void* context = field<void*>(area, off::kTaContext);
    return context && field<void*>(context, off::kTcFont);
}

// La zone, créée à la première demande et replacée quand la taille de l'image change. Nul tant
// qu'elle ne peut pas servir : détour refusé, console du moteur pas encore née, pas de contexte de
// texte (le tracé le lit sans le contrôler, 0x4310A7).
static void* MessageArea() {
    if (!g_consoleDisplay) return nullptr;
    void* console = *reinterpret_cast<void**>(addr::kConsoleLines);
    if (!console) return nullptr;
    int width = fn::DrawWidth(), height = fn::DrawHeight();
    if (width < kMinWidth || height < kMinHeight) return nullptr;

    // Les deux tiers droits de l'écran, pour qu'une longue ligne ne soit pas coupée en deux ; le
    // texte y est aligné à droite. En hauteur, un sixième d'écran de lignes, et de la marge en plus
    // dessous : une ligne trop longue en occupe deux, et le moteur efface en hâte la dernière ligne
    // d'une zone qui déborde, ici la plus récente.
    int margin = height / 40;
    int16_t lines[4] = {static_cast<int16_t>(width / 3), static_cast<int16_t>(margin),
                        static_cast<int16_t>(width - margin), static_cast<int16_t>(margin + height / 6)};
    int16_t bounds[4] = {lines[0], lines[1], lines[2], static_cast<int16_t>(lines[3] + height / 10)};

    if (!g_messageArea) {
        void* family = field<void*>(console, off::kTaFontFamily);
        if (!family) return nullptr;
        // La police et la taille de la console du moteur ; lignes alignées à droite, empilées
        // depuis le haut, et qui s'estompent si elles débordent. Le moteur compte lui-même les
        // lignes que le rectangle contient et crée le contexte de texte.
        void* area = nullptr;
        fn::TextAreaNew(&area, lines, 0, -1, family, off::kTcAlignRight, 0, field<uint16_t>(console, off::kTaFontSize),
                        field<uint8_t>(console, off::kTaScaleFont), 0, 0, 1);
        if (!area) return nullptr;
        memcpy(&field<int16_t>(area, off::kTaBounds), bounds, sizeof(bounds));
        g_messageArea = area;
        LOG("messages : zone creee en haut a droite (%d x %d), %u ligne(s)", width, height,
            static_cast<unsigned>(field<uint16_t>(area, off::kTaMaxEntries)));
    } else if (memcmp(&field<int16_t>(g_messageArea, off::kTaBounds), bounds, sizeof(bounds)) != 0) {
        fn::TextAreaResize(g_messageArea, lines, -1);
        memcpy(&field<int16_t>(g_messageArea, off::kTaBounds), bounds, sizeof(bounds));
    }
    return MessageAreaUsable() ? g_messageArea : nullptr;
}

// Le moteur n'use que les lignes de ses propres zones : celles de la nôtre le sont ici, au temps
// réel, que le jeu tourne ou qu'un menu soit ouvert.
void UpdateMessages() {
    double now = clk::Seconds();
    double elapsed = now - g_messageClock;
    g_messageClock = now;
    void* area = g_messageArea;
    if (!area || elapsed <= 0.0) return;
    if (elapsed > 1.0) elapsed = 1.0;
    g_messageDebt += elapsed * kTicksPerSecond;
    int ticks = static_cast<int>(g_messageDebt);
    if (ticks <= 0) return;
    g_messageDebt -= ticks;

    uint8_t* entries = field<uint8_t*>(area, off::kTaEntries);
    uint16_t& count = field<uint16_t>(area, off::kTaEntryCount);
    if (!entries || count > field<uint16_t>(area, off::kTaMaxEntries)) return;
    int kept = 0;
    for (int i = 0; i < count; i++) {
        uint8_t* entry = entries + i * off::kTeSize;
        int32_t& timer = field<int32_t>(entry, off::kTeTimer);
        if (timer > 0 && (timer -= ticks) <= 0) continue;  // usée : elle s'en va
        if (kept != i) memmove(entries + kept * off::kTeSize, entry, off::kTeSize);
        kept++;
    }
    count = static_cast<uint16_t>(kept);
}

static void __cdecl ONIMP_ENTRY ConsoleDisplayHook() {
    g_consoleDisplay();
    if (!g_messagesShown || !MessageAreaUsable()) return;
    if (field<uint16_t>(g_messageArea, off::kTaEntryCount)) fn::TextAreaDisplay(g_messageArea);
}

bool InstallMessageHook() {
    if (CallTarget(addr::kConsoleDisplayCallSite) != reinterpret_cast<void*>(addr::kConsoleDisplayTarget)) return false;
    void* previous = nullptr;
    if (!PatchCall(addr::kConsoleDisplayCallSite, reinterpret_cast<void*>(&ConsoleDisplayHook), &previous)) return false;
    g_consoleDisplay = reinterpret_cast<void(__cdecl*)()>(previous);
    return true;
}

void ConsolePrint(const char* format, ...) {
    // Dans la langue du moment : le format et les mots qu'on lui donne sont traduits (voir lang.h).
    char text[256];
    va_list args;
    va_start(args, format);
    loc::FormatV(text, sizeof(text), format, args);
    va_end(args);
    LOG("console : %s", text);
    if (!g_messagesShown) return;

    uint32_t fade = *reinterpret_cast<uint32_t*>(addr::kConsoleFadeTime);
    if (void* area = MessageArea()) {
        fn::TextAreaPrint(area, kMessagePriority, kMessageColor, kMessageShadow, text, 0, fade);
        return;
    }
    // Pas de zone à nous : la console du moteur, dans ses couleurs, comme jusqu'à la 0.19.
    void* console = *reinterpret_cast<void**>(addr::kConsoleLines);
    if (!console) return;
    fn::TextAreaPrint(console, 1, *reinterpret_cast<uint32_t*>(addr::kConsoleColor),
                      *reinterpret_cast<uint32_t*>(addr::kConsoleShadow), text, 0, fade);
}

}  // namespace mem
