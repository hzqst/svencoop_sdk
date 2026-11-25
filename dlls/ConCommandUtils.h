#ifndef CONCOMMANDUTILS_H
#define CONCOMMANDUTILS_H

#include "ConCommandDefs.h"

struct edict_s;

const char *ConCommandKindToString(ConCommandKind::Type kind);
ConCommandKind::Type StringToConCommandKind(const char *text);
void ConsoleOutput(edict_s *player, const char *format, ...);
void OutputFoundCommand(edict_s *player, const char *prefix, void *command, const char *name,
                        const char *helpInfo, ConCommandKind::Type kind, void (*getValues)(void *, const char *&, const char *&));
bool UTIL_StringWildcardSearch(const char *text, const char *pattern, bool caseSensitive, bool emptyOption);
bool UTIL_StringBeginsWith(const char *text, const char *prefix, bool caseSensitive, bool ignoreSpaces);
bool UTIL_StringEndsWith(const char *text, const char *suffix, bool caseSensitive, bool ignoreSpaces);

#endif
