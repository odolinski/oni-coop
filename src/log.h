#pragma once

// Vérification des formats printf par GCC ; sans effet avec MSVC.
#if defined(__GNUC__)
#define ONIMP_PRINTF(formatIndex, firstArg) __attribute__((format(printf, formatIndex, firstArg)))
#else
#define ONIMP_PRINTF(formatIndex, firstArg)
#endif

namespace logx {

// Faux si le fichier ne peut pas être ouvert, ou si une autre instance du jeu y écrit déjà. Le
// journal du lancement précédent, s'il y en a un, est d'abord mis de côté dans le sous-dossier
// kArchiveFolder, sous un nom daté : il n'est plus écrasé.
bool Open(const char* path);
extern const char* const kArchiveFolder;
const char* Archived();   // nom sous lequel le journal précédent a été gardé ; vide s'il n'y en avait pas
void Write(const char* format, ...) ONIMP_PRINTF(1, 2);

}  // namespace logx

#define LOG(...) ::logx::Write(__VA_ARGS__)
