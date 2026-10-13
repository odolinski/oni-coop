# OniMP — objectifs

État au 13 octobre 2026, version 0.39. Le fonctionnement du mod est décrit dans
[README.md](README.md).

Trois niveaux de preuve sont distingués, parce qu'ils ne valent pas la même chose :

- **Vu en jeu** : constaté par toi dans une vraie partie. **Vu dans les journaux** : lu dans
  les journaux d'une vraie partie, sans retour sur ce qui se voyait à l'écran.
- **Testé hors jeu** : vérifié par un outil de test, sans lancer Oni.
- **Écrit, pas vérifié** : le code existe et compile, personne ne l'a encore vu fonctionner.

## Réussis

| Objectif | Preuve |
|---|---|
| Charger le mod sans modifier `Oni.exe` (DLL prise en charge par Daodan) | Vu en jeu |
| Toutes les modifications d'Oni.exe en mémoire acceptées, sans plantage au lancement | Vu dans les journaux de la 0.11, hôte et client, deux niveaux, de la 0.12 et de la 0.13, deux niveaux encore : aucune ligne `inattendu`, aucune `EXCEPTION` |
| L'hôte s'annonce sur Radmin VPN, le client le trouve et le rejoint sans adresse, sur une seule machine | Vu dans les journaux de la 0.12 : `partie annoncee sur 26.x.x.x (Radmin VPN)` chez l'hôte ; `partie 'Oni' trouvee a 26.x.x.x:27777 (niveau 'EnvWarehouse') : connexion`, puis `pair connecte` chez le client |
| Lire l'état du joueur : niveau, position, orientation, animation, santé | Vu en jeu |
| Lancer deux fois le jeu sur la même machine | Vu en jeu |
| Connexion hôte / client en UDP, reconnexion après coupure | Vu en jeu |
| Voir l'autre joueur : apparition, déplacement, animations | Vu en jeu |
| PNJ pilotés par l'hôte : mêmes ennemis des deux côtés | Vu en jeu (0.3) |
| PNJ qui s'en prennent aussi au joueur du client | Vu en jeu (0.5) |
| Scripts de niveau interceptés chez l'hôte et rejoués chez le client | Vu dans les journaux, encore en 0.10 : 301 commandes interceptées, 20 rejouées à la connexion, aucun plantage |
| Coups des PNJ qui blessent le joueur du client et le font réagir | Vu dans les journaux, encore en 0.10 : `coup recu : 10 de 'Floor1_Striker_2'`, `reaction : 'KONOKOknockdown_foot_bk' imposee par l'hote` |
| Reprise des deux joueurs à la dernière sauvegarde de l'hôte quand le client meurt | Vu dans les journaux (0.7) : deux reprises au point de sauvegarde 2 |
| Arme lâchée par un joueur ramassable par l'autre | Vu en jeu (0.9) |
| Arme lâchée par un joueur : mêmes cartouches chez l'autre | Vu dans les journaux de la 0.10, dans les deux sens : `lachee avec 8 cartouche(s)` chez l'un, `lachee par l'autre joueur, 8 cartouche(s)` chez l'autre |
| PNJ qui changent de cible pour le joueur le plus proche | Vu dans les journaux de la 0.10, dans les deux sens : `cible : 'Floor1_Striker_2' quitte 'mp_remote' pour 'konoko', plus proche` |
| Tir ami : les tirs d'un joueur blessent l'autre | Vu dans les journaux de la 0.10, de l'hôte vers le client : `tir ami : 3 sur l'autre joueur, annonce`, `coup recu : 3 de 'mp_remote'`, jusqu'à la mort du client |
| Jauge de vie au-dessus de l'autre joueur | Vu en jeu (0.10) : la barre convenait ; les chiffres, sur fond blanc, ont été refaits en 0.11 |
| Tir ami : prises d'un joueur sur l'autre | Vu dans les journaux de la 0.11, dans les deux sens : `projection : 'char_0' projette 'mp_remote', annoncee` chez l'un, `projection : 'mp_remote' projette 'char_0'` chez l'autre. Chute et dégâts à l'écran : pas de retour |
| Vie des PNJ doublée quand le client rejoint l'hôte | Vu dans les journaux de la 0.11, hôte et client : `pnj : vie multipliee par 2 pour 6 personnage(s)`, puis pour le PNJ du niveau suivant |
| Console activée par le client, répercutée chez l'hôte | Vu dans les journaux de la 0.11 : `console 1 activee ici : annoncee`, `console 1 activee par l'autre joueur` |
| Réglages d'affichage gardés d'un lancement à l'autre | Vu sur disque (0.11) : section `[display]` écrite dans `onimp.ini` par un changement fait en jeu |
| Tir ami : coups de mêlée entre joueurs, dans les deux sens, avec la réaction de celui qui encaisse | Vu dans les journaux de la 0.13 : `tir ami : 4 sur l'autre joueur, annonce` et `reaction : 'KONCOMhit_head1' imposee au joueur distant, annoncee` chez celui qui frappe, `reaction : 'KONCOMhit_head1' imposee par l'autre machine` chez l'autre ; de l'hôte vers le client, puis du client vers l'hôte. Signalé en jeu en 0.10 : la mêlée ne passait pas |
| Invincibilité des joueurs, activée par l'hôte | Vu dans les journaux de la 0.13 : après `invincibilite activee` des deux côtés, plus aucun `coup recu` alors que coups et prises continuent d'être annoncés. Effet à l'écran : pas de retour |
| Les scripts lancent le jeu que l'AE Installer tient à jour ; ses mods sont dans le journal | Vu dans les journaux de la 0.13, écrits dans le dossier `AE` : `mods AE : 10 paquet(s), empreinte B5680635 : ..., 34090 Blood`, chez l'hôte et chez le client. Constaté en 0.12 : les scripts lançaient le jeu de la racine, sans les mods choisis |
| Mods comparés entre l'hôte et le client à chaque connexion | Vu dans les journaux de la 0.13, des deux côtés et à chaque reconnexion : `mods AE : memes paquets chez l'autre joueur (10, empreinte B5680635)` |
| Partie avec un mod choisi dans l'AE Installer | Vu dans les journaux de la 0.13 : vingt minutes sur deux niveaux avec Blood installé, sans `EXCEPTION`. Sang à l'écran : pas de retour |
| Passage au niveau suivant | Vu dans les journaux de la 0.13 : `niveau : 'lab'` chez l'hôte puis chez le client, connexion reprise, `pnj pilote : 'Barabus'`. Défaut : le client charge ce niveau deux fois (voir « Vie et mort des joueurs ») |
| Le client retrouve seul l'hôte après un chargement de niveau | Vu dans les journaux de la 0.13 : `l'hote ne repond plus : nouvelle recherche`, `partie 'Oni' trouvee a 26.x.x.x, mais complete`, puis `: connexion` six dixièmes de seconde plus tard |
| Quatre joueurs : un hôte et trois clients, chacun avec son numéro | Vu dans les journaux de la 0.15, quatre fenêtres sur une machine : `pair connecte (...), joueur 1`, puis `joueur 2`, puis `joueur 3` chez l'hôte ; `l'hote ; ici joueur N` chez chaque client, un numéro chacun ; aucune `EXCEPTION` |
| Chaque joueur voit tous les autres : un double par joueur | Vu dans les journaux de la 0.15 : chez chacun des quatre, un `spawn : 'mp_remoteN'` pour chacun des trois autres, de `mp_remote0` à `mp_remote3`. À l'écran : pas de retour |
| PNJ partagés à quatre : le plus proche de tous les joueurs pour cible, coups portés au bon client, dégâts des clients comptés | Vu dans les journaux de la 0.15 : `cible : 'A_t48' quitte 'mp_remote2' pour 'mp_remote3', plus proche` ; `reaction : 'KONCOMhit_foot1' imposee au joueur 3, annoncee` chez l'hôte, `coup recu : 6 de 'A_t48'` chez le client 3 et chez lui seul ; `degats client 3 : 4 sur 'A_t48'` |
| Difficulté selon le nombre de clients : vie et dégâts des PNJ | Vu dans les journaux de la 0.15, sur les quatre machines : `difficulte : PNJ : vie x2, degats x1.05 (1 client(s))`, puis `vie x3, degats x1.10`, puis `vie x4, degats x1.15`. Dégâts : deux coups de 6 points reçus `6` puis `7` par le client 3, soit 15 % de plus |
| Réglage `Online damage` dans l'onglet Coop du menu F1 de l'hôte | Vu dans les journaux de la 0.15 : `reglage : degats en ligne desactives`, puis `actives` une seconde plus tard chez l'hôte ; chez le client, `difficulte : PNJ : vie x2, degats normaux` deux millisecondes après, menu de l'hôte encore ouvert |
| Un client parti compte encore vingt secondes pour la difficulté | Vu dans les journaux de la 0.15 : `pair perdu` à 21:15:40, `difficulte : PNJ : vie et degats d'origine` à 21:16:00 |
| Un journal par client lancé sur la même machine | Vu sur disque après la partie à quatre : `onimp_client.log`, `onimp_client_2.log`, `onimp_client_3.log` |
| Vie des PNJ portée à ce que dit le fichier de difficulté quand un client arrive | Vu en jeu (0.16) : vie doublée. Journaux de la 0.16, hôte et client : `difficulte : onimp_difficulty.ini lu ; vie 200 / 300 / 400, ...`, `difficulte : PNJ : vie x2.00, ... (1 client(s))`, `pnj : vie portee a 200 pour cent de l'origine pour 1 personnage(s)`, puis `vie d'origine rendue` vingt secondes après le départ du client |
| Jauges au-dessus des autres joueurs et des PNJ, chez l'hôte et chez le client | Vu en jeu (0.16) : elles y sont. Elles clignotaient : corrigé en 0.17, à confirmer |
| Jeu qui continue derrière le menu F1 de l'hôte | Vu dans les journaux de la 0.16 : `menu : ouvert, le jeu continue derriere`, puis `menu : ferme apres 771 image(s), 287 tick(s) simule(s)` pour cinq secondes de menu ; tir ami et invincibilité changés pendant ce temps, reçus par le client dans le dixième de seconde ; `menu : plus personne, le jeu se met en pause` au départ du client. À l'écran : pas de retour |
| Écrans de texte du niveau ouverts depuis la boucle du jeu : plus de jeu figé après leur fermeture | Vu dans les journaux de la 0.20, hôte et client, vingt minutes : cinq écrans ouverts chez l'hôte, quatre chez le client (`ecran de texte : 'level_1d' demande, le script continue`, `ouvert`, `menu : ferme apres 200 image(s)` chez l'hôte, `621 image(s)` chez le client), chacun refermé à son heure, et la partie continue. Avant : `Oni.exe a cessé d'interagir` au journal de Windows pour la 0.19 |
| Trois niveaux à la suite à deux : `EnvWarehouse`, `manplant`, `lab` | Vu dans les journaux de la 0.21 : connexion reprise à chaque niveau, PNJ pilotés, renforts créés chez le client, combat contre Barabus (`degats client 1 : N sur 'Barabus'`), aucune `EXCEPTION` en cinquante minutes |
| Plus d'inondation d'annonces pour deux consoles de même numéro | Vu dans les journaux de la 0.21 : `console 7 (rang 1) activee ici : annoncee`, une seule fois ; journaux de 28 et 47 Ko pour cinquante minutes, contre 5 Mo en 0.20 |
| Messages du mod dans leur propre zone | Vu dans les journaux de la 0.21, hôte et client : `messages : zone creee en haut a droite (1280 x 800), 6 ligne(s)`. À l'écran : pas de retour |
| Activation simple par les scripts `run_*.bat` et la variable `ONIMP` | Vu en jeu |
| Canal réseau fiable et ordonné pour les événements | Testé hors jeu |
| Installation qui conserve le fichier d'origine (`realbink_orig.dll`, empreinte identique) | Vérifié sur disque |

## Faits, à confirmer en jeu

Par version d'apparition.

| Objectif | Version | État |
|---|---|---|
| Commandes de console `mp_host`, `mp_connect`, `mp_status`, `mp_npcs` | 0.1 | Enregistrées par le moteur, jamais exécutées |
| Réglage par `onimp.ini` seul, sans les scripts | 0.1 | Écrit, pas vérifié |
| Désinstallation (`uninstall.bat`) | 0.1 | Écrit, jamais lancé |
| Sprint fluide dans les deux sens | 0.2 | Testé hors jeu par simulation ; pas de retour explicite depuis le correctif |
| Objectifs, dialogues et fin de niveau identiques des deux côtés | 0.3 | Mécanisme vu en jeu, résultat à l'écran pas détaillé |
| Dégâts infligés par le client comptés chez l'hôte | 0.3 | Vu dans les journaux de la 0.5 (`degats client : ...`) |
| Portes à touche d'action ouvertes chez les deux joueurs | 0.4 | Écrit, pas vérifié |
| Arme visible dans la main de l'autre joueur | 0.4 | Vu en jeu. Défaut vu dans les journaux de la 0.10, et encore en 0.13 à chaque apparition du double : il n'avait pas l'arme annoncée (`arme du pantin : tient '', range '', attendu 'w1_tap'`), elle lui a été redonnée par script. Cause inconnue |
| Tirs de l'autre joueur visibles | 0.4 | Vu en jeu |
| Plus de tir fantôme quand un joueur frappe avec la touche de tir | 0.5 | Pas de nouveau signalement depuis le correctif |
| Projections faites par le client : chute et dégâts du PNJ chez les deux joueurs | 0.5 | Vu dans les journaux : rejouée chez l'hôte. Perdues après une reconnexion, corrigé en 0.6 |
| Projections d'un PNJ sur le joueur du client | 0.5 | Vu dans les journaux de la 0.6, et de la 0.13 avec un boss : `projection : 'Barabus' projette 'mp_remote', annoncee` chez l'hôte, `projection : 'Barabus' projette 'konoko'` chez le client |
| Canal fiable intact après un chargement de niveau trop long chez l'hôte | 0.6 | Testé hors jeu ; le défaut avait été vu dans les journaux de la 0.5 |
| Dégâts arbitrés : plus de coup compté deux fois ni perdu | 0.6 | Écrit, pas vérifié ; détour testé hors jeu |
| Tirs des PNJ chez le client | 0.6 | Vu dans les journaux de la 0.7 : dégâts de tirs reçus, plus de plantage. Visibles et audibles : pas de retour |
| Arme ou bonus ramassé par un joueur retiré chez l'autre | 0.6 | Vu dans les journaux |
| Le client suit aussi quand c'est l'hôte qui meurt ou recharge | 0.7 | Écrit, pas vérifié |
| Rechargement de l'autre joueur visible | 0.8 | Écrit, pas vérifié |
| Le client déclenche les zones du niveau (apparition des ennemis) quand il est en avance | 0.8 | Écrit, pas vérifié. Vu en jeu jusqu'en 0.7 : rien ne se déclenchait |
| Projection par un PNJ : une seule chute, dégâts comptés une fois | 0.8 | Écrit, pas vérifié. Vu en 0.7 : chute et dégâts en double |
| Tirs dans la bonne direction, bras et arme orientés comme chez l'autre joueur | 0.9 | Vu dans les journaux de la 0.10 : le détour s'applique au double de chaque joueur, et à un PNJ chez le client (`visee : '...' suit la direction recue`). Résultat à l'écran : pas de retour. Signalé en 0.8 : visée, bras et projectiles pas synchronisés |
| Tir ami du client vers l'hôte | 0.10 | Vu dans les journaux de la 0.13 pour la mêlée : `tir ami : 10 sur l'autre joueur, annonce` chez le client. Avec une arme : pas de retour |
| Onglet Coop dans le menu F1 de l'hôte, avec ces deux réglages | 0.10 | Journaux de la 0.13 : les deux réglages changés à deux secondes d'écart chez l'hôte, et reçus ensemble par le client 2,3 s après le premier. L'hôte n'émettait donc plus : menu ouvert. Depuis la 0.14 ils partent sans attendre qu'il le referme |
| Connexion gardée pendant que le menu F1 est ouvert | 0.10 | Écrit, pas vérifié |
| Commandes de console `mp_teamkill`, `mp_godmode` | 0.10 | Écrit, pas vérifié |
| Onglet Display du menu F1, chez l'hôte et chez le client | 0.11 | Réglages écrits dans `onimp.ini`, donc changés en jeu ; par le menu ou par la console, pas de retour |
| Jauges refaites : barre verte, orange puis rouge, chiffres sur la barre, plus de fond blanc | 0.11 | Vu en jeu en 0.16 : les jauges s'affichent, mais clignotent (voir 0.17). Couleurs, chiffres, fond : pas de retour. Signalé en jeu en 0.10 : fond blanc sous les chiffres |
| Santé des PNJ au-dessus de leur tête | 0.11 | Vu en jeu en 0.16, chez l'hôte et chez le client ; elle clignotait (voir 0.17) |
| Commandes de console `mp_playerhealth`, `mp_npchealth` | 0.11 | Écrit, pas vérifié |
| Plus de butin en double chez le client : un PNJ n'y meurt que quand l'hôte l'annonce | 0.11 | Écrit, pas vérifié. Signalé en jeu en 0.10 : trop de munitions au sol chez le client. Cause lue dans Oni.exe : PNJ tué sur place, ranimé par l'état de l'hôte, tué de nouveau |
| Bonus ramassé par un joueur : c'est le même qui disparaît chez l'autre | 0.11 | Écrit, pas vérifié. Cause lue dans Oni.exe : le type du bonus était lu au mauvais endroit |
| Bonus au sol du client conformes à ceux de l'hôte ; cartouches des armes tombées des PNJ | 0.11 | Écrit, pas vérifié |
| L'hôte annonce sa partie sur Radmin VPN, le client le trouve et le rejoint sans adresse, entre deux machines | 0.12 | Vu dans les journaux sur une seule machine, par la carte Radmin (voir « Réussis »). Pas vérifié entre deux machines |
| Refus explicites de l'hôte : partie complète, version différente | 0.12 | Testé hors jeu. Partie complète : lu aussi dans le journal du client en 0.13 (`..., mais complete`) |
| Radmin VPN coupé puis rétabli en cours de partie : signalé, connexion reprise | 0.12 | Écrit, pas vérifié |
| Détecteur de désynchronisation : empreintes comparées chaque seconde, renvoi forcé de ce qui diffère | 0.12 | Vu dans les journaux de la 0.13 : un écart détecté et renvoyé (`desync : reglages differe chez le client (0000000F ici, 0000000C chez lui), renvoi 1/3` chez l'hôte, `desync : l'hote renvoie son etat (reglages)` chez le client), rien d'autre en vingt minutes. C'était une fausse alerte : l'hôte sortait de son menu de pause et lisait d'un coup des rapports en retard sur des réglages déjà en route. Corrigé en 0.14 |
| Commande de console `mp_join` | 0.12 | Écrit, pas vérifié |
| Mods différents entre l'hôte et un client : écart signalé dans leurs deux consoles | 0.13 | Comparaison testée hors jeu. Message : écrit, jamais vu, les fenêtres d'une même machine partageant leurs mods |
| Lancement arrêté, avec un message, quand OniMP n'est pas installé dans le jeu visé | 0.13 | Détection vérifiée sur disque, sur la DLL OniMP et sur la DLL d'origine. Message : écrit, jamais vu |
| `ae_installer.bat` : ouvre l'AE Installer du jeu que lancent les scripts | 0.13 | Écrit, jamais lancé |
| Ce qui valait entre deux joueurs vaut entre tous : armes en main, lâchées, ramassées, bonus, portes, consoles, rechargement, projections, tir ami et réactions | 0.14 | Vu dans les journaux de la 0.15 pour l'arme en main (chaque double reçoit la sienne) et une prise de l'hôte sur un client. Entre deux clients, par l'hôte : testé hors jeu, pas de retour en jeu |
| Zones du niveau déclenchées par n'importe quel client | 0.14 | Écrit, pas vérifié |
| Reprise commune quand n'importe quel client meurt ; scripts du niveau rejoués au seul client qui arrive | 0.14 | Écrit, pas vérifié |
| Détecteur de désynchronisation tenu pour chaque client séparément | 0.14 | Règles testées hors jeu. En jeu : écrit, pas vérifié |
| Menu de pause de l'hôte : le réseau reste servi, ses réglages partent menu ouvert | 0.14 | Vu dans les journaux de la 0.15 : un réglage changé dans le menu arrive au client deux millisecondes plus tard. Remplacé en 0.16 par le jeu qui continue derrière les menus |
| Plus de fausse alerte `desync : reglages` quand l'hôte referme son menu | 0.14 | Testé hors jeu (rapports en rafale). Vue en jeu en 0.13 |
| Jauge des autres joueurs : la barre seule, sans chiffres | 0.14 | Écrit, pas vérifié |
| Commande de console `mp_onlinedamage` | 0.15 | Écrit, pas vérifié |
| Corrections de la revue de code : joueur revenu après une longue absence de nouveau visible des autres clients, message trop long d'un client écarté, une seule reprise pour deux clients morts à la suite, annonce d'un client redistribuée avant ses conséquences | 0.15 | Écrit, pas vérifié. Les tests réseau passent toujours |
| Difficulté réglable dans un fichier, `onimp_difficulty.ini` : vie et dégâts des PNJ pour un, deux et trois clients, en pour cent | 0.16 | Lecture testée hors jeu, sur le fichier installé : valeurs par défaut, valeurs partielles, bornes. En jeu : écrit, pas vérifié |
| Ce fichier relu en cours de partie, à chaque chargement de niveau et à chaque arrivée ou départ d'un client | 0.16 | Écrit, pas vérifié. Depuis la 0.17 il est relu dès qu'il est enregistré |
| En session, le jeu continue derrière les menus (F1, Échap, écran d'une console), chez l'hôte comme chez les clients | 0.16 | Menu F1 de l'hôte : vu dans les journaux de la 0.16 (voir « Réussis »). Menu d'un client : vu dans le journal d'un client de la 0.19, `menu : ferme apres 9945 image(s), 3618 tick(s) simule(s)` pour une minute de menu, des coups de PNJ reçus pendant ce temps. Échap : écrit, pas vérifié. Écran d'une console : il figeait par moments le jeu, voir la 0.20. Vérifié dans l'exécutable : l'appel détourné est celui de la boucle des dialogues, dans les deux copies d'`Oni.exe`, et Daodan n'y touche pas |
| Le joueur qui a un menu ouvert lâche ses commandes au lieu de continuer sur sa lancée | 0.16 | Écrit, pas vérifié |
| Réseau servi dans tous les menus, même sans autre joueur : un client peut arriver pendant que l'hôte est dans le sien | 0.16 | Vu dans le journal d'un client de la 0.16, dans l'autre sens : menu ouvert seul (`personne d'autre, le jeu est en pause`), hôte trouvé et rejoint une seconde plus tard, `menu : un joueur est relie, le jeu reprend derriere`. Chez l'hôte : pas vérifié |
| Jauges qui ne clignotent plus : tracé immédiat, dans l'ordre, chaque couche un peu plus près de la caméra | 0.17 | Écrit, pas vérifié. Signalé en jeu en 0.16 : toutes les jauges clignotaient, chez l'hôte et chez le client. Cause lue dans Oni.exe : les rectangles transparents attendent la fin de l'image dans une liste triée par profondeur, et ceux d'une jauge avaient tous la même |
| Fichier de difficulté relu dès qu'il est enregistré, sans chargement de niveau ni reconnexion ; la console de l'hôte le confirme | 0.17 | Testé hors jeu : fichier écrit, lu, réécrit, relu, date qui change. En jeu : écrit, pas vérifié |
| Console et journal qui disent pourquoi les dégâts du fichier ne comptent pas quand `Online damage` est sur `OFF` | 0.17 | Écrit, pas vérifié |
| Chargement de la 0.17, une partie à deux | 0.17 | Vu dans le journal de l'hôte : fichier lu, `difficulte : PNJ : vie x2.00, degats x1.05 (1 client(s))`, `pnj : vie portee a 200 pour cent de l'origine pour 3 personnage(s)`, aucune ligne `inattendu` ni `EXCEPTION`. Jauges à l'écran : pas de retour |
| Curseurs `NPC health` et `NPC damage` dans l'onglet Coop du menu F1 de l'hôte : vie et dégâts que chaque client ajoute aux PNJ | 0.18 | Écrit, pas vérifié : ni l'aspect, ni le déplacement. Demandé en 0.17 pour ne plus avoir de fichier à modifier. Valeurs et écriture du fichier testées hors jeu ; contrôle du moteur et ses fonctions lus dans Oni.exe (dialogue Options) |
| Vie des PNJ et jauges qui suivent le curseur pendant qu'il glisse, chez l'hôte et chez les clients | 0.18 | Vu dans le journal d'un client de la 0.19 : pendant que l'hôte déplace un curseur, `difficulte : PNJ : vie x8.85, degats x4.50 (1 client(s))`, puis `x7.20`, `x6.15`, une ligne tous les dixièmes de seconde. Jauges à l'écran : pas de retour |
| Valeurs des curseurs gardées dans `onimp_difficulty.ini`, retrouvées au lancement suivant | 0.18 | Écriture et relecture testées hors jeu. En jeu : écrit, pas vérifié |
| Déplacer `NPC damage` remet `Online damage` sur `ON` | 0.18 | Écrit, pas vérifié |
| Quatre modes de difficulté (facile, normal, difficile, survie), choisis par l'hôte dans l'onglet Coop et suivis aussitôt par les clients | 0.19 | Mode survie : vu dans le journal d'un client de la 0.19, `reglage : difficulte de l'hote Survival`, `OniMP : difficulte Survival : 5 s pour ranimer, releve a 25 % de sante`. Les autres modes, et l'effet en jeu : pas de retour. Valeurs des modes testées hors jeu |
| Ennemis du mode facile, normal ou difficile du jeu d'origine selon le mode : la difficulté lue par le moteur est celle de l'hôte, sans toucher aux options du jeu | 0.19 | Écrit, pas vérifié. Fonction du moteur et ses appelants lus dans Oni.exe ; mêmes octets dans les deux copies |
| Mode survie : vie et dégâts des ennemis x1,5 au-delà du mode difficile, +1 point de vie par seconde, +2 pour un boss | 0.19 | Vie et dégâts : vu dans le journal d'un client de la 0.19, `difficulte : PNJ : vie x9.00, degats x1.50 (1 client(s))`, soit les 600 % de vie du fichier multipliés par 1,5 et des dégâts normaux multipliés par 1,5, puis `degats x4.50` une fois `Online damage` remis (300 % dans le fichier). Régénération : pas de retour |
| Joueur à terre au lieu de mort quand un coéquipier est debout ; compte à rebours de 35, 25, 10 ou 5 s au-dessus de lui ; alerte à l'écran des coéquipiers | 0.19 | Écrit, pas vérifié. Lu dans Oni.exe : coup mortel arrêté à un point de vie et chute au sol d'un personnage increvable, touches coupées comme en cinématique |
| Réanimation : CTRL gardé 3 s près du joueur à terre, compte à rebours arrêté pendant ce temps ; interrompue par un coup d'ennemi, il reprend où il en était ; relevé à 45, 40, 35 ou 25 % de sa santé | 0.19 | Écrit, pas vérifié. Compte à rebours arrêté puis repris testé hors jeu |
| Compte à rebours à zéro : le joueur meurt, la mission échoue pour tous et reprend à la dernière sauvegarde | 0.19 | Écrit, pas vérifié |
| Jauges aux couleurs : hôte vert, clients bleu, orange, jaune ; ennemis rouge ; boss violet | 0.19 | Écrit, pas vérifié |
| Multiplicateur d'ennemis x1 à x4 dans l'onglet Coop ; minimum x2 à deux joueurs, x3 à trois, x4 verrouillé à quatre | 0.19 | À deux : vu dans le journal d'un client de la 0.19, l'hôte passe de `ennemis x2` à `x3`, `x4`, puis de nouveau `x2`, jamais x1. À trois et à quatre : pas de retour. Minimum et choix du menu testés hors jeu |
| Renforts : copies des ennemis ordinaires, sans les boss ni les personnages dont un script du niveau dépend ; créés aussi chez les clients | 0.19 | Vu dans le journal d'un client de la 0.19, à x4 : `renfort : 'A_t50_x4' cree d'apres l'hote`, de même `_x3` et `_x2`, puis `pnj pilote : 'A_t50_x4'` ; pareil pour `Bay1_Thug_1` et `Bay2_Thug_2` ; aucune `EXCEPTION`. À l'écran, et jusqu'à la fin d'un niveau : pas de retour. Filtre essayé sur les données des quatorze niveaux : 562 ennemis sur 704, aucun nom déjà pris |
| Civils mêlés aux ennemis dans les niveaux qui s'y prêtent ; mission échouée si un joueur en tue un | 0.19 | Écrit, pas vérifié. Tirage et noms testés hors jeu |
| Onglet Coop chez les clients, en lecture, mis à jour menu ouvert | 0.19 | Lecture seule : vu dans le journal d'un client de la 0.19, `OniMP : reglage reserve a l'hote` à chaque clic, menu ouvert. Mise à jour menu ouvert : pas de retour |
| Commandes de console `mp_difficulty`, `mp_enemies`, `mp_civilians` | 0.19 | Écrit, pas vérifié |
| Écrans de texte du niveau (cadre bleu, plein écran) ouverts depuis la boucle du jeu : plus de jeu figé après leur fermeture | 0.20 | Vu dans les journaux de la 0.20 (voir « Réussis »). Signalé en jeu en 0.19 : par moments l'écran de l'hôte ou d'un client restait figé après un de ces écrans, jeu à relancer. Cause lue dans Oni.exe : l'écran s'ouvrait de l'intérieur du script, la simulation repartait derrière lui et relançait ce même script, qui ne rendait plus les commandes. Détour testé hors jeu sur le même prologue ; mêmes octets dans les deux copies d'`Oni.exe` |
| Le script qui demande un écran de texte continue sans attendre sa fermeture ; personne n'attend la lecture d'un autre | 0.20 | Écrit, pas vérifié |
| Messages du mod en haut à droite de l'écran, en vert, alignés à droite | 0.20 | Vu en jeu en 0.20 : ils y étaient, mais le texte du scénario (sous-titres) avait suivi, la console du jeu entière ayant été déplacée. Refait en 0.21. Demandé en jeu en 0.19 : en bas de l'écran ils tombaient sur ce qui s'y affiche. Zone de texte de la console lue dans Oni.exe |
| Bouton `Messages` de l'onglet Display, commande `mp_messages` ; réglage gardé dans `onimp.ini` | 0.20 | Réglage vu sur disque : `messages=true` écrit dans `AE\onimp.ini` par un changement fait en jeu |
| Texte du scénario remis comme avant : la console du jeu n'est plus touchée, les messages du mod ont leur propre zone en haut à droite | 0.21 | Écrit, pas vérifié. Signalé en jeu en 0.20 : le texte du scénario était parti en haut à droite avec les messages du mod |
| Plus d'inondation d'annonces pour deux consoles (ou deux portes) de même numéro | 0.21 | Vu dans les journaux de la 0.21 (voir « Réussis »). Lu dans les journaux de la 0.20 : 99 319 `console 7 activee ici : annoncee` chez le client en dix minutes, une par image, jusqu'à son plantage ; `desync : consoles differe` chez l'hôte |
| PNJ qui quittent un joueur à terre pour le joueur debout le plus proche | 0.21 | Écrit, pas vérifié. Signalé en jeu en 0.20 : ils restaient autour du joueur à terre sans le frapper, et ne changeaient de cible que si un autre joueur venait se faire toucher. Cause lue dans Oni.exe : le moteur ne revoit la cible d'une IA que lorsqu'un contact lui arrive |
| Rechargement visible chez les autres, pour les joueurs et les PNJ | 0.21 | Transmis, pas montré : signalé en jeu en 0.21, et lu dans ses journaux (`rechargement : 'mp_remote1' ne peut pas le montrer : pas d'animation pour cette arme`, cinq fois). Cause : le double n'avait pas la variante d'animation de son arme. Corrigé en 0.22, à confirmer. Signalé en jeu en 0.20 : on ne voyait pas recharger. L'annonce de la 0.8 n'écrivait rien au journal ; celle-ci dit ce qui est joué, ou pourquoi non |
| Aura d'un joueur surpuissant (hypo pris en pleine forme) visible chez les autres | 0.21 | Émise : vu dans les journaux de la 0.21 (`aura : allumee chez notre joueur, annoncee dans son etat`, puis `eteinte`). Chez l'autre joueur : pas de retour. Champ et calcul lus dans Oni.exe |
| Identifiants au-dessus des PNJ et des armes au sol ; boutons `NPC IDs` et `Weapon IDs` de l'onglet Display, commandes `mp_npcids` et `mp_weaponids` | 0.21 | Écrit, pas vérifié. Police vérifiée hors jeu |
| Journal de plantage : registres et adresses de retour de la pile | 0.21 | Écrit, pas vérifié. Demandé par le plantage du client de la 0.20 (`EXCEPTION C0000005 a 0042e228`, tracé de texte du moteur), dont l'appelant n'a pas pu être retrouvé |
| Rechargement montré : le double reçoit la variante d'animation de son arme | 0.22 | Joué sur l'autre machine : vu dans les journaux de la 0.22, `rechargement : 'mp_remote1' recharge 'w2_sap' comme sur l'autre machine` chez l'hôte, la même ligne pour `'mp_remote0'` chez le client. À l'écran : pas de retour. PNJ chez le client : `ne peut pas le montrer : personnage inactif`, quatre fois |
| Arrivée d'un joueur : compte à rebours, reprise commune au dernier point de sauvegarde, nouveau venu sans armes | 0.22 | En partie : vu dans les journaux de la 0.22, pas de reprise quand les deux niveaux viennent d'être chargés au même point (`rien a reprendre`), inventaire vidé (`arrivee : inventaire de notre joueur vide`). Compte à rebours et reprise : pas vérifiés |
| Départ commun après chaque chargement, écrans d'accueil et de fin de niveau sautés | 0.22 | Écran d'accueil sauté : vu dans les journaux de la 0.22, hôte et client. Attente et départ donnés par l'hôte après un chargement commun : pas vérifiés |
| Objet du niveau donné à tous quand un joueur le ramasse | 0.22 | Écrit, pas vérifié |
| Renforts alignés sur leur original : équipe, passivité, joueurs connus | 0.22 | Écrit, pas vérifié |
| Arme des PNJ dictée par l'hôte | 0.22 | À l'œuvre : vu dans le journal du client de la 0.22 (`arme du PNJ 'Floor1_Striker_1_x3' : 'w2_sap' lachee ici seulement, reprise`). À l'écran, et avec Barabus : pas de retour |
| Numéro des armes : deux armes de la même classe ne sont plus confondues, un échange ne fait plus perdre ni gagner de cartouches | 0.23 | Écrit, pas vérifié en jeu. Signalé en jeu en 0.22, et lu dans ses journaux dans les deux sens : `sol : arme 'w2_sap', 0 cartouche(s) au lieu de 28 d'apres l'hote` après un échange du client, `30 cartouche(s) au lieu de 0` après un autre. Rapprochement vérifié hors jeu (`onimp_groundtest`) |
| Munitions des autres joueurs au-dessus de leur barre de vie, chez l'hôte et chez les clients | 0.24 | Vues en jeu en 0.24, à en juger par la demande qui a suivi : des carrés et non des ronds, une seule rangée de six cases, plus sobre. Refait ainsi en 0.25 : écrit, pas vérifié |
| Aura d'un joueur surpuissant : la moindre aura part, et le journal dit ce que le moteur en montre chez les autres | 0.24 | Vu dans les journaux de la 0.24, trois fois : hôte `aura : allumee chez notre joueur, annoncee dans son etat` ; client `aura : allumee sur 'mp_remote0' d'apres sa machine`, puis `aura : 'mp_remote0' : le moteur la montre ici a 0.31 (demandee 0.31)`. À l'écran : pas de retour |
| Bouclier de force et invisibilité d'un joueur visibles chez les autres ; PNJ de l'hôte qui ne voient plus un client invisible, bouclier d'un client qui arrête les tirs chez l'hôte | 0.24 | Bouclier transmis : vu dans les journaux de la 0.24, `bouclier : notre joueur en porte un` chez l'hôte, `bouclier : 'mp_remote0' en porte un d'apres sa machine (100)` chez le client, puis `n'en a plus` quatre minutes après. À l'écran, et l'invisibilité : pas de retour |
| Civils et alliés du niveau qui parlent au joueur d'un client et lui remettent ce qu'ils donnent : hypos, chargeurs, cellules, bouclier, arme, objet du niveau | 0.24 | Vu dans les journaux de la 0.24, pour un hypo, niveau `lab` : hôte `civil : 'Floor2_Sci_5' engage la conversation avec le joueur 1, a sa touche d'action`, dix secondes après `civil : 1 hypo(s) donne(s) au pantin du joueur 1 : envoye(s) a son joueur` ; client `civil : 1 hypo(s) recu(s) d'un PNJ de l'hote, 1 pris`. À voir : la conversation n'est notée terminée que trois minutes plus tard. Arme, bouclier, objet du niveau : pas de retour |
| Points de sauvegarde : ceux de l'hôte valent pour tous, un client relié n'enregistre rien de lui-même | 0.24 | Vu dans les journaux de la 0.24 : hôte `sauvegarde : point 3 atteint, dit aux clients`, client `sauvegarde : l'hote a atteint le point 3`, sans faute. Le plantage du client est venu huit minutes plus tard, d'ailleurs (voir la ligne des zones de texte) |
| Fermeture du jeu : plus de faute en libérant une zone de texte du moteur restée sans lignes ; état des quatre zones au journal | 0.24 | Ligne vue dans le journal de l'hôte de la 0.24, zones saines : `fermeture : le jeu se ferme ; zones de texte : commande 0/1, console 0/3, messages 0/13, sous-titres 0/13,`. Cas d'une zone sans lignes : pas rencontré |
| Bouton `Player ammo` de l'onglet Display, commande `mp_playerammo` | 0.24 | Écrit, pas vérifié |
| Plantage du client en pleine partie : zones de texte du moteur contrôlées à chaque image et juste avant leur tracé, refaites à neuf si elles ont perdu leurs lignes | 0.25 | Vu dans le journal du client de la 0.25 : la zone des messages a été trouvée abîmée et refaite quatre fois, sans plantage ; ces lignes ont donné la cause, corrigée en 0.26. Lu dans le journal du client de la 0.24 : `EXCEPTION C0000005 a 0043102e (Oni.exe), acces 0000020c`, tracé d'une zone de texte dont le pointeur des lignes est nul ; même zone, même état que la faute de fermeture de la 0.23. Ce qui la vide chez le client n'est pas établi : le journal le dira (`zone de texte '...' du moteur abimee ... octets ...`) |
| Titre des fenêtres : `Host - Oni` et `Client - Oni` par les scripts `run_host_radmin.bat` et `run_client_radmin.bat` | 0.25 | Vu dans les journaux de la 0.25 : `fenetre : titre 'Host - Oni' (c'etait 'ONI ')`, et `'Client - Oni'` |
| Munitions : une seule rangée de six cases carrées, rouges depuis la gauche pour les balles, vertes depuis la droite pour l'énergie, petite et sobre | 0.25 | Écrit, pas vérifié. Mon choix, à confirmer : quand il y a plus de six munitions en tout, les cases du milieu sont mi-rouges mi-vertes |
| Pseudo : champ de saisie dans l'onglet Display, gardé dans `onimp.ini` chez l'hôte comme chez le client, affiché au-dessus de la barre de vie de chaque coéquipier ; commande `mp_name` | 0.25 | Saisie et enregistrement vus dans les journaux de la 0.25 (`joueurs : pseudo 'Odolinsky' enregistre dans onimp.ini`) ; pas de retour sur l'affichage. Champ de saisie du moteur lu dans Oni.exe et dans les données (`dialog_setup_player`) |
| Jauge, munitions et pseudo de chaque joueur lisibles à travers les murs | 0.25 | Écrit, pas vérifié |
| Tenues de Konoko : une par joueur, attribuée d'office à l'arrivée, changée par le bouton `Skin`, jamais la même à deux joueurs, vue chez les quatre | 0.25 | Changement vu dans les journaux de la 0.25 (`tenue : 'konoko' passe de 'konoko_generic' a 'k3' (joueur 1)`) ; pas de retour sur ce qui se voit. Un bouton qui passe à la tenue libre suivante, et non la liste déroulante demandée. Sept classes lues dans les données communes |
| Latence de chaque joueur à côté de son pseudo, bouton `Ping` | 0.25 | Mesure testée hors jeu avec le réseau. Affichage : écrit, pas vérifié |
| Écran de texte d'une console de données ouvert chez le seul joueur qui l'utilise | 0.25 | Console d'un client : vu en jeu et dans les journaux de la 0.25. Console de l'hôte : l'écran partait encore chez tous, corrigé en 0.26. Constaté dans les journaux de la 0.24 : `ecran de texte : 'level_3c' ouvert` chez l'hôte et chez le client pour une console de l'hôte |
| Munitions illimitées pour tous, bouton `Unlimited ammo` de l'onglet Coop, réservé à l'hôte ; commande `mp_unlimitedammo` | 0.25 | Réglage vu dans les journaux de la 0.25 chez l'hôte et chez le client (`reglage : munitions illimitees activees`, puis `desactivees`). Effet sur les armes : pas de retour |
| Plantage du client après une vingtaine de minutes : débordement de l'historique des commandes du moteur corrigé, commandes du mod tenues hors de cet historique | 0.26 | Écrit, pas vérifié en jeu. Cause lue dans le journal du client de la 0.25 : `zone de texte 'messages' du moteur abimee` quatre fois, toutes les cinq secondes, dix-neuf minutes après le lancement, un seul octet du pointeur changé ; et dans l'exécutable (`0x4317D0`, case 256 d'un tableau de 256) |
| Écran de texte d'une console utilisée par l'hôte : chez lui seul. L'activation d'une console est dite par le moteur au moment où elle a lieu | 0.26 | Écrit, pas vérifié en jeu. Défaut lu dans les journaux de la 0.25 : `'level_1a' demande par un script, pour tous les joueurs` pour une console de l'hôte, `... que le joueur 1 vient d'utiliser, pour lui seul` pour celle du client. Détour testé hors jeu |
| Une seule arme par PNJ : la sienne reprise où qu'elle soit tombée, le sol du client aligné sur celui de l'hôte, l'arme en trop retirée ; chez l'hôte, arme du sol retirée si le PNJ en reçoit une neuve | 0.26 | Écrit, pas vérifié. Signalé en jeu en 0.25 pour Barabas ; aucun journal de ce combat, cause déduite du code |
| Langue du mod, français ou anglais : bouton `Language` de l'onglet Display, effet immédiat sur les messages, le menu F1 et le panneau d'un joueur à terre ; commande `mp_language` | 0.26 | Dictionnaire et formats testés hors jeu (`onimp_langtest`, 158 textes). Menu et messages en jeu : écrit, pas vérifié |
| Joueur à terre : panneau aux couleurs du jeu, repère qui bat et se voit à travers les murs à taille constante, invite `CTRL POUR RANIMER` à portée | 0.26 | Écrit, pas vérifié. Jamais vu à l'écran : couleurs, tailles et place sont à juger en jeu |
| Barabas (ou tout PNJ) en double à chaque reprise d'une sauvegarde : le journal des scripts de l'hôte est vidé à tout rechargement ; un double chez le client n'est plus tracé et part en une demi-seconde | 0.28 | Écrit, pas vérifié. Cause lue dans les journaux de la 0.27 : commandes rejouées au client 16, 32, 48... 176 au fil des reprises de `lab`, et `pnj orphelin : suppression de 'Barabus'` 58 fois |
| PNJ vus de loin qui flottent, glissent, tremblent : gardés actifs par l'hôte près de chaque joueur, et chez le client | 0.28 | Écrit, pas vérifié. Cause déduite du code, pas vue dans un journal |
| Joueur à terre qui rebondit et glisse : corps fixé au sol sur sa machine, pantin sans collision chez les autres, réanimation à 26 unités | 0.28 | Écrit, pas vérifié |
| Cinématiques : un seul personnage principal, les pantins des autres joueurs cachés et sans collision | 0.28 | Écrit, pas vérifié |
| Portraits des dialogues en bas de l'écran, au-dessus des sous-titres ; bouton `Low portraits`, commande `mp_portraits` | 0.28 | Écrit, pas vérifié |
| Douze chargeurs et douze cellules par joueur (hypos à six), rangée de douze cases au-dessus des joueurs, et au-dessus du nôtre au-delà de six | 0.28 | Écrit, pas vérifié. Le moteur est modifié en huit endroits, vérifiés avant d'y toucher |
| Jeu automatique d'un client : suit l'hôte, frappe ou tire sur les ennemis proches, prend un hypo, ranime l'hôte ; bouton `Auto-play` de l'onglet Coop, commande `mp_autoplay` | 0.28 | Écrit, pas vérifié. Jamais vu à l'œuvre : distances, rythme des coups et replacement sont à régler en jeu |
| Alerte sonore : `AlertFall.mp3` joué une fois, sans boucle, quand un joueur tombe à terre, entendu de tous ; volume de 0 à 100 % par le curseur `Alert volume` de l'onglet Coop, chez l'hôte comme chez un client ; commande `mp_alertvolume` | 0.27 | Lecture testée hors jeu, volume à zéro (jouée une fois, arrêtée d'elle-même, rejouable). En jeu : écrit, pas vérifié |
| Bouton `Wallhack (ESP)` de l'onglet Display : chaque joueur coupe ou remet ce qui se voit à travers les murs (jauge, munitions, pseudo, panneau d'un joueur à terre) ; commande `mp_wallhack` | 0.27 | Écrit, pas vérifié |
| Tout ce qui se change dans le menu est gardé au lancement suivant, chez l'hôte et chez un client : `Team kill`, `God mode` et `Unlimited ammo` compris ; sections `[display_client]` et `[audio_client]` pour un client qui partage le fichier de l'hôte | 0.27 | Testé hors jeu (`onimp_configtest`). En jeu : écrit, pas vérifié. Constaté dans l'`onimp.ini` de la 0.26 : une seule clé `language` pour les deux fenêtres |
| Battement de cœur en boucle à terre ou sous 20 % de santé, coupé dès le relevé ; commande `mp_heartbeat`. **Retiré en 0.27**, remplacé par l'alerte | 0.26 | Lecture testée hors jeu, volume à zéro (ouverture 125 ms, boucle, arrêt immédiat). En jeu : écrit, pas vérifié |
| Jeu figé des clients après « Shinatama : Uploading new objective data » : plus aucune position qui ne soit un nombre n'entre dans le moteur, ne part sur le réseau ni n'en arrive ; joueur et personnages ramenés à leur dernière place saine à chaque tick et à chaque image | 0.29 | Écrit, pas vérifié en jeu. Cause lue dans la mémoire des deux clients figés de la 0.28 : position du joueur « NaN », boussole du HUD en boucle sans fin (`0x500860`), toutes les zones du niveau déclenchées d'un coup. Ce qui a produit le premier « NaN » n'est pas établi |
| Arme d'un PNJ prise par une prise (Barabas ou tout ennemi) : une seule arme, celle du joueur ; l'hôte la retire au PNJ sans la poser au sol, les autres clients à leur pantin | 0.29 | Écrit, pas vérifié. Défaut lu dans les journaux de la 0.28, à 20:18:36 : chez l'hôte `arme du pantin 1 : attendu 'w11_ba1'` puis `arme du PNJ 'Barabus' : 'w11_ba1' lachee` ; chez le client `aucune 'w11_ba1' a reprendre ici, une neuve lui est donnee` |
| Joueur à terre touché au sol : le coup se montre chez tous par l'animation « touché à terre », sans le relever, sans toucher au compte à rebours ni à la réanimation en cours ; toute autre réaction est refusée à son pantin | 0.29 | Écrit, pas vérifié |
| Réanimation : celui qu'on ranime le reste tant qu'il est à portée ; plus de saut d'un joueur à terre à l'autre à chaque coup reçu | 0.29 | Écrit, pas vérifié. Défaut lu dans le journal de l'hôte de la 0.28 : dix-sept `reanimation : du joueur N commencee` en quatre secondes, dont sept `interrompue apres 0.0 s`, entre deux joueurs à terre côte à côte |
| Jeu automatique : un joueur à terre passe avant tout, l'hôte comme un autre client | 0.29 | Écrit, pas vérifié |
| Jeu automatique : il rejoint l'hôte par où l'hôte est passé quand la ligne droite ne passe pas ; replacement en dernier recours seulement | 0.29 | Trace testée hors jeu (`onimp_trailtest` : un mur contourné sans le traverser). En jeu : écrit, pas vérifié |
| Jeu automatique : ennemi au sol frappé par un balayage, à la bonne distance | 0.29 | Écrit, pas vérifié. Coup choisi d'après les données du jeu : portée et hauteur des 84 coups de Konoko |
| Barabas (ou tout PNJ retiré par un script du niveau) ne revient plus chez les clients : pas recréé d'après des états en retard ; l'hôte sans PNJ le dit, et ce qui reste chez un client est retiré en trois secondes | 0.30 | Écrit, pas vérifié. Signalé en jeu en 0.29 : après la cinématique de sa défaite, Barabas tombait du ciel et restait immobile au sol chez les clients. Cause lue dans le journal du client : `ai2_spawn Barabus force` à l'instant de `cinematique : finie`, puis `pnj orphelin : suppression de 'Barabus'` vingt-trois secondes après |
| Jeu automatique, soin et aura : un hypo sous 85 % de santé, et en combat un de plus en pleine forme pour l'aura | 0.30 | Écrit, pas vérifié |
| Jeu automatique : va prendre une arme chargée au sol quand la sienne ne sert pas, et les munitions qui lui vont (cellules ou chargeurs) quand il lui reste moins de deux recharges | 0.30 | Écrit, pas vérifié. Type de munitions de chaque arme lu dans les données et vérifié sur les douze classes |
| Jeu automatique : armé, il tire de loin sans avancer, vise le buste, recharge, et ne tire pas quand un coéquipier est sur la ligne | 0.30 | Écrit, pas vérifié. La visée verticale est écrite dans le personnage du joueur : son effet n'a jamais été vu |
| Jeu automatique : corps à corps sans arme, et dès qu'un ennemi est à moins de 20 unités même armé | 0.30 | Écrit, pas vérifié |
| Cinématique de l'usine : les personnages gardent leur tenue (le mod ne prend plus pour notre joueur le personnage que suit le script) | 0.31 | Écrit, pas vérifié. Signalé en jeu en 0.30. Cause lue dans les journaux : `tenue : 'Recep' passe de 'generic_female_1' a 'k4_K'` chez l'hôte, `a 'k4_L'` chez le client, pendant le `chr_focus 3` du script |
| Joueur d'un client plus figé après la cinématique de l'usine : `ai2_takecontrol` suit l'hôte, dans l'ordre de ses `chr_focus` ; main rendue d'office après cinq secondes hors cinématique | 0.31 | Écrit, pas vérifié. Signalé en jeu en 0.30 : client immobile, qui tire encore. Cause lue dans l'exécutable (la commande vise le personnage que `chr_focus` désigne à cet instant) et dans les journaux (le script du client avait soixante-cinq secondes d'avance) |
| Films des gestes de Konoko, marche vers un repère : joués chez un client au moment où l'hôte les donne, pour son propre joueur seulement | 0.31 | Écrit, pas vérifié. Classement testé hors jeu (`onimp_codectest`) |
| Toutes les cinématiques passées en revue | 0.31 | Fait sur les scripts des quatorze niveaux, hors jeu : `chr_focus` dans l'usine seulement ; `ai2_takecontrol` dans sept niveaux ; 95 films sur le joueur ; cinq tests « exactement un personnage dans la zone », tous dans l'entrepôt. Aucune cinématique n'a été regardée en jeu |
| Entrepôt : ennemis et objectifs après la cinématique d'ouverture (les joueurs ne comptent que pour un dans les zones que lisent les scripts) | 0.31 | Écrit, pas vérifié. Signalé en jeu en 0.30. Cause lue dans le script : `if (trigvolume_count (63) eq 1) ai2_spawn A_t48`, deux joueurs dans la zone |
| Jeu automatique : prend l'arme la plus forte qui serve ; en pénurie, une arme plus faible ; jamais celle de Barabas, lâchée si une prise la lui donne | 0.31 | Armes ramassées : vu dans les journaux de la 0.31 à la 0.33. L'ordre de force des armes est le mien. L'interdiction de l'arme de Barabas, demandée en 0.30, a été levée en 0.34, à la demande |
| Jeu automatique : ramasse les munitions des deux sortes | 0.31 | Écrit, pas vérifié. Vu dans le journal du client de la 0.30 : il allait vers des munitions et renonçait deux secondes après, six fois, toutes dans l'usine où son personnage était figé |
| Jeu automatique : un hypo resté sans effet n'est pas redemandé avant vingt secondes | 0.31 | Écrit, pas vérifié. Vu dans le journal du client de la 0.30 : cinquante-quatre `un hypo pour l'aura` à quatre secondes d'écart, santé inchangée, toutes dans l'usine |
| Jeu automatique : ligne de vue. Il ne tire que sur un ennemi qu'il voit, ne va chercher qu'un objet qu'il voit, ne coupe tout droit vers l'hôte que s'il le voit | 0.32 | Écrit, pas vérifié. Signalé en jeu en 0.31 : il tirait à travers les murs. Rayon du moteur (celui du viseur laser) lu dans l'exécutable |
| Jeu automatique : ramasse armes, munitions de son arme et hypos ; l'objet choisi est tenu jusqu'au bout | 0.32 | En partie. Vu dans les journaux de deux essais de la 0.32 : l'objet est tenu, neuf armes ramassées, plus d'alternance. Les bonus n'étaient pas pris : signalé de nouveau en 0.32, refait en 0.33 |
| Jeu automatique : ramasse vraiment chargeurs, cellules et hypos | 0.33 | Vu dans les journaux d'un essai de la 0.33 (laboratoire, dix-neuf minutes) : trois hypos et deux chargeurs ramassés par lui, les cinq confirmés chez l'hôte. Pas de retour sur ce qui se voit à l'écran. Signalé en jeu en 0.32 ; cause lue dans l'exécutable : le moteur ne donne un bonus qu'à la touche d'échange d'arme, jamais au passage |
| Jeu automatique : munitions des deux sortes et hypos jusqu'à l'inventaire plein ; ce qui lui manque avant le tir, le plein quand aucun ennemi n'est en vue ; arme faible prise quand la forte est vide et sans recharge | 0.33 | Écrit, pas vérifié. Demandé en 0.32. Les douze classes d'armes relues dans les données du jeu |
| Pantin d'un autre joueur : ne ramasse plus de lui-même le bonus voisin de celui que ce joueur prend | 0.33 | Vu dans les journaux de l'essai de la 0.33, sur deux cas seulement : après chaque ramassage de l'hôte, plus aucun bonus annoncé par le client. Avant, lu dans ceux de la 0.31 et de la 0.32 : quatre fois, un dixième de seconde après un ramassage de l'hôte, le client annonçait un bonus d'une autre sorte, que personne n'avait reçu |
| Journal du lancement précédent gardé dans `onimp_journaux`, avec le `debugger.txt` du moteur | 0.33 | Vu dans le dossier du jeu après le premier lancement de la 0.33 : les journaux de l'essai précédent y sont, datés. Les journaux de la partie où l'hôte a planté à l'aéroport (0.32) avaient été écrasés par le lancement suivant |
| Jeu automatique : le butin avant le combat. Dès qu'il voit des munitions, des deux sortes, ou un hypo à portée, il change de route et remplit l'inventaire | 0.34 | Écrit, pas vérifié. Demandé en 0.33. Un objet qu'il n'arrive pas à prendre est laissé de plus en plus longtemps |
| Jeu automatique : arme sortie quand un ennemi est à combattre, rangée une seconde après le dernier, rangée aussitôt quand elle est vide et sans recharge | 0.34 | Écrit, pas vérifié. Demandé en 0.33. Routines de la touche d'échange d'arme lues dans l'exécutable ; sprint sans arme seulement, lu dans les animations (78 unités par seconde contre 53 et 48) |
| Jeu automatique : armé, il n'avance pas vers sa cible et recule quand elle approche ; ennemi collé à lui, corps à corps | 0.34 | Écrit, pas vérifié. Demandé en 0.33. Deux rayons derrière lui avant de reculer : ni mur, ni vide |
| Jeu automatique : ne recharge que l'arme vide, s'il porte de quoi ; puis reprend le tir | 0.34 | Écrit, pas vérifié. Demandé en 0.33. C'était déjà la seule condition ; la touche part maintenant dès que l'arme est vide, au lieu d'attendre jusqu'à une seconde et demie |
| Jeu automatique : ramasse l'arme de Barabas, la plus forte pour lui | 0.34 | Écrit, pas vérifié. Demandé en 0.33 (elle lui était interdite depuis la 0.31, à la demande d'alors). Elle ne se range pas : il la lâche quand elle est vide et sans recharge |
| Réanimation : le joueur à terre voit la barre se remplir, comme celui qui le ranime ; les autres joueurs aussi | 0.34 | Écrit, pas vérifié. Demandé en 0.33 |
| Pantin d'un client qui tire sans fin, vu de l'hôte, à la fin d'une cinématique : gâchette plus annoncée quand les touches du joueur ne comptent pas ; jeu automatique qui ne tire plus sans voir | 0.32 | Écrit, pas vérifié. Signalé en jeu en 0.31. Lu dans le journal du client : `tire sur un ennemi` dans les trois secondes qui suivent `cinematique : finie`, chargeur vidé en moins de quatre secondes, dans l'usine et à l'aéroport. Si le défaut tient aussi à l'état transmis, cela n'a pas été vu directement |
| Jeu automatique : sprint dès qu'il a du chemin à faire | 0.32 | Écrit, pas vérifié. Sprint du joueur lu dans l'exécutable : double appui sur « avancer » en moins de quinze ticks |
| Jeu automatique : ne court plus sur place devant un ennemi au sol | 0.32 | Écrit, pas vérifié. Signalé en jeu en 0.31. Cause déduite : le corps couché l'arrêtait avant la distance où il frappait |
| Jeu automatique au corps à corps seulement : plus de tir, arme rangée et jamais ressortie, ni arme ni munitions ramassées ou cherchées ; des hypos seulement | 0.35 | Écrit, pas vérifié. Demandé pour mettre fin à ses blocages. Renverse les demandes de la 0.30 à la 0.34 sur les armes ; leur code reste derrière un réglage du source |
| Onglets Cheat, Character et Spawn du menu F1, dans la colonne de droite, réservés à l'hôte et aux clients qu'il autorise | 0.35 | Écrit, pas vérifié. Place des boutons calculée d'après le gabarit du dialogue, lu hors jeu |
| Interrupteur général des cheats chez l'hôte : coupé, plus aucun cheat n'agit chez personne ; accès donné client par client, suivi menu ouvert | 0.35 | Réglages gardés d'un lancement à l'autre : testé hors jeu. Le reste : écrit, pas vérifié |
| Cheats de chaque joueur, sur son seul personnage : invincibilité et munitions illimitées (déplacées de l'onglet Coop), coup fatal, aura permanente | 0.35 | Écrit, pas vérifié |
| Curseurs de vitesse d'attaque, de vitesse de déplacement et de saut ; plus de dégâts de chute quand le saut est réglé au-dessus de 100 | 0.35 | Écrit, pas vérifié. Sites du moteur lus dans l'exécutable, octets vérifiés dans `AE\Oni.exe` |
| Incarner n'importe quelle classe de personnage à la place de Konoko, vue des autres joueurs ; liste filtrée et paginée, touche F5 réglable pour passer à la suivante | 0.35 | Écrit, pas vérifié |
| Faire apparaître devant soi objets, armes (cartouches réglables) et PNJ alliés ou ennemis ; à la demande d'un client, créés chez l'hôte | 0.35 | Écrit, pas vérifié. Pas essayé pendant la partie de la 0.35 |
| Lasers de tous les niveaux détectés par l'hôte seul, pour tous les joueurs (remplacé en 0.39, voir plus bas) ; lasers et tourelles allumés et éteints chez les clients comme chez l'hôte ; ennemis des environs prévenus du joueur détecté | 0.36 | Vu en jeu et dans les journaux de la 0.36 pour la détection du client par l'hôte et l'allumage des tourelles chez le client. Signalé en jeu en 0.35 : les défenses ne réagissaient qu'à l'hôte. Cause lue dans l'exécutable : un laser ne teste que le joueur de la partie |
| Tourelles : cible choisie par l'hôte parmi les joueurs debout, la même sur toutes les machines ; le joueur visé est touché sur sa propre machine | 0.37 | Cible : vue dans les journaux de la 0.37, chez l'hôte et chez le client. Dégâts : signalé en jeu en 0.37, toujours rien chez le client. Signalé en jeu en 0.36 (usine, Deadly Brain) : « les pièges attaquent le client mais il ne prend aucun dégât ». Première cause, lue dans la mémoire des deux jeux ouverts : chaque machine faisait viser à ses tourelles le pantin de l'autre joueur |
| Tourelles d'un client qui tirent enfin : le moteur leur interdit le tir tant que « toutes les IA » sont passives, ce qu'un client impose chez lui en permanence ; elles lisent maintenant le drapeau de l'hôte | 0.38 | **Vu en jeu** en 0.38, salle du Deadly Brain de l'usine (« fonctionne pour le client et l'hôte »), et dans les journaux : tirs sur le client chez lui, vie perdue vingt-deux fois. Signalé en jeu en 0.36 et en 0.37 : « côté client il ne se passe absolument rien ». Cause lue dans l'exécutable (`0x4CF65D`) |
| Laser coupé chez l'hôte annoncé à l'instant aux clients, qui rejouent son son et ses événements (script, alarme, portes, consoles, tourelles) ; alarme d'un laser ou d'une console suivie d'une vague | 0.38 | Rejeu vu dans les journaux de la 0.38 (501 fois en six minutes, limité depuis à quatre par seconde et par laser). Alarme d'un laser : aucun laser du jeu n'en porte |
| Lasers : chaque machine détecte aussi son propre joueur, sur les rayons qu'elle affiche, et le dit aux autres, qui lancent chez elles ce que le laser déclenche ; la détection par l'hôte reste à côté ; un joueur à terre ne déclenche plus rien | 0.39 | Écrit, pas vérifié. Signalé en jeu en 0.38 : « dans d'autres salles... quand le client passe sur les lasers rien ne marche ». Cause **déduite**, pas observée, de l'exécutable et des données des niveaux : 288 lasers sur 291 glissent sur un rail, chaque machine les fait avancer pour son compte, et l'hôte testait le pantin du client contre des rayons que le client ne voyait pas là dès qu'il avait rechargé seul. Pas vérifié dans la mémoire des deux jeux : ils étaient fermés |
| Lasers mobiles : rayons d'un client recalés sur ceux de l'hôte, toutes les deux secondes et à son arrivée | 0.39 | Écrit, pas vérifié. Écart entre deux rayons testé hors jeu |
| Tourelles : cible = ceux qui viennent de couper le laser, puis le plus proche de ceux qu'elle peut toucher, la cible du moment gardée entre deux joueurs à même distance ; chez le client qui coupe le laser, sa tourelle le vise sans attendre l'hôte | 0.39 | **Vu en jeu dans le premier jet** (partie de 14:54 à 15:09, usine, couloir des lasers 2 à 4) : la tourelle vise le client qui coupe son laser, tire chez lui, et il perd de la vie ; tourelles 1, 2 et 3. Lu dans les journaux de la 0.38 : tourelle 1 quinze secondes sur l'hôte sans un tir. La règle livrée a été réécrite depuis (plusieurs joueurs dans les mêmes rayons) : testée hors jeu, pas rejouée |
| Les 291 lasers et 64 tourelles des quatorze niveaux relevés hors jeu : 277 lasers allument une tourelle, 6 lancent un script (gaz des hangars), aucun ne porte alarme, porte ou console ; 288 sont mobiles, 165 allumés dès le chargement ; durée et champ de chaque classe de tourelle | 0.39 | Fait, hors jeu |
| Alarmes : une vague de trois ennemis du niveau par joueur présent, sur `ai2_tripalarm` et sur une console dont un ennemi se sert | 0.36 | Taille et noms des vagues testés hors jeu. Le reste : écrit, pas vérifié |
| Client qui charge de lui-même le niveau de l'hôte quand il se trouve dans un autre | 0.36 | Écrit, pas vérifié. Le changement de niveau ordinaire et la reprise dans un même niveau, eux, sont vus dans les journaux de la 0.35 |

## À faire

Par ordre de priorité.

### 1. Valider la version 0.39

Protocole 25 : tous les joueurs en 0.39, une version antérieure est refusée. Première ligne des
journaux : `OniMP 0.39` ; aucune ligne `inattendu`. Les sources de la 0.38 sont dans
`Projet Oni PS2\_sauvegarde_onimp_0.38`, sa DLL dans `OniMP\build\realbink_0.38_backup.dll` ; les
journaux de son essai dans `AE\onimp_host_0.38_usine.log` et `onimp_client_0.38_usine.log`. Un
premier jet de la 0.39 (cible des tourelles seule, détection chez l'hôte seul, protocole 24) est
dans `_sauvegarde_onimp_0.39_premier_jet` ; tu l'as joué de 14:54 à 15:09, journaux gardés dans
`AE\onimp_host_0.39_premier_jet.log` et `onimp_client_0.39_premier_jet.log` : dans le couloir des
lasers 2 à 4 les tourelles visent le client qui coupe le laser, tirent chez lui et le blessent.

**Ce que la 0.39 livrée ajoute au premier jet.** Le jeu du client détecte aussi lui-même son
joueur, sur les rayons qu'il affiche, et prévient l'hôte ; les rayons du client sont recalés sur
ceux de l'hôte ; deux joueurs dans les mêmes rayons ne font plus hésiter la tourelle. La détection
par l'hôte, celle que tu as vue marcher, reste en place. Ces ajouts n'ont pas été vus en jeu.

**L'essai qui compte** : là où « rien ne marche » en 0.38. Le client coupe un laser, l'hôte où il
veut, loin de préférence. Dans l'usine : les lasers 2 (tourelle 1, long couloir), 3 (tourelles 2
et 3) et 4, avant la salle du Deadly Brain. Faire l'essai **après que le client a rechargé seul**
(rechargement depuis son menu, ou partie rejointe en cours) : c'est le cas où ses rayons ne sont
plus ceux de l'hôte. Journal du client : `defenses : rayons de N laser(s) recales sur ceux de
l'hote, ecart jusqu'a X image(s) de rail` dit de combien ils s'étaient écartés.

- le laser doit réagir **quand le client traverse le rayon qu'il voit**, et seulement alors : son
  du laser chez lui à l'instant, message `OniMP : laser 2 : <pseudo> detecte` chez les deux ;
- la tourelle doit se tourner vers **le client** et lui tirer dessus, même si l'hôte est plus près
  d'elle, et le blesser ;
- cinq secondes plus tard, ou si le client se met à l'abri, elle prend le joueur le plus proche
  **qu'elle voit**. Dire si elle vise encore un joueur caché derrière un mur alors que l'autre est
  en vue, ou si elle passe sans cesse de l'un à l'autre ;
- regarder les deux écrans : les rayons doivent être **au même endroit** chez l'hôte et chez le
  client, deux secondes au plus après l'arrivée du client. Dire s'ils sautillent.

Les lignes de journal qui prouvent chaque maillon sont dans le README, « Lasers, tourelles,
alarmes », tableau « Journal : une ligne par maillon » : la première qui manque dit où cela casse.
`mp_status` résume chez chacun : `lasers coupes : N ici, N chez les autres`, `rayons : N envoi(s),
N recale(s)`, `tir libre`, `N salve(s) ici`, `notre joueur touche N fois`.

**La salle du Deadly Brain** (usine, point de sauvegarde 4) marchait en 0.38 : vérifier qu'elle
marche toujours, pour le client comme pour l'hôte.

**Les autres niveaux** : mêmes pièges partout d'après les données (un laser allume une tourelle).
Les plus garnis : le QG du TCTF (47 lasers, 13 tourelles) et sa seconde visite (93 lasers, 17
tourelles), où presque tous les lasers sont allumés dès le chargement. Dans les hangars de
l'aéroport les lasers lancent le gaz (script `gas`) : dire si le client voit le gaz et la porte se
fermer quand c'est lui qui coupe un laser, et qui le gaz blesse.

#### Validé de la 0.38

Vu en jeu par toi : la salle du Deadly Brain de l'usine marche pour le client et pour l'hôte. Dans
les journaux : tirs des tourelles sur le client chez lui, vie perdue, laser rejoué, tourelles
muettes le temps d'une cinématique de l'hôte. Les sources de la 0.37 sont dans
`Projet Oni PS2\_sauvegarde_onimp_0.37`, sa DLL dans `OniMP\build\realbink_0.37_backup.dll` ; les
journaux de son essai dans `AE\onimp_host_0.37_usine.log` et `onimp_client_0.37_usine.log`.

**Toujours le même essai** : usine, point de sauvegarde 4, salle du Deadly Brain, le client coupe
un laser. Ce que la 0.38 doit changer, et la ligne de journal qui le prouve chez le **client**
(`AE\onimp_client.log`) :

1. le laser réagit chez lui : son du laser, message `OniMP : laser 202 : <pseudo> detecte`.
   Journal : `defenses : laser 202 (rang N) coupe chez l'hote par le joueur 1 : son joue, ses
   evenements rejoues ici` ;
2. les tourelles lui tirent dessus. Journal : `defenses : tourelle 20 (rang N) tire ici, sur notre
   joueur` ;
3. il est blessé. Journal : `defenses : notre joueur perd de la vie sous le tir d'une tourelle (N,
   il en avait M)`.

Si la ligne 2 manque alors que `vise le joueur 1, d'apres l'hote : c'est le notre` y est : le tir
est encore retenu, regarder s'il y a `defenses : chez l'hote les IA sont tenues passives` (une
cinématique de l'hôte en cours) ou, au lancement, `tourelles : code inattendu`. Si la ligne 2 y
est et pas la 3 : les tirs partent et ne touchent pas. `mp_status` chez le client résume : `tir
libre`, `N salve(s) ici`, `N fois touche`, `N laser(s) rejoue(s)`.

Le reste de la liste de la 0.37, inchangé :

- le **client** coupe un laser, l'hôte restant loin des tourelles : chez le client, les tourelles
  doivent maintenant lui tirer dessus et le blesser ; chez l'hôte, on les voit tirer sur lui.
  Journal du client : `defenses : tourelle 20 (rang N) vise le joueur 1, d'apres l'hote : c'est le
  notre` ;
- l'**hôte** s'approche plus près d'une tourelle que le client : elle se tourne vers lui, chez les
  deux, et c'est lui qui encaisse ;
- un joueur **à terre** n'est plus visé : la tourelle passe à l'autre joueur, ou s'arrête ;
- dire si une tourelle tire encore dans le vide chez l'un, si elle hésite d'un joueur à l'autre,
  ou si le client est blessé par une tourelle qui, à l'écran, vise ailleurs.

Le reste de la 0.36 est à valider tel quel, ci-dessous : seule la détection par les lasers et
l'allumage des tourelles ont été vus.

#### Reste à valider de la 0.36

Les sources de la 0.35 sont dans `Projet Oni PS2\_sauvegarde_onimp_0.35`, sa DLL dans
`OniMP\build\realbink_0.35_backup.dll`.

**Lasers et tourelles**, avec `npcs` ou `quests` (les scripts `_coop` et `_radmin`) :

- le **client** traverse un laser, l'hôte restant à l'écart : `OniMP : laser N : <pseudo du client>
  detecte` chez les deux, et ce que le laser déclenche se passe chez les deux (tourelles qui
  s'allument, ennemis qui arrivent). Journal de l'hôte : `defenses : laser N coupe par mp_remote1
  (joueur 1)` ;
- l'**hôte** traverse le même laser : le client doit voir les tourelles s'allumer chez lui
  (`defenses : tourelle N allume d'apres l'hote` dans son journal), et se faire tirer dessus s'il
  est à portée. Dire si une tourelle tire chez l'un et pas chez l'autre, ou ne vise jamais le
  client ;
- **salle du Deadly Brain** (`tctf_ii`) : chaque console éteinte, par l'hôte ou par un client,
  doit éteindre ses lasers et ses tourelles chez tous ;
- usine (`manplant`), là où tu as joué la 0.35 : ses lasers sont les plus nombreux après ceux du
  Deadly Brain. `mp_status` : ligne `defenses : N laser(s), N tourelle(s)...`.

**Alarmes** : laisser un ennemi atteindre une console d'alarme (entrepôt : six appels
`ai2_doalarm` dans son script). Attendu : `OniMP : alarme : 6 ennemi(s) en renfort (2 joueur(s))`,
et dans le journal de l'hôte `vague : alarme (console N, par '...') : 6 ennemi(s) en renfort pour
2 joueur(s), depuis 4 endroit(s) du niveau`. Dire d'où ils arrivent (sous les yeux ?), s'ils
rejoignent bien les joueurs ou restent plantés, si le client les voit aux mêmes endroits
(`vague : '..._w00' cree d'apres l'hote`), et si trois par joueur est trop ou trop peu.

**Niveau de l'hôte** : en partie, l'hôte ouvre son menu (Échap), `Load Game`, et charge la
sauvegarde d'un **autre** niveau. Attendu chez le client, deux à trois secondes après :
`OniMP : l'hote a charge un autre niveau : nous le suivons`, puis le chargement, puis le départ
commun. Autre essai : lancer le client dans le niveau 1 pendant que l'hôte est dans l'usine.

#### Reste à valider de la 0.35

Vu dans les journaux de la partie du 13 octobre (entrepôt puis usine, quarante-sept minutes,
gardés dans `AE\onimp_host_0.35_usine.log` et `onimp_client_0.35_usine.log`) : aucune `EXCEPTION`,
aucun `inattendu` ; les cinq onglets acceptés par le moteur ; accès donné et retiré au client dans
l'instant ; interrupteur général ; aura permanente ; une soixantaine de personnages incarnés à la
suite, redits au client, sans refus, et gardés d'un niveau à l'autre ; jeu automatique au corps à
corps seulement (90 lignes de coups, sept hypos, aucune arme) ; passage de l'entrepôt à l'usine et
reprise commune au point 4. Pas essayé, ou sans trace dans un journal : l'onglet `Spawn`, `God
mode`, `Unlimited ammo`, `One-hit kill`, les trois curseurs. Le jeu automatique s'est encore
bloqué cinq fois en suivant l'hôte (`replace a cote de l'hote (bloque en chemin)`). Reste donc, de
la liste ci-dessous, tout ce qui se juge à l'écran.

Les sources de la 0.34 sont dans `Projet Oni PS2\_sauvegarde_onimp_0.34`, sa DLL dans
`OniMP\build\realbink_0.34_backup.dll`.

**D'abord, que rien n'est cassé** : lancer hôte et client comme d'habitude, jouer deux minutes
sans toucher aux nouveaux onglets. Les deux nouveaux points d'accroche du moteur tournent à chaque
tick pour chaque personnage : si le jeu plante au chargement d'un niveau ou au premier pas, c'est
là qu'il faut regarder (`EXCEPTION` dans le journal).

**Menu F1 chez l'hôte** : trois onglets à droite, sous `Close` : `Cheat`, `Character`, `Spawn`.
Dire s'ils sont à leur place, si des boutons se chevauchent ou sortent du cadre, si un titre est
coupé. L'onglet `Coop` n'a plus `God mode` ni `Unlimited ammo`.

**Menu F1 chez le client** : les trois onglets n'y sont pas. L'hôte, onglet `Cheat`, clique
`P1 : blocked` : ils paraissent chez le client, même si son menu est ouvert (`cheats : l'hote nous
ouvre les onglets...` dans son journal). Un second clic les retire.

**Onglet Cheat**, hôte puis client autorisé, avec `Cheats : ON` chez l'hôte :

- `God mode` : plus de vie perdue. Chez l'hôte seulement s'il est seul à l'avoir mis : le client
  doit encore prendre des coups ;
- `Unlimited ammo` : l'arme ne se vide plus ;
- `One-hit kill` : un coup de poing tue un Striker ; un tir aussi. Vérifier qu'un civil et un allié
  ne meurent pas d'un coup ;
- `Permanent aura` : la lueur autour de Konoko, vue aussi de l'autre joueur, sans hypo ;
- `Attack speed` à 200 % : les coups partent deux fois plus vite. Dire si certains ne touchent
  plus, ou si le son manque ;
- `Move speed` à 200 % : course deux fois plus rapide. Dire si le personnage traverse un mur, une
  porte fermée, ou tombe à travers le sol dans un escalier ;
- `Jump` à 300 % : saut bien plus haut, et aucun dégât en retombant, même de très haut ;
- **`Cheats : OFF` chez l'hôte** : tout cela s'arrête à l'instant, chez l'hôte comme chez le
  client, et revient tel quel quand il remet `ON`.

**Onglet Character** : choisir `striker_easy_1` (filtre `Syndicate`). Le corps change sur place.
Dire si la santé a changé, si les coups et les prises marchent, si la caméra est à la bonne
hauteur, et ce que voit l'autre joueur. Touche F5 en jeu : classe suivante, puis Konoko après la
dernière. Un clic sur `Character : ...` rend Konoko. Passer un niveau, relancer le jeu : le
personnage doit être repris.

**Onglet Spawn** :

- `Items` : un hypo, un chargeur, un bouclier devant soi ; les ramasser ;
- `Weapons` : un fusil à plasma à `Ammo : 50 %` ; le ramasser, compter les tirs. Chez un client
  avec PNJ partagés, l'arme doit paraître chez l'hôte et chez lui ;
- `NPCs`, `Team : Enemy` : un Striker apparaît face au joueur et attaque. `Team : Ally` : il se bat
  contre les ennemis du niveau, pas contre nous. Dire s'il reste planté, s'il attaque le mauvais
  camp, si le client le voit au même endroit que l'hôte.

**Jeu automatique** (client, onglet Coop, `Auto-play`) : il ne tire plus et ne sort plus son arme
(`jeu automatique : range 'w2_sap' (il se bat a mains nues)` une fois, puis plus rien sur les
armes), il ne va plus vers les armes ni les munitions, il frappe l'ennemi qu'il voit. Dire s'il se
bloque encore, et où : la ligne du journal qui se répète au moment du blocage dit pourquoi.
`mp_status` : `en marche (corps a corps seulement)`.

#### Reste à valider de la 0.34

Les journaux de l'essai de la 0.33 sont gardés dans `AE` :
`onimp_host_0.33_lab.log`, `onimp_client_0.33_lab.log` (laboratoire, dix-neuf minutes) ; ceux
des lancements précédents sont dans `AE\onimp_journaux`.

Jeu automatique (client, F1, onglet Coop, `Auto-play`), à regarder sur l'écran du client :

- **l'arme** : rangée quand il suit l'hôte sans ennemi autour (`jeu automatique : range 'w2_sap'
  (plus d'ennemi en vue)`), et il sprinte ; sortie dès qu'un ennemi se montre (`sort 'w2_sap'
  (ennemi a combattre)`). Dire s'il la range et la ressort sans cesse, ou s'il tarde trop à tirer
  après l'avoir sortie ;
- **arme vide, plus de recharge** : il la range aussitôt (`range 'w2_sap' (vide et sans
  recharge)`) et se bat à mains nues ; il la ressort après avoir ramassé une recharge ;
- **rechargement** : seulement l'arme vide (`'w2_sap' vide, il recharge (N recharge(s) dans
  l'inventaire)`), jamais avant ; puis il reprend le tir ;
- **le recul** : armé, il ne court plus vers sa cible ; quand elle approche à moins de 50 unités
  il recule en tirant, et la frappe quand elle le serre. Dire s'il recule dans un mur, dans le
  vide, ou s'il reste planté sans tirer (`recul bloque, il tient sa place deux secondes`) ;
- **le butin d'abord** : en plein combat, il part chercher un chargeur, une cellule ou un hypo
  qu'il voit, puis revient. Dire si cela le fait tuer, ou s'il s'acharne sur un objet qu'il ne
  peut pas atteindre (`renonce a ce qu'il allait chercher (...), N secondes`, la durée triple à
  chaque échec au même endroit) ;
- **l'arme de Barabas** : au sol, il va la prendre (`une arme en vue a N unites : w11_ba1, il y
  va`). Elle ne se range pas et il ne sprinte plus avec ; vide et sans cellule, il la lâche ;
- `mp_status` chez le client : le jeu automatique y tient maintenant deux lignes ; la seconde
  compte les ticks de tir et de recul, les rechargements, et les fois où l'arme a été rangée et
  sortie.

Réanimation, sans jeu automatique : le joueur à terre voit, au-dessus de son personnage, la barre
bleu clair se remplir pendant les trois secondes où on le ranime, comme la voit celui qui tient
CTRL ; elle se vide de nouveau si la réanimation est interrompue. À trois joueurs, le troisième la
voit aussi.

**Hôte planté à l'aéroport** (signalé en 0.32, « après un déclencheur de quête ») : cause toujours
inconnue, pas reproduit depuis. Les journaux de cette partie avaient été écrasés ; depuis la 0.33
ils sont gardés dans `onimp_journaux`, avec le `debugger.txt` du moteur. S'il se reproduit : noter
ce qui venait de se passer à l'écran (console activée, porte, cinématique), et, si le jeu est figé
plutôt que fermé, le laisser ouvert : sa pile se lit dans le processus.

#### Reste à valider de la 0.33

Vu dans les journaux de l'essai de la 0.33 : le jeu automatique ramasse les bonus (trois hypos,
deux chargeurs, les cinq confirmés chez l'hôte, deux hypos pris aussitôt) ; plus de bonus fantôme
après les deux ramassages de l'hôte ; journaux du lancement précédent gardés. Un seul refus du
moteur, pour une recharge tout juste tombée, que l'hôte déplaçait encore : corrigé en 0.34. Reste,
à l'écran du client :

- **un bonus au sol** : il y va et le prend. Chez le client, `jeu automatique : chargeur ramasse ;
  il porte N chargeur(s), N cellule(s), N hypo(s)` (ou `cellule d'energie ramassee`, `hypo
  ramasse`) ; chez l'hôte, `bonus de type 0 ramasse par le joueur 1` (1 : cellule, 2 : hypo). Plus
  aucune ligne `dessus sans le prendre`. Une ligne `le moteur refuse de le lui donner` hors
  inventaire plein serait un défaut : garder le journal ;
- il ramasse ce qui traîne autour de lui, chargeurs, cellules et hypos, jusqu'à douze, douze et
  six (depuis la 0.34, pendant un combat aussi). Dire s'il s'éloigne trop de l'hôte, ou s'il
  laisse quelque chose ;
- **arme vide et sans recharge** : il prend une autre arme du sol, même plus faible ; il reprend
  la forte quand il a retrouvé de quoi la recharger ;
- il se soigne : `jeu automatique : sante a N sur M, un hypo`. Dans l'essai de la 0.32 il est mort
  cinq fois dans l'entrepôt, sans avoir pu prendre un seul des six hypos qu'il visait.

Bonus, à deux joueurs sans jeu automatique : l'hôte ramasse un bonus posé à côté d'un autre (un
ennemi mort en laisse souvent deux). Le second doit rester au sol chez les deux, et le journal de
l'hôte ne doit pas montrer, un dixième de seconde après son `bonus de type N ramasse : annonce`,
une ligne `bonus de type M ramasse par le joueur 1`.

Journaux : après le deuxième lancement, le dossier `AE\onimp_journaux` contient le journal du
premier, et le nouveau journal le dit (`journal : celui du lancement precedent est garde, ...`).

Ce qui a été cherché sur l'hôte planté à l'aéroport, sans rien trouver : le journal d'événements
de Windows ne contient rien pour `Oni.exe` à cette heure-là ; les journaux de la 0.31 dans ce
niveau (quatre chargements, jusqu'au premier point de sauvegarde) ne montrent ni `EXCEPTION` ni
arrêt anormal ; les scripts du niveau n'emploient ni `trigvolume_count` ni `chr_focus`.

#### Reste à valider de la 0.32

Jeu automatique, à regarder sur l'écran du client et sur celui de l'hôte :

- **un ennemi derrière un mur** : il ne tire pas ; il tire dès que l'ennemi est en vue. Dire s'il
  tire encore dans le décor, et où ;
- **fin d'une cinématique**, vu de l'hôte : le personnage du client ne tire plus dans le vide. S'il
  tire encore, dire si le jeu automatique était en marche chez le client, et garder les deux
  journaux : c'est ce qui dira si la cause est le jeu automatique ou l'état transmis ;
- **au sol** : vu dans les journaux de la 0.32, les armes sont prises (`jeu automatique : une arme
  en vue a N unites : w2_sap, il y va`, puis `arme 'w2_sap' ramassee`) et l'alternance `va chercher
  des munitions` / `rejoint l'hote` a disparu ; les bonus sont l'affaire de la 0.33 ;
- **sprint** : il sprinte pour rejoindre l'hôte, pour aller à un objet, à un joueur à terre. Dire
  s'il dépasse son but, s'il sprinte dans les murs, si cela gêne ses coups ;
- **ennemi au sol** : il s'arrête devant et balaie, au lieu de courir sur place ;
- `mp_status` chez le client : la ligne du jeu automatique compte les rayons tirés et les objets
  cachés.

Vu dans les journaux de l'essai de la 0.31 (quarante minutes, entrepôt depuis sa cinématique
d'ouverture, usine, aéroport) : l'entrepôt démarre (`zones : zone 63, 2 personnage(s) dont 1
joueur(s) en plus du premier : comptes pour un, soit 1`, puis l'ennemi `A_t48`, et de même la zone
80 plus loin) ; dans l'usine `cinematique : le script suit 'Recep' (chr_focus) ; notre joueur reste
'konoko'`, plus aucune ligne `tenue : 'Recep'`, et le client se bat et se déplace après la
cinématique sans que la ligne `aux mains de l'IA de deplacement` apparaisse ; l'hypo sans effet
n'est demandé qu'une fois. Aucune ligne `NAN :`, `inattendu` ni `EXCEPTION`.

#### Reste à valider de la 0.31

Protocole 20 inchangé ; tous les joueurs doivent avoir la 0.31. Première ligne des journaux :
`OniMP 0.31` ; la ligne `quetes : 321 commandes de script interceptees (111 d'etat, 34
ponctuelles)` en compte une d'état et quatre ponctuelles de plus qu'en 0.30, celles des
cinématiques. Aucune ligne `inattendu`. Les journaux de l'essai de la 0.30 sont gardés dans `AE` :
`onimp_host_0.30_cinematiques.log`, `onimp_client_0.30_cinematiques.log`.

Usine (`manplant`), depuis le début du niveau, à deux :

- pendant la cinématique : la réceptionniste garde sa tenue, chez l'hôte comme chez le client ;
  plus de ligne `tenue : 'Recep' passe de ...` ; à la place `cinematique : le script suit 'Recep'
  (chr_focus) ; notre joueur reste 'konoko'` ;
- côté client, Konoko entre dans le bâtiment en même temps que les policiers, comme chez l'hôte ;
- à la fin : le joueur du client bouge, à la main comme en jeu automatique ;
- si la ligne `cinematique : notre joueur etait reste aux mains de l'IA de deplacement` apparaît,
  me le dire : c'est le filet, et il ne devrait pas servir.

Entrepôt (`EnvWarehouse`), nouvelle partie, donc avec sa cinématique d'ouverture :

- après l'écran de consignes : le premier ennemi apparaît (`spawn` ou `pnj pilote : 'A_t48'`), puis
  la suite du niveau : porte déverrouillée à sa mort, objectif, boussole ;
- journal de l'hôte : `zones : zone 63, 2 personnage(s) dont 1 joueur(s) en plus du premier :
  comptes pour un, soit 1`.

Les autres cinématiques n'ont été relues que dans les scripts : **dire laquelle va encore mal**
(niveau, moment, ce qui se voit), et garder les deux journaux.

Jeu automatique :

- deux armes au sol : il va à la plus forte ; arme vide et sans recharge : il en prend une plus
  faible plutôt que de rester sans ;
- l'arme de Barabas au sol : sans objet depuis la 0.34, où il la ramasse (voir « Valider la
  version 0.34 ») ;
- munitions au sol, cellules comme chargeurs : il va les prendre (`va chercher des munitions`, puis
  chez l'hôte `bonus de type 0 ramasse par le joueur 1`, ou `type 1` pour une cellule) ;
- plus de ligne `un hypo pour l'aura` toutes les quatre secondes.

#### Reste à valider de la 0.30

Protocole 20, comme la 0.29 ; tous les joueurs doivent tout de même avoir la 0.30, les correctifs
étant de chaque côté. Première ligne des journaux : `OniMP 0.30`.

Barabas, niveau `lab`, à deux ou trois :

- le battre et regarder la cinématique côté client : à la fin il s'envole et **ne revient pas** ;
  rien ne tombe du ciel, personne d'immobile au sol ;
- journal du client : plus de `bsl : ai2_spawn Barabus force` après `cinematique : finie`, plus de
  `pnj orphelin : suppression de 'Barabus'` ;
- si un Barabas reste tout de même, me donner les lignes du journal du client entre `cinematique :
  commence` et une demi-minute après `cinematique : finie`.

Jeu automatique (client, F1, onglet Coop, `Auto-play`), à regarder sur l'écran du client :

- une arme chargée traîne au sol, le bot n'en a pas : il va la prendre (`jeu automatique : va
  chercher une arme`, puis `arme '...' ramassee (N cartouche(s))`), et chez l'hôte son personnage
  l'a en main ;
- armé, un ennemi à distance : il s'arrête, se tourne et tire (`tire sur un ennemi`). **Dire s'il
  touche**, s'il tire trop haut, trop bas, dans un mur, ou sur un coéquipier ;
- l'ennemi arrive au contact : il passe aux coups (`frappe un ennemi`), arme en main ;
- arme vide : il recharge, puis va chercher des munitions (`va chercher des munitions`) ;
- en combat, en pleine forme, avec un hypo : il le prend et l'aura s'allume (`un hypo pour
  l'aura`). Dire si c'est trop d'hypos dépensés : un seul seuil à changer ;
- un joueur à terre passe toujours avant tout le reste.

Vu dans le journal d'un client de l'essai de la 0.29 (vingt-sept minutes, `EnvWarehouse` puis
`lab`, deux clients en jeu automatique) : **aucun jeu figé**, aucune `EXCEPTION`, et aucune ligne de la
garde des positions (préfixe `garde :` en 0.29, `NAN :` depuis la 0.30 : l'ancien se confondait à la
recherche avec `sauvegarde :`), donc pas de position invalide cette fois ; un désarmement dans chaque sens (`desarmement : notre
joueur prend 'w2_sap' a 'Floor1_Striker_1_x3', annonce`, et `le joueur 1 a pris 'w2_sap' a
'Floor1_Striker_1_x2' : retiree ici aussi`) ; un coup reçu au sol montré (`a terre : coup recu au
sol, reaction 'KONCOMhit_fallen1' jouee ; compte a rebours inchange (4.4 s)`) : le moteur choisit
bien une animation de personnage couché ; le jeu automatique a ranimé quinze fois (`menee au
bout`), suivi la trace de l'hôte (`rejoint l'hote par ou il est passe`), balayé des ennemis au sol,
buté quinze fois et n'a été replacé que deux fois. Le journal de l'hôte de cet essai a été réécrit
par le lancement suivant : ce que l'hôte a fait de l'arme des PNJ désarmés n'a pas été relu.

#### Reste à valider de la 0.29

Protocole 20 : **tous les joueurs doivent avoir la 0.29** (une version plus ancienne est refusée à
la connexion). Dans tous les journaux : première ligne `OniMP 0.29`, aucune ligne `inattendu` ni
`EXCEPTION`. Les journaux de la partie figée de la 0.28 sont gardés dans `AE` :
`onimp_host_0.28_fige.log`, `onimp_client_0.28_fige.log`, `onimp_client_2_0.28_fige.log`.

Jeu figé, le plus important. Refaire la même partie : niveau `lab`, deux clients en jeu
automatique, mode survie, tir ami, l'hôte au projecteur de phase.

- plus de fenêtre `Client - Oni (Ne répond pas)` ;
- **chercher dans les trois journaux les lignes qui commencent par `NAN :`**. S'il n'y en a
  aucune, la cause ne s'est pas représentée. S'il y en a, la partie a continué au lieu de se figer,
  et la première dit qui a été touché, quand et dans quelle animation : me donner cette ligne et
  les vingt qui la précèdent, c'est ce qui manque pour trouver la cause ;
- plus de `sauvegarde : point 3 atteint` quelques secondes après une reprise au point 2.

Arme de Barabas, et de tout ennemi armé :

- le désarmer par une prise, côté client puis côté hôte : **une seule arme**, dans les mains du
  joueur ; le PNJ n'en a plus, et rien ne tombe au sol ;
- journal de celui qui désarme : `desarmement : notre joueur prend 'w11_ba1' a 'Barabus', annonce` ;
  journal de l'hôte : `desarmement : le joueur 1 a pris 'w11_ba1' a 'Barabus' : retiree au PNJ, une
  seule de chaque` ; chez un autre client : `... : retiree ici aussi` ;
- plus de `aucune 'w11_ba1' a reprendre ici, une neuve lui est donnee` dans la seconde qui suit ;
- si l'arme est encore en double, me donner les lignes `desarmement`, `arme du PNJ` et `arme du
  pantin` des trois journaux autour de ce moment.

Joueur à terre :

- un ennemi le frappe au sol : il tressaille sans se relever, chez lui comme chez les autres ; le
  compte à rebours continue, la réanimation en cours aussi (`a terre : coup recu au sol, reaction
  '...' jouee ; compte a rebours inchange`). **Dire ce qui se voit** : je ne sais pas quelle
  animation le moteur choisit pour un personnage couché, ni s'il en choisit une ;
- deux joueurs à terre côte à côte, CTRL tenu : la réanimation reste sur le même joueur jusqu'au
  bout ; le journal de l'hôte n'aligne plus les `commencee` / `interrompue apres 0.0 s`.

Jeu automatique (client, F1, onglet Coop, `Auto-play`) :

- un joueur tombe à terre : le bot lâche son combat et va le ranimer (`jeu automatique : va ranimer
  un joueur a terre`, puis `ranime un joueur a terre`) ;
- l'hôte passe une porte, prend un couloir, monte un étage : le bot arrive par le même chemin
  (`rejoint l'hote par ou il est passe`) au lieu de buter contre le mur ; un `replace a cote de
  l'hote` ne doit venir qu'en dernier recours, après sept secondes sans avancer ;
- un ennemi tombe : le bot s'approche et le balaie (`frappe un ennemi au sol`). Dire si le coup
  porte, s'il est trop loin, trop près, à côté ;
- **dire ce qu'il fait encore mal**, et où (quelle salle, quel obstacle) : distances et délais sont
  des réglages.

#### Reste à valider de la 0.28 et des versions précédentes

Les lignes de journal à chercher sont entre parenthèses. Les journaux sont dans le dossier du jeu,
`AE` (voir le README, « Quel dossier de jeu »). D'abord, dans tous : première ligne `OniMP 0.28`,
la ligne `historique des commandes : debordement du moteur corrige`, et aucune ligne `inattendu`
ni `EXCEPTION`. Le protocole n'a pas changé depuis la 0.25, mais tous les joueurs doivent avoir la
0.28 : les correctifs sont de chaque côté, et l'inventaire à douze places doit être le même
partout (ligne `inventaire : douze chargeurs et douze cellules au lieu de six`).

**Après un défaut, les journaux ne se perdent plus** : depuis la 0.33, `onimp_host.log` et
`onimp_client.log` sont déplacés au lancement suivant dans `AE\onimp_journaux`, sous un nom daté.
Jusqu'à la 0.32 ils étaient réécrits à chaque lancement.

Nouveautés de la 0.28 : trois défauts, deux demandes sur les cinématiques et deux ajouts signalés
après l'essai de la 0.27. Rien n'en a été vu en jeu. À deux, `run_host_radmin.bat` /
`run_client_radmin.bat`.

Barabas en double, niveau `lab`, au point de sauvegarde d'avant le combat :

- perdre et reprendre plusieurs fois : **un seul Barabas** chez le client à chaque reprise ;
- journal de l'hôte : `quetes : niveau recharge, journal des 16 commande(s) d'etat vide`, et
  `quetes : 16 commande(s) d'etat rejouee(s) au client 1` à chaque reprise, le même nombre, plus
  32, 48, 64 ;
- journal du client : plus aucune ligne `pnj orphelin : suppression de 'Barabus'` ; s'il en reste
  une, me la donner avec ce qui la précède.

PNJ vus de loin :

- le client s'éloigne de l'hôte (une autre salle, un autre étage) et regarde des ennemis : ils
  marchent au sol et s'animent normalement, sans flotter ni trembler ; ils l'attaquent ;
- journal de l'hôte : `pnj : N garde(s) actif(s) pres d'un autre joueur, loin du notre` ;
- dire si le défaut reste, et **où se trouvait l'hôte** par rapport au client et aux PNJ à ce
  moment : c'est ce qui dira si la cause est la bonne.

Joueur à terre :

- tomber sous les coups de plusieurs ennemis : le corps se pose une fois et ne bouge plus, même
  si un personnage marche dessus (`a terre : corps pose en (...), il n'en bouge plus`) ; vu de
  l'autre joueur, il ne tressaute plus quand on s'en approche ;
- le ranimer est possible d'un peu plus loin qu'avant.

Cinématiques, à deux, sur un niveau qui en a (début de `manplant` ou de `lab`) :

- chez l'hôte comme chez le client : un seul personnage principal à l'écran, pas de second
  Konoko ; les deux journaux : `cinematique : commence, 1 autre(s) joueur(s) caches`, puis
  `finie, 1 autre(s) joueur(s) remontres` ;
- à la fin l'autre joueur réapparaît, avec sa jauge ;
- les portraits de ceux qui parlent entrent **en bas**, à gauche ou à droite, au-dessus des
  sous-titres : dire s'ils les recouvrent, s'ils sont trop bas ou trop hauts ;
- F1, onglet Display, `Low portraits` sur `OFF` : ils reviennent en haut.

Munitions :

- ramasser plus de six chargeurs, plus de six cellules : le jeu les prend jusqu'à douze, puis les
  laisse au sol ; un septième hypo reste au sol comme avant ;
- l'affichage du jeu, en bas de l'écran, n'en montre que six : au-delà, une rangée de douze cases
  apparaît au-dessus de notre propre personnage ; les autres joueurs ont toujours la leur ;
- dire si la rangée de douze cases est lisible, ou trop petite.

Jeu automatique, côté client, F1, onglet Coop, `Auto-play` sur `ON` :

- le personnage du client suit l'hôte, s'arrête près de lui, le rejoint quand il s'éloigne ;
- devant un ennemi : il se tourne vers lui et le frappe ; avec une arme chargée, il tire ;
- bloqué derrière un obstacle : il saute, puis réapparaît à côté de l'hôte (`jeu automatique :
  notre joueur replace a cote de l'hote (bloque en chemin)`) ;
- chez l'hôte, le bouton affiche `(client)` et ne s'enclenche pas ;
- **dire tout ce qu'il fait mal** : il tourne en rond, frappe dans le vide, se replace trop
  souvent, reste collé à l'hôte... Tout se règle, mais je ne l'ai jamais vu jouer.

Vu dans les journaux de l'essai de la 0.27 (une heure et demie, `EnvWarehouse`, `manplant`,
`lab`) : aucune ligne `inattendu`, `abimee` ni `EXCEPTION`, client compris au-delà de vingt
minutes ; l'arme de Barabas lâchée 49 fois chez l'hôte, et chez le client reprise 92 fois par la
sienne (`la sienne, reprise ou qu'elle soit`), une neuve donnée 5 fois faute d'arme à reprendre,
et une arme en trop retirée du sol 10 fois (`retiree d'ici, une seule de chaque`) ; le tir ami
mis par l'hôte et suivi par le client à chaque reconnexion ; l'alerte jouée par la fenêtre
active et laissée par l'autre. Les listes de la 0.27 et de la 0.26 qui suivent restent à faire pour ce qui touche à
l'écran.

Nouveautés de la 0.27 : une correction et deux demandes faites après un premier essai de la 0.26.
À deux, `run_host_radmin.bat` / `run_client_radmin.bat`.

Son d'alerte :

- un joueur tombe à terre, l'hôte ou le client : `AlertFall.mp3` s'entend **une fois**, pas en
  boucle, quelle que soit la fenêtre où l'on joue (journal de cette fenêtre : `son : alerte jouee
  une fois, volume 50 pour cent (joueur 1 a terre)` ; journal de l'autre : `son : alerte laissee a
  la fenetre du jeu qui a le clavier`) ;
- plus aucun son en boucle, plus rien sur la santé critique ;
- F1, onglet Coop : sous les deux curseurs des PNJ, un troisième, `Alert volume : 50 %` (`Volume
  alerte` en français). Le déplacer : le son est joué une fois à l'arrêt du curseur, au nouveau
  volume. À 0 : plus rien quand un joueur tombe ;
- **chez le client** l'onglet Coop n'a que ce curseur-là sous ses boutons, et il y marche ;
- dire si le son est trop fort à 50 %, et si le curseur est à sa place.

Vue à travers les murs, F1, onglet Display :

- dix boutons, le dernier `Wallhack (ESP) : ON` (`Vision murs (ESP)` en français), puis la ligne
  du pseudo. **Les boutons de réglage sont moins hauts qu'avant** (32 points au lieu de 40) pour
  tenir à dix : dire s'ils sont bien dessinés, et si le texte y est centré ;
- sur `OFF` : derrière un mur, la jauge, les munitions et le pseudo de l'autre joueur
  disparaissent ; en vue, ils sont là comme avant. Un joueur à terre derrière un mur : son panneau
  ne se voit plus ;
- sur `ON` : tout revient à travers les murs ;
- le réglage de l'hôte ne change rien chez le client, et inversement.

Réglages gardés, le point demandé :

- **chez l'hôte** : mettre `Team kill`, `God mode`, `Unlimited ammo` sur `ON`, changer la langue,
  couper `Wallhack`, déplacer `Alert volume`. **Chez le client** : changer la langue dans l'autre
  sens, couper `Ping`, mettre un autre volume. Fermer les deux jeux, les relancer : chacun retrouve
  exactement ce qu'il avait réglé (journal de l'hôte : `reglages de l'hote repris d'onimp.ini : tir
  ami 1, invincibilite 1, munitions illimitees 1`) ;
- dans `AE\onimp.ini` : `[display]` et `[audio]` portent les réglages de l'hôte, `[display_client]`
  et `[audio_client]` ceux du client ;
- le fichier à regarder ou à modifier est **`AE\onimp.ini`** ; `OniMP\onimp.ini` n'est que le
  modèle de la première installation, le jeu ne le lit pas.

Vu dans les journaux du premier essai de la 0.26 (hôte et client, quinze minutes) : première ligne
`OniMP 0.26`, `historique des commandes : debordement du moteur corrige`, aucune ligne
`inattendu`, `abimee` ni `EXCEPTION` ; la langue anglaise reprise au lancement ; le battement de
cœur lancé sur la santé critique puis arrêté à la mort du joueur ; un joueur à terre et son compte
à rebours. Pas vu : une console, un combat contre un PNJ armé, une partie de plus de vingt minutes
côté client. La liste de la 0.26 qui suit reste donc à faire, sauf son et battement de cœur.

Nouveautés de la 0.26 : quatre demandes faites après l'essai de la 0.25, et la cause du plantage
du client. À deux, `run_host_radmin.bat` / `run_client_radmin.bat`.

Plantage du client, le plus important :

- jouer **trente minutes** d'affilée côté client, au-delà des vingt où il plantait : il ne plante
  plus ;
- le journal du client ne doit plus avoir aucune ligne `zone de texte ... abimee` ; s'il en a une,
  me la donner avec les dix lignes qui la précèdent ;
- si le client plante encore : ne rien relancer, copier les deux journaux.

Consoles de données, niveau `lab` :

- **l'hôte** utilise une console de données : l'écran de texte s'ouvre chez lui seul, le client
  continue de jouer (hôte : `ecran de texte : 'level_1a' demande par la console que notre joueur
  vient d'utiliser, pour lui seul`) ;
- le client en utilise une : chez lui seul, comme en 0.25 (`... que le joueur 1 vient d'utiliser,
  pour lui seul`) ;
- une consigne du niveau, un objectif, une porte : toujours chez les deux.

Armes des PNJ, contre Barabas ou contre n'importe quel ennemi armé :

- le frapper jusqu'à ce qu'il perde son arme, plusieurs fois : compter les armes au sol chez
  l'hôte et chez le client (`Weapon IDs` sur `ON` les nomme). Il ne doit jamais y en avoir une de
  plus qu'avant ;
- le client ramasse l'arme tombée : l'hôte le voit la tenir, et il n'en reste pas une copie par
  terre chez l'un des deux ;
- **me donner, des deux journaux, les lignes `arme du PNJ` et `sol : arme`** du combat : `lachee`,
  `reprise`, `la sienne, reprise ou qu'elle soit`, `une neuve lui est donnee`, `retiree d'ici, une
  seule de chaque`, `celle du sol est retiree` ;
- à surveiller : une arme qui disparaît du sol chez le client alors qu'elle est bien chez l'hôte.

Langue, F1, onglet Display :

- neuf boutons, le dernier `Language : Francais` (affiché `Langue : Francais`), puis la ligne du
  pseudo : dire si tout tient, les boutons étant un peu plus serrés qu'en 0.25 ;
- le menu est en français au premier lancement : dire si un libellé déborde de son bouton, ou de
  l'étiquette d'un curseur de l'onglet Coop ;
- un clic sur `Langue` : l'onglet, tous les boutons et la ligne du pseudo passent à l'anglais sous
  les yeux, sans fermer le menu ; `OniMP : language : English` à l'écran ; les messages suivants
  sont en anglais ; un second clic remet le français ;
- fermer le jeu, le relancer : la langue est gardée (`language` sous `[display]`).

Joueur à terre :

- chez celui qui tombe : le panneau `A TERRE`, sa barre graduée qui se vide de l'ambre au rouge,
  les secondes dessus ;
- chez les autres : le même panneau avec son pseudo et, au-dessus, une croix dans un anneau qui
  bat ; **derrière un mur, un étage plus bas, à l'autre bout de la salle** il reste visible, et ne
  rapetisse pas avec la distance ;
- s'approcher : `CTRL POUR RANIMER` apparaît sous la barre ; tenir CTRL : le titre devient
  `REANIMATION`, la barre se remplit en bleu clair ;
- **dire ce que vaut le panneau à l'écran** (couleurs, taille, lisibilité, place du repère) : je ne
  l'ai jamais vu, et tout se règle.

Restes de la 0.25. Vu depuis dans ses journaux : le titre des fenêtres (`fenetre : titre 'Host -
Oni' (c'etait 'ONI ')`), les tenues (`tenue : 'konoko' passe de 'konoko_generic' a 'k3' (joueur
1)`), un pseudo saisi et gardé (`joueurs : pseudo 'Odolinsky' enregistre dans onimp.ini`), l'écran
d'une console du client chez lui seul, les munitions illimitées mises puis ôtées. La liste qui
suit est gardée pour ce qui reste à dire de l'écran.

Fenêtres : la barre de titre de la fenêtre de l'hôte porte `Host - Oni`, celle du client `Client -
Oni`, dès le menu principal (journal : `fenetre : titre 'Host - Oni' (c'etait 'ONI ')`).

Au-dessus des autres joueurs :

- de bas en haut : la barre de vie, une rangée de six petites cases carrées, le pseudo suivi de la
  latence (`HOST 12MS`, `P1 14MS` tant que personne n'a choisi de pseudo) ;
- cases : rouges à gauche pour les chargeurs de balles, vertes à droite pour les cellules ; dire si
  la rangée est trop petite, trop grande, mal placée ;
- **derrière un mur, un étage plus bas, à l'autre bout de la salle** : barre, cases et pseudo
  restent visibles, à l'endroit où se trouve le joueur ; dire s'ils disparaissent, s'ils clignotent,
  s'ils apparaissent au mauvais endroit quand le joueur est derrière soi ;
- F1, onglet Display : boutons `Ping` et `Skin`, puis la ligne `Name` et son champ. Dire si tout
  tient dans le menu et si le champ a l'air d'un champ ;
- cliquer dans le champ, taper un pseudo : une seconde après, `OniMP : pseudo ...` à l'écran, et
  l'autre joueur le voit au-dessus de nous ; fermer le jeu, le relancer : le pseudo est encore là
  (`name_host` ou `name_client` sous `[player]` dans `AE\onimp.ini`) ;
- si le champ n'y est pas ou ne prend pas les touches : le dire, et essayer `mp_name "pseudo"` ;
- `Ping : OFF` : les millisecondes disparaissent, le pseudo reste.

Tenues :

- à l'arrivée du client, il a de lui-même une autre tenue que l'hôte, chez lui et chez l'hôte
  (hôte : `joueurs : joueur 1 arrive, tenue 'K3' en attendant son choix` ; partout : `tenue :
  'mp_remote1' passe de 'konoko_generic' a 'k3' (joueur 1)`) ;
- bouton `Skin` : chaque clic change la tenue, chez soi et chez l'autre dans la seconde ; la tenue
  de l'autre joueur n'est jamais proposée ;
- après une reprise ou un changement de niveau : chacun retrouve sa tenue ;
- **dire à quoi ressemble chaque tenue** (`K3`, `K4 A`...) pour que je leur donne de vrais noms, et
  toute ligne `tenue : ... ne peut pas prendre la classe` ;
- à surveiller : un personnage déformé, figé ou sans animation après un changement de tenue.

Consoles de données, niveau `lab` (celles de l'essai de la 0.24) :

- l'hôte utilise une console de données : l'écran de texte s'ouvre chez lui seul (hôte : `ecran de
  texte : 'level_3c' demande par la console que notre joueur vient d'utiliser, pour lui seul`) ; le
  client continue de jouer sans être interrompu ;
- le client en utilise une : l'écran s'ouvre chez lui seul (hôte : `... que le joueur 1 vient
  d'utiliser, pour lui seul`) ;
- une consigne du niveau (début de niveau, objectif) : toujours chez les deux (`... demande par un
  script, pour tous les joueurs`) ;
- les objectifs et les portes que la console débloque changent toujours chez les deux.

Munitions illimitées, F1, onglet Coop de l'hôte :

- sept boutons, le dernier `Unlimited ammo : OFF`, puis les deux curseurs : dire si tout tient ;
- sur `ON` : `OniMP : munitions illimitees activees` chez les deux ; l'hôte et le client tirent
  sans que l'arme se vide ; sur `OFF` : elle se vide de nouveau ;
- chez le client le bouton se lit mais ne change rien (`OniMP : reglage reserve a l'hote`).

Restes de la 0.24, vus dans les journaux mais pas décrits : dire ce qui se voit à l'écran pour
l'aura et pour la lueur du bouclier de l'autre joueur, et si un civil donne bien une arme ou un
bouclier au client.

Nouveautés de la 0.24 : trois défauts et une demande signalés en jeu en 0.23. Vu depuis dans ses
journaux : l'aura, le bouclier, un hypo donné par un civil au client, un point de sauvegarde passé
sans faute (voir « Faits, à confirmer en jeu »). La liste qui suit est gardée pour ce qui reste à
dire de l'écran ; les munitions y sont décrites telles qu'en 0.24, deux rangées de points, et ont
été refaites en 0.25 (voir plus haut).

Munitions au-dessus des joueurs :

- au-dessus de la barre de vie de l'autre joueur, deux rangées de six cases sombres, de la largeur
  de la barre ; rangée du bas, un point rouge par chargeur de balles qu'il a en réserve ; rangée du
  haut, un point vert par cellule d'énergie ;
- il ramasse un chargeur : un point rouge de plus chez l'autre, dans la seconde ; il recharge :
  un de moins ; pareil pour les cellules avec une arme à énergie ;
- vu de l'hôte sur le client, et du client sur l'hôte : les mêmes nombres que ceux de l'interface
  du jeu de l'autre joueur ;
- F1, onglet Display : un sixième bouton, `Player ammo : ON` ; sur `OFF` les cases disparaissent,
  sur cette machine seulement ;
- dire si les cases sont trop petites, trop grandes, mal alignées sur la barre, ou si elles
  clignotent ; s'il n'y en a pas du tout : ligne `jauge : texture des reserves refusee` au journal.

Civils qui donnent un objet, niveau `Airport` (le civil caché derrière les caisses au début,
`HIDDEN_CIV_1` avec `NPC IDs`) ou tout niveau à civils :

- le **client** s'approche du civil, le regarde, presse la touche d'action : le civil se tourne
  vers lui, parle, fait son geste (hôte : `civil : 'hidden_civ_1' engage la conversation avec le
  joueur 1, a sa touche d'action`) ;
- le client reçoit l'objet, avec le son et le message du jeu, et `OniMP : un civil vous donne 1
  hypo(s)` (hôte : `civil : 1 hypo(s) donne(s) au pantin du joueur 1 : envoye(s) a son joueur` ;
  client : `civil : 1 hypo(s) recu(s) d'un PNJ de l'hote, 1 pris`) ; son interface montre un hypo
  de plus ;
- un civil qui donne un bouclier : le corps du client luit chez lui et chez l'hôte ;
- un civil qui donne une arme : le client l'a en main, celle qu'il tenait est à ses pieds, chez
  les deux (hôte : `civil : arme 'w1_tap' donnee par '...' au pantin du joueur 1 : envoyee a son
  joueur`) ;
- l'hôte parle à un civil : rien n'a changé pour lui ;
- un civil qui a déjà donné : la touche n'a plus d'effet, ni pour l'hôte ni pour le client ;
- `mp_status` : `civils : N demande(s) a l'hote, M don(s) recu(s)` chez le client ;
- me dire toute ligne `introuvable ici, rien d'envoye` ou `garnies hors conversation`, et si le
  civil reste figé ou tourné vers personne après la conversation.

Aura, bouclier, invisibilité :

- un joueur prend un hypo en pleine forme : l'aura l'entoure chez lui et chez l'autre. Chez lui :
  `aura : allumee chez notre joueur, annoncee dans son etat`. Chez l'autre : `aura : allumee sur
  'mp_remote0' d'apres sa machine (N sur 255)`, puis `aura : 'mp_remote0' : le moteur la montre ici
  a 0.NN (demandee 0.NN)`. **Si elle ne se voit toujours pas, me donner ces trois lignes** : elles
  disent si c'est l'émission, la transmission ou le tracé qui manque ;
- un joueur porte un bouclier (bonus, ou civil) : son corps luit chez l'autre aussi, et l'éclat
  d'un tir arrêté s'y voit (`bouclier : 'mp_remote1' en porte un d'apres sa machine (100)`) ;
- un client invisible : transparent chez l'hôte, et au bout d'une seconde et demie les ennemis le
  perdent de vue (`invisibilite : 'mp_remote1' est invisible d'apres sa machine`) ;
- un client avec bouclier sous les tirs d'un ennemi : il ne perd pas de vie tant que le bouclier
  tient, comme l'hôte.

Point de sauvegarde, le cas qui plantait chez le client en 0.23 :

- le client arrive le premier sur un point de sauvegarde, puis l'hôte, puis les deux ensemble :
  pas de plantage. Client : `sauvegarde : point 2 rencontre par notre script, laisse a l'hote`,
  puis `OniMP : point de sauvegarde 2 atteint, garde par l'hote`. Hôte : `sauvegarde : point 2
  atteint, dit aux clients` ;
- un joueur meurt ensuite : reprise commune à ce point, chez les deux ;
- **si le client plante encore** : ne rien relancer, copier les deux journaux, et me dire ce qui
  se voit (fenêtre fermée d'un coup, image figée, message de Windows) ;
- fermer le jeu par son menu, hôte puis client : dernière ligne `fermeture : le jeu se ferme ;
  zones de texte : commande 0/1, console 0/4, messages 0/13, sous-titres 0/13,` et pas de ligne
  `EXCEPTION` derrière. Me dire toute zone marquée `SANS LIGNES`.

Nouveauté de la 0.23, un défaut signalé en jeu en 0.22 : les cartouches d'une arme échangée.
Deux armes de la même classe, l'une entamée, l'autre vide (deux `w2_sap` du niveau `lab`) :

- le client tient l'entamée et l'échange contre la vide posée au sol : l'arme lâchée garde ses
  cartouches, chez lui comme chez l'hôte. Client : `arme 'w2_sap' #4097 lachee avec 28
  cartouche(s) : annoncee`, puis `arme 'w2_sap' #2 ramassee : annoncee` ; hôte : `arme 'w2_sap'
  #4097 lachee par le joueur 1, 28 cartouche(s)`, puis `arme 'w2_sap' #2 ramassee par le joueur
  1`. Les numéros sont ceux de la partie, et chaque arme garde le sien d'un échange à l'autre ;
- dans la seconde qui suit, plus de `sol : arme 'w2_sap' ..., 0 cartouche(s) au lieu de 28
  d'apres l'hote` chez le client ;
- échanger dix fois de suite, puis faire de même chez l'hôte : les deux armes gardent chacune
  leur compte, 28 et 0, sur les deux machines ;
- l'un lâche l'arme entamée, l'autre la ramasse et tire : elle a le nombre de cartouches annoncé ;
- me dire toute ligne `: reconnue a l'endroit` qui revient pour une arme déjà échangée, et toute
  ligne `introuvable au sol ici`.

Nouveautés de la 0.22, cinq défauts et une demande signalés en jeu en 0.21. Vu depuis dans ses
journaux : l'écran d'accueil sauté, le rechargement joué sur l'autre machine, l'inventaire vidé à
l'arrivée, l'arme d'un PNJ reprise chez le client. Le reste n'a pas été vu.

Lancement, hôte puis client, chacun « New Game » ou la même sauvegarde :

- plus d'écran d'accueil du niveau à passer d'une touche, ni chez l'un ni chez l'autre (journal :
  `niveau : ecran 'intro_splash_screen' saute, session en cours`) ;
- le client trouve l'hôte : chez tous, `OniMP : le joueur 1 arrive : reprise au dernier point de
  sauvegarde dans 5 s`, puis `reprise dans 4 s`, `3`, `2`, `1` (hôte : `arrivee : joueur 1 (parti du
  point 0, ... ; ici point 2, 85 s de jeu)`, puis `reprise : arrivee d'un joueur`) ;
- l'hôte recharge, le client aussi ; chez les deux `OniMP : niveau charge : en attente...`, le
  jeu figé quelques secondes, puis `OniMP : tous les joueurs ont charge : c'est parti` au même
  moment (hôte : `attente : le joueur 1 a charge le niveau`, `attente : terminee, tous les joueurs
  ont charge` ; client : `attente : terminee, depart donne par l'hote`) ;
- le client repart sans armes : `OniMP : vous arrivez dans la partie sans armes` deux secondes
  après le départ ; l'hôte a celles de sa sauvegarde ;
- objectifs (F1) : les mêmes chez les deux ;
- si les deux lancent « New Game » à moins de dix secondes d'écart : pas de reprise (hôte :
  `arrivee : joueur 1 ; nos deux niveaux viennent d'etre charges au point 0, rien a reprendre`) ;
- le client ferme son jeu pendant que l'hôte charge : l'hôte ne l'attend que dix secondes
  (`attente : le joueur 1 ne revient pas, on ne l'attend plus`) ;
- mort d'un joueur, puis fin de niveau : reprise et niveau suivant démarrent de même, ensemble,
  sans touche à presser, et le client ne charge le niveau suivant qu'une fois (client : `reprise :
  l'hote a charge ce niveau au point 0, nous aussi a l'instant : rien a recharger`).

Rechargement : un joueur recharge, l'autre voit le geste (chez l'autre : `rechargement :
'mp_remote1' recharge 'w1_tap' comme sur l'autre machine`, et plus `ne peut pas le montrer`). Si
cette ligne y est et que le geste ne se voit toujours pas, le dire : ce sera alors le tracé.

Objet du niveau, entrepôt : le **client** ramasse l'objet (LSI), puis l'un des deux va à la porte
qui l'attend : elle s'ouvre (hôte : `objet du niveau : ramasse par le joueur 1, donne aussi a notre
joueur`).

Renforts, `Enemies : x2` ou plus :

- le garde de la fin de l'entrepôt et sa copie (`GUARD1`, `GUARD1_X2` avec `NPC IDs`) attaquent
  tous deux, et se laissent frapper (hôte : `renfort : 'guard1_x2' passe dans l'equipe 2, comme
  'guard1'` si l'original change de camp après coup) ;
- les copies entrent dans le combat avec leur original au lieu de patrouiller de leur côté ;
- dire, avec son nom, tout ennemi qui reste inerte ou qu'on ne peut pas frapper.

Arme des PNJ, niveau `lab`, Barabus :

- son arme se voit chez les deux quand il tire ; frappé par le client, il ne la perd que si
  l'hôte l'a vu la perdre (client : `arme du PNJ 'Barabus' : 'w11_ba1' lachee ici seulement,
  reprise`) ;
- quand elle tombe pour de bon, elle est au sol chez les deux, et l'un ou l'autre peut la prendre ;
- plus de `arme 'w11_ba1' ramassee par le joueur 1 : introuvable au sol ici` chez l'hôte.

Nouveautés de la 0.21. Consoles, zone des messages et aura émise ont été vues dans les journaux
(voir « Réussis ») ; ce qui suit reste à dire de ce qui se voit à l'écran.

Texte à l'écran, hôte ou client :

- le texte du scénario (sous-titres des dialogues) est de nouveau en bas de l'écran, en blanc,
  comme avant la 0.20 ;
- les messages du mod, eux seuls, sont en haut à droite, en vert : `OniMP : joueur 1 connecte`,
  un réglage changé dans l'onglet Coop, `mp_status` (journal : `messages : zone creee en haut a
  droite (L x H), N ligne(s)` à leur premier message) ;
- ils s'effacent au bout de dix secondes, y compris menu F1 ouvert ;
- s'ils sont restés en bas, en blanc : ligne `messages : code inattendu` dans le journal.

Consoles, hôte et un client, entrepôt, salle de l'alarme :

- un joueur utilise une console d'alarme : une seule ligne `console 7 (rang 0) activee ici :
  annoncee` chez lui, une seule `console 7 (rang 0) activee par le joueur N` chez l'autre ; plus
  de lignes par milliers, plus de `desync : consoles differe` ;
- le journal ne grossit plus : quelques centaines de lignes pour vingt minutes, pas cinq mégaoctets.

Joueur à terre, à deux, des ennemis autour :

- un joueur tombe : dans la demi-seconde, les ennemis qui l'entouraient se tournent vers l'autre
  joueur et vont vers lui, même s'il est à l'autre bout de la salle (hôte : `cible : N PNJ autour
  d'un joueur a terre diriges vers un joueur debout`) ;
- ils ne restent plus plantés autour du joueur à terre ;
- le joueur relevé redevient une cible comme les autres.

Rechargement, à deux, chacun une arme et des munitions :

- un joueur recharge : l'autre voit son personnage faire le geste (chez lui : `rechargement :
  notre joueur recharge 'w1_tap', annonce dans son etat` ; chez l'autre : `rechargement :
  'mp_remote1' recharge 'w1_tap' comme sur l'autre machine`) ;
- si le geste ne se voit pas alors que cette seconde ligne y est : le dire, c'est alors le tracé
  et non la transmission ; si elle dit `ne peut pas le montrer`, joindre la raison ;
- un PNJ armé qui recharge chez l'hôte : même geste chez le client.

Aura, à deux, un hypo en pleine forme :

- le joueur qui le prend s'entoure de lumière chez lui, et son personnage aussi chez l'autre
  (chez lui : `aura : allumee chez notre joueur, annoncee dans son etat`) ; elle faiblit puis
  s'éteint chez les deux à mesure que sa santé redescend.

Identifiants, F1, onglet Display :

- cinq boutons : `Player health`, `NPC health`, `Messages`, `NPC IDs`, `Weapon IDs` ;
- `NPC IDs : ON` : le nom de chaque PNJ au-dessus de sa jauge, en capitales blanches sur un
  cartouche sombre (`A_T50`, `BAY1_THUG_1_X2`), le même chez l'hôte et chez le client ;
- `Weapon IDs : ON` : le nom de chaque arme au sol au-dessus d'elle, en jaune pâle (`W1_TAP`) ;
- `OFF` : ils disparaissent, sur cette machine seulement ; le choix est encore là après avoir
  relancé le jeu ;
- dire si une étiquette est illisible, trop grande, coupée, ou cachée par le décor.

Nouveautés de la 0.20. Les écrans de texte ont été vus dans les journaux (voir « Réussis ») ; ce
qui suit reste à dire de ce qui se voit à l'écran.

Écrans de texte du niveau (cadre bleu, plein écran), hôte et un client en coop complet
(`run_host_radmin.bat` / `run_client_radmin.bat`), premier niveau :

- l'hôte utilise une console de données, ou atteint une consigne (Konoko regarde son poignet) :
  l'écran s'ouvre chez lui et chez le client (dans les deux journaux : `ecran de texte : 'level_1d'
  demande, le script continue`, puis `ecran de texte : 'level_1d' ouvert`, puis `menu : ouvert, le
  jeu continue derriere`) ;
- chacun le referme de son côté, dans l'ordre qu'il veut : il retrouve ses commandes aussitôt,
  même si l'autre lit encore (`menu : ferme apres N image(s), ...`). C'est le cas qui figeait le
  jeu en 0.19 ;
- un seul écran par demande : plus de second écran identique derrière le premier ;
- le client utilise une console : même chose, dans l'autre sens ;
- l'hôte ouvre F1 juste avant qu'un script ne demande un écran, ou un client a F1 ouvert à ce
  moment-là : l'écran n'apparaît qu'à la fermeture du menu ;
- à surveiller : ce que le script fait pendant qu'on lit (la cinématique qui suit l'écran de fin
  d'entraînement, des ennemis qui arrivent). Le dire si un écran tombe à un mauvais moment ;
- `mp_status` : `ecrans de texte : N ouvert(s), 0 en attente` ;
- hôte seul en session, puis jeu lancé hors session (`ONIMP=off`) : l'écran s'ouvre et se ferme
  comme dans le jeu d'origine.

Messages à l'écran (leur place a été refaite en 0.21, voir plus haut) :

- les lignes `OniMP : ...` s'affichent en haut à droite, en vert, alignées sur le bord droit, la
  plus récente en bas ;
- plusieurs messages à la suite (cliquer plusieurs fois sur `Enemies` dans l'onglet Coop) : ils
  s'empilent vers le bas, les plus anciens s'en vont, aucun n'est coupé par un bord. Dire la
  résolution du jeu si une ligne dépasse, se replie mal ou disparaît trop vite ;
- après un chargement de niveau, ils sont toujours en haut à droite ;
- F1, onglet Display : un troisième bouton, `Messages : ON`. Sur `OFF`, plus aucune ligne `OniMP`
  à l'écran, mais elles sont toujours dans le journal (`console : OniMP : ...`) ; le choix est
  encore là après avoir relancé le jeu (`messages=false` sous `[display]` dans `AE\onimp.ini`) ;
- `mp_messages 1` dans la console les remet.

Nouveautés de la 0.19. Les journaux d'un client de la 0.19 en montrent une partie (voir « Faits, à
confirmer en jeu ») ; ce qui se voyait à l'écran n'a pas été rapporté. À essayer dans cet ordre, du
moins risqué au plus risqué.

Menu, hôte seul puis avec un client. F1, onglet Coop :

- six boutons l'un sous l'autre, `Difficulty : Normal`, `Enemies : x1`, `Civilians : ON`, `Team
  kill`, `God mode`, `Online damage`, puis les deux curseurs ; tout tient dans le menu, rien ne se
  chevauche, tout disparaît quand on choisit un autre onglet ;
- `Difficulty` : chaque clic passe au mode suivant, `Easy`, `Normal`, `Hard`, `Survival`, et la
  console le dit (`OniMP : difficulte Hard : 10 s pour ranimer, releve a 35 % de sante`) ; le choix
  est encore là après avoir relancé le jeu (`difficulty = hard` dans `AE\onimp.ini`) ;
- `Enemies`, hôte seul : x1, x2, x3, x4 puis x1 ; avec un client : `x2 (min x2)`, puis x3, x4, x2 ;
  à l'arrivée d'un client le bouton passe de lui-même à `x2 (min x2)` (`OniMP : ennemis x2, minimum
  a 2 joueurs`) ; à quatre joueurs : `x4 (locked)`, un clic n'y change rien ;
- F1 chez un client : onglet Coop avec les six boutons, sans curseurs ; ils montrent les réglages
  de l'hôte, un clic n'y change rien, et un réglage changé par l'hôte s'y voit sans refermer le
  menu.

Couleurs des jauges, à deux puis à quatre :

- au-dessus de l'hôte, vu d'un client : barre verte ; au-dessus des clients 1, 2 et 3 : bleue,
  orange, jaune ; la barre raccourcit quand le joueur perd de la vie, sans changer de couleur ;
- ennemis : barre rouge avec les chiffres ; Barabas (fin du niveau `lab`) : violette ; un PNJ qui
  n'est pas hostile (civil, allié) : gris clair.

Difficulté du jeu d'origine :

- `Difficulty : Hard` avant de charger un niveau : les ennemis ont un quart de vie en plus (jauge
  d'un Striker du premier niveau à comparer avec `Normal`), voient et parent mieux ;
- passer de `Normal` à `Hard` en cours de niveau, un ennemi en vue : sa jauge monte d'un quart
  aussitôt (`pnj : vie portee a 125 pour cent de l'origine pour N personnage(s)`) ; retour à
  `Normal` : elle revient ;
- `Survival` : la vie monte encore de moitié (`OniMP : PNJ : vie x1.50, degats x1.50 (0
  client(s))` hôte seul), un ennemi blessé regagne un point par seconde sur sa jauge, Barabas
  deux, et un coup de PNJ fait moitié plus mal ;
- quitter le jeu, puis le relancer sans OniMP en session (`ONIMP=off`) : les options du jeu ont
  gardé leur propre difficulté.

Joueur à terre, à deux, `God mode` sur `OFF` :

- un joueur se laisse tuer par un ennemi, l'autre debout dans le même niveau : il tombe au sol et
  y reste, ne peut plus bouger, les ennemis le laissent pour l'autre joueur (journal : `a terre :
  notre joueur est tombe, 25 s pour etre ranime`) ; au-dessus de lui, chez lui et chez l'autre,
  une grande barre avec les secondes qui restent ; à l'écran de l'autre, `OniMP : JOUEUR 1 A
  TERRE : 25 s pour le ranimer...`, redit toutes les cinq secondes ;
- l'autre s'approche et garde CTRL : `OniMP : reanimation du joueur 1 : gardez CTRL 3 s`, la barre
  devient blanche et se remplit, les secondes ne baissent plus ; au bout de trois secondes le
  joueur à terre retrouve 40 % de sa santé (`a terre : ranime par le joueur 0 ..., N points de vie
  (40 pour cent)`), reprend ses commandes et se relève avec une touche de déplacement ;
- CTRL relâché avant la fin, ou un ennemi qui frappe celui qui ranime : `reanimation du joueur 1
  interrompue par un coup`, et chez le joueur à terre `compte a rebours repris a N s`, N étant
  la valeur où il s'était arrêté ;
- personne ne le ranime : à zéro il meurt, `OniMP : le joueur 1 n'a pas ete ranime a temps :
  mission echouee` chez tous, puis reprise commune à la dernière sauvegarde ;
- les deux joueurs à zéro : le second meurt tout de suite, personne ne pouvant plus le ranimer ;
- hôte seul : il meurt comme avant, sans passer à terre ;
- refaire en `Easy` (35 s, 45 %), `Hard` (10 s, 35 %) et `Survival` (5 s, 25 %) ;
- à surveiller : le joueur à terre doit rester couché tout le compte à rebours, sans se relever ni
  glisser ; relevé, il ne doit pas rester bloqué ni garder ses touches coupées ; une cinématique
  qui démarre pendant qu'un joueur est à terre.

Multiplicateur d'ennemis. C'est la partie la plus risquée : elle crée des personnages que le
niveau n'a pas prévus. D'abord hôte seul, premier niveau, `Enemies : x2` puis `x4` :

- une seconde après chaque ennemi ordinaire, une copie apparaît à côté de lui, puis les suivantes
  (journal de l'hôte : `renfort : 'A_t48_x2' aux cotes de 'A_t48' (ennemis x2)`) ; même classe,
  même arme ; ils se battent et patrouillent comme lui ;
- aucun plantage, aucun renfort coincé dans un mur ou tombé sous le sol ; le dire avec le nom du
  niveau et de l'ennemi si cela arrive ;
- le niveau se termine normalement : portes, consoles, objectifs, cinématiques, fin de niveau ;
- les personnages des cinématiques ne sont pas doublés, ni Barabas ;
- `mp_status` : `ennemis x2 : N renfort(s) cree(s), M present(s)` ;
- avec un client (`run_host_radmin.bat` / `run_client_radmin.bat`) : les renforts existent aussi
  chez lui, au même endroit, bougent et se battent (client : `renfort : '..._x2' cree d'apres
  l'hote`, `pnj pilote : '..._x2'`) ; aucune ligne `desync : PNJ` durable chez l'hôte.

Civils, `Civilians : ON`, niveau `Airport` ou `lab` :

- quelques civils près de certains ennemis (journal : `civil : 'mpc00_generic_male_4' pres de
  '...'`), six au plus à la fois ; ils ne se battent pas, les ennemis ne les visent pas ;
- un joueur en tue un : `OniMP : un civil a ete tue par le joueur N : mission echouee` chez tous,
  puis reprise à la dernière sauvegarde, une seconde et demie plus tard ;
- un civil tué par un ennemi ou une explosion du décor : rien (`civil : '...' est mort`) ;
- niveau `roof` ou premier niveau : aucun civil ;
- `Civilians : OFF` : plus aucun nouveau civil.
Curseurs de difficulté, nouveauté de la 0.18. L'hôte suffit pour les voir ; un client pour en
voir l'effet. Chez l'hôte, F1, onglet Coop :

- sous les trois boutons, deux étiquettes et deux curseurs : `NPC health : +100 % per client` et
  `NPC damage : +5 % per client`, curseurs au cinquième et au quarantième de leur course. S'ils
  n'y sont pas : ligne `menu : curseurs de l'onglet Coop refuses` dans le journal de l'hôte ;
- ils ont l'aspect de ceux du menu Options du jeu, tiennent dans le menu, et disparaissent quand
  on choisit un autre onglet (Objective, Help, Display) ;
- tirer `NPC health` à la souris : l'étiquette suit, de +0 à +500 par pas de 10 ; avec un client
  connecté, la jauge des PNJ change pendant le déplacement, chez l'hôte et chez le client
  (`pnj : vie portee a N pour cent de l'origine` dans les deux journaux) ;
- relâcher : une demi-seconde plus tard, journal de l'hôte `difficulte : reglee dans le menu,
  ecrite dans onimp_difficulty.ini ; vie ...`, console de chacun `OniMP : PNJ : vie x..., degats
  x... (1 client(s))`, et `AE\onimp_difficulty.ini` porte les nouvelles valeurs, commentaires
  intacts ;
- `Online damage : OFF` : l'étiquette des dégâts finit par `(off)` ; tirer `NPC damage` : le bouton
  repasse sur `ON` et un coup de PNJ fait plus mal (`coup recu : ...` chez le client) ;
- fermer le menu aussitôt après avoir bougé un curseur : la valeur est écrite quand même ;
- relancer le jeu : les curseurs sont où on les avait laissés ;
- F1 chez un client : onglet Coop sans curseurs (depuis la 0.19 ; avant, pas d'onglet Coop).

Nouveautés de la 0.17, deux défauts signalés en jeu en 0.16.

Jauges, à regarder chez l'hôte et chez un client :

- au-dessus d'un autre joueur et d'un PNJ, immobile puis en mouvement, de près et de loin : la
  barre ne clignote plus, la partie pleine et les chiffres restent devant le fond ;
- les chiffres d'un PNJ sont entiers, aucun n'est rogné par son voisin ;
- un PNJ devant une porte, un meuble ou une console : sa jauge reste visible ; derrière un mur ou
  un pilier, elle est cachée comme lui ;
- deux PNJ l'un derrière l'autre : la jauge du plus proche passe devant ;
- si quelque chose cloche (jauge noire, jauge absente, rectangle autour des chiffres), le dire avec
  la distance à laquelle cela se voit.

Fichier de difficulté, `AE\onimp_difficulty.ini`, hôte et un client en jeu :

- mettre `clients_1 = 300` sous `[health]` et enregistrer, sans toucher au jeu : dans la seconde,
  console de l'hôte `OniMP : difficulte relue : vie 300 / 300 / 400, ...`, puis chez tous `OniMP :
  PNJ : vie x3.00, ...`, et la jauge des PNJ passe à trois fois leur vie d'origine (journaux :
  `pnj : vie portee a 300 pour cent de l'origine pour N personnage(s)`) ;
- `Online damage : ON` dans l'onglet Coop du menu F1 de l'hôte, puis `clients_1 = 200` sous
  `[damage]` : `degats x2.00`, et un coup de PNJ fait deux fois plus mal (`coup recu : 12 de
  '...'` au lieu de 6 chez le client) ;
- `Online damage : OFF` : `OniMP : PNJ : vie x3.00, degats normaux, Online damage sur OFF (1
  client(s))`, et les coups reviennent à leur valeur d'origine.

Menus, nouveauté de la 0.16. C'est la partie la moins sûre : elle fait tourner la simulation du
moteur depuis ses propres menus. Le menu F1 de l'hôte a tourné sans plantage dans les journaux de
la 0.16 ; ce qui se voyait à l'écran n'a pas été rapporté. À essayer d'abord à deux, hôte et un
client :

- l'hôte ouvre F1 pendant qu'un PNJ se bat : chez le client le PNJ continue de bouger, et l'hôte
  le voit bouger derrière son menu (hôte : `menu : ouvert, le jeu continue derriere`, puis à la
  fermeture `menu : ferme apres N image(s), M tick(s) simule(s)`, M valant à peu près soixante
  fois le nombre de secondes passées dans le menu) ;
- le client ouvre F1 en courant : son personnage s'arrête, chez lui et chez l'hôte ; un PNJ peut
  le frapper pendant qu'il est dans le menu (`coup recu : ...` chez le client) ;
- le menu lui-même n'a pas changé : onglets, pages, onglets Coop et Display, fermeture ;
- même chose avec Échap (menu principal, options) et retour au jeu : pas de plantage, le jeu
  reprend normalement. C'est le cas à surveiller, ce menu s'ouvrant de l'intérieur de la
  simulation ;
- l'écran d'une console de données, lu par un joueur : le jeu continue chez les autres et
  derrière lui ;
- hôte seul, sans client : F1 met le jeu en pause comme avant (`menu : ouvert, personne d'autre,
  le jeu est en pause`) ; un client lancé pendant ce temps le rejoint (`menu : un joueur est
  relie, le jeu reprend derriere`) ;
- un joueur meurt ou le niveau se termine pendant qu'un autre est dans un menu : la reprise ou le
  niveau suivant se fait à la fermeture du menu ;
- `mp_status` : `... jeu en marche dans les menus` ;
- pour revenir à la pause d'origine : `run_in_menus = false` dans `AE\onimp.ini` (journal : `menu :
  run_in_menus = false, ...`).

Fichier de difficulté, restes de la 0.16 (le fichier lu au premier client et la vie doublée ont
été vus, voir « Réussis ») :

- une valeur absurde (`clients_1 = 99999`) est ramenée à sa borne, 2000 ;
- deux puis trois clients : `clients_2` et `clients_3` s'appliquent à leur arrivée ;
- sans lancer le jeu : `OniMP\build\onimp_difficultytest.exe AE\onimp_difficulty.ini` affiche ce
  que le jeu en tirera.

Quatre joueurs, restes de la 0.14 et de la 0.15. Les journaux de la partie à quatre du 9 octobre
montrent les trois clients reliés, un double par joueur partout, la difficulté qui monte avec
chaque client et le PNJ qui change de cible entre deux clients. Reste, à quatre :

- au-dessus de chaque autre joueur : la barre de vie seule, sans chiffres ; au-dessus des PNJ, la
  barre avec ses chiffres ;
- un client lâche son arme : elle tombe chez les trois autres, et un autre client la ramasse
  (chez les autres, hôte compris : `arme '...' #K lachee par le joueur N, M cartouche(s)`) ;
- `Team kill : ON`, un client frappe un autre client : celui-ci perd de la vie et réagit, et les
  deux autres le voient (`tir ami : D sur le joueur N, annonce` chez celui qui frappe ; `coup
  recu : D de 'mp_remoteM'` chez le joueur N seulement, aucune ligne chez les autres) ;
- un client meurt : reprise commune, les quatre rechargent (hôte : `reprise : le joueur N est
  mort`) ; deux clients morts à quelques secondes d'écart : une seule reprise ;
- un client ferme son jeu : son personnage disparaît chez les autres en cinq secondes au plus
  (`pantin joueur N : suppression`) et la partie continue ; il relance et revient ;
- un cinquième lancement de `run_client_radmin.bat` : `partie '...' trouvee a ..., mais complete
  (4 joueurs)` ;
- `mp_status` chez l'hôte : `mode=host joueurs=4/4 clients=1 2 3`.

Vu dans ces mêmes journaux, à regarder : à chaque apparition d'un double, `arme du pantin N :
tient '', range '', attendu 'w1_tap'`, l'arme lui étant redonnée aussitôt. C'est le défaut déjà
connu à deux (voir « Objets »), maintenant une fois par double.

Mods de l'AE Installer, restes de la 0.13. Aujourd'hui sont installés les neuf paquets du socle
et `34090 Blood` ; Brutal AI (21501) et Better Warehouse / Training (24501) sont téléchargés mais
pas installés, ils n'étaient pas cochés à la dernière installation :

- avec Blood, du sang aux coups et aux impacts de balle, chez l'hôte et chez les clients ;
- `ae_installer.bat`, cocher un mod de plus, installer, relancer : il figure dans la ligne
  `mods AE :` et se voit en jeu ;
- si l'AE Installer a réinstallé Daodan : `run_host_radmin.bat` s'arrête sur `OniMP n'est pas
  installe dans ...` ; `install.bat`, puis relancer ;
- sur deux machines, un mod de plus chez l'un : console de l'hôte et de ce client, `OniMP : mods
  AE differents de ceux du joueur N`, puis `ici seulement : ...` et `chez lui seulement : ...` ;
- mods à essayer un par un, parce qu'ils touchent ce qu'OniMP synchronise : Brutal AI (profils de
  mêlée des PNJ et de Konoko), puis tout mod de personnages, d'armes ou de niveau. Relever toute
  ligne `desync :` ou `EXCEPTION`, et tout PNJ, animation ou arme qui manque chez un client.

Nouveautés de la 0.12. La recherche de l'hôte par la carte Radmin est passée sur une seule machine
(voir « Réussis ») ; reste à la valider **sur deux machines** reliées au même réseau Radmin VPN :

- l'hôte lance `run_host_radmin.bat`, le client `run_client_radmin.bat`, chacun démarre le même
  niveau : le client trouve l'hôte et se connecte sans adresse (hôte : `partie annoncee sur
  26.x.x.x (Radmin VPN)` ; client : `recherche d'un hote`, puis `partie 'Oni' trouvee a
  26.x.x.x:27777 (niveau '...') : connexion`, puis `pair connecte`) ;
- si rien n'arrive chez le client : pare-feu, voir le README ;
- le client coupe Radmin VPN en cours de partie : `pair perdu`, `Radmin VPN introuvable`, puis à
  son retour `Radmin VPN connecte`, la partie est retrouvée et la connexion reprend seule ;
- l'hôte recharge son niveau ou meurt : le client le retrouve seul ;
- en cours de partie, `mp_status` chez l'hôte : `desync : N controle(s), 0 renvoi(s)`, N qui
  monte d'un par seconde et par client ; aucune ligne `desync :` dans son journal pendant une
  partie normale.
  S'il y en a une, la relever avec ses deux empreintes : c'est soit un vrai écart, soit une
  fausse alerte du détecteur à corriger.

Sur une seule machine, `run_host_radmin.bat` puis `run_client_radmin.bat` font la même chose par
la carte Radmin locale, ce qui vérifie tout sauf la traversée du réseau. Les fenêtres y partagent
le dossier du jeu, donc les mods : un écart de mods ne se voit que sur deux machines.

Confirmé par les journaux de la 0.13 : mêlée entre joueurs avec le tir ami, dans les deux sens ;
invincibilité ; mods listés et comparés ; passage au niveau suivant ; hôte retrouvé après un
chargement. Par ceux de la 0.11 : prises entre joueurs, vie des PNJ doublée, console activée par
le client et répercutée chez l'hôte, réglages d'affichage gardés dans `onimp.ini`.

Reste de la 0.11 :

- `Team kill : ON`, invincibilité coupée : celui qui est frappé perd de la vie (`coup recu : N de
  'mp_remoteM'`). Les journaux de la 0.13 montrent les coups et les réactions, pas la vie perdue,
  l'invincibilité étant active ;
- `Team kill : OFF` : les coups traversent de nouveau les autres joueurs ;
- au-dessus de chaque autre joueur : une barre pleine, sans fond blanc ni chiffres, de la couleur
  de son numéro depuis la 0.19 (voir plus haut) ;
- la même jauge, un peu plus petite et avec `santé/maximum` en chiffres blancs, au-dessus de
  chaque PNJ vivant, chez l'hôte et chez les clients, et la même valeur partout ;
- F1 chez l'hôte et chez le client : onglets Coop puis Display sous Help ; `Player health` et
  `NPC health` passent de `ON` à `OFF` et les jauges disparaissent sur
  cette machine seulement ; le choix est encore là après avoir relancé le jeu ;
- le client tue un PNJ : il ne tombe qu'une fois, ne se relève pas, et laisse au sol la même
  chose que chez l'hôte (client : `sol : N bonus pose(s), M retire(s) d'apres l'hote`) ;
- un joueur ramasse un chargeur posé à côté d'un hypo : c'est le chargeur qui disparaît chez
  l'autre (`bonus de type 0 ramasse`, type 2 pour un hypo) ;
- un PNJ armé tire puis meurt : son arme a le même nombre de cartouches chez les deux joueurs
  (client : `sol : arme '...' #K, N cartouche(s) au lieu de M d'apres l'hote`).

Reste de la 0.10 :

- tir ami avec une arme, du client vers l'hôte ;
- `God mode : ON` : à l'écran, personne ne perd de vie (coups de PNJ, tirs, chutes) ;
- l'hôte reste plus de cinq secondes dans le menu F1 : pas de `pair perdu` chez les clients.

Reste de la 0.9 :

- un joueur vise un point précis (un mur, un ennemi), avec et sans ennemi près du viseur, en
  haut, en bas et sur les côtés : chez l'autre, son bras et son arme pointent le même endroit
  et ses tirs y arrivent ; dans les deux sens, et pour un PNJ armé vu depuis le client.

Reste des versions précédentes :

- un PNJ projette le client : une seule chute, la vie ne baisse qu'une fois (hôte :
  `projection : ... annoncee` ; client : un seul `coup recu : 20` après `projection : ...
  projette 'konoko'`) ;
- chaque joueur voit l'autre recharger ;
- le client part devant : les ennemis apparaissent quand il entre dans une salle, chez les
  deux joueurs ;
- l'hôte meurt : reprise commune, le client suit ;
- les tirs d'un PNJ sur le client se voient et s'entendent chez le client ;
- les coups du client sur un PNJ ne comptent qu'une fois (hôte : `degats client : ...`) ;
- un bonus ramassé par un joueur disparaît chez l'autre (`bonus ... ramasse`) ;
- après une arme ramassée ou lâchée, relever les lignes
  `arme du pantin : tient ..., range ..., attendu ...` ;
- après la mort d'un joueur ou un rechargement de niveau, tout cela marche encore (les deux
  journaux doivent montrer `pair perdu` puis `pair connecte` au même moment) ;
- une porte à touche ouverte par un joueur s'ouvre chez l'autre ;
- une console utilisée par le client produit son effet chez l'hôte.

### 2. Combat

- Joueur à terre : les PNJ ne le frappent pas au sol, rien ne le blessant, et se tournent vers un
  joueur debout. À voir à l'usage : le laisser vulnérable aux coups au sol, qui raccourciraient son
  compte à rebours, pour que les ennemis aient une raison de rester.
- Réaction des PNJ aux coups du client : elle est décidée chez l'hôte, quand le double du
  client y touche le PNJ. Chez le client le PNJ ne réagit donc qu'avec retard, et pas du tout
  si le coup a manqué chez l'hôte.
- Voix et cris des PNJ chez le client : leur IA ne tourne que chez l'hôte.
- Armes des PNJ : depuis la 0.22 l'hôte dit ce que chacun tient et le client s'y conforme. Reste
  le geste : une arme ramassée par un PNJ lui apparaît en main, et ses cartouches ne suivent pas.
- Cible des PNJ : seule la distance départage les joueurs. Un PNJ ne tient plus compte de celui
  qui l'a blessé.
- Difficulté : les mêmes pourcentages pour tous les PNJ. À voir à l'usage : des valeurs à part
  pour les boss, dont les scripts attendent des seuils de vie.
- Renforts : ils apparaissent à côté de leur original, à vue du joueur s'il est à vue. Les faire
  arriver d'un point hors de vue, ou par les entrées du niveau.
- Renforts : ceux d'un ennemi auquel un script donne ensuite des ordres par son nom (changer de
  poste, courir à une alarme) ne le suivent pas. À voir à l'usage, niveau par niveau.
- Civils : sans rôle ni trajet, ils restent où ils apparaissent. Leur donner les trajets des
  civils du niveau quand il en a.
- Tir ami : le coup d'un joueur sur l'autre est jugé chez celui qui frappe, avec ce qu'il voit.
  Celui qui encaisse peut donc être touché par un coup qui le manque à son écran.
- Tir ami : en frappant, le joueur ne se tourne pas de lui-même vers l'autre joueur comme il le
  fait vers un ennemi (le moteur refait le test d'équipe en `0x4E9AFA`).
- Tir ami : vérifier qu'une prise entre deux Konoko trouve bien son animation de victime.
- Vie des PNJ multipliée : les seuils de vie attendus par les scripts de boss ne le sont pas.

### 3. Vie et mort des joueurs

- Reprise : tous les joueurs réapparaissent au même point, les uns dans les autres, avec l'état
  enregistré de l'hôte. Donner à chaque client son propre état enregistré, et sa place.
- Reprise : si le jeu recharge de lui-même le niveau du client avant que l'hôte n'ait repris,
  le client charge deux fois.
- Rejoindre une partie avancée : depuis la 0.22 l'hôte reprend à son dernier point de sauvegarde
  et le client suit. Reste le cas du client arrivé dans un autre niveau que l'hôte : il faudrait
  lui faire charger celui de l'hôte.
- Arrivée d'un joueur : tout le monde revient au dernier point de sauvegarde. À voir à l'usage :
  faire apparaître le nouveau venu près de l'hôte, sans rien recharger.
- Départ commun : l'attente est une pause sans rien à l'écran qu'une ligne de texte. À voir à
  l'usage : un vrai écran d'attente, avec qui a chargé.
- Sauvegarde déclenchée par le client : elle enregistre l'état du joueur de l'hôte là où il se
  trouve, même loin derrière.
- Santé de l'autre joueur : son maximum affiché est celui du personnage distant, pas celui que
  l'autre machine connaît.
- Joueur à terre : la réanimation se fait à la touche CTRL lue au clavier, quelle que soit la
  configuration des touches du jeu. La lire dans cette configuration, ou la rendre réglable.
- Joueur à terre : rien ne montre où il est à travers les murs. Une flèche ou une distance à
  l'écran des coéquipiers.
- Joueur relevé : il se relève de lui-même à la première touche de déplacement. À voir à
  l'usage : le relever d'office.
- Barre de vie de l'interface du jeu (en bas de l'écran) : lui donner la couleur du joueur, comme
  aux jauges.
- Mode de difficulté changé en cours de niveau : la vie des joueurs (x1,3 en facile) ne suit qu'au
  prochain chargement.

### 4. Objets

- Arme du double : il lui arrive de ne pas avoir l'arme que l'autre joueur tient (vu dans les
  journaux de la 0.8 et de la 0.10). Elle lui est redonnée, mais la cause reste à trouver.
- Armes au sol : le client ne crée ni ne retire d'arme d'après l'hôte, il n'en corrige que les
  cartouches. Celle d'un PNJ suit maintenant ce que l'hôte dit qu'il tient (0.22). Depuis la 0.23
  chaque arme a un numéro commun aux machines : il servirait aussi à poser chez le client une arme
  qui n'existe que chez l'hôte, ou à en retirer une de trop.
- Arme reprise par un PNJ chez le client (`lachee ici seulement, reprise`) : il prend encore la
  plus proche de sa classe, qui peut être celle qu'un joueur vient de lâcher à côté de lui.
- Objet du niveau : donné à tous quand un joueur le ramasse. Les clés de porte de l'inventaire
  (`DoorKeys`), si un niveau s'en sert, ne sont pas partagées.
- Bonus posés d'après l'hôte chez le client : ils ne tombent pas, ils rejoignent par à-coups
  l'endroit où celui de l'hôte s'arrête.
- Sans l'option `npcs`, rien de tout cela : chaque joueur a son propre sol.
- Munitions et rechargement du personnage distant (il a des munitions infinies).

### 5. Progression

- Enchaînement des niveaux : trois niveaux à la suite à deux en 0.21. Le double chargement du
  client est corrigé en 0.22, à confirmer. À quatre : à vérifier.
- Écran de fin de niveau : sauté en session depuis la 0.22. Le remplacer par un récapitulatif
  commun.
- Cinématiques chez le client : caméra et blocage des commandes à vérifier.
- Sauvegardes : seul l'hôte sauvegarde ; reprise d'une partie à deux non prévue.

### 6. Menu et confort

- Écrans de texte : le script n'attend plus la lecture. À voir à l'usage : pour ceux qui précèdent
  une cinématique, faire attendre le script jusqu'à ce que l'hôte ait refermé le sien ; et laisser
  chaque client choisir de ne pas recevoir les écrans demandés par l'hôte.
- Messages à l'écran : sur `OFF`, tout disparaît. À voir à l'usage : garder les alertes (joueur à
  terre, mission échouée) quel que soit le réglage.
- Identifiants : ce sont des noms. À voir à l'usage : un numéro court en plus, le même sur toutes
  les machines, et l'identifiant de l'arme que tient un personnage.
- Menus en session : le joueur qui en ouvre un reste dans le monde, sans défense. À voir à
  l'usage : le rendre invulnérable le temps du menu, ou le signaler aux autres.
- Menu F1 du client : il lit les réglages de l'hôte dans l'onglet Coop depuis la 0.19. Reste à
  griser ses boutons, pour qu'il voie d'emblée qu'ils ne sont pas à lui.
- Curseurs de difficulté : ils règlent ce que chaque client ajoute, le même pas pour un, deux et
  trois clients. Une valeur à part pour chaque nombre de clients, ou des PNJ plus faibles qu'à
  l'origine, ne se règlent encore que dans `onimp_difficulty.ini`. À voir à l'usage.
- Menu F1 du client : lui montrer aussi ce que les curseurs de l'hôte donnent (vie et dégâts des
  PNJ par client).
- Jauges des PNJ : elles se voient pour tous les PNJ présents jusqu'à une certaine distance. À
  voir à l'usage : les réserver aux PNJ en combat, ou aux ennemis.
- Écrans de fin de niveau : ils attendent une touche, et rien ne tourne pendant ce temps. Ce ne
  sont pas des dialogues : le jeu qui continue derrière les menus ne les couvre pas.
- Fenêtres côte à côte au lieu de fenêtres plein écran superposées : à quatre sur une machine,
  Alt+Tab ne suffit plus.
- Savoir qui est qui : depuis la 0.19 la jauge de chaque joueur a sa couleur (vert, bleu, orange,
  jaune). Les doubles eux-mêmes restent tous de la même classe (`konoko_generic`) et sans nom.
- Lancement refusé par Daodan si la touche Maj est enfoncée : prévenir le joueur.
- Message clair quand deux joueurs ne sont pas dans le même niveau ou la même version.

### 7. Portée

- Hôte qui s'en va : la partie s'arrête pour tous. Passer la main à un client.
- Jeu par Internet : traversée de routeur, authentification, chiffrement. Aujourd'hui, réseau
  local de confiance uniquement.

### 8. Mods de l'AE Installer

- Comparer aussi la version des paquets et les fichiers de niveau eux-mêmes : seuls les numéros de
  paquet le sont.
- Mods qui apportent leurs propres scripts de niveau : vérifier que l'option `quests` intercepte
  et transmet ce qu'ils font.
- Laisser l'hôte refuser un client dont les mods diffèrent, au lieu de le signaler seulement.
- Vie des PNJ doublée et personnage distant (`konoko_generic`) : à revoir avec un mod qui change
  les personnages.

### 9. Technique

- Journal du client : la ligne `bsl : ai2_allpassive 1`, répétée toutes les cinq secondes, noie
  le reste. À ne plus écrire.
- Menu Échap en session : la simulation y tourne de l'intérieur d'elle-même. Si un défaut
  apparaît là et pas avec F1, ouvrir ce menu depuis la boucle du jeu, comme F1, plutôt que depuis
  le traitement des touches.
- Compilation MSVC, jamais essayée.
- Test en jeu automatisable : aucun moyen aujourd'hui de démarrer un niveau sans passer par
  le menu.

## Limites assumées

- Quatre joueurs au plus, et tout passe par l'hôte : ce que fait un client met un aller-retour
  de plus à parvenir à un autre client qu'à l'hôte.
- Chaque autre joueur est affiché avec 100 ms de retard, prix de l'interpolation.
- Les projectiles sont simulés séparément sur chaque machine : trajectoires légèrement
  différentes.
- Un PNJ loin du joueur hôte est inactif chez lui : il apparaît figé chez le client.
- Les commandes tapées dans la console développeur du client sont neutralisées en mode coop.
- Les réglages de partie (tir ami, invincibilité) appartiennent à l'hôte et sont coupés à
  chaque lancement.
- La difficulté ne suit le nombre de clients qu'avec des PNJ partagés (option `npcs`) : sans
  elle, chaque joueur affronte seul ses propres ennemis, à leur vie et leurs dégâts d'origine.
  C'est le fichier de l'hôte qui vaut pour toute la partie.
- Au-dessus des autres joueurs, la santé n'est plus chiffrée : la barre seule, depuis la 0.14.
- En session, un menu n'arrête plus le jeu : celui qui l'ouvre reste dans le monde, immobile. Seul,
  ou tant que personne n'est relié, la pause est celle du jeu d'origine.
- Des mods différents chez l'hôte et un client sont signalés, pas empêchés : c'est aux joueurs de
  cocher les mêmes dans l'AE Installer.
- En session, un écran de texte du niveau n'arrête plus son script : la suite se déroule pendant
  qu'on lit. C'est le prix d'un écran qui ne peut plus figer le jeu ni faire attendre les autres.
- Un joueur à terre n'est pas frappé au sol : les ennemis l'abandonnent pour un joueur debout.
- Un joueur qui arrive ramène tout le monde au dernier point de sauvegarde de l'hôte, et repart
  sans armes.
- En session, ni écran d'accueil après un chargement, ni écran de fin de niveau.
- Les identifiants sont des noms, ceux du niveau et des classes d'arme, pas des numéros.

## Journal des versions

| Version | Contenu |
|---|---|
| 0.1 | Chargement, lecture mémoire, réseau, personnage distant, animations |
| 0.2 | Interpolation sur ligne de temps, verrou d'animation : correctif du sprint |
| 0.3 | PNJ pilotés par l'hôte, scripts de niveau décidés par l'hôte |
| 0.4 | Portes, consoles, arme en main, tirs, visée |
| 0.5 | Correctifs de combat : plus de tir pendant les coups, PNJ visant aussi le client, projections du client et des PNJ |
| 0.6 | Canal fiable réparé après reconnexion, dégâts arbitrés, tirs et coups des PNJ sur le client, armes et bonus au sol |
| 0.7 | Plantage au tir d'un PNJ chez le client corrigé, reprise commune à la dernière sauvegarde de l'hôte |
| 0.8 | Projection qui ne se rejoue plus, visée du bras, rechargement, zones de déclenchement ouvertes au client |
| 0.9 | Direction réelle de l'arme transmise et imposée dans le moteur, PNJ qui visent le joueur le plus proche |
| 0.10 | Cartouches d'une arme lâchée, santé de l'autre joueur au-dessus de sa tête, tir ami et invincibilité réglés par l'hôte depuis un onglet du menu F1, connexion gardée pendant ce menu |
| 0.12 | Partie annoncée sur le réseau (Radmin VPN en priorité), client qui cherche et rejoint l'hôte sans adresse, refus explicites, détecteur de désynchronisation avec renvoi forcé |
| 0.11 | Mêlée et prises entre joueurs avec le tir ami, jauges de vie refaites et étendues aux PNJ, onglet Display du menu F1, vie des PNJ doublée à deux, butin en double corrigé chez le client, bonus au sol et cartouches des armes alignés sur l'hôte |
| 0.13 | Jeu lancé depuis le dossier que l'AE Installer tient à jour, donc avec ses mods ; liste des mods dans le journal, échangée et comparée entre les deux joueurs ; lancement arrêté si OniMP n'est pas installé dans ce dossier |
| 0.14 | Quatre joueurs : un hôte et trois clients, l'hôte redistribuant à chacun ce qui vient des autres ; un double par joueur ; réseau de l'hôte servi pendant son menu de pause ; jauge des joueurs sans chiffres ; fausse alerte du détecteur de désynchronisation corrigée ; un journal par client sur une même machine |
| 0.15 | Difficulté selon le nombre de clients : vie des PNJ x2, x3, x4, dégâts des PNJ majorés de 5 % par client, réglage `Online damage` de l'hôte dans le menu F1 pour revenir aux dégâts normaux ; corrections issues d'une revue du code de la 0.14 |
| 0.16 | Difficulté réglable dans `onimp_difficulty.ini`, en pour cent, relu en cours de partie ; en session, le jeu continue derrière les menus au lieu de se mettre en pause |
| 0.17 | Jauges qui ne clignotent plus (tracé immédiat, couches décalées vers la caméra) ; fichier de difficulté relu dès qu'il est enregistré, confirmé dans la console ; message quand `Online damage` sur `OFF` fait ignorer les dégâts du fichier |
| 0.18 | Deux curseurs dans l'onglet Coop du menu F1 de l'hôte : vie et dégâts que chaque client ajoute aux PNJ, appliqués pendant le déplacement et gardés dans `onimp_difficulty.ini` |
| 0.19 | Quatre modes de difficulté fondés sur ceux du jeu d'origine, avec un mode survie ; joueur à terre, compte à rebours et réanimation à CTRL, mission échouée s'il n'est pas ranimé ; jauges aux couleurs des joueurs, des ennemis et des boss ; multiplicateur d'ennemis x1 à x4 avec minimum selon le nombre de joueurs, renforts créés d'après les ennemis du niveau ; civils dont la mort par un joueur fait échouer la mission ; onglet Coop lisible chez les clients |
| 0.20 | Écrans de texte du niveau ouverts depuis la boucle du jeu et non plus de l'intérieur de leur script : plus de jeu figé après leur fermeture, et le script n'attend plus la lecture ; messages du mod en haut à droite et en vert, masquables par le bouton `Messages` de l'onglet Display |
| 0.21 | Console du jeu remise comme avant, messages du mod dans leur propre zone en haut à droite ; portes et consoles de même numéro suivies à part, plus d'inondation d'annonces ; PNJ qui quittent un joueur à terre pour un joueur debout ; rechargement et aura dans l'état des personnages ; identifiants au-dessus des PNJ et des armes au sol, masquables dans l'onglet Display ; pile dans le journal de plantage |
| 0.22 | Protocole 16. Arrivée d'un joueur : reprise commune au dernier point de sauvegarde après un compte à rebours, nouveau venu sans armes ; départ commun après chaque chargement, écrans d'accueil et de fin de niveau sautés ; plus de double chargement du client au niveau suivant ; rechargement enfin montré (variante d'arme du double) ; objet du niveau donné à tous ; renforts alignés sur leur original (équipe, passivité, connaissance des joueurs) ; arme des PNJ dictée par l'hôte |
| 0.23 | Protocole 17. Numéro des armes, le même sur toutes les machines, porté par chaque annonce d'arme lâchée ou ramassée et par la liste du sol de l'hôte : deux armes de la même classe au même endroit ne sont plus confondues, et un échange ne fait plus perdre ni gagner de cartouches ; l'arme que le joueur d'un client vient de lâcher n'est plus corrigée par une liste en retard |
| 0.24 | Protocole 18. Munitions des autres joueurs au-dessus de leur barre de vie, en cases : points rouges pour les chargeurs de balles, verts pour les cellules d'énergie ; aura sur un octet, bouclier de force et invisibilité dans l'état des personnages ; civils du niveau qui parlent au joueur d'un client et lui remettent ce qu'ils donnent ; points de sauvegarde laissés à l'hôte ; garde à la fermeture du jeu sur une zone de texte du moteur sans lignes |
| 0.25 | Protocole 19. Zones de texte du moteur contrôlées et refaites à chaque image (plantage du client de la 0.24) ; titre des fenêtres par les scripts Radmin ; munitions en une rangée de six cases carrées ; pseudo saisi dans le menu, tenue de Konoko propre à chaque joueur, latence, le tout lisible à travers les murs ; écran de texte d'une console pour le seul joueur qui l'utilise ; munitions illimitées décidées par l'hôte |
| 0.28 | Protocole 19 inchangé. Barabas en double à chaque reprise corrigé (journal des scripts vidé à tout rechargement, doubles cachés et retirés aussitôt) ; PNJ gardés actifs près de chaque joueur (ils flottaient et tremblaient, vus de loin) ; joueur à terre fixé au sol et insensible aux coups ; cinématiques avec un seul personnage principal, portraits des dialogues en bas ; douze chargeurs et douze cellules, rangée de douze cases ; jeu automatique pour un client |
| 0.27 | Protocole 19 inchangé. Battement de cœur en boucle remplacé par une alerte jouée une fois quand un joueur tombe, entendue de tous, avec un curseur de volume dans l'onglet Coop chez l'hôte comme chez les clients ; bouton `Wallhack (ESP)` de l'onglet Display pour couper la vue à travers les murs ; tous les réglages du menu gardés d'un lancement à l'autre, `Team kill`, `God mode` et `Unlimited ammo` compris, avec des sections propres au client dans `onimp.ini` ; boutons de réglage moins hauts |
| 0.26 | Protocole 19 inchangé. Cause du plantage du client trouvée et corrigée : l'historique des commandes du moteur débordait après 256 commandes et écrasait la zone des messages ; écran de texte d'une console de l'hôte chez lui seul (activation des consoles détournée) ; une seule arme par PNJ, sol du client aligné sur celui de l'hôte (arme de Barabas en double) ; langue du mod, français ou anglais, changée dans l'onglet Display avec effet immédiat ; joueur à terre : panneau aux couleurs du jeu, repère visible à travers les murs, invite de réanimation, battement de cœur en boucle à terre ou en santé critique |
| 0.29 | Protocole 20. Jeu figé des clients : cause lue dans leur mémoire (position du joueur « NaN », boussole du HUD en boucle sans fin, zones du niveau déclenchées d'un coup) ; plus aucune position invalide n'entre dans le moteur ni ne passe par le réseau, joueur et personnages ramenés à leur dernière place saine. Arme prise à un PNJ par une prise : annoncée, retirée au PNJ par l'hôte, une seule de chaque. Joueur à terre : coup reçu au sol montré sans le relever ; réanimation qui ne saute plus d'un joueur à terre à l'autre. Jeu automatique refait : un joueur à terre d'abord, hôte rejoint par où il est passé, ennemi au sol balayé |
| 0.30 | Protocole 20 inchangé. Barabas qui retombait du ciel chez les clients après la cinématique de sa défaite : un PNJ retiré par un script de l'hôte n'est plus recréé d'après des états en retard, et l'hôte sans PNJ envoie un relevé vide. Jeu automatique, nouvelles priorités : ranimer, hypo pour le soin et pour l'aura, aller prendre une arme chargée et ses munitions, tirer de loin sans toucher un coéquipier, corps à corps sans arme ou dès qu'un ennemi est au contact |
| 0.31 | Protocole 20 inchangé. Cinématiques : le mod ne prend plus pour notre joueur le personnage que suit le script (tenues mélangées dans l'usine) ; `ai2_takecontrol` suit l'hôte dans l'ordre de ses `chr_focus` (joueur d'un client figé après la cinématique), films et marches de Konoko au moment de l'hôte ; scripts des quatorze niveaux passés en revue. Entrepôt qui ne démarrait pas après sa cinématique : dans les zones que lisent les scripts, tous les joueurs comptent pour un. Jeu automatique : arme la plus forte qui serve, arme plus faible en pénurie, jamais celle de Barabas, munitions des deux sortes, hypo vérifié |
| 0.32 | Protocole 20 inchangé. Jeu automatique : ligne de vue par un rayon du moteur avant de tirer, d'aller chercher un objet ou de couper vers l'hôte ; butin choisi une fois et tenu jusqu'au bout, hypos ramassés quand il est blessé ; sprint dès qu'il a du chemin à faire ; plus de course sur place devant un ennemi au sol. Gâchette d'un joueur plus annoncée quand ses touches ne comptent pas (pantin qui tirait sans fin après une cinématique) |
| 0.33 | Protocole 20 inchangé. Jeu automatique : il ramasse enfin les bonus, par la routine du moteur (celui-ci ne donne rien à qui passe dessus, seulement à la touche d'échange d'arme) ; chargeurs, cellules et hypos jusqu'à l'inventaire plein, ce qui lui manque avant le tir et le plein quand aucun ennemi n'est en vue ; arme faible prise quand la forte est vide. Le pantin d'un autre joueur ne ramasse plus de lui-même le bonus voisin de celui que ce joueur prend (bonus perdu pour tous). Le journal du lancement précédent est gardé dans `onimp_journaux`, avec le `debugger.txt` du moteur |
| 0.34 | Protocole 20 inchangé. Jeu automatique : le butin passe avant le combat ; l'arme est sortie devant un ennemi, rangée une seconde après le dernier (il n'y a de sprint que sans arme en main) et rangée aussitôt quand elle est vide et sans recharge ; armé, il n'avance plus vers sa cible et recule quand elle approche, après avoir regardé derrière lui ; il ne recharge que l'arme vide ; il ramasse l'arme de Barabas. Réanimation : le joueur à terre et les autres joueurs voient la barre se remplir, comme celui qui ranime. La place d'un bonus qui tombe encore est suivie jusqu'au ramassage |
| 0.35 | Protocole 21. Jeu automatique au corps à corps seulement : plus de tir, plus d'arme sortie ni ramassée, plus de munitions cherchées. Trois onglets de plus dans le menu F1, réservés à l'hôte et aux clients qu'il autorise : `Cheat` (interrupteur général de l'hôte, invincibilité et munitions illimitées devenues propres à chaque joueur, coup fatal, aura permanente, vitesse d'attaque, vitesse de déplacement, super saut sans dégâts de chute), `Character` (incarner n'importe quelle classe de personnage, touche F5 pour la suivante) et `Spawn` (objets, armes, PNJ alliés ou ennemis devant soi) |
| 0.36 | Protocole 22. Défenses du niveau décidées par l'hôte, dans tous les niveaux : ses lasers détectent tous les joueurs (ils ne testaient que le sien), ceux des clients n'en détectent plus, lasers et tourelles s'allument et s'éteignent chez tous comme chez lui, les ennemis des environs sont prévenus du joueur détecté. Alarmes (`ai2_tripalarm`, console dont un ennemi se sert) : une vague de trois ennemis du niveau par joueur présent, menés par le calcul de chemin du moteur. Un client resté dans un autre niveau que l'hôte charge de lui-même celui de l'hôte |
| 0.37 | Protocole 23. Tourelles : leur cible est choisie par l'hôte, parmi les joueurs debout, et dite aux clients ; chaque machine fait viser le même joueur aux siennes, et c'est sur la machine de ce joueur que leurs tirs le blessent. En 0.36 chaque machine visait de son côté le pantin de l'autre joueur : les tourelles tiraient sur le client chez l'hôte sans le blesser, et ne lui tiraient pas dessus chez lui |
| 0.38 | Protocole 24. Tourelles d'un client : elles tirent enfin. Le moteur interdit le tir aux tourelles tant que « toutes les IA » sont passives (`ai2_allpassive`), ce qu'un client du mod impose chez lui en permanence pour ses copies des PNJ ; elles lisent maintenant un drapeau du mod, qui chez un client est celui de l'hôte. Laser coupé chez l'hôte annoncé à l'instant aux clients, qui rejouent son son et ses événements (script, alarme, portes, consoles) ; alarme d'un laser ou d'une console comptée pour les vagues ; lignes de journal qui disent quelle tourelle tire sur qui, et si notre joueur est touché |
| 0.39 | Protocole 25. Lasers, toutes salles et tous niveaux : chaque machine détecte son propre joueur, sur les rayons qu'elle affiche, et l'annonce aux autres, qui lancent chez elles ce que le laser déclenche ; l'hôte ne teste plus le pantin des clients (presque tous les lasers glissent sur un rail, que chaque machine fait avancer pour son compte : le client traversait le rayon qu'il voyait sans rien déclencher). Rayons des lasers mobiles d'un client recalés sur ceux de l'hôte. Tourelles : la cible n'est plus le joueur le plus proche à vol d'oiseau (une tourelle suivait l'hôte derrière un mur sans tirer et ignorait le client qui venait de couper son laser) mais d'abord celui qui a coupé le laser, puis le plus proche de ceux qu'elle peut toucher d'après le test du moteur ; plus d'hésitation entre deux joueurs ; chez le client qui coupe le laser, sa tourelle le vise sans attendre l'hôte. Lasers à plusieurs rayons, que le moteur redéclenche à chaque image : quatre annonces par seconde au plus, une ligne de journal toutes les deux secondes. Journal : une ligne par maillon, du laser coupé au dégât, avec la raison du choix de l'hôte. Relevé hors jeu des 291 lasers et 64 tourelles des quatorze niveaux |
