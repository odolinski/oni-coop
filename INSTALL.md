# Install guide: play Oni in co-op

About 15 minutes. **Every player** does steps 1 to 4 on their own PC. No programming needed.

You need:

- a Windows PC;
- Oni (PC, 2001) already installed, from your own copy of the game;
- one to three friends who follow this same guide.

## Step 1 — Install the Anniversary Edition

The Anniversary Edition is the community patch that makes Oni run on modern Windows. OniMP needs
it.

1. Download **Anniversary-Edition-Setup-v1.4.exe** (20 MB) from the official Oni Mod Depot:
   <http://mods.oni2.net/node/79>
2. Run it and follow its instructions. It works on your Oni folder: the one that contains
   `Oni.exe` and `GameDataFolder`.
3. Let the AE Installer finish installing its packages.

If it asks for **.NET Framework 3.5**, install it from Microsoft:
<https://dotnet.microsoft.com/en-us/download/dotnet-framework/net35-sp1>

When it is done, your Oni folder contains a new folder named `AE`, with its own `Oni.exe`. Start
the game once to check that it runs, then close it.

## Step 2 — Install Radmin VPN

Radmin VPN is a free program that puts you and your friends on the same private network, so the
game can find them. **Skip this step if you all play in the same house, on the same network.**

1. Download it from the official site and install it: <https://www.radmin-vpn.com/>
2. **One** player clicks **Create network**, and picks a network name and a password.
3. The **others** click **Join network** and type that name and password.
4. Check that every player appears in the list, online.

## Step 3 — Put the mod folder in the right place

1. On the GitHub page of this project, click the green **Code** button, then **Download ZIP**.
2. Extract the ZIP. You get a folder named `oni-coop-master`.
3. Move that folder **into your Oni folder, next to the `AE` folder**.

It must look like this:

```
Oni\                        your Oni folder
├── AE\                     created in step 1
│   ├── Oni.exe
│   └── AEInstaller\
└── oni-coop-master\        the mod (the name does not matter)
    ├── build\
    ├── scripts\
    └── README.md
```

Open `oni-coop-master`: you must see `scripts` right away. If you see another `oni-coop-master`
folder inside it instead, move that inner folder up next to `AE` and delete the empty one.

## Step 4 — Install the mod

1. Close Oni.
2. Open `oni-coop-master\scripts` and double-click **install.bat**.

A black window opens and closes at once: that is normal. To check that it worked, look in the `AE`
folder: there is now a file named `realbink_orig.dll`.

Windows may warn you about a file downloaded from the Internet: click **More info**, then **Run
anyway**.

## Step 5 — Play

1. Everyone opens Radmin VPN and checks they are connected.
2. **The host** (one player) double-clicks `scripts\run_host_radmin.bat`.
3. **Each other player** double-clicks `scripts\run_client_radmin.bat`.
4. Everyone starts the same level: **New Game** for all, or the same saved level.

The other players find the host by themselves, with no address to type. A green message appears at
the top right of the screen, `OniMP : connecte a l'hote, vous etes le joueur 1`, and the players
see each other.

Good to know:

- The first time, Windows Firewall asks about `Oni.exe`: click **Allow**, and tick **Public
  networks** too. Radmin VPN counts as a public network.
- Do not hold **Shift** while the game starts: it opens a settings window instead of the game.
- **F1** opens the menu with the co-op settings. The host chooses the rules in the `Coop` tab.
- The mod's text is in French at first. For English: **F1**, tab `Affichage`, button `Langue`.
- A teammate on the ground? Stand next to them and hold **CTRL** for 3 seconds to revive them.
- Four players at most. If the host quits, the session ends for everyone.
- All players need the same version of this mod, and the same mods ticked in the AE Installer.

### Same house, without Radmin VPN

The host double-clicks `scripts\run_host_coop.bat`. Each other player opens the `scripts` folder,
types `cmd` in the address bar of the window, presses Enter, then types:

```
run_client_coop.bat auto
```

## Update

Download the ZIP again, replace the `oni-coop-master` folder with the new one, and run
`install.bat` again with the game closed. Your settings are kept. Every player must update.

## Uninstall

Double-click `scripts\uninstall.bat`: the game is back to how it was. You can then delete the
`oni-coop-master` folder.

## Problems

The scripts and the mod's logs speak French. The messages below are shown as they appear.

| What you see | What to do |
|---|---|
| `Oni.exe introuvable dans ...` | the mod folder is in the wrong place: see step 3 |
| `OniMP n'est pas installe dans ...` | run `install.bat` again. The AE Installer undoes the install when it reinstalls its core packages |
| `realbink.dll introuvable : Daodan (Anniversary Edition) est requis` | the Anniversary Edition is not installed: see step 1 |
| A settings window opens instead of the game | you were holding Shift: close it and start again |
| The other players never join | everyone on the same Radmin VPN network and online; `Oni.exe` allowed in the firewall on public networks; the host must be in a level, not in a menu |
| `Radmin VPN introuvable` | Radmin VPN is closed or disconnected |
| `version differente` | you do not all have the same version: everyone downloads the ZIP again |
| `mods AE differents` | tick the same mods in the AE Installer on every PC (`scripts\ae_installer.bat` opens it) |
| The game no longer starts | double-click `scripts\uninstall.bat` |

Still stuck? The file `onimp_host.log` or `onimp_client.log`, in the `AE` folder, says what
happened. Attach it when you report a problem.
