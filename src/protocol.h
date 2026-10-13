// Format des datagrammes UDP. Little-endian, sans remplissage.
#pragma once

#include <cstdint>

namespace proto {

constexpr uint32_t kMagic = 0x504D4E4F;  // "ONMP"
constexpr uint8_t kVersion = 25;

// Un hôte et jusqu'à trois clients. Chaque joueur a un numéro : 0 pour l'hôte, 1 à 3 pour les
// clients, donné par l'hôte dans son kWelcome. Les clients ne se parlent pas entre eux : tout
// passe par l'hôte, qui redistribue.
constexpr int kMaxPlayers = 4;

enum Type : uint8_t {
    kHello = 1,     // client -> hôte, répété jusqu'au kWelcome ; porte un Hello
    kWelcome,       // hôte -> client ; porte un Welcome
    kPlayer,        // 1 EntityState : un joueur. Le sien pour un client ; l'hôte envoie le sien et
                    // redistribue à chaque client ceux des autres
    kNpcSnapshot,   // hôte -> client, N EntityState
    kNpcInfo,       // hôte -> client, N NpcInfo (identifiant -> nom)
    kEvent,         // fiable : renvoyé jusqu'au kAck
    kAck,
    kBye,
    kStream,        // flux fiable et ordonné : StreamHeader + données opaques
    kStreamAck,     // StreamAck : numéro du prochain message attendu
    kBeacon,        // hôte -> diffusion sur le réseau : Beacon, pour être trouvé sans adresse
    kReject,        // hôte -> client : Reject, réponse à un kHello refusé
    kPing,          // Ping, dans les deux sens, une fois par seconde : mesure de la latence
    kPong,          // le même Ping, renvoyé tel quel par celui qui l'a reçu
};

enum EntityFlags : uint16_t {
    kEntityActive = 1,  // possède un ActiveCharacter (animation valide)
    kEntityDead = 2,
    kEntityFire = 4,    // gâchette principale enfoncée (joueur ou PNJ)
    kEntityAltFire = 8, // joueur : gâchette secondaire enfoncée
    // Joueur à terre, qui attend d'être ranimé : health porte alors le temps qu'il lui reste, en
    // dixièmes de seconde, et non sa santé.
    kEntityDowned = 16,
    kEntityReviving = 32,  // joueur à terre qu'un autre est en train de ranimer : son compte à rebours est arrêté
    kEntityReloading = 64, // recharge son arme : l'animation, jouée par-dessus celle du corps, n'est pas dans animID
};
// Bits 8 à 12 de flags : durée (en frames) du fondu en cours vers l'animation courante, 0 si aucun.
constexpr int kEntityInterpShift = 8;
constexpr uint16_t kEntityInterpMask = 0x1F;
// Bits 13 à 15 : libres. Jusqu'au protocole 17 l'aura y voyageait, en huit niveaux ; elle a
// maintenant son octet (EntityState.super).

// Réserves d'un joueur, six de chaque au plus dans le moteur : chargeurs de balles dans les quatre
// bits bas d'EntityState.ammo, cellules d'énergie dans les quatre bits hauts.
constexpr int kAmmoSlots = 6;
constexpr uint8_t kAmmoMask = 0xF;
constexpr int kCellShift = 4;
// Invisibilité : EntityState.cloak compte le temps qui reste par pas de ce nombre de ticks.
constexpr int kCloakStep = 8;

enum EventKind : uint8_t {
    kEventDamage = 1,   // client -> hôte : dégâts infligés à un PNJ
};

constexpr uint32_t kHostPlayerId = 0;

#pragma pack(push, 1)

struct Header {
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint16_t seq;
    uint32_t levelHash;   // FNV-1a du nom du niveau chargé chez l'émetteur
};

// Le PlayerPacket de la spec, étendu.
struct EntityState {
    float x, y, z;
    float rotation;        // radians
    uint32_t animID;       // FNV-1a du nom d'instance TRAM, 0 = aucune
    uint32_t characterID;  // joueurs : numéro du joueur, 0 à kMaxPlayers - 1 ; PNJ : FNV-1a du nom + rang
    uint16_t animFrame;
    int16_t health;        // joueur à terre (kEntityDowned) : dixièmes de seconde avant sa mort
    uint16_t flags;        // EntityFlags + durée de fondu
    uint16_t tick;         // 16 bits bas du GameTime de l'émetteur (60 par seconde)
    int16_t aimYaw;        // direction de l'arme dans le monde, kAimScale par radian ; 0, 0 si inconnue
    int16_t aimPitch;
    // Ce qui se voit sur le personnage sans être dans son animation, tel que le montre la machine
    // qui le simule. Un pantin n'a ni la santé ni l'inventaire de celui qu'il représente.
    uint8_t super;         // aura du personnage « surpuissant », de 0, aucune, à 255, entière
    uint8_t shield;        // bouclier de force : ce qu'il en reste, 100 pour un bouclier neuf
    uint8_t cloak;         // invisibilité : temps qui reste, en pas de kCloakStep ticks ; 0 : visible
    uint8_t ammo;          // joueurs : chargeurs et cellules en réserve (kAmmoMask, kCellShift)
};

constexpr float kAimScale = 10000.0f;

// Premier octet de chaque message du flux fiable. Ceux qu'un joueur adresse aux autres (portes,
// consoles, armes, bonus, rechargement, projections, coups, réactions, réanimations) sont redistribués par l'hôte
// aux autres clients, avec le numéro de leur auteur (StreamHeader.origin).
enum ReliableKind : uint8_t {
    kReliableScript = 1,    // hôte -> client : commande de script (voir scriptcodec.h)
    kReliableDoor = 2,      // u32 niveau, u16 identifiant : porte ouverte
    kReliableConsole = 3,   // u32 niveau, u16 identifiant : console activée
    kReliableWeapon = 4,    // u32 niveau, u8 longueur, nom de la classe d'arme en main (vide : aucune)
    kReliableThrow = 5,     // Throw : projection décidée chez l'émetteur sur un personnage du destinataire
    kReliableHit = 6,       // Hit : coup ou tir reçu par le joueur désigné (PNJ de l'hôte, ou tir ami)
    kReliableImpose = 7,    // Impose : réaction à ce coup, subie par le joueur désigné
    kReliableWeaponEvent = 8,  // u32 niveau, WeaponEvent : arme lâchée ou ramassée par le joueur de l'émetteur
    kReliablePowerup = 9,   // u32 niveau, PowerupEvent : bonus ramassé par le joueur de l'émetteur
    kReliableLevelInstance = 10,   // LevelInstance, hôte -> client : chargement de niveau en cours chez l'hôte
    kReliableRestartRequest = 11,  // RestartRequest, client -> hôte : son joueur est mort
    kReliableReload = 12,   // u32 niveau : le joueur de l'émetteur commence à recharger son arme
    kReliableSettings = 13, // Settings, hôte -> client : réglages de partie
    // Hôte -> client, PNJ partagés : ce qui se trouve au sol chez l'hôte. u32 niveau, u8 nombre,
    // puis autant de GroundPowerup ou de GroundWeapon.
    kReliableGroundPowerups = 14,
    kReliableGroundWeapons = 15,
    kReliableDigest = 16,   // Digest, client -> hôte : empreintes de son état, une fois par seconde
    kReliableResync = 17,   // Resync, hôte -> client : parties de l'état qu'il lui renvoie de force
    // Entre l'hôte et chaque client, dans les deux sens, à chaque connexion : u16 nombre, puis autant
    // de u32, les paquets de l'AE Installer installés chez l'émetteur (voir modlist.h).
    kReliableMods = 18,
    kReliableRevive = 19,   // Revive : l'émetteur ranime un joueur à terre (début, abandon, réussite)
    kReliableAlert = 20,    // Alert, hôte -> client : la mission vient d'échouer, et pourquoi
    kReliableJoin = 21,     // Join, client -> hôte : il arrive dans la partie, et où en est son niveau
    kReliableReady = 22,    // Ready, client -> hôte : son niveau est chargé, il attend le départ
    kReliableGo = 23,       // Go, hôte -> client : tout le monde a chargé, la partie repart
    // Hôte -> client, PNJ partagés : l'arme que tient chaque PNJ armé. u32 niveau, u8 nombre, puis
    // autant de NpcWeapon. Un PNJ absent de la liste n'a rien en main.
    kReliableNpcWeapons = 24,
    kReliableInteract = 25, // u32 niveau, client -> hôte : son joueur presse la touche d'action devant un PNJ
    kReliableGift = 26,     // Gift, hôte -> ce client : ce qu'un PNJ vient de donner à son joueur
    kReliableSavePoint = 27,  // SavePoint, hôte -> client : l'hôte vient d'atteindre un point de sauvegarde
    kReliableProfile = 28,  // Profile, client -> hôte : son pseudo, et la tenue qu'il voudrait
    kReliableRoster = 29,   // Roster, hôte -> clients : pseudo, tenue et latence de chaque joueur
    kReliableDisarm = 30,   // Disarm : le joueur de l'émetteur vient de prendre son arme à un PNJ
    kReliableMorph = 31,    // Morph, client -> hôte : le personnage qu'il veut incarner
    kReliableMorphs = 32,   // Morphs, hôte -> clients : le personnage que chaque joueur incarne
    kReliableSpawn = 33,    // Spawn, client -> hôte : ce qu'il demande de faire apparaître, et où
    kReliableSpawned = 34,  // Spawn, hôte -> clients : arme que l'hôte vient de poser au sol
    // Hôte -> clients, PNJ partagés : lasers et tourelles du niveau, allumés ou éteints. u32 niveau,
    // u8 drapeaux (DefenseFlags), u8 nombre, puis autant de DefenseState. L'état de l'hôte fait foi ;
    // les clients s'y alignent.
    kReliableDefenses = 35,
    // Hôte -> clients, PNJ partagés : la cible de chaque tourelle du niveau, dans l'ordre du niveau.
    // u32 niveau, u8 nombre, puis autant d'octets (TurretTarget). C'est l'hôte qui choisit ; chaque
    // client fait viser à ses tourelles le même joueur, et c'est sur la machine de ce joueur que
    // leurs tirs le blessent.
    kReliableTurretTargets = 36,
    // LaserTrip, PNJ partagés : un joueur vient de couper ce laser. Un client l'envoie pour son
    // propre joueur, vu sur sa machine là où elle affiche le laser ; l'hôte pour le sien, et pour
    // tout client dont il voit le pantin le couper chez lui. Les autres machines font alors chez
    // elles ce que le moteur fait à cet instant : le son du laser et ses événements (tourelles,
    // alarme, script, portes, consoles). Un laser éteint chez l'hôte reste sans suite chez lui.
    kReliableLaserTrip = 37,
    // Hôte -> clients, PNJ partagés : où en sont les rayons des lasers mobiles. u32 niveau, puis
    // pour chaque laser : u8 rang, u8 nombre de rayons, et par rayon un float (image de son rail)
    // et un i8 (son sens, +1 ou -1). Un client y recale les siens quand ils s'en sont écartés.
    kReliableLaserRays = 38,
};

struct LaserTrip {
    uint32_t levelHash;
    uint16_t index;          // rang du laser parmi ceux du niveau, le même sur toutes les machines
    uint8_t player;          // numéro du joueur qui l'a coupé ; venant d'un client, c'est toujours le sien
};
// Rayons d'un laser, au plus : les lasers du jeu en ont un, deux, trois, quatre ou six.
constexpr int kMaxLaserRays = 8;

enum TurretTarget : uint8_t {
    kTurretNoTarget = 0,     // aucune cible
    kTurretPlayer = 1,       // 1 à kMaxPlayers : le joueur de ce numéro, plus un
    kTurretOther = 0xFF,     // un personnage qui n'est pas un joueur : chaque machine le choisit elle-même
};
constexpr int kMaxTurrets = 64;

enum DefenseFlags : uint8_t {
    // Chez l'hôte un script tient toutes les IA passives (une cinématique) : ses tourelles ne tirent
    // pas, celles des clients non plus.
    kDefensesHeld = 1,
};

enum DefenseBits : uint8_t {
    kDefenseTurret = 1,      // une tourelle ; sinon un laser
    kDefenseActive = 2,      // allumé
    kDefenseHit = 4,         // laser : un joueur le coupe en ce moment
};

struct DefenseState {
    uint16_t id;             // numéro du laser ou de la tourelle dans le niveau
    uint8_t bits;            // DefenseBits
};
constexpr int kMaxDefenses = 200;

struct NpcInfo {
    uint32_t characterID;
    char name[32];
};

struct Event {
    uint16_t eventId;
    uint8_t kind;          // EventKind
    uint8_t pad;
    uint32_t characterID;
    int32_t value;
};

struct Ack {
    uint16_t eventId;
};

// Identifie une connexion : le client en tire un nouveau à chaque tentative. L'hôte sait ainsi
// qu'un client qui se représente a tout remis à zéro, même s'il ne l'avait pas vu partir.
struct Hello {
    uint32_t session;
};

struct Welcome {
    uint32_t session;        // celle du kHello accepté
    uint8_t player;          // numéro donné à ce client, 1 à kMaxPlayers - 1
    uint8_t maxPlayers;
};

// Le lanceur est simulé chez l'émetteur, la cible chez le destinataire.
struct Throw {
    uint32_t levelHash;
    uint32_t sourceID;       // characterID du lanceur
    uint32_t targetID;       // characterID de la cible
    uint32_t throwAnim;      // animID du lanceur
    uint32_t targetAnim;     // animID que le moteur impose à la cible
    uint8_t sourceIsPlayer;  // les identifiants de joueur et de PNJ ne partagent pas le même espace
    uint8_t targetIsPlayer;
};

// Dégâts décidés chez l'émetteur, où l'attaquant est simulé, et subis par le joueur targetID, sur
// sa machine. PNJ : de l'hôte vers un client. Joueur : d'un joueur à un autre, si le tir ami est
// activé.
struct Hit {
    uint32_t levelHash;
    uint32_t attackerID;     // characterID de l'attaquant
    uint32_t impact;         // type d'impact du moteur
    float knockback;
    float direction[3];      // poussée ; ignorée si hasDirection est nul
    uint16_t damage;
    uint8_t ownerType;       // 1 : arme, 2 : mêlée
    uint8_t hasDirection;
    uint8_t attackerIsPlayer;  // les identifiants de joueur et de PNJ ne partagent pas le même espace
    uint8_t targetID;        // numéro du joueur qui encaisse
};

// Réglages de partie, décidés par l'hôte. Envoyés à chaque connexion et à chaque changement.
struct Settings {
    uint8_t teamKill;        // les joueurs peuvent se blesser
    uint8_t cheats;          // interrupteur général des cheats : coupé, plus aucun n'agit chez personne
    uint8_t onlineDamage;    // les PNJ frappent plus fort à mesure qu'il y a de clients
    uint8_t clients;         // clients que l'hôte compte pour la difficulté
    // Ce que la difficulté donne en ce moment, d'après le fichier de l'hôte, en pour cent des
    // valeurs d'origine : vie des PNJ, et leurs dégâts sur les joueurs.
    uint16_t npcHealthPercent;
    uint16_t npcDamagePercent;
    uint8_t difficulty;      // mode de difficulté (rules::Mode)
    uint8_t enemies;         // multiplicateur d'ennemis en vigueur, de 1 à 4
    uint8_t civilians;       // des civils se mêlent aux ennemis
    uint8_t players;         // joueurs que l'hôte compte pour le minimum du multiplicateur
    uint8_t cheatAccess;     // un bit par numéro de joueur : il a les onglets Cheat, Character et Spawn
};

// Classe de personnage (ONCC) ou d'arme (ONWC) : vingt signes au plus dans les données du jeu.
constexpr int kClassLength = 24;

// Le personnage qu'un joueur incarne à la place de Konoko. Vide : sa tenue de Konoko.
struct Morph {
    char className[kClassLength];
};

struct Morphs {
    char className[kMaxPlayers][kClassLength];
};

enum SpawnKind : uint8_t {
    kSpawnItem = 0,          // name : inutilisé ; type : celui du bonus dans le moteur ; amount : combien
    kSpawnWeapon = 1,        // name : classe de l'arme ; amount : cartouches, en pour cent de sa contenance
    kSpawnNpc = 2,           // name : classe du personnage ; type : 1 allié, 0 ennemi
};

// Un client autorisé demande à l'hôte de faire apparaître quelque chose devant son joueur. L'hôte
// le crée chez lui, d'où les PNJ et les bonus gagnent les clients par les chemins ordinaires ; une
// arme, que rien ne transporte, leur est annoncée par le même message.
struct Spawn {
    uint32_t levelHash;
    float x, y, z;
    float facing;
    uint8_t kind;            // SpawnKind
    uint8_t type;
    uint16_t amount;
    char name[kClassLength];
};

// Pseudo d'un joueur : quinze signes au plus, zéro terminal compris dans la taille.
constexpr int kNameLength = 16;
constexpr uint8_t kAnySkin = 0xFF;  // Profile.skin : la première tenue libre

// Ce qu'un client dit de lui à l'hôte, à chaque connexion et quand il le change.
struct Profile {
    char name[kNameLength];
    uint8_t skin;            // tenue voulue (numéro, voir roster.h), ou kAnySkin
};

// Ce que l'hôte dit de tous les joueurs. C'est lui qui attribue les tenues : jamais la même à deux
// joueurs présents.
struct Roster {
    uint8_t present;         // un bit par numéro de joueur
    uint8_t skin[kMaxPlayers];
    uint16_t ping[kMaxPlayers];  // aller-retour entre ce joueur et l'hôte, en millisecondes ; 0 pour l'hôte
    char name[kMaxPlayers][kNameLength];
};

struct Ping {
    uint32_t stamp;          // horloge de l'émetteur, en millisecondes
};

// Animation que le moteur de l'émetteur a imposée au pantin du joueur targetID après un coup jugé
// chez lui : recul, chute, parade. PNJ : de l'hôte vers un client. Joueur : d'un joueur à un autre,
// si le tir ami est activé.
struct Impose {
    uint32_t levelHash;
    uint32_t animID;
    uint8_t byPlayer;        // le coup venait du joueur de l'émetteur, et non d'un PNJ
    uint8_t targetID;        // numéro du joueur qui la subit
};

enum WeaponEventKind : uint8_t {
    kWeaponDropped = 1,      // l'arme en main est tombée au sol
    kWeaponPicked = 2,       // une arme au sol est passée en main ; x, y, z : où elle se trouvait
};
// Depuis la 0.23 chaque arme porte un numéro, le même sur toutes les machines (voir groundmatch.h) :
// celui du joueur qui l'a donné dans les bits de poids fort, puis son compteur. 0 : sans numéro.

// Envoyé à chaque connexion et à chaque (re)chargement de niveau chez l'hôte. Un numéro que le
// client ne connaît pas, pour le niveau où il se trouve : l'hôte a repris à sa dernière
// sauvegarde, le client recharge au même point avec le même état enregistré.
struct LevelInstance {
    uint32_t levelHash;
    uint32_t instance;       // jamais nul, change à chaque chargement
    int32_t savePoint;       // variable de script save_point de l'hôte
    uint8_t data[516];       // état du joueur enregistré à ce point par save_game
    // Numéro du niveau de l'hôte. Un client resté dans un autre niveau (l'hôte a chargé une
    // sauvegarde depuis son menu, ou le client arrive d'ailleurs) le charge de lui-même.
    uint16_t levelNumber;
};

// Pourquoi la mission échoue et reprend à la dernière sauvegarde.
enum RestartReason : uint8_t {
    kRestartDeath = 0,       // un joueur est mort
    kRestartBledOut = 1,     // un joueur à terre n'a pas été ranimé à temps
    kRestartCivilian = 2,    // un joueur a tué un civil
    kRestartJoin = 3,        // un joueur arrive : tout le monde repart du même point
};

// Un client qui arrive dit où en est son niveau. L'hôte en déduit s'il faut repartir ensemble de
// son dernier point de sauvegarde.
struct Join {
    uint32_t levelHash;
    int32_t savePoint;       // point de sauvegarde d'où le client est parti
    uint8_t fresh;           // son niveau vient d'être chargé
};

// Chargement de l'hôte que ce client a rejoint et fini de charger.
struct Ready {
    uint32_t levelHash;
    uint32_t instance;
};

struct Go {
    uint32_t instance;       // chargement de l'hôte qui démarre
};

struct NpcWeapon {
    uint32_t characterID;
    char name[12];           // classe de l'arme, vide : aucune
    uint8_t holstered;       // rangée, et non en main
};

// Une prise qui désarme : l'arme passe des mains du PNJ à celles du joueur sans toucher le sol.
// L'hôte, qui simule le PNJ, la lui retire ; les autres clients la retirent à leur pantin. Sans
// cette annonce le PNJ la gardait chez l'hôte, et il y en avait deux.
struct Disarm {
    uint32_t levelHash;
    uint32_t characterID;    // le PNJ désarmé
    uint16_t ammo;           // cartouches de l'arme au moment où elle est prise
    char name[12];           // classe de l'arme
};

// Ce qu'un PNJ de l'hôte vient de donner au pantin de ce client (un civil à qui son joueur a
// parlé) : c'est à son vrai joueur de le recevoir.
enum GiftKind : uint8_t {
    kGiftPowerup = 0,        // type : celui du moteur (chargeur, cellule, hypo, bouclier, invisibilité, LSI)
    kGiftWeapon = 1,         // weapon : classe de l'arme, amount : ses cartouches
};

struct Gift {
    uint32_t levelHash;
    uint8_t kind;            // GiftKind
    uint8_t type;            // bonus : type du moteur
    uint16_t amount;         // bonus : combien ; arme : cartouches qu'elle contient
    char weapon[16];         // arme : nom de sa classe
};

// Les points de sauvegarde sont ceux de l'hôte : un client n'enregistre rien de lui-même.
struct SavePoint {
    uint32_t levelHash;
    int32_t savePoint;       // variable de script save_point de l'hôte
};

struct RestartRequest {
    uint32_t levelHash;
    uint32_t instance;       // chargement de l'hôte que suit ce client (LevelInstance), 0 : inconnu
    uint8_t reason;          // RestartReason
};

// Ce que l'hôte dit aux clients avant de reprendre : la raison, et le joueur en cause.
struct Alert {
    uint8_t reason;          // RestartReason
    uint8_t player;
};

enum RevivePhase : uint8_t {
    kReviveStart = 1,        // l'émetteur commence : le compte à rebours du joueur à terre s'arrête
    kReviveCancel = 2,       // interrompu (touche relâchée, coup reçu, trop loin) : il reprend
    kReviveDone = 3,         // mené au bout : le joueur à terre se relève
};

// Jugé chez l'émetteur, qui voit le joueur à terre et tient la touche ; appliqué chez ce joueur.
struct Revive {
    uint32_t levelHash;
    uint8_t target;          // numéro du joueur à terre
    uint8_t phase;           // RevivePhase
};

// Ces deux messages suivent le u32 niveau, comme les autres messages d'objets du niveau.
struct WeaponEvent {
    float x, y, z;
    uint8_t kind;            // WeaponEventKind
    char name[33];           // classe de l'arme
    uint16_t ammo;           // arme lâchée : cartouches qu'elle contient encore
    uint16_t id;             // numéro de l'arme ; ramassée : 0 si l'émetteur ne lui en connaît pas
};

struct PowerupEvent {
    float x, y, z;           // où le bonus se trouvait
    uint16_t type;           // type du moteur : munitions, cellule, hypo...
};

struct GroundPowerup {
    float x, y, z;
    uint8_t type;            // type du moteur
    uint16_t amount;         // 1, ou la charge d'un bouclier ou d'une invisibilité
};

struct GroundWeapon {
    float x, y, z;
    uint32_t classHash;      // FNV-1a du nom de la classe d'arme
    uint16_t ammo;           // cartouches qu'elle contient
    uint16_t id;             // numéro de l'arme
};

// Annonce de l'hôte, diffusée toutes les deux secondes sur le port de la partie + 1. Lue par un
// client quelle que soit sa version : c'est elle qui lui dit si la partie lui est ouverte.
enum BeaconState : uint8_t {
    kBeaconWaiting = 0,      // il reste une place
    kBeaconPlaying = 1,      // partie complète
};

struct Beacon {
    uint32_t hostId;         // tiré au démarrage de l'hôte : distingue deux hôtes, ou un hôte relancé
    uint16_t port;           // port UDP de la partie
    uint8_t state;           // BeaconState
    uint8_t players;         // joueurs présents, hôte compris
    uint8_t maxPlayers;
    char name[32];           // nom de la partie
    char modVersion[8];      // version d'OniMP, pour l'afficher ; la compatibilité se lit dans Header.version
    char level[32];          // niveau chargé chez l'hôte
};

enum RejectReason : uint8_t {
    kRejectVersion = 1,      // protocole différent
    kRejectFull = 2,         // toutes les places de client sont prises
};

struct Reject {
    uint32_t session;        // celle du kHello refusé
    uint8_t reason;          // RejectReason
    uint8_t hostVersion;     // protocole de l'hôte
};

// Empreintes de ce qui doit être identique chez l'hôte et chez un client, une par partie de
// l'état. Chaque client envoie les siennes ; l'hôte les compare aux siennes et lui renvoie ce qui
// diffère.
enum DigestPart : uint8_t {
    kDigestLevel,            // chargement de niveau suivi
    kDigestNpcs,             // PNJ vivants
    kDigestConsoles,         // consoles activées
    kDigestGround,           // bonus au sol
    kDigestSettings,         // tir ami, cheats et leurs accès, difficulté, options npcs et quests
    kDigestScripts,          // commandes de script passées sur cette connexion
    kDigestParts,
};

struct Digest {
    uint32_t levelHash;
    uint32_t parts[kDigestParts];
};

struct Resync {
    uint32_t parts;          // un bit par DigestPart
};

struct StreamHeader {
    uint32_t seq;          // 0, 1, 2... depuis l'établissement de la connexion
    uint8_t origin;        // joueur auteur du message : l'émetteur, ou celui dont l'hôte redistribue le message
};

struct StreamAck {
    uint32_t next;         // tous les messages de numéro inférieur ont été reçus
};

#pragma pack(pop)

static_assert(sizeof(Header) == 12, "Header");
static_assert(sizeof(EntityState) == 40, "EntityState");
static_assert(sizeof(NpcInfo) == 36, "NpcInfo");
static_assert(sizeof(Event) == 12, "Event");
static_assert(sizeof(Throw) == 22, "Throw");
static_assert(sizeof(Hit) == 34, "Hit");
static_assert(sizeof(Settings) == 13, "Settings");
static_assert(sizeof(Morph) == 24, "Morph");
static_assert(sizeof(Morphs) == 96, "Morphs");
static_assert(sizeof(Spawn) == 48, "Spawn");
static_assert(sizeof(Profile) == 17, "Profile");
static_assert(sizeof(Roster) == 77, "Roster");
static_assert(sizeof(Ping) == 4, "Ping");
static_assert(sizeof(RestartRequest) == 9, "RestartRequest");
static_assert(sizeof(Alert) == 2, "Alert");
static_assert(sizeof(Revive) == 6, "Revive");
static_assert(sizeof(Join) == 9, "Join");
static_assert(sizeof(Ready) == 8, "Ready");
static_assert(sizeof(Go) == 4, "Go");
static_assert(sizeof(Gift) == 24, "Gift");
static_assert(sizeof(SavePoint) == 8, "SavePoint");
static_assert(sizeof(NpcWeapon) == 17, "NpcWeapon");
static_assert(sizeof(LevelInstance) == 530, "LevelInstance");
static_assert(sizeof(DefenseState) == 3, "DefenseState");
static_assert(sizeof(LaserTrip) == 7, "LaserTrip");
static_assert(sizeof(WeaponEvent) == 50, "WeaponEvent");
static_assert(sizeof(PowerupEvent) == 14, "PowerupEvent");
static_assert(sizeof(Impose) == 10, "Impose");
static_assert(sizeof(Welcome) == 6, "Welcome");
static_assert(sizeof(StreamHeader) == 5, "StreamHeader");
static_assert(sizeof(GroundPowerup) == 15, "GroundPowerup");
static_assert(sizeof(GroundWeapon) == 20, "GroundWeapon");
static_assert(sizeof(Beacon) == 81, "Beacon");
static_assert(sizeof(Reject) == 6, "Reject");
static_assert(sizeof(Digest) == 4 + 4 * kDigestParts, "Digest");

// Chaque datagramme reste sous 1400 octets.
constexpr int kMaxEntitiesPerPacket = 34;
constexpr int kMaxInfosPerPacket = 32;
constexpr int kMaxStreamPayload = 1200;

}  // namespace proto
