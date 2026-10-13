#include "autoplay.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "log.h"
#include "memory.h"
#include "network.h"
#include "oni.h"
#include "protocol.h"
#include "replication.h"
#include "revive.h"
#include "trail.h"

using namespace oni;

namespace bot {

static const float kPi = 3.14159265f;
static const float kFollowStart = 45.0f;     // plus loin que cela de l'hôte : il le rejoint
static const float kFollowStop = 25.0f;      // arrivé à cette distance : il s'arrête
static const float kRejoin = 900.0f;         // au-delà, il est replacé à côté de l'hôte
static const float kThreat = 90.0f;          // un ennemi à cette distance de lui est une menace
static const float kLeaderThreat = 70.0f;    // ou à cette distance de l'hôte
static const float kFloor = 14.0f;           // écart de hauteur au-delà duquel on n'est plus au même étage
// Portées mesurées dans les animations de Konoko (TRAM, « Extents » de chaque coup) : ses coups
// debout portent de 8,4 à 10,4 unités devant elle et passent à plus de 9 de haut ; le balayage
// (accroupi + pied, KONCOMkick_low1) porte à 10,6 et descend à 1 du sol.
static const float kStrike = 11.5f;          // ennemi debout : il frappe en deçà
static const float kStrikeClose = 9.0f;      // et cesse d'avancer en deçà
static const float kGroundStrike = 9.5f;     // ennemi au sol : il balaie en deçà
static const float kGroundClose = 7.0f;      // et cesse d'avancer en deçà
static const float kBlockedReach = 7.0f;     // retenu par le corps de l'ennemi : il frappe d'autant plus loin
static const int kBlockedTicks = 30;         // une demi-seconde sans s'en approcher : quelque chose le retient
// Distances de l'IA du jeu, lues dans les classes d'armes (ONWC, « MinShootingDistance ») : aucune
// arme ne se tire à moins de 12 à 20 unités, 70 pour le lance-superballes, 80 pour le canon hurleur.
static const float kContact = 20.0f;         // ennemi plus près que cela : corps à corps, arme en main ou non
static const float kRanged = 200.0f;         // armé : un ennemi est pris pour cible jusqu'à cette distance
static const float kExplosiveMin = 80.0f;    // arme dont le tir explose : pas de tir plus près
static const float kFireLane = 7.0f;         // coéquipier à moins que cela de la ligne de tir : il ne tire pas
static const float kAimOrigin = 14.0f;       // la visée part de cette hauteur au-dessus de sa place
static const float kAimChest = 9.0f;         // et vise à cette hauteur d'un ennemi debout
static const float kAimFallen = 1.5f;        // ou d'un ennemi au sol
static const int kReloadTicks = 90;          // arme vide : la touche de rechargement, à ce rythme
// Distance de tir : armé, il n'avance pas vers sa cible, et recule quand elle approche. Vitesses
// lues dans les animations de Konoko : un ennemi court à 53 unités par seconde, elle recule à 42
// avec un pistolet et à 36 avec un fusil. Reculer ne la met donc pas hors d'atteinte : cela lui
// donne deux à trois secondes de tir de plus, puis l'ennemi la serre et c'est le corps à corps.
static const float kKeepAway = 50.0f;        // ennemi plus près que cela : il recule en tirant
static const float kExplosiveKeep = 90.0f;   // la même distance, avec une arme dont le tir explose
static const float kBackStep = 10.0f;        // avant de reculer, il regarde à cette distance derrière lui
static const float kWaist = 6.0f;            // hauteur d'où partent ces regards
static const float kBackDrop = 16.0f;        // là-bas, pas de sol à moins que cela sous ses pieds : le vide
static const int kBackCheckTicks = 6;        // derrière lui : regardé dix fois par seconde
static const int kBackBan = 120;             // recul qui bute : il tient sa place deux secondes
// Arme en main ou rangée. Il n'y a de sprint que sans arme en main : 78 unités par seconde, contre
// 53 en courant avec un pistolet et 48 avec un fusil (seule l'animation KONSPRrun porte la
// variante « sprint », et une animation d'arme passe avant elle).
static const int kHolsterAfter = 60;         // plus d'ennemi depuis une seconde : il range son arme
static const int kWeaponRetry = 45;          // d'un geste (ranger, sortir) au suivant
static const int kWeaponRefused = 10;        // geste refusé par le moteur : nouvel essai dans un sixième de seconde
static const float kSwapNear = 40.0f;        // à cette distance de l'arme qu'il va prendre, il sort la sienne pour la lâcher
// Ligne de vue : un rayon du moteur, de ses yeux à ce qu'il regarde.
static const float kEye = 13.0f;             // hauteur de ses yeux au-dessus de sa place
static const float kRayMargin = 6.0f;        // obstacle à moins que cela de la cible : derrière elle, il ne la cache pas
static const int kSightTicks = 10;           // un ennemi est regardé six fois par seconde
static const int kSeenMemory = 180;          // perdu de vue depuis moins de trois secondes : poursuivi à pied
static const int kLeaderSightTicks = 15;
static const float kFetch = 120.0f;          // objet au sol : il y va jusqu'à cette distance
static const float kPickup = 6.0f;           // il est dessus
static const float kDropNear = 12.0f;        // il lâche l'arme qui ne sert plus à cette distance de la nouvelle
static const int kPickupWait = 20;           // ticks passés dessus avant de la prendre d'office
static const int kTakeEvery = 6;             // sur un bonus : il le ramasse, dix essais par seconde
static const int kLootScan = 20;             // sans objet en vue, il regarde le sol trois fois par seconde
static const int kLootBan = 600;             // objet qu'il n'a pas pu prendre : laissé dix secondes
static const int kLootHidden = 240;          // objet caché par le décor : revu dans quatre secondes
static const int kLootTimeout = 600;         // dix secondes pour l'atteindre
static const int kLootStall = 150;           // deux secondes et demie sans s'en approcher : il y renonce
static const int kHypoRetry = 1200;          // hypo resté sans effet : pas d'autre essai avant vingt secondes
static const uint16_t kPowerupClip = 0, kPowerupCell = 1, kPowerupHypo = 2;  // types de bonus du moteur
static const float kReviveStop = 12.0f;      // il s'arrête à cette distance du joueur à terre
static const float kReviveReach = 20.0f;     // et le ranime dès cette distance (la portée est de 26)
static const float kTurn = 0.22f;            // radians par tick : un demi-tour en un quart de seconde
static const float kSprintFrom = 12.0f;      // plus loin que cela de son but : il sprinte
static const int kSampleTicks = 30;          // toutes les demi-secondes : a-t-il avancé ?
static const float kProgress = 3.0f;         // moins que cela en une demi-seconde : il bute
static const int kStuckJump = 60;            // bloqué depuis une seconde : il saute
static const int kStuckDetour = 120;         // depuis deux : il renonce à la ligne droite
static const int kStuckRejoin = 420;         // depuis sept : replacé à côté de l'hôte
static const int kDirectBan = 300;           // ligne droite vers l'hôte interdite cinq secondes après un blocage
static const int kEnemyBan = 240;            // ennemi laissé quatre secondes quand un obstacle l'en sépare
static const int kStrikeTicks = 18;          // d'un coup au suivant
static const int kStrikeHold = 4;            // touche tenue pendant ce nombre de ticks
static const int kGroundTicks = 100;         // d'un balayage au suivant : l'animation dure 94 ticks
static const int kHypoTicks = 240;           // d'un hypo au suivant
static const int kHealBelow = 85;            // pour cent de la santé : en dessous, un hypo
static const int kHurt = 50;                 // pour cent : en dessous, un hypo au sol passe avant une arme
static const int kCritical = 20;             // pour cent : en dessous, un ennemi au contact passe avant une réanimation
static const int kNever = -100000;
// Depuis la 0.35 le jeu automatique ne se bat plus qu'au corps à corps : poings, pieds, balayage.
// Il ne sort, ne ramasse ni ne tire aucune arme, et ne va plus chercher ni arme ni munitions ;
// celle qu'il porte en arrivant est rangée, ou lâchée si elle ne se range pas. C'est ce qui a été
// demandé pour mettre fin à ses blocages (allers et retours vers une arme ou un chargeur, arme
// rangée puis ressortie, recul contre un mur). Tout ce qui servait au tir est resté en place
// derrière ce réglage : à faux, il rend le comportement de la 0.34.
static const bool kMeleeOnly = true;

static uint64_t g_held = 0;                  // actions tenues par lui au tick précédent
static int g_tick = 0;
static bool g_following = false;
static int g_stuck = 0;                      // ticks passés à vouloir avancer sans avancer
static Vector3 g_sample = {};
static int g_strikes = 0;
static int g_groundAt = -1000;               // tick du dernier balayage
static int g_hypoUntil = 0, g_nextHypo = 0;
static bool g_revive = false;
static const char* g_doing = "inactif";
static const char* g_said = "";              // dernière occupation notée au journal
static int g_saidAt = -1000;                 // et à quel tick : une ligne par seconde au plus
static unsigned g_rejoins = 0, g_detours = 0, g_sweeps = 0;
static trail::Trail g_trail;                 // par où l'hôte est passé
static int g_directBanUntil = 0;             // tick jusqu'auquel il suit la trace plutôt que la ligne droite
static uint16_t g_bannedEnemy = 0;           // ennemi qu'un obstacle sépare de lui
static int g_enemyBanUntil = 0;
static int g_onLoot = 0;                     // ticks passés sur l'objet qu'il vient chercher
static bool g_pitched = false;               // la visée verticale est la nôtre : à remettre droite
static bool g_fighting = false;              // un ennemi est à portée à ce tick
static bool g_sprint = false;                // son déplacement de ce tick se fait au sprint
static bool g_firing = false;                // il tenait la gâchette au dernier tick joué
static unsigned g_shots = 0, g_pickups = 0, g_hypos = 0, g_rays = 0, g_hidden = 0;
static int g_hypoCheckAt = 0;                // tick où regarder si l'hypo demandé a été pris
static uint16_t g_hypoBefore = 0;            // hypos portés avant de le demander
// Approche d'un ennemi au corps à corps : la plus petite distance atteinte, et quand.
static uint16_t g_closeHandle = 0;
static float g_closeBest = 0.0f;
static int g_closeAt = 0;
static int g_enemyAt = kNever;               // dernier tick où un ennemi était à combattre
static bool g_swapping = false;              // il change d'arme : la sienne doit être en main pour être lâchée
static int g_weaponAskAt = 0;                // pas de geste d'arme (ranger, sortir) avant ce tick
static int g_reloadUntil = 0, g_nextReload = 0;
static int g_backCheckedAt = kNever;         // tick du dernier regard derrière lui
static bool g_backClear = false;             // et ce qu'il y a vu : la voie est libre
static int g_backBanUntil = 0;               // il a buté en reculant : plus de recul avant ce tick
static int g_backSince = 0;                  // tick où il s'est mis à reculer ; 0 : il ne recule pas
static Vector3 g_backSample = {};
static unsigned g_holsters = 0, g_draws = 0, g_reloads = 0, g_backTicks = 0;

static bool Finite(const Vector3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

static float Flat(const Vector3& a, const Vector3& b) {
    float dx = b.x - a.x, dz = b.z - a.z;
    return std::sqrt(dx * dx + dz * dz);
}

static float Distance(const Vector3& a, const Vector3& b) {
    float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

static Vector3 Raised(const Vector3& at, float height) { return {at.x, at.y + height, at.z}; }

// Le moteur compte l'orientation de 0 à 2 pi, 0 vers les z croissants.
static float Wrap(float angle) {
    if (!std::isfinite(angle)) return 0.0f;
    angle = std::fmod(angle, 2.0f * kPi);
    return angle < 0.0f ? angle + 2.0f * kPi : angle;
}

// Tourne le personnage vers ce point, d'un pas au plus ; rend l'angle qui reste à faire. Un point
// qui n'est pas un nombre ne touche à rien : écrit dans l'orientation, il gagnait la position du
// joueur au tick suivant.
static float TurnTo(Character* player, const Vector3& from, const Vector3& to) {
    float current = field<float>(player, off::kChFacing);
    if (!Finite(from) || !Finite(to) || !std::isfinite(current)) return kPi;
    float diff = Wrap(std::atan2(to.x - from.x, to.z - from.z)) - current;
    if (diff > kPi) diff -= 2.0f * kPi;
    if (diff < -kPi) diff += 2.0f * kPi;
    float step = diff > kTurn ? kTurn : (diff < -kTurn ? -kTurn : diff);
    float facing = Wrap(current + step);
    field<float>(player, off::kChFacing) = facing;
    field<float>(player, off::kChDesiredFacing) = facing;
    return std::fabs(diff - step);
}

static void Say(const char* doing) {
    g_doing = doing;
    // Au journal quand il change d'occupation, mais jamais plus d'une ligne par seconde : entre
    // deux occupations voisines il peut hésiter d'un tick à l'autre.
    if (doing == g_said || g_tick - g_saidAt < 60) return;
    g_said = doing;
    g_saidAt = g_tick;
    LOG("jeu automatique : %s", doing);
}

static void Rejoin(Character* player, Character* leader, const char* why) {
    Vector3 there = mem::Location(leader);
    if (!Finite(there)) return;
    there.x += 2.0f;  // le moteur écarte de lui-même deux personnages qui se touchent
    mem::WriteTransform(player, there, field<float>(leader, off::kChFacing));
    g_stuck = 0;
    g_sample = there;
    g_rejoins++;
    trail::Clear(g_trail);
    g_directBanUntil = 0;
    LOG("jeu automatique : notre joueur replace a cote de l'hote (%s)", why);
}

// Le joueur à terre le plus proche de nous, l'hôte comme un autre client ; faux si aucun.
static bool NearestDowned(const Vector3& here, Vector3& where, uint32_t& who) {
    float best = 0.0f;
    bool found = false;
    for (uint32_t id = 0; id < static_cast<uint32_t>(proto::kMaxPlayers); id++) {
        Vector3 there;
        if (id == net::LocalPlayerId() || !rep::DownedPlayerAt(id, there) || !Finite(there)) continue;
        float distance = Flat(here, there) + std::fabs(there.y - here.y);
        if (found && distance >= best) continue;
        best = distance;
        where = there;
        who = id;
        found = true;
    }
    return found;
}

// Avance vers ce but, en sprintant s'il est loin.
static uint64_t Walk(Character* player, const Vector3& here, const Vector3& goal, float stop) {
    uint64_t wanted = 0;
    float distance = Flat(here, goal);
    if (distance > stop && TurnTo(player, here, goal) < 0.7f) wanted |= mem::kActionForward;
    if (distance > stop + kSprintFrom) g_sprint = true;
    // Une marche, un rebord : le but est juste devant et plus haut.
    if ((wanted & mem::kActionForward) && goal.y - here.y > 4.0f && distance < 14.0f && g_tick % 30 < 3) {
        wanted |= mem::kActionJump;
    }
    return wanted;
}

// -------------------------------------------------------------------------------------------------
// Ligne de vue
// -------------------------------------------------------------------------------------------------
// Rien ne cache ce point : ni le décor, ni un objet du niveau, ni un coéquipier. target : le
// personnage qu'on regarde, s'il y en a un ; le toucher, c'est le voir. Jusqu'à la 0.31 le jeu
// automatique ne regardait rien : il tirait sur un ennemi à travers un mur, et courait contre le mur
// vers une arme posée derrière.
static bool LineClear(Character* player, const Vector3& from, const Vector3& to, Character* target) {
    mem::Ray ray;
    g_rays++;
    if (!mem::CastRay(player, from, to, ray) || ray.kind == mem::RayKind::Nothing) return true;
    float length = Distance(from, to);
    if (ray.kind == mem::RayKind::Character) {
        if (ray.chr == target || ray.distance >= length - kRayMargin) return true;
        // Un autre personnage avant la cible : un coéquipier bouche la ligne, un ennemi non.
        return !(ray.chr && rep::IsPlayerCharacter(ray.chr));
    }
    return ray.distance >= length - kRayMargin;
}

// Ce qu'il sait de chaque ennemi regardé : s'il le voit, et quand il l'a vu pour la dernière fois.
struct Sight {
    uint16_t handle = 0;
    int at = kNever;
    int seenAt = kNever;
    bool clear = false;
};
static Sight g_sights[16];

static Sight& SightOf(uint16_t handle) {
    Sight* oldest = &g_sights[0];
    for (Sight& sight : g_sights) {
        if (sight.handle == handle && sight.at != kNever) return sight;
        if (sight.at < oldest->at) oldest = &sight;
    }
    *oldest = Sight();
    oldest->handle = handle;
    return *oldest;
}

static bool SeesEnemy(Character* player, const Vector3& here, Character* enemy) {
    Sight& sight = SightOf(mem::Handle(enemy));
    if (g_tick - sight.at >= kSightTicks) {
        Vector3 at = mem::Location(enemy);
        sight.clear = LineClear(player, Raised(here, kEye), Raised(at, mem::Fallen(enemy) ? kAimFallen + 1.0f : kAimChest), enemy);
        sight.at = g_tick;
        if (sight.clear) sight.seenAt = g_tick;
    }
    return sight.clear;
}

static bool SeenLately(Character* enemy) { return g_tick - SightOf(mem::Handle(enemy)).seenAt <= kSeenMemory; }

static bool SeesLeader(Character* player, const Vector3& here, Character* leader, const Vector3& leaderAt) {
    static int at = kNever;
    static bool clear = true;
    if (g_tick - at >= kLeaderSightTicks || g_tick < at) {
        at = g_tick;
        clear = LineClear(player, Raised(here, kEye), Raised(leaderAt, kAimChest), leader);
    }
    return clear;
}

// Suit l'hôte. De près, au même étage et en vue, en ligne droite ; sinon, ou dès qu'il bute, par la
// trace des endroits où l'hôte est passé, dans l'ordre : un chemin que le niveau permet, puisqu'un
// joueur vient de le faire, portes, couloirs et escaliers compris.
static uint64_t Follow(Character* player, const Vector3& here, Character* leader, const Vector3& leaderAt, float stop,
                       bool& onTrail) {
    bool near = Flat(here, leaderAt) < trail::kDirectRange && std::fabs(leaderAt.y - here.y) < kFloor;
    bool direct = near && g_tick >= g_directBanUntil && (g_trail.count == 0 || SeesLeader(player, here, leader, leaderAt));
    // En ligne droite, les points que l'hôte a semés à côté de notre route sont tenus pour dépassés.
    trail::Reach(g_trail, here, direct ? trail::kLoose : trail::kReach);
    onTrail = !direct && g_trail.count > 0;
    if (!onTrail) return Walk(player, here, leaderAt, stop);
    uint64_t wanted = Walk(player, here, trail::Next(g_trail), 0.0f);
    if (Flat(here, leaderAt) > stop + kSprintFrom) g_sprint = true;  // d'un point de la trace au suivant, sans ralentir
    return wanted;
}

// -------------------------------------------------------------------------------------------------
// Arme, munitions, hypos
// -------------------------------------------------------------------------------------------------
// Les armes du jeu, de la plus forte à la plus faible. Leur puissance n'est pas écrite dans leur
// classe (elle tient aux projectiles) : c'est l'ordre dans lequel le jeu les donne au joueur au fil
// des niveaux, le canon hurleur en tête, le pistolet avant les deux armes qui repoussent ou
// étourdissent sans guère blesser. Une arme qu'un mod ajouterait vaut un pistolet.
static int WeaponRank(const char* className) {
    static const struct {
        const char* name;
        int rank;
    } kRanks[] = {{"w11_ba1", 10}, {"w9_scr", 9}, {"w7_scc", 8}, {"w5_sbg", 7}, {"w3_phr", 6}, {"w8_mbo", 5},
                  {"w2_sap", 4}, {"w1_tap", 3}, {"w4_psm", 2}, {"w6_vdg", 1}};
    for (const auto& entry : kRanks) {
        if (strcmp(className, entry.name) == 0) return entry.rank;
    }
    return 3;
}

// Armes qu'il ne touche jamais : les deux armes de boss que le joueur n'est pas censé tenir (9999
// tirs). S'il en a une en main (une prise qui désarme la lui donne), il la lâche. L'arme de
// Barabas (w11_ba1) lui était interdite de la 0.31 à la 0.33 ; depuis la 0.34 il la prend, et
// c'est pour lui la plus forte de toutes.
static bool Forbidden(const char* className) {
    return strncmp(className, "w10_", 4) == 0 || strncmp(className, "w12_", 4) == 0;
}

// Recharges qu'il porte pour cette arme : cellules d'énergie ou chargeurs de balles, selon elle.
static int Refills(Character* player, void* weapon) {
    mem::Reserves reserves = mem::ReadReserves(player);
    return mem::WeaponUsesCells(weapon) ? reserves.cells : reserves.clips;
}

// L'arme sert : elle n'est pas interdite, et elle a des cartouches ou de quoi la recharger.
static bool Usable(Character* player, void* weapon) {
    if (!weapon || Forbidden(mem::WeaponClassName(weapon))) return false;
    return mem::WeaponAmmo(weapon) > 0 || Refills(player, weapon) > 0;
}

// Son arme : celle qu'il a en main, ou celle qu'il a rangée. Konoko n'en porte qu'une.
static void* Carried(Character* player) {
    void* held = field<void*>(player, off::kChWeaponInHand);
    return held ? held : field<void*>(player, off::kChWeaponStored);
}

enum class Loot { None, Weapon, Ammo, Hypo };
struct LootTarget {
    Loot kind = Loot::None;
    Vector3 at = {};
    void* weapon = nullptr;
    uint16_t type = 0;       // bonus : son type
};
static const char* LootName(Loot kind) {
    return kind == Loot::Weapon ? "une arme" : kind == Loot::Ammo ? "des munitions" : "un hypo";
}

// Ce qu'il est parti chercher. Choisi une fois, puis tenu jusqu'à ce qu'il l'ait, qu'un autre le
// prenne ou qu'il y renonce. Jusqu'à la 0.31 le choix était refait à chaque tick : au moindre saut
// l'objet n'était plus « au même étage », il repartait vers l'hôte, puis revenait, et ainsi de suite
// (journal de la 0.31 : « va chercher des munitions » et « rejoint l'hote » en alternance, une fois
// par seconde pendant quarante secondes, sans rien ramasser).
static LootTarget g_loot;
static int g_lootSince = 0;       // tick où il l'a choisi
static float g_lootBest = 0.0f;   // plus petite distance atteinte
static int g_lootBestAt = 0;      // et à quel tick
static int g_nextLootScan = 0;

// Objets auxquels il a renoncé pour un temps.
struct Ban {
    Vector3 at = {};
    int until = 0;
    int failures = 0;    // fois où il a dû renoncer à cet objet
};
static Ban g_bans[8];
static const int kBanMemory = 3600;  // un objet auquel il a renoncé il y a plus d'une minute est comme neuf
static const int kBanLongest = 3600;

static bool LootBanned(const Vector3& at) {
    for (const Ban& ban : g_bans) {
        if (g_tick < ban.until && Flat(at, ban.at) < kPickup && std::fabs(at.y - ban.at.y) < kFloor) return true;
    }
    return false;
}

// why nul : l'objet est seulement caché par le décor, il sera regardé de nouveau sans bruit.
// Sinon il n'a pas pu le prendre : laissé dix secondes, puis trois fois plus longtemps à chaque
// nouvel échec au même endroit. Depuis la 0.34 le butin passe avant le combat : un objet hors
// d'atteinte ne doit pas le détourner d'un ennemi toutes les dix secondes.
static void BanLoot(const Vector3& at, int ticks, const char* why) {
    Ban* slot = nullptr;
    for (Ban& ban : g_bans) {
        bool same = ban.failures > 0 && Flat(at, ban.at) < kPickup && std::fabs(at.y - ban.at.y) < kFloor;
        if (same && g_tick - ban.until < kBanMemory) slot = &ban;
    }
    int failures = slot ? slot->failures : 0;
    if (!slot) {
        slot = &g_bans[0];
        for (Ban& ban : g_bans) {
            if (ban.until < slot->until) slot = &ban;
        }
    }
    if (why) {
        for (int i = 0; i < failures && ticks < kBanLongest; i++) ticks *= 3;
        if (ticks > kBanLongest) ticks = kBanLongest;
        failures++;
    }
    slot->at = at;
    slot->until = g_tick + ticks;
    slot->failures = failures;
    if (why) LOG("jeu automatique : renonce a ce qu'il allait chercher (%s), %d secondes", why, ticks / 60);
    g_loot = LootTarget();
    g_onLoot = 0;
}

// À portée, au même étage, pas écarté.
static bool WithinFetch(const Vector3& here, const Vector3& at, float& distance) {
    if (!Finite(at) || std::fabs(at.y - here.y) > kFloor || LootBanned(at)) return false;
    distance = Flat(here, at);
    return distance < kFetch;
}

// Une arme : la plus forte de celles du sol qui servent (chargée, ou pour laquelle il a des
// recharges), si elle vaut mieux que la sienne. Une arme vide et sans recharge ne vaut rien : il
// prend alors ce qu'il trouve, même un pistolet. Jamais celle de Barabas.
static LootTarget BestWeapon(Character* player, const Vector3& here) {
    LootTarget best;
    void* carried = Carried(player);
    int bestRank = Usable(player, carried) ? WeaponRank(mem::WeaponClassName(carried)) : 0;
    float bestDistance = kFetch;
    for (int i = 0; i < addr::kMaxWeapons; i++) {
        void* weapon = mem::WeaponAt(i);
        if (!mem::WeaponOnGround(weapon) || !Usable(player, weapon)) continue;
        Vector3 at = mem::WeaponPosition(weapon);
        float distance = 0.0f;
        if (!WithinFetch(here, at, distance)) continue;
        int rank = WeaponRank(mem::WeaponClassName(weapon));
        // Plus forte que ce qu'on a retenu ; à force égale avec une autre arme du sol, la plus proche.
        if (rank < bestRank || (rank == bestRank && (best.kind == Loot::None || distance >= bestDistance))) continue;
        bestRank = rank;
        bestDistance = distance;
        best.kind = Loot::Weapon;
        best.at = at;
        best.weapon = weapon;
    }
    return best;
}

// Le bonus de ce type le plus proche.
static LootTarget NearestPowerup(const Vector3& here, uint16_t type, Loot kind) {
    LootTarget best;
    float bestDistance = kFetch;
    int count = mem::PowerupCount();
    for (int i = 0; i < count; i++) {
        uint16_t found = 0, amount = 0;
        Vector3 at;
        mem::ReadPowerup(i, found, amount, at);
        float distance = 0.0f;
        if (found != type || !WithinFetch(here, at, distance) || distance >= bestDistance) continue;
        bestDistance = distance;
        best.kind = kind;
        best.at = at;
        best.type = type;
    }
    return best;
}

// Ce qu'il va chercher, dans l'ordre, dès qu'il le voit à portée et quoi qu'il fasse d'autre
// (0.34 : le butin passe avant le combat, il n'attend plus qu'aucun ennemi ne soit en vue) :
//   1. un hypo, mal en point et sans en porter ;
//   2. une arme qui serve, quand la sienne ne sert plus ou qu'il n'en a pas : la plus forte de
//      celles du sol, et faute de mieux la plus faible ;
//   3. des recharges pour l'arme qu'il porte, quand il n'en a plus ;
//   4. une arme plus forte que la sienne ;
//   5. le plein : chargeurs (balles) et cellules (énergie) quelle que soit son arme, et hypos, le
//      plus proche d'abord, jusqu'à ce que l'inventaire soit plein.
static LootTarget FindLoot(Character* player, const Vector3& here, int healthPercent) {
    void* carried = Carried(player);
    mem::Reserves reserves = mem::ReadReserves(player);
    if (healthPercent < kHurt && reserves.hypos == 0) {
        LootTarget hypo = NearestPowerup(here, kPowerupHypo, Loot::Hypo);
        if (hypo.kind != Loot::None) return hypo;
    }
    if (kMeleeOnly) {
        // Mêlée seule : ni arme ni munitions. Restent les hypos qui lui manquent, et seulement au
        // calme : en plein combat, seul l'hypo dont il a besoin tout de suite (ci-dessus) le détourne.
        bool calm = g_enemyAt == kNever || g_tick - g_enemyAt > kHolsterAfter;
        return calm && reserves.hypos < mem::kHypoCapacity ? NearestPowerup(here, kPowerupHypo, Loot::Hypo) : LootTarget();
    }
    LootTarget weapon = BestWeapon(player, here);
    if (weapon.kind != Loot::None && !Usable(player, carried)) return weapon;
    if (carried && !Forbidden(mem::WeaponClassName(carried)) && Refills(player, carried) == 0) {
        LootTarget refill = NearestPowerup(here, mem::WeaponUsesCells(carried) ? kPowerupCell : kPowerupClip, Loot::Ammo);
        if (refill.kind != Loot::None) return refill;
    }
    if (weapon.kind != Loot::None) return weapon;

    int capacity = mem::ReserveCapacity();
    LootTarget best;
    auto consider = [&](bool wanted, uint16_t type, Loot kind) {
        if (!wanted) return;
        LootTarget found = NearestPowerup(here, type, kind);
        if (found.kind == Loot::None) return;
        if (best.kind == Loot::None || Flat(here, found.at) < Flat(here, best.at)) best = found;
    };
    consider(reserves.clips < capacity, kPowerupClip, Loot::Ammo);
    consider(reserves.cells < capacity, kPowerupCell, Loot::Ammo);
    consider(reserves.hypos < mem::kHypoCapacity, kPowerupHypo, Loot::Hypo);
    return best;
}

// L'objet qu'il était parti chercher est toujours là.
static bool LootStillThere() {
    if (g_loot.kind == Loot::Weapon) {
        if (!mem::WeaponOnGround(g_loot.weapon)) return false;
        Vector3 at = mem::WeaponPosition(g_loot.weapon);
        if (!Finite(at) || Flat(at, g_loot.at) > 25.0f) return false;
        g_loot.at = at;  // elle peut encore glisser
        return true;
    }
    // Un bonus qu'un ennemi vient de laisser tombe et roule encore : sa place est suivie, comme
    // celle d'une arme. Sans cela le ramassage le cherchait là où il n'était plus (journal de la
    // 0.33 : un chargeur visé en plein combat, « le moteur refuse de le lui donner »).
    int count = mem::PowerupCount();
    bool there = false;
    float nearest = kPickup;
    Vector3 place = g_loot.at;
    for (int i = 0; i < count; i++) {
        uint16_t found = 0, amount = 0;
        Vector3 at;
        mem::ReadPowerup(i, found, amount, at);
        if (found != g_loot.type || !Finite(at) || std::fabs(at.y - g_loot.at.y) >= 10.0f) continue;
        float distance = Flat(at, g_loot.at);
        if (distance >= nearest) continue;
        nearest = distance;
        place = at;
        there = true;
    }
    g_loot.at = place;
    return there;
}

// Tient l'objet choisi, ou en choisit un. Rend celui qu'il poursuit, nul s'il n'y en a pas.
static const LootTarget* PursueLoot(Character* player, const Vector3& here, int healthPercent) {
    if (g_loot.kind != Loot::None) {
        if (!LootStillThere()) {
            // Pris, par lui en passant dessus ou par un autre.
            g_loot = LootTarget();
            g_onLoot = 0;
            g_nextLootScan = g_tick;
        } else {
            float distance = Flat(here, g_loot.at);
            if (distance < g_lootBest - 2.0f) {
                g_lootBest = distance;
                g_lootBestAt = g_tick;
            }
            if (g_tick - g_lootSince > kLootTimeout) {
                BanLoot(g_loot.at, kLootBan, "trop long a atteindre");
            } else if (distance > kPickup && g_tick - g_lootBestAt > kLootStall) {
                BanLoot(g_loot.at, kLootBan, "il ne s'en approche plus");
            }
        }
    }
    if (g_loot.kind == Loot::None && g_tick >= g_nextLootScan) {
        g_nextLootScan = g_tick + kLootScan;
        LootTarget found = FindLoot(player, here, healthPercent);
        if (found.kind != Loot::None) {
            // Il n'y va que s'il le voit : derrière un mur, il courrait contre le mur.
            if (!LineClear(player, Raised(here, kEye), Raised(found.at, 2.0f), nullptr)) {
                g_hidden++;
                BanLoot(found.at, kLootHidden, nullptr);
            } else {
                g_loot = found;
                g_lootSince = g_tick;
                g_lootBest = Flat(here, found.at);
                g_lootBestAt = g_tick;
                g_onLoot = 0;
                LOG("jeu automatique : %s en vue a %.0f unites%s%s, il y va", LootName(found.kind), g_lootBest,
                    found.kind == Loot::Weapon ? " : " : "", found.kind == Loot::Weapon ? mem::WeaponClassName(found.weapon) : "");
            }
        }
    }
    return g_loot.kind != Loot::None ? &g_loot : nullptr;
}

// Un coéquipier se trouve sur la ligne de tir, entre nous et la cible.
static bool AllyInLine(const Vector3& here, const Vector3& target) {
    float dx = target.x - here.x, dz = target.z - here.z;
    float length = dx * dx + dz * dz;
    if (length < 1.0f) return false;
    for (uint32_t id = 0; id < static_cast<uint32_t>(proto::kMaxPlayers); id++) {
        Character* ally = rep::RemotePlayerCharacter(id);
        if (!ally) continue;
        Vector3 at = mem::Location(ally);
        if (!Finite(at)) continue;
        float t = ((at.x - here.x) * dx + (at.z - here.z) * dz) / length;
        if (t <= 0.0f || t >= 1.0f) continue;
        float px = here.x + dx * t - at.x, pz = here.z + dz * t - at.z;
        if (px * px + pz * pz < kFireLane * kFireLane) return true;
    }
    return false;
}

static void ResetPitch(Character* player) {
    if (!g_pitched) return;
    mem::SetPlayerPitch(player, 0.0f);
    g_pitched = false;
}

// Corps à corps : ennemi au sol, le balayage ; debout, deux coups de poing puis un coup de pied.
static uint64_t Melee(Character* player, const Vector3& here, Character* enemy, const Vector3& enemyAt,
                      float enemyDistance) {
    uint64_t wanted = 0;
    float left = TurnTo(player, here, enemyAt);
    // S'en approche-t-il encore ? Le corps d'un ennemi, couché surtout, l'arrête avant la distance
    // voulue : il courait alors sur place devant lui sans jamais frapper. Retenu depuis une
    // demi-seconde, il cesse d'avancer et frappe de là où il est.
    uint16_t handle = mem::Handle(enemy);
    if (handle != g_closeHandle || enemyDistance < g_closeBest - 1.0f || enemyDistance > g_closeBest + 4.0f) {
        g_closeHandle = handle;
        g_closeBest = enemyDistance;
        g_closeAt = g_tick;
    }
    bool blocked = g_tick - g_closeAt > kBlockedTicks;
    if (mem::Fallen(enemy)) {
        // Les coups debout passent au-dessus d'un corps couché. Le balayage est celui des coups de
        // Konoko qui descend au sol avec la meilleure cadence : 5 points toutes les 1,6 s, contre
        // 10 toutes les 3,7 s pour la roulade sautée.
        Say("frappe un ennemi au sol");
        float reach = kGroundStrike + (blocked ? kBlockedReach : 0.0f);
        if (enemyDistance > kGroundClose && left < 0.6f && !(blocked && enemyDistance <= reach)) wanted |= mem::kActionForward;
        if (enemyDistance <= reach && left < 0.3f && g_tick - g_groundAt >= kGroundTicks) {
            g_groundAt = g_tick;
            g_sweeps++;
        }
        // Accroupi et pied enfoncés au même tick, depuis la station debout : c'est ce qui donne le
        // balayage, et non le coup de pied accroupi, qui passe plus haut.
        if (g_tick - g_groundAt < kStrikeHold) wanted = mem::kActionCrouch | mem::kActionKick;
        return wanted;
    }
    Say("frappe un ennemi");
    float reach = kStrike + (blocked ? kBlockedReach : 0.0f);
    if (enemyDistance > kStrikeClose && left < 0.7f && !(blocked && enemyDistance <= reach)) wanted |= mem::kActionForward;
    if (enemyDistance > kStrike + kSprintFrom) g_sprint = true;  // de loin il arrive au sprint, et ralentit pour frapper
    if (enemyDistance <= reach && left < 0.5f) {
        int phase = g_tick % kStrikeTicks;
        if (phase < kStrikeHold) wanted |= g_strikes % 3 == 2 ? mem::kActionKick : mem::kActionPunch;
        else if (phase == kStrikeHold) g_strikes++;
    }
    return wanted;
}

// L'ennemi à combattre.
struct Foe {
    Character* chr = nullptr;
    Vector3 at = {};
    float distance = 0.0f;
    bool visible = false;
};

// Tir : tourné vers la cible, visée à la hauteur de son buste (ou de son corps au sol), gâchette
// tenue. Jamais quand un coéquipier est sur la ligne de tir : le tir ami peut être activé.
// Rechargement : jamais par avance. Une seule condition, l'arme vide (zéro cartouche) et de quoi
// la recharger dans l'inventaire ; sans recharge elle ne « sert » plus, et il ne vient pas ici :
// KeepWeapon la range. Rechargée, il reprend le tir.
static uint64_t Shoot(Character* player, const Vector3& here, const Foe& foe, void* weapon) {
    uint64_t wanted = 0;
    float left = TurnTo(player, here, foe.at);
    float height = foe.at.y + (mem::Fallen(foe.chr) ? kAimFallen : kAimChest) - (here.y + kAimOrigin);
    mem::SetPlayerPitch(player, std::atan2(height, foe.distance < 1.0f ? 1.0f : foe.distance));
    g_pitched = true;
    if (!weapon) {
        Say("sort son arme");  // elle est rangée : KeepWeapon la sort, il fait déjà face
        return 0;
    }
    if (mem::WeaponAmmo(weapon) == 0) {
        Say("recharge son arme");
        int refills = Refills(player, weapon);
        if (refills > 0 && !mem::Reloading(player) && g_tick >= g_nextReload) {
            g_reloadUntil = g_tick + 3;
            g_nextReload = g_tick + kReloadTicks;
            g_reloads++;
            LOG("jeu automatique : '%s' vide, il recharge (%d recharge(s) dans l'inventaire)", mem::WeaponClassName(weapon),
                refills);
        }
        if (g_tick < g_reloadUntil) wanted |= mem::kActionReload;
    } else if (mem::WeaponExplosive(weapon) && foe.distance < kExplosiveMin) {
        Say("s'ecarte avant un tir qui explose");
    } else {
        Say("tire sur un ennemi");
        if (left < 0.12f && !AllyInLine(here, foe.at)) {
            wanted |= mem::kActionFire;
            g_shots++;
        }
    }
    return wanted;
}

// Peut-il reculer ? Il ne voit pas derrière lui : deux rayons du moteur le font pour lui. Le
// premier, à hauteur de taille, d'ici à dix unités en arrière : un mur, un objet, un personnage.
// Le second, de là-bas vers le bas : sans sol à moins d'un étage, c'est le vide, et il ne recule
// pas. Regardé dix fois par seconde.
static bool BackClear(Character* player, const Vector3& here) {
    if (g_tick < g_backBanUntil) return false;
    if (g_tick >= g_backCheckedAt && g_tick - g_backCheckedAt < kBackCheckTicks) return g_backClear;
    g_backCheckedAt = g_tick;
    g_backClear = false;
    float facing = field<float>(player, off::kChFacing);
    if (!std::isfinite(facing)) return false;
    Vector3 behind = {here.x - std::sin(facing) * kBackStep, here.y, here.z - std::cos(facing) * kBackStep};
    mem::Ray ray;
    g_rays++;
    if (!mem::CastRay(player, Raised(here, kWaist), Raised(behind, kWaist), ray)) return false;
    if (ray.kind != mem::RayKind::Nothing && ray.distance < kBackStep + 2.0f) return false;  // quelque chose derrière lui
    g_rays++;
    if (!mem::CastRay(player, Raised(behind, kWaist), Raised(behind, -kBackDrop), ray)) return false;
    bool ground = ray.kind == mem::RayKind::Environment || ray.kind == mem::RayKind::Object;
    g_backClear = ground && ray.distance <= kWaist + kBackDrop;
    return g_backClear;
}

// Le plus proche de ceux qu'il voit. Un ennemi qu'il ne voit pas ne compte que s'il est au contact,
// ou s'il vient de le perdre de vue : il le poursuit alors à pied, sans tirer. À défaut, celui qui
// serre l'hôte de près, s'il est en vue. Jamais celui d'un autre étage, ni celui qu'un obstacle
// sépare de lui, le temps de l'oublier.
static Foe PickEnemy(Character* player, const Vector3& here, bool armed, Character* leader, const Vector3& leaderAt) {
    Foe seen, pursued;
    Character* list[8];
    int count = rep::Hostiles(here, armed ? kRanged : kThreat, list, 8);
    for (int i = 0; i < count; i++) {
        Character* chr = list[i];
        Vector3 at = mem::Location(chr);
        if (!Finite(at) || std::fabs(at.y - here.y) > kFloor) continue;
        if (mem::Handle(chr) == g_bannedEnemy && g_tick < g_enemyBanUntil) continue;
        float distance = Flat(here, at);
        if (SeesEnemy(player, here, chr)) {
            seen = {chr, at, distance, true};
            break;
        }
        if (!pursued.chr && (distance <= kContact || (distance <= kThreat && SeenLately(chr)))) {
            pursued = {chr, at, distance, false};
        }
    }
    if (seen.chr) return pursued.chr && pursued.distance <= kContact && pursued.distance < seen.distance ? pursued : seen;
    if (pursued.chr) return pursued;
    if (!leader) return Foe();
    count = rep::Hostiles(leaderAt, kLeaderThreat, list, 4);
    for (int i = 0; i < count; i++) {
        Character* chr = list[i];
        Vector3 at = mem::Location(chr);
        if (!Finite(at) || std::fabs(at.y - here.y) > kFloor) continue;
        if (mem::Handle(chr) == g_bannedEnemy && g_tick < g_enemyBanUntil) continue;
        if (SeesEnemy(player, here, chr)) return {chr, at, Flat(here, at), true};
    }
    return Foe();
}

// Ce que le jeu automatique veut tenir comme touches à ce tick, dans l'ordre demandé pour la 0.34 :
// ranimer un coéquipier, ramasser ce qui traîne, puis combattre (tirer en gardant ses distances,
// frapper l'ennemi qui le serre), et à défaut suivre l'hôte. Que son arme soit en main ou rangée
// est l'affaire de KeepWeapon, appelée ensuite quoi qu'il ait décidé ici.
static uint64_t Decide(Character* player) {
    Vector3 here = mem::Location(player);
    g_fighting = false;
    g_sprint = false;
    if (!Finite(here)) return 0;  // la garde le ramène au tick suivant
    Character* leader = rep::RemotePlayerCharacter(proto::kHostPlayerId);
    Vector3 leaderAt = leader ? mem::Location(leader) : here;
    if (!Finite(leaderAt)) {
        leader = nullptr;
        leaderAt = here;
    }
    float leaderDistance = leader ? Flat(here, leaderAt) : 0.0f;
    if (leader) trail::Record(g_trail, leaderAt);
    else trail::Clear(g_trail);
    if (leader && leaderDistance > kRejoin) {
        Rejoin(player, leader, "trop loin de lui");
        return 0;
    }

    void* held = field<void*>(player, off::kChWeaponInHand);
    void* carried = Carried(player);
    // Rangée ou non : devant un ennemi elle sort. En mêlée seule il n'est jamais « armé » : tout
    // ennemi se traite au corps à corps, et le tir (point 4) n'est jamais pris.
    bool armed = !kMeleeOnly && Usable(player, carried);
    Foe foe = PickEnemy(player, here, armed, leader, leaderAt);
    bool contact = foe.chr && foe.distance <= kContact;
    g_fighting = foe.chr && (foe.visible || contact);
    if (foe.chr) g_enemyAt = g_tick;

    int32_t health = mem::Health(player), most = static_cast<int32_t>(field<uint32_t>(player, off::kChMaxHealth));
    int healthPercent = most > 0 ? static_cast<int>(health * 100 / most) : 100;
    bool critical = healthPercent < kCritical;

    uint64_t wanted = 0;
    bool toLeader = false, onTrail = false, toEnemy = false, toLoot = false;
    bool shooting = false;
    bool tooCloseToFire = foe.chr && held && mem::WeaponExplosive(held) && foe.distance < kExplosiveMin;
    const LootTarget* loot = nullptr;

    // 1. Un joueur est à terre : tout le reste attend. Seule exception, notre propre santé au plus
    // bas avec un ennemi au contact : mort, le bot ne ranimerait personne.
    Vector3 downAt;
    uint32_t downed = 0;
    if (NearestDowned(here, downAt, downed) && !(critical && contact)) {
        float distance = Flat(here, downAt);
        bool reach = distance < kReviveReach && std::fabs(downAt.y - here.y) < kFloor;
        g_revive = reach;
        Say(reach ? "ranime un joueur a terre" : "va ranimer un joueur a terre");
        if (reach && distance <= kReviveStop) {
            // Sur place et sans frapper, le temps de le relever : seul un coup reçu l'interrompt.
            g_stuck = 0;
            g_sample = here;
            ResetPitch(player);
            return 0;
        }
        if (downed == proto::kHostPlayerId && leader) {
            toLeader = true;
            wanted = Follow(player, here, leader, leaderAt, kReviveStop, onTrail);
        } else {
            wanted = Walk(player, here, downAt, kReviveStop);
        }
    } else if ((loot = PursueLoot(player, here, healthPercent)) != nullptr) {
        // 2. Ce qui traîne, avant tout combat : il change de route aussitôt pour l'arme, les
        // munitions des deux sortes et les hypos, jusqu'à ce que l'inventaire soit plein.
        toLoot = true;
        float distance = Flat(here, loot->at);
        bool onIt = distance < kPickup;
        g_onLoot = onIt ? g_onLoot + 1 : 0;
        Say(loot->kind == Loot::Weapon ? "va chercher une arme"
                                       : loot->kind == Loot::Ammo ? "va chercher des munitions" : "va chercher un hypo");
        if (loot->kind == Loot::Weapon) {
            // Il ne doit plus rien porter pour la prendre : son arme est lâchée en arrivant. Rangée,
            // elle sort d'abord (KeepWeapon) : le moteur ne lâche que l'arme en main.
            if (carried && distance < kSwapNear) g_swapping = true;
            if (held && distance < kDropNear && g_tick % 60 < 3) wanted |= mem::kActionDrop;
            // Le moteur ne la ramasse qu'à la touche d'échange d'arme : elle est prise d'office,
            // par le même appel que lui, une fois qu'il est dessus sans plus rien porter.
            if (!carried && g_onLoot > kPickupWait) {
                void* taken = loot->weapon;
                mem::TakeWeapon(player, taken);
                g_pickups++;
                LOG("jeu automatique : arme '%s' ramassee (%u cartouche(s))", mem::WeaponClassName(taken),
                    static_cast<unsigned>(mem::WeaponAmmo(taken)));
                g_loot = LootTarget();
                g_onLoot = 0;
                loot = nullptr;
            }
        } else if (onIt && g_onLoot % kTakeEvery == 1 && mem::TakePowerup(player, loot->type, loot->at)) {
            // Le moteur ne donne un bonus qu'à la touche d'échange d'arme, jamais au seul passage
            // (0x4F74CD) : rester dessus ne suffisait pas. Journal de la 0.32 : neuf fois « des
            // munitions en vue, il y va », neuf fois « dessus sans le prendre ». Il le ramasse donc
            // par la routine que cette touche appelle, aux conditions du moteur.
            mem::Reserves now = mem::ReadReserves(player);
            g_pickups++;
            LOG("jeu automatique : %s ; il porte %u chargeur(s), %u cellule(s), %u hypo(s)",
                loot->kind == Loot::Hypo ? "hypo ramasse"
                                         : loot->type == kPowerupCell ? "cellule d'energie ramassee" : "chargeur ramasse",
                static_cast<unsigned>(now.clips), static_cast<unsigned>(now.cells), static_cast<unsigned>(now.hypos));
            g_loot = LootTarget();
            g_onLoot = 0;
            g_nextLootScan = g_tick;  // le suivant tout de suite
            loot = nullptr;
        } else if (g_onLoot > 2 * kPickupWait) {
            // Dessus et le moteur le refuse : inventaire plein, ou bonus trop haut pour sa main.
            BanLoot(loot->at, kLootBan, "le moteur refuse de le lui donner");
            loot = nullptr;
        }
        if (loot) wanted |= Walk(player, here, loot->at, 1.0f);
    } else if (contact) {
        // 3. Un ennemi collé à lui (20 unités) : corps à corps tout de suite, arme en main ou non.
        // L'IA du jeu fait de même : aucune de ses armes ne se tire à moins de 12 à 20 unités.
        toEnemy = true;
        wanted = Melee(player, here, foe.chr, foe.at, foe.distance);
    } else if (foe.chr && foe.visible && armed && !(tooCloseToFire && !BackClear(player, here))) {
        // 4. Armé, et l'ennemi en vue : le tir, à distance. Il n'avance jamais vers sa cible, et
        // recule quand elle approche, tant que rien ne l'arrête derrière lui. Seule exception : un
        // tir qui explose, l'ennemi trop près pour tirer, et pas la place de reculer. Rester là
        // sans rien faire le livrerait : c'est alors le corps à corps, comme sans arme.
        shooting = true;
        wanted = Shoot(player, here, foe, held);
        float keep = held && mem::WeaponExplosive(held) ? kExplosiveKeep : kKeepAway;
        if (foe.distance < keep && BackClear(player, here)) wanted |= mem::kActionBackward;
    } else if (foe.chr && foe.distance <= kThreat) {
        // 5. Sans arme qui serve, ou ennemi perdu de vue à l'instant : au corps à corps.
        toEnemy = true;
        wanted = Melee(player, here, foe.chr, foe.at, foe.distance);
    } else if (leader) {
        // 6. Rien d'autre à faire : rester près de l'hôte.
        if (leaderDistance > kFollowStart || std::fabs(leaderAt.y - here.y) > kFloor) g_following = true;
        else if (leaderDistance < kFollowStop) g_following = false;
        if (!g_following) {
            Say("reste pres de l'hote");
            // À l'arrêt : la trace ne garde que ce que l'hôte fait depuis qu'il s'éloigne.
            trail::Reach(g_trail, here, trail::kIdle);
            g_stuck = 0;
            g_sample = here;
            ResetPitch(player);
            return 0;
        }
        toLeader = true;
        wanted = Follow(player, here, leader, leaderAt, kFollowStop, onTrail);
        Say(onTrail ? "rejoint l'hote par ou il est passe" : "rejoint l'hote");
    } else {
        Say("attend l'hote");
        ResetPitch(player);
        return 0;
    }
    if (!shooting) ResetPitch(player);
    if (!toLoot) {
        g_onLoot = 0;
        // L'objet choisi attend : le temps passé à autre chose ne compte pas contre lui.
        if (g_loot.kind != Loot::None) {
            g_lootSince++;
            g_lootBestAt++;
        }
    }

    // Recule-t-il vraiment ? Les deux rayons ne voient pas tout (une marche, un coin) : s'il n'a
    // pas bougé en une demi-seconde, il cesse de reculer pour deux secondes et tire de là.
    if (wanted & mem::kActionBackward) {
        g_backTicks++;
        if (!g_backSince) {
            g_backSince = g_tick;
            g_backSample = here;
        } else if (g_tick - g_backSince >= kSampleTicks) {
            if (Flat(here, g_backSample) < kProgress) {
                g_backBanUntil = g_tick + kBackBan;
                LOG("jeu automatique : recul bloque, il tient sa place deux secondes");
            }
            g_backSince = g_tick;
            g_backSample = here;
        }
    } else {
        g_backSince = 0;
    }

    if (!(wanted & mem::kActionForward)) {
        g_stuck = 0;
        g_sample = here;
        return wanted;
    }
    // Il veut avancer : avance-t-il ? Sinon un saut, puis un autre chemin, et en dernier recours le
    // replacement à côté de l'hôte.
    if (g_tick % kSampleTicks == 0) {
        g_stuck = Flat(here, g_sample) < kProgress ? g_stuck + kSampleTicks : 0;
        g_sample = here;
    }
    if (g_stuck >= kStuckJump && g_tick % 45 < 3) wanted |= mem::kActionJump;
    if (g_stuck == kStuckDetour) {
        g_detours++;
        if (toLoot && g_loot.kind != Loot::None) {
            BanLoot(g_loot.at, kLootBan, "bloque en chemin");
        } else if (toEnemy && foe.chr) {
            // Un obstacle entre lui et cet ennemi : il le laisse, et retourne à l'hôte.
            g_bannedEnemy = mem::Handle(foe.chr);
            g_enemyBanUntil = g_tick + kEnemyBan;
            LOG("jeu automatique : bloque devant '%s', laisse quatre secondes", mem::Name(foe.chr));
        } else if (toLeader && !onTrail && g_trail.count > 0) {
            // La ligne droite ne passe pas : par où l'hôte est passé.
            g_directBanUntil = g_tick + kDirectBan;
            LOG("jeu automatique : bloque en ligne droite, suit la trace de l'hote (%d point(s))", g_trail.count);
        } else if (toLeader && onTrail) {
            trail::Skip(g_trail);  // ce point de la trace ne s'atteint pas : au suivant
            LOG("jeu automatique : bloque sur la trace, passe au point suivant (%d restant(s))", g_trail.count);
        }
        g_stuck = kStuckDetour + kSampleTicks;  // la suite du compte mène au replacement
    }
    if (g_stuck >= kStuckRejoin && leader && !toLoot) {
        Rejoin(player, leader, "bloque en chemin");
        return 0;
    }
    return wanted;
}

// L'arme : en main devant un ennemi, rangée le reste du temps. Trois règles (0.34) :
//   A. un ennemi est à combattre (en vue, ou à portée) : il la sort, si elle sert ;
//   B. plus d'ennemi depuis une seconde : il la range, et retrouve le sprint, qui n'existe que
//      sans arme en main ;
//   C. elle est vide et il n'a pas de quoi la recharger : il la range aussitôt, ennemi ou non, et
//      se bat à mains nues jusqu'à ce qu'il ait retrouvé des recharges.
// Les gestes sont ceux de la touche d'échange d'arme, par les routines qu'elle appelle. L'arme de
// Barabas ne se range pas (drapeau de sa classe) : vide et sans recharge, il la lâche.
// Rend les touches que cela demande (lâcher).
static uint64_t KeepWeapon(Character* player) {
    void* held = field<void*>(player, off::kChWeaponInHand);
    void* stored = field<void*>(player, off::kChWeaponStored);
    void* carried = held ? held : stored;
    if (!carried) return 0;
    const char* name = mem::WeaponClassName(carried);
    if (Forbidden(name)) return 0;  // Filter la lui fait lâcher
    bool usable = Usable(player, carried);
    bool enemyNow = g_enemyAt == g_tick;
    bool enemyLately = g_tick >= g_enemyAt && g_tick - g_enemyAt <= kHolsterAfter;
    // Mêlée seule : l'arme ne sort jamais, et celle qu'il a en main est rangée quoi qu'il arrive.
    bool wantDrawn = !kMeleeOnly && (g_swapping || (usable && enemyNow));
    bool wantStored = kMeleeOnly || (!g_swapping && (!usable || !enemyLately));

    if (held && wantStored) {
        if (!mem::WeaponHolsterable(held)) {
            if (usable && !kMeleeOnly) return 0;  // elle ne se range pas : il la garde en main tant qu'elle sert
            if (g_tick % 60 == 0) {
                LOG("jeu automatique : lache '%s', %s : elle ne se range pas", name,
                    kMeleeOnly ? "il se bat a mains nues" : "vide et sans recharge");
            }
            return g_tick % 60 < 3 ? mem::kActionDrop : 0;
        }
        if (g_tick < g_weaponAskAt) return 0;
        if (mem::HolsterWeapon(player, true)) {
            g_weaponAskAt = g_tick + kWeaponRetry;
            g_holsters++;
            LOG("jeu automatique : range '%s' (%s)", name,
                kMeleeOnly ? "il se bat a mains nues" : usable ? "plus d'ennemi en vue" : "vide et sans recharge");
        } else {
            g_weaponAskAt = g_tick + kWeaponRefused;
        }
    } else if (!held && stored && wantDrawn) {
        if (g_tick < g_weaponAskAt) return 0;
        if (mem::HolsterWeapon(player, false)) {
            g_weaponAskAt = g_tick + kWeaponRetry;
            g_draws++;
            LOG("jeu automatique : sort '%s' (%s)", name, g_swapping ? "pour la lacher et en prendre une autre" : "ennemi a combattre");
        } else {
            g_weaponAskAt = g_tick + kWeaponRefused;
        }
    }
    return 0;
}

static uint64_t Think(Character* player) {
    g_swapping = false;
    uint64_t wanted = Decide(player);
    return wanted | KeepWeapon(player);
}

static bool Playing(Character* player) {
    return Enabled() && net::Connected() && !mem::IsDead(player) && !revive::Downed() && mem::InputEnabled() &&
           !mem::InCutscene();
}

// Appelé par le moteur chaque tick, avec les touches du joueur. Les actions de jeu (avancer,
// frapper, tirer, souris) sont remplacées par celles du jeu automatique ; le menu, la console et
// les captures restent au joueur, qui garde ainsi de quoi le couper.
static void Filter(Character* player, mem::GameInput& input) {
    g_revive = false;
    if (player != mem::Player() || !Playing(player)) {
        // Suspendu : ce qu'il tenait est relâché, au lieu de rester noté enfoncé.
        if (g_held && player == mem::Player()) {
            input.down &= ~g_held;
            input.pressed &= ~g_held;
            input.released |= g_held;
            input.up = ~input.down;
            g_doing = "suspendu";
            g_held = 0;
        }
        return;
    }
    g_tick++;
    uint64_t wanted = Think(player);

    // 2. Soin et aura. Un hypo dès que la santé n'est plus au plus haut ; et en plein combat, un
    // de plus en pleine forme : la santé passe alors au-dessus du maximum, ce qui donne l'aura du
    // personnage surpuissant et ses coups plus forts. Un à la fois, quatre secondes entre deux,
    // le temps que le précédent ait fini d'agir.
    int32_t health = mem::Health(player), most = static_cast<int32_t>(field<uint32_t>(player, off::kChMaxHealth));
    uint16_t hypos = mem::ReadReserves(player).hypos;
    // L'hypo demandé a-t-il été pris ? Le moteur le refuse pendant un rechargement, ou quand la
    // santé dépasse déjà le maximum ; et un personnage aux mains de l'IA de déplacement n'en prend
    // pas (journal de la 0.30 : cinquante-quatre demandes sans effet, une toutes les quatre
    // secondes). Resté sans effet, il n'est pas redemandé avant vingt secondes.
    if (g_hypoCheckAt && g_tick >= g_hypoCheckAt) {
        g_hypoCheckAt = 0;
        if (hypos >= g_hypoBefore) {
            g_nextHypo = g_tick + kHypoRetry;
            LOG("jeu automatique : l'hypo demande n'a pas ete pris (il en reste %u), pas d'autre essai avant vingt secondes",
                static_cast<unsigned>(hypos));
        }
    }
    if (g_tick < g_hypoUntil) {
        wanted |= mem::kActionHypo;
    } else if (g_tick >= g_nextHypo && most > 0 && hypos > 0) {
        bool low = health * 100 < most * kHealBelow;
        bool boosted = health > most || mem::SuperShown(player) > 0.02f;
        if (low || (g_fighting && !boosted)) {
            g_hypoUntil = g_tick + 3;
            g_nextHypo = g_tick + kHypoTicks;
            g_hypoBefore = hypos;
            g_hypoCheckAt = g_tick + 30;
            g_hypos++;
            LOG("jeu automatique : sante a %d sur %d, un hypo%s", static_cast<int>(health), static_cast<int>(most),
                low ? "" : " pour l'aura");
        }
    }

    // Une arme qu'il ne doit pas tenir (une arme de boss, qu'une prise peut lui mettre en main) :
    // lâchée aussitôt.
    void* inHand = field<void*>(player, off::kChWeaponInHand);
    if (inHand && Forbidden(mem::WeaponClassName(inHand))) {
        if (g_tick % 60 < 3) wanted |= mem::kActionDrop;
        if (g_tick % 60 == 0) LOG("jeu automatique : lache '%s', qu'il ne doit pas tenir", mem::WeaponClassName(inHand));
    }

    // Sprint : dès qu'il a du chemin à faire et rien d'autre en main que la touche « avancer ». Le
    // jeu n'a pas d'endurance : seuls un coup, un tir ou un but tout proche l'en empêchent.
    const uint64_t kBusy = mem::kActionPunch | mem::kActionKick | mem::kActionCrouch | mem::kActionFire;
    mem::Sprint(player, g_sprint && (wanted & mem::kActionForward) && !(wanted & kBusy));

    uint64_t kept = input.down & mem::kActionsUtility;
    input.pressed = (input.pressed & mem::kActionsUtility) | (wanted & ~g_held);
    input.released = (input.released & mem::kActionsUtility) | (g_held & ~wanted);
    input.down = kept | wanted;
    input.up = ~input.down;
    input.mouseX = 0.0f;
    input.mouseY = 0.0f;
    g_held = wanted;
    g_firing = (wanted & mem::kActionFire) != 0;
}

void Update() {
    Character* player = mem::Player();
    if (!player || (!g_firing && !g_pitched) || Playing(player)) return;
    // Le moteur ne relit pas les touches pendant une cinématique : la gâchette qu'il tenait à
    // l'instant où elle commence serait restée enfoncée jusqu'à sa fin.
    if (g_firing) {
        mem::Trigger(player, mem::TriggerEvent::Released);
        LOG("jeu automatique : suspendu la gachette enfoncee, relachee");
    }
    g_firing = false;
    ResetPitch(player);
}

bool Install() {
    static_assert(sizeof(mem::GameInput) == 48, "GameInput : 48 octets dans le moteur");
    return mem::InstallPlayerInputHook(&Filter);
}

bool Enabled() { return cfg::Get().autoPlay && net::CurrentMode() == cfg::Mode::Client; }

bool WantsRevive() { return g_revive; }

void SetEnabled(bool enabled) {
    if (net::CurrentMode() != cfg::Mode::Client) {
        mem::ConsolePrint("OniMP : jeu automatique reserve aux clients");
        return;
    }
    if (cfg::Get().autoPlay == enabled) return;
    cfg::Get().autoPlay = enabled;
    cfg::SaveAutoPlay();
    g_stuck = 0;
    g_following = false;
    LOG("reglage : jeu automatique %s", enabled ? "active" : "desactive");
    mem::ConsolePrint("OniMP : jeu automatique %s", enabled ? "active" : "desactive");
}

void OnLevelChanged() {
    g_held = 0;
    g_stuck = 0;
    g_following = false;
    g_revive = false;
    g_directBanUntil = 0;
    g_enemyBanUntil = 0;
    g_groundAt = -1000;
    g_tick = 0;
    g_hypoUntil = 0;
    g_nextHypo = 0;
    g_said = "";
    g_saidAt = -1000;
    g_onLoot = 0;
    g_pitched = false;
    g_fighting = false;
    g_firing = false;
    g_sprint = false;
    g_hypoCheckAt = 0;
    g_loot = LootTarget();
    g_nextLootScan = 0;
    g_closeHandle = 0;
    g_enemyAt = kNever;
    g_swapping = false;
    g_weaponAskAt = 0;
    g_reloadUntil = 0;
    g_nextReload = 0;
    g_backCheckedAt = kNever;
    g_backClear = false;
    g_backBanUntil = 0;
    g_backSince = 0;
    for (Ban& ban : g_bans) ban = Ban();
    for (Sight& sight : g_sights) sight = Sight();
    trail::Clear(g_trail);
}

void Describe(char* out, int capacity) {
    if (!Enabled()) {
        snprintf(out, static_cast<size_t>(capacity), "jeu automatique : %s",
                 net::CurrentMode() == cfg::Mode::Client ? "coupe" : "reserve aux clients");
    } else {
        snprintf(out, static_cast<size_t>(capacity),
                 "jeu automatique : en marche%s, %s, trace de %d point(s), %u detour(s), %u replacement(s), "
                 "%u objet(s) ramasse(s), %u cache(s), %u hypo(s) pris, %u rayon(s)",
                 kMeleeOnly ? " (corps a corps seulement)" : "", g_doing, g_trail.count, g_detours, g_rejoins, g_pickups,
                 g_hidden, g_hypos, g_rays);
    }
}

void DescribeCombat(char* out, int capacity) {
    if (!Enabled()) {
        if (capacity > 0) out[0] = 0;
        return;
    }
    snprintf(out, static_cast<size_t>(capacity),
             "jeu automatique : %u tick(s) de tir, %u de recul, %u rechargement(s), %u balayage(s), arme rangee %u fois "
             "et sortie %u fois",
             g_shots, g_backTicks, g_reloads, g_sweeps, g_holsters, g_draws);
}

}  // namespace bot
