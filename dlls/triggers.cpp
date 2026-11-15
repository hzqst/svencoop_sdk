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

===== triggers.cpp ========================================================

  spawn and use functions for editor-placed triggers              

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "saverestore.h"
#include "gamerules.h"
#include "game_persistent_inventory.h"
#include "effects.h"
#include "sprite_train.h"
#include "InventoryRules.h"
#include "EntityHandle.h"
#include "user_messages.h"
#include "teleportdestination.h"
#include "trains.h"

#define	SF_TRIG_PUSH_ALLOW_PUSHABLES	4
#define	SF_TRIG_PUSH_NO_CLIENTS		8
#define	SF_TRIG_PUSH_NO_MONSTERS	16
#define	SF_TRIG_PUSH_LIMIT			64

#define	TRIGGER_PUSH_MODE_BASEVELOCITY	0
#define	TRIGGER_PUSH_MODE_LIMIT			1
#define	TRIGGER_PUSH_MODE_SET			2

#define	SF_TRIGGER_PUSH_START_OFF	2//spawnflag that makes trigger_push spawn turned OFF
#define SF_TRIGGER_HURT_TARGETONCE	1// Only fire hurt target once
#define	SF_TRIGGER_HURT_START_OFF	2//spawnflag that makes trigger_push spawn turned OFF
#define	SF_TRIGGER_HURT_NO_CLIENTS	8//spawnflag that makes trigger_push spawn turned OFF
#define SF_TRIGGER_HURT_CLIENTONLYFIRE	16// trigger hurt will only fire its target if it is hurting a client
#define SF_TRIGGER_HURT_CLIENTONLYTOUCH 32// only clients may touch this trigger.
#define SF_TRIGGER_HURT_NON_MOVING 64// also hurts monsters that stand still inside the trigger

#define SF_TRIGGER_EVERYTHING_ELSE 8
#define SF_TRIGGER_FIRE_ON_ENTER 16
#define SF_TRIGGER_FIRE_ON_EXIT 32

extern DLL_GLOBAL BOOL		g_fGameOver;

extern void SetMovedir(entvars_t* pev);
extern Vector VecBModelOrigin( entvars_t* pevBModel );

class CFrictionModifier : public CBaseEntity
{
public:
	void		Spawn( void );
	void		KeyValue( KeyValueData *pkvd );
	void EXPORT	ChangeFriction( CBaseEntity *pOther );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	static	TYPEDESCRIPTION m_SaveData[];

	float		m_frictionFraction;		// Sorry, couldn't resist this name :)
};

LINK_ENTITY_TO_CLASS( func_friction, CFrictionModifier );

// Global Savedata for changelevel friction modifier
TYPEDESCRIPTION	CFrictionModifier::m_SaveData[] = 
{
	DEFINE_FIELD( CFrictionModifier, m_frictionFraction, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE(CFrictionModifier,CBaseEntity);


// Modify an entity's friction
void CFrictionModifier :: Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	SET_MODEL(ENT(pev), STRING(pev->model));    // set size and link into world
	pev->movetype = MOVETYPE_NONE;
	SetTouch( &CFrictionModifier::ChangeFriction );
}


// Sets toucher's friction to m_frictionFraction (1.0 = normal friction)
void CFrictionModifier :: ChangeFriction( CBaseEntity *pOther )
{
	if ( pOther->pev->movetype != MOVETYPE_BOUNCEMISSILE && pOther->pev->movetype != MOVETYPE_BOUNCE )
		pOther->pev->friction = m_frictionFraction;
}



// Sets toucher's friction to m_frictionFraction (1.0 = normal friction)
void CFrictionModifier :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "modifier"))
	{
		m_frictionFraction = atof(pkvd->szValue) / 100.0;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}


// This trigger will fire when the level spawns (or respawns if not fire once)
// It will check a global state before firing.  It supports delay and killtargets

#define SF_AUTO_FIREONCE		0x0001

class CAutoTrigger : public CBaseDelay
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );
	void Precache( void );
	void Think( void );

	int ObjectCaps( void ) { return CBaseDelay::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

private:
	int			m_globalstate;
	USE_TYPE	triggerType;
};
LINK_ENTITY_TO_CLASS( trigger_auto, CAutoTrigger );

TYPEDESCRIPTION	CAutoTrigger::m_SaveData[] = 
{
	DEFINE_FIELD( CAutoTrigger, m_globalstate, FIELD_STRING ),
	DEFINE_FIELD( CAutoTrigger, triggerType, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE(CAutoTrigger,CBaseDelay);

void CAutoTrigger::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "globalstate"))
	{
		m_globalstate = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "triggerstate"))
	{
		int type = atoi( pkvd->szValue );
		switch( type )
		{
		case 0:
			triggerType = USE_OFF;
			break;
		case 2:
			triggerType = USE_TOGGLE;
			break;
		default:
			triggerType = USE_ON;
			break;
		}
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}


void CAutoTrigger::Spawn( void )
{
	Precache();
}


void CAutoTrigger::Precache( void )
{
	pev->nextthink = gpGlobals->time + 0.1;
}


void CAutoTrigger::Think( void )
{
	if ( !m_globalstate || gGlobalState.EntityGetState( m_globalstate ) == GLOBAL_ON )
	{
		SUB_UseTargets( this, triggerType, 0 );
		if ( pev->spawnflags & SF_AUTO_FIREONCE )
			UTIL_Remove( this );
	}
}



#define SF_RELAY_FIREONCE		0x0001
#define SF_RELAY_USE_ACTIVATOR	0x0040

class CTriggerRelay : public CBaseDelay
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int ObjectCaps( void ) { return CBaseDelay::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

private:
	USE_TYPE	triggerType;
	float		m_flTriggerValue;
	float		m_flDelayBeforeReset;
	float		m_flNextFireTime;
};
LINK_ENTITY_TO_CLASS( trigger_relay, CTriggerRelay );

TYPEDESCRIPTION	CTriggerRelay::m_SaveData[] = 
{
	DEFINE_FIELD( CTriggerRelay, triggerType, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE(CTriggerRelay,CBaseDelay);

void CTriggerRelay::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "triggerstate") || FStrEq(pkvd->szKeyName, "value"))
	{
		int type = atoi( pkvd->szValue );

		switch( type )
		{
		case 0:
			triggerType = USE_OFF;
			break;
		case 2:
			triggerType = USE_TOGGLE;
			break;
		case 3:
			triggerType = USE_SET;
			break;
		case 4:
			triggerType = USE_KILL;
			break;
		default:
			triggerType = USE_ON;
			break;
		}

		pkvd->fHandled = TRUE;
	}
	else if ( !stricmp( pkvd->szKeyName, "triggervalue" ) )
	{
		m_flTriggerValue = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( !stricmp( pkvd->szKeyName, "m_flDelayBeforeReset" ) )
	{
		m_flDelayBeforeReset = V_max( 0.0f, (float) atof( pkvd->szValue ) );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}


void CTriggerRelay::Spawn( void )
{
	if ( FStrEq( STRING( pev->target ), STRING( pev->targetname ) ) )
	{
		ALERT( at_error, "[trigger_relay] %s: target == targetname, clearing the target to avoid recursion.\n", STRING( pev->targetname ) );
		pev->target = 0;
	}
}




void CTriggerRelay::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( gpGlobals->time < m_flNextFireTime )
		return;

	SUB_UseTargets( ( pev->spawnflags & SF_RELAY_USE_ACTIVATOR ) ? pActivator : this, triggerType, m_flTriggerValue );

	if ( pev->spawnflags & SF_RELAY_FIREONCE )
		UTIL_Remove( this );
	else
		m_flNextFireTime = gpGlobals->time + m_flDelayBeforeReset;
}


//**********************************************************
// The Multimanager Entity - when fired, will fire up to 16 targets 
// at specified times.
// FLAG:		THREAD (create clones when triggered)
// FLAG:		CLONE (this is a clone for a threaded execution)

#define SF_MULTIMAN_CLONE		0x80000000
#define SF_MULTIMAN_THREAD		0x00000001

class CMultiManager : public CBaseToggle
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn ( void );
	void EXPORT ManagerThink ( void );
	void EXPORT ManagerUse   ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

#if _DEBUG
	void EXPORT ManagerReport( void );
#endif

	BOOL		HasTarget( string_t targetname );
	
	int ObjectCaps( void ) { return CBaseToggle::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	int		m_cTargets;	// the total number of targets in this manager's fire list.
	int		m_index;	// Current target
	float	m_startTime;// Time we started firing
	int		m_iTargetName	[ MAX_MULTI_TARGETS ];// list if indexes into global string array
	float	m_flTargetDelay [ MAX_MULTI_TARGETS ];// delay (in seconds) from time of manager fire to target fire
	int		m_flTargetAction [ MAX_MULTI_TARGETS ];// trigger state sent to each target
private:
	inline BOOL IsClone( void ) { return (pev->spawnflags & SF_MULTIMAN_CLONE) ? TRUE : FALSE; }
	inline BOOL ShouldClone( void ) 
	{ 
		if ( IsClone() )
			return FALSE;

		return (pev->spawnflags & SF_MULTIMAN_THREAD) ? TRUE : FALSE; 
	}

	CMultiManager *Clone( void );
};
LINK_ENTITY_TO_CLASS( multi_manager, CMultiManager );

// Global Savedata for multi_manager
TYPEDESCRIPTION	CMultiManager::m_SaveData[] = 
{
	DEFINE_FIELD( CMultiManager, m_cTargets, FIELD_INTEGER ),
	DEFINE_FIELD( CMultiManager, m_index, FIELD_INTEGER ),
	DEFINE_FIELD( CMultiManager, m_startTime, FIELD_TIME ),
	DEFINE_ARRAY( CMultiManager, m_iTargetName, FIELD_STRING, MAX_MULTI_TARGETS ),
	DEFINE_ARRAY( CMultiManager, m_flTargetDelay, FIELD_FLOAT, MAX_MULTI_TARGETS ),
	DEFINE_ARRAY( CMultiManager, m_flTargetAction, FIELD_INTEGER, MAX_MULTI_TARGETS ),
};

IMPLEMENT_SAVERESTORE(CMultiManager,CBaseToggle);

void CMultiManager :: KeyValue( KeyValueData *pkvd )
{
	// UNDONE: Maybe this should do something like this:
	//CBaseToggle::KeyValue( pkvd );
	// if ( !pkvd->fHandled )
	// ... etc.

	if (FStrEq(pkvd->szKeyName, "wait"))
	{
		m_flWait = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else // add this field to the target list
	{
		// this assumes that additional fields are targetnames and their values are delay values.
		if ( m_cTargets >= MAX_MULTI_TARGETS )
		{
			ALERT( at_console, "[Multi_manager] %s: Exceeded the maximum targets allowed (%d).\n", STRING( pev->targetname ), MAX_MULTI_TARGETS );
			return;
		}

		char tmp[512];

		UTIL_StripToken( pkvd->szKeyName, tmp, sizeof( tmp ) );
		m_iTargetName [ m_cTargets ] = ALLOC_STRING( tmp );

		const char *pszUseType = strchr( pkvd->szValue, '#' );

		if ( pszUseType )
		{
			// "delay#state", the state is a single digit
			char szUseType[2];

			UTIL_StripToken( pkvd->szValue, tmp, sizeof( tmp ) );
			m_flTargetDelay [ m_cTargets ] = atof( tmp );

			szUseType[0] = pszUseType[1];
			szUseType[1] = 0;

			switch ( atoi( szUseType ) )
			{
			case 0:
				m_flTargetAction [ m_cTargets ] = USE_OFF;
				break;
			case 1:
				m_flTargetAction [ m_cTargets ] = USE_ON;
				break;
			case 2:
				m_flTargetAction [ m_cTargets ] = USE_KILL;
				break;
			default:
				m_flTargetAction [ m_cTargets ] = USE_TOGGLE;
				break;
			}
		}
		else
		{
			m_flTargetDelay [ m_cTargets ] = atof( pkvd->szValue );
			m_flTargetAction [ m_cTargets ] = USE_TOGGLE;
		}

		m_cTargets++;
		pkvd->fHandled = TRUE;
	}
}


void CMultiManager :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	SetUse ( &CMultiManager::ManagerUse );
	SetThink ( &CMultiManager::ManagerThink);

	// Sort targets
	// Quick and dirty bubble sort
	int swapped = 1;

	while ( swapped )
	{
		swapped = 0;
		for ( int i = 1; i < m_cTargets; i++ )
		{
			if ( m_flTargetDelay[i] < m_flTargetDelay[i-1] )
			{
				// Swap out of order elements
				int name = m_iTargetName[i];
				float delay = m_flTargetDelay[i];
				int useType = m_flTargetAction[i];
				m_iTargetName[i] = m_iTargetName[i-1];
				m_flTargetDelay[i] = m_flTargetDelay[i-1];
				m_flTargetAction[i] = m_flTargetAction[i-1];
				m_iTargetName[i-1] = name;
				m_flTargetDelay[i-1] = delay;
				m_flTargetAction[i-1] = useType;
				swapped = 1;
			}
		}
	}
}


BOOL CMultiManager::HasTarget( string_t targetname )
{ 
	for ( int i = 0; i < m_cTargets; i++ )
		if ( FStrEq(STRING(targetname), STRING(m_iTargetName[i])) )
			return TRUE;
	
	return FALSE;
}


// Designers were using this to fire targets that may or may not exist -- 
// so I changed it to use the standard target fire code, made it a little simpler.
void CMultiManager :: ManagerThink ( void )
{
	float	time;

	time = gpGlobals->time - m_startTime;
	while ( m_index < m_cTargets && m_flTargetDelay[ m_index ] <= time )
	{
		if ( m_flTargetAction[ m_index ] == USE_KILL )
		{
			edict_t *pentTarget = NULL;

			while ( !FNullEnt( pentTarget = FIND_ENTITY_BY_TARGETNAME( pentTarget, STRING( m_iTargetName[ m_index ] ) ) ) )
			{
				CBaseEntity *pTarget = CBaseEntity::Instance( pentTarget );

				if ( pTarget && !FBitSet( pTarget->pev->flags, FL_KILLME ) )
					UTIL_Remove( pTarget );
			}
		}
		else
		{
			FireTargets( STRING( m_iTargetName[ m_index ] ), m_hActivator, this, (USE_TYPE) m_flTargetAction[ m_index ], 0 );
		}

		m_index++;
	}

	if ( m_index >= m_cTargets )// have we fired all targets?
	{
		SetThink( NULL );
		if ( IsClone() )
		{
			UTIL_Remove( this );
			return;
		}
		SetUse ( &CMultiManager::ManagerUse );// allow manager re-use 
	}
	else
		pev->nextthink = m_startTime + m_flTargetDelay[ m_index ];
}

CMultiManager *CMultiManager::Clone( void )
{
	CMultiManager *pMulti = GetClassPtr( (CMultiManager *)NULL );

	edict_t *pEdict = pMulti->pev->pContainingEntity;
	memcpy( pMulti->pev, pev, sizeof(*pev) );
	pMulti->pev->pContainingEntity = pEdict;

	pMulti->pev->spawnflags |= SF_MULTIMAN_CLONE;
	pMulti->m_cTargets = m_cTargets;
	memcpy( pMulti->m_iTargetName, m_iTargetName, sizeof( m_iTargetName ) );
	memcpy( pMulti->m_flTargetDelay, m_flTargetDelay, sizeof( m_flTargetDelay ) );
	memcpy( pMulti->m_flTargetAction, m_flTargetAction, sizeof( m_flTargetAction ) );

	return pMulti;
}


// The USE function builds the time table and starts the entity thinking.
void CMultiManager :: ManagerUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// In multiplayer games, clone the MM and execute in the clone (like a thread)
	// to allow multiple players to trigger the same multimanager
	if ( ShouldClone() )
	{
		CMultiManager *pClone = Clone();
		pClone->ManagerUse( pActivator, pCaller, useType, value );
		return;
	}

	m_hActivator = pActivator;
	m_index = 0;
	m_startTime = gpGlobals->time;

	SetUse( NULL );// disable use until all targets have fired

	SetThink ( &CMultiManager::ManagerThink );
	pev->nextthink = gpGlobals->time;
}

#if _DEBUG
void CMultiManager :: ManagerReport ( void )
{
	int	cIndex;

	for ( cIndex = 0 ; cIndex < m_cTargets ; cIndex++ )
	{
		ALERT ( at_console, "%s %f\n", STRING(m_iTargetName[cIndex]), m_flTargetDelay[cIndex] );
	}
}
#endif

//***********************************************************


//
// Render parameters trigger
//
// This entity will copy its render parameters (renderfx, rendermode, rendercolor, renderamt)
// to its targets when triggered.
//


// Flags to indicate masking off various render parameters that are normally copied to the targets
#define SF_RENDER_MASKFX	(1<<0)
#define SF_RENDER_MASKAMT	(1<<1)
#define SF_RENDER_MASKMODE	(1<<2)
#define SF_RENDER_MASKCOLOR	(1<<3)
#define SF_RENDER_CONTINUOUS ( 1 << 4 )

class CRenderFxManager : public CBaseEntity
{
public:
	void Spawn( void );
	void Use ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void SetRenderProperties( CBaseEntity *pTarget );
	void EXPORT EffectThink( void );
};

LINK_ENTITY_TO_CLASS( env_render, CRenderFxManager );

void CRenderFxManager :: Spawn ( void )
{
	pev->solid = SOLID_NOT;

	if ( FBitSet( pev->spawnflags, SF_RENDER_CONTINUOUS ) )
	{
		SetThink( &CRenderFxManager::EffectThink );
		pev->nextthink = gpGlobals->time + 0.1f;
	}
}

void CRenderFxManager::EffectThink( void )
{
	Use( NULL, NULL, USE_TOGGLE, 1.0f );
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CRenderFxManager :: Use ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( FStringNull(pev->target))
		return;

	if ( FStrEq( STRING( pev->target ), "!activator" ) )
	{
		SetRenderProperties( pActivator );
		return;
	}

	if ( FStrEq( STRING( pev->target ), "!caller" ) )
	{
		SetRenderProperties( pCaller );
		return;
	}

	CBaseEntity * pentTarget	= NULL;

	for ( ;; )
	{
		if ( pev->frags <= 0 )
			pentTarget = UTIL_FindEntityByTargetname(pentTarget, STRING(pev->target));
		else
			pentTarget = UTIL_FindEntityInSphere( pentTarget, pev->origin, pev->frags, STRING( pev->target ), "targetname" );
			if ( !pentTarget )
			return;
			if ( !pentTarget->IsAlive() && pev->frags > 0 )
			return;

		SetRenderProperties( pentTarget );
		}
	}

void CRenderFxManager::SetRenderProperties( CBaseEntity *pTarget )
{
	if ( !pTarget )
		return;

	if ( FClassnameIs( pTarget->pev, "env_spritetrain" ) )
		pTarget = static_cast<CSpriteTrain *>( pTarget )->m_hSprite;

	if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKMODE ) )
	{
		pTarget->pev->rendermode = pev->rendermode;
		pTarget->m_iOriginalRenderMode = pev->rendermode;
	}

	if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKFX ) )
	{
		pTarget->pev->renderfx = pev->renderfx;
		pTarget->m_iOriginalRenderFX = pev->renderfx;
	}

	if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKAMT ) )
	{
		pTarget->pev->renderamt = pev->renderamt;
		pTarget->m_flOriginalRenderAmount = pev->renderamt;
	}

	if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKCOLOR ) )
	{
		pTarget->pev->rendercolor = pev->rendercolor;
		pTarget->m_vecOriginalRenderColor = pev->rendercolor;
	}
}

struct CTriggerActivator
{
	CTriggerActivator() : m_pNext( NULL ) {}

	EntityHandle<CBaseEntity>	m_hEntity;
	CTriggerActivator		   *m_pNext;
};

class CBaseTrigger : public CBaseToggle
{
public:
	void EXPORT TeleportTouch ( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT MultiTouch( CBaseEntity *pOther );
	void EXPORT HurtTouch ( CBaseEntity *pOther );
	void EXPORT CDAudioTouch ( CBaseEntity *pOther );
	void ActivateMultiTrigger( CBaseEntity *pActivator );
	bool FireMultiTrigger( CBaseEntity *pActivator );
	void EXPORT MultiWaitOver( void );
	void EXPORT CounterUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void InitTrigger( void );

	virtual int	ObjectCaps( void ) { return CBaseToggle :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	float					m_flNextTriggerTime;
	bool					m_fTinyMonsters;
	CTriggerActivator	   *m_pActivators;
};

LINK_ENTITY_TO_CLASS( trigger, CBaseTrigger );

/*
================
InitTrigger
================
*/
void CBaseTrigger::InitTrigger( )
{
	// trigger angles are used for one-way touches.  An angle of 0 is assumed
	// to mean no restrictions, so use a yaw of 360 instead.
	if (pev->angles != g_vecZero)
		SetMovedir(pev);
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	SET_MODEL(ENT(pev), STRING(pev->model));    // set size and link into world
	if ( CVAR_GET_FLOAT("showtriggers") == 0 )
		SetBits( pev->effects, EF_NODRAW );

	m_fTinyMonsters = false;
}


//
// Cache user-entity-field values until spawn is called.
//

void CBaseTrigger :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "count"))
	{
		m_cTriggersLeft = (int) atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damagetype"))
	{
		m_bitsDamageInflict = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "tiny_monsters" ) )
	{
		m_fTinyMonsters = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

class CTriggerHurt : public CBaseTrigger
{
public:
	void Spawn( void );
	void Think( void );
	void EXPORT RadiationThink( void );

	float m_flNextRadiationThink;
};

LINK_ENTITY_TO_CLASS( trigger_hurt, CTriggerHurt );

class CTriggerGeneWormHit : public CTriggerHurt
{
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int ObjectCaps( void ) { return 0; }
};

LINK_ENTITY_TO_CLASS( trigger_geneworm_hit, CTriggerGeneWormHit );

void CTriggerGeneWormHit::Spawn( void )
{
	pev->dmg = 20.0f;
	CTriggerHurt::Spawn();
}

void CTriggerGeneWormHit::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_pfnUse )
		( this->*m_pfnUse )( pActivator, pCaller, useType, value );
}

//
// trigger_monsterjump
//
class CTriggerMonsterJump : public CBaseTrigger
{
public:
	void Spawn( void );
	void Touch( CBaseEntity *pOther );
	void Think( void );
};

LINK_ENTITY_TO_CLASS( trigger_monsterjump, CTriggerMonsterJump );


void CTriggerMonsterJump :: Spawn ( void )
{
	SetMovedir ( pev );
	
	InitTrigger ();

	pev->nextthink = 0;
	pev->speed = 200;
	m_flHeight = 150;

	if ( !FStringNull ( pev->targetname ) )
	{// if targetted, spawn turned off
		pev->solid = SOLID_NOT;
		UTIL_SetOrigin( pev, pev->origin ); // Unlink from trigger list
		SetUse( &CTriggerMonsterJump::ToggleUse );
	}
}


void CTriggerMonsterJump :: Think( void )
{
	pev->solid = SOLID_NOT;// kill the trigger for now !!!UNDONE
	UTIL_SetOrigin( pev, pev->origin ); // Unlink from trigger list
	SetThink( NULL );
}

void CTriggerMonsterJump :: Touch( CBaseEntity *pOther )
{
	entvars_t *pevOther = pOther->pev;

	if ( !FBitSet ( pevOther->flags , FL_MONSTER ) ) 
	{// touched by a non-monster.
		return;
	}

	pevOther->origin.z += 1;
	
	if ( FBitSet ( pevOther->flags, FL_ONGROUND ) ) 
	{// clear the onground so physics don't bitch
		pevOther->flags &= ~FL_ONGROUND;
	}

	// toss the monster!
	pevOther->velocity = pev->movedir * pev->speed;
	pevOther->velocity.z += m_flHeight;
	pev->nextthink = gpGlobals->time;
}


//=====================================
//
// trigger_cdaudio - starts/stops cd audio tracks
//
class CTriggerCDAudio : public CBaseTrigger
{
public:
	void Spawn( void );

	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Play( void );
	void Touch ( CBaseEntity *pOther );
};

LINK_ENTITY_TO_CLASS( trigger_cdaudio, CTriggerCDAudio );

//
// Changes tracks or stops CD when player touches
//
// !!!HACK - overloaded HEALTH to avoid adding new field
void CTriggerCDAudio :: Touch ( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
	{// only clients may trigger these events
		return;
	}

	Play();
}

void CTriggerCDAudio :: Spawn( void )
{
	InitTrigger();
}

void CTriggerCDAudio::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Play();
}

void PlayCDTrack( int iTrack, CBaseEntity *pActivator )
{
	if ( iTrack == 0 || iTrack < -1 || iTrack > 30 )
	{
		ALERT ( at_warning, "[CD Audio] Track %d out of range, must be 1-30 (or -1 to stop).\n", iTrack );
		return;
	}

	if ( pActivator && pActivator->IsPlayer() )
		MESSAGE_BEGIN( MSG_ONE, gmsgCdAudio, pActivator->pev->origin, pActivator->edict() );
	else
		MESSAGE_BEGIN( MSG_ALL, gmsgCdAudio );

	WRITE_BYTE( iTrack > 0 ? iTrack : -1 );
	MESSAGE_END();
	}


// only plays for ONE client, so only use in single play!
void CTriggerCDAudio :: Play( void )
{
	PlayCDTrack( (int)pev->health, NULL );
	
	SetTouch( NULL );
	UTIL_Remove( this );
}


// This plays a CD track when fired or when the player enters it's radius
class CTargetCDAudio : public CPointEntity
{
public:
	void			Spawn( void );
	void			KeyValue( KeyValueData *pkvd );

	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void			Think( void );
	void			Play( CBaseEntity *pActivator = NULL );

	bool			m_bActive;
};

LINK_ENTITY_TO_CLASS( target_cdaudio, CTargetCDAudio );

void CTargetCDAudio :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "radius"))
	{
		pev->scale = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CTargetCDAudio :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	m_bActive = false;
}

void CTargetCDAudio::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	switch ( useType )
	{
	case USE_OFF:
		m_bActive = false;
		break;

	case USE_ON:
		m_bActive = true;
		break;

	case USE_SET:
		pev->health = (int) value;
		m_bActive = true;
		break;

	case USE_TOGGLE:
		m_bActive = !m_bActive;
		break;

	case USE_KILL:
		SetThink( NULL );
		UTIL_Remove( this );
		break;

	default:
		break;
	}

	pev->nextthink = gpGlobals->time + 0.01;
}

void CTargetCDAudio::Think( void )
{
	if ( !m_bActive )
	{
		SetThink( NULL );
		return;
	}

	bool fPlayed = false;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer )
			continue;

		if ( pev->scale > 0 && pev->scale < ( pPlayer->pev->origin - pev->origin ).Length() )
			continue;

		PlayCDTrack( (int) pev->health, pPlayer );
		fPlayed = true;
	}

	if ( fPlayed )
	{
		m_bActive = false;
		SetThink( NULL );
		UTIL_Remove( this );
	}

	pev->nextthink = gpGlobals->time + 0.5;
}

void CTargetCDAudio::Play( CBaseEntity *pActivator )
{
	PlayCDTrack( (int)pev->health, pActivator );
	UTIL_Remove(this);
}

//=====================================

//
// trigger_hurt - hurts anything that touches it. if the trigger has a targetname, firing it will toggle state
//
//int gfToggleState = 0; // used to determine when all radiation trigger hurts have called 'RadiationThink'

void CTriggerHurt :: Spawn( void )
{
	InitTrigger();
	SetTouch ( &CTriggerHurt::HurtTouch );
	SetUse( pev->targetname ? &CTriggerHurt::ToggleUse : NULL );
	SetThink( NULL );
	m_flNextRadiationThink = gpGlobals->time + 0.1f;
		pev->nextthink = m_flNextRadiationThink;

	if ( FBitSet (pev->spawnflags, SF_TRIGGER_HURT_START_OFF) )// if flagged to Start Turned Off, make trigger nonsolid.
		pev->solid = SOLID_NOT;

	UTIL_SetOrigin( pev, pev->origin );		// Link into the list
}

void CTriggerHurt::Think( void )
{
	int count = 0;

	if ( pev->solid == SOLID_TRIGGER && FBitSet( pev->spawnflags, SF_TRIGGER_HURT_NON_MOVING ) )
	{
		CBaseEntity *entities[256];

		memset( entities, 0, sizeof( entities ) );
		count = UTIL_EntitiesInBox( entities, ARRAYSIZE( entities ), pev->absmin, pev->absmax, FL_MONSTER );

		for ( int i = 0; i < count; ++i )
		{
			CBaseEntity *entity = entities[i];

			if ( entity && entity->pev->solid != SOLID_NOT && entity->IsAlive() )
				HurtTouch( entity );
		}
	}

	if ( ( m_bitsDamageInflict & DMG_RADIATION ) && gpGlobals->time >= m_flNextRadiationThink )
	{
		RadiationThink();
		m_flNextRadiationThink = gpGlobals->time + RANDOM_FLOAT( 0.05f, 0.5f );
	}

	pev->nextthink = gpGlobals->time + ( count > 0 ? 0.01 : 0.05 );
}

// trigger hurt that causes radiation will do a radius
// check and set the player's geiger counter level
// according to distance from center of trigger

void CTriggerHurt :: RadiationThink( void )
{
	m_flNextRadiationThink = gpGlobals->time + 0.25f;

	Vector origin = pev->origin;
	Vector viewOffset = pev->view_ofs;

	pev->origin = (pev->absmin + pev->absmax) * 0.5;
	pev->view_ofs = pev->view_ofs * 0.0;

	edict_t *pentPlayer = FIND_CLIENT_IN_PVS(edict());

	pev->origin = origin;
	pev->view_ofs = viewOffset;

	// reset origin

	if (!FNullEnt(pentPlayer))
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)VARS(pentPlayer));

		if ( !pPlayer->IsObserver() )
		{
			Vector triggerCenter = (pev->absmin + pev->absmax) * 0.5;
			Vector playerCenter = ( pentPlayer->v.absmin + pentPlayer->v.absmax) * 0.5;
			float flRange = ( triggerCenter - playerCenter ).Length();

		// if player's current geiger counter range is larger
		// than range to this trigger hurt, reset player's
		// geiger counter range

		if (pPlayer->m_flgeigerRange >= flRange)
			pPlayer->m_flgeigerRange = flRange;
	}
	}
}

//
// ToggleUse - If this is the USE function for a trigger, its state will toggle every time it's fired
//
void CBaseTrigger :: ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (pev->solid == SOLID_NOT)
	{// if the trigger is off, turn it on
		pev->solid = SOLID_TRIGGER;
		
		// Force retouch
		gpGlobals->force_retouch++;
	}
	else
	{// turn the trigger off
		pev->solid = SOLID_NOT;
	}
	UTIL_SetOrigin( pev, pev->origin );
}

// When touched, a hurt trigger does DMG points of damage each half-second
void CBaseTrigger :: HurtTouch ( CBaseEntity *pOther )
{
	float fldmg;

	if ( !pOther->pev->takedamage )
		return;

	if ( (pev->spawnflags & SF_TRIGGER_HURT_CLIENTONLYTOUCH) && !pOther->IsPlayer() )
	{
		// this trigger is only allowed to touch clients, and this ain't a client.
		return;
	}

	if ( (pev->spawnflags & SF_TRIGGER_HURT_NO_CLIENTS) && pOther->IsPlayer() )
		return;

	if ( m_pInventoryRules->CanUseTrigger( pOther, true ) )
		return;

	// HACKHACK -- In multiplayer, players touch this based on packet receipt.
	// So the players who send packets later aren't always hurt.  Keep track of
	// how much time has passed and whether or not you've touched that player
	if ( g_pGameRules->IsMultiplayer() )
	{
		if ( pev->dmgtime > gpGlobals->time )
		{
			if ( gpGlobals->time != pev->pain_finished )
			{// too early to hurt again, and not same frame with a different entity
				if ( !pOther->IsPlayer() )
					return;

				// Mark this player as touched
				// BUGBUG - There can be only 32 players!
				pev->impulse |= 1 << (pOther->entindex() - 1);
				return;
			}

			// If I've already touched this entity (this time), then bail out
			if ( pev->impulse & ( 1 << (pOther->entindex() - 1) ) )
				return;
		}
		else if ( gpGlobals->time > pev->pain_finished )
		{
			// New clock, "un-touch" all players
			pev->impulse = 0;
		}
	}



	// If this is time_based damage (poison, radiation), override the pev->dmg with a 
	// default for the given damage type.  Monsters only take time-based damage
	// while touching the trigger.  Player continues taking damage for a while after
	// leaving the trigger

	fldmg = pev->dmg * 0.5;	// 0.5 seconds worth of damage, pev->dmg is damage/second


	// JAY: Cut this because it wasn't fully realized.  Damage is simpler now.
#if 0
	switch (m_bitsDamageInflict)
	{
	default: break;
	case DMG_POISON:		fldmg = POISON_DAMAGE/4; break;
	case DMG_NERVEGAS:		fldmg = NERVEGAS_DAMAGE/4; break;
	case DMG_RADIATION:		fldmg = RADIATION_DAMAGE/4; break;
	case DMG_PARALYZE:		fldmg = PARALYZE_DAMAGE/4; break; // UNDONE: cut this? should slow movement to 50%
	case DMG_ACID:			fldmg = ACID_DAMAGE/4; break;
	case DMG_SLOWBURN:		fldmg = SLOWBURN_DAMAGE/4; break;
	case DMG_SLOWFREEZE:	fldmg = SLOWFREEZE_DAMAGE/4; break;
	}
#endif

	if ( fldmg < 0 )
		pOther->TakeHealth( -fldmg, m_bitsDamageInflict );
	else
		pOther->TakeDamage( pev, pev, fldmg, m_bitsDamageInflict );

	// Store pain time so we can get all of the other entities on this frame
	pev->pain_finished = gpGlobals->time;

	// Apply damage every half second
	pev->dmgtime = gpGlobals->time + 0.5;// half second delay until this trigger can hurt toucher again

	pev->impulse |= 1 << (pOther->entindex() - 1);

  
	
	if ( pev->target )
	{
		// trigger has a target it wants to fire. 
		if ( pev->spawnflags & SF_TRIGGER_HURT_CLIENTONLYFIRE )
		{
			// if the toucher isn't a client, don't fire the target!
			if ( !pOther->IsPlayer() )
			{
				return;
			}
		}

		SUB_UseTargets( pOther, USE_TOGGLE, 0 );
		if ( pev->spawnflags & SF_TRIGGER_HURT_TARGETONCE )
			pev->target = 0;
	}
}


/*QUAKED trigger_multiple (.5 .5 .5) ? notouch
Variable sized repeatable trigger.  Must be targeted at one or more entities.
If "health" is set, the trigger must be killed to activate each time.
If "delay" is set, the trigger waits some time after activating before firing.
"wait" : Seconds between triggerings. (.2 default)
If notouch is set, the trigger is only fired by other entities, not by touching.
NOTOUCH has been obsoleted by trigger_relay!
sounds
1)      secret
2)      beep beep
3)      large switch
4)
NEW
if a trigger has a NETNAME, that NETNAME will become the TARGET of the triggered object.
*/
class CTriggerMultiple : public CBaseTrigger
{
public:
	void Spawn( void );
};

LINK_ENTITY_TO_CLASS( trigger_multiple, CTriggerMultiple );

void CTriggerMultiple :: Spawn( void )
{
	InitTrigger();
	SetTouch( &CTriggerMultiple::MultiTouch );
		}

/*QUAKED trigger_once (.5 .5 .5) ? notouch
Variable sized trigger. Triggers once, then removes itself.  You must set the key "target" to the name of another object in the level that has a matching
"targetname".  If "health" is set, the trigger must be killed to activate.
If notouch is set, the trigger is only fired by other entities, not by touching.
if "killtarget" is set, any objects that have a matching "target" will be removed when the trigger is fired.
if "angle" is set, the trigger will only fire when someone is facing the direction of the angle.  Use "360" for an angle of 0.
sounds
1)      secret
2)      beep beep
3)      large switch
4)
*/
class CTriggerOnce : public CTriggerMultiple
{
public:
	void Spawn( void );
};

LINK_ENTITY_TO_CLASS( trigger_once, CTriggerOnce );
void CTriggerOnce::Spawn( void )
{
	m_flWait = -1;
	
	CTriggerMultiple :: Spawn();
}

void CBaseTrigger :: MultiTouch( CBaseEntity *pOther )
{
	int flags = pev->spawnflags;
	entvars_t *pevToucher = pOther->pev;

	// Only touch clients, monsters, or pushables (depending on flags)
	if ( ( flags & SF_TRIGGER_EVERYTHING_ELSE ) && !( pevToucher->flags & ( FL_CLIENT | FL_MONSTER ) ) &&
	     !FClassnameIs( pevToucher, "func_pushable" ) )
	{
		ActivateMultiTrigger( pOther );
		return;
	}

	if ( !( flags & SF_TRIGGER_NOCLIENTS ) && (pevToucher->flags & FL_CLIENT) )
	{
		ActivateMultiTrigger( pOther );
		return;
	}

	if ( ( flags & SF_TRIGGER_ALLOWMONSTERS ) && (pevToucher->flags & FL_MONSTER) )
	{
		if ( m_fTinyMonsters )
		{
			ActivateMultiTrigger( pOther );
			return;
		}

		if ( pOther->Classify() != CLASS_INSECT &&
		     pOther->Classify() != CLASS_ALIEN_BIOWEAPON &&
		     pOther->Classify() != CLASS_PLAYER_BIOWEAPON &&
		     pevToucher->movetype != MOVETYPE_BOUNCE && pevToucher->movetype != MOVETYPE_BOUNCEMISSILE )
		{
			ActivateMultiTrigger( pOther );
			return;
		}
	}

	if ( ( flags & SF_TRIGGER_PUSHABLES) && FClassnameIs(pevToucher,"func_pushable") )
		ActivateMultiTrigger( pOther );
	}


//
// the trigger was just touched/killed/used
// self.enemy should be set to the activator so it can be held through a delay
// so wait for the delay time before firing
//
void CBaseTrigger :: ActivateMultiTrigger( CBaseEntity *pActivator )
{
	if ( pev->spawnflags & ( SF_TRIGGER_FIRE_ON_ENTER | SF_TRIGGER_FIRE_ON_EXIT ) )
	{
		if ( m_flWait != -1.0f )
			m_flWait = 0;

		for ( CTriggerActivator *entry = m_pActivators; entry; entry = entry->m_pNext )
		{
			if ( (CBaseEntity *) entry->m_hEntity == pActivator )
				return;
		}

		CTriggerActivator *entry = new CTriggerActivator;

		entry->m_hEntity = pActivator;
		entry->m_pNext = m_pActivators;
		m_pActivators = entry;
		SetThink( &CBaseTrigger::MultiWaitOver );
		pev->nextthink = gpGlobals->time;

		if ( !( pev->spawnflags & SF_TRIGGER_FIRE_ON_ENTER ) )
			return;
	}

	if (pev->nextthink > gpGlobals->time)
		return;         // still waiting for reset time

	if (!UTIL_IsMasterTriggered(m_sMaster,pActivator))
		return;

	if ( !FireMultiTrigger( pActivator ))
			return;

	if (m_flWait >= 0)
	{
		SetThink( &CBaseTrigger::MultiWaitOver );
		pev->nextthink = m_flWait + gpGlobals->time;
	}
	else
	{
		// we can't just remove (self) here, because this is a touch function
		// called while C code is looping through area links...
		SetTouch( NULL );
		pev->nextthink = gpGlobals->time + 0.1;
		SetThink(  &CBaseTrigger::SUB_Remove );
	}
}

bool CBaseTrigger::FireMultiTrigger( CBaseEntity *pActivator )
{
	// AdamR: Inventory filtering rules
	if ( !m_pInventoryRules->CanUseTrigger(pActivator))
	{
		pev->nextthink = m_flWait + gpGlobals->time;
		return false;
	}

	if ( FClassnameIs( pev, "trigger_secret" ) )
	{
		if ( !pev->enemy || !FClassnameIs( pev->enemy, "player" ) )
			return false;

		gpGlobals->found_secrets++;
	}

	if ( !FStringNull( pev->noise ) )
		EMIT_SOUND_DYN( edict(), CHAN_VOICE, STRING( pev->noise ), 1, ATTN_NORM,
		                0, PITCH_NORM, 0, false, g_vecZero );

	m_hActivator = NULL;

	if ( pActivator && pActivator->pev )
	{
		edict_t *entity = pActivator->pev->pContainingEntity;

		if ( entity && !entity->free )
			m_hActivator.Set( entity );
	}

	edict_t *activatorEdict = m_hActivator.Get();
	CBaseEntity *activator = NULL;

	if ( activatorEdict && !activatorEdict->free )
		activator = CBaseEntity::Instance( activatorEdict );

	SUB_UseTargets( activator, USE_TOGGLE, 0 );

	if ( pev->message && pActivator->IsPlayer() )
		UTIL_ShowMessage( STRING( pev->message ), pActivator );

	if ( FClassnameIs( pev, "trigger_once" ) )
		UTIL_Remove( this );

	return true;
}

// the wait time has passed, so set back up for another activation
void CBaseTrigger :: MultiWaitOver( void )
{
	if ( pev->spawnflags & ( SF_TRIGGER_FIRE_ON_ENTER | SF_TRIGGER_FIRE_ON_EXIT ) )
	{
		CTriggerActivator *previous = m_pActivators;
		bool removeHead = false;

		if ( previous )
		{
			CBaseEntity *entity = previous->m_hEntity;

			if ( !entity || ( !entity->Intersects( this ) && !Intersects( previous->m_hEntity ) ) )
			{
				if ( pev->spawnflags & SF_TRIGGER_FIRE_ON_EXIT )
				{
					CBaseEntity *activator = previous->m_hEntity;

					FireMultiTrigger( activator ? activator : this );
				}

				removeHead = true;
			}

			CTriggerActivator *current = previous->m_pNext;

			while ( current )
			{
				entity = current->m_hEntity;

				if ( entity && ( entity->Intersects( this ) || Intersects( current->m_hEntity ) ) )
				{
					previous = current;
					current = current->m_pNext;
				}
				else
				{
					if ( pev->spawnflags & SF_TRIGGER_FIRE_ON_EXIT )
					{
						CBaseEntity *activator = current->m_hEntity;

						FireMultiTrigger( activator ? activator : this );
					}

					previous->m_pNext = current->m_pNext;
					delete current;
					current = previous->m_pNext;
				}
			}

			if ( removeHead )
			{
				CTriggerActivator *head = m_pActivators;
				CTriggerActivator *next = head->m_pNext;

				delete head;
				m_pActivators = next;
			}
		}

		if ( m_pActivators )
		{
			pev->nextthink = m_flWait + gpGlobals->time;
			return;
		}
	}

	SetThink( NULL );
}

// ========================= COUNTING TRIGGER =====================================

//
// GLOBALS ASSUMED SET:  g_eoActivator
//
void CounterMessage( CBaseEntity *pActivator, const char *pszMessage )
{
	ALERT( at_console, "%s", pszMessage );

	if ( pActivator->IsPlayer() && pActivator->IsNetClient() )
		ClientPrint( pActivator->pev, HUD_PRINTNOTIFY, pszMessage );
	else
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, pszMessage );
}

void CBaseTrigger::CounterUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_cTriggersLeft--;
	m_hActivator = pActivator;

	if (m_cTriggersLeft < 0)
		return;

	BOOL fTellActivator =
		(m_hActivator != 0) &&
		FClassnameIs(m_hActivator->pev, "player") &&
		!FBitSet(pev->spawnflags, SPAWNFLAG_NOMESSAGE);
	if (m_cTriggersLeft != 0)
	{
		if (fTellActivator)
		{
			char szMessage[128];

			// UNDONE: I don't think we want these Quakesque messages
			switch (m_cTriggersLeft)
			{
			case 1:
			case 2:
			case 3:
				sprintf( szMessage, "Only %d more to go...\n", m_cTriggersLeft );
				break;
			default:
				sprintf( szMessage, "There are %d to go...\n", m_cTriggersLeft );
				break;
			}

			CounterMessage( pActivator, szMessage );
		}
		return;
	}

	// !!!UNDONE: I don't think we want these Quakesque messages
	if (fTellActivator)
		CounterMessage( pActivator, "Sequence completed!\n" );

	ActivateMultiTrigger( m_hActivator );
}


/*QUAKED trigger_counter (.5 .5 .5) ? nomessage
Acts as an intermediary for an action that takes multiple inputs.
If nomessage is not set, it will print "1 more.. " etc when triggered and
"sequence complete" when finished.  After the counter has been triggered "cTriggersLeft"
times (default 2), it will fire all of it's targets and remove itself.
*/
class CTriggerCounter : public CBaseTrigger
{
public:
	void Spawn( void );
};
LINK_ENTITY_TO_CLASS( trigger_counter, CTriggerCounter );

void CTriggerCounter :: Spawn( void )
{
	// By making the flWait be -1, this counter-trigger will disappear after it's activated
	// (but of course it needs cTriggersLeft "uses" before that happens).
	m_flWait = -1;

	if (m_cTriggersLeft == 0)
		m_cTriggersLeft = 2;
	SetUse( &CTriggerCounter::CounterUse );
}

// ====================== TRIGGER_CHANGELEVEL ================================

class CTriggerVolume : public CPointEntity	// Derive from point entity so this doesn't move across levels
{
public:
	void		Spawn( void );
};

LINK_ENTITY_TO_CLASS( trigger_transition, CTriggerVolume );

// Define space that travels across a level transition
void CTriggerVolume :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	SET_MODEL(ENT(pev), STRING(pev->model));    // set size and link into world
	pev->model = iStringNull;
	pev->modelindex = 0;
}


// Fires a target after level transition and then dies
class CFireAndDie : public CBaseDelay
{
public:
	void Spawn( void );
	void Precache( void );
	void Think( void );
	int ObjectCaps( void ) { return CBaseDelay::ObjectCaps() | FCAP_FORCE_TRANSITION; }	// Always go across transitions
};
LINK_ENTITY_TO_CLASS( fireanddie, CFireAndDie );

void CFireAndDie::Spawn( void )
{
	pev->classname = MAKE_STRING("fireanddie");
	// Don't call Precache() - it should be called on restore
}


void CFireAndDie::Precache( void )
{
	// This gets called on restore
	pev->nextthink = gpGlobals->time + m_flDelay;
}


void CFireAndDie::Think( void )
{
	SUB_UseTargets( this, USE_TOGGLE, 0 );
	UTIL_Remove( this );
}


#define SF_CHANGELEVEL_USEONLY		0x0002
#define SF_CHANGELEVEL_IGNORE_NEXTMAP	0x0004
class CChangeLevel : public CBaseTrigger
{
public:
	bool IsEnabled( void ) { return pev->solid != SOLID_BSP; }
	void Spawn( void );
	void EXPORT PostInit( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT UseChangeLevel ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT TriggerChangeLevel( void );
	void EXPORT ExecuteChangeLevel( void );
	void EXPORT TouchChangeLevel( CBaseEntity *pOther );
	void ChangeLevelNow( CBaseEntity *pActivator );

	static edict_t *FindLandmark( const char *pLandmarkName );
	static int ChangeList( LEVELLIST *pLevelList, int maxList );
	static int AddTransitionToList( LEVELLIST *pLevelList, int listCount, const char *pMapName, const char *pLandmarkName, edict_t *pentLandmark );
	static int InTransitionVolume( CBaseEntity *pEntity, char *pVolumeName );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	char m_szMapName[cchMapNameMost];		// trigger_changelevel only:  next map
	char m_szLandmarkName[cchMapNameMost];		// trigger_changelevel only:  landmark on next map
	int		m_changeTarget;
	float	m_changeTargetDelay;
	bool	m_fKeepInventory;
	float	m_flPercentOfPlayers;
};
LINK_ENTITY_TO_CLASS( trigger_changelevel, CChangeLevel );

// Global Savedata for changelevel trigger
TYPEDESCRIPTION	CChangeLevel::m_SaveData[] = 
{
	DEFINE_ARRAY( CChangeLevel, m_szMapName, FIELD_CHARACTER, cchMapNameMost ),
	DEFINE_ARRAY( CChangeLevel, m_szLandmarkName, FIELD_CHARACTER, cchMapNameMost ),
	DEFINE_FIELD( CChangeLevel, m_changeTarget, FIELD_STRING ),
	DEFINE_FIELD( CChangeLevel, m_changeTargetDelay, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE(CChangeLevel,CBaseTrigger);

//
// Cache user-entity-field values until spawn is called.
//

void CChangeLevel :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "map"))
	{
		if (strlen(pkvd->szValue) >= cchMapNameMost)
			ALERT( at_error, "Map name '%s' too long (32 chars)\n", pkvd->szValue );
		strcpy(m_szMapName, pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "landmark"))
	{
		if (strlen(pkvd->szValue) >= cchMapNameMost)
			ALERT( at_error, "Landmark name '%s' too long (32 chars)\n", pkvd->szValue );
		strcpy(m_szLandmarkName, pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "changetarget"))
	{
		m_changeTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "changedelay"))
	{
		m_changeTargetDelay = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "keep_inventory" ) )
	{
		m_fKeepInventory = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( !stricmp( pkvd->szKeyName, "percent_of_players" ) )
	{
		m_flPercentOfPlayers = V_max( 0.0f, V_min( (float) atof( pkvd->szValue ), 1.0f ) );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseTrigger::KeyValue( pkvd );
}


/*QUAKED trigger_changelevel (0.5 0.5 0.5) ? NO_INTERMISSION
When the player touches this, he gets sent to the map listed in the "map" variable.  Unless the NO_INTERMISSION flag is set, the view will go to the info_intermission spot and display stats.
*/

void CChangeLevel :: Spawn( void )
{
	if ( FStrEq( m_szMapName, "" ) )
		ALERT( at_console, "A trigger_changelevel doesn't have a map (X%f Y%f Z%f)",
		       pev->origin.x, pev->origin.y, pev->origin.z );

	if (!FStringNull ( pev->targetname ) )
		SetUse ( &CChangeLevel::UseChangeLevel );

	if ( FStrEq( m_szMapName, CVAR_GET_STRING( "sv_notransition" ) ) )
		pev->solid = SOLID_BSP;

	InitTrigger();
	if ( !(pev->spawnflags & SF_CHANGELEVEL_USEONLY) )
		SetTouch( &CChangeLevel::TouchChangeLevel );

	SetThink( &CChangeLevel::PostInit );
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CChangeLevel::PostInit( void )
{
	// transports to illegal map, make intouchable
	if ( FStrEq( m_szMapName, CVAR_GET_STRING( "sv_notransition" ) ) )
	{
		ALERT( at_console, "Map change to %s is illegal, ignoring\n", CVAR_GET_STRING( "sv_notransition" ) );

		pev->solid = SOLID_BSP;
		pev->movetype = MOVETYPE_PUSH;

		SetTouch( NULL );
		SetThink( NULL );
		UTIL_SetOrigin( pev, pev->origin );
	}
}

void CChangeLevel :: ExecuteChangeLevel( void )
{
	MESSAGE_BEGIN( MSG_ALL, SVC_CDTRACK );
		WRITE_BYTE( 3 );
		WRITE_BYTE( 3 );
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ALL, SVC_INTERMISSION);
	MESSAGE_END();
}


FILE_GLOBAL char st_szNextMap[cchMapNameMost];
FILE_GLOBAL char st_szNextSpot[cchMapNameMost];

edict_t *CChangeLevel :: FindLandmark( const char *pLandmarkName )
{
	edict_t	*pentLandmark;

	pentLandmark = FIND_ENTITY_BY_STRING( NULL, "targetname", pLandmarkName );
	while ( !FNullEnt( pentLandmark ) )
	{
		// Found the landmark
		if ( FClassnameIs( pentLandmark, "info_landmark" ) )
			return pentLandmark;
		else
			pentLandmark = FIND_ENTITY_BY_STRING( pentLandmark, "targetname", pLandmarkName );
	}
	ALERT( at_error, "Can't find landmark %s\n", pLandmarkName );
	return NULL;
}


//=========================================================
// CChangeLevel :: Use - allows level transitions to be 
// triggered by buttons, etc.
//
//=========================================================
void CChangeLevel :: UseChangeLevel ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	ChangeLevelNow( pActivator );
}

void CChangeLevel :: ChangeLevelNow( CBaseEntity *pActivator )
{
	ASSERT(!FStrEq(m_szMapName, ""));

	if ( pev->solid == SOLID_BSP )
		return;

	// Some people are firing these multiple times in a frame, disable
	if ( gpGlobals->time == pev->dmgtime )
		return;

	pev->dmgtime = gpGlobals->time;

	const char *pszNextMap = CVAR_GET_STRING( "mp_nextmap" );

	if ( !( pev->spawnflags & SF_CHANGELEVEL_IGNORE_NEXTMAP ) && IS_MAP_VALID( (char *) pszNextMap )
		&& *pszNextMap && strlen( pszNextMap ) <= cchMapNameMost )
		strncpy( st_szNextMap, pszNextMap, sizeof( st_szNextMap ) - 1 );
	else
		strncpy( st_szNextMap, m_szMapName, sizeof( st_szNextMap ) - 1 );

	if ( !IS_MAP_VALID( st_szNextMap ) )
	{
		ALERT( at_console, "CHANGE LEVEL: Map %s not found! Aborting.\n", st_szNextMap );
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "Next map not found:\n%s\n", st_szNextMap );
		return;
	}

	if ( pActivator && pActivator->IsPlayer() && !InTransitionVolume( pActivator, m_szLandmarkName ) )
	{
		ALERT( at_aiconsole, "Player '%s' isn't in the transition volume %s, aborting\n", STRING( pActivator->pev->netname ), m_szLandmarkName );
		return;
	}

	// Create an entity to fire the changetarget
	if ( m_changeTarget )
	{
		CFireAndDie *pFireAndDie = GetClassPtr( (CFireAndDie *)NULL );

		if ( pFireAndDie )
		{
			// Set target and delay
			pFireAndDie->pev->target = m_changeTarget;
			pFireAndDie->m_flDelay = m_changeTargetDelay;

			if ( pActivator && pActivator->IsPlayer() )
				pFireAndDie->pev->origin = pActivator->pev->origin;

			// Call spawn
			DispatchSpawn( pFireAndDie->edict() );
		}
	}

	m_hActivator = pActivator;
	SUB_UseTargets( pActivator, USE_TOGGLE, 0 );
	st_szNextSpot[0] = 0;	// Init landmark to NULL

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		ALERT( at_console, "ChangeLevel: Set Landmark Offsets\n" );
		UTIL_PlayerByIndex( i );
	}

	delete g_pGamePersistentInventory;
	g_pGamePersistentInventory = NULL;

	if ( m_fKeepInventory )
	{
		CGamePersistentInventory *inventory = new CGamePersistentInventory( st_szNextMap );

		inventory->CapturePlayers();
		g_pGamePersistentInventory = inventory;
	}

	CVAR_SET_FLOAT( "mp_keep_globalstate", 1 );

	ALERT( at_console, "CHANGE LEVEL: %s %s\n", st_szNextMap, st_szNextSpot );
	UTIL_LogPrintf( "map<%s> ended, going to new level: %s\n", STRING( gpGlobals->mapname ), st_szNextMap );
	UTIL_ClientPrintAll( HUD_PRINTCENTER, "Now changing level to:\n%s\n", st_szNextMap );

	CHANGE_LEVEL( st_szNextMap, NULL );
}

//
// GLOBALS ASSUMED SET:  st_szNextMap
//
void CChangeLevel :: TouchChangeLevel( CBaseEntity *pOther )
{
	if ( !pOther || !m_pInventoryRules->CanUseTrigger( pOther ) || !pOther->IsPlayer() )
		return;

	if ( m_flPercentOfPlayers > 0 )
	{
		int iInside = 0;
		int iOutside = 0;

		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBasePlayer *pPlayer = (CBasePlayer *) UTIL_PlayerByIndex( i );

			if ( !pPlayer || !pPlayer->IsInWorld() || !pPlayer->IsConnected() || !pPlayer->IsAlive() )
				continue;

			int iHull = human_hull;

			if ( pPlayer->pev->flags & FL_DUCKING )
				iHull = head_hull;

			TraceResult tr;

			TRACE_MODEL( pPlayer->pev->origin, pPlayer->pev->origin, iHull, edict(), &tr );

			if ( tr.fStartSolid )
				iInside++;
			else
				iOutside++;
		}

		if ( iInside + iOutside != 0 && m_flPercentOfPlayers > (float) iInside / (float) ( iInside + iOutside ) )
			return;
	}

	ChangeLevelNow( pOther );
}


// Add a transition to the list, but ignore duplicates 
// (a designer may have placed multiple trigger_changelevels with the same landmark)
int CChangeLevel::AddTransitionToList( LEVELLIST *pLevelList, int listCount, const char *pMapName, const char *pLandmarkName, edict_t *pentLandmark )
{
	int i;

	if ( !pLevelList || !pMapName || !pLandmarkName || !pentLandmark )
		return 0;

	for ( i = 0; i < listCount; i++ )
	{
		if ( pLevelList[i].pentLandmark == pentLandmark && strcmp( pLevelList[i].mapName, pMapName ) == 0 )
			return 0;
	}
	strcpy( pLevelList[listCount].mapName, pMapName );
	strcpy( pLevelList[listCount].landmarkName, pLandmarkName );
	pLevelList[listCount].pentLandmark = pentLandmark;
	pLevelList[listCount].vecLandmarkOrigin = VARS(pentLandmark)->origin;

	return 1;
}

int BuildChangeList( LEVELLIST *pLevelList, int maxList )
{
	return CChangeLevel::ChangeList( pLevelList, maxList );
}


int CChangeLevel::InTransitionVolume( CBaseEntity *pEntity, char *pVolumeName )
{
	edict_t	*pentVolume;


	if ( pEntity->ObjectCaps() & FCAP_FORCE_TRANSITION )
		return 1;

	// If you're following another entity, follow it through the transition (weapons follow the player)
	if ( pEntity->pev->movetype == MOVETYPE_FOLLOW )
	{
		if ( pEntity->pev->aiment != NULL )
			pEntity = CBaseEntity::Instance( pEntity->pev->aiment );
	}

	int inVolume = 1;	// Unless we find a trigger_transition, everything is in the volume

	pentVolume = FIND_ENTITY_BY_TARGETNAME( NULL, pVolumeName );
	while ( !FNullEnt( pentVolume ) )
	{
		CBaseEntity *pVolume = CBaseEntity::Instance( pentVolume );
		
		if ( pVolume && FClassnameIs( pVolume->pev, "trigger_transition" ) )
		{
			if ( pVolume->Intersects( pEntity ) )	// It touches one, it's in the volume
				return 1;
			else
				inVolume = 0;	// Found a trigger_transition, but I don't intersect it -- if I don't find another, don't go!
		}
		pentVolume = FIND_ENTITY_BY_TARGETNAME( pentVolume, pVolumeName );
	}

	return inVolume;
}


// We can only ever move 512 entities across a transition
#define MAX_ENTITY 512

// This has grown into a complicated beast
// Can we make this more elegant?
// This builds the list of all transitions on this level and which entities are in their PVS's and can / should
// be moved across.
int CChangeLevel::ChangeList( LEVELLIST *pLevelList, int maxList )
{
	edict_t	*pentChangelevel;

	// Entities never travel across a level change, but every changelevel still gets its private data
	pentChangelevel = FIND_ENTITY_BY_STRING( NULL, "classname", "trigger_changelevel" );

	while ( !FNullEnt( pentChangelevel ) )
	{
		GetClassPtr( (CChangeLevel *)VARS(pentChangelevel) );

		pentChangelevel = FIND_ENTITY_BY_STRING( pentChangelevel, "classname", "trigger_changelevel" );
	}

	return 0;
}

/*
go to the next level for deathmatch
only called if a time or frag limit has expired
*/
void NextLevel( void )
{
	edict_t* pent;
	CChangeLevel *pChange;
	
	// find a trigger_changelevel
	pent = FIND_ENTITY_BY_CLASSNAME(NULL, "trigger_changelevel");
	
	// go back to start if no trigger_changelevel
	if (FNullEnt(pent))
	{
		gpGlobals->mapname = ALLOC_STRING("start");
		pChange = GetClassPtr( (CChangeLevel *)NULL );
		strcpy(pChange->m_szMapName, "start");
	}
	else
		pChange = GetClassPtr( (CChangeLevel *)VARS(pent));
	
	strcpy(st_szNextMap, pChange->m_szMapName);
	g_fGameOver = TRUE;
	
	if (pChange->pev->nextthink < gpGlobals->time)
	{
		pChange->SetThink( &CChangeLevel::ExecuteChangeLevel );
		pChange->pev->nextthink = gpGlobals->time + 0.1;
	}
}


// ============================== LADDER =======================================

class CLadder : public CBaseTrigger
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );
	void Precache( void );
	void EXPORT ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	bool m_fActive;
};
LINK_ENTITY_TO_CLASS( func_ladder, CLadder );


void CLadder :: KeyValue( KeyValueData *pkvd )
{
	CBaseTrigger::KeyValue( pkvd );
}


//=========================================================
// func_ladder - makes an area vertically negotiable
//=========================================================
void CLadder :: Precache( void )
{
	// Do all of this in here because we need to 'convert' old saved games
	pev->solid = SOLID_NOT;
	pev->skin = CONTENTS_LADDER;
	if ( CVAR_GET_FLOAT("showtriggers") == 0 )
	{
		pev->rendermode = kRenderTransTexture;
		pev->renderamt = 0;
	}
	pev->effects &= ~EF_NODRAW;
}


void CLadder :: Spawn( void )
{
	Precache();

	SET_MODEL(ENT(pev), STRING(pev->model));    // set size and link into world
	pev->movetype = MOVETYPE_PUSH;
	SetUse( &CLadder::ToggleUse );
}

// The contents only change for the engine when the model is set again
void CLadder::ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_fActive ) )
		return;

	m_fActive = !m_fActive;
	pev->skin = m_fActive ? CONTENTS_LADDER : CONTENTS_EMPTY;

	SET_MODEL( edict(), STRING( pev->model ) );
}


// ========================== A TRIGGER THAT PUSHES YOU ===============================

class CTriggerPush : public CBaseTrigger
{
public:
	void OnCreate( void )
	{
		CBaseTrigger::OnCreate();
		m_iPushMode = TRIGGER_PUSH_MODE_BASEVELOCITY;
	}

	void OnSetOriginByMap( void )
	{
		m_vecSavedAngles = pev->angles;
		SetMovedir( pev );
		pev->angles = m_vecSavedAngles;
	}

	Vector m_vecSavedAngles;
	int m_iPushMode;

	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Touch( CBaseEntity *pOther );
};
LINK_ENTITY_TO_CLASS( trigger_push, CTriggerPush );


void CTriggerPush :: KeyValue( KeyValueData *pkvd )
{
	if ( !stricmp( pkvd->szKeyName, "pushmode" ) )
	{
		m_iPushMode = V_max( 0, V_min( atoi( pkvd->szValue ), TRIGGER_PUSH_MODE_SET ) );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseTrigger::KeyValue( pkvd );
}


/*QUAKED trigger_push (.5 .5 .5) ? TRIG_PUSH_ONCE
Pushes the player
*/

void CTriggerPush :: Spawn( )
{
	if ( pev->angles == g_vecZero )
		pev->angles.y = 360;

	m_vecSavedAngles = pev->angles;
	InitTrigger();
	pev->angles = m_vecSavedAngles;

	if (pev->speed == 0)
		pev->speed = 100;

	if ( FBitSet (pev->spawnflags, SF_TRIGGER_PUSH_START_OFF) )// if flagged to Start Turned Off, make trigger nonsolid.
		pev->solid = SOLID_NOT;

	if ( FBitSet( pev->spawnflags, SF_TRIG_PUSH_LIMIT ) && m_iPushMode == TRIGGER_PUSH_MODE_BASEVELOCITY )
		m_iPushMode = TRIGGER_PUSH_MODE_LIMIT;

	SetUse( &CTriggerPush::ToggleUse );

	UTIL_SetOrigin( pev, pev->origin );		// Link into the list
}


void CTriggerPush :: Touch( CBaseEntity *pOther )
{
	entvars_t* pevToucher = pOther->pev;

	if ( pevToucher->solid == SOLID_NOT || ( pevToucher->solid == SOLID_BSP && pevToucher->movetype != MOVETYPE_PUSHSTEP ) )
		return;

	// UNDONE: Is there a better way than health to detect things that have physics? (clients/monsters)
	switch( pevToucher->movetype )
	{
	case MOVETYPE_NONE:
	case MOVETYPE_PUSH:
	case MOVETYPE_NOCLIP:
	case MOVETYPE_FOLLOW:
		return;
	}

	if ( FBitSet( pev->spawnflags, SF_TRIG_PUSH_NO_MONSTERS ) && ( pevToucher->flags & FL_MONSTER ) )
		return;

	if ( FBitSet( pev->spawnflags, SF_TRIG_PUSH_NO_CLIENTS ) && ( pevToucher->flags & FL_CLIENT ) )
		return;

	if ( !FBitSet( pev->spawnflags, SF_TRIG_PUSH_ALLOW_PUSHABLES )
		&& ( FClassnameIs( pevToucher, "func_pushable" ) || FClassnameIs( pevToucher, "func_breakable" ) ) )
		return;

	if ( FClassnameIs( pevToucher, "grappletongue" ) || m_pInventoryRules->CanUseTrigger( pOther, true ) )
		return;

	Vector vecPush = ( pev->speed * pev->movedir );
	float flMissingSpeed = fabs( pev->speed ) - DotProduct( vecPush.Normalize(), pevToucher->velocity );

	if ( vecPush.z > 0 && ( pevToucher->flags & FL_ONGROUND ) )
	{
		pevToucher->origin.z += 0.1;
		SET_ORIGIN( ENT( pevToucher ), pevToucher->origin );
		pevToucher->flags &= ~FL_ONGROUND;
	}

	// Instant trigger, just transfer velocity and remove
	if (FBitSet(pev->spawnflags, SF_TRIG_PUSH_ONCE))
	{
		if ( m_iPushMode != TRIGGER_PUSH_MODE_BASEVELOCITY )
			pevToucher->velocity = vecPush;
		else
			pevToucher->velocity = pevToucher->velocity + vecPush;

		UTIL_Remove( this );
	}
	else if ( m_iPushMode == TRIGGER_PUSH_MODE_LIMIT )
	{
		if ( flMissingSpeed > 0 )
			pevToucher->velocity = pevToucher->velocity + vecPush.Normalize() * flMissingSpeed;
	}
	else if ( m_iPushMode == TRIGGER_PUSH_MODE_SET )
	{
		pevToucher->velocity = vecPush;
	}
	else
	{	// Push field, transfer to base velocity
		if ( pevToucher->flags & FL_BASEVELOCITY )
			vecPush = vecPush + pevToucher->basevelocity;

		pevToucher->basevelocity = vecPush;
		pevToucher->flags |= FL_BASEVELOCITY;
//		ALERT( at_console, "Vel %f, base %f\n", pevToucher->velocity.z, pevToucher->basevelocity.z );
	}
}


//======================================
// teleport trigger
//
//

void CBaseTrigger :: TeleportTouch( CBaseEntity *pOther )
{
	entvars_t* pevToucher = pOther->pev;
	edict_t	*pentTarget = NULL;

	// Only teleport monsters or clients
	if ( !FBitSet( pevToucher->flags, FL_CLIENT|FL_MONSTER ) )
		return;
    
	if (!UTIL_IsMasterTriggered(m_sMaster, pOther))
		return;
 	
	if ( !( pev->spawnflags & SF_TRIGGER_ALLOWMONSTERS ) )
	{// no monsters allowed!
		if ( FBitSet( pevToucher->flags, FL_MONSTER ) )
		{
			return;
		}
	}

	if ( ( pev->spawnflags & SF_TRIGGER_NOCLIENTS ) )
	{// no clients allowed
		if ( pOther->IsPlayer() )
		{
			return;
		}
	}
	
	pentTarget = FIND_ENTITY_BY_TARGETNAME( pentTarget, STRING(pev->target) );
	if (FNullEnt(pentTarget))
	   return;	
	
	Vector tmp = VARS( pentTarget )->origin;

	if ( pOther->IsPlayer() )
	{
		tmp.z -= pOther->pev->mins.z;// make origin adjustments in case the teleportee is a player. (origin in center, not at feet)
	}

	tmp.z++;

	pevToucher->flags &= ~FL_ONGROUND;
	
	UTIL_SetOrigin( pevToucher, tmp );

	pevToucher->angles = pentTarget->v.angles;

	if ( pOther->IsPlayer() )
	{
		pevToucher->v_angle = pentTarget->v.angles;
	}

	pevToucher->fixangle = TRUE;
	pevToucher->velocity = pevToucher->basevelocity = g_vecZero;
}


class CTriggerTeleport : public CBaseTrigger
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Touch( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	float	m_flTeleportCooldown;
	bool	m_fCustomCooldown;
	bool	m_fActive;
	bool	m_fTeleportIfBlocked;
};

LINK_ENTITY_TO_CLASS( trigger_teleport, CTriggerTeleport );

void CTriggerTeleport::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "teleport_cooldown" ) )
	{
		m_flTeleportCooldown = atof( pkvd->szValue );
		m_fCustomCooldown = true;
	}
	else if ( FStrEq( pkvd->szKeyName, "teleport_start_inactive" ) )
	{
		if ( atoi( pkvd->szValue ) > 0 )
			m_iExtraSpawnFlags |= SF_TELEPORT_START_OFF;
	}
	else if ( FStrEq( pkvd->szKeyName, "teleport_ignore_delay" ) )
	{
		if ( atoi( pkvd->szValue ) > 0 )
			m_iExtraSpawnFlags |= SF_TELEPORT_IGNORE_DELAYS;
	}
	else if ( FStrEq( pkvd->szKeyName, "teleport_if_blocked" ) )
	{
		if ( atoi( pkvd->szValue ) > 0 )
			m_fTeleportIfBlocked = true;
	}
	else
	{
		CBaseTrigger::KeyValue( pkvd );
		return;
	}

	pkvd->fHandled = TRUE;
}

void CTriggerTeleport :: Spawn( void )
{
	InitTrigger();
	m_fActive = !( pev->spawnflags & SF_TELEPORT_START_OFF );
	SetTouch( NULL );
	SetUse( NULL );
}

void CTriggerTeleport::Touch( CBaseEntity *pOther )
{
	if ( !UTIL_IsMasterTriggered( m_sMaster, pOther ) )
		return;

	if ( !m_fActive )
		return;

	if ( !( pev->spawnflags & SF_TELEPORT_IGNORE_DELAYS ) && m_flNextTriggerTime > gpGlobals->time )
		return;

	entvars_t *other = pOther->pev;

	// AdamR: Inventory filtering rules
	if ( !m_pInventoryRules->CanUseTrigger(pOther))
		return;

	int flags = pev->spawnflags;
	int otherFlags = other->flags;

	if ( !( flags & SF_TRIGGER_EVERYTHING_ELSE ) || ( otherFlags & ( FL_CLIENT | FL_MONSTER ) ) ||
	     FClassnameIs( other, "func_pushable" ) )
	{
		if ( ( ( flags & SF_TRIGGER_NOCLIENTS ) || !( otherFlags & FL_CLIENT ) ) &&
		     ( !( flags & SF_TRIGGER_ALLOWMONSTERS ) || !( otherFlags & FL_MONSTER ) ) )
		{
			if ( !( flags & SF_TRIGGER_PUSHABLES ) || !FClassnameIs( other, "func_pushable" ) )
				return;
		}
	}

	CBaseEntity *destination = NULL;

	if ( flags & SF_TELEPORT_RANDOM_DESTINATION )
	{
		CBaseEntity *destinations[32];
		int count = 0;

		while ( count < ARRAYSIZE( destinations ) )
		{
			destination = UTIL_FindEntityByTargetname( destination, STRING( pev->target ) );

			if ( !destination )
				break;

			destinations[count++] = destination;
		}

		if ( !count )
			return;

		destination = destinations[RANDOM_LONG( 0, count - 1 )];
	}
	else
	{
		destination = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );

		if ( !destination || !destination->pev )
			return;

		edict_t *entity = destination->pev->pContainingEntity;

		if ( !entity || entity->free || !OFFSET( entity ) )
			return;
	}

	int teleportFlags = pev->spawnflags | ( m_fTeleportIfBlocked ? SF_TELEPORT_CHECK_BLOCKED : 0 );

	if ( UTIL_TeleportEntity( pOther, destination, teleportFlags, pev->origin ) )
		m_flNextTriggerTime = gpGlobals->time + ( m_fCustomCooldown ? m_flTeleportCooldown : 1.0f );
}

void CTriggerTeleport::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	switch ( useType )
	{
		case USE_OFF:
			m_fActive = false;
			break;

		case USE_ON:
			m_fActive = true;
			break;

		case USE_SET:
			m_fActive = value >= 1.0f;
			break;

		case USE_KILL:
			UTIL_Remove( this );
			break;

		default:
			m_fActive = !m_fActive;
			break;
	}
}

class CTriggerSave : public CBaseTrigger
{
public:
	void Spawn( void );
	void EXPORT SaveTouch( CBaseEntity *pOther );
};
LINK_ENTITY_TO_CLASS( trigger_autosave, CTriggerSave );

void CTriggerSave::Spawn( void )
{
	if ( g_pGameRules->IsDeathmatch() )
	{
		REMOVE_ENTITY( ENT(pev) );
		return;
	}

	InitTrigger();
	SetTouch( &CTriggerSave::SaveTouch );
}

void CTriggerSave::SaveTouch( CBaseEntity *pOther )
{
	if ( !UTIL_IsMasterTriggered( m_sMaster, pOther ) )
		return;

	// Only save on clients
	if ( !pOther->IsPlayer() )
		return;
    
	SetTouch( NULL );
	UTIL_Remove( this );
	SERVER_COMMAND( "autosave\n" );
}

#define SF_ENDSECTION_USEONLY		0x0001

class CTriggerEndSection : public CBaseTrigger
{
public:
	void Spawn( void );
	void EXPORT EndSectionTouch( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT EndSectionUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};
LINK_ENTITY_TO_CLASS( trigger_endsection, CTriggerEndSection );


void CTriggerEndSection::EndSectionUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Only save on clients
	if ( pActivator && !pActivator->IsNetClient() )
		return;
    
	SetUse( NULL );

	if ( pev->message )
	{
		g_engfuncs.pfnEndSection(STRING(pev->message));
	}
	UTIL_Remove( this );
}

void CTriggerEndSection::Spawn( void )
{
	if ( g_pGameRules->IsDeathmatch() )
	{
		REMOVE_ENTITY( ENT(pev) );
		return;
	}

	InitTrigger();

	SetUse ( &CTriggerEndSection::EndSectionUse );
	// If it is a "use only" trigger, then don't set the touch function.
	if ( ! (pev->spawnflags & SF_ENDSECTION_USEONLY) )
		SetTouch( &CTriggerEndSection::EndSectionTouch );
}

void CTriggerEndSection::EndSectionTouch( CBaseEntity *pOther )
{
	// Only save on clients
	if ( !pOther->IsNetClient() )
		return;
    
	SetTouch( NULL );

	if (pev->message)
	{
		g_engfuncs.pfnEndSection(STRING(pev->message));
	}
	UTIL_Remove( this );
}

void CTriggerEndSection :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "section"))
	{
//		m_iszSectionName = ALLOC_STRING( pkvd->szValue );
		// Store this in message so we don't have to write save/restore for this ent
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseTrigger::KeyValue( pkvd );
}


class CTriggerGravity : public CBaseTrigger
{
public:
	void Spawn( void );
	void EXPORT GravityTouch( CBaseEntity *pOther );
};
LINK_ENTITY_TO_CLASS( trigger_gravity, CTriggerGravity );

void CTriggerGravity::Spawn( void )
{
	InitTrigger();
	SetTouch( &CTriggerGravity::GravityTouch );
}

void CTriggerGravity::GravityTouch( CBaseEntity *pOther )
{
	// Only save on clients
	if ( !pOther->IsPlayer() )
		return;

	pOther->pev->gravity = pev->gravity;
}







// this is a really bad idea.
class CTriggerChangeTarget : public CBaseDelay
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int ObjectCaps( void ) { return CBaseDelay::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

private:
	int		m_iszNewTarget;
};
LINK_ENTITY_TO_CLASS( trigger_changetarget, CTriggerChangeTarget );

TYPEDESCRIPTION	CTriggerChangeTarget::m_SaveData[] = 
{
	DEFINE_FIELD( CTriggerChangeTarget, m_iszNewTarget, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE(CTriggerChangeTarget,CBaseDelay);

void CTriggerChangeTarget::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "m_iszNewTarget"))
	{
		m_iszNewTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CTriggerChangeTarget::Spawn( void )
{
}


void CTriggerChangeTarget::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pTarget = NULL;

	while ( ( pTarget = UTIL_FindEntityByString( pTarget, "targetname", STRING( pev->target ) ) ) != NULL )
	{
		pTarget->pev->target = m_iszNewTarget;

		CBaseMonster *pMonster = pTarget->MyMonsterPointer( );

		if (pMonster)
		{
			pMonster->m_hGoalEnt = NULL;
		}

		if ( FClassnameIs( pTarget->pev, "path_track" ) )
			( (CPathTrack *) pTarget )->Link();
	}
}
