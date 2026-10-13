// Test de la liste des mods de l'AE Installer sans le jeu : lecture d'installed_mods.xml,
// comparaison de deux listes, format réseau, noms des paquets.
// Avec un dossier de jeu en argument : affiche la liste que la DLL y lira, sans rien tester.
#include <cstdio>
#include <cstring>

#include "../src/modlist.h"

static int g_failures = 0;

static void Expect(bool condition, const char* what) {
    if (condition) return;
    g_failures++;
    printf("ECHEC : %s\n", what);
}

// Le fichier tel que l'AE Installer l'écrit.
static const char* const kInstalled =
    "<?xml version=\"1.0\" ?><MIL><mods><int>50</int><int>2000</int><int>2200</int><int>2400</int>"
    "<int>3000</int><int>4000</int><int>5000</int><int>5100</int><int>6000</int><int>34090</int></mods>"
    "<affectedLevels><string>level0_final</string><string>level8_final</string><string>.svn</string>"
    "<string>level4_final</string></affectedLevels></MIL>";

static int Show(const char* folder) {
    std::string gameDir = folder;
    if (!gameDir.empty() && gameDir.back() != '\\' && gameDir.back() != '/') gameDir += '\\';
    std::vector<uint32_t> ids;
    if (!mods::ReadInstalled(gameDir, ids)) {
        printf("mods AE : GameDataFolder\\installed_mods.xml introuvable dans %s\n", gameDir.c_str());
        return 1;
    }
    printf("mods AE : %d paquet(s), empreinte %08X : %s\n", static_cast<int>(ids.size()),
           static_cast<unsigned>(mods::Fingerprint(ids)),
           mods::Label(ids, mods::ReadPackageNames(gameDir), 0).c_str());
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1) return Show(argv[1]);

    // 1. Lecture : les dix paquets, triés ; les niveaux touchés ne sont pas pris pour des paquets.
    std::vector<uint32_t> ids = mods::ParseInstalled(kInstalled);
    Expect(ids.size() == 10 && ids.front() == 50 && ids.back() == 34090, "lecture : dix paquets, de 50 a 34090");

    std::vector<uint32_t> messy = mods::ParseInstalled(
        "<MIL><mods><int>34090</int><int>50</int><int>50</int><int>x</int><int>2000</int></mods>"
        "<other><int>7</int></other></MIL>");
    Expect(messy == std::vector<uint32_t>({50, 2000, 34090}), "lecture : tri, doublons, entier hors du bloc <mods>");
    Expect(mods::ParseInstalled("<MIL><affectedLevels></affectedLevels></MIL>").empty(), "lecture : pas de bloc <mods>");
    Expect(mods::ParseInstalled("<MIL><mods></mods></MIL>").empty(), "lecture : aucun paquet");

    // 2. Comparaison : ce que chacun a de plus que l'autre.
    std::vector<uint32_t> here = {50, 2000, 34090};
    std::vector<uint32_t> there = {50, 2000, 21501, 24501};
    Expect(mods::Missing(here, there) == std::vector<uint32_t>({34090}), "comparaison : paquet d'ici seulement");
    Expect(mods::Missing(there, here) == std::vector<uint32_t>({21501, 24501}), "comparaison : paquets de l'autre");
    Expect(mods::Missing(here, here).empty(), "comparaison : listes identiques");
    Expect(mods::Fingerprint(here) == mods::Fingerprint(mods::ParseInstalled(
               "<mods><int>34090</int><int>2000</int><int>50</int></mods>")),
           "empreinte : la meme quel que soit l'ordre du fichier");
    Expect(mods::Fingerprint(here) != mods::Fingerprint(there), "empreinte : differente pour deux listes");

    // 3. Format réseau : aller et retour, messages tronqués ou trop longs refusés.
    uint8_t message[2 + 4 * mods::kMaxListed];
    int size = mods::Encode(ids, message, sizeof(message));
    std::vector<uint32_t> decoded;
    Expect(size == 2 + 4 * 10 && mods::Decode(message, size, decoded) && decoded == ids, "reseau : aller et retour");
    Expect(!mods::Decode(message, size - 1, decoded), "reseau : message tronque refuse");
    Expect(!mods::Decode(message, 1, decoded), "reseau : message sans nombre refuse");
    Expect(mods::Encode(ids, message, 2 + 4 * 9) == 0, "reseau : tampon trop court");
    std::vector<uint32_t> many(mods::kMaxListed + 1, 1);
    Expect(mods::Encode(many, message, sizeof(message)) == 0, "reseau : liste trop longue non envoyee");
    uint16_t tooMany = mods::kMaxListed + 1;
    memcpy(message, &tooMany, 2);
    Expect(!mods::Decode(message, sizeof(message), decoded), "reseau : nombre trop grand refuse");
    size = mods::Encode({}, message, sizeof(message));
    Expect(size == 2 && mods::Decode(message, size, decoded) && decoded.empty(), "reseau : liste vide");

    // 4. Noms : dossiers d'AEInstaller/packages.
    uint32_t id = 0;
    std::string name;
    Expect(mods::SplitPackageFolder("34090Blood", id, name) && id == 34090 && name == "Blood", "nom : 34090Blood");
    Expect(mods::SplitPackageFolder("24501Better_Warehouse___Training", id, name) && id == 24501 &&
               name == "Better Warehouse Training",
           "nom : soulignes repetes");
    Expect(mods::SplitPackageFolder("00050Menu_Fixes__AE_", id, name) && id == 50 && name == "Menu Fixes AE",
           "nom : zeros en tete, souligne final");
    Expect(!mods::SplitPackageFolder("vanilla", id, name) && !mods::SplitPackageFolder("1234", id, name),
           "nom : dossier qui n'est pas un paquet");

    std::map<uint32_t, std::string> names = {{34090, "Blood"}, {21501, "Brutal AI"}};
    Expect(mods::Label({21501, 34090, 99999}, names, 0) == "21501 Brutal AI, 34090 Blood, 99999", "libelle : complet");
    Expect(mods::Label({50, 2000, 21501, 34090}, names, 3) == "50, 2000, 21501 Brutal AI (+1)", "libelle : abrege");
    Expect(mods::Label({}, names, 0) == "aucun", "libelle : liste vide");

    printf("%s\n", g_failures == 0 ? "OK" : "ECHEC");
    return g_failures == 0 ? 0 : 1;
}
