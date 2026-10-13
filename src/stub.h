// Petit morceau de code machine x86 généré par commande de script interceptée : il note le
// numéro de la commande puis saute dans la fonction commune, sans toucher aux registres ni à la
// pile. La fonction commune reçoit donc exactement les arguments de l'appelant, et lui rend la
// main directement. Testé seul par tools/stubtest.cpp.
#pragma once

#include <cstdint>
#include <cstring>

namespace stub {

constexpr int kSize = 16;

//   C7 05 <&variable> <index>    mov dword ptr [variable], index
//   E9 <rel32>                   jmp target
//   CC                           int3 (jamais atteint)
inline void Write(uint8_t* code, int index, volatile int* variable, void* target) {
    uint32_t variableAddress = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(variable));
    uint32_t number = static_cast<uint32_t>(index);
    int32_t jump = static_cast<int32_t>(reinterpret_cast<uintptr_t>(target) - (reinterpret_cast<uintptr_t>(code) + 15));
    code[0] = 0xC7;
    code[1] = 0x05;
    memcpy(code + 2, &variableAddress, 4);
    memcpy(code + 6, &number, 4);
    code[10] = 0xE9;
    memcpy(code + 11, &jump, 4);
    code[15] = 0xCC;
}

}  // namespace stub
