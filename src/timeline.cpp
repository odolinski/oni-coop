#include "timeline.h"

#include <cmath>
#include <cstring>

using oni::Vector3;

namespace timeline {

static const double kMaxExtrapolationTicks = 9.0;
static const double kClockResetTicks = 30.0;  // écart d'horloge au-delà duquel on se recale net
static const float kSnapDistance = 40.0f;     // au-delà : téléportation, pas de glissement
static const int kMaxInterpFrames = 12;

static Vector3 PositionOf(const Sample& sample) { return {sample.state.x, sample.state.y, sample.state.z}; }

static float DistanceSquared(const Vector3& a, const Vector3& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

static float LerpAngle(float from, float to, float alpha) {
    const float kTwoPi = 6.28318530718f;
    float delta = std::fmod(to - from, kTwoPi);
    if (delta > kTwoPi * 0.5f) delta -= kTwoPi;
    if (delta < -kTwoPi * 0.5f) delta += kTwoPi;
    float result = std::fmod(from + delta * alpha, kTwoPi);
    return result < 0.0f ? result + kTwoPi : result;
}

static bool HasAnimation(const proto::EntityState& state) {
    return (state.flags & proto::kEntityActive) != 0 && state.animID != 0;
}

static int InterpFramesOf(const proto::EntityState& state) {
    int frames = (state.flags >> proto::kEntityInterpShift) & proto::kEntityInterpMask;
    return frames > kMaxInterpFrames ? kMaxInterpFrames : frames;
}

void Push(Buffer& buffer, const proto::EntityState& state, double now) {
    int64_t tick = state.tick;
    if (buffer.count > 0) {
        Sample& last = buffer.samples[buffer.count - 1];
        int16_t delta = static_cast<int16_t>(state.tick - static_cast<uint16_t>(last.tick));
        if (delta == 0) {
            last.state = state;  // jeu en pause chez l'émetteur : son horloge n'avance pas
            return;
        }
        if (delta < 0 && delta > -120) return;  // paquet en retard
        if (delta < 0 || delta > 600) {
            buffer.count = 0;  // horloge repartie de zéro (niveau rechargé) ou longue coupure
            buffer.haveOffset = false;
        } else {
            tick = last.tick + delta;
        }
    }

    if (buffer.count == kCapacity) {
        memmove(buffer.samples, buffer.samples + 1, sizeof(Sample) * (kCapacity - 1));
        buffer.count--;
    }
    buffer.samples[buffer.count++] = {tick, state};

    // Les instants d'arrivée sont irréguliers : l'écart d'horloge est lissé.
    double offset = now * kTicksPerSecond - static_cast<double>(tick);
    if (!buffer.haveOffset || std::fabs(offset - buffer.tickOffset) > kClockResetTicks) {
        buffer.tickOffset = offset;
        buffer.haveOffset = true;
    } else {
        buffer.tickOffset += (offset - buffer.tickOffset) * 0.05;
    }
}

double RenderTick(const Buffer& buffer, double now, double delayTicks) {
    return now * kTicksPerSecond - buffer.tickOffset - delayTicks;
}

bool Evaluate(const Buffer& buffer, double renderTick, Pose& out) {
    if (buffer.count == 0) return false;

    // a : dernier état antérieur à l'instant affiché ; b : le suivant.
    int next = 0;
    while (next < buffer.count && static_cast<double>(buffer.samples[next].tick) <= renderTick) next++;
    const Sample* a = next > 0 ? &buffer.samples[next - 1] : nullptr;
    const Sample* b = next < buffer.count ? &buffer.samples[next] : nullptr;

    double span = a && b ? static_cast<double>(b->tick - a->tick) : 0.0;
    double u = span > 0.0 ? (renderTick - static_cast<double>(a->tick)) / span : 0.0;

    // --- Position et orientation ---
    if (!a) {
        out.position = PositionOf(*b);
        out.facing = b->state.rotation;
    } else if (!b) {
        // Rien de plus récent (paquet perdu) : on prolonge brièvement le dernier déplacement.
        out.position = PositionOf(*a);
        out.facing = a->state.rotation;
        if (next >= 2) {
            const Sample& previous = buffer.samples[next - 2];
            double previousSpan = static_cast<double>(a->tick - previous.tick);
            double ahead = renderTick - static_cast<double>(a->tick);
            if (ahead > kMaxExtrapolationTicks) ahead = kMaxExtrapolationTicks;
            Vector3 from = PositionOf(previous);
            if (previousSpan > 0.0 && DistanceSquared(from, out.position) < kSnapDistance * kSnapDistance) {
                float k = static_cast<float>(ahead / previousSpan);
                out.position.x += (out.position.x - from.x) * k;
                out.position.y += (out.position.y - from.y) * k;
                out.position.z += (out.position.z - from.z) * k;
            }
        }
    } else {
        Vector3 from = PositionOf(*a);
        Vector3 to = PositionOf(*b);
        float t = DistanceSquared(from, to) > kSnapDistance * kSnapDistance ? 0.0f : static_cast<float>(u);
        out.position = {from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t, from.z + (to.z - from.z) * t};
        out.facing = LerpAngle(a->state.rotation, b->state.rotation, t);
    }

    const Sample* base = a ? a : b;

    // --- Tir et visée ---
    out.flags = base->state.flags;
    out.super = base->state.super;
    out.shield = base->state.shield;
    out.cloak = base->state.cloak;
    out.hasAim = base->state.aimYaw != 0 || base->state.aimPitch != 0;
    out.aimYaw = base->state.aimYaw / proto::kAimScale;
    out.aimPitch = base->state.aimPitch / proto::kAimScale;
    if (out.hasAim && a && b && (b->state.aimYaw != 0 || b->state.aimPitch != 0)) {
        float t = static_cast<float>(u);
        out.aimYaw = LerpAngle(out.aimYaw, b->state.aimYaw / proto::kAimScale, t);
        out.aimPitch += (b->state.aimPitch / proto::kAimScale - out.aimPitch) * t;
    }

    // --- Animation ---
    out.hasAnimation = HasAnimation(base->state);
    out.animID = base->state.animID;
    out.frame = base->state.animFrame + (a ? renderTick - static_cast<double>(a->tick) : 0.0);
    out.interpFrames = 0;
    if (!out.hasAnimation) return true;

    if (a && b && HasAnimation(b->state)) {
        bool changed = b->state.animID != a->state.animID;
        bool restarted = !changed && b->state.animFrame < a->state.animFrame;
        if (changed || restarted) {
            // L'animation de b a démarré entre a et b : sa frame donne le tick exact du départ.
            double start = static_cast<double>(b->tick) - b->state.animFrame;
            if (start < static_cast<double>(a->tick)) start = static_cast<double>(a->tick);
            if (renderTick >= start) {
                out.animID = b->state.animID;
                out.frame = b->state.animFrame - (static_cast<double>(b->tick) - renderTick);
                out.interpFrames = InterpFramesOf(b->state);
            }
        } else {
            // Même animation : interpoler la frame suit aussi ses pauses (impacts, étourdissements).
            out.frame = a->state.animFrame + (b->state.animFrame - a->state.animFrame) * u;
        }
    }
    return true;
}

}  // namespace timeline
