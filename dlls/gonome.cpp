#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "soundent.h"
#include "decals.h"
#include "weapons.h"
#include "game.h"
#include "gonome.h"

extern cvar_t sk_gonome_health;
extern cvar_t sk_gonome_dmg_one_slash;
extern cvar_t sk_gonome_dmg_guts;
extern cvar_t sk_gonome_dmg_one_bite;

LINK_ENTITY_TO_CLASS( monster_gonome, CGonome );
LINK_ENTITY_TO_CLASS( monster_gonome_dead, CGonomeDead );
LINK_ENTITY_TO_CLASS( gonomespit, CGonomeSpit );

enum { SCHED_GONOME_EAT = 59 };

static const char *gonomeAlertSounds[] =
{
	"zombie/zo_alert10.wav",
	"zombie/zo_alert20.wav",
	"zombie/zo_alert30.wav"
};

static const char *gonomeClawSounds[] =
{
	"zombie/claw_strike1.wav",
	"zombie/claw_strike2.wav",
	"zombie/claw_strike3.wav"
};

static const char *gonomePoses[] =
{
	"dead_on_stomach1_1",
	"dead_on_back",
	"dead_on_side",
	"dead_on_stomach1_2"
};

TYPEDESCRIPTION CGonomeSpit::m_SaveData[] =
{
	DEFINE_FIELD( CGonomeSpit, m_maxFrame, FIELD_INTEGER )
};

IMPLEMENT_SAVERESTORE( CGonomeSpit, CBaseEntity );

TYPEDESCRIPTION CGonome::m_SaveData[] =
{
	DEFINE_FIELD( CGonome, m_fCanThreatDisplay, FIELD_CHARACTER ),
	DEFINE_FIELD( CGonome, m_flLastHurtTime, FIELD_TIME ),
	DEFINE_FIELD( CGonome, m_flNextSpitTime, FIELD_TIME )
};

IMPLEMENT_SAVERESTORE( CGonome, CBaseMonster );

Task_t tlGonomeRangeAttack[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_SET_ACTIVITY, ACT_IDLE }
};

Schedule_t slGonomeRangeAttack[] =
{
	{
		tlGonomeRangeAttack,
		ARRAYSIZE( tlGonomeRangeAttack ),
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD,
		0,
		"Squid Range Attack1"
	}
};

Task_t tlGonomeChaseEnemy[] =
{
	{ TASK_SET_FAIL_SCHEDULE, SCHED_RANGE_ATTACK1 },
	{ TASK_MOVE_TO_ENEMY, 55 }
};

Schedule_t slGonomeChaseEnemy[] =
{
	{
		tlGonomeChaseEnemy,
		ARRAYSIZE( tlGonomeChaseEnemy ),
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD |
		bits_COND_TASK_FAILED,
		0,
		"Squid Chase Enemy"
	}
};

Task_t tlGonomeEat[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_WAIT, 1.5 },
	{ TASK_GET_PATH_TO_ENEMY_CORPSE, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, ACT_VICTORY_DANCE },
	{ TASK_SET_ACTIVITY, ACT_IDLE }
};

Schedule_t slGonomeEat[] =
{
	{
		tlGonomeEat,
		ARRAYSIZE( tlGonomeEat ),
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY,
		0,
		"GonomeEat"
	}
};

DEFINE_CUSTOM_SCHEDULES( CGonome )
{
	slGonomeRangeAttack,
	slGonomeChaseEnemy,
	slGonomeEat
};

IMPLEMENT_CUSTOM_SCHEDULES( CGonome, CBaseMonster );

void CGonomeSpit::Spawn( void )
{
	pev->movetype = MOVETYPE_BOUNCE;
	pev->gravity = 0.7f;
	pev->classname = MAKE_STRING( "gonomespit" );
	pev->solid = SOLID_BBOX;
	pev->frame = 0;
	pev->scale = 0.5f;
	pev->rendercolor = Vector( 255, 0, 0 );
	pev->rendermode = kRenderTransAlpha;
	pev->renderamt = 150;
	UTIL_SetModel( this, "sprites/blood_chnk.spr" );
	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	m_maxFrame = MODEL_FRAMES( pev->modelindex ) - 1;
}

void CGonomeSpit::Animate( void )
{
	pev->nextthink = gpGlobals->time + 0.1f;

	if ( pev->frame++ && pev->frame > m_maxFrame )
		pev->frame = 0;
}

void CGonomeSpit::Shoot( entvars_t *owner, Vector start, Vector velocity )
{
	CGonomeSpit *spit = GetClassPtr( (CGonomeSpit *) NULL );

	spit->Spawn();
	UTIL_SetOrigin( spit->pev, start );
	spit->pev->velocity = velocity;
	spit->pev->owner = owner ? ENT( owner ) : NULL;
	spit->SetThink( &CGonomeSpit::Animate );
	spit->pev->nextthink = gpGlobals->time + 0.1f;
}

void CGonomeSpit::Touch( CBaseEntity *other )
{
	int pitch = (int) RANDOM_FLOAT( 90, 110 );

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "bullchicken/bc_acid1.wav", 1, ATTN_NORM, 0, pitch );
	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, RANDOM_LONG( 0, 1 ) ? "bullchicken/bc_spithit2.wav" : "bullchicken/bc_spithit1.wav", 1, ATTN_NORM, 0, pitch );

	TraceResult trace;

	trace.vecEndPos = g_vecZero;
	trace.vecPlaneNormal = g_vecZero;
	UTIL_TraceLine( pev->origin, pev->origin + pev->velocity * 10, dont_ignore_monsters, edict(), &trace );

	if ( other->pev->takedamage == DAMAGE_NO )
		UTIL_DecalTrace( &trace, DECAL_BLOOD1 + RANDOM_LONG( 0, 5 ) );
	else
		other->TakeDamage( pev, pev->owner ? VARS( pev->owner ) : NULL, sk_gonome_dmg_guts.value, DMG_ACID );

	SpawnBlood( trace.vecEndPos, BLOOD_COLOR_RED, 40 );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time;
}

void CGonome::Spawn( void )
{
	Precache();

	if ( !SetupModel() )
		UTIL_SetModel( this, "models/gonome.mdl" );

	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 80 ) );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_YELLOW;
	pev->effects = 0;

	if ( pev->health == 0 )
		pev->health = sk_gonome_health.value;

	m_flFieldOfView = -0.7f;
	m_MonsterState = MONSTERSTATE_NONE;
	m_fCanThreatDisplay = true;
	m_flNextSpitTime = gpGlobals->time;
	m_afCapability = bits_CAP_DOORS_GROUP | bits_CAP_RANGE_ATTACK1 | bits_CAP_MELEE_ATTACK1 | bits_CAP_MELEE_ATTACK2;
	m_afMoveShootCap = bits_MOVESHOOTCAP_MELEE_ATTACK1 | bits_MOVESHOOTCAP_MELEE_ATTACK2 | bits_MOVESHOOTCAP_RANGE_ATTACK1;

	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( "Gonome" );

	MonsterInit();

	if ( IsPlayerAlly() && !( pev->spawnflags & SF_MONSTER_PREDISASTER ) )
	{
		SetUse( &CBaseMonster::CallFollowerPlayerUse );
		m_fCanPlayerFollow = true;
	}
}

void CGonome::Precache( void )
{
	CBaseMonster::Precache();

	if ( !pev->model )
		UTIL_PrecacheModel( this, "models/gonome.mdl" );

	UTIL_PrecacheModel( this, "sprites/blood_chnk.spr" );
	PrecacheSound_Intercept( this, "zombie/claw_miss2.wav" );
	PrecacheSound_Intercept( this, "gonome/gonome_melee1.wav" );
	PrecacheSound_Intercept( this, "gonome/gonome_melee2.wav" );

	static const char *sounds[] =
	{
		"gonome/gonome_death2.wav", "gonome/gonome_death3.wav", "gonome/gonome_death4.wav",
		"gonome/gonome_idle1.wav", "gonome/gonome_idle2.wav", "gonome/gonome_idle3.wav",
		"gonome/gonome_pain1.wav", "gonome/gonome_pain2.wav", "gonome/gonome_pain3.wav", "gonome/gonome_pain4.wav",
		"gonome/gonome_eat.wav", "gonome/gonome_jumpattack.wav", "bullchicken/bc_acid1.wav",
		"bullchicken/bc_spithit1.wav", "bullchicken/bc_spithit2.wav"
	};

	for ( int i = 0; i < ARRAYSIZE( sounds ); i++ )
		PrecacheSound_Intercept( this, sounds[i] );

	for ( int i = 0; i < ARRAYSIZE( gonomeClawSounds ); i++ )
		PrecacheSound_Intercept( this, gonomeClawSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( gonomeAlertSounds ); i++ )
		PrecacheSound_Intercept( this, gonomeAlertSounds[i] );
}

int CGonome::ObjectCaps( void )
{
	return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

int CGonome::Classify( void )
{
	return GetClassification( CLASS_ALIEN_MONSTER );
}

void CGonome::SetYawSpeed( void )
{
	pev->yaw_speed = 360;
}

int CGonome::ISoundMask( void )
{
	return 95;
}

void CGonome::HandleAnimEvent( MonsterEvent_t *event )
{
	int pitch = RANDOM_LONG( 0, 10 ) + 100;

	if ( event->event == 1 || event->event == 2 || ( event->event >= 19 && event->event <= 22 ) )
	{
		bool bite = event->event >= 19;
		int damageType = event->event == 1 ? DMG_CLUB | DMG_ALWAYSGIB : DMG_SLASH;
		CBaseEntity *hit = CheckTraceHullAttack( bite ? 30 : 85,
			(int) ( bite ? sk_gonome_dmg_one_bite.value : sk_gonome_dmg_one_slash.value ), damageType );

		if ( !hit )
		{
			EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "zombie/claw_miss2.wav", 1, ATTN_NORM, 0, pitch );
			return;
		}

		EMIT_SOUND( edict(), CHAN_WEAPON, gonomeClawSounds[RANDOM_LONG( 0, 2 )], 1, ATTN_NORM );

		if ( ( hit->pev->flags & FL_CLIENT ) || ( ( hit->pev->flags & FL_MONSTER ) && hit->pev->movetype != MOVETYPE_FLY ) )
		{
			if ( event->event == 1 )
			{
				hit->pev->punchangle.z = -20;
				hit->pev->punchangle.x = 20;
				hit->pev->velocity = hit->pev->velocity - gpGlobals->v_right * 200;
			}
			else
			{
				if ( bite )
				{
					hit->pev->punchangle.z = -15;
					hit->pev->punchangle.x = -15;
				}

				hit->pev->velocity = hit->pev->velocity - gpGlobals->v_forward * 100;
			}
		}

		return;
	}

	switch ( event->event )
	{
	case 3:
	{
		Vector position( 0, 0, 0 ), angles( 0, 0, 0 );

		GetAttachment( 0, position, angles );
		SpawnBlood( position, BLOOD_COLOR_RED, 40 );
		break;
	}
	case 4:
	{
		Vector position( 0, 0, 0 ), angles( 0, 0, 0 );

		UTIL_MakeVectors( pev->angles );
		GetAttachment( 0, position, angles );

		if ( m_hEnemy == NULL )
			break;

		Vector velocity = VecCheckThrow( pev, position, m_hEnemy->Center(), 1500, 0.7f );

		if ( velocity == g_vecZero )
			velocity = ( m_hEnemy->BodyTarget( position ) - position ).Normalize() * 1500;

		velocity.x += RANDOM_FLOAT( -0.05f, 0.05f );
		velocity.y += RANDOM_FLOAT( -0.05f, 0.05f );
		velocity.z += RANDOM_FLOAT( -0.05f, 0.05f );
		CGonomeSpit::Shoot( pev, position, velocity );
		break;
	}
	default:
		CBaseMonster::HandleAnimEvent( event );
		break;
	}
}

void CGonome::IdleSound( void )
{
	static const char *sounds[] = { "gonome/gonome_idle1.wav", "gonome/gonome_idle2.wav", "gonome/gonome_idle3.wav" };

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, sounds[RANDOM_LONG( 0, 2 )], 1, 0.7f, 0, 100 );
}

void CGonome::PainSound( void )
{
	if ( RANDOM_LONG( 0, 2 ) != 0 )
		return;

	int pitch = RANDOM_LONG( 85, 120 );
	static const char *sounds[] = { "gonome/gonome_pain1.wav", "gonome/gonome_pain2.wav", "gonome/gonome_pain3.wav", "gonome/gonome_pain4.wav" };

	EMIT_SOUND_DYN( edict(), CHAN_BODY, sounds[RANDOM_LONG( 0, 3 )], 1, ATTN_NORM, 0, pitch );
}

void CGonome::DeathSound( void )
{
	static const char *sounds[] = { "gonome/gonome_death2.wav", "gonome/gonome_death3.wav", "gonome/gonome_death4.wav" };

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, sounds[RANDOM_LONG( 0, 2 )], 1, ATTN_NORM, 0, 100 );
}

void CGonome::AlertSound( void )
{
	int pitch = RANDOM_LONG( 0, 9 ) + 95;

	EMIT_SOUND_DYN( edict(), CHAN_BODY, gonomeAlertSounds[RANDOM_LONG( 0, 2 )], 1, ATTN_NORM, 0, pitch );
}

void CGonome::UseSound( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "gonome/gonome_pain1.wav", 1, ATTN_NORM, 0, 100 );
}

void CGonome::UnUseSound( void )
{
	ChangeSchedule( GetScheduleOfType( SCHED_SMALL_FLINCH ) );
}

BOOL CGonome::CheckMeleeAttack1( float dot, float distance )
{
	if ( !( distance <= 64 && distance >= 55 && dot >= 0.5f ) )
		return FALSE;

	if ( m_hEnemy == NULL || !m_hEnemy->IsMoving() )
		return TRUE;

	return m_hEnemy->IsPlayer() && !( m_hEnemy->pev->velocity.Length2D() > 100 );
}

BOOL CGonome::CheckMeleeAttack1_Move( float dot, float distance )
{
	return distance <= 164 && distance >= 55 && dot >= 0.5f;
}

BOOL CGonome::CheckMeleeAttack2( float dot, float distance )
{
	if ( !( distance <= 65 && dot >= 0.5f ) || HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
		return FALSE;

	return m_hEnemy == NULL || !m_hEnemy->IsMoving();
}

BOOL CGonome::CheckMeleeAttack2_Move( float dot, float distance )
{
	return distance <= 128 && dot >= 0.5f && !HasConditions( bits_COND_CAN_MELEE_ATTACK1_MOVING );
}

BOOL CGonome::CheckRangeAttack1( float dot, float distance )
{
	if ( m_flSpitDelayTime > gpGlobals->time || ( IsMoving() && distance >= 512 )
		|| !( distance > 192 && distance <= 768 && dot >= 0.5f && gpGlobals->time >= m_flNextSpitTime ) )
		return FALSE;

	if ( m_hEnemy != NULL && fabs( pev->origin.z - m_hEnemy->pev->origin.z ) > 512 )
		return FALSE;

	m_flNextSpitTime = IsMoving() ? gpGlobals->time + 8 : gpGlobals->time + 1.5;

	if ( m_hEnemy == NULL )
		return FALSE;

	Vector start = pev->origin + Vector( -40, 0, 90 );
	TraceResult trace;

	trace.vecEndPos = g_vecZero;
	trace.vecPlaneNormal = g_vecZero;
	UTIL_TraceLine( start, m_hEnemy->BodyTarget( start ), ignore_monsters, ignore_glass, edict(), &trace );

	return trace.flFraction == 1;
}

BOOL CGonome::CheckRangeAttack1_Move( float dot, float distance )
{
	if ( m_flSpitDelayTime > gpGlobals->time || ( IsMoving() && distance >= 512 )
		|| !( distance > 256 && distance <= 784 && dot >= 0.5f && gpGlobals->time >= m_flNextSpitTime ) )
		return FALSE;

	if ( m_hEnemy != NULL && fabs( pev->origin.z - m_hEnemy->pev->origin.z ) > 512 )
		return FALSE;

	m_flNextSpitTime = IsMoving() ? gpGlobals->time + 8 : gpGlobals->time + 1.5;

	if ( m_hEnemy == NULL )
		return FALSE;

	Vector start = pev->origin + Vector( -40, 0, 90 );
	TraceResult trace;

	trace.vecEndPos = g_vecZero;
	trace.vecPlaneNormal = g_vecZero;
	UTIL_TraceLine( start, m_hEnemy->BodyTarget( start ), ignore_monsters, ignore_glass, edict(), &trace );

	return trace.flFraction == 1;
}

void CGonome::RunAI( void )
{
	CBaseMonster::RunAI();

	if ( m_hEnemy != NULL && m_Activity == ACT_RUN )
		pev->framerate = ( pev->origin - m_hEnemy->pev->origin ).Length2D() >= 256 ? 1.1f : 1.3f;

	if ( m_Activity == ACT_VICTORY_DANCE )
		pev->health = fminf( pev->max_health, pev->health + 150 );

	if ( m_Activity == ACT_RANGE_ATTACK1 )
		pev->framerate = 1.3f;

	if ( m_Activity == ACT_MELEE_ATTACK1 )
		pev->framerate = 1.1f;

	if ( HasConditions( bits_COND_NEW_ENEMY ) )
		m_flSpitDelayTime = gpGlobals->time + 4;
}

void CGonome::StartTask( Task_t *task )
{
	if ( task->iTask != TASK_MOVE_TO_ENEMY )
	{
		CBaseMonster::StartTask( task );
		return;
	}

	if ( m_hEnemy == NULL )
	{
		TaskComplete();
		return;
	}

	PainSound();
	m_vecMoveGoal = m_hEnemy->pev->origin;

	if ( !MoveToEnemy( ACT_RUN, 2, 0 ) )
		SetConditions( bits_COND_TASK_FAILED );
}

void CGonome::RunTask( Task_t *task )
{
	if ( task->iTask == TASK_MELEE_ATTACK1 || task->iTask == TASK_MELEE_ATTACK2 )
	{
		if ( m_hEnemy != NULL )
		{
			float distance = ( m_hEnemy->Center() - Center() ).Length();

			if ( distance > 64 || distance < 55 )
				TaskComplete();
			else if ( m_hEnemy->IsMoving()
				&& ( !m_hEnemy->IsPlayer() || m_hEnemy->pev->velocity.Length2D() > 100 ) )
				TaskComplete();
		}

		CBaseMonster::RunTask( task );
		return;
	}

	if ( task->iTask != TASK_MOVE_TO_ENEMY )
	{
		CBaseMonster::RunTask( task );
		return;
	}

	if ( m_hEnemy == NULL )
	{
		TaskComplete();
		return;
	}

	float distance = ( m_vecMoveGoal - pev->origin ).Length();

	if ( task->flData > distance || distance > task->flData * 0.5f )
	{
		m_vecMoveGoal = m_hEnemy->pev->origin;
		m_movementGoal = MOVEGOAL_ENEMY;
		FRefreshRoute( 0 );
	}

	int conditions = GetConditions();

	CheckAttacks( m_hEnemy, distance );

	if ( task->flData > distance || HasConditions( bits_COND_CAN_ATTACK ) )
	{
		ClearConditions( -1 );
		SetConditions( conditions );
		MakeIdealYaw( m_vecEnemyLKP );
		ChangeYaw( (int) pev->yaw_speed );
		RouteClear();
	}
	else
	{
		ClearConditions( -1 );
		SetConditions( conditions );
		m_movementActivity = ACT_RUN;
	}
}

Schedule_t *CGonome::GetSchedule( void )
{
	if ( m_MonsterState == MONSTERSTATE_COMBAT )
	{
		if ( HasConditions( bits_COND_ENEMY_DEAD ) )
		{
			m_hEnemy = NULL;

			if ( GetEnemy() )
				ClearConditions( bits_COND_ENEMY_DEAD );
			else
				SetState( MONSTERSTATE_ALERT );

			return GetSchedule();
		}

		if ( HasConditions( bits_COND_NEW_ENEMY ) )
			return GetScheduleOfType( SCHED_WAKE_ANGRY );

		if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
			return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

		if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK2 ) )
			return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

		return GetScheduleOfType( SCHED_CHASE_ENEMY );
	}

	if ( m_MonsterState == MONSTERSTATE_ALERT && HasConditions( bits_COND_ENEMY_DEAD ) )
		return GetScheduleOfType( SCHED_GONOME_EAT );

	return CBaseMonster::GetSchedule();
}

Schedule_t *CGonome::GetScheduleOfType( int type )
{
	if ( type == SCHED_CHASE_ENEMY_FAILED )
		return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

	if ( type == SCHED_RANGE_ATTACK1 )
		return slGonomeRangeAttack;

	if ( type == SCHED_GONOME_EAT )
		return slGonomeEat;

	return CBaseMonster::GetScheduleOfType( type );
}

void CGonome::SetActivity( Activity activity )
{
	int sequence;

	if ( activity == ACT_RANGE_ATTACK1 )
	{
		sequence = LookupSequence( "attack3" );
		EMIT_SOUND_DYN( edict(), CHAN_VOICE, "gonome/gonome_eat.wav", 1, 2, 0, 100 );
	}
	else if ( activity == ACT_MELEE_ATTACK1 || activity == ACT_MELEE_ATTACK2 )
	{
		bool second = HasConditions( bits_COND_CAN_MELEE_ATTACK2 | bits_COND_CAN_MELEE_ATTACK2_MOVING );

		sequence = LookupSequence( second ? "attack2" : "attack1" );

		if ( m_hEnemy != NULL && m_hEnemy->IsAlive() )
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, second ? "gonome/gonome_melee2.wav" : "gonome/gonome_melee1.wav", 1, ATTN_NORM, 0, 100 );
		else
			IdleSound();
	}
	else
		sequence = LookupActivity( activity );

	m_Activity = activity;
	m_IdealActivity = activity;

	if ( sequence < 0 )
	{
		ALERT( at_console, "%s has no sequence for act:%d\n", STRING( pev->classname ), activity );
		pev->sequence = 0;
		return;
	}

	if ( pev->sequence != sequence || !m_fSequenceLoops )
		pev->frame = 0;

	pev->sequence = sequence;
	ResetSequenceInfo();
	SetYawSpeed();
}

int CGonome::IgnoreConditions( void )
{
	int conditions = CBaseMonster::IgnoreConditions();

	if ( m_Activity == ACT_MELEE_ATTACK1 )
		conditions |= bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE;

	if ( ( m_Activity == ACT_SMALL_FLINCH || m_Activity == ACT_BIG_FLINCH ) && gpGlobals->time > m_flLastHurtTime )
		m_flLastHurtTime = gpGlobals->time + 2;

	return conditions;
}

BOOL CGonome::FValidateHintType( short type )
{
	if ( type == 10 )
		return TRUE;

	ALERT( at_aiconsole, "Couldn't validate hint type" );
	return FALSE;
}

int CGonome::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	CBaseEntity *entity = CBaseEntity::Instance( attacker );

	if ( damageType & ( DMG_BULLET | DMG_SLASH | DMG_CLUB | DMG_SNIPER ) )
		damage *= 0.35f;

	if ( IsFriendly( entity ) )
		damage = 0;

	return CBaseMonster::TakeDamage( inflictor, attacker, damage, damageType );
}

float CGonome::GetPointsForDamage( float damage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	return pev->max_health / sk_gonome_health.value * 10 * ( damage / pev->max_health );
}

void CGonomeDead::Spawn( void )
{
	Precache();
	pev->health = sk_gonome_health.value;
	m_bloodColor = BLOOD_COLOR_YELLOW;
	pev->sequence = LookupSequence( gonomePoses[m_iPose] );

	if ( pev->sequence == -1 )
		ALERT( at_console, "monster_gonome_dead at ( %f, %f, %f ) with bad/missing pose!\n", pev->origin.x, pev->origin.y, pev->origin.z );

	MonsterInitDead();
}

void CGonomeDead::Precache( void )
{
	if ( pev->model )
	{
		UTIL_PrecacheModel( this, STRING( pev->model ) );
		UTIL_SetModel( this, STRING( pev->model ) );
	}
	else
	{
		UTIL_PrecacheModel( this, "models/gonome.mdl" );
		UTIL_SetModel( this, "models/gonome.mdl" );
	}
}

void CGonomeDead::KeyValue( KeyValueData *data )
{
	if ( !FStrEq( data->szKeyName, "pose" ) )
	{
		CBaseMonster::KeyValue( data );
		return;
	}

	unsigned int pose = strtoul( data->szValue, NULL, 10 );

	m_iPose = V_min( pose, 3u );
	data->fHandled = TRUE;
}
