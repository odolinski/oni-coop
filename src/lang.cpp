#include "lang.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

namespace loc {

namespace {

struct Entry {
    const char* french;
    const char* english;
};

// Les deux versions d'un format prennent les mêmes arguments, dans le même ordre (FirstMismatch le
// contrôle). Sans accents : la police du jeu ne les a pas tous.
const Entry kEntries[] = {
    // --- Menu F1 (libellés nés en anglais) ---
    {"Affichage", "Display"},
    {"Difficulte", "Difficulty"},
    {"Ennemis", "Enemies"},
    {"Civils", "Civilians"},
    {"Tir ami", "Team kill"},
    {"Invincibilite", "God mode"},
    {"Degats en ligne", "Online damage"},
    {"Munitions infinies", "Unlimited ammo"},
    {"Vie des PNJ", "NPC health"},
    {"Degats des PNJ", "NPC damage"},
    {"Vie des joueurs", "Player health"},
    {"Noms des PNJ", "NPC IDs"},
    {"Noms des armes", "Weapon IDs"},
    {"Munitions joueurs", "Player ammo"},
    {"Latence", "Ping"},
    {"Tenue", "Skin"},
    {"Pseudo", "Name"},
    {"Langue", "Language"},
    {"Vision murs (ESP)", "Wallhack (ESP)"},
    {"Volume alerte", "Alert volume"},
    {"Jeu auto", "Auto-play"},
    {"Portraits en bas", "Low portraits"},
    {"Chapitre", "Chapter outfit"},
    {"Facile", "Easy"},
    {"Difficile", "Hard"},
    {"Survie", "Survival"},
    {"%s : +%d %% / client%s", "%s : +%d %% per client%s"},
    {"x%d (bloque)", "x%d (locked)"},

    // --- Onglets Cheat, Character et Spawn (0.35) ---
    {"Personnage", "Character"},
    {"Apparition", "Spawn"},
    {"Coup fatal", "One-hit kill"},
    {"Aura permanente", "Permanent aura"},
    {"Vitesse attaque", "Attack speed"},
    {"Vitesse course", "Move speed"},
    {"Saut", "Jump"},
    {"Filtre", "Filter"},
    {"Touche", "Hotkey"},
    {"Genre", "Kind"},
    {"Precedent", "Previous"},
    {"Suivant (%d/%d)", "Next (%d/%d)"},
    {"autorise", "allowed"},
    {"bloque", "blocked"},
    {"Tous", "All"},
    {"Syndicat", "Syndicate"},
    {"Boss", "Bosses"},
    {"Autres", "Other"},
    {"Objets", "Items"},
    {"Armes", "Weapons"},  // « PNJ » / « NPCs » : déjà plus bas, avec les messages
    {"Nombre : x%d", "Count : x%d"},
    {"Cartouches : %d %%", "Ammo : %d %%"},
    {"Camp : %s", "Team : %s"},
    {"Allie", "Ally"},
    {"Ennemi", "Enemy"},
    {"Hypo", "Hypo spray"},
    {"Chargeur", "Ammo clip"},
    {"Cellule d'energie", "Energy cell"},
    {"Bouclier de force", "Force shield"},
    {"Camouflage de phase", "Phase cloak"},
    {"OniMP : cheats actives par l'hote", "OniMP : cheats switched on by the host"},
    {"OniMP : cheats coupes par l'hote", "OniMP : cheats switched off by the host"},
    {"OniMP : joueur %u autorise aux cheats", "OniMP : player %u may now use cheats"},
    {"OniMP : joueur %u interdit de cheats", "OniMP : player %u may no longer use cheats"},
    {"OniMP : l'hote vous ouvre les onglets Cheat, Character et Spawn",
     "OniMP : the host gives you the Cheat, Character and Spawn tabs"},
    {"OniMP : l'hote vous retire les onglets Cheat, Character et Spawn",
     "OniMP : the host takes back the Cheat, Character and Spawn tabs"},
    {"OniMP : sans effet tant que l'hote laisse les cheats coupes",
     "OniMP : no effect while the host keeps cheats switched off"},
    {"OniMP : coup fatal %s", "OniMP : one-hit kill %s"},
    {"OniMP : aura permanente %s", "OniMP : permanent aura %s"},
    {"OniMP : personnage %s", "OniMP : character %s"},
    {"OniMP : personnage : acces non donne par l'hote", "OniMP : character : access not given by the host"},
    {"OniMP : personnage inconnu dans ce niveau", "OniMP : unknown character in this level"},
    {"OniMP : apparition : %s", "OniMP : spawn : %s"},
    {"OniMP : apparition refusee (%d PNJ deja la)", "OniMP : spawn refused (%d NPCs already there)"},
    {"OniMP : attaque %d %%, deplacement %d %%, saut %d %%", "OniMP : attack %d %%, movement %d %%, jump %d %%"},
    {"OniMP : cheats %s, acces 0x%X", "OniMP : cheats %s, access 0x%X"},

    // --- Lasers, alarmes, niveau de l'hôte (0.36) ---
    {"OniMP : laser %u : %s detecte", "OniMP : laser %u : %s detected"},
    {"OniMP : alarme : %d ennemi(s) en renfort (%d joueur(s))", "OniMP : alarm : %d enemy reinforcement(s) (%d player(s))"},
    {"OniMP : l'hote a charge un autre niveau : nous le suivons", "OniMP : the host loaded another level : following"},

    // --- Au-dessus d'un joueur à terre ---
    {"A TERRE", "DOWN"},
    {"CTRL POUR RANIMER", "HOLD CTRL TO REVIVE"},
    {"REANIMATION", "REVIVING"},

    // --- Réglages de l'hôte ---
    {"OniMP : difficulte relue : vie %u / %u / %u, degats %u / %u / %u pour cent",
     "OniMP : difficulty reloaded : health %u / %u / %u, damage %u / %u / %u percent"},
    {"OniMP : ennemis x%d, minimum a %d joueurs", "OniMP : enemies x%d, minimum with %d players"},
    {"OniMP : ennemis x%d", "OniMP : enemies x%d"},
    {"OniMP : ennemis x%d, verrouille a %d joueurs", "OniMP : enemies x%d, locked with %d players"},
    {"OniMP : ennemis x%d (%d joueur(s))", "OniMP : enemies x%d (%d player(s))"},
    {"OniMP : tir ami %s", "OniMP : team kill %s"},
    {"active", "enabled"},
    {"desactive", "disabled"},
    {"OniMP : invincibilite %s", "OniMP : god mode %s"},
    {"activee", "switched on"},
    {"desactivee", "switched off"},
    {"OniMP : reglage reserve a l'hote", "OniMP : host-only setting"},
    {"OniMP : munitions illimitees %s", "OniMP : unlimited ammo %s"},
    {"activees", "turned on"},
    {"desactivees", "turned off"},
    {"OniMP : difficulte %s : %d s pour ranimer, releve a %u %% de sante",
     "OniMP : difficulty %s : %d s to revive, back up at %u %% health"},
    {"OniMP : difficulte %s (0 facile, 1 normal, 2 difficile, 3 survie)",
     "OniMP : difficulty %s (0 easy, 1 normal, 2 hard, 3 survival)"},
    {"OniMP : civils %s", "OniMP : civilians %s"},
    {"presents : en tuer un fait echouer la mission", "present : killing one fails the mission"},
    {"absents", "absent"},
    {"presents", "present"},
    {"OniMP : civils presents : en tuer un fait echouer la mission",
     "OniMP : civilians present : killing one fails the mission"},
    {"OniMP : degats des PNJ %s", "OniMP : NPC damage %s"},
    {"en ligne, plus forts a chaque client", "online, stronger with each client"},
    {"normaux", "normal"},
    {"PNJ : vie et degats d'origine", "NPCs : original health and damage"},
    {"PNJ : vie x%u.%02u, degats x%u.%02u (%u client(s))", "NPCs : health x%u.%02u, damage x%u.%02u (%u client(s))"},
    {"PNJ : vie x%u.%02u, degats normaux, Online damage sur OFF (%u client(s))",
     "NPCs : health x%u.%02u, normal damage, Online damage is OFF (%u client(s))"},

    // --- Affichage ---
    {"OniMP : %s %s", "OniMP : %s %s"},
    {"affiches", "shown"},
    {"masques", "hidden"},
    {"affichee", "displayed"},
    {"masquee", "not displayed"},
    {"affichees", "visible"},
    {"masquees", "not visible"},
    {"points de vie des joueurs", "player health bars"},
    {"points de vie des PNJ", "NPC health bars"},
    {"identifiants des PNJ", "NPC names"},
    {"identifiants des armes", "weapon names"},
    {"munitions des joueurs", "player ammo"},
    {"latences des joueurs", "player pings"},
    {"joueurs a travers les murs", "players through walls"},
    {"OniMP : messages masques (le journal les garde)", "OniMP : messages hidden (still written to the log)"},
    {"OniMP : messages affiches", "OniMP : messages shown"},
    {"OniMP : messages %s", "OniMP : messages %s"},
    {"masques (le journal les garde)", "hidden (still written to the log)"},
    {"OniMP : sante des joueurs %s", "OniMP : player health %s"},
    {"OniMP : sante des PNJ %s", "OniMP : NPC health %s"},
    {"OniMP : identifiants des PNJ %s", "OniMP : NPC names %s"},
    {"OniMP : identifiants des armes %s", "OniMP : weapon names %s"},
    {"OniMP : latences des joueurs %s", "OniMP : player pings %s"},
    {"OniMP : munitions des joueurs %s", "OniMP : player ammo %s"},
    {"OniMP : pseudo '%s'", "OniMP : name '%s'"},
    {"OniMP : pseudo %s", "OniMP : name %s"},
    {"retire", "removed"},
    {"OniMP : tenue %s (mp_skin 0 a %d pour en changer)", "OniMP : skin %s (mp_skin 0 to %d to change it)"},
    {"OniMP : tenue %s deja prise par un autre joueur", "OniMP : skin %s is already worn by another player"},
    {"OniMP : langue : francais", "OniMP : language : English"},
    {"OniMP : volume de l'alerte %d %%", "OniMP : alert volume %d %%"},
    {"OniMP : jeu automatique %s", "OniMP : auto-play %s"},
    {"OniMP : jeu automatique reserve aux clients", "OniMP : auto-play is for clients only"},
    {"OniMP : portraits des dialogues %s", "OniMP : dialogue portraits %s"},
    {"en bas", "at the bottom"},
    {"en haut", "at the top"},
    {"OniMP : langue %s (mp_language \"fr\" ou \"en\")", "OniMP : language %s (mp_language \"fr\" or \"en\")"},

    // --- Synchronisation ---
    {"OniMP : desynchronisation persistante du joueur %u (%s)", "OniMP : player %u is still out of sync (%s)"},
    {"OniMP : desynchronisation du joueur %u (%s), etat renvoye", "OniMP : player %u out of sync (%s), state sent again"},
    {"OniMP : resynchronisation par l'hote (%s)", "OniMP : resynchronised by the host (%s)"},
    {"OniMP : mods AE differents de ceux du joueur %u", "OniMP : AE mods differ from player %u's"},
    {"OniMP : ici seulement : %s", "OniMP : only here : %s"},
    {"OniMP : chez lui seulement : %s", "OniMP : only there : %s"},
    {"OniMP : le joueur %u n'est pas dans le meme niveau", "OniMP : player %u is not in the same level"},
    {"OniMP : joueur %d apparu", "OniMP : player %d appeared"},
    {"OniMP : synchronisation des PNJ %s", "OniMP : NPC synchronisation %s"},
    {"niveau", "level"},
    {"PNJ", "NPCs"},
    {"objets au sol", "ground items"},
    {"reglages", "settings"},

    // --- Civils ---
    {"OniMP : un civil vous donne une arme (%s)", "OniMP : a civilian gives you a weapon (%s)"},
    {"OniMP : un civil vous donne %u %s%s", "OniMP : a civilian gives you %u %s%s"},
    {"OniMP : un civil vous donne : %s%s", "OniMP : a civilian gives you : %s%s"},
    {"chargeur(s) de balles", "ballistic clip(s)"},
    {"cellule(s) d'energie", "energy cell(s)"},
    {"hypo(s)", "hypo(s)"},
    {"bouclier de force", "force shield"},
    {"invisibilite", "phase cloak"},
    {"objet du niveau", "level item"},
    {" (vous n'avez plus de place)", " (you have no room left)"},
    {"OniMP : le joueur %u a recu l'objet du niveau", "OniMP : player %u received the level item"},
    {"OniMP : le joueur %u a ramasse l'objet du niveau", "OniMP : player %u picked up the level item"},

    // --- Niveau, reprise ---
    {"OniMP : tous les joueurs ont charge : c'est parti", "OniMP : all players have loaded : go"},
    {"OniMP : attente terminee (%s) : c'est parti", "OniMP : done waiting (%s) : go"},
    {"OniMP : en attente de %d joueur(s) qui charge(nt) le niveau...", "OniMP : waiting for %d player(s) loading the level..."},
    {"OniMP : pas de depart donne par l'hote : la partie reprend", "OniMP : no start signal from the host : the game resumes"},
    {"OniMP : en attente du depart donne par l'hote...", "OniMP : waiting for the host's start signal..."},
    {"OniMP : le joueur %u n'a pas ete ranime a temps : mission echouee", "OniMP : player %u was not revived in time : mission failed"},
    {"OniMP : un civil a ete tue par le joueur %u : mission echouee", "OniMP : a civilian was killed by player %u : mission failed"},
    {"OniMP : le joueur %u arrive : reprise au dernier point de sauvegarde dans %d s",
     "OniMP : player %u is joining : restarting from the last save point in %d s"},
    {"OniMP : le joueur %u est mort, reprise a la derniere sauvegarde", "OniMP : player %u died, restarting from the last save"},
    {"OniMP : point de sauvegarde %d atteint, garde par l'hote", "OniMP : save point %d reached, kept by the host"},
    {"OniMP : niveau charge : en attente de %d joueur(s)...", "OniMP : level loaded : waiting for %d player(s)..."},
    {"OniMP : niveau charge : en attente du depart donne par l'hote...", "OniMP : level loaded : waiting for the host's start signal..."},
    {"OniMP : reprise a la derniere sauvegarde de l'hote", "OniMP : restarting from the host's last save"},
    {"OniMP : vous arrivez dans la partie sans armes", "OniMP : you join the game without weapons"},
    {"OniMP : reprise dans %d s", "OniMP : restarting in %d s"},
    {"tous les joueurs ont charge", "all players have loaded"},
    {"plus personne a attendre", "nobody left to wait for"},
    {"delai depasse", "timed out"},

    // --- Session ---
    {"OniMP : session arretee", "OniMP : session closed"},
    {"OniMP : echec du demarrage reseau (voir le journal)", "OniMP : network start failed (see the log)"},
    {"OniMP : hote sur le port %u, partie '%s', %d joueurs au plus", "OniMP : hosting on port %u, game '%s', %d players at most"},
    {"OniMP : recherche d'un hote sur le reseau", "OniMP : looking for a host on the network"},
    {"OniMP : connexion a %s:%u", "OniMP : connecting to %s:%u"},
    {"OniMP : joueur %u %s (%d sur %d)", "OniMP : player %u %s (%d of %d)"},
    {"connecte", "connected"},
    {"deconnecte", "disconnected"},
    {"OniMP : connecte a l'hote, vous etes le joueur %u", "OniMP : connected to the host, you are player %u"},
    {"OniMP : hote perdu", "OniMP : host lost"},
    {"OniMP : adresse invalide (exemple : mp_connect \"192.168.1.10:27777\")",
     "OniMP : invalid address (example : mp_connect \"192.168.1.10:27777\")"},
    {"Radmin VPN connecte", "Radmin VPN connected"},
    {"Radmin VPN deconnecte", "Radmin VPN disconnected"},
    {"refuse par l'hote : la partie est complete", "refused by the host : the game is full"},
    {"refuse par l'hote : version differente (protocole %u chez lui, %u ici)",
     "refused by the host : different version (protocol %u there, %u here)"},
    {"partie '%s' trouvee a %s, mais complete (%u joueurs)", "game '%s' found at %s, but full (%u players)"},
    {"partie '%s' trouvee a %s, mais en OniMP %s (protocole %u, ici %u)",
     "game '%s' found at %s, but running OniMP %s (protocol %u, %u here)"},
    {"partie '%s' trouvee a %s:%u (niveau '%s', %u joueur(s) sur %u) : connexion",
     "game '%s' found at %s:%u (level '%s', %u of %u player(s)) : connecting"},
    {"l'hote ne repond plus : nouvelle recherche", "the host no longer answers : searching again"},

    // --- Joueur à terre ---
    {"OniMP : vous etes a terre : %d s pour qu'un coequipier vous ranime", "OniMP : you are down : %d s for a teammate to revive you"},
    {"OniMP : ranime par le joueur %u : %u points de vie, une touche de deplacement pour vous relever",
     "OniMP : revived by player %u : %u health, press a movement key to get up"},
    {"OniMP : le joueur %u vous ranime", "OniMP : player %u is reviving you"},
    {"OniMP : reanimation interrompue : il vous reste %d s", "OniMP : revive interrupted : %d s left"},
    {"OniMP : reanimation du joueur %d interrompue%s", "OniMP : reviving player %d interrupted%s"},
    {" par un coup", " by a hit"},
    {"OniMP : reanimation du joueur %d : gardez CTRL %d s", "OniMP : reviving player %d : hold CTRL for %d s"},
    {"OniMP : joueur %d ranime", "OniMP : player %d revived"},
    {"OniMP : JOUEUR %u A TERRE : %d s pour le ranimer, CTRL maintenu %d s pres de lui",
     "OniMP : PLAYER %u IS DOWN : %d s to revive, hold CTRL %d s next to them"},
    {"OniMP : joueur %u a terre : %d s", "OniMP : player %u down : %d s"},
};

Language g_language = Language::French;
std::unordered_map<std::string, const char*> g_toEnglish, g_toFrench;
bool g_built = false;

void Build() {
    if (g_built) return;
    g_built = true;
    for (const Entry& entry : kEntries) {
        g_toEnglish[entry.french] = entry.english;
        g_toFrench[entry.english] = entry.french;
    }
}

// Suite des conversions d'un format : « sdu » pour « %s ... %d ... %u ». Les longueurs comptent.
std::string Signature(const char* format) {
    std::string signature;
    for (const char* c = format; *c; c++) {
        if (*c != '%') continue;
        c++;
        if (*c == '%') continue;
        while (*c && strchr("-+ #0123456789.", *c)) c++;
        while (*c && strchr("hlzjt", *c)) signature.push_back(*c++);
        if (!*c) break;
        signature.push_back(*c);
    }
    return signature;
}

void Append(char* out, size_t size, size_t& length, const char* text, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (length + 1 < size) out[length] = text[i];
        length++;
    }
}

}  // namespace

Language Current() { return g_language; }
void Set(Language language) { g_language = language; }
const char* Label(Language language) { return language == Language::English ? "English" : "Francais"; }
const char* Key(Language language) { return language == Language::English ? "en" : "fr"; }

Language FromKey(const char* key, Language fallback) {
    if (!key) return fallback;
    if ((key[0] == 'e' || key[0] == 'E') && (key[1] == 'n' || key[1] == 'N')) return Language::English;
    if ((key[0] == 'f' || key[0] == 'F') && (key[1] == 'r' || key[1] == 'R')) return Language::French;
    return fallback;
}

const char* Tr(const char* text) {
    if (!text || !*text) return text;
    Build();
    const auto& table = g_language == Language::English ? g_toEnglish : g_toFrench;
    auto found = table.find(text);
    return found == table.end() ? text : found->second;
}

int FormatV(char* out, size_t size, const char* format, va_list args) {
    const char* translated = Tr(format);
    // Un format dont les deux versions ne s'accordent pas lirait de travers ses arguments : la
    // version d'origine vaut mieux qu'une faute.
    if (translated != format && Signature(translated) != Signature(format)) translated = format;

    size_t length = 0;
    for (const char* c = translated; *c;) {
        if (*c != '%') {
            Append(out, size, length, c, 1);
            c++;
            continue;
        }
        const char* start = c++;
        if (*c == '%') {
            Append(out, size, length, "%", 1);
            c++;
            continue;
        }
        while (*c && strchr("-+ #0123456789.", *c)) c++;
        int longs = 0;
        while (*c && strchr("hlzjt", *c)) {
            if (*c == 'l') longs++;
            c++;
        }
        if (!*c) break;
        char spec[32];
        size_t specLength = static_cast<size_t>(c - start) + 1;
        if (specLength >= sizeof(spec)) specLength = sizeof(spec) - 1;
        memcpy(spec, start, specLength);
        spec[specLength] = '\0';
        char piece[320];
        int written = 0;
        switch (*c) {
            case 's': {
                const char* text = va_arg(args, const char*);
                written = snprintf(piece, sizeof(piece), spec, text ? Tr(text) : "");
                break;
            }
            case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
                written = snprintf(piece, sizeof(piece), spec, va_arg(args, double));
                break;
            case 'p':
                written = snprintf(piece, sizeof(piece), spec, va_arg(args, void*));
                break;
            default:  // d, i, u, x, X, c, o
                if (longs >= 2) written = snprintf(piece, sizeof(piece), spec, va_arg(args, long long));
                else written = snprintf(piece, sizeof(piece), spec, va_arg(args, int));
                break;
        }
        c++;
        if (written < 0) written = 0;
        if (written >= static_cast<int>(sizeof(piece))) written = static_cast<int>(sizeof(piece)) - 1;
        Append(out, size, length, piece, static_cast<size_t>(written));
    }
    if (size) out[length < size ? length : size - 1] = '\0';
    return static_cast<int>(length);
}

int Format(char* out, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int length = FormatV(out, size, format, args);
    va_end(args);
    return length;
}

int EntryCount() { return static_cast<int>(sizeof(kEntries) / sizeof(kEntries[0])); }

int Collisions() {
    Build();
    int count = EntryCount();
    int collisions = (count - static_cast<int>(g_toEnglish.size())) + (count - static_cast<int>(g_toFrench.size()));
    // Un texte français dans une entrée et anglais dans une autre se traduirait dans les deux langues.
    for (const Entry& entry : kEntries) {
        if (strcmp(entry.french, entry.english) == 0) continue;
        if (g_toFrench.count(entry.french) || g_toEnglish.count(entry.english)) collisions++;
    }
    return collisions;
}

const char* FirstMismatch() {
    for (const Entry& entry : kEntries) {
        if (Signature(entry.french) != Signature(entry.english)) return entry.french;
    }
    return nullptr;
}

}  // namespace loc
