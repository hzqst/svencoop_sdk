//========= Copyright (c) 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "archtypes.h"     // DAL
#include "interface.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
typedef void * FileHandle_t;
typedef int FileFindHandle_t;
typedef int WaitForResourcesHandle_t;


//-----------------------------------------------------------------------------
// Enums used by the interface
//-----------------------------------------------------------------------------
#ifndef FILESYSTEM_INTERNAL_H
typedef enum
{
	FILESYSTEM_SEEK_HEAD = 0,
	FILESYSTEM_SEEK_CURRENT,
	FILESYSTEM_SEEK_TAIL,
} FileSystemSeek_t;

enum
{
	FILESYSTEM_INVALID_FIND_HANDLE = -1
};

typedef enum
{
	// Don't print anything
	FILESYSTEM_WARNING_QUIET = 0,

	// On shutdown, report names of files left unclosed
	FILESYSTEM_WARNING_REPORTUNCLOSED,

	// Report number of times a file was opened, closed
	FILESYSTEM_WARNING_REPORTUSAGE,

	// Report all open/close events to console ( !slow! )
	FILESYSTEM_WARNING_REPORTALLACCESSES
} FileWarningLevel_t;

#define FILESYSTEM_INVALID_HANDLE	( FileHandle_t )0
#endif

typedef enum
{
	PATH_ADD_TO_HEAD = 0,		// front of the search-path list
	PATH_ADD_TO_TAIL,			// end of the search-path list
	PATH_ADD_TO_HEAD_ATINDEX,	// before the first path with the same pathID
	PATH_ADD_TO_TAIL_ATINDEX	// at the last path with the same pathID
} SearchPathAdd_t;

// turn off any windows defines
#undef GetCurrentDirectory

//-----------------------------------------------------------------------------
// Purpose: Main file system interface
//-----------------------------------------------------------------------------
class IFileSystem : public IBaseInterface
{
public:
	// Mount and unmount the filesystem
	virtual void			Mount(void) = 0;
	virtual void			Unmount(void) = 0;

	// Remove all search paths (including write path?)
	virtual void			RemoveAllSearchPaths( void ) = 0;

	// Add paths in priority order (mod dir, game dir, ....)
	// If one or more .pak files are in the specified directory, then they are
	//  added after the file system path
	// If the path is the relative path to a .bsp file, then any previous .bsp file 
	//  override is cleared and the current .bsp is searched for an embedded PAK file
	//  and this file becomes the highest priority search path ( i.e., it's looked at first
	//   even before the mod's file system path ).
	virtual void			AddSearchPath( const char* pPath, const char* pathID ) = 0;
	virtual bool			RemoveSearchPath( const char* pPath ) = 0;

	// Deletes a file
	virtual void			RemoveFile( const char* pRelativePath, const char* pathID ) = 0;

	// this isn't implementable on STEAM as is.
	virtual void			CreateDirHierarchy( const char* path, const char* pathID ) = 0;

	// File I/O and info
	virtual bool			FileExists( const char* pFileName ) = 0;
	virtual bool			IsDirectory( const char* pFileName ) = 0;

	// opens a file
	// if pathID is NULL, all paths will be searched for the file
	virtual FileHandle_t	Open( const char* pFileName, const char* pOptions, const char* pathID = 0L ) = 0;

	virtual void			Close( FileHandle_t file ) = 0;

	virtual void			Seek( FileHandle_t file, int pos, FileSystemSeek_t seekType ) = 0;
	virtual unsigned int	Tell( FileHandle_t file ) = 0;

	virtual unsigned int	Size( FileHandle_t file ) = 0;
	virtual unsigned int	Size( const char* pFileName ) = 0;

	virtual long			GetFileTime( const char* pFileName ) = 0;
	virtual void			FileTimeToString( char* pStrip, int maxCharsIncludingTerminator, long fileTime ) = 0;

	virtual bool			IsOk( FileHandle_t file ) = 0;

	virtual void			Flush( FileHandle_t file ) = 0;
	virtual bool			EndOfFile( FileHandle_t file ) = 0;

	virtual int				Read( void* pOutput, int size, FileHandle_t file ) = 0;
	virtual int				Write( void const* pInput, int size, FileHandle_t file ) = 0;
	virtual char*			ReadLine( char* pOutput, int maxChars, FileHandle_t file ) = 0;
	virtual int				FPrintf( FileHandle_t file, char* pFormat, ... ) = 0;

	// direct filesystem buffer access
	// returns a handle to a buffer containing the file data
	// this is the optimal way to access the complete data for a file, 
	// since the file preloader has probably already got it in memory
	virtual void*			GetReadBuffer( FileHandle_t file, int* outBufferSize, bool failIfNotInCache ) = 0;
	virtual void            ReleaseReadBuffer( FileHandle_t file, void* readBuffer ) = 0;

	// FindFirst/FindNext
	virtual const char*		FindFirst( const char* pWildCard, FileFindHandle_t* pHandle, const char* pathID = 0L ) = 0;
	virtual const char*		FindNext( FileFindHandle_t handle ) = 0;
	virtual bool			FindIsDirectory( FileFindHandle_t handle ) = 0;
	virtual void			FindClose( FileFindHandle_t handle ) = 0;

	virtual void			GetLocalCopy( const char* pFileName ) = 0;

	virtual const char*		GetLocalPath( const char* pFileName, char* pLocalPath, int localPathBufferSize ) = 0;

	// Note: This is sort of a secondary feature; but it's really useful to have it here
	virtual char*			ParseFile( char* pFileBytes, char* pToken, bool* pWasQuoted )	= 0;

	// Returns true on success ( based on current list of search paths, otherwise false if 
	//  it can't be resolved )
	virtual bool			FullPathToRelativePath( const char* pFullpath, char* pRelative ) = 0;

	// Gets the current working directory
	virtual bool			GetCurrentDirectory( char* pDirectory, int maxlen ) = 0;

	// Dump to printf/OutputDebugString the list of files that have not been closed
	virtual void			PrintOpenedFiles( void ) = 0;

	virtual void			SetWarningFunc( void (*pfnWarning)( const char* fmt, ... ) ) = 0;
	virtual void			SetWarningLevel( FileWarningLevel_t level ) = 0;

	virtual void			LogLevelLoadStarted( const char* name ) = 0;
	virtual void			LogLevelLoadFinished( const char* name ) = 0;
	virtual int				HintResourceNeed( const char* hintlist, int forgetEverything ) = 0;
	virtual int				PauseResourcePreloading( void ) = 0;
	virtual int				ResumeResourcePreloading( void ) = 0;
	virtual int				SetVBuf( FileHandle_t stream, char* buffer, int mode, long size ) = 0;
	virtual void			GetInterfaceVersion( char* p, int maxlen ) = 0;
	virtual bool			IsFileImmediatelyAvailable(const char* pFileName) = 0;

	// starts waiting for resources to be available
	// returns FILESYSTEM_INVALID_HANDLE if there is nothing to wait on
	virtual WaitForResourcesHandle_t WaitForResources( const char* resourcelist ) = 0;
	// get progress on waiting for resources; progress is a float [0, 1], complete is true on the waiting being done
	// returns false if no progress is available
	// any calls after complete is true or on an invalid handle will return false, 0.0f, true
	virtual bool			GetWaitForResourcesProgress( WaitForResourcesHandle_t handle, float* progress /* out */ , bool* complete /* out */ ) = 0;
	// cancels a progress call
	virtual void			CancelWaitForResources( WaitForResourcesHandle_t handle ) = 0;
	// returns true if the appID has all its caches fully preloaded
	virtual bool			IsAppReadyForOfflinePlay( int appID ) = 0;

	// interface for custom pack files > 4Gb
	virtual bool			AddPackFile( const char* fullpath, const char* pathID ) = 0;
	
	// open a file but force the data to come from the steam cache, NOT from disk
	virtual FileHandle_t	OpenFromCacheForRead( const char* pFileName, const char* pOptions, const char* pathID = 0L ) = 0;

	virtual void			AddSearchPathNoWrite( const char* pPath, const char* pathID ) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Sven Co-op engine filesystem extension, exposed as "SCFileSystem002"
//-----------------------------------------------------------------------------
class IFileSystem_SvEngine : public IFileSystem
{
public:
	virtual FileWarningLevel_t GetWarningLevel( void ) = 0;

	virtual void			DumpPaths( void ) = 0;

	// Returns the search-path directory the current result of the given find
	// handle was located in
	virtual const char		*FindGetCurSearchPath( FileFindHandle_t findHandle ) = 0;

	// New search paths always include pack files
	// bAllowWrite controls writing to the path
	// Ex functions accept pathID lists such as "GAME|GAMECONFIG"
	virtual void			AddSearchPathEx( const char *pPath, const char *pathID, bool bAllowWrite, SearchPathAdd_t addType ) = 0;
	virtual bool			RemoveSearchPathEx( const char *pPath, const char *pathID ) = 0;
	virtual int				GetSearchPath( const char *pathID, bool bGetPackFiles, char *pPath, int nMaxLen ) = 0;
	virtual const char		*RelativePathToFullPath( const char *pFileName, const char *pPathID, char *pLocalPath, int localPathBufferSize ) = 0;
	virtual bool			FullPathToRelativePathEx( const char *pFullPath, const char *pPathId, char *pRelative, int nMaxLen ) = 0;

	virtual bool			FileExistsEx( const char *pFileName, const char *pPathID ) = 0;
	virtual bool			IsDirectoryEx( const char *pFileName, const char *pathID ) = 0;
	virtual unsigned int	SizeEx( const char *pFileName, const char *pPathID ) = 0;
	virtual long			GetFileTimeEx( const char *pFileName, const char *pPathID ) = 0;
};

// Steam3/Src compat
#define IBaseFileSystem IFileSystem
extern IFileSystem* g_pFileSystem;

#define FILESYSTEM_INTERFACE_VERSION "VFileSystem009"

#endif // FILESYSTEM_H