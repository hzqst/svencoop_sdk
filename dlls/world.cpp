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
/*

===== world.cpp ========================================================

  precaches and defs for entities and other data that must always be available.

*/

#include "extdll.h"
#include "server_sound.h"
#include "server_support.h"
#include "server_entvars.h"
#include "CMap.h"
#include "CServer.h"
#include "CASServerManager.h"
#include "CASScriptEntityPreprocessor.h"
#include "CMapPreprocessor.h"
#include "CObjectLifetimeHandler.h"
#include "BSPFile.h"
#include "CString.h"
#include "CStringMap.h"
#include "CReplacementMap.h"
#include "WorldModels.h"
#include "../engine/bspfile.h"
#include "server_filesystem.h"
#include "util.h"
#include "cbase.h"
#include "nodes.h"
#include "soundent.h"
#include "client.h"
#include "decals.h"
#include "skill.h"
#include "effects.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "SvenVersion.h"
#include "teamplay_gamerules.h"
#include "monsters.h"
#include "talkmonster.h"

#define MAX_MAP_GAME_VERSION	SVEN_GAME_VERSION_NUMBER

extern CGraph WorldGraph;
extern CSoundEnt *pSoundEnt;

extern CBaseEntity				*g_pLastSpawn;
DLL_GLOBAL edict_t				*g_pBodyQueueHead;
CGlobalState					gGlobalState;
extern DLL_GLOBAL	int			gDisplayTitle;

string_t g_szModelReplacement;
string_t g_szSoundReplacement;
bool g_iActiveIdle;
int g_iMapForGameVersion;

StandardWeaponModels g_pStandardWeaponModels[256];
const char *gszNoCustomMdl = "none";

CWorld *CWorld::m_pInstance;

//
// This must match the list in util.h
//
DLL_DECALLIST gDecals[] = {
	{ "{shot1",	0 },		// DECAL_GUNSHOT1 
	{ "{shot2",	0 },		// DECAL_GUNSHOT2
	{ "{shot3",0 },			// DECAL_GUNSHOT3
	{ "{shot4",	0 },		// DECAL_GUNSHOT4
	{ "{shot5",	0 },		// DECAL_GUNSHOT5
	{ "{lambda01", 0 },		// DECAL_LAMBDA1
	{ "{lambda02", 0 },		// DECAL_LAMBDA2
	{ "{lambda03", 0 },		// DECAL_LAMBDA3
	{ "{lambda04", 0 },		// DECAL_LAMBDA4
	{ "{lambda05", 0 },		// DECAL_LAMBDA5
	{ "{lambda06", 0 },		// DECAL_LAMBDA6
	{ "{scorch1", 0 },		// DECAL_SCORCH1
	{ "{scorch2", 0 },		// DECAL_SCORCH2
	{ "{blood1", 0 },		// DECAL_BLOOD1
	{ "{blood2", 0 },		// DECAL_BLOOD2
	{ "{blood3", 0 },		// DECAL_BLOOD3
	{ "{blood4", 0 },		// DECAL_BLOOD4
	{ "{blood5", 0 },		// DECAL_BLOOD5
	{ "{blood6", 0 },		// DECAL_BLOOD6
	{ "{yblood1", 0 },		// DECAL_YBLOOD1
	{ "{yblood2", 0 },		// DECAL_YBLOOD2
	{ "{yblood3", 0 },		// DECAL_YBLOOD3
	{ "{yblood4", 0 },		// DECAL_YBLOOD4
	{ "{yblood5", 0 },		// DECAL_YBLOOD5
	{ "{yblood6", 0 },		// DECAL_YBLOOD6
	{ "{break1", 0 },		// DECAL_GLASSBREAK1
	{ "{break2", 0 },		// DECAL_GLASSBREAK2
	{ "{break3", 0 },		// DECAL_GLASSBREAK3
	{ "{bigshot1", 0 },		// DECAL_BIGSHOT1
	{ "{bigshot2", 0 },		// DECAL_BIGSHOT2
	{ "{bigshot3", 0 },		// DECAL_BIGSHOT3
	{ "{bigshot4", 0 },		// DECAL_BIGSHOT4
	{ "{bigshot5", 0 },		// DECAL_BIGSHOT5
	{ "{spit1", 0 },		// DECAL_SPIT1
	{ "{spit2", 0 },		// DECAL_SPIT2
	{ "{bproof1", 0 },		// DECAL_BPROOF1
	{ "{gargstomp", 0 },	// DECAL_GARGSTOMP1,	// Gargantua stomp crack
	{ "{smscorch1", 0 },	// DECAL_SMALLSCORCH1,	// Small scorch mark
	{ "{smscorch2", 0 },	// DECAL_SMALLSCORCH2,	// Small scorch mark
	{ "{smscorch3", 0 },	// DECAL_SMALLSCORCH3,	// Small scorch mark
	{ "{mommablob", 0 },	// DECAL_MOMMABIRTH		// BM Birth spray
	{ "{mommablob", 0 },	// DECAL_MOMMASPLAT		// BM Mortar spray?? need decal
	{ "{ofscorch1", 0 },	// DECAL_OFSCORCH1
	{ "{ofscorch2", 0 },	// DECAL_OFSCORCH2
	{ "{ofscorch3", 0 },	// DECAL_OFSCORCH3
	{ "{spr_splt1", 0 },	// DECAL_SPORESPLAT1
	{ "{spr_splt2", 0 },	// DECAL_SPORESPLAT2
	{ "{spr_splt3", 0 },	// DECAL_SPORESPLAT3
};

/*
==============================================================================

BODY QUE

==============================================================================
*/

#define SF_DECAL_NOTINDEATHMATCH		2048

class CDecal : public CBaseEntity
{
public:
	void	Spawn( void );
	void	KeyValue( KeyValueData *pkvd );
	void	EXPORT StaticDecal( void );
	void	EXPORT TriggerDecal( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( infodecal, CDecal );

// UNDONE:  These won't get sent to joining players in multi-player
void CDecal :: Spawn( void )
{
	if ( pev->skin < 0 || (gpGlobals->deathmatch && FBitSet( pev->spawnflags, SF_DECAL_NOTINDEATHMATCH )) )
	{
		REMOVE_ENTITY(ENT(pev));
		return;
	}

	if ( FStringNull ( pev->targetname ) )
	{
		SetThink( &CDecal::StaticDecal );
		// if there's no targetname, the decal will spray itself on as soon as the world is done spawning.
		pev->nextthink = gpGlobals->time;
	}
	else
	{
		// if there IS a targetname, the decal sprays itself on when it is triggered.
		SetThink ( &CDecal::SUB_DoNothing );
		SetUse( &CDecal::TriggerDecal);
	}
}

void CDecal :: TriggerDecal ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// this is set up as a USE function for infodecals that have targetnames, so that the
	// decal doesn't get applied until it is fired. (usually by a scripted sequence)
	TraceResult trace;
	int			entityIndex;

	UTIL_TraceLine( pev->origin - Vector(5,5,5), pev->origin + Vector(5,5,5),  ignore_monsters, ENT(pev), &trace );

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE( TE_BSPDECAL );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( (int)pev->skin );
		entityIndex = (short)ENTINDEX(trace.pHit);
		WRITE_SHORT( entityIndex );
		if ( entityIndex )
			WRITE_SHORT( (int)VARS(trace.pHit)->modelindex );
	MESSAGE_END();

	SetThink( &CDecal::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1;
}


void CDecal :: StaticDecal( void )
{
	TraceResult trace;
	int			entityIndex, modelIndex;

	UTIL_TraceLine( pev->origin - Vector(5,5,5), pev->origin + Vector(5,5,5),  ignore_monsters, ENT(pev), &trace );

	entityIndex = (short)ENTINDEX(trace.pHit);
	if ( entityIndex )
		modelIndex = (int)VARS(trace.pHit)->modelindex;
	else
		modelIndex = 0;

	g_engfuncs.pfnStaticDecal( pev->origin, (int)pev->skin, entityIndex, modelIndex );

	SUB_Remove();
}


void CDecal :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "texture"))
	{
		pev->skin = DECAL_INDEX( pkvd->szValue );
		
		// Found
		if ( pev->skin >= 0 )
			return;
		ALERT( at_console, "Can't find decal %s\n", pkvd->szValue );
	}
	else
		CBaseEntity::KeyValue( pkvd );
}


// Body queue class here.... It's really just CBaseEntity
class CCorpse : public CBaseEntity
{
	virtual int ObjectCaps( void ) { return FCAP_DONT_SAVE; }	
};

LINK_ENTITY_TO_CLASS( bodyque, CCorpse );

static void InitBodyQue(void)
{
	string_t	istrClassname = MAKE_STRING("bodyque");

	g_pBodyQueueHead = CREATE_NAMED_ENTITY( istrClassname );
	entvars_t *pev = VARS(g_pBodyQueueHead);
	
	// Reserve 3 more slots for dead bodies
	for ( int i = 0; i < 3; i++ )
	{
		pev->owner = CREATE_NAMED_ENTITY( istrClassname );
		pev = VARS(pev->owner);
	}
	
	pev->owner = g_pBodyQueueHead;
}


//
// make a body que entry for the given ent so the ent can be respawned elsewhere
//
// GLOBALS ASSUMED SET:  g_eoBodyQueueHead
//
void CopyToBodyQue(entvars_t *pev) 
{
	if (pev->effects & EF_NODRAW)
		return;

	entvars_t *pevHead	= VARS(g_pBodyQueueHead);

	pevHead->angles		= pev->angles;
	pevHead->model		= pev->model;
	pevHead->modelindex	= pev->modelindex;
	pevHead->frame		= pev->frame;
	pevHead->colormap	= pev->colormap;
	pevHead->movetype	= MOVETYPE_TOSS;
	pevHead->velocity	= pev->velocity;
	pevHead->flags		= 0;
	pevHead->deadflag	= pev->deadflag;
	pevHead->renderfx	= kRenderFxDeadPlayer;
	pevHead->renderamt	= ENTINDEX( ENT( pev ) );

	pevHead->effects    = pev->effects | EF_NOINTERP;
	//pevHead->goalstarttime = pev->goalstarttime;
	//pevHead->goalframe	= pev->goalframe;
	//pevHead->goalendtime = pev->goalendtime ;
	
	pevHead->sequence = pev->sequence;
	pevHead->animtime = pev->animtime;

	UTIL_SetOrigin(pevHead, pev->origin);
	UTIL_SetSize(pevHead, pev->mins, pev->maxs);
	g_pBodyQueueHead = pevHead->owner;
}


CGlobalState::CGlobalState( void )
{
	Reset();
}

void CGlobalState::Reset( void )
{
	m_pList = NULL; 
	m_listCount = 0;
}

globalentity_t *CGlobalState :: Find( string_t globalname )
{
	if ( !globalname )
		return NULL;

	globalentity_t *pTest;
	const char *pEntityName = STRING(globalname);

	
	pTest = m_pList;
	while ( pTest )
	{
		if ( FStrEq( pEntityName, pTest->name ) )
			break;
	
		pTest = pTest->pNext;
	}

	return pTest;
}


// This is available all the time now on impulse 104, remove later
//#ifdef _DEBUG
void CGlobalState :: DumpGlobals( void )
{
	static const char *estates[] = { "Off", "On", "Dead" };
	globalentity_t *pTest;

	ALERT( at_console, "-- Globals --\n" );
	pTest = m_pList;
	while ( pTest )
	{
		ALERT( at_console, "%s: %s (%s)\n", pTest->name, pTest->levelName, estates[pTest->state] );
		pTest = pTest->pNext;
	}
}
//#endif


void CGlobalState :: EntityAdd( string_t globalname, string_t mapName, GLOBALESTATE state )
{
	globalentity_t *pNewEntity = (globalentity_t *)calloc( sizeof( globalentity_t ), 1 );

	if ( !pNewEntity )
		return;
	pNewEntity->pNext = m_pList;
	m_pList = pNewEntity;
	UTIL_CopyString( pNewEntity->name, STRING( globalname ), sizeof(pNewEntity->name));

	const char *pMapName = STRING(mapName);

	if ( strlen(pMapName) <= sizeof(pNewEntity->levelName) )
		UTIL_CopyString( pNewEntity->levelName, pMapName, sizeof(pNewEntity->levelName) );
	pNewEntity->state = state;
	m_listCount++;
}


void CGlobalState :: EntitySetState( string_t globalname, GLOBALESTATE state )
{
	globalentity_t *pEnt = Find( globalname );

	if ( pEnt )
		pEnt->state = state;
}


const globalentity_t *CGlobalState :: EntityFromTable( string_t globalname )
{
	globalentity_t *pEnt = Find( globalname );

	return pEnt;
}


GLOBALESTATE CGlobalState :: EntityGetState( string_t globalname )
{
	globalentity_t *pEnt = Find( globalname );
	if ( pEnt )
		return pEnt->state;

	return GLOBAL_OFF;
}


// Global Savedata for Delay
TYPEDESCRIPTION	CGlobalState::m_SaveData[] = 
{
	DEFINE_FIELD( CGlobalState, m_listCount, FIELD_INTEGER ),
};

// Global Savedata for Delay
TYPEDESCRIPTION	gGlobalEntitySaveData[] = 
{
	DEFINE_ARRAY( globalentity_t, name, FIELD_CHARACTER, 64 ),
	DEFINE_ARRAY( globalentity_t, levelName, FIELD_CHARACTER, 32 ),
	DEFINE_FIELD( globalentity_t, state, FIELD_INTEGER ),
};


int CGlobalState::Save( CSave &save )
{
	int i;
	globalentity_t *pEntity;

	if ( !save.WriteFields( "GLOBAL", this, m_SaveData, ARRAYSIZE(m_SaveData) ) )
		return 0;
	
	pEntity = m_pList;
	for ( i = 0; i < m_listCount && pEntity; i++ )
	{
		if ( !save.WriteFields( "GENT", pEntity, gGlobalEntitySaveData, ARRAYSIZE(gGlobalEntitySaveData) ) )
			return 0;

		pEntity = pEntity->pNext;
	}
	
	return 1;
}

int CGlobalState::Restore( CRestore &restore )
{
	int i, listCount;
	globalentity_t tmpEntity;


	ClearStates();
	if ( !restore.ReadFields( "GLOBAL", this, m_SaveData, ARRAYSIZE(m_SaveData) ) )
		return 0;
	
	listCount = m_listCount;	// Get new list count
	m_listCount = 0;				// Clear loaded data

	for ( i = 0; i < listCount; i++ )
	{
		if ( !restore.ReadFields( "GENT", &tmpEntity, gGlobalEntitySaveData, ARRAYSIZE(gGlobalEntitySaveData) ) )
			return 0;
		EntityAdd( MAKE_STRING(tmpEntity.name), MAKE_STRING(tmpEntity.levelName), tmpEntity.state );
	}
	return 1;
}

void CGlobalState::EntityUpdate( string_t globalname, string_t mapname )
{
	globalentity_t *pEnt = Find( globalname );

	if ( pEnt )
		UTIL_CopyString( pEnt->levelName, STRING(mapname), sizeof(pEnt->levelName));
}


void CGlobalState::ClearStates( void )
{
	globalentity_t *pFree = m_pList;
	while ( pFree )
	{
		globalentity_t *pNext = pFree->pNext;
		free( pFree );
		pFree = pNext;
	}
	Reset();
}


void SaveGlobalState( SAVERESTOREDATA *pSaveData )
{
	CSave saveHelper( pSaveData );
	gGlobalState.Save( saveHelper );
}


void RestoreGlobalState( SAVERESTOREDATA *pSaveData )
{
	CRestore restoreHelper( pSaveData );
	gGlobalState.Restore( restoreHelper );
}


void ResetGlobalState( void )
{
	if ( CVAR_GET_FLOAT("mp_keep_globalstate") < 1.0f )
		gGlobalState.ClearStates();
	else
		CVAR_SET_FLOAT("mp_keep_globalstate", 0);
	gInitHUD = TRUE;	// Init the HUD on a new game / load game
}

// moved CWorld class definition to cbase.h
//=======================
// CWorld
//
// This spawns first when each level begins.
//=======================

LINK_ENTITY_TO_CLASS( worldspawn, CWorld );

#define SF_WORLD_DARK		0x0001		// Fade from black at startup
#define SF_WORLD_TITLE		0x0002		// Display game title at startup
#define SF_WORLD_FORCETEAM	0x0004		// Force teams

extern DLL_GLOBAL BOOL		g_fGameOver;
float g_flWeaponCheat;

CWorld *CWorld::GetInstance( void )
{
	return m_pInstance;
}

void CWorld::OnCreate( void )
{
	CMap::CreateInstance();
	CASServerManager::GetInstance()->WorldCreated();
	m_pInstance = this;
	CBaseEntity::OnCreate();

	CVAR_SET_STRING( "sv_skyname", "desert" );
	CVAR_SET_FLOAT( "sv_skyvec_x", 0 );
	CVAR_SET_FLOAT( "sv_skyvec_y", 0 );
	CVAR_SET_FLOAT( "sv_skyvec_z", 0 );
	CVAR_SET_FLOAT( "sv_skycolor_r", 200 );
	CVAR_SET_FLOAT( "sv_skycolor_g", 194 );
	CVAR_SET_FLOAT( "sv_skycolor_b", 188 );

	double maxDistance = CVAR_GET_FLOAT( "sv_zmax" );

	maxDistance = V_min( maxDistance, 454046.777344 );
	CVAR_SET_FLOAT( "sv_zmax", V_max( 4096.0, maxDistance ) );

	UTIL_CopyString( m_szSentenceFile, "sound/default_sentences.txt", sizeof( m_szSentenceFile ) );

	for ( int i = 0; i < ARRAYSIZE( g_pStandardWeaponModels ); i++ )
	{
		StandardWeaponModels &models = g_pStandardWeaponModels[i];

		UTIL_CopyString( models.view, gszNoCustomMdl, sizeof( models.view ) );
		UTIL_CopyString( models.world, gszNoCustomMdl, sizeof( models.world ) );
		UTIL_CopyString( models.player, gszNoCustomMdl, sizeof( models.player ) );
	}

	CServer_SoundEngine::GetSoundEngine()->Initialize();

	g_iActiveIdle = false;
	g_szSoundReplacement = 0;
	g_szModelReplacement = 0;
	g_iMapForGameVersion = 0;
}

void CWorld::OnDestroy( void )
{
	g_MapObjectLifetimeHandler.Clear();
	CBaseEntity::OnDestroy();
	m_pInstance = NULL;
}

const char *CWorld::MODELREPLACEMENT_Find( const char *model )
{
	const char *replacement = m_ModelReplacementMap.Find( model );

	return replacement ? replacement : model;
}

void CWorld::PrecacheGlobalCustomModels( string_t fileName )
{
	ReplacementSettings settings = { "Model", "models", NULL };

	LoadReplacementMap( STRING( fileName ), true, settings, m_ModelReplacementMap );
}

void CWorld::PrecacheStandardWeaponModels( void )
{
	for ( int i = 0; i < ARRAYSIZE( g_pStandardWeaponModels ); i++ )
	{
		StandardWeaponModels &models = g_pStandardWeaponModels[i];

		if ( strcmp( models.view, gszNoCustomMdl ) )
			UTIL_PrecacheModel( this, models.view );

		if ( strcmp( models.world, gszNoCustomMdl ) )
			UTIL_PrecacheModel( this, models.world );

		if ( strcmp( models.player, gszNoCustomMdl ) )
			UTIL_PrecacheModel( this, models.player );
	}
}

const char *GetWeaponStandardModel( WEAPONMDLIDX modelType, int weaponId, const char *defaultModel )
{
	if ( (unsigned int) weaponId >= ARRAYSIZE( g_pStandardWeaponModels ) )
		return defaultModel;

	const StandardWeaponModels &models = g_pStandardWeaponModels[weaponId];
	const char *model;

	switch ( modelType )
	{
		case 0:
			model = models.player;
			break;
		case 1:
			model = models.world;
			break;
		case 2:
			model = models.view;
			break;
		default:
			return defaultModel;
	}

	return stricmp( model, gszNoCustomMdl ) ? model : defaultModel;
}

void CWorld::WeaponModelKey( KeyValueData *pkvd )
{
	static const struct
	{
		const char *name;
		int         id;
	} weapons[] = {
		{ "WEAPON_CROWBAR", 1 },
		{ "WEAPON_GLOCK", 2 },
		{ "WEAPON_PYTHON", 3 },
		{ "WEAPON_DESERT_EAGLE", 27 },
		{ "WEAPON_MP5", 4 },
		{ "WEAPON_CHAINGUN", 5 },
		{ "WEAPON_CROSSBOW", 6 },
		{ "WEAPON_SHOTGUN", 7 },
		{ "WEAPON_RPG", 8 },
		{ "WEAPON_GAUSS", 9 },
		{ "WEAPON_EGON", 10 },
		{ "WEAPON_HORNETGUN", 11 },
		{ "WEAPON_HANDGRENADE", 12 },
		{ "WEAPON_TRIPMINE", 13 },
		{ "WEAPON_SATCHEL", 14 },
		{ "WEAPON_SNARK", 15 },
		{ "WEAPON_UZI", 17 },
		{ "WEAPON_MEDKIT", 18 },
		{ "WEAPON_CROWBAR_ELECTRIC", 19 },
		{ "WEAPON_PIPEWRENCH", 20 },
		{ "WEAPON_SHOCKRIFLE", 28 },
		{ "WEAPON_DISPLACER", 29 }
	};
	static const char suffixes[] = { 'v', 'w', 'p' };

	for ( int i = 0; i < ARRAYSIZE( weapons ); i++ )
	{
		for ( int modelType = 0; modelType < ARRAYSIZE( suffixes ); modelType++ )
		{
			char key[64];

			UTIL_FormatString( key, sizeof( key ), "%s_%c", weapons[i].name, suffixes[modelType] );

			if ( strcmp( pkvd->szKeyName, key ) )
				continue;

			StandardWeaponModels &models = g_pStandardWeaponModels[weapons[i].id];
			char *model = modelType == 0 ? models.view : modelType == 1 ? models.world
			                                                            : models.player;

			UTIL_CopyString( model, key, sizeof( models.view ) );
			pkvd->fHandled = TRUE;
			return;
		}
	}
}

void CWorld :: Spawn( void )
{
	SetupEntVars();

	g_fGameOver = FALSE;
	Precache( );

	if ( g_iMapForGameVersion > MAX_MAP_GAME_VERSION )
	{
		char szError[256];

		sprintf( szError, "This map requires game version %d. You can't run it on version %d.\n", g_iMapForGameVersion, MAX_MAP_GAME_VERSION );
		UTIL_HostError( szError, -1 );
		return;
	}

	g_flWeaponCheat = CVAR_GET_FLOAT( "sv_cheats" );  // Is the impulse 101 command allowed?
}

static const char *g_szMapGenericFiles[] =
{
	"maps/%s_skl.cfg",
	"maps/%s.cfg",
	"maps/%s.res",
	"maps/%s.txt",
	"maps/%s_motd.txt",
	"maps/%s.gmr",
	"maps/%s.gsr",
};

void CWorld :: Precache( void )
{
	UTIL_ResetPrecachedModelCount();

	g_pLastSpawn = NULL;

	g_engfuncs.pfnForceUnmodified( force_exactfile, NULL, NULL, "cl_dlls/client.dll" );
	g_engfuncs.pfnForceUnmodified( force_exactfile, NULL, NULL, "cl_dlls/client.so" );
	g_engfuncs.pfnForceUnmodified( force_exactfile, NULL, NULL, "cl_dlls/client.dylib" );

	for ( int iFile = 0; iFile < ARRAYSIZE( g_szMapGenericFiles ); ++iFile )
	{
		char szFile[256];

		_snprintf( szFile, sizeof( szFile ), g_szMapGenericFiles[iFile], STRING( gpGlobals->mapname ) );

		// Only files the server really has may be offered to the clients
		if ( ( (IFileSystem_SvEngine *) g_pFileSystem )->FileExistsEx( szFile, "GAME|DEFAULTGAME" ) )
			g_engfuncs.pfnPrecacheGeneric( (char *) STRING( ALLOC_STRING( szFile ) ) );
	}

	PRECACHE_MODEL( "models/error.mdl" );
	PRECACHE_MODEL( "sprites/error.spr" );

	CVAR_SET_FLOAT( "skill", 2 );
	CVAR_SET_STRING( "mp_nomedkit", "0" );
	CVAR_SET_STRING( "mp_disable_pcbalancing", "0" );
	CVAR_SET_STRING( "mp_disable_medkit_points", "0" );
	CVAR_SET_STRING( "mp_pcbalancing_factorlist", "" );
	CVAR_SET_STRING( "weaponmode_9mmhandgun", "0" );
	CVAR_SET_STRING( "weaponmode_eagle", "0" );
	CVAR_SET_STRING( "weaponmode_shotgun", "0" );
	CVAR_SET_STRING( "weaponmode_mp5", "0" );
	CVAR_SET_STRING( "weaponmode_crossbow", "0" );
	CVAR_SET_STRING( "weaponmode_rpg", "0" );
	CVAR_SET_STRING( "mp_consistency", "0" );
	CVAR_SET_STRING( "sv_allowdownload", "1" );
	CVAR_SET_STRING( "sv_send_resources", "1" );
	CVAR_SET_STRING( "mp_nextmap", "" );
	CVAR_SET_STRING( "sv_notransition", "" );
	CVAR_SET_STRING( "sv_friction", "4" );

#if 1
	CVAR_SET_STRING("sv_gravity", "800"); // 67ft/sec
	CVAR_SET_STRING("sv_stepsize", "18");
#else
	CVAR_SET_STRING("sv_gravity", "384"); // 32ft/sec
	CVAR_SET_STRING("sv_stepsize", "24");
#endif

	CVAR_SET_STRING("room_type", "0");// clear DSP
	CVAR_SET_FLOAT( "sv_zmax", 16000 );

	CServer_SoundEngine::GetSoundEngine()->Precache();

	g_flNextAmbientRefresh = 0;

	// Set up game rules
	if (g_pGameRules)
	{
		delete g_pGameRules;
	}

	CServer::GetInstance()->MapInit();
	g_pGameRules = InstallGameRules( );

	if ( g_szModelReplacement )
		PrecacheGlobalCustomModels( g_szModelReplacement );

	CMap::GetInstance()->Initialize( );

	//!!!UNDONE why is there so much Spawn code in the Precache function? I'll just keep it here 

	///!!!LATER - do we want a sound ent in deathmatch? (sjb)
	//pSoundEnt = CBaseEntity::Create( "soundent", g_vecZero, g_vecZero, edict() );
	pSoundEnt = GetClassPtr( ( CSoundEnt *)NULL );
	pSoundEnt->Spawn();

	if ( !pSoundEnt )
	{
		ALERT ( at_console, "**COULD NOT CREATE SOUNDENT**\n" );
	}

	Weapon_Init();									// get weapon precaches

	ClientPrecache();
	PrecacheStandardWeaponModels();

// sounds used from C physics code
	PrecacheSound_Intercept( this, "common/null.wav" );	// clears sound channels
	CServer_SoundEngine::GetSoundEngine()->PrecacheSound("common/null.wav");

	PrecacheSound_Intercept( this, "items/suitchargeok1.wav" ); //!!! temporary sound for respawning weapons.
	PrecacheSound_Intercept( this, "items/gunpickup2.wav" ); // player picks up a gun.

	PrecacheSound_Intercept( this, "common/bodydrop3.wav" ); // dead bodies hitting the ground (animation events)
	PrecacheSound_Intercept( this, "common/bodydrop4.wav" );
	
	g_Language = (int)CVAR_GET_FLOAT( "sv_language" );
	if ( g_Language == LANGUAGE_GERMAN )
	{
		PRECACHE_MODEL( "models/germangibs.mdl" );
	}
	else
	{
		PRECACHE_MODEL( "models/hgibs.mdl" );
		PRECACHE_MODEL( "models/agibs.mdl" );
	}

	PRECACHE_MODEL( "sprites/muzzleflash_saw.spr" );

	PrecacheSound_Intercept( this, "weapons/ric1.wav");
	PrecacheSound_Intercept( this, "weapons/ric2.wav");
	PrecacheSound_Intercept( this, "weapons/ric3.wav");
	PrecacheSound_Intercept( this, "weapons/ric4.wav");
	PrecacheSound_Intercept( this, "weapons/ric5.wav" );
	PrecacheSound_Intercept( this, "weapons/explode3.wav" );
	PrecacheSound_Intercept( this, "weapons/explode4.wav" );
	PrecacheSound_Intercept( this, "weapons/explode5.wav" );
	PrecacheSound_Intercept( this, "misc/r_tele3.wav" );
//
// Setup light animation tables. 'a' is total darkness, 'z' is maxbright.
//

	// 0 normal
	LIGHT_STYLE(0, "m");
	
	// 1 FLICKER (first variety)
	LIGHT_STYLE(1, "mmnmmommommnonmmonqnmmo");
	
	// 2 SLOW STRONG PULSE
	LIGHT_STYLE(2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");
	
	// 3 CANDLE (first variety)
	LIGHT_STYLE(3, "mmmmmaaaaammmmmaaaaaabcdefgabcdefg");
	
	// 4 FAST STROBE
	LIGHT_STYLE(4, "mamamamamama");
	
	// 5 GENTLE PULSE 1
	LIGHT_STYLE(5,"jklmnopqrstuvwxyzyxwvutsrqponmlkj");
	
	// 6 FLICKER (second variety)
	LIGHT_STYLE(6, "nmonqnmomnmomomno");
	
	// 7 CANDLE (second variety)
	LIGHT_STYLE(7, "mmmaaaabcdefgmmmmaaaammmaamm");
	
	// 8 CANDLE (third variety)
	LIGHT_STYLE(8, "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa");
	
	// 9 SLOW STROBE (fourth variety)
	LIGHT_STYLE(9, "aaaaaaaazzzzzzzz");
	
	// 10 FLUORESCENT FLICKER
	LIGHT_STYLE(10, "mmamammmmammamamaaamammma");

	// 11 SLOW PULSE NOT FADE TO BLACK
	LIGHT_STYLE(11, "abcdefghijklmnopqrrqponmlkjihgfedcba");
	
	// 12 UNDERWATER LIGHT MUTATION
	// this light only distorts the lightmap - no contribution
	// is made to the brightness of affected surfaces
	LIGHT_STYLE(12, "mmnnmmnnnmmnn");
	
	// styles 32-62 are assigned by the light program for switchable lights

	// 63 testing
	LIGHT_STYLE(63, "a");

	for ( int i = 0; i < ARRAYSIZE(gDecals); i++ )
		gDecals[i].index = DECAL_INDEX( gDecals[i].name );

// init the WorldGraph.
	WorldGraph.InitGraph();

// make sure the .NOD file is newer than the .BSP file.
	if ( !WorldGraph.CheckNODFile ( ( char * )STRING( gpGlobals->mapname ) ) )
	{// NOD file is not present, or is older than the BSP file.
		WorldGraph.AllocNodes ();
	}
	else
	{// Load the node graph for this level
		if ( !WorldGraph.FLoadGraph ( (char *)STRING( gpGlobals->mapname ) ) )
		{// couldn't load, so alloc and prepare to build a graph.
			ALERT ( at_console, "*Error opening .NOD file\n" );
			WorldGraph.AllocNodes ();
		}
		else
		{
			ALERT ( at_console, "\n*Graph Loaded!\n" );
		}
	}

	if ( pev->netname )
	{
		ALERT( at_aiconsole, "Chapter title: %s\n", STRING(pev->netname) );
		CBaseEntity *pEntity = CBaseEntity::Create( "env_message", g_vecZero, g_vecZero, false, NULL );
		if ( pEntity )
		{
			pEntity->SetThink( &CBaseEntity::SUB_CallUseToggle );
			pEntity->pev->message = pev->netname;
			pev->netname = 0;
			pEntity->pev->nextthink = gpGlobals->time + 0.3;
			pEntity->pev->spawnflags = SF_MESSAGE_ONCE;
		}
	}

	if ( pev->spawnflags & SF_WORLD_DARK )
		CVAR_SET_FLOAT( "v_dark", 1.0 );
	else
		CVAR_SET_FLOAT( "v_dark", 0.0 );

	if ( pev->spawnflags & SF_WORLD_TITLE )
		gDisplayTitle = TRUE;		// display the game title if this key is set
	else
		gDisplayTitle = FALSE;

	if ( pev->spawnflags & SF_WORLD_FORCETEAM )
	{
		CVAR_SET_FLOAT( "mp_defaultteam", 1 );
	}
	else
	{
		CVAR_SET_FLOAT( "mp_defaultteam", 0 );
	}

	if ( g_szSoundReplacement )
		m_pSoundReplacementMap = CMap::GetInstance()->GetSoundReplacementMap( STRING( g_szSoundReplacement ), true );

	CTalkMonster::g_talkWaitTime = 0;

	InitBodyQue();

	CString mapPath;
	CMemory<unsigned int> mapData;

	mapPath.Format( "maps/%s.bsp", STRING( gpGlobals->mapname ) );

	dheader_t *header = LoadBSPFile( mapPath.Data(), mapData );

	if ( header )
	{
		CASScriptEntityPreprocessor scripts;
		CBaseEntityPreprocessor *preprocessors[] = { &scripts };
		CMapPreprocessor preprocessor( preprocessors, ARRAYSIZE( preprocessors ) );

		preprocessor.Preprocess( mapData.Data() + header->lumps[LUMP_ENTITIES].fileofs,
			header->lumps[LUMP_ENTITIES].filelen );
	}
	else
	{
		ALERT( at_console, "Failed to load map '%s' for ent data parsing!\n", mapPath.Data() );
		CASServerManager::GetInstance()->LoadModules();
	}

	Weapon_Precache();
}


//
// Just to ignore the "wad" field.
//
void CWorld :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "freeroam" ) )
	{
		g_iActiveIdle = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "scversion" ) || FStrEq( pkvd->szKeyName, "scversion2" ) )
	{
		int version = atoi( pkvd->szValue );

		if ( version > g_iMapForGameVersion )
			g_iMapForGameVersion = version;

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "globalsoundlist" ) )
	{
		g_szSoundReplacement = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "globalmodellist" ) )
	{
		g_szModelReplacement = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "sentence_file"))
	{
		CServer_SoundEngine *sound = CServer_SoundEngine::GetSoundEngine();

		UTIL_CopyString(sound->m_szSentenceFile, pkvd->szValue, sizeof(sound->m_szSentenceFile));
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "materials_file"))
	{
		CServer_SoundEngine *sound = CServer_SoundEngine::GetSoundEngine();

		UTIL_CopyString(sound->m_szMaterialsFile, pkvd->szValue, sizeof(sound->m_szMaterialsFile));
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "skyname") )
	{
		// Sent over net now.
		CVAR_SET_STRING( "sv_skyname", pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "sounds") )
	{
		gpGlobals->cdAudioTrack = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "WaveHeight") )
	{
		// Sent over net now.
		pev->scale = atof(pkvd->szValue) * (1.0/8.0);
		pkvd->fHandled = TRUE;
		CVAR_SET_FLOAT( "sv_wateramp", pev->scale );
	}
	else if ( FStrEq(pkvd->szKeyName, "MaxRange") )
	{
		pev->speed = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "chaptertitle") )
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "startdark") )
	{
		// UNDONE: This is a gross hack!!! The CVAR is NOT sent over the client/sever link
		// but it will work for single player
		int flag = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
		if ( flag )
			pev->spawnflags |= SF_WORLD_DARK;
	}
	else if ( FStrEq(pkvd->szKeyName, "newunit") )
	{
		// Single player only.  Clear save directory if set
		if ( atoi(pkvd->szValue) )
			CVAR_SET_FLOAT( "sv_newunit", 1 );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "gametitle") )
	{
		if ( atoi(pkvd->szValue) )
			pev->spawnflags |= SF_WORLD_TITLE;

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "mapteams") )
	{
		pev->team = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "defaultteam") )
	{
		if ( atoi(pkvd->szValue) )
		{
			pev->spawnflags |= SF_WORLD_FORCETEAM;
		}
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );

	WeaponModelKey( pkvd );
}
