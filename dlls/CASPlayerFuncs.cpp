#include "extdll.h"
#include "SvenVersion.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "client.h"
#include "ConCommandDefs.h"
#include "CServer.h"
#include "CSpawnPoints.h"
#include "server_support.h"
#include "CASServerManager.h"
#include "CASModule.h"
#include "CASModuleInfo.h"
#include "CASLog.h"
#include "CASPlayerFuncs.h"
#include "FileSystem.h"
#include <time.h>

CASPlayerFuncs::~CASPlayerFuncs()
{
}

void CASPlayerFuncs::SayText( CBasePlayer *player, const CString &text )
{
	UTIL_SayText( text.Data(), player );
}

void CASPlayerFuncs::SayTextAll( CBasePlayer *player, const CString &text )
{
	UTIL_SayTextAll( text.Data(), player );
}

void CASPlayerFuncs::ClientPrint( CBasePlayer *player, int destination, const CString &message,
                                 const CString &line2, const CString &line3, const CString &line4, const CString &line5 )
{
	if ( !player )
		return;

	::ClientPrint( player->pev, destination, message.Data(), line2.Length() ? line2.Data() : NULL,
	               line3.Length() ? line3.Data() : NULL, line4.Length() ? line4.Data() : NULL,
	               line5.Length() ? line5.Data() : NULL );
}

void CASPlayerFuncs::ClientPrintAll( int destination, const CString &message, const CString &line2,
                                    const CString &line3, const CString &line4, const CString &line5 )
{
	UTIL_ClientPrintAll( destination, message.Data(), line2.Length() ? line2.Data() : NULL,
	                     line3.Length() ? line3.Data() : NULL, line4.Length() ? line4.Data() : NULL,
	                     line5.Length() ? line5.Data() : NULL );
}

void CASPlayerFuncs::CenterPrintAll( const CString &message, const CString &line2, const CString &line3,
                                    const CString &line4, const CString &line5 )
{
	UTIL_ClientPrintAll( HUD_PRINTCENTER, message.Data(), line2.Length() ? line2.Data() : NULL,
	                     line3.Length() ? line3.Data() : NULL, line4.Length() ? line4.Data() : NULL,
	                     line5.Length() ? line5.Data() : NULL );
}

void CASPlayerFuncs::ShowMessage( CBasePlayer *player, const CString &message )
{
	UTIL_ShowMessage( message.Data(), player );
}

void CASPlayerFuncs::ShowMessageAll( const CString &message )
{
	UTIL_ShowMessageAll( message.Data() );
}

#define HUD_TEXT_CHANNELS_PER_MODULE_TYPE	4
#define HUD_TEXT_MAX_SCRIPT_CHANNEL			8

void CASPlayerFuncs::ConvertHudTextChannel( hudtextparms_t &params )
{
	CASModule *module = CASServerManager::GetInstance()->m_pActiveModule;
	int firstChannel = module && module->m_pModuleInfo->m_ModuleType == ModuleType::MAP ? HUD_TEXT_CHANNELS_PER_MODULE_TYPE : 0;
	int channel = params.channel < 0 ? 0 : params.channel > HUD_TEXT_MAX_SCRIPT_CHANNEL ? HUD_TEXT_MAX_SCRIPT_CHANNEL : params.channel;

	if ( channel >= 1 )
		channel--;

	params.channel = firstChannel + channel % HUD_TEXT_CHANNELS_PER_MODULE_TYPE;
}

void CASPlayerFuncs::HudMessage( CBasePlayer *player, const hudtextparms_t &params, const CString &message )
{
	hudtextparms_t converted = params;

	ConvertHudTextChannel( converted );
	UTIL_HudMessage( player, converted, message.Data() );
}

void CASPlayerFuncs::HudMessageAll( const hudtextparms_t &params, const CString &message )
{
	hudtextparms_t converted = params;

	ConvertHudTextChannel( converted );
	UTIL_HudMessageAll( converted, message.Data() );
}

void CASPlayerFuncs::PrintKeyBindingString( CBasePlayer *player, const CString &text )
{
	UTIL_PrintKeyBindingString( player, text.Data() );
}

void CASPlayerFuncs::PrintKeyBindingStringAll( const CString &text )
{
	UTIL_PrintKeyBindingStringAll( text.Data() );
}

int CASPlayerFuncs::HudGetChannelGroup()
{
	CASModule *module = CASServerManager::GetInstance()->m_pActiveModule;

	if ( !module )
		return -1;

	return module->m_pModuleInfo->m_ModuleType == ModuleType::PLUGIN ? 1 : 0;
}

void CASPlayerFuncs::HudToggleElement( CBasePlayer *player, unsigned char channel, bool visible )
{
	UTIL_HudToggleElement( player, HudGetChannelGroup(), channel, visible );
}

void CASPlayerFuncs::HudCustomSprite( CBasePlayer *player, const hudspriteparams_t &params )
{
	UTIL_HudCustomSprite( player, HudGetChannelGroup(), params );
}

void CASPlayerFuncs::HudNumDisplay( CBasePlayer *player, const hudnumdisplayparams_t &params )
{
	UTIL_HudNumDisplay( player, HudGetChannelGroup(), params );
}

void CASPlayerFuncs::HudUpdateNum( CBasePlayer *player, unsigned char channel, float value )
{
	UTIL_HudUpdateNum( player, HudGetChannelGroup(), channel, value );
}

void CASPlayerFuncs::HudTimeDisplay( CBasePlayer *player, const hudnumdisplayparams_t &params )
{
	UTIL_HudTimeDisplay( player, HudGetChannelGroup(), params );
}

void CASPlayerFuncs::HudUpdateTime( CBasePlayer *player, unsigned char channel, float value )
{
	UTIL_HudUpdateTime( player, HudGetChannelGroup(), channel, value );
}

CBasePlayer *CASPlayerFuncs::FindPlayerByIndex( int index )
{
	return static_cast<CBasePlayer *>( UTIL_PlayerByIndex( index ) );
}

CBasePlayer *CASPlayerFuncs::FindPlayerByName( const CString &name, bool caseSensitive )
{
	return UTIL_CBasePlayerByName( name.Data(), caseSensitive );
}

int CASPlayerFuncs::GetNumPlayers() const
{
	return CServer::GetInstance()->GetPlayerCount();
}

void CASPlayerFuncs::ScreenShake( const Vector &center, float amplitude, float frequency, float duration, float radius )
{
	UTIL_ScreenShake( center, amplitude, frequency, duration, radius );
}

void CASPlayerFuncs::ScreenShakeAll( const Vector &center, float amplitude, float frequency, float duration )
{
	UTIL_ScreenShakeAll( center, amplitude, frequency, duration );
}

void CASPlayerFuncs::ScreenFade( CBaseEntity *entity, const Vector &color, float fadeTime, float fadeHold, int alpha, int flags )
{
	UTIL_ScreenFade( entity, color, fadeTime, fadeHold, alpha, flags );
}

void CASPlayerFuncs::ScreenFadeAll( const Vector &color, float fadeTime, float fadeHold, int alpha, int flags )
{
	UTIL_ScreenFadeAll( color, fadeTime, fadeHold, alpha, flags );
}

void CASPlayerFuncs::ConcussionEffect( CBaseEntity *entity, float amplitude, float frequency, float fadeTime )
{
	UTIL_ConcussionEffect( entity, amplitude, frequency, fadeTime );
}

BOOL CASPlayerFuncs::GetNextBestWeapon( CBasePlayer *player, CBasePlayerItem *weapon )
{
	return g_pGameRules->GetNextBestWeapon( player, weapon );
}

bool CASPlayerFuncs::CheatsAllowed( CBasePlayer *player, const CString &cheatName, bool mustBeAlive, bool noMessage ) const
{
	return ::CheatsAllowed( player, cheatName.Data(), mustBeAlive, noMessage );
}

AdminLevel_t CASPlayerFuncs::AdminLevel( CBasePlayer *player ) const
{
	return ::AdminLevel( player );
}

CString CASPlayerFuncs::AdminLevelToString( AdminLevel_t level ) const
{
	return CString( ::AdminLevelToString( level ) );
}

AdminLevel_t CASPlayerFuncs::StringToAdminLevel( const CString &text ) const
{
	return ::StringToAdminLevel( text.Data() );
}

bool CASPlayerFuncs::ReportBug( CBasePlayer *player, const CString &description, CString &fileName )
{
	if ( !description.Length() || description.FindFirstNotOf( " \t" ) == CString::INVALID_INDEX )
		return false;

	time_t timestamp = time( NULL );
	tm *local = localtime( &timestamp );
	char timeText[256];
	char path[MAX_PATH];

	if ( strftime( timeText, sizeof( timeText ), "%Y-%m-%d_%H-%M-%S", local ) &&
	     (unsigned int) snprintf( path, sizeof( path ), "%s/bugreport_%s.txt", "bugreports", timeText ) < sizeof( path ) )
	{
		g_pFileSystem->CreateDirHierarchy( "bugreports", "GAMECONFIG" );
		FileHandle_t file = g_pFileSystem->Open( path, "a", "GAMECONFIG" );

		if ( file )
		{
			fileName = path;
			CASLog::GetInstance()->Log( AS_LOG_DEFAULT, "Logging bug report to \"%s\"\n", path );
			strftime( timeText, sizeof( timeText ), "%Y-%m-%d %H:%M:%S", local );
			g_pFileSystem->FPrintf( file,
			                       "Bug report:\nDate and time: %s\nServer title: %s\nCode revision: #%d\n"
			                       "Reporter: %s\nMap name: %s\nPosition: %f %f %f\nDescription:\n%s\n",
			                       timeText, CVAR_GET_STRING( "hostname" ), SVEN_BUILD_NUMBER, STRING( player->pev->netname ),
			                       STRING( gpGlobals->mapname ), player->pev->origin.x, player->pev->origin.y,
			                       player->pev->origin.z, description.Data() );
			g_pFileSystem->Close( file );
			return true;
		}
	}

	fileName = "";
	return false;
}

bool CASPlayerFuncs::ReportBug( CBasePlayer *player, const CString &description )
{
	CString fileName;

	return ReportBug( player, description, fileName );
}

void CASPlayerFuncs::ApplyMapCfgToPlayer( CBasePlayer *player, bool reEquip )
{
	if ( !player )
		return;

	if ( reEquip )
	{
		player->RemoveAllItems( false, true );
		player->SetItemPickupTimes( gpGlobals->time );
	}

	if ( g_pGameRules->IsMultiplayer() )
		static_cast<CHalfLifeMultiplay *>( g_pGameRules )->LoadEquipCFG( player );
}

void CASPlayerFuncs::RespawnPlayer( CBasePlayer *player, bool moveLiving, bool respawnDead )
{
	UTIL_RespawnPlayer( player, moveLiving, respawnDead );
}

void CASPlayerFuncs::RespawnAllPlayers( bool moveLiving, bool respawnDead )
{
	UTIL_RespawnAllPlayers( moveLiving, respawnDead );
}

bool CASPlayerFuncs::SpawnPointFilterPasses( CBaseEntity *point, CBaseEntity *player )
{
	return UTIL_SpawnPointFilterPasses( point, player );
}

bool CASPlayerFuncs::IsSpawnPointValid( CBaseEntity *point, CBaseEntity *player )
{
	return UTIL_IsSpawnPointValid( point, player );
}

bool CASPlayerFuncs::IsSpawnPointOccupied( CBaseEntity *point )
{
	return UTIL_IsSpawnPointOccupied( point );
}

int CASPlayerFuncs::SharedRandomLong( unsigned int seed, int low, int high )
{
	return UTIL_SharedRandomLong( seed, low, high );
}

float CASPlayerFuncs::SharedRandomFloat( unsigned int seed, float low, float high )
{
	return UTIL_SharedRandomFloat( seed, low, high );
}

int CASPlayerFuncs::GetAmmoIndex( const CString &name )
{
	return CBasePlayer::GetAmmoIndex( name.Data() );
}

CBasePlayer *CASPlayerFuncs::CreateBot( const CString &name )
{
	if ( !name.Length() )
		return NULL;

	edict_t *bot = g_engfuncs.pfnCreateFakeClient( name.Data() );

	if ( !bot )
		return NULL;

	bot->v.frags = 0;
	int index = ENTINDEX( bot );
	char *info = g_engfuncs.pfnGetInfoKeyBuffer( bot );

	g_engfuncs.pfnSetClientKeyValue( index, info, "rate", "3500" );
	g_engfuncs.pfnSetClientKeyValue( index, info, "cl_updaterate", "20" );
	g_engfuncs.pfnSetClientKeyValue( index, info, "cl_lw", "1" );
	g_engfuncs.pfnSetClientKeyValue( index, info, "cl_lc", "1" );
	g_engfuncs.pfnSetClientKeyValue( index, info, "cl_dlmax", "128" );

	char reason[128];

	if ( !ClientConnect( bot, STRING( bot->v.netname ), "127.0.0.1", reason ) )
	{
		REMOVE_ENTITY( bot );
		return NULL;
	}

	ClientPutInServer( bot );

	if ( bot->free || !bot->pvPrivateData )
	{
		ALERT( at_console, "Failed to create CBasePlayer instance for bot '%s'\n", name.Data() );
		ClientDisconnect( bot );
		REMOVE_ENTITY( bot );
		return NULL;
	}

	return (CBasePlayer *) bot->pvPrivateData;
}

void CASPlayerFuncs::BotDisconnect( CBasePlayer *bot )
{
	if ( !bot || !bot->pev )
		return;

	edict_t *entity = bot->pev->pContainingEntity;

	if ( !entity || entity->free || !ENTINDEX( entity ) || !( bot->pev->flags & FL_FAKECLIENT ) )
		return;

	ClientDisconnect( entity );
	REMOVE_ENTITY( bot->pev ? bot->pev->pContainingEntity : NULL );
}
