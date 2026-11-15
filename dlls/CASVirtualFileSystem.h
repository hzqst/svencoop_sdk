#ifndef CASVIRTUALFILESYSTEM_H
#define CASVIRTUALFILESYSTEM_H

#include "CASDirectoryList.h"
#include "CASExtensionList.h"
#include "CASFileSystemUtils.h"

class CASFile;
class CASFileQuota;
class CASBaseManager;
class CASServerManager;
class CCommand;

class CASVirtualFileSystem : public CASDirectoryList, public CASExtensionList
{
public:
	enum { MINIMUM_WRITE_SPACE = 8 };

	CASVirtualFileSystem();
	~CASVirtualFileSystem();

	void SetFileQuota(unsigned int totalSize, unsigned int fileLimit);
	CASFile *OpenFile(const char *fileName, OpenFileFlags_t flags, ModuleType::Type moduleType);
	void RemoveFile(const char *fileName, ModuleType::Type moduleType);
	void Startup(void);
	void Shutdown(void);
	void MapChange(void);

private:
	friend class CASFile;
	friend void as_fs_quota_usage_Callback( const CCommand &command );
	friend void as_fs_validate_file_usage_Callback( const CCommand &command );
	friend const CASFileQuota *CASVirtualFileSystem_GetFileQuota(const CASVirtualFileSystem *fileSystem);
	friend class CASBaseManager;
	friend class CASServerManager;

	CASFileQuota	*m_pFileQuota;
	unsigned char	m_uiAccessFlags;
};

#endif
