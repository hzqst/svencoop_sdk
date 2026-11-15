#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "squadmonster.h"
#include "schedule.h"
#include "soundent.h"
#include "effects.h"
#include "weapons.h"
#include "game.h"

CBaseEntity *TorHandleEntity( EHANDLE &handle )
{
	edict_t *entry = handle.Get();

	return entry && !entry->free ? (CBaseEntity *) GET_PRIVATE( entry ) : NULL;
}

enum
{
	SCHED_TOR_SUPPRESS = LAST_COMMON_SCHEDULE + 1,
	SCHED_TOR_THREAT_DISPLAY,
};

enum
{
	TASK_TOR_GET_PATH_TO_ENEMY_CORPSE = LAST_COMMON_TASK + 2,
	TASK_TOR_TELEPORT,
	TASK_TOR_MAKE_GRUNT,
};

class CTor : public CSquadMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetupFriendly( void );
	void SetObjectCollisionBox( void );
	void SetYawSpeed( void );
	int Classify( void );
	int ObjectCaps( void );
	int ISoundMask( void );
	int IRelationship( CBaseEntity *target, bool ignoreNoTarget = false );
	float GetPointsForDamage( float damage );
	void HandleAnimEvent( MonsterEvent_t *event );
	void TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType );
	int TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType );
	void Killed( entvars_t *attacker, int gib );
	void UpdateOnRemove( void );
	void SetActivity( Activity activity );
	BOOL FCanCheckAttacks( void );
	BOOL CheckMeleeAttack1( float dot, float distance );
	BOOL CheckMeleeAttack2( float dot, float distance );
	BOOL CheckRangeAttack1( float dot, float distance );
	BOOL CheckRangeAttack2( float dot, float distance );
	void StartTask( Task_t *task );
	void RunTask( Task_t *task );
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int type );
	void ScheduleChange( void );
	void PrescheduleThink( void );
	void AlertSound( void );
	void DeathSound( void );
	void PainSound( void );
	void AttackSound( void );
	void TeleportCrySound( void );
	void UseSound( void );
	void UnUseSound( void );
	void StopTalking( void );
	BOOL ShouldSpeak( void );
	void SonicBlast( void );
	void ExplosionBlast( Vector origin );
	Vector FindTeleportPosition( void );
	void TelefragStuff( Vector origin );
	void MakeTeleportEffect( Vector origin );
	void MakeGrunt( Vector origin );
	void AddFriend( CBaseMonster *monster );
	void UpdateFriends( void );
	void RemoveEffects( void );
	int Save( CSave &save );
	int Restore( CRestore &restore );

	CUSTOM_SCHEDULES;
	static TYPEDESCRIPTION m_SaveData[];

private:
	bool	m_fCanHornetAttack;
	int		m_iTorAmmo;
	float	m_flNextHornetAttackCheck;
	float	m_flNextPainTime;
	int		m_iMuzzleFlash;
	float	m_flNextSpeakTime;
	float	m_flNextWordTime;
	int		m_iLastWord;
	int		m_iFriends;
	Vector	m_vecTeleportPosition;
	EHANDLE	m_hFriends[3];
	float	m_flNextTeleportTime;
	EHANDLE	m_hEffects[4];
	int		m_iShockwave;
	int		m_iEnergyBeam;

	static const char *pAttackHitSounds[];
	static const char *pAttackMissSounds[];
	static const char *pDieSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pAttackSounds[];
};

LINK_ENTITY_TO_CLASS( monster_alien_tor, CTor );
LINK_ENTITY_TO_CLASS( monster_alien_commander, CTor );

TYPEDESCRIPTION CTor::m_SaveData[] = {
	DEFINE_FIELD( CTor, m_fCanHornetAttack, FIELD_CHARACTER ),
	DEFINE_FIELD( CTor, m_flNextHornetAttackCheck, FIELD_TIME ),
	DEFINE_FIELD( CTor, m_flNextPainTime, FIELD_TIME ),
	DEFINE_FIELD( CTor, m_flNextSpeakTime, FIELD_TIME ),
	DEFINE_FIELD( CTor, m_flNextWordTime, FIELD_TIME ),
	DEFINE_FIELD( CTor, m_iLastWord, FIELD_INTEGER ),
	DEFINE_FIELD( CTor, m_iTorAmmo, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CTor, CSquadMonster );

const char *CTor::pAttackHitSounds[] = {
	"zombie/claw_strike1.wav", "zombie/claw_strike2.wav", "zombie/claw_strike3.wav"
};
const char *CTor::pAttackMissSounds[] = { "zombie/claw_miss1.wav", "zombie/claw_miss2.wav" };
const char *CTor::pDieSounds[] = { "tor/tor-die.wav", "tor/tor-die2.wav" };
const char *CTor::pIdleSounds[] = { "tor/tor-idle.wav", "tor/tor-idle2.wav", "tor/tor-idle3.wav" };
const char *CTor::pAlertSounds[] = { "tor/tor-test1.wav", "tor/tor-alerted.wav" };
const char *CTor::pPainSounds[] = { "tor/tor-pain.wav" };
const char *CTor::pAttackSounds[] = { "tor/tor-staff-discharge.wav" };

void CTor::Spawn( void )
{
	Precache();

	if ( !SetupModel() )
		SetupFriendly();

	UTIL_SetSize( pev, Vector( -24, -24, 0 ), Vector( 24, 24, 88 ) );

	if ( pev->health == 0 )
		pev->health = sk_tor_health.value;

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->effects = 0;
	m_bloodColor = BLOOD_COLOR_GREEN;
	m_flFieldOfView = 0.2f;
	m_MonsterState = MONSTERSTATE_NONE;
	m_afCapability = bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_USE_TANK;
	m_HackedGunPos = Vector( 24, 64, 48 );
	m_flNextSpeakTime = m_flNextWordTime = gpGlobals->time + 10 + RANDOM_LONG( 0, 10 );
	m_iFriends = 0;
	m_flNextTeleportTime = gpGlobals->time + 5;
	m_iTorAmmo = 10;

	if ( !m_FormattedName )
		m_FormattedName = m_fPlayerAlly ? MAKE_STRING( "Tor" ) : MAKE_STRING( "Xen Commander" );

	m_afMoveShootCap = 0;
	m_hSquadLeader = edict() && !edict()->free ? this : NULL;
	MonsterInit();
	m_hSquadLeader = edict() && !edict()->free ? this : NULL;

	if ( m_fPlayerAlly && !( pev->spawnflags & SF_MONSTER_PREDISASTER ) )
	{
		SetUse( &CBaseMonster::CallFollowerPlayerUse );
		m_fCanPlayerFollow = true;
	}

	pev->view_ofs = Vector( 0, 0, 64 );
	m_flNextHornetAttackCheck = gpGlobals->time + 0.5f;
}

void CTor::Precache( void )
{
	CBaseEntity::Precache();
	UTIL_PrecacheMonster( "monster_alien_grunt", m_fPlayerAlly );
	UTIL_PrecacheOther( "nihilanth_energy_ball" );

	if ( !pev->model )
		UTIL_PrecacheModel( this, m_fPlayerAlly ? "models/torf.mdl" : "models/tor.mdl" );

	m_iMuzzleFlash = UTIL_PrecacheModel( this, "sprites/muz4.spr" );
	m_iShockwave = UTIL_PrecacheModel( this, "sprites/shockwave.spr" );
	m_iEnergyBeam = UTIL_PrecacheModel( this, "sprites/xenobeam.spr" );
	UTIL_PrecacheModel( this, "sprites/exit1.spr" );
	UTIL_PrecacheModel( this, "sprites/lgtning.spr" );
	PrecacheSound_Intercept( this, "debris/beamstart8.wav" );
	PrecacheSound_Intercept( this, "debris/beamstart7.wav" );
	PrecacheSound_Intercept( this, "tor/tor-summon.wav" );
	PrecacheSound_Intercept( this, "tor/tor-foot.wav" );
	PrecacheSound_Intercept( this, "tor/tor-attack1.wav" );
	PrecacheSound_Intercept( this, "tor/tor-attack2.wav" );
	PrecacheSound_Intercept( this, "houndeye/he_blast1.wav" );
	PrecacheSound_Intercept( this, "houndeye/he_blast2.wav" );
	PrecacheSound_Intercept( this, "houndeye/he_blast3.wav" );

	for ( unsigned int i = 0; i < ARRAYSIZE( pAttackHitSounds ); i++ )
		PrecacheSound_Intercept( this, pAttackHitSounds[i] );

	for ( unsigned int i = 0; i < ARRAYSIZE( pAttackMissSounds ); i++ )
		PrecacheSound_Intercept( this, pAttackMissSounds[i] );

	for ( unsigned int i = 0; i < ARRAYSIZE( pIdleSounds ); i++ )
		PrecacheSound_Intercept( this, pIdleSounds[i] );

	for ( unsigned int i = 0; i < ARRAYSIZE( pDieSounds ); i++ )
		PrecacheSound_Intercept( this, pDieSounds[i] );

	PrecacheSound_Intercept( this, pPainSounds[0] );
	PrecacheSound_Intercept( this, pAttackSounds[0] );

	for ( unsigned int i = 0; i < ARRAYSIZE( pAlertSounds ); i++ )
		PrecacheSound_Intercept( this, pAlertSounds[i] );
}

void CTor::SetupFriendly( void )
{
	UTIL_SetModel( this, m_fPlayerAlly ? "models/torf.mdl" : "models/tor.mdl" );
}

void CTor::SetObjectCollisionBox( void )
{
	if ( m_fCustomModel && !( pev->spawnflags & SF_MONSTER_NO_MODEL_BOX ) )
	{
		Vector mins, maxs;

		ExtractBbox( 1, mins, maxs );
		mins.z = 0;
		pev->absmin = pev->origin + mins;
		pev->absmax = pev->origin + maxs;
	}
	else
	{
		pev->absmin = pev->origin + Vector( -32, -32, 0 );
		pev->absmax = pev->origin + Vector( 32, 32, 85 );
	}
}

void CTor::SetYawSpeed( void )
{
	pev->yaw_speed = 270;
}

int CTor::Classify( void )
{
	return GetClassification( CLASS_ALIEN_MILITARY );
}

int CTor::ObjectCaps( void )
{
	return m_fPlayerAlly ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

int CTor::ISoundMask( void )
{
	return bits_SOUND_WORLD | bits_SOUND_COMBAT | bits_SOUND_PLAYER | bits_SOUND_DANGER;
}

int CTor::IRelationship( CBaseEntity *target, bool ignoreNoTarget )
{
	if ( !target )
		return R_NO;

	if ( ( target->pev->flags & FL_MONSTER ) && target->m_fOverrideClass )
		return CBaseEntity::IRelationship( target, ignoreNoTarget );

	if ( !m_fPlayerAlly && target->Classify() == CLASS_HUMAN_MILITARY )
		return R_NM;

	return CBaseEntity::IRelationship( target, false );
}

float CTor::GetPointsForDamage( float damage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	float points = pev->max_health / sk_tor_health.value;

	points *= 4.0f;

	return points * ( damage / pev->max_health );
}

void CTor::SetActivity( Activity activity )
{
	if ( activity == ACT_IDLE && m_MonsterState == MONSTERSTATE_COMBAT )
		activity = ACT_IDLE_ANGRY;

	int sequence = activity == ACT_RANGE_ATTACK1 ? LookupSequence( "attack_idle" ) : LookupActivity( activity );

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

BOOL CTor::FCanCheckAttacks( void )
{
	return !HasConditions( bits_COND_ENEMY_TOOFAR );
}

BOOL CTor::CheckMeleeAttack1( float dot, float distance )
{
	if ( !HasConditions( bits_COND_SEE_ENEMY ) || TorHandleEntity( m_hEnemy.m_Handle ) == NULL )
		return FALSE;

	CBaseEntity *entity = NULL;
	int enemies = 0;

	while ( ( entity = UTIL_FindEntityInSphere( entity, pev->origin, 192 ) ) != NULL )
	{
		if ( entity->edict()->free )
			break;

		if ( IRelationship( entity ) > R_NO && ( entity->pev->flags & ( FL_CLIENT | FL_MONSTER ) ) && entity->IsAlive() )
		{
			enemies++;

			if ( enemies > 2 )
				return TRUE;
		}
	}

	return FALSE;
}

BOOL CTor::CheckMeleeAttack2( float dot, float distance )
{
	return !HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) && HasConditions( bits_COND_SEE_ENEMY )
		&& distance <= 100 && dot >= 0.6f && TorHandleEntity( m_hEnemy.m_Handle ) != NULL;
}

BOOL CTor::CheckRangeAttack1( float dot, float distance )
{
	if ( distance < 256 )
		return FALSE;

	if ( gpGlobals->time < m_flNextHornetAttackCheck )
		return m_fCanHornetAttack;

	if ( TorHandleEntity( m_hEnemy.m_Handle ) != NULL && HasConditions( bits_COND_SEE_ENEMY ) )
	{
		Vector armPosition, armAngles;
		TraceResult trace;

		UTIL_MakeVectors( pev->angles );
		GetAttachment( 0, armPosition, armAngles );
		UTIL_TraceLine( armPosition, TorHandleEntity( m_hEnemy.m_Handle )->BodyTarget( armPosition ), dont_ignore_monsters, edict(), &trace );

		if ( trace.flFraction == 1 || trace.pHit == TorHandleEntity( m_hEnemy.m_Handle )->edict() )
		{
			m_fCanHornetAttack = true;
			return TRUE;
		}
	}

	m_fCanHornetAttack = false;
	m_flNextHornetAttackCheck = gpGlobals->time + 0.2;
	return FALSE;
}

BOOL CTor::CheckRangeAttack2( float dot, float distance )
{
	return gpGlobals->time >= m_flNextTeleportTime && ( distance > 100 || dot < 0.6f )
		&& ( pev->max_health * 0.75 > pev->health || RANDOM_LONG( 0, 10 ) > 9 )
		&& m_iFriends < 3 && FindTeleportPosition() != g_vecZero;
}

void CTor::StopTalking( void )
{
	m_flNextSpeakTime = m_flNextWordTime = gpGlobals->time + 10 + RANDOM_LONG( 0, 10 );
}

BOOL CTor::ShouldSpeak( void )
{
	if ( m_flNextSpeakTime > gpGlobals->time )
		return FALSE;

	if ( ( pev->spawnflags & SF_MONSTER_GAG ) && m_MonsterState != MONSTERSTATE_COMBAT )
	{
		m_flNextSpeakTime = gpGlobals->time + 3;
		return FALSE;
	}

	return TRUE;
}

void CTor::PrescheduleThink( void )
{
	if ( TorHandleEntity( m_hEnemy.m_Handle ) != NULL && m_Activity == ACT_RUN )
		pev->framerate = 1.5f;

	if ( !ShouldSpeak() || gpGlobals->time <= m_flNextWordTime )
		return;

	int attempts = 0;
	int word;

	do
	{
		word = RANDOM_LONG( 0, 2 );

		if ( word != m_iLastWord )
			break;
	} while ( attempts++ < 5 );

	m_iLastWord = V_max( 0, V_min( word, 2 ) );
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, pIdleSounds[m_iLastWord], 1, ATTN_NORM, 0, 100 );

	if ( RANDOM_LONG( 1, 10 ) > 1 )
		m_flNextWordTime = gpGlobals->time + RANDOM_FLOAT( 0.5, 1 );
	else
		StopTalking();
}

void CTor::AlertSound( void )
{
	StopTalking();
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, pAlertSounds[RANDOM_LONG( 0, ARRAYSIZE( pAlertSounds ) - 1 )], 1, ATTN_NORM, 0, 100 );
}

void CTor::DeathSound( void )
{
	StopTalking();
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, pDieSounds[RANDOM_LONG( 0, ARRAYSIZE( pDieSounds ) - 1 )], 1, ATTN_NORM, 0, 100 );
}

void CTor::PainSound( void )
{
	if ( m_flNextPainTime > gpGlobals->time )
		return;

	m_flNextPainTime = gpGlobals->time + 0.6;
	StopTalking();
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, pPainSounds[RANDOM_LONG( 0, ARRAYSIZE( pPainSounds ) - 1 )], 1, ATTN_NORM, 0, 100 );
}

void CTor::AttackSound( void )
{
	StopTalking();
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, pAttackSounds[RANDOM_LONG( 0, ARRAYSIZE( pAttackSounds ) - 1 )], 1, 0.6f, 0, 100 );
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 0.3f, this );
}

void CTor::TeleportCrySound( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "tor/tor-summon.wav", 1, ATTN_NORM, 0, 100 );
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 512, 0.3f, this );
}

void CTor::UseSound( void )
{
	AlertSound();
}

void CTor::UnUseSound( void )
{
	PainSound();
}

void CTor::TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType )
{
	if ( trace->iHitgroup == 10 && ( damageType & ( DMG_BULLET | DMG_SLASH | DMG_CLUB ) ) )
	{
		if ( pev->dmgtime != gpGlobals->time || RANDOM_LONG( 0, 10 ) < 1 )
		{
			UTIL_Ricochet( trace->vecEndPos, RANDOM_FLOAT( 1, 2 ) );
			pev->dmgtime = gpGlobals->time;
		}

		if ( RANDOM_LONG( 0, 1 ) == 0 )
		{
			Vector tracer;

			tracer.x = direction.x + RANDOM_FLOAT( -0.3, 0.3 );
			tracer.y = direction.y + RANDOM_FLOAT( -0.3, 0.3 );
			tracer.z = direction.z + RANDOM_FLOAT( -0.3, 0.3 );
			tracer = tracer * -512;

			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, trace->vecEndPos );
			WRITE_BYTE( TE_TRACER );
			WRITE_COORD( trace->vecEndPos.x );
			WRITE_COORD( trace->vecEndPos.y );
			WRITE_COORD( trace->vecEndPos.z );
			WRITE_COORD( tracer.x );
			WRITE_COORD( tracer.y );
			WRITE_COORD( tracer.z );
			MESSAGE_END();
		}

		damage -= 45;

		if ( damage <= 0 )
			damage = 0.1f;

		AddMultiDamage( attacker, this, damage, damageType );
		return;
	}

	if ( damageType & ( DMG_SLASH | DMG_CLUB ) )
	{
		damage = ( damage - 15 ) * 0.5f;

		if ( damage < 0 )
			damage = 0.1f;
	}

	CBaseMonster::TraceAttack( attacker, damage, direction, trace, damageType );
}

int CTor::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	if ( damageType & ( DMG_SLASH | DMG_CLUB ) )
	{
		damage = ( damage - 15 ) * 0.5f;

		if ( damage < 0 )
			damage = 0.1f;
	}

	return CBaseMonster::TakeDamage( inflictor, attacker, damage, damageType );
}

void CTor::Killed( entvars_t *attacker, int gib )
{
	RemoveEffects();
	CSquadMonster::Killed( attacker, gib );
}

void CTor::UpdateOnRemove( void )
{
	CBaseEntity::UpdateOnRemove();
	RemoveEffects();
}

void CTor::UpdateFriends( void )
{
	m_iFriends = 0;

	for ( unsigned int i = 0; i < ARRAYSIZE( m_hFriends ); i++ )
	{
		if ( TorHandleEntity( m_hFriends[i] ) != NULL && TorHandleEntity( m_hFriends[i] )->IsAlive() )
			m_iFriends++;
	}
}

void CTor::AddFriend( CBaseMonster *monster )
{
	if ( !monster )
		return;

	for ( unsigned int i = 0; i < ARRAYSIZE( m_hFriends ); i++ )
	{
		if ( TorHandleEntity( m_hFriends[i] ) == NULL || !TorHandleEntity( m_hFriends[i] )->IsAlive() )
		{
			m_hFriends[i] = monster->edict() && !monster->edict()->free ? monster : NULL;
			m_iFriends++;
			return;
		}

		if ( TorHandleEntity( m_hFriends[i] ) == monster )
			return;
	}
}

void CTor::RemoveEffects( void )
{
	for ( unsigned int i = 0; i < ARRAYSIZE( m_hEffects ); i++ )
	{
		if ( TorHandleEntity( m_hEffects[i] ) != NULL )
		{
			UTIL_Remove( TorHandleEntity( m_hEffects[i] ) );
			m_hEffects[i] = NULL;
		}
	}
}

void CTor::ScheduleChange( void )
{
	UpdateFriends();
	RemoveEffects();
}

void CTor::SonicBlast( void )
{
	switch ( RANDOM_LONG( 0, 2 ) )
	{
		case 0:
			EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "houndeye/he_blast1.wav", 1, ATTN_NORM, 0, 100 );
			break;
		case 1:
			EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "houndeye/he_blast2.wav", 1, ATTN_NORM, 0, 100 );
			break;
		case 2:
			EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "houndeye/he_blast3.wav", 1, ATTN_NORM, 0, 100 );
			break;
	}

	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 512, 0.3f, this );
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
	WRITE_BYTE( TE_BEAMCYLINDER );
	WRITE_COORD( pev->origin.x );
	WRITE_COORD( pev->origin.y );
	WRITE_COORD( pev->origin.z + 16 );
	WRITE_COORD( pev->origin.x );
	WRITE_COORD( pev->origin.y );
	WRITE_COORD( pev->origin.z + 16 + 1920 );
	WRITE_SHORT( m_iShockwave );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 2 );
	WRITE_BYTE( 16 );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 255 );
	WRITE_BYTE( 255 );
	WRITE_BYTE( 255 );
	WRITE_BYTE( 255 );
	WRITE_BYTE( 0 );
	MESSAGE_END();

	CBaseEntity *entity = NULL;

	while ( ( entity = UTIL_FindEntityInSphere( entity, pev->origin, 384 ) ) != NULL )
	{
		if ( entity->edict()->free )
			break;

		if ( !entity->pev->takedamage || !( entity->pev->flags & ( FL_CLIENT | FL_MONSTER ) ) )
			continue;

		float damage = entity->pev->movetype == MOVETYPE_BOUNCE ? 1.0f : sk_tor_sonicblast.value;
		float distance = ( entity->Center() - pev->origin ).Length();

		damage -= ( distance / 384 ) * damage;

		if ( IRelationship( entity ) <= R_NO || entity->BloodColor() == DONT_BLEED || !FVisible( entity, true ) || !( damage > 0 ) )
			continue;

		entity->TakeDamage( pev, pev, damage, DMG_SONIC | DMG_ALWAYSGIB );

		Vector targetCenter = entity->Center();
		Vector center = Center();
		Vector direction = ( targetCenter - ( center - Vector( 0, 0, 256 ) ) ).Normalize();

		entity->pev->velocity = entity->pev->velocity + direction * 1000;

		if ( entity->pev->velocity.Length() > 1000 )
			entity->pev->velocity = entity->pev->velocity.Normalize() * 1000;

		entity->pev->flags &= ~FL_ONGROUND;
	}
}

void CTor::ExplosionBlast( Vector origin )
{
	CBaseEntity *entity = NULL;

	while ( ( entity = UTIL_FindEntityInSphere( entity, origin, 128 ) ) != NULL )
	{
		if ( entity->edict()->free )
			break;

		if ( !entity->pev->takedamage || ( entity->pev->movetype != MOVETYPE_STEP && entity->pev->movetype != MOVETYPE_WALK ) )
			continue;

		float damage = sk_tor_energybeam.value;
		float distance = ( entity->Center() - origin ).Length();

		damage -= distance * 0.0078125f * damage;

		if ( IRelationship( entity ) <= R_NO || !( damage > 0 ) )
			continue;

		entity->TakeDamage( pev, pev, damage, DMG_BLAST );

		Vector direction = ( entity->Center() - ( origin - Vector( 0, 0, 256 ) ) ).Normalize();

		entity->pev->velocity = entity->pev->velocity + direction * 200;

		if ( entity->pev->velocity.Length() > 600 )
			entity->pev->velocity = entity->pev->velocity.Normalize() * 600;

		entity->pev->flags &= ~FL_ONGROUND;
	}
}

Vector CTor::FindTeleportPosition( void )
{
	Vector result = g_vecZero;
	int side = RANDOM_LONG( 0, 1 ) ? 1 : -1;

	UTIL_MakeAimVectors( pev->angles );

	Vector start = pev->origin + gpGlobals->v_up * 128 + gpGlobals->v_right * side * 48 + gpGlobals->v_forward * 16;

	for ( int i = 0; i < 3; i++ )
	{
		float right = RANDOM_FLOAT( 0, 1 );
		float forward = RANDOM_FLOAT( -1, 1 );
		Vector end = start + ( gpGlobals->v_right * side * right + gpGlobals->v_forward * forward ) * 256;
		TraceResult trace;

		UTIL_TraceLine( start, end, ignore_monsters, ignore_glass, edict(), &trace );

		if ( !( ( Center() - trace.vecEndPos ).Length() > 96 ) )
			continue;

		TraceResult hull;

		UTIL_TraceHull( trace.vecEndPos, trace.vecEndPos + Vector( 0, 0, 10 ), dont_ignore_monsters, large_hull, NULL, &hull );

		if ( !hull.fStartSolid && hull.flFraction == 1 )
		{
			result = trace.vecEndPos;
			break;
		}
	}

	return result;
}

void CTor::TelefragStuff( Vector origin )
{
	CBaseEntity *entities[16];
	int count = UTIL_EntitiesInBox( entities, ARRAYSIZE( entities ), origin + Vector( -20, -20, -80 ),
		origin + Vector( 20, 20, 0 ), FL_CLIENT | FL_MONSTER );

	for ( int i = 0; i < count; i++ )
		entities[i]->TakeDamage( VARS( INDEXENT( 0 ) ), VARS( INDEXENT( 0 ) ), 300, DMG_GENERIC );
}

void CTor::MakeTeleportEffect( Vector origin )
{
	EMIT_SOUND_DYN( edict(), CHAN_ITEM, "debris/beamstart8.wav", 1, ATTN_NORM, 0, 100 );
	TeleportCrySound();

	for ( unsigned int i = 0; i < ARRAYSIZE( m_hEffects ); i++ )
	{
		CBeam *beam = CBeam::BeamCreate( "sprites/lgtning.spr", 50 );

		m_hEffects[i] = beam && beam->edict() && !beam->edict()->free ? beam : NULL;

		if ( TorHandleEntity( m_hEffects[i] ) == NULL )
			return;

		beam->PointsInit( origin, EyePosition() );
		beam->SetBrightness( 150 );
		beam->SetNoise( 80 );
		beam->SetFlags( 0x80 );
		beam->SetScrollRate( 50 );
		beam->SetColor( m_fPlayerAlly ? 194 : 96, m_fPlayerAlly ? 40 : 128, 16 );
	}

	CSprite *sprite = CSprite::SpriteCreate( "sprites/exit1.spr", origin, true );

	sprite->pev->rendermode = kRenderGlow;
	sprite->pev->rendercolor = Vector( 200, 200, 95 );
	sprite->pev->renderamt = 255;
	sprite->pev->renderfx = kRenderFxNoDissipation;
	sprite->pev->scale = 2;
	sprite->AnimateAndDie( 5 );
}

void CTor::MakeGrunt( Vector origin )
{
	TelefragStuff( origin );

	CBaseEntity *entity = CBaseEntity::Create( "monster_alien_grunt", origin - Vector( 0, 0, 36 ), pev->angles, true, NULL );

	if ( entity )
	{
		CSquadMonster *grunt = entity->MySquadMonsterPointer();

		grunt->m_fOverrideClass = m_fOverrideClass;
		grunt->m_iClassSelection = m_iClassSelection;
		grunt->m_fPlayerAlly = m_fPlayerAlly;

		if ( TorHandleEntity( m_hEnemy.m_Handle ) != NULL && TorHandleEntity( m_hEnemy.m_Handle )->IsAlive() )
			grunt->PushEnemy( TorHandleEntity( m_hEnemy.m_Handle ), m_vecEnemyLKP );

		grunt->pev->spawnflags |= SF_MONSTER_FALL_TO_GROUND;
		grunt->m_flFallDamageTime = gpGlobals->time;
		grunt->m_fFallDamageReady = true;
		DispatchSpawn( grunt->edict() );
		m_hSquadLeader = edict() && !edict()->free ? this : NULL;
		SquadAdd( grunt );
		AddFriend( grunt );
		grunt->MyMonsterPointer()->m_MonsterState = MONSTERSTATE_IDLE;
		grunt->m_IdealMonsterState = MONSTERSTATE_IDLE;
		grunt->ChangeSchedule( grunt->MyMonsterPointer()->GetScheduleOfType( SCHED_TELEPORT_FALL ) );
		m_flNextTeleportTime = gpGlobals->time + 8;
	}

	ScheduleChange();
}

void CTor::StartTask( Task_t *task )
{
	switch ( task->iTask )
	{
		case TASK_TOR_GET_PATH_TO_ENEMY_CORPSE:
			UTIL_MakeVectors( pev->angles );

			if ( BuildRoute( m_vecEnemyLKP - gpGlobals->v_forward * 50, bits_MF_TO_LOCATION, NULL ) )
				TaskComplete();
			else
			{
				ALERT( at_aiconsole, "AGruntGetPathToEnemyCorpse failed!!\n" );
				TaskFail();
			}
			break;

		case TASK_TOR_TELEPORT:
			if ( m_iFriends < 3 )
			{
				m_IdealActivity = ACT_RANGE_ATTACK2;
				m_vecTeleportPosition = FindTeleportPosition();

				if ( m_vecTeleportPosition != g_vecZero )
				{
					MakeTeleportEffect( m_vecTeleportPosition );
					break;
				}
			}

			TaskFail();
			break;

		case TASK_TOR_MAKE_GRUNT:
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, "debris/beamstart7.wav", 1, ATTN_NORM, 0, 100 );
			MakeGrunt( m_vecTeleportPosition );
			TaskComplete();
			break;

		default:
			CBaseMonster::StartTask( task );
			break;
	}
}

void CTor::RunTask( Task_t *task )
{
	if ( task->iTask == TASK_TOR_TELEPORT )
	{
		if ( m_fSequenceFinished )
			TaskComplete();
	}
	else
		CBaseMonster::RunTask( task );
}

Task_t tlTorSuppress[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
};

Schedule_t slTorSuppress[] = {
	{ tlTorSuppress, ARRAYSIZE( tlTorSuppress ), 0, 0, "Tor Suppress Hornet" },
};

Task_t tlTorRangeAttack1[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
};

Schedule_t slTorRangeAttack1[] = {
	{ tlTorRangeAttack1, ARRAYSIZE( tlTorRangeAttack1 ),
	  bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY | bits_COND_ENEMY_DEAD, 0, "Tor Range Attack1" },
};

Task_t tlTorStaffSlam[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_MELEE_ATTACK1, 0 },
};

Schedule_t slTorStaffSlam[] = {
	{ tlTorStaffSlam, ARRAYSIZE( tlTorStaffSlam ), 0, 0, "Staff Slam" },
};

Task_t tlTorTakeCoverFromEnemy[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 1 },
	{ TASK_REMEMBER, bits_MEMORY_INCOVER },
	{ TASK_FACE_ENEMY, 0 },
};

Schedule_t slTorTakeCoverFromEnemy[] = {
	{ tlTorTakeCoverFromEnemy, ARRAYSIZE( tlTorTakeCoverFromEnemy ), bits_COND_NEW_ENEMY, 0, "TorTakeCoverFromEnemy" },
};

Task_t tlTorVictoryDance[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, ACT_IDLE },
	{ TASK_SET_FAIL_SCHEDULE, SCHED_TOR_THREAT_DISPLAY },
	{ TASK_WAIT, 0.2f },
	{ TASK_TOR_GET_PATH_TO_ENEMY_CORPSE, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, ACT_CROUCH },
	{ TASK_PLAY_SEQUENCE, ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, ACT_STAND },
	{ TASK_PLAY_SEQUENCE, ACT_THREAT_DISPLAY },
	{ TASK_PLAY_SEQUENCE, ACT_CROUCH },
	{ TASK_PLAY_SEQUENCE, ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, ACT_VICTORY_DANCE },
	{ TASK_PLAY_SEQUENCE, ACT_STAND },
};

Schedule_t slTorVictoryDance[] = {
	{ tlTorVictoryDance, ARRAYSIZE( tlTorVictoryDance ),
	  bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY | bits_COND_HEAR_SOUND,
	  bits_SOUND_DANGER, "TorVictoryDance" },
};

Task_t tlTorThreatDisplay[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, ACT_SIGNAL1 },
};

Schedule_t slTorThreatDisplay[] = {
	{ tlTorThreatDisplay, ARRAYSIZE( tlTorThreatDisplay ),
	  bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY,
	  bits_SOUND_WORLD | bits_SOUND_COMBAT | bits_SOUND_PLAYER, "TorThreatDisplay" },
};

Task_t tlTorMakeFriends[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TOR_TELEPORT, 0 },
	{ TASK_TOR_MAKE_GRUNT, 0 },
	{ TASK_SET_ACTIVITY, ACT_IDLE },
	{ TASK_SET_SCHEDULE, SCHED_LOSE_ENEMY },
};

Schedule_t slTorMakeFriends[] = {
	{ tlTorMakeFriends, ARRAYSIZE( tlTorMakeFriends ), 0, 0, "Tor MakeFriends" },
};

DEFINE_CUSTOM_SCHEDULES( CTor )
{
	slTorSuppress,
	slTorRangeAttack1,
	slTorStaffSlam,
	slTorTakeCoverFromEnemy,
	slTorVictoryDance,
	slTorThreatDisplay,
	slTorMakeFriends,
};

IMPLEMENT_CUSTOM_SCHEDULES( CTor, CSquadMonster );

Schedule_t *CTor::GetScheduleOfType( int type )
{
	switch ( type )
	{
		case SCHED_VICTORY_DANCE:
			return slTorVictoryDance;
		case SCHED_TAKE_COVER_FROM_ENEMY:
			return slTorTakeCoverFromEnemy;
		case SCHED_MELEE_ATTACK1:
			return slTorStaffSlam;
		case SCHED_MELEE_ATTACK2:
			switch ( RANDOM_LONG( 0, 1 ) )
			{
				case 0:
					EMIT_SOUND( edict(), CHAN_VOICE, "tor/tor-attack1.wav", 1, ATTN_NORM );
					break;
				case 1:
					EMIT_SOUND( edict(), CHAN_VOICE, "tor/tor-attack2.wav", 1, ATTN_NORM );
					break;
			}
			break;
		case SCHED_RANGE_ATTACK1:
			return slTorRangeAttack1;
		case SCHED_RANGE_ATTACK2:
			return slTorMakeFriends;
		case SCHED_TOR_SUPPRESS:
			return slTorSuppress;
		case SCHED_TOR_THREAT_DISPLAY:
			return slTorThreatDisplay;
	}

	return CBaseMonster::GetScheduleOfType( type );
}

Schedule_t *CTor::GetSchedule( void )
{
	if ( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		CSound *sound = PBestSound();

		if ( sound && ( sound->m_iType & bits_SOUND_DANGER ) )
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
	}

	if ( m_MonsterState != MONSTERSTATE_COMBAT || HasConditions( bits_COND_ENEMY_DEAD ) )
		return CBaseMonster::GetSchedule();

	if ( HasConditions( bits_COND_NEW_ENEMY ) )
		return GetScheduleOfType( SCHED_WAKE_ANGRY );

	if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
	{
		AttackSound();
		return GetScheduleOfType( SCHED_MELEE_ATTACK1 );
	}

	if ( HasConditions( bits_COND_HEAVY_DAMAGE ) )
		return GetScheduleOfType( SCHED_SMALL_FLINCH );

	if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) )
		return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

	if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
		return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

	if ( HasConditions( bits_COND_ENEMY_TOOFAR ) )
		return GetScheduleOfType( SCHED_CHASE_ENEMY );

	return CBaseMonster::GetSchedule();
}

void CTor::HandleAnimEvent( MonsterEvent_t *event )
{
	switch ( event->event )
	{
		case 1:
			SonicBlast();
			break;

		case 2:
		case 13:
		{
			CBaseEntity *hurt = CheckTraceHullAttack( 100, sk_tor_punch.value, DMG_CLUB );

			if ( hurt )
			{
				if ( ( ( hurt->pev->flags & FL_CLIENT ) || ( ( hurt->pev->flags & FL_MONSTER ) && hurt->pev->movetype != MOVETYPE_FLY ) )
					&& ( hurt->IsPlayer() || hurt->IsAlive() ) )
				{
					hurt->pev->punchangle.y = 25;
					hurt->pev->punchangle.x = 8;
					hurt->pev->flags &= ~FL_ONGROUND;
					hurt->pev->velocity = hurt->pev->velocity + gpGlobals->v_right * 300 + gpGlobals->v_forward * 150 + gpGlobals->v_up * 200;
				}

				int pitch = 100 + RANDOM_LONG( -5, 5 );
				const char *sample = pAttackHitSounds[RANDOM_LONG( 0, ARRAYSIZE( pAttackHitSounds ) - 1 )];

				EMIT_SOUND_DYN( edict(), CHAN_WEAPON, sample, 1, ATTN_NORM, 0, pitch );
			}
			else
			{
				int pitch = 100 + RANDOM_LONG( -5, 5 );
				const char *sample = pAttackMissSounds[RANDOM_LONG( 0, ARRAYSIZE( pAttackMissSounds ) - 1 )];

				EMIT_SOUND_DYN( edict(), CHAN_WEAPON, sample, 1, ATTN_NORM, 0, pitch );
			}
			break;
		}

		case 3:
		{
			if ( TorHandleEntity( m_hEnemy.m_Handle ) == NULL )
				break;

			Vector angles;

			if ( HasConditions( bits_COND_SEE_ENEMY ) )
			{
				Vector direction = m_vecEnemyLKP - pev->origin;

				angles = UTIL_VecToAngles( direction );
				direction = direction.Normalize();
			}
			else
			{
				angles = pev->angles;
				UTIL_MakeAimVectors( angles );
			}

			AttackSound();
			pev->effects = EF_MUZZLEFLASH;

			if ( angles.x > 180 )
				angles.x -= 360;

			SetBlending( 0, angles.x );

			Vector armPosition, armAngles;

			GetAttachment( 0, armPosition, armAngles );
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, armPosition );
			WRITE_BYTE( TE_SPRITE );
			WRITE_COORD( armPosition.x );
			WRITE_COORD( armPosition.y );
			WRITE_COORD( armPosition.z );
			WRITE_SHORT( m_iMuzzleFlash );
			WRITE_BYTE( 6 );
			WRITE_BYTE( 128 );
			MESSAGE_END();

			ShootAtEnemy( armPosition );

			Vector start = pev->origin + Vector( 0, 0, 55 );
			Vector bodyTarget = TorHandleEntity( m_hEnemy.m_Handle )->BodyTarget( start );
			Vector target = bodyTarget - TorHandleEntity( m_hEnemy.m_Handle )->Center() + m_vecEnemyLKP;
			TraceResult trace;

			UTIL_TraceLine( start, target, dont_ignore_monsters, edict(), &trace );

			Vector end = trace.vecEndPos + trace.vecPlaneNormal * 32;

			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMPOINTS );
			WRITE_COORD( armPosition.x );
			WRITE_COORD( armPosition.y );
			WRITE_COORD( armPosition.z );
			WRITE_COORD( end.x );
			WRITE_COORD( end.y );
			WRITE_COORD( end.z );
			WRITE_SHORT( m_iEnergyBeam );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 15 );
			WRITE_BYTE( 10 );
			WRITE_BYTE( 32 );
			WRITE_BYTE( 5 );
			WRITE_BYTE( 217 );
			WRITE_BYTE( 226 );
			WRITE_BYTE( 146 );
			WRITE_BYTE( 128 );
			WRITE_BYTE( 15 );
			MESSAGE_END();

			CBeam *beam = CBeam::BeamCreate( "sprites/lgtning.spr", 50 );

			if ( beam )
			{
				beam->PointsInit( end, armPosition );
				beam->SetBrightness( 150 );
				beam->SetNoise( 10 );
				beam->SetFlags( 0x10 );
				beam->SetScrollRate( 150 );
				beam->SetColor( m_fPlayerAlly ? 194 : 96, m_fPlayerAlly ? 40 : 128, 16 );
				beam->SetThink( &CBaseEntity::SUB_Remove );
				beam->pev->nextthink = gpGlobals->time + 1;
				ExplosionBlast( end );
				m_iTorAmmo--;

				if ( m_iTorAmmo < 1 )
				{
					m_iTorAmmo = 10;
					m_fCanHornetAttack = false;
					m_flNextHornetAttackCheck = gpGlobals->time + 5;
				}
			}
			break;
		}

		case 7:
		{
			CBaseEntity *hurt = CheckTraceHullAttack( 100, sk_tor_punch.value * 2, DMG_CLUB | DMG_NEVERGIB );

			if ( hurt )
			{
				if ( ( hurt->pev->flags & FL_CLIENT ) || ( ( hurt->pev->flags & FL_MONSTER ) && hurt->pev->movetype != MOVETYPE_FLY ) )
				{
					hurt->pev->punchangle.x = -30;

					if ( hurt->IsPlayer() )
						hurt->pev->velocity = hurt->pev->velocity + gpGlobals->v_forward * 50;
				}

				int pitch = 100 + RANDOM_LONG( -5, 5 );

				EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM, 0, pitch );

				Vector armPosition, armAngles;

				GetAttachment( 0, armPosition, armAngles );
				SpawnBlood( armPosition, hurt->BloodColor(), 25 );
			}
			else
			{
				int pitch = 100 + RANDOM_LONG( -5, 5 );
				const char *sample = pAttackMissSounds[RANDOM_LONG( 0, ARRAYSIZE( pAttackMissSounds ) - 1 )];

				EMIT_SOUND_DYN( edict(), CHAN_WEAPON, sample, 1, ATTN_NORM, 0, pitch );
			}
			break;
		}

		case 10:
		case 11:
			switch ( RANDOM_LONG( 0, 1 ) )
			{
				case 0:
					EMIT_SOUND_DYN( edict(), CHAN_BODY, "tor/tor-foot.wav", 1, ATTN_NORM, 0, 90 );
					break;
				case 1:
					EMIT_SOUND_DYN( edict(), CHAN_BODY, "tor/tor-foot.wav", 1, ATTN_NORM, 0, 70 );
					break;
			}
			break;

		case 12:
		{
			CBaseEntity *hurt = CheckTraceHullAttack( 100, sk_tor_punch.value, DMG_CLUB );

			if ( hurt )
			{
				if ( ( hurt->pev->flags & FL_CLIENT ) || ( ( hurt->pev->flags & FL_MONSTER ) && hurt->pev->movetype != MOVETYPE_FLY ) )
				{
					hurt->pev->punchangle.y = -25;
					hurt->pev->punchangle.x = 8;

					if ( hurt->IsPlayer() )
						hurt->pev->velocity = hurt->pev->velocity + gpGlobals->v_right * 250;
				}

				int pitch = 100 + RANDOM_LONG( -5, 5 );
				const char *sample = pAttackHitSounds[RANDOM_LONG( 0, ARRAYSIZE( pAttackHitSounds ) - 1 )];

				EMIT_SOUND_DYN( edict(), CHAN_WEAPON, sample, 1, ATTN_NORM, 0, pitch );

				Vector armPosition, armAngles;

				GetAttachment( 0, armPosition, armAngles );
			}
			else
			{
				int pitch = 100 + RANDOM_LONG( -5, 5 );
				const char *sample = pAttackMissSounds[RANDOM_LONG( 0, ARRAYSIZE( pAttackMissSounds ) - 1 )];

				EMIT_SOUND_DYN( edict(), CHAN_WEAPON, sample, 1, ATTN_NORM, 0, pitch );
			}
			break;
		}

		default:
			CBaseMonster::HandleAnimEvent( event );
			break;
	}
}
