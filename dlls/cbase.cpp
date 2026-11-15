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
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"saverestore.h"
#include	"client.h"
#include	"decals.h"
#include	"gamerules.h"
#include	"game.h"
#include	"monsters.h"
#include "CASServerManager.h"
#include "CASModule.h"
#include "CASLog.h"
#include "CScriptDictionary.h"
#include "CMap.h"
#include "server_entvars.h"
#include "ICustomEntity.h"
#include "CASHook.h"

void EntvarsKeyvalue( entvars_t *pev, KeyValueData *pkvd );

edict_t *CreateNamedEntity_Intercept( int className )
{
	const char *name = STRING( className );

	if ( strlen( name ) < 1 )
	{
		ALERT( at_error, "Tried to create an entity with no class name.\n", name );
		return NULL;
	}

	edict_t *entity;

	try
	{
		entity = g_engfuncs.pfnCreateNamedEntity( className );
	}
	catch ( ... )
	{
		ALERT( at_error, "Tried to create an entity \"%s\" with invalid class name.\n", name );
		return NULL;
	}

	edict_t *instance = entity ? entity : ENT( 0 );
	CBaseEntity *actor = NULL;

	if ( instance && !instance->free && instance->pvPrivateData )
		actor = (CBaseEntity *) instance->pvPrivateData;

	DispatchEntityCreatedHook( actor );
	return entity;
}

void PM_Move ( struct playermove_s *ppmove, int server );
void PM_Init ( struct playermove_s *ppmove );
char PM_FindTextureType( char *name );

extern Vector VecBModelOrigin( entvars_t* pevBModel );
extern DLL_GLOBAL Vector		g_vecAttackDir;
extern DLL_GLOBAL int			g_iSkillLevel;

static DLL_FUNCTIONS gFunctionTable = 
{
	GameDLLInit,				//pfnGameInit
	DispatchSpawn,				//pfnSpawn
	DispatchThink,				//pfnThink
	DispatchUse,				//pfnUse
	DispatchTouch,				//pfnTouch
	DispatchBlocked,			//pfnBlocked
	DispatchKeyValue,			//pfnKeyValue
	DispatchSave,				//pfnSave
	DispatchRestore,			//pfnRestore
	DispatchObjectCollsionBox,	//pfnAbsBox

	SaveWriteFields,			//pfnSaveWriteFields
	SaveReadFields,				//pfnSaveReadFields

	SaveGlobalState,			//pfnSaveGlobalState
	RestoreGlobalState,			//pfnRestoreGlobalState
	ResetGlobalState,			//pfnResetGlobalState

	ClientConnect,				//pfnClientConnect
	ClientDisconnect,			//pfnClientDisconnect
	ClientKill,					//pfnClientKill
	ClientPutInServer,			//pfnClientPutInServer
	ClientCommand,				//pfnClientCommand
	ClientUserInfoChanged,		//pfnClientUserInfoChanged
	ServerActivate,				//pfnServerActivate
	ServerDeactivate,			//pfnServerDeactivate

	PlayerPreThink,				//pfnPlayerPreThink
	PlayerPostThink,			//pfnPlayerPostThink

	StartFrame,					//pfnStartFrame
	ParmsNewLevel,				//pfnParmsNewLevel
	ParmsChangeLevel,			//pfnParmsChangeLevel

	GetGameDescription,         //pfnGetGameDescription    Returns string describing current .dll game.
	PlayerCustomization,        //pfnPlayerCustomization   Notifies .dll of new customization for player.

	SpectatorConnect,			//pfnSpectatorConnect      Called when spectator joins server
	SpectatorDisconnect,        //pfnSpectatorDisconnect   Called when spectator leaves the server
	SpectatorThink,				//pfnSpectatorThink        Called when spectator sends a command packet (usercmd_t)

	Sys_Error,					//pfnSys_Error				Called when engine has encountered an error

	PM_Move,					//pfnPM_Move
	PM_Init,					//pfnPM_Init				Server version of player movement initialization
	PM_FindTextureType,			//pfnPM_FindTextureType
	
	SetupVisibility,			//pfnSetupVisibility        Set up PVS and PAS for networking for this client
	UpdateClientData,			//pfnUpdateClientData       Set up data sent only to specific client
	AddToFullPack,				//pfnAddToFullPack
	CreateBaseline,				//pfnCreateBaseline			Tweak entity baseline for network encoding, allows setup of player baselines, too.
	RegisterEncoders,			//pfnRegisterEncoders		Callbacks for network encoding
	GetWeaponData,				//pfnGetWeaponData
	CmdStart,					//pfnCmdStart
	CmdEnd,						//pfnCmdEnd
	ConnectionlessPacket,		//pfnConnectionlessPacket
	GetHullBounds,				//pfnGetHullBounds
	CreateInstancedBaselines,   //pfnCreateInstancedBaselines
	InconsistentFile,			//pfnInconsistentFile
	AllowLagCompensation,		//pfnAllowLagCompensation
};

static NEW_DLL_FUNCTIONS gNewFunctionTable =
{
	DispatchOnFreeEntPrivateData,	//pfnOnFreeEntPrivateData
	GameDLLUninit,				//pfnGameShutdown
	DispatchShouldCollide,			//pfnShouldCollide
	NULL,							//pfnCvarValue
	NULL,							//pfnCvarValue2
};

static void SetObjectCollisionBox( entvars_t *pev );

extern "C" {

	int GetEntityAPI( DLL_FUNCTIONS *pFunctionTable, int interfaceVersion )
{
	if ( !pFunctionTable || interfaceVersion != INTERFACE_VERSION )
	{
		return FALSE;
	}
	
	memcpy( pFunctionTable, &gFunctionTable, sizeof( DLL_FUNCTIONS ) );
	return TRUE;
}

int GetEntityAPI2( DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion )
{
	if ( !pFunctionTable || *interfaceVersion != INTERFACE_VERSION )
	{
		// Tell engine what version we had, so it can figure out who is out of date.
		*interfaceVersion = INTERFACE_VERSION;
		return FALSE;
	}
	
	memcpy( pFunctionTable, &gFunctionTable, sizeof( DLL_FUNCTIONS ) );
	return TRUE;
}

int GetNewDLLFunctions( NEW_DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion )
{
	if ( !pFunctionTable || *interfaceVersion != NEW_DLL_FUNCTIONS_VERSION )
	{
		*interfaceVersion = NEW_DLL_FUNCTIONS_VERSION;
		return FALSE;
	}

	memcpy( pFunctionTable, &gNewFunctionTable, sizeof( NEW_DLL_FUNCTIONS ) );
	return TRUE;
}

}

void DispatchOnFreeEntPrivateData( edict_t *pent )
{
	if ( !pent )
		return;

	CBaseEntity *pEntity = (CBaseEntity *) GET_PRIVATE( pent );

	if ( pEntity )
	{
		pEntity->OnDestroy();
		pEntity->~CBaseEntity();
	}
}

CBaseEntity::CBaseEntity()
{
	m_pCustomKeyvalues = new CCustomKeyvalues( DEFAULT_CUSTOM_KEYVALUES_MAX_HEIGHT );
	m_flCreationTime = 0.0f;
	m_iExtraSpawnFlags = 0;
	m_iszOnDestroy = 0;
	m_pUserData = NULL;
	m_pSoundReplacementMap = NULL;
	m_hHealer = NULL;
	m_fCustomModel = false;
	m_uiEntityFlags = 0;
	m_vecLastOrigin = Vector( 0, 0, 0 );
	memset( m_uiPortalVisibilityGeneration, 0, sizeof( m_uiPortalVisibilityGeneration ) );
}

CBaseEntity::~CBaseEntity()
{
	delete m_pCustomKeyvalues;
}

void CBaseEntity::OnCreate( void )
{
	m_flCreationTime = gpGlobals->time;
	m_pUserData = NULL;
	SetPlayerAlly( false );
}

void CBaseEntity::Precache( void )
{
	m_fCustomModel = pev->model != 0;

	if ( m_fCustomModel )
		UTIL_PrecacheModel( this, STRING( pev->model ) );

	if ( m_iszSoundList && !m_pSoundReplacementMap )
		m_pSoundReplacementMap = CMap::GetInstance()->GetSoundReplacementMap( STRING( m_iszSoundList ), false );
}

void CBaseEntity::PreSpawn( void )
{
	pev->spawnflags |= m_iExtraSpawnFlags;
	m_iOriginalRenderMode = pev->rendermode;
	m_iOriginalRenderFX = pev->renderfx;
	m_flOriginalRenderAmount = pev->renderamt;
	m_vecOriginalRenderColor = pev->rendercolor;
}

bool CBaseEntity::SetupModel( void )
{
	if ( !m_fCustomModel )
		return false;

	SET_MODEL( ENT( pev ), STRING( pev->model ) );

	if ( !stricmp( STRING( pev->model ), "models/w_svencoop_pickup.mdl" ) )
		return false;

	if ( pev->spawnflags & SF_MONSTER_NO_MODEL_BOX )
		m_fCustomModel = false;

	return true;
}

const char *CBaseEntity::SOUNDREPLACEMENT_Find( const char *sample )
{
	if ( !m_pSoundReplacementMap )
		return sample;

	const char *replacement = (const char *) m_pSoundReplacementMap->Find( sample );

	return replacement ? replacement : sample;
}

void CBaseEntity::AddEntityFlag( int mask )
{
	m_uiEntityFlags |= (unsigned int) mask;
}

void CBaseEntity::RemoveEntityFlag( int mask )
{
	// Deliberately multiplies rather than masks
	m_uiEntityFlags *= (unsigned int) ~mask;
}

bool CBaseEntity::HasEntityFlag( int mask ) const
{
	return ( m_uiEntityFlags & (unsigned int) mask ) != 0;
}

void CBaseEntity::ClearEntityFlags( void )
{
	m_uiEntityFlags = 0;
}

bool CBaseEntity::MandatoryKeyValue( KeyValueData *pkvd )
{
	if ( stricmp( pkvd->szKeyName, "ondestroyfn" ) )
		return false;

	SetOnDestroyFunction( pkvd->szValue );
	pkvd->fHandled = TRUE;

	return true;
}

void CBaseEntity::CreateUserData( void )
{
	if ( !m_pUserData )
		m_pUserData = CScriptDictionary::Create( CASServerManager::GetInstance()->m_pEngine );
}

bool UserData_CheckModuleAccess( void )
{
	CASServerManager *manager = CASServerManager::GetInstance();

	if ( !manager->m_pActiveModule || manager->IsActiveModuleOfType( ModuleType::MAP ) )
		return true;

	manager->SetExceptionOnActiveContext( "Only map scripts can access entity user data!", true );
	return false;
}

CScriptDictionary *CBaseEntity::GetUserData( void )
{
	if ( !UserData_CheckModuleAccess() )
		return NULL;

	CreateUserData();
	m_pUserData->AddRef();
	return m_pUserData;
}

CScriptDictValue &CBaseEntity::GetUserData( const CString &key )
{
	// The keyed entry bypasses the map-script restriction
	CreateUserData();
	return *( *m_pUserData )[key];
}

void CBaseEntity::ClearUserData( void )
{
	ClearUserData( true );
}

void CBaseEntity::ClearUserData( bool checkContext )
{
	if ( checkContext && !UserData_CheckModuleAccess() )
		return;

	if ( m_pUserData )
		m_pUserData->Release();

	m_pUserData = NULL;
}

void CBaseEntity::OnDestroy( void )
{
	CASServerManager *manager = CASServerManager::GetInstance();

	if ( manager->IsScriptLifecycleEntered() && m_iszOnDestroy )
	{
		CASModule *module = manager->GetMapModule();

		if ( module )
			module->CallFunction( false, STRING( m_iszOnDestroy ), 0, this );
	}

	if ( m_pUserData )
		m_pUserData->Release();

	m_pUserData = NULL;
}

void CBaseEntity::SetOnDestroyFunction( const char *name )
{
	if ( !*name )
	{
		m_iszOnDestroy = MAKE_STRING( "" );
		return;
	}

	char declaration[512] = { 0 };
	int length = _snprintf( declaration, sizeof( declaration ), "%s %s(%s)", "void", name, "CBaseEntity@" );

	declaration[sizeof( declaration ) - 1] = 0;

	if ( length < 0 || length >= (int) sizeof( declaration ) )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "%s: function name '%s' is too long!\n",
			pev ? STRING( pev->classname ) : "", name );
		return;
	}

	m_iszOnDestroy = AllocStringLocal( declaration );
}

int CBaseEntity::FormatASFunction( char *buffer, unsigned int size, const char *returnType,
                                   const char *name, const char *parameters, bool warn ) const
{
	if ( !buffer || !size || !returnType || !*returnType || !name || !*name || !parameters )
		return 0;

	int length = _snprintf( buffer, size, "%s %s(%s)", returnType, name, parameters );

	bool truncated = length < 0 || (unsigned int) length >= size;

	if ( warn && truncated )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "%s: function name '%s' is too long!\n",
		                              pev ? STRING( pev->classname ) : "", name );
	}

	return truncated ? 0 : length;
}

bool CBaseEntity::SetCustomKeyvalue( const char *szKeyName, const void *pValue, entvartype_s type, Ownership ownership )
{
	return m_pCustomKeyvalues->SetKeyvalue( szKeyName, pValue, type, ownership );
}

void *CBaseEntity::GetCustomKeyvalue( const char *szKeyName, entvartype_s &type )
{
	return m_pCustomKeyvalues->GetKeyvalue( szKeyName, type );
}

bool CBaseEntity::RemoveCustomKeyvalue( const char *szKeyName )
{
	return m_pCustomKeyvalues->RemoveKeyvalue( szKeyName );
}

bool CBaseEntity::HasCustomKeyvalue( const char *szKeyName )
{
	return m_pCustomKeyvalues->HasKeyvalue( szKeyName );
}

void *CBaseEntity::InitializeCustomKeyvalueWithDefault( const char *szKeyName, entvartype_s &type )
{
	return m_pCustomKeyvalues->InitializeKeyvalueWithDefault( szKeyName, type );
}

entvartype_s CBaseEntity::GetEntvar( const char *szKeyName, void **value, int &index )
{
	entvartype_s type = VAR_INVALID;

	index = INVALID_ENTVAR_INDEX;

	if ( CCustomKeyvalues::IsCustomKeyvalue( szKeyName ) )
	{
		*value = GetCustomKeyvalue( szKeyName, type );

		if ( type == VAR_INVALID )
			*value = InitializeCustomKeyvalueWithDefault( szKeyName, type );

		return type;
	}

	type = GetEntvarField( pev, szKeyName, value, index );

	if ( type == VAR_INVALID )
		*value = GetKeyValue( szKeyName, type );

	return type;
}


int DispatchSpawn( edict_t *pent )
{
	if ( !pent || pent->free || !pent->pvPrivateData )
	{
		ALERT( at_error, "DispatchSpawn called on an invalid entity!\n" );
		return 0;
	}

	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);

	if (pEntity)
	{
		// Initialize these or entities who don't link to the world won't have anything in here
		pEntity->pev->absmin = pEntity->pev->origin - Vector(1,1,1);
		pEntity->pev->absmax = pEntity->pev->origin + Vector(1,1,1);

		pEntity->PreSpawn();
		pEntity->Spawn();

		if ( pent->free )
			return 0;

		// Try to get the pointer again, in case the spawn function deleted the entity.
		// UNDONE: Spawn() should really return a code to ask that the entity be deleted, but
		// that would touch too much code for me to do that right now.
		pEntity = (CBaseEntity *)GET_PRIVATE(pent);

		if ( pEntity )
		{
			if ( g_pGameRules && !g_pGameRules->IsAllowedToSpawn( pEntity ) )
				return -1;	// return that this entity should be deleted
			if ( pEntity->pev->flags & FL_KILLME )
				return -1;

			pEntity->PostSpawn();

			ICustomEntity *custom = pEntity->MyCustomPointer();

			if ( custom && !custom->OnScriptClassReady() )
			{
				CASLog::GetInstance()->Error( AS_LOG_DEFAULT,
					"Custom entity(%s): initialization failed, destroying entity\n", STRING( pEntity->pev->classname ) );
				return -1;
			}
		}


		// Handle global stuff here
		if ( pEntity && pEntity->pev->globalname ) 
		{
			const globalentity_t *pGlobal = gGlobalState.EntityFromTable( pEntity->pev->globalname );
			if ( pGlobal )
			{
				// Already dead? delete
				if ( pGlobal->state == GLOBAL_DEAD )
					return -1;
				else if ( !FStrEq( STRING(gpGlobals->mapname), pGlobal->levelName ) )
					pEntity->MakeDormant();	// Hasn't been moved to this level yet, wait but stay alive
				// In this level & not dead, continue on as normal
			}
			else
			{
				// Spawned entities default to 'On'
				gGlobalState.EntityAdd( pEntity->pev->globalname, gpGlobals->mapname, GLOBAL_ON );
//				ALERT( at_console, "Added global entity %s (%s)\n", STRING(pEntity->pev->classname), STRING(pEntity->pev->globalname) );
			}
		}

	}

	return 0;
}

void DispatchKeyValue( edict_t *pentKeyvalue, KeyValueData *pkvd )
{
	if ( !pkvd || !pentKeyvalue )
		return;

	EntvarsKeyvalue( VARS(pentKeyvalue), pkvd );

	// If the key was an entity variable, or there's no class set yet, don't look for the object, it may
	// not exist yet.
	if ( pkvd->fHandled || pkvd->szClassName == NULL )
		return;

	// Get the actualy entity object
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pentKeyvalue);

	if ( !pEntity )
		return;

	pEntity->KeyValue( pkvd );
}


// HACKHACK -- this is a hack to keep the node graph entity from "touching" things (like triggers)
// while it builds the graph
BOOL gTouchDisabled = FALSE;
void DispatchTouch( edict_t *pentTouched, edict_t *pentOther )
{
	if ( gTouchDisabled )
		return;

	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pentTouched);
	CBaseEntity *pOther = (CBaseEntity *)GET_PRIVATE( pentOther );

	if ( pEntity && pOther && ! ((pEntity->pev->flags | pOther->pev->flags) & FL_KILLME) )
		pEntity->Touch( pOther );
}


void DispatchUse( edict_t *pentUsed, edict_t *pentOther )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pentUsed);
	CBaseEntity *pOther = (CBaseEntity *)GET_PRIVATE(pentOther);

	if (pEntity && !(pEntity->pev->flags & FL_KILLME) )
		pEntity->Use( pOther, pOther, USE_TOGGLE, 0 );
}

void DispatchThink( edict_t *pent )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);
	if (pEntity)
	{
		if ( FBitSet( pEntity->pev->flags, FL_DORMANT ) )
			ALERT( at_error, "Dormant entity %s is thinking!!\n", STRING(pEntity->pev->classname) );
				
		pEntity->Think();
	}
}

void DispatchBlocked( edict_t *pentBlocked, edict_t *pentOther )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE( pentBlocked );
	CBaseEntity *pOther = (CBaseEntity *)GET_PRIVATE( pentOther );

	if (pEntity)
		pEntity->Blocked( pOther );
}

void DispatchSave( edict_t *pent, SAVERESTOREDATA *pSaveData )
{
	if (!pent || pent->free)
		return;

	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);
	
	if ( pEntity && pSaveData )
	{
		ENTITYTABLE *pTable = &pSaveData->pTable[ pSaveData->currentIndex ];

		if ( pTable->pent != pent )
			ALERT( at_error, "ENTITY TABLE OR INDEX IS WRONG!!!!\n" );

		if ( pEntity->ObjectCaps() & FCAP_DONT_SAVE )
			return;

		// These don't use ltime & nextthink as times really, but we'll fudge around it.
		if ( pEntity->pev->movetype == MOVETYPE_PUSH )
		{
			float delta = pEntity->pev->nextthink - pEntity->pev->ltime;
			pEntity->pev->ltime = gpGlobals->time;
			pEntity->pev->nextthink = pEntity->pev->ltime + delta;
		}

		pTable->location = pSaveData->size;		// Remember entity position for file I/O
		pTable->classname = pEntity->pev->classname;	// Remember entity class for respawn

		CSave saveHelper( pSaveData );
		pEntity->Save( saveHelper );

		pTable->size = pSaveData->size - pTable->location;	// Size of entity block is data size written to block
	}
}


// Find the matching global entity.  Spit out an error if the designer made entities of
// different classes with the same global name
CBaseEntity *FindGlobalEntity( string_t classname, string_t globalname )
{
	edict_t *pent = FIND_ENTITY_BY_STRING( NULL, "globalname", STRING(globalname) );

	if (!pent)
		pent = ENT(0);

	if (!pent || pent->free)
		return NULL;

	CBaseEntity *pReturn = (CBaseEntity *)GET_PRIVATE( pent );
	if ( pReturn )
	{
		if ( !FClassnameIs( pReturn->pev, STRING(classname) ) )
		{
			ALERT( at_console, "Global entity found %s, wrong class %s\n", STRING(globalname), STRING(pReturn->pev->classname) );
			pReturn = NULL;
		}
	}

	return pReturn;
}


void InitializeRestoreEntVars(entvars_t &variables)
{
	const Vector zero(0, 0, 0);

	// Entvars construction leaves scalar and pointer fields untouched
	variables.origin = zero;
	variables.oldorigin = zero;
	variables.velocity = zero;
	variables.basevelocity = zero;
	variables.clbasevelocity = zero;
	variables.movedir = zero;
	variables.angles = zero;
	variables.avelocity = zero;
	variables.punchangle = zero;
	variables.v_angle = zero;
	variables.endpos = zero;
	variables.startpos = zero;

	variables.absmin = zero;
	variables.absmax = zero;
	variables.mins = zero;
	variables.maxs = zero;
	variables.size = zero;

	variables.rendercolor = zero;

	variables.view_ofs = zero;

	variables.vuser1 = zero;
	variables.vuser2 = zero;
	variables.vuser3 = zero;
	variables.vuser4 = zero;
}


int DispatchRestore( edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity )
{
	if (!pent || pent->free)
		return 0;

	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);

	if ( pEntity && pSaveData )
	{
		entvars_t tmpVars;
		Vector oldOffset(0, 0, 0);

		InitializeRestoreEntVars(tmpVars);

		CRestore restoreHelper( pSaveData );
		if ( globalEntity )
		{
			CRestore tmpRestore( pSaveData );
			tmpRestore.PrecacheMode( 0 );
			tmpRestore.ReadEntVars( "ENTVARS", &tmpVars );

			// HACKHACK - reset the save pointers, we're going to restore for real this time
			pSaveData->size = pSaveData->pTable[pSaveData->currentIndex].location;
			pSaveData->pCurrentData = pSaveData->pBaseData + pSaveData->size;
			// -------------------


			const globalentity_t *pGlobal = gGlobalState.EntityFromTable( tmpVars.globalname );
			
			// Don't overlay any instance of the global that isn't the latest
			// pSaveData->szCurrentMapName is the level this entity is coming from
			// pGlobla->levelName is the last level the global entity was active in.
			// If they aren't the same, then this global update is out of date.
			if ( !FStrEq( pSaveData->szCurrentMapName, pGlobal->levelName ) )
				return 0;

			// Compute the new global offset
			oldOffset = pSaveData->vecLandmarkOffset;
			CBaseEntity *pNewEntity = FindGlobalEntity( tmpVars.classname, tmpVars.globalname );
			if ( pNewEntity )
			{
//				ALERT( at_console, "Overlay %s with %s\n", STRING(pNewEntity->pev->classname), STRING(tmpVars.classname) );
				// Tell the restore code we're overlaying a global entity from another level
				restoreHelper.SetGlobalMode( 1 );	// Don't overwrite global fields
				pSaveData->vecLandmarkOffset = (pSaveData->vecLandmarkOffset - pNewEntity->pev->mins) + tmpVars.mins;
				pEntity = pNewEntity;// we're going to restore this data OVER the old entity
				pent = ENT( pEntity->pev );
				// Update the global table to say that the global definition of this entity should come from this level
				gGlobalState.EntityUpdate( pEntity->pev->globalname, gpGlobals->mapname );
			}
			else
			{
				// This entity will be freed automatically by the engine.  If we don't do a restore on a matching entity (below)
				// or call EntityUpdate() to move it to this level, we haven't changed global state at all.
				return 0;
			}

		}

		if ( pEntity->ObjectCaps() & FCAP_MUST_SPAWN )
		{
			pEntity->Restore( restoreHelper );
			pEntity->Spawn();
		}
		else
		{
			pEntity->Restore( restoreHelper );
			pEntity->Precache( );
		}

		// Again, could be deleted, get the pointer again.
		pEntity = NULL;

		if (pent && !pent->free)
			pEntity = (CBaseEntity *)GET_PRIVATE(pent);

#if 0
		if ( pEntity && pEntity->pev->globalname && globalEntity ) 
		{
			ALERT( at_console, "Global %s is %s\n", STRING(pEntity->pev->globalname), STRING(pEntity->pev->model) );
		}
#endif

		// Is this an overriding global entity (coming over the transition), or one restoring in a level
		if ( globalEntity )
		{
//			ALERT( at_console, "After: %f %f %f %s\n", pEntity->pev->origin.x, pEntity->pev->origin.y, pEntity->pev->origin.z, STRING(pEntity->pev->model) );
			pSaveData->vecLandmarkOffset = oldOffset;
			if ( pEntity )
			{
				UTIL_SetOrigin( pEntity->pev, pEntity->pev->origin );
				pEntity->OverrideReset();
			}
		}
		else if ( pEntity && pEntity->pev->globalname ) 
		{
			const globalentity_t *pGlobal = gGlobalState.EntityFromTable( pEntity->pev->globalname );
			if ( pGlobal )
			{
				// Already dead? delete
				if ( pGlobal->state == GLOBAL_DEAD )
					return -1;
				else if ( !FStrEq( STRING(gpGlobals->mapname), pGlobal->levelName ) )
				{
					pEntity->MakeDormant();	// Hasn't been moved to this level yet, wait but stay alive
				}
				// In this level & not dead, continue on as normal
			}
			else
			{
				ALERT( at_error, "Global Entity %s (%s) not in table!!!\n", STRING(pEntity->pev->globalname), STRING(pEntity->pev->classname) );
				// Spawned entities default to 'On'
				gGlobalState.EntityAdd( pEntity->pev->globalname, gpGlobals->mapname, GLOBAL_ON );
			}
		}
	}
	return 0;
}


void DispatchObjectCollsionBox( edict_t *pent )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);
	if (pEntity)
	{
		pEntity->SetObjectCollisionBox();
	}
	else
		SetObjectCollisionBox( &pent->v );
}


void SaveWriteFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount )
{
	CSave saveHelper( pSaveData );
	saveHelper.WriteFields( pname, pBaseData, pFields, fieldCount );
}


void SaveReadFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount )
{
	CRestore restoreHelper( pSaveData );
	restoreHelper.ReadFields( pname, pBaseData, pFields, fieldCount );
}


edict_t * EHANDLE::Get( void ) 
{ 
	if (m_pent)
	{
		if (m_pent->serialnumber == m_serialnumber) 
			return m_pent; 
		else
			return NULL;
	}
	return NULL; 
};

edict_t * EHANDLE::Set( edict_t *pent ) 
{ 
	m_pent = pent;  
	if (pent) 
		m_serialnumber = m_pent->serialnumber; 
	return pent; 
};


EHANDLE :: operator CBaseEntity *() 
{ 
	return (CBaseEntity *)GET_PRIVATE( Get( ) ); 
};


CBaseEntity * EHANDLE :: operator = (CBaseEntity *pEntity)
{
	if (pEntity)
	{
		m_pent = ENT( pEntity->pev );
		if (m_pent)
			m_serialnumber = m_pent->serialnumber;
	}
	else
	{
		m_pent = NULL;
		m_serialnumber = 0;
	}
	return pEntity;
}

CBaseEntity * EHANDLE :: operator -> ()
{
	return (CBaseEntity *)GET_PRIVATE( Get( ) ); 
}


// Give health up to health_cap or max_health
// Negative amounts cause damage
bool CBaseEntity :: TakeHealth( float flHealth, int bitsDamageType, int health_cap )
{
	if (!pev->takedamage && !IsPlayer() )
		return false;

	if ( !IsAlive() )
		return false;

	float flCap = pev->max_health;

	if ( flHealth >= 0 )
	{
		if ( health_cap )
			flCap = health_cap;

// heal
	if ( pev->health >= flCap )
			return false;
	}

	pev->health += flHealth;

	if (pev->health > flCap )
		pev->health = flCap;

	if ( (int) pev->health <= 0 )
		Killed( pev, GIB_NORMAL );

	return true;
}

// Give armor up to armor_cap or armortype
bool CBaseEntity :: TakeArmor( float flArmor, int bitsDamageType, int armor_cap )
{
	if ( !pev->takedamage )
		return false;

	float flCap = pev->armortype;

	if ( armor_cap )
	{
		flCap = armor_cap;

		if ( pev->armorvalue >= flCap )
			return false;
	}
	else if ( flArmor >= 0 && pev->armorvalue >= flCap )
	{
		return false;
	}

	pev->armorvalue += flArmor;

	if ( pev->armorvalue > flCap )
		pev->armorvalue = flCap;

	return true;
}

void CBaseEntity :: Teleport( Vector vecOrigin, Vector vecAngles )
{
	UTIL_SetOrigin( pev, vecOrigin );
	pev->angles = vecAngles;
}

// Fills pList with living entities whose flags match flagMask (any entity when 0)
int CBaseEntity :: FindMonstersInWorld( CBaseEntity **pList, int listMax, int flagMask )
{
	edict_t *pEdict = INDEXENT( 1 );

	if ( !pEdict )
		return 0;

	int count = 0;

	for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		if ( pEdict->free )
			continue;

		if ( flagMask && !( pEdict->v.flags & flagMask ) )
			continue;

		CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );

		if ( !pEntity || !pEntity->IsAlive() )
			continue;

		pList[count++] = pEntity;

		if ( count >= listMax )
			return listMax;
	}

	return count;
}

// Point entities are triggers without a brush model
bool CBaseEntity :: IsPointEnt( void )
{
	if ( pev->solid != SOLID_TRIGGER )
		return false;

	if ( !IsBSPModel() )
		return true;

	return !pev->modelindex;
}

bool CBaseEntity::FVisibleFromPos( const Vector &start, const Vector &end )
{
	TraceResult trace;
	edict_t *entity = pev ? ENT( pev ) : NULL;

	trace.vecEndPos = Vector( 0.0f, 0.0f, 0.0f );
	trace.vecPlaneNormal = Vector( 0.0f, 0.0f, 0.0f );

	UTIL_TraceLine( start, end, ignore_monsters, ignore_glass, entity, &trace );

	return trace.flFraction == 1.0f;
}

bool CBaseEntity::IsFacing( entvars_t *target, float fieldOfView )
{
	Vector direction( target->origin.x - pev->origin.x, target->origin.y - pev->origin.y, 0.0f );
	float lengthSquared = direction.y * direction.y + direction.x * direction.x + 0.0f;
	float length = (float) sqrt( (double) lengthSquared );

	if ( length == 0.0f )
		direction = Vector( 0.0f, 0.0f, 1.0f );
	else
	{
		float inverse = 1.0f / length;

		direction.x *= inverse;
		direction.y *= inverse;
		direction.z *= inverse;
	}

	Vector angles = pev->v_angle;
	Vector forward( 0.0f, 0.0f, 0.0f );

	angles.x = 0.0f;
	AngleVectors( angles, &forward, NULL, NULL );

	float dot = forward.y * direction.y + forward.x * direction.x + forward.z * direction.z;

	return dot > fieldOfView;
}

CBaseEntity *DamagePointsEntity( entvars_t *variables )
{
	edict_t *entity = variables ? ENT( variables ) : NULL;

	if ( !entity )
		entity = ENT( 0 );

	if ( !entity || entity->free )
		return NULL;

	return (CBaseEntity *) GET_PRIVATE( entity );
}

void CBaseEntity::GetDamagePoints( entvars_t *inflictor, entvars_t *attacker, float damage )
{
	if ( ( !inflictor && !attacker ) || pev->health < 0.0f || !IsAlive() )
		return;

	CBaseEntity *attackerEntity = DamagePointsEntity( attacker );
	CBaseEntity *inflictorEntity = DamagePointsEntity( inflictor );
	int multiplier = 1;

	if ( inflictorEntity && inflictorEntity->IsPlayer()
		&& g_pGameRules->PlayerRelationship( inflictorEntity, this ) == GR_TEAMMATE )
		multiplier = -1;

	if ( attackerEntity && attackerEntity->IsPlayer()
		&& g_pGameRules->PlayerRelationship( attackerEntity, this ) == GR_TEAMMATE )
		multiplier = -1;

	CBaseEntity *recipient = inflictorEntity;

	if ( attackerEntity && ( attackerEntity->IsPlayer() || ( attackerEntity->pev->flags & FL_MONSTER ) ) )
		recipient = attackerEntity;

	if ( !recipient )
		return;

	float previous = recipient->pev->frags;
	float amount = pev->health < damage ? pev->health : damage;
	double points = (double) GetPointsForDamage( amount ) * (double) multiplier;

	recipient->pev->frags = (float) ( (double) previous + points );
}

// Something blocks a mover: clear what can't be crushed, then hurt it. Returns whether it survived
bool CBaseEntity :: BlockedByEntity( CBaseEntity *pOther, float flDamage )
{
	if ( !pOther )
		return false;

	if ( FClassnameIs( pOther->pev, "weaponbox" )
		|| FClassnameIs( pOther->pev, "monster_satchel" )
		|| FClassnameIs( pOther->pev, "monster_tripmine" ) )
	{
		pOther->CriticalRemove();
	}

	if ( flDamage != 0 )
		pOther->TakeDamage( pev, pev, flDamage, DMG_CRUSH );

	return pOther->pev->health > 0;
}

// inflict damage on this entity.  bitsDamageType indicates type of damage inflicted, ie: DMG_CRUSH

int CBaseEntity :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	Vector			vecTemp;

	if (!pev->takedamage)
		return 0;

	// UNDONE: some entity types may be immune or resistant to some bitsDamageType
	
	// if Attacker == Inflictor, the attack was a melee or other instant-hit attack.
	// (that is, no actual entity projectile was involved in the attack so use the shooter's origin). 
	if ( pevAttacker == pevInflictor )	
	{
		vecTemp = pevInflictor->origin - ( VecBModelOrigin(pev) );
	}
	else
	// an actual missile was involved.
	{
		vecTemp = pevInflictor->origin - ( VecBModelOrigin(pev) );
	}

// this global is still used for glass and other non-monster killables, along with decals.
	g_vecAttackDir = vecTemp.Normalize();
		
// save damage based on the target's armor level

// figure momentum add (don't let hurt brushes or other triggers move player)
	if ((!FNullEnt(pevInflictor)) && (pev->movetype == MOVETYPE_WALK || pev->movetype == MOVETYPE_STEP) && (pevAttacker->solid != SOLID_TRIGGER) )
	{
		Vector vecDir = pev->origin - (pevInflictor->absmin + pevInflictor->absmax) * 0.5;
		vecDir = vecDir.Normalize();

		float flForce = flDamage * ((32 * 32 * 72.0) / (pev->size.x * pev->size.y * pev->size.z)) * 5;
		
		if (flForce > 1000.0) 
			flForce = 1000.0;
		pev->velocity = pev->velocity + vecDir * flForce;
	}

// do the damage
	pev->health -= flDamage;
	if (pev->health <= 0)
	{
		Killed( pevAttacker, GIB_NORMAL );
		return 0;
	}

	return 1;
}


void CBaseEntity :: Killed( entvars_t *pevAttacker, int iGib )
{
	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DEAD;
	UTIL_Remove( this );
}


CBaseEntity *CBaseEntity::GetNextTarget( void )
{
	if ( FStringNull( pev->target ) )
		return NULL;
	edict_t *pTarget = FIND_ENTITY_BY_TARGETNAME ( NULL, STRING(pev->target) );
	if ( FNullEnt(pTarget) )
		return NULL;

	return Instance( pTarget );
}

// Global Savedata for Delay
TYPEDESCRIPTION	CBaseEntity::m_SaveData[] = 
{
	DEFINE_FIELD( CBaseEntity, m_hGoalEnt, FIELD_EHANDLE ),

	DEFINE_FIELD( CBaseEntity, m_pfnThink, FIELD_FUNCTION ),		// UNDONE: Build table of these!!!
	DEFINE_FIELD( CBaseEntity, m_pfnTouch, FIELD_FUNCTION ),
	DEFINE_FIELD( CBaseEntity, m_pfnUse, FIELD_FUNCTION ),
	DEFINE_FIELD( CBaseEntity, m_pfnBlocked, FIELD_FUNCTION ),
};


int CBaseEntity::Save( CSave &save )
{
	if ( save.WriteEntVars( "ENTVARS", pev ) )
		return save.WriteFields( "BASE", this, m_SaveData, ARRAYSIZE(m_SaveData) );

	return 0;
}

int CBaseEntity::Restore( CRestore &restore )
{
	int status;

	status = restore.ReadEntVars( "ENTVARS", pev );
	if ( status )
		status = restore.ReadFields( "BASE", this, m_SaveData, ARRAYSIZE(m_SaveData) );

    if ( pev->modelindex != 0 && !FStringNull(pev->model) )
	{
		Vector mins, maxs;
		mins = pev->mins;	// Set model is about to destroy these
		maxs = pev->maxs;


		PRECACHE_MODEL( (char *)STRING(pev->model) );
		SET_MODEL(ENT(pev), STRING(pev->model));
		UTIL_SetSize(pev, mins, maxs);	// Reset them
	}

	return status;
}


// Initialize absmin & absmax to the appropriate box
void SetObjectCollisionBox( entvars_t *pev )
{
	if ( (pev->solid == SOLID_BSP) && 
		 (pev->angles.x || pev->angles.y|| pev->angles.z) )
	{	// expand for rotation
		float		max, v;
		int			i;

		max = 0;
		for (i=0 ; i<3 ; i++)
		{
			v = fabs( ((float *)pev->mins)[i]);
			if (v > max)
				max = v;
			v = fabs( ((float *)pev->maxs)[i]);
			if (v > max)
				max = v;
		}
		for (i=0 ; i<3 ; i++)
		{
			((float *)pev->absmin)[i] = ((float *)pev->origin)[i] - max;
			((float *)pev->absmax)[i] = ((float *)pev->origin)[i] + max;
		}
	}
	else
	{
		pev->absmin = pev->origin + pev->mins;
		pev->absmax = pev->origin + pev->maxs;
	}

	pev->absmin.x -= 1;
	pev->absmin.y -= 1;
	pev->absmin.z -= 1;
	pev->absmax.x += 1;
	pev->absmax.y += 1;
	pev->absmax.z += 1;
}


void CBaseEntity::SetObjectCollisionBox( void )
{
	::SetObjectCollisionBox( pev );
}


int	CBaseEntity :: Intersects( CBaseEntity *pOther )
{
	if (!pOther || !pOther->pev)
		return 0;

	edict_t *edict = pOther->pev->pContainingEntity;

	if (!edict || edict->free || !OFFSET(edict))
		return 0;

	// Unordered coordinates do not count as a separating axis
	if ( pOther->pev->absmin.x > pev->absmax.x ||
		 pOther->pev->absmin.y > pev->absmax.y ||
		 pOther->pev->absmin.z > pev->absmax.z ||
		pev->absmin.x > pOther->pev->absmax.x ||
		pev->absmin.y > pOther->pev->absmax.y ||
		pev->absmin.z > pOther->pev->absmax.z )
		 return 0;
	return 1;
}

void CBaseEntity :: MakeDormant( void )
{
	SetBits( pev->flags, FL_DORMANT );
	
	// Don't touch
	pev->solid = SOLID_NOT;
	// Don't move
	pev->movetype = MOVETYPE_NONE;
	// Don't draw
	SetBits( pev->effects, EF_NODRAW );
	// Don't think
	pev->nextthink = 0;
	// Relink
	UTIL_SetOrigin( pev, pev->origin );
}

int CBaseEntity :: IsDormant( void )
{
	return FBitSet( pev->flags, FL_DORMANT );
}

BOOL CBaseEntity :: IsInWorld( void )
{
	// position 
	if (pev->origin.x >= 4096) return FALSE;
	if (pev->origin.y >= 4096) return FALSE;
	if (pev->origin.z >= 4096) return FALSE;
	if (pev->origin.x <= -4096) return FALSE;
	if (pev->origin.y <= -4096) return FALSE;
	if (pev->origin.z <= -4096) return FALSE;
	// speed
	if (pev->velocity.x >= 2000) return FALSE;
	if (pev->velocity.y >= 2000) return FALSE;
	if (pev->velocity.z >= 2000) return FALSE;
	if (pev->velocity.x <= -2000) return FALSE;
	if (pev->velocity.y <= -2000) return FALSE;
	if (pev->velocity.z <= -2000) return FALSE;

	return TRUE;
}

int CBaseEntity::ShouldToggle( USE_TYPE useType, BOOL currentState )
{
	if ( useType != USE_TOGGLE && useType != USE_SET )
	{
		if ( (currentState && useType == USE_ON) || (!currentState && useType == USE_OFF) )
			return 0;
	}
	return 1;
}


int	CBaseEntity :: DamageDecal( int bitsDamageType )
{
	if ( pev->rendermode == kRenderTransAlpha )
		return -1;

	if ( pev->rendermode != kRenderNormal )
		return DECAL_BPROOF1;

	return DECAL_GUNSHOT1 + RANDOM_LONG(0,4);
}



// NOTE: szName must be a pointer to constant memory, e.g. "monster_class" because the entity
// will keep a pointer to it after this call.
// fCreateAndDontSpawn leaves Spawn to the caller, so keyvalues can be set first
CBaseEntity * CBaseEntity::Create( const char *szName, const Vector &vecOrigin, const Vector &vecAngles, bool fCreateAndDontSpawn, edict_t *pentOwner )
{
	if ( !szName || !szName[0] )
		return NULL;

	edict_t *pent = CREATE_NAMED_ENTITY( MAKE_STRING( szName ));
	if ( FNullEnt( pent ) || !GET_PRIVATE( pent ) )
	{
		ALERT ( at_console, "CBaseEntity::Create: Couldn't create entity \"%s\"!\n", szName );
		return NULL;
	}

	CBaseEntity *pEntity = Instance( pent );
	pEntity->pev->owner = pentOwner;
	pEntity->pev->origin = vecOrigin;
	pEntity->pev->angles = vecAngles;
	pEntity->pev->classname = MAKE_STRING( szName );

	if ( !fCreateAndDontSpawn )
		DispatchSpawn( pEntity->edict() );
	return pEntity;
}


