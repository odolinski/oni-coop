# OniMP — experimental network co-op for Oni (PC, 2001)

OniMP is a DLL mod that adds co-op to Oni: up to four players (one host, three clients) play the
campaign together over a LAN or Radmin VPN. Version 0.39, network protocol 25.

It runs on top of the [Anniversary Edition](https://wiki.oni2.net/Anniversary_Edition) and changes
nothing on disk in `Oni.exe`: every patch is applied in memory when the game starts.

**Just want to play? Follow [INSTALL.md](INSTALL.md).** No compiler needed, the built DLL is in
this repository.

Enjoying the mod? You can [support its development on PayPal](https://paypal.me/odolinski).

This is an experimental mod written from the disassembly of `Oni.exe`. Expect bugs; see
[Status and known limits](#status-and-known-limits).

## What you get

- Every player sees the others: position, animations, weapon in hand, aiming, shots, reloads.
- Shared enemies, driven by the host. They attack the nearest player and get tougher with each
  extra player.
- Shared level scripts: objectives, doors, consoles, enemy spawns, dialogues, cutscenes, end of
  level. Save points are the host's.
- Shared level defenses: lasers, turrets and alarms.
- Dropped weapons and pickups are the same for everyone.
- Health bar, ammo, name and ping above each teammate, readable through walls.
- A player who reaches zero health goes **down** instead of dying. A teammate holds **CTRL** for
  3 seconds next to them to revive them before a countdown ends.
- Rules set by the host: difficulty (Easy, Normal, Hard, Survival), enemy multiplier x1 to x4,
  civilians, team kill.
- The game no longer pauses behind menus while other players are connected.
- A player who joins mid-level sends everyone back to the host's last save point, after a
  5-second countdown, and starts without weapons.
- Optional cheats, character swap (play as any character) and spawn menu, granted by the host.
- Client auto-play: a client's character follows the host and fights on its own.
- Mod text in English or French.

## Requirements

- Oni for Windows (retail) with the Anniversary Edition installed. Its Daodan DLL
  (`binkw32.dll`) is what loads OniMP.
- The same OniMP version and the same Anniversary Edition mods on every machine.
- A LAN, or [Radmin VPN](https://www.radmin-vpn.com/) to play over the Internet.

## Quick start

1. Put this folder inside your Oni folder, next to the `AE` folder.
2. Close Oni and run `scripts\install.bat`.
3. Host runs `scripts\run_host_radmin.bat`, each other player `scripts\run_client_radmin.bat`.
4. Everyone starts the same level. Clients find the host and join by themselves.

The step-by-step version, with Radmin VPN and the Anniversary Edition, is in
[INSTALL.md](INSTALL.md).

## Scripts

All scripts are in `scripts\`. Every player must use the same pair.

| Host / client | What is shared |
|---|---|
| `run_host.bat` / `run_client.bat` | players only; each has their own enemies and level scripts |
| `run_host_npcs.bat` / `run_client_npcs.bat` | plus enemies, driven by the host |
| `run_host_coop.bat` / `run_client_coop.bat` | plus level scripts: full co-op |
| `run_host_radmin.bat` / `run_client_radmin.bat` | full co-op over Radmin VPN only; the client finds the host by itself |

`run_client*.bat` takes the host address as an argument (`run_client_coop.bat 192.168.1.10`), or
`auto` to search the network. Without an argument it connects to the same machine, which is how
you test with several windows on one PC.

| Script | Purpose |
|---|---|
| `install.bat` | installs the DLL into the game folder; the original `realbink.dll` is kept as `realbink_orig.dll` |
| `uninstall.bat` | puts the original file back |
| `ae_installer.bat` | opens the AE Installer of the game folder the scripts launch |

The game folder is the `AE` sub-folder of the folder that contains this one, if it holds
`Oni.exe` and `AEInstaller`; otherwise the folder that contains this one. Set the `ONIMP_GAME`
environment variable to point somewhere else.

Network: UDP port 27777 on the host, 27778 on clients (host announcements). Allow `Oni.exe`
through the firewall, on public networks too when using Radmin VPN.

## In-game settings: F1 menu

The pause menu (F1) gets extra tabs while a session is running. Everything changed there is saved
and restored at the next launch.

| Tab | Who | Contents |
|---|---|---|
| Coop | host sets, clients read | `Difficulty`, `Enemies`, `Civilians`, `Team kill`, `Online damage`; sliders `NPC health` and `NPC damage` per extra player; `Alert volume`; `Auto-play` (clients) |
| Display | each player | health bars, ammo, ping, NPC and weapon IDs, mod messages, `Wallhack (ESP)`, `Low portraits`, `Skin`, `Language`, and a `Name` field |
| Cheat | host, and clients the host allows | master switch `Cheats`, `God mode`, `Unlimited ammo`, `One-hit kill`, `Permanent aura`, attack speed, move speed, jump; per-client access buttons |
| Character | same | play as any character class of the game; F5 switches to the next one |
| Spawn | same | spawn items, weapons and NPCs (allies or enemies) in front of you |

The mod's text is French by default. To switch: F1, tab `Affichage`, button `Langue`.

### Difficulty and revive

| Mode | Enemies | Time to revive a downed player | Health when revived |
|---|---|---|---|
| Easy | the game's easy mode | 35 s | 45 % |
| Normal | normal mode | 25 s | 40 % |
| Hard | hard mode | 10 s | 35 % |
| Survival | hard mode, health and damage x1.5, enemies regenerate | 5 s | 25 % |

Going down needs a teammate still standing in the same level; otherwise the player dies as usual.
If the countdown ends, the mission fails for everyone and restarts from the host's last save
point.

With shared enemies, each connected client adds enemy health (default: 200 %, 300 %, 400 % for
one, two, three clients) and, with `Online damage` on, enemy damage (105 %, 110 %, 115 %). The
enemy multiplier never goes below the number of players. Bosses and script-bound characters are
not multiplied.

## Configuration files

Both files sit next to the `Oni.exe` that is launched (in `AE\`). The copies in this repository
are templates that `install.bat` copies only when the game has none: editing them changes nothing
in an installed game.

- `onimp.ini`: network role and port, game rules, display, cheats. Every option is commented in
  the file. The F1 menu rewrites it.
- `onimp_difficulty.ini`: enemy health and damage per number of clients, in percent. Read by the
  host, reloaded as soon as it is saved.

Environment variables, read at launch (this is what the scripts set):

| Variable | Example | Meaning |
|---|---|---|
| `ONIMP` | `host:27777`, `connect:192.168.1.10:27777`, `join`, `off` | network role; overrides `onimp.ini` |
| `ONIMP_NPCS`, `ONIMP_QUESTS` | `1` | shared enemies; shared level scripts (also enables shared enemies) |
| `ONIMP_INTERFACE` | `auto`, `radmin`, `any`, an IP address | network the host announces on |
| `ONIMP_TITLE` | `Host - Oni` | title of the game window |
| `ONIMP_GAME` | a folder | game folder used by the scripts |

## Console commands

Everything in the F1 menu also exists as a console command. They are less tested than the menu.
The developer console opens with the key left of `1`, once developer mode is on (type
`thedayismine` on the F1 pause screen).

| Command | Effect |
|---|---|
| `mp_host [port]`, `mp_connect "ip[:port]"`, `mp_join [port]`, `mp_disconnect` | start, join, search for or leave a session |
| `mp_status` | session state: players, our number, mod comparison |
| `mp_npcs [0\|1]` | shared enemies on or off |
| `mp_difficulty [0-3]`, `mp_enemies [1-4]`, `mp_civilians [0\|1]`, `mp_teamkill [0\|1]`, `mp_onlinedamage [0\|1]` | host: game rules |
| `mp_name "name"`, `mp_skin [0-6]`, `mp_language "en"\|"fr"` | our name, outfit, language |
| `mp_playerhealth`, `mp_npchealth`, `mp_playerammo`, `mp_ping`, `mp_messages`, `mp_npcids`, `mp_weaponids`, `mp_wallhack`, `mp_portraits` `[0\|1]` | display toggles, this machine only |
| `mp_alertvolume [0-100]` | volume of the "player down" sound |
| `mp_autoplay [0\|1]` | client: auto-play |
| `mp_cheats [0\|1]`, `mp_allow player 0\|1` | host: master cheat switch, access for client 1 to 3 |
| `mp_godmode`, `mp_unlimitedammo`, `mp_onehit`, `mp_aura` `[0\|1]` | our own cheats |
| `mp_speeds [attack] [move] [jump]` | our speeds in percent (100 to 300, jump up to 400) |
| `mp_character ["class"]` | play as this character class; no argument: next one |

## Build from source

You need CMake 3.16 or later, Ninja and a **32-bit** C++17 compiler: Oni is a 32-bit program and
cannot load a 64-bit DLL. Tested with MinGW-w64 i686 (GCC 15.2). An MSVC x86 configuration exists
but has never been built.

From this folder:

```
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The toolchain file expects the compiler in `C:/mingw64/bin`; pass `-DONIMP_MINGW_BIN=C:/path/to/bin`
for another location. The result is `build/realbink.dll`, statically linked. Close Oni before
building or installing: the DLL is locked while the game runs.

Add `-DONIMP_BUILD_TESTS=ON` to also build the offline tests in `tools/` (network, interpolation,
script codec, desync rules, difficulty, language and others). They run without the game.

## How it works

- **Loading.** Daodan loads `realbink.dll` just before the engine starts. OniMP takes that name
  and forwards every Bink video function to the original file, renamed `realbink_orig.dll`.
- **Patching.** Calls and functions of `Oni.exe` are redirected in memory, each one only after
  checking that the code in place is the expected one. An unknown `Oni.exe` gets no patch at all.
  Addresses and offsets are in `src/oni.h`.
- **Network.** UDP, star topology: clients talk only to the host, which relays to the others.
  Player states are sent about 30 times per second and interpolated; events (doors, pickups,
  hits, script commands) go through a reliable ordered channel.
- **Authority.** The host simulates enemies and runs the level scripts; clients replay what it
  sends. Each machine simulates its own player.
- **Discovery.** The host broadcasts a small announcement every two seconds on the game port + 1.
- **Desync check.** Host and clients compare fingerprints of their state every second; the host
  resends what differs.

| Folder | Contents |
|---|---|
| `src/` | the mod |
| `tools/` | offline tests |
| `scripts/` | install, uninstall, launch |
| `sounds/` | "player down" alert sound |
| `build/realbink.dll` | the built mod, version 0.39 |
| `docs/` | full technical reference (French) |

## Status and known limits

Many features are written but not yet confirmed in a real game. [OBJECTIFS.md](OBJECTIFS.md)
(French) lists every feature with its level of proof: seen in game, seen in logs, tested offline,
or written only.

- Four players at most. Everything goes through the host: if the host leaves, the session ends.
- No encryption and no authentication. Play on trusted networks only.
- Auto-discovery only works where broadcasts pass: one LAN or one Radmin VPN network.
- Discovery finds the host but does not start the game: each player still starts the level.
- A difference in Anniversary Edition mods is reported, not blocked.
- Without full co-op (`_coop` or `_radmin` scripts), each player has their own level scripts,
  doors and objectives.
- Projectiles are simulated on each machine, so their paths can differ slightly.
- Other players are displayed 100 ms late.
- With a menu open in a session, your character stays in the world, defenseless.
- NPC voices are not synchronized.
- Auto-play fights in melee only and has no path-finding: it follows where the host walked.
- Log files (`onimp_host.log`, `onimp_client.log`, older ones in `onimp_journaux\`) are written
  in French, next to `Oni.exe`. A crash leaves an `EXCEPTION` line there.

## Troubleshooting

| Problem | Fix |
|---|---|
| The game no longer starts | run `scripts\uninstall.bat` |
| `OniMP n'est pas installe dans ...` at launch | the AE Installer rewrote `realbink.dll`: run `install.bat` again |
| No `onimp*.log` file next to `Oni.exe` | the DLL is not loaded: run `install.bat`; check that Daodan is there |
| The client never finds the host | same Radmin VPN network on both sides, firewall, and the host must be in a level, not in a menu |
| `version differente` | players do not have the same OniMP version |
| `mods AE differents` | install the same mods in the AE Installer on every machine |
| A setting in `onimp.ini` has no effect | edit the one next to `Oni.exe`, or use the F1 menu |

More cases are covered in the [technical reference](docs/REFERENCE.fr.md) (French).

## Documentation

- [INSTALL.md](INSTALL.md): step-by-step installation.
- [OBJECTIFS.md](OBJECTIFS.md): what works, what is left to confirm (French).
- [docs/REFERENCE.fr.md](docs/REFERENCE.fr.md): full technical reference (French).

## Support

OniMP is free. If you enjoy it and want to help it go on, you can make a donation:
**[paypal.me/odolinski](https://paypal.me/odolinski)**. Thank you!

## Credits

OniMP relies on the Anniversary Edition and on Daodan, both made by the Oni community
([oni2.net](https://wiki.oni2.net/)). This repository contains no game files; you need your own
copy of Oni.
