// Test de la langue du mod, sans le jeu : le dictionnaire français / anglais (chaque format prend
// les mêmes arguments dans les deux langues, aucun texte n'y figure deux fois), la traduction d'un
// texte dans les deux sens, celle d'un format et des mots qu'on lui donne, et le changement de
// langue pris en compte à l'appel suivant.
#include <cstdio>
#include <cstring>

#include "../src/lang.h"

static int g_failures = 0;

static void Expect(bool condition, const char* what) {
    if (condition) return;
    g_failures++;
    printf("ECHEC : %s\n", what);
}

static bool Same(const char* a, const char* b) { return a && b && strcmp(a, b) == 0; }

int main() {
    char text[160];

    // 1. Le dictionnaire.
    Expect(loc::EntryCount() > 100, "plus de cent textes");
    const char* mismatch = loc::FirstMismatch();
    if (mismatch) printf("format dont les deux versions ne prennent pas les memes arguments : %s\n", mismatch);
    Expect(mismatch == nullptr, "les deux versions de chaque format prennent les memes arguments");
    Expect(loc::Collisions() == 0, "aucun texte en double, ni francais ici et anglais la");

    // 2. Français d'origine : les messages restent, les libellés du menu sont traduits.
    Expect(loc::Current() == loc::Language::French, "francais au depart");
    Expect(Same(loc::Tr("Display"), "Affichage") && Same(loc::Tr("Language"), "Langue"), "menu en francais");
    Expect(Same(loc::Tr("Alert volume"), "Volume alerte") && Same(loc::Tr("Wallhack (ESP)"), "Vision murs (ESP)"),
           "curseur du volume et bouton de la vue a travers les murs en francais");
    loc::Format(text, sizeof(text), "%s : %d %%", "Alert volume", 50);
    Expect(Same(text, "Volume alerte : 50 %"), "etiquette du volume en francais");
    Expect(Same(loc::Tr("A TERRE"), "A TERRE"), "un texte francais reste en francais");
    Expect(Same(loc::Tr("Coop"), "Coop") && Same(loc::Tr("ON"), "ON"), "un texte inconnu reste tel quel");
    const char* unknown = "rien de connu";
    Expect(loc::Tr(unknown) == unknown && loc::Tr(nullptr) == nullptr, "inconnu ou nul : rendu tel quel");
    loc::Format(text, sizeof(text), "%s : %s", "Difficulty", "Hard");
    Expect(Same(text, "Difficulte : Difficile"), "titre d'un bouton en francais");
    loc::Format(text, sizeof(text), "%s : +%d %% per client%s", "NPC damage", 200, " (off)");
    Expect(Same(text, "Degats des PNJ : +200 % / client (off)"), "etiquette d'un curseur en francais");
    loc::Format(text, sizeof(text), "OniMP : tir ami %s", "active");
    Expect(Same(text, "OniMP : tir ami active"), "message en francais");

    // 3. Anglais : dès l'appel suivant.
    loc::Set(loc::Language::English);
    Expect(Same(loc::Tr("Display"), "Display") && Same(loc::Tr("A TERRE"), "DOWN"), "anglais");
    Expect(Same(loc::Tr("CTRL POUR RANIMER"), "HOLD CTRL TO REVIVE"), "invite de reanimation en anglais");
    loc::Format(text, sizeof(text), "%s : %s", "Difficulty", "Hard");
    Expect(Same(text, "Difficulty : Hard"), "titre d'un bouton en anglais");
    loc::Format(text, sizeof(text), "OniMP : tir ami %s", "active");
    Expect(Same(text, "OniMP : team kill enabled"), "message et mot traduits");
    loc::Format(text, sizeof(text), "OniMP : ennemis x%d, minimum a %d joueurs", 3, 3);
    Expect(Same(text, "OniMP : enemies x3, minimum with 3 players"), "nombres a leur place");
    loc::Format(text, sizeof(text), "PNJ : vie x%u.%02u, degats x%u.%02u (%u client(s))", 2u, 5u, 1u, 10u, 1u);
    Expect(Same(text, "NPCs : health x2.05, damage x1.10 (1 client(s))"), "largeurs des nombres gardees");
    loc::Format(text, sizeof(text), "OniMP : difficulte %s : %d s pour ranimer, releve a %u %% de sante", "Hard", 10, 35u);
    Expect(Same(text, "OniMP : difficulty Hard : 10 s to revive, back up at 35 % health"), "pour cent ecrit tel quel");
    loc::Format(text, sizeof(text), "OniMP : pseudo '%s'", "Odolinsky");
    Expect(Same(text, "OniMP : name 'Odolinsky'"), "un pseudo n'est pas traduit");
    loc::Format(text, sizeof(text), "OniMP : %s %s", "joueurs a travers les murs", "masques");
    Expect(Same(text, "OniMP : players through walls hidden"), "vue a travers les murs annoncee en anglais");
    loc::Format(text, sizeof(text), "OniMP : volume de l'alerte %d %%", 35);
    Expect(Same(text, "OniMP : alert volume 35 %"), "volume annonce en anglais");
    loc::Format(text, sizeof(text), "OniMP : jeu automatique %s", "active");
    Expect(Same(text, "OniMP : auto-play enabled"), "jeu automatique annonce en anglais");
    loc::Format(text, sizeof(text), "OniMP : portraits des dialogues %s", "en bas");
    Expect(Same(text, "OniMP : dialogue portraits at the bottom"), "portraits annonces en anglais");
    loc::Format(text, sizeof(text), "OniMP : langue : francais");
    Expect(Same(text, "OniMP : language : English"), "annonce du changement dans la nouvelle langue");
    loc::Format(text, sizeof(text), "texte sans traduction %d", 7);
    Expect(Same(text, "texte sans traduction 7"), "format inconnu : tel quel");

    // 4. Jamais plus que la place donnée.
    char small[8];
    int length = loc::Format(small, sizeof(small), "OniMP : tir ami %s", "active");
    Expect(strlen(small) == sizeof(small) - 1 && length > static_cast<int>(sizeof(small)), "texte coupe a la place donnee");

    // 5. Retour au français, et ce que garde onimp.ini.
    loc::Set(loc::Language::French);
    Expect(Same(loc::Tr("DOWN"), "A TERRE") && Same(loc::Tr("Skin"), "Tenue"), "retour au francais");
    Expect(Same(loc::Key(loc::Language::English), "en") && Same(loc::Key(loc::Language::French), "fr"), "cles");
    Expect(loc::FromKey("en", loc::Language::French) == loc::Language::English &&
               loc::FromKey("FR", loc::Language::English) == loc::Language::French &&
               loc::FromKey("", loc::Language::English) == loc::Language::English &&
               loc::FromKey(nullptr, loc::Language::French) == loc::Language::French,
           "cle lue, ou valeur par defaut");

    if (g_failures) {
        printf("%d echec(s)\n", g_failures);
        return 1;
    }
    printf("langue : %d textes, tout est conforme\n", loc::EntryCount());
    return 0;
}
