// Trace d'un joueur : les endroits où il est passé, dans l'ordre. Le jeu automatique s'en sert pour
// rejoindre l'hôte quand la ligne droite ne passe pas : un chemin que le niveau permet, puisqu'un
// joueur vient de le faire à pied, portes, couloirs et escaliers compris. Le moteur a bien son propre
// calcul de chemin (les volumes « BNV » du niveau reliés en graphe, puis une grille par salle), mais
// il appartient à l'état d'une IA, et le personnage du jeu automatique est celui d'un joueur.
// Rien ici ne touche au jeu : testé hors jeu par onimp_trailtest.
#pragma once

#include "oni.h"

namespace trail {

constexpr int kCapacity = 96;
constexpr float kStep = 10.0f;         // un point tous les dix pas du joueur suivi
constexpr float kStepHeight = 5.0f;    // ou dès qu'il monte ou descend d'autant
constexpr float kJump = 150.0f;        // d'un point au suivant, pas plus : au-delà il a été déplacé, la trace repart
constexpr float kReach = 9.0f;         // un point est atteint à cette distance, à plat
constexpr float kReachHeight = 14.0f;  // et à cette hauteur près : pas celui de l'étage du dessus
constexpr float kLoose = 25.0f;        // en ligne droite vers le joueur suivi : points tenus pour dépassés
constexpr float kIdle = 30.0f;         // à l'arrêt près de lui : de même
constexpr float kDirectRange = 60.0f;  // plus près que cela et au même étage : la ligne droite d'abord

struct Trail {
    oni::Vector3 points[kCapacity];
    int count = 0;
};

void Clear(Trail& trail);
// Note où est le joueur suivi. Vrai si un point a été ajouté. Pleine, la trace perd son point le
// plus ancien.
bool Record(Trail& trail, const oni::Vector3& leader);
// Retire les points dépassés : tous jusqu'au dernier qui se trouve à radius de here. Passer près
// d'un point plus récent fait ainsi sauter les boucles du joueur suivi. Rend combien sont retirés.
int Reach(Trail& trail, const oni::Vector3& here, float radius = kReach);
// Le prochain point à rejoindre. À n'appeler que si count > 0.
const oni::Vector3& Next(const Trail& trail);
// Renonce au prochain point (il ne s'atteint pas).
void Skip(Trail& trail);

}  // namespace trail
