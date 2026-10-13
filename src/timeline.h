// Ligne de temps d'une entité distante : range les états reçus et calcule la pose à afficher à
// un instant donné. Aucun accès au jeu : testable seul (tools/timelinetest.cpp).
#pragma once

#include <cstdint>

#include "oni.h"
#include "protocol.h"

namespace timeline {

constexpr double kTicksPerSecond = 60.0;  // horloge du moteur : animations et physique
constexpr int kCapacity = 16;

struct Sample {
    int64_t tick;               // GameTime de l'émetteur, déroulé sur 64 bits
    proto::EntityState state;
};

struct Buffer {
    Sample samples[kCapacity];  // du plus ancien au plus récent
    int count = 0;
    double tickOffset = 0.0;    // horloge locale (en ticks) moins horloge de l'émetteur
    bool haveOffset = false;
};

struct Pose {
    oni::Vector3 position;
    float facing;
    bool hasAnimation;   // faux : entité inactive chez l'émetteur
    uint32_t animID;
    double frame;        // peut dépasser la durée de l'animation si aucun état plus récent
    int interpFrames;    // fondu à appliquer si cette animation doit être posée
    uint16_t flags;      // drapeaux de l'état affiché (tir, mort...)
    uint8_t super;       // aura, bouclier et invisibilité de l'état affiché (voir proto::EntityState)
    uint8_t shield;
    uint8_t cloak;
    bool hasAim;         // faux : l'émetteur n'a pas transmis de visée
    float aimYaw;        // radians
    float aimPitch;
};

// Ajoute un état reçu à l'instant local now (secondes).
void Push(Buffer& buffer, const proto::EntityState& state, double now);

// Instant de l'émetteur à afficher maintenant, compte tenu du retard d'affichage voulu.
double RenderTick(const Buffer& buffer, double now, double delayTicks);

// Faux si le tampon est vide.
bool Evaluate(const Buffer& buffer, double renderTick, Pose& out);

}  // namespace timeline
