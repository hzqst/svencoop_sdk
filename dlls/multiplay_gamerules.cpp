/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// teamplay_gamerules.cpp
//
#include	"extdll.h"
#include	"SvenVersion.h"
#include "server_parse.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"weapons.h"
#include	"gamerules.h"
#include "MapCycle.h"
#include "CAdminList.h"
#include "server_support.h"
#include "CASServerManager.h"
#include "CString.h"
#include "CMap.h"
#include "CCfgFile.h"
#include "CASHook.h"
#include "scripted.h"
#include "CServer.h"
#include "server_commands.h"
#include "CVoteManager.h"
#include "maprules.h"
#include "server_filesystem.h"
#include "user_messages.h"

#include	"monsters.h"
#include	"game.h"
#include	"items.h"
#include "item_inventory.h"
#include "game_persistent_inventory.h"
#include "trigger_camera.h"
#include	"voice_gamemgr.h"
#include "CMultiplayGameMgrHelper.h"
#include "observer.h"

#define MIN_INTERMISSION_TIME	8.0f
#define BODYQUE_HIDE_DELAY		30.0f

#include	"hltv.h"
#include <sys/stat.h>

#if !defined ( _WIN32 )
#include <ctype.h>
#endif

extern DLL_GLOBAL CGameRules	*g_pGameRules;
extern DLL_GLOBAL BOOL	g_fGameOver;
extern int gmsgDeathMsg;	// client dll messages
extern int gmsgScoreInfo;
extern int gmsgMOTD;
extern int gmsgServerName;

extern int g_teamplay;

#define ITEM_RESPAWN_TIME	30
#define WEAPON_RESPAWN_TIME	20
#define AMMO_RESPAWN_TIME	20

int CountPlayers( void );

float g_flIntermissionStartTime = 0;

CVoiceGameMgr	g_VoiceGameMgr;

struct playerclass_weapon_t
{
	char                  m_szName[64];
	playerclass_weapon_t *m_pNext;
};

struct playerclass_t
{
	int                   m_iId;
	char                  m_szName[32];
	char                  m_szModel[32];
	int                   m_iMaxSpeed;
	int                   m_iHealth;
	int                   m_iArmor;
	playerclass_weapon_t *m_pWeapons;
	playerclass_t        *m_pNext;
};

static playerclass_t g_PlayerClassList;

//*********************************************************
// Rules for the half-life multiplayer game.
//*********************************************************

#define MAX_BROWSER_HOSTNAME_LENGTH	64
#define MAX_SAFE_VOTE_MAPS			150

CHalfLifeMultiplay :: CHalfLifeMultiplay()
{
	AddCustomServerCommands();

	g_VoiceGameMgr.Init(&g_GameMgrHelper, gpGlobals->maxClients);

	m_flIntermissionEndTime = 0;
	g_flIntermissionStartTime = 0;

	// 11/8/98
	// Modified by YWB:  Server .cfg file is now a cvar, so that
	//  server ops can run multiple game servers, with different server .cfg files,
	//  from a single installed directory.
	// Mapcyclefile is already a cvar.

	// 3/31/99
	// Added lservercfg file cvar, since listen and dedicated servers should not
	// share a single config file. (sjb)
	const char *serverConfig = CVAR_GET_STRING( IS_DEDICATED_SERVER() ? "servercfgfile" : "lservercfgfile" );

	if ( strlen( serverConfig ) )
	{
		char command[128];

		ALERT( at_console, "Executing %s server config file '%s'.\n",
		       IS_DEDICATED_SERVER() ? "dedicated" : "listen", serverConfig );
		UTIL_FormatString( command, sizeof( command ), "exec %s\n", serverConfig );
		SERVER_COMMAND( command );
		SERVER_EXECUTE();
	}

	int hostnameLength = (int) strlen( CVAR_GET_STRING( "hostname" ) );

	if ( hostnameLength > MAX_BROWSER_HOSTNAME_LENGTH )
	{
		ALERT( at_warning, "Server names longer than %d characters will be truncated in the Steam server browser (yours is %d).\n",
		       MAX_BROWSER_HOSTNAME_LENGTH, hostnameLength );
	}

	if ( IS_DEDICATED_SERVER() )
	{
		const char *contact = CVAR_GET_STRING( "sv_contact" );

		if ( strlen( contact ) )
			ALERT( at_console, "Administrative contact: %s\n", contact );
		else
			ALERT( at_warning, "Administrative contact is not set for this server. Please insert contact details (such as an email address, URL, or Discord) in the \"sv_contact\" CVAR for your server.\n" );
	}

	CVoteManager &votes = CServer::GetInstance()->GetVoteManager();

	votes.FreeMapVoteList();

	const char *mapVoteConfig = CVAR_GET_STRING( "mapvotecfgfile" );

	if ( strlen( mapVoteConfig ) )
	{
		char command[128];

		ALERT( at_console, "Executing map vote config file '%s'.\n", mapVoteConfig );
		UTIL_FormatString( command, sizeof( command ), "exec %s\n", mapVoteConfig );
		SERVER_COMMAND( command );
		SERVER_EXECUTE();

		int mapCount = votes.CountMapsInVoteList();

		ALERT( at_console, "%d map(s) loaded into the vote list.\n", mapCount );

		if ( mapCount > MAX_SAFE_VOTE_MAPS )
		{
			ALERT( at_warning, "More than %d maps in vote list may render your server un-joinable for the reliable channel overflowing (yours has %d).\n",
			       MAX_SAFE_VOTE_MAPS, mapCount );
		}
	}

	CASServerManager::GetInstance()->OnMapBegin();

	CMap *map = CMap::GetInstance();

	map->LoadGlobalConfigFile();

	edict_t *world = INDEXENT( 0 );

	if ( world && world->v.speed > 0.0f )
	{
		float range = V_max( 4096.0f, world->v.speed );

		range = V_min( 454046.78125f, range );
		CVAR_SET_FLOAT( "sv_zmax", range );
	}

	map->LoadMapConfigFile();

	g_iSkillLevel = SKILL_MEDIUM;
	map->LoadGlobalSkillFile();
	map->LoadMapSkillFile();

	// Materialize the skill cache from the flat cvars
	CGameRules::RefreshSkillData();
	g_iSkillLevel = SKILL_MEDIUM;
	gSkillData.iSkillLevel = SKILL_MEDIUM;

	GetNextMap();
	RefreshSkillData();

	g_pAdminList = NULL;
	LoadServerAdmins();

	m_flEmptyServerTime = -1.0f;

	if ( IS_DEDICATED_SERVER() )
	{
		float emptyTime = CVAR_GET_FLOAT( "mp_timelimit_empty" );

		if ( emptyTime != 0.0f )
		{
			UTIL_LogPrintf( "Server is empty, map will end in %.2f minute(s) if nobody connects.\n", emptyTime );
			m_flEmptyServerTime = emptyTime;
		}
	}
}

CHalfLifeMultiplay::~CHalfLifeMultiplay()
{
	delete g_pAdminList;
}

bool CHalfLifeMultiplay::ShouldCollide( edict_t *touched, edict_t *other )
{
	CBaseEntity *entity = touched->free ? NULL : CBaseEntity::Instance( touched );
	CBaseEntity *otherEntity = other->free ? NULL : CBaseEntity::Instance( other );

	if ( otherEntity && ( other->v.flags & FL_MONSTER ) )
	{
		CBaseMonster *monster = (CBaseMonster *) otherEntity;

		if ( ( monster->m_MonsterState == MONSTERSTATE_SCRIPT || otherEntity->pev->iuser2 > 0 )
		     && ( touched->v.flags & FL_CLIENT ) )
			return false;
	}

	if ( entity && otherEntity )
	{
		if ( entity->HasEntityFlag( 2 ) && entity->pev->owner )
		{
			edict_t *owner = entity->pev->owner;
			CBaseEntity *ownerEntity = owner->free ? NULL : CBaseEntity::Instance( owner );

			if ( ownerEntity && PlayerRelationship( ownerEntity, otherEntity ) == GR_TEAMMATE )
				return false;
		}
		else if ( otherEntity->HasEntityFlag( 2 ) && otherEntity->pev->owner )
		{
			edict_t *owner = otherEntity->pev->owner;
			CBaseEntity *ownerEntity = owner->free ? NULL : CBaseEntity::Instance( owner );

			if ( ownerEntity && PlayerRelationship( ownerEntity, entity ) == GR_TEAMMATE )
				return false;
		}
	}

	if ( !( other->v.flags & FL_CLIENT ) || !( touched->v.flags & FL_MONSTER ) )
		return true;

	if ( touched->v.iuser2 > 0 )
		return false;

	if ( !entity || !entity->IsMonster() )
		return true;

	CBaseMonster *monster = (CBaseMonster *) entity;

	if ( monster->m_MonsterState != MONSTERSTATE_SCRIPT || !monster->m_pCine )
		return true;

	CCineMonster *cine = monster->m_pCine;
	edict_t *cineEdict = cine->edict();

	if ( !cineEdict || cineEdict->free || CBaseEntity::Instance( cineEdict ) != cine )
		return true;

	Vector2D distance( monster->pev->origin.x - other->v.origin.x,
	                   monster->pev->origin.y - other->v.origin.y );

	if ( !( distance.Length() < 32.0f ) || !monster->Intersects( otherEntity ) )
		return true;

	UTIL_MakeVectors( monster->pev->angles );

	Vector offsets[4] =
	{
		gpGlobals->v_forward * 48.0f,
		gpGlobals->v_right * -48.0f,
		gpGlobals->v_right * 48.0f,
		gpGlobals->v_forward * -48.0f
	};
	TraceResult trace;
	Vector2D push;

	trace.vecEndPos = g_vecZero;
	trace.vecPlaneNormal = g_vecZero;

	// With all four traces blocked, this reuses the edict pointer's float bits
	memcpy( &push.x, &other, sizeof( push.x ) );
	push.y = push.x;

	for ( int index = 0; index < 4; ++index )
	{
		Vector probe = monster->pev->origin + offsets[index];

		UTIL_TraceLine( probe + Vector( 0, 0, 64 ), probe, dont_ignore_monsters, monster->edict(), &trace );

		if ( !trace.fStartSolid && trace.flFraction == 1.0f )
		{
			Vector2D direction( probe.x - other->v.origin.x, probe.y - other->v.origin.y );

			push = direction.Normalize() * 64.0f;
			break;
		}
	}

	if ( other->v.velocity.Length2D() < 320.0f )
	{
		other->v.velocity.x += push.x;
		other->v.velocity.y += push.y;
	}

	return false;
}

void CHalfLifeMultiplay::ClientPutIntoServer( CBasePlayer *player )
{
	if ( m_flEmptyServerTime == -1.0f )
		return;

	m_flEmptyServerTime = -1.0f;
	UTIL_LogPrintf( "Server is no longer empty, idle timer disabled.\n" );
}

playerclass_t *CHalfLifeMultiplay::ClassForId( int id )
{
	if ( !id )
		return NULL;

	for ( playerclass_t *playerClass = &g_PlayerClassList; playerClass; playerClass = playerClass->m_pNext )
	{
		if ( playerClass->m_iId == id )
			return playerClass;
	}

	return NULL;
}

void CHalfLifeMultiplay::SetupClass( CBasePlayer *player )
{
	playerclass_t *playerClass = ClassForId( player->m_iPlayerClass );

	if ( !playerClass )
		return;

	player->pev->health = playerClass->m_iHealth;
	player->pev->max_health = playerClass->m_iHealth;
	player->pev->armortype = playerClass->m_iArmor;
	player->pev->armorvalue = playerClass->m_iArmor;
	player->pev->maxspeed = playerClass->m_iMaxSpeed;

	for ( playerclass_weapon_t *weapon = playerClass->m_pWeapons; weapon; weapon = weapon->m_pNext )
	{
		// The loop grants the first name again for every list entry
		player->GiveNamedItem( playerClass->m_pWeapons->m_szName );
	}

	if ( player->pev->max_health == 0.0f )
		player->pev->max_health = player->pev->health;

	if ( player->pev->armortype == 0.0f )
		player->pev->armortype = player->pev->armorvalue;

	if ( player->pev->maxspeed == 0.0f )
		player->pev->maxspeed = CVAR_GET_FLOAT( "sv_maxspeed" );

	char *info = g_engfuncs.pfnGetInfoKeyBuffer( player->edict() );

	g_engfuncs.pfnSetClientKeyValue( player->entindex(), info, (char *) "model", playerClass->m_szModel );
}

void CHalfLifeMultiplay::ClientUserInfoChanged( CBasePlayer *player, char *infobuffer )
{
	if ( !player )
		return;

	int clientIndex = player->entindex();
	const char *hideXtra = g_engfuncs.pfnInfoKeyValue( infobuffer, "cl_hidextra" );

	if ( hideXtra && isdigit( *hideXtra ) )
		player->m_iHideXtra = atoi( hideXtra );
	else
	{
		char previous[64 + 1] = { 0 };

		UTIL_FormatString( previous, sizeof( previous ), "%d", player->m_iHideXtra );
		g_engfuncs.pfnSetClientKeyValue( clientIndex, infobuffer, "cl_hidextra", previous );
	}

	const char *hideAdmin = g_engfuncs.pfnInfoKeyValue( infobuffer, "cl_hideadmin" );

	if ( hideAdmin && isdigit( *hideAdmin ) )
		player->m_iHideAdmin = atoi( hideAdmin );
	else
	{
		char previous[64 + 1] = { 0 };

		UTIL_FormatString( previous, sizeof( previous ), "%d", player->m_iHideAdmin );
		g_engfuncs.pfnSetClientKeyValue( clientIndex, infobuffer, "cl_hideadmin", previous );
	}

	const char *autoSwitch = g_engfuncs.pfnInfoKeyValue( infobuffer, "hud_weaponautoswitch" );

	if ( autoSwitch && isdigit( *autoSwitch ) )
		player->m_iWeaponAutoSwitch = atoi( autoSwitch );
	else
	{
		char previous[64 + 1] = { 0 };

		UTIL_FormatString( previous, sizeof( previous ), "%d", player->m_iWeaponAutoSwitch );
		g_engfuncs.pfnSetClientKeyValue( clientIndex, infobuffer, "hud_weaponautoswitch", previous );
	}

	player->m_fIsImpostor = player->IsImpostor( g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );

	player->SendScoreInfo( NULL );

	char *requestedModel = g_engfuncs.pfnInfoKeyValue( infobuffer, (char *) "model" );
	char *previousModel = g_engfuncs.pfnInfoKeyValue( infobuffer, (char *) "cl_model" );

	if ( player->m_fOverridePlayerModel )
	{
		if ( !player->m_fOverridePlayerModelAcknowledged )
		{
			player->m_fOverridePlayerModelAcknowledged = true;
		}
		else if ( player->ShouldForcePlayerModel() )
		{
			if ( stricmp( requestedModel, player->m_szOverriddenPlayerModel ) )
			{
				g_engfuncs.pfnSetClientKeyValue( clientIndex, infobuffer, (char *) "cl_model", requestedModel );
				g_engfuncs.pfnSetClientKeyValue( clientIndex, infobuffer, (char *) "model", player->m_szOverriddenPlayerModel );
			}
		}
		else
		{
			if ( previousModel[0] )
				g_engfuncs.pfnSetClientKeyValue( clientIndex, infobuffer, (char *) "cl_model", (char *) "" );

			player->ResetOverriddenPlayerModel( false, false );
		}
	}
	else
	{
		CMap *map = CMap::GetInstance();

		if ( map->HasForcedPlayerModels() )
		{
			for ( unsigned int index = 0; index < map->m_uiForcedPlayerModels; ++index )
			{
				if ( !stricmp( requestedModel, map->GetForcedPlayerModel( index ) ) )
					return;
			}

			g_engfuncs.pfnSetClientKeyValue( clientIndex, infobuffer, (char *) "cl_model", requestedModel );

			int chosen = RANDOM_LONG( 0, (int) map->m_uiForcedPlayerModels - 1 );

			g_engfuncs.pfnSetClientKeyValue( clientIndex, infobuffer, (char *) "model", (char *) map->GetForcedPlayerModel( chosen ) );

			const char *list = STRING( map->m_iszForcedPlayerModels );
			char message[256];

			if ( strlen( list ) < 180 )
			{
				UTIL_CopyString( message, "* On this map, player models are restricted to:\n", sizeof( message ) );
				UTIL_SayText( message, player );
				sprintf( message, "* %s\n", list );
				UTIL_SayText( message, player );
			}
			else
			{
				UTIL_CopyString( message, "* Player models are restricted on this map.\n", sizeof( message ) );
				UTIL_SayText( message, player );
			}
		}
	}

	char *topColor = g_engfuncs.pfnInfoKeyValue( infobuffer, (char *) "topcolor" );
	char *bottomColor = g_engfuncs.pfnInfoKeyValue( infobuffer, (char *) "bottomcolor" );
	int bottomValue = atoi( bottomColor );
	int topValue = atoi( topColor );

	player->SetColors( topValue, bottomValue );
}

void CHalfLifeMultiplay::LoadEquipCFG( CBasePlayer *player )
{
	CCfgFile *config = CMap::GetInstance()->m_pMapConfigFile;

	if ( config )
		config->ApplyCfgOnPlayer( player );

	if ( CVAR_GET_FLOAT( "mp_nomedkit" ) == 0.0f )
		player->GiveNamedItem( "weapon_medkit" );
}

void CHalfLifeMultiplay::SendMapInfo( CBasePlayer *player, const char *fileName )
{
	if ( !fileName )
		return;

	enum
	{
		MAX_MAPS = 2048,
		MAP_NAME_SIZE = 33,
		MAPS_PER_MESSAGE = 5
	};
	char( *maps )[MAP_NAME_SIZE] = new char[MAX_MAPS][MAP_NAME_SIZE];
	unsigned int count = 0;

	memset( maps, 0, MAX_MAPS * MAP_NAME_SIZE );

	if ( !strcmp( fileName, "votemaplist" ) )
	{
		CVoteManager &votes = CServer::GetInstance()->GetVoteManager();

		for ( unsigned int index = 0; count < MAX_MAPS; ++index )
		{
			const char *name = votes.GetMapInVoteList( index );

			if ( !name )
				break;

			if ( name[0] )
				UTIL_CopyString( maps[count++], name, MAP_NAME_SIZE );
		}
	}
	else
	{
		FileHandle_t file = g_pFileSystem->Open( fileName, "r", "GAME" );

		if ( file )
		{
			char line[256];

			memset( line, 0, sizeof( line ) );

			while ( count < MAX_MAPS && g_pFileSystem->ReadLine( line, sizeof( line ), file ) )
			{
				unsigned int length = strlen( line );

				if ( length && line[length - 1] == '\n' )
					line[--length] = 0;

				if ( !length )
					continue;

				if ( line[length - 1] == '\r' )
					line[--length] = 0;

				char *format = strstr( line, "%n" );

				if ( format )
					format[1] = 's';

				if ( length )
					UTIL_CopyString( maps[count++], line, MAP_NAME_SIZE );
			}

			g_pFileSystem->Close( file );
		}
		else
			ALERT( at_warning, "Could not open map list file '%s', map vote menu will be useless!\n", fileName );
	}

	for ( unsigned int first = 0; first < count; first += MAPS_PER_MESSAGE )
	{
		unsigned int end = V_min( first + MAPS_PER_MESSAGE, count );

		MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgMapList, NULL, player->edict() );

			if ( !first )
			{
				WRITE_BYTE( 0 );
				WRITE_SHORT( count );
			}
			else
			{
				WRITE_BYTE( 1 );
				WRITE_SHORT( first );
				WRITE_SHORT( end );
			}

			for ( unsigned int index = first; index < end; ++index )
				WRITE_STRING( maps[index] );

		MESSAGE_END();
	}

	MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgMapList, NULL, player->edict() );
		WRITE_BYTE( 123 );
	MESSAGE_END();

	delete[] maps;
}

BOOL CHalfLifeMultiplay::ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if(g_VoiceGameMgr.ClientCommand(pPlayer, pcmd))
		return TRUE;

	if ( !strncmp( pcmd, "CLASS_", 6 ) )
	{
		static const char *classCommands[] =
		{
			"CLASS_1", "CLASS_2", "CLASS_3", "CLASS_4", "CLASS_5",
			"CLASS_6", "CLASS_7", "CLASS_8", "CLASS_9", "CLASS_10"
		};
		int id;

		for ( id = 1; id <= 10; ++id )
		{
			if ( !strcmp( pcmd, classCommands[id - 1] ) )
				break;
		}

		pPlayer->m_iPlayerClass = id;
		pPlayer->pev->playerclass = id;
		return TRUE;
	}

	if ( CGameRules::ClientCommand(pPlayer, pcmd) )
		return TRUE;

	if ( HandleCameraCommand( pPlayer, pcmd ) )
		return TRUE;

	return FALSE;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::RefreshSkillData( void )
{
}

// longest the intermission can last, in seconds
#define MAX_INTERMISSION_TIME		60

extern cvar_t timeleft, fragsleft;

extern cvar_t mp_chattime;

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: Think ( void )
{
	///// Check game rules /////
	static int last_frags;
	static int last_time;
	static int last_emptytime;

	int frags_remaining = 0;
	int time_remaining = 0;

	if ( g_fGameOver )   // someone else quit the game already
	{
		// check to see if we should change levels now
		if ( m_flIntermissionEndTime < gpGlobals->time && ( g_flIntermissionStartTime + MIN_INTERMISSION_TIME ) < gpGlobals->time )
			ChangeLevel(); // intermission is over

		return;
	}

	if ( gpGlobals->time >= m_flNextBodyqueHideTime )
	{
		CBaseEntity *pBodyque = NULL;

		while ( ( pBodyque = UTIL_FindEntityByClassname( pBodyque, "bodyque" ) ) != NULL )
			pBodyque->pev->effects |= EF_NODRAW;

		m_flNextBodyqueHideTime = gpGlobals->time + BODYQUE_HIDE_DELAY;
	}

	float flTimeLimit = timelimit.value * 60;
	float flFragLimit = fraglimit.value;
	float flEmptyTimeLimit = mp_timelimit_empty.value * 60.0f;
	int empty_time_remaining = 0;

	if ( flEmptyTimeLimit != 0.0f )
	{
		float remaining = flEmptyTimeLimit;

		if ( m_flEmptyServerTime >= 0.0f )
			remaining -= gpGlobals->time - m_flEmptyServerTime;

		empty_time_remaining = (int) remaining;
	}

	time_remaining = (int)(flTimeLimit ? ( flTimeLimit - gpGlobals->time ) : 0);
	
	if ( flTimeLimit != 0 && gpGlobals->time >= flTimeLimit && CVAR_GET_FLOAT( "mp_notimelimit" ) == 0.0f )
	{
		GoToIntermission();
		return;
	}

	if ( IS_DEDICATED_SERVER() && mp_timelimit_empty.value != 0.0f
	     && m_flEmptyServerTime >= 0.0f && empty_time_remaining <= 0 )
	{
		UTIL_LogPrintf( "No players have been present for %.2f minutes, changing map.\n", mp_timelimit_empty.value );
		GoToIntermission();
		return;
	}

	if ( flFragLimit )
	{
		int bestfrags = 0x7FFFFFFF;
		int remain;

		// check if any player is over the frag limit
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

			if ( pPlayer && pPlayer->pev->frags >= flFragLimit )
			{
				GoToIntermission();
				return;
			}


			if ( pPlayer )
			{
				remain = flFragLimit - pPlayer->pev->frags;
				if ( remain < bestfrags )
				{
					bestfrags = remain;
				}
			}

		}
		frags_remaining = bestfrags;
	}

	// Updates when frags change
	if ( frags_remaining != last_frags )
	{
		g_engfuncs.pfnCvar_DirectSet( &fragsleft, UTIL_VarArgs( "%i", frags_remaining ) );
	}

	// Updates once per second
	if ( timeleft.value != last_time )
	{
		g_engfuncs.pfnCvar_DirectSet( &timeleft, UTIL_VarArgs( "%i", time_remaining ) );
	}

	if ( timeleft_empty.value != last_emptytime )
		g_engfuncs.pfnCvar_DirectSet( &timeleft_empty, UTIL_VarArgs( "%i", empty_time_remaining ) );

	last_emptytime = empty_time_remaining;
	last_frags = frags_remaining;
	last_time  = time_remaining;

	g_VoiceGameMgr.Update(gpGlobals->frametime);
}


//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsMultiplayer( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsDeathmatch( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsCoOp( void )
{
	return gpGlobals->coop != 0.0f;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::FShouldSwitchWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
	if ( !pWeapon->CanDeploy() || pPlayer->GetWeaponsBlocked() )
		return FALSE;

	CBasePlayerItem *activeItem = pPlayer->m_pActiveItem;

	if ( !activeItem )
		return TRUE;

	if ( pPlayer->IsExclusiveHoldItemActive() )
		return FALSE;

	if ( pWeapon->m_bExclusiveHold )
		return TRUE;

	if ( !activeItem->CanHolster() )
		return FALSE;

	if ( pPlayer->m_iWeaponAutoSwitch <= 0 )
	{
		pPlayer->m_iWeaponAutoSwitch = 0;
		return FALSE;
	}

	if ( pPlayer->m_iWeaponAutoSwitch >= 2 )
	{
		pPlayer->m_iWeaponAutoSwitch = 2;
		return TRUE;
	}

	return pWeapon->iWeight() > activeItem->iWeight();
}

bool CHalfLifeMultiplay :: GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon )
{
	if ( pCurrentWeapon && !pCurrentWeapon->CanHolster() )
		return FALSE;

	CBasePlayerItem *pBest = NULL;
	int iBestWeight = -1;

	for ( int i = 0 ; i < MAX_ITEM_TYPES ; ++i )
	{
		for ( CBasePlayerItem *pCheck = pPlayer->m_rgpPlayerItems[ i ]; pCheck; pCheck = pCheck->m_pNext )
		{
			if ( !pCurrentWeapon || ( pCheck->iWeight() > -1 && pCheck->iWeight() == pCurrentWeapon->iWeight() && pCheck != pCurrentWeapon ) )
			{
				if ( ( pCheck->CanDeploy() || pCheck->m_bExclusiveHold ) && pPlayer->SwitchWeapon( pCheck ) )
					return TRUE;
			}
			else if ( pCheck->iWeight() > iBestWeight && pCheck != pCurrentWeapon && pCheck->CanDeploy() )
				{
					// if this weapon is useable, flag it as the best
					iBestWeight = pCheck->iWeight();
					pBest = pCheck;
				}
			}
	}

	// if we make it here, we've checked all the weapons and found no useable
	// weapon in the same catagory as the current weapon.

	// if pBest is null, we didn't find ANYTHING. Shouldn't be possible- should always
	// at least get the crowbar, but ya never know.
	if ( !pBest )
		return FALSE;

	pPlayer->SwitchWeapon( pBest );

	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ] )
{
	g_VoiceGameMgr.ClientConnected(pEntity);

	if ( !CServer::GetInstance()->ClientConnected( pEntity, pszName, pszAddress, szRejectReason ) )
		return FALSE;

	MESSAGE_BEGIN( MSG_ONE, SVC_ROOMTYPE, NULL, pEntity );
	WRITE_SHORT( 0 );
	MESSAGE_END();

	bool allow = true;

	if ( CASServerManager::GetInstance()->IsScriptLifecycleEntered() )
	{
		CString playerName( pszName );
		CString playerAddress( pszAddress );
		CString reason( szRejectReason );

		g_ClientConnectedHook.Call( 0, pEntity, &playerName, &playerAddress, &allow, &reason );

		if ( !allow )
			UTIL_CopyString( szRejectReason, reason.Data(), 128 );
	}

	return allow;
}

extern int gmsgSayText;
extern int gmsgGameMode;

void CHalfLifeMultiplay :: UpdateGameMode( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgGameMode, NULL, pPlayer->edict() );
		WRITE_BYTE( 0 );  // game mode none
	MESSAGE_END();
}

void CHalfLifeMultiplay :: InitHUD( CBasePlayer *pl )
{
	// notify other clients of player joining the game
	UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs( "%s has joined the game\n", 
		( pl->pev->netname && STRING(pl->pev->netname)[0] != 0 ) ? STRING(pl->pev->netname) : "unconnected" ) );

	// team match?
	if ( g_teamplay )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" entered the game\n",  
			STRING( pl->pev->netname ), 
			GETPLAYERUSERID( pl->edict() ),
			GETPLAYERAUTHID( pl->edict() ),
			g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pl->edict() ), "model" ) );
	}
	else
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%i>\" entered the game\n",  
			STRING( pl->pev->netname ), 
			GETPLAYERUSERID( pl->edict() ),
			GETPLAYERAUTHID( pl->edict() ),
			GETPLAYERUSERID( pl->edict() ) );
	}

	UpdateGameMode( pl );

	// Send a score update to enable the HUD scoreboard
	pl->InitScoreInfo();

	// loop through all active players and send their score info to the new client
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		// FIXME:  Probably don't need to cast this just to read m_iDeaths
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr )
		{
			plr->SendScoreInfo( pl->edict() );

			MESSAGE_BEGIN( MSG_ONE, gmsgSpectator, NULL, pl->edict() );
				WRITE_BYTE( i );
				WRITE_BYTE( plr->pev->iuser1 != 0 );
			MESSAGE_END();
		}
	}

	memset( pl->m_flNextWeaponPickup, 0, sizeof( pl->m_flNextWeaponPickup ) );

	CVoteManager &votes = CServer::GetInstance()->GetVoteManager();

	SendMapInfo( pl, votes.GetMapInVoteList( 0 ) ? "votemaplist"
	                                          : CVAR_GET_STRING( "mapcyclefile" ) );

	SendMOTDToClient( pl->edict() );

	MESSAGE_BEGIN( MSG_ONE, gmsgTimeEnd, NULL, pl->edict() );
		WRITE_LONG( (int) ( CVAR_GET_FLOAT( "mp_timelimit" ) * 60.0f - gpGlobals->time ) );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgServerVer, NULL, pl->edict() );
		WRITE_STRING( SVEN_GAME_VERSION_STRING );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgServerBuild, NULL, pl->edict() );
		WRITE_STRING( SVEN_BUILD_STRING );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgNextMap, NULL, pl->edict() );
		WRITE_STRING( CVAR_GET_STRING( "mp_nextmap_cycle" ) );
	MESSAGE_END();

	if ( g_fGameOver )
	{
		MESSAGE_BEGIN( MSG_ONE, SVC_INTERMISSION, NULL, pl->edict() );
		MESSAGE_END();
	}
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: ClientDisconnected( edict_t *pClient )
{
	if ( IS_DEDICATED_SERVER() && mp_timelimit_empty.value != 0.0f && CountPlayers() == 0 )
	{
		m_flEmptyServerTime = gpGlobals->time;
		UTIL_LogPrintf( "Last player disconnected, map will end in %.2f minute(s) if nobody connects.\n", mp_timelimit_empty.value );
	}
	else
		m_flEmptyServerTime = -1.0f;

	if ( pClient )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgSpectator );
			WRITE_BYTE( ENTINDEX( pClient ) );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );

		if ( pPlayer )
		{
			FireTargets( "game_playerleave", pPlayer, pPlayer, USE_TOGGLE, 0 );

			// team match?
			if ( g_teamplay )
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" disconnected\n",  
					STRING( pPlayer->pev->netname ), 
					GETPLAYERUSERID( pPlayer->edict() ),
					GETPLAYERAUTHID( pPlayer->edict() ),
					g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model" ) );
			}
			else
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%i>\" disconnected\n",  
					STRING( pPlayer->pev->netname ), 
					GETPLAYERUSERID( pPlayer->edict() ),
					GETPLAYERAUTHID( pPlayer->edict() ),
					GETPLAYERUSERID( pPlayer->edict() ) );
			}

			pPlayer->RemoveAllItems( TRUE );// destroy all of the players weapons and items

			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *pOther = (CBasePlayer *) UTIL_PlayerByIndex( i );

				if ( !pOther || pOther == pPlayer )
					continue;

				CObserver *pObserver = pOther->GetObserver();

				if ( pObserver->GetObserverTarget() == pPlayer )
					pObserver->ResetObserverTarget();
			}
		}
	}
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay :: FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	int iFallDamage = (int)falldamage.value;

	switch ( iFallDamage )
	{
	case 1://progressive
		pPlayer->m_flFallVelocity -= PLAYER_MAX_SAFE_FALL_SPEED;
		return pPlayer->m_flFallVelocity * DAMAGE_FOR_FALL_SPEED;
		break;
	case -1:// none
		return 0;
		break;
	default:
	case 0:// fixed
		return 10;
		break;
	}
} 

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: PlayerThink( CBasePlayer *pPlayer )
{
	if ( g_fGameOver )
	{
		// check for button presses
		if ( pPlayer->m_afButtonPressed & ( IN_DUCK | IN_ATTACK | IN_ATTACK2 | IN_USE | IN_JUMP ) )
			m_iEndIntermissionButtonHit = TRUE;

		// clear attack/use commands from player
		pPlayer->m_afButtonPressed = 0;
		pPlayer->pev->button = 0;
		pPlayer->m_afButtonReleased = 0;
	}
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: PlayerSpawn( CBasePlayer *pPlayer )
{
	BOOL		addDefault = TRUE;
	CBaseEntity	*pWeaponEntity = NULL;

	pPlayer->pev->weapons |= (1<<WEAPON_SUIT);
	pPlayer->SendScoreInfo( NULL );

	while ( ( pWeaponEntity = UTIL_FindEntityByClassname( pWeaponEntity, "game_player_equip" )) != NULL )
	{
		if ( !GamePlayerEquipOnSpawn( pWeaponEntity, pPlayer ) )
			addDefault = FALSE;
	}

	if ( addDefault )
		LoadEquipCFG( pPlayer );

	if ( g_pGamePersistentInventory )
	{
		if ( !stricmp( g_pGamePersistentInventory->m_szMapName, STRING( gpGlobals->mapname ) ) )
			g_pGamePersistentInventory->Load( pPlayer );
		else
		{
			delete g_pGamePersistentInventory;
			g_pGamePersistentInventory = NULL;
		}
	}

	SetupClass( pPlayer );

	MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgServerName, NULL, pPlayer->edict() );
		WRITE_STRING( CVAR_GET_STRING( "hostname" ) );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgNextMap, NULL, pPlayer->edict() );
		WRITE_STRING( CVAR_GET_STRING( "mp_nextmap_cycle" ) );
	MESSAGE_END();

	g_PlayerSpawnHook.Call( 0, pPlayer );// 4 full reloads
	}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	return TRUE;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay :: FlPlayerSpawnTime( CBasePlayer *pPlayer )
{
	return gpGlobals->time;//now!
}

BOOL CHalfLifeMultiplay :: AllowAutoTargetCrosshair( void )
{
	return CVAR_GET_FLOAT( "mp_autocrosshair" ) != 0.0f;
}

//=========================================================
// IPointsForKill - how many points awarded to anyone
// that kills this player?
//=========================================================
int CHalfLifeMultiplay :: IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	return 1;
}


//=========================================================
// PlayerKilled - someone/something killed this player
//=========================================================
void CHalfLifeMultiplay :: PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	DeathNotice( pVictim, pKiller, pInflictor );

	pVictim->m_iDeaths += 1;


	FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );
	CBasePlayer *peKiller = NULL;
	CBaseEntity *ktmp = CBaseEntity::Instance( pKiller );
	if ( ktmp && (ktmp->Classify() == CLASS_PLAYER) )
		peKiller = (CBasePlayer*)ktmp;

	if ( pVictim->pev == pKiller )  
	{  // killed self
		pKiller->frags -= 1;
	}
	else if ( ktmp && ktmp->IsPlayer() )
	{
		// if a player dies in a deathmatch game and the killer is a client, award the killer some points
		pKiller->frags += IPointsForKill( peKiller, pVictim );
		
		FireTargets( "game_playerkill", ktmp, ktmp, USE_TOGGLE, 0 );
	}
	else
	{  // killed by the world
		pKiller->frags -= 1;
	}

	// update the scores
	// killed scores
	pVictim->SendScoreInfo( NULL );

	// killers score, if it's a player
	CBaseEntity *ep = CBaseEntity::Instance( pKiller );
	if ( ep && ep->Classify() == CLASS_PLAYER )
	{
		CBasePlayer *PK = (CBasePlayer*)ep;

		PK->SendScoreInfo( NULL );

		// let the killer paint another decal as soon as he'd like.
		PK->m_flNextDecalTime = gpGlobals->time;
	}
#ifndef HLDEMO_BUILD
	if ( pVictim->HasNamedPlayerItem("weapon_satchel") )
	{
		DeactivateSatchels( pVictim );
	}
#endif
}

//=========================================================
// Deathnotice. 
//=========================================================
void CHalfLifeMultiplay::DeathNotice( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pevInflictor )
{
	// Work out what killed the player, and send a message to all clients about it
	CBaseEntity *Killer = CBaseEntity::Instance( pKiller );

	const char *killer_weapon_name = "world";		// by default, the player is killed by the world
	int killer_index = 0;
	
	// Hack to fix name change
	const char *tau = "tau_cannon";
	const char *gluon = "gluon gun";

	if ( pKiller->flags & FL_CLIENT )
	{
		killer_index = ENTINDEX(ENT(pKiller));
		
		if ( pevInflictor )
		{
			if ( pevInflictor == pKiller )
			{
				// If the inflictor is the killer,  then it must be their current weapon doing the damage
				CBasePlayer *pPlayer = (CBasePlayer*)CBaseEntity::Instance( pKiller );
				
				if ( pPlayer->m_pActiveItem )
				{
					killer_weapon_name = pPlayer->m_pActiveItem->pszName();
				}
			}
			else
			{
				killer_weapon_name = STRING( pevInflictor->classname );  // it's just that easy
			}
		}
	}
	else
	{
		killer_weapon_name = STRING( pevInflictor->classname );
	}

	// strip the monster_* or weapon_* from the inflictor's classname
	if ( strncmp( killer_weapon_name, "weapon_", 7 ) == 0 )
		killer_weapon_name += 7;
	else if ( strncmp( killer_weapon_name, "monster_", 8 ) == 0 )
		killer_weapon_name += 8;
	else if ( strncmp( killer_weapon_name, "func_", 5 ) == 0 )
		killer_weapon_name += 5;

	MESSAGE_BEGIN( MSG_ALL, gmsgDeathMsg );
		WRITE_BYTE( killer_index );						// the killer
		WRITE_BYTE( ENTINDEX(pVictim->edict()) );		// the victim
		WRITE_STRING( killer_weapon_name );		// what they were killed by (should this be a string?)
	MESSAGE_END();

	// replace the code names with the 'real' names
	if ( !strcmp( killer_weapon_name, "egon" ) )
		killer_weapon_name = gluon;
	else if ( !strcmp( killer_weapon_name, "gauss" ) )
		killer_weapon_name = tau;

	if ( pVictim->pev == pKiller )  
	{
		// killed self

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\"\n",  
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );		
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" committed suicide with \"%s\"\n",  
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );		
		}
	}
	else if ( pKiller->flags & FL_CLIENT )
	{
		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\" with \"%s\"\n",  
				STRING( pKiller->netname ),
				GETPLAYERUSERID( ENT(pKiller) ),
				GETPLAYERAUTHID( ENT(pKiller) ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( ENT(pKiller) ), "model" ),
				STRING( pVictim->pev->netname ),
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" killed \"%s<%i><%s><%i>\" with \"%s\"\n",  
				STRING( pKiller->netname ),
				GETPLAYERUSERID( ENT(pKiller) ),
				GETPLAYERAUTHID( ENT(pKiller) ),
				GETPLAYERUSERID( ENT(pKiller) ),
				STRING( pVictim->pev->netname ),
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );
		}
	}
	else
	{ 
		// killed by the world

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\" (world)\n",
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ), 
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );		
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" committed suicide with \"%s\" (world)\n",
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ), 
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );		
		}
	}

	char szText[512];

	memset( szText, 0, sizeof( szText ) );

	if ( Killer && (Killer->pev->flags & FL_MONSTER) )
	{
		sprintf( szText, "%s", STRING( pVictim->pev->netname ) );
		strcat( szText, " was killed by " );
		strcat( szText, ( strcspn( killer_weapon_name, "aeiouAEIOU" ) == 0 ) ? "an " : "a " );
		strcat( szText, killer_weapon_name );
		strcat( szText, ".\n" );
	}
	else if ( pVictim->pev == pKiller )
	{
		sprintf( szText, "%s", STRING( pVictim->pev->netname ) );
		strcat( szText, " committed suicide.\n" );
	}
	else if ( pKiller->flags & FL_CLIENT )
	{
		sprintf( szText, "%s", STRING( pKiller->netname ) );
		strcat( szText, " : " );
		strcat( szText, killer_weapon_name );
		strcat( szText, " : " );
		strcat( szText, STRING( pVictim->pev->netname ) );
		strcat( szText, "\n" );
	}
	else if ( FClassnameIs( pKiller, "worldspawn" ) )
	{
		sprintf( szText, "%s", STRING( pVictim->pev->netname ) );
		strcat( szText, ( pVictim->pev->flags & FL_DUCKING ) ? " drowned.\n" : " fell or something.\n" );
	}
	else if ( pKiller->solid == SOLID_BSP )
	{
		sprintf( szText, "%s", STRING( pVictim->pev->netname ) );

		if ( FClassnameIs( pKiller, "func_tank" ) || FClassnameIs( pKiller, "func_tanklaser" ) || FClassnameIs( pKiller, "func_tankrocket" ) )
			strcat( szText, " was killed by a mounted gun.\n" );
		else
			strcat( szText, " was smashed.\n" );
	}
	else if ( FClassnameIs( pKiller, "monster_mortar" ) )
	{
		sprintf( szText, "%s", STRING( pVictim->pev->netname ) );
		strcat( szText, " was blown to shreds by a mortar.\n" );
	}
	else
	{
		int bits = pVictim->m_bitsDamageType;
		const char *pszPhrase = NULL;

		if ( bits & DMG_CRUSH )
			pszPhrase = " was crushed";
		else if ( bits & DMG_BULLET )
			pszPhrase = " was shot";
		else if ( bits & DMG_SLASH )
			pszPhrase = " has been chopped";
		else if ( bits & DMG_BURN )
			pszPhrase = " burned down";
		else if ( bits & ( DMG_FREEZE | DMG_SLOWFREEZE ) )
			pszPhrase = " froze to death";
		else if ( bits & DMG_FALL )
			pszPhrase = " fell";
		else if ( bits & DMG_BLAST )
			pszPhrase = " blew up";
		else if ( bits & DMG_CLUB )
			pszPhrase = " was crowbared";
		else if ( bits & DMG_SHOCK )
			pszPhrase = " was electrocuted";
		else if ( bits & DMG_SONIC )
			pszPhrase = " was killed by sound";
		else if ( bits & DMG_ENERGYBEAM )
			pszPhrase = " was cut by a laser";
		else if ( bits & DMG_ALWAYSGIB )
			pszPhrase = " was gibbed";
		else if ( bits & DMG_BUCKSHOT )
			pszPhrase = " played catapult";
		else if ( bits & DMG_DROWN )
			pszPhrase = " drank too much";
		else if ( bits & DMG_PARALYZE )
			pszPhrase = " was paralyzed";
		else if ( bits & ( DMG_NERVEGAS | DMG_POISON | DMG_ACID ) )
			pszPhrase = " has been poisoned";
		else if ( bits & DMG_ACID )
			pszPhrase = " has been dissolved";
		else if ( bits & DMG_RADIATION )
			pszPhrase = " went nuclear";
		else if ( bits & DMG_DROWNRECOVER )
			pszPhrase = " was baked like a cake";
		else if ( bits & DMG_MORTAR )
			pszPhrase = " was blown to shreds by a mortar";
		else if ( bits & DMG_SNIPER )
			pszPhrase = " was executed by a Sniper";

		sprintf( szText, "%s", STRING( pVictim->pev->netname ) );

		if ( pszPhrase )
		{
			strcat( szText, pszPhrase );
			strcat( szText, ".\n" );
		}
		else
			strcat( szText, " died mysteriously.\n" );
	}

	UTIL_ClientPrintAll( HUD_PRINTTALK, szText );
}

//=========================================================
// PlayerGotWeapon - player has grabbed a weapon that was
// sitting in the world
//=========================================================
void CHalfLifeMultiplay :: PlayerGotWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
}

//=========================================================
// FlWeaponRespawnTime - what is the time in the future
// at which this weapon may spawn?
//=========================================================
float CHalfLifeMultiplay :: FlWeaponRespawnTime( CBasePlayerItem *pWeapon )
{
	if ( weaponstay.value > 0 )
	{
		// make sure it's only certain weapons
		if ( !(pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD) )
		{
			return gpGlobals->time + 0;		// weapon respawns almost instantly
		}
	}

	return gpGlobals->time + WEAPON_RESPAWN_TIME;
}

// when we are within this close to running out of entities,  items 
// marked with the ITEM_FLAG_LIMITINWORLD will delay their respawn
#define ENTITY_INTOLERANCE	100

//=========================================================
// FlWeaponRespawnTime - Returns 0 if the weapon can respawn 
// now,  otherwise it returns the time at which it can try
// to spawn again.
//=========================================================
float CHalfLifeMultiplay :: FlWeaponTryRespawn( CBasePlayerItem *pWeapon )
{
	if ( pWeapon && pWeapon->m_iId && (pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD) )
	{
		if ( NUMBER_OF_ENTITIES() < (gpGlobals->maxEntities - ENTITY_INTOLERANCE) )
			return 0;

		// we're past the entity tolerance level,  so delay the respawn
		return FlWeaponRespawnTime( pWeapon );
	}

	return 0;
}

//=========================================================
// VecWeaponRespawnSpot - where should this weapon spawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay :: VecWeaponRespawnSpot( CBasePlayerItem *pWeapon )
{
	return pWeapon->pev->origin;
}

//=========================================================
// WeaponShouldRespawn - any conditions inhibiting the
// respawning of this weapon?
//=========================================================
int CHalfLifeMultiplay :: WeaponShouldRespawn( CBasePlayerItem *pWeapon )
{
	if ( pWeapon->pev->spawnflags & SF_NORESPAWN )
	{
		return GR_WEAPON_RESPAWN_NO;
	}

	return GR_WEAPON_RESPAWN_YES;
}

//=========================================================
// CanHaveWeapon - returns FALSE if the player is not allowed
// to pick up this weapon
//=========================================================
BOOL CHalfLifeMultiplay::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pItem )
{
	if ( pPlayer->pev->deadflag != DEAD_NO )
		return FALSE;

	if ( pPlayer->IsExclusiveHoldItemActive() && pItem->m_bExclusiveHold )
		return FALSE;

	float pickupTime = pPlayer->m_flNextWeaponPickup[pItem->m_iId];

	if ( pickupTime != 0 && pickupTime > gpGlobals->time &&
		!( pItem->pev->spawnflags & ( SF_CREATEDWEAPON | SF_GIVENITEM ) ) )
		return FALSE;

	if ( CVAR_GET_FLOAT( "mp_weaponstay" ) > 0 &&
		!( pItem->pev->spawnflags & SF_CREATEDWEAPON ) &&
		!( pItem->iFlags() & ITEM_FLAG_LIMITINWORLD ) )
		{
			CBasePlayerItem *it = pPlayer->FindItem( STRING( pItem->pev->classname ) );

		if ( it && !it->CanHaveDuplicates() )
			return FALSE;
	}

	return CGameRules::CanHavePlayerItem( pPlayer, pItem );
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::CanHaveItem( CBasePlayer *pPlayer, CItem *pItem )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerGotItem( CBasePlayer *pPlayer, CItem *pItem )
{
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::ItemShouldRespawn( CItem *pItem )
{
	if ( ( pItem->pev->spawnflags & SF_NORESPAWN ) || !pItem->m_RespawnDelay.ItemCanRespawn() )
		return GR_ITEM_RESPAWN_NO;

	return GR_ITEM_RESPAWN_YES;
}


//=========================================================
// At what time in the future may this Item respawn?
//=========================================================
float CHalfLifeMultiplay::FlItemRespawnTime( CItem *pItem )
{
	return gpGlobals->time + pItem->m_RespawnDelay.GetRespawnTime( ITEM_RESPAWN_TIME, false );
}

//=========================================================
// Where should this item respawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay::VecItemRespawnSpot( CItem *pItem )
{
	return pItem->pev->origin;
}

float CGameRules::FlInventoryRespawnTime( CItemInventory *item )
{
	// The single-player rules share the inventory respawn policy
	return gpGlobals->time + item->m_RespawnDelay.GetRespawnTime( 30.0f, false );
}

float CHalfLifeMultiplay::FlInventoryRespawnTime( CItemInventory *item )
{
	return gpGlobals->time + item->m_RespawnDelay.GetRespawnTime( 30.0f, false );
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerGotAmmo( CBasePlayer *pPlayer, char *szName, int iCount )
{
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsAllowedToSpawn( CBaseEntity *pEntity )
{
//	if ( pEntity->pev->flags & FL_MONSTER )
//		return FALSE;

	return TRUE;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::AmmoShouldRespawn( CBasePlayerAmmo *pAmmo )
{
	if ( ( pAmmo->pev->spawnflags & SF_NORESPAWN ) || !pAmmo->m_RespawnDelay.ItemCanRespawn() )
	{
		return GR_AMMO_RESPAWN_NO;
	}

	return GR_AMMO_RESPAWN_YES;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlAmmoRespawnTime( CBasePlayerAmmo *pAmmo )
{
	return gpGlobals->time + pAmmo->m_RespawnDelay.GetRespawnTime( AMMO_RESPAWN_TIME, false );
}

//=========================================================
//=========================================================
Vector CHalfLifeMultiplay::VecAmmoRespawnSpot( CBasePlayerAmmo *pAmmo )
{
	return pAmmo->pev->origin;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlHealthChargerRechargeTime( void )
{
	return 30.0f;
}


float CHalfLifeMultiplay::FlHEVChargerRechargeTime( void )
{
	return 15.0f;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::DeadPlayerWeapons( CBasePlayer *pPlayer )
{
	return GR_PLR_DROP_GUN_ACTIVE;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::DeadPlayerAmmo( CBasePlayer *pPlayer )
{
	return GR_PLR_DROP_AMMO_ACTIVE;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	if ( pPlayer && pTarget
	     && ( pPlayer->Classify() == pTarget->Classify()
	          || pPlayer->IRelationship( pTarget, false ) <= R_AL ) )
		return GR_TEAMMATE;

	return GR_NOTTEAMMATE;
}

BOOL CHalfLifeMultiplay :: PlayFootstepSounds( CBasePlayer *pl, float fvol )
{
	if ( g_footsteps && g_footsteps->value == 0 )
		return FALSE;

	if ( pl->IsOnLadder() || pl->pev->velocity.Length2D() > 220 )
		return TRUE;  // only make step sounds in multiplayer if the player is moving fast enough

	return FALSE;
}

BOOL CHalfLifeMultiplay :: FAllowFlashlight( void )
{
	return CVAR_GET_FLOAT( "mp_flashlight" ) != 0.0f;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: FAllowMonsters( void )
{
	return TRUE;
}

//=========================================================
//======== CHalfLifeMultiplay private functions ===========
#define INTERMISSION_TIME		6

void CHalfLifeMultiplay :: GoToIntermission( void )
{
	if ( g_fGameOver )
		return;  // intermission has already been triggered, so ignore.

	MESSAGE_BEGIN( MSG_ALL, gmsgServerName );
		WRITE_STRING( CVAR_GET_STRING( "hostname" ) );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ALL, gmsgNextMap );
		WRITE_STRING( CVAR_GET_STRING( "mp_nextmap_cycle" ) );
	MESSAGE_END();

	UTIL_LogPrintf( "map<%s> ended, going to intermission:\n", STRING( gpGlobals->mapname ) );
	UTIL_ClientPrintAll( HUD_PRINTCENTER, "" );

	MESSAGE_BEGIN(MSG_ALL, SVC_INTERMISSION);
	MESSAGE_END();

	g_flIntermissionStartTime = gpGlobals->time;

	// bounds check
	float flChatTime = CVAR_GET_FLOAT( "mp_chattime" );

	if ( flChatTime > MAX_INTERMISSION_TIME )
		flChatTime = MAX_INTERMISSION_TIME;
	else if ( flChatTime < MIN_INTERMISSION_TIME )
		flChatTime = MIN_INTERMISSION_TIME;

	m_flIntermissionEndTime = gpGlobals->time + flChatTime;

	g_fGameOver = TRUE;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *) UTIL_PlayerByIndex( i );

		if ( pPlayer )
		{
			pPlayer->LogPlayerStats();
			pPlayer->SendScoreInfo( NULL );
			pPlayer->pev->takedamage = DAMAGE_NO;
		}
	}

	delete g_pGamePersistentInventory;
	g_pGamePersistentInventory = NULL;
}

char com_token[MAX_COM_TOKEN_LENGTH];

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char *data)
{
	int             c;
	int             len;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;                    // end of file;
		data++;
	}
	
// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse single characters
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c == ',' )
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c == ',' )
			break;
	} while (c>32);
	
	com_token[len] = 0;
	return data;
}

/*
==============
COM_TokenWaiting

Returns 1 if additional data is waiting to be processed on this line
==============
*/
int COM_TokenWaiting( char *buffer )
{
	char *p;

	p = buffer;
	while ( *p && *p!='\n')
	{
		if ( !isspace( *p ) || isalnum( *p ) )
			return 1;

		p++;
	}

	return 0;
}



/*
==============
CountPlayers

Determine the current # of active players on the server for map cycling logic
==============
*/
int CountPlayers( void )
{
	int	num = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

		if ( pEnt )
		{
			num = num + 1;
		}
	}

	return num;
}

/*
==============
ExtractCommandString

Parse commands/key value pairs to issue right after map xxx command is issued on server
 level transition
==============
*/
void ExtractCommandString( char *s, char *szCommand, unsigned int size )
{
	// Now make rules happen
	char	pkey[512];
	char	value[512];	// use two buffers so compares
								// work without stomping on each other
	char	*o;
	
	if ( *s == '\\' )
		s++;

	while (1)
	{
		o = pkey;
		while ( *s != '\\' )
		{
			if ( !*s )
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;

		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		UTIL_AppendString( szCommand, pkey, size );
		if ( strlen( value ) > 0 )
		{
			UTIL_AppendString( szCommand, " ", size );
			UTIL_AppendString( szCommand, value, size );
		}
		UTIL_AppendString( szCommand, "\n", size );

		if (!*s)
			return;
		s++;
	}
}

void CHalfLifeMultiplay::GetNextMap( void )
{
	mapcycle_t &cycle = g_mapcycle;
	const char *overrideMap = CVAR_GET_STRING( "mp_nextmap" );
	unsigned int partedLimit = (unsigned int) (long long) CVAR_GET_FLOAT( "mp_nextmap_parted_mapcfg_limit" );

	if ( overrideMap[0] && IS_MAP_VALID( (char *) overrideMap ) )
	{
		if ( !( CVAR_GET_FLOAT( "mp_nextmap_ignore_mapcfg" ) >= 1.0f ) )
		{
			if ( cycle.parted < partedLimit )
			{
				CVAR_SET_STRING( "mp_nextmap_cycle", overrideMap );
				CVAR_SET_FLOAT( "mp_nextmap_cycle_minp", 0 );
				CVAR_SET_FLOAT( "mp_nextmap_cycle_maxp", 0 );
				CVAR_SET_STRING( "mp_nextmap_cycle_cmds", "" );
				CVAR_SET_STRING( "mp_nextmap_cycle_rules", "" );
				++cycle.parted;
				ALERT( at_console, "Map configuration overrides map cycle, parted from the cycle %d time(s). Next map will be: %s\n",
				       cycle.parted, overrideMap );
				return;
			}

			ALERT( at_console, "Map configuration overrides map cycle. Ignored (cycle parted limit %d exceeded), next map would have been: %s\n",
			       partedLimit, overrideMap );
		}
		else
			ALERT( at_console, "Map configuration overrides map cycle. Ignored (denied by server config), next map would have been: %s\n", overrideMap );
	}

	if ( cycle.parted && cycle.position )
	{
		cycle.parted = 0;
		++cycle.position;
	}

	const char *currentMap = STRING( gpGlobals->mapname );
	char nextMap[cchMapNameMost + 1] = {};
	char firstMap[cchMapNameMost + 1] = {};
	char commands[1024] = {};
	char rules[1024] = {};
	int minPlayers = 0;
	int maxPlayers = 0;
	int players = CServer::GetInstance()->GetPlayerCount();
	char *mapCycleFile = (char *) CVAR_GET_STRING( "mapcyclefile" );
	bool shuffle = CVAR_GET_FLOAT( "mapcycleshuffle" ) >= 1.0f;
	char fullPath[MAX_PATH + 1] = {};
	bool reload = true;

	UTIL_CopyString( firstMap, "svencoop1", sizeof( firstMap ) );
	GET_GAME_DIR( fullPath );
	UTIL_AppendString( fullPath, "/", sizeof( fullPath ) );
	UTIL_AppendString( fullPath, mapCycleFile, sizeof( fullPath ) );

	if ( cycle.filename[0] && !stricmp( mapCycleFile, cycle.filename ) )
	{
		struct stat fileInfo;

		if ( cycle.modified && stat( fullPath, &fileInfo ) == 0 && fileInfo.st_mtime == cycle.modified )
		{
			if ( cycle.items && cycle.count )
			{
				if ( ( !cycle.shuffled && !shuffle ) || cycle.position <= (unsigned int) cycle.count )
					reload = false;
				else
					ALERT( at_console, "Map cycle file \"%s\" will be re-loaded so it can be re-shuffled.\n", mapCycleFile );
			}
			else
				ALERT( at_console, "Map cycle file \"%s\" will be re-loaded due to the previous load being empty.\n", mapCycleFile );
		}
		else
			ALERT( at_console, "Map cycle file \"%s\" will be re-loaded due to content modification.\n", mapCycleFile );
	}
	else
		ALERT( at_console, "Map cycle file \"%s\" will be re-loaded due to file pointer modification.\n", mapCycleFile );

	bool loaded = true;

	if ( reload )
	{
		DestroyMapCycle( &cycle );
		loaded = ReloadMapCycleFile( mapCycleFile, &cycle, shuffle );

		if ( !loaded )
			ALERT( at_warning, "Map cycle file \"%s\" failed to load.\n", mapCycleFile );
		else if ( !cycle.items || !cycle.count )
			ALERT( at_warning, "Map cycle file \"%s\" has no valid maps.\n", mapCycleFile );
		else
			ALERT( at_console, "Map cycle file \"%s\" loaded successfully with %d map(s).\n", mapCycleFile, cycle.count );
	}

	if ( loaded && cycle.items && cycle.count )
	{
		UTIL_CopyString( nextMap, currentMap, sizeof( nextMap ) );
		UTIL_CopyString( firstMap, currentMap, sizeof( firstMap ) );

		mapcycle_item_t *start = cycle.next_item;
		mapcycle_item_t *item = start;
		unsigned int checked = 0;
		bool selected = false;
		bool found = false;

		for ( ; item->next != start; item = item->next )
		{
			bool skip = false;

			++checked;
			found = false;

			if ( item->minplayers )
			{
				if ( players >= item->minplayers )
				{
					minPlayers = item->minplayers;
					found = true;
				}
				else
				{
					ALERT( at_console, "Map cycle: Too few players for \"%s\" (%d needed), skipping.\n", item->mapname, item->minplayers );
					skip = true;
				}
			}

			if ( item->maxplayers )
			{
				if ( players <= item->maxplayers )
				{
					maxPlayers = item->maxplayers;
					found = true;
				}
				else
				{
					ALERT( at_console, "Map cycle: Too many players for \"%s\" (limit is %d), skipping.\n", item->mapname, item->maxplayers );
					skip = true;
				}
			}

			if ( IS_MAP_VALID( item->mapname ) )
			{
				found = true;

				if ( checked <= 1 )
					UTIL_CopyString( firstMap, item->mapname, sizeof( firstMap ) );
			}
			else
			{
				ALERT( at_console, "Map cycle: Invalid/missing map \"%s\", skipping.\n", item->mapname );
				skip = true;
			}

			if ( checked >= cycle.position && !skip )
			{
				cycle.position = checked;
				selected = true;
				break;
			}
		}

		if ( selected && found )
		{
			if ( !stricmp( currentMap, item->mapname ) )
			{
				++cycle.position;
				item = item->next;

				if ( cycle.position > (unsigned int) cycle.count )
					cycle.position = 1;
			}
			else
			{
				// The rollback check compares the selected name with itself
				if ( stricmp( item->mapname, item->mapname ) )
					ALERT( at_console, "Map cycle: Unexpectedly running map \"%s\" instead of \"%s\", rolling back cycle position to previous map \"%s\".\n",
					       currentMap, item->mapname, item->mapname );
				else
					ALERT( at_console, "Map cycle: Unexpectedly running map \"%s\" instead of \"%s\", holding cycle position.\n",
					       currentMap, item->mapname );
			}
		}
		else
		{
			item = start;
			cycle.position = 1;
		}

		char rawCommands[1500] = {};

		UTIL_CopyString( nextMap, item->mapname, sizeof( nextMap ) );
		ExtractCommandString( item->rulebuffer, rawCommands, sizeof( rawCommands ) );
		UTIL_CopyString( commands, rawCommands, sizeof( commands ) );
		UTIL_CopyString( rules, item->rulebuffer, sizeof( rules ) );
	}

	if ( !IS_MAP_VALID( nextMap ) )
	{
		ALERT( at_console, "Map cycle: Invalid/missing map \"%s\", reverting to \"%s\".\n", nextMap, firstMap );
		UTIL_CopyString( nextMap, firstMap, sizeof( nextMap ) );
	}

	CVAR_SET_STRING( "mp_nextmap_cycle", nextMap );
	CVAR_SET_FLOAT( "mp_nextmap_cycle_minp", minPlayers );
	CVAR_SET_FLOAT( "mp_nextmap_cycle_maxp", maxPlayers );
	CVAR_SET_STRING( "mp_nextmap_cycle_cmds", commands );
	CVAR_SET_STRING( "mp_nextmap_cycle_rules", rules );

	ALERT( at_console, "Next map in cycle is: %s [%d/%d]\n", CVAR_GET_STRING( "mp_nextmap_cycle" ), cycle.position, cycle.count );
}

/*
==============
ChangeLevel

Server is changing to a new level, check mapcycle.txt for map name and setup info
==============
*/
void CHalfLifeMultiplay :: ChangeLevel( void )
{
	static char szPreviousMapCycleFile[ 256 ];
	mapcycle_t &mapcycle = g_mapcycle;

	char szNextMap[cchMapNameMost + 1];
	char szFirstMapInList[cchMapNameMost + 1];
	char szCommands[ 1500 ];
	char szRules[ 1500 ];
	int minplayers = 0, maxplayers = 0;
	strcpy( szFirstMapInList, "hldm1" );  // the absolute default level is hldm1

	int	curplayers;
	BOOL do_cycle = TRUE;

	// find the map to change to
	char *mapcfile = (char*)CVAR_GET_STRING( "mapcyclefile" );
	ASSERT( mapcfile != NULL );

	szCommands[ 0 ] = '\0';
	szRules[ 0 ] = '\0';

	curplayers = CountPlayers();

	// Has the map cycle filename changed?
	if ( stricmp( mapcfile, szPreviousMapCycleFile ) )
	{
		strcpy( szPreviousMapCycleFile, mapcfile );

		DestroyMapCycle( &mapcycle );

		if ( !ReloadMapCycleFile( mapcfile, &mapcycle, CVAR_GET_FLOAT( "mapcycleshuffle" ) >= 1.0f )
		     || !mapcycle.items )
		{
			ALERT( at_console, "Unable to load map cycle file %s\n", mapcfile );
			do_cycle = FALSE;
		}
	}

	if ( do_cycle && mapcycle.items )
	{
		BOOL keeplooking = FALSE;
		BOOL found = FALSE;
		mapcycle_item_s *item;

		// Assume current map
		strcpy( szNextMap, STRING(gpGlobals->mapname) );
		strcpy( szFirstMapInList, STRING(gpGlobals->mapname) );

		// Traverse list
		for ( item = mapcycle.next_item; item->next != mapcycle.next_item; item = item->next )
		{
			keeplooking = FALSE;

			ASSERT( item != NULL );

			if ( item->minplayers != 0 )
			{
				if ( curplayers >= item->minplayers )
				{
					found = TRUE;
					minplayers = item->minplayers;
				}
				else
				{
					keeplooking = TRUE;
				}
			}

			if ( item->maxplayers != 0 )
			{
				if ( curplayers <= item->maxplayers )
				{
					found = TRUE;
					maxplayers = item->maxplayers;
				}
				else
				{
					keeplooking = TRUE;
				}
			}

			if ( keeplooking )
				continue;

			found = TRUE;
			break;
		}

		if ( !found )
		{
			item = mapcycle.next_item;
		}			
		
		// Increment next item pointer
		mapcycle.next_item = item->next;

		// Perform logic on current item
		strcpy( szNextMap, item->mapname );

		ExtractCommandString( item->rulebuffer, szCommands, 1500 );
		strcpy( szRules, item->rulebuffer );
	}

	if ( !IS_MAP_VALID(szNextMap) )
	{
		strcpy( szNextMap, szFirstMapInList );
	}

	g_fGameOver = TRUE;

	ALERT( at_console, "CHANGE LEVEL: %s\n", szNextMap );
	if ( minplayers || maxplayers )
	{
		ALERT( at_console, "PLAYER COUNT:  min %i max %i current %i\n", minplayers, maxplayers, curplayers );
	}
	if ( strlen( szRules ) > 0 )
	{
		ALERT( at_console, "RULES:  %s\n", szRules );
	}
	
	CHANGE_LEVEL( szNextMap, NULL );
	if ( strlen( szCommands ) > 0 )
	{
		SERVER_COMMAND( szCommands );
	}
}

#define MAX_MOTD_CHUNK	  60
#define MAX_MOTD_LENGTH   1536 // (MAX_MOTD_CHUNK * 4)

void CHalfLifeMultiplay :: SendMOTDToClient( edict_t *client )
{
	// read from the MOTD.txt file
	int length, char_count = 0;
	char *pFileList;
	char *aFileList = pFileList = (char*)LOAD_FILE_FOR_ME( (char *)CVAR_GET_STRING( "motdfile" ), &length );

	// send the server name
	MESSAGE_BEGIN( MSG_ONE, gmsgServerName, NULL, client );
		WRITE_STRING( CVAR_GET_STRING("hostname") );
	MESSAGE_END();

	// Send the message of the day
	// read it chunk-by-chunk,  and send it in parts

	while ( pFileList && *pFileList && char_count < MAX_MOTD_LENGTH )
	{
		char chunk[MAX_MOTD_CHUNK+1];
		
		if ( strlen( pFileList ) < MAX_MOTD_CHUNK )
		{
			strcpy( chunk, pFileList );
		}
		else
		{
			strncpy( chunk, pFileList, MAX_MOTD_CHUNK );
			chunk[MAX_MOTD_CHUNK] = 0;		// strncpy doesn't always append the null terminator
		}

		char_count += strlen( chunk );
		if ( char_count < MAX_MOTD_LENGTH )
			pFileList = aFileList + char_count; 
		else
			*pFileList = 0;

		MESSAGE_BEGIN( MSG_ONE, gmsgMOTD, NULL, client );
			WRITE_BYTE( *pFileList ? FALSE : TRUE );	// FALSE means there is still more message to come
			WRITE_STRING( chunk );
		MESSAGE_END();
	}

	FREE_FILE( aFileList );
}
	

