#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "effects.h"
#include "weapons.h"
#include "schedule.h"
#include "soundent.h"
#include "animation.h"
#include "customentity.h"
#include "user_messages.h"
#include "server_support.h"
#include "InventoryRules.h"
#include "sporegrenade.h"
#include "squeakgrenade.h"

#define SF_KINGPIN_NO_ROUTE_TELEPORT		0x400
#define SF_TRIGGER_HURT_CLIENTONLYTOUCH	32

extern cvar_t sk_kingpin_health;
extern cvar_t sk_kingpin_lightning;
extern cvar_t sk_kingpin_tele_blast;
extern cvar_t sk_kingpin_plasma_blast;
extern cvar_t sk_kingpin_melee;
extern cvar_t sk_kingpin_telefrag;

class CKingpinTeleportCheck : public CBaseEntity
{
public:
	void Spawn( void );
	bool CanBlockTeleport( CBaseEntity *other );
	void EXPORT TeleportTestTouch( CBaseEntity *other );

	bool	m_fBlocked;
	EHANDLE	m_hBlocker;
};

class CKingpinCrawlerBeam : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void Assign( CBaseEntity *target );
	void Assign( const Vector &position );
	void SetRadius( float radius ) { m_flRadius = radius; }
	void SetDecayRate( float decay ) { m_flDecayRate = decay; }
	void EXPORT CrawlerThink( void );
	virtual float GetSoundVolue( void ) { return 1.0f; }

private:
	EHANDLE	m_hOwner;
	EHANDLE	m_hTarget;
	Vector	m_vecTarget;
	EHANDLE	m_hPreviousTargets[3];
	int		m_iPreviousTarget;
	bool	m_fAssigned;
	float	m_flPower;
	float	m_flDecayRate;
	float	m_flRadius;
	int		m_iFlareSprite;
};

class CKingpinPlasmaBall : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	CBaseEntity *BestVisibleEnemy( void );
	void EXPORT HuntThink( void );
	void EXPORT ExplodeTouch( CBaseEntity *other );
	void Die( void );
	virtual float GetSoundVolue( void ) { return 1.0f; }

private:
	float	m_flDieTime;
	EHANDLE	m_hOwner;
	int		m_iTrailSprite;
	int		m_iShockWaveSprite;
	int		m_iFlareSprite;
};

class CKingpin : public CBaseMonster
{
public:
	void CheckAndApplyGenericAttacks( void ) { CBaseMonster::CheckAndApplyGenericAttacks(); }
	void Spawn( void );
	void Precache( void );
	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int ObjectCaps( void ) { return m_fPlayerAlly ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION; }
	int Classify( void ) { return GetClassification( CLASS_ALIEN_MILITARY ); }
	void SetYawSpeed( void ) { pev->yaw_speed = 100; }
	BOOL FCanCheckAttacks( void ) { return !HasConditions( bits_COND_ENEMY_OCCLUDED ); }
	int IgnoreConditions( void ) { return CBaseMonster::IgnoreConditions() | bits_COND_LIGHT_DAMAGE; }
	BOOL CheckRangeAttack2( float dot, float distance );
	float GetPointsForDamage( float damage );
	virtual float GetDamageAmount( void ) { return sk_kingpin_melee.value; }
	virtual int GetVoicePitch( void ) { return 100; }
	virtual float GetSoundVolue( void ) { return 1.0f; }
	void TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType );
	int TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType );
	void Killed( entvars_t *attacker, int gib );
	void GibMonster( void );
	void UpdateOnRemove( void );
	void MonsterThink( void );
	void Move( float interval );
	void HandleAnimEvent( MonsterEvent_t *event );
	void StartTask( Task_t *task );
	void RunTask( Task_t *task );
	Schedule_t *GetScheduleOfType( int type );
	void PainSound( void );
	void AlertSound( void );
	void IdleSound( void );
	void DeathSound( void );
	CUSTOM_SCHEDULES;

private:
	void RemoveEffects( bool remove );
	void ResolveRenderProps( void );
	void ShieldOn( void );
	void ShieldOff( void );
	void RemoveDeflectBeam( void );
	void MakeDeflectTarget( CBaseEntity *target );
	void CreateDetBeam( CBaseEntity *target );
	bool TryToDetonateTarget( CBaseEntity *target );
	void FindNewProjectileThreats( void );
	void DeflectStuff( void );
	void ChargeEyes( void );
	void LightningAttack( int eye );
	void DoShockWave( Vector position, int red, int green, int blue );

	int		m_iBlastSprite;
	int		m_iShockWaveSprite;
	int		m_iLightningSprite;
	int		m_iFlareSprite;

	Vector	m_vecTeleportTarget;
	EHANDLE	m_hTeleportCheck;
	float	m_flTeleportStartTime;
	bool	m_fTeleportEffect;
	int		m_iPreviousRoute;

	EHANDLE	m_hDeflectTarget;
	EHANDLE	m_hDeflectBeam;

	int		m_iRestoreRenderMode;
	int		m_iRestoreRenderFX;
	float	m_flRestoreRenderAmount;
	Vector	m_vecRestoreRenderColor;
	int		m_iRestoreRenderFlags;
	bool	m_fShieldEnabled;
	float	m_flShieldCharge;
	bool	m_fShieldRendered;
	float	m_flShieldHitTime;

	EHANDLE	m_hEyeGlow[4];
	float	m_flEyeChargeTime[4];
	float	m_flLastShockWaveTime;

	int		SelectEyeToFire( float flForward, float flRight );
};

LINK_ENTITY_TO_CLASS( monster_kingpin, CKingpin );

// The first charged eye, leaving out the one facing away from the target
int CKingpin::SelectEyeToFire( float flForward, float flRight )
{
	int excluded = flRight >= 0 ? 1 : 0;

	if ( flForward >= 0 )
		excluded += 2;

	for ( int i = 0; i < ARRAYSIZE( m_flEyeChargeTime ); i++ )
	{
		if ( i != excluded && gpGlobals->time > m_flEyeChargeTime[i] && m_flEyeChargeTime[i] != 0 )
			return i;
	}

	return -1;
}
LINK_ENTITY_TO_CLASS( kingpin_teleport_check, CKingpinTeleportCheck );
LINK_ENTITY_TO_CLASS( kingpin_crawler_beam, CKingpinCrawlerBeam );
LINK_ENTITY_TO_CLASS( kingpin_plasma_ball, CKingpinPlasmaBall );

static const char *pAttackSounds[] = {
	"controller/con_attack1.wav", "controller/con_attack2.wav", "controller/con_attack3.wav"
};
static const char *pHitSounds[] = {
	"zombie/claw_strike1.wav", "zombie/claw_strike2.wav", "zombie/claw_strike3.wav"
};
static const char *pMissSounds[] = {
	"zombie/claw_miss1.wav", "zombie/claw_miss2.wav"
};
static const char *pIdleSounds[] = {
	"kingpin/kingpin_idle1.wav", "kingpin/kingpin_idle2.wav", "kingpin/kingpin_idle3.wav"
};
static const char *pAlertSounds[] = {
	"kingpin/kingpin_alert1.wav", "kingpin/kingpin_alert2.wav", "kingpin/kingpin_alert3.wav"
};
static const char *pPainSounds[] = {
	"kingpin/kingpin_pain1.wav", "kingpin/kingpin_pain2.wav", "kingpin/kingpin_pain3.wav"
};
static const char *pDeathSounds[] = {
	"kingpin/kingpin_death1.wav", "kingpin/kingpin_death2.wav"
};

enum
{
	TASK_KINGPIN_START_TELEPORT = LAST_COMMON_TASK + 1,
	TASK_KINGPIN_CHECK_TELEPORT,
	TASK_KINGPIN_TELEPORT,
	TASK_KINGPIN_CHECK_ARRIVAL,
	TASK_KINGPIN_FIND_VANTAGE_POINT,
	TASK_KINGPIN_PLASMA_BALL
};

Task_t tlKingpinTeleport[] = {
	{ TASK_KINGPIN_CHECK_TELEPORT, 0 },
	{ TASK_STOP_MOVING, 0 },
	{ TASK_KINGPIN_START_TELEPORT, 0 },
	{ TASK_KINGPIN_TELEPORT, 0 },
	{ TASK_KINGPIN_CHECK_ARRIVAL, 0 }
};
Schedule_t slKingpinTeleport[] = {
	{ tlKingpinTeleport, ARRAYSIZE( tlKingpinTeleport ), bits_COND_TASK_FAILED, 0, "KingpinTeleport" }
};
Task_t tlKingpinWakeAngry[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SOUND_WAKE, 0 }
};
Schedule_t slKingpinWakeAngry[] = {
	{
		tlKingpinWakeAngry,
		ARRAYSIZE( tlKingpinWakeAngry ),
		0,
		0,
		"Kingpin Wake Angry No Turn"
	}
};
Task_t tlKingpinVantagePoint[] = {
	{ TASK_KINGPIN_FIND_VANTAGE_POINT, 0 },
	{ TASK_KINGPIN_CHECK_TELEPORT, 0 },
	{ TASK_STOP_MOVING, 0 },
	{ TASK_KINGPIN_START_TELEPORT, 0 },
	{ TASK_KINGPIN_TELEPORT, 0 },
	{ TASK_KINGPIN_CHECK_ARRIVAL, 0 }
};
Schedule_t slKingpinVantagePoint[] = {
	{ tlKingpinVantagePoint, ARRAYSIZE( tlKingpinVantagePoint ), bits_COND_TASK_FAILED, 0, "KingpinTeleportToNewVantagePoint" }
};
Task_t tlKingpinPlasmaBall[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_KINGPIN_PLASMA_BALL, 0 }
};
Schedule_t slKingpinPlasmaBall[] = {
	{
		tlKingpinPlasmaBall,
		ARRAYSIZE( tlKingpinPlasmaBall ),
		0,
		0,
		"Kingpin Ball Attack"
	}
};

DEFINE_CUSTOM_SCHEDULES( CKingpin ){
	slKingpinTeleport,
	slKingpinWakeAngry,
	slKingpinVantagePoint,
	slKingpinPlasmaBall,
};

IMPLEMENT_CUSTOM_SCHEDULES( CKingpin, CBaseMonster );

TYPEDESCRIPTION CKingpin::m_SaveData[] = {
	DEFINE_FIELD( CKingpin, m_flTeleportStartTime, FIELD_TIME ),
	DEFINE_FIELD( CKingpin, m_vecTeleportTarget, FIELD_POSITION_VECTOR ),
};

IMPLEMENT_SAVERESTORE( CKingpin, CBaseMonster );

void CKingpin::Precache( void )
{
	CBaseEntity::Precache();

	if ( !pev->model )
		UTIL_PrecacheModel( this, "models/kingpin.mdl" );

	m_iBlastSprite = UTIL_PrecacheModel( this, "sprites/nhth1.spr" );
	UTIL_PrecacheModel( this, "sprites/lgtning.spr" );
	UTIL_PrecacheModel( this, "sprites/b-tele1.spr" );
	UTIL_PrecacheModel( this, "sprites/lgtning.spr" );
	m_iShockWaveSprite = UTIL_PrecacheModel( this, "sprites/shockwave.spr" );
	UTIL_PrecacheModel( this, "sprites/boss_glow.spr" );
	m_iFlareSprite = UTIL_PrecacheModel( this, "sprites/flare2.spr" );
	m_iLightningSprite = UTIL_PrecacheModel( this, "sprites/kingpin_beam.spr" );
	UTIL_PrecacheModel( this, "sprites/laserbeam.spr" );
	UTIL_PrecacheModel( this, "sprites/muz4.spr" );
	UTIL_PrecacheModel( this, "models/stickygibpink.mdl" );

	for ( int i = 0; i < ARRAYSIZE( pAttackSounds ); i++ )
		PrecacheSound_Intercept( this, pAttackSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pHitSounds ); i++ )
		PrecacheSound_Intercept( this, pHitSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pMissSounds ); i++ )
		PrecacheSound_Intercept( this, pMissSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pIdleSounds ); i++ )
		PrecacheSound_Intercept( this, pIdleSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pAlertSounds ); i++ )
		PrecacheSound_Intercept( this, pAlertSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pPainSounds ); i++ )
		PrecacheSound_Intercept( this, pPainSounds[i] );

	for ( int i = 0; i < ARRAYSIZE( pDeathSounds ); i++ )
		PrecacheSound_Intercept( this, pDeathSounds[i] );

	PrecacheSound_Intercept( this, "kingpin/port_suckout1.wav" );
	PrecacheSound_Intercept( this, "kingpin/port_suckin1.wav" );
	PrecacheSound_Intercept( this, "debris/beamstart10.wav" );
	UTIL_PrecacheOther( "kingpin_crawler_beam" );
	UTIL_PrecacheOther( "kingpin_plasma_ball" );
}

float CKingpin::GetPointsForDamage( float damage )
{
	return ( pev->max_health / sk_kingpin_health.value ) * 3.0f * ( damage / pev->max_health );
}

BOOL CKingpin::CheckRangeAttack2( float dot, float distance )
{
	return gpGlobals->time - m_flLastShockWaveTime >= 10.0f && FVisible( m_vecEnemyLKP );
}

void CKingpin::ShieldOn( void )
{
	if ( !m_fShieldEnabled && m_flShieldCharge > 0 )
	{
		m_fShieldEnabled = true;
		ResolveRenderProps();
	}
}

void CKingpin::ShieldOff( void )
{
	if ( m_fShieldEnabled )
	{
		m_fShieldEnabled = false;
		ResolveRenderProps();
	}
}

void CKingpin::ResolveRenderProps( void )
{
	m_fShieldRendered = false;

	if ( m_pSchedule && GetTask() && GetTask()->iTask == TASK_KINGPIN_START_TELEPORT )
	{
		pev->rendermode = kRenderTransAdd;
		pev->renderfx = kRenderFxNone;
	}
	else if ( m_fShieldEnabled && m_flShieldHitTime >= gpGlobals->time )
	{
		m_fShieldRendered = true;
		pev->renderfx = kRenderFxGlowShell;
		pev->renderamt = m_flShieldCharge;
		pev->rendercolor = Vector( 100, 100, 255 );

		if ( m_iRestoreRenderFlags & 1 )
			pev->rendermode = m_iRestoreRenderMode;
	}
	else
	{
		if ( m_iRestoreRenderFlags & 1 )
			pev->rendermode = m_iRestoreRenderMode;

		if ( m_iRestoreRenderFlags & 2 )
			pev->renderfx = m_iRestoreRenderFX;

		if ( m_iRestoreRenderFlags & 4 )
			pev->renderamt = m_flRestoreRenderAmount;

		if ( m_iRestoreRenderFlags & 8 )
			pev->rendercolor = m_vecRestoreRenderColor;
	}
}

void CKingpin::TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType )
{
	if ( m_fShieldEnabled )
	{
		trace->iHitgroup = 0;
		Vector position = trace->vecEndPos - direction * m_flShieldCharge * 0.0625f;

		MESSAGE_BEGIN( MSG_PVS, gmsgShieldRic, position );
		WRITE_COORD( position.x );
		WRITE_COORD( position.y );
		WRITE_COORD( position.z );
		MESSAGE_END();
	}

	CBaseMonster::TraceAttack( attacker, damage, direction, trace, damageType );
}

int CKingpin::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	if ( m_fShieldEnabled )
	{
		m_flShieldHitTime = gpGlobals->time + 0.5f;

		if ( !m_fShieldRendered )
			ResolveRenderProps();

		if ( damage > 0 && !( damageType & ( DMG_DROWN | DMG_FALL ) ) )
		{
			float ratio = ( damageType & DMG_BLAST ) ? 0.4f : 0.2f;
			float absorbed = ( damage - damage * 0.01f ) * ratio;

			if ( absorbed <= m_flShieldCharge )
			{
				m_flShieldCharge -= absorbed;
				damage *= 0.01f;
			}
			else
			{
				damage -= ( 1.0f / ratio ) * m_flShieldCharge;
				m_flShieldCharge = 0;
				ShieldOff();
			}

			if ( m_fShieldRendered )
				pev->renderamt = m_flShieldCharge;
		}
	}

	return CBaseMonster::TakeDamage( inflictor, attacker, damage * 0.75f, damageType );
}

void CKingpin::Killed( entvars_t *attacker, int gib )
{
	RemoveEffects( false );
	CBaseMonster::Killed( attacker, gib );
}

void CKingpin::UpdateOnRemove( void )
{
	CBaseEntity::UpdateOnRemove();
	RemoveEffects( true );
}

void CKingpin::GibMonster( void )
{
	Vector position = Center();

	MESSAGE_BEGIN( MSG_ALL, gmsgGib );
	WRITE_BYTE( 4 );
	WRITE_COORD( position.x );
	WRITE_COORD( position.y );
	WRITE_COORD( position.z );
	WRITE_COORD( 600 );
	WRITE_COORD( 600 );
	WRITE_COORD( 600 );
	MESSAGE_END();

	CBaseMonster::GibMonster();
}

Schedule_t *CKingpin::GetScheduleOfType( int type )
{
	if ( type == SCHED_WAKE_ANGRY )
		return slKingpinWakeAngry;

	if ( type == SCHED_RANGE_ATTACK2 )
		return slKingpinPlasmaBall;

	return CBaseMonster::GetScheduleOfType( type );
}

void CKingpin::Spawn( void )
{
	Precache();

	if ( !SetupModel() )
		UTIL_SetModel( this, "models/kingpin.mdl" );

	UTIL_SetSize( pev, Vector( -24, -24, 0 ), Vector( 24, 24, 112 ) );
	UTIL_SetOrigin( pev, pev->origin );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_YELLOW;

	if ( pev->health == 0 )
		pev->health = sk_kingpin_health.value;

	m_afCapability |= bits_CAP_RANGE_ATTACK1 | bits_CAP_RANGE_ATTACK2;
	m_flFieldOfView = -1;
	m_MonsterState = MONSTERSTATE_NONE;

	if ( !m_FormattedName )
		m_FormattedName = AllocStringLocal( m_fPlayerAlly ? "Friendly Kingpin" : "Kingpin" );

	MonsterInit();

	if ( m_fPlayerAlly && !( pev->spawnflags & SF_MONSTER_PREDISASTER ) )
	{
		SetUse( &CBaseMonster::MonsterUse );
		m_fCanPlayerFollow = true;
	}

	m_flDistTooFar = 2048;
	m_vecTeleportTarget = g_vecZero;
	m_hTeleportCheck = NULL;
	m_flTeleportStartTime = 0;
	m_fTeleportEffect = false;
	m_iPreviousRoute = -1;
	m_hDeflectTarget = NULL;
	m_hDeflectBeam = NULL;
	m_iRestoreRenderMode = pev->rendermode;
	m_iRestoreRenderFX = pev->renderfx;
	m_flRestoreRenderAmount = pev->renderamt;
	m_vecRestoreRenderColor = pev->rendercolor;
	m_iRestoreRenderFlags = 15;
	m_fShieldEnabled = false;
	m_flShieldCharge = 255;
	m_fShieldRendered = false;
	m_flShieldHitTime = 0;

	for ( int i = 0; i < 4; i++ )
	{
		CSprite *glow = CSprite::SpriteCreate( "sprites/boss_glow.spr", pev->origin, FALSE );

		m_hEyeGlow[i] = glow;
		UTIL_SetOrigin( glow->pev, pev->origin );
		glow->SetAttachment( edict(), i + 1 );
		glow->pev->framerate = 10;
		glow->TurnOn();
		glow->SetTransparency( kRenderTransAdd, 255, 255, 255, 0, kRenderFxNone );
		glow->SetScale( 0.3f );
		m_flEyeChargeTime[i] = 0;
	}

	m_flLastShockWaveTime = 0;
}

void CKingpin::PainSound( void )
{
	if ( RANDOM_LONG( 0, 5 ) < 2 )
	{
		int pitch = GetVoicePitch();
		float volume = GetSoundVolue();

		EMIT_SOUND_DYN( edict(), CHAN_VOICE, pPainSounds[RANDOM_LONG( 0, ARRAYSIZE( pPainSounds ) - 1 )], volume, ATTN_NORM, 0, pitch );
	}
}

void CKingpin::AlertSound( void )
{
	int pitch = GetVoicePitch();
	float volume = GetSoundVolue();

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, pAlertSounds[RANDOM_LONG( 0, ARRAYSIZE( pAlertSounds ) - 1 )], volume, ATTN_NORM, 0, pitch );
}

void CKingpin::IdleSound( void )
{
	int pitch = GetVoicePitch();
	float volume = GetSoundVolue();

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, pIdleSounds[RANDOM_LONG( 0, ARRAYSIZE( pIdleSounds ) - 1 )], volume, ATTN_NORM, 0, pitch );
}

void CKingpin::DeathSound( void )
{
	int pitch = GetVoicePitch();
	float volume = GetSoundVolue();

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, pDeathSounds[RANDOM_LONG( 0, ARRAYSIZE( pDeathSounds ) - 1 )], volume, ATTN_IDLE, 0, pitch );
}

void CKingpin::RemoveEffects( bool remove )
{
	for ( int i = 0; i < 4; i++ )
	{
		m_flEyeChargeTime[i] = 0;

		if ( m_hEyeGlow[i] )
		{
			if ( remove )
				UTIL_Remove( m_hEyeGlow[i] );
			else
				m_hEyeGlow[i]->SUB_StartFadeOut();

			m_hEyeGlow[i] = NULL;
		}
	}

	RemoveDeflectBeam();
}

void CKingpin::RemoveDeflectBeam( void )
{
	if ( m_hDeflectBeam )
	{
		UTIL_Remove( m_hDeflectBeam );
		m_hDeflectBeam = NULL;
	}
}

void CKingpin::MakeDeflectTarget( CBaseEntity *target )
{
	RemoveDeflectBeam();
	m_hDeflectTarget = target;

	CBeam *beam = CBeam::BeamCreate( "sprites/laserbeam.spr", 20 );

	m_hDeflectBeam = beam;
	beam->EntsInit( entindex(), target->entindex() );
	beam->SetColor( 255, 255, 255 );
	beam->SetBrightness( 255 );
	beam->SetFlags( BEAM_FSHADEIN );
	beam->SetNoise( 100 );
	beam->SetScrollRate( 50 );
}

void CKingpin::CreateDetBeam( CBaseEntity *target )
{
	CBeam *beam = CBeam::BeamCreate( "sprites/laserbeam.spr", 50 );

	beam->PointsInit( target->pev->origin, Center() );
	beam->SetColor( 255, 255, 255 );
	beam->SetBrightness( 255 );
	beam->SetFlags( BEAM_FSHADEOUT );
	beam->SetNoise( 50 );
	beam->SetScrollRate( 50 );
	beam->SetThink( &CBaseEntity::SUB_Remove );
	beam->pev->nextthink = gpGlobals->time + 0.2f;
}

bool CKingpin::TryToDetonateTarget( CBaseEntity *target )
{
	target->pev->owner = edict();

	if ( FClassnameIs( target->pev, "grenade" ) || FClassnameIs( target->pev, "rpg_rocket" ) || FClassnameIs( target->pev, "monster_satchel" ) )
	{
		CreateDetBeam( target );
		dynamic_cast<CGrenade *>( target )->Detonate();
		return true;
	}

	if ( FClassnameIs( target->pev, "sporegrenade" ) )
	{
		CreateDetBeam( target );
		dynamic_cast<CSporeGrenade *>( target )->Detonate();
		return true;
	}

	if ( FClassnameIs( target->pev, "monster_snark" ) )
	{
		CreateDetBeam( target );

		EHANDLE handle;

		handle = target;

		target->TakeDamage( pev, pev, 100, DMG_GENERIC );

		if ( !handle || handle->pev->health <= 0 )
			return true;
	}

	MakeDeflectTarget( target );
	return false;
}

void CKingpin::DeflectStuff( void )
{
	if ( !m_hDeflectTarget )
	{
		RemoveDeflectBeam();
		FindNewProjectileThreats();
	}

	if ( !m_hDeflectTarget )
		return;

	CBaseEntity *target = m_hDeflectTarget;
	Vector offset = target->pev->origin - pev->origin;
	float distance = offset.Length();

	if ( distance > 256 )
	{
		RemoveDeflectBeam();
		m_hDeflectTarget = NULL;
		return;
	}

	Vector force = offset / distance * gpGlobals->frametime * 2000000.0f / max( distance, 10.0f );

	force.z = max( force.z, 50.0f );
	target->pev->velocity = target->pev->velocity + force;
	target->pev->flags &= ~FL_ONGROUND;

	if ( FClassnameIs( target->pev, "rpg_rocket" ) )
		target->pev->angles = UTIL_VecToAngles( target->pev->velocity );
}

void CKingpin::ChargeEyes( void )
{
	for ( int i = 0; i < 4; i++ )
	{
		if ( !m_hEyeGlow[i] )
			continue;

		if ( m_flEyeChargeTime[i] == 0 )
		{
			m_hEyeGlow[i]->pev->renderamt = 0;

			if ( m_MonsterState == MONSTERSTATE_COMBAT )
				m_flEyeChargeTime[i] = gpGlobals->time + 4;
		}
		else if ( m_MonsterState != MONSTERSTATE_COMBAT )
		{
			m_flEyeChargeTime[i] = 0;
			m_hEyeGlow[i]->pev->renderamt = 0;
		}
		else
		{
			float remaining = m_flEyeChargeTime[i] - gpGlobals->time;

			if ( remaining > 4 )
				m_hEyeGlow[i]->pev->renderamt = 0;
			else if ( remaining >= 0 )
				m_hEyeGlow[i]->pev->renderamt = ( 1 - remaining * 0.25f ) * 255;
			else
				m_hEyeGlow[i]->pev->renderamt = 255;
		}
	}
}

void CKingpin::Move( float interval )
{
	if ( !( pev->spawnflags & SF_KINGPIN_NO_ROUTE_TELEPORT ) && m_MonsterState == MONSTERSTATE_COMBAT && m_iPreviousRoute != m_iRouteIndex )
	{
		m_iPreviousRoute = m_iRouteIndex;

		if ( ( m_Route[m_iRouteIndex].vecLocation - pev->origin ).Length() > 768 && FVisible( m_Route[m_iRouteIndex].vecLocation ) )
		{
			m_vecTeleportTarget = m_Route[m_iRouteIndex].vecLocation;
			ChangeSchedule( slKingpinTeleport );
			return;
		}
	}

	CBaseMonster::Move( interval );
}

void CKingpin::MonsterThink( void )
{
	ChargeEyes();
	DeflectStuff();

	if ( m_fShieldRendered && gpGlobals->time > m_flShieldHitTime )
		ResolveRenderProps();

	if ( m_MonsterState == MONSTERSTATE_COMBAT )
	{
		ShieldOn();

		if ( m_hEnemy && FVisible( m_hEnemy, true ) )
		{
			Vector direction = ( m_hEnemy->pev->origin - pev->origin ).Normalize();

			UTIL_MakeVectors( pev->angles );

			int eye = SelectEyeToFire( DotProduct( direction, gpGlobals->v_forward ), DotProduct( direction, gpGlobals->v_right ) );

			if ( eye >= 0 )
			{
				LightningAttack( eye );
				m_flEyeChargeTime[eye] = 0;
			}
		}
	}
	else
	{
		ShieldOff();
	}

	CBaseMonster::MonsterThink();
}

void CKingpin::HandleAnimEvent( MonsterEvent_t *event )
{
	switch ( event->event )
	{
		case 1:
		case 2:
		{
			CBaseEntity *hit = CheckTraceHullAttack( 100, (int) sk_kingpin_melee.value, DMG_SLASH );

			if ( hit )
			{
				if ( ( hit->pev->flags & FL_CLIENT ) || ( ( hit->pev->flags & FL_MONSTER ) && hit->pev->movetype != MOVETYPE_FLY ) )
				{
					hit->pev->punchangle.x = -20;
					hit->pev->punchangle.z = 10;
					hit->pev->flags &= ~FL_ONGROUND;
					hit->pev->velocity = hit->pev->velocity + gpGlobals->v_forward * 100 + gpGlobals->v_up * 200
						+ gpGlobals->v_right * ( event->event == 1 ? 200 : -200 );
				}

				int pitch = RANDOM_LONG( -95, 105 );

				EMIT_SOUND_DYN( edict(), CHAN_WEAPON, pHitSounds[RANDOM_LONG( 0, ARRAYSIZE( pHitSounds ) - 1 )], 1, ATTN_NORM, 0, pitch );
			}
			else
			{
				int pitch = RANDOM_LONG( -95, 105 );

				EMIT_SOUND_DYN( edict(), CHAN_WEAPON, pMissSounds[RANDOM_LONG( 0, ARRAYSIZE( pMissSounds ) - 1 )], 1, ATTN_NORM, 0, pitch );
			}
			break;
		}
		case 3:
		{
			m_flLastShockWaveTime = gpGlobals->time;
			UTIL_MakeVectors( pev->angles );

			Vector position = pev->origin + gpGlobals->v_forward * 32 + gpGlobals->v_up * 64;

			MESSAGE_BEGIN( MSG_PVS, gmsgRampSprite, position );
			WRITE_SHORT( m_iBlastSprite );
			WRITE_BYTE( 15 );
			WRITE_COORD( position.x );
			WRITE_COORD( position.y );
			WRITE_COORD( position.z );
			WRITE_SHORT( 33806 );
			WRITE_BYTE( 3 );
			WRITE_BYTE( 255 );
			WRITE_BYTE( 160 );
			WRITE_BYTE( 10 );
			WRITE_BYTE( 10 );
			MESSAGE_END();
			break;
		}
		case 4:
			pev->framerate = 0;
			break;

		case 5:
			break;

		default:
			CBaseMonster::HandleAnimEvent( event );
			break;
	}
}

void CKingpinTeleportCheck::Spawn( void )
{
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_FLY;
	UTIL_SetModel( this, STRING( pev->model ) );
	pev->effects |= EF_NODRAW;
	UTIL_SetSize( pev, Vector( -24, -24, 0 ), Vector( 24, 24, 112 ) );
	UTIL_SetOrigin( pev, pev->origin );
	SetTouch( &CKingpinTeleportCheck::TeleportTestTouch );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 1;
	m_fBlocked = false;
	m_hBlocker = NULL;
}

void CKingpinTeleportCheck::TeleportTestTouch( CBaseEntity *other )
{
	if ( CanBlockTeleport( other ) )
	{
		m_fBlocked = true;
		m_hBlocker = other;
		pev->solid = SOLID_NOT;
	}
}

bool CKingpinTeleportCheck::CanBlockTeleport( CBaseEntity *other )
{
	if ( other->edict() == pev->owner )
		return true;

	if ( ( pev->flags & FL_MONSTERCLIP )
		&& ( FClassnameIs( other->pev, "func_monsterclip" ) || FClassnameIs( other->pev, "func_clip" ) )
		&& other->pev->solid != SOLID_NOT && ( other->pev->flags & FL_MONSTERCLIP ) )
		return true;

	if ( other->pev->solid != SOLID_NOT && other->pev->solid != SOLID_TRIGGER )
	{
		if ( other->pev->takedamage != DAMAGE_NO && !( other->pev->flags & FL_GODMODE ) )
		{
			if ( other->pev->health > sk_kingpin_telefrag.value || FClassnameIs( other->pev, "xen_tree" ) || strstr( STRING( other->pev->classname ), "xen_spore" ) )
				return true;

			static const char *noTelefrag[] = {
				"monster_apache", "monster_blkop_apache", "monster_osprey", "monster_blkop_osprey",
				"monster_gargantua", "monster_bigmomma", "monster_voltigore", "monster_tentacle",
				"monster_nihilanth", "monster_ichthyosaur", "monster_sentry", "monster_miniturret", "monster_turret"
			};

			for ( int i = 0; i < ARRAYSIZE( noTelefrag ); i++ )
			{
				if ( FClassnameIs( other->pev, noTelefrag[i] ) )
					return true;
			}

			return false;
		}

		return !FClassnameIs( other->pev, "kingpin_energy_ball" );
	}

	CBaseToggle *trigger = dynamic_cast<CBaseToggle *>( other );

	if ( trigger && trigger->pev->solid == SOLID_TRIGGER && pev->takedamage != DAMAGE_NO
		&& FClassnameIs( trigger->pev, "trigger_hurt" ) && !( trigger->pev->spawnflags & SF_TRIGGER_HURT_CLIENTONLYTOUCH ) && trigger->pev->dmg > 0 )
		return !trigger->m_pInventoryRules->CanUseTrigger( this, true );

	return false;
}

void CKingpinCrawlerBeam::Precache( void )
{
	m_iFlareSprite = UTIL_PrecacheModel( this, "sprites/muz7.spr" );
	UTIL_PrecacheModel( this, "sprites/lgtning.spr" );
	PrecacheSound_Intercept( this, "debris/beamstart14.wav" );
}

void CKingpinCrawlerBeam::Spawn( void )
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	m_hOwner = CBaseEntity::Instance( pev->owner );
	m_hTarget = NULL;
	m_vecTarget = g_vecZero;

	for ( int i = 0; i < 3; i++ )
		m_hPreviousTargets[i] = NULL;

	m_iPreviousTarget = 0;
	m_fAssigned = false;
	m_flPower = 1;
	m_flDecayRate = 0.9f;
	m_flRadius = 256;
}

void KingpinCrawlerFlare( int sprite, const Vector &position )
{
	MESSAGE_BEGIN( MSG_PVS, gmsgRampSprite, position );
	WRITE_SHORT( sprite );
	WRITE_BYTE( 5 );
	WRITE_COORD( position.x );
	WRITE_COORD( position.y );
	WRITE_COORD( position.z );
	WRITE_SHORT( 3086 );
	WRITE_BYTE( 10 );
	WRITE_BYTE( 255 );
	WRITE_BYTE( 110 );
	WRITE_BYTE( 127 );
	WRITE_BYTE( -127 );
	MESSAGE_END();
}

void CKingpinCrawlerBeam::Assign( CBaseEntity *target )
{
	m_fAssigned = true;
	m_hTarget = target;

	if ( m_hTarget )
		m_vecTarget = m_hTarget->Center();

	m_hPreviousTargets[m_iPreviousTarget] = target;

	m_iPreviousTarget++;

	if ( m_iPreviousTarget >= 3 )
		m_iPreviousTarget = 0;

	SetThink( &CKingpinCrawlerBeam::CrawlerThink );
	pev->nextthink = gpGlobals->time;
	KingpinCrawlerFlare( m_iFlareSprite, m_vecTarget );
}

void CKingpinCrawlerBeam::Assign( const Vector &position )
{
	m_fAssigned = true;
	m_vecTarget = position;
	SetThink( &CKingpinCrawlerBeam::CrawlerThink );
	pev->nextthink = gpGlobals->time;
	KingpinCrawlerFlare( m_iFlareSprite, m_vecTarget );
}

void CKingpinCrawlerBeam::CrawlerThink( void )
{
	if ( !m_fAssigned )
	{
		UTIL_Remove( this );
		return;
	}

	float delay = RANDOM_FLOAT( 0.1f, 0.15f );
	Vector start = m_hTarget ? m_hTarget->Center() : m_vecTarget;
	edict_t *ignore = m_hTarget ? m_hTarget->edict() : NULL;
	CBaseEntity *target = NULL;

	while ( ( target = UTIL_FindEntityInSphere( target, start, m_flRadius ) ) != NULL )
	{
		if ( target->pev->takedamage == DAMAGE_NO || target->pev->health <= 0 )
			continue;

		if ( m_hOwner && ( target == (CBaseEntity *) m_hOwner || m_hOwner->IRelationship( target ) <= R_NO ) )
			continue;

		if ( ( target->pev->flags & FL_MONSTER ) && target->pev->deadflag == DEAD_DEAD )
			continue;

		int previous;

		for ( previous = 0; previous < 3; previous++ )
		{
			if ( target == (CBaseEntity *) m_hPreviousTargets[previous] )
				break;
		}

		if ( previous != 3 )
			continue;

		Vector end = target->Center();
		TraceResult trace;

		UTIL_TraceLine( start, end, ignore_monsters, ignore, &trace );

		if ( trace.flFraction != 1 && trace.pHit != target->edict() )
			continue;

		target->TakeDamage( pev, m_hOwner ? m_hOwner->pev : pev, m_flPower * sk_kingpin_lightning.value, DMG_SHOCK );
		m_flPower *= m_flDecayRate;

		CBeam *beam = CBeam::BeamCreate( "sprites/lgtning.spr", (int) ( m_flPower * 50 ) );

		if ( beam )
		{
			beam->PointsInit( start, end );
			beam->SetBrightness( 255 );
			beam->SetNoise( 50 );
			beam->SetScrollRate( 50 );
			beam->SetColor( 200, 150, 255 );
			beam->SetThink( &CBaseEntity::SUB_Remove );
			beam->pev->nextthink = gpGlobals->time + delay + delay;
		}

		int pitch = RANDOM_LONG( 90, 100 );

		EMIT_SOUND_DYN( edict(), CHAN_AUTO, "debris/beamstart14.wav", GetSoundVolue(), ATTN_NORM, 0, pitch );
		KingpinCrawlerFlare( m_iFlareSprite, end );

		if ( m_flPower < 0.5f )
		{
			UTIL_Remove( this );
			return;
		}

		m_hTarget = target;
		m_vecTarget = end;
		m_hPreviousTargets[m_iPreviousTarget] = target;

		m_iPreviousTarget++;

		if ( m_iPreviousTarget >= 3 )
			m_iPreviousTarget = 0;

		pev->nextthink = gpGlobals->time + delay;
		return;
	}

	UTIL_Remove( this );
}

void CKingpinPlasmaBall::Precache( void )
{
	UTIL_PrecacheModel( this, "sprites/nhth1.spr" );
	m_iTrailSprite = UTIL_PrecacheModel( this, "sprites/laserbeam.spr" );
	m_iShockWaveSprite = UTIL_PrecacheModel( this, "sprites/shockwave.spr" );
	m_iFlareSprite = UTIL_PrecacheModel( this, "sprites/nhth1.spr" );
	PrecacheSound_Intercept( this, "kingpin/kingpin_seeker_amb.wav" );
	PrecacheSound_Intercept( this, "tor/tor-staff-discharge.wav" );
}

void CKingpinPlasmaBall::Spawn( void )
{
	Precache();
	UTIL_SetModel( this, "sprites/nhth1.spr" );
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->rendermode = kRenderTransAdd;
	pev->rendercolor = Vector( 255, 255, 255 );
	pev->renderamt = 255;
	pev->scale = 1;
	m_iOriginalRenderMode = pev->rendermode;
	m_iOriginalRenderFX = pev->renderfx;
	m_flOriginalRenderAmount = pev->renderamt;
	m_vecOriginalRenderColor = pev->rendercolor;
	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	UTIL_SetOrigin( pev, pev->origin );
	pev->scale = 1.5f;
	m_flFieldOfView = -1;
	m_hOwner = CBaseEntity::Instance( pev->owner );
	SetThink( &CKingpinPlasmaBall::HuntThink );
	SetTouch( &CKingpinPlasmaBall::ExplodeTouch );
	pev->nextthink = gpGlobals->time + 0.1f;
	m_flDieTime = gpGlobals->time + 30;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
	WRITE_BYTE( TE_BEAMFOLLOW );
	WRITE_SHORT( entindex() );
	WRITE_SHORT( m_iTrailSprite );
	WRITE_BYTE( 5 );
	WRITE_BYTE( 15 );
	WRITE_BYTE( 200 );
	WRITE_BYTE( 200 );
	WRITE_BYTE( 255 );
	WRITE_BYTE( 255 );
	MESSAGE_END();

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "kingpin/kingpin_seeker_amb.wav", 1, ATTN_IDLE, 0, 100 );
}

void CKingpinPlasmaBall::Die( void )
{
	STOP_SOUND( edict(), CHAN_VOICE, "kingpin/kingpin_seeker_amb.wav" );
	SetTouch( NULL );
	pev->velocity = g_vecZero;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	UTIL_Remove( this );
}

CBaseEntity *CKingpinPlasmaBall::BestVisibleEnemy( void )
{
	if ( !m_hOwner )
		return NULL;

	CBaseEntity *best = NULL;
	int bestRelationship = 0;
	int bestDistance = 65536;
	CBaseEntity *candidate = m_pLink;

	Center();

	while ( candidate )
	{
		if ( candidate->IsAlive() || candidate->IsBSPModel() )
		{
			// Keep this node when Look rejects the entity
			if ( candidate->pev->flags & FL_NOTARGET )
				continue;

			int relationship = m_hOwner->IRelationship( candidate );

			if ( relationship > R_NO )
			{
				if ( relationship > bestRelationship || m_hOwner->IRelationship( candidate ) == R_FR )
				{
					bestRelationship = relationship;
					best = candidate;
					bestDistance = (int) ( candidate->Center() - Center() ).Length();
				}
				else if ( m_hOwner->IRelationship( candidate ) == bestRelationship )
				{
					int distance = (int) ( candidate->Center() - Center() ).Length();

					if ( distance < bestDistance )
					{
						best = candidate;
						bestDistance = distance;
					}
				}
			}
		}

		candidate = candidate->m_pLink;
	}

	return best;
}

void CKingpinPlasmaBall::HuntThink( void )
{
	if ( !m_hOwner || gpGlobals->time > m_flDieTime )
	{
		Die();
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1f;
	pev->frame = ( (int) pev->frame + 1 ) % 11;

	if ( m_hEnemy && !m_hEnemy->IsAlive() )
		m_hEnemy = NULL;

	Vector target = g_vecZero;

	if ( m_hEnemy )
	{
		if ( FVisible( m_hEnemy, true ) )
		{
			m_vecEnemyLKP = m_hEnemy->Center();
			target = m_vecEnemyLKP;
		}
		else
		{
			Look( 2048 );

			CBaseEntity *enemy = BestVisibleEnemy();

			if ( enemy )
			{
				m_hEnemy = enemy;
				m_vecEnemyLKP = m_hEnemy->Center();
				target = m_vecEnemyLKP;
			}
			else if ( ( pev->origin - m_vecEnemyLKP ).Length() < 32 )
			{
				Die();
				return;
			}
			else
			{
				target = m_vecEnemyLKP;
			}
		}
	}
	else if ( m_hOwner )
	{
		CBaseMonster *owner = dynamic_cast<CBaseMonster *>( (CBaseEntity *) m_hOwner );

		if ( owner && owner->m_hEnemy && FVisible( owner->m_hEnemy, true ) )
		{
			m_hEnemy = owner->m_hEnemy;
			m_vecEnemyLKP = m_hEnemy->Center();
			target = m_vecEnemyLKP;
		}
	}
	else
	{
		Look( 2048 );

		CBaseEntity *enemy = BestVisibleEnemy();

		if ( !enemy )
			return;

		m_hEnemy = enemy;
		m_vecEnemyLKP = m_hEnemy->Center();
		target = m_vecEnemyLKP;
	}

	Vector currentDirection = pev->velocity.Normalize();
	Vector targetDirection = ( target - pev->origin ).Normalize();

	pev->velocity = ( currentDirection * 0.5f + targetDirection * 0.5f ) * 350 * 0.5f * ( DotProduct( currentDirection, targetDirection ) + 1 );
}

Vector KingpinBlastDirection( Vector direction )
{
	float vertical = fabs( direction.z );

	if ( vertical > 0.999f )
	{
		direction.x = 0.70710677f;
		direction.z = 0.70710677f;
	}
	else if ( vertical > 0.5f )
	{
		direction.z = max( direction.x, direction.y );
		direction = direction.Normalize();
	}

	return direction;
}

void CKingpin::DoShockWave( Vector position, int red, int green, int blue )
{
	int pitch = RANDOM_LONG( 90, 100 );

	EMIT_SOUND_DYN( edict(), CHAN_AUTO, "kingpin/port_suckin1.wav", GetSoundVolue(), ATTN_NORM, 0, pitch );

	TraceResult trace;

	UTIL_TraceLine( position, position - Vector( 0, 0, 128 ), ignore_monsters, NULL, &trace );

	if ( trace.flFraction < 1 )
	{
		for ( int i = 0; i < 5; i++ )
		{
			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMCYLINDER );
			WRITE_COORD( trace.vecEndPos.x );
			WRITE_COORD( trace.vecEndPos.y );
			WRITE_COORD( trace.vecEndPos.z + 8 + i * 3 );
			WRITE_COORD( trace.vecEndPos.x );
			WRITE_COORD( trace.vecEndPos.y );
			WRITE_COORD( trace.vecEndPos.z + 100 + i * 200 );
			WRITE_SHORT( m_iShockWaveSprite );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 2 );
			WRITE_BYTE( 8 + i * 3 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( red );
			WRITE_BYTE( green );
			WRITE_BYTE( blue );
			WRITE_BYTE( 100 + i * 31 );
			WRITE_BYTE( 0 );
			MESSAGE_END();
		}

		position = trace.vecEndPos;
	}

	CBaseEntity *target = NULL;

	while ( ( target = UTIL_FindEntityInSphere( target, position, 256 ) ) != NULL )
	{
		if ( target->pev->takedamage == DAMAGE_NO || !( target->pev->flags & ( FL_CLIENT | FL_MONSTER ) )
			|| IRelationship( target ) <= R_NO || target->BloodColor() == DONT_BLEED )
			continue;

		Vector center = target->Center();

		UTIL_TraceLine( position, center, ignore_monsters, target->edict(), &trace );

		if ( trace.flFraction < 1 )
			continue;

		float damage = target->pev->movetype == MOVETYPE_PUSHSTEP ? 1 : sk_kingpin_tele_blast.value;
		Vector direction = center - position;
		float distance = direction.Length();

		damage -= distance * ( 1.0f / 256 ) * damage;

		if ( damage > 0 )
		{
			target->TakeDamage( pev, pev, damage, DMG_SHOCK );
			target->pev->velocity = target->pev->velocity + KingpinBlastDirection( direction * ( 1.0f / distance ) ) * 1000;
			target->pev->flags &= ~FL_ONGROUND;
		}
	}
}

void CKingpinPlasmaBall::ExplodeTouch( CBaseEntity *other )
{
	int pitch = RANDOM_LONG( 90, 100 );

	EMIT_SOUND_DYN( edict(), CHAN_AUTO, "tor/tor-staff-discharge.wav", GetSoundVolue(), ATTN_NORM, 0, pitch );

	Vector position = pev->origin;
	TraceResult trace;

	UTIL_TraceLine( position, position - Vector( 0, 0, 128 ), ignore_monsters, NULL, &trace );

	if ( trace.flFraction < 1 )
	{
		for ( int i = 0; i < 7; i++ )
		{
			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMCYLINDER );
			WRITE_COORD( trace.vecEndPos.x );
			WRITE_COORD( trace.vecEndPos.y );
			WRITE_COORD( trace.vecEndPos.z + 8 + i * 5 );
			WRITE_COORD( trace.vecEndPos.x );
			WRITE_COORD( trace.vecEndPos.y );
			WRITE_COORD( trace.vecEndPos.z + 100 + i * 300 );
			WRITE_SHORT( m_iShockWaveSprite );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 4 );
			WRITE_BYTE( 8 + i * 3 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 150 );
			WRITE_BYTE( 100 );
			WRITE_BYTE( 255 );
			WRITE_BYTE( 100 + i * 31 );
			WRITE_BYTE( 0 );
			MESSAGE_END();
		}

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMDISK );
		WRITE_COORD( trace.vecEndPos.x );
		WRITE_COORD( trace.vecEndPos.y );
		WRITE_COORD( trace.vecEndPos.z + 8 );
		WRITE_COORD( trace.vecEndPos.x );
		WRITE_COORD( trace.vecEndPos.y );
		WRITE_COORD( trace.vecEndPos.z + 750 );
		WRITE_SHORT( m_iShockWaveSprite );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 4 );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 200 );
		WRITE_BYTE( 150 );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 0 );
		MESSAGE_END();

		position = trace.vecEndPos;
	}

	CBaseEntity *target = NULL;

	while ( ( target = UTIL_FindEntityInSphere( target, position, 512 ) ) != NULL )
	{
		if ( target->pev->takedamage == DAMAGE_NO || !( target->pev->flags & ( FL_CLIENT | FL_MONSTER ) ) )
			continue;

		if ( m_hOwner && ( target == (CBaseEntity *) m_hOwner || m_hOwner->IRelationship( target ) <= R_NO ) )
			continue;

		if ( target->BloodColor() == DONT_BLEED )
			continue;

		Vector center = target->Center();

		UTIL_TraceLine( position, center, ignore_monsters, target->edict(), &trace );

		if ( trace.flFraction < 1 )
			continue;

		float damage = target->pev->movetype == MOVETYPE_PUSHSTEP ? 1 : sk_kingpin_plasma_blast.value;
		Vector direction = center - position;
		float distance = direction.Length();

		damage -= distance * ( 1.0f / 512 ) * damage;

		if ( damage > 0 )
		{
			target->TakeDamage( pev, m_hOwner ? m_hOwner->pev : pev, damage, DMG_SHOCK );
			target->pev->velocity = target->pev->velocity + KingpinBlastDirection( direction * ( 1.0f / distance ) ) * 200;
			target->pev->flags &= ~FL_ONGROUND;
		}
	}

	CKingpinCrawlerBeam *crawler = static_cast<CKingpinCrawlerBeam *>( CBaseEntity::Create(
		"kingpin_crawler_beam", pev->origin, g_vecZero, false, m_hOwner ? m_hOwner->edict() : NULL ) );

	crawler->SetRadius( 512 );
	crawler->SetDecayRate( 0.9f );
	crawler->Assign( pev->origin );
	Die();
}

void CKingpin::LightningAttack( int eye )
{
	if ( !m_hEnemy )
		return;

	Vector origin = Center();
	Vector angles = g_vecZero;

	GetAttachment( eye, origin, angles );

	Vector start = origin;
	Vector end = m_hEnemy->Center();
	edict_t *ignore = edict();
	edict_t *previous = ignore;
	TraceResult trace;

	memset( &trace, 0, sizeof( trace ) );

	for ( int i = 0; i < 100; i++ )
	{
		UTIL_TraceLine( start, end, dont_ignore_monsters, ignore, &trace );

		if ( trace.flFraction == 1 || trace.pHit == previous )
			break;

		CBaseEntity *hit = CBaseEntity::Instance( trace.pHit );

		if ( !hit || hit->pev->takedamage == DAMAGE_NO )
			break;

		ClearMultiDamage();
		hit->TraceAttack( pev, sk_kingpin_lightning.value, ( end - start ).Normalize(), &trace, DMG_SHOCK );
		ApplyMultiDamage( pev, pev );
		previous = ignore;
		ignore = trace.pHit;
		start = trace.vecEndPos;
	}

	CBeam *beam = CBeam::BeamCreate( "sprites/kingpin_beam.spr", 50 );

	if ( beam )
	{
		beam->PointsInit( origin, trace.vecEndPos );
		beam->SetBrightness( 255 );
		beam->SetNoise( 50 );
		beam->SetScrollRate( 50 );
		beam->SetColor( 150, 100, 255 );
		beam->SetThink( &CBaseEntity::SUB_Remove );
		beam->pev->nextthink = gpGlobals->time + 0.3f;
	}

	MESSAGE_BEGIN( MSG_PVS, gmsgRampSprite, origin );
	WRITE_SHORT( m_iFlareSprite );
	WRITE_BYTE( 10 );
	WRITE_COORD( origin.x );
	WRITE_COORD( origin.y );
	WRITE_COORD( origin.z );
	WRITE_SHORT( 1039 );
	WRITE_BYTE( 63 );
	WRITE_BYTE( 20 );
	WRITE_BYTE( 255 );
	WRITE_BYTE( 110 );
	WRITE_BYTE( -75 );
	MESSAGE_END();

	int pitch = RANDOM_LONG( 90, 100 );

	EMIT_SOUND_DYN( edict(), CHAN_AUTO, "debris/beamstart10.wav", GetSoundVolue(), ATTN_NORM, 0, pitch );
}

void CKingpin::StartTask( Task_t *task )
{
	ResolveRenderProps();

	switch ( task->iTask )
	{
		case TASK_SOUND_WAKE:
			m_flTeleportStartTime = gpGlobals->time - 7;
			CBaseMonster::StartTask( task );
			break;

		case TASK_FIND_COVER_FROM_BEST_SOUND:
		case TASK_FIND_COVER_FROM_ENEMY:
		case TASK_FIND_LATERAL_COVER_FROM_ENEMY:
		case TASK_FIND_NODE_COVER_FROM_ENEMY:
		case TASK_FIND_NEAR_NODE_COVER_FROM_ENEMY:
		case TASK_FIND_FAR_NODE_COVER_FROM_ENEMY:
		case TASK_FIND_COVER_FROM_ORIGIN:
		case TASK_FIND_ATTACKPOINT_TO_ENEMY:
		case TASK_FIND_COVER_DISTANCE_FROM_ENEMY:
			m_iPreviousRoute = -1;
			CBaseMonster::StartTask( task );
			break;

		case TASK_DIE:
			pev->movetype = MOVETYPE_FLY;
			pev->velocity = Vector( 0, 0, 16 );
			m_flTeleportStartTime = gpGlobals->time;
			CBaseMonster::StartTask( task );
			break;

		case TASK_KINGPIN_START_TELEPORT:
			if ( !m_hEnemy )
			{
				TaskFail();
				break;
			}

			pev->movetype = MOVETYPE_FLY;
			pev->velocity = Vector( 0, 0, 16 );
			pev->solid = SOLID_NOT;
			ShieldOff();
			pev->rendermode = kRenderTransAdd;
			pev->renderamt = 255;
			pev->renderfx = kRenderFxNone;
			pev->rendercolor = g_vecZero;
			m_flTeleportStartTime = gpGlobals->time;
			m_fTeleportEffect = false;

			for ( int i = 0; i < 2; i++ )
			{
				int pitch = RANDOM_LONG( 90, 100 );

				EMIT_SOUND_DYN( edict(), CHAN_AUTO, "kingpin/port_suckout1.wav", GetSoundVolue(), ATTN_NORM, 0, pitch );
			}
			break;

		case TASK_KINGPIN_CHECK_TELEPORT:
		case TASK_KINGPIN_CHECK_ARRIVAL:
			m_hTeleportCheck = CBaseEntity::Create( "kingpin_teleport_check", m_vecTeleportTarget, g_vecZero, true, edict() );

			if ( m_hTeleportCheck )
			{
				m_hTeleportCheck->pev->model = pev->model ? pev->model : AllocStringLocal( "models/kingpin.mdl" );
				DispatchSpawn( m_hTeleportCheck->edict() );
			}

			m_flTeleportStartTime = gpGlobals->time;
			break;

		case TASK_KINGPIN_PLASMA_BALL:
			m_IdealActivity = ACT_RANGE_ATTACK2;
			break;

		default:
			CBaseMonster::StartTask( task );
			break;
	}
}

void KingpinTeleportBeam( const Vector &start, const Vector &end, int sprite, int frameRate, int life, int width,
                                 int noise, int red, int green, int blue, int brightness, int scroll )
{
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
	WRITE_BYTE( TE_BEAMPOINTS );
	WRITE_COORD( start.x );
	WRITE_COORD( start.y );
	WRITE_COORD( start.z );
	WRITE_COORD( end.x );
	WRITE_COORD( end.y );
	WRITE_COORD( end.z );
	WRITE_SHORT( sprite );
	WRITE_BYTE( 0 );
	WRITE_BYTE( frameRate );
	WRITE_BYTE( life );
	WRITE_BYTE( width );
	WRITE_BYTE( noise );
	WRITE_BYTE( red );
	WRITE_BYTE( green );
	WRITE_BYTE( blue );
	WRITE_BYTE( brightness );
	WRITE_BYTE( scroll );
	MESSAGE_END();
}

void CKingpin::RunTask( Task_t *task )
{
	switch ( task->iTask )
	{
		case TASK_DIE:
		{
			float progress = ( gpGlobals->time - m_flTeleportStartTime ) * 0.5f;

			if ( progress >= 1 )
			{
				Vector position = Center();

				MESSAGE_BEGIN( MSG_PVS, gmsgSRDetonate, position );
				WRITE_COORD( position.x );
				WRITE_COORD( position.y );
				WRITE_COORD( position.z );
				WRITE_BYTE( 96 );
				MESSAGE_END();

				GibMonster();
				return;
			}

			int count = 1 - (int) ( progress * -4 );

			for ( int i = 0; i < count; i++ )
			{
				Vector position = Center();
				int z = RANDOM_LONG( -256, 256 );
				int y = RANDOM_LONG( -256, 256 );
				int x = RANDOM_LONG( -256, 256 );
				TraceResult trace;

				UTIL_TraceLine( position, position + Vector( x, y, z ), ignore_monsters, edict(), &trace );

				if ( trace.flFraction < 1 )
				{
					int color = (int) ( progress * 205 + 50 );
					int width = RANDOM_LONG( 30, color );

					KingpinTeleportBeam( position, trace.vecEndPos, m_iLightningSprite, 15, 3, width, 255, color, 100,
					                     (int) ( ( 1 - progress ) * 205 + 50 ), 255, 0 );
				}
			}
			return;
		}
		case TASK_KINGPIN_START_TELEPORT:
		{
			float progress = ( gpGlobals->time - m_flTeleportStartTime ) * 0.5f;
			CBaseEntity *target = NULL;

			while ( ( target = UTIL_FindEntityInSphere( target, m_vecTeleportTarget, 64 ) ) != NULL )
			{
				int movement = target->pev->movetype;

				if ( movement == MOVETYPE_NONE || movement == MOVETYPE_PUSH || movement == MOVETYPE_NOCLIP || movement == MOVETYPE_FOLLOW
					|| target->pev->solid == SOLID_NOT || target->pev->solid == SOLID_BSP )
					continue;

				Vector direction = target->Center() - m_vecTeleportTarget;
				float distance = direction.Length();

				if ( distance != 0 )
					direction = direction * ( 1.0f / distance );
				else
					direction = g_vecZero;

				target->pev->velocity = target->pev->velocity + KingpinBlastDirection( direction ) * 200;
			}

			if ( !m_fTeleportEffect && progress > 0.5f )
			{
				Vector position = Center() + Vector( 0, 0, 32 );
				CSprite *sprite = CSprite::SpriteCreate( "sprites/b-tele1.spr", position, TRUE );

				if ( sprite )
				{
					sprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone );
					sprite->SetScale( 2 );
					sprite->AnimateAndDie( 15 );
				}

				position = m_vecTeleportTarget + Vector( 0, 0, pev->size.z * 0.5f );
				sprite = CSprite::SpriteCreate( "sprites/b-tele1.spr", position, TRUE );

				if ( sprite )
				{
					sprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone );
					sprite->SetScale( 2 );
					sprite->AnimateAndDie( 15 );
				}

				CSoundEnt::InsertSound( bits_SOUND_DANGER, position, 256, 1, this );
				m_fTeleportEffect = true;
			}
			else if ( progress >= 1 )
			{
				TaskComplete();
				return;
			}

			pev->renderamt = ( 1 - progress ) * 255;
			return;
		}
		case TASK_KINGPIN_CHECK_TELEPORT:
		case TASK_KINGPIN_CHECK_ARRIVAL:
		{
			if ( gpGlobals->time - m_flTeleportStartTime <= 0.1f )
				return;

			CKingpinTeleportCheck *check = dynamic_cast<CKingpinTeleportCheck *>( (CBaseEntity *) m_hTeleportCheck );

			m_hTeleportCheck = NULL;

			if ( check && !check->m_fBlocked )
			{
				UTIL_Remove( check );
				TaskComplete();
				return;
			}

			if ( task->iTask == TASK_KINGPIN_CHECK_TELEPORT )
			{
				TaskFail();
				return;
			}

			entvars_t *attacker = check && check->m_hBlocker ? check->m_hBlocker->pev : VARS( INDEXENT( 0 ) );

			TakeDamage( attacker, attacker, pev->health + 200, DMG_ALWAYSGIB );
			TaskComplete();
			return;
		}
		case TASK_KINGPIN_TELEPORT:
		{
			Vector start = Center();

			pev->effects |= EF_NOINTERP;
			pev->movetype = MOVETYPE_STEP;
			pev->solid = SOLID_SLIDEBOX;
			pev->flags &= ~FL_ONGROUND;
			UTIL_SetOrigin( pev, m_vecTeleportTarget );

			Vector end = Center();

			for ( int i = 0; i < 3; i++ )
				KingpinTeleportBeam( start, end, m_iLightningSprite, 0, 2, 100, 30, 200, 200, 255, 255, 50 );

			DoShockWave( end, 180, 160, 255 );

			CBaseEntity *targets[64];
			int count = UTIL_EntitiesInBox( targets, ARRAYSIZE( targets ), m_vecTeleportTarget + pev->mins, m_vecTeleportTarget + pev->maxs, FL_CLIENT | FL_MONSTER );

			for ( int i = 0; i < count; i++ )
			{
				if ( targets[i]->edict() != edict() && targets[i]->pev->solid >= SOLID_BBOX )
					targets[i]->TakeDamage( pev, pev, sk_kingpin_telefrag.value + 200, DMG_ALWAYSGIB );
			}

			if ( !HasConditions( bits_COND_TASK_FAILED ) )
				TaskComplete();

			return;
		}
		case TASK_KINGPIN_PLASMA_BALL:
			if ( m_fSequenceFinished )
			{
				TaskComplete();
				return;
			}

			if ( gpGlobals->time - m_flLastShockWaveTime < 1.3f )
			{
				UTIL_MakeVectors( pev->angles );

				Vector start = pev->origin + gpGlobals->v_forward * 32 + gpGlobals->v_up * 64;
				int z = RANDOM_LONG( -256, 256 );
				int y = RANDOM_LONG( -256, 256 );
				int x = RANDOM_LONG( -256, 256 );
				TraceResult trace;

				UTIL_TraceLine( start, start + Vector( x, y, z ), ignore_monsters, edict(), &trace );

				if ( trace.flFraction < 1 )
					KingpinTeleportBeam( start, trace.vecEndPos, m_iLightningSprite, 15, 1, 50, 255, 200, 100, 255, 150, 0 );
			}
			else if ( pev->framerate == 0 )
			{
				pev->framerate = 1;
				UTIL_MakeVectors( pev->angles );

				Vector position = pev->origin + gpGlobals->v_forward * 32 + gpGlobals->v_up * 64;
				CBaseEntity *ball = CBaseEntity::Create( "kingpin_plasma_ball", position, g_vecZero, false, edict() );

				if ( ball )
					ball->pev->velocity = gpGlobals->v_forward;
			}
			return;

		default:
			CBaseMonster::RunTask( task );
			return;
	}
}

void CKingpin::FindNewProjectileThreats( void )
{
	Vector center = Center();
	CBaseEntity *projectile = NULL;

	while ( ( projectile = UTIL_FindEntityInSphere( projectile, center, 256 ) ) != NULL )
	{
		bool snark = FClassnameIs( projectile->pev, "monster_snark" );
		bool explosive = FClassnameIs( projectile->pev, "grenade" ) || FClassnameIs( projectile->pev, "rpg_rocket" )
			|| FClassnameIs( projectile->pev, "monster_satchel" ) || FClassnameIs( projectile->pev, "sporegrenade" );

		if ( !snark && !explosive )
			continue;

		CBaseEntity *owner = projectile;

		if ( snark )
		{
			CSqueakGrenade *grenade = dynamic_cast<CSqueakGrenade *>( projectile );

			if ( grenade && grenade->GetOriginalOwner() )
				owner = grenade->GetOriginalOwner();
		}
		else if ( projectile->pev->owner )
		{
			owner = CBaseEntity::Instance( projectile->pev->owner );
		}

		if ( IRelationship( owner ) <= R_NO )
			continue;

		TraceResult trace;

		UTIL_TraceLine( pev->origin, projectile->pev->origin, ignore_monsters, edict(), &trace );

		if ( trace.flFraction < 1 && trace.pHit != projectile->edict() )
			continue;

		if ( snark )
		{
			MakeDeflectTarget( projectile );
			return;
		}

		float speed = projectile->pev->velocity.Length();

		if ( speed > 1000 )
		{
			Vector end = projectile->pev->origin + projectile->pev->velocity / speed * 512;

			UTIL_TraceLine( projectile->pev->origin, end, dont_ignore_monsters, projectile->edict(), &trace );

			if ( trace.pHit == edict() )
			{
				TryToDetonateTarget( projectile );
				return;
			}
		}

		if ( pev->health < 50 && ( center - projectile->pev->origin ).Length() > 200 )
		{
			CBaseEntity *target = NULL;

			while ( ( target = UTIL_FindEntityInSphere( target, projectile->pev->origin, 200 ) ) != NULL )
			{
				if ( ( target->IsAlive() || target->IsBSPModel() ) && IRelationship( target ) > R_NO )
				{
					TryToDetonateTarget( projectile );
					return;
				}
			}
		}

		MakeDeflectTarget( projectile );
		return;
	}
}
