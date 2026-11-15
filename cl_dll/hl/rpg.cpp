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

LINK_ENTITY_TO_CLASS( weapon_rpg, CRpg );

void CRpg::Spawn( void )
{
	Precache();

	if ( CClassicModeWrapper::isEnabled() )
		SET_MODEL( ENT( pev ), GetW_Model( "models/hlclassic/w_rpg.mdl" ) );
	else
		SET_MODEL( ENT( pev ), GetW_Model( "models/w_rpg.mdl" ) );

	m_iId = WEAPON_RPG;
	m_iClip = RPG_MAX_CLIP;
	m_fSpotActive = FALSE;
	m_flCustomDmg = pev->dmg;

	// more default ammo in multiplay.
	if ( IsMultiplayer() )
		m_iDefaultAmmo = RPG_DEFAULT_GIVE * 2;
	else
		m_iDefaultAmmo = RPG_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}

void CRpg::Precache( void )
{
	PrecacheCustomModels();

	if ( CClassicModeWrapper::isEnabled() )
	{
		PRECACHE_MODEL( "models/hlclassic/w_rpg.mdl" );
		PRECACHE_MODEL( "models/hlclassic/v_rpg.mdl" );
		PRECACHE_MODEL( "models/hlclassic/p_rpg.mdl" );
	}
	else
	{
		PRECACHE_MODEL( "models/w_rpg.mdl" );
		PRECACHE_MODEL( "models/v_rpg.mdl" );
		PRECACHE_MODEL( "models/p_rpg.mdl" );
		PRECACHE_MODEL( "models/p_rpg_r.mdl" );
	}

	PRECACHE_SOUND( "hlclassic/items/9mmclip1.wav" );
	PRECACHE_SOUND( "hlclassic/weapons/rocketfire1.wav" );
	PRECACHE_SOUND( "hlclassic/weapons/glauncher.wav" );
	PRECACHE_SOUND( "items/9mmclip1.wav" );
	PRECACHE_SOUND( "weapons/rocketfire1.wav" );
	PRECACHE_SOUND( "weapons/glauncher.wav" ); // alternative fire sound

	UTIL_PrecacheOther( "rpg_laser_spot" );
	UTIL_PrecacheOther( "rpg_rocket" );
}

void CRpg::Reload( void )
{
	if ( m_iClip != 0 )
		return;

	if ( !DefaultReload( RPG_MAX_CLIP, RPG_RELOAD, 2.0 ) )
		return;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_hPlayer->random_seed, 10, 15 );

	// the reload model shows the rocket going in
	if ( !CClassicModeWrapper::isEnabled() )
		m_hPlayer->pev->weaponmodel = MAKE_STRING( GetP_Model( "models/p_rpg_r.mdl" ) );

	if ( m_fSpotActive )
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 2.1;
}

bool CRpg::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "rockets";
	p->iMaxAmmo1 = ROCKET_MAX_CARRY;
	p->pszAmmo1Entity = "ammo_rpgclip";
	p->iAmmo1Drop = RPG_AMMO1_DROP;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->pszAmmo2Entity = NULL;
	p->iAmmo2Drop = AMMO_DROP_NONE;
	p->iMaxClip = RPG_MAX_CLIP;
	p->iSlot = 3;
	p->iPosition = 0;
	p->iId = m_iId = WEAPON_RPG;
	p->iFlags = 0;
	p->iWeight = RPG_WEIGHT;

	return true;
}

bool CRpg::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		AddToPlayerMessage();
		return true;
	}

	return false;
}

bool CRpg::Deploy( void )
{
	int iAnim;

	if ( m_iClip )
		iAnim = RPG_DRAW1;
	else
		iAnim = RPG_DRAW_UL;

	if ( CClassicModeWrapper::isEnabled() )
		return DefaultDeploy( GetV_Model( "models/hlclassic/v_rpg.mdl" ), GetP_Model( "models/hlclassic/p_rpg.mdl" ), iAnim, "rpg" );

	return DefaultDeploy( GetV_Model( "models/v_rpg.mdl" ), GetP_Model( "models/p_rpg.mdl" ), iAnim, "rpg" );
}

bool CRpg::CanHolster( void )
{
	// can't put away while guiding a missile.
	return !m_fSpotActive || !m_cActiveRockets;
}

void CRpg::Holster( int skiplocal /* = 0 */ )
{
	m_fInReload = false;// cancel any reload in progress.

	m_hPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0;
	m_hPlayer->pev->viewmodel = 0;
}

void CRpg::PrimaryAttack( void )
{
	if ( !m_iClip )
	{
		PlayEmptySound();

		if ( UTIL_WeaponTimeBase() + 0.5 > m_flNextPrimaryAttack )
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;

		return;
	}

	m_hPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_hPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	// player "shoot" animation
	m_hPlayer->SetAnimation( PLAYER_ATTACK1 );

	SendWeaponAnim( RPG_FIRE2 );

	UTIL_MakeVectors( m_hPlayer->pev->v_angle );
	m_hPlayer->GetGunPosition();

	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), g_evWeaponCustom, 0.0, (float *)&g_vecZero, (float *)&g_vecZero,
		WEAPONCUSTOM_SND_ROCKETFIRE1, 0.9, WEAPONCUSTOM_SOUND, CHAN_WEAPON, 0, 0 );
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), g_evWeaponCustom, 0.0, (float *)&g_vecZero, (float *)&g_vecZero,
		WEAPONCUSTOM_SND_GLAUNCHER, 0.7, WEAPONCUSTOM_SOUND, CHAN_BODY, 0, 0 );

	m_iClip = 0;

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.5;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.5;
}

void CRpg::WeaponIdle( void )
{
	if ( m_hPlayer && !(CBaseEntity *)m_hPlayer->m_hTank && !CClassicModeWrapper::isEnabled() )
		m_hPlayer->pev->weaponmodel = MAKE_STRING( GetP_Model( "models/p_rpg.mdl" ) );

	ResetEmptySound();

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( !m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1;
		return;
	}

	int iAnim;
	float flRand = UTIL_SharedRandomFloat( m_hPlayer->random_seed, 0, 1 );

	if ( flRand <= 0.75 || m_fSpotActive )
	{
		if ( m_iClip )
			iAnim = RPG_IDLE;
		else
			iAnim = RPG_IDLE_UL;

		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 90.0 / 15.0;
	}
	else
	{
		if ( m_iClip )
			iAnim = RPG_FIDGET;
		else
			iAnim = RPG_FIDGET_UL;

		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3.0;
	}

	SendWeaponAnim( iAnim );
}

void CRpg::GetWeaponData( weapon_data_t *data )
{
	data->iuser4 = m_fSpotActive;
	data->fuser1 = m_cActiveRockets;
}

void CRpg::SetWeaponData( weapon_data_t *data )
{
	m_fSpotActive = data->iuser4;
	m_cActiveRockets = (int)data->fuser1;
}

class CRpgAmmo : public CPlayerAmmo
{
public:
	CRpgAmmo() : CPlayerAmmo( AMMO_RPGCLIP_GIVE, "rockets", ROCKET_MAX_CARRY ) { }

	const char *GetDefaultModel( void ) { return CClassicModeWrapper::isEnabled() ? "models/hlclassic/w_rpgammo.mdl" : "models/w_rpgammo.mdl"; }
	const char *GetPickupSound( void ) { return CClassicModeWrapper::isEnabled() ? "hlclassic/items/9mmclip1.wav" : "items/9mmclip1.wav"; }
};

LINK_ENTITY_TO_CLASS( ammo_rpgclip, CRpgAmmo );
