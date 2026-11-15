#ifndef CASFILEQUOTA_H
#define CASFILEQUOTA_H

#include "CASBaseClass.h"
#include "CASFileSystemUtils.h"
#include "CString.h"
#include <map>
#include <string.h>

class CCommand;

struct CStringLess
{
	bool operator()(const CString &first, const CString &second) const
	{
		return strcmp(first.Data(), second.Data()) < 0;
	}
};

class CASFileQuota : public CASRefCountedBaseClass
{
public:
	CASFileQuota(unsigned int totalSize, unsigned int fileLimit);
	~CASFileQuota();

	void Release(void) const;
	unsigned int GetTotalSize(void) const;
	unsigned int GetSizeInUse(void) const;
	unsigned int GetSizeLeft(void) const;
	bool CanWriteAmount(unsigned int size) const;
	unsigned int SizeLeftAfterWrite(unsigned int size) const;
	void BytesWritten(unsigned int size);
	void BytesRemoved(unsigned int size);
	bool FileLimitReached(void) const;
	unsigned int GetBytesWrittenToFile(const CString &fileName) const;
	void AddWriteFileToList(const CString &fileName, OpenFileFlags_t flags);
	void UpdateBytesWrittenToFile(const CString &fileName, unsigned int size);
	void RemoveWriteFileFromList(const CString &fileName);
	void RemoveFilesInDirectory(const char *directory);
	void RemoveAllFiles(void);
	void ValidateFileList(void);

private:
	friend void as_fs_quota_usage_Callback( const CCommand &command );

	struct FileData_t
	{
		CString			m_szFileName;
		unsigned int	m_uiBytesWritten;
	};

	typedef std::map<CString, FileData_t, CStringLess> FileMap_t;

	unsigned int	m_uiTotalSize;
	unsigned int	m_uiSizeInUse;
	unsigned int	m_uiFileLimit;
	FileMap_t		m_Files;
};

#endif
