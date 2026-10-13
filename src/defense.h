// Défenses du niveau : lasers (objets TRIG), tourelles (objets TURR) et alarmes.
//
// Le moteur ne teste ses lasers que contre le joueur de la partie, et tout ce qu'un laser déclenche
// (tourelles allumées, alarme, script) ne se passe que sur la machine où il a été coupé. À
// plusieurs, cela donnait des défenses qui ne réagissaient qu'au joueur de chaque machine, chacune
// pour son compte. Avec des PNJ partagés :
//   - chaque machine teste ses lasers contre son propre joueur, là où elle les affiche, et dit aux
//     autres lequel il vient de couper ; l'hôte teste aussi les siens contre le pantin de chaque
//     client. Le premier qui voit un joueur couper un laser l'annonce, et chaque machine fait
//     alors chez elle ce que ce laser déclenche ;
//   - l'état de chaque laser et de chaque tourelle (allumé, éteint) part de l'hôte vers les
//     clients, qui s'y alignent, comme la position des rayons des lasers mobiles ;
//   - l'hôte choisit la cible de chaque tourelle, et chaque machine fait viser le même joueur à la
//     sienne : sur la machine de ce joueur, ce sont ses tirs qui le blessent ;
//   - le joueur qu'un laser vient de détecter est signalé aux ennemis des environs, qui vont à lui.
// Une alarme (commande de script ai2_tripalarm, ou console dont un ennemi se sert) amène une vague
// d'ennemis du niveau, trois par joueur présent (voir spawn::Wave).
// Sans PNJ partagés chacun a son propre monde, et rien de tout cela ne change.
#pragma once

#include <cstdint>

#include "oni.h"

namespace defense {

// Au chargement de la DLL. Faux si le code du moteur n'est pas celui attendu : les lasers de l'hôte
// ne testent alors que son joueur, et un client n'est détecté que par les siens.
bool InstallLaserHook();
// Même réserve : un laser coupé sur une machine ne déclenche alors rien sur les autres, que les
// tourelles que l'hôte allume.
bool InstallTripHook();
bool InstallAlarmHook();   // même réserve : pas de vague sur une alarme du moteur, les consoles restent
// Message proto::kReliableLaserTrip, premier octet déjà retiré ; from : son auteur, le joueur qui
// vient de couper le laser sur sa machine.
void OnTrip(uint8_t from, const uint8_t* data, int size);
// Message proto::kReliableLaserRays, premier octet déjà retiré : où en sont les rayons de l'hôte.
void OnRays(uint8_t from, const uint8_t* data, int size);
// Même réserve : chaque machine choisit alors seule la cible de ses tourelles. Sinon c'est l'hôte
// qui la choisit, parmi les joueurs debout, et chaque client fait viser le même joueur aux siennes :
// sur la machine de ce joueur, c'est lui que les tirs touchent.
bool InstallTurretHook();
// Même réserve : les tourelles d'un client ne tirent alors jamais. Le moteur interdit le tir aux
// tourelles tant que « toutes les IA » sont passives (ai2_allpassive), ce qu'un client du mod
// impose chez lui en permanence ; elles lisent désormais un drapeau à nous, qui chez un client est
// celui de l'hôte.
bool InstallFireHook();
// Message proto::kReliableTurretTargets, premier octet déjà retiré ; from : son auteur.
void OnTargets(uint8_t from, const uint8_t* data, int size);
void OnLevelChanged();
void OnConnection(uint8_t player, bool connected);
void Update(double now);
// Message proto::kReliableDefenses, premier octet déjà retiré ; from : son auteur.
void OnReliable(uint8_t from, const uint8_t* data, int size);
// Un PNJ vient de se servir d'une console : c'est une alarme qu'il donne (ai2_doalarm).
void OnConsoleAlarm(uint16_t console, oni::Character* user);
void Resend();             // hôte : redit l'état des défenses aux clients
void Describe(char* out, int capacity);
void DescribeTurrets(char* out, int capacity);  // seconde ligne : cibles et tirs des tourelles

}  // namespace defense
