#include "log.h"

#include <windows.h>

#include <share.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace logx {

static FILE* g_file = nullptr;
static char g_archived[MAX_PATH] = "";

const char* const kArchiveFolder = "onimp_journaux";
static const size_t kArchivesKept = 60;

// Ne garde dans le dossier que les fichiers les plus récents.
static void Prune(const std::string& folder) {
    struct Entry {
        std::string name;
        ULONGLONG written;
    };
    std::vector<Entry> entries;
    WIN32_FIND_DATAA found;
    HANDLE search = FindFirstFileA((folder + "\\*").c_str(), &found);
    if (search == INVALID_HANDLE_VALUE) return;
    do {
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        ULONGLONG written = (static_cast<ULONGLONG>(found.ftLastWriteTime.dwHighDateTime) << 32) | found.ftLastWriteTime.dwLowDateTime;
        entries.push_back({found.cFileName, written});
    } while (FindNextFileA(search, &found));
    FindClose(search);
    if (entries.size() <= kArchivesKept) return;
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.written > b.written; });
    for (size_t i = kArchivesKept; i < entries.size(); i++) DeleteFileA((folder + "\\" + entries[i].name).c_str());
}

// Le journal du lancement précédent porte le même nom que le nôtre. Il était écrasé : après un
// plantage, il suffisait de relancer le jeu pour perdre la seule trace de ce qui s'était passé
// (partie de la 0.32 dans l'aéroport : hôte planté, relancé, plus rien à lire). Il est donc mis de
// côté dans un sous-dossier, daté de sa dernière écriture, avec une copie du debugger.txt du
// moteur, que tous les lancements se partagent et que le suivant vide aussi.
// Un journal qu'une autre instance du jeu tient ouvert ne se laisse pas déplacer : il reste en
// place, et Open échoue ensuite sur lui comme avant.
static void Archive(const char* path) {
    g_archived[0] = 0;
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) return;  // premier lancement
    if (!data.nFileSizeLow && !data.nFileSizeHigh) return;
    const char* slash = strrchr(path, '\\');
    const char* base = slash ? slash + 1 : path;
    std::string gameFolder(path, static_cast<size_t>(base - path));
    std::string folder = gameFolder + kArchiveFolder;
    CreateDirectoryA(folder.c_str(), nullptr);

    FILETIME local;
    SYSTEMTIME at;
    if (!FileTimeToLocalFileTime(&data.ftLastWriteTime, &local) || !FileTimeToSystemTime(&local, &at)) return;
    char stamp[32];
    snprintf(stamp, sizeof(stamp), "_%04u%02u%02u_%02u%02u%02u", at.wYear, at.wMonth, at.wDay, at.wHour, at.wMinute,
             at.wSecond);
    std::string stem(base);
    size_t dot = stem.rfind('.');
    if (dot != std::string::npos) stem.erase(dot);
    std::string name = stem + stamp + ".log";
    if (!MoveFileExA(path, (folder + "\\" + name).c_str(), MOVEFILE_REPLACE_EXISTING)) return;
    snprintf(g_archived, sizeof(g_archived), "%s", name.c_str());
    CopyFileA((gameFolder + "debugger.txt").c_str(), (folder + "\\" + stem + stamp + "_debugger.txt").c_str(), FALSE);
    Prune(folder);
}

bool Open(const char* path) {
    if (g_file) fclose(g_file);
    Archive(path);
    // En écriture pour nous seuls : plusieurs instances du jeu tournent dans le même dossier, et
    // chacune doit garder son journal.
    g_file = _fsopen(path, "w", _SH_DENYWR);
    return g_file != nullptr;
}

const char* Archived() { return g_archived; }

void Write(const char* format, ...) {
    if (!g_file) return;
    SYSTEMTIME now;
    GetLocalTime(&now);
    fprintf(g_file, "[%02u:%02u:%02u.%03u] ", now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
    va_list args;
    va_start(args, format);
    vfprintf(g_file, format, args);
    va_end(args);
    fputc('\n', g_file);
    fflush(g_file);  // le journal doit survivre à un plantage du jeu
}

}  // namespace logx
