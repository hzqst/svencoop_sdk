#ifndef ASFILESYSTEM_H
#define ASFILESYSTEM_H

#include "CASDirectoryList.h"

class CASVirtualFileSystem;
class CASDocumentation;
class CKeyvaluesWriter;
struct cvar_s;

namespace FileAccess
{
	enum Type
	{
		NONE = -1,
		READ,
		WRITE
	};
}

namespace DirectoryFlag
{
	enum Type
	{
		NONE = -1,
		IMPLICIT_SUBDIRECTORIES
	};
}

const char *ModuleTypeToString(ModuleType::Type type);
ModuleType::Type StringToModuleType(const char *text);
const char *FileAccessToString(FileAccess::Type access);
FileAccess::Type StringToFileAccess(const char *text);
const char *DirectoryFlagToString(DirectoryFlag::Type flag);
DirectoryFlag::Type StringToDirectoryFlag(const char *text);
bool ModuleTypesToKeyvaluesBlock(CKeyvaluesWriter &writer, unsigned char types,
	const char *blockName = "modules", const char *keyName = "module");
unsigned long long ParseByteSizeString(const char *text);
unsigned int ASComputeFileQuota(void);

class CWriteFileSystemToFile
{
public:
	CWriteFileSystemToFile(CKeyvaluesWriter &writer, bool writeDetails = true);

	bool Write(void);
	void operator()( unsigned int level, const CASDirectory *directory, const char *path );

private:

	CKeyvaluesWriter	*m_pWriter;
	bool			m_bWriteDetails;
	unsigned int	m_uiLevel;
};

extern cvar_s as_file_size_quota;

void RegisterSCScriptFileSystem(CASDocumentation *documentation);

#endif
