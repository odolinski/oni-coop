// Test de ce qu'onimp.ini garde d'un lancement à l'autre, sans le jeu : les réglages changés dans
// le menu F1 par l'hôte et par un client qui partagent le même fichier, comme sur une seule
// machine. Chacun doit retrouver les siens, et aucun ne doit défaire ceux de l'autre.
// Écrit puis supprime un onimp.ini à côté de cet outil.
#include <windows.h>

#include <cstdio>
#include <string>

#include "../src/config.h"

static int g_failures = 0;

static void Expect(bool condition, const char* what) {
    if (condition) return;
    g_failures++;
    printf("ECHEC : %s\n", what);
}

// Un lancement du jeu dans ce rôle : la configuration repart de ses valeurs d'origine, puis lit le
// fichier.
static cfg::Config& Launch(const char* role) {
    SetEnvironmentVariableA("ONIMP", role);
    cfg::Get() = cfg::Config();
    cfg::Load();
    return cfg::Get();
}

int main() {
    std::string ini = cfg::GameDir() + "onimp.ini";
    DeleteFileA(ini.c_str());
    SetEnvironmentVariableA("ONIMP_NPCS", nullptr);
    SetEnvironmentVariableA("ONIMP_QUESTS", nullptr);
    SetEnvironmentVariableA("ONIMP_TITLE", nullptr);

    // 1. Sans fichier : les valeurs d'origine.
    cfg::Config& first = Launch("host");
    Expect(first.mode == cfg::Mode::Host, "role de l'hote");
    Expect(first.language == "fr" && first.showThroughWalls && first.alertVolume == 50, "valeurs d'origine de l'affichage");
    Expect(!first.teamKill && !first.cheats && first.cheatAccess == 0, "regles coupees a l'origine");
    Expect(!first.cheatGod && !first.cheatOneHit && first.cheatMoveSpeed == 100 && first.cheatJump == 100 &&
               first.character.empty() && first.characterKey == "F5",
           "cheats coupes a l'origine, touche F5");
    Expect(first.lowPortraits && !first.autoPlay, "portraits en bas et pas de jeu automatique a l'origine");

    // 2. L'hôte change tout dans son menu.
    first.language = "en";
    first.showThroughWalls = false;
    first.showPing = false;
    first.alertVolume = 35;
    first.teamKill = true;
    first.cheats = true;
    first.cheatAccess = 0x2;
    first.cheatGod = true;
    first.cheatMoveSpeed = 150;
    first.character = "striker_easy_1";
    first.playerName = "Hote";
    cfg::SaveDisplay();
    cfg::SaveAudio();
    cfg::SaveRules();
    cfg::SaveCheats();
    cfg::SavePlayer();

    // 3. Un client lancé ensuite, qui n'a encore rien réglé, part de ce que dit la section commune.
    cfg::Config& client = Launch("connect:127.0.0.1");
    Expect(client.mode == cfg::Mode::Client, "role du client");
    Expect(client.language == "en" && !client.showThroughWalls && !client.showPing && client.alertVolume == 35,
           "client sans reglage : ceux de la section commune");
    Expect(client.playerName.empty(), "le pseudo de l'hote n'est pas celui du client");
    Expect(!client.cheatGod && client.cheatMoveSpeed == 100 && client.character.empty(),
           "les cheats de l'hote ne sont pas ceux du client");

    // 4. Le client change les siens.
    client.language = "fr";
    client.showThroughWalls = true;
    client.showNpcIds = false;
    client.alertVolume = 60;
    client.playerName = "Client";
    client.lowPortraits = false;
    client.autoPlay = true;
    client.cheatOneHit = true;
    client.cheatJump = 250;
    client.characterKey = "F8";
    cfg::SaveDisplay();
    cfg::SaveAudio();
    cfg::SavePlayer();
    cfg::SaveAutoPlay();
    cfg::SaveCheats();

    // 5. Relance de l'hôte : il retrouve tout ce qu'il avait réglé, rien de ce que le client a changé.
    cfg::Config& host = Launch("host");
    Expect(host.language == "en" && !host.showThroughWalls && !host.showPing && host.showNpcIds && host.alertVolume == 35,
           "hote relance : ses reglages d'affichage et son volume, pas ceux du client");
    Expect(host.teamKill && host.cheats && host.cheatAccess == 0x2, "hote relance : tir ami, cheats et acces du joueur 1 gardes");
    Expect(host.cheatGod && !host.cheatOneHit && host.cheatMoveSpeed == 150 && host.cheatJump == 100 &&
               host.character == "striker_easy_1" && host.characterKey == "F5",
           "hote relance : ses cheats et son personnage, pas ceux du client");
    Expect(host.playerName == "Hote", "hote relance : son pseudo");
    Expect(host.lowPortraits && !host.autoPlay, "hote relance : ses portraits en bas, jamais de jeu automatique");

    // 6. Relance du client : les siens.
    cfg::Config& again = Launch("connect:127.0.0.1");
    Expect(again.language == "fr" && again.showThroughWalls && !again.showNpcIds && !again.showPing && again.alertVolume == 60,
           "client relance : ses reglages, et pour le reste ceux de la section commune");
    Expect(again.playerName == "Client", "client relance : son pseudo");
    Expect(!again.lowPortraits && again.autoPlay, "client relance : ses portraits en haut, son jeu automatique");
    Expect(again.cheatOneHit && !again.cheatGod && again.cheatJump == 250 && again.cheatMoveSpeed == 100 &&
               again.character.empty() && again.characterKey == "F8",
           "client relance : ses cheats et sa touche, pas ceux de l'hote");

    // 7. Un volume hors bornes dans le fichier est ignoré.
    WritePrivateProfileStringA("audio", "alert_volume", "250", ini.c_str());
    Expect(Launch("host").alertVolume == 50, "volume hors bornes : valeur d'origine");
    WritePrivateProfileStringA("audio", "alert_volume", "0", ini.c_str());
    Expect(Launch("host").alertVolume == 0, "volume a zero garde : muet");

    DeleteFileA(ini.c_str());
    if (g_failures) {
        printf("%d echec(s)\n", g_failures);
        return 1;
    }
    printf("OK\n");
    return 0;
}
