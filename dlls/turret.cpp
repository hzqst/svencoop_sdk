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
/*

===== turret.cpp ========================================================

*/

//
// TODO: 
//		Take advantage of new monster fields like m_hEnemy and get rid of that OFFSET() stuff
//		Revisit enemy validation stuff, maybe it's not necessary with the newest monster code
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "effects.h"
#include "TypedEntityHandle.h"
#include "turret.h"
#include "soundent.h"
#include "player.h"
#include "pipewrench.h"

extern cvar_t sk_turret_health;
extern cvar_t sk_miniturret_health;
extern cvar_t sk_sentry_health;

extern Vector VecBModelOrigin( entvars_t* pevBModel );

#define TURRET_SHOTS	2
#define TURRET_RANGE	(100 * 12)
#define TURRET_SPREAD	Vector( 0, 0, 0 )
#define TURRET_TURNRATE	30		//angles per 0.1 second
#define TURRET_MAXWAIT	15		// seconds turret will stay active w/o a target
#define TURRET_MAXSPIN	5		// seconds turret barrel will spin w/o a target
#define TURRET_MACHINE_VOLUME	0.5

typedef enum
{
	TURRET_ANIM_NONE = 0,
	TURRET_ANIM_FIRE,
	TURRET_ANIM_SPIN,
	TURRET_ANIM_DEPLOY,
	TURRET_ANIM_RETIRE,
	TURRET_ANIM_DIE,
} TURRET_ANIM;

class CBaseTurret : public CBaseMonster
{
public:
	void Spawn(void);
	virtual void Precache(void);
	void UpdateOnRemove( void );
	void RemoveEffects( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT TurretUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	virtual void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	virtual int	 TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	virtual int	 Classify(void);
	void BecomeDead( float flSavedHealth );

	int BloodColor( void ) { return DONT_BLEED; }
	void GibMonster( void ) {}	// UNDONE: Throw turret gibs?

	// Think functions

	void EXPORT ActiveThink(void);
	void EXPORT SearchThink(void);
	void EXPORT AutoSearchThink(void);
	virtual void EXPORT TurretDeath(void);
	void EXPORT CallTurretDeath( void ) { TurretDeath(); }

	virtual void EXPORT SpinDownCall(void) { m_iSpin = 0; }
	virtual void EXPORT SpinUpCall(void) { m_iSpin = 1; }

	// void SpinDown(void);
	// float EXPORT SpinDownCall( void ) { return SpinDown(); }

	// virtual float SpinDown(void) { return 0;}
	// virtual float Retire(void) { return 0;}

	void EXPORT Deploy(void);
	void EXPORT Retire(void);
	
	void EXPORT Initialize(void);

	virtual void Ping(void);
	virtual void EyeOn(void);
	virtual void EyeOff(void);

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];

	// other functions
	void SetTurretAnim(TURRET_ANIM anim);
	int MoveTurret(void);
	virtual void Shoot(Vector &vecSrc, Vector &vecDirToEnemy) { };

	float m_flMaxSpin;		// Max time to spin the barrel w/o a target
	int m_iSpin;

	CEntityHandle<CSprite>	m_hEyeGlow;
	int						m_eyeBrightness;

	int	m_iDeployHeight;
	int	m_iRetractHeight;
	int m_iMinPitch;

	int m_iBaseTurnRate;	// angles per second
	float m_fTurnRate;		// actual turn rate
	int m_iOrientation;		// 0 = floor, 1 = Ceiling
	bool m_bOn;
	int m_fBeserk;			// Sometimes this bitch will just freak out
	bool m_bAutoStart;		// true if the turret auto deploys when a target
							// enters its range

	int m_iAttackRange;

	Vector m_vecLastSight;
	float m_flLastSight;	// Last time we saw a target
	float m_flMaxWait;		// Max time to seach w/o a target
	int m_iSearchSpeed;		// Not Used!

	// movement
	float	m_flStartYaw;
	Vector	m_vecCurAngles;
	Vector	m_vecGoalAngles;


	float	m_flPingTime;	// Time until the next ping, used when searching
	float	m_flSpinUpTime;	// Amount of time until the barrel should spin down when searching
};

void SquadMakerInitializeTurret( CBaseEntity *entity, int orientation )
{
	CBaseTurret *turret = static_cast<CBaseTurret *>( entity );

	turret->pev->spawnflags |= SF_MONSTER_TURRET_AUTOACTIVATE | SF_MONSTER_TURRET_STARTINACTIVE;

	if ( orientation == 1 )
	{
		turret->pev->origin.z -= 0.1;
		UTIL_SetOrigin( turret->pev, turret->pev->origin );
		turret->m_iOrientation = 1;
	}

	turret->Initialize();
}

TYPEDESCRIPTION	CBaseTurret::m_SaveData[] = 
{
	DEFINE_FIELD( CBaseTurret, m_flMaxSpin, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseTurret, m_iSpin, FIELD_INTEGER ),

	DEFINE_FIELD( CBaseTurret, m_hEyeGlow, FIELD_EHANDLE ),
	DEFINE_FIELD( CBaseTurret, m_eyeBrightness, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseTurret, m_iDeployHeight, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseTurret, m_iRetractHeight, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseTurret, m_iMinPitch, FIELD_INTEGER ),

	DEFINE_FIELD( CBaseTurret, m_iBaseTurnRate, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseTurret, m_fTurnRate, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseTurret, m_iOrientation, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseTurret, m_bOn, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseTurret, m_fBeserk, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseTurret, m_bAutoStart, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseTurret, m_iAttackRange, FIELD_INTEGER ),


	DEFINE_FIELD( CBaseTurret, m_vecLastSight, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CBaseTurret, m_flLastSight, FIELD_TIME ),
	DEFINE_FIELD( CBaseTurret, m_flMaxWait, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseTurret, m_iSearchSpeed, FIELD_INTEGER ),

	DEFINE_FIELD( CBaseTurret, m_flStartYaw, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseTurret, m_vecCurAngles, FIELD_VECTOR ),
	DEFINE_FIELD( CBaseTurret, m_vecGoalAngles, FIELD_VECTOR ),

	DEFINE_FIELD( CBaseTurret, m_flPingTime, FIELD_TIME ),
	DEFINE_FIELD( CBaseTurret, m_flSpinUpTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CBaseTurret, CBaseMonster );

class CTurret : public CBaseTurret
{
public:
	void Spawn(void);
	void Precache(void);
	void CreateEye( void );
	void UpdateOnRemove( void );
	float GetPointsForDamage( float flDamage );
	// Think functions
	void SpinUpCall(void);
	void SpinDownCall(void);

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];

	// other functions
	void Shoot(Vector &vecSrc, Vector &vecDirToEnemy);

private:
	int m_iStartSpin;

};
TYPEDESCRIPTION	CTurret::m_SaveData[] = 
{
	DEFINE_FIELD( CTurret, m_iStartSpin, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CTurret, CBaseTurret );


class CMiniTurret : public CBaseTurret
{
public:
	void Spawn( );
	void Precache(void);
	float GetPointsForDamage( float flDamage );
	// other functions
	void Shoot(Vector &vecSrc, Vector &vecDirToEnemy);
};


LINK_ENTITY_TO_CLASS( monster_turret, CTurret );
LINK_ENTITY_TO_CLASS( monster_miniturret, CMiniTurret );

void CBaseTurret::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "maxsleep"))
	{
		if ( atof( pkvd->szValue ) > 0 )
			m_flMaxWait = atof(pkvd->szValue);

		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "orientation"))
	{
		m_iOrientation = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;

	}
	else if (FStrEq(pkvd->szKeyName, "searchspeed"))
	{
		if ( atoi( pkvd->szValue ) > 0 )
			m_iSearchSpeed = atoi(pkvd->szValue);

		pkvd->fHandled = TRUE;

	}
	else if (FStrEq(pkvd->szKeyName, "turnrate"))
	{
		if ( atoi( pkvd->szValue ) > 0 )
			m_iBaseTurnRate = atoi(pkvd->szValue);

		pkvd->fHandled = TRUE;
	}
	else if ( !stricmp( pkvd->szKeyName, "attackrange" ) )
	{
		if ( atoi( pkvd->szValue ) > 0 )
			m_iAttackRange = atoi( pkvd->szValue );

		pkvd->fHandled = TRUE;
	}
	else if ( !stricmp( pkvd->szKeyName, "style" ) ||
			  !stricmp( pkvd->szKeyName, "height" ) ||
			  !stricmp( pkvd->szKeyName, "value1" ) ||
			  !stricmp( pkvd->szKeyName, "value2" ) ||
			  !stricmp( pkvd->szKeyName, "value3" ) )
		pkvd->fHandled = TRUE;
	else
		CBaseMonster::KeyValue( pkvd );
}


void CBaseTurret::Spawn()
{
	Precache( );
	pev->nextthink		= gpGlobals->time + 1.0f;
	pev->movetype		= MOVETYPE_FLY;
	pev->sequence		= 0;
	pev->frame			= 0;
	pev->solid			= SOLID_SLIDEBOX;
	pev->takedamage		= DAMAGE_AIM;
	pev->gaitsequence = -1;
	pev->max_health = pev->health;

	if (pev->scale <= 0.0f )
		pev->scale = 1.0f;

	pev->flags |= FL_MONSTER;
	SetUse( &CBaseTurret::TurretUse );

	if (( ( pev->spawnflags & SF_MONSTER_TURRET_AUTOACTIVATE ) || !pev->targetname )
		 && !( pev->spawnflags & SF_MONSTER_TURRET_STARTINACTIVE ))
	{
		m_bAutoStart = TRUE;
	}

	ResetSequenceInfo( );
	SetBoneController( 0, 0 );
	SetBoneController( 1, 0 );
	m_flFieldOfView = VIEW_FIELD_FULL;
	// m_flSightRange = TURRET_RANGE;
}


void CBaseTurret::Precache( )
{
	CBaseEntity::Precache();
	PrecacheSound_Intercept( this, "turret/tu_ping.wav");
	PrecacheSound_Intercept( this, "turret/tu_active2.wav");
	PrecacheSound_Intercept( this, "turret/tu_die.wav");
	PrecacheSound_Intercept( this, "turret/tu_die2.wav");
	PrecacheSound_Intercept( this, "turret/tu_die3.wav");
	PrecacheSound_Intercept( this, "turret/tu_deploy.wav");
	PrecacheSound_Intercept( this, "turret/tu_spinup.wav");
	PrecacheSound_Intercept( this, "turret/tu_spindown.wav");
	PrecacheSound_Intercept( this, "turret/tu_search.wav");
	PrecacheSound_Intercept( this, "turret/tu_alert.wav");
	PrecacheSound_Intercept( this, "debris/metal6.wav" );
}

void CBaseTurret::RemoveEffects( void )
{
	if ( m_hEyeGlow != NULL )
	{
		UTIL_Remove( m_hEyeGlow );
		m_hEyeGlow = NULL;
	}
}

void CBaseTurret::UpdateOnRemove( void )
{
	CBaseEntity::UpdateOnRemove();
	RemoveEffects();
}

#define TURRET_GLOW_SPRITE "sprites/flare3.spr"

void CTurret::Spawn()
{ 
	Precache( );

	if ( !SetupModel() )
		SetModel_Intercept( edict(), "models/turret.mdl");

	if ( pev->health == 0.0f )
		pev->health			= sk_turret_health.value;
	m_HackedGunPos		= Vector( 0, 0, 12.75 );
	m_flMaxSpin =		TURRET_MAXSPIN;
	pev->view_ofs.z = 12.75;

	CBaseTurret::Spawn( );

	m_iRetractHeight = 16;
	m_iDeployHeight = 32;
	m_iMinPitch	= -15;
	UTIL_SetSize(pev, Vector(-32, -32, -m_iRetractHeight), Vector(32, 32, m_iRetractHeight));
	
	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( "Turret" );

	Initialize();
	CreateEye();

	pev->nextthink = gpGlobals->time + 0.1;
}

void CTurret::CreateEye( void )
{
	RemoveEffects();
	m_hEyeGlow = CSprite::SpriteCreate( TURRET_GLOW_SPRITE, pev->origin, false, 0.0f );

	CSprite *eye = m_hEyeGlow;

	eye->SetTransparency( kRenderGlow, 255, 0, 0, 0, kRenderFxNoDissipation );
	eye->SetAttachment( edict(), 2 );
	m_eyeBrightness = 0;
}

void CTurret::UpdateOnRemove( void )
{
	CBaseTurret::UpdateOnRemove();
	STOP_SOUND( edict(), CHAN_VOICE, "turret/tu_active2.wav" );
}

void CTurret::Precache()
{
	CBaseTurret::Precache( );
	UTIL_PrecacheModel( this, "models/turret.mdl");
	UTIL_PrecacheModel( this, TURRET_GLOW_SPRITE);
	PrecacheSound_Intercept( this, "turret/tu_fire1.wav" );
}

void CMiniTurret::Spawn()
{ 
	Precache( );

	if ( !SetupModel() )
		SetModel_Intercept( edict(), "models/miniturret.mdl");

	if ( pev->health == 0.0f )
		pev->health			= sk_miniturret_health.value;
	m_HackedGunPos		= Vector( 0, 0, 12.75 );
	m_flMaxSpin = 0;
	pev->view_ofs.z = 12.75;

	CBaseTurret::Spawn( );
	m_iRetractHeight = 16;
	m_iDeployHeight = 32;
	m_iMinPitch	= -15;
	UTIL_SetSize(pev, Vector(-16, -16, -m_iRetractHeight), Vector(16, 16, m_iRetractHeight));

	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( "Mini-Turret" );

	Initialize();
	pev->nextthink = gpGlobals->time + 0.1;
}


void CMiniTurret::Precache()
{
	CBaseTurret::Precache( );

	if ( pev->modelindex == 0 )
		UTIL_PrecacheModel( this, "models/miniturret.mdl");
	PRECACHE_SOUND("weapons/hks_hl3.wav");
}

void CBaseTurret::Initialize(void)
{
	m_bOn = 0;
	m_fBeserk = 0;
	m_iSpin = 0;
	pev->deadflag = DEAD_NO;

	SetBoneController( 0, 0 );
	SetBoneController( 1, 0 );

	if (m_iBaseTurnRate == 0) m_iBaseTurnRate = TURRET_TURNRATE;
	if (m_flMaxWait == 0) m_flMaxWait = TURRET_MAXWAIT;
	m_flStartYaw = pev->angles.y;
	if (m_iOrientation == 1)
	{
		pev->idealpitch = 180;
		pev->angles.x = 180;
		m_HackedGunPos.z = -m_HackedGunPos.z;
		pev->view_ofs.z = -pev->view_ofs.z;
		pev->effects |= EF_INVLIGHT;
		pev->angles.y = pev->angles.y + 180;
		if (pev->angles.y > 360)
			pev->angles.y = pev->angles.y - 360;
	}

	if ( m_iAttackRange == 0 )
		m_iAttackRange = 1200;

	m_vecGoalAngles.x = 0;

	if ( !FClassnameIs( pev, "monster_sentry" ) )
	{
		UTIL_MakeVectors( pev->angles );

		if ( m_iOrientation == 1 )
			pev->origin = pev->origin + gpGlobals->v_forward * 0.1f;
		else
			pev->origin = pev->origin - gpGlobals->v_forward * 0.1f;
	}

	if ( m_bAutoStart )
	{
		m_flLastSight = gpGlobals->time + m_flMaxWait;
		SetThink(&CBaseTurret::AutoSearchThink);
		pev->nextthink = gpGlobals->time + 0.1;
	}
	else
	{
		SetThink(&CBaseTurret::SUB_DoNothing);
		pev->flags |= FL_NOTARGET;
	}
}

void CBaseTurret::TurretUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_bOn ) )
		return;

	if (m_bOn)
	{
		m_hEnemy = NULL;
		pev->nextthink = gpGlobals->time + 0.1;
		m_bAutoStart = FALSE;// switching off a turret disables autostart
		//!!!! this should spin down first!!BUGBUG
		SetThink(&CBaseTurret::Retire);
	}
	else 
	{
		pev->nextthink = gpGlobals->time + 0.1; // turn on delay

		// if the turret is flagged as an autoactivate turret, re-enable it's ability open self.
		if ( ( pev->spawnflags & SF_MONSTER_TURRET_AUTOACTIVATE ) || !pev->targetname )
		{
			m_bAutoStart = TRUE;
		}
		
		SetThink(&CBaseTurret::Deploy);
	}
}


void CBaseTurret::Ping( void )
{
	// make the pinging noise every second while searching
	if (m_flPingTime == 0)
		m_flPingTime = gpGlobals->time + 1;
	else if (m_flPingTime <= gpGlobals->time)
	{
		m_flPingTime = gpGlobals->time + 1;
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "turret/tu_ping.wav", 1, ATTN_NORM);
		EyeOn( );
	}
	else if (m_eyeBrightness > 0)
	{
		EyeOff( );
	}
}


void CBaseTurret::EyeOn( void )
{
	CSprite *eye = m_hEyeGlow;

	if ( eye )
	{
		if (m_eyeBrightness != 255)
			m_eyeBrightness = 255;

		eye->SetBrightness( m_eyeBrightness );
	}
}


void CBaseTurret::EyeOff( void )
{
	CSprite *eye = m_hEyeGlow;

	if ( eye && m_eyeBrightness > 0)
		{
			m_eyeBrightness = V_max( 0, m_eyeBrightness - 30 );
		eye->SetBrightness( m_eyeBrightness );
		}
	}


void CBaseTurret::ActiveThink(void)
{
	int fAttack = 0;
	Vector vecDirToEnemy;

	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance( );

	if ((!m_bOn) || (m_hEnemy == NULL))
	{
		m_hEnemy = NULL;
		m_flLastSight = gpGlobals->time + m_flMaxWait;
		SetThink(&CBaseTurret::SearchThink);
		return;
	}
	
	// if it's dead, look for something new
	if ( !m_hEnemy->IsAlive() || IRelationship( m_hEnemy ) <= R_NO )
	{
		if (!m_flLastSight)
		{
			m_flLastSight = gpGlobals->time + 0.5; // continue-shooting timeout
		}
		else
		{
			if (gpGlobals->time > m_flLastSight)
			{	
				m_hEnemy = NULL;
				m_flLastSight = gpGlobals->time + m_flMaxWait;
				SetThink(&CBaseTurret::SearchThink);
				return;
			}
		}
	}

	Vector vecMid = pev->origin + pev->view_ofs;
	Vector vecMidEnemy = m_hEnemy->BodyTarget( vecMid );

	// Look for our current enemy
	int fEnemyVisible = FBoxVisible(pev, m_hEnemy->pev, vecMidEnemy );	

	vecDirToEnemy = vecMidEnemy - vecMid;	// calculate dir and dist to enemy
	float flDistToEnemy = vecDirToEnemy.Length();

	Vector vec = UTIL_VecToAngles(vecMidEnemy - vecMid);	

	// Current enmey is not visible.
	if (!fEnemyVisible || (flDistToEnemy > m_iAttackRange))
	{
		if (!m_flLastSight)
			m_flLastSight = gpGlobals->time + 0.5;
		else
		{
			// Should we look for a new target?
			if (gpGlobals->time > m_flLastSight)
			{
				m_hEnemy = NULL;
				m_flLastSight = gpGlobals->time + m_flMaxWait;
				SetThink(&CBaseTurret::SearchThink);
				return;
			}
		}
		fEnemyVisible = 0;
	}
	else
	{
		m_vecLastSight = vecMidEnemy;
	}

	UTIL_MakeAimVectors(m_vecCurAngles);	

	/*
	ALERT( at_console, "%.0f %.0f : %.2f %.2f %.2f\n", 
		m_vecCurAngles.x, m_vecCurAngles.y,
		gpGlobals->v_forward.x, gpGlobals->v_forward.y, gpGlobals->v_forward.z );
	*/
	
	Vector vecLOS = vecDirToEnemy; //vecMid - m_vecLastSight;
	vecLOS = vecLOS.Normalize();

	// Is the Gun looking at the target
	if (DotProduct(vecLOS, gpGlobals->v_forward) <= 0.866) // 30 degree slop
		fAttack = FALSE;
	else
		fAttack = TRUE;

	// fire the gun
	if (m_iSpin && ((fAttack) || (m_fBeserk)))
	{
		Vector vecSrc, vecAng;
		GetAttachment( 0, vecSrc, vecAng );
		SetTurretAnim(TURRET_ANIM_FIRE);
		Shoot(vecSrc, gpGlobals->v_forward );
	} 
	else
	{
		SetTurretAnim(TURRET_ANIM_SPIN);
	}

	//move the gun
	if (m_fBeserk)
	{
		if (RANDOM_LONG(0,9) == 0)
		{
			m_vecGoalAngles.y = RANDOM_FLOAT(0,360);
			m_vecGoalAngles.x = RANDOM_FLOAT(0,90) - 90 * m_iOrientation;
			TakeDamage(pev,pev,1, DMG_GENERIC); // don't beserk forever
			return;
		}
	} 
	else if (fEnemyVisible)
	{
		if (vec.y > 360)
			vec.y -= 360;

		if (vec.y < 0)
			vec.y += 360;

		//ALERT(at_console, "[%.2f]", vec.x);
		
		if (vec.x < -180)
			vec.x += 360;

		if (vec.x > 180)
			vec.x -= 360;

		// now all numbers should be in [1...360]
		// pin to turret limitations to [-90...15]

		if (m_iOrientation == 0)
		{
			if (vec.x > 90)
				vec.x = 90;
			else if (vec.x < m_iMinPitch)
				vec.x = m_iMinPitch;
		}
		else
		{
			if (vec.x < -90)
				vec.x = -90;
			else if (vec.x > -m_iMinPitch)
				vec.x = -m_iMinPitch;
		}

		// ALERT(at_console, "->[%.2f]\n", vec.x);

		m_vecGoalAngles.y = vec.y;
		m_vecGoalAngles.x = vec.x;

	}

	SpinUpCall();
	MoveTurret();
}


void CTurret::Shoot(Vector &vecSrc, Vector &vecDirToEnemy)
{
	FireBullets( 1, vecSrc, vecDirToEnemy, TURRET_SPREAD, m_iAttackRange, BULLET_MONSTER_12MM, 1 );
	EMIT_SOUND( edict(), CHAN_BODY, "turret/tu_fire1.wav", 1.0f, 0.6f );
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 0.3f, this );
	pev->effects |= EF_MUZZLEFLASH;
}


void CMiniTurret::Shoot(Vector &vecSrc, Vector &vecDirToEnemy)
{
	FireBullets( 1, vecSrc, vecDirToEnemy, TURRET_SPREAD, m_iAttackRange, BULLET_MONSTER_9MM, 1 );
	EMIT_SOUND( edict(), CHAN_BODY, "weapons/hks_hl3.wav", 1, ATTN_NORM );
	pev->effects = pev->effects | EF_MUZZLEFLASH;
}


void CBaseTurret::Deploy(void)
{
	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance( );

	if (pev->sequence != TURRET_ANIM_DEPLOY)
	{
		m_bOn = 1;
		pev->flags &= ~FL_NOTARGET;
		SetTurretAnim(TURRET_ANIM_DEPLOY);
		EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_deploy.wav", TURRET_MACHINE_VOLUME, ATTN_NORM);
		SUB_UseTargets( this, USE_ON, 0 );
	}

	if (m_fSequenceFinished)
	{
		if ( !FClassnameIs( pev, "monster_sentry" ) )
		{
			pev->maxs.z = m_iDeployHeight;
			pev->mins.z = -m_iDeployHeight;
		}

		UTIL_SetSize(pev, pev->mins, pev->maxs);

		m_vecCurAngles.x = 0;

		if (m_iOrientation == 1)
		{
			m_vecCurAngles.y = UTIL_AngleMod( pev->angles.y + 180 );
		}
		else
		{
			m_vecCurAngles.y = UTIL_AngleMod( pev->angles.y );
		}

		SetTurretAnim(TURRET_ANIM_SPIN);
		pev->framerate = 0;
		SetThink(&CBaseTurret::SearchThink);
	}

	m_flLastSight = gpGlobals->time + m_flMaxWait;
}

void CBaseTurret::Retire(void)
{
	// make the turret level
	m_vecGoalAngles.x = 0;
	m_vecGoalAngles.y = m_flStartYaw;

	pev->nextthink = gpGlobals->time + 0.1;

	StudioFrameAdvance( );

	EyeOff( );

	if (!MoveTurret())
	{
		if (m_iSpin)
		{
			SpinDownCall();
		}
		else if (pev->sequence != TURRET_ANIM_RETIRE)
		{
			SetTurretAnim(TURRET_ANIM_RETIRE);
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "turret/tu_deploy.wav", TURRET_MACHINE_VOLUME, ATTN_NORM, 0, 120);
			SUB_UseTargets( this, USE_OFF, 0 );
		}
		else if (m_fSequenceFinished) 
		{	
			m_bOn = 0;
			m_flLastSight = 0;
			SetTurretAnim(TURRET_ANIM_NONE);

			if ( !FClassnameIs( pev, "monster_sentry" ) )
			{
				pev->maxs.z = m_iRetractHeight;
				pev->mins.z = -m_iRetractHeight;
			}

			UTIL_SetSize(pev, pev->mins, pev->maxs);
			if (m_bAutoStart)
			{
				SetThink(&CBaseTurret::AutoSearchThink);
				pev->nextthink = gpGlobals->time + .1;
			}
			else
			{
				SetThink(&CBaseTurret::SUB_DoNothing);
				pev->flags |= FL_NOTARGET;
			}
		}
	}
	else
	{
		SetTurretAnim(TURRET_ANIM_SPIN);
	}
}


void CTurret::SpinUpCall(void)
{
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	// Are we already spun up? If not start the two stage process.
	if (!m_iSpin)
	{
		SetTurretAnim( TURRET_ANIM_SPIN );
		// for the first pass, spin up the the barrel
		if (!m_iStartSpin)
		{
			pev->nextthink = gpGlobals->time + 1.0; // spinup delay
			EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_spinup.wav", TURRET_MACHINE_VOLUME, ATTN_NORM);
			m_iStartSpin = 1;
			pev->framerate = 0.1;
		}
		// after the barrel is spun up, turn on the hum
		else if (pev->framerate >= 1.0)
		{
			pev->nextthink = gpGlobals->time + 0.1; // retarget delay
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "turret/tu_active2.wav", TURRET_MACHINE_VOLUME, ATTN_NORM);
			SetThink(&CTurret::ActiveThink);
			m_iStartSpin = 0;
			m_iSpin = 1;
		} 
		else
		{
			pev->framerate += 0.075;
		}
	}

	if (m_iSpin)
	{
		SetThink(&CTurret::ActiveThink);
	}
}


void CTurret::SpinDownCall(void)
{
	if (m_iSpin)
	{
		SetTurretAnim( TURRET_ANIM_SPIN );
		if (pev->framerate == 1.0)
		{
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "turret/tu_active2.wav", 0, 0, SND_STOP, 100);
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "turret/tu_spindown.wav", TURRET_MACHINE_VOLUME, ATTN_NORM);
		}
		pev->framerate -= 0.02;
		if (pev->framerate <= 0)
		{
			pev->framerate = 0;
			m_iSpin = 0;
		}
	}
}


void CBaseTurret::SetTurretAnim(TURRET_ANIM anim)
{
	if (pev->sequence != anim)
	{
		switch(anim)
		{
		case TURRET_ANIM_FIRE:
		case TURRET_ANIM_SPIN:
			if (pev->sequence != TURRET_ANIM_FIRE && pev->sequence != TURRET_ANIM_SPIN)
			{
				pev->frame = 0;
			}
			break;
		default:
			pev->frame = 0;
			break;
		}

		pev->sequence = anim;
		ResetSequenceInfo( );

		switch(anim)
		{
		case TURRET_ANIM_RETIRE:
			pev->frame			= 255;
			pev->framerate		= -1.0;
			break;
		case TURRET_ANIM_DIE:
			pev->framerate		= 1.0;
			break;
		}
		//ALERT(at_console, "Turret anim #%d\n", anim);
	}
}


//
// This search function will sit with the turret deployed and look for a new target. 
// After a set amount of time, the barrel will spin down. After m_flMaxWait, the turret will
// retact.
//
void CBaseTurret::SearchThink(void)
{
	// ensure rethink
	SetTurretAnim(TURRET_ANIM_SPIN);
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_flSpinUpTime == 0 && m_flMaxSpin)
		m_flSpinUpTime = gpGlobals->time + m_flMaxSpin;

	Ping( );

	// If we have a target and we're still healthy
	if (m_hEnemy != NULL)
	{
		if ( !m_hEnemy->IsAlive() || IRelationship( m_hEnemy ) <= R_NO )
			m_hEnemy = NULL;// Dead enemy forces a search for new one
	}


	// Acquire Target
	if (m_hEnemy == NULL)
	{
		Look( m_iAttackRange );
		m_hEnemy = BestVisibleEnemy();
	}

	// If we've found a target, spin up the barrel and start to attack
	if (m_hEnemy != NULL)
	{
		m_flLastSight = 0;
		m_flSpinUpTime = 0;
		SetThink(&CBaseTurret::ActiveThink);
	}
	else
	{
		// Are we out of time, do we need to retract?
 		if (gpGlobals->time > m_flLastSight)
		{
			//Before we retrace, make sure that we are spun down.
			m_flLastSight = 0;
			m_flSpinUpTime = 0;
			SetThink(&CBaseTurret::Retire);
		}
		// should we stop the spin?
		else if ((m_flSpinUpTime) && (gpGlobals->time > m_flSpinUpTime))
		{
			SpinDownCall();
		}
		
		// generic hunt for new victims
		m_vecGoalAngles.y = (m_vecGoalAngles.y + 0.1 * m_fTurnRate);
		if (m_vecGoalAngles.y >= 360)
			m_vecGoalAngles.y -= 360;
		MoveTurret();
	}
}


// 
// This think function will deploy the turret when something comes into range. This is for
// automatically activated turrets.
//
void CBaseTurret::AutoSearchThink(void)
{
	// ensure rethink
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.3;

	// If we have a target and we're still healthy

	if (m_hEnemy != NULL)
	{
		if ( !m_hEnemy->IsAlive() || IRelationship( m_hEnemy ) <= R_NO )
			m_hEnemy = NULL;// Dead enemy forces a search for new one
	}

	// Acquire Target

	if (m_hEnemy == NULL)
	{
		Look( m_iAttackRange );
		m_hEnemy = BestVisibleEnemy();
	}

	if (m_hEnemy != NULL)
	{
		SetThink(&CBaseTurret::Deploy);
		EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_alert.wav", TURRET_MACHINE_VOLUME, ATTN_NORM);
	}
}


void CBaseTurret ::	TurretDeath( void )
{
	BOOL iActive = FALSE;

	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->deadflag != DEAD_DEAD)
	{
		pev->deadflag = DEAD_DEAD;

		float flRndSound = RANDOM_FLOAT ( 0 , 1 );

		if ( flRndSound <= 0.33 )
			EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_die.wav", 1.0, ATTN_NORM);
		else if ( flRndSound <= 0.66 )
			EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_die2.wav", 1.0, ATTN_NORM);
		else 
			EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_die3.wav", 1.0, ATTN_NORM);

		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "turret/tu_active2.wav", 0, 0, SND_STOP, 100);

		if (m_iOrientation == 0)
			m_vecGoalAngles.x = -15;
		else
			m_vecGoalAngles.x = -90;

		SetTurretAnim(TURRET_ANIM_DIE); 

		EyeOn( );	
	}

	EyeOff( );

	if (pev->dmgtime + RANDOM_FLOAT( 0, 2 ) > gpGlobals->time)
	{
		// lots of smoke
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( RANDOM_FLOAT( pev->absmin.x, pev->absmax.x ) );
			WRITE_COORD( RANDOM_FLOAT( pev->absmin.y, pev->absmax.y ) );
			WRITE_COORD( pev->origin.z - m_iOrientation * 64 );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 25 ); // scale * 10
			WRITE_BYTE( 10 - m_iOrientation * 5); // framerate
		MESSAGE_END();
	}
	
	if (pev->dmgtime + RANDOM_FLOAT( 0, 5 ) > gpGlobals->time)
	{
		Vector vecSrc = Vector( RANDOM_FLOAT( pev->absmin.x, pev->absmax.x ), RANDOM_FLOAT( pev->absmin.y, pev->absmax.y ), 0 );
		if (m_iOrientation == 0)
			vecSrc = vecSrc + Vector( 0, 0, RANDOM_FLOAT( pev->origin.z, pev->absmax.z ) );
		else
			vecSrc = vecSrc + Vector( 0, 0, RANDOM_FLOAT( pev->absmin.z, pev->origin.z ) );

		UTIL_Sparks( vecSrc );
	}

	if (m_fSequenceFinished && !MoveTurret( ) && pev->dmgtime + 5 < gpGlobals->time)
	{
		pev->framerate = 0;
		SetThink( NULL );
	}
}



bool CanRepairTurret( CBaseEntity *entity );

void CBaseTurret :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	CBaseEntity *attacker = CBaseEntity::Instance( pevAttacker );
	bool bRepair = attacker && ( bitsDamageType & DMG_CLUB ) && CanRepairTurret( attacker ) && IRelationship( attacker, true ) <= 0;

	if ( !pev->takedamage )
	{
		// a destroyed turret comes back to life when it is repaired
		if ( bRepair && pev->max_health > pev->health )
		{
			EMIT_SOUND( edict(), CHAN_AUTO, "debris/metal6.wav", 1, 0.6f );
			TakeHealth( fabs( flDamage ), bitsDamageType );

			if ( pev->health < 1 )
				pev->health = flDamage;

			Forget( bits_MEMORY_KILLED );

			int iHealth = (int) pev->health;
			int iMaxHealth = (int) pev->max_health;

			Spawn();

			pev->deadflag = DEAD_NO;
			pev->health = iHealth;
			pev->max_health = iMaxHealth;
			pev->nextthink = gpGlobals->time;
			m_bAutoStart = TRUE;
		}

		return;
	}

	if ( !bRepair && ptr->iHitgroup == 10 )
	{
		// hit armor
		if ( pev->dmgtime != gpGlobals->time || (RANDOM_LONG(0,10) < 1) )
		{
			UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT( 1, 2) );
			pev->dmgtime = gpGlobals->time;
		}

		flDamage = 0.1;// don't hurt the monster much, but allow bits_COND_LIGHT_DAMAGE to be generated
	}

	AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
}

// take damage. bitsDamageType indicates type of damage sustained, ie: DMG_BULLET

bool CanRepairTurret( CBaseEntity *entity )
{
	if ( !entity || !entity->IsPlayer() )
		return false;

	CBasePlayerItem *item = ( (CBasePlayer *) entity )->m_pActiveItem;

	if ( !item )
		return false;

	ItemInfo info;

	memset( &info, 0, sizeof( info ) );
	item->GetItemInfo( &info );

	if ( info.iId != WEAPON_PIPEWRENCH )
		return false;

	CWrench *wrench = dynamic_cast<CWrench *>( item->GetWeaponPtr() );

	return wrench && wrench->m_iChargeBonus > 0;
}

int CBaseTurret::TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType)
{
	if ( !pevAttacker )
		return 0;

	CBaseEntity *attacker = CBaseEntity::Instance( pevAttacker );

	if ( attacker && ( bitsDamageType & DMG_CLUB ) && CanRepairTurret( attacker ) && IRelationship( attacker, true ) <= 0 )
	{
		if ( pev->health < pev->max_health )
		{
			EMIT_SOUND( edict(), CHAN_AUTO, "debris/metal6.wav", 1, 0.6f );
			TakeHealth( fabs( flDamage ), bitsDamageType );
			GetDamagePoints( pevAttacker, pevInflictor, -flDamage );

			if ( pev->health < 1 )
				pev->health = 1;

			pev->takedamage = DAMAGE_AIM;
		}
	}
	else
	{
		if ( !pev->takedamage || CheckAttacker( attacker ) )
			return 0;

		if (!m_bOn)
			flDamage /= 10.0;

		pev->health -= flDamage;
		SetConditions( bits_COND_LIGHT_DAMAGE );
		FCheckAITrigger();
		GetDamagePoints( pevAttacker, pevInflictor, flDamage );
	}

	if (pev->health <= 0)
	{
		pev->health = 0;
		pev->takedamage = DAMAGE_NO;
		pev->dmgtime = gpGlobals->time;

		ClearBits (pev->flags, FL_MONSTER); // why are they set in the first place???

		SetUse(NULL);
		SetThink(&CBaseTurret::CallTurretDeath);
		SUB_UseTargets( this, USE_ON, 0 ); // wake up others
		pev->nextthink = gpGlobals->time + 0.1;

		return 0;
	}

	if (pev->health > 10)
	{
		m_fBeserk = 0;
	}
	else
	{
		if (m_bOn && (1 || RANDOM_LONG(0, 0x7FFF) > 800))
		{
			m_fBeserk = 1;
			SetThink(&CBaseTurret::SearchThink);
		}
	}

	return 1;
}

int CBaseTurret::MoveTurret(void)
{
	int state = 0;
	// any x movement?
	
	if (m_vecCurAngles.x != m_vecGoalAngles.x)
	{
		float flDir = m_vecGoalAngles.x > m_vecCurAngles.x ? 1 : -1 ;

		m_vecCurAngles.x += 0.1 * m_fTurnRate * flDir;

		// if we started below the goal, and now we're past, peg to goal
		if (flDir == 1)
		{
			if (m_vecCurAngles.x > m_vecGoalAngles.x)
				m_vecCurAngles.x = m_vecGoalAngles.x;
		} 
		else
		{
			if (m_vecCurAngles.x < m_vecGoalAngles.x)
				m_vecCurAngles.x = m_vecGoalAngles.x;
		}

		if (m_iOrientation == 0)
			SetBoneController(1, -m_vecCurAngles.x);
		else
			SetBoneController(1, m_vecCurAngles.x);
		state = 1;
	}

	if (m_vecCurAngles.y != m_vecGoalAngles.y)
	{
		float flDir = m_vecGoalAngles.y > m_vecCurAngles.y ? 1 : -1 ;
		float flDist = fabs(m_vecGoalAngles.y - m_vecCurAngles.y);
		
		if (flDist > 180)
		{
			flDist = 360 - flDist;
			flDir = -flDir;
		}
		if (flDist > 30)
		{
			if (m_fTurnRate < m_iBaseTurnRate * 10)
			{
				m_fTurnRate += m_iBaseTurnRate;
			}
		}
		else if (m_fTurnRate > 45)
		{
			m_fTurnRate -= m_iBaseTurnRate;
		}
		else
		{
			m_fTurnRate += m_iBaseTurnRate;
		}

		m_vecCurAngles.y += 0.1 * m_fTurnRate * flDir;

		if (m_vecCurAngles.y < 0)
			m_vecCurAngles.y += 360;
		else if (m_vecCurAngles.y >= 360)
			m_vecCurAngles.y -= 360;

		if (flDist < (0.05 * m_iBaseTurnRate))
			m_vecCurAngles.y = m_vecGoalAngles.y;

		//ALERT(at_console, "%.2f -> %.2f\n", m_vecCurAngles.y, y);
		if (m_iOrientation == 0)
			SetBoneController(0, m_vecCurAngles.y - pev->angles.y );
		else 
			SetBoneController(0, pev->angles.y - 180 - m_vecCurAngles.y );
		state = 1;
	}

	if (!state)
		m_fTurnRate = m_iBaseTurnRate;

	//ALERT(at_console, "(%.2f, %.2f)->(%.2f, %.2f)\n", m_vecCurAngles.x, 
	//	m_vecCurAngles.y, m_vecGoalAngles.x, m_vecGoalAngles.y);
	return state;
}

//
// ID as a machine
//
int	CBaseTurret::Classify ( void )
{
	return GetClassification( CLASS_MACHINE );
}

void CBaseTurret::BecomeDead( float flSavedHealth )
{
	CBaseMonster::BecomeDead( flSavedHealth );
	pev->health = 0.0f;
}

float CTurret::GetPointsForDamage( float flDamage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	return ( pev->max_health / sk_turret_health.value ) * 5.0f * ( flDamage / pev->max_health );
}

float CMiniTurret::GetPointsForDamage( float flDamage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	return ( pev->max_health / sk_miniturret_health.value ) * 3.0f * ( flDamage / pev->max_health );
}

//=========================================================
// Sentry gun - smallest turret, placed near grunt entrenchments
//=========================================================
class CSentry : public CBaseTurret
{
public:
	void EXPORT Deploy( void );
	int BloodColor( void ) { return DONT_BLEED; }
	void Spawn( );
	void Precache(void);
	void CreateExplosion( void );
	void Killed( entvars_t *pevAttacker, int iGib );
	void GibMonster( void );
	void TurretDeath( void );
	float GetPointsForDamage( float flDamage );
	// other functions
	void Shoot(Vector &vecSrc, Vector &vecDirToEnemy);
	int TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType);
	void EXPORT SentryTouch( CBaseEntity *pOther );
	void EXPORT SentryDeath( void );

private:
	int m_iDeathAttempts;
	int m_iChromeGibModel;
	int m_iComputerGibModel;
};

LINK_ENTITY_TO_CLASS( monster_sentry, CSentry );

void CSentry::Deploy( void )
{
	pev->movetype = MOVETYPE_STEP;
	pev->nextthink = gpGlobals->time + 0.1f;
	StudioFrameAdvance();

	if ( pev->sequence != TURRET_ANIM_DEPLOY )
	{
		m_bOn = 1;
		pev->flags &= ~FL_NOTARGET;
		SetTurretAnim( TURRET_ANIM_DEPLOY );
		EMIT_SOUND( edict(), CHAN_BODY, "turret/tu_deploy.wav", TURRET_MACHINE_VOLUME, ATTN_NORM );
		SUB_UseTargets( this, USE_ON, 0 );
	}

	if ( m_fSequenceFinished )
	{
		m_vecCurAngles.x = 0;
		m_vecCurAngles.y = UTIL_AngleMod( m_iOrientation == 1 ? pev->angles.y + 180 : pev->angles.y );

		SetTurretAnim( TURRET_ANIM_SPIN );
		pev->framerate = 0;
		SetThink( &CBaseTurret::SearchThink );
	}

	m_flLastSight = gpGlobals->time + m_flMaxWait;
}

float CSentry::GetPointsForDamage( float flDamage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	return ( pev->max_health / sk_sentry_health.value ) * 2.0f * ( flDamage / pev->max_health );
}

void CSentry::Precache()
{
	CBaseTurret::Precache( );

	if ( pev->model == 0 )
		UTIL_PrecacheModel( this, "models/sentry.mdl");

	m_iChromeGibModel = UTIL_PrecacheModel( this, "models/chromegibs.mdl" );
	m_iComputerGibModel = UTIL_PrecacheModel( this, "models/computergibs.mdl" );
	PrecacheSound_Intercept( this, "debris/beamstart14.wav" );
	PrecacheSound_Intercept( this, "weapons/hks_hl3.wav" );
}

void CSentry::Spawn()
{
	Precache( );

	if ( !SetupModel() )
		SetModel_Intercept( edict(), "models/sentry.mdl");

	if ( pev->health == 0.0f )
		pev->health			= sk_sentry_health.value;
	m_HackedGunPos		= Vector( 0, 0, 48 );
	pev->view_ofs.z		= 48;
	CBaseTurret::Spawn();
	m_flMaxWait = 1000000.0f;
	m_flMaxSpin = 1000000.0f;
	m_iRetractHeight = 0;
	m_iDeployHeight = 0;
	m_iMinPitch	= -60;
	m_iDeathAttempts = 0;
	pev->movetype = MOVETYPE_TOSS;
	UTIL_SetSize(pev, Vector(-12, -12, 0 ), Vector( 12, 12, 56 ));

	SetTouch(&CSentry::SentryTouch);
	Initialize();

	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( "Sentry Turret" );
}

void CSentry::Shoot(Vector &vecSrc, Vector &vecDirToEnemy)
{
	FireBullets( 1, vecSrc, vecDirToEnemy, TURRET_SPREAD, m_iAttackRange, BULLET_MONSTER_MP5, 1 );
	EMIT_SOUND( edict(), CHAN_AUTO, "weapons/hks_hl3.wav", 1.0f, ATTN_NORM);
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 0.3f, this );
	pev->effects |= EF_MUZZLEFLASH;
}

int CSentry::TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType)
{
	int result = CBaseTurret::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );

	if ( pev->health > 0 && !m_bOn
		&& m_pfnThink != static_cast<void ( CBaseEntity::* )( void )>( &CSentry::Deploy ) )
	{
		SetThink( &CSentry::Deploy );
		pev->nextthink = gpGlobals->time + 0.1;
	}

	return result;
}


void CSentry::SentryTouch( CBaseEntity *pOther )
{
	if ( pOther && (pOther->IsPlayer() || (pOther->pev->flags & FL_MONSTER)) )
	{
		TakeDamage(pOther->pev, pOther->pev, 0, 0 );
		pOther->TakeDamage( pOther->pev, pOther->pev, 0, 0 );
	}
}


void CSentry::TurretDeath( void )
{
	SentryDeath();
}

void CSentry ::	SentryDeath( void )
{
	FCheckAITrigger();
	Remember( bits_MEMORY_KILLED );
	pev->framerate = 1.0f;

	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	if ( m_iDeathAttempts < 4 )
	{
		Vector position( 0, 0, 0 );
		Vector angles( 0, 0, 0 );
		TraceResult trace;

		memset( &trace, 0, sizeof( trace ) );
		GetAttachment( 0, position, angles );
		UTIL_TraceLine( pev->origin, position, ignore_monsters, edict(), &trace );

		if ( trace.flFraction != 1.0f || trace.fStartSolid )
			pev->angles.y += 90.0f;

		++m_iDeathAttempts;
	}

	if ( pev->deadflag != DEAD_DEAD)
	{
		pev->deadflag = DEAD_DEAD;

		double flRndSound = RANDOM_FLOAT ( 0 , 1 );
		const char *sound;

		if ( flRndSound <= 0.33 )
			sound = "turret/tu_die.wav";
		else if ( flRndSound <= 0.66 )
			sound = "turret/tu_die2.wav";
		else
			sound = "turret/tu_die3.wav";

		EMIT_SOUND( edict(), CHAN_BODY, sound, VOL_NORM, ATTN_NORM);
		STOP_SOUND( edict(), CHAN_VOICE, "turret/tu_active2.wav" );

		SetBoneController( 0, 0 );
		SetBoneController( 1, 0 );

		SetTurretAnim(TURRET_ANIM_DIE);
		UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 16 ) );
		pev->angles.y = UTIL_AngleMod( pev->angles.y + RANDOM_LONG( 0, 2 ) * 120 );

		EyeOn( );
	}

	RemoveEffects( );

	Vector position( 0, 0, 0 );
	Vector angles( 0, 0, 0 );
	GetAttachment( 1, position, angles );

	float smokeTime = pev->dmgtime + RANDOM_FLOAT( 0, 2 );

	if ( smokeTime > gpGlobals->time)
	{
		// lots of smoke
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( position.x + RANDOM_FLOAT( -16, 16 ) );
			WRITE_COORD( position.y + RANDOM_FLOAT( -16, 16 ) );
			WRITE_COORD( position.z - 32 );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 15 ); // scale * 10
			WRITE_BYTE( 8 ); // framerate
		MESSAGE_END();
	}

	float sparkTime = pev->dmgtime + RANDOM_FLOAT( 0, 8 );

	if ( sparkTime > gpGlobals->time)
		UTIL_Sparks( position );

	float deathTime = pev->dmgtime + 5.0f;

	if (m_fSequenceFinished && gpGlobals->time > deathTime )
	{
		m_bOn = false;
		pev->framerate = 0;
		SetThink( NULL );
		m_flShockGlowTime = 0;

		if ( pev->deadflag != DEAD_DEAD )
			pev->deadflag = DEAD_DEAD;

		CreateExplosion();
	}
}

void CSentry::Killed( entvars_t *, int )
{
	SentryDeath();
	m_bOn = false;
	pev->framerate = 0;
	SetThink( NULL );
	m_flShockGlowTime = 0;

	if ( pev->deadflag != DEAD_DEAD )
		pev->deadflag = DEAD_DEAD;

	CreateExplosion();
}

void CSentry::GibMonster( void )
{
	SentryDeath();
	m_bOn = false;
	pev->framerate = 0;
	SetThink( NULL );
	m_flShockGlowTime = 0;

	if ( pev->deadflag != DEAD_DEAD )
		pev->deadflag = DEAD_DEAD;

	CreateExplosion();
}

void StartSentryExplosion( Vector origin, float unused, float delay, int amount )
{
	CBaseEntity *explosion = CBaseEntity::Create( "env_explosion", origin, g_vecZero, false, NULL );
	char magnitude[128];
	KeyValueData data;

	sprintf( magnitude, "%3d", amount );
	data.szKeyName = "iMagnitude";
	data.szValue = magnitude;
	explosion->KeyValue( &data );
	explosion->Spawn();
	explosion->SetThink( &CBaseEntity::SUB_CallUseToggle );
	explosion->pev->nextthink = gpGlobals->time + delay;
}

void CSentry::CreateExplosion( void )
{
	Vector position( 0, 0, 0 );
	Vector angles( 0, 0, 0 );

	GetAttachment( 0, position, angles );
	pev->solid = SOLID_NOT;

	StartSentryExplosion( position + Vector( 0, 0, 24 ), 0, 0, 35 );

	if ( pev->deadflag != DEAD_DEAD )
		pev->deadflag = DEAD_DEAD;

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, position );
		WRITE_BYTE( TE_BREAKMODEL );
		WRITE_COORD( position.x );
		WRITE_COORD( position.y );
		WRITE_COORD( position.z );
		WRITE_COORD( 200 );
		WRITE_COORD( 200 );
		WRITE_COORD( 64 );
		WRITE_COORD( 10 );
		WRITE_COORD( 20 );
		WRITE_COORD( 80 );
		WRITE_BYTE( 30 );
		WRITE_SHORT( m_iComputerGibModel );
		WRITE_BYTE( 15 );
		WRITE_BYTE( 100 );
		WRITE_BYTE( BREAK_METAL );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_BREAKMODEL );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( 200 );
		WRITE_COORD( 200 );
		WRITE_COORD( 96 );
		WRITE_COORD( 0 );
		WRITE_COORD( 0 );
		WRITE_COORD( 10 );
		WRITE_BYTE( 30 );
		WRITE_SHORT( m_iChromeGibModel );
		WRITE_BYTE( 15 );
		WRITE_BYTE( 100 );
		WRITE_BYTE( BREAK_METAL );
	MESSAGE_END();

	EMIT_SOUND( edict(), CHAN_BODY, "debris/beamstart14.wav", VOL_NORM, ATTN_NORM );
	SetThink( &CSentry::SUB_Remove );
	pev->nextthink = gpGlobals->time;

	CBaseEntity *smoker = CBaseEntity::Create( "env_smoker", pev->origin, g_vecZero, false, NULL );

	smoker->pev->health = 1;
	smoker->pev->scale = 10;
	smoker->pev->dmg = 0;
	smoker->pev->nextthink = gpGlobals->time + 0.5f;
}

