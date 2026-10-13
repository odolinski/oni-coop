cmake_minimum_required(VERSION 3.16)
project(OniMP LANGUAGES CXX)

# Oni.exe est un exécutable x86 32 bits : la DLL doit l'être aussi.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "OniMP doit etre compile en 32 bits. "
        "MinGW : -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake ; MSVC : -A Win32.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Le fichier produit s'appelle realbink.dll : c'est le nom que Daodan charge.
add_library(realbink SHARED
    src/main.cpp
    src/memory.cpp
    src/network.cpp
    src/replication.cpp
    src/timeline.cpp
    src/scriptsync.cpp
    src/scriptcodec.cpp
    src/world.cpp
    src/groundmatch.cpp
    src/coop.cpp
    src/cheat.cpp
    src/defense.cpp
    src/difficulty.cpp
    src/rules.cpp
    src/revive.cpp
    src/alert.cpp
    src/autoplay.cpp
    src/trail.cpp
    src/lang.cpp
    src/spawns.cpp
    src/interact.cpp
    src/roster.cpp
    src/textconsole.cpp
    src/desync.cpp
    src/desynccheck.cpp
    src/gauge.cpp
    src/levelsync.cpp
    src/mods.cpp
    src/modlist.cpp
    src/config.cpp
    src/log.cpp
    src/exports.def
)
set_target_properties(realbink PROPERTIES PREFIX "" OUTPUT_NAME "realbink")
target_link_libraries(realbink PRIVATE ws2_32 iphlpapi winmm)

# Outils de test sans le jeu. Inutiles pour jouer : construits seulement sur demande
# (-DONIMP_BUILD_TESTS=ON).
option(ONIMP_BUILD_TESTS "Construire les outils de test onimp_*test.exe" OFF)
set(ONIMP_TOOLS "")
if(ONIMP_BUILD_TESTS)
    # Module réseau : deux processus sur la boucle locale.
    add_executable(onimp_nettest
        tools/nettest.cpp
        src/network.cpp
        src/config.cpp
        src/difficulty.cpp
        src/rules.cpp
        src/lang.cpp
        src/log.cpp
    )
    target_link_libraries(onimp_nettest PRIVATE ws2_32 iphlpapi)

    # Interpolation des entités distantes (émetteur simulé).
    add_executable(onimp_timelinetest
        tools/timelinetest.cpp
        src/timeline.cpp
    )

    # Classement et format réseau des commandes de script.
    add_executable(onimp_codectest
        tools/codectest.cpp
        src/scriptcodec.cpp
    )

    # Stubs d'interception des commandes de script (code machine généré).
    add_executable(onimp_stubtest tools/stubtest.cpp)

    # Détour d'une fonction du moteur (prologue recopié dans un tremplin).
    add_executable(onimp_hooktest
        tools/hooktest.cpp
        src/memory.cpp
        src/lang.cpp
        src/log.cpp
    )

    # Détecteur de désynchronisation : ce que l'hôte décide de renvoyer au client, et quand.
    add_executable(onimp_desynctest
        tools/desynctest.cpp
        src/desynccheck.cpp
    )

    # Mods de l'AE Installer : liste installée, comparaison avec celle de l'autre joueur.
    add_executable(onimp_modtest
        tools/modtest.cpp
        src/modlist.cpp
    )

    # Difficulté selon le nombre de clients : vie et dégâts des PNJ, compte des clients.
    add_executable(onimp_difficultytest
        tools/difficultytest.cpp
        src/difficulty.cpp
        src/config.cpp
        src/rules.cpp
    )

    # Règles de la partie coop : modes de difficulté, compte à rebours d'un joueur à terre,
    # multiplicateur d'ennemis, noms des renforts et des civils.
    add_executable(onimp_rulestest
        tools/rulestest.cpp
        src/rules.cpp
    )

    # Numéro des armes et rapprochement des armes au sol : échange de deux armes identiques.
    add_executable(onimp_groundtest
        tools/groundtest.cpp
        src/groundmatch.cpp
    )

    # Langue du mod : dictionnaire français / anglais, formats traduits.
    add_executable(onimp_langtest
        tools/langtest.cpp
        src/lang.cpp
    )

    # Ce qu'onimp.ini garde d'un lancement à l'autre, pour l'hôte et pour un client du même fichier.
    add_executable(onimp_configtest
        tools/configtest.cpp
        src/config.cpp
        src/difficulty.cpp
        src/rules.cpp
    )

    # Trace de l'hôte suivie par le jeu automatique : contourner un mur, sauter une boucle.
    add_executable(onimp_trailtest
        tools/trailtest.cpp
        src/trail.cpp
    )

    set(ONIMP_TOOLS onimp_nettest onimp_timelinetest onimp_codectest onimp_stubtest onimp_hooktest
        onimp_desynctest onimp_modtest onimp_difficultytest onimp_rulestest onimp_groundtest onimp_langtest
        onimp_configtest onimp_trailtest)
endif()

if(MINGW)
    # -mstackrealign : le moteur appelle nos fonctions avec une pile alignée sur 4 octets.
    target_compile_options(realbink PRIVATE -Wall -Wextra -mstackrealign)
    # Lien statique : aucune DLL d'exécution MinGW à copier à côté du jeu.
    target_link_options(realbink PRIVATE -static -static-libgcc -static-libstdc++)
    foreach(tool ${ONIMP_TOOLS})
        target_compile_options(${tool} PRIVATE -Wall -Wextra)
        target_link_options(${tool} PRIVATE -static -static-libgcc -static-libstdc++)
    endforeach()
elseif(MSVC)
    target_compile_options(realbink PRIVATE /W3)
    target_compile_definitions(realbink PRIVATE _CRT_SECURE_NO_WARNINGS)
    foreach(tool ${ONIMP_TOOLS})
        target_compile_definitions(${tool} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endforeach()
    set_property(TARGET realbink ${ONIMP_TOOLS} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
