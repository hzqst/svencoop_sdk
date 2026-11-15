#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "nodes.h"
#include "soundent.h"
#include "stukabat.h"

extern cvar_t sk_stukabat;
extern cvar_t sk_stukabat_dmg_bite;

#define STUKABAT_AE_BITE	1
#define FLYING_AE_FLAP		8
#define FLYING_AE_FLAPSOUND	9

enum
{
	SCHED_STUKA_REST_PERCH = LAST_COMMON_SCHEDULE + 1,
	SCHED_STUKA_FLINCH_WAKE_ANGRY
};

enum
{
	TASK_STUKA_FIND_PERCH = LAST_COMMON_TASK + 1,
	TASK_STUKA_FIND_GROUND,
	TASK_STUKA_MOVE,
	TASK_STUKA_TAKE_OFF,
	TASK_STUKA_FALL
};

static Task_t tlStukaRestPerch[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_STUKA_FIND_PERCH, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_STAND },
	{ TASK_SET_ACTIVITY, ACT_IDLE }
};
static Schedule_t slStukaRestPerch[] = {
	{ tlStukaRestPerch, ARRAYSIZE( tlStukaRestPerch ),
	  bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY, 0, "StukaRestPerch" }
};
static Task_t tlStukaDie[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_STUKA_FALL, 0 },
	{ TASK_DIE, 0 }
};
static Schedule_t slStukaDie[] = {
	{
		tlStukaDie,
		ARRAYSIZE( tlStukaDie ),
		0,
		0,
		"StukaDeath"
	}
};
static Task_t tlStukaFlinchWakeAngry[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SMALL_FLINCH, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_HOVER },
	{ TASK_FACE_IDEAL, 0 }
};
static Schedule_t slStukaFlinchWakeAngry[] = {
	{
		tlStukaFlinchWakeAngry,
		ARRAYSIZE( tlStukaFlinchWakeAngry ),
		0,
		0,
		"StukaFlinchWakeAngry"
	}
};
static Task_t tlStukaChaseEnemy[] = {
	{ TASK_SET_FAIL_SCHEDULE, SCHED_TAKE_COVER_FROM_ENEMY },
	{ TASK_GET_PATH_TO_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 }
};
static Schedule_t slStukaChaseEnemy[] = {
	{ tlStukaChaseEnemy, ARRAYSIZE( tlStukaChaseEnemy ),
	  bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK1 |
	      bits_COND_CAN_MELEE_ATTACK2 | bits_COND_NEW_ENEMY | bits_COND_HEAR_SOUND | bits_COND_ENEMY_DEAD,
	  bits_SOUND_DANGER, "StukaChaseEnemy" }
};
static Task_t tlStukaGroundTakeOff[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_LEAP },
	{ TASK_PLAY_SEQUENCE, (float) ACT_HOVER }
};
static Schedule_t slStukaGroundTakeOff[] = {
	{
		tlStukaGroundTakeOff,
		ARRAYSIZE( tlStukaGroundTakeOff ),
		0,
		0,
		"StukaGroundTakeOff"
	}
};
static Task_t tlStukaVictoryDance[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_HOVER },
	{ TASK_STUKA_FIND_GROUND, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_LAND },
	{ TASK_GET_PATH_TO_ENEMY_CORPSE, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_EAT }
};
static Schedule_t slStukaVictoryDance[] = {
	{ tlStukaVictoryDance, ARRAYSIZE( tlStukaVictoryDance ),
	  bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY | bits_COND_HEAR_SOUND,
	  bits_SOUND_DANGER, "StukaVictoryDance" }
};

DEFINE_CUSTOM_SCHEDULES( CStukabat ){
	slStukaRestPerch,
	slStukaFlinchWakeAngry,
	slStukaGroundTakeOff,
	slStukaChaseEnemy,
	slStukaVictoryDance,
	slStukaDie
};
IMPLEMENT_CUSTOM_SCHEDULES( CStukabat, CBaseMonster );
LINK_ENTITY_TO_CLASS( monster_stukabat, CStukabat );

void CStukabat::Spawn( void )
{
	Precache();

	if ( !m_fCustomModel )
		UTIL_SetModel( this, "models/stukabat.mdl" );

	UTIL_SetSize( pev, Vector( -12, -12, 0 ), Vector( 12, 12, 24 ) );

	if ( pev->health == 0 )
		pev->health = 100;

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_FLY;
	SetBits( pev->flags, FL_FLY );
	pev->effects = 0;
	pev->view_ofs = Vector( 0, 0, 22 );
	pev->yaw_speed = 150;
	m_afCapability = bits_CAP_HEAR;
	m_flFieldOfView = 0.5;
	m_MonsterState = MONSTERSTATE_NONE;
	m_bloodColor = BLOOD_COLOR_YELLOW;
	m_stopTime = gpGlobals->time;
	m_pFlapSound = "zombie/claw_miss1.wav";
	m_flMaxSpeed = 400;

	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( "Stukabat" );

	MonsterInit();
	SetActivity( ACT_FLY );
	SetTouch( &CStukabat::PainTouch );
	SetActiveMode( 0 );
	m_flightSpeed = 0;
	m_SaveVelocity = pev->velocity;
	m_bTouched = false;
}

void CStukabat::Precache( void )
{
	CBaseEntity::Precache();

	if ( !pev->model )
		UTIL_PrecacheModel( this, "models/stukabat.mdl" );

	PrecacheSound_Intercept( this, "zombie/claw_miss1.wav" );
	PrecacheSound_Intercept( this, "zombie/claw_miss2.wav" );
	PrecacheSound_Intercept( this, "headcrab/hc_headbite.wav" );
}

int CStukabat::Classify( void )
{
	return GetClassification( CLASS_ALIEN_PREDATOR );
}

float CStukabat::GetPointsForDamage( float damage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	float points = pev->max_health / sk_stukabat.value;

	return (float) ( points * 0.5 * ( damage / pev->max_health ) );
}

void CStukabat::SetYawSpeed( void )
{
	pev->yaw_speed = 150;
}

BOOL CStukabat::CheckRangeAttack1( float dot, float distance )
{
	return distance <= 56 && dot >= 0.5;
}

BOOL CStukabat::CheckRangeAttack2( float dot, float distance )
{
	return FALSE;
}

BOOL CStukabat::CheckMeleeAttack1( float dot, float distance )
{
	return distance <= 64 && dot >= 0.5;
}

void CStukabat::HandleAnimEvent( MonsterEvent_t *event )
{
	switch ( event->event )
	{
		case STUKABAT_AE_BITE:
			if ( CheckTraceHullAttack( 80, sk_stukabat_dmg_bite.value, DMG_POISON ) )
				EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "headcrab/hc_headbite.wav", 0.6, ATTN_IDLE,
				                0, 150 + RANDOM_LONG( -10, 10 ) );
			else
				EMIT_SOUND_DYN( edict(), CHAN_BODY, "zombie/claw_miss2.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			break;
		case FLYING_AE_FLAP:
			break;
		case FLYING_AE_FLAPSOUND:
			if ( m_pFlapSound )
				EMIT_SOUND( edict(), CHAN_BODY, m_pFlapSound, 1, ATTN_NORM );
			break;
		default:
			CBaseMonster::HandleAnimEvent( event );
			break;
	}
}

void CStukabat::SetActivity( Activity activity )
{
	if ( activity == ACT_IDLE && m_iActiveMode != 2 )
		activity = pev->movetype == MOVETYPE_STEP && pev->groundentity ? ACT_CROUCHIDLE : ACT_HOVER;

	CBaseMonster::SetActivity( activity );
}

Activity CStukabat::GetStoppedActivity( void )
{
	return pev->movetype == MOVETYPE_FLY ? ACT_HOVER : ACT_CROUCHIDLE;
}

void CStukabat::Stop( void )
{
	m_IdealActivity = GetStoppedActivity();
	m_flightSpeed = 0;
	pev->velocity = g_vecZero;
	m_SaveVelocity = pev->velocity;
}

void CStukabat::SetActiveMode( int mode )
{
	if ( mode == 0 )
		m_stopTime = gpGlobals->time + 5;

	m_iActiveMode = mode;
}

void CStukabat::PainTouch( CBaseEntity *other )
{
	if ( other && IRelationship( other ) > R_NO )
	{
		m_flLastTouch = gpGlobals->time;
		m_bTouched = true;
	}
}

void CStukabat::MonsterThink( void )
{
	CBaseMonster::MonsterThink();

	if ( HasMemory( bits_MEMORY_KILLED ) )
		return;

	Vector direction = ( m_Route[m_iRouteIndex].vecLocation - pev->origin ).Normalize();

	m_SaveVelocity = direction * m_flightSpeed * 0.2f + m_SaveVelocity * 0.8f;

	if ( m_flightSpeed <= 0 )
		m_SaveVelocity = g_vecZero;

	pev->velocity = m_SaveVelocity;

	if ( m_MonsterState == MONSTERSTATE_IDLE || m_MonsterState == MONSTERSTATE_ALERT )
	{
		if ( gpGlobals->time > m_stopTime && m_iActiveMode != 2 )
			SetActiveMode( 1 );

		if ( m_iActiveMode == 0 )
			SetActiveMode( 4 );
	}
	else
	{
		if ( HasConditions( bits_COND_SEE_ENEMY ) )
			m_stopTime = gpGlobals->time + 5;

		if ( m_iActiveMode != 4 )
			SetActiveMode( 0 );
	}
}

BOOL CStukabat::ShouldAdvanceRoute( float waypointDistance )
{
	Vector delta = m_Route[m_iRouteIndex].vecLocation - pev->origin;

	return FBitSet( pev->flags, FL_ONGROUND ) ? delta.Length2D() <= 8 : delta.Length() <= 8;
}

int CStukabat::CheckLocalMove( const Vector &start, const Vector &end, CBaseEntity *target,
                               float *distance, Vector *endPosition, bool walkMoveCheck )
{
	if ( pev->movetype == MOVETYPE_STEP )
		return CBaseMonster::CheckLocalMove( start, end, target, distance, endPosition, false );

	TraceResult trace;

	trace.vecEndPos = g_vecZero;
	trace.vecPlaneNormal = g_vecZero;
	TRACE_MONSTER_HULL( edict(), start, end, dont_ignore_monsters, edict(), &trace );

	if ( distance )
		*distance = ( trace.vecEndPos - start ).Length();

	if ( endPosition )
		*endPosition = trace.vecEndPos;

	if ( trace.fStartSolid || trace.flFraction < 1.0 )
	{
		if ( target && target->edict() == trace.pHit )
			return LOCALMOVE_VALID;

		return LOCALMOVE_INVALID;
	}

	return LOCALMOVE_VALID;
}

void CStukabat::MoveExecute( CBaseEntity *target, const Vector &direction, float interval )
{
	if ( pev->movetype != MOVETYPE_FLY )
	{
		CBaseMonster::MoveExecute( target, direction, interval );
		return;
	}

	if ( m_IdealActivity != m_movementActivity )
		m_IdealActivity = m_movementActivity;

	MakeIdealYaw( pev->origin + m_SaveVelocity );
	ChangeYaw( pev->yaw_speed );
}

float CStukabat::CeilingPos( const Vector &position )
{
	Vector ceiling = position + Vector( 0, 0, 4096 );
	TraceResult trace;

	trace.vecEndPos = g_vecZero;
	trace.vecPlaneNormal = g_vecZero;
	TRACE_MONSTER_HULL( edict(), position, ceiling, dont_ignore_monsters, edict(), &trace );

	if ( trace.flFraction != 1 )
		ceiling.z = trace.vecEndPos.z;

	return ceiling.z;
}

float CStukabat::FloorZ( const Vector &position )
{
	Vector floor = position - Vector( 0, 0, 2048 );
	TraceResult trace;

	trace.vecEndPos = g_vecZero;
	trace.vecPlaneNormal = g_vecZero;
	UTIL_TraceLine( position, floor, ignore_monsters, NULL, &trace );

	return trace.flFraction == 1 ? floor.z : trace.vecEndPos.z;
}

bool CStukabat::FindPerchPosition( Vector position, Vector &perch )
{
	position.z = CeilingPos( position );

	if ( UTIL_PointContents( position ) < CONTENTS_SOLID )
		return false;

	perch = position;

	return true;
}

bool CStukabat::FindPerchNode( float distance, Vector &perch )
{
	if ( !WorldGraph.m_fGraphPresent || !WorldGraph.m_fGraphPointersSet )
	{
		ALERT( at_aiconsole, "Graph not ready for FindPerchNode!\n" );
		return false;
	}

	if ( distance == 0 )
		distance = 784;

	int nearest = WorldGraph.FindNearestNode( pev->origin, this );

	WorldGraph.HullIndex( this );

	if ( nearest == NO_NODE )
	{
		ALERT( at_aiconsole, "FindPerchNode() - %s has no nearest node!\n", STRING( pev->classname ) );
		return false;
	}

	for ( int i = 0; i < WorldGraph.m_cNodes; i++ )
	{
		int index = ( i + WorldGraph.m_iLastActiveIdleSearch ) % WorldGraph.m_cNodes;
		CNode &node = WorldGraph.Node( index );

		if ( !( node.m_afNodeInfo & bits_NODE_AIR ) )
			continue;

		WorldGraph.m_iLastActiveIdleSearch = index + 1;

		if ( ( pev->origin - node.m_vecOrigin ).Length() < distance &&
		     FindPerchPosition( node.m_vecOrigin, perch ) )
			return true;
	}

	return false;
}

void CStukabat::StartTask( Task_t *task )
{
	switch ( task->iTask )
	{
		case TASK_RUN_PATH:
		case TASK_WALK_PATH:
		case TASK_STUKA_MOVE:
			m_movementActivity = pev->movetype == MOVETYPE_FLY ? ACT_FLY : ACT_WALK;
			TaskComplete();
			break;
		case TASK_PLAY_SEQUENCE:
			switch ( (int) task->flData )
			{
				case ACT_LEAP:
					pev->movetype = MOVETYPE_FLY;
					ClearBits( pev->flags, FL_ONGROUND );
					SetBits( pev->flags, FL_FLY );
					UTIL_SetOrigin( pev, pev->origin );
					break;
				case ACT_LAND:
					pev->movetype = MOVETYPE_STEP;
					ClearBits( pev->flags, FL_ONGROUND | FL_FLY );
					UTIL_SetOrigin( pev, pev->origin );
					break;
				case ACT_STAND:
					SetActiveMode( 2 );
					break;
			}

			CBaseMonster::StartTask( task );
			break;
		case TASK_STUKA_FIND_PERCH:
		{
			Vector perch = pev->origin;

			if ( !FindPerchNode( 1024, perch ) && !FindPerchPosition( perch, perch ) )
			{
				TaskFail();
				break;
			}

			if ( MoveToLocation( m_movementActivity, 0, perch ) )
				TaskComplete();
			else
				TaskFail();
			break;
		}
		case TASK_STUKA_FIND_GROUND:
		{
			Vector floor = pev->origin;

			floor.z = FloorZ( pev->origin );

			if ( MoveToLocation( m_movementActivity, 0, floor ) )
				TaskComplete();
			else
				TaskFail();
			break;
		}
		case TASK_STUKA_TAKE_OFF:
			if ( pev->movetype == MOVETYPE_FLY )
				TaskComplete();
			else
				m_IdealActivity = ACT_LEAP;
			break;
		case TASK_STUKA_FALL:
			pev->movetype = MOVETYPE_TOSS;
			ClearBits( pev->flags, FL_ONGROUND | FL_FLY );
			pev->angles.z = 0;
			pev->angles.x = 0;
			m_IdealActivity = ACT_FALL;
			break;
		default:
			CBaseMonster::StartTask( task );
			break;
	}
}

void CStukabat::RunTask( Task_t *task )
{
	switch ( task->iTask )
	{
		case TASK_PLAY_SEQUENCE:
			CBaseMonster::RunTask( task );

			if ( (int) task->flData == 8 )
			{
				if ( m_iTaskStatus == TASKSTATUS_COMPLETE )
					UTIL_SetOrigin( pev, pev->origin );

				pev->velocity = g_vecZero;
				m_flightSpeed = 0;
				pev->groundentity = NULL;
				pev->movetype = MOVETYPE_FLY;
				ClearBits( pev->flags, FL_ONGROUND );
				SetBits( pev->flags, FL_FLY );
			}
			else if ( (int) task->flData == ACT_HOVER )
				SetActiveMode( 0 );
			break;
		case TASK_WAIT_FOR_MOVEMENT:
			if ( pev->movetype == MOVETYPE_FLY )
			{
				if ( ( m_vecMoveGoal - pev->origin ).Length2D() < pev->size.x * 4 &&
				     fabs( m_vecMoveGoal.z - pev->origin.z ) > pev->size.y )
					m_movementActivity = ACT_HOVER;
				else
					m_movementActivity = ACT_FLY;
			}

			CBaseMonster::RunTask( task );
			break;
		case TASK_STUKA_TAKE_OFF:
			if ( m_fSequenceFinished )
				TaskComplete();
			break;
		case TASK_STUKA_FALL:
			if ( pev->groundentity )
				TaskComplete();
			break;
		default:
			CBaseMonster::RunTask( task );
			break;
	}
}

Schedule_t *CStukabat::GetScheduleOfType( int type )
{
	switch ( type )
	{
		case SCHED_CHASE_ENEMY:
			return slStukaChaseEnemy;
		case SCHED_VICTORY_DANCE:
			return slStukaVictoryDance;
		case SCHED_DIE:
			return slStukaDie;
		case SCHED_STUKA_REST_PERCH:
			return slStukaRestPerch;
		case SCHED_STUKA_FLINCH_WAKE_ANGRY:
			return slStukaFlinchWakeAngry;
		default:
			return CBaseMonster::GetScheduleOfType( type );
	}
}

Schedule_t *CStukabat::GetSchedule( void )
{
	switch ( m_MonsterState )
	{
		case MONSTERSTATE_COMBAT:
			if ( pev->movetype == MOVETYPE_STEP )
				return slStukaGroundTakeOff;

			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
			{
				m_hEnemy = NULL;

				if ( GetEnemy() )
					ClearConditions( bits_COND_ENEMY_DEAD );
				else
					SetState( MONSTERSTATE_ALERT );

				return GetSchedule();
			}

			if ( HasAllConditions( bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE ) )
				return GetScheduleOfType( SCHED_SMALL_FLINCH );

			if ( HasConditions( bits_COND_HEAVY_DAMAGE ) )
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
			break;
		case MONSTERSTATE_ALERT:
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
				return GetScheduleOfType( SCHED_VICTORY_DANCE );

			// Fall through
		case MONSTERSTATE_IDLE:
			if ( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
			{
				SetActiveMode( 0 );
				return GetScheduleOfType( SCHED_SMALL_FLINCH );
			}

			if ( pev->movetype == MOVETYPE_STEP || pev->groundentity )
				return slStukaGroundTakeOff;

			if ( m_iActiveMode == 1 )
				return GetScheduleOfType( SCHED_STUKA_REST_PERCH );

			if ( m_iActiveMode == 2 )
				return GetScheduleOfType( SCHED_IDLE_STAND );
			break;
	}

	return CBaseMonster::GetSchedule();
}

void CStukabat::Move( float interval )
{
	if ( pev->movetype == MOVETYPE_FLY && m_flightSpeed < 10 )
		m_flightSpeed = 370;

	if ( pev->movetype == MOVETYPE_STEP )
	{
		m_flightSpeed = 0;
		CBaseMonster::Move( interval );
		return;
	}

	if ( FRouteClear() )
	{
		ALERT( at_aiconsole, "Tried to move with no route!\n" );
		TaskFail();
		return;
	}

	if ( m_flMoveWaitFinished > gpGlobals->time )
		return;

	Vector delta = m_Route[m_iRouteIndex].vecLocation - pev->origin;
	Vector direction = delta.Normalize();
	float distance = delta.Length();
	float checkDistance = V_min( distance, 200.0f );
	int routeType = m_Route[m_iRouteIndex].iType & ~bits_MF_NOT_TO_MASK;
	CBaseEntity *target = NULL;

	if ( routeType == bits_MF_TO_ENEMY )
		target = m_hEnemy;
	else if ( routeType == bits_MF_TO_TARGETENT )
		target = m_hTargetEnt;

	float travelled = 0;
	int localMove = CheckLocalMove( pev->origin, pev->origin + direction * checkDistance,
	                                target, &travelled, NULL, false );

	if ( localMove != LOCALMOVE_VALID )
	{
		Stop();

		CBaseEntity *blocker = CBaseEntity::Instance( gpGlobals->trace_ent );

		if ( blocker )
		{
			DispatchBlocked( edict(), blocker->edict() );

			if ( m_moveWaitTime > 0 && blocker->IsMoving() && !blocker->IsPlayer() &&
			     gpGlobals->time - m_flMoveWaitFinished > 3 )
			{
				if ( m_flightSpeed > travelled )
				{
					m_flMoveWaitFinished = gpGlobals->time + m_moveWaitTime;
					return;
				}

				localMove = LOCALMOVE_VALID;
			}
		}

		if ( localMove != LOCALMOVE_VALID )
		{
			Vector apex = g_vecZero;
			Vector apex2 = g_vecZero;

			for ( int i = m_iRouteIndex; i < ROUTE_SIZE; i++ )
			{
				if ( !FTriangulate( pev->origin, m_Route[i].vecLocation, distance, target, &apex, &apex2, false ) )
					continue;

				if ( apex2 != g_vecZero )
					InsertWaypoint( apex2, bits_MF_TO_DETOUR );

				InsertWaypoint( apex, bits_MF_TO_DETOUR );
				RouteSimplify( target );
				localMove = LOCALMOVE_VALID;
				break;
			}
		}

		if ( localMove != LOCALMOVE_VALID && blocker && !blocker->IsBSPModel() &&
		     gpGlobals->time - 1 > m_flMoveWaitFinished && !HasMemory( bits_MEMORY_MOVE_FAILED ) )
		{
			m_flMoveWaitFinished = gpGlobals->time;
			localMove = LOCALMOVE_VALID;
		}

		if ( localMove != LOCALMOVE_VALID )
		{
			if ( m_moveWaitTime > 0 && !HasMemory( bits_MEMORY_MOVE_FAILED ) )
			{
				int radius = (int) m_moveradius;

				if ( FRefreshRoute( false ) )
				{
					if ( gpGlobals->time - m_flMoveWaitFinished < 0.2 )
						Remember( bits_MEMORY_MOVE_FAILED );

					m_flMoveWaitFinished = gpGlobals->time + 0.1;
					Stop();
				}
				else
					TaskFail();

				m_moveradius = radius;
			}
			else
			{
				Stop();
				TaskFail();
				m_flMoveWaitFinished = gpGlobals->time + 0.5;
				ALERT( at_aiconsole, "%s Faile\td to move (%d)!\n", STRING( pev->classname ),
				       HasMemory( bits_MEMORY_MOVE_FAILED ) != FALSE );
			}

			return;
		}
	}

	m_flGroundSpeed = m_flightSpeed;

	if ( ShouldAdvanceRoute( distance ) )
		AdvanceRoute( distance );

	MoveExecute( target, direction, interval );

	if ( MovementIsComplete() )
	{
		Stop();
		RouteClear();
	}
	else if ( distance < 256 )
	{
		CBaseEntity *goal = m_movementGoal == MOVEGOAL_ENEMY ? (CBaseEntity *) m_hEnemy : (CBaseEntity *) m_hTargetEnt;

		RouteSimplify( goal );
	}
}

bool CStukabat::ProbeZ( const Vector &position, const Vector &probe, float *fraction )
{
	int contents = UTIL_PointContents( position );

	if ( ( FBitSet( pev->flags, FL_SWIM ) != 0 ) != ( contents == CONTENTS_WATER ) )
	{
		*fraction = 0;
		return true;
	}

	if ( UTIL_PointContents( probe ) == contents )
	{
		*fraction = 1;
		return false;
	}

	Vector direction = ( probe - position ).Normalize();
	float length = ( probe - position ).Length();
	float lower = 0;
	float upper = length;

	while ( upper - lower > 1 )
	{
		float midpoint = ( upper - lower ) * 0.5f + lower;
		Vector sample = position + direction * midpoint;

		if ( UTIL_PointContents( sample ) == contents )
			lower = midpoint;
		else
			upper = midpoint;
	}

	*fraction = lower / length;

	return true;
}

Vector CStukabat::DoProbe( const Vector &probe )
{
	Vector normal( 0, 0, 1 );
	float fraction;
	bool bumped = ProbeZ( pev->origin, probe, &fraction );
	TraceResult trace;

	trace.vecEndPos = g_vecZero;
	trace.vecPlaneNormal = g_vecZero;
	TRACE_MONSTER_HULL( edict(), pev->origin, probe, dont_ignore_monsters, edict(), &trace );

	if ( trace.fAllSolid || trace.flFraction < 0.99 )
	{
		trace.flFraction = V_max( 0.0f, V_min( 1.0f, trace.flFraction ) );

		if ( fraction > trace.flFraction )
		{
			fraction = trace.flFraction;
			bumped = true;
			normal = trace.vecPlaneNormal;
		}
	}

	if ( !bumped || ( m_hEnemy != NULL && trace.pHit == m_hEnemy->edict() ) )
		return g_vecZero;

	Vector direction = probe - pev->origin;
	Vector steering = CrossProduct( CrossProduct( direction, normal ), direction );
	float force = DotProduct( normal.Normalize(), m_SaveVelocity.Normalize() ) * ( ( 1 - fraction ) * m_flightSpeed );

	if ( force < 0 )
		force = -force;

	return steering.Normalize() * force;
}

void CStukabat::Fly( void )
{
	if ( m_flightSpeed >= 350 )
	{
		if ( m_Activity != ACT_GLIDE )
		{
			SetSequenceByName( "Dive_cycler" );
			m_Activity = ACT_GLIDE;
			m_IdealActivity = ACT_GLIDE;
		}

		m_flightSpeed = 180;
	}
	else if ( m_Activity != ACT_FLY )
		SetActivity( ACT_FLY );

	if ( m_bTouched )
	{
		if ( m_flMaxSpeed > m_flightSpeed )
			m_flightSpeed += 40;
	}
	else if ( m_flightSpeed > m_flMaxSpeed )
		m_flightSpeed -= 4;

	Vector angles = UTIL_VecToAngles( m_SaveVelocity );
	Vector forward, right, up;

	angles.x = -angles.x;
	UTIL_MakeVectorsPrivate( angles, forward, right, up );

	Vector front = DoProbe( pev->origin + forward * 300 );
	Vector rightProbe = DoProbe( pev->origin + forward * 100 + right );
	Vector leftProbe = DoProbe( pev->origin + forward * 100 - right );
	Vector upProbe = DoProbe( pev->origin + forward * 100 + up );
	Vector downProbe = DoProbe( pev->origin + forward * 100 - up );

	m_SaveVelocity = ( m_SaveVelocity + ( front + rightProbe + leftProbe + upProbe + downProbe ) * 0.5f ).Normalize();
	angles = Vector( -pev->angles.x, pev->angles.y, pev->angles.z );
	UTIL_MakeVectorsPrivate( angles, forward, right, up );

	float dot = DotProduct( forward, m_SaveVelocity );

	if ( dot > 0.5 )
		m_SaveVelocity = m_SaveVelocity * m_flightSpeed;
	else if ( dot > 0 )
		m_SaveVelocity = m_SaveVelocity * m_flightSpeed * ( dot + 0.5f );
	else
		m_SaveVelocity = m_SaveVelocity * 80;

	pev->velocity = m_SaveVelocity;
	angles = UTIL_VecToAngles( m_SaveVelocity );

	if ( angles.x > 180 )
		angles.x -= 360;

	pev->angles.x = UTIL_Approach( angles.x, pev->angles.x, 50 * m_flFrameTime );
	pev->angles.x = V_max( -80.0f, V_min( 80.0f, pev->angles.x ) );

	float turn = 360;

	if ( fabs( angles.y - pev->angles.y ) < fabs( turn ) )
		turn = angles.y - pev->angles.y;

	if ( fabs( angles.y - pev->angles.y + 360 ) < fabs( turn ) )
		turn = angles.y - pev->angles.y + 360;

	if ( fabs( angles.y - pev->angles.y - 360 ) < fabs( turn ) )
		turn = angles.y - pev->angles.y - 360;

	float speed = m_flightSpeed * m_flFrameTime;

	if ( fabs( turn ) > speed )
		turn = turn < 0 ? -speed : speed;

	pev->angles.y += turn;
	pev->angles.z -= turn;
	pev->angles.y = fmod( pev->angles.y + 360.0, 360.0 );
	turn = 360;

	if ( fabs( angles.z - pev->angles.z ) < fabs( turn ) )
		turn = angles.z - pev->angles.z;

	if ( fabs( angles.z - pev->angles.z + 360 ) < fabs( turn ) )
		turn = angles.z - pev->angles.z + 360;

	if ( fabs( angles.z - pev->angles.z - 360 ) < fabs( turn ) )
		turn = angles.z - pev->angles.z - 360;

	speed = m_flightSpeed * 0.5f * m_flFrameTime;

	if ( fabs( turn ) < speed )
		pev->angles.z += turn;
	else
		pev->angles.z += turn < 0 ? -speed : speed;

	pev->angles.z = V_max( -20.0f, V_min( 20.0f, pev->angles.z ) );
}
