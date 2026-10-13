// Session UDP en étoile (WinSock2, socket non bloquant, interrogé depuis le thread du jeu) : un
// hôte, jusqu'à trois clients. Les clients ne se parlent pas entre eux : l'hôte redistribue.
#pragma once

#include "config.h"
#include "protocol.h"

namespace net {

// Les joueurs sont désignés par leur numéro : 0 pour l'hôte, 1 à 3 pour les clients.
struct Handlers {
    // État d'un autre joueur ; son numéro est dans state.characterID.
    void (*onPlayer)(const proto::EntityState& state, uint32_t levelHash);
    void (*onNpcSnapshot)(const proto::EntityState* states, int count, uint32_t levelHash);
    void (*onNpcInfo)(const proto::NpcInfo* infos, int count);
    void (*onEvent)(uint8_t from, const proto::Event& event, uint32_t levelHash);
    // Hôte : le client player arrive ou s'en va. Client : player vaut 0, l'hôte.
    void (*onConnection)(uint8_t player, bool connected);
    // Flux ordonné, voir SendReliable. from : auteur du message, qui n'est pas forcément la
    // machine qui nous l'a remis (voir shared).
    void (*onReliable)(uint8_t from, const uint8_t* payload, int size);
    // Ce que le joueur doit savoir : partie trouvée, refus de l'hôte, Radmin VPN absent.
    void (*onNotice)(const char* text);
    // Hôte : vrai si un message fiable de ce genre (son premier octet), reçu d'un client, concerne
    // les autres joueurs. L'hôte le redistribue alors aux autres clients, son auteur restant ce
    // client. Nul : rien n'est redistribué.
    bool (*shared)(uint8_t kind);
};

// Adresse d'hôte qui demande la recherche automatique (voir Start).
constexpr const char* kAutoHost = "auto";

// À appeler avant Start. gameName : nom annoncé par l'hôte. joinName : le client ne rejoint que
// la partie de ce nom ; vide, la première trouvée. interfaceSpec : réseau où l'hôte s'annonce et
// d'où chacun accepte l'autre :
//   "auto"    Radmin VPN (26.0.0.0/8) s'il est là, sinon toutes les interfaces
//   "radmin"  Radmin VPN seulement
//   "any"     toutes les interfaces
//   a.b.c.d   l'interface qui porte cette adresse
void Configure(const char* gameName, const char* joinName, const char* modVersion, const char* interfaceSpec);
// Niveau chargé ici, pour l'annonce de l'hôte.
void SetLevelName(const char* name);

// Hôte : écoute sur port, et s'annonce sur port + 1. Client : vise host:port ; si host vaut
// kAutoHost, attend l'annonce d'un hôte et le rejoint. Remplace la session en cours.
bool Start(cfg::Mode mode, const char* host, uint16_t port);
void Stop();

// Client : rompt la connexion au prochain Poll et en ouvre une nouvelle. L'hôte y voit un client
// qui arrive : il lui renvoie tout ce qu'il envoie à un nouveau venu. Sans effet sinon.
void RequestReconnect();

bool Running();
bool Connected();               // hôte : au moins un client ; client : l'hôte répond
bool Connected(uint8_t player); // hôte : ce client est là ; client : seul l'hôte, 0, peut l'être
int ConnectedCount();           // machines reliées à celle-ci
bool Searching();               // client : aucun hôte trouvé pour l'instant
cfg::Mode CurrentMode();
// Numéro de notre joueur. Client : celui que l'hôte a donné à la connexion, 1 d'ici là.
uint8_t LocalPlayerId();
// Aller-retour mesuré avec ce joueur, en millisecondes, lissé ; négatif si inconnu. Hôte : avec
// chaque client. Client : avec l'hôte seulement, par qui tout passe.
int PingMs(uint8_t player);
void Status(char* out, int capacity);

// Reçoit tout ce qui est en attente, gère annonces, poignée de main, délais et renvois.
void Poll(double now, uint32_t levelHash, const Handlers& handlers);
// Hôte, quand Poll ne tourne plus (menu de pause) : le réseau reste servi. Ce qui vient des
// clients est accusé et redistribué aux autres comme d'habitude ; ce qui était pour l'hôte est
// gardé, et remis à ses fonctions au prochain Poll. Seules les arrivées et les départs attendent.
void PumpRelay(double now);

// Notre joueur : à l'hôte pour un client, à tous les clients pour l'hôte.
void SendPlayer(const proto::EntityState& state, uint32_t levelHash);
// Hôte : à tous les clients.
void SendNpcSnapshot(const proto::EntityState* states, int count, uint32_t levelHash);
void SendNpcInfo(const proto::NpcInfo* infos, int count, uint32_t levelHash);
// Client : à l'hôte.
void SendEvent(uint8_t kind, uint32_t characterID, int32_t value, uint32_t levelHash);

// Flux fiable et ordonné, un par liaison : chaque message arrive une fois, dans l'ordre d'émission.
// La file d'une liaison est vidée à chaque (dé)connexion. Client : à l'hôte. Hôte : à tous les
// clients. Faux si personne n'est là, ou message trop gros.
bool SendReliable(const void* payload, int size);
// Hôte : à ce seul client. Client : player ne peut être que l'hôte.
bool SendReliableTo(uint8_t player, const void* payload, int size);
int ReliableBacklog();     // messages pas encore accusés, toutes liaisons confondues

}  // namespace net
