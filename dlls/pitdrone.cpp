#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "squadmonster.h"
#include "schedule.h"
#include "nodes.h"
#include "soundent.h"
#include "weapons.h"
#include "pitdrone.h"

extern cvar_t sk_pitdrone_health;
extern cvar_t sk_pitdrone_dmg_whip;
extern cvar_t sk_pitdrone_dmg_bite;
extern cvar_t sk_pitdrone_dmg_spit;

int iPitdroneSpikeModel;
int iSpikeTrail;

enum
{
	SCHED_PITDRONE_HURTHOP = LAST_COMMON_SCHEDULE + 1,
	SCHED_PITDRONE_SMELLFOOD,
	SCHED_PITDRONE_SEECRAB,
	SCHED_PITDRONE_EAT,
	SCHED_PITDRONE_EAT2,
	SCHED_PITDRONE_SNIFF_AND_EAT,
	SCHED_PITDRONE_WALLOW,
	SCHED_PITDRONE_RELOAD
};

enum
{
	TASK_PITDRONE_HOPTURN = LAST_COMMON_TASK + 1
};

namespace
{
const char *pPitdroneIdleSounds[] = {
	"pitdrone/pit_drone_idle1.wav",
	"pitdrone/pit_drone_idle2.wav",
	"pitdrone/pit_drone_idle3.wav"
};

const char *pPitdronePainSounds[] = {
	"pitdrone/pit_drone_pain1.wav",
	"pitdrone/pit_drone_pain2.wav",
	"pitdrone/pit_drone_pain3.wav",
	"pitdrone/pit_drone_pain4.wav"
};

const char *pPitdroneAlertSounds[] = {
	"pitdrone/pit_drone_alert1.wav",
	"pitdrone/pit_drone_alert2.wav",
	"pitdrone/pit_drone_alert3.wav"
};
}

LINK_ENTITY_TO_CLASS( monster_pitdrone, CPitdrone );
LINK_ENTITY_TO_CLASS( pitdronespike, CSpike );

TYPEDESCRIPTION CPitdrone::m_SaveData[] = {
	DEFINE_FIELD( CPitdrone, m_flLastHurtTime, FIELD_TIME ),
	DEFINE_FIELD( CPitdrone, m_flNextSpitTime, FIELD_TIME )
};

IMPLEMENT_SAVERESTORE( CPitdrone, CSquadMonster );

void CSpike::Spawn( void )
{
	Vector direction = gpGlobals->v_forward;

	UTIL_SetModel( this, "models/pit_drone_spike.mdl" );
	pev->movetype = MOVETYPE_BOUNCEMISSILE;
	pev->classname = MAKE_STRING( "pitdronespike" );
	pev->angles = UTIL_VecToAngles( direction );
	pev->solid = SOLID_BBOX;
	pev->frame = 0;
	pev->scale = 1.5f;
	UTIL_SetSize( pev, Vector( -1, -1, -1 ), Vector( 1, 1, 1 ) );
	SetThink( &CSpike::IgniteThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CSpike::Precache( void )
{
	iSpikeTrail = UTIL_PrecacheModel( this, "sprites/spike_trail.spr" );
}

void CSpike::IgniteThink( void )
{
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
	WRITE_BYTE( TE_BEAMFOLLOW );
	WRITE_SHORT( entindex() );
	WRITE_SHORT( iSpikeTrail );
	WRITE_BYTE( 3 );
	WRITE_BYTE( (int) ( pev->scale * 1.5 ) );
	WRITE_BYTE( 150 );
	WRITE_BYTE( 165 );
	WRITE_BYTE( 30 );
	WRITE_BYTE( 255 );
	MESSAGE_END();

	SetThink( &CSpike::FlySpikeThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CSpike::FlySpikeThink( void )
{
	UTIL_MakeAimVectors( pev->angles );
	pev->angles = UTIL_VecToAngles( gpGlobals->v_forward );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CSpike::Shoot( entvars_t *owner, Vector start, Vector velocity )
{
	CSpike *spike = GetClassPtr( (CSpike *) NULL );

	spike->Spawn();
	UTIL_SetOrigin( spike->pev, start );
	spike->pev->angles = UTIL_VecToAngles( velocity );
	spike->pev->velocity = velocity;
	spike->pev->owner = ENT( owner );
	spike->pev->scale *= owner->scale;
	spike->SetThink( &CSpike::IgniteThink );
	spike->pev->nextthink = gpGlobals->time;
}

void CSpike::Touch( CBaseEntity *other )
{
	SetTouch( NULL );
	SetThink( NULL );

	if ( other->pev->takedamage != DAMAGE_NO )
	{
		TraceResult trace = UTIL_GetGlobalTrace();

		other->TakeDamage( pev, pev->owner ? VARS( pev->owner ) : NULL, sk_pitdrone_dmg_spit.value, DMG_GENERIC );
		pev->velocity = g_vecZero;

		switch ( RANDOM_LONG( 0, 1 ) )
		{
			case 0:
				EMIT_SOUND( edict(), CHAN_BODY, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM );
				break;
			case 1:
				EMIT_SOUND( edict(), CHAN_BODY, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM );
				break;
		}

		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time;
		return;
	}

	int pitch = 130 + RANDOM_LONG( 0, 7 );
	float volume = RANDOM_FLOAT( 0.95, 1 );

	EMIT_SOUND_DYN( edict(), CHAN_BODY, "weapons/xbow_hit1.wav", volume, ATTN_NORM, 0, pitch );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time;

	if ( FClassnameIs( other->pev, "worldspawn" ) )
	{
		Vector direction = pev->velocity.Normalize();

		UTIL_SetOrigin( pev, pev->origin - direction * 5 );
		pev->angles = UTIL_VecToAngles( direction );
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_FLY;
		pev->velocity = g_vecZero;
		pev->avelocity.z = 0;
		pev->angles.z = RANDOM_LONG( 0, 360 );
		pev->nextthink = gpGlobals->time + 10.0;
	}
}

CPitdrone::CPitdrone()
{
	m_flAmmo = 6;
}

void CPitdrone::Spawn( void )
{
	Precache();

	if ( !SetupModel() )
		UTIL_SetModel( this, "models/pit_drone.mdl" );

	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 48 ) );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_GREEN;
	pev->effects = 0;

	if ( pev->health == 0 )
		pev->health = sk_pitdrone_health.value;

	m_flFieldOfView = -0.7f;
	m_MonsterState = MONSTERSTATE_NONE;
	m_afCapability |= bits_CAP_SQUAD;
	m_flNextSpitTime = gpGlobals->time;
	UpdateSpikeBodygroup();
	m_flNextSpitCheck = gpGlobals->time + 5;

	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( "Pit Drone" );

	MonsterInit();

	if ( m_fPlayerAlly && !( pev->spawnflags & SF_MONSTER_PREDISASTER ) )
	{
		SetUse( &CBaseMonster::CallFollowerPlayerUse );
		m_fCanPlayerFollow = true;
	}
}

void CPitdrone::KeyValue( KeyValueData *data )
{
	if ( FStrEq( data->szKeyName, "initammo" ) )
	{
		m_flAmmo = atof( data->szValue );
		data->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue( data );
}

void CPitdrone::Precache( void )
{
	CBaseEntity::Precache();

	if ( !pev->model )
		UTIL_PrecacheModel( this, "models/pit_drone.mdl" );

	UTIL_PrecacheModel( this, "models/pit_drone_gibs.mdl" );
	UTIL_PrecacheModel( this, "models/pit_drone_spike.mdl" );
	iPitdroneSpikeModel = UTIL_PrecacheModel( this, "sprites/pitdronespit.spr" );
	PrecacheSound_Intercept( this, "zombie/claw_miss2.wav" );
	PrecacheSound_Intercept( this, "pitdrone/pit_drone_attack_spike1.wav" );

	for ( unsigned int i = 0; i < ARRAYSIZE( pPitdroneAlertSounds ); i++ )
		PrecacheSound_Intercept( this, pPitdroneAlertSounds[i] );

	for ( unsigned int i = 0; i < ARRAYSIZE( pPitdroneIdleSounds ); i++ )
		PrecacheSound_Intercept( this, pPitdroneIdleSounds[i] );

	PrecacheSound_Intercept( this, "pitdrone/pit_drone_die1.wav" );
	PrecacheSound_Intercept( this, "pitdrone/pit_drone_die2.wav" );
	PrecacheSound_Intercept( this, "pitdrone/pit_drone_die3.wav" );
	PrecacheSound_Intercept( this, "bullchicken/bc_bite2.wav" );

	for ( unsigned int i = 0; i < ARRAYSIZE( pPitdronePainSounds ); i++ )
		PrecacheSound_Intercept( this, pPitdronePainSounds[i] );

	PrecacheSound_Intercept( this, "pitdrone/pit_drone_melee_attack1.wav" );
	PrecacheSound_Intercept( this, "pitdrone/pit_drone_melee_attack2.wav" );
	PrecacheSound_Intercept( this, "weapons/xbow_hit1.wav" );
	PrecacheSound_Intercept( this, "pitdrone/pit_drone_eat.wav" );
	iSpikeTrail = UTIL_PrecacheModel( this, "sprites/spike_trail.spr" );
}

int CPitdrone::ObjectCaps( void )
{
	return m_fPlayerAlly ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

int CPitdrone::Classify( void )
{
	return GetClassification( CLASS_XRACE_PITDRONE );
}

int CPitdrone::ISoundMask( void )
{
	return bits_SOUND_WORLD | bits_SOUND_COMBAT | bits_SOUND_CARCASS | bits_SOUND_MEAT | bits_SOUND_GARBAGE | bits_SOUND_PLAYER;
}

void CPitdrone::SetYawSpeed( void )
{
	pev->yaw_speed = 270;
}

void CPitdrone::UpdateSpikeBodygroup( void )
{
	int ammo = (int) m_flAmmo;

	if ( ammo < 0 )
		ammo = 0;
	else if ( ammo > 6 )
		ammo = 6;

	SetBodygroup( 1, ammo > 0 ? 7 - ammo : 0 );
}

void CPitdrone::IdleSound( void )
{
	int pitch = RANDOM_LONG( 85, 120 );
	const char *sample = pPitdroneIdleSounds[RANDOM_LONG( 0, ARRAYSIZE( pPitdroneIdleSounds ) - 1 )];

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, sample, 1, ATTN_NORM, 0, pitch );
}

void CPitdrone::PainSound( void )
{
	int pitch = RANDOM_LONG( 85, 120 );
	const char *sample = pPitdronePainSounds[RANDOM_LONG( 0, ARRAYSIZE( pPitdronePainSounds ) - 1 )];

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, sample, 1, ATTN_NORM, 0, pitch );
}

void CPitdrone::DeathSound( void )
{
}

void CPitdrone::AlertSound( void )
{
	int pitch = RANDOM_LONG( 140, 160 );
	const char *sample = pPitdroneAlertSounds[RANDOM_LONG( 0, ARRAYSIZE( pPitdroneAlertSounds ) - 1 )];

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, sample, 1, ATTN_NORM, 0, pitch );
}

void CPitdrone::UseSound( void )
{
	AlertSound();
}

void CPitdrone::UnUseSound( void )
{
	PainSound();
}

BOOL CPitdrone::CheckMeleeAttack1( float dot, float distance )
{
	return RANDOM_LONG( 0, 6 ) >= 4 && distance <= 55 && dot >= 0.7;
}

BOOL CPitdrone::CheckMeleeAttack2( float dot, float distance )
{
	return distance <= 55 && dot >= 0.7 && !HasConditions( bits_COND_CAN_MELEE_ATTACK2 );
}

BOOL CPitdrone::CheckRangeAttack1( float dot, float distance )
{
	if ( m_flAmmo <= 0 )
	{
		SetBodygroup( 1, 0 );
		SetConditions( bits_COND_NO_AMMO_LOADED );
		return FALSE;
	}

	if ( m_flNextSpitCheck > gpGlobals->time
		|| ( IsMoving() && distance >= 512 )
		|| !( distance > 64 && distance <= 784 && dot >= 0.5f && gpGlobals->time >= m_flNextSpitTime )
		|| ( m_hEnemy != NULL && fabs( pev->origin.z - m_hEnemy->pev->origin.z ) > 256 ) )
		return FALSE;

	m_flNextSpitTime = gpGlobals->time + ( IsMoving() ? 5.0f : 0.5f );
	return TRUE;
}

BOOL CPitdrone::FValidateHintType( short hint )
{
	if ( hint == HINT_WORLD_HUMAN_BLOOD )
		return TRUE;

	ALERT( at_aiconsole, "Couldn't validate hint type" );
	return FALSE;
}

void CPitdrone::RunAI( void )
{
	CSquadMonster::RunAI();

	if ( m_hEnemy != NULL && m_Activity == ACT_RUN
		&& ( pev->origin - m_hEnemy->pev->origin ).Length2D() < 600 )
		pev->framerate = 1.3f;

	if ( HasConditions( bits_COND_NEW_ENEMY ) )
		m_flNextSpitCheck = gpGlobals->time + 4;

	if ( m_flAmmo <= 0 )
	{
		SetBodygroup( 1, 0 );
		SetConditions( bits_COND_NO_AMMO_LOADED );
	}
}

void CPitdrone::SetActivity( Activity activity )
{
	if ( activity == ACT_EAT )
	{
		float health = pev->health + 100;

		pev->health = health > pev->max_health ? pev->max_health : health;
	}

	CBaseMonster::SetActivity( activity );
}

int CPitdrone::IRelationship( CBaseEntity *target, bool ignoreNoTarget )
{
	if ( !target )
		return R_NO;

	if ( ( target->pev->flags & FL_MONSTER ) && target->m_fOverrideClass )
		return CBaseEntity::IRelationship( target, ignoreNoTarget );

	if ( !m_fPlayerAlly && FClassnameIs( target->pev, "monster_shockroach" ) )
		return R_DL;

	if ( gpGlobals->time - m_flLastHurtTime < 5 && FClassnameIs( target->pev, "monster_headcrab" ) )
		return R_NO;

	return CBaseEntity::IRelationship( target, false );
}

int CPitdrone::IgnoreConditions( void )
{
	int ignored = CBaseMonster::IgnoreConditions();

	if ( gpGlobals->time - m_flLastHurtTime <= 20 )
		ignored = bits_COND_SMELL | bits_COND_SMELL_FOOD;

	if ( m_hEnemy != NULL
		&& ( FClassnameIs( m_hEnemy->pev, "monster_headcrab" ) || FClassnameIs( m_hEnemy->pev, "monster_shockroach" ) ) )
		ignored = bits_COND_SMELL | bits_COND_SMELL_FOOD;

	return ignored;
}

MONSTERSTATE CPitdrone::GetIdealState( void )
{
	int conditions = IScheduleFlags();

	if ( m_MonsterState == MONSTERSTATE_COMBAT && m_hEnemy != NULL
		&& ( conditions & ( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
		&& FClassnameIs( m_hEnemy->pev, "monster_headcrab" ) )
	{
		m_hEnemy = NULL;
		m_IdealMonsterState = MONSTERSTATE_ALERT;
	}

	m_IdealMonsterState = CSquadMonster::GetIdealState();
	return m_IdealMonsterState;
}

void CPitdrone::StartTask( Task_t *task )
{
	if ( task->iTask == TASK_PITDRONE_HOPTURN )
	{
		SetActivity( ACT_HOP );
		MakeIdealYaw( m_vecEnemyLKP );
	}
	else
		CBaseMonster::StartTask( task );
}

void CPitdrone::RunTask( Task_t *task )
{
	if ( task->iTask != TASK_PITDRONE_HOPTURN )
	{
		CBaseMonster::RunTask( task );
		return;
	}

	MakeIdealYaw( m_vecEnemyLKP );
	ChangeYaw( pev->yaw_speed );

	if ( m_fSequenceFinished )
		m_iTaskStatus = TASKSTATUS_COMPLETE;
}

void CPitdrone::TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType )
{
	switch ( trace->iHitgroup )
	{
		case HITGROUP_GENERIC:
		case HITGROUP_CHEST:
		case HITGROUP_STOMACH:
		case HITGROUP_LEFTARM:
		case HITGROUP_RIGHTARM:
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
			if ( damageType & ( DMG_BULLET | DMG_SLASH | DMG_BLAST ) )
				damage *= 0.45;
			break;

		case HITGROUP_HEAD:
		case 10:
			damage *= 0.5f;
			break;
	}

	CBaseMonster::TraceAttack( attacker, damage, direction, trace, damageType );
}

int CPitdrone::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	if ( m_hEnemy != NULL && IsMoving() && attacker == m_hEnemy->pev
		&& gpGlobals->time - m_flLastHurtTime > 3
		&& ( pev->origin - m_hEnemy->pev->origin ).Length2D() > 400 )
	{
		Vector apex( 0, 0, 0 ), secondApex( 0, 0, 0 );
		float distance = ( pev->origin - m_Route[m_iRouteIndex].vecLocation ).Length2D();

		if ( FTriangulate( pev->origin, m_Route[m_iRouteIndex].vecLocation, distance * 0.5f, m_hEnemy, &apex, &secondApex, true ) )
			InsertWaypoint( apex, bits_MF_TO_DETOUR | bits_MF_DONT_SIMPLIFY );
	}

	if ( damageType & ( DMG_BULLET | DMG_SLASH | DMG_CLUB | DMG_SNIPER ) )
		damage *= 0.45;

	if ( !FClassnameIs( attacker, "monster_headcrab" ) )
		m_flLastHurtTime = gpGlobals->time;

	if ( IsFriendly( CBaseEntity::Instance( inflictor ) ) )
		damage = 0;

	return CBaseMonster::TakeDamage( inflictor, attacker, damage, damageType );
}

float CPitdrone::GetPointsForDamage( float damage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	float points = pev->max_health / sk_pitdrone_health.value;

	points *= 5.0f;

	return points * ( damage / pev->max_health );
}

void CPitdrone::SpawnRandomGibs( entvars_t *victim, int count, int human )
{
	for ( int i = 0; i < count; i++ )
	{
		CGib *gib = GetClassPtr( (CGib *) NULL );

		gib->Spawn( "models/pit_drone_gibs.mdl" );
		gib->pev->body = RANDOM_LONG( 0, count - 1 );

		if ( victim )
		{
			gib->pev->origin.x = victim->absmin.x + victim->size.x * RANDOM_FLOAT( 0, 1 );
			gib->pev->origin.y = victim->absmin.y + victim->size.y * RANDOM_FLOAT( 0, 1 );
			gib->pev->origin.z = victim->absmin.z + victim->size.z * RANDOM_FLOAT( 0, 1 ) + 1;
			gib->pev->velocity = -g_vecAttackDir;
			gib->pev->velocity.x += RANDOM_FLOAT( -0.25, 0.25 );
			gib->pev->velocity.y += RANDOM_FLOAT( -0.25, 0.25 );
			gib->pev->velocity.z += RANDOM_FLOAT( -0.25, 0.25 );
			gib->pev->velocity = gib->pev->velocity * RANDOM_FLOAT( 300, 400 );
			gib->pev->avelocity.x = RANDOM_FLOAT( 100, 200 );
			gib->pev->avelocity.y = RANDOM_FLOAT( 100, 300 );
			gib->m_bloodColor = CBaseEntity::Instance( victim )->BloodColor();

			if ( victim->health > -50 )
				gib->pev->velocity = gib->pev->velocity * 0.7f;
			else if ( victim->health <= -200 )
				gib->pev->velocity = gib->pev->velocity * 4;
			else
				gib->pev->velocity = gib->pev->velocity + gib->pev->velocity;

			gib->pev->solid = SOLID_BBOX;
			UTIL_SetSize( gib->pev, g_vecZero, g_vecZero );
		}

		gib->LimitVelocity();
	}
}

void CPitdrone::GibMonster( void )
{
	EMIT_SOUND( edict(), CHAN_WEAPON, "common/bodysplat.wav", 1, ATTN_NORM );
	SpawnRandomGibs( pev, 6, 1 );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time;
}

Task_t tlPitdroneRangeAttack1[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, SCHED_CHASE_ENEMY },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_SET_ACTIVITY, ACT_IDLE }
};

Schedule_t slPitdroneRangeAttack1[] = {
	{ tlPitdroneRangeAttack1, ARRAYSIZE( tlPitdroneRangeAttack1 ),
	  bits_COND_NO_AMMO_LOADED | bits_COND_ENEMY_OCCLUDED | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY | bits_COND_ENEMY_DEAD,
	  0, "Squid Range Attack1" }
};

Task_t tlPitdroneChaseEnemy[] = {
	{ TASK_SET_FAIL_SCHEDULE, SCHED_RANGE_ATTACK1 },
	{ TASK_GET_PATH_TO_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 }
};

Schedule_t slPitdroneChaseEnemy[] = {
	{ tlPitdroneChaseEnemy, ARRAYSIZE( tlPitdroneChaseEnemy ),
	  bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK2 | bits_COND_NEW_ENEMY | bits_COND_HEAR_SOUND | bits_COND_ENEMY_DEAD,
	  bits_SOUND_DANGER, "Squid Chase Enemy" }
};

Task_t tlPitdroneHurtHop[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SOUND_WAKE, 0 },
	{ TASK_PITDRONE_HOPTURN, 0 },
	{ TASK_FACE_ENEMY, 0 }
};

Schedule_t slPitdroneHurtHop[] = {
	{
		tlPitdroneHurtHop,
		ARRAYSIZE( tlPitdroneHurtHop ),
		0,
		0,
		"SquidHurtHop"
	}
};

Task_t tlPitdroneSeeCrab[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SOUND_WAKE, 0 },
	{ TASK_FACE_ENEMY, 0 }
};

Schedule_t slPitdroneSeeCrab[] = {
	{ tlPitdroneSeeCrab, ARRAYSIZE( tlPitdroneSeeCrab ),
	  bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE, 0, "SquidSeeCrab" }
};

Task_t tlPitdroneEat[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_EAT, 10 },
	{ TASK_STORE_LASTPOSITION, 0 },
	{ TASK_GET_PATH_TO_BESTSCENT, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_PLAY_SEQUENCE, ACT_EAT },
	{ TASK_PLAY_SEQUENCE, ACT_EAT },
	{ TASK_PLAY_SEQUENCE, ACT_EAT },
	{ TASK_EAT, 50 },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_CLEAR_LASTPOSITION, 0 }
};

Schedule_t slPitdroneEat[] = {
	{ tlPitdroneEat, ARRAYSIZE( tlPitdroneEat ),
	  bits_COND_SEE_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY,
	  bits_SOUND_CARCASS | bits_SOUND_MEAT, "PitDrone Eat" }
};

Task_t tlPitdroneEat2[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_STORE_LASTPOSITION, 0 },
	{ TASK_GET_PATH_TO_ENEMY_CORPSE, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_PLAY_SEQUENCE, ACT_EAT },
	{ TASK_PLAY_SEQUENCE, ACT_EAT },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_CLEAR_LASTPOSITION, 0 },
	{ TASK_TURN_LEFT, 179 }
};

Schedule_t slPitdroneEat2[] = {
	{ tlPitdroneEat2, ARRAYSIZE( tlPitdroneEat2 ),
	  bits_COND_SEE_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY | bits_COND_TASK_FAILED,
	  0, "PitDrone Eat2" }
};

Task_t tlPitdroneSniffAndEat[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_EAT, 10 },
	{ TASK_STORE_LASTPOSITION, 0 },
	{ TASK_GET_PATH_TO_BESTSCENT, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_PLAY_SEQUENCE, ACT_EAT },
	{ TASK_PLAY_SEQUENCE, ACT_EAT },
	{ TASK_PLAY_SEQUENCE, ACT_EAT },
	{ TASK_EAT, 50 },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_CLEAR_LASTPOSITION, 0 }
};

Schedule_t slPitdroneSniffAndEat[] = {
	{ tlPitdroneSniffAndEat, ARRAYSIZE( tlPitdroneSniffAndEat ),
	  bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY,
	  bits_SOUND_CARCASS | bits_SOUND_MEAT, "SquidSniffAndEat" }
};

Task_t tlPitdroneWallow[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_EAT, 10 },
	{ TASK_STORE_LASTPOSITION, 0 },
	{ TASK_GET_PATH_TO_BESTSCENT, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_EAT, 50 },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_CLEAR_LASTPOSITION, 0 }
};

Schedule_t slPitdroneWallow[] = {
	{ tlPitdroneWallow, ARRAYSIZE( tlPitdroneWallow ),
	  bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY,
	  bits_SOUND_GARBAGE, "SquidWallow" }
};

Task_t tlPitdroneReload[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, SCHED_RELOAD },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, ACT_RELOAD }
};

Schedule_t slPitdroneReload[] = {
	{ tlPitdroneReload, ARRAYSIZE( tlPitdroneReload ),
	  bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND, bits_SOUND_DANGER, "PitdroneHideReload" }
};

DEFINE_CUSTOM_SCHEDULES( CPitdrone ){
	slPitdroneRangeAttack1,
	slPitdroneChaseEnemy,
	slPitdroneHurtHop,
	slPitdroneSeeCrab,
	slPitdroneEat,
	slPitdroneEat2,
	slPitdroneSniffAndEat,
	slPitdroneWallow,
	slPitdroneReload
};

IMPLEMENT_CUSTOM_SCHEDULES( CPitdrone, CBaseMonster );

Schedule_t *CPitdrone::GetScheduleOfType( int type )
{
	switch ( type )
	{
		case SCHED_CHASE_ENEMY:
			return slPitdroneChaseEnemy;
		case SCHED_RANGE_ATTACK1:
			return slPitdroneRangeAttack1;
		case SCHED_PITDRONE_HURTHOP:
			return slPitdroneHurtHop;
		case SCHED_PITDRONE_SEECRAB:
			return slPitdroneSeeCrab;
		case SCHED_PITDRONE_EAT:
			return slPitdroneEat;
		case SCHED_PITDRONE_EAT2:
			return slPitdroneEat2;
		case SCHED_PITDRONE_SNIFF_AND_EAT:
			return slPitdroneSniffAndEat;
		case SCHED_PITDRONE_WALLOW:
			return slPitdroneWallow;
		case SCHED_PITDRONE_RELOAD:
			return slPitdroneReload;
	}

	return CBaseMonster::GetScheduleOfType( type );
}

Schedule_t *CPitdrone::GetSchedule( void )
{
	if ( m_MonsterState == MONSTERSTATE_COMBAT )
	{
		if ( HasConditions( bits_COND_ENEMY_DEAD ) )
			return CBaseMonster::GetSchedule();

		if ( HasConditions( bits_COND_NEW_ENEMY ) )
		{
			AlertSound();
			return GetScheduleOfType( SCHED_WAKE_ANGRY );
		}

		if ( HasConditions( bits_COND_NO_AMMO_LOADED ) )
			return GetScheduleOfType( SCHED_PITDRONE_RELOAD );

		if ( !HasConditions( bits_COND_SMELL_FOOD ) )
		{
			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

			if ( HasConditions( bits_COND_CAN_MELEE_ATTACK2 ) )
				return GetScheduleOfType( SCHED_MELEE_ATTACK2 );

			return GetScheduleOfType( SCHED_CHASE_ENEMY );
		}
	}
	else if ( m_MonsterState == MONSTERSTATE_ALERT )
	{
		if ( HasConditions( bits_COND_ENEMY_DEAD ) )
			return GetScheduleOfType( SCHED_PITDRONE_EAT2 );

		if ( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
			return GetScheduleOfType( SCHED_PITDRONE_HURTHOP );

		if ( !HasConditions( bits_COND_SMELL_FOOD ) )
		{
			if ( HasConditions( bits_COND_SMELL ) && PBestScent() )
				return GetScheduleOfType( SCHED_PITDRONE_WALLOW );

			return CBaseMonster::GetSchedule();
		}
	}
	else
		return CBaseMonster::GetSchedule();

	CSound *scent = PBestScent();

	if ( !scent || ( FInViewCone( &scent->m_vecOrigin ) && FVisible( scent->m_vecOrigin ) ) )
		return GetScheduleOfType( SCHED_PITDRONE_EAT );

	return GetScheduleOfType( SCHED_PITDRONE_SNIFF_AND_EAT );
}

void CPitdrone::HandleAnimEvent( MonsterEvent_t *event )
{
	switch ( event->event )
	{
		case 1:
		{
			if ( HasConditions( bits_COND_NO_AMMO_LOADED ) )
				GetScheduleOfType( SCHED_PITDRONE_RELOAD );

			if ( !NoFriendlyFire() )
			{
				TaskComplete();
				break;
			}

			UTIL_MakeVectors( pev->angles );

			Vector start = pev->origin + ( gpGlobals->v_right * 10 + gpGlobals->v_up + gpGlobals->v_forward * 33 ) * pev->scale;
			Vector direction = start;

			if ( m_hEnemy != NULL )
			{
				direction = m_hEnemy->pev->origin + m_hEnemy->pev->view_ofs / 1.5f - start;

				if ( m_hEnemy->IsBSPModel() )
					direction = m_hEnemy->Center() - start;
			}

			direction = direction.Normalize();
			direction.x += RANDOM_FLOAT( -0.02, 0.02 );
			direction.y += RANDOM_FLOAT( -0.02, 0.02 );
			direction.z += RANDOM_FLOAT( -0.02, 0.02 );

			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, start );
			WRITE_BYTE( TE_SPRITE_SPRAY );
			WRITE_COORD( start.x );
			WRITE_COORD( start.y );
			WRITE_COORD( start.z );
			WRITE_COORD( direction.x );
			WRITE_COORD( direction.y );
			WRITE_COORD( direction.z );
			WRITE_SHORT( iPitdroneSpikeModel );
			WRITE_BYTE( 10 );
			WRITE_BYTE( 150 );
			WRITE_BYTE( 25 );
			MESSAGE_END();

			float speed = fminf( 2000.0f, pev->scale * 900.0f );

			CSpike::Shoot( pev, start, direction * speed );
			m_flAmmo -= 1;
			UpdateSpikeBodygroup();
			break;
		}

		case 2:
		{
			CBaseEntity *hurt = CheckTraceHullAttack( 50, (int) sk_pitdrone_dmg_bite.value, DMG_SLASH | DMG_ALWAYSGIB );

			if ( hurt )
			{
				int pitch = 90 + RANDOM_LONG( 0, 15 );

				EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "bullchicken/bc_bite2.wav", 1, ATTN_NORM, 0, pitch );

				if ( ( hurt->pev->flags & FL_CLIENT )
					|| ( ( hurt->pev->flags & FL_MONSTER ) && hurt->pev->movetype != MOVETYPE_FLY ) )
				{
					hurt->pev->punchangle.z = -25;
					hurt->pev->punchangle.x = -25;
					hurt->pev->velocity = hurt->pev->velocity + gpGlobals->v_forward * 50;
				}
			}
			else
			{
				PainSound();
				TaskComplete();
			}
			break;
		}

		case 4:
		{
			int damageType = RANDOM_LONG( 0, 10 ) <= 5 ? DMG_SLASH | DMG_ALWAYSGIB : DMG_SLASH;
			CBaseEntity *hurt = CheckTraceHullAttack( 50, (int) sk_pitdrone_dmg_whip.value, damageType );

			if ( hurt )
			{
				if ( ( hurt->pev->flags & FL_CLIENT )
					|| ( ( hurt->pev->flags & FL_MONSTER ) && hurt->pev->movetype != MOVETYPE_FLY ) )
				{
					hurt->pev->punchangle.z = -20;
					hurt->pev->punchangle.x = 20;
					hurt->pev->velocity = hurt->pev->velocity - gpGlobals->v_right * 100;
				}
			}
			else
			{
				PainSound();
				TaskComplete();
			}
			break;
		}

		case 5:
			if ( pev->flags & FL_ONGROUND )
				pev->flags -= FL_ONGROUND;
			break;

		case 6:
			break;

		case 7:
			m_flAmmo = 6;
			UpdateSpikeBodygroup();
			ClearConditions( bits_COND_NO_AMMO_LOADED );
			break;

		default:
			CBaseMonster::HandleAnimEvent( event );
			break;
	}
}
