#ifndef OEM_BUILD

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "squadmonster.h"
#include "weapons.h"
#include "effects.h"
#include "customentity.h"
#include "soundent.h"
#include "decals.h"
#include "gargantua.h"
#include "babygarg.h"

extern cvar_t sk_babygargantua_health;
extern cvar_t sk_babygargantua_dmg_slash;
extern cvar_t sk_babygargantua_dmg_fire;

#define BABYGARG_DAMAGE ( DMG_ENERGYBEAM | DMG_CRUSH | DMG_MORTAR | DMG_BLAST | DMG_BURN )
#define BABYGARG_FLAME_LENGTH 180

LINK_ENTITY_TO_CLASS( monster_babygarg, CBabyGarg );

TYPEDESCRIPTION CBabyGarg::m_SaveData[] =
{
	DEFINE_FIELD( CBabyGarg, m_hEyeGlow, FIELD_EHANDLE ),
	DEFINE_FIELD( CBabyGarg, m_eyeBrightness, FIELD_INTEGER ),
	DEFINE_FIELD( CBabyGarg, m_seeTime, FIELD_TIME ),
	DEFINE_FIELD( CBabyGarg, m_flameTime, FIELD_TIME ),
	DEFINE_FIELD( CBabyGarg, m_streakTime, FIELD_TIME ),
	DEFINE_FIELD( CBabyGarg, m_painSoundTime, FIELD_TIME ),
	DEFINE_ARRAY( CBabyGarg, m_hFlame, FIELD_EHANDLE, 4 ),
	DEFINE_FIELD( CBabyGarg, m_flameX, FIELD_FLOAT ),
	DEFINE_FIELD( CBabyGarg, m_flameY, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CBabyGarg, CSquadMonster );

const char *CBabyGarg::pDieSounds[] =
{
	"babygarg/gar_die1.wav",
	"babygarg/gar_die2.wav",
};

const char *CBabyGarg::pAttackHitSounds[] =
{
	"zombie/claw_strike1.wav",
	"zombie/claw_strike2.wav",
	"zombie/claw_strike3.wav",
};

const char *CBabyGarg::pBeamAttackSounds[] =
{
	"babygarg/gar_flameoff1.wav",
	"babygarg/gar_flameon1.wav",
	"babygarg/gar_flamerun1.wav",
};

const char *CBabyGarg::pAttackMissSounds[] =
{
	"zombie/claw_miss1.wav",
	"zombie/claw_miss2.wav",
};

const char *CBabyGarg::pFootSounds[] =
{
	"babygarg/gar_step1.wav",
	"babygarg/gar_step2.wav",
};

const char *CBabyGarg::pIdleSounds[] =
{
	"babygarg/gar_idle1.wav",
	"babygarg/gar_idle2.wav",
	"babygarg/gar_idle3.wav",
	"babygarg/gar_idle4.wav",
	"babygarg/gar_idle5.wav",
};

const char *CBabyGarg::pAlertSounds[] =
{
	"babygarg/gar_alert1.wav",
	"babygarg/gar_alert2.wav",
	"babygarg/gar_alert3.wav",
};

const char *CBabyGarg::pPainSounds[] =
{
	"babygarg/gar_pain1.wav",
	"babygarg/gar_pain2.wav",
	"babygarg/gar_pain3.wav",
};

const char *CBabyGarg::pAttackSounds[] =
{
	"babygarg/gar_attack1.wav",
	"babygarg/gar_attack2.wav",
	"babygarg/gar_attack3.wav",
};

const char *CBabyGarg::pStompSounds[] =
{
	"babygarg/gar_stomp1.wav",
};

const char *CBabyGarg::pBreatheSounds[] =
{
	"babygarg/gar_breathe1.wav",
	"babygarg/gar_breathe2.wav",
	"babygarg/gar_breathe3.wav",
};

enum
{
	SCHED_BABYGARG_HIDE = LAST_COMMON_SCHEDULE + 1,
};

enum
{
	TASK_BABYGARG_SOUND_ATTACK = LAST_COMMON_TASK + 1,
	TASK_BABYGARG_FLAME_SWEEP,
	TASK_BABYGARG_FIND_PARENT,
};

Task_t tlBabyGargHide[] =
{
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_MELEE_ATTACK1 },
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_BABYGARG_FIND_PARENT, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
};

Schedule_t slBabyGargHide[] =
{
	{ tlBabyGargHide, ARRAYSIZE( tlBabyGargHide ),
	  bits_COND_HEAR_SOUND | bits_COND_SPECIAL1, bits_SOUND_DANGER, "GargHide" },
};

Task_t tlBabyGargSwipe[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_FACE_ENEMY, 0.0f },
	{ TASK_MELEE_ATTACK1, 0.0f },
	{ TASK_FIND_COVER_FROM_ENEMY, 0.0f },
	{ TASK_RUN_PATH, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_FACE_ENEMY, 0.0f },
	{ TASK_BABYGARG_FIND_PARENT, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_FACE_ENEMY, 0.0f },
};

Schedule_t slBabyGargSwipe[] =
{
	{ tlBabyGargSwipe, ARRAYSIZE( tlBabyGargSwipe ),
	  bits_COND_HEAR_SOUND | bits_COND_CAN_MELEE_ATTACK2 | bits_COND_HEAVY_DAMAGE | bits_COND_ENEMY_TOOFAR, bits_SOUND_DANGER, "GargSwipe" },
};

Task_t tlBabyGargFlame[] =
{
	{ TASK_STOP_MOVING, 0.0f },
	{ TASK_FACE_ENEMY, 0.0f },
	{ TASK_BABYGARG_SOUND_ATTACK, 0.0f },
	{ TASK_SET_ACTIVITY, (float) ACT_MELEE_ATTACK2 },
	{ TASK_BABYGARG_FLAME_SWEEP, 2.0f },
	{ TASK_BABYGARG_FIND_PARENT, 0.0f },
	{ TASK_WAIT_FOR_MOVEMENT, 0.0f },
	{ TASK_FACE_ENEMY, 0.0f },
};

Schedule_t slBabyGargFlame[] =
{
	{ tlBabyGargFlame, ARRAYSIZE( tlBabyGargFlame ),
	  bits_COND_HEAR_SOUND, bits_SOUND_DANGER, "GargFlame" },
};

DEFINE_CUSTOM_SCHEDULES( CBabyGarg )
{
	slBabyGargHide,
	slBabyGargSwipe,
	slBabyGargFlame,
};

IMPLEMENT_CUSTOM_SCHEDULES( CBabyGarg, CBaseMonster );

void CBabyGarg::Spawn( void )
{
	Precache();

	if ( !SetupModel() )
		UTIL_SetModel( this, "models/babygarg.mdl" );

	UTIL_SetSize( pev, Vector( -32, -32, 0 ), Vector( 32, 32, 96 ) );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_GREEN;

	if ( pev->health == 0.0f )
		pev->health = sk_babygargantua_health.value;

	m_flFieldOfView = -0.2f;
	m_MonsterState = MONSTERSTATE_NONE;
	m_afMemory = 0;

	if ( FStringNull( m_FormattedName ) )
		m_FormattedName = MAKE_STRING( "Baby Gargantua" );

	MonsterInit();
	EyeOff();
	m_seeTime = gpGlobals->time + 5;
	m_flameTime = gpGlobals->time + 2;

	if ( IsPlayerAlly() && !( pev->spawnflags & SF_MONSTER_PREDISASTER ) )
	{
		SetUse( &CBabyGarg::CallFollowerPlayerUse );
		m_fCanPlayerFollow = true;
	}
}

int CBabyGarg::Classify( void )
{
	return GetClassification( CLASS_ALIEN_MONSTER );
}

int CBabyGarg::IRelationship( CBaseEntity *pTarget, bool fIgnoreNoTarget )
{
	if ( !pTarget )
		return R_NO;

	CBaseMonster *monster = pTarget->MyMonsterPointer();

	if ( ( pTarget->pev->flags & FL_MONSTER ) && monster && monster->m_fOverrideClass )
		return CBaseEntity::IRelationship( pTarget, fIgnoreNoTarget );

	if ( IsPlayerAlly() || m_fOverrideClass )
		return CBaseEntity::IRelationship( pTarget, fIgnoreNoTarget );

	if ( FClassnameIs( pTarget->pev, "monster_alien_babyvoltigore" ) || FClassnameIs( pTarget->pev, "monster_shocktrooper" ) || FClassnameIs( pTarget->pev, "monster_alien_voltigore" ) )
		return R_HT;

	return CBaseEntity::IRelationship( pTarget, fIgnoreNoTarget );
}

int CBabyGarg::ObjectCaps( void )
{
	return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

float CBabyGarg::GetPointsForDamage( float flDamage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	return ( pev->max_health / sk_babygargantua_health.value ) * 15.0f * ( flDamage / pev->max_health );
}

void CBabyGarg::SetYawSpeed( void )
{
	switch ( m_Activity )
	{
		case ACT_IDLE:
		case ACT_WALK:
		case ACT_RUN:
			pev->yaw_speed = 200;
			break;
		default:
			pev->yaw_speed = 270;
			break;
	}
}

BOOL CBabyGarg::CheckMeleeAttack1( float flDot, float flDist )
{
	return flDot >= 0.7 && flDist <= 65.0;
}

BOOL CBabyGarg::CheckMeleeAttack2( float flDot, float flDist )
{
	return gpGlobals->time > m_flameTime && flDot >= 0.8 && flDist > 65.0 && flDist <= 180.0;
}

BOOL CBabyGarg::CheckRangeAttack1( float flDot, float flDist )
{
	return gpGlobals->time > m_seeTime && flDot >= 0.7 && flDist > 65.0;
}

void CBabyGarg::SetObjectCollisionBox( void )
{
	Vector mins( -80, -80, 0 ), maxs( 80, 80, 214 );

	if ( !( pev->spawnflags & SF_MONSTER_NO_MODEL_BOX ) && m_fCustomModel )
	{
		mins = g_vecZero;
		maxs = g_vecZero;
		ExtractBbox( 1, mins, maxs );
		mins.z = 0;
	}

	pev->absmin = pev->origin + mins;
	pev->absmax = pev->origin + maxs;
}

int CBabyGarg::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	bool hostileGarg = false;

	if ( pevAttacker && ( FClassnameIs( pevAttacker, "monster_gargantua" ) || FClassnameIs( pevAttacker, "monster_babygarg" ) ) )
	{
		CBaseEntity *attacker = Instance( pevAttacker );

		if ( attacker )
			hostileGarg = IRelationship( attacker ) > R_NO;
	}

	if ( IsAlive() )
	{
		if ( !( bitsDamageType & BABYGARG_DAMAGE ) && !hostileGarg )
			flDamage *= 0.5;

		if ( bitsDamageType & DMG_BLAST )
			SetConditions( bits_COND_LIGHT_DAMAGE );

		if ( flDamage > 35.0 )
			SetConditions( bits_COND_HEAVY_DAMAGE );
	}

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CBabyGarg::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if ( IsAlive() )
	{
		if ( !( bitsDamageType & BABYGARG_DAMAGE ) )
		{
			if ( pev->dmgtime != gpGlobals->time || RANDOM_LONG( 0, 100 ) < 20 )
				pev->dmgtime = gpGlobals->time;
		}
		else if ( gpGlobals->time > m_painSoundTime )
		{
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, pPainSounds[RANDOM_LONG( 0, ARRAYSIZE( pPainSounds ) - 1 )], 1.0, ATTN_NORM, 0, PITCH_NORM );
			m_painSoundTime = gpGlobals->time + RANDOM_FLOAT( 2.5, 4.0 );
		}
	}

	CBaseMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

CSprite *CBabyGarg::CreateEye( void )
{
	CSprite *eye = CSprite::SpriteCreate( "sprites/gargeye1.spr", pev->origin, FALSE );

	if ( !eye )
		return NULL;

	if ( IsPlayerAlly() )
		eye->SetTransparency( kRenderGlow, 0, 255, 0, 0, kRenderFxNoDissipation );
	else
		eye->SetTransparency( kRenderGlow, 255, 255, 255, 0, kRenderFxNoDissipation );

	eye->SetAttachment( edict(), 1 );
	eye->pev->scale = 0.5f;
	return eye;
}

void CBabyGarg::EyeOn( int level )
{
	if ( m_hEyeGlow == NULL )
	{
		m_hEyeGlow = CreateEye();

		if ( m_hEyeGlow != NULL )
			m_hEyeGlow->pev->renderamt = m_flEyeBrightness;
	}

	m_eyeBrightness = level;
}

void CBabyGarg::EyeOff( void )
{
	m_eyeBrightness = 0;

	if ( m_hEyeGlow != NULL )
	{
		m_hEyeGlow->pev->aiment = NULL;
		UTIL_Remove( m_hEyeGlow );
	}

	m_hEyeGlow = NULL;
}

void CBabyGarg::EyeUpdate( void )
{
	if ( m_hEyeGlow == NULL )
		return;

	m_hEyeGlow->pev->renderamt = UTIL_Approach( m_eyeBrightness, m_hEyeGlow->pev->renderamt, 26 );
	m_flEyeBrightness = m_hEyeGlow->pev->renderamt;

	if ( m_hEyeGlow->pev->renderamt == 0 )
		m_hEyeGlow->pev->effects |= EF_NODRAW;
	else
		m_hEyeGlow->pev->effects &= ~EF_NODRAW;

	UTIL_SetOrigin( m_hEyeGlow->pev, pev->origin );
}

void CBabyGarg::PrescheduleThink( void )
{
	if ( HasConditions( bits_COND_SEE_ENEMY ) )
		EyeOn( 200 );
	else
	{
		m_seeTime = gpGlobals->time + 5;
		EyeOff();
	}

	CBaseEntity *ground = Instance( pev->groundentity );

	if ( ground && ( ground->pev->flags & ( FL_CLIENT | FL_MONSTER ) ) )
	{
		ground->TakeDamage( pev, pev, 1000, DMG_CRUSH );
		pev->groundentity = NULL;
		pev->flags &= ~FL_ONGROUND;
		pev->velocity.z -= 1;
	}

	EyeUpdate();
}

void CBabyGarg::OnDestroy( void )
{
	EyeOff();
	RemoveFlames();
	CBaseMonster::OnDestroy();
}

void CBabyGarg::UpdateOnRemove( void )
{
	CBaseEntity::UpdateOnRemove();
	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, pBeamAttackSounds[0], 1.0, ATTN_NORM, SND_STOP, PITCH_NORM );
	EyeOff();
	RemoveFlames();
}

void CBabyGarg::Killed( entvars_t *pevAttacker, int iGib )
{
	EyeOff();
	RemoveFlames();
	CSquadMonster::Killed( pevAttacker, GIB_NEVER );
}

void CBabyGarg::FlameControls( float angleX, float angleY )
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

CBaseEntity *CBabyGarg::FindRunAwayTarget( void )
{
	CBaseEntity *candidate = NULL;
	CBaseEntity *nearest = NULL;

	while ( ( candidate = UTIL_FindEntityByClassname( candidate, "monster_gargantua" ) ) != NULL )
	{
		if ( IRelationship( candidate ) > R_NO || !candidate->IsAlive() || !FVisible( candidate, true ) )
			continue;

		if ( !nearest || !FVisible( nearest, true ) || ( nearest->pev->origin - pev->origin ).Length() > ( candidate->pev->origin - pev->origin ).Length() )
			nearest = candidate;
	}

	return nearest;
}

Schedule_t *CBabyGarg::GetScheduleOfType( int Type )
{
	if ( m_hFlame[0] != NULL )
		FlameDestroy();

	switch ( Type )
	{
		case SCHED_MELEE_ATTACK2:
			return slBabyGargFlame;
		case SCHED_BABYGARG_HIDE:
			return slBabyGargHide;
		case SCHED_MELEE_ATTACK1:
			return slBabyGargSwipe;
		default:
			return CBaseMonster::GetScheduleOfType( Type );
	}
}

void CBabyGarg::StartTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_BABYGARG_FLAME_SWEEP:
			FlameCreate();
			m_flWaitFinished = gpGlobals->time + pTask->flData;
			m_flameTime = gpGlobals->time + 2;
			m_flameX = 0;
			m_flameY = 0;
			return;

		case TASK_BABYGARG_FIND_PARENT:
		{
			CBaseEntity *parent = FindRunAwayTarget();

			if ( !parent || !MoveToLocation( ACT_RUN, 0, parent->Center(), 128 ) )
			{
				CBaseEntity *enemy = m_hEnemy;
				entvars_t *threat = enemy ? enemy->pev : pev;

				if ( !FindCover( threat->origin, threat->view_ofs, 128, 784 ) )
					TaskFail();
			}
			break;
		}

		case TASK_BABYGARG_SOUND_ATTACK:
			if ( RANDOM_LONG( 0, 100 ) < 30 )
				EMIT_SOUND_DYN( edict(), CHAN_VOICE, pAttackSounds[RANDOM_LONG( 0, ARRAYSIZE( pAttackSounds ) - 1 )], 1.0, ATTN_NORM, 0, PITCH_NORM );
			break;

		default:
			CBaseMonster::StartTask( pTask );
			return;
	}

	if ( !HasConditions( bits_COND_TASK_FAILED ) )
		TaskComplete();
}

void CBabyGarg::RunTask( Task_t *pTask )
{
	if ( pTask->iTask != TASK_BABYGARG_FLAME_SWEEP )
	{
		CBaseMonster::RunTask( pTask );
		return;
	}

	if ( gpGlobals->time > m_flWaitFinished )
	{
		FlameDestroy();

		if ( !HasConditions( bits_COND_TASK_FAILED ) )
			TaskComplete();

		FlameControls( 0, 0 );
		SetBoneController( 0, 0 );
		SetBoneController( 1, 0 );
		return;
	}

	float angleX = 0;
	float angleY = 0;

	FlameUpdate();

	CBaseEntity *enemy = m_hEnemy;

	if ( enemy )
	{
		Vector midpoint = pev->origin + Vector( 0, 0, 48 );
		Vector aim = enemy->BodyTarget( midpoint ) - midpoint;
		Vector angles = UTIL_VecToAngles( aim );

		angleX = -angles.x;
		angleY = angles.y - pev->angles.y;

		if ( aim.Length() > 400 || fabs( angleY ) > 60 )
		{
			m_flWaitFinished = gpGlobals->time;
			m_flameTime = gpGlobals->time + 0.1;
		}
	}

	FlameControls( angleX, angleY );
}

void CBabyGarg::FlameCreate( void )
{
	UTIL_MakeVectors( pev->angles );
	RemoveFlames();

	for ( int i = 0; i < 4; i++ )
	{
		CBeam *beam = CBeam::BeamCreate( "sprites/xbeam3.spr", i < 2 ? 120 : 70 );

		m_hFlame[i] = beam;

		if ( !beam )
			continue;

		Vector origin, angles;
		TraceResult trace;
		int attachment = i % 2;

		GetAttachment( attachment + 1, origin, angles );
		UTIL_TraceLine( origin, origin + gpGlobals->v_forward * BABYGARG_FLAME_LENGTH, dont_ignore_monsters, edict(), &trace );
		beam->PointEntInit( trace.vecEndPos, entindex() );

		if ( i < 2 )
			beam->SetColor( 255, 130, 90 );
		else
			beam->SetColor( 0, 100, 255 );

		beam->SetBrightness( 190 );
		beam->SetFlags( BEAM_FSHADEIN );
		beam->SetScrollRate( 20 );
		beam->SetEndAttachment( attachment + 2 );
		CSoundEnt::InsertSound( bits_SOUND_COMBAT, origin, 384, 0.3, this );
	}

	EMIT_SOUND_DYN( edict(), CHAN_BODY, pBeamAttackSounds[1], 1.0, ATTN_NORM, 0, PITCH_NORM );
	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, pBeamAttackSounds[2], 1.0, ATTN_NORM, 0, PITCH_NORM );
}

void CBabyGarg::RemoveFlames( void )
{
	for ( int i = 0; i < ARRAYSIZE( m_hFlame ); i++ )
	{
		if ( m_hFlame[i] == NULL )
			continue;

		m_hFlame[i]->pev->aiment = NULL;
		UTIL_Remove( m_hFlame[i] );
		m_hFlame[i] = NULL;
	}
}

void CBabyGarg::FlameDestroy( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, pBeamAttackSounds[0], 1.0, ATTN_NORM, 0, PITCH_NORM );
	RemoveFlames();
}

void CBabyGarg::FlameUpdate( void )
{
	bool streaks = false;

	for ( int i = 0; i < 2; i++ )
	{
		CBeam *beam = (CBeam *) (CBaseEntity *) m_hFlame[i];

		if ( !beam )
			continue;

		Vector origin, angles;
		TraceResult trace;

		UTIL_MakeVectors( pev->angles + Vector( m_flameX, m_flameY, 0 ) );
		GetAttachment( i + 1, origin, angles );
		UTIL_TraceLine( origin, origin + gpGlobals->v_forward * BABYGARG_FLAME_LENGTH, dont_ignore_monsters, edict(), &trace );
		beam->SetStartPos( trace.vecEndPos );

		CBeam *secondary = (CBeam *) (CBaseEntity *) m_hFlame[i + 2];

		if ( secondary )
			secondary->SetStartPos( origin * 0.6f + trace.vecEndPos * 0.4f );

		if ( trace.flFraction != 1.0 && gpGlobals->time > m_streakTime )
		{
			StreakSplash( trace.vecEndPos, trace.vecPlaneNormal, 6, 20, 50, 400 );
			UTIL_DecalTrace( &trace, DECAL_SMALLSCORCH1 + RANDOM_LONG( 0, 2 ) );
			streaks = true;
		}

		FlameDamage( origin, trace.vecEndPos, pev, pev, sk_babygargantua_dmg_fire.value, CLASS_ALIEN_MONSTER, DMG_BURN );

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_ELIGHT );
		WRITE_SHORT( entindex() + 0x2000 + i * 0x1000 );
		WRITE_COORD( origin.x );
		WRITE_COORD( origin.y );
		WRITE_COORD( origin.z );
		WRITE_COORD( RANDOM_FLOAT( 32, 48 ) );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 2 );
		WRITE_COORD( 0 );
		MESSAGE_END();
	}

	if ( streaks )
		m_streakTime = gpGlobals->time;
}

void CBabyGarg::StompAttack( void )
{
	TraceResult trace;

	UTIL_MakeVectors( pev->angles );

	Vector start = pev->origin + Vector( 0, 0, 60 ) + gpGlobals->v_forward * 35;
	Vector end = start + ShootAtEnemy( start ) * 1024;

	UTIL_TraceLine( start, end, ignore_monsters, edict(), &trace );
	CStomp::StompCreate( start, trace.vecEndPos, 750, pev );
	UTIL_ScreenShake( pev->origin, 12, 100, 2, 1000 );
	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, pStompSounds[RANDOM_LONG( 0, ARRAYSIZE( pStompSounds ) - 1 )], 1.0, ATTN_NORM, 0, PITCH_NORM + RANDOM_LONG( -10, 10 ) );
	UTIL_TraceLine( pev->origin, pev->origin - Vector( 0, 0, 20 ), ignore_monsters, edict(), &trace );

	if ( trace.flFraction < 1.0 )
		UTIL_DecalTrace( &trace, DECAL_GARGSTOMP1 );
}

void CBabyGarg::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch ( pEvent->event )
	{
		case 1:
		case 7:
		{
			CBaseEntity *hit = CheckTraceHullAttack( 75, sk_babygargantua_dmg_slash.value, DMG_SLASH );

			if ( hit && ( ( hit->pev->flags & FL_CLIENT ) || ( ( hit->pev->flags & FL_MONSTER ) && hit->pev->movetype != MOVETYPE_FLY ) ) )
			{
				if ( pEvent->event == 1 )
				{
					hit->pev->punchangle = Vector( -30, -30, 30 );
					hit->pev->velocity = hit->pev->velocity - gpGlobals->v_right * 400;
					hit->pev->velocity = hit->pev->velocity + gpGlobals->v_up * 400;
					hit->pev->velocity = hit->pev->velocity + gpGlobals->v_forward * 400;
					hit->pev->flags &= ~FL_ONGROUND;
				}
				else
				{
					hit->pev->velocity = hit->pev->velocity + gpGlobals->v_forward * 800;
					hit->pev->velocity = hit->pev->velocity + gpGlobals->v_up * 800;
				}
			}

			if ( pEvent->event == 1 )
			{
				int pitch = 50 + RANDOM_LONG( 0, 15 );
				const char *sound = hit ? pAttackHitSounds[RANDOM_LONG( 0, ARRAYSIZE( pAttackHitSounds ) - 1 )]
				                        : pAttackMissSounds[RANDOM_LONG( 0, ARRAYSIZE( pAttackMissSounds ) - 1 )];

				EMIT_SOUND_DYN( edict(), CHAN_WEAPON, sound, 1.0, ATTN_NORM, 0, pitch );
			}
			else
			{
				int pitch = 90 + RANDOM_LONG( 0, 15 );

				if ( hit )
					EMIT_SOUND_DYN( edict(), CHAN_WEAPON, pFootSounds[0], 1.0, 0.5, 0, pitch );
				else
					EMIT_SOUND_DYN( edict(), CHAN_WEAPON, pAttackMissSounds[RANDOM_LONG( 0, ARRAYSIZE( pAttackMissSounds ) - 1 )], 1.0, ATTN_NORM, 0, pitch );
			}
			break;
		}

		case 3:
		case 4:
		{
			int pitch = PITCH_NORM + RANDOM_LONG( -10, 10 );

			EMIT_SOUND_DYN( edict(), CHAN_BODY, pFootSounds[RANDOM_LONG( 0, ARRAYSIZE( pFootSounds ) - 1 )], 1.0, ATTN_NORM, 0, pitch );
			break;
		}

		case 5:
			StompAttack();
			m_seeTime = gpGlobals->time + 12;
			break;

		case 6:
		{
			int pitch = PITCH_NORM + RANDOM_LONG( -10, 10 );

			EMIT_SOUND_DYN( edict(), CHAN_VOICE, pBreatheSounds[RANDOM_LONG( 0, ARRAYSIZE( pBreatheSounds ) - 1 )], 1.0, ATTN_NORM, 0, pitch );
			break;
		}

		default:
			CBaseMonster::HandleAnimEvent( pEvent );
			break;
	}
}

void CBabyGarg::Precache( void )
{
	CBaseEntity::Precache();

	if ( pev->modelindex == 0 )
		UTIL_PrecacheModel( this, "models/babygarg.mdl" );

	UTIL_PrecacheModel( this, "sprites/gargeye1.spr" );
	UTIL_PrecacheModel( this, "sprites/xbeam3.spr" );
	UTIL_PrecacheModel( this, "sprites/xbeam3.spr" );
	UTIL_PrecacheModel( this, "sprites/gargeye1.spr" );
	UTIL_PrecacheModel( this, "models/metalplategibs.mdl" );
	PrecacheSound_Intercept( this, "weapons/mine_charge.wav" );

	for ( int i = 0; i < ARRAYSIZE( pDieSounds ); i++ )
		PrecacheSound_Intercept( this, pDieSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pAttackHitSounds ); i++ )
		PrecacheSound_Intercept( this, pAttackHitSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pBeamAttackSounds ); i++ )
		PrecacheSound_Intercept( this, pBeamAttackSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pAttackMissSounds ); i++ )
		PrecacheSound_Intercept( this, pAttackMissSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pFootSounds ); i++ )
		PrecacheSound_Intercept( this, pFootSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pIdleSounds ); i++ )
		PrecacheSound_Intercept( this, pIdleSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pAlertSounds ); i++ )
		PrecacheSound_Intercept( this, pAlertSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pPainSounds ); i++ )
		PrecacheSound_Intercept( this, pPainSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pAttackSounds ); i++ )
		PrecacheSound_Intercept( this, pAttackSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pStompSounds ); i++ )
		PrecacheSound_Intercept( this, pStompSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pBreatheSounds ); i++ )
		PrecacheSound_Intercept( this, pBreatheSounds[i] );
}

void CBabyGarg::FlameDamage( Vector vecStart, Vector vecEnd, entvars_t *pevInflictor, entvars_t *pevAttacker,
                             float flDamage, int iClassIgnore, int bitsDamageType )
{
	CBaseEntity *entity = NULL;
	Vector midpoint = ( vecStart + vecEnd ) * 0.5;
	Vector aim = ( vecEnd - vecStart ).Normalize();
	float searchRadius = ( vecStart - midpoint ).Length();

	while ( ( entity = UTIL_FindEntityInSphere( entity, midpoint, searchRadius ) ) != NULL )
	{
		if ( IRelationship( entity ) <= R_NO || entity->pev->takedamage == DAMAGE_NO )
			continue;

		Vector target = entity->BodyTarget( midpoint );
		float distance = DotProduct( aim, target - midpoint );

		if ( distance > searchRadius || distance < -searchRadius )
			distance = searchRadius;

		Vector origin = midpoint + aim * distance;
		TraceResult trace;

		UTIL_TraceLine( origin, target, dont_ignore_monsters, edict(), &trace );

		if ( trace.flFraction != 1.0 && trace.pHit != entity->edict() )
			continue;

		distance = ( origin - trace.vecEndPos ).Length();

		float adjustedDamage = flDamage;

		if ( distance > 64 )
		{
			adjustedDamage = flDamage - ( distance - 64 ) * 0.4;

			if ( adjustedDamage <= 0 )
				continue;
		}

		if ( trace.flFraction != 1.0 )
		{
			ClearMultiDamage();
			entity->TraceAttack( pevInflictor, adjustedDamage, ( trace.vecEndPos - origin ).Normalize(), &trace, bitsDamageType );
			ApplyMultiDamage( pevInflictor, pevAttacker );
		}
		else
			entity->TakeDamage( pevInflictor, pevAttacker, adjustedDamage, bitsDamageType );
	}
}

#endif
