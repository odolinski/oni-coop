// Difficulté selon le nombre de clients (sans accès au jeu) : ce que la vie et les dégâts des PNJ
// deviennent quand un, deux ou trois clients sont là, et combien de clients compter à un instant
// donné. Les valeurs viennent du fichier onimp_difficulty.ini de l'hôte.
#pragma once

#include <cstdint>

namespace difficulty {

constexpr int kMaxClients = 3;

// Tout est en pour cent de la valeur d'origine du jeu : 100 la laisse telle quelle.
constexpr uint32_t kMinHealthPercent = 10;
constexpr uint32_t kMaxHealthPercent = 2000;
constexpr uint32_t kMaxDamagePercent = 1000;

// Par nombre de clients connectés. La case 0, l'hôte seul, vaut toujours 100.
struct Table {
    uint32_t health[kMaxClients + 1];   // vie des PNJ
    uint32_t damage[kMaxClients + 1];   // dégâts des PNJ sur les joueurs
};

// La vie d'origine une fois de plus par client, 5 % de dégâts de plus par client. Ce que vaut un
// fichier absent, ou une ligne qui y manque : la difficulté ne se règle pas ici mais dans le
// fichier, que l'hôte relit dès qu'il est enregistré.
Table DefaultTable();
// Ramène chaque valeur dans ses bornes, et la case 0 à 100.
Table Sanitized(Table table);
uint32_t HealthPercent(const Table& table, uint32_t clients);
uint32_t DamagePercent(const Table& table, uint32_t clients);

// Réglage en jeu, par un curseur : une seule valeur, ce que chaque client ajoute, en pour cent de
// l'origine. La ligne de la table (vie ou dégâts) qui en sort monte d'autant à chaque client :
// 100 + bonus pour un, 100 + 2 × bonus pour deux, 100 + 3 × bonus pour trois.
void SetPerClientBonus(uint32_t* values, uint32_t bonus);
// Bonus d'une ligne de cette forme ; -1 si elle n'en est pas, écrite à la main dans le fichier.
int PerClientBonus(const uint32_t* values);

// Un client qui recharge son niveau se déconnecte un instant : il compte encore ce temps-là.
constexpr double kGrace = 20.0;

// Nombre de clients retenu pour la difficulté. Une hausse est prise tout de suite ; une baisse
// seulement si elle tient kGrace secondes, pour qu'une reconnexion ne fasse pas varier la vie des
// PNJ.
class ClientCount {
public:
    // wanted : clients présents à l'instant now. immediate : une baisse est prise sans attendre.
    // Vrai si le nombre retenu vient de changer.
    bool Update(uint32_t wanted, bool immediate, double now);
    uint32_t value() const { return value_; }

private:
    uint32_t value_ = 0;
    bool lowering_ = false;   // wanted est sous value_ depuis since_
    double since_ = 0.0;
};

// Maximum de vie d'un PNJ dont l'original est baseMax ; jamais nul.
uint32_t ScaledMax(uint32_t baseMax, uint32_t percent);
// Porte la vie d'un PNJ à percent pour cent de celle d'origine : son maximum, et sa santé du moment
// dans la même proportion, arrondie vers le haut pour qu'une baisse ne tue pas un PNJ presque
// mort. Un PNJ mort reste à zéro.
void Rescale(uint32_t& maxHealth, uint32_t& health, uint32_t baseMax, uint32_t percent, bool dead);

// Dégâts portés à percent pour cent. Un coup fait rarement plus de vingt points : 5 % de plus se
// perdraient à l'arrondi. carry garde donc, en centièmes de point, ce qui n'a pas encore été
// infligé, pour le coup suivant.
uint32_t ScaleDamage(uint32_t damage, uint32_t percent, uint32_t& carry);

}  // namespace difficulty
