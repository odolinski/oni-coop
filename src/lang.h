// Langue du mod : français ou anglais, changée en jeu et prise en compte aussitôt. Chaque texte que
// le mod montre (messages à l'écran, onglets et boutons du menu F1) est écrit une fois dans le
// code, dans sa langue d'origine, et un dictionnaire donne l'autre : les messages sont nés en
// français, les libellés du menu en anglais. Aucun accès au jeu : vérifié par onimp_langtest.
#pragma once

#include <cstdarg>
#include <cstddef>

namespace loc {

enum class Language { French, English };
constexpr int kLanguageCount = 2;

Language Current();
void Set(Language language);
const char* Label(Language language);          // « Francais », « English »
const char* Key(Language language);            // « fr », « en » : ce qu'onimp.ini garde
Language FromKey(const char* key, Language fallback);

// Le texte dans la langue du moment ; lui-même s'il y est déjà, ou si le dictionnaire ne le connaît
// pas. Le résultat vit aussi longtemps que le programme.
const char* Tr(const char* text);

// Comme vsnprintf, dans la langue du moment : le format est traduit, et chaque chaîne donnée à un
// %s aussi quand le dictionnaire la connaît (« active », « Hard »). Les deux versions d'un format
// prennent les mêmes arguments dans le même ordre.
// Deux noms : sur cette plate-forme va_list est un char*, et un seul argument de ce type choisirait
// la mauvaise des deux.
int FormatV(char* out, size_t size, const char* format, va_list args);
int Format(char* out, size_t size, const char* format, ...);

// Pour le test : nombre d'entrées du dictionnaire, la première dont les deux formats ne prennent
// pas les mêmes arguments (nul si toutes concordent), et le nombre de textes qui figurent deux fois
// d'un même côté, donc dont une traduction serait perdue.
int EntryCount();
const char* FirstMismatch();
int Collisions();

}  // namespace loc
