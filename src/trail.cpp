#include "trail.h"

#include <cmath>
#include <cstring>

using oni::Vector3;

namespace trail {

static bool Finite(const Vector3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

static float Flat(const Vector3& a, const Vector3& b) {
    float dx = b.x - a.x, dz = b.z - a.z;
    return std::sqrt(dx * dx + dz * dz);
}

static void Drop(Trail& trail, int count) {
    if (count <= 0) return;
    if (count >= trail.count) {
        trail.count = 0;
        return;
    }
    memmove(trail.points, trail.points + count, sizeof(Vector3) * static_cast<size_t>(trail.count - count));
    trail.count -= count;
}

void Clear(Trail& trail) { trail.count = 0; }

bool Record(Trail& trail, const Vector3& leader) {
    if (!Finite(leader)) return false;
    if (trail.count > 0) {
        const Vector3& last = trail.points[trail.count - 1];
        float flat = Flat(last, leader), height = std::fabs(leader.y - last.y);
        if (flat < kStep && height < kStepHeight) return false;
        // Déplacé d'un coup (script du niveau, reprise) : entre les deux, aucun chemin à pied.
        if (flat > kJump || height > kJump) trail.count = 0;
    }
    if (trail.count == kCapacity) Drop(trail, 1);
    trail.points[trail.count++] = leader;
    return true;
}

int Reach(Trail& trail, const Vector3& here, float radius) {
    if (!Finite(here)) return 0;
    int reached = 0;
    for (int i = 0; i < trail.count; i++) {
        const Vector3& point = trail.points[i];
        if (Flat(here, point) < radius && std::fabs(point.y - here.y) < kReachHeight) reached = i + 1;
    }
    Drop(trail, reached);
    return reached;
}

const Vector3& Next(const Trail& trail) { return trail.points[0]; }

void Skip(Trail& trail) { Drop(trail, 1); }

}  // namespace trail
