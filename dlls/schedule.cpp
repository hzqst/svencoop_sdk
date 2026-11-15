/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// schedule.cpp - functions and data pertaining to the 
// monsters' AI scheduling system.
//=========================================================
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "animation.h"
#include "scripted.h"
#include "nodes.h"
#include "defaultai.h"
#include "soundent.h"

#include "func_tank.h"
#include "weapons.h"

#define THREAT_SOUND_NEAR_DIST			768
#define COMBAT_SOUND_APPROACH			128
#define GRENADE_MOVE_TIME				2
#define TARGET_REPATH_DIST				80
#define MOVE_AWAY_COVER_WAIT			2
#define CORPSE_REACH_DIST				96
#define COVER_ENEMY_MIN_DIST			128
#define COVER_ORIGIN_SAFETY_MIN			256
#define TARGET_RANGE_MOVE_RADIUS		96
#define TARGET_RANGE_FORCE_MOVE_RADIUS	64
#define HINT_LINK_MOVE_RADIUS			32
#define GUARD_FOLLOW_DIST				256
#define GUARD_COMBAT_DIST				512
#define GUARD_RECALL_DIST				2048
#define ENEMY_FACING_DOT				0.9
#define SCRIPT_MOVETO_INSTANT			4

CBaseEntity *TankTaskEntity( EHANDLE &handle )
{
	edict_t *entity = handle.Get();

	if ( !entity || entity->free )
		return NULL;

	return (CBaseEntity *) GET_PRIVATE( entity );
}

extern CGraph WorldGraph;

// a danger sound left by a grenade that has already gone off
bool DangerSoundExploded( CSound *pSound )
{
	CBaseEntity *pOwner = pSound->m_hOwner;

	if ( pOwner == NULL || !FClassnameIs( pOwner->pev, "grenade" ) )
		return false;

	CGrenade *pGrenade = dynamic_cast<CGrenade *>( pOwner->MyItemPointer() );

	return pGrenade != NULL && pGrenade->m_bExploded;
}

void FollowGuardEnt( CBaseMonster *pMonster )
{
	if ( pMonster->m_hTargetEnt != NULL && pMonster->m_hTargetEnt->edict() == pMonster->m_hGuardEnt->edict() )
		return;

	pMonster->StartPlayerFollowing( pMonster->m_hGuardEnt, false );
}

//=========================================================
// FHaveSchedule - Returns TRUE if monster's m_pSchedule
// is anything other than NULL.
//=========================================================
BOOL CBaseMonster :: FHaveSchedule( void )
{
	if ( m_pSchedule == NULL )
	{
		return FALSE;
	}

	return TRUE;
}

//=========================================================
// ClearSchedule - blanks out the caller's schedule pointer
// and index.
//=========================================================
void CBaseMonster :: ClearSchedule( void )
{
	m_iTaskStatus = TASKSTATUS_NEW;
	m_pSchedule = NULL;
	m_iScheduleIndex = 0;
}

//=========================================================
// FScheduleDone - Returns TRUE if the caller is on the
// last task in the schedule
//=========================================================
BOOL CBaseMonster :: FScheduleDone ( void )
{
	ASSERT( m_pSchedule != NULL );
	
	if ( m_iScheduleIndex == m_pSchedule->cTasks )
	{
		return TRUE;
	}

	return FALSE;
}

//=========================================================
// ChangeSchedule - replaces the monster's schedule pointer
// with the passed pointer, and sets the ScheduleIndex back
// to 0
//=========================================================
void CBaseMonster :: ChangeSchedule ( Schedule_t *pNewSchedule )
{
	ASSERT( pNewSchedule != NULL );

	if ( m_pSchedule != GetScheduleOfType( SCHED_SMALL_FLINCH_SPECIAL ) )
		m_pScheduleSaved = m_pSchedule;

	m_pSchedule			= pNewSchedule;
	m_iScheduleIndex	= 0;
	m_iTaskStatus		= TASKSTATUS_NEW;
	m_afConditions		= 0;// clear all of the conditions
	m_failSchedule		= SCHED_NONE;

	if ( m_pSchedule->iInterruptMask & bits_COND_HEAR_SOUND && !(m_pSchedule->iSoundMask) )
	{
		ALERT ( at_aiconsole, "COND_HEAR_SOUND with no sound mask!\n" );
	}
	else if ( m_pSchedule->iSoundMask && !(m_pSchedule->iInterruptMask & bits_COND_HEAR_SOUND) )
	{
		ALERT ( at_aiconsole, "Sound mask without COND_HEAR_SOUND!\n" );
	}

#if _DEBUG
	if ( !ScheduleFromName( pNewSchedule->pName ) )
	{
		ALERT( at_console, "Schedule %s not in table!!!\n", pNewSchedule->pName );
	}
#endif
	
// this is very useful code if you can isolate a test case in a level with a single monster. It will notify
// you of every schedule selection the monster makes.
#if 0
	if ( FClassnameIs( pev, "monster_human_grunt" ) )
	{
		Task_t *pTask = GetTask();
		
		if ( pTask )
		{
			const char *pName = NULL;

			if ( m_pSchedule )
			{
				pName = m_pSchedule->pName;
			}
			else
			{
				pName = "No Schedule";
			}
			
			if ( !pName )
			{
				pName = "Unknown";
			}

			ALERT( at_aiconsole, "%s: picked schedule %s\n", STRING( pev->classname ), pName );
		}
	}
#endif// 0

}

//=========================================================
// NextScheduledTask - increments the ScheduleIndex
//=========================================================
void CBaseMonster :: NextScheduledTask ( void )
{
	ASSERT( m_pSchedule != NULL );

	m_iTaskStatus = TASKSTATUS_NEW;
	m_iScheduleIndex++;

	if ( FScheduleDone() )
	{
		// just completed last task in schedule, so make it invalid by clearing it.
		SetConditions( bits_COND_SCHEDULE_DONE );
		//ClearSchedule();	
	}
}

//=========================================================
// IScheduleFlags - returns an integer with all Conditions
// bits that are currently set and also set in the current
// schedule's Interrupt mask.
//=========================================================
int CBaseMonster :: IScheduleFlags ( void )
{
	if( !m_pSchedule )
	{
		return 0;
	}
	
	// strip off all bits excepts the ones capable of breaking this schedule.
	return m_afConditions & m_pSchedule->iInterruptMask;
}

//=========================================================
// FScheduleValid - returns TRUE as long as the current
// schedule is still the proper schedule to be executing,
// taking into account all conditions
//=========================================================
BOOL CBaseMonster :: FScheduleValid ( void )
{
	if ( m_pSchedule == NULL )
	{
		// schedule is empty, and therefore not valid.
		return FALSE;
	}

	// nothing but the schedule itself ends an AI script
	if ( m_fInAIScript && ( m_MonsterState == MONSTERSTATE_SCRIPT || m_IdealMonsterState == MONSTERSTATE_SCRIPT )
		&& !HasConditions( bits_COND_SCHEDULE_DONE | bits_COND_TASK_FAILED ) )
	{
		return TRUE;
	}

	if ( HasConditions( m_pSchedule->iInterruptMask | bits_COND_SCHEDULE_DONE | bits_COND_TASK_FAILED ) )
	{
#ifdef DEBUG
		if ( HasConditions ( bits_COND_TASK_FAILED ) && m_failSchedule == SCHED_NONE )
		{
			// fail! Send a visual indicator.
			ALERT ( at_aiconsole, "Schedule: %s Failed\n", m_pSchedule->pName );

			Vector tmp = pev->origin;
			tmp.z = pev->absmax.z + 16;
			UTIL_Sparks( tmp );
		}
#endif // DEBUG

		// some condition has interrupted the schedule, or the schedule is done
		return FALSE;
	}
	
	return TRUE;
}

//=========================================================
// MaintainSchedule - does all the per-think schedule maintenance.
// ensures that the monster leaves this function with a valid
// schedule!
//=========================================================
void CBaseMonster :: MaintainSchedule ( void )
{
	Schedule_t	*pNewSchedule;
	int			i;

	// UNDONE: Tune/fix this 10... This is just here so infinite loops are impossible
	for ( i = 0; i < 10; i++ )
	{
		if ( m_pSchedule != NULL && TaskIsComplete() )
		{
			NextScheduledTask();                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   
		}

	// validate existing schedule 
		if ( !FScheduleValid() || m_MonsterState != m_IdealMonsterState )
		{
			// if we come into this block of code, the schedule is going to have to be changed.
			// if the previous schedule was interrupted by a condition, GetIdealState will be 
			// called. Else, a schedule finished normally.

			// Notify the monster that his schedule is changing
			ScheduleChange();

			// Call GetIdealState if we're not dead and one or more of the following...
			// - in COMBAT state with no enemy (it died?)
			// - conditions bits (excluding SCHEDULE_DONE) indicate interruption,
			// - schedule is done but schedule indicates it wants GetIdealState called
			//   after successful completion (by setting bits_COND_SCHEDULE_DONE in iInterruptMask)
			// DEAD & SCRIPT are not suggestions, they are commands!
			if ( m_IdealMonsterState != MONSTERSTATE_DEAD && 
				 (m_IdealMonsterState != MONSTERSTATE_SCRIPT || m_IdealMonsterState == m_MonsterState) )
			{
				if (	(m_afConditions && !HasConditions(bits_COND_SCHEDULE_DONE)) ||
						(m_pSchedule && (m_pSchedule->iInterruptMask & bits_COND_SCHEDULE_DONE)) ||
						((m_MonsterState == MONSTERSTATE_COMBAT) && (m_hEnemy == NULL))	)
				{
					GetIdealState();
				}
			}
			if ( HasConditions( bits_COND_TASK_FAILED ) && m_MonsterState == m_IdealMonsterState )
			{
				if ( m_failSchedule != SCHED_NONE )
					pNewSchedule = GetScheduleOfType( m_failSchedule );
				else
					pNewSchedule = GetScheduleOfType( SCHED_FAIL );
				// schedule was invalid because the current task failed to start or complete
				ALERT ( at_aiconsole, "Schedule Failed at %d!\n", m_iScheduleIndex );
				ChangeSchedule( pNewSchedule );
			}
			else
			{
				SetState( m_IdealMonsterState );
				if ( m_MonsterState == MONSTERSTATE_SCRIPT || m_MonsterState == MONSTERSTATE_DEAD )
					pNewSchedule = CBaseMonster::GetSchedule();
				else if ( m_pSchedule && m_pSchedule == GetScheduleOfType( SCHED_SMALL_FLINCH_SPECIAL ) && m_pScheduleSaved )
				{
					pNewSchedule = m_pScheduleSaved;
					m_pScheduleSaved = NULL;
				}
				else
					pNewSchedule = GetSchedule();
				ChangeSchedule( pNewSchedule );
			}
		}

		if ( m_iTaskStatus == TASKSTATUS_NEW )
		{
			Task_t *pTask = GetTask();
			ASSERT( pTask != NULL );
			TaskBegin();
			StartTask( pTask );
		}

		// UNDONE: Twice?!!!
		if ( m_Activity != m_IdealActivity )
		{
			SetActivity ( m_IdealActivity );
		}

		if ( !TaskIsComplete() && m_iTaskStatus != TASKSTATUS_NEW )
			break;
	}

	if ( TaskIsRunning() )
	{
		Task_t *pTask = GetTask();
		ASSERT( pTask != NULL );
		RunTask( pTask );
	}

	// UNDONE: We have to do this so that we have an animation set to blend to if RunTask changes the animation
	// RunTask() will always change animations at the end of a script!
	// Don't do this twice
	if ( m_Activity != m_IdealActivity )
	{
		SetActivity ( m_IdealActivity );
	}
}

#define AP_WAIT_UNTIL_TARGETED		4	// TASK_WAIT_FOR_MOVEMENT_AP flData
#define CINE_BLOCKER_MARGIN			16	// how close to a blocking player a scripted move may stop

//=========================================================
// RunTask 
//=========================================================
void CBaseMonster :: RunTask ( Task_t *pTask )
{
	if ( RunPathWaypointTask( pTask ) )
		return;

	switch ( pTask->iTask )
	{
	case TASK_USE_TANK:
		{
			MakeIdealYaw( m_vecEnemyLKP );
			ChangeYaw( pev->yaw_speed );

			CFuncTank *tank = static_cast<CFuncTank *>( TankTaskEntity( m_hSelectedTank ) );

			if ( !tank )
			{
				TaskComplete();
				break;
			}

			UTIL_MakeVectors( tank->pev->angles );

			Vector goal = tank->Center();

			goal = goal - gpGlobals->v_forward * tank->m_flUseRadius;

			if ( ( GetGunPosition() - goal ).Length() > pev->size.y * 2.5f )
			{
				TaskFail();
				tank->MonsterStopControl();
				break;
			}

			CBaseEntity *enemy = TankTaskEntity( m_hEnemy.m_Handle );

			if ( !enemy || !enemy->IsAlive() || !FVisible( enemy, true ) )
			{
				m_hEnemy = BestVisibleEnemy();
				enemy = TankTaskEntity( m_hEnemy.m_Handle );
			}

			if ( !enemy )
			{
				tank->MonsterStopControl();
				TaskComplete();
				break;
			}

			UTIL_MakeVectors( tank->pev->v_angle );

			Vector2D direction = ( enemy->pev->origin - tank->pev->origin ).Make2D().Normalize();
			float dot = DotProduct( direction, gpGlobals->v_forward.Make2D() );

			if ( 1.0f - tank->m_yawRange / 100.0f > dot )
			{
				TaskComplete();
				tank->MonsterStopControl();
				ALERT( at_aiconsole, "flDot: %f\n", dot );
			}

			break;
		}
	case TASK_RUN_TO_TANK:
		{
			CFuncTank *tank = static_cast<CFuncTank *>( TankTaskEntity( m_hSelectedTank ) );

			if ( !tank )
			{
				TaskFail();
				break;
			}

			UTIL_MakeVectors( Vector( 0, tank->pev->angles.y, 0 ) );

			Vector goal = tank->Center();

			goal = goal - gpGlobals->v_forward * ( tank->m_flUseRadius + pev->size.y );

			if ( ( goal - GetGunPosition() ).Length() <= pev->size.y )
			{
				TaskComplete();
				break;
			}

			CBaseEntity *controller = TankTaskEntity( tank->m_hMonsterController );

			if ( ( controller && controller != this ) || TankTaskEntity( tank->m_hController ) )
			{
				TaskFail();
				m_flNextTankSearchTime = gpGlobals->time + 10;
			}
			else if ( MovementIsComplete() )
			{
				TaskComplete();
				RouteClear();
			}

			break;
		}

	case TASK_TURN_RIGHT:
	case TASK_TURN_LEFT:
		{
			ChangeYaw( pev->yaw_speed );

			if ( FacingIdeal() )
			{
				TaskComplete();
			}
			break;
		}

	case TASK_PLAY_SEQUENCE_FACE_ENEMY:
	case TASK_PLAY_SEQUENCE_FACE_TARGET:
		{
			CBaseEntity *pTarget;

			if ( pTask->iTask == TASK_PLAY_SEQUENCE_FACE_TARGET )
			{
				if ( ShouldStopPlayerFollow() )
				{
					StopPlayerFollowing( false, true );
					TaskFail();
					break;
				}

				pTarget = m_hTargetEnt;
			}
			else
				pTarget = m_hEnemy;

			if ( pTarget )
			{
				pev->ideal_yaw = UTIL_VecToYaw( pTarget->Center() - pev->origin );
				ChangeYaw( pev->yaw_speed );
			}

			if ( m_fSequenceFinished )
				TaskComplete();
		}
		break;

	case TASK_PLAY_SEQUENCE_COWER:
		{
			if ( m_fSequenceFinished )
			{
				TaskComplete();
				break;
			}

			if ( m_pDangerSound != NULL && DangerSoundExploded( m_pDangerSound ) )
			{
				m_pDangerSound = NULL;
				TaskComplete();
				break;
			}

			Listen();

			if ( HasConditions( bits_COND_HEAR_SOUND ) )
			{
				CSound *pSound = PBestSound();

				if ( m_pDangerSound != NULL && m_pDangerSound->m_hOwner != NULL && pSound != NULL && pSound->m_hOwner != NULL
					&& (CBaseEntity *) m_pDangerSound->m_hOwner != (CBaseEntity *) pSound->m_hOwner
					&& ( pSound->m_iType & bits_SOUND_DANGER ) )
				{
					m_pDangerSound = NULL;
					TaskComplete();
					ChangeSchedule( GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND ) );
				}
			}

			break;
		}

	case TASK_PLAY_SEQUENCE:
	case TASK_PLAY_ACTIVE_IDLE:
		{
			if ( m_fSequenceFinished )
			{
				TaskComplete();
			}
			break;
		}


	case TASK_FACE_ENEMY:
		{
			MakeIdealYaw( m_vecEnemyLKP );

			ChangeYaw( pev->yaw_speed );

			if ( FacingIdeal() )
			{
				TaskComplete();
			}
			break;
		}
	case TASK_FACE_HINTNODE:
	case TASK_FACE_LASTPOSITION:
	case TASK_FACE_TARGET:
	case TASK_FACE_IDEAL:
	case TASK_FACE_ROUTE:
		{
			ChangeYaw( pev->yaw_speed );

			if ( FacingIdeal() )
			{
				TaskComplete();
			}
			break;
		}
	case TASK_WAIT_PVS:
		{
			if ( !FNullEnt(FIND_CLIENT_IN_PVS(edict())) )
			{
				TaskComplete();
			}
			break;
		}
	case TASK_WAIT_INDEFINITE:
		{
			// don't do anything.
			break;
		}
	case TASK_WAIT:
	case TASK_WAIT_RANDOM:
		{
			if ( gpGlobals->time >= m_flWaitFinished )
			{
				TaskComplete();
			}
			break;
		}
	case TASK_WAIT_FACE_ENEMY:
		{
			if ( m_hEnemy != NULL )
			{
				MakeIdealYaw ( m_vecEnemyLKP );
				ChangeYaw( pev->yaw_speed );
			}

			if ( gpGlobals->time >= m_flWaitFinished )
			{
				TaskComplete();
			}
			break;
		}
	case TASK_MOVE_TO_TARGET_RANGE:
		{
			float distance;

			if ( m_hTargetEnt == NULL )
			{
				TaskFail();
				break;
			}

			if ( ShouldStopPlayerFollow() )
			{
				StopPlayerFollowing( false, true );
				TaskFail();
				break;
			}

			distance = ( m_vecMoveGoal - pev->origin ).Length2D();

			// Re-evaluate when the target has moved too far, unless we're on our way around something
			if ( distance > pTask->flData * 0.5 && !( m_Route[ m_iRouteIndex ].iType & bits_MF_TO_DETOUR )
				&& ( m_vecMoveGoal - m_hTargetEnt->pev->origin ).Length() > TARGET_REPATH_DIST )
			{
				m_vecMoveGoal = m_hTargetEnt->pev->origin;
				m_movementGoal = MOVEGOAL_TARGETENT;
				FRefreshRoute();
			}

			// Set the appropriate activity based on an overlapping range
			// overlap the range to prevent oscillation
			if ( distance < pTask->flData )
			{
				TaskComplete();
				RouteClear();		// Stop moving
			}
			else if ( FClassnameIs( pev, "monster_human_medic_ally" ) && m_hHealTarget != NULL )
				m_movementActivity = ACT_RUN;
			else if ( distance < 190 && m_movementActivity != ACT_WALK )
				m_movementActivity = ACT_WALK;
			else if ( distance >= 270 && m_movementActivity != ACT_RUN )
			{
				if ( LookupActivity( ACT_RUN ) != ACTIVITY_NOT_AVAILABLE )
					m_movementActivity = ACT_RUN;
				else
					m_movementActivity = ACT_WALK;
			}

			break;
		}
	case TASK_MOVE_TO_TARGET_RANGE_FORCE:
		{
			if ( m_hTargetEnt == NULL )
			{
				TaskFail();
				break;
			}

			if ( ShouldStopPlayerFollow() )
			{
				StopPlayerFollowing( false, true );
				TaskFail();
				break;
			}

			CBaseEntity *pTarget = m_hTargetEnt;
			float distance = ( m_vecMoveGoal - pev->origin ).Length2D();
			float flReach = pTarget->pev->size.y * 0.5 + pev->size.y * 0.5;

			// bodies wider than the range can't get within it
			if ( flReach >= pTask->flData )
				distance = distance - fabs( pTask->flData - flReach ) - 18;

			if ( pTask->flData > distance || ( m_vecMoveGoal - pTarget->pev->origin ).Length2D() > pTask->flData * 0.5 )
			{
				m_vecMoveGoal = pTarget->pev->origin;
				distance = ( m_vecMoveGoal - pev->origin ).Length2D();
				FRefreshRoute();
			}

			if ( pTask->flData > distance )
			{
				TaskComplete();
				RouteClear();		// Stop moving
			}
			else if ( m_movementActivity != ACT_RUN )
			{
				if ( LookupActivity( ACT_RUN ) != ACTIVITY_NOT_AVAILABLE )
					m_movementActivity = ACT_RUN;
				else
					m_movementActivity = ACT_WALK;
			}

			break;
		}
	case TASK_WALK_PLAYERPATH_FOR_UNITS:
		{
			if ( ( m_vecLastPosition - pev->origin ).Length2D() > pTask->flData || MovementIsComplete() )
			{
				TaskComplete();
				RouteClear();		// Stop moving
			}

			break;
		}
	case TASK_FIND_COVER_FROM_BEST_SOUND:
		{
			if ( m_pDangerSound == NULL || m_pDangerSound->m_hOwner == NULL
				|| ( m_pDangerSound->m_hOwner->pev->effects & EF_NODRAW ) )
			{
				TaskComplete();
				break;
			}

			CBaseEntity *pOwner = m_pDangerSound->m_hOwner;

			if ( m_flWaitFinished > gpGlobals->time )
			{
				if ( !FVisible( pOwner->pev->origin ) )
					break;

				SetTurnActivity();
				MakeIdealYaw( pOwner->pev->origin );
				ChangeYaw( pev->yaw_speed );
				break;
			}

			if ( !FacingIdeal() && FVisible( pOwner->pev->origin ) )
			{
				SetTurnActivity();
				ChangeYaw( pev->yaw_speed );
				break;
			}

			if ( !FindLateralSafetyGrenade( m_pDangerSound->m_vecOrigin )
				&& !FindLateralCover( m_pDangerSound->m_vecOrigin, g_vecZero )
				&& !FindCoverGrenade( m_pDangerSound->m_vecOrigin, g_vecZero, m_pDangerSound->m_iVolume, CoverRadius() ) )
			{
				TaskFail();
				m_pDangerSound = NULL;
				break;
			}

			TaskComplete();
			break;
		}
	case TASK_WAIT_FOR_MOVEMENT_GRENADE:
		{
			if ( m_pDangerSound == NULL || m_pDangerSound->m_hOwner == NULL )
			{
				TaskComplete();
				RouteClear();		// Stop moving
				break;
			}

			if ( DangerSoundExploded( m_pDangerSound ) )
			{
				m_pDangerSound = NULL;
				TaskComplete();
				RouteClear();		// Stop moving
				break;
			}

			bool fDone = false;

			if ( HasMemory( bits_MEMORY_MOVE_RADIUS_ONLY ) && m_moveradius != 0
				&& ( m_vecMoveGoal - pev->origin ).Length2D() <= m_moveradius )
			{
				fDone = true;
			}

			if ( fDone || MovementIsComplete() || gpGlobals->time >= m_flGrenadeMoveTime )
			{
				TaskComplete();
				RouteClear();		// Stop moving
			}

			break;
		}
	case TASK_GLIDE_TO_GROUND:
		{
			if ( ( pev->flags & FL_ONGROUND ) || pev->groundentity != NULL )
			{
				TaskComplete();
				break;
			}

			if ( pev->velocity.z > 0 || m_fSequenceFinished )
				TaskComplete();

			break;
		}
	case TASK_WAIT_FOR_MOVEMENT_AP:
		{
			bool fDone = false;

			if ( m_hEnemy != NULL )
			{
			if ( pTask->flData == AP_WAIT_UNTIL_TARGETED )
				{
					// stop once the enemy is after someone else
					CBaseMonster *pEnemyMonster = m_hEnemy->MyMonsterPointer();

					if ( pEnemyMonster && pEnemyMonster->m_hEnemy != NULL && pEnemyMonster->m_hEnemy != this )
						fDone = true;
				}
				else if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK2 ) )
				{
					// stop once we can attack from here
					fDone = true;
				}
			}

			if ( !fDone && HasMemory( bits_MEMORY_MOVE_RADIUS_ONLY ) && m_moveradius != 0
				&& ( m_vecMoveGoal - pev->origin ).Length2D() <= m_moveradius )
			{
				fDone = true;
			}

			if ( fDone || MovementIsComplete())
			{
				TaskComplete();
				RouteClear();		// Stop moving
			}
			break;
		}
	case TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED:
		{
			bool fDone = false;

			if ( HasMemory( bits_MEMORY_INCOVER ) )
			{
				// started in cover: come out to fight once the enemy shows, or to reload once it's gone
				if ( ( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions( bits_COND_NO_AMMO_LOADED ) )
					|| HasAllConditions( bits_COND_ENEMY_OCCLUDED | bits_COND_NO_AMMO_LOADED ) )
				{
					Forget( bits_MEMORY_INCOVER );
					fDone = true;
				}
			}
			else if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) )
			{
				// out of its sight, and it can't reach us any other way
				CBaseMonster *pEnemyMonster;

				if ( m_hEnemy != NULL )
					pEnemyMonster = m_hEnemy->MyMonsterPointer();
				else
					pEnemyMonster = NULL;

				if ( pEnemyMonster == NULL || !( pEnemyMonster->m_afConditions & bits_COND_CAN_ATTACK ) )
					fDone = true;
			}

			if ( fDone )
			{
				TaskComplete();
				RouteClear();		// Stop moving
				break;
			}
		}
		// otherwise wait like TASK_WAIT_FOR_MOVEMENT
	case TASK_WAIT_FOR_MOVEMENT:
		{
			bool fDone = MovementIsComplete();

			// a scripted move (flData) ends inside the sequence's moveto_radius,
			// or next to a player blocking the way there
			if ( !fDone && pTask->flData != 0 && m_flMoveToRadius != 0 )
			{
				if ( ( m_vecMoveGoal - pev->origin ).Length() < m_flMoveToRadius )
				{
					fDone = true;
				}
				else if ( m_hCineBlocker != NULL
					&& ( m_hCineBlocker->pev->origin - m_vecMoveGoal ).Length() < m_flMoveToRadius
					&& ( pev->origin - m_hCineBlocker->pev->origin ).Length2D() <= pev->size.y * 0.5 + CINE_BLOCKER_MARGIN )
				{
					fDone = true;
				}
			}

			// a blocked move that stopped within its move radius
			if ( !fDone && HasMemory( bits_MEMORY_MOVE_RADIUS_ONLY ) && m_moveradius != 0
				&& ( m_vecMoveGoal - pev->origin ).Length2D() <= m_moveradius )
			{
				fDone = true;
			}

			if ( fDone )
			{
				TaskComplete();
				RouteClear();		// Stop moving
			}
			break;
		}
	case TASK_DIE:
		{
			if ( m_fBodyDropEvent )
				UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 1 ) );

			if ( m_fSequenceFinished && pev->frame >= 255 )
			{
				pev->deadflag = DEAD_DEAD;

				SetThink ( NULL );
				StopAnimation();

				if ( ShouldFadeOnDeath() )
				{
					m_flMaximumFadeWait = gpGlobals->time + 10.0f;
					pev->nextthink = gpGlobals->time + 0.1;
					SetThink( &CBaseEntity::SUB_StartFadeOut );
				}
				else
				{
					pev->avelocity = g_vecZero;
					pev->solid = SOLID_NOT;
					UTIL_SetOrigin( pev, pev->origin );

					if ( pev->flags & FL_MONSTER )
					{
						pev->movetype = MOVETYPE_TOSS;
						FloatInWater();
						AlignToGround();
						m_fDeathKnockback = false;
					}
				}

				// body is gonna be around for a while, so have it stink for a bit.
				CSoundEnt::InsertSound ( bits_SOUND_CARCASS, pev->origin, 384, 30, this );
				}
			else
			{
				FloatInWater();
			}
			break;
		}
	case TASK_RANGE_ATTACK1_NOTURN:
	case TASK_MELEE_ATTACK1_NOTURN:
	case TASK_MELEE_ATTACK2_NOTURN:
	case TASK_RANGE_ATTACK2_NOTURN:
	case TASK_RELOAD_NOTURN:
		{
			if ( m_fSequenceFinished )
			{
				if ( !m_fSequenceLoops )
					m_Activity = ACT_RESET;

				TaskComplete();
			}
			break;
		}
	case TASK_RANGE_ATTACK1:
	case TASK_MELEE_ATTACK1:
	case TASK_MELEE_ATTACK2:
	case TASK_RANGE_ATTACK2:
	case TASK_SPECIAL_ATTACK1:
	case TASK_SPECIAL_ATTACK2:
	case TASK_RELOAD:
		{
			MakeIdealYaw ( m_vecEnemyLKP );
			ChangeYaw ( pev->yaw_speed );

			if ( m_fSequenceFinished )
			{
				if ( !m_fSequenceLoops )
				{
					m_IdealActivity = ACT_RESET;
					m_Activity = ACT_RESET;
				}

				TaskComplete();
			}
			else if ( m_flAutomaticAttackTime > 0 && gpGlobals->time > m_flAutomaticAttackTime )
			{
				m_Activity = ACT_RESET;
				m_fSequenceLoops = FALSE;
				m_flAutomaticAttackTime = 0;
				TaskComplete();
			}

			break;
		}
	case TASK_SMALL_FLINCH:
		{
			if ( m_fSequenceFinished )
			{
				TaskComplete();
			}
		}
		break;
	case TASK_WAIT_FOR_SCRIPT:
		{
			if ( m_pCine->m_iDelay <= 0 && gpGlobals->time >= m_pCine->m_startTime )
			{
				TaskComplete();
				m_pCine->StartSequence( (CBaseMonster *)this, m_pCine->m_iszPlay, TRUE );
				if ( m_fSequenceFinished )
					ClearSchedule();
				pev->framerate = 1.0;
				//ALERT( at_aiconsole, "Script %s has begun for %s\n", STRING( m_pCine->m_iszPlay ), STRING(pev->classname) );
			}
			break;
		}
	case TASK_PLAY_SCRIPT:
		{
			if (m_fSequenceFinished)
			{
				m_pCine->SequenceDone( this );
			}
			break;
		}
	}
}

//=========================================================
// SetTurnActivity - measures the difference between the way
// the monster is facing and determines whether or not to
// select one of the 180 turn animations.
//=========================================================
void CBaseMonster :: SetTurnActivity ( void )
{
	float flYD;
	flYD = FlYawDiff();

	if ( flYD <= -45 && LookupActivity ( ACT_TURN_RIGHT ) != ACTIVITY_NOT_AVAILABLE )
	{// big right turn
		m_IdealActivity = ACT_TURN_RIGHT;
	}
	else if ( flYD > 45 && LookupActivity ( ACT_TURN_LEFT ) != ACTIVITY_NOT_AVAILABLE )
	{// big left turn
		m_IdealActivity = ACT_TURN_LEFT;
	}

	if ( !FClassnameIs( pev, "monster_op4loader" ) && LookupActivity( m_IdealActivity ) == ACTIVITY_NOT_AVAILABLE )
	{
		m_IdealActivity = ACT_IDLE_ANGRY;

		if ( LookupActivity( m_IdealActivity ) == ACTIVITY_NOT_AVAILABLE )
			m_IdealActivity = ACT_IDLE;
	}
}

//=========================================================
// Start task - selects the correct activity and performs
// any necessary calculations to start the next task on the
// schedule. 
//=========================================================
#define COVER_DISTANCE_MIN			108
#define COVER_DISTANCE_MAX			1024
#define COVER_DISTANCE_GARG_MIN		512
#define COVER_DISTANCE_GARG_MAX		2048

void CBaseMonster :: StartTask ( Task_t *pTask )
{
	if ( StartPathWaypointTask( pTask ) )
		return;

	switch ( pTask->iTask )
	{
	case TASK_FIND_TANK:
		{
			if ( TankTaskEntity( m_hSelectedTank ) )
				TaskComplete();
			else
				TaskFail();

			break;
		}
	case TASK_GET_PATH_TO_TANK:
		{
			CFuncTank *tank = static_cast<CFuncTank *>( TankTaskEntity( m_hSelectedTank ) );

			if ( !tank )
			{
				TaskFail();
				break;
			}

			UTIL_MakeVectors( Vector( 0, tank->pev->angles.y, 0 ) );

			Vector goal = tank->Center();

			goal = goal - gpGlobals->v_forward * ( tank->m_flUseRadius + pev->size.y );

			if ( ( goal - GetGunPosition() ).Length() <= pev->size.y )
			{
				TaskComplete();
				break;
			}

			m_moveradius = 16;

			if ( BuildRoute( goal, bits_MF_TO_LOCATION, tank ) )
				TaskComplete();
			else
			{
				ALERT( at_aiconsole, "GetPathToTank failed!!\n" );
				m_flNextTankSearchTime = gpGlobals->time + 5;
				TaskFail();
			}

			break;
		}
	case TASK_USE_TANK:
		{
			CFuncTank *tank = static_cast<CFuncTank *>( TankTaskEntity( m_hSelectedTank ) );

			if ( !tank )
			{
				TaskComplete();
				break;
			}

			CBaseEntity *enemy = TankTaskEntity( m_hEnemy.m_Handle );

			if ( !enemy || !enemy->IsAlive() || !FVisible( enemy, true ) )
			{
				m_hEnemy = BestVisibleEnemy();
				enemy = TankTaskEntity( m_hEnemy.m_Handle );
			}

			if ( !enemy )
			{
				TaskComplete();
				break;
			}

			CBaseEntity *controller = TankTaskEntity( tank->m_hMonsterController );

			if ( controller && controller != this )
			{
				m_hSelectedTank = NULL;
				m_flNextTankSearchTime = gpGlobals->time + 5;
				TaskComplete();
				break;
			}

			tank->MonsterStartup( this );
			MakeIdealYaw( m_vecEnemyLKP );
			ChangeYaw( pev->yaw_speed );
			break;
		}
	case TASK_RUN_TO_TANK:
		{
			CFuncTank *tank = static_cast<CFuncTank *>( TankTaskEntity( m_hSelectedTank ) );

			if ( !tank )
			{
				TaskFail();
				break;
			}

			CBaseEntity *controller = TankTaskEntity( tank->m_hMonsterController );

			if ( ( controller && controller != this ) || TankTaskEntity( tank->m_hController ) )
			{
				TaskFail();
				m_flNextTankSearchTime = gpGlobals->time + 5;
				break;
			}

			UTIL_MakeVectors( Vector( 0, tank->pev->angles.y, 0 ) );

			Vector goal = tank->Center();

			goal = goal - gpGlobals->v_forward * ( tank->m_flUseRadius + pev->size.y );

			if ( ( goal - pev->origin + GetGunPosition() ).Length() <= pev->size.y )
			{
				TaskComplete();
				break;
			}

			if ( FRouteClear() )
			{
				TaskFail();
				break;
			}

			if ( LookupActivity( ACT_RUN ) != ACTIVITY_NOT_AVAILABLE )
				m_movementActivity = ACT_RUN;
			else
				m_movementActivity = ACT_WALK;

			break;
		}

	case TASK_TURN_RIGHT:
		{
			float flCurrentYaw;
			
			flCurrentYaw = UTIL_AngleMod( pev->angles.y );
			pev->ideal_yaw = UTIL_AngleMod( flCurrentYaw - pTask->flData );
			SetTurnActivity();
			break;
		}
	case TASK_TURN_LEFT:
		{
			float flCurrentYaw;
			
			flCurrentYaw = UTIL_AngleMod( pev->angles.y );
			pev->ideal_yaw = UTIL_AngleMod( flCurrentYaw + pTask->flData );
			SetTurnActivity();
			break;
		}
	case TASK_REMEMBER:
		{
			Remember ( (int)pTask->flData );
			TaskComplete();
			break;
		}
	case TASK_FORGET:
		{
			Forget ( (int)pTask->flData );
			TaskComplete();
			break;
		}
	case TASK_FIND_HINTNODE:
		{
			m_iHintNode = FindRoamNode();

			if ( m_iHintNode != NO_NODE )
			{
				TaskComplete();
			}
			else
			{
				TaskFail();
			}
			break;
		}
	case TASK_STORE_LASTPOSITION:
		{
			m_vecLastPosition = pev->origin;
			TaskComplete();
			break;
		}
	case TASK_CLEAR_LASTPOSITION:
		{
			m_vecLastPosition = g_vecZero;
			TaskComplete();
			break;
		}
	case TASK_CLEAR_HINTNODE:
		{
			m_iHintNode = NO_NODE;
			TaskComplete();
			break;
		}
	case TASK_STOP_MOVING:
		{
			if ( m_Activity == ACT_RUN_HURT || m_Activity == ACT_RUN || m_Activity == ACT_WALK_HURT || m_Activity == ACT_WALK )
			{
				m_IdealActivity = GetStoppedActivity();
			}

			SetGaitActivity( m_IdealActivity );
			Stop();

			pev->gaitsequence = GAIT_SEQUENCE_NONE;
			m_GaitActivity = ACT_RESET;
			ResetGaitSequenceInfo();

			RouteClear();
			TaskComplete();
			break;
		}
	case TASK_PLAY_SEQUENCE_FACE_ENEMY:
	case TASK_PLAY_SEQUENCE_FACE_TARGET:
	case TASK_PLAY_SEQUENCE:
		{
			m_IdealActivity = ( Activity )( int )pTask->flData;
			break;
		}
	case TASK_PLAY_SEQUENCE_COWER:
		{
			if ( m_pDangerSound != NULL && DangerSoundExploded( m_pDangerSound ) )
			{
				m_pDangerSound = NULL;
				TaskComplete();
				break;
			}

			m_IdealActivity = ( Activity )( int )pTask->flData;
			break;
		}
	case TASK_PLAY_ACTIVE_IDLE:
		{
			// monsters verify that they have a sequence for the node's activity BEFORE
			// moving towards the node, so it's ok to just set the activity without checking here.
			m_IdealActivity = ( Activity )WorldGraph.m_pNodes[ m_iHintNode ].m_sHintActivity;
			break;
		}
	case TASK_SET_SCHEDULE:
		{
			Schedule_t *pNewSchedule;

			pNewSchedule = GetScheduleOfType( (int)pTask->flData );
			
			if ( pNewSchedule )
			{
				ChangeSchedule( pNewSchedule );
			}
			else
			{
				TaskFail();
			}

			break;
		}
	case TASK_FIND_NEAR_NODE_COVER_FROM_ENEMY:
		{
			if ( m_hEnemy == NULL )
			{
				TaskFail();
				return;
			}

			if ( FindCover( m_hEnemy->pev->origin, m_hEnemy->pev->view_ofs, 0, pTask->flData ) )
			{
				// try for cover farther than the FLData from the schedule.
				TaskComplete();
			}
			else
			{
				// no coverwhatsoever.
				TaskFail();
			}
			break;
		}
	case TASK_FIND_FAR_NODE_COVER_FROM_ENEMY:
		{
			if ( m_hEnemy == NULL )
			{
				TaskFail();
				return;
			}

			if ( FindCover( m_hEnemy->pev->origin, m_hEnemy->pev->view_ofs, pTask->flData, CoverRadius() ) )
			{
				// try for cover farther than the FLData from the schedule.
				TaskComplete();
			}
			else
			{
				// no coverwhatsoever.
				TaskFail();
			}
			break;
		}
	case TASK_FIND_NODE_COVER_FROM_ENEMY:
		{
			if ( m_hEnemy == NULL )
			{
				TaskFail();
				return;
			}

			if ( FindCover( m_hEnemy->pev->origin, m_hEnemy->pev->view_ofs, 0, CoverRadius() ) )
			{
				// try for cover farther than the FLData from the schedule.
				TaskComplete();
			}
			else
			{
				// no coverwhatsoever.
				TaskFail();
			}
			break;
		}
	case TASK_FIND_COVER_FROM_ENEMY:
		{
			CBaseEntity *pCover;

			if ( m_hEnemy == NULL )
			{
				// Find cover from self if no enemy available
				pCover = this;
//				TaskFail();
//				return;
			}
			else
				pCover = m_hEnemy;

			if ( FindLateralCover( pCover->Center(), pCover->pev->view_ofs ) )
			{
				// try lateral first
				m_flMoveWaitFinished = gpGlobals->time + pTask->flData;
				TaskComplete();
			}
			else if ( FindCover( pCover->Center(), pCover->pev->view_ofs, COVER_ENEMY_MIN_DIST, CoverRadius() )
				|| FindCoverDistance( pCover->Center(), pCover->pev->view_ofs, COVER_DISTANCE_MIN, COVER_DISTANCE_MAX )
				|| FindLateralSafety( pCover->Center(), COVER_DISTANCE_MIN, COVER_DISTANCE_MAX ) )
			{
				// then try for plain ole cover
				m_flMoveWaitFinished = gpGlobals->time + pTask->flData;
				TaskComplete();
			}
			else
			{
				// no coverwhatsoever.
				TaskFail();
			}
			break;
		}
	case TASK_FIND_ATTACKPOINT_TO_ENEMY:
		{
			if ( m_hEnemy == NULL )
			{
				TaskFail();
				break;
			}

			// flData 1: a monster without a range attack (or a gargantua) that already sees the enemy needn't move
			if ( pTask->flData == 1 && ( !( m_afCapability & ( bits_CAP_RANGE_ATTACK1 | bits_CAP_RANGE_ATTACK2 ) )
				|| FClassnameIs( pev, "monster_gargantua" ) ) && HasConditions( bits_COND_SEE_ENEMY ) )
			{
				TaskFail();
				break;
			}

			if ( FindLateralAttackPoint( m_hEnemy->Center(), m_hEnemy->pev->view_ofs )
				|| FindAttackPoint( m_hEnemy->Center(), m_hEnemy->pev->view_ofs, 0, CoverRadius() ) )
			{
				m_flMoveWaitFinished = gpGlobals->time + pTask->flData;
				TaskComplete();
			}
			else
			{
				TaskFail();
			}
			break;
		}
	case TASK_FIND_COVER_DISTANCE_FROM_ENEMY:
		{
			entvars_t *pevThreat;

			if ( m_hEnemy != NULL )
				pevThreat = m_hEnemy->pev;
			else
				pevThreat = pev;

			float flMinDist = COVER_DISTANCE_MIN;
			float flMaxDist = COVER_DISTANCE_MAX;

			// a gargantua reaches much farther
			if ( FClassnameIs( pevThreat, "monster_gargantua" ) )
			{
				flMinDist = COVER_DISTANCE_GARG_MIN;
				flMaxDist = COVER_DISTANCE_GARG_MAX;
			}

			if ( FindCoverDistance( pevThreat->origin, pevThreat->view_ofs, flMinDist, flMaxDist )
				|| FindLateralSafety( pevThreat->origin, flMinDist, flMaxDist )
				|| FindCover( pevThreat->origin, pevThreat->view_ofs, 0, CoverRadius() ) )
			{
				m_flMoveWaitFinished = gpGlobals->time;
				TaskComplete();
			}
			else
			{
				// no coverwhatsoever.
				TaskFail();
			}
			break;
		}
	case TASK_FIND_COVER_FROM_ORIGIN:
		{
			if ( FindCover( pev->origin, pev->view_ofs, 0, CoverRadius() ) )
			{
				// then try for plain ole cover
				m_flMoveWaitFinished = gpGlobals->time + pTask->flData;
				TaskComplete();
			}
			else if ( FindLateralSafety( pev->origin, COVER_ORIGIN_SAFETY_MIN, COVER_DISTANCE_MAX ) )
			{
				m_flMoveWaitFinished = gpGlobals->time;
				TaskComplete();
			}
			else
			{
				// no cover!
				TaskFail();
			}
		}
		break;
	case TASK_FIND_COVER_FROM_BEST_SOUND:
		{
			CSound *pBestSound = PBestSound();

			if ( pBestSound == NULL || pBestSound->m_hOwner == NULL )
			{
				TaskFail();
				m_pDangerSound = NULL;
				break;
			}

			if ( m_afCapability & bits_CAP_TURN_HEAD )
			{
				float yaw = VecToYaw( pBestSound->m_vecOrigin - pev->origin ) - pev->angles.y;

				if ( yaw > 180 )
					yaw -= 360;

				if ( yaw < -180 )
					yaw += 360;

				SetBoneController( 0, yaw );
			}

			m_pDangerSound = pBestSound;
			m_flWaitFinished = gpGlobals->time;

			CBaseEntity *pOwner = pBestSound->m_hOwner;

			if ( FClassnameIs( pOwner->pev, "grenade" ) || FClassnameIs( pOwner->pev, "rpg_rocket" ) )
			{
				if ( ( pev->origin - pOwner->pev->origin ).Length() < THREAT_SOUND_NEAR_DIST || !FInViewCone( &pOwner->pev->origin ) )
				{
					if ( m_MonsterState == MONSTERSTATE_COMBAT || m_MonsterState == MONSTERSTATE_ALERT )
						m_flWaitFinished = gpGlobals->time + RANDOM_FLOAT( 0.1, 0.5 );
					else
						m_flWaitFinished = gpGlobals->time + RANDOM_FLOAT( 0.5, 1 );
				}
			}

			m_IdealActivity = ACT_IDLE_ANGRY;

			if ( LookupActivity( ACT_IDLE_ANGRY ) == ACTIVITY_NOT_AVAILABLE )
				m_IdealActivity = ACT_IDLE;

			break;
		}
	case TASK_FACE_HINTNODE:
		{
			pev->ideal_yaw = WorldGraph.m_pNodes[ m_iHintNode ].m_flHintYaw;
			SetTurnActivity();
			break;
		}
	
	case TASK_FACE_LASTPOSITION:
		MakeIdealYaw ( m_vecLastPosition );
		SetTurnActivity(); 
		break;

	case TASK_FACE_TARGET:
		if ( m_hTargetEnt != NULL )
		{
			if ( m_hEnemy != NULL && m_hGuardEnt != NULL )
			{
				MakeIdealYaw ( m_vecEnemyLKP );
				SetTurnActivity();
			}
			else if ( ShouldStopPlayerFollow() )
			{
				StopPlayerFollowing( false, true );
				TaskFail();
			}
			else
			{
				MakeIdealYaw ( m_hTargetEnt->pev->origin );
				SetTurnActivity();
			}
		}
		else
			TaskFail();
		break;
	case TASK_FACE_ENEMY:
		{
			MakeIdealYaw ( m_vecEnemyLKP );
			SetTurnActivity(); 
			break;
		}
	case TASK_FACE_IDEAL:
		{
			SetTurnActivity();
			break;
		}
	case TASK_FACE_ROUTE:
		{
			if (FRouteClear())
			{
				ALERT(at_aiconsole, "No route to face!\n");
				TaskFail();
			}
			else
			{
				MakeIdealYaw(m_Route[m_iRouteIndex].vecLocation);
				SetTurnActivity();
			}
			break;
		}
	case TASK_WAIT_PVS:
	case TASK_WAIT_INDEFINITE:
		{
			// don't do anything.
			break;
		}
	case TASK_WAIT:
	case TASK_WAIT_FACE_ENEMY:
		{// set a future time that tells us when the wait is over.
			m_flWaitFinished = gpGlobals->time + pTask->flData;	
			break;
		}
	case TASK_WAIT_RANDOM:
		{// set a future time that tells us when the wait is over.
			m_flWaitFinished = gpGlobals->time + RANDOM_FLOAT( 0.1, pTask->flData );
			break;
		}
	case TASK_MOVE_TO_TARGET_RANGE:
		{
			if ( m_hTargetEnt == NULL )
			{
				TaskFail();
				break;
			}

			if ( ShouldStopPlayerFollow() )
			{
				StopPlayerFollowing( false, true );
				TaskFail();
				break;
			}

			m_vecMoveGoal = m_hTargetEnt->Center();

			if ( ( m_vecMoveGoal - pev->origin ).Length2D() < 1 )
				TaskComplete();
			else if ( !MoveToTarget( ACT_WALK, 2, TARGET_RANGE_MOVE_RADIUS ) )
				TaskFail();

			break;
		}
	case TASK_MOVE_TO_TARGET_RANGE_FORCE:
		{
			if ( m_hTargetEnt == NULL )
			{
				TaskFail();
				break;
			}

			if ( ( m_hTargetEnt->pev->origin - pev->origin ).Length2D() < 1 )
			{
				TaskComplete();
				break;
			}

			m_vecMoveGoal = m_hTargetEnt->pev->origin;

			if ( MoveToTarget( m_movementActivity, 2, TARGET_RANGE_FORCE_MOVE_RADIUS ) )
			{
				if ( LookupActivity( ACT_RUN ) != ACTIVITY_NOT_AVAILABLE )
					m_movementActivity = ACT_RUN;
				else
					m_movementActivity = ACT_WALK;
			}
			else
			{
				ALERT( at_aiconsole, "%s Failed to reach target(Force)!\n", STRING( pev->classname ) );
				TaskFail();
			}

			break;
		}
	case TASK_WALK_PLAYERPATH_FOR_UNITS:
		{
			m_movementActivity = ACT_WALK;
			break;
		}
	case TASK_MOVE_AWAY_PLAYERPATH:
		{
			ALERT( at_aiconsole, "Trying Move Away Path...\n" );

			Vector vecAngles( pev->angles.x, pev->ideal_yaw + 180, pev->angles.z );
			Vector vecForward;

			AngleVectors( vecAngles, &vecForward, NULL, NULL );

			if ( MoveToLocation( ACT_WALK, 2, pev->origin + vecForward * pTask->flData ) )
			{
				TaskComplete();
			}
			else if ( FindCover( pev->origin, pev->view_ofs, 0, CoverRadius() ) )
			{
				m_flMoveWaitFinished = gpGlobals->time + MOVE_AWAY_COVER_WAIT;
				TaskComplete();
			}
			else
			{
				ALERT( at_aiconsole, "Task failed\n" );
				TaskFail();
			}

			break;
		}
	case TASK_GET_PATH_TO_BESTCOMBATSOUND:
		{
			CSound *pSound = PBestSound();

			if ( pSound && MoveToLocation( m_movementActivity, 2, pSound->m_vecOrigin, COMBAT_SOUND_APPROACH ) )
			{
				TaskComplete();
			}
			else
			{
				// no way to get there =(
				ALERT ( at_aiconsole, "GetPathToBestCombatSound failed!!\n" );
				TaskFail();
			}
			break;
		}
	case TASK_WAIT_FOR_MOVEMENT_GRENADE:
		{
			m_flMoveWaitFinished = 0;

			if ( FRouteClear() )
				TaskComplete();

			if ( m_pDangerSound != NULL && DangerSoundExploded( m_pDangerSound ) )
			{
				m_pDangerSound = NULL;
				TaskComplete();
				RouteClear();		// Stop moving
			}

			if ( m_pDangerSound == NULL || m_pDangerSound->m_hOwner == NULL )
			{
				TaskComplete();
				RouteClear();		// Stop moving
			}

			m_flGrenadeMoveTime = gpGlobals->time + GRENADE_MOVE_TIME;
			break;
		}
	case TASK_GLIDE_TO_GROUND:
		{
			m_IdealActivity = ACT_GLIDE;
			break;
		}
	case TASK_RUN_TO_TARGET:
	case TASK_WALK_TO_TARGET:
		{
			Activity newActivity;

			if ( pTask->iTask == TASK_WALK_TO_TARGET )
				newActivity = ACT_WALK;
			else
				newActivity = ACT_RUN;
			// This monster can't do this!
			if ( LookupActivity( newActivity ) == ACTIVITY_NOT_AVAILABLE )
				TaskComplete();
			else
			{
				if ( m_hTargetEnt == NULL || !MoveToTarget( newActivity, 2 ) )
				{
					TaskFail();
					ALERT( at_aiconsole, "%s Failed to reach target!!!\n", STRING(pev->classname) );
					RouteClear();
					break;
				}
			}
			TaskComplete();
			break;
		}
	case TASK_CLEAR_MOVE_WAIT:
		{
			m_flMoveWaitFinished = gpGlobals->time;
			TaskComplete();
			break;
		}
	case TASK_MELEE_ATTACK1_NOTURN:
	case TASK_MELEE_ATTACK1:
		{
			m_IdealActivity = ACT_MELEE_ATTACK1;
			break;
		}
	case TASK_MELEE_ATTACK2_NOTURN:
	case TASK_MELEE_ATTACK2:
		{
			m_IdealActivity = ACT_MELEE_ATTACK2;
			break;
		}
	case TASK_RANGE_ATTACK1_NOTURN:
	case TASK_RANGE_ATTACK1:
		{
			m_IdealActivity = ACT_RANGE_ATTACK1;
			break;
		}
	case TASK_RANGE_ATTACK2_NOTURN:
	case TASK_RANGE_ATTACK2:
		{
			m_IdealActivity = ACT_RANGE_ATTACK2;
			break;
		}
	case TASK_RELOAD_NOTURN:
	case TASK_RELOAD:
		{
			m_IdealActivity = ACT_RELOAD;
			break;
		}
	case TASK_SPECIAL_ATTACK1:
		{
			m_IdealActivity = ACT_SPECIAL_ATTACK1;
			break;
		}
	case TASK_SPECIAL_ATTACK2:
		{
			m_IdealActivity = ACT_SPECIAL_ATTACK2;
			break;
		}
	case TASK_SET_ACTIVITY:
		{
			m_IdealActivity = (Activity)(int)pTask->flData;
			TaskComplete();
			break;
		}
	case TASK_GET_PATH_TO_ENEMY_LKP:
		{
			if ( BuildRoute ( m_vecEnemyLKP, bits_MF_TO_LOCATION, NULL ) )
			{
				TaskComplete();
			}
			else if (BuildNearestRoute( m_vecEnemyLKP, pev->view_ofs, 0, (m_vecEnemyLKP - pev->origin).Length() ))
			{
				TaskComplete();
			}
			else
			{
				// no way to get there =(
				ALERT ( at_aiconsole, "GetPathToEnemyLKP failed!!\n" );
				TaskFail();
			}
			break;
		}
	case TASK_GET_PATH_TO_ENEMY:
		{
			CBaseEntity *pEnemy = m_hEnemy;

			if ( pEnemy == NULL )
			{
				TaskFail();
				return;
			}

			if ( BuildRoute ( pEnemy->Center(), bits_MF_TO_ENEMY, pEnemy ) )
			{
				TaskComplete();
			}
			else if (BuildNearestRoute( pEnemy->Center(), pEnemy->pev->view_ofs, 0, (pEnemy->pev->origin - pev->origin).Length() ))
			{
				TaskComplete();
			}
			else if ( BuildRoute ( m_vecEnemyLKP, bits_MF_TO_LOCATION, NULL ) )
			{
				TaskComplete();
			}
			else if (BuildNearestRoute( m_vecEnemyLKP, pev->view_ofs, 0, (m_vecEnemyLKP - pev->origin).Length() ))
			{
				TaskComplete();
			}
			else
			{
				// no way to get there =(
				ALERT ( at_aiconsole, "GetPathToEnemy failed!!\n" );
				TaskFail();
			}
			break;
		}
	case TASK_GET_PATH_TO_ENEMY_CORPSE:
		{
			UTIL_MakeVectors( pev->angles );

			if ( FClassnameIs( pev, "monster_pitdrone" ) )
			{
				if ( BuildRoute ( m_vecEnemyLKP, bits_MF_TO_LOCATION, NULL ) || ( m_vecEnemyLKP - pev->origin ).Length2D() < CORPSE_REACH_DIST )
					TaskComplete();
				else
				{
					ALERT ( at_aiconsole, "GetPathToEnemyCorpse failed!!\n" );
					TaskFail();
				}
			}
			else if ( BuildRoute ( m_vecEnemyLKP - gpGlobals->v_forward * 64, bits_MF_TO_LOCATION, NULL ) )
			{
				TaskComplete();
			}
			else
			{
				ALERT ( at_aiconsole, "GetPathToEnemyCorpse failed!!\n" );
				TaskFail();
			}
		}
		break;
	case TASK_GET_PATH_TO_SPOT:
		{
			CBaseEntity *pPlayer = CBaseEntity::Instance( FIND_ENTITY_BY_CLASSNAME( NULL, "player" ) );
			if ( BuildRoute ( m_vecMoveGoal, bits_MF_TO_LOCATION, pPlayer ) )
			{
				TaskComplete();
			}
			else
			{
				// no way to get there =(
				ALERT ( at_aiconsole, "GetPathToSpot failed!!\n" );
				TaskFail();
			}
			break;
		}

	case TASK_GET_PATH_TO_TARGET:
		{
			RouteClear();
			if ( m_hTargetEnt != NULL && MoveToTarget( m_movementActivity, 1 ) )
			{
				TaskComplete();
			}
			else
			{
				// no way to get there =(
				ALERT ( at_aiconsole, "GetPathToSpot failed!!\n" );
				TaskFail();
			}
			break;
		}
	case TASK_GET_PATH_TO_HINTNODE:// for active idles!
		{
			if ( m_iHintNode == NO_NODE )
			{
				TaskFail();
				break;
			}

			if ( MoveToLocation( m_movementActivity, 2, WorldGraph.m_pNodes[ m_iHintNode ].m_vecOrigin ) )
			{
				TaskComplete();
				break;
			}

			// no way to get there =(, so settle for somewhere beside it
			bool fFound = false;

			for ( int i = 0; i < WorldGraph.m_pNodes[ m_iHintNode ].m_cNumLinks; i++ )
			{
				Vector vecLinked = WorldGraph.m_pNodes[ WorldGraph.INodeLink( m_iHintNode, i ) ].m_vecOrigin;

				if ( MoveToLocation( m_movementActivity, 2, vecLinked, HINT_LINK_MOVE_RADIUS ) )
				{
					fFound = true;
					break;
				}
			}

			if ( fFound )
				TaskComplete();
			else
				TaskFail();

			break;
		}
	case TASK_GET_PATH_TO_LASTPOSITION:
		{
			m_vecMoveGoal = m_vecLastPosition;

			if ( MoveToLocation( m_movementActivity, 2, m_vecMoveGoal ) )
			{
				TaskComplete();
			}
			else
			{
				// no way to get there =(
				ALERT ( at_aiconsole, "GetPathToLastPosition failed!!\n" );
				TaskFail();
			}
			break;
		}
	case TASK_GET_PATH_TO_BESTSOUND:
		{
			CSound *pSound;

			pSound = PBestSound();

			if ( pSound && MoveToLocation( m_movementActivity, 2, pSound->m_vecOrigin ) )
			{
				TaskComplete();
			}
			else
			{
				// no way to get there =(
				ALERT ( at_aiconsole, "GetPathToBestSound failed!!\n" );
				TaskFail();
			}
			break;
		}
case TASK_GET_PATH_TO_BESTSCENT:
		{
			CSound *pScent;

			pScent = PBestScent();

			if ( pScent && MoveToLocation( m_movementActivity, 2, pScent->m_vecOrigin ) )
			{
				TaskComplete();
			}
			else
			{
				// no way to get there =(
				ALERT ( at_aiconsole, "GetPathToBestScent failed!!\n" );
				
				TaskFail();
			}
			break;
		}
	case TASK_RUN_PATH:
		{
			// UNDONE: This is in some default AI and some monsters can't run? -- walk instead?
			if ( LookupActivity( ACT_RUN ) != ACTIVITY_NOT_AVAILABLE )
			{
				m_movementActivity = ACT_RUN;
			}
			else
			{
				m_movementActivity = ACT_WALK;
			}
			TaskComplete();
			break;
		}
	case TASK_WALK_PATH:
		{
			if ( pev->movetype == MOVETYPE_FLY )
			{
				m_movementActivity = ACT_FLY;
			}
			if ( LookupActivity( ACT_WALK ) != ACTIVITY_NOT_AVAILABLE )
			{
				m_movementActivity = ACT_WALK;
			}
			else
			{
				m_movementActivity = ACT_RUN;
			}
			TaskComplete();
			break;
		}
	case TASK_STRAFE_PATH:
		{
			Vector2D	vec2DirToPoint; 
			Vector2D	vec2RightSide;

			// to start strafing, we have to first figure out if the target is on the left side or right side
			UTIL_MakeVectors ( pev->angles );

			vec2DirToPoint = ( m_Route[ m_iRouteIndex ].vecLocation - pev->origin ).Make2D().Normalize();
			vec2RightSide = gpGlobals->v_right.Make2D().Normalize();

			if ( DotProduct ( vec2DirToPoint, vec2RightSide ) > 0 )
			{
				// strafe right
				m_movementActivity = ACT_STRAFE_RIGHT;
			}
			else
			{
				// strafe left
				m_movementActivity = ACT_STRAFE_LEFT;
			}
			TaskComplete();
			break;
		}


	case TASK_WAIT_FOR_MOVEMENT:
		{
			m_flMoveWaitFinished = 0;

			if (FRouteClear())
			{
				TaskComplete();
			}
			break;
		}

	case TASK_WAIT_FOR_MOVEMENT_AP:
	case TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED:
		{
			// note whether we start out of the enemy's sight
			if ( m_hEnemy != NULL && !FVisible( m_hEnemy, true ) )
				SetConditions( bits_COND_ENEMY_OCCLUDED );

			ClearConditions( bits_COND_SEE_HATE );

			if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) )
				Remember( bits_MEMORY_INCOVER );

			m_flMoveWaitFinished = 0;

			if ( FRouteClear() )
				TaskComplete();

			break;
		}

	case TASK_EAT:
		{
			Eat( pTask->flData );
			TaskComplete();
			break;
		}
	case TASK_SMALL_FLINCH:
		{
			m_IdealActivity = GetSmallFlinchActivity();
			break;
		}
	case TASK_DIE:
		{
			RouteClear();	
			
			m_IdealActivity = GetDeathActivity();

			pev->deadflag = DEAD_DYING;
			break;
		}
	case TASK_SOUND_WAKE:
		{
			AlertSound();
			TaskComplete();
			break;
		}
	case TASK_SOUND_DIE:
		{
			DeathSound();
			TaskComplete();
			break;
		}
	case TASK_SOUND_IDLE:
		{
			IdleSound();
			TaskComplete();
			break;
		}
	case TASK_SOUND_PAIN:
		{
			PainSound();
			TaskComplete();
			break;
		}
	case TASK_SOUND_DEATH:
		{
			DeathSound();
			TaskComplete();
			break;
		}
	case TASK_SOUND_ANGRY:
		{
			// sounds are complete as soon as we get here, cause we've already played them.
			ALERT ( at_aiconsole, "SOUND\n" );			
			TaskComplete();
			break;
		}
	case TASK_WAIT_FOR_SCRIPT:
		{
			if ( !m_pCine )
			{
				TaskFail();
				break;
			}

			if ( m_pCine->m_fMoveTo == SCRIPT_MOVETO_INSTANT )
			{
				TraceResult tr;

				UTIL_TraceLine( pev->origin, pev->origin - Vector( 0, 0, pev->size.z ), ignore_monsters, edict(), &tr );

				if ( tr.flFraction == 1.0 )
					pev->movetype = MOVETYPE_FLY;
			}

			if (m_pCine->m_iszIdle)
			{
				m_pCine->StartSequence( (CBaseMonster *)this, m_pCine->m_iszIdle, FALSE );
				if (FStrEq( STRING(m_pCine->m_iszIdle), STRING(m_pCine->m_iszPlay)))
				{
					pev->framerate = 0;
				}
			}
			else
				m_IdealActivity = ACT_IDLE;

			break;
		}
	case TASK_PLAY_SCRIPT:
		{
			pev->movetype = MOVETYPE_FLY;
			ClearBits(pev->flags, FL_ONGROUND);
			m_scriptState = SCRIPT_PLAYING;
			break;
		}
	case TASK_ENABLE_SCRIPT:
		{
			m_pCine->DelayStart( 0 );
			TaskComplete();
			break;
		}
	case TASK_PLANT_ON_SCRIPT:
		{
			if ( m_pCine && m_flMoveToRadius == 0 )
			{
				pev->origin = m_pCine->pev->origin;	// Plant on target
			}

			TaskComplete();
			break;
		}
	case TASK_FACE_SCRIPT:
		{
			if ( m_pCine )
			{
				pev->ideal_yaw = UTIL_AngleMod( m_pCine->pev->angles.y );
			}

			TaskComplete();
			m_IdealActivity = ACT_IDLE;
			RouteClear();
			break;
		}

	case TASK_SUGGEST_STATE:
		{
			m_IdealMonsterState = (MONSTERSTATE)(int)pTask->flData;
			TaskComplete();
			break;
		}

	case TASK_SET_FAIL_SCHEDULE:
		m_failSchedule = (int)pTask->flData;
		TaskComplete();
		break;

	case TASK_CLEAR_FAIL_SCHEDULE:
		m_failSchedule = SCHED_NONE;
		TaskComplete();
		break;

	default:
		{
			ALERT ( at_aiconsole, "No StartTask entry for %d\n", (SHARED_TASKS)pTask->iTask );
			break;
		}
	}
}

//=========================================================
// GetTask - returns a pointer to the current 
// scheduled task. NULL if there's a problem.
//=========================================================
Task_t	*CBaseMonster :: GetTask ( void ) 
{
	if ( m_iScheduleIndex < 0 || m_iScheduleIndex >= m_pSchedule->cTasks )
	{
		// m_iScheduleIndex is not within valid range for the monster's current schedule.
		return NULL;
	}
	else
	{
		return &m_pSchedule->pTasklist[ m_iScheduleIndex ];
	}
}

//=========================================================
// GetSchedule - Decides which type of schedule best suits
// the monster's current state and conditions. Then calls
// monster's member function to get a pointer to a schedule
// of the proper type.
//=========================================================
Schedule_t *CBaseMonster :: GetSchedule ( void )
{
	CFuncTank *tank = static_cast<CFuncTank *>( TankTaskEntity( m_hSelectedTank ) );

	if ( tank )
	{
		if ( TankTaskEntity( tank->m_hMonsterController ) == this )
			tank->MonsterStopControl();

		m_hSelectedTank = NULL;
	}

	switch	( m_MonsterState )
	{
	case MONSTERSTATE_PRONE:
		{
			return GetScheduleOfType( SCHED_BARNACLE_VICTIM_GRAB );
			break;
		}
	case MONSTERSTATE_NONE:
		{
			ALERT ( at_aiconsole, "MONSTERSTATE IS NONE!\n" );
			break;
		}
	case MONSTERSTATE_IDLE:
		{
			if ( m_hPathWaypoint != NULL )
				return GetScheduleOfType( SCHED_PATH_WAYPOINT );

			if ( m_hGuardEnt != NULL )
			{
				FollowGuardEnt( this );

				if ( m_hTargetEnt != NULL && ( Center() - m_hTargetEnt->Center() ).Length2D() > GUARD_FOLLOW_DIST )
					return GetScheduleOfType( SCHED_TARGET_PLAYERCHASE_FORCE );
			}

			if ( HasConditions ( bits_COND_HEAR_SOUND ) && !FClassnameIs( pev, "monster_furniture" ) )
			{
				return GetScheduleOfType( SCHED_ALERT_FACE );
			}

			if ( m_hEnemy == NULL && IsPlayerFollowing() )
			{
				if ( !m_hTargetEnt->IsAlive() )
				{
					StopPlayerFollowing( false, false );
					return GetScheduleOfType( SCHED_IDLE_STAND );
				}

				if ( HasConditions( bits_COND_CLIENT_PUSH ) )
					return GetScheduleOfType( SCHED_MOVE_AWAY_PLAYERFOLLOW );

				return GetScheduleOfType( SCHED_TARGET_PLAYERFACE );
			}

			if ( FRouteClear() )
			{
				// no valid route!
				return GetScheduleOfType( SCHED_IDLE_STAND );
			}
			else
			{
				// valid route. Get moving
				return GetScheduleOfType( SCHED_IDLE_WALK );
			}
			break;
		}
	case MONSTERSTATE_ALERT:
		{
			if ( m_hPathWaypoint != NULL )
				return GetScheduleOfType( SCHED_PATH_WAYPOINT );

			if ( m_hGuardEnt != NULL )
			{
				FollowGuardEnt( this );

				if ( m_hTargetEnt != NULL && ( Center() - m_hTargetEnt->Center() ).Length2D() > GUARD_FOLLOW_DIST )
					return GetScheduleOfType( SCHED_TARGET_PLAYERCHASE_FORCE );
			}

			if ( ( m_hEnemy != NULL && HasConditions( bits_COND_SEE_ENEMY ) ) || !IsPlayerFollowing() )
			{
				if ( HasConditions( bits_COND_ENEMY_DEAD ) && LookupActivity( ACT_VICTORY_DANCE ) != ACTIVITY_NOT_AVAILABLE )
				{
					return GetScheduleOfType ( SCHED_VICTORY_DANCE );
				}

				if ( !HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
				{
					if ( !HasConditions( bits_COND_HEAR_SOUND ) || FClassnameIs( pev, "monster_furniture" ) )
						return GetScheduleOfType( SCHED_ALERT_STAND );

					if ( ( pev->spawnflags & SF_MONSTER_PRISONER ) || gpGlobals->time < m_useTime )
						return GetScheduleOfType( SCHED_ALERT_FACE );

					CSound *pSound = PBestSound();

					if ( pSound == NULL || !( pSound->m_iType & bits_SOUND_COMBAT ) || pSound->m_hOwner == NULL )
						return GetScheduleOfType( SCHED_ALERT_FACE );

					if ( IRelationship( pSound->m_hOwner, false ) > R_NO || RANDOM_LONG( 0, 1 ) == 0 )
						return GetScheduleOfType( SCHED_ALERT_FACE );

					return GetScheduleOfType( SCHED_INVESTIGATE_COMBAT );
				}
			}
			else if ( !HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
			{
				if ( !m_hTargetEnt->IsAlive() || m_hTargetEnt->pev->deadflag != DEAD_NO )
				{
					StopPlayerFollowing( false, false );
					return GetScheduleOfType( SCHED_ALERT_STAND );
				}

				if ( HasConditions( bits_COND_CLIENT_PUSH ) )
					return GetScheduleOfType( SCHED_MOVE_AWAY_PLAYERFOLLOW );

				return GetScheduleOfType( SCHED_TARGET_PLAYERFACE );
			}

			if ( fabs( FlYawDiff() ) < (1.0 - m_flFieldOfView) * 60 ) // roughly in the correct direction
			{
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ORIGIN );
			}
			else
			{
				return GetScheduleOfType( SCHED_ALERT_SMALL_FLINCH );
			}
			break;
		}
	case MONSTERSTATE_COMBAT:
		{
			if ( m_hPathWaypoint != NULL )
				return GetScheduleOfType( SCHED_PATH_WAYPOINT );

			if ( m_hGuardEnt != NULL )
			{
				FollowGuardEnt( this );

				if ( m_hTargetEnt != NULL )
				{
					float flGuardDist = ( Center() - m_hTargetEnt->Center() ).Length2D();

					if ( flGuardDist > GUARD_COMBAT_DIST )
					{
						if ( m_hEnemy == NULL )
							return GetScheduleOfType( SCHED_TARGET_PLAYERCHASE_FORCE );

						CBaseMonster *pGuarded = m_hTargetEnt->MyMonsterPointer();

						if ( pGuarded != NULL && pGuarded->m_hEnemy != NULL )
						{
							if ( ( pGuarded->m_hEnemy->Center() - m_hTargetEnt->Center() ).Length() < GUARD_COMBAT_DIST && flGuardDist > GUARD_RECALL_DIST )
								return GetScheduleOfType( SCHED_TARGET_PLAYERCHASE_FORCE );

							Vector vecEnemyCenter = m_hEnemy->Center();

							PushEnemy( m_hEnemy, vecEnemyCenter );
							m_hEnemy = pGuarded->m_hEnemy;
						}

						if ( !FVisible( m_hTargetEnt, true ) )
							return GetScheduleOfType( SCHED_TARGET_PLAYERCHASE_FORCE );
					}
				}
			}

			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
			{
				// clear the current (dead) enemy and try to find another.
				m_hEnemy = NULL;

				if ( GetEnemy() )
				{
					ClearConditions( bits_COND_ENEMY_DEAD );
					return GetSchedule();
				}
				else
				{
					m_hEnemy = NULL;
					SetState( MONSTERSTATE_ALERT );
					return GetSchedule();
				}
			}

			if ( HasConditions(bits_COND_NEW_ENEMY) )
			{
				MakeIdealYaw( m_vecEnemyLKP );
				return GetScheduleOfType ( SCHED_WAKE_ANGRY );
			}

			if ( HasConditions(bits_COND_LIGHT_DAMAGE) && !HasMemory( bits_MEMORY_FLINCHED) )
			{
				return GetScheduleOfType( SCHED_SMALL_FLINCH );
			}

			if ( HasConditions( bits_COND_HEAVY_DAMAGE ) )
			{
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
			}

			if ( !HasConditions( bits_COND_SEE_FEAR | bits_COND_SEE_ENEMY ) )
			{
				// we can't see the enemy
				if ( HasConditions( bits_COND_ENEMY_OCCLUDED | bits_COND_ENEMY_TOOFAR ) )
				{
					// chase!
					return GetScheduleOfType( SCHED_CHASE_ENEMY );
				}
				else
				{
					// enemy is unseen, but not occluded!
					// turn to face enemy
					return GetScheduleOfType( SCHED_COMBAT_FACE );
				}
			}

			// what we fear has noticed us
			if ( HasConditions( bits_COND_SEE_FEAR ) && m_hEnemy != NULL )
			{
				CBaseMonster *pEnemyMonster = m_hEnemy->MyMonsterPointer();

				if ( pEnemyMonster != NULL )
				{
					if ( pEnemyMonster->pev->flags & FL_CLIENT )
					{
						if ( pEnemyMonster->IsFacing( pev, ENEMY_FACING_DOT ) )
							return GetScheduleOfType( SCHED_LOSE_ENEMY );
					}
					else if ( pEnemyMonster->HasConditions( bits_COND_SEE_ENEMY ) && pEnemyMonster->m_hEnemy != NULL && (CBaseEntity *) pEnemyMonster->m_hEnemy == this )
					{
						return GetScheduleOfType( SCHED_LOSE_ENEMY );
					}
				}
			}

			// we can see the enemy
			if ( HasConditions(bits_COND_CAN_RANGE_ATTACK1) )
			{
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
			}
			if ( HasConditions(bits_COND_CAN_RANGE_ATTACK2) )
			{
				return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
			}
			if ( HasConditions(bits_COND_CAN_MELEE_ATTACK1) )
			{
				return GetScheduleOfType( SCHED_MELEE_ATTACK1 );
			}
			if ( HasConditions(bits_COND_CAN_MELEE_ATTACK2) )
			{
				return GetScheduleOfType( SCHED_MELEE_ATTACK2 );
			}

			if ( FacingIdeal() )
			{
				// if we can see enemy but can't use either attack type, we must need to get closer to enemy
				return GetScheduleOfType( SCHED_CHASE_ENEMY );
			}

			//turn
			return GetScheduleOfType( SCHED_COMBAT_FACE );
		}
	case MONSTERSTATE_DEAD:
		{
			return GetScheduleOfType( SCHED_DIE );
			break;
		}
	case MONSTERSTATE_SCRIPT:
		{
			ASSERT( m_pCine != NULL );
			if ( !m_pCine )
			{
				ALERT( at_aiconsole, "Script failed for %s\n", STRING(pev->classname) );
				CineCleanup();
				return GetScheduleOfType( SCHED_IDLE_STAND );
			}

			return GetScheduleOfType( SCHED_AISCRIPT );
		}
	default:
		{
			ALERT ( at_aiconsole, "Invalid State for GetSchedule!\n" );
			break;
		}
	}

	return &slError[ 0 ];
}
