#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "squadmonster.h"
#include "schedule.h"
#include "nodes.h"
#include "effects.h"
#include "customentity.h"
#include "soundent.h"
#include "decals.h"
#include "weapons.h"
#include "voltigore.h"

extern cvar_t sk_voltigore_health;
extern cvar_t sk_voltigore_dmg_punch;
extern cvar_t sk_voltigore_dmg_beam;
extern cvar_t sk_voltigore_dmg_explode;

class CVoltShock : public CBaseEntity
{
public:
	void Spawn( void );
	void Touch( CBaseEntity *pOther );
	static void Shoot( entvars_t *pevOwner, Vector start, Vector velocity );
	void EXPORT FlyShockThink( void );
	void EXPORT IgniteThink( void );
	void EXPORT RadiusDamageThink( void );
	void EXPORT RemoveThink( void );

private:
	void TraceBeam( TraceResult &trace, float distance );

	float m_flExpireTime;
	float m_flNextBeamTime; // written but never read
	int   m_iBeamCount;
	int   m_iCurrentBeam;

	EHANDLE m_hBeams[7];
};

LINK_ENTITY_TO_CLASS( monster_alien_voltigore, CVoltigore );
LINK_ENTITY_TO_CLASS( monster_alien_babyvoltigore, CBabyVoltigore );
LINK_ENTITY_TO_CLASS( voltigoreshock, CVoltShock );

TYPEDESCRIPTION CVoltigore::m_SaveData[] =
{
	DEFINE_FIELD( CVoltigore, m_flLastHurtTime, FIELD_TIME ),
	DEFINE_FIELD( CVoltigore, m_flNextSpitTime, FIELD_TIME ),
};

TYPEDESCRIPTION CBabyVoltigore::m_SaveData[] =
{
	DEFINE_FIELD( CBabyVoltigore, m_fCanThreatDisplay, FIELD_CHARACTER ),
	DEFINE_FIELD( CBabyVoltigore, m_flLastHurtTime, FIELD_TIME ),
	DEFINE_FIELD( CBabyVoltigore, m_flNextSpitTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CVoltigore, CSquadMonster );
IMPLEMENT_SAVERESTORE( CBabyVoltigore, CSquadMonster );

enum
{
	SCHED_VOLTIGORE_HURTHOP = LAST_COMMON_SCHEDULE + 1,
	SCHED_VOLTIGORE_EAT,
	SCHED_VOLTIGORE_EAT2,
	SCHED_VOLTIGORE_SNIFF_AND_EAT,
	SCHED_VOLTIGORE_WALLOW,
};

enum
{
	SCHED_BABYVOLTIGORE_HURTHOP = LAST_COMMON_SCHEDULE + 1,
	SCHED_BABYVOLTIGORE_SEECRAB,
	SCHED_BABYVOLTIGORE_EAT,
	SCHED_BABYVOLTIGORE_EAT2,
	SCHED_BABYVOLTIGORE_SNIFF_AND_EAT,
	SCHED_BABYVOLTIGORE_WALLOW,
};

static const char *const pVoltigorePainSounds[] =
{
	"voltigore/voltigore_pain1.wav",
	"voltigore/voltigore_pain2.wav",
	"voltigore/voltigore_pain3.wav",
	"voltigore/voltigore_pain4.wav",
};

static const char *const pVoltigoreAlertSounds[] =
{
	"voltigore/voltigore_alert1.wav",
	"voltigore/voltigore_alert2.wav",
	"voltigore/voltigore_alert3.wav",
};

static const char *const pVoltigoreDieSounds[] =
{
	"voltigore/voltigore_die1.wav",
	"voltigore/voltigore_die2.wav",
	"voltigore/voltigore_die3.wav",
};

static const char *const pVoltigoreIdleSounds[] =
{
	"voltigore/voltigore_idle1.wav",
	"voltigore/voltigore_idle2.wav",
	"voltigore/voltigore_idle3.wav",
};

static const char *const pVoltigoreMeleeSounds[] =
{
	"voltigore/voltigore_attack_melee1.wav",
	"voltigore/voltigore_attack_melee2.wav",
};

void CBabyVoltigore::Spawn( void )
{
	Precache();

	if ( !SetupModel() )
		UTIL_SetModel( this, "models/baby_voltigore.mdl" );

	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 36 ) );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_GREEN;
	pev->effects = 0;

	if ( pev->health == 0.0f )
		pev->health = 80.0f;

	m_flFieldOfView = -0.7f;
	m_MonsterState = MONSTERSTATE_NONE;
	m_afCapability = bits_CAP_HEAR | bits_CAP_TURN_HEAD;
	m_afMoveShootCap = bits_MOVESHOOTCAP_MELEE_ATTACK1 | bits_MOVESHOOTCAP_RANGE_ATTACK1;
	m_fCanThreatDisplay = true;
	m_flNextSpitTime = gpGlobals->time;

	if ( FStringNull( m_FormattedName ) )
		m_FormattedName = MAKE_STRING( "Baby Voltigore" );

	MonsterInit();

	if ( IsPlayerAlly() && !( pev->spawnflags & SF_MONSTER_PREDISASTER ) )
	{
		SetUse( &CBabyVoltigore::CallFollowerPlayerUse );
		m_fCanPlayerFollow = true;
	}
}

void CBabyVoltigore::Precache( void )
{
	CBaseEntity::Precache();

	if ( FStringNull( pev->model ) )
		UTIL_PrecacheModel( this, "models/baby_voltigore.mdl" );

	UTIL_PrecacheModel( this, "sprites/blueflare2.spr" );
	PrecacheSound_Intercept( this, "zombie/claw_miss2.wav" );
	PrecacheSound_Intercept( this, "debris/beamstart2.wav" );
	PrecacheSound_Intercept( this, "voltigore/voltigore_attack_shock.wav" );

	for ( int i = 0; i < ARRAYSIZE( pVoltigoreAlertSounds ); i++ )
		PrecacheSound_Intercept( this, pVoltigoreAlertSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pVoltigoreDieSounds ); i++ )
		PrecacheSound_Intercept( this, pVoltigoreDieSounds[i] );

	PrecacheSound_Intercept( this, "voltigore/voltigore_run_grunt1.wav" );
	PrecacheSound_Intercept( this, "voltigore/voltigore_run_grunt2.wav" );
	PrecacheSound_Intercept( this, "voltigore/voltigore_communicate3.wav" );
	PrecacheSound_Intercept( this, "voltigore/voltigore_footstep1.wav" );
	PrecacheSound_Intercept( this, "voltigore/voltigore_footstep2.wav" );
	PrecacheSound_Intercept( this, "voltigore/voltigore_footstep3.wav" );
	PrecacheSound_Intercept( this, "zombie/claw_strike3.wav" );

	for ( int i = 0; i < ARRAYSIZE( pVoltigorePainSounds ); i++ )
		PrecacheSound_Intercept( this, pVoltigorePainSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pVoltigoreMeleeSounds ); i++ )
		PrecacheSound_Intercept( this, pVoltigoreMeleeSounds[i] );

	PrecacheSound_Intercept( this, "voltigore/voltigore_eat.wav" );
}

int CBabyVoltigore::ObjectCaps( void )
{
	return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

int CBabyVoltigore::Classify( void )
{
	return GetClassification( CLASS_XRACE_SHOCK );
}

void CBabyVoltigore::SetYawSpeed( void )
{
	pev->yaw_speed = 270;
}

int CBabyVoltigore::ISoundMask( void )
{
	return bits_SOUND_WORLD | bits_SOUND_COMBAT | bits_SOUND_PLAYER | bits_SOUND_CARCASS | bits_SOUND_MEAT | bits_SOUND_GARBAGE;
}

Vector CBabyVoltigore::Center( void )
{
	return pev->origin + Vector( 0, 0, 6 );
}

Vector CBabyVoltigore::BodyTarget( const Vector &source )
{
	return Center();
}

void CBabyVoltigore::SetObjectCollisionBox( void )
{
	pev->absmin = pev->origin + Vector( -16, -16, 0 );
	pev->absmax = pev->origin + Vector( 16, 16, 36 );
}

int CBabyVoltigore::IRelationship( CBaseEntity *pTarget, bool fIgnoreNoTarget )
{
	if ( !pTarget )
		return R_NO;

	if ( ( pTarget->pev->flags & FL_MONSTER ) && pTarget->m_fOverrideClass )
		return CBaseEntity::IRelationship( pTarget, false );

	if ( !IsPlayerAlly() && ( FClassnameIs( pTarget->pev, "monster_alien_grunt" ) || FClassnameIs( pTarget->pev, "monster_babygarg" ) || FClassnameIs( pTarget->pev, "monster_gargantua" ) ) )
	{
		return R_DL;
	}

	return CBaseEntity::IRelationship( pTarget, fIgnoreNoTarget );
}

int CBabyVoltigore::IgnoreConditions( void )
{
	int conditions = CBaseMonster::IgnoreConditions();

	if ( gpGlobals->time - m_flLastHurtTime <= 20.0f )
		conditions = bits_COND_SMELL | bits_COND_SMELL_FOOD;

	if ( m_hEnemy && FClassnameIs( m_hEnemy->pev, "monster_headcrab" ) )
		conditions = bits_COND_SMELL | bits_COND_SMELL_FOOD;

	return conditions;
}

BOOL CBabyVoltigore::CheckMeleeAttack1( float flDot, float flDist )
{
	return flDist <= 48.0f && flDot >= 0.7;
}

BOOL CBabyVoltigore::CheckRangeAttack1( float flDot, float flDist )
{
	if ( ( IsMoving() && flDist >= 512.0f ) || flDist <= 64.0f || flDist > 784.0f || flDot < 0.5f || gpGlobals->time < m_flNextSpitTime )
	{
		return FALSE;
	}

	m_flNextSpitTime = gpGlobals->time + ( IsMoving() ? 5.0f : 0.5f );
	return TRUE;
}

void CBabyVoltigore::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	CBaseMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

int CBabyVoltigore::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( m_hEnemy && IsMoving() && pevAttacker == m_hEnemy->pev && gpGlobals->time - m_flLastHurtTime > 3.0f )
	{
		if ( ( pev->origin - m_hEnemy->pev->origin ).Length2D() > 400.0f )
		{
			Vector apex;
			Vector apex2;
			Vector destination = m_Route[m_iRouteIndex].vecLocation;
			float distance = ( pev->origin - destination ).Length2D();

			if ( FTriangulate( pev->origin, destination, distance * 0.5f, m_hEnemy, &apex, &apex2, true ) )
				InsertWaypoint( apex, bits_MF_TO_DETOUR | bits_MF_DONT_SIMPLIFY );
		}
	}

	if ( IsFriendly( CBaseEntity::Instance( pevInflictor ) ) )
		flDamage = 0;

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CBabyVoltigore::PainSound( void )
{
	int pitch = RANDOM_LONG( 120, 140 );

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, pVoltigorePainSounds[RANDOM_LONG( 0, 3 )], 1, ATTN_NORM, 0, pitch );
}

void CBabyVoltigore::AlertSound( void )
{
	int pitch = RANDOM_LONG( 120, 140 );

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, pVoltigoreAlertSounds[RANDOM_LONG( 0, 2 )], 1, ATTN_IDLE, 0, pitch );
}

void CBabyVoltigore::DeathSound( void )
{
	int pitch = RANDOM_LONG( 120, 140 );

	RANDOM_LONG( 0, 3 );
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "voltigore/voltigore_die1.wav", 1, ATTN_NORM, 0, pitch );
}

void CBabyVoltigore::AttackSound( void )
{
	int pitch = RANDOM_LONG( 120, 140 );

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, pVoltigoreMeleeSounds[RANDOM_LONG( 0, 1 )], 1, ATTN_NORM, 0, pitch );
}

void CBabyVoltigore::UseSound( void )
{
	AlertSound();
}

void CBabyVoltigore::UnUseSound( void )
{
	PainSound();
}

void CBabyVoltigore::UpdateSprite( void )
{
}

void CBabyVoltigore::RunAI( void )
{
	CSquadMonster::RunAI();
}

void CBabyVoltigore::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	if ( pEvent->event == 1 )
	{
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "debris/beamstart2.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 0, 15 ) + 110 );
		UTIL_MakeVectors( pev->angles );
		RANDOM_FLOAT( -0.02f, 0.02f );
		RANDOM_FLOAT( -0.02f, 0.02f );
		RANDOM_FLOAT( -0.02f, 0.02f );
		return;
	}

	if ( pEvent->event != 12 && pEvent->event != 13 )
	{
		CBaseMonster::HandleAnimEvent( pEvent );
		return;
	}

	CBaseEntity *victim = CheckTraceHullAttack( 56.0f, 10, DMG_SLASH );

	if ( victim )
	{
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "zombie/claw_strike3.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 0, 15 ) + 100 );

		if ( victim->pev->flags & FL_CLIENT )
		{
			victim->pev->punchangle.z = -5;
			victim->pev->punchangle.x = 5;
		}
	}
	else
	{
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "zombie/claw_miss2.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 0, 15 ) + 100 );
	}
}

void CBabyVoltigore::StartTask( Task_t *pTask )
{
	if ( pTask->iTask == TASK_GET_PATH_TO_ENEMY )
	{
		if ( !m_hEnemy )
		{
			TaskFail();
			return;
		}

		if ( !BuildRoute( m_hEnemy->Center(), bits_MF_TO_ENEMY, m_hEnemy ) && !BuildNearestRoute( m_hEnemy->Center(), m_hEnemy->pev->view_ofs, 0,
		                                                                                          ( m_hEnemy->pev->origin - pev->origin ).Length() ) )
		{
			ALERT( at_aiconsole, "GetPathToEnemy failed!!\n" );
			TaskFail();
			return;
		}

		TaskComplete();
		return;
	}

	if ( pTask->iTask == TASK_MELEE_ATTACK1 )
		AttackSound();

	CBaseMonster::StartTask( pTask );
}

void CBabyVoltigore::RunTask( Task_t *pTask )
{
	CBaseMonster::RunTask( pTask );
}

void CBabyVoltigore::SetActivity( Activity activity )
{
	if ( activity == ACT_VICTORY_DANCE )
		pev->health = V_min( pev->max_health, pev->health + 100.0f );

	int sequence = LookupActivity( activity );

	if ( sequence > -1 )
	{
		if ( pev->sequence != sequence || !m_fSequenceLoops )
		{
			if ( ( m_Activity != ACT_WALK && m_Activity != ACT_RUN ) || ( activity != ACT_WALK && activity != ACT_RUN ) )
			{
				pev->frame = 0;
			}
		}

		pev->sequence = sequence;
		ResetSequenceInfo();
		SetYawSpeed();
	}
	else
	{
		ALERT( at_aiconsole, "%s has no sequence for act:%d\n", STRING( pev->classname ), activity );
		pev->sequence = 0;
	}

	m_IdealActivity = activity;
	m_Activity = activity;
}

MONSTERSTATE CBabyVoltigore::GetIdealState( void )
{
	int conditions = GetConditions();

	if ( m_MonsterState == MONSTERSTATE_COMBAT && m_hEnemy && ( conditions & ( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) ) && FClassnameIs( m_hEnemy->pev, "monster_headcrab" ) )
	{
		m_hEnemy = NULL;
		m_IdealMonsterState = MONSTERSTATE_ALERT;
	}

	m_IdealMonsterState = CSquadMonster::GetIdealState();
	return m_IdealMonsterState;
}

Task_t tlBabyVoltigoreRangeAttack1[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_CHASE_ENEMY },
	{ TASK_FACE_IDEAL, 0.0f },
	{ TASK_RANGE_ATTACK1, 0.0f },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
};

Schedule_t slBabyVoltigoreRangeAttack1[] =
{
	{ tlBabyVoltigoreRangeAttack1, ARRAYSIZE( tlBabyVoltigoreRangeAttack1 ),
	  bits_COND_ENEMY_OCCLUDED | bits_COND_NEW_ENEMY | bits_COND_ENEMY_DEAD, 0, "BabyVoltigore Range Attack1" },
};

Task_t tlBabyVoltigoreSwipe[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_FACE_ENEMY, 0.0f },
	{ TASK_MELEE_ATTACK1, 0.0f },
};

Schedule_t slBabyVoltigoreSwipe[] =
{
	{ tlBabyVoltigoreSwipe, ARRAYSIZE( tlBabyVoltigoreSwipe ),
	  0, 0, "BabyVoltigore Swipe" },
};

Task_t tlBabyVoltigoreChaseEnemy[] =
{
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_TAKE_COVER_FROM_ENEMY },
	{ TASK_GET_PATH_TO_ENEMY, 0.0f },
	{ TASK_RUN_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
};

Schedule_t slBabyVoltigoreChaseEnemy[] =
{
	{ tlBabyVoltigoreChaseEnemy, ARRAYSIZE( tlBabyVoltigoreChaseEnemy ),
	  bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK2
		  | bits_COND_NEW_ENEMY | bits_COND_HEAR_SOUND | bits_COND_ENEMY_DEAD, bits_SOUND_DANGER, "Squid Chase Enemy" },
};

Task_t tlBabyVoltigoreSeeCrab[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_SOUND_WAKE, 0.0f },
	{ TASK_FACE_ENEMY, 0.0f },
};

Schedule_t slBabyVoltigoreSeeCrab[] =
{
	{ tlBabyVoltigoreSeeCrab, ARRAYSIZE( tlBabyVoltigoreSeeCrab ),
	  bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE, 0, "SquidSeeCrab" },
};

Task_t tlBabyVoltigoreEat2[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_STORE_LASTPOSITION, 0.0f },
	{ TASK_GET_PATH_TO_ENEMY_CORPSE, 0.0f },
	{ TASK_WALK_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0.0f },
	{ TASK_WALK_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_CLEAR_LASTPOSITION, 0.0f },
	{ TASK_TURN_LEFT, 179.0f },
};

Schedule_t slBabyVoltigoreEat2[] =
{
	{ tlBabyVoltigoreEat2, ARRAYSIZE( tlBabyVoltigoreEat2 ),
	  bits_COND_SEE_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY, 0, "BabyVoltigore Eat2" },
};

Task_t tlBabyVoltigoreEat[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_EAT, 10.0f },
	{ TASK_STORE_LASTPOSITION, 0.0f },
	{ TASK_GET_PATH_TO_BESTSCENT, 0.0f },
	{ TASK_WALK_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_EAT, 50.0f },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0.0f },
	{ TASK_WALK_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_CLEAR_LASTPOSITION, 0.0f },
};

Schedule_t slBabyVoltigoreEat[] =
{
	{ tlBabyVoltigoreEat, ARRAYSIZE( tlBabyVoltigoreEat ),
	  bits_COND_SEE_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY, bits_SOUND_CARCASS | bits_SOUND_MEAT, "BabyVoltigore Eat" },
};

Task_t tlBabyVoltigoreSniffAndEat[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_EAT, 10.0f },
	{ TASK_STORE_LASTPOSITION, 0.0f },
	{ TASK_GET_PATH_TO_BESTSCENT, 0.0f },
	{ TASK_WALK_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_EAT, 50.0f },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0.0f },
	{ TASK_WALK_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_CLEAR_LASTPOSITION, 0.0f },
};

Schedule_t slBabyVoltigoreSniffAndEat[] =
{
	{ tlBabyVoltigoreSniffAndEat, ARRAYSIZE( tlBabyVoltigoreSniffAndEat ),
	  bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY, bits_SOUND_CARCASS | bits_SOUND_MEAT, "SquidSniffAndEat" },
};

Task_t tlBabyVoltigoreWallow[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_EAT, 10.0f },
	{ TASK_STORE_LASTPOSITION, 0.0f },
	{ TASK_GET_PATH_TO_BESTSCENT, 0.0f },
	{ TASK_WALK_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_EAT, 50.0f },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0.0f },
	{ TASK_WALK_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_CLEAR_LASTPOSITION, 0.0f },
};

Schedule_t slBabyVoltigoreWallow[] =
{
	{ tlBabyVoltigoreWallow, ARRAYSIZE( tlBabyVoltigoreWallow ),
	  bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY, bits_SOUND_GARBAGE, "SquidWallow" },
};

Task_t tlVoltigoreChaseEnemy[] =
{
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_RANGE_ATTACK1 },
	{ TASK_GET_PATH_TO_ENEMY, 0.0f },
	{ TASK_RUN_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
};

Schedule_t slVoltigoreChaseEnemy[] =
{
	{ tlVoltigoreChaseEnemy, ARRAYSIZE( tlVoltigoreChaseEnemy ),
	  bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK2
		  | bits_COND_NEW_ENEMY | bits_COND_HEAR_SOUND | bits_COND_ENEMY_DEAD, bits_SOUND_DANGER, "Squid Chase Enemy" },
};

Task_t tlVoltigoreSwipe[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_FACE_ENEMY, 0.0f },
	{ TASK_MELEE_ATTACK1, 0.0f },
};

Schedule_t slVoltigoreSwipe[] =
{
	{ tlVoltigoreSwipe, ARRAYSIZE( tlVoltigoreSwipe ),
	  0, 0, "Voltigore Swipe" },
};

Task_t tlVoltigoreRangeAttack1[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_CHASE_ENEMY },
	{ TASK_FACE_IDEAL, 0.0f },
	{ TASK_RANGE_ATTACK1, 0.0f },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
};

Schedule_t slVoltigoreRangeAttack1[] =
{
	{ tlVoltigoreRangeAttack1, ARRAYSIZE( tlVoltigoreRangeAttack1 ),
	  bits_COND_ENEMY_OCCLUDED | bits_COND_NEW_ENEMY | bits_COND_ENEMY_DEAD, 0, "Voltigore Range Attack1" },
};

Task_t tlVoltigoreEat[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_EAT, 10.0f },
	{ TASK_STORE_LASTPOSITION, 0.0f },
	{ TASK_GET_PATH_TO_BESTSCENT, 0.0f },
	{ TASK_WALK_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_EAT, 50.0f },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0.0f },
	{ TASK_WALK_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_CLEAR_LASTPOSITION, 0.0f },
};

Schedule_t slVoltigoreEat[] =
{
	{ tlVoltigoreEat, ARRAYSIZE( tlVoltigoreEat ),
	  bits_COND_SEE_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY, bits_SOUND_CARCASS | bits_SOUND_MEAT, "voltigore Eat" },
};

Task_t tlVoltigoreEat2[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_STORE_LASTPOSITION, 0.0f },
	{ TASK_GET_PATH_TO_ENEMY_CORPSE, 0.0f },
	{ TASK_WALK_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0.0f },
	{ TASK_WALK_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_CLEAR_LASTPOSITION, 0.0f },
	{ TASK_TURN_LEFT, 179.0f },
};

Schedule_t slVoltigoreEat2[] =
{
	{ tlVoltigoreEat2, ARRAYSIZE( tlVoltigoreEat2 ),
	  bits_COND_SEE_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY, 0, "voltigore Eat2" },
};

Task_t tlVoltigoreSniffAndEat[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_EAT, 10.0f },
	{ TASK_STORE_LASTPOSITION, 0.0f },
	{ TASK_GET_PATH_TO_BESTSCENT, 0.0f },
	{ TASK_WALK_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_EAT, 50.0f },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0.0f },
	{ TASK_WALK_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_CLEAR_LASTPOSITION, 0.0f },
};

Schedule_t slVoltigoreSniffAndEat[] =
{
	{ tlVoltigoreSniffAndEat, ARRAYSIZE( tlVoltigoreSniffAndEat ),
	  bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY, bits_SOUND_CARCASS | bits_SOUND_MEAT, "SquidSniffAndEat" },
};

Task_t tlVoltigoreWallow[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_EAT, 10.0f },
	{ TASK_STORE_LASTPOSITION, 0.0f },
	{ TASK_GET_PATH_TO_BESTSCENT, 0.0f },
	{ TASK_WALK_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_EAT, 50.0f },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0.0f },
	{ TASK_WALK_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_CLEAR_LASTPOSITION, 0.0f },
};

Schedule_t slVoltigoreWallow[] =
{
	{ tlVoltigoreWallow, ARRAYSIZE( tlVoltigoreWallow ),
	  bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY, bits_SOUND_GARBAGE, "SquidWallow" },
};

DEFINE_CUSTOM_SCHEDULES( CBabyVoltigore ){
	slBabyVoltigoreRangeAttack1,
	slBabyVoltigoreSwipe,
	slBabyVoltigoreChaseEnemy,
	slBabyVoltigoreSeeCrab,
	slBabyVoltigoreEat2,
	slBabyVoltigoreEat,
	slBabyVoltigoreSniffAndEat,
	slBabyVoltigoreWallow,
};

IMPLEMENT_CUSTOM_SCHEDULES( CBabyVoltigore, CSquadMonster );

DEFINE_CUSTOM_SCHEDULES( CVoltigore ){
	slVoltigoreChaseEnemy,
	slVoltigoreSwipe,
	slVoltigoreRangeAttack1,
	slVoltigoreEat,
	slVoltigoreEat2,
	slVoltigoreSniffAndEat,
	slVoltigoreWallow,
};

IMPLEMENT_CUSTOM_SCHEDULES( CVoltigore, CSquadMonster );

BOOL CBabyVoltigore::FValidateHintType( short sHint )
{
	if ( sHint == HINT_WORLD_HUMAN_BLOOD )
		return TRUE;

	ALERT( at_aiconsole, "Couldn't validate hint type" );
	return FALSE;
}

Schedule_t *CBabyVoltigore::GetScheduleOfType( int type )
{
	switch ( type )
	{
		case SCHED_CHASE_ENEMY:
			return slBabyVoltigoreChaseEnemy;
		case SCHED_MELEE_ATTACK1:
			return slBabyVoltigoreSwipe;
		case SCHED_RANGE_ATTACK1:
			m_iSpriteBrightness = 255;
			return slBabyVoltigoreRangeAttack1;
		case SCHED_BABYVOLTIGORE_SEECRAB:
			return slBabyVoltigoreSeeCrab;
		case SCHED_BABYVOLTIGORE_EAT:
			return slBabyVoltigoreEat;
		case SCHED_BABYVOLTIGORE_EAT2:
			return slBabyVoltigoreEat2;
		case SCHED_BABYVOLTIGORE_SNIFF_AND_EAT:
			return slBabyVoltigoreSniffAndEat;
		case SCHED_BABYVOLTIGORE_WALLOW:
			return slBabyVoltigoreWallow;
		default:
			return CBaseMonster::GetScheduleOfType( type );
	}
}

Schedule_t *CBabyVoltigore::GetSchedule( void )
{
	switch ( m_MonsterState )
	{
		case MONSTERSTATE_ALERT:
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
				return GetScheduleOfType( SCHED_BABYVOLTIGORE_EAT2 );

			if ( HasConditions( bits_COND_SMELL_FOOD ) )
			{
				CSound *scent = PBestScent();

				if ( !scent || ( FInViewCone( &scent->m_vecOrigin ) && FVisible( scent->m_vecOrigin ) ) )
					return GetScheduleOfType( SCHED_BABYVOLTIGORE_EAT );

				return GetScheduleOfType( SCHED_BABYVOLTIGORE_SNIFF_AND_EAT );
			}

			if ( HasConditions( bits_COND_SMELL ) && PBestScent() )
				return GetScheduleOfType( SCHED_BABYVOLTIGORE_WALLOW );

			break;

		case MONSTERSTATE_COMBAT:
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
				break;

			if ( HasConditions( bits_COND_NEW_ENEMY ) )
			{
				AlertSound();

				if ( m_fCanThreatDisplay && IRelationship( m_hEnemy ) == R_HT )
				{
					m_fCanThreatDisplay = false;
					return GetScheduleOfType( SCHED_BABYVOLTIGORE_SEECRAB );
				}

				return GetScheduleOfType( SCHED_WAKE_ANGRY );
			}

			if ( HasConditions( bits_COND_SMELL_FOOD ) )
			{
				CSound *scent = PBestScent();

				if ( !scent || ( FInViewCone( &scent->m_vecOrigin ) && FVisible( scent->m_vecOrigin ) ) )
					return GetScheduleOfType( SCHED_BABYVOLTIGORE_EAT );

				return GetScheduleOfType( SCHED_BABYVOLTIGORE_SNIFF_AND_EAT );
			}

			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK2 ) )
				return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

			return GetScheduleOfType( SCHED_CHASE_ENEMY );

		default:
			break;
	}

	return CBaseMonster::GetSchedule();
}

void CVoltigore::Spawn( void )
{
	Precache();

	if ( !SetupModel() )
		UTIL_SetModel( this, "models/voltigore.mdl" );

	UTIL_SetSize( pev, Vector( -48, -48, 0 ), Vector( 48, 48, 90 ) );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_GREEN;
	pev->effects = 0;

	if ( pev->health == 0.0f )
		pev->health = sk_voltigore_health.value;

	m_flFieldOfView = -0.7f;
	m_MonsterState = MONSTERSTATE_NONE;
	m_afCapability = bits_CAP_SQUAD | bits_CAP_HEAR | bits_CAP_TURN_HEAD;
	m_afMoveShootCap = bits_MOVESHOOTCAP_MELEE_ATTACK1 | bits_MOVESHOOTCAP_MELEE_ATTACK2;
	m_flNextSpitTime = gpGlobals->time;
	m_fDeathBeams = false;
	m_flDeathTime = gpGlobals->time;

	CSprite *glow = CSprite::SpriteCreate( "sprites/blueflare2.spr", pev->origin, FALSE );

	m_hGlow = glow;

	if ( glow )
	{
		glow->SetTransparency( kRenderGlow, 255, 255, 255, 0, kRenderFxNoDissipation );
		glow->SetAttachment( edict(), 4 );
	}

	if ( FStringNull( m_FormattedName ) )
		m_FormattedName = MAKE_STRING( "Voltigore" );

	MonsterInit();

	if ( IsPlayerAlly() && !( pev->spawnflags & SF_MONSTER_PREDISASTER ) )
	{
		SetUse( &CVoltigore::CallFollowerPlayerUse );
		m_fCanPlayerFollow = true;
	}
}

void CVoltigore::Precache( void )
{
	CBaseEntity::Precache();

	if ( FStringNull( pev->model ) )
		UTIL_PrecacheModel( this, "models/voltigore.mdl" );

	UTIL_PrecacheModel( this, "sprites/spore_exp_01.spr" );
	UTIL_PrecacheModel( this, "sprites/spore_exp_c_01.spr" );
	m_iSpitSprite = UTIL_PrecacheModel( this, "sprites/pitdronespit.spr" );
	UTIL_PrecacheModel( this, "models/vgibs.mdl" );
	UTIL_PrecacheModel( this, "sprites/blueflare2.spr" );
	UTIL_PrecacheModel( this, "sprites/lgtning.spr" );

	for ( int i = 0; i < ARRAYSIZE( pVoltigoreIdleSounds ); i++ )
		PrecacheSound_Intercept( this, pVoltigoreIdleSounds[i] );

	PrecacheSound_Intercept( this, "voltigore/voltigore_attack_shock.wav" );

	for ( int i = 0; i < ARRAYSIZE( pVoltigoreAlertSounds ); i++ )
		PrecacheSound_Intercept( this, pVoltigoreAlertSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pVoltigoreDieSounds ); i++ )
		PrecacheSound_Intercept( this, pVoltigoreDieSounds[i] );

	PrecacheSound_Intercept( this, "voltigore/voltigore_run_grunt1.wav" );
	PrecacheSound_Intercept( this, "voltigore/voltigore_run_grunt2.wav" );
	PrecacheSound_Intercept( this, "voltigore/voltigore_communicate3.wav" );
	PrecacheSound_Intercept( this, "voltigore/voltigore_footstep1.wav" );
	PrecacheSound_Intercept( this, "voltigore/voltigore_footstep2.wav" );
	PrecacheSound_Intercept( this, "voltigore/voltigore_footstep3.wav" );

	for ( int i = 0; i < ARRAYSIZE( pVoltigorePainSounds ); i++ )
		PrecacheSound_Intercept( this, pVoltigorePainSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pVoltigoreMeleeSounds ); i++ )
		PrecacheSound_Intercept( this, pVoltigoreMeleeSounds[i] );

	PrecacheSound_Intercept( this, "voltigore/voltigore_eat.wav" );
	PrecacheSound_Intercept( this, "zombie/claw_strike3.wav" );
	PrecacheSound_Intercept( this, "zombie/claw_miss2.wav" );
	PrecacheSound_Intercept( this, "debris/beamstart1.wav" );
	PrecacheSound_Intercept( this, "weapons/splauncher_impact.wav" );
	PrecacheSound_Intercept( this, "debris/zap4.wav" );
	PrecacheSound_Intercept( this, "debris/zap6.wav" );
	PrecacheSound_Intercept( this, "debris/zap7.wav" );
}

int CVoltigore::ObjectCaps( void )
{
	return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

int CVoltigore::Classify( void )
{
	return GetClassification( CLASS_XRACE_SHOCK );
}

float CVoltigore::GetPointsForDamage( float flDamage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	return ( pev->max_health / sk_voltigore_health.value ) * 15.0f * ( flDamage / pev->max_health );
}

void CVoltigore::SetObjectCollisionBox( void )
{
	pev->absmin = pev->origin + Vector( -80, -80, 0 );
	pev->absmax = pev->origin + Vector( 80, 80, 90 );
}

void CVoltigore::SetYawSpeed( void )
{
	pev->yaw_speed = 200;
}

int CVoltigore::ISoundMask( void )
{
	return bits_SOUND_WORLD | bits_SOUND_COMBAT | bits_SOUND_PLAYER | bits_SOUND_CARCASS | bits_SOUND_MEAT | bits_SOUND_GARBAGE;
}

BOOL CVoltigore::FValidateHintType( short sHint )
{
	if ( sHint == HINT_WORLD_HUMAN_BLOOD )
		return TRUE;

	ALERT( at_aiconsole, "Couldn't validate hint type" );
	return FALSE;
}

int CVoltigore::IRelationship( CBaseEntity *pTarget, bool fIgnoreNoTarget )
{
	if ( !pTarget )
		return R_NO;

	if ( ( pTarget->pev->flags & FL_MONSTER ) && pTarget->m_fOverrideClass )
		return CBaseEntity::IRelationship( pTarget, fIgnoreNoTarget );

	if ( !IsPlayerAlly() && ( FClassnameIs( pTarget->pev, "monster_alien_grunt" ) || FClassnameIs( pTarget->pev, "monster_babygarg" ) || FClassnameIs( pTarget->pev, "monster_gargantua" ) ) )
	{
		return R_DL;
	}

	return CBaseEntity::IRelationship( pTarget, false );
}

int CVoltigore::IgnoreConditions( void )
{
	int conditions = CBaseMonster::IgnoreConditions();

	if ( gpGlobals->time - m_flLastHurtTime <= 20.0f )
		return bits_COND_SMELL | bits_COND_SMELL_FOOD;

	return conditions;
}

BOOL CVoltigore::CheckMeleeAttack1( float flDot, float flDist )
{
	return RANDOM_LONG( 0, 6 ) >= 4 && flDist - 70.0f <= 60.0f && flDot >= 0.7;
}

BOOL CVoltigore::CheckMeleeAttack2( float flDot, float flDist )
{
	return flDist - 70.0f <= 60.0f && flDot >= 0.7 && !HasConditions( bits_COND_CAN_MELEE_ATTACK1 );
}

BOOL CVoltigore::CheckRangeAttack1( float flDot, float flDist )
{
	if ( ( IsMoving() && flDist >= 512.0f ) || flDist <= 64.0f || flDist > 784.0f || flDot < 0.5f || gpGlobals->time < m_flNextSpitTime )
	{
		return FALSE;
	}

	m_flNextSpitTime = gpGlobals->time + ( IsMoving() ? 5.0f : 0.5f );

	TraceResult trace;

	UTIL_MakeVectors( pev->angles );

	Vector start = ( gpGlobals->v_forward * 50 + gpGlobals->v_right + gpGlobals->v_up * 23 ) + pev->origin;
	Vector end = ( m_hEnemy->BodyTarget( start ) - m_hEnemy->Center() ) + m_vecEnemyLKP;

	UTIL_TraceLine( start, end, dont_ignore_monsters, edict(), &trace );
	return trace.flFraction == 1.0f || trace.pHit == m_hEnemy->edict();
}

int CVoltigore::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( bitsDamageType & ( DMG_BULLET | DMG_SLASH | DMG_CLUB | DMG_SHOCK ) )
		flDamage *= 0.35f;

	CBaseEntity *inflictor = CBaseEntity::Instance( pevInflictor );

	if ( inflictor && IRelationship( inflictor ) < R_NO && FClassnameIs( inflictor->pev, "monster_alien_voltigore" ) )
		flDamage = 0;

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CVoltigore::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	switch ( ptr->iHitgroup )
	{
		case 0:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			if ( bitsDamageType & ( DMG_BULLET | DMG_SLASH | DMG_BLAST ) )
				flDamage *= 0.45;

			break;
		case 1:
		case 10:
			flDamage *= 0.5f;
			break;
		default:
			break;
	}

	CBaseMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

void CVoltigore::IdleSound( void )
{
	int pitch = RANDOM_LONG( 85, 120 );

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, pVoltigoreIdleSounds[RANDOM_LONG( 0, 2 )], 1, ATTN_NORM, 0, pitch );
}

void CVoltigore::PainSound( void )
{
	int pitch = RANDOM_LONG( 85, 120 );

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, pVoltigorePainSounds[RANDOM_LONG( 0, 3 )], 1, ATTN_NORM, 0, pitch );
}

void CVoltigore::AlertSound( void )
{
	int pitch = RANDOM_LONG( 100, 120 );

	EMIT_SOUND_DYN( edict(), CHAN_BODY, pVoltigoreAlertSounds[RANDOM_LONG( 0, 2 )], 1, ATTN_IDLE, 0, pitch );
}

void CVoltigore::UseSound( void )
{
	AlertSound();
}

void CVoltigore::UnUseSound( void )
{
	PainSound();
}

void CVoltigore::RunAI( void )
{
	CBaseEntity *ground = CBaseEntity::Instance( pev->groundentity );

	if ( ground && ( ground->pev->flags & ( FL_CLIENT | FL_MONSTER ) ) )
	{
		ground->TakeDamage( pev, pev, 500, DMG_CRUSH );
		pev->groundentity = NULL;
		pev->flags &= ~FL_ONGROUND;
		pev->velocity.z -= 1.0f;
	}

	CSquadMonster::RunAI();
}

void CVoltigore::StartTask( Task_t *pTask )
{
	if ( pTask->iTask == TASK_GET_PATH_TO_ENEMY )
	{
		if ( !m_hEnemy )
		{
			TaskFail();
			return;
		}

		if ( !BuildRoute( m_hEnemy->Center(), bits_MF_TO_ENEMY, m_hEnemy ) && !BuildNearestRoute( m_hEnemy->Center(), m_hEnemy->pev->view_ofs, 0,
		                                                                                          ( m_hEnemy->pev->origin - pev->origin ).Length() ) )
		{
			ALERT( at_aiconsole, "GetPathToEnemy failed!!\n" );
			TaskFail();
			return;
		}

		TaskComplete();
		return;
	}

	if ( pTask->iTask == TASK_RANGE_ATTACK1 )
		BodyBeams( true );

	CBaseMonster::StartTask( pTask );
}

void CVoltigore::SetActivity( Activity activity )
{
	if ( activity == ACT_VICTORY_DANCE )
		pev->health = V_min( pev->max_health, pev->health + 100.0f );

	CBaseMonster::SetActivity( activity == ACT_SMALL_FLINCH ? ACT_MELEE_ATTACK1 : activity );
}

Schedule_t *CVoltigore::GetScheduleOfType( int type )
{
	switch ( type )
	{
		case SCHED_CHASE_ENEMY:
			return slVoltigoreChaseEnemy;
		case SCHED_MELEE_ATTACK1:
			return slVoltigoreSwipe;
		case SCHED_RANGE_ATTACK1:
			return slVoltigoreRangeAttack1;
		case SCHED_VOLTIGORE_EAT:
			return slVoltigoreEat;
		case SCHED_VOLTIGORE_EAT2:
			return slVoltigoreEat2;
		case SCHED_VOLTIGORE_SNIFF_AND_EAT:
			return slVoltigoreSniffAndEat;
		case SCHED_VOLTIGORE_WALLOW:
			return slVoltigoreWallow;
		default:
			return CBaseMonster::GetScheduleOfType( type );
	}
}

Schedule_t *CVoltigore::GetSchedule( void )
{
	BodyBeams( false );

	switch ( m_MonsterState )
	{
		case MONSTERSTATE_ALERT:
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
				return GetScheduleOfType( SCHED_VOLTIGORE_EAT2 );

			if ( HasConditions( bits_COND_SMELL_FOOD ) )
			{
				CSound *scent = PBestScent();

				if ( !scent || ( FInViewCone( &scent->m_vecOrigin ) && FVisible( scent->m_vecOrigin ) ) )
					return GetScheduleOfType( SCHED_VOLTIGORE_EAT );

				return GetScheduleOfType( SCHED_VOLTIGORE_SNIFF_AND_EAT );
			}

			if ( HasConditions( bits_COND_SMELL ) && PBestScent() )
				return GetScheduleOfType( SCHED_VOLTIGORE_WALLOW );

			break;

		case MONSTERSTATE_COMBAT:
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
				break;

			if ( HasConditions( bits_COND_NEW_ENEMY ) )
			{
				AlertSound();
				return GetScheduleOfType( SCHED_WAKE_ANGRY );
			}

			if ( HasConditions( bits_COND_SMELL_FOOD ) )
			{
				CSound *scent = PBestScent();

				if ( !scent || ( FInViewCone( &scent->m_vecOrigin ) && FVisible( scent->m_vecOrigin ) ) )
					return GetScheduleOfType( SCHED_VOLTIGORE_EAT );

				return GetScheduleOfType( SCHED_VOLTIGORE_SNIFF_AND_EAT );
			}

			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

			if ( HasConditions( bits_COND_CAN_MELEE_ATTACK2 ) )
				return GetScheduleOfType( SCHED_MELEE_ATTACK2 );

			return GetScheduleOfType( SCHED_CHASE_ENEMY );

		default:
			break;
	}

	return CBaseMonster::GetSchedule();
}

void CVoltigore::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	if ( pEvent->event == 1 )
	{
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "debris/beamstart1.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 0, 15 ) + 130 );
		UTIL_MakeVectors( pev->angles );

		Vector start = ( gpGlobals->v_forward * 50 + gpGlobals->v_right + gpGlobals->v_up * 23 ) + pev->origin;
		Vector target = gpGlobals->v_forward;

		if ( m_hEnemy )
			target = m_hEnemy->pev->origin + m_hEnemy->pev->view_ofs / 1.5f;

		Vector velocity = ( target - start ).Normalize();

		velocity.x += RANDOM_FLOAT( -0.02f, 0.02f );
		velocity.y += RANDOM_FLOAT( -0.02f, 0.02f );
		velocity.z += RANDOM_FLOAT( -0.02f, 0.02f );
		CVoltShock::Shoot( pev, start, velocity * 1100 );
		BodyBeams( false );
		return;
	}

	if ( pEvent->event != 12 && pEvent->event != 13 )
	{
		CBaseMonster::HandleAnimEvent( pEvent );
		return;
	}

	int damageType = DMG_SLASH | DMG_ALWAYSGIB;

	if ( pEvent->event == 12 && RANDOM_LONG( 0, 10 ) > 5 )
		damageType = DMG_SLASH;

	CBaseEntity *victim = CheckTraceHullAttack( 120.0f, (int) sk_voltigore_dmg_punch.value, damageType );

	if ( !victim )
	{
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "zombie/claw_miss2.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 0, 15 ) + 90 );
		return;
	}

	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "zombie/claw_strike3.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 0, 15 ) + 90 );

	if ( !( victim->pev->flags & FL_CLIENT ) )
		return;

	if ( pEvent->event == 12 )
	{
		victim->pev->punchangle.z = -20;
		victim->pev->punchangle.x = 20;
		victim->pev->velocity = victim->pev->velocity - gpGlobals->v_forward * 100;
	}
	else
	{
		victim->pev->punchangle.z = -25;
		victim->pev->punchangle.x = -25;
		victim->pev->velocity = victim->pev->velocity + gpGlobals->v_up * 50;
	}
}

void CVoltigore::BodyBeams( bool fEnable )
{
	for ( int i = 0; i < ARRAYSIZE( m_hBodyBeams ); i++ )
	{
		if ( m_hBodyBeams[i] )
		{
			UTIL_Remove( m_hBodyBeams[i] );
			m_hBodyBeams[i] = NULL;
		}
	}

	if ( m_hGlow )
		( (CSprite *) (CBaseEntity *) m_hGlow )->SetTransparency( kRenderGlow, 255, 255, 255, 0, kRenderFxNoDissipation );

	if ( !fEnable )
		return;

	for ( int i = 0; i < ARRAYSIZE( m_hBodyBeams ); i++ )
	{
		CBeam *beam = CBeam::BeamCreate( "sprites/lgtning.spr", 50 );

		if ( beam )
		{
			beam->EntsInit( entindex(), entindex() );
			beam->SetStartAttachment( i );
			beam->SetEndAttachment( 4 );
			beam->SetColor( 200, 15, 255 );
			beam->SetBrightness( 255 );
			beam->SetNoise( 30 );
		}

		m_hBodyBeams[i] = beam;
	}
}

void CVoltigore::CreateDeathBeams( void )
{
	if ( m_fDeathBeams )
		return;

	UTIL_MakeVectors( pev->angles );

	for ( int i = 0; i < ARRAYSIZE( m_hDeathBeams ); i++ )
	{
		float right = RANDOM_FLOAT( -1, 1 );
		float up = RANDOM_FLOAT( 0, 1 );
		float forward = RANDOM_FLOAT( -1, 1 );
		Vector end = pev->origin + ( gpGlobals->v_right * right + gpGlobals->v_up * up + gpGlobals->v_forward * forward ) * 256;
		TraceResult trace;

		UTIL_TraceLine( Center(), end, ignore_monsters, edict(), &trace );

		CBeam *beam = CBeam::BeamCreate( "sprites/lgtning.spr", 15 );

		if ( beam )
		{
			beam->PointsInit( Center(), trace.vecEndPos );
			beam->SetColor( 200, 15, 255 );
			beam->SetBrightness( 255 );
			beam->SetNoise( 120 );
			beam->SetWidth( 20 );
			beam->SetFlags( BEAM_FSHADEOUT );
		}

		m_hDeathBeams[i] = beam;
	}

	m_fDeathBeams = true;
}

void CVoltigore::RemoveDeathBeams( void )
{
	for ( int i = 0; i < ARRAYSIZE( m_hDeathBeams ); i++ )
	{
		if ( m_hDeathBeams[i] )
		{
			UTIL_Remove( m_hDeathBeams[i] );
			m_hDeathBeams[i] = NULL;
		}
	}
}

void CVoltigore::UpdateOnRemove( void )
{
	CBaseEntity::UpdateOnRemove();
	RemoveDeathBeams();
}

void CVoltigore::Revive( void )
{
	CBaseMonster::Revive();
	RemoveDeathBeams();
}

Activity CVoltigore::GetDeathActivity( void )
{
	Activity activity = (Activity) ( ACT_DIEBACKWARD + RANDOM_LONG( 0, 1 ) );

	if ( LookupActivity( activity ) == -1 )
		return ACT_DIESIMPLE;

	return activity;
}

void CVoltigore::VoltigoreDeathThink( void )
{
	if ( ( m_fSequenceFinished || pev->frame > 255 ) && gpGlobals->time > m_flDeathTime )
	{
		SpawnGibs();
		SetThink( &CVoltigore::SUB_Remove );
		pev->nextthink = gpGlobals->time;
		return;
	}

	CSoundEnt::InsertSound( bits_SOUND_DANGER, pev->origin, (int) ( sk_voltigore_dmg_explode.value * 1.5f ), 0.1f, this );
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CVoltigore::SpawnGibs( void )
{
	RadiusDamage( pev, pev, sk_voltigore_dmg_explode.value, CLASS_NONE, DMG_POISON | DMG_ACID );
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "weapons/splauncher_impact.wav", 1, ATTN_NORM, 0, PITCH_NORM );

	Vector center = Center();

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
	WRITE_BYTE( TE_SPRITE_SPRAY );
	WRITE_COORD( center.x );
	WRITE_COORD( center.y );
	WRITE_COORD( pev->origin.z );
	WRITE_COORD( 0 );
	WRITE_COORD( 0 );
	WRITE_COORD( 1 );
	WRITE_SHORT( m_iSpitSprite );
	WRITE_BYTE( 250 );
	WRITE_BYTE( 100 );
	WRITE_BYTE( 350 );
	MESSAGE_END();

	CSprite *explosion = CSprite::SpriteCreate( "sprites/spore_exp_01.spr", center, FALSE );

	if ( explosion )
	{
		explosion->SetTransparency( kRenderGlow, 200, 200, 90, 200, kRenderFxNone );
		explosion->SetScale( 4 );
		explosion->AnimateAndDie( 15 );
	}

	for ( int i = 0; i < ARRAYSIZE( m_hGibs ); i++ )
	{
		CGib *gib = GetClassPtr( (CGib *) NULL );

		if ( gib )
		{
			gib->Spawn( "models/vgibs.mdl" );
			gib->pev->body = i;
			gib->pev->origin.x = pev->absmin.x + RANDOM_FLOAT( 0, 1 ) * pev->size.x;
			gib->pev->origin.y = pev->absmin.y + RANDOM_FLOAT( 0, 1 ) * pev->size.y;
			gib->pev->origin.z = pev->origin.z;
			gib->pev->velocity.x += RANDOM_FLOAT( -15, 15 );
			gib->pev->velocity.y += RANDOM_FLOAT( -15, 15 );
			gib->pev->velocity.z += RANDOM_FLOAT( 20, 40 );
			gib->pev->velocity = gib->pev->velocity * 30;
			gib->pev->avelocity.x = RANDOM_FLOAT( 100, 200 );
			gib->pev->avelocity.y = RANDOM_FLOAT( 100, 300 );
			gib->pev->avelocity.z = RANDOM_FLOAT( 100, 200 );
			gib->m_bloodColor = BloodColor();
			gib->pev->solid = SOLID_SLIDEBOX;
			UTIL_SetSize( gib->pev, g_vecZero, g_vecZero );
		}

		m_hGibs[i] = gib;
	}

	for ( int i = 0; i < ARRAYSIZE( m_hDeathBeams ); i++ )
	{
		CBeam *beam = (CBeam *) (CBaseEntity *) m_hDeathBeams[i];

		if ( !beam )
			continue;

		beam->SetNoise( 150 );
		beam->SetWidth( 40 );
		beam->pev->flags &= ~FL_NOTARGET;
		beam->SetThink( &CBeam::SUB_Remove );
		beam->pev->nextthink = gpGlobals->time + 3.0f;

		int first = i < 9 ? i : 8;
		int second = i < 9 ? i + 1 : 9;
		CBaseEntity *firstGib = m_hGibs[first];
		CBaseEntity *secondGib = m_hGibs[second];

		if ( firstGib && secondGib )
			beam->EntsInit( firstGib->entindex(), secondGib->entindex() );

		int adjacent = i < 9 ? i + 1 : i - 1;
		CBeam *next = (CBeam *) (CBaseEntity *) m_hDeathBeams[adjacent];

		if ( next )
		{
			next->SetNoise( 150 );
			next->SetWidth( 40 );
			next->pev->flags &= ~FL_NOTARGET;
		}
	}

	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "common/bodysplat.wav", 0.5f, ATTN_NORM, 0, PITCH_NORM );
}

void CVoltigore::DeathSound( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, pVoltigoreDieSounds[RANDOM_LONG( 0, 2 )], 1, ATTN_IDLE, 0, PITCH_NORM );
	EMIT_SOUND_DYN( edict(), CHAN_ITEM, RANDOM_LONG( 0, 1 ) ? "debris/zap4.wav" : "debris/zap6.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
	pev->takedamage = DAMAGE_NO;
	BecomeDead( 1000 );
	pev->deadflag = DEAD_DEAD;
	FCheckAITrigger();
	BodyBeams( false );
	SetThink( &CVoltigore::VoltigoreDeathThink );
	pev->nextthink = gpGlobals->time + 0.1f;
	m_flDeathTime = gpGlobals->time + RANDOM_LONG( 4, 8 );
	CreateDeathBeams();
}

void CVoltigore::CallGibMonster( void )
{
	SetTouch( NULL );
	BecomeDead( pev->health );
	pev->takedamage = DAMAGE_NO;
	pev->solid = SOLID_NOT;
	GibMonster();
}

void CVoltigore::GibMonster( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_ITEM, "debris/zap7.wav", 1, ATTN_IDLE, 0, PITCH_NORM );
	SetActivity( GetDeathActivity() );
	pev->deadflag = DEAD_DEAD;
	FCheckAITrigger();
	BodyBeams( false );
	SpawnGibs();
	SetThink( &CVoltigore::SUB_Remove );
	pev->nextthink = gpGlobals->time;
}

void CVoltShock::Spawn( void )
{
	pev->movetype = MOVETYPE_FLY;
	pev->classname = MAKE_STRING( "voltigoreshock" );
	pev->rendermode = kRenderGlow;
	pev->renderamt = 200;
	pev->solid = SOLID_BBOX;
	UTIL_SetModel( this, "sprites/blueflare2.spr" );
	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	m_iBeamCount = 0;
	m_flExpireTime = gpGlobals->time;
	m_flNextBeamTime = gpGlobals->time;
}

void CVoltShock::Shoot( entvars_t *pevOwner, Vector start, Vector velocity )
{
	CVoltShock *shock = GetClassPtr( (CVoltShock *) NULL );

	if ( !shock )
		return;

	shock->Spawn();
	UTIL_SetOrigin( shock->pev, start );
	shock->pev->angles = UTIL_VecToAngles( velocity );
	shock->pev->velocity = velocity;
	shock->pev->owner = pevOwner ? ENT( pevOwner ) : NULL;
	shock->SetThink( &CVoltShock::FlyShockThink );
	shock->pev->nextthink = gpGlobals->time;
}

void CVoltShock::TraceBeam( TraceResult &trace, float distance )
{
	float right = RANDOM_FLOAT( -1, 1 );
	float up = RANDOM_FLOAT( -1, 1 );
	float forward = RANDOM_FLOAT( -1, 1 );
	Vector direction = gpGlobals->v_up * up + gpGlobals->v_right * right + gpGlobals->v_forward * forward;

	UTIL_TraceLine( pev->origin, pev->origin + direction * distance, ignore_monsters, edict(), &trace );
}

void CVoltShock::FlyShockThink( void )
{
	if ( m_iBeamCount < 7 )
	{
		TraceResult trace;

		TraceBeam( trace, 768 );

		CBeam *beam = CBeam::BeamCreate( "sprites/lgtning.spr", 30 );

		if ( beam )
		{
			if ( trace.flFraction == 1.0f )
				TraceBeam( trace, 1024 );

			if ( trace.flFraction != 1.0f )
				beam->PointEntInit( trace.vecEndPos, entindex() );

			beam->SetColor( 200, 15, 255 );
			beam->SetBrightness( 255 );
			beam->SetNoise( 70 );
			beam->SetWidth( 40 );
			m_hBeams[m_iBeamCount] = beam;
		}

		m_iBeamCount++;
	}

	if ( m_iBeamCount >= 6 )
	{
		SetThink( &CVoltShock::IgniteThink );
		m_iCurrentBeam = 0;
	}

	pev->nextthink = gpGlobals->time + 0.1f;
}

void CVoltShock::IgniteThink( void )
{
	TraceResult trace;

	TraceBeam( trace, 768 );

	CBeam *beam = (CBeam *) (CBaseEntity *) m_hBeams[m_iCurrentBeam];

	if ( beam )
	{
		if ( trace.flFraction == 1.0f )
			TraceBeam( trace, 1024 );

		if ( trace.flFraction != 1.0f )
			beam->PointEntInit( trace.vecEndPos, entindex() );
	}

	m_iCurrentBeam = m_iCurrentBeam < 6 ? m_iCurrentBeam + 1 : 0;
	pev->nextthink = gpGlobals->time;
}

void CVoltShock::Touch( CBaseEntity *pOther )
{
	SetTouch( NULL );
	pev->angles = UTIL_VecToAngles( pev->velocity.Normalize() );
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_FLY;
	pev->velocity = g_vecZero;
	pev->avelocity.z = 0;
	pev->angles.z = RANDOM_LONG( 0, 360 );

	if ( pOther->pev->takedamage )
	{
		entvars_t *attacker = pev->owner ? VARS( pev->owner ) : NULL;
		float damage = ( pOther->pev->flags & FL_MONSTER ) && !FClassnameIs( pOther->pev, "monster_apache" ) ? 20.0f : 40.0f;

		pOther->TakeDamage( pev, attacker, damage, DMG_GENERIC );
	}
	else if ( FClassnameIs( pOther->pev, "worldspawn" ) )
	{
		TraceResult trace = UTIL_GetGlobalTrace();

		UTIL_DecalTrace( &trace, DECAL_SMALLSCORCH1 + RANDOM_LONG( 0, 1 ) );
	}

	m_iCurrentBeam = 0;
	SetThink( &CVoltShock::RadiusDamageThink );
	m_flExpireTime = gpGlobals->time + 0.5f;
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CVoltShock::RadiusDamageThink( void )
{
	if ( m_iBeamCount < 7 )
	{
		TraceResult trace;

		TraceBeam( trace, 768 );

		CBeam *beam = CBeam::BeamCreate( "sprites/lgtning.spr", 30 );

		if ( beam )
		{
			if ( trace.flFraction == 1.0f )
				TraceBeam( trace, 1024 );

			if ( trace.flFraction != 1.0f )
				beam->PointEntInit( trace.vecEndPos, entindex() );

			beam->SetColor( 200, 15, 255 );
			beam->SetBrightness( 255 );
			beam->SetNoise( 70 );
			beam->SetWidth( 40 );
		}

		m_hBeams[m_iBeamCount] = beam;
		m_iBeamCount++;
	}

	entvars_t *attacker = pev->owner ? VARS( pev->owner ) : NULL;

	::RadiusDamage( pev->origin, pev, attacker, 16.0f, 128.0f, CLASS_XRACE_SHOCK, DMG_SHOCK );

	if ( gpGlobals->time > m_flExpireTime )
		SetThink( &CVoltShock::RemoveThink );

	pev->nextthink = gpGlobals->time + 0.1f;
}

void CVoltShock::RemoveThink( void )
{
	for ( int i = 0; i < ARRAYSIZE( m_hBeams ); i++ )
	{
		if ( m_hBeams[i] )
		{
			UTIL_Remove( m_hBeams[i] );
			m_hBeams[i] = NULL;
		}
	}

	SetThink( &CVoltShock::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1f;
}
