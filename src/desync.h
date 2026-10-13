// Détection des désynchronisations. Une fois par seconde chaque client envoie à l'hôte une
// empreinte de chaque partie de son état qui doit être la même que chez lui (PNJ vivants, consoles,
// bonus au sol, réglages, chargement de niveau, scripts). L'hôte les compare aux siennes. Une
// partie qui diffère plusieurs secondes de suite alors que rien n'y bouge chez l'hôte est renvoyée
// de force à ce client.
#pragma once

#include <cstdint>

namespace desync {

void OnConnection(uint8_t player, bool connected);

// Messages fiables kReliableDigest (reçu par l'hôte) et kReliableResync (reçu par un client),
// octet de genre retiré. from : le joueur qui en est l'auteur.
void OnDigest(uint8_t from, const uint8_t* data, int size, double now);
void OnResync(uint8_t from, const uint8_t* data, int size);

// Une fois par frame.
void Update(double now);

void Describe(char* out, int capacity);

}  // namespace desync
