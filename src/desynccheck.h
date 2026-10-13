// Comparaison des empreintes de l'hôte et du client (sans accès au jeu) : décide quelles parties
// de l'état renvoyer au client, et quand renoncer.
#pragma once

#include <cstdint>

#include "protocol.h"

namespace desync {

// Le client suit l'hôte avec un léger retard : un écart d'une seconde ou deux n'est rien. Il faut
// ce nombre de rapports divergents d'affilée, sans que la partie ait bougé chez l'hôte, pour
// qu'elle soit renvoyée.
constexpr int kStreak = 4;
constexpr int kMaxTries = 3;              // renvois d'une même partie avant de renoncer
constexpr double kResyncInterval = 5.0;   // entre deux renvois
// Des rapports arrivés ensemble ne valent pas plusieurs observations : l'hôte sort de son menu de
// pause et lit d'un coup tout ce que le client lui a envoyé entre-temps. En deçà de cet écart, un
// rapport n'est pas compté.
constexpr double kMinSpacing = 0.5;

struct Verdict {
    uint32_t repair;       // un bit par proto::DigestPart : à renvoyer maintenant
    uint32_t persistent;   // l'écart tient malgré les renvois : à signaler, une fois
};

class Checker {
public:
    void Reset();
    // À chaque rapport du client. ours : nos empreintes au même moment.
    Verdict Compare(const proto::Digest& ours, const proto::Digest& theirs, double now);
    int tries(int part) const { return tries_[part]; }

private:
    proto::Digest previous_ = {};   // nos empreintes au rapport précédent
    bool hasPrevious_ = false;
    int streak_[proto::kDigestParts] = {};
    int tries_[proto::kDigestParts] = {};
    bool reported_[proto::kDigestParts] = {};
    double nextResync_ = 0.0;
    double lastReport_ = -1000.0;   // instant du rapport précédent
};

}  // namespace desync
