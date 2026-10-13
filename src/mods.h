// Mods de l'AE Installer : ils changent les données du jeu, que tous les joueurs doivent avoir
// identiques. La liste des paquets installés ici est écrite dans le journal ; à chaque connexion
// l'hôte et le client se l'envoient et la comparent.
#pragma once

#include <cstdint>

namespace mods {

// Au chargement de la DLL : lit la liste dans le dossier du jeu et l'écrit dans le journal.
void Load();
// Hôte : envoie la liste au client qui arrive. Client : à l'hôte.
void OnConnection(uint8_t player, bool connected);
// Message proto::kReliableMods, premier octet déjà retiré : la liste du joueur from.
void OnReliable(uint8_t from, const uint8_t* data, int size);
void Describe(char* out, int capacity);

}  // namespace mods
