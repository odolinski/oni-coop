// Interactions « neutres » avec les joueurs des clients : un civil ou un allié du niveau à qui l'on
// parle avec la touche d'action, et qui donne quelque chose (hypo, chargeurs, bouclier, arme, objet
// du niveau). Le moteur ne les connaît qu'entre un PNJ et le joueur de la partie. Avec des PNJ
// pilotés par l'hôte, ceux d'un client sont des pantins sans IA : sa touche d'action n'y trouvait
// personne à qui parler. Elle part donc vers l'hôte, où le PNJ engage la conversation avec le pantin
// de ce client ; ce qu'il lui remet est repris au pantin et envoyé au vrai joueur.
#pragma once

#include <cstdint>

#include "oni.h"

namespace interact {

// Au chargement de la DLL : la touche d'action d'un client devant un PNJ passe par nous. Faux si le
// code du moteur n'est pas celui attendu (rien n'est alors modifié).
bool Install();

void OnLevelChanged();
// Une fois par frame, après les états distants et avant l'arme en main des pantins.
void Update(double now);

// Hôte, à chaque image où le pantin du client player est piloté, avant que son bouclier et son
// invisibilité ne soient récrits d'après sa machine : shieldSet et cloakSet sont ceux posés à
// l'image précédente. Ce que le moteur y a ajouté depuis, comme ce qu'il a mis dans ses réserves,
// est ce qu'un PNJ vient de lui donner.
void WatchGifts(uint8_t player, oni::Character* puppet, uint16_t shieldSet, uint16_t cloakSet);

// Messages fiables kReliableInteract (hôte) et kReliableGift (client), octet de genre retiré.
void OnInteract(uint8_t from, const uint8_t* data, int size);
void OnGift(uint8_t from, const uint8_t* data, int size);

void Describe(char* out, int capacity);  // pour mp_status

}  // namespace interact
