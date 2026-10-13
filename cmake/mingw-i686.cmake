# Toolchain MinGW-w64 32 bits (i686).
# Le "gcc" trouvé dans le PATH peut être une version 64 bits : on désigne le compilateur
# explicitement. Autre emplacement : -DONIMP_MINGW_BIN=C:/chemin/vers/bin
if(NOT DEFINED ONIMP_MINGW_BIN)
    set(ONIMP_MINGW_BIN "C:/mingw64/bin")
endif()
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ONIMP_MINGW_BIN)

set(CMAKE_C_COMPILER "${ONIMP_MINGW_BIN}/i686-w64-mingw32-gcc.exe")
set(CMAKE_CXX_COMPILER "${ONIMP_MINGW_BIN}/i686-w64-mingw32-g++.exe")
