#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "squadmonster.h"
#include "talkmonster.h"
#include "weapons.h"
#include "soundent.h"
#include "user_messages.h"
#include "shock.h"
#include "sporegrenade.h"
#include "shocktrooper.h"

extern cvar_t sk_shocktrooper_health;
extern cvar_t sk_shocktrooper_kick;
extern cvar_t sk_shocktrooper_enable_spores;
extern cvar_t sk_hgrunt_gspeed;

#define HGRUNT_AE_RELOAD		2
#define HGRUNT_AE_KICK			3
#define HGRUNT_AE_BURST1		4
#define HGRUNT_AE_GREN_TOSS		7
#define HGRUNT_AE_CAUGHT_ENEMY	10
#define HGRUNT_AE_DROP_GUN		11

// pev->weapons
#define SHOCK_ROACH				( 1 << 0 )
#define SHOCK_GRENADE			( 1 << 1 )
#define SHOCK_NODROP			( 1 << 8 )

#define SHOCK_GUN_GROUP			1
#define SHOCK_GUN_ROACH			0
#define SHOCK_GUN_NONE			1

enum
{
	SCHED_SHOCK_SUPPRESS = LAST_COMMON_SCHEDULE + 1,
	SCHED_SHOCK_ESTABLISH_LINE_OF_FIRE,
	SCHED_SHOCK_COVER_AND_RELOAD,
	SCHED_SHOCK_SWEEP,
	SCHED_SHOCK_FOUND_ENEMY,
	SCHED_SHOCK_WAIT_IN_COVER,
	SCHED_SHOCK_TAKECOVER_FAILED,
	SCHED_SHOCK_ELOF_FAIL
};

enum
{
	TASK_SHOCK_FACE_TOSS_DIR = LAST_COMMON_TASK + 1,
	TASK_SHOCK_SPEAK_SENTENCE,
	TASK_SHOCK_CHECK_FIRE
};

static int g_fShocktrooperQuestion;
static const char *shockSentences[] = {
	"ST_GREN",
	"ST_ALERT",
	"ST_MONSTER",
	"ST_COVER",
	"ST_THROW",
	"ST_CHARGE",
	"ST_TAUNT"
};

LINK_ENTITY_TO_CLASS( monster_shocktrooper, CShocktrooper );

TYPEDESCRIPTION CShocktrooper::m_SaveData[] = {
	DEFINE_FIELD( CShocktrooper, m_flNextGrenadeCheck, FIELD_TIME ),
	DEFINE_FIELD( CShocktrooper, m_flNextPainTime, FIELD_TIME ),
	DEFINE_FIELD( CShocktrooper, m_vecTossVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( CShocktrooper, m_fThrowGrenade, FIELD_CHARACTER ),
	DEFINE_FIELD( CShocktrooper, m_fStanding, FIELD_CHARACTER ),
	DEFINE_FIELD( CShocktrooper, m_fFirstEncounter, FIELD_CHARACTER ),
	DEFINE_FIELD( CShocktrooper, m_cClipSize, FIELD_INTEGER ),
	DEFINE_FIELD( CShocktrooper, m_voicePitch, FIELD_INTEGER ),
	DEFINE_FIELD( CShocktrooper, m_iSentence, FIELD_INTEGER )
};

IMPLEMENT_SAVERESTORE( CShocktrooper, CSquadMonster );

void CShocktrooper::Spawn( void )
{
	Precache();

	if ( !SetupModel() )
		UTIL_SetModel( this, "models/strooper.mdl" );

	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 88 ) );

	if ( pev->weapons & SHOCK_NODROP )
		pev->spawnflags |= SF_MONSTER_NO_WPN_DROP;

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_YELLOW;
	pev->effects = 0;

	if ( pev->health == 0 )
		pev->health = sk_shocktrooper_health.value;

	m_flHealthScale = sk_shocktrooper_health.value;
	m_flFieldOfView = -0.7f;
	m_MonsterState = MONSTERSTATE_NONE;
	m_flNextGrenadeCheck = gpGlobals->time + 1.0f;
	m_flNextPainTime = gpGlobals->time;
	m_iSentence = -1;
	m_afCapability = bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP | bits_CAP_USE_TANK;
	m_fEnemyEluded = FALSE;
	m_fFirstEncounter = true;
	m_fDroppedShockroach = false;
	m_HackedGunPos = Vector( 0, 0, 55 );
	pev->weapons = SHOCK_ROACH | SHOCK_GRENADE;
	m_cClipSize = 15;
	m_cAmmoLoaded = 15;
	m_afMoveShootCap = 0;

	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( "Shock Trooper" );

	MonsterInit();
	pev->view_ofs = Vector( 0, 0, 64 );

	if ( IsPlayerAlly() && !( pev->spawnflags & SF_MONSTER_PREDISASTER ) )
	{
		SetUse( &CBaseMonster::CallFollowerPlayerUse );
		m_fCanPlayerFollow = true;
	}
}

void CShocktrooper::Precache( void )
{
	CBaseMonster::Precache();
	UTIL_PrecacheOther( "monster_shockroach" );
	UTIL_PrecacheOther( "sporegrenade" );
	UTIL_PrecacheOther( "shock_beam" );

	if ( !pev->model )
		UTIL_PrecacheModel( this, "models/strooper.mdl" );

	UTIL_PrecacheModel( this, "models/strooper_gibs.mdl" );
	m_flMuzzleSprite = (float) UTIL_PrecacheModel( this, "sprites/muzzle_shock.spr" );
	PrecacheSound_Intercept( this, "shocktrooper/shock_fire.wav" );
	PrecacheSound_Intercept( this, "shocktrooper/shock_trooper_die1.wav" );
	PrecacheSound_Intercept( this, "shocktrooper/shock_trooper_die2.wav" );
	PrecacheSound_Intercept( this, "shocktrooper/shock_trooper_die3.wav" );
	PrecacheSound_Intercept( this, "shocktrooper/shock_trooper_pain1.wav" );
	PrecacheSound_Intercept( this, "shocktrooper/shock_trooper_pain2.wav" );
	PrecacheSound_Intercept( this, "shocktrooper/shock_trooper_pain4.wav" );
	PrecacheSound_Intercept( this, "shocktrooper/shock_trooper_pain5.wav" );
	PrecacheSound_Intercept( this, "zombie/claw_miss2.wav" );

	if ( RANDOM_LONG( 0, 1 ) )
		m_voicePitch = RANDOM_LONG( 0, 7 ) + 109;
	else
		m_voicePitch = 100;
}

int CShocktrooper::Classify( void )
{
	return GetClassification( CLASS_XRACE_SHOCK );
}

void CShocktrooper::GibMonster( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "common/bodysplat.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
	SpawnRandomGibs( pev, 7, 1 );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time;
}

void CShocktrooper::SpawnRandomGibs( entvars_t *pevVictim, int cGibs, int )
{
	for ( int i = 0; i < cGibs; i++ )
	{
		CGib *gib = GetClassPtr( (CGib *) NULL );

		gib->Spawn( "models/strooper_gibs.mdl" );
		gib->pev->body = RANDOM_LONG( 0, cGibs - 1 );

		if ( pevVictim )
		{
			gib->pev->origin.x = pevVictim->absmin.x + pevVictim->size.x * RANDOM_FLOAT( 0, 1 );
			gib->pev->origin.y = pevVictim->absmin.y + pevVictim->size.y * RANDOM_FLOAT( 0, 1 );
			gib->pev->origin.z = pevVictim->absmin.z + pevVictim->size.z * RANDOM_FLOAT( 0, 1 ) + 1;

			gib->pev->velocity = -g_vecAttackDir;
			gib->pev->velocity.x += RANDOM_FLOAT( -0.25, 0.25 );
			gib->pev->velocity.y += RANDOM_FLOAT( -0.25, 0.25 );
			gib->pev->velocity.z += RANDOM_FLOAT( -0.25, 0.25 );
			gib->pev->velocity = gib->pev->velocity * RANDOM_FLOAT( 300, 400 );
			gib->pev->avelocity.x = RANDOM_FLOAT( 100, 200 );
			gib->pev->avelocity.y = RANDOM_FLOAT( 100, 300 );
			gib->m_bloodColor = CBaseEntity::Instance( pevVictim )->BloodColor();

			if ( pevVictim->health > -50 )
				gib->pev->velocity = gib->pev->velocity * 0.7f;
			else if ( pevVictim->health > -200 )
				gib->pev->velocity = gib->pev->velocity * 2;
			else
				gib->pev->velocity = gib->pev->velocity * 4;

			gib->pev->solid = SOLID_BBOX;
			UTIL_SetSize( gib->pev, g_vecZero, g_vecZero );
		}

		gib->LimitVelocity();
	}
}

int CShocktrooper::ObjectCaps( void )
{
	return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

float CShocktrooper::GetPointsForDamage( float damage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	return ( pev->max_health / sk_shocktrooper_health.value ) * 6.0f * ( damage / pev->max_health );
}

void CShocktrooper::Revive( void )
{
	CBaseMonster::Revive();
	SetBodygroup( SHOCK_GUN_GROUP, SHOCK_GUN_ROACH );
}

int CShocktrooper::ISoundMask( void )
{
	return bits_SOUND_WORLD | bits_SOUND_COMBAT | bits_SOUND_PLAYER | bits_SOUND_DANGER | bits_SOUND_BULLETHIT;
}

void CShocktrooper::SetYawSpeed( void )
{
	pev->yaw_speed = 270;
}

BOOL CShocktrooper::FCanCheckAttacks( void )
{
	return !HasConditions( bits_COND_ENEMY_TOOFAR );
}

BOOL CShocktrooper::CheckMeleeAttack1( float dot, float distance )
{
	if ( m_hEnemy == NULL )
		return FALSE;

	CBaseMonster *enemy = m_hEnemy->MyMonsterPointer();

	return enemy && distance <= 64.0f && dot >= 0.7 && enemy->Classify() != CLASS_ALIEN_BIOWEAPON && enemy->Classify() != CLASS_PLAYER_BIOWEAPON;
}

BOOL CShocktrooper::CheckRangeAttack1( float dot, float distance )
{
	if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) || distance > 2048.0f || dot < 0.5f || !NoFriendlyFire() )
		return FALSE;

	if ( !m_hEnemy->IsPlayer() && distance <= 96.0f )
		return FALSE;

	TraceResult trace;
	Vector source;
	Vector angles;

	GetAttachment( 1, source, angles );
	UTIL_TraceLine( source, m_hEnemy->BodyTarget( source ), ignore_monsters, ignore_glass, edict(), &trace );

	if ( trace.flFraction != 1.0f && trace.pHit != m_hEnemy->edict() )
	{
		m_fStanding = !m_fStanding;
		return FALSE;
	}

	return TRUE;
}

BOOL CShocktrooper::CheckRangeAttack2( float dot, float distance )
{
	if ( RANDOM_LONG( 0, 1 ) && m_flGroundSpeed != 0.0f )
	{
		m_fThrowGrenade = false;
		return FALSE;
	}

	if ( m_flNextGrenadeCheck > gpGlobals->time )
		return m_fThrowGrenade;

	if ( !( m_hEnemy->pev->flags & FL_ONGROUND ) && m_hEnemy->pev->waterlevel == 0 && m_vecEnemyLKP.z > pev->origin.z + 72.0f )
	{
		m_fThrowGrenade = false;
		return FALSE;
	}

	Vector target;

	if ( m_hEnemy->IsBSPModel() )
	{
		Vector body = m_hEnemy->BodyTarget( pev->origin );
		Vector center = m_hEnemy->Center();

		target = body - center;
	}
	else
		target = m_hEnemy->BodyTarget( pev->origin ) - m_hEnemy->pev->origin;

	target = target + m_vecEnemyLKP;

	if ( SquadMemberInRange( target, 256 ) || ( target - pev->origin ).Length2D() <= 256.0f )
	{
		m_fThrowGrenade = false;
		m_flNextGrenadeCheck = gpGlobals->time + 1.0f;
		return FALSE;
	}

	Vector velocity = VecCheckThrow( pev, GetGunPosition(), target, sk_hgrunt_gspeed.value, 1.0f );

	if ( velocity == g_vecZero )
	{
		m_fThrowGrenade = false;
		m_flNextGrenadeCheck = gpGlobals->time + 1.0f;
		return FALSE;
	}

	m_vecTossVelocity = velocity;
	m_fThrowGrenade = true;
	m_flNextGrenadeCheck = gpGlobals->time + 0.3f;
	return TRUE;
}

void CShocktrooper::CheckAmmo( void )
{
	if ( m_cAmmoLoaded <= 0 )
		SetConditions( bits_COND_NO_AMMO_LOADED );
}

void CShocktrooper::PrescheduleThink( void )
{
	if ( InSquad() && m_hEnemy != NULL )
	{
		if ( HasConditions( bits_COND_SEE_ENEMY ) )
			MySquadLeader()->m_flLastEnemySightTime = gpGlobals->time;
		else if ( gpGlobals->time - MySquadLeader()->m_flLastEnemySightTime > 5.0f )
			MySquadLeader()->m_fEnemyEluded = TRUE;
	}
}

void CShocktrooper::RunAI( void )
{
	if ( gpGlobals->time > m_flNextAmmoRecharge )
	{
		if ( m_cAmmoLoaded >= 15 )
			ClearConditions( bits_COND_NO_AMMO_LOADED );
		else
			m_cAmmoLoaded++;

		m_flNextAmmoRecharge = gpGlobals->time + 0.5f;
	}

	if ( m_Activity == ACT_COWER )
		pev->framerate = 1.5f;

	if ( pev->skin != 0 || RANDOM_LONG( 0, 30 ) != 0 )
	{
		if ( pev->skin != 0 )
			pev->skin--;
	}
	else
		pev->skin = 3;

	CSquadMonster::RunAI();
}

void CShocktrooper::SetActivity( Activity activity )
{
	int sequence;

	switch ( activity )
	{
		case ACT_RUN:
			sequence = LookupActivity( pev->health > 50.0f ? ACT_RUN : ACT_RUN_HURT );
			break;

		case ACT_RANGE_ATTACK1:
			if ( pev->weapons & SHOCK_ROACH )
				sequence = LookupSequence( m_fStanding ? "standing_mp5" : "crouching_mp5" );
			else
				sequence = LookupSequence( m_fStanding ? "standing_shotgun" : "crouching_shotgun" );
			break;

		case ACT_RANGE_ATTACK2:
			sequence = LookupSequence( "throwgrenade" );
			break;

		default:
			sequence = LookupActivity( activity );
			break;
	}

	m_Activity = activity;
	m_IdealActivity = activity;

	if ( sequence < 0 )
	{
		ALERT( at_console, "%s has no sequence for act:%d\n", STRING( pev->classname ), activity );
		pev->sequence = 0;
		return;
	}

	if ( pev->sequence != sequence || !m_fSequenceLoops )
		pev->frame = 0;

	pev->sequence = sequence;
	ResetSequenceInfo();
	SetYawSpeed();
}

void CShocktrooper::StartTask( Task_t *task )
{
	switch ( task->iTask )
	{
		case TASK_RUN_PATH:
		case TASK_WALK_PATH:
			Forget( bits_MEMORY_INCOVER );
			CBaseMonster::StartTask( task );
			break;

		case TASK_FACE_IDEAL:
		case TASK_FACE_ENEMY:
			CBaseMonster::StartTask( task );

			if ( pev->movetype == MOVETYPE_FLY )
				m_IdealActivity = ACT_WALK;
			break;

		case TASK_RELOAD:
			m_IdealActivity = ACT_RELOAD;
			break;

		case TASK_SHOCK_FACE_TOSS_DIR:
			break;

		case TASK_SHOCK_SPEAK_SENTENCE:
			SpeakSentence();
			TaskComplete();
			break;

		case TASK_SHOCK_CHECK_FIRE:
			TaskComplete();

			if ( !NoFriendlyFire() )
			{
				ChangeSchedule( GetScheduleOfType( SCHED_FIND_ATTACK_POINT ) );
				SetConditions( bits_COND_SPECIAL1 );
			}
			break;

		default:
			CBaseMonster::StartTask( task );
			break;
	}
}

void CShocktrooper::RunTask( Task_t *task )
{
	switch ( task->iTask )
	{
		case TASK_FACE_ENEMY:
			if ( m_hEnemy != NULL )
				MakeIdealYaw( m_vecEnemyLKP + m_hEnemy->pev->velocity / 2500.0f );
			else
				MakeIdealYaw( m_vecEnemyLKP );

			ChangeYaw( pev->yaw_speed );

			if ( FacingIdeal() )
				TaskComplete();
			break;

		case TASK_SHOCK_FACE_TOSS_DIR:
			MakeIdealYaw( pev->origin + m_vecTossVelocity * 64.0f );
			ChangeYaw( pev->yaw_speed );

			if ( FacingIdeal() )
				m_iTaskStatus = TASKSTATUS_COMPLETE;
			break;

		default:
			CBaseMonster::RunTask( task );
			break;
	}
}

Vector CShocktrooper::GetGunPosition( void )
{
	return pev->origin + Vector( 0, 0, m_fStanding ? 70 : 58 );
}

void CShocktrooper::Shoot( void )
{
	if ( m_hEnemy == NULL )
		return;

	Vector source;
	Vector angles;

	GetAttachment( 1, source, angles );

	Vector muzzle = source;
	Vector direction = ShootAtEnemy( source );

	direction = direction + m_hEnemy->pev->velocity / 2700.0f;

	UTIL_MakeVectors( pev->angles );
	CShockEffect::Shoot( pev, source, direction * 2000.0f );
	angles = UTIL_VecToAngles( direction );
	SetBlending( 0, angles.x );
	muzzle = muzzle + gpGlobals->v_up + gpGlobals->v_forward * 20.0f + gpGlobals->v_right * 5.0f;
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, source );
	WRITE_BYTE( TE_SPRITE );
	WRITE_COORD( muzzle.x );
	WRITE_COORD( muzzle.y );
	WRITE_COORD( muzzle.z );
	WRITE_SHORT( (int) m_flMuzzleSprite );
	WRITE_BYTE( 5 );
	WRITE_BYTE( 150 );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_PVS, gmsgShkFlash, muzzle );
	WRITE_COORD( muzzle.x );
	WRITE_COORD( muzzle.y );
	WRITE_COORD( muzzle.z );
	WRITE_BYTE( 0 );
	MESSAGE_END();
}

CBaseEntity *CShocktrooper::Kick( void )
{
	UTIL_MakeVectors( pev->angles );

	Vector source = pev->origin + Vector( 0, 0, pev->size.z * 0.5 );
	Vector end = source + gpGlobals->v_forward * 70.0f;
	TraceResult trace;

	UTIL_TraceHull( source, end, dont_ignore_monsters, head_hull, edict(), &trace );
	return CBaseEntity::Instance( trace.pHit );
}

void CShocktrooper::HandleAnimEvent( MonsterEvent_t *event )
{
	switch ( event->event )
	{
		case HGRUNT_AE_RELOAD:
			if ( m_cAmmoLoaded <= 0 )
				m_cAmmoLoaded = 15;

			ClearConditions( bits_COND_NO_AMMO_LOADED );
			break;

		case HGRUNT_AE_KICK:
		{
			CBaseEntity *target = Kick();

			if ( !target )
				break;

			if ( ( ( target->pev->flags & FL_MONSTER )
			       || ( ( target->pev->flags & FL_CLIENT ) && target->pev->movetype != MOVETYPE_WALK ) )
			     && !FClassnameIs( target->pev, "monster_alien_voltigore" )
			     && !FClassnameIs( target->pev, "monster_gargantua" )
			     && !FClassnameIs( target->pev, "monster_bigmomma" ) )
			{
				UTIL_MakeVectors( pev->angles );
				target->pev->punchangle.x = 15;

				if ( target->pev->flags & FL_CLIENT )
					target->pev->velocity = target->pev->velocity + gpGlobals->v_forward * 100.0f + gpGlobals->v_right * 50.0f;
			}

			target->TakeDamage( pev, pev, sk_shocktrooper_kick.value, DMG_CLUB );
			break;
		}

		case HGRUNT_AE_BURST1:
			Shoot();
			CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 0.3f, this );
			break;

		case HGRUNT_AE_GREN_TOSS:
			UTIL_MakeVectors( pev->angles );
			CSporeGrenade::ShootTimed( pev, GetGunPosition(), m_vecTossVelocity, 5.0f );
			m_fThrowGrenade = false;
			m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 4, 8 );
			break;

		case HGRUNT_AE_CAUGHT_ENEMY:
			AlertSound();
			CBaseMonster::HandleAnimEvent( event );
			break;

		case HGRUNT_AE_DROP_GUN:
			if ( !m_fDroppedShockroach && !( pev->spawnflags & SF_MONSTER_NO_WPN_DROP ) )
			{
				SetBodygroup( SHOCK_GUN_GROUP, SHOCK_GUN_NONE );
				UTIL_SetSize( pev, Vector( -1, -1, 0 ), Vector( 1, 1, 1 ) );

				Vector angles( 0, pev->angles.y, 0 );
				CBaseEntity *roach = CBaseEntity::Create( "monster_shockroach", GetGunPosition(), angles, true, NULL );

				roach->pev->spawnflags |= SF_MONSTER_FALL_TO_GROUND;
				roach->SetPlayerAllyDirect( IsPlayerAlly() );

				CBaseMonster *monster = roach->MyMonsterPointer();

				monster->m_fOverrideClass = m_fOverrideClass;
				monster->m_iClassSelection = m_iClassSelection;
				DispatchSpawn( roach->edict() );
				monster->ChangeSchedule( GetScheduleOfType( SCHED_RANGE_ATTACK1 ) );
				m_fDroppedShockroach = true;
			}
			break;

		default:
			CBaseMonster::HandleAnimEvent( event );
			break;
	}
}

int CShocktrooper::IRelationship( CBaseEntity *target, bool ignoreNoTarget )
{
	if ( target && !target->m_fOverrideClass && Classify() == CLASS_XRACE_SHOCK
	     && ( FClassnameIs( target->pev, "monster_alien_grunt" )
	          || FClassnameIs( target->pev, "monster_gargantua" )
	          || FClassnameIs( target->pev, "monster_bigmomma" ) ) )
		return R_HT;

	return CBaseMonster::IRelationship( target, ignoreNoTarget );
}

void CShocktrooper::TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType )
{
	CBaseMonster::TraceAttack( attacker, damage, direction, trace, damageType );
}

int CShocktrooper::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	UTIL_GetGlobalTrace();

	// The hitgroup test always selects this armor branch
	if ( damageType & ( DMG_BULLET | DMG_SNIPER ) )
		damage = damage * 0.45;

	if ( damageType & ( DMG_SLASH | DMG_CLUB | DMG_SHOCK ) )
	{
		if ( damage >= 5.0f )
			damage = damage - 5.0f;

		damage = damage * 0.4;
	}

	if ( ( damageType & DMG_BLAST ) && pev->health > 10.0f )
	{
		if ( damage > 15.0f )
			damage = damage - 15.0f;

		damage = damage * 0.6;
	}

	return CBaseMonster::TakeDamage( inflictor, attacker, damage, damageType );
}

BOOL CShocktrooper::FOkToSpeak( void )
{
	if ( !IsAlive() || CTalkMonster::g_talkWaitTime > gpGlobals->time )
		return FALSE;

	return !( pev->spawnflags & SF_MONSTER_GAG ) || m_MonsterState == MONSTERSTATE_COMBAT;
}

void CShocktrooper::JustSpoke( void )
{
	CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT( 1.5, 2.0 );
	m_iSentence = -1;
}

void CShocktrooper::SpeakSentence( void )
{
	if ( m_iSentence != -1 && FOkToSpeak() )
	{
		SENTENCEG_PlayRndSz( edict(), shockSentences[m_iSentence], 0.9f, 0.8f, 0, m_voicePitch );
		JustSpoke();
	}
}

void CShocktrooper::AlertSound( void )
{
	if ( FOkToSpeak() )
	{
		SENTENCEG_PlayRndSz( edict(), "ST_ALERT", 0.9f, 0.8f, 0, m_voicePitch );
		JustSpoke();
	}
}

void CShocktrooper::UseSound( void )
{
	AlertSound();
}

void CShocktrooper::UnUseSound( void )
{
	IdleSound();
}

void CShocktrooper::IdleSound( void )
{
	if ( !FOkToSpeak() || ( !g_fShocktrooperQuestion && !RANDOM_LONG( 0, 1 ) ) )
		return;

	if ( g_fShocktrooperQuestion == 0 )
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
			case 0:
				SENTENCEG_PlayRndSz( edict(), "ST_CHECK", 0.9f, 0.8f, 0, m_voicePitch );
				g_fShocktrooperQuestion = 1;
				break;
			case 1:
				SENTENCEG_PlayRndSz( edict(), "ST_QUEST", 0.9f, 0.8f, 0, m_voicePitch );
				g_fShocktrooperQuestion = 2;
				break;
			case 2:
				SENTENCEG_PlayRndSz( edict(), "ST_IDLE", 0.9f, 0.8f, 0, m_voicePitch );
				break;
		}
	}
	else
	{
		if ( g_fShocktrooperQuestion == 1 )
			SENTENCEG_PlayRndSz( edict(), "ST_CLEAR", 0.9f, 0.8f, 0, m_voicePitch );
		else if ( g_fShocktrooperQuestion == 2 )
			SENTENCEG_PlayRndSz( edict(), "ST_ANSWER", 0.9f, 0.8f, 0, m_voicePitch );

		g_fShocktrooperQuestion = 0;
	}

	JustSpoke();
}

void CShocktrooper::DeathSound( void )
{
	static const char *sounds[] = {
		"shocktrooper/shock_trooper_die1.wav",
		"shocktrooper/shock_trooper_die2.wav",
		"shocktrooper/shock_trooper_die3.wav"
	};

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, sounds[RANDOM_LONG( 0, 2 )], 1.0f, ATTN_NORM, 0, PITCH_NORM );
}

void CShocktrooper::PainSound( void )
{
	if ( gpGlobals->time <= m_flNextPainTime )
		return;

	static const char *sounds[] = {
		"shocktrooper/shock_trooper_pain1.wav",
		"shocktrooper/shock_trooper_pain2.wav",
		"shocktrooper/shock_trooper_pain4.wav",
		"shocktrooper/shock_trooper_pain5.wav"
	};
	int index = RANDOM_LONG( 0, 6 );

	if ( index < ARRAYSIZE( sounds ) )
		EMIT_SOUND_DYN( edict(), CHAN_VOICE, sounds[index], 1.0f, ATTN_NORM, 0, PITCH_NORM );

	m_flNextPainTime = gpGlobals->time + 1.0f;
}

static Task_t tlShockFail[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT, 1 }
};

static Schedule_t slShockFail[] = {
	{
		tlShockFail,
		ARRAYSIZE( tlShockFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK2 |
		bits_COND_NEW_ENEMY,
		0,
		"Grunt Fail"
	}
};

static Task_t tlShockCombatFail[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 1 }
};

static Schedule_t slShockCombatFail[] = {
	{
		tlShockCombatFail,
		ARRAYSIZE( tlShockCombatFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"Grunt Combat Fail"
	}
};

static Task_t tlShockVictoryDance[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_WAIT, 1.5f },
	{ TASK_GET_PATH_TO_ENEMY_CORPSE, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE }
};

static Schedule_t slShockVictoryDance[] = {
	{
		tlShockVictoryDance,
		ARRAYSIZE( tlShockVictoryDance ),
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntVictoryDance"
	}
};

static Task_t tlShockEstablishLineOfFire[] = {
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_SHOCK_ELOF_FAIL },
	{ TASK_GET_PATH_TO_ENEMY, 0 },
	{ TASK_SHOCK_SPEAK_SENTENCE, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_AP, 0 }
};

static Schedule_t slShockEstablishLineOfFire[] = {
	{
		tlShockEstablishLineOfFire,
		ARRAYSIZE( tlShockEstablishLineOfFire ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK2 |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD,
		bits_SOUND_DANGER,
		"GruntEstablishLineOfFire"
	}
};

static Task_t tlShockFoundEnemy[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_SIGNAL1 }
};

static Schedule_t slShockFoundEnemy[] = {
	{
		tlShockFoundEnemy,
		ARRAYSIZE( tlShockFoundEnemy ),
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntFoundEnemy"
	}
};

static Task_t tlShockCombatFace[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT, 0.5f },
	{ TASK_SET_SCHEDULE, (float) SCHED_SHOCK_SWEEP }
};

static Schedule_t slShockCombatFace[] = {
	{
		tlShockCombatFace,
		ARRAYSIZE( tlShockCombatFace ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD,
		0,
		"Combat Face"
	}
};

static Task_t tlShockSignalSuppress[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_SIGNAL2 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_SHOCK_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_SHOCK_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_SHOCK_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_SHOCK_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_SHOCK_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 }
};

static Schedule_t slShockSignalSuppress[] = {
	{
		tlShockSignalSuppress,
		ARRAYSIZE( tlShockSignalSuppress ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_SPECIAL1,
		bits_SOUND_DANGER,
		"SignalSuppress"
	}
};

static Task_t tlShockSuppress[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_SHOCK_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_SHOCK_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_SHOCK_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_SHOCK_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_SHOCK_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 }
};

static Schedule_t slShockSuppress[] = {
	{
		tlShockSuppress,
		ARRAYSIZE( tlShockSuppress ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_SPECIAL1,
		bits_SOUND_DANGER,
		"Suppress"
	}
};

static Task_t tlShockWaitInCover[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 0.2f }
};

static Schedule_t slShockWaitInCover[] = {
	{
		tlShockWaitInCover,
		ARRAYSIZE( tlShockWaitInCover ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK2 |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntWaitInCover"
	}
};

static Task_t tlShockTakeCover[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_SHOCK_TAKECOVER_FAILED },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_SHOCK_SPEAK_SENTENCE, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_SET_SCHEDULE, (float) SCHED_SHOCK_WAIT_IN_COVER }
};

static Schedule_t slShockTakeCover[] = {
	{
		tlShockTakeCover,
		ARRAYSIZE( tlShockTakeCover ),
		0,
		0,
		"TakeCover"
	}
};

static Task_t tlShockTossGrenadeCover[] = {
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RANGE_ATTACK2, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_TAKE_COVER_FROM_ENEMY }
};

static Schedule_t slShockTossGrenadeCover[] = {
	{
		tlShockTossGrenadeCover,
		ARRAYSIZE( tlShockTossGrenadeCover ),
		0,
		0,
		"TossGrenadeCover"
	}
};

static Task_t tlShockTakeCoverFromBestSound[] = {
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_COWER },
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FIND_COVER_FROM_BEST_SOUND, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_GRENADE, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_TURN_LEFT, 179 }
};

static Schedule_t slShockTakeCoverFromBestSound[] = {
	{
		tlShockTakeCoverFromBestSound,
		ARRAYSIZE( tlShockTakeCoverFromBestSound ),
		0,
		bits_SOUND_DANGER,
		"GruntTakeCoverFromBestSound"
	}
};

static Task_t tlShockHideReload[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_RELOAD },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_RELOAD }
};

static Schedule_t slShockHideReload[] = {
	{
		tlShockHideReload,
		ARRAYSIZE( tlShockHideReload ),
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntHideReload"
	}
};

static Task_t tlShockSweep[] = {
	{ TASK_TURN_LEFT, 179 },
	{ TASK_WAIT, 1 },
	{ TASK_TURN_LEFT, 179 },
	{ TASK_WAIT, 1 }
};

static Schedule_t slShockSweep[] = {
	{
		tlShockSweep,
		ARRAYSIZE( tlShockSweep ),
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
	}
};

static Task_t tlShockRangeAttack1A[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_CROUCH },
	{ TASK_SHOCK_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_SHOCK_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_SHOCK_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_SHOCK_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_TAKE_COVER_FROM_ENEMY }
};

static Schedule_t slShockRangeAttack1A[] = {
	{
		tlShockRangeAttack1A,
		ARRAYSIZE( tlShockRangeAttack1A ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_SPECIAL1,
		bits_SOUND_DANGER,
		"Range Attack1A"
	}
};

static Task_t tlShockRangeAttack1B[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SHOCK_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_SHOCK_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_SHOCK_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_SHOCK_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_TAKE_COVER_FROM_ENEMY }
};

static Schedule_t slShockRangeAttack1B[] = {
	{
		tlShockRangeAttack1B,
		ARRAYSIZE( tlShockRangeAttack1B ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_SPECIAL1,
		bits_SOUND_DANGER,
		"Range Attack1B"
	}
};

static Task_t tlShockRangeAttack2[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SHOCK_FACE_TOSS_DIR, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_RANGE_ATTACK2 },
	{ TASK_SET_SCHEDULE, (float) SCHED_SHOCK_WAIT_IN_COVER }
};

static Schedule_t slShockRangeAttack2[] = {
	{
		tlShockRangeAttack2,
		ARRAYSIZE( tlShockRangeAttack2 ),
		0,
		0,
		"RangeAttack2"
	}
};

DEFINE_CUSTOM_SCHEDULES( CShocktrooper ){
	slShockFail,
	slShockCombatFail,
	slShockVictoryDance,
	slShockEstablishLineOfFire,
	slShockFoundEnemy,
	slShockCombatFace,
	slShockSignalSuppress,
	slShockSuppress,
	slShockWaitInCover,
	slShockTakeCover,
	slShockTossGrenadeCover,
	slShockTakeCoverFromBestSound,
	slShockHideReload,
	slShockSweep,
	slShockRangeAttack1A,
	slShockRangeAttack1B,
	slShockRangeAttack2
};

IMPLEMENT_CUSTOM_SCHEDULES( CShocktrooper, CSquadMonster );

Schedule_t *CShocktrooper::GetSchedule( void )
{
	m_iSentence = -1;

	if ( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		CSound *sound = PBestSound();

		if ( sound && ( sound->m_iType & bits_SOUND_DANGER ) )
		{
			if ( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz( edict(), "ST_GREN", 0.9f, 0.8f, 0, m_voicePitch );
				JustSpoke();
			}

			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
		}
	}

	if ( m_MonsterState != MONSTERSTATE_COMBAT || HasConditions( bits_COND_ENEMY_DEAD ) )
		return CBaseMonster::GetSchedule();

	if ( HasConditions( bits_COND_NEW_ENEMY ) )
	{
		if ( InSquad() )
		{
			MySquadLeader()->m_fEnemyEluded = FALSE;

			if ( !IsLeader() )
			{
				if ( m_fFirstEncounter )
				{
					m_fFirstEncounter = false;
					return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
				}

				return GetScheduleOfType( SCHED_SHOCK_ESTABLISH_LINE_OF_FIRE );
			}

			if ( FOkToSpeak() )
			{
				if ( m_hEnemy != NULL && m_hEnemy->IsPlayer() )
					SENTENCEG_PlayRndSz( edict(), "ST_ALERT", 0.9f, 0.8f, 0, m_voicePitch );
				else if ( m_hEnemy != NULL )
				{
					int classification = m_hEnemy->Classify();

					if ( classification != CLASS_HUMAN_PASSIVE && classification != CLASS_MACHINE && ( IsPlayerAlly() || classification != CLASS_PLAYER_ALLY ) )
						SENTENCEG_PlayRndSz( edict(), "ST_MONST", 0.9f, 0.8f, 0, m_voicePitch );
				}

				JustSpoke();
			}

			return GetScheduleOfType( HasConditions( bits_COND_CAN_RANGE_ATTACK1 )
			                              ? SCHED_SHOCK_SUPPRESS
			                              : SCHED_SHOCK_ESTABLISH_LINE_OF_FIRE );
		}
	}
	else
	{
		if ( HasConditions( bits_COND_NO_AMMO_LOADED ) )
			return GetScheduleOfType( SCHED_SHOCK_COVER_AND_RELOAD );

		if ( HasConditions( bits_COND_LIGHT_DAMAGE ) )
		{
			if ( RANDOM_LONG( 0, 99 ) > 90 || m_hEnemy == NULL )
				return GetScheduleOfType( SCHED_SMALL_FLINCH );

			if ( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz( edict(), "ST_COVER", 0.9f, 0.8f, 0, m_voicePitch );
				m_iSentence = 3;
				JustSpoke();
			}

			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
		}

		if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
			return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

		if ( ( pev->weapons & SHOCK_GRENADE ) && HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
			return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

		if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
		{
			if ( InSquad() && MySquadLeader()->m_fEnemyEluded && !HasConditions( bits_COND_ENEMY_FACING_ME ) )
			{
				MySquadLeader()->m_fEnemyEluded = FALSE;
				return GetScheduleOfType( SCHED_SHOCK_FOUND_ENEMY );
			}

			if ( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) || !HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) || !OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
		}

		if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) )
		{
			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
			{
				if ( FOkToSpeak() )
				{
					SENTENCEG_PlayRndSz( edict(), "ST_THROW", 0.9f, 0.8f, 0, m_voicePitch );
					JustSpoke();
				}

				return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
			}

			if ( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
			{
				if ( FOkToSpeak() )
				{
					SENTENCEG_PlayRndSz( edict(), "ST_CHARGE", 0.9f, 0.8f, 0, m_voicePitch );
					m_iSentence = 5;
					JustSpoke();
				}

				return GetScheduleOfType( SCHED_SHOCK_ESTABLISH_LINE_OF_FIRE );
			}

			if ( FOkToSpeak() && RANDOM_LONG( 0, 1 ) )
			{
				SENTENCEG_PlayRndSz( edict(), "ST_TAUNT", 0.9f, 0.8f, 0, m_voicePitch );
				JustSpoke();
			}

			return GetScheduleOfType( SCHED_STANDOFF );
		}
	}

	if ( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
	{
		m_iSentence = 6;
		return GetScheduleOfType( SCHED_SHOCK_ESTABLISH_LINE_OF_FIRE );
	}

	return CBaseMonster::GetSchedule();
}

Schedule_t *CShocktrooper::GetScheduleOfType( int type )
{
	switch ( type )
	{
		case SCHED_TAKE_COVER_FROM_ENEMY:
			if ( InSquad() && sk_shocktrooper_enable_spores.value >= 1.0f
			     && HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
			{
				if ( FOkToSpeak() )
				{
					SENTENCEG_PlayRndSz( edict(), "ST_THROW", 0.9f, 0.8f, 0, m_voicePitch );
					JustSpoke();
				}

				return slShockTossGrenadeCover;
			}

			return slShockTakeCover;

		case SCHED_TAKE_COVER_FROM_BEST_SOUND:
			return slShockTakeCoverFromBestSound;

		case SCHED_COMBAT_FACE:
			return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

		case SCHED_SHOCK_TAKECOVER_FAILED:
			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) && OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			return GetScheduleOfType( HasConditions( bits_COND_SEE_ENEMY ) ? SCHED_RANGE_ATTACK1 : SCHED_FAIL );

		case SCHED_SHOCK_ELOF_FAIL:
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );

		case SCHED_SHOCK_ESTABLISH_LINE_OF_FIRE:
			return slShockEstablishLineOfFire;

		case SCHED_RANGE_ATTACK1:
			if ( HasConditions( bits_COND_NO_AMMO_LOADED ) && HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

			return m_fStanding ? slShockRangeAttack1B : slShockRangeAttack1A;

		case SCHED_RANGE_ATTACK2:
			return slShockRangeAttack2;

		case SCHED_SHOCK_WAIT_IN_COVER:
			return slShockWaitInCover;

		case SCHED_SHOCK_SWEEP:
			return slShockSweep;

		case SCHED_SHOCK_COVER_AND_RELOAD:
			return slShockHideReload;

		case SCHED_SHOCK_FOUND_ENEMY:
			return slShockFoundEnemy;

		case SCHED_VICTORY_DANCE:
			return InSquad() && !IsLeader() ? slShockFail : slShockVictoryDance;

		case SCHED_SHOCK_SUPPRESS:
			if ( m_hEnemy->IsPlayer() && m_fFirstEncounter )
			{
				m_fFirstEncounter = false;
				return slShockSignalSuppress;
			}

			return slShockSuppress;

		case SCHED_FAIL:
			return m_hEnemy != NULL ? slShockCombatFail : slShockFail;

		default:
			return CBaseMonster::GetScheduleOfType( type );
	}
}
