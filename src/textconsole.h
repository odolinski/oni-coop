// Écrans de texte du niveau : la commande de script text_console (console de données, consigne,
// message de l'état-major), un dialogue plein cadre que le joueur referme lui-même.
//
// Dans le jeu d'origine la commande ouvre le dialogue sur place et n'en revient qu'à sa fermeture :
// la boucle des dialogues tourne à l'intérieur de l'interpréteur de scripts, donc de la
// simulation. En session le jeu continue derrière les dialogues (voir main.cpp) : la simulation
// repartait alors de l'intérieur d'elle-même, et son ordonnanceur de scripts, trouvant le script de
// l'écran ni endormi ni terminé, le relançait depuis le début sur le même contexte puis le
// libérait. À la fermeture, le premier passage reprenait sur un contexte vidé : le script
// s'arrêtait n'importe où, souvent avant de rendre les commandes qu'il venait de couper
// (« input 0 » puis l'écran puis « input 1 »), chez l'hôte comme chez les clients, qui suivent ses
// scripts. Le jeu semblait figé.
//
// En session la commande ne fait donc plus que noter l'écran demandé et rend la main aussitôt :
// le script continue. L'écran est ouvert à l'image suivante depuis la boucle du jeu, hors de la
// simulation, comme le moteur ouvre lui-même son menu de pause. Hors session rien ne change.
#pragma once

namespace textcon {

// Au chargement de la DLL. Faux si le code du moteur n'est pas celui attendu (rien n'est alors
// modifié).
bool Install();
// Les écrans demandés dans le niveau quitté ne valent plus.
void OnLevelChanged();
// Hôte, une fois par frame, après le relevé des consoles : décide pour qui est chaque écran que ses
// scripts viennent de demander. Celui d'une console de données n'est que pour le joueur qui vient
// de l'utiliser ; les autres sont pour tous.
void Resolve(double now);
// Une fois par image, depuis la boucle du jeu et hors de notre propre boucle : ouvre l'écran
// demandé, et ne revient qu'à sa fermeture.
void Update();
// État pour mp_status.
void Describe(char* out, int capacity);

}  // namespace textcon
