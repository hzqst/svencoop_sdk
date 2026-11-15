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
#ifndef OEM_BUILD

//=========================================================
// Gargantua
//=========================================================
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"nodes.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"defaultai.h"
#include	"customentity.h"
#include	"weapons.h"
#include	"effects.h"
#include	"soundent.h"
#include	"decals.h"
#include	"explode.h"
#include	"func_break.h"
#include	"gargantua.h"
#include	"TypedEntityHandle.h"

//=========================================================
// Gargantua Monster
//=========================================================
const float GARG_ATTACKDIST = 80.0;

// Garg animation events
#define GARG_AE_SLASH_LEFT			1
//#define GARG_AE_BEAM_ATTACK_RIGHT	2		// No longer used
#define GARG_AE_LEFT_FOOT			3
#define GARG_AE_RIGHT_FOOT			4
#define GARG_AE_STOMP				5
#define GARG_AE_BREATHE				6
#define GARG_AE_GRAB					9
#define GARG_AE_BITE					10
#define GARG_AE_BITE_BLOOD			11


// Gargantua is immune to any damage but this
#define GARG_DAMAGE					(DMG_ENERGYBEAM|DMG_CRUSH|DMG_MORTAR|DMG_BLAST|DMG_BURN)
#define GARG_EYE_SPRITE_NAME		"sprites/gargeye1.spr"
#define GARG_BEAM_SPRITE_NAME		"sprites/xbeam3.spr"
#define GARG_BEAM_SPRITE2			"sprites/xbeam3.spr"
#define GARG_STOMP_SPRITE_NAME		"sprites/gargeye1.spr"
#define GARG_STOMP_BUZZ_SOUND		"weapons/mine_charge.wav"
#define GARG_FLAME_LENGTH			330
#define GARG_GIB_MODEL				"models/metalplategibs.mdl"

#define ATTN_GARG					(ATTN_NORM)

#define STOMP_SPRITE_COUNT			10

int gStompSprite = 0, gGargGibModel = 0;
void SpawnExplosion( entvars_t *pevOwner, Vector center, float randomRange, float time, int magnitude );

class CSmoker;

// Spiral Effect
class CSpiral : public CBaseEntity
{
public:
	void Spawn( void );
	void Think( void );
	int ObjectCaps( void ) { return FCAP_DONT_SAVE; }
	static CSpiral *Create( const Vector &origin, float height, float radius, float duration );
};
LINK_ENTITY_TO_CLASS( streak_spiral, CSpiral );


LINK_ENTITY_TO_CLASS( garg_stomp, CStomp );
CStomp *CStomp::StompCreate( const Vector &origin, const Vector &end, float speed, entvars_t *pevOwner )
{
	CStomp *pStomp = GetClassPtr( (CStomp *)NULL );

	pStomp->pev->origin = origin;
	Vector dir = (end - origin);
	pStomp->pev->scale = dir.Length();
	pStomp->pev->movedir = dir.Normalize();
	pStomp->pev->speed = speed;

	if ( pevOwner )
	{
		edict_t *owner = ENT( pevOwner );

		if ( owner && !owner->free && ENTINDEX( owner ) != 0 )
			pStomp->pev->owner = owner;
	}

	pStomp->Spawn();

	return pStomp;
}

void CStomp::Spawn( void )
{
	pev->nextthink = gpGlobals->time;
	pev->classname = MAKE_STRING("garg_stomp");
	pev->dmgtime = gpGlobals->time;

	pev->framerate = 30;
	pev->model = MAKE_STRING(GARG_STOMP_SPRITE_NAME);
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	EMIT_SOUND_DYN( edict(), CHAN_BODY, GARG_STOMP_BUZZ_SOUND, 1, ATTN_NORM, 0, PITCH_NORM * 0.55);
}


#define	STOMP_INTERVAL		0.05

extern cvar_t sk_gargantua_dmg_stomp;

void CStomp::Think( void )
{
	TraceResult tr;

	pev->nextthink = gpGlobals->time + 0.1;

	// Do damage for this frame
	Vector vecStart = pev->origin;
	vecStart.z += 30;
	Vector vecEnd = vecStart + (pev->movedir * pev->speed * gpGlobals->frametime);

	UTIL_TraceHull( vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr );
	
	if ( tr.pHit && tr.pHit != pev->owner )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		entvars_t *pevOwner = pev;
		if ( pev->owner )
			pevOwner = VARS(pev->owner);

		if ( pEntity )
			pEntity->TakeDamage( pev, pevOwner, sk_gargantua_dmg_stomp.value, DMG_SONIC );
	}
	
	// Accelerate the effect
	pev->speed = pev->speed + (gpGlobals->frametime) * pev->framerate;
	pev->framerate = pev->framerate + (gpGlobals->frametime) * 1500;
	
	// Move and spawn trails
	while ( gpGlobals->time - pev->dmgtime > STOMP_INTERVAL )
	{
		pev->origin = pev->origin + pev->movedir * pev->speed * STOMP_INTERVAL;
		for ( int i = 0; i < 2; i++ )
		{
			CSprite *pSprite = CSprite::SpriteCreate( GARG_STOMP_SPRITE_NAME, pev->origin, TRUE );
			if ( pSprite )
			{
				UTIL_TraceLine( pev->origin, pev->origin - Vector(0,0,500), ignore_monsters, edict(), &tr );
				pSprite->pev->origin = tr.vecEndPos;
				pSprite->pev->velocity = Vector(RANDOM_FLOAT(-200,200),RANDOM_FLOAT(-200,200),175);
				// pSprite->AnimateAndDie( RANDOM_FLOAT( 8.0, 12.0 ) );
				pSprite->pev->nextthink = gpGlobals->time + 0.3;

				if ( pev->owner )
					pSprite->pev->scale = VARS( pev->owner )->scale;

				pSprite->SetThink( &CSprite::SUB_Remove );
				pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxFadeFast );
			}
		}
		pev->dmgtime += STOMP_INTERVAL;
		// Scale has the "life" of this effect
		pev->scale -= STOMP_INTERVAL * pev->speed;
		if ( pev->scale <= 0 )
		{
			// Life has run out
			UTIL_Remove(this);
			STOP_SOUND( edict(), CHAN_BODY, GARG_STOMP_BUZZ_SOUND );
		}

	}
}


void StreakSplash( const Vector &origin, const Vector &direction, int color, int count, int speed, int velocityRange )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, origin );
		WRITE_BYTE( TE_STREAK_SPLASH );
		WRITE_COORD( origin.x );		// origin
		WRITE_COORD( origin.y );
		WRITE_COORD( origin.z );
		WRITE_COORD( direction.x );	// direction
		WRITE_COORD( direction.y );
		WRITE_COORD( direction.z );
		WRITE_BYTE( color );	// Streak color 6
		WRITE_SHORT( count );	// count
		WRITE_SHORT( speed );
		WRITE_SHORT( velocityRange );	// Random velocity modifier
	MESSAGE_END();
}


class CGargantua : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int ObjectCaps( void );
	int IRelationship( CBaseEntity *other, bool ignoreNoTarget = false );
	BOOL FInViewCone( CBaseEntity *entity );
	void CheckAndApplyGenericAttacks( void );
	void AlertSound( void );
	void UseSound( void );
	void UnUseSound( void );
	void OnDestroy( void );
	void UpdateOnRemove( void );
	int  Classify ( void );
	float GetPointsForDamage( float flDamage );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	void HandleAnimEvent( MonsterEvent_t *pEvent );

	BOOL CheckMeleeAttack1( float flDot, float flDist );		// Swipe
	BOOL CheckMeleeAttack1_Move( float flDot, float flDist );
	BOOL CheckMeleeAttack2_Move( float flDot, float flDist );
	BOOL CheckMeleeAttack2( float flDot, float flDist );		// Flames
	BOOL CheckRangeAttack2( float flDot, float flDist );
	BOOL CheckRangeAttack1( float flDot, float flDist );		// Stomp attack
	void SetObjectCollisionBox( void )
	{
		pev->absmin = pev->origin + Vector( -80, -80, 0 );
		pev->absmax = pev->origin + Vector( 80, 80, 214 );
	}

	Schedule_t *GetScheduleOfType( int Type );
	void StartTask( Task_t *pTask );
	void RunTask( Task_t *pTask );

	void PrescheduleThink( void );

	void Killed( entvars_t *pevAttacker, int iGib );
	void DeathEffect( void );

	void CreateEye( void );
	void EyeOff( void );
	void EyeOn( int level );
	void EyeUpdate( void );
	void Leap( void );
	void StompAttack( void );
	void FlameCreate( void );
	bool FlameUpdate( void );
	void FlameControls( float angleX, float angleY );
	void RemoveFlames( void );
	void FlameDestroy( void );
	inline BOOL FlameIsOn( void ) { return m_hFlame[0] != NULL; }

	void FlameDamage( Vector vecStart, Vector vecEnd, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	CUSTOM_SCHEDULES;

private:
	static const char *pAttackHitSounds[];
	static const char *pBeamAttackSounds[];
	static const char *pAttackMissSounds[];
	static const char *pRicSounds[];
	static const char *pFootSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pAttackSounds[];
	static const char *pStompSounds[];
	static const char *pBreatheSounds[];

	CBaseEntity* GargantuaCheckTraceHullAttack(float flDist, int iDamage, int iDmgType);

	EHANDLE					m_hBiteVictim;
	float					m_flBiteBloodTime;
	CEntityHandle<CSprite>	m_hEyeGlow;
	float					m_flEyeAmount;
	CEntityHandle<CBeam>	m_hFlame[4];

	int						m_eyeBrightness;	// Brightness target
	float					m_seeTime;			// Time to attack (when I see the enemy, I set this)
	float					m_flameTime;		// Time of next flame attack
	float					m_painSoundTime;	// Time of next pain sound
	float					m_streakTime;		// streak timer (don't send too many)
	float					m_flameX;			// Flame thrower aim
	float					m_flameY;
	float					m_flEnemyLockTime;
	int						m_iBiteBloodColor;
};

LINK_ENTITY_TO_CLASS( monster_gargantua, CGargantua );

TYPEDESCRIPTION	CGargantua::m_SaveData[] = 
{
	DEFINE_FIELD( CGargantua, m_hEyeGlow, FIELD_EHANDLE ),
	DEFINE_FIELD( CGargantua, m_eyeBrightness, FIELD_INTEGER ),
	DEFINE_FIELD( CGargantua, m_seeTime, FIELD_TIME ),
	DEFINE_FIELD( CGargantua, m_flameTime, FIELD_TIME ),
	DEFINE_FIELD( CGargantua, m_streakTime, FIELD_TIME ),
	DEFINE_FIELD( CGargantua, m_painSoundTime, FIELD_TIME ),
	DEFINE_ARRAY( CGargantua, m_hFlame, FIELD_EHANDLE, 4 ),
	DEFINE_FIELD( CGargantua, m_flameX, FIELD_FLOAT ),
	DEFINE_FIELD( CGargantua, m_flameY, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CGargantua, CBaseMonster );

extern cvar_t sk_gargantua_health;

float CGargantua::GetPointsForDamage( float flDamage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	return ( pev->max_health / sk_gargantua_health.value ) * 25.0f * ( flDamage / pev->max_health );
}

const char *CGargantua::pAttackHitSounds[] = 
{
	"zombie/claw_strike1.wav",
	"zombie/claw_strike2.wav",
	"zombie/claw_strike3.wav",
};

const char *CGargantua::pBeamAttackSounds[] = 
{
	"garg/gar_flameoff1.wav",
	"garg/gar_flameon1.wav",
	"garg/gar_flamerun1.wav",
};


const char *CGargantua::pAttackMissSounds[] = 
{
	"zombie/claw_miss1.wav",
	"zombie/claw_miss2.wav",
};

const char *CGargantua::pRicSounds[] = 
{
#if 0
	"weapons/ric1.wav",
	"weapons/ric2.wav",
	"weapons/ric3.wav",
	"weapons/ric4.wav",
	"weapons/ric5.wav",
#else
	"debris/metal4.wav",
	"debris/metal6.wav",
	"weapons/ric4.wav",
	"weapons/ric5.wav",
#endif
};

const char *CGargantua::pFootSounds[] = 
{
	"garg/gar_step1.wav",
	"garg/gar_step2.wav",
};


const char *CGargantua::pIdleSounds[] = 
{
	"garg/gar_idle1.wav",
	"garg/gar_idle2.wav",
	"garg/gar_idle3.wav",
	"garg/gar_idle4.wav",
	"garg/gar_idle5.wav",
};


const char *CGargantua::pAttackSounds[] = 
{
	"garg/gar_attack1.wav",
	"garg/gar_attack2.wav",
	"garg/gar_attack3.wav",
};

const char *CGargantua::pAlertSounds[] = 
{
	"garg/gar_alert1.wav",
	"garg/gar_alert2.wav",
	"garg/gar_alert3.wav",
};

const char *CGargantua::pPainSounds[] = 
{
	"garg/gar_pain1.wav",
	"garg/gar_pain2.wav",
	"garg/gar_pain3.wav",
};

const char *CGargantua::pStompSounds[] = 
{
	"garg/gar_stomp1.wav",
};

const char *CGargantua::pBreatheSounds[] = 
{
	"garg/gar_breathe1.wav",
	"garg/gar_breathe2.wav",
	"garg/gar_breathe3.wav",
};
//=========================================================
// AI Schedules Specific to this monster
//=========================================================
#if 0
enum
{
	SCHED_ = LAST_COMMON_SCHEDULE + 1,
};
#endif

enum
{
	TASK_SOUND_ATTACK = LAST_COMMON_TASK + 1,
	TASK_FLAME_SWEEP,
	TASK_BITE_HEAD,
	TASK_CHECK_BITE_HULL,
};

Task_t	tlGargFlame[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_SOUND_ATTACK,		(float)0		},
	// { TASK_PLAY_SEQUENCE,		(float)ACT_SIGNAL1	},
	{ TASK_SET_ACTIVITY,		(float)ACT_MELEE_ATTACK2 },
	{ TASK_FLAME_SWEEP,			(float)4.5		},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE	},
};

Schedule_t slGargFlame[] =
{
	{
		tlGargFlame,
		ARRAYSIZE( tlGargFlame ),
		bits_COND_CAN_MELEE_ATTACK1,
		0,
		"GargFlame"
	},
};


// primary melee attack
Task_t	tlGargSwipe[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_MELEE_ATTACK1,		(float)0		},
};

Schedule_t slGargSwipe[] =
{
	{
		tlGargSwipe,
		ARRAYSIZE( tlGargSwipe ),
		0,
		0,
		"GargSwipe"
	},
};


Task_t tlGargBite[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_CHASE_ENEMY },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_CHECK_BITE_HULL, 0 },
	{ TASK_BITE_HEAD, 0 },
};

Schedule_t slGargBite[] =
{
	{
		tlGargBite,
		ARRAYSIZE( tlGargBite ),
		0,
		0,
		"GargBite"
	},
};

DEFINE_CUSTOM_SCHEDULES( CGargantua )
{
	slGargFlame,
	slGargSwipe,
	slGargBite,
};

IMPLEMENT_CUSTOM_SCHEDULES( CGargantua, CBaseMonster );


void CGargantua::CreateEye( void )
{
	CSprite *eye = CSprite::SpriteCreate( GARG_EYE_SPRITE_NAME, pev->origin, FALSE );

	eye->SetTransparency( kRenderGlow, 255, 255, 255, 0, kRenderFxNoDissipation );
	eye->SetColor( IsPlayerAlly() ? 0 : 255, IsPlayerAlly() ? 255 : 0, 0 );
	eye->SetAttachment( edict(), 1 );
	eye->pev->scale = pev->scale;
	m_hEyeGlow = eye;

	if ( m_hEyeGlow )
		m_hEyeGlow->pev->renderamt = m_flEyeAmount;
}

void CGargantua::EyeOn( int level )
{
	if ( !m_hEyeGlow )
		CreateEye();

	m_eyeBrightness = level;
}


void CGargantua::EyeOff( void )
{
	m_eyeBrightness = 0;

	if ( m_hEyeGlow )
	{
		UTIL_Remove( m_hEyeGlow );
		m_hEyeGlow = NULL;
	}
}


void CGargantua::EyeUpdate( void )
{
	if ( m_hEyeGlow )
	{
		m_hEyeGlow->pev->renderamt = UTIL_Approach( m_eyeBrightness, m_hEyeGlow->pev->renderamt, 26 );
		m_flEyeAmount = m_hEyeGlow->pev->renderamt;

		if ( m_hEyeGlow->pev->renderamt == 0 )
			m_hEyeGlow->pev->effects |= EF_NODRAW;
		else
			m_hEyeGlow->pev->effects &= ~EF_NODRAW;
		UTIL_SetOrigin( m_hEyeGlow->pev, pev->origin );
	}
}


#define STOMP_START_SPEED	500

void CGargantua::StompAttack( void )
{
	TraceResult trace;

	UTIL_MakeVectors( pev->angles );
	Vector vecStart = pev->origin + Vector(0,0,60) + 35 * pev->scale * gpGlobals->v_forward;
	Vector vecAim = ShootAtEnemy( vecStart );
	Vector vecEnd = (vecAim * 1024 * pev->scale) + vecStart;

	UTIL_TraceLine( vecStart, vecEnd, ignore_monsters, edict(), &trace );
	CStomp::StompCreate( vecStart, trace.vecEndPos, STOMP_START_SPEED, pev );
	UTIL_ScreenShake( pev->origin, 12.0, 100.0, 2.0, 1000 );
	EMIT_SOUND_DYN ( edict(), CHAN_WEAPON, pStompSounds[ RANDOM_LONG(0,ARRAYSIZE(pStompSounds)-1) ], 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG(-10,10) );

	UTIL_TraceLine( pev->origin, pev->origin - Vector(0,0,20), ignore_monsters, edict(), &trace );
	if ( trace.flFraction < 1.0 )
		UTIL_DecalTrace( &trace, DECAL_GARGSTOMP1 );
}


void CGargantua :: FlameCreate( void )
{
	int			i;
	Vector		posGun, angleGun;
	TraceResult trace;

	UTIL_MakeVectors( pev->angles );

	for ( i = 0; i < 4; i++ )
	{
		if ( i < 2 )
			m_hFlame[i] = CBeam::BeamCreate( GARG_BEAM_SPRITE_NAME, 240 * pev->scale );
		else
			m_hFlame[i] = CBeam::BeamCreate( GARG_BEAM_SPRITE2, 140 * pev->scale );
		if ( m_hFlame[i] )
		{
			int attach = i%2;
			// attachment is 0 based in GetAttachment
			GetAttachment( attach+1, posGun, angleGun );

			Vector vecEnd = (gpGlobals->v_forward * GARG_FLAME_LENGTH * pev->scale ) + posGun;
			UTIL_TraceLine( posGun, vecEnd, dont_ignore_monsters, edict(), &trace );

			m_hFlame[i]->PointEntInit( trace.vecEndPos, entindex() );
			if ( i < 2 )
				m_hFlame[i]->SetColor( 255, 130, 90 );
			else
				m_hFlame[i]->SetColor( 0, 120, 255 );
			m_hFlame[i]->SetBrightness( 190 );
			m_hFlame[i]->SetFlags( BEAM_FSHADEIN );
			m_hFlame[i]->SetScrollRate( 20 );
			// attachment is 1 based in SetEndAttachment
			m_hFlame[i]->SetEndAttachment( attach + 2 );
			CSoundEnt::InsertSound( bits_SOUND_COMBAT, posGun, 384, 0.3, this );
		}
	}
	EMIT_SOUND_DYN ( edict(), CHAN_BODY, pBeamAttackSounds[ 1 ], 1.0, ATTN_NORM, 0, PITCH_NORM );
	EMIT_SOUND_DYN ( edict(), CHAN_WEAPON, pBeamAttackSounds[ 2 ], 1.0, ATTN_NORM, 0, PITCH_NORM );
}


void CGargantua :: FlameControls( float angleX, float angleY )
{
	if ( angleY < -180 )
		angleY += 360;
	else if ( angleY > 180 )
		angleY -= 360;

	if ( angleY < -45 )
		angleY = -45;
	else if ( angleY > 45 )
		angleY = 45;

	m_flameX = UTIL_ApproachAngle( angleX, m_flameX, 4 );
	m_flameY = UTIL_ApproachAngle( angleY, m_flameY, 8 );
	SetBoneController( 0, m_flameY );
	SetBoneController( 1, m_flameX );
}


bool CGargantua :: FlameUpdate( void )
{
	int				i;
	TraceResult		trace;
	Vector			vecStart, angleGun;
	BOOL			streaks = FALSE;

	for ( i = 0; i < 2; i++ )
	{
		if ( !m_hFlame[i] )
			return false;

		if ( m_hFlame[i] )
		{
			Vector vecAim = pev->angles;
			vecAim.x += m_flameX;
			vecAim.y += m_flameY;

			UTIL_MakeVectors( vecAim );

			GetAttachment( i+1, vecStart, angleGun );
			Vector vecEnd = vecStart + (gpGlobals->v_forward * GARG_FLAME_LENGTH * pev->scale ); //  - offset[i] * gpGlobals->v_right;

			UTIL_TraceLine( vecStart, vecEnd, dont_ignore_monsters, edict(), &trace );

			m_hFlame[i]->SetStartPos( trace.vecEndPos );
			if ( m_hFlame[i + 2] )
				m_hFlame[i+2]->SetStartPos( (vecStart * 0.6) + (trace.vecEndPos * 0.4) );

			if ( trace.flFraction != 1.0 && gpGlobals->time > m_streakTime )
			{
				StreakSplash( trace.vecEndPos, trace.vecPlaneNormal, 6, 20, 50, 400 );
				streaks = TRUE;
				UTIL_DecalTrace( &trace, DECAL_SMALLSCORCH1 + RANDOM_LONG(0,2) );
			}
			// RadiusDamage( trace.vecEndPos, pev, pev, gSkillData.gargantuaDmgFire, CLASS_ALIEN_MONSTER, DMG_BURN );
			FlameDamage( vecStart, trace.vecEndPos, pev, pev, gSkillData.gargantuaDmgFire, CLASS_ALIEN_MONSTER, DMG_BURN );

			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_ELIGHT );
				WRITE_SHORT( entindex( ) + 0x1000 * (i + 2) );		// entity, attachment
				WRITE_COORD( vecStart.x );		// origin
				WRITE_COORD( vecStart.y );
				WRITE_COORD( vecStart.z );
				WRITE_COORD( RANDOM_FLOAT( 32, 48 ) );	// radius
				WRITE_BYTE( 255 );	// R
				WRITE_BYTE( 255 );	// G
				WRITE_BYTE( 255 );	// B
				WRITE_BYTE( 2 );	// life * 10
				WRITE_COORD( 0 ); // decay
			MESSAGE_END();
		}
	}
	if ( streaks )
		m_streakTime = gpGlobals->time;

	return true;
}



void CGargantua :: FlameDamage( Vector vecStart, Vector vecEnd, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType )
{
	CBaseEntity *pEntity = NULL;
	TraceResult	tr;
	float		flAdjustedDamage;
	Vector		vecSpot;

	Vector vecMid = (vecStart + vecEnd) * 0.5;

	float searchRadius = (vecStart - vecMid).Length();

	Vector vecAim = (vecEnd - vecStart).Normalize( );

	// iterate on all entities in the vicinity.
	while ((pEntity = UTIL_FindEntityInSphere( pEntity, vecMid, searchRadius )) != NULL)
	{
		if ( pEntity->pev->takedamage != DAMAGE_NO && IRelationship( pEntity ) > R_NO )
		{
			vecSpot = pEntity->BodyTarget( vecMid );

			float dist = DotProduct( vecAim, vecSpot - vecMid );
			if (dist > searchRadius)
				dist = searchRadius;
			else if (dist < -searchRadius)
				dist = searchRadius;

			Vector vecSrc = vecMid + dist * vecAim;

			UTIL_TraceLine ( vecSrc, vecSpot, dont_ignore_monsters, ENT(pev), &tr );

			if ( tr.flFraction == 1.0 || tr.pHit == pEntity->edict() )
			{// the explosion can 'see' this entity, so hurt them!
				// decrease damage for an ent that's farther from the flame.
				dist = ( vecSrc - tr.vecEndPos ).Length();

				if (dist > 64 * pev->scale )
				{
					flAdjustedDamage = flDamage - (dist - 64 * pev->scale ) * 0.4;
					if (flAdjustedDamage <= 0)
						continue;
				}
				else
				{
					flAdjustedDamage = flDamage;
				}

				// ALERT( at_console, "hit %s\n", STRING( pEntity->pev->classname ) );
				if (tr.flFraction != 1.0)
				{
					ClearMultiDamage( );
					pEntity->TraceAttack( pevInflictor, flAdjustedDamage, (tr.vecEndPos - vecSrc).Normalize( ), &tr, bitsDamageType );
					ApplyMultiDamage( pevInflictor, pevAttacker );
				}
				else
				{
					pEntity->TakeDamage ( pevInflictor, pevAttacker, flAdjustedDamage, bitsDamageType );
				}
			}
		}
	}
}


void CGargantua::RemoveFlames( void )
{
	for ( int i = 0; i < 4; ++i )
	{
		if ( m_hFlame[i] )
		{
			m_hFlame[i]->pev->aiment = NULL;
			UTIL_Remove( m_hFlame[i] );
			m_hFlame[i] = NULL;
		}
	}
}

void CGargantua::OnDestroy( void )
{
	EyeOff();
	RemoveFlames();
	CBaseMonster::OnDestroy();
}

void CGargantua::UpdateOnRemove( void )
{
	CBaseEntity::UpdateOnRemove();
	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, pBeamAttackSounds[0], 1.0, ATTN_NORM, SND_STOP, PITCH_NORM );
	EyeOff();
	RemoveFlames();
}

void CGargantua :: FlameDestroy( void )
{
	EMIT_SOUND_DYN ( edict(), CHAN_WEAPON, pBeamAttackSounds[ 0 ], 1.0, ATTN_NORM, 0, PITCH_NORM );
	RemoveFlames();
}


void CGargantua :: PrescheduleThink( void )
{
	CBaseEntity *victim = m_hBiteVictim;

	if ( !victim )
	{
		m_flThinkDelay = 0.1;
	}
	else
	{
		Vector position, angles;

		GetAttachment( 0, position, angles );

		if ( gpGlobals->time > m_flBiteBloodTime )
		{
			m_flBiteBloodTime = gpGlobals->time + 0.5;
			UTIL_BloodStream( position - Vector( 0, 0, 16 ), g_vecZero, victim->BloodColor(), 100 );
		}

		float height = victim->pev->absmax.z - victim->pev->absmin.z;

		if ( victim->IsPlayer() )
			position.z -= 40;
		else
			position.z -= height;

		victim->pev->origin = position - gpGlobals->v_forward * 4;
		victim->pev->movetype = MOVETYPE_NOCLIP;
		victim->pev->solid = SOLID_NOT;
		victim->pev->flags &= ~FL_ONGROUND;
		victim->pev->gravity = 0;
		UTIL_SetOrigin( victim->pev, victim->pev->origin );
	}

	CBaseEntity *ground;

	if ( pev->groundentity )
		ground = CBaseEntity::Instance( pev->groundentity );
	else
		ground = NULL;

	if ( ground && ( ground->pev->flags & ( FL_MONSTER | FL_CLIENT ) ) )
	{
		ground->TakeDamage( pev, pev, ground->pev->health, DMG_CRUSH );
		pev->groundentity = NULL;
		pev->flags &= ~FL_ONGROUND;
		pev->velocity.z -= 1;
	}

	if ( !HasConditions( bits_COND_SEE_ENEMY ) )
	{
		m_seeTime = gpGlobals->time + 5;
		EyeOff();
	}
	else
		EyeOn( 200 );

	EyeUpdate();
}


//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CGargantua :: Classify ( void )
{
	return GetClassification( CLASS_ALIEN_MONSTER );
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CGargantua :: SetYawSpeed ( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_IDLE:
		ys = 120;
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 270;
		break;
	case ACT_WALK:
	case ACT_RUN:
		ys = 360;
		break;

	default:
		ys = 120;
		break;
	}

	pev->yaw_speed = ys;
}


//=========================================================
// Spawn
//=========================================================
void CGargantua :: Spawn()
{
	Precache( );

	if ( !SetupModel() )
		UTIL_SetModel( this, "models/garg.mdl");
	UTIL_SetSize( pev, Vector( -32, -32, 0 ), Vector( 32, 32, 64 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_GREEN;
	if ( pev->health == 0.0f )
		pev->health			= sk_gargantua_health.value;
	//pev->view_ofs		= Vector ( 0, 0, 96 );// taken from mdl file
	m_flFieldOfView		= -0.2;// width of forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;

	m_afCapability |= bits_CAP_TURN_HEAD | bits_CAP_RANGE_ATTACK2;
	m_afMoveShootCap = 0;
	m_hBiteVictim = NULL;
	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( "Gargantua" );

	MonsterInit();

	if ( m_fPlayerAlly && !( pev->spawnflags & SF_MONSTER_PREDISASTER ) )
	{
		SetUse( &CBaseMonster::CallFollowerPlayerUse );
		m_fCanPlayerFollow = true;
	}
	EyeOff();
	m_seeTime = gpGlobals->time + 5;
	m_flameTime = gpGlobals->time + 2;
	m_iBiteBloodColor = BLOOD_COLOR_RED;
}


//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CGargantua :: Precache()
{
	int i;

	CBaseEntity::Precache();

	UTIL_PrecacheModel( this, "models/garg.mdl");
	UTIL_PrecacheModel( this, GARG_EYE_SPRITE_NAME );
	UTIL_PrecacheModel( this, GARG_BEAM_SPRITE_NAME );
	UTIL_PrecacheModel( this, GARG_BEAM_SPRITE2 );
	gStompSprite = UTIL_PrecacheModel( this, GARG_STOMP_SPRITE_NAME );
	gGargGibModel = UTIL_PrecacheModel( this, GARG_GIB_MODEL );
	PRECACHE_SOUND( GARG_STOMP_BUZZ_SOUND );
	PRECACHE_SOUND( "bullchicken/bc_bite3.wav" );
	PRECACHE_SOUND( "barnacle/bcl_chew1.wav" );
	PRECACHE_SOUND( "tentacle/te_roar1.wav" );

	for ( i = 0; i < ARRAYSIZE( pAttackHitSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackHitSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pBeamAttackSounds ); i++ )
		PRECACHE_SOUND((char *)pBeamAttackSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAttackMissSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackMissSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pRicSounds ); i++ )
		PRECACHE_SOUND((char *)pRicSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pFootSounds ); i++ )
		PRECACHE_SOUND((char *)pFootSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pIdleSounds ); i++ )
		PRECACHE_SOUND((char *)pIdleSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAlertSounds ); i++ )
		PRECACHE_SOUND((char *)pAlertSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pPainSounds ); i++ )
		PRECACHE_SOUND((char *)pPainSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAttackSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pStompSounds ); i++ )
		PRECACHE_SOUND((char *)pStompSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pBreatheSounds ); i++ )
		PRECACHE_SOUND((char *)pBreatheSounds[i]);
}	


void CGargantua::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	ALERT( at_aiconsole, "CGargantua::TraceAttack\n");

	if ( !IsAlive() )
	{
		CBaseMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
		return;
	}

	// UNDONE: Hit group specific damage?
	if ( bitsDamageType & (GARG_DAMAGE|DMG_BLAST) )
	{
		if ( m_painSoundTime < gpGlobals->time )
		{
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pPainSounds[ RANDOM_LONG(0,ARRAYSIZE(pPainSounds)-1) ], 1.0, ATTN_GARG, 0, PITCH_NORM );
			m_painSoundTime = gpGlobals->time + RANDOM_FLOAT( 2.5, 4 );
		}

		if ( m_hEnemy != NULL && pevAttacker != m_hEnemy->pev )
		{
			CBaseEntity *attacker;

			if ( pevAttacker )
				attacker = CBaseEntity::Instance( pevAttacker );
			else
				attacker = CBaseEntity::Instance( (edict_t *) NULL );

			if ( IRelationship( attacker ) > R_NO )
			{
				m_hEnemy = attacker;
				m_flEnemyLockTime = gpGlobals->time + 5;

				if ( m_hBiteVictim == NULL )
					ChangeSchedule( GetScheduleOfType( SCHED_COMBAT_FACE ) );
			}
		}
	}

	bitsDamageType &= GARG_DAMAGE;

	if ( bitsDamageType == 0)
	{
		if ( pev->dmgtime != gpGlobals->time || (RANDOM_LONG(0,100) < 20) )
		{
			UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT(0.5,1.5) );
			pev->dmgtime = gpGlobals->time + 0.1;
//			if ( RANDOM_LONG(0,100) < 25 )
//				EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, pRicSounds[ RANDOM_LONG(0,ARRAYSIZE(pRicSounds)-1) ], 1.0, ATTN_NORM, 0, PITCH_NORM );
		}
		flDamage = 0;

		if ( pevAttacker && ( pevAttacker->flags & ( FL_MONSTER | FL_CLIENT ) ) && m_hEnemy == NULL )
		{
			MakeIdealYaw( pevAttacker->origin );
			SetConditions( bits_COND_LIGHT_DAMAGE );
		}
	}

	CBaseMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );

}



int CGargantua::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	bool fEnemyGarg = false;

	if ( pevAttacker && ( FClassnameIs( pevAttacker, "monster_gargantua" ) || FClassnameIs( pevAttacker, "monster_babygarg" ) ) )
	{
		if ( IRelationship( CBaseEntity::Instance( pevAttacker ) ) <= R_NO )
			flDamage = 0;
		else
			fEnemyGarg = true;
	}

	if ( IsAlive() )
	{
		if ( FClassnameIs( pevInflictor, "shock_beam" ) )
		{
			flDamage *= 0.2;
		}
		else
		{
			if ( !(bitsDamageType & GARG_DAMAGE) && !fEnemyGarg )
				flDamage *= 0.01;

			if ( bitsDamageType & DMG_BLAST )
				SetConditions( bits_COND_LIGHT_DAMAGE );
		}
	}

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}


void CGargantua::DeathEffect( void )
{
	int i;
	UTIL_MakeVectors(pev->angles);
	Vector deathPos = pev->origin + gpGlobals->v_forward * 100;

	// Create a spiral of streaks
	CSpiral::Create( deathPos, (pev->absmax.z - pev->absmin.z) * 0.6, 125, 1.5 );

	Vector position = pev->origin;
	position.z += 32;
	for ( i = 0; i < 7; i+=2 )
	{
		SpawnExplosion( pev, position, 70, (i * 0.3), 60 + (i*20) );
		position.z += 15;
	}

	CBaseEntity *pSmoker = CBaseEntity::Create( "env_smoker", pev->origin, g_vecZero, false, NULL );
	pSmoker->pev->health = 1;	// 1 smoke balls
	pSmoker->pev->scale = 46;	// 4.6X normal size
	pSmoker->pev->dmg = 0;		// 0 radial distribution
	pSmoker->pev->nextthink = gpGlobals->time + 2.5;	// Start in 2.5 seconds
}


void CGargantua::Killed( entvars_t *pevAttacker, int iGib )
{
	CBaseEntity *victim = m_hBiteVictim;

	if ( victim )
	{
		if ( victim->IsPlayer() )
			victim->pev->movetype = MOVETYPE_WALK;

		if ( victim->IsPlayer() || victim->IsMonster() )
			victim->MyMonsterPointer()->BarnacleVictimReleased();

		victim->pev->gravity = 1;
		victim->pev->solid = SOLID_BBOX;
		victim->pev->flags &= ~FL_ONGROUND;
		m_hBiteVictim = NULL;
	}

	EyeOff();
	CBaseMonster::Killed( pevAttacker, GIB_NEVER );
}

//=========================================================
// CheckMeleeAttack1
// Garg swipe attack
// 
//=========================================================
BOOL CGargantua::CheckMeleeAttack1( float flDot, float flDist )
{
//	ALERT(at_aiconsole, "CheckMelee(%f, %f)\n", flDot, flDist);

	if (flDot >= 0.7)
	{
		if (flDist <= pev->scale * 112.0 )
			return TRUE;
	}
	return FALSE;
}

BOOL CGargantua::CheckMeleeAttack1_Move( float flDot, float flDist )
{
	return HasConditions( bits_COND_SEE_ENEMY )
	       && flDist <= pev->scale * 80.0 * 2.0 && flDot >= 0.6;
}

BOOL CGargantua::CheckMeleeAttack2_Move( float flDot, float flDist )
{
	return HasConditions( bits_COND_SEE_ENEMY )
	       && flDist <= pev->scale * 80.0 * 6.0 && flDot >= 0.6;
}


// Flame thrower madness!
BOOL CGargantua::CheckMeleeAttack2( float flDot, float flDist )
{
//	ALERT(at_aiconsole, "CheckMelee(%f, %f)\n", flDot, flDist);

	if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1_MOVING ) )
		return FALSE;

	CBaseEntity *pEnemy = m_hEnemy;

	if ( pEnemy && pEnemy->pev->health < 50.0f && m_pSchedule != slFail )
		return FALSE;

	if ( !(flDot >= 0.8 && flDist > pev->scale * 80.0 ) && m_pSchedule != slFail )
		return FALSE;

	return flDist <= pev->scale * 330.0f;
}


//=========================================================
// CheckRangeAttack1
// flDot is the cos of the angle of the cone within which
// the attack can occur.
//=========================================================
//
// Stomp attack
//
//=========================================================
BOOL CGargantua::CheckRangeAttack2( float flDot, float flDist )
{
	if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1_MOVING ) )
		return FALSE;

	CBaseEntity *enemy = m_hEnemy;

	if ( !enemy || enemy->pev->health > 50 )
		return FALSE;

	return enemy->pev->movetype != MOVETYPE_FLY
	       && enemy->pev->movetype != MOVETYPE_BOUNCE
	       && flDot >= 0.7f && flDist <= pev->scale * 80
	       && enemy->pev->size.z > 12 && enemy->pev->size.z < 96;
}

BOOL CGargantua::CheckRangeAttack1( float flDot, float flDist )
{
	CBaseEntity *pEnemy = m_hEnemy;

	if ( pEnemy && pEnemy->pev->deadflag == DEAD_DYING )
		return FALSE;

	return gpGlobals->time > m_seeTime && flDot >= 0.7 && flDist > pev->scale * 80.0;
}




//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CGargantua::HandleAnimEvent(MonsterEvent_t *pEvent)
{
	switch( pEvent->event )
	{
	case GARG_AE_SLASH_LEFT:
		{
			// HACKHACK!!!
			CBaseEntity *pHurt = GargantuaCheckTraceHullAttack( GARG_ATTACKDIST + 10.0, gSkillData.gargantuaDmgSlash, DMG_SLASH );
			if (pHurt)
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.x = -30; // pitch
					pHurt->pev->punchangle.y = -30;	// yaw
					pHurt->pev->punchangle.z = 30;	// roll
					//UTIL_MakeVectors(pev->angles);	// called by CheckTraceHullAttack
					pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_right * 100;
				}
				EMIT_SOUND_DYN ( edict(), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, 50 + RANDOM_LONG(0,15) );
			}
			else // Play a random attack miss sound
				EMIT_SOUND_DYN ( edict(), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, 50 + RANDOM_LONG(0,15) );

			Vector forward;
			UTIL_MakeVectorsPrivate( pev->angles, forward, NULL, NULL );
		}
		break;

	case GARG_AE_RIGHT_FOOT:
	case GARG_AE_LEFT_FOOT:
		UTIL_ScreenShake( pev->origin, 4.0, 3.0, 1.0, 750 );
		EMIT_SOUND_DYN ( edict(), CHAN_BODY, pFootSounds[ RANDOM_LONG(0,ARRAYSIZE(pFootSounds)-1) ], 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG(-10,10) );
		break;

	case GARG_AE_STOMP:
		StompAttack();
		m_seeTime = gpGlobals->time + 12;
		break;

	case GARG_AE_BREATHE:
		EMIT_SOUND_DYN ( edict(), CHAN_VOICE, pBreatheSounds[ RANDOM_LONG(0,ARRAYSIZE(pBreatheSounds)-1) ], 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG(-10,10) );
		break;

	case GARG_AE_GRAB:
		{
			UTIL_ScreenShake( pev->origin, 4.0, 3.0, 1.0, 750 );
			EMIT_SOUND_DYN( edict(), CHAN_BODY, pFootSounds[RANDOM_LONG( 0, ARRAYSIZE( pFootSounds ) - 1 )], 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG( -10, 10 ) );

			if ( m_MonsterState != MONSTERSTATE_SCRIPT )
			{
				CBaseEntity *victim = GargantuaCheckTraceHullAttack( pev->scale * 144, 1, DMG_SLASH );

				if ( victim && victim == (CBaseEntity *) m_hEnemy )
				{
					m_hBiteVictim = victim;
					victim->FBecomeProne( this );
					m_flThinkDelay = 0;
				}
				else
					TaskFail();
			}
		}
		break;

	case GARG_AE_BITE:
		{
			EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "garg/gar_idle3.wav", 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG( -10, 10 ) );

			CBaseEntity *victim = m_hBiteVictim;

			if ( m_MonsterState != MONSTERSTATE_SCRIPT && victim )
			{
				Vector position, angles;

				m_iBiteBloodColor = victim->BloodColor();
				GetBonePosition( 22, position, angles );
				UTIL_BloodStream( position - Vector( 0, 0, 16 ), g_vecZero, m_iBiteBloodColor, 600 );
				EMIT_AMBIENT_SOUND( edict(), position, "bullchicken/bc_bite3.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );

				if ( victim->IsPlayer() )
				{
					victim->pev->armorvalue = 0;
					victim->pev->movetype = MOVETYPE_WALK;
				}

				victim->pev->health = -100;
				victim->pev->gravity = 1;
				victim->pev->solid = SOLID_NOT;
				victim->pev->flags &= ~FL_ONGROUND;
				victim->Killed( pev, GIB_ALWAYS );
				m_hBiteVictim = NULL;
			}
		}
		break;

	case GARG_AE_BITE_BLOOD:
		{
			Vector position, angles;

			GetBonePosition( 22, position, angles );
			UTIL_BloodStream( position, g_vecZero, m_iBiteBloodColor, 600 );
			m_iBiteBloodColor = BLOOD_COLOR_RED;
		}
		break;

	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}


//=========================================================
// CheckTraceHullAttack - expects a length to trace, amount 
// of damage to do, and damage type. Returns a pointer to
// the damaged entity in case the monster wishes to do
// other stuff to the victim (punchangle, etc)
// Used for many contact-range melee attacks. Bites, claws, etc.

// Overridden for Gargantua because his swing starts lower as
// a percentage of his height (otherwise he swings over the
// players head)
//=========================================================
CBaseEntity* CGargantua::GargantuaCheckTraceHullAttack(float flDist, int iDamage, int iDmgType)
{
	TraceResult tr;

	UTIL_MakeVectors( pev->angles );
	Vector vecStart = pev->origin + Vector( 0, 0, 32 );
	CBaseEntity *enemy = m_hEnemy;

	if ( enemy )
	{
		float height = enemy->Center().z;

		if ( height >= pev->origin.z && height <= pev->origin.z + 214 )
			vecStart.z = height;
	}

	Vector vecEnd = vecStart + gpGlobals->v_forward * flDist;

	if ( enemy )
	{
		if ( enemy->IsBSPModel() )
		{
			UTIL_TraceLine( vecStart, m_vecEnemyLKP, dont_ignore_monsters, edict(), &tr );

			if ( tr.pHit )
			{
				if ( iDamage > 0 )
					enemy->TakeDamage( pev, pev, iDamage, iDmgType );

				return enemy;
			}
		}

		if ( ( enemy->Center() - vecStart ).Length() < flDist )
		{
			if ( iDamage > 0 )
				enemy->TakeDamage( pev, pev, iDamage, iDmgType );

			return enemy;
		}
	}

	UTIL_TraceHull( vecStart, vecEnd, dont_ignore_monsters, head_hull, edict(), &tr );

	if ( tr.pHit )
	{
		CBaseEntity *entity = CBaseEntity::Instance( tr.pHit );

		if ( iDamage > 0 )
			entity->TakeDamage( pev, pev, iDamage, iDmgType );

		return entity;
	}

	return NULL;
}


Schedule_t *CGargantua::GetScheduleOfType( int Type )
{
	CBaseEntity *victim = m_hBiteVictim;

	if ( victim )
	{
		if ( victim->IsPlayer() )
			victim->pev->movetype = MOVETYPE_WALK;
		else if ( victim->IsMonster() )
			victim->MyMonsterPointer()->BarnacleVictimReleased();

		victim->pev->gravity = 1;
		victim->pev->solid = SOLID_BBOX;
		victim->pev->flags &= ~FL_ONGROUND;
		m_hBiteVictim = NULL;
	}

	// HACKHACK - turn off the flames if they are on and garg goes scripted / dead
	if ( FlameIsOn() )
		FlameDestroy();

	switch( Type )
	{
		case SCHED_CHASE_ENEMY_FAILED:
		return slFail;
	case SCHED_RANGE_ATTACK2:
		pev->yaw_speed = 360;
		return slGargBite;
	case SCHED_MELEE_ATTACK2:
			return slGargFlame;
		case SCHED_MELEE_ATTACK1:
			return slGargSwipe;
	}

	return CBaseMonster::GetScheduleOfType( Type );
}


void CGargantua::StartTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_STOP_MOVING:
		FlameControls( 0, 0 );
		SetBoneController( 0, 0 );
		SetBoneController( 1, 0 );
		CBaseMonster::StartTask( pTask );
		break;

	case TASK_BITE_HEAD:
		SetSequenceByName( "bitehead" );
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "garg/gar_alert1.wav", 1.0, ATTN_GARG, 0, 95 );
		break;

	case TASK_CHECK_BITE_HULL:
		{
			CBaseEntity *hit = GargantuaCheckTraceHullAttack( pev->scale * 112, 1, DMG_SLASH );
			CBaseEntity *enemy = m_hEnemy;

			if ( hit && ( IRelationship( hit ) > R_NO || hit == enemy ) )
			{
				TaskComplete();
				m_flThinkDelay = 0;
			}
			else if ( !hit && enemy && ( enemy->Center() - Center() ).Length() < pev->scale * 112 )
			{
				MakeIdealYaw( enemy->Center() );
				ChangeYaw( (int) pev->yaw_speed );
				TaskComplete();
				m_flThinkDelay = 0;
			}
			else
				TaskFail();
		}
		break;

	case TASK_FLAME_SWEEP:
		FlameCreate();
		m_flWaitFinished = gpGlobals->time + pTask->flData;
		m_flameTime = gpGlobals->time + 6;
		m_flameX = 0;
		m_flameY = 0;
		break;

	case TASK_SOUND_ATTACK:
		if ( RANDOM_LONG(0,100) < 30 )
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pAttackSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackSounds)-1) ], 1.0, ATTN_GARG, 0, PITCH_NORM );

		if ( !HasConditions( bits_COND_TASK_FAILED ) )
			TaskComplete();
		break;

	case TASK_DIE:
		m_flWaitFinished = gpGlobals->time + 1.6;
		DeathEffect();
		// FALL THROUGH
	default:
		CBaseMonster::StartTask( pTask );
		break;
	}
}

//=========================================================
// RunTask
//=========================================================
void CGargantua::RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_BITE_HEAD:
		if ( m_hBiteVictim == NULL )
		{
			if ( m_hEnemy != NULL )
				MakeIdealYaw( m_hEnemy->Center() );

			ChangeYaw( (int) pev->yaw_speed );
		}

		if ( m_fSequenceFinished )
		{
			if ( !HasConditions( bits_COND_TASK_FAILED ) )
				TaskComplete();

			m_hBiteVictim = NULL;
		}
		break;

	case TASK_DIE:
		if ( gpGlobals->time > m_flWaitFinished )
		{
			pev->renderfx = kRenderFxExplode;
			pev->rendercolor.x = 255;
			pev->rendercolor.y = 0;
			pev->rendercolor.z = 0;
			StopAnimation();
			pev->nextthink = gpGlobals->time + 0.15;
			SetThink( &CGargantua::SUB_Remove );
			int i;
			int parts = MODEL_FRAMES( gGargGibModel );
			for ( i = 0; i < 10; i++ )
			{
				CGib *pGib = GetClassPtr( (CGib *)NULL );

				pGib->Spawn( GARG_GIB_MODEL );

				int bodyPart = 0;
				if ( parts > 1 )
					bodyPart = RANDOM_LONG( 0, pev->body-1 );

				pGib->pev->body = bodyPart;
				pGib->m_bloodColor = BLOOD_COLOR_YELLOW;
				pGib->m_material = matNone;
				pGib->pev->origin = pev->origin;
				pGib->pev->velocity = UTIL_RandomBloodVector() * RANDOM_FLOAT( 300, 500 );
				pGib->pev->nextthink = gpGlobals->time + 1.25;
				pGib->SetThink( &CGib::SUB_FadeOut );
			}
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
				WRITE_BYTE( TE_BREAKMODEL);

				// position
				WRITE_COORD( pev->origin.x );
				WRITE_COORD( pev->origin.y );
				WRITE_COORD( pev->origin.z );

				// size
				WRITE_COORD( 200 );
				WRITE_COORD( 200 );
				WRITE_COORD( 128 );

				// velocity
				WRITE_COORD( 0 );
				WRITE_COORD( 0 );
				WRITE_COORD( 0 );

				// randomization
				WRITE_BYTE( 200 );

				// Model
				WRITE_SHORT( gGargGibModel );	//model id#

				// # of shards
				WRITE_BYTE( 50 );

				// duration
				WRITE_BYTE( 20 );// 3.0 seconds

				// flags

				WRITE_BYTE( BREAK_FLESH );
			MESSAGE_END();

			return;
		}
		else
			CBaseMonster::RunTask(pTask);
		break;

	case TASK_FLAME_SWEEP:
		if ( gpGlobals->time > m_flWaitFinished )
		{
			FlameDestroy();
			TaskComplete();
			FlameControls( 0, 0 );
			SetBoneController( 0, 0 );
			SetBoneController( 1, 0 );
		}
		else
		{
			BOOL cancel = FALSE;

			Vector angles = g_vecZero;

			FlameUpdate();
			CBaseEntity *pEnemy = m_hEnemy;
			if ( pEnemy )
			{
				Vector org = pev->origin;
				org.z += 64 * pev->scale;
				Vector dir = pEnemy->BodyTarget(org) - org;
				angles = UTIL_VecToAngles( dir );
				angles.x = -angles.x;
				angles.y -= pev->angles.y;
				if ( dir.Length() > 400 * pev->scale )
					cancel = TRUE;
			}
			if ( fabs(angles.y) > 60 )
				cancel = TRUE;

			if ( cancel )
			{
				m_flWaitFinished -= 0.5;
				m_flameTime -= 0.5;
			}
			// FlameControls( angles.x + 2 * sin(gpGlobals->time*8), angles.y + 28 * sin(gpGlobals->time*8.5) );
			FlameControls( angles.x, angles.y );
		}
		break;

	default:
		CBaseMonster::RunTask( pTask );
		break;
	}
}



int CGargantua::ObjectCaps( void )
{
	return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

int CGargantua::IRelationship( CBaseEntity *other, bool ignoreNoTarget )
{
	if ( !other )
		return R_NO;

	CBaseEntity *enemy = m_hEnemy;

	if ( m_flEnemyLockTime > gpGlobals->time - 3 && enemy == other && enemy->pev->health > 35 )
		return R_FR;

	if ( ( other->pev->flags & FL_MONSTER ) && other->MyMonsterPointer()->m_fOverrideClass )
		return CBaseEntity::IRelationship( other, ignoreNoTarget );

	if ( !IsPlayerAlly() && !m_fOverrideClass
	     && ( FClassnameIs( other->pev, "monster_alien_babyvoltigore" )
	          || FClassnameIs( other->pev, "monster_shocktrooper" )
	          || FClassnameIs( other->pev, "monster_alien_voltigore" ) ) )
		return R_DL;

	int relationship = CBaseEntity::IRelationship( other, ignoreNoTarget );

	return relationship == R_HT ? R_DL : relationship;
}

BOOL CGargantua::FInViewCone( CBaseEntity *entity )
{
	if ( m_flEnemyLockTime > gpGlobals->time && m_hEnemy != NULL && (CBaseEntity *) m_hEnemy == entity )
		return TRUE;

	return CBaseMonster::FInViewCone( entity );
}

void CGargantua::AlertSound( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "garg/gar_alert1.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
}

void CGargantua::UseSound( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "garg/gar_alert1.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
}

void CGargantua::UnUseSound( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_BODY, RANDOM_LONG( 0, 1 ) ? "garg/gar_pain2.wav" : "garg/gar_pain3.wav", 0.8, ATTN_NORM, 0, PITCH_NORM );
}

void CGargantua::CheckAndApplyGenericAttacks( void )
{
	if ( !m_afMoveShootCap )
	{
		m_IdealActivity = m_movementActivity;
		pev->gaitsequence = GAIT_SEQUENCE_NONE;
		return;
	}

	if ( m_hEnemy != NULL )
		CheckAttacks_Move( m_hEnemy, ( m_hEnemy->pev->origin - pev->origin ).Length() );

	CheckAmmo();

	bool stopAttack = false;
	bool activeAttack = true;

	switch ( GetSequenceActivityName( pev->sequence ) )
	{
	case ACT_RANGE_ATTACK1:
		stopAttack = !HasConditions( bits_COND_CAN_RANGE_ATTACK1_MOVING );
		break;

	case ACT_RANGE_ATTACK2:
		stopAttack = !HasConditions( bits_COND_CAN_RANGE_ATTACK2_MOVING );
		break;

	case ACT_MELEE_ATTACK1:
		if ( !HasConditions( bits_COND_CAN_MELEE_ATTACK1_MOVING ) && m_fSequenceFinished )
			stopAttack = true;
		else if ( FlameIsOn() )
			FlameDestroy();

		m_movementActivity = ACT_RUN;
		break;

	case ACT_MELEE_ATTACK2:
		if ( HasConditions( bits_COND_CAN_MELEE_ATTACK2_MOVING ) )
		{
			if ( FlameUpdate() )
			{
				Vector angles = g_vecZero;
				bool cancel = false;

				if ( m_hEnemy != NULL )
				{
					Vector origin = pev->origin + Vector( 0, 0, 64 );
					Vector direction = m_hEnemy->BodyTarget( origin ) - origin;

					angles = UTIL_VecToAngles( direction );
					angles.x = -angles.x;
					angles.y -= pev->angles.y;
					cancel = direction.Length() > 400;
				}

				if ( cancel || fabs( angles.y ) > 60 )
				{
					m_flWaitFinished -= 0.5;
					m_flameTime -= 0.5;
				}

				FlameControls( angles.x, angles.y );
			}
			else
				FlameCreate();
		}
		else
		{
			if ( FlameIsOn() )
				FlameDestroy();

			m_movementActivity = ACT_RUN;
			stopAttack = true;
		}
		break;

	default:
		activeAttack = false;
		break;
	}

	if ( activeAttack && !stopAttack && !m_fSequenceFinished && m_Activity != ACT_RESET )
		return;

	if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1_MOVING ) )
		m_IdealActivity = ACT_MELEE_ATTACK1;
	else if ( HasConditions( bits_COND_CAN_MELEE_ATTACK2_MOVING ) )
		m_IdealActivity = ACT_MELEE_ATTACK2;
	else if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1_MOVING ) )
		m_IdealActivity = ACT_RANGE_ATTACK1;
	else if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2_MOVING ) )
		m_IdealActivity = ACT_RANGE_ATTACK2;
	else if ( HasConditions( bits_COND_NO_AMMO_LOADED ) )
		m_IdealActivity = ACT_RELOAD;
	else
	{
		m_IdealActivity = m_movementActivity;
		pev->gaitsequence = GAIT_SEQUENCE_NONE;
	}
}

class CSmoker : public CBaseEntity
{
public:
	void Spawn( void );
	void Think( void );
};

LINK_ENTITY_TO_CLASS( env_smoker, CSmoker );

void CSmoker::Spawn( void )
{
	pev->movetype = MOVETYPE_NONE;
	pev->nextthink = gpGlobals->time;
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, g_vecZero, g_vecZero );
	pev->effects |= EF_NODRAW;
	pev->angles = g_vecZero;
}


void CSmoker::Think( void )
{
	// lots of smoke
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( pev->origin.x + RANDOM_FLOAT( -pev->dmg, pev->dmg ));
		WRITE_COORD( pev->origin.y + RANDOM_FLOAT( -pev->dmg, pev->dmg ));
		WRITE_COORD( pev->origin.z);
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( RANDOM_LONG(pev->scale, pev->scale * 1.1) );
		WRITE_BYTE( RANDOM_LONG(8,14)  ); // framerate
	MESSAGE_END();

	pev->health--;
	if ( pev->health > 0 )
		pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.1, 0.2);
	else
		UTIL_Remove( this );
}


void CSpiral::Spawn( void )
{
	pev->movetype = MOVETYPE_NONE;
	pev->nextthink = gpGlobals->time;
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, g_vecZero, g_vecZero );
	pev->effects |= EF_NODRAW;
	pev->angles = g_vecZero;
}


CSpiral *CSpiral::Create( const Vector &origin, float height, float radius, float duration )
{
	if ( duration <= 0 )
		return NULL;

	CSpiral *pSpiral = GetClassPtr( (CSpiral *)NULL );
	pSpiral->Spawn();
	pSpiral->pev->dmgtime = pSpiral->pev->nextthink;
	pSpiral->pev->origin = origin;
	pSpiral->pev->scale = radius;
	pSpiral->pev->dmg = height;
	pSpiral->pev->speed = duration;
	pSpiral->pev->health = 0;
	pSpiral->pev->angles = g_vecZero;

	return pSpiral;
}

#define SPIRAL_INTERVAL		0.1 //025

void CSpiral::Think( void )
{
	float time = gpGlobals->time - pev->dmgtime;

	while ( time > SPIRAL_INTERVAL )
	{
		Vector position = pev->origin;
		Vector direction = Vector(0,0,1);
		
		float fraction = 1.0 / pev->speed;

		float radius = (pev->scale * pev->health) * fraction;

		position.z += (pev->health * pev->dmg) * fraction;
		pev->angles.y = (pev->health * 360 * 8) * fraction;
		UTIL_MakeVectors( pev->angles );
		position = position + gpGlobals->v_forward * radius;
		direction = (direction + gpGlobals->v_forward).Normalize();

		StreakSplash( position, Vector(0,0,1), RANDOM_LONG(8,11), 20, RANDOM_LONG(50,150), 400 );

		// Jeez, how many counters should this take ? :)
		pev->dmgtime += SPIRAL_INTERVAL;
		pev->health += SPIRAL_INTERVAL;
		time -= SPIRAL_INTERVAL;
	}

	pev->nextthink = gpGlobals->time;

	if ( pev->health >= pev->speed )
		UTIL_Remove( this );
}


// HACKHACK Cut and pasted from explode.cpp
void SpawnExplosion( entvars_t *pevOwner, Vector center, float randomRange, float time, int magnitude )
{
	KeyValueData	kvd;
	char			buf[128];

	center.x += RANDOM_FLOAT( -randomRange, randomRange );
	center.y += RANDOM_FLOAT( -randomRange, randomRange );

	CBaseEntity *pExplosion = CBaseEntity::Create( "env_explosion", center, g_vecZero, false, pevOwner ? ENT( pevOwner ) : NULL );
	sprintf( buf, "%3d", magnitude );
	kvd.szKeyName = "iMagnitude";
	kvd.szValue = buf;
	pExplosion->KeyValue( &kvd );
	pExplosion->Spawn();
	pExplosion->SetThink( &CBaseEntity::SUB_CallUseToggle );
	pExplosion->pev->nextthink = gpGlobals->time + time;
}



#endif
