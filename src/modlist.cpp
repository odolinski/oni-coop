#include "modlist.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>

namespace mods {

static void SortUnique(std::vector<uint32_t>& ids) {
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
}

std::vector<uint32_t> ParseInstalled(const std::string& xml) {
    std::vector<uint32_t> ids;
    // Le fichier liste aussi les niveaux touchés (<affectedLevels>) : seul le bloc <mods> compte.
    size_t begin = xml.find("<mods>");
    size_t end = xml.find("</mods>");
    if (begin == std::string::npos || end == std::string::npos || end < begin) return ids;

    static const char kOpen[] = "<int>";
    for (size_t at = xml.find(kOpen, begin); at != std::string::npos && at < end; at = xml.find(kOpen, at)) {
        at += sizeof(kOpen) - 1;
        const char* text = xml.c_str() + at;
        char* stop = nullptr;
        unsigned long value = strtoul(text, &stop, 10);
        if (stop != text && strncmp(stop, "</int>", 6) == 0) ids.push_back(static_cast<uint32_t>(value));
    }
    SortUnique(ids);
    return ids;
}

bool ReadInstalled(const std::string& gameDir, std::vector<uint32_t>& ids) {
    std::string path = gameDir + "GameDataFolder\\installed_mods.xml";
    FILE* file = fopen(path.c_str(), "rb");
    if (!file) return false;
    std::string xml;
    char buffer[4096];
    size_t read;
    while ((read = fread(buffer, 1, sizeof(buffer), file)) > 0 && xml.size() < (1u << 20)) xml.append(buffer, read);
    fclose(file);
    ids = ParseInstalled(xml);
    return true;
}

bool SplitPackageFolder(const char* folder, uint32_t& id, std::string& name) {
    static const int kDigits = 5;
    uint32_t number = 0;
    for (int i = 0; i < kDigits; i++) {
        if (folder[i] < '0' || folder[i] > '9') return false;
        number = number * 10 + static_cast<uint32_t>(folder[i] - '0');
    }
    // L'AE Installer remplace par « _ » tout ce qui n'est ni lettre ni chiffre.
    std::string text;
    for (const char* c = folder + kDigits; *c; c++) {
        bool space = *c == '_' || *c == ' ';
        if (!space) text += *c;
        else if (!text.empty() && text.back() != ' ') text += ' ';
    }
    while (!text.empty() && text.back() == ' ') text.pop_back();
    id = number;
    name = text;
    return true;
}

std::map<uint32_t, std::string> ReadPackageNames(const std::string& gameDir) {
    std::map<uint32_t, std::string> names;
    WIN32_FIND_DATAA found;
    HANDLE search = FindFirstFileA((gameDir + "AEInstaller\\packages\\*").c_str(), &found);
    if (search == INVALID_HANDLE_VALUE) return names;
    do {
        uint32_t id;
        std::string name;
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && SplitPackageFolder(found.cFileName, id, name)) {
            names[id] = name;
        }
    } while (FindNextFileA(search, &found));
    FindClose(search);
    return names;
}

std::vector<uint32_t> Missing(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    std::vector<uint32_t> missing;
    std::set_difference(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(missing));
    return missing;
}

uint32_t Fingerprint(const std::vector<uint32_t>& ids) {
    uint32_t hash = 2166136261u;  // FNV-1a
    for (uint32_t id : ids) {
        for (int shift = 0; shift < 32; shift += 8) hash = (hash ^ ((id >> shift) & 0xFF)) * 16777619u;
    }
    return hash;
}

std::string Label(const std::vector<uint32_t>& ids, const std::map<uint32_t, std::string>& names, int limit) {
    if (ids.empty()) return "aucun";
    std::string label;
    int listed = 0;
    for (uint32_t id : ids) {
        if (limit > 0 && listed == limit) break;
        if (listed++ > 0) label += ", ";
        label += std::to_string(id);
        auto name = names.find(id);
        if (name != names.end() && !name->second.empty()) label += " " + name->second;
    }
    int left = static_cast<int>(ids.size()) - listed;
    if (left > 0) label += " (+" + std::to_string(left) + ")";
    return label;
}

int Encode(const std::vector<uint32_t>& ids, uint8_t* out, int capacity) {
    int size = 2 + 4 * static_cast<int>(ids.size());
    if (ids.size() > static_cast<size_t>(kMaxListed) || size > capacity) return 0;
    uint16_t count = static_cast<uint16_t>(ids.size());
    memcpy(out, &count, 2);
    if (count) memcpy(out + 2, ids.data(), 4 * static_cast<size_t>(count));
    return size;
}

bool Decode(const uint8_t* data, int size, std::vector<uint32_t>& ids) {
    uint16_t count;
    if (size < 2) return false;
    memcpy(&count, data, 2);
    if (count > kMaxListed || size < 2 + 4 * static_cast<int>(count)) return false;
    ids.resize(count);
    if (count) memcpy(ids.data(), data + 2, 4 * static_cast<size_t>(count));
    SortUnique(ids);
    return true;
}

}  // namespace mods
