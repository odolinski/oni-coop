# OniMP — coop en réseau expérimental pour Oni (PC, 2001)

> Référence technique complète, en français. Présentation courte en anglais :
> [README.md](../README.md) ; installation pas à pas : [INSTALL.md](../INSTALL.md).

OniMP est une DLL qui ajoute à Oni un mode coopératif en réseau local, jusqu'à quatre joueurs,
sur le modèle hôte / client (listen server). Version 0.39, protocole réseau 25.

C'est un mod expérimental, écrit à partir du désassemblage d'Oni.exe. Lisez
[État de validation](#état-de-validation) et [Limites connues](#limites-connues) avant de
vous lancer. L'avancement détaillé est dans [OBJECTIFS.md](../OBJECTIFS.md).

## Ce que fait le mod

Jusqu'à quatre joueurs : un hôte et trois clients au plus. Chaque joueur a un numéro, 0 pour
l'hôte, 1 à 3 pour les clients dans l'ordre de leur arrivée. Les clients ne se parlent pas entre
eux : l'hôte redistribue à chacun ce qui vient des autres. Ce document a été écrit pour deux
joueurs ; là où il dit encore « l'autre joueur » ou « les deux joueurs », lire « chaque autre
joueur » et « tous les joueurs » : ce qui est décrit vaut entre tous ceux qui sont là.

Pour se trouver : l'hôte annonce sa partie sur le réseau, et le client peut le chercher et le
rejoindre tout seul, sans saisir d'adresse. Prévu pour Radmin VPN, marche aussi en réseau local.

Avec les mods de l'Anniversary Edition : les scripts lancent le jeu que l'AE Installer tient à
jour, donc avec les mods qui y sont cochés. Tous les joueurs doivent avoir installé les mêmes ; à
la connexion l'hôte et le client comparent leurs listes, et un écart est signalé dans la console.

Toujours, entre tous les joueurs connectés dans le même niveau :

- chacun voit l'autre : position, orientation, animations ;
- son arme en main, sa visée (buste, bras et arme orientés), ses tirs, son rechargement ;
- les armes lâchées et ramassées, avec les cartouches qu'il leur reste, et les bonus ramassés ;
- sa santé au-dessus de sa tête : une barre de la couleur de son numéro (hôte en vert, puis bleu,
  orange, jaune), sans chiffres ;
- ses munitions au-dessus de cette barre : une rangée de six petites cases carrées, rouges pour
  ses chargeurs de balles, vertes pour ses cellules d'énergie (voir
  [Munitions des autres joueurs](#munitions-des-autres-joueurs)) ;
- son pseudo et sa latence au-dessus, le tout lisible à travers les murs, et une tenue de Konoko
  que personne d'autre ne porte (voir [Pseudos, tenues, latence](#pseudos-tenues-latence)) ;
- la santé des PNJ : la même barre, plus petite, avec les chiffres dessus, rouge pour un ennemi,
  violette pour un boss ;
- un joueur qui tombe à zéro reste à terre au lieu de mourir : un coéquipier le ranime en gardant
  CTRL enfoncé trois secondes près de lui, avant la fin d'un compte à rebours (voir
  [Joueur à terre et réanimation](#joueur-à-terre-et-réanimation)) ;
- des règles de partie décidées par l'hôte : mode de difficulté (facile, normal, difficile,
  survie), multiplicateur d'ennemis de x1 à x4, civils, tir ami (tirs, coups de mêlée et prises),
  et dégâts des PNJ, normaux ou « en ligne » ;
- son rechargement, et ce qui se voit sur lui sans être un geste : l'aura de lumière qui l'entoure
  quand un hypo pris en pleine forme le rend plus fort, la lueur de son bouclier de force, son
  invisibilité ;
- un joueur qui arrive en cours de niveau fait repartir tout le monde du dernier point de sauvegarde
  de l'hôte, après un compte à rebours de cinq secondes, et commence sans armes ;
- après chaque chargement la partie démarre chez tous en même temps, dès que tous ont chargé, sans
  écran d'accueil ni touche à presser ;
- des réglages propres à chaque joueur : son pseudo, sa tenue, et afficher ou non la santé des
  joueurs, leurs munitions, leur latence, la santé des PNJ, les messages du mod (écrits en vert en
  haut à droite de l'écran), et les identifiants des PNJ et des armes au sol ;
- trois onglets réservés à l'hôte et aux clients qu'il autorise (depuis la 0.35) : `Cheat`
  (invincibilité, munitions illimitées, coup fatal, aura permanente, vitesse d'attaque, vitesse de
  déplacement, super saut sans dégâts de chute), `Character` (incarner n'importe quel personnage du
  jeu à la place de Konoko, touche F5 pour passer au suivant) et `Spawn` (faire apparaître devant
  soi objets, armes et PNJ, alliés ou ennemis). Chacun règle ses propres cheats, qui n'agissent
  que sur son personnage ; l'hôte garde un interrupteur général qui les coupe chez tous (voir
  [Cheats, personnage incarné, apparitions](#cheats-personnage-incarné-apparitions)) ;
- un client resté dans un autre niveau que l'hôte charge de lui-même celui de l'hôte, au même
  point de sauvegarde (depuis la 0.36 : sauvegarde chargée par l'hôte depuis son menu, client qui
  arrive d'ailleurs) ;
- plus de pause derrière les menus : tant qu'un autre joueur est relié, le monde continue de
  tourner quand on ouvre le menu F1, le menu Échap ou l'écran d'une console ;
- les écrans de texte du niveau (consoles de données, consignes) s'ouvrent sans arrêter le script
  qui les demande : personne n'attend plus qu'un autre ait fini de lire (voir
  [Écrans de texte du niveau](#écrans-de-texte-du-niveau)).

Avec l'option `npcs` (PNJ pilotés par l'hôte) : mêmes ennemis des deux côtés, qui s'en prennent
au joueur le plus proche et se durcissent avec le nombre de clients (plus de vie et, si l'hôte
le veut, plus de dégâts : les valeurs se règlent dans `onimp_difficulty.ini`), dégâts arbitrés
entre les machines, projections, mêmes bonus au sol des deux côtés, reprise commune
à la dernière sauvegarde de l'hôte quand un joueur meurt. Depuis la 0.36 les défenses du niveau
sont communes : un laser coupé par un joueur, là où il le voit, allume les tourelles chez tous,
elles visent le joueur que l'hôte choisit et le blessent chez lui, et une alarme amène trois
ennemis du niveau par joueur (voir
[Lasers, tourelles, alarmes](#lasers-tourelles-alarmes)). Les civils et les alliés du niveau qui
donnent quelque chose quand on leur parle le donnent aussi au joueur d'un client (voir
[Civils qui donnent un objet](#civils-qui-donnent-un-objet)).

Avec l'option `quests` (coop complet, qui active aussi `npcs`) : les scripts du niveau sont
ceux de l'hôte. Objectifs, portes, consoles, apparition des ennemis, dialogues et fin de niveau
sont partagés. Les points de sauvegarde sont ceux de l'hôte : un client n'enregistre rien de
lui-même. L'écran de texte d'une console de données ne s'ouvre que chez le joueur qui vient de
l'utiliser ; ceux que le niveau montre de lui-même s'ouvrent chez tous.

## Prérequis

- Oni PC avec l'**Anniversary Edition** : la DLL Daodan (`binkw32.dll`) doit être présente,
  c'est elle qui charge OniMP.
- `Oni.exe` dans la version attendue par Daodan (build du 4 novembre 2000). OniMP le vérifie
  au démarrage et ne pose aucun patch sur un exécutable inconnu.
- Pour compiler : CMake 3.16 ou plus, Ninja, et un compilateur C++17 **32 bits**
  (MinGW-w64 i686 ou MSVC x86). Oni étant un programme 32 bits, une DLL 64 bits ne peut pas
  être chargée.

## Contenu

```
OniMP/
  CMakeLists.txt
  cmake/mingw-i686.cmake    toolchain MinGW 32 bits
  src/
    main.cpp                point d'entrée, hooks, boucle par frame, commandes de console
    memory.cpp/.h           lecture et écriture de l'état du jeu, spawn, animations, visée, dégâts
    network.cpp/.h          session UDP en étoile (WinSock2) : un hôte, trois clients, ce que l'hôte
                            redistribue ; annonce et recherche de l'hôte
    clock.h                 horloge commune à tout le mod
    desync.cpp/.h           comparaison de l'état de l'hôte et de chaque client, renvoi de ce qui diffère
    desynccheck.cpp/.h      règles de cette comparaison (sans accès au jeu)
    replication.cpp/.h      autres joueurs et PNJ : états reçus, projections, dégâts, mêlée entre joueurs, IA
    gauge.cpp/.h            jauge de vie au-dessus d'un personnage : barre et chiffres, compte à rebours ;
                            munitions d'un joueur en cases au-dessus d'elle ; identifiants en lettres
                            au-dessus des PNJ et des armes au sol
    rules.cpp/.h            règles de la partie : modes de difficulté, compte à rebours d'un joueur à
                            terre, multiplicateur d'ennemis, noms des renforts et des civils (sans accès
                            au jeu)
    revive.cpp/.h           joueur à terre et réanimation
    alert.cpp/.h            son joué une fois quand un joueur tombe à terre (MP3 lu par Windows)
    autoplay.cpp/.h         jeu automatique d'un client : son personnage suit l'hôte et se bat seul
    cheat.cpp/.h            onglets Cheat, Character et Spawn : accès donnés par l'hôte, cheats de chaque
                            joueur, personnage incarné, apparitions
    defense.cpp/.h          lasers que chaque joueur coupe chez lui, tourelles et rayons décidés par l'hôte, alarmes
    trail.cpp/.h            trace des endroits où l'hôte est passé, que suit le jeu automatique
    lang.cpp/.h             langue des textes du mod : dictionnaire français / anglais (sans accès au jeu)
    spawns.cpp/.h           renforts du multiplicateur d'ennemis, civils
    interact.cpp/.h         civils du niveau qui parlent au joueur d'un client et lui donnent un objet
    roster.cpp/.h           pseudo, tenue et latence de chaque joueur ; tenues attribuées par l'hôte
    textconsole.cpp/.h      écrans de texte du niveau, ouverts depuis la boucle du jeu
    timeline.cpp/.h         interpolation des états reçus (sans accès au jeu)
    world.cpp/.h            portes ouvertes, consoles activées, arme en main, objets au sol
    groundmatch.cpp/.h      numéro des armes, rapprochement des armes au sol (sans accès au jeu)
    coop.cpp/.h             réglages de partie de l'hôte, réglages d'affichage, onglets du menu F1
    difficulty.cpp/.h       vie et dégâts des PNJ selon le nombre de clients : table du fichier de
                            difficulté, calculs, compte des clients (sans accès au jeu)
    levelsync.cpp/.h        reprise commune à la dernière sauvegarde de l'hôte
    mods.cpp/.h             mods de l'AE Installer : liste installée, comparée entre l'hôte et chaque client
    modlist.cpp/.h          lecture et comparaison de cette liste (sans accès au jeu)
    scriptsync.cpp/.h       « quêtes » : commandes de script transmises de l'hôte au client
    scriptcodec.cpp/.h      classement et format réseau de ces commandes (sans accès au jeu)
    stub.h                  code machine généré pour intercepter chaque commande
    protocol.h              format des paquets
    oni.h                   adresses et offsets d'Oni.exe
    config.cpp/.h           onimp.ini et variable d'environnement ONIMP
    log.cpp/.h              journal
    exports.def             renvoi des fonctions Bink vers realbink_orig.dll
  tools/                    douze tests sans le jeu : réseau, interpolation, codec, stubs, détour, désynchronisation,
                            mods, difficulté, règles, armes au sol, langue, réglages gardés
  sounds/                   AlertFall.mp3 : le son joué quand un joueur tombe
  scripts/                  install, uninstall, run_host, run_client (variantes _npcs, _coop et _radmin),
                            ae_installer ; game_dir et launch, appelés par les autres
  onimp.ini                 configuration par défaut
  onimp_difficulty.ini      difficulté par défaut : vie et dégâts des PNJ selon le nombre de clients
  README.md                 présentation courte, en anglais
  INSTALL.md                installation pas à pas, en anglais
  docs/REFERENCE.fr.md      ce fichier
  OBJECTIFS.md              ce qui marche, ce qui reste à confirmer, ce qui reste à faire
```

## Compiler

### MinGW-w64 (testé : GCC 15.2 i686, CMake 4.1, Ninja)

Depuis le dossier qui contient `OniMP` :

```
cmake -S OniMP -B OniMP/build -G Ninja -DCMAKE_TOOLCHAIN_FILE=OniMP/cmake/mingw-i686.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build OniMP/build
```

Le fichier de toolchain suppose le compilateur dans `C:/mingw64/bin`. Pour un autre
emplacement, ajoutez `-DONIMP_MINGW_BIN=C:/chemin/vers/bin`. Le `gcc` du `PATH` n'est pas
utilisé : il s'agit souvent d'une version 64 bits.

Le résultat est `OniMP/build/realbink.dll`, lié statiquement (aucune DLL MinGW à copier).

Les neuf outils de test `onimp_*test.exe` ne servent pas à jouer et ne sont pas construits
par défaut. Pour les obtenir, ajoutez `-DONIMP_BUILD_TESTS=ON` à la première commande.

### MSVC (non testé)

```
cmake -S OniMP -B OniMP/build-msvc -A Win32
cmake --build OniMP/build-msvc --config Release
```

Le code évite les extensions propres à GCC, mais cette configuration n'a pas été compilée.

## Installer et désinstaller

### Quel dossier de jeu

Une machine peut avoir plusieurs copies d'Oni : le jeu d'origine, et l'Anniversary Edition, que
son installeur pose dans un sous-dossier `AE`. Seule la copie que l'AE Installer tient à jour
reçoit les mods qu'on y coche. Tous les scripts d'OniMP visent donc le même dossier, celui que
donne `scripts\game_dir.bat` :

1. le dossier de la variable d'environnement `ONIMP_GAME`, si elle est définie ;
2. sinon le sous-dossier `AE` du dossier qui contient `OniMP`, s'il a son `Oni.exe` et son
   `AEInstaller` ;
3. sinon le dossier qui contient `OniMP`.

« Dossier du jeu » désigne ce dossier dans la suite : la DLL y est installée, `onimp.ini` et les
journaux s'y trouvent, et ses mods sont ceux de la partie. Jusqu'à la version 0.12 les scripts
visaient toujours le dossier qui contient `OniMP` : avec une Anniversary Edition dans son
sous-dossier `AE`, ils lançaient l'autre copie du jeu, sans les mods.

### Installer

```
OniMP\scripts\install.bat
```

Le script renomme le `realbink.dll` d'origine en `realbink_orig.dll`, copie la DLL OniMP à
sa place et dépose `onimp.ini` et `onimp_difficulty.ini` à côté d'`Oni.exe` s'ils n'y sont pas ;
des fichiers déjà là, donc vos réglages, ne sont jamais écrasés. Il affiche le dossier où il l'a
fait. Daodan charge `realbink.dll` juste avant de démarrer le moteur : OniMP s'installe à ce
moment-là et renvoie toutes les fonctions vidéo Bink vers le fichier d'origine.

`OniMP\scripts\uninstall.bat` remet le fichier d'origine en place. Si l'AE Installer
réinstalle Daodan, il réécrit `realbink.dll` : relancez `install.bat`. Les scripts `run_*.bat`
le vérifient, et s'arrêtent sur un message au lieu de lancer un jeu sans OniMP.

Fermez Oni avant d'installer ou de recompiler, la DLL est verrouillée pendant le jeu.

## Lancer une partie

Trois paires de scripts, du plus simple au plus complet. Tous les joueurs doivent lancer la
même paire : l'hôte son script, chaque client le sien. Une partie compte un hôte et un à trois
clients ; un client peut arriver ou repartir en cours de partie.

| Scripts | Ce qui est partagé |
|---|---|
| `run_host.bat` / `run_client.bat` | les joueurs seulement ; chacun a ses ennemis et ses scripts |
| `run_host_npcs.bat` / `run_client_npcs.bat` | en plus : les PNJ, pilotés par l'hôte |
| `run_host_coop.bat` / `run_client_coop.bat` | en plus : les scripts du niveau, décidés par l'hôte (coop complet) |

### Sur une seule machine

1. Compilez, installez, fermez toute instance d'Oni.
2. Lancez le script de l'hôte, puis celui du client : une fois par client, trois fois au plus.
3. Dans **chaque** fenêtre, démarrez une partie dans le **même niveau** (par exemple
   « New Game » partout).
4. Chaque joueur doit voir apparaître les autres. La console affiche « OniMP : joueur 1 apparu »,
   avec le numéro de chacun.

À savoir :

- En mode fenêtré (`-noswitch`), chaque fenêtre couvre l'écran et toutes se recouvrent :
  passez de l'une à l'autre avec Alt+Tab. En niveau, une instance continue de tourner quand
  elle n'a pas le focus (vérifié).
- Ne maintenez pas **Maj** pendant le lancement : Daodan ouvre alors sa fenêtre de
  configuration au lieu du jeu.
- Oni refuse normalement d'être lancé deux fois. OniMP lève ce contrôle
  (`allow_multiple_instances` dans `onimp.ini`).
- Toutes les instances partagent `persist.dat` et `startup.txt`.

Chaque instance écrit son journal dans le dossier du jeu : `onimp_host.log`, `onimp_client.log`
(ou `onimp.log` si le mode est `off`). Quand plusieurs clients tournent sur la même machine, le
deuxième écrit `onimp_client_2.log` et le troisième `onimp_client_3.log` : le rang est celui du
lancement, pas le numéro du joueur, que la première ligne `pair connecte` du journal donne. Pour une
ligne d'état par seconde, passez `trace = true` dans la section `[debug]` d'`onimp.ini`.

**Journaux gardés (0.33).** Jusqu'à la 0.32 chaque lancement écrasait le journal du précédent :
après un plantage, relancer le jeu suffisait à perdre la seule trace de ce qui s'était passé.
Désormais le journal trouvé au lancement est d'abord déplacé dans le sous-dossier
`onimp_journaux`, sous un nom daté de sa dernière écriture (`onimp_host_20261013_014438.log`),
avec une copie du `debugger.txt` du moteur prise au même moment
(`onimp_host_20261013_014438_debugger.txt`) : tous les lancements se partagent ce fichier, et le
suivant le vide. Le nouveau journal le dit à sa deuxième ligne (`journal : celui du lancement
precedent est garde, ...`). Le dossier garde les soixante fichiers les plus récents. Le journal
qu'une autre instance du jeu tient ouvert n'est pas touché.

### En réseau local

Sur l'hôte : le script `run_host*.bat`, et autorisez `Oni.exe` en UDP sur le port 27777 dans le
pare-feu. Sur chaque autre machine : `run_client*.bat 192.168.x.x` (adresse de l'hôte), ou
`run_client*.bat auto` pour que le client cherche l'hôte lui-même. Toutes les machines ont besoin
de la même version d'OniMP et des mêmes données de jeu, donc des mêmes mods (voir
[Avec les mods de l'AE Installer](#avec-les-mods-de-lae-installer)).

### Par Radmin VPN, sans adresse à saisir

1. Tous les joueurs sont connectés au **même réseau Radmin VPN** (chacun y a une adresse en
   26.x.x.x).
2. L'hôte lance `run_host_radmin.bat`, chaque client `run_client_radmin.bat` : coop complet, et
   rien d'autre à régler.
3. Chacun démarre une partie dans le même niveau. Dès qu'il est en jeu, l'hôte annonce sa
   partie toutes les deux secondes sur le réseau Radmin. Le client, dès qu'il est en jeu lui
   aussi, reçoit l'annonce, y lit l'adresse de l'hôte et se connecte. La console du client
   affiche `OniMP : partie 'Oni' trouvee a 26.x.x.x:27777 (niveau '...', 1 joueur(s) sur 4) :
   connexion`, puis `OniMP : connecte a l'hote, vous etes le joueur 1`.

Pare-feu : Windows range en général le réseau Radmin parmi les réseaux **publics**. `Oni.exe`
doit y être autorisé en UDP entrant : port 27777 chez l'hôte, port 27778 chez le client (c'est
là qu'arrivent les annonces). À la première écoute Windows le demande ; cochez les réseaux
publics.

Ce que dit la console quand cela ne se fait pas :

| Message | Cause |
|---|---|
| `Radmin VPN introuvable` | pas d'adresse en 26.x.x.x sur cette machine : Radmin VPN est fermé ou déconnecté. Le message `Radmin VPN connecte` suit dès qu'il revient |
| `partie '...' trouvee a ..., mais complete (4 joueurs)` | l'hôte a déjà trois clients |
| `partie '...' trouvee a ..., mais en OniMP ...` | ce joueur et l'hôte n'ont pas la même version |
| `refuse par l'hote : ...` | même raisons, dites par l'hôte au moment de la connexion |
| `l'hote ne repond plus : nouvelle recherche` | six secondes sans réponse ; le client écoute de nouveau les annonces |
| rien du tout | les annonces n'arrivent pas : pare-feu du client, ou ce joueur et l'hôte ne sont pas sur le même réseau Radmin |

Les scripts `_radmin` restreignent tout à Radmin VPN (`interface = radmin`). Sans cela
(`interface = auto`, valeur par défaut), l'hôte s'annonce sur Radmin VPN s'il est connecté, sinon
sur toutes les interfaces de la machine, et le client accepte l'annonce d'où qu'elle vienne.
Plusieurs parties sur le même réseau : donnez un nom à la vôtre (`name` chez l'hôte, `join` chez
le client).

### Avec les mods de l'AE Installer

L'AE Installer installe les mods (personnages, armes, IA, niveaux, textures, effets) dans le
`GameDataFolder` du dossier du jeu. Les scripts `run_*.bat` lancent ce jeu-là : ce qui y est
installé est ce qui se joue, quelle que soit la paire de scripts.

1. `OniMP\scripts\ae_installer.bat` ouvre l'AE Installer du dossier du jeu. Cochez les mods et
   installez. Un mod seulement téléchargé n'est pas installé : il doit être coché au moment de
   l'installation.
2. Chaque autre joueur installe **les mêmes mods** sur sa machine. Sur une seule machine, toutes
   les fenêtres partagent le dossier, donc les mods.
3. Lancez la partie comme d'habitude.

Au lancement, la deuxième ligne du journal donne les paquets installés dans le jeu lancé, avec
une empreinte de la liste :

```
mods AE : 10 paquet(s), empreinte B5680635 : 50 Menu Fixes AE, ..., 6000 Missing Sounds, 34090 Blood
```

À chaque connexion l'hôte et le client échangent leur liste. Si elles sont les mêmes, les journaux
le disent : `mods AE : memes paquets chez le joueur 1 (10, empreinte B5680635)` chez l'hôte, la
même ligne pour le joueur 0 chez le client. Sinon la console l'affiche chez les deux, avec ce que
chacun a de plus que l'autre :

```
OniMP : mods AE differents de ceux du joueur 1
OniMP : ici seulement : 34090 Blood
OniMP : chez lui seulement : 21501 Brutal AI
```

Chaque client se compare à l'hôte, pas aux autres clients : deux clients qui ont les mods de
l'hôte ont les mêmes.

`mp_status` redit le résultat. La partie n'est pas refusée : c'est aux joueurs d'aligner leurs
mods. OniMP désigne les animations, les PNJ et les armes par leur nom ; un mod qui en ajoute ou en
change doit être présent des deux côtés, sans quoi l'autre machine reçoit des noms qu'elle ne
connaît pas. Un mod de textures ou d'effets ne change en principe que ce que voit celui qui l'a.

## Configurer

Trois moyens, du plus prioritaire au moins prioritaire.

**Commandes de console**, en jeu. La console développeur d'Oni s'ouvre avec la touche à gauche
du 1, une fois le mode développeur activé (code `thedayismine` saisi sur l'écran de pause F1) :

| Commande | Effet |
|---|---|
| `mp_host [port]` | héberge une partie (27777 par défaut) |
| `mp_connect "ip[:port]"` | rejoint un hôte ; les guillemets sont nécessaires |
| `mp_join [port]` | cherche un hôte sur le réseau et le rejoint |
| `mp_disconnect` | ferme la session |
| `mp_status` | affiche l'état de la session : joueurs présents, numéro du nôtre, comparaison des mods |
| `mp_npcs [0\|1]` | active ou coupe la synchronisation des PNJ |
| `mp_teamkill [0\|1]` | hôte : les joueurs peuvent se blesser entre eux ; sans argument, affiche le réglage |
| `mp_godmode [0\|1]` | notre joueur est invincible (depuis la 0.35 : cheat de chaque joueur, et non plus règle de l'hôte) ; sans argument, affiche le réglage |
| `mp_onlinedamage [0\|1]` | hôte : les PNJ font aux joueurs les dégâts d'`onimp_difficulty.ini` ; 0 : dégâts normaux ; sans argument, affiche le réglage |
| `mp_difficulty [0-3]` | hôte : mode de difficulté, 0 facile, 1 normal, 2 difficile, 3 survie ; sans argument, affiche le mode |
| `mp_enemies [1-4]` | hôte : multiplicateur d'ennemis ; sans argument, affiche celui qui vaut |
| `mp_civilians [0\|1]` | hôte : des civils se mêlent aux ennemis ; sans argument, affiche le réglage |
| `mp_playerhealth [0\|1]` | affiche ou masque la santé des autres joueurs, sur cette machine |
| `mp_npchealth [0\|1]` | affiche ou masque la santé des PNJ, sur cette machine |
| `mp_messages [0\|1]` | affiche ou masque les messages du mod à l'écran, sur cette machine ; sans argument, dit le réglage |
| `mp_npcids [0\|1]` | affiche ou masque le nom de chaque PNJ au-dessus de lui, sur cette machine |
| `mp_weaponids [0\|1]` | affiche ou masque le nom de chaque arme au sol au-dessus d'elle, sur cette machine |
| `mp_playerammo [0\|1]` | affiche ou masque les munitions des autres joueurs au-dessus de leur jauge, sur cette machine |
| `mp_ping [0\|1]` | affiche ou masque la latence des autres joueurs à côté de leur pseudo, sur cette machine |
| `mp_name "pseudo"` | notre pseudo, quinze signes au plus ; sans argument, le dit |
| `mp_skin [0-6]` | notre tenue, par son numéro ; refusée si un autre joueur la porte ; sans argument, la dit |
| `mp_unlimitedammo [0\|1]` | munitions illimitées pour notre joueur (cheat de chaque joueur depuis la 0.35) ; sans argument, affiche le réglage |
| `mp_cheats [0\|1]` | hôte : interrupteur général des cheats ; sans argument, affiche son état et les accès |
| `mp_allow joueur 0\|1` | hôte : ouvre (1) ou retire (0) les onglets Cheat, Character et Spawn au client de ce numéro, 1 à 3 |
| `mp_onehit [0\|1]` | coup fatal : nos coups et nos tirs tuent un ennemi d'un coup |
| `mp_aura [0\|1]` | aura permanente pour notre joueur |
| `mp_speeds [attaque] [déplacement] [saut]` | vitesse d'attaque, de déplacement (100 à 300) et force du saut (100 à 400), en pour cent ; sans argument, les affiche |
| `mp_character ["classe"]` | incarne cette classe de personnage (`"konoko"` pour revenir) ; sans argument, passe à la suivante, comme la touche F5 |

**Variable d'environnement `ONIMP`**, lue au lancement. Elle permet de lancer plusieurs instances
depuis le même dossier avec des rôles différents ; c'est ce que font les scripts `run_*.bat` :

```
set ONIMP=host:27777
set ONIMP=connect:192.168.1.10:27777
set ONIMP=join
set ONIMP=off
```

`join` (ou `connect:auto`) : client qui cherche l'hôte lui-même. `ONIMP_INTERFACE` remplace de
la même façon l'option `interface`. `ONIMP_TITLE` donne son titre à la fenêtre du jeu : les scripts
`run_host_radmin.bat` et `run_client_radmin.bat` y mettent `Host - Oni` et `Client - Oni`, pour
distinguer deux fenêtres sur une même machine. `ONIMP_GAME`, elle, est lue par les scripts : c'est le dossier
du jeu à lancer, quand ce n'est pas celui qu'ils trouvent seuls (voir
[Quel dossier de jeu](#quel-dossier-de-jeu)).

**Fichier `onimp.ini`**, à côté d'`Oni.exe`. Chaque option est commentée dans le fichier.

| Section | Option | Rôle |
|---|---|---|
| `[net]` | `mode`, `host`, `port`, `rate` | rôle (`off`, `host`, `client`), adresse de l'hôte ou `auto` pour le chercher, port, envois par seconde |
| `[net]` | `name`, `join` | hôte : nom de la partie annoncée ; client en recherche : ne rejoindre que la partie de ce nom |
| `[net]` | `interface` | réseau où l'hôte s'annonce et d'où chacun accepte l'autre : `auto`, `radmin`, `any` ou une adresse |
| `[player]` | `class` | classe du personnage affiché pour un autre joueur tant que sa tenue n'est pas connue |
| `[player]` | `name_host`, `name_client` | notre pseudo, quand cette machine est l'hôte, quand elle est un client ; écrits par le jeu |
| `[player]` | `skin_host`, `skin_client` | notre tenue, de 0 à 6 ; `-1` : au choix de l'hôte ; écrits par le jeu |
| `[sync]` | `npcs` | PNJ pilotés par l'hôte ; à activer des deux côtés |
| `[sync]` | `delete_orphans` | client : supprime les PNJ que l'hôte ne connaît pas |
| `[sync]` | `quests` | scripts du niveau décidés par l'hôte ; active aussi `npcs` ; lue au lancement |
| `[game]` | `allow_multiple_instances` | plusieurs instances d'Oni sur la même machine |
| `[game]` | `online_damage` | hôte : dégâts des PNJ « en ligne », ceux d'`onimp_difficulty.ini` (`true`, par défaut), ou normaux (`false`) |
| `[game]` | `difficulty` | hôte : mode de difficulté, `easy`, `normal` (par défaut), `hard` ou `survival` |
| `[game]` | `enemies` | hôte : multiplicateur d'ennemis choisi, de 1 (par défaut) à 4 |
| `[game]` | `civilians` | hôte : civils mêlés aux ennemis (`true`, par défaut) |
| `[game]` | `run_in_menus` | en session, le jeu continue derrière les menus (`true`, par défaut) ; `false` : il se met en pause, comme en solo |
| `[game]` | `join_reload` | hôte : à l'arrivée d'un joueur en cours de niveau, reprise commune au dernier point de sauvegarde (`true`, par défaut) ; `false` : il garde le niveau qu'il a chargé |
| `[game]` | `ready_check` | après un chargement, la partie reste en pause jusqu'à ce que tous aient chargé (`true`, par défaut) ; `false` : chacun démarre dès qu'il a chargé |
| `[display]` | `player_health`, `npc_health` | santé affichée au-dessus des autres joueurs, des PNJ |
| `[display]` | `messages` | messages du mod à l'écran (`true`, par défaut) ; `false` : ils ne vont plus qu'au journal |
| `[display]` | `npc_ids`, `weapon_ids` | identifiants au-dessus des PNJ, des armes au sol (`true`, par défaut) |
| `[display]` | `player_ammo` | munitions des autres joueurs au-dessus de leur jauge (`true`, par défaut) |
| `[display]` | `ping` | latence des autres joueurs à côté de leur pseudo (`true`, par défaut) |
| `[display]` | `wallhack` | les autres joueurs vus à travers les murs (`true`, par défaut) |
| `[display]` | `portraits_bottom` | portraits des dialogues en bas de l'écran, au-dessus des sous-titres (`true`, par défaut) |
| `[game_client]` | `autoplay` | client : jeu automatique (`false`, par défaut) ; réécrit par l'onglet Coop |
| `[display]` | `language` | langue des textes du mod, `fr` (par défaut) ou `en` |
| `[audio]` | `alert_volume` | volume du son joué quand un joueur tombe, de 0 à 100 (50 par défaut ; 0 : muet) |
| `[display_client]`, `[audio_client]` | les mêmes clés | ce que règle un client : il n'écrit que dans ces sections, l'hôte que dans `[display]` et `[audio]` (voir « Ce que le menu garde ») |
| `[game]` | `team_kill` | hôte : tir ami (`false`, par défaut) ; réécrit par l'onglet Coop |
| `[game]` | `cheats`, `cheat_access` | hôte : interrupteur général des cheats (`false`, par défaut) et clients à qui il ouvre les onglets Cheat, Character et Spawn (2 pour le joueur 1, 4 pour le 2, 8 pour le 3, additionnés) ; réécrits par l'onglet Cheat |
| `[cheats]`, `[cheats_client]` | `god_mode`, `unlimited_ammo`, `one_hit_kill`, `permanent_aura` | cheats de ce joueur (`false`, par défaut) : l'hôte écrit sous `[cheats]`, un client sous `[cheats_client]` |
| `[cheats]`, `[cheats_client]` | `attack_speed`, `move_speed`, `jump` | en pour cent, 100 par défaut ; jusqu'à 300, et 400 pour le saut |
| `[cheats]`, `[cheats_client]` | `character`, `character_key` | classe du personnage incarné (vide : Konoko) et touche « personnage suivant », de `F5` (par défaut) à `F12` |
| `[debug]` | `trace` | une ligne d'état par seconde dans le journal |

### Réglages en jeu : menu F1

Le menu de pause du jeu (F1) a des onglets de plus, sous l'onglet Help. Chacun s'ouvre sur des
boutons, qui restent enfoncés quand le réglage est actif.

**Coop** : les règles de la partie. L'hôte les règle ; depuis la 0.19 les clients ont l'onglet
aussi, pour les lire : un clic n'y change rien, et ce que l'hôte change s'y voit aussitôt, menu
ouvert.

| Bouton | Effet |
|---|---|
| `Difficulty : Normal` | mode de difficulté ; chaque clic passe au suivant : `Easy`, `Normal`, `Hard`, `Survival` (voir [Difficulté : quatre modes](#difficulté--quatre-modes)) |
| `Enemies : x1` | multiplicateur d'ennemis ; chaque clic passe au suivant parmi ceux que le nombre de joueurs permet. `(min x2)` : le minimum du moment ; `(locked)` : x4 imposé à quatre joueurs (voir [Multiplicateur d'ennemis et civils](#multiplicateur-dennemis-et-civils)) |
| `Civilians : ON` | des civils se mêlent aux ennemis ; en tuer un fait échouer la mission |
| `Team kill` | sur `ON`, les tirs, les coups de mêlée et les prises d'un joueur atteignent les autres, entre tous les joueurs |

| `Online damage` | sur `ON`, dégâts « en ligne » : les PNJ font aux joueurs les dégâts que `onimp_difficulty.ini` donne pour le nombre de clients présents, 5 % de plus par client si le fichier n'a pas été modifié. Sur `OFF`, dégâts normaux |

| `Auto-play : ...` | **client seulement** (chez l'hôte le bouton affiche `(client)` et un clic l'explique). Sur `ON`, le personnage du client suit l'hôte et se bat seul (voir [Jeu automatique](#jeu-automatique-dun-client)) |

Sous ces boutons, chez l'hôte seulement, deux curseurs règlent ce que chaque client ajoute aux PNJ,
sans quitter le jeu ni toucher à un fichier (depuis la 0.18) :

| Curseur | Ce qu'il règle |
|---|---|
| `NPC health : +100 % per client` | la vie que chaque client connecté ajoute aux PNJ, en pour cent de leur vie d'origine. De +0 à +500, par crans de 10. À +100 : vie doublée avec un client, triplée avec deux, quadruplée avec trois. À +0 : vie d'origine |
| `NPC damage : +5 % per client` | les dégâts que chaque client ajoute aux coups des PNJ sur les joueurs. De +0 à +200, par crans de 5. À +100 : dégâts doublés avec un client |

Le curseur se déplace à la souris. L'étiquette au-dessus dit la valeur ; les PNJ en jeu et leurs
jauges suivent pendant le déplacement, chez l'hôte comme chez les clients. Une demi-seconde après
le dernier mouvement, ou à la fermeture du menu, les valeurs sont écrites dans
`onimp_difficulty.ini` : elles sont encore là au prochain lancement. Déplacer `NPC damage` remet
`Online damage` sur `ON`, sans quoi il ne servirait à rien ; avec `Online damage` sur `OFF`,
l'étiquette finit par `(off)`. Si le fichier a été écrit à la main avec des valeurs qui ne montent
pas du même pas à chaque client, l'étiquette montre les trois (`NPC health : 150 / 300 / 400 %`),
et déplacer le curseur les remplace.

Un troisième curseur, `Alert volume : 50 %`, est là chez l'hôte **et chez chaque client** (depuis
la 0.27) : le volume du son joué quand un joueur tombe à terre, de 0 à 100 % par crans de 5. Il ne
règle que la machine où on le déplace. Le son est joué une fois quand le curseur s'arrête, pour
juger du volume ; à 0 il est coupé.

Depuis la 0.27 tous les réglages de cet onglet sont gardés dans `onimp.ini` et repris au lancement
suivant, `Team kill` compris (il était coupé à chaque lancement jusqu'à la 0.26). `God mode` et
`Unlimited ammo`, qui étaient dans cet onglet jusqu'à la 0.34, sont depuis la 0.35 dans l'onglet
`Cheat`, où chaque joueur règle les siens. `Online damage` ne touche qu'aux dégâts ; la vie
des PNJ suit le nombre de clients dans les deux cas (voir « PNJ pilotés par l'hôte »). Les clients
suivent ces réglages sans pouvoir les changer, et oublient le tir ami quand l'hôte s'en va. Changés
dans ce menu, ils partent vers les clients sans attendre que l'hôte le referme. Ce sont les mêmes
réglages que `mp_difficulty`, `mp_enemies`, `mp_civilians`, `mp_teamkill` et `mp_onlinedamage`.

**Cheat**, **Character** et **Spawn** (depuis la 0.35) : trois onglets dans la colonne de droite,
sous le bouton `Close`, que seuls voient l'hôte et les clients à qui il en a ouvert l'accès. Ils
sont décrits dans [Cheats, personnage incarné, apparitions](#cheats-personnage-incarné-apparitions).

**Display**, chez l'hôte et chez le client : ce que chacun voit sur son écran.

| Bouton | Effet quand il est sur `ON` |
|---|---|
| `Player health` | la santé de chaque autre joueur s'affiche au-dessus de lui, en barre sans chiffres |
| `NPC health` | la santé des PNJ s'affiche au-dessus d'eux |
| `Messages` | les messages du mod (`OniMP : ...`) s'affichent à l'écran |
| `NPC IDs` | l'identifiant de chaque PNJ s'affiche au-dessus de sa jauge |
| `Weapon IDs` | l'identifiant de chaque arme au sol s'affiche au-dessus d'elle |
| `Player ammo` | les munitions de chaque autre joueur s'affichent au-dessus de sa jauge |
| `Ping` | la latence de chaque autre joueur s'affiche à côté de son pseudo, en millisecondes |
| `Skin : ...` | notre tenue ; chaque clic passe à la suivante que personne d'autre ne porte |
| `Language : ...` | langue des textes du mod, `Francais` ou `English` ; chaque clic passe à l'autre, et tout ce qui est affiché suit aussitôt |
| `Wallhack (ESP)` | sur `ON`, jauge, munitions, pseudo et panneau « à terre » des autres joueurs se voient à travers les murs ; sur `OFF`, seulement quand le joueur lui-même est en vue |
| `Low portraits` | sur `ON`, les portraits des personnages qui parlent pendant les dialogues entrent en bas de l'écran, à gauche ou à droite, juste au-dessus des sous-titres ; sur `OFF`, en haut comme dans le jeu d'origine |

Sous ces boutons, une ligne `Name` avec un champ de saisie : cliquer dedans, taper son pseudo.

Les sept premiers réglages sont actifs par défaut, ne concernent que la machine où ils sont changés et
sont gardés dans `onimp.ini`. Ce sont les mêmes que `mp_playerhealth`, `mp_npchealth`,
`mp_messages`, `mp_npcids`, `mp_weaponids`, `mp_playerammo` et `mp_ping`.

### Ce que le menu garde d'un lancement à l'autre

Tout ce qui se change dans les onglets Coop et Display, chez l'hôte comme chez un client, est écrit
dans `onimp.ini` au moment du changement et repris au lancement suivant (depuis la 0.27 sans
exception).

| Réglage | Qui | Où |
|---|---|---|
| `Difficulty`, `Enemies`, `Civilians`, `Team kill`, `Online damage` | l'hôte | `[game]` |
| `Cheats` (interrupteur général) et accès de chaque client | l'hôte | `cheats` et `cheat_access` sous `[game]` |
| boutons et curseurs de l'onglet Cheat, personnage incarné, touche « personnage suivant » | chacun | `[cheats]` pour l'hôte, `[cheats_client]` pour un client |
| curseurs `NPC health` et `NPC damage` | l'hôte | `onimp_difficulty.ini` |
| curseur `Alert volume` | chacun | `[audio]` pour l'hôte, `[audio_client]` pour un client |
| `Auto-play` | un client | `[game_client]` |
| boutons de l'onglet Display, `Language` et `Wallhack (ESP)` compris | chacun | `[display]` pour l'hôte, `[display_client]` pour un client |
| pseudo et tenue | chacun | `name_host`, `skin_host`, `name_client`, `skin_client` sous `[player]` |

Un client ne garde pas les règles de la partie : ce sont celles de l'hôte, qu'il reçoit à chaque
connexion. Sur une seule machine, l'hôte et le client lisent le même `onimp.ini` : c'est pour cela
que chacun a ses sections, et que l'un ne défait pas ce que l'autre a réglé (jusqu'à la 0.26 ils
partageaient `[display]`, et le dernier à changer un bouton récrivait toute la section avec ses
propres valeurs). Un client qui n'a encore rien réglé lui-même part de ce que dit la section de
l'hôte.

**Le fichier lu par le jeu est celui du dossier du jeu**, `AE\onimp.ini`. `OniMP\onimp.ini` n'est
que le modèle qu'`install.bat` copie quand le jeu n'en a pas encore : le modifier ne change rien à
une partie.

### Pseudos, tenues, latence

Depuis la 0.25, au-dessus de chaque autre joueur, de bas en haut : sa barre de vie, ses munitions,
puis son pseudo suivi de sa latence (`PSEUDO 42MS`), dans la couleur de son numéro.

- **À travers les murs.** Ces trois éléments restent lisibles quand le joueur est derrière un mur,
  un étage plus bas ou à l'autre bout du niveau : ils sont tracés par-dessus le décor, à leur place
  à l'écran. On sait ainsi toujours où sont les trois autres. Les jauges des PNJ, elles, restent
  cachées par les murs.
- **Pseudo.** F1, onglet Display, ligne `Name` : cliquer dans le champ et taper, quinze signes au
  plus (lettres sans accent, chiffres, espace, `_`, `-`, `.`). Il est pris une seconde après la
  dernière frappe, ou à la fermeture du menu, écrit dans `onimp.ini` (`name_host` ou `name_client`
  sous `[player]`, selon le rôle de la machine) et dit aux autres aussitôt. Il est encore là au
  lancement suivant. Sans pseudo : `HOST`, `P1`, `P2`, `P3`. La commande `mp_name "pseudo"` fait de
  même. Les pseudos sont écrits en capitales.
- **Latence.** L'aller-retour entre ce joueur et l'hôte, en millisecondes, mesuré une fois par
  seconde et lissé. Au-dessus de l'hôte, vu d'un client : son propre aller-retour avec lui. Sur une
  seule machine elle vaut le temps d'une image ou deux.
- **Tenues.** Sept tenues de Konoko : celle du chapitre en cours (`Chapter outfit`, celle que le
  niveau lui donne) et les six autres que l'Anniversary Edition range dans les données communes
  (`K3`, `K4 A`, `K4 F`, `K4 G`, `K4 K`, `K4 L`). L'hôte a par défaut celle du chapitre. Un joueur
  qui arrive reçoit de lui-même la première tenue libre, donc une autre que celles des joueurs déjà
  là. Le bouton `Skin` passe à la tenue suivante que personne ne porte : une tenue prise est sautée,
  et `mp_skin N` la refuse en le disant. C'est l'hôte qui tranche et le redit à tous : la nouvelle
  tenue se voit chez les quatre joueurs dans la seconde. Le choix est gardé dans `onimp.ini`.

### Langue : français ou anglais

Depuis la 0.26, chaque joueur choisit la langue des textes du mod : F1, onglet Display, bouton
`Language` (en français : onglet `Affichage`, bouton `Langue`). Français par défaut.

- **Ce qui change, tout de suite, sans rien relancer** : les messages du mod à l'écran, les
  onglets, boutons, curseurs et étiquettes du menu F1 (le menu ouvert est récrit sous les yeux), le
  panneau d'un joueur à terre (`A TERRE` / `DOWN`, `CTRL POUR RANIMER` / `HOLD CTRL TO REVIVE`).
- **Comment** : chaque texte est écrit une seule fois dans le code, et un dictionnaire
  (`src/lang.cpp`, 168 entrées) donne l'autre langue au moment où il s'affiche. Un format est
  traduit avec les mots qu'on lui donne (`OniMP : tir ami active` / `OniMP : team kill enabled`).
  Un texte que le dictionnaire ne connaît pas s'affiche tel quel.
- **Gardé** dans `onimp.ini` : `language = fr` ou `en`, sous `[display]`. Commande : `mp_language
  "en"`. Chaque machine a la sienne : l'hôte peut jouer en français et un client en anglais.
- **Ce qui ne change pas** : le journal (`onimp_*.log`) et `mp_status`, qui restent en français
  pour que les lignes à chercher soient les mêmes chez tous ; l'aide des commandes `mp_*`,
  enregistrée au lancement ; les textes du jeu lui-même, auxquels le mod ne touche pas.
- **En français, les libellés du menu sont raccourcis** pour tenir dans les boutons : `Vie des
  joueurs`, `Vie des PNJ`, `Noms des PNJ`, `Noms des armes`, `Munitions joueurs`, `Latence`,
  `Tenue`, `Langue`, `Vision murs (ESP)`, `Pseudo` ; onglet Coop : `Difficulte`, `Ennemis`,
  `Civils`, `Tir ami`, `Invincibilite`, `Degats en ligne`, `Munitions infinies`, `Volume alerte`. Sans accents : la police des menus ne
  les a pas tous.

### Jeu automatique d'un client

Depuis la 0.28, un client peut confier son personnage au mod : F1, onglet Coop, bouton
`Auto-play` (`Jeu auto`). Réservé aux clients ; gardé dans `onimp.ini` ; commande `mp_autoplay 0`
/ `1`. Le menu, la console et les captures d'écran restent au joueur, qui garde de quoi le couper.
Refait en 0.29, priorités revues en 0.30, ligne de vue, butin et sprint en 0.32, ramassage des
bonus et conditions du butin en 0.33. En 0.34 : le butin avant le combat, l'arme rangée ou sortie
selon qu'il y a un ennemi, le recul devant un ennemi qui approche, le rechargement de l'arme vide
seulement, et l'arme de Barabas permise.

**Depuis la 0.35 : corps à corps seulement.** Le jeu automatique ne se bat plus qu'avec ses poings
et ses pieds (deux coups de poing puis un coup de pied, balayage pour un ennemi au sol). Cela
renverse ce qui avait été demandé de la 0.30 à la 0.34 :

- **il ne tire plus** : le point 4 ci-dessous (tirer en gardant ses distances, reculer, recharger)
  n'est plus jamais pris. Un ennemi vu jusqu'à 90 unités est rejoint au sprint et frappé ;
- **il ne sort plus son arme** : celle qu'il porte au début du niveau, ou qu'une prise lui met en
  main, est rangée aussitôt (`jeu automatique : range 'w2_sap' (il se bat a mains nues)`), et n'en
  ressort pas. L'arme de Barabas, qui ne se range pas, est lâchée ;
- **il ne ramasse plus ni arme ni munitions**, et ne les cherche plus du regard : c'étaient ses
  allers et retours vers un chargeur ou une arme, et ses hésitations entre butin et combat, qui le
  bloquaient. Du point 2 il ne reste que les **hypos** : celui dont il a besoin tout de suite
  (santé sous 50 %, aucun sur lui) à tout moment, les autres seulement quand plus aucun ennemi
  n'est là depuis une seconde. Cette réserve, que le plein d'hypos attende le calme, n'était pas
  dans la demande : c'est un choix fait en l'écrivant, pour qu'un hypo au sol ne le sorte pas d'un
  combat ;
- le reste est inchangé : ranimer d'abord, suivre l'hôte par sa trace, se soigner et prendre l'aura
  à l'hypo, sprinter, se débloquer.

Le code du tir est resté en place, derrière un réglage du source (`kMeleeOnly`, `autoplay.cpp`) : le
remettre à faux et recompiler rend le comportement de la 0.34. `mp_status` dit `en marche (corps a
corps seulement)`. La suite décrit l'ordre complet, tel qu'il valait en 0.34 :

1. **Ranimer.** Dès qu'un joueur est à terre, l'hôte ou un autre client, il laisse tout, va à lui
   et le ranime : il s'arrête à 12 unités, et la réanimation commence dès 20 (elle porte à 26).
   Pendant qu'il ranime il ne frappe pas. Seule exception : sa propre santé sous 20 % avec un
   ennemi au contact, qu'il traite d'abord. Comme pour un joueur, un coup d'ennemi reçu interrompt
   la réanimation, qui est reprise aussitôt.
2. **Ramasser ce qui traîne** (revu en 0.31, 0.33 et 0.34), à 120 unités au plus, au même étage, et
   s'il le voit. Depuis la 0.34 **le butin passe avant le combat** : dès qu'il voit quelque chose à
   prendre il change de route, ennemi en vue ou non (en 0.33 il n'allait faire le plein qu'au
   calme). L'ordre : un hypo, quand sa santé est sous 50 % et qu'il n'en porte pas ; une arme qui
   serve, quand la sienne ne sert plus ou qu'il n'en a pas ; des recharges pour son arme, quand il
   n'en porte plus ; une arme plus forte que la sienne ; puis le plein, le plus proche d'abord :
   chargeurs (balles, rouges) et cellules (énergie, vertes) quelle que soit son arme, et hypos,
   jusqu'à ce que l'inventaire soit plein (douze chargeurs, douze cellules, six hypos ; six de
   chaque si la contenance n'a pas été relevée).
   **Une arme** : la plus forte de celles du sol qui servent, si elle vaut mieux que la sienne ; il
   lâche la sienne en arrivant (rangée, il la sort d'abord : le moteur ne lâche que l'arme en
   main). Une arme « sert » si elle est chargée ou s'il porte de quoi la recharger. L'ordre de force
   est celui du tableau plus bas. **Pénurie** : une arme vide et sans recharge ne vaut plus rien, et
   il prend alors ce qu'il trouve, même un pistolet ; il reprendra la forte s'il retrouve de quoi
   la recharger. **L'arme de Barabas** (`w11_ba1`) lui était interdite de la 0.31 à la 0.33 ;
   depuis la 0.34 il la prend dès qu'elle est au sol, et c'est pour lui la plus forte. Les deux
   autres armes de boss (`w10_sni`, `w12_ba2`, 9999 tirs), que le joueur n'est pas censé tenir,
   restent interdites : il ne les ramasse pas, et si une prise lui en met une en main il la lâche
   aussitôt (`jeu automatique : lache 'w12_ba2', qu'il ne doit pas tenir`).
   **Des munitions**, des deux sortes : cellules d'énergie et chargeurs de balles (le drapeau
   `UsesCells` de la classe d'une arme dit lesquelles elle prend).
   Ce qu'il n'arrive pas à prendre (inventaire plein, objet hors d'atteinte, chemin barré) est
   laissé dix secondes, puis trois fois plus longtemps à chaque nouvel échec au même endroit,
   jusqu'à une minute : un objet hors d'atteinte ne doit pas le détourner d'un combat toutes les dix
   secondes.
3. **Corps à corps, ennemi collé à lui.** Arme en main ou non, dès qu'un ennemi est à moins de 20
   unités. Debout : il avance jusqu'à 9 unités et frappe en deçà de 11,5, deux coups de poing puis
   un coup de pied. **Au sol** : il s'approche à 7 unités et balaie (accroupi + pied) en deçà de
   9,5, toutes les 1,7 s (voir plus bas).
4. **Tirer, en gardant ses distances.** Armé, il prend pour cible l'ennemi le plus proche qu'il
   voit, jusqu'à 200 unités, au même étage (moins de 14 unités d'écart en hauteur) : il se tourne
   vers lui, vise son buste (ou son corps s'il est au sol) et tient la gâchette. **Il ne tire pas
   quand un coéquipier est à moins de 7 unités de la ligne de tir** : le tir ami peut être activé.
   Une arme dont le tir explose (`w5_sbg`, `w11_ba1`) n'est pas tirée à moins de 80 unités.
   **Il n'avance jamais vers sa cible, et recule quand elle approche** (0.34) : ennemi à moins de
   50 unités (90 avec une arme dont le tir explose), il tient « reculer » tout en tirant. Avant
   chaque pas en arrière il regarde derrière lui, par deux rayons du moteur, dix fois par seconde :
   l'un à hauteur de taille sur dix unités (un mur, un objet, un personnage), l'autre de là-bas
   vers le bas (pas de sol à moins d'un étage : le vide). S'il n'a pas bougé d'un pas en une
   demi-seconde alors qu'il recule, il tient sa place deux secondes (`jeu automatique : recul
   bloque...`). Reculer ne le met pas hors d'atteinte : d'après les animations, un ennemi court à
   53 unités par seconde, Konoko recule à 42 avec un pistolet et à 36 avec un fusil. Cela lui
   donne deux à trois secondes de tir de plus ; puis l'ennemi le serre, et c'est le point 3.
   **Rechargement** (0.34) : jamais par avance. Une seule condition : l'arme vide, zéro cartouche,
   et de quoi la recharger dans l'inventaire (`jeu automatique : 'w2_sap' vide, il recharge (2
   recharge(s) dans l'inventaire)`). Rechargée, il reprend le tir. Vide et sans recharge, elle est
   rangée (voir plus bas) et il passe au point 5.
5. **Corps à corps, sans arme qui serve.** Contre l'ennemi le plus proche de lui (90 unités) ou à
   défaut de l'hôte (70), de la même façon qu'au point 3. C'est aussi ce qu'il fait d'un ennemi
   qu'il vient de perdre de vue, ou quand le tir de son arme explose, que l'ennemi est trop près
   pour tirer et qu'il n'a pas la place de reculer.
6. **Suivre l'hôte.** À plus de 45 unités de lui, ou à un autre étage, il le rejoint et s'arrête
   à 25. De près (moins de 60 unités, même étage) il va en ligne droite. Sinon, ou dès que la
   ligne droite bute, il suit **la trace de l'hôte** : un point noté tous les dix pas de l'hôte,
   96 au plus, rejoints dans l'ordre. C'est un chemin que le niveau permet, puisqu'un joueur vient
   de le faire à pied : portes, couloirs, escaliers. Repasser près d'un point plus récent fait
   sauter les détours de l'hôte.

À côté de cet ordre, deux choses qui ne l'arrêtent pas, parce que ce ne sont que des touches :

- **Se soigner, prendre l'aura.** Un hypo dès que sa santé passe sous 85 %. Et en combat, un de
  plus en pleine forme : la santé passe au-dessus du maximum, ce qui donne l'aura du personnage
  surpuissant et ses coups plus forts. Un hypo à la fois, quatre secondes entre deux. Hors combat
  il ne prend pas d'hypo pour l'aura, qui s'éteindrait avant de servir (journal : `jeu
  automatique : sante a N sur M, un hypo pour l'aura`). Depuis la 0.31 il regarde une demi-seconde
  après si l'hypo a bien été pris ; sinon il n'en redemande pas avant vingt secondes (`l'hypo
  demande n'a pas ete pris`). Le moteur le refuse pendant un rechargement, et quand la santé
  dépasse déjà le maximum.
- **L'arme, en main ou rangée (0.34).** Trois règles. **A** : un ennemi est à combattre, en vue ou
  à portée : il sort son arme, si elle sert. **B** : plus d'ennemi depuis une seconde : il la
  range. **C** : elle est vide et il n'a pas de quoi la recharger : il la range aussitôt, ennemi
  ou non, et se bat à mains nues jusqu'à ce qu'il ait retrouvé des recharges. Ranger sert à aller
  vite : dans les données du jeu, seule l'animation de course sans arme a une variante « sprint »
  (78 unités par seconde), et une animation d'arme passe avant elle ; arme en main, Konoko court
  à 53 unités par seconde avec un pistolet, à 48 avec un fusil. Les gestes sont ceux de la touche
  d'échange d'arme quand il n'y a rien à ramasser, par les routines qu'elle appelle (`0x4EDD20`
  pour ranger, `0x4EDEB0` pour sortir) : le moteur joue l'animation, et l'arme change de place à sa
  frame d'action. Journal : `jeu automatique : range 'w2_sap' (plus d'ennemi en vue)`, `range
  'w2_sap' (vide et sans recharge)`, `sort 'w2_sap' (ennemi a combattre)`. **L'arme de Barabas ne
  se range pas** (drapeau de sa classe, `0x4EDD7B`) : il la garde en main tant qu'elle sert, donc
  sans sprint, et la lâche quand elle est vide et sans recharge (`lache 'w11_ba1', vide et sans
  recharge : elle ne se range pas`).

**Ligne de vue (0.32).** Avant de tirer, d'aller chercher un objet ou de couper tout droit vers
l'hôte, il tire un rayon de ses yeux (13 unités au-dessus de sa place) jusqu'à ce qu'il regarde : le
buste d'un ennemi, le sol d'un objet. C'est le rayon du moteur, celui du viseur laser (`0x4D4E50`) :
il rend le premier obstacle, personnage, objet du niveau ou décor. La voie est libre si le rayon
touche la cible même, ou si l'obstacle est à moins de 6 unités d'elle (derrière elle, il ne la cache
pas) ; un coéquipier sur la ligne la bouche, un autre ennemi non. Chaque ennemi est regardé six fois
par seconde. Ce qui en découle :

- il ne tire que sur un ennemi qu'il voit ; jusqu'à la 0.31 il vidait son chargeur dans un mur dès
  qu'un ennemi se trouvait à moins de 200 unités, ce qui se voyait surtout à la fin des
  cinématiques, où le niveau fait apparaître des ennemis au loin ;
- sa cible est le plus proche des ennemis qu'il voit, et non plus le plus proche tout court ;
- un ennemi perdu de vue depuis moins de trois secondes est poursuivi à pied, sans tir ; au contact
  (20 unités) un ennemi compte toujours, vu ou non ;
- un objet caché par le décor est laissé, et regardé de nouveau quatre secondes plus tard ;
- vers l'hôte, la ligne droite n'est prise que s'il le voit ; sinon c'est la trace.

**Butin tenu jusqu'au bout (0.32).** L'objet qu'il va chercher est choisi une fois, puis gardé
jusqu'à ce qu'il l'ait, qu'un autre le prenne ou qu'il y renonce : dix secondes pour l'atteindre,
deux secondes et demie sans s'en approcher. Jusqu'à la 0.31 le choix était refait à chaque tick : au
moindre saut l'objet n'était plus « au même étage », il repartait vers l'hôte, puis revenait
(journal de la 0.31 : `va chercher des munitions` et `rejoint l'hote` en alternance, une fois par
seconde pendant quarante secondes). Journal : `jeu automatique : des munitions en vue a 42 unites,
il y va`.

**Ramasser un bonus (0.33).** Le moteur ne donne jamais un bonus à qui passe dessus. Il n'y a
qu'un chemin, lu dans l'exécutable : la touche d'échange d'arme (`0x4F74CD`). À cet appui, il
cherche le bonus que le personnage peut encore porter à moins de 8 unités à plat, entre 3 unités
sous ses pieds et 18 au-dessus (`0x4FA4B0`, `0x51B3E0`) ; s'il y en a un, il le donne tout de suite
quand l'animation en cours le permet (`0x4EDA10`), sinon il joue l'animation « ramasser », et le
bonus est donné à sa frame d'action (`0x4DDD07`). Sans bonus ni arme à portée, la même touche range
l'arme. Le jeu automatique, lui, allait se poster sur le bonus et attendait : rien ne venait, et il
y renonçait au bout de deux tiers de seconde. C'est ce que montrent tous ses journaux : en 0.32,
dans le laboratoire, neuf fois `des munitions en vue` et neuf fois `renonce a ce qu'il allait
chercher (dessus sans le prendre)` ; dans l'entrepôt, six hypos et une recharge visés, cinq
renoncements, aucun bonus pris. Les quatre bonus que l'hôte lui a attribués en 0.31 et en 0.32
n'étaient pas à lui non plus (voir « Armes et bonus au sol », le pantin qui ramassait le bonus
voisin). Les armes, elles, étaient prises : pour elles le mod appelait déjà la routine du moteur.
Depuis la 0.33 il fait de même pour un bonus : arrivé à moins de 6 unités, il appelle la routine que
la touche appelle, aux conditions du moteur (distance, hauteur, place dans l'inventaire), dix fois
par seconde au plus. Journal : `jeu automatique : chargeur ramasse ; il porte 3 chargeur(s), 0
cellule(s), 1 hypo(s)` (ou `cellule d'energie ramassee`, `hypo ramasse`), puis chez l'hôte `bonus de
type 0 ramasse par le joueur 1`. Si le moteur refuse : `renonce a ce qu'il allait chercher (le
moteur refuse de le lui donner)`. Vu à l'œuvre dans le journal du premier essai de la 0.33
(laboratoire, dix-neuf minutes) : trois hypos et deux chargeurs ramassés par lui, chacun
confirmé chez l'hôte ; un seul refus, pour une recharge qu'un ennemi venait de laisser, posée
depuis un quart de seconde et que l'hôte déplaçait encore. Depuis la 0.34 la place d'un bonus
est suivie jusqu'à ce qu'il le prenne.

**Les armes du jeu**, telles que leurs classes (`ONWC`) les décrivent. « Tirs » : ce que contient
une recharge. « Distances » : celles entre lesquelles l'IA du jeu tire avec cette arme. « Rang » :
celui que le jeu automatique lui donne, 10 pour la plus forte.

| Classe | Arme | Recharge | Tirs | Distances de l'IA | Rang |
|---|---|---|---|---|---|
| `w11_ba1` | arme de Barabas, tir explosif, ne se range pas | cellule | 20 | | 10 (depuis la 0.34) |
| `w9_scr` | canon hurleur | cellule | 6 | 80 à 350 | 9 |
| `w7_scc` | canon Scram | chargeur | 5 | 18 à 200 | 8 |
| `w5_sbg` | lance-superballes, tir explosif | chargeur | 4 | à partir de 70 | 7 |
| `w3_phr` | fusil à plasma | cellule | 10 | à partir de 16 | 6 |
| `w8_mbo` | arc à mercure | chargeur | 2 | à partir de 12 | 5 |
| `w2_sap` | pistolet-mitrailleur | chargeur | 30 | 12 à 450 | 4 |
| `w1_tap` | pistolet | chargeur | 10 | 12 à 900 | 3 |
| `w4_psm` | projecteur de phase | cellule | 300 | 20 à 180 | 2 |
| `w6_vdg` | Van de Graaff | cellule | 5 | 16 à 50 | 1 |
| `w10_sni`, `w12_ba2` | armes de boss | cellule | 9999 | | jamais prises |

Des neuf armes ordinaires, cinq prennent des chargeurs et quatre des cellules : porter les deux
sortes, c'est pouvoir recharger n'importe laquelle. Le rang reste l'ordre dans lequel le jeu donne les armes ; la
puissance d'un tir tient aux projectiles, et n'est pas écrite dans la classe.

**Sprint (0.32).** Dès qu'il a plus de 12 unités à faire, il sprinte, vers l'hôte, un objet, un
joueur à terre ou un ennemi encore loin. Le jeu n'a pas d'endurance : seuls un coup, un tir ou un
but tout proche le ramènent à la course. Chez un joueur le sprint vient d'un double appui sur
« avancer » en moins de quinze ticks (`0x4DDF01`) ; le jeu automatique pose directement l'état que
ce double appui donne, et le retire quand il doit ralentir.

**Devant un ennemi au sol (0.32).** Le corps couché l'arrêtait à une dizaine d'unités, plus loin que
la distance où il frappait : il courait sur place contre lui. Quand il ne s'approche plus depuis une
demi-seconde, il cesse d'avancer et frappe de là, sept unités plus loin qu'à l'ordinaire.

**Quand il n'avance pas** (moins de 3 unités en une demi-seconde alors qu'il le veut) : un saut au
bout d'une seconde ; au bout de deux, un autre chemin : la trace à la place de la ligne droite
(pour cinq secondes), le point suivant de la trace, ou l'ennemi laissé de côté quatre secondes si
c'est lui qu'un obstacle sépare ; au bout de sept, en dernier recours, il est replacé à côté de
l'hôte. De même à plus de 900 unités de lui. Il ne voit pas les murs : c'est de ne plus avancer
qui le lui apprend. Le moteur a son propre calcul de chemin (les volumes « BNV » du niveau reliés
en graphe, puis une grille par salle), mais il appartient à l'état d'une IA, et ce personnage est
celui d'un joueur : la trace s'en passe.

**Coups au sol.** Mesuré dans les données du jeu : 84 coups relevés dans les animations de Konoko,
toutes variantes d'arme comprises (`TRAM`, champ `Extents` : portée et hauteur du coup à chaque
image). Les coups de poing et de pied qu'elle enchaîne debout passent à 9 unités de haut ou plus :
au-dessus d'un corps couché. Ceux qui descendent au sol :

| Coup | Touches | Dégâts | Descend à | Portée | Durée |
|---|---|---|---|---|---|
| Roulade sautée (`KONOKOjump_crouch`) | saut, puis accroupi en l'air | 10 | 0,4 | 7,2 | 3,7 s |
| Coup de pied lourd (`KONCOMkick_heavy`) | pied ; ce qui le déclenche n'est pas établi | 10 | sol | 6,9 | 2,7 s |
| **Balayage** (`KONCOMkick_low1`) | accroupi + pied, depuis debout | 5 | 1,1 | 10,6 | 1,6 s |
| Glissade (`KONOKOrun_slide`) | accroupi en courant | 5 | 0,2 | 10,0 | 1,4 s |
| Coup de pied accroupi (`KONCOMcrouch_kick1`) | pied, déjà accroupi | 8 | 3,8 | 10,2 | 1,5 s |

Le jeu automatique emploie le balayage : 3,2 points par seconde contre 2,7 pour la roulade, la
plus longue portée, et il s'enchaîne sans élan. Les deux touches sont enfoncées au même tick,
depuis la station debout : accroupi d'abord, c'est le coup de pied accroupi qui part, et il passe
plus haut. La roulade et la glissade demandent un élan réglé à l'image près ; elles ne sont pas
employées. Le dernier coup de l'enchaînement poing, poing, pied (`KONCOMcomb_p_p_k`, 20 points)
descend à 3 unités : il peut toucher un ennemi en train de tomber.

**D'où viennent ces règles.** L'IA du jeu range le combat de chaque classe de personnage dans une
table : un comportement par distance (de loin, à mi-distance, de près), un autre « sans arme », et
des distances de tir propres à chaque arme. Ces dernières se lisent dans les données (classes
d'armes `ONWC`, `MinShootingDistance`) : aucune arme ne se tire à moins de 12 à 20 unités, 70 pour
le lance-superballes (`w5_sbg`) et 80 pour le canon hurleur (`w9_scr`). Le jeu automatique reprend
cet ordre, tirer de loin, mêlée de près et faute d'arme, avec un seuil de 20 unités, et met devant
ce que l'IA du jeu n'a pas : ranimer un coéquipier.

- **Suspendu** pendant une cinématique, quand son joueur est à terre ou mort, hors session.
- **Journal** : une ligne quand il change d'occupation, une par seconde au plus (`jeu automatique :
  va ranimer un joueur a terre`, `frappe un ennemi au sol`, `rejoint l'hote par ou il est passe`),
  et une par blocage (`bloque en ligne droite, suit la trace de l'hote (N point(s))`, `bloque
  devant '...', laisse quatre secondes`).
- **Comment** : chaque tick, le moteur donne au personnage du joueur les touches du moment ; le mod
  remplace à cet instant les actions de jeu par celles qu'il a choisies, et tourne le personnage
  lui-même. Tout le reste (coups, santé, armes, réseau) suit les chemins ordinaires : pour les
  autres machines, c'est un joueur comme un autre. Journal : `jeu automatique : notre joueur
  replace a cote de l'hote (...)` ; `mp_status` dit ce qu'il fait.

### Lasers, tourelles, alarmes

Avec des PNJ partagés (options `npcs` ou `quests`), les défenses d'un niveau sont communes à tous
les joueurs, dans tous les niveaux : rien n'est écrit niveau par niveau, tout passe par le moteur.
Trois règles depuis la 0.39 :

1. **chaque machine détecte son propre joueur**, sur les lasers tels qu'elle les affiche, et dit
   aux autres lequel il vient de couper ; l'hôte détecte aussi le pantin de chaque client sur les
   siens, comme avant : le premier des deux qui le voit l'annonce ;
2. **l'hôte décide de la suite** : lasers et tourelles allumés ou éteints, cible de chaque
   tourelle, position des rayons, vague d'une alarme ;
3. **les dégâts sont infligés là où le joueur visé joue** : c'est sa machine qui fait tirer ses
   propres tourelles sur lui.

**Ce qui n'allait pas dans le jeu d'origine.** Lu dans l'exécutable : un laser (objet `TRIG` du
niveau) ne regarde que s'il coupe le joueur de la partie (`0x4CBA19`), et aucun autre personnage.
Ce qu'il déclenche (tourelles allumées, script, alarme) ne se passe que sur la machine où il a été
coupé, et une tourelle allumée vise le plus proche des personnages, pantin d'un autre joueur
compris, que rien ne blesse. À plusieurs, chaque machine avait donc ses propres défenses, qui ne
réagissaient qu'à son joueur et tiraient sur n'importe qui.

**Ce qui vaut maintenant.**

- **Qui coupe un laser : chaque machine pour son joueur (0.39).** De la 0.36 à la 0.38 c'était
  l'hôte qui testait ses lasers contre le pantin de chaque client, et les lasers d'un client ne
  détectaient plus personne. Cela marchait dans la salle du Deadly Brain de l'usine, vu en jeu en
  0.38 ; ailleurs, « quand le client passe sur les lasers rien ne marche ». Deux causes. La
  première se lit dans les journaux de cet essai : la cible des tourelles (plus bas). La seconde
  est déduite du moteur et des données des niveaux, **sans avoir été vue dans la mémoire des deux
  jeux**, fermés entre-temps. Presque tous les lasers glissent le long d'un rail (288 sur les 291
  du jeu), et le moteur ne règle leur position sur aucune horloge : un rayon avance d'un pas par
  tick tant que son laser est allumé (`0x4CB19C`, `0x4CAFC0`), sur chaque machine pour son compte.
  Un client qui recharge le niveau seul, comme dans cet essai, ou qui arrive en cours de partie,
  n'a donc plus ses rayons là où l'hôte a les siens, pour tout laser allumé avant son arrivée ; et
  165 lasers le sont dès le chargement. L'hôte testait alors le pantin du client contre des rayons
  que le client voyait ailleurs : il traversait un rayon sans rien déclencher, et se faisait
  prendre là où il n'en voyait aucun. Dans la salle du Deadly Brain, onze lasers à deux et quatre
  rayons balaient toute la pièce : on y est pris où que soient les rayons, et le défaut ne se
  voyait pas. (Dans le couloir de l'essai, les lasers 2 sont allumés par un script, au même
  instant chez les deux : le client y a bien été détecté, et c'est la cible qui a manqué.)
  Maintenant chaque machine laisse aussi le moteur tester ses lasers contre son propre joueur,
  comme dans le jeu d'origine : ce que le joueur voit compte. Au même instant elle prévient les
  autres ; chacune fait alors chez elle ce que le moteur fait quand un laser est coupé, son du
  laser et événements, au nom de ce joueur. La détection par l'hôte reste, à côté : c'est elle qui
  a été vue en jeu, salle du Deadly Brain en 0.38 et couloir des lasers 2 à 4 dans le premier jet
  de la 0.39 ; la détection par la machine du joueur, elle, n'a pas encore tourné en jeu. Les deux
  annoncent la même chose, un seul son est joué. L'hôte retient qui a coupé quel laser, et
  prévient les ennemis des environs. Son état fait foi : un laser éteint chez lui reste sans
  suite, même coupé chez un client qui ne l'a pas encore appris. Un joueur à terre ne déclenche
  plus rien : son corps resterait dans le rayon. Message à tous : `OniMP : laser 12 : PSEUDO
  detecte`.
- **Rayons au même endroit chez tous (0.39).** Pour que chacun voie les lasers où les autres les
  voient, et voie l'autre couper un laser là où il le coupe chez lui : toutes les deux secondes,
  et dès qu'un client arrive, l'hôte dit où en est chaque rayon de chaque laser mobile sur son rail
  et dans quel sens il va. Un client y recale les siens quand ils s'en sont écartés de plus que ce
  qu'ils parcourent pendant le trajet du message, et les laisse courir entre deux relevés.
- **Lasers et tourelles alignés (0.36).** Cinq fois par seconde l'hôte relève chaque laser et
  chaque tourelle, allumé ou éteint ; ce qui change part vers les clients, qui allument ou
  éteignent les leurs par les routines des commandes `trig_activate`, `trig_deactivate`,
  `turret_activate` et `turret_deactivate`. Cela vaut quelle que soit la cause : un laser coupé,
  une console utilisée (les quatre du Deadly Brain éteignent leurs défenses chez tous), un script.
  Un client réaligne les siennes une fois par seconde s'il s'en est écarté ; une tourelle qu'un
  laser vient d'allumer chez lui n'est pas éteinte pendant une seconde et demie au nom d'un état
  de l'hôte parti avant qu'il ne l'apprenne.
- **Tourelles : la cible est celle de l'hôte (0.37).** Le moteur fait viser à une tourelle le plus
  proche des personnages vivants des équipes qu'elle vise (`0x4CF7E0`), et la 0.36 laissait chaque
  machine faire ce choix pour son compte. Signalé en jeu, et vu dans la mémoire des deux jeux
  ouverts (usine, salle du Deadly Brain) : chez l'hôte, à terre, la tourelle visait le pantin du
  client ; chez le client, elle visait le pantin de l'hôte, plus proche d'elle que lui. Deux
  cibles, aucun dégât : « les pièges attaquent le client mais il ne prend aucun dégât ».
  Maintenant l'hôte choisit, parmi les joueurs **debout** (un joueur à terre n'est plus visé), et
  dit son choix aux clients à chaque changement. Chaque machine fait viser à sa tourelle le même
  joueur : sur la machine de ce joueur c'est le vrai qui est visé, donc touché ; sur les autres
  c'est son pantin, et l'on voit les tirs partir vers lui.
- **Tourelles : qui l'hôte choisit (0.39).** Lu dans les journaux de la 0.38, usine, couloir du
  laser 2 et de la tourelle 1 : le client coupe le laser, la tourelle s'allume chez les deux, puis
  elle vise **l'hôte** pendant quinze secondes sans un seul tir, et s'éteint. La routine du moteur
  prend le plus proche des joueurs, qu'elle puisse le toucher ou non ; seul, il n'y a qu'un joueur,
  et le défaut n'existe pas. Le choix de l'hôte suit maintenant cet ordre :
  1. **ceux qui viennent de couper un laser** de cette tourelle, depuis moins de cinq secondes, et
     qu'elle peut toucher : le piège s'en prend à qui l'a déclenché. S'ils sont deux, le plus
     proche ;
  2. sinon **le plus proche de ceux qu'elle peut toucher**, d'après le test du moteur lui-même
     (`0x4CF6B0` : tout près, ou sans décor entre eux), celui dont dépend ensuite le tir ;
  3. faute de mieux, le plus proche tout court, qu'elle suit sans tirer.

  La cible du moment est gardée tant qu'une autre n'est pas une fois et demie plus près : une
  tourelle n'hésite plus entre deux joueurs à même distance. Chez le client qui vient de couper le
  laser, sa propre tourelle le prend pour cible sans attendre le choix de l'hôte, qui arrive un
  instant plus tard : certaines ne restent allumées que deux secondes. Vu en jeu dans le premier
  jet de la 0.39, même couloir : le client coupe le laser 2, la tourelle 1 le vise dans les onze
  millisecondes (`vise le joueur 1`), tire chez lui un dixième de seconde plus tard, et il perd
  de la vie (260, 243, 225... 142) ; les tourelles 2 et 3 de même. La règle livrée est celle-là,
  réécrite pour deux joueurs dans les mêmes rayons et vérifiée hors jeu (`onimp_rulestest`).
- **Tourelles : le tir chez un client (0.38).** C'était la cause de « côté client il ne se passe
  absolument rien », signalé en 0.36 puis en 0.37. Lu dans l'exécutable : sa visée faite, une
  tourelle ne tire que si le drapeau « toutes les IA passives » du moteur est nul (`0x4CF65D`) ;
  sinon elle cesse le feu. C'est le drapeau de la commande de script `ai2_allpassive`, que les
  niveaux posent le temps d'une cinématique. Or un client du mod l'applique toutes les cinq
  secondes, pour que ses copies des PNJ de l'hôte ne pensent pas par elles-mêmes. Ses tourelles
  s'allumaient donc avec celles de l'hôte, recevaient la bonne cible, et ne tiraient jamais. Elles
  lisent maintenant un drapeau du mod : hors session et chez l'hôte, celui du moteur, sans rien
  changer ; chez un client, celui de l'hôte, qu'il lui transmet. Une cinématique de l'hôte fait
  donc taire les tourelles chez tous, et elles reprennent chez tous quand elle finit.
- **Ce qu'un laser déclenche (0.38).** Les événements d'un objet du niveau sont de onze genres
  (table du moteur en `0x552098`) : script, tourelle allumée ou éteinte, console activée ou
  désactivée, alarme, laser allumé ou éteint, porte verrouillée ou déverrouillée. Ils sont lancés
  sur chaque machine à l'instant où l'une d'elles annonce le laser coupé (`0x4CBB0A` chez elle,
  `0x4F3CC0` chez les autres), et non par un relevé cinq fois par seconde, qui manquait le passage
  d'un joueur qui court. Le script d'un laser ne fait effet que chez l'hôte, qui en transmet les
  commandes comme celles de tout script de niveau.
- **Lasers à plusieurs rayons.** Un laser a un, deux, trois, quatre ou six rayons. Quand un joueur
  en coupe un sans les couper tous, le moteur se contredit à chaque image : le rayon coupé pose le
  drapeau « coupé » et lance les événements, le rayon suivant le lève (`0x4CBB19`), et tout
  recommence à l'image d'après. C'est le bourdonnement continu d'un laser où l'on reste, dans le
  jeu d'origine aussi. En 0.38 chaque déclenchement partait vers les clients : 500 annonces en six
  minutes, jusqu'à 140 par seconde. Une machine n'en envoie plus que quatre par seconde et par
  laser ; le journal en note une toutes les deux secondes, avec le compte ; les ennemis des
  environs sont prévenus au même rythme.
- **Ennemis.** Le joueur qu'un laser vient de détecter est signalé aux ennemis à moins de 220
  unités de lui : ils vont à lui par le calcul de chemin du moteur. C'est un ajout au jeu
  d'origine, où un laser ne fait que ce que son niveau lui a attaché ; il a été demandé.
- Sans PNJ partagés, chacun a son propre monde : rien de tout cela ne change.

**Tous les niveaux.** Relevé hors jeu, dans les données des quatorze niveaux : 291 lasers dans neuf
d'entre eux (3 dans l'entrepôt, 21 dans l'usine, 23 dans les hangars de l'aéroport, 47 et 93 dans
les deux visites du QG du TCTF, 56, 7, 29 et 12 dans quatre autres), 64 tourelles dans sept.

| | |
|---|---|
| Ce qu'un laser déclenche | une tourelle allumée pour 277 d'entre eux, un script pour 6 (le piège à gaz des hangars, script `gas`, dont toutes les commandes sont de celles que l'hôte transmet déjà), rien pour 8. Aucun ne porte d'alarme, de porte ni de console |
| Mouvement | 288 glissent sur un rail, 3 sont fixes ; 165 sont allumés dès le chargement, les autres par un script ; aucun ne clignote |
| Rayons | un seul pour 263 lasers ; deux ou quatre pour 15 lasers de l'usine, dont les onze de la salle du Deadly Brain ; trois, quatre ou six pour 13 autres |
| Durée d'une tourelle allumée | celle de sa classe : quinze secondes pour la plupart, trois pour celles à arc au mercure (`mbo`), deux pour celles à canon hurleur (`scr`). La rallumer pendant ce temps ne change rien (`0x4CD9DA`) : rester dans le laser ne la retient pas, elle s'éteint, puis le passage suivant la rallume |
| Champ d'une tourelle | murale : 70 degrés de part et d'autre, et de -45 à +20 degrés en hauteur ; au sol ou au plafond : tout le tour |

La chaîne est donc la même partout : laser coupé chez le joueur qui le voit (0.39), tourelle
allumée chez tous (0.36), cible commune (0.37) et bien choisie (0.39), tir chez le joueur visé
(0.38). Le piège à gaz des hangars passe par le script de l'hôte.

**Alarmes.** Une alarme est donnée de trois façons dans les niveaux d'Oni, et le mod les suit
toutes sans liste de consoles à tenir : la commande de script `ai2_tripalarm` (usine, complexe,
centre neurologique, centrale), l'événement « Activate Alarm » d'un laser ou d'une console, et une
console dont un ennemi se sert (`ai2_doalarm`, dans dix niveaux sur quatorze). Les deux premières
mènent à la même routine du moteur (`0x49AC30`), la commande par un appel et l'événement par un
saut : depuis la 0.38 c'est l'entrée de la routine qui est détournée, et l'alarme d'un laser
compte aussi (la 0.36 ne voyait que la commande). Chez l'hôte, chaque alarme amène une **vague** :

- **trois ennemis par joueur présent**, hôte compris : 3 seul, 6 à deux, 12 à quatre. Ceux d'une
  vague précédente encore en vie sont décomptés : jamais plus de trois ennemis de vague par joueur
  à la fois. Huit secondes au moins entre deux vagues, et rien au-delà de 96 personnages ;
- **des ennemis du niveau** : des copies de ses propres ennemis ordinaires (ni boss, ni civil, ni
  personnage à script), donc avec la classe, l'arme et les réglages de combat que ce niveau leur
  donne. Strikers et Thugs dans l'usine, Ninjas et Elites plus loin, sans table à tenir ;
- **par les chemins du moteur** : ils naissent là où le niveau place leur original, les plus
  proches de l'alarme d'abord et de préférence à plus de 45 unités de tout joueur, depuis quatre
  endroits au plus ; ils reçoivent la place du joueur qui a causé l'alarme, et c'est l'IA du jeu
  qui les y mène ;
- chez les clients ils sont créés comme les renforts, d'après leur nom (`ambush_striker_3_w07`).

**Journal : une ligne par maillon.** Quand un piège ne fait rien à un joueur, la première ligne qui
manque dit où la chaîne casse. Pour un client (joueur 1) qui coupe le laser 2, tourelle 1 :

| Machine | Ligne | Ce qu'elle prouve |
|---|---|---|
| client | `defenses : laser 2 (rang 4) coupe ici par notre joueur (joueur 1), 1 fois depuis la ligne precedente : dit a l'hote` | son moteur a vu son joueur dans le rayon, et l'a annoncé |
| hôte | `defenses : laser 2 (rang 4) coupe par le joueur 1 (mp_remote1) d'apres sa machine, 1 annonce(s) depuis la ligne precedente : son joue, ses evenements lances ici` | l'annonce est arrivée, la tourelle est allumée chez l'hôte (`... mais il est eteint ici : sans suite` : le laser n'était pas allumé chez lui) |
| hôte | `defenses : laser 2 (rang 4) coupe ici par mp_remote1 (joueur 1), 1 fois depuis la ligne precedente : dit aux clients` | l'hôte l'a vu aussi, sur ses propres rayons ; chez le client : `... coupe par le joueur 1 (le notre) d'apres l'hote`. L'une des deux lignes suffit ; s'il n'y a que celle-ci, la machine du client n'a rien vu |
| hôte | `defenses : tourelle 1 allume`, puis `defenses : tourelle 1 (rang 0) vise le joueur 1, qui vient de couper son laser` | son choix, et pourquoi (`le plus proche de ceux qu'elle peut toucher`, ou `le plus proche : elle ne peut toucher personne`) |
| client | `defenses : tourelle 1 (rang 0) vise le joueur 1, en attendant le choix de l'hote : c'est le notre`, puis `... d'apres l'hote : c'est le notre` | sa tourelle le vise, d'elle-même puis sur ordre |
| client | `defenses : tourelle 1 (rang 0) tire ici, sur notre joueur` | elle tire, chez lui |
| client | `defenses : notre joueur perd de la vie sous le tir d'une tourelle (59, il en avait 72)` | et le touche |

Aussi : `defenses : rayons de 12 laser(s) recales sur ceux de l'hote, ecart jusqu'a 85 image(s) de
rail` chez un client qui vient d'arriver, `defenses : tourelle 4 allume d'apres l'hote`, `vague :
alarme (console 3, par 'alarm_thug_1') : 6 ennemi(s) en renfort pour 2 joueur(s), depuis 4
endroit(s) du niveau, vers 'konoko'`. `mp_status` a deux lignes, `defenses : ...` et
`tourelles : ...`.

**Niveau de l'hôte.** Jusqu'à la 0.35 un client changeait de niveau avec l'hôte parce que la
commande de victoire du script lui était transmise, et reprenait avec lui à une sauvegarde du
même niveau (vu encore dans les journaux de la 0.35 : `reprise : l'hote a charge ce niveau au
point 4`). Restait un cas : l'hôte qui charge depuis son menu la sauvegarde d'un **autre**
niveau, ou un client qui se connecte depuis un autre niveau. L'hôte dit maintenant le numéro de
son niveau ; un client qui se trouve ailleurs deux secondes et demie plus tard le charge de
lui-même, au même point de sauvegarde, par la reprise après un échec du moteur faite dans cet
autre niveau (`reprise : l'hote est dans un autre niveau (numero 3, point de sauvegarde 2)...`).

**Vu en jeu, et pas vu.** Vu en 0.38, salle du Deadly Brain de l'usine : tourelles allumées chez
tous, cible commune, tirs et dégâts chez le client. Vu dans le premier jet de la 0.39, couloir des
lasers 2 à 4 : la tourelle vise le client qui coupe son laser, tire et le blesse chez lui. Pas
encore essayé : un laser détecté par la machine du joueur qui le coupe, les rayons recalés sur
ceux de l'hôte, les vagues d'alarme, le client qui suit l'hôte dans un autre niveau. À
surveiller : un laser coupé chez un client et resté sans suite chez l'hôte, une tourelle qui reste
allumée chez un client, des rayons qui sautillent, une vague qui naît sous les yeux d'un joueur ou
qui ne trouve pas son chemin.

### Cheats, personnage incarné, apparitions

Depuis la 0.35, trois onglets de plus dans le menu F1 : `Cheat`, `Character` et `Spawn`. Ils sont
dans la colonne de droite, sous le bouton `Close` (la colonne de gauche n'avait plus de place que
pour un). En français : `Cheat`, `Personnage`, `Apparition`.

**Qui y a droit.** L'hôte, toujours. Un client, seulement si l'hôte lui en a ouvert l'accès : par
défaut les trois onglets ne lui sont pas montrés du tout. L'hôte a pour cela, dans son onglet
`Cheat`, un bouton par client : `P1 : blocked` devient `P1 : allowed` d'un clic (le pseudo du
joueur remplace `P1` quand il en a un ; `(absent)` : personne n'a ce numéro en ce moment). L'accès
se donne et se retire en cours de partie, menu du client ouvert ou non : les onglets paraissent ou
disparaissent chez lui dans l'instant, et celui qu'il avait ouvert se referme. Les accès vont par
numéro de joueur (1, 2, 3, dans l'ordre d'arrivée), pas par pseudo, et sont gardés dans
`onimp.ini` : un autre joueur qui prend plus tard le numéro 1 hérite de l'accès du numéro 1.

**L'interrupteur général.** Premier bouton de l'onglet `Cheat` : `Cheats : ON` ou `OFF`, que seul
l'hôte change (un client le voit, et un clic lui répond « réglage réservé à l'hôte »). Sur `OFF`,
qui est le réglage d'origine, plus aucun cheat n'agit, chez personne, à l'instant où l'hôte
clique : invincibilité, munitions, coup fatal et aura tombent, vitesses et saut redeviennent ceux
du jeu. Les réglages de chacun ne sont pas effacés : ils reprennent tels quels quand l'hôte remet
`ON`. Un joueur qui règle un cheat pendant que l'interrupteur est coupé en est prévenu à l'écran.
L'interrupteur ne coupe que l'onglet `Cheat` : le personnage incarné et les apparitions ne
dépendent que de l'accès. Ce partage est un choix fait en l'écrivant, la demande ne le tranchait
pas.

**Chacun les siens.** Les cheats n'agissent que sur le personnage du joueur qui les règle, sur sa
machine, et sont gardés dans son `onimp.ini`. `God mode` et `Unlimited ammo`, que l'hôte décidait
pour tous dans l'onglet Coop jusqu'à la 0.34, sont maintenant de ceux-là : l'hôte invincible ne
rend plus ses clients invincibles.

Onglet **Cheat** :

| Réglage | Effet |
|---|---|
| `Cheats : ON` | l'interrupteur général de l'hôte, décrit plus haut |
| `God mode` | notre joueur ne perd plus de vie ; seule une mise à mort directe, par un script ou par le moteur, passe encore |
| `Unlimited ammo` | notre arme en main se regarnit toute seule : plus besoin de recharger ni de chargeurs |
| `One-hit kill` | un coup ou un tir de notre joueur achève l'ennemi qu'il touche. Un ennemi : un personnage d'une équipe en guerre avec la nôtre. Ni un allié, ni un civil, ni un autre joueur (le tir ami garde ses dégâts). Un personnage que son script rend increvable garde son dernier point de vie |
| `Permanent aura` | l'aura du personnage surpuissant, lueur et coups plus forts, sans hypo et sans fin ; les autres joueurs la voient |
| `P1 : allowed` (trois boutons) | **hôte seulement** : l'accès de chaque client aux trois onglets |
| curseur `Attack speed : 100 %` | vitesse des animations d'attaque, de 100 à 300 % par crans de 10 |
| curseur `Move speed : 100 %` | vitesse de déplacement, course, marche et recul, de 100 à 300 % |
| curseur `Jump : 100 %` | force du saut, de 100 à 400 % par crans de 20. **Au-dessus de 100, plus de dégâts de chute** |

Onglet **Character** : incarner un autre personnage que Konoko.

- La liste vient des données chargées par le jeu : toutes les classes de personnage (`ONCC`) du
  niveau en cours, relevées à chaque chargement. Avec l'Anniversary Edition, qui range les
  personnages dans les données communes, ce sont les 124 mêmes partout.
- `Character : Konoko` dit qui on incarne. Un clic sur ce bouton rend Konoko, dans la tenue
  choisie à l'onglet Display.
- `Filter` : `All`, `Konoko`, `TCTF` (policiers, SWAT, gardes), `Syndicate` (Strikers, Furies,
  Tankers, Ninjas, Elites, Thugs), `Bosses` (Barabas, Muro, Griffin, Mukade, Shinatama),
  `Civilians`, `Other`. Le classement est fait d'après le nom des classes ; une classe ajoutée par
  un mod va dans `Other`.
- Huit classes par page, `Previous` et `Next (2/5)` pour tourner. Un clic sur une classe
  l'incarne : corps, textures et animations sont remplacés sur place, par la routine du moteur que
  la commande de script `chr_set_class` appelle. Celle qu'on incarne est entre chevrons.
- `Hotkey : F5` : la touche « personnage suivant ». Chaque clic passe à la touche suivante, de
  `F5` à `F12`. En jeu, la touche passe à la classe suivante de la liste filtrée, puis revient à
  Konoko après la dernière. Elle n'est lue que dans la fenêtre du jeu qui a le clavier.
- Les autres joueurs voient le changement : c'est l'hôte qui l'enregistre et le redit à tous. Le
  choix est gardé dans `onimp.ini` et repris au lancement et au niveau suivants. Si l'hôte retire
  l'accès à un client, celui-ci redevient Konoko chez tous.

Onglet **Spawn** : faire apparaître quelque chose devant son joueur.

- `Kind` : `Items`, `Weapons` ou `NPCs`. Le bouton du dessous règle ce qui va avec : `Count : x1`
  (1, 3 ou 6 objets d'un clic), `Ammo : 100 %` (cartouches de l'arme posée : 100, 75, 50, 25 ou
  0 % de sa contenance), `Team : Enemy` ou `Ally`.
- **Objets** : hypo, chargeur de balles, cellule d'énergie, bouclier de force, camouflage de
  phase. Posés au sol à neuf unités devant le joueur.
- **Armes** : toutes les classes d'armes du jeu (`ONWC`), les douze, armes de boss comprises, par
  la routine de la commande de script `weapon_spawn`.
- **PNJ** : n'importe quelle classe de personnage, avec le même filtre que l'onglet Character.
  `Enemy` : il est d'une équipe en guerre avec la nôtre, et attaque. `Ally` : il est de notre
  équipe, et se bat à nos côtés. Il apparaît à 22 unités, face au joueur, sans arme. Vingt-quatre
  PNJ apparus en vie au plus : le moteur n'a que 128 personnages.
- Rien n'apparaît dans un mur : si quelque chose barre la route devant le joueur, l'apparition se
  fait en deçà.
- Avec des PNJ partagés (options `npcs` ou `quests`), ce que demande un client est créé chez
  l'hôte, à l'endroit que le client désigne, et gagne tous les joueurs. Sans PNJ partagés, chacun
  a son propre monde : l'apparition n'existe que chez celui qui la demande.

**Comment c'est fait.**

- **Vitesses et saut** : le moteur met à jour chaque personnage en deux temps par tick, ses touches
  (`0x4F6AD0`) puis tout le reste (`0x4DDBA0`). Le mod s'intercale autour des deux appels, pour
  notre joueur seulement. *Déplacement* : après le second, le pas du tick que le moteur vient de
  calculer est allongé, à plat ; c'est ensuite sa propre physique qui déplace le personnage, donc
  qui l'arrête contre un mur comme d'ordinaire. Un coup garde son élan d'origine, et un joueur qui
  encaisse ou qui est au sol n'est pas poussé plus loin. *Attaque* : le moteur avance d'une image
  par tick ; le mod en saute de quoi tenir la cadence voulue (une image sur deux à 200 %), sans
  toucher à la dernière, où le moteur enchaîne l'animation suivante, et use du même pas le temps de
  récupération entre deux attaques. *Saut* : la vitesse de départ et la poussée de la touche tenue
  se lisent dans la classe du personnage ; elles y sont multipliées le temps de l'appel, et remises
  aussitôt après, pour qu'un PNJ de la même classe saute comme avant.
- **Dégâts de chute** : le moteur les inflige à l'atterrissage (`0x4EC98A`), et tue qui reste cinq
  secondes en l'air (`0x4DEB65`). Les deux passent par sa fonction de dégâts avec le type « chute » :
  le mod les annule pour notre joueur quand son saut est réglé au-dessus de 100.
- **Coup fatal** : dans l'arbitrage des dégâts, les dégâts d'un coup de notre joueur sur un ennemi
  sont portés à la santé de la victime. Chez un client, c'est ce montant qu'il annonce à l'hôte,
  qui tranche comme pour tout autre coup.
- **PNJ apparu** : un combattant du niveau sert de modèle (ses réglages de combat et d'alerte), dont
  la classe, le nom et l'équipe sont changés, sans arme ni script. Son nom, `mpx2_07_striker_easy_1`,
  porte son équipe et sa classe : c'est ce qui permet à chaque client de le créer à son tour quand
  l'hôte le lui annonce, comme un renfort.

**Ce qui n'a pas été vu en jeu.** Tout ce chapitre est écrit, compilé et vérifié contre
l'exécutable (les onze sites du moteur employés ont les octets attendus), mais aucun de ces
réglages n'a encore été essayé dans une partie. Ce qui est le plus à surveiller :

- à vitesse d'attaque élevée, un coup dont la fenêtre ne dure qu'une image peut être sauté, et
  certains sons ou effets attachés à une image précise aussi ;
- un personnage incarné garde-t-il sa santé, ses coups, ses prises ? Le moteur fait le
  remplacement ; ce qu'il change d'autre n'a pas été relevé. Les cinématiques jouent sur lui les
  animations écrites pour Konoko ;
- un PNJ apparu se bat-il comme ceux du niveau ? Il en a les réglages, mais pas de patrouille ni
  de script ;
- sur une machine où l'hôte et un client partagent le clavier, la touche F5 n'agit que dans la
  fenêtre active.

Journal : `cheats : ...`, `personnage : joueur 1 incarne striker_easy_1`, `apparition : PNJ
'mpx2_00_striker_easy_1' cree pour le joueur 1, equipe 2 (ennemi)`, `apparition : arme 'w3_phr'
posee pour le joueur 0, 10 cartouche(s)`. `mp_status` a une ligne `cheats : ...`.

### Munitions des autres joueurs

Au-dessus de la barre de vie de chaque autre joueur, une seule rangée de douze petites cases
carrées, de la largeur de la barre. Depuis la 0.28 un joueur porte jusqu'à **douze chargeurs et
douze cellules** au lieu de six (les hypos restent à six) : le moteur a sa borne écrite à six
endroits, portés à douze en session. Son propre affichage, en bas de l'écran, ne sait en montrer
que six : au-delà, les réserves de notre joueur se lisent aussi au-dessus de lui, dans la même
rangée. (La 0.24 traçait deux rangées de points ronds ; de la 0.25 à la 0.27 la rangée avait six
cases.)

- **Carrés rouges, depuis la gauche** : les chargeurs de balles qu'il a en réserve.
- **Carrés verts, depuis la droite** : ses cellules d'énergie.
- Une case sombre est vide. Deux rouges à gauche et trois verts à droite : deux chargeurs, trois
  cellules.
- S'il porte plus de six munitions en tout, les cases du milieu servent aux deux : moitié gauche
  rouge, moitié droite verte. Six de chaque : six cases mi-rouges mi-vertes.
- Ce sont les réserves, celles que l'interface du jeu montre à chaque joueur pour lui-même, pas les
  cartouches de l'arme qu'il tient.
- Chaque joueur envoie les siennes avec son état, trente fois par seconde : elles changent chez
  les autres dès qu'il recharge ou ramasse des munitions. L'hôte voit celles des clients, chaque
  client celles de l'hôte et des autres clients.
- Elles ne sont pas tracées au-dessus d'un joueur à terre, dont la barre porte le compte à
  rebours, ni au-dessus de son propre personnage : l'interface du jeu les donne déjà.

### Civils qui donnent un objet

Dans le jeu d'origine, certains civils et alliés d'un niveau parlent au joueur qui presse la touche
d'action devant eux, et lui remettent quelque chose : un hypo, des chargeurs, des cellules, un
bouclier de force, une arme, parfois l'objet du niveau. Avec des PNJ pilotés par l'hôte ils ne
répondaient qu'à l'hôte (signalé en jeu en 0.23). Depuis la 0.24 :

- le joueur d'un client presse la touche d'action devant un tel PNJ, comme l'hôte : même distance
  (un corps et demi), et en le regardant ;
- chez l'hôte, le PNJ se tourne vers ce joueur, lui parle, fait son geste ; chez le client il fait
  les mêmes gestes, sans les voix ;
- ce qu'il donne arrive au joueur du client, avec le son et le message du jeu, et une ligne du
  mod : `OniMP : un civil vous donne 2 chargeur(s) de balles`, `... : bouclier de force`, `... une
  arme (w1_tap)` ;
- une arme donnée est prise en main, et celle que le joueur tenait tombe à ses pieds, où il peut
  la reprendre ;
- l'objet du niveau, donné à un joueur, l'est à tous, comme lorsqu'il est ramassé ;
- un civil qui a donné ne redonne pas : ce qu'il avait est parti, pour tous les joueurs.

**Messages à l'écran.** Les messages du mod (joueur connecté, réglage changé par l'hôte, joueur à
terre...) s'affichent **en haut à droite**, alignés sur le bord droit, **en vert**, le plus récent
en bas, et s'effacent d'eux-mêmes au bout de dix secondes. Ils ont leur propre zone : le texte du
jeu, en bas de l'écran, reste où il est, sous-titres des dialogues compris. (La 0.20 avait déplacé
la console du jeu entière en haut à droite, et les sous-titres du scénario avec elle : c'est
défait.) Avec `Messages` sur `OFF`, plus aucun message du mod ne s'affiche, alertes comprises
(joueur à terre, mission échouée) ; tous restent écrits dans le journal, sur les lignes qui
commencent par `console :`.

**Identifiants** (depuis la 0.21). L'identifiant d'un PNJ est son nom dans le niveau (`A_t50`,
`Bay1_Thug_1_x2` pour un renfort), écrit en blanc sur un cartouche sombre au-dessus de sa jauge ;
celui d'une arme au sol est le nom de sa classe (`w1_tap`, `w2_sap`), en jaune pâle au-dessus
d'elle. Ce sont les noms qu'emploient le journal et les commandes de script (`chr_teleport`,
`ai2_kill`, `chr_giveweapon`), et les mêmes chez l'hôte et chez les clients. Écrits en capitales ;
un PNJ aussi loin que sa jauge se voit, une arme deux fois moins loin.

### Difficulté : quatre modes

L'hôte choisit le mode dans l'onglet Coop (`Difficulty`), ou par `mp_difficulty`. Les clients le
suivent aussitôt.

| Mode | Ennemis | Joueur à terre | Relevé avec | En plus |
|---|---|---|---|---|
| `Easy` | ceux du mode facile du jeu | 35 s | 45 % de sa santé | |
| `Normal` | ceux du mode normal | 25 s | 40 % | |
| `Hard` | ceux du mode difficile | 10 s | 35 % | |
| `Survival` | ceux du mode difficile, vie et dégâts x1,5 | 5 s | 25 % | les ennemis se soignent : +1 point de vie par seconde, +2 pour un boss |

- « Ceux du mode facile du jeu » : c'est la difficulté du jeu d'origine elle-même qui est réglée,
  le temps de la session, sans toucher aux options du jeu ni à `persist.dat`. Elle décide de ce
  que le moteur en tire lui-même : vie des ennemis (x1,25 en difficile), vie des joueurs (x1,3 en
  facile), attention, parades et esquives des IA, précision de leurs tirs, et les ennemis
  remplacés par une classe plus forte en difficile.
- Changé en cours de niveau, le mode vaut tout de suite pour le comportement des IA et pour la vie
  des ennemis déjà là, portée de l'ancienne valeur à la nouvelle. La vie des joueurs et les
  classes des ennemis déjà apparus ne changent qu'au prochain chargement de niveau.
- Ce que le mode survie ajoute se multiplie avec ce que `onimp_difficulty.ini` donne selon le
  nombre de clients (voir ci-dessous) : 200 % de vie à deux joueurs deviennent 300 %.
- Les boss sont Barabas, Muro, Muro transformé, Mukade et le Muro du rêve, reconnus à leur classe
  de personnage (`barabus`, `muro_generic`, `mutantmuro`, `super_ninja_1`).
- Un client garde dans son `onimp.ini` le mode de sa dernière partie : son premier niveau se
  charge avant que l'hôte lui ait rien dit.

### Joueur à terre et réanimation

Dès qu'un coéquipier est là, debout, dans le même niveau :

- **À terre.** Un joueur qui tombe à zéro ne meurt pas : il s'écroule et reste au sol, sans
  pouvoir bouger, et plus rien ne le blesse. Les ennemis se tournent vers ceux qui sont debout.
  Un compte à rebours commence, de la durée que donne le mode de difficulté. Il s'affiche en
  secondes au-dessus du joueur à terre, sur une barre qui se vide, chez lui comme chez les
  autres, et chaque coéquipier est prévenu à l'écran (`OniMP : JOUEUR 1 A TERRE : 25 s pour le
  ranimer...`), puis de nouveau toutes les cinq secondes, et chaque seconde sur la fin.
- **Ranimer.** Un coéquipier s'approche (à un corps de distance environ) et garde **CTRL**
  enfoncé trois secondes sans le lâcher. Pendant ce temps le compte à rebours est arrêté, et la
  barre, devenue bleu clair, se remplit. Jusqu'à la 0.33 elle ne se remplissait que chez celui
  qui ranime : le joueur à terre n'avait que le titre `REANIMATION` au-dessus d'un compte à
  rebours arrêté, sans savoir quand il se relèverait. Depuis la 0.34 **le joueur à terre voit la
  barre se remplir lui aussi**, et les autres joueurs de même. La réanimation dure un temps fixe :
  chaque machine la compte depuis l'instant où elle l'a apprise (l'annonce reçue par le joueur à
  terre, son état pour les autres), sans rien de plus sur le réseau.
- **Interruption.** Si celui qui ranime encaisse le coup d'un ennemi, lâche la touche ou
  s'éloigne, la réanimation est annulée : le compte à rebours reprend exactement où il s'était
  arrêté, et il faut recommencer les trois secondes.
- **Relevé.** Le joueur retrouve la part de sa santé que donne le mode (45, 40, 35 ou 25 %),
  reprend ses commandes, se relève avec une touche de déplacement, et reste deux secondes à
  l'abri des coups.
- **Échec.** Si le compte à rebours arrive à zéro, le joueur meurt et la mission échoue pour
  tous : reprise commune à la dernière sauvegarde de l'hôte.
- **Sans personne pour ranimer** (seul, dernier joueur debout, coéquipiers dans un autre niveau),
  un joueur meurt comme dans le jeu d'origine. Une mise à mort par un script du niveau passe
  toujours.

Depuis la 0.26, ce qui se voit et s'entend :

- **Corps fixé au sol** (depuis la 0.28). Signalé en 0.27 : le joueur à terre rebondissait à plat
  ventre et glissait, hors de portée de celui qui venait le ranimer. Rien ne retenait le corps :
  l'élan de la chute, un personnage qui marche dessus. Sur la machine du joueur à terre, dès que
  le corps se pose, ou au bout de deux secondes, sa place est prise et il y est ramené, vitesse
  annulée, au moindre écart ; la hauteur reste au moteur, pour qu'il suive le sol. Sur les autres
  machines, son pantin ne heurte plus rien tant qu'il est à terre : sa place y est remise à chaque
  image à celle que dit sa machine, et celui qui s'approchait pour le ranimer le repoussait
  entre-temps, d'où les tressautements. La portée de la réanimation passe de 20 à 26 unités.
  Journal : `a terre : corps pose en (...)`.
- **Panneau.** À la place de la jauge du joueur à terre, un panneau aux couleurs des écrans du jeu
  (fond bleu nuit, filet bleu clair). De bas en haut : l'invite, la barre, le titre, le pseudo, le
  repère. La barre est graduée en dix et porte les secondes ; elle se vide de l'ambre au rouge, et
  bat quand il reste moins d'un tiers du temps. Le titre dit `A TERRE` en rouge, puis
  `REANIMATION` en bleu clair dès que quelqu'un le ranime.
- **Repère à travers les murs.** Une croix de secours dans un anneau, au-dessus du panneau, qui bat
  au rythme d'un cœur. Tout le panneau se voit à travers les murs et les étages, comme les jauges
  des joueurs, et il garde sa taille à l'écran quand le joueur est loin (au-delà de 90 unités, et
  jusqu'à sept fois cette distance) : on le repère de l'autre bout du niveau. Depuis la 0.27 le
  bouton `Wallhack (ESP)` de l'onglet Display coupe cette vue à travers les murs, pour le panneau
  comme pour les jauges : chaque joueur en décide pour lui-même.
- **Invite.** `CTRL POUR RANIMER` sous la barre, chez le joueur qui est à portée de main et en état
  de ranimer ; elle disparaît dès qu'il tient la touche, ou si un autre s'en charge.
- **Alerte sonore** (depuis la 0.27 ; elle remplace le battement de cœur en boucle de la 0.26).
  `OniMP\sounds\AlertFall.mp3` est joué **une fois**, sans boucle, à l'instant où un joueur tombe à
  terre, l'hôte ou un client : **tous les joueurs l'entendent**, celui qui tombe compris. Chaque
  machine le joue pour elle-même quand elle voit un joueur tomber ; rien de plus ne passe par le
  réseau. Si un second joueur tombe pendant que le son joue, il repart du début. Plus de son sur
  la santé critique. Le volume se règle par le curseur `Alert volume` de l'onglet Coop, de 0 à
  100 %, chez l'hôte comme chez chaque client, pour sa machine seulement ; à 0 rien n'est joué.
  Commande : `mp_alertvolume 0` à `100`. Le son ne sort que de la fenêtre du jeu qui a le
  clavier : avec deux fenêtres sur une même machine, qui voient toutes deux le joueur tomber, on
  l'entend une fois et non deux. Le fichier est lu par Windows, hors du moteur du jeu ;
  `install.bat` le copie à côté d'Oni.exe sous le nom `onimp_alert.mp3`, et à défaut le mod le
  cherche dans `OniMP\sounds`. Journal : `son : alerte prete (...)`, `son : alerte jouee une fois,
  volume 50 pour cent (joueur 1 a terre)`.

Depuis la 0.29 :

- **Coup reçu au sol.** Un joueur à terre ne perd rien et ne se relève pas, mais un coup qui
  l'atteint au sol se voit : la machine de l'attaquant annonce l'animation que le moteur a imposée
  au pantin, et la machine du joueur la joue si elle part du sol et y revient (`a terre : coup recu
  au sol, reaction '...' jouee ; compte a rebours inchange`). Le compte à rebours, le corps fixé au
  sol et la réanimation en cours n'en savent rien. Toute autre réaction (un recul debout, une chute)
  est refusée au pantin d'un joueur à terre, qui reprend aussitôt la pose de son joueur : elle le
  relevait chez les autres alors qu'il restait couché chez lui.
- **Celui qu'on ranime le reste.** Tant qu'il est à terre et à moins de 32 unités, un autre joueur
  à terre plus proche ne détourne pas celui qui ranime. Et le joueur qu'on vient de cesser de
  ranimer n'est plus tenu pour « ranimé par un autre » pendant que son état le dit encore : entre
  deux joueurs à terre côte à côte, chaque coup reçu faisait sauter la réanimation de l'un à
  l'autre (journal de l'hôte de la 0.28 : dix-sept débuts en quatre secondes).

### Multiplicateur d'ennemis et civils

**Multiplicateur.** L'hôte choisit x1, x2, x3 ou x4 dans l'onglet Coop (`Enemies`), ou par
`mp_enemies`. Seul, il choisit librement. À plusieurs, le multiplicateur ne descend pas sous le
nombre de joueurs : x2 au moins à deux, x3 au moins à trois, x4 imposé à quatre. Quand un joueur
arrive, il monte de lui-même ; un joueur parti compte encore vingt secondes.

- Chaque ennemi ordinaire du niveau reçoit ses renforts une seconde après son apparition : des
  copies créées à ses côtés d'après son propre objet de niveau, donc de la même classe, avec la
  même arme et le même rôle (garde, patrouille). Elles s'appellent comme lui, suivi de `_x2`,
  `_x3`, `_x4`.
- **Ne sont jamais multipliés**, pour ne pas casser la progression : les boss ; les personnages
  increvables, invincibles, passifs ou qui ne se battent pas ; ceux qui ne sont pas hostiles aux
  joueurs ; et tout personnage auquel un script du niveau est attaché (à sa mort, à sa défaite,
  quand il donne l'alarme...), parce que le niveau attend quelque chose de lui. Dans les données
  du jeu d'origine, cela laisse quatre ennemis sur cinq.
- Un renfort n'a aucun script et ne porte le nom d'aucun personnage du niveau : rien de ce que
  les scripts attendent ne dépend de lui.
- Rien n'apparaît pendant une cinématique ni dans la seconde qui suit, et le renfort d'un
  personnage que le niveau retire (fin d'une scène) est retiré avec lui.
- Le moteur n'a que 128 personnages : au-delà de 96 présents, ou de 40 renforts, plus rien n'est
  ajouté, pour que les scripts puissent toujours créer les leurs.
- Monter le multiplicateur en cours de niveau renforce les ennemis déjà là ; le baisser ne retire
  personne.

**Civils** (`Civilians`). Dans les niveaux où ils ont leur place (aérogare et pistes, quartier
général du TCTF, bâtiment administratif, usine, laboratoires, centrale), un ennemi sur six arrive
avec un civil à ses côtés : passants et employés, ou techniciens et chercheurs, selon le niveau.
Six au plus à la fois. Ils ne se battent pas, et les ennemis ne les visent pas. **Si un joueur en
tue un, la mission échoue** et reprend à la dernière sauvegarde de l'hôte (`OniMP : un civil a ete
tue par le joueur 2 : mission echouee`). Un civil tué par un ennemi ne coûte rien. Leur place ne
doit rien au hasard d'une partie : ils sont aux mêmes endroits d'une reprise à l'autre. Seuls les
civils ajoutés par le mod comptent ; ceux du jeu d'origine restent ce qu'ils étaient.

Tout cela se décide sur la machine qui simule les PNJ : l'hôte avec l'option `npcs`, qui annonce
renforts et civils aux clients comme ses autres PNJ ; chaque machine pour elle-même sans cette
option.

### Difficulté selon le nombre de clients : `onimp_difficulty.ini`

Le plus simple est de régler la difficulté en jeu, par les deux curseurs de l'onglet Coop du menu
F1 de l'hôte (voir plus haut) : rien à recompiler, rien à relancer, aucun fichier à ouvrir. Ce
qu'ils règlent est gardé dans un second fichier, à côté d'`Oni.exe` lui aussi (dans `AE` ici :
`AE\onimp_difficulty.ini`). Ce fichier se modifie aussi à la main, pour ce que les curseurs ne
font pas : une valeur à part pour deux ou trois clients, ou des PNJ plus faibles qu'à l'origine.
Jamais besoin de toucher au code : les valeurs écrites dans `src/difficulty.cpp` ne servent que
pour une ligne absente du fichier, et ne changent rien sans recompiler.

```ini
[health]
; vie des PNJ, boss compris. De 10 a 2000.
clients_1 = 200
clients_2 = 300
clients_3 = 400

[damage]
; degats des PNJ sur les joueurs. De 0 a 1000.
clients_1 = 105
clients_2 = 110
clients_3 = 115
```

- Chaque valeur est un pourcentage de la valeur d'origine du jeu : 100 la laisse telle quelle, 200
  la double, 50 la divise par deux. `clients_1` vaut quand un client est connecté (deux joueurs),
  `clients_2` pour deux clients, `clients_3` pour trois.
- `[health]` : la vie de tous les PNJ, boss compris. Elle s'applique toujours.
- `[damage]` : ce qu'un coup ou un tir de PNJ enlève à un joueur. Ne s'applique que si
  `Online damage` est sur `ON` dans l'onglet Coop du menu F1 de l'hôte (`online_damage = true`
  dans `onimp.ini`) ; sur `OFF` les PNJ font leurs dégâts d'origine, quoi que dise le fichier, et
  la console le dit : `OniMP : PNJ : vie x2.00, degats normaux, Online damage sur OFF (1
  client(s))`. Avec les valeurs d'origine l'écart est mince, 5 % par client : un coup de 6 points
  en fait 6 ou 7. Pour le voir, essayer `clients_1 = 200`.
- Seul le fichier de l'**hôte** compte : il envoie aux clients ce qu'il en tire.
- Il est relu dès qu'il est enregistré, en pleine partie : l'hôte regarde sa date une fois par
  seconde (depuis la 0.17 ; avant, il fallait un chargement de niveau ou l'arrivée d'un client).
  Inutile de relancer le jeu. La console de l'hôte le confirme (`OniMP : difficulte relue : vie
  150 / 300 / 400, degats 200 / 110 / 115 pour cent`), son journal redit ce qu'il y a lu
  (`difficulte : onimp_difficulty.ini lu ; vie 200 / 300 / 400, degats 105 / 110 / 115 pour cent
  avec 1, 2, 3 client(s)`), et la console de chacun ce que cela donne (`OniMP : PNJ : vie x3.00,
  degats x1.10 (2 client(s))`). La jauge des PNJ suit aussitôt.
- Une valeur absente garde sa valeur par défaut, une valeur hors bornes y est ramenée, un fichier
  absent vaut le fichier d'origine. `OniMP\build\onimp_difficultytest.exe AE\onimp_difficulty.ini`
  affiche ce que le jeu en tirera, sans le lancer.
- Sans l'option `npcs` (scripts `run_host.bat` / `run_client.bat` seuls), chacun a ses propres
  PNJ et rien de tout cela ne s'applique.

## Tester sans le jeu

Ces outils demandent une compilation avec `-DONIMP_BUILD_TESTS=ON` (voir [Compiler](#compiler)).

```
OniMP\build\onimp_nettest.exe host 27801
OniMP\build\onimp_nettest.exe client 127.0.0.1:27801
```

Les deux processus échangent de faux joueurs, de faux PNJ et un événement fiable, puis
affichent `OK` ou `ECHEC`. Utile pour distinguer un problème de pare-feu d'un problème de jeu.

Avec `stall` en dernier argument des deux commandes, l'hôte se fige six secondes et demie,
comme pendant un chargement de niveau : le client le croit parti et se reconnecte alors que
l'hôte n'a rien vu. Le test vérifie que les deux côtés repartent d'un canal fiable vierge.
Avec `resync`, c'est le client qui demande une nouvelle connexion, comme il le fait après avoir
rechargé son niveau.

Recherche automatique et refus :

```
OniMP\build\onimp_nettest.exe host 27801
OniMP\build\onimp_nettest.exe client auto:27801
```

Le client ne reçoit pas d'adresse : il attend l'annonce de l'hôte et le rejoint. Avec `radmin` en
dernier argument des deux commandes, tout passe par Radmin VPN seulement ; le test échoue s'il
n'est pas connecté.

Partie à quatre :

```
OniMP\build\onimp_nettest.exe host 27801 multi3
OniMP\build\onimp_nettest.exe client 127.0.0.1:27801 multi3      (trois fois)
OniMP\build\onimp_nettest.exe client 127.0.0.1:27801 full        (pendant que les trois sont là)
```

Chaque client doit recevoir de l'hôte l'état des deux autres et leurs messages fiables, dans
l'ordre et avec le numéro de leur auteur. Au milieu du test l'hôte cesse trois secondes
d'interroger le réseau, comme dans son menu de pause : les clients doivent continuer de se voir
sans interruption, recevoir pendant ce temps les messages des autres et ceux de l'hôte, et l'hôte
retrouver à sa reprise ce qui lui était destiné. Le client lancé avec `full` est de trop : il doit
être refusé et apprendre que la partie est complète. `multi2` fait le même test à trois joueurs. Sur une seule machine, ces
tests montrent que l'annonce sort bien par la carte Radmin et qu'elle est comprise ; ils ne
disent pas qu'elle traverse le réseau Radmin jusqu'à une autre machine.

| Outil | Ce qu'il vérifie |
|---|---|
| `onimp_timelinetest.exe` | un joueur simulé passe du repos à la course puis au sprint, son état part à 30 Hz avec des retards irréguliers ; la pose recalculée à 144 images par seconde doit correspondre : bonne animation, bonne frame, position à moins d'un tiers de tick |
| `onimp_codectest.exe` | le classement des commandes de script et leur format réseau |
| `onimp_stubtest.exe` | le code machine généré pour intercepter ces commandes : arguments intacts, valeur de retour, pile équilibrée sur 60 000 appels |
| `onimp_hooktest.exe` | le détour d'une fonction, sur six fonctions factices qui ont les prologues et les arguments de celles détournées dans le moteur : passage par le détour, appel de l'originale, pile intacte, refus d'un prologue inattendu, comparaison du prologue encore lisible après le saut ; et le remplacement d'une adresse de fonction écrite dans le code |
| `onimp_desynctest.exe` | les règles du détecteur de désynchronisation : rien pour deux états identiques, un état qui bouge chez l'hôte ou deux niveaux différents ; renvoi au quatrième rapport divergent, trois fois au plus, à cinq secondes d'intervalle ; un seul signalement ensuite ; rien pour des rapports arrivés en rafale |
| `onimp_difficultytest.exe` | la difficulté selon le nombre de clients : valeurs par défaut du fichier et bornes ; vie portée à 200, 300, 400 % et retour exact à l'origine, un PNJ presque mort qui ne meurt pas d'une baisse ; dégâts tenus au point près sur cent coups, à 105 comme à 50 % ; un client absent un instant qui compte encore, un départ pris après vingt secondes ; un fichier écrit, lu, réécrit puis relu dans le même processus, avec sa date qui change ; les valeurs d'un curseur du menu, de +0 à son plus haut cran, écrites dans le fichier sans en perdre les commentaires. Avec un fichier `onimp_difficulty.ini` en argument, il affiche ce que le jeu en tirera |
| `onimp_modtest.exe` | la liste des mods de l'AE Installer : lecture d'`installed_mods.xml`, ce que chaque joueur a de plus que l'autre, empreinte, format réseau, noms des paquets |
| `onimp_rulestest.exe` | les règles de la partie : les quatre modes (35, 25, 10 et 5 s ; 45, 40, 35 et 25 % ; régénération en survie seulement) ; un compte à rebours arrêté par une réanimation puis repris où il en était, jusqu'à zéro ; le multiplicateur d'ennemis, libre seul, x2 au moins à deux, x3 à trois, verrouillé à x4 à quatre, et ce que le bouton du menu propose ; les noms des renforts et des civils, écrits et relus ; les classes de boss ; les niveaux à civils, un ennemi sur six ; la taille et les noms des vagues d'alarme ; l'écart entre deux positions d'un rayon de laser sur son rail, en boucle ou en va-et-vient, et ce qu'on en tolère avant de le recaler, le pas d'un rayon tel que le moteur le fait ; la cible d'une tourelle, du joueur qui vient de couper son laser au plus proche hors d'atteinte |
| `onimp_configtest.exe` | ce qu'`onimp.ini` garde d'un lancement à l'autre : l'hôte et un client du même fichier changent chacun leurs réglages d'affichage, leur langue, leur volume et leur pseudo, puis sont relancés ; chacun retrouve les siens et aucun n'a défait ceux de l'autre ; tir ami, invincibilité et munitions illimitées de l'hôte gardés ; un client qui n'a rien réglé part de la section de l'hôte ; un volume hors bornes ignoré, un volume à zéro gardé |
| `onimp_langtest.exe` | la langue du mod : chaque format du dictionnaire prend les mêmes arguments en français et en anglais, aucun texte n'y figure deux fois ; un libellé du menu, un message et les mots qu'on lui donne traduits dans les deux sens ; le changement de langue pris en compte à l'appel suivant ; un texte inconnu rendu tel quel ; jamais plus que la place donnée |
| `onimp_groundtest.exe` | le numéro des armes et le rapprochement des armes au sol : un numéro par arme, jamais le même d'un joueur à l'autre ; l'échange de deux armes de la même classe au même endroit, tel que relevé dans les journaux de la 0.22 (la copie du joueur ne reprend pas l'arme qu'elle vient de lâcher, que le numéro de l'autre soit connu ou non) ; une liste du sol en retard, qui ne touche pas à l'arme lâchée ; deux armes côte à côte retrouvées par leur numéro malgré l'endroit ; un numéro donné à tort, qui se défait |
| `onimp_trailtest.exe` | la trace de l'hôte que suit le jeu automatique : un joueur suivi contourne un mur de 160 pas, le suiveur le rejoint de point en point sans jamais traverser le mur ; un grand tour de l'hôte sauté quand il repasse près du suiveur ; le point de l'étage du dessus pas tenu pour atteint d'en bas ; trace pleine, joueur déplacé d'un coup, nombres invalides |

Avec un dossier de jeu en argument, `onimp_modtest.exe` ne teste rien : il affiche la ligne
`mods AE :` que la DLL écrira pour ce dossier. C'est le moyen de savoir, sans lancer le jeu, quels
mods sont installés dans une copie d'Oni :

```
OniMP\build\onimp_modtest.exe AE
```

Ces tests vérifient les mécanismes, pas leur effet dans le jeu.

## Fonctionnement

Les adresses et offsets, dans `src/oni.h`, viennent des symboles de débogage de Daodan 4.1 et
du désassemblage d'Oni.exe. Tout se passe sur le thread du jeu.

### Ce qu'OniMP modifie dans Oni.exe

Rien sur le disque : les modifications sont faites en mémoire au chargement, après vérification
du code en place. Si un endroit n'a pas le contenu attendu, il est laissé intact, la fonction
correspondante est perdue et le journal le dit (ligne contenant `inattendu`).

| Adresse | Modification | Sert à |
|---|---|---|
| `0x4D4746` | appel redirigé | la boucle d'OniMP, une fois par frame en jeu |
| `0x468AD4` | appel redirigé | en session, le jeu continue derrière les menus |
| `0x4D345A` | appel redirigé, à la suite de Daodan | enregistrer les commandes `mp_*` |
| `0x4D48D9` | appel redirigé | ne pas afficher l'écran d'échec avant une reprise commune |
| `0x4E17F6` | appel redirigé | les jauges de vie au-dessus des autres joueurs et des PNJ |
| `0x48B1D0` | fonction détournée | les PNJ traitent le personnage distant en adversaire |
| `0x49BA10` | fonction détournée | entre deux joueurs, les PNJ visent le plus proche |
| `0x4E65F0` | fonction détournée | arbitrer chaque dégât entre les machines ; tir ami, invincibilité |
| `0x51CED0` | fonction détournée | un personnage piloté par le réseau peut tirer sans planter |
| `0x4E5360` | fonction détournée | imposer la direction de l'arme d'un personnage piloté par le réseau |
| `0x50F240` | fonction détournée | en session, la difficulté lue par le moteur est celle du mode choisi par l'hôte |
| `0x504C30` | fonction détournée | en session, les écrans de texte des scripts s'ouvrent depuis la boucle du jeu, hors de la simulation |
| `0x4FBDE8` | appel redirigé | les messages du mod, tracés dans leur propre zone à la suite de la console du jeu |
| `0x5068BE`, `0x4D4914` | appels redirigés | en session, ni écran d'accueil après un chargement de niveau, ni écran de fin de niveau gagné : tous deux attendent une touche |
| `0x4E7740` | fonction détournée | tir ami : les coups de mêlée d'un joueur atteignent l'autre |
| `0x4EA2B0` | fonction détournée | tir ami : un joueur peut faire une prise à l'autre |
| `0x4DCD01` | adresse de fonction remplacée | le client déclenche les zones du niveau chez l'hôte |
| `0x4DDD07` | appel redirigé | le pantin d'un autre joueur ne ramasse plus, de lui-même, le bonus voisin de celui que ce joueur a pris |
| `0x4F81F8` | appel redirigé | la touche d'action d'un client devant un PNJ part vers l'hôte, où se tient la conversation |
| `0x4D3233` | appel redirigé | à la fermeture du jeu, une zone de texte du moteur restée sans lignes n'est pas libérée : le moteur y plantait |
| `0x4FBDE3` | appel redirigé | juste avant de tracer ses messages, le moteur retrouve saine une zone de texte qui avait perdu ses lignes : il plantait en la traçant |
| `0x501933` | adresse de fonction remplacée | les onglets Coop, Display, Cheat, Character et Spawn du menu F1, la connexion gardée pendant ce menu |
| `0x4DD068`, `0x4DD5A7` | appels redirigés | les deux temps de la mise à jour d'un personnage à chaque tick, ses touches puis le reste : vitesse d'attaque, vitesse de déplacement et saut de notre joueur (onglet Cheat) |

| `0x49AC30` | fonction détournée | l'alarme donnée dans le moteur, par la commande de script `ai2_tripalarm` comme par l'événement d'un laser ou d'une console : une vague d'ennemis la suit |
| `0x4CF27C` | appel redirigé | la recherche de cible d'une tourelle : celle de l'hôte, la même sur toutes les machines |
| `0x4CF65E` | adresse de donnée remplacée | le drapeau qu'une tourelle lit avant de tirer : celui du mod, qui chez un client vaut celui de l'hôte, au lieu de « toutes les IA passives » |
| `0x4CBA23` | appel redirigé | le test d'un laser contre le joueur de la partie : chez l'hôte il est fait aussi pour le pantin de chaque client ; chez un client il reste celui du moteur, contre son seul joueur (de la 0.36 à la 0.38 ses lasers ne détectaient plus personne) |
| `0x4CBB0A` | appel redirigé | les événements d'un laser qu'un joueur coupe ici : les autres machines sont prévenues et les lancent chez elles ; un joueur à terre n'en déclenche plus |
| quatre appels vers `0x478950` | appels redirigés, avec l'option `quests` seulement | intercepter les commandes de script à leur enregistrement |
| import `FindWindowA` | entrée de la table d'imports remplacée | lancer Oni plusieurs fois sur la même machine |

### Se trouver et se connecter

- **Annonce de l'hôte.** Toutes les deux secondes, l'hôte diffuse un petit paquet UDP sur le port
  de la partie + 1 (27778) : identifiant de l'hôte, nom de la partie, port, version d'OniMP,
  niveau chargé, et son état, en attente d'un joueur ou partie en cours et complète.
- **Par la bonne carte réseau.** Une machine reliée à Radmin VPN a plusieurs cartes : Ethernet
  ou Wi-Fi, et la carte virtuelle de Radmin, en 26.x.x.x. Une diffusion envoyée sans précaution
  part par la carte que Windows préfère, pas forcément celle-là, et l'autre joueur ne la reçoit
  jamais. L'hôte relève donc ses interfaces (`GetIpAddrTable`) et, pour chacune de celles qu'il
  retient, ouvre une prise **liée à l'adresse de cette interface** : c'est ce lien qui fait
  sortir la diffusion par elle. L'annonce y part vers l'adresse de diffusion de son sous-réseau
  (26.255.255.255 pour Radmin VPN) et vers 255.255.255.255. Le relevé est refait toutes les cinq
  secondes : Radmin VPN peut être lancé ou coupé en cours de partie.
- **Le reste n'a pas besoin d'être lié.** Les paquets adressés à une machine précise suivent la
  table de routage de Windows : vers une adresse en 26.x.x.x ils sortent d'eux-mêmes par la
  carte Radmin. La prise de la partie écoute donc sur toutes les interfaces, ce qui garde les
  essais sur une seule machine possibles. Le tri se fait à la réception : avec
  `interface = radmin`, l'hôte ignore les appels et le client les annonces qui ne viennent pas
  de 26.x.x.x.
- **Recherche du client.** Avec `host = auto`, le client écoute le port des annonces. La
  première partie qui lui est ouverte (même protocole, en attente, du bon nom si `join` est
  renseigné) lui donne l'adresse de l'hôte, celle d'où vient l'annonce, et il s'y connecte comme
  à une adresse saisie à la main. Une partie écartée est signalée une fois, avec sa raison. Si
  l'hôte ne répond pas pendant six secondes, le client écoute de nouveau les annonces ; un hôte
  perdu de vue (niveau qui se recharge) est d'abord rappelé directement.
- **Accord ou refus.** Le client appelle (`Hello`), l'hôte accepte (`Welcome`, qui porte le numéro
  donné à ce client) ou, depuis la 0.12, refuse en disant pourquoi (`Reject`) : partie complète,
  trois clients étant déjà là, ou version différente. Jusque-là
  un client de trop ou d'une autre version n'avait aucune réponse. Un appel d'une autre version
  du protocole est le seul paquet étranger auquel l'hôte réponde.

### À plusieurs : tout passe par l'hôte

Jusqu'à la version 0.13 il y avait un hôte et un client. Depuis la 0.14 l'hôte tient trois places
de client.

- **Numéros.** L'hôte est le joueur 0. Chaque client reçoit à sa connexion le numéro de la
  première place libre, de 1 à 3, et le garde tant qu'il ne reste pas cinq secondes sans donner de
  nouvelles ; une reconnexion après un chargement de niveau ne le change pas. Tout ce qui désigne
  un joueur sur le réseau porte ce numéro.
- **Une liaison par client.** Chaque client n'a qu'une liaison, vers l'hôte ; l'hôte en a une par
  client, chacune avec son propre canal fiable, ses propres numéros de message et son propre
  délai de silence. Un client qui se reconnecte ne dérange pas les autres.
- **États des joueurs.** Chaque client envoie l'état de son joueur à l'hôte. L'hôte le renvoie tel
  quel aux autres clients, avec le niveau où se trouve ce joueur. Un client reçoit donc par sa
  seule liaison l'état de l'hôte et celui des autres clients, et les range par numéro.
- **Messages entre joueurs.** Ce qu'un joueur annonce aux autres (porte ouverte, console activée,
  arme en main, arme lâchée ou ramassée, bonus ramassé, rechargement, projection, coup, réaction)
  part d'un client vers l'hôte, qui le traite pour lui-même puis le redistribue aux autres
  clients par leur canal fiable, en y gardant le numéro de son auteur. Ce que l'hôte annonce
  lui-même part directement vers tous. Ce qui ne regarde que l'hôte et un client ne l'est pas :
  scripts, reprise de niveau, réglages, empreintes, mods.
- **Qui est visé.** Un coup ou une réaction désigne le joueur qui l'encaisse ; chaque machine le
  reçoit et seule celle de ce joueur l'applique. Une projection est appliquée par la machine qui
  simule sa cible. Chaque machine vérifie aussi qu'un message vient de qui pouvait l'envoyer : un
  coup de PNJ ne vaut que venant de l'hôte, un coup de joueur que venant de ce joueur.
- **Ce qui ne va qu'à un client.** Le journal des scripts du niveau est rejoué au seul client qui
  arrive ; les autres l'ont déjà, et le rejouer chez eux y referait apparaître les ennemis.
- **Pantins.** Chaque machine a un personnage par autre joueur, nommé `mp_remote` suivi de son
  numéro (`mp_remote0` pour l'hôte chez les clients). Tout ce qui valait pour le pantin unique
  vaut pour chacun : cible des PNJ, zones de déclenchement chez l'hôte, tir ami, jauge.
- **Menu de pause de l'hôte.** Depuis la 0.16 le jeu et le réseau tournent pendant les menus
  (voir « Menus : le jeu continue derrière »), chez l'hôte comme chez les clients. Si cela ne peut
  pas se faire (`run_in_menus = false`, ou code du moteur inattendu), l'enveloppe posée sur le menu
  F1 de l'hôte sert encore le réseau à elle seule, comme en 0.14 : les états et les messages des
  clients sont accusés et redistribués aux autres, et ce qui était pour l'hôte est gardé puis
  remis à ses fonctions à la reprise.

### Menus : le jeu continue derrière

Dans Oni un menu arrête le jeu. Le menu de pause (F1), le menu principal ouvert en cours de partie
(Échap) et l'écran d'une console de données sont des dialogues modaux : une boucle fermée
(`0x4689D0`) qui trace le dialogue et traite ses messages jusqu'à sa fermeture. La boucle du jeu
attend qu'elle rende la main ; pendant ce temps aucun tick n'est simulé.

Seul, c'est une pause. À plusieurs, c'est une désynchronisation : les PNJ, que l'hôte simule, se
figeaient chez tout le monde dès qu'il ouvrait un menu, et le joueur d'un client dans un menu ne
bougeait plus et n'encaissait plus rien pendant que la partie continuait chez les autres.

- **Ce qui est fait.** La boucle des dialogues appelle à chaque image la mise à jour des fenêtres ;
  cet appel (`0x468AD4`) est redirigé. Quand une session est ouverte et qu'un autre joueur est
  relié, OniMP y fait ce que la boucle du jeu fait elle-même à chaque image : avancer l'horloge du
  jeu (`0x4FBEB0`), simuler les ticks dus (`0x4FAFB0`), puis sa propre boucle, réseau compris.
- **Les menus ne changent pas.** Ils s'ouvrent, se lisent et se ferment comme avant. Le moteur
  retrace déjà la scène derrière ses dialogues : elle est maintenant vivante au lieu d'être figée.
- **Le joueur dans son menu.** Il ne reçoit aucune commande : la simulation est appelée avec des
  entrées vides, et non sans entrées, faute de quoi le moteur reconduirait les touches du tick
  précédent et un joueur qui ouvrait son menu en courant courrait toujours. Il reste dans le
  monde : un ennemi peut le frapper pendant qu'il lit ses objectifs.
- **Quand personne n'est relié.** Le jeu se met en pause comme avant, hôte en attente ou client en
  recherche compris. Le réseau, lui, reste servi : un client peut arriver pendant que l'hôte est
  dans son menu, et le jeu repart alors derrière.
- **Le menu Échap.** Il s'ouvre depuis l'intérieur de la simulation, pendant le traitement des
  touches. La faire tourner à ce moment-là, c'est l'appeler de l'intérieur d'elle-même : le tick
  en cours chez l'appelant ne reprend qu'à la fermeture du menu, après ceux qui auront été simulés
  entre-temps. Les fins de niveau, victoire ou reprise, sont de toute façon traitées par la boucle
  du jeu, donc après la fermeture. Un dialogue que cette simulation ouvrirait à son tour ne la fait
  pas tourner une troisième fois : celui-là fige le jeu. Les écrans de texte des scripts ne
  s'ouvrent plus de là depuis la 0.20 (voir « Écrans de texte du niveau »).
- **Dans le journal.** `menu : ouvert, le jeu continue derriere`, puis à la fermeture
  `menu : ferme apres N image(s), M tick(s) simule(s)` : M vaut à peu près soixante fois le nombre
  de secondes passées dans le menu.
- **Pour s'en passer.** `run_in_menus = false` dans `onimp.ini` : le jeu se remet en pause
  derrière les menus, comme en solo.

### Écrans de texte du niveau

Les consoles de données, les consignes de l'entraînement et les messages que Konoko lit à son
poignet sont des écrans de texte plein cadre : la commande de script `text_console`, appelée
cinquante-six fois dans les scripts de douze niveaux. C'est un dialogue du moteur, comme le menu
de pause, que le joueur referme lui-même.

- **Ce qui n'allait pas.** Signalé en jeu en 0.19 : après certains de ces écrans, le jeu restait
  figé chez l'hôte ou chez un client, et il fallait le relancer. La cause a été lue dans
  l'exécutable, pas observée pas à pas. La commande (`0x504C30`) ouvre le
  dialogue sur place et ne rend la main au script qu'à sa fermeture : la boucle des dialogues
  tourne alors à l'intérieur de l'interpréteur de scripts, lui-même à l'intérieur de la
  simulation. Or depuis la 0.16 la simulation tourne derrière les dialogues : elle repartait donc
  de l'intérieur d'elle-même. Son ordonnanceur de scripts (`0x47D7E0`) y retrouvait le script de
  l'écran, ni endormi ni terminé, et le relançait depuis le début sur le même contexte
  (`0x47D8C1`) : commandes rejouées, second écran ouvert par-dessus le premier, puis contexte
  libéré. À la fermeture du premier écran, le script d'origine reprenait sur ce contexte libéré et
  s'arrêtait n'importe où. Beaucoup de ces scripts coupent les commandes du joueur avant l'écran
  et les rendent après (`input 0`, `text_console`, `input 1`) : le joueur ne les retrouvait pas,
  chez l'hôte comme chez les clients, qui suivent ses scripts.
- **Ce qui est fait.** La commande est détournée. En session elle note l'écran demandé et rend la
  main aussitôt ; l'écran est ouvert à l'image suivante depuis la boucle du jeu, hors de la
  simulation, là où le moteur ouvre lui-même son menu de pause. Derrière lui le jeu continue,
  comme derrière les autres menus.
- **Ce que cela change.** Le script n'attend plus que le joueur ait fini de lire : la suite
  (commandes rendues, cinématique, ennemis) arrive pendant la lecture. Plus personne n'attend non
  plus la lecture d'un autre : jusque-là le script de l'hôte restait arrêté tant que l'hôte
  lisait, et les clients avec lui.
- **Chez les clients.** Avec l'option `quests`, l'écran demandé par un script de l'hôte s'ouvre
  aussi chez chaque client, comme avant, et de la même façon.
- **Plusieurs écrans.** Un écran demandé pendant qu'un autre est ouvert, ou pendant le menu F1,
  attend son tour ; le même demandé deux fois n'est montré qu'une fois. Ceux qui attendent encore
  à un changement de niveau sont oubliés.
- **Hors session**, rien ne change : l'écran s'ouvre sur place et le jeu est en pause.
- **Dans le journal.** `ecran de texte : 'level_1d' demande, le script continue`, puis
  `ecran de texte : 'level_1d' ouvert`, puis les lignes `menu : ouvert` et `menu : ferme` de tout
  dialogue. `mp_status` les compte.
- **Si le détour est refusé** (code du moteur inattendu), le jeu se remet en pause derrière tous
  les menus, comme avec `run_in_menus = false` : mieux vaut cela qu'une simulation relancée dans
  elle-même. Le journal le dit (`ecrans de texte : code inattendu, ...`).

### Cinématiques : un seul personnage principal, portraits en bas

- **Les autres joueurs sont cachés** (depuis la 0.28). Une cinématique met en scène le personnage
  du joueur ; à plusieurs, les pantins des autres, eux aussi Konoko, s'y voyaient en double ou
  en triple, souvent à l'endroit même où le script place le personnage principal. Entre
  `begin_cutscene` et `end_cutscene`, sur chaque machine (le client reçoit ces commandes de
  l'hôte), les pantins des autres joueurs ne sont plus tracés, ce qui s'affiche au-dessus d'eux
  non plus, et ils ne heurtent plus rien (`chr_nocollision`) ; tout revient à la fin. Chacun voit
  ainsi la cinématique avec un seul personnage principal, le sien. Journal : `cinematique :
  commence, 1 autre(s) joueur(s) caches`, puis `finie, ... remontres`.
- **Portraits des dialogues en bas** (depuis la 0.28). Le jeu fait entrer le portrait de celui qui
  parle en haut de l'écran, à gauche ou à droite. Les scripts donnent à chaque portrait une place
  de départ et une place d'arrivée parmi vingt-six : neuf à l'écran, en trois rangées de trois,
  les autres autour. En session, la rangée du haut et ses abords sont remplacés par la rangée du
  bas, du même côté : le portrait arrive par le bas et s'arrête 65 points au-dessus du bord, juste
  au-dessus des sous-titres. Rien d'autre n'est changé à ces portraits. Bouton `Low portraits` de
  l'onglet Display (`Portraits en bas`), commande `mp_portraits 0` / `1` ; hors session, ils
  restent en haut.

### Cinématiques : ce que les scripts d'un client ne décident plus (0.31)

Chez un client les scripts du niveau tournent, mais leurs commandes transmises ne font rien sur
place : elles attendent que l'hôte les donne. Les commandes bloquantes (un dialogue, une animation
à finir) n'y bloquent donc rien, et **le script d'un client court en avance sur celui de l'hôte** :
dans l'usine, il avait fini la cinématique d'ouverture soixante-cinq secondes avant lui (journal de
la 0.30 : `sauvegarde : point 1 rencontre par notre script` à 22:45:11, `l'hote a atteint le point
1` à 22:46:16). Ce que ce script garde pour lui s'exécute alors au mauvais moment. Relevé sur les
scripts des quatorze niveaux (les commandes que le mod ne transmet pas, et celles qui visent le
joueur) :

- `chr_focus` (usine seulement) : le moteur suit un autre personnage, la réceptionniste, pendant
  trente et une secondes. Le mod prenait ce personnage pour notre joueur : il lui donnait la tenue
  du joueur (`tenue : 'Recep' passe de 'generic_female_1' a 'k4_K'`, chez l'hôte comme chez le
  client, chacun la sienne : c'était le mélange de tenues), envoyait son état aux autres à la
  place du nôtre, et le rendait increvable. `mem::Player` rend maintenant le joueur du niveau, le
  premier personnage créé, celui que les scripts appellent 0, quoi que le moteur suive (`cinematique
  : le script suit 'Recep' (chr_focus) ; notre joueur reste 'konoko'`).
- `ai2_takecontrol` (25 appels, sept niveaux) : `1` passe le personnage du joueur à l'IA de
  déplacement, `0` le lui rend ; et le moteur vise **le personnage que `chr_focus` désigne à cet
  instant** (`0x4B8940`). Le client, en avance, exécutait le `0` pendant le `chr_focus` de l'hôte :
  il rendait la main à la réceptionniste, et son joueur restait aux mains de l'IA. Figé : ses
  touches de déplacement n'y faisaient plus rien, mais il tournait et tirait encore, et ne pouvait
  plus prendre d'hypo (cinquante-quatre demandes sans effet dans le journal du client). La commande
  suit maintenant l'hôte, dans l'ordre de ses `chr_focus`. Et hors cinématique, touches rendues, un
  joueur resté cinq secondes aux mains de l'IA est libéré d'office (`cinematique : notre joueur
  etait reste aux mains de l'IA de deplacement, la main lui est rendue`).
- `playback` (95 appels sur le joueur, dans les quatorze niveaux), `ai2_movetoflag` (10),
  `ai2_lookatchar`, `ai2_setmovementmode` : le film des gestes de Konoko, sa marche vers un repère.
  Joués par le script du client, donc trop tôt. L'hôte les donne maintenant à tous ; un client ne
  les suit que pour son propre joueur, ses PNJ étant déjà pilotés par l'hôte.

Restent aux scripts de chaque machine, sans effet sur le joueur : les ordres donnés aux IA
(`ai2_dopath`, `ai2_attack`, `ai2_setjobstate`...), qui chez un client visent des pantins.

### Zones : tous les joueurs ne comptent que pour un (0.31)

Des scripts demandent combien de personnages se trouvent dans une zone (`trigvolume_count`), et
certains n'agissent que s'il y en a exactement un. À la fin de la cinématique d'ouverture de
l'entrepôt : `if (trigvolume_count (63) eq 1) ai2_spawn A_t48`. Le moteur compte tout personnage
d'une des équipes que la zone retient (`0x4DCB70`), et le pantin d'un autre joueur est de l'équipe
du joueur : à deux le compte valait deux, le premier ennemi n'apparaissait pas, et avec lui la porte
et l'objectif que sa mort commande. C'était l'entrepôt qui ne démarrait pas (journal de l'hôte de
la 0.30 : plus rien après `cinematique : finie`, huit minutes durant). Dans ce compte, les joueurs
présents dans la zone, le nôtre et les pantins des autres, comptent maintenant ensemble pour un
(`zones : zone 63, 2 personnage(s) dont 1 joueur(s) en plus du premier : comptes pour un, soit 1`).
Les tests « personne » et « quelqu'un » des autres niveaux n'en sont pas changés. L'entrepôt a cinq
tests « exactement un » ; aucun autre niveau n'en a.

### Vérifier que l'hôte et chaque client sont d'accord

Le mod n'est pas une simulation rejouée à l'identique des deux côtés : l'hôte décide (PNJ,
scripts, sol) et le client suit. La plupart des écarts se corrigent donc d'eux-mêmes, par le
flux continu des états. Le détecteur est là pour ceux qui ne se corrigent pas.

- **Empreintes.** Une fois par seconde, le client envoie à l'hôte une empreinte de chaque partie
  de son état qui doit être la même des deux côtés : PNJ vivants, consoles activées, bonus au
  sol (combien de chaque type), réglages et options, chargement de niveau suivi, nombre de
  commandes de script passées sur la connexion.
- **Comparaison.** L'hôte calcule les siennes et compare, partie par partie, client par client,
  chacun avec son propre compte. Des rapports arrivés ensemble, parce que l'hôte sortait de son
  menu de pause, ne comptent que pour un : en 0.13 une telle rafale avait fait renvoyer des
  réglages qui étaient déjà en route. Le client a toujours un léger retard : une partie n'est déclarée divergente qu'après quatre rapports
  d'affilée où elle diffère **sans avoir bougé chez l'hôte** entre-temps. Rien n'est comparé
  quand les deux joueurs ne sont pas dans le même niveau, ni pendant les dix premières secondes
  d'une connexion, le temps que le client reçoive l'état de l'hôte.
- **Renvoi forcé.** L'hôte renvoie alors au client ce dont il décide pour cette partie : noms
  des PNJ (le client redemande au niveau ceux qu'il n'a pas pu créer), consoles activées, état
  du sol, réglages, chargement de niveau. Trois renvois au plus par partie, à cinq secondes
  d'intervalle ; si l'écart tient encore, il est signalé une fois dans la console et le journal
  (`desync : ...`), avec les deux empreintes. Un écart sur les scripts est seulement signalé :
  le flux fiable ne perd rien, il n'y a rien à renvoyer.
- **Jamais de rechargement forcé.** Le renvoi ne recharge pas le niveau du client, sauf dans le
  cas déjà prévu où il n'a pas suivi une reprise de l'hôte.

### Mods de l'AE Installer

- **Où ils sont.** L'AE Installer reconstruit les fichiers de niveau de `GameDataFolder` à partir
  du jeu d'origine et des paquets cochés, puis y écrit `installed_mods.xml`, la liste des numéros
  de paquet installés. Le moteur charge ce `GameDataFolder` sans savoir qu'il est modifié : OniMP
  n'a rien à faire pour qu'un mod soit actif, il suffit de lancer le bon jeu. C'est ce que règle
  `game_dir.bat`.
- **Liste.** Au chargement, OniMP lit `installed_mods.xml` et l'écrit dans son journal, avec le
  nom de chaque paquet, tiré du nom de son dossier dans `AEInstaller\packages`, et une empreinte
  de la liste, la même pour deux listes identiques.
- **Comparaison.** À chaque connexion, hôte et client s'envoient leur liste par le canal fiable.
  Chacun la compare à la sienne et dit ce que l'un a et pas l'autre. Le journal le note à chaque
  connexion ; la console ne le redit que si la liste de l'autre joueur a changé, une connexion
  étant rouverte à chaque rechargement de niveau.
- **Entre qui.** L'hôte se compare à chaque client, et chaque client à l'hôte seulement.

### Les autres joueurs

- **Chargement.** OniMP prend le nom `realbink.dll`. Son `DllMain` lit la configuration, ouvre
  le journal et pose les modifications du tableau ci-dessus ; le réseau ne démarre qu'une fois
  en jeu.
- **Boucle.** L'appel `0x4D4746`, exécuté une fois par frame dans la boucle de jeu, est
  redirigé. OniMP y lit le réseau, applique les états reçus et envoie l'état local.
- **Personnage.** L'autre joueur est un personnage créé avec `ONrGameState_NewCharacter` à
  partir d'un objet `CHAR` construit en mémoire, sans IA active et insensible aux dégâts.
- **Interpolation.** Il est affiché avec 100 ms de retard. OniMP connaît donc l'état qui suit
  celui qu'il affiche : la position est interpolée entre deux paquets, et un changement
  d'animation est rejoué au tick exact où il a eu lieu chez l'émetteur (chaque paquet porte
  l'horloge de jeu et la frame d'animation). Sans cela, les pas très courts du sprint étaient
  appris trop tard et l'animation se désagrégeait.
- **Verrou d'animation.** L'animation est imposée avec la fonction qu'utilise la commande
  `chr_animate`, et le personnage est maintenu dans le même état « animation forcée » : le
  moteur ne choisit plus lui-même l'animation suivante.
- **Animations.** Les pointeurs diffèrent d'un processus à l'autre : `animID` est le hash
  FNV-1a du nom de l'animation, résolu par une table reconstruite à chaque niveau.
- **Niveau.** Chaque paquet porte le hash du nom du niveau. Les états d'un autre niveau sont
  ignorés.
- **Reconnexion.** Chaque tentative de connexion du client porte un identifiant neuf. Un hôte
  qui n'a pas vu partir son client (il chargeait un niveau) reconnaît ainsi qu'il revient de
  zéro et remet lui aussi son canal fiable à zéro. Avant la 0.6, les deux côtés se retrouvaient
  avec des numéros décalés : projections, armes, portes et scripts se perdaient jusqu'à la fin
  de la partie.
- **Menu de pause et connexion.** En session, le menu F1 n'arrête plus le jeu ni la boucle
  d'OniMP (voir « Menus : le jeu continue derrière »). Si cela ne peut pas se faire, l'enveloppe
  posée sur le menu (voir « Onglet Coop ») continue d'envoyer l'état du joueur, dix fois par
  seconde : sans cela les autres machines le croiraient parti au bout de cinq secondes.
- **Jauges de vie.** Au-dessus de chaque autre joueur et de chaque PNJ vivant s'affiche une barre.
  Jusqu'à la 0.18 elle passait du vert à l'orange puis au rouge avec la santé ; depuis la 0.19 sa
  couleur dit à qui elle est, et sa longueur seule dit la santé : verte pour l'hôte (joueur 1),
  bleue, orange et jaune pour les clients dans l'ordre de leurs numéros, rouge pour un ennemi,
  violette pour un boss, gris clair pour un PNJ qui n'est pas hostile aux joueurs (alliés,
  civils). Au-dessus d'un joueur à terre, la jauge, plus grande, porte son compte à rebours.
  Celle d'un PNJ porte sa
  santé et son maximum en chiffres ; celle d'un joueur n'en porte plus depuis la 0.14, la barre
  suffit. Le moteur a pour sa mise au point un affichage du nom et de la
  santé au-dessus des personnages (`ai2_shownames`, `ai2_showhealth`), appelé pendant le rendu de
  la scène : c'est son appel qui est détourné. Jusqu'à la version 0.10 les chiffres étaient écrits
  par cet affichage lui-même, qui ne sait écrire qu'en noir sur un rectangle blanc. Tout est
  maintenant tracé avec la fonction du moteur qui pose un rectangle texturé face à la caméra :
  une texture unie, teintée, pour la barre, et une petite texture par chiffre, transparente
  autour du tracé. Les identifiants (depuis la 0.21) sont tracés de la même façon, une petite
  texture par lettre, sur un cartouche sombre inscrit dans la profondeur. Ces textures sont recréées
  à chaque niveau. Pour l'autre joueur, la santé
  montrée est celle que sa machine transmet, pas celle du personnage distant, invincible ici.
  Les jauges des PNJ ne sont tracées qu'en session (hôte ou client), et jusqu'à une certaine
  distance.
- **Jauges : ordre des couches.** Une jauge est un empilement de rectangles : cadre, barre vide,
  barre pleine, chiffres. Pendant le rendu de la scène, le moteur ne trace pas un rectangle
  transparent quand on le lui demande : il le range dans une liste qu'il trie par profondeur à la
  fin de l'image (`0x41DBE0`, tri en `0x41C660`). Les nôtres, tous au même point, y avaient la même
  profondeur, et sortaient du tri dans un ordre qui changeait d'une image à l'autre, selon ce que
  la liste contenait d'autre : le cadre ou la barre vide passait par moments devant le reste, et
  toutes les jauges clignotaient (versions 0.11 à 0.16). Depuis la 0.17 la jauge demande un tracé
  immédiat, comme le moteur le fait pour ses particules et son interface : les couches sont tracées
  dans l'ordre des appels. Chacune est en plus rapprochée de la caméra d'une fraction de sa
  distance, le long de la ligne de visée, et réduite d'autant : même place et même taille à
  l'écran, mais elle passe devant la précédente quelle que soit la comparaison de profondeur, et
  la jauge, inscrite dans la profondeur, n'est pas recouverte par ce que le moteur trace derrière
  elle après les personnages (portes, objets, particules).

- **Jauges des joueurs à travers les murs** (depuis la 0.25). Le moteur n'appelle l'affichage
  au-dessus d'un personnage que s'il trace ce personnage : un joueur caché par un mur n'avait donc
  pas de jauge. Celles des autres joueurs sont maintenant tracées une fois par image, toutes
  ensemble, avec le premier personnage que le moteur trace, qu'il voie les autres ou non. Pour
  passer devant le décor sans toucher aux états de profondeur du rendu, chacune est rapprochée de
  la caméra sur la ligne qui mène à sa place, à dix unités d'elle, et réduite d'autant : même
  endroit et même taille à l'écran, mais devant le mur, déjà tracé à ce moment de l'image. C'est le
  procédé qui décale déjà les couches d'une jauge. Rien n'est tracé pour un joueur situé derrière la
  caméra.
- **Pseudo et latence.** Écrits au-dessus de la jauge avec les lettres des identifiants. La
  latence est l'aller-retour d'un petit paquet que chaque machine envoie une fois par seconde à
  celles qui lui sont reliées et qui lui revient tel quel ; l'hôte redit aux clients, toutes les
  deux secondes, celle de chacun.
- **Munitions au-dessus de la jauge** (depuis la 0.24, refaites en 0.25). Une rangée de six cases
  carrées, tracée comme la jauge : un fond inscrit dans la profondeur, une case sombre par
  emplacement, et un carré teinté, rouge ou vert, dans les cases garnies. Les nombres viennent de l'état de l'autre joueur, où ils
  tiennent sur un octet : ses chargeurs dans une moitié, ses cellules dans l'autre. Ils sont lus
  chez lui dans son inventaire (`0x1A2` et `0x1A6` de son personnage, que le moteur borne à six,
  `0x51CAA7`) ; ceux de son pantin, ici, ne veulent rien dire.

### Armes

- **Arme en main et tirs.** L'arme tenue par chaque joueur est annoncée à l'autre, qui la donne
  au personnage correspondant. L'état de la gâchette et la direction de l'arme voyagent dans le
  paquet d'état : le personnage distant tire avec les mêmes demandes de tir que celles que le
  moteur pose pour un joueur, donc avec de vrais projectiles.
- **Visée.** Ce qui est transmis est la direction réelle de l'arme, pas la seule visée du
  joueur. Le moteur ajoute en effet à cette visée la visée automatique, qui dévie l'arme vers
  l'ennemi proche du viseur (jusqu'à 20 degrés), et le recul. Jusqu'à la version 0.8 seule la
  visée brute partait : dès qu'un joueur tirait sur un ennemi, son arme et ses projectiles
  n'avaient pas la même direction chez l'autre.
  À la réception, la direction est imposée au personnage distant à l'intérieur du moteur, par un
  détour de la fonction qui pose le corps (`0x4E5360`). Elle est appelée à chaque tick, juste
  après que le moteur a recalculé la visée du personnage et juste avant qu'il oriente le buste,
  le bras et l'arme. L'écrire depuis l'extérieur, une fois par image, ne suffit pas : pour un
  personnage d'IA, ce qu'est le personnage distant, le moteur la recalcule entre-temps. Les PNJ
  pilotés par l'hôte reçoivent leur visée de la même façon chez le client.
- **Rechargement.** C'est une animation jouée par-dessus celle du corps, que l'animation
  transmise ne porte pas. Jusqu'à la 0.20 son début était annoncé à part, pour les joueurs
  seulement, et rien au journal ne disait si elle était jouée : signalé en jeu en 0.20, on ne
  voyait personne recharger. Depuis la 0.21 l'état de chaque personnage, joueur ou PNJ, porte un
  drapeau « recharge » tant que le rechargement dure. Quand il se lève, l'autre machine pose
  l'animation de l'arme sur le personnage correspondant, par la recherche que fait le moteur
  (`0x51C696`) ; si ce personnage n'a pas encore l'arme en main, elle réessaie un instant. En 0.21
  cette recherche ne trouvait rien (`... ne peut pas le montrer : pas d'animation pour cette arme`
  à chaque rechargement) : elle écarte toute animation d'une variante que le personnage n'a pas
  (`0x428063`), celles de rechargement sont de la variante « pistolet » ou « fusil », et le moteur
  ne donne cette variante qu'à un personnage qui sort lui-même son arme (`0x4DFC6F`), ce qu'un
  double ne fait jamais. Depuis la 0.22 elle lui est donnée à chaque image, d'après son arme. Pendant
  le rechargement sa visée n'est plus imposée, le bras étant à l'arme, et la gâchette n'est plus
  transmise, comme le moteur la relâche lui-même. Le journal le dit des deux côtés : `rechargement :
  notre joueur recharge 'w1_tap', annonce dans son etat`, puis en face `rechargement : 'mp_remote1'
  recharge 'w1_tap' comme sur l'autre machine`, ou la raison pour laquelle il n'a pas pu l'être.
- **Arme des PNJ** (depuis la 0.22). Chez un client un PNJ suit l'hôte pour ses gestes et ses
  tirs, mais son arme restait l'affaire du moteur local : un coup du joueur du client la lui
  faisait lâcher là alors qu'il la tenait toujours chez l'hôte, ou l'inverse. Signalé en jeu en
  0.21 avec Barabus : arme invisible ou au sol selon la machine, tirs sans arme visible, et l'arme
  ramassée d'un côté seulement (`arme 'w11_ba1' ramassee par le joueur 1 : introuvable au sol
  ici`). L'hôte dit maintenant, quatre fois par seconde si cela change, ce que tient chaque PNJ
  armé, en main ou rangé. Le client y ramène les siens : l'arme tombée chez lui seulement est
  reprise aussitôt, avant que quiconque la ramasse de ce côté ; celle que l'hôte a vue tomber
  tombe chez lui aussi, et se ramasse des deux côtés ; un PNJ sans la bonne arme la reçoit. Cela
  vaut pour tous les PNJ, boss ou non.
- **Aura.** Un hypo pris en pleine forme porte la santé au-dessus du maximum : le personnage
  s'entoure d'une lueur et frappe plus fort. Le moteur tire cette lueur, à chaque tick, de la santé
  du personnage (`0x4DF88A`), et le double d'un autre joueur n'a pas la sienne. L'état transmis
  porte donc l'intensité de l'aura telle qu'elle se voit chez l'émetteur, et l'autre machine la
  demande au moteur pour le personnage correspondant, comme le fait la commande de script
  `chr_super` : le moteur allume alors lui-même les particules « super » de la classe du
  personnage (`0x4DF95C`). Cela vaut aussi pour les PNJ pilotés par l'hôte. Seule la lueur est
  transmise : les dégâts restent jugés chez celui qui frappe. Signalé en jeu en 0.23 : l'aura
  n'était pas synchronisée. La cause n'est pas établie, les journaux de cette partie ne montrant
  aucune aura. Un défaut certain de la 0.23 est corrigé : l'intensité partait en huit niveaux,
  arrondie, et une aura faible (santé à moins de 7 % au-dessus du maximum) valait zéro. Depuis la
  0.24 elle a son octet, et la moindre aura part. Surtout, rien ne disait ce qu'il en advenait à
  l'arrivée : le journal le dit maintenant des deux côtés, pour trancher au prochain essai :
  `aura : allumee chez notre joueur, annoncee dans son etat` chez l'un ; chez l'autre
  `aura : allumee sur 'mp_remote0' d'apres sa machine (N sur 255)`, puis une seconde plus tard
  `aura : 'mp_remote0' : le moteur la montre ici a 0.31 (demandee 0.31)`. Si cette dernière ligne
  dit `0.00`, c'est le moteur qui ne la trace pas ici, et non la transmission qui manque.
- **Bouclier de force et invisibilité** (depuis la 0.24). Le bouclier, que le jeu donne par un
  bonus ou par un civil, fait luire le corps tant qu'il en reste et arrête les tirs ; l'invisibilité
  rend le personnage transparent et le cache aux IA. Le moteur tire les deux de l'inventaire du
  personnage (`0x51BBAF`, `0x517A92`, `0x49A357`), et un double n'a pas celui du joueur qu'il
  représente : rien ne s'en voyait chez les autres (signalé en jeu en 0.23). L'état de chaque
  personnage porte maintenant ce qu'il lui reste de l'un et de l'autre, et l'autre machine l'écrit
  dans l'inventaire du personnage correspondant, à chaque image. Le moteur en tire le reste : la
  lueur, sa montée, l'éclat quand un tir est arrêté, la transparence. Deux effets au-delà de
  l'image : chez l'hôte, les PNJ ne voient plus le joueur invisible d'un client, comme ils ne
  voient pas le sien ; et les tirs des PNJ sur le pantin d'un client qui porte un bouclier y sont
  arrêtés au lieu de lui être annoncés comme des coups. Journal : `bouclier : notre joueur en porte
  un, annonce dans son etat`, et en face `bouclier : 'mp_remote1' en porte un d'apres sa machine
  (100)` ; de même `invisibilite : ...`. Cela vaut aussi pour les PNJ pilotés par l'hôte.
- **Tirer ou frapper.** Dans Oni, les touches de tir servent aussi à frapper : sans arme en main,
  ou avec une arme vide et sans chargeur. La gâchette n'est donc transmise que lorsque le moteur
  la transmet lui-même à l'arme. Sans cela le personnage distant, qui a des munitions infinies,
  tirait pendant que le joueur donnait des coups. L'arme du personnage distant est aussi
  comparée à ce qu'il tient réellement, et rangée ou ressortie par la fonction du moteur qui le
  fait sans animation : celle de la commande `chr_forceholster` peut être refusée en cours
  d'attaque.
- **Armes et bonus au sol.** Quand un joueur lâche son arme, le personnage qui le représente
  chez l'autre la lâche à son tour : elle tombe à ses pieds et l'autre joueur peut la prendre.
  Quand il en ramasse une, ce personnage prend la même arme au sol, qui n'y reste donc pas : par
  son numéro depuis la 0.23 (voir « Numéro des armes »), à défaut l'arme de même classe la plus
  proche. Un bonus ramassé (munitions, cellule, hypo...) est retiré chez l'autre. Les bonus sont
  appariés par leur type et leur position, à une marge près. Jusqu'à la version
  0.10 le type d'un bonus était lu au mauvais endroit, et valait 1 pour presque tous : un
  chargeur ramassé par un joueur pouvait faire disparaître un hypo chez l'autre, où le chargeur
  restait.
- **Le pantin ne ramasse rien par lui-même (0.33).** Un joueur qui ramasse un bonus joue une
  animation, et c'est à la frame d'action de celle-ci que le moteur lui donne le bonus le plus
  proche. Son pantin, sur les autres machines, rejoue la même animation : leur moteur lui donnait
  donc, à lui aussi, le bonus le plus proche. Or l'annonce du vrai ramassage y était déjà arrivée
  et avait retiré le bon bonus : le plus proche était alors le voisin. La machine voyait
  disparaître un bonus près de son propre joueur, l'annonçait comme ramassé par lui, et les autres
  le retiraient : un bonus perdu pour tout le monde chaque fois qu'un joueur en prenait un à côté
  d'un autre. Lu dans les journaux de la 0.32, dans l'entrepôt : l'hôte annonce un chargeur à
  01:38:36.081, le client annonce un hypo à 01:38:36.190, que personne n'a reçu ; de même à
  01:32:08, et deux fois dans ceux de la 0.31. Pour un pantin, la recherche du moteur ne trouve
  plus rien (`0x4DDD07`) : seule l'annonce du joueur qui ramasse fait foi.
- **Le sol de l'hôte fait référence** (option `npcs`). Quatre fois par seconde, si quelque chose
  y a changé, l'hôte envoie la liste de ses bonus au sol et celle de ses armes au sol. Le client
  s'y conforme : il pose les bonus qui lui manquent, retire ceux que l'hôte n'a pas, et donne aux
  armes le nombre de cartouches qu'elles ont chez l'hôte. Un bonus que le joueur du client vient
  de ramasser n'est pas reposé pendant deux secondes, le temps que l'hôte l'apprenne. Les armes
  elles-mêmes ne sont ni créées ni retirées par ce moyen. Depuis la 0.23 chaque arme de la liste
  porte son numéro, et le client corrige celle des siennes qui porte le même. L'arme que son
  joueur vient de lâcher n'est pas corrigée pendant une seconde et demie : la liste peut dater
  d'avant son annonce, et c'est sa machine qui sait ce qu'il a tiré.
- **Cartouches d'une arme lâchée.** L'annonce d'une arme lâchée porte le nombre de cartouches
  qu'elle contient encore, et l'arme tombée chez l'autre joueur le reçoit. Le personnage
  distant ayant des munitions infinies, elle y tombait pleine jusqu'à la version 0.9.
- **Numéro des armes** (depuis la 0.23). Chaque machine a sa propre table d'armes : rien ne
  disait que telle arme d'ici était telle arme de l'hôte, sinon sa classe et l'endroit où elle se
  trouvait. Cela ne distingue pas deux armes de la même classe au même endroit, et c'est le cas à
  chaque échange : le joueur lâche la sienne sur celle qu'il ramasse. Signalé en jeu en 0.22, et
  lu dans ses journaux : le client lâche une arme de 28 cartouches pour en prendre une vide ; un
  tiers de seconde plus tard la liste de l'hôte lui dit que l'arme au sol est vide, et elle le
  devient (`sol : arme 'w2_sap', 0 cartouche(s) au lieu de 28 d'apres l'hote`). Dans l'autre
  sens, une arme vide en recevait 30. Cause : chez l'hôte, la copie du client lâchait son arme
  puis prenait « la plus proche de la même classe », qui pouvait être celle qu'elle venait de
  lâcher ; le sol de l'hôte gardait alors l'arme vide, et l'hôte fait référence. Chaque arme
  porte donc un numéro, le même partout : celui du joueur qui l'a donné, puis un compteur. L'hôte
  numérote les armes de son sol quand il les liste, un joueur numérote celle qu'il lâche si elle
  n'en a pas, et toutes les annonces (lâchée, ramassée, liste du sol) le portent. Une arme encore
  inconnue ici fait connaissance par l'endroit, une fois : elle prend le numéro de l'arme de
  l'hôte la plus proche. Une arme ramassée dont le numéro n'est pas connu est cherchée à l'endroit
  comme avant, mais ce n'est jamais celle que ce joueur vient de lâcher. Un numéro ne vaut qu'à
  portée de l'endroit annoncé : au-delà, ou si l'arme est ici dans les mains d'un PNJ, il est
  retiré et l'arme refait connaissance. Les chargeurs et les cellules qu'un joueur porte sur lui
  ne sont pas concernés : ils n'existent que sur sa machine, et rien d'autre que l'inventaire
  vidé à son arrivée n'y touche.
- **Une seule arme de chaque : l'hôte fait foi** (depuis la 0.26). Signalé en 0.25 : l'arme de
  Barabas en double chez l'hôte et chez le client chaque fois qu'un coup la lui fait perdre. Chez
  un client, un PNJ lâche son arme de son côté, à l'endroit et à l'instant où il s'y trouve ;
  quand l'hôte dit ensuite qu'il la tient de nouveau, le client la cherchait à ses pieds (40
  unités) et, ne l'y trouvant pas, lui en créait une neuve en laissant la première par terre. Un
  joueur du client pouvait la ramasser, et l'hôte, qui ne la connaissait pas, en créait une à son
  tour pour le pantin. Trois corrections :
  - chez le client, chaque PNJ garde la trace de **sa** dernière arme, et la reprend où qu'elle
    soit tombée ; une arme neuve ne lui est donnée que s'il n'y a rien à reprendre, et le journal
    le dit (`arme du PNJ '...' : aucune '...' a reprendre ici, une neuve lui est donnee`) ;
  - chez le client, le sol suit celui de l'hôte : une arme de sa liste qui n'a pas été retrouvée à
    son endroit est rapprochée d'une arme d'ici de la même classe restée sans pareille, et une
    arme d'ici qui n'a aucune pareille chez l'hôte est retirée à la liste suivante, trois à six
    secondes plus tard (`sol : arme '...' absente du sol de l'hote depuis 3 s : retiree d'ici, une
    seule de chaque`). L'arme que notre joueur vient de lâcher est épargnée le temps que l'hôte
    l'apprenne ;
  - chez l'hôte, un PNJ n'a jamais deux armes : s'il en reçoit une neuve (du moteur, d'un script)
    alors que la sienne, de la même classe, est encore au sol, celle du sol est retirée. Neuve :
    créée à l'instant ; une arme qui existait déjà, il l'a ramassée, et rien n'est retiré. Le
    journal de l'hôte suit chaque arme de PNJ : `lachee`, `reprise`, `ramassee`, `recue, neuve`.

  Les bonus au sol (hypos, chargeurs, cellules, bouclier) suivaient déjà la liste de l'hôte, ajouts
  et retraits compris.

**Gâchette d'un joueur (0.32).** L'état d'un joueur dit aux autres s'il tient la gâchette, d'après
la dernière touche de tir que le moteur a notée ; leur pantin tire alors, sans compter ses
cartouches. Or le moteur ne relit pas les touches pendant une cinématique, ni quand elles sont
coupées : la dernière notée y restait, et un joueur qui tirait à l'instant où la cinématique
commence donnait chez les autres un pantin qui tirait sans fin. La gâchette n'est plus annoncée
quand les touches du joueur ne comptent pas (cinématique, touches coupées, personnage aux mains de
l'IA de déplacement), et le jeu automatique relâche la sienne dès qu'il est suspendu
(`jeu automatique : suspendu la gachette enfoncee, relachee`).

**Arme prise à un PNJ (0.29).** Une prise sur un ennemi armé peut le désarmer : son arme passe de
ses mains à celles du joueur sans toucher le sol, et aucune annonce d'arme lâchée ou ramassée n'en
parlait. Désarmé par un client, le PNJ gardait la sienne chez l'hôte, qui donnait en plus une arme
neuve au pantin de ce client ; chez le client, le PNJ en recevait aussitôt une neuve puisque l'hôte
la lui prêtait toujours. Deux armes pour une : c'était encore l'arme de Barabas en double. La
machine du joueur qui désarme le voit (l'arme qu'il a en main était, à l'image d'avant, dans les
mains d'un PNJ) et l'annonce (`proto::kReliableDisarm`). L'hôte, seul à décider de ce que tiennent
les PNJ, retire l'arme au sien sans la poser au sol, ou la retire du sol s'il venait de la lâcher ;
chaque autre client la retire à son pantin ; et aucun client ne redonne d'arme à ce PNJ tant que la
liste de l'hôte ne l'a pas dit désarmé, trois secondes au plus. Cela vaut pour tout PNJ, boss ou
non, et que le joueur soit celui de l'hôte ou d'un client. Journal : `desarmement : ...`.

### Combat

- **Dégâts : qui décide.** Les deux machines simulent les mêmes combats avec un léger décalage ;
  sans arbitrage, un coup compterait deux fois ou pas du tout. La fonction du moteur par
  laquelle passent tous les dégâts (`0x4E65F0`) est détournée, ce qui donne pour chacun
  l'attaquant et la victime. La santé d'un personnage ne change que sur la machine qui le
  simule (l'hôte pour les PNJ, chacun pour son joueur), et un coup est jugé par la machine qui
  simule l'attaquant. Un coup porté par un personnage piloté par le réseau passe donc avec des
  dégâts nuls : il garde son recul, son bruit et l'alerte de l'IA, sans toucher à la santé.
  - Le client frappe ou touche un PNJ : compté chez lui, annoncé à l'hôte, qui l'applique au
    nom du personnage du client.
  - Un PNJ frappe ou touche le joueur du client : compté chez l'hôte, sur le personnage qui le
    représente, et annoncé au client, qui l'encaisse.
  - Les joueurs ne se blessent pas entre eux, sauf si l'hôte active le tir ami.
- **Tir ami.** Réglage de l'hôte, envoyé au client à chaque connexion et à chaque changement.
  Quand il est actif, un coup ou un tir de notre joueur sur le personnage distant est annoncé à
  l'autre machine, où le vrai joueur l'encaisse, comme le sont les coups des PNJ sur le joueur
  du client. Une projection suit son propre chemin (voir « Projections ») : elle ne blesse
  l'autre joueur que si le tir ami est actif.
- **Mêlée entre joueurs.** Jusqu'à la version 0.10, seuls les tirs passaient. Le moteur ignore
  un coup de mêlée porté à un personnage d'une équipe amie (`0x4E7779`), et ne choisit pour une
  prise ni un ami ni un personnage invincible (`0x4EA314`, `0x4EA371`). Or le personnage distant
  est créé dans l'équipe du joueur, et rendu invincible : les coups le traversaient. Les deux
  fonctions sont détournées. Quand le tir ami est actif, le temps que le moteur juge un coup de
  notre joueur ou cherche la cible de sa prise, le personnage distant se présente comme un
  adversaire. Le moteur fait alors le reste : parade, dégâts, animation imposée à la cible.
  Les dégâts partent par l'annonce du tir ami. L'animation que le moteur impose au personnage
  distant (recul, chute, parade) est annoncée elle aussi, et jouée par le vrai joueur sur l'autre
  machine, comme pour les coups des PNJ. Dans l'autre sens rien ne change ici : les coups du
  personnage distant sont jugés par sa propre machine. Tout cela vaut pour l'hôte comme pour le
  client.
- **Invincibilité.** Autre réglage de l'hôte, transmis de la même façon. Quand il est actif,
  chaque machine annule tout dégât sur son propre joueur, y compris ceux que l'autre machine
  lui annonce. Seule passe encore une mise à mort directe, par une commande de script ou par
  le moteur, qui ne compte pas comme un coup.
- **Réactions aux coups.** Quand un PNJ frappe chez l'hôte le personnage du client, le moteur
  impose à ce personnage une animation (recul, chute, parade). Elle est annoncée au client, qui
  la joue sur son propre joueur. Coups et réactions sont appliqués chez le client avec le
  retard d'affichage des PNJ, pour tomber sur le geste qui les porte. Il en va de même, dans les
  deux sens, des coups de mêlée d'un joueur sur l'autre quand le tir ami est actif.
- **Projections (prises).** Le moteur décide d'une projection chez le lanceur, puis impose à la
  cible son animation de chute, qui porte les dégâts. Quand la cible est simulée par l'autre
  machine (un PNJ pour le client, le joueur d'en face pour tous), rien ne s'y passait : la
  projection est annoncée par le canal fiable (lanceur, cible, animations). Chez le
  destinataire, la cible est donnée au personnage qui représente le lanceur juste avant qu'il
  pose son animation : le moteur applique alors la projection lui-même, dégâts compris. Chez
  l'émetteur, le personnage projeté n'est plus piloté par le réseau tant que sa chute se
  déroule, sinon il serait ramené debout. L'animation du lanceur n'est pas verrouillée sur le
  personnage qui le représente : verrouillée, le moteur la rejouait à sa fin et appliquait la
  projection une seconde fois (double chute, doubles dégâts, version 0.7).
- **PNJ et joueur distant.** Le personnage distant est mis en `ai2_passive` pour que son IA ne
  le pilote pas. Or le moteur classe un personnage passif « hostile sans menace », comme un
  civil : un PNJ qui connaît aussi une vraie menace, le joueur local, n'attaque qu'elle. La
  fonction du moteur qui pose cette question (`0x48B1D0`) est détournée : pour le seul
  personnage distant, elle répond comme pour un personnage actif de son équipe.
- **Cible des PNJ : le joueur le plus proche.** Entre deux adversaires qu'il voit aussi bien, le
  moteur garde pour cible celui qui a blessé le PNJ et ne regarde les distances qu'ensuite. Un
  PNJ frappé par un joueur ne le lâchait donc plus, même avec l'autre joueur collé à lui. Chez
  l'hôte, la fonction qui ordonne deux adversaires (`0x49BA10`) est détournée : quand elle
  compare les deux joueurs, c'est la distance au PNJ qui décide. Pour changer de cible en cours
  de combat, l'autre joueur doit être plus près d'au moins 20 %, sinon le PNJ hésiterait sans
  cesse ; c'est la marge que le moteur applique lui-même. Rien ne change quand le PNJ ne voit
  qu'un des deux joueurs, ni quand un script lui impose une cible.
- **Tirs des PNJ.** L'hôte transmet pour chaque PNJ la gâchette de son arme et sa visée, comme
  pour un joueur. Chez le client le PNJ tire alors vraiment : c'est ce qui donne les tirs à
  voir et à entendre. Leurs dégâts, eux, viennent de l'hôte.
- **Tirs d'un personnage piloté par le réseau.** Pour les tirs d'une IA, le moteur va chercher
  son imprécision dans l'état de son comportement en cours (`0x51CED0`), sans vérifier qu'il
  avait prévu de tirer. Chez le client l'IA des PNJ est à l'arrêt, souvent en patrouille : le
  premier PNJ qui tirait sur ordre de l'hôte faisait planter le jeu (version 0.6). Cette
  fonction est détournée et répond zéro pour les personnages pilotés par le réseau.

### Niveau et partie

- **Zones de déclenchement.** Les scripts du niveau sont lancés par des zones que le joueur
  traverse, presque toutes réservées au joueur : le moteur y écarte tout personnage d'un autre
  type, donc celui qui représente le client chez l'hôte. Un client en avance ne déclenchait
  rien. Chez l'hôte, la fonction appelée pour chaque zone est remplacée par une enveloppe qui,
  le temps de l'appel, présente ce personnage comme un joueur. Ses scripts tournent alors chez
  l'hôte et leurs effets repartent vers le client, comme pour l'hôte lui-même.
- **Civils qui donnent un objet** (depuis la 0.24). Le moteur appelle cela une interaction
  « neutre » : la touche d'action d'un personnage, quand elle n'a trouvé ni porte ni console,
  cherche un PNJ en face de lui qui a quelque chose à dire (`0x4F81F8`, `0x48BB40`), et ce PNJ
  passe dans un état où il s'approche, parle, fait un geste et remet ce que lui donne sa fiche
  (`0x4AD1F0`). Chez un client les PNJ sont des pantins sans IA, et rien ne se passait. L'appel est
  redirigé : chez un client dont les PNJ sont ceux de l'hôte, la touche d'action envoie une demande
  à l'hôte au lieu de chercher sur place. L'hôte rejoue alors cette touche pour le pantin de ce
  client, par la fonction du moteur. Le PNJ ne connaît pour interlocuteur que « le joueur de la
  partie », lu au moment où il entre dans cet état (`0x4ACE7F`) : le temps de l'appel, c'est ce
  pantin. La conversation se déroule ensuite chez l'hôte comme avec son propre joueur, et les
  gestes du PNJ arrivent au client avec le reste de son état. Ce que le PNJ remet, le moteur le
  met dans l'inventaire du pantin (`0x4AD8CE` à `0x4ADA2E`) : l'hôte l'y reprend à l'image
  suivante et l'envoie au vrai joueur, qui le reçoit par la fonction du moteur qui donne un bonus
  (`0x51CA30`), avec son et message. Une arme donnée est retirée chez l'hôte, où le moteur l'avait
  mise dans la main du pantin ou posée aux pieds du PNJ, et le client la reçoit comme d'un script
  (`chr_giveweapon`) : celle qu'il tenait tombe, et les annonces d'armes habituelles le disent aux
  autres. Le script que la fiche du PNJ lance après la conversation tourne chez l'hôte, et ses
  effets repartent vers les clients comme ceux de tout script. Journal de l'hôte : `civil :
  'hidden_civ_1' engage la conversation avec le joueur 1, a sa touche d'action`, `civil : 1 hypo(s)
  donne(s) au pantin du joueur 1 : envoye(s) a son joueur`, `civil : conversation avec le joueur 1
  terminee` ; journal du client : `civil : 1 hypo(s) recu(s) d'un PNJ de l'hote, 1 pris`.
- **Points de sauvegarde** (depuis la 0.24). À un point de sauvegarde le script du niveau appelle
  `save_game` : le moteur note où en est son joueur, récrit `persist.dat` en entier et affiche
  « autosave » (`0x4FD690`). Chez un client cela ne servait à rien : sa reprise est celle de
  l'hôte, qui lui envoie son propre point avec son état. Et cela pouvait nuire : sur une seule
  machine les fenêtres se partagent `persist.dat`, que le client récrivait avec sa copie en
  mémoire, périmée, par-dessus la progression de l'hôte. Signalé en jeu en 0.23 : le client
  plantait en arrivant sur un point de sauvegarde. Le journal de ce client ne montre aucune faute
  à ce moment-là, seulement une à la fermeture du jeu (voir ci-dessous) : la cause n'est donc pas
  établie. Ce qui est fait : en session, un client relié à l'hôte saute sa propre commande
  `save_game` (`sauvegarde : point 2 rencontre par notre script, laisse a l'hote`), et l'hôte dit
  aux clients quand la sienne a eu lieu (`OniMP : point de sauvegarde 2 atteint, garde par
  l'hote`). Un client coupé de l'hôte garde ses propres sauvegardes.
- **Fermeture du jeu** (depuis la 0.24). En se fermant, le moteur libère ses zones de texte une à
  une et, pour chacune, ses lignes, sans regarder s'il y en a (`0x42FAF6`) ; or sa libération lit
  quatre octets avant le pointeur (`0x424B30`). Le journal du client de la 0.23 se termine ainsi :
  `EXCEPTION C0000005 a 00424b30, acces fffffffc`, appelée pour la zone des messages du niveau,
  celle où s'écrit « autosave ». Une zone restée sans lignes est maintenant écartée avant que le
  moteur n'y passe, et le journal note l'état des quatre : `fermeture : le jeu se ferme ; zones de
  texte : commande 0/1, console 0/4, messages 0/13, sous-titres 0/13,`. Une zone marquée `SANS
  LIGNES` est à signaler : c'est elle qui faisait planter, et on ne sait pas encore ce qui la vide.
- **Zones de texte du moteur : garde et réparation** (depuis la 0.25). Le client de la 0.24 a
  planté en pleine partie : `EXCEPTION C0000005 a 0043102e (Oni.exe), acces 0000020c`. `0x43102E`
  est le tracé d'une zone de texte (`0x430FD0`), à l'endroit où il lit sa première ligne : le
  pointeur de ses lignes était nul. C'est la même zone, dans le même état, que celle dont la
  libération faisait planter le client de la 0.23 à la fermeture : ce plantage-là n'était donc pas
  propre aux points de sauvegarde, et le client de la 0.24 a d'ailleurs passé un point de
  sauvegarde sans faute huit minutes plus tôt. Le moteur a quatre zones de ce genre (ligne de
  commande, console, messages du niveau, sous-titres) et ne contrôle rien en les traçant ni en y
  écrivant. **Ce qui vide cette zone chez le client n'est pas établi** : aucun des appels du
  moteur ou de Daodan qui la redimensionnent ne le fait, et cela n'est jamais arrivé chez l'hôte.
  Ce qui est fait : à chaque image, et de nouveau juste avant que le moteur ne trace ses messages,
  chaque zone est contrôlée (lignes, contexte de texte, nombre de lignes). Tant qu'elle est saine
  on en retient la forme ; dès qu'elle ne l'est plus, une zone neuve de la même forme est créée et
  mise à sa place, sans toucher à l'ancienne, et le jeu continue. Le journal garde l'instant et les
  octets de la zone trouvée abîmée : `zone de texte 'messages' du moteur abimee (plus de lignes) a
  0FD09400, saine a l'image precedente : octets ...`, puis `zone de texte 'messages' refaite a
  neuf`. Ces deux lignes sont à me donner : elles diront enfin ce qui la touche.
- **Cause du plantage du client : l'historique des commandes du moteur** (trouvée et corrigée en
  0.26). Le journal du client de la 0.25 a donné ces lignes, quatre fois, à cinq secondes d'écart,
  dix-neuf minutes après le lancement, et la garde a tenu : pas de plantage ce jour-là. La zone des
  messages, à peine refaite à `0FDDE5E0`, était retrouvée à `0FDDE500`. Un seul octet du pointeur
  changeait, toujours pour zéro, au rythme exact de la commande `ai2_allpassive 1` que le client
  exécute toutes les cinq secondes. Le moteur range chaque ligne de commande exécutée dans
  l'historique de sa console (`0x4317D0`) : 256 lignes de 512 octets, de `0x5AB388` à `0x5CB388`.
  Une fois plein, il décale tout d'une ligne puis écrit la nouvelle à la case 256, la première
  **au-delà** du tableau : sur ce qui le suit en mémoire, le drapeau « ligne de commande ouverte »
  (`0x5CB396`) et le pointeur de la zone des messages (`0x5CB398`), dont le zéro final de la
  ligne écrase le premier octet. Il faut 256 commandes pour y arriver : jamais en solo, vingt
  minutes chez un client d'OniMP. D'où un plantage toujours tardif, jamais chez l'hôte, et sans
  rapport avec les points de sauvegarde. Deux corrections : les commandes du mod ne vont plus dans
  l'historique (la case où le moteur écrit est sauvée puis rendue, les compteurs aussi) ; et les
  deux comparaisons à 256 du moteur sont ramenées à 255, ce qui corrige la faute pour les
  commandes des scripts et du joueur aussi. Journal : `historique des commandes : debordement du
  moteur corrige`. La garde des zones de texte reste en place, et ne devrait plus rien trouver.
- **Écran de texte d'une console : pour le joueur qui l'utilise** (depuis la 0.25). Jusque-là
  l'écran d'une console de données s'ouvrait chez tous dès qu'un joueur l'utilisait, et coupait les
  autres dans ce qu'ils faisaient. L'hôte, dont les scripts décident, sait quel joueur vient
  d'activer une console : la sienne par le relevé des consoles, celle d'un client par son annonce.
  La commande `text_console` n'est plus transmise d'office aux clients. Quand un script de l'hôte la
  demande, il regarde si une console a été activée dans la seconde qui précède : si oui, l'écran
  n'est ouvert que chez ce joueur (chez l'hôte seul, ou envoyé à ce seul client) ; sinon c'est une
  consigne du niveau, et il s'ouvre chez tous comme avant. Le reste du script de la console
  (objectif, porte déverrouillée, son) vaut toujours pour tous. Journal de l'hôte : `ecran de texte
  : 'level_3c' demande par la console que le joueur 1 vient d'utiliser, pour lui seul`, ou `...
  demande par un script, pour tous les joueurs`.
- **Console activée : dite par le moteur lui-même** (depuis la 0.26). En 0.25 l'écran d'une
  console utilisée par l'hôte partait encore chez tous (`'level_1a' demande par un script, pour
  tous les joueurs`), alors que celui d'un client restait bien chez lui. Une console de données se
  remet à zéro dans le tick même où elle sert, pour resservir : le relevé des consoles, fait une
  fois par image, ne la voyait jamais activée chez l'hôte, qui ne savait donc pas que son propre
  joueur venait de l'utiliser. La fonction du moteur qui active une console au nom d'un personnage
  (`0x4C0880`) est maintenant détournée : au moment où elle est appelée, avant que le script de la
  console ne tourne, l'hôte note quel joueur l'utilise, le sien ou le pantin d'un client. Une
  console éteinte ou déjà servie, que le moteur refuse, n'est pas comptée. L'écran demandé dans la
  seconde et demie qui suit n'est ouvert que chez ce joueur. Les consignes du niveau, que les
  scripts montrent sans console, restent pour tous, de même que les objectifs et les portes.
- **Tenues** (depuis la 0.25). Une tenue est une classe de personnage du moteur. Les données
  communes de l'Anniversary Edition en portent sept pour Konoko, celle du chapitre comprise ; elles
  sont chargées dans tous les niveaux. L'hôte tient la liste des joueurs (pseudo, tenue, latence),
  attribue à chacun une tenue que personne d'autre ne porte, et la redit à tous à chaque
  changement. Chaque machine donne alors à son joueur et aux doubles des autres la classe voulue,
  par la fonction du moteur qui sert à la commande de script `chr_set_class` (`0x4D7C30`), et la
  redonne après chaque chargement de niveau. Journal : `tenue : 'mp_remote1' passe de
  'konoko_generic' a 'k4_A' (joueur 1)`.
- **Munitions illimitées** (depuis la 0.25). Le moteur a un drapeau pour les personnages qui tirent
  sans compter : l'arme en main est regarnie à chaque tir (`0x51A99B`). Quand l'hôte active le
  réglage, chaque machine le pose sur son propre joueur, et le retire quand il le coupe.
- **Arrivée d'un joueur** (depuis la 0.22). Un client qui arrive a chargé son propre niveau, à son
  propre point de sauvegarde : ni les ennemis, ni les portes, ni les objectifs n'y sont ceux de
  l'hôte. Jusqu'à la 0.21 il le gardait, et l'hôte devait recharger à la main pour que tous
  repartent du même point. Maintenant le client s'annonce à sa première rencontre avec l'hôte (son
  point de sauvegarde, et si son niveau vient d'être chargé). L'hôte le dit à tous (`OniMP : le
  joueur 1 arrive : reprise au dernier point de sauvegarde dans 5 s`, puis une ligne par seconde),
  recharge son dernier point de sauvegarde, et les clients le suivent comme pour toute reprise :
  même état du niveau, mêmes objectifs. Le nouveau venu repart sans armes ni réserves (`OniMP :
  vous arrivez dans la partie sans armes`) : celles que la sauvegarde lui rend sont à l'hôte, et le
  même inventaire chez deux joueurs faisait des armes en double. Pas de reprise si les deux niveaux
  viennent d'être chargés au même point (moins de dix secondes de jeu de part et d'autre), ni avec
  `join_reload = false`.
- **Départ commun** (depuis la 0.22). Après un chargement, le moteur montre l'écran d'accueil du
  niveau et attend une touche (`0x5068BE`) ; de même à la fin d'un niveau gagné (`0x4D4914`), et
  pour quelques scripts (`splash_screen`). Chaque joueur passait cet écran à son heure, sa machine
  muette pour les autres pendant ce temps. En session ces écrans ne sont plus montrés. À la place,
  chaque machine met le jeu en pause dès son niveau chargé (le drapeau de pause du moteur, lu par
  sa boucle en `0x4D4692` : ni horloge ni simulation, l'affichage et le réseau continuent). Chaque
  client dit à l'hôte qu'il a chargé ; quand tous ceux qui étaient là l'ont dit, l'hôte donne le
  départ, et la pause tombe chez tous (`OniMP : tous les joueurs ont charge : c'est parti`). Un
  joueur resté injoignable dix secondes n'est plus attendu, et personne n'attend plus de
  vingt-cinq secondes. `ready_check = false` : pas d'attente.
- **Plus de double chargement.** Un client dont le niveau vient d'être chargé au point de
  sauvegarde que l'hôte annonce ne le recharge plus : c'était le cas à chaque passage au niveau
  suivant, que chacun atteint de son côté, jusqu'à la 0.21.
- **Mort et reprise.** Le niveau de l'hôte fait référence. À chaque connexion et à chaque
  chargement de niveau, l'hôte envoie au client le numéro de ce chargement, son point de
  sauvegarde et l'état que `save_game` y avait enregistré (position, santé, armes). Quand ce
  numéro change pour le niveau où se trouve le client, c'est que l'hôte a repris à sa dernière
  sauvegarde : le client recharge le sien au même point, avec ce même état, par le chemin
  qu'emprunte la commande de script `lose`. Quand le joueur d'un client meurt, il le signale à
  l'hôte, qui reprend à sa dernière sauvegarde deux secondes et demie plus tard ; le client suit.
  Une mort de l'hôte, rechargée par le jeu lui-même, entraîne le client de la même façon.
  Après tout rechargement, le client ouvre une nouvelle connexion : l'hôte lui renvoie alors
  ce que ses scripts ont fait depuis son propre chargement. Tant que l'autre joueur est
  connecté, l'écran d'échec du jeu n'est pas affiché avant le rechargement : il dure jusqu'à
  vingt secondes sans appui sur une touche, pendant lesquelles rien ne tourne.
- **Onglets Coop et Display du menu de pause.** Le menu F1 est un dialogue du moteur : cinq
  boutons pour onglets et des zones où il dessine la page. Nos onglets sont des boutons créés
  sous les autres (Coop puis Display chez l'hôte, Display seul chez le client), et pour chacun
  deux boutons de réglage prennent la place de la page quand il est choisi. Ils sont créés par
  la fonction du moteur qui crée les autres (`0x475160`), avec le type, le style et la police
  lus dans les données du dialogue. La fonction de messages du dialogue (`0x501990`) est
  remplacée par une enveloppe qui traite ces boutons et passe le reste au moteur. Si le
  dialogue n'a pas la forme attendue, rien n'est ajouté et les commandes de console restent.
- **Curseurs de difficulté.** Ce sont les curseurs du moteur, ceux de son dialogue Options
  (qualité, gamma, volume) : un contrôle de type 17, créé par la même fonction que les boutons,
  avec au-dessus un contrôle de texte, de type 20, pour l'étiquette. Sa plage et sa position se
  posent par les fonctions qu'emploie le dialogue Options (`0x472650`, `0x4723B0`). Quand le
  curseur bouge, à la souris ou au clavier, il envoie au dialogue le message des boutons, avec le
  code 1 et lui-même en paramètre ; l'enveloppe y lit sa position (`0x4723A0`). Un cran vaut 10
  points de vie pour cent, ou 5 de dégâts, par client. La table de difficulté de l'hôte change
  aussitôt, donc la vie de ses PNJ ; les clients sont prévenus dix fois par seconde au plus
  pendant qu'un curseur glisse, et le fichier n'est écrit qu'une fois le curseur au repos, ou le
  menu fermé. Rien de neuf sur le réseau : les clients reçoivent, comme avant, la vie et les
  dégâts du moment. Si les curseurs ne peuvent pas être créés, l'onglet garde ses boutons et le
  journal le dit (`menu : curseurs de l'onglet Coop refuses`).
- **Mode de difficulté.** Le moteur lit la difficulté par une petite fonction (`0x50F240`) chaque
  fois qu'il en a besoin : vie d'un personnage à sa création (`0x4DB352`, facteurs lus dans les
  réglages du jeu, instance `ONGS`), attention et parades des IA, précision de leurs tirs, classe
  plus forte de certains ennemis en difficile. Cette fonction est détournée : en session elle
  répond la difficulté du mode choisi par l'hôte, et celle des options du jeu sinon. Rien n'est
  écrit dans `persist.dat`. Le détour vaut dès le lancement, d'après `onimp.ini` : le premier
  niveau crée ses personnages avant que la session s'ouvre. Le moteur n'appliquant la difficulté à
  la vie d'un ennemi qu'à sa création, OniMP retient pour chaque PNJ le facteur qui valait alors,
  et porte sa vie au facteur du moment quand l'hôte change de mode. Ce que le mode survie ajoute
  (vie, dégâts) passe par les mêmes pourcentages que la difficulté selon le nombre de clients ; la
  régénération est faite une fois par seconde par la machine qui simule les PNJ.
- **Joueur à terre.** Le moteur a de quoi arrêter un coup mortel : un personnage « increvable »
  (drapeau que pose `chr_unkillable`) garde un point de vie (`0x4E6921`), et le moteur le jette
  au sol (`0x4E6E83`). Tant qu'un coéquipier est debout dans le même niveau, chaque machine pose
  ce drapeau sur son propre joueur. Quand sa santé tombe à un point, il est à terre : ses touches
  sont coupées par le drapeau qu'emploient les cinématiques (commande de script `input`), il est
  rendu invincible, verrouillé dans son animation au sol, et son compte à rebours commence. Son
  état, envoyé trente fois par seconde comme d'habitude, porte alors un drapeau « à terre » et,
  à la place de sa santé, le temps qui lui reste en dixièmes de seconde : c'est ce que les autres
  machines affichent au-dessus de lui, sans message de plus. Chez l'hôte, un joueur à terre ne
  compte plus comme une menace pour les PNJ, et entre deux joueurs ils choisissent celui qui est
  debout. Sans coéquipier pour ranimer, le drapeau est retiré et le joueur meurt normalement.
- **Réanimation.** Elle est jugée sur la machine de celui qui ranime : joueur à terre à portée,
  CTRL enfoncé (lu au clavier, et seulement si la fenêtre a le clavier, plusieurs fenêtres du jeu
  pouvant tourner sur une machine), pas de coup d'ennemi reçu. Trois messages fiables,
  redistribués par l'hôte comme les autres messages entre joueurs : début (le compte à rebours
  s'arrête chez le joueur à terre), abandon (il reprend), réussite (le joueur se relève avec la
  santé du mode). Si celui qui ranime disparaît ou ne donne pas suite en cinq secondes, le compte
  à rebours reprend de lui-même. À zéro, la machine du joueur à terre le tue et demande l'échec
  de la mission : l'hôte le dit à tous, avec la raison, puis recharge sa dernière sauvegarde, et
  les clients suivent comme pour une mort.
- **Renforts alignés sur leur original** (depuis la 0.22). Un renfort naît de l'objet du niveau,
  tel que le niveau l'a écrit : il n'a rien de ce que les scripts ont fait de son original depuis,
  ni de ce que celui-ci a appris. Signalé en jeu en 0.21 : des ennemis inertes, qui erraient, ou
  pris pour des alliés. Le garde de la fin de l'entrepôt, par exemple, naît dans l'équipe de
  Konoko et un script le passe au Syndicat : sa copie restait une alliée. Un renfort prend donc
  l'équipe qu'a son original au moment de sa création, puis, deux fois par seconde tant que
  l'original est là, il est aligné sur lui : même équipe, même passivité (figurant d'une scène),
  même consigne d'ignorer le joueur, et la connaissance des joueurs que l'original tient pour des
  menaces, pour qu'il entre dans le combat avec lui au lieu de patrouiller de son côté. Un ennemi
  n'est plus multiplié qu'après deux secondes de vie au lieu d'une, le temps que le script qui l'a
  fait apparaître l'ait réglé.
- **Renforts.** La machine qui simule les PNJ passe ses personnages en revue à chaque image. Pour
  un ennemi qui peut être multiplié (voir « Multiplicateur d'ennemis et civils »), elle retrouve
  son objet de niveau par son nom, comme le fait `ai2_spawn` (`0x48A450`), en fait une copie en
  mémoire (autre nom, position de l'ennemi à cet instant, plus aucun script ni drapeau de boss ou
  d'increvable) et la donne à la fonction du moteur qui crée un personnage (`0x4DAC50`). Le
  renfort est donc un personnage ordinaire du moteur, avec l'IA, l'arme et le rôle de l'original.
  Une création par image au plus. Avec des PNJ partagés, l'hôte l'envoie aux clients avec ses
  autres PNJ ; un client qui reçoit un nom inconnu de son niveau y reconnaît un renfort (`_x2`) ou
  un civil (`mpc`), et le crée de la même façon au lieu de le demander au niveau.
- **Civils.** Créés comme le personnage d'un autre joueur, à partir d'un objet de niveau
  construit en mémoire : une classe de civil, une équipe neutre, le drapeau « ne se bat pas ».
  Leur nom porte leur classe, pour que les clients créent le même. La machine qui simule les PNJ
  note qui les blesse (son joueur par le filtre des dégâts, celui d'un client par ses annonces) ;
  un civil mort dans les trois secondes qui suivent le coup d'un joueur fait échouer la mission.
### Positions qui ne sont pas des nombres

Dans la 0.28, les deux clients d'une partie à trois se sont figés ensemble (`Oni.exe ne répond
pas`), juste après le sous-titre « Shinatama : Uploading new objective data ». Leur mémoire, lue
pendant qu'ils étaient figés, a donné la suite des faits :

- la position, la place et l'orientation du joueur de chaque client valaient « NaN », ce que donne
  un calcul impossible (zéro divisé par zéro). Une fois là, il ne s'en va plus : tout ce que le
  moteur en tire l'est à son tour ;
- une place « NaN » passe pour être dans toutes les zones du niveau à la fois : elles se sont
  toutes déclenchées d'un coup, chez le client puis chez l'hôte par le pantin du client (`sauvegarde
  : point 3 atteint` six secondes après une reprise au point 2, vingt-quatre PNJ de plus à piloter
  dans la même seconde), dont celle qui donne un nouvel objectif et allume la boussole ;
- la boussole du HUD ramène son angle entre 0 et un tour par une boucle « tant qu'il est négatif,
  ajouter un tour » (`0x500860`). Pour un angle « NaN » elle ne finit jamais : le jeu est figé.

Ce qui a produit le premier « NaN » n'est pas établi. Le même ennemi (`Floor1_Striker_2_x2`, que
les deux personnages en jeu automatique frappaient) en portait un dans les deux mémoires, de signe
opposé à celui des joueurs, et l'hôte tirait au même moment au projecteur de phase. Depuis la
0.29, à chaque maillon de la chaîne :

- `mem::WriteTransform` refuse une position ou une orientation qui n'est pas un nombre, et réécrit
  en entier celle d'un personnage qui n'en est plus un, au lieu de la déplacer ;
- un état reçu du réseau dans ce cas est écarté (`NAN : etat recu avec une position invalide`),
  et aucun ne part d'ici ;
- la poussée d'un coup qui n'est pas un nombre est retirée, le coup restant compté (`NAN :
  poussee invalide d'un coup sur '...'`) ;
- le jeu automatique ne tourne plus son personnage vers un point invalide ;
- la place de chaque personnage est notée quand elle est saine, à chaque image, et à chaque tick
  pour notre joueur ; celui dont la place ne l'est plus y est ramené, élans remis à zéro, avant que
  le moteur ne trace l'image (`NAN : position de 'konoko' invalide au tick (...) : remis en
  (...)`). Cette ligne dit qui a été touché le premier, et dans quelle animation : c'est elle qui
  donnera la cause ;
- chez l'hôte, un pantin dans cet état ne se présente plus aux zones du niveau.

### Paquet d'état

```cpp
struct EntityState {
    float x, y, z;
    float rotation;        // radians
    uint32_t animID;       // hash du nom de l'animation
    uint32_t characterID;  // joueur : son numéro, 0 = hôte, 1 à 3 = clients ; PNJ = hash du nom
    uint16_t animFrame;
    int16_t health;        // joueur à terre : dixièmes de seconde avant sa mort
    uint16_t flags;        // actif, mort, tir, tir secondaire, à terre, ranimé, recharge, durée du fondu
    uint16_t tick;         // horloge de jeu de l'émetteur (60 par seconde)
    int16_t aimYaw;        // direction de l'arme
    int16_t aimPitch;
    uint8_t super;         // aura, de 0 à 255
    uint8_t shield;        // bouclier de force : ce qu'il en reste, 100 neuf
    uint8_t cloak;         // invisibilité : temps qui reste, par pas de huit ticks
    uint8_t ammo;          // joueur : chargeurs (4 bits bas) et cellules (4 bits hauts) en réserve
};
```

Envoyé une trentaine de fois par seconde pour le joueur, quinze pour les PNJ. Tout ce qui ne
doit pas se perdre (armes, coups, projections, scripts, réglages, reprise) passe par un canal
fiable et ordonné, décrit dans `src/protocol.h`.

Tous les joueurs doivent utiliser la même version d'OniMP : un paquet d'une autre version du
protocole est ignoré. La 0.39 (protocole 25 : laser coupé annoncé par la machine du joueur, rayons
des lasers mobiles) ne se connecte pas à une version antérieure : le client d'une autre version
est refusé avec un message. La 0.38 avait le protocole 24 (laser coupé annoncé aux clients, IA de
l'hôte tenues passives), la 0.37 le protocole 23 (cible des tourelles), la 0.36 le protocole
22 (état des lasers et des tourelles, numéro du niveau de l'hôte), la 0.35 le protocole 21 (cheats, personnage incarné, apparitions), la 0.29
le 20 (arme prise à un PNJ).
Les 0.19, 0.20 et 0.21 partageaient le protocole 15 ; la 0.22 avait le 16, la 0.23 le 17, la 0.24
le 18, les 0.25 à 0.28 le 19.

## PNJ pilotés par l'hôte

Désactivé par défaut : sans cette option, chaque joueur a ses propres ennemis. Pour
l'essayer, lancez `run_host_npcs.bat` et `run_client_npcs.bat`. Pour l'activer durablement,
passez `npcs = true` dans `onimp.ini` **des deux côtés** (ou `mp_npcs 1` dans la console).

- **PNJ gardés actifs près de chaque joueur** (depuis la 0.28). Signalé en 0.27 : vus de loin,
  les PNJ flottaient, glissaient et tremblaient, surtout chez le client. Le moteur met en sommeil
  un PNJ loin de **son** joueur : plus de physique ni d'animation, il avance par à-coups le long
  de son chemin sans suivre le sol. Chez l'hôte, un PNJ loin de lui mais près d'un client était
  dans cet état, et c'est cet état que le client recevait. L'hôte garde maintenant actif tout PNJ
  à moins de 600 unités d'un autre joueur (ce que fait `chr_lock_active`), et le rend au moteur
  au-delà de 800 ; ces PNJ voient et combattent donc aussi un client éloigné de l'hôte. Chez le
  client, chaque PNJ piloté est gardé actif, pour jouer les animations que l'hôte lui donne.
  Journal de l'hôte : `pnj : 3 garde(s) actif(s) pres d'un autre joueur, loin du notre`.
- **PNJ en double** (depuis la 0.28). Un PNJ d'ici que l'hôte ne connaît pas, alors qu'il en
  connaît un du même nom, est un double : il n'est plus tracé, et il est retiré en moins d'une
  demi-seconde au lieu de trois (`pnj orphelin : suppression de 'Barabus', en double d'un PNJ que
  l'hote connait`).
- L'hôte envoie l'état de tous ses PNJ, identifiés par leur nom.
- Le client coupe son IA (`ai2_allpassive 1`) et pilote ses PNJ de même nom avec les états
  reçus. Un PNJ absent chez lui est créé avec `ai2_spawn`. Un PNJ que l'hôte ne connaît pas
  est supprimé au bout de trois secondes (`delete_orphans`).
- La santé des PNJ est décidée par l'hôte. Un coup porté par le client est envoyé à l'hôte
  comme événement de dégâts. Une projection faite par le client est annoncée à part : c'est
  l'hôte qui la rejoue et en compte les dégâts.
- La mort d'un PNJ aussi est décidée par l'hôte. Chez le client, un coup du joueur ne l'achève
  plus sur place : il lui laisse un point de vie jusqu'à ce que l'hôte annonce sa mort. Jusqu'à
  la version 0.10 le PNJ mourait chez le client, était ramené à la vie par l'état suivant de
  l'hôte, puis tué de nouveau, et laissait tomber deux fois ses chargeurs et ses hypos.
- **Vie selon le nombre de clients.** Chaque PNJ, boss compris, a la vie que le fichier de
  difficulté de l'hôte donne pour le nombre de clients présents : deux fois celle d'origine avec
  un client, trois avec deux, quatre avec trois si le fichier n'a pas été modifié. Son maximum et
  sa santé du moment suivent ensemble, dans la même proportion, quand un client arrive ou s'en va,
  et de même pour tout PNJ qui apparaît ensuite. Jusqu'à la version 0.14 la vie était doublée dès
  le premier client et n'allait pas plus loin.
- **Dégâts « en ligne ».** Si l'hôte garde ce réglage, les dégâts qu'un PNJ inflige à un joueur
  suivent le même fichier : 105, 110 puis 115 % par défaut. Le coup est changé chez l'hôte, qui
  simule les PNJ, qu'il touche son joueur ou celui d'un client ; ce que les joueurs infligent ne
  change pas. Un coup ne faisant souvent que quelques points, ce que l'arrondi écarte est gardé
  pour le coup suivant porté au même joueur : le pourcentage est tenu au point près sur la durée.
- **Qui compte.** L'hôte compte ses clients, lit dans son fichier ce que ce nombre vaut, et donne
  le tout aux autres machines, qui en ont besoin pour le maximum de leurs jauges. Un client qui
  s'en va compte encore vingt secondes : il ne fait souvent que recharger son niveau, et la vie
  des PNJ ne doit pas varier pour si peu. La console le dit à chaque changement :
  `OniMP : PNJ : vie x3.00, degats x1.10 (2 client(s))`.
- Les bonus au sol sont ceux de l'hôte (voir « Le sol de l'hôte fait référence ») : un PNJ tué
  chez le client n'y laisse que son arme, ses bonus viennent de la liste de l'hôte.
- Une projection d'un PNJ sur le joueur du client est annoncée de la même façon, de l'hôte vers
  le client, qui l'applique à son propre joueur.
- La santé de chaque joueur est décidée sur sa propre machine. Les coups et les tirs des PNJ
  sur le joueur du client sont jugés par l'hôte, qui les lui annonce.
- Chez l'hôte, un PNJ qui voit plusieurs joueurs s'en prend au plus proche.
- Chez le client, les PNJ visent et tirent quand l'hôte le dit, avec des munitions infinies :
  ils n'ont pas d'IA pour recharger.

**PNJ retiré par l'hôte (0.30).** Un script du niveau peut retirer un personnage (`chr_delete`) ;
l'hôte fait rejouer la commande au client, qui retire le sien. Mais les derniers états reçus de
l'hôte donnaient encore ce PNJ en vie, et le client, ne le trouvant plus chez lui, le recréait
aussitôt. C'était Barabas après la cinématique de sa défaite : recréé à l'instant où elle finit,
replacé d'après les derniers états reçus, donc en l'air, il retombait et restait planté là (journal du
client de la 0.29 : `ai2_spawn Barabus force` à la même milliseconde que `cinematique : finie`).
Et il y restait tant que l'hôte n'avait aucun autre PNJ : sans PNJ l'hôte n'envoyait plus de relevé,
ce que le client prenait pour un silence, pendant lequel il ne retire rien (`pnj orphelin :
suppression de 'Barabus'` vingt-trois secondes plus tard, trois secondes après l'apparition des
gardes de l'étage). Depuis la 0.30 : un PNJ disparu d'ici n'est recréé que si l'hôte le donne encore
une demi-seconde après ; et l'hôte sans PNJ envoie un relevé vide, qui le dit.

## Quêtes : scripts de niveau décidés par l'hôte

Désactivé par défaut. Pour l'essayer, lancez `run_host_coop.bat` et `run_client_coop.bat`
(cette option active aussi les PNJ pilotés par l'hôte). Pour l'activer durablement :
`quests = true` dans `onimp.ini` des deux côtés. L'option est lue au lancement du jeu.

**Rechargement : le journal des scripts repart de zéro** (depuis la 0.28). L'hôte garde les
commandes d'état du niveau pour les rejouer à un client qui arrive. Il vidait ce journal quand
l'horloge du niveau repartait en arrière ; or un point de sauvegarde qui rejoue ses commandes au
chargement les enregistre toutes à l'instant zéro, et d'une reprise à la suivante cet instant
n'était jamais « avant » le précédent. Le journal gardait donc les commandes de chaque reprise,
et le client, qui se reconnecte à chaque rechargement, les rejouait toutes : au niveau `lab`,
16 commandes puis 32, 48... jusqu'à 176, et un Barabas de plus à chaque reprise, retiré trois
secondes plus tard (58 fois dans le journal du client de la 0.27). L'horloge se compare
maintenant à ce qu'elle valait à la dernière image : tout rechargement vide le journal, avec ou
sans commande entre-temps (`quetes : niveau recharge, journal des 16 commande(s) d'etat vide`).
Les personnages, eux, sont recréés par le moteur à chaque chargement, chez l'hôte comme chez le
client.

Dans Oni, tout ce qui fait avancer un niveau passe par ses scripts : objectifs, portes
verrouillées, apparition des ennemis, dialogues, cinématiques, fin de niveau. Sans cette
option, chaque joueur fait tourner les siens et les deux parties divergent.

Avec l'option :

- **Chez l'hôte**, les scripts tournent normalement. Chaque commande qui modifie le niveau
  est transmise au client, dans l'ordre, par un canal fiable.
- **Chez le client**, les scripts locaux continuent de tourner mais leurs commandes à effet
  sont neutralisées : seules celles reçues de l'hôte s'exécutent. Tant que l'hôte n'est pas
  connecté, le niveau du client reste donc inerte.
- **Arrivée en cours de niveau.** L'hôte garde le journal des commandes d'état exécutées
  depuis le début du niveau et le rejoue au client qui se connecte. Les effets ponctuels
  (dialogues, sons, animations) ne sont pas rejoués.
- **Actions du client.** Le personnage qui représente le client chez l'hôte déclenche les zones
  du niveau comme le joueur de l'hôte (voir « Zones de déclenchement »). Jusqu'à la version 0.7
  il était écarté des zones réservées au joueur, c'est-à-dire de presque toutes.
- **Portes.** Une porte qui s'ouvre chez un joueur, quelle qu'en soit la cause (proximité,
  touche d'action), s'ouvre chez l'autre.
- **Consoles.** Une console activée par un joueur est activée chez l'autre, au nom du
  personnage qui le représente. Chez l'hôte, son script s'exécute donc comme s'il l'avait
  utilisée lui-même, et ses effets repartent vers le client.
- **Portes et consoles de même numéro.** Plusieurs objets d'un niveau peuvent porter le même
  identifiant : dans l'entrepôt, les deux consoles d'alarme d'une salle ont toutes deux le 7.
  Jusqu'à la 0.20 OniMP ne les suivait que par ce numéro : l'état de l'une écrasait celui de
  l'autre à chaque image, la première passait sans fin pour « tout juste activée », et les deux
  machines se la renvoyaient à chaque image (99 000 annonces en dix minutes dans une partie de la
  0.20 ; le client a planté au bout de ces dix minutes, sans que le lien soit établi). Depuis la 0.21 chacune est suivie par son numéro et son rang
  parmi ses homonymes, et un même objet n'est jamais annoncé deux fois en une demi-seconde.
- **Fin de niveau.** La commande de victoire est transmise : les deux joueurs passent au
  niveau suivant.
- **Objet du niveau.** Certains niveaux font ramasser un objet (LSI) que leurs scripts cherchent
  ensuite sur le joueur de la machine où ils tournent (`chr_has_lsi 0`), donc sur celui de l'hôte.
  Ramassé par un client, il manquait à l'hôte : la porte qui l'attend ne s'ouvrait pas et le
  niveau restait bloqué (signalé en jeu en 0.21). Depuis la 0.22 chaque machine le donne à son
  propre joueur dès qu'un autre joueur l'a ramassé (`OniMP : le joueur 1 a ramasse l'objet du
  niveau`) : c'est l'objet de toute l'équipe.

Le classement des commandes (état, ponctuelle, locale) est dans `src/scriptcodec.cpp`. Avec
`trace = true`, le journal liste les commandes interceptées au démarrage et chaque commande
reçue par le client.

Les commandes tapées dans la console développeur du client sont neutralisées comme celles de
ses scripts.

## État de validation

« Vu en jeu » : constaté dans une vraie partie. « Vu dans les journaux » : lu dans les journaux
d'une vraie partie, sans retour sur ce qui se voyait à l'écran. Le détail par objectif est dans
[OBJECTIFS.md](../OBJECTIFS.md).

| Élément | État |
|---|---|
| Compilation MinGW i686, exports Bink renvoyés (83 sur 83) | vérifié |
| Chargement par Daodan 4.1 ; toutes les modifications d'Oni.exe acceptées | vu dans les journaux de la 0.10, chez l'hôte et chez le client : aucune ligne `inattendu` |
| Commandes `mp_*` | enregistrement vérifié, exécution **non testée** |
| Lecture mémoire : niveau, position, orientation, animation, santé | vérifié en jeu (niveau `EnvWarehouse`) |
| Deux instances sur la même machine | vérifié en jeu |
| Protocole réseau : connexion, états, événement fiable, déconnexion | vérifié avec `onimp_nettest` |
| Connexion entre deux instances du jeu | vérifié en jeu |
| Apparition et pilotage du joueur distant, animations (0.1) | vérifié en jeu, sauf le sprint |
| Interpolation et sprint (0.2) | vérifié par simulation (`onimp_timelinetest`) |
| PNJ pilotés par l'hôte (0.3) | vu en jeu : PNJ synchronisés |
| Quêtes : canal fiable ordonné, format des commandes, stubs d'interception (0.3) | vérifié hors jeu (`onimp_nettest`, `onimp_codectest`, `onimp_stubtest`) |
| Quêtes : interception dans le moteur, exécution chez le client (0.3) | vu dans les journaux, encore en 0.10 : 301 commandes interceptées, 20 rejouées au client à sa connexion, aucun plantage |
| Portes, consoles (0.4) | **non testé en jeu** |
| Arme en main et tirs du joueur distant (0.4) | vu en jeu ; les tirs à tort pendant les coups, corrigés en 0.5, n'ont plus été signalés |
| PNJ visant aussi le joueur distant (0.5) | vu en jeu |
| Coups du client comptés chez l'hôte, projections du client (0.5) | vu dans les journaux |
| Reconnexion vue d'un seul côté (0.6) | vérifié hors jeu (`onimp_nettest ... stall`) ; le défaut avait été vu en jeu en 0.5 |
| Détours et remplacements d'adresse | vérifié hors jeu (`onimp_hooktest`) : le mécanisme, pas son effet dans le jeu |
| Coups des PNJ sur le client, réaction imposée (0.6) | vu dans les journaux, encore en 0.10 : `coup recu`, `reaction ... imposee par l'hote` |
| Projection par un PNJ sur le client (0.6) | vu dans les journaux ; une seule chute depuis la 0.8 : **non testé en jeu** |
| Dégâts arbitrés : plus de coup compté deux fois (0.6) | **non vérifié** |
| Tirs des PNJ chez le client (0.6, plantage corrigé en 0.7) | vu dans les journaux de la 0.7 : dégâts reçus, plus de plantage |
| Armes lâchées et ramassées, bonus ramassés (0.6) | vu en jeu en 0.9 : l'arme lâchée par un joueur a été ramassée par l'autre (elle était pleine, corrigé en 0.10) |
| Reprise à la dernière sauvegarde de l'hôte après la mort du client (0.7) | vu dans les journaux : demandée, hôte rechargé, client rechargé au même point de sauvegarde |
| Rechargement visible chez l'autre joueur (0.8) | **non testé en jeu** |
| Zones de déclenchement ouvertes au client (0.8) | **non testé en jeu** |
| Direction de l'arme imposée dans le moteur, visée automatique et recul transmis (0.9) | vu dans les journaux de la 0.10 : le détour s'applique au personnage distant des deux côtés, et à un PNJ chez le client. Résultat à l'écran : **pas de retour** |
| PNJ qui s'en prennent au joueur le plus proche (0.9) | vu dans les journaux de la 0.10 : `cible : '...' quitte '...' pour '...', plus proche`, dans les deux sens |
| Cartouches restantes d'une arme lâchée (0.10) | vu dans les journaux : même nombre annoncé chez l'un et reçu chez l'autre, dans les deux sens |
| Jauge et santé de l'autre joueur au-dessus de sa tête (0.10) | vu en jeu : la barre convenait, les chiffres s'affichaient sur un rectangle blanc ; refait en 0.11 |
| Tir ami et invincibilité décidés par l'hôte (0.10) | tir ami : vu dans les journaux pour les tirs de l'hôte sur le client (`tir ami : ...` chez l'un, `coup recu : ... de 'mp_remote'` chez l'autre). Invincibilité : vu dans les journaux de la 0.13, plus aucun `coup recu` après `invincibilite activee` alors que les coups continuent d'être annoncés |
| Onglet Coop du menu F1 (0.10) | **pas de retour** ; les journaux montrent des réglages changés chez l'hôte et suivis par le client, sans dire si c'était par le menu ou par la console |
| Connexion gardée pendant le menu F1 (0.10) | **non testé en jeu** au-delà de cinq secondes |
| Chargement de la 0.11 : détours de la mêlée, jauges | vu dans les journaux de la 0.11, hôte et client, deux niveaux : aucune ligne `inattendu`, `EXCEPTION` ni `jauge : textures refusees` |
| Prises entre joueurs avec le tir ami (0.11) | vu dans les journaux, dans les deux sens : `projection : 'char_0' projette 'mp_remote', annoncee` chez l'un, `projection : 'mp_remote' projette 'char_0'` chez l'autre |
| Coups de mêlée entre joueurs avec le tir ami (0.11) | vu dans les journaux de la 0.13, dans les deux sens : `tir ami : 4 sur l'autre joueur, annonce` et `reaction : 'KONCOMhit_head1' imposee au joueur distant, annoncee` chez celui qui frappe, `reaction : '...' imposee par l'autre machine` chez l'autre |
| Jauges refaites : barre colorée, chiffres dessus, plus de fond blanc (0.11) | **pas de retour** sur ce qui se voit ; tracé des chiffres vérifié hors jeu |
| Santé des PNJ au-dessus d'eux (0.11) | **pas de retour** |
| Vie des PNJ doublée à deux (0.11) | vu dans les journaux, hôte et client : `pnj : vie multipliee par 2 pour 6 personnage(s)`, puis pour le PNJ du niveau suivant |
| Onglet Display du menu F1, chez l'hôte et le client (0.11) | vu sur disque : `onimp.ini` porte la section `[display]` que seul un changement en jeu écrit. Par le menu ou la console : pas de retour |
| Bonus au sol identiques chez les deux joueurs, plus de butin en double chez le client (0.11) | **pas de retour** : aucune ligne `sol :` ni PNJ tué dans ces journaux |
| Annonce de l'hôte et recherche automatique du client (0.12) | vu dans les journaux de la 0.12, deux fenêtres sur une seule machine, par la carte Radmin : `partie annoncee sur 26.x.x.x (Radmin VPN)` chez l'hôte, `partie 'Oni' trouvee a 26.x.x.x:27777 (niveau 'EnvWarehouse') : connexion` puis `pair connecte` chez le client. **Non testé entre deux machines** |
| Refus explicites : partie complète, autre version (0.12) | vérifié hors jeu : client de trop refusé et prévenu ; appel en protocole 99 refusé avec la raison « version ». Partie complète : lu aussi dans le journal du client en 0.13 |
| Reconnexion d'un client en recherche automatique après un hôte figé (0.12) | vérifié hors jeu (`onimp_nettest ... auto ... stall`) ; vu dans les journaux de la 0.13 après un changement de niveau : `l'hote ne repond plus : nouvelle recherche`, puis la partie retrouvée |
| Détecteur de désynchronisation : règles de décision (0.12) | vérifié hors jeu (`onimp_desynctest`) |
| Détecteur de désynchronisation : empreintes calculées en jeu, renvois (0.12) | vu dans les journaux de la 0.13 : un écart sur les réglages détecté chez l'hôte et renvoyé au client (`desync : reglages differe chez le client (0000000F ici, 0000000C chez lui), renvoi 1/3`), rien d'autre en vingt minutes. C'était une fausse alerte, due au menu de pause de l'hôte ; corrigée en 0.14 |
| Scripts qui visent le jeu tenu à jour par l'AE Installer (0.13) | vu dans les journaux de la 0.13 : ils sont écrits dans le sous-dossier `AE` et portent ses mods |
| Liste des mods de l'AE Installer dans le journal (0.13) | vu dans les journaux de la 0.13, hôte et client : `mods AE : 10 paquet(s), empreinte B5680635 : ..., 34090 Blood` |
| Mods comparés entre l'hôte et le client (0.13) | vu dans les journaux de la 0.13, des deux côtés et à chaque reconnexion : `mods AE : memes paquets chez l'autre joueur (10, empreinte B5680635)`. Le message pour des mods différents : **jamais vu**, comparaison vérifiée hors jeu (`onimp_modtest`) |
| Partie à deux avec un mod de l'AE Installer (0.13) | vu dans les journaux de la 0.13 : vingt minutes sur deux niveaux avec Blood installé, sans `EXCEPTION`. Sang à l'écran : **pas de retour**. Mods de personnages ou d'IA : **non testés** |
| Passage au niveau suivant à deux (0.3) | vu dans les journaux de la 0.13 : `niveau : 'lab'` chez l'hôte puis chez le client, connexion reprise, PNJ pilotés. Le client charge ce niveau deux fois |
| Quatre joueurs : liaisons de l'hôte, numéros, états et messages redistribués, client de trop refusé (0.14) | vu dans les journaux de la 0.15, un hôte et trois clients sur une machine : `pair connecte (...), joueur 1`, puis 2, puis 3 chez l'hôte, `ici joueur N` chez chaque client. Client de trop refusé : vérifié hors jeu seulement |
| Réseau de l'hôte servi pendant son menu de pause (0.14) | vu dans les journaux de la 0.15 : un réglage changé dans le menu F1 de l'hôte arrive au client deux millisecondes plus tard (`reglage : degats en ligne desactives`, `difficulte : PNJ : vie x2, degats normaux`), menu encore ouvert. Remplacé en 0.16 par le jeu qui continue derrière les menus |
| Un double par joueur, dégâts, tir ami, armes, objets et zones du niveau entre plusieurs joueurs (0.14) | vu dans les journaux de la 0.15 : chaque machine crée un double pour chacun des trois autres (`spawn : 'mp_remote0'` à `'mp_remote3'`) ; un PNJ passe d'un client à l'autre (`cible : 'A_t48' quitte 'mp_remote2' pour 'mp_remote3', plus proche`) ; ses coups arrivent au bon client (`reaction : ... imposee au joueur 3, annoncee`, `coup recu : 6 de 'A_t48'` chez le client 3 seul) ; `degats client 3 : 4 sur 'A_t48'`. Tir ami et armes entre deux clients : **pas de retour** |
| Détecteur de désynchronisation par client, rapports en rafale ignorés (0.14) | règles vérifiées hors jeu (`onimp_desynctest`). **Non testé en jeu** |
| Jauge des autres joueurs sans chiffres (0.14) | **non testé en jeu** |
| Vie des PNJ selon le nombre de clients, dégâts « en ligne », compte des clients (0.15) | vu dans les journaux de la 0.15, sur les quatre machines : `difficulte : PNJ : vie x2, degats x1.05 (1 client(s))`, puis `x3` / `x1.10`, puis `x4` / `x1.15` ; deux coups de 6 points reçus 6 puis 7 par le client 3, soit 15 % de plus ; bouton `Online damage` du menu F1 basculé et suivi par le client ; un client parti compté encore vingt secondes |
| Corrections de la revue de code (0.15) | **non testé** : états d'un joueur revenu après une longue absence, message trop long d'un client, reprise demandée deux fois par deux clients morts à la suite. Les tests réseau existants passent toujours |
| Un journal par instance sur une même machine (0.14) | vu sur disque après la partie à quatre de la 0.15 : `onimp_client.log`, `onimp_client_2.log`, `onimp_client_3.log` |
| Difficulté lue dans `onimp_difficulty.ini` (0.16) | lecture, valeurs partielles et bornes vérifiées hors jeu (`onimp_difficultytest`, avec le fichier installé). Relecture en cours de partie, effet sur les PNJ : **non testé en jeu** |
| Jeu qui continue derrière les menus (0.16) | **non testé en jeu**. Vérifié dans l'exécutable : l'appel détourné est bien celui de la boucle des dialogues, dans les deux copies d'`Oni.exe`, et Daodan n'y touche pas |
| Jauges au-dessus des autres joueurs et des PNJ (0.11) | vu en jeu en 0.16 : présentes chez l'hôte et chez le client, sur les joueurs et sur les PNJ, mais elles clignotaient |
| Jauges qui ne clignotent plus (0.17) | **non testé en jeu**. Cause lue dans l'exécutable : rectangles transparents triés par profondeur en fin d'image, tous à la même. Tracé immédiat et couches décalées : écrit, pas vu |
| Fichier de difficulté relu dès qu'il est enregistré (0.17) | lecture, réécriture et relecture vérifiées hors jeu (`onimp_difficultytest`). En jeu : **non testé** |
| Vie des PNJ doublée à l'arrivée d'un client (0.16) | vu en jeu, et dans les journaux : `difficulte : PNJ : vie x2.00, ... (1 client(s))`, `pnj : vie portee a 200 pour cent de l'origine` chez l'hôte et chez le client |
| Jeu qui continue derrière le menu F1 (0.16) | vu dans les journaux de la 0.16 : `menu : ouvert, le jeu continue derriere`, `menu : ferme apres 771 image(s), 287 tick(s) simule(s)` pour cinq secondes de menu, deux réglages changés pendant ce temps et reçus par le client ; `plus personne, le jeu se met en pause` au départ du client. À l'écran : **pas de retour** |
| Curseurs `NPC health` et `NPC damage` de l'onglet Coop (0.18) | **non testé en jeu** : ni leur aspect, ni leur déplacement. Calcul des valeurs et écriture du fichier vérifiés hors jeu (`onimp_difficultytest`). Contrôle et fonctions lus dans l'exécutable, tels que le dialogue Options les emploie |
| Mode de difficulté : difficulté du jeu d'origine réglée par l'hôte, survie (0.19) | **non testé en jeu**. Lu dans l'exécutable : la fonction détournée est celle que le moteur interroge pour la vie d'un personnage à sa création, l'attention et les parades des IA, la précision de leurs tirs ; ses premiers octets sont les mêmes dans les deux copies d'`Oni.exe`. Valeurs des modes vérifiées hors jeu (`onimp_rulestest`) |
| Joueur à terre, compte à rebours, réanimation à CTRL (0.19) | **non testé en jeu**. Lu dans l'exécutable : un personnage increvable garde un point de vie sur un coup mortel et le moteur le jette au sol ; la coupure des touches est celle des cinématiques. Compte à rebours arrêté et repris vérifié hors jeu (`onimp_rulestest`) ; message de réanimation redistribué entre clients comme les autres (`onimp_nettest`) |
| Jauges aux couleurs des joueurs, des ennemis et des boss (0.19) | **non testé en jeu** |
| Multiplicateur d'ennemis, renforts (0.19) | **non testé en jeu**. Lu dans l'exécutable : l'objet de niveau d'un personnage, tel que `ai2_spawn` le cherche et le donne à la création. Filtre essayé sur les données des quatorze niveaux : 562 ennemis sur 704 multipliables, aucun nom de renfort déjà pris. Minimum par nombre de joueurs et noms vérifiés hors jeu (`onimp_rulestest`) |
| Civils, mission échouée si un joueur en tue un (0.19) | **non testé en jeu**. Tirage et noms vérifiés hors jeu (`onimp_rulestest`) |
| Onglet Coop chez les clients, en lecture (0.19) | lecture seule vue dans le journal d'un client de la 0.19 : `OniMP : reglage reserve a l'hote` à chaque clic. Mise à jour menu ouvert : **pas de retour** |
| Écrans de texte du niveau ouverts depuis la boucle du jeu (0.20) | vu dans les journaux de la 0.20, hôte et client, vingt minutes : cinq écrans chez l'hôte, quatre chez le client (`ecran de texte : 'level_1d' demande, le script continue`, `ouvert`, puis `menu : ferme`), refermés par chacun à son heure, la partie continuant ensuite. À l'écran : **pas de retour** |
| Messages du mod en haut à droite, en vert, masquables (0.20) | vu en jeu : en haut à droite, mais le texte du scénario avait suivi. Refait en 0.21 : zone à part, **non testé en jeu** |
| Portes et consoles de même numéro, annonces bornées (0.21) | vu dans les journaux de la 0.21, cinquante minutes sur trois niveaux : une seule ligne par console (`console 7 (rang 1) activee ici : annoncee`), journaux de 28 et 47 Ko au lieu de 5 Mo. Le défaut avait été lu dans les journaux de la 0.20 : `console 7 activee ici : annoncee` à chaque image chez le client, `console 7 activee par le joueur 1` chez l'hôte |
| Messages du mod dans leur propre zone, console du jeu intacte (0.21) | zone créée, vu dans les journaux de la 0.21, hôte et client : `messages : zone creee en haut a droite (1280 x 800), 6 ligne(s)`. À l'écran : **pas de retour**. Lu dans l'exécutable : création, tracé et lignes d'une zone de texte, là où le moteur trace la sienne |
| PNJ détournés d'un joueur à terre vers un joueur debout (0.21) | **non testé en jeu**. Lu dans l'exécutable : un contact donné à une IA lui fait revoir sa cible, et c'est ce que fait `ai2_makeaware` |
| Rechargement dans l'état des personnages, joueurs et PNJ (0.21) | transmis, pas montré en 0.21 (`'mp_remote1' ne peut pas le montrer : pas d'animation pour cette arme`). Corrigé en 0.22 (variante d'arme du double), et joué sur l'autre machine, vu dans les journaux de la 0.22 : `rechargement : 'mp_remote1' recharge 'w2_sap' comme sur l'autre machine` chez l'hôte, la même ligne pour `'mp_remote0'` chez le client. À l'écran : **pas de retour**. PNJ, chez le client : `'Floor1_Striker_1_x3' ne peut pas le montrer : personnage inactif`, quatre fois |
| Aura transmise (0.21) | émise, vu dans les journaux de la 0.21 : `aura : allumee chez notre joueur, annoncee dans son etat`, puis `eteinte`. Chez l'autre joueur : **pas de retour**. Lu dans l'exécutable : la valeur que pose `chr_super` et celle que le moteur en tire à chaque tick |
| Identifiants des PNJ et des armes au sol (0.21) | **pas de retour**. Tracé des lettres vérifié hors jeu |
| Trois niveaux à la suite à deux (0.21) | vu dans les journaux de la 0.21 : `EnvWarehouse`, `manplant`, `lab`, connexion reprise à chaque niveau, aucune `EXCEPTION` |
| Arrivée d'un joueur : reprise commune, nouveau venu sans armes (0.22) | en partie, vu dans les journaux de la 0.22 : client arrivé une seconde après son chargement, au même point que l'hôte, donc sans reprise (`arrivee : joueur 1 ; nos deux niveaux viennent d'etre charges au point 2, rien a reprendre`), et inventaire vidé (`arrivee : inventaire de notre joueur vide`). Compte à rebours et reprise commune : **non testé en jeu** |
| Départ commun après un chargement, écrans d'accueil sautés (0.22) | écran sauté, vu dans les journaux de la 0.22, hôte et client : `niveau : ecran 'intro_splash_screen' saute, session en cours`. Attente après un chargement commun, départ donné par l'hôte : **non testé en jeu** (seule ligne vue : `attente : le joueur 1 a charge le niveau`) |
| Objet du niveau donné à tous quand un joueur le ramasse (0.22) | **non testé en jeu**. Lu dans l'exécutable : l'octet que lit `chr_has_lsi` |
| Renforts alignés sur leur original : équipe, passivité, connaissance des joueurs (0.22) | **non testé en jeu**. Lu dans les données : `guard1` naît dans l'équipe 0, un script le passe au Syndicat |
| Arme des PNJ dictée par l'hôte (0.22) | à l'œuvre, vu dans le journal du client de la 0.22 : `arme du PNJ 'Floor1_Striker_1_x3' : 'w2_sap' lachee ici seulement, reprise`, puis `tient 'w2_sap', l'hote dit ''`. À l'écran, et avec Barabus : **pas de retour** |
| Numéro des armes : échange de deux armes de la même classe sans cartouches perdues ni gagnées (0.23) | **non testé en jeu**. Défaut lu dans les journaux de la 0.22, dans les deux sens ; rapprochement vérifié hors jeu (`onimp_groundtest`) |
| Munitions des autres joueurs au-dessus de leur jauge (0.24) | vues en jeu en 0.24, à en juger par la demande qui a suivi (des carrés et non des ronds, une seule rangée). Refaites ainsi en 0.25 : **non testé en jeu** |
| Aura sur un octet, bouclier de force et invisibilité dans l'état des personnages (0.24) | aura et bouclier : vu dans les journaux de la 0.24. Hôte `aura : allumee chez notre joueur`, client `aura : allumee sur 'mp_remote0' d'apres sa machine`, puis `le moteur la montre ici a 0.31 (demandee 0.31)` ; `bouclier : 'mp_remote0' en porte un d'apres sa machine (100)`. À l'écran, et l'invisibilité : **pas de retour** |
| Civils qui donnent un objet au joueur d'un client (0.24) | vu dans les journaux de la 0.24, pour un hypo : hôte `civil : 'Floor2_Sci_5' engage la conversation avec le joueur 1, a sa touche d'action`, `1 hypo(s) donne(s) au pantin du joueur 1 : envoye(s) a son joueur` ; client `1 hypo(s) recu(s) d'un PNJ de l'hote, 1 pris`. Armes, boucliers, objet du niveau : **pas de retour** |
| Points de sauvegarde laissés à l'hôte (0.24) | vu dans les journaux de la 0.24 : hôte `sauvegarde : point 3 atteint, dit aux clients`, client `sauvegarde : l'hote a atteint le point 3`, et la partie continue huit minutes |
| Fermeture du jeu sans plantage quand une zone de texte du moteur est sans lignes (0.24) | ligne de fermeture vue dans le journal de l'hôte de la 0.24 (`fermeture : le jeu se ferme ; zones de texte : commande 0/1, console 0/3, messages 0/13, sous-titres 0/13,`), zones saines. Cas d'une zone sans lignes : **pas rencontré** |
| Zones de texte du moteur contrôlées et refaites à chaque image (0.25) | vu dans le journal du client de la 0.25 : zone des messages trouvée abîmée et refaite quatre fois, pas de plantage. Ce qui la vidait : l'historique des commandes du moteur, corrigé en 0.26 |
| Titre des fenêtres par les scripts Radmin (0.25) | vu dans les journaux de la 0.25 : `fenetre : titre 'Host - Oni' (c'etait 'ONI ')`, et `'Client - Oni'` |
| Pseudo saisi dans l'onglet Display, gardé dans `onimp.ini`, affiché au-dessus des joueurs (0.25) | saisie et enregistrement vus dans les journaux de la 0.25 (`joueurs : pseudo 'Odolinsky' enregistre dans onimp.ini`) ; pas de retour sur l'affichage. Champ de saisie du moteur et ses messages lus dans l'exécutable ; son gabarit lu dans les données (`dialog_setup_player`) |
| Jauges, munitions et pseudos des joueurs lisibles à travers les murs (0.25) | **non testé en jeu** |
| Tenues de Konoko attribuées par l'hôte, une par joueur, changées par le bouton `Skin` (0.25) | changement vu dans les journaux de la 0.25 (`tenue : 'konoko' passe de 'konoko_generic' a 'k3' (joueur 1)`) ; pas de retour sur ce qui se voit. Classes lues dans les données communes (`k3`, `k4_A`, `k4_F`, `k4_G`, `k4_K`, `k4_L`) ; fonction du moteur lue dans l'exécutable |
| Latence au-dessus des joueurs, bouton `Ping` (0.25) | mesure vérifiée hors jeu avec le reste du réseau (`onimp_nettest`, à deux et à quatre). Affichage : **non testé en jeu** |
| Écran de texte d'une console ouvert chez le seul joueur qui l'utilise (0.25) | console d'un client : vu en jeu et dans les journaux de la 0.25. Console de l'hôte : l'écran partait encore chez tous, corrigé en 0.26. Constaté dans les journaux de la 0.24 : `ecran de texte : 'level_3c' ouvert` chez l'hôte et chez le client pour une console utilisée par l'hôte |
| Munitions illimitées décidées par l'hôte (0.25) | vu dans les journaux de la 0.25 : `reglage : munitions illimitees activees` puis `desactivees` chez l'hôte et chez le client. Effet sur les armes : pas de retour |
| Plantage du client : débordement de l'historique des commandes du moteur corrigé (0.26) | **non testé en jeu**. Cause lue dans les journaux de la 0.25 (zone des messages abîmée toutes les cinq secondes) et dans l'exécutable (`0x4317D0`) |
| Écran de texte d'une console de l'hôte chez lui seul, par détour de l'activation des consoles (0.26) | **non testé en jeu**. Détour vérifié hors jeu (`onimp_hooktest`) ; fonction et appelant lus dans l'exécutable |
| Une seule arme par PNJ et par endroit : arme d'un PNJ reprise où qu'elle soit, sol du client aligné sur celui de l'hôte (0.26) | **non testé en jeu**. Aucun journal d'un combat contre Barabas : cause déduite du code |
| Langue du mod, français ou anglais, changée dans l'onglet Display (0.26) | dictionnaire et formats vérifiés hors jeu (`onimp_langtest`). Menu et messages : **non testé en jeu** |
| Panneau d'un joueur à terre, repère à travers les murs, invite de réanimation (0.26) | **non testé en jeu**, jamais vu à l'écran |
| Battement de cœur à terre ou en santé critique (0.26) | vu dans le journal de l'hôte de la 0.26 : `lance en boucle (sante critique)`, puis `arrete (joueur mort)`. Remplacé en 0.27 par l'alerte jouée une fois |
| Langue gardée d'un lancement à l'autre (0.26) | vu dans les journaux de la 0.26 : `langue des textes a l'ecran : English` au lancement, messages en anglais |
| Débordement de l'historique des commandes corrigé (0.26) | correctif posé, vu dans les journaux de la 0.26 (`historique des commandes : debordement du moteur corrige`). Partie de plus de vingt minutes côté client : **pas encore jouée** |
| Alerte sonore jouée une fois quand un joueur tombe, chez tous les joueurs ; volume réglé par un curseur de l'onglet Coop (0.27) | lecture vérifiée hors jeu, volume à zéro : jouée une fois, arrêtée d'elle-même à la fin, rejouable, relancée en cours de lecture. En jeu : **non testé** |
| Bouton `Wallhack (ESP)` : vue à travers les murs coupée ou remise par chaque joueur (0.27) | **non testé en jeu** |
| Tous les réglages du menu gardés dans `onimp.ini`, chez l'hôte et chez un client du même fichier (0.27) | vérifié hors jeu (`onimp_configtest`). En jeu : **non testé** |
| Barabas en double à chaque reprise : journal des scripts de l'hôte vidé à tout rechargement (0.28) | cause lue dans les journaux de la 0.27 (commandes rejouées : 16, 32, 48... 176 ; `pnj orphelin : suppression de 'Barabus'` 58 fois). Correctif : **non testé en jeu** |
| PNJ qui flottent et tremblent de loin : gardés actifs chez l'hôte près de chaque joueur, et chez le client (0.28) | **non testé en jeu**. Cause déduite du code du mod et du moteur (`chr_lock_active` lu dans l'exécutable) ; aucune ligne de journal ne la montrait |
| Joueur à terre : corps fixé au sol, pantin sans collision chez les autres, portée de réanimation de 26 unités (0.28) | **non testé en jeu** |
| Cinématiques : pantins des autres joueurs cachés et sans collision (0.28) | **non testé en jeu**. Test de visibilité du moteur et drapeau de cinématique lus dans l'exécutable |
| Portraits des dialogues en bas de l'écran (0.28) | **non testé en jeu**. Table des vingt-six places lue dans l'exécutable |
| Douze chargeurs et douze cellules, hypos à six ; rangée de douze cases (0.28) | **non testé en jeu**. Six opérandes et deux cases de table de sauts modifiés dans le moteur, tout ou rien ; code ajouté relu au désassembleur |
| Jeu automatique d'un client (0.28) | vu dans les journaux de la 0.28 : il se bat, projette des ennemis, ranime. Signalé en jeu : il fonce dans les murs et peine à toucher un ennemi au sol. Refait en 0.29 |
| Jeu figé des clients : positions qui ne sont pas des nombres écartées partout, joueur et personnages ramenés à leur dernière place saine (0.29) | **non testé en jeu**. Cause lue dans la mémoire des deux clients figés de la 0.28 ; ce qui a produit le premier nombre invalide n'est pas établi |
| Arme prise à un PNJ par une prise : annoncée, retirée au PNJ chez l'hôte (0.29) | annonce et retrait chez un autre client vus dans le journal d'un client de la 0.29. Chez l'hôte : journal réécrit, pas relu |
| Coup reçu au sol par un joueur à terre, montré sans le relever ; réanimation qui ne saute plus d'un joueur à terre à l'autre (0.29) | vu dans le journal d'un client de la 0.29 pour le coup au sol (`reaction 'KONCOMhit_fallen1' jouee ; compte a rebours inchange`). À l'écran : pas de retour |
| Jeu automatique refait : réanimation d'abord, trace de l'hôte, balayage d'un ennemi au sol (0.29) | vu dans le journal d'un client de la 0.29 : quinze réanimations menées au bout, trace suivie, ennemis au sol balayés, deux replacements en vingt-sept minutes. Ce qui se voit à l'écran : pas de retour |
| PNJ retiré par un script de l'hôte plus recréé chez le client ; relevé vide de l'hôte sans PNJ (0.30) | **non testé en jeu**. Défaut lu dans le journal du client de la 0.29 |
| Cinématiques : tenue du joueur plus donnée au personnage que suit le script ; joueur d'un client plus figé après la cinématique de l'usine ; films de Konoko au moment de l'hôte (0.31) | **non testé en jeu**. Causes lues dans les journaux de la 0.30, dans le script de l'usine et dans l'exécutable (`chr_focus`, `ai2_takecontrol`) ; scripts des quatorze niveaux passés en revue |
| Entrepôt qui ne démarrait pas après sa cinématique : joueurs comptés pour un dans les zones (0.31) | **non testé en jeu**. Cause lue dans le script (`trigvolume_count (63) eq 1`) et dans l'exécutable |
| Jeu automatique : arme la plus forte, arme plus faible en cas de pénurie, jamais celle de Barabas (permise depuis la 0.34), munitions des deux sortes, hypo vérifié (0.31) | vu dans le journal du client de la 0.31 : cinq armes ramassées, un hypo refusé puis plus de demande en rafale ; les munitions, elles, n'étaient pas ramassées (refait en 0.32) |
| Cinématique de l'usine et entrepôt (0.31) | vu dans les journaux de la 0.31 : `cinematique : le script suit 'Recep' (chr_focus) ; notre joueur reste 'konoko'`, plus de tenue donnée à la réceptionniste, le client bouge et se bat après la cinématique sans que le filet serve ; `zones : zone 63, 2 personnage(s) ... comptes pour un, soit 1`, puis l'ennemi `A_t48`. À l'écran : pas de retour |
| Jeu automatique : ligne de vue avant de tirer et d'aller chercher, butin tenu jusqu'au bout, sprint, ennemi au sol frappé sans courir sur place (0.32) | vu dans les journaux de deux essais de la 0.32 (laboratoire, entrepôt) : plus d'alternance entre un objet et l'hôte, neuf armes ramassées, et dans l'entrepôt 92 lignes `frappe un ennemi au sol`. Les bonus, eux, n'étaient toujours pas pris (refait en 0.33). Ligne de vue et sprint : pas de retour sur ce qui se voit à l'écran |
| Jeu automatique : bonus ramassés par la routine du moteur ; chargeurs, cellules et hypos jusqu'à l'inventaire plein (0.33) | vu dans les journaux d'un essai de la 0.33 (laboratoire, dix-neuf minutes) : trois hypos et deux chargeurs ramassés par lui, les cinq confirmés chez l'hôte (`bonus de type N ramasse par le joueur 1`), deux hypos pris aussitôt ramassés. À l'écran : pas de retour |
| Pantin d'un autre joueur qui ramassait le bonus voisin de celui que ce joueur prenait (0.33) | vu dans les journaux du même essai, sur peu de cas : après les deux ramassages de l'hôte, plus aucun bonus annoncé par le client dans la seconde qui suit |
| Journal du lancement précédent gardé dans `onimp_journaux` (0.33) | vu dans le dossier du jeu : les deux journaux de l'essai précédent y sont, datés, avec le `debugger.txt`, et la deuxième ligne des nouveaux journaux le dit |
| Jeu automatique : butin avant le combat ; arme sortie devant un ennemi, rangée sinon, rangée aussitôt quand elle est vide et sans recharge ; recul devant un ennemi qui approche ; rechargement de l'arme vide seulement ; arme de Barabas permise (0.34) | **non testé en jeu**. Routines de rangement et vitesses lues dans l'exécutable et dans les animations ; jamais vues à l'œuvre |
| Barre de réanimation visible aussi du joueur à terre et des autres joueurs (0.34) | **non testé en jeu** |
| Jeu automatique au corps à corps seulement : ni tir, ni arme sortie ou ramassée, ni munitions cherchées (0.35) | vu dans les journaux de la 0.35 (entrepôt puis usine, quarante-sept minutes) : `range 'w1_tap' (il se bat a mains nues)` deux fois, puis 90 lignes `frappe un ennemi` ou `frappe un ennemi au sol`, sept hypos ramassés, aucune ligne de tir, d'arme sortie ni de munitions. Cinq replacements à côté de l'hôte (`bloque en chemin`) et huit points de trace sautés : il se bloque encore parfois en suivant l'hôte. À l'écran : pas de retour |
| Onglets Cheat, Character et Spawn, dans la colonne de droite du menu F1 ; montrés à l'hôte et aux clients autorisés seulement, menu ouvert compris (0.35) | vu dans les journaux de la 0.35 : `menu : onglets Coop (montre), Display (montre), Cheat (montre), Character (montre), Spawn (montre)` chez l'hôte, les trois derniers `(cache, acces non donne)` chez le client ; le moteur a accepté tous les boutons. Leur place à l'écran : pas de retour |
| Interrupteur général de l'hôte et accès par client, dans les réglages transmis (0.35) | vu dans les journaux de la 0.35 : `cheats : joueur 1 autorise` chez l'hôte, `l'hote nous ouvre les onglets Cheat, Character et Spawn` chez le client deux millisecondes après, puis retiré de même ; `interrupteur general ouvert` reçu aussi. Réglages gardés : testé hors jeu (`onimp_configtest`) |
| Cheats de chaque joueur : invincibilité, munitions illimitées, coup fatal, aura permanente (0.35) | aura permanente : réglée chez l'hôte et chez le client, et montrée sur leurs pantins (`aura : 'mp_remote1' : le moteur la montre ici a 1.00`, journaux de la 0.35). Les trois autres : **non testés en jeu** ; invincibilité et munitions reprennent le mécanisme de la 0.10 et de la 0.25 |
| Vitesse d'attaque, vitesse de déplacement, super saut sans dégâts de chute (0.35) | **non testé en jeu**. Les sites du moteur employés ont été lus dans l'exécutable et leurs octets vérifiés dans `AE\Oni.exe` |
| Personnage incarné, vu des autres joueurs, touche F5 (0.35) | vu dans les journaux de la 0.35 : une soixantaine de classes incarnées à la suite par l'hôte, chacune redite au client dans les sept millisecondes (`personnage : joueur 0 incarne bd_1, d'apres l'hote`), aucun refus du moteur, et `super_ninja_1` repris au niveau suivant et après un rechargement. Santé, coups, caméra : pas de retour |
| Apparition d'objets, d'armes et de PNJ alliés ou ennemis, chez l'hôte et à la demande d'un client (0.35) | **non testé en jeu** |
| Lasers de l'hôte qui détectent tous les joueurs, lasers d'un client qui n'en détectent plus (0.36 à 0.38, remplacé en 0.39 : chaque machine détecte son joueur) ; lasers et tourelles alignés sur l'hôte ; ennemis prévenus (0.36) | vu en jeu et dans les journaux de la 0.36 (usine, salle du Deadly Brain) : `laser 202 coupe par mp_remote1 (joueur 1)` quinze fois chez l'hôte, où les tourelles s'en prennent alors au client ; `tourelle 20 allume d'apres l'hote` chez le client onze millisecondes après l'hôte. Défaut signalé : chez le client rien ne lui tirait dessus, et il ne prenait aucun dégât (voir la ligne suivante). Ennemis prévenus : aucun n'était à portée |
| Cible des tourelles choisie par l'hôte parmi les joueurs debout, la même sur toutes les machines : le joueur visé est touché chez lui (0.37) | cible : vue dans les journaux de la 0.37, `tourelle 20 (rang 4) vise le joueur 1` chez l'hôte et `... vise le joueur 1, d'apres l'hote : c'est le notre` chez le client treize millisecondes après, 44 fois. Dégâts : signalé en jeu, toujours rien chez le client ; la cible n'était pas la seule cause (ligne suivante) |
| Tourelles d'un client qui tirent : drapeau de tir détourné, celui de l'hôte au lieu d'`ai2_allpassive` (0.38) | **vu en jeu** (0.38, salle du Deadly Brain de l'usine : « fonctionne pour le client et l'hôte ») et dans les journaux : chez le client, `tourelle 20 (rang N) tire ici, sur notre joueur` dix fois, `notre joueur perd de la vie sous le tir d'une tourelle` vingt-deux fois ; `chez l'hote les IA sont tenues passives par un script` le temps d'une cinématique, puis `libres` |
| Laser coupé chez l'hôte rejoué chez les clients, son et événements ; alarme d'un laser ou d'une console comptée (0.38) | rejeu : vu dans les journaux de la 0.38, `laser 202 (rang 0) coupe chez l'hote par le joueur 1 : son joue, ses evenements rejoues ici`, 501 fois en six minutes (trop : voir la ligne suivante). Alarme d'un laser : aucun laser du jeu n'en porte, d'après le relevé des niveaux |
| Laser détecté aussi par la machine du joueur qui le coupe, sur les rayons qu'elle affiche, et annoncé aux autres, la détection par l'hôte restant à côté ; rayons des lasers mobiles recalés sur ceux de l'hôte (0.39) | **non testé en jeu**. Défaut signalé en jeu en 0.38 : « dans d'autres salles... quand le client passe sur les lasers rien ne marche ». Cause **déduite**, pas observée : de l'exécutable (un rayon avance tant que son laser est allumé, sur chaque machine pour son compte) et des données des niveaux (288 lasers mobiles sur 291, 165 allumés au chargement) ; les deux jeux étaient fermés quand il aurait fallu comparer leurs rayons en mémoire. Sites du moteur vérifiés dans `AE\Oni.exe` ; écart entre deux rayons : `onimp_rulestest` |
| Tourelles : cible = ceux qui ont coupé le laser, puis le plus proche de ceux qu'elle peut toucher ; lasers à plusieurs rayons annoncés quatre fois par seconde au plus (0.39) | **vu en jeu dans le premier jet de la 0.39** (partie de 14:54 à 15:09, journaux `AE\onimp_host_0.39_premier_jet.log` et `onimp_client_0.39_premier_jet.log`) : `laser 2 (rang 4) coupe par mp_remote1 (joueur 1), 1 fois depuis la ligne precedente`, `tourelle 1 (rang 0) vise le joueur 1`, `tire ici, sur mp_remote1` chez l'hôte ; chez le client `tire ici, sur notre joueur` sept fois et `notre joueur perd de la vie` dix-huit fois, tourelles 1, 2 et 3. Défaut lu dans les journaux de la 0.38 : tourelle 1 quinze secondes sur l'hôte sans un tir. La règle livrée, réécrite depuis (plusieurs joueurs dans les mêmes rayons), est testée hors jeu (`onimp_rulestest`), **pas rejouée** |
| Vague d'alarme : trois ennemis du niveau par joueur, sur `ai2_tripalarm` et sur une console dont un ennemi se sert (0.36) | taille et noms des vagues : testé hors jeu (`onimp_rulestest`). Le reste : **non testé en jeu** |
| Client qui charge de lui-même le niveau de l'hôte quand il est dans un autre (0.36) | **non testé en jeu** |
| Hôte planté à l'aéroport après un déclencheur (signalé en 0.32) | **cause inconnue**. Les journaux de cette partie ont été écrasés par le lancement suivant, Windows n'a rien noté pour `Oni.exe`, et les journaux de la 0.31 dans ce niveau ne montrent aucune faute. La 0.33 garde les journaux : à reproduire |
| Gâchette plus annoncée pendant une cinématique ; pantin d'un client qui ne tire plus sans fin (0.32) | **non testé en jeu**. Cause déduite du code et des journaux, pas vue directement |
| Jeu automatique : hypo pour le soin et pour l'aura, arme et munitions ramassées, tir à distance, corps à corps au contact (0.30) | **non testé en jeu**. Touches de rechargement et de lâcher lues dans les symboles de Daodan ; drapeaux des classes d'armes lus dans les données. La visée verticale écrite dans le personnage du joueur n'a jamais été vue à l'œuvre |
| Boutons de réglage de 32 points de haut au lieu de 40, pour tenir à dix dans l'onglet Display (0.27) | **non testé en jeu**, jamais vu à l'écran. Le moteur a des boutons de 20, 25 et 40 points dans ses propres dialogues |
| Compilation MSVC | **non testé** |

Le code des lignes « non testé » repose sur une analyse statique d'Oni.exe. Attendez-vous à
des ajustements.

## Dépannage

- **Le jeu ne démarre plus.** Lancez `uninstall.bat` : le jeu revient à son état d'origine.
- **Les mods cochés dans l'AE Installer n'apparaissent pas en jeu.** Le jeu lancé n'est pas celui
  que cet AE Installer tient à jour, ou le mod a été téléchargé sans être installé. La ligne
  `mods AE :` du journal dit ce qui est installé dans le jeu lancé ;
  `onimp_modtest.exe <dossier>` le dit sans lancer le jeu. Ouvrez l'AE Installer par
  `ae_installer.bat`, ou désignez le bon dossier par `ONIMP_GAME`.
- **`OniMP n'est pas installe dans ...` au lancement.** L'AE Installer a réécrit `realbink.dll`,
  ou OniMP n'a jamais été installé dans ce dossier : lancez `install.bat`.
- **`OniMP : mods AE differents de ceux du joueur N` dans la console.** Les deux lignes
  suivantes donnent les paquets que l'un a et pas l'autre ; la liste complète est dans le journal.
  Cochez les mêmes mods des deux côtés et réinstallez.
- **Pas de fichier `onimp*.log`.** La DLL n'est pas chargée : `realbink.dll` a été réécrit, ou
  Daodan est absent. Ou le journal est dans une autre copie du jeu : il s'écrit à côté de
  l'`Oni.exe` lancé.
- **« Oni.exe non reconnu » dans le journal.** L'exécutable n'est pas celui attendu ; aucun
  patch n'est posé.
- **Une ligne « code inattendu » ou « dialogue de pause inattendu » dans le journal.** Une
  modification d'Oni.exe a été refusée : la ligne dit quelle fonction manque. Le reste marche.
- **Un autre joueur n'apparaît pas.** Vérifiez dans le journal de l'hôte et dans le sien la
  ligne `pair connecte`, puis que tous sont dans le même niveau (`mp_status`). Entre deux clients,
  c'est l'hôte qui fait passer : s'ils voient tous deux l'hôte mais pas l'un l'autre, joignez les
  trois journaux.
- **Le client ne trouve pas l'hôte (recherche automatique).** Voyez le tableau de
  « Par Radmin VPN, sans adresse à saisir ». Dans le journal de l'hôte, la ligne
  `partie annoncee sur ...` dit par quelles adresses il s'annonce ; dans celui du client,
  `recherche d'un hote` puis `partie '...' trouvee`. L'hôte n'annonce rien tant qu'il n'est pas
  en jeu, ni pendant qu'il est dans le menu F1 ou qu'il charge un niveau.
- **`OniMP : desynchronisation (...)` dans la console de l'hôte.** Il vient de renvoyer au
  client la partie de l'état nommée ; rien à faire. `desynchronisation persistante` : l'écart a
  résisté à trois renvois. Le journal de l'hôte donne les deux empreintes (`desync : ...`) ;
  joignez-le à tout rapport. `reglages` désigne aussi des options `npcs` ou `quests` différentes
  entre les deux joueurs : lancez la même paire de scripts.
- **Pas d'onglet Coop ou Display dans le menu F1.** Aucun des deux n'apparaît hors session (mode
  `off`). Sinon, voyez le journal (point précédent) ; les commandes `mp_*` font la même chose.
  Chez un client l'onglet Coop se lit sans se régler, et n'a pas les curseurs.
- **Un joueur meurt au lieu de rester à terre.** Il n'y avait personne pour le ranimer : il faut
  un coéquipier debout dans le même niveau. `mp_status` le dit (`un coequipier peut ranimer` ou
  `personne pour ranimer`).
- **CTRL ne ranime pas.** Il faut être à un corps de distance du joueur à terre, garder la touche
  trois secondes sans la lâcher, et que la fenêtre du jeu ait le clavier. La console dit
  `reanimation du joueur N : gardez CTRL 3 s` au début, puis `joueur N ranime` ou `reanimation
  ... interrompue`.
- **Pas de renforts malgré `Enemies : x2`.** Ils n'arrivent qu'une seconde après l'ennemi, jamais
  pendant une cinématique, et pas pour les ennemis que le niveau tient à part (voir
  « Multiplicateur d'ennemis et civils »). Le journal de la machine qui simule les PNJ les note
  (`renfort : 'Floor1_Striker_2_x2' aux cotes de 'Floor1_Striker_2'`), et `mp_status` les compte.
- **Pas de jauge au-dessus des personnages.** Vérifiez l'onglet Display, puis cherchez dans le
  journal une ligne `jauge : textures refusees` ou `sante : code inattendu`.
- **Plus aucun message `OniMP : ...` à l'écran.** `Messages` est sur `OFF` dans l'onglet Display
  du menu F1 (`messages = false` sous `[display]` dans `onimp.ini`). `mp_messages 1` les remet.
- **Les messages du mod sont en bas de l'écran, en blanc.** Leur zone n'a pas pu être posée : le
  journal a une ligne `messages : code inattendu`. Ils passent alors par la console du jeu.
- **Pas d'identifiants au-dessus des PNJ ou des armes.** Voyez `NPC IDs` et `Weapon IDs` dans
  l'onglet Display, puis cherchez dans le journal `jauge : textures des etiquettes refusees`.
- **On ne voit pas un autre joueur recharger.** Les deux journaux le disent : `rechargement :
  notre joueur recharge '...'` chez celui qui recharge, et chez l'autre `rechargement : 'mp_remoteN'
  recharge '...' comme sur l'autre machine` ou `... ne peut pas le montrer : ...` avec la raison.
  Joignez ces lignes.
- **Des PNJ restent plantés autour d'un joueur à terre.** Chez la machine qui simule les PNJ
  (l'hôte), le journal doit dire `cible : N PNJ autour d'un joueur a terre diriges vers un joueur
  debout`. Il faut un joueur debout dans les parages : à 900 unités du jeu au plus, une grande salle.
- **Le jeu reste figé après un écran de texte du niveau** (cadre bleu, plein écran). C'était un
  défaut de la 0.19, corrigé en 0.20 : vérifiez que la première ligne du journal dit `OniMP 0.20`,
  chez tous les joueurs, et qu'il n'a pas de ligne `ecrans de texte : code inattendu`. Si cela se
  reproduit, joignez les journaux : les lignes `ecran de texte :` disent quel écran, et quand.
- **La vie ou les dégâts des PNJ ne changent pas quand je change les valeurs.** Elles se règlent
  par les curseurs de l'onglet Coop du menu F1 de l'hôte, ou
  dans `onimp_difficulty.ini` du dossier du jeu (`AE` ici), chez l'hôte, pas dans
  `src/difficulty.cpp` ni dans la copie du dossier `OniMP`. Après l'enregistrement, la console de
  l'hôte doit dire `OniMP : difficulte relue : ...` dans la seconde ; sinon ce n'est pas le bon
  fichier, ou les valeurs n'ont pas changé. Pour les dégâts, `Online damage` doit être sur `ON`
  (menu F1 de l'hôte, onglet Coop) : sur `OFF`, la console dit `degats normaux, Online damage sur
  OFF`. La vie ne vaut qu'avec un client connecté et des PNJ partagés.
- **Le jeu reste figé après un chargement, avec `en attente de N joueur(s)` à l'écran.** C'est
  l'attente commune : elle tombe d'elle-même quand tous ont chargé, au bout de dix secondes pour un
  joueur parti, de vingt-cinq au plus. `mp_status` le dit (`en attente des autres joueurs`). Pour
  s'en passer : `ready_check = false` dans `onimp.ini`.
- **La partie recharge quand un joueur arrive.** C'est voulu depuis la 0.22 (voir « Arrivée d'un
  joueur »). Pour garder l'ancien comportement : `join_reload = false` chez l'hôte.
- **`refuse par l'hote : version differente`.** La 0.23 a changé de protocole : tous les joueurs
  doivent l'avoir.
- **Des joueurs se sont perdus de vue** (`pair perdu` dans un journal). Une machine est restée
  plus de cinq secondes sans rien envoyer : chargement de niveau, fenêtre figée. La connexion se
  rétablit seule. Si c'est l'hôte, tous les clients le perdent en même temps.
- **Le client plante en pleine partie.** Signalé en 0.23 (à un point de sauvegarde) et en 0.24,
  après une vingtaine de minutes ; en 0.25 la garde des zones de texte a rattrapé la faute, sans
  plantage. Cause trouvée en 0.26 : l'historique des
  commandes du moteur déborde après 256 commandes et écrase le pointeur de la zone des messages
  (voir « Niveau et partie »). Corrigé en 0.26 ; la ligne `historique des commandes : debordement
  du moteur corrige` doit figurer au début du journal. Si cela se reproduit, les journaux de la
  partie se retrouvent, depuis la 0.33, dans le sous-dossier `onimp_journaux` après le lancement
  suivant (voir « Sur une seule machine »). Dans celui du client, chercher `zone de texte`,
  `EXCEPTION` et `fermeture :`.
- **Le jeu plante ou se fige** (hôte ou client). Notez l'heure, et ce qui venait de se passer à
  l'écran. Le journal de la partie est gardé : `onimp_journaux\onimp_host_<date>_<heure>.log`, et à
  côté le `debugger.txt` du moteur. Une faute du programme y laisse une ligne `EXCEPTION` suivie de
  `pile :`. Un jeu figé n'y laisse rien : ne le fermez pas tout de suite si vous pouvez, sa pile se
  lit dans le processus encore ouvert.
- **Pas de son quand un joueur tombe.** Journal : `son : alerte introuvable` (copier
  `OniMP\sounds\AlertFall.mp3` à côté d'Oni.exe sous le nom `onimp_alert.mp3`, ou relancer
  `install.bat`) ; `son : commande ... refusee par Windows` (le fichier n'est pas un MP3 lisible) ;
  `son : alerte muette, volume a zero` (curseur `Alert volume` de l'onglet Coop) ; `son : alerte
  laissee a la fenetre du jeu qui a le clavier` (cette fenêtre était en arrière-plan). `mp_status` :
  ligne `alerte :`.
- **Un réglage changé dans `onimp.ini` n'a aucun effet.** Le jeu lit le fichier de son propre
  dossier, `AE\onimp.ini`, pas `OniMP\onimp.ini`, qui n'est que le modèle copié à la première
  installation. Un client lit d'abord `[display_client]` et `[audio_client]`. Le plus sûr : régler
  dans le menu F1, qui écrit au bon endroit.
- **Le menu F1 est en anglais, ou en français, et on veut l'autre.** Onglet Display (`Affichage`),
  bouton `Language` (`Langue`), ou `mp_language "fr"` / `"en"`.
- **Le champ `Name` n'apparaît pas dans l'onglet Display.** Ligne `menu : champ de saisie du
  pseudo indisponible` ou `refuse par le moteur` dans le journal. La commande `mp_name "pseudo"`
  fait la même chose, et `name_host` / `name_client` sous `[player]` dans `onimp.ini` aussi.
- **Un civil ne donne rien au joueur d'un client.** Il faut l'option `npcs` (scripts `_npcs`,
  `_coop` ou `_radmin`), être à un corps et demi de lui et le regarder. Journal de l'hôte : une
  ligne `civil : '...' engage la conversation avec le joueur N` doit suivre la touche. Sans elle,
  le civil n'a rien à dire, a déjà donné, ou un ennemi est trop près. `mp_status` compte les
  demandes et les dons des deux côtés.
- **Plantage.** Le journal contient une ligne `EXCEPTION` avec l'adresse et le module fautifs,
  suivie depuis la 0.21 des registres et d'une ligne `pile :`, les adresses de retour trouvées sur
  la pile : c'est elle qui dit d'où la fonction fautive était appelée. Joignez les deux journaux à
  tout rapport.

## Limites connues

- Jeu automatique : il sait si un mur le sépare de ce qu'il regarde, mais ne cherche pas de chemin
  pour le contourner, et ne voit le vide que derrière lui, quand il recule. Il suit la trace de
  l'hôte, donc ne sait aller que là où l'hôte est passé ; vers un ennemi, un objet ou un joueur à
  terre autre que l'hôte il va en ligne droite, et renonce quand il n'avance plus. Il ne pare pas.
  Depuis la 0.35 il ne se bat qu'au corps à corps : il ne tire sur rien, même de loin, même avec
  une arme chargée dans le dos, et un ennemi qui le canarde depuis une hauteur qu'il ne peut pas
  atteindre le blesse sans réponse.
- Cheats (0.35) : chaque machine applique les siens et l'hôte ne les contrôle pas. Un client qui
  aurait modifié son mod pourrait ignorer l'interrupteur général ou l'absence d'accès ; seules les
  demandes de personnage et d'apparition sont refusées par l'hôte lui-même. Les accès vont par
  numéro de joueur, pas par personne.
- Une prise qui désarme un PNJ est annoncée par la machine du joueur ; l'inverse (un PNJ qui
  désarme un joueur) ne l'est pas.
- Quatre joueurs au plus : un hôte et trois clients.
- Tout passe par l'hôte. S'il quitte la partie, elle s'arrête pour tous ; ce qu'un client fait
  met un aller-retour de plus à parvenir à un autre client qu'à l'hôte.
- Difficulté : un seul fichier, celui de l'hôte, pour toute la partie ; les mêmes pourcentages
  pour tous les PNJ, boss compris. Sans l'option `npcs`, seuls le mode de difficulté et le
  multiplicateur d'ennemis valent, sur chaque machine pour ses propres PNJ.
- Mode de difficulté changé en cours de niveau : la vie des joueurs et la classe des ennemis déjà
  apparus ne suivent qu'au prochain chargement. Un client qui n'a jamais rejoint cet hôte charge
  son premier niveau avec le mode de sa partie précédente.
- Joueur à terre : il faut un coéquipier debout dans le même niveau ; sinon le joueur meurt comme
  avant. La barre de vie de l'interface du jeu, en bas de l'écran, garde ses couleurs d'origine :
  seules les jauges au-dessus des personnages portent celles des joueurs.
- Réanimation : jugée chez celui qui ranime, d'après la position qu'il voit du joueur à terre.
  La touche est CTRL, quelle que soit la configuration des touches du jeu, où elle sert aussi à
  autre chose.
- Arrivée d'un joueur : la reprise commune ne se fait que si le nouveau venu est dans le même
  niveau que l'hôte. Elle ramène tout le monde au dernier point de sauvegarde de l'hôte, donc en
  arrière si l'hôte avait avancé depuis.
- Départ commun : pendant l'attente rien ne répond, ni F1 ni Échap ; elle dure vingt-cinq secondes
  au plus. Les écrans d'accueil et de fin de niveau ne sont plus montrés en session.
- Objet du niveau : donné à tous dès qu'un joueur le ramasse. Les scripts ne le reprennent qu'au
  joueur de l'hôte.
- Arme des PNJ : celle que l'hôte annonce est donnée telle quelle à un PNJ qui ne l'a pas, sans
  le geste de la ramasser ; ses cartouches sont celles du client.
- Renforts : ils apparaissent à côté de leur original, là où il se trouve, deux secondes après
  lui, à vue s'il est à vue. Les ennemis auxquels un script du niveau est attaché ne sont pas
  multipliés, les boss non plus. Un renfort suit le rôle de son original (garde, patrouille),
  pas les ordres que les scripts donnent ensuite à celui-ci par son nom.
- Civils : créés sans rôle ni trajet, ils restent là où ils apparaissent tant que rien ne les
  inquiète. Seuls ceux du mod font échouer la mission.
- En session le jeu ne s'arrête plus derrière un menu : celui qui en ouvre un reste dans le
  monde, immobile et sans défense. Seul, ou tant que personne n'est relié, la pause est celle du
  jeu d'origine.
- Menu Échap ouvert en session : à sa fermeture le jeu peut rattraper six ticks d'un coup, un
  dixième de seconde.
- Écrans de texte du niveau, en session : le script qui en demande un n'attend plus sa fermeture.
  Ce qui suit dans le script (cinématique, ennemis, commandes rendues) se déroule derrière
  l'écran, pendant que le joueur lit, et le joueur qui lit reste dans le monde, sans défense.
- Messages à l'écran : sur `OFF`, les alertes du mod (joueur à terre, mission échouée) ne
  s'affichent plus non plus.
- Joueur à terre : les PNJ ne le frappent pas au sol, rien ne le blessant ; ils se tournent vers
  le joueur debout le plus proche d'eux, s'il y en a un dans les parages (900 unités du jeu).
- Identifiants : ce sont des noms, pas des numéros. Les armes en main n'en portent pas, seules
  celles qui sont au sol.
- Aura : seule la lueur est transmise. Elle suit celle de l'autre machine avec le retard
  d'affichage des personnages.
- Bouclier de force d'un autre joueur : sa lueur et ce qu'il arrête sont ceux de sa machine. Un
  tir arrêté ici par le bouclier de son pantin ne l'use pas chez lui : c'est sa machine qui compte
  ce qu'il encaisse.
- Munitions au-dessus des joueurs : les réserves seulement, six de chaque au plus, pas les
  cartouches de l'arme en main ; pas au-dessus de son propre personnage.
- Jauges à travers les murs : celles des autres joueurs seulement. Elles passent devant tout ce
  qui est à plus de dix unités de la caméra ; un mur contre lequel la caméra est collée peut
  encore les cacher, et elles passent aussi devant le personnage du joueur quand elles le
  croisent à l'écran.
- Pseudos : écrits en capitales, sans accents ; quinze signes. Sur une seule machine l'hôte et le
  client ont chacun le leur dans `onimp.ini`, mais deux clients lancés là se partagent le même.
- Tenues : un bouton qui passe à la tenue libre suivante, pas une liste déroulante ; une tenue
  prise est sautée, elle n'est pas montrée grisée. Les sept tenues sont celles des données
  communes de l'Anniversary Edition : sans elle, ou avec d'autres mods de personnages, seules
  celles qui existent sont données. Leurs noms dans le menu sont ceux de leurs classes : je ne
  sais pas à quoi chacune ressemble.
- Latence : mesurée entre chaque client et l'hôte ; elle compte le temps d'une image de chaque
  côté, les paquets n'étant lus qu'une fois par image.
- Écran de texte d'une console : il est attribué au joueur qui a activé une console dans la
  seconde qui précède. Une console dont le script attend plus d'une seconde avant de montrer son
  texte l'ouvre chez tous, comme avant.
- Munitions illimitées : l'arme en main ne se vide plus ; les réserves montrées au-dessus des
  joueurs ne bougent donc plus.
- Civils qui donnent un objet : les voix de la conversation ne s'entendent que chez l'hôte. Ce
  qu'un client ne peut plus porter (réserves pleines) est perdu, là où le jeu d'origine le posait
  aux pieds du civil. Une arme donnée est prise en main d'office, celle que le joueur tenait
  tombant à ses pieds ; dans le jeu d'origine c'est l'arme donnée qui restait au sol. Pendant la
  conversation le joueur du client n'est ni tourné vers le civil ni retenu, comme l'est celui de
  l'hôte. Sans l'option `npcs`, chacun a ses propres civils et rien de cela ne joue.
- Points de sauvegarde : en session un client n'en enregistre aucun dans son propre
  `persist.dat`. Sur une autre machine que l'hôte, sa liste de parties à reprendre n'avance donc
  pas pendant la partie.
- La recherche automatique trouve l'hôte, elle ne lance pas la partie : chaque joueur démarre
  encore lui-même le même niveau, et l'hôte ne s'annonce qu'une fois en jeu.
- Annonces : elles ne passent que là où les diffusions passent, donc sur un même réseau local ou
  un même réseau Radmin VPN, pas à travers Internet sans lui. Sur un réseau partagé avec
  d'autres, tout le monde les reçoit.
- Détecteur de désynchronisation : il compare des empreintes, pas tout l'état. Positions et
  animations n'y figurent pas (elles sont envoyées en continu), ni les portes, qui s'ouvrent et
  se ferment sans cesse. Deux PNJ intervertis ou un bonus déplacé ne se voient pas.
- Mods de l'AE Installer : aucun, hors les neuf paquets du socle de l'AE, n'a encore été essayé à
  deux. Un mod installé des deux côtés doit être suivi, OniMP désignant animations, PNJ et armes
  par leur nom ; un mod qui apporte ses propres scripts de niveau reste à vérifier avec l'option
  `quests`.
- Mods : un écart entre l'hôte et un client est signalé, pas empêché. La comparaison porte sur les
  numéros de paquet, pas sur leur version ni sur le contenu des fichiers : deux versions d'un même
  mod passent pour identiques. Un jeu sans `installed_mods.xml` (installé sans l'AE Installer)
  n'est pas comparé.
- Sans l'option `quests`, chaque joueur progresse dans sa propre copie du niveau : scripts,
  objectifs, portes et cinématiques ne sont pas partagés.
- Ne sont jamais synchronisés : voix et cris des PNJ (décidés par leur IA, qui ne tourne que chez
  l'hôte). Chargeurs, cellules et hypos restent à chacun : les autres voient seulement combien de
  chargeurs et de cellules il a.
- Objets au sol : sans l'option `npcs`, chaque joueur a son propre sol, et un bonus qui n'existe
  que chez l'un ne peut pas être retiré chez l'autre. Avec elle, les bonus du client suivent ceux
  de l'hôte avec un quart de seconde de retard, et un bonus posé d'après l'hôte ne tombe pas : il
  rejoint par à-coups l'endroit où celui de l'hôte s'arrête. Les armes au sol ne sont ni créées
  ni retirées d'après l'hôte, seules leurs cartouches le sont : une arme ramassée ou perdue par
  un PNJ n'est pas transmise.
- Vie des PNJ multipliée : les scripts qui attendent qu'un boss passe sous un nombre de points de
  vie gardent leur seuil d'origine. Il est donc atteint plus tard dans le combat, d'autant plus
  tard qu'il y a de clients.
- Les projectiles sont simulés séparément sur chaque machine à partir de la même direction :
  leurs trajectoires peuvent différer légèrement. Ceux d'un personnage piloté par le réseau ne
  blessent pas : un tir de PNJ peut donc vous manquer à l'écran et vous blesser quand même,
  s'il a touché chez l'hôte, ou l'inverse.
- Un coup de PNJ qui touche votre joueur à l'écran sans avoir touché chez l'hôte le fait
  réagir sans le blesser.
- Tir ami : le coup est jugé chez celui qui frappe. Le joueur distant est insensible aux dégâts
  chez vous ; vos coups le blessent sur sa machine, avec le délai du réseau, et peuvent le
  toucher alors qu'ils le manquent à son écran. De même une parade est jugée chez celui qui
  frappe, d'après ce que faisait l'autre joueur un dixième de seconde plus tôt.
- Tir ami : votre joueur ne se tourne pas de lui-même vers l'autre joueur en frappant, comme il
  le fait vers un ennemi. Une prise sur l'autre joueur n'a lieu que si sa classe de personnage a
  l'animation de victime correspondante.
- Avec `run_in_menus = false`, un menu n'arrête le jeu que chez celui qui l'ouvre : les autres
  continuent de jouer, et si c'est l'hôte, les PNJ se figent chez les clients.
- La santé maximale affichée au-dessus de l'autre joueur est celle du personnage distant, pas
  celle que sa machine connaît.
- Portes et consoles ne sont partagées qu'en mode coop (`quests`).
- Le joueur distant est affiché avec 100 ms de retard.
- Une zone de sauvegarde déclenchée par le client enregistre l'état du joueur de l'hôte là où
  il se trouve à ce moment, même loin derrière.
- Pas de sécurité : aucun chiffrement ni authentification. Réseau local de confiance
  uniquement.
- PNJ : un personnage inactif chez l'hôte (loin de son joueur) est envoyé sans animation et
  reste figé chez le client. Les PNJ homonymes sont distingués par leur rang, ce qui peut
  les intervertir.
