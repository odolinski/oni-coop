// Jauge de vie affichée au-dessus d'un personnage : une barre de la couleur de celui qu'elle
// désigne (un joueur selon son numéro, un ennemi, un boss) et, si on le demande, la santé en
// chiffres par-dessus. Sert aussi au compte à rebours d'un joueur à terre.
#pragma once

#include "oni.h"

namespace gauge {

// Couleurs des barres, en 0xAARRGGBB. Joueur : vert pour l'hôte (numéro 0), puis bleu, orange et
// jaune pour les clients, dans l'ordre de leurs numéros.
uint32_t PlayerColor(int player);
constexpr uint32_t kEnemyColor = 0xFFE62828;     // rouge
constexpr uint32_t kBossColor = 0xFFA24BE8;      // violet
constexpr uint32_t kNeutralColor = 0xFFD0D0D0;   // gris clair : ni joueur ni ennemi (alliés, civils)
constexpr uint32_t kReviveColor = 0xFFFFFFFF;    // blanc : réanimation en cours

// À appeler pendant le rendu de la scène. scale : taille de la jauge, 1 pour celle d'un joueur.
// digits : santé et maximum écrits sur la barre. label : texte écrit au-dessus d'elle, ou nul.
void Draw(oni::Character* chr, int32_t health, int32_t maxHealth, float scale, bool digits, uint32_t color,
          const char* label = nullptr);
// Réserves d'un joueur, juste au-dessus de sa jauge : une seule rangée de douze cases carrées,
// petite et centrée, de la largeur de la jauge. Un carré rouge par chargeur de balles, depuis la
// gauche ; un carré vert par cellule d'énergie, depuis la droite ; une case sans carré est vide.
// Si les deux réserves dépassent douze à elles deux, les cases du milieu sont coupées en deux,
// rouge à gauche, vert à droite. scale : celle donnée à Draw pour ce personnage.
void DrawReserves(oni::Character* chr, int clips, int cells, float scale);
// Hauteur que cette rangée prend au-dessus de la jauge, pour une échelle de 1 : à donner à
// DrawNameplate quand elle est tracée.
float ReservesHeight();
constexpr uint32_t kClipColor = 0xFFD9412B;      // rouge : balles
constexpr uint32_t kCellColor = 0xFF58C84A;      // vert : énergie
// Texte au-dessus de la jauge d'un joueur (pseudo, latence), sur le même cartouche que les
// identifiants. lift : hauteur de ce qui est tracé entre la jauge et lui.
void DrawNameplate(oni::Character* chr, const char* text, float scale, uint32_t color, float lift);
// Tracés suivants à travers les murs : rapprochés de la caméra sur la ligne qui mène à leur place,
// à la même taille à l'écran. Pour les jauges des joueurs, que l'on doit pouvoir situer où qu'ils
// soient ; à remettre à faux aussitôt après.
void SetOverlay(bool overlay);
// Étiquette seule : un texte court sur un cartouche sombre, face à la caméra, au-dessus d'un
// personnage ou en un point du monde. Lettres sans accents, chiffres, « _ », « - » et « . » ; les
// minuscules sont écrites en capitales, tout autre signe en « ? ».
void DrawLabel(oni::Character* chr, const char* text, float scale, uint32_t color);
void DrawLabelAt(const oni::Vector3& position, const char* text, float scale, uint32_t color);
constexpr uint32_t kLabelColor = 0xFFFFFFFF;     // blanc
constexpr uint32_t kWeaponLabelColor = 0xFFFFD75E;  // jaune pâle : une arme au sol
// Joueur à terre : à la place de sa jauge, un panneau aux couleurs des écrans du jeu (fond bleu
// nuit, filet bleu clair). Une barre graduée qui se vide avec le temps qui lui reste, de l'ambre
// au rouge, les secondes écrites dessus ; elle se remplit en bleu clair quand c'est nous qui le
// ranimons. Au-dessus : le titre, son pseudo, et un repère qui bat, une croix de secours.
// Dessous : la touche à tenir, quand on est à portée.
struct Downed {
    float seconds;       // temps qu'il lui reste
    float remaining;     // part de ce temps, de 0 à 1
    float progress;      // réanimation en cours, de 0 à 1, qui que ce soit qui ranime ; négatif : personne
    bool reviving;       // quelqu'un le ranime : le temps est arrêté
    float pulse;         // battement du repère, de 0 à 1
    const char* title;   // au-dessus de la barre : « A TERRE », « REANIMATION »
    const char* name;    // son pseudo, au-dessus du titre ; nul : aucun
    const char* hint;    // sous la barre : la touche à tenir ; nul : rien
    bool beacon;         // repère : au loin, garde à l'écran la taille qu'il a de près
};
// playerColor : couleur du joueur, pour son pseudo. Avec SetOverlay, se voit à travers les murs.
void DrawDowned(oni::Character* chr, const Downed& downed, uint32_t playerColor);

// Nouveau niveau chargé : les textures de la jauge ont disparu avec l'ancien.
void OnLevelChanged();

}  // namespace gauge
