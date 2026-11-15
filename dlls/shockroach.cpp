#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "shockroach.h"
#include "weapons.h"
#include "player.h"
#include "user_messages.h"

extern cvar_t sk_headcrab_health;
extern cvar_t sk_headcrab_dmg_bite;
extern cvar_t sk_shockroach_dmg_xpl_touch;
extern cvar_t sk_shockroach_dmg_xpl_splash;

enum
{
	TASK_SHOCKROACH_WAIT_FOR_LAND = LAST_COMMON_TASK + 1,
	TASK_SHOCKROACH_DETONATE_JUMP
};


LINK_ENTITY_TO_CLASS( monster_shockroach, CShockRoach );

static const char *shockRoachIdleSounds[] =
{
	"shockroach/shock_idle1.wav",
	"shockroach/shock_idle2.wav",
	"shockroach/shock_idle3.wav"
};

static const char *shockRoachAttackSounds[] =
{
	"shockroach/shock_jump1.wav",
	"shockroach/shock_jump2.wav"
};

Task_t tlSRRangeAttack1[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_SET_ACTIVITY, ACT_IDLE },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_WAIT_RANDOM, 0.5f }
};

Task_t tlSRRangeAttack1Fast[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_SET_ACTIVITY, ACT_IDLE }
};

Task_t tlSRDetonate[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_WAIT, 0.6f },
	{ TASK_SHOCKROACH_DETONATE_JUMP, 0 }
};

Task_t tlSRThrowDie[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_WAIT, 0.1f },
	{ TASK_DIE, 0 }
};

Schedule_t slSRRangeAttack1[] =
{
	{
		tlSRRangeAttack1,
		ARRAYSIZE( tlSRRangeAttack1 ),
		bits_COND_NO_AMMO_LOADED | bits_COND_ENEMY_OCCLUDED,
		0,
		"SRRangeAttack1"
	}
};

Schedule_t slSRRangeAttack1Fast[] =
{
	{
		tlSRRangeAttack1Fast,
		ARRAYSIZE( tlSRRangeAttack1Fast ),
		bits_COND_NO_AMMO_LOADED | bits_COND_ENEMY_OCCLUDED,
		0,
		"SRRAFast"
	}
};

Schedule_t slSRDetonate[] =
{
	{ tlSRDetonate, ARRAYSIZE( tlSRDetonate ), 0, 0, "SRDetonate" }
};

Schedule_t slSRThrowDie[] =
{
	{ tlSRThrowDie, ARRAYSIZE( tlSRThrowDie ), 0, 0, "SRThrowDie" }
};

DEFINE_CUSTOM_SCHEDULES( CShockRoach )
{
	slSRRangeAttack1,
	slSRRangeAttack1Fast,
	slSRDetonate,
	slSRThrowDie
};

IMPLEMENT_CUSTOM_SCHEDULES( CShockRoach, CBaseMonster );

void CShockRoach::Spawn( void )
{
	Precache();

	if ( !SetupModel() )
		UTIL_SetModel( this, "models/w_shock_rifle.mdl" );

	UTIL_SetSize( pev, Vector( -12, -12, 0 ), Vector( 12, 12, 24 ) );

	if ( pev->health == 0.0f )
		pev->health = sk_headcrab_health.value;

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_YELLOW;
	pev->effects = 0;
	pev->view_ofs = Vector( 0, 0, 20 );
	m_flFieldOfView = 0.5f;
	m_MonsterState = MONSTERSTATE_NONE;
	m_flExpireTime = gpGlobals->time + 20.0f;

	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( "Shock Roach" );

	MonsterInit();

	if ( !IsPlayerAlly() )
		SetUse( &CShockRoach::PickUp );
}

void CShockRoach::Precache( void )
{
	CBaseMonster::Precache();

	if ( !pev->model )
		UTIL_PrecacheModel( this, "models/w_shock_rifle.mdl" );

	for ( int i = 0; i < ARRAYSIZE( shockRoachIdleSounds ); i++ )
		PrecacheSound_Intercept( this, shockRoachIdleSounds[i] );

	PrecacheSound_Intercept( this, "shockroach/shock_walk.wav" );
	PrecacheSound_Intercept( this, "shockroach/shock_flinch.wav" );

	for ( int i = 0; i < ARRAYSIZE( shockRoachAttackSounds ); i++ )
		PrecacheSound_Intercept( this, shockRoachAttackSounds[i] );

	PrecacheSound_Intercept( this, "shockroach/shock_die.wav" );
	PrecacheSound_Intercept( this, "shockroach/shock_bite.wav" );
	PrecacheSound_Intercept( this, "weapons/shock_discharge.wav" );
	UTIL_PrecacheModel( this, "sprites/blueflare1.spr" );
}

int CShockRoach::ObjectCaps( void )
{
	return FCAP_ACROSS_TRANSITION | FCAP_IMPULSE_USE;
}

int CShockRoach::Classify( void )
{
	return GetClassification( CLASS_ALIEN_PREY );
}

Vector CShockRoach::Center( void )
{
	return Vector( pev->origin.x, pev->origin.y, pev->origin.z + 6.0f );
}

Vector CShockRoach::BodyTarget( const Vector &source )
{
	return Center();
}

float CShockRoach::GetPointsForDamage( float damage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	float healthScale = pev->max_health / sk_headcrab_health.value;

	return ( damage / pev->max_health ) * healthScale;
}

float CShockRoach::GetDamageAmount( void )
{
	return sk_headcrab_dmg_bite.value;
}

void CShockRoach::SetYawSpeed( void )
{
	pev->yaw_speed = 270;
}

void CShockRoach::IdleSound( void )
{
	const char *sound = shockRoachIdleSounds[RANDOM_LONG( 0, ARRAYSIZE( shockRoachIdleSounds ) - 1 )];

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, sound, 1.0f, 1.5f, 0, PITCH_NORM );
}

void CShockRoach::AlertSound( void )
{
	RANDOM_LONG( 0, 0 );
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "shockroach/shock_walk.wav", 1.0f, 1.5f, 0, PITCH_NORM );
}

void CShockRoach::PainSound( void )
{
	RANDOM_LONG( 0, 0 );
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "shockroach/shock_flinch.wav", 1.0f, 1.5f, 0, PITCH_NORM );
}

void CShockRoach::DeathSound( void )
{
	RANDOM_LONG( 0, 0 );
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "shockroach/shock_die.wav", 1.0f, 1.5f, 0, PITCH_NORM );
}

void CShockRoach::PrescheduleThink( void )
{
	if ( m_MonsterState == MONSTERSTATE_COMBAT && RANDOM_FLOAT( 0, 5 ) < 0.1 )
		IdleSound();
}

BOOL CShockRoach::CheckRangeAttack1( float dot, float distance )
{
	return ( pev->flags & FL_ONGROUND ) && distance <= 256.0f && dot >= 0.65;
}

BOOL CShockRoach::CheckRangeAttack2( float dot, float distance )
{
	return FALSE;
}

void CShockRoach::HandleAnimEvent( MonsterEvent_t *event )
{
	if ( event->event != 2 )
	{
		CBaseMonster::HandleAnimEvent( event );
		return;
	}

	ClearBits( pev->flags, FL_ONGROUND );
	UTIL_SetOrigin( pev, pev->origin + Vector( 0, 0, 1 ) );
	UTIL_MakeVectors( pev->angles );

	Vector jumpVelocity;

	if ( m_hEnemy != NULL )
	{
		float gravity = V_max( 1.0f, CVAR_GET_FLOAT( "sv_gravity" ) );
		float height = V_max( 16.0f, m_hEnemy->pev->origin.z + m_hEnemy->pev->view_ofs.z - pev->origin.z );
		float speed = sqrt( height * ( gravity + gravity ) );
		float time = speed / gravity;

		jumpVelocity = m_hEnemy->pev->origin + m_hEnemy->pev->view_ofs - pev->origin;
		jumpVelocity = jumpVelocity * ( 1.0 / time );
		jumpVelocity.z = speed;

		float distance = jumpVelocity.Length();

		if ( distance > 650.0f )
			jumpVelocity = jumpVelocity * ( 650.0 / distance );
	}
	else
		jumpVelocity = Vector( gpGlobals->v_forward.x, gpGlobals->v_forward.y, gpGlobals->v_up.z ) * 350.0f;

	const char *sound = shockRoachAttackSounds[RANDOM_LONG( 0, ARRAYSIZE( shockRoachAttackSounds ) - 1 )];

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, sound, 1.0f, ATTN_IDLE, 0, PITCH_NORM );
	pev->velocity = jumpVelocity;
	m_flNextAttack = gpGlobals->time + 2.0f;
}

void CShockRoach::LeapTouch( CBaseEntity *other )
{
	if ( other->pev->takedamage == DAMAGE_NO )
		return;

	int otherClass = other->Classify();

	if ( otherClass == Classify() )
		return;

	if ( !( pev->flags & FL_ONGROUND ) )
	{
		RANDOM_LONG( 0, 0 );
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "shockroach/shock_bite.wav", 1.0f, ATTN_IDLE, 0, PITCH_NORM );
		other->TakeDamage( pev, pev, GetDamageAmount(), DMG_SLASH );
	}

	SetTouch( NULL );
}

void CShockRoach::PickUp( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE type, float value )
{
	if ( pev->deadflag != DEAD_NO || !( pev->flags & FL_ONGROUND )
	     || !activator || !activator->IsPlayer() )
		return;

	CBasePlayer *player = (CBasePlayer *) activator;

	if ( player->IsExclusiveHoldItemActive() )
		return;

	player->GiveNamedItem( "weapon_shockrifle", SF_GIVENITEM | SF_CREATEDWEAPON, 0 );
	SUB_Remove();
}

void CShockRoach::StartTask( Task_t *task )
{
	switch ( task->iTask )
	{
	case TASK_RANGE_ATTACK1:
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, shockRoachAttackSounds[0], 1.0f, ATTN_IDLE, 0, PITCH_NORM );
		SetTouch( &CShockRoach::LeapTouch );
		m_IdealActivity = ACT_RANGE_ATTACK1;
		break;

	case TASK_SHOCKROACH_DETONATE_JUMP:
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, shockRoachAttackSounds[0], 1.0f, ATTN_IDLE, 0, PITCH_NORM );
		SetTouch( &CShockRoach::DetonateTouch );
		m_IdealActivity = ACT_RANGE_ATTACK1;
		break;

	case TASK_SHOCKROACH_WAIT_FOR_LAND:
		break;

	default:
		CBaseMonster::StartTask( task );
		break;
	}
}

void CShockRoach::RunTask( Task_t *task )
{
	switch ( task->iTask )
	{
	case TASK_RANGE_ATTACK1:
	case TASK_RANGE_ATTACK2:
		if ( m_fSequenceFinished )
		{
			if ( !HasConditions( bits_COND_TASK_FAILED ) )
				TaskComplete();

			SetTouch( NULL );
			m_IdealActivity = ACT_IDLE;
		}
		break;

	case TASK_SHOCKROACH_WAIT_FOR_LAND:
		if ( ( pev->groundentity || ( pev->flags & FL_ONGROUND ) )
		     && !HasConditions( bits_COND_TASK_FAILED ) )
			TaskComplete();
		break;

	case TASK_SHOCKROACH_DETONATE_JUMP:
		if ( pev->velocity.z < 0.0f )
			Killed( NULL, GIB_NEVER );
		break;

	case TASK_WAIT_FOR_MOVEMENT_AP:
		if ( pev->waterlevel > 1 && ( !pev->groundentity || !( pev->flags & FL_ONGROUND ) ) )
			TaskComplete();
		else
			CBaseMonster::RunTask( task );
		break;

	default:
		CBaseMonster::RunTask( task );
		break;
	}
}

Schedule_t *CShockRoach::GetScheduleOfType( int type )
{
	if ( type == SCHED_RANGE_ATTACK1 )
		return slSRRangeAttack1;

	if ( type == SCHED_DIE )
	{
		if ( m_fExplosiveLeap )
			return slSRDetonate;

		if ( !pev->groundentity && !( pev->flags & FL_ONGROUND ) )
			return slSRThrowDie;
	}

	if ( gpGlobals->time >= m_flExpireTime )
		return CBaseMonster::GetScheduleOfType( SCHED_DIE );

	return CBaseMonster::GetScheduleOfType( type );
}

void CShockRoach::ChangeSchedule( Schedule_t *schedule )
{
	if ( schedule == slSRDetonate )
		m_fExplosiveLeap = true;

	CBaseMonster::ChangeSchedule( schedule );
}

void CShockRoach::Killed( entvars_t *attacker, int gib )
{
	if ( m_fExplosiveLeap )
		Detonate( attacker, GIB_NEVER, NULL );
	else
		CBaseMonster::Killed( attacker, gib );
}

void CShockRoach::DetonateTouch( CBaseEntity *other )
{
	Detonate( NULL, GIB_NEVER, other );
}

void CShockRoach::Detonate( entvars_t *attacker, int gib, CBaseEntity *other )
{
	if ( !m_fDetonated )
	{
		m_fDetonated = true;
		pev->model = 0;
		SetThink( &CShockRoach::SUB_Remove );
		SetTouch( NULL );
		pev->nextthink = gpGlobals->time + 0.1;
		pev->takedamage = DAMAGE_NO;

		entvars_t *owner = pev->owner ? VARS( pev->owner ) : NULL;

		if ( other && other->pev->takedamage != DAMAGE_NO )
		{
			int otherClass = other->Classify();

			if ( otherClass != Classify() )
				other->TakeDamage( pev, owner, sk_shockroach_dmg_xpl_touch.value, DMG_SHOCK | DMG_NEVERGIB );
		}

		float damage = sk_shockroach_dmg_xpl_splash.value;
		float radius = (double) damage * 2.5;

		::RadiusDamage( pev->origin, pev, owner, damage, radius, CLASS_NONE, DMG_SHOCK | DMG_NEVERGIB );

		MESSAGE_BEGIN( MSG_PVS, gmsgSRDetonate, pev->origin );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_BYTE( 64 );
		MESSAGE_END();
	}

	CBaseMonster::Killed( attacker, gib );
}
