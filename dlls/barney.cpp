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
// monster template
//=========================================================
// UNDONE: Holster weapon?

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"talkmonster.h"
#include	"schedule.h"
#include	"defaultai.h"
#include	"scripted.h"
#include	"weapons.h"
#include	"soundent.h"
#include "CClassicMode.h"

extern cvar_t sk_barney_health;

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
// first flag is barney dying for scripted sequences?
#define		BARNEY_AE_DRAW		( 2 )
#define		BARNEY_AE_SHOOT		( 3 )
#define		BARNEY_AE_HOLSTER	( 4 )

#define	BARNEY_BODY_GUNHOLSTERED	0
#define	BARNEY_BODY_GUNDRAWN		1
#define BARNEY_BODY_GUNGONE			2
#define BARNEY_GUN_GROUP			1

enum
{
	SCHED_BARNEY_RELOAD = LAST_TALKMONSTER_SCHEDULE + 1,
};

class CBarney : public CTalkMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetupFriendly( void );
	void SetPlayerAlly( bool state );
	float GetPointsForDamage( float damage );
	void CheckAmmo( void );
	void FearScream( void );
	void SetYawSpeed( void );
	int  ISoundMask( void );
	int  GunBodygroup( void );
	void BarneyFirePistol( void );
	void AlertSound( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	
	void RunTask( Task_t *pTask );
	void StartTask( Task_t *pTask );
	int ObjectCaps( void );
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	
	void DeclineFollowing( void );

	// Override these to set behavior
	Schedule_t *GetScheduleOfType ( int Type );
	Schedule_t *GetSchedule ( void );
	MONSTERSTATE GetIdealState ( void );

	void DeathSound( void );
	void PainSound( void );
	
	void TalkInit( void );

	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	void Killed( entvars_t *pevAttacker, int iGib );
	
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	bool	m_fGunDrawn;
	float	m_painTime;
	float	m_checkAttackTime;
	BOOL	m_lastAttackCheck;

	// UNDONE: What is this for?  It isn't used?
	float	m_flPlayerDamage;// how much pain has the player inflicted on me?

	int   m_iBrassShell;
	int   m_cClipSize;
	float m_flNextFearScream;

	CUSTOM_SCHEDULES;
};

LINK_ENTITY_TO_CLASS( monster_barney, CBarney );

TYPEDESCRIPTION CBarney::m_SaveData[] =
{
	DEFINE_FIELD( CBarney, m_fGunDrawn, FIELD_CHARACTER ),
	DEFINE_FIELD( CBarney, m_painTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CBarney, CTalkMonster );

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
Task_t tlBaFollow[] =
{
	{ TASK_MOVE_TO_TARGET_RANGE, 128 },
	{ TASK_SET_SCHEDULE, (float) SCHED_TARGET_FACE },
};

Schedule_t slBaFollow[] =
{
	{
		tlBaFollow,
		ARRAYSIZE( tlBaFollow ),
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"Follow"
	},
};

Task_t tlBarneyEnemyDraw[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_ARM },
};

Schedule_t slBarneyEnemyDraw[] =
{
	{
		tlBarneyEnemyDraw,
		ARRAYSIZE( tlBarneyEnemyDraw ),
		0,
		0,
		"Barney Enemy Draw"
	},
};

Task_t tlBaFaceTarget[] =
{
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_FACE_TARGET, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_SET_SCHEDULE, (float) SCHED_TARGET_CHASE },
};

Schedule_t slBaFaceTarget[] =
{
	{
		tlBaFaceTarget,
		ARRAYSIZE( tlBaFaceTarget ),
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_SPECIAL1,
		bits_SOUND_DANGER,
		"FaceTarget"
	},
};

Task_t tlIdleBaStand[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT, 2 },
	{ TASK_TLK_HEADRESET, 0 },
};

Schedule_t slIdleBaStand[] =
{
	{
		tlIdleBaStand,
		ARRAYSIZE( tlIdleBaStand ),
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_SMELL,
		bits_SOUND_COMBAT |
		bits_SOUND_CARCASS |
		bits_SOUND_MEAT |
		bits_SOUND_DANGER |
		bits_SOUND_GARBAGE,
		"IdleStand"
	},
};

Task_t tlBarneyReload[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_RELOAD },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_RELOAD, 0 },
	{ TASK_FACE_ENEMY, 0 },
};

Schedule_t slBarneyReload[] =
{
	{
		tlBarneyReload,
		ARRAYSIZE( tlBarneyReload ),
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"Barney Reload"
	},
};

Task_t tlBarneyReloadQuick[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_RELOAD, 0 },
	{ TASK_FACE_ENEMY, 0 },
};

Schedule_t slBarneyReloadQuick[] =
{
	{
		tlBarneyReloadQuick,
		ARRAYSIZE( tlBarneyReloadQuick ),
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"Barney Reload Quick"
	},
};

DEFINE_CUSTOM_SCHEDULES( CBarney )
{
	slBaFollow,
	slBarneyEnemyDraw,
	slBaFaceTarget,
	slIdleBaStand,
	slBarneyReload,
	slBarneyReloadQuick,
};

IMPLEMENT_CUSTOM_SCHEDULES( CBarney, CTalkMonster );

void CBarney :: StartTask( Task_t *pTask )
{
	CTalkMonster::StartTask( pTask );	
}

void CBarney :: RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_RANGE_ATTACK1:
			pev->framerate = 1.5;

			if ( !NoFriendlyFire() )
			{
				ChangeSchedule( GetScheduleOfType( SCHED_FIND_ATTACK_POINT ) );
				break;
		}
		CTalkMonster::RunTask( pTask );
			break;

		case TASK_RELOAD:
			MakeIdealYaw( m_vecEnemyLKP );
			ChangeYaw( pev->yaw_speed );

			if ( m_fSequenceFinished )
			{
				m_cAmmoLoaded = m_cClipSize;
				ClearConditions( bits_COND_NO_AMMO_LOADED );
				TaskComplete();
			}
			break;
	default:
		CTalkMonster::RunTask( pTask );
		break;
	}
}




//=========================================================
// ISoundMask - returns a bit mask indicating which types
// of sounds this monster regards. 
//=========================================================
int CBarney :: ISoundMask ( void) 
{
	return 0xFF;
}

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CBarney :: Classify ( void )
{
	return GetClassification( CLASS_HUMAN_MILITARY );
}

//=========================================================
// ALertSound - barney says "Freeze!"
//=========================================================
void CBarney :: AlertSound( void )
{
	if ( m_hEnemy != NULL )
	{
		if ( FOkToSpeak() )
		{
			PlaySentence( "BA_ATTACK", RANDOM_FLOAT(2.8, 3.2), VOL_NORM, ATTN_IDLE );
		}
	}

}
//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CBarney :: SetYawSpeed ( void )
{
	pev->yaw_speed = 270;
}


//=========================================================
// CheckRangeAttack1
//=========================================================
BOOL CBarney :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( flDist > 2048 || flDot < 0.5 || !NoFriendlyFire() )
		return FALSE;

	TraceResult tr;
	Vector shootOrigin = pev->origin + Vector( 0, 0, 55 );
	CBaseEntity *pEnemy = m_hEnemy;
	Vector shootTarget = ( pEnemy->BodyTarget( shootOrigin ) - pEnemy->Center() ) + m_vecEnemyLKP;

	UTIL_TraceLine( shootOrigin, shootTarget, dont_ignore_monsters, ENT(pev), &tr );

	return tr.flFraction == 1.0 || tr.pHit == pEnemy->edict();
}


//=========================================================
// BarneyFirePistol - shoots one round from the pistol at
// the enemy barney is facing.
//=========================================================
void CBarney :: BarneyFirePistol ( void )
{
	Vector vecShootOrigin;

	UTIL_MakeVectors(pev->angles);
	vecShootOrigin = pev->origin + Vector( 0, 0, 55 );
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	Vector angDir = UTIL_VecToAngles( vecShootDir );

	FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_2DEGREES, 1024, BULLET_MONSTER_9MM );

	Vector vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT( 40, 90 ) + gpGlobals->v_up * RANDOM_FLOAT( 75, 200 ) + gpGlobals->v_forward * RANDOM_FLOAT( -40, 40 );

	EjectBrass( vecShootOrigin + vecShootDir * 17, vecShellVelocity, pev->angles.y, m_iBrassShell, TE_BOUNCE_SHELL );

	int pitchShift = RANDOM_LONG( 0, 20 );
	
	// Only shift about half the time
	if ( pitchShift > 10 )
		pitchShift = 0;
	else
		pitchShift -= 5;

	SetBlending( 0, angDir.x );
	pev->effects = EF_MUZZLEFLASH;

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, 1024, 0.3, this );

	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/pl_gun3.wav", 1, ATTN_NORM, 0, 100 + pitchShift );

	if ( pev->movetype != MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
		m_flAutomaticAttackTime = gpGlobals->time + RANDOM_FLOAT( 0.2, 0.5 );

	// UNDONE: Reload?
	m_cAmmoLoaded--;// take away a bullet!
}
		
//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//
// Returns number of events handled, 0 if none.
//=========================================================
void CBarney :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case BARNEY_AE_SHOOT:
		BarneyFirePistol();
		break;

	case BARNEY_AE_DRAW:
		// barney's bodygroup switches here so he can pull gun from holster
		SetBodygroup( GunBodygroup(), BARNEY_BODY_GUNDRAWN );
		m_fGunDrawn = TRUE;
		break;

	case BARNEY_AE_HOLSTER:
		// change bodygroup to replace gun in holster
		SetBodygroup( GunBodygroup(), BARNEY_BODY_GUNHOLSTERED );
		m_fGunDrawn = FALSE;
		break;

	default:
		CTalkMonster::HandleAnimEvent( pEvent );
	}
}

//=========================================================
// Spawn
//=========================================================
void CBarney :: Spawn( void )
{
	Precache( );

	if ( !SetupModel() )
		SetupFriendly();
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;

	if ( pev->health == 0 )
		pev->health			= sk_barney_health.value;
	pev->view_ofs		= Vector ( 0, 0, 50 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= -0.7f; // NOTE: we need a wide field of view so npc will notice player and say hello
	m_MonsterState		= MONSTERSTATE_NONE;

	m_afCapability		= bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP | bits_CAP_USE_TANK;
	m_fCheckCreatureDanger = true;
	m_flNextFearScream = gpGlobals->time;
	m_cClipSize = 17;
	m_cAmmoLoaded = 17;
	SetBodygroup( GunBodygroup(), BARNEY_BODY_GUNHOLSTERED );
	m_fGunDrawn = FALSE;

	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( IsPlayerAlly() ? "Barney" : "Barnabus" );

	MonsterInit();
	SetUse( &CBarney::CallFollowerPlayerUse );
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CBarney :: Precache( void )
{
	TalkInit();
	CTalkMonster::Precache();

	if ( !pev->model )
	{
		const char *model;

		if ( CClassicMode::GetInstance()->IsEnabled() )
			model = "models/hlclassic/barney.mdl";
		else
			model = IsPlayerAlly() ? "models/barney.mdl" : "models/barnabus.mdl";

		UTIL_PrecacheModel( this, model );
	}

	PrecacheSound_Intercept( this, "barney/ba_attack2.wav" );
	PrecacheSound_Intercept( this, "barney/ba_pain1.wav" );
	PrecacheSound_Intercept( this, "barney/ba_pain2.wav" );
	PrecacheSound_Intercept( this, "barney/ba_pain3.wav" );
	PrecacheSound_Intercept( this, "barney/ba_die1.wav" );
	PrecacheSound_Intercept( this, "barney/ba_die2.wav" );
	PrecacheSound_Intercept( this, "barney/ba_die3.wav" );
	PrecacheSound_Intercept( this, "barney/down.wav" );
	PrecacheSound_Intercept( this, "barney/hey.wav" );
	PrecacheSound_Intercept( this, "barney/aghh.wav" );

	m_iBrassShell = UTIL_PrecacheModel( this, CClassicMode::GetInstance()->IsEnabled()
	                                              ? "models/hlclassic/shell.mdl"
	                                              : "models/shell.mdl" );
}	

// Init talk data
void CBarney :: TalkInit()
{
	
	CTalkMonster::TalkInit();

	// scientists speach group names (group names are in sentences.txt)

	m_szGrp[TLK_ANSWER]  =	"BA_ANSWER";
	m_szGrp[TLK_QUESTION] =	"BA_QUESTION";
	m_szGrp[TLK_IDLE] =		"BA_IDLE";
	m_szGrp[TLK_STARE] =		"BA_STARE";
	m_szGrp[TLK_USE] =		"BA_OK";
	m_szGrp[TLK_UNUSE] =	"BA_WAIT";
	m_szGrp[TLK_STOP] =		"BA_STOP";

	if ( IsPlayerAlly() )
		m_szGrp[TLK_NOSHOOT] = "BA_SCARED";
	else
		m_szGrp[TLK_NOSHOOT] = NULL;

	m_szGrp[TLK_HELLO] =	"BA_HELLO";

	m_szGrp[TLK_PLHURT1] =	"!BA_CUREA";
	m_szGrp[TLK_PLHURT2] =	"!BA_CUREB"; 
	m_szGrp[TLK_PLHURT3] =	"!BA_CUREC";

	m_szGrp[TLK_PHELLO] =	NULL;	//"BA_PHELLO";		// UNDONE
	m_szGrp[TLK_PIDLE] =	NULL;	//"BA_PIDLE";			// UNDONE
	m_szGrp[TLK_PQUESTION] = "BA_PQUEST";		// UNDONE

	m_szGrp[TLK_SMELL] =	"BA_SMELL";
	
	m_szGrp[TLK_WOUND] =	"BA_WOUND";
	m_szGrp[TLK_MORTAL] =	"BA_MORTAL";

	// get voice for head - just one barney voice for now
	m_voicePitch = 100;
}


static BOOL IsFacing( entvars_t *pevTest, const Vector &reference )
{
	Vector vecDir = (reference - pevTest->origin);
	vecDir.z = 0;
	vecDir = vecDir.Normalize();
	Vector forward, angle;
	angle = pevTest->v_angle;
	angle.x = 0;
	UTIL_MakeVectorsPrivate( angle, forward, NULL, NULL );
	// He's facing me, he meant it
	if ( DotProduct( forward, vecDir ) > 0.96 )	// +/- 15 degrees or so
	{
		return TRUE;
	}
	return FALSE;
}


int CBarney :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if ( !pevAttacker )
		return 0;

	CBaseEntity *pAttacker = CBaseEntity::Instance( pevAttacker );

	// mp_npckill protection: no damage and no provocation
	if ( CheckAttacker( pAttacker ) )
		return 0;

	// make sure friends talk about it if player hurts talkmonsters...
	int ret = CTalkMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);

	if ( ( !IsAlive() || pev->deadflag == DEAD_DYING ) && !IsPlayerAlly() )
		return ret;

	if ( m_MonsterState == MONSTERSTATE_PRONE || !(pevAttacker->flags & FL_CLIENT) )
		return ret;

	// This is a heurstic to determine if the player intended to harm me
	// If I have an enemy, we can't establish intent (may just be crossfire)
	if ( m_hEnemy != NULL )
	{
		if ( !(m_hEnemy->IsPlayer()) && pev->deadflag == DEAD_NO )
			PlaySentence( "BA_SHOT", 4, VOL_NORM, ATTN_NORM );

		return ret;
	}

	if ( pev->deadflag != DEAD_NO )
		return ret;

	// If the player was facing directly at me, or I'm already suspicious, get mad
	if ( (m_afMemory & bits_MEMORY_SUSPICIOUS) || pAttacker->IsFacing( pev, 0.96f ) )
	{
		// Alright, now I'm pissed!
		PlaySentence( "BA_MAD", 4, VOL_NORM, ATTN_NORM );

		Remember( bits_MEMORY_PROVOKED );
		StopPlayerFollowing( TRUE );
	}
	else
	{
		// Hey, be careful with that
		PlaySentence( "BA_SHOT", 4, VOL_NORM, ATTN_NORM );
		Remember( bits_MEMORY_SUSPICIOUS );
	}

	return ret;
}

	
//=========================================================
// PainSound
//=========================================================
void CBarney :: PainSound ( void )
{
	if (gpGlobals->time < m_painTime)
		return;
	
	m_painTime = gpGlobals->time + RANDOM_FLOAT(0.5, 0.75);

	switch (RANDOM_LONG(0,2))
	{
	case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_pain1.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_pain2.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_pain3.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	}
}

//=========================================================
// DeathSound 
//=========================================================
void CBarney :: DeathSound ( void )
{
	switch (RANDOM_LONG(0,2))
	{
	case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_die1.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_die2.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_die3.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	}
}


void CBarney::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	switch( ptr->iHitgroup)
	{
	case HITGROUP_CHEST:
	case HITGROUP_STOMACH:
		if (bitsDamageType & (DMG_BULLET | DMG_SLASH | DMG_BLAST))
		{
			if ( flDamage >= 2 )
				flDamage -= 2;

			flDamage *= 0.5;
		}
		break;
	case 10:
		if (bitsDamageType & (DMG_BULLET | DMG_SLASH | DMG_CLUB | DMG_SNIPER))
		{
			flDamage -= 20;
			if (flDamage <= 0)
			{
				UTIL_Ricochet( ptr->vecEndPos, 1.0 );
				flDamage = 0.01;
			}
		}
		// always a head shot
		ptr->iHitgroup = HITGROUP_HEAD;
		break;
	}

	CTalkMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}


void CBarney::Killed( entvars_t *pevAttacker, int iGib )
{
	if ( GetBodygroup( BARNEY_GUN_GROUP ) != BARNEY_BODY_GUNGONE && DropWeapon( "weapon_9mmhandgun", false ) )
	{// drop the gun!
		SetBodygroup( GunBodygroup(), BARNEY_BODY_GUNGONE );
		m_fGunDrawn = FALSE;
	}

	CTalkMonster::Killed( pevAttacker, iGib );
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

Schedule_t* CBarney :: GetScheduleOfType ( int Type )
{
	switch( Type )
	{
	case SCHED_ARM_WEAPON:
		if ( m_hEnemy != NULL )
				return slBarneyEnemyDraw;
			break;

	// Hook these to make a looping schedule
	case SCHED_TARGET_FACE:
		{
			Schedule_t *schedule = CTalkMonster::GetScheduleOfType(Type);

			if ( schedule == slIdleStand )
				return slBaFaceTarget;

			return schedule;
		}

		case SCHED_TARGET_CHASE:
		return slBaFollow;

	case SCHED_IDLE_STAND:
		{
			Schedule_t *schedule = CTalkMonster::GetScheduleOfType(Type);

			if ( schedule == slIdleStand )
				return slIdleBaStand;

			return schedule;
		}

		case SCHED_RELOAD:
			return slBarneyReloadQuick;

		case SCHED_BARNEY_RELOAD:
			return slBarneyReload;
	}

	return CTalkMonster::GetScheduleOfType( Type );
}

//=========================================================
// GetSchedule - Decides which type of schedule best suits
// the monster's current state and conditions. Then calls
// monster's member function to get a pointer to a schedule
// of the proper type.
//=========================================================
Schedule_t *CBarney :: GetSchedule ( void )
{
	if ( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		CSound *pSound = PBestSound();
		if ( pSound && (pSound->m_iType & bits_SOUND_DANGER) )
		{
			FearScream();
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
		}
	}
	if ( HasConditions( bits_COND_ENEMY_DEAD ) && FOkToSpeak() )
		PlaySentence( "BA_KILL", 4, VOL_NORM, ATTN_NORM );

	switch( m_MonsterState )
	{
	case MONSTERSTATE_IDLE:
			if ( m_cAmmoLoaded != m_cClipSize )
				return GetScheduleOfType( SCHED_BARNEY_RELOAD );
			// Fall through

		case MONSTERSTATE_ALERT:
			if ( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
				return GetScheduleOfType( SCHED_SMALL_FLINCH );

			if ( ( m_hEnemy == NULL || !HasConditions( bits_COND_SEE_ENEMY ) ) && IsPlayerFollowing() )
			{
				if ( !m_hTargetEnt->IsAlive() )
				{
					StopPlayerFollowing( false, false );
					return CBaseMonster :: GetSchedule();
			}

				return GetScheduleOfType( HasConditions( bits_COND_CLIENT_PUSH ) ? SCHED_MOVE_AWAY_FOLLOW : SCHED_TARGET_FACE );
			}

			if ( HasConditions( bits_COND_CLIENT_PUSH ) )
				return GetScheduleOfType( SCHED_MOVE_AWAY );

			TrySmellTalk();
			return CBaseMonster::GetSchedule();

		case MONSTERSTATE_COMBAT:
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
				return CBaseMonster::GetSchedule();

			if ( HasAllConditions( bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE) )
				return GetScheduleOfType( SCHED_SMALL_FLINCH );

			// wait for one schedule to draw gun
			if (!m_fGunDrawn )
				return GetScheduleOfType( SCHED_ARM_WEAPON );

			if ( HasConditions( bits_COND_HEAVY_DAMAGE ) )
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );

			if ( HasConditions( bits_COND_NO_AMMO_LOADED ) )
				return GetScheduleOfType( SCHED_BARNEY_RELOAD );
		break;
	}

	return CBaseMonster::GetSchedule();
}

MONSTERSTATE CBarney :: GetIdealState ( void )
{
	return CTalkMonster::GetIdealState();
}



void CBarney::DeclineFollowing( void )
{
	PlaySentence( "BA_POK", 2, VOL_NORM, ATTN_NORM );
}





void CBarney::SetPlayerAlly( bool state )
{
	m_fPlayerAlly = !state;
}

int CBarney::ObjectCaps( void )
{
	return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

float CBarney::GetPointsForDamage( float damage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	return (float) ( (double) ( pev->max_health / sk_barney_health.value ) * 2.5 * (double) ( damage / pev->max_health ) );
}

int CBarney::GunBodygroup( void )
{
	if ( GetBodygroupSubmodelCount( BARNEY_GUN_GROUP ) < 3 )
		return 2;

	return BARNEY_GUN_GROUP;
}

void CBarney::CheckAmmo( void )
{
	if ( m_cAmmoLoaded <= 0 )
		SetConditions( bits_COND_NO_AMMO_LOADED );
}

void CBarney::SetupFriendly( void )
{
	const char *model;

	if ( CClassicMode::GetInstance()->IsEnabled() )
		model = "models/hlclassic/barney.mdl";
	else
		model = IsPlayerAlly() ? "models/barney.mdl" : "models/barnabus.mdl";

	UTIL_SetModel( this, model );
}

void CBarney::FearScream( void )
{
	if ( gpGlobals->time > m_flNextFearScream )
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
			case 0:
				EMIT_SOUND_DYN( edict(), CHAN_VOICE, "barney/down.wav", 1, ATTN_NORM, 0, GetVoicePitch() );
				break;

			case 1:
				EMIT_SOUND_DYN( edict(), CHAN_VOICE, "barney/aghh.wav", 1, ATTN_NORM, 0, GetVoicePitch() );
				break;

			case 2:
				EMIT_SOUND_DYN( edict(), CHAN_VOICE, "barney/hey.wav", 1, ATTN_NORM, 0, GetVoicePitch() );
				break;
		}

		float delay = (float) RANDOM_LONG( 2, 5 );

		m_flNextFearScream = gpGlobals->time + delay;
	}
}

//=========================================================
// DEAD BARNEY PROP
//
// Designer selects a pose in worldcraft, 0 through num_poses-1
// this value is added to what is selected as the 'first dead pose'
// among the monster's normal animations. All dead poses must
// appear sequentially in the model file. Be sure and set
// the m_iFirstPose properly!
//
//=========================================================
class CDeadBarney : public CBaseMonster
{
public:
	void Spawn( void );
	int	Classify ( void ) { return	CLASS_PLAYER_ALLY; }

	void KeyValue( KeyValueData *pkvd );

	int	m_iPose;// which sequence to display	-- temporary, don't need to save
	static const char *m_szPoses[3];
};

const char *CDeadBarney::m_szPoses[] = { "lying_on_back", "lying_on_side", "lying_on_stomach" };

void CDeadBarney::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "pose"))
	{
		m_iPose = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else 
		CBaseMonster::KeyValue( pkvd );
}

LINK_ENTITY_TO_CLASS( monster_barney_dead, CDeadBarney );

//=========================================================
// ********** DeadBarney SPAWN **********
//=========================================================
void CDeadBarney :: Spawn( )
{
	const char *model;

	if ( pev->model )
		model = STRING( pev->model );
	else if ( CClassicMode::GetInstance()->IsEnabled() )
		model = "models/hlclassic/barney.mdl";
	else
		model = "models/barney.mdl";

	UTIL_PrecacheModel( this, model );
	UTIL_SetModel( this, model );

	m_bloodColor		= BLOOD_COLOR_RED;
	// Corpses have less health
	pev->health			= 8;//gSkillData.barneyHealth;
	pev->skin			= 0;
	pev->effects		= 0;
	pev->yaw_speed		= 8;

	m_iPose = V_max( 0, V_min( m_iPose, (int) ARRAYSIZE( m_szPoses ) - 1 ) );

	pev->sequence = LookupSequence( m_szPoses[m_iPose] );

	if ( pev->sequence == -1 )
	{
		ALERT( at_console, "Dead \"%s\" with bad pose %d at (X%f Y%f Z%f)\n",
		       STRING( pev->classname ), m_iPose, pev->origin.x, pev->origin.y, pev->origin.z );
		UTIL_Remove( this );
		return;
	}

	MonsterInitDead();
}


