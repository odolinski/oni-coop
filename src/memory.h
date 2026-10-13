// Lecture / écriture de l'état du jeu dans la mémoire d'Oni.exe.
#pragma once

#include "log.h"
#include "oni.h"
#include "protocol.h"

namespace mem {

// --- Patch --------------------------------------------------------------------------------------
bool ExeSupported();
// Redirige un "call rel32" (opcode E8) vers target. Renvoie l'ancienne cible dans previous.
bool PatchCall(uintptr_t site, void* target, void** previous);
void* CallTarget(uintptr_t site);  // nullptr si l'octet n'est pas E8
// Remplace, dans la table d'imports de module, chaque entrée pointant sur original.
// Renvoie le nombre d'entrées modifiées.
int PatchImports(void* module, void* original, void* replacement);
// Détourne une fonction vers hook. expected : ses length premiers octets (5 au moins), qui doivent
// former des instructions entières et recopiables ailleurs. original reçoit de quoi l'appeler encore.
bool HookEntry(uintptr_t address, const uint8_t* expected, size_t length, void* hook, void** original);
// Remplace une adresse de fonction écrite dans le code (opérande immédiat). Faux, sans rien
// modifier, si l'adresse en place n'est pas expected.
bool PatchPointer(uintptr_t site, void* expected, void* replacement);

// --- État global --------------------------------------------------------------------------------
oni::GameState* Game();
// Notre joueur : le personnage du joueur dans ce niveau. Ce n'est pas toujours celui que le moteur
// tient pour tel : une cinématique peut désigner un autre personnage à suivre (chr_focus), et
// jusqu'à la 0.30 le mod le prenait alors pour notre joueur, lui donnait sa tenue et envoyait son
// état aux autres.
oni::Character* Player();
// Le personnage que le moteur suit en ce moment : notre joueur, sauf pendant un chr_focus.
oni::Character* FocusedCharacter();
int32_t GameTime();
bool Paused();
uint32_t LevelHash();        // 0 si aucun niveau
const char* LevelName();
// Faux : les touches du joueur sont ignorées, comme pendant une cinématique.
bool InputEnabled();
void SetInputEnabled(bool enabled);
// Arrête ou relance la simulation du moteur ; l'affichage et notre boucle continuent. À la
// relance, le temps passé à l'arrêt n'est pas rattrapé.
void SetPaused(bool paused);
// Écran d'accueil du niveau que la boucle du jeu s'apprête à montrer : le retire. Vrai s'il y en
// avait un.
bool DropSplash();
// Difficulté que le moteur applique en ce moment : 0 facile, 1 normal, 2 difficile.
int Difficulty();
// Ce que cette difficulté multiplie dans la vie d'un ennemi à sa création ; 1 si inconnu.
float EnemyHealthFactor(int difficulty);
// Détourne la lecture de la difficulté par le moteur : level répond ce qui vaut, ou un nombre
// négatif pour laisser celle des options du jeu. Faux si le code du moteur n'est pas celui attendu
// (rien n'est alors modifié).
bool InstallDifficultyHook(int (*level)());

// --- Personnages --------------------------------------------------------------------------------
int CharacterCount();
oni::Character* CharacterAt(int index);
bool InUse(oni::Character* chr);
bool IsDead(oni::Character* chr);
bool IsAI(oni::Character* chr);
uint16_t Handle(oni::Character* chr);
const char* Name(oni::Character* chr);
int32_t Health(oni::Character* chr);
const char* ClassName(oni::Character* chr);        // classe de personnage, "" si inconnue
// Donne au personnage une autre classe (une autre tenue de Konoko), en place. Faux si elle n'est
// pas dans les données chargées.
bool SetClass(oni::Character* chr, const char* className);
bool HasScripts(oni::Character* chr);              // un script du niveau lui est attaché
oni::Vector3 Location(oni::Character* chr);
oni::ActiveCharacter* Active(oni::Character* chr);

void ReadEntity(oni::Character* chr, uint32_t characterID, proto::EntityState& out);
// Sans effet si position ou facing n'est pas un nombre. Une position du personnage qui n'en est
// plus un est réécrite telle quelle, et ses élans invalides remis à zéro.
void WriteTransform(oni::Character* chr, const oni::Vector3& position, float facing);
// Position, place, orientation et vitesse du personnage sont des nombres (ni NaN ni infini).
bool TransformValid(oni::Character* chr);
// Note la place du personnage quand elle est saine, l'y ramène quand elle ne l'est plus. Le moteur
// ne s'en remet jamais seul, et sa boussole boucle sans fin sur un angle invalide : c'était le jeu
// figé des clients de la 0.28. when : pour le journal. Vrai s'il a fallu le ramener.
bool GuardTransform(oni::Character* chr, const char* when);
bool GuardPlayerTransform(const char* when);   // notre joueur : à chaque tick
int GuardTransforms(const char* when);         // tous : à chaque image, avant le tracé ; rend combien ont été ramenés

// --- Armes --------------------------------------------------------------------------------------
const char* WeaponInHand(oni::Character* chr);     // nom de la classe d'arme, "" si aucune
const char* WeaponStored(oni::Character* chr);     // arme rangée, "" si aucune
// Direction de l'arme d'un pantin, dans le monde, en radians. À rappeler à chaque image : sans
// rappel pendant une demi-seconde, le pantin retrouve la visée du moteur.
void SetAim(oni::Character* chr, float yaw, float pitch);
void ClearAim(oni::Character* chr);                // regarde droit devant
void ForgetAims();                                 // à chaque chargement de niveau
// Fait appliquer par le moteur les visées données à SetAim, à chaque tick. Faux si le code du
// moteur n'est pas celui attendu (rien n'est alors modifié).
bool InstallAimHook();
// Bouclier de force (100 neuf) et invisibilité (en ticks) : ce qu'il en reste au personnage. Un
// pantin reçoit ceux du personnage qu'il représente : le moteur en tire la lueur du bouclier, l'arrêt
// des tirs, la transparence, et ce qu'en voient les IA.
uint16_t Shield(oni::Character* chr);
void SetShield(oni::Character* chr, uint16_t amount);
uint16_t Cloak(oni::Character* chr);
void SetCloak(oni::Character* chr, uint16_t ticks);
// Réserves : chargeurs de balles, cellules d'énergie, hypos. Six de chaque au plus dans le moteur.
struct Reserves {
    uint16_t clips, cells, hypos;
};
Reserves ReadReserves(oni::Character* chr);
void ClearReserves(oni::Character* chr);
// Donne un bonus au personnage, comme le fait un PNJ : type du moteur, quantité. Borné par ce qu'il
// peut porter ; son et message à l'écran si c'est notre joueur. Rend ce qu'il a réellement reçu.
uint16_t GivePowerup(oni::Character* chr, uint16_t type, uint16_t amount);
// Interaction « neutre » (un civil ou un allié qui parle, donne un objet) : le personnage avec qui
// chr est en train de parler, nullptr sinon.
oni::Character* NeutralPartner(oni::Character* chr);
// La touche d'action de who devant le PNJ qui lui fait face : celui-ci engage la conversation avec
// lui, ou la poursuit. Vaut pour un pantin : le moteur, lui, ne connaît que le joueur de la partie.
// Vrai s'il y avait un PNJ à qui parler.
bool NeutralAction(oni::Character* who);
void SetInfiniteAmmo(oni::Character* chr);         // pantin qui tire sans gérer de chargeur
bool HasInfiniteAmmo(oni::Character* chr);
void ClearInfiniteAmmo(oni::Character* chr);
void ClearDrops(oni::Character* chr);              // il ne laissera aucun bonus en mourant, son arme seulement
bool Reloading(oni::Character* chr);               // animation de rechargement en cours
// La joue sur un pantin, pour l'arme qu'il tient. Autre chose que Played ou Already : elle n'a pas
// pu l'être, et pourquoi.
enum class ReloadResult { Played, Already, Inactive, NoWeapon, NoAnimation };
// Donne au pantin la variante d'animation de l'arme qu'il tient, que le moteur ne pose qu'à un
// personnage qui sort lui-même son arme.
void SyncWeaponVariant(oni::Character* chr);
ReloadResult PlayReload(oni::Character* chr);
// Aura du personnage « surpuissant », de 0 à 1 : celle que le moteur montre en ce moment, et celle
// qu'on lui demande de montrer (un pantin n'a pas la santé du joueur qu'il représente).
float SuperShown(oni::Character* chr);
void SetSuper(oni::Character* chr, float amount);
// Immédiats, sans animation : rien que le moteur puisse refuser en cours d'attaque.
void StoreWeapon(oni::Character* chr, bool store); // range l'arme en main, ou sort l'arme rangée
void* DropWeapon(oni::Character* chr);             // l'arme en main tombe au sol ; la renvoie, nullptr si aucune
uint16_t WeaponAmmo(void* weapon);                 // cartouches restantes dans l'arme
void SetWeaponAmmo(void* weapon, uint16_t ammo);   // bornées à la contenance de l'arme
void TakeWeapon(oni::Character* chr, void* weapon);  // prend en main une arme au sol
// Classe de l'arme : elle se recharge avec des cellules d'énergie (sinon des chargeurs de balles) ;
// son tir explose (à ne pas tirer de près).
bool WeaponUsesCells(void* weapon);
bool WeaponExplosive(void* weapon);
bool WeaponHolsterable(void* weapon);              // faux pour une arme qui ne se range pas (celle de Barabas)
// Range l'arme en main (store) ou sort l'arme rangée, comme le fait la touche d'échange d'arme
// quand il n'y a rien à ramasser : par l'animation du moteur, l'arme changeant de place en cours
// de geste. Vrai si le geste est lancé ; faux s'il n'y a rien à ranger ou à sortir, si l'arme ne
// se range pas, ou si une animation d'arme (rechargement, autre geste) est déjà en cours.
bool HolsterWeapon(oni::Character* chr, bool store);
// Visée verticale du joueur, en radians, positive vers le haut : ce que sa souris règle.
void SetPlayerPitch(oni::Character* chr, float pitch);

// --- Objets au sol ------------------------------------------------------------------------------
void* WeaponAt(int index);                         // case de la table des armes, nullptr hors table
int WeaponIndex(void* weapon);                     // -1 si ce n'est pas une case de la table
bool WeaponInUse(void* weapon);                    // case occupée, au sol ou chez un personnage
bool WeaponOnGround(void* weapon);                 // en service et sans propriétaire
oni::Character* WeaponOwner(void* weapon);         // qui la tient ou l'a rangée, nullptr au sol
const char* WeaponClassName(void* weapon);         // "" si inconnue
oni::Vector3 WeaponPosition(void* weapon);
// L'arme au sol de cette classe la plus proche de position, dans le rayon donné ; except n'est
// jamais renvoyée.
void* FindGroundWeapon(const char* className, const oni::Vector3& position, float radius, void* except = nullptr);
void DeleteWeapon(void* weapon);                   // retire une arme au sol
int PowerupCount();
void ReadPowerup(int index, uint16_t& type, uint16_t& amount, oni::Vector3& position);
// Pose un bonus à cet endroit. Faux si le moteur le refuse.
bool NewPowerup(uint16_t type, uint16_t amount, const oni::Vector3& position);
// Déplace un bonus posé ; sans effet sur un bonus en train de tomber.
void MovePowerup(int index, const oni::Vector3& position);
// Retire le bonus de ce type le plus proche de position, dans le rayon donné. Faux si aucun.
bool DeletePowerupNear(uint16_t type, const oni::Vector3& position, float radius);
// Fait ramasser au personnage le bonus de ce type posé en position, par la routine du moteur et à
// ses conditions : à portée de main, et s'il lui reste de la place. Vrai si le bonus a quitté le
// sol. Le moteur ne ramasse un bonus qu'à la touche d'échange d'arme, jamais au seul passage.
bool TakePowerup(oni::Character* chr, uint16_t type, const oni::Vector3& position);
enum class TriggerEvent { Pressed, Held, Released, AltPressed, AltReleased };
void Trigger(oni::Character* chr, TriggerEvent event);

// --- Ligne de vue -------------------------------------------------------------------------------
enum class RayKind { Nothing, Character, Environment, Object };
struct Ray {
    RayKind kind;
    oni::Character* chr;     // personnage touché, s'il a pu être reconnu
    oni::Vector3 point;      // où le rayon s'arrête
    float distance;          // de l'origine à ce point
};
// Premier obstacle sur la ligne qui va de origin vers target, comme le voit le viseur laser du
// moteur : un personnage (jamais from, celui qui regarde), un objet du niveau ou le décor. Faux
// s'il n'y a pas de rayon à tirer (points confondus, nombres invalides) ; rien de touché : kind
// vaut Nothing.
bool CastRay(oni::Character* from, const oni::Vector3& origin, const oni::Vector3& target, Ray& out);
// Sprint du joueur. on : il sprinte tant qu'il tient la touche « avancer », ce que donne chez un
// joueur le double appui sur cette touche ; sinon il revient à la course.
void Sprint(oni::Character* chr, bool on);

// --- Animations ---------------------------------------------------------------------------------
void RebuildAnimTable();                 // à chaque chargement de niveau
uint32_t AnimHash(void* tram);
void* AnimFromHash(uint32_t hash);
const char* AnimName(void* tram);
void* CurrentAnimation(oni::Character* chr);
int CurrentFrame(oni::Character* chr);
int AnimDuration(void* tram);            // en frames
bool SetAnimation(oni::Character* chr, void* tram, int interpFrames);
void SetAnimFrame(oni::Character* chr, int frame);
// Verrouillé : le moteur ne choisit plus lui-même l'animation suivante du personnage.
void LockAnimation(oni::Character* chr, bool locked);
// Au sol, sur le dos ou sur le ventre, ou en train d'y tomber. resting, si non nul : il y est et
// n'en bouge plus.
bool Fallen(oni::Character* chr, bool* resting = nullptr);
// Le couche sur le dos, là où il est. Faux si sa classe n'a pas l'animation.
bool LayDown(oni::Character* chr);
// Animation jouée au sol de bout en bout : elle part d'un état couché et y revient. C'est le repos
// au sol, ou un coup reçu à terre ; jamais une chute ni un relevé.
bool GroundAnimation(void* tram);

// --- Projections --------------------------------------------------------------------------------
bool IsThrowAnimation(void* tram);       // animation de lanceur
// Vrai si chr est en train de projeter quelqu'un : target et l'animation que le moteur lui a imposée.
bool ThrowInProgress(oni::Character* chr, oni::Character*& target, void*& targetAnim);
// Cible de la prochaine animation de lanceur posée sur chr ; nullptr : aucune.
void SetThrowTarget(oni::Character* chr, oni::Character* target, void* targetAnim);

// --- Actions ------------------------------------------------------------------------------------
// Personnage créé de toutes pièces. osdFlags : drapeaux de son objet CHAR (off::kOsdFlag*) ; par
// défaut ceux d'un pantin, increvable et aux munitions infinies.
oni::Character* SpawnPuppet(const char* className, const char* name, const oni::Vector3& position,
                            float facing, uint16_t team,
                            uint32_t osdFlags = oni::off::kOsdFlagUnkillable | oni::off::kOsdFlagInfiniteAmmo);
// Objet CHAR du niveau qui porte ce nom, celui que ai2_spawn fait apparaître ; nullptr si aucun.
void* FindCharacterObject(const char* name);
// Copie d'un personnage du niveau, créée d'après son objet CHAR : même classe, même arme, même
// rôle, sous un autre nom et à un autre endroit, sans aucun des scripts qui lui sont attachés.
// team : équipe du personnage créé ; négatif, celle de l'objet du niveau.
oni::Character* SpawnCopy(void* object, const char* name, const oni::Vector3& position, float facing, int team = -1);
// PNJ créé de toutes pièces, avec une IA qui se bat : un combattant du niveau lui sert de modèle
// (ses réglages de combat et d'alerte), dont on change la classe, le nom et l'équipe, sans arme ni
// script. Sans combattant dans le niveau : un personnage aux réglages vides.
oni::Character* SpawnNpc(const char* className, const char* name, const oni::Vector3& position, float facing,
                         uint16_t team);
// Arme neuve de cette classe posée au sol, par la routine de la commande de script weapon_spawn.
// nullptr si la classe est inconnue ou si le moteur n'a plus de place.
void* SpawnWeapon(const char* className, const oni::Vector3& position, float facing);
uint16_t WeaponCapacity(void* weapon);             // cartouches d'une arme pleine
// Classes de personnages (ONCC) et d'armes (ONWC) présentes dans les données chargées : leur nombre,
// et le nom de chacune ("" hors table).
int CharacterClassCount();
const char* CharacterClassAt(int index);
int WeaponClassCount();
const char* WeaponClassAt(int index);
void DeleteCharacter(oni::Character* chr);
void SetHealth(oni::Character* chr, uint32_t hitPoints);
void Kill(oni::Character* chr);

// --- Dégâts -------------------------------------------------------------------------------------
struct DamageInfo {
    oni::Character* victim;
    oni::Character* attacker;        // nullptr : décor, chute, script
    uint32_t damage;                 // le filtre peut le modifier
    uint32_t ownerType;              // oni::fn::kOwnerWeapon, kOwnerMelee...
    uint32_t impact;
    float knockback;
    const oni::Vector3* direction;   // poussée, ou nullptr
    // Faux : dégâts portés par l'animation de la victime (chute d'une projection), attribués à
    // son lanceur comme un coup mais sans point d'impact.
    bool hit;
};
typedef void (*DamageFilter)(DamageInfo& info);
// Fait passer par filter tous les dégâts que le moteur inflige. Faux si le code du moteur n'est
// pas celui attendu (rien n'est alors modifié).
bool InstallDamageHook(DamageFilter filter);
// Les tirs d'un personnage pour lequel isPuppet répond vrai ne consultent plus son IA : chez un
// pantin elle est à l'arrêt, dans un état qui n'a pas prévu de tirer, et le moteur y lirait un
// pointeur nul. Faux si le code du moteur n'est pas celui attendu.
bool InstallShooterHook(bool (*isPuppet)(oni::Character*));

// --- Sauvegarde et reprise ----------------------------------------------------------------------
// Dernier point de sauvegarde atteint et état du joueur qui y a été enregistré
// (oni::addr::kContinueSize octets).
void ReadContinue(int32_t& savePoint, uint8_t* data);
void WriteContinue(int32_t savePoint, const uint8_t* data);
// Recharge le niveau à ce point, comme la commande de script lose.
void RestartLevel();
// Zones de texte du moteur (ligne de commande, console, messages du niveau, sous-titres) : à chaque
// image, et juste avant que le moteur ne trace ses messages, celle qui a perdu ses lignes ou son
// contexte est refaite à neuf d'après ce qu'elle était, et le journal dit dans quel état elle a
// été trouvée. Le moteur la trace et y écrit sans rien contrôler : c'était le plantage du client
// des 0.23 et 0.24.
void GuardTextAreas();
// Fait passer cette garde juste avant l'affichage des messages du niveau. Faux si le code du
// moteur n'est pas celui attendu ; la garde de chaque image reste.
bool InstallTextAreaGuard();
// Fermeture du jeu : écarte une zone de texte du moteur restée sans lignes, dont la libération le
// ferait planter, et note l'état des quatre au journal. Faux si le code du moteur n'est pas celui
// attendu (rien n'est alors modifié).
bool InstallExitGuard();
// Objet du niveau (LSI), que les scripts cherchent sur le joueur de l'hôte.
bool HasLsi(oni::Character* chr);
void GiveLsi(oni::Character* chr);
// Équipe et drapeaux de l'IA d'un personnage.
uint16_t Team(oni::Character* chr);
void SetTeam(oni::Character* chr, uint16_t team);
// Vrai si cette IA a un contact assez sûr (vu ou entendu de près) avec ce personnage.
bool Knows(oni::Character* ai, oni::Character* other);
// Personnages que cette IA tient pour des menaces, connus avec assez de sûreté. Rend leur nombre.
int Threats(oni::Character* ai, oni::Character** out, int capacity);

// Inflige des dégâts sans passer par le filtre. Sans attaquant : acte de script, qui tue même un
// personnage increvable.
void Damage(oni::Character* chr, uint32_t amount, oni::Character* attacker = nullptr,
            uint32_t ownerType = oni::fn::kOwnerMelee, const oni::Vector3* direction = nullptr,
            float knockback = 0.0f, uint32_t impact = 0);

bool Exec(const char* format, ...) ONIMP_PRINTF(1, 2);  // ligne BSL, hors de l'historique de la console
// Corrige dans le moteur le débordement de son historique de commandes, qui écrasait après 256
// commandes la zone des messages du niveau : c'était le plantage du client. Faux si le code du
// moteur n'est pas celui attendu ; nos propres commandes restent alors hors de l'historique.
bool FixCommandHistory();
// Fait passer par notify(console, user) toute activation d'une console par un personnage, avant
// que le moteur ne lance son script. Faux si le code du moteur n'est pas celui attendu.
bool InstallConsoleHook(void (*notify)(void* console, oni::Character* user));
// Une cinématique du niveau est en cours.
bool InCutscene();
// Garde un personnage actif où que soit notre joueur (ce que fait chr_lock_active), ou le rend au
// moteur. Faux si le moteur n'a plus de place pour un personnage actif de plus.
bool LockActive(oni::Character* chr, bool locked);
// Le moteur ne trace plus les personnages pour lesquels hidden rend vrai. Faux si son code n'est
// pas celui attendu.
bool InstallVisibilityHook(bool (*hidden)(oni::Character* chr));
// Portraits des dialogues : place(code) rend la place à donner à la place de celle que demande le
// script (voir addr::kInsertStartCallSite). Faux si le code du moteur n'est pas celui attendu.
bool InstallInsertHook(uint8_t (*place)(uint8_t code));
// Touches du joueur telles que le moteur les donne chaque tick à son personnage.
struct GameInput {
    float mouseX, mouseY;      // déplacement de la souris
    float unknown1;
    int32_t unknown2;
    uint64_t down;             // actions tenues (voir kAction*)
    uint64_t up;               // leur contraire
    uint64_t pressed;          // enfoncées à ce tick
    uint64_t released;         // relâchées à ce tick
};
constexpr uint64_t kActionForward = 0x200000;
constexpr uint64_t kActionBackward = 0x400000;
constexpr uint64_t kActionJump = 0x8000000;
constexpr uint64_t kActionCrouch = 0x10000000;
constexpr uint64_t kActionPunch = 0x20000000;
constexpr uint64_t kActionKick = 0x40000000;
constexpr uint64_t kActionHypo = 0x400000000ull;
constexpr uint64_t kActionReload = 0x800000000ull;
constexpr uint64_t kActionDrop = 0x2000000000ull;
constexpr uint64_t kActionFire = 0x4000000000ull;
constexpr uint64_t kActionsUtility = 0x1FFFFF;   // menu, console, captures : tout ce qui n'est pas du jeu
// filter(joueur, touches) est appelé chaque tick avant que le moteur ne les applique, et peut les
// changer. Faux si le code du moteur n'est pas celui attendu.
bool InstallPlayerInputHook(void (*filter)(oni::Character* player, GameInput& input));
// Mise à jour d'un personnage actif, à chaque tick : d'abord ses touches, puis tout le reste
// (saut, gravité, pas de l'animation, image suivante). filter est appelé avant et après chacun des
// deux temps, pour chaque personnage. Faux si le code du moteur n'est pas celui attendu.
enum class StepPhase { BeforeInput, AfterInput, BeforeStep, AfterStep };
bool InstallStepHook(void (*filter)(oni::Character* chr, StepPhase phase));
int AnimAttacks(void* tram);             // coups que porte l'animation, 0 si ce n'est pas une attaque

// --- Lasers, tourelles, alarmes -----------------------------------------------------------------
// Le moteur ne teste ses lasers que contre le joueur de la partie. candidates(joueur, out,
// capacité) dit contre qui les tester à sa place et rend leur nombre : zéro, le laser ne détecte
// personne ; négatif, le moteur fait comme d'ordinaire. Faux si son code n'est pas celui attendu.
bool InstallLaserHook(int (*candidates)(oni::Character* player, oni::Character** out, int capacity));
// Le dernier personnage qu'un laser a coupé par ce détour, et à quel tick du jeu ; nullptr si aucun.
oni::Character* LastLaserHit(int32_t* gameTime);
// Un laser vient d'être coupé, et le moteur va lancer ses événements, au nom du joueur de la
// partie quel que soit le personnage testé : allowed(laser, personnage) dit s'il le peut, et
// tripped(laser, personnage) est appelé aussitôt après s'il l'a fait. Faux si le code du moteur
// n'est pas celui attendu.
bool InstallLaserTripHook(bool (*allowed)(void* laser, oni::Character* cause),
                          void (*tripped)(void* laser, oni::Character* cause));
// Lasers du niveau, dans l'ordre où le moteur les range : le même sur toutes les machines.
int Lasers(void** out, int capacity);
// Les deux choses que le moteur fait quand ce laser est coupé : jouer son son (faux si sa classe
// n'en a pas), et lancer ses événements au nom de ce personnage.
bool PlayLaserSound(void* laser);
void RunLaserEvents(void* laser, oni::Character* cause);
// Numéros des tourelles que ce laser allume quand il est coupé (ses événements « Activate
// Turret »). Rend leur nombre.
int LaserTurrets(void* laser, uint16_t* out, int capacity);
// Rayons d'un laser mobile : où en est chacun sur son rail, en images, et dans quel sens il va
// (+1 ou -1). Rend leur nombre ; zéro pour un laser fixe, pour un laser qui en a plus que capacity,
// ou si ce qu'on y lit n'est pas une position sur le rail et un sens.
int LaserRays(void* laser, float* frames, int8_t* directions, int capacity);
// Les y remet, s'il en a bien count. Une position hors du rail y est ramenée.
void SetLaserRays(void* laser, const float* frames, const int8_t* directions, int count);
int LaserRailFrames(void* laser);        // longueur de son rail en images, 0 s'il n'en a pas
float LaserSpeed(void* laser);           // images de rail par tick
bool LaserLoops(void* laser);            // au bout du rail il repart du début ; sinon il fait demi-tour
// La tourelle pourrait-elle tirer sur ce personnage, d'après le test du moteur (tout près, ou
// sans décor entre eux) ? Sa cible du moment n'est pas changée.
bool TurretCanShoot(void* turret, oni::Character* target);
// Fait passer par tripped(alarme, cause) toute alarme donnée dans le moteur, par la commande de
// script ai2_tripalarm comme par l'événement d'un laser ou d'une console, après qu'il a prévenu
// ses IA. Faux si son code n'est pas celui attendu.
bool InstallAlarmHook(void (*tripped)(uint8_t alarm, oni::Character* cause));
// État d'un groupe de lasers ou de tourelles de même numéro.
enum class DefenseKind : uint8_t { Laser, Turret };
struct Defense {
    DefenseKind kind;
    uint16_t id;
    bool active;     // allumé, pour l'un au moins des objets du numéro
    bool hit;        // laser : un joueur le coupe en ce moment
};
// Relève lasers et tourelles du niveau, un par genre et par numéro. Rend leur nombre.
int ReadDefenses(Defense* out, int capacity);
void SetDefense(DefenseKind kind, uint16_t id, bool active);
// Cible d'une tourelle : choose(tourelle, choix du moteur) rend celle qu'elle doit prendre, nul
// pour aucune. Faux si le code du moteur n'est pas celui attendu.
bool InstallTurretTargetHook(oni::Character* (*choose)(void* turret, oni::Character* engineChoice));
// Une tourelle ne tire que si le drapeau « toutes les IA passives » du moteur est nul. Après cet
// appel elle lit à sa place l'octet flag : nul, elle tire ; non nul, elle cesse le feu. Faux, sans
// rien modifier, si le code du moteur n'est pas celui attendu.
bool InstallTurretFireFlag(uint8_t* flag);
bool AiAllPassive();                     // le drapeau du moteur, celui d'ai2_allpassive

// Tourelles du niveau, dans l'ordre où le moteur les range : le même sur toutes les machines.
int Turrets(void** out, int capacity);
// Personnages vivants parmi lesquels le moteur cherche la cible d'une tourelle.
int LivingCharacters(oni::Character** out, int capacity);
// Numéro du niveau en cours, et chargement d'un autre : comme une reprise après un échec, mais
// dans ce niveau-là, au point de sauvegarde donné par WriteContinue.
uint16_t LevelNumber();
void LoadLevel(uint16_t number);
// Porte de six à douze ce qu'un personnage peut porter de chargeurs et de cellules ; les hypos
// restent à six. Tout ou rien : faux, sans rien changer, si le code du moteur n'est pas celui
// attendu.
bool RaiseReserveCapacity();
int ReserveCapacity();                   // chargeurs, et autant de cellules, qu'un personnage peut porter
constexpr int kHypoCapacity = 6;
bool InsideExec();                       // vrai pendant qu'une ligne lancée par Exec s'exécute

// --- Messages à l'écran -------------------------------------------------------------------------
// Les messages du mod ont leur propre zone de texte, en haut à droite de l'écran, en vert, alignés
// à droite, le plus récent en bas. La console du moteur, en bas de l'écran, n'est pas touchée : les
// sous-titres des dialogues du niveau y passent.
// Au chargement de la DLL : fait tracer cette zone à la suite de la console du moteur. Faux si le
// code du moteur n'est pas celui attendu ; les messages vont alors dans la console du moteur.
bool InstallMessageHook();
// À chaque image : use les messages affichés, qui s'effacent d'eux-mêmes après dix secondes.
void UpdateMessages();
// Écrit dans le journal, et à l'écran si les messages y sont affichés.
void ConsolePrint(const char* format, ...) ONIMP_PRINTF(1, 2);
// Messages affichés à l'écran ou non, sur cette machine. Le journal les garde dans les deux cas.
void ShowMessages(bool shown);
bool MessagesShown();

uint32_t Fnv1a(const char* text, size_t maxLength = 256);

}  // namespace mem
