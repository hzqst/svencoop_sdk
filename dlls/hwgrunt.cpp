#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "animation.h"
#include "hwgrunt.h"
#include "weapons.h"
#include "soundent.h"
#include "talkmonster.h"
#include "server_sound.h"

extern cvar_t sk_hwgrunt_health;
extern cvar_t sk_hwgrunt_minipellets;
extern Schedule_t slAlertFace[];

#define HWGRUNT_AE_SHOOT		1
#define HWGRUNT_AE_RELOAD		2
#define HWGRUNT_AE_DROP_MINIGUN	11
#define HWGRUNT_AE_PICKUP_DONE	12

#define HWGRUNT_NODROP			256		// pev->weapons: the minigun stays with the corpse
#define HWGRUNT_GUN_GROUP		1

enum
{
	HWGRUNT_ARMED_MINIGUN,
	HWGRUNT_SIDEARM_GLOCK,
	HWGRUNT_SIDEARM_DEAGLE,
	HWGRUNT_SIDEARM_357,
	HWGRUNT_ARMED_NONE
};

namespace
{
enum
{
	TASK_HWGRUNT_REPEL = LAST_COMMON_TASK - 1,
	TASK_HWGRUNT_SHOOT = LAST_COMMON_TASK + 1,
	TASK_HWGRUNT_GET_PATH_TO_MINIGUN,
	TASK_HWGRUNT_PICKUP_MINIGUN
};

enum
{
	SCHED_HWGRUNT_REPEL = LAST_COMMON_SCHEDULE + 1,
	SCHED_HWGRUNT_REPEL_LAND,
	SCHED_HWGRUNT_RELOAD
};

const char *g_HWGruntPainSounds[] = {
	"hgrunt/gr_pain5.wav", "hgrunt/gr_pain4.wav", "hgrunt/gr_pain3.wav",
	"hgrunt/gr_pain2.wav", "hgrunt/gr_pain1.wav"
};

const char *g_HWGruntShootSounds[] = { "minigun/hw_shoot1.wav" };

const char *g_HWGruntSentences[] = {
	"HG_GREN", "HG_ALERT", "HG_MONSTER", "HG_COVER",
	"HG_THROW", "HG_CHARGE", "HG_TAUNT", "HG_FLOF"
};

Task_t tlHWGruntRangeAttack1[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_HWGRUNT_SHOOT, 0 }
};

Schedule_t slHWGruntRangeAttack1[] = {
	{
		tlHWGruntRangeAttack1,
		ARRAYSIZE( tlHWGruntRangeAttack1 ),
		0,
		0,
		"HWGrunt Primary Range Attack1"
	}
};

Task_t tlHWGruntSidearmAttack[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RANGE_ATTACK1, 0 }
};

Schedule_t slHWGruntSidearmAttack[] = {
	{
		tlHWGruntSidearmAttack,
		ARRAYSIZE( tlHWGruntSidearmAttack ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD,
		bits_SOUND_DANGER,
		"HWGrunt Secondary Range Attack1"
	}
};

Task_t tlHWGruntPickupGun[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, SCHED_SMALL_FLINCH },
	{ TASK_HWGRUNT_GET_PATH_TO_MINIGUN, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_HWGRUNT_PICKUP_MINIGUN, 0 }
};

Schedule_t slHWGruntPickupGun[] = {
	{
		tlHWGruntPickupGun,
		ARRAYSIZE( tlHWGruntPickupGun ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"HWGrunt Pickup Gun"
	}
};

Task_t tlHWGruntReload[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, SCHED_RELOAD },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_REMEMBER, bits_MEMORY_INCOVER },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_RELOAD, 0 },
	{ TASK_FACE_ENEMY, 0 }
};

Schedule_t slHWGruntReload[] = {
	{
		tlHWGruntReload,
		ARRAYSIZE( tlHWGruntReload ),
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"HWGrunt Reload"
	}
};

Task_t tlHWGruntRepel[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_HWGRUNT_REPEL, 0 }
};

Schedule_t slHWGruntRepel[] = {
	{
		tlHWGruntRepel,
		ARRAYSIZE( tlHWGruntRepel ),
		bits_COND_SEE_ENEMY |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_COMBAT |
		bits_SOUND_PLAYER |
		bits_SOUND_DANGER,
		"Repel"
	}
};

Task_t tlHWGruntRepelLand[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_PLAY_SEQUENCE, ACT_LAND },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_CLEAR_LASTPOSITION, 0 }
};

Schedule_t slHWGruntRepelLand[] = {
	{
		tlHWGruntRepelLand,
		ARRAYSIZE( tlHWGruntRepelLand ),
		bits_COND_SEE_ENEMY |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_COMBAT |
		bits_SOUND_PLAYER |
		bits_SOUND_DANGER,
		"Repel Land"
	}
};

Task_t tlHWGruntVictoryDance[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, ACT_IDLE },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_CLEAR_LASTPOSITION, 0 },
	{ TASK_STORE_LASTPOSITION, 0 },
	{ TASK_WAIT, 1.5f },
	{ TASK_GET_PATH_TO_ENEMY_CORPSE, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_WAIT, 4 },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 }
};

Schedule_t slHWGruntVictoryDance[] = {
	{
		tlHWGruntVictoryDance,
		ARRAYSIZE( tlHWGruntVictoryDance ),
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntVictoryDance"
	}
};

CBaseEntity *HWGruntEntity( EHANDLE &handle )
{
	edict_t *entity = handle.Get();

	if ( !entity || entity->free || !GET_PRIVATE( entity ) )
		return NULL;

	return CBaseEntity::Instance( entity );
}
}

LINK_ENTITY_TO_CLASS( monster_hwgrunt, CHWGrunt );

Schedule_t *CHWGrunt::m_scheduleList[] = {
	slHWGruntRangeAttack1,
	slHWGruntSidearmAttack,
	slHWGruntRepel,
	slHWGruntRepelLand
};

void CHWGrunt::OnCreate( void )
{
	m_iArmedWeapon = HWGRUNT_ARMED_MINIGUN;
	m_fDisableMinigunDrop = false;
	m_ShootingState = HWGRUNT::SHOOTING_IDLE;
	CSquadMonster::OnCreate();
}

void CHWGrunt::KeyValue( KeyValueData *data )
{
	if ( FStrEq( data->szKeyName, "disable_minigun_drop" ) )
	{
		m_fDisableMinigunDrop = atoi( data->szValue ) == 1;
		data->fHandled = TRUE;
	}
	else
		CSquadMonster::KeyValue( data );
}

int CHWGrunt::Classify( void )
{
	return GetClassification( CLASS_HUMAN_MILITARY );
}

int CHWGrunt::ObjectCaps( void )
{
	return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

int CHWGrunt::ISoundMask( void )
{
	return bits_SOUND_WORLD | bits_SOUND_COMBAT | bits_SOUND_PLAYER | bits_SOUND_DANGER;
}

float CHWGrunt::GetPointsForDamage( float damage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	float points = pev->max_health / sk_hwgrunt_health.value;
	float fraction = damage / pev->max_health;

	points *= 10.0f;
	return points * fraction;
}

void CHWGrunt::SetupFriendly( void )
{
	UTIL_SetModel( this, IsPlayerAlly() ? "models/hwgruntf.mdl" : "models/hwgrunt.mdl" );
}

void CHWGrunt::Precache( void )
{
	CSquadMonster::Precache();

	if ( !pev->model )
		UTIL_PrecacheModel( this, IsPlayerAlly() ? "models/hwgruntf.mdl" : "models/hwgrunt.mdl" );

	m_iBrassShell = UTIL_PrecacheModel( this, "models/shell.mdl" );
	PrecacheSound_Intercept( this, g_HWGruntShootSounds[0] );

	for ( int i = 0; i < ARRAYSIZE( g_HWGruntPainSounds ); i++ )
		PrecacheSound_Intercept( this, g_HWGruntPainSounds[i] );

	PrecacheSound_Intercept( this, "hgrunt/gr_die1.wav" );
	PrecacheSound_Intercept( this, "hgrunt/gr_die2.wav" );
	PrecacheSound_Intercept( this, "hgrunt/gr_die3.wav" );
	PrecacheSound_Intercept( this, "hassault/hw_spinup.wav" );
	PrecacheSound_Intercept( this, "hassault/hw_spindown.wav" );
	PrecacheSound_Intercept( this, "hassault/hw_spin.wav" );
	PrecacheSound_Intercept( this, "hgrunt/yessir.wav" );
	PrecacheSound_Intercept( this, "weapons/pl_gun3.wav" );
	PrecacheSound_Intercept( this, "otis/ot_attack.wav" );
	PrecacheSound_Intercept( this, "weapons/357_shot1.wav" );

	if ( RANDOM_LONG( 0, 1 ) )
		m_voicePitch = 92 + RANDOM_LONG( 0, 7 );
	else
		m_voicePitch = 95;
}

void CHWGrunt::Spawn( void )
{
	Precache();

	if ( !SetupModel() )
		SetupFriendly();

	if ( pev->scale <= 0 )
		pev->scale = 1;

	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );
	pev->effects = 0;
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->view_ofs = Vector( 0, 0, 28 );

	if ( pev->health == 0 )
		pev->health = sk_hwgrunt_health.value;

	m_bloodColor = BLOOD_COLOR_RED;
	m_flFieldOfView = -0.7f;
	m_MonsterState = MONSTERSTATE_NONE;
	m_afCapability = bits_CAP_SQUAD | bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;
	m_fPickupAnimationDone = false;
	m_vecGunReference = pev->origin;
	m_flNextShot = gpGlobals->time;
	m_flNextSpinAnimation = gpGlobals->time;
	m_flSidearmCooldown = gpGlobals->time;
	m_flSpawnAttackTime = gpGlobals->time;
	m_vecGunOffset = Vector( 8, 16, 36 );
	m_iAmmo = 6;
	m_iClipSize = 6;
	SetFiringState( HWGRUNT::SHOOTING_IDLE );
	m_fHasPistolSequence = LookupSequence( "pistol_shoot" ) != -1;

	if ( pev->weapons == HWGRUNT_NODROP )
	{
		m_fDisableMinigunDrop = true;
		pev->weapons = HWGRUNT_SIDEARM_GLOCK;
	}
	else if ( pev->weapons < 0 || pev->weapons > HWGRUNT_ARMED_NONE )
		pev->weapons = 0;

	if ( HWGruntEntity( m_hMinigun ) || m_iArmedWeapon > 0 )
		SetArmedWeapon( m_iSidearm );
	else if ( pev->weapons >= 0 && pev->weapons <= 3 )
	{
		m_iArmedWeapon = HWGRUNT_ARMED_MINIGUN;
		SetBodygroup( HWGRUNT_GUN_GROUP, 0 );
		m_iUnknown694 = 0;
		m_iSidearm = pev->weapons ? pev->weapons : RANDOM_LONG( HWGRUNT_SIDEARM_GLOCK, HWGRUNT_SIDEARM_357 );
	}

	switch ( m_iSidearm )
	{
		case HWGRUNT_SIDEARM_GLOCK:
			m_iAmmo = m_iClipSize = 17;
			break;
		case HWGRUNT_SIDEARM_DEAGLE:
			m_iAmmo = m_iClipSize = 7;
			break;
		case HWGRUNT_SIDEARM_357:
			m_iAmmo = m_iClipSize = 6;
			break;
	}

	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( "Heavy Weapons Grunt" );

	MonsterInit();

	if ( IsPlayerAlly() && !( pev->spawnflags & SF_MONSTER_PREDISASTER ) )
	{
		SetUse( &CBaseMonster::CallFollowerPlayerUse );
		m_fCanPlayerFollow = true;
	}
}

void CHWGrunt::SetArmedWeapon( int weapon )
{
	if ( m_fHasPistolSequence )
	{
		if ( weapon < 0 || weapon > HWGRUNT_ARMED_NONE )
			return;

		m_iArmedWeapon = weapon;
		SetBodygroup( HWGRUNT_GUN_GROUP, weapon );
	}
	else
	{
		m_iArmedWeapon = weapon ? HWGRUNT_ARMED_NONE : HWGRUNT_ARMED_MINIGUN;
		SetBodygroup( HWGRUNT_GUN_GROUP, weapon ? 1 : 0 );
	}
}

void CHWGrunt::SetFiringState( HWGRUNT::ShootingStates state )
{
	m_ShootingState = state;
	m_flThinkDelay = state == HWGRUNT::SHOOTING_FIRING || state == HWGRUNT::SHOOTING_SUPPRESSING ? 0.0f : 0.1f;
}

void CHWGrunt::CheckAmmo( void )
{
	if ( m_iAmmo <= 0 )
		SetConditions( bits_COND_NO_AMMO_LOADED );
}

void CHWGrunt::SetYawSpeed( void )
{
	pev->yaw_speed = m_Activity == ACT_FLY || m_Activity == ACT_GLIDE ? 150 : 270;
}

int CHWGrunt::IgnoreConditions( void )
{
	int conditions = CSquadMonster::IgnoreConditions();

	if ( ( m_Activity == ACT_SMALL_FLINCH || m_Activity == ACT_BIG_FLINCH ) && gpGlobals->time > m_flNextFlinch )
		m_flNextFlinch = gpGlobals->time + 6;

	return conditions;
}

void CHWGrunt::RunAI( void )
{
	if ( pev->waterlevel == 3 && gpGlobals->time > pev->pain_finished )
	{
		TakeDamage( VARS( INDEXENT( 0 ) ), VARS( INDEXENT( 0 ) ), 10, DMG_DROWN );
		pev->pain_finished = gpGlobals->time + 1;
	}

	CSquadMonster::RunAI();
}

bool CHWGrunt::FOkToSpeak( void )
{
	if ( !IsAlive() || CTalkMonster::g_talkWaitTime > gpGlobals->time )
		return false;

	return !( pev->spawnflags & SF_MONSTER_GAG ) || m_MonsterState == MONSTERSTATE_COMBAT;
}

void CHWGrunt::JustSpoke( void )
{
	CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT( 1.5f, 2.0f );
	m_iSentence = -1;
}

void CHWGrunt::SpeakSentence( void )
{
	if ( m_iSentence == -1 || !IsAlive() || !( CTalkMonster::g_talkWaitTime <= gpGlobals->time )
		|| ( ( pev->spawnflags & SF_MONSTER_GAG ) && m_MonsterState != MONSTERSTATE_COMBAT ) )
		return;

	CServer_SoundEngine::GetSoundEngine()->PlaySentenceGroup( edict(), g_HWGruntSentences[m_iSentence],
		0.35f, ATTN_NORM, 0, m_voicePitch, -1 );
	JustSpoke();
}

void CHWGrunt::IdleSound( void )
{
	if ( !IsAlive() || !( CTalkMonster::g_talkWaitTime <= gpGlobals->time )
		|| ( ( pev->spawnflags & SF_MONSTER_GAG ) && m_MonsterState != MONSTERSTATE_COMBAT )
		|| ( !m_iQuestion && !RANDOM_LONG( 0, 1 ) ) )
		return;

	const char *sentence = NULL;
	int question = m_iQuestion;
	bool changeQuestion = false;

	if ( !m_iQuestion )
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
			case 0:
				sentence = "HG_CHECK";
				question = 1;
				changeQuestion = true;
				break;
			case 1:
				sentence = "HG_QUEST";
				question = 2;
				changeQuestion = true;
				break;
			case 2:
				sentence = "HG_IDLE";
				break;
		}
	}
	else
	{
		if ( m_iQuestion == 1 )
			sentence = "HG_CLEAR";
		else if ( m_iQuestion == 2 )
			sentence = "HG_ANSWER";

		question = 0;
		changeQuestion = true;
	}

	if ( sentence )
		CServer_SoundEngine::GetSoundEngine()->PlaySentenceGroup( edict(), sentence, 0.35f, ATTN_NORM, 0, m_voicePitch, -1 );

	if ( changeQuestion )
		m_iQuestion = question;

	JustSpoke();
}

void CHWGrunt::AlertSound( void )
{
	CServer_SoundEngine::GetSoundEngine()->PlaySentenceGroup( edict(), "HG_ALERT", 0.35f, ATTN_NORM, 0, m_voicePitch, -1 );
	JustSpoke();
}

void CHWGrunt::AttackSound( void )
{
	int pitch = RANDOM_LONG( -5, 5 ) + 100;
	int sound = RANDOM_LONG( 0, -1 );

	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, g_HWGruntShootSounds[sound], 1, ATTN_NORM, 0, pitch );
}

void CHWGrunt::PainSound( void )
{
	int pitch = 95 + RANDOM_LONG( 0, 9 );

	if ( RANDOM_LONG( 0, 5 ) < 2 )
		EMIT_SOUND_DYN( edict(), CHAN_VOICE, g_HWGruntPainSounds[RANDOM_LONG( 0, 4 )], 1, ATTN_NORM, 0, pitch );
}

void CHWGrunt::DeathSound( void )
{
	int pitch = 95 + RANDOM_LONG( 0, 9 );
	int sound = RANDOM_LONG( 0, 4 );

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, g_HWGruntPainSounds[sound], 1, ATTN_NORM, 0, pitch );
}

void CHWGrunt::UseSound( void )
{
	switch ( RANDOM_LONG( 0, 1 ) )
	{
		case 0:
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, "hgrunt/yessir.wav", 0.8f, ATTN_NORM, 0, m_voicePitch );
			break;
		case 1:
			PlaySentence( "!HG_ANSWER0", 4, 1, ATTN_NORM );
			break;
	}

	JustSpoke();
}

void CHWGrunt::UnUseSound( void )
{
	switch ( RANDOM_LONG( 0, 1 ) )
	{
		case 0:
			PlaySentence( "!HG_TAUNT1", 4, 1, ATTN_NORM );
			break;
		case 1:
			PlaySentence( "!HG_QUEST2", 4, 1, ATTN_NORM );
			break;
	}

	JustSpoke();
}

CBaseEntity *CHWGrunt::FindWeapon( void )
{
	CBaseEntity *weapon = NULL;

	while ( ( weapon = UTIL_FindEntityByClassname( weapon, "weapon_minigun" ) ) != NULL )
	{
		if ( !FVisible( weapon->pev->origin ) || ( weapon->pev->effects & EF_NODRAW ) )
			continue;

		if ( !weapon->pev->owner || !FClassnameIs( weapon->pev->owner, "player" ) )
			return weapon;
	}

	return NULL;
}

bool CHWGrunt::FacingShootingIdeal( void )
{
	return fabs( FlYawDiff() ) <= 20.0f;
}

bool CHWGrunt::CheckMinigunAttack( CBaseEntity *enemy )
{
	if ( !enemy )
		return false;

	if ( !( pev->flags & FL_ONGROUND ) && pev->velocity.z > 48 )
		return false;

	Vector source = pev->origin + Vector( 0, 0, pev->scale * m_vecGunOffset.z );
	Vector target = enemy->BodyTarget( source );
	TraceResult trace;

	UTIL_TraceLine( source, target, ignore_monsters, edict(), &trace );

	if ( trace.fStartSolid )
		return false;

	return trace.flFraction == 1.0f || trace.pHit == enemy->edict();
}

bool CHWGrunt::CheckSidearmAttackFrom( const Vector &source )
{
	CBaseEntity *enemy = HWGruntEntity( m_hEnemy.m_Handle );
	Vector target = enemy->BodyTarget( source );
	TraceResult trace;

	UTIL_TraceLine( source, target, ignore_monsters, ignore_glass, edict(), &trace );
	return trace.flFraction == 1.0f || trace.pHit == HWGruntEntity( m_hEnemy.m_Handle )->edict();
}

BOOL CHWGrunt::CheckRangeAttack1( float dot, float distance )
{
	if ( !HWGruntEntity( m_hEnemy.m_Handle ) )
		return FALSE;

	if ( m_iArmedWeapon == HWGRUNT_ARMED_MINIGUN )
	{
		if ( pev->waterlevel != 3 && m_flNextShot <= gpGlobals->time && distance <= 2048 && NoFriendlyFire() )
			return CheckMinigunAttack( HWGruntEntity( m_hEnemy.m_Handle ) );

		return FALSE;
	}

	if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) || distance > 2048 || dot < 0.5f || !NoFriendlyFire() )
		return FALSE;

	Vector standingSource = pev->origin + Vector( 0, 0, 68 ) * pev->scale;
	Vector crouchingSource = pev->origin + Vector( 0, 0, 48 ) * pev->scale;

	if ( RANDOM_LONG( 0, 9 ) == 0 && m_Activity != ACT_RANGE_ATTACK1 )
		m_fStanding = RANDOM_LONG( 0, 1 ) != 0;

	if ( m_fStanding )
	{
		if ( CheckSidearmAttackFrom( standingSource ) )
			return TRUE;

		if ( CheckSidearmAttackFrom( crouchingSource ) )
		{
			m_fStanding = false;
			return TRUE;
		}
	}
	else
	{
		if ( CheckSidearmAttackFrom( crouchingSource ) )
			return TRUE;

		if ( CheckSidearmAttackFrom( standingSource ) )
		{
			m_fStanding = true;
			return TRUE;
		}
	}

	return FALSE;
}

CBaseEntity *CHWGrunt::BestVisibleEnemySpecial( void )
{
	float nearest = 65536.0f;
	int relationship = 0;
	int waterlevel = 2;
	CBaseEntity *best = NULL;
	CBaseEntity *candidate = HWGruntEntity( m_pLink );

	while ( candidate )
	{
		if ( !candidate->IsBSPModel() && candidate->pev->waterlevel <= waterlevel
			&& CheckMinigunAttack( candidate ) && NoFriendlyFire( pev->origin, candidate ) )
		{
			int candidateRelationship = IRelationship( candidate, false );

			if ( candidateRelationship > relationship )
			{
				relationship = candidateRelationship;
				nearest = ( candidate->pev->origin - pev->origin ).Length();
				waterlevel = candidate->pev->waterlevel;
				best = candidate;
			}
			else if ( candidateRelationship == relationship && candidateRelationship > 0 )
			{
				float distance = ( candidate->pev->origin - pev->origin ).Length();

				if ( nearest > distance )
				{
					nearest = distance;
					waterlevel = candidate->pev->waterlevel;
					best = candidate;
				}
			}
		}

		candidate = HWGruntEntity( candidate->m_pLink );
	}

	return best;
}

Schedule_t *CHWGrunt::ScheduleFromName( const char *name )
{
	Schedule_t *schedule = ScheduleInList( name, m_scheduleList, ARRAYSIZE( m_scheduleList ) );

	return schedule ? schedule : CBaseMonster::ScheduleFromName( name );
}

Schedule_t *CHWGrunt::GetScheduleOfType( int type )
{
	switch ( type )
	{
		case SCHED_VICTORY_DANCE:
			if ( HWGruntEntity( m_hSquadLeader ) && HWGruntEntity( m_hSquadLeader ) != this )
				return slAlertFace;

			return slHWGruntVictoryDance;
		case SCHED_RANGE_ATTACK1:
			return m_iArmedWeapon ? slHWGruntSidearmAttack : slHWGruntRangeAttack1;
		case SCHED_HWGRUNT_REPEL:
			if ( pev->velocity.z > -128 )
				pev->velocity.z -= 32;

			return slHWGruntRepel;
		case SCHED_HWGRUNT_REPEL_LAND:
			return slHWGruntRepelLand;
		case SCHED_HWGRUNT_RELOAD:
			return slHWGruntReload;
		default:
			return CBaseMonster::GetScheduleOfType( type );
	}
}

Schedule_t *CHWGrunt::GetSchedule( void )
{
	if ( pev->movetype == MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
	{
		if ( !( pev->flags & FL_ONGROUND ) && !pev->groundentity && pev->velocity.z <= 0 )
			return GetScheduleOfType( SCHED_HWGRUNT_REPEL );

		pev->movetype = MOVETYPE_STEP;
		return GetScheduleOfType( SCHED_HWGRUNT_REPEL_LAND );
	}

	if ( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		CSound *sound = PBestSound();

		if ( sound )
		{
			if ( sound->m_iType & bits_SOUND_DANGER )
			{
				if ( FOkToSpeak() )
				{
					CServer_SoundEngine::GetSoundEngine()->PlaySentenceGroup( edict(), "HG_GREN", 0.35f, ATTN_NORM, 0, m_voicePitch, -1 );
					JustSpoke();
				}

				MakeIdealYaw( sound->m_vecOrigin );
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
			}

			if ( !HasConditions( bits_COND_SEE_ENEMY ) && ( sound->m_iType & ( bits_SOUND_WORLD | bits_SOUND_PLAYER ) ) )
				MakeIdealYaw( sound->m_vecOrigin );
		}
	}

	switch ( m_MonsterState )
	{
		case MONSTERSTATE_COMBAT:
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
				return CSquadMonster::GetSchedule();

			if ( m_iArmedWeapon == HWGRUNT_ARMED_MINIGUN )
			{
				if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
					return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

				return CSquadMonster::GetSchedule();
			}

			if ( m_iArmedWeapon == HWGRUNT_ARMED_NONE )
			{
				if ( !HWGruntEntity( m_hMinigun ) )
					return CSquadMonster::GetSchedule();
			}
			else
			{
				if ( m_flSidearmCooldown > gpGlobals->time )
					return GetScheduleOfType( pev->flags & FL_ONGROUND ? SCHED_TAKE_COVER_FROM_ENEMY : SCHED_IDLE_STAND );

				if ( HasConditions( bits_COND_NO_AMMO_LOADED ) )
					return GetScheduleOfType( SCHED_HWGRUNT_RELOAD );

				if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
					return slHWGruntSidearmAttack;

				CBaseEntity *weapon = HWGruntEntity( m_hMinigun );

				if ( !HasConditions( bits_COND_ENEMY_OCCLUDED ) || !weapon )
					return CSquadMonster::GetSchedule();

				if ( HWGruntEntity( m_hEnemy.m_Handle ) && ( pev->origin - m_vecEnemyLKP ).Length() <= ( pev->origin - weapon->Center() ).Length() )
					return CSquadMonster::GetSchedule();
			}

			return slHWGruntPickupGun;
		case MONSTERSTATE_ALERT:
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
				return GetScheduleOfType( SCHED_VICTORY_DANCE );

			// fall through
		case MONSTERSTATE_IDLE:
			if ( HWGruntEntity( m_hMinigun ) )
				return slHWGruntPickupGun;

			if ( m_iArmedWeapon != HWGRUNT_ARMED_MINIGUN )
			{
				m_hMinigun = FindWeapon();

				if ( m_iArmedWeapon != HWGRUNT_ARMED_MINIGUN && m_iAmmo != m_iClipSize )
					return GetScheduleOfType( SCHED_HWGRUNT_RELOAD );
			}

			return CSquadMonster::GetSchedule();
		default:
			return CSquadMonster::GetSchedule();
	}
}

void CHWGrunt::SetActivity( Activity activity )
{
	int sequence;

	switch ( activity )
	{
		case ACT_IDLE:
		case ACT_TURN_LEFT:
		case ACT_TURN_RIGHT:
			sequence = m_iArmedWeapon && m_fHasPistolSequence ? LookupSequence( "pistol_idle" ) : LookupActivity( activity );
			break;
		case ACT_WALK:
			sequence = LookupSequence( m_iArmedWeapon && m_fHasPistolSequence ? "pistol_walk" : "creeping_walk" );
			break;
		case ACT_RUN:
			sequence = LookupSequence( m_iArmedWeapon && m_fHasPistolSequence ? "pistol_run" : "run" );
			break;
		case ACT_RANGE_ATTACK1:
			if ( m_iArmedWeapon )
				sequence = LookupSequence( m_fStanding && m_fHasPistolSequence ? "pistol_shoot" : "pistol_crouchshoot" );
			else
				sequence = LookupSequence( "attack" );

			break;
		default:
			sequence = LookupActivity( activity );
			break;
	}

	m_Activity = activity;
	m_IdealActivity = activity;

	if ( sequence < 0 )
		pev->sequence = 0;
	else
	{
		if ( pev->sequence != sequence || !m_fSequenceLoops )
			pev->frame = 0;

		pev->sequence = sequence;
		ResetSequenceInfo();
		SetYawSpeed();
	}

	switch ( m_Activity )
	{
		case ACT_DIESIMPLE:
		case ACT_DIEBACKWARD:
		case ACT_DIEFORWARD:
		case ACT_DIEVIOLENT:
		case ACT_DIE_HEADSHOT:
		case ACT_DIE_CHESTSHOT:
		case ACT_DIE_GUTSHOT:
		case ACT_DIE_BACKSHOT:
		{
			Vector angles;

			GetAttachment( 1, m_vecGunReference, angles );
			break;
		}
	}
}

void CHWGrunt::StartTask( Task_t *task )
{
	switch ( task->iTask )
	{
		case TASK_FACE_IDEAL:
		case TASK_FACE_ENEMY:
			CSquadMonster::StartTask( task );

			if ( pev->movetype == MOVETYPE_FLY )
				m_IdealActivity = ACT_GLIDE;

			break;
		case TASK_HWGRUNT_SHOOT:
			if ( m_iArmedWeapon )
				TaskFail();
			else
				SetFiringState( HWGRUNT::SHOOTING_IDLE );

			break;
		case TASK_HWGRUNT_GET_PATH_TO_MINIGUN:
		{
			CBaseEntity *weapon = HWGruntEntity( m_hMinigun );

			if ( !weapon || ( weapon->pev->owner && weapon->pev->owner != edict() )
				|| weapon->pev->movetype == MOVETYPE_FOLLOW || weapon->pev->effects == EF_NODRAW )
				m_hMinigun = FindWeapon();

			weapon = HWGruntEntity( m_hMinigun );

			if ( !weapon || !MoveToLocation( ACT_RUN, 1, weapon->Center(), 32 ) )
				TaskFail();
			else
				TaskComplete();

			break;
		}
		case TASK_HWGRUNT_PICKUP_MINIGUN:
		{
			CBaseEntity *weapon = HWGruntEntity( m_hMinigun );

			if ( !weapon )
				break;

			if ( weapon->pev->movetype == MOVETYPE_FOLLOW )
			{
				TaskFail();
				break;
			}

			if ( ( weapon->Center() - Center() ).Length2D() <= 64 )
			{
				if ( LookupSequence( "pickup_minigun" ) == -1 )
					SetSequenceByName( "small_pain2" );
				else
				{
					m_fPickupAnimationDone = false;
					SetSequenceByName( "pickup_minigun" );
				}
			}
			else
				ChangeSchedule( slHWGruntPickupGun );

			break;
		}
		default:
			CSquadMonster::StartTask( task );
			break;
	}
}

void CHWGrunt::RunTask( Task_t *task )
{
	if ( task->iTask == TASK_HWGRUNT_SHOOT )
	{
		ExecuteShootTask();
		return;
	}

	if ( task->iTask != TASK_HWGRUNT_PICKUP_MINIGUN )
	{
		m_flThinkDelay = 0.1f;
		CSquadMonster::RunTask( task );
		return;
	}

	CBaseEntity *weapon = HWGruntEntity( m_hMinigun );

	if ( !weapon || ( weapon->pev->owner && FClassnameIs( weapon->pev->owner, "player" ) ) || ( weapon->pev->effects & EF_NODRAW ) )
	{
		TaskFail();
		return;
	}

	if ( m_fSequenceFinished || m_fPickupAnimationDone )
	{
		if ( FClassnameIs( weapon->pev, "weaponbox" ) )
			weapon->CriticalRemove();
		else
			UTIL_Remove( weapon );

		m_iArmedWeapon = HWGRUNT_ARMED_MINIGUN;
		SetBodygroup( HWGRUNT_GUN_GROUP, 0 );
		m_hMinigun = NULL;
		TaskComplete();
	}
}

void CHWGrunt::UpdateGunAngles( const Vector &target )
{
	Vector direction = target - ( pev->origin + Vector( 0, 0, m_vecGunOffset.z ) );
	Vector angles = UTIL_VecToAngles( direction );
	float pitch = UTIL_ApproachAngle( angles.x, m_vecGunAngles.x, 3 );

	m_vecGunAngles = Vector( pitch, pitch, pitch );
	SetBoneController( 0, pitch );
}

void CHWGrunt::SetShootEnemy( CBaseEntity *enemy )
{
	PushEnemy( HWGruntEntity( m_hEnemy.m_Handle ), m_vecSuppressionTarget );
	m_hEnemy = enemy;
	m_vecSuppressionTarget = HWGruntEntity( m_hEnemy.m_Handle )->BodyTarget( pev->origin );
	m_vecEnemyLKP = m_vecSuppressionTarget;
}

void CHWGrunt::SpinDown( const char *sequence )
{
	SetFiringState( HWGRUNT::SHOOTING_SPINDOWN );
	SetSequenceByName( sequence );
	EMIT_SOUND_DYN( edict(), CHAN_ITEM, "hassault/hw_spindown.wav", 1, ATTN_NORM, 0, RANDOM_LONG( -5, 5 ) + 100 );
}

bool CHWGrunt::UpdateShootTarget( bool grounded, CBaseEntity *&enemy, bool canFire )
{
	if ( grounded )
	{
		Look( (int) m_flDistLook );
		enemy = BestVisibleEnemySpecial();

		if ( enemy )
		{
			if ( enemy != HWGruntEntity( m_hEnemy.m_Handle ) )
				SetShootEnemy( enemy );

			return true;
		}
	}

	SetFiringState( HWGRUNT::SHOOTING_SPINDOWN );

	if ( !canFire )
	{
		m_iSentence = 7;
		SpeakSentence();
	}

	CBaseEntity *currentEnemy = HWGruntEntity( m_hEnemy.m_Handle );

	if ( currentEnemy && !currentEnemy->IsBSPModel() && grounded && canFire )
	{
		SetSequenceByName( "small_pain" );
		m_fSequenceFinished = TRUE;
	}
	else
		SetSequenceByName( "spindown" );

	EMIT_SOUND_DYN( edict(), CHAN_ITEM, "hassault/hw_spindown.wav", 1, ATTN_NORM, 0, RANDOM_LONG( -5, 5 ) + 100 );
	m_flNextLookTime = gpGlobals->time;
	return false;
}

void CHWGrunt::ExecuteShootTask( void )
{
	if ( m_iArmedWeapon )
	{
		TaskFail();
		return;
	}

	if ( !FacingIdeal() )
	{
		float yaw = FlYawDiff();

		if ( yaw > -45 || LookupActivity( ACT_TURN_RIGHT ) == ACTIVITY_NOT_AVAILABLE )
		{
			if ( yaw <= 45 || LookupActivity( ACT_TURN_LEFT ) == ACTIVITY_NOT_AVAILABLE )
				ChangeYaw( 120 );
			else
			{
				m_IdealActivity = ACT_TURN_LEFT;
				ChangeYaw( 250 );
			}
		}
		else
		{
			m_IdealActivity = ACT_TURN_RIGHT;
			ChangeYaw( 250 );
		}
	}

	bool grounded = ( pev->flags & FL_ONGROUND ) && pev->velocity.z < 48;

	switch ( m_ShootingState )
	{
		case HWGRUNT::SHOOTING_IDLE:
			if ( !grounded )
			{
				TaskFail();
				return;
			}

			SetSequenceByName( "spinup" );
			EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "hassault/hw_spinup.wav", 1, ATTN_NORM, 0, RANDOM_LONG( -5, 5 ) + 100 );
			SetFiringState( HWGRUNT::SHOOTING_SPINUP );
			m_flNextShot = gpGlobals->time + 1;
			return;
		case HWGRUNT::SHOOTING_SPINUP:
			if ( HasConditions( bits_COND_SEE_ENEMY ) )
			{
				m_vecEnemyLKP = HWGruntEntity( m_hEnemy.m_Handle )->BodyTarget( pev->origin );
				m_vecSuppressionTarget = m_vecEnemyLKP;
				m_flLastEnemySightTime = gpGlobals->time;
				MakeIdealYaw( m_vecSuppressionTarget );
			}

			UpdateGunAngles( m_vecEnemyLKP );

			if ( gpGlobals->time >= m_flNextShot )
			{
				SetFiringState( HWGRUNT::SHOOTING_FIRING );
				m_flNextLookTime = gpGlobals->time;
			}

			return;
		case HWGRUNT::SHOOTING_FIRING:
		{
			CBaseEntity *newEnemy = NULL;

			if ( gpGlobals->time >= m_flNextLookTime )
			{
				Look( (int) m_flDistLook );
				newEnemy = BestVisibleEnemySpecial();

				if ( !newEnemy )
				{
					CBaseEntity *enemy = HWGruntEntity( m_hEnemy.m_Handle );

					if ( !enemy || enemy->IsBSPModel() || !grounded )
					{
						SetSequenceByName( "spindown" );
						EMIT_SOUND_DYN( edict(), CHAN_ITEM, "hassault/hw_spindown.wav", 1, ATTN_NORM, 0, RANDOM_LONG( -5, 5 ) + 100 );
						SetFiringState( HWGRUNT::SHOOTING_SPINDOWN );
						return;
					}
				}
				else if ( newEnemy != HWGruntEntity( m_hEnemy.m_Handle ) )
				{
					SetShootEnemy( newEnemy );
					m_iSentence = 1;
					SpeakSentence();
				}

				m_flNextLookTime = gpGlobals->time + 1;
			}

			bool canFire = NoFriendlyFire();
			bool canAttack = CheckMinigunAttack( HWGruntEntity( m_hEnemy.m_Handle ) );
			CBaseEntity *enemy = HWGruntEntity( m_hEnemy.m_Handle );

			if ( !enemy || enemy->IsBSPModel() || !canFire || !grounded )
			{
				UpdateShootTarget( grounded, newEnemy, canFire );
				return;
			}

			if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) )
			{
				m_vecSuppressionTarget = m_vecEnemyLKP;
				SetFiringState( HWGRUNT::SHOOTING_SUPPRESSING );
				m_flNextLookTime = gpGlobals->time;
				return;
			}

			m_vecEnemyLKP = enemy->BodyTarget( pev->origin );
			m_vecSuppressionTarget = m_vecEnemyLKP;
			m_flLastEnemySightTime = gpGlobals->time;
			MakeIdealYaw( m_vecSuppressionTarget );

			if ( !canAttack )
			{
				UpdateShootTarget( grounded, newEnemy, canFire );
				return;
			}

			UpdateGunAngles( m_vecEnemyLKP );

			if ( gpGlobals->time >= m_flNextShot )
			{
				if ( FacingShootingIdeal() )
				{
					Shoot( g_vecZero );
					m_IdealActivity = ACT_RANGE_ATTACK1;
					m_flNextShot = gpGlobals->time + 0.07;
					m_flNextSpinAnimation = gpGlobals->time;
				}
				else if ( gpGlobals->time >= m_flNextSpinAnimation )
				{
					if ( m_IdealActivity != ACT_TURN_LEFT && m_IdealActivity != ACT_TURN_RIGHT )
						SetSequenceByName( "spindown" );

					m_flNextSpinAnimation = gpGlobals->time + 0.4f;
				}
			}

			return;
		}
		case HWGRUNT::SHOOTING_SUPPRESSING:
		{
			CBaseEntity *newEnemy = NULL;

			if ( gpGlobals->time >= m_flNextLookTime )
			{
				Look( (int) m_flDistLook );
				newEnemy = BestVisibleEnemySpecial();
				m_flNextLookTime = gpGlobals->time + 0.5f;

				if ( newEnemy && grounded )
				{
					if ( newEnemy == HWGruntEntity( m_hEnemy.m_Handle ) )
						m_iSentence = 6;
					else
					{
						PushEnemy( HWGruntEntity( m_hEnemy.m_Handle ), m_vecSuppressionTarget );
						m_iSentence = 1;
						m_hEnemy = newEnemy;
					}

					SpeakSentence();
					m_vecEnemyLKP = HWGruntEntity( m_hEnemy.m_Handle )->BodyTarget( pev->origin );
					m_vecSuppressionTarget = m_vecEnemyLKP;
					SetFiringState( HWGRUNT::SHOOTING_FIRING );
					return;
				}
			}

			if ( HasConditions( bits_COND_SEE_ENEMY ) )
			{
				CBaseEntity *enemy = HWGruntEntity( m_hEnemy.m_Handle );

				if ( !newEnemy && enemy && CheckMinigunAttack( enemy ) )
				{
					m_iSentence = 6;
					SpeakSentence();
					m_vecEnemyLKP = HWGruntEntity( m_hEnemy.m_Handle )->BodyTarget( pev->origin );
					m_vecSuppressionTarget = m_vecEnemyLKP;
					SetFiringState( HWGRUNT::SHOOTING_FIRING );
					return;
				}

				SpinDown( "small_pain" );
				return;
			}

			if ( !grounded || !NoFriendlyFireToPos( m_vecSuppressionTarget ) || gpGlobals->time - m_flLastEnemySightTime > 4 )
			{
				SpinDown( "spindown" );
				return;
			}

			MakeIdealYaw( m_vecSuppressionTarget );
			UpdateGunAngles( m_vecSuppressionTarget );

			if ( gpGlobals->time >= m_flNextShot && FacingShootingIdeal() )
			{
				Shoot( m_vecSuppressionTarget );
				m_IdealActivity = ACT_RANGE_ATTACK1;
				m_flNextShot = gpGlobals->time + 0.06f;
			}

			return;
		}
		case HWGRUNT::SHOOTING_SPINDOWN:
			if ( gpGlobals->time >= m_flNextLookTime )
			{
				Look( (int) m_flDistLook );
				CBaseEntity *newEnemy = BestVisibleEnemySpecial();

				if ( newEnemy )
				{
					if ( newEnemy != HWGruntEntity( m_hEnemy.m_Handle ) )
					{
						SetShootEnemy( newEnemy );
						m_iSentence = 1;
						SpeakSentence();
					}

					if ( CheckMinigunAttack( HWGruntEntity( m_hEnemy.m_Handle ) ) )
						TaskComplete();
				}

				m_flNextLookTime = gpGlobals->time + 2;
			}

			if ( HWGruntEntity( m_hEnemy.m_Handle ) && HasConditions( bits_COND_SEE_ENEMY ) )
			{
				m_vecEnemyLKP = HWGruntEntity( m_hEnemy.m_Handle )->BodyTarget( pev->origin );
				m_vecSuppressionTarget = m_vecEnemyLKP;
				m_flLastEnemySightTime = gpGlobals->time;
				MakeIdealYaw( m_vecSuppressionTarget );
			}

			if ( m_fSequenceFinished )
				TaskComplete();

			return;
	}
}

void CHWGrunt::Shoot( Vector target )
{
	if ( !HWGruntEntity( m_hEnemy.m_Handle ) )
		return;

	Vector angles;

	if ( target == g_vecZero )
	{
		angles = UTIL_VecToAngles( HWGruntEntity( m_hEnemy.m_Handle )->Center() - Center() );
		angles.x = -angles.x;
	}
	else
		angles = UTIL_VecToAngles( target - Center() );

	UTIL_MakeVectors( angles );

	Vector source;

	if ( m_iArmedWeapon )
		source = pev->origin + Vector( 0, 0, m_fStanding ? 68 : 48 ) * pev->scale;
	else
		source = pev->origin + Vector( 0, 0, pev->scale * m_vecGunOffset.z );

	Vector shellOrigin = g_vecZero;
	Vector shellAngles = g_vecZero;

	GetAttachment( 1, shellOrigin, shellAngles );
	pev->effects |= EF_MUZZLEFLASH;

	if ( target == g_vecZero )
	{
		if ( ( source - HWGruntEntity( m_hEnemy.m_Handle )->Center() ).Length() >= 96 )
			target = HWGruntEntity( m_hEnemy.m_Handle )->BodyTarget( source );
		else
			target = HWGruntEntity( m_hEnemy.m_Handle )->Center();
	}

	Vector direction = ( target - source ).Normalize();
	bool ejectShell = false;

	switch ( m_iArmedWeapon )
	{
		case 0:
			FireBullets( (ULONG) sk_hwgrunt_minipellets.value, source, direction, VECTOR_CONE_4DEGREES,
				4096, BULLET_MONSTER_12MM, 4, -1, NULL, FBDM_DRAW );
			AttackSound();
			CSoundEnt::InsertSound( bits_SOUND_WORLD, pev->origin, 1536, 0.3f, this );
			CSoundEnt::InsertSound( bits_SOUND_DANGER, target, 64, 0.1f, this );
			ejectShell = true;
			break;
		case 1:
			FireBullets( 1, source, direction, VECTOR_CONE_2DEGREES, 4096, BULLET_MONSTER_9MM, 0, -1, NULL, FBDM_DRAW );
			EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "weapons/pl_gun3.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			CSoundEnt::InsertSound( bits_SOUND_WORLD, pev->origin, 1024, 0.3f, this );
			m_iAmmo--;
			pev->framerate = RANDOM_LONG( 4, 6 );
			ejectShell = true;
			break;
		case 2:
			FireBullets( 1, source, direction, VECTOR_CONE_4DEGREES, 4096, BULLET_MONSTER_EAGLE, 0, -1, NULL, FBDM_DRAW );
			EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "otis/ot_attack.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			CSoundEnt::InsertSound( bits_SOUND_WORLD, pev->origin, 1024, 0.3f, this );
			m_iAmmo--;
			pev->framerate = RANDOM_LONG( 2, 3 );
			ejectShell = true;
			break;
		case 3:
			FireBullets( 1, source, direction, VECTOR_CONE_2DEGREES, 4096, BULLET_PLAYER_357, 0, -1, NULL, FBDM_DRAW );
			EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "weapons/357_shot1.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			CSoundEnt::InsertSound( bits_SOUND_WORLD, pev->origin, 1024, 0.3f, this );
			m_iAmmo--;
			pev->framerate = RANDOM_LONG( 1, 2 );
			break;
	}

	if ( ejectShell )
	{
		float rightSpeed = RANDOM_FLOAT( 40, 90 );
		float upSpeed = RANDOM_FLOAT( 75, 200 );
		float forwardSpeed = RANDOM_FLOAT( -40, 40 );
		Vector velocity = gpGlobals->v_up * upSpeed + gpGlobals->v_right * rightSpeed + gpGlobals->v_forward * forwardSpeed;

		EjectBrass( shellOrigin, velocity, pev->angles.y, m_iBrassShell, 1 );
	}
}

void CHWGrunt::DropMinigun( float angleDirection )
{
	CBaseEntity *weapon = DropWeapon( "weapon_minigun", true );

	if ( !weapon )
		return;

	Vector position = g_vecZero;
	Vector angles = g_vecZero;
	TraceResult trace;

	GetAttachment( 1, position, angles );
	UTIL_TraceLine( pev->origin, position, ignore_monsters, dont_ignore_glass, edict(), &trace );
	DROP_TO_FLOOR( weapon->edict() );

	Vector displacement = position - m_vecGunReference;
	Vector direction = displacement.Normalize();
	float speed = displacement.Length() * 10;

	if ( speed >= 300 )
		speed = 300;

	weapon->pev->velocity = direction * speed;
	weapon->pev->angles = g_vecZero;
	weapon->pev->angles.y = pev->angles.y;
	weapon->pev->angles.y = UTIL_AngleMod( weapon->pev->angles.y + angleDirection * 90 );
	weapon->pev->angles.x = UTIL_AngleMod( weapon->pev->angles.x + angleDirection * 45 );

	float yawSign = RANDOM_LONG( 0, 1 ) ? -1.0f : 1.0f;
	float yawSpeed = RANDOM_FLOAT( 100, 400 ) * yawSign;
	float pitchSign = RANDOM_LONG( 0, 1 ) ? -1.0f : 1.0f;
	float pitchSpeed = RANDOM_FLOAT( 100, 400 ) * pitchSign;

	weapon->pev->avelocity = Vector( pitchSpeed, yawSpeed, 0 );
	m_iArmedWeapon = HWGRUNT_ARMED_NONE;
	SetBodygroup( HWGRUNT_GUN_GROUP, m_fHasPistolSequence ? 4 : 1 );
	m_hMinigun = weapon;
}

void CHWGrunt::HandleAnimEvent( MonsterEvent_t *event )
{
	switch ( event->event )
	{
		case HWGRUNT_AE_SHOOT:
			Shoot( g_vecZero );
			break;
		case HWGRUNT_AE_RELOAD:
			EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "hgrunt/gr_reload1.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			ClearConditions( bits_COND_NO_AMMO_LOADED );
			m_iAmmo = m_iClipSize;
			break;
		case HWGRUNT_AE_DROP_MINIGUN:
			if ( !m_fDisableMinigunDrop && m_iArmedWeapon == HWGRUNT_ARMED_MINIGUN )
				DropMinigun( 1 );

			break;
		case HWGRUNT_AE_PICKUP_DONE:
			m_fPickupAnimationDone = true;
			break;
		default:
			CSquadMonster::HandleAnimEvent( event );
			break;
	}
}

void CHWGrunt::GibMonster( void )
{
	if ( !m_fDisableMinigunDrop && m_iArmedWeapon == HWGRUNT_ARMED_MINIGUN )
	{
		Vector position = g_vecZero;
		Vector angles = g_vecZero;

		GetAttachment( 1, position, angles );
		CBaseEntity *weapon = DropItem( "weapon_minigun", position + Vector( 0, 0, 16 ), pev->angles );

		if ( weapon )
			weapon->pev->spawnflags |= SF_NORESPAWN;
	}

	CSquadMonster::GibMonster();
}

void CHWGrunt::Killed( entvars_t *attacker, int gib )
{
	if ( !m_fHasPistolSequence && !m_fDisableMinigunDrop && m_iArmedWeapon == HWGRUNT_ARMED_MINIGUN )
		DropMinigun( -1 );

	CSquadMonster::Killed( attacker, gib );
}

int CHWGrunt::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	edict_t *attackerEdict = attacker ? ENT( attacker ) : NULL;

	if ( !attackerEdict )
		attackerEdict = INDEXENT( 0 );

	CBaseEntity *attackerEntity = attackerEdict && !attackerEdict->free ? CBaseEntity::Instance( attackerEdict ) : NULL;

	if ( attackerEntity && CheckAttacker( attackerEntity ) )
		return 0;

	if ( ( damageType & DMG_BLAST ) && m_iArmedWeapon == HWGRUNT_ARMED_MINIGUN && m_fHasPistolSequence && !m_fDisableMinigunDrop
		&& damage > 50 && RANDOM_LONG( 0, 10 ) < 10 )
	{
		Vector position = g_vecZero;
		Vector angles = g_vecZero;

		GetAttachment( 1, position, angles );
		CBaseEntity *weapon = DropItem( "weapon_minigun", position + Vector( 0, 0, 16 ), pev->angles );

		if ( weapon )
		{
			weapon->pev->spawnflags |= SF_NORESPAWN;
			SetArmedWeapon( m_iSidearm );
			m_hMinigun = weapon;
			TaskFail();
			m_flSidearmCooldown = gpGlobals->time + 3;
		}
	}

	return CSquadMonster::TakeDamage( inflictor, attacker, damage, damageType );
}

void CHWGrunt::TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType )
{
	if ( trace->iHitgroup == HITGROUP_CHEST || trace->iHitgroup == HITGROUP_STOMACH )
	{
		if ( damageType & ( DMG_SNIPER | DMG_BLAST | DMG_SLASH | DMG_BULLET ) )
		{
			if ( damage >= 2 )
				damage -= 2;

			damage *= 0.5f;
		}
	}
	else if ( trace->iHitgroup == 11 )
	{
		if ( damageType & ( DMG_SNIPER | DMG_SLASH | DMG_BULLET ) )
			UTIL_Ricochet( trace->vecEndPos, 1 );

		damage = fminf( 0.01f, damage );
	}

	CSquadMonster::TraceAttack( attacker, damage, direction, trace, damageType );
}
