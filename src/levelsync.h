// Reprise commune à la dernière sauvegarde. Le niveau de l'hôte fait référence : chaque fois
// qu'il le (re)charge, chaque client recharge le sien au même point de sauvegarde, avec l'état que
// l'hôte y avait enregistré. La mort du joueur d'un client demande cette reprise à l'hôte, et
// l'arrivée d'un joueur en cours de niveau aussi, après un compte à rebours.
//
// Après chaque chargement le jeu reste en pause jusqu'à ce que tous les joueurs aient chargé : la
// partie démarre chez tous en même temps, sans écran d'accueil ni touche à presser.
#pragma once

#include <cstdint>

namespace level {

// Au chargement de la DLL : pas d'écran d'échec avant une reprise quand d'autres joueurs sont là.
// Faux si le code du moteur n'est pas celui attendu (rien n'est alors modifié).
bool InstallSplashHook();
// De même : en session, ni l'écran d'accueil montré à la fin de chaque chargement de niveau, ni
// celui d'une fin de niveau gagnée, qui attendent tous deux une touche. À poser après le précédent.
bool InstallIntroSplashHook();

// Notre niveau vient d'être chargé ou rechargé.
void OnLevelLoaded();
void OnConnection(uint8_t player, bool connected);

// Messages fiables kReliableLevelInstance, kReliableRestartRequest et kReliableAlert, octet de
// genre retiré.
void OnInstance(uint8_t from, const uint8_t* data, int size);
void OnRestartRequest(uint8_t from, const uint8_t* data, int size, double now);
void OnAlert(uint8_t from, const uint8_t* data, int size);
// kReliableJoin (un client arrive), kReliableReady (il a chargé) et kReliableGo (départ).
void OnJoin(uint8_t from, const uint8_t* data, int size, double now);
void OnReady(uint8_t from, const uint8_t* data, int size);
void OnGo(uint8_t from, const uint8_t* data, int size);
// Points de sauvegarde : ceux de l'hôte valent pour tous. Hôte : sa commande de script save_game
// vient de s'exécuter, il le dit aux clients. Client relié : la sienne a été sautée (savePoint,
// négatif si inconnu) ; puis kReliableSavePoint, l'annonce de l'hôte.
void OnHostSaved();
void OnLocalSaveSkipped(int32_t savePoint);
void OnSavePoint(uint8_t from, const uint8_t* data, int size);
// Le jeu est en pause en attendant que tous aient chargé.
bool Waiting();
// À la fin de chaque image de la boucle du jeu : en session, l'écran d'accueil du niveau, qui fige
// tout jusqu'à l'appui d'une touche, n'est pas montré.
void DropSplash();

// La mission échoue pour tous : un joueur à terre n'a pas été ranimé à temps, ou un joueur a tué
// un civil (proto::RestartReason). player : le joueur en cause. L'hôte le dit à tous et reprend à
// sa dernière sauvegarde ; un client le lui demande.
void Fail(uint8_t reason, uint8_t player, double now);

// Une fois par frame.
void Update(double now);

// Chargement de niveau en cours chez l'hôte : son numéro chez lui, celui que nous suivons chez le
// client. Sert à comparer l'hôte et un client.
uint32_t Instance();
// Hôte : redit à ce client où il en est.
void Resend(uint8_t player);

void Describe(char* out, int capacity);

}  // namespace level
