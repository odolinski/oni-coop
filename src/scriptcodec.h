// Commandes de script (BSL) répliquées de l'hôte vers le client : classement et format réseau.
// Aucun accès au jeu : testable seul (tools/codectest.cpp).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace codec {

enum class Class {
    None,       // exécutée localement, jamais transmise (attentes, requêtes, IA, débogage)
    State,      // modifie l'état du niveau : transmise, et rejouée à un client arrivé en cours
    Transient,  // effet ponctuel (son, dialogue, animation) : transmise, jamais rejouée
    // Exécutée chez l'hôte seulement, jamais rejouée chez un client : sa sauvegarde est celle de la
    // partie. Le client qui la rencontre dans ses propres scripts la saute, comme les autres.
    HostOnly,
};

Class Classify(const char* commandName);

// Mêmes valeurs que sl_type dans le moteur.
enum ArgType : uint8_t { kInt = 0, kString = 1, kFloat = 2, kBool = 3 };

struct Arg {
    uint8_t type = kInt;
    int32_t integer = 0;   // kInt, kBool
    float real = 0.0f;     // kFloat
    std::string text;      // kString
};

struct Command {
    uint32_t levelHash = 0;   // niveau dans lequel l'hôte l'a exécutée
    std::string name;
    std::vector<Arg> args;
};

constexpr int kMaxArgs = 16;

// Faux si la commande ne tient pas dans un message.
bool Encode(const Command& command, std::string& out);
bool Decode(const uint8_t* data, int size, Command& out);

}  // namespace codec
