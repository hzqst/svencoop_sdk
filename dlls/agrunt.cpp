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
// Agrunt - Dominant, warlike alien grunt monster
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include "game.h"
#include	"schedule.h"
#include	"squadmonster.h"
#include	"weapons.h"
#include	"soundent.h"
#include	"hornet.h"
#include "agrunt.h"
#include "sqknest.h"
#include "CClassicMode.h"

extern cvar_t sk_agrunt_dmg_punch;
extern cvar_t sk_agrunt_berserker_dmg_punch;
extern cvar_t sk_agrunt_health;
extern cvar_t sk_agrunt_melee_engage_distance;

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_AGRUNT_SUPPRESS = LAST_COMMON_SCHEDULE + 1,
	SCHED_AGRUNT_THREAT_DISPLAY,
};

//=========================================================
// monster-specific tasks
//=========================================================
enum 
{
	TASK_AGRUNT_SETUP_HIDE_ATTACK = LAST_COMMON_TASK + 1,
	TASK_AGRUNT_GET_PATH_TO_ENEMY_CORPSE,
	TASK_AGRUNT_TELEPORT_FALL,
	TASK_AGRUNT_TELEPORT_LAND,
	TASK_AGRUNT_FACE_TOSS,
};

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		AGRUNT_AE_HORNET1	( 1 )
#define		AGRUNT_AE_HORNET2	( 2 )
#define		AGRUNT_AE_HORNET3	( 3 )
#define		AGRUNT_AE_HORNET4	( 4 )
#define		AGRUNT_AE_HORNET5	( 5 )
// some events are set up in the QC file that aren't recognized by the code yet.
#define		AGRUNT_AE_PUNCH		( 6 )
#define		AGRUNT_AE_BITE		( 7 )

#define		AGRUNT_AE_LEFT_FOOT	 ( 10 )
#define		AGRUNT_AE_RIGHT_FOOT ( 11 )

#define		AGRUNT_AE_LEFT_PUNCH ( 12 )
#define		AGRUNT_AE_RIGHT_PUNCH ( 13 )
#define AGRUNT_AE_SNARK ( 14 )



#define		AGRUNT_MELEE_DIST	90
#define		AGRUNT_BERSERKER_MELEE_DIST	75


LINK_ENTITY_TO_CLASS( monster_alien_grunt, CAGrunt );

void CAGrunt::DeathNotice( entvars_t *child )
{
	if ( FStrEq( STRING( child->classname ), "monster_snark" ) )
	{
		--m_iNumSnarks;
		return;
	}

	if ( FStrEq( STRING( child->classname ), "monster_sqknest" ) )
	{
		CSqkNest *nest = (CSqkNest *) CBaseEntity::Instance( child );

		if ( nest )
			m_iNumSnarks -= nest->m_iSnarkCount;
	}
}

TYPEDESCRIPTION	CAGrunt::m_SaveData[] = 
{
	DEFINE_FIELD( CAGrunt, m_fCanHornetAttack, FIELD_CHARACTER ),
	DEFINE_FIELD( CAGrunt, m_flNextHornetAttackCheck, FIELD_TIME ),
	DEFINE_FIELD( CAGrunt, m_flNextPainTime, FIELD_TIME ),
	DEFINE_FIELD( CAGrunt, m_flNextSpeakTime, FIELD_TIME ),
	DEFINE_FIELD( CAGrunt, m_flNextWordTime, FIELD_TIME ),
	DEFINE_FIELD( CAGrunt, m_iLastWord, FIELD_INTEGER ),
	DEFINE_FIELD( CAGrunt, m_flNextGrenadeCheck, FIELD_TIME ),
	DEFINE_FIELD( CAGrunt, m_vecTossVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( CAGrunt, m_fThrowGrenade, FIELD_CHARACTER ),
	DEFINE_FIELD( CAGrunt, m_iNumSnarks, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CAGrunt, CSquadMonster );

const char *CAGrunt::pAttackHitSounds[] = 
{
	"zombie/claw_strike1.wav",
	"zombie/claw_strike2.wav",
	"zombie/claw_strike3.wav",
};

const char *CAGrunt::pAttackMissSounds[] = 
{
	"zombie/claw_miss1.wav",
	"zombie/claw_miss2.wav",
};

const char *CAGrunt::pAttackSounds[] =
{
	"agrunt/ag_attack1.wav",
	"agrunt/ag_attack2.wav",
	"agrunt/ag_attack3.wav",
};

const char *CAGrunt::pDieSounds[] =
{
	"agrunt/ag_die1.wav",
	"agrunt/ag_die4.wav",
	"agrunt/ag_die5.wav",
};

const char *CAGrunt::pPainSounds[] =
{
	"agrunt/ag_pain1.wav",
	"agrunt/ag_pain2.wav",
	"agrunt/ag_pain3.wav",
	"agrunt/ag_pain4.wav",
	"agrunt/ag_pain5.wav",
};

const char *CAGrunt::pIdleSounds[] =
{
	"agrunt/ag_idle1.wav",
	"agrunt/ag_idle2.wav",
	"agrunt/ag_idle3.wav",
	"agrunt/ag_idle4.wav",
};

const char *CAGrunt::pAlertSounds[] =
{
	"agrunt/ag_alert1.wav",
	"agrunt/ag_alert3.wav",
	"agrunt/ag_alert4.wav",
	"agrunt/ag_alert5.wav",
};

//=========================================================
// IRelationship - overridden because Human Grunts are 
// Alien Grunt's nemesis.
//=========================================================
int CAGrunt::IRelationship ( CBaseEntity *pTarget, bool ignoreNoTarget )
{
	if ( pTarget && !pTarget->m_fOverrideClass && Classify() == CLASS_ALIEN_MILITARY
	     && FClassnameIs( pTarget->pev, "monster_human_grunt" ) )
	{
		return R_NM;
	}

	return CSquadMonster :: IRelationship( pTarget, ignoreNoTarget );
}

//=========================================================
// ISoundMask 
//=========================================================
int CAGrunt :: ISoundMask ( void )
{
	return	bits_SOUND_WORLD	|
			bits_SOUND_COMBAT	|
			bits_SOUND_PLAYER	|
			bits_SOUND_DANGER;
}

//=========================================================
// TraceAttack
//=========================================================
void CAGrunt :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	if ( ptr->iHitgroup == 10 && ( bitsDamageType & ( DMG_BULLET | DMG_SLASH | DMG_CLUB | DMG_POISON | DMG_ACID ) ) )
	{
		// hit armor
		float flArmorDamage = flDamage - 50;
		int bitsArmorDamageType = bitsDamageType & ~DMG_POISON;

		if ( flArmorDamage <= 0 )
		{
			if ( pev->dmgtime != gpGlobals->time || ( RANDOM_LONG( 0, 10 ) < 1 ) )
			{
				UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT( 1, 2 ) );

				if ( RANDOM_LONG( 0, 3 ) == 0 )
				{
					Vector vecTracerDir = vecDir;

					vecTracerDir.x += RANDOM_FLOAT( -0.3, 0.3 );
					vecTracerDir.y += RANDOM_FLOAT( -0.3, 0.3 );
					vecTracerDir.z += RANDOM_FLOAT( -0.3, 0.3 );

					vecTracerDir = vecTracerDir * -512;

					MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, ptr->vecEndPos );
					WRITE_BYTE( TE_TRACER );
						WRITE_COORD( ptr->vecEndPos.x );
						WRITE_COORD( ptr->vecEndPos.y );
						WRITE_COORD( ptr->vecEndPos.z );

						WRITE_COORD( vecTracerDir.x );
						WRITE_COORD( vecTracerDir.y );
						WRITE_COORD( vecTracerDir.z );
					MESSAGE_END();
				}

				pev->dmgtime = gpGlobals->time;
			}

			flArmorDamage = 0.1;// don't hurt the monster much, but allow bits_COND_LIGHT_DAMAGE to be generated
		}

		AddMultiDamage( pevAttacker, this, flArmorDamage, bitsArmorDamageType );
		return;
	}

	if ( bitsDamageType & DMG_BULLET )
		flDamage *= ( ptr->iHitgroup == HITGROUP_HEAD ) ? 0.75 : 0.5;

	CSquadMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

//=========================================================
// StopTalking - won't speak again for 10-20 seconds.
//=========================================================
void CAGrunt::StopTalking( void )
{
	m_flNextWordTime = m_flNextSpeakTime = gpGlobals->time + 10 + RANDOM_LONG(0, 10);
}

//=========================================================
// ShouldSpeak - Should this agrunt be talking?
//=========================================================
BOOL CAGrunt::ShouldSpeak( void )
{
	if ( m_flNextSpeakTime > gpGlobals->time )
	{
		// my time to talk is still in the future.
		return FALSE;
	}

	if ( pev->spawnflags & SF_MONSTER_GAG )
	{
		if ( m_MonsterState != MONSTERSTATE_COMBAT )
		{
			// if gagged, don't talk outside of combat.
			// if not going to talk because of this, put the talk time 
			// into the future a bit, so we don't talk immediately after 
			// going into combat
			m_flNextSpeakTime = gpGlobals->time + 3;
			return FALSE;
		}
	}

	return TRUE;
}

//=========================================================
// PrescheduleThink 
//=========================================================
void CAGrunt :: PrescheduleThink ( void )
{
	if ( ShouldSpeak() )
	{
		if ( m_flNextWordTime < gpGlobals->time )
		{
			int num = -1;
			int tries = 0;

			do
			{
				num = RANDOM_LONG(0,ARRAYSIZE(pIdleSounds)-1);
			} while ( num == m_iLastWord && tries++ < 5 );

			num = V_max( 0, V_min( num, (int) ARRAYSIZE( pIdleSounds ) - 1 ) );
			m_iLastWord = num;

			// play a new sound
			EMIT_SOUND ( ENT(pev), CHAN_VOICE, pIdleSounds[ num ], 1.0, ATTN_NORM );

			// is this word our last?
			if ( RANDOM_LONG( 1, 10 ) <= 1 )
			{
				// stop talking.
				StopTalking();
			}
			else
			{
				m_flNextWordTime = gpGlobals->time + RANDOM_FLOAT( 0.5, 1 );
			}
		}
	}

	if ( pev->weapons == 8 )
	{
		if ( m_Activity == ACT_MELEE_ATTACK1 || m_Activity == ACT_RUN )
			pev->framerate = 1.1;
		else
			pev->framerate = 1.0;
	}
}

//=========================================================
// DieSound
//=========================================================
void CAGrunt :: DeathSound ( void )
{
	StopTalking();

	EMIT_SOUND ( ENT(pev), CHAN_VOICE, pDieSounds[RANDOM_LONG(0,ARRAYSIZE(pDieSounds)-1)], 1.0, ATTN_NORM );
}

//=========================================================
// AlertSound
//=========================================================
void CAGrunt :: AlertSound ( void )
{
	StopTalking();

	EMIT_SOUND ( ENT(pev), CHAN_VOICE, pAlertSounds[RANDOM_LONG(0,ARRAYSIZE(pAlertSounds)-1)], 1.0, ATTN_NORM );
}

//=========================================================
// AttackSound
//=========================================================
void CAGrunt :: AttackSound ( void )
{
	StopTalking();

	EMIT_SOUND ( ENT(pev), CHAN_VOICE, pAttackSounds[RANDOM_LONG(0,ARRAYSIZE(pAttackSounds)-1)], 1.0, ATTN_NORM );
}

//=========================================================
// PainSound
//=========================================================
void CAGrunt :: PainSound ( void )
{
	if ( m_flNextPainTime > gpGlobals->time )
	{
		return;
	}

	m_flNextPainTime = gpGlobals->time + 0.6;

	StopTalking();

	EMIT_SOUND ( ENT(pev), CHAN_VOICE, pPainSounds[RANDOM_LONG(0,ARRAYSIZE(pPainSounds)-1)], 1.0, ATTN_NORM );
}

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CAGrunt :: Classify ( void )
{
	return GetClassification( CLASS_ALIEN_MILITARY );
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CAGrunt :: SetYawSpeed ( void )
{
	pev->yaw_speed = 270;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//
// Returns number of events handled, 0 if none.
//=========================================================
void CAGrunt :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case AGRUNT_AE_HORNET1:
	case AGRUNT_AE_HORNET2:
	case AGRUNT_AE_HORNET3:
	case AGRUNT_AE_HORNET4:
	case AGRUNT_AE_HORNET5:
		{
			if ( m_hEnemy == NULL )
				break;

			// m_vecEnemyLKP should be center of enemy body
			Vector vecArmPos, vecArmDir;
			Vector vecDirToEnemy;
			Vector angDir;

			if (HasConditions( bits_COND_SEE_ENEMY))
			{
				vecDirToEnemy = ( ( m_vecEnemyLKP ) - pev->origin );
				angDir = UTIL_VecToAngles( vecDirToEnemy );
				vecDirToEnemy = vecDirToEnemy.Normalize();
			}
			else
			{
				angDir = pev->angles;
				UTIL_MakeAimVectors( angDir );
				vecDirToEnemy = gpGlobals->v_forward;
			}

			pev->effects = EF_MUZZLEFLASH;

			// make angles +-180
			if (angDir.x > 180)
			{
				angDir.x = angDir.x - 360;
			}

			SetBlending( 0, angDir.x );
			GetAttachment( 0, vecArmPos, vecArmDir );

			vecArmPos = vecArmPos + vecDirToEnemy * 32;
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecArmPos );
				WRITE_BYTE( TE_SPRITE );
				WRITE_COORD( vecArmPos.x );	// pos
				WRITE_COORD( vecArmPos.y );	
				WRITE_COORD( vecArmPos.z );	
				WRITE_SHORT( m_iMuzzleFlash );		// model
				WRITE_BYTE( 6 );				// size * 10
				WRITE_BYTE( 128 );			// brightness
			MESSAGE_END();

			CBaseEntity *pHornet = CBaseEntity::Create( "hornet", vecArmPos, UTIL_VecToAngles( vecDirToEnemy ), false, edict() );
			CHornet *pHornetMonster = (CHornet *) pHornet->MyMonsterPointer();

			CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 512, 0.3, this );

			if ( pHornetMonster )
			{
				pHornetMonster->m_hEnemy = m_hEnemy;
				pHornetMonster->m_fPlayerAlly = m_fPlayerAlly;
				pHornetMonster->m_fOverrideClass = m_fOverrideClass;
				pHornetMonster->m_iClassSelection = m_iClassSelection;

				if ( HasConditions( bits_COND_SEE_ENEMY ) && NoFriendlyFire() )
				{
					Vector vecAim = ShootAtEnemy( vecArmPos ) + m_hEnemy->pev->velocity / 2700;

					pHornetMonster->SetThink( &CHornet::StartDart );
					pHornetMonster->pev->velocity = vecAim * 1000;
				}
				else
				{
					pHornetMonster->SetThink( &CHornet::StartTrack );
					pHornetMonster->pev->velocity = gpGlobals->v_forward * 250;
				}

				pHornetMonster->pev->angles = UTIL_VecToAngles( pHornet->pev->velocity );
			}
		}
		break;

	case AGRUNT_AE_LEFT_FOOT:
		switch (RANDOM_LONG(0,1))
		{
		// left foot
		case 0:	EMIT_SOUND_DYN ( ENT(pev), CHAN_BODY, "player/pl_ladder2.wav", 1, ATTN_NORM, 0, 70 );	break;
		case 1:	EMIT_SOUND_DYN ( ENT(pev), CHAN_BODY, "player/pl_ladder4.wav", 1, ATTN_NORM, 0, 70 );	break;
		}

		if ( m_Activity == ACT_RUN )
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "agrunt/ag_alert5.wav", 0.9, ATTN_NORM, 0, 70 );
		break;
	case AGRUNT_AE_RIGHT_FOOT:
		// right foot
		switch (RANDOM_LONG(0,1))
		{
		case 0:	EMIT_SOUND_DYN ( ENT(pev), CHAN_BODY, "player/pl_ladder1.wav", 1, ATTN_NORM, 0, 70 );	break;
		case 1:	EMIT_SOUND_DYN ( ENT(pev), CHAN_BODY, "player/pl_ladder3.wav", 1, ATTN_NORM, 0 ,70);	break;
		}

		if ( m_Activity == ACT_RUN )
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "agrunt/ag_alert5.wav", 0.9, ATTN_NORM, 0, 100 );
		break;

	case AGRUNT_AE_LEFT_PUNCH:
	case AGRUNT_AE_RIGHT_PUNCH:
		{
			CBaseEntity *pHurt;

			// grunts carrying weapons are the berserker variant
			if ( pev->weapons != 0 )
				pHurt = CheckTraceHullAttack( AGRUNT_BERSERKER_MELEE_DIST, sk_agrunt_berserker_dmg_punch.value, DMG_CLUB );
			else
				pHurt = CheckTraceHullAttack( AGRUNT_MELEE_DIST, sk_agrunt_dmg_punch.value, DMG_CLUB );

			if ( pHurt )
			{
				if ( ( pHurt->pev->flags & FL_CLIENT )
					|| ( ( pHurt->pev->flags & FL_MONSTER ) && pHurt->pev->movetype != MOVETYPE_FLY ) )
				{
					float flSide = 1;

					if ( pEvent->event == AGRUNT_AE_RIGHT_PUNCH )
						flSide = -1;

					float flPunch = 15;
					float flRight = 150;
					float flForward = 100;
					float flUp = 100;

					if ( pev->weapons != 0 )
					{
						flPunch = 25;
						flRight = 300;
						flForward = 150;
						flUp = 200;
					}

					pHurt->pev->punchangle.y = -flSide * flPunch;
					pHurt->pev->punchangle.x = 8;
					pHurt->pev->flags &= ~FL_ONGROUND;

					// heavier victims are pushed less
					float flScale = 100 / V_max( pHurt->pev->max_health, 100.0f );
					Vector vecForward, vecRight, vecUp;

					AngleVectors( pev->angles, &vecForward, &vecRight, &vecUp );

					pHurt->pev->velocity = pHurt->pev->velocity
						+ vecRight * ( flSide * flRight * flScale )
						+ vecForward * ( flForward * flScale )
						+ vecUp * ( flUp * flScale );
				}

				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
			}
			else
			{
				// Play a random attack miss sound
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
			}
		}
		break;

	case AGRUNT_AE_SNARK:
		{
			if ( !m_fThrowGrenade )
				break;

			UTIL_MakeVectors( pev->angles );

			CBaseEntity *entity = CBaseEntity::Create( "monster_sqknest", GetGunPosition(), g_vecZero, true, edict() );

			if ( entity )
			{
				CBaseMonster *nest = entity->MyMonsterPointer();

				nest->m_fPlayerAlly = m_fPlayerAlly;
				nest->m_fOverrideClass = m_fOverrideClass;
				nest->m_iClassSelection = m_iClassSelection;
				nest->pev->spawnflags |= SF_MONSTER_FALL_TO_GROUND;

				if ( pev->spawnflags & SF_MONSTER_HITMONSTERCLIP )
					nest->pev->spawnflags |= SF_MONSTER_HITMONSTERCLIP;

				DispatchSpawn( nest->edict() );
				nest->pev->velocity = m_vecTossVelocity;
				nest->pev->avelocity = Vector( 0, 200, 0 );
				m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 5, 20 );
			}
			else
				m_flNextGrenadeCheck = gpGlobals->time + 3;

			m_fThrowGrenade = false;
			break;
		}

	default:
		CSquadMonster::HandleAnimEvent( pEvent );
		break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CAGrunt :: Spawn( void )
{
	Precache( );

	if ( !SetupModel() )
		SetupFriendly();
	UTIL_SetSize(pev, Vector(-24, -24, 0), Vector( 24, 24, 88 ));

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_GREEN;
	pev->effects		= 0;

	if ( pev->health == 0 )
		pev->health			= sk_agrunt_health.value;
	m_flFieldOfView		= 0.2;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_RANGE_ATTACK2 | bits_CAP_USE_TANK;
	m_afMoveShootCap = bits_MOVESHOOTCAP_MELEE_ATTACK1 | bits_MOVESHOOTCAP_RANGE_ATTACK1;

	m_HackedGunPos		= Vector( 24, 64, 48 );

	m_flNextSpeakTime	= m_flNextWordTime = gpGlobals->time + 10 + RANDOM_LONG(0, 10);
	m_iNumSnarks = 0;
	m_flNextGrenadeCheck = gpGlobals->time + 1;

	if ( !m_FormattedName )
		m_FormattedName = AllocStringLocal( "Alien Grunt" );

	MonsterInit();

	if ( pev->weapons == 8 )
	{
		m_afMoveShootCap &= ~bits_MOVESHOOTCAP_RANGE_ATTACK1;
		SetBodygroup( 1, 1 );
	}
	else
		pev->weapons = 0;

	if ( m_fPlayerAlly && !( pev->spawnflags & SF_MONSTER_PREDISASTER ) )
	{
		SetUse( &CBaseMonster::CallFollowerPlayerUse );
		m_fCanPlayerFollow = true;
	}
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CAGrunt :: Precache( void )
{
	CBaseEntity::Precache();
	UTIL_PrecacheOther( "hornet" );
	UTIL_PrecacheOther( "monster_sqknest" );

	if ( !pev->model )
	{
		const char *model;

		if ( CClassicMode::GetInstance()->IsEnabled() )
			model = "models/hlclassic/agrunt.mdl";
		else
			model = IsPlayerAlly() ? "models/agruntf.mdl" : "models/agrunt.mdl";

		UTIL_PrecacheModel( this, model );
	}

	m_iMuzzleFlash = UTIL_PrecacheModel( this, "sprites/muz4.spr" );

	for ( int i = 0; i < ARRAYSIZE( pAttackHitSounds ); ++i )
		PrecacheSound_Intercept( this, pAttackHitSounds[i]);

	for ( int i = 0; i < ARRAYSIZE( pAttackMissSounds ); ++i )
		PrecacheSound_Intercept( this, pAttackMissSounds[i]);

	for ( int i = 0; i < ARRAYSIZE( pIdleSounds ); ++i )
		PrecacheSound_Intercept( this, pIdleSounds[i]);

	for ( int i = 0; i < ARRAYSIZE( pDieSounds ); ++i )
		PrecacheSound_Intercept( this, pDieSounds[i]);

	for ( int i = 0; i < ARRAYSIZE( pPainSounds ); ++i )
		PrecacheSound_Intercept( this, pPainSounds[i]);

	for ( int i = 0; i < ARRAYSIZE( pAttackSounds ); ++i )
		PrecacheSound_Intercept( this, pAttackSounds[i]);

	for ( int i = 0; i < ARRAYSIZE( pAlertSounds ); ++i )
		PrecacheSound_Intercept( this, pAlertSounds[i]);
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

Task_t tlAGruntSuppress[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
};

Schedule_t slAGruntSuppress[] =
{
	{
		tlAGruntSuppress,
		ARRAYSIZE( tlAGruntSuppress ),
		0,
		0,
		"AGrunt Suppress Hornet"
	},
};

Task_t tlAGruntRangeAttack1[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
};

Schedule_t slAGruntRangeAttack1[] =
{
	{
		tlAGruntRangeAttack1,
		ARRAYSIZE( tlAGruntRangeAttack1 ),
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD |
		bits_COND_HEAVY_DAMAGE,
		0,
		"AGrunt Range Attack1"
	},
};

Task_t tlAGruntHiddenRangeAttack[] =
{
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_STANDOFF },
	{ TASK_AGRUNT_SETUP_HIDE_ATTACK, 0 },
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_RANGE_ATTACK1_NOTURN, 0 },
};

Schedule_t slAGruntHiddenRangeAttack[] =
{
	{
		tlAGruntHiddenRangeAttack,
		ARRAYSIZE( tlAGruntHiddenRangeAttack ),
		bits_COND_NEW_ENEMY |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"AGrunt Hidden Range Attack1"
	},
};

Task_t tlAGruntTakeCoverFromEnemy[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_FACE_ENEMY, 0 },
};

Schedule_t slAGruntTakeCoverFromEnemy[] =
{
	{
		tlAGruntTakeCoverFromEnemy,
		ARRAYSIZE( tlAGruntTakeCoverFromEnemy ),
		bits_COND_NEW_ENEMY,
		0,
		"AGruntTakeCoverFromEnemy"
	},
};

Task_t tlAGruntVictoryDance[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_AGRUNT_THREAT_DISPLAY },
	{ TASK_WAIT, 0.2f },
	{ TASK_AGRUNT_GET_PATH_TO_ENEMY_CORPSE, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_CROUCH },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float) ACT_STAND },
	{ TASK_PLAY_SEQUENCE, (float) ACT_THREAT_DISPLAY },
	{ TASK_PLAY_SEQUENCE, (float) ACT_CROUCH },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, (float) ACT_STAND },
};

Schedule_t slAGruntVictoryDance[] =
{
	{
		tlAGruntVictoryDance,
		ARRAYSIZE( tlAGruntVictoryDance ),
		bits_COND_NEW_ENEMY |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"AGruntVictoryDance"
	},
};

Task_t tlAGruntThreatDisplay[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_THREAT_DISPLAY },
};

Schedule_t slAGruntThreatDisplay[] =
{
	{
		tlAGruntThreatDisplay,
		ARRAYSIZE( tlAGruntThreatDisplay ),
		bits_COND_NEW_ENEMY |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE,
		bits_SOUND_PLAYER |
		bits_SOUND_COMBAT |
		bits_SOUND_WORLD,
		"AGruntThreatDisplay"
	},
};

Task_t tlAGruntTeleportFall[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_AGRUNT_TELEPORT_FALL, 0 },
	{ TASK_AGRUNT_TELEPORT_LAND, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
};

Schedule_t slAGruntTeleportFall[] =
{
	{
		tlAGruntTeleportFall,
		ARRAYSIZE( tlAGruntTeleportFall ),
		0,
		0,
		"AGrunt TeleportFall"
	},
};

Task_t tlAGruntRangeAttack2[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_AGRUNT_FACE_TOSS, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_RANGE_ATTACK2 },
};

Schedule_t slAGruntRangeAttack2[] =
{
	{
		tlAGruntRangeAttack2,
		ARRAYSIZE( tlAGruntRangeAttack2 ),
		0,
		0,
		"AGrunt Range Attack2"
	},
};

DEFINE_CUSTOM_SCHEDULES( CAGrunt )
{
	slAGruntSuppress,
	slAGruntRangeAttack1,
	slAGruntHiddenRangeAttack,
	slAGruntTakeCoverFromEnemy,
	slAGruntVictoryDance,
	slAGruntThreatDisplay,
	slAGruntTeleportFall,
};

Schedule_t *CAGrunt::ScheduleFromName( const char *name )
{
	Schedule_t *schedule = ScheduleInList( name, m_scheduleList, ARRAYSIZE( m_scheduleList ) );

	if ( !schedule )
		return CSquadMonster::ScheduleFromName( name );

	return schedule;
}


//=========================================================
// FCanCheckAttacks - this is overridden for alien grunts
// because they can use their smart weapons against unseen
// enemies. Base class doesn't attack anyone it can't see.
//=========================================================
BOOL CAGrunt :: FCanCheckAttacks ( void )
{
	if ( HasConditions( bits_COND_SEE_ENEMY ) )
		m_iHideAttackAttempts = 0;

	if ( !HasConditions( bits_COND_ENEMY_TOOFAR ) )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

//=========================================================
// CheckMeleeAttack1 - alien grunts zap the crap out of
// any enemy that gets too close.
//=========================================================
BOOL CAGrunt :: CheckMeleeAttack1 ( float flDot, float flDist )
{
	if ( flDist > 90 || flDot < 0.6 )
		return FALSE;

	CBaseEntity *enemy = m_hEnemy;

	if ( !enemy || !enemy->IsMoving() )
		return TRUE;

	if ( !enemy->IsPlayer() )
		return enemy->IsFacing( pev, -0.7f );

	return enemy->pev->velocity.Length2D() <= 10;
}

BOOL CAGrunt::CheckMeleeAttack1_Move( float flDot, float flDist )
{
	return HasConditions( bits_COND_SEE_ENEMY ) && flDist <= 180 && flDot >= 0.6;
}

//=========================================================
// CheckRangeAttack1
//
// !!!LATER - we may want to load balance this. Several
// tracelines are done, so we may not want to do this every
// server frame. Definitely not while firing.
//=========================================================
BOOL CAGrunt :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( pev->weapons == 8 )
		return FALSE;

	CBaseEntity *enemy = m_hEnemy;

	if ( enemy && ( enemy->pev->movetype == MOVETYPE_FLY || enemy->pev->movetype == MOVETYPE_NOCLIP )
	     && ( enemy->pev->origin - pev->origin ).Length2D() <= 1024 )
	{
		Vector vecArmPos = g_vecZero;
		Vector angles = g_vecZero;
		TraceResult tr;

		// verify that a shot fired from the gun will hit the enemy before the world.
		// !!!LATER - we may wish to do something different for projectile weapons as opposed to instant-hit
		UTIL_MakeVectors( pev->angles );
		GetAttachment( 0, vecArmPos, angles );
//		UTIL_TraceLine( vecArmPos, vecArmPos + gpGlobals->v_forward * 256, ignore_monsters, ENT(pev), &tr);
		UTIL_TraceLine( vecArmPos - gpGlobals->v_forward * 24, enemy->BodyTarget( vecArmPos ),
		                dont_ignore_monsters, edict(), &tr );

		if ( tr.flFraction != 1.0f && tr.pHit != enemy->edict() )
			return TRUE;

		float nextCheck = gpGlobals->time + RANDOM_FLOAT( 2, 5 );

		m_fCanHornetAttack = TRUE;
		m_flNextHornetAttackCheck = nextCheck;
		return TRUE;
	}

	if ( HasConditions( bits_COND_SEE_ENEMY ) && flDist <= 1024 && NoFriendlyFire()
	     && flDist >= sk_agrunt_melee_engage_distance.value )
	{
		Vector vecArmPos = g_vecZero;
		Vector angles = g_vecZero;
		TraceResult tr;

		UTIL_MakeVectors( pev->angles );
		GetAttachment( 0, vecArmPos, angles );
		UTIL_TraceLine( vecArmPos - gpGlobals->v_forward * 24, m_hEnemy->BodyTarget(vecArmPos), dont_ignore_monsters, edict(), &tr);

		if ( tr.flFraction == 1.0 || tr.pHit == m_hEnemy->edict() )
		{
			float nextCheck = gpGlobals->time + RANDOM_FLOAT( 2, 5 );
			m_fCanHornetAttack = TRUE;
			m_flNextHornetAttackCheck = nextCheck;
			return TRUE;
		}
	}

	m_fCanHornetAttack = FALSE;
	m_flNextHornetAttackCheck = gpGlobals->time + 0.5f;
	return FALSE;
}

BOOL CAGrunt::CheckRangeAttack1_Move( float flDot, float flDist )
{
	if ( pev->weapons == 8 || !HasConditions( bits_COND_SEE_ENEMY )
	     || flDist < 180 || flDist > 1024 || !NoFriendlyFire() )
		return FALSE;

	Vector position = g_vecZero;
	Vector angles = g_vecZero;
	TraceResult trace;

	UTIL_MakeVectors( pev->angles );
	GetAttachment( 0, position, angles );

	// The moving check shifts the attachment components by one float
	Vector start = Vector( 0, position.x, position.y ) - gpGlobals->v_forward * 24;

	UTIL_TraceLine( start, m_hEnemy->BodyTarget( position ), dont_ignore_monsters, edict(), &trace );
	return trace.flFraction == 1.0f || trace.pHit == m_hEnemy->edict();
}

BOOL CAGrunt::CheckRangeAttack2( float flDot, float flDist )
{
	if ( CClassicMode::GetInstance()->IsEnabled() || m_iNumSnarks > 0 )
		return FALSE;

	if ( m_flNextGrenadeCheck > gpGlobals->time )
		return m_fThrowGrenade;

	if ( RANDOM_FLOAT( 0, 10 ) < 8 )
	{
		m_fThrowGrenade = false;
		m_flNextGrenadeCheck = gpGlobals->time + 5;
		return FALSE;
	}

	if ( !( m_hEnemy->pev->flags & FL_ONGROUND ) && m_hEnemy->pev->waterlevel == 0
	     && m_vecEnemyLKP.z > pev->absmax.z )
	{
		m_fThrowGrenade = false;
		return FALSE;
	}

	Vector target;

	if ( RANDOM_LONG( 0, 1 ) )
	{
		if ( m_hEnemy->IsBSPModel() )
		{
			target.x = m_hEnemy->Center().x;
			target.y = m_hEnemy->Center().y;
		}
		else
		{
			target.x = m_hEnemy->pev->origin.x;
			target.y = m_hEnemy->pev->origin.y;
		}

		target.z = m_hEnemy->pev->absmin.z;
	}
	else
	{
		target = m_vecEnemyLKP;
	}

	Vector velocity = VecCheckToss( pev, GetGunPosition(), target, 1.0f );

	if ( velocity == g_vecZero )
	{
		m_fThrowGrenade = false;
		m_flNextGrenadeCheck = gpGlobals->time + 1;
		return FALSE;
	}

	m_vecTossVelocity = velocity;
	m_fThrowGrenade = true;
	m_flNextGrenadeCheck = gpGlobals->time + 3;
	return TRUE;
}

//=========================================================
// StartTask
//=========================================================
void CAGrunt :: StartTask ( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_AGRUNT_GET_PATH_TO_ENEMY_CORPSE:
		{
			UTIL_MakeVectors( pev->angles );
			if ( BuildRoute ( m_vecEnemyLKP - gpGlobals->v_forward * 50, bits_MF_TO_LOCATION, NULL ) )
				TaskComplete();
			else
			{
				ALERT ( at_aiconsole, "AGruntGetPathToEnemyCorpse failed!!\n" );
				TaskFail();
			}
			break;
		}

	case TASK_AGRUNT_SETUP_HIDE_ATTACK:
		{
			if ( m_hEnemy == NULL )
			{
				ALERT( at_aiconsole, "AGrunt - no enemy monster ptr!!!\n" );
				TaskFail();
				break;
			}

			if ( m_iHideAttackAttempts > 12 )
			{
				TaskFail();
				break;
			}

			++m_iHideAttackAttempts;

			Vector center = Center();
			TraceResult tr;

			UTIL_MakeVectors( UTIL_VecToAngles( m_vecEnemyLKP - Center() ) );

			UTIL_TraceLine( center, m_vecEnemyLKP, ignore_monsters, edict(), &tr);
			if ( tr.flFraction == 1.0 )
			{
				MakeIdealYaw ( m_vecEnemyLKP );
				TaskComplete();
				break;
			}

			UTIL_TraceLine( center - gpGlobals->v_forward * 64, m_vecEnemyLKP, ignore_monsters, edict(), &tr);
				if ( tr.flFraction == 1.0 )
				{
					MakeIdealYaw ( pev->origin - gpGlobals->v_right * 128 );
				TaskComplete();
				break;
			}

			UTIL_TraceLine( center + gpGlobals->v_forward * 128, m_vecEnemyLKP, ignore_monsters, edict(), &tr);
				if ( tr.flFraction == 1.0 )
				{
					MakeIdealYaw ( pev->origin + gpGlobals->v_right * 256 );
				TaskComplete();
				break;
			}

			UTIL_TraceLine( center - gpGlobals->v_forward * 128, m_vecEnemyLKP, ignore_monsters, edict(), &tr);
				if ( tr.flFraction == 1.0 )
				{
					MakeIdealYaw ( pev->origin - gpGlobals->v_right * 256 );
				TaskComplete();
				}
			else
				TaskFail();

			break;
		}

	case TASK_AGRUNT_TELEPORT_FALL:
		if ( pev->groundentity || ( pev->flags & FL_ONGROUND ) )
		{
			++m_iScheduleIndex;
			TaskComplete();
		}
		break;

	default:
		CSquadMonster :: StartTask ( pTask );
		break;
	}
}

void CAGrunt::RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_MELEE_ATTACK1:
		{
			CBaseEntity *enemy = m_hEnemy;

			if ( enemy && ( !HasConditions( bits_COND_SEE_ENEMY )
			                || ( enemy->Center() - Center() ).Length() > 90
			                || ( enemy->IsMoving() && ( !enemy->IsPlayer() || enemy->pev->velocity.Length2D() > 10 ) ) ) )
				TaskComplete();

			CSquadMonster::RunTask( pTask );
			break;
		}

	case TASK_AGRUNT_TELEPORT_FALL:
		{
			if ( pev->groundentity || ( pev->flags & FL_ONGROUND ) || pev->velocity.z == 0 )
			{
				PainSound();
				SetSequenceByName( "landhard" );
				TaskComplete();
				break;
			}

			TraceResult trace;

			UTIL_TraceHull( pev->origin + Vector( 0, 0, 48 ), pev->origin - Vector( 0, 0, 8 ),
			                dont_ignore_monsters, head_hull, edict(), &trace );

			if ( trace.fStartSolid || trace.flFraction != 1.0f )
			{
				PainSound();
				SetSequenceByName( "landhard" );
				TaskComplete();
			}
			break;
		}

	case TASK_AGRUNT_TELEPORT_LAND:
		if ( m_fSequenceFinished )
			TaskComplete();
		break;

	case TASK_AGRUNT_FACE_TOSS:
		MakeIdealYaw( pev->origin + m_vecTossVelocity * 64 );
		ChangeYaw( pev->yaw_speed );

		if ( FacingIdeal() )
			TaskComplete();
		break;

	default:
		CSquadMonster::RunTask( pTask );
		break;
	}
}

//=========================================================
// GetSchedule - Decides which type of schedule best suits
// the monster's current state and conditions. Then calls
// monster's member function to get a pointer to a schedule
// of the proper type.
//=========================================================
Schedule_t *CAGrunt :: GetSchedule ( void )
{
	if ( HasConditions(bits_COND_HEAR_SOUND) )
	{
		CSound *pSound = PBestSound();
		if ( pSound && (pSound->m_iType & bits_SOUND_DANGER) )
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
		}

	if ( m_MonsterState != MONSTERSTATE_COMBAT )
		return CSquadMonster::GetSchedule();

	if ( m_hGuardEnt != NULL )
	{
		if ( m_hTargetEnt != m_hGuardEnt )
			StartPlayerFollowing( m_hGuardEnt, false );

		if ( m_hTargetEnt != NULL )
		{
			float distance = ( Center() - m_hTargetEnt->Center() ).Length2D();

			if ( distance > 512 )
			{
				if ( m_hEnemy == NULL )
					return GetScheduleOfType( SCHED_TARGET_PLAYERCHASE_FORCE );

				CBaseMonster *target = m_hTargetEnt->MyMonsterPointer();

				if ( target )
				{
					CBaseEntity *enemy = target->m_hEnemy;

					if ( enemy )
					{
						if ( ( enemy->Center() - m_hTargetEnt->Center() ).Length() < 512
						     && distance > 2048 )
							return GetScheduleOfType( SCHED_TARGET_PLAYERCHASE_FORCE );

						if ( m_hEnemy != NULL )
						{
							Vector position = m_hEnemy->Center();

							PushEnemy( m_hEnemy, position );
							m_hEnemy = enemy;
						}
					}
				}

				if ( !FVisible( m_hTargetEnt, true ) )
					return GetScheduleOfType( SCHED_TARGET_PLAYERCHASE_FORCE );
			}
		}
	}

	if ( HasConditions( bits_COND_ENEMY_DEAD ) )
		return CSquadMonster:: GetSchedule();

	if ( HasConditions(bits_COND_NEW_ENEMY) )
		return GetScheduleOfType( SCHED_WAKE_ANGRY );

	if ( HasConditions ( bits_COND_CAN_MELEE_ATTACK1 ) )
			{
				AttackSound();// this is a total hack. Should be parto f the schedule
				return GetScheduleOfType ( SCHED_MELEE_ATTACK1 );
			}

			if ( pev->weapons == 0 )
	{
		if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) && OccupySlot ( bits_SLOTS_AGRUNT_HORNET ) )
			return GetScheduleOfType ( HasConditions( bits_COND_HEAVY_DAMAGE )
			                          ? SCHED_SMALL_FLINCH : SCHED_RANGE_ATTACK1 );

		if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) )
			return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
	}

	return GetScheduleOfType ( SCHED_CHASE_ENEMY );
			}

//=========================================================
//=========================================================
Schedule_t* CAGrunt :: GetScheduleOfType ( int Type )
{
	switch	( Type )
	{
	case SCHED_CHASE_ENEMY_FAILED:
		{
			if ( pev->weapons != 0 )
				return CSquadMonster::GetScheduleOfType( SCHED_FIND_ATTACK_POINT );

			if ( !HasConditions( bits_COND_SEE_ENEMY ) || !NoFriendlyFire() )
				return slAGruntHiddenRangeAttack;

			Vector position = g_vecZero;
			Vector angles = g_vecZero;
			TraceResult trace;

			UTIL_MakeVectors( pev->angles );
			GetAttachment( 0, position, angles );

			Vector start( 0, position.x, position.y );

			UTIL_TraceLine( start, m_hEnemy->BodyTarget( position ), dont_ignore_monsters, edict(), &trace );

			if ( trace.flFraction == 1.0f || trace.pHit == m_hEnemy->edict() )
			{
				float nextCheck = gpGlobals->time + RANDOM_FLOAT( 2, 5 );

				m_fCanHornetAttack = TRUE;
				m_flNextHornetAttackCheck = nextCheck;
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
			}

			m_fCanHornetAttack = FALSE;
			m_flNextHornetAttackCheck = (float) ( gpGlobals->time + 0.2 );
			return CSquadMonster::GetScheduleOfType( SCHED_FIND_ATTACK_POINT );
		}

	case SCHED_VICTORY_DANCE:
		return slAGruntVictoryDance;

	case SCHED_TAKE_COVER_FROM_ENEMY:
		return slAGruntTakeCoverFromEnemy;

	case SCHED_RANGE_ATTACK1:
		return HasConditions( bits_COND_SEE_ENEMY ) ? slAGruntRangeAttack1 : slAGruntHiddenRangeAttack;

	case SCHED_RANGE_ATTACK2:
		return slAGruntRangeAttack2;

	case SCHED_TELEPORT_FALL:
		return slAGruntTeleportFall;

	case SCHED_AGRUNT_SUPPRESS:
		return slAGruntSuppress;

	case SCHED_AGRUNT_THREAT_DISPLAY:
		return slAGruntThreatDisplay;

	default:
		return CSquadMonster :: GetScheduleOfType( Type );
}
}

void CAGrunt::SetupFriendly( void )
{
	const char *model;

	if ( CClassicMode::GetInstance()->IsEnabled() )
		model = "models/hlclassic/agrunt.mdl";
	else
		model = IsPlayerAlly() ? "models/agruntf.mdl" : "models/agrunt.mdl";

	UTIL_SetModel( this, model );
}

int CAGrunt::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	if ( damageType & ( DMG_SLASH | DMG_CLUB ) )
	{
		if ( attacker->flags & FL_CLIENT )
			damage = (float) ( damage * 0.25 );
		else
			damage = (float) ( ( damage - 10.0f ) * 0.2 );

		if ( damage < 0 )
			damage = 0.1f;
	}

	return CBaseMonster::TakeDamage( inflictor, attacker, damage, damageType );
}

void CAGrunt::Killed( entvars_t *attacker, int gib )
{
	CSquadMonster::Killed( attacker, gib );
}

int CAGrunt::ObjectCaps( void )
{
	return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

float CAGrunt::GetPointsForDamage( float flDamage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	float points = pev->max_health / sk_agrunt_health.value;

	points *= 4.0f;

	return points * ( flDamage / pev->max_health );
}

void CAGrunt::UseSound( void )
{
	AlertSound();
}

void CAGrunt::UnUseSound( void )
{
	PainSound();
}
