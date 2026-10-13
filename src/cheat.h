// Onglets « Cheat », « Character » et « Spawn » du menu F1 : ce qu'un joueur peut changer à son
// propre personnage et faire apparaître devant lui.
//
// Qui y a droit. L'hôte, toujours. Un client, seulement si l'hôte lui a ouvert l'accès : sans cela
// les trois onglets ne lui sont pas montrés. L'hôte tient aussi l'interrupteur général des cheats :
// coupé, plus aucun cheat n'agit chez personne, et vitesses, saut et dégâts sont ceux du jeu. Il ne
// coupe que l'onglet Cheat ; le personnage incarné et les apparitions ne dépendent que de l'accès.
//
// Sur qui cela agit. Les cheats sont ceux de chaque joueur : chacun règle les siens, qui ne touchent
// que son personnage, et les garde dans son onimp.ini. Le personnage incarné est décidé par l'hôte
// à la demande du joueur, et redit à tous, pour que chacun le voie ainsi. Une apparition demandée
// par un client est créée chez l'hôte, d'où elle gagne tout le monde.
#pragma once

#include <cstddef>
#include <cstdint>

#include "oni.h"

namespace cheat {

// Au chargement de la DLL : vitesses et saut passent par la mise à jour des personnages. Faux si le
// code du moteur n'est pas celui attendu ; les autres cheats restent.
bool Install();
void RegisterCommands();
void OnSessionStarted(bool hosting);
void OnConnection(uint8_t player, bool connected);
void OnLevelChanged();
void Update(double now);
// Messages proto::kReliableMorph, kReliableMorphs, kReliableSpawn et kReliableSpawned, premier
// octet déjà retiré ; from : leur auteur.
void OnReliable(uint8_t kind, uint8_t from, const uint8_t* data, int size);
void Describe(char* out, int capacity);

// --- Accès ---------------------------------------------------------------------------------------
bool Master();                         // interrupteur général, celui de l'hôte
void SetMaster(bool enabled);          // hôte seulement
uint8_t AccessMask();                  // un bit par numéro de joueur autorisé ; celui de l'hôte l'est toujours
bool Allowed(uint8_t player);
void SetAllowed(uint8_t player, bool allowed);  // hôte seulement
bool Access();                         // ce joueur a les trois onglets
bool Active();                         // et ses cheats agissent : accès, interrupteur général, session
// Client : ce que l'hôte vient de dire de l'interrupteur et des accès.
void OnHostSettings(bool master, uint8_t accessMask);

// --- Cheats de ce joueur -------------------------------------------------------------------------
// Le réglage tel qu'il l'a laissé, pour le menu ; ce qui agit vraiment demande aussi Active().
bool God();
void SetGod(bool enabled);
bool Ammo();
void SetAmmo(bool enabled);
bool OneHit();
void SetOneHit(bool enabled);
bool Aura();
void SetAura(bool enabled);
int AttackSpeed();                     // pour cent, de 100 à kMaxSpeed
void SetAttackSpeed(int percent);
int MoveSpeed();
void SetMoveSpeed(int percent);
int Jump();                            // pour cent, de 100 à kMaxJump
void SetJump(int percent);
constexpr int kMaxSpeed = 300;
constexpr int kMaxJump = 400;

// Lus par l'arbitrage des dégâts (replication.cpp).
bool GodMode();                        // notre joueur ne perd pas de vie
bool NoFallDamage();                   // ni en tombant : le saut est réglé au-dessus de 100
// Dégâts d'un coup de notre joueur sur victim : assez pour l'achever si le coup fatal est actif et
// que victim est un ennemi ; sinon ceux du coup.
uint32_t StrikeDamage(oni::Character* player, oni::Character* victim, uint32_t damage);

// --- Personnage incarné --------------------------------------------------------------------------
// Classe (ONCC) que ce joueur incarne à la place de Konoko, "" s'il garde sa tenue.
const char* CharacterOf(uint8_t player);
// Demande à incarner cette classe ; "" : redevenir Konoko. L'hôte tranche et le redit à tous.
void SetCharacter(const char* className);
void NextCharacter();                  // la suivante de la liste filtrée : c'est ce que fait la touche

// --- Apparitions ---------------------------------------------------------------------------------
// Client : un PNJ que l'hôte a fait apparaître, reconnu à son nom, est créé ici au même endroit.
// Faux si ce nom n'est pas celui d'une apparition.
bool CreateSpawned(const char* name, const oni::Vector3& position, float facing);

// --- Menu F1 -------------------------------------------------------------------------------------
// Lignes dont le titre est composé ici (index : laquelle) et ce qu'un clic y fait.
constexpr int kListRows = 8;           // classes ou objets montrés par page
void AccessTitle(int player, char* out, size_t size);     // « P1 PSEUDO : allowed »
void AccessClick(int player);
void CharacterValue(char* out, size_t size);              // classe incarnée, ou « Konoko »
void ResetCharacter();
void CharacterFilterValue(char* out, size_t size);
void NextCharacterFilter();
void KeyValue(char* out, size_t size);
void NextKey();
void CharacterRowTitle(int row, char* out, size_t size);
void CharacterRowClick(int row);
void CharacterPageTitle(int direction, char* out, size_t size);   // -1 : précédente, 1 : suivante
void CharacterPageClick(int direction);
void SpawnKindValue(char* out, size_t size);
void NextSpawnKind();
void SpawnOptionTitle(int unused, char* out, size_t size);        // quantité, munitions ou camp, selon le genre
void SpawnOptionClick(int unused);
void SpawnFilterValue(char* out, size_t size);
void NextSpawnFilter();
void SpawnRowTitle(int row, char* out, size_t size);
void SpawnRowClick(int row);
void SpawnPageTitle(int direction, char* out, size_t size);
void SpawnPageClick(int direction);

}  // namespace cheat
