// Synchronisation des objets du niveau actionnés par les joueurs : portes ouvertes, consoles
// activées, arme tenue en main, armes lâchées ou ramassées, bonus ramassés. Les changements locaux
// sont détectés par comparaison d'état à chaque frame, puis annoncés aux autres joueurs par le flux
// fiable. Avec des PNJ partagés, l'hôte envoie en plus l'état de son sol (bonus, cartouches des
// armes) et les clients s'y conforment.
#pragma once

#include <cstdint>

namespace world {

void OnLevelChanged();
void OnConnection(uint8_t player, bool connected);

// Message du flux fiable, premier octet (proto::ReliableKind) déjà retiré. from : le joueur qui en
// est l'auteur.
void OnReliable(uint8_t kind, uint8_t from, const uint8_t* data, int size, double now);

// Une fois par frame, après l'application des états distants.
void Update(double now);

// Comparaison de l'hôte et d'un client : empreintes des consoles activées et des bonus au sol ;
// hôte : renvoi aux clients des consoles activées, de l'état du sol.
uint32_t ConsoleDigest();
uint32_t GroundDigest();
void ResendConsoles();
// Au chargement de la DLL : toute activation d'une console par un personnage nous est dite au
// moment où elle a lieu. Faux si le code du moteur n'est pas celui attendu : on s'en tient alors au
// relevé des consoles, qui ne voit pas celles que leur script remet à zéro aussitôt.
bool InstallConsoleHook();
// Joueur qui a activé une console depuis l'instant since, ou négatif ; la console n'est comptée
// qu'une fois. Dit à textconsole.cpp pour qui est l'écran de texte que son script ouvre.
int TakeConsoleUser(double since);
void ResendGround();

void Describe(char* out, int capacity);

}  // namespace world
