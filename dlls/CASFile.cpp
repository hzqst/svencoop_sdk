#include "extdll.h"
#include "server_filesystem.h"
#include "CASFile.h"
#include "CASVirtualFileSystem.h"
#include "CASFileQuota.h"
#include "CASBLOB.h"
#include <sstream>

CASFile::CASFile(CASVirtualFileSystem *fileSystem, const CASDirectory *directory, const char *fileName,
	OpenFileFlags_t flags, FileHandle_t file, unsigned int bytesWritten)
	: m_pFileSystem(fileSystem), m_pDirectory(directory), m_szFileName(fileName),
	m_uiFlags(flags), m_pFile(file), m_uiBytesWritten(bytesWritten)
{
}

CASFile::~CASFile()
{
	Close();
}

void CASFile::Release(void) const
{
	if (InternalRelease())
		delete this;
}

bool CASFile::IsOpen(void) const
{
	return m_pFile != NULL;
}

void CASFile::Close(void)
{
	if (m_pFile)
	{
		g_pFileSystem->Close(m_pFile);
		m_pFile = NULL;
	}
}

bool CASFile::Remove(void)
{
	if (!m_pFile || (m_uiFlags & OpenFileFlagBit::READ))
		return false;

	Close();
	m_pFileSystem->m_pFileQuota->RemoveWriteFileFromList(m_szFileName);
	g_pFileSystem->RemoveFile(m_szFileName.Data(), "GAMECONFIG");
	return true;
}

unsigned int CASFile::GetSize(void) const
{
	return m_pFile ? g_pFileSystem->Size(m_pFile) : 0;
}

unsigned int CASFile::Tell(void) const
{
	return m_pFile ? g_pFileSystem->Tell(m_pFile) : 0;
}

unsigned int CASFile::Seek(unsigned int position, SeekFileFlags_t mode) const
{
	if (!m_pFile)
		return 0;

	g_pFileSystem->Seek(m_pFile, position, (FileSystemSeek_t)mode);
	return g_pFileSystem->Tell(m_pFile);
}

bool CASFile::EOFReached(void) const
{
	return !g_pFileSystem->IsOk(m_pFile) || g_pFileSystem->EndOfFile(m_pFile);
}

CString CASFile::ReadCharacter(void)
{
	if (!(m_uiFlags & OpenFileFlagBit::READ) || !m_pFile
		|| !g_pFileSystem->IsOk(m_pFile) || g_pFileSystem->EndOfFile(m_pFile))
		return CString("");

	char character = 0;

	g_pFileSystem->Read(&character, sizeof(character), m_pFile);
	return CString(character);
}

void CASFile::ReadLine(CString &line, const CString &delimiter)
{
	if (!(m_uiFlags & OpenFileFlagBit::READ))
		return;

	if (!delimiter.Length() || !m_pFile)
	{
		line = "";
		return;
	}

	char end = delimiter.Data()[0];
	char character;
	std::ostringstream stream;

	while (g_pFileSystem->IsOk(m_pFile) && !g_pFileSystem->EndOfFile(m_pFile)
		&& g_pFileSystem->Read(&character, sizeof(character), m_pFile) && character != end)
		stream << character;

	line = stream.str().c_str();

	if (end == '\n' && line.Length() && line.Data()[line.Length() - 1] == '\r')
		line.Resize(line.Length() - 1, true);
}

bool CASFile::Read(CASBLOB *blob, unsigned int size)
{
	if (!(m_uiFlags & OpenFileFlagBit::READ) || !blob)
		return false;

	bool success = false;

	if (m_pFile)
	{
		if (!size)
		{
			success = true;
		}
		else if (blob->CanWriteSize(size) && size <= GetSize() - Tell())
		{
			unsigned char *buffer = new unsigned char[size];

			memset(buffer, 0, size);

			unsigned int bytesRead = g_pFileSystem->Read(buffer, size, m_pFile);

			if (bytesRead)
				blob->Write(buffer, bytesRead);

			delete[] buffer;
			success = true;
		}
	}

	blob->Release();
	return success;
}

bool CASFile::Read(CASBLOB *blob)
{
	if (!(m_uiFlags & OpenFileFlagBit::READ) || !blob)
		return false;

	bool success = false;

	if (m_pFile)
	{
		unsigned int size = blob->CanResize() ? GetSize() - Tell() : blob->GetWriteBytesLeft();

		if (size)
		{
			unsigned char *buffer = new unsigned char[size];

			memset(buffer, 0, size);

			unsigned int bytesRead = g_pFileSystem->Read(buffer, size, m_pFile);

			if (bytesRead)
				blob->Write(buffer, bytesRead);

			delete[] buffer;
			success = true;
		}
	}

	blob->Release();
	return success;
}

CASBLOB *CASFile::ReadBlob(unsigned int size, bool canResize)
{
	if (!(m_uiFlags & OpenFileFlagBit::READ))
		return NULL;

	CASBLOB *pBlob = new CASBLOB(size, canResize);

	pBlob->AddRef();

	if (!Read(pBlob, size))
	{
		pBlob->Release();
		return NULL;
	}

	return pBlob;
}

CASBLOB *CASFile::ReadBlob(void)
{
	if (!(m_uiFlags & OpenFileFlagBit::READ))
		return NULL;

	CASBLOB *pBlob = new CASBLOB;

	pBlob->AddRef();

	if (!Read(pBlob))
	{
		pBlob->Release();
		return NULL;
	}

	return pBlob;
}

void CASFile::Write(const CString &value)
{
	if ((m_uiFlags & (OpenFileFlagBit::WRITE | OpenFileFlagBit::APPEND)) && m_pFile)
		WriteBytes(value.Data(), value.Length());
}

void CASFile::Write(const CASBLOB *blob)
{
	if (!(m_uiFlags & (OpenFileFlagBit::WRITE | OpenFileFlagBit::APPEND)) || !m_pFile || !blob)
		return;

	WriteBytes(blob->m_pBuffer, blob->m_uiWriteOffset);
	blob->Release();
}

void CASFile::WriteBytes(const void *data, unsigned int size)
{
	if (!(m_uiFlags & (OpenFileFlagBit::WRITE | OpenFileFlagBit::APPEND)) || !data || !size || !m_pFile)
		return;

	CASFileQuota *pQuota = m_pFileSystem->m_pFileQuota;

	if (!pQuota->CanWriteAmount(size))
		return;

	g_pFileSystem->Write(data, size, m_pFile);
	m_pFileSystem->m_pFileQuota->UpdateBytesWrittenToFile(m_szFileName, size);
	m_uiBytesWritten += size;
}
