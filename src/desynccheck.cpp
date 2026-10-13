#include "desynccheck.h"

#include <cstring>

namespace desync {

void Checker::Reset() { *this = Checker(); }

Verdict Checker::Compare(const proto::Digest& ours, const proto::Digest& theirs, double now) {
    Verdict verdict = {0, 0};
    bool burst = now - lastReport_ < kMinSpacing;
    lastReport_ = now;
    if (burst) return verdict;  // arrivé avec le précédent : rien de neuf à en apprendre
    proto::Digest previous = previous_;
    bool comparable = hasPrevious_ && theirs.levelHash == ours.levelHash && previous.levelHash == ours.levelHash;
    previous_ = ours;
    hasPrevious_ = true;
    if (!comparable) {  // l'autre joueur est dans un autre niveau, ou nous venons d'en changer
        memset(streak_, 0, sizeof(streak_));
        return verdict;
    }

    uint32_t due = 0;
    for (int part = 0; part < proto::kDigestParts; part++) {
        if (theirs.parts[part] == ours.parts[part]) {
            streak_[part] = 0;
            tries_[part] = 0;
            reported_[part] = false;
        } else if (ours.parts[part] != previous.parts[part]) {
            // Cette partie vient de changer ici : le client n'a pas encore pu suivre.
            streak_[part] = 0;
        } else if (++streak_[part] >= kStreak) {
            due |= 1u << part;
        }
    }
    if (!due || now < nextResync_) return verdict;

    for (int part = 0; part < proto::kDigestParts; part++) {
        if (!(due & (1u << part))) continue;
        streak_[part] = 0;
        // Scripts : le flux fiable ne perd rien par construction. Un écart y signale un défaut du
        // mod, qu'un renvoi ne corrigerait pas ; il est seulement signalé.
        if (part == proto::kDigestScripts) tries_[part] = kMaxTries;
        if (tries_[part] >= kMaxTries) {
            if (!reported_[part]) verdict.persistent |= 1u << part;
            reported_[part] = true;
            continue;
        }
        tries_[part]++;
        verdict.repair |= 1u << part;
    }
    if (verdict.repair) nextResync_ = now + kResyncInterval;
    return verdict;
}

}  // namespace desync
