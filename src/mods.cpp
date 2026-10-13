#include "mods.h"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "config.h"
#include "log.h"
#include "memory.h"
#include "modlist.h"
#include "network.h"
#include "protocol.h"

namespace mods {

enum class Peer { Unknown, Same, Different };

// La console n'a qu'une ligne par message : au-delà, le détail est dans le journal.
static const int kConsoleListed = 3;

static bool g_known = false;                     // installed_mods.xml trouvé
static std::vector<uint32_t> g_installed;
static std::map<uint32_t, std::string> g_names;
// Par numéro de joueur. L'hôte se compare à chaque client, un client à l'hôte seulement : deux
// clients qui ont chacun les paquets de l'hôte ont les mêmes.
static Peer g_peer[proto::kMaxPlayers] = {};
static bool g_noticed[proto::kMaxPlayers] = {};                  // un écart a déjà été dit au joueur...
static std::vector<uint32_t> g_noticedList[proto::kMaxPlayers];  // ... pour cette liste de l'autre machine

void Load() {
    const std::string& gameDir = cfg::GameDir();
    g_known = ReadInstalled(gameDir, g_installed);
    if (!g_known) {
        LOG("mods AE : GameDataFolder\\installed_mods.xml introuvable dans %s : liste inconnue, pas de "
            "comparaison avec les autres joueurs", gameDir.c_str());
        return;
    }
    g_names = ReadPackageNames(gameDir);
    LOG("mods AE : %d paquet(s), empreinte %08X : %s", static_cast<int>(g_installed.size()),
        static_cast<unsigned>(Fingerprint(g_installed)), Label(g_installed, g_names, 0).c_str());
}

void OnConnection(uint8_t player, bool connected) {
    if (player >= proto::kMaxPlayers) return;
    g_peer[player] = Peer::Unknown;
    if (!connected || !g_known) return;
    uint8_t message[1 + 2 + 4 * kMaxListed];
    message[0] = proto::kReliableMods;
    int size = Encode(g_installed, message + 1, static_cast<int>(sizeof(message)) - 1);
    if (size > 0) net::SendReliableTo(player, message, 1 + size);
}

void OnReliable(uint8_t from, const uint8_t* data, int size) {
    std::vector<uint32_t> theirs;
    if (from >= proto::kMaxPlayers || !Decode(data, size, theirs)) return;
    if (!g_known) {
        LOG("mods AE : le joueur %u a %d paquet(s), rien a leur comparer ici", from, static_cast<int>(theirs.size()));
        return;
    }

    std::vector<uint32_t> onlyHere = Missing(g_installed, theirs);
    std::vector<uint32_t> onlyThere = Missing(theirs, g_installed);
    if (onlyHere.empty() && onlyThere.empty()) {
        g_peer[from] = Peer::Same;
        LOG("mods AE : memes paquets chez le joueur %u (%d, empreinte %08X)", from, static_cast<int>(theirs.size()),
            static_cast<unsigned>(Fingerprint(theirs)));
        return;
    }

    g_peer[from] = Peer::Different;
    LOG("mods AE : paquets differents chez le joueur %u (empreinte %08X ici, %08X chez lui) ; ici seulement : "
        "%s ; chez lui seulement : %s", from, static_cast<unsigned>(Fingerprint(g_installed)),
        static_cast<unsigned>(Fingerprint(theirs)), Label(onlyHere, g_names, 0).c_str(),
        Label(onlyThere, g_names, 0).c_str());

    // Chaque rechargement de niveau rouvre la connexion : l'écart n'est redit à la console que
    // si la liste de ce joueur a changé.
    if (g_noticed[from] && theirs == g_noticedList[from]) return;
    g_noticed[from] = true;
    g_noticedList[from] = theirs;
    mem::ConsolePrint("OniMP : mods AE differents de ceux du joueur %u", from);
    if (!onlyHere.empty()) {
        mem::ConsolePrint("OniMP : ici seulement : %s", Label(onlyHere, g_names, kConsoleListed).c_str());
    }
    if (!onlyThere.empty()) {
        mem::ConsolePrint("OniMP : chez lui seulement : %s", Label(onlyThere, g_names, kConsoleListed).c_str());
    }
}

void Describe(char* out, int capacity) {
    if (!g_known) {
        snprintf(out, static_cast<size_t>(capacity), "mods AE : liste inconnue ici");
        return;
    }
    std::string others;
    for (int player = 0; player < proto::kMaxPlayers; player++) {
        if (g_peer[player] == Peer::Unknown) continue;
        others += std::string(others.empty() ? "" : ", ") + "joueur " + std::to_string(player) +
                  (g_peer[player] == Peer::Same ? " memes paquets" : " paquets differents (voir le journal)");
    }
    snprintf(out, static_cast<size_t>(capacity), "mods AE : %d paquet(s), empreinte %08X ; %s",
             static_cast<int>(g_installed.size()), static_cast<unsigned>(Fingerprint(g_installed)),
             others.empty() ? "pas compare" : others.c_str());
}

}  // namespace mods
