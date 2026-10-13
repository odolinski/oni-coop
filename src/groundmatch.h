// Identité des armes d'une machine à l'autre, et rapprochement des armes au sol (sans accès au
// jeu : vérifié par onimp_groundtest).
//
// Chaque machine a sa table d'armes, et la même arme n'y occupe pas la même case. Jusqu'à la 0.22
// une arme était désignée par sa classe et l'endroit où elle se trouvait, ce qui ne distingue pas
// deux armes de la même classe au même endroit. Or c'est le cas à chaque échange : le joueur lâche
// la sienne sur celle qu'il ramasse. Sa copie sur l'autre machine reprenait parfois celle qu'elle
// venait de lâcher ; le sol de l'hôte, qui fait référence, gardait alors la mauvaise des deux, et
// imposait ses cartouches à l'arme lâchée par le client.
//
// Chaque arme porte donc un numéro, le même sur toutes les machines. Il est donné par la première
// qui a besoin d'en parler (l'hôte quand il liste son sol, un joueur quand il lâche une arme qui
// n'en a pas) et voyage avec chaque annonce. L'endroit ne sert plus qu'à faire connaissance : une
// arme encore sans numéro ici prend celui de l'arme de l'hôte la plus proche.
#pragma once

#include <cstdint>

#include "protocol.h"

namespace ground {

constexpr int kSlots = 64;  // cases de la table des armes du moteur

// Un numéro : le joueur qui l'a donné, puis son compteur. Deux machines ne donnent jamais le même.
// 0 : arme sans numéro.
constexpr int kIdPlayerShift = 12;
constexpr uint16_t kIdCounterMask = 0x0FFF;

enum State : uint8_t {
    kFree,      // case inoccupée
    kOnGround,  // au sol
    kOurs,      // tenue ou rangée par notre joueur
    kPuppet,    // par le pantin d'un autre joueur
    kNpc,       // par un PNJ
};

// Ce qu'il faut savoir d'une case de la table des armes d'ici.
struct Weapon {
    State state = kFree;
    uint32_t classHash = 0;  // FNV-1a du nom de sa classe
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Ids {
    uint16_t of[kSlots] = {};  // numéro de l'arme de chaque case
    uint16_t counter = 0;      // dernier numéro donné par cette machine
};

// La case de l'arme qui porte ce numéro, -1 si aucune.
int SlotOf(const Ids& ids, uint16_t id);
// L'arme de cette case porte désormais ce numéro, et elle seule.
void Bind(Ids& ids, int slot, uint16_t id);
// Le numéro de l'arme de cette case ; elle en reçoit un de ce joueur si elle n'en a pas.
uint16_t Allocate(Ids& ids, int slot, uint8_t player);

// L'arme d'ici qu'un autre joueur vient de ramasser à cet endroit, -1 si elle n'y est pas. id : le
// numéro qu'il lui connaît, 0 s'il ne lui en connaît pas. lastDropped : celui de la dernière arme
// qu'il a lâchée. exact : trouvée par son numéro, et non parce qu'elle était la plus proche.
int FindPicked(Ids& ids, const Weapon* weapons, uint32_t classHash, float x, float y, float z, float radius,
               uint16_t id, uint16_t lastDropped, bool* exact);

// Client : pour chacune des armes au sol chez l'hôte, la case de la même arme ici (slots[i], -1 si
// elle n'est pas au sol ici). Les armes qui font connaissance prennent le numéro de l'hôte.
void Match(Ids& ids, const Weapon* weapons, const proto::GroundWeapon* host, int count, float radius, int* slots);

}  // namespace ground
