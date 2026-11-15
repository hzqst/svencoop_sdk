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
// hgrunt
//=========================================================

//=========================================================
// Hit groups!	
//=========================================================
/*

  1 - Head
  2 - Stomach
  3 - Gun

*/


#include	"extdll.h"
#include	"plane.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"animation.h"
#include	"squadmonster.h"
#include	"weapons.h"
#include	"talkmonster.h"
#include "CClassicMode.h"
#include	"soundent.h"
#include	"effects.h"
#include	"customentity.h"
#include "hgrunt_repel.h"

int g_fGruntQuestion;				// true if an idle grunt asked a question. Cleared when someone answers.

extern DLL_GLOBAL int		g_iSkillLevel;
extern cvar_t sk_hgrunt_health;
extern cvar_t sk_plr_rpg;
extern cvar_t sk_hgrunt_pellets;
extern cvar_t sk_hgrunt_kick;
extern cvar_t sk_grunt_handgrenades;

//=========================================================
// monster-specific DEFINE's
//=========================================================
#define	GRUNT_CLIP_SIZE					30 // how many bullets in a clip? - NOTE: 3 round burst sound, so keep as 3 * x!
#define GRUNT_VOL						0.35		// volume of grunt sounds
#define GRUNT_ATTN						ATTN_NORM	// attenutation of grunt sentences
#define HGRUNT_LIMP_HEALTH				20
#define HGRUNT_DMG_HEADSHOT				( DMG_BULLET | DMG_CLUB )	// damage types that can kill a grunt with a single headshot.
#define HGRUNT_NUM_HEADS				2 // how many grunt heads are there? 
#define HGRUNT_MINIMUM_HEADSHOT_DAMAGE	15 // must do at least this much damage in one shot to head to score a headshot kill
#define	HGRUNT_SENTENCE_VOLUME			(float)0.35 // volume of grunt sentences

#define HGRUNT_9MMAR				( 1 << 0)
#define HGRUNT_HANDGRENADE			( 1 << 1)
#define HGRUNT_GRENADELAUNCHER		( 1 << 2)
#define HGRUNT_SHOTGUN				( 1 << 3)
#define HGRUNT_RPG					( 1 << 6 )
#define HGRUNT_SNIPER				( 1 << 7 )
#define bits_MOVESHOOTCAP_RPG		( 1 << 4 )

#define HEAD_GROUP					1
#define HEAD_GRUNT					0
#define HEAD_COMMANDER				1
#define HEAD_SHOTGUN				2
#define HEAD_M203					3
#define GUN_GROUP					2
#define GUN_MP5						0
#define GUN_SHOTGUN					1
#define GUN_NONE					2

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		HGRUNT_AE_RELOAD		( 2 )
#define		HGRUNT_AE_KICK			( 3 )
#define		HGRUNT_AE_BURST1		( 4 )
#define		HGRUNT_AE_BURST2		( 5 ) 
#define		HGRUNT_AE_BURST3		( 6 ) 
#define		HGRUNT_AE_GREN_TOSS		( 7 )
#define		HGRUNT_AE_GREN_LAUNCH	( 8 )
#define		HGRUNT_AE_GREN_DROP		( 9 )
#define		HGRUNT_AE_CAUGHT_ENEMY	( 10) // grunt established sight with an enemy (player only) that had previously eluded the squad.
#define		HGRUNT_AE_DROP_GUN		( 11) // grunt (probably dead) is dropping his mp5.

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_GRUNT_SUPPRESS = LAST_COMMON_SCHEDULE + 1,
	SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE,// move to a location to set up an attack against the enemy. (usually when a friendly is in the way).
	SCHED_GRUNT_COVER_AND_RELOAD,
	SCHED_GRUNT_SWEEP,
	SCHED_GRUNT_FOUND_ENEMY,
	SCHED_GRUNT_REPEL,
	SCHED_GRUNT_REPEL_ATTACK,
	SCHED_GRUNT_REPEL_LAND,
	SCHED_GRUNT_WAIT_FACE_ENEMY,
	SCHED_GRUNT_TAKECOVER_FAILED,// special schedule type that forces analysis of conditions and picks the best possible schedule to recover from this type of failure.
	SCHED_GRUNT_ELOF_FAIL,
};

//=========================================================
// monster-specific tasks
//=========================================================
enum 
{
	TASK_GRUNT_FACE_TOSS_DIR = LAST_COMMON_TASK + 1,
	TASK_GRUNT_SPEAK_SENTENCE,
	TASK_GRUNT_CHECK_FIRE,
	TASK_GRUNT_WAIT_ROCKET,
	TASK_GRUNT_WAIT_SNIPER,
};

//=========================================================
// monster-specific conditions
//=========================================================
#define bits_COND_GRUNT_NOFIRE	( bits_COND_SPECIAL1 )

class CHGrunt : public CSquadMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetupFriendly( void );
	void SetYawSpeed ( void );
	int  Classify ( void );
	int ObjectCaps( void );

	virtual float GetPointsForDamage( float flDamage )
	{
		//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
		return V_min( 4, ( flDamage / pev->max_health ) * ( 4 * ( pev->max_health / sk_hgrunt_health.value ) ) );
	}

	int ISoundMask ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	BOOL FCanCheckAttacks ( void );
	BOOL CheckMeleeAttack1 ( float flDot, float flDist );
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	BOOL CheckRangeAttack1Rocket( float distance, CBaseEntity *target );
	BOOL CheckRangeAttack1Sniper( float distance, CBaseEntity *target );
	BOOL CheckRangeAttack2 ( float flDot, float flDist );
	CBaseEntity *BestVisibleEnemy( void );
	void CheckAmmo ( void );
	void SetActivity ( Activity NewActivity );
	void SetGaitActivity( Activity NewActivity );
	void StartTask ( Task_t *pTask );
	void RunTask ( Task_t *pTask );
	void DeathSound( void );
	void PainSound( void );
	void IdleSound ( void );
	void UseSound( void );
	void UnUseSound( void );
	Vector GetGunPosition( void );
	void Shoot ( void );
	void Shotgun ( void );
	void RPGShoot( void );
	void KillLaserSpot( void );
	void UpdateSpot( bool instant );
	void SniperShoot( void );
	CBaseEntity *DropWeapon( void );
	void PrescheduleThink ( void );
	void GibMonster( void );
	void SpeakSentence( void );

	int	Save( CSave &save ); 
	int Restore( CRestore &restore );
	
	CBaseEntity	*Kick( void );
	Schedule_t	*GetSchedule( void );
	Schedule_t  *GetScheduleOfType ( int Type );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	int IRelationship ( CBaseEntity *pTarget, bool ignoreNoTarget = false );

	BOOL FOkToSpeak( void );
	void JustSpoke( void );

	CUSTOM_SCHEDULES;
	static TYPEDESCRIPTION m_SaveData[];

	// checking the feasibility of a grenade toss is kind of costly, so we do it every couple of seconds,
	// not every server frame.
	float m_flNextGrenadeCheck;
	float m_flNextPainTime;
	float m_flLastEnemySightTime;

	Vector	m_vecTossVelocity;

	bool	m_fThrowGrenade;
	bool	m_fStanding;
	bool	m_fFirstEncounter;// only put on the handsign show in the squad's first encounter.
	int		m_cClipSize;

	int m_voicePitch;

	int		m_iBrassShell;
	int		m_iShotgunShell;

	int		m_iSentence;

	float		m_flRPGSafetyRadius;
	bool		m_fHasM16;
	EHANDLE		m_hRPGEntity;
	EHANDLE		m_hRPGLaserSpot;

	Vector m_vecRPGCurrentAngles;
	Vector m_vecRPGAimAngles;

	static const char *pGruntSentences[];
};

LINK_ENTITY_TO_CLASS( monster_human_grunt, CHGrunt );

TYPEDESCRIPTION	CHGrunt::m_SaveData[] = 
{
	DEFINE_FIELD( CHGrunt, m_flNextGrenadeCheck, FIELD_TIME ),
	DEFINE_FIELD( CHGrunt, m_flNextPainTime, FIELD_TIME ),
//	DEFINE_FIELD( CHGrunt, m_flLastEnemySightTime, FIELD_TIME ), // don't save, go to zero
	DEFINE_FIELD( CHGrunt, m_vecTossVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( CHGrunt, m_fThrowGrenade, FIELD_CHARACTER ),
	DEFINE_FIELD( CHGrunt, m_fStanding, FIELD_CHARACTER ),
	DEFINE_FIELD( CHGrunt, m_fFirstEncounter, FIELD_CHARACTER ),
	DEFINE_FIELD( CHGrunt, m_cClipSize, FIELD_INTEGER ),
	DEFINE_FIELD( CHGrunt, m_voicePitch, FIELD_INTEGER ),
//  DEFINE_FIELD( CShotgun, m_iBrassShell, FIELD_INTEGER ),
//  DEFINE_FIELD( CShotgun, m_iShotgunShell, FIELD_INTEGER ),
	DEFINE_FIELD( CHGrunt, m_iSentence, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CHGrunt, CSquadMonster );

const char *CHGrunt::pGruntSentences[] = 
{
	"HG_GREN", // grenade scared grunt
	"HG_ALERT", // sees player
	"HG_MONSTER", // sees monster
	"HG_COVER", // running to cover
	"HG_THROW", // about to throw grenade
	"HG_CHARGE",  // running out to get the enemy
	"HG_TAUNT", // say rude things
};

enum HGRUNT_SENTENCE_TYPES
{
	HGRUNT_SENT_NONE = -1,
	HGRUNT_SENT_GREN = 0,
	HGRUNT_SENT_ALERT,
	HGRUNT_SENT_MONSTER,
	HGRUNT_SENT_COVER,
	HGRUNT_SENT_THROW,
	HGRUNT_SENT_CHARGE,
	HGRUNT_SENT_TAUNT,
};

//=========================================================
// Speak Sentence - say your cued up sentence.
//
// Some grunt sentences (take cover and charge) rely on actually
// being able to execute the intended action. It's really lame
// when a grunt says 'COVER ME' and then doesn't move. The problem
// is that the sentences were played when the decision to TRY
// to move to cover was made. Now the sentence is played after 
// we know for sure that there is a valid path. The schedule
// may still fail but in most cases, well after the grunt has 
// started moving.
//=========================================================
void CHGrunt :: SpeakSentence( void )
{
	if ( m_iSentence == HGRUNT_SENT_NONE )
	{
		// no sentence cued up.
		return; 
	}

	if (FOkToSpeak())
	{
		SENTENCEG_PlayRndSz( ENT(pev), pGruntSentences[ m_iSentence ], HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
		JustSpoke();
	}
}

//=========================================================
// IRelationship - overridden because Alien Grunts are 
// Human Grunt's nemesis.
//=========================================================
int CHGrunt::IRelationship ( CBaseEntity *pTarget, bool ignoreNoTarget )
{
	if ( pTarget && !pTarget->m_fOverrideClass && Classify() == CLASS_HUMAN_MILITARY
		 && ( FClassnameIs( pTarget->pev, "monster_alien_grunt" ) || FClassnameIs( pTarget->pev, "monster_gargantua" )
			  || FClassnameIs( pTarget->pev, "monster_bigmomma" ) ) )
	{
		return R_NM;
	}

	return CSquadMonster::IRelationship( pTarget, ignoreNoTarget );
}

//=========================================================
// GibMonster - make gun fly through the air.
//=========================================================
void CHGrunt :: GibMonster ( void )
{
	if ( GetBodygroup( 2 ) != 2 )
	{// throw a gun if the grunt has one
		DropWeapon();
	}

	KillLaserSpot();

	CBaseMonster :: GibMonster();
}

//=========================================================
// ISoundMask - Overidden for human grunts because they 
// hear the DANGER sound that is made by hand grenades and
// other dangerous items.
//=========================================================
int CHGrunt :: ISoundMask ( void )
{
	return	bits_SOUND_WORLD	|
			bits_SOUND_COMBAT	|
			bits_SOUND_PLAYER	|
			bits_SOUND_DANGER;
}

//=========================================================
// someone else is talking - don't speak
//=========================================================
BOOL CHGrunt :: FOkToSpeak( void )
{
// if someone else is talking, don't speak
	if ( !IsAlive() || CTalkMonster::g_talkWaitTime > gpGlobals->time )
		return FALSE;

	if ( pev->spawnflags & SF_MONSTER_GAG )
		return m_MonsterState == MONSTERSTATE_COMBAT;

	return TRUE;
}

//=========================================================
//=========================================================
void CHGrunt :: JustSpoke( void )
{
	CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT(1.5, 2.0);
	m_iSentence = HGRUNT_SENT_NONE;
}

void CHGrunt::UseSound( void )
{
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "hgrunt/yessir.wav", 0.8f, 0.8f, 0, m_voicePitch );
		break;
	case 1:
		PlaySentence( "!HG_ANSWER0", 4.0f, 1.0f, 0.8f );
		break;
	}

	JustSpoke();
}

void CHGrunt::UnUseSound( void )
{
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		PlaySentence( "!HG_TAUNT1", 4.0f, 1.0f, 0.8f );
		break;
	case 1:
		PlaySentence( "!HG_QUEST2", 4.0f, 1.0f, 0.8f );
		break;
	}

	JustSpoke();
}

//=========================================================
// PrescheduleThink - this function runs after conditions
// are collected and before scheduling code is run.
//=========================================================
void CHGrunt :: PrescheduleThink ( void )
{
	if ( InSquad() && m_hEnemy != NULL )
	{
		if ( HasConditions ( bits_COND_SEE_ENEMY ) )
			MySquadLeader()->m_flLastEnemySightTime = gpGlobals->time;
		else if ( gpGlobals->time - MySquadLeader()->m_flLastEnemySightTime > 5 )
			MySquadLeader()->m_fEnemyEluded = TRUE;
			}

	pev->view_ofs = Vector( 0, 0, m_fStanding ? 60 : 48 );

	if ( m_hEnemy != NULL )
		UpdateSpot( false );
}

//=========================================================
// FCanCheckAttacks - this is overridden for human grunts
// because they can throw/shoot grenades when they can't see their
// target and the base class doesn't check attacks if the monster
// cannot see its enemy.
//
// !!!BUGBUG - this gets called before a 3-round burst is fired
// which means that a friendly can still be hit with up to 2 rounds. 
// ALSO, grenades will not be tossed if there is a friendly in front,
// this is a bad bug. Friendly machine gun fire avoidance
// will unecessarily prevent the throwing of a grenade as well.
//=========================================================
BOOL CHGrunt :: FCanCheckAttacks ( void )
{
	if ( !HasConditions( bits_COND_ENEMY_TOOFAR ) )
		return TRUE;

	return HasConditions( bits_COND_SEE_ENEMY ) && ( pev->weapons & ( HGRUNT_RPG | HGRUNT_SNIPER ) ) != 0;
}


//=========================================================
// CheckMeleeAttack1
//=========================================================
BOOL CHGrunt :: CheckMeleeAttack1 ( float flDot, float flDist )
{
	if ( m_hEnemy == NULL )
		return FALSE;

	if ( flDist <= 64 && flDot >= 0.7	&& 
		 m_hEnemy->Classify() != CLASS_ALIEN_BIOWEAPON &&
		 m_hEnemy->Classify() != CLASS_PLAYER_BIOWEAPON )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// CheckRangeAttack1 - overridden for HGrunt, cause 
// FCanCheckAttacks() doesn't disqualify all attacks based
// on whether or not the enemy is occluded because unlike
// the base class, the HGrunt can attack when the enemy is
// occluded (throw grenade over wall, etc). We must 
// disqualify the machine gun attack if the enemy is occluded.
//=========================================================
BOOL CHGrunt :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) )
		return FALSE;

	if ( pev->weapons & HGRUNT_SNIPER )
		return CheckRangeAttack1Sniper( flDist, m_hEnemy );

	if ( flDot < 0.5f )
		return FALSE;

	if ( pev->weapons & HGRUNT_RPG )
		return CheckRangeAttack1Rocket( flDist, m_hEnemy );

	if ( flDist > 2048 || !NoFriendlyFire() )
		return FALSE;

		if ( !m_hEnemy->IsPlayer() && flDist <= 64 )
		return FALSE;

	Vector vecSrc = pev->origin + Vector( 0, 0, 60 );
	Vector crouching = pev->origin + Vector( 0, 0, 48 );
	TraceResult tr;

	if ( RANDOM_LONG( 0, 9 ) == 0 && m_Activity != ACT_RANGE_ATTACK1 )
		m_fStanding = RANDOM_LONG( 0, 1 ) != 0;

	if ( m_fStanding )
	{
		UTIL_TraceLine( vecSrc, m_hEnemy->BodyTarget( vecSrc ), ignore_monsters, ignore_glass, edict(), &tr );

		if ( tr.flFraction != 1.0f && tr.pHit != m_hEnemy->edict() )
		{
			UTIL_TraceLine( crouching, m_hEnemy->BodyTarget( crouching ), ignore_monsters, ignore_glass, edict(), &tr );

			if ( tr.flFraction != 1.0f && tr.pHit != m_hEnemy->edict() )
				return FALSE;

			m_fStanding = FALSE;
		}

		return TRUE;
	}

	UTIL_TraceLine( crouching, m_hEnemy->BodyTarget( crouching ), ignore_monsters, ignore_glass, edict(), &tr );

	if ( tr.flFraction == 1.0f || tr.pHit == m_hEnemy->edict() )
		return TRUE;

		// verify that a bullet fired from the gun will hit the enemy before the world.
		UTIL_TraceLine( vecSrc, m_hEnemy->BodyTarget(vecSrc), ignore_monsters, ignore_glass, edict(), &tr);

		if ( tr.flFraction != 1.0f && tr.pHit != m_hEnemy->edict() )
		return FALSE;

	m_fStanding = TRUE;
	return TRUE;
		}

//=========================================================
// CheckRangeAttack2 - this checks the Grunt's grenade
// attack. 
//=========================================================
BOOL CHGrunt :: CheckRangeAttack2 ( float flDot, float flDist )
{
	if (! FBitSet(pev->weapons, (HGRUNT_HANDGRENADE | HGRUNT_GRENADELAUNCHER)))
	{
		return FALSE;
	}

	// rocket and sniper grunts keep shooting at an enemy they can see
	if ( ( pev->weapons & ( HGRUNT_RPG | HGRUNT_SNIPER ) ) && HasConditions( bits_COND_SEE_ENEMY ) )
	{
		m_fThrowGrenade = FALSE;
		return m_fThrowGrenade;
	}

	// assume things haven't changed too much since last time
	if (gpGlobals->time < m_flNextGrenadeCheck )
	{
		return m_fThrowGrenade;
	}

	if ( !FBitSet ( m_hEnemy->pev->flags, FL_ONGROUND ) && m_hEnemy->pev->waterlevel == 0 && m_vecEnemyLKP.z > pev->absmax.z  )
	{
		//!!!BUGBUG - we should make this check movetype and make sure it isn't FLY? Players who jump a lot are unlikely to 
		// be grenaded.
		// don't throw grenades at anything that isn't on the ground!
		m_fThrowGrenade = FALSE;
		return m_fThrowGrenade;
	}

	Vector vecTarget;

	if (FBitSet( pev->weapons, HGRUNT_HANDGRENADE))
	{
		// find feet
		if (RANDOM_LONG(0,1))
		{
			// magically know where they are
			Vector vecFeet = m_hEnemy->IsBSPModel() ? m_hEnemy->Center() : m_hEnemy->pev->origin;

			vecTarget = Vector( vecFeet.x, vecFeet.y, m_hEnemy->pev->absmin.z );
		}
		else
		{
			// toss it to where you last saw them
			vecTarget = m_vecEnemyLKP;
		}
	}
	else
	{
		// find target
		Vector vecBase = m_hEnemy->IsBSPModel() ? m_hEnemy->Center() : m_hEnemy->pev->origin;

		vecTarget = m_vecEnemyLKP + ( m_hEnemy->BodyTarget( pev->origin ) - vecBase );

		// estimate position
		if (HasConditions( bits_COND_SEE_ENEMY))
			vecTarget = vecTarget + ((vecTarget - pev->origin).Length() / gSkillData.hgruntGrenadeSpeed) * m_hEnemy->pev->velocity;
	}

	// are any of my squad members near the intended grenade impact area? Launched grenades
	// also need some distance from the grunt himself
	if ( SquadMemberInRange( vecTarget, 256 )
		 || ( ( vecTarget - pev->origin ).Length2D() <= 256 && FBitSet( pev->weapons, HGRUNT_GRENADELAUNCHER ) ) )
	{
		m_fThrowGrenade = FALSE;
		m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
		return m_fThrowGrenade;
	}

	Vector vecToss;

	if (FBitSet( pev->weapons, HGRUNT_HANDGRENADE))
		vecToss = VecCheckToss( pev, GetGunPosition(), vecTarget, 1.0 );
	else
		vecToss = VecCheckThrow( pev, GetGunPosition(), vecTarget, gSkillData.hgruntGrenadeSpeed, 0.7 );

	if ( vecToss != g_vecZero )
	{
		m_vecTossVelocity = vecToss;

		// throw a hand grenade
		m_fThrowGrenade = TRUE;
		// don't check again for a while.
		m_flNextGrenadeCheck = gpGlobals->time + 0.3; // 1/3 second.
		return m_fThrowGrenade;
	}

	// don't throw
	m_fThrowGrenade = FALSE;
	// don't check again for a while.
	m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.

	return m_fThrowGrenade;
}


//=========================================================
// TraceAttack - make sure we're not taking it in the helmet
//=========================================================
void CHGrunt :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	if ( ptr->iHitgroup == HITGROUP_CHEST || ptr->iHitgroup == HITGROUP_STOMACH )
	{
		if ( bitsDamageType & ( DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_SNIPER ) )
		{
			if ( flDamage >= 2 )
				flDamage -= 2;

			flDamage *= 0.5;
		}
	}
	// check for helmet shot
	else if (ptr->iHitgroup == 11)
	{
		// make sure we're wearing one
		if (GetBodygroup( 1 ) == HEAD_GRUNT && (bitsDamageType & (DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_CLUB | DMG_SNIPER)))
		{
			// absorb damage
			flDamage -= 20;
			if (flDamage <= 0)
			{
				UTIL_Ricochet( ptr->vecEndPos, 1.0 );
				flDamage = 0.01;
			}
		}
		// it's head shot anyways
		ptr->iHitgroup = HITGROUP_HEAD;
	}

	CBaseMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}


//=========================================================
// TakeDamage - overridden for the grunt because the grunt
// needs to forget that he is in cover if he's hurt. (Obviously
// not in a safe place anymore).
//=========================================================
int CHGrunt :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	Forget( bits_MEMORY_INCOVER );

	return CSquadMonster :: TakeDamage ( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CHGrunt :: SetYawSpeed ( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_FLY:
	case ACT_GLIDE:
		ys = 200;
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 180;
		break;
	default:
		ys = 270;
		break;
	}

	pev->yaw_speed = ys;
}

void CHGrunt :: IdleSound( void )
{
	if (FOkToSpeak() && (g_fGruntQuestion || RANDOM_LONG(0,1)))
	{
		if (!g_fGruntQuestion)
		{
			// ask question or make statement
			switch (RANDOM_LONG(0,2))
			{
			case 0: // check in
				SENTENCEG_PlayRndSz(ENT(pev), "HG_CHECK", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch);
				g_fGruntQuestion = 1;
				break;
			case 1: // question
				SENTENCEG_PlayRndSz(ENT(pev), "HG_QUEST", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch);
				g_fGruntQuestion = 2;
				break;
			case 2: // statement
				SENTENCEG_PlayRndSz(ENT(pev), "HG_IDLE", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch);
				break;
			}
		}
		else
		{
			switch (g_fGruntQuestion)
			{
			case 1: // check in
				SENTENCEG_PlayRndSz(ENT(pev), "HG_CLEAR", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch);
				break;
			case 2: // question 
				SENTENCEG_PlayRndSz(ENT(pev), "HG_ANSWER", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch);
				break;
			}
			g_fGruntQuestion = 0;
		}
		JustSpoke();
	}
}

//=========================================================
// CheckAmmo - overridden for the grunt because he actually
// uses ammo! (base class doesn't)
//=========================================================
void CHGrunt :: CheckAmmo ( void )
{
	if ( m_cAmmoLoaded <= 0 )
	{
		SetConditions(bits_COND_NO_AMMO_LOADED);
	}
}

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CHGrunt :: Classify ( void )
{
	return GetClassification( CLASS_HUMAN_MILITARY );
}

void CHGrunt::SetupFriendly( void )
{
	const char *model;

	if ( CClassicMode::GetInstance()->IsEnabled() )
		model = "models/hlclassic/hgrunt.mdl";
	else
		model = IsPlayerAlly() ? "models/hgruntf.mdl" : "models/hgrunt.mdl";

	UTIL_SetModel( this, model );
	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
}

//=========================================================
//=========================================================
CBaseEntity *CHGrunt :: Kick( void )
{
	TraceResult tr;

	UTIL_MakeVectors( pev->angles );
	Vector vecStart = pev->origin;
	vecStart.z += pev->size.z * 0.5;
	Vector vecEnd = vecStart + (gpGlobals->v_forward * 70);

	UTIL_TraceHull( vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr );
	
	if ( tr.pHit && !tr.pHit->free )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		return pEntity;
	}

	return NULL;
}

//=========================================================
// GetGunPosition	return the end of the barrel
//=========================================================

Vector CHGrunt :: GetGunPosition( )
{
	if (m_fStanding && !( pev->weapons & HGRUNT_RPG ) )
		return pev->origin + Vector( 0, 0, 60 );

	return pev->origin + Vector( 0, 0, ( pev->weapons & HGRUNT_SNIPER ) ? 64 : 48 );
	}

//=========================================================
// Shoot
//=========================================================
void CHGrunt :: Shoot ( void )
{
	if (m_hEnemy == NULL)
		return;

	Vector vecShootOrigin = GetGunPosition();
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	UTIL_MakeVectors ( pev->angles );

	Vector	vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT(40,90);

	vecShellVelocity = vecShellVelocity + gpGlobals->v_up * RANDOM_FLOAT(75,200);
	vecShellVelocity = vecShellVelocity + gpGlobals->v_forward * RANDOM_FLOAT(-40, 40);
	EjectBrass ( vecShootOrigin - vecShootDir * -24, vecShellVelocity, pev->angles.y, m_iBrassShell, TE_BOUNCE_SHELL);
	pev->effects |= EF_MUZZLEFLASH;
	--m_cAmmoLoaded;

	Vector angles = UTIL_VecToAngles( vecShootDir );

	SetBlending( 0, angles.x );

	if ( !( pev->weapons & HGRUNT_GRENADELAUNCHER ) || CClassicMode::GetInstance()->IsEnabled() )
	{
		FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_10DEGREES, 2048, BULLET_MONSTER_MP5 );

		if ( pev->movetype != MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE
			&& !CClassicMode::GetInstance()->IsEnabled() )
		{
			m_fSequenceFinished = TRUE;
			m_Activity = ACT_RESET;
		}
	}
	else
	{
		FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_8DEGREES, 2048, BULLET_MONSTER_SAW );
}
}

//=========================================================
// Shoot
//=========================================================
void CHGrunt :: Shotgun ( void )
{
	if (m_hEnemy == NULL)
		return;

	Vector vecShootOrigin = GetGunPosition();
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	UTIL_MakeVectors ( pev->angles );

	Vector	vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT(40,90);

	vecShellVelocity = vecShellVelocity + gpGlobals->v_up * RANDOM_FLOAT(75,200);
	vecShellVelocity = vecShellVelocity + gpGlobals->v_forward * RANDOM_FLOAT(-40, 40);
	EjectBrass ( vecShootOrigin - vecShootDir * -24, vecShellVelocity, pev->angles.y, m_iShotgunShell, TE_BOUNCE_SHOTSHELL);
	pev->effects |= EF_MUZZLEFLASH;
	--m_cAmmoLoaded;// take away a bullet!

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, angDir.x );

	if ( ( m_hEnemy->Center() - pev->origin ).Length2D() >= 768 || m_cAmmoLoaded <= 0
		|| CClassicMode::GetInstance()->IsEnabled() )
	{
		FireBullets( (ULONG) sk_hgrunt_pellets.value, vecShootOrigin, vecShootDir, VECTOR_CONE_15DEGREES,
		             2048, BULLET_MONSTER_BUCKSHOT, 0 );
	}
	else
	{
		FireBullets( (ULONG) sk_hgrunt_pellets.value, vecShootOrigin, vecShootDir, VECTOR_CONE_20DEGREES,
		             2048, BULLET_MONSTER_BUCKSHOT, 0 );

		if ( pev->movetype != MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
			m_flAutomaticAttackTime = gpGlobals->time + 0.25;
	}
}

void CHGrunt::RPGShoot( void )
{
	if ( m_hEnemy == NULL )
		return;

	Vector source = GetGunPosition();
	Vector angles = UTIL_VecToAngles( m_hEnemy->Center() - source );

	m_hRPGEntity = CRpgRocket::CreateRpgRocket( source, angles, this, NULL );
	--m_cAmmoLoaded;
	SetBlending( 0, angles.x );
}

void CHGrunt::SniperShoot( void )
{
	if ( m_hEnemy == NULL )
		return;

	UTIL_MakeVectors( pev->angles );

	Vector source = GetGunPosition();
	Vector direction = ShootAtEnemy( source );

	direction = direction - m_hEnemy->pev->velocity / 15000;

	Vector shellVelocity = gpGlobals->v_right * RANDOM_FLOAT( 40, 90 );

	shellVelocity = shellVelocity + gpGlobals->v_up * RANDOM_FLOAT( 75, 200 );
	shellVelocity = shellVelocity + gpGlobals->v_forward * RANDOM_FLOAT( -40, 40 );
	EjectBrass( source - direction * -24, shellVelocity, pev->angles.y, m_iBrassShell, TE_BOUNCE_SHELL );
	FireBullets( 1, source, direction, g_vecZero, 8192, BULLET_MONSTER_SNIPER, 0 );

	Vector angles = UTIL_VecToAngles( direction );

	SetBlending( 0, angles.x );
	pev->effects |= EF_MUZZLEFLASH;
	--m_cAmmoLoaded;
}

CBaseEntity *CHGrunt::DropWeapon( void )
{
	switch ( pev->weapons & ~HGRUNT_HANDGRENADE )
	{
		case HGRUNT_9MMAR:
			return CBaseMonster::DropWeapon( "weapon_9mmAR", false );
		case HGRUNT_GRENADELAUNCHER:
		{
			CBaseEntity *weapon = CBaseMonster::DropWeapon( "weapon_m16", false );

			CBaseMonster::DropWeapon( "ammo_ARgrenades", false );
			return weapon;
		}
		case HGRUNT_SHOTGUN:
			return CBaseMonster::DropWeapon( "weapon_shotgun", false );
		case HGRUNT_RPG:
			return CBaseMonster::DropWeapon( "weapon_rpg", false );
		case HGRUNT_SNIPER:
			return CBaseMonster::DropWeapon( "weapon_sniperrifle", false );
		default:
			return NULL;
	}
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CHGrunt :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case HGRUNT_AE_DROP_GUN:
			DropWeapon();
			break;

		case HGRUNT_AE_RELOAD:
			EMIT_SOUND( ENT(pev), CHAN_WEAPON,
			            ( pev->weapons & HGRUNT_SNIPER ) ? "weapons/sniper_bolt1.wav" : "hgrunt/gr_reload1.wav", 1, 0.8f );
			m_cAmmoLoaded = m_cClipSize;
			ClearConditions(bits_COND_NO_AMMO_LOADED);
			break;

		case HGRUNT_AE_GREN_TOSS:
			UTIL_MakeVectors( pev->angles );
			// CGrenade::ShootTimed( pev, pev->origin + gpGlobals->v_forward * 34 + Vector (0, 0, 32), m_vecTossVelocity, 3.5 );
			CGrenade::ShootTimed( pev, GetGunPosition(), m_vecTossVelocity, 3.5 );

			m_fThrowGrenade = FALSE;
			m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 5, 10 );
			break;

		case HGRUNT_AE_GREN_LAUNCH:
			EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/glauncher.wav", 0.8, 0.8f );
			CGrenade::ShootContact( pev, GetGunPosition(), m_vecTossVelocity );
			m_fThrowGrenade = FALSE;
			m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 4, 8 );
			break;

		case HGRUNT_AE_GREN_DROP:
			UTIL_MakeVectors( pev->angles );
			CGrenade::ShootTimed( pev, pev->origin + gpGlobals->v_forward * 17 - gpGlobals->v_right * 27 + gpGlobals->v_up * 6, g_vecZero, 3 );
			break;

		case HGRUNT_AE_BURST1:
			if ( !NoFriendlyFire() && m_hEnemy != NULL && m_hEnemy->IsAlive() )
				break;

			if ( pev->weapons & HGRUNT_9MMAR )
			{
				Shoot();

				// the first round of the three round burst plays the sound and puts a sound in the world sound list.
				if ( CClassicMode::GetInstance()->IsEnabled() )
				{
					EMIT_SOUND( ENT(pev), CHAN_WEAPON,
					            RANDOM_LONG( 0, 1 ) ? "hlclassic/hgrunt/gr_mgun1.wav" : "hlclassic/hgrunt/gr_mgun2.wav", 1, 0.8f );
				}
				else if ( pev->weapons & HGRUNT_GRENADELAUNCHER )
				{
					EMIT_SOUND( ENT( pev ), CHAN_AUTO, "weapons/m16_3round.wav", 1, 0.8f );
				}
				else
				{
					switch ( RANDOM_LONG( 0, 2 ) )
					{
						case 0:
							EMIT_SOUND( ENT( pev ), CHAN_AUTO, "weapons/hks1.wav", 1, 0.8f );
							break;
						case 1:
							EMIT_SOUND( ENT( pev ), CHAN_AUTO, "weapons/hks2.wav", 1, 0.8f );
							break;
						case 2:
							EMIT_SOUND( ENT( pev ), CHAN_AUTO, "weapons/hks3.wav", 1, 0.8f );
							break;
					}
				}
			}
			else if ( pev->weapons & HGRUNT_RPG )
			{
				if ( pev->movetype != MOVETYPE_FLY )
					RPGShoot();
			}
			else if ( pev->weapons & HGRUNT_SNIPER )
			{
				if ( pev->movetype != MOVETYPE_FLY )
				{
					SniperShoot();
					EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/sniper_fire.wav", 1, 0.4f );
				}
			}
			else
			{
				Shotgun( );

				EMIT_SOUND(ENT(pev), CHAN_AUTO,
				            CClassicMode::GetInstance()->IsEnabled() ? "hlclassic/weapons/sbarrel1.wav" : "weapons/sbarrel1.wav", 1, 0.8f );
			}

			CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, 1024, 0.3f, this );
			break;

		case HGRUNT_AE_BURST2:
		case HGRUNT_AE_BURST3:
			if ( ( pev->weapons & HGRUNT_GRENADELAUNCHER ) || CClassicMode::GetInstance()->IsEnabled() )
			{
				if ( NoFriendlyFire() || m_hEnemy == NULL || !m_hEnemy->IsAlive() )
					Shoot();
			}
			break;

		case HGRUNT_AE_KICK:
		{
			CBaseEntity *pHurt = Kick();

			if ( pHurt == NULL )
				break;

			if ( ( ( pHurt->pev->flags & FL_CLIENT ) || ( ( pHurt->pev->flags & FL_MONSTER )
				&& pHurt->pev->movetype != MOVETYPE_FLY ) )
				&& !FClassnameIs( pHurt->pev, "monster_alien_voltigore" )
				&& !FClassnameIs( pHurt->pev, "monster_gargantua" )
				&& !FClassnameIs( pHurt->pev, "monster_bigmomma" ) )
			{
				// SOUND HERE!
				UTIL_MakeVectors( pev->angles );
				pHurt->pev->punchangle.x = 45;
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * 300 + gpGlobals->v_up * 50;
			}

			pHurt->TakeDamage( pev, pev, sk_hgrunt_kick.value, DMG_CLUB );
			break;
		}
		case HGRUNT_AE_CAUGHT_ENEMY:
			if ( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz(ENT(pev), "HG_ALERT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
				 JustSpoke();
			}

		default:
			CSquadMonster::HandleAnimEvent( pEvent );
			break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CHGrunt :: Spawn( void )
{
	Precache( );

	if ( !SetupModel() )
		SetupFriendly();
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->effects		= 0;

	if ( pev->health == 0 )
		pev->health			= sk_hgrunt_health.value;
	m_flFieldOfView		= -0.7f;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_flNextGrenadeCheck = gpGlobals->time + 1;
	m_flNextPainTime	= gpGlobals->time;
	m_iSentence			= HGRUNT_SENT_NONE;

	m_afCapability		= bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP | bits_CAP_USE_TANK;

	m_fEnemyEluded		= FALSE;
	m_fFirstEncounter = TRUE;
	m_fCheckCreatureDanger = TRUE;// this is true when the grunt spawns, because he hasn't encountered an enemy yet.

	m_HackedGunPos = Vector ( 0, 0, 55 );
	m_hRPGEntity = NULL;
	m_hRPGLaserSpot = NULL;
	m_flRPGSafetyRadius = (float) ( sk_plr_rpg.value * 2.5 );
	m_fHasM16 = LookupSequence( "standing_m16a2" ) != -1;

	if (pev->weapons == 0)
		pev->weapons = HGRUNT_9MMAR | HGRUNT_HANDGRENADE;

	m_afMoveShootCap = 0;

	if ( pev->weapons & HGRUNT_SHOTGUN )
	{
		SetBodygroup( HEAD_GROUP, HEAD_SHOTGUN );
		SetBodygroup( GUN_GROUP, GUN_SHOTGUN );
		m_cClipSize		= 8;
	}
	else if ( pev->weapons & HGRUNT_RPG )
	{
		m_afMoveShootCap = bits_MOVESHOOTCAP_RPG;
		SetBodygroup( HEAD_GROUP, 4 );
		SetBodygroup( GUN_GROUP, 3 );
		m_cClipSize = 1;
	}
	else if ( pev->weapons & HGRUNT_SNIPER )
	{
		SetBodygroup( HEAD_GROUP, 4 );
		SetBodygroup( GUN_GROUP, 5 );
		m_cClipSize = 5;
	}
	else
	{
		if ( m_fHasM16 )
			SetBodygroup( GUN_GROUP, 4 );

		m_cClipSize		= GRUNT_CLIP_SIZE;
	}
	m_cAmmoLoaded		= m_cClipSize;
	pev->skin = RANDOM_LONG( 0, 99 ) >= 80;

	if ( pev->weapons & HGRUNT_GRENADELAUNCHER )
	{
		pev->weapons = HGRUNT_9MMAR | HGRUNT_GRENADELAUNCHER;
		SetBodygroup( HEAD_GROUP, HEAD_M203 );
		SetBodygroup( GUN_GROUP, CClassicMode::GetInstance()->IsEnabled() ? 4 : 0 );
		pev->skin = 1; // alway dark skin
	}

	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( "Human Grunt" );

	MonsterInit();

	if ( pev->weapons & HGRUNT_SNIPER )
		m_flDistLook = 4096;

	if ( IsPlayerAlly() && !( pev->spawnflags & SF_MONSTER_PREDISASTER ) )
	{
		SetUse( &CBaseMonster::CallFollowerPlayerUse );
		m_fCanPlayerFollow = true;
	}
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CHGrunt :: Precache( void )
{
	CBaseMonster::Precache();

	if ( !pev->model )
	{
		const char *model;

		if ( CClassicMode::GetInstance()->IsEnabled() )
			model = "models/hlclassic/hgrunt.mdl";
		else
			model = IsPlayerAlly() ? "models/hgruntf.mdl" : "models/hgrunt.mdl";

		UTIL_PrecacheModel( this, model );
	}

	PrecacheSound_Intercept( this, "hgrunt/yessir.wav" );
	PrecacheSound_Intercept( this, "hlclassic/hgrunt/gr_mgun1.wav" );
	PrecacheSound_Intercept( this, "hlclassic/hgrunt/gr_mgun2.wav" );
	PrecacheSound_Intercept( this, "hlclassic/weapons/sbarrel1.wav" );
	PrecacheSound_Intercept( this, "weapons/hks1.wav" );
	PrecacheSound_Intercept( this, "weapons/hks2.wav" );
	PrecacheSound_Intercept( this, "weapons/hks3.wav" );
	PrecacheSound_Intercept( this, "weapons/m16_3round.wav" );
	PrecacheSound_Intercept( this, "hgrunt/gr_mgun1.wav" );
	PrecacheSound_Intercept( this, "weapons/sbarrel1.wav" );
	PrecacheSound_Intercept( this, "hgrunt/gr_die1.wav" );
	PrecacheSound_Intercept( this, "hgrunt/gr_die2.wav" );
	PrecacheSound_Intercept( this, "hgrunt/gr_die3.wav" );
	PrecacheSound_Intercept( this, "hgrunt/gr_pain1.wav" );
	PrecacheSound_Intercept( this, "hgrunt/gr_pain2.wav" );
	PrecacheSound_Intercept( this, "hgrunt/gr_pain3.wav" );
	PrecacheSound_Intercept( this, "hgrunt/gr_pain4.wav" );
	PrecacheSound_Intercept( this, "hgrunt/gr_pain5.wav" );
	PrecacheSound_Intercept( this, "hgrunt/gr_reload1.wav" );
	PrecacheSound_Intercept( this, "weapons/glauncher.wav" );
	PrecacheSound_Intercept( this, "weapons/sniper_fire.wav" );
	PrecacheSound_Intercept( this, "weapons/sniper_reload_first_seq.wav" );
	PrecacheSound_Intercept( this, "weapons/sniper_bolt1.wav" );
	PrecacheSound_Intercept( this, "zombie/claw_miss2.wav");// because we use the basemonster SWIPE animation event

	// get voice pitch
	if (RANDOM_LONG(0,1))
		m_voicePitch = 109 + RANDOM_LONG(0,7);
	else
		m_voicePitch = 100;

	bool classic = CClassicMode::GetInstance()->IsEnabled();

	m_iBrassShell = UTIL_PrecacheModel( this, classic ? "models/hlclassic/shell.mdl" : "models/shell.mdl");// brass shell
	m_iShotgunShell = UTIL_PrecacheModel( this, classic ? "models/hlclassic/shotgunshell.mdl" : "models/shotgunshell.mdl");
}

//=========================================================
// start task
//=========================================================
void CHGrunt :: StartTask ( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_WALK_PATH:
	case TASK_RUN_PATH:
		// grunt no longer assumes he is covered if he moves
		Forget( bits_MEMORY_INCOVER );
		CSquadMonster ::StartTask( pTask );
			break;

	case TASK_FACE_IDEAL:
	case TASK_FACE_ENEMY:
		CSquadMonster :: StartTask( pTask );
		if (pev->movetype == MOVETYPE_FLY)
				m_IdealActivity = ACT_GLIDE;
			break;

		case TASK_RANGE_ATTACK1:
			if ( ( pev->weapons & HGRUNT_RPG ) && m_hRPGLaserSpot == NULL )
			{
				m_hRPGLaserSpot = CRPGSpot::CreateSpot();
				m_hRPGLaserSpot->pev->owner = edict();
				m_hRPGLaserSpot->pev->movetype = MOVETYPE_FLY;

				UTIL_MakeVectors( UTIL_VecToAngles( m_vecEnemyLKP - Center() ) );

				Vector end = GetGunPosition() + gpGlobals->v_forward * 4096;
				TraceResult trace;

				UTIL_TraceLine( GetGunPosition(), end, dont_ignore_monsters, edict(), &trace );
				m_hRPGLaserSpot->pev->origin = trace.vecEndPos;
				m_hRPGLaserSpot->pev->velocity = g_vecZero;
				m_vecRPGCurrentAngles = pev->angles;
				UpdateSpot( true );
			}

			CSquadMonster::StartTask( pTask );
			break;

		case TASK_RELOAD:
			m_IdealActivity = ACT_RELOAD;
			break;

		case TASK_FIND_ATTACKPOINT_TO_ENEMY:
		{
			if ( m_hEnemy == NULL )
			{
				TaskFail();
				break;
			}

			BOOL found;

			if ( pev->weapons & HGRUNT_RPG )
			{
				found = FindLateralAttackPoint( m_hEnemy->Center(), m_hEnemy->pev->view_ofs, m_flRPGSafetyRadius )
				        || FindAttackPoint( m_hEnemy->Center(), m_hEnemy->pev->view_ofs, m_flRPGSafetyRadius, 0 );
			}
			else
			{
				found = FindLateralAttackPoint( m_hEnemy->Center(), m_hEnemy->pev->view_ofs )
				        || FindAttackPoint( m_hEnemy->Center(), m_hEnemy->pev->view_ofs, 0, CoverRadius() );
			}

			if ( found )
			{
				m_flMoveWaitFinished = gpGlobals->time + pTask->flData;
				TaskComplete();
			}
			else
				TaskFail();
			break;
		}

		case TASK_GRUNT_FACE_TOSS_DIR:
			break;

		case TASK_GRUNT_SPEAK_SENTENCE:
			SpeakSentence();
			TaskComplete();
			break;

		case TASK_GRUNT_CHECK_FIRE:
		{
			TaskComplete();

			bool clear = NoFriendlyFire();

			if ( pev->weapons & HGRUNT_RPG )
			{
				CBaseEntity *entity = NULL;

				while ( ( entity = UTIL_FindEntityInSphere( entity, m_hEnemy->pev->origin, m_flRPGSafetyRadius ) ) != NULL )
				{
					if ( ( entity->pev->flags & FL_MONSTER ) && IRelationship( entity ) <= R_NO
					     && entity->IsAlive() && FVisible( entity, true ) )
					{
						clear = false;
						break;
					}
				}
			}

			if ( !clear )
			{
				ChangeSchedule( GetScheduleOfType( SCHED_FIND_ATTACK_POINT ) );
				SetConditions( bits_COND_GRUNT_NOFIRE );
			}
		break;
		}

		case TASK_GRUNT_WAIT_ROCKET:
			m_IdealActivity = ACT_CROUCH;
			UpdateSpot( false );
			break;

		case TASK_GRUNT_WAIT_SNIPER:
		{
			m_IdealActivity = ACT_IDLE_ANGRY;

			if ( m_hEnemy == NULL )
			{
				m_flWaitFinished = gpGlobals->time + pTask->flData;
				break;
			}

			m_flWaitFinished = gpGlobals->time;

			float distance = ( m_hEnemy->Center() - Center() ).Length();

			if ( m_hEnemy->IsMoving() )
				m_flWaitFinished += 0.3f;

			if ( distance < 384 )
				m_flWaitFinished += 0.5f;
			else if ( distance < 1024 )
				m_flWaitFinished += 0.8f;
			else
				m_flWaitFinished += 1.5f;
			break;
		}

		default:
		CSquadMonster :: StartTask( pTask );
		break;
	}
}

//=========================================================
// RunTask
//=========================================================
void CHGrunt :: RunTask ( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_WAIT_FOR_MOVEMENT_AP:
			if ( m_movementGoal == MOVEGOAL_NONE )
			{
				TaskComplete();
				Stop();
			}

			if ( !( pev->weapons & HGRUNT_RPG ) || !HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) || HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
			{
				if ( m_hEnemy != NULL && NoFriendlyFire() && HasConditions( bits_COND_CAN_ATTACK ) )
				{
					TaskComplete();
					Stop();
				}
			}
			break;

		case TASK_GRUNT_FACE_TOSS_DIR:
			MakeIdealYaw( pev->origin + m_vecTossVelocity * 64 );
			ChangeYaw( pev->yaw_speed );

			if ( FacingIdeal() )
				m_iTaskStatus = TASKSTATUS_COMPLETE;
			break;

		case TASK_GRUNT_WAIT_ROCKET:
			if ( m_hRPGEntity == NULL || m_hEnemy == NULL || ( m_hRPGEntity->pev->effects & EF_NODRAW ) )
			{
				KillLaserSpot();
				TaskComplete();
				break;
		}

			UpdateSpot( false );
			MakeIdealYaw( m_vecEnemyLKP );
			ChangeYaw( pev->yaw_speed );
			break;

		case TASK_GRUNT_WAIT_SNIPER:
			MakeIdealYaw( m_vecEnemyLKP );
			ChangeYaw( pev->yaw_speed );

			if ( gpGlobals->time >= m_flWaitFinished )
				TaskComplete();
			break;

		default:
			CSquadMonster :: RunTask( pTask );
			break;
		}
	}

//=========================================================
// PainSound
//=========================================================
void CHGrunt :: PainSound ( void )
{
	if ( gpGlobals->time > m_flNextPainTime )
	{
#if 0
		if ( RANDOM_LONG(0,99) < 5 )
		{
			// pain sentences are rare
			if (FOkToSpeak())
			{
				SENTENCEG_PlayRndSz(ENT(pev), "HG_PAIN", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, PITCH_NORM);
				JustSpoke();
				return;
			}
		}
#endif 
		switch ( RANDOM_LONG(0,6) )
		{
		case 0:	
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "hgrunt/gr_pain3.wav", 1, ATTN_NORM );	
			break;
		case 1:
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "hgrunt/gr_pain4.wav", 1, ATTN_NORM );	
			break;
		case 2:
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "hgrunt/gr_pain5.wav", 1, ATTN_NORM );	
			break;
		case 3:
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "hgrunt/gr_pain1.wav", 1, ATTN_NORM );	
			break;
		case 4:
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "hgrunt/gr_pain2.wav", 1, ATTN_NORM );	
			break;
		}

		m_flNextPainTime = gpGlobals->time + 1;
	}
}

//=========================================================
// DeathSound 
//=========================================================
void CHGrunt :: DeathSound ( void )
{
	switch ( RANDOM_LONG(0,2) )
	{
	case 0:	
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "hgrunt/gr_die1.wav", 1, ATTN_IDLE );	
		break;
	case 1:
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "hgrunt/gr_die2.wav", 1, ATTN_IDLE );	
		break;
	case 2:
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "hgrunt/gr_die3.wav", 1, ATTN_IDLE );	
		break;
	}

	KillLaserSpot();
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

//=========================================================
// GruntFail
//=========================================================
Task_t tlGruntFail[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 2 },
};

Schedule_t slGruntFail[] =
{
	{
		tlGruntFail,
		ARRAYSIZE( tlGruntFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK2 |
		bits_COND_NEW_ENEMY,
		0,
		"Grunt Fail"
	},
};

Task_t tlGruntCombatFail[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 0.5f },
};

Schedule_t slGruntCombatFail[] =
{
	{
		tlGruntCombatFail,
		ARRAYSIZE( tlGruntCombatFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"Grunt Combat Fail"
	},
};

Task_t tlGruntVictoryDance[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_WAIT, 1.5f },
	{ TASK_GET_PATH_TO_ENEMY_CORPSE, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
};

Schedule_t slGruntVictoryDance[] =
{
	{
		tlGruntVictoryDance,
		ARRAYSIZE( tlGruntVictoryDance ),
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntVictoryDance"
	},
};

Task_t tlGruntEstablishLineOfFire[] =
{
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_GRUNT_ELOF_FAIL },
	{ TASK_GET_PATH_TO_ENEMY, 0 },
	{ TASK_GRUNT_SPEAK_SENTENCE, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_AP, 0 },
};

Schedule_t slGruntEstablishLineOfFire[] =
{
	{
		tlGruntEstablishLineOfFire,
		ARRAYSIZE( tlGruntEstablishLineOfFire ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK2 |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD,
		bits_SOUND_DANGER,
		"GruntEstablishLineOfFire"
	},
};

Task_t tlGruntFoundEnemy[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_SIGNAL1 },
};

Schedule_t slGruntFoundEnemy[] =
{
	{
		tlGruntFoundEnemy,
		ARRAYSIZE( tlGruntFoundEnemy ),
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntFoundEnemy"
	},
};

Task_t tlGruntCombatFace[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_GRUNT_SWEEP },
};

Schedule_t slGruntCombatFace[] =
{
	{
		tlGruntCombatFace,
		ARRAYSIZE( tlGruntCombatFace ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD,
		0,
		"Combat Face"
	},
};

Task_t tlGruntSignalSuppress[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_SIGNAL2 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
};

Schedule_t slGruntSignalSuppress[] =
{
	{
		tlGruntSignalSuppress,
		ARRAYSIZE( tlGruntSignalSuppress ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"SignalSuppress"
	},
};

Task_t tlGruntSuppress[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
};

Schedule_t slGruntSuppress[] =
{
	{
		tlGruntSuppress,
		ARRAYSIZE( tlGruntSuppress ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"Suppress"
	},
};

Task_t tlGruntWaitInCover[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 1 },
};

Schedule_t slGruntWaitInCover[] =
{
	{
		tlGruntWaitInCover,
		ARRAYSIZE( tlGruntWaitInCover ),
		bits_COND_SEE_ENEMY |
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK2 |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntWaitInCover"
	},
};

Task_t tlGruntTakeCover[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_GRUNT_TAKECOVER_FAILED },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_GRUNT_SPEAK_SENTENCE, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_SET_SCHEDULE, (float) SCHED_GRUNT_WAIT_FACE_ENEMY },
};

Schedule_t slGruntTakeCover[] =
{
	{
		tlGruntTakeCover,
		ARRAYSIZE( tlGruntTakeCover ),
		0,
		0,
		"TakeCover"
	},
};

Task_t tlGruntGrenadeCover[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FIND_COVER_FROM_ENEMY, 30 },
	{ TASK_FIND_FAR_NODE_COVER_FROM_ENEMY, 384 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_SPECIAL_ATTACK1 },
	{ TASK_CLEAR_MOVE_WAIT, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_GRUNT_WAIT_FACE_ENEMY },
};

Schedule_t slGruntGrenadeCover[] =
{
	{
		tlGruntGrenadeCover,
		ARRAYSIZE( tlGruntGrenadeCover ),
		0,
		0,
		"GrenadeCover"
	},
};

Task_t tlGruntTossGrenadeCover[] =
{
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RANGE_ATTACK2, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_TAKE_COVER_FROM_ENEMY },
};

Schedule_t slGruntTossGrenadeCover[] =
{
	{
		tlGruntTossGrenadeCover,
		ARRAYSIZE( tlGruntTossGrenadeCover ),
		0,
		0,
		"TossGrenadeCover"
	},
};

Task_t tlGruntTakeCoverFromBestSound[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_COWER },
	{ TASK_FIND_COVER_FROM_BEST_SOUND, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_GRENADE, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_TURN_LEFT, 179 },
};

Schedule_t slGruntTakeCoverFromBestSound[] =
{
	{
		tlGruntTakeCoverFromBestSound,
		ARRAYSIZE( tlGruntTakeCoverFromBestSound ),
		0,
		bits_SOUND_DANGER,
		"GruntTakeCoverFromBestSound"
	},
};

Task_t tlGruntHideReload[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_RELOAD },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_RELOAD },
};

Schedule_t slGruntHideReload[] =
{
	{
		tlGruntHideReload,
		ARRAYSIZE( tlGruntHideReload ),
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntHideReload"
	},
};

Task_t tlGruntSweep[] =
{
	{ TASK_TURN_LEFT, 179 },
	{ TASK_WAIT, 1 },
	{ TASK_TURN_LEFT, 179 },
	{ TASK_WAIT, 1 },
};

Schedule_t slGruntSweep[] =
{
	{
		tlGruntSweep,
		ARRAYSIZE( tlGruntSweep ),
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_WORLD |
		bits_SOUND_PLAYER |
		bits_SOUND_DANGER,
		"Grunt Sweep"
	},
};

Task_t tlGruntRangeAttack1A[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_CROUCH },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FIND_COVER_DISTANCE_FROM_ENEMY, 1 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FIND_COVER_DISTANCE_FROM_ENEMY, 1 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FIND_COVER_DISTANCE_FROM_ENEMY, 1 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
};

Schedule_t slGruntRangeAttack1A[] =
{
	{
		tlGruntRangeAttack1A,
		ARRAYSIZE( tlGruntRangeAttack1A ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"Range Attack1A"
	},
};

Task_t tlGruntRangeAttack1B[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_WAIT_FACE_ENEMY, 0.1f },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
};

Schedule_t slGruntRangeAttack1B[] =
{
	{
		tlGruntRangeAttack1B,
		ARRAYSIZE( tlGruntRangeAttack1B ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"Range Attack1B"
	},
};

Task_t tlGruntRangeAttack2[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_GRUNT_FACE_TOSS_DIR, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_RANGE_ATTACK2 },
	{ TASK_SET_SCHEDULE, (float) SCHED_GRUNT_WAIT_FACE_ENEMY },
};

Schedule_t slGruntRangeAttack2[] =
{
	{
		tlGruntRangeAttack2,
		ARRAYSIZE( tlGruntRangeAttack2 ),
		0,
		0,
		"RangeAttack2"
	},
};

Task_t tlGruntRangeAttack1Rocket[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_GRUNT_WAIT_ROCKET, 0 },
	{ TASK_WAIT_FACE_ENEMY, 1 },
};

Schedule_t slGruntRangeAttack1Rocket[] =
{
	{
		tlGruntRangeAttack1Rocket,
		ARRAYSIZE( tlGruntRangeAttack1Rocket ),
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_ENEMY_DEAD,
		0,
		"Range Attack1Rocket"
	},
};

Task_t tlGruntRangeAttack1C[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_GRUNT_WAIT_SNIPER, 0.5f },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_GRUNT_WAIT_FACE_ENEMY },
};

Schedule_t slGruntRangeAttack1C[] =
{
	{
		tlGruntRangeAttack1C,
		ARRAYSIZE( tlGruntRangeAttack1C ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"Range Attack1C"
	},
};

Task_t tlGruntRangeAttack1D[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_GRUNT_WAIT_SNIPER, 0 },
	{ TASK_GRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_CLEAR_LASTPOSITION, 0 },
	{ TASK_STORE_LASTPOSITION, 0 },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_CLEAR_LASTPOSITION, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_AP, 0 },
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_GRUNT_WAIT_FACE_ENEMY },
};

Schedule_t slGruntRangeAttack1D[] =
{
	{
		tlGruntRangeAttack1D,
		ARRAYSIZE( tlGruntRangeAttack1D ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"Range Attack1D"
	},
};

Task_t tlGruntRepel[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_GLIDE },
};

Schedule_t slGruntRepel[] =
{
	{
		tlGruntRepel,
		ARRAYSIZE( tlGruntRepel ),
		bits_COND_SEE_ENEMY |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_COMBAT |
		bits_SOUND_PLAYER |
		bits_SOUND_DANGER,
		"Repel"
	},
};

Task_t tlGruntRepelAttack[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_FLY },
};

Schedule_t slGruntRepelAttack[] =
{
	{
		tlGruntRepelAttack,
		ARRAYSIZE( tlGruntRepelAttack ),
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD,
		0,
		"Repel Attack"
	},
};

Task_t tlGruntRepelLand[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_LAND },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_CLEAR_LASTPOSITION, 0 },
};

Schedule_t slGruntRepelLand[] =
{
	{
		tlGruntRepelLand,
		ARRAYSIZE( tlGruntRepelLand ),
		bits_COND_SEE_ENEMY |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_COMBAT |
		bits_SOUND_PLAYER |
		bits_SOUND_DANGER,
		"Repel Land"
	},
};

DEFINE_CUSTOM_SCHEDULES( CHGrunt )
{
	slGruntFail,
	slGruntCombatFail,
	slGruntVictoryDance,
	slGruntEstablishLineOfFire,
	slGruntFoundEnemy,
	slGruntCombatFace,
	slGruntSignalSuppress,
	slGruntSuppress,
	slGruntWaitInCover,
	slGruntTakeCover,
	slGruntGrenadeCover,
	slGruntTossGrenadeCover,
	slGruntTakeCoverFromBestSound,
	slGruntHideReload,
	slGruntSweep,
	slGruntRangeAttack1A,
	slGruntRangeAttack1B,
	slGruntRangeAttack2,
	slGruntRangeAttack1Rocket,
	slGruntRangeAttack1C,
	slGruntRangeAttack1D,
	slGruntRepel,
	slGruntRepelAttack,
	slGruntRepelLand,
};

IMPLEMENT_CUSTOM_SCHEDULES( CHGrunt, CSquadMonster );

void CHGrunt::SetGaitActivity( Activity NewActivity )
{
	m_fStanding = TRUE;

	if ( pev->health <= 20.0f )
	{
		if ( NewActivity == ACT_WALK )
			NewActivity = ACT_WALK_HURT;
		else if ( NewActivity == ACT_RUN )
			NewActivity = ACT_RUN_HURT;
	}

	CBaseMonster::SetGaitActivity( NewActivity );
}

//=========================================================
// SetActivity 
//=========================================================
void CHGrunt :: SetActivity ( Activity NewActivity )
{
	int	iSequence = ACTIVITY_NOT_AVAILABLE;
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	switch ( NewActivity)
	{
	case ACT_RANGE_ATTACK1:
		// grunt is either shooting standing or shooting crouched
		if (FBitSet( pev->weapons, HGRUNT_9MMAR))
		{
			if ( FBitSet( pev->weapons, HGRUNT_GRENADELAUNCHER ) )
			{
				if ( m_fStanding )
				{
					iSequence = LookupSequence( "standing_m16a2" );
				}
				else
				{
					iSequence = LookupSequence( "crouching_m16a2" );
				}
			}

			if ( !FBitSet( pev->weapons, HGRUNT_GRENADELAUNCHER ) || iSequence == ACTIVITY_NOT_AVAILABLE )
			{
				if ( m_fStanding )
				{
					// get aimable sequence
					iSequence = LookupSequence( "standing_mp5" );
				}
				else
				{
					// get crouching shoot
					iSequence = LookupSequence( "crouching_mp5" );
				}
			}
		}
		else if ( FBitSet( pev->weapons, HGRUNT_RPG ) )
		{
			iSequence = LookupSequence( "rpg_shoot" );
		}
		else if ( FBitSet( pev->weapons, HGRUNT_SNIPER ) )
		{
			if ( m_fStanding )
			{
				iSequence = LookupSequence( "standing_m40a1" );
			}
			else
			{
				iSequence = LookupSequence( "crouching_m40a1" );
			}
		}
		else
		{
			if ( m_fStanding )
			{
				// get aimable sequence
				iSequence = LookupSequence( "standing_shotgun" );
			}
			else
			{
				// get crouching shoot
				iSequence = LookupSequence( "crouching_shotgun" );
			}
		}
		break;
	case ACT_RANGE_ATTACK2:
		// grunt is going to a secondary long range attack. This may be a thrown
		// grenade or fired grenade, we must determine which and pick proper sequence
		if ( pev->weapons & HGRUNT_HANDGRENADE )
		{
			// get toss anim
			iSequence = LookupSequence( "throwgrenade" );
		}
		else
		{
			// get launch anim
			iSequence = LookupSequence( "launchgrenade" );
		}
		break;
	case ACT_RUN:
		if ( pev->health <= HGRUNT_LIMP_HEALTH )
		{
			// limp!
			iSequence = LookupActivity ( ACT_RUN_HURT );
			pev->gaitsequence = iSequence;
		}
		else
		{
			iSequence = LookupActivity ( NewActivity );
		}
		break;
	case ACT_WALK:
		if ( pev->health <= HGRUNT_LIMP_HEALTH )
		{
			// limp!
			iSequence = LookupActivity ( ACT_WALK_HURT );
			pev->gaitsequence = iSequence;
		}
		else
		{
			iSequence = LookupActivity ( NewActivity );
		}
		break;
	case ACT_CROUCH:
		if ( FBitSet( pev->weapons, HGRUNT_RPG ) )
		{
			if ( m_MonsterState == MONSTERSTATE_COMBAT )
			{
				iSequence = LookupSequence( "rpg_aim" );
			}
			else
			{
				iSequence = LookupSequence( "rpg_idle" );
			}
		}
		else
		{
			iSequence = LookupActivity ( NewActivity );
		}
		break;
	case ACT_RELOAD:
		if ( FBitSet( pev->weapons, HGRUNT_RPG ) )
		{
			iSequence = LookupSequence( "rpg_reload" );
		}
		else if ( FBitSet( pev->weapons, HGRUNT_SNIPER ) )
		{
			iSequence = LookupSequence( "reload_m40a1" );
			EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/sniper_reload_first_seq.wav", 1, ATTN_NORM );
		}
		else
		{
			iSequence = LookupActivity ( NewActivity );
		}
		break;
	case ACT_IDLE:
		if ( FBitSet( pev->weapons, HGRUNT_RPG ) )
		{
			if ( m_MonsterState == MONSTERSTATE_COMBAT )
			{
				iSequence = LookupSequence( "rpg_aim" );
			}
			else
			{
				iSequence = LookupSequence( "rpg_idle" );
			}
		}
		else if ( m_MonsterState == MONSTERSTATE_COMBAT )
		{
			if ( m_fStanding )
			{
				NewActivity = ACT_IDLE_ANGRY;
			}
			else
			{
				NewActivity = ACT_CROUCHIDLE;
			}

			iSequence = LookupActivity ( NewActivity );

			if ( iSequence == ACTIVITY_NOT_AVAILABLE )
				iSequence = LookupSequence( "crouching_idle" );
		}
		else
		{
			iSequence = LookupActivity ( NewActivity );
		}
		break;
	default:
		iSequence = LookupActivity ( NewActivity );
		break;
	}

	m_Activity = NewActivity; // Go ahead and set this so it doesn't keep trying when the anim is not present
	m_IdealActivity = NewActivity;

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		pev->sequence		= iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
		SetYawSpeed();
	}
	else
	{
		// Not available try to get default anim
		pev->sequence		= 0;	// Set to the reset anim (if it's there)
	}
}

//=========================================================
// Get Schedule!
//=========================================================
Schedule_t *CHGrunt :: GetSchedule( void )
{

	// clear old sentence
	m_iSentence = HGRUNT_SENT_NONE;
	KillLaserSpot();

	// flying? If PRONE, barnacle has me. IF not, it's assumed I am rapelling.
	if ( pev->movetype == MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
	{
		if ( ( pev->flags & FL_ONGROUND ) || pev->groundentity || pev->velocity.z > 0 )
		{
			// just landed
			pev->movetype = MOVETYPE_STEP;
			return GetScheduleOfType ( SCHED_GRUNT_REPEL_LAND );
		}

		if ( m_MonsterState != MONSTERSTATE_COMBAT || ( pev->weapons & ( HGRUNT_RPG | HGRUNT_SNIPER | HGRUNT_SHOTGUN ) ) )
			return GetScheduleOfType( SCHED_GRUNT_REPEL );

		return GetScheduleOfType ( SCHED_GRUNT_REPEL_ATTACK );
	}

	// grunts place HIGH priority on running away from danger sounds.
	if ( HasConditions(bits_COND_HEAR_SOUND) )
	{
		CSound *pSound = PBestSound();
		if ( pSound)
		{
			if (pSound->m_iType & bits_SOUND_DANGER)
			{
				// dangerous sound nearby!

				//!!!KELLY - currently, this is the grunt's signal that a grenade has landed nearby,
				// and the grunt should find cover from the blast
				// good place for "SHIT!" or some other colorful verbal indicator of dismay.
				// It's not safe to play a verbal order here "Scatter", etc cause
				// this may only affect a single individual in a squad.

				if (FOkToSpeak())
				{
					SENTENCEG_PlayRndSz( edict(), "HG_GREN", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
					JustSpoke();
				}

				MakeIdealYaw( pSound->m_vecOrigin );
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
			}

			if ( !HasConditions( bits_COND_SEE_ENEMY ) && ( pSound->m_iType & ( bits_SOUND_COMBAT | bits_SOUND_PLAYER ) ) )
				MakeIdealYaw( pSound->m_vecOrigin );
		}
	}

	if ( m_MonsterState != MONSTERSTATE_COMBAT )
		return CBaseMonster::GetSchedule();

	if ( m_hGuardEnt != NULL )
	{
		if ( m_hTargetEnt != m_hGuardEnt )
			StartPlayerFollowing( m_hGuardEnt, false );

		if ( m_hTargetEnt != NULL && ( Center() - m_hTargetEnt->Center() ).Length2D() > 256 )
			return GetScheduleOfType( SCHED_TARGET_PLAYERCHASE_FORCE );
	}

	if ( HasConditions( bits_COND_ENEMY_DEAD ) )
		return CBaseMonster :: GetSchedule();

	if ( HasConditions(bits_COND_NEW_ENEMY) )
			{
				if ( InSquad() )
				{
					MySquadLeader()->m_fEnemyEluded = FALSE;

					if ( !IsLeader() )
					{
				if ( m_fFirstEncounter )
				{
					m_fFirstEncounter = FALSE;
					return GetScheduleOfType ( SCHED_TAKE_COVER_FROM_ENEMY );
					}

				if ( ( pev->weapons & HGRUNT_RPG ) && FCanCheckAttacks() )
				{
					if ( ( pev->weapons & HGRUNT_HANDGRENADE ) && HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) )
						return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

					return GetScheduleOfType( SCHED_FIND_ATTACK_POINT );
				}

				return GetScheduleOfType( SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE );
			}

			if (FOkToSpeak())// && RANDOM_LONG(0,1))
						{
							if ( m_hEnemy != NULL && m_hEnemy->IsPlayer())
								// player
								SENTENCEG_PlayRndSz( edict(), "HG_ALERT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
							else if ( m_hEnemy != NULL
				          && ( m_fPlayerAlly || m_hEnemy->Classify() != CLASS_PLAYER_ALLY) && m_hEnemy->Classify() != CLASS_HUMAN_PASSIVE && m_hEnemy->Classify() != CLASS_MACHINE)
					SENTENCEG_PlayRndSz( edict(), "HG_MONST", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);

							JustSpoke();
						}

						if ( HasConditions ( bits_COND_NO_AMMO_LOADED ) )
				return GetScheduleOfType( SCHED_GRUNT_COVER_AND_RELOAD );

			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
				return GetScheduleOfType ( SCHED_GRUNT_SUPPRESS );

			if ( ( pev->weapons & HGRUNT_RPG ) && FCanCheckAttacks() )
			{
				if ( ( pev->weapons & HGRUNT_HANDGRENADE ) && HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) )
					return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

				return GetScheduleOfType( SCHED_FIND_ATTACK_POINT );
			}

			return GetScheduleOfType ( SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE );
						}
					}
	else
	{
		if ( HasConditions ( bits_COND_NO_AMMO_LOADED ) )
			return GetScheduleOfType ( SCHED_GRUNT_COVER_AND_RELOAD );

		if ( HasConditions( bits_COND_LIGHT_DAMAGE ) )
			{
			if ( RANDOM_LONG(0,99) > 90 || m_hEnemy == NULL )
				return GetScheduleOfType( SCHED_SMALL_FLINCH );

			if (FOkToSpeak()) // && RANDOM_LONG(0,1))
					{
				SENTENCEG_PlayRndSz( edict(), "HG_COVER", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
				m_iSentence = HGRUNT_SENT_COVER;
				JustSpoke();
						//JustSpoke();
					}
					return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
				}

		if ( HasConditions ( bits_COND_CAN_MELEE_ATTACK1 ) )
			return GetScheduleOfType ( SCHED_MELEE_ATTACK1 );

		if ( ( pev->weapons & HGRUNT_GRENADELAUNCHER) && HasConditions ( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
			return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

		if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
			{
				if ( InSquad() && MySquadLeader()->m_fEnemyEluded && !HasConditions ( bits_COND_ENEMY_FACING_ME ) )
					{
						MySquadLeader()->m_fEnemyEluded = FALSE;
						return GetScheduleOfType ( SCHED_GRUNT_FOUND_ENEMY );
					}

			if ( !OccupySlot ( bits_SLOTS_HGRUNT_ENGAGE ) )
				{
				if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
					return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

				if ( !( pev->weapons & HGRUNT_SNIPER ) || !HasConditions( bits_COND_SEE_ENEMY ) )
					return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
				}

			return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
		}

		if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) )
			{
				if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
				{
					//!!!KELLY - this grunt is about to throw or fire a grenade at the player. Great place for "fire in the hole"  "frag out" etc
					if (FOkToSpeak())
					{
						SENTENCEG_PlayRndSz( edict(), "HG_THROW", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
						JustSpoke();
					}
					return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
				}

			if ( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) && RANDOM_LONG( 0, 1 ) )
			{
				if ( !( pev->weapons & HGRUNT_SNIPER ) || RANDOM_LONG( 0, 5 ) <= 1 )
				{
					//!!!KELLY - grunt cannot see the enemy and has just decided to
					// charge the enemy's position.
					if (FOkToSpeak())
						m_iSentence = HGRUNT_SENT_CHARGE;

					if ( ( pev->weapons & HGRUNT_RPG ) && FCanCheckAttacks() )
					{
						if ( ( pev->weapons & HGRUNT_HANDGRENADE ) && HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) )
							return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

						return GetScheduleOfType( SCHED_FIND_ATTACK_POINT );
						//JustSpoke();
					}

					return GetScheduleOfType( SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE );
				}
			}
			else if (FOkToSpeak() && RANDOM_LONG(0,1))
					{
						SENTENCEG_PlayRndSz( edict(), "HG_TAUNT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
						JustSpoke();
					}
					return GetScheduleOfType( SCHED_STANDOFF );
				}
			}

			if ( HasConditions( bits_COND_SEE_FEAR ) && m_hEnemy != NULL )
	{
		CBaseMonster *enemy = m_hEnemy->MyMonsterPointer();

		if ( enemy )
		{
			if ( enemy->pev->flags & FL_CLIENT )
			{
				if ( enemy->IsFacing( pev, 0.9f ) )
					return GetScheduleOfType( SCHED_LOSE_ENEMY );
			}
			else if ( enemy->HasConditions( bits_COND_SEE_ENEMY ) && enemy->m_hEnemy == this )
				return GetScheduleOfType( SCHED_LOSE_ENEMY );
		}
	}

	if ( ( GetConditions() & ( bits_COND_SEE_ENEMY | bits_COND_CAN_RANGE_ATTACK1 ) ) == bits_COND_SEE_ENEMY )
	{
		m_iSentence = HGRUNT_SENT_TAUNT;

		if ( ( pev->weapons & HGRUNT_RPG ) && FCanCheckAttacks() )
		{
			if ( ( pev->weapons & HGRUNT_HANDGRENADE ) && HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

			return GetScheduleOfType( SCHED_FIND_ATTACK_POINT );
		}

		return GetScheduleOfType ( SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE );
			}

	return CBaseMonster:: GetSchedule();
}

//=========================================================
//=========================================================
Schedule_t* CHGrunt :: GetScheduleOfType ( int Type )
{
	switch	( Type )
	{
	case SCHED_COMBAT_FACE:
			return slGruntCombatFace;

		case SCHED_VICTORY_DANCE:
			return !InSquad() || IsLeader() ? slGruntVictoryDance : slGruntFail;

		case SCHED_TAKE_COVER_FROM_ENEMY:
			if ( RANDOM_LONG( 0, 1 ) && InSquad() )
			{
				if ( sk_grunt_handgrenades.value >= 1 && HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
				{
					if (FOkToSpeak())
					{
						SENTENCEG_PlayRndSz( edict(), "HG_THROW", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
						JustSpoke();
					}
					return slGruntTossGrenadeCover;
				}

				return slGruntTakeCover;
			}

			if ( RANDOM_LONG(0,1) || m_hEnemy == NULL || m_hEnemy->pev->movetype == MOVETYPE_FLY || HasConditions( bits_COND_ENEMY_OCCLUDED ) )
				return slGruntTakeCover;

			if ( ( m_hEnemy->Center() - pev->origin ).Length2D() >= 512 )
				return slGruntTakeCover;

			if ( SquadMemberInRange( pev->origin, 256 ) )
				return slGruntTakeCover;

			return slGruntGrenadeCover;

		case SCHED_TAKE_COVER_FROM_BEST_SOUND:
			return slGruntTakeCoverFromBestSound;
	case SCHED_RANGE_ATTACK1:
			if ( pev->weapons & HGRUNT_RPG )
				return slGruntRangeAttack1Rocket;

			if ( pev->weapons & HGRUNT_SNIPER )
				return RANDOM_LONG(0,9) > 0 ? slGruntRangeAttack1C : slGruntRangeAttack1D;

			if ( m_fStanding )
				return slGruntRangeAttack1B;

			return slGruntRangeAttack1A;

		case SCHED_RANGE_ATTACK2:
			return slGruntRangeAttack2;

		case SCHED_FAIL:
			return m_hEnemy != NULL ? slGruntCombatFail : slGruntFail;

		case SCHED_GRUNT_SUPPRESS:
			if ( pev->weapons & HGRUNT_RPG )
				return slGruntRangeAttack1Rocket;

			if ( m_hEnemy == NULL || !m_hEnemy->IsPlayer() || !m_fFirstEncounter )
				return slGruntSuppress;

			m_fFirstEncounter = FALSE;// after first encounter, leader won't issue handsigns anymore when he has a new enemy
				return slGruntSignalSuppress;

		case SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE:
			return slGruntEstablishLineOfFire;

		case SCHED_GRUNT_COVER_AND_RELOAD:
			return slGruntHideReload;

		case SCHED_GRUNT_SWEEP:
			return slGruntSweep;

		case SCHED_GRUNT_FOUND_ENEMY:
			return slGruntFoundEnemy;

		case SCHED_GRUNT_REPEL:
			if (pev->velocity.z > -128)
				pev->velocity.z -= 32;
			return slGruntRepel;

		case SCHED_GRUNT_REPEL_ATTACK:
			if (pev->velocity.z > -128)
				pev->velocity.z -= 32;
			return slGruntRepelAttack;

		case SCHED_GRUNT_REPEL_LAND:
			return slGruntRepelLand;

		case SCHED_GRUNT_WAIT_FACE_ENEMY:
			return slGruntWaitInCover;

		case SCHED_GRUNT_TAKECOVER_FAILED:
			if ( ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) && OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
			     || ( m_hEnemy != NULL && m_hEnemy->pev->movetype == MOVETYPE_FLY
			          && HasConditions( bits_COND_SEE_ENEMY | bits_COND_CAN_RANGE_ATTACK1 ) ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			return GetScheduleOfType( SCHED_FAIL );

		case SCHED_GRUNT_ELOF_FAIL:
			if ( m_hEnemy != NULL && m_hEnemy->pev->movetype == MOVETYPE_FLY && HasConditions( bits_COND_SEE_ENEMY | bits_COND_CAN_RANGE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			return GetScheduleOfType( SCHED_FIND_ATTACK_POINT );

		default:
			return CBaseMonster:: GetScheduleOfType ( Type );
		}
	}


int CHGrunt::ObjectCaps( void )
{
	return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

BOOL CHGrunt::CheckRangeAttack1Rocket( float distance, CBaseEntity *target )
{
	if ( !target )
		return FALSE;

	Vector source = pev->origin + Vector( 0, 0, 48 );
	TraceResult trace;

	if ( m_flRPGSafetyRadius > distance )
		return FALSE;

	CBaseEntity *entity = NULL;

	while ( ( entity = UTIL_FindEntityInSphere( entity, m_vecEnemyLKP, m_flRPGSafetyRadius ) ) != NULL )
	{
		if ( ( entity->pev->flags & FL_MONSTER ) && IRelationship( entity ) <= R_NO
		     && entity->IsAlive() && FVisible( entity, false ) )
			return FALSE;
	}

	UTIL_TraceLine( source, target->Center(), missile, ignore_glass, edict(), &trace );
	return trace.flFraction == 1.0f || trace.pHit == target->edict();
}

BOOL CHGrunt::CheckRangeAttack1Sniper( float distance, CBaseEntity *target )
{
	if ( !( distance > 64 ) )
		return FALSE;

	Vector source = GetGunPosition();
	TraceResult trace;

	UTIL_TraceLine( source, target->BodyTarget( source ), ignore_monsters, ignore_glass, edict(), &trace );
	return trace.flFraction == 1.0f || trace.pHit == target->edict();
}

CBaseEntity *CHGrunt::BestVisibleEnemy( void )
{
	if ( !( pev->weapons & HGRUNT_RPG ) )
		return CBaseMonster::BestVisibleEnemy();

	CBaseEntity *best = NULL;
	CBaseEntity *entity = m_pLink;
	int bestRelationship = R_NO;
	int nearest = 65536;

	while ( entity )
	{
		if ( entity->IsAlive() || entity->IsBSPModel() )
		{
			if ( IRelationship( entity ) > bestRelationship || IRelationship( entity ) == R_FR )
			{
				bestRelationship = IRelationship( entity );
				nearest = (int) ( entity->Center() - Center() ).Length();
				best = entity;
			}
			else if ( IRelationship( entity ) == bestRelationship && IRelationship( entity ) > R_NO )
			{
				int distance = (int) ( entity->Center() - Center() ).Length();

				if ( distance < nearest || CheckRangeAttack1Rocket( (float) distance, entity ) )
				{
					nearest = distance;
					best = entity;
					bestRelationship = IRelationship( entity );
				}
			}
		}

		entity = entity->m_pLink;
	}

	return best;
}

void CHGrunt::KillLaserSpot( void )
{
	if ( m_hRPGLaserSpot != NULL )
	{
		UTIL_Remove( m_hRPGLaserSpot );
		m_hRPGLaserSpot = NULL;
	}
}

void CHGrunt::UpdateSpot( bool instant )
{
	if ( m_hRPGLaserSpot == NULL || m_hEnemy == NULL )
		return;

	Vector source = GetGunPosition();

	m_vecRPGAimAngles = UTIL_VecToAngles( m_hEnemy->Center() - source );

	if ( instant )
		m_vecRPGCurrentAngles = m_vecRPGAimAngles;
	else
	{
		m_vecRPGCurrentAngles.x = UTIL_ApproachAngle( m_vecRPGAimAngles.x, m_vecRPGCurrentAngles.x, m_flFrameTime * 25 );
		m_vecRPGCurrentAngles.y = UTIL_ApproachAngle( m_vecRPGAimAngles.y, m_vecRPGCurrentAngles.y, m_flFrameTime * 25 );
		m_vecRPGCurrentAngles.z = UTIL_ApproachAngle( m_vecRPGAimAngles.z, m_vecRPGCurrentAngles.z, m_flFrameTime * 25 );
	}

	UTIL_MakeVectors( Vector( -m_vecRPGCurrentAngles.x, m_vecRPGCurrentAngles.y, m_vecRPGCurrentAngles.z ) );

	Vector end = source + gpGlobals->v_forward * 4096;
	TraceResult trace;

	UTIL_TraceLine( source, end, dont_ignore_monsters, edict(), &trace );
	UTIL_SetOrigin( m_hRPGLaserSpot->pev, trace.vecEndPos );
	SetBlending( 0, m_vecRPGCurrentAngles.x );
}

//=========================================================
// CHGruntRepel - when triggered, spawns a monster_human_grunt
// repelling down a line.
//=========================================================

class CHGruntRepel : public CBaseMonster
{
public:
	float GetPointsForDamage( float flDamage )
	{
		//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
		return 0;
	}

	void Spawn( void );
	void Precache( void );
	void EXPORT RepelUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int m_iSpriteTexture;	// Don't save, precache
};

LINK_ENTITY_TO_CLASS( monster_grunt_repel, CHGruntRepel );

void CHGruntRepel::Spawn( void )
{
	Precache( );
	pev->solid = SOLID_NOT;

	if ( FStringNull( pev->targetname ) )
		RepelUse( NULL, NULL, USE_TOGGLE, 0 );
	else
		SetUse( &CHGruntRepel::RepelUse );
}

void CHGruntRepel::Precache( void )
{
	CBaseEntity::Precache();

	HumanGruntRepelPrecache( this, "monster_human_grunt",
	                         IsPlayerAlly() ? "models/hgruntf.mdl" : "models/hgrunt.mdl", false, m_iSpriteTexture );
}

void CHGruntRepel::RepelUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	HumanGruntRepelUse( this, "monster_human_grunt" );
}



//=========================================================
// DEAD HGRUNT PROP
//=========================================================
class CDeadHGrunt : public CBaseMonster
{
public:
	void Spawn( void );
	int Classify( void ) { return GetClassification( CLASS_HUMAN_MILITARY ); }

	void KeyValue( KeyValueData *pkvd );

	int	m_iPose;// which sequence to display	-- temporary, don't need to save
	static const char *m_szPoses[3];
};

const char *CDeadHGrunt::m_szPoses[] = { "deadstomach", "deadside", "deadsitting" };

void CDeadHGrunt::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "pose"))
	{
		m_iPose = atoi(pkvd->szValue);

		if ( m_iPose < 0 )
			m_iPose = 0;
		else if ( m_iPose > 2 )
			m_iPose = 2;

		pkvd->fHandled = TRUE;
	}
	else 
		CBaseMonster::KeyValue( pkvd );
}

LINK_ENTITY_TO_CLASS( monster_hgrunt_dead, CDeadHGrunt );

//=========================================================
// ********** DeadHGrunt SPAWN **********
//=========================================================
void SpawnDeadHumanGrunt( CBaseMonster *corpse, int &pose );

void CDeadHGrunt :: Spawn( void )
{
	SpawnDeadHumanGrunt( this, m_iPose );
	}
