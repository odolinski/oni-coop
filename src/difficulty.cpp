#include "difficulty.h"

namespace difficulty {

// Valeurs de repli seulement. Pour changer la difficulté, modifier onimp_difficulty.ini à côté
// d'Oni.exe : chaque ligne qui s'y trouve passe avant ce qui est écrit ici, sans recompiler.
Table DefaultTable() {
    Table table = {};
    for (int clients = 0; clients <= kMaxClients; clients++) {
        table.health[clients] = 100 + 100 * static_cast<uint32_t>(clients);
        table.damage[clients] = 100 + 5 * static_cast<uint32_t>(clients);
    }
    return table;
}

static uint32_t Clamp(uint32_t value, uint32_t low, uint32_t high) {
    return value < low ? low : (value > high ? high : value);
}

Table Sanitized(Table table) {
    table.health[0] = 100;
    table.damage[0] = 100;
    for (int clients = 1; clients <= kMaxClients; clients++) {
        table.health[clients] = Clamp(table.health[clients], kMinHealthPercent, kMaxHealthPercent);
        table.damage[clients] = Clamp(table.damage[clients], 0, kMaxDamagePercent);
    }
    return table;
}

static uint32_t Index(uint32_t clients) { return clients > kMaxClients ? kMaxClients : clients; }

uint32_t HealthPercent(const Table& table, uint32_t clients) { return table.health[Index(clients)]; }
uint32_t DamagePercent(const Table& table, uint32_t clients) { return table.damage[Index(clients)]; }

void SetPerClientBonus(uint32_t* values, uint32_t bonus) {
    for (int clients = 0; clients <= kMaxClients; clients++) {
        values[clients] = 100 + bonus * static_cast<uint32_t>(clients);
    }
}

int PerClientBonus(const uint32_t* values) {
    if (values[1] < 100) return -1;
    uint32_t bonus = values[1] - 100;
    for (int clients = 2; clients <= kMaxClients; clients++) {
        if (values[clients] != 100 + bonus * static_cast<uint32_t>(clients)) return -1;
    }
    return static_cast<int>(bonus);
}

bool ClientCount::Update(uint32_t wanted, bool immediate, double now) {
    if (wanted == value_) {
        lowering_ = false;
        return false;
    }
    if (wanted < value_ && !immediate) {
        if (!lowering_) {
            lowering_ = true;
            since_ = now;
        }
        if (now - since_ < kGrace) return false;
    }
    lowering_ = false;
    value_ = wanted;
    return true;
}

uint32_t ScaledMax(uint32_t baseMax, uint32_t percent) {
    uint64_t scaled = static_cast<uint64_t>(baseMax) * percent / 100;
    if (scaled < 1) return 1;
    return scaled > 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32_t>(scaled);
}

void Rescale(uint32_t& maxHealth, uint32_t& health, uint32_t baseMax, uint32_t percent, bool dead) {
    uint32_t newMax = ScaledMax(baseMax, percent);
    if (!dead && maxHealth > 0) {
        health = static_cast<uint32_t>((static_cast<uint64_t>(health) * newMax + maxHealth - 1) / maxHealth);
    }
    if (health > newMax) health = newMax;
    maxHealth = newMax;
}

uint32_t ScaleDamage(uint32_t damage, uint32_t percent, uint32_t& carry) {
    if (percent == 100 || !damage) return damage;
    uint64_t total = static_cast<uint64_t>(damage) * percent + carry;
    carry = static_cast<uint32_t>(total % 100);
    total /= 100;
    return total > 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32_t>(total);
}

}  // namespace difficulty
