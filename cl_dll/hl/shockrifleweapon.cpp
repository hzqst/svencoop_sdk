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

static const char *pSparkSounds[] =
{
	"buttons/spark1.wav",
	"buttons/spark2.wav",
	"buttons/spark3.wav",
	"buttons/spark4.wav",
	"buttons/spark5.wav",
	"buttons/spark6.wav",
};

LINK_ENTITY_TO_CLASS( weapon_shockrifle, CShockRifle );

bool CShockRifle::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "shock charges";
	p->iMaxAmmo1 = SHOCK_MAX_CARRY;
	p->pszAmmo1Entity = NULL;
	p->iAmmo1Drop = AMMO_DROP_NONE;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->pszAmmo2Entity = NULL;
	p->iAmmo2Drop = AMMO_DROP_NONE;
	p->iMaxClip = SHOCKRIFLE_MAX_CLIP;
	p->iSlot = 9;
	p->iPosition = 0;
	p->iId = m_iId = WEAPON_SHOCKRIFLE;
	p->iFlags = ITEM_FLAG_NOAUTORELOAD | ITEM_FLAG_NOAUTOSWITCHEMPTY;
	p->iWeight = SHOCKRIFLE_WEIGHT;

	return true;
}

bool CShockRifle::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( !CBasePlayerWeapon::AddToPlayer( pPlayer ) )
		return false;

	// multiplayer rifles come fully charged
	if ( IsMultiplayer() && !( pev->spawnflags & SF_NODEFAULTAMMO ) )
		pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] = SHOCK_MAX_CARRY;

	AddToPlayerMessage();
	return true;
}

void CShockRifle::Materialize( void )
{
	CBasePlayerWeapon::Materialize();
}

void CShockRifle::CustomDecrement( float flDecrement )
{
	m_flRechargeTime = V_max( m_flRechargeTime - flDecrement, -0.01 );
	m_flAmmoDecayTime = V_max( m_flAmmoDecayTime - flDecrement, -0.01 );
}

void CShockRifle::Precache( void )
{
	PRECACHE_MODEL( "models/v_shock.mdl" );
	PRECACHE_MODEL( "models/w_shock_rifle.mdl" );
	PRECACHE_MODEL( "models/p_shock.mdl" );

	m_iBeam = PRECACHE_MODEL( "sprites/lgtning.spr" );
	PRECACHE_MODEL( "sprites/blueflare1.spr" );

	PRECACHE_SOUND( "weapons/shock_discharge2.wav" );
	PRECACHE_SOUND( "weapons/shock_draw.wav" );
	PRECACHE_SOUND( "weapons/shock_fire.wav" );
	PRECACHE_SOUND( "weapons/shock_impact.wav" );
	PRECACHE_SOUND( "weapons/shock_recharge.wav" );
	PRECACHE_SOUND( "debris/beamstart15.wav" );
	PRECACHE_SOUND( "weapons/lhit.wav" );
	PRECACHE_SOUND( "weapons/lhit_end.wav" );

	for ( int i = 0; i < ARRAYSIZE( pSparkSounds ); i++ )
		PRECACHE_SOUND( (char *)pSparkSounds[ i ] );

	PrecacheCustomModels();

	UTIL_PrecacheOther( "monster_shockroach" );
	UTIL_PrecacheOther( "shock_beam" );

	m_usShockRifle = PRECACHE_EVENT( 1, "events/shockrifle.sc" );
	m_usShockRifleBeam = PRECACHE_EVENT( 1, "events/shockriflebeam.sc" );
}

void CShockRifle::Spawn( void )
{
	m_bExclusiveHold = true;
	m_iId = WEAPON_SHOCKRIFLE;
	pev->classname = MAKE_STRING( "weapon_shockrifle" );

	Precache();
	SET_MODEL( ENT( pev ), GetW_Model( "models/w_shock_rifle.mdl" ) );

	m_iDefaultAmmo = SHOCKRIFLE_DEFAULT_GIVE;
	m_flCustomDmg = pev->dmg;

	FallInit();// get ready to fall down.
}

const char *CShockRifle::GetPickupSound( void )
{
	return "weapons/shock_draw.wav";
}

bool CShockRifle::Deploy( void )
{
	return DefaultDeploy( GetV_Model( "models/v_shock.mdl" ), GetP_Model( "models/p_shock.mdl" ), SHOCKRIFLE_DRAW, "bow" );
}

void CShockRifle::Holster( int skiplocal /* = 0 */ )
{
	if ( m_fInAttack )
		EndBeamAttack();

	m_hPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	m_hPlayer->pev->viewmodel = 0;
}

CBasePlayerItem *CShockRifle::DropItem( void )
{
	return NULL;
}

void CShockRifle::PrimaryAttack( void )
{
	if ( m_fInAttack )
		EndBeamAttack();

	if ( m_hPlayer->pev->waterlevel == 3 )
	{
		// discharges the whole battery under water
		UseAmmo( SHOCKRIFLE_WATER_DISCHARGE );
		SecondaryAttack();

		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
		return;
	}

	if ( m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] <= 0 )
		return;

	m_hPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	m_hPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	UTIL_MakeVectors( m_hPlayer->pev->v_angle );

	Vector vecSrc = m_hPlayer->GetGunPosition() + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -12;

	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), m_usShockRifle, 0.0, (float *)&vecSrc, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 0 );

	// player "shoot" animation
	m_hPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.2;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_hPlayer->random_seed, 10, 15 );

	UseAmmo( SHOCKRIFLE_PRIMARY_AMMO );
}

// Continuous beam attack
// The server draws the beam
void CShockRifle::BeamFire( const Vector &vecSrc, const Vector &vecDir )
{
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), m_usShockRifleBeam, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 0 );
}

void CShockRifle::SecondaryAttack( void )
{
	Vector vecSrc = m_hPlayer->GetGunPosition();

	UTIL_MakeVectors( m_hPlayer->pev->v_angle + m_hPlayer->pev->punchangle );

	if ( m_hPlayer->pev->flags & FL_INWATER )
		UseAmmo( SHOCKRIFLE_WATER_BEAM_AMMO );

	if ( m_fInAttack )
	{
		if ( m_fInAttack == 1 )
		{
			BeamFire( vecSrc, gpGlobals->v_forward );

			m_hPlayer->m_iWeaponVolume = SHOCKRIFLE_BEAM_VOLUME;

			if ( m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] <= 0 )
			{
				EndBeamAttack();
				m_fInAttack = 0;
			}
		}

		return;
	}

	if ( m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] <= 0 )
	{
		PlayEmptySound();
		return;
	}

	m_flStartAttack = gpGlobals->time;
	m_hPlayer->m_iWeaponVolume = SHOCKRIFLE_BEAM_VOLUME;

	EMIT_SOUND_DYN( ENT( m_hPlayer->pev ), CHAN_WEAPON, "debris/beamstart15.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
	EMIT_SOUND_DYN( ENT( m_hPlayer->pev ), CHAN_STATIC, "weapons/lhit.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );

	pev->dmgtime = gpGlobals->time + 0.2;
	m_fInAttack = 1;
}

void CShockRifle::TertiaryAttack( void )
{
	WeaponIdle();
}

void CShockRifle::EndBeamAttack( void )
{
	m_fInAttack = 0;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
}

// Drains a nearly empty battery
void CShockRifle::AmmoDecay( void )
{
	if ( m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] > SHOCKRIFLE_DECAY_AMMO )
		return;

	if ( UTIL_WeaponTimeBase() <= m_flAmmoDecayTime )
		return;

	UseAmmo( 1 );

	m_flAmmoDecayTime = m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] / 60.0 + UTIL_WeaponTimeBase() + 0.05;
}

void CShockRifle::UseAmmo( int iCount )
{
	if ( m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] <= 0 )
		return;

	m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] -= iCount;

	if ( m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] <= 0 )
	{
		m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] = 0;
		DetonateInHand();
	}
}

// Empty on the client
void CShockRifle::DetonateInHand( void )
{
}

void CShockRifle::ItemPostFrame( void )
{
	CBasePlayerWeapon::ItemPostFrame();
}

void CShockRifle::InactiveItemPostFrame( void )
{
	AmmoDecay();
}

void CShockRifle::WeaponIdle( void )
{
	if ( m_fInAttack )
		EndBeamAttack();

	AmmoDecay();

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	int iAnim;
	float flRand = UTIL_SharedRandomFloat( m_hPlayer->random_seed, 0, 1 );

	if ( flRand <= 0.75 )
	{
		iAnim = SHOCKRIFLE_IDLE3;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.5;
	}
	else
	{
		iAnim = SHOCKRIFLE_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3.75;
	}

	SendWeaponAnim( iAnim );
}

void CShockRifle::GetWeaponData( weapon_data_t *data )
{
	data->iuser1 = m_iPrimaryAmmoType;
	data->iuser2 = m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ];
	data->iuser3 = m_iSecondaryAmmoType;
	data->iuser4 = m_hPlayer->m_rgAmmo[ m_iSecondaryAmmoType ];
	data->fuser1 = m_flRechargeTime;
}

void CShockRifle::SetWeaponData( weapon_data_t *data )
{
	m_iPrimaryAmmoType = data->iuser1;
	m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] = data->iuser2;
	m_flRechargeTime = data->fuser1;
}
