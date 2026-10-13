#pragma once

#include <cstdint>
#include <string>

#include "difficulty.h"

namespace cfg {

enum class Mode { Off, Host, Client };

struct Config {
    Mode mode = Mode::Off;
    std::string host = "127.0.0.1";            // client : adresse de l'hôte, ou "auto" pour le chercher
    uint16_t port = 27777;
    std::string gameName = "Oni";              // hôte : nom de la partie annoncée sur le réseau
    std::string joinName;                      // client en recherche : ne rejoint que la partie de ce nom
    std::string netInterface = "auto";         // auto | radmin | any | a.b.c.d (voir net::Configure)
    int rate = 30;                             // envois par seconde
    std::string playerClass = "konoko_generic";
    // Notre pseudo (vide : aucun) et notre tenue (numéro de roster.h, négatif : au choix de l'hôte).
    // Gardés à part pour l'hôte et pour le client, qui partagent ce fichier sur une même machine.
    std::string playerName;
    int playerSkin = -1;
    std::string windowTitle;                   // titre à donner à la fenêtre du jeu ; vide : celui du jeu
    bool npcs = false;                         // phase B : PNJ pilotés par l'hôte
    bool quests = false;                       // scripts de niveau décidés par l'hôte (implique npcs)
    bool deleteOrphans = true;                 // client : supprime les PNJ absents chez l'hôte
    bool debug = false;                        // trace 1 Hz de l'état du joueur local
    bool multiInstance = true;                 // lève le contrôle "une seule instance" d'Oni
    bool onlineDamage = true;                  // hôte : PNJ qui frappent plus fort à mesure qu'il y a de clients
    uint8_t difficulty = 1;                    // hôte : mode de difficulté (rules::Mode), normal par défaut
    int enemies = 1;                           // hôte : multiplicateur d'ennemis choisi, de 1 à 4
    bool civilians = true;                     // hôte : des civils se mêlent aux ennemis
    bool runInMenus = true;                    // en session, le jeu continue derrière les menus
    bool joinReload = true;                    // hôte : à l'arrivée d'un joueur, reprise commune au dernier point de sauvegarde
    bool readyCheck = true;                    // après un chargement, la partie attend que tous aient chargé
    bool showPlayerHealth = true;              // jauge de vie au-dessus de l'autre joueur
    bool showNpcHealth = true;                 // jauges de vie au-dessus des PNJ
    bool showMessages = true;                  // messages du mod à l'écran, en haut à droite
    bool showNpcIds = true;                    // nom de chaque PNJ au-dessus de lui
    bool showWeaponIds = true;                 // nom de la classe de chaque arme au sol au-dessus d'elle
    bool showPlayerAmmo = true;                // chargeurs et cellules de chaque autre joueur au-dessus de sa jauge
    bool showPing = true;                      // latence de chaque autre joueur à côté de son pseudo
    std::string language = "fr";               // langue des textes du mod à l'écran : fr ou en (voir lang.h)
    bool showThroughWalls = true;              // jauge, pseudo et panneau « à terre » des autres joueurs vus à travers les murs
    int alertVolume = 50;                      // son joué quand un joueur tombe à terre, de 0 à 100 ; 0 : muet
    bool lowPortraits = true;                  // portraits des dialogues en bas de l'écran, au-dessus des sous-titres
    bool autoPlay = false;                     // client : son personnage est confié au jeu automatique
    // Hôte : règles de la partie changées dans l'onglet Coop, reprises au lancement suivant.
    bool teamKill = false;                     // les joueurs peuvent se blesser
    // Hôte : interrupteur général des cheats, et clients à qui il ouvre les onglets Cheat,
    // Character et Spawn (un bit par numéro de joueur).
    bool cheats = false;
    int cheatAccess = 0;
    // Cheats de ce joueur, qui n'agissent que sur son personnage. À chacun les siens : [cheats]
    // pour l'hôte, [cheats_client] pour un client.
    bool cheatGod = false;                     // invincible
    bool cheatAmmo = false;                    // son arme en main se regarnit toute seule
    bool cheatOneHit = false;                  // ses coups et ses tirs tuent un ennemi d'un coup
    bool cheatAura = false;                    // aura du personnage surpuissant, en permanence
    int cheatAttackSpeed = 100;                // vitesse de ses animations d'attaque, en pour cent
    int cheatMoveSpeed = 100;                  // vitesse de ses déplacements, en pour cent
    int cheatJump = 100;                       // force de son saut, en pour cent ; au-delà de 100, plus de dégâts de chute
    std::string character;                     // classe du personnage incarné ; vide : Konoko
    std::string characterKey = "F5";           // touche « personnage suivant »
};

Config& Get();

// Lit onimp.ini (à côté d'Oni.exe) puis applique la variable d'environnement ONIMP.
void Load();
// Réécrit dans onimp.ini les réglages d'affichage, changés en jeu, langue comprise. L'hôte et le
// client d'une même machine partagent ce fichier : l'hôte écrit sous [display], un client sous
// [display_client], et aucun ne défait ce que l'autre a réglé.
void SaveDisplay();
// Réécrit dans onimp.ini le volume de l'alerte : [audio] pour l'hôte, [audio_client] pour un client.
void SaveAudio();
// Réécrit dans onimp.ini le jeu automatique d'un client ([game_client]).
void SaveAutoPlay();
// Réécrit dans onimp.ini notre pseudo et notre tenue, sous les clés du rôle de cette instance.
void SavePlayer();
// Réécrit dans onimp.ini les règles de partie de l'hôte, changées en jeu : dégâts en ligne, mode
// de difficulté, multiplicateur d'ennemis, civils, tir ami, interrupteur général des cheats et
// accès donnés aux clients.
void SaveRules();
// Réécrit dans onimp.ini les cheats de ce joueur, le personnage qu'il incarne et sa touche :
// [cheats] pour l'hôte, [cheats_client] pour un client.
void SaveCheats();
// Client : n'y réécrit que le mode de difficulté, celui qu'il tient de l'hôte. Le reste est à
// l'hôte, qui peut partager ce fichier quand tout tourne sur une même machine.
void SaveDifficultyMode();
// Fichier de difficulté de l'hôte, à côté d'Oni.exe : vie et dégâts des PNJ selon le nombre de
// clients. Une valeur absente garde sa valeur d'origine, une valeur hors bornes y est ramenée.
// found : le fichier existe.
std::string DifficultyPath();
difficulty::Table LoadDifficulty(const char* path, bool* found);
// Date de dernière écriture du fichier, 0 s'il n'existe pas : elle change quand on l'enregistre.
uint64_t DifficultyStamp(const char* path);
// Écrit les six valeurs dans le fichier, réglées en jeu. Ses commentaires sont gardés ; un fichier
// absent est créé.
void SaveDifficulty(const char* path, const difficulty::Table& table);

// "host", "host:27777", "connect:192.168.1.10", "connect:192.168.1.10:27777", "off".
// "join", "join:27777" ou "connect:auto" : client qui cherche lui-même l'hôte sur le réseau.
bool ParseSpec(const char* spec, Config& out);

// Dossier d'Oni.exe, avec le séparateur final.
const std::string& GameDir();

const char* ModeName(Mode mode);

}  // namespace cfg
