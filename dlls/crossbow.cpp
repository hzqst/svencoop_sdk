/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"
#include "CClassicMode.h"
#include "game.h"
#include "playerammo.h"
#include "server_sound.h"
#include "weaponinfo.h"
#include "event_flags.h"

#define WEAPONCUSTOM_SND_XBOW_FIRE1 5
#define WEAPONCUSTOM_SND_XBOW_RELOAD1 6

#undef PRECACHE_MODEL
#define PRECACHE_MODEL( path ) UTIL_PrecacheModel( this, path )
#undef PRECACHE_SOUND
#define PRECACHE_SOUND( path ) PrecacheSound_Intercept( this, path )

#define BOLT_AIR_VELOCITY 2000
#define BOLT_WATER_VELOCITY 1000

class CCrossbowBolt : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_NONE; }
	void EXPORT BubbleThink( void );
	void EXPORT BoltTouch( CBaseEntity *other );
	void EXPORT ExplodeThink( void );
	static CCrossbowBolt *BoltCreate( edict_t *owner = NULL );

	int		m_iTrail;
	bool	m_fExplosive;
	EHANDLE	m_hOwner;
};

LINK_ENTITY_TO_CLASS( crossbow_bolt, CCrossbowBolt );

CCrossbowBolt *CCrossbowBolt::BoltCreate( edict_t *owner )
{
	// Create a new entity with CCrossbowBolt private data
	CCrossbowBolt *pBolt = GetClassPtr( (CCrossbowBolt *)NULL );

	if ( owner )
		pBolt->pev->owner = owner;

	pBolt->m_hOwner = NULL;
	pBolt->pev->classname = MAKE_STRING("bolt");
	pBolt->Spawn();

	return pBolt;
}

void CCrossbowBolt::Spawn( void )
{
	Precache( );
	pev->movetype = MOVETYPE_FLYMISSILE;
	pev->solid = SOLID_BBOX;

	pev->gravity = 0.5;
	SetModel_Intercept( edict(), "models/crossbow_bolt.mdl");

	UTIL_SetOrigin( pev, pev->origin );

	SetTouch( &CCrossbowBolt::BoltTouch );
	SetThink( &CCrossbowBolt::BubbleThink );
	pev->nextthink = gpGlobals->time + 0.2;
}

void CCrossbowBolt::Precache( void )
{
	PRECACHE_MODEL ("models/crossbow_bolt.mdl");
	PRECACHE_SOUND("weapons/xbow_hitbod1.wav");
	PRECACHE_SOUND("weapons/xbow_hitbod2.wav");
	PRECACHE_SOUND("weapons/xbow_fly1.wav");
	PRECACHE_SOUND("weapons/xbow_hit1.wav");
	PRECACHE_SOUND("fvox/beep.wav");
	m_iTrail = PRECACHE_MODEL("sprites/streak.spr");
}

void CCrossbowBolt::BubbleThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->waterlevel != 0 )
		UTIL_BubbleTrail( pev->origin - pev->velocity * 0.1, pev->origin, 1 );
}

void CCrossbowBolt::BoltTouch( CBaseEntity *pOther )
{
	SetTouch( NULL );

	if ( pOther && pOther->pev->takedamage >= DAMAGE_YES )
	{
		ClearMultiDamage();

		float damage;

		if ( pev->dmg > 0 )
			damage = pev->dmg;
		else
			damage = sk_plr_xbow_bolt_monster.value;

		TraceResult tr = UTIL_GetGlobalTrace( );
		CBaseEntity *owner = m_hOwner;
		entvars_t	*pevOwner = owner ? owner->pev : pev->owner ? VARS( pev->owner )
		                                                      : NULL;

		pOther->TraceAttack(pevOwner, damage, pev->velocity.Normalize(), &tr, DMG_BULLET | DMG_NEVERGIB | DMG_POISON );
		ApplyMultiDamage( pev, pevOwner );
		// play body "thwack" sound
		switch( RANDOM_LONG(0,1) )
		{
		case 0:
				EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM,
					0, PITCH_NORM, 0, false, g_vecZero ); break;
		case 1:
				EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM,
					0, PITCH_NORM, 0, false, g_vecZero ); break;
		}
	}
	else
	{
		int pitch = 98 + RANDOM_LONG( 0, 7 );

		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "weapons/xbow_hit1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, pitch, 0, false, g_vecZero );

		if (UTIL_PointContents(pev->origin) != CONTENTS_WATER)
			UTIL_Sparks( pev->origin );
		}

	if ( m_fExplosive )
	{
		UTIL_SetOrigin( pev, pev->origin - pev->velocity.Normalize() * 12 );
		SetThink( &CCrossbowBolt::ExplodeThink );
	}
	else
	{
		SetThink( &CCrossbowBolt::SUB_Remove );
	}

	pev->nextthink = gpGlobals->time;

	if ( !pOther || pOther->pev->movetype != MOVETYPE_PUSHSTEP )
		pev->velocity = g_vecZero;
}

void CCrossbowBolt::ExplodeThink( void )
{
	int iContents = UTIL_PointContents ( pev->origin );

	pev->dmg = 40;

	( *g_engfuncs.pfnMessageBegin )( MSG_PVS, SVC_TEMPENTITY, pev->origin, NULL );
		WRITE_BYTE( TE_EXPLOSION);
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
	WRITE_SHORT( iContents == CONTENTS_WATER ? g_sModelIndexWExplosion : g_sModelIndexFireball );
	WRITE_BYTE( 10 ); // scale * 10
		WRITE_BYTE( 15  ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NOSOUND );
	MESSAGE_END();

	const char *sample = NULL;

	switch ( RANDOM_LONG( 0, 2 ) )
	{
		case 0:
			sample = "weapons/explode3.wav";
			break;
		case 1:
			sample = "weapons/explode4.wav";
			break;
		case 2:
			sample = "weapons/explode5.wav";
			break;
	}

	if ( sample )
		CServer_SoundEngine::GetSoundEngine()->PlaySound( NULL, CHAN_AUTO, (char *) sample,
		                                                  1.0f, 0.3f, 0, PITCH_NORM, 0, true, pev->origin, 0 );

	CBaseEntity *owner = m_hOwner;
	entvars_t *pevOwner = owner ? owner->pev : pev->owner ? VARS( pev->owner )
	                                                      : NULL; // can't traceline attack owner if this is set

	::RadiusDamage( pev->origin, pev, pevOwner, pev->dmg, 128, CLASS_NONE, DMG_BLAST | DMG_ALWAYSGIB );

	UTIL_Remove(this);
}

enum crossbow_e
{
	CROSSBOW_IDLE1 = 0, // full
	CROSSBOW_IDLE2,     // empty
	CROSSBOW_FIDGET1,   // full
	CROSSBOW_FIDGET2,   // empty
	CROSSBOW_FIRE1,     // full
	CROSSBOW_FIRE2,     // reload
	CROSSBOW_FIRE3,     // empty
	CROSSBOW_RELOAD,    // from empty
	CROSSBOW_DRAW1,     // full
	CROSSBOW_DRAW2,     // empty
	CROSSBOW_HOLSTER1,  // full
	CROSSBOW_HOLSTER2,  // empty
};

LINK_ENTITY_TO_CLASS( weapon_crossbow, CCrossbow );

void CCrossbow::Spawn( void )
{
	m_iId = WEAPON_CROSSBOW;

	Precache( );

	if ( CClassicMode::GetInstance()->IsEnabled() )
		SetModel_Intercept( edict(), GetW_Model( "models/hlclassic/w_crossbow.mdl" ) );
	else
		SetModel_Intercept( edict(), GetW_Model( "models/w_crossbow.mdl" ) );

	m_iDefaultAmmo = CROSSBOW_DEFAULT_GIVE;
	m_flCustomDmg = pev->dmg;

	CBasePhysicsObject::Spawn();// get ready to fall down.
}

void CCrossbow::Precache( void )
{
	PrecacheCustomModels();

	bool bClassic = CClassicMode::GetInstance()->IsEnabled();

	PRECACHE_MODEL( bClassic ? "models/hlclassic/w_crossbow.mdl" : "models/w_crossbow.mdl");
	PRECACHE_MODEL( bClassic ? "models/hlclassic/v_crossbow.mdl" : "models/v_crossbow.mdl");
	PRECACHE_MODEL( bClassic ? "models/hlclassic/p_crossbow.mdl" : "models/p_crossbow.mdl");

	PRECACHE_SOUND("weapons/xbow_fire1.wav");
	PRECACHE_SOUND("weapons/xbow_reload1.wav");

	UTIL_PrecacheOther( "crossbow_bolt" );
}

bool CCrossbow::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "bolts";
	p->iMaxAmmo1 = BOLT_MAX_CARRY;
	p->pszAmmo1Entity = "ammo_crossbow";
	p->iAmmo1Drop = AMMO_CROSSBOWCLIP_GIVE;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->pszAmmo2Entity = NULL;
	p->iAmmo2Drop = AMMO_DROP_NONE;
	p->iMaxClip = CROSSBOW_MAX_CLIP;
	p->iSlot = 2;
	p->iPosition = 2;
	p->iId = WEAPON_CROSSBOW;
	p->iFlags = 0;
	p->iWeight = CROSSBOW_WEIGHT;
	return true;
}

bool CCrossbow::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		AddToPlayerMessage();
		return TRUE;
	}
	return FALSE;
}

bool CCrossbow::Deploy( void )
{
	int iAnim;

	if ( m_iClip )
		iAnim = CROSSBOW_DRAW1;
	else
		iAnim = CROSSBOW_DRAW2;

	if ( CClassicMode::GetInstance()->IsEnabled() )
		return DefaultDeploy( GetV_Model( "models/hlclassic/v_crossbow.mdl" ),
			GetP_Model( "models/hlclassic/p_crossbow.mdl" ), iAnim, "bow" );
	return DefaultDeploy( GetV_Model( "models/v_crossbow.mdl" ),
		GetP_Model( "models/p_crossbow.mdl" ), iAnim, "bow" );
}

void CCrossbow::Holster( int skiplocal /* = 0 */ )
{
	m_fInReload = FALSE;// cancel any reload in progress.

	if ( m_fInZoom )
		SecondaryAttack( );

	m_hPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	m_hPlayer->pev->viewmodel = 0;
}

void CCrossbow::PrimaryAttack( void )
{
	if ( m_fInZoom && IsMultiplayer() )
		FireSniperBolt();
	else
		FireBolt();
}

// zoomed crossbow in multiplayer
void CCrossbow::FireSniperBolt( void )
{
	if (m_iClip == 0)
	{
		PlayEmptySound( );
		return;
	}

	m_hPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	m_iClip--;

	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), g_evWeaponCustom, 0.0, (float *)&g_vecZero, (float *)&g_vecZero,
	                     WEAPONCUSTOM_SND_XBOW_FIRE1, RANDOM_FLOAT( 0.95, 1.0 ), WEAPONCUSTOM_SOUND, CHAN_WEAPON, 0, 0 );

	if ( m_iClip )
	{
		PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), g_evWeaponCustom, 0.0, (float *) &g_vecZero, (float *) &g_vecZero,
		                     WEAPONCUSTOM_SND_XBOW_RELOAD1, RANDOM_FLOAT( 0.95, 1.0 ), WEAPONCUSTOM_SOUND, CHAN_ITEM, 0, 0 );

		SendWeaponAnim( CROSSBOW_FIRE1 );
	}
	else if ( m_hPlayer->m_rgAmmo[m_iPrimaryAmmoType] == 0 )
	{
		SendWeaponAnim( CROSSBOW_FIRE3 );
	}

	// player "shoot" animation
	m_hPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector anglesAim = m_hPlayer->pev->v_angle + m_hPlayer->pev->punchangle;
	UTIL_MakeVectors( anglesAim );
	anglesAim.x = -anglesAim.x;

	Vector vecSrc = m_hPlayer->GetGunPosition( ) - gpGlobals->v_up * 2;
	Vector vecDir = gpGlobals->v_forward;
	CCrossbowBolt *bolt = CCrossbowBolt::BoltCreate( m_hPlayer->edict() );

	bolt->pev->origin = vecSrc;
	bolt->pev->angles = anglesAim;
	bolt->m_hOwner = m_hPlayer;
	bolt->pev->owner = m_hPlayer->edict();
	bolt->m_fExplosive = false;

	float speed;

	if ( m_hPlayer->pev->waterlevel == 3 )
		speed = BOLT_WATER_VELOCITY;
	else
		speed = BOLT_AIR_VELOCITY;

	bolt->pev->velocity = vecDir * speed;
	bolt->pev->speed = speed;
	bolt->pev->avelocity.z = 10;

	if ( m_flCustomDmg > 0 )
		bolt->pev->dmg = m_flCustomDmg;

	if ( !m_iClip && m_hPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
	{
		// HEV suit - indicate out of ammo condition
		m_hPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );
	}

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.55;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.55;

	if ( m_iClip != 0 )
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5.0;
	else
		m_flTimeWeaponIdle = 0.75;

	m_hPlayer->pev->punchangle.x -= 2;
}

void CCrossbow::FireBolt( void )
{
	if (m_iClip == 0)
	{
		PlayEmptySound( );
		return;
	}

	m_hPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

	m_iClip--;

	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), g_evWeaponCustom, 0.0, (float *)&g_vecZero, (float *)&g_vecZero,
	                     WEAPONCUSTOM_SND_XBOW_FIRE1, RANDOM_FLOAT( 0.95, 1.0 ), WEAPONCUSTOM_SOUND, CHAN_WEAPON, 0, 0 );

	if ( m_iClip )
	{
		PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), g_evWeaponCustom, 0.0, (float *) &g_vecZero, (float *) &g_vecZero,
		                     WEAPONCUSTOM_SND_XBOW_RELOAD1, RANDOM_FLOAT( 0.95, 1.0 ), WEAPONCUSTOM_SOUND, CHAN_ITEM, 0, 0 );

		SendWeaponAnim( CROSSBOW_FIRE1 );
	}
	else
	{
		SendWeaponAnim( CROSSBOW_FIRE3 );
	}

	// player "shoot" animation
	m_hPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector anglesAim = m_hPlayer->pev->v_angle + m_hPlayer->pev->punchangle;
	UTIL_MakeVectors( anglesAim );

	anglesAim.x		= -anglesAim.x;
	Vector vecSrc	 = m_hPlayer->GetGunPosition( ) - gpGlobals->v_up * 2;
	Vector vecDir	 = gpGlobals->v_forward;
	CCrossbowBolt *pBolt = CCrossbowBolt::BoltCreate( m_hPlayer->edict() );
	pBolt->pev->origin = vecSrc;
	pBolt->pev->angles = anglesAim;
	pBolt->m_hOwner = m_hPlayer;
	pBolt->pev->owner = m_hPlayer->edict();
	pBolt->m_fExplosive = CVAR_GET_FLOAT( "weaponmode_crossbow" ) != 1.0f;

	float speed;

	if ( m_hPlayer->pev->waterlevel == 3 )
		speed = BOLT_WATER_VELOCITY;
	else
		speed = BOLT_AIR_VELOCITY;

	pBolt->pev->velocity = vecDir * speed;
		pBolt->pev->speed = speed;
	pBolt->pev->avelocity.z = 10;

	if ( m_flCustomDmg > 0 )
		pBolt->pev->dmg = m_flCustomDmg;

	if (!m_iClip && m_hPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	{
		// HEV suit - indicate out of ammo condition
		m_hPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
	}

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.55;

	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.55;

	if (m_iClip != 0)
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5.0;
	else
		m_flTimeWeaponIdle = 0.75;

	m_hPlayer->pev->punchangle.x -= 2;
}

void CCrossbow::SecondaryAttack( void )
{
	if ( m_fInZoom )
	{
		if ( FRunfuncs() )
			SetFOV( 0 );

		strncpy( m_hPlayer->m_szAnimExtention, "bow", sizeof( m_hPlayer->m_szAnimExtention ) ); // 0 means reset to default fov
		m_fInZoom = false;
	}
	else if ( !( m_hPlayer->m_afPhysicsFlags & PFLAG_CAMERA ) )
	{
		if ( FRunfuncs() )
			SetFOV( 20 );

		strncpy( m_hPlayer->m_szAnimExtention, "bowscope", sizeof( m_hPlayer->m_szAnimExtention ) );
		m_fInZoom = true;
	}

	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.6;
}

void CCrossbow::Reload( void )
{
	if ( m_fInZoom )
		SecondaryAttack();

	if ( DefaultReload( CROSSBOW_MAX_CLIP, CROSSBOW_RELOAD, 4.5 ) )
	{
		PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), g_evWeaponCustom, 0.0, (float *) &g_vecZero, (float *) &g_vecZero,
		                     WEAPONCUSTOM_SND_XBOW_RELOAD1, RANDOM_FLOAT(0.95, 1.0), WEAPONCUSTOM_SOUND, CHAN_ITEM, 0, 0 );

		CBasePlayerWeapon::Reload();
	}
}

void CCrossbow::WeaponIdle( void )
{
	m_hPlayer->GetAutoaimVector( AUTOAIM_2DEGREES );  // Update the autoaim crosshair in DM

	ResetEmptySound( );

	if ( !( UTIL_WeaponTimeBase() > m_flTimeWeaponIdle ) )
		return;

	float flRand = UTIL_SharedRandomFloat( m_hPlayer->random_seed, 0, 1 );
		if (flRand <= 0.75)
		{
		SendWeaponAnim( m_iClip ? CROSSBOW_IDLE1 : CROSSBOW_IDLE2 );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_hPlayer->random_seed, 10, 15 );
		}
		else if (m_iClip)
			{
				SendWeaponAnim( CROSSBOW_FIDGET1 );
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 90.0 / 30.0;
			}
			else
			{
				SendWeaponAnim( CROSSBOW_FIDGET2 );
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 80.0 / 30.0;
			}
		}

void CCrossbow::GetWeaponData( weapon_data_t *data )
{
	data->iuser1 = m_iPrimaryAmmoType;
	data->iuser2 = m_hPlayer->m_rgAmmo[m_iPrimaryAmmoType];
}

void CCrossbow::SetWeaponData( weapon_data_t *data )
{
	m_iPrimaryAmmoType = data->iuser1;
	m_hPlayer->m_rgAmmo[m_iPrimaryAmmoType] = data->iuser2;
}

class CCrossbowAmmo : public CPlayerAmmo
{
public:
	CCrossbowAmmo() : CPlayerAmmo( AMMO_CROSSBOWCLIP_GIVE, "bolts", BOLT_MAX_CARRY ) {}

	const char *GetDefaultModel( void ) { return CClassicMode::GetInstance()->IsEnabled() ? "models/hlclassic/w_crossbow_clip.mdl" : "models/w_crossbow_clip.mdl"; }
	const char *GetPickupSound( void ) { return CClassicMode::GetInstance()->IsEnabled() ? "hlclassic/items/9mmclip1.wav" : "items/9mmclip1.wav"; }
};

LINK_ENTITY_TO_CLASS( ammo_crossbow, CCrossbowAmmo );

#endif
