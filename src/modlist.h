// Mods de l'AE Installer (sans accès au jeu) : liste des paquets installés dans le dossier du jeu,
// comparaison avec celle de l'autre joueur, format réseau.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mods {

// Paquets transmis au plus : le message reste sous proto::kMaxStreamPayload.
constexpr int kMaxListed = 256;

// Numéros de paquet du bloc <mods> d'installed_mods.xml, triés, sans doublon.
std::vector<uint32_t> ParseInstalled(const std::string& xml);

// Lit GameDataFolder/installed_mods.xml, que l'AE Installer réécrit à chaque installation de
// mods. gameDir finit par un séparateur. Faux si le fichier manque.
bool ReadInstalled(const std::string& gameDir, std::vector<uint32_t>& ids);

// Dossier d'un paquet dans AEInstaller/packages : cinq chiffres puis le nom, « 34090Blood ».
bool SplitPackageFolder(const char* folder, uint32_t& id, std::string& name);

// Noms des paquets téléchargés dans AEInstaller/packages, installés ou non.
std::map<uint32_t, std::string> ReadPackageNames(const std::string& gameDir);

// Numéros de a absents de b. Les deux listes sont triées.
std::vector<uint32_t> Missing(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b);

// Même valeur pour deux listes identiques : se compare d'un coup d'œil entre deux journaux.
uint32_t Fingerprint(const std::vector<uint32_t>& ids);

// « 34090 Blood, 21501 Brutal AI » ; au-delà de limit paquets (0 : tous), « (+N) ».
std::string Label(const std::vector<uint32_t>& ids, const std::map<uint32_t, std::string>& names, int limit);

// Message proto::kReliableMods, premier octet retiré : u16 nombre, puis autant de u32. Encode
// rend la taille écrite, 0 si la liste est trop longue pour capacity ou pour kMaxListed.
int Encode(const std::vector<uint32_t>& ids, uint8_t* out, int capacity);
bool Decode(const uint8_t* data, int size, std::vector<uint32_t>& ids);

}  // namespace mods
