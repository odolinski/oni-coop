// Règles de la partie coop (sans accès au jeu) : les quatre modes de difficulté et ce que chacun
// vaut, le compte à rebours d'un joueur à terre, le multiplicateur d'ennemis et son minimum selon
// le nombre de joueurs, les noms des renforts et des civils que le mod ajoute au niveau.
#pragma once

#include <cstddef>
#include <cstdint>

namespace rules {

// -------------------------------------------------------------------------------------------------
// Modes de difficulté
// -------------------------------------------------------------------------------------------------
enum Mode : uint8_t { kEasy, kNormal, kHard, kSurvival, kModeCount };

struct ModeRules {
    const char* name;          // dans le menu
    const char* key;           // dans onimp.ini
    double bleedSeconds;       // temps laissé pour ranimer un joueur à terre
    uint32_t revivePercent;    // santé du joueur ranimé, en pour cent de son maximum
    // Difficulté du jeu d'origine dont viennent les ennemis : 0 facile, 1 normal, 2 difficile.
    int engineLevel;
    // Au-delà de ce que donne le jeu d'origine, en pour cent : 100, rien de plus.
    uint32_t healthPercent;    // vie des PNJ
    uint32_t damagePercent;    // leurs dégâts sur les joueurs
    uint32_t regenEnemy;       // points de vie rendus par seconde à un ennemi ordinaire
    uint32_t regenBoss;        // à un boss
};

const ModeRules& Of(uint8_t mode);                    // un mode inconnu vaut kNormal
uint8_t ModeFromKey(const char* key, uint8_t fallback);
uint8_t NextMode(uint8_t mode);                       // le suivant dans le menu, en boucle

// Un coéquipier ranime un joueur à terre en gardant la touche enfoncée ce temps-là, sans coupure.
constexpr double kReviveHold = 3.0;

// Santé d'un joueur ranimé : jamais moins de deux points, un seul le ferait retomber aussitôt.
uint32_t RevivedHealth(uint32_t maxHealth, uint32_t percent);

// Compte à rebours d'un joueur à terre. Il s'arrête tant qu'un coéquipier le ranime, et reprend où
// il en était si la réanimation est interrompue.
class Bleed {
public:
    void Start(double seconds);
    // elapsed : secondes écoulées depuis l'appel précédent. Vrai quand le temps est épuisé.
    bool Tick(double elapsed, bool paused);
    double remaining() const { return remaining_; }
    double total() const { return total_; }

private:
    double remaining_ = 0.0;
    double total_ = 0.0;
};

// Le compte à rebours voyage dans l'état du joueur, à la place de sa santé : en dixièmes de seconde.
int16_t BleedToWire(double seconds);
double BleedFromWire(int16_t tenths);

// -------------------------------------------------------------------------------------------------
// Multiplicateur d'ennemis
// -------------------------------------------------------------------------------------------------
constexpr int kMinEnemies = 1;
constexpr int kMaxEnemies = 4;

// Minimum imposé par le nombre de joueurs présents, hôte compris : x2 à deux, x3 à trois, x4 à
// quatre. Seul, l'hôte choisit librement.
int MinEnemies(int players);
// Ce qui vaut en jeu : le choix de l'hôte, relevé au minimum.
int EffectiveEnemies(int chosen, int players);
// Choix suivant dans le menu, parmi ceux que le nombre de joueurs permet, en boucle.
int NextEnemies(int chosen, int players);

// -------------------------------------------------------------------------------------------------
// Personnages ajoutés par le mod
// -------------------------------------------------------------------------------------------------
constexpr size_t kNameSize = 32;   // nom d'un personnage du moteur, zéro final compris

// Renfort : copie d'un ennemi du niveau. Il porte son nom suivi de _x2, _x3 ou _x4. Faux si le nom
// de l'original est trop long pour cela.
bool CloneName(const char* original, int rank, char* out, size_t size);
// Vrai si name a cette forme : original reçoit le nom de l'ennemi copié, rank son rang.
bool ParseCloneName(const char* name, char* original, size_t size, int* rank);

// Ennemi d'une vague d'alarme : copie d'un ennemi du niveau, comme un renfort, sous son nom suivi
// de _w00 à _w99. Faux si le nom de l'original est trop long pour cela.
bool WaveName(const char* original, int index, char* out, size_t size);
bool ParseWaveName(const char* name, char* original, size_t size);
// Ennemis qu'une alarme amène : trois par joueur présent, hôte compris.
constexpr int kWavePerPlayer = 3;
int WaveSize(int players);

// Rayon d'un laser mobile : écart entre deux positions sur son rail, en images. Un rail en boucle
// se referme : sa dernière image touche la première.
float RayGap(float a, float b, int railFrames, bool loops);
// Écart au-delà duquel un client recale son rayon sur celui de l'hôte : ce que le rayon parcourt
// en un dixième de seconde, et un peu plus, pour ne pas le faire sautiller à chaque relevé.
float RayTolerance(float speed);
// Où sera le rayon dans ticks ticks, d'après le pas du moteur : il avance de speed images dans son
// sens (+1 ou -1) ; au bout du rail il repart du début si le rail est en boucle, sinon fait
// demi-tour. Sert à ramener la position dite par l'hôte à l'instant où le client la reçoit.
void AdvanceRay(float* frame, int* direction, float speed, int ticks, int railFrames, bool loops);

// Cible d'une tourelle, choisie par l'hôte parmi les joueurs qu'elle peut viser. Dans l'ordre :
// le plus proche de ceux qui viennent de couper un de ses lasers et qu'elle peut toucher (le piège
// s'en prend à qui l'a déclenché) ; sinon le plus proche de ceux qu'elle peut toucher ; faute de
// mieux le plus proche tout court, qu'elle suivra sans tirer. À chaque rang la cible du moment est
// gardée, si elle en est, tant que l'autre n'est pas keepRatio fois plus près : une tourelle
// n'hésite pas entre deux joueurs à même distance.
struct TurretCandidate {
    float distance;      // de la tourelle à lui
    bool reachable;      // elle pourrait lui tirer dessus
    bool tripped;        // il vient de couper un laser de cette tourelle
};
enum TurretChoice : uint8_t { kTurretNobody, kTurretTripper, kTurretReachable, kTurretNearest };
// current : rang dans la liste de la cible du moment, -1 si elle n'en a pas ou n'y est pas. Rend
// le rang retenu, -1 pour personne.
int ChooseTurretTarget(const TurretCandidate* list, int count, int current, float keepRatio, TurretChoice* why);

// Civil : mpcNN_ suivi de sa classe de personnage, que la machine qui le crée d'après ce seul nom
// y retrouve.
bool CivilianName(int index, const char* className, char* out, size_t size);
bool ParseCivilianName(const char* name, char* className, size_t size);

// Barabas, Muro, Muro transformé, Mukade et le Muro du rêve, par leur classe de personnage.
bool IsBossClass(const char* className);

// Classes des civils qui ont leur place dans ce niveau (bureaux, aéroport, laboratoires...) ;
// nullptr et zéro dans un niveau où ils n'en ont pas.
const char* const* CivilianClasses(const char* levelName, int* count);
// Un ennemi sur six environ arrive avec un civil. Le tirage ne dépend que du niveau et du nom de
// l'ennemi : les civils sont aux mêmes endroits d'une reprise à l'autre.
bool BringsCivilian(uint32_t levelHash, uint32_t nameHash);
const char* CivilianClassFor(const char* levelName, uint32_t nameHash);

}  // namespace rules
