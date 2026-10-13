// Alerte sonore : un son joué une fois, sans boucle, chaque fois qu'un joueur tombe à terre, le
// nôtre ou un autre. Chaque machine le joue pour elle-même quand elle voit un joueur tomber : tout
// le monde l'entend. Le fichier est un MP3 lu par Windows (MCI), hors du moteur du jeu :
// onimp_alert.mp3 à côté d'Oni.exe, ou à défaut OniMP\sounds\AlertFall.mp3. Ne s'entend que dans
// la fenêtre du jeu qui a le clavier : avec plusieurs fenêtres sur une même machine, une seule le
// joue. Le volume, de 0 à 100, est propre à chaque joueur et gardé dans onimp.ini.
#pragma once

#include <cstdint>

namespace alert {

// Une fois par image : le son est préparé dès que la partie tourne, pas à l'instant où il sert.
void Update();
// Ce joueur vient de tomber à terre : le son, une fois.
void PlayerDown(uint32_t player);

int Volume();                  // de 0 à 100 ; à 0, rien n'est joué
// Appliqué tout de suite au son, ramené entre 0 et 100. Save le garde dans onimp.ini.
void SetVolume(int percent);
void Save();
// Le son, une fois, pour juger du volume qu'on vient de régler.
void Preview();

void Describe(char* out, int capacity);

}  // namespace alert
