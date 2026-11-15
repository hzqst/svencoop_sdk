#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "weapons.h"
#include "animation.h"
#include "decals.h"
#include "agrunt.h"
#include "squeakgrenade.h"
#include "sqknest.h"

extern cvar_t sk_sqknest_health;

enum
{
	TASK_SQKNEST_FIND_WALK = LAST_COMMON_TASK + 1,
	TASK_SQKNEST_BLIND_WALK
};

static Task_t tlSqkNestRangeAttack1[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_RANGE_ATTACK1_NOTURN, 0 }
};
static Schedule_t slSqkNestRangeAttack1[] = {
	{ tlSqkNestRangeAttack1, ARRAYSIZE( tlSqkNestRangeAttack1 ), bits_COND_ENEMY_OCCLUDED, 0, "SqkNestRangeAttack1" }
};
static Task_t tlSqkNestRandomWalk[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SQKNEST_FIND_WALK, 0 },
	{ TASK_SQKNEST_BLIND_WALK, 0 }
};
static Schedule_t slSqkNestRandomWalk[] = {
	{ tlSqkNestRandomWalk, ARRAYSIZE( tlSqkNestRandomWalk ), 0x73F44, 0x7F, "Wander around" }
};

DEFINE_CUSTOM_SCHEDULES( CSqkNest ){
	slSqkNestRangeAttack1,
	slSqkNestRandomWalk
};
IMPLEMENT_CUSTOM_SCHEDULES( CSqkNest, CBaseMonster );
LINK_ENTITY_TO_CLASS( monster_sqknest, CSqkNest );

const char *CSqkNest::pStepSounds[] = {
	"debris/flesh1.wav",
	"debris/flesh2.wav",
	"debris/flesh3.wav",
	"debris/flesh5.wav",
	"debris/flesh6.wav",
	"debris/flesh7.wav"
};

void CSqkNest::Spawn( void )
{
	Precache();

	if ( IRelationshipByClass( CLASS_PLAYER ) > R_NO )
		m_fPlayerAlly = false;

	if ( !m_fCustomModel )
		UTIL_SetModel( this, "models/w_sqknest.mdl" );

	UTIL_SetSize( pev, Vector( -12, -12, 0 ), Vector( 12, 12, 16 ) );
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_BOUNCE;
	pev->friction = 0.8;
	m_bloodColor = BLOOD_COLOR_YELLOW;
	pev->effects = 0;

	if ( pev->health == 0 )
		pev->health = sk_sqknest_health.value;

	pev->view_ofs = Vector( 0, 0, 20 );
	pev->yaw_speed = 100;
	pev->framerate = 1;
	m_flFieldOfView = -1;
	m_MonsterState = MONSTERSTATE_NONE;
	NotifySnarkCount( 5 );
	m_iSnarkCount = 5;
	m_afCapability = bits_CAP_RANGE_ATTACK1;

	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( m_fPlayerAlly ? "Friendly Snark Nest" : "Snark Nest" );

	SetTouch( &CSqkNest::ThrowTouch );
	m_flWaitFinished = gpGlobals->time + 5;
	MonsterInit();

	if ( m_fPlayerAlly && !( pev->spawnflags & SF_MONSTER_PREDISASTER ) )
	{
		SetUse( &CBaseMonster::CallFollowerPlayerUse );
		m_fCanPlayerFollow = true;
	}
}

void CSqkNest::Precache( void )
{
	CBaseEntity::Precache();
	UTIL_PrecacheModel( this, "models/w_sqknest.mdl" );
	m_iGibModelIndex = UTIL_PrecacheModel( this, "models/fleshgibs.mdl" );

	for ( int i = 0; i < ARRAYSIZE( pStepSounds ); i++ )
		PrecacheSound_Intercept( this, pStepSounds[i] );

	PrecacheSound_Intercept( this, "debris/bustflesh1.wav" );
	PrecacheSound_Intercept( this, "debris/bustflesh2.wav" );
	UTIL_PrecacheOther( "monster_snark" );
}

int CSqkNest::Classify( void )
{
	return GetClassification( CLASS_ALIEN_BIOWEAPON );
}

int CSqkNest::ObjectCaps( void )
{
	return m_fPlayerAlly ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

float CSqkNest::GetPointsForDamage( float damage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	return ( damage / pev->max_health ) * ( pev->max_health / sk_sqknest_health.value );
}

BOOL CSqkNest::CheckRangeAttack1( float dot, float distance )
{
	return distance <= 128;
}

void CSqkNest::SetObjectCollisionBox( void )
{
	if ( ( pev->spawnflags & SF_MONSTER_NO_MODEL_BOX ) || !m_fCustomModel )
	{
		CBaseEntity::SetObjectCollisionBox();
		return;
	}

	Vector mins = g_vecZero;
	Vector maxs = g_vecZero;

	ExtractBbox( 1, mins, maxs );
	mins.z = 0;
	pev->absmin = pev->origin + mins;
	pev->absmax = pev->origin + maxs;
}

void CSqkNest::HandleAnimEvent( MonsterEvent_t *event )
{
	if ( event->event != 1 )
	{
		CBaseMonster::HandleAnimEvent( event );
		return;
	}

	int pitch = 90 + RANDOM_LONG( 0, 20 );
	float volume = RANDOM_FLOAT( 0.5, 0.75 );
	const char *sound = pStepSounds[RANDOM_LONG( 0, ARRAYSIZE( pStepSounds ) - 1 )];

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, sound, volume, ATTN_NORM, 0, pitch );
}

void CSqkNest::NotifySnarkCount( int change )
{
	CBaseEntity *owner = pev->owner ? CBaseEntity::Instance( pev->owner ) : NULL;
	CAGrunt *grunt = dynamic_cast<CAGrunt *>( owner );

	if ( grunt )
		grunt->m_iNumSnarks += change;
}

void CSqkNest::Killed( entvars_t *attacker, int gib )
{
	CBaseMonster::Killed( attacker, gib );

	if ( pev->deadflag == DEAD_DEAD )
		return;

	NotifySnarkCount( 1 - m_iSnarkCount );
	m_iSnarkCount = 1;
	Burst();
}

void CSqkNest::StartBurst( void )
{
	pev->renderfx = kRenderFxExplode;
	SetThink( &CSqkNest::Burst );
	pev->nextthink = gpGlobals->time + 0.05;
	pev->takedamage = DAMAGE_NO;
}

void CSqkNest::Burst( void )
{
	SetBits( pev->effects, EF_NODRAW );
	pev->takedamage = DAMAGE_NO;
	pev->solid = SOLID_NOT;

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
	WRITE_BYTE( TE_BREAKMODEL );
	WRITE_COORD( pev->origin.x );
	WRITE_COORD( pev->origin.y );
	WRITE_COORD( pev->origin.z );
	WRITE_COORD( 16 );
	WRITE_COORD( 8 );
	WRITE_COORD( 16 );
	WRITE_COORD( 50 );
	WRITE_COORD( 50 );
	WRITE_COORD( 50 );
	WRITE_BYTE( 10 );
	WRITE_SHORT( m_iGibModelIndex );
	WRITE_BYTE( 20 );
	WRITE_BYTE( 50 );
	WRITE_BYTE( 0 );
	MESSAGE_END();

	int pitch = 90 + RANDOM_LONG( 0, 20 );
	int sound = RANDOM_LONG( 0, 1 );

	if ( sound == 0 )
		EMIT_SOUND_DYN( edict(), CHAN_VOICE, "debris/bustflesh1.wav", 1, ATTN_NORM, 0, pitch );
	else if ( sound == 1 )
		EMIT_SOUND_DYN( edict(), CHAN_VOICE, "debris/bustflesh2.wav", 1, ATTN_NORM, 0, pitch );

	TraceResult trace;

	trace.vecEndPos = g_vecZero;
	trace.vecPlaneNormal = g_vecZero;
	UTIL_TraceLine( pev->origin, pev->origin - Vector( 0, 0, 100 ), ignore_monsters, NULL, &trace );
	UTIL_DecalTrace( &trace, DECAL_MOMMABIRTH );

	if ( !pev->owner || FClassnameIs( pev->owner, "monstermaker" ) ||
	     FClassnameIs( pev->owner, "squadmaker" ) || FClassnameIs( pev->owner, "env_xenmaker" ) )
		m_hOwner = NULL;
	else
		m_hOwner = CBaseEntity::Instance( pev->owner );

	if ( m_hEnemy != NULL )
		m_vecSpawnAngles = UTIL_VecToAngles( m_hEnemy->pev->origin - pev->origin );
	else if ( g_vecAttackDir != g_vecZero )
		m_vecSpawnAngles = UTIL_VecToAngles( g_vecAttackDir );
	else
		m_vecSpawnAngles = g_vecZero;

	SetThink( &CSqkNest::SpawnSnark );
	m_flAutomaticAttackTime = gpGlobals->time;
	pev->nextthink = gpGlobals->time;
}

void CSqkNest::SpawnSnark( void )
{
	if ( m_iSnarkCount < 1 || gpGlobals->time - m_flAutomaticAttackTime > 1 )
	{
		SUB_Remove();
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;

	CBaseEntity *entities[64];
	int count = UTIL_EntitiesInBox( entities, ARRAYSIZE( entities ),
	                                pev->origin + Vector( -5, -5, 0 ), pev->origin + Vector( 5, 5, 10 ), FL_MONSTER );

	for ( int i = 0; i < count; i++ )
	{
		if ( entities[i]->pev != pev && entities[i]->pev->solid > SOLID_TRIGGER )
			return;
	}

	Vector angles = m_vecSpawnAngles;

	if ( angles == g_vecZero )
	{
		float z = RANDOM_FLOAT( -1, 1 );
		float y = RANDOM_FLOAT( -1, 1 );
		float x = RANDOM_FLOAT( -1, 1 );

		angles = UTIL_VecToAngles( Vector( x, y, z ) );
	}

	edict_t *owner = m_hOwner != NULL ? m_hOwner->edict() : NULL;

	if ( IRelationshipByClass( CLASS_PLAYER ) > R_NO )
		m_fPlayerAlly = false;

	CBaseEntity *snark = CBaseEntity::Create( "monster_snark", pev->origin + Vector( 0, 0, 1 ), angles, true, owner );

	snark->m_fPlayerAlly = m_fPlayerAlly;
	snark->m_fOverrideClass = m_fOverrideClass;
	snark->m_iClassSelection = m_iClassSelection;

	if ( pev->spawnflags & SF_MONSTER_HITMONSTERCLIP )
		SetBits( snark->pev->spawnflags, SF_MONSTER_HITMONSTERCLIP );

	float z = RANDOM_FLOAT( 100, 200 );
	float y = RANDOM_FLOAT( -100, 100 );
	float x = RANDOM_FLOAT( -100, 100 );

	snark->pev->velocity = Vector( x, y, z );
	snark->Spawn();
	( (CSqueakGrenade *) snark )->m_bNestAlly = m_fPlayerAlly;
	m_iSnarkCount--;
}

void CSqkNest::ThrownWakeUp( void )
{
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	SetTouch( NULL );
	MonsterInit();
}

void CSqkNest::ThrowTouch( CBaseEntity *other )
{
	pev->velocity = pev->velocity * 0.75f;

	if ( ( pev->groundentity || FBitSet( pev->flags, FL_ONGROUND ) ) && pev->velocity.Length() < 40 )
		ThrownWakeUp();
}

void CSqkNest::MonsterThink( void )
{
	if ( pev->solid == SOLID_SLIDEBOX )
	{
		CBaseMonster::MonsterThink();
		return;
	}

	if ( ( pev->groundentity || FBitSet( pev->flags, FL_ONGROUND ) ) && pev->velocity.Length() < 40 )
	{
		ThrownWakeUp();
		return;
	}

	if ( pev->velocity.Length() >= 1 )
	{
		m_flWaitFinished = gpGlobals->time + 5;
		pev->nextthink = gpGlobals->time + 1;
	}
	else
	{
		pev->avelocity = g_vecZero;

		if ( gpGlobals->time > m_flWaitFinished )
			ThrownWakeUp();
	}
}

void CSqkNest::BlindWalkBlocked( CBaseEntity *other )
{
	TaskFail();
}

void CSqkNest::StartTask( Task_t *task )
{
	SetBlocked( NULL );

	switch ( task->iTask )
	{
		case TASK_RANGE_ATTACK1_NOTURN:
			StartBurst();
			break;
		case TASK_SQKNEST_FIND_WALK:
		{
			RouteClear();
			UTIL_MakeVectors( pev->angles );

			Vector forward = gpGlobals->v_forward;
			Vector right = gpGlobals->v_right;
			Vector start = GetGunPosition();
			Vector goal = start + forward * 10000;
			Vector end = g_vecZero;
			float distance = -1;

			CheckLocalMove( start, goal, NULL, &distance, &end, false );

			if ( distance >= 0 && distance <= 200 )
			{
				TraceResult trace;

				trace.vecEndPos = g_vecZero;
				trace.vecPlaneNormal = g_vecZero;
				TRACE_MONSTER_HULL( edict(), start, start + forward * ( distance + 18 + 18 ),
				                    dont_ignore_monsters, edict(), &trace );

				if ( trace.flFraction == 1 )
					goal = start - forward * 10000;
				else
				{
					Vector normal = CrossProduct( CrossProduct( trace.vecPlaneNormal, right ), right );

					goal = trace.vecEndPos + ( forward - normal * ( DotProduct( forward, normal ) * 2 ) ) * 10000;
				}
			}

			m_Route[0].vecLocation = goal;
			m_Route[0].iType = bits_MF_TO_LOCATION;
			m_movementGoal = MOVEGOAL_LOCATION;
			m_movementActivity = ACT_RUN;
			TaskComplete();
			break;
		}
		case TASK_SQKNEST_BLIND_WALK:
			SetBlocked( &CSqkNest::BlindWalkBlocked );
			break;
		default:
			CBaseMonster::StartTask( task );
			break;
	}
}

void CSqkNest::RunTask( Task_t *task )
{
	if ( task->iTask == TASK_SQKNEST_BLIND_WALK )
		return;

	if ( task->iTask == TASK_WAIT_FOR_MOVEMENT_AP )
	{
		if ( m_hEnemy != NULL && !FVisible( m_hEnemy, true ) )
		{
			m_hEnemy = NULL;
			TaskFail();
		}

		if ( pev->framerate >= 5 )
			pev->framerate = 5;
		else
		{
			pev->framerate += 0.1f;

			void *model = GET_MODEL_PTR( edict() );

			if ( model )
				GetSequenceInfo( model, pev, &m_flFrameRate, &m_flGroundSpeed );
		}
	}

	CBaseMonster::RunTask( task );
}

Schedule_t *CSqkNest::GetScheduleOfType( int type )
{
	if ( type == SCHED_IDLE_STAND )
		return slSqkNestRandomWalk;

	if ( type == SCHED_RANGE_ATTACK1 )
		return slSqkNestRangeAttack1;

	return CBaseMonster::GetScheduleOfType( type );
}
