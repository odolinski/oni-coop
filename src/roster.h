// Ce qui distingue les joueurs à l'écran : leur pseudo, leur tenue de Konoko, leur latence. Chacun
// choisit son pseudo et demande sa tenue ; l'hôte tient la liste de tous, attribue les tenues
// (jamais la même à deux joueurs présents) et la redit aux clients à chaque changement.
#pragma once

#include <cstddef>
#include <cstdint>

#include "oni.h"

namespace roster {

// Tenues : la tenue du chapitre en cours (celle que le niveau donne à Konoko), puis les classes de
// personnage de Konoko présentes dans les données communes à tous les niveaux.
constexpr int kSkinCount = 7;
const char* SkinLabel(int skin);            // nom montré dans le menu
// Classe de personnage à donner au pantin de ce joueur à sa création.
const char* ClassFor(uint8_t player);
// La classe voulue pour ce joueur vient de changer (personnage incarné, voir cheat.h) : les essais
// déjà faits pour l'habiller ne comptent plus.
void Redress(uint8_t player);

// Pseudo de ce joueur tel qu'on l'affiche : le sien, ou à défaut « HOST », « P1 » à « P3 ».
const char* Name(uint8_t player);
// Latence à afficher au-dessus de ce joueur, en millisecondes ; négatif si inconnue. Pour un client,
// au-dessus de l'hôte : son propre aller-retour avec lui ; au-dessus d'un autre client : celui de ce
// client avec l'hôte.
int PingMs(uint8_t player);

// Notre pseudo, tel que saisi (vide : aucun). Le changer l'écrit dans onimp.ini et le dit aux autres.
const char* LocalName();
void SetLocalName(const char* name);
// Bouton « Skin » du menu : la tenue du moment, et le passage à la suivante que personne n'a prise.
void SkinValue(char* out, size_t size);
void NextSkin();
bool SetSkin(int skin);                     // faux si elle est prise, ou inconnue

void OnLevelChanged();
void OnConnection(uint8_t player, bool connected);
// Messages fiables kReliableProfile (hôte) et kReliableRoster (client), octet de genre retiré.
void OnProfile(uint8_t from, const uint8_t* data, int size);
void OnRoster(uint8_t from, const uint8_t* data, int size);
// Une fois par frame, après les états distants : donne à chaque personnage la tenue de son joueur.
void Update(double now);

void Describe(char* out, int capacity);     // pour mp_status

}  // namespace roster
