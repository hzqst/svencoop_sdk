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
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "PickupUtils.h"

#define SF_AIRTANK_NO_DAMAGE		1
#define SF_AIRTANK_START_EMPTY	2

#define SF_ITEM_TOUCH_ONLY		128
#define SF_ITEM_USE_WITHOUT_LOS	512

#define AIRTANK_TOUCH_DELAY	3

class CAirtank : public CGrenade
{
	void Spawn( void );
	void Precache( void );
	int ObjectCaps( void );
	void EXPORT TankThink( void );
	void EXPORT TankTouch( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void GiveAir( CBaseEntity *pOther );
	int	 BloodColor( void ) { return DONT_BLEED; };
	void Killed( entvars_t *pevAttacker, int iGib );
	void KeyValue( KeyValueData *pkvd );

	virtual int		Save( CSave &save ); 
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];

	float		m_flLastTouchTime;
	int			m_iState;
	float		m_flAirTimeGiven;
	float		m_flRefillDelay;

	string_t	m_szSoundOnConsume;
	string_t	m_szSoundOnIsEmpty;
	string_t	m_szSoundOnRefill;
	string_t	m_szTriggerOnConsume;
	string_t	m_szTriggerOnIsEmpty;
	string_t	m_szTriggerOnRefill;
	string_t	m_szTriggerOnDestroy;
};


LINK_ENTITY_TO_CLASS( item_airtank, CAirtank );
TYPEDESCRIPTION	CAirtank::m_SaveData[] = 
{
	DEFINE_FIELD( CAirtank, m_flLastTouchTime, FIELD_FLOAT ),
	DEFINE_FIELD( CAirtank, m_iState, FIELD_INTEGER ),
	DEFINE_FIELD( CAirtank, m_flAirTimeGiven, FIELD_FLOAT ),
	DEFINE_FIELD( CAirtank, m_flRefillDelay, FIELD_FLOAT ),
	DEFINE_FIELD( CAirtank, m_szSoundOnConsume, FIELD_STRING ),
	DEFINE_FIELD( CAirtank, m_szSoundOnIsEmpty, FIELD_STRING ),
	DEFINE_FIELD( CAirtank, m_szSoundOnRefill, FIELD_STRING ),
	DEFINE_FIELD( CAirtank, m_szTriggerOnConsume, FIELD_STRING ),
	DEFINE_FIELD( CAirtank, m_szTriggerOnIsEmpty, FIELD_STRING ),
	DEFINE_FIELD( CAirtank, m_szTriggerOnRefill, FIELD_STRING ),
	DEFINE_FIELD( CAirtank, m_szTriggerOnDestroy, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CAirtank, CGrenade );


int CAirtank::ObjectCaps( void )
{
	if ( pev->spawnflags & SF_ITEM_TOUCH_ONLY )
		return FCAP_ACROSS_TRANSITION;

	return FCAP_ACROSS_TRANSITION | FCAP_IMPULSE_USE;
}

void CAirtank::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "air_time_given" ) )
	{
		m_flAirTimeGiven = (float) V_max( 0.0, atof( pkvd->szValue ) );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "refill_delay" ) )
	{
		m_flRefillDelay = (float) V_max( -1.0, atof( pkvd->szValue ) );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "target_on_consume" ) )
	{
		if ( *pkvd->szValue )
			m_szTriggerOnConsume = ALLOC_STRING( pkvd->szValue );

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "target_on_is_empty" ) )
	{
		if ( *pkvd->szValue )
			m_szTriggerOnIsEmpty = ALLOC_STRING( pkvd->szValue );

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "target_on_refill" ) )
	{
		if ( *pkvd->szValue )
			m_szTriggerOnRefill = ALLOC_STRING( pkvd->szValue );

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "target_on_destroy" ) )
	{
		if ( *pkvd->szValue )
			m_szTriggerOnDestroy = ALLOC_STRING( pkvd->szValue );

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sound_on_consume" ) )
	{
		if ( *pkvd->szValue )
			m_szSoundOnConsume = ALLOC_STRING( pkvd->szValue );

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sound_on_is_empty" ) )
	{
		if ( *pkvd->szValue )
			m_szSoundOnIsEmpty = ALLOC_STRING( pkvd->szValue );

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sound_on_refill" ) )
	{
		if ( *pkvd->szValue )
			m_szSoundOnRefill = ALLOC_STRING( pkvd->szValue );

		pkvd->fHandled = TRUE;
	}
	else
	{
		CGrenade::KeyValue( pkvd );
	}
}


void CAirtank :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	if ( SetupModel() )
		SET_MODEL( ENT(pev), STRING( pev->model ) );
	else
		SET_MODEL(ENT(pev), "models/w_oxygen.mdl");

	UTIL_SetSize(pev, Vector( -16, -16, 0), Vector(16, 16, 36));
	UTIL_SetOrigin( pev, pev->origin );

	SetTouch( &CAirtank::TankTouch );
	SetThink( &CAirtank::TankThink );
	SetUse( &CBaseEntity::Use );

	pev->flags |= FL_MONSTER;
	pev->takedamage		= ( pev->spawnflags & SF_AIRTANK_NO_DAMAGE ) ? DAMAGE_NO : DAMAGE_YES;

	m_flLastTouchTime = 0;

	if ( pev->spawnflags & SF_AIRTANK_START_EMPTY )
		m_iState = -1;
	else
		m_iState = 1;

	if ( pev->health <= 0 )
		pev->health = 20;

	if ( pev->dmg <= 0 )
		pev->dmg = 50;

	if ( m_flAirTimeGiven == 0 )
		m_flAirTimeGiven = 12;

	if ( m_flRefillDelay == 0 )
		m_flRefillDelay = 30;
}

void CAirtank::Precache( void )
{
	UTIL_PrecacheModel( this, pev->model ? STRING( pev->model ) : "models/w_oxygen.mdl" );

	CBaseEntity::Precache();

	PrecacheSound_Intercept( this, m_szSoundOnConsume ? STRING( m_szSoundOnConsume ) : "doors/aliendoor3.wav" );
	PrecacheSound_Intercept( this, m_szSoundOnIsEmpty ? STRING( m_szSoundOnIsEmpty ) : "player/pl_swim2.wav" );
	PrecacheSound_Intercept( this, m_szSoundOnRefill ? STRING( m_szSoundOnRefill ) : "items/airtank1.wav" );
}


void CAirtank :: Killed( entvars_t *pevAttacker, int iGib )
{
	if ( pevAttacker )
		pev->owner = ENT( pevAttacker );
	else
		pev->owner = NULL;

	// UNDONE: this should make a big bubble cloud, not an explosion

	if ( !( pev->spawnflags & SF_AIRTANK_NO_DAMAGE ) )
		Explode( pev->origin, Vector( 0, 0, -1 ) );

	if ( m_szTriggerOnDestroy )
		FireTargets( STRING( m_szTriggerOnDestroy ), this, this, USE_TOGGLE, 0 );
}


void CAirtank::TankThink( void )
{
	if ( m_iState == -1 || m_iState == 1 )
		return;

	// Fire trigger
	m_iState = 1;
	SUB_UseTargets( this, USE_TOGGLE, 0 );

	if ( m_szTriggerOnRefill )
		FireTargets( STRING( m_szTriggerOnRefill ), this, this, USE_TOGGLE, 0 );

	EMIT_SOUND( ENT(pev), CHAN_BODY, m_szSoundOnRefill ? STRING( m_szSoundOnRefill ) : "items/airtank1.wav", 1.0, ATTN_NORM );
}


void CAirtank::TankTouch( CBaseEntity *pOther )
{
	if ( m_flLastTouchTime >= 0 && gpGlobals->time - m_flLastTouchTime < AIRTANK_TOUCH_DELAY )
		return;

	if ( !pOther || !pOther->IsPlayer() )
		return;

	if ( pev->spawnflags & SF_ITEM_USE_ONLY )
		return;

	m_flLastTouchTime = gpGlobals->time;
	GiveAir( pOther );
}


void CAirtank::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	switch ( useType )
	{
	case USE_OFF:
		m_iState = -1;
		pev->nextthink = gpGlobals->time;
		break;

	case USE_ON:
		m_iState = 0;
		pev->nextthink = gpGlobals->time;
		break;

	case USE_SET:
		if ( !pActivator || !pActivator->IsPlayer() || value < 1 )
			return;

		if ( pev->spawnflags & SF_ITEM_TOUCH_ONLY )
			return;

		if ( !( pev->spawnflags & SF_ITEM_USE_WITHOUT_LOS ) && !UTIL_HasLineOfSight( pActivator, this, false, false ) )
			return;

		GiveAir( pActivator );
		break;

	case USE_TOGGLE:
		if ( pActivator && pActivator->IsPlayer() )
			GiveAir( pActivator );
		break;

	case USE_KILL:
		if ( value < 1 || pev->takedamage == DAMAGE_NO )
			UTIL_Remove( this );
		else if ( pActivator )
			Killed( pActivator->pev, GIB_NORMAL );
		else
			Killed( NULL, GIB_NORMAL );
		break;
	}
}


void CAirtank::GiveAir( CBaseEntity *pOther )
{
	if ( !pOther || !pOther->IsPlayer() )
		return;

	if ( m_iState <= 0 )
	{
		if ( m_szTriggerOnIsEmpty )
			FireTargets( STRING( m_szTriggerOnIsEmpty ), pOther, this, USE_TOGGLE, 0 );

		// "no oxygen" sound
		EMIT_SOUND( ENT(pev), CHAN_BODY, m_szSoundOnIsEmpty ? STRING( m_szSoundOnIsEmpty ) : "player/pl_swim2.wav", 1.0, ATTN_NORM );
		return;
	}

	// give player more seconds of air
	pOther->pev->air_finished = V_max( 0.0f, gpGlobals->time + m_flAirTimeGiven );

	// suit recharge sound
	SUB_UseTargets( this, USE_TOGGLE, 1 );

	if ( m_szTriggerOnConsume )
		FireTargets( STRING( m_szTriggerOnConsume ), pOther, this, USE_TOGGLE, 0 );

	EMIT_SOUND( ENT(pev), CHAN_BODY, m_szSoundOnConsume ? STRING( m_szSoundOnConsume ) : "doors/aliendoor3.wav", 1.0, ATTN_NORM );

	// recharge airtank in the configured time, a negative delay never refills
	pev->nextthink = gpGlobals->time + m_flRefillDelay;

	if ( m_flRefillDelay >= 0 )
		m_iState = 0;
	else
		m_iState = -1;
}
