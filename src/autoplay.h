// Jeu automatique : un client confie son personnage au mod, qui le fait suivre l'hôte et frapper
// les ennemis qui les menacent. Rien d'autre qu'un joueur de plus au clavier : le mod remplace,
// chaque tick, les touches que le moteur donne au personnage du joueur. Tout le reste (santé,
// coups, armes, réseau) passe par les chemins ordinaires. Réglage de l'onglet Coop du menu F1,
// réservé aux clients, gardé dans onimp.ini.
//
// Depuis la 0.35 il ne se bat plus qu'au corps à corps (kMeleeOnly, autoplay.cpp) : il ne sort, ne
// ramasse ni ne tire aucune arme, ne va plus chercher ni arme ni munitions, et range celle qu'il
// porte. Des points ci-dessous, le 4 n'est donc plus jamais pris, et du 2 il ne reste que les
// hypos : celui dont il a besoin tout de suite à tout moment, les autres quand aucun ennemi n'est
// là. Ce qui suit décrit aussi le comportement de la 0.34, que le réglage remis à faux rendrait.
//
// Ce qu'il fait, par ordre de priorité (0.34) :
//   1. un joueur est à terre, l'hôte ou un autre client : il laisse tout, va à lui et le ranime ;
//      seule exception, sa propre santé sous 20 % avec un ennemi au contact ;
//   2. ce qui traîne, s'il le voit à 120 unités : il change de route aussitôt, combat ou non. Un
//      hypo quand il est mal en point et n'en porte pas, une arme qui serve quand la sienne est
//      vide et sans recharge (la plus forte, et faute de mieux la plus faible), des recharges pour
//      son arme, une arme plus forte, puis le plein : chargeurs, cellules et hypos, tout ce que
//      l'inventaire peut encore porter. L'arme de Barabas est permise, et la plus forte. Arrivé
//      sur un bonus il le ramasse par la routine du moteur ;
//   3. un ennemi collé à lui (20 unités) : corps à corps, avec le balayage (accroupi + pied) pour
//      celui qui est au sol ;
//   4. armé, il tire sur le plus proche des ennemis qu'il voit, jusqu'à 200 unités, jamais quand
//      un coéquipier est sur la ligne de tir. Il n'avance pas vers sa cible, et recule quand elle
//      est à moins de 50 unités (90 avec une arme dont le tir explose), après avoir regardé
//      derrière lui : ni mur, ni vide. Il ne recharge que l'arme vide, s'il porte de quoi ;
//   5. sans arme qui serve : corps à corps contre l'ennemi le plus proche ;
//   6. sinon il reste près de l'hôte : en ligne droite s'il est près et en vue, et par la trace des
//      endroits où l'hôte est passé (trail.h) sinon, ou dès que la ligne droite bute.
// À côté de cela, deux touches qui ne l'arrêtent pas. L'hypo : dès que sa santé passe sous 85 %,
// et en combat un de plus en pleine forme, pour l'aura du personnage surpuissant. L'arme : sortie
// dès qu'un ennemi est à combattre, rangée une seconde après le dernier (il n'y a de sprint que
// sans arme en main), rangée aussitôt quand elle est vide et qu'il n'a pas de quoi la recharger.
// Voir, c'est un rayon du moteur tiré de ses yeux à ce qu'il regarde (mem::CastRay) : il ne tire
// ni ne court vers ce qu'un mur lui cache. Il sprinte dès qu'il a du chemin à faire. Quand il
// n'avance pas : un saut au bout d'une seconde, un autre chemin au bout de deux (la trace au lieu
// de la ligne droite, le point suivant de la trace, l'ennemi ou l'objet laissé de côté), et en
// dernier recours, au bout de sept, le replacement à côté de l'hôte. Il ne cherche pas de chemin
// pour contourner un obstacle.
#pragma once

namespace bot {

// Au chargement de la DLL. Faux si le code du moteur n'est pas celui attendu.
bool Install();
// Le jeu automatique est demandé, et cette machine est un client.
bool Enabled();
// Client seulement ; l'hôte en est averti à l'écran. Gardé dans onimp.ini.
void SetEnabled(bool enabled);
// Le jeu automatique a mené notre joueur près d'un joueur à terre : il le ranime, comme la touche.
bool WantsRevive();
void OnLevelChanged();
// À chaque image : quand le jeu automatique vient d'être suspendu (cinématique, joueur à terre,
// réglage coupé) alors qu'il tenait la gâchette, elle est relâchée. Le moteur ne relit pas les
// touches pendant une cinématique : sans cela l'arme continuait de tirer.
void Update();
void Describe(char* out, int capacity);
// Seconde ligne de mp_status : ce qu'il a fait de son arme. Vide quand il est coupé.
void DescribeCombat(char* out, int capacity);

}  // namespace bot
