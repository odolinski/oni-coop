// Test de la trace de l'hôte sans le jeu : un joueur suivi contourne un mur en « U », et un suiveur
// qui marche de point en point doit le rejoindre sans jamais traverser le mur. Vérifie aussi les
// boucles sautées, la trace pleine, le joueur déplacé d'un coup et les nombres qui n'en sont pas.
#include <cmath>
#include <cstdio>

#include "../src/trail.h"

using oni::Vector3;

namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
    printf("%s : %s\n", ok ? "ok   " : "ECHEC", what);
    if (!ok) g_failures++;
}

float Flat(const Vector3& a, const Vector3& b) { return std::sqrt((b.x - a.x) * (b.x - a.x) + (b.z - a.z) * (b.z - a.z)); }

// Le mur : le segment x = 50, z de -100 à 60. Vrai si le pas de a à b le traverse.
bool CrossesWall(const Vector3& a, const Vector3& b) {
    if ((a.x - 50.0f) * (b.x - 50.0f) >= 0.0f) return false;
    float t = (50.0f - a.x) / (b.x - a.x);
    float z = a.z + (b.z - a.z) * t;
    return z > -100.0f && z < 60.0f;
}

// Avance de step vers le but.
Vector3 Toward(const Vector3& from, const Vector3& to, float step) {
    float distance = Flat(from, to);
    if (distance <= step) return to;
    float k = step / distance;
    return {from.x + (to.x - from.x) * k, from.y + (to.y - from.y) * k, from.z + (to.z - from.z) * k};
}

}  // namespace

int main() {
    // --- Contourner un mur ---------------------------------------------------------------------
    // Le joueur suivi part de (0, 0, 0), monte en z jusqu'à 80, passe le bout du mur, redescend de
    // l'autre côté jusqu'à (100, 0, 0) : à vol d'oiseau, cent pas à travers le mur.
    const Vector3 route[] = {{0, 0, 0}, {0, 0, 80}, {100, 0, 80}, {100, 0, 0}};
    trail::Trail trail;
    Vector3 leader = route[0];
    for (int leg = 1; leg < 4; leg++) {
        while (Flat(leader, route[leg]) > 0.01f) {
            leader = Toward(leader, route[leg], 1.5f);
            trail::Record(trail, leader);
        }
    }
    Check(trail.count >= 20 && trail.count <= 30, "contour : un point tous les dix pas (26 pour 260 pas)");

    Vector3 follower = {0, 0, 0};
    bool crossed = false;
    int ticks = 0;
    for (; ticks < 2000 && trail.count > 0; ticks++) {
        trail::Reach(trail, follower);
        if (trail.count == 0) break;
        Vector3 next = Toward(follower, trail::Next(trail), 1.0f);
        crossed = crossed || CrossesWall(follower, next);
        follower = next;
    }
    Check(!crossed, "contour : le suiveur ne traverse jamais le mur");
    Check(trail.count == 0 && Flat(follower, leader) < 2.0f * trail::kReach, "contour : il arrive pres du joueur suivi");
    Check(ticks > 200 && ticks < 320, "contour : en a peu pres la longueur du chemin");

    // --- Boucle sautée -------------------------------------------------------------------------
    // Le joueur suivi fait un grand tour et repasse devant le suiveur : celui-ci ne refait pas le tour.
    trail::Clear(trail);
    for (int i = 0; i <= 360; i += 3) {
        float a = static_cast<float>(i) * 3.14159265f / 180.0f;
        trail::Record(trail, {100.0f - 100.0f * std::cos(a), 0, 100.0f * std::sin(a)});
    }
    for (int i = 0; i < 5; i++) trail::Record(trail, {-12.0f * static_cast<float>(i + 1), 0, 0});
    int before = trail.count;
    int dropped = trail::Reach(trail, {0, 0, 2});
    Check(before > 60 && dropped >= before - 6, "boucle : repasser pres du suiveur retire tout le tour");
    Check(trail.count > 0 && trail::Next(trail).x < 0.0f, "boucle : le prochain point est la suite, pas le tour");

    // --- Étages --------------------------------------------------------------------------------
    trail::Clear(trail);
    trail::Record(trail, {0, 0, 0});
    trail::Record(trail, {0, 30, 0});   // l'étage du dessus, à la verticale
    Check(trail.count == 2, "etages : monter sans avancer ajoute un point");
    trail::Reach(trail, {1, 0, 1});
    Check(trail.count == 1 && trail::Next(trail).y == 30.0f, "etages : le point du dessus n'est pas tenu pour atteint d'en bas");

    // --- Trace pleine, joueur déplacé, nombres invalides -----------------------------------------
    trail::Clear(trail);
    for (int i = 0; i < 300; i++) trail::Record(trail, {static_cast<float>(i) * 12.0f, 0, 0});
    Check(trail.count == trail::kCapacity, "pleine : elle garde ses 96 derniers points");
    Check(trail::Next(trail).x == static_cast<float>(300 - trail::kCapacity) * 12.0f, "pleine : le plus ancien est parti");
    trail::Record(trail, {9000.0f, 0, 9000.0f});
    Check(trail.count == 1, "deplace d'un coup : la trace repart de la");
    float nan = std::nanf("");
    Check(!trail::Record(trail, {nan, 0, 0}) && trail.count == 1, "nombre invalide : jamais note");
    Check(trail::Reach(trail, {nan, nan, nan}) == 0 && trail.count == 1, "nombre invalide : n'atteint rien");
    trail::Skip(trail);
    Check(trail.count == 0, "renoncer au dernier point vide la trace");

    printf("%s\n", g_failures ? "ECHEC" : "tout passe");
    return g_failures ? 1 : 0;
}
