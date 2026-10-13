// Test de la ligne de temps sans le jeu : un émetteur simulé (repos, course, sprint à pas très
// courts) est échantillonné à 30 Hz avec des arrivées irrégulières, puis rejoué à 144 images par
// seconde. La pose calculée est comparée à la vérité au même instant.
#include <cmath>
#include <cstdio>

#include "../src/timeline.h"

namespace {

const uint32_t kIdle = 0x1001, kRunLeft = 0x2001, kRunRight = 0x2002, kSprintLeft = 0x3001, kSprintRight = 0x3002;
const int kIdleLength = 60, kRunLength = 11, kSprintLength = 7;
// En ticks. Départs impairs : les changements tombent entre deux paquets (envoyés aux ticks pairs).
const int kRunStart = 91, kSprintStart = 245, kEnd = 600;

struct Truth {
    float x;
    uint32_t animID;
    int frame;
    int duration;
};

// État exact de l'émetteur au tick donné.
Truth TruthAt(int tick) {
    Truth truth = {0.0f, kIdle, 0, kIdleLength};
    if (tick < kRunStart) {
        truth.frame = tick % kIdleLength;
        return truth;
    }
    float runDistance = 0.6f * static_cast<float>((tick < kSprintStart ? tick : kSprintStart) - kRunStart);
    if (tick < kSprintStart) {
        int step = (tick - kRunStart) / kRunLength;
        truth = {runDistance, step % 2 ? kRunRight : kRunLeft, (tick - kRunStart) % kRunLength, kRunLength};
        return truth;
    }
    int step = (tick - kSprintStart) / kSprintLength;
    truth = {runDistance + 1.1f * static_cast<float>(tick - kSprintStart), step % 2 ? kSprintRight : kSprintLeft,
             (tick - kSprintStart) % kSprintLength, kSprintLength};
    return truth;
}

int FrameDistance(int a, int b, int duration) {
    int distance = std::abs(a - b);
    return duration - distance < distance ? duration - distance : distance;
}

// Bruit déterministe dans [0, 1[.
double Noise(int seed) {
    unsigned value = static_cast<unsigned>(seed) * 2654435761u;
    return static_cast<double>((value >> 8) & 0xFFFF) / 65536.0;
}

}  // namespace

int main() {
    const double kDelayTicks = 6.0;
    const double kClockSkew = 1234.5;   // les deux horloges n'ont aucune raison de coïncider
    timeline::Buffer buffer;

    int evaluated = 0, wrongAnim = 0, wrongFrame = 0;
    double worstPosition = 0.0;
    int nextSendTick = 0;

    // Temps local, par pas d'une image à 144 Hz.
    for (int image = 0; image < 144 * (kEnd / 60); image++) {
        double now = kClockSkew + image / 144.0;
        double senderTicks = (now - kClockSkew) * 60.0;

        // Paquets partis toutes les 2 ticks, arrivés avec 0 à 12 ms de retard variable.
        while (nextSendTick <= senderTicks - Noise(nextSendTick) * 0.72) {
            Truth truth = TruthAt(nextSendTick);
            proto::EntityState state = {};
            state.x = truth.x;
            state.animID = truth.animID;
            state.animFrame = static_cast<uint16_t>(truth.frame);
            state.flags = proto::kEntityActive;
            state.tick = static_cast<uint16_t>(nextSendTick + 65000);  // force un passage par 65535
            timeline::Push(buffer, state, now);
            nextSendTick += 2;
        }

        timeline::Pose pose;
        double renderTick = timeline::RenderTick(buffer, now, kDelayTicks);
        if (!timeline::Evaluate(buffer, renderTick, pose)) continue;

        double truthTick = renderTick - 65000.0;
        if (truthTick < 4.0 || truthTick > kEnd - 20) continue;  // hors du régime établi
        Truth truth = TruthAt(static_cast<int>(std::floor(truthTick)));
        evaluated++;

        if (pose.animID != truth.animID) {
            wrongAnim++;
        } else if (FrameDistance(static_cast<int>(pose.frame + 0.5) % truth.duration, truth.frame, truth.duration) > 1) {
            wrongFrame++;
        }
        // Position attendue à l'instant exact, entre deux ticks.
        Truth nextTruth = TruthAt(static_cast<int>(std::floor(truthTick)) + 1);
        double fraction = truthTick - std::floor(truthTick);
        double expected = truth.x + (nextTruth.x - truth.x) * fraction;
        double error = std::fabs(pose.position.x - expected);
        if (error > worstPosition) worstPosition = error;
    }

    printf("poses evaluees=%d  animation fausse=%d  frame fausse=%d  pire ecart de position=%.3f\n", evaluated,
           wrongAnim, wrongFrame, worstPosition);
    // Un pas de sprint parcourt 1,1 unité par tick : l'écart toléré vaut moins d'un tiers de tick.
    bool ok = evaluated > 1000 && wrongAnim == 0 && wrongFrame == 0 && worstPosition < 0.35;
    printf("%s\n", ok ? "OK" : "ECHEC");
    return ok ? 0 : 1;
}
