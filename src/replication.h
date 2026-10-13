// Application des états reçus : pantins des autres joueurs et PNJ pilotés par l'hôte.
#pragma once

#include "oni.h"
#include "protocol.h"

namespace rep {

// Personnage qui représente ici le joueur de ce numéro, ou nullptr : notre propre numéro, un
// joueur absent, ou qui n'est pas dans notre niveau.
oni::Character* RemotePlayerCharacter(uint32_t player);
// L'ennemi vivant le plus proche de ce point, à range au plus ; nul s'il n'y en a pas. distance
// reçoit ce qui l'en sépare.
oni::Character* NearestHostile(const oni::Vector3& from, float range, float* distance);
// Les ennemis vivants à range au plus de ce point, du plus proche au plus lointain ; rend leur
// nombre, capacity au plus.
int Hostiles(const oni::Vector3& from, float range, oni::Character** out, int capacity);
// Au chargement de la DLL : les pantins des autres joueurs ne sont plus tracés pendant une
// cinématique, ni un PNJ en double le temps qu'il soit retiré.
bool InstallVisibilityHook();
// Dernier état reçu de ce joueur, tant que son personnage est là. Faux sinon.
bool RemotePlayerState(uint32_t player, proto::EntityState& out);

// Au chargement de la DLL : les PNJ traitent le pantin d'un autre joueur en adversaire à part
// entière. Faux si le code du moteur n'est pas celui attendu (rien n'est alors modifié).
bool InstallAiHook();
// Au chargement de la DLL : chaque dégât du moteur est arbitré entre les machines (qui a frappé,
// qui encaisse). Même réserve.
bool InstallDamageFilter();
// Au chargement de la DLL : un personnage piloté par le réseau peut tirer sans que le moteur
// consulte son IA, à l'arrêt. Même réserve.
bool InstallShooterHook();
// Au chargement de la DLL : chez l'hôte, les pantins des clients déclenchent les zones réservées
// au joueur. Même réserve.
bool InstallTriggerHook();
// Au chargement de la DLL : dans le compte des personnages présents dans une zone, que lisent les
// scripts, tous les joueurs ne font qu'un. Même réserve.
bool InstallTriggerCountHook();
// Le pantin d'un autre joueur ne ramasse plus de bonus par lui-même : voir replication.cpp.
bool InstallPickupHook();
// Au chargement de la DLL : chez l'hôte, un PNJ qui voit plusieurs joueurs s'en prend au plus
// proche. Même réserve.
bool InstallTargetHook();
// Au chargement de la DLL : la santé des autres joueurs et celle des PNJ s'affichent au-dessus
// d'eux. Même réserve.
bool InstallOverheadHook();
// Au chargement de la DLL : avec le tir ami, les coups de mêlée et les prises de notre joueur
// atteignent les pantins des autres. Même réserve.
bool InstallMeleeHook();

// Nouveau niveau chargé : tous les pointeurs de personnages sont périmés.
void OnLevelChanged();
// Hôte : ce client est parti, son pantin est retiré. Client : l'hôte est perdu, et avec lui tous
// les autres joueurs ; la main revient à l'IA locale.
void OnDisconnected(uint8_t player);
// La session s'arrête ou change de rôle : plus aucun pantin, la main revient à l'IA locale.
void OnSessionClosed();
// Cesse de piloter les PNJ et rend la main à l'IA locale.
void ReleaseNpcs();

// --- Réception ----------------------------------------------------------------------------------
// État d'un autre joueur, dont le numéro est dans state.characterID.
void OnPlayerState(const proto::EntityState& state, uint32_t levelHash, double now);
void OnNpcSnapshot(const proto::EntityState* states, int count, uint32_t levelHash, double now);
void OnNpcInfo(const proto::NpcInfo* infos, int count);
void OnEvent(uint8_t from, const proto::Event& event, uint32_t levelHash);
// Messages fiables, sans leur octet de type ; from : le joueur qui en est l'auteur.
// kReliableThrow : un pantin d'ici projette un personnage simulé ici.
void OnThrow(uint8_t from, const uint8_t* data, int size, double now);
// kReliableHit et kReliableImpose : coup reçu par notre joueur et jugé par une autre machine (PNJ
// de l'hôte, ou autre joueur avec le tir ami), et réaction qu'il lui impose. Sans effet quand ils
// désignent un autre joueur que le nôtre.
void OnHit(uint8_t from, const uint8_t* data, int size, double now);
void OnImpose(uint8_t from, const uint8_t* data, int size, double now);

// --- Émission -----------------------------------------------------------------------------------
bool CollectLocalPlayer(proto::EntityState& out);
// Hôte : remplit states (et infos si non nul, même indice) avec tous les PNJ. Renvoie le nombre.
int CollectNpcs(proto::EntityState* states, proto::NpcInfo* infos, int capacity);

// --- Chaque frame -------------------------------------------------------------------------------
void Update(double now);

// --- Comparaison de l'hôte et d'un client -------------------------------------------------------
// Empreinte des PNJ vivants ici.
uint32_t NpcDigest();
// Hôte : les noms des PNJ partiront avec le prochain envoi, sans attendre leur tour.
void ResendNames();
bool TakeNamesRequest();   // vrai une fois après ResendNames
// Client : redemande au niveau les PNJ de l'hôte qu'il a refusé de créer.
void RetrySpawns();

int NpcPuppetCount();
int RemotePlayerCount();                              // pantins de joueurs présents ici
void DescribeRemotePlayers(char* out, int capacity);  // pour la trace de débogage

// Vrai pour notre joueur et pour le personnage qui représente ici un autre joueur.
bool IsPlayerCharacter(oni::Character* chr);
// Message proto::kReliableNpcWeapons, octet de genre retiré : l'arme que tient chaque PNJ chez
// l'hôte.
void OnNpcWeapons(uint8_t from, const uint8_t* data, int size);
// Message proto::kReliableDisarm, octet de genre retiré : un joueur vient de prendre son arme à un
// PNJ. Chez l'hôte le PNJ la perd ; chez un autre client, son pantin.
void OnDisarm(uint8_t from, const uint8_t* data, int size, double now);
// Le joueur de ce numéro est à terre ici, et attend d'être ranimé ; where reçoit sa place.
bool DownedPlayerAt(uint32_t player, oni::Vector3& where);

}  // namespace rep
