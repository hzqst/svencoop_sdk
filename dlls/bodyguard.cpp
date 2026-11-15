#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "bodyguard.h"
#include "schedule.h"
#include "defaultai.h"
#include "soundent.h"
#include "weapons.h"
#include "animation.h"

extern cvar_t sk_barney_health;
extern cvar_t sk_hgrunt_pellets;
extern cvar_t sk_hwgrunt_minipellets;

enum
{
	BG_WEAPON_PISTOL = 1,
	BG_WEAPON_DEAGLE,
	BG_WEAPON_SHOTGUN,
	BG_WEAPON_UZIS,
	BG_WEAPON_MP5,
	BG_WEAPON_SNIPER,
	BG_WEAPON_MINIGUN
};

enum
{
	SCHED_BG_RELOAD = LAST_TALKMONSTER_SCHEDULE + 1,
	TASK_BG_MINIGUN = LAST_COMMON_TASK + 1
};

static Task_t tlBodyGuardFollow[] =
{
	{ TASK_MOVE_TO_TARGET_RANGE, 128 },
	{ TASK_SET_SCHEDULE, SCHED_TARGET_FACE },
};

static Schedule_t slBodyGuardFollow[] =
{
	{
		tlBodyGuardFollow,
		ARRAYSIZE( tlBodyGuardFollow ),
		bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"Follow"
	},
};

static Task_t tlBodyGuardFaceTarget[] =
{
	{ TASK_SET_ACTIVITY, ACT_IDLE },
	{ TASK_FACE_TARGET, 0 },
	{ TASK_SET_ACTIVITY, ACT_IDLE },
	{ TASK_SET_SCHEDULE, SCHED_TARGET_CHASE },
};

static Schedule_t slBodyGuardFaceTarget[] =
{
	{
		tlBodyGuardFaceTarget,
		ARRAYSIZE( tlBodyGuardFaceTarget ),
		bits_COND_CLIENT_PUSH | bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE
			| bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"FaceTarget"
	},
};

static Task_t tlBodyGuardIdleStand[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, ACT_IDLE },
	{ TASK_WAIT, 2 },
	{ TASK_TLK_HEADRESET, 0 },
};

static Schedule_t slBodyGuardIdleStand[] =
{
	{
		tlBodyGuardIdleStand,
		ARRAYSIZE( tlBodyGuardIdleStand ),
		bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE
			| bits_COND_HEAR_SOUND | bits_COND_SMELL,
		bits_SOUND_COMBAT | bits_SOUND_DANGER | bits_SOUND_MEAT | bits_SOUND_CARCASS | bits_SOUND_GARBAGE,
		"IdleStand"
	},
};

static Task_t tlBodyGuardReload[] =
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

static Schedule_t slBodyGuardReload[] =
{
	{
		tlBodyGuardReload,
		ARRAYSIZE( tlBodyGuardReload ),
		bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"BODYGUARD Reload"
	},
};

static Task_t tlBodyGuardReloadQuick[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_RELOAD, 0 },
	{ TASK_FACE_ENEMY, 0 },
};

static Schedule_t slBodyGuardReloadQuick[] =
{
	{
		tlBodyGuardReloadQuick,
		ARRAYSIZE( tlBodyGuardReloadQuick ),
		bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"BODYGUARD Reload Quick"
	},
};

static Task_t tlBodyGuardMinigun[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_BG_MINIGUN, 0 },
};

static Schedule_t slBodyGuardMinigun[] =
{
	{
		tlBodyGuardMinigun,
		ARRAYSIZE( tlBodyGuardMinigun ),
		0,
		0,
		"Bodyguard Minigun Attack"
	},
};

LINK_ENTITY_TO_CLASS( monster_bodyguard, CBodyGuard );

TYPEDESCRIPTION CBodyGuard::m_SaveData[] =
{
	DEFINE_FIELD( CBodyGuard, m_painTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CBodyGuard, CTalkMonster );

DEFINE_CUSTOM_SCHEDULES( CBodyGuard )
{
	slBodyGuardFollow,
	slBodyGuardFaceTarget,
	slBodyGuardIdleStand,
	slBodyGuardReload,
	slBodyGuardReloadQuick,
	slBodyGuardMinigun,
};

IMPLEMENT_CUSTOM_SCHEDULES( CBodyGuard, CTalkMonster );

void CBodyGuard::Spawn( void )
{
	Precache();

	if ( !SetupModel() )
		SetupFriendly();

	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_RED;

	if ( pev->health == 0 )
		pev->health = 100;

	pev->view_ofs = Vector( 0, 0, 50 );
	m_flFieldOfView = VIEW_FIELD_WIDE;
	m_MonsterState = MONSTERSTATE_NONE;
	pev->body = 0;
	m_afCapability = bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP | bits_CAP_USE_TANK;
	m_fCheckCreatureDanger = true;
	m_afMoveShootCap = bits_MOVESHOOTCAP_RANGE_ATTACK1;
	m_HackedGunPos = Vector( 0, 0, 55 );

	if ( RANDOM_LONG( 0, 10 ) > 8 )
		pev->skin = 1;

	if ( !pev->weapons )
		pev->weapons = RANDOM_LONG( BG_WEAPON_PISTOL, BG_WEAPON_MINIGUN );

	switch ( pev->weapons )
	{
	case BG_WEAPON_PISTOL:
		m_cClipSize = 17;
		break;
	case BG_WEAPON_DEAGLE:
		m_cClipSize = 7;
		break;
	case BG_WEAPON_SHOTGUN:
		m_cClipSize = 8;
		break;
	case BG_WEAPON_UZIS:
		m_cClipSize = 32;
		break;
	case BG_WEAPON_MP5:
		m_cClipSize = 30;
		break;
	case BG_WEAPON_SNIPER:
		m_flDistLook = 4096;
		m_cClipSize = 1;
		break;
	case BG_WEAPON_MINIGUN:
		m_cClipSize = 0;
		m_afMoveShootCap = 0;
		break;
	}

	m_cAmmoLoaded = m_cClipSize;
	SetBodygroup( 2, pev->weapons );

	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( IsPlayerAlly() ? "Body Guard" : "Agent" );

	MonsterInit();
	SetUse( &CBodyGuard::CallFollowerPlayerUse );
}

void CBodyGuard::Precache( void )
{
	TalkInit();
	CTalkMonster::Precache();

	if ( !pev->modelindex )
		UTIL_PrecacheModel( this, "models/bgman.mdl" );

	static const char *sounds[] =
	{
		"bodyguard/pain1.ogg",
		"bodyguard/pain2.ogg",
		"bodyguard/pain3.ogg",
		"bodyguard/die1.ogg",
		"bodyguard/die2.ogg",
		"bodyguard/die3.ogg",
		"minigun/hw_shoot1.wav",
		"hassault/hw_spinup.wav",
		"hassault/hw_spindown.wav",
		"hassault/hw_spin.wav",
		"otis/ot_attack.wav",
		"weapons/hks1.wav",
		"weapons/hks2.wav",
		"weapons/hks3.wav",
		"weapons/sbarrel1.wav",
		"weapons/uzi/fire_both1.wav",
	};

	for ( int i = 0; i < ARRAYSIZE( sounds ); i++ )
		PrecacheSound_Intercept( this, sounds[i] );

	m_iShell = UTIL_PrecacheModel( this, "models/shell.mdl" );
	m_iShotgunShell = UTIL_PrecacheModel( this, "models/shotgunshell.mdl" );
}

void CBodyGuard::SetupFriendly( void )
{
	UTIL_SetModel( this, "models/bgman.mdl" );
}

int CBodyGuard::Classify( void )
{
	return GetClassification( CLASS_HUMAN_MILITARY );
}

int CBodyGuard::ObjectCaps( void )
{
	return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

int CBodyGuard::ISoundMask( void )
{
	return 0xff;
}

void CBodyGuard::SetPlayerAlly( bool state )
{
	SetPlayerAllyDirect( !state );
}

float CBodyGuard::GetPointsForDamage( float damage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	float healthFactor = pev->max_health / sk_barney_health.value;
	float fraction = damage / pev->max_health;

	return (float) ( (double) healthFactor * 2.5 * (double) fraction );
}

void CBodyGuard::TalkInit( void )
{
	CTalkMonster::TalkInit();
	m_szGrp[TLK_ANSWER] = "BG_ANSWER";
	m_szGrp[TLK_QUESTION] = "BG_QUESTION";
	m_szGrp[TLK_IDLE] = "BG_IDLE";
	m_szGrp[TLK_STARE] = "BG_STARE";
	m_szGrp[TLK_USE] = "BG_OK";
	m_szGrp[TLK_UNUSE] = "BG_WAIT";
	m_szGrp[TLK_STOP] = "BG_STOP";
	m_szGrp[TLK_NOSHOOT] = IsPlayerAlly() ? "BG_SCARED" : NULL;
	m_szGrp[TLK_HELLO] = "BG_HELLO";
	m_szGrp[TLK_PLHURT1] = "!BG_CUREA";
	m_szGrp[TLK_PLHURT2] = "!BG_CUREB";
	m_szGrp[TLK_PLHURT3] = "!BG_CUREC";
	m_szGrp[TLK_PHELLO] = NULL;
	m_szGrp[TLK_PIDLE] = NULL;
	m_szGrp[TLK_PQUESTION] = "BG_PQUEST";
	m_szGrp[TLK_SMELL] = "BG_SMELL";
	m_szGrp[TLK_WOUND] = "BG_WOUND";
	m_szGrp[TLK_MORTAL] = "BG_MORTAL";
	m_voicePitch = RANDOM_LONG( 0, 10 ) + 95;
}

void CBodyGuard::DeclineFollowing( void )
{
	PlaySentence( "BG_POK", 2, VOL_NORM, ATTN_NORM );
}

void CBodyGuard::AlertSound( void )
{
	if ( m_hEnemy != NULL && FOkToSpeak() )
		PlaySentence( "BG_ATTACK", RANDOM_FLOAT( 2.8, 3.2 ), VOL_NORM, ATTN_NORM );
}

void CBodyGuard::PainSound( void )
{
	if ( m_painTime > gpGlobals->time )
		return;

	m_painTime = gpGlobals->time + RANDOM_FLOAT( 0.5, 0.75 );

	static const char *sounds[] = { "bodyguard/pain1.ogg", "bodyguard/pain2.ogg", "bodyguard/pain3.ogg" };
	const char *sound = sounds[RANDOM_LONG( 0, 2 )];
	int pitch = GetVoicePitch();

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, sound, VOL_NORM, ATTN_NORM, 0, pitch );
}

void CBodyGuard::DeathSound( void )
{
	static const char *sounds[] = { "bodyguard/die1.ogg", "bodyguard/die2.ogg", "bodyguard/die3.ogg" };
	const char *sound = sounds[RANDOM_LONG( 0, 2 )];
	int pitch = GetVoicePitch();

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, sound, VOL_NORM, ATTN_NORM, 0, pitch );
}

void CBodyGuard::CheckAmmo( void )
{
	if ( m_cAmmoLoaded <= 0 && pev->weapons != BG_WEAPON_MINIGUN )
		SetConditions( bits_COND_NO_AMMO_LOADED );
}

BOOL CBodyGuard::FCanCheckAttacks( void )
{
	if ( !HasConditions( bits_COND_ENEMY_TOOFAR ) )
		return TRUE;

	return pev->weapons == BG_WEAPON_SNIPER && HasConditions( bits_COND_SEE_ENEMY );
}

BOOL CBodyGuard::CheckRangeAttack1( float dot, float distance )
{
	CBaseEntity *enemy = m_hEnemy;

	if ( !enemy )
		return FALSE;

	Vector source;
	bool ignoreGlass = true;

	switch ( pev->weapons )
	{
	case BG_WEAPON_PISTOL:
	case BG_WEAPON_DEAGLE:
		if ( distance > 2048 || dot < 0.5 || !NoFriendlyFire() )
			return FALSE;

		source = pev->origin + Vector( 0, 0, 55 );
		break;

	case BG_WEAPON_SHOTGUN:
	case BG_WEAPON_UZIS:
	case BG_WEAPON_MP5:
		if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) || dot < 0.5 || distance > 512 || !NoFriendlyFire() )
			return FALSE;

		source = pev->origin + Vector( 0, 0, 55 );
		break;

	case BG_WEAPON_SNIPER:
		source = GetGunPosition();
		break;

	case BG_WEAPON_MINIGUN:
		if ( m_flNextMinigunAttack > gpGlobals->time || distance > 2048 || !NoFriendlyFire() )
			return FALSE;

		source = pev->origin + Vector( 0, 0, 55 );
		ignoreGlass = false;
		break;

	default:
		return FALSE;
	}

	TraceResult trace;

	memset( &trace, 0, sizeof( trace ) );

	if ( ignoreGlass )
		UTIL_TraceLine( source, enemy->BodyTarget( source ), ignore_monsters, ignore_glass, edict(), &trace );
	else
		UTIL_TraceLine( source, enemy->BodyTarget( source ), ignore_monsters, edict(), &trace );

	return trace.flFraction == 1 || trace.pHit == enemy->edict();
}

void CBodyGuard::SetActivity( Activity activity )
{
	int sequence = ACTIVITY_NOT_AVAILABLE;

	switch ( activity )
	{
	case ACT_IDLE:
		if ( pev->weapons == BG_WEAPON_MINIGUN )
			sequence = LookupSequence( "idle_minigun" );
		else
		{
			if ( m_MonsterState == MONSTERSTATE_COMBAT && LookupActivity( ACT_IDLE_ANGRY ) != ACTIVITY_NOT_AVAILABLE )
				activity = ACT_IDLE_ANGRY;

			sequence = LookupActivity( activity );
		}
		break;

	case ACT_WALK:
	case ACT_RUN:
		sequence = pev->weapons == BG_WEAPON_MINIGUN ? LookupSequence( "walk_minigun" ) : LookupActivity( activity );
		break;

	case ACT_RANGE_ATTACK1:
		switch ( pev->weapons )
		{
		case BG_WEAPON_PISTOL:
			sequence = LookupSequence( "shoot_pistol" );
			break;
		case BG_WEAPON_DEAGLE:
			sequence = LookupSequence( "shoot_deagle" );
			break;
		case BG_WEAPON_SHOTGUN:
			sequence = LookupSequence( "shoot_shotgun" );
			break;
		case BG_WEAPON_UZIS:
			sequence = LookupSequence( "shoot_uzis" );
			break;
		case BG_WEAPON_MP5:
			sequence = LookupSequence( "shoot_mp5" );
			break;
		case BG_WEAPON_SNIPER:
			sequence = LookupSequence( "shoot_sniper" );
			break;
		case BG_WEAPON_MINIGUN:
			sequence = LookupSequence( "shoot_minigun" );
			break;
		}
		break;

	case ACT_RELOAD:
		switch ( pev->weapons )
		{
		case BG_WEAPON_PISTOL:
		case BG_WEAPON_DEAGLE:
			sequence = LookupSequence( "reload_pistol" );
			break;
		case BG_WEAPON_SHOTGUN:
			sequence = LookupSequence( "reload_shotgun" );
			break;
		case BG_WEAPON_UZIS:
			sequence = LookupSequence( "reload_uzis" );
			break;
		case BG_WEAPON_MP5:
			sequence = LookupSequence( "reload_mp5" );
			break;
		case BG_WEAPON_SNIPER:
			sequence = LookupSequence( "reload_sniper" );
			break;
		}
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
		pev->frame = 0;
		pev->sequence = sequence;
		ResetSequenceInfo();
		SetYawSpeed();
	}

	if ( m_Activity == ACT_RUN && pev->weapons == BG_WEAPON_MINIGUN )
		pev->framerate = 1.5;
}

void CBodyGuard::SetGaitActivity( Activity activity )
{
	int sequence = LookupActivity( activity );

	if ( activity == ACT_WALK || activity == ACT_RUN )
		sequence = pev->weapons == BG_WEAPON_MINIGUN ? LookupSequence( "walk_minigun" ) : LookupActivity( activity );

	if ( sequence < 0 )
		pev->gaitsequence = 255;
	else
	{
		pev->gaitsequence = sequence;
		ResetGaitSequenceInfo();
	}

	m_GaitActivity = activity;
	SetYawSpeed();
}

void CBodyGuard::SetYawSpeed( void )
{
	pev->yaw_speed = 360;
}

float CBodyGuard::ChangeYawNoDelay( int yawSpeed )
{
	float current = UTIL_AngleMod( pev->angles.y );
	float ideal = pev->ideal_yaw;

	if ( current == ideal )
		return 0;

	float speed = (float) yawSpeed * 0.035 * 10.0;
	float move = ideal - current;

	if ( ideal > current )
	{
		if ( move >= 180 )
			move -= 360;
	}
	else if ( move <= -180 )
		move += 360;

	if ( move > 0 )
		move = V_min( speed, move );
	else
		move = V_max( -speed, move );

	pev->angles.y = UTIL_AngleMod( current + move );

	if ( m_afCapability & bits_CAP_TURN_HEAD )
	{
		float yaw = ideal - pev->angles.y;

		if ( yaw > 180 )
			yaw -= 360;

		if ( yaw < -180 )
			yaw += 360;

		SetBoneController( 0, yaw );
	}

	return move;
}

bool CBodyGuard::FacingShootingIdeal( void )
{
	float difference = FlYawDiff();

	return fabsf( difference ) <= 20;
}

void CBodyGuard::HandleAnimEvent( MonsterEvent_t *event )
{
	if ( event->event != 3 && event->event != 4 )
	{
		CTalkMonster::HandleAnimEvent( event );
		return;
	}

	switch ( pev->weapons )
	{
	case BG_WEAPON_PISTOL:
		FirePistol();
		break;
	case BG_WEAPON_DEAGLE:
		FireDeagle();
		break;
	case BG_WEAPON_SHOTGUN:
		FireShotgun();
		break;
	case BG_WEAPON_UZIS:
		FireUziAkimbo();
		break;
	case BG_WEAPON_MP5:
		FireMp5();
		break;
	case BG_WEAPON_SNIPER:
		FireSniperRifle();
		break;
	case BG_WEAPON_MINIGUN:
		FireMinigun();
		break;
	}
}

Vector BodyGuardShellVelocity( void )
{
	float up = RANDOM_FLOAT( 40, 90 );
	Vector velocity = gpGlobals->v_up * up;
	float right = RANDOM_FLOAT( 75, 200 );

	velocity = gpGlobals->v_right * right + velocity;

	float forward = RANDOM_FLOAT( -40, 40 );

	return velocity + gpGlobals->v_forward * forward;
}

void CBodyGuard::FirePistol( void )
{
	Vector source = pev->origin + Vector( 0, 0, 55 );
	Vector direction = ShootAtEnemy( source );
	Vector angles = UTIL_VecToAngles( direction );

	FireBullets( 1, source, direction, VECTOR_CONE_2DEGREES, 2048, BULLET_MONSTER_9MM, 4, 0, NULL, FBDM_DRAW );
	EjectBrass( source + direction * 17, BodyGuardShellVelocity(), pev->angles.y, m_iShell, TE_BOUNCE_SHELL );

	int pitch = RANDOM_LONG( 0, 20 );

	pitch = pitch > 10 ? 100 : pitch + 95;
	SetBlending( 0, angles.x );
	pev->effects = EF_MUZZLEFLASH;
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 0.3, this );
	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "weapons/pl_gun3.wav", VOL_NORM, ATTN_NORM, 0, pitch );
	m_cAmmoLoaded--;
}

void CBodyGuard::FireDeagle( void )
{
	Vector source = pev->origin + Vector( 0, 0, 55 );
	Vector direction = ShootAtEnemy( source );
	Vector angles = UTIL_VecToAngles( direction );

	FireBullets( 1, source, direction, VECTOR_CONE_2DEGREES, 2048, BULLET_MONSTER_EAGLE, 4, 0, NULL, FBDM_DRAW );
	EjectBrass( source + direction * 17, BodyGuardShellVelocity(), pev->angles.y, m_iShell, TE_BOUNCE_SHELL );

	int pitch = RANDOM_LONG( 0, 20 );

	pitch = pitch > 10 ? 100 : pitch + 95;
	SetBlending( 0, angles.x );
	pev->effects = EF_MUZZLEFLASH;
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 0.3, this );
	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "otis/ot_attack.wav", VOL_NORM, ATTN_NORM, 0, pitch );
	m_cAmmoLoaded--;
}

void CBodyGuard::FireShotgun( void )
{
	if ( m_hEnemy == NULL )
		return;

	Vector source = GetGunPosition();
	Vector direction = ShootAtEnemy( source );

	UTIL_MakeVectors( pev->angles );
	EjectBrass( source + direction * 24, BodyGuardShellVelocity(), pev->angles.y, m_iShotgunShell,
	            TE_BOUNCE_SHOTSHELL );
	FireBullets( (int) sk_hgrunt_pellets.value, source, direction, VECTOR_CONE_15DEGREES, 2048, BULLET_MONSTER_BUCKSHOT,
	             0, 0, NULL, FBDM_DRAW );
	pev->effects |= EF_MUZZLEFLASH;
	m_cAmmoLoaded--;

	Vector angles = UTIL_VecToAngles( direction );

	SetBlending( 0, angles.x );
	EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/sbarrel1.wav", VOL_NORM, ATTN_NORM, 0, PITCH_NORM );
}

void CBodyGuard::FireMp5( void )
{
	if ( m_hEnemy == NULL )
		return;

	UTIL_MakeVectors( pev->angles );

	Vector source = GetGunPosition();
	Vector direction = ShootAtEnemy( source );

	EjectBrass( source + direction * 24, BodyGuardShellVelocity(), pev->angles.y, m_iShell, TE_BOUNCE_SHELL );
	FireBullets( 1, source, direction, VECTOR_CONE_5DEGREES, 2048, BULLET_MONSTER_MP5, 4, 0, NULL, FBDM_DRAW );

	Vector angles = UTIL_VecToAngles( direction );

	SetBlending( 0, angles.x );
	pev->effects |= EF_MUZZLEFLASH;
	m_cAmmoLoaded--;

	static const char *sounds[] = { "weapons/hks1.wav", "weapons/hks2.wav", "weapons/hks3.wav" };
	const char *sound = sounds[RANDOM_LONG( 0, 2 )];

	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, sound, VOL_NORM, ATTN_NORM, 0, PITCH_NORM );

	if ( pev->movetype != MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
	{
		m_fSequenceFinished = TRUE;
		m_Activity = ACT_RESET;
	}
}

void CBodyGuard::FireUziAkimbo( void )
{
	Vector source = pev->origin + Vector( 0, 0, 55 );
	Vector direction = ShootAtEnemy( source );
	Vector angles = UTIL_VecToAngles( direction );

	FireBullets( 1, source, direction, VECTOR_CONE_2DEGREES, 1024, BULLET_MONSTER_9MM, 4, 0, NULL, FBDM_DRAW );
	EjectBrass( source + direction * 17, BodyGuardShellVelocity(), pev->angles.y, m_iShell, TE_BOUNCE_SHELL );

	int pitch = RANDOM_LONG( 0, 20 );

	pitch = pitch > 10 ? 100 : pitch + 95;
	SetBlending( 0, angles.x );
	pev->effects = EF_MUZZLEFLASH;
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 0.3, this );

	const char *sound = RANDOM_LONG( 0, 1 ) ? "weapons/uzi/fire_both1.wav" : "weapons/uzi/fire_both2.wav";

	EMIT_SOUND_DYN( edict(), CHAN_AUTO, sound, VOL_NORM, ATTN_NORM, 0, pitch );
	m_cAmmoLoaded--;

	if ( pev->movetype != MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
	{
		m_fSequenceFinished = TRUE;
		m_Activity = ACT_RESET;
	}
}

void CBodyGuard::FireSniperRifle( void )
{
	CBaseEntity *enemy = m_hEnemy;

	if ( !enemy )
		return;

	UTIL_MakeVectors( pev->angles );

	int aimed = RANDOM_LONG( 0, 2 );
	Vector source = GetGunPosition();
	Vector direction = ShootAtEnemy( source );
	Vector spread = VECTOR_CONE_3DEGREES;

	if ( aimed > 0 )
	{
		direction = direction - enemy->pev->velocity / 15000;
		spread = VECTOR_CONE_1DEGREES;
	}

	EjectBrass( source + direction * 24, BodyGuardShellVelocity(), pev->angles.y, m_iShell, TE_BOUNCE_SHELL );
	FireBullets( 1, source, direction, spread, 4096, BULLET_MONSTER_SNIPER, 0, 0, NULL, FBDM_DRAW );

	Vector angles = UTIL_VecToAngles( direction );

	SetBlending( 0, angles.x );
	pev->effects |= EF_MUZZLEFLASH;
	EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/sniper_fire.wav", VOL_NORM, 0.4, 0, PITCH_NORM );
	m_cAmmoLoaded--;
}

void CBodyGuard::FireMinigun( void )
{
	UTIL_MakeVectors( pev->angles );

	Vector source = pev->origin + Vector( 0, 0, 55 );
	Vector direction = ShootAtEnemy( source );

	EjectBrass( source - direction * 24, BodyGuardShellVelocity(), pev->angles.y, m_iShell, TE_BOUNCE_SHELL );
	FireBullets( (int) sk_hwgrunt_minipellets.value, source, direction, VECTOR_CONE_6DEGREES, 4096, BULLET_MONSTER_SAW,
	             4, -1, NULL, FBDM_DRAW );
	pev->effects |= EF_MUZZLEFLASH;
	EMIT_SOUND_DYN( edict(), RANDOM_LONG( 2, 3 ) + 1, "minigun/hw_shoot1.wav", VOL_NORM, 0.4, 0, PITCH_NORM );

	Vector angles = UTIL_VecToAngles( ShootAtEnemy( Center() ) );

	SetBlending( 0, angles.x );
}

void CBodyGuard::StartTask( Task_t *task )
{
	if ( task->iTask != TASK_BG_MINIGUN )
	{
		CTalkMonster::StartTask( task );
		return;
	}

	m_fMinigunStarted = false;
	m_flThinkDelay = 0.1;
}

void CBodyGuard::RunTask( Task_t *task )
{
	if ( task->iTask == TASK_RANGE_ATTACK1 && !NoFriendlyFire() )
	{
		ChangeSchedule( GetScheduleOfType( SCHED_FIND_ATTACK_POINT ) );
		return;
	}

	if ( task->iTask == TASK_RELOAD )
	{
		MakeIdealYaw( m_vecEnemyLKP );
		ChangeYaw( pev->yaw_speed );

		if ( m_fSequenceFinished )
		{
			m_cAmmoLoaded = m_cClipSize;
			ClearConditions( bits_COND_NO_AMMO_LOADED );
			TaskComplete();
		}

		return;
	}

	if ( task->iTask != TASK_BG_MINIGUN )
	{
		CTalkMonster::RunTask( task );
		return;
	}

	if ( !m_fMinigunStarted )
	{
		int pitch = RANDOM_LONG( -5, 5 ) + PITCH_NORM;

		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "hassault/hw_spinup.wav", VOL_NORM, ATTN_NORM, 0, pitch );
		SetSequenceByName( "shoot_minigun_spinup" );
		MakeIdealYaw( m_vecEnemyLKP );

		Vector angles = UTIL_VecToAngles( ShootAtEnemy( Center() ) );

		SetBlending( 0, angles.x );

		if ( !FacingIdeal() )
			ChangeYawNoDelay( 70 );

		m_fMinigunStarted = true;
		m_flNextMinigunAttack = gpGlobals->time + 0.8;
		m_flSpinupEnd = m_flNextMinigunAttack;
		return;
	}

	CBaseEntity *enemy = m_hEnemy;

	if ( enemy && !HasConditions( bits_COND_ENEMY_OCCLUDED ) && gpGlobals->time >= m_flSpinupEnd )
	{
		m_flThinkDelay = 0;

		if ( gpGlobals->time < m_flNextMinigunAttack )
			return;

		if ( FacingShootingIdeal() )
		{
			FireMinigun();
			m_IdealActivity = ACT_RANGE_ATTACK1;
			m_flNextMinigunAttack = gpGlobals->time + 0.06;
		}
		else if ( gpGlobals->time >= m_flNextSpinSound )
		{
			EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "hassault/hw_spin.wav", VOL_NORM, ATTN_NORM, 0, PITCH_NORM );
			m_flNextSpinSound = gpGlobals->time + 0.4;
		}

		if ( enemy->pev->deadflag == DEAD_DYING || !enemy->IsAlive() || !NoFriendlyFire( pev->origin ) )
		{
			CBaseEntity *previous = enemy;

			Look( 4092 );
			m_hEnemy = BestVisibleEnemySpecial();
			enemy = m_hEnemy;

			if ( enemy )
				m_vecEnemyLKP = enemy->Center();
			else if ( previous->IsAlive() )
			{
				m_hEnemy = previous;
				m_flThinkDelay = 0.1;

				int pitch = RANDOM_LONG( -5, 5 ) + PITCH_NORM;

				EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "hassault/hw_spindown.wav", VOL_NORM, ATTN_NORM, 0, pitch );
				TaskComplete();
			}
		}

		MakeIdealYaw( m_vecEnemyLKP );
		SetTurnActivity();

		if ( !FacingIdeal() )
			ChangeYawNoDelay( 120 );

		return;
	}

	CBaseEntity *previous = enemy;

	Look( 4092 );
	m_hEnemy = BestVisibleEnemy();
	enemy = m_hEnemy;

	if ( enemy )
		m_vecEnemyLKP = enemy->pev->origin;
	else if ( previous && previous->IsAlive() )
	{
		m_hEnemy = previous;
		enemy = previous;
	}

	if ( gpGlobals->time < m_flSpinupEnd || ( enemy && NoFriendlyFire( pev->origin ) && FVisible( enemy, true ) ) )
	{
		if ( gpGlobals->time > m_flSpinupEnd && gpGlobals->time >= m_flNextSpinSound )
		{
			EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "hassault/hw_spin.wav", VOL_NORM, ATTN_NORM, 0, PITCH_NORM );
			m_flNextSpinSound = gpGlobals->time + 0.4;
		}

		if ( !FacingIdeal() )
			ChangeYawNoDelay( 70 );

		return;
	}

	m_flThinkDelay = 0.1;

	int pitch = RANDOM_LONG( -5, 5 ) + PITCH_NORM;

	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "hassault/hw_spindown.wav", VOL_NORM, ATTN_NORM, 0, pitch );
	TaskComplete();
}

CBaseEntity *CBodyGuard::BestVisibleEnemySpecial( void )
{
	CBaseEntity *best = NULL;
	int bestRelationship = R_NO;
	int bestDistance = 65536;

	for ( CBaseEntity *entity = m_pLink; entity; entity = entity->m_pLink )
	{
		if ( !entity->IsAlive() )
			continue;

		int relationship = IRelationship( entity );

		if ( relationship > bestRelationship || relationship == R_FR )
		{
			if ( NoFriendlyFire( pev->origin, entity ) )
			{
				bestRelationship = relationship;
				bestDistance = (int) ( entity->pev->origin - pev->origin ).Length();
				best = entity;
			}
		}
		else if ( relationship == bestRelationship && relationship > R_NO && NoFriendlyFire( pev->origin, entity ) )
		{
			int distance = (int) ( entity->pev->origin - pev->origin ).Length();

			if ( distance < bestDistance )
			{
				bestDistance = distance;
				bestRelationship = relationship;
				best = entity;
			}
		}
	}

	return best;
}

Schedule_t *CBodyGuard::GetScheduleOfType( int type )
{
	switch ( type )
	{
	case SCHED_IDLE_STAND:
	{
		Schedule_t *schedule = CTalkMonster::GetScheduleOfType( type );

		return schedule == slIdleStand ? slBodyGuardIdleStand : schedule;
	}

	case SCHED_TARGET_FACE:
	{
		Schedule_t *schedule = CTalkMonster::GetScheduleOfType( type );

		return schedule == slIdleStand ? slBodyGuardFaceTarget : schedule;
	}

	case SCHED_TARGET_CHASE:
		return slBodyGuardFollow;

	case SCHED_RELOAD:
		return slBodyGuardReloadQuick;

	case SCHED_BG_RELOAD:
		return slBodyGuardReload;
	}

	return CTalkMonster::GetScheduleOfType( type );
}

Schedule_t *CBodyGuard::GetSchedule( void )
{
	if ( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		CSound *sound = PBestSound();

		if ( sound && ( sound->m_iType & bits_SOUND_DANGER ) )
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
	}

	if ( HasConditions( bits_COND_ENEMY_DEAD ) && FOkToSpeak() )
		PlaySentence( "BG_KILL", 4, VOL_NORM, ATTN_NORM );

	switch ( m_MonsterState )
	{
	case MONSTERSTATE_IDLE:
	case MONSTERSTATE_ALERT:
		if ( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
			return GetScheduleOfType( SCHED_SMALL_FLINCH );

		if ( ( m_hEnemy != NULL && HasConditions( bits_COND_SEE_ENEMY ) ) || !IsPlayerFollowing() )
		{
			if ( HasConditions( bits_COND_CLIENT_PUSH ) )
				return GetScheduleOfType( SCHED_MOVE_AWAY );

			TrySmellTalk();

			if ( m_cAmmoLoaded != m_cClipSize )
				return GetScheduleOfType( SCHED_BG_RELOAD );

			return CTalkMonster::GetSchedule();
		}

		if ( !m_hTargetEnt->IsAlive() )
		{
			StopPlayerFollowing( false );
			return CTalkMonster::GetSchedule();
		}

		return GetScheduleOfType( HasConditions( bits_COND_CLIENT_PUSH ) ? SCHED_MOVE_AWAY_FOLLOW : SCHED_TARGET_FACE );

	case MONSTERSTATE_COMBAT:
		if ( HasConditions( bits_COND_ENEMY_DEAD ) )
			return CTalkMonster::GetSchedule();

		if ( HasConditions( bits_COND_NEW_ENEMY ) && HasConditions( bits_COND_LIGHT_DAMAGE ) )
			return GetScheduleOfType( SCHED_SMALL_FLINCH );

		if ( HasConditions( bits_COND_HEAVY_DAMAGE ) )
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );

		if ( HasConditions( bits_COND_NO_AMMO_LOADED ) )
			return GetScheduleOfType( SCHED_BG_RELOAD );

		if ( pev->weapons == BG_WEAPON_MINIGUN && HasConditions( bits_COND_CAN_ATTACK ) )
			return slBodyGuardMinigun;
		break;
	}

	return CTalkMonster::GetSchedule();
}

void CBodyGuard::TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType )
{
	switch ( trace->iHitgroup )
	{
	case HITGROUP_HEAD:
	case 10:
		damage *= 0.7;
		break;

	case HITGROUP_CHEST:
	case HITGROUP_STOMACH:
		if ( damageType & ( DMG_BULLET | DMG_SLASH | DMG_BLAST ) )
		{
			if ( damage >= 2 )
				damage -= 2;

			damage *= 0.5f;
		}
		break;
	}

	CTalkMonster::TraceAttack( attacker, damage, direction, trace, damageType );
}

int CBodyGuard::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	if ( !attacker )
		return 0;

	edict_t *attackerEdict = ENT( attacker );

	if ( !attackerEdict )
		attackerEdict = INDEXENT( 0 );

	CBaseEntity *attackerEntity =
	    attackerEdict && !attackerEdict->free ? (CBaseEntity *) GET_PRIVATE( attackerEdict ) : NULL;

	if ( CheckAttacker( attackerEntity ) )
		return 0;

	int result = CTalkMonster::TakeDamage( inflictor, attacker, damage, damageType );

	if ( ( ( !IsAlive() || pev->deadflag == DEAD_DYING ) && !IsPlayerAlly() ) || m_MonsterState == MONSTERSTATE_PRONE ||
	     !( attacker->flags & FL_CLIENT ) || !IsPlayerAlly() )
		return result;

	CBaseEntity *enemy = m_hEnemy;

	if ( enemy )
	{
		if ( !enemy->IsPlayer() && pev->deadflag == DEAD_NO )
			PlaySentence( "BG_SHOT", 4, VOL_NORM, ATTN_NORM );

		return result;
	}

	if ( pev->deadflag != DEAD_NO )
		return result;

	if ( HasMemory( bits_MEMORY_SUSPICIOUS ) || ( attackerEntity && attackerEntity->IsFacing( pev, 0.96f ) ) )
	{
		PlaySentence( "BG_MAD", 4, VOL_NORM, ATTN_NORM );
		Remember( bits_MEMORY_PROVOKED );
		StopPlayerFollowing( true );
	}
	else
	{
		PlaySentence( "BG_SHOT", 4, VOL_NORM, ATTN_NORM );
		Remember( bits_MEMORY_SUSPICIOUS );
	}

	return result;
}

void CBodyGuard::Killed( entvars_t *attacker, int gib )
{
	if ( GetBodygroup( 2 ) != 0 )
	{
		SetBodygroup( 2, 0 );

		static const char *weapons[] =
		{
			NULL,
			"weapon_9mmhandgun",
			"weapon_eagle",
			"weapon_shotgun",
			"weapon_uziakimbo",
			"weapon_9mmAR",
			"weapon_sniperrifle",
			"weapon_minigun",
		};

		if ( pev->weapons >= BG_WEAPON_PISTOL && pev->weapons <= BG_WEAPON_MINIGUN )
			DropWeapon( weapons[pev->weapons], false );
	}

	CTalkMonster::Killed( attacker, gib );
}
