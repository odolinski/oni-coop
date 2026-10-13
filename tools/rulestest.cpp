// Test des règles de la partie coop, sans le jeu : les quatre modes de difficulté, le compte à
// rebours d'un joueur à terre à travers une réanimation interrompue, le multiplicateur d'ennemis et
// son minimum selon le nombre de joueurs, les noms des renforts et des civils.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "../src/rules.h"

static int g_failures = 0;

static void Expect(bool condition, const char* what) {
    if (condition) return;
    g_failures++;
    printf("ECHEC : %s\n", what);
}

static bool Near(double a, double b) { return std::fabs(a - b) < 1e-6; }

int main() {
    // 1. Les quatre modes, tels que demandés.
    const rules::ModeRules& easy = rules::Of(rules::kEasy);
    const rules::ModeRules& normal = rules::Of(rules::kNormal);
    const rules::ModeRules& hard = rules::Of(rules::kHard);
    const rules::ModeRules& survival = rules::Of(rules::kSurvival);
    Expect(Near(easy.bleedSeconds, 35) && easy.revivePercent == 45 && easy.engineLevel == 0, "facile : 35 s, 45 %");
    Expect(Near(normal.bleedSeconds, 25) && normal.revivePercent == 40 && normal.engineLevel == 1,
           "normal : 25 s, 40 %");
    Expect(Near(hard.bleedSeconds, 10) && hard.revivePercent == 35 && hard.engineLevel == 2, "difficile : 10 s, 35 %");
    Expect(Near(survival.bleedSeconds, 5) && survival.revivePercent == 25 && survival.engineLevel == 2,
           "survie : 5 s, 25 %, ennemis du mode difficile");
    Expect(!easy.regenEnemy && !normal.regenEnemy && !hard.regenEnemy && !easy.regenBoss && !hard.regenBoss,
           "pas de regeneration hors survie");
    Expect(survival.regenEnemy == 1 && survival.regenBoss == 2, "survie : +1 par seconde, +2 pour un boss");
    Expect(easy.healthPercent == 100 && hard.damagePercent == 100 && survival.healthPercent > 100 &&
               survival.damagePercent > 100,
           "survie seule va au-dela du jeu d'origine");
    Expect(&rules::Of(200) == &normal, "mode inconnu : normal");
    Expect(rules::ModeFromKey("survival", rules::kNormal) == rules::kSurvival &&
               rules::ModeFromKey("n'importe", rules::kHard) == rules::kHard &&
               rules::ModeFromKey(nullptr, rules::kEasy) == rules::kEasy,
           "mode lu dans onimp.ini");
    Expect(rules::NextMode(rules::kEasy) == rules::kNormal && rules::NextMode(rules::kSurvival) == rules::kEasy,
           "menu : mode suivant, en boucle");

    // 2. Santé d'un joueur ranimé.
    Expect(rules::RevivedHealth(200, 45) == 90 && rules::RevivedHealth(200, 40) == 80 &&
               rules::RevivedHealth(200, 35) == 70 && rules::RevivedHealth(200, 25) == 50,
           "releve a 45, 40, 35, 25 pour cent");
    Expect(rules::RevivedHealth(3, 25) == 2 && rules::RevivedHealth(0, 45) == 2, "releve : jamais moins de deux points");

    // 3. Compte à rebours en mode normal. Un coéquipier commence à ranimer à 17,5 s de la fin, est
    //    frappé au bout de deux secondes : le compte reprend exactement à 17,5 s.
    rules::Bleed bleed;
    bleed.Start(normal.bleedSeconds);
    bool over = false;
    for (int i = 0; i < 450; i++) over = bleed.Tick(1.0 / 60.0, false) || over;
    Expect(!over && Near(bleed.remaining(), 17.5), "compte a rebours : 7,5 s ecoulees");
    for (int i = 0; i < 120; i++) over = bleed.Tick(1.0 / 60.0, true) || over;
    Expect(!over && Near(bleed.remaining(), 17.5), "compte a rebours : arrete pendant la reanimation");
    for (int i = 0; i < 60; i++) over = bleed.Tick(1.0 / 60.0, false) || over;
    Expect(!over && Near(bleed.remaining(), 16.5), "compte a rebours : repris ou il s'etait arrete");
    int ticks = 0;
    while (!bleed.Tick(1.0 / 60.0, false) && ticks < 10000) ticks++;
    Expect(ticks >= 988 && ticks <= 991 && Near(bleed.remaining(), 0.0), "compte a rebours : zero apres 16,5 s de plus");
    Expect(bleed.Tick(1.0, true), "compte a rebours : epuise, il le reste");
    Expect(Near(bleed.total(), 25.0), "compte a rebours : duree de depart gardee");

    // Tel qu'il voyage dans l'état du joueur.
    Expect(rules::BleedToWire(35.0) == 350 && rules::BleedToWire(0.04) == 1 && rules::BleedToWire(0.0) == 0 &&
               rules::BleedToWire(-3.0) == 0,
           "compte a rebours : dixiemes de seconde, zero seulement une fois epuise");
    Expect(Near(rules::BleedFromWire(125), 12.5) && Near(rules::BleedFromWire(-4), 0.0), "compte a rebours : relu");

    // 4. Multiplicateur d'ennemis : libre seul, x2 au moins à deux, x3 à trois, x4 verrouillé à quatre.
    Expect(rules::MinEnemies(1) == 1 && rules::MinEnemies(2) == 2 && rules::MinEnemies(3) == 3 &&
               rules::MinEnemies(4) == 4 && rules::MinEnemies(9) == 4 && rules::MinEnemies(0) == 1,
           "minimum selon le nombre de joueurs");
    for (int chosen = 1; chosen <= 4; chosen++) {
        Expect(rules::EffectiveEnemies(chosen, 1) == chosen, "seul : choix libre");
        Expect(rules::EffectiveEnemies(chosen, 4) == 4, "a quatre : x4 quel que soit le choix");
    }
    Expect(rules::EffectiveEnemies(1, 2) == 2 && rules::EffectiveEnemies(3, 2) == 3 &&
               rules::EffectiveEnemies(1, 3) == 3 && rules::EffectiveEnemies(4, 3) == 4,
           "a deux et a trois : releve au minimum, sinon garde");
    Expect(rules::EffectiveEnemies(0, 1) == 1 && rules::EffectiveEnemies(99, 1) == 4, "choix hors bornes ramene");
    // Le bouton du menu ne propose que ce qui est permis.
    Expect(rules::NextEnemies(1, 1) == 2 && rules::NextEnemies(4, 1) == 1, "menu, seul : x1 a x4 en boucle");
    Expect(rules::NextEnemies(1, 2) == 3 && rules::NextEnemies(2, 2) == 3 && rules::NextEnemies(4, 2) == 2,
           "menu, a deux : x2, x3, x4");
    Expect(rules::NextEnemies(3, 3) == 4 && rules::NextEnemies(4, 3) == 3, "menu, a trois : x3, x4");
    Expect(rules::NextEnemies(1, 4) == 4 && rules::NextEnemies(4, 4) == 4, "menu, a quatre : x4 seulement");

    // 5. Noms des renforts.
    char name[rules::kNameSize], original[rules::kNameSize];
    int rank = 0;
    Expect(rules::CloneName("Floor1_Striker_2", 2, name, sizeof(name)) && strcmp(name, "Floor1_Striker_2_x2") == 0,
           "renfort : nom");
    Expect(rules::ParseCloneName(name, original, sizeof(original), &rank) && rank == 2 &&
               strcmp(original, "Floor1_Striker_2") == 0,
           "renfort : nom relu");
    Expect(rules::CloneName("a", 4, name, sizeof(name)) && rules::ParseCloneName(name, original, sizeof(original), &rank) &&
               rank == 4 && strcmp(original, "a") == 0,
           "renfort : nom d'une lettre, rang 4");
    Expect(!rules::CloneName("Floor1_Striker_2", 1, name, sizeof(name)) &&
               !rules::CloneName("Floor1_Striker_2", 5, name, sizeof(name)) && !rules::CloneName("", 2, name, sizeof(name)),
           "renfort : rangs 2 a 4 seulement, original nomme");
    Expect(rules::CloneName("abcdefghijklmnopqrstuvwxyz01", 2, name, sizeof(name)) && strlen(name) == 31,
           "renfort : 28 caracteres, le nom tient tout juste");
    Expect(!rules::CloneName("abcdefghijklmnopqrstuvwxyz012", 2, name, sizeof(name)), "renfort : original trop long");
    Expect(!rules::ParseCloneName("Floor1_Striker_2", original, sizeof(original), &rank) &&
               !rules::ParseCloneName("Striker_x5", original, sizeof(original), &rank) &&
               !rules::ParseCloneName("Striker_x1", original, sizeof(original), &rank) &&
               !rules::ParseCloneName("_x2", original, sizeof(original), &rank),
           "renfort : un personnage du niveau n'en est pas un");

    // 6. Noms des civils : la classe se relit dans le nom.
    char className[rules::kNameSize];
    Expect(rules::CivilianName(7, "generic_female_9", name, sizeof(name)) && strcmp(name, "mpc07_generic_female_9") == 0,
           "civil : nom");
    Expect(rules::ParseCivilianName(name, className, sizeof(className)) && strcmp(className, "generic_female_9") == 0,
           "civil : classe relue");
    Expect(rules::ParseCivilianName(name, nullptr, rules::kNameSize), "civil : reconnu sans demander sa classe");
    Expect(!rules::ParseCivilianName("mpc7_bd_1", className, sizeof(className)) &&
               !rules::ParseCivilianName("mpc07_", className, sizeof(className)) &&
               !rules::ParseCivilianName("mpc07_bd 1", className, sizeof(className)) &&
               !rules::ParseCivilianName("mp_remote1", className, sizeof(className)) &&
               !rules::ParseCivilianName("Floor1_Sci_1", className, sizeof(className)),
           "civil : rien d'autre n'en est un");
    Expect(!rules::CivilianName(100, "bd_1", name, sizeof(name)) && !rules::CivilianName(0, "", name, sizeof(name)),
           "civil : numero et classe bornes");
    Expect(!rules::ParseCloneName("mpc07_generic_female_9", original, sizeof(original), &rank), "civil : pas un renfort");

    // 7. Boss.
    Expect(rules::IsBossClass("barabus") && rules::IsBossClass("muro_generic") && rules::IsBossClass("mutantmuro") &&
               rules::IsBossClass("super_ninja_1"),
           "boss : Barabas, Muro, Muro transforme, Mukade");
    Expect(!rules::IsBossClass("striker_hard_1") && !rules::IsBossClass("ninja_hard_1") && !rules::IsBossClass("") &&
               !rules::IsBossClass(nullptr),
           "boss : pas les autres");

    // 8. Civils : dans les niveaux qui s'y prêtent seulement, avec des classes de civils.
    int count = 0;
    Expect(rules::CivilianClasses("Airport", &count) && count == 15, "civils : aerogare, passants");
    Expect(rules::CivilianClasses("lab", &count) && count == 9, "civils : laboratoire, personnel");
    Expect(rules::CivilianClasses("airport_iii", &count) && rules::CivilianClasses("TCTF", &count) &&
               !rules::CivilianClasses("airport_ii", &count) && !rules::CivilianClasses("tctf_", &count),
           "civils : nom du niveau sans tenir compte de la casse, entier");
    Expect(!rules::CivilianClasses("roof", &count) && count == 0 && !rules::CivilianClasses("dream_lab", &count) &&
               !rules::CivilianClasses("compound", &count) && !rules::CivilianClasses(nullptr, &count),
           "civils : ni sur les toits, ni dans le reve, ni au repaire");
    Expect(rules::CivilianClassFor("tctf", 12345) != nullptr && rules::CivilianClassFor("roof", 12345) == nullptr,
           "civils : classe tiree pour le niveau");
    // Un ennemi sur six environ, et toujours les mêmes d'une reprise à l'autre.
    int brought = 0;
    for (uint32_t enemy = 0; enemy < 6000; enemy++) brought += rules::BringsCivilian(0xD3CAB9F7u, enemy * 2654435761u);
    Expect(brought > 850 && brought < 1150, "civils : un ennemi sur six environ");
    Expect(rules::BringsCivilian(1, 42) == rules::BringsCivilian(1, 42), "civils : meme tirage a chaque fois");

    // Vagues d'alarme : trois ennemis par joueur, et un nom que chaque machine sait relire.
    Expect(rules::WaveSize(1) == 3 && rules::WaveSize(2) == 6 && rules::WaveSize(4) == 12, "vague : trois ennemis par joueur");
    Expect(rules::WaveSize(0) == 3 && rules::WaveSize(9) == 12, "vague : bornee a un et quatre joueurs");
    char wave[rules::kNameSize], back[rules::kNameSize];
    Expect(rules::WaveName("ambush_striker_3", 7, wave, sizeof(wave)) && strcmp(wave, "ambush_striker_3_w07") == 0,
           "vague : nom de l'original suivi de _w07");
    Expect(rules::ParseWaveName(wave, back, sizeof(back)) && strcmp(back, "ambush_striker_3") == 0,
           "vague : l'original se relit dans le nom");
    Expect(!rules::ParseWaveName("ambush_striker_3", back, sizeof(back)) && !rules::ParseWaveName("a_x2", back, sizeof(back)) &&
               !rules::ParseWaveName("guard_wab", back, sizeof(back)),
           "vague : un nom du niveau ou un renfort n'en est pas une");
    Expect(!rules::WaveName("un_nom_de_personnage_bien_long_x", 1, wave, sizeof(wave)) &&
               !rules::WaveName("striker", 100, wave, sizeof(wave)) && !rules::WaveName("", 1, wave, sizeof(wave)),
           "vague : nom trop long, numero hors bornes ou nom vide refuses");
    Expect(!rules::ParseCloneName("ambush_striker_3_w07", back, sizeof(back), &rank), "vague : pas prise pour un renfort");

    // Rayons des lasers mobiles : écart sur le rail, et ce qu'on tolère avant de recaler.
    Expect(rules::RayGap(10.0f, 14.5f, 100, false) == 4.5f && rules::RayGap(14.5f, 10.0f, 100, false) == 4.5f,
           "rayon : ecart sans signe");
    Expect(rules::RayGap(2.0f, 98.0f, 100, true) == 4.0f && rules::RayGap(2.0f, 98.0f, 100, false) == 96.0f,
           "rayon : un rail en boucle se referme, un rail en va-et-vient non");
    Expect(rules::RayGap(10.0f, 40.0f, 100, true) == 30.0f && rules::RayGap(0.0f, 50.0f, 100, true) == 50.0f,
           "rayon : en boucle, le plus court des deux chemins");
    Expect(rules::RayGap(3.0f, 9.0f, 0, true) == 6.0f, "rayon : rail de longueur inconnue, ecart brut");
    Expect(rules::RayTolerance(1.0f) == 7.5f && rules::RayTolerance(-2.0f) == 13.5f && rules::RayTolerance(0.0f) == 1.5f,
           "rayon : tolerance selon la vitesse, quel que soit le sens");
    // Le pas du moteur, refait pour ramener la position de l'hôte à l'instant où elle arrive.
    float frame = 10.0f;
    int direction = 1;
    rules::AdvanceRay(&frame, &direction, 0.5f, 6, 100, false);
    Expect(frame == 13.0f && direction == 1, "rayon : six ticks a une demi-image par tick");
    frame = 98.0f;
    direction = 1;
    rules::AdvanceRay(&frame, &direction, 1.0f, 3, 100, false);
    Expect(frame == 98.0f && direction == -1, "rayon : en va-et-vient, demi-tour sur la derniere image puis retour");
    frame = 98.0f;
    direction = 1;
    rules::AdvanceRay(&frame, &direction, 1.0f, 3, 100, true);
    Expect(frame == 1.0f && direction == 1, "rayon : en boucle, il repart du debut dans le meme sens");
    frame = 1.0f;
    direction = -1;
    rules::AdvanceRay(&frame, &direction, 1.0f, 3, 100, false);
    Expect(frame == 1.0f && direction == 1, "rayon : en va-et-vient, demi-tour sur la premiere image");
    frame = 1.0f;
    direction = -1;
    rules::AdvanceRay(&frame, &direction, 1.0f, 3, 100, true);
    Expect(frame == 98.0f && direction == -1, "rayon : en boucle a reculons, il repart de la fin");
    frame = 250.0f;
    direction = 1;
    rules::AdvanceRay(&frame, &direction, 1.0f, 3, 100, false);
    Expect(frame == 250.0f && direction == 1, "rayon : une position hors du rail est laissee telle quelle");
    frame = 5.0f;
    rules::AdvanceRay(&frame, &direction, 1.0f, 0, 100, false);
    Expect(frame == 5.0f, "rayon : zero tick, rien ne bouge");

    // Cible d'une tourelle : qui vient de couper son laser, puis qui elle peut toucher, puis le plus proche.
    rules::TurretChoice why = rules::kTurretNobody;
    const rules::TurretCandidate hidden[] = {{270.0f, false, false}, {328.0f, true, true}};
    Expect(rules::ChooseTurretTarget(hidden, 2, 0, 1.5f, &why) == 1 && why == rules::kTurretTripper,
           "tourelle : celui qui vient de couper le laser, l'autre etant cache derriere un mur");
    const rules::TurretCandidate idle[] = {{80.0f, true, false}, {300.0f, true, true}};
    Expect(rules::ChooseTurretTarget(idle, 2, 0, 1.5f, &why) == 1 && why == rules::kTurretTripper,
           "tourelle : celui qui vient de couper le laser, meme si l'autre est plus pres et a decouvert");
    const rules::TurretCandidate nobody[] = {{270.0f, false, false}, {328.0f, true, false}};
    Expect(rules::ChooseTurretTarget(nobody, 2, 0, 1.5f, &why) == 1 && why == rules::kTurretReachable,
           "tourelle : le joueur a decouvert plutot que le plus proche cache derriere un mur");
    const rules::TurretCandidate cover[] = {{100.0f, true, false}, {60.0f, false, true}};
    Expect(rules::ChooseTurretTarget(cover, 2, -1, 1.5f, &why) == 0 && why == rules::kTurretReachable,
           "tourelle : celui qui a coupe le laser s'est mis a l'abri, elle prend l'autre");
    const rules::TurretCandidate both[] = {{140.0f, true, true}, {100.0f, true, true}};
    Expect(rules::ChooseTurretTarget(both, 2, -1, 1.5f, &why) == 1 && why == rules::kTurretTripper,
           "tourelle : deux joueurs dans ses lasers, le plus proche");
    Expect(rules::ChooseTurretTarget(both, 2, 0, 1.5f, &why) == 0, "tourelle : cible du moment gardee a moins d'une fois et demie");
    const rules::TurretCandidate far[] = {{160.0f, true, true}, {100.0f, true, true}};
    Expect(rules::ChooseTurretTarget(far, 2, 0, 1.5f, &why) == 1, "tourelle : cible du moment lachee au-dela");
    const rules::TurretCandidate stale[] = {{50.0f, true, false}, {200.0f, true, true}};
    Expect(rules::ChooseTurretTarget(stale, 2, 0, 1.5f, &why) == 1 && why == rules::kTurretTripper,
           "tourelle : la cible du moment n'a rien coupe, elle passe a celui qui vient de couper le laser");
    const rules::TurretCandidate blind[] = {{140.0f, false, true}, {100.0f, false, false}};
    Expect(rules::ChooseTurretTarget(blind, 2, 0, 1.5f, &why) == 0 && why == rules::kTurretNearest,
           "tourelle : personne a decouvert, elle suit sa cible du moment sans tirer");
    Expect(rules::ChooseTurretTarget(blind, 2, -1, 1.5f, &why) == 1 && why == rules::kTurretNearest,
           "tourelle : personne a decouvert et pas de cible, le plus proche");
    Expect(rules::ChooseTurretTarget(both, 0, -1, 1.5f, &why) == -1 && why == rules::kTurretNobody,
           "tourelle : aucun joueur a viser");
    Expect(rules::ChooseTurretTarget(both, 2, 7, 1.5f, &why) == 1, "tourelle : rang hors de la liste ignore");

    if (g_failures) {
        printf("%d echec(s)\n", g_failures);
        return 1;
    }
    printf("OK\n");
    return 0;
}
