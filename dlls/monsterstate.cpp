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
// monsterstate.cpp - base class monster functions for 
// controlling core AI.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "nodes.h"
#include "monsters.h"
#include "animation.h"
#include "saverestore.h"
#include "soundent.h"
#include "game.h"

//=========================================================
// SetState
//=========================================================
void CBaseMonster :: SetState ( MONSTERSTATE State )
{
/*
	if ( State != m_MonsterState )
	{
		ALERT ( at_aiconsole, "State Changed to %d\n", State );
	}
*/
	
	switch( State )
	{
	
	// Drop enemy pointers when going to idle
	case MONSTERSTATE_IDLE:

		if ( m_hEnemy != NULL )
		{
			m_hEnemy = NULL;// not allowed to have an enemy anymore.
			ALERT ( at_aiconsole, "Stripped\n" );
		}
		break;
	}

	m_MonsterState = State;
	m_IdealMonsterState = State;
}

//=========================================================
// RunAI
//=========================================================
#define FALL_DAMAGE_DELAY	3	// seconds after first touching the ground
#define CORPSE_ALIGN_MAX_SPEED	10

void CBaseMonster :: RunAI ( void )
{
	// to test model's eye height
	//UTIL_ParticleEffect ( pev->origin + pev->view_ofs, g_vecZero, 255, 10 );

	if ( pev->deadflag == DEAD_DEAD )
		m_fDeathKnockback = false;

	// falls only hurt once the monster has stood on something
	if ( !m_fFallDamageReady )
	{
		TraceResult tr;

		UTIL_TraceHull( pev->origin, pev->origin - Vector( 0, 0, pev->size.z * 0.5 ), ignore_monsters, human_hull, ENT( pev ), &tr );

		if ( tr.pHit )
			m_fFallDamageReady = true;

		m_flFallDamageTime = gpGlobals->time + FALL_DAMAGE_DELAY;
	}

	// a monster told to guard an entity follows it
	if ( m_hGuardEnt == NULL && m_iszGuardEntName )
	{
		m_hGuardEnt = UTIL_FindEntityByTargetname( NULL, STRING( m_iszGuardEntName ) );

		if ( m_hGuardEnt != NULL && !IsPlayerFollowing() )
			StartPlayerFollowing( m_hGuardEnt, false );
	}

	// IDLE sound permitted in ALERT state is because monsters were silent in ALERT state. Only play IDLE sound in IDLE state
	// once we have sounds for that state.
	if ( ( m_MonsterState == MONSTERSTATE_IDLE || m_MonsterState == MONSTERSTATE_ALERT ) && RANDOM_LONG(0,99) == 0 && !(pev->spawnflags & SF_MONSTER_GAG) )
	{
		IdleSound();
	}

	// a resting corpse follows the ground
	if ( m_MonsterState == MONSTERSTATE_DEAD
		&& ( ( pev->flags & FL_ONGROUND ) || pev->velocity.Length() <= CORPSE_ALIGN_MAX_SPEED ) )
	{
		AlignToGround();
	}

	if ( m_MonsterState != MONSTERSTATE_NONE	&& 
		 m_MonsterState != MONSTERSTATE_PRONE   && 
		 m_MonsterState != MONSTERSTATE_DEAD )// don't bother with this crap if monster is prone. 
	{
		// collect some sensory Condition information.
		// don't let monsters outside of the player's PVS act up, or most of the interesting
		// things will happen before the player gets there!
		// UPDATE: We now let COMBAT state monsters think and act fully outside of player PVS. This allows the player to leave 
		// an area where monsters are fighting, and the fight will continue.
		Listen();// check for audible sounds.

		if ( sv_ai_enemy_detection_mode.value > 0 || ( m_MonsterState == MONSTERSTATE_COMBAT ) || ( m_MonsterState == MONSTERSTATE_SCRIPT )
			|| !FNullEnt( FIND_CLIENT_IN_PVS( edict() ) ) )
		{
			Look( m_flDistLook );

			// now filter conditions.
			ClearConditions( IgnoreConditions() );

			GetEnemy();
		}

		// do these calculations if monster has an enemy.
		if ( m_hEnemy != NULL )
		{
			CheckEnemy( m_hEnemy );
		}

		CheckAmmo();

		CheckCreatureDanger();

		CheckSelectedTank();

		EvaluatePaths();
	}

	if ( m_MonsterState == MONSTERSTATE_DEAD )
		CheckRevival();

	if ( m_fShockGlow && gpGlobals->time > m_flShockGlowTime )
		ShockGlowEffect( false );

	CheckTimeBasedDamage();

	CheckFallDamage();

	FCheckAITrigger();

	PrescheduleThink();

	MaintainSchedule();

	if ( !( pev->flags & FL_ONGROUND ) )
		m_flFallVelocity = -pev->velocity.z;

	// if the monster didn't use these conditions during the above call to MaintainSchedule() or CheckAITrigger()
	// we throw them out cause we don't want them sitting around through the lifespan of a schedule
	// that doesn't use them. 
	m_afConditions &= ~( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE );
}

//=========================================================
// GetIdealState - surveys the Conditions information available
// and finds the best new state for a monster.
//=========================================================
MONSTERSTATE CBaseMonster :: GetIdealState ( void )
{
	int	iConditions;

	iConditions = IScheduleFlags();
	
	// If no schedule conditions, the new ideal state is probably the reason we're in here.
	switch ( m_MonsterState )
	{
	case MONSTERSTATE_IDLE:
		
		/*
		IDLE goes to ALERT upon hearing a sound
		-IDLE goes to ALERT upon being injured
		IDLE goes to ALERT upon seeing food
		-IDLE goes to COMBAT upon sighting an enemy
		IDLE goes to HUNT upon smelling food
		*/
		{
			if ( pev->spawnflags & SF_MONSTER_PRISONER )
				break;

			if ( iConditions & bits_COND_NEW_ENEMY )			
			{
				// new enemy! This means an idle monster has seen someone it dislikes, or 
				// that a monster in combat has found a more suitable target to attack
				m_IdealMonsterState = MONSTERSTATE_COMBAT;
			}
			else if ( iConditions & ( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
			{
				MakeIdealYaw ( m_vecEnemyLKP );
				m_IdealMonsterState = MONSTERSTATE_ALERT;
			}
			else if ( iConditions & bits_COND_HEAR_SOUND )
			{
				CSound *pSound = PBestSound();

				if ( !pSound )
					break;

				FaceSound( pSound, gpGlobals->time >= m_useTime );

				if ( pSound->m_iType & (bits_SOUND_COMBAT|bits_SOUND_DANGER) )
					m_IdealMonsterState = MONSTERSTATE_ALERT;
			}
			else if ( iConditions & (bits_COND_SMELL | bits_COND_SMELL_FOOD) )
			{
				m_IdealMonsterState = MONSTERSTATE_ALERT;
			}

			break;
		}
	case MONSTERSTATE_ALERT:
		/*
		ALERT goes to IDLE upon becoming bored
		-ALERT goes to COMBAT upon sighting an enemy
		ALERT goes to HUNT upon hearing a noise
		*/
		{
			if ( iConditions & (bits_COND_NEW_ENEMY|bits_COND_SEE_ENEMY) )			
			{
				// see an enemy we MUST attack
				m_IdealMonsterState = MONSTERSTATE_COMBAT;
			}
			else if ( iConditions & bits_COND_HEAR_SOUND )
			{
				CSound *pSound = PBestSound();

				if ( !pSound )
					break;

				FaceSound( pSound, ( pSound->m_iType & (bits_SOUND_COMBAT|bits_SOUND_DANGER) ) != 0 );
				m_IdealMonsterState = MONSTERSTATE_ALERT;
			}
			break;
		}
	case MONSTERSTATE_COMBAT:
		/*
		COMBAT goes to HUNT upon losing sight of enemy
		COMBAT goes to ALERT upon death of enemy
		*/
		{
			if ( m_hEnemy == NULL )
				m_IdealMonsterState = MONSTERSTATE_ALERT;
			break;
		}
	case MONSTERSTATE_HUNT:
		/*
		HUNT goes to ALERT upon seeing food
		HUNT goes to ALERT upon being injured
		HUNT goes to IDLE if goal touched
		HUNT goes to COMBAT upon seeing enemy
		*/
		{
			break;
		}
	case MONSTERSTATE_SCRIPT:
		if ( iConditions & (bits_COND_TASK_FAILED|bits_COND_LIGHT_DAMAGE|bits_COND_HEAVY_DAMAGE) )
		{
			ExitScriptedSequence();	// This will set the ideal state
		}
		break;

	case MONSTERSTATE_DEAD:
		if ( m_IdealMonsterState != MONSTERSTATE_PRONE )
			m_IdealMonsterState = MONSTERSTATE_DEAD;
		break;
	}

	return m_IdealMonsterState;
}

//=========================================================
// FaceSound - turn toward a sound or follow a friend's look direction
//=========================================================
#define FACE_SOUND_LOOK_DIST	2048

void CBaseMonster :: FaceSound( CSound *pSound, bool fMayFollowLook )
{
	CBaseEntity *pOwner = pSound->m_hOwner;

	if ( fMayFollowLook && pOwner && IRelationship( pOwner, false ) <= R_NO && RANDOM_LONG( 0, 1 ) )
	{
		UTIL_MakeVectors( pOwner->pev->angles );
		MakeIdealYaw( pOwner->pev->origin + gpGlobals->v_forward * FACE_SOUND_LOOK_DIST );
	}
	else
	{
		MakeIdealYaw( pSound->m_vecOrigin );
	}
}
