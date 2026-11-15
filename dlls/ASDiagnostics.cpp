#include "extdll.h"
#include "util.h"
#include "ASDiagnostics.h"
#include "ASPrintVariable.h"
#include "CASLog.h"
#include "CASModule.h"
#include "CASServerManager.h"
#include "FileSystem.h"
#include "server_support.h"
#include <angelscript.h>
#include "../external/angelscript/sdk/add_on/scripthelper/scripthelper.h"

#define DIAGNOSTICS_FILE_NAME_LENGTH	320
#define DIAGNOSTICS_OUTPUT_FILE_LENGTH	( DIAGNOSTICS_FILE_NAME_LENGTH + sizeof( ".txt" ) )

void ASPrintGlobals( const char *fileName )
{
	asIScriptEngine *engine = CASServerManager::GetInstance()->m_pEngine;

	if ( !engine )
		return;

	if ( !fileName )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "CASDocumentation::PrintGlobals: null file name!\n" );
		return;
	}

	char outputFile[DIAGNOSTICS_OUTPUT_FILE_LENGTH];

	if ( strlen( fileName ) >= DIAGNOSTICS_FILE_NAME_LENGTH )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "Can't output globals to file %s: file name too long!\n", fileName );
		return;
	}

	UTIL_FormatString( outputFile, sizeof( outputFile ), "%s.txt", fileName );

	FileHandle_t file = g_pFileSystem->Open( outputFile, "w", "GAMECONFIG" );

	if ( !file )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "Couldn't open file %s for output of globals!\n", outputFile );
		return;
	}

	g_pFileSystem->FPrintf( file, "Angelscript global variables output:\n" );
	CASLog::GetInstance()->Log( AS_LOG_DEFAULT, "Outputting globals to file %s\n", outputFile );

	unsigned int count = engine->GetGlobalPropertyCount();

	g_pFileSystem->FPrintf( file, "Sven Co-op global variables:\n" );

	for ( unsigned int index = 0; index < count; index++ )
	{
		const char *name = NULL;
		const char *nameSpace = NULL;
		int typeId;
		bool isConst;
		void *address;

		if ( engine->GetGlobalPropertyByIndex( index, &name, &nameSpace, &typeId, &isConst, NULL, &address ) < 0 )
		{
			CASLog::GetInstance()->Warning( AS_LOG_DEFAULT, "Couldn't get global var information\n" );
			continue;
		}

		char qualifiedName[256];

		// This limit is the output file's, although the formatted name buffer is smaller
		if ( strlen( nameSpace ) + strlen( name ) < DIAGNOSTICS_OUTPUT_FILE_LENGTH )
		{
			if ( *nameSpace )
				UTIL_FormatString( qualifiedName, sizeof( qualifiedName ), "%s::%s", nameSpace, name );
			else
				UTIL_FormatString( qualifiedName, sizeof( qualifiedName ), "%s", name );

			ASPrintVariable( file, typeId, address, qualifiedName );
		}
		else
		{
			CASLog::GetInstance()->Warning( AS_LOG_DEFAULT,
			                                "Can't print variable %s::%s information; name is too long!\n", nameSpace, name );
		}
	}

	CASServerManager::GetInstance()->GetMapModule()->PrintGlobals( file );
	g_pFileSystem->Close( file );
}

void ASOutputConfig( const char *fileName )
{
	if ( !fileName )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "CASManager::OutputConfig: null file name!\n" );
		return;
	}

	if ( strlen( fileName ) >= DIAGNOSTICS_OUTPUT_FILE_LENGTH )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "Can't output config to file '%s': file name too long!\n", fileName );
		return;
	}

	char gameDirectory[MAX_PATH + 1];

	UTIL_GetGameDir( gameDirectory, sizeof( gameDirectory ) );

	char outputFile[DIAGNOSTICS_OUTPUT_FILE_LENGTH];
	int length = UTIL_FormatString( outputFile, sizeof( outputFile ), "%s/%s.txt", gameDirectory, fileName );

	if ( (unsigned int) length >= sizeof( outputFile ) )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "Failed to output config to file '%s'\n", fileName );
		return;
	}

	if ( WriteConfigToFile( CASServerManager::GetInstance()->m_pEngine, outputFile ) < 0 )
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "Failed to output config to file '%s', error while writing!\n", outputFile );
}
