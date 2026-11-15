#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "enginecallback.h"
#include "ASDiagnostics.h"
#include "ASFileSystem.h"
#include "CASServerManager.h"
#include "CASPluginManager.h"
#include "CASConCommandSystem.h"
#include "CASCommand.h"
#include "CASModuleInfo.h"
#include "CASHeaderList.h"
#include "CASDocumentation.h"
#include "CASFileQuota.h"
#include "CASHook.h"
#include "CASTextMenus.h"
#include "CASScriptBaseClasses.h"
#include "CASVirtualFileSystem.h"
#include "CCommand.h"
#include "CConCommand.h"
#include "CClientCommand.h"
#include "CConCommandSystem.h"
#include "CKeyvaluesWriter.h"
#include "server_support.h"
#include "FileSystem.h"
#include "../external/angelscript/sdk/add_on/scripthelper/scripthelper.h"
#include <stdlib.h>

void as_globals_Callback( const CCommand &command )
{
	CBasePlayer *player = CConCommandSystem::GetInstance()->GetCurrentPlayer();

	if ( !player )
		return;

	char buffer[256];

	if ( command.ArgC() == 1 )
	{
		UTIL_FormatString( buffer, sizeof( buffer ),
		                   "as_globals: Outputting global variables to default file \"%s\"\n", "asglobals" );
		CLIENT_PRINTF( player->edict(), print_console, buffer );
		ASPrintGlobals( "asglobals" );
	}
	else if ( command.ArgC() == 2 )
	{
		const char *fileName = command.Arg( 1 );

		if ( strstr( fileName, ".." ) || strstr( fileName, "/" ) || strstr( fileName, "\\" ) )
		{
			CLIENT_PRINTF( player->edict(), print_console,
			               "as_globals: Cannot output global variables; file name contains '..', '/' or '\\', which is not allowed\n" );
			return;
		}

		UTIL_FormatString( buffer, sizeof( buffer ), "as_globals: Outputting global variables to file \"%s\"\n", fileName );
		CLIENT_PRINTF( player->edict(), print_console, buffer );
		ASPrintGlobals( fileName );
	}
	else
	{
		UTIL_FormatString( buffer, sizeof( buffer ),
		                   "as_globals usage:\n"
		                   "\"as_globals\" to output global variables to default file \"%s\"\n"
		                   "\"as_globals <filename> to output to file of given name\n",
		                   "asglobals" );
		CLIENT_PRINTF( player->edict(), print_console, buffer );
	}
}

void as_generate_config_Callback( const CCommand &command )
{
	CBasePlayer *player = CConCommandSystem::GetInstance()->GetCurrentPlayer();

	if ( !player )
		return;

	char buffer[256];

	if ( command.ArgC() == 1 )
	{
		UTIL_FormatString( buffer, sizeof( buffer ),
		                   "as_generate_config: Outputting config to default file \"%s\"\n", "asconfig" );
		CLIENT_PRINTF( player->edict(), print_console, buffer );
		ASOutputConfig( "asconfig" );
	}
	else if ( command.ArgC() == 2 )
	{
		const char *fileName = command.Arg( 1 );

		if ( strstr( fileName, ".." ) || strstr( fileName, "/" ) || strstr( fileName, "\\" ) )
		{
			CLIENT_PRINTF( player->edict(), print_console,
			               "as_generate_config: Cannot output config; file name contains '..', '/' or '\\', which is not allowed\n" );
			return;
		}

		UTIL_FormatString( buffer, sizeof( buffer ), "as_generate_config: Outputting config to file \"%s\"\n", fileName );
		CLIENT_PRINTF( player->edict(), print_console, buffer );
		ASOutputConfig( fileName );
	}
	else
	{
		UTIL_FormatString( buffer, sizeof( buffer ),
		                   "as_generate_config usage:\n"
		                   "\"as_generate_config\" to output config to default file \"%s\"\n"
		                   "\"as_generate_config <filename>\" to output to file of given name\n",
		                   "asconfig" );
		CLIENT_PRINTF( player->edict(), print_console, buffer );
	}
}

void as_fs_dumpfilesystem_Callback( const CCommand &command )
{
	bool writeDetails = true;

	if ( command.ArgC() == 3 )
		writeDetails = atoi( command.Arg( 2 ) ) != 0;
	else if ( command.ArgC() != 2 )
	{
		ALERT( at_console, "as_fs_dumpfilesystem usage: as_fs_dumpfilesystem <filename>\n" );
		return;
	}

	char fileName[MAX_PATH + 1];
	int length = UTIL_FormatString( fileName, sizeof( fileName ), "%s.txt", command.Arg( 1 ) );

	if ( (unsigned int) length >= sizeof( fileName ) )
	{
		ALERT( at_console, "Filename too long!\n" );
		return;
	}

	CKeyvaluesWriter writer( fileName );

	if ( writer.m_hFile )
	{
		CWriteFileSystemToFile operation( writer, writeDetails );

		operation.Write( *CASServerManager::GetInstance()->m_pFileSystem );
	}

	if ( writer.m_hFile && !writer.m_bError )
		ALERT( at_console, "Written filesystem to file '%s'\n", writer.m_szFileName );
	else
		ALERT( at_error, "An error occurred while writing filesystem to file '%s'\n", fileName );
}

void as_dumphooks_Callback( const CCommand &command )
{
	if ( command.ArgC() != 2 )
	{
		ALERT( at_console, "as_dumphooks usage: as_dumphooks <filename> (in the game directory)\n" );
		return;
	}

	CASHook::OutputHooksToFile( command.Arg( 1 ) );
}

void as_fs_quota_usage_Callback( const CCommand &command )
{
	CASFileQuota *quota = CASServerManager::GetInstance()->m_pFileSystem->m_pFileQuota;

	ALERT( at_console, "Quota usage: %u files, %u bytes in use, %u max\nFilename: usage in bytes:\n",
	       (unsigned int) quota->m_Files.size(), quota->m_uiSizeInUse, quota->m_uiTotalSize );

	for ( auto it = quota->m_Files.cbegin(); it != quota->m_Files.cend(); it++ )
		ALERT( at_console, "%s: %u\n", it->second.m_szFileName.Data(), it->second.m_uiBytesWritten );

	ALERT( at_console, "End Quota usage\n" );
}

void as_fs_validate_file_usage_Callback( const CCommand &command )
{
	CASServerManager::GetInstance()->m_pFileSystem->m_pFileQuota->ValidateFileList();
}

void as_execute_Callback( const CCommand &command )
{
	const char *moduleName;

	if ( command.ArgC() == 2 )
		moduleName = "MapModule";
	else if ( command.ArgC() == 3 )
		moduleName = command.Arg( 1 );
	else
	{
		ALERT( at_console, "as_execute usage: as_execute <module name(optional; default: MapModule)> \"<code>\"" );
		return;
	}

	asIScriptEngine *engine = CASServerManager::GetInstance()->m_pEngine;
	asIScriptModule *module = engine->GetModule( moduleName, asGM_ONLY_IF_EXISTS );

	if ( !module )
	{
		ALERT( at_console, "as_execute: couldn't find the module!\n" );
		return;
	}

	if ( !module->GetUserData( 10000 ) )
	{
		ALERT( at_console, "as_execute: module is in an invalid state!\n" );
		return;
	}

	ExecuteString( engine, command.Arg( command.ArgC() - 1 ), module );
}

void as_generate_includes_Callback( const CCommand &command )
{
	CBasePlayer *player = CConCommandSystem::GetInstance()->GetCurrentPlayer();

	if ( !player )
		return;

	if ( command.ArgC() != 3 )
	{
		CLIENT_PRINTF( player->edict(), print_console,
		               "as_generate_includes usage:\n"
		               "\"as_generate_includes\" <module type> <filename> to output the includes of the given module type to the given filename\n" );
		return;
	}

	const char *fileName = command.Arg( 2 );

	if ( strstr( fileName, ".." ) || strstr( fileName, "/" ) || strstr( fileName, "\\" ) )
	{
		CLIENT_PRINTF( player->edict(), print_console,
		               "as_generate_includes: Cannot output includes; file name contains '..', '/' or '\\', which is not allowed\n" );
		return;
	}

	const char *moduleName = command.Arg( 1 );
	ModuleType::Type moduleType;
	char buffer[256];

	if ( !stricmp( moduleName, "plugin" ) )
		moduleType = ModuleType::PLUGIN;
	else if ( !stricmp( moduleName, "map" ) )
		moduleType = ModuleType::MAP;
	else
	{
		UTIL_FormatString( buffer, sizeof( buffer ), "as_generate_includes: unknown module type \"%s\"!\n", moduleName );
		CLIENT_PRINTF( player->edict(), print_console, buffer );
		return;
	}

	CASModuleInfo *info = CASModuleInfo::FindModuleInfo( moduleType );

	if ( !info )
		return;

	char outputFileName[MAX_PATH + 1];
	int length = UTIL_FormatString( outputFileName, sizeof( outputFileName ), "%s.txt", fileName );

	if ( (unsigned int) length >= sizeof( outputFileName ) )
	{
		CLIENT_PRINTF( player->edict(), print_console, "as_generate_includes: failed to format filename!\n" );
		return;
	}

	UTIL_FormatString( buffer, sizeof( buffer ),
	                   "as_generate_includes: Outputting includes of module type \"%s\" to file \"%s\"\n", moduleName, fileName );

	FileHandle_t file = g_pFileSystem->Open( outputFileName, "w", "GAME" );

	if ( file )
	{
		CASHeaderList *headers = info->GenerateHeaders();

		for ( auto header = headers->m_pHead; header; header = header->m_pNext )
			g_pFileSystem->FPrintf( file, "%s", header->m_pszSource );

		headers->Release();
		g_pFileSystem->Close( file );
	}
	else
	{
		UTIL_FormatString( buffer, sizeof( buffer ), "as_generate_includes: failed to open file \"%s\" for writing!\n", fileName );
	}

	CLIENT_PRINTF( player->edict(), print_console, buffer );
}

void as_listmapscriptinfo_Callback( const CCommand &command )
{
	CBasePlayer *player = CConCommandSystem::GetInstance()->GetCurrentPlayer();

	if ( player )
		OutputModuleScriptInfo( player->edict(), CASServerManager::GetInstance()->GetMapModule() );
}

void as_listplugins_Callback( const CCommand &command )
{
	CBasePlayer *player = CConCommandSystem::GetInstance()->GetCurrentPlayer();

	if ( player )
		CASServerManager::GetInstance()->m_pPluginManager->ListPlugins( player->edict() );
}

void as_plugins_Callback( const CCommand &command )
{
	CASServerManager::GetInstance()->m_pPluginManager->ListPlugins( NULL );
}

void as_reloadplugin_Callback( const CCommand &command )
{
	if ( command.ArgC() != 2 )
	{
		ALERT( at_console, "Angelscript: as_reloadplugin usage:\n\"as_reloadplugin <plugin name>\" to reload a plugin\n" );
		return;
	}

	CASServerManager::GetInstance()->m_pPluginManager->ReloadPlugin( command.Arg( 1 ) );
}

void as_reloadplugins_Callback( const CCommand &command )
{
	CASServerManager::GetInstance()->m_pPluginManager->ReloadAllPlugins();
}

void as_removeplugin_Callback( const CCommand &command )
{
	if ( command.ArgC() != 2 )
	{
		ALERT( at_console, "Angelscript: as_removeplugin usage:\n\"as_removeplugin <plugin name>\" to remove a plugin\n" );
		return;
	}

	CASServerManager::GetInstance()->m_pPluginManager->RemovePlugin( command.Arg( 1 ) );
}

void as_removeplugins_Callback( const CCommand &command )
{
	CASServerManager::GetInstance()->m_pPluginManager->RemoveAllPlugins( false );
}

void as_showversioninfo_Callback( const CCommand &command )
{
	CBasePlayer *player = CConCommandSystem::GetInstance()->GetCurrentPlayer();

	if ( !player )
		return;

	char buffer[512];
	int length = UTIL_FormatString( buffer, sizeof( buffer ),
	                                "Sven Co-op Angelscript API\nAngelscript version: %s\nSC API version: %s\n",
	                                ANGELSCRIPT_VERSION_STRING, CASDocumentation::API_VERSION );

	if ( (unsigned int) length < sizeof( buffer ) )
		CLIENT_PRINTF( player->edict(), print_console, buffer );
}

void as_menuselect_Callback( const CCommand &command )
{
	if ( command.ArgC() >= 2 )
		CASTextMenus::GetInstance()->HandleMenuSelect( CConCommandSystem::GetInstance()->GetCurrentPlayer(), atoi( command.Arg( 1 ) ) );
}

void as_command_Callback( const CCommand &command )
{
	if ( command.ArgC() == 1 )
	{
		ALERT( at_console, "Angelscript server command usage: as_command <command or cvar name> <parameter(s)>\n" );
		return;
	}

	const char *name = command.Arg( 1 );
	CASCommand *copy = new CASCommand( command.GetArgumentsString() );
	CASConCommandSystem *system = CASConCommandSystem::GetInstance();

	if ( system->m_StateByte )
	{
		system->EnqueueCommand( copy, ServerCommandQueueCallback );
	}
	else
	{
		int result = static_cast<IConCommandSystem *>( system )->Execute( *copy, NULL, true );

		ServerCommandQueueCallback( (ConCommandExecuteResult::Type) result, name, copy );
	}

	copy->Release();
}

void as_commandstats_Callback( const CCommand &command )
{
	CASConCommandSystem::GetInstance()->DisplayCommandStats();
}

void as_findcommands_Callback( const CCommand &command )
{
	CBasePlayer *player = CConCommandSystem::GetInstance()->GetCurrentPlayer();

	if ( !player )
		return;

	if ( command.ArgC() == 2 )
		CASConCommandSystem::GetInstance()->FindCommands( command.Arg( 1 ), player );
	else
		CLIENT_PRINTF( player->edict(), print_console, "as_findcommands usage: as_findcommands <substring>\n" );
}

void as_removeallcommands_Callback( const CCommand &command )
{
	CASConCommandSystem::GetInstance()->ClearCommands();
}

void as_scriptbaseclasses_Callback( const CCommand &command )
{
	CBasePlayer *player = CConCommandSystem::GetInstance()->GetCurrentPlayer();

	if ( player )
		CASScriptBaseClasses::OutputToConsole( player->edict() );
}

CConCommand as_dumphooks( "as_dumphooks", as_dumphooks_Callback, ConCommandFlag::AdminOnly | ConCommandFlag::Cheat,
                          "Outputs all Angelscript hooks to a file" );
CConCommand as_fs_dumpfilesystem( "as_fs_dumpfilesystem", as_fs_dumpfilesystem_Callback, ConCommandFlag::AdminOnly,
                                  "Outputs the Angelscript file system and blacklist to a file" );
CConCommand as_fs_quota_usage( "as_fs_quota_usage", as_fs_quota_usage_Callback, ConCommandFlag::AdminOnly,
                               "Reports the current usage state of the Angelscript File System Quota" );
CConCommand as_fs_validate_file_usage( "as_fs_validate_file_usage", as_fs_validate_file_usage_Callback, ConCommandFlag::AdminOnly,
                                       "Validates the quota file list; files removed externally are removed from the list" );
CConCommand as_execute( "as_execute", as_execute_Callback, ConCommandFlag::AdminOnly,
                        "Executes arbitrary Angelscript code. Call with no parameters for usage." );
CClientCommand as_generate_config( "as_generate_config", as_generate_config_Callback, ConCommandFlag::AdminOnly,
                                   "Outputs a file that contains the entire API as a config. Defaults to \"asconfig\"" );
CClientCommand as_generate_includes( "as_generate_includes", as_generate_includes_Callback, ConCommandFlag::AdminOnly,
                                     "Outputs the automatically included headers for a given module type to a file" );
CClientCommand as_globals( "as_globals", as_globals_Callback, ConCommandFlag::AdminOnly,
                           "Outputs a list of global variables that exist in the API to a file. Defaults to \"asglobals\"" );
CClientCommand as_listmapscriptinfo( "as_listmapscriptinfo", as_listmapscriptinfo_Callback, ConCommandFlag::None,
                                     "Outputs map script information" );
CClientCommand as_listplugins( "as_listplugins", as_listplugins_Callback, ConCommandFlag::None,
                               "Shows a list of all plugins currently active on this server" );
CConCommand as_plugins( "as_plugins", as_plugins_Callback, ConCommandFlag::AdminOnly,
                        "Shows a list of all plugins currently active on this server" );
CConCommand as_reloadplugin( "as_reloadplugin", as_reloadplugin_Callback, ConCommandFlag::AdminOnly,
                             "Reloads a plugin that is in the list of plugins" );
CConCommand as_reloadplugins( "as_reloadplugins", as_reloadplugins_Callback, ConCommandFlag::AdminOnly,
                              "Reloads all plugins from the plugin list file" );
CConCommand as_removeplugin( "as_removeplugin", as_removeplugin_Callback, ConCommandFlag::AdminOnly,
                             "Removes a plugin from the plugin list" );
CConCommand as_removeplugins( "as_removeplugins", as_removeplugins_Callback, ConCommandFlag::AdminOnly,
                              "Removes all plugins" );
CClientCommand as_showversioninfo( "as_showversioninfo", as_showversioninfo_Callback, ConCommandFlag::None,
                                   "Shows Angelscript version info" );
CClientCommand as_menuselect( "as_menuselect", as_menuselect_Callback, ConCommandFlag::None,
                              "Used by text menus to handle player input" );
CConCommand as_command( "as_command", as_command_Callback, ConCommandFlag::AdminOnly,
                        "Executes an Angelscript console command" );
CConCommand as_commandstats( "as_commandstats", as_commandstats_Callback, ConCommandFlag::None,
                             "Displays Angelscript console command statistics" );
CClientCommand as_findcommands( "as_findcommands", as_findcommands_Callback, ConCommandFlag::None,
                                "Finds Angelscript specific commands" );
CConCommand as_removeallcommands( "as_removeallcommands", as_removeallcommands_Callback, ConCommandFlag::AdminOnly,
                                  "Removes all commands from the Angelscript console command system. Debug only." );
CClientCommand as_scriptbaseclasses( "as_scriptbaseclasses", as_scriptbaseclasses_Callback, ConCommandFlag::AdminOnly,
                                     "Shows the custom entity base class implementations" );
