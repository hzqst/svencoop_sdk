#ifndef CLIENT_SUPPORT_H
#define CLIENT_SUPPORT_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

char *UTIL_CopyString(char *destination, const char *source, unsigned int size);
char *UTIL_AppendString(char *destination, const char *source, unsigned int size);
int UTIL_FormatString(char *destination, unsigned int size, const char *format, ...);
char *UTIL_FixSlashes(char *path);
void UTIL_FormatNetAdr(char *destination, unsigned int size, struct netadr_s *address);

// Clear the buffer and format a null-terminated string
template <unsigned int size>
int UTIL_FormatString(char (&destination)[size], const char *format, ...)
{
	if (!format)
		return 0;

	unsigned int formatLength = strlen(format);

	memset(destination, 0, size);

	if (!formatLength)
		return 0;

	va_list arguments;

	va_start(arguments, format);
	int result = vsnprintf(destination, size, format, arguments);
	va_end(arguments);

	destination[size - 1] = 0;
	return result;
}

char *READ_ALLOC_STRING(void);
char *UTIL_GetMapName(void);
void StripMapFromPFN(const char *source, char *destination);
char *GetTopSubdir(const char *path);
const char *GetIpAddr(void);
void UTIL_HostError(const char *message, int status);
#ifdef _WIN32
struct HINSTANCE__ *LoadGameLibrary(const char *name);
#endif
char *TrimString(char *text);

#endif
