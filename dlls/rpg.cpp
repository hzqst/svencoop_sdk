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
#if !defined( OEM_BUILD )

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
#include "soundent.h"
#include "weaponinfo.h"
#include "event_flags.h"

#define WEAPONCUSTOM_SND_ROCKETFIRE1 1

#undef PRECACHE_MODEL
#define PRECACHE_MODEL( path ) UTIL_PrecacheModel( this, path )
#undef PRECACHE_SOUND
#define PRECACHE_SOUND( path ) PrecacheSound_Intercept( this, path )

enum rpg_e
{
	RPG_IDLE = 0,
	RPG_FIDGET,
	RPG_RELOAD,    // to reload
	RPG_FIRE2,     // to empty
	RPG_HOLSTER1,  // loaded
	RPG_DRAW1,     // loaded
	RPG_HOLSTER2,  // unloaded
	RPG_DRAW_UL,   // unloaded
	RPG_IDLE_UL,   // unloaded idle
	RPG_FIDGET_UL, // unloaded fidget
};

LINK_ENTITY_TO_CLASS( rpg_laser_spot, CRPGSpot );

//=========================================================
//=========================================================
CRPGSpot *CRPGSpot::CreateSpot( void )
{
	CRPGSpot *pSpot = GetClassPtr( (CRPGSpot *) NULL );
	pSpot->Spawn();

	pSpot->pev->classname = MAKE_STRING( "rpg_laser_spot" );

	return pSpot;
}

//=========================================================
//=========================================================
void CRPGSpot::Spawn( void )
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	pev->rendermode = kRenderGlow;
	pev->renderfx = kRenderFxNoDissipation;
	pev->renderamt = 255;

	SetModel_Intercept( edict(), "sprites/laserdot.spr" );
	UTIL_SetOrigin( pev, pev->origin );
};

//=========================================================
// Suspend- make the laser sight invisible.
//=========================================================
void CRPGSpot::Suspend( float flSuspendTime )
{
	pev->effects |= EF_NODRAW;

	SetThink( &CRPGSpot::Revive );
	pev->nextthink = gpGlobals->time + flSuspendTime;
}

//=========================================================
// Revive - bring a suspended laser sight back.
//=========================================================
void CRPGSpot::Revive( void )
{
	pev->effects &= ~EF_NODRAW;

	SetThink( NULL );
}

void CRPGSpot::Precache( void )
{
	PRECACHE_MODEL( "sprites/laserdot.spr" );
};

LINK_ENTITY_TO_CLASS( rpg_rocket, CRpgRocket );

TYPEDESCRIPTION CRpgRocket::m_SaveData[] = {
	DEFINE_FIELD( CRpgRocket, m_flIgniteTime, FIELD_TIME ),
	DEFINE_FIELD( CRpgRocket, m_hLauncher, FIELD_EHANDLE ),
};

IMPLEMENT_SAVERESTORE( CRpgRocket, CGrenade );

//=========================================================
//=========================================================
CRpgRocket *CRpgRocket::CreateRpgRocket( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, CRpg *pLauncher )
{
	CRpgRocket *pRocket = GetClassPtr( (CRpgRocket *)NULL );

	UTIL_SetOrigin( pRocket->pev, vecOrigin );
	pRocket->pev->angles = vecAngles;
	pRocket->pev->owner = pOwner->edict();
	pRocket->Spawn();
	pRocket->SetTouch( &CRpgRocket::RocketTouch );
	pRocket->pev->owner = pOwner->edict();

	if ( pLauncher )
	{
		pRocket->m_hLauncher = pLauncher;
		pRocket->m_flCustomDmg = pLauncher->m_flCustomDmg;
		pLauncher->m_cActiveRockets++;
	}

	return pRocket;
}

//=========================================================
//=========================================================
void CRpgRocket :: Spawn( void )
{
	Precache();

	AddEntityFlag( 2 );
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SetModel_Intercept( edict(), "models/rpgrocket.mdl");
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin( pev, pev->origin );

	pev->classname = MAKE_STRING("rpg_rocket");

	SetThink( &CRpgRocket::IgniteThink );
	SetTouch( &CRpgRocket::ExplodeTouch );

	if ( pev->owner && !( VARS( pev->owner )->flags & FL_MONSTER ) )
	{
		pev->angles.x -= 30;
	UTIL_MakeVectors( pev->angles );
	pev->angles.x = -(pev->angles.x + 30);

	pev->nextthink = gpGlobals->time + 0.4;
	}
	else
	{
		pev->nextthink = gpGlobals->time;
	}

	pev->gravity = 0.5;
	pev->velocity = gpGlobals->v_forward * 250;
	pev->dmg = sk_plr_rpg.value;

	if ( m_flCustomDmg > 0 )
		pev->dmg = m_flCustomDmg;
}

//=========================================================
//=========================================================
void CRpgRocket :: RocketTouch ( CBaseEntity *pOther )
{
	CRpg *launcher = (CRpg *) (CBaseEntity *) m_hLauncher;

	if ( launcher )
	{
		// my launcher is still around, tell it I'm dead.
		launcher->m_cActiveRockets--;
	}

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "weapons/rocket1.wav", 0, 0,
		SND_STOP, PITCH_NORM, 0, false, g_vecZero );
	ExplodeTouch( pOther );
}

//=========================================================
//=========================================================
void CRpgRocket :: Precache( void )
{
	PRECACHE_MODEL("models/rpgrocket.mdl");
	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	PRECACHE_SOUND ("weapons/rocket1.wav");
}

void CRpgRocket :: IgniteThink( void  )
{
	// pev->movetype = MOVETYPE_TOSS;

	pev->movetype = MOVETYPE_BOUNCEMISSILE;
	pev->effects |= EF_LIGHT;

	// make rocket sound
	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 1, 0.5,
		0, PITCH_NORM, 0, false, g_vecZero );

	// rocket trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );

		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(entindex());	// entity
		WRITE_SHORT(m_iTrail );	// model
		WRITE_BYTE( 40 ); // life
		WRITE_BYTE( 5 );  // width
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness

	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)

	m_flIgniteTime = gpGlobals->time;

	CSoundEnt::InsertSound( bits_SOUND_DANGER, pev->origin + pev->velocity,
	                        (int) pev->velocity.Length(), 0.1f, this );

	// set to follow laser spot
	SetThink( &CRpgRocket::FollowThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CRpgRocket :: FollowThink( void  )
{
	CBaseEntity *pOther = NULL;
	Vector vecTarget;
	Vector vecDir;
	float flDist, flMax, flDot;
	TraceResult tr;

	UTIL_MakeAimVectors( pev->angles );

	vecTarget = gpGlobals->v_forward;
	flMax = 4096;

	// Examine all entities within a reasonable radius
	while ((pOther = UTIL_FindEntityByClassname( pOther, "rpg_laser_spot" )) != NULL)
	{
		if ( !pOther->pev->owner || !pev->owner || pOther->pev->owner != pev->owner )
			continue;

		UTIL_TraceLine ( pev->origin, pOther->pev->origin, dont_ignore_monsters, ENT(pev), &tr );
		// ALERT( at_console, "%f\n", tr.flFraction );
		if (tr.flFraction >= 0.90)
		{
			vecDir = pOther->pev->origin - pev->origin;
			flDist = vecDir.Length( );
			vecDir = vecDir.Normalize( );
			flDot = DotProduct( gpGlobals->v_forward, vecDir );
			if ((flDot > 0) && (flDist * (1 - flDot) < flMax))
			{
				flMax = flDist * (1 - flDot);
				vecTarget = vecDir;
			}
		}
	}

	pev->angles = UTIL_VecToAngles( vecTarget );

	// this acceleration and turning math is totally wrong, but it seems to respond well so don't change it.
	float flSpeed = pev->velocity.Length();
	if (gpGlobals->time - m_flIgniteTime < 1.0)
	{
		pev->velocity = pev->velocity * 0.2 + vecTarget * (flSpeed * 0.8 + 400);
		if (pev->waterlevel == 3)
		{
			// go slow underwater
			if (pev->velocity.Length() > 400 )
			{
				pev->velocity = pev->velocity.Normalize() * 400;
			}
			UTIL_BubbleTrail( pev->origin - pev->velocity * 0.1, pev->origin, 4 );
		}
		else
		{
			if (pev->velocity.Length() > 2000)
			{
				pev->velocity = pev->velocity.Normalize() * 2000;
			}
		}
	}
	else
	{
		if (pev->effects & EF_LIGHT)
		{
			pev->effects = 0;
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 0, 0,
				SND_STOP, PITCH_NORM, 0, false, g_vecZero );
		}
		pev->velocity = pev->velocity * 0.2 + vecTarget * flSpeed * 0.798;
		if (pev->waterlevel == 0 && pev->velocity.Length() < 1500)
		{
			CRpg *launcher = (CRpg *) (CBaseEntity *) m_hLauncher;

			if ( launcher )
				launcher->m_cActiveRockets--;

			Detonate( );
		}
	}

	CSoundEnt::InsertSound( bits_SOUND_DANGER, pev->origin + pev->velocity,
	                        (int) ( pev->velocity.Length() * 0.5f ), 0.1f, this );
	CSoundEnt::InsertSound( bits_SOUND_DANGER, pev->origin + pev->velocity * 2,
	                        (int) ( pev->velocity.Length() * 0.5f ), 0.1f, this );

	pev->nextthink = gpGlobals->time + 0.1;
}

LINK_ENTITY_TO_CLASS( weapon_rpg, CRpg );

void CRpg::Spawn( void )
{
	Precache( );

	if ( CClassicMode::GetInstance()->IsEnabled() )
		SetModel_Intercept( edict(), GetW_Model( "models/hlclassic/w_rpg.mdl" ) );
	else
		SetModel_Intercept( edict(), GetW_Model( "models/w_rpg.mdl" ) );

	m_iId = WEAPON_RPG;
	m_iClip = RPG_MAX_CLIP;
	m_fSpotActive = FALSE;
	m_flCustomDmg = pev->dmg;

	// more default ammo in multiplay.
	if ( IsMultiplayer() )
		m_iDefaultAmmo = RPG_DEFAULT_GIVE * 2;
	else
		m_iDefaultAmmo = RPG_DEFAULT_GIVE;

	CBasePhysicsObject::Spawn();// get ready to fall down.
}

void CRpg::Precache( void )
{
	PrecacheCustomModels();

	if ( CClassicMode::GetInstance()->IsEnabled() )
	{
		PRECACHE_MODEL( "models/hlclassic/w_rpg.mdl" );
		PRECACHE_MODEL( "models/hlclassic/v_rpg.mdl" );
		PRECACHE_MODEL( "models/hlclassic/p_rpg.mdl" );
	}
	else
	{
	PRECACHE_MODEL("models/w_rpg.mdl");
	PRECACHE_MODEL("models/v_rpg.mdl");
	PRECACHE_MODEL("models/p_rpg.mdl");
		PRECACHE_MODEL( "models/p_rpg_r.mdl" );
	}

	PRECACHE_SOUND( "hlclassic/items/9mmclip1.wav" );
	PRECACHE_SOUND( "hlclassic/weapons/rocketfire1.wav" );
	PRECACHE_SOUND( "hlclassic/weapons/glauncher.wav" );
	PRECACHE_SOUND("items/9mmclip1.wav" );

	PRECACHE_SOUND("weapons/rocketfire1.wav");
	PRECACHE_SOUND("weapons/glauncher.wav"); // alternative fire sound

	UTIL_PrecacheOther( "rpg_laser_spot" );
	UTIL_PrecacheOther( "rpg_rocket" );
}

void CRpg::Reload( void )
{
	if ( m_cActiveRockets > 0 && m_fSpotActive )
		return;

	if ( m_iClip != 0 )
		return;

	if ( !DefaultReload( RPG_MAX_CLIP, RPG_RELOAD, 2.0 ) )
		return;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_hPlayer->random_seed, 10, 15 );

	if ( !CClassicMode::GetInstance()->IsEnabled() )
		m_hPlayer->pev->weaponmodel = MAKE_STRING( GetP_Model( "models/p_rpg_r.mdl" ) );

	if ( m_fSpotActive )
	{
		if ( m_hSpot )
			m_hSpot->Suspend( 2.1 );

		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 2.1;
	}

	CBasePlayerWeapon::Reload();
}

bool CRpg::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "rockets";
	p->iMaxAmmo1 = ROCKET_MAX_CARRY;
	p->pszAmmo1Entity = "ammo_rpgclip";
	p->iAmmo1Drop = AMMO_RPGCLIP_GIVE;
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
		return TRUE;
	}
	return FALSE;
}

bool CRpg::Deploy( void )
{
	int iAnim;

	if ( m_iClip )
		iAnim = RPG_DRAW1;
	else
		iAnim = RPG_DRAW_UL;

	if ( CClassicMode::GetInstance()->IsEnabled() )
		return DefaultDeploy( GetV_Model( "models/hlclassic/v_rpg.mdl" ),
			GetP_Model( "models/hlclassic/p_rpg.mdl" ), iAnim, "rpg" );

	return DefaultDeploy( GetV_Model( "models/v_rpg.mdl" ),
		GetP_Model( "models/p_rpg.mdl" ), iAnim, "rpg" );
	}

bool CRpg::CanHolster( void )
{
	// can't put away while guiding a missile.
	return !m_fSpotActive || !m_cActiveRockets;
}

void CRpg::Holster( int skiplocal /* = 0 */ )
{
	m_fInReload = FALSE;// cancel any reload in progress.

	m_hPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0;

	if ( m_hSpot )
	{
		m_hSpot->Killed( NULL, GIB_NEVER );
		m_hSpot = NULL;
	}

	m_hPlayer->pev->viewmodel = 0;
}

void CRpg::PrimaryAttack( void )
{
	if ( !m_iClip )
	{
		PlayEmptySound();

		if ( UTIL_WeaponTimeBase() + 0.5 > m_flNextPrimaryAttack )
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;

		UpdateSpot();
		return;
	}

	m_hPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_hPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	// player "shoot" animation
	m_hPlayer->SetAnimation( PLAYER_ATTACK1 );

	SendWeaponAnim( RPG_FIRE2 );

		UTIL_MakeVectors( m_hPlayer->pev->v_angle );
		Vector vecSrc = m_hPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 - gpGlobals->v_up * 8;

		CRpgRocket *pRocket = CRpgRocket::CreateRpgRocket( vecSrc, m_hPlayer->pev->v_angle, m_hPlayer, this );

		UTIL_MakeVectors( m_hPlayer->pev->v_angle );// RpgRocket::Create stomps on globals, so remake.
		pRocket->pev->velocity = pRocket->pev->velocity + gpGlobals->v_forward * DotProduct( m_hPlayer->pev->velocity, gpGlobals->v_forward );

	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), g_evWeaponCustom, 0.0, (float *) &g_vecZero, (float *) &g_vecZero,
	                     WEAPONCUSTOM_SND_ROCKETFIRE1, 0.9, WEAPONCUSTOM_SOUND, CHAN_WEAPON, 0, 0 );
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), g_evWeaponCustom, 0.0, (float *) &g_vecZero, (float *) &g_vecZero,
	                     WEAPONCUSTOM_SND_GLAUNCHER, 0.7, WEAPONCUSTOM_SOUND, CHAN_BODY, 0, 0 );

		m_iClip = 0;

		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.5;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.5;
	m_hPlayer->pev->punchangle.x -= 5;
	UpdateSpot( );
}

void CRpg::SecondaryAttack( void )
{
	if ( CVAR_GET_FLOAT( "weaponmode_rpg" ) != 0.0f )
		return;

	m_fSpotActive = ! m_fSpotActive;

	if (!m_fSpotActive && m_hSpot )
	{
		m_hSpot->Killed( NULL, GIB_NORMAL );
		m_hSpot = NULL;
	}

	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.2;
}

void CRpg::WeaponIdle( void )
{
	if ( m_hPlayer && !(CBaseEntity *) m_hPlayer->m_hTank
		&& !CClassicMode::GetInstance()->IsEnabled() )
		m_hPlayer->pev->weaponmodel = MAKE_STRING( GetP_Model( "models/p_rpg.mdl" ) );

	UpdateSpot( );

	ResetEmptySound( );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( !m_hPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1;
		return;
	}

	int iAnim;
		float flRand = UTIL_SharedRandomFloat( m_hPlayer->random_seed, 0, 1 );
		if (flRand <= 0.75 || m_fSpotActive)
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

void CRpg::UpdateSpot( void )
{
	if ( !m_fSpotActive )
		return;

	if (!m_hSpot )
	{
		m_hSpot = CRPGSpot::CreateSpot();
		m_hSpot->pev->owner = m_hPlayer->edict();
		}

		UTIL_MakeVectors( m_hPlayer->pev->v_angle );
		Vector vecSrc = m_hPlayer->GetGunPosition( );
	TraceResult tr;
		UTIL_TraceLine ( vecSrc, vecSrc + gpGlobals->v_forward * 8192, dont_ignore_monsters, m_hPlayer->edict(), &tr );

		UTIL_SetOrigin( m_hSpot->pev, tr.vecEndPos );
	}

void CRpg::GetWeaponData( weapon_data_t *data )
{
	data->iuser4 = m_fSpotActive;
	data->fuser1 = m_cActiveRockets;
}

void CRpg::SetWeaponData( weapon_data_t *data )
{
	m_fSpotActive = data->iuser4;
	m_cActiveRockets = (int) data->fuser1;
}

class CRpgAmmo : public CPlayerAmmo
{
public:
	CRpgAmmo() : CPlayerAmmo( AMMO_RPGCLIP_GIVE, "rockets", ROCKET_MAX_CARRY ) {}

	const char *GetDefaultModel( void ) { return CClassicMode::GetInstance()->IsEnabled() ? "models/hlclassic/w_rpgammo.mdl" : "models/w_rpgammo.mdl"; }
	const char *GetPickupSound( void ) { return CClassicMode::GetInstance()->IsEnabled() ? "hlclassic/items/9mmclip1.wav" : "items/9mmclip1.wav"; }
};

LINK_ENTITY_TO_CLASS( ammo_rpgclip, CRpgAmmo );

#endif
