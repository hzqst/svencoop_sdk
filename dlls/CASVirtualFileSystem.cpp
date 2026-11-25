#include "extdll.h"
#include "server_filesystem.h"
#include "CASVirtualFileSystem.h"
#include "CASFile.h"
#include "CASFileQuota.h"

CASVirtualFileSystem::CASVirtualFileSystem() : m_uiAccessFlags(0)
{
	m_pFileQuota = new CASFileQuota(0, 0);
}

CASVirtualFileSystem::~CASVirtualFileSystem()
{
	if (m_pFileQuota)
		m_pFileQuota->Release();

	m_pFileQuota = NULL;
}

void CASVirtualFileSystem::SetFileQuota(unsigned int totalSize, unsigned int fileLimit)
{
	CASFileQuota *pQuota = new CASFileQuota(totalSize, fileLimit);

	if (m_pFileQuota)
		m_pFileQuota->Release();

	m_pFileQuota = pQuota;
}

CASFile *CASVirtualFileSystem::OpenFile(const char *fileName, OpenFileFlags_t flags, ModuleType::Type moduleType)
{
	bool writing = (flags & (OpenFileFlagBit::WRITE | OpenFileFlagBit::APPEND)) != 0;
	FileAccessBit::Type access = writing ? FileAccessBit::WRITE : FileAccessBit::READ;

	if (!(m_uiAccessFlags & access) || !fileName || !*fileName || HasExtension(fileName, true))
		return NULL;

	CString name(fileName);
	CString directory;
	const char *separator = strrchr(fileName, '/');

	if (separator)
	{
		unsigned int position = separator - fileName;
		CString suffix;

		directory.Assign(fileName, 0, position);
		suffix.Assign(fileName, position + 1, CString::DEFAULT_LENGTH);
		name = suffix;
	}

	if (!name.Length())
		return NULL;

	const CASDirectory *pDirectory;

	if (!CanAccessDirectory(directory.Data(), moduleType, access, &pDirectory))
		return NULL;

	CASFileQuota *pQuota = m_pFileQuota;

	if (writing && (!pQuota->CanWriteAmount(MINIMUM_WRITE_SPACE) || pQuota->FileLimitReached()))
		return NULL;

	CString fullPath(directory);

	fullPath.Append("/", 0, CString::DEFAULT_LENGTH);
	fullPath.Append(name.Data(), 0, CString::DEFAULT_LENGTH);

	CString mode;

	if (!CASFileSystemUtils::FormatOpenFlags(flags, mode) || g_pFileSystem->IsDirectory(fullPath.Data()))
		return NULL;

	FileHandle_t file = g_pFileSystem->Open(fullPath.Data(), mode.Data(), writing ? "GAMECONFIG" : "GAME");

	if (!file)
	{
		m_pFileQuota->RemoveWriteFileFromList(fullPath);
		return NULL;
	}

	if (writing)
		pQuota->AddWriteFileToList(fullPath, flags);

	unsigned int bytesWritten = flags & OpenFileFlagBit::APPEND ? pQuota->GetBytesWrittenToFile(fullPath) : 0;

	return new CASFile(this, pDirectory, fullPath.Data(), flags, file, bytesWritten);
}

void CASVirtualFileSystem::RemoveFile(const char *fileName, ModuleType::Type moduleType)
{
	CASFile *pFile = OpenFile(fileName, OpenFileFlagBit::APPEND, moduleType);

	if (pFile)
	{
		pFile->Remove();
		pFile->Release();
	}
}

void CASVirtualFileSystem::Startup(void)
{
	CreateWritableDirectories();
	ClearTempDirectories(TempDirFlagBit::STARTUP, m_pFileQuota);
}

void CASVirtualFileSystem::Shutdown(void)
{
	ClearTempDirectories(TempDirFlagBit::SHUTDOWN, m_pFileQuota);
}

void CASVirtualFileSystem::MapChange(void)
{
	ClearTempDirectories(TempDirFlagBit::MAP_CHANGE, m_pFileQuota);
}
