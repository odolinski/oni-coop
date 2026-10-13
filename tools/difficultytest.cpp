// Test de la difficulté selon le nombre de clients, sans le jeu : table des valeurs et ses bornes,
// vie et dégâts des PNJ, compte des clients à travers arrivées, départs et reconnexions.
// Avec un fichier onimp_difficulty.ini en argument : affiche les valeurs que le jeu en tirera, sans
// rien tester.
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "../src/config.h"
#include "../src/difficulty.h"

static int g_failures = 0;

static void Expect(bool condition, const char* what) {
    if (condition) return;
    g_failures++;
    printf("ECHEC : %s\n", what);
}

static bool Write(const std::string& path, const char* text) {
    FILE* file = fopen(path.c_str(), "wb");
    if (!file) return false;
    bool written = fwrite(text, 1, strlen(text), file) == strlen(text);
    return fclose(file) == 0 && written;
}

static int Show(const char* path) {
    bool found = false;
    difficulty::Table table = cfg::LoadDifficulty(path, &found);
    printf("%s : %s\n", path, found ? "lu" : "absent, valeurs d'origine");
    for (int clients = 1; clients <= difficulty::kMaxClients; clients++) {
        printf("  %d client(s) : vie %u pour cent, degats %u pour cent\n", clients,
               static_cast<unsigned>(table.health[clients]), static_cast<unsigned>(table.damage[clients]));
    }
    return found ? 0 : 1;
}

int main(int argc, char** argv) {
    if (argc > 1) return Show(argv[1]);

    // 1. Valeurs d'origine du fichier : la vie une fois de plus par client, 5 % de dégâts de plus.
    difficulty::Table table = difficulty::DefaultTable();
    Expect(difficulty::HealthPercent(table, 0) == 100 && difficulty::HealthPercent(table, 1) == 200 &&
               difficulty::HealthPercent(table, 2) == 300 && difficulty::HealthPercent(table, 3) == 400,
           "table : vie 100 seul, puis 200, 300, 400");
    Expect(difficulty::DamagePercent(table, 0) == 100 && difficulty::DamagePercent(table, 1) == 105 &&
               difficulty::DamagePercent(table, 2) == 110 && difficulty::DamagePercent(table, 3) == 115,
           "table : degats 100 seul, puis 105, 110, 115");
    Expect(difficulty::HealthPercent(table, 9) == 400, "table : au-dela de trois clients, la derniere valeur");

    // Un fichier fantaisiste : chaque valeur est ramenée dans ses bornes, et l'hôte seul reste à 100.
    difficulty::Table odd = {{555, 0, 150, 99999}, {7, 0, 250, 99999}};
    odd = difficulty::Sanitized(odd);
    Expect(odd.health[0] == 100 && odd.damage[0] == 100, "bornes : l'hote seul reste a 100");
    Expect(odd.health[1] == difficulty::kMinHealthPercent && odd.health[2] == 150 &&
               odd.health[3] == difficulty::kMaxHealthPercent,
           "bornes : vie");
    Expect(odd.damage[1] == 0 && odd.damage[2] == 250 && odd.damage[3] == difficulty::kMaxDamagePercent,
           "bornes : degats, zero permis");

    // 2. Vie d'un PNJ : un Striker à 100 points, blessé à 37, voit arriver trois clients puis les
    //    voit partir. Son maximum revient exactement au sien, et sa santé aussi.
    uint32_t maxHealth = 100, health = 37;
    difficulty::Rescale(maxHealth, health, 100, 200, false);
    Expect(maxHealth == 200 && health == 74, "vie : un client, maximum et sante doubles");
    difficulty::Rescale(maxHealth, health, 100, 300, false);
    Expect(maxHealth == 300 && health == 111, "vie : deux clients, triples");
    difficulty::Rescale(maxHealth, health, 100, 400, false);
    Expect(maxHealth == 400 && health == 148, "vie : trois clients, quadruples");
    difficulty::Rescale(maxHealth, health, 100, 200, false);
    Expect(maxHealth == 200 && health == 74, "vie : deux clients partent, de nouveau doubles");
    difficulty::Rescale(maxHealth, health, 100, 100, false);
    Expect(maxHealth == 100 && health == 37, "vie : plus de client, valeurs d'origine");

    // Valeurs choisies dans le fichier : 250 %, puis moins que l'origine.
    difficulty::Rescale(maxHealth, health, 100, 250, false);
    Expect(maxHealth == 250 && health == 93, "vie : 250 pour cent, sante arrondie vers le haut");
    difficulty::Rescale(maxHealth, health, 100, 50, false);
    Expect(maxHealth == 50 && health == 19, "vie : 50 pour cent");

    maxHealth = 300;
    health = 1;
    difficulty::Rescale(maxHealth, health, 100, 100, false);
    Expect(maxHealth == 100 && health == 1, "vie : un PNJ a un point ne meurt pas d'une baisse");
    maxHealth = 100;
    health = 0;
    difficulty::Rescale(maxHealth, health, 100, 400, true);
    Expect(maxHealth == 400 && health == 0, "vie : un PNJ mort le reste");
    Expect(difficulty::ScaledMax(3, 10) == 1, "vie : un maximum ne tombe jamais a zero");

    // 3. Dégâts : cent coups de 10 points à 105 % valent 1050 points, pas 1000 ni 1100.
    uint32_t carry = 0, total = 0, smallest = 1000, largest = 0;
    for (int hit = 0; hit < 100; hit++) {
        uint32_t damage = difficulty::ScaleDamage(10, 105, carry);
        total += damage;
        if (damage < smallest) smallest = damage;
        if (damage > largest) largest = damage;
    }
    Expect(total == 1050 && smallest == 10 && largest == 11, "degats : 105 pour cent exacts sur cent coups de 10");
    carry = 0;
    total = 0;
    for (int hit = 0; hit < 200; hit++) total += difficulty::ScaleDamage(3, 115, carry);
    Expect(total == 690, "degats : 115 pour cent exacts sur deux cents coups de 3");
    carry = 0;
    total = 0;
    for (int hit = 0; hit < 100; hit++) total += difficulty::ScaleDamage(3, 50, carry);
    Expect(total == 150, "degats : 50 pour cent exacts sur cent coups de 3");
    carry = 0;
    Expect(difficulty::ScaleDamage(40, 110, carry) == 44 && carry == 0, "degats : 110 pour cent d'un coup de 40");
    Expect(difficulty::ScaleDamage(40, 0, carry) == 0, "degats : a zero pour cent, plus rien");
    carry = 7;
    Expect(difficulty::ScaleDamage(25, 100, carry) == 25 && carry == 7, "degats : a 100 pour cent, coup intact");
    Expect(difficulty::ScaleDamage(0, 115, carry) == 0 && carry == 7, "degats : un coup nul le reste");

    // 4. Compte des clients. Une arrivée compte tout de suite.
    difficulty::ClientCount count;
    Expect(!count.Update(0, false, 0.0) && count.value() == 0, "compte : personne");
    Expect(count.Update(1, false, 1.0) && count.value() == 1, "compte : un client arrive");
    Expect(count.Update(3, false, 2.0) && count.value() == 3, "compte : deux de plus");
    // Un client recharge son niveau : parti deux secondes, puis revenu. Rien ne bouge.
    Expect(!count.Update(2, false, 10.0) && !count.Update(2, false, 11.0) && count.value() == 3,
           "compte : un client absent un instant compte encore");
    Expect(!count.Update(3, false, 12.0) && count.value() == 3, "compte : il est revenu");
    // Parti pour de bon : compté encore vingt secondes, plus ensuite.
    Expect(!count.Update(2, false, 30.0) && !count.Update(2, false, 49.9) && count.value() == 3,
           "compte : depart pas encore pris");
    Expect(count.Update(2, false, 50.0) && count.value() == 2, "compte : depart pris apres vingt secondes");
    // Le délai repart à chaque nouvelle absence : un aller-retour l'annule.
    count.Update(1, false, 60.0);
    count.Update(2, false, 70.0);
    Expect(!count.Update(1, false, 75.0) && !count.Update(1, false, 94.9) && count.value() == 2,
           "compte : le delai repart de la nouvelle absence");
    Expect(count.Update(1, false, 95.0) && count.value() == 1, "compte : puis elle est prise");
    // Un client suit l'hôte, qui a déjà attendu : baisse prise sans délai.
    Expect(count.Update(0, true, 96.0) && count.value() == 0, "compte : baisse immediate quand on le demande");

    // 5. Le fichier, modifié pendant que le jeu tourne : ce qui vient d'y être enregistré est ce
    //    qui est lu, et sa date a changé, ce que l'hôte guette pour le relire.
    char folder[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, folder);
    std::string path = std::string(folder) + "onimp_difficultytest.ini";
    DeleteFileA(path.c_str());
    bool found = true;
    table = cfg::LoadDifficulty(path.c_str(), &found);
    Expect(!found && cfg::DifficultyStamp(path.c_str()) == 0 && table.health[1] == 200 && table.damage[3] == 115,
           "fichier : absent, valeurs d'origine");
    Expect(Write(path, "; essai\n[health]\nclients_1 = 150\n\n[damage]\nclients_1 = 200\n"), "fichier : ecrit");
    uint64_t first = cfg::DifficultyStamp(path.c_str());
    table = cfg::LoadDifficulty(path.c_str(), &found);
    Expect(found && first != 0, "fichier : present, date connue");
    Expect(table.health[1] == 150 && table.health[2] == 300 && table.damage[1] == 200 && table.damage[2] == 110,
           "fichier : valeurs ecrites lues, les autres d'origine");
    Sleep(50);
    // Enregistré par un éditeur qui écrit des fins de ligne Windows.
    Expect(Write(path, "[health]\r\nclients_1 = 350\r\nclients_2 = 99999\r\n[damage]\r\nclients_1 = 50\r\n"),
           "fichier : reecrit");
    Expect(cfg::DifficultyStamp(path.c_str()) != first, "fichier : sa date change quand il est reenregistre");
    table = cfg::LoadDifficulty(path.c_str(), &found);
    Expect(table.health[1] == 350 && table.health[2] == difficulty::kMaxHealthPercent && table.health[3] == 400 &&
               table.damage[1] == 50,
           "fichier : relu, nouvelles valeurs sans relancer");

    // 6. Curseurs du menu F1 : une valeur par ligne, ce que chaque client ajoute. La table d'origine
    //    est de cette forme, +100 pour la vie et +5 pour les dégâts.
    table = difficulty::DefaultTable();
    Expect(difficulty::PerClientBonus(table.health) == 100 && difficulty::PerClientBonus(table.damage) == 5,
           "curseur : la table d'origine vaut +100 et +5 par client");
    difficulty::SetPerClientBonus(table.health, 250);
    difficulty::SetPerClientBonus(table.damage, 0);
    Expect(table.health[0] == 100 && table.health[1] == 350 && table.health[2] == 600 && table.health[3] == 850,
           "curseur : +250 par client");
    Expect(table.damage[1] == 100 && table.damage[3] == 100 && difficulty::PerClientBonus(table.damage) == 0,
           "curseur : a zero, valeurs d'origine");
    difficulty::Table byHand = {{100, 150, 300, 400}, {100, 50, 110, 115}};
    Expect(difficulty::PerClientBonus(byHand.health) == -1 && difficulty::PerClientBonus(byHand.damage) == -1,
           "curseur : une ligne ecrite a la main n'est pas un bonus par client");
    // Le plus haut cran de chaque curseur tient dans les bornes du fichier.
    difficulty::SetPerClientBonus(table.health, 500);
    difficulty::SetPerClientBonus(table.damage, 200);
    difficulty::Table kept = difficulty::Sanitized(table);
    Expect(kept.health[3] == 1600 && kept.damage[3] == 700 && difficulty::PerClientBonus(kept.health) == 500,
           "curseur : crans les plus hauts dans les bornes");

    // Ce que le menu règle est écrit dans le fichier, sans en perdre les commentaires.
    Expect(Write(path, "; a garder\n[health]\n; vie\nclients_1 = 200\nclients_2 = 300\nclients_3 = 400\n\n[damage]\n"
                       "clients_1 = 105\n"),
           "fichier : ecrit pour le menu");
    cfg::SaveDifficulty(path.c_str(), kept);
    table = cfg::LoadDifficulty(path.c_str(), &found);
    Expect(memcmp(&table, &kept, sizeof(table)) == 0, "fichier : valeurs du menu ecrites puis relues");
    std::string content;
    if (FILE* file = fopen(path.c_str(), "rb")) {
        char buffer[512];
        size_t size = fread(buffer, 1, sizeof(buffer), file);
        content.assign(buffer, size);
        fclose(file);
    }
    Expect(content.find("; a garder") != std::string::npos && content.find("; vie") != std::string::npos,
           "fichier : commentaires gardes");
    DeleteFileA(path.c_str());
    cfg::SaveDifficulty(path.c_str(), kept);
    table = cfg::LoadDifficulty(path.c_str(), &found);
    Expect(found && memcmp(&table, &kept, sizeof(table)) == 0, "fichier : absent, cree par le menu");
    DeleteFileA(path.c_str());

    printf("%s\n", g_failures == 0 ? "OK" : "ECHEC");
    return g_failures == 0 ? 0 : 1;
}
