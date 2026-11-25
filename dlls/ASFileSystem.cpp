#include "extdll.h"
#include "enginecallback.h"
#include "server_support.h"
#include "ASFileSystem.h"
#include "ASDirectoryEnumeration.h"
#include "CASVirtualFileSystem.h"
#include "CASLog.h"
#include "CKeyvaluesWriter.h"
#include <stdlib.h>
#include "CASDocumentation.h"
#include "CASBLOB.h"
#include "ASBLOB.h"
#include "CASFile.h"
#include "CASFileQuota.h"
#include "CASServerManager.h"
#include "CASModule.h"
#include "CASModuleInfo.h"
#include "CCVar.h"

enum
{
	KILOBYTE_SHIFT = 10,
	MEGABYTE_SHIFT = 20,
	GIGABYTE_SHIFT = 30,
	TERABYTE_SHIFT = 40,
	DEFAULT_FILE_SIZE_QUOTA = 32U << MEGABYTE_SHIFT
};

cvar_t as_file_size_quota = { "as_file_size_quota", "32Mb", FCVAR_PROTECTED };

const char *ModuleTypeToString(ModuleType::Type type)
{
	if (type == ModuleType::MAP)
		return "MAP";

	if (type == ModuleType::PLUGIN)
		return "PLUGIN";

	return "UNKNOWN";
}

ModuleType::Type StringToModuleType(const char *text)
{
	if (!text || !stricmp(text, "UNKNOWN"))
		return ModuleType::UNKNOWN;

	if (!stricmp(text, "MAP"))
		return ModuleType::MAP;

	return !stricmp(text, "PLUGIN") ? ModuleType::PLUGIN : ModuleType::UNKNOWN;
}

const char *FileAccessToString(FileAccess::Type access)
{
	if (access == FileAccess::READ)
		return "READ";

	if (access == FileAccess::WRITE)
		return "WRITE";

	return "NONE";
}

FileAccess::Type StringToFileAccess(const char *text)
{
	if (!text || !stricmp(text, "NONE"))
		return FileAccess::NONE;

	if (!stricmp(text, "READ"))
		return FileAccess::READ;

	return !stricmp(text, "WRITE") ? FileAccess::WRITE : FileAccess::NONE;
}

const char *DirectoryFlagToString(DirectoryFlag::Type flag)
{
	return flag == DirectoryFlag::IMPLICIT_SUBDIRECTORIES ? "IMPLICIT_SUBDIRECTORIES" : "NONE";
}

DirectoryFlag::Type StringToDirectoryFlag(const char *text)
{
	if (!text || !stricmp(text, "NONE"))
		return DirectoryFlag::NONE;

	return !stricmp(text, "IMPLICIT_SUBDIRECTORIES") ? DirectoryFlag::IMPLICIT_SUBDIRECTORIES : DirectoryFlag::NONE;
}

bool ModuleTypesToKeyvaluesBlock(CKeyvaluesWriter &writer, unsigned char types,
	const char *blockName, const char *keyName)
{
	if (!writer.m_hFile)
		return false;

	if (!blockName || !*blockName)
		blockName = "modules";

	if (!keyName || !*keyName)
		keyName = "module";

	if (types)
	{
		writer.BeginBlock(blockName);

		for (unsigned int type = ModuleType::MAP; type <= ModuleType::PLUGIN; type++)
		{
			if (types & (1 << type))
				writer.WriteKeyvalue(keyName, ModuleTypeToString((ModuleType::Type)type));
		}

		writer.EndBlock();
	}

	return !writer.m_bError;
}

unsigned long long ParseByteSizeString(const char *text)
{
	if (!text || !*text)
		return 0;

	char *end;
	unsigned long long value = strtoull(text, &end, 10);

	if (!end || end == text)
		return 0;

	if (!*end)
		return value;

	if (!strncmp("Kb", end, sizeof("Kb") - 1))
		return value << KILOBYTE_SHIFT;

	if (!strncmp("Mb", end, sizeof("Mb") - 1))
		return value << MEGABYTE_SHIFT;

	if (!strncmp("Gb", end, sizeof("Gb") - 1))
		return value << GIGABYTE_SHIFT;

	if (!strncmp("Tb", end, sizeof("Tb") - 1))
		return value << TERABYTE_SHIFT;

	return 0;
}

unsigned int ASComputeFileQuota(void)
{
	unsigned long long quota = ParseByteSizeString(as_file_size_quota.string);

	return quota ? (unsigned int)quota : DEFAULT_FILE_SIZE_QUOTA;
}

CWriteFileSystemToFile::CWriteFileSystemToFile(CKeyvaluesWriter &writer, bool writeDetails)
	: m_pWriter(&writer), m_bWriteDetails(writeDetails), m_uiLevel(0)
{
}

bool CWriteFileSystemToFile::Write(void)
{
	if (!m_pWriter->m_hFile)
		return false;

	m_pWriter->BeginBlock("FileSystem");

	const CASDirectory *root = CASServerManager::GetInstance()->m_pFileSystem->m_pRoot;

	if (root)
	{
		char path[CASDirectory::MAX_NAME_LENGTH];
		unsigned int level = 0;

		path[0] = 0;
		EnumerateDirectories( *this, level, root, path, path, sizeof( path ) );

		while (m_uiLevel)
		{
			m_uiLevel--;
			m_pWriter->EndBlock();
		}
	}

	const CASVirtualFileSystem &fileSystem = *CASServerManager::GetInstance()->m_pFileSystem;

	if (!fileSystem.m_Extensions.empty())
	{
		m_pWriter->BeginBlock("ExtensionBlacklist");

		for (std::set<CString, CStringCaseInsensitiveLess>::const_iterator it = fileSystem.m_Extensions.begin();
			it != fileSystem.m_Extensions.end(); it++)
			m_pWriter->WriteKeyvalue("extension", it->Data());

		m_pWriter->EndBlock();
	}

	m_pWriter->EndBlock();
	return m_pWriter->m_hFile && !m_pWriter->m_bError;
}

void CWriteFileSystemToFile::operator()( unsigned int level, const CASDirectory *directory, const char * )
{
	while (level < m_uiLevel)
	{
		m_uiLevel--;
		m_pWriter->EndBlock();
	}

	if (level > m_uiLevel)
		m_uiLevel = level + 1;

	m_pWriter->BeginBlock(directory->m_szName);

	if (m_bWriteDetails)
	{
		unsigned char accessFlags = directory->m_uiAccessFlags;
		unsigned char directoryFlags = directory->m_uiDirectoryFlags;

		ModuleTypesToKeyvaluesBlock(*m_pWriter, directory->m_uiModuleTypes);

		if (accessFlags)
		{
			m_pWriter->BeginBlock("access");

			for (unsigned int access = FileAccess::READ; access <= FileAccess::WRITE; access++)
			{
				if (accessFlags & (1 << access))
					m_pWriter->WriteKeyvalue("access", FileAccessToString((FileAccess::Type)access));
			}

			m_pWriter->EndBlock();
		}

		if (directoryFlags)
		{
			m_pWriter->BeginBlock("flags");

			if (directoryFlags & DirectoryFlagBit::IMPLICIT_SUBDIRECTORIES)
				m_pWriter->WriteKeyvalue("flag", DirectoryFlagToString(DirectoryFlag::IMPLICIT_SUBDIRECTORIES));

			m_pWriter->EndBlock();
		}
	}

}

static const CCVarArguments as_fs_output_openfile_arguments = {
	0, "Whether or not to output successful OpenFile calls", "0", { NULL }, false
};
static CCVar as_fs_output_openfile("as_fs_output_openfile", as_fs_output_openfile_arguments);

const CASFileQuota *CASVirtualFileSystem_GetFileQuota(const CASVirtualFileSystem *fileSystem)
{
	CASFileQuota *quota = fileSystem->m_pFileQuota;

	quota->AddRef();
	return quota;
}

CASFile *CASVirtualFileSystem_OpenFile(CASVirtualFileSystem *fileSystem, const CString &fileName, unsigned char flags)
{
	CASModule *module = CASServerManager::GetInstance()->m_pActiveModule;

	if (!module)
		return NULL;

	CASFile *file = fileSystem->OpenFile(fileName.Data(), flags, module->m_pModuleInfo->m_ModuleType);

	if (file && (int)as_fs_output_openfile.m_flValue)
		ALERT(at_console, "File '%s' opened\n", fileName.Data());

	return file;
}

void CASVirtualFileSystem_RemoveFile(CASVirtualFileSystem *fileSystem, const CString &fileName)
{
	CASModule *module = CASServerManager::GetInstance()->m_pActiveModule;

	if (module)
		fileSystem->RemoveFile(fileName.Data(), module->m_pModuleInfo->m_ModuleType);
}

void RegisterSCScriptFile(CASDocumentation *documentation)
{
	documentation->RegisterObjectType("File class.\nIs used to read from/write to files in disk.", "File", sizeof(CASFile), asOBJ_REF);
	documentation->RegisterObjectBehaviour("AddRef", "File", asBEHAVE_ADDREF, "void AddRef()", asMETHOD(CASFile, AddRef), asCALL_THISCALL);
	documentation->RegisterObjectBehaviour("Release", "File", asBEHAVE_RELEASE, "void Release()", asMETHOD(CASFile, Release), asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Returns whether the file was successfully opened.",
		"File",
		"bool IsOpen() const",
		asMETHOD(CASFile, IsOpen),
		asCALL_THISCALL);
	documentation->RegisterObjectMethod("Closes the file if it is open.", "File", "void Close()", asMETHOD(CASFile, Close), asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Removes the file. The file must be open, and you must have write access.",
		"File",
		"void Remove()",
		asMETHOD(CASFile, Remove),
		asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Returns the size of the file.",
		"File",
		"size_t GetSize() const",
		asMETHOD(CASFile, GetSize),
		asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Tells the position of the read/write pointer.",
		"File",
		"size_t Tell() const",
		asMETHOD(CASFile, Tell),
		asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Sets the read/write pointer to a new position. Returns the new position within the file.",
		"File",
		"size_t Seek(const size_t uiPosition, const SeekFileFlags_t uiSeekMode)",
		asMETHOD(CASFile, Seek),
		asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Returns whether end of file was reached.",
		"File",
		"bool EOFReached() const",
		asMETHOD(CASFile, EOFReached),
		asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Reads a single character.",
		"File",
		"string ReadCharacter()",
		asMETHOD(CASFile, ReadCharacter),
		asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Reads a line from the file.",
		"File",
		"void ReadLine(string& out szOutLine, const string& in szDelim = '\n')",
		asMETHOD(CASFile, ReadLine),
		asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Reads a number of bytes into the given BLOB.",
		"File",
		"bool Read(BLOB@ pBlob, size_t uiSizeInBytes)",
		asMETHODPR(CASFile, Read, (CASBLOB *, unsigned int), bool),
		asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Reads as much as possible data into the given BLOB.",
		"File",
		"bool Read(BLOB@ pBlob)",
		asMETHODPR(CASFile, Read, (CASBLOB *), bool),
		asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Reads a number of bytes into a BLOB.",
		"File",
		"BLOB@ ReadBlob(size_t uiSizeInBytes, bool fCanResize = true)",
		asMETHODPR(CASFile, ReadBlob, (unsigned int, bool), CASBLOB *),
		asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Reads as much as possible data into a BLOB.",
		"File",
		"BLOB@ ReadBlob()",
		asMETHODPR(CASFile, ReadBlob, (), CASBLOB *),
		asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Writes a string to the file.",
		"File",
		"void Write(const string& in szString)",
		asMETHODPR(CASFile, Write, (const CString &), void),
		asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Writes a blob to the file.",
		"File",
		"void Write(const BLOB@ pBlob)",
		asMETHODPR(CASFile, Write, (const CASBLOB *), void),
		asCALL_THISCALL);
}

void RegisterSCScriptOpenFileFlag(CASDocumentation *documentation)
{
	CString nameSpace(documentation->GetDefaultNamespace());

	documentation->SetDefaultNamespace("OpenFile");
	documentation->RegisterEnum("Flags passed to FileSystem::OpenFile.", "OpenFile");
	documentation->RegisterEnumValue("Open file for reading.", "OpenFile", "READ", OpenFileFlagBit::READ);
	documentation->RegisterEnumValue("Open file for writing.", "OpenFile", "WRITE", OpenFileFlagBit::WRITE);
	documentation->RegisterEnumValue("Open file for appending.", "OpenFile", "APPEND", OpenFileFlagBit::APPEND);
	documentation->RegisterEnumValue("Open in binary mode.", "OpenFile", "BINARY", OpenFileFlagBit::BINARY);
	documentation->SetDefaultNamespace(nameSpace.Data());
	documentation->RegisterTypedef("Open File Flags", "OpenFileFlags_t", "uint8");
}

void RegisterSCScriptSeekFileFlag(CASDocumentation *documentation)
{
	CString nameSpace(documentation->GetDefaultNamespace());

	documentation->SetDefaultNamespace("SeekFile");
	documentation->RegisterEnum("Mode flags passed to File::Seek.", "SeekFile");
	documentation->RegisterEnumValue("Seek from the start of the file.", "SeekFile", "SET", SeekFileFlagBit::SET);
	documentation->RegisterEnumValue("Seek from the current offset in the file.", "SeekFile", "CUR", SeekFileFlagBit::CUR);
	documentation->RegisterEnumValue("Seek from the end of the file.", "SeekFile", "END", SeekFileFlagBit::END);
	documentation->SetDefaultNamespace(nameSpace.Data());
	documentation->RegisterTypedef("Seek File Flags", "SeekFileFlags_t", "uint8");
}

void RegisterSCScriptFileSystemUtils(CASDocumentation *documentation)
{
	documentation->SetDefaultNamespace("FileSystem");
	documentation->RegisterGlobalFunction(
		"Returns whether the given open flags are valid.",
		"bool FlagsValid(const OpenFileFlags_t uiOpenFlags)",
		asFUNCTION(CASFileSystemUtils::FlagsValid),
		asCALL_CDECL);
	documentation->RegisterGlobalFunction(
		"Filters the given flags, removing unnecessary flags.",
		"OpenFileFlags_t FilterFlags(OpenFileFlags_t uiOpenFlags)",
		asFUNCTION(CASFileSystemUtils::FilterFlags),
		asCALL_CDECL);
	documentation->RegisterGlobalFunction(
		"Formats the open flags into a string that represents the flags.",
		"bool FormatOpenFlags(OpenFileFlags_t uiOpenFlags, string& out szOutFlags)",
		asFUNCTION(CASFileSystemUtils::FormatOpenFlags),
		asCALL_CDECL);
	documentation->SetDefaultNamespace("");
}

void RegisterSCScriptQuota(CASDocumentation *documentation)
{
	documentation->RegisterObjectType("Represents a quota that filesystems adhere to when handling write operations.", "FileQuota", 0, asOBJ_REF);
	documentation->RegisterObjectBehaviour("AddRef", "FileQuota", asBEHAVE_ADDREF, "void AddRef()", asMETHOD(CASFileQuota, AddRef), asCALL_THISCALL);
	documentation->RegisterObjectBehaviour(
		"Release",
		"FileQuota",
		asBEHAVE_RELEASE,
		"void Release()",
		asMETHOD(CASFileQuota, Release),
		asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Gets the total size, in bytes, that is allowed to be used by filesystems using this quota object.",
		"FileQuota",
		"size_t GetTotalSize() const",
		asMETHOD(CASFileQuota, GetTotalSize),
		asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Gets the size, in bytes, that is in use.",
		"FileQuota",
		"size_t GetSizeInUse() const",
		asMETHOD(CASFileQuota, GetSizeInUse),
		asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Gets the amount of size, in bytes, left that can be written to.",
		"FileQuota",
		"size_t GetSizeLeft() const",
		asMETHOD(CASFileQuota, GetSizeLeft),
		asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Returns whether the given amount of bytes can be written.",
		"FileQuota",
		"bool CanWriteAmount(size_t uiSizeInBytes) const",
		asMETHOD(CASFileQuota, CanWriteAmount),
		asCALL_THISCALL);
	documentation->RegisterObjectMethod(
		"Returns the size that is left after the given amount of bytes have been written.",
		"FileQuota",
		"size_t SizeLeftAfterWrite(size_t uiSizeInBytes) const",
		asMETHOD(CASFileQuota, SizeLeftAfterWrite),
		asCALL_THISCALL);
}

void RegisterSCScriptVirtualFileSystem(CASDocumentation *documentation)
{
	documentation->RegisterObjectType("Virtual File System", "CVirtualFileSystem", 0, asOBJ_REF | asOBJ_NOCOUNT);
	documentation->RegisterObjectMethod(
		"Gets the file system quota object. Do not store a handle to this object, as it may be replaced at any time.",
		"CVirtualFileSystem",
		"const FileQuota@ GetFileQuota() const",
		asFUNCTION(CASVirtualFileSystem_GetFileQuota),
		asCALL_CDECL_OBJFIRST);
	documentation->RegisterObjectMethod(
		"Opens a file. Returns nullptr if the file could not be opened.",
		"CVirtualFileSystem",
		"File@ OpenFile(const string& in szFilename, const OpenFileFlags_t uiOpenFlags)",
		asFUNCTION(CASVirtualFileSystem_OpenFile),
		asCALL_CDECL_OBJFIRST);
	documentation->RegisterObjectMethod(
		"Removes a file.",
		"CVirtualFileSystem",
		"void RemoveFile(const string& in szFilename)",
		asFUNCTION(CASVirtualFileSystem_RemoveFile),
		asCALL_CDECL_OBJFIRST);
	documentation->RegisterGlobalProperty(
		"Global file system instance",
		"CVirtualFileSystem g_FileSystem",
		CASServerManager::GetInstance()->m_pFileSystem);
}

void RegisterSCScriptFileSystem(CASDocumentation *documentation)
{
	RegisterSCScriptOpenFileFlag(documentation);
	RegisterSCScriptSeekFileFlag(documentation);
	RegisterSCScriptBLOB(documentation);
	RegisterSCScriptFile(documentation);
	RegisterSCScriptFileSystemUtils(documentation);
	RegisterSCScriptQuota(documentation);
	RegisterSCScriptVirtualFileSystem(documentation);
}
