#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "gamerules.h"
#include "charger.h"

TYPEDESCRIPTION CBaseCharger::m_SaveData[] =
{
	DEFINE_FIELD( CBaseCharger, m_flNextCharge, FIELD_TIME ),
	DEFINE_FIELD( CBaseCharger, m_iReactivate, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseCharger, m_iJuice, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseCharger, m_State, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseCharger, m_flSoundTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CBaseCharger, CBaseToggle );

void CBaseCharger::KeyValue( KeyValueData *data )
{
	if ( FStrEq( data->szKeyName, "style" ) || FStrEq( data->szKeyName, "height" )
		|| FStrEq( data->szKeyName, "value1" ) || FStrEq( data->szKeyName, "value2" )
		|| FStrEq( data->szKeyName, "value3" ) )
	{
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "dmdelay" ) )
	{
		m_iReactivate = atoi( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "CustomJuice" ) )
	{
		m_iCustomJuice = V_max( -2, atoi( data->szValue ) );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "CustomRechargeTime" ) )
	{
		m_iCustomRechargeTime = V_max( -1, atoi( data->szValue ) );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "TriggerOnEmpty" ) )
	{
		m_iszTriggerOnEmpty = AllocStringLocal( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "TriggerOnRecharged" ) )
	{
		m_iszTriggerOnRecharged = AllocStringLocal( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "CustomDeniedSound" ) )
	{
		m_pszDeniedSound = STRING( AllocStringLocal( data->szValue ) );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "CustomStartSound" ) )
	{
		m_pszStartSound = STRING( AllocStringLocal( data->szValue ) );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "CustomLoopSound" ) )
	{
		m_pszLoopSound = STRING( AllocStringLocal( data->szValue ) );
		data->fHandled = TRUE;
	}
	else
	{
		CBaseToggle::KeyValue( data );
	}
}

void CBaseCharger::Precache( void )
{
	CBaseEntity::Precache();

	ChargerSounds sounds = GetChargerSounds();

	if ( !m_pszDeniedSound )
		m_pszDeniedSound = sounds.denied;

	if ( !m_pszStartSound )
		m_pszStartSound = sounds.start;

	if ( !m_pszLoopSound )
		m_pszLoopSound = sounds.loop;

	PrecacheSound_Intercept( this, m_pszDeniedSound );
	PrecacheSound_Intercept( this, m_pszStartSound );
	PrecacheSound_Intercept( this, m_pszLoopSound );
}

void CBaseCharger::Spawn( void )
{
	Precache();

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	SetModel_Intercept( edict(), STRING( pev->model ) );

	Recharge( true );
}

void CBaseCharger::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( !activator || !activator->IsPlayer() )
		return;

	if ( m_iJuice <= 0 )
	{
		if ( pev->frame == 0.0f && m_iszTriggerOnEmpty )
			FireTargets( STRING( m_iszTriggerOnEmpty ), this, this, USE_TOGGLE, 0 );

		pev->frame = 1;
		Off();
	}

	CBasePlayer *player = dynamic_cast<CBasePlayer *>( activator );

	if ( !player || ChargeDenied( player ) )
		return;

	pev->nextthink = pev->ltime + 0.25f;
	SetThink( &CBaseCharger::Off );

	if ( m_flNextCharge >= gpGlobals->time )
		return;

	if ( m_State == STATE_OFF )
	{
		m_State = STATE_STARTING;
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, m_pszStartSound, 1.0f, ATTN_NORM,
		                0, PITCH_NORM, 0, false, g_vecZero );
		m_flSoundTime = gpGlobals->time + 0.56f;
	}

	if ( m_State == STATE_STARTING && m_flSoundTime <= gpGlobals->time )
	{
		m_State = STATE_CHARGING;
		EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, m_pszLoopSound, 1.0f, ATTN_NORM,
		                0, PITCH_NORM, 0, false, g_vecZero );
	}

	Charge( player );
	m_flNextCharge = gpGlobals->time + 0.05f;
}

void CBaseCharger::Off( void )
{
	if ( m_State == STATE_RECHARGING )
		return;

	if ( m_State > STATE_STARTING )
	{
		// The stop call selects the start sample on the static channel
		EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, m_pszStartSound, 0, 0,
		                SND_STOP, PITCH_NORM, 0, false, g_vecZero );
	}

	m_State = STATE_OFF;

	float rechargeTime;

	if ( m_iCustomRechargeTime < 0 )
		rechargeTime = m_iCustomRechargeTime + 1;
	else if ( m_iCustomRechargeTime > 0 )
		rechargeTime = m_iCustomRechargeTime;
	else
		rechargeTime = GetRechargeTime();

	m_iReactivate = (int) rechargeTime;

	if ( !m_iJuice )
	{
		BOOL multiplayer = g_pGameRules->IsMultiplayer();

		if ( m_iReactivate > 0 )
		{
			m_State = STATE_RECHARGING;
			pev->nextthink = pev->ltime + m_iReactivate;
			SetThink( &CBaseCharger::Recharge );
			return;
		}

		if ( multiplayer && !m_iReactivate )
		{
			Recharge( false );
			return;
		}
	}

	SetThink( &CBaseEntity::SUB_DoNothing );
}

void CBaseCharger::Recharge( void )
{
	Recharge( false );
}

void CBaseCharger::Recharge( bool fromSpawn )
{
	if ( m_iCustomJuice == -1 )
	{
		m_iJuice = 0;
		pev->frame = 1;
	}
	else
	{
		m_iJuice = m_iCustomJuice > 0 ? m_iCustomJuice : GetDefaultCapacity();
	}

	if ( m_iJuice )
	{
		if ( !fromSpawn )
		{
			EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, m_pszStartSound, 1.0f, ATTN_NORM,
			                0, PITCH_NORM, 0, false, g_vecZero );
		}

		pev->frame = 0;

		if ( m_iszTriggerOnRecharged )
			FireTargets( STRING( m_iszTriggerOnRecharged ), this, this, USE_TOGGLE, 0 );
	}

	m_State = STATE_OFF;

	if ( !fromSpawn )
		SetThink( &CBaseEntity::SUB_DoNothing );
}
