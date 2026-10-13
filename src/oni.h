// Oni.exe (PC, build du 2000-11-04, TimeDateStamp 0x3A036F03) : adresses et offsets.
// Sources : symboles DWARF de Daodan 4.1 (binkw32.dll) et désassemblage d'Oni.exe.
// Toutes les fonctions du moteur sont en __fastcall (ecx, edx, puis pile nettoyée par l'appelé),
// sauf mention contraire.
#pragma once

#include <cstddef>
#include <cstdint>

#define ONICALL __fastcall

// Le moteur (MSVC 6) n'aligne la pile que sur 4 octets : toute fonction qu'il appelle doit la
// réaligner avant d'exécuter du code GCC.
#if defined(__GNUC__)
#define ONIMP_ENTRY __attribute__((force_align_arg_pointer))
#else
#define ONIMP_ENTRY
#endif

namespace oni {

struct Vector3 {
    float x, y, z;
};

// Types opaques : on n'accède aux champs que par offset (voir namespace off).
struct GameState;
struct Character;
struct ActiveCharacter;

template <typename T>
inline T& field(void* base, size_t offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

constexpr uint32_t Tag(char a, char b, char c, char d) {
    return (uint32_t(uint8_t(a)) << 24) | (uint32_t(uint8_t(b)) << 16) | (uint32_t(uint8_t(c)) << 8) |
           uint32_t(uint8_t(d));
}

// ---------------------------------------------------------------------------------------------
// Adresses
// ---------------------------------------------------------------------------------------------
namespace addr {
// Même contrôle de version que Daodan (DllMain) : *(u32*)0x51ACD0 == 0x09D36852.
constexpr uintptr_t kExeCheck = 0x0051ACD0;
constexpr uint32_t kExeCheckValue = 0x09D36852;

constexpr uintptr_t kGameStatePtr = 0x005ECE7C;  // GameState**
// Code d'Oni.exe : de sa première fonction à ses constantes.
constexpr uintptr_t kCodeBegin = 0x00401000;
constexpr uintptr_t kCodeEnd = 0x00529000;

// Console (reproduit DDrConsole_Print de Daodan).
constexpr uintptr_t kConsoleLines = 0x005CB468;   // zone de texte de la console
constexpr uintptr_t kConsoleColor = 0x00533F70;
constexpr uintptr_t kConsoleShadow = 0x00533F74;
constexpr uintptr_t kConsoleFadeTime = 0x00533F68;

// Sites d'appel redirigés (opcode E8 rel32).
constexpr uintptr_t kTickCallSite = 0x004D4746;      // call 0x483BD0, une fois par frame en jeu
constexpr uintptr_t kTickCallTarget = 0x00483BD0;
// Boucle des dialogues modaux (0x4689D0) : menu de pause, menu principal ouvert en jeu, écran
// d'une console de données. La boucle du jeu (0x4D4640) ou sa simulation attend qu'elle rende la
// main. À chaque image elle trace le dialogue, par-dessus la scène du jeu, puis appelle ici la mise
// à jour des fenêtres.
constexpr uintptr_t kModalUpdateCallSite = 0x00468AD4;
constexpr uintptr_t kModalUpdateTarget = 0x00477070;
constexpr uintptr_t kGameInitCallSite = 0x004D345A;  // redirigé par Daodan vers DDrGame_Init
// Niveau perdu : appel de l'écran d'échec, juste avant le rechargement (voir off::kGsOutcome).
constexpr uintptr_t kLoseSplashCallSite = 0x004D48D9;
constexpr uintptr_t kSplashTarget = 0x004F4D40;
// Le même écran, appelé à la fin de chaque chargement de niveau (« intro_splash_screen ») et à la
// fin d'un niveau gagné (« win_splash_screen »).
constexpr uintptr_t kIntroSplashCallSite = 0x005068BE;
constexpr uintptr_t kWinSplashCallSite = 0x004D4914;

// Entrée de fonction détournée (saut posé sur ses premières instructions).
// bool (Character* a, Character* b, bool viaKnowledge) : a peut-il s'en prendre à b ? Toujours
// faux si a est passif. Décide entre autres du rang d'une cible : menace ou simple présence.
constexpr uintptr_t kPotentiallyHostile = 0x0048B1D0;
// push ebx ; push ebp ; push esi ; mov esi, ecx : indépendant de l'adresse, donc recopiable.
constexpr uint8_t kPotentiallyHostileEntry[5] = {0x53, 0x55, 0x56, 0x8B, 0xF1};
// Tous les dégâts infligés à un personnage passent par cette fonction (voir fn::TakeDamage).
constexpr uintptr_t kTakeDamage = 0x004E65F0;
// sub esp, 0x2C ; push ebx ; push ebp
constexpr uint8_t kTakeDamageEntry[5] = {0x83, 0xEC, 0x2C, 0x53, 0x55};

// float (uint32_t owner) : imprécision ajoutée aux tirs d'un personnage. Pour une IA, lue dans
// l'état de son comportement en cours (combat, patrouille) sans vérifier qu'il a prévu de tirer.
constexpr uintptr_t kShooterInaccuracy = 0x0051CED0;
// push ecx ; push esi ; mov esi, ecx ; mov eax, esi
constexpr uint8_t kShooterInaccuracyEntry[6] = {0x51, 0x56, 0x8B, 0xF1, 0x8B, 0xC6};

// void (Character*, ActiveCharacter*) : pose du personnage pour le tick. Animation, puis écran de
// visée d'après kAcHeadFacing et kAcHeadPitch. Appelée juste après le calcul de la visée
// (0x4E048B) et juste avant celui des matrices, qui oriente l'arme d'après les mêmes champs.
constexpr uintptr_t kBuildPose = 0x004E5360;
// sub esp, 0x14 ; push ebx ; push ebp
constexpr uint8_t kBuildPoseEntry[5] = {0x83, 0xEC, 0x14, 0x53, 0x55};

// int (Contact* a, Contact* b, bool keepCurrent, Character* owner) : ordre de deux contacts dans
// ce que sait une IA. Négatif : a passe devant ; positif : b. Sert à trier ses contacts, à
// choisir sa cible, et à décider d'en changer (0x490777 : a est alors la cible actuelle,
// keepCurrent vaut 1, et seul un résultat positif fait changer).
constexpr uintptr_t kCompareContacts = 0x0049BA10;
// push ecx ; push ebx ; push ebp ; push esi ; push edi
constexpr uint8_t kCompareContactsEntry[5] = {0x51, 0x53, 0x55, 0x56, 0x57};

// void (Character* attacker, Character* target, ...) : un coup de mêlée d'attacker atteint target.
// Sans effet si les deux équipes sont amies (0x4E7779), sauf pour quelques animations. Ensuite :
// parade, dégâts (fn::TakeDamage), animation imposée à la cible.
constexpr uintptr_t kMeleeHit = 0x004E7740;
// sub esp, 0xCC
constexpr uint8_t kMeleeHitEntry[6] = {0x81, 0xEC, 0xCC, 0x00, 0x00, 0x00};
// Character* (Character* attacker, void* out, float, float, float, bool forThrow) : personnage en
// face d'attacker, pour une prise ou pour se tourner vers lui. Écarte les équipes amies (0x4EA314)
// et, pour une prise, les personnages invincibles (0x4EA371).
constexpr uintptr_t kFindTarget = 0x004EA2B0;
// sub esp, 0x24 ; fld dword ptr [esp+0x30]
constexpr uint8_t kFindTargetEntry[7] = {0x83, 0xEC, 0x24, 0xD9, 0x44, 0x24, 0x30};

// Affichage de mise au point au-dessus des personnages (variables ai2_shownames et
// ai2_showhealth) : appelé pour chaque personnage actif pendant le rendu de la scène.
constexpr uintptr_t kOverheadCallSite = 0x004E17F6;  // call 0x48C5F0
constexpr uintptr_t kOverheadTarget = 0x0048C5F0;    // void (Character*)
// Caméra de la scène en cours de rendu : celle que le moteur interroge au début de l'affichage des
// personnages (0x4E1554). Voir fn::CameraView.
constexpr uintptr_t kSceneCameraPtr = 0x00631004;    // Camera**

// Menu de pause (F1) : dialogue 0x46 ouvert en 0x5018C0. L'adresse de sa fonction de messages
// est l'opérande d'un « push imm32 » en 0x501932.
constexpr uintptr_t kPauseCallbackOperand = 0x00501933;
constexpr uintptr_t kPauseCallback = 0x00501990;

// Dernier point de sauvegarde atteint (variable de script save_point) et état du joueur
// enregistré à ce point par save_game. Lus par le chargement de niveau et par restore_game.
constexpr uintptr_t kContinueSavePoint = 0x005ECE88;  // int
constexpr uintptr_t kContinueData = 0x005ECC10;
constexpr size_t kContinueSize = 0x81 * 4;

// Mise à jour des zones de déclenchement : l'adresse de la fonction appelée pour chaque zone est
// l'opérande d'un « mov edx, imm32 » en 0x4DCD00. Une zone « joueur seulement » y écarte tout
// personnage dont le type n'est pas celui du joueur (0x4E0DD7).
constexpr uintptr_t kTriggerCallbackOperand = 0x004DCD01;
constexpr uintptr_t kTriggerCallback = 0x004E0CF0;
// Commande de script trigvolume_count (0x4BA400) : son appel à la fonction qui compte les
// personnages présents dans les zones d'un numéro donné (0x4CD360, numéro dans ecx).
constexpr uintptr_t kTriggerCountCallSite = 0x004BA407;
constexpr uintptr_t kTriggerCountTarget = 0x004CD360;
// À la frame d'action d'une animation « ramasser un objet » (types 0x9D et 0xE3), le moteur cherche
// le bonus du sol que le personnage peut prendre, à seize unités au plus, et le lui donne.
constexpr uintptr_t kPickupSearchCallSite = 0x004DDD07;  // call 0x51B3E0
constexpr uintptr_t kPickupSearchTarget = 0x0051B3E0;

// int () : difficulté choisie dans les options du jeu, 0 facile, 1 normal, 2 difficile. Lue par le
// moteur chaque fois qu'il en a besoin : vie d'un personnage à sa création (0x4DB352), attention
// et parades des IA (0x48B935, 0x4A06C7), précision de leurs tirs (0x51CFCB).
constexpr uintptr_t kGetDifficulty = 0x0050F240;
// mov eax, ds:0x5ED388 : adresse absolue, donc recopiable.
constexpr uint8_t kGetDifficultyEntry[5] = {0xA1, 0x88, 0xD3, 0x5E, 0x00};
// Réglages du jeu (instance ONGS) : ce que chaque difficulté multiplie.
constexpr uintptr_t kGameSettingsPtr = 0x005ECBFC;   // ONGS**

// Commande de script text_console : l'écran de texte plein cadre d'une console de données, ou
// d'une consigne du niveau. Elle ouvre son dialogue (0x47, boucle 0x4689D0) elle-même, et ne rend
// la main au script qu'à sa fermeture : la boucle des dialogues tourne alors à l'intérieur de
// l'interpréteur de scripts, lui-même à l'intérieur de la simulation.
constexpr uintptr_t kTextConsole = 0x00504C30;
// sub esp, 8 ; cmp edx, 1
constexpr uint8_t kTextConsoleEntry[6] = {0x83, 0xEC, 0x08, 0x83, 0xFA, 0x01};

// Affichage des lignes de la console, une fois par image, parmi le reste de l'interface du jeu
// (0x4FBDE3 : messages du niveau ; ici : console). Les messages du mod sont tracés à sa suite.
constexpr uintptr_t kConsoleDisplayCallSite = 0x004FBDE8;  // call 0x4303E0
constexpr uintptr_t kConsoleDisplayTarget = 0x004303E0;
// Zones de texte du moteur, libérées une à une à la fermeture du jeu (0x42FA60, appelée d'ici) :
// ligne de commande, lignes de la console, messages du niveau (celle où s'écrit « autosave » à un
// point de sauvegarde, 0x4304B0), sous-titres. Le moteur y libère les lignes de chacune sans
// regarder s'il y en a (0x42FAF6), et sa libération lit quatre octets avant le pointeur (0x424B30).
// Affichage des messages du niveau, juste avant celui de la console (0x4FBDE3) : la zone des
// sous-titres si elle a des lignes, sinon celle des messages (0x430400).
constexpr uintptr_t kLevelMessagesCallSite = 0x004FBDE3;     // call 0x430400
constexpr uintptr_t kLevelMessagesTarget = 0x00430400;
// Historique des lignes de commande exécutées (0x4317D0) : 256 lignes de 512 octets, de
// kCommandHistory à 0x5CB388, leur nombre, et le rang de la dernière. Le moteur a une faute d'un
// cran : plein, il décale tout d'une ligne puis écrit la nouvelle à la case 256, la première au-delà
// du tableau (0x4318AF), c'est-à-dire sur ce qui le suit en mémoire : le drapeau « ligne de commande
// ouverte » (0x5CB396) et le pointeur de la zone des messages du niveau (0x5CB398). Les deux
// comparaisons à 256 (opérandes en kHistoryLimitA et kHistoryLimitB) sont ramenées à 255.
constexpr uintptr_t kCommandHistory = 0x005AB388;
constexpr size_t kHistoryLineSize = 0x200;
constexpr int32_t kHistoryCapacity = 0x100;
constexpr uintptr_t kHistoryCount = 0x005CB480;      // int
constexpr uintptr_t kHistoryLast = 0x00533F64;       // int
constexpr uintptr_t kHistoryLimitA = 0x00431884;     // cmp dword [kHistoryCount], 0x100
constexpr uintptr_t kHistoryLimitB = 0x004318D0;     // cmp eax, 0x100
// Activation d'une console par un personnage (voir fn::ConsoleActivate) : sub esp, 0xC ; push esi ;
// push edi.
constexpr uintptr_t kConsoleActivate = 0x004C0880;
constexpr uint8_t kConsoleActivateEntry[5] = {0x83, 0xEC, 0x0C, 0x56, 0x57};
// Affichage des personnages (0x4E15xx) : pour chacun, ce test dit s'il est en vue ; sinon il n'est
// pas tracé. fastcall (personnage, volume de vue), rend un octet.
constexpr uintptr_t kVisibleCallSite = 0x004E16A4;  // call 0x4E27D0
constexpr uintptr_t kVisibleTarget = 0x004E27D0;
// Portraits des dialogues (« cinematic insert ») : cinematic_start et cinematic_stop appellent ces
// deux fonctions avec une place de départ et une place d'arrivée, de 0 à 25. Les places 1 à 9 sont
// à l'écran, en trois rangées de trois (1 en haut à gauche, 9 en bas à droite) ; les autres sont
// hors de l'écran, autour : 10 à 14 au-dessus, 15 et 16 de part et d'autre de la rangée du haut,
// 17 et 18 de celle du milieu, 19 et 20 de celle du bas, 21 à 25 dessous (table en 0x551848, dans
// un écran de 1024 x 728 ; la rangée du bas s'arrête 65 points au-dessus du bord, où sont les
// sous-titres).
constexpr uintptr_t kInsertStartCallSite = 0x004F38BA;  // call 0x4F2BE0
constexpr uintptr_t kInsertStartTarget = 0x004F2BE0;
constexpr uintptr_t kInsertStopCallSite = 0x004F3921;   // call 0x4F30C0
constexpr uintptr_t kInsertStopTarget = 0x004F30C0;
// Chaque tick, le moteur donne au personnage du joueur les touches du moment (0x4ED420, fastcall :
// personnage, GameInput de l'état du jeu).
constexpr uintptr_t kPlayerInputCallSite = 0x004FA934;  // call 0x4ED420
constexpr uintptr_t kPlayerInputTarget = 0x004ED420;
// Mise à jour des personnages, une fois par tick (0x4DCFxx à 0x4DDB9E), en deux boucles sur les
// personnages actifs. La première traite les touches de chacun (fastcall : personnage, état
// actif) : c'est là que la touche de saut tenue ajoute la poussée de la classe (0x4F7978). La
// seconde fait tout le reste (fastcall : personnage) : départ du saut (0x4DEA0F), gravité, pas de
// l'animation rangé dans kAcStep (0x4DF204), image suivante (0x4E00B2). Une troisième boucle donne
// ensuite ce pas à la physique (0x4DDA83), qui déplace le personnage contre le décor.
constexpr uintptr_t kCharacterInputCallSite = 0x004DD068;  // call 0x4F6AD0
constexpr uintptr_t kCharacterInputTarget = 0x004F6AD0;
constexpr uintptr_t kCharacterStepCallSite = 0x004DD5A7;   // call 0x4DDBA0
constexpr uintptr_t kCharacterStepTarget = 0x004DDBA0;
// Laser d'un piège (objet TRIG). C'est en l'affichant que le moteur regarde si l'un de ses rayons
// coupe le personnage du joueur de la partie (GameState+0xAC, appel en 0x4CBA23), et aucun autre :
// à chaque image, pour tous les lasers du niveau, où que regarde la caméra (0x4CB6F0, appelée par
// la boucle des objets 0x4D1250 ; le rendu de la scène, 0x4FB480, tourne aussi derrière un menu).
// S'il le coupe, les événements du laser partent une fois (0x4CBB0A : tourelles, alarmes,
// scripts), et le drapeau kTrigFlagHit reste posé tant qu'il le coupe. Un laser à plusieurs rayons
// dont un seul est coupé lève ce drapeau au rayon suivant (0x4CBB19) : ses événements repartent
// alors à chaque image.
// Le test : fastcall (personnage, départ du rayon) et six arguments sur la pile, rend un octet.
constexpr uintptr_t kLaserTestCallSite = 0x004CBA23;       // call 0x4F1630
constexpr uintptr_t kLaserTestTarget = 0x004F1630;
// Routine qui donne l'alarme (fastcall : numéro de l'alarme, personnage qui l'a causée) : les IA
// du groupe d'alarme (masque en Character+0xFD8) apprennent alors où il est. Deux chemins y
// mènent : la commande de script ai2_tripalarm, par un appel (0x4B5E48), et l'événement « Activate
// Alarm » d'un laser ou d'une console, par un saut (0x4F3DD9). C'est donc son entrée qui est
// détournée, et non un site d'appel. mov al, ds:0x5EC0C4 : adresse absolue, donc recopiable.
constexpr uintptr_t kAlarmTarget = 0x0049AC30;
constexpr uint8_t kAlarmEntry[5] = {0xA0, 0xC4, 0xC0, 0x5E, 0x00};
// Un laser vient d'être coupé (0x4CBAC8) : le moteur joue son son, puis lance ici ses événements
// (fastcall : liste d'événements du laser, personnage). Les événements d'un objet du niveau sont
// de onze genres (table en 0x552098) : script, tourelle allumée ou éteinte, console activée ou
// désactivée, alarme, laser allumé ou éteint, porte verrouillée ou déverrouillée.
constexpr uintptr_t kLaserEventsCallSite = 0x004CBB0A;     // call 0x4F3CC0
constexpr uintptr_t kEventsRun = 0x004F3CC0;
// Mise à jour d'une tourelle allumée (0x4CF140) : toutes les quinze ticks elle cherche sa cible
// ici (fastcall : tourelle, rend un personnage ou nul). La routine prend le plus proche des
// personnages vivants (off::kGsLiving) dont l'équipe est dans le masque de la tourelle, sans
// distinguer un joueur d'un pantin : chaque machine choisissait donc la sienne.
constexpr uintptr_t kTurretTargetCallSite = 0x004CF27C;    // call 0x4CF7E0
constexpr uintptr_t kTurretTargetTarget = 0x004CF7E0;
// Tir d'une tourelle (0x4CF65D) : visée faite, elle ne tire que si le drapeau « toutes les IA
// passives » du moteur est nul ; sinon elle cesse le feu (0x4CF686). C'est le drapeau que règle la
// commande de script ai2_allpassive (0x48B0D4), que les niveaux posent le temps d'une cinématique.
// Or un client du mod le tient posé en permanence, pour que ses copies des PNJ de l'hôte ne
// pensent pas : ses tourelles s'allumaient, visaient, et ne tiraient jamais. L'instruction est un
// « mov al, [adresse] » : son opérande, en kTurretPassiveOperand, est remplacé par l'adresse d'un
// octet à nous.
constexpr uintptr_t kTurretPassiveRead = 0x004CF65D;       // A0 C4 C0 5E 00
constexpr uintptr_t kTurretPassiveOperand = 0x004CF65E;
constexpr uintptr_t kAiAllPassive = 0x005EC0C4;            // u8
// Contenance de l'inventaire : six chargeurs, six cellules, six hypos. La borne est écrite trois
// fois dans la fonction qui donne un bonus (0x51CA30) et trois fois dans celle qui cherche au sol
// le bonus qu'un personnage peut prendre (0x51B3xx) ; les trois sortes passent par le même code,
// où mène une table de sauts par sorte. Chaque adresse est celle de l'opérande, lu sur quatre
// octets avec ce qui le suit.
constexpr uintptr_t kReserveLimits[6] = {0x0051CAA9, 0x0051CAAE, 0x0051CAE9, 0x0051B4C3, 0x0051B4C9, 0x0051B4D1};
constexpr uint32_t kReserveLimitCode[6] = {0x66087C06, 0x36720006, 0x00000006, 0x66437C06, 0x33047206, 0x00000006};
constexpr uintptr_t kGiveHypoEntry = 0x0051CBDC;     // case « hypo » de la table de 0x51CA30...
constexpr uintptr_t kGiveHypoCase = 0x0051CAFA;      // ... qui lit le compte dans ax et saute en kGiveShared
constexpr uintptr_t kGiveShared = 0x0051CA91;
constexpr uintptr_t kSearchHypoEntry = 0x0051B57C;   // la même pour la recherche au sol : compte dans si
constexpr uintptr_t kSearchHypoCase = 0x0051B4E2;
constexpr uintptr_t kSearchShared = 0x0051B4AF;
constexpr uintptr_t kConsoleTerminateCallSite = 0x004D3233;  // call 0x42FA60
constexpr uintptr_t kConsoleTerminateTarget = 0x0042FA60;
constexpr uintptr_t kTextAreas[] = {0x00571B74, 0x005CB468, 0x005CB398, 0x005CB470};

// Touche d'action d'un personnage (0x4F81A8) : d'abord la porte ou la console à sa portée, puis ici
// le PNJ devant lui qui a quelque chose à lui dire (interaction « neutre » : civils, alliés).
constexpr uintptr_t kNeutralActionCallSite = 0x004F81F8;     // call 0x48BB40
constexpr uintptr_t kNeutralActionTarget = 0x0048BB40;

// Tables fixes du moteur.
constexpr uintptr_t kWeapons = 0x006221C0;        // Weapon[64]
constexpr int kMaxWeapons = 64;
constexpr size_t kWeaponSize = 0x170;
constexpr uintptr_t kPowerups = 0x00620460;       // bonus au sol, tassés en tête de table
constexpr uintptr_t kPowerupCount = 0x00622160;   // u16
constexpr int kMaxPowerups = 64;
constexpr size_t kPowerupSize = 0x74;
}  // namespace addr

// ---------------------------------------------------------------------------------------------
// Offsets
// ---------------------------------------------------------------------------------------------
namespace off {
// GameState
constexpr size_t kGsInCutscene = 0x03C;          // u8 : entre begin_cutscene (0x4FD15B) et end_cutscene (0x4FD1E5)
constexpr size_t kGsPlayerCharacter = 0x0AC;     // Character*
constexpr size_t kGsGameTime = 0x144;            // int, ticks 1/60 s
// Entrées d'un tick, telles que la boucle du jeu les donne à la simulation : boutons, puis
// mouvements analogiques. Tout à zéro : aucune touche.
constexpr size_t kActionSize = 0x30;
constexpr int kMaxTicksPerFrame = 6;             // la simulation n'en rattrape jamais plus par image
constexpr size_t kGsLevel = 0x150;               // OniLevel*, name[64] à +0
constexpr size_t kGsCharacterStorage = 0x1260;   // Character[128]
constexpr size_t kGsCharacterCount = 0xB6260;    // short
constexpr size_t kGsIsGamePaused = 0x1679AC;     // char
// Nul : les touches du joueur sont ignorées (0x4F6B2C, 0x4FA6CF). C'est ce que règle la commande
// de script input, et ce que coupent les cinématiques (0x4FD14E).
constexpr size_t kGsInputEnabled = 0x1679AD;     // char
// Nom de l'écran d'accueil du niveau que la boucle du jeu doit montrer (0x4D475C) : posé par la
// commande de script splash_screen, il fige tout jusqu'à l'appui d'une touche. Vide : aucun.
constexpr size_t kGsSplashName = 0x118;          // char[]
// Instant de la machine au dernier passage de l'horloge du jeu (0x4FBEDD) : l'écart avec l'instant
// présent s'ajoute au temps à simuler.
constexpr size_t kGsLastMachineTime = 0x140;     // u32, soixantièmes de seconde
// Issue du niveau, lue par la boucle de jeu (0x4D48C1) : 1 gagné, 2 perdu. Perdu recharge le
// niveau au dernier point de sauvegarde ; c'est ce que pose la commande de script lose.
constexpr size_t kGsOutcome = 0x1679BC;          // int
constexpr int32_t kOutcomeLost = 2;
// Numéro du niveau en cours. C'est lui que la boucle du jeu relit pour recharger après un échec
// (0x4D48EA, puis 0x5059D0) : changé juste avant, c'est un autre niveau qui est chargé, au point
// de sauvegarde et avec l'état du joueur de addr::kContinueSavePoint et kContinueData.
constexpr size_t kGsLevelNumber = 0x1679D8;      // u16
// Personnages vivants et actifs, ceux parmi lesquels une tourelle cherche sa cible (0x4FCA90,
// 0x4FCAA0). Relevé dans la mémoire d'une partie : notre joueur et les pantins des autres, mais
// pas un joueur mort.
constexpr size_t kGsLiving = 0x1672F8;           // Character*[128]
constexpr size_t kGsLivingCount = 0x1674F8;      // u32

constexpr size_t kCharacterSize = 0x16A0;
constexpr int kMaxCharacters = 128;

// Character
constexpr size_t kChHandle = 0x000;              // u16 : index | (0x80|sel) << 8
constexpr size_t kChActiveIndex = 0x002;         // u16, 0 = inactif
constexpr size_t kChFlags = 0x004;               // u32
constexpr size_t kChFlags2 = 0x008;              // u32
constexpr size_t kChClass = 0x00C;               // ONCC*
constexpr size_t kChTeam = 0x012;                // u16
constexpr size_t kChName = 0x014;                // char[32]
constexpr size_t kChHealth = 0x038;              // u32
constexpr size_t kChMaxHealth = 0x03C;           // u32
// Scripts du niveau attachés au personnage, recopiés de son objet CHAR à sa création (0x48A6C8) :
// apparition, mort, combat, alarme, blessure, défaite, plus de munitions, chemin introuvable.
constexpr size_t kChScripts = 0x048;             // 8 noms de 32 caractères
constexpr int kChScriptCount = 8;
constexpr size_t kChScriptSize = 0x20;
constexpr size_t kChPosition = 0x148;            // Vector3
constexpr size_t kChLastPosition = 0x154;        // Vector3
constexpr size_t kChLocation = 0x160;            // Vector3 (position de référence)
constexpr size_t kChFacing = 0x16C;              // float, radians
constexpr size_t kChDesiredFacing = 0x170;       // float
constexpr size_t kChCosmeticFacing = 0x174;      // float
constexpr size_t kChWeaponInHand = 0x194;        // Weapon*, premier emplacement de l'inventaire
constexpr size_t kChWeaponStored = 0x198;        // Weapon* rangée
// Réserves du personnage, six de chaque au plus (0x51CAA7) : chargeurs de balles, hypos, cellules
// d'énergie.
constexpr size_t kChAmmoClips = 0x1A2;           // u16
constexpr size_t kChHypos = 0x1A4;               // u16
constexpr size_t kChCells = 0x1A6;               // u16
constexpr int kMaxReserve = 6;
// Bouclier de force : ce qu'il en reste, 100 pour un bouclier neuf (0x51D007). Tant qu'il en reste
// il arrête les tirs (0x517A92), et le corps luit d'autant (kChShieldShown y monte de deux par
// tick, 0x51BBAF, et règle la lueur, 0x4EF76B).
constexpr size_t kChShieldShown = 0x1B6;         // u16
constexpr size_t kChShield = 0x1B8;              // u16
// Invisibilité : ticks qui restent, 1800 pour une neuve (0x51D018). Tant qu'il en reste le moteur
// compte les ticks passés invisible (0x51BBF8) ; au-delà de 90 les IA ne voient plus le personnage
// (0x49A361).
constexpr size_t kChCloak = 0x1BA;               // u16
constexpr size_t kChCloakFrames = 0x1BC;         // u32
// L'autre personnage d'une interaction « neutre » en cours, nul sinon : posé chez les deux quand le
// PNJ commence à parler (0x4AD3D9), retiré à la fin (0x4AD064).
constexpr size_t kChNeutralPartner = 0x162C;     // Character*
// Ce qu'une IA laisse tomber en mourant (0x4EB724) : nombres de chargeurs, d'hypos et de cellules,
// bouclier, invisibilité, LSI. Rien n'y est remis à zéro une fois lâché.
constexpr size_t kChDrops = 0x1A8;
constexpr size_t kChDropsSize = 0x14;
constexpr size_t kChVelocity = 0x1C4;            // Vector3
// Recul de l'arme : angle ajouté à la visée verticale (0x4E576C). Entretenu par le traitement
// des touches du joueur (0x4F7C7F).
constexpr size_t kChRecoil = 0x1D4;              // float, radians
// Aura du personnage « surpuissant » : la lueur autour de lui et ses coups plus forts. À chaque tick
// (0x4DF88A) le moteur prend le plus grand de kChSuper et, pour une classe qui y a droit, de sa
// santé au-dessus du maximum rapportée à ce maximum (un hypo pris en pleine forme), le borne à 1, et
// y amène peu à peu kChSuperShown, qui règle la lueur.
constexpr size_t kChHasLsi = 0x1B4;              // u8 : porte l'objet du niveau (LSI), lu par chr_has_lsi
// Ce que sait une IA : liste de ses contacts (off::kCt*), du plus important au moindre.
constexpr size_t kChContacts = 0xF7C;            // premier contact, nul si aucun
constexpr size_t kCtNext = 0x54;                 // contact suivant
constexpr size_t kChSuper = 0x1664;              // float : valeur demandée (commande chr_super, 0x4BA6BA)
constexpr size_t kChSuperShown = 0x1668;         // float : valeur du moment, de 0 à 1
constexpr size_t kChType = 0x1E8;                // int, 2 = IA (AI2)
// Ce que fait l'IA en ce moment, et les données de cet état. 8 : interaction « neutre », un PNJ qui
// parle à quelqu'un (0x48BBEA) ; ses données commencent huit octets plus loin (0x4ACDC0).
constexpr size_t kChAiGoal = 0x1F8;              // int
constexpr int32_t kAiGoalNeutral = 8;
constexpr size_t kChAiState = 0xF70;             // pointeur
constexpr size_t kNsBase = 0x08;
constexpr size_t kNsTarget = 0x000;              // Character* : à qui il parle (0x4ACE84)
constexpr size_t kNsGiveWeapon = 0x0BC;          // char[32] : classe de l'arme qu'il donne, « none » sinon (0x4AD9DD)
constexpr size_t kNsGave = 0x296;                // u8 : il a remis ce qu'il avait à donner (0x4ADA49)
constexpr size_t kChAiFlags = 0x1F0;             // u32, état de l'IA
constexpr uint32_t kAiFlagPassive = 0x2;         // posé par ai2_passive
constexpr uint32_t kAiFlagNonCombatant = 0x4;    // ne se bat pas : civil (0x48A93D)
constexpr uint32_t kAiFlagIgnorePlayer = 0x2000; // posé par ai2_makeignoreplayer (0x49ACF3)

// Weapon
constexpr size_t kWpFlags = 0x02;                // u16
constexpr uint16_t kWpFlagInUse = 0x01;
constexpr uint16_t kWpFlagFiring = 0x10;         // gâchette enfoncée (0x51A28B), jusqu'à son relâchement
constexpr size_t kWpClass = 0x04;                // ONWC*
constexpr size_t kWpOwner = 0x08;                // Character*, nul : arme au sol
constexpr size_t kWpAmmo = 0x56;                 // u16, cartouches restantes dans l'arme (0x51A9E7)
constexpr size_t kWpPosition = 0x30;             // Vector3

// Bonus au sol (0x51C160)
constexpr size_t kPuAmount = 0x02;               // u16 : 1, ou la charge d'un bouclier ou d'une invisibilité
constexpr size_t kPuType = 0x04;                 // u32 : 0 chargeur, 1 cellule, 2 hypo, 3 bouclier, 4 invisibilité, 5 LSI
constexpr uint32_t kPuTypeLsi = 5;               // l'objet du niveau, que ses scripts attendent
constexpr uint32_t kPuTypeCount = 6;
constexpr size_t kPuPosition = 0x08;             // Vector3
constexpr size_t kPuPhysics = 0x34;              // nul : posé, ne tombe pas

// Laser (objet TRIG) et tourelle (objet TURR). Un laser éteint ne détecte rien ; une tourelle
// éteinte ne vise ni ne tire. Les commandes de script trig_activate, trig_deactivate,
// turret_activate et turret_deactivate les désignent par leur numéro (voir fn::TriggerActivate).
// Classe du laser (0x74) et, dans elle, le son joué à l'instant où un joueur coupe le rayon
// (0x4CBAEB, puis fn::SoundPlayAt).
constexpr size_t kTrigEvents = 0x90;             // liste d'événements du laser (0x4CBB04)
// Liste d'événements d'un objet du niveau, telle que la parcourt le moteur (0x4F3CD1) : leur
// nombre, puis un pointeur vers des événements de kEventSize octets, genre puis paramètre.
constexpr size_t kEventCount = 0x00;             // u16
constexpr size_t kEventItems = 0x04;             // pointeur
constexpr size_t kEventSize = 0x24;
constexpr size_t kEventType = 0x00;              // u32
constexpr size_t kEventId = 0x04;                // u16 : numéro de la tourelle, de la console, de la porte...
constexpr uint32_t kEventActivateTurret = 2;
constexpr size_t kTrigClass = 0x74;              // pointeur
constexpr size_t kTrigClassHitSound = 0x68;      // pointeur, nul : aucun son
constexpr size_t kTrigId = 0x2C;                 // u16
constexpr size_t kTrigFlags = 0x2E;              // u16
constexpr uint16_t kTrigFlagHit = 0x2;           // le rayon coupe le joueur en ce moment (0x4CBACC)
constexpr uint16_t kTrigFlagActive = 0x4;        // allumé (0x4CB23A)
// Rayons d'un laser. Presque tous glissent le long d'un rail : l'animation de sa classe, de
// kRailFrames images. Chaque rayon y avance de kTrigSpeed images par tick, dans son sens, et
// seulement tant que le laser est allumé (0x4CB19C, puis 0x4CAFC0) ; au bout du rail il fait
// demi-tour (kTrigFlagBounce) ou repart du début (0x4CB05F à 0x4CB0BA). Le moteur ne règle ces
// positions sur aucune horloge : deux machines qui n'ont pas chargé le niveau au même instant, ou
// pas allumé le laser au même tick, n'ont pas leurs rayons au même endroit.
constexpr uint16_t kTrigFlagBounce = 0x20;
constexpr size_t kTrigSpeed = 0x84;              // float, images de rail par tick ; nul : laser fixe
constexpr size_t kTrigRayCount = 0x88;           // u16
constexpr size_t kTrigRays = 0x9C;               // pointeur vers kTrigRayCount rayons de kRaySize octets
constexpr size_t kRaySize = 0x4C;
constexpr size_t kRayFrame = 0x00;               // float : où il en est sur le rail (0x4CB01D)
constexpr size_t kRayDirection = 0x0C;           // i16 : +1 ou -1 (0x4CB004)
constexpr size_t kTrigClassRail = 0x20;          // pointeur, nul : classe sans rail
constexpr size_t kRailFrames = 0x72;             // u16
constexpr size_t kTurretId = 0x2C;               // u16
// Une tourelle qu'on allume le reste le temps que dit sa classe (TURR+0x600, compté en 0x4CF245 :
// quinze secondes pour la plupart, trois pour celles à arc au mercure, deux pour celles à canon
// hurleur), revient au repos et s'éteint. L'allumer pendant ce temps ne change rien (0x4CD9DA) :
// rester dans le laser ne la retient pas. Une tourelle murale ne tourne que de 70 degrés de part
// et d'autre, et de -45 à +20 degrés en hauteur (TURR+0x604 à 0x610) ; les autres font tout le tour.
constexpr size_t kTurretState = 0x30;            // u16 : 0 éteinte, 1 allumée (0x4CD9DA, 0x4CDAC1)
constexpr size_t kTurretFlags = 0x2E;            // u16
constexpr uint16_t kTurretFlagFiring = 0x4;      // elle tire : posé au tir, levé quand elle cesse le feu (0x4CF69D)
constexpr size_t kTurretTarget = 0x270;          // Character* : sa cible du moment, nul si aucune (0x4CF283)
// Une tourelle prend pour cible le plus proche des personnages dont l'équipe est dans ce masque
// (0x4CF7E0), joueur ou non : le pantin d'un autre joueur compte comme le nôtre.
constexpr size_t kTurretTeams = 0x78;            // u32, un bit par équipe
// Zone de déclenchement (objet TRGV) : son numéro (0x4CD380) et les équipes qu'elle compte (0x4DCB82).
constexpr size_t kTrgvId = 0x158;                // u32
constexpr size_t kTrgvTeams = 0x2E4;             // u32, un bit par équipe
// Objets de niveau (OBJtObject) : données propres à +0x2C.
constexpr size_t kDoorId = 0x2C;                 // u16
constexpr size_t kDoorState = 0x38;              // u16, 1 = ouverte (test de OBJrDoor_Open)
constexpr size_t kConsoleId = 0x2C;              // u16
constexpr size_t kConsoleFlags = 0x2E;           // u16
constexpr uint16_t kConsoleTriggered = 0x2;      // posé par OBJrConsole_OnActivate
constexpr uint16_t kConsoleActive = 0x4;         // sans lui, la console refuse de servir (0x4C08EA)

constexpr uint32_t kChFlagDead = 0x0001;
// Un coup mortel ne lui laisse qu'un point de vie (0x4E6921) et le jette au sol (0x4E6E83) ; seule
// une mise à mort par script passe. Posé par chr_unkillable.
constexpr uint32_t kChFlagUnkillable = 0x0020;
constexpr uint32_t kChFlagInfiniteAmmo = 0x0040;  // l'arme en main est regarnie à chaque tir (0x51A99B)
constexpr uint32_t kChFlagInvincible = 0x0100;    // posé par chr_invincible ; lu par les dégâts (0x4E6657)
constexpr uint32_t kChFlagInUse = 0x8000;
constexpr uint32_t kChFlagActiveLock = 0x100000;  // posé par chr_lock_active : jamais mis en sommeil (0x4B9540)
constexpr uint32_t kChFlagScriptedAnim = 0x200000;  // posé par chr_animate tant que l'anim est forcée
// Posé par ai2_takecontrol 1 sur le personnage du joueur, retiré par ai2_takecontrol 0 (0x4B8940) :
// tant qu'il l'est, ses déplacements sont ceux que lui dicte l'IA, et ses touches n'y font rien.
// La commande vise le personnage que chr_focus désigne à l'instant où elle s'exécute.
constexpr uint32_t kChFlagAiMovement = 0x400000;
constexpr uint32_t kChFlagBoss = 0x10000000;      // objet CHAR marqué comme boss (0x48A77B)
// Flags2. Posé à l'appui du tir quand l'arme en main est vide et sans chargeur (0x4F6CBA) : tant
// qu'il l'est, le moteur relâche la gâchette et la touche de tir donne un coup de poing.
constexpr uint32_t kChFlag2FireIsMelee = 0x1;
constexpr int kChTypePlayer = 0;
constexpr int kChTypeAI2 = 2;

// ActiveCharacter
constexpr size_t kAcPhysics = 0x0004;            // PhyContext*
// Déplacement du tick : le pas de l'animation tourné selon l'orientation, plus chute, saut et
// poussée. Écrit par la mise à jour du personnage (0x4DF204), donné ensuite à la physique.
constexpr size_t kAcStep = 0x00C4;               // Vector3
constexpr size_t kAcInAir = 0x00F6;              // u16 : ticks passés en l'air, 0 au sol
constexpr size_t kAcFallVelocity = 0x00F8;       // Vector3 : y monte au saut, la gravité l'use
// Temps de récupération après une attaque, en ticks : tant qu'il en reste, pas de nouvelle attaque
// (0x4E44DE, 0x4E4500). Posés à la fin de l'animation, usés d'un par tick (0x4DE2DB).
constexpr size_t kAcSoftPause = 0x1C8A;          // u16
constexpr size_t kAcHardPause = 0x1C8C;          // u16
// Ticks pendant lesquels le personnage encaisse un coup, ou le pare (noms des symboles de Daodan).
constexpr size_t kAcHitStun = 0x1C94;            // u16
constexpr size_t kAcBlockStun = 0x1C96;          // u16
constexpr size_t kAcActionsDownLow = 0x2168;     // u32 : moitié basse des actions tenues (saut : 0x8000000)
// Élans du personnage, des vecteurs de flottants : poussée d'un coup (0xB8), pas de l'animation
// (0xC4), 0xD4, 0xF0, chute (0xF8), saut (0x104). Relevés « NaN » dans la mémoire des clients figés
// de la 0.28 ; les champs qui les séparent sont des entiers.
constexpr size_t kAcMotion[][2] = {{0xB8, 0xD0}, {0xD4, 0xE0}, {0xF0, 0xF4}, {0xF8, 0x110}};
constexpr size_t kAcAnimation = 0x1AF8;          // TRAM*
constexpr size_t kAcAnimToState = 0x1AFC;        // short
constexpr size_t kAcAnimFromState = 0x1AFE;      // short
// États d'animation : au sol, sur le dos ou sur le ventre (0x4E6E34).
constexpr int16_t kAnimStateFallenBack = 18;
constexpr int16_t kAnimStateFallenFront = 50;
constexpr uint32_t kAnimTypeStand = 6;           // animation de repos d'un état
constexpr size_t kAcStitching = 0x1B08;          // char, fondu entre deux animations en cours
constexpr size_t kAcInterpLength = 0x1B22;       // short, durée du fondu en frames
constexpr size_t kAcAnimFrame = 0x1C88;          // short
// Animation jouée par-dessus l'animation principale : rechargement, arme rangée ou sortie.
constexpr size_t kAcOverlay = 0x1C64;            // {TRAM*, short frame, int flags}
// Visée demandée par l'IA. Chaque tick, l'« exécuteur » en tire kAcIsAiming et kAcAimTarget
// (0x498F50) ; sans demande, il remet kAcIsAiming à zéro, même pour une IA passive. Il ignore une
// direction à plus de 130 degrés de l'orientation du corps.
constexpr size_t kAcExecAiming = 0x1A88;         // char : une visée est demandée
constexpr size_t kAcExecStopAiming = 0x1A89;     // char : revenir droit devant, puis cesser
constexpr size_t kAcExecAimAtPoint = 0x1A8A;     // char : kAcExecAim est un point, sinon une direction
constexpr size_t kAcExecAimWeaponSpeed = 0x1A8B; // char : vitesse pondérée par l'arme
constexpr size_t kAcExecAim = 0x1A8C;            // Vector3
constexpr size_t kAcExecAimSpeed = 0x1AB4;       // float, angle parcouru par tick (à un facteur près)
constexpr size_t kAcVariantFlags = 0x1CA0;       // u16, variantes d'animation ; 0x7800 : arme en main
// > 0 : en fin d'animation le moteur rejoue la même au lieu d'en choisir une (0x4E2ED1).
constexpr size_t kAcForcedAnimFrames = 0x1C90;   // int
// Projection (prise). Le moteur choisit cible et animation de la cible (0x4EAA10), puis les lit
// au moment où l'animation du lanceur est posée (0x4E9B89) : il impose alors son animation à la
// cible et la place devant le lanceur.
constexpr size_t kAcThrowing = 0x1ED8;           // short : numéro de la cible une fois projetée, -1 sinon
constexpr size_t kAcThrownBy = 0x1EDA;           // short : numéro du lanceur
constexpr size_t kAcThrowTarget = 0x1EDC;        // Character*
constexpr size_t kAcTargetThrow = 0x1EE0;        // TRAM* imposée à la cible
// Actions maintenues ce tick (GameInput.ActionsDown, moitié haute). Bits lus par le moteur en
// 0x4F6D11 (tir) et 0x4F6D43 (tir secondaire).
constexpr size_t kAcActionsDownHigh = 0x216C;    // u32
constexpr uint32_t kActionFire = 0x40;
constexpr uint32_t kActionAltFire = 0x80;
// Sprint du joueur (0x4DDF01) : -1 tant qu'il sprinte, ce qui lui donne la variante d'animation
// « sprint » ; sinon l'instant du dernier appui sur « avancer ». Le moteur l'y met quand la touche
// est enfoncée deux fois en moins de quinze ticks, et l'en sort dès qu'elle est relâchée.
constexpr size_t kAcSprintTimer = 0x2190;        // int
// Non nul : le moteur recalcule chaque tick la visée vers kAcAimTarget (0x4DBE20) ; nul : droit
// devant. L'IA le pose quand elle vise.
constexpr size_t kAcIsAiming = 0x2195;           // char
constexpr size_t kAcAimTarget = 0x2198;          // Vector3, point visé
constexpr size_t kAcExtraBody = 0x2228;          // modèle de l'arme tenue : géométrie à +4
constexpr size_t kAcAimVector = 0x21A4;          // Vector3, direction de visée
constexpr size_t kAcCamVector = 0x21B0;          // Vector3
// Visée relative au corps, celle que suivent le buste, le bras et l'arme. Joueur : sa souris.
// IA : recalculée chaque tick d'après kAcAimTarget, ou remise à zéro (0x4E0137).
constexpr size_t kAcHeadFacing = 0x21BC;         // float, gauche-droite
constexpr size_t kAcHeadPitch = 0x21C0;          // float, haut-bas
constexpr size_t kAcAimFacing = 0x21C4;          // float, visée gauche-droite dans le monde, 0 à 2 pi
constexpr size_t kAcAimPitch = 0x21C8;           // float, 0 à 2 pi
// Visée automatique du joueur : écart ajouté pour pointer l'ennemi proche du viseur (0x4EFAD0),
// jusqu'à 0,35 rad.
constexpr size_t kAcAutoAimFacing = 0x21CC;      // float
constexpr size_t kAcAutoAimPitch = 0x21D0;       // float
// Demandes de tir posées pour le tick suivant : maintenu (auto), appui, appui secondaire, maintenu.
constexpr size_t kAcTriggerRequests = 0x21DF;    // char[4]

// Contact : ce qu'une IA sait d'un autre personnage (liste à Character+0xF7C).
constexpr size_t kCtOwner = 0x00;                // Character*, l'IA
constexpr size_t kCtEntity = 0x04;               // Character*, le personnage connu
constexpr size_t kCtStrength = 0x24;             // int : 4 certain (vu), 3 fort, 2 faible, 0 perdu
constexpr int32_t kCtStrengthStrong = 3;
constexpr size_t kCtPriority = 0x2C;             // int : 2 menace, 3 cible imposée par un script

// PhyContext
constexpr size_t kPhVelocity = 0x68;             // Vector3
constexpr size_t kPhPosition = 0x84;             // Vector3

// TRAM
constexpr size_t kTramFlags = 0x034;             // u32
constexpr uint32_t kTramFlagThrowSource = 0x8000;  // animation du lanceur d'une projection
// Animation de la cible d'une projection : ses dégâts sont attribués au lanceur, lu sans
// contrôle dans kAcThrownBy (0x4DD83A).
constexpr uint32_t kTramFlagThrowTarget = 0x10000;
constexpr size_t kTramType = 0x15A;              // u16
constexpr size_t kTramFromState = 0x15E;         // u16
constexpr size_t kTramToState = 0x160;           // u16
constexpr uint16_t kTramTypeReloadFirst = 0xA4;  // rechargements : un type par famille d'arme
constexpr uint16_t kTramTypeReloadLast = 0xAB;
constexpr size_t kTramDuration = 0x166;          // u16, en frames
// Nombre de coups que porte l'animation. Relevé dans level0_Final.dat : 1 pour KONCOMpunch_lt,
// KONCOMkick_low1 et KONOKOjump_crouch, 0 pour KONCOMhit_head1.
constexpr size_t kTramAttackCount = 0x17A;       // u8

// Texture (instance TXMP)
constexpr size_t kTxWidth = 0x84;                // u16
constexpr size_t kTxHeight = 0x86;               // u16
constexpr size_t kTxTexelType = 0x88;            // u32
constexpr size_t kTxPixels = 0x94;               // lignes de haut en bas, sans remplissage
constexpr uint32_t kTexelArgb4444 = 0;           // 16 bits par point : alpha, rouge, vert, bleu (table 0x529E78)

// États du rendu, donnés à fn::DrawStateSetInt et fn::GeomStateSet.
constexpr uint32_t kDrawStateZWrite = 4;         // 0 : n'écrit pas la profondeur (0x476157)
// 1 pendant le rendu de la scène : un tracé transparent (alpha sous 255, ou texture qui a un alpha)
// n'est pas fait sur-le-champ, il attend la fin de l'image dans une liste triée par profondeur
// (0x41E3A1, 0x41DBE0, 0x41C660). 0 : tracé immédiat, dans l'ordre des appels, comme le moteur le
// fait pour ses particules (0x4FB9EE) et son interface (0x47614B).
constexpr uint32_t kDrawStateSubmitMode = 8;
constexpr uint32_t kGeomStateSpriteMode = 6;     // 0 : face à la caméra (0x48C99E)

// ONWC
constexpr size_t kWcReloadAnimType = 0xBC;       // u16, type de l'animation de rechargement
constexpr size_t kWcFlags = 0xCC;                // u32
constexpr uint32_t kWcFlagTwoHanded = 0x8;       // arme tenue à deux mains (0x4DFC98)
constexpr uint32_t kWcFlagNoHolster = 0x2;       // ne se range pas (0x4EDD7B) : l'arme de Barabas
// Lus dans level0_Final.dat et rapprochés des drapeaux nommés par OniSplit : 0x24 pour une arme
// « UsesCells Automatic », 0x208 pour « TwoHanded Explosive ».
constexpr uint32_t kWcFlagUsesCells = 0x4;       // se recharge avec des cellules d'énergie
constexpr uint32_t kWcFlagExplosive = 0x200;     // son tir explose
constexpr size_t kWcMaxAmmo = 0xC0;              // u16, contenance de l'arme

// Fenêtre de l'interface (WMtWindow) : dialogue, bouton, cadre...
constexpr size_t kWnNext = 0x04;                 // fenêtre sœur suivante
constexpr size_t kWnFirstChild = 0x10;
constexpr size_t kWnId = 0x20;                   // u16
constexpr size_t kWnTitle = 0x40;                // char[256]
constexpr uint32_t kWnFlagVisible = 0x1;
constexpr uint32_t kWnFlagChild = 0x2000;        // ajouté par le moteur aux contrôles d'un dialogue (0x468E49)
constexpr uint16_t kWnTypeButton = 4;
// Champ de saisie (fonction de fenêtre 0x46B9C0) : celui du dialogue « dialog_setup_player » resté
// dans les données du jeu. Son texte se lit et s'écrit par messages : kEfGetText (param1 : tampon,
// param2 : sa taille ; 0x46C2D0), kEfSetText (param1 : texte ; 0x46C640), kEfSetMaxChars (param1 ;
// 0x46C5B0). Il prend le clavier quand on clique dedans.
constexpr uint16_t kWnTypeEditField = 7;
constexpr uint32_t kEfGetText = 0x40;
constexpr uint32_t kEfSetMaxChars = 0x41;
constexpr uint32_t kEfSetText = 0x42;
// Curseur horizontal : ceux du dialogue Options (qualité, gamma, volume). Voir fn::SliderSetRange.
constexpr uint16_t kWnTypeSlider = 17;
// Texte : son titre, relu à chaque tracé (0x473320). Style : 0x20000 centré, 0x40000 à droite,
// 0x100000 centré en hauteur (0x47329B) ; 0x110000 est celui des étiquettes du dialogue Options.
constexpr uint16_t kWnTypeText = 20;
constexpr uint32_t kTxStyleCentered = 0x00130000;

// Gabarit de dialogue (instance WMDD) : sa fenêtre, puis ses contrôles.
constexpr size_t kDlgHeight = 0x112;             // s16
constexpr size_t kDlgItemCount = 0x114;          // u32
constexpr size_t kDlgItems = 0x118;
constexpr size_t kDlgItemSize = 0x124;
constexpr size_t kItTitle = 0x000;               // char[256]
constexpr size_t kItType = 0x100;                // u16
constexpr size_t kItId = 0x102;                  // u16
constexpr size_t kItFlags = 0x104;               // u32
constexpr size_t kItStyle = 0x108;               // u32
constexpr size_t kItX = 0x10C;                   // s16, puis y, largeur, hauteur
constexpr size_t kItY = 0x10E;
constexpr size_t kItWidth = 0x110;
constexpr size_t kItHeight = 0x112;
constexpr size_t kItFont = 0x114;                // 16 octets, donnés à fn::WindowSetFont

// Menu de pause : identifiants de ses contrôles, et messages reçus par sa fonction (0x501990).
constexpr uint16_t kPauseTabFirst = 0x65;        // onglets Objective, Items, Weapons, Diary, Help
constexpr uint16_t kPauseTabLast = 0x69;
constexpr uint16_t kPausePrevious = 0x6A;        // boutons de page, deux jeux selon l'onglet
constexpr uint16_t kPauseNext = 0x6B;
constexpr uint16_t kPausePreviousAlt = 0x70;
constexpr uint16_t kPauseNextAlt = 0x71;
constexpr uint16_t kPauseMainArea = 0x6E;        // zones où le moteur dessine la page
constexpr uint16_t kPauseSubArea = 0x6F;
constexpr uint16_t kPauseMainOutline = 0x00;     // leurs cadres
constexpr uint16_t kPauseSubOutline = 0x78;
constexpr uint32_t kWmDestroy = 0x13;
// param1 : identifiant du contrôle dans le mot bas, ce qu'il annonce dans le mot haut ; param2 : le
// contrôle.
constexpr uint32_t kWmCommand = 0x17;
// Mot haut de param1 quand un curseur vient de bouger, à la souris ou au clavier (0x473131, 0x472B03).
constexpr uint32_t kSlNotifyMoved = 1;
constexpr uint32_t kWmDrawItem = 0x36;
constexpr uint32_t kWmInitDialog = 0x3D;
constexpr uint32_t kBtSetCheck = 0x3A;           // à un bouton : param1 non nul, il reste enfoncé

// Zone de texte (COtTextArea) : celle des lignes de la console, addr::kConsoleLines, que le moteur
// pose en bas de l'écran, sur toute sa largeur, lignes alignées à gauche et empilées depuis le bas
// (0x42FE90, création en 0x43000B). Les sous-titres des dialogues du niveau y passent aussi
// (0x5126BE). Celle des messages du mod est une zone à part, du même genre.
constexpr size_t kTaFontFamily = 0x00;           // TSFF*
constexpr size_t kTaContext = 0x04;              // contexte de texte : police, alignement, couleur
constexpr size_t kTaAlignment = 0x08;            // u16, donné tel quel au contexte de texte
constexpr size_t kTaFontSize = 0x0A;             // u16
constexpr size_t kTaScaleFont = 0x0C;            // u8 : taille de police proportionnelle à la hauteur d'image
constexpr size_t kTaBottomJustify = 0x0D;        // u8 : la dernière ligne en bas, les autres au-dessus
constexpr size_t kTaDisplayCompletion = 0x0E;    // u8
constexpr size_t kTaFadeOverBounds = 0x0F;       // u8
constexpr size_t kTaFormatting = 0x10;           // u8 : le texte porte des commandes de mise en forme
constexpr size_t kTaSize = 0x24;
constexpr size_t kTaBounds = 0x12;               // s16 gauche, haut, droite, bas
constexpr size_t kTaEntryCount = 0x1A;           // u16 : lignes présentes
constexpr size_t kTaMaxEntries = 0x1C;           // u16 : lignes gardées, la plus ancienne chassée ensuite
constexpr size_t kTaEntries = 0x20;              // lignes, de la plus ancienne à la plus récente
constexpr size_t kTeSize = 0x214;                // une ligne
constexpr size_t kTeTimer = 0x04;                // int : ticks qui lui restent ; sous 64 elle s'estompe ; 0 : sans fin
// Contexte de texte : alignement des lignes dans le rectangle qu'on lui donne (0x42E5B4, 0x42E6E7).
constexpr size_t kTcFont = 0x04;                 // police retenue pour sa taille ; nulle si aucune ne convient (0x42EC3C)
constexpr size_t kTcFormat = 0x08;               // u16
constexpr uint16_t kTcAlignMask = 0x7;
constexpr uint16_t kTcAlignLeft = 0x1;
constexpr uint16_t kTcAlignRight = 0x4;

// ONCC
// Saut et chute de la classe (noms des symboles de Daodan). Le saut part à kClassJumpVelocity
// (0x4DEA0F) ; tant que la touche est tenue, pendant kClassJumpTicks ticks, kClassJumpPush s'y
// ajoute à chaque tick (0x4F7978). Une chute plus haute que kClassFallSafe blesse, à proportion,
// jusqu'à tuer à kClassFallDeath (0x4EC937) : des dégâts de type fn::kOwnerFall.
constexpr size_t kClassJumpVelocity = 0x008;     // float
constexpr size_t kClassJumpPush = 0x010;         // float
constexpr size_t kClassJumpTicks = 0x016;        // u16
constexpr size_t kClassFallSafe = 0x018;         // float
constexpr size_t kClassFallDeath = 0x01C;        // float
constexpr size_t kClassAnimations = 0xC80;       // collection des animations de la classe
constexpr size_t kClassLeftHanded = 0xC8C;       // u8 : tient son arme de la main gauche
// Variante d'animation que donne l'arme en main (0x4DFC6F) : une ou deux mains, droitier ou gaucher.
constexpr uint16_t kVariantWeaponMask = 0x7800;
constexpr uint16_t kVariantPistolRight = 0x0800;
constexpr uint16_t kVariantPistolLeft = 0x1000;
constexpr uint16_t kVariantRifleRight = 0x2000;
constexpr uint16_t kVariantRifleLeft = 0x4000;
constexpr size_t kClassDefaultMeleeId = 0x2A6;   // u16, comparé à OSD+0x1D0 par NewCharacter

// Objet 'CHAR' en mémoire (OBJtObject) puis OSD perso à +0x2C.
constexpr size_t kObjType = 0x00;                // 'CHAR'
constexpr size_t kObjPosition = 0x0C;            // Vector3
constexpr size_t kObjOsd = 0x2C;
constexpr size_t kOsdFlags = 0x000;              // u32
constexpr size_t kOsdClass = 0x004;              // char[64]
constexpr size_t kOsdName = 0x044;               // char[32]
constexpr size_t kOsdWeapon = 0x064;             // char[64]
constexpr size_t kOsdScripts = 0x0A4;            // 8 noms de 32 caractères (voir kChScripts)
constexpr size_t kOsdScriptsSize = 0x100;
constexpr size_t kOsdTeam = 0x1C0;               // u16
constexpr size_t kOsdMeleeId = 0x1D0;            // u16
constexpr size_t kOsdFacing = 0x1FC;             // float, radians
constexpr size_t kOsdSize = 0x200;               // jusqu'à l'orientation comprise
constexpr size_t kFakeObjectSize = 0x2C + 0x400; // large marge au-delà de l'OSD réel

constexpr uint32_t kOsdFlagPlayer = 0x0001;
constexpr uint32_t kOsdFlagNotPresent = 0x0004;  // n'apparaît que sur ai2_spawn
constexpr uint32_t kOsdFlagNonCombatant = 0x0008;
constexpr uint32_t kOsdFlagSpawned = 0x0020;     // déjà apparu (0x48A4D6)
constexpr uint32_t kOsdFlagUnkillable = 0x0040;
constexpr uint32_t kOsdFlagInfiniteAmmo = 0x0080;
constexpr uint32_t kOsdFlagBoss = 0x0400;
}  // namespace off

// ---------------------------------------------------------------------------------------------
// Scripting (BSL)
// ---------------------------------------------------------------------------------------------
enum sl_type : int32_t { sl_int32, sl_str32, sl_float, sl_bool, sl_void };

struct sl_arg {
    int32_t type;
    union {
        void* value;
        int32_t value_int32;
        char* value_str32;
        float value_float;
        char value_bool;
    } val;
};

struct sl_callinfo {
    char* name;
    char* calllocation;
    int linenumber;
};

typedef uint16_t(ONICALL* sl_func)(sl_callinfo* info, uint32_t numargs, sl_arg* args, int* dontuse1,
                                   int* dontuse2, sl_arg* ret);

// ---------------------------------------------------------------------------------------------
// Fonctions du moteur
// ---------------------------------------------------------------------------------------------
namespace fn {
template <typename F>
inline F at(uintptr_t address) {
    return reinterpret_cast<F>(address);
}

// startObject : OBJtObject 'CHAR'. Renvoie 0 si OK ; écrit l'index du slot dans outIndex.
inline const auto NewCharacter =
    at<int16_t(ONICALL*)(void* startObject, void* aiSetup, void* flag, uint32_t* outIndex)>(0x004DAC50);
inline const auto DeleteCharacter = at<void(ONICALL*)(Character*)>(0x004DC480);
// Change la classe d'un personnage en place : corps, textures, animations. C'est ce qu'appelle la
// commande de script chr_set_class (0x4D9A42), qui, elle, désigne le personnage par sa case.
inline const auto SetCharacterClass = at<void(ONICALL*)(Character*, void* characterClass)>(0x004D7C30);
inline const auto GetDifficulty = at<int32_t (*)()>(addr::kGetDifficulty);
// Les deux temps de la simulation dans la boucle du jeu. Le premier avance l'horloge du jeu
// jusqu'à l'instant présent. Le second simule les ticks dus, kMaxTicksPerFrame au plus : actions
// donne les entrées du joueur, une par tick (off::kActionSize octets chacune) ; outTicks reçoit le
// nombre de ticks simulés. Renvoie 0 si tout va bien.
inline const auto UpdateServerTime = at<void(ONICALL*)(GameState*)>(0x004FBEB0);
// Instant de la machine, en soixantièmes de seconde : celui que lit l'horloge du jeu.
inline const auto MachineTime = at<uint32_t(ONICALL*)()>(0x004263E0);
inline const auto GameUpdate =
    at<uint16_t(ONICALL*)(uint32_t numActions, void* actions, uint32_t* outTicks)>(0x004FAFB0);
inline const auto GetActiveCharacter = at<ActiveCharacter*(ONICALL*)(Character*)>(0x004F1180);
// Donne un ActiveCharacter à un personnage qui n'en a pas (trop loin du joueur). 0 si OK.
inline const auto MakeActive = at<int16_t(ONICALL*)(Character*, uint32_t copyCurrent)>(0x004F10F0);
inline const auto SetAnimationExternal =
    at<void(ONICALL*)(Character*, int16_t fromState, void* tram, int32_t interpFrames)>(0x004EB340);
inline const auto SetHitPoints = at<void(ONICALL*)(Character*, uint32_t hitPoints)>(0x004EB220);
// knockback est un float passé par la pile. direction, si non nulle, pousse la victime. owner :
// type dans l'octet haut (kOwner*) et, pour une arme ou un coup, handle de l'attaquant.
typedef uint32_t(ONICALL* TakeDamageFunction)(Character*, uint32_t damage, uint32_t knockback,
                                              const Vector3* location, const Vector3* direction, uint32_t owner,
                                              uint32_t impact);
inline const auto TakeDamage = at<TakeDamageFunction>(addr::kTakeDamage);
constexpr uint32_t kOwnerTypeMask = 0xFF000000;
constexpr uint32_t kOwnerWeapon = 0x01000000;
constexpr uint32_t kOwnerMelee = 0x02000000;
constexpr uint32_t kOwnerFall = 0x05000000;
constexpr uint32_t kOwnerActOfGod = 0x06000000;   // AI2rSmite : tue même un personnage increvable
// Attaquant désigné par owner, ou nul (décor, chute, personnage disparu).
inline const auto OwnerCharacter = at<Character*(ONICALL*)(uint32_t owner)>(0x0051CDA0);

// weapon non nulle : la prend en main (celle qu'il tenait tombe). Nulle : range l'arme en main
// si holster, sinon sort l'arme rangée. Immédiat, sans animation.
inline const auto UseWeapon = at<void(ONICALL*)(Character*, void* weapon, uint32_t holster)>(0x004ED770);
// Ce que fait la touche d'échange d'arme quand il n'y a rien à ramasser (0x4F7547, 0x4F7582) :
// ranger l'arme en main, ou sortir l'arme rangée. L'appel ne fait que poser l'animation d'overlay
// (type 0xA0 pour ranger, 0xA1 ou 0xA2 pour sortir) ; l'arme change de place à sa frame d'action.
// Sans effet quand l'animation en cours ne le permet pas, et, pour ranger, quand la classe de l'arme
// porte kWcFlagNoHolster. La touche ne les appelle que si aucun overlay n'est en cours.
inline const auto HolsterWeapon = at<uint8_t(ONICALL*)(Character*, ActiveCharacter*, uint32_t flag)>(0x004EDD20);
inline const auto DrawWeapon = at<void(ONICALL*)(Character*, ActiveCharacter*, void* weapon)>(0x004EDEB0);
// Détache l'arme d'un emplacement de l'inventaire (kChWeaponInHand) et la laisse tomber.
inline const auto DropSlot =
    at<void(ONICALL*)(void* inventory, uint32_t slot, uint32_t knockAway, uint32_t keep)>(0x0051B590);
inline const auto DeleteWeapon = at<void(ONICALL*)(void* weapon)>(0x00518F90);
// Pose au sol une arme neuve de cette classe : c'est l'appel de la commande de script
// weapon_spawn (0x4B85BE). place : le repère du niveau où la poser, dont seuls sont lus le point
// (kPlacePosition) et l'orientation (kPlaceFacing). nullptr si les soixante-quatre cases sont
// prises.
inline const auto WeaponSpawn = at<void*(ONICALL*)(void* weaponClass, const void* place)>(0x00518C00);
constexpr size_t kPlaceSize = 0x44;
constexpr size_t kPlacePosition = 0x30;          // Vector3
constexpr size_t kPlaceFacing = 0x3C;            // float
// Animation d'une collection pour un type, un état de départ et des variantes ; nullptr si aucune.
inline const auto LookupAnimation =
    at<void*(ONICALL*)(void* collection, uint32_t type, uint32_t fromState, uint32_t variants)>(0x00427F00);
constexpr uint32_t kAnimStateOverlay = 7;        // état passé par le moteur pour ses animations d'arme
// Pose une animation dans un emplacement d'overlay (kAcOverlay), depuis sa première frame.
inline const auto SetOverlay = at<void(ONICALL*)(void* overlay, void* tram, uint32_t flags)>(0x004D7B80);
inline const auto DeletePowerup = at<void(ONICALL*)(void* powerup)>(0x0051C4F0);
// Fait ramasser un bonus du sol à un personnage : le donne (fn::GivePowerup, borné par ce qu'il
// peut porter, son et message si c'est le joueur) et, s'il en a reçu, le retire du sol. C'est
// l'appel du moteur à la touche d'échange d'arme (0x4F75BF) quand l'animation en cours permet de
// ramasser sans s'arrêter. Aucune distance n'y est vérifiée : celle du moteur (kPickupReach) l'est
// par son appelant.
inline const auto PickupPowerup = at<void(ONICALL*)(Character* chr, void* powerup)>(0x004EDA10);
// Ce que le moteur exige avant cet appel (0x4FA4B0 puis 0x51B3E0) : le bonus à moins de huit unités
// à plat, et entre trois unités sous les pieds du personnage et dix-huit au-dessus.
constexpr float kPickupReach = 8.0f;
constexpr float kPickupBelow = 3.0f;
constexpr float kPickupAbove = 18.0f;
// Pose un bonus à cet endroit, sans le faire tomber. nullptr si amount est nul ou le type inconnu.
inline const auto NewPowerup =
    at<void*(ONICALL*)(uint32_t type, uint32_t amount, const Vector3* position, float facing)>(0x0051C160);

// Donne un bonus à un personnage : type du moteur (off::kPuType), quantité. ignoreMax nul : ses
// réserves sont bornées à six, un bouclier ou une invisibilité à leur valeur neuve. notify : son et
// message à l'écran si c'est le joueur. Renvoie ce qui a été réellement donné.
// Réveille un personnage que le moteur avait mis en sommeil (loin du joueur : plus de physique ni
// d'animation). Rend son état actif, nul s'il n'y a plus de place.
inline const auto ForceActive = at<void*(ONICALL*)(Character* chr)>(0x004F11B0);
inline const auto GivePowerup =
    at<uint16_t(ONICALL*)(Character*, uint32_t type, uint32_t amount, uint32_t ignoreMax, uint32_t notify)>(0x0051CA30);
// Touche d'action devant un PNJ qui a une interaction « neutre » : avec activate, le PNJ en face de
// who (à 35 unités au plus, dans les 60 degrés de son regard, 0x4ACA44) passe dans cet état, ou
// poursuit celle qu'il avait commencée avec lui. Vrai s'il y en avait un. Le PNJ prend pour
// interlocuteur le joueur de la partie, lu à ce moment-là (0x4ACE7F), et non who.
inline const auto NeutralAction = at<uint8_t(ONICALL*)(Character* who, uint32_t activate)>(addr::kNeutralActionTarget);

// Donne à une IA la connaissance certaine d'un personnage, là où il se trouve : ce que font les
// commandes de script ai2_makeaware (0, 0) et ai2_attack (0, 1 : cible imposée). Le moteur revoit
// alors la cible de l'IA (0x490745).
inline const auto AiMakeAware =
    at<void(ONICALL*)(Character* ai, Character* target, uint32_t disableNotify, uint32_t forceTarget)>(0x0049AC00);
// Relation de la première équipe envers la seconde : 0 amie, 1 neutre, 2 hostile.
inline const auto FriendOrFoe = at<uint32_t(ONICALL*)(uint32_t team, uint32_t otherTeam)>(0x0048B2C0);
// Rayon tiré dans le monde : ce qu'il touche en premier, d'un personnage (autre que celui du numéro
// donné, 0xFFFE : tous comptent), d'un objet du niveau ou du décor. C'est la fonction du viseur
// laser (appelée en 0x4E1A22). direction : vecteur unité ; le décor est cherché jusqu'à mille fois
// sa longueur (0x4D4F29). Sans décor chargé, le résultat est « rien ».
struct RayResult {
    uint32_t type;       // kRay*
    void* hit;           // personnage (Character*), objet, ou numéro du polygone du décor
    uint32_t part;       // personnage : partie du corps touchée (16 bits)
    Vector3 point;       // où le rayon s'arrête
};
constexpr uint32_t kRayNothing = 0, kRayCharacter = 1, kRayEnvironment = 2, kRayObject = 3;
inline const auto RayToEverything =
    at<void(ONICALL*)(uint32_t ignoredIndex, const Vector3* origin, const Vector3* direction, RayResult* out)>(0x004D4E50);
// Zone de déclenchement (objet TRGV) : ce personnage, s'il est d'une des équipes du masque, s'y
// trouve. C'est le test du moteur quand il compte les présents (0x4DCB70).
inline const auto VolumeContains = at<uint8_t(ONICALL*)(void* volume, uint32_t teamMask, Character* chr)>(0x004CC680);

// Point du personnage au-dessus duquel le moteur place nom et santé.
inline const auto OverheadAnchor = at<void(ONICALL*)(Character*, Vector3* out)>(0x004E3460);
// Caméra : sa position, la direction du regard, le haut. Une sortie nulle est sautée.
inline const auto CameraView =
    at<void(ONICALL*)(void* camera, Vector3* location, Vector3* view, Vector3* up)>(0x0041B4A0);

// Rendu. Mêmes appels que l'affichage de mise au point du moteur (0x48C7FC à 0x48C9EA), qui écrit
// le nom d'un personnage dans une texture et la montre sur un rectangle face à la caméra.
// Texture créée pour la durée du niveau. Dimensions arrondies à la puissance de deux. 0 si OK.
inline const auto TextureNew =
    at<int16_t(ONICALL*)(uint32_t width, uint32_t height, uint32_t texelType, uint32_t allocate, uint32_t flags,
                         const char* name, void** outTexture)>(0x0041EB00);
constexpr uint32_t kTextureAllocate = 3;         // valeur passée par le moteur pour sa texture de noms
inline const auto TexelTypeAvailable = at<uint8_t(ONICALL*)(uint32_t texelType)>(0x0041A680);
// À appeler après avoir écrit dans une texture : le rendu la recharge. 0 si OK.
inline const auto InstanceUpdate = at<int16_t(ONICALL*)(void* instance)>(0x00423CB0);
// Rectangle texturé centré sur center, face à la caméra. halfWidth et halfHeight : demi-côtés, en
// unités du monde. color : 0xAARRGGBB, multipliée par la texture ; alpha : 0 à 255. offset décale
// le rectangle vers la droite, en demi-largeurs ; chop en retire cette fraction par la droite.
inline const auto SpriteDraw =
    at<void(ONICALL*)(void* texture, const Vector3* center, float halfWidth, float halfHeight, uint32_t color,
                      uint32_t alpha, float rotation, const Vector3* direction, const void* orientation,
                      float offset, float shorten, float chop)>(0x0041A830);
inline const auto GeomStateSet = at<void(ONICALL*)(uint32_t state, uint32_t value)>(0x0041E720);
inline const auto GeomStateGet = at<uint32_t(ONICALL*)(uint32_t state)>(0x0041E740);
inline const auto GeomStateCommit = at<void(ONICALL*)()>(0x0041E870);
// Pile des états de rendu : huit niveaux, dont un que SpriteDraw prend pour lui.
inline const auto DrawStatePush = at<int16_t(ONICALL*)()>(0x0041E1F0);
inline const auto DrawStatePop = at<int16_t(ONICALL*)()>(0x0041E2B0);
inline const auto DrawStateSetInt = at<void(ONICALL*)(uint32_t state, uint32_t value)>(0x0041E170);

// Interface. Mêmes appels que le moteur quand il crée les contrôles d'un dialogue (0x468E5D).
inline const auto WindowNew =
    at<void*(ONICALL*)(uint32_t type, const char* title, uint32_t flags, uint32_t style, uint32_t id, int32_t x,
                       int32_t y, int32_t width, int32_t height, void* parent, uint32_t creationData)>(0x00475160);
inline const auto WindowSetFont = at<uint8_t(ONICALL*)(void* window, const void* fontInfo)>(0x00475630);
inline const auto WindowSetVisible = at<uint8_t(ONICALL*)(void* window, uint32_t visible)>(0x00475A10);
inline const auto WindowSend =
    at<uint32_t(ONICALL*)(void* window, uint32_t message, uint32_t param1, uint32_t param2)>(0x004760D0);
inline const auto DialogItem = at<void*(ONICALL*)(void* dialog, uint32_t id)>(0x004689A0);
// Curseur. Mêmes appels que le dialogue Options du moteur (0x4D2602, 0x4D2AD0, 0x4D2905). Sa
// position est un entier de minimum à maximum ; SetRange la remet au minimum, SetPosition la borne
// et ne prévient pas le dialogue.
inline const auto SliderSetRange = at<void(ONICALL*)(void* slider, int32_t minimum, int32_t maximum)>(0x00472650);
inline const auto SliderSetPosition = at<void(ONICALL*)(void* slider, int32_t position)>(0x004723B0);
inline const auto SliderGetPosition = at<int32_t(ONICALL*)(void* slider)>(0x004723A0);
// Relâche les boutons first à last, puis enfonce checked.
inline const auto DialogRadioCheck =
    at<void(ONICALL*)(void* dialog, uint32_t first, uint32_t last, uint32_t checked)>(0x00468BB0);
typedef uint8_t(ONICALL* DialogCallback)(void* dialog, uint32_t message, uint32_t param1, uint32_t param2);

inline const auto GetDataPtr =
    at<int16_t(ONICALL*)(uint32_t tag, const char* name, void* outDataPtr)>(0x004232E0);
inline const auto GetDataPtrByNumber =
    at<int16_t(ONICALL*)(uint32_t tag, uint32_t index, void** outDataPtr)>(0x00423680);
inline const auto GetTagCount = at<uint32_t(ONICALL*)(uint32_t tag)>(0x004236F0);
inline const auto GetInstanceName = at<char*(ONICALL*)(void* dataPtr)>(0x00423D90);

inline const auto CommandExecute = at<uint8_t(ONICALL*)(char* line)>(0x004317D0);
inline const auto TextAreaPrint =
    at<void(ONICALL*)(void* area, uint32_t priority, uint32_t color, uint32_t shadow, const char* text,
                      uint32_t unknown, uint32_t fadeTime)>(0x00431340);
// Zone de texte à créer. bounds : gauche, haut, droite, bas. Les autres arguments sont ceux que le
// moteur donne à la sienne (0x42FFDF) : lignes gardées (-1 : autant que le rectangle en contient),
// police, alignement (off::kTcAlign*), taille, et quatre drapeaux. 0 si elle est créée.
inline const auto TextAreaNew =
    at<int16_t(ONICALL*)(void** outArea, int16_t* bounds, uint32_t unused, int32_t lines, void* fontFamily,
                         uint32_t alignment, uint32_t formattingCommands, uint32_t fontSize, uint32_t scaleFont,
                         uint32_t bottomJustify, uint32_t displayCompletion, uint32_t fadeOverBounds)>(0x00430F00);
// Trace les lignes d'une zone de texte. Son contexte de texte doit exister.
inline const auto TextAreaDisplay = at<void(ONICALL*)(void* area)>(0x00430FD0);
// Nouveau rectangle d'une zone de texte (gauche, haut, droite, bas). lines : lignes gardées, ou -1
// pour autant que le rectangle en contient. Celles qui y sont déjà restent.
inline const auto TextAreaResize = at<int16_t(ONICALL*)(void* area, int16_t* bounds, int32_t lines)>(0x00431460);
// Taille de l'image, en pixels.
inline const auto DrawWidth = at<uint16_t(ONICALL*)()>(0x0041E0F0);
inline const auto DrawHeight = at<uint16_t(ONICALL*)()>(0x0041E130);
inline const auto RegisterCommandVoid =
    at<uint16_t(ONICALL*)(const char* name, const char* description, const char* argFormat,
                          sl_func callback)>(0x00477B40);

// Demandes de tir d'un personnage, telles que les pose le traitement des touches du joueur
// (0x4F6D01 à 0x4F6D6A). Le moteur les consomme au tick suivant.
typedef void(ONICALL* TriggerFunction)(Character*, ActiveCharacter*);
inline const auto TriggerPressed = at<TriggerFunction>(0x004E8F80);
inline const auto TriggerHeld = at<TriggerFunction>(0x004E8FA0);
inline const auto TriggerReleased = at<TriggerFunction>(0x004E9000);
inline const auto AltTriggerPressed = at<TriggerFunction>(0x004E8FD0);
inline const auto AltTriggerReleased = at<TriggerFunction>(0x004E9020);

// Allume ou éteint tous les lasers, toutes les tourelles de ce numéro (0xFFFF : tous) : ce
// qu'appellent trig_activate (0x4B61F8), trig_deactivate, turret_activate (0x4B6338) et
// turret_deactivate.
inline const auto TriggerActivate = at<void(ONICALL*)(uint32_t id)>(0x004CB200);
inline const auto TriggerDeactivate = at<void(ONICALL*)(uint32_t id)>(0x004CB280);
inline const auto TurretActivate = at<uint16_t(ONICALL*)(uint32_t id)>(0x004CD990);
inline const auto TurretDeactivate = at<uint16_t(ONICALL*)(uint32_t id)>(0x004CDA80);
// La tourelle peut-elle tirer sur sa cible du moment (kTurretTarget) : vrai si elle est tout près,
// ou si rien du décor ne les sépare (0x4CF7CA). Ne change rien à la tourelle. C'est le test que le
// moteur fait après avoir choisi la cible (0x4CF292), et dont dépend le tir.
inline const auto TurretCanShoot = at<uint8_t(ONICALL*)(void* turret)>(0x004CF6B0);
// Lance les événements d'un objet du niveau, au nom de ce personnage (son nom est donné au script
// d'un événement « Execute Script »).
inline const auto RunEvents = at<uint16_t(ONICALL*)(void* events, Character* cause)>(addr::kEventsRun);
// Joue un son à cet endroit du niveau : c'est l'appel du moteur quand un laser est coupé
// (0x4CBAFD), avec trois zéros pour le reste.
inline const auto SoundPlayAt =
    at<void(ONICALL*)(void* sound, const Vector3* position, uint32_t a, uint32_t b, uint32_t c)>(0x00512480);
// Ouvre une porte ; sans personnage, l'ouverture est forcée (c'est ce que fait door_open).
inline const auto DoorOpen = at<void(ONICALL*)(void* door, Character* opener)>(0x004C26C0);
// Sans effet si la console est inactive ou déjà déclenchée ; sinon exécute ses événements.
inline const auto ConsoleActivate = at<int16_t(ONICALL*)(void* console, Character* user)>(0x004C0880);
// Appelle callback(objet, userData) pour chaque objet du type ; le rappel renvoie 0 pour arrêter.
typedef uint8_t(ONICALL* ObjectCallback)(void* object, void* userData);
inline const auto EnumerateObjects =
    at<int32_t(ONICALL*)(uint32_t type, ObjectCallback callback, void* userData)>(0x004D0080);

// cdecl variadique, écrit dans startup.txt.
inline const auto StartupMessage = at<void(__cdecl*)(const char* format, ...)>(0x00424860);
}  // namespace fn

}  // namespace oni
