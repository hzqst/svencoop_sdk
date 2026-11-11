/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "weaponinfo.h"
#include "event_flags.h"

LINK_ENTITY_TO_CLASS( weapon_crossbow, CCrossbow );

void CCrossbow::Spawn( void )
{
	m_iId = WEAPON_CROSSBOW;

	Precache();

	if ( CClassicModeWrapper::isEnabled() )
		SET_MODEL( ENT( pev ), GetW_Model( "models/hlclassic/w_crossbow.mdl" ) );
	else
		SET_MODEL( ENT( pev ), GetW_Model( "models/w_crossbow.mdl" ) );

	m_iDefaultAmmo = CROSSBOW_DEFAULT_GIVE;
	m_flCustomDmg = pev->dmg;

	FallInit();// get ready to fall down.
}

void CCrossbow::Precache( void )
{
	PrecacheCustomModels();

	bool bClassic = CClassicModeWrapper::isEnabled();

	PRECACHE_MODEL( bClassic ? "models/hlclassic/w_crossbow.mdl" : "models/w_crossbow.mdl" );
	PRECACHE_MODEL( bClassic ? "models/hlclassic/v_crossbow.mdl" : "models/v_crossbow.mdl" );
	PRECACHE_MODEL( bClassic ? "models/hlclassic/p_crossbow.mdl" : "models/p_crossbow.mdl" );

	PRECACHE_SOUND( "weapons/xbow_fire1.wav" );
	PRECACHE_SOUND( "weapons/xbow_reload1.wav" );
}

bool CCrossbow::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "bolts";
	p->iMaxAmmo1 = BOLT_MAX_CARRY;
	p->pszAmmo1Entity = "ammo_crossbow";
	p->iAmmo1Drop = CROSSBOW_AMMO1_DROP;
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
		return true;
	}

	return false;
}

bool CCrossbow::Deploy( void )
{
	int iAnim;

	if ( m_iClip )
		iAnim = CROSSBOW_DRAW1;
	else
		iAnim = CROSSBOW_DRAW2;

	if ( CClassicModeWrapper::isEnabled() )
		return DefaultDeploy( GetV_Model( "models/hlclassic/v_crossbow.mdl" ), GetP_Model( "models/hlclassic/p_crossbow.mdl" ), iAnim, "bow" );

	return DefaultDeploy( GetV_Model( "models/v_crossbow.mdl" ), GetP_Model( "models/p_crossbow.mdl" ), iAnim, "bow" );
}

void CCrossbow::Holster( int skiplocal /* = 0 */ )
{
	m_fInReload = false;// cancel any reload in progress.

	if ( m_fInZoom )
		SecondaryAttack();

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
	if ( m_iClip == 0 )
	{
		PlayEmptySound();
		return;
	}

	m_hPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

	m_iClip--;

	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), g_evWeaponCustom, 0.0, (float *)&g_vecZero, (float *)&g_vecZero,
		WEAPONCUSTOM_SND_XBOW_FIRE1, RANDOM_FLOAT( 0.95, 1.0 ), WEAPONCUSTOM_SOUND, CHAN_WEAPON, 0, 0 );

	if ( m_iClip )
	{
		PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), g_evWeaponCustom, 0.0, (float *)&g_vecZero, (float *)&g_vecZero,
			WEAPONCUSTOM_SND_XBOW_RELOAD1, RANDOM_FLOAT( 0.95, 1.0 ), WEAPONCUSTOM_SOUND, CHAN_ITEM, 0, 0 );

		SendWeaponAnim( CROSSBOW_FIRE1 );
	}
	else if ( m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] == 0 )
	{
		SendWeaponAnim( CROSSBOW_FIRE3 );
	}

	// player "shoot" animation
	m_hPlayer->SetAnimation( PLAYER_ATTACK1 );

	if ( !m_iClip && m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] <= 0 )
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
	if ( m_iClip == 0 )
	{
		PlayEmptySound();
		return;
	}

	m_hPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

	m_iClip--;

	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), g_evWeaponCustom, 0.0, (float *)&g_vecZero, (float *)&g_vecZero,
		WEAPONCUSTOM_SND_XBOW_FIRE1, RANDOM_FLOAT( 0.95, 1.0 ), WEAPONCUSTOM_SOUND, CHAN_WEAPON, 0, 0 );

	if ( m_iClip )
	{
		PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), g_evWeaponCustom, 0.0, (float *)&g_vecZero, (float *)&g_vecZero,
			WEAPONCUSTOM_SND_XBOW_RELOAD1, RANDOM_FLOAT( 0.95, 1.0 ), WEAPONCUSTOM_SOUND, CHAN_ITEM, 0, 0 );

		SendWeaponAnim( CROSSBOW_FIRE1 );
	}
	else
	{
		SendWeaponAnim( CROSSBOW_FIRE3 );
	}

	// player "shoot" animation
	m_hPlayer->SetAnimation( PLAYER_ATTACK1 );

	if ( !m_iClip && m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] <= 0 )
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

void CCrossbow::SecondaryAttack( void )
{
	if ( m_fInZoom )
	{
		strncpy( m_hPlayer->m_szAnimExtention, "bow", sizeof( m_hPlayer->m_szAnimExtention ) );
		m_fInZoom = false;
	}
	else if ( !( m_hPlayer->m_afPhysicsFlags & PFLAG_CAMERA ) )
	{
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
		PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), g_evWeaponCustom, 0.0, (float *)&g_vecZero, (float *)&g_vecZero,
			WEAPONCUSTOM_SND_XBOW_RELOAD1, RANDOM_FLOAT( 0.95, 1.0 ), WEAPONCUSTOM_SOUND, CHAN_ITEM, 0, 0 );
	}
}

void CCrossbow::WeaponIdle( void )
{
	m_hPlayer->GetAutoaimVector( AUTOAIM_2DEGREES );  // Update the autoaim crosshair in DM

	ResetEmptySound();

	if ( UTIL_WeaponTimeBase() > m_flNextSecondaryAttack )
	{
		// keep the zoom in sync with the scope state
		if ( m_fInZoom )
		{
			if ( !( m_hPlayer->m_afPhysicsFlags & PFLAG_CAMERA ) )
			{
				if ( FRunfuncs() )
					SetFOV( CROSSBOW_ZOOM_FOV );

				strncpy( m_hPlayer->m_szAnimExtention, "bowscope", sizeof( m_hPlayer->m_szAnimExtention ) );
			}
		}
		else
		{
			if ( FRunfuncs() )
				SetFOV( 0 );

			strncpy( m_hPlayer->m_szAnimExtention, "bow", sizeof( m_hPlayer->m_szAnimExtention ) );
		}
	}

	if ( UTIL_WeaponTimeBase() <= m_flTimeWeaponIdle )
		return;

	float flRand = UTIL_SharedRandomFloat( m_hPlayer->random_seed, 0, 1 );

	if ( flRand <= 0.75 )
	{
		SendWeaponAnim( m_iClip ? CROSSBOW_IDLE1 : CROSSBOW_IDLE2 );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_hPlayer->random_seed, 10, 15 );
	}
	else if ( m_iClip )
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
	data->iuser2 = m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ];
}

void CCrossbow::SetWeaponData( weapon_data_t *data )
{
	m_iPrimaryAmmoType = data->iuser1;
	m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] = data->iuser2;
}

class CCrossbowAmmo : public CPlayerAmmo
{
public:
	CCrossbowAmmo() : CPlayerAmmo( AMMO_CROSSBOWCLIP_GIVE, "bolts", BOLT_MAX_CARRY ) { }

	const char *GetDefaultModel( void ) { return CClassicModeWrapper::isEnabled() ? "models/hlclassic/w_crossbow_clip.mdl" : "models/w_crossbow_clip.mdl"; }
	const char *GetPickupSound( void ) { return CClassicModeWrapper::isEnabled() ? "hlclassic/items/9mmclip1.wav" : "items/9mmclip1.wav"; }
};

LINK_ENTITY_TO_CLASS( ammo_crossbow, CCrossbowAmmo );
