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

===== plats.cpp ========================================================

  spawn, think, and touch functions for trains, etc

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "plats.h"
#include "trains.h"
#include "saverestore.h"
#include "doors.h"

static void PlatSpawnInsideTrigger(entvars_t* pevPlatform);


TYPEDESCRIPTION	CBasePlatTrain::m_SaveData[] = 
{
	DEFINE_FIELD( CBasePlatTrain, m_bMoveSnd, FIELD_CHARACTER ),
	DEFINE_FIELD( CBasePlatTrain, m_bStopSnd, FIELD_CHARACTER ),
	DEFINE_FIELD( CBasePlatTrain, m_volume, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CBasePlatTrain, CBaseToggle );

void CBasePlatTrain :: KeyValue( KeyValueData *pkvd )
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
	else if (FStrEq(pkvd->szKeyName, "height"))
	{
		m_flHeight = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "rotation"))
	{
		m_vecFinalAngle.x = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "movesnd"))
	{
		m_bMoveSnd = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "movesnd_loop"))
	{
		m_fMoveSoundLoop = atoi( pkvd->szValue ) >= 1;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "stopsnd"))
	{
		m_bStopSnd = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "volume"))
	{
		m_volume = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( !m_StartStopVars.KeyValue( pkvd ) )
		CBaseToggle::KeyValue( pkvd );
}

#define noiseMoving noise
#define noiseArrived noise1

const char *PlatMovingSound( int iSound )
{
	switch ( iSound )
	{
	case 1:		return "plats/bigmove1.wav";
	case 2:		return "plats/bigmove2.wav";
	case 3:		return "plats/elevmove1.wav";
	case 4:		return "plats/elevmove2.wav";
	case 5:		return "plats/elevmove3.wav";
	case 6:		return "plats/freightmove1.wav";
	case 7:		return "plats/freightmove2.wav";
	case 8:		return "plats/heavymove1.wav";
	case 9:		return "plats/rackmove1.wav";
	case 10:	return "plats/railmove1.wav";
	case 11:	return "plats/squeekmove1.wav";
	case 12:	return "plats/talkmove1.wav";
	case 13:	return "plats/talkmove2.wav";
	default:	return "common/null.wav";
	}
}

const char *PlatStopSound( int iSound )
{
	switch ( iSound )
	{
	case 1:		return "plats/bigstop1.wav";
	case 2:		return "plats/bigstop2.wav";
	case 3:		return "plats/freightstop1.wav";
	case 4:		return "plats/heavystop2.wav";
	case 5:		return "plats/rackstop1.wav";
	case 6:		return "plats/railstop1.wav";
	case 7:		return "plats/squeekstop1.wav";
	case 8:		return "plats/talkstop1.wav";
	default:	return "common/null.wav";
	}
}

const char *TrainMovingSound( int iSound )
{
	switch ( iSound )
	{
	case 1:		return "plats/ttrain1.wav";
	case 2:		return "plats/ttrain2.wav";
	case 3:		return "plats/ttrain3.wav";
	case 4:		return "plats/ttrain4.wav";
	case 5:		return "plats/ttrain6.wav";
	case 6:		return "plats/ttrain7.wav";
	default:	return NULL;
	}
}

void CBasePlatTrain::Precache( void )
{
	CBaseEntity::Precache();

	if ( !pev->noiseMoving )
		pev->noiseMoving = MAKE_STRING( PlatMovingSound( m_bMoveSnd ) );

	PrecacheSound_Intercept( this, STRING( pev->noiseMoving ) );

	if ( !pev->noiseArrived )
		pev->noiseArrived = MAKE_STRING( PlatStopSound( m_bStopSnd ) );

	PrecacheSound_Intercept( this, STRING( pev->noiseArrived ) );
}

//
//====================== PLAT code ====================================================
//


#define noiseMovement noise
#define noiseStopMoving noise1

class CFuncPlat : public CBasePlatTrain
{
public:
	CFuncPlat()
	{
		m_StartStopVars.m_iszFireOnStart = 0;
		m_StartStopVars.m_iFireOnStartTriggerState = USE_TOGGLE;
		m_StartStopVars.m_iszFireOnStop = 0;
		m_StartStopVars.m_iFireOnStopTriggerState = USE_TOGGLE;
		m_OpenClosableVars.m_iszFireOnOpening = 0;
		m_OpenClosableVars.m_iFireOnOpeningTriggerState = USE_TOGGLE;
		m_OpenClosableVars.m_iszFireOnClosing = 0;
		m_OpenClosableVars.m_iFireOnClosingTriggerState = USE_TOGGLE;
		m_OpenClosableVars.m_iszFireOnOpened = 0;
		m_OpenClosableVars.m_iFireOnOpenedTriggerState = USE_TOGGLE;
		m_OpenClosableVars.m_iszFireOnClosed = 0;
		m_OpenClosableVars.m_iFireOnClosedTriggerState = USE_TOGGLE;
	}

	void Spawn( void );
	void Precache( void );
	void Setup( void );
	void KeyValue( KeyValueData *pkvd );

	COpenClosableVars	m_OpenClosableVars;

	virtual void Blocked( CBaseEntity *pOther );


	void EXPORT PlatUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void	EXPORT CallGoDown( void ) { GoDown(); }
	void	EXPORT CallHitTop( void  ) { HitTop(); }
	void	EXPORT CallHitBottom( void ) { HitBottom(); }

	virtual void GoUp( void );
	virtual void GoDown( void );
	virtual void HitTop( void );
	virtual void HitBottom( void );
};
LINK_ENTITY_TO_CLASS( func_plat, CFuncPlat );


// UNDONE: Need to save this!!! It needs class & linkage
class CPlatTrigger : public CBaseEntity
{
public:
	virtual int	ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_DONT_SAVE; }
	void SpawnInsideTrigger( CFuncPlat *pPlatform );
	void Touch( CBaseEntity *pOther );
	CFuncPlat *m_pPlatform;
	EHANDLE m_hPlatform;
};



/*QUAKED func_plat (0 .5 .8) ? PLAT_LOW_TRIGGER
speed	default 150

Plats are always drawn in the extended position, so they will light correctly.

If the plat is the target of another trigger or button, it will start out disabled in
the extended position until it is trigger, when it will lower and become a normal plat.

If the "height" key is set, that will determine the amount the plat moves, instead of
being implicitly determined by the model's height.

Set "sounds" to one of the following:
1) base fast
2) chain slow
*/

void CFuncPlat :: Setup( void )
{
	//pev->noiseMovement = MAKE_STRING("plats/platmove1.wav");
	//pev->noiseStopMoving = MAKE_STRING("plats/platstop1.wav");

	if (m_flTLength == 0)
		m_flTLength = 80;
	if (m_flTWidth == 0)
		m_flTWidth = 10;
	
	pev->angles		= g_vecZero;

	pev->solid		= SOLID_BSP;
	pev->movetype	= MOVETYPE_PUSH;

	UTIL_SetOrigin(pev, pev->origin);		// set size and link into world
	UTIL_SetSize(pev, pev->mins, pev->maxs);
	SET_MODEL(ENT(pev), STRING(pev->model) );

	// vecPosition1 is the top position, vecPosition2 is the bottom
	m_vecPosition1 = pev->origin;
	m_vecPosition2 = pev->origin;
	if (m_flHeight != 0)
		m_vecPosition2.z = pev->origin.z - m_flHeight;
	else
		m_vecPosition2.z = pev->origin.z - pev->size.z + 8;
	if (pev->speed == 0)
		pev->speed = 150;

	if ( m_volume == 0 )
		m_volume = 0.85;
}


void CFuncPlat :: Precache( )
{
	CBasePlatTrain::Precache();
	//PRECACHE_SOUND("plats/platmove1.wav");
	//PRECACHE_SOUND("plats/platstop1.wav");
	if ( !IsTogglePlat() )
		PlatSpawnInsideTrigger( pev );		// the "start moving" trigger
}


void CFuncPlat :: Spawn( )
{
	Setup();

	Precache();

	// If this platform is the target of some button, it starts at the TOP position,
	// and is brought down by that button.  Otherwise, it starts at BOTTOM.
	if ( !FStringNull(pev->targetname) )
	{
		UTIL_SetOrigin (pev, m_vecPosition1);
		m_toggle_state = TS_AT_TOP;
		SetUse( &CFuncPlat::PlatUse );
	}
	else
	{
		UTIL_SetOrigin (pev, m_vecPosition2);
		m_toggle_state = TS_AT_BOTTOM;
	}
}



static void PlatSpawnInsideTrigger(entvars_t* pevPlatform)
{
	GetClassPtr( (CPlatTrigger *)NULL)->SpawnInsideTrigger( GetClassPtr( (CFuncPlat *)pevPlatform ) );
}
		

//
// Create a trigger entity for a platform.
//
void CPlatTrigger :: SpawnInsideTrigger( CFuncPlat *pPlatform )
{
	m_pPlatform = pPlatform;
	m_hPlatform = pPlatform;
	// Create trigger entity, "point" it at the owning platform, give it a touch method
	pev->solid		= SOLID_TRIGGER;
	pev->movetype	= MOVETYPE_NONE;
	pev->origin = pPlatform->pev->origin;

	// Establish the trigger field's size
	Vector vecTMin = m_pPlatform->pev->mins + Vector ( 25 , 25 , 0 );
	Vector vecTMax = m_pPlatform->pev->maxs + Vector ( 25 , 25 , 8 );
	vecTMin.z = vecTMax.z - ( m_pPlatform->m_vecPosition1.z - m_pPlatform->m_vecPosition2.z + 8 );
	if (m_pPlatform->pev->size.x <= 50)
	{
		vecTMin.x = (m_pPlatform->pev->mins.x + m_pPlatform->pev->maxs.x) / 2;
		vecTMax.x = vecTMin.x + 1;
	}
	if (m_pPlatform->pev->size.y <= 50)
	{
		vecTMin.y = (m_pPlatform->pev->mins.y + m_pPlatform->pev->maxs.y) / 2;
		vecTMax.y = vecTMin.y + 1;
	}
	UTIL_SetSize ( pev, vecTMin, vecTMax );
}


//
// When the platform's trigger field is touched, the platform ???
//
void CPlatTrigger :: Touch( CBaseEntity *pOther )
{
	// Ignore touches by non-players
	entvars_t*	pevToucher = pOther->pev;
	if ( !FClassnameIs (pevToucher, "player") )
		return;

	// Ignore touches by corpses
	if (!pOther->IsAlive())
		return;

	CFuncPlat *pPlatform = (CFuncPlat *) (CBaseEntity *) m_hPlatform;

	if ( !pPlatform )
	{
		UTIL_Remove( this );
		return;
	}

	// Make linked platform go up/down.
	if (pPlatform->m_toggle_state == TS_AT_BOTTOM)
		pPlatform->GoUp();
	else if (pPlatform->m_toggle_state == TS_AT_TOP)
		pPlatform->pev->nextthink = pPlatform->pev->ltime + 1;// delay going down
}


//
// Used by SUB_UseTargets, when a platform is the target of a button.
// Start bringing platform down.
//
void CFuncPlat :: PlatUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( IsTogglePlat() )
	{
		// Top is off, bottom is on
		BOOL on = (m_toggle_state == TS_AT_BOTTOM) ? TRUE : FALSE;

		if ( !ShouldToggle( useType, on ) )
			return;

		if (m_toggle_state == TS_AT_TOP)
			GoDown();
		else if ( m_toggle_state == TS_AT_BOTTOM )
			GoUp();
	}
	else
	{
		SetUse( NULL );

		if (m_toggle_state == TS_AT_TOP)
			GoDown();
	}
}


//
// Platform is at top, now starts moving down.
//
void CFuncPlat::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "fireonopening" ) )
	{
		m_OpenClosableVars.m_iszFireOnOpening = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonopening_triggerstate" ) )
	{
		m_OpenClosableVars.m_iFireOnOpeningTriggerState = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonclosing" ) )
	{
		m_OpenClosableVars.m_iszFireOnClosing = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonclosing_triggerstate" ) )
	{
		m_OpenClosableVars.m_iFireOnClosingTriggerState = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonopened" ) || FStrEq( pkvd->szKeyName, "fireonopen" ) )
	{
		m_OpenClosableVars.m_iszFireOnOpened = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonopened_triggerstate" ) )
	{
		m_OpenClosableVars.m_iFireOnOpenedTriggerState = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonclosed" ) )
	{
		m_OpenClosableVars.m_iszFireOnClosed = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonclosed_triggerstate" ) )
	{
		m_OpenClosableVars.m_iFireOnClosedTriggerState = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBasePlatTrain::KeyValue( pkvd );
	}
}

void CFuncPlat :: GoDown( void )
{
	if(pev->noiseMovement)
		EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING( pev->noiseMovement ), m_volume, ATTN_NORM, MoveSoundFlags(), PITCH_NORM );

	ASSERT(m_toggle_state == TS_AT_TOP || m_toggle_state == TS_GOING_UP);
	m_toggle_state = TS_GOING_DOWN;
	SetMoveDone(&CFuncPlat::CallHitBottom);
	LinearMove(m_vecPosition2, pev->speed);

	EntityHandle<CBaseEntity> activator( m_hActivator );

	m_StartStopVars.TriggerStartedTargets( this, activator );

	activator = (CBaseEntity *) m_hActivator;
	m_OpenClosableVars.TriggerMoveStartedTargets( this, activator, false, false );
}


//
// Platform has hit bottom.  Stops and waits forever.
//
void CFuncPlat :: HitBottom( void )
{
	if(pev->noiseMovement)
		STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMovement));

	if (pev->noiseStopMoving)
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, (char*)STRING(pev->noiseStopMoving), m_volume, ATTN_NORM);

	ASSERT(m_toggle_state == TS_GOING_DOWN);
	m_toggle_state = TS_AT_BOTTOM;

	EntityHandle<CBaseEntity> activator( m_hActivator );

	m_StartStopVars.TriggerStoppedTargets( this, activator );

	activator = (CBaseEntity *) m_hActivator;
	m_OpenClosableVars.TriggerMoveFinishedTargets( this, activator, false, false );
}


//
// Platform is at bottom, now starts moving up
//
void CFuncPlat :: GoUp( void )
{
	if (pev->noiseMovement)
		EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING( pev->noiseMovement ), m_volume, ATTN_NORM, MoveSoundFlags(), PITCH_NORM );

	ASSERT(m_toggle_state == TS_AT_BOTTOM || m_toggle_state == TS_GOING_DOWN);
	m_toggle_state = TS_GOING_UP;
	SetMoveDone(&CFuncPlat::CallHitTop);
	LinearMove(m_vecPosition1, pev->speed);

	EntityHandle<CBaseEntity> activator( m_hActivator );

	m_StartStopVars.TriggerStartedTargets( this, activator );

	activator = (CBaseEntity *) m_hActivator;
	m_OpenClosableVars.TriggerMoveStartedTargets( this, activator, true, false );
}


//
// Platform has hit top.  Pauses, then starts back down again.
//
void CFuncPlat :: HitTop( void )
{
	if(pev->noiseMovement)
		STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMovement));

	if (pev->noiseStopMoving)
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, (char*)STRING(pev->noiseStopMoving), m_volume, ATTN_NORM);
	
	ASSERT(m_toggle_state == TS_GOING_UP);
	m_toggle_state = TS_AT_TOP;

	if ( !IsTogglePlat() )
	{
		// After a delay, the platform will automatically start going down again.
		SetThink( &CFuncPlat::CallGoDown );
		pev->nextthink = pev->ltime + 3;
	}

	EntityHandle<CBaseEntity> activator( m_hActivator );

	m_StartStopVars.TriggerStoppedTargets( this, activator );

	activator = (CBaseEntity *) m_hActivator;
	m_OpenClosableVars.TriggerMoveFinishedTargets( this, activator, true, false );
}


void CFuncPlat :: Blocked( CBaseEntity *pOther )
{
	ALERT( at_aiconsole, "%s Blocked by %s\n", STRING(pev->classname), STRING(pOther->pev->classname) );
	// Hurt the blocker a little
	pOther->TakeDamage(pev, pev, 1, DMG_CRUSH);

	if(pev->noiseMovement)
		STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMovement));
	
	// Send the platform back where it came from
	ASSERT(m_toggle_state == TS_GOING_UP || m_toggle_state == TS_GOING_DOWN);
	if (m_toggle_state == TS_GOING_UP)
		GoDown();
	else if (m_toggle_state == TS_GOING_DOWN)
		GoUp ();
}


class CFuncPlatRot : public CFuncPlat
{
public:
	void Spawn( void );
	void SetupRotation( void );

	virtual void	GoUp( void );
	virtual void	GoDown( void );
	virtual void	HitTop( void );
	virtual void	HitBottom( void );
	
	void			RotMove( Vector &destAngle, float time );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	Vector	m_end, m_start;
};
LINK_ENTITY_TO_CLASS( func_platrot, CFuncPlatRot );
TYPEDESCRIPTION	CFuncPlatRot::m_SaveData[] = 
{
	DEFINE_FIELD( CFuncPlatRot, m_end, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncPlatRot, m_start, FIELD_VECTOR ),
};

IMPLEMENT_SAVERESTORE( CFuncPlatRot, CFuncPlat );


void CFuncPlatRot :: SetupRotation( void )
{
	if ( m_vecFinalAngle.x != 0 )		// This plat rotates too!
	{
		CBaseToggle :: AxisDir( pev );
		m_start	= pev->angles;
		m_end = pev->angles + pev->movedir * m_vecFinalAngle.x;
	}
	else
	{
		m_start = g_vecZero;
		m_end = g_vecZero;
	}
	if ( !FStringNull(pev->targetname) )	// Start at top
	{
		pev->angles = m_end;
	}
}


void CFuncPlatRot :: Spawn( void )
{
	CFuncPlat :: Spawn();
	SetupRotation();
}

void CFuncPlatRot :: GoDown( void )
{
	CFuncPlat :: GoDown();
	RotMove( m_start, pev->nextthink - pev->ltime );
}


//
// Platform has hit bottom.  Stops and waits forever.
//
void CFuncPlatRot :: HitBottom( void )
{
	CFuncPlat :: HitBottom();
	pev->avelocity = g_vecZero;
	pev->angles = m_start;
}


//
// Platform is at bottom, now starts moving up
//
void CFuncPlatRot :: GoUp( void )
{
	CFuncPlat :: GoUp();
	RotMove( m_end, pev->nextthink - pev->ltime );
}


//
// Platform has hit top.  Pauses, then starts back down again.
//
void CFuncPlatRot :: HitTop( void )
{
	CFuncPlat :: HitTop();
	pev->avelocity = g_vecZero;
	pev->angles = m_end;
}


void CFuncPlatRot :: RotMove( Vector &destAngle, float time )
{
	// set destdelta to the vector needed to move
	Vector vecDestDelta = destAngle - pev->angles;

	// Travel time is so short, we're practically there already;  so make it so.
	if ( time >= 0.1)
		pev->avelocity = vecDestDelta / time;
	else
	{
		pev->avelocity = vecDestDelta;
		pev->nextthink = pev->ltime + 1;
	}
}


//
//====================== TRAIN code ==================================================
//


LINK_ENTITY_TO_CLASS( func_train, CFuncTrain );
TYPEDESCRIPTION	CFuncTrain::m_SaveData[] =
{
	DEFINE_FIELD( CFuncTrain, m_hCurrentTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( CFuncTrain, m_activated, FIELD_CHARACTER ),
};

IMPLEMENT_SAVERESTORE( CFuncTrain, CBasePlatTrain );

void CFuncTrain::MoveSound( bool start, bool stopped )
{
	if ( pev->noiseMovement )
	{
		if ( start )
		{
			EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING( pev->noiseMovement ), m_volume, ATTN_NORM, MoveSoundFlags(), PITCH_NORM );
			return;
		}

		STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noiseMovement ) );
	}

	if ( !start && stopped && pev->noiseStopMoving )
		EMIT_SOUND( edict(), CHAN_VOICE, STRING( pev->noiseStopMoving ), m_volume, ATTN_NORM );
}

void CFuncTrain::UpdateOnRemove( void )
{
	CBaseEntity::UpdateOnRemove();

	if ( pev->noiseMovement )
		STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noiseMovement ) );
}


void CFuncTrain :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iObeyTriggerMode" ) )
	{
		m_iObeyTriggerMode = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBasePlatTrain::KeyValue( pkvd );
}


void CFuncTrain :: Blocked( CBaseEntity *pOther )

{
	if ( gpGlobals->time < m_flActivateFinished)
		return;

	m_flActivateFinished = gpGlobals->time + 0.5;
	
	pOther->TakeDamage(pev, pev, pev->dmg, DMG_CRUSH);
}


void CFuncTrain :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_iObeyTriggerMode )
	{
		if ( pev->spawnflags & SF_TRAIN_WAIT_RETRIGGER )
		{
			if ( useType != USE_OFF )
			{
				pev->spawnflags &= ~SF_TRAIN_WAIT_RETRIGGER;
				Next();
			}

			return;
		}

		if ( useType == USE_ON )
			return;
	}

	if ( pev->spawnflags & SF_TRAIN_WAIT_RETRIGGER )
	{
		// Move toward my target
		pev->spawnflags &= ~SF_TRAIN_WAIT_RETRIGGER;
		Next();
	}
	else
	{
		pev->spawnflags |= SF_TRAIN_WAIT_RETRIGGER;
		// Pop back to last target if it's available
		if ( pev->enemy )
			pev->target = pev->enemy->v.targetname;
		pev->nextthink = 0;
		pev->velocity = g_vecZero;
		MoveSound( false, true );
	}
}


void CFuncTrain :: Wait( void )
{
	if ( !m_hCurrentTarget )
		return;

	Vector vecDest = m_hCurrentTarget->pev->origin - ( pev->mins + pev->maxs ) * 0.5;

	if ( !( ( vecDest - pev->origin ).Length() <= 0.1 ) )
	{
		// the path corner moved while we were on our way
		ClearBits( pev->effects, EF_NOINTERP );
		SetMoveDone( &CFuncTrain::Wait );
		LinearMove( vecDest, pev->speed );
		return;
	}

	pev->origin = vecDest;

	// Fire the pass target if there is one
	if ( m_hCurrentTarget->pev->message )
	{
		FireTargets( STRING(m_hCurrentTarget->pev->message), this, this, USE_TOGGLE, 0 );
		if ( FBitSet( m_hCurrentTarget->pev->spawnflags, SF_CORNER_FIREONCE ) )
			m_hCurrentTarget->pev->message = 0;
	}
		
	// need pointer to LAST target.
	if ( FBitSet (m_hCurrentTarget->pev->spawnflags , SF_TRAIN_WAIT_RETRIGGER ) || ( pev->spawnflags & SF_TRAIN_WAIT_RETRIGGER ) )
    {
		pev->spawnflags |= SF_TRAIN_WAIT_RETRIGGER;
        // clear the sound channel.
		MoveSound( false, true );
		pev->nextthink = 0;

		EntityHandle<CBaseEntity> activator( m_hActivator );
		m_StartStopVars.TriggerStoppedTargets( this, activator );
        return;
    }
    
    // ALERT ( at_console, "%f\n", m_flWait );

	if (m_flWait != 0)
	{// -1 wait will wait forever!		
		pev->nextthink = pev->ltime + m_flWait;
		MoveSound( false, true );
		SetThink( &CFuncTrain::Next );
	}
	else
	{
		Next();// do it RIGHT now!
	}
}


//
// Train next - path corner needs to change to next target 
//
void CFuncTrain :: Next( void )
{
	CBaseEntity	*pTarg;

	
	// now find our next target
	if ( m_bRandomNextTarget )
	{
		CBaseEntity *pTargets[32];
		CBaseEntity *pFound = NULL;
		int iCount = 0;

		for ( ; iCount < 32; iCount++ )
		{
			pFound = UTIL_FindEntityByTargetname( pFound, STRING( pev->target ) );

			if ( !pFound )
				break;

			pTargets[iCount] = pFound;
		}

		pTarg = iCount ? pTargets[RANDOM_LONG( 0, iCount - 1 )] : NULL;
	}
	else
	{
		pTarg = GetNextTarget();
	}

    if ( !pTarg )
	{
		if ( pev->noiseMovement )
			STOP_SOUND( edict(), CHAN_STATIC, (char*)STRING(pev->noiseMovement) );
		// Play stop sound
		if ( pev->noiseStopMoving )
			EMIT_SOUND (ENT(pev), CHAN_VOICE, (char*)STRING(pev->noiseStopMoving), m_volume, ATTN_NORM);
		return;
	}

	// Save last target in case we need to find it again
	pev->message = pev->target;

	pev->target = pTarg->pev->target;
	m_bRandomNextTarget = FClassnameIs( pTarg->pev, "path_corner" ) && FBitSet( pTarg->pev->spawnflags, SF_CORNER_RANDOM_TARGET );

	m_flWait = pTarg->GetDelay();

	if ( m_hCurrentTarget == pTarg && m_flWait == 0 )
		m_flWait = 0.01;

	if ( m_hCurrentTarget && m_hCurrentTarget->pev->speed != 0 )
	{// don't copy speed from target if it is 0 (uninitialized)
        pev->speed = m_hCurrentTarget->pev->speed;
		ALERT( at_aiconsole, "Train %s speed to %4.2f\n", STRING(pev->targetname), pev->speed );
	}
	m_hCurrentTarget = pTarg;// keep track of this since path corners change our target for us.

    pev->enemy = pTarg->edict();//hack

	if(FBitSet(m_hCurrentTarget->pev->spawnflags, SF_CORNER_TELEPORT))
	{
		// Path corner has indicated a teleport to the next corner.
		SetBits(pev->effects, EF_NOINTERP);
		UTIL_SetOrigin(pev, pTarg->pev->origin - (pev->mins + pev->maxs)* 0.5);
		Wait(); // Get on with doing the next path corner.
	}
	else
	{
		// Normal linear move.
		
		// CHANGED this from CHAN_VOICE to CHAN_STATIC around OEM beta time because trains should
		// use CHAN_STATIC for their movement sounds to prevent sound field problems.
		// this is not a hack or temporary fix, this is how things should be. (sjb).
		MoveSound( true, false );
		ClearBits(pev->effects, EF_NOINTERP);
		SetMoveDone( &CFuncTrain::Wait );
		LinearMove (pTarg->pev->origin - (pev->mins + pev->maxs)* 0.5, pev->speed);

		EntityHandle<CBaseEntity> activator( m_hActivator );
		m_StartStopVars.TriggerStartedTargets( this, activator );
	}
}


void CFuncTrain :: Activate( void )
{
	// Not yet active, so teleport to first target
	if ( !m_activated )
	{
		CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );

		if ( !pTarget )
			return;

		pev->target = pTarget->pev->target;
		m_hCurrentTarget = pTarget;

		UTIL_SetOrigin( pev, pTarget->pev->origin - ( pev->mins + pev->maxs ) * 0.5 );
		
		if ( FStringNull(pev->targetname) )
		{	// not triggered, so start immediately
			pev->nextthink = pev->ltime + 0.1;
			SetThink( &CFuncTrain::Next );
		}
		else
			pev->spawnflags |= SF_TRAIN_WAIT_RETRIGGER;

		m_activated = true;
	}
}

/*QUAKED func_train (0 .5 .8) ?
Trains are moving platforms that players can ride.
The targets origin specifies the min point of the train at each corner.
The train spawns at the first target it is pointing at.
If the train is the target of a button or trigger, it will not begin moving until activated.
speed	default 100
dmg		default	2
sounds
1) ratchet metal
*/

void CFuncTrain :: Spawn( void )
{
	Precache();
	if (pev->speed == 0)
		pev->speed = 100;
	
	if ( FStringNull(pev->target) )
		ALERT(at_console, "FuncTrain \"%s\" with no target\n", STRING(pev->targetname));

	pev->movetype = MOVETYPE_PUSH;
	
	if ( FBitSet (pev->spawnflags, SF_TRACKTRAIN_PASSABLE) )
		pev->solid		= SOLID_NOT;
	else
		pev->solid		= SOLID_BSP;

	SET_MODEL( ENT(pev), STRING(pev->model) );
	UTIL_SetSize (pev, pev->mins, pev->maxs);
	UTIL_SetOrigin(pev, pev->origin);

	m_activated = FALSE;

	if ( m_volume == 0 )
		m_volume = 0.85;
}


void CFuncTrain::Precache( void )
{
	CBasePlatTrain::Precache();
}


void CFuncTrain::OverrideReset( void )
{
	CBaseEntity	*pTarg;

	// Are we moving?
	if ( pev->velocity != g_vecZero && pev->nextthink != 0 )
	{
		pev->target = pev->message;
		// now find our next target
		pTarg = GetNextTarget();
		if ( !pTarg )
		{
			pev->nextthink = 0;
			pev->velocity = g_vecZero;
		}
		else	// Keep moving for 0.1 secs, then find path_corner again and restart
		{
			SetThink( &CFuncTrain::Next );
			pev->nextthink = pev->ltime + 0.1;
		}
	}
}




// ---------------------------------------------------------------------
//
// Track Train
//
// ---------------------------------------------------------------------

TYPEDESCRIPTION CFuncTrackTrain::m_SaveData[] =
{
	DEFINE_FIELD( CFuncTrackTrain, m_hPath, FIELD_EHANDLE ),
	DEFINE_FIELD( CFuncTrackTrain, m_length, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_height, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_flMaxSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_dir, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_startSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_controlMins, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncTrackTrain, m_controlMaxs, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncTrackTrain, m_sounds, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncTrackTrain, m_flVolume, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_flBank, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_oldSpeed, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CFuncTrackTrain, CBaseEntity );
LINK_ENTITY_TO_CLASS( func_tracktrain, CFuncTrackTrain );

void CFuncTrackTrain :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "wheels"))
	{
		m_length = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "height"))
	{
		m_height = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "startspeed"))
	{
		m_startSpeed = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "sounds"))
	{
		m_sounds = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "volume"))
	{
		m_flVolume = (float) (atoi(pkvd->szValue));
		m_flVolume *= 0.1;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "bank"))
	{
		m_flBank = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( !stricmp( pkvd->szKeyName, "soundlist" ) )
	{
		m_iszSoundList = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}


void CFuncTrackTrain :: NextThink( float thinkTime, BOOL alwaysThink )
{
	if ( alwaysThink )
		pev->flags |= FL_ALWAYSTHINK;
	else
		pev->flags &= ~FL_ALWAYSTHINK;

	pev->nextthink = thinkTime;
}


void CFuncTrackTrain :: Blocked( CBaseEntity *pOther )
{
	entvars_t	*pevOther = pOther->pev;

	// Blocker is on-ground on the train
	if ( FBitSet( pevOther->flags, FL_ONGROUND ) && VARS(pevOther->groundentity) == pev )
	{
		float deltaSpeed = fabs(pev->speed);
		if ( deltaSpeed > 50 )
			deltaSpeed = 50;
		if ( !pevOther->velocity.z )
			pevOther->velocity.z += deltaSpeed;
		return;
	}
	else
		pevOther->velocity = (pevOther->origin - pev->origin ).Normalize() * pev->dmg;

	ALERT( at_aiconsole, "TRAIN(%s): Blocked by %s (dmg:%.2f)\n", STRING(pev->targetname), STRING(pOther->pev->classname), pev->dmg );
	if ( pev->dmg <= 0 )
		return;
	// we can't hurt this thing, so we're not concerned with it
	pOther->TakeDamage(pev, pev, pev->dmg, DMG_CRUSH);
}


void CFuncTrackTrain :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( useType != USE_SET )
	{
		if ( !ShouldToggle( useType, (pev->speed != 0) ) )
			return;

		if ( pev->speed == 0 )
		{
			pev->speed = m_flMaxSpeed * m_dir;

			Next();
		}
		else
		{
			pev->speed = 0;
			pev->velocity = g_vecZero;
			pev->avelocity = g_vecZero;
			StopSound();
			SetThink( NULL );
		}
	}
	else
	{
		m_iszGoal = 0;

		float delta = value;

		delta = ((int)(pev->speed * 4) / (int) m_flMaxSpeed )*0.25 + 0.25 * delta;
		if ( delta > 1 )
			delta = 1;
		else if ( delta < -1 )
			delta = -1;
		if ( pev->spawnflags & SF_TRACKTRAIN_FORWARDONLY )
		{
			if ( delta < 0 )
				delta = 0;
		}
		pev->speed = m_flMaxSpeed * delta;
		Next();	
		ALERT( at_aiconsole, "TRAIN(%s), speed to %.2f\n", STRING(pev->targetname), pev->speed );
	}
}


static float Fix( float angle )
{
	while ( angle < 0 )
		angle += 360;
	while ( angle > 360 )
		angle -= 360;

	return angle;
}


static void FixupAngles( Vector &v )
{
	v.x = Fix( v.x );
	v.y = Fix( v.y );
	v.z = Fix( v.z );
}

#define TRAIN_STARTPITCH	60
#define TRAIN_MAXPITCH		200
#define TRAIN_MAXSPEED		1000	// approx max speed for sound pitch calculation

void CFuncTrackTrain::SetMaxSpeed( float speed )
{
	m_flMaxSpeed = speed;
	pev->impulse = (int) speed;
	pev->speed = pev->speed < m_flMaxSpeed ? pev->speed : m_flMaxSpeed;
}

void CFuncTrackTrain :: StopSound()
{
	// if sound playing, stop it
	if (m_soundPlaying && pev->noise)
	{
		EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING( pev->noise ), 0, 0, SND_STOP, PITCH_NORM,
		                0, false, g_vecZero );

		/*
		STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise));
		*/
		EMIT_SOUND_DYN( edict(), CHAN_ITEM, STRING( pev->noise1 ), m_flVolume, ATTN_NORM, 0, PITCH_NORM,
		                0, false, g_vecZero );
	}

	m_soundPlaying = 0;
}

void CFuncTrackTrain :: UpdateSound()
{
	if (!pev->noise)
		return;

	float pitch = TRAIN_STARTPITCH + ( fabsf(pev->speed) * (TRAIN_MAXPITCH - TRAIN_STARTPITCH) / TRAIN_MAXSPEED);

	if (!m_soundPlaying)
	{
		// play startup sound for train
		EMIT_SOUND_DYN( edict(), CHAN_ITEM, STRING( pev->noise2 ), m_flVolume, ATTN_NORM, 0, PITCH_NORM,
		                0, false, g_vecZero );
		EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING(pev->noise), m_flVolume, ATTN_NORM, 0, (int) pitch,
		                0, false, g_vecZero );
		m_soundPlaying = 1;
	}
	else if ( pev->speed != m_flLastSoundSpeed )
	{
		EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING( pev->noise ), m_flVolume, ATTN_NORM, SND_CHANGE_PITCH, (int) pitch, 0, false, g_vecZero );
		m_flLastSoundSpeed = pev->speed;
	}
}

void CFuncTrackTrain :: Next( void )
{
	float time = 0.5;

	if ( m_iszGoal && !( ( pev->origin - m_hPath->pev->origin ).Length() >= 32.0f )
		&& FStrEq( STRING( m_hPath->pev->targetname ), STRING( m_iszGoal ) ) )
	{
		StopSound();
		pev->velocity = g_vecZero;
		pev->speed = 0;
		m_oldSpeed = pev->speed;
		m_iszGoal = 0;
	}

	if ( !pev->speed )
	{
		ALERT( at_aiconsole, "TRAIN(%s): Speed is 0\n", STRING(pev->targetname) );
		StopSound();
		return;
	}

//	if ( !m_ppath )
//		m_ppath = CPathTrack::Instance(FIND_ENTITY_BY_TARGETNAME( NULL, STRING(pev->target) ));
	if ( !m_hPath )
	{	
		ALERT( at_aiconsole, "TRAIN(%s): Lost path\n", STRING(pev->targetname) );
		StopSound();
		return;
	}

	UpdateSound();

	Vector nextPos = pev->origin;

	nextPos.z -= m_height;
	CPathTrack *pnext = m_hPath->LookAhead( &nextPos, pev->speed * 0.1, 1 );
	nextPos.z += m_height;

	pev->velocity = (nextPos - pev->origin) * 10;
	Vector nextFront = pev->origin;

	nextFront.z -= m_height;
	if ( m_length > 0 )
		m_hPath->LookAhead( &nextFront, m_length, 0 );
	else
		m_hPath->LookAhead( &nextFront, 100, 0 );
	nextFront.z += m_height;

	Vector delta = nextFront - pev->origin;
	Vector angles = UTIL_VecToAngles( delta );
	// The train actually points west
	angles.y += 180;

	// !!!  All of this crap has to be done to make the angles not wrap around, revisit this.
	FixupAngles( angles );
	FixupAngles( pev->angles );

	if ( !pnext || (delta.x == 0 && delta.y == 0) )
		angles = pev->angles;

	float vy, vx;
	if ( !(pev->spawnflags & SF_TRACKTRAIN_NOPITCH) )
		vx = UTIL_AngleDistance( angles.x, pev->angles.x );
	else
		vx = 0;
	if ( pev->spawnflags & SF_TRACKTRAIN_NOYAW )
		vy = 0;
	else
		vy = UTIL_AngleDistance( angles.y, pev->angles.y );

	pev->avelocity.y = vy * 10;
	pev->avelocity.x = vx * 10;

	if ( m_flBank != 0 )
	{
		if ( pev->avelocity.y < -5 )
			pev->avelocity.z = UTIL_AngleDistance( UTIL_ApproachAngle( -m_flBank, pev->angles.z, m_flBank*2 ), pev->angles.z);
		else if ( pev->avelocity.y > 5 )
			pev->avelocity.z = UTIL_AngleDistance( UTIL_ApproachAngle( m_flBank, pev->angles.z, m_flBank*2 ), pev->angles.z);
		else
			pev->avelocity.z = UTIL_AngleDistance( UTIL_ApproachAngle( 0, pev->angles.z, m_flBank*4 ), pev->angles.z) * 4;
	}
		
	if ( pnext )
	{
		if ( pnext != m_hPath )
		{
			CPathTrack *pFire;

			if ( pev->speed < 0 )
				pFire = (CPathTrack *) m_hPath;
			else
				pFire = pnext;

			m_hPath = pnext;
			// Fire the pass target if there is one
			if ( pFire->pev->message )
			{
				FireTargets( STRING(pFire->pev->message), this, this, USE_TOGGLE, 0 );
				if ( pFire->pev->spawnflags & SF_PATH_FIREONCE )
					pFire->pev->message = 0;
			}

			if ( pFire->pev->spawnflags & SF_PATH_DISABLE_TRAIN )
				pev->spawnflags |= SF_TRACKTRAIN_NOCONTROL;

			float previousSpeed = pev->speed;

			if ( ( pev->spawnflags & SF_TRACKTRAIN_NOCONTROL ) && pFire->pev->speed != 0 )
				pev->speed = pFire->pev->speed;

			if ( pFire->m_flMaxSpeed >= 0 )
				SetMaxSpeed( pFire->m_flMaxSpeed );

			if ( pFire->m_flNewSpeed >= 0 )
				pev->speed = pFire->m_flNewSpeed < m_flMaxSpeed ? pFire->m_flNewSpeed : m_flMaxSpeed;

			if ( pev->speed != previousSpeed )
				ALERT( at_aiconsole, "TrackTrain %s speed to %4.2f\n", STRING(pev->targetname), pev->speed );

			if ( m_iszGoal && FStrEq( STRING( pFire->pev->targetname ), STRING( m_iszGoal ) ) )
			{
				StopSound();
				pev->velocity = g_vecZero;
				pev->speed = 0;
				m_oldSpeed = pev->speed;
				m_iszGoal = 0;
			}

		}
		SetThink( &CFuncTrackTrain::Next );
		NextThink( pev->ltime + time, TRUE );
	}
	else	// end of path, stop
	{
		StopSound();
		pev->velocity = (nextPos - pev->origin);
		pev->avelocity = g_vecZero;
		float distance = pev->velocity.Length();
		m_oldSpeed = pev->speed;


		pev->speed = 0;
		m_hDeadEndPath = FindDeadEnd();

		// Move to the dead end
		
		// Are we there yet?
		if ( distance > 0 )
		{
			// no, how long to get there?
			time = distance / m_oldSpeed;
			pev->velocity = pev->velocity * (m_oldSpeed / distance);
			SetThink( &CFuncTrackTrain::DeadEnd );
			NextThink( pev->ltime + time, FALSE );
		}
		else
		{
			DeadEnd();
		}
	}
}

void CFuncTrackTrain::DeadEnd()
{
	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;

	CPathTrack *pTrack = m_hDeadEndPath;
	if ( pTrack )
	{
		ALERT( at_aiconsole, "at %s\n", STRING(pTrack->pev->targetname) );
		if ( pTrack->pev->netname )
			FireTargets( STRING(pTrack->pev->netname), this, this, USE_TOGGLE, 0 );

		m_hDeadEndPath = NULL;
	}
	else
		ALERT( at_aiconsole, "\n" );
}

CPathTrack *CFuncTrackTrain::FindDeadEnd()
{
	CPathTrack *path = m_hPath;

	ALERT( at_aiconsole, "TRAIN(%s): Dead end ", STRING( pev->targetname ) );

	if ( !path )
		return NULL;

	struct VisitedNode
	{
		VisitedNode               *next;
		CEntityHandle<CPathTrack>  path;
	};

	VisitedNode *head = new VisitedNode();
	VisitedNode *tail = head;

	head->path = path;
	path->m_bVisited = true;

	for ( ;; )
	{
		CPathTrack *next = m_oldSpeed < 0 ? path->GetPrevious() : path->GetNext();

		if ( !path->ValidPath( next, 1 ) )
			break;

		if ( next->m_bVisited )
		{
			path = NULL;
			break;
		}

		VisitedNode *entry = new VisitedNode();

		tail->next = entry;
		tail = entry;
		entry->path = next;
		next->m_bVisited = true;
		path = next;
	}

	while ( head )
	{
		VisitedNode *next = head->next;

		head->path->m_bVisited = false;
		delete head;
		head = next;
	}

	return path;
}

bool CFuncTrackTrain::ShouldDisablePlayerCollisions( void ) const
{
	return true;
}

void CFuncTrackTrain :: SetControls( entvars_t *pevControls )
{
	Vector offset = pevControls->origin - pev->oldorigin;

	m_controlMins = pevControls->mins + offset;
	m_controlMaxs = pevControls->maxs + offset;
}


BOOL CFuncTrackTrain :: OnControls( entvars_t *pevTest )
{
	Vector offset = pevTest->origin - pev->origin;

	if ( pev->spawnflags & SF_TRACKTRAIN_NOCONTROL )
		return FALSE;

	// Transform offset into local coordinates
	UTIL_MakeVectors( pev->angles );
	Vector local;
	local.x = DotProduct( offset, gpGlobals->v_forward );
	local.y = -DotProduct( offset, gpGlobals->v_right );
	local.z = DotProduct( offset, gpGlobals->v_up );

	if ( local.x >= m_controlMins.x && local.y >= m_controlMins.y && local.z >= m_controlMins.z &&
		 local.x <= m_controlMaxs.x && local.y <= m_controlMaxs.y && local.z <= m_controlMaxs.z )
		 return TRUE;

	return FALSE;
}


void CFuncTrackTrain :: Find( void )
{
	m_hPath = CPathTrack::Instance(FIND_ENTITY_BY_TARGETNAME( NULL, STRING(pev->target) ));
	if ( !m_hPath )
		return;

	entvars_t *pevTarget = m_hPath->pev;
	if ( !FClassnameIs( pevTarget, "path_track" ) )
	{
		ALERT( at_error, "func_track_train must be on a path of path_track\n" );
		m_hPath = NULL;
		return;
	}

	Vector nextPos = pevTarget->origin;
	nextPos.z += m_height;

	Vector look = nextPos;
	look.z -= m_height;
	m_hPath->LookAhead( &look, m_length, 0 );
	look.z += m_height;

	pev->angles = UTIL_VecToAngles( look - nextPos );
	// The train actually points west
	pev->angles.y += 180;

	if ( pev->spawnflags & SF_TRACKTRAIN_NOPITCH )
		pev->angles.x = 0;
    UTIL_SetOrigin( pev, nextPos );
	NextThink( pev->ltime + 0.1, FALSE );
	SetThink( &CFuncTrackTrain::Next );
	pev->speed = m_startSpeed;

	UpdateSound();
}


void CFuncTrackTrain :: NearestPath( void )
{
	CBaseEntity *pTrack = NULL;
	CBaseEntity *pNearest = NULL;
	float dist, closest;

	closest = 1024;

	while ((pTrack = UTIL_FindEntityInSphere( pTrack, pev->origin, 1024 )) != NULL)
	{
		// filter out non-tracks
		if ( !(pTrack->pev->flags & (FL_CLIENT|FL_MONSTER)) && FClassnameIs( pTrack->pev, "path_track" ) )
		{
			dist = (pev->origin - pTrack->pev->origin).Length();
			if ( dist < closest )
			{
				closest = dist;
				pNearest = pTrack;
			}
		}
	}

	if ( !pNearest )
	{
		ALERT( at_console, "Can't find a nearby track !!!\n" );
		SetThink(NULL);
		return;
	}

	ALERT( at_aiconsole, "TRAIN: %s, Nearest track is %s\n", STRING(pev->targetname), STRING(pNearest->pev->targetname) );
	// If I'm closer to the next path_track on this path, then it's my real path
	pTrack = ((CPathTrack *)pNearest)->GetNext();
	if ( pTrack )
	{
		if ( (pev->origin - pTrack->pev->origin).Length() < (pev->origin - pNearest->pev->origin).Length() )
			pNearest = pTrack;
	}

	m_hPath = (CPathTrack *)pNearest;

	if ( pev->speed != 0 )
	{
		NextThink( pev->ltime + 0.1, FALSE );
		SetThink( &CFuncTrackTrain::Next );
	}
}


void CFuncTrackTrain::OverrideReset( void )
{
	NextThink( pev->ltime + 0.1, FALSE );
	SetThink( &CFuncTrackTrain::NearestPath );
}


CFuncTrackTrain *CFuncTrackTrain::Instance( edict_t *pent )
{ 
	if ( FClassnameIs( pent, "func_tracktrain" ) )
		return (CFuncTrackTrain *)GET_PRIVATE(pent);
	return NULL;
}

/*QUAKED func_train (0 .5 .8) ?
Trains are moving platforms that players can ride.
The targets origin specifies the min point of the train at each corner.
The train spawns at the first target it is pointing at.
If the train is the target of a button or trigger, it will not begin moving until activated.
speed	default 100
dmg		default	2
sounds
1) ratchet metal
*/

void CFuncTrackTrain :: Spawn( void )
{
	if ( pev->speed == 0 )
		m_flMaxSpeed = 100;
	else
		m_flMaxSpeed = pev->speed;

	pev->impulse = m_flMaxSpeed;
	pev->speed = V_min( 0.0f, m_flMaxSpeed );
	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;

	m_dir = 1;

	if ( FStringNull(pev->target) )
		ALERT( at_console, "func_tracktrain with no target" );

	if ( pev->spawnflags & SF_TRACKTRAIN_PASSABLE )
		pev->solid = SOLID_NOT;
	else
		pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	
	SET_MODEL( ENT(pev), STRING(pev->model) );

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( pev, pev->origin );

	// Cache off placed origin for train controls
	pev->oldorigin = pev->origin;
	pev->angles.x += 90;

	m_controlMins = pev->mins;
	m_controlMaxs = pev->maxs;
	m_controlMaxs.z += 72;
// start trains on the next frame, to make sure their targets have had
// a chance to spawn/activate
	NextThink( pev->ltime + 0.1, FALSE );
	SetThink( &CFuncTrackTrain::Find );
	Precache();
}

void CFuncTrackTrain :: Precache()
{
	CBaseEntity::Precache();

	if (m_flVolume == 0 )
		m_flVolume = 1.0;

	if ( !pev->noise )
	{
		const char *pszSound = TrainMovingSound( m_sounds );

		if ( pszSound )
			pev->noise = MAKE_STRING( pszSound );
		else
			pev->noise = 0;
	}

	if ( pev->noise )
		PrecacheSound_Intercept( this, STRING( pev->noise ) );

	if ( !pev->noise1 )
		pev->noise1 = MAKE_STRING("plats/ttrain_brake1.wav");

	if ( !pev->noise2 )
		pev->noise2 = MAKE_STRING( "plats/ttrain_start1.wav" );

	PrecacheSound_Intercept( this, STRING( pev->noise1 ) );
	PrecacheSound_Intercept( this, STRING( pev->noise2 ) );

	m_usAdjustPitch = PRECACHE_EVENT( 1, "events/train.sc" );
}

// This class defines the volume of space that the player must stand in to control the train
class CFuncTrainControls : public CBaseEntity
{
public:
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Spawn( void );
	void EXPORT Find( void );
};
LINK_ENTITY_TO_CLASS( func_traincontrols, CFuncTrainControls );


void CFuncTrainControls :: Find( void )
{
	edict_t *pTarget = NULL;

	do 
	{
		pTarget = FIND_ENTITY_BY_TARGETNAME( pTarget, STRING(pev->target) );
	} while ( !FNullEnt(pTarget) && !FClassnameIs(pTarget, "func_tracktrain") );

	if ( FNullEnt( pTarget ) )
	{
		ALERT( at_console, "No train %s\n", STRING(pev->target) );
		return;
	}

	CFuncTrackTrain *ptrain = CFuncTrackTrain::Instance(pTarget);
	ptrain->SetControls( pev );
	UTIL_Remove( this );
}


void CFuncTrainControls :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	SET_MODEL( ENT(pev), STRING(pev->model) );

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( pev, pev->origin );
	
	SetThink(&CFuncTrainControls::Find );
	pev->nextthink = gpGlobals->time;
}



// ----------------------------------------------------------------------------
//
// Track changer / Train elevator
//
// ----------------------------------------------------------------------------

#define SF_TRACK_ACTIVATETRAIN		0x00000001
#define SF_TRACK_RELINK				0x00000002
#define SF_TRACK_ROTMOVE			0x00000004
#define SF_TRACK_STARTBOTTOM		0x00000008
#define SF_TRACK_DONT_MOVE			0x00000010

//
// This entity is a rotating/moving platform that will carry a train to a new track.
// It must be larger in X-Y planar area than the train, since it must contain the
// train within these dimensions in order to operate when the train is near it.
//

typedef enum { TRAIN_SAFE, TRAIN_BLOCKING, TRAIN_FOLLOWING } TRAIN_CODE;

class CFuncTrackChange : public CFuncPlatRot
{
public:
	CPathTrack *FindPathTrack( const char *name );
	void Spawn( void );
	void Precache( void );

//	virtual void	Blocked( void );
	virtual void	EXPORT GoUp( void );
	virtual void	EXPORT GoDown( void );

	void			KeyValue( KeyValueData* pkvd );
	void			Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void			EXPORT Find( void );
	TRAIN_CODE		EvaluateTrain( CPathTrack *pcurrent );
	void			UpdateTrain( Vector &dest );
	virtual void	HitBottom( void );
	virtual void	HitTop( void );
	void			Touch( CBaseEntity *pOther );
	virtual void	UpdateAutoTargets( int toggleState );
	virtual	BOOL	IsTogglePlat( void ) { return TRUE; }

	void			DisableUse( void ) { m_use = 0; }
	void			EnableUse( void ) { m_use = 1; }
	int				UseEnabled( void ) { return m_use; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	virtual void	OverrideReset( void );


	CEntityHandle<CPathTrack> m_hTrackTop;
	CEntityHandle<CPathTrack> m_hTrackBottom;

	CEntityHandle<CFuncTrackTrain> m_hTrain;

	int				m_trackTopName;
	int				m_trackBottomName;
	int				m_trainName;
	TRAIN_CODE		m_code;
	int				m_targetState;
	int				m_use;
};
LINK_ENTITY_TO_CLASS( func_trackchange, CFuncTrackChange );

TYPEDESCRIPTION	CFuncTrackChange::m_SaveData[] = 
{
	DEFINE_GLOBAL_FIELD( CFuncTrackChange, m_hTrackTop, FIELD_EHANDLE ),
	DEFINE_GLOBAL_FIELD( CFuncTrackChange, m_hTrackBottom, FIELD_EHANDLE ),
	DEFINE_GLOBAL_FIELD( CFuncTrackChange, m_hTrain, FIELD_EHANDLE ),
	DEFINE_GLOBAL_FIELD( CFuncTrackChange, m_trackTopName, FIELD_STRING ),
	DEFINE_GLOBAL_FIELD( CFuncTrackChange, m_trackBottomName, FIELD_STRING ),
	DEFINE_GLOBAL_FIELD( CFuncTrackChange, m_trainName, FIELD_STRING ),
	DEFINE_FIELD( CFuncTrackChange, m_code, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncTrackChange, m_targetState, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncTrackChange, m_use, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CFuncTrackChange, CFuncPlatRot );

void CFuncTrackChange :: Spawn( void )
{
	Setup();
	if ( FBitSet( pev->spawnflags, SF_TRACK_DONT_MOVE ) )
		m_vecPosition2.z = pev->origin.z;

	SetupRotation();

	if ( FBitSet( pev->spawnflags, SF_TRACK_STARTBOTTOM ) )
	{
		UTIL_SetOrigin (pev, m_vecPosition2);
		m_toggle_state = TS_AT_BOTTOM;
		pev->angles = m_start;
		m_targetState = TS_AT_TOP;
	}
	else
	{
		UTIL_SetOrigin (pev, m_vecPosition1);
		m_toggle_state = TS_AT_TOP;
		pev->angles = m_end;
		m_targetState = TS_AT_BOTTOM;
	}

	EnableUse();
	pev->nextthink = pev->ltime + 2.0;
	SetThink( &CFuncTrackChange::Find );
	Precache();
}

void CFuncTrackChange :: Precache( void )
{
	// Can't trigger sound
	PRECACHE_SOUND( "buttons/button11.wav" );
	
	CFuncPlatRot::Precache();
}


// UNDONE: Filter touches before re-evaluating the train.
void CFuncTrackChange :: Touch( CBaseEntity *pOther )
{
#if 0
	TRAIN_CODE code;
	entvars_t *pevToucher = pOther->pev;
#endif
}



void CFuncTrackChange :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq(pkvd->szKeyName, "train") )
	{
		m_trainName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "toptrack") )
	{
		m_trackTopName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "bottomtrack") )
	{
		m_trackBottomName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		CFuncPlatRot::KeyValue( pkvd );		// Pass up to base class
	}
}


void CFuncTrackChange::OverrideReset( void )
{
	pev->nextthink = pev->ltime + 1.0;
	SetThink( &CFuncTrackChange::Find );
}

CPathTrack *CFuncTrackChange::FindPathTrack( const char *name )
{
	edict_t *target = NULL;

	while ( ( target = FIND_ENTITY_BY_TARGETNAME( target, name ) ) != NULL )
	{
		CPathTrack *path = !target->free ? CPathTrack::Instance( target ) : NULL;

		if ( path )
			return path;
	}

	return NULL;
}

void CFuncTrackChange :: Find( void )
{
	edict_t *target = FIND_ENTITY_BY_TARGETNAME( NULL, STRING( m_trainName ) );

	m_hTrain = target && !target->free ? CFuncTrackTrain::Instance( target ) : NULL;

	if ( !m_hTrain )
	{
		ALERT( at_error, "Can't find train for track change! %s\n", STRING( m_trainName ) );
		return;
	}

	m_hTrackTop = FindPathTrack( STRING( m_trackTopName ) );

	if ( !m_hTrackTop )
	{
		ALERT( at_error, "Can't find top track for track change! %s\n", STRING( m_trackTopName ) );
		return;
	}

	m_hTrackBottom = FindPathTrack( STRING( m_trackBottomName ) );

	if ( !m_hTrackBottom )
	{
		ALERT( at_error, "Can't find bottom track for track change! %s\n", STRING( m_trackBottomName ) );
		return;
	}

	Vector center = ( pev->absmin + pev->absmax ) * 0.5;

	m_hTrackBottom = m_hTrackBottom->Nearest( center );
	m_hTrackTop = m_hTrackTop->Nearest( center );
	UpdateAutoTargets( m_toggle_state );
	SetThink( NULL );
}


TRAIN_CODE CFuncTrackChange :: EvaluateTrain( CPathTrack *pcurrent )
{
	// Go ahead and work, we don't have anything to switch, so just be an elevator
	if ( !pcurrent || !m_hTrain )
		return TRAIN_SAFE;

	if ( m_hTrain->m_hPath != pcurrent )
	{
		if ( !pcurrent->m_hPrevious )
			return TRAIN_SAFE;

		if ( m_hTrain->m_hPath != pcurrent->m_hPrevious &&
			 ( !pcurrent->m_hNext || m_hTrain->m_hPath != pcurrent->m_hNext ) )
			return TRAIN_SAFE;
	}

	if ( m_hTrain->pev->speed != 0 )
		return TRAIN_BLOCKING;

	Vector dist = pev->origin - m_hTrain->pev->origin;
	float length = dist.Length2D();

	if ( length < m_hTrain->m_length )		// Empirically determined close distance
		return TRAIN_FOLLOWING;

	return length <= ( 150 + m_hTrain->m_length ) ? TRAIN_BLOCKING : TRAIN_SAFE;
}


void CFuncTrackChange :: UpdateTrain( Vector &dest )
{
	float time = (pev->nextthink - pev->ltime);

	m_hTrain->pev->velocity = pev->velocity;
	m_hTrain->pev->avelocity = pev->avelocity;
	m_hTrain->NextThink( m_hTrain->pev->ltime + time, FALSE );

	// Attempt at getting the train to rotate properly around the origin of the trackchange
	if ( !( time > 0 ) )
		return;

	Vector offset = m_hTrain->pev->origin - pev->origin;
	Vector delta = dest - pev->angles;
	// Transform offset into local coordinates
	UTIL_MakeInvVectors( delta, gpGlobals );
	Vector local;
	local.x = DotProduct( offset, gpGlobals->v_forward );
	local.y = DotProduct( offset, gpGlobals->v_right );
	local.z = DotProduct( offset, gpGlobals->v_up );

	local = local - offset;
	m_hTrain->pev->velocity = pev->velocity + (local * (1.0/time));
}

void CFuncTrackChange :: GoDown( void )
{
	if ( m_code == TRAIN_BLOCKING )
		return;

	// HitBottom may get called during CFuncPlat::GoDown(), so set up for that
	// before you call GoDown()

	UpdateAutoTargets( TS_GOING_DOWN );
	// If ROTMOVE, move & rotate
	if ( FBitSet( pev->spawnflags, SF_TRACK_DONT_MOVE ) )
	{
		SetMoveDone( &CFuncTrackChange::CallHitBottom );
		m_toggle_state = TS_GOING_DOWN;
		AngularMove( m_start, pev->speed );
	}
	else
	{
		CFuncPlat :: GoDown();
		SetMoveDone( &CFuncTrackChange::CallHitBottom );
		RotMove( m_start, pev->nextthink - pev->ltime );
	}
	// Otherwise, rotate first, move second

	// If the train is moving with the platform, update it
	if ( m_code == TRAIN_FOLLOWING )
	{
		UpdateTrain( m_start );
		m_hTrain->m_hPath = NULL;
	}
}


//
// Platform is at bottom, now starts moving up
//
void CFuncTrackChange :: GoUp( void )
{
	if ( m_code == TRAIN_BLOCKING )
		return;

	// HitTop may get called during CFuncPlat::GoUp(), so set up for that
	// before you call GoUp();

	UpdateAutoTargets( TS_GOING_UP );
	if ( FBitSet( pev->spawnflags, SF_TRACK_DONT_MOVE ) )
	{
		m_toggle_state = TS_GOING_UP;
		SetMoveDone( &CFuncTrackChange::CallHitTop );
		AngularMove( m_end, pev->speed );
	}
	else
	{
		// If ROTMOVE, move & rotate
		CFuncPlat :: GoUp();
		SetMoveDone( &CFuncTrackChange::CallHitTop );
		RotMove( m_end, pev->nextthink - pev->ltime );
	}
	
	// Otherwise, move first, rotate second

	// If the train is moving with the platform, update it
	if ( m_code == TRAIN_FOLLOWING )
	{
		UpdateTrain( m_end );
		m_hTrain->m_hPath = NULL;
	}
}


// Normal track change
void CFuncTrackChange :: UpdateAutoTargets( int toggleState )
{
	if ( !m_hTrackTop || !m_hTrackBottom )
		return;

	if ( toggleState == TS_AT_TOP )
		ClearBits( m_hTrackTop->pev->spawnflags, SF_PATH_DISABLED );
	else
		SetBits( m_hTrackTop->pev->spawnflags, SF_PATH_DISABLED );

	if ( toggleState == TS_AT_BOTTOM )
		ClearBits( m_hTrackBottom->pev->spawnflags, SF_PATH_DISABLED );
	else
		SetBits( m_hTrackBottom->pev->spawnflags, SF_PATH_DISABLED );
}


void CFuncTrackChange :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_toggle_state != TS_AT_TOP && m_toggle_state != TS_AT_BOTTOM )
		return;

	// If train is in "safe" area, but not on the elevator, play alarm sound
	if ( m_toggle_state == TS_AT_TOP )
		m_code = EvaluateTrain( m_hTrackTop );
	else if ( m_toggle_state == TS_AT_BOTTOM )
		m_code = EvaluateTrain( m_hTrackBottom );
	else
		m_code = TRAIN_BLOCKING;
	if ( m_code == TRAIN_BLOCKING )
	{
		// Play alarm and return
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "buttons/button11.wav", 1, ATTN_NORM);
		return;
	}

	// Otherwise, it's safe to move
	// If at top, go down
	// at bottom, go up

	DisableUse();
	if (m_toggle_state == TS_AT_TOP)
		GoDown();
	else
		GoUp();
}


//
// Platform has hit bottom.  Stops and waits forever.
//
void CFuncTrackChange :: HitBottom( void )
{
	CFuncPlatRot :: HitBottom();
	if ( m_code == TRAIN_FOLLOWING )
	{
//		UpdateTrain();
		m_hTrain->SetTrack( m_hTrackBottom );
		m_hTrain->pev->speed = 0;
	}
	SetThink( NULL );
	pev->nextthink = -1;

	UpdateAutoTargets( m_toggle_state );

	EnableUse();
}


//
// Platform has hit bottom.  Stops and waits forever.
//
void CFuncTrackChange :: HitTop( void )
{
	CFuncPlatRot :: HitTop();
	if ( m_code == TRAIN_FOLLOWING )
	{
//		UpdateTrain();
		m_hTrain->SetTrack( m_hTrackTop );
		m_hTrain->pev->speed = 0;
	}
	
	// Don't let the plat go back down
	SetThink( NULL );
	pev->nextthink = -1;
	UpdateAutoTargets( m_toggle_state );
	EnableUse();
}



class CFuncTrackAuto : public CFuncTrackChange
{
public:
	void			Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void	UpdateAutoTargets( int toggleState );
};

LINK_ENTITY_TO_CLASS( func_trackautochange, CFuncTrackAuto );

// Auto track change
void CFuncTrackAuto :: UpdateAutoTargets( int toggleState )
{
	CPathTrack *pTarget, *pNextTarget;

	if ( !m_hTrackTop || !m_hTrackBottom )
		return;

	if ( m_targetState == TS_AT_TOP )
	{
		pTarget = m_hTrackTop->GetNext();
		pNextTarget = m_hTrackBottom->GetNext();
	}
	else
	{
		pTarget = m_hTrackBottom->GetNext();
		pNextTarget = m_hTrackTop->GetNext();
	}
	if ( pTarget )
	{
		ClearBits( pTarget->pev->spawnflags, SF_PATH_DISABLED );
		if ( m_code == TRAIN_FOLLOWING && m_hTrain && m_hTrain->pev->speed == 0 )
			m_hTrain->Use( this, this, USE_ON, 0 );
	}

	if ( pNextTarget )
		SetBits( pNextTarget->pev->spawnflags, SF_PATH_DISABLED );

}


void CFuncTrackAuto :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CPathTrack *pTarget;

	if ( !UseEnabled() )
		return;

	if ( m_toggle_state == TS_AT_TOP )
		pTarget = m_hTrackTop;
	else if ( m_toggle_state == TS_AT_BOTTOM )
		pTarget = m_hTrackBottom;
	else
		pTarget = NULL;

	if ( FClassnameIs( pActivator->pev, "func_tracktrain" ) )
	{
		m_code = EvaluateTrain( pTarget );
		// Safe to fire?
		if ( m_code == TRAIN_FOLLOWING && m_toggle_state != m_targetState )
		{
			DisableUse();
			if (m_toggle_state == TS_AT_TOP)
				GoDown();
			else
				GoUp();
		}
	}
	else
	{
		if ( pTarget )
			pTarget = pTarget->GetNext();
		if ( pTarget && m_hTrain->m_hPath != pTarget && ShouldToggle( useType, m_targetState ) )
		{
			if ( m_targetState == TS_AT_TOP )
				m_targetState = TS_AT_BOTTOM;
			else
				m_targetState = TS_AT_TOP;
		}

		UpdateAutoTargets( m_targetState );
	}
}


// ----------------------------------------------------------
//
//
// pev->speed is the travel speed
// pev->health is current health
// pev->max_health is the amount to reset to each time it starts

#define FGUNTARGET_START_ON			0x0001

class CGunTarget : public CBaseMonster
{
public:
	void			Spawn( void );
	void			Activate( void );
	void EXPORT		Next( void );
	void EXPORT		Start( void );
	void EXPORT		Wait( void );
	void			Stop( void );

	int				BloodColor( void ) { return DONT_BLEED; }
	int				Classify( void ) { return CLASS_MACHINE; }
	int				TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void			Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	Vector			BodyTarget( const Vector &posSrc ) { return pev->origin; }

	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

private:
	bool			m_on;
};


LINK_ENTITY_TO_CLASS( func_guntarget, CGunTarget );

TYPEDESCRIPTION	CGunTarget::m_SaveData[] = 
{
	DEFINE_FIELD( CGunTarget, m_on, FIELD_CHARACTER ),
};

IMPLEMENT_SAVERESTORE( CGunTarget, CBaseMonster );


void CGunTarget::Spawn( void )
{
	pev->solid		= SOLID_BSP;
	pev->movetype	= MOVETYPE_PUSH;

	UTIL_SetOrigin(pev, pev->origin);
	SET_MODEL(ENT(pev), STRING(pev->model) );

	if ( pev->speed == 0 )
		pev->speed = 100;

	// Don't take damage until "on"
	pev->takedamage = DAMAGE_NO;
	pev->flags |= FL_MONSTER;

	m_on = FALSE;
	pev->max_health = pev->health;

	if ( pev->spawnflags & FGUNTARGET_START_ON )
	{
		SetThink( &CGunTarget::Start );
		pev->nextthink = pev->ltime + 0.3;
	}
}


void CGunTarget::Activate( void )
{
	CBaseEntity	*pTarg;

	// now find our next target
	pTarg = GetNextTarget();
	if ( pTarg )
	{
		m_hTargetEnt = pTarg;
		UTIL_SetOrigin( pev, pTarg->pev->origin - (pev->mins + pev->maxs) * 0.5 );
	}
}


void CGunTarget::Start( void )
{
	Use( this, this, USE_ON, 0 );
}


void CGunTarget::Next( void )
{
	SetThink( NULL );

	m_hTargetEnt = GetNextTarget();
	CBaseEntity *pTarget = m_hTargetEnt;
	
	if ( !pTarget )
	{
		Stop();
		return;
	}
	SetMoveDone( &CGunTarget::Wait );
	LinearMove( pTarget->pev->origin - (pev->mins + pev->maxs) * 0.5, pev->speed );
}


void CGunTarget::Wait( void )
{
	CBaseEntity *pTarget = m_hTargetEnt;
	
	if ( !pTarget )
	{
		Stop();
		return;
	}

	// Fire the pass target if there is one
	if ( pTarget->pev->message )
	{
		FireTargets( STRING(pTarget->pev->message), this, this, USE_TOGGLE, 0 );
		if ( FBitSet( pTarget->pev->spawnflags, SF_CORNER_FIREONCE ) )
			pTarget->pev->message = 0;
	}
		
	m_flWait = pTarget->GetDelay();

	pev->target = pTarget->pev->target;
	SetThink( &CGunTarget::Next );
	if (m_flWait != 0)
	{// -1 wait will wait forever!		
		pev->nextthink = pev->ltime + m_flWait;
	}
	else
	{
		Next();// do it RIGHT now!
	}
}


void CGunTarget::Stop( void )
{
	pev->velocity = g_vecZero;
	pev->nextthink = 0;
	pev->takedamage = DAMAGE_NO;
}


int	CGunTarget::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( pev->health > 0 )
	{
		pev->health -= flDamage;
		if ( pev->health <= 0 )
		{
			pev->health = 0;
			Stop();
			if ( pev->message )
				FireTargets( STRING(pev->message), this, this, USE_TOGGLE, 0 );
		}
	}
	return 0;
}


void CGunTarget::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_on ) )
		return;

	if ( m_on )
	{
		Stop();
	}
	else
	{
		pev->takedamage = DAMAGE_AIM;
		m_hTargetEnt = GetNextTarget();
		if ( m_hTargetEnt == NULL )
			return;
		pev->health = pev->max_health;
		Next();
	}
}



