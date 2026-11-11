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
#include "minigun.h"

#ifndef CLIENT_DLL
#include "server_support.h"
#endif

LINK_ENTITY_TO_CLASS( weapon_minigun, CMinigun );

void CMinigun::Spawn( void )
{
	m_bExclusiveHold = true;
	m_iId = WEAPON_MINIGUN;
	pev->classname = MAKE_STRING( "weapon_minigun" );

	Precache();
	SET_MODEL( ENT( pev ), GetW_Model( "models/w_minigun.mdl" ) );

	m_iDefaultAmmo = MINIGUN_DEFAULT_GIVE;
	m_flCustomDmg = pev->dmg;
	m_fSpunUp = false;
	m_iSecondaryAmmoType = 0;

#ifdef CLIENT_DLL
	FallInit();
#else
	CBasePhysicsObject::Spawn();
#endif

	m_iSpeedState = MINIGUN_SPEED_NONE;
	m_flSpeedReduction = 0;
}

void CMinigun::Precache( void )
{
	PrecacheCustomModels();

#ifdef CLIENT_DLL
	PRECACHE_MODEL( "models/v_minigun.mdl" );
	PRECACHE_MODEL( "models/p_minigunidle.mdl" );
	PRECACHE_MODEL( "models/p_minigunspin.mdl" );
	PRECACHE_MODEL( "models/w_minigun.mdl" );
	PRECACHE_MODEL( "models/w_saw_clip.mdl" );

	m_iShell = PRECACHE_MODEL( "models/saw_shell.mdl" );// brass shell

	PRECACHE_SOUND( "minigun/hw_shoot1.wav" );
	PRECACHE_SOUND( "hassault/hw_spinup.wav" );
	PRECACHE_SOUND( "hassault/hw_spindown.wav" );
	PRECACHE_SOUND( "hassault/hw_spin.wav" );
#else
	UTIL_PrecacheModel( this, "models/v_minigun.mdl" );
	UTIL_PrecacheModel( this, "models/p_minigunidle.mdl" );
	UTIL_PrecacheModel( this, "models/p_minigunspin.mdl" );
	UTIL_PrecacheModel( this, "models/w_minigun.mdl" );
	UTIL_PrecacheModel( this, "models/w_saw_clip.mdl" );
	m_iShell = UTIL_PrecacheModel( this, "models/saw_shell.mdl" );
	PrecacheSound_Intercept( this, "minigun/hw_shoot1.wav" );
	PrecacheSound_Intercept( this, "hassault/hw_spinup.wav" );
	PrecacheSound_Intercept( this, "hassault/hw_spindown.wav" );
	PrecacheSound_Intercept( this, "hassault/hw_spin.wav" );
#endif

	m_usMinigunFire = PRECACHE_EVENT( 1, "events/minigun.sc" );
	m_usMinigunSpin = PRECACHE_EVENT( 1, "events/minigun_spin.sc" );
}

bool CMinigun::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "556";
	p->iMaxAmmo1 = _556_MAX_CARRY;
	p->pszAmmo1Entity = "ammo_556";
	p->iAmmo1Drop = MINIGUN_AMMO1_DROP;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->pszAmmo2Entity = NULL;
	p->iAmmo2Drop = AMMO_DROP_NONE;
	p->iMaxClip = MINIGUN_MAX_CLIP;
	p->iSlot = 9;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_MINIGUN;
	p->iFlags = ITEM_FLAG_NOAUTORELOAD | ITEM_FLAG_NOAUTOSWITCHEMPTY | ITEM_FLAG_IGNOREWEAPONSTAY;
	p->iWeight = MINIGUN_WEIGHT;

	return true;
}

bool CMinigun::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		AddToPlayerMessage();
		m_flSpeedReduction = 0;

#ifdef CLIENT_DLL
		m_hPlayer->SwitchWeapon( this );
#else
		pPlayer->SwitchWeapon( this );
#endif

		return true;
	}

	return false;
}

void CMinigun::PrimaryAttack( void )
{
	m_iId = WEAPON_MINIGUN;

	if ( m_flNextPrimaryAttack > UTIL_WeaponTimeBase() )
		return;

	// don't fire underwater or on ladders
	if ( m_hPlayer->pev->waterlevel == 3 || m_hPlayer->IsOnLadder() )
	{
		SetPModel( false );
		PlayEmptySound();

		m_fSpunUp = false;
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.15;
		return;
	}

	m_fSpinning = true;

	if ( !m_fSpunUp )
	{
		SetPlayerSpeed( MINIGUN_SPEED_SPINUP );
		SetPModel( true );

		PLAYBACK_EVENT_FULL( FEV_RELIABLE | FEV_UPDATE, m_hPlayer->edict(), m_usMinigunSpin, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, MINIGUN_SPIN_START, 0, 0, 0 );

		SendWeaponAnim( MINIGUN_SPINUP );
		EMIT_SOUND_DYN( ENT( m_hPlayer->pev ), CHAN_ITEM, "hassault/hw_spinup.wav", 1.0, ATTN_NORM, 0, 94 + RANDOM_LONG( 0, 15 ) );

		m_fSpunUp = true;
		m_flNextPrimaryAttack = m_flTimeWeaponIdle = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.0;
		return;
	}

	SetPlayerSpeed( MINIGUN_SPEED_FIRE );

	Vector vecSrc = m_hPlayer->GetGunPosition();
	Vector vecAiming = m_hPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	Vector vecSpread = BulletAccuracy( VECTOR_CONE_10DEGREES, VECTOR_CONE_7DEGREES, VECTOR_CONE_5DEGREES );

	m_hPlayer->m_iWeaponVolume = 1536;

	if ( m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] <= 0 )
	{
		// out of ammo, keep the barrels spinning
		SetPModel( true );

		PLAYBACK_EVENT_FULL( FEV_RELIABLE, m_hPlayer->edict(), m_usMinigunSpin, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, MINIGUN_SPIN_LOOP, 0, 0, 0 );

		if ( m_hPlayer->pev->weaponanim != MINIGUN_SPINIDLE )
			SendWeaponAnim( MINIGUN_SPINIDLE );

		m_flNextPrimaryAttack = m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.06;
		return;
	}

	// player "shoot" animation
	m_hPlayer->SetAnimation( PLAYER_ATTACK1 );

	SetPModel( true );

	m_hPlayer->FireBullets( 2, vecSrc, vecAiming, vecSpread, 4096, BULLET_PLAYER_SAW, 0, (int)m_flCustomDmg, NULL, FBDM_DRAW );

	m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ]--;

	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), m_usMinigunFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 0 );

	if ( m_hPlayer->pev->weaponanim != MINIGUN_SPINFIRE )
		SendWeaponAnim( MINIGUN_SPINFIRE );

#ifndef CLIENT_DLL
	if ( m_hPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		m_hPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );
#endif

	m_flNextPrimaryAttack = m_flNextPrimaryAttack + 0.06;

	if ( UTIL_WeaponTimeBase() > m_flNextPrimaryAttack )
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.06;
}

// Spins the barrels without firing
void CMinigun::SecondaryAttack( void )
{
	if ( m_hPlayer->IsOnLadder() || m_hPlayer->pev->waterlevel == 3 )
	{
		SetPModel( false );
		PlayEmptySound();

		m_fSpunUp = false;
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.15;
		return;
	}

	if ( m_hPlayer->pev->button & IN_ATTACK )
	{
		PrimaryAttack();
		return;
	}

	if ( m_flNextSecondaryAttack > UTIL_WeaponTimeBase() )
		return;

	m_fSpinning = true;
	SetPlayerSpeed( MINIGUN_SPEED_SPINUP );

	float flNextAttack;

	if ( m_fSpunUp )
	{
		SetPModel( true );

		PLAYBACK_EVENT_FULL( FEV_RELIABLE, m_hPlayer->edict(), m_usMinigunSpin, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, MINIGUN_SPIN_LOOP, 0, 0, 0 );

		if ( m_hPlayer->pev->weaponanim != MINIGUN_SPINIDLE )
			SendWeaponAnim( MINIGUN_SPINIDLE );

		flNextAttack = UTIL_WeaponTimeBase() + 0.4;
	}
	else
	{
		SetPModel( true );

		PLAYBACK_EVENT_FULL( FEV_RELIABLE, m_hPlayer->edict(), m_usMinigunSpin, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, MINIGUN_SPIN_START, 0, 0, 0 );

		SendWeaponAnim( MINIGUN_SPINUP );
		m_fSpunUp = true;

		flNextAttack = UTIL_WeaponTimeBase() + 0.9;
	}

	m_flTimeWeaponIdle = flNextAttack;
	m_flNextPrimaryAttack = flNextAttack;
	m_flNextSecondaryAttack = flNextAttack;
}

void CMinigun::TertiaryAttack( void )
{
	SecondaryAttack();
}

void CMinigun::Reload( void )
{
}

void CMinigun::CustomDecrement( float decrement )
{
}

int CMinigun::SecondaryAmmoIndex( void )
{
	return m_iSecondaryAmmoType;
}

bool CMinigun::Deploy( void )
{
	m_hPlayer->m_fGrappleFired = false;

	SetPlayerSpeed( MINIGUN_SPEED_IDLE );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0;

	m_fSpunUp = false;
	m_fSpinModel = false;
	m_fSpinning = false;
	m_flDeployTime = UTIL_WeaponTimeBase() + 2.0;

#ifdef CLIENT_DLL
	return DefaultDeploy( GetV_Model( "models/v_minigun.mdl" ), GetP_Model( "models/p_minigunidle.mdl" ), MINIGUN_DRAW, "minigun" );
#else
	const char *viewModel = GetV_Model( "models/v_minigun.mdl" );

	m_hPlayer->pev->viewmodel = MAKE_STRING( viewModel );

	const char *playerModel = GetP_Model( "models/p_minigunidle.mdl" );

	m_hPlayer->pev->weaponmodel = MAKE_STRING( playerModel );
	UTIL_CopyString( m_hPlayer->m_szAnimExtention, "minigun", sizeof( m_hPlayer->m_szAnimExtention ) );
	SendWeaponAnim( MINIGUN_DRAW );
	m_hPlayer->SetAnimation( PLAYER_DEPLOY );
	return true;
#endif
}

CBasePlayerItem *CMinigun::DropItem( void )
{
	m_hPlayer->pev->fuser4 = 0;

	return CBasePlayerWeapon::DropItem();
}

void CMinigun::Holster( int skiplocal /* = 0 */ )
{
	SetPlayerSpeed( MINIGUN_SPEED_NONE );

	m_hPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	m_fSpunUp = false;
	m_hPlayer->pev->viewmodel = 0;
}

void CMinigun::WeaponIdle( void )
{
	ResetEmptySound();

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( !m_fSpinning )
		m_fSpinning = true;

	SetPlayerSpeed( MINIGUN_SPEED_IDLE );

	if ( m_fSpunUp )
	{
		SetPModel( true );

		PLAYBACK_EVENT_FULL( FEV_RELIABLE | FEV_UPDATE, m_hPlayer->edict(), m_usMinigunSpin, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, MINIGUN_SPIN_STOP, 0, 0, 0 );

		m_fSpunUp = false;
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0;
	}
	else
	{
		SetPModel( false );
		SendWeaponAnim( MINIGUN_IDLE );

		float idleDelay = UTIL_SharedRandomFloat( m_hPlayer->random_seed, 10, 15 );

		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + idleDelay;
	}
}

// Use a dropped minigun to pick it up
int CMinigun::ObjectCaps( void )
{
	if ( m_hPlayer )
		return 0;

	return FCAP_IMPULSE_USE;
}

void CMinigun::OnCreate( void )
{
	CBasePlayerWeapon::OnCreate();

	pev->spawnflags |= SF_ITEM_USE_ONLY;
}

void CMinigun::Materialize( void )
{
#ifdef CLIENT_DLL
	CBasePlayerWeapon::Materialize();
#else
	CBasePickupObject::Materialize();
#endif
}

void CMinigun::TouchedWorld( bool firstTouch )
{
}

void CMinigun::Dropped( CBaseEntity *pDropper )
{
	CBasePlayerWeapon::Dropped( pDropper );

	pev->spawnflags &= ~SF_ITEM_USE_ONLY;
}

// Once it rests on the ground, the minigun can only be picked up with +use
bool CMinigun::CanCollect( CBaseEntity *pOther, COLLECT_TYPE collectType )
{
	if ( !CBasePlayerWeapon::CanCollect( pOther, collectType ) )
		return false;

	if ( ( pev->flags & FL_ONGROUND ) && !( pev->spawnflags & SF_ITEM_USE_ONLY ) )
		pev->spawnflags |= SF_ITEM_USE_ONLY;

	return collectType != COLLECT_TOUCH || !( pev->flags & FL_ONGROUND );
}

void CMinigun::SetPModel( bool bSpinning )
{
	if ( m_fSpinModel == bSpinning )
		return;

	const char *model = GetP_Model( bSpinning ? "models/p_minigunspin.mdl" : "models/p_minigunidle.mdl" );

	m_hPlayer->pev->weaponmodel = MAKE_STRING( model );
	m_fSpinModel = bSpinning;
}

// pev->fuser4 tells the movement code that the minigun slows the player down
void CMinigun::SetPlayerSpeed( int iState )
{
	if ( iState == m_iSpeedState )
		return;

	m_hPlayer->m_flEffectSpeed += m_flSpeedReduction;

	switch ( iState )
	{
	case MINIGUN_SPEED_FIRE:
		m_flSpeedReduction = 0.99;
		break;

	case MINIGUN_SPEED_IDLE:
		m_flSpeedReduction = 0.33;
		break;

	case MINIGUN_SPEED_SPINUP:
		m_flSpeedReduction = 0.67;
		break;

	default:
		m_flSpeedReduction = 0;
		break;
	}

	if ( iState != MINIGUN_SPEED_NONE )
	{
		m_hPlayer->pev->fuser4 = 1.0;
		m_hPlayer->m_flEffectSpeed -= m_flSpeedReduction;
	}
	else
		m_hPlayer->pev->fuser4 = 0;

#ifndef CLIENT_DLL
	m_hPlayer->UpdatePlayerSpeed();
#endif

	m_iSpeedState = iState;
}

void CMinigun::GetWeaponData( weapon_data_t *data )
{
	data->iuser1 = m_iPrimaryAmmoType;
	data->iuser2 = m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ];
	data->fuser1 = m_fSpunUp;
}

void CMinigun::SetWeaponData( weapon_data_t *data )
{
	m_iPrimaryAmmoType = data->iuser1;
	m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] = data->iuser2;
	m_fSpunUp = data->fuser1 != 0;
}

#ifdef CLIENT_DLL
class CMinigunChainammo : public CPlayerAmmo
{
public:
	CMinigunChainammo() : CPlayerAmmo( AMMO_556_GIVE, "556", _556_MAX_CARRY ) { }

	const char *GetDefaultModel( void ) { return "models/w_saw_clip.mdl"; }
};

LINK_ENTITY_TO_CLASS( ammo_556, CMinigunChainammo );
#endif
