// Test du numéro des armes et du rapprochement des armes au sol, sans le jeu : les situations
// relevées dans les journaux de la 0.22 (échange de deux armes de la même classe au même endroit)
// sont rejouées sur des tables d'armes fictives.
#include <cstdio>

#include "../src/groundmatch.h"

static int g_failures = 0;

static void Expect(bool condition, const char* what) {
    if (condition) return;
    g_failures++;
    printf("ECHEC : %s\n", what);
}

static const uint32_t kSap = 0x5A9, kTap = 0x7A9;  // deux classes d'armes
static const float kRadius = 60.0f;

static ground::Weapon Weapon(ground::State state, uint32_t classHash, float x) {
    ground::Weapon weapon;
    weapon.state = state;
    weapon.classHash = classHash;
    weapon.x = x;
    return weapon;
}

static proto::GroundWeapon Host(float x, uint32_t classHash, uint16_t ammo, uint16_t id) {
    proto::GroundWeapon entry = {x, 0.0f, 0.0f, classHash, ammo, id};
    return entry;
}

int main() {
    bool exact = false;

    // 1. Numéros : un par arme, jamais le même d'un joueur à l'autre, stable pour une arme.
    {
        ground::Ids ids;
        uint16_t first = ground::Allocate(ids, 0, 1);
        Expect(first == 0x1001, "premier numero du joueur 1");
        Expect(ground::Allocate(ids, 0, 1) == first, "une arme garde son numero");
        Expect(ground::Allocate(ids, 1, 1) == 0x1002, "l'arme suivante prend le numero suivant");
        ground::Ids host;
        Expect(ground::Allocate(host, 0, 0) == 0x0001, "l'hote numerote dans sa propre plage");
        Expect(ground::Allocate(ids, ground::kSlots, 1) == 0, "case hors table : pas de numero");
        // Le compteur fait le tour : un numéro encore porté n'est pas redonné.
        ids.counter = ground::kIdCounterMask;
        Expect(ground::Allocate(ids, 2, 1) == 0x1003, "apres le tour du compteur : le premier numero libre");
        // Un numéro ne désigne qu'une arme.
        ground::Bind(ids, 5, first);
        Expect(ids.of[5] == first && ids.of[0] == 0, "un numero passe d'une case a l'autre, sans doublon");
        Expect(ground::SlotOf(ids, first) == 5 && ground::SlotOf(ids, 0) == -1, "recherche par numero");
    }

    // 2. L'échange vu dans les journaux de la 0.22, chez l'hôte. Le client tient A (28 cartouches)
    //    et la lâche sur B, vide, qu'il ramasse. Son pantin vient de lâcher A à ses pieds, plus
    //    près de l'endroit annoncé que B ne l'est : c'est pourtant B qu'il doit prendre.
    {
        ground::Ids ids;
        ground::Weapon weapons[ground::kSlots];
        weapons[3] = Weapon(ground::kOnGround, kSap, 10.0f);  // A, tout juste lâchée par le pantin
        weapons[7] = Weapon(ground::kOnGround, kSap, 25.0f);  // B
        ground::Bind(ids, 3, 0x1001);
        ground::Bind(ids, 7, 0x0005);

        int slot = ground::FindPicked(ids, weapons, kSap, 12.0f, 0.0f, 0.0f, kRadius, 0x0005, 0x1001, &exact);
        Expect(slot == 7 && exact, "echange : l'arme ramassee est retrouvee par son numero");

        // Le client ne connaissait pas encore le numéro de B.
        slot = ground::FindPicked(ids, weapons, kSap, 12.0f, 0.0f, 0.0f, kRadius, 0, 0x1001, &exact);
        Expect(slot == 7 && !exact, "echange sans numero : jamais l'arme qu'il vient de lacher");
        Expect(ids.of[7] == 0x0005, "echange sans numero : l'arme prise garde le sien");

        // Ce que faisait la 0.22 : la plus proche, donc celle qu'il venait de lâcher.
        slot = ground::FindPicked(ids, weapons, kSap, 12.0f, 0.0f, 0.0f, kRadius, 0, 0, &exact);
        Expect(slot == 3, "sans cette precaution, la plus proche est celle qu'il vient de lacher");

        // Il reprend celle qu'il avait lâchée : annoncée sous son numéro, c'est bien elle.
        slot = ground::FindPicked(ids, weapons, kSap, 24.0f, 0.0f, 0.0f, kRadius, 0x1001, 0x1001, &exact);
        Expect(slot == 3 && exact, "arme lachee puis reprise : retrouvee par son numero");

        // Une autre classe ne répond jamais.
        slot = ground::FindPicked(ids, weapons, kTap, 12.0f, 0.0f, 0.0f, kRadius, 0, 0, &exact);
        Expect(slot == -1, "une arme d'une autre classe n'est jamais prise");
    }

    // 3. Numéro inconnu ici : l'arme n'a pas encore fait connaissance. Seule une arme sans numéro
    //    peut être elle, et elle le prend.
    {
        ground::Ids ids;
        ground::Weapon weapons[ground::kSlots];
        weapons[3] = Weapon(ground::kOnGround, kSap, 10.0f);
        weapons[7] = Weapon(ground::kOnGround, kSap, 25.0f);
        ground::Bind(ids, 3, 0x1001);
        int slot = ground::FindPicked(ids, weapons, kSap, 12.0f, 0.0f, 0.0f, kRadius, 0x0009, 0, &exact);
        Expect(slot == 7 && !exact && ids.of[7] == 0x0009, "numero inconnu : l'arme sans numero le prend");
        Expect(ids.of[3] == 0x1001, "numero inconnu : l'arme voisine garde le sien");
        slot = ground::FindPicked(ids, weapons, kSap, 12.0f, 0.0f, 0.0f, kRadius, 0x000A, 0, &exact);
        Expect(slot == -1, "numero inconnu et aucune arme sans numero : introuvable");
    }

    // 4. Numéro connu, mais l'arme n'est pas où on l'attend.
    {
        ground::Ids ids;
        ground::Weapon weapons[ground::kSlots];
        weapons[2] = Weapon(ground::kOurs, kSap, 0.0f);       // notre joueur la tient
        weapons[4] = Weapon(ground::kOnGround, kSap, 5.0f);   // une autre, sans numéro
        ground::Bind(ids, 2, 0x0005);
        int slot = ground::FindPicked(ids, weapons, kSap, 4.0f, 0.0f, 0.0f, kRadius, 0x0005, 0, &exact);
        Expect(slot == -1 && ids.of[2] == 0x0005 && ids.of[4] == 0, "deja tenue par un joueur ici : aucune autre ne la remplace");

        weapons[2].state = ground::kNpc;  // un PNJ la tient ici : ce numéro lui avait été donné à tort
        slot = ground::FindPicked(ids, weapons, kSap, 4.0f, 0.0f, 0.0f, kRadius, 0x0005, 0, &exact);
        Expect(slot == 4 && ids.of[4] == 0x0005 && ids.of[2] == 0, "tenue par un PNJ ici : le numero revient a l'arme au sol");

        weapons[4].x = 500.0f;  // au sol, mais loin de l'endroit annoncé
        slot = ground::FindPicked(ids, weapons, kSap, 4.0f, 0.0f, 0.0f, kRadius, 0x0005, 0, &exact);
        Expect(slot == -1 && ids.of[4] == 0, "trop loin de l'endroit annonce : ce n'etait pas elle");
    }

    // 5. Liste du sol de l'hôte, chez le client : deux armes de la même classe côte à côte, pas
    //    tout à fait au même endroit sur les deux machines (journal client de la 0.22, 07:48:38 :
    //    l'arme de 29 cartouches passait à 0 et l'arme vide à 29).
    {
        ground::Weapon weapons[ground::kSlots];
        weapons[2] = Weapon(ground::kOnGround, kSap, 0.0f);
        weapons[4] = Weapon(ground::kOnGround, kSap, 8.0f);
        proto::GroundWeapon host[2] = {Host(7.0f, kSap, 0, 0x000B), Host(1.0f, kSap, 29, 0x000C)};
        int slots[2];

        ground::Ids ids;
        ground::Bind(ids, 2, 0x000B);
        ground::Bind(ids, 4, 0x000C);
        ground::Match(ids, weapons, host, 2, kRadius, slots);
        Expect(slots[0] == 2 && slots[1] == 4, "liste du sol : chaque arme retrouvee par son numero, malgre l'endroit");

        // Sans numéro, l'endroit décide, et elles font connaissance ainsi.
        ground::Ids fresh;
        ground::Match(fresh, weapons, host, 2, kRadius, slots);
        Expect(slots[0] == 4 && slots[1] == 2, "liste du sol : sans numero, la plus proche");
        Expect(fresh.of[4] == 0x000B && fresh.of[2] == 0x000C, "liste du sol : elles prennent le numero de l'hote");
        // Une fois la connaissance faite, elle tient même si les armes bougent.
        weapons[2].x = 9.0f;
        weapons[4].x = 0.0f;
        ground::Match(fresh, weapons, host, 2, kRadius, slots);
        Expect(slots[0] == 4 && slots[1] == 2, "liste du sol : une arme connue le reste");
    }

    // 6. Le défaut signalé, chez le client. Il a lâché A (28 cartouches, numéro 21) et tient B,
    //    vide, numéro 22.
    {
        ground::Ids ids;
        ground::Weapon weapons[ground::kSlots];
        weapons[2] = Weapon(ground::kOnGround, kSap, 5.0f);  // A
        weapons[4] = Weapon(ground::kOurs, kSap, 0.0f);      // B
        ground::Bind(ids, 2, 0x1015);
        ground::Bind(ids, 4, 0x0016);
        int slots[2];

        // Liste partie avant que l'hôte ait reçu l'échange : B y est encore au sol, vide.
        proto::GroundWeapon late[1] = {Host(4.0f, kSap, 0, 0x0016)};
        ground::Match(ids, weapons, late, 1, kRadius, slots);
        Expect(slots[0] == -1, "liste en retard : l'arme vide que l'on tient ne passe pas pour celle qu'on a lachee");
        Expect(ids.of[2] == 0x1015 && ids.of[4] == 0x0016, "liste en retard : les numeros restent");

        // Liste suivante : A au sol, avec les cartouches annoncées par le client.
        proto::GroundWeapon next[1] = {Host(9.0f, kSap, 28, 0x1015)};
        ground::Match(ids, weapons, next, 1, kRadius, slots);
        Expect(slots[0] == 2, "liste a jour : l'arme lachee est reconnue");

        // Un numéro inconnu ne se pose jamais sur une arme qui en porte déjà un.
        proto::GroundWeapon other[1] = {Host(5.0f, kSap, 0, 0x0030)};
        ground::Match(ids, weapons, other, 1, kRadius, slots);
        Expect(slots[0] == -1 && ids.of[2] == 0x1015, "une arme connue ne prend pas le numero d'une autre");
    }

    // 7. Numéros donnés à tort : ils se défont.
    {
        ground::Ids ids;
        ground::Weapon weapons[ground::kSlots];
        weapons[2] = Weapon(ground::kNpc, kSap, 0.0f);       // un PNJ la tient ici, au sol chez l'hôte
        weapons[4] = Weapon(ground::kOnGround, kSap, 3.0f);  // la vraie, encore sans numéro
        weapons[6] = Weapon(ground::kOnGround, kSap, 900.0f);
        ground::Bind(ids, 2, 0x0007);
        ground::Bind(ids, 6, 0x0008);
        proto::GroundWeapon host[2] = {Host(2.0f, kSap, 12, 0x0007), Host(40.0f, kSap, 3, 0x0008)};
        int slots[2];
        ground::Match(ids, weapons, host, 2, kRadius, slots);
        Expect(slots[0] == 4 && ids.of[4] == 0x0007 && ids.of[2] == 0, "tenue par un PNJ ici : le numero passe a l'arme au sol");
        Expect(slots[1] == -1 && ids.of[6] == 0, "au sol mais trop loin de celle de l'hote : numero retire");
        // Une case réutilisée par une arme d'une autre classe.
        weapons[4].classHash = kTap;
        ground::Match(ids, weapons, host, 1, kRadius, slots);
        Expect(slots[0] == -1 && ids.of[4] == 0, "case reprise par une autre classe : numero retire");
    }

    printf("%s\n", g_failures == 0 ? "OK" : "ECHEC");
    return g_failures == 0 ? 0 : 1;
}
