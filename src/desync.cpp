#include "desync.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "cheat.h"
#include "config.h"
#include "coop.h"
#include "desynccheck.h"
#include "lang.h"
#include "levelsync.h"
#include "log.h"
#include "memory.h"
#include "network.h"
#include "protocol.h"
#include "replication.h"
#include "scriptsync.h"
#include "world.h"

namespace desync {

static const double kDigestInterval = 1.0;

static const char* const kPartNames[proto::kDigestParts] = {
    "niveau", "PNJ", "consoles", "objets au sol", "reglages", "scripts",
};

// À son arrivée un client n'a pas encore l'état de l'hôte : PNJ en trop à retirer, manquants à
// créer, sol à aligner. Ses premiers rapports ne sont pas comparés.
static const int kGraceReports = 10;

static double g_nextDigest = 0.0;               // client
static Checker g_checker[proto::kMaxPlayers];   // hôte : un par client, par numéro de joueur
static int g_grace[proto::kMaxPlayers] = {};
static unsigned g_checks = 0;
static unsigned g_resyncs = 0;

// Les parties que les options de cette machine ne partagent pas valent zéro : deux machines aux
// options différentes se voient ainsi par leurs réglages, sans fausse alerte ailleurs. player :
// chez l'hôte, le client auquel ces empreintes seront comparées ; chez un client, l'hôte.
static proto::Digest Build(uint8_t player) {
    const cfg::Config& config = cfg::Get();
    proto::Digest digest = {};
    digest.levelHash = mem::LevelHash();
    digest.parts[proto::kDigestLevel] = level::Instance();
    if (config.npcs) {
        digest.parts[proto::kDigestNpcs] = rep::NpcDigest();
        digest.parts[proto::kDigestGround] = world::GroundDigest();
    }
    if (config.quests) {
        digest.parts[proto::kDigestConsoles] = world::ConsoleDigest();
        digest.parts[proto::kDigestScripts] = script::StreamCount(player);
    }
    digest.parts[proto::kDigestSettings] = (coop::TeamKill() ? 1u : 0u) | (cheat::Master() ? 2u : 0u) |
                                           (config.npcs ? 4u : 0u) | (config.quests ? 8u : 0u) |
                                           (coop::OnlineDamage() ? 16u : 0u) | (coop::Civilians() ? 32u : 0u) |
                                           (static_cast<uint32_t>(coop::Difficulty()) << 6) |
                                           ((coop::NpcHealthPercent() & 0xFFFFu) << 8) |
                                           (static_cast<uint32_t>(coop::EnemyMultiplier()) << 24) |
                                           (static_cast<uint32_t>(cheat::AccessMask() & 0xFu) << 28);
    return digest;
}

void OnConnection(uint8_t player, bool) {
    g_nextDigest = 0.0;
    if (player >= proto::kMaxPlayers) return;
    g_checker[player].Reset();
    g_grace[player] = kGraceReports;
}

// shown : pour l'écran, dans la langue du joueur ; sinon pour le journal, en français.
static std::string Names(uint32_t parts, bool shown = false) {
    std::string names;
    for (int part = 0; part < proto::kDigestParts; part++) {
        if (!(parts & (1u << part))) continue;
        names += std::string(names.empty() ? "" : ", ") + (shown ? loc::Tr(kPartNames[part]) : kPartNames[part]);
    }
    return names;
}

// Hôte : renvoie à ce client ce dont il décide, pour les parties données. Ce qui repart vers
// tous les clients est sans effet chez ceux qui l'ont déjà.
static void Repair(uint8_t player, uint32_t parts) {
    proto::Resync resync = {parts};
    uint8_t message[1 + sizeof(resync)];
    message[0] = proto::kReliableResync;
    memcpy(message + 1, &resync, sizeof(resync));
    net::SendReliableTo(player, message, sizeof(message));  // avant les données : le client sait pourquoi elles arrivent

    if (parts & (1u << proto::kDigestLevel)) level::Resend(player);
    if (parts & (1u << proto::kDigestNpcs)) rep::ResendNames();
    if (parts & (1u << proto::kDigestConsoles)) world::ResendConsoles();
    if (parts & (1u << proto::kDigestGround)) world::ResendGround();
    if (parts & (1u << proto::kDigestSettings)) coop::Resend();
}

void OnDigest(uint8_t from, const uint8_t* data, int size, double now) {
    proto::Digest theirs;
    if (net::CurrentMode() != cfg::Mode::Host || size < static_cast<int>(sizeof(theirs)) || !mem::Player()) return;
    if (from < 1 || from >= proto::kMaxPlayers) return;
    memcpy(&theirs, data, sizeof(theirs));
    proto::Digest ours = Build(from);
    g_checks++;
    if (g_grace[from] > 0) {
        g_grace[from]--;
        return;
    }

    Checker& checker = g_checker[from];
    Verdict verdict = checker.Compare(ours, theirs, now);
    for (int part = 0; part < proto::kDigestParts; part++) {
        unsigned here = static_cast<unsigned>(ours.parts[part]), there = static_cast<unsigned>(theirs.parts[part]);
        if (verdict.repair & (1u << part)) {
            LOG("desync : %s differe chez le client %u (%08X ici, %08X chez lui), renvoi %d/%d", kPartNames[part], from,
                here, there, checker.tries(part), kMaxTries);
        }
        if (verdict.persistent & (1u << part)) {
            LOG("desync : %s differe toujours chez le client %u (%08X ici, %08X chez lui), plus de renvoi",
                kPartNames[part], from, here, there);
        }
    }
    if (verdict.persistent) {
        mem::ConsolePrint("OniMP : desynchronisation persistante du joueur %u (%s)", from,
                          Names(verdict.persistent, true).c_str());
    }
    if (!verdict.repair) return;
    g_resyncs++;
    mem::ConsolePrint("OniMP : desynchronisation du joueur %u (%s), etat renvoye", from,
                      Names(verdict.repair, true).c_str());
    Repair(from, verdict.repair);
}

void OnResync(uint8_t from, const uint8_t* data, int size) {
    proto::Resync resync;
    if (net::CurrentMode() != cfg::Mode::Client || from != proto::kHostPlayerId) return;
    if (size < static_cast<int>(sizeof(resync))) return;
    memcpy(&resync, data, sizeof(resync));
    g_resyncs++;
    LOG("desync : l'hote renvoie son etat (%s)", Names(resync.parts).c_str());
    mem::ConsolePrint("OniMP : resynchronisation par l'hote (%s)", Names(resync.parts, true).c_str());
    // Un PNJ que le niveau a refusé de créer est redemandé.
    if (resync.parts & (1u << proto::kDigestNpcs)) rep::RetrySpawns();
}

void Update(double now) {
    if (net::CurrentMode() != cfg::Mode::Client || !net::Connected() || !mem::Player()) return;
    if (now < g_nextDigest) return;
    g_nextDigest = now + kDigestInterval;

    proto::Digest digest = Build(proto::kHostPlayerId);
    uint8_t message[1 + sizeof(digest)];
    message[0] = proto::kReliableDigest;
    memcpy(message + 1, &digest, sizeof(digest));
    net::SendReliable(message, sizeof(message));
    g_checks++;
}

void Describe(char* out, int capacity) {
    snprintf(out, static_cast<size_t>(capacity), "desync : %u controle(s), %u renvoi(s)", g_checks, g_resyncs);
}

}  // namespace desync
