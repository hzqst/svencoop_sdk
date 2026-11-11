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
/*

===== monsters.cpp ========================================================

  Monster-related utility code

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "EntityRelationships.h"
#include "nodes.h"
#include "monsters.h"
#include "animation.h"
#include "saverestore.h"
#include "weapons.h"
#include "CClassicMode.h"
#include "scripted.h"
#include "squadmonster.h"
#include "squadtalkmonster.h"
#include "plane.h"
#include "decals.h"
#include "soundent.h"
#include "gamerules.h"
#include "game.h"
#include "player.h"
#include "server_support.h"
#include "func_tank.h"
#include "attackvectors.h"

#define MONSTER_CUT_CORNER_DIST		12


Vector VecBModelOrigin( entvars_t* pevBModel );

extern DLL_GLOBAL	BOOL	g_fDrawLines;
extern DLL_GLOBAL	short	g_sModelIndexLaser;// holds the index for the laser beam
extern DLL_GLOBAL	short	g_sModelIndexLaserDot;// holds the index for the laser beam dot

extern CGraph WorldGraph;// the world node graph



// Global Savedata for monster
// UNDONE: Save schedule data?  Can this be done?  We may
// lose our enemy pointer or other data (goal ent, target, etc)
// that make the current schedule invalid, perhaps it's best
// to just pick a new one when we start up again.
TYPEDESCRIPTION	CBaseMonster::m_SaveData[] = 
{
	DEFINE_FIELD( CBaseMonster, m_hEnemy, FIELD_EHANDLE ),
	DEFINE_FIELD( CBaseMonster, m_hTargetEnt, FIELD_EHANDLE ),
	DEFINE_FIELD( CBaseMonster, m_hHealTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( CBaseMonster, m_fReviving, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseMonster, m_hHealer, FIELD_EHANDLE ),
	DEFINE_FIELD( CBaseMonster, m_flNextMedicCallTime, FIELD_TIME ),
	DEFINE_FIELD( CBaseMonster, m_useTime, FIELD_TIME ),
	DEFINE_FIELD( CBaseMonster, m_fCanPlayerFollow, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseMonster, m_hGuardEnt, FIELD_EHANDLE ),
	DEFINE_FIELD( CBaseMonster, m_iszGuardEntName, FIELD_STRING ),
	DEFINE_FIELD( CBaseMonster, m_hCineBlocker, FIELD_EHANDLE ),
	DEFINE_FIELD( CBaseMonster, m_flMoveToRadius, FIELD_FLOAT ),
	DEFINE_ARRAY( CBaseMonster, m_hOldEnemy, FIELD_EHANDLE, MAX_OLD_ENEMIES ),
	DEFINE_ARRAY( CBaseMonster, m_vecOldEnemy, FIELD_POSITION_VECTOR, MAX_OLD_ENEMIES ),
	DEFINE_FIELD( CBaseMonster, m_flFieldOfView, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseMonster, m_flWaitFinished, FIELD_TIME ),
	DEFINE_FIELD( CBaseMonster, m_flMoveWaitFinished, FIELD_TIME ),

	DEFINE_FIELD( CBaseMonster, m_Activity, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseMonster, m_IdealActivity, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseMonster, m_LastHitGroup, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseMonster, m_MonsterState, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseMonster, m_IdealMonsterState, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseMonster, m_iTaskStatus, FIELD_INTEGER ),

	//Schedule_t			*m_pSchedule;

	DEFINE_FIELD( CBaseMonster, m_iScheduleIndex, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseMonster, m_afConditions, FIELD_INTEGER ),
	//WayPoint_t			m_Route[ ROUTE_SIZE ];
//	DEFINE_FIELD( CBaseMonster, m_movementGoal, FIELD_INTEGER ),
//	DEFINE_FIELD( CBaseMonster, m_iRouteIndex, FIELD_INTEGER ),
//	DEFINE_FIELD( CBaseMonster, m_moveWaitTime, FIELD_FLOAT ),

	DEFINE_FIELD( CBaseMonster, m_vecMoveGoal, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CBaseMonster, m_movementActivity, FIELD_INTEGER ),

	//		int					m_iAudibleList; // first index of a linked list of sounds that the monster can hear.
//	DEFINE_FIELD( CBaseMonster, m_afSoundTypes, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseMonster, m_vecLastPosition, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CBaseMonster, m_iHintNode, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseMonster, m_afMemory, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseMonster, m_iMaxHealth, FIELD_INTEGER ),

	DEFINE_FIELD( CBaseMonster, m_vecEnemyLKP, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CBaseMonster, m_cAmmoLoaded, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseMonster, m_afCapability, FIELD_INTEGER ),

	DEFINE_FIELD( CBaseMonster, m_flNextAttack, FIELD_TIME ),
	DEFINE_FIELD( CBaseMonster, m_bitsDamageType, FIELD_INTEGER ),
	DEFINE_ARRAY( CBaseMonster, m_rgbTimeBasedDamage, FIELD_CHARACTER, CDMG_TIMEBASED ),
	DEFINE_FIELD( CBaseMonster, m_bloodColor, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseMonster, m_failSchedule, FIELD_INTEGER ),

	DEFINE_FIELD( CBaseMonster, m_flHungryTime, FIELD_TIME ),
	DEFINE_FIELD( CBaseMonster, m_flDistTooFar, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseMonster, m_flDistLook, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseMonster, m_iTriggerCondition, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseMonster, m_iszTriggerTarget, FIELD_STRING ),

	DEFINE_FIELD( CBaseMonster, m_HackedGunPos, FIELD_VECTOR ),

	DEFINE_FIELD( CBaseMonster, m_scriptState, FIELD_INTEGER ),
	{ FIELD_EHANDLE, "m_hCine", offsetof( CBaseMonster, m_pCine ), 1, 0 },
};

//IMPLEMENT_SAVERESTORE( CBaseMonster, CBaseToggle );
int CBaseMonster::Save( CSave &save )
{
	if ( !CBaseToggle::Save(save) )
		return 0;
	return save.WriteFields( "CBaseMonster", this, m_SaveData, ARRAYSIZE(m_SaveData) );
}

int CBaseMonster::Restore( CRestore &restore )
{
	if ( !CBaseToggle::Restore(restore) )
		return 0;
	int status = restore.ReadFields( "CBaseMonster", this, m_SaveData, ARRAYSIZE(m_SaveData) );
	
	// We don't save/restore routes yet
	RouteClear();

	// We don't save/restore schedules yet
	m_pSchedule = NULL;
	m_iTaskStatus = TASKSTATUS_NEW;
	
	// Reset animation
	m_Activity = ACT_RESET;

	// If we don't have an enemy, clear conditions like see enemy, etc.
	if ( m_hEnemy == NULL )
		m_afConditions = 0;

	return status;
}


//=========================================================
// Eat - makes a monster full for a little while.
//=========================================================
void CBaseMonster :: Eat ( float flFullDuration )
{
	m_flHungryTime = gpGlobals->time + flFullDuration;
}

//=========================================================
// FShouldEat - returns true if a monster is hungry.
//=========================================================
BOOL CBaseMonster :: FShouldEat ( void )
{
	if ( m_flHungryTime > gpGlobals->time )
	{
		return FALSE;
	}

	return TRUE;
}

//=========================================================
// BarnacleVictimBitten - called
// by Barnacle victims when the barnacle pulls their head
// into its mouth
//=========================================================
void CBaseMonster :: BarnacleVictimBitten ( entvars_t *pevBarnacle, float flDamage )
{
	Schedule_t	*schedule = GetScheduleOfType( SCHED_BARNACLE_VICTIM_CHOMP );

	ChangeSchedule( schedule );
	TakeDamage( pevBarnacle, pevBarnacle, flDamage, DMG_SLASH | DMG_ALWAYSGIB );
	}

//=========================================================
// BarnacleVictimReleased - called by barnacle victims when
// the host barnacle is killed.
//=========================================================
void CBaseMonster :: BarnacleVictimReleased ( void )
{
	m_IdealMonsterState = MONSTERSTATE_IDLE;

	pev->velocity = g_vecZero;
	pev->movetype = MOVETYPE_STEP;
}

//=========================================================
// Listen - monsters dig through the active sound list for
// any sounds that may interest them. (smells, too!)
//=========================================================
void CBaseMonster :: Listen ( void )
{
	int		iSound;
	int		iMySounds;
	float	hearingSensitivity;
	CSound	*pCurrentSound;

	m_iAudibleList = SOUNDLIST_EMPTY; 
	ClearConditions(bits_COND_HEAR_SOUND | bits_COND_SMELL | bits_COND_SMELL_FOOD);
	m_afSoundTypes = 0;

	iMySounds = ISoundMask();

	if ( m_pSchedule )
	{
		//!!!WATCH THIS SPOT IF YOU ARE HAVING SOUND RELATED BUGS!
		// Make sure your schedule AND personal sound masks agree!
		iMySounds &= m_pSchedule->iSoundMask;
	}

	iSound = CSoundEnt::ActiveList();

	// UNDONE: Clear these here?
	ClearConditions( bits_COND_HEAR_SOUND | bits_COND_SMELL_FOOD | bits_COND_SMELL );
	hearingSensitivity = HearingSensitivity( );

	while ( iSound != SOUNDLIST_EMPTY )
	{
		pCurrentSound = CSoundEnt::SoundPointerForIndex( iSound );

		if ( pCurrentSound	&& 
			 ( pCurrentSound->m_iType & iMySounds )	&& 
			 ( pCurrentSound->m_vecOrigin - EarPosition() ).Length() <= pCurrentSound->m_iVolume * hearingSensitivity )

		//if ( ( g_pSoundEnt->m_SoundPool[ iSound ].m_iType & iMySounds ) && ( g_pSoundEnt->m_SoundPool[ iSound ].m_vecOrigin - EarPosition()).Length () <= g_pSoundEnt->m_SoundPool[ iSound ].m_iVolume * hearingSensitivity ) 
		{
 			// the monster cares about this sound, and it's close enough to hear.
			//g_pSoundEnt->m_SoundPool[ iSound ].m_iNextAudible = m_iAudibleList;
			pCurrentSound->m_iNextAudible = m_iAudibleList;
			
			if ( pCurrentSound->FIsSound() )
			{
				// this is an audible sound.
				SetConditions( bits_COND_HEAR_SOUND );
			}
			else
			{
				// if not a sound, must be a smell - determine if it's just a scent, or if it's a food scent
//				if ( g_pSoundEnt->m_SoundPool[ iSound ].m_iType & ( bits_SOUND_MEAT | bits_SOUND_CARCASS ) )
				if ( pCurrentSound->m_iType & ( bits_SOUND_MEAT | bits_SOUND_CARCASS ) )
				{
					// the detected scent is a food item, so set both conditions.
					// !!!BUGBUG - maybe a virtual function to determine whether or not the scent is food?
					SetConditions( bits_COND_SMELL_FOOD );
					SetConditions( bits_COND_SMELL );
				}
				else
				{
					// just a normal scent. 
					SetConditions( bits_COND_SMELL );
				}
			}

//			m_afSoundTypes |= g_pSoundEnt->m_SoundPool[ iSound ].m_iType;
			m_afSoundTypes |= pCurrentSound->m_iType;

			m_iAudibleList = iSound;
		}

//		iSound = g_pSoundEnt->m_SoundPool[ iSound ].m_iNext;
		iSound = pCurrentSound->m_iNext;
	}
}

//=========================================================
// FLSoundVolume - subtracts the volume of the given sound
// from the distance the sound source is from the caller, 
// and returns that value, which is considered to be the 'local' 
// volume of the sound. 
//=========================================================
float CBaseMonster :: FLSoundVolume ( CSound *pSound )
{
	return ( pSound->m_iVolume - ( ( pSound->m_vecOrigin - pev->origin ).Length() ) );
}

//=========================================================
// FValidateHintType - tells use whether or not the monster cares
// about the type of Hint Node given
//=========================================================
BOOL CBaseMonster :: FValidateHintType ( short sHint )
{
	return FALSE;
}

//=========================================================
// Look - Base class monster function to find enemies or
// food by sight. iDistance is distance ( in units ) that the
// monster can see.
//
// Sets the sight bits of the m_afConditions mask to indicate
// which types of entities were sighted.
// Function also sets the Looker's m_pLink to the head of a
// link list of the visible living ents and dead enemies
// (linked via each ent's m_pLink field), and its
// m_pSightLink to a list of everything visible, corpses
// included (linked via m_pSightLink)
//
//=========================================================
#define LOOK_MAX_ENTITIES	150
#define ENEMY_SEEN_MEMORY_TIME	5

void CBaseMonster :: Look ( int iDistance )
{
	int	iSighted = 0;

	// DON'T let visibility information from last frame sit around!
	ClearConditions(bits_COND_SEE_HATE | bits_COND_SEE_DISLIKE | bits_COND_SEE_ENEMY | bits_COND_SEE_FEAR | bits_COND_SEE_NEMESIS | bits_COND_SEE_CLIENT);

	m_pLink = NULL;
	m_pSightLink = NULL;// the current visible entity that we're dealing with

	// See no evil if prisoner is set
	if ( FBitSet( pev->spawnflags, SF_MONSTER_PRISONER ) || FClassnameIs( pev, "monster_furniture" ) )
		return;

	CBaseEntity *pList[LOOK_MAX_ENTITIES];

		Vector delta = Vector( iDistance, iDistance, iDistance );

		// Find only monsters/clients in box, NOT limited to PVS
		int count = UTIL_EntitiesInBox( pList, LOOK_MAX_ENTITIES, pev->origin - delta, pev->origin + delta, FL_CLIENT|FL_MONSTER );
		for ( int i = 0; i < count; i++ )
		{
		CBaseEntity *pSightEnt = pList[i];
			// !!!temporarily only considering other monsters and clients, don't see prisoners
			if ( pSightEnt == this || FBitSet( pSightEnt->pev->spawnflags, SF_MONSTER_PRISONER ) )
			continue;

		if ( !FInViewCone( pSightEnt ) || !FVisible( pSightEnt ) )
			continue;

		bool fLink = true;

		if ( !( pSightEnt->pev->health > 0 ) )
		{
			// a corpse only counts while it is the enemy or a remembered one
			fLink = pSightEnt == m_hEnemy;

			for ( int j = 0; j < MAX_OLD_ENEMIES && !fLink; j++ )
				fLink = pSightEnt == m_hOldEnemy[j];
		}
		else
		{
					if ( pSightEnt->IsPlayer() )
					{
						if ( pev->spawnflags & SF_MONSTER_WAIT_TILL_SEEN )
						{
							CBaseMonster *pClient = pSightEnt->MyMonsterPointer();
							// don't link this client in the list if the monster is wait till seen and the player isn't facing the monster
							if ( !pClient->FInViewCone( this ) )
						continue;

					// player sees us, become normal now.
					pev->spawnflags &= ~SF_MONSTER_WAIT_TILL_SEEN;
							}

				// if we see a client, remember that (mostly for scripted AI)
				iSighted |= bits_COND_SEE_CLIENT;
					}

			// we know this ent is visible, so if it also happens to be our enemy, store that now.
			if ( pSightEnt == m_hEnemy )
			{
				iSighted |= bits_COND_SEE_ENEMY;
				m_flEnemySeenTime = gpGlobals->time + ENEMY_SEEN_MEMORY_TIME;
			}
		}

		if ( fLink )
		{
			pSightEnt->m_pLink = m_pLink;
					m_pLink = pSightEnt;

			switch ( IRelationship ( pSightEnt ) )
					{
					case	R_NM:
						iSighted |= bits_COND_SEE_NEMESIS;
						break;
					case	R_HT:
						iSighted |= bits_COND_SEE_HATE;
						break;
					case	R_DL:
						iSighted |= bits_COND_SEE_DISLIKE;
						break;
					case	R_FR:
						iSighted |= bits_COND_SEE_FEAR;
						break;
			default:
				break;
					}
				}

		pSightEnt->m_pSightLink = m_pSightLink;
		m_pSightLink = pSightEnt;
	}

	SetConditions( iSighted );
}

//=========================================================
// ISoundMask - returns a bit mask indicating which types
// of sounds this monster regards. In the base class implementation,
// monsters care about all sounds, but no scents.
//=========================================================
int CBaseMonster :: ISoundMask ( void )
{
	return	bits_SOUND_WORLD	|
			bits_SOUND_COMBAT	|
			bits_SOUND_PLAYER;
}

//=========================================================
// PBestSound - returns a pointer to the sound the monster 
// should react to. Right now responds only to nearest sound.
//=========================================================
CSound* CBaseMonster :: PBestSound ( void )
{	
	int iThisSound; 
	int	iBestSound = -1;
	float flBestDist = 8192;// so first nearby sound will become best so far.
	float flDist;
	CSound *pSound;

	iThisSound = m_iAudibleList; 

	if ( iThisSound == SOUNDLIST_EMPTY )
	{
		ALERT ( at_aiconsole, "ERROR! monster %s has no audible sounds!\n", STRING(pev->classname) );
#if _DEBUG
		ALERT( at_error, "NULL Return from PBestSound\n" );
#endif
		return NULL;
	}

	while ( iThisSound != SOUNDLIST_EMPTY )
	{
		pSound = CSoundEnt::SoundPointerForIndex( iThisSound );

		if ( pSound && pSound->FIsSound() )
		{
			flDist = ( pSound->m_vecOrigin - EarPosition()).Length();

			if ( flDist < flBestDist )
			{
				iBestSound = iThisSound;
				flBestDist = flDist;
			}
		}

		iThisSound = pSound->m_iNextAudible;
	}
	if ( iBestSound >= 0 )
	{
		pSound = CSoundEnt::SoundPointerForIndex( iBestSound );
		return pSound;
	}
#if _DEBUG
	ALERT( at_error, "NULL Return from PBestSound\n" );
#endif
	return NULL;
}

//=========================================================
// PBestScent - returns a pointer to the scent the monster 
// should react to. Right now responds only to nearest scent
//=========================================================
CSound* CBaseMonster :: PBestScent ( void )
{	
	int iThisScent; 
	int	iBestScent = -1;
	float flBestDist = 8192;// so first nearby smell will become best so far.
	float flDist;
	CSound *pSound;

	iThisScent = m_iAudibleList;// smells are in the sound list.

	if ( iThisScent == SOUNDLIST_EMPTY )
	{
		ALERT ( at_aiconsole, "ERROR! PBestScent() has empty soundlist!\n" );
#if _DEBUG
		ALERT( at_error, "NULL Return from PBestSound\n" );
#endif
		return NULL;
	}

	while ( iThisScent != SOUNDLIST_EMPTY )
	{
		pSound = CSoundEnt::SoundPointerForIndex( iThisScent );

		if ( pSound->FIsScent() )
		{
			flDist = ( pSound->m_vecOrigin - pev->origin ).Length();

			if ( flDist < flBestDist )
			{
				iBestScent = iThisScent;
				flBestDist = flDist;
			}
		}

		iThisScent = pSound->m_iNextAudible;
	}
	if ( iBestScent >= 0 )
	{
		pSound = CSoundEnt::SoundPointerForIndex( iBestScent );

		return pSound;
	}
#if _DEBUG
	ALERT( at_error, "NULL Return from PBestScent\n" );
#endif
	return NULL;
}



//=========================================================
// Monster Think - calls out to core AI functions and handles this
// monster's specific animation events
//=========================================================
void CBaseMonster :: MonsterThink ( void )
{
	pev->nextthink = gpGlobals->time + m_flThinkInterval;
	m_flFrameTime = gpGlobals->time - m_flLastThinkTime;
	m_flLastThinkTime = gpGlobals->time;// keep monster thinking.


	RunAI();

	float flInterval = StudioFrameAdvance();

	DispatchAnimEvents( flInterval ); // animate
// start or end a fidget
// This needs a better home -- switching animations over time should be encapsulated on a per-activity basis
// perhaps MaintainActivity() or a ShiftAnimationOverTime() or something.
	if ( m_MonsterState != MONSTERSTATE_SCRIPT && m_MonsterState != MONSTERSTATE_DEAD && m_Activity == ACT_IDLE && m_fSequenceFinished )
		SetActivity( ACT_IDLE );

	if ( m_GaitActivity == m_movementActivity && flInterval == 0 )
		flInterval = pev->nextthink - gpGlobals->time;

	if ( m_movementGoal != MOVEGOAL_NONE )
		Move( flInterval );
	}

//=========================================================
// CBaseMonster - USE - will make a monster angry at whomever
// activated it.
//=========================================================
void CBaseMonster :: MonsterUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_IdealMonsterState = MONSTERSTATE_ALERT;
}

//=========================================================
// CheckFallDamage - a landing staggers the monster, and a
// fast one hurts it (mp_falldamage: -1 none, 1 realistic,
// otherwise a flat amount). It also crushes whatever
// monster or player it lands on
//=========================================================
#define MONSTER_MAX_SAFE_FALL_SPEED		580		// same as the player's
#define MONSTER_FATAL_FALL_SPEED		1024
#define MONSTER_FLAT_FALL_DAMAGE		10
#define MONSTER_FALL_CRUSH_SCALE		1.5

void CBaseMonster :: CheckFallDamage( void )
{
	if ( m_fFallDamageReady
		&& ( m_afCapability & bits_CAP_FALL_DAMAGE )
		&& gpGlobals->time > m_flFallDamageTime
		&& ( ( pev->flags & FL_ONGROUND ) || pev->groundentity != NULL )
		&& pev->health > 0
		&& m_flFallVelocity >= pev->size.z
		&& pev->watertype != CONTENTS_WATER
		&& m_MonsterState != MONSTERSTATE_SCRIPT )
	{
		bool fFlinch = false;

		if ( m_flFallVelocity <= MONSTER_MAX_SAFE_FALL_SPEED )
		{
			// a fighting monster doesn't stop for a safe landing, except a gargantua
			fFlinch = pev->deadflag == DEAD_NO && ( m_hEnemy == NULL || FClassnameIs( pev, "monster_gargantua" ) );
		}
		else
		{
			int iFallDamage = (int)CVAR_GET_FLOAT( "mp_falldamage" );
			int iDamage = 0;

			if ( iFallDamage == 1 )
			{
				m_flFallVelocity -= MONSTER_MAX_SAFE_FALL_SPEED;
				iDamage = (int)( m_flFallVelocity * 100.0 / ( MONSTER_FATAL_FALL_SPEED - MONSTER_MAX_SAFE_FALL_SPEED ) );
			}
			else if ( iFallDamage != -1 )
			{
				iDamage = MONSTER_FLAT_FALL_DAMAGE;
			}

			if ( iDamage > 0 )
			{
				entvars_t *pevWorld = VARS( INDEXENT( 0 ) );

				TakeDamage( pevWorld, pevWorld, iDamage, DMG_FALL );

				CBaseEntity *pGround = CBaseEntity::Instance( pev->groundentity );

				if ( pGround && ( pGround->pev->flags & ( FL_MONSTER | FL_CLIENT ) ) )
				{
					pGround->TakeDamage( pev, pev, MONSTER_FALL_CRUSH_SCALE * iDamage, DMG_CRUSH );

					pev->groundentity = NULL;
					pev->flags &= ~FL_ONGROUND;
					pev->velocity.z -= 1;
				}

				fFlinch = pev->deadflag == DEAD_NO && pev->health > 0;
			}
		}

		if ( fFlinch )
			ChangeSchedule( GetScheduleOfType( SCHED_SMALL_FLINCH_SPECIAL ) );
	}

	if ( pev->flags & FL_ONGROUND )
		m_flFallVelocity = 0;
}

//=========================================================
// CheckAndApplyGenericAttacks - play the attack over the movement gait
// Requires m_afMoveShootCap to attack while moving
//=========================================================
void CBaseMonster :: CheckAndApplyGenericAttacks( void )
{
	if ( m_flMoveWaitFinished > gpGlobals->time )
		return;

	if ( !m_afMoveShootCap )
	{
		m_IdealActivity = m_movementActivity;
		pev->gaitsequence = GAIT_SEQUENCE_NONE;
		return;
	}

	ClearConditions( bits_COND_CAN_ATTACK_MOVING );

	if ( m_hEnemy != NULL && m_hEnemy->IsAlive() )
		CheckAttacks_Move( m_hEnemy, ( m_hEnemy->pev->origin - pev->origin ).Length2D() );

	CheckAmmo();

	bool fAttacking;

	switch ( GetSequenceActivityName( pev->sequence ) )
	{
	case ACT_RANGE_ATTACK1:
		fAttacking = HasConditions( bits_COND_CAN_RANGE_ATTACK1_MOVING ) != FALSE;
		break;

	case ACT_RANGE_ATTACK2:
		fAttacking = HasConditions( bits_COND_CAN_RANGE_ATTACK2_MOVING ) != FALSE;
		break;

	case ACT_MELEE_ATTACK1:
		fAttacking = HasConditions( bits_COND_CAN_MELEE_ATTACK1_MOVING ) != FALSE;
		break;

	case ACT_MELEE_ATTACK2:
		fAttacking = HasConditions( bits_COND_CAN_MELEE_ATTACK2_MOVING ) != FALSE;
		break;

	case ACT_RELOAD:
		fAttacking = true;
		break;

	default:
		fAttacking = false;
		break;
	}

	// let an attack that still applies play out
	if ( fAttacking && !m_fSequenceFinished && m_Activity != ACT_RESET )
		return;

	if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1_MOVING ) )
	{
		m_IdealActivity = ACT_MELEE_ATTACK1;
	}
	else if ( HasConditions( bits_COND_CAN_MELEE_ATTACK2_MOVING ) )
	{
		m_IdealActivity = ACT_MELEE_ATTACK2;
	}
	else if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1_MOVING ) )
	{
		m_IdealActivity = ACT_RANGE_ATTACK1;
	}
	else if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2_MOVING ) )
	{
		m_IdealActivity = ACT_RANGE_ATTACK2;
	}
	else if ( HasConditions( bits_COND_NO_AMMO_LOADED ) )
	{
		m_IdealActivity = ACT_RELOAD;
	}
	else
	{
		// nothing to attack with: the whole body moves again
		pev->sequence = pev->gaitsequence;
		ResetSequenceInfo();

		m_Activity = m_movementActivity;
		m_IdealActivity = m_movementActivity;
	}

	if ( m_fSequenceFinished && m_Activity != ACT_RELOAD )
		SetActivity( m_IdealActivity );
}

//=========================================================
// FindCoverDistance - any node this monster can use that
// is between flMinDist and flMaxDist from the threat
// Visibility isn't checked
//=========================================================
BOOL CBaseMonster :: FindCoverDistance ( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist )
{
	int i;
	float flDist;

	if ( !WorldGraph.m_fGraphPresent || !WorldGraph.m_fGraphPointersSet )
		return FALSE;

	if ( WorldGraph.FindNearestNode( pev->origin, this ) == NO_NODE )
		return FALSE;

	for ( i = 0 ; i < WorldGraph.m_cNodes ; i++ )
	{
		int nodeNumber = ( i + WorldGraph.m_iLastCoverSearch ) % WorldGraph.m_cNodes;
		CNode &node = WorldGraph.Node( nodeNumber );

		if ( !( node.m_afNodeInfo & WorldGraph.NodeType( this ) ) )
			continue;

		WorldGraph.m_iLastCoverSearch = nodeNumber + 1; // next monster that searches for cover node will start where we left off here.

		flDist = ( vecThreat - node.m_vecOrigin ).Length();

		if ( flDist >= flMinDist && flDist <= flMaxDist && MoveToLocation( ACT_RUN, 0, node.m_vecOrigin ) )
			return TRUE;
	}

	return FALSE;
}

//=========================================================
// CheckCreatureDanger - a monster being attacked by a
// creature that grabs or chews its victims runs from it
// (SCHED_LOSE_ENEMY). A hurt one first calls a nearby medic
//=========================================================
#define CREATURE_DANGER_RADIUS		96

static const char *s_szDangerCreatures[] =
{
	"monster_gonome",
	"monster_pitdrone",
	"monster_headcrab",
	"monster_zombie",
	"monster_zombie_barney",
	"monster_zombie_soldier",
	"monster_bullchicken",
	"monster_gargantua",
	"monster_babygarg",
	"monster_bigmomma",
	"monster_alien_voltigore",
};

bool IsDangerCreature( entvars_t *pevCreature, int iCount )
{
	for ( int i = 0; i < iCount; i++ )
	{
		if ( FClassnameIs( pevCreature, s_szDangerCreatures[i] ) )
			return true;
	}

	return false;
}

#define MEDIC_CALL_RADIUS		512
#define MEDIC_HEAL_CALL_DELAY	8

void CBaseMonster :: CheckCreatureDanger( void )
{
	// badly hurt, or hurt at all while not fighting: call a medic
	if ( pev->health < pev->max_health * 0.75
		|| ( ( m_MonsterState == MONSTERSTATE_IDLE || m_MonsterState == MONSTERSTATE_ALERT ) && pev->health < pev->max_health ) )
	{
		CBaseEntity *pEntity = NULL;

		while ( ( pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, MEDIC_CALL_RADIUS ) ) != NULL )
		{
			if ( !FClassnameIs( pEntity->pev, "monster_human_medic_ally" ) || !( gpGlobals->time >= m_flNextMedicCallTime ) )
				continue;

			CSquadTalkMonster *pMedic = dynamic_cast<CSquadTalkMonster *>( pEntity );

			if ( pMedic && pMedic->HealMe( this ) )
			{
				MedicCallSound();
				m_flNextMedicCallTime = gpGlobals->time + MEDIC_HEAL_CALL_DELAY;
				break;
			}
		}
	}

	if ( !m_fCheckCreatureDanger || m_MonsterState == MONSTERSTATE_PRONE || m_MonsterState == MONSTERSTATE_SCRIPT )
		return;

	if ( !( pev->flags & FL_ONGROUND ) || m_hEnemy == NULL || pev->deadflag != DEAD_NO )
		return;

	Schedule_t *pLoseEnemy = GetScheduleOfType( SCHED_LOSE_ENEMY );
	CBaseMonster *pEnemy = m_hEnemy->IsPlayer() ? NULL : m_hEnemy->MyMonsterPointer();

	// the enemy itself is a danger and is going for us
	if ( pEnemy && IsDangerCreature( pEnemy->pev, ARRAYSIZE( s_szDangerCreatures ) )
		&& ( ( pEnemy->pev->deadflag == DEAD_NO && ( pEnemy->m_afConditions & bits_COND_CAN_ATTACK ) )
			|| pEnemy->m_Activity == ACT_MELEE_ATTACK1 || pEnemy->m_Activity == ACT_MELEE_ATTACK2 )
		&& pEnemy->m_hEnemy == this )
	{
		if ( !m_pSchedule || ( !m_fIgnoreCreatureDanger && m_pSchedule != pLoseEnemy ) )
			ChangeSchedule( pLoseEnemy );

		return;
	}

	// Target nearby danger creatures that are chasing us
	CBaseEntity *pEntity = NULL;
	CBaseMonster *pDanger = NULL;

	while ( ( pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, CREATURE_DANGER_RADIUS ) ) != NULL )
	{
		if ( FClassnameIs( pEntity->pev, "monster_babygarg" )
			|| !IsDangerCreature( pEntity->pev, ARRAYSIZE( s_szDangerCreatures ) ) )
		{
			continue;
		}

		CBaseMonster *pMonster = pEntity->MyMonsterPointer();

		if ( pMonster && pMonster->pev->deadflag == DEAD_NO && pMonster->m_hEnemy == this )
		{
			pDanger = pMonster;
			break;
		}
	}

	if ( !pDanger )
		return;

	if ( m_pSchedule && ( m_fIgnoreCreatureDanger || m_pSchedule == pLoseEnemy ) )
		return;

	PushEnemy( pDanger, pDanger->pev->origin );
	SetConditions( bits_COND_NEW_ENEMY );
	m_hEnemy = pDanger;

	ChangeSchedule( pLoseEnemy );
}

//=========================================================
// Following players. This came from CTalkMonster,
// so any player ally can be led
//=========================================================
void CBaseMonster :: FollowerPlayerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !IsAlive() )
		return;

	// Don't allow use during a scripted_sentence
	if ( m_useTime > gpGlobals->time )
		return;

	if ( pCaller == NULL || !pCaller->IsPlayer() || IRelationship( pCaller, true ) > R_NO )
		return;

	// Pre-disaster followers can't be used
	if ( pev->spawnflags & SF_MONSTER_PREDISASTER )
	{
		DeclineFollowing();
	}
	else if ( CanPlayerFollow()
		|| ( m_MonsterState != MONSTERSTATE_SCRIPT && m_hTargetEnt != NULL && pActivator != m_hTargetEnt ) )
	{
		// another player takes the follower over
		StartPlayerFollowing( pCaller, false );
	}
	else
	{
		StopPlayerFollowing( true, false );
	}
}


bool CBaseMonster :: CanPlayerFollow( void )
{
	if ( !m_fCanPlayerFollow || m_MonsterState == MONSTERSTATE_SCRIPT )
		return false;

	if ( !IsAlive() )
		return false;

	return !IsPlayerFollowing();
}


// The leader died, or a revive the follower was busy with has worked
bool CBaseMonster :: ShouldStopPlayerFollow( void )
{
	if ( !IsPlayerFollowing() )
		return false;

	if ( m_hHealTarget == NULL && !m_hTargetEnt->IsAlive() )
		return true;

	if ( m_hHealTarget != NULL && m_fReviving )
		return m_hHealTarget->IsAlive();

	return false;
}


void CBaseMonster :: StartPlayerFollowing( CBaseEntity *pLeader, bool fSilent )
{
	ALERT( at_console, "OK, Let's go!\n" );

	m_hHealTarget = NULL;
	m_hTargetEnt = NULL;
	m_fReviving = false;

	// spoken before taking the leader, so talk monsters look at nobody
	if ( !fSilent )
		UseSound();

	if ( m_pCine )
		m_pCine->CancelScript();

	m_hTargetEnt = pLeader;
	ClearConditions( bits_COND_CLIENT_PUSH );
	ClearSchedule();
}


void CBaseMonster :: StopPlayerFollowing( bool clearSchedule, bool fSilent )
{
	if ( !IsPlayerFollowing() )
		return;

	ALERT( at_console, "OK, I'll wait here.\n" );

	if ( m_movementGoal == MOVEGOAL_TARGETENT )
		RouteClear(); // Stop him from walking toward the player

	m_hHealTarget = NULL;
	m_hTargetEnt = NULL;

	if ( clearSchedule )
		ClearSchedule();

	if ( m_hEnemy != NULL )
		m_IdealMonsterState = MONSTERSTATE_COMBAT;

	if ( !fSilent )
		UnUseSound();
}

//=========================================================
// Ignore conditions - before a set of conditions is allowed
// to interrupt a monster's schedule, this function removes
// conditions that we have flagged to interrupt the current
// schedule, but may not want to interrupt the schedule every
// time. (Pain, for instance)
//=========================================================
int CBaseMonster :: IgnoreConditions ( void )
{
	int iIgnoreConditions = 0;

	if ( !FShouldEat() )
	{
		// not hungry? Ignore food smell.
		iIgnoreConditions |= bits_COND_SMELL_FOOD;
	}

	if ( m_MonsterState == MONSTERSTATE_SCRIPT && m_pCine )
		iIgnoreConditions |= m_pCine->IgnoreConditions();

	return iIgnoreConditions;
}

//=========================================================
// 	RouteClear - zeroes out the monster's route array and goal
//=========================================================
void CBaseMonster :: RouteClear ( void )
{
	RouteNew();
	Stop();

	m_movementGoal = MOVEGOAL_NONE;
	m_movementActivity = ACT_IDLE;
	m_moveradius = 0;
	m_hCineBlocker = NULL;
	Forget( bits_MEMORY_MOVE_FAILED | bits_MEMORY_MOVE_RADIUS_ONLY );
}

//=========================================================
// Route New - clears out a route to be changed, but keeps
//				goal intact.
//=========================================================
void CBaseMonster :: RouteNew ( void )
{
	m_Route[ 0 ].iType		= 0;
	m_iRouteIndex			= 0;
	m_moveradius			= 0;
	m_hCineBlocker			= NULL;
	Forget( bits_MEMORY_MOVE_RADIUS_ONLY );
}

//=========================================================
// FRouteClear - returns TRUE is the Route is cleared out
// ( invalid )
//=========================================================
BOOL CBaseMonster :: FRouteClear ( void )
{
	if ( m_Route[ m_iRouteIndex ].iType == 0 || m_movementGoal == MOVEGOAL_NONE )
		return TRUE;

	return FALSE;
}

//=========================================================
// FRefreshRoute - after calculating a path to the monster's
// target, this function copies as many waypoints as possible
// from that path to the monster's Route array
//=========================================================
BOOL CBaseMonster :: FRefreshRoute ( float flMoveRadius )
{
	CBaseEntity	*pPathCorner;
	int			i;
	BOOL		returnCode;

	RouteNew();
	m_moveradius = flMoveRadius;

	returnCode = FALSE;

	switch( m_movementGoal )
	{
		case MOVEGOAL_PATHCORNER:
			{
				// monster is on a path_corner loop
				pPathCorner = m_pGoalEnt;
				i = 0;

				while ( pPathCorner && i < ROUTE_SIZE )
				{
					m_Route[ i ].iType = bits_MF_TO_PATHCORNER;
					m_Route[ i ].vecLocation = pPathCorner->pev->origin;

					pPathCorner = pPathCorner->GetNextTarget();

					// Last path_corner in list?
					if ( !pPathCorner )
						m_Route[i].iType |= bits_MF_IS_GOAL;
					
					i++;
				}
			}
			returnCode = TRUE;
			break;

		case MOVEGOAL_ENEMY:
			returnCode = BuildRoute( m_vecEnemyLKP, bits_MF_TO_ENEMY, m_hEnemy );
			break;

		case MOVEGOAL_LOCATION:
			returnCode = BuildRoute( m_vecMoveGoal, bits_MF_TO_LOCATION, NULL );
			break;

		case MOVEGOAL_COVER:
			returnCode = BuildRoute( m_vecMoveGoal, bits_MF_TO_LOCATION, NULL, true );
			break;

		case MOVEGOAL_TARGETENT:
			if (m_hTargetEnt != NULL)
			{
				// the goal is kept, so a reached move radius can be measured against it
				m_vecMoveGoal = m_hTargetEnt->pev->origin;
				returnCode = BuildRoute( m_vecMoveGoal, bits_MF_TO_TARGETENT, m_hTargetEnt );
			}
			break;

		case MOVEGOAL_NODE:
			returnCode = FGetNodeRoute( m_vecMoveGoal );
//			if ( returnCode )
//				RouteSimplify( NULL );
			break;
	}

	return returnCode;
}


// These moves take a radius: stopping within it of the goal counts as arriving
BOOL CBaseMonster::MoveToEnemy( Activity movementAct, float waitTime, float flMoveRadius )
{
	m_movementActivity = movementAct;
	m_moveWaitTime = waitTime;

	m_movementGoal = MOVEGOAL_ENEMY;

	BOOL fResult = FRefreshRoute( flMoveRadius );

	m_moveradius = flMoveRadius;
	return fResult;
}


BOOL CBaseMonster::MoveToLocation( Activity movementAct, float waitTime, const Vector &goal, float flMoveRadius )
{
	m_movementActivity = movementAct;
	m_moveWaitTime = waitTime;

	m_movementGoal = MOVEGOAL_LOCATION;
	m_vecMoveGoal = goal;

	BOOL fResult = FRefreshRoute( flMoveRadius );

	m_moveradius = flMoveRadius;
	return fResult;
}


BOOL CBaseMonster::MoveToCover( Activity movementAct, float waitTime, const Vector &goal, float flMoveRadius )
{
	m_movementActivity = movementAct;
	m_moveWaitTime = waitTime;

	m_movementGoal = MOVEGOAL_COVER;
	m_vecMoveGoal = goal;

	BOOL fResult = FRefreshRoute( flMoveRadius );

	m_moveradius = flMoveRadius;
	return fResult;
}


BOOL CBaseMonster::MoveToTarget( Activity movementAct, float waitTime, float flMoveRadius )
{
	m_movementActivity = movementAct;
	m_moveWaitTime = waitTime;

	m_movementGoal = MOVEGOAL_TARGETENT;

	if ( m_hTargetEnt == NULL )
		return FALSE;

	BOOL fResult = FRefreshRoute( flMoveRadius );

	m_moveradius = flMoveRadius;
	return fResult;
}


BOOL CBaseMonster::MoveToNode( Activity movementAct, float waitTime, const Vector &goal, float flMoveRadius )
{
	m_movementActivity = movementAct;
	m_moveWaitTime = waitTime;

	m_movementGoal = MOVEGOAL_NODE;
	m_vecMoveGoal = goal;

	BOOL fResult = FRefreshRoute( flMoveRadius );

	m_moveradius = flMoveRadius;
	return fResult;
}


#ifdef _DEBUG
void DrawRoute( entvars_t *pev, WayPoint_t *m_Route, int m_iRouteIndex, int r, int g, int b )
{
	int			i;

	if ( m_Route[m_iRouteIndex].iType == 0 )
	{
		ALERT( at_aiconsole, "Can't draw route!\n" );
		return;
	}

//	UTIL_ParticleEffect ( m_Route[ m_iRouteIndex ].vecLocation, g_vecZero, 255, 25 );

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMPOINTS);
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( m_Route[ m_iRouteIndex ].vecLocation.x );
		WRITE_COORD( m_Route[ m_iRouteIndex ].vecLocation.y );
		WRITE_COORD( m_Route[ m_iRouteIndex ].vecLocation.z );

		WRITE_SHORT( g_sModelIndexLaser );
		WRITE_BYTE( 0 ); // frame start
		WRITE_BYTE( 10 ); // framerate
		WRITE_BYTE( 1 ); // life
		WRITE_BYTE( 16 );  // width
		WRITE_BYTE( 0 );   // noise
		WRITE_BYTE( r );   // r, g, b
		WRITE_BYTE( g );   // r, g, b
		WRITE_BYTE( b );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
		WRITE_BYTE( 10 );		// speed
	MESSAGE_END();

	for ( i = m_iRouteIndex ; i < ROUTE_SIZE - 1; i++ )
	{
		if ( (m_Route[ i ].iType & bits_MF_IS_GOAL) || (m_Route[ i+1 ].iType == 0) )
			break;

		
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMPOINTS );
			WRITE_COORD( m_Route[ i ].vecLocation.x );
			WRITE_COORD( m_Route[ i ].vecLocation.y );
			WRITE_COORD( m_Route[ i ].vecLocation.z );
			WRITE_COORD( m_Route[ i + 1 ].vecLocation.x );
			WRITE_COORD( m_Route[ i + 1 ].vecLocation.y );
			WRITE_COORD( m_Route[ i + 1 ].vecLocation.z );
			WRITE_SHORT( g_sModelIndexLaser );
			WRITE_BYTE( 0 ); // frame start
			WRITE_BYTE( 10 ); // framerate
			WRITE_BYTE( 1 ); // life
			WRITE_BYTE( 8 );  // width
			WRITE_BYTE( 0 );   // noise
			WRITE_BYTE( r );   // r, g, b
			WRITE_BYTE( g );   // r, g, b
			WRITE_BYTE( b );   // r, g, b
			WRITE_BYTE( 255 );	// brightness
			WRITE_BYTE( 10 );		// speed
		MESSAGE_END();

//		UTIL_ParticleEffect ( m_Route[ i ].vecLocation, g_vecZero, 255, 25 );
	}
}
#endif


int ShouldSimplify( int routeType )
{
	routeType &= ~bits_MF_IS_GOAL;

	if ( (routeType == bits_MF_TO_PATHCORNER) || (routeType & bits_MF_DONT_SIMPLIFY) )
		return FALSE;
	return TRUE;
}

//=========================================================
// RouteSimplify
//
// Attempts to make the route more direct by cutting out
// unnecessary nodes & cutting corners.
//
//=========================================================
void CBaseMonster :: RouteSimplify( CBaseEntity *pTargetEnt )
{
	// BUGBUG: this doesn't work 100% yet
	int			i, count, outCount;
	Vector		vecStart;
	WayPoint_t	outRoute[ ROUTE_SIZE * 2 ];	// Any points except the ends can turn into 2 points in the simplified route

	count = 0;

	for ( i = m_iRouteIndex; i < ROUTE_SIZE; i++ )
	{
		if ( !m_Route[i].iType )
			break;
		else
			count++;
		if ( m_Route[i].iType & bits_MF_IS_GOAL )
			break;
	}
	// Can't simplify a direct route!
	if ( count < 2 )
	{
//		DrawRoute( pev, m_Route, m_iRouteIndex, 0, 0, 255 );
		return;
	}

	outCount = 0;
	vecStart = pev->origin;
	for ( i = 0; i < count-1; i++ )
	{
		// Don't eliminate path_corners
		if ( !ShouldSimplify( m_Route[m_iRouteIndex+i].iType ) )
		{
			outRoute[outCount] = m_Route[ m_iRouteIndex + i ];
			outCount++;
		}
		else if ( CheckLocalMove ( vecStart, m_Route[m_iRouteIndex+i+1].vecLocation, pTargetEnt, NULL ) == LOCALMOVE_VALID )
		{
			// Skip vert
			continue;
		}
		else
		{
			Vector vecTest, vecSplit;

			// Halfway between this and next
			vecTest = (m_Route[m_iRouteIndex+i+1].vecLocation + m_Route[m_iRouteIndex+i].vecLocation) * 0.5;

			// Halfway between this and previous
			vecSplit = (m_Route[m_iRouteIndex+i].vecLocation + vecStart) * 0.5;

			int iType = (m_Route[m_iRouteIndex+i].iType | bits_MF_TO_DETOUR) & ~bits_MF_NOT_TO_MASK;
			if ( CheckLocalMove ( vecStart, vecTest, pTargetEnt, NULL, NULL, true ) >= LOCALMOVE_VALID )
			{
				outRoute[outCount].iType = iType;
				outRoute[outCount].vecLocation = vecTest;
			}
			else if ( CheckLocalMove ( vecSplit, vecTest, pTargetEnt, NULL, NULL, true ) >= LOCALMOVE_VALID )
			{
				outRoute[outCount].iType = iType;
				outRoute[outCount].vecLocation = vecSplit;
				outRoute[outCount+1].iType = iType;
				outRoute[outCount+1].vecLocation = vecTest;
				outCount++; // Adding an extra point
			}
			else
			{
				outRoute[outCount] = m_Route[ m_iRouteIndex + i ];
			}
		}
		// Get last point
		vecStart = outRoute[ outCount ].vecLocation;
		outCount++;
	}
	ASSERT( i < count );
	outRoute[outCount] = m_Route[ m_iRouteIndex + i ];
	outCount++;
	
	// Terminate
	outRoute[outCount].iType = 0;
	ASSERT( outCount < (ROUTE_SIZE*2) );

// Copy the simplified route, disable for testing
	m_iRouteIndex = 0;
	for ( i = 0; i < ROUTE_SIZE && i < outCount; i++ )
	{
		m_Route[i] = outRoute[i];
	}

	// Terminate route
	if ( i < ROUTE_SIZE )
		m_Route[i].iType = 0;

// Debug, test movement code
#if 0
//	if ( CVAR_GET_FLOAT( "simplify" ) != 0 )
		DrawRoute( pev, outRoute, 0, 255, 0, 0 );
//	else
		DrawRoute( pev, m_Route, m_iRouteIndex, 0, 255, 0 );
#endif
}

//=========================================================
// FBecomeProne - tries to send a monster into PRONE state.
// right now only used when a barnacle snatches someone, so 
// may have some special case stuff for that.
//=========================================================
bool CBaseMonster :: FBecomeProne ( CBaseEntity *pOther )
{
	if ( FBitSet ( pev->flags, FL_ONGROUND ) )
	{
		pev->flags -= FL_ONGROUND;
	}

	m_IdealMonsterState = MONSTERSTATE_PRONE;
	return TRUE;
}

//=========================================================
// CheckRangeAttack1
//=========================================================
BOOL CBaseMonster :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( flDist > 64 && flDist <= 784 && flDot >= 0.5 )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// CheckRangeAttack2
//=========================================================
BOOL CBaseMonster :: CheckRangeAttack2 ( float flDot, float flDist )
{
	if ( flDist > 64 && flDist <= 512 && flDot >= 0.5 )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// CheckMeleeAttack1
//=========================================================
// An enemy standing on us counts too, not just one on the ground
BOOL CBaseMonster :: CheckMeleeAttack1 ( float flDot, float flDist )
{
	// Decent fix to keep folks from kicking/punching hornets and snarks is to check the onground flag(sjb)
	if ( flDist <= 64 && flDot >= 0.7 )
		return TRUE;

	return m_hEnemy != NULL && m_hEnemy->pev->groundentity == edict();
}

//=========================================================
// CheckMeleeAttack2
//=========================================================
BOOL CBaseMonster :: CheckMeleeAttack2 ( float flDot, float flDist )
{
	if ( flDist <= 64 && flDot >= 0.7 )
		return TRUE;

	return m_hEnemy != NULL && m_hEnemy->pev->groundentity == edict();
}

//=========================================================
// The _Move checks are the attacks a monster may make
// while moving (CheckAttacks_Move)
//=========================================================
#define MOVE_RANGE_ATTACK1_MAX_DIST		2048

BOOL CBaseMonster :: CheckRangeAttack1_Move ( float flDot, float flDist )
{
	if ( HasConditions( bits_COND_NO_AMMO_LOADED ) || !NoFriendlyFire() || !HasConditions( bits_COND_SEE_ENEMY ) )
		return FALSE;

	return flDist > 64 && flDist <= MOVE_RANGE_ATTACK1_MAX_DIST && flDot >= 0.5;
}

BOOL CBaseMonster :: CheckRangeAttack2_Move ( float flDot, float flDist )
{
	return flDist > 64 && flDist <= 512 && flDot >= 0.5;
}

BOOL CBaseMonster :: CheckMeleeAttack1_Move ( float flDot, float flDist )
{
	return flDist <= 64 && flDot >= 0.7;
}

BOOL CBaseMonster :: CheckMeleeAttack2_Move ( float flDot, float flDist )
{
	return flDist <= 64 && flDot >= 0.7;
}

//=========================================================
// CheckAttacks_Move - CheckAttacks for the attacks allowed
// while moving (m_afMoveShootCap). This measures from
// center to center, and to the surface of a brush target
//=========================================================
#define VOLTIGORE_REACH		75	// a voltigore's size brings it this much closer

void CBaseMonster :: CheckAttacks_Move ( CBaseEntity *pTarget, float flDist )
{
	Vector2D	vec2LOS;
	float		flDot;

	UTIL_MakeVectors ( pev->angles );

	vec2LOS = ( pTarget->Center() - Center() ).Make2D();
	vec2LOS = vec2LOS.Normalize();

	flDot = DotProduct ( vec2LOS , gpGlobals->v_forward.Make2D() );

	if ( FClassnameIs( pTarget->pev, "monster_alien_voltigore" ) )
		flDist -= VOLTIGORE_REACH;

	if ( pTarget->IsBSPModel() )
	{
		TraceResult tr;

		UTIL_TraceLine( Center(), m_vecEnemyLKP, ignore_monsters, ENT( pev ), &tr );

		if ( tr.flFraction == 1.0 || tr.pHit == pTarget->edict() )
			flDist = ( tr.vecEndPos - Center() ).Length();
	}

	ClearConditions( bits_COND_CAN_ATTACK_MOVING );

	if ( FVisible( m_hEnemy, true ) )
		SetConditions( bits_COND_SEE_ENEMY );

	if ( ( m_afCapability & bits_CAP_RANGE_ATTACK1 ) && ( m_afMoveShootCap & bits_MOVESHOOTCAP_RANGE_ATTACK1 )
		&& CheckRangeAttack1_Move( flDot, flDist ) )
	{
		SetConditions( bits_COND_CAN_RANGE_ATTACK1_MOVING );
	}

	if ( ( m_afCapability & bits_CAP_RANGE_ATTACK2 ) && ( m_afMoveShootCap & bits_MOVESHOOTCAP_RANGE_ATTACK2 )
		&& CheckRangeAttack2_Move( flDot, flDist ) )
	{
		SetConditions( bits_COND_CAN_RANGE_ATTACK2_MOVING );
	}

	// flying and noclipping targets can't be hit with a melee attack
	if ( pTarget->pev->movetype == MOVETYPE_FLY || pTarget->pev->movetype == MOVETYPE_NOCLIP )
		return;

	if ( ( m_afCapability & bits_CAP_MELEE_ATTACK1 ) && ( m_afMoveShootCap & bits_MOVESHOOTCAP_MELEE_ATTACK1 )
		&& CheckMeleeAttack1_Move( flDot, flDist ) )
	{
		SetConditions( bits_COND_CAN_MELEE_ATTACK1_MOVING );
	}

	if ( ( m_afCapability & bits_CAP_MELEE_ATTACK2 ) && ( m_afMoveShootCap & bits_MOVESHOOTCAP_MELEE_ATTACK2 )
		&& CheckMeleeAttack2_Move( flDot, flDist ) )
	{
		SetConditions( bits_COND_CAN_MELEE_ATTACK2_MOVING );
	}
}

//=========================================================
// CheckAttacks - sets all of the bits for attacks that the
// monster is capable of carrying out on the passed entity.
//=========================================================
void CBaseMonster :: CheckAttacks ( CBaseEntity *pTarget, float flDist )
{
	Vector2D	vec2LOS;
	float		flDot;

	UTIL_MakeVectors ( pev->angles );

	vec2LOS = ( pTarget->pev->origin - pev->origin ).Make2D();
	vec2LOS = vec2LOS.Normalize();

	flDot = DotProduct (vec2LOS , gpGlobals->v_forward.Make2D() );

	// we know the enemy is in front now. We'll find which attacks the monster is capable of by
	// checking for corresponding Activities in the model file, then do the simple checks to validate
	// those attack types.
	
	// Clear all attack conditions
	ClearConditions( bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_RANGE_ATTACK2 | bits_COND_CAN_MELEE_ATTACK1 |bits_COND_CAN_MELEE_ATTACK2 );

	if ( m_afCapability & bits_CAP_RANGE_ATTACK1 )
	{
		if ( CheckRangeAttack1 ( flDot, flDist ) )
			SetConditions( bits_COND_CAN_RANGE_ATTACK1 );
	}
	if ( m_afCapability & bits_CAP_RANGE_ATTACK2 )
	{
		if ( CheckRangeAttack2 ( flDot, flDist ) )
			SetConditions( bits_COND_CAN_RANGE_ATTACK2 );
	}
	if ( m_afCapability & bits_CAP_MELEE_ATTACK1 )
	{
		if ( CheckMeleeAttack1 ( flDot, flDist ) )
			SetConditions( bits_COND_CAN_MELEE_ATTACK1 );
	}
	if ( m_afCapability & bits_CAP_MELEE_ATTACK2 )
	{
		if ( CheckMeleeAttack2 ( flDot, flDist ) )
			SetConditions( bits_COND_CAN_MELEE_ATTACK2 );
	}
}

//=========================================================
// CanCheckAttacks - prequalifies a monster to do more fine
// checking of potential attacks. 
//=========================================================
BOOL CBaseMonster :: FCanCheckAttacks ( void )
{
	if ( HasConditions(bits_COND_SEE_ENEMY) && !HasConditions( bits_COND_ENEMY_TOOFAR ) )
	{
		return TRUE;
	}

	return FALSE;
}

//=========================================================
// CheckEnemy - part of the Condition collection process,
// gets and stores data and conditions pertaining to a monster's
// enemy. Returns TRUE if Enemy LKP was updated.
//=========================================================
int CBaseMonster :: CheckEnemy ( CBaseEntity *pEnemy )
{
	float	flDistToEnemy;
	int		iUpdatedLKP;// set this to TRUE if you update the EnemyLKP in this function.

	iUpdatedLKP = FALSE;
	ClearConditions ( bits_COND_ENEMY_FACING_ME );
	
	if ( IRelationship( pEnemy ) <= R_NO && m_hEnemy != NULL && (CBaseEntity *) m_hEnemy == pEnemy )
	{
		m_hEnemy = NULL;
		ClearConditions( bits_COND_SEE_ENEMY | bits_COND_ENEMY_OCCLUDED );
		SetConditions( bits_COND_NEW_ENEMY );

		return FALSE;
	}

	if ( !FVisible( pEnemy ) )
	{
		ASSERT(!HasConditions(bits_COND_SEE_ENEMY));
		SetConditions( bits_COND_ENEMY_OCCLUDED );
	}
	else
		ClearConditions( bits_COND_ENEMY_OCCLUDED );

	if ( !pEnemy->IsAlive() )
	{
		m_vecEnemyLKP = pEnemy->pev->origin;
		SetConditions ( bits_COND_ENEMY_DEAD );
		ClearConditions( bits_COND_SEE_ENEMY | bits_COND_ENEMY_OCCLUDED );
		return FALSE;
	}

	Vector vecEnemyPos = pEnemy->pev->origin;

	if ( pEnemy->IsBSPModel() )
		vecEnemyPos = pEnemy->Center();

	// distance to enemy's origin
	flDistToEnemy = ( vecEnemyPos - Center() ).Length();

	if ( !pEnemy->IsBSPModel() )
	{
		if ( pEnemy->pev->size.y > 32 )
			flDistToEnemy -= pEnemy->pev->size.y * 0.5;

		vecEnemyPos.z += pEnemy->pev->size.z * 0.5;
	}

	// distance to enemy's head
	float flDistToEnemy2 = (vecEnemyPos - pev->origin).Length();

	if ( pEnemy->pev->size.y > 32 )
		flDistToEnemy2 -= pEnemy->pev->size.y * 0.5;

	if (flDistToEnemy2 < flDistToEnemy)
		flDistToEnemy = flDistToEnemy2;
	else
	{
		// distance to enemy's feet
		vecEnemyPos.z -= pEnemy->pev->size.z;
		float flDistToEnemy2 = (vecEnemyPos - pev->origin).Length();
		if (flDistToEnemy2 < flDistToEnemy)
			flDistToEnemy = flDistToEnemy2;
	}

	if ( HasConditions( bits_COND_SEE_ENEMY ) )
	{
		CBaseMonster *pEnemyMonster;

		iUpdatedLKP = TRUE;

		if ( pEnemy->IsBSPModel() )
		{
			CAttackVectors *pAttackVectors = pEnemy->GetAttackVectors();

			if ( pAttackVectors )
			{
				for ( AttackVector *pVector = pAttackVectors->m_pHead; pVector; pVector = pVector->m_pNext )
				{
					m_vecEnemyLKP = pVector->m_vecPoint;

					if ( NoFriendlyFireToPos( pVector->m_vecPoint ) )
						break;
				}
			}
		}
		else
			m_vecEnemyLKP = pEnemy->pev->origin;

		pEnemyMonster = pEnemy->MyMonsterPointer();

		if ( pEnemyMonster )
		{
			if ( pEnemyMonster->FInViewCone ( this ) )
			{
				SetConditions ( bits_COND_ENEMY_FACING_ME );
			}
			else
				ClearConditions( bits_COND_ENEMY_FACING_ME );
		}

		if (pEnemy->pev->velocity != Vector( 0, 0, 0))
		{
			// trail the enemy a bit
			m_vecEnemyLKP = m_vecEnemyLKP - pEnemy->pev->velocity * RANDOM_FLOAT( -0.05, 0 );
		}
		else
		{
			// UNDONE: use pev->oldorigin?
		}
	}
	else if ( !HasConditions(bits_COND_ENEMY_OCCLUDED|bits_COND_SEE_ENEMY) && ( flDistToEnemy <= m_flDistTooFar ) )
	{
		// if the enemy is not occluded, and unseen, that means it is behind or beside the monster.
		// if the enemy is near enough the monster, we go ahead and let the monster know where the
		// enemy is.
		iUpdatedLKP = TRUE;
		m_vecEnemyLKP = pEnemy->pev->origin;

		if ( pEnemy->IsBSPModel() )
			m_vecEnemyLKP = pEnemy->Center();
	}

	if ( flDistToEnemy >= m_flDistTooFar && pEnemy->pev->movetype == 1 )
	{
		// enemy is very far away from monster, measured on the ground plane
		ClearConditions( bits_COND_ENEMY_TOOFAR );

		if ( ( pEnemy->Center() - pev->origin ).Length2D() >= m_flDistTooFar * 1.5 )
			SetConditions( bits_COND_ENEMY_TOOFAR );
	}
	else if ( flDistToEnemy >= m_flDistTooFar )
	{
		// enemy is very far away from monster
		SetConditions( bits_COND_ENEMY_TOOFAR );
	}
	else
		ClearConditions( bits_COND_ENEMY_TOOFAR );

	if ( FCanCheckAttacks() && m_MonsterState != MONSTERSTATE_SCRIPT )
	{
		CheckAttacks ( m_hEnemy, flDistToEnemy );
	}

	if ( m_movementGoal == MOVEGOAL_ENEMY && m_hEnemy != NULL && ( m_vecEnemyLKP - pev->origin ).Length2D() < 32 )
		m_vecEnemyLKP = m_hEnemy->pev->origin;

	if ( m_movementGoal == MOVEGOAL_ENEMY )
	{
		for ( int i = m_iRouteIndex; i < ROUTE_SIZE; i++ )
		{
			if ( m_Route[ i ].iType == (bits_MF_IS_GOAL|bits_MF_TO_ENEMY) )
			{
				// UNDONE: Should we allow monsters to override this distance (80?)
				if ( (m_Route[ i ].vecLocation - m_vecEnemyLKP).Length() > 80 )
				{
					// Refresh
					FRefreshRoute();
					return TRUE;
				}
			}
		}
	}

	return iUpdatedLKP;
}

//=========================================================
// PushEnemy - remember the last few enemies, always remember the player
//=========================================================
void CBaseMonster :: PushEnemy( CBaseEntity *pEnemy, Vector &vecLastKnownPos )
{
	int i;

	if (pEnemy == NULL)
		return;

	// UNDONE: blah, this is bad, we should use a stack but I'm too lazy to code one.
	for (i = 0; i < MAX_OLD_ENEMIES; i++)
	{
		if (m_hOldEnemy[i] == pEnemy)
			return;
		if (m_hOldEnemy[i] == NULL) // someone died, reuse their slot
			break;
	}
	if (i >= MAX_OLD_ENEMIES)
		return;

	m_hOldEnemy[i] = pEnemy;
	m_vecOldEnemy[i] = vecLastKnownPos;
}

//=========================================================
// PopEnemy - try remembering the last few enemies
//=========================================================
BOOL CBaseMonster :: PopEnemy( )
{
	// UNDONE: blah, this is bad, we should use a stack but I'm too lazy to code one.
	for (int i = MAX_OLD_ENEMIES - 1; i >= 0; i--)
	{
		if (m_hOldEnemy[i] != NULL)
		{
			if (m_hOldEnemy[i]->IsAlive( ) && IRelationship( m_hOldEnemy[i], false ) > R_NO) // cheat and know when they die
			{
				m_hEnemy = m_hOldEnemy[i];
				m_vecEnemyLKP = m_vecOldEnemy[i];
				// ALERT( at_console, "remembering\n");
				return TRUE;
			}
			else
			{
				m_hOldEnemy[i] = NULL;
			}
		}
	}
	return FALSE;
}

//=========================================================
// SetActivity 
//=========================================================
void CBaseMonster :: SetActivity ( Activity NewActivity )
{
	int	iSequence;

	iSequence = LookupActivity ( NewActivity );

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			// don't reset frame between walk and run
			if ( !(m_Activity == ACT_WALK || m_Activity == ACT_RUN) || !(NewActivity == ACT_WALK || NewActivity == ACT_RUN))
				pev->frame = 0;
		}

		pev->sequence		= iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
	}
	else
	{
		// Not available try to get default anim
		pev->sequence		= 0;	// Set to the reset anim (if it's there)
	}

	m_Activity = NewActivity; // Go ahead and set this so it doesn't keep trying when the anim is not present

	// In case someone calls this with something other than the ideal activity
	m_IdealActivity = m_Activity;

	SetYawSpeed();
}

//=========================================================
// SetSequenceByName
//=========================================================
void CBaseMonster :: SetSequenceByName ( const char *szSequence )
{
	int	iSequence;

	iSequence = LookupSequence ( szSequence );

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		// a named sequence has no activity, so the schedule can't restart it
		m_Activity = ACT_RESET;
		m_IdealActivity = ACT_RESET;

		pev->sequence		= iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
		SetYawSpeed();
	}
	else
	{
		// Not available try to get default anim
		ALERT ( at_aiconsole, "%s has no sequence named:%f\n", STRING(pev->classname), szSequence );
		pev->sequence		= 0;	// Set to the reset anim (if it's there)
	}
}

//=========================================================
// CheckLocalMove - returns TRUE if the caller can walk a 
// straight line from its current origin to the given 
// location. If so, don't use the node graph!
//
// if a valid pointer to a int is passed, the function
// will fill that int with the distance that the check 
// reached before hitting something. THIS ONLY HAPPENS
// IF THE LOCAL MOVE CHECK FAILS!
//
// !!!PERFORMANCE - should we try to load balance this?
// DON"T USE SETORIGIN! 
//=========================================================
#define	LOCAL_STEP_SIZE	18
#define	LOCAL_MOVE_MAX_HEIGHT_DIFF	64
int CBaseMonster :: CheckLocalMove ( const Vector &vecStart, const Vector &vecEnd, CBaseEntity *pTarget, float *pflDist, Vector *pvecEndPosition, bool fWalkMoveCheck )
{
	Vector	vecStartPos;// record monster's position before trying the move
	Vector	vecLastPos;// record monster's position before trying the move
	float	flYaw;
	float	flDist;
	float	flStep, stepSize;
	int		iReturn;

	vecStartPos = pev->origin;
	vecLastPos = pev->origin;

	edict_t *pentOldGround = pev->groundentity;


	flYaw = UTIL_VecToYaw ( vecEnd - vecStart );// build a yaw that points to the goal.
	flDist = ( vecEnd - vecStart ).Length2D();// get the distance.
	iReturn = LOCALMOVE_VALID;// assume everything will be ok.

	// move the monster to the start of the local move that's to be checked.
	UTIL_SetOrigin( pev, vecStart );// !!!BUGBUG - won't this fire triggers? - nope, SetOrigin doesn't fire

	if ( !(pev->flags & (FL_FLY|FL_SWIM)) )
		DROP_TO_FLOOR( ENT( pev ) );//make sure monster is on the floor!

	// this loop takes single steps to the goal.
	for ( flStep = 0 ; flStep < flDist ; flStep += LOCAL_STEP_SIZE )
	{
		stepSize = LOCAL_STEP_SIZE;

		if ( (flStep + LOCAL_STEP_SIZE) >= (flDist-1) )
			stepSize = (flDist - flStep) - 1;

		vecLastPos = pev->origin;

		edict_t *pentTarget;

		if ( pTarget && pTarget->pev )
			pentTarget = pTarget->edict();
		else
			pentTarget = NULL;

		BOOL fMoved;

		if ( fWalkMoveCheck )
			fMoved = WALK_MOVE( ENT(pev), flYaw, stepSize, WALKMOVE_CHECKONLY );
		else
			fMoved = SV_MoveToLocation( ENT( pev ), flYaw, vecEnd, stepSize, pentTarget, false );

		if ( fMoved )
			continue;

		// can't take the next step, fail!
		if ( pflDist != NULL )
			*pflDist = flStep;

		// a scripted move counts once it's within the sequence's moveto_radius,
		// or once a player inside that radius is in the way
		if ( m_MonsterState == MONSTERSTATE_SCRIPT && m_pCine && m_pCine->m_flMoveToRadius != 0 )
		{
			CBaseEntity *pBlocker = CBaseEntity::Instance( gpGlobals->trace_ent ? gpGlobals->trace_ent : INDEXENT( 0 ) );
			float flCineRadius = m_pCine->m_flMoveToRadius;
			bool fArrived = false;

			if ( flCineRadius > ( pev->origin - m_pCine->pev->origin ).Length() )
			{
				m_hCineBlocker = NULL;
				fArrived = true;
			}
			else if ( pBlocker && ( pBlocker->pev->flags & FL_CLIENT )
				&& flCineRadius > ( pBlocker->pev->origin - m_pCine->pev->origin ).Length() )
			{
				m_hCineBlocker = pBlocker;
				fArrived = true;
			}
			if ( fArrived )
			{
				m_afMemory |= bits_MEMORY_MOVE_RADIUS_ONLY;
				UTIL_SetOrigin( pev, vecStartPos );

				if ( pvecEndPosition )
					*pvecEndPosition = vecLastPos;

				return LOCALMOVE_VALID;
			}
		}

		// stopped within the move radius of the goal, which is as good as arriving
		if ( m_moveradius != 0 && ( pev->origin - m_vecMoveGoal ).Length() <= m_moveradius )
		{
			m_afMemory |= bits_MEMORY_MOVE_RADIUS_ONLY;
			UTIL_SetOrigin( pev, vecStartPos );

			if ( pvecEndPosition )
				*pvecEndPosition = vecLastPos;

			return LOCALMOVE_VALID;
		}

		// if this step hits target ent, the move is legal.
		if ( pentTarget && pentTarget == gpGlobals->trace_ent )
			{
			UTIL_SetOrigin( pev, vecStartPos );

			if ( pvecEndPosition )
				*pvecEndPosition = vecLastPos;

			return LOCALMOVE_VALID;
		}

		iReturn = LOCALMOVE_INVALID;
				break;
	}

	if ( iReturn == LOCALMOVE_VALID && 	!(pev->flags & (FL_FLY|FL_SWIM) ) && fabs( vecEnd.z - pev->origin.z ) > LOCAL_MOVE_MAX_HEIGHT_DIFF )
	{
		// The monster can move to a spot UNDER the target, but not to it. Don't try to triangulate, go directly to the node graph.
		// UNDONE: Magic # 64 -- this used to be pev->size.z but that won't work for small creatures like the headcrab
		if (!pTarget || (pTarget->pev->flags & FL_ONGROUND) || pTarget->pev->movetype == MOVETYPE_FLY
			|| pTarget->pev->movetype == MOVETYPE_NOCLIP )
		{
			iReturn = LOCALMOVE_INVALID_DONT_TRIANGULATE;
		}
	}
	/*
	// uncommenting this block will draw a line representing the nearest legal move.
	WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(MSG_BROADCAST, TE_SHOWLINE);
	WRITE_COORD(MSG_BROADCAST, pev->origin.x);
	WRITE_COORD(MSG_BROADCAST, pev->origin.y);
	WRITE_COORD(MSG_BROADCAST, pev->origin.z);
	WRITE_COORD(MSG_BROADCAST, vecStart.x);
	WRITE_COORD(MSG_BROADCAST, vecStart.y);
	WRITE_COORD(MSG_BROADCAST, vecStart.z);
	*/

	// since we've actually moved the monster during the check, undo the move.
	UTIL_SetOrigin( pev, vecStartPos );
	pev->groundentity = pentOldGround;

	if ( pvecEndPosition )
		*pvecEndPosition = vecLastPos;

	return iReturn;
}


float CBaseMonster :: OpenDoorAndWait( entvars_t *pevDoor )
{
	float flTravelTime = 0;

	//ALERT(at_aiconsole, "A door. ");
	CBaseEntity *pcbeDoor = CBaseEntity::Instance(pevDoor);
	if (pcbeDoor && !pcbeDoor->IsLockedByMaster())
	{
		//ALERT(at_aiconsole, "unlocked! ");
		pcbeDoor->Use(this, this, USE_ON, 0.0);
		//ALERT(at_aiconsole, "pevDoor->nextthink = %d ms\n", (int)(1000*pevDoor->nextthink));
		//ALERT(at_aiconsole, "pevDoor->ltime = %d ms\n", (int)(1000*pevDoor->ltime));
		//ALERT(at_aiconsole, "pev-> nextthink = %d ms\n", (int)(1000*pev->nextthink));
		//ALERT(at_aiconsole, "pev->ltime = %d ms\n", (int)(1000*pev->ltime));
		flTravelTime = pevDoor->nextthink - pevDoor->ltime;
		//ALERT(at_aiconsole, "Waiting %d ms\n", (int)(1000*flTravelTime));
		if ( pcbeDoor->pev->targetname )
		{
			edict_t *pentTarget = NULL;
			for (;;)
			{
				pentTarget = FIND_ENTITY_BY_TARGETNAME( pentTarget, STRING(pcbeDoor->pev->targetname));

				if ( VARS( pentTarget ) != pcbeDoor->pev )
				{
					if (FNullEnt(pentTarget))
						break;

					if ( FClassnameIs ( pentTarget, STRING(pcbeDoor->pev->classname) ) )
					{
						CBaseEntity *pDoor = Instance(pentTarget);
						if ( pDoor )
							pDoor->Use(this, this, USE_ON, 0.0);
					}
				}
			}
		}
	}

	return gpGlobals->time + flTravelTime;
}


//=========================================================
// AdvanceRoute - poorly named function that advances the 
// m_iRouteIndex. If it goes beyond ROUTE_SIZE, the route 
// is refreshed. 
//=========================================================
void CBaseMonster :: AdvanceRoute ( float distance )
{

	if ( m_iRouteIndex >= ROUTE_SIZE - 1 )
	{
		int moveRadius = (int) m_moveradius;

		if ( !FRefreshRoute() )
		{
			ALERT ( at_aiconsole, "Can't Refresh Route!!\n" );
			MovementComplete();
		}

		m_moveradius = (float) moveRadius;
	}
	else
	{
		if ( ! (m_Route[ m_iRouteIndex ].iType & bits_MF_IS_GOAL) )
		{
			// If we've just passed a path_corner, advance m_pGoalEnt
			if ( (m_Route[ m_iRouteIndex ].iType & ~bits_MF_NOT_TO_MASK) == bits_MF_TO_PATHCORNER )
			{
				if ( m_pGoalEnt )
					m_pGoalEnt = m_pGoalEnt->GetNextTarget();
			}

			if ( m_Route[m_iRouteIndex].iType & bits_MF_TO_JUMP )
			{
				pev->velocity = m_Route[m_iRouteIndex].vecJumpVelocity;
				pev->flags &= ~FL_ONGROUND;
				m_flMoveWaitFinished = gpGlobals->time + 2.0f;
			}

			if ( ( m_Route[m_iRouteIndex].iType & bits_MF_TO_LADDER )
				&& ( m_Route[m_iRouteIndex + 1].iType & bits_MF_TO_LADDER ) )
			{
				pev->movetype = MOVETYPE_FLY;
				pev->velocity = m_Route[m_iRouteIndex].vecLadderVelocity;
				m_flMoveWaitFinished = gpGlobals->time + m_Route[m_iRouteIndex].flSavedMoveWait;
				m_vecLadderEnd = m_Route[m_iRouteIndex + 1].vecLocation;
			}

			// IF both waypoints are nodes, then check for a link for a door and operate it.
			//
			if (  (m_Route[m_iRouteIndex].iType   & bits_MF_TO_NODE) == bits_MF_TO_NODE
			   && (m_Route[m_iRouteIndex+1].iType & bits_MF_TO_NODE) == bits_MF_TO_NODE)
			{
				//ALERT(at_aiconsole, "SVD: Two nodes. ");

				int iSrcNode  = WorldGraph.FindNearestNode(m_Route[m_iRouteIndex].vecLocation, this );
				int iDestNode = WorldGraph.FindNearestNode(m_Route[m_iRouteIndex+1].vecLocation, this );

				int iLink;
				WorldGraph.HashSearch(iSrcNode, iDestNode, iLink);

				if ( iLink >= 0 && WorldGraph.m_pLinkPool[iLink].m_pLinkEnt != NULL )
				{
					//ALERT(at_aiconsole, "A link. ");
					if ( WorldGraph.HandleLinkEnt ( iSrcNode, WorldGraph.m_pLinkPool[iLink].m_pLinkEnt, m_afCapability, CGraph::NODEGRAPH_DYNAMIC ) )
					{
						//ALERT(at_aiconsole, "usable.");
						entvars_t *pevDoor = WorldGraph.m_pLinkPool[iLink].m_pLinkEnt;
						if (pevDoor)
						{
							m_flMoveWaitFinished = OpenDoorAndWait( pevDoor );
//							ALERT( at_aiconsole, "Wating for door %.2f\n", m_flMoveWaitFinished-gpGlobals->time );
						}
					}
				}
				//ALERT(at_aiconsole, "\n");
			}
			m_iRouteIndex++;
		}
		else	// At goal!!!
		{
			if ( distance <= m_flGroundSpeed * 0.2 || FClassnameIs( pev, "monster_rat" ) || distance == 0 )
			{
				MovementComplete();
			}
		}
	}
}


int CBaseMonster :: RouteClassify( int iMoveFlag )
{
	int movementGoal;

	movementGoal = MOVEGOAL_NONE;

	if ( iMoveFlag & bits_MF_TO_TARGETENT )
		movementGoal = MOVEGOAL_TARGETENT;
	else if ( iMoveFlag & bits_MF_TO_ENEMY )
		movementGoal = MOVEGOAL_ENEMY;
	else if ( iMoveFlag & bits_MF_TO_PATHCORNER )
		movementGoal = MOVEGOAL_PATHCORNER;
	else if ( iMoveFlag & bits_MF_TO_NODE )
		movementGoal = MOVEGOAL_NODE;
	else if ( iMoveFlag & bits_MF_TO_LOCATION )
		movementGoal = MOVEGOAL_LOCATION;

	return movementGoal;
}

//=========================================================
// BuildRoute
//=========================================================
BOOL CBaseMonster :: BuildRoute ( const Vector &vecGoal, int iMoveFlag, CBaseEntity *pTarget, bool fNoExtension )
{
	float	flDist;
	Vector	vecApex;
	Vector	vecApex2;
	int		iLocalMove;

	RouteNew();
	m_movementGoal = RouteClassify( iMoveFlag );

// so we don't end up with no moveflags
	m_Route[ 0 ].vecLocation = vecGoal;
	m_Route[ 0 ].iType = iMoveFlag | bits_MF_IS_GOAL;

// check simple local move
	iLocalMove = CheckLocalMove( pev->origin, vecGoal, pTarget, &flDist );

	if ( iLocalMove == LOCALMOVE_VALID )
	{
		// monster can walk straight there!
		return TRUE;
	}
// try to triangulate around any obstacles.
	else if ( iLocalMove != LOCALMOVE_INVALID_DONT_TRIANGULATE && FTriangulate( pev->origin, vecGoal, flDist, pTarget, &vecApex, &vecApex2, fNoExtension ) )
	{
		// there is a slightly more complicated path that allows the monster to reach vecGoal
		m_Route[ 0 ].vecLocation = vecApex;
		m_Route[ 0 ].iType = (iMoveFlag | bits_MF_TO_DETOUR);

		int iGoal = 1;

		// an extended triangulation adds a second detour
		if ( vecApex2 != g_vecZero )
		{
			m_Route[ 1 ].vecLocation = vecApex2;
			m_Route[ 1 ].iType = (iMoveFlag | bits_MF_TO_DETOUR);
			iGoal = 2;
		}

		m_Route[ iGoal ].vecLocation = vecGoal;
		m_Route[ iGoal ].iType = iMoveFlag | bits_MF_IS_GOAL;

			/*
			WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(MSG_BROADCAST, TE_SHOWLINE);
			WRITE_COORD(MSG_BROADCAST, vecApex.x );
			WRITE_COORD(MSG_BROADCAST, vecApex.y );
			WRITE_COORD(MSG_BROADCAST, vecApex.z );
			WRITE_COORD(MSG_BROADCAST, vecApex.x );
			WRITE_COORD(MSG_BROADCAST, vecApex.y );
			WRITE_COORD(MSG_BROADCAST, vecApex.z + 128 );
			*/

		RouteSimplify( pTarget );
		return TRUE;
	}

// last ditch, try nodes
	if ( FGetNodeRoute( vecGoal ) )
	{
//		ALERT ( at_console, "Can get there on nodes\n" );
		m_vecMoveGoal = vecGoal;
		RouteSimplify( pTarget );
		return TRUE;
	}

	// b0rk
	return FALSE;
}


//=========================================================
// InsertWaypoint - Rebuilds the existing route so that the
// supplied vector and moveflags are the first waypoint in
// the route, and fills the rest of the route with as much
// of the pre-existing route as possible
//=========================================================
void CBaseMonster :: InsertWaypoint ( Vector vecLocation, int afMoveFlags )
{
	int			i, type;

	
	// we have to save some Index and Type information from the real
	// path_corner or node waypoint that the monster was trying to reach. This makes sure that data necessary 
	// to refresh the original path exists even in the new waypoints that don't correspond directy to a path_corner
	// or node. 
	type = afMoveFlags | (m_Route[ m_iRouteIndex ].iType & ~bits_MF_NOT_TO_MASK);

	for ( i = ROUTE_SIZE-1; i > 0; i-- )
		m_Route[i] = m_Route[i-1];

	m_Route[ m_iRouteIndex ].vecLocation = vecLocation;
	m_Route[ m_iRouteIndex ].iType = type;
}

//=========================================================
// FTriangulate - tries to overcome local obstacles by
// triangulating a path around them.
//
// This ignores flDist and measures from vecStart to
// vecEnd. A blocked side continues from where its move
// stopped, and FTriangulateExtension may add a second apex
// in pApex2 (left zero otherwise)
//=========================================================
#define TRIANGULATE_MIN_WIDTH		24		// CheckLocalMove uses a min of 24
#define TRIANGULATE_MAX_WIDTH		48
#define TRIANGULATE_TRIES			6
#define TRIANGULATE_SIDE_BIAS		45		// added to the forward vector's y, not to an angle
#define TRIANGULATE_APEX_SIGHT_DIST	1024	// an apex this close that sees a hidden enemy is good enough

BOOL CBaseMonster :: FTriangulate ( const Vector &vecStart , const Vector &vecFarSide, float flDist, CBaseEntity *pTargetEnt, Vector *pApex, Vector *pApex2, bool fNoExtension )
{
	Vector		vecDir;
	Vector		vecForward;
	Vector		vecLeft;// the spot we'll try to triangulate to on the left
	Vector		vecRight;// the spot we'll try to triangulate to on the right
	Vector		vecTop;// the spot we'll try to triangulate to on the top
	Vector		vecBottom;// the spot we'll try to triangulate to on the bottom
	Vector		vecEndPosition;// the spot that we'll move to after hitting the triangulated point, before moving on to our normal goal.
	int			i;
	int			sizeX, sizeZ;
	float		flDist2D;

	// a swimmer won't chase a target out of the water
	if ( pTargetEnt && ( pev->flags & FL_SWIM ) && pev->waterlevel != 0 )
	{
		if ( ( pTargetEnt->pev->flags & FL_ONGROUND ) || pTargetEnt->pev->waterlevel <= 1 )
			return FALSE;
	}

	// the extension is only tried when the caller can take its second apex
	if ( pApex2 )
		*pApex2 = g_vecZero;
	else
		fNoExtension = true;

	sizeX = (int)pev->size.x;
	if (sizeX < TRIANGULATE_MIN_WIDTH )
		sizeX = TRIANGULATE_MIN_WIDTH;
	else if (sizeX > TRIANGULATE_MAX_WIDTH )
		sizeX = TRIANGULATE_MAX_WIDTH;
	sizeZ = (int)pev->size.z;
	//if (sizeZ < 24.0)
	//	sizeZ = 24.0;

	vecForward = ( vecFarSide - vecStart ).Normalize();

	if ( m_fTriangulateFlip )
		vecForward.y += TRIANGULATE_SIDE_BIAS;
	else
		vecForward.y += -TRIANGULATE_SIDE_BIAS;

	Vector vecDirUp(0,0,1);
	vecDir = CrossProduct ( vecForward, vecDirUp);

	flDist2D = ( vecFarSide - vecStart ).Length2D();

	// start checking right about where the object is, picking two equidistant starting points, one on
	// the left, one on the right. As we progress through the loop, we'll push these away from the obstacle,
	// hoping to find a way around on either side
	vecLeft = vecStart + ( vecForward * ( flDist2D + sizeX ) ) - vecDir * ( sizeX * 3 );
	vecRight = vecStart + ( vecForward * ( flDist2D + sizeX ) ) + vecDir * ( sizeX * 3 );
	if (pev->movetype == MOVETYPE_FLY)
	{
		vecTop = vecStart + (vecForward * flDist2D ) + (vecDirUp * sizeZ * 3);
		vecBottom = vecStart + (vecForward * flDist2D ) - (vecDirUp *  sizeZ * 3);
	}

	vecDir = vecDir * sizeX * 4;
	if (pev->movetype == MOVETYPE_FLY)
		vecDirUp = vecDirUp * sizeZ * 2;

	for ( i = 0 ; i < TRIANGULATE_TRIES; i++ )
	{
		Vector vecRightApex = vecRight;
		Vector vecLeftApex = vecLeft;
		int iRight = CheckLocalMove( vecStart, vecRight, pTargetEnt, NULL, &vecEndPosition );

		if ( iRight == LOCALMOVE_INVALID )
			vecRightApex = vecEndPosition;

		if ( iRight != LOCALMOVE_INVALID_DONT_TRIANGULATE
			&& CheckLocalMove( vecRightApex, vecFarSide, pTargetEnt, NULL ) == LOCALMOVE_VALID )
			{
				if ( pApex )
				*pApex = vecRightApex;

			return TRUE;
			}

		// the left check starts from the monster, not vecStart
		int iLeft = CheckLocalMove( pev->origin, vecLeft, pTargetEnt, NULL, &vecEndPosition );

		if ( iLeft == LOCALMOVE_INVALID )
			vecLeftApex = vecEndPosition;

		if ( iLeft != LOCALMOVE_INVALID_DONT_TRIANGULATE
			&& CheckLocalMove( vecLeftApex, vecFarSide, pTargetEnt, NULL ) == LOCALMOVE_VALID )
			{
				if ( pApex )
				*pApex = vecLeftApex;

			return TRUE;
		}

		if (pev->movetype == MOVETYPE_FLY)
		{
			if ( CheckLocalMove( vecStart, vecTop, pTargetEnt, NULL ) == LOCALMOVE_VALID
				&& CheckLocalMove ( vecTop, vecFarSide, pTargetEnt, NULL ) == LOCALMOVE_VALID )
				{
					if ( pApex )
					*pApex = vecTop;

				return TRUE;
			}

			if ( CheckLocalMove( vecStart, vecBottom, pTargetEnt, NULL ) == LOCALMOVE_VALID
				&& CheckLocalMove ( vecBottom, vecFarSide, pTargetEnt, NULL ) == LOCALMOVE_VALID )
				{
					if ( pApex )
					*pApex = vecBottom;

				return TRUE;
				}
		}

		// an enemy out of sight can be fought from an apex that sees it
		if ( pTargetEnt && !pTargetEnt->IsBSPModel() && !FVisible( pTargetEnt, true )
			&& pTargetEnt->pev->waterlevel <= 1 && (CBaseEntity *) m_hEnemy == pTargetEnt )
		{
			Vector vecTargetEyes = pTargetEnt->pev->origin + pTargetEnt->pev->view_ofs;
			TraceResult tr;

			if ( iLeft != LOCALMOVE_INVALID_DONT_TRIANGULATE )
			{
				UTIL_TraceLine( vecLeftApex + pev->view_ofs, vecTargetEyes, ignore_monsters, ENT( pev ), &tr );

				if ( tr.flFraction == 1.0
					&& ( pTargetEnt->pev->origin - vecLeftApex ).Length2D() <= TRIANGULATE_APEX_SIGHT_DIST )
				{
					if ( pApex )
						*pApex = vecLeftApex;

					return TRUE;
				}
			}

			if ( iRight != LOCALMOVE_INVALID_DONT_TRIANGULATE )
			{
				UTIL_TraceLine( vecRightApex + pev->view_ofs, vecTargetEyes, ignore_monsters, ENT( pev ), &tr );

				if ( tr.flFraction == 1.0
					&& ( pTargetEnt->pev->origin - vecRightApex ).Length2D() <= TRIANGULATE_APEX_SIGHT_DIST )
				{
					if ( pApex )
						*pApex = vecRightApex;

					return TRUE;
				}
			}
		}

		if ( !fNoExtension )
		{
			if ( iRight != LOCALMOVE_INVALID_DONT_TRIANGULATE
				&& FTriangulateExtension( vecRightApex, vecFarSide, flDist2D, pTargetEnt, pApex2 ) )
			{
				*pApex = vecRightApex;
				return TRUE;
			}

			if ( iLeft != LOCALMOVE_INVALID_DONT_TRIANGULATE
				&& FTriangulateExtension( vecLeftApex, vecFarSide, flDist2D, pTargetEnt, pApex2 ) )
			{
				*pApex = vecLeftApex;
				return TRUE;
			}
		}

		vecRight = vecRight + vecDir;
		vecLeft = vecLeft - vecDir;
		if (pev->movetype == MOVETYPE_FLY)
		{
			vecTop = vecTop + vecDirUp;
			vecBottom = vecBottom - vecDirUp;
		}
	}

	// try the other side first next time
	m_fTriangulateFlip = !m_fTriangulateFlip;
	return FALSE;
}

//=========================================================
// FTriangulateExtension - one more triangulation step from
// an apex that couldn't reach vecEnd. flDist is unused
//=========================================================
BOOL CBaseMonster :: FTriangulateExtension ( const Vector &vecStart, const Vector &vecEnd, float flDist, CBaseEntity *pTargetEnt, Vector *pApex )
{
	Vector	vecEndPosition;
	float	sizeX;
	int		iMove;

	sizeX = pev->size.x;

	if ( sizeX < TRIANGULATE_MIN_WIDTH )
		sizeX = TRIANGULATE_MIN_WIDTH;
	else if ( sizeX > TRIANGULATE_MAX_WIDTH )
		sizeX = TRIANGULATE_MAX_WIDTH;

	Vector vecForward = ( vecEnd - vecStart ).Normalize();
	Vector vecDir = CrossProduct( vecForward, Vector( 0, 0, 1 ) ) * ( sizeX * 3 );
	Vector vecAhead = vecStart + vecForward * ( ( vecEnd - vecStart ).Length2D() + sizeX );
	Vector vecRight = vecAhead + vecDir;
	Vector vecLeft = vecAhead - vecDir;

	iMove = CheckLocalMove( vecStart, vecRight, pTargetEnt, NULL, &vecEndPosition );

	if ( iMove == LOCALMOVE_VALID )
	{
		if ( CheckLocalMove( vecRight, vecEnd, pTargetEnt, NULL ) == LOCALMOVE_VALID )
		{
			if ( pApex )
				*pApex = vecRight;

			return TRUE;
		}
	}
	else if ( iMove == LOCALMOVE_INVALID && vecEndPosition != vecStart
		&& CheckLocalMove( vecEndPosition, vecEnd, pTargetEnt, NULL ) == LOCALMOVE_VALID )
	{
		if ( pApex )
			*pApex = vecEndPosition;

		return TRUE;
	}

	iMove = CheckLocalMove( vecStart, vecLeft, pTargetEnt, NULL, &vecEndPosition );

	if ( iMove == LOCALMOVE_VALID )
	{
		if ( CheckLocalMove( vecLeft, vecEnd, pTargetEnt, NULL ) == LOCALMOVE_VALID )
		{
			if ( pApex )
				*pApex = vecLeft;

			return TRUE;
		}
	}
	else if ( iMove == LOCALMOVE_INVALID && vecEndPosition != vecStart
		&& CheckLocalMove( vecEndPosition, vecEnd, pTargetEnt, NULL ) == LOCALMOVE_VALID )
	{
		if ( pApex )
			*pApex = vecEndPosition;

		return TRUE;
	}

	return FALSE;
}

//=========================================================
// Move - take a single step towards the next ROUTE location
//=========================================================
#define DIST_TO_CHECK	200

void CBaseMonster :: Move ( float flInterval )
{
	Vector vecApex = g_vecZero;
	Vector vecApex2 = g_vecZero;

	if ( pev->velocity.z > 8.0f && pev->waterlevel == 0 )
		return;

	if ( pev->movetype == MOVETYPE_FLY )
	{
		if ( ( m_Route[m_iRouteIndex].iType & bits_MF_TO_LADDER )
			&& !( m_Route[m_iRouteIndex + 1].iType & bits_MF_TO_LADDER ) )
		{
			LadderMoveDone();
		}
	}
	else
	{
		if ( pev->velocity.z < 0 && pev->waterlevel == 0 )
		{
			pev->movetype = MOVETYPE_TOSS;
			return;
		}

		pev->movetype = MOVETYPE_STEP;
	}

	if ( FRouteClear() && ( m_movementGoal == MOVEGOAL_NONE || !FRefreshRoute() ) )
		{
			ALERT( at_aiconsole, "Tried to move with no route!\n" );
			TaskFail();
			return;
		}

	if ( m_flMoveWaitFinished > gpGlobals->time )
	{
		Stop();
		return;
	}

	Vector vecOffset = m_Route[ m_iRouteIndex ].vecLocation - pev->origin;

	float flWaypointDist;

	if ( pev->movetype == MOVETYPE_FLY )
		flWaypointDist = vecOffset.Length();
	else
		flWaypointDist = vecOffset.Length2D();

	if ( ShouldAdvanceRoute( flWaypointDist ) )
	{
		AdvanceRoute( flWaypointDist );
		vecOffset = m_Route[ m_iRouteIndex ].vecLocation - pev->origin;

		if ( pev->movetype == MOVETYPE_FLY )
			flWaypointDist = vecOffset.Length();
		else
			flWaypointDist = vecOffset.Length2D();
	}

	Vector vecDir = vecOffset.Normalize();
	float flCheckDist = V_min( flWaypointDist, (float) DIST_TO_CHECK );
	CBaseEntity *pTargetEnt = NULL;

	for ( int i = m_iRouteIndex; i < ROUTE_SIZE - 1; ++i )
	{
		int moveType = m_Route[i].iType & ~bits_MF_NOT_TO_MASK;

		if ( moveType == bits_MF_TO_ENEMY )
	{
		// only on a PURE move to enemy ( i.e., ONLY MF_TO_ENEMY set, not MF_TO_ENEMY and DETOUR )
		pTargetEnt = m_hEnemy;
			break;
		}

		if ( moveType == bits_MF_TO_TARGETENT )
	{
		pTargetEnt = m_hTargetEnt;
			break;
		}
	}

	float flDist = 0;
	Vector vecEndPosition = g_vecZero;
	int moveResult = CheckLocalMove ( pev->origin, pev->origin + vecDir * flCheckDist, pTargetEnt, &flDist, &vecEndPosition, false );

	if ( moveResult == LOCALMOVE_INVALID )
	{
		CBaseEntity *pBlocker = CBaseEntity::Instance( gpGlobals->trace_ent );
		if (pBlocker)
		{
			DispatchBlocked( edict(), pBlocker->edict() );

			if ( !pBlocker->IsPlayer() )
			{
				TraceResult trace;
				Vector start = pev->origin + Vector( 0, 0, 16 );

				UTIL_TraceLine( start, start + vecDir * flCheckDist, ignore_monsters, NULL, &trace );

				if ( trace.pHit )
				{
					CBaseEntity *door = CBaseEntity::Instance( trace.pHit );

					if ( door && ( FClassnameIs( door->pev, "func_door" )
						|| FClassnameIs( door->pev, "func_door_rotating" ) )
						&& WorldGraph.HandleLinkEnt( 0, door, m_afCapability, CGraph::NODEGRAPH_DYNAMIC, this ) )
					{
						m_flMoveWaitFinished = OpenDoorAndWait( door->pev );
						return;
					}

		if ( m_Route[m_iRouteIndex].iType & bits_MF_TO_LADDER )
					{
						if ( ( pev->origin - m_Route[m_iRouteIndex].vecLocation ).Length2D() < 32.0f )
							AdvanceRoute( 0 );

						moveResult = LOCALMOVE_VALID;
					}
				}
			}
		}

		CBaseMonster *pBlockingMonster;

		if ( pBlocker && pBlocker->IsPlayer() )
			pBlockingMonster = pBlocker->MyMonsterPointer();
		else
			pBlockingMonster = NULL;

		float blockerSpeed;

		if ( pBlockingMonster )
			blockerSpeed = pBlockingMonster->m_flGroundSpeed;
		else
			blockerSpeed = 0;

		if ( moveResult == LOCALMOVE_INVALID && m_flGroundSpeed > 0 && m_moveWaitTime > 0
			&& pBlocker && pBlocker->IsMoving() && gpGlobals->time - m_flMoveWaitFinished > 3.0f )
		{
			if ( m_flGroundSpeed * flInterval >= flDist
				&& ( !pBlocker->IsPlayer() || m_flGroundSpeed > blockerSpeed || flDist < 1.0f ) )
			{
				Stop();
				m_flMoveWaitFinished = gpGlobals->time + m_moveWaitTime;
				return;
			}
			// Ok, still enough room to take a step
		}
		else if ( moveResult == LOCALMOVE_INVALID && m_flGroundSpeed > 0
			&& ( !pBlocker || !pBlocker->IsPlayer() || blockerSpeed < m_flGroundSpeed
				|| pBlocker->IsFacing( pev, 0.9f ) ) )
		{
			// try to triangulate around whatever is in the way.
			if ( FTriangulate( pev->origin, m_Route[ m_iRouteIndex ].vecLocation,
				flWaypointDist, pTargetEnt, &vecApex, &vecApex2 ) )
			{
				if ( vecApex2 != g_vecZero )
					InsertWaypoint( vecApex2, bits_MF_TO_DETOUR );

				InsertWaypoint( vecApex, bits_MF_TO_DETOUR );
				RouteSimplify( pTargetEnt );
			}
			else if ( !pBlocker || pBlocker->IsBSPModel()
				|| gpGlobals->time - 1.0f <= m_flMoveWaitFinished || HasMemory( bits_MEMORY_MOVE_FAILED ) )
			{
				if ( m_moveWaitTime <= 0 || HasMemory( bits_MEMORY_MOVE_FAILED ) )
				{
//				ALERT ( at_aiconsole, "Couldn't Triangulate\n" );
				Stop();
					TaskFail();

						m_flMoveWaitFinished = gpGlobals->time + 0.5f;
					ALERT( at_aiconsole, "%s Failed to move (%d)!\n", STRING(pev->classname), HasMemory( bits_MEMORY_MOVE_FAILED ) != 0 );
				}
				else
				{
					int moveRadius = (int) m_moveradius;

					if ( FRefreshRoute() )
					{
						if ( gpGlobals->time - m_flMoveWaitFinished < 0.2f )
							Remember( bits_MEMORY_MOVE_FAILED );

						m_flMoveWaitFinished = gpGlobals->time + 0.1f;
						Stop();
					}
					else
					{
						TaskFail();
					}

					m_moveradius = (float) moveRadius;
					//ALERT( at_aiconsole, "%f, %f, %f\n", pev->origin.z, (pev->origin + (vecDir * flCheckDist)).z, m_Route[m_iRouteIndex].vecLocation.z );
				}
				return;
			}
			else
			{
				m_flMoveWaitFinished = gpGlobals->time;
			}
		}
	}

	// Might be waiting for a door
	if ( m_flMoveWaitFinished > gpGlobals->time )
	{
		Stop();
		return;
	}

	float speed = V_max( 0.1f, pev->scale ) * m_flGroundSpeed;

	if ( speed * flInterval > flCheckDist )
		flInterval = flCheckDist / speed;

	MoveExecute( pTargetEnt, vecDir, flInterval );

	if ( MovementIsComplete() )
	{
		Stop();
		RouteClear();
	}
}


void CBaseMonster::LadderMoveDone( void )
{
	pev->movetype = MOVETYPE_STEP;
	UTIL_SetOrigin( pev, m_vecLadderEnd );
	pev->velocity = g_vecZero;
}

BOOL CBaseMonster:: ShouldAdvanceRoute( float flWaypointDist )
{
	if ( flWaypointDist <= MONSTER_CUT_CORNER_DIST )
	{
		// ALERT( at_console, "cut %f\n", flWaypointDist );
		return TRUE;
	}

	return FALSE;
}


#define MOVE_FAST_TURN_DIST		512		// farther from the waypoint than this, turn at...
#define MOVE_FAST_TURN_SPEED	126		// ...this yaw speed
#define MOVE_MIN_TURN_DIST		8
#define MOVE_MAX_FALL_SPEED		150		// falling faster than this, a monster doesn't walk

void CBaseMonster::MoveExecute( CBaseEntity *pTargetEnt, const Vector &vecDir, float flInterval )
{
//	float flYaw = UTIL_VecToYaw ( m_Route[ m_iRouteIndex ].vecLocation - pev->origin );// build a yaw that points to the goal.
//	WALK_MOVE( ENT(pev), flYaw, m_flGroundSpeed * flInterval, WALKMOVE_NORMAL );
	if ( !( pev->flags & FL_ONGROUND ) && pev->velocity.z > 0 )
		return;

	if ( pev->velocity.z < -MOVE_MAX_FALL_SPEED )
		return;

	float flTotal = V_max( 0.1f, pev->scale ) * m_flGroundSpeed * pev->framerate * flInterval;

	if ( m_IdealActivity != m_movementActivity || m_movementActivity != m_GaitActivity )
	{
		SetGaitActivity( m_movementActivity );
		m_movementActivity = m_GaitActivity;
		pev->iuser1 = 1;
	}

	CheckAndApplyGenericAttacks();

	// turn faster the closer the waypoint is
	Vector vecWaypoint = m_Route[ m_iRouteIndex ].vecLocation;
	float flWaypointDist = ( vecWaypoint - pev->origin ).Length2D();
	float flTurnDist = V_max( (float) MOVE_MIN_TURN_DIST, V_min( flWaypointDist, (float) MOVE_FAST_TURN_DIST ) );
	int iYawSpeed = (int) ( 360.0f / flTurnDist * 180.0f );

	MakeIdealYaw( vecWaypoint );
	ChangeYaw( iYawSpeed );

	float flYaw = pev->angles.y;

	edict_t *pentTarget;

	if ( pTargetEnt )
		pentTarget = pTargetEnt->edict();
	else
		pentTarget = NULL;

	while (flTotal > 0.001)
	{
		// don't walk more than 16 units or stairs stop working
		float flStep = V_min( 16.0, flTotal );

		WALK_MOVE( ENT( pev ), flYaw, 0, WALKMOVE_NORMAL );

		float flRadians = flYaw * (float) ( M_PI / 180.0 );
		Vector vecStep( cos( flRadians ) * flStep, sin( flRadians ) * flStep, 0 );

		if ( !SV_movestep( ENT( pev ), vecStep, true, pentTarget ) )
			SV_NewChaseDir( ENT(pev), m_Route[ m_iRouteIndex ].vecLocation, flStep, pentTarget );
		flTotal -= flStep;
	}
	// ALERT( at_console, "dist %f\n", m_flGroundSpeed * pev->framerate * flInterval );
}


//=========================================================
// MonsterInit - after a monster is spawned, it needs to 
// be dropped into the world, checked for mobility problems,
// and put on the proper path, if any. This function does
// all of those things after the monster spawns. Any
// initialization that should take place for all monsters
// goes here.
//=========================================================
void CBaseMonster :: MonsterInit ( void )
{
	if (!g_pGameRules->FAllowMonsters())
	{
		pev->flags |= FL_KILLME;		// Post this because some monster code modifies class data after calling this function
//		REMOVE_ENTITY(ENT(pev));
		return;
	}

	if ( pev->vuser1 != g_vecZero || pev->vuser2 != g_vecZero )
	{
		Vector minimum;

		if ( pev->vuser1 == g_vecZero )
			minimum = pev->mins;
		else
			minimum = pev->vuser1;

		Vector maximum;

		if ( pev->vuser2 == g_vecZero )
			maximum = pev->maxs;
		else
			maximum = pev->vuser2;

		UTIL_SetSize( pev, minimum, maximum );
		m_fCustomModel = true;
	}

	pev->effects		= 0;
	pev->takedamage		= DAMAGE_AIM;
	pev->ideal_yaw		= UTIL_AngleMod( pev->angles.y );
	pev->max_health		= pev->health;
	pev->deadflag		= DEAD_NO;
	pev->gaitsequence = GAIT_SEQUENCE_NONE;
	m_GaitActivity = ACT_RESET;

	if ( pev->scale == 0 )
		pev->scale = 1;

	m_IdealMonsterState	= MONSTERSTATE_IDLE;// Assume monster will be idle, until proven otherwise

	m_IdealActivity = ACT_IDLE;
	m_iOriginalRenderMode = pev->rendermode;
	m_iOriginalRenderFX = pev->renderfx;
	m_flOriginalRenderAmount = pev->renderamt;
	m_vecOriginalRenderColor = pev->rendercolor;
	pev->flags |= FL_MONSTER;
	if ( pev->spawnflags & SF_MONSTER_HITMONSTERCLIP )
		pev->flags |= FL_MONSTERCLIP;

	if ( m_fFreeRoamMode2 )
		pev->spawnflags |= SF_MONSTER_FREE_ROAM_MODE2;

	if ( m_fFreeRoamMode1 )
		pev->spawnflags |= SF_MONSTER_FREE_ROAM_MODE1;

	ClearSchedule();
	RouteClear();
	InitBoneControllers( ); // FIX: should be done in Spawn

	m_iHintNode			= NO_NODE;

	m_afMemory			= MEMORY_CLEAR;

	m_hEnemy			= NULL;

	m_flDistTooFar		= 1024;
	m_flDistLook		= 2048;

	Vector eyePosition( 0, 0, 0 );

	GetEyePosition( GET_MODEL_PTR( edict() ), eyePosition );
	pev->view_ofs = eyePosition;

	SetThink( &CBaseMonster::MonsterInitThink );
	pev->nextthink = gpGlobals->time + 0.1;
	m_fIgnoreCreatureDanger = false;

	switch ( m_iBloodColorOverride )
	{
		case -1:
			m_bloodColor = DONT_BLEED;
			break;
		case 1:
			m_bloodColor = BLOOD_COLOR_RED;
			break;
		case 2:
			m_bloodColor = BLOOD_COLOR_YELLOW;
			break;
	}

	if ( IsPlayerAlly() && !( pev->spawnflags & SF_MONSTER_PREDISASTER ) )
	{
		SetUse ( &CBaseMonster::CallFollowerPlayerUse );
		m_fCanPlayerFollow = true;
	}
}

//=========================================================
// MonsterInitThink - Calls StartMonster. Startmonster is
// virtual, but this function cannot be
//=========================================================
void CBaseMonster :: MonsterInitThink ( void )
{
	StartMonster();
}

void CBaseMonster::OnCreate( void )
{
	m_pScheduleSaved = NULL;
	m_pInventory = NULL;
	ResetEffects();
	CBaseToggle::OnCreate();
}

void CBaseMonster::OnDestroy( void )
{
	edict_t *selected = m_hSelectedTank.Get();

	if ( selected && !selected->free && selected->pvPrivateData )
	{
		CFuncTank *tank = static_cast<CFuncTank *>( (CBaseEntity *) selected->pvPrivateData );
		edict_t *controller = tank->m_hMonsterController.Get();

		if ( controller && !controller->free && controller->pvPrivateData == this )
			tank->MonsterStopControl();

		m_hSelectedTank = NULL;
	}

	CBaseToggle::OnDestroy();
}

void CBaseMonster::ResetEffects( void )
{
	m_vecEffectGlowColor = g_vecZero;
	m_iEffectBlockWeapons = 0;
	m_iEffectInvulnerable = 0;
	m_iEffectInvisible = 0;
	m_iEffectNonSolid = 0;
	m_flEffectRespiration = 0;
	m_flEffectGravity = 1;
	m_flEffectFriction = 1;
	m_flEffectSpeed = 1;
	m_flEffectDamage = 1;
	m_flOriginalGravity = 1;
	m_flOriginalFriction = 1;
}

void CBaseMonster::ApplyEffects( void )
{
	CBasePlayer *player = IsPlayer() ? dynamic_cast<CBasePlayer *>( this ) : NULL;

	if ( player != NULL && player->IsObserver() )
		return;

	if ( !( m_vecEffectGlowColor.x > 0 ) )
		m_vecEffectGlowColor.x = 0;

	if ( !( m_vecEffectGlowColor.y > 0 ) )
		m_vecEffectGlowColor.y = 0;

	if ( !( m_vecEffectGlowColor.z > 0 ) )
		m_vecEffectGlowColor.z = 0;

	if ( m_iEffectBlockWeapons < 0 )
		m_iEffectBlockWeapons = 0;

	if ( m_iEffectInvulnerable < 0 )
		m_iEffectInvulnerable = 0;

	if ( m_iEffectInvisible < 0 )
		m_iEffectInvisible = 0;

	if ( m_iEffectNonSolid < 0 )
		m_iEffectNonSolid = 0;

	if ( m_vecEffectGlowColor == g_vecZero )
	{
		pev->renderfx = m_iOriginalRenderFX;
		pev->rendercolor = m_vecOriginalRenderColor;
	}
	else
	{
		pev->renderfx = kRenderFxGlowShell;
		pev->rendercolor.x = min( m_vecEffectGlowColor.x, 255.0f );
		pev->rendercolor.y = min( m_vecEffectGlowColor.y, 255.0f );
		pev->rendercolor.z = min( m_vecEffectGlowColor.z, 255.0f );
	}

	bool wasBlocked = ( pev->flags & FL_NOWEAPONS ) != 0;
	bool isBlocked = m_iEffectBlockWeapons > 0;

	if ( isBlocked )
		pev->flags |= FL_NOWEAPONS;
	else
		pev->flags &= ~FL_NOWEAPONS;

	if ( m_iEffectInvulnerable > 0 )
		pev->flags |= FL_GODMODE;
	else
		pev->flags &= ~FL_GODMODE;

	if ( m_iEffectInvisible > 0 )
	{
		pev->renderamt = 0;
		pev->rendermode = kRenderTransTexture;
		pev->flags |= FL_NOTARGET;
	}
	else
	{
		pev->renderamt = m_flOriginalRenderAmount;
		pev->rendermode = m_iOriginalRenderMode;
		pev->flags &= ~FL_NOTARGET;
	}

	pev->solid = m_iEffectNonSolid > 0 ? SOLID_NOT : SOLID_SLIDEBOX;
	pev->gravity = m_flOriginalGravity * m_flEffectGravity;

	float friction = m_flOriginalFriction * m_flEffectFriction;

	if ( IsPlayer() && friction > 1.0f )
		friction = 1.0f;

	pev->friction = friction;
	player = IsPlayer() ? dynamic_cast<CBasePlayer *>( this ) : NULL;

	if ( player == NULL )
		return;

	int cheats = player->GetCheats() | player->m_iKeyValueCheats;

	pev->movetype = MOVETYPE_WALK;

	if ( cheats & PLAYER_CHEAT_GODMODE )
		pev->flags |= FL_GODMODE;

	if ( cheats & PLAYER_CHEAT_NOCLIP )
		pev->movetype = MOVETYPE_NOCLIP;

	if ( cheats & PLAYER_CHEAT_NOTARGET )
		pev->flags |= FL_NOTARGET;

	if ( cheats & PLAYER_CHEAT_NONSOLID )
		pev->solid = SOLID_NOT;

	if ( cheats & PLAYER_CHEAT_INVISIBLE )
	{
		pev->renderamt = 0;
		pev->rendermode = kRenderTransTexture;
		pev->flags |= FL_NOTARGET;
	}

	if ( isBlocked != wasBlocked )
	{
		if ( pev->flags & FL_NOWEAPONS )
			player->BlockWeapons( this );
		else
			player->UnblockWeapons( this );
	}

	player->UpdatePlayerSpeed();
}

void CBaseMonster::Precache( void )
{
	CBaseEntity::Precache();
}

bool CBaseMonster::AreBaseReviveConditionsMet( void )
{
	return !m_fIsNotRevivable && !IsAlive() && m_pfnThink != NULL;
}

bool CBaseMonster::IsRevivable( void )
{
	return AreBaseReviveConditionsMet() && ( pev->flags & FL_MONSTER ) && m_bloodColor != DONT_BLEED;
}

void CBaseMonster::BeginRevive( float flTimeUntilRevive )
{
	pev->renderamt = 255;
	pev->nextthink = gpGlobals->time + flTimeUntilRevive;
}

void CBaseMonster::EndRevive( float flTimeUntilRevive )
{
	pev->renderamt = m_flOriginalRenderAmount;
	m_flReviveTime = gpGlobals->time;
	SetThink( &CBaseMonster::ReviveThink );
	pev->nextthink = gpGlobals->time + flTimeUntilRevive;
}

void CBaseMonster::ReviveThink( void )
{
	CBaseEntity *entities[8];
	Vector minimum = pev->origin - Vector( 16, 16, 0 );
	Vector maximum = pev->origin + Vector( 16, 16, 72 );

	pev->flags |= FL_FROZEN;

	int count = UTIL_EntitiesInBox( entities, ARRAYSIZE( entities ), minimum, maximum, FL_CLIENT | FL_MONSTER );
	int remaining = count;

	for ( int i = 0; i < count; i++ )
	{
		CBaseEntity *entity = entities[i];

		if ( !entity || !entity->IsAlive() || entity == this )
		{
			--remaining;
			continue;
		}

		if ( entity->IsPlayer() )
		{
			TraceResult trace;
			Vector top = pev->origin + Vector( 0, 0, pev->maxs.z );

			UTIL_TraceLine( pev->origin, top, dont_ignore_monsters, pev->owner, &trace );

			if ( trace.flFraction == 1.0f )
				--remaining;
		}
		else
		{
			entity->pev->flags &= ~FL_ONGROUND;

			Vector direction = ( entity->Center() - Center() ).Normalize();

			entity->pev->velocity = entity->pev->velocity + direction * 100;
		}
	}

	if ( remaining <= 0 )
	{
		Revive();
		return;
	}

	if ( gpGlobals->time > m_flReviveTime + 4.0f )
	{
		pev->flags &= ~FL_FROZEN;

		if ( IsPlayer() )
		{
			CBasePlayer *player = (CBasePlayer *) this;

			player->m_fReviving = false;
			SetThink( &CBasePlayer::PlayerDeathThink );
		}
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

//=========================================================
// CheckRevival - a corpse asks the medics around it to
// revive it
//=========================================================
#define MEDIC_REVIVE_CALL_DELAY	4

void CBaseMonster::CheckRevival( void )
{
	if ( !( gpGlobals->time >= m_flNextMedicCallTime ) )
		return;

	CBaseEntity *pEntity = NULL;

	while ( ( pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, MEDIC_CALL_RADIUS ) ) != NULL )
	{
		if ( !pEntity->IsAlive() || IRelationship( pEntity ) > R_NO || !FClassnameIs( pEntity->pev, "monster_human_medic_ally" ) )
			continue;

		CSquadTalkMonster *pMedic = dynamic_cast<CSquadTalkMonster *>( pEntity );

		if ( pMedic && pMedic->ReviveMe( this ) )
			m_flNextMedicCallTime = gpGlobals->time + MEDIC_REVIVE_CALL_DELAY;
	}
}

void CBaseMonster::Revive( void )
{
	float maximumHealth = pev->max_health;

	pev->rendermode = m_iOriginalRenderMode;
	pev->renderfx = m_iOriginalRenderFX;
	pev->renderamt = m_flOriginalRenderAmount;
	pev->rendercolor = m_vecOriginalRenderColor;
	Spawn();

	pev->health = (float) ( maximumHealth * 0.5 );
	pev->max_health = maximumHealth;
	pev->angles.x = 0;
	pev->angles.z = 0;
	pev->flags &= ~FL_FROZEN;
	UTIL_SetOrigin( pev, pev->origin );
}

void CBaseMonster::ShockGlowEffect( bool fEnable )
{
	if ( fEnable )
	{
		if ( pev->rendermode != kRenderNormal || pev->renderfx != kRenderFxNone )
			return;

		pev->renderfx = kRenderFxGlowShell;
		pev->renderamt = 4;
		pev->rendercolor = Vector( 85, 200, 255 );
		m_fShockGlow = true;
	}
	else if ( m_fShockGlow )
	{
		pev->renderfx = pev->iuser4;
		pev->renderamt = pev->fuser3;
		pev->rendercolor = pev->vuser4;
		m_fShockGlow = false;
	}
}

// Forget the enemy and every remembered one
void CBaseMonster :: ClearEnemyList( void )
{
	m_hEnemy = NULL;

	for ( int i = 0; i < MAX_OLD_ENEMIES; i++ )
		m_hOldEnemy[i] = NULL;
}

// Same side (player ally or not) and same class
bool CBaseMonster :: IsFriendly( CBaseEntity *pOther )
{
	if ( !pOther || pOther->IsPlayerAlly() != IsPlayerAlly() )
		return false;

	return pOther->Classify() == Classify();
}

int CBaseEntity::GetClassification( int cl )
{
	if ( m_fOverrideClass )
		return m_iClassSelection;

	if ( m_fPlayerAlly )
		return CLASS_PLAYER_ALLY;

	return cl;
}

const char *CBaseEntity::GetClassificationTag( void )
{
	return UTIL_ClassifyToTag( Classify() );
}

const char *CBaseEntity::GetClassificationName( void )
{
	return UTIL_ClassifyToName( Classify() );
}

//=========================================================
// StartMonster - final bit of initization before a monster
// is turned over to the AI.
//=========================================================
void CBaseMonster :: StartMonster ( void )
{
	m_fShockGlow = false;
	m_flShockGlowTime = gpGlobals->time;
	m_fCanFadeStart = false;
	m_flNextTankSearchTime = gpGlobals->time;
	m_flThinkInterval = 0.1f;
	m_flAutomaticAttackTime = 0;
	m_fFallDamageReady = false;
	m_fDeathKnockback = false;
	m_flNextLookTime = gpGlobals->time;
	m_hGuardEnt = NULL;

	if ( LookupActivity ( ACT_RANGE_ATTACK1 ) != ACTIVITY_NOT_AVAILABLE )
	{
		m_afCapability |= bits_CAP_RANGE_ATTACK1;
	}
	if ( LookupActivity ( ACT_RANGE_ATTACK2 ) != ACTIVITY_NOT_AVAILABLE )
	{
		m_afCapability |= bits_CAP_RANGE_ATTACK2;
	}
	if ( LookupActivity ( ACT_MELEE_ATTACK1 ) != ACTIVITY_NOT_AVAILABLE )
	{
		m_afCapability |= bits_CAP_MELEE_ATTACK1;
	}
	if ( LookupActivity ( ACT_MELEE_ATTACK2 ) != ACTIVITY_NOT_AVAILABLE )
	{
		m_afCapability |= bits_CAP_MELEE_ATTACK2;
	}

	// Raise monster off the floor one unit, then drop to floor
	if ( pev->size.z > 32 )
		m_afCapability |= bits_CAP_FALL_DAMAGE;

	if ( pev->movetype != MOVETYPE_FLY && !FBitSet( pev->spawnflags, SF_MONSTER_FALL_TO_GROUND ) )
	{
		pev->flags &= ~FL_ONGROUND;
		pev->origin.z += 1;
		UTIL_SetOrigin( pev, pev->origin );
		DROP_TO_FLOOR( edict() );
		// Try to move the monster to make sure it's not stuck in a brush.
		if (!WALK_MOVE ( edict(), 0, 0, WALKMOVE_NORMAL ) )
		{
			ALERT( at_console, "Monster '%s' stuck in wall (X%.2f Y%.2f Z%.2f) -- level design error\n", STRING(pev->classname), pev->origin.x, pev->origin.y, pev->origin.z );
		}
	}
	else
	{
		WALK_MOVE( edict(), 0, 0, WALKMOVE_NORMAL );
		UTIL_SetOrigin( pev, pev->origin );
		pev->flags &= ~FL_ONGROUND;
	}

	if ( !FStringNull(pev->target) )// this monster has a target
	{
		edict_t *target = FIND_ENTITY_BY_TARGETNAME ( NULL, STRING( pev->target ) );

		if ( target == NULL )
			target = INDEXENT( 0 );

		CBaseEntity *goal;

		if ( target != NULL && !target->free )
			goal = CBaseEntity::Instance( target );
		else
			goal = NULL;

		m_pGoalEnt = NULL;

		if ( goal != NULL && goal->pev != NULL )
		{
			edict_t *goalEdict = goal->edict();

			if ( goalEdict != NULL && !goalEdict->free )
				m_pGoalEnt = CBaseEntity::Instance( goalEdict );
		}

		if ( !m_pGoalEnt )
		{
			ALERT(at_error, "ReadyMonster()--%s couldn't find target %s", STRING(pev->classname), STRING(pev->target));
		}
		else
		{
			// Monster will start turning towards his destination
			MakeIdealYaw ( m_pGoalEnt->pev->origin );

			if ( FClassnameIs( m_pGoalEnt->pev, "path_corner"))
			{
				m_movementGoal = MOVEGOAL_PATHCORNER;

				if ( pev->movetype == MOVETYPE_FLY )
					m_movementActivity = ACT_FLY;
				else
					m_movementActivity = ACT_WALK;

			if ( !FRefreshRoute() )
					ALERT ( at_aiconsole, "Can't Create Route!\n" );

				if ( m_hEnemy != NULL )
					m_hEnemy = NULL;

				m_MonsterState = MONSTERSTATE_IDLE;
				m_IdealMonsterState = MONSTERSTATE_IDLE;
			ChangeSchedule( GetScheduleOfType( SCHED_IDLE_WALK ) );
		}
	}
	}

	m_flLastThinkTime = gpGlobals->time;
	SetThink ( &CBaseMonster::CallMonsterThink );
	pev->nextthink += RANDOM_FLOAT(0.1, 0.4); // spread think times.

	if ( !FStringNull(pev->targetname) )// wait until triggered
	{
		if ( m_hEnemy != NULL )
			m_hEnemy = NULL;

		m_MonsterState = MONSTERSTATE_IDLE;
		m_IdealMonsterState = MONSTERSTATE_IDLE;
		// UNDONE: Some scripted sequence monsters don't have an idle?
		SetActivity( ACT_IDLE );
		ChangeSchedule( GetScheduleOfType( SCHED_WAIT_TRIGGER ) );
	}

	if ( pev->spawnflags & SF_MONSTER_WAIT_TILL_SEEN )
	{
		if ( m_hEnemy != NULL )
			m_hEnemy = NULL;

		m_MonsterState = MONSTERSTATE_IDLE;
		m_IdealMonsterState = MONSTERSTATE_IDLE;
		SetActivity( ACT_IDLE );
		ChangeSchedule( GetScheduleOfType( SCHED_WAIT_TILL_SEEN ) );
	}
}

void CBaseMonster :: MovementComplete( void )
{
	switch( m_iTaskStatus )
	{
	case TASKSTATUS_NEW:
	case TASKSTATUS_RUNNING:
		m_iTaskStatus = TASKSTATUS_RUNNING_TASK;
		break;

	case TASKSTATUS_RUNNING_MOVEMENT:
		TaskComplete();
		break;

	case TASKSTATUS_RUNNING_TASK:
		ALERT( at_error, "Movement completed twice!\n" );
		break;

	case TASKSTATUS_COMPLETE:
		break;
	}
	m_movementGoal = MOVEGOAL_NONE;
}


int CBaseMonster::TaskIsRunning( void )
{
	if ( m_iTaskStatus != TASKSTATUS_COMPLETE &&
		 m_iTaskStatus != TASKSTATUS_RUNNING_MOVEMENT )
		 return 1;

	return 0;
}

//=========================================================
// IRelationship - returns an integer that describes the
// relationship between two types of monster.
//=========================================================
int CBaseEntity::IRelationship( CBaseEntity *other, bool ignoreNoTarget )
{
	if ( !other )
		return R_NO;

	if ( ( other->pev->flags & FL_NOTARGET ) && !ignoreNoTarget )
		return R_NO;

	unsigned int myClass = Classify();

	if ( myClass > CLASS_TEAM4 )
		return R_NO;

	unsigned int otherClass = other->Classify();

	if ( otherClass > CLASS_TEAM4 )
		return R_NO;

	int relationship = iEnemy[myClass][otherClass];

	if ( relationship > R_NO && pev->max_health * WOUNDED_FEAR_HEALTH_FRACTION > pev->health )
		return R_FR;

	return relationship;
}

//=========================================================
// Check whether a squad monster would hit another monster or player
//=========================================================
#define FRIENDLY_FIRE_MAX_MONSTERS	256
#define BULLET_CONE_DOT				0.95	// cos of the half angle bullets are expected to spread over
#define FRIENDLY_FIRE_SIDE_SCALE	1.2		// half width of the firing lane, in monster widths
#define FRIENDLY_FIRE_MEMBER_SCALE	3		// a member this many monster depths away can still be in the way
#define FRIENDLY_FIRE_FAR_ENEMY		1024	// beyond this, any visible member in the lane blocks the shot
#define FRIENDLY_FIRE_MID_ENEMY		512
#define FRIENDLY_FIRE_NEAR_ENEMY	96		// a member this close to a mid range enemy blocks the shot
#define FRIENDLY_FIRE_NO_ENEMY		65536

// Could a member be friendly enough that we must not shoot it?
bool IsFriendlyFireCandidate( CBaseMonster *pShooter, CBaseEntity *pMember )
{
	if ( !pMember || pMember == pShooter )
		return false;

	CBaseMonster *pMonster = pMember->MyMonsterPointer();

	if ( pMonster && pMonster->IRelationship( pShooter ) > R_NO )
		return false;

	int memberClass = pMember->Classify();

	return memberClass != CLASS_NONE && memberClass != CLASS_PLAYER_BIOWEAPON && memberClass != CLASS_ALIEN_BIOWEAPON;
}

//=========================================================
// Monster stepping. This is the engine's sv_move.c movement
// in the game, traced with the monster's hull
//=========================================================
#define MOVE_STEP_HEIGHT		18		// how high a walking monster steps up
#define MOVE_MIN_DROP			36		// a step down probes at least this far
#define MOVE_DROP_HEIGHTS		3		// ...or this many times the monster's height
#define MOVE_TALL_MONSTER		96		// taller monsters start the probe higher
#define MOVE_TALL_MARGIN		16
#define MOVE_HEAD_MARGIN		8		// a walker whose head would be under water stops
#define CHASE_DIR_DEADZONE		10		// goal offsets below this need no turn on that axis
#define CHASE_DIR_NONE			-1

bool IsHurtingContents( int contents )
{
	return contents == CONTENTS_LAVA || contents == CONTENTS_SLIME;
}

bool SV_movestep( edict_t *ent, Vector vecMove, bool relink, edict_t *pentTarget )
{
	TraceResult tr;
	Vector vecOldOrigin = ent->v.origin;
	Vector vecNewOrigin = ent->v.origin + vecMove;

	// flying and swimming monsters move straight
	if ( ent->v.flags & ( FL_FLY | FL_SWIM ) )
	{
		TRACE_MONSTER_HULL( ent, ent->v.origin, vecNewOrigin, dont_ignore_monsters, ent, &tr );

		Vector vecFeet = tr.vecEndPos + Vector( 0, 0, ent->v.mins.z + 1 );

		// the contents are masked with ~2 here, which no engine contents value passes
		if ( ( ent->v.flags & FL_SWIM ) && ent->v.waterlevel <= 1 && !( UTIL_PointContents( vecFeet ) & ~2 ) )
			return false;

		if ( ( ent->v.flags & FL_FLY ) && ent->v.waterlevel == 0 && ( UTIL_PointContents( vecFeet ) & ~2 ) )
			return false;

		if ( tr.flFraction != 1.0 )
			return false;

		ent->v.origin = tr.vecEndPos;

		if ( relink )
			UTIL_SetOrigin( &ent->v, ent->v.origin );

		return true;
	}

	// push down from a step above the wished position
	vecNewOrigin.z += MOVE_STEP_HEIGHT;

	Vector vecEnd = vecNewOrigin;
	vecEnd.z -= V_max( MOVE_DROP_HEIGHTS * ent->v.size.z, MOVE_MIN_DROP );

	TRACE_MONSTER_HULL( ent, vecNewOrigin, vecEnd, dont_ignore_monsters, ent, &tr );

	if ( tr.fAllSolid )
		return false;

	if ( tr.fStartSolid )
	{
		vecNewOrigin.z -= MOVE_STEP_HEIGHT;
		TRACE_MONSTER_HULL( ent, vecNewOrigin, vecEnd, dont_ignore_monsters, ent, &tr );

		if ( tr.fAllSolid || tr.fStartSolid )
			return false;
	}

	if ( ent->v.size.z > MOVE_TALL_MONSTER )
	{
		float flRaise = ent->v.size.z - MOVE_TALL_MONSTER - MOVE_TALL_MARGIN;

		vecEnd.z += flRaise;
		vecNewOrigin.z += flRaise;
		TRACE_MONSTER_HULL( ent, vecNewOrigin, vecEnd, dont_ignore_monsters, ent, &tr );

		if ( tr.fAllSolid )
			return false;

		if ( tr.fStartSolid )
		{
			vecNewOrigin.z -= MOVE_STEP_HEIGHT;
			TRACE_MONSTER_HULL( ent, vecNewOrigin, vecEnd, dont_ignore_monsters, ent, &tr );

			if ( tr.fAllSolid || tr.fStartSolid )
				return false;
		}
	}

	// a walker outside liquid won't step into lava or slime, or deep water
	int oldContents = UTIL_PointContents( vecOldOrigin );

	if ( !IsHurtingContents( oldContents ) && oldContents != CONTENTS_WATER )
	{
		int newContents = UTIL_PointContents( tr.vecEndPos );

		if ( IsHurtingContents( newContents ) )
			return false;

		if ( newContents == CONTENTS_WATER
			&& UTIL_PointContents( tr.vecEndPos + Vector( 0, 0, ent->v.size.z - MOVE_HEAD_MARGIN ) ) == CONTENTS_WATER )
		{
			return false;
		}
	}

	if ( tr.flFraction == 1.0 )
	{
		// if monster had the ground pulled out, go ahead and fall
		if ( !( ent->v.flags & FL_PARTIALGROUND ) )
			return false;

		ent->v.origin = ent->v.origin + vecMove;

		if ( relink )
			UTIL_SetOrigin( &ent->v, ent->v.origin );

		ent->v.flags &= ~FL_ONGROUND;
		return true;
	}

	ent->v.origin = tr.vecEndPos;

	// dropped more than a step: keep the step height and fall the rest
	if ( vecOldOrigin.z - ent->v.origin.z > MOVE_STEP_HEIGHT )
	{
		ent->v.origin = vecOldOrigin + vecMove - Vector( 0, 0, MOVE_STEP_HEIGHT );

		if ( !relink )
			return true;

		UTIL_SetOrigin( &ent->v, ent->v.origin );
		ent->v.flags &= ~( FL_ONGROUND | FL_PARTIALGROUND );
	}
	else
	{
		ent->v.flags &= ~FL_PARTIALGROUND;
	}

	ent->v.groundentity = tr.pHit;

	if ( relink )
		UTIL_SetOrigin( &ent->v, ent->v.origin );

	return true;
}

bool SV_StepDirection( edict_t *ent, float yaw, float dist, edict_t *pentTarget )
{
	yaw = yaw * ( M_PI * 2 ) / 360;

	Vector vecMove( cos( yaw ) * dist, sin( yaw ) * dist, 0 );

	return SV_movestep( ent, vecMove, true, pentTarget );
}

// Try the direct diagonal, then each axis, the old heading, then every direction
bool SV_NewChaseDir( edict_t *actor, Vector vecGoal, float dist, edict_t *pentTarget )
{
	float olddir = UTIL_AngleMod( 45 * (int)( actor->v.ideal_yaw / 45 ) );
	float turnaround = UTIL_AngleMod( olddir - 180 );

	float deltax = vecGoal.x - actor->v.origin.x;
	float deltay = vecGoal.y - actor->v.origin.y;
	float d[3];
	float tdir;

	if ( deltax > CHASE_DIR_DEADZONE )
		d[1] = 0;
	else if ( deltax < -CHASE_DIR_DEADZONE )
		d[1] = 180;
	else
		d[1] = CHASE_DIR_NONE;

	if ( deltay < -CHASE_DIR_DEADZONE )
		d[2] = 270;
	else if ( deltay > CHASE_DIR_DEADZONE )
		d[2] = 90;
	else
		d[2] = CHASE_DIR_NONE;

	// try direct route (215 is the engine's typo for 225)
	if ( d[1] != CHASE_DIR_NONE && d[2] != CHASE_DIR_NONE )
	{
		if ( d[1] == 0 )
			tdir = d[2] == 90 ? 45 : 315;
		else
			tdir = d[2] == 90 ? 135 : 215;

		if ( tdir != turnaround && SV_StepDirection( actor, tdir, dist, pentTarget ) )
			return true;
	}

	// try other directions
	if ( ( rand() & 1 ) || fabs( deltay ) > fabs( deltax ) )
	{
		tdir = d[1];
		d[1] = d[2];
		d[2] = tdir;
	}

	if ( d[1] != CHASE_DIR_NONE && d[1] != turnaround && SV_StepDirection( actor, d[1], dist, pentTarget ) )
		return true;

	if ( d[2] != CHASE_DIR_NONE && d[2] != turnaround && SV_StepDirection( actor, d[2], dist, pentTarget ) )
		return true;

	// there is no direct path to the goal, so pick another direction
	if ( olddir != CHASE_DIR_NONE && SV_StepDirection( actor, olddir, dist, pentTarget ) )
		return true;

	if ( rand() & 1 )
	{
		for ( tdir = 0; tdir <= 315; tdir += 45 )
		{
			if ( tdir != turnaround && SV_StepDirection( actor, tdir, dist, pentTarget ) )
				return true;
		}
	}
	else
	{
		for ( tdir = 315; tdir >= 0; tdir -= 45 )
		{
			if ( tdir != turnaround && SV_StepDirection( actor, tdir, dist, pentTarget ) )
				return true;
		}
	}

	if ( turnaround != CHASE_DIR_NONE && SV_StepDirection( actor, turnaround, dist, pentTarget ) )
		return true;

	// can't move
	actor->v.ideal_yaw = olddir;

	if ( !ENT_IS_ON_FLOOR( actor ) )
		actor->v.flags &= ~FL_ONGROUND;

	return false;
}

// Try another route to vecGoal if the step is blocked and fChaseGoal is set
bool SV_MoveToLocation( edict_t *ent, float yaw, Vector vecGoal, float dist, edict_t *pentTarget, bool fChaseGoal )
{
	if ( SV_StepDirection( ent, yaw, dist, pentTarget ) )
		return true;

	if ( !fChaseGoal )
		return false;

	return SV_NewChaseDir( ent, vecGoal, dist, pentTarget );
}

// Clear the leg animation if the model has no matching activity
void CBaseMonster::SetGaitActivity( Activity NewActivity )
{
	int iSequence = LookupActivity( NewActivity );

	if ( iSequence >= 0 )
	{
		pev->gaitsequence = iSequence;
		ResetGaitSequenceInfo();
	}
	else
	{
		pev->gaitsequence = GAIT_SEQUENCE_NONE;
	}

	m_GaitActivity = NewActivity;
	SetYawSpeed();
}

bool CBaseMonster::NoFriendlyFire( Vector vecPos, CBaseEntity *pTarget )
{
	if ( !( m_afCapability & bits_CAP_SQUAD ) )
		return true;

	if ( !pTarget )
		return false;

	UTIL_MakeVectors( UTIL_VecToAngles( pTarget->Center() - vecPos ) );

	CBaseEntity *pList[FRIENDLY_FIRE_MAX_MONSTERS];
	int count = FindMonstersInWorld( pList, FRIENDLY_FIRE_MAX_MONSTERS, FL_MONSTER );

	for ( int i = 0; i < count; i++ )
	{
		if ( !IsFriendlyFireCandidate( this, pList[i] ) )
			continue;

		if ( FInBulletCone( pList[i], vecPos ) && FVisibleGunPos( pList[i], vecPos ) )
			return false;
	}

	return true;
}

bool CBaseMonster::NoFriendlyFireToPos( Vector vecPos )
{
	if ( !( m_afCapability & bits_CAP_SQUAD ) )
		return true;

	CPlane backPlane;
	CPlane leftPlane;
	CPlane rightPlane;

	UTIL_MakeVectors( UTIL_VecToAngles( vecPos - Center() ) );

	Vector vecSide = gpGlobals->v_right * ( pev->size.x * FRIENDLY_FIRE_SIDE_SCALE );

	leftPlane.InitializePlane( gpGlobals->v_right, Center() - vecSide );
	rightPlane.InitializePlane( -gpGlobals->v_right, Center() + vecSide );
	backPlane.InitializePlane( gpGlobals->v_forward, Center() );

	CBaseEntity *pList[FRIENDLY_FIRE_MAX_MONSTERS];
	int count = FindMonstersInWorld( pList, FRIENDLY_FIRE_MAX_MONSTERS, FL_MONSTER );

	for ( int i = 0; i < count; i++ )
	{
		CBaseEntity *pMember = pList[i];

		if ( !IsFriendlyFireCandidate( this, pMember ) )
			continue;

		if ( !backPlane.PointInFront( pMember->pev->origin )
			|| !leftPlane.PointInFront( pMember->pev->origin )
			|| !rightPlane.PointInFront( pMember->pev->origin ) )
		{
			continue;
		}

		float flMemberDist = ( pMember->pev->origin - Center() ).Length2D();
		float flEnemyDist = FRIENDLY_FIRE_NO_ENEMY;
		float flEnemyToMember = FRIENDLY_FIRE_NO_ENEMY;

		if ( m_hEnemy != NULL )
		{
			flEnemyDist = ( m_hEnemy->Center() - Center() ).Length2D();
			flEnemyToMember = ( m_hEnemy->Center() - pMember->pev->origin ).Length2D();
		}

		if ( !FVisibleGunPos( pMember, Center() ) )
			continue;

		bool fInLane = FInBulletCone( pMember, Center() )
			&& ( flEnemyDist > flMemberDist
				|| ( flMemberDist > FRIENDLY_FIRE_MEMBER_SCALE * pev->size.y && flMemberDist > flEnemyDist ) );

		if ( fInLane
			|| flEnemyDist > FRIENDLY_FIRE_FAR_ENEMY
			|| ( flEnemyDist > FRIENDLY_FIRE_MID_ENEMY && flEnemyToMember < FRIENDLY_FIRE_NEAR_ENEMY ) )
		{
			return false;
		}
	}

	return true;
}

// Can a gun held at vecSrc see the entity's eyes? Glass blocks the view
bool CBaseMonster::FVisibleGunPos( CBaseEntity *pEntity, const Vector &vecSrc )
{
	TraceResult tr;

	UTIL_TraceLine( vecSrc + pev->view_ofs, pEntity->EyePosition(), ignore_monsters, dont_ignore_glass, edict(), &tr );

	return tr.flFraction == 1.0;
}

// Is the entity's origin or top within the cone of a shot from vecSrc at our enemy?
bool CBaseMonster::FInBulletCone( CBaseEntity *pEntity, const Vector &vecSrc )
{
	if ( m_hEnemy != NULL )
		UTIL_MakeVectors( UTIL_VecToAngles( m_hEnemy->Center() - vecSrc ) );
	else
		UTIL_MakeVectors( pev->angles );

	Vector vecDir = ( pEntity->pev->origin - vecSrc ).Normalize();

	if ( DotProduct( vecDir, gpGlobals->v_forward ) > BULLET_CONE_DOT )
		return true;

	vecDir = ( pEntity->pev->origin + Vector( 0, 0, pEntity->pev->maxs.z ) - vecSrc ).Normalize();

	return DotProduct( vecDir, gpGlobals->v_forward ) > BULLET_CONE_DOT;
}

//=========================================================
// FindCover - tries to find a nearby node that will hide
// the caller from its enemy. 
//
// If supplied, search will return a node at least as far
// away as MinDist, but no farther than MaxDist. 
// if MaxDist isn't supplied, it defaults to a reasonable 
// value
//=========================================================
// UNDONE: Should this find the nearest node?

//float CGraph::PathLength( int iStart, int iDest, int iHull, int afCapMask )

BOOL CBaseMonster :: FindCover ( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist )
{
	int i;
	int iMyHullIndex;
	int iMyNode;
	int iThreatNode;
	float flDist;
	Vector	vecLookersOffset;
	TraceResult tr;

	if ( !flMaxDist )
	{
		// user didn't supply a MaxDist, so work up a crazy one.
		flMaxDist = 784;
	}

	if ( flMinDist > 0.5 * flMaxDist)
	{
#if _DEBUG
		ALERT ( at_console, "FindCover MinDist (%.0f) too close to MaxDist (%.0f)\n", flMinDist, flMaxDist );
#endif
		flMinDist = 0.5 * flMaxDist;
	}

	if ( !WorldGraph.m_fGraphPresent || !WorldGraph.m_fGraphPointersSet )
	{
		ALERT ( at_aiconsole, "Graph not ready for findcover!\n" );
		return FALSE;
	}

	iMyNode = WorldGraph.FindNearestNode( pev->origin, this );
	iThreatNode = WorldGraph.FindNearestNode ( vecThreat, this );
	iMyHullIndex = WorldGraph.HullIndex( this );

	if ( iMyNode == NO_NODE )
	{
		ALERT ( at_aiconsole, "FindCover() - %s has no nearest node!\n", STRING(pev->classname));
		return FALSE;
	}
	if ( iThreatNode == NO_NODE )
	{
		// ALERT ( at_aiconsole, "FindCover() - Threat has no nearest node!\n" );
		iThreatNode = iMyNode;
		// return FALSE;
	}

	vecViewOffset = g_vecZero;
	vecLookersOffset = vecThreat + vecViewOffset;// calculate location of enemy's eyes

	// we'll do a rough sample to find nodes that are relatively nearby
	for ( i = 0 ; i < WorldGraph.m_cNodes ; i++ )
	{
		int nodeNumber = (i + WorldGraph.m_iLastCoverSearch) % WorldGraph.m_cNodes;

		CNode &node = WorldGraph.Node( nodeNumber );

		if ( !( WorldGraph.NodeType( this ) & node.m_afNodeInfo ) )
			continue;

		WorldGraph.m_iLastCoverSearch = nodeNumber + 1; // next monster that searches for cover node will start where we left off here.

		// could use an optimization here!!
		flDist = ( pev->origin - node.m_vecOrigin ).Length();

		// DON'T do the trace check on a node that is farther away than a node that we've already found to
		// provide cover! Also make sure the node is within the mins/maxs of the search.
		if ( flDist >= flMinDist && flDist < flMaxDist )
		{
			UTIL_TraceLine ( node.m_vecOrigin + vecViewOffset, vecLookersOffset, ignore_monsters, dont_ignore_glass,  ENT(pev), &tr );

			// if this node will block the threat's line of sight to me...
			if ( tr.flFraction != 1.0 )
			{
				// ..and the way there stays out of the threat's sight, the node is good
				if ( vecThreat == pev->origin || ( iMyNode == iThreatNode )
					|| WorldGraph.PathVis( iMyNode, nodeNumber, vecThreat, pev->origin, iMyHullIndex, m_afCapability ) )
				{
					if ( MoveToCover( ACT_RUN, 0, node.m_vecOrigin, 0 ) )
					{
						/*
						MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
							WRITE_BYTE( TE_SHOWLINE);
							
							WRITE_COORD( node.m_vecOrigin.x );
							WRITE_COORD( node.m_vecOrigin.y );
							WRITE_COORD( node.m_vecOrigin.z );

							WRITE_COORD( vecLookersOffset.x );
							WRITE_COORD( vecLookersOffset.y );
							WRITE_COORD( vecLookersOffset.z );
						MESSAGE_END();
						*/

						return TRUE;
					}
				}
			}
		}
	}
	return FALSE;
}


//=========================================================
// FindCoverGrenade - FindCover for a grenade threat: only
// nodes this monster can use, and monsters block the sight
// line too
//=========================================================
#define COVER_DEFAULT_MAX_DIST	784

BOOL CBaseMonster :: FindCoverGrenade ( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist )
{
	int i;
	int iMyHullIndex;
	int iMyNode;
	int iThreatNode;
	float flDist;
	Vector	vecLookersOffset;
	TraceResult tr;

	if ( !flMaxDist )
		flMaxDist = COVER_DEFAULT_MAX_DIST;

	if ( flMinDist > 0.5 * flMaxDist )
		flMinDist = 0.5 * flMaxDist;

	if ( !WorldGraph.m_fGraphPresent || !WorldGraph.m_fGraphPointersSet )
	{
		ALERT ( at_aiconsole, "Graph not ready for findcover!\n" );
		return FALSE;
	}

	iMyNode = WorldGraph.FindNearestNode( pev->origin, this );
	iThreatNode = WorldGraph.FindNearestNode ( vecThreat, this );
	iMyHullIndex = WorldGraph.HullIndex( this );

	if ( iMyNode == NO_NODE )
	{
		ALERT ( at_aiconsole, "FindCoverGrenade() - %s has no nearest node!\n", STRING( pev->classname ) );
		return FALSE;
	}

	if ( iThreatNode == NO_NODE )
		iThreatNode = iMyNode;

	vecLookersOffset = vecThreat + vecViewOffset;// calculate location of enemy's eyes

	for ( i = 0 ; i < WorldGraph.m_cNodes ; i++ )
	{
		int nodeNumber = ( i + WorldGraph.m_iLastCoverSearch ) % WorldGraph.m_cNodes;
		CNode &node = WorldGraph.Node( nodeNumber );

		if ( !( node.m_afNodeInfo & WorldGraph.NodeType( this ) ) )
			continue;

		WorldGraph.m_iLastCoverSearch = nodeNumber + 1; // next monster that searches for cover node will start where we left off here.

		flDist = ( pev->origin - node.m_vecOrigin ).Length();

		if ( flDist < flMinDist || flDist >= flMaxDist )
			continue;

		UTIL_TraceLine ( node.m_vecOrigin + vecViewOffset, vecLookersOffset, dont_ignore_monsters, dont_ignore_glass, ENT( pev ), &tr );

		// if this node will block the threat's line of sight to me...
		if ( tr.flFraction == 1.0 )
			continue;

		// ..and is also closer to me than the threat, or the same distance from myself and the threat the node is good.
		if ( ( iMyNode == iThreatNode )
			|| WorldGraph.PathLength( iMyNode, nodeNumber, iMyHullIndex, m_afCapability ) <= WorldGraph.PathLength( iThreatNode, nodeNumber, iMyHullIndex, m_afCapability ) )
		{
			if ( FValidateCover ( node.m_vecOrigin ) && MoveToCover( ACT_RUN, 0, node.m_vecOrigin, 0 ) )
				return TRUE;
		}
	}

	return FALSE;
}

//=========================================================
// BuildNearestRoute - tries to build a route as close to the target
// as possible, even if there isn't a path to the final point.
//
// If supplied, search will return a node at least as far
// away as MinDist from vecThreat, but no farther than MaxDist. 
// if MaxDist isn't supplied, it defaults to a reasonable 
// value
//=========================================================
BOOL CBaseMonster :: BuildNearestRoute ( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist )
{
	int i;
	int iMyHullIndex;
	int iMyNode;
	float flDist;
	Vector	vecLookersOffset;
	TraceResult tr;

	if ( !flMaxDist )
	{
		// user didn't supply a MaxDist, so work up a crazy one.
		flMaxDist = 784;
	}

	if ( flMinDist > 0.5 * flMaxDist)
	{
#if _DEBUG
		ALERT ( at_console, "FindCover MinDist (%.0f) too close to MaxDist (%.0f)\n", flMinDist, flMaxDist );
#endif
		flMinDist = 0.5 * flMaxDist;
	}

	if ( !WorldGraph.m_fGraphPresent || !WorldGraph.m_fGraphPointersSet )
	{
		ALERT ( at_aiconsole, "Graph not ready for BuildNearestRoute!\n" );
		return FALSE;
	}

	iMyNode = WorldGraph.FindNearestNode( pev->origin, this );
	iMyHullIndex = WorldGraph.HullIndex( this );

	if ( iMyNode == NO_NODE )
	{
		ALERT ( at_aiconsole, "BuildNearestRoute() - %s has no nearest node!\n", STRING(pev->classname));
		return FALSE;
	}

	vecLookersOffset = vecThreat + vecViewOffset;// calculate location of enemy's eyes

	// we'll do a rough sample to find nodes that are relatively nearby
	for ( i = 0 ; i < WorldGraph.m_cNodes ; i++ )
	{
		int nodeNumber = (i + WorldGraph.m_iLastCoverSearch) % WorldGraph.m_cNodes;

		CNode &node = WorldGraph.Node( nodeNumber );
		WorldGraph.m_iLastCoverSearch = nodeNumber + 1; // next monster that searches for cover node will start where we left off here.

		// can I get there?
		if (WorldGraph.NextNodeInRoute( iMyNode, nodeNumber, iMyHullIndex, 0 ) != iMyNode)
		{
			flDist = ( vecThreat - node.m_vecOrigin ).Length();

			// is it close?
			if ( flDist > flMinDist && flDist < flMaxDist)
			{
				// can I see where I want to be from there?
				UTIL_TraceLine( node.m_vecOrigin + pev->view_ofs, vecLookersOffset, ignore_monsters, edict(), &tr );

				if (tr.flFraction == 1.0)
				{
					// try to actually get there
					if ( BuildRoute ( node.m_vecOrigin, bits_MF_TO_LOCATION, NULL ) )
					{
						flMaxDist = flDist;
						m_vecMoveGoal = node.m_vecOrigin;
						return TRUE; // UNDONE: keep looking for something closer!
					}
				}
			}
		}
	}

	return FALSE;
}



//=========================================================
// BestVisibleEnemy - this functions searches the link
// list whose head is the caller's m_pLink field, and returns
// a pointer to the enemy entity in that list that is nearest the 
// caller.
//
// !!!UNDONE - currently, this only returns the closest enemy.
// we'll want to consider distance, relationship, attack types, back turned, etc.
//=========================================================
CBaseEntity *CBaseMonster :: BestVisibleEnemy ( void )
{
	CBaseEntity	*pReturn;
	CBaseEntity	*pNextEnt;
	int			iNearest;
	int			iDist;
	int			iBestRelationship;

	iNearest = 8192;// so first visible entity will become the closest.
	pNextEnt = m_pLink;
	pReturn = NULL;
	iBestRelationship = R_NO;

	while ( pNextEnt != NULL )
	{
		if ( pNextEnt->IsAlive() )
		{
			if ( IRelationship( pNextEnt) > iBestRelationship )
			{
				// this entity is disliked MORE than the entity that we 
				// currently think is the best visible enemy. No need to do 
				// a distance check, just get mad at this one for now.
				iBestRelationship = IRelationship ( pNextEnt );
				iNearest = ( pNextEnt->pev->origin - pev->origin ).Length();
				pReturn = pNextEnt;
			}
			else if ( IRelationship( pNextEnt) == iBestRelationship )
			{
				// this entity is disliked just as much as the entity that
				// we currently think is the best visible enemy, so we only
				// get mad at it if it is closer.
				iDist = ( pNextEnt->pev->origin - pev->origin ).Length();
				
				if ( iDist <= iNearest )
				{
					iNearest = iDist;
					iBestRelationship = IRelationship ( pNextEnt );
					pReturn = pNextEnt;
				}
			}
		}

		pNextEnt = pNextEnt->m_pLink;
	}

	return pReturn;
}


//=========================================================
// MakeIdealYaw - gets a yaw value for the caller that would
// face the supplied vector. Value is stuffed into the monster's
// ideal_yaw
//=========================================================
void CBaseMonster :: MakeIdealYaw( Vector vecTarget )
{
	Vector	vecProjection( 0, 0, 0 );

	// strafing monster needs to face 90 degrees away from its goal
	if ( m_movementActivity == ACT_STRAFE_LEFT )
	{
		vecProjection.x = -vecTarget.y;
		vecProjection.y = vecTarget.x;

		pev->ideal_yaw = UTIL_VecToYaw( vecProjection - pev->origin );
	}
	else if ( m_movementActivity == ACT_STRAFE_RIGHT )
	{
		vecProjection.x = vecTarget.y;
		vecProjection.y = vecTarget.x;

		pev->ideal_yaw = UTIL_VecToYaw( vecProjection - pev->origin );
	}
	else
	{
		pev->ideal_yaw = UTIL_VecToYaw ( vecTarget - pev->origin );
	}
}

//=========================================================
// FlYawDiff - returns the difference ( in degrees ) between
// monster's current yaw and ideal_yaw
//
// Positive result is left turn, negative is right turn
//=========================================================
float	CBaseMonster::FlYawDiff ( void )
{
	float	flCurrentYaw;

	flCurrentYaw = UTIL_AngleMod( pev->angles.y );

	if ( flCurrentYaw == pev->ideal_yaw )
	{
		return 0;
	}


	return UTIL_AngleDiff( pev->ideal_yaw, flCurrentYaw );
}


//=========================================================
// Changeyaw - turns a monster towards its ideal_yaw
//=========================================================
float CBaseMonster::ChangeYaw ( int yawSpeed )
{
	float		ideal, current, move, speed;

	current = UTIL_AngleMod( pev->angles.y );
	ideal = UTIL_AngleMod( pev->ideal_yaw );
	if (current != ideal)
	{
		// turn rate is per second of monster think time, not per server frame
		speed = (float)( yawSpeed * 2 ) * m_flFrameTime;
		move = ideal - current;

		if (ideal > current)
		{
			if (move >= 180)
				move = move - 360;
		}
		else
		{
			if (move <= -180)
				move = move + 360;
		}

		if (move > 0)
		{// turning to the monster's left
			if (move > speed)
				move = speed;
		}
		else
		{// turning to the monster's right
			if (move < -speed)
				move = -speed;
		}
		
		pev->angles.y = UTIL_AngleMod (current + move);

		// turn head in desired direction only if they have a turnable head
		if (m_afCapability & bits_CAP_TURN_HEAD)
		{
			float yaw = pev->ideal_yaw - pev->angles.y;
			if (yaw > 180) yaw -= 360;
			if (yaw < -180) yaw += 360;
			// yaw *= 0.8;
			SetBoneController( 0, yaw );
		}
	}
	else
		move = 0;

	return move;
}

//=========================================================
// VecToYaw - turns a directional vector into a yaw value
// that points down that vector.
//=========================================================
float	CBaseMonster::VecToYaw ( Vector vecDir )
{
	if (vecDir.x == 0 && vecDir.y == 0 && vecDir.z == 0)
		return pev->angles.y;

	return UTIL_VecToYaw( vecDir );
}


//=========================================================
// SetEyePosition
//
// queries the monster's model for $eyeposition and copies
// that vector to the monster's view_ofs
//
//=========================================================
void CBaseMonster :: SetEyePosition ( void )
{
	Vector  vecEyePosition;
	void	*pmodel = GET_MODEL_PTR( ENT(pev) );

	GetEyePosition( pmodel, vecEyePosition );

	pev->view_ofs = vecEyePosition;

	if ( pev->view_ofs == g_vecZero )
	{
		ALERT ( at_aiconsole, "%s has no view_ofs!\n", STRING ( pev->classname ) );
	}
}

void CBaseMonster :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case SCRIPT_EVENT_DEAD:
		if ( m_MonsterState == MONSTERSTATE_SCRIPT )
		{
			pev->deadflag = DEAD_DYING;
			// Kill me now! (and fade out when CineCleanup() is called)
#if _DEBUG
			ALERT( at_aiconsole, "Death event: %s\n", STRING(pev->classname) );
#endif
			pev->health = 0;
		}
#if _DEBUG
		else
			ALERT( at_aiconsole, "INVALID death event:%s\n", STRING(pev->classname) );
#endif
		break;
	case SCRIPT_EVENT_NOT_DEAD:
		if ( m_MonsterState == MONSTERSTATE_SCRIPT )
		{
			pev->deadflag = DEAD_NO;
			// This is for life/death sequences where the player can determine whether a character is dead or alive after the script 
			pev->health = pev->max_health;
		}
		break;

	case SCRIPT_EVENT_SOUND:			// Play a named wave file
		EMIT_SOUND( edict(), CHAN_BODY, pEvent->options, 1.0, ATTN_IDLE );
		break;

	case SCRIPT_EVENT_SOUND_VOICE:
		EMIT_SOUND( edict(), CHAN_VOICE, pEvent->options, 1.0, ATTN_IDLE );
		break;

	case SCRIPT_EVENT_SENTENCE_RND1:		// Play a named sentence group 33% of the time
		if (RANDOM_LONG(0,2) == 0)
			break;
		// fall through...
	case SCRIPT_EVENT_SENTENCE:			// Play a named sentence group
		SENTENCEG_PlayRndSz( edict(), pEvent->options, 1.0, ATTN_IDLE, 0, 100 );
		break;

	case SCRIPT_EVENT_FIREEVENT:		// Fire a trigger
		FireTargets( pEvent->options, this, this, USE_TOGGLE, 0 );
		break;

	case SCRIPT_EVENT_NOINTERRUPT:		// Can't be interrupted from now on
		if ( m_pCine )
			m_pCine->AllowInterrupt( FALSE );
		break;

	case SCRIPT_EVENT_CANINTERRUPT:		// OK to interrupt now
		if ( m_pCine )
			m_pCine->AllowInterrupt( TRUE );
		break;

#if 0
	case SCRIPT_EVENT_INAIR:			// Don't DROP_TO_FLOOR()
	case SCRIPT_EVENT_ENDANIMATION:		// Set ending animation sequence to
		break;
#endif

	case MONSTER_EVENT_BODYDROP_HEAVY:
		if ( pev->flags & FL_ONGROUND )
		{
			if ( RANDOM_LONG( 0, 1 ) == 0 )
			{
				EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "common/bodydrop3.wav", 1, ATTN_NORM, 0, 90 );
			}
			else
			{
				EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "common/bodydrop4.wav", 1, ATTN_NORM, 0, 90 );
			}

			m_fBodyDropEvent = true;
		}
		break;

	case MONSTER_EVENT_BODYDROP_LIGHT:
		if ( pev->flags & FL_ONGROUND )
		{
			if ( RANDOM_LONG( 0, 1 ) == 0 )
			{
				EMIT_SOUND( ENT(pev), CHAN_BODY, "common/bodydrop3.wav", 1, ATTN_NORM );
			}
			else
			{
				EMIT_SOUND( ENT(pev), CHAN_BODY, "common/bodydrop4.wav", 1, ATTN_NORM );
			}

			m_fBodyDropEvent = true;
		}
		break;

	case MONSTER_EVENT_FOOTSTEP:
		if ( pev->flags & FL_ONGROUND )
			HandleStep();
		break;

	case MONSTER_EVENT_SWISHSOUND:
		{
			// NO MONSTER may use this anim event unless that monster's precache precaches this sound!!!
			EMIT_SOUND( ENT(pev), CHAN_BODY, "zombie/claw_miss2.wav", 1, ATTN_NORM );
			break;
		}

	default:
		ALERT( at_aiconsole, "Unhandled animation event %d for %s\n", pEvent->event, STRING(pev->classname) );
		break;

	}
}


// Combat

Vector CBaseMonster :: GetGunPosition( )
{
	UTIL_MakeVectors(pev->angles);

	// Vector vecSrc = pev->origin + gpGlobals->v_forward * 10;
	//vecSrc.z = pevShooter->absmin.z + pevShooter->size.z * 0.7;
	//vecSrc.z = pev->origin.z + (pev->view_ofs.z - 4);
	Vector vecSrc = pev->origin 
					+ gpGlobals->v_forward * m_HackedGunPos.y 
					+ gpGlobals->v_right * m_HackedGunPos.x 
					+ gpGlobals->v_up * m_HackedGunPos.z;

	return vecSrc;
}





//=========================================================
// NODE GRAPH
//=========================================================





//=========================================================
// FGetNodeRoute - tries to build an entire node path from
// the callers origin to the passed vector. If this is 
// possible, ROUTE_SIZE waypoints will be copied into the
// callers m_Route. TRUE is returned if the operation 
// succeeds (path is valid) or FALSE if failed (no path 
// exists )
//=========================================================
BOOL CBaseMonster :: FGetNodeRoute ( Vector vecDest )
{
	int iPath[ MAX_PATH_SIZE ];
	int iSrcNode, iDestNode;
	int iResult;
	int i;
	int iNumToCopy;

	iSrcNode = WorldGraph.FindNearestNode ( pev->origin, this );
	iDestNode = WorldGraph.FindNearestNode ( vecDest, this );

	if ( iSrcNode == -1 )
	{
		// no node nearest self
//		ALERT ( at_aiconsole, "FGetNodeRoute: No valid node near self!\n" );
		return FALSE;
	}
	else if ( iDestNode == -1 )
	{
		// no node nearest target
//		ALERT ( at_aiconsole, "FGetNodeRoute: No valid node near target!\n" );
		return FALSE;
	}

	// valid src and dest nodes were found, so it's safe to proceed with
	// find shortest path
	int iNodeHull = WorldGraph.HullIndex( this ); // make this a monster virtual function
	iResult = WorldGraph.FindShortestPath ( iPath, iSrcNode, iDestNode, iNodeHull, m_afCapability );

	if ( !iResult )
	{
#if 1
		ALERT ( at_aiconsole, "No Path from %d to %d!\n", iSrcNode, iDestNode );
		return FALSE;
#else
		BOOL bRoutingSave = WorldGraph.m_fRoutingComplete;
		WorldGraph.m_fRoutingComplete = FALSE;
		iResult = WorldGraph.FindShortestPath(iPath, iSrcNode, iDestNode, iNodeHull, m_afCapability);
		WorldGraph.m_fRoutingComplete = bRoutingSave;
		if ( !iResult )
		{
			ALERT ( at_aiconsole, "No Path from %d to %d!\n", iSrcNode, iDestNode );
			return FALSE;
		}
		else
		{
			ALERT ( at_aiconsole, "Routing is inconsistent!" );
		}
#endif
	}

	// there's a valid path within iPath now, so now we will fill the route array
	// up with as many of the waypoints as it will hold.
	
	// don't copy ROUTE_SIZE entries if the path returned is shorter
	// than ROUTE_SIZE!!!
	if ( iResult < ROUTE_SIZE )
	{
		iNumToCopy = iResult;
	}
	else
	{
		iNumToCopy = ROUTE_SIZE;
	}
	
	for ( i = 0 ; i < iNumToCopy; i++ )
	{
		m_Route[ i ].vecLocation = WorldGraph.m_pNodes[ iPath[ i ] ].m_vecOrigin;
		m_Route[ i ].iType = bits_MF_TO_NODE;
	}
	
	if ( iNumToCopy < ROUTE_SIZE )
	{
		m_Route[ iNumToCopy ].vecLocation = vecDest;
		m_Route[ iNumToCopy ].iType |= bits_MF_IS_GOAL;
	}

	return TRUE;
}

//=========================================================
// FindHintNode
//=========================================================
int CBaseMonster :: FindHintNode ( void )
{
	int i;
	TraceResult tr;

	if ( !WorldGraph.m_fGraphPresent )
	{
		ALERT ( at_aiconsole, "find_hintnode: graph not ready!\n" );
		return NO_NODE;
	}

	if ( WorldGraph.m_iLastActiveIdleSearch >= WorldGraph.m_cNodes )
	{
		WorldGraph.m_iLastActiveIdleSearch = 0;
	}

	for ( i = 0; i < WorldGraph.m_cNodes ; i++ )
	{
		int nodeNumber = (i + WorldGraph.m_iLastActiveIdleSearch) % WorldGraph.m_cNodes;
		CNode &node = WorldGraph.Node( nodeNumber );

		if ( node.m_sHintType )
		{
			// this node has a hint. Take it if it is visible, the monster likes it, and the monster has an animation to match the hint's activity.
			if ( FValidateHintType ( node.m_sHintType ) )
			{
				if ( !node.m_sHintActivity || LookupActivity ( node.m_sHintActivity ) != ACTIVITY_NOT_AVAILABLE )
				{
					UTIL_TraceLine ( pev->origin + pev->view_ofs, node.m_vecOrigin + pev->view_ofs, ignore_monsters, ENT(pev), &tr );

					if ( tr.flFraction == 1.0 )
					{
						WorldGraph.m_iLastActiveIdleSearch = nodeNumber + 1; // next monster that searches for hint nodes will start where we left off.
						return nodeNumber;// take it!
					}
				}
			}
		}
	}

	WorldGraph.m_iLastActiveIdleSearch = 0;// start at the top of the list for the next search.

	return NO_NODE;
}
			

void CBaseMonster::ReportAIState( void )
{
	ALERT_TYPE level = at_console;

	static const char *pStateNames[] = { "None", "Idle", "Combat", "Alert", "Hunt", "Prone", "Scripted", "Dead" };

	ALERT( level, "%s: ", STRING(pev->classname) );
	if ( (int)m_MonsterState < ARRAYSIZE(pStateNames) )
		ALERT( level, "State: %s, ", pStateNames[m_MonsterState] );
	int i = 0;
	while ( activity_map[i].type != 0 )
	{
		if ( activity_map[i].type == (int)m_Activity )
		{
			ALERT( level, "Activity %s, ", activity_map[i].name );
			break;
		}
		i++;
	}

	if ( m_pSchedule )
	{
		const char *pName = NULL;
		pName = m_pSchedule->pName;
		if ( !pName )
			pName = "Unknown";
		ALERT( level, "Schedule %s, ", pName );
		Task_t *pTask = GetTask();
		if ( pTask )
			ALERT( level, "Task %d (#%d), ", pTask->iTask, m_iScheduleIndex );
	}
	else
		ALERT( level, "No Schedule, " );

	if ( m_hEnemy != NULL )
		ALERT( level, "\nEnemy is %s", STRING(m_hEnemy->pev->classname) );
	else
		ALERT( level, "No enemy" );

	if ( IsMoving() )
	{
		ALERT( level, " Moving " );
		if ( m_flMoveWaitFinished > gpGlobals->time )
			ALERT( level, ": Stopped for %.2f. ", m_flMoveWaitFinished - gpGlobals->time );
		else if ( m_IdealActivity == GetStoppedActivity() )
			ALERT( level, ": In stopped anim. " );
	}

	CSquadMonster *pSquadMonster = MySquadMonsterPointer();

	if ( pSquadMonster )
	{
		if ( !pSquadMonster->InSquad() )
		{
			ALERT ( level, "not " );
		}

		ALERT ( level, "In Squad, " );

		if ( !pSquadMonster->IsLeader() )
		{
			ALERT ( level, "not " );
		}

		ALERT ( level, "Leader." );
	}

	ALERT( level, "\n" );
	ALERT( level, "Yaw speed:%3.1f,Health: %3.1f\n", pev->yaw_speed, pev->health );
	if ( pev->spawnflags & SF_MONSTER_PRISONER )
		ALERT( level, " PRISONER! " );
	if ( pev->spawnflags & SF_MONSTER_PREDISASTER )
		ALERT( level, " Pre-Disaster! " );
	ALERT( level, "\n" );
}

//=========================================================
// KeyValue
//
// !!! netname entvar field is used in squadmonster for groupname!!!
//=========================================================
void CBaseMonster :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "TriggerTarget"))
	{
		m_iszTriggerTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "TriggerCondition") )
	{
		m_iTriggerCondition = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "minhullsize" ) )
	{
		UTIL_StringToVector( pev->vuser1, pkvd->szValue, ' ' );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "maxhullsize" ) )
	{
		UTIL_StringToVector( pev->vuser2, pkvd->szValue, ' ' );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "bloodcolor" ) )
	{
		int color = atoi( pkvd->szValue );

		if ( color == -1 || color == 1 || color == 2 )
			m_iBloodColorOverride = color;

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "guard_ent" ) )
	{
		m_iszGuardEntName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "path_name" ) )
	{
		m_iszPathName = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "is_not_revivable" ) )
	{
		m_fIsNotRevivable = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "displayname" ) )
	{
		m_FormattedName = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "freeroam" ) )
	{
		int mode = atoi( pkvd->szValue );

		if ( mode == 1 )
			m_fFreeRoamMode1 = true;
		else if ( mode == 2 )
			m_fFreeRoamMode2 = true;

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "classify" ) )
	{
		SetClassification( atoi( pkvd->szValue ) );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "is_player_ally" ) )
	{
		SetPlayerAlly( atoi( pkvd->szValue ) == 1 );
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseToggle::KeyValue( pkvd );
	}
}

//=========================================================
// FCheckAITrigger - checks the monster's AI Trigger Conditions,
// if there is a condition, then checks to see if condition is 
// met. If yes, the monster's TriggerTarget is fired.
//
// Returns TRUE if the target is fired.
//=========================================================
BOOL CBaseMonster :: FCheckAITrigger ( void )
{
	BOOL fFireTarget;

	if ( m_iTriggerCondition == AITRIGGER_NONE )
	{
		// no conditions, so this trigger is never fired.
		return FALSE; 
	}

	fFireTarget = FALSE;

	switch ( m_iTriggerCondition )
	{
	case AITRIGGER_SEEPLAYER_ANGRY_AT_PLAYER:
		if ( m_hEnemy != NULL && m_hEnemy->IsPlayer() && HasConditions ( bits_COND_SEE_ENEMY ) )
		{
			fFireTarget = TRUE;
		}
		break;
	case AITRIGGER_SEEPLAYER_UNCONDITIONAL:
		if ( HasConditions ( bits_COND_SEE_CLIENT ) )
		{
			fFireTarget = TRUE;
		}
		break;
	case AITRIGGER_SEEPLAYER_NOT_IN_COMBAT:
		if ( HasConditions ( bits_COND_SEE_CLIENT ) && 
			 m_MonsterState != MONSTERSTATE_COMBAT	&& 
			 m_MonsterState != MONSTERSTATE_PRONE	&& 
			 m_MonsterState != MONSTERSTATE_SCRIPT)
		{
			fFireTarget = TRUE;
		}
		break;
	case AITRIGGER_TAKEDAMAGE:
		if ( m_afConditions & ( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
		{
			fFireTarget = TRUE;
		}
		break;
	case AITRIGGER_DEATH:
		if ( pev->deadflag != DEAD_NO )
		{
			fFireTarget = TRUE;
		}
		break;
	case AITRIGGER_HALFHEALTH:
		if ( IsAlive() && pev->health <= ( pev->max_health / 2 ) )
		{
			fFireTarget = TRUE;
		}
		break;
/*

  // !!!UNDONE - no persistant game state that allows us to track these two. 

	case AITRIGGER_SQUADMEMBERDIE:
		break;
	case AITRIGGER_SQUADLEADERDIE:
		break;
*/
	case AITRIGGER_HEARWORLD:
		if ( m_afConditions & bits_COND_HEAR_SOUND && m_afSoundTypes & bits_SOUND_WORLD )
		{
			fFireTarget = TRUE;
		}
		break;
	case AITRIGGER_HEARPLAYER:
		if ( m_afConditions & bits_COND_HEAR_SOUND && m_afSoundTypes & bits_SOUND_PLAYER )
		{
			fFireTarget = TRUE;
		}
		break;
	case AITRIGGER_HEARCOMBAT:
		if ( m_afConditions & bits_COND_HEAR_SOUND && m_afSoundTypes & bits_SOUND_COMBAT )
		{
			fFireTarget = TRUE;
		}
		break;
	}

	if ( fFireTarget )
	{
		// fire the target, then set the trigger conditions to NONE so we don't fire again
		ALERT ( at_aiconsole, "AI Trigger Fire Target\n" );
		FireTargets( STRING( m_iszTriggerTarget ), this, this, USE_TOGGLE, 0 );
		m_iTriggerCondition = AITRIGGER_NONE;
		return TRUE;
	}

	return FALSE;
}

//=========================================================	
// CanPlaySequence - determines whether or not the monster
// can play the scripted sequence or AI sequence that is 
// trying to possess it. If DisregardState is set, the monster
// will be sucked into the script no matter what state it is
// in. ONLY Scripted AI ents should allow this.
//=========================================================	
int CBaseMonster :: CanPlaySequence( BOOL fDisregardMonsterState, int interruptLevel )
{
	if ( m_pCine || !IsAlive() || m_MonsterState == MONSTERSTATE_PRONE )
	{
		// monster is already running a scripted sequence or dead!
		return FALSE;
	}
	
	if ( fDisregardMonsterState )
	{
		// ok to go, no matter what the monster state. (scripted AI)
		return TRUE;
	}

	if ( m_MonsterState == MONSTERSTATE_NONE || m_MonsterState == MONSTERSTATE_IDLE || m_IdealMonsterState == MONSTERSTATE_IDLE )
	{
		// ok to go, but only in these states
		return TRUE;
	}
	
	if ( m_MonsterState == MONSTERSTATE_ALERT && interruptLevel >= SS_INTERRUPT_BY_NAME )
		return TRUE;

	// unknown situation
	return FALSE;
}


//=========================================================
// FindAttackPoint - FindCover turned around: a node within
// range of the threat (or of us, without an enemy) that the
// threat's eyes can see, with no friend in the line of fire
//=========================================================
BOOL CBaseMonster :: FindAttackPoint ( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist )
{
	int i;
	int iMyNode;
	float flDist;
	Vector	vecLookersOffset;
	TraceResult tr;

	if ( !flMaxDist )
		flMaxDist = COVER_DEFAULT_MAX_DIST;

	if ( flMinDist > 0.5 * flMaxDist )
		flMinDist = 0.5 * flMaxDist;

	if ( !WorldGraph.m_fGraphPresent || !WorldGraph.m_fGraphPointersSet )
		return FALSE;

	iMyNode = WorldGraph.FindNearestNode( pev->origin, this );

	if ( iMyNode == NO_NODE )
		return FALSE;

	vecLookersOffset = vecThreat + vecViewOffset;// calculate location of enemy's eyes

	for ( i = 0 ; i < WorldGraph.m_cNodes ; i++ )
	{
		int nodeNumber = ( i + WorldGraph.m_iLastCoverSearch ) % WorldGraph.m_cNodes;
		CNode &node = WorldGraph.Node( nodeNumber );

		if ( !( node.m_afNodeInfo & WorldGraph.NodeType( this ) ) )
			continue;

		WorldGraph.m_iLastCoverSearch = nodeNumber + 1; // next monster that searches for cover node will start where we left off here.

		if ( m_hEnemy != NULL )
			flDist = ( vecThreat - node.m_vecOrigin ).Length();
		else
			flDist = ( pev->origin - node.m_vecOrigin ).Length();

		if ( flDist < flMinDist || flDist >= flMaxDist )
			continue;

		UTIL_TraceLine ( node.m_vecOrigin + vecViewOffset, vecLookersOffset, ignore_monsters, ignore_glass, ENT( pev ), &tr );

		if ( tr.flFraction == 1.0 && NoFriendlyFire( node.m_vecOrigin ) && MoveToLocation( ACT_RUN, 0, node.m_vecOrigin ) )
			return TRUE;
	}

	return FALSE;
}

//=========================================================
// FindLateralAttackPoint - steps out up to five times
// flDist (128 by default) to where the threat can be seen:
// sideways first while the enemy is close and in sight,
// otherwise forward first. A blocked step may still end
// somewhere usable, at least flDist from the threat
//=========================================================
#define LATERAL_ATTACK_DIST		128
#define LATERAL_ATTACK_CHECKS	5
#define LATERAL_ATTACK_DIRS		3

BOOL CBaseMonster :: FindLateralAttackPoint ( const Vector &vecThreat, const Vector &vecViewOffset )
{
	return FindLateralAttackPoint( vecThreat, vecViewOffset, 0 );
}

BOOL CBaseMonster :: FindLateralAttackPoint ( const Vector &vecThreat, const Vector &vecViewOffset, float flDist )
{
	Vector	vecStep[ LATERAL_ATTACK_DIRS ];
	int		iMove[ LATERAL_ATTACK_DIRS ];
	Vector	vecEndPosition;
	TraceResult tr;
	int		i, iDir;

	if ( m_hEnemy == NULL )
		return FALSE;

	UTIL_MakeVectors( UTIL_VecToAngles( m_hEnemy->Center() - pev->origin ) );

	if ( flDist <= 0 )
		flDist = LATERAL_ATTACK_DIST;

	Vector vecLeft = Vector( -gpGlobals->v_right.x, -gpGlobals->v_right.y, 0 ) * flDist;
	Vector vecRight = Vector( gpGlobals->v_right.x, gpGlobals->v_right.y, 0 ) * flDist;
	Vector vecForward = Vector( gpGlobals->v_forward.x, gpGlobals->v_forward.y, 0 ) * flDist;

	CheckEnemy( m_hEnemy );

	if ( !HasConditions( bits_COND_ENEMY_TOOFAR ) && HasConditions( bits_COND_SEE_ENEMY ) )
	{
		vecStep[0] = vecLeft;
		vecStep[1] = vecRight;
		vecStep[2] = vecForward;
	}
	else
	{
		vecStep[0] = vecForward;
		vecStep[1] = vecLeft;
		vecStep[2] = vecRight;
	}

	Vector vecLookersOffset = vecThreat + vecViewOffset;

	for ( iDir = 0; iDir < LATERAL_ATTACK_DIRS; iDir++ )
		iMove[ iDir ] = LOCALMOVE_VALID;

	for ( i = 1; i <= LATERAL_ATTACK_CHECKS; i++ )
	{
		for ( iDir = 0; iDir < LATERAL_ATTACK_DIRS; iDir++ )
		{
			// a direction that ran into a wall is given up
			if ( iMove[ iDir ] == LOCALMOVE_INVALID )
				continue;

			Vector vecTest = pev->origin + vecStep[ iDir ] * i;

			UTIL_TraceLine( vecLookersOffset, vecTest + pev->view_ofs, ignore_monsters, ignore_glass, ENT( pev ), &tr );

			if ( tr.flFraction != 1.0 )
				continue;

			iMove[ iDir ] = CheckLocalMove( pev->origin, vecTest, NULL, NULL, &vecEndPosition );

			if ( iMove[ iDir ] == LOCALMOVE_VALID )
			{
				if ( NoFriendlyFire( vecTest ) && MoveToLocation( ACT_RUN, 0, vecTest ) )
					return TRUE;

				continue;
			}

			if ( vecEndPosition == pev->origin || ( vecEndPosition - vecThreat ).Length() < flDist )
				continue;

			UTIL_TraceLine( vecLookersOffset, vecEndPosition + pev->view_ofs, ignore_monsters, ignore_glass, ENT( pev ), &tr );

			if ( tr.flFraction == 1.0 && NoFriendlyFire( vecEndPosition ) && MoveToLocation( ACT_RUN, 0, vecEndPosition ) )
				return TRUE;
		}
	}

	return FALSE;
}

//=========================================================
// CheckAttacksFromPosition - could we attack pTarget from
// vecPosition? One we can't see counts as yes
//=========================================================
BOOL CBaseMonster :: CheckAttacksFromPosition ( CBaseEntity *pTarget, float flDist, Vector vecPosition )
{
	Vector2D	vec2LOS;
	float		flDot;

	if ( !FVisible( pTarget, true ) )
		return TRUE;

	UTIL_MakeVectors( UTIL_VecToAngles( pTarget->pev->origin - vecPosition ) );

	vec2LOS = ( pTarget->pev->origin - vecPosition ).Make2D();
	vec2LOS = vec2LOS.Normalize();

	flDot = DotProduct ( vec2LOS , gpGlobals->v_forward.Make2D() );

	if ( FClassnameIs( pTarget->pev, "monster_alien_voltigore" ) )
		flDist -= VOLTIGORE_REACH;

	if ( ( m_afCapability & bits_CAP_RANGE_ATTACK1 ) && CheckRangeAttack1( flDot, flDist ) )
		return TRUE;

	if ( ( m_afCapability & bits_CAP_RANGE_ATTACK2 ) && CheckRangeAttack2( flDot, flDist ) )
		return TRUE;

	if ( ( m_afCapability & bits_CAP_MELEE_ATTACK1 ) && CheckMeleeAttack1( flDot, flDist ) )
		return TRUE;

	return ( m_afCapability & bits_CAP_MELEE_ATTACK2 ) && CheckMeleeAttack2( flDot, flDist );
}

//=========================================================
// FindRoamNode - the next node of our type, from where the
// last search stopped, that we can see from here
// TASK_FIND_HINTNODE uses this instead of FindHintNode
//=========================================================
int CBaseMonster :: FindRoamNode ( void )
{
	int i;
	TraceResult tr;

	if ( !WorldGraph.m_fGraphPresent )
	{
		ALERT ( at_aiconsole, "find_roamnode: graph not ready!\n" );
		return NO_NODE;
	}

	if ( WorldGraph.m_iLastActiveIdleSearch >= WorldGraph.m_cNodes )
		WorldGraph.m_iLastActiveIdleSearch = 0;

	for ( i = 0; i < WorldGraph.m_cNodes ; i++ )
	{
		int nodeNumber = ( i + WorldGraph.m_iLastActiveIdleSearch ) % WorldGraph.m_cNodes;
		CNode &node = WorldGraph.Node( nodeNumber );

		if ( !( node.m_afNodeInfo & WorldGraph.NodeType( this ) ) )
			continue;

		UTIL_TraceLine ( pev->origin + pev->view_ofs, node.m_vecOrigin + pev->view_ofs, ignore_monsters, ENT( pev ), &tr );

		if ( tr.flFraction == 1.0 )
		{
			WorldGraph.m_iLastActiveIdleSearch = nodeNumber + 1;
			return nodeNumber;
		}
	}

	WorldGraph.m_iLastActiveIdleSearch = 0;
	return NO_NODE;
}

//=========================================================
// TestBuildRoute / TestFGetNodeRoute - would BuildRoute and
// FGetNodeRoute find a way? Nothing is changed
//=========================================================
BOOL CBaseMonster :: TestBuildRoute ( const Vector &vecGoal, int iMoveFlag, CBaseEntity *pTarget )
{
	float	flDist;
	Vector	vecApex;
	Vector	vecApex2;
	int		iLocalMove = CheckLocalMove( pev->origin, vecGoal, pTarget, &flDist );

	if ( iLocalMove == LOCALMOVE_VALID )
		return TRUE;

	if ( iLocalMove != LOCALMOVE_INVALID_DONT_TRIANGULATE
		&& FTriangulate( pev->origin, vecGoal, flDist, pTarget, &vecApex, &vecApex2 ) )
	{
		return TRUE;
	}

	return TestFGetNodeRoute( vecGoal );
}

BOOL CBaseMonster :: TestFGetNodeRoute ( Vector vecDest )
{
	int iPath[ MAX_PATH_SIZE ];
	int iSrcNode = WorldGraph.FindNearestNode( pev->origin, this );
	int iDestNode = WorldGraph.FindNearestNode( vecDest, this );

	if ( iSrcNode == NO_NODE || iDestNode == NO_NODE )
		return FALSE;

	int iNodeHull = WorldGraph.HullIndex( this );

	if ( WorldGraph.FindShortestPath( iPath, iSrcNode, iDestNode, iNodeHull, m_afCapability ) )
		return TRUE;

	ALERT( at_aiconsole, "No Path from %d to %d!\n", iSrcNode, iDestNode );
	return FALSE;
}

//=========================================================
// FindLateralSafety - steps away from a threat: to either
// side of it, straight back, then along each world axis
// A blocked step still counts once it ends at least
// flMinDist from the threat
//=========================================================
#define LATERAL_SAFETY_DIST				2048	// used when flDist isn't given
#define LATERAL_SAFETY_CHECKS			7
#define LATERAL_SAFETY_GRENADE_SCALE	2.5		// times a hand grenade's damage

BOOL CBaseMonster :: FindLateralSafety ( const Vector &vecThreat, float flMinDist, float flDist )
{
	Vector	vecCheck[ LATERAL_SAFETY_CHECKS ];
	Vector	vecEndPosition;
	int		i;

	if ( flDist <= 0 )
		flDist = LATERAL_SAFETY_DIST;

	if ( vecThreat == pev->origin )
		UTIL_MakeVectors( pev->angles );
	else
		UTIL_MakeVectors( UTIL_VecToAngles( vecThreat - pev->origin ) );

	Vector vecRight = Vector( gpGlobals->v_right.x, gpGlobals->v_right.y, 0 ) * flDist;
	Vector vecForward = Vector( gpGlobals->v_forward.x, gpGlobals->v_forward.y, 0 ) * flDist;

	vecCheck[0] = pev->origin - vecRight;
	vecCheck[1] = pev->origin + vecRight;
	vecCheck[2] = pev->origin - vecForward;
	vecCheck[3] = pev->origin + Vector( flDist, 0, 0 );
	vecCheck[4] = pev->origin + Vector( 0, flDist, 0 );
	vecCheck[5] = pev->origin - Vector( flDist, 0, 0 );
	vecCheck[6] = pev->origin - Vector( 0, flDist, 0 );

	for ( i = 0; i < LATERAL_SAFETY_CHECKS; i++ )
	{
		if ( CheckLocalMove( pev->origin, vecCheck[i], NULL, NULL, &vecEndPosition ) == LOCALMOVE_VALID )
		{
			if ( MoveToLocation( ACT_RUN, 0, vecCheck[i] ) )
				return TRUE;
		}
		else if ( ( vecThreat - vecEndPosition ).Length() >= flMinDist && vecEndPosition != pev->origin )
		{
			if ( MoveToLocation( ACT_RUN, 0, vecEndPosition ) )
				return TRUE;
		}
	}

	return FALSE;
}

BOOL CBaseMonster :: FindLateralSafetyGrenade ( const Vector &vecThreat )
{
	return FindLateralSafety( vecThreat, LATERAL_SAFETY_GRENADE_SCALE * sk_plr_hand_grenade.value, 0 );
}

//=========================================================
// FindLateralCover - attempts to locate a spot in the world
// directly to the left or right of the caller that will
// conceal them from view of pSightEnt
//=========================================================
#define	COVER_CHECKS	5// how many checks are made
#define COVER_DELTA		48// distance between checks
#define COVER_FAR_DIST	1024

// a spot out of the threat's sight, or the end of a blocked move there if that is hidden too
BOOL TryLateralCoverSpot( CBaseMonster *pMonster, const Vector &vecThreat, const Vector &vecViewOffset, const Vector &vecTest, bool fSkipIfStuck )
{
	TraceResult	tr;
	Vector		vecEnd = g_vecZero;
	entvars_t	*pev = pMonster->pev;

	// it's faster to check the SightEnt's visibility to the potential spot than to check the local move, so we do that first.
	UTIL_TraceLine( vecThreat + vecViewOffset, vecTest + pev->view_ofs, ignore_monsters, ignore_glass, ENT(pev)/*pentIgnore*/, &tr );

	if ( tr.flFraction == 1.0 )
		return FALSE;

	if ( pMonster->CheckLocalMove( pev->origin, vecTest, NULL, NULL, &vecEnd, false ) >= LOCALMOVE_VALID )
		return pMonster->MoveToLocation( ACT_RUN, 0, vecTest );

	if ( fSkipIfStuck && vecEnd == pev->origin )
		return FALSE;

	UTIL_TraceLine( vecThreat + vecViewOffset, vecEnd + pev->view_ofs, ignore_monsters, ignore_glass, ENT(pev)/*pentIgnore*/, &tr );

	if ( tr.flFraction == 1.0 )
		return FALSE;

	return pMonster->MoveToLocation( ACT_RUN, 0, vecEnd );
}

BOOL CBaseMonster :: FindLateralCover ( const Vector &vecThreat, const Vector &vecViewOffset )
{
	Vector	vecLeftTest;
	Vector	vecRightTest;
	Vector	vecStepRight;
	int		i;

	UTIL_MakeVectors ( pev->angles );
	vecStepRight = gpGlobals->v_right * COVER_DELTA;
	vecStepRight.z = 0;

	vecLeftTest = vecRightTest = pev->origin;

	for ( i = 0 ; i < COVER_CHECKS ; i++ )
	{
		vecLeftTest = vecLeftTest - vecStepRight;
		vecRightTest = vecRightTest + vecStepRight;

		if ( TryLateralCoverSpot( this, vecThreat, vecViewOffset, vecLeftTest, false ) )
			return TRUE;

		if ( TryLateralCoverSpot( this, vecThreat, vecViewOffset, vecRightTest, false ) )
			return TRUE;
	}

	// nothing close by, so try as far as we could go to either side
	if ( TryLateralCoverSpot( this, vecThreat, vecViewOffset, pev->origin - gpGlobals->v_right * COVER_FAR_DIST, true ) )
		return TRUE;

	return TryLateralCoverSpot( this, vecThreat, vecViewOffset, pev->origin + gpGlobals->v_right * COVER_FAR_DIST, true );
}


Vector CBaseMonster :: ShootAtEnemy( const Vector &shootOrigin )
{
	edict_t *enemyEdict = m_hEnemy.Get();

	CBaseEntity *pEnemy;

	if ( enemyEdict && !enemyEdict->free )
		pEnemy = (CBaseEntity *) GET_PRIVATE( enemyEdict );
	else
		pEnemy = NULL;

	if ( pEnemy )
	{
		if ( pEnemy->IsBSPModel() )
			return ( m_vecEnemyLKP - shootOrigin ).Normalize();

		return ( (pEnemy->BodyTarget( shootOrigin ) - pEnemy->pev->origin) + m_vecEnemyLKP - shootOrigin ).Normalize();
	}
	else
		return gpGlobals->v_forward;
}



//=========================================================
// FacingIdeal - tells us if a monster is facing its ideal
// yaw. Created this function because many spots in the 
// code were checking the yawdiff against this magic
// number. Nicer to have it in one place if we're gonna
// be stuck with it.
//=========================================================
BOOL CBaseMonster :: FacingIdeal( void )
{
	if ( fabs( FlYawDiff() ) <= 0.006 )//!!!BUGBUG - no magic numbers!!!
	{
		return TRUE;
	}

	return FALSE;
}

//=========================================================
// FCanActiveIdle
//=========================================================
BOOL CBaseMonster :: FCanActiveIdle ( void )
{
	if ( m_hEnemy != NULL
		 || ( m_MonsterState != MONSTERSTATE_IDLE && m_MonsterState != MONSTERSTATE_ALERT )
		 || IsMoving() )
		return FALSE;

	return ( g_iActiveIdle || ( pev->spawnflags & SF_MONSTER_FREE_ROAM_MODE2 ) )
		&& !( pev->spawnflags & SF_MONSTER_FREE_ROAM_MODE1 );
}

void CBaseMonster::PlaySentence( const char *pszSentence, float duration, float volume, float attenuation )
{
	if ( !pszSentence )
		return;

	// '!' names a sentence, '+' a plain sound file
	if ( pszSentence[0] == '!' )
		EMIT_SOUND_DYN( edict(), CHAN_VOICE, pszSentence, volume, attenuation, 0, PITCH_NORM );
	else if ( pszSentence[0] == '+' )
		EMIT_SOUND_DYN( edict(), CHAN_VOICE, pszSentence + 1, volume, attenuation, 0, PITCH_NORM );
	else
		SENTENCEG_PlayRndSz( edict(), pszSentence, volume, attenuation, 0, PITCH_NORM );
}


void CBaseMonster::PlayScriptedSentence( const char *pszSentence, float duration, float volume, float attenuation, BOOL bConcurrent, CBaseEntity *pListener )
{
	ClearConditions( bits_COND_CLIENT_PUSH );
	m_useTime = gpGlobals->time + duration;

	PlaySentence( pszSentence, duration, volume, attenuation );
}


void CBaseMonster::SentenceStop( void )
{
	EMIT_SOUND( edict(), CHAN_VOICE, "common/null.wav", 1.0, ATTN_IDLE );
}


void CBaseMonster::CorpseFallThink( void )
{
	if ( pev->flags & FL_ONGROUND )
	{
		SetThink ( NULL );

		SetSequenceBox( );
		UTIL_SetOrigin( pev, pev->origin );// link into world.
	}
	else
		pev->nextthink = gpGlobals->time + 0.1;
}

// Call after animation/pose is set up
void CBaseMonster :: MonsterInitDead( void )
{
	InitBoneControllers();
	pev->gaitsequence = GAIT_SEQUENCE_NONE;
	m_GaitActivity = ACT_RESET;
	pev->flags |= FL_MONSTER;

	pev->solid			= SOLID_BBOX;
	pev->movetype		= MOVETYPE_TOSS;// so he'll fall to ground

	pev->frame = 0;
	ResetSequenceInfo( );
	pev->framerate = 0;

	// Copy health
	pev->max_health		= pev->health;
	pev->deadflag		= DEAD_DEAD;

	UTIL_SetSize(pev, g_vecZero, g_vecZero );
	UTIL_SetOrigin( pev, pev->origin );

	// Setup health counters, etc.
	BecomeDead( 0 );
	SetThink( &CBaseMonster::CorpseFallThink );
	pev->nextthink = gpGlobals->time + 0.5;
}

//=========================================================
// BBoxIsFlat - check to see if the monster's bounding box
// is lying flat on a surface (traces from all four corners
// are same length.)
//=========================================================
BOOL CBaseMonster :: BBoxFlat ( void )
{
	TraceResult	tr;
	Vector		vecPoint;
	float		flXSize, flYSize;
	float		flLength;
	float		flLength2;

	flXSize = pev->size.x / 2;
	flYSize = pev->size.y / 2;

	vecPoint.x = pev->origin.x + flXSize;
	vecPoint.y = pev->origin.y + flYSize;
	vecPoint.z = pev->origin.z;

	UTIL_TraceLine ( vecPoint, vecPoint - Vector ( 0, 0, 100 ), ignore_monsters, ENT(pev), &tr );
	flLength = (vecPoint - tr.vecEndPos).Length();

	vecPoint.x = pev->origin.x - flXSize;
	vecPoint.y = pev->origin.y - flYSize;

	UTIL_TraceLine ( vecPoint, vecPoint - Vector ( 0, 0, 100 ), ignore_monsters, ENT(pev), &tr );
	flLength2 = (vecPoint - tr.vecEndPos).Length();
	if ( flLength2 > flLength )
	{
		return FALSE;
	}
	flLength = flLength2;

	vecPoint.x = pev->origin.x - flXSize;
	vecPoint.y = pev->origin.y + flYSize;
	UTIL_TraceLine ( vecPoint, vecPoint - Vector ( 0, 0, 100 ), ignore_monsters, ENT(pev), &tr );
	flLength2 = (vecPoint - tr.vecEndPos).Length();
	if ( flLength2 > flLength )
	{
		return FALSE;
	}
	flLength = flLength2;

	vecPoint.x = pev->origin.x + flXSize;
	vecPoint.y = pev->origin.y - flYSize;
	UTIL_TraceLine ( vecPoint, vecPoint - Vector ( 0, 0, 100 ), ignore_monsters, ENT(pev), &tr );
	flLength2 = (vecPoint - tr.vecEndPos).Length();
	if ( flLength2 > flLength )
	{
		return FALSE;
	}
	flLength = flLength2;

	return TRUE;
}

//=========================================================
// Get Enemy - tries to find the best suitable enemy for the monster.
//=========================================================
BOOL CBaseMonster :: GetEnemy ( void )
{
	CBaseEntity *pNewEnemy;

	if ( HasConditions(bits_COND_SEE_HATE | bits_COND_SEE_FEAR | bits_COND_SEE_DISLIKE | bits_COND_SEE_NEMESIS) )
	{
		pNewEnemy = BestVisibleEnemy();

		// stay on a visible enemy unless the new one is more hated and in reach
		if ( m_hEnemy != NULL && pNewEnemy != NULL && pNewEnemy != m_hEnemy && FVisible( m_hEnemy, true )
			&& IRelationship( m_hEnemy, false ) >= IRelationship( pNewEnemy, false )
			&& (int) ( pNewEnemy->pev->origin - pev->origin ).Length() > m_flDistTooFar )
		{
			pNewEnemy = NULL;
		}

		// an enemy just out of sight is still the one to beat if it is closer
		if ( m_hEnemy != NULL && pNewEnemy != NULL && m_hEnemy->IsAlive() && !FVisible( m_hEnemy, true )
			&& m_flEnemySeenTime > gpGlobals->time
			&& (int) ( m_hEnemy->pev->origin - pev->origin ).Length() < (int) ( pNewEnemy->pev->origin - pev->origin ).Length() )
		{
			pNewEnemy = NULL;
		}

		if ( pNewEnemy != m_hEnemy && pNewEnemy != NULL)
		{
			// DO NOT mess with the monster's m_hEnemy pointer unless the schedule the monster is currently running will be interrupted
			// by COND_NEW_ENEMY. This will eliminate the problem of monsters getting a new enemy while they are in a schedule that doesn't care,
			// and then not realizing it by the time they get to a schedule that does. I don't feel this is a good permanent fix. 

			if ( m_pSchedule )
			{
				if ( m_pSchedule->iInterruptMask & bits_COND_NEW_ENEMY )
				{
					PushEnemy( m_hEnemy, m_vecEnemyLKP );
					SetConditions(bits_COND_NEW_ENEMY);
					m_flNextTankSearchTime = gpGlobals->time;
					m_hEnemy = pNewEnemy;
					m_vecEnemyLKP = m_hEnemy->Center();
					MakeIdealYaw( m_vecEnemyLKP );
				}
				// if the new enemy has an owner, take that one as well
				if (pNewEnemy->pev->owner != NULL)
				{
					CBaseEntity *pOwner = GetMonsterPointer( pNewEnemy->pev->owner );
					if ( pOwner && (pOwner->pev->flags & FL_MONSTER) && IRelationship( pOwner ) != R_NO )
						PushEnemy( pOwner, m_vecEnemyLKP );
				}
			}
		}
	}

	// remember old enemies
	if (m_hEnemy == NULL && PopEnemy( ))
	{
		if ( m_pSchedule )
		{
			if ( m_pSchedule->iInterruptMask & bits_COND_NEW_ENEMY )
			{
				SetConditions(bits_COND_NEW_ENEMY);
				m_flNextTankSearchTime = gpGlobals->time;
			}
		}
	}

	if ( m_hEnemy != NULL && m_hEnemy->IsAlive() && IRelationship( m_hEnemy, false ) > R_NO )
	{
		// monster has an enemy.
		return TRUE;
	}

	return FALSE;// monster has no enemy
}


//=========================================================
// DropItem - dead monster drops named item 
//=========================================================
CBaseEntity *CBaseMonster::DropItem( CBaseEntity *item, const Vector &position, const Vector &angles )
{
	if ( item == NULL )
		return NULL;

	UTIL_SetOrigin( item->pev, position );
	item->pev->angles = angles;

	if ( pev->velocity.Length2D() == 0 )
	{
		float z = (float) RANDOM_LONG( 150, 200 );
		float y = (float) RANDOM_LONG( -200, 200 );
		float x = (float) RANDOM_LONG( -200, 200 );

		item->pev->velocity = Vector( x, y, z );
	}
	else
	{
		item->pev->velocity = pev->velocity;
	}

	CBasePickupObject *pickup = dynamic_cast<CBasePickupObject *>( item );

	if ( pickup )
		pickup->m_iUnknown180 = 3;

	return item;
}

CBaseEntity *CBaseMonster::DropWeapon( const char *name, bool force )
{
	if ( !name || !*name )
		return NULL;

	int dropWeapons = (int) CVAR_GET_FLOAT( "npc_dropweapons" );

	if ( dropWeapons < 0 || ( dropWeapons == 0 && !force ) )
		return NULL;

	Vector position( 0, 0, 0 );
	Vector angles( 0, 0, 0 );

	GetAttachment( 0, position, angles );

	const CString &mapped = CClassicMode::GetInstance()->FindMappedItem( name );

	if ( !mapped.IsEmpty() )
		name = STRING( AllocStringLocal( mapped.Data() ) );

	CBaseEntity *item = CBaseEntity::Create( name, position, angles, true, edict() );

	if ( item == NULL )
		return NULL;

	item->pev->spawnflags |= SF_NORESPAWN | SF_DELAYREMOVE | SF_CREATEDWEAPON;
	DispatchSpawn( item->edict() );

	if ( item->MyItemPointer() || dynamic_cast<CBasePlayerAmmo *>( item ) )
	{
		DropItem( item, pev->origin, pev->angles );
		return item;
	}

	UTIL_Remove( item );
	return NULL;
}

CBaseEntity* CBaseMonster :: DropItem ( const char *pszItemName, const Vector &vecPos, const Vector &vecAng )
{
	if ( !pszItemName )
	{
		ALERT ( at_console, "DropItem() - No item name!\n" );
		return NULL;
	}

	CBaseEntity *pItem = CBaseEntity::Create( pszItemName, vecPos, vecAng, false, edict() );

	if ( pItem )
	{
		// do we want this behavior to be default?! (sjb)
		pItem->pev->velocity = pev->velocity;
		pItem->pev->avelocity = Vector ( 0, RANDOM_FLOAT( 0, 100 ), 0 );
		return pItem;
	}
	else
	{
		ALERT ( at_console, "DropItem() - Didn't create!\n" );
		return FALSE;
	}

}


BOOL CBaseMonster :: ShouldFadeOnDeath( void )
{
	return !(pev->spawnflags & SF_MONSTER_DONT_FADE );
}
