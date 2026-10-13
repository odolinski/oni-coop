#include "groundmatch.h"

namespace ground {

int SlotOf(const Ids& ids, uint16_t id) {
    if (!id) return -1;
    for (int slot = 0; slot < kSlots; slot++) {
        if (ids.of[slot] == id) return slot;
    }
    return -1;
}

void Bind(Ids& ids, int slot, uint16_t id) {
    if (slot < 0 || slot >= kSlots) return;
    int other = SlotOf(ids, id);
    if (other >= 0) ids.of[other] = 0;
    ids.of[slot] = id;
}

uint16_t Allocate(Ids& ids, int slot, uint8_t player) {
    if (slot < 0 || slot >= kSlots) return 0;
    if (ids.of[slot]) return ids.of[slot];
    for (int tries = 0; tries < kIdCounterMask; tries++) {
        ids.counter = static_cast<uint16_t>(ids.counter % kIdCounterMask + 1);  // de 1 à 4095
        uint16_t id = static_cast<uint16_t>((player << kIdPlayerShift) | ids.counter);
        if (SlotOf(ids, id) >= 0) continue;  // le compteur a fait le tour : numéro encore porté
        ids.of[slot] = id;
        return id;
    }
    return 0;
}

static float DistanceSquared(const Weapon& weapon, float x, float y, float z) {
    float dx = weapon.x - x, dy = weapon.y - y, dz = weapon.z - z;
    return dx * dx + dy * dy + dz * dz;
}

// L'arme au sol de cette classe la plus proche, parmi celles que le filtre laisse passer.
template <typename Filter>
static int Nearest(const Weapon* weapons, uint32_t classHash, float x, float y, float z, float radius, Filter allowed) {
    int best = -1;
    float bestDistance = radius * radius;
    for (int slot = 0; slot < kSlots; slot++) {
        const Weapon& weapon = weapons[slot];
        if (weapon.state != kOnGround || weapon.classHash != classHash || !allowed(slot)) continue;
        float distance = DistanceSquared(weapon, x, y, z);
        if (distance > bestDistance) continue;
        best = slot;
        bestDistance = distance;
    }
    return best;
}

int FindPicked(Ids& ids, const Weapon* weapons, uint32_t classHash, float x, float y, float z, float radius,
               uint16_t id, uint16_t lastDropped, bool* exact) {
    *exact = false;
    int known = SlotOf(ids, id);
    if (known >= 0) {
        const Weapon& weapon = weapons[known];
        if (weapon.classHash == classHash) {
            if (weapon.state == kOnGround && DistanceSquared(weapon, x, y, z) <= radius * radius) {
                *exact = true;
                return known;
            }
            // Un joueur la tient déjà ici (deux joueurs l'ont prise au même moment, ou le pantin de
            // celui-ci l'a déjà) : aucune autre ne la remplace.
            if (weapon.state == kOurs || weapon.state == kPuppet) return -1;
        }
        ids.of[known] = 0;  // ce n'était pas elle : numéro pris par erreur, ou case réutilisée
    }
    // Sans numéro connu ici, reste l'endroit. Deux armes sont écartées : celle que ce joueur vient
    // de lâcher (reprise, elle aurait été annoncée sous son numéro), et, s'il donne un numéro,
    // toutes celles qui en portent un autre.
    int dropped = SlotOf(ids, lastDropped);
    int best = Nearest(weapons, classHash, x, y, z, radius,
                       [&](int slot) { return slot != dropped && (!id || !ids.of[slot]); });
    if (best >= 0 && id) Bind(ids, best, id);
    return best;
}

void Match(Ids& ids, const Weapon* weapons, const proto::GroundWeapon* host, int count, float radius, int* slots) {
    if (count > kSlots) count = kSlots;
    bool taken[kSlots] = {};    // armes d'ici déjà rapprochées d'une arme de l'hôte
    bool settled[kSlots] = {};  // armes de l'hôte dont le sort est réglé
    // Par numéro d'abord : deux armes de la même classe côte à côte ne sont plus confondues.
    for (int i = 0; i < count; i++) {
        slots[i] = -1;
        int known = SlotOf(ids, host[i].id);
        if (known < 0) continue;
        const Weapon& weapon = weapons[known];
        if (weapon.classHash == host[i].classHash) {
            if (weapon.state == kOnGround && DistanceSquared(weapon, host[i].x, host[i].y, host[i].z) <= radius * radius) {
                slots[i] = known;
                taken[known] = settled[i] = true;
                continue;
            }
            // Notre joueur la tient : la liste date d'avant son ramassage, ou son pantin l'a
            // laissée tomber chez l'hôte. Elle n'est à rapprocher d'aucune autre.
            if (weapon.state == kOurs) {
                settled[i] = true;
                continue;
            }
        }
        ids.of[known] = 0;  // tenue par un autre, trop loin, ou case réutilisée : ce n'était pas elle
    }
    // Les autres font connaissance : la plus proche des armes d'ici encore sans numéro.
    for (int i = 0; i < count; i++) {
        if (settled[i]) continue;
        int best = Nearest(weapons, host[i].classHash, host[i].x, host[i].y, host[i].z, radius,
                           [&](int slot) { return !taken[slot] && !ids.of[slot]; });
        if (best < 0) continue;
        taken[best] = true;
        slots[i] = best;
        if (host[i].id) Bind(ids, best, host[i].id);
    }
}

}  // namespace ground
