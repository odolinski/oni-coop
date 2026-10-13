// Horloge commune à tout le mod : secondes écoulées depuis son premier appel.
#pragma once

#include <windows.h>

namespace clk {

inline double Seconds() {
    static LARGE_INTEGER frequency = {};
    static LARGE_INTEGER start = {};
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
        start = counter;
    }
    return static_cast<double>(counter.QuadPart - start.QuadPart) / static_cast<double>(frequency.QuadPart);
}

}  // namespace clk
