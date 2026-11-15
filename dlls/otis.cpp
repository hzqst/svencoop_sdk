#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "otis.h"
#include "schedule.h"
#include "defaultai.h"
#include "weapons.h"
#include "soundent.h"

extern cvar_t sk_otis_health;

enum
{
	SCHED_OTIS_RELOAD = LAST_TALKMONSTER_SCHEDULE + 1,
};

LINK_ENTITY_TO_CLASS( monster_otis, COtis );

TYPEDESCRIPTION COtis::m_SaveData[] =
{
	DEFINE_FIELD( COtis, m_fGunDrawn, FIELD_CHARACTER ),
	DEFINE_FIELD( COtis, m_painTime, FIELD_TIME ),
	DEFINE_FIELD( COtis, m_head, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( COtis, CTalkMonster );

Task_t tlOtisFollow[] =
{
	{ TASK_MOVE_TO_TARGET_RANGE, 128 },
	{ TASK_SET_SCHEDULE, SCHED_TARGET_FACE },
};

Schedule_t slOtisFollow[] =
{
	{
		tlOtisFollow,
		ARRAYSIZE( tlOtisFollow ),
		bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"Follow"
	},
};

Task_t tlOtisEnemyDraw[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, ACT_ARM },
};

Schedule_t slOtisEnemyDraw[] =
{
	{
		tlOtisEnemyDraw,
		ARRAYSIZE( tlOtisEnemyDraw ),
		0,
		0,
		"Otis Enemy Draw"
	},
};

Task_t tlOtisFaceTarget[] =
{
	{ TASK_SET_ACTIVITY, ACT_IDLE },
	{ TASK_FACE_TARGET, 0 },
	{ TASK_SET_ACTIVITY, ACT_IDLE },
	{ TASK_SET_SCHEDULE, SCHED_TARGET_CHASE },
};

Schedule_t slOtisFaceTarget[] =
{
	{
		tlOtisFaceTarget,
		ARRAYSIZE( tlOtisFaceTarget ),
		bits_COND_CLIENT_PUSH | bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE
			| bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"FaceTarget"
	},
};

Task_t tlOtisIdleStand[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, ACT_IDLE },
	{ TASK_WAIT, 2 },
	{ TASK_TLK_HEADRESET, 0 },
};

Schedule_t slOtisIdleStand[] =
{
	{
		tlOtisIdleStand,
		ARRAYSIZE( tlOtisIdleStand ),
		bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE
			| bits_COND_HEAR_SOUND | bits_COND_SMELL,
		bits_SOUND_COMBAT | bits_SOUND_DANGER | bits_SOUND_MEAT | bits_SOUND_CARCASS | bits_SOUND_GARBAGE,
		"IdleStand"
	},
};

Task_t tlOtisReload[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, SCHED_RELOAD },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_REMEMBER, bits_MEMORY_INCOVER },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_RELOAD, 0 },
	{ TASK_FACE_ENEMY, 0 },
};

Schedule_t slOtisReload[] =
{
	{
		tlOtisReload,
		ARRAYSIZE( tlOtisReload ),
		bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"Barney Reload"
	},
};

Task_t tlOtisReloadQuick[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_RELOAD, 0 },
	{ TASK_FACE_ENEMY, 0 },
};

Schedule_t slOtisReloadQuick[] =
{
	{
		tlOtisReloadQuick,
		ARRAYSIZE( tlOtisReloadQuick ),
		bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"Barney Reload Quick"
	},
};

DEFINE_CUSTOM_SCHEDULES( COtis )
{
	slOtisFollow,
	slOtisEnemyDraw,
	slOtisFaceTarget,
	slOtisIdleStand,
	slOtisReload,
	slOtisReloadQuick,
};

IMPLEMENT_CUSTOM_SCHEDULES( COtis, CTalkMonster );

CBaseEntity *OtisHandleEntity( EHANDLE &handle )
{
	edict_t *entity = handle.Get();

	if ( !entity || entity->free )
		return NULL;

	return (CBaseEntity *) GET_PRIVATE( entity );
}

void COtis::Spawn( void )
{
	Precache();

	if ( !SetupModel() )
		SetupFriendly();

	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_RED;

	if ( pev->health == 0 )
		pev->health = sk_otis_health.value;

	pev->view_ofs = Vector( 0, 0, 50 );
	m_flFieldOfView = -0.7f;
	m_MonsterState = MONSTERSTATE_ALERT;
	pev->body = 0;
	m_cClipSize = 7;
	m_cAmmoLoaded = 7;
	m_fGunDrawn = false;
	m_afCapability = bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP | bits_CAP_USE_TANK;
	m_fCheckCreatureDanger = true;

	if ( m_head == 0 )
		SetBodygroup( 2, 1 );

	if ( m_head == 1 )
		SetBodygroup( 2, 0 );

	if ( m_head == -1 )
	{
		m_head = RANDOM_LONG( 0, 20 ) < 11 ? 1 : 0;
		SetBodygroup( 2, m_head == 1 ? 0 : 1 );
	}

	m_fGunDrawn = false;
	SetBodygroup( 1, 0 );

	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( IsPlayerAlly() ? "Otis" : "Otto" );

	MonsterInit();
	SetUse( &CBaseMonster::CallFollowerPlayerUse );
}

void COtis::Precache( void )
{
	TalkInit();
	CTalkMonster::Precache();

	if ( !pev->model )
		UTIL_PrecacheModel( this, IsPlayerAlly() ? "models/otis.mdl" : "models/otisf.mdl" );

	PrecacheSound_Intercept( this, "otis/ot_attack.wav" );
	PrecacheSound_Intercept( this, "barney/ba_pain1.wav" );
	PrecacheSound_Intercept( this, "barney/ba_pain2.wav" );
	PrecacheSound_Intercept( this, "barney/ba_pain3.wav" );
	PrecacheSound_Intercept( this, "barney/ba_die1.wav" );
	PrecacheSound_Intercept( this, "barney/ba_die2.wav" );
	PrecacheSound_Intercept( this, "barney/ba_die3.wav" );
	m_iShell = UTIL_PrecacheModel( this, "models/shell.mdl" );
}

void COtis::KeyValue( KeyValueData *data )
{
	if ( !strcmp( data->szKeyName, "head" ) )
	{
		m_head = atoi( data->szValue );
		data->fHandled = TRUE;
	}
	else
	{
		CTalkMonster::KeyValue( data );
	}
}

int COtis::Classify( void )
{
	return GetClassification( CLASS_HUMAN_MILITARY );
}

int COtis::ISoundMask( void )
{
	return 0xFF;
}

int COtis::ObjectCaps( void )
{
	return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

void COtis::SetPlayerAlly( bool state )
{
	SetPlayerAllyDirect( !state );
}

void COtis::SetupFriendly( void )
{
	SetModel_Intercept( edict(), IsPlayerAlly() ? "models/otis.mdl" : "models/otisf.mdl" );
}

void COtis::SetYawSpeed( void )
{
	pev->yaw_speed = 270;
}

void COtis::CheckAmmo( void )
{
	if ( m_cAmmoLoaded <= 0 )
		SetConditions( bits_COND_NO_AMMO_LOADED );
}

BOOL COtis::CheckRangeAttack1( float dot, float distance )
{
	if ( !( distance <= 2048 ) || !( dot >= 0.5f ) || !NoFriendlyFire() )
		return FALSE;

	TraceResult trace;
	Vector origin = pev->origin + Vector( 0, 0, 55 );
	CBaseEntity *enemy = OtisHandleEntity( m_hEnemy.m_Handle );
	Vector target = ( enemy->BodyTarget( origin ) - enemy->Center() ) + m_vecEnemyLKP;

	UTIL_TraceLine( origin, target, dont_ignore_monsters, edict(), &trace );

	return trace.flFraction == 1.0f || trace.pHit == enemy->edict();
}

void COtis::OtisFirePistol( void )
{
	Vector origin = pev->origin + Vector( 0, 0, 55 );
	Vector direction = ShootAtEnemy( origin );
	Vector angles = UTIL_VecToAngles( direction );

	FireBullets( 1, origin, direction, VECTOR_CONE_6DEGREES, 1024, BULLET_MONSTER_EAGLE );

	Vector shellVelocity = gpGlobals->v_right * RANDOM_FLOAT( 40, 90 );

	shellVelocity = shellVelocity + gpGlobals->v_up * RANDOM_FLOAT( 75, 200 );
	shellVelocity = shellVelocity + gpGlobals->v_forward * RANDOM_FLOAT( -40, 40 );
	EjectBrass( origin - direction * -17, shellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHELL );

	int pitch = RANDOM_LONG( 0, 20 );

	SetBlending( 0, angles.x );
	pev->effects = EF_MUZZLEFLASH;
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 0.3f, this );
	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "otis/ot_attack.wav", VOL_NORM, ATTN_NORM, 0,
	                pitch > 10 ? 100 : 92 + pitch, 0, false, g_vecZero );

	if ( pev->movetype != MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
		m_flAutomaticAttackTime = gpGlobals->time + RANDOM_FLOAT( 0.2f, 0.5f );

	m_cAmmoLoaded--;
}

void COtis::DeclineFollowing( void )
{
	PlaySentence( "OT_POK", 2, VOL_NORM, ATTN_NORM );
}

void COtis::AlertSound( void )
{
	if ( OtisHandleEntity( m_hEnemy.m_Handle ) && FOkToSpeak() )
		PlaySentence( "OT_ATTACK", RANDOM_FLOAT( 2.8f, 3.2f ), VOL_NORM, ATTN_NORM );
}

void COtis::PainSound( void )
{
	if ( m_painTime > gpGlobals->time )
		return;

	m_painTime = gpGlobals->time + RANDOM_FLOAT( 0.5f, 0.75f );

	const char *sounds[] = { "barney/ba_pain1.wav", "barney/ba_pain2.wav", "barney/ba_pain3.wav" };
	int sound = RANDOM_LONG( 0, 2 );

	if ( sound >= 0 && sound < ARRAYSIZE( sounds ) )
		EMIT_SOUND_DYN( edict(), CHAN_VOICE, sounds[sound], VOL_NORM, ATTN_NORM, 0, GetVoicePitch(), 0, false, g_vecZero );
}

void COtis::DeathSound( void )
{
	const char *sounds[] = { "barney/ba_die1.wav", "barney/ba_die2.wav", "barney/ba_die3.wav" };
	int sound = RANDOM_LONG( 0, 2 );

	if ( sound >= 0 && sound < ARRAYSIZE( sounds ) )
		EMIT_SOUND_DYN( edict(), CHAN_VOICE, sounds[sound], VOL_NORM, ATTN_NORM, 0, GetVoicePitch(), 0, false, g_vecZero );
}

void COtis::TalkInit( void )
{
	CTalkMonster::TalkInit();

	m_szGrp[TLK_ANSWER] = "OT_ANSWER";
	m_szGrp[TLK_QUESTION] = "OT_QUESTION";
	m_szGrp[TLK_IDLE] = "OT_IDLE";
	m_szGrp[TLK_STARE] = "OT_STARE";
	m_szGrp[TLK_USE] = "OT_OK";
	m_szGrp[TLK_UNUSE] = "OT_WAIT";
	m_szGrp[TLK_STOP] = "OT_STOP";
	m_szGrp[TLK_NOSHOOT] = IsPlayerAlly() ? "OT_SCARED" : NULL;
	m_szGrp[TLK_HELLO] = "OT_HELLO";
	m_szGrp[TLK_PHELLO] = NULL;
	m_szGrp[TLK_PIDLE] = NULL;
	m_szGrp[TLK_PQUESTION] = "OT_PQUEST";
	m_szGrp[TLK_PLHURT1] = "!OT_CUREA";
	m_szGrp[TLK_PLHURT2] = "!OT_CUREB";
	m_szGrp[TLK_PLHURT3] = "!OT_CUREC";
	m_szGrp[TLK_SMELL] = "OT_SMELL";
	m_szGrp[TLK_WOUND] = "OT_WOUND";
	m_szGrp[TLK_MORTAL] = "OT_MORTAL";
	m_voicePitch = 100;
}

void COtis::RunAI( void )
{
	pev->framerate = m_Activity == ACT_RUN ? 0.9f : 1.0f;
	CBaseMonster::RunAI();
}

void COtis::RunTask( Task_t *task )
{
	if ( task->iTask == TASK_RELOAD )
	{
		MakeIdealYaw( m_vecEnemyLKP );
		ChangeYaw( (int) pev->yaw_speed );

		if ( m_fSequenceFinished )
		{
			m_cAmmoLoaded = m_cClipSize;
			ClearConditions( bits_COND_NO_AMMO_LOADED );
			TaskComplete();
		}

		return;
	}

	if ( task->iTask == TASK_RANGE_ATTACK1 )
	{
		CBaseEntity *enemy = OtisHandleEntity( m_hEnemy.m_Handle );

		if ( enemy && enemy->IsPlayer() )
			pev->framerate = 1.5f;

		if ( !NoFriendlyFire() )
		{
			ChangeSchedule( GetScheduleOfType( SCHED_FIND_ATTACK_POINT ) );
			return;
		}
	}

	CTalkMonster::RunTask( task );
}

void COtis::HandleAnimEvent( MonsterEvent_t *event )
{
	switch ( event->event )
	{
		case 2:
			m_fGunDrawn = true;
			SetBodygroup( 1, 1 );
			break;

		case 3:
			if ( FOkToSpeak() )
				PlaySentence( "OT_ATTACK", RANDOM_FLOAT( 2.8f, 3.2f ), VOL_NORM, ATTN_NORM );

			OtisFirePistol();
			break;

		case 4:
			m_fGunDrawn = false;
			SetBodygroup( 1, 0 );
			break;

		default:
			CTalkMonster::HandleAnimEvent( event );
			break;
	}
}

int COtis::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	if ( !attacker )
		return 0;

	edict_t *attackerEdict = ENT( attacker );

	if ( !attackerEdict )
		attackerEdict = INDEXENT( 0 );

	CBaseEntity *attackerEntity = attackerEdict && !attackerEdict->free
		? (CBaseEntity *) GET_PRIVATE( attackerEdict ) : NULL;

	if ( CheckAttacker( attackerEntity ) )
		return 0;

	int result = CTalkMonster::TakeDamage( inflictor, attacker, damage, damageType );

	if ( ( !IsAlive() || pev->deadflag == DEAD_DYING ) && !IsPlayerAlly() )
		return result;

	if ( m_MonsterState == MONSTERSTATE_PRONE || !( attacker->flags & FL_CLIENT ) )
		return result;

	CBaseEntity *enemy = OtisHandleEntity( m_hEnemy.m_Handle );

	if ( !enemy )
	{
		if ( pev->deadflag != DEAD_NO )
			return result;

		if ( ( m_afMemory & bits_MEMORY_SUSPICIOUS ) || attackerEntity->IsFacing( pev, 0.96f ) )
		{
			PlaySentence( "OT_MAD", 4, VOL_NORM, ATTN_NORM );
			Remember( bits_MEMORY_PROVOKED );
			StopPlayerFollowing( true );
		}
		else
		{
			PlaySentence( "OT_SHOT", 4, VOL_NORM, ATTN_NORM );
			Remember( bits_MEMORY_SUSPICIOUS );
		}
	}
	else if ( !enemy->IsPlayer() && pev->deadflag == DEAD_NO )
	{
		PlaySentence( "OT_SHOT", 4, VOL_NORM, ATTN_NORM );
	}

	return result;
}

void COtis::TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType )
{
	switch ( trace->iHitgroup )
	{
		case HITGROUP_CHEST:
		case HITGROUP_STOMACH:
			if ( damageType & ( DMG_BULLET | DMG_SLASH | DMG_BLAST ) )
			{
				if ( damage >= 2 )
					damage -= 2;

				damage = (float) ( (double) damage * 0.5 );
			}
			break;

		case HITGROUP_HEAD:
		case 10:
			damage = (float) ( (double) damage * 0.7 );
			break;
	}

	CBaseMonster::TraceAttack( attacker, damage, direction, trace, damageType );
}

void COtis::Killed( entvars_t *attacker, int gib )
{
	if ( m_fGunDrawn )
	{
		DropWeapon( "weapon_eagle", false );
		m_fGunDrawn = false;
		SetBodygroup( 1, 0 );
	}

	CTalkMonster::Killed( attacker, gib );
}

float COtis::GetPointsForDamage( float damage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	return ( pev->max_health / sk_otis_health.value * 4.0f ) * ( damage / pev->max_health );
}

Schedule_t *COtis::GetScheduleOfType( int type )
{
	Schedule_t *schedule;

	switch ( type )
	{
		case SCHED_ARM_WEAPON:
			if ( OtisHandleEntity( m_hEnemy.m_Handle ) )
				return slOtisEnemyDraw;
			break;

		case SCHED_TARGET_FACE:
			schedule = CTalkMonster::GetScheduleOfType( type );
			return schedule == slIdleStand ? slOtisFaceTarget : schedule;

		case SCHED_TARGET_CHASE:
			return slOtisFollow;

		case SCHED_RELOAD:
			return slOtisReloadQuick;

		case SCHED_OTIS_RELOAD:
			return slOtisReload;

		case SCHED_IDLE_STAND:
			schedule = CTalkMonster::GetScheduleOfType( type );
			return schedule == slIdleStand ? slOtisIdleStand : schedule;
	}

	return CTalkMonster::GetScheduleOfType( type );
}

Schedule_t *COtis::GetSchedule( void )
{
	if ( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		CSound *sound = PBestSound();

		if ( sound && ( sound->m_iType & bits_SOUND_DANGER ) )
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
	}

	if ( HasConditions( bits_COND_ENEMY_DEAD ) && FOkToSpeak() )
		PlaySentence( "OT_KILL", 4, VOL_NORM, ATTN_NORM );

	switch ( m_MonsterState )
	{
		case MONSTERSTATE_COMBAT:
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
				return CBaseMonster::GetSchedule();

			if ( HasAllConditions( bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE ) )
				return GetScheduleOfType( SCHED_SMALL_FLINCH );

			if ( !m_fGunDrawn )
				return GetScheduleOfType( SCHED_ARM_WEAPON );

			if ( HasConditions( bits_COND_HEAVY_DAMAGE ) )
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );

			if ( HasConditions( bits_COND_NO_AMMO_LOADED ) )
				return GetScheduleOfType( SCHED_OTIS_RELOAD );
			break;

		case MONSTERSTATE_IDLE:
			if ( m_cAmmoLoaded != m_cClipSize )
				return GetScheduleOfType( SCHED_OTIS_RELOAD );
			// Fall through
		case MONSTERSTATE_ALERT:
			if ( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
				return GetScheduleOfType( SCHED_SMALL_FLINCH );

			if ( ( !OtisHandleEntity( m_hEnemy.m_Handle ) || !HasConditions( bits_COND_SEE_ENEMY ) ) && IsPlayerFollowing() )
			{
				if ( !OtisHandleEntity( m_hTargetEnt.m_Handle )->IsAlive() )
				{
					StopPlayerFollowing( false );
					return CBaseMonster::GetSchedule();
				}

				if ( HasConditions( bits_COND_CLIENT_PUSH ) )
					return GetScheduleOfType( SCHED_MOVE_AWAY_FOLLOW );

				return GetScheduleOfType( SCHED_TARGET_FACE );
			}

			if ( HasConditions( bits_COND_CLIENT_PUSH ) )
				return GetScheduleOfType( SCHED_MOVE_AWAY );

			TrySmellTalk();
			break;
	}

	return CBaseMonster::GetSchedule();
}

class CDeadOtis : public CBaseMonster
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *data );
	int Classify( void );

	int m_iPose;

	static const char *m_szPoses[5];
};

LINK_ENTITY_TO_CLASS( monster_otis_dead, CDeadOtis );

const char *CDeadOtis::m_szPoses[] =
{
	"lying_on_back",
	"lying_on_side",
	"lying_on_stomach",
	"stuffed_in_vent",
	"dead_sitting",
};

int CDeadOtis::Classify( void )
{
	return GetClassification( CLASS_PLAYER_ALLY );
}

void CDeadOtis::KeyValue( KeyValueData *data )
{
	if ( !strcmp( data->szKeyName, "pose" ) )
	{
		m_iPose = atoi( data->szValue );

		if ( m_iPose < 0 )
			m_iPose = 0;

		if ( m_iPose > 4 )
			m_iPose = 4;

		data->fHandled = TRUE;
	}
	else
	{
		CBaseMonster::KeyValue( data );
	}
}

void CDeadOtis::Spawn( void )
{
	if ( !pev->model )
	{
		UTIL_PrecacheModel( this, "models/otis.mdl" );
		SetModel_Intercept( edict(), "models/otis.mdl" );
	}
	else
	{
		UTIL_PrecacheModel( this, STRING( pev->model ) );
		SetModel_Intercept( edict(), STRING( pev->model ) );
	}

	m_bloodColor = BLOOD_COLOR_RED;
	pev->health = 8;
	pev->skin = 0;
	pev->effects = 0;
	pev->yaw_speed = 8;

	if ( m_iPose < 0 )
		m_iPose = 0;

	if ( m_iPose > 4 )
		m_iPose = 4;

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
