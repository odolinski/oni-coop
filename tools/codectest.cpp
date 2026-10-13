// Test du classement et du format réseau des commandes de script, sans le jeu.
#include <cstdio>
#include <cstring>

#include "../src/scriptcodec.h"

static int g_failures = 0;

static void Check(bool condition, const char* what) {
    if (!condition) {
        g_failures++;
        printf("ECHEC : %s\n", what);
    }
}

static void CheckClass(const char* name, codec::Class expected) {
    if (codec::Classify(name) != expected) {
        g_failures++;
        printf("ECHEC : classement de '%s'\n", name);
    }
}

int main() {
    using codec::Class;

    // Effets de niveau : transmis et rejoués à un client arrivé en cours de route.
    for (const char* name : {"objective_set", "target_set", "door_lock", "door_unlock", "console_activate",
                             "trigvolume_enable", "trig_activate", "turret_activate", "env_show", "obj_create",
                             "ai2_spawn", "ai2_kill", "chr_delete", "chr_teleport", "chr_invincible", "input",
                             "begin_cutscene", "end_cutscene", "cm_interpolate", "cm_reset", "letterbox",
                             "sound_music_start", "particle", "weapon_spawn", "powerup_spawn", "ui_show_element"}) {
        CheckClass(name, Class::State);
    }
    // Effets ponctuels : transmis, jamais rejoués.
    for (const char* name : {"sound_dialog_play", "sound_dialog_play_block", "chr_animate", "chr_animate_block",
                             "chr_envanim", "env_anim", "env_anim_block", "dmsg", "message", "text_console",
                             "timer_start", "win", "lose", "cinematic_start"}) {
        CheckClass(name, Class::Transient);
    }
    // Restent locales : attentes, requêtes, comportement de l'IA, commandes des mods.
    for (const char* name : {"chr_wait_animation", "chr_wait_health", "cm_wait", "trigvolume_count", "chr_has_lsi",
                             "chr_is_player", "ai2_dopath", "ai2_attack", "ai2_passive", "ai2_allpassive",
                             "ai2_setjobstate", "restore_game", "sleep", "fork", "mp_host", "mp_status", "d_regen",
                             "bind", "p3_spawn", "difficulty", "chr_location"}) {
        CheckClass(name, Class::None);
    }

    // Cinématiques : la main du joueur passée à l'IA de déplacement suit l'hôte, dans l'ordre de ses
    // chr_focus ; les ordres donnés à un personnage nommé partent au moment où l'hôte les donne.
    CheckClass("ai2_takecontrol", Class::State);
    CheckClass("chr_focus", Class::State);
    for (const char* name : {"playback", "ai2_movetoflag", "ai2_lookatchar", "ai2_setmovementmode"}) {
        CheckClass(name, Class::Transient);
    }
    // Le film bloquant et ses variantes restent aux scripts de chaque machine.
    for (const char* name : {"playback_block", "playback_debug", "ai2_lookatme", "ai2_followme"}) {
        CheckClass(name, Class::None);
    }

    // Chez l'hôte seulement : un client ne sauvegarde rien de lui-même.
    CheckClass("save_game", Class::HostOnly);

    // Aller-retour du format réseau.
    codec::Command command;
    command.levelHash = 0xD3CAB9F7;
    command.name = "chr_teleport";
    codec::Arg text;
    text.type = codec::kString;
    text.text = "A_t48";
    codec::Arg integer;
    integer.type = codec::kInt;
    integer.integer = -55;
    codec::Arg real;
    real.type = codec::kFloat;
    real.real = 0.125f;
    codec::Arg flag;
    flag.type = codec::kBool;
    flag.integer = 1;
    codec::Arg empty;
    empty.type = codec::kString;
    command.args = {text, integer, real, flag, empty};

    std::string payload;
    Check(codec::Encode(command, payload), "encodage");
    codec::Command decoded;
    Check(codec::Decode(reinterpret_cast<const uint8_t*>(payload.data()), static_cast<int>(payload.size()), decoded),
          "decodage");
    Check(decoded.levelHash == command.levelHash && decoded.name == command.name, "en-tete");
    Check(decoded.args.size() == 5, "nombre d'arguments");
    if (decoded.args.size() == 5) {
        Check(decoded.args[0].type == codec::kString && decoded.args[0].text == "A_t48", "chaine");
        Check(decoded.args[1].type == codec::kInt && decoded.args[1].integer == -55, "entier negatif");
        Check(decoded.args[2].type == codec::kFloat && decoded.args[2].real == 0.125f, "flottant");
        Check(decoded.args[3].type == codec::kBool && decoded.args[3].integer == 1, "booleen");
        Check(decoded.args[4].type == codec::kString && decoded.args[4].text.empty(), "chaine vide");
    }

    // Données abîmées : refusées, sans lecture hors limites.
    for (size_t cut = 0; cut < payload.size(); cut++) {
        codec::Command partial;
        Check(!codec::Decode(reinterpret_cast<const uint8_t*>(payload.data()), static_cast<int>(cut), partial),
              "message tronque accepte");
    }
    std::string padded = payload + "x";
    Check(!codec::Decode(reinterpret_cast<const uint8_t*>(padded.data()), static_cast<int>(padded.size()), decoded),
          "octets en trop acceptes");

    // Commande trop grosse : refusée à l'encodage.
    codec::Command huge;
    huge.name = "dmsg";
    codec::Arg big;
    big.type = codec::kString;
    big.text.assign(250, 'a');
    huge.args.assign(8, big);
    Check(!codec::Encode(huge, payload), "commande trop grosse acceptee");

    printf("%s\n", g_failures == 0 ? "OK" : "ECHEC");
    return g_failures == 0 ? 0 : 1;
}
