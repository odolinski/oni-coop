// Synchronisation des « quêtes » : les scripts de niveau ne produisent d'effets que chez l'hôte,
// qui transmet aux clients chaque commande exécutée (objectifs, portes, spawns, dialogues...).
#pragma once

#include <cstdint>

namespace script {

// À appeler depuis DllMain, avant que le moteur n'enregistre ses commandes de script.
bool Install();

// Hôte : rejoue au client qui arrive l'état accumulé depuis le début du niveau.
void OnConnection(uint8_t player, bool connected);

// Client : commande reçue de l'hôte par le flux fiable (octet de genre déjà retiré).
void OnReliable(uint8_t from, const uint8_t* payload, int size);

// Hôte : fait ouvrir un écran de texte du niveau (commande text_console) chez le client player, ou
// chez tous si player est négatif. Cette commande-là n'est plus transmise d'office : c'est
// textconsole.cpp qui dit qui doit la voir.
void SendScreen(const char* name, int player);

// Client : exécute les commandes en attente pour le niveau courant. Une fois par frame.
void Update(double now);

// Commandes passées sur une connexion en cours ; 0 sans l'option quests. Hôte : envoyées au
// client player. Client : reçues de l'hôte, player valant 0. Les deux nombres doivent se rejoindre.
uint32_t StreamCount(uint8_t player);

void Describe(char* out, int capacity);  // pour mp_status et la trace
void DumpCommands();                     // liste des commandes connues, dans le journal

}  // namespace script
