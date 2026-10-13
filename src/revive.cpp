#include "revive.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "alert.h"
#include "autoplay.h"
#include "coop.h"
#include "levelsync.h"
#include "log.h"
#include "memory.h"
#include "network.h"
#include "oni.h"
#include "replication.h"
#include "rules.h"

using namespace oni;

namespace revive {

static const float kRange = 26.0f;           // du joueur debout au joueur à terre : un corps et demi
static const double kShield = 2.0;           // tout juste relevé, le joueur est encore à l'abri
static const double kLayDownDelay = 1.5;     // à terre mais toujours debout : on le couche
static const double kReviverTimeout = rules::kReviveHold + 2.0;  // sans suite de celui qui ranime
static const double kMaxStep = 0.5;          // une image trop longue ne fait pas sauter le compte à rebours
static const double kReminder = 5.0;         // d'un rappel à l'écran au suivant
static const double kRest = 1.0;             // après une réanimation menée au bout
static const double kPulseRate = 7.5;        // battement du repère, en radians par seconde : 1,2 par seconde
static const double kSettle = 2.0;           // à terre depuis ce délai, le corps est fixé même s'il bouge encore
static const float kDrift = 0.05f;           // écart à partir duquel il est ramené à sa place

// Notre joueur.
static bool g_downed = false;
static rules::Bleed g_bleed;
static double g_lastTick = 0.0;
static int g_reviver = -1;                   // joueur qui nous ranime : le compte à rebours est arrêté
static double g_reviverSince = 0.0;
static bool g_guarded = false;               // kChFlagUnkillable est posé par nous
static bool g_madeInvincible = false;        // kChFlagInvincible aussi
static bool g_inputWasEnabled = true;
static double g_shieldUntil = 0.0;
static double g_standingSince = 0.0;         // à terre sans être au sol, depuis cet instant
static double g_downSince = 0.0;
static bool g_pinned = false;                // le corps est arrivé au sol : il n'en bouge plus
static Vector3 g_pin = {};                   // sa place
static double g_now = 0.0;
static double g_reactUntil = 0.0;            // un coup reçu au sol se joue jusque-là : pas de verrou sur le repos

// Celui que nous ranimons.
static int g_target = -1;
static double g_holdSince = 0.0;
static double g_restUntil = 0.0;
// Celui que nous venons de cesser de ranimer : son état porte encore un instant « on me ranime ».
static int g_lastTarget = -1;
static double g_lastTargetUntil = 0.0;
static const double kStaleFlag = 0.6;        // le temps que cet état fasse l'aller-retour
static const float kKeepRange = 32.0f;       // celui qu'on ranime le reste jusqu'à cette distance
static const int kGroundInterp = 4;          // fondu vers un coup reçu au sol
static bool g_hit = false;                   // coup d'ennemi encaissé depuis la dernière image

// Les autres joueurs, tels que leur état les montre.
static bool g_seenDown[proto::kMaxPlayers] = {};
static double g_nextReminder[proto::kMaxPlayers] = {};
// Depuis quand l'état d'un autre joueur à terre dit « on me ranime » ; nul : personne.
static double g_revivedSince[proto::kMaxPlayers] = {};

static uint32_t Local() { return net::LocalPlayerId(); }

bool Downed() { return g_downed; }
bool Shielded() { return g_downed || g_now < g_shieldUntil; }
void OnLocalHit() { g_hit = true; }

// État du joueur de ce numéro s'il est à terre ici, dans notre niveau.
static bool DownedState(uint32_t player, proto::EntityState& state) {
    if (player == Local() || !rep::RemotePlayerState(player, state)) return false;
    return (state.flags & proto::kEntityDowned) != 0 && (state.flags & proto::kEntityDead) == 0;
}

// Un coéquipier est là, debout : quelqu'un peut nous ranimer.
static bool TeammateStanding() {
    for (uint32_t player = 0; player < static_cast<uint32_t>(proto::kMaxPlayers); player++) {
        proto::EntityState state;
        if (player == Local() || !rep::RemotePlayerState(player, state)) continue;
        if (!(state.flags & (proto::kEntityDead | proto::kEntityDowned))) return true;
    }
    return false;
}

// -------------------------------------------------------------------------------------------------
// Notre joueur à terre
// -------------------------------------------------------------------------------------------------
// Le moteur a de quoi arrêter un coup mortel : un personnage « increvable » garde un point de vie
// et tombe au sol. Notre joueur l'est tant qu'un coéquipier peut le ranimer ; sinon il meurt comme
// dans le jeu d'origine.
static void Guard(Character* player, bool wanted) {
    uint32_t& flags = field<uint32_t>(player, off::kChFlags);
    if (wanted) {
        // Déjà increvable sans nous : un script du niveau en a décidé, et c'est son affaire.
        if (!g_guarded && (flags & off::kChFlagUnkillable)) return;
        flags |= off::kChFlagUnkillable;
        g_guarded = true;
    } else if (g_guarded) {
        flags &= ~off::kChFlagUnkillable;
        g_guarded = false;
    }
}

static void GoDown(Character* player, double now) {
    const rules::ModeRules& mode = rules::Of(coop::Difficulty());
    g_downed = true;
    g_bleed.Start(mode.bleedSeconds);
    g_lastTick = now;
    g_reviver = -1;
    g_standingSince = 0.0;
    // À terre, plus rien ne l'atteint ni ne le fait réagir, et il ne répond plus aux touches.
    uint32_t& flags = field<uint32_t>(player, off::kChFlags);
    g_madeInvincible = !(flags & off::kChFlagInvincible);
    flags |= off::kChFlagInvincible;
    g_inputWasEnabled = mem::InputEnabled();
    mem::SetInputEnabled(false);
    g_downSince = now;
    g_pinned = false;
    LOG("a terre : notre joueur est tombe, %d s pour etre ranime (difficulte %s)",
        static_cast<int>(mode.bleedSeconds), mode.name);
    mem::ConsolePrint("OniMP : vous etes a terre : %d s pour qu'un coequipier vous ranime",
                      static_cast<int>(mode.bleedSeconds));
    alert::PlayerDown(Local());
}

// Notre joueur n'est plus à terre : relevé, ou mort.
static void Release(Character* player) {
    if (g_madeInvincible) field<uint32_t>(player, off::kChFlags) &= ~off::kChFlagInvincible;
    g_madeInvincible = false;
    mem::LockAnimation(player, false);
    g_pinned = false;
    g_reactUntil = 0.0;
    if (g_inputWasEnabled) mem::SetInputEnabled(true);
    g_downed = false;
    g_reviver = -1;
}

bool OnGroundReaction(void* tram, double now) {
    Character* player = mem::Player();
    if (!g_downed || !player || mem::IsDead(player) || !mem::GroundAnimation(tram)) return false;
    // Sans verrou le temps qu'elle se joue : verrouillée, le moteur la rejouerait en boucle. À sa
    // fin il retrouve de lui-même le repos au sol, que UpdateDowned verrouille de nouveau.
    mem::LockAnimation(player, false);
    if (!mem::SetAnimation(player, tram, kGroundInterp)) return false;
    g_reactUntil = now + mem::AnimDuration(tram) / 60.0 + 0.1;
    static double nextLog = 0.0;
    if (now >= nextLog) {
        nextLog = now + 1.0;
        LOG("a terre : coup recu au sol, reaction '%s' jouee ; compte a rebours inchange (%.1f s%s)", mem::AnimName(tram),
            g_bleed.remaining(), g_reviver >= 0 ? ", reanimation en cours" : "");
    }
    return true;
}

static void BleedOut(Character* player, double now) {
    LOG("a terre : temps ecoule, notre joueur meurt et la mission echoue");
    Release(player);
    Guard(player, false);
    mem::Kill(player);
    level::Fail(proto::kRestartBledOut, static_cast<uint8_t>(Local()), now);
}

static void GetUp(Character* player, uint8_t by, double now) {
    const rules::ModeRules& mode = rules::Of(coop::Difficulty());
    uint32_t health = rules::RevivedHealth(field<uint32_t>(player, off::kChMaxHealth), mode.revivePercent);
    double left = g_bleed.remaining();
    Release(player);
    mem::SetHealth(player, health);
    g_shieldUntil = now + kShield;
    LOG("a terre : ranime par le joueur %u a %.1f s de la fin, %u points de vie (%u pour cent)", by, left,
        static_cast<unsigned>(health), static_cast<unsigned>(mode.revivePercent));
    mem::ConsolePrint("OniMP : ranime par le joueur %u : %u points de vie, une touche de deplacement pour vous relever",
                      by, static_cast<unsigned>(health));
}

static void UpdateDowned(Character* player, double now) {
    if (mem::IsDead(player)) {  // tué par un script du niveau
        Release(player);
        return;
    }
    mem::SetInputEnabled(false);  // une cinématique qui se termine les rendrait

    // Le moteur l'a jeté au sol avec le coup. S'il ne l'a pas fait (coup reçu en l'air, classe sans
    // l'animation), on le couche ; une fois au sol, il y reste.
    bool resting = false;
    if (mem::Fallen(player, &resting)) {
        g_standingSince = 0.0;
        // Pas pendant un coup reçu au sol, qui doit se jouer une fois et rendre la place au repos.
        if (resting && now >= g_reactUntil) mem::LockAnimation(player, true);
    } else if (g_standingSince == 0.0) {
        g_standingSince = now;
    } else if (now - g_standingSince > kLayDownDelay) {
        mem::LayDown(player);
        g_standingSince = now;
    }

    // Arrivé au sol, le corps n'en bouge plus. Le moteur le laissait glisser : l'élan de la chute,
    // un ennemi ou le pantin d'un coéquipier qui le pousse en marchant dessus. Il s'éloignait de
    // celui qui venait le ranimer. (Chez les autres, son pantin ne heurte plus rien tant qu'il est
    // à terre : voir rep, UpdateRemotePlayer.) Sa place est prise quand il se pose (ou au bout de deux secondes s'il
    // rebondit encore), et il y est ramené, vitesse annulée, dès qu'il s'en écarte ; la hauteur
    // reste au moteur, pour qu'il suive le sol.
    if (resting || now - g_downSince > kSettle) {
        Vector3 here = mem::Location(player);
        if (!g_pinned) {
            g_pinned = true;
            g_pin = here;
            LOG("a terre : corps pose en (%.0f, %.0f, %.0f), il n'en bouge plus", here.x, here.y, here.z);
        } else if (std::fabs(here.x - g_pin.x) > kDrift || std::fabs(here.z - g_pin.z) > kDrift) {
            Vector3 back = {g_pin.x, here.y, g_pin.z};
            mem::WriteTransform(player, back, field<float>(player, off::kChFacing));
        }
    }

    // Celui qui nous ranimait est parti, tombé à son tour, ou ne donne plus suite : le compte à
    // rebours reprend.
    if (g_reviver >= 0) {
        proto::EntityState state;
        bool there = rep::RemotePlayerState(static_cast<uint32_t>(g_reviver), state) &&
                     !(state.flags & (proto::kEntityDead | proto::kEntityDowned));
        if (!there || now - g_reviverSince > kReviverTimeout) {
            LOG("a terre : le joueur %d ne nous ranime plus, compte a rebours repris a %.1f s", g_reviver,
                g_bleed.remaining());
            g_reviver = -1;
        }
    }

    double elapsed = now - g_lastTick;
    g_lastTick = now;
    if (elapsed > kMaxStep) elapsed = kMaxStep;
    if (g_bleed.Tick(elapsed, g_reviver >= 0)) BleedOut(player, now);
}

void OnReliable(uint8_t from, const uint8_t* data, int size, double now) {
    proto::Revive message;
    if (size < static_cast<int>(sizeof(message))) return;
    memcpy(&message, data, sizeof(message));
    if (message.levelHash != mem::LevelHash() || from >= proto::kMaxPlayers || from == Local()) return;
    if (message.target != Local()) return;  // pour la machine d'un autre joueur
    Character* player = mem::Player();
    if (!g_downed || !player) return;

    if (message.phase == proto::kReviveStart) {
        if (g_reviver >= 0) return;  // un autre s'en occupe déjà
        g_reviver = from;
        g_reviverSince = now;
        LOG("a terre : le joueur %u nous ranime, compte a rebours arrete a %.1f s", from, g_bleed.remaining());
        mem::ConsolePrint("OniMP : le joueur %u vous ranime", from);
    } else if (message.phase == proto::kReviveCancel) {
        if (g_reviver != from) return;
        g_reviver = -1;
        g_lastTick = now;
        LOG("a terre : reanimation interrompue, compte a rebours repris a %.1f s", g_bleed.remaining());
        mem::ConsolePrint("OniMP : reanimation interrompue : il vous reste %d s",
                          static_cast<int>(std::ceil(g_bleed.remaining())));
    } else if (message.phase == proto::kReviveDone) {
        if (g_reviver == from) GetUp(player, from, now);
    }
}

// -------------------------------------------------------------------------------------------------
// Ranimer un coéquipier
// -------------------------------------------------------------------------------------------------
static bool KeyHeld() {
    if (!(GetAsyncKeyState(VK_CONTROL) & 0x8000)) return false;
    // Plusieurs fenêtres du jeu peuvent tourner sur une même machine : seule compte celle qui a le
    // clavier.
    HWND window = GetForegroundWindow();
    DWORD process = 0;
    if (!window) return false;
    GetWindowThreadProcessId(window, &process);
    return process == GetCurrentProcessId();
}

static void Send(int target, proto::RevivePhase phase) {
    proto::Revive message = {mem::LevelHash(), static_cast<uint8_t>(target), static_cast<uint8_t>(phase)};
    uint8_t packet[1 + sizeof(message)];
    packet[0] = proto::kReliableRevive;
    memcpy(packet + 1, &message, sizeof(message));
    net::SendReliable(packet, sizeof(packet));
}

// Ce joueur à terre est à portée de main du nôtre, qui est en état de le ranimer.
static bool WithinReach(uint32_t id) {
    Character* player = mem::Player();
    Character* chr = rep::RemotePlayerCharacter(id);
    if (!player || !chr || g_downed || !net::Connected() || mem::IsDead(player)) return false;
    Vector3 here = mem::Location(player), there = mem::Location(chr);
    float dx = there.x - here.x, dy = there.y - here.y, dz = there.z - here.z;
    return dx * dx + dy * dy + dz * dz <= kRange * kRange;
}

// Le joueur à terre le plus proche du nôtre, à portée de main ; -1 si aucun.
static int NearestDowned(Character* player) {
    int best = -1;
    float bestDistance = kRange * kRange;
    Vector3 here = mem::Location(player);
    for (uint32_t id = 0; id < static_cast<uint32_t>(proto::kMaxPlayers); id++) {
        proto::EntityState state;
        Character* chr = rep::RemotePlayerCharacter(id);
        if (!chr || !DownedState(id, state)) continue;
        // Un autre le ranime déjà. Sauf si c'était nous à l'instant : son état le dit encore le
        // temps de faire l'aller-retour, et l'écarter nous faisait sauter d'un joueur à terre à
        // l'autre à chaque coup reçu (journaux de la 0.28 : dix annonces par seconde).
        bool ours = static_cast<int>(id) == g_target || (static_cast<int>(id) == g_lastTarget && g_now < g_lastTargetUntil);
        if ((state.flags & proto::kEntityReviving) && !ours) continue;
        Vector3 there = mem::Location(chr);
        float dx = there.x - here.x, dy = there.y - here.y, dz = there.z - here.z;
        float distance = dx * dx + dy * dy + dz * dz;
        if (distance > bestDistance) continue;
        best = static_cast<int>(id);
        bestDistance = distance;
    }
    return best;
}

static void UpdateReviving(Character* player, double now) {
    bool hit = g_hit;
    g_hit = false;
    // La touche, ou le jeu automatique quand il a mené notre joueur près d'un joueur à terre.
    bool able = net::Connected() && !g_downed && !mem::IsDead(player) && now >= g_restUntil &&
                (KeyHeld() || bot::WantsRevive());
    int nearest = able ? NearestDowned(player) : -1;
    // Celui qu'on ranime le reste tant qu'il est là, à terre et à portée : un autre joueur à terre
    // un peu plus près ne nous en détourne pas.
    if (able && g_target >= 0) {
        proto::EntityState state;
        Character* chr = rep::RemotePlayerCharacter(static_cast<uint32_t>(g_target));
        if (chr && DownedState(static_cast<uint32_t>(g_target), state)) {
            Vector3 here = mem::Location(player), there = mem::Location(chr);
            float dx = there.x - here.x, dy = there.y - here.y, dz = there.z - here.z;
            if (dx * dx + dy * dy + dz * dz <= kKeepRange * kKeepRange) nearest = g_target;
        }
    }

    if (g_target >= 0 && (nearest != g_target || hit)) {
        Send(g_target, proto::kReviveCancel);
        LOG("reanimation : du joueur %d interrompue apres %.1f s (%s)", g_target, now - g_holdSince,
            hit ? "coup d'un ennemi" : "touche relachee, ou trop loin");
        mem::ConsolePrint("OniMP : reanimation du joueur %d interrompue%s", g_target, hit ? " par un coup" : "");
        g_lastTarget = g_target;
        g_lastTargetUntil = now + kStaleFlag;
        g_target = -1;
        return;
    }
    if (g_target < 0) {
        if (nearest < 0 || hit) return;
        g_target = nearest;
        g_holdSince = now;
        Send(g_target, proto::kReviveStart);
        LOG("reanimation : du joueur %d commencee", g_target);
        mem::ConsolePrint("OniMP : reanimation du joueur %d : gardez CTRL %d s", g_target,
                          static_cast<int>(rules::kReviveHold));
        return;
    }
    if (now - g_holdSince < rules::kReviveHold) return;
    Send(g_target, proto::kReviveDone);
    LOG("reanimation : du joueur %d menee au bout", g_target);
    mem::ConsolePrint("OniMP : joueur %d ranime", g_target);
    g_target = -1;
    g_restUntil = now + kRest;
}

// -------------------------------------------------------------------------------------------------
// Alerte : un coéquipier est à terre
// -------------------------------------------------------------------------------------------------
// Dite à l'écran quand il tombe, puis redite avec le temps qui lui reste : toutes les cinq
// secondes, et chaque seconde sur la fin.
static void WatchOthers(double now) {
    for (uint32_t id = 0; id < static_cast<uint32_t>(proto::kMaxPlayers); id++) {
        proto::EntityState state;
        bool down = net::Connected() && DownedState(id, state);
        if (down && (!g_seenDown[id] || now >= g_nextReminder[id])) {
            int seconds = static_cast<int>(std::ceil(rules::BleedFromWire(state.health)));
            if (!g_seenDown[id]) {
                alert::PlayerDown(id);  // une fois, à l'instant où on le voit tomber
                mem::ConsolePrint("OniMP : JOUEUR %u A TERRE : %d s pour le ranimer, CTRL maintenu %d s pres de lui",
                                  static_cast<unsigned>(id), seconds, static_cast<int>(rules::kReviveHold));
            } else if (!(state.flags & proto::kEntityReviving)) {
                mem::ConsolePrint("OniMP : joueur %u a terre : %d s", static_cast<unsigned>(id), seconds);
            }
            g_nextReminder[id] = now + (seconds > 6 ? kReminder : 1.0);
        } else if (!down && g_seenDown[id]) {
            LOG("a terre : le joueur %u ne l'est plus", static_cast<unsigned>(id));
        }
        g_seenDown[id] = down;
        // Un troisième joueur le ranime : la barre se remplit d'après l'instant où on l'a appris.
        bool revived = down && (state.flags & proto::kEntityReviving) != 0;
        if (!revived) g_revivedSince[id] = 0.0;
        else if (g_revivedSince[id] <= 0.0) g_revivedSince[id] = now;
    }
}

// -------------------------------------------------------------------------------------------------
// Chaque frame
// -------------------------------------------------------------------------------------------------
void Update(double now) {
    g_now = now;
    Character* player = mem::Player();
    if (!player) return;
    if (g_downed) {
        UpdateDowned(player, now);
    } else {
        bool possible = net::Connected() && !mem::IsDead(player) && TeammateStanding();
        Guard(player, possible);
        // Le moteur vient d'arrêter un coup mortel à un point de vie.
        if (g_guarded && mem::Health(player) <= 1) GoDown(player, now);
    }
    UpdateReviving(player, now);
    WatchOthers(now);
}

void OnLevelChanged() {
    // Le moteur a recréé les personnages : nos drapeaux sont partis avec les anciens.
    if (g_downed && g_inputWasEnabled) mem::SetInputEnabled(true);
    g_downed = false;
    g_pinned = false;
    g_guarded = false;
    g_madeInvincible = false;
    g_reviver = -1;
    g_target = -1;
    g_lastTarget = -1;
    g_reactUntil = 0.0;
    g_hit = false;
    g_shieldUntil = 0.0;
    g_restUntil = 0.0;
    memset(g_seenDown, 0, sizeof(g_seenDown));
    memset(g_revivedSince, 0, sizeof(g_revivedSince));
}

void Stamp(proto::EntityState& state) {
    if (!g_downed) return;
    state.flags |= proto::kEntityDowned;
    if (g_reviver >= 0) state.flags |= proto::kEntityReviving;
    state.health = rules::BleedToWire(g_bleed.remaining());
}

bool GetCountdown(uint32_t player, Countdown& out) {
    double remaining = 0.0, total = rules::Of(coop::Difficulty()).bleedSeconds;
    out = {};
    if (player == Local()) {
        if (!g_downed) return false;
        remaining = g_bleed.remaining();
        total = g_bleed.total();
        out.reviving = g_reviver >= 0;
    } else {
        proto::EntityState state;
        if (!DownedState(player, state)) return false;
        remaining = rules::BleedFromWire(state.health);
        out.reviving = (state.flags & proto::kEntityReviving) != 0;
    }
    out.seconds = static_cast<float>(remaining);
    out.remaining = total > 0.0 ? static_cast<float>(remaining / total) : 0.0f;
    out.pulse = static_cast<float>(0.5 + 0.5 * std::sin(g_now * kPulseRate));
    out.progress = -1.0f;
    // La barre montre où en est la réanimation, à tous ceux qui la regardent. Jusqu'à la 0.33 seul
    // celui qui ranime la voyait se remplir : le joueur à terre n'avait que le titre « REANIMATION »
    // au-dessus d'un compte à rebours arrêté, sans savoir quand il se relèverait. La réanimation
    // dure un temps fixe : chaque machine la compte depuis l'instant où elle l'a apprise.
    double since = 0.0;  // nul : personne ne le ranime, ou on ne sait pas depuis quand
    if (g_target >= 0 && static_cast<uint32_t>(g_target) == player) {
        // Nous le ranimons.
        out.ours = true;
        out.reviving = true;
        since = g_holdSince;
    } else if (player == Local()) {
        if (out.reviving) since = g_reviverSince;  // on nous ranime : depuis l'annonce reçue
    } else if (out.reviving) {
        if (player < static_cast<uint32_t>(proto::kMaxPlayers)) since = g_revivedSince[player];
    } else {
        out.canRevive = WithinReach(player);
    }
    if (since > 0.0) {
        double done = (g_now - since) / rules::kReviveHold;
        out.progress = static_cast<float>(done < 0.0 ? 0.0 : done > 1.0 ? 1.0 : done);
    }
    return true;
}

void Describe(char* out, int capacity) {
    const rules::ModeRules& mode = rules::Of(coop::Difficulty());
    if (g_downed) {
        snprintf(out, static_cast<size_t>(capacity), "a terre, %.0f s restantes%s", g_bleed.remaining(),
                 g_reviver >= 0 ? ", reanimation en cours" : "");
    } else {
        snprintf(out, static_cast<size_t>(capacity), "debout ; a terre %d s, releve a %u %% (%s)",
                 static_cast<int>(mode.bleedSeconds), static_cast<unsigned>(mode.revivePercent),
                 g_guarded ? "un coequipier peut ranimer" : "personne pour ranimer");
    }
}

}  // namespace revive
