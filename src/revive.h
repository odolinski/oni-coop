// Joueur à terre et réanimation. Tant qu'un coéquipier est debout, un joueur qui tombe à zéro ne
// meurt pas : il reste à terre, et un compte à rebours commence, dont la durée dépend du mode de
// difficulté. Un coéquipier le ranime en gardant CTRL enfoncé près de lui pendant trois secondes ;
// le compte à rebours s'arrête pendant ce temps, et reprend où il en était si la réanimation est
// interrompue. À zéro, le joueur meurt et la mission échoue pour tous.
//
// Chaque machine décide pour son propre joueur : c'est elle qui le met à terre, tient son compte à
// rebours et le relève. Celle du coéquipier juge de la réanimation (distance, touche, coup reçu) et
// l'annonce.
#pragma once

#include <cstdint>

#include "protocol.h"

namespace revive {

// Nouveau niveau chargé : plus personne n'est à terre.
void OnLevelChanged();
// Une fois par frame, après l'application des états distants.
void Update(double now);
// Message proto::kReliableRevive, octet de genre retiré ; from : son auteur.
void OnReliable(uint8_t from, const uint8_t* data, int size, double now);

bool Downed();     // notre joueur est à terre
// Notre joueur est à terre, ou vient d'être relevé : rien ne le blesse.
bool Shielded();
// Notre joueur vient d'encaisser le coup d'un ennemi : s'il ranimait quelqu'un, c'est interrompu.
void OnLocalHit();
// État de notre joueur tel qu'il part vers les autres : à terre, il porte son compte à rebours à
// la place de sa santé.
void Stamp(proto::EntityState& state);

// Ce qui se trace au-dessus d'un joueur à terre, le nôtre compris.
struct Countdown {
    float seconds;     // temps qu'il lui reste
    float remaining;   // part de ce temps, de 0 à 1
    bool reviving;     // quelqu'un le ranime : le temps est arrêté
    bool ours;         // c'est nous qui le ranimons
    // Où en est la réanimation, de 0 à 1 ; négatif : personne ne le ranime. Connu de tous depuis la
    // 0.34 : de celui qui ranime, du joueur à terre, qui voit ainsi la barre se remplir comme lui,
    // et des autres joueurs.
    float progress;
    bool canRevive;    // à portée de main, et rien ne nous retient : CTRL le ranimerait
    float pulse;       // battement du repère, de 0 à 1, au rythme d'un cœur
};
// Faux si le joueur de ce numéro n'est pas à terre.
bool GetCountdown(uint32_t player, Countdown& out);

void Describe(char* out, int capacity);

// Notre joueur est à terre et un coup l'atteint au sol : il le montre par l'animation « touché à
// terre » qu'a choisie la machine de l'attaquant, sans se relever ni rien perdre ; le compte à
// rebours et la réanimation en cours n'en savent rien. Faux, sans rien jouer, si l'animation n'est
// pas de celles qui restent au sol.
bool OnGroundReaction(void* tram, double now);

}  // namespace revive
