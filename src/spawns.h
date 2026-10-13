// Personnages que le mod ajoute au niveau : les renforts du multiplicateur d'ennemis, et les civils
// qui se mêlent à eux.
//
// Renforts : chaque ennemi ordinaire du niveau reçoit autant de copies que le multiplicateur en
// demande, créées à ses côtés d'après son propre objet de niveau (même classe, même arme, même
// rôle), sans aucun script. Ne sont jamais multipliés : les boss, les personnages increvables,
// passifs ou qui ne se battent pas, ceux qui ne sont pas hostiles aux joueurs, et ceux auxquels un
// script du niveau est attaché, parce que le niveau attend quelque chose d'eux. Rien n'apparaît
// pendant une cinématique, et le renfort d'un personnage que le niveau retire est retiré avec lui.
//
// Civils : dans les niveaux où ils ont leur place, un ennemi sur six arrive avec un civil. Si un
// joueur en tue un, la mission échoue.
//
// Tout cela se décide sur la machine qui simule les PNJ : l'hôte, ou chacun pour soi sans PNJ
// partagés. Avec des PNJ partagés, un client les crée quand l'hôte les lui annonce (voir Create).
#pragma once

#include <cstdint>

#include "oni.h"

namespace spawn {

void OnLevelChanged();
// Une fois par frame, après les PNJ.
void Update(double now);

// Client, PNJ partagés : l'hôte a un PNJ de ce nom, absent ici. Vrai si c'est l'un des nôtres,
// renfort ou civil : il est alors créé à cet endroit, si c'est possible. Faux : au niveau de le
// faire apparaître.
bool Create(const char* name, const oni::Vector3& position, float facing);

// Une alarme vient d'être donnée (hôte, ou machine qui simule ses propres PNJ) : des ennemis du
// niveau arrivent en renfort, trois par joueur présent moins ceux d'une vague précédente encore en
// vie. cause : le personnage qui l'a causée ou donnée, nullptr s'il n'est pas connu ; why : pour le
// journal. Rend le nombre d'ennemis créés.
int Wave(oni::Character* cause, const char* why);

bool IsCivilian(oni::Character* chr);
// Un joueur vient de blesser ce personnage. player : son numéro.
void OnPlayerDamage(oni::Character* victim, uint8_t player);

void Describe(char* out, int capacity);

}  // namespace spawn
