#include "extdll.h"
#include "enginecallback.h"
#include "server_support.h"
#include "ConCommandUtils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

const char *ConCommandKindToString(ConCommandKind::Type kind)
{
	switch (kind)
	{
		case ConCommandKind::ConCommand:
			return "ConCommand";
		case ConCommandKind::ClientCommand:
			return "ClientCommand";
		case ConCommandKind::CVar:
			return "CVar";
		default:
			return "None";
	}
}

ConCommandKind::Type StringToConCommandKind(const char *text)
{
	if (!text || !*text || !stricmp(text, "None"))
		return ConCommandKind::None;

	if (!stricmp(text, "ConCommand"))
		return ConCommandKind::ConCommand;

	if (!stricmp(text, "ClientCommand"))
		return ConCommandKind::ClientCommand;

	if (!stricmp(text, "CVar"))
		return ConCommandKind::CVar;

	return ConCommandKind::None;
}

void ConsoleOutput(edict_s *player, const char *format, ...)
{
	char buffer[4096];
	va_list arguments;

	va_start(arguments, format);
	vsnprintf(buffer, sizeof(buffer), format, arguments);
	va_end(arguments);

	if (player)
		g_engfuncs.pfnClientPrintf(player, print_console, buffer);
	else
		ALERT(at_console, "%s", buffer);
}

void OutputFoundCommand(edict_s *player, const char *prefix, void *command, const char *name,
                        const char *helpInfo, ConCommandKind::Type kind, void (*getValues)(void *, const char *&, const char *&))
{
	if (!UTIL_StringWildcardSearch(name, prefix, false, true) && (!helpInfo || !UTIL_StringWildcardSearch(helpInfo, prefix, false, true)))
		return;

	ConsoleOutput(player, "%s %s", ConCommandKindToString(kind), name);

	if (helpInfo && *helpInfo)
		ConsoleOutput(player, ": %s\n", helpInfo);

	if (kind == ConCommandKind::CVar)
	{
		const char *defaultValue;
		const char *currentValue;

		getValues(command, defaultValue, currentValue);
		ConsoleOutput(player, "Default '%s', current '%s'\n", defaultValue, currentValue);
	}
}

bool UTIL_StringBeginsWith(const char *text, const char *prefix, bool caseSensitive, bool rejectEmpty)
{
	if (!text || !prefix || !*text || (rejectEmpty && !*prefix))
		return false;

	size_t length = strlen(prefix);

	if (strlen(text) < length)
		return false;

	for (size_t i = 0; i < length; i++)
	{
		if (caseSensitive ? text[i] != prefix[i] : toupper(text[i]) != toupper(prefix[i]))
			return false;
	}

	return true;
}

bool UTIL_StringEndsWith(const char *text, const char *suffix, bool caseSensitive, bool allowEmpty)
{
	if (!text || !suffix || !*text || (!allowEmpty && !*suffix))
		return false;

	size_t textLength = strlen(text);
	size_t length = strlen(suffix);

	if (textLength < length)
		return false;

	const char *start = text + textLength - length;

	for (size_t i = 0; i < length; i++)
	{
		if (caseSensitive ? start[i] != suffix[i] : toupper(start[i]) != toupper(suffix[i]))
			return false;
	}

	return true;
}

bool UTIL_StringWildcardSearch(const char *text, const char *pattern, bool caseSensitive, bool emptyOption)
{
	if (!text || !pattern || !*text || !*pattern)
		return false;

	const char *wildcard = strchr(pattern, '*');

	if (!wildcard)
		return false;

	size_t length = strlen(pattern);
	char *part = new char[length];
	bool result = false;

	if (wildcard == pattern)
	{
		UTIL_CopyString(part, pattern + 1, length);
		result = UTIL_StringEndsWith(text, part, caseSensitive, emptyOption);
	}
	else if (wildcard == pattern + length - 1)
	{
		UTIL_CopyString(part, pattern, length);
		result = UTIL_StringBeginsWith(text, part, caseSensitive, emptyOption);
	}

	delete[] part;
	return result;
}
