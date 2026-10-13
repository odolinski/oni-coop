// Réglages de partie décidés par l'hôte et suivis par les clients (tir ami, invincibilité, mode de
// difficulté, multiplicateur d'ennemis, civils), et réglages d'affichage propres à chaque joueur
// (santé au-dessus des personnages). Ils se changent depuis des onglets ajoutés au menu de pause du
// jeu (F1), ou depuis la console.
#pragma once

#include <cstdint>

namespace coop {

bool TeamKill();   // les joueurs peuvent se blesser
// Un réglage montré dans le menu F1 a changé ailleurs (pseudo, tenue, cheat, accès donné par
// l'hôte) : l'onglet ouvert se met à jour, et les onglets réservés suivent l'accès.
void Touch();

// Hôte seulement : sans effet chez le client, qui suit les réglages reçus. Gardé dans onimp.ini
// comme les autres règles. L'invincibilité et les munitions illimitées sont depuis la 0.35 des
// cheats de chaque joueur (voir cheat.h).
void SetTeamKill(bool enabled);
// Une session vient de s'ouvrir ou de se fermer. L'hôte y retrouve ces règles telles qu'il les
// avait laissées ; un client attend celles de l'hôte.
void OnSessionStarted(bool hosting);

// Mode de difficulté (rules::Mode) : facile, normal, difficile ou survie. Il fixe la difficulté du
// jeu d'origine dont viennent les ennemis, le temps laissé pour ranimer un joueur à terre, la santé
// qu'il retrouve, et en survie ce que les ennemis ont de plus. Choisi par l'hôte, gardé dans
// onimp.ini, suivi par les clients.
uint8_t Difficulty();
void SetDifficulty(uint8_t mode);    // hôte seulement
// Multiplicateur d'ennemis en vigueur, de 1 à 4 : le choix de l'hôte, relevé au minimum qu'impose
// le nombre de joueurs (x2 à deux, x3 à trois, x4 à quatre).
int EnemyMultiplier();
void SetEnemyMultiplier(int chosen); // hôte seulement
// Des civils se mêlent aux ennemis ; en tuer un fait échouer la mission.
bool Civilians();
void SetCivilians(bool enabled);     // hôte seulement
// Joueurs comptés pour le minimum du multiplicateur, hôte compris.
int Players();

// Difficulté selon le nombre de clients, avec des PNJ partagés (option npcs). L'hôte compte ses
// clients et lit ce que chaque nombre vaut dans son fichier onimp_difficulty.ini ; les clients
// tiennent le résultat de lui. Un client parti compte encore un moment, le temps qu'il recharge
// son niveau. Tout est en pour cent de la valeur d'origine : 100, inchangé. Ce que le mode de
// difficulté ajoute (survie) y est compris.
uint32_t NpcHealthPercent();   // vie des PNJ
// Dégâts des PNJ sur les joueurs : ceux du fichier quand le réglage « dégâts en ligne » de l'hôte
// est actif, 100 sinon. Appliqués sur la machine qui simule les PNJ.
uint32_t NpcDamagePercent();
bool OnlineDamage();
void SetOnlineDamage(bool enabled);  // hôte seulement ; gardé dans onimp.ini
// Une fois par frame : tient le compte des clients à jour. Hôte : relit aussi onimp_difficulty.ini
// quand il vient d'être enregistré.
void Update(double now);
// Hôte : relit onimp_difficulty.ini, modifiable en cours de partie. À chaque chargement de niveau.
void ReloadDifficulty();
// Le jeu et le réseau tournent pendant les menus (voir main.cpp) : le dialogue de pause n'a plus à
// garder la connexion lui-même.
void SetMenusServed(bool served);

// Sur cette machine seulement : les jauges des autres joueurs, celles des PNJ, et les messages du
// mod à l'écran (le journal les garde de toute façon).
bool ShowPlayerHealth();
bool ShowNpcHealth();
bool ShowMessages();
// Identifiants : le nom d'un PNJ au-dessus de lui, celui de la classe d'une arme au sol.
bool ShowNpcIds();
bool ShowWeaponIds();
// Réserves de chaque autre joueur au-dessus de sa jauge : chargeurs de balles, cellules d'énergie.
bool ShowPlayerAmmo();
void SetShowPlayerAmmo(bool shown);
// Latence de chaque autre joueur, écrite à côté de son pseudo.
bool ShowPing();
void SetShowPing(bool shown);
// Jauge, munitions, pseudo et panneau « à terre » des autres joueurs vus à travers les murs.
// Coupé : ils ne se voient plus que lorsque le joueur lui-même est en vue.
bool ShowThroughWalls();
void SetShowThroughWalls(bool shown);
// Portraits des dialogues en bas de l'écran, au-dessus des sous-titres, au lieu du haut.
bool LowPortraits();
void SetLowPortraits(bool low);
// Place à donner à un portrait à la place de celle que demande le script du niveau (voir
// mem::InstallInsertHook) : la rangée du haut et ses abords deviennent la rangée du bas.
uint8_t InsertPlace(uint8_t code);
void SetShowPlayerHealth(bool shown);
void SetShowNpcHealth(bool shown);
void SetShowMessages(bool shown);
void SetShowNpcIds(bool shown);
void SetShowWeaponIds(bool shown);

// Hôte : renvoie les réglages au client qui arrive. Client : les oublie quand l'hôte s'en va.
void OnConnection(uint8_t player, bool connected);
// Hôte : redit ses réglages de partie aux clients.
void Resend();
// Message proto::kReliableSettings, premier octet déjà retiré ; from : son auteur.
void OnReliable(uint8_t from, const uint8_t* data, int size);

// Au chargement de la DLL : ajoute au menu de pause l'onglet « Coop » (que l'hôte règle et que les
// clients consultent), l'onglet « Display », et les onglets « Cheat », « Character » et « Spawn »,
// réservés à l'hôte et aux clients qu'il autorise. Faux si le code du moteur n'est pas celui
// attendu (rien n'est alors modifié).
bool InstallMenuHook();
// Au chargement de la DLL : en session, la difficulté que lit le moteur est celle du mode choisi
// par l'hôte, et non celle des options du jeu. Même réserve.
bool InstallDifficultyHook();
// Commandes de console mp_teamkill, mp_onlinedamage, mp_difficulty, mp_enemies,
// mp_civilians, mp_playerhealth, mp_npchealth, mp_messages, mp_npcids et mp_weaponids.
void RegisterCommands();

}  // namespace coop
