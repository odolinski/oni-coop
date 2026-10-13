#include "scriptcodec.h"

#include <cstring>

namespace codec {

namespace {

struct Rule {
    const char* pattern;   // nom exact, ou préfixe si terminé par '*'
    Class cls;
};

// Première règle qui correspond. Tout ce qui n'est pas listé reste local.
const Rule kRules[] = {
    // --- Jamais transmises, bien que leur préfixe le soit ---
    {"cm_wait", Class::None},
    {"trigvolume_count", Class::None},
    {"chr_wait_*", Class::None},
    {"chr_has_*", Class::None},
    {"chr_is_player", Class::None},
    {"chr_location", Class::None},          // lecture ou écriture de position : géré par la synchro
    {"chr_location_settocamera", Class::None},
    {"chr_debug_*", Class::None},
    {"chr_display_combat_stats", Class::None},
    {"chr_draw_dot", Class::None},
    {"chr_who", Class::None},
    {"chr_health", Class::None},
    {"chr_max_health", Class::None},
    {"chr_main_class", Class::None},
    {"chr_kill_all_ai", Class::None},
    {"sound_list_broken_links", Class::None},
    {"sound_objects_reset", Class::None},
    {"ui_show_help", Class::None},

    // --- Chez l'hôte seulement ---
    {"save_game", Class::HostOnly},

    // --- Effets ponctuels ---
    {"sound_dialog_*", Class::Transient},
    {"sound_impulse_*", Class::Transient},
    // Ordres donnés à un personnage nommé pendant une cinématique : film de ses gestes, marche vers
    // un repère, regard. L'hôte les donne à tous ; un client ne les suit que pour son propre joueur
    // (ses PNJ sont déjà pilotés par l'hôte), au moment où l'hôte les donne.
    {"playback", Class::Transient},
    {"ai2_movetoflag", Class::Transient},
    {"ai2_lookatchar", Class::Transient},
    {"ai2_setmovementmode", Class::Transient},
    {"chr_animate*", Class::Transient},
    {"chr_envanim*", Class::Transient},
    {"chr_playback*", Class::Transient},
    {"chr_talk", Class::Transient},
    {"chr_vocalize", Class::Transient},
    {"chr_pain", Class::Transient},
    {"env_anim*", Class::Transient},
    {"dmsg", Class::Transient},
    {"dprint", Class::Transient},
    {"message*", Class::Transient},
    {"splash_screen", Class::Transient},
    {"text_console", Class::Transient},
    {"timer_*", Class::Transient},
    {"cutscene_sync", Class::Transient},
    {"cinematic_*", Class::Transient},
    {"win", Class::Transient},
    {"lose", Class::Transient},

    // --- État du niveau ---
    {"objective_*", Class::State},
    {"target_set", Class::State},
    {"door_*", Class::State},
    {"console_*", Class::State},
    {"trig_*", Class::State},
    {"trigvolume_*", Class::State},
    {"turret_*", Class::State},
    {"env_*", Class::State},
    {"obj_*", Class::State},
    {"ai2_spawn*", Class::State},
    {"ai2_kill", Class::State},
    // Le personnage du joueur passé aux mains de l'IA de déplacement, puis rendu : au moment où
    // l'hôte le fait, et dans le même ordre que ses chr_focus. Laissée aux scripts d'un client,
    // qui courent en avance sur l'hôte (rien ne les y bloque), la commande qui rend la main
    // tombait pendant un chr_focus et la rendait à un autre : le joueur du client restait figé.
    {"ai2_takecontrol", Class::State},
    {"chr_*", Class::State},
    {"input", Class::State},
    {"lock_keys", Class::State},
    {"begin_cutscene", Class::State},
    {"end_cutscene", Class::State},
    {"letterbox", Class::State},
    {"fade_in", Class::State},
    {"fade_out", Class::State},
    {"cm_*", Class::State},
    {"gs_*", Class::State},
    {"gl_fog_*", Class::State},
    {"sound_music_*", Class::State},
    {"sound_ambient_*", Class::State},
    {"particle", Class::State},
    {"weapon_spawn", Class::State},
    {"powerup_spawn", Class::State},
    {"give_powerup", Class::State},
    {"ui_*", Class::State},
};

bool Matches(const char* pattern, const char* name) {
    size_t length = strlen(pattern);
    if (length > 0 && pattern[length - 1] == '*') return strncmp(pattern, name, length - 1) == 0;
    return strcmp(pattern, name) == 0;
}

void PutU8(std::string& out, uint8_t value) { out.push_back(static_cast<char>(value)); }

void PutU32(std::string& out, uint32_t value) {
    char bytes[4];
    memcpy(bytes, &value, 4);
    out.append(bytes, 4);
}

bool PutText(std::string& out, const std::string& text) {
    if (text.size() > 255) return false;
    PutU8(out, static_cast<uint8_t>(text.size()));
    out.append(text);
    return true;
}

struct Reader {
    const uint8_t* data;
    int size;
    int position = 0;

    bool U8(uint8_t& value) {
        if (position + 1 > size) return false;
        value = data[position++];
        return true;
    }
    bool U32(uint32_t& value) {
        if (position + 4 > size) return false;
        memcpy(&value, data + position, 4);
        position += 4;
        return true;
    }
    bool Text(std::string& text) {
        uint8_t length;
        if (!U8(length) || position + length > size) return false;
        text.assign(reinterpret_cast<const char*>(data + position), length);
        position += length;
        return true;
    }
};

}  // namespace

Class Classify(const char* commandName) {
    for (const Rule& rule : kRules) {
        if (Matches(rule.pattern, commandName)) return rule.cls;
    }
    return Class::None;
}

bool Encode(const Command& command, std::string& out) {
    out.clear();
    if (command.args.size() > kMaxArgs) return false;
    PutU32(out, command.levelHash);
    if (!PutText(out, command.name)) return false;
    PutU8(out, static_cast<uint8_t>(command.args.size()));
    for (const Arg& arg : command.args) {
        PutU8(out, arg.type);
        switch (arg.type) {
            case kString:
                if (!PutText(out, arg.text)) return false;
                break;
            case kFloat: {
                uint32_t bits;
                memcpy(&bits, &arg.real, 4);
                PutU32(out, bits);
                break;
            }
            case kInt:
            case kBool:
                PutU32(out, static_cast<uint32_t>(arg.integer));
                break;
            default:
                return false;
        }
    }
    return out.size() <= 1000;
}

bool Decode(const uint8_t* data, int size, Command& out) {
    Reader reader = {data, size};
    uint8_t count;
    if (!reader.U32(out.levelHash) || !reader.Text(out.name) || !reader.U8(count)) return false;
    if (out.name.empty() || count > kMaxArgs) return false;

    out.args.clear();
    for (int i = 0; i < count; i++) {
        Arg arg;
        uint32_t bits = 0;
        if (!reader.U8(arg.type)) return false;
        switch (arg.type) {
            case kString:
                if (!reader.Text(arg.text)) return false;
                break;
            case kFloat:
                if (!reader.U32(bits)) return false;
                memcpy(&arg.real, &bits, 4);
                break;
            case kInt:
            case kBool:
                if (!reader.U32(bits)) return false;
                arg.integer = static_cast<int32_t>(bits);
                break;
            default:
                return false;
        }
        out.args.push_back(arg);
    }
    return reader.position == size;
}

}  // namespace codec
