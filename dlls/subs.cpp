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

===== subs.cpp ========================================================

  frequently used global functions

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "CSpawnPoints.h"
#include "InventoryRules.h"
#include "saverestore.h"
#include "nodes.h"
#include "doors.h"

#define SF_BUTTON_DONTMOVE		1

extern CGraph WorldGraph;

extern BOOL FEntIsVisible(entvars_t* pev, entvars_t* pevTarget);

extern DLL_GLOBAL int g_iSkillLevel;


// Landmark class
void CPointEntity :: Spawn( void )
{
	pev->solid = SOLID_NOT;
//	UTIL_SetSize(pev, g_vecZero, g_vecZero);
}


class CNullEntity : public CBaseEntity
{
public:
	void Spawn( void );
};


// Null Entity, remove on startup
void CNullEntity :: Spawn( void )
{
	REMOVE_ENTITY(ENT(pev));
}
LINK_ENTITY_TO_CLASS(info_null,CNullEntity);

class CBaseDMStart : public CPointEntity
{
public:
	void		Spawn( void );
	void		Precache( void );
	void		KeyValue( KeyValueData *pkvd );
	int			ObjectCaps( void ) { return FCAP_ACROSS_TRANSITION; }
	BOOL		IsTriggered( CBaseEntity *pEntity );
	BOOL		IsAlive( void ) { return m_fEnabled; }
	void		Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value );
	Vector		GetSpawnSpot( void );
	void		PlayerSpawns( CBasePlayer *player );

private:
	bool		m_fEnabled;
	USE_TYPE	m_triggerState;
};

// These are the new entry points to entities. 
LINK_ENTITY_TO_CLASS(info_player_deathmatch,CBaseDMStart);
LINK_ENTITY_TO_CLASS(info_player_dm2,CBaseDMStart);
LINK_ENTITY_TO_CLASS(info_player_start,CPointEntity);
LINK_ENTITY_TO_CLASS(info_player_coop,CPointEntity);
LINK_ENTITY_TO_CLASS(info_landmark,CPointEntity);

void CBaseDMStart::Spawn( void )
{
	m_fEnabled = !( pev->spawnflags & SF_SPAWNPOINT_START_OFF );

	if ( pev->spawnflags & SF_SPAWNPOINT_REPEL )
		Precache();
}

void CBaseDMStart::Precache( void )
{
	PrecacheModel_Intercept( this, "sprites/rope.spr" );
}

void CBaseDMStart::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	switch ( useType )
	{
	case USE_OFF:
		m_fEnabled = false;
		break;
	case USE_ON:
		m_fEnabled = true;
		break;
	case USE_SET:
		m_fEnabled = value != 0;
		break;
	case USE_TOGGLE:
		m_fEnabled = !m_fEnabled;
		break;
	case USE_KILL:
		UTIL_Remove( this );
		break;
	}
}

void CBaseDMStart::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "master"))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "triggerstate" ) )
	{
		int state = atoi( pkvd->szValue );

		m_triggerState = state == 0 ? USE_OFF : state == 2 ? USE_TOGGLE : USE_ON;
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

Vector CBaseDMStart::GetSpawnSpot( void )
{
	if ( CVAR_GET_POINTER( "mp_multiplespawn" )->value >= 1 )
		return pev->origin;

	Vector mins = pev->origin - Vector( 16, 16, 36 );
	Vector maxs = pev->origin + Vector( 16, 16, 36 );
	CBaseEntity *entities[1];

	if ( UTIL_EntitiesInBox( entities, ARRAYSIZE( entities ), mins, maxs, FL_CLIENT ) == 0 )
		return pev->origin;

	float angle = 0;

	do
	{
		Vector position( (float) sin( (double) angle ) * 40 + pev->origin.x,
			(float) cos( (double) angle ) * 40 + pev->origin.y, pev->origin.z + 0.0f );
		TraceResult trace = {};

		mins = position - Vector( 16, 16, 36 );
		maxs = position + Vector( 16, 16, 36 );
		UTIL_TraceLine( mins, maxs, ignore_monsters, NULL, &trace );

		if ( UTIL_PointContents( mins ) >= CONTENTS_EMPTY
			&& UTIL_PointContents( maxs ) >= CONTENTS_EMPTY
			&& !( trace.flFraction < 1 )
			&& UTIL_EntitiesInBox( entities, ARRAYSIZE( entities ), mins, maxs, FL_CLIENT ) == 0 )
		{
			return position;
		}

		angle = (float) ( (double) angle + 0.7853981633974483 );
	}
	while ( (double) angle < 6.283185307179586 );

	return pev->origin;
}

void CBaseDMStart::PlayerSpawns( CBasePlayer *player )
{
	if ( pev->netname )
		player->pev->targetname = pev->netname;

	if ( pev->spawnflags & SF_SPAWNPOINT_REPEL )
		player->StartRepelDown( pev->frags );

	if ( pev->spawnflags & SF_SPAWNPOINT_FIRE_TARGET )
		FireTargets( STRING( pev->target ), player, this, m_triggerState, 0 );
}

Vector GetSpawnSpotPlace( CBaseEntity *point )
{
	if ( !point )
		return g_vecZero;

	if ( FClassnameIs( point->pev, "info_player_start" )
		|| FClassnameIs( point->pev, "info_player_deathmatch" )
		|| FClassnameIs( point->pev, "info_player_dm2" ) )
	{
		return ( (CBaseDMStart *) point )->GetSpawnSpot();
	}

	return point->pev->origin;
}

void PlayerSpawns( edict_t *point, edict_t *player )
{
	if ( !point || point->free || !GET_PRIVATE( point ) || !player || player->free || !GET_PRIVATE( player ) )
		return;

	if ( !FStrEq( STRING( point->v.classname ), "info_player_deathmatch" )
		&& !FStrEq( STRING( point->v.classname ), "info_player_dm2" ) )
	{
		return;
	}

	CBaseEntity *entity = CBaseEntity::Instance( player );

	if ( !entity || !entity->IsPlayer() )
		return;

	CBaseDMStart *spawn = dynamic_cast<CBaseDMStart *>( CBaseEntity::Instance( point ) );

	if ( !spawn || player->free || !GET_PRIVATE( player ) )
		return;

	entity = CBaseEntity::Instance( player );

	if ( entity && entity->IsPlayer() )
		spawn->PlayerSpawns( (CBasePlayer *) entity );
}

BOOL CBaseDMStart::IsTriggered( CBaseEntity *pEntity )
{
	BOOL master = UTIL_IsMasterTriggered( pev->netname, pEntity );

	return master;
}

// This updates global tables that need to know about entities being removed
void CBaseEntity::UpdateOnRemove( void )
{
	int	i;

	if ( FBitSet( pev->flags, FL_GRAPHED ) )
	{
	// this entity was a LinkEnt in the world node graph, so we must remove it from
	// the graph since we are removing it from the world.
		for ( i = 0 ; i < WorldGraph.m_cLinks ; i++ )
		{
			if ( WorldGraph.m_pLinkPool [ i ].m_pLinkEnt == pev )
			{
				// if this link has a link ent which is the same ent that is removing itself, remove it!
				WorldGraph.m_pLinkPool [ i ].m_pLinkEnt = NULL;
			}
		}
	}

	// tell owner ( if any ) that we're gone. This is mostly for MonsterMaker functionality
	CBaseEntity *pOwner = CBaseEntity::Instance( pev->owner );

	if ( pOwner )
		pOwner->DeathNotice( pev );

	if ( pev->globalname )
		gGlobalState.EntitySetState( pev->globalname, GLOBAL_DEAD );
}

// Convenient way to delay removing oneself
void CBaseEntity :: SUB_Remove( void )
{
	UpdateOnRemove();
	if (pev->health > 0)
	{
		// this situation can screw up monsters who can't tell their entity pointers are invalid.
		pev->health = 0;
		ALERT( at_aiconsole, "SUB_Remove called on entity with health > 0\n");
	}

	REMOVE_ENTITY(ENT(pev));
}


// Convenient way to explicitly do nothing (passed to functions that require a method)
void CBaseEntity :: SUB_DoNothing( void )
{
}


CBaseDelay::CBaseDelay()
{
	m_hDelayActivator = NULL;
}

// Global Savedata for Delay
TYPEDESCRIPTION	CBaseDelay::m_SaveData[] = 
{
	DEFINE_FIELD( CBaseDelay, m_flDelay, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseDelay, m_iszKillTarget, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CBaseDelay, CBaseEntity );

void CBaseDelay :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "delay"))
	{
		m_flDelay = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "killtarget"))
	{
		m_iszKillTarget = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseEntity::KeyValue( pkvd );
	}
}


/*
==============================
SUB_UseTargets

If self.delay is set, a DelayedUse entity will be created that will actually
do the SUB_UseTargets after that many seconds have passed.

Removes all entities with a targetname that match self.killtarget,
and removes them, so some events can remove other triggers.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function (if they have one)

==============================
*/
void CBaseEntity :: SUB_UseTargets( CBaseEntity *pActivator, USE_TYPE useType, float value )
{
	//
	// fire targets
	//
	if (!FStringNull(pev->target))
	{
		FireTargets( STRING(pev->target), pActivator, this, useType, value );
	}
}


#define TARGET_MAX_RECURSION	128

int giTargetRecursionLevel = 0;

// Type 0 blocks listed names and type 1 allows only listed names
bool DoesFilterAllowTrigger( const char *pszName, const char *pszFilter, int iType )
{
	if ( UTIL_FindTokenInTokenList( pszName, pszFilter ) )
		return iType != 0;

	return iType != 1;
}

void FireTarget( CBaseEntity *pTarget, CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pTarget )
		return;

	if ( pCaller )
	{
		if ( pCaller->targetnameOutFilter
			&& !DoesFilterAllowTrigger( STRING( pTarget->pev->targetname ), STRING( pCaller->targetnameOutFilter ), pCaller->targetnameOutFilterType ) )
			return;

		if ( pCaller->classnameOutFilter
			&& !DoesFilterAllowTrigger( STRING( pTarget->pev->classname ), STRING( pCaller->classnameOutFilter ), pCaller->classnameOutFilterType ) )
			return;

		if ( pTarget->targetnameInFilter
			&& !DoesFilterAllowTrigger( STRING( pCaller->pev->targetname ), STRING( pTarget->targetnameInFilter ), pTarget->targetnameInFilterType ) )
			return;

		if ( pTarget->classnameInFilter
			&& !DoesFilterAllowTrigger( STRING( pCaller->pev->classname ), STRING( pTarget->classnameInFilter ), pTarget->classnameInFilterType ) )
			return;
	}

	pTarget->Use( pActivator, pCaller, useType, value );
}

void FireTargets( const char *targetName, CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !targetName || !*targetName )
		return;

	if ( pActivator )
	{
		const char *pszActivatorName = STRING( pActivator->pev->targetname );

		if ( strcmp( pszActivatorName, targetName ) )
		{
			giTargetRecursionLevel = 0;
		}
		else
		{
			if ( giTargetRecursionLevel > TARGET_MAX_RECURSION )
			{
				ALERT( at_console, "Warning: %s \"%s\" triggered itself over %i times.\n",
					STRING( pActivator->pev->classname ), pszActivatorName, TARGET_MAX_RECURSION );
				giTargetRecursionLevel = 0;
				return;
			}

			++giTargetRecursionLevel;
		}

		if ( !( pActivator->pev->flags & FL_KILLME ) && !strcmp( "!activator", targetName ) )
		{
			FireTarget( pActivator, pActivator, pCaller, useType, value );
			giTargetRecursionLevel = 0;
			return;
		}
	}
	else
	{
		giTargetRecursionLevel = 0;
	}

	edict_t *pentTarget = NULL;

	for (;;)
	{
		pentTarget = FIND_ENTITY_BY_TARGETNAME(pentTarget, targetName);
		if (FNullEnt(pentTarget))
			break;

		CBaseEntity *pTarget = (CBaseEntity *) GET_PRIVATE( pentTarget );
		if ( pTarget && !(pTarget->pev->flags & FL_KILLME) )
			FireTarget( pTarget, pActivator, pCaller, useType, value );
		}

	giTargetRecursionLevel = 0;
}

LINK_ENTITY_TO_CLASS( DelayedUse, CBaseDelay );


void CBaseDelay :: SUB_UseTargets( CBaseEntity *pActivator, USE_TYPE useType, float value )
{
	//
	// exit immediatly if we don't have a target or kill target
	//
	if (FStringNull(pev->target) && !m_iszKillTarget)
		return;

	//
	// check for a delay
	//
	if (m_flDelay != 0)
	{
		// create a temp object to fire at a later time
		CBaseDelay *pTemp = GetClassPtr( (CBaseDelay *)NULL);
		pTemp->pev->classname = MAKE_STRING("DelayedUse");

		pTemp->pev->nextthink = gpGlobals->time + m_flDelay;

		pTemp->SetThink( &CBaseDelay::CallDelayThink );
		
		// Save the useType
		pTemp->pev->button = (int)useType;

		pTemp->pev->frags = value;
		pTemp->m_iszKillTarget = m_iszKillTarget;
		pTemp->m_flDelay = 0; // prevent "recursion"
		pTemp->pev->target = pev->target;

		// HACKHACK
		// This wasn't in the release build of Half-Life.  We should have moved m_hActivator into this class
		// but changing member variable hierarchy would break save/restore without some ugly code.
		// This code is not as ugly as that code
		if ( pActivator && pActivator->IsPlayer() )		// If a player activates, then save it
		{
			pTemp->pev->owner = pActivator->edict();
		}
		else
		{
			pTemp->pev->owner = NULL;
		}

		edict_t *activator;

		if ( pActivator && pActivator->pev )
			activator = pActivator->pev->pContainingEntity;
		else
			activator = NULL;

		if ( activator && !activator->free )
			pTemp->m_hDelayActivator = pActivator;
		else
			pTemp->m_hDelayActivator = NULL;

		return;
	}

	//
	// kill the killtargets
	//

	if ( m_iszKillTarget )
	{
		edict_t *pentKillTarget = NULL;

		ALERT( at_aiconsole, "KillTarget: %s\n", STRING(m_iszKillTarget) );
		pentKillTarget = FIND_ENTITY_BY_TARGETNAME( NULL, STRING(m_iszKillTarget) );
		while ( !FNullEnt(pentKillTarget) )
		{
			UTIL_Remove( CBaseEntity::Instance(pentKillTarget) );

			ALERT( at_aiconsole, "killing %s\n", STRING( pentKillTarget->v.classname ) );
			pentKillTarget = FIND_ENTITY_BY_TARGETNAME( pentKillTarget, STRING(m_iszKillTarget) );
		}
	}
	
	//
	// fire targets
	//
	if (!FStringNull(pev->target))
	{
		FireTargets( STRING(pev->target), pActivator, this, useType, value );
	}
}


/*
void CBaseDelay :: SUB_UseTargetsEntMethod( void )
{
	SUB_UseTargets(pev);
}
*/

/*
QuakeEd only writes a single float for angles (bad idea), so up and down are
just constant angles.
*/
void SetMovedir( entvars_t *pev )
{
	if (pev->angles == Vector(0, -1, 0))
	{
		pev->movedir = Vector(0, 0, 1);
	}
	else if (pev->angles == Vector(0, -2, 0))
	{
		pev->movedir = Vector(0, 0, -1);
	}
	else
	{
		UTIL_MakeVectors(pev->angles);
		pev->movedir = gpGlobals->v_forward;
	}
	
	pev->angles = g_vecZero;
}




void CBaseDelay::DelayThink( void )
{
	CBaseEntity *pActivator = NULL;

	edict_t *activator = m_hDelayActivator.Get();

	if ( !activator || activator->free || !GET_PRIVATE( activator ) )
		activator = pev->owner;

	if ( activator && !activator->free && GET_PRIVATE( activator ) )
		pActivator = CBaseEntity::Instance( activator );

	SUB_UseTargets( pActivator, (USE_TYPE)pev->button, pev->frags );
	REMOVE_ENTITY(ENT(pev));
}


// Global Savedata for Toggle
TYPEDESCRIPTION	CBaseToggle::m_SaveData[] = 
{
	DEFINE_FIELD( CBaseToggle, m_toggle_state, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseToggle, m_flMoveDistance, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseToggle, m_flWait, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseToggle, m_flLip, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseToggle, m_flTWidth, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseToggle, m_flTLength, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseToggle, m_vecPosition1, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CBaseToggle, m_vecPosition2, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CBaseToggle, m_vecAngle1, FIELD_VECTOR ),		// UNDONE: Position could go through transition, but also angle?
	DEFINE_FIELD( CBaseToggle, m_vecAngle2, FIELD_VECTOR ),		// UNDONE: Position could go through transition, but also angle?
	DEFINE_FIELD( CBaseToggle, m_cTriggersLeft, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseToggle, m_flHeight, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseToggle, m_hActivator, FIELD_EHANDLE ),
	DEFINE_FIELD( CBaseToggle, m_pfnCallWhenMoveDone, FIELD_FUNCTION ),
	DEFINE_FIELD( CBaseToggle, m_vecFinalDest, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CBaseToggle, m_vecFinalAngle, FIELD_VECTOR ),
	DEFINE_FIELD( CBaseToggle, m_sMaster, FIELD_STRING),
	DEFINE_FIELD( CBaseToggle, m_bitsDamageInflict, FIELD_INTEGER ),	// damage type inflicted
};
IMPLEMENT_SAVERESTORE( CBaseToggle, CBaseAnimating );

void CBaseToggle::OnCreate( void )
{
	CBaseAnimating::OnCreate();

	m_pInventoryRules = new CInventoryRules( this );
}

void CBaseToggle::OnDestroy( void )
{
	delete m_pInventoryRules;
	m_pInventoryRules = NULL;

	CBaseAnimating::OnDestroy();
}

void CBaseToggle::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "lip"))
	{
		m_flLip = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "wait"))
	{
		m_flWait = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "master"))
	{
		m_sMaster = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "distance"))
	{
		m_flMoveDistance = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( m_pInventoryRules->KeyValue( pkvd ) )
		pkvd->fHandled = TRUE;
	else
		CBaseDelay::KeyValue( pkvd );
}

/*
=============
LinearMove

calculate pev->velocity and pev->nextthink to reach vecDest from
pev->origin traveling at flSpeed
===============
*/
void CBaseToggle ::  LinearMove( Vector	vecDest, float flSpeed )
{
	ASSERTSZ(flSpeed != 0, "LinearMove:  no speed is defined!");
//	ASSERTSZ(m_pfnCallWhenMoveDone != NULL, "LinearMove: no post-move function defined");
	
	m_vecFinalDest = vecDest;

	// Already there?
	if ( vecDest == pev->origin || ( FClassnameIs( pev, "func_button" ) && ( m_flLip == 0 || FBitSet( pev->spawnflags, SF_BUTTON_DONTMOVE ) ) ) )
	{
		LinearMoveDone();
		return;
	}
		
	// set destdelta to the vector needed to move
	Vector vecDestDelta = vecDest - pev->origin;
	
	// divide vector length by speed to get time to reach dest
	float flTravelTime = vecDestDelta.Length() / flSpeed;

	// set nextthink to trigger a call to LinearMoveDone when dest is reached
	pev->nextthink = pev->ltime + flTravelTime;
	SetThink( &CBaseToggle::LinearMoveDone );

	// scale the destdelta vector by the time spent traveling to get velocity
	pev->velocity = vecDestDelta / flTravelTime;
}


/*
============
After moving, set origin to exact final destination, call "move done" function
============
*/
void CBaseToggle :: LinearMoveDone( void )
{
	Vector delta = m_vecFinalDest - pev->origin;
	float error = delta.Length();
	if ( error > 0.03125 )
	{
		LinearMove( m_vecFinalDest, 100 );
		return;
	}

	UTIL_SetOrigin(pev, m_vecFinalDest);
	pev->velocity = g_vecZero;
	pev->nextthink = -1;
	if ( m_pfnCallWhenMoveDone )
		(this->*m_pfnCallWhenMoveDone)();
}

BOOL CBaseToggle :: IsLockedByMaster( void )
{
	if (m_sMaster && !UTIL_IsMasterTriggered(m_sMaster, m_hActivator))
		return TRUE;
	else
		return FALSE;
}

/*
=============
AngularMove

calculate pev->velocity and pev->nextthink to reach vecDest from
pev->origin traveling at flSpeed
Just like LinearMove, but rotational.
===============
*/
void CBaseToggle :: AngularMove( Vector vecDestAngle, float flSpeed )
{
	ASSERTSZ(flSpeed != 0, "AngularMove:  no speed is defined!");
//	ASSERTSZ(m_pfnCallWhenMoveDone != NULL, "AngularMove: no post-move function defined");
	
	m_vecFinalAngle = vecDestAngle;

	// Already there?
	if (vecDestAngle == pev->angles)
	{
		AngularMoveDone();
		return;
	}
	
	// set destdelta to the vector needed to move
	Vector vecDestDelta = vecDestAngle - pev->angles;
	
	// divide by speed to get time to reach dest
	float flTravelTime = vecDestDelta.Length() / flSpeed;

	// set nextthink to trigger a call to AngularMoveDone when dest is reached
	pev->nextthink = pev->ltime + flTravelTime;
	SetThink( &CBaseToggle::AngularMoveDone );

	// scale the destdelta vector by the time spent traveling to get velocity
	pev->avelocity = vecDestDelta / flTravelTime;
}


/*
============
After rotating, set angle to exact final angle, call "move done" function
============
*/
void CBaseToggle :: AngularMoveDone( void )
{
	pev->angles = m_vecFinalAngle;
	pev->avelocity = g_vecZero;
	pev->nextthink = -1;
	if ( m_pfnCallWhenMoveDone )
		(this->*m_pfnCallWhenMoveDone)();
}


float CBaseToggle :: AxisValue( int flags, const Vector &angles )
{
	if ( FBitSet(flags, SF_DOOR_ROTATE_Z) )
		return angles.z;
	if ( FBitSet(flags, SF_DOOR_ROTATE_X) )
		return angles.x;

	return angles.y;
}


void CBaseToggle :: AxisDir( entvars_t *pev )
{
	if ( FBitSet(pev->spawnflags, SF_DOOR_ROTATE_Z) )
		pev->movedir = Vector ( 0, 0, 1 );	// around z-axis
	else if ( FBitSet(pev->spawnflags, SF_DOOR_ROTATE_X) )
		pev->movedir = Vector ( 1, 0, 0 );	// around x-axis
	else
		pev->movedir = Vector ( 0, 1, 0 );		// around y-axis
}


float CBaseToggle :: AxisDelta( int flags, const Vector &angle1, const Vector &angle2 )
{
	if ( FBitSet (flags, SF_DOOR_ROTATE_Z) )
		return angle1.z - angle2.z;
	
	if ( FBitSet (flags, SF_DOOR_ROTATE_X) )
		return angle1.x - angle2.x;

	return angle1.y - angle2.y;
}


/*
=============
FEntIsVisible

returns TRUE if the passed entity is visible to caller, even if not infront ()
=============
*/
	BOOL
FEntIsVisible(
	entvars_t*		pev,
	entvars_t*		pevTarget)
	{
	Vector vecSpot1 = pev->origin + pev->view_ofs;
	Vector vecSpot2 = pevTarget->origin + pevTarget->view_ofs;
	TraceResult tr;

	UTIL_TraceLine(vecSpot1, vecSpot2, ignore_monsters, ENT(pev), &tr);
	
	if (tr.fInOpen && tr.fInWater)
		return FALSE;                   // sight line crossed contents

	if (tr.flFraction == 1)
		return TRUE;

	return FALSE;
	}


