#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "game.h"
#include "schedule.h"

extern cvar_t sk_zombie_barney_health;
extern cvar_t sk_zombie_barney_dmg_one_slash;
extern cvar_t sk_zombie_barney_dmg_both_slash;
extern cvar_t sk_zombie_soldier_health;
extern cvar_t sk_zombie_soldier_dmg_one_slash;
extern cvar_t sk_zombie_soldier_dmg_both_slash;

static const char *zombieHitSounds[] =
{
	"zombie/claw_strike1.wav",
	"zombie/claw_strike2.wav",
	"zombie/claw_strike3.wav"
};

static const char *zombieMissSounds[] =
{
	"zombie/claw_miss1.wav",
	"zombie/claw_miss2.wav"
};

static const char *zombieAttackSounds[] =
{
	"zombie/zo_attack1.wav",
	"zombie/zo_attack2.wav"
};

static const char *zombieIdleSounds[] =
{
	"zombie/zo_idle1.wav",
	"zombie/zo_idle2.wav",
	"zombie/zo_idle3.wav",
	"zombie/zo_idle4.wav"
};

static const char *zombieAlertSounds[] =
{
	"zombie/zo_alert10.wav",
	"zombie/zo_alert20.wav",
	"zombie/zo_alert30.wav"
};

static const char *zombiePainSounds[] =
{
	"zombie/zo_pain1.wav",
	"zombie/zo_pain2.wav"
};

void ZombiePrecacheSounds( void )
{
	int i;

	for ( i = 0; i < ARRAYSIZE( zombieHitSounds ); i++ )
		PRECACHE_SOUND( (char *) zombieHitSounds[i] );

	for ( i = 0; i < ARRAYSIZE( zombieMissSounds ); i++ )
		PRECACHE_SOUND( (char *) zombieMissSounds[i] );

	for ( i = 0; i < ARRAYSIZE( zombieAttackSounds ); i++ )
		PRECACHE_SOUND( (char *) zombieAttackSounds[i] );

	for ( i = 0; i < ARRAYSIZE( zombieIdleSounds ); i++ )
		PRECACHE_SOUND( (char *) zombieIdleSounds[i] );

	for ( i = 0; i < ARRAYSIZE( zombieAlertSounds ); i++ )
		PRECACHE_SOUND( (char *) zombieAlertSounds[i] );

	for ( i = 0; i < ARRAYSIZE( zombiePainSounds ); i++ )
		PRECACHE_SOUND( (char *) zombiePainSounds[i] );
}

void ZombieAttackSound( CBaseMonster *monster )
{
	int pitch = 100 + RANDOM_LONG( -5, 5 );
	const char *sound = zombieAttackSounds[RANDOM_LONG( 0, ARRAYSIZE( zombieAttackSounds ) - 1 )];

	EMIT_SOUND_DYN( monster->edict(), CHAN_VOICE, sound, 1.0, ATTN_NORM, 0, pitch );
}

void ZombiePainSound( CBaseMonster *monster )
{
	int pitch = 95 + RANDOM_LONG( 0, 9 );

	if ( RANDOM_LONG( 0, 5 ) >= 2 )
		return;

	const char *sound = zombiePainSounds[RANDOM_LONG( 0, ARRAYSIZE( zombiePainSounds ) - 1 )];

	EMIT_SOUND_DYN( monster->edict(), CHAN_VOICE, sound, 1.0, ATTN_NORM, 0, pitch );
}

void ZombieAlertSound( CBaseMonster *monster )
{
	int pitch = 95 + RANDOM_LONG( 0, 9 );
	const char *sound = zombieAlertSounds[RANDOM_LONG( 0, ARRAYSIZE( zombieAlertSounds ) - 1 )];

	EMIT_SOUND_DYN( monster->edict(), CHAN_VOICE, sound, 1.0, ATTN_NORM, 0, pitch );
}

void ZombieIdleSound( CBaseMonster *monster )
{
	int pitch = 100 + RANDOM_LONG( -5, 5 );
	const char *sound = zombieIdleSounds[RANDOM_LONG( 0, ARRAYSIZE( zombieIdleSounds ) - 1 )];

	EMIT_SOUND_DYN( monster->edict(), CHAN_VOICE, sound, 1.0, ATTN_NORM, 0, pitch );
}

void ZombieHandleAnimEvent( CBaseMonster *monster, MonsterEvent_t *event, float oneSlash, float bothSlash )
{
	if ( event->event < 1 || event->event > 3 )
	{
		monster->CBaseMonster::HandleAnimEvent( event );
		return;
	}

	int damage = (int) ( event->event == 3 ? bothSlash : oneSlash );
	CBaseEntity *hurt = monster->CheckTraceHullAttack( 70.0f, damage, DMG_SLASH );

	if ( hurt )
	{
		if ( ( hurt->pev->flags & FL_CLIENT )
		     || ( ( hurt->pev->flags & FL_MONSTER ) && hurt->pev->movetype != MOVETYPE_FLY ) )
		{
			hurt->pev->punchangle.x = 5.0f;

			if ( event->event == 1 )
			{
				hurt->pev->punchangle.z = -18.0f;
				hurt->pev->velocity = hurt->pev->velocity - gpGlobals->v_right * 100.0f;
			}
			else if ( event->event == 2 )
			{
				hurt->pev->punchangle.z = 18.0f;
				hurt->pev->velocity = hurt->pev->velocity + gpGlobals->v_right * 100.0f;
			}
			else
				hurt->pev->velocity = hurt->pev->velocity - gpGlobals->v_forward * 100.0f;
		}
	}

	int pitch = 100 + RANDOM_LONG( -5, 5 );
	const char *sound = hurt
	    ? zombieHitSounds[RANDOM_LONG( 0, ARRAYSIZE( zombieHitSounds ) - 1 )]
	    : zombieMissSounds[RANDOM_LONG( 0, ARRAYSIZE( zombieMissSounds ) - 1 )];

	EMIT_SOUND_DYN( monster->edict(), CHAN_WEAPON, sound, 1.0, ATTN_NORM, 0, pitch );

	if ( RANDOM_LONG( 0, 1 ) )
		ZombieAttackSound( monster );
}

int ZombieIgnoreConditions( CBaseMonster *monster, float &nextFlinch, float delay )
{
	int ignore = monster->CBaseMonster::IgnoreConditions();

	if ( ( monster->m_Activity == ACT_MELEE_ATTACK1 || monster->m_Activity == ACT_MELEE_ATTACK2 )
	     && nextFlinch >= gpGlobals->time )
		ignore |= bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE;

	if ( ( monster->m_Activity == ACT_SMALL_FLINCH || monster->m_Activity == ACT_BIG_FLINCH )
	     && nextFlinch < gpGlobals->time )
		nextFlinch = gpGlobals->time + delay;

	return ignore;
}

class CZombieBarney : public CBaseMonster
{
public:
	// the sequence decides the box, standing on the entity's origin
	void SetObjectCollisionBox( void )
	{
		if ( ( pev->spawnflags & SF_MONSTER_NO_MODEL_BOX ) || !m_fCustomModel )
		{
			CBaseEntity::SetObjectCollisionBox();
			return;
		}

		Vector mins = g_vecZero;
		Vector maxs = g_vecZero;

		ExtractBbox( 1, mins, maxs );
		mins.z = 0;
		pev->absmin = pev->origin + mins;
		pev->absmax = pev->origin + maxs;
	}

	void Spawn( void );
	void Precache( void );
	int ObjectCaps( void );
	int Classify( void );
	void SetYawSpeed( void );
	void HandleAnimEvent( MonsterEvent_t *event );
	int IgnoreConditions( void );
	int TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType );
	void TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType );
	float GetPointsForDamage( float damage );
	BOOL CheckRangeAttack1( float dot, float distance ) { return FALSE; }
	BOOL CheckRangeAttack2( float dot, float distance ) { return FALSE; }
	void PainSound( void );
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );
	void UseSound( void );
	void UnUseSound( void );

	float m_flNextFlinch;
};

LINK_ENTITY_TO_CLASS( monster_zombie_barney, CZombieBarney );

void CZombieBarney::Spawn( void )
{
	Precache();

	if ( !SetupModel() )
		UTIL_SetModel( this, "models/zombie_barney.mdl" );

	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_GREEN;

	if ( pev->health == 0.0f )
		pev->health = sk_zombie_barney_health.value;

	pev->view_ofs = VEC_VIEW;
	m_flFieldOfView = 0.5f;
	m_MonsterState = MONSTERSTATE_NONE;
	m_afCapability = bits_CAP_DOORS_GROUP;
	m_afMoveShootCap = 0;

	if ( !m_FormattedName )
		m_FormattedName = AllocStringLocal( "Zombie Barney" );

	MonsterInit();
}

void CZombieBarney::Precache( void )
{
	CBaseMonster::Precache();

	if ( !pev->model )
		UTIL_PrecacheModel( this, "models/zombie_barney.mdl" );

	ZombiePrecacheSounds();
}

int CZombieBarney::ObjectCaps( void )
{
	return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

int CZombieBarney::Classify( void )
{
	return GetClassification( CLASS_ALIEN_MONSTER );
}

void CZombieBarney::SetYawSpeed( void )
{
	pev->yaw_speed = 270.0f;
}

void CZombieBarney::HandleAnimEvent( MonsterEvent_t *event )
{
	ZombieHandleAnimEvent( this, event, sk_zombie_barney_dmg_one_slash.value, sk_zombie_barney_dmg_both_slash.value );
}

int CZombieBarney::IgnoreConditions( void )
{
	return ZombieIgnoreConditions( this, m_flNextFlinch, 2.0f );
}

int CZombieBarney::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	return CBaseMonster::TakeDamage( inflictor, attacker, damage, damageType );
}

void CZombieBarney::TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType )
{
	if ( ( trace->iHitgroup == HITGROUP_CHEST || trace->iHitgroup == HITGROUP_STOMACH )
	     && ( damageType & ( DMG_BULLET | DMG_SLASH | DMG_BLAST ) ) )
	{
		if ( damage >= 2.0f )
			damage -= 2.0f;

		damage *= 0.5;
	}

	if ( trace->iHitgroup != HITGROUP_HEAD && damageType == DMG_BULLET )
		damage *= 0.3;

	CBaseMonster::TraceAttack( attacker, damage, direction, trace, damageType );
}

float CZombieBarney::GetPointsForDamage( float damage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	float points = pev->max_health / sk_zombie_barney_health.value;

	points *= 2.0f;
	return points * ( damage / pev->max_health );
}

void CZombieBarney::PainSound( void )
{
	ZombiePainSound( this );
}
void CZombieBarney::AlertSound( void )
{
	ZombieAlertSound( this );
}
void CZombieBarney::IdleSound( void )
{
	ZombieIdleSound( this );
}
void CZombieBarney::AttackSound( void )
{
	ZombieAttackSound( this );
}
void CZombieBarney::UseSound( void )
{
	AlertSound();
}
void CZombieBarney::UnUseSound( void )
{
	PainSound();
}

class CZombieSoldier : public CBaseMonster
{
public:
	// the sequence decides the box, standing on the entity's origin
	void SetObjectCollisionBox( void )
	{
		if ( ( pev->spawnflags & SF_MONSTER_NO_MODEL_BOX ) || !m_fCustomModel )
		{
			CBaseEntity::SetObjectCollisionBox();
			return;
		}

		Vector mins = g_vecZero;
		Vector maxs = g_vecZero;

		ExtractBbox( 1, mins, maxs );
		mins.z = 0;
		pev->absmin = pev->origin + mins;
		pev->absmax = pev->origin + maxs;
	}

	void Spawn( void );
	void Precache( void );
	int ObjectCaps( void );
	int Classify( void );
	void SetYawSpeed( void );
	void HandleAnimEvent( MonsterEvent_t *event );
	void RunAI( void );
	int IgnoreConditions( void );
	int TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType );
	void TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType );
	float GetPointsForDamage( float damage );
	BOOL CheckRangeAttack1( float dot, float distance ) { return FALSE; }
	BOOL CheckRangeAttack2( float dot, float distance ) { return FALSE; }
	void PainSound( void );
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );
	void UseSound( void );
	void UnUseSound( void );

	float m_flNextFlinch;
};

LINK_ENTITY_TO_CLASS( monster_zombie_soldier, CZombieSoldier );

void CZombieSoldier::Spawn( void )
{
	Precache();

	if ( !SetupModel() )
		UTIL_SetModel( this, "models/zombie_soldier.mdl" );

	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_GREEN;

	if ( pev->health == 0.0f )
		pev->health = sk_zombie_soldier_health.value;

	pev->view_ofs = VEC_VIEW;
	m_flFieldOfView = 0.5f;
	m_MonsterState = MONSTERSTATE_NONE;
	m_afCapability = bits_CAP_DOORS_GROUP;
	m_afMoveShootCap = 0;

	if ( !m_FormattedName )
		m_FormattedName = AllocStringLocal( "Zombie Soldier" );

	MonsterInit();
}

void CZombieSoldier::Precache( void )
{
	CBaseMonster::Precache();

	if ( !pev->model )
		UTIL_PrecacheModel( this, "models/zombie_soldier.mdl" );

	ZombiePrecacheSounds();
}

int CZombieSoldier::ObjectCaps( void )
{
	return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

int CZombieSoldier::Classify( void )
{
	return GetClassification( CLASS_ALIEN_MONSTER );
}

void CZombieSoldier::SetYawSpeed( void )
{
	pev->yaw_speed = 270.0f;
}

void CZombieSoldier::HandleAnimEvent( MonsterEvent_t *event )
{
	ZombieHandleAnimEvent( this, event, sk_zombie_soldier_dmg_one_slash.value, sk_zombie_soldier_dmg_both_slash.value );
}

void CZombieSoldier::RunAI( void )
{
	CBaseMonster::RunAI();
	pev->framerate = 1.0f;

	if ( m_hEnemy != NULL && IsMoving() )
	{
		float distance = ( pev->origin - m_hEnemy->pev->origin ).Length2D();

		pev->framerate = distance < 256.0f ? 1.4f : 1.0f;
	}
}

int CZombieSoldier::IgnoreConditions( void )
{
	return ZombieIgnoreConditions( this, m_flNextFlinch, 3.0f );
}

int CZombieSoldier::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	return CBaseMonster::TakeDamage( inflictor, attacker, damage, damageType );
}

void CZombieSoldier::TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType )
{
	if ( ( trace->iHitgroup == HITGROUP_CHEST || trace->iHitgroup == HITGROUP_STOMACH )
	     && ( damageType & ( DMG_BULLET | DMG_SLASH | DMG_BLAST | ( 1 << 24 ) ) ) )
	{
		if ( damage >= 2.0f )
			damage -= 2.0f;

		damage *= 0.5;
	}

	if ( trace->iHitgroup != HITGROUP_HEAD && ( damageType & DMG_BULLET ) )
		damage *= 0.3;

	CBaseMonster::TraceAttack( attacker, damage, direction, trace, damageType );
}

float CZombieSoldier::GetPointsForDamage( float damage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	float points = pev->max_health / sk_zombie_soldier_health.value;

	points *= 3.0f;
	return points * ( damage / pev->max_health );
}

void CZombieSoldier::PainSound( void )
{
	ZombiePainSound( this );
}
void CZombieSoldier::AlertSound( void )
{
	ZombieAlertSound( this );
}
void CZombieSoldier::IdleSound( void )
{
	ZombieIdleSound( this );
}
void CZombieSoldier::AttackSound( void )
{
	ZombieAttackSound( this );
}
void CZombieSoldier::UseSound( void )
{
	AlertSound();
}
void CZombieSoldier::UnUseSound( void )
{
	PainSound();
}
