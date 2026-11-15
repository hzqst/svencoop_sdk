#ifndef CLOG_H
#define CLOG_H

#include "FileSystem.h"
#include <stdarg.h>
#include <time.h>

class CASLog;

class CLog
{
public:
	enum
	{
		MAX_DIRECTORY_LENGTH = 260 + 1,
		MAX_FILE_NAME_LENGTH = 128 + 1,
		MAX_MESSAGE_LENGTH = 1024,
		MAX_TIME_LENGTH = 256
	};

	CLog(const char *directory, const char *fileName);
	~CLog();

	bool Open(bool limitFiles = false, unsigned int count = 0);
	void Close(void);
	void Printf(const char *format, ...);
	void VPrintf(const char *format, va_list args);
	bool CreateFilename(char *buffer, unsigned int size, tm *time);
	bool CreateFilename(char *buffer, unsigned int size);
	unsigned int LimitToAmount(unsigned int count);

private:
	friend class CASLog;

	char			m_szDirectory[MAX_DIRECTORY_LENGTH];
	char			m_szFileName[MAX_FILE_NAME_LENGTH];
	FileHandle_t	m_pFile;
};

#endif
