// Test du détour de fonction sans le jeu. Six fonctions __fastcall écrites en code machine, avec
// les mêmes prologues et les mêmes nombres d'arguments que celles détournées dans le moteur,
// doivent passer par le détour, rester appelables par leur tremplin (arguments, valeur de retour)
// et rendre la pile intacte. Un prologue différent de celui attendu doit être refusé sans rien
// modifier.
#include <windows.h>

#include <cstdio>
#include <cstring>

#include "../src/memory.h"
#include "../src/oni.h"

// Comme la question d'hostilité de l'IA : deux registres, un argument de pile.
typedef uint32_t(ONICALL* Sum3)(uint32_t a, uint32_t b, uint32_t c);
//   53 55 56        push ebx ; push ebp ; push esi
//   8B F1           mov esi, ecx
//   8B C6           mov eax, esi
//   03 C2           add eax, edx
//   03 44 24 10     add eax, [esp+0x10]
//   5E 5D 5B        pop esi ; pop ebp ; pop ebx
//   C2 04 00        ret 4
static const uint8_t kSum3[] = {0x53, 0x55, 0x56, 0x8B, 0xF1, 0x8B, 0xC6, 0x03, 0xC2, 0x03,
                                0x44, 0x24, 0x10, 0x5E, 0x5D, 0x5B, 0xC2, 0x04, 0x00};

// Comme la fonction de dégâts : deux registres, cinq arguments de pile.
typedef uint32_t(ONICALL* Sum7)(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f, uint32_t g);
//   83 EC 2C        sub esp, 0x2C
//   53 55           push ebx ; push ebp
//   8B C1           mov eax, ecx
//   03 C2           add eax, edx
//   03 44 24 38     add eax, [esp+0x38]     (puis 0x3C, 0x40, 0x44, 0x48)
//   5D 5B           pop ebp ; pop ebx
//   83 C4 2C        add esp, 0x2C
//   C2 14 00        ret 0x14
static const uint8_t kSum7[] = {0x83, 0xEC, 0x2C, 0x53, 0x55, 0x8B, 0xC1, 0x03, 0xC2, 0x03, 0x44, 0x24,
                                0x38, 0x03, 0x44, 0x24, 0x3C, 0x03, 0x44, 0x24, 0x40, 0x03, 0x44, 0x24,
                                0x44, 0x03, 0x44, 0x24, 0x48, 0x5D, 0x5B, 0x83, 0xC4, 0x2C, 0xC2, 0x14, 0x00};

// Comme l'imprécision d'un tireur : un registre, résultat flottant, prologue de six octets (le
// saut du détour en prend cinq, le sixième devient un nop).
typedef float(ONICALL* ToFloat)(uint32_t a);
//   51 56           push ecx ; push esi
//   8B F1           mov esi, ecx
//   8B C6           mov eax, esi
//   89 44 24 04     mov [esp+4], eax
//   DB 44 24 04     fild dword ptr [esp+4]
//   5E 59           pop esi ; pop ecx
//   C3              ret
static const uint8_t kToFloat[] = {0x51, 0x56, 0x8B, 0xF1, 0x8B, 0xC6, 0x89, 0x44, 0x24,
                                   0x04, 0xDB, 0x44, 0x24, 0x04, 0x5E, 0x59, 0xC3};

// Comme la pose d'un personnage : deux registres, aucun argument de pile.
typedef uint32_t(ONICALL* Sum2)(uint32_t a, uint32_t b);
//   83 EC 14        sub esp, 0x14
//   53 55           push ebx ; push ebp
//   8B C1           mov eax, ecx
//   03 C2           add eax, edx
//   5D 5B           pop ebp ; pop ebx
//   83 C4 14        add esp, 0x14
//   C3              ret
static const uint8_t kSum2[] = {0x83, 0xEC, 0x14, 0x53, 0x55, 0x8B, 0xC1, 0x03, 0xC2, 0x5D, 0x5B, 0x83, 0xC4, 0x14, 0xC3};

// Comme la comparaison de deux contacts : deux registres, deux arguments de pile.
typedef uint32_t(ONICALL* Sum4)(uint32_t a, uint32_t b, uint32_t c, uint32_t d);
//   51 53 55 56 57  push ecx ; push ebx ; push ebp ; push esi ; push edi
//   8B C1           mov eax, ecx
//   03 C2           add eax, edx
//   03 44 24 18     add eax, [esp+0x18]
//   03 44 24 1C     add eax, [esp+0x1C]
//   5F 5E 5D 5B 59  pop edi ; pop esi ; pop ebp ; pop ebx ; pop ecx
//   C2 08 00        ret 8
static const uint8_t kSum4[] = {0x51, 0x53, 0x55, 0x56, 0x57, 0x8B, 0xC1, 0x03, 0xC2, 0x03, 0x44, 0x24, 0x18,
                                0x03, 0x44, 0x24, 0x1C, 0x5F, 0x5E, 0x5D, 0x5B, 0x59, 0xC2, 0x08, 0x00};

// Comme la commande de script text_console : deux registres, quatre arguments de pile, et un
// prologue dont la comparaison n'est lue qu'après le saut du détour, par le « je » resté en place.
typedef uint32_t(ONICALL* Pick6)(uint32_t a, uint32_t count, uint32_t c, uint32_t d, uint32_t e, uint32_t f);
//   83 EC 08        sub esp, 8
//   83 FA 01        cmp edx, 1
//   57              push edi
//   74 0C           je +12
//   B8 01 00 00 00  mov eax, 1              (mauvais nombre d'arguments)
//   5F              pop edi
//   83 C4 08        add esp, 8
//   C2 10 00        ret 0x10
//   8B 44 24 10     mov eax, [esp+0x10]
//   03 44 24 1C     add eax, [esp+0x1C]
//   03 C1           add eax, ecx
//   5F              pop edi
//   83 C4 08        add esp, 8
//   C2 10 00        ret 0x10
static const uint8_t kPick6[] = {0x83, 0xEC, 0x08, 0x83, 0xFA, 0x01, 0x57, 0x74, 0x0C, 0xB8, 0x01, 0x00, 0x00,
                                 0x00, 0x5F, 0x83, 0xC4, 0x08, 0xC2, 0x10, 0x00, 0x8B, 0x44, 0x24, 0x10, 0x03,
                                 0x44, 0x24, 0x1C, 0x03, 0xC1, 0x5F, 0x83, 0xC4, 0x08, 0xC2, 0x10, 0x00};

static void* g_original2 = nullptr;
static void* g_original4 = nullptr;
static void* g_original6 = nullptr;
static int g_calls6 = 0;

// Comme le détour de l'écran de texte : en session la demande est notée sans appeler la fonction
// d'origine ; sinon elle y va telle quelle.
static uint32_t ONICALL ONIMP_ENTRY Hook6(uint32_t a, uint32_t count, uint32_t c, uint32_t d, uint32_t e, uint32_t f) {
    g_calls6++;
    if (c & 1) return 0u;
    return reinterpret_cast<Pick6>(g_original6)(a, count, c, d, e, f);
}
static int g_calls2 = 0;
static int g_calls4 = 0;

// Comme le détour de la pose : un état est modifié, puis la fonction d'origine est appelée.
static uint32_t ONICALL ONIMP_ENTRY Hook2(uint32_t a, uint32_t b) {
    g_calls2++;
    return reinterpret_cast<Sum2>(g_original2)(a, b + 5u);
}

// Comme le détour de la comparaison : certaines réponses sont données sans la fonction d'origine.
static uint32_t ONICALL ONIMP_ENTRY Hook4(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    g_calls4++;
    if (c & 1) return 77u;
    return reinterpret_cast<Sum4>(g_original4)(a, b, c, d);
}

static void* g_original3 = nullptr;
static void* g_original7 = nullptr;
static void* g_originalFloat = nullptr;
static int g_calls3 = 0;
static int g_calls7 = 0;
static int g_callsFloat = 0;

// Comme le détour de l'imprécision : certains appels ne vont pas jusqu'à la fonction d'origine.
static float ONICALL ONIMP_ENTRY HookFloat(uint32_t a) {
    g_callsFloat++;
    if (a & 1) return 0.0f;
    return reinterpret_cast<ToFloat>(g_originalFloat)(a);
}

static uint32_t ONICALL ONIMP_ENTRY Hook3(uint32_t a, uint32_t b, uint32_t c) {
    g_calls3++;
    return reinterpret_cast<Sum3>(g_original3)(a, b, c) + 1000000u;
}

// Comme le filtre de dégâts : le deuxième argument est remplacé avant l'appel d'origine.
static uint32_t ONICALL ONIMP_ENTRY Hook7(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f,
                                          uint32_t g) {
    g_calls7++;
    return reinterpret_cast<Sum7>(g_original7)(a, b & 1 ? 0 : b, c, d, e, f, g);
}

int main() {
    uint8_t* code = static_cast<uint8_t*>(VirtualAlloc(nullptr, 512, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!code) return 2;
    memcpy(code, kSum3, sizeof(kSum3));
    memcpy(code + 128, kSum7, sizeof(kSum7));
    memcpy(code + 192, kToFloat, sizeof(kToFloat));
    memcpy(code + 256, kSum2, sizeof(kSum2));
    memcpy(code + 320, kSum4, sizeof(kSum4));
    memcpy(code + 384, kPick6, sizeof(kPick6));
    Sum3 sum3 = reinterpret_cast<Sum3>(code);
    Sum7 sum7 = reinterpret_cast<Sum7>(code + 128);
    ToFloat toFloat = reinterpret_cast<ToFloat>(code + 192);
    Sum2 sum2 = reinterpret_cast<Sum2>(code + 256);
    Sum4 sum4 = reinterpret_cast<Sum4>(code + 320);
    Pick6 pick6 = reinterpret_cast<Pick6>(code + 384);
    bool plain = sum3(1, 20, 300) == 321 && sum7(1, 20, 300, 4000, 50000, 600000, 7000000) == 7654321 &&
                 toFloat(1234) == 1234.0f && sum2(1, 20) == 21 && sum4(1, 20, 300, 4000) == 4321 &&
                 pick6(1, 1, 300, 4000, 50000, 600000) == 600301 && pick6(1, 2, 300, 4000, 50000, 600000) == 1;

    const uint8_t wrong[5] = {0x53, 0x55, 0x56, 0x8B, 0xF2};
    void* unused = nullptr;
    bool refused = !mem::HookEntry(reinterpret_cast<uintptr_t>(code), wrong, sizeof(wrong),
                                   reinterpret_cast<void*>(&Hook3), &unused) &&
                   memcmp(code, kSum3, sizeof(kSum3)) == 0 && !unused;

    bool hooked = mem::HookEntry(reinterpret_cast<uintptr_t>(code), oni::addr::kPotentiallyHostileEntry,
                                 sizeof(oni::addr::kPotentiallyHostileEntry), reinterpret_cast<void*>(&Hook3),
                                 &g_original3) &&
                  mem::HookEntry(reinterpret_cast<uintptr_t>(code + 128), oni::addr::kTakeDamageEntry,
                                 sizeof(oni::addr::kTakeDamageEntry), reinterpret_cast<void*>(&Hook7), &g_original7) &&
                  mem::HookEntry(reinterpret_cast<uintptr_t>(code + 192), oni::addr::kShooterInaccuracyEntry,
                                 sizeof(oni::addr::kShooterInaccuracyEntry), reinterpret_cast<void*>(&HookFloat),
                                 &g_originalFloat) &&
                  mem::HookEntry(reinterpret_cast<uintptr_t>(code + 256), oni::addr::kBuildPoseEntry,
                                 sizeof(oni::addr::kBuildPoseEntry), reinterpret_cast<void*>(&Hook2), &g_original2) &&
                  mem::HookEntry(reinterpret_cast<uintptr_t>(code + 320), oni::addr::kCompareContactsEntry,
                                 sizeof(oni::addr::kCompareContactsEntry), reinterpret_cast<void*>(&Hook4),
                                 &g_original4) &&
                  mem::HookEntry(reinterpret_cast<uintptr_t>(code + 384), oni::addr::kTextConsoleEntry,
                                 sizeof(oni::addr::kTextConsoleEntry), reinterpret_cast<void*>(&Hook6), &g_original6);

    // Adresse de fonction écrite dans le code, comme celle de la mise à jour des zones de
    // déclenchement : remplacée seulement si celle en place est bien celle attendue.
    void* inPlace = reinterpret_cast<void*>(&Hook3);
    memcpy(code + 240, &inPlace, sizeof(inPlace));
    uintptr_t operand = reinterpret_cast<uintptr_t>(code + 240);
    bool pointerRefused = !mem::PatchPointer(operand, reinterpret_cast<void*>(&Hook7), reinterpret_cast<void*>(&HookFloat));
    bool pointerPatched = mem::PatchPointer(operand, reinterpret_cast<void*>(&Hook3), reinterpret_cast<void*>(&Hook7));
    memcpy(&inPlace, code + 240, sizeof(inPlace));
    hooked = hooked && pointerRefused && pointerPatched && inPlace == reinterpret_cast<void*>(&Hook7);

    // Une pile mal rendue se décale à chaque appel et corromprait les variables ci-dessous.
    volatile uint32_t guardBefore = 0xA5A5A5A5;
    int wrongReturn = 0;
    volatile uint32_t guardAfter = 0x5A5A5A5A;
    const int kRounds = 60000;
    for (uint32_t i = 0; hooked && i < static_cast<uint32_t>(kRounds); i++) {
        if (sum3(i, i * 3u, 7u) != i * 4u + 7u + 1000000u) wrongReturn++;
        uint32_t expected = i + (i & 1 ? 0 : i) + 3u + 4u + 5u + 6u + 7u;
        if (sum7(i, i, 3u, 4u, 5u, 6u, 7u) != expected) wrongReturn++;
        if (toFloat(i) != (i & 1 ? 0.0f : static_cast<float>(i))) wrongReturn++;
        if (sum2(i, 9u) != i + 14u) wrongReturn++;
        if (sum4(i, 2u, i, 4u) != (i & 1 ? 77u : i * 2u + 6u)) wrongReturn++;
        // Un seul argument : la fonction d'origine répond ; deux : elle refuse, par l'autre branche.
        uint32_t count = (i & 2) ? 2u : 1u;
        if (pick6(i, count, i, 4u, 5u, 6u) != (i & 1 ? 0u : (count == 1 ? i * 2u + 6u : 1u))) wrongReturn++;
    }

    bool ok = plain && refused && hooked && g_calls3 == kRounds && g_calls7 == kRounds && g_callsFloat == kRounds &&
              g_calls2 == kRounds && g_calls4 == kRounds && g_calls6 == kRounds && wrongReturn == 0 &&
              guardBefore == 0xA5A5A5A5 && guardAfter == 0x5A5A5A5A;
    printf("direct=%d refus=%d pose=%d appels=%d+%d+%d+%d+%d+%d retours faux=%d\n%s\n", plain, refused, hooked,
           g_calls3, g_calls7, g_callsFloat, g_calls2, g_calls4, g_calls6, wrongReturn, ok ? "OK" : "ECHEC");
    return ok ? 0 : 1;
}
