// Test des stubs d'interception sans le jeu : chaque stub doit transmettre son numéro, laisser
// intacts les six arguments d'un appel __fastcall, propager la valeur de retour et rendre la pile
// dans l'état où il l'a trouvée (l'appelé nettoie ses arguments, comme dans le moteur).
#include <windows.h>

#include <cstdio>

#include "../src/oni.h"
#include "../src/stub.h"

typedef uint16_t(ONICALL* Callback)(void* info, uint32_t count, void* args, int* ticks, int* stall, void* result);

static volatile int g_current = -1;
static int g_calls = 0;
static int g_errors = 0;
static int g_expectedIndex = 0;
static int g_ticks, g_stall;

static uint16_t ONICALL ONIMP_ENTRY Wrapper(void* info, uint32_t count, void* args, int* ticks, int* stall, void* result) {
    int index = g_current;
    g_calls++;
    if (index != g_expectedIndex) g_errors++;
    if (info != reinterpret_cast<void*>(0x11111111) || count != static_cast<uint32_t>(index) * 3u ||
        args != reinterpret_cast<void*>(0x33333333) || ticks != &g_ticks || stall != &g_stall ||
        result != reinterpret_cast<void*>(0x66666666)) {
        g_errors++;
    }
    *ticks = index;
    return static_cast<uint16_t>(index + 7);
}

int main() {
    const int kCount = 600;
    uint8_t* code = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, kCount * stub::kSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!code) return 2;
    for (int i = 0; i < kCount; i++) stub::Write(code + i * stub::kSize, i, &g_current, reinterpret_cast<void*>(&Wrapper));

    // Une pile mal rendue se décale à chaque appel : 60 000 appels la feraient déborder ou
    // corrompraient les variables locales ci-dessous.
    volatile uint32_t guardBefore = 0xA5A5A5A5;
    int wrongReturn = 0;
    volatile uint32_t guardAfter = 0x5A5A5A5A;
    for (int round = 0; round < 100; round++) {
        for (int i = 0; i < kCount; i++) {
            g_expectedIndex = i;
            Callback callback = reinterpret_cast<Callback>(code + i * stub::kSize);
            uint16_t value = callback(reinterpret_cast<void*>(0x11111111), static_cast<uint32_t>(i) * 3u,
                                      reinterpret_cast<void*>(0x33333333), &g_ticks, &g_stall,
                                      reinterpret_cast<void*>(0x66666666));
            if (value != static_cast<uint16_t>(i + 7) || g_ticks != i) wrongReturn++;
        }
    }

    bool ok = g_calls == kCount * 100 && g_errors == 0 && wrongReturn == 0 && guardBefore == 0xA5A5A5A5 &&
              guardAfter == 0x5A5A5A5A;
    printf("appels=%d erreurs d'arguments=%d retours faux=%d\n%s\n", g_calls, g_errors, wrongReturn, ok ? "OK" : "ECHEC");
    return ok ? 0 : 1;
}
