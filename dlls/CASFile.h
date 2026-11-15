#ifndef CASFILE_H
#define CASFILE_H

#include "CASBaseClass.h"
#include "CASFileSystemUtils.h"
#include "CString.h"
#include "FileSystem.h"

class CASBLOB;
class CASDirectory;
class CASVirtualFileSystem;

class CASFile : public CASRefCountedBaseClass
{
public:
	CASFile(CASVirtualFileSystem *fileSystem, const CASDirectory *directory, const char *fileName,
		OpenFileFlags_t flags, FileHandle_t file, unsigned int bytesWritten);
	~CASFile();

	void Release(void) const;
	bool IsOpen(void) const;
	void Close(void);
	bool Remove(void);
	unsigned int GetSize(void) const;
	unsigned int Tell(void) const;
	unsigned int Seek(unsigned int position, SeekFileFlags_t mode) const;
	bool EOFReached(void) const;
	CString ReadCharacter(void);
	void ReadLine(CString &line, const CString &delimiter = CString("\n"));
	bool Read(CASBLOB *blob, unsigned int size);
	bool Read(CASBLOB *blob);
	CASBLOB *ReadBlob(unsigned int size, bool canResize = true);
	CASBLOB *ReadBlob(void);
	void Write(const CString &value);
	void Write(const CASBLOB *blob);
	void WriteBytes(const void *data, unsigned int size);

private:
	CASVirtualFileSystem	*m_pFileSystem;
	const CASDirectory	*m_pDirectory;
	CString				m_szFileName;
	OpenFileFlags_t		m_uiFlags;
	FileHandle_t		m_pFile;
	unsigned int		m_uiBytesWritten;
};

#endif
