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
#include "../client_electro.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"

#include "usercmd.h"
#include "entity_state.h"
#include "demo_api.h"
#include "pm_defs.h"
#include "pm_shared.h"
#include "event_api.h"
#include "event_flags.h"
#include "r_efx.h"
#include "cl_entity.h"

#include "../hud_iface.h"
#include "../com_weapons.h"
#include "../demo.h"

#define MAX_CLIENT_WEAPON_ENTS	256

extern globalvars_t *gpGlobals;
extern int g_iWaterLevel;

// Pool of client side entities. Each one gets an edict so entity handles and edict() work
static edict_t		edicts[ MAX_CLIENT_WEAPON_ENTS ];
static int			num_ents = 0;

// The entity we'll use to represent the local client
static CBasePlayer	player;

// Local version of game .dll global variables ( time, etc. )
static globalvars_t	Globals;

CBasePlayerWeapon *g_pWpns[ MAX_WEAPONS ];

unsigned int g_evWeaponCustom;
unsigned short g_EvCheckClient;

// Weapon placeholder entities
CGlock				g_Glock;
CPython				g_Python;
CDesertEagle		g_DesertEagle;
CUzi				g_Uzi;
CMP5				g_Mp5;
CMinigun			g_Minigun;
CM249				g_M249;
CM16				g_M16;
CSporeLauncher		g_SporeLauncher;
CSniperRifle		g_SniperRifle;
CRpg				g_Rpg;
CCrossbow			g_Crossbow;
CGauss				g_Gauss;
CHgun				g_HGun;
CShotgun			g_Shotgun;
CSatchel			g_Satchel;
CHandGrenade		g_HandGren;
CShockRifle			g_ShockRifle;
CDisplacer			g_Displacer;
CWeaponFlamethrower	g_Flamethrower;


/*
======================
AlertMessage

Print debug messages to console
======================
*/
void AlertMessage( ALERT_TYPE atype, char *szFmt, ... )
{
	va_list		argptr;
	static char	string[1024];

	if (atype != at_notice &&
		(g_pDeveloper->value < 1 || (atype == at_aiconsole && g_pDeveloper->value < 4)))
		return;

	va_start (argptr, szFmt);
	vsprintf (string, szFmt,argptr);
	va_end (argptr);

	gEngfuncs.Con_DPrintf( string );
}

bool CanHaveXtra( int iXtra, CBasePlayer *pPlayer )
{
	return true;
}

/*
=====================
HUD_PrepEntity

Links the raw entity to an edict holder.  If a player is passed in as the owner, then
we set up the m_hPlayer field
=====================
*/
void HUD_PrepEntity( CBaseEntity *pEntity, CBasePlayer *pWeaponOwner )
{
	if ( !pEntity )
	{
		ALERT( at_error, "HUD_PrepEntity: Null entity or entity vars passed.\n" );
		return;
	}

	if ( num_ents >= MAX_CLIENT_WEAPON_ENTS )
	{
		ALERT( at_error, "HUD_PrepEntity: Exceeded maximum number of entities.\n" );
		return;
	}

	edict_t *pEdict = &edicts[ num_ents ];

	memset( pEdict, 0, sizeof( *pEdict ) );
	pEdict->serialnumber = num_ents;
	pEntity->pev = &pEdict->v;
	pEntity->pev->pContainingEntity = pEdict;
	pEdict->pvPrivateData = pEntity;

	pEntity->Precache();
	pEntity->Spawn();

	if ( pWeaponOwner )
	{
		ItemInfo info;

		memset( &info, 0, sizeof( info ) );

		CBasePlayerWeapon *pWeapon = dynamic_cast<CBasePlayerWeapon *>( pEntity );

		if ( pWeapon )
		{
			pWeapon->m_hPlayer = pWeaponOwner;

			if ( pWeapon->GetItemInfo( &info ) )
				CBasePlayerItem::ItemInfoArray[ info.iId ] = info;

		g_pWpns[ info.iId ] = pWeapon;
	}
}

	num_ents++;
}

/*
=====================
CBaseEntity :: Killed

If weapons code "kills" an entity, just set its effects to EF_NODRAW
=====================
*/
void CBaseEntity :: Killed( entvars_t *pevAttacker, int iGib )
{
	pev->effects |= EF_NODRAW;
}

/*
=====================
CBasePlayerWeapon :: DefaultReload
=====================
*/
bool CBasePlayerWeapon :: DefaultReload( int iClipSize, int iAnim, float fDelay, int body, int iClip, int iAmmoType )
{
	if ( !player.m_pActiveItem )
		return false;

	if ( player.m_rgAmmo[ iAmmoType ] <= 0 )
		return false;

	int j = V_min( iClipSize - iClip, player.m_rgAmmo[ iAmmoType ] );

	if ( j == 0 )
		return false;

	m_hPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + fDelay;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3;
	m_fInReload = true;

	SendWeaponAnim( iAnim, 0, body );
	pev->body = body;

	return true;
}

bool CBasePlayerWeapon :: DefaultReload( int iClipSize, int iAnim, float fDelay, int body )
{

	if ( !m_hPlayer->m_pActiveItem )
		return FALSE;

	CBasePlayerWeapon *pActive = g_pWpns[ m_hPlayer->m_pActiveItem->m_iId ];

	return DefaultReload( iClipSize, iAnim, fDelay, body, pActive->m_iClip, pActive->m_iPrimaryAmmoType );
}

/*
=====================
CBasePlayerWeapon :: ReloadClip

Moves ammo from the player's inventory into the given clip
=====================
*/
void CBasePlayerWeapon :: ReloadClip( int iClipSize, int &iClip, int iAmmoType )
{
	int j = V_min( iClipSize - iClip, m_hPlayer->m_rgAmmo[ iAmmoType ] );

	iClip += j;
	m_hPlayer->m_rgAmmo[ iAmmoType ] -= j;
}

/*
=====================
CBasePlayerItem :: ObjectCaps
=====================
*/
int CBasePlayerItem :: ObjectCaps( void )
{
	// hidden while carried
	if ( m_hPlayer && ( pev->effects & EF_NODRAW ) )
		return FCAP_ACROSS_TRANSITION;

	if ( pev->spawnflags & SF_ITEM_TOUCH_ONLY )
		return FCAP_ACROSS_TRANSITION;

	return FCAP_ACROSS_TRANSITION | FCAP_IMPULSE_USE;
}

/*
=====================
CBasePlayerWeapon :: CanDeploy
=====================
*/
bool CBasePlayerWeapon :: CanDeploy( void )
{
	bool bHasAmmo = false;

	if ( !pszAmmo1() )
	{
		// this weapon doesn't use ammo, can always deploy.
		return TRUE;
	}

	bHasAmmo |= (m_hPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0);

	if ( pszAmmo2() )
		bHasAmmo |= (m_hPlayer->m_rgAmmo[m_iSecondaryAmmoType] != 0);

	if (m_iClip > 0)
		bHasAmmo = true;

	if ( m_iClip2 > 0 )
		bHasAmmo = true;

	return bHasAmmo;
}

/*
=====================
CBasePlayerWeapon :: DefaultDeploy

=====================
*/
bool CBasePlayerWeapon :: DefaultDeploy(const char *szViewModel, const char *szWeaponModel, int iAnim, const char *szAnimExt, int skiplocal, int	body )
{
	if ( !CanDeploy() )
		return FALSE;

	gEngfuncs.CL_LoadModel( szViewModel, &m_hPlayer->pev->viewmodel );

	SendWeaponAnim( iAnim, skiplocal, body );

	m_hPlayer->m_flNextAttack = 0.5;
	m_flTimeWeaponIdle = 1.0;
	return TRUE;
}

/*
=====================
CBasePlayerWeapon :: PlayEmptySound

=====================
*/
bool CBasePlayerWeapon :: PlayEmptySound( void )
{
	if ( m_bPlayEmptySound )
	{
		HUD_PlaySound( "weapons/357_cock1.wav", 0.8 );
		m_bPlayEmptySound = false;
	}
	return false;
}

/*
=====================
CBasePlayerWeapon :: ResetEmptySound

=====================
*/
void CBasePlayerWeapon :: ResetEmptySound( void )
{
	m_bPlayEmptySound = false;
}

/*
=====================
CBasePlayerWeapon::Holster

Put away weapon
=====================
*/
void CBasePlayerWeapon::Holster( int skiplocal /* = 0 */ )
{
	m_fInReload = FALSE; // cancel any reload in progress.
	m_hPlayer->pev->viewmodel = 0;
}

/*
=====================
CBasePlayerWeapon::SendWeaponAnim

Animate weapon model
=====================
*/
void CBasePlayerWeapon::SendWeaponAnim( int iAnim, int skiplocal, int body )
{
	m_hPlayer->pev->weaponanim = iAnim;

	HUD_SendWeaponAnim( iAnim, body, 0 );
}

/*
=====================
CBasePlayerWeapon::BulletAccuracy

Picks the spread for the player's current speed and stance
=====================
*/
Vector CBasePlayerWeapon::BulletAccuracy( const Vector &vecMoving, const Vector &vecStanding, const Vector &vecCrouched )
{
	Vector vecSpread = vecMoving;

	if ( m_hPlayer->pev->velocity.Length() != 0 )
		return vecSpread;

	vecSpread = vecStanding;

	if ( m_hPlayer->pev->flags & FL_DUCKING )
		vecSpread = vecCrouched;

	return vecSpread;
}

/*
=====================
CBasePlayerWeapon::ItemPostFrame

Handles weapon firing, reloading, etc.
=====================
*/
void CBasePlayerWeapon::ItemPostFrame( void )
{
	if ( pev->fuser2 > 0 && UTIL_WeaponTimeBase() >= m_flNextSecondaryAttack )
	{
		BurstSupplement();
		return;
	}

	FinishReload();

	if ((m_hPlayer->pev->button & IN_ATTACK2) && (m_flNextSecondaryAttack <= 0.0))
	{
		if ( pszAmmo2() && !m_hPlayer->m_rgAmmo[SecondaryAmmoIndex()] )
			m_bFireOnEmpty = TRUE;

		SecondaryAttack();
		m_hPlayer->pev->button &= ~IN_ATTACK2;
	}
	else if ((m_hPlayer->pev->button & IN_ATTACK) && (m_flNextPrimaryAttack <= 0.0))
	{
		if ( (m_iClip == 0 && pszAmmo1()) || (iMaxClip() == WEAPON_NOCLIP && !m_hPlayer->m_rgAmmo[PrimaryAmmoIndex()] ) )
			m_bFireOnEmpty = TRUE;

		PrimaryAttack();
	}
	else if ( ( m_hPlayer->pev->button & IN_ALT1 ) && ( m_flNextTertiaryAttack <= 0.0 ) )
	{
		TertiaryAttack();
		m_hPlayer->pev->button &= ~IN_ALT1;
	}
	else if ( ( m_hPlayer->pev->button & IN_RELOAD ) && iMaxClip() != WEAPON_NOCLIP && !m_fInReload )
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.
		Reload();
	}
	else if ( !(m_hPlayer->pev->button & (IN_ATTACK|IN_ATTACK2 | IN_ALT1 ) ) )
	{
		// no fire buttons down
		m_bFireOnEmpty = FALSE;

		// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
		if ( ShouldReload() )
			Reload();
		else
			WeaponIdle( );
		return;
	}

	// catch all
	if ( ShouldWeaponIdle() )
		WeaponIdle();
	}

/*
=====================
CBasePlayerWeapon::FinishReload

Fills the clip once the reload delay has passed
=====================
*/
void CBasePlayerWeapon::FinishReload( void )
{
	if ( !m_fInReload )
		return;

	m_hPlayer->SetAnimation( PLAYER_RELOAD );

	if ( m_hPlayer->m_flNextAttack <= 0.0 )
	{
		// complete the reload.
		int j = V_min( iMaxClip() - m_iClip, player.m_rgAmmo[ m_iPrimaryAmmoType ] );

		// Add them to the clip
		m_iClip += j;

		if ( player.m_pActiveItem )
			player.m_rgAmmo[ g_pWpns[ player.m_pActiveItem->m_iId ]->m_iPrimaryAmmoType ] -= j;

		m_fInReload = false;
	}
}

/*
=====================
CBasePlayerWeapon::ShouldReload

Whether this weapon should reload automatically
=====================
*/
bool CBasePlayerWeapon::ShouldReload( void )
{
	return m_iClip == 0 && !( iFlags() & ITEM_FLAG_NOAUTORELOAD ) && m_flNextPrimaryAttack < 0.0;
}

/*
=====================
CBasePlayerWeapon::BurstSupplement

Fires the remaining rounds of a burst
=====================
*/
void CBasePlayerWeapon::BurstSupplement( void )
{
	m_hPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_hPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_hPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	Vector vecSrc = m_hPlayer->GetGunPosition();
	Vector vecAiming = m_hPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	Vector vecSpread = BulletAccuracy( VECTOR_CONE_4DEGREES, VECTOR_CONE_3DEGREES, VECTOR_CONE_2DEGREES );

	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), m_usBurstFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, m_iClip, 0, 0, 0 );

	m_hPlayer->FireBullets( 1, vecSrc, vecAiming, vecSpread, 8192, BULLET_PLAYER_SAW, 2, (int)m_flCustomDmg, NULL, FBDM_DRAW );

	m_flNextSecondaryAttack = m_flNextSecondaryAttack + 0.08;

	if ( UTIL_WeaponTimeBase() > m_flNextSecondaryAttack )
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.08;

	pev->fuser2 = pev->fuser2 - 1;
	m_iClip--;
}

/*
=====================
CBasePlayerWeapon::PrintState

=====================
*/
void CBasePlayerWeapon::PrintState( void )
{
	COM_Log( "hl.log", "%.4f ", gpGlobals->time );
	COM_Log( "hl.log", "%.4f ", m_hPlayer->m_flNextAttack );
	COM_Log( "hl.log", "%.4f ", m_flNextPrimaryAttack );
	COM_Log( "hl.log", "%.4f ", m_flTimeWeaponIdle - gpGlobals->time );
	COM_Log( "hl.log", "%i ", m_iClip );
}

bool CBasePlayerWeapon::IsMultiplayer( void )
{
	return gEngfuncs.GetMaxClients() > 1;
}

bool CBasePlayerWeapon::FRunfuncs( void )
{
	return g_runfuncs;
}

void CBasePlayerWeapon::SetFOV( int fov )
{
	SetGlobalFOV( fov );
}

/*
=====================
CBasePlayerWeapon::FCanRun

Asks the event system whether client side events run at all
=====================
*/
bool CBasePlayerWeapon::FCanRun( void )
{
	g_fEventRunned = false;

	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), g_EvCheckClient, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0, 0, m_iClip == 0, 0 );

	return g_fEventRunned;
}

/*
=====================
CBasePlayerWeapon::SetV_Model

=====================
*/
void CBasePlayerWeapon::SetV_Model( const char *szViewModel )
{
	int iModelIndex;

	if ( gEngfuncs.CL_LoadModel( szViewModel, &iModelIndex ) )
	{
		m_hPlayer->pev->viewmodel = iModelIndex;
	}
	else if ( strcmp( szViewModel, "models/v_satchel.mdl" ) && strcmp( szViewModel, "models/v_satchel_radio.mdl" ) )
	{
		ALERT( at_console, "ChangeV_Model: could not load %s model\n", szViewModel );
	}
}

void CBasePlayerWeapon::ChangeWeaponSkin( short skin )
{
	cl_entity_t *pViewModel = gEngfuncs.GetViewModel();

	if ( pViewModel )
		pViewModel->curstate.skin = skin;
}

CPlayerAmmo::CPlayerAmmo( int iAmmoGive, const char *pszAmmoName, int iMaxCarry )
{
	m_iAmmoGive = iAmmoGive;
	m_pszAmmoName = pszAmmoName;
	m_iMaxCarry = iMaxCarry;
}

void CPlayerAmmo::Spawn( void )
{
	Precache();

	if ( FStringNull( pev->model ) )
		SET_MODEL( ENT( pev ), GetDefaultModel() );
	else
		SET_MODEL( ENT( pev ), STRING( pev->model ) );

	CBasePlayerAmmo::Spawn();
}

void CPlayerAmmo::Precache( void )
{
	CBasePlayerAmmo::Precache();

	PRECACHE_MODEL( (char *)( pev->model ? STRING( pev->model ) : GetDefaultModel() ) );
	PRECACHE_SOUND( (char *)GetPickupSound() );
}

bool CPlayerAmmo::AddAmmo( CBaseEntity *pOther )
{
	if ( pOther->GiveAmmo( m_iAmmoGive, (char *)m_pszAmmoName, m_iMaxCarry ) == -1 )
		return false;

	EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, GetPickupSound(), 1.0, ATTN_NORM, 0, PITCH_NORM );
	return true;
}

const char *CPlayerAmmo::GetPickupSound( void )
{
	return "items/9mmclip1.wav";
}

/*
=====================
CBasePlayer::SelectItem

  Switch weapons
=====================
*/
void CBasePlayer::SelectItem(const char *pstr)
{
}

void CBasePlayer::DeployWeapon( void )
{
	if ( m_pActiveItem )
		m_pActiveItem->Deploy();
}

void CBasePlayer::HolsterWeapon( void )
{
	if ( m_pActiveItem )
		m_pActiveItem->Holster();
}

/*
=====================
CBasePlayer::SelectLastItem

=====================
*/
void CBasePlayer::SelectLastItem(void)
{
	if (!m_pLastItem || !m_pActiveItem || !m_pActiveItem->CanHolster() )
		return;

	if ( !m_pLastItem->m_hPlayer || !m_pActiveItem->m_hPlayer )
		return;

	m_pActiveItem->Holster( );

	CBasePlayerItem *pTemp = m_pActiveItem;
	m_pActiveItem = m_pLastItem;
	m_pLastItem = pTemp;
	m_pActiveItem->Deploy( );
}

/*
=====================
CBasePlayer::Killed

=====================
*/
void CBasePlayer::Killed( entvars_t *pevAttacker, int iGib )
{
	// Holster weapon immediately, to allow it to cleanup
	if ( m_pActiveItem )
		 m_pActiveItem->Holster( );
}

/*
=====================
CBasePlayer::Spawn

=====================
*/
void CBasePlayer::Spawn( void )
{
	if ( m_pActiveItem && pev->viewmodel )
		m_pActiveItem->Deploy( );
}

/*
=====================
UTIL_TraceLine

Don't actually trace, but act like the trace didn't hit anything.
=====================
*/
void UTIL_TraceLine( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, edict_t *pentIgnore, TraceResult *ptr )
{
	memset( ptr, 0, sizeof( *ptr ) );
	ptr->flFraction = 1.0;
}

/*
=====================
UTIL_ParticleBox

For debugging, draw a box around a player made out of particles
=====================
*/
void UTIL_ParticleBox( CBasePlayer *player, float *mins, float *maxs, float life, unsigned char r, unsigned char g, unsigned char b )
{
	int i;
	vec3_t mmin, mmax;

	for ( i = 0; i < 3; i++ )
	{
		mmin[ i ] = player->pev->origin[ i ] + mins[ i ];
		mmax[ i ] = player->pev->origin[ i ] + maxs[ i ];
	}

	gEngfuncs.pEfxAPI->R_ParticleBox( (float *)&mmin, (float *)&mmax, 5.0, 0, 255, 0 );
}

/*
=====================
UTIL_ParticleBoxes

For debugging, draw boxes for other collidable players
=====================
*/
void UTIL_ParticleBoxes( void )
{
	int idx;
	physent_t *pe;
	cl_entity_t *player;
	vec3_t mins, maxs;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	player = gEngfuncs.GetLocalPlayer();
	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers ( player->index - 1 );

	for ( idx = 1; idx < 100; idx++ )
	{
		pe = gEngfuncs.pEventAPI->EV_GetPhysent( idx );
		if ( !pe )
			break;

		if ( pe->info >= 1 && pe->info <= gEngfuncs.GetMaxClients() )
		{
			mins = pe->origin + pe->mins;
			maxs = pe->origin + pe->maxs;

			gEngfuncs.pEfxAPI->R_ParticleBox( (float *)&mins, (float *)&maxs, 0, 0, 255, 2.0 );
		}
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();
}

/*
=====================
UTIL_ParticleLine

For debugging, draw a line made out of particles
=====================
*/
void UTIL_ParticleLine( CBasePlayer *player, float *start, float *end, float life, unsigned char r, unsigned char g, unsigned char b )
{
	gEngfuncs.pEfxAPI->R_ParticleLine( start, end, r, g, b, life );
}

/*
=====================
HUD_InitClientWeapons

Set up weapons, player and functions needed to run weapons code client-side.
=====================
*/
void EV_PrecacheClientsideEvents( void )
{
	g_evWeaponCustom = PRECACHE_EVENT( 1, "events/weapon_custom.sc" );
}

void HUD_InitClientWeapons( void )
{
	static bool initialized = false;
	if ( initialized )
		return;

	initialized = true;

	// Set up pointer ( dummy object )
	gpGlobals = &Globals;

	// Fill in current time ( probably not needed )
	gpGlobals->time = gEngfuncs.GetClientTime();

	// Fake functions
	g_engfuncs.pfnPrecacheEvent		= gEngfuncs.pfnPrecacheEvent;
	g_engfuncs.pfnRandomFloat		= gEngfuncs.pfnRandomFloat;

	// Fake functions
	g_engfuncs.pfnPrecacheModel		= stub_PrecacheModel;
	g_engfuncs.pfnPrecacheSound		= stub_PrecacheSound;
	g_engfuncs.pfnNameForFunction	= stub_NameForFunction;
	g_engfuncs.pfnSetModel			= stub_SetModel;
	g_engfuncs.pfnSetClientMaxspeed = HUD_SetMaxSpeed;

	// Handled locally
	g_engfuncs.pfnPlaybackEvent		= HUD_PlaybackEvent;
	g_engfuncs.pfnAlertMessage		= AlertMessage;

	// Pass through to engine
	g_engfuncs.pfnRandomLong		= gEngfuncs.pfnRandomLong;

	// Allocate a slot for the local player
	HUD_PrepEntity( &player		, NULL );

	// Allocate slot(s) for each weapon that we are going to be predicting
	HUD_PrepEntity( &g_Glock	, &player );
	HUD_PrepEntity( &g_Python, &player );
	HUD_PrepEntity( &g_DesertEagle, &player );
	HUD_PrepEntity( &g_Uzi, &player );
	HUD_PrepEntity( &g_Mp5	, &player );
	HUD_PrepEntity( &g_Minigun, &player );
	HUD_PrepEntity( &g_M249, &player );
	HUD_PrepEntity( &g_M16, &player );
	HUD_PrepEntity( &g_SporeLauncher, &player );
	HUD_PrepEntity( &g_SniperRifle, &player );
	HUD_PrepEntity( &g_Rpg, &player );
	HUD_PrepEntity( &g_Crossbow, &player );
	HUD_PrepEntity( &g_Gauss, &player );
	HUD_PrepEntity( &g_HGun, &player );
	HUD_PrepEntity( &g_Shotgun	, &player );
	HUD_PrepEntity( &g_Satchel, &player );
	HUD_PrepEntity( &g_HandGren	, &player );
	HUD_PrepEntity( &g_ShockRifle, &player );
	HUD_PrepEntity( &g_Displacer, &player );
	HUD_PrepEntity( &g_Flamethrower, &player );

	EV_PrecacheClientsideEvents();
}

/*
=====================
HUD_WeaponsPostThink

Run Weapon firing code on client
=====================
*/
void HUD_WeaponsPostThink( local_state_s *from, local_state_s *to, usercmd_t *cmd, double time, unsigned int random_seed )
{
	int i;
	int buttonsChanged;
	CBasePlayerWeapon *pWeapon = NULL;
	CBasePlayerWeapon *pCurrent;
	weapon_data_t *pfrom, *pto;
	static int lasthealth;

	HUD_InitClientWeapons();

	// Get current clock
	gpGlobals->time = time;

	// Fill in data based on selected weapon
	// FIXME, make this a method in each weapon?  where you pass in an entity_state_t *?
	switch ( from->client.m_iId )
	{
		case WEAPON_GLOCK:
			pWeapon = &g_Glock;
			break;

		case WEAPON_PYTHON:
			pWeapon = &g_Python;
			break;

		case WEAPON_MP5:
			pWeapon = &g_Mp5;
			break;

		case WEAPON_CROSSBOW:
			pWeapon = &g_Crossbow;
			break;

		case WEAPON_SHOTGUN:
			pWeapon = &g_Shotgun;
			break;

		case WEAPON_RPG:
			pWeapon = &g_Rpg;
			break;

		case WEAPON_GAUSS:
			pWeapon = &g_Gauss;
			break;

		case WEAPON_HORNETGUN:
			pWeapon = &g_HGun;
			break;

		case WEAPON_HANDGRENADE:
			pWeapon = &g_HandGren;
			break;

		case WEAPON_SATCHEL:
			pWeapon = &g_Satchel;
			break;

		case WEAPON_UZI:
			pWeapon = &g_Uzi;
			break;

		case WEAPON_MINIGUN:
			pWeapon = &g_Minigun;
			break;

		case WEAPON_SNIPERRIFLE:
			pWeapon = &g_SniperRifle;
			break;

		case WEAPON_M249:
			pWeapon = &g_M249;
			break;

		case WEAPON_M16:
			pWeapon = &g_M16;
			break;

		case WEAPON_SPORELAUNCHER:
			pWeapon = &g_SporeLauncher;
			break;

		case WEAPON_EAGLE:
			pWeapon = &g_DesertEagle;
			break;

		case WEAPON_SHOCKRIFLE:
			pWeapon = &g_ShockRifle;
			break;

		case WEAPON_DISPLACER:
			pWeapon = &g_Displacer;
			break;
	}

	// We are not predicting the current weapon, just bow out here.
	if ( !pWeapon )
		return;

	for ( i = 0; i < MAX_WEAPONS; i++ )
	{
		pCurrent = g_pWpns[ i ];
		if ( !pCurrent )
			continue;

		pfrom = &from->weapondata[ i ];

		pCurrent->m_fInReload			= pfrom->m_fInReload != 0;
//		pCurrent->m_flPumpTime			= pfrom->m_flPumpTime;
		pCurrent->m_iClip				= pfrom->m_iClip;
		pCurrent->m_flNextPrimaryAttack	= pfrom->m_flNextPrimaryAttack;
		pCurrent->m_flNextSecondaryAttack = pfrom->m_flNextSecondaryAttack;
		pCurrent->m_flTimeWeaponIdle	= pfrom->m_flTimeWeaponIdle;
		pCurrent->m_fInZoom					= pfrom->m_fInZoom != 0;
		pCurrent->SetWeaponData( pfrom );

		if ( pCurrent->m_iId == WEAPON_UZI && from->client.m_iId == WEAPON_UZI )
			( (CUzi *)pCurrent )->CheckGilded( g_fAllowGilded );
	}

	// For random weapon events, use this seed to seed random # generator
	player.random_seed = random_seed;

	// Get old buttons from previous state.
	player.m_afButtonLast = from->playerstate.oldbuttons;

	// Which buttsons chave changed
	buttonsChanged = (player.m_afButtonLast ^ cmd->buttons);	// These buttons have changed this frame

	// Debounced button codes for pressed/released
	// The changed ones still down are "pressed"
	player.m_afButtonPressed =  buttonsChanged & cmd->buttons;
	// The ones not down are "released"
	player.m_afButtonReleased = buttonsChanged & (~cmd->buttons);

	// Set player variables that weapons code might check/alter
	player.pev->button = cmd->buttons;

	player.pev->velocity = from->client.velocity;
	player.pev->flags = from->client.flags;

	player.pev->deadflag = from->client.deadflag;
	player.pev->waterlevel = from->client.waterlevel;
	g_iWaterLevel = player.pev->waterlevel;
	player.pev->maxspeed    = from->client.maxspeed;
	player.pev->fov = from->client.fov;
	player.pev->weaponanim = from->client.weaponanim;
	player.pev->viewmodel = from->client.viewmodel;
	player.m_flNextAttack = from->client.m_flNextAttack;
	player.pev->fuser4 = from->client.fuser4;


	// Point to current weapon object
	if ( from->client.m_iId )
	{
		player.m_pActiveItem = g_pWpns[ from->client.m_iId ];
	}

	// Store pointer to our destination entity_state_t so we can get our origin, etc. from it
	//  for setting up events on the client
	g_finalstate = to;

	// Don't go firing anything if we have died or are spectating
	// Or if we don't have a weapon model deployed
	if ( player.pev->deadflag != ( DEAD_DISCARDBODY + 1 ) )
	{
		if ( !CL_IsDead() && player.pev->viewmodel && player.m_flNextAttack <= 0 )
		{
			pWeapon->ItemPostFrame();
		}
	}

	// If we are running events/etc. go ahead and see if we
	//  managed to die between last frame and this one
	// If so, run the appropriate player killed or spawn function
	if ( g_runfuncs )
	{
		if ( to->client.health <= 0 && lasthealth > 0 )
		{
			player.Killed( NULL, 0 );
		}
		else if ( to->client.health > 0 && lasthealth <= 0 )
		{
			player.Spawn();
		}

		lasthealth = to->client.health;
	}

	// Assume that we are not going to switch weapons
	to->client.m_iId					= from->client.m_iId;

	// Now see if we issued a changeweapon command ( and we're not dead )
	if ( cmd->weaponselect && player.pev->viewmodel
	     && ( player.pev->deadflag != ( DEAD_DISCARDBODY + 1 ) ) )
	{
		// Switched to a different weapon?
		if ( from->weapondata[ cmd->weaponselect ].m_iId == cmd->weaponselect )
		{
			CBasePlayerWeapon *pNew = g_pWpns[ cmd->weaponselect ];
			if ( pNew && ( pNew->m_iId != pWeapon->m_iId ) )
			{
				// Put away old weapon
				if (player.m_pActiveItem)
					player.m_pActiveItem->Holster( );

				player.m_pLastItem = player.m_pActiveItem;
				player.m_pActiveItem = pNew;

				// Deploy new weapon
				if (player.m_pActiveItem)
				{
					player.m_pActiveItem->Deploy( );
				}

				// Update weapon id so we can predict things correctly.
				to->client.m_iId = cmd->weaponselect;
				cmd->weaponselect = 0;
			}
		}
	}

	// Copy in results of prediction code
	to->client.viewmodel				= player.pev->viewmodel;
	to->client.fov						= player.pev->fov;
	to->client.weaponanim				= player.pev->weaponanim;
	to->client.m_flNextAttack			= player.m_flNextAttack;
	to->client.maxspeed					= player.pev->maxspeed;

	//HL Weapons
	to->client.fuser4					= from->client.fuser4;

	// client.fuser4 tells the movement code whether the minigun slows the player down
	if ( player.m_pActiveItem )
		to->client.fuser4 = ( player.m_pActiveItem->m_iId == WEAPON_MINIGUN ) ? 1.0 : 0.0;

	// Make sure that weapon animation matches what the game .dll is telling us
	//  over the wire ( fixes some animation glitches )
	if ( g_runfuncs && ( HUD_GetWeaponAnim() != to->client.weaponanim ) )
	{
		// Force a fixed anim down to viewmodel
		HUD_SendWeaponAnim( to->client.weaponanim, HUD_GetWeaponBody(), 1 );
	}

	for ( i = 0; i < MAX_WEAPONS; i++ )
	{
		pCurrent = g_pWpns[ i ];

		pto = &to->weapondata[ i ];

		if ( !pCurrent )
			continue;

		pto->m_fInReload				= pCurrent->m_fInReload;
//		pto->m_flPumpTime				= pCurrent->m_flPumpTime;
		pto->m_iClip					= pCurrent->m_iClip;
		pto->m_flNextPrimaryAttack		= pCurrent->m_flNextPrimaryAttack;
		pto->m_flNextSecondaryAttack	= pCurrent->m_flNextSecondaryAttack;
		pto->m_flTimeWeaponIdle			= pCurrent->m_flTimeWeaponIdle;

		// The M16 keeps its burst state in pev
		if ( pCurrent->m_iId == WEAPON_M16 && from->client.m_iId == WEAPON_M16 )
		{
			pto->fuser2						= pCurrent->pev->fuser2;
		pto->fuser3						= pCurrent->pev->fuser3;
		}

		// Decrement weapon counters, server does this at same time ( during post think, after doing everything else )
		pto->m_flNextReload				-= cmd->msec / 1000.0;
		pto->m_fNextAimBonus			-= cmd->msec / 1000.0;
		pto->m_flNextPrimaryAttack		-= cmd->msec / 1000.0;
		pto->m_flNextSecondaryAttack	-= cmd->msec / 1000.0;
		pto->m_flTimeWeaponIdle			-= cmd->msec / 1000.0;

		pCurrent->CustomDecrement( cmd->msec / 1000.0 );

		if ( pto->m_flPumpTime != -9999 )
		{
			pto->m_flPumpTime -= cmd->msec / 1000.0;
			if ( pto->m_flPumpTime < -0.001 )
				pto->m_flPumpTime = -0.001;
		}

		if ( pto->m_fNextAimBonus < -1.0 )
		{
			pto->m_fNextAimBonus = -1.0;
		}

		if ( pto->m_flNextPrimaryAttack < -1.0 )
		{
			pto->m_flNextPrimaryAttack = -1.0;
		}

		if ( pto->m_flNextSecondaryAttack < -0.001 )
		{
			pto->m_flNextSecondaryAttack = -0.001;
		}

		if ( pto->m_flTimeWeaponIdle < -0.001 )
		{
			pto->m_flTimeWeaponIdle = -0.001;
		}

		if ( pto->m_flNextReload < -0.001 )
		{
			pto->m_flNextReload = -0.001;
		}
	}

	// m_flNextAttack is now part of the weapons, but is part of the player instead
	to->client.m_flNextAttack -= cmd->msec / 1000.0;
	if ( to->client.m_flNextAttack < -0.001 )
	{
		to->client.m_flNextAttack = -0.001;
	}

	// Wipe it so we can't use it after this frame
	g_finalstate = NULL;
}

/*
=====================
HUD_PostRunCmd

Client calls this during prediction, after it has moved the player and updated any info changed into to->
time is the current client clock based on prediction
cmd is the command that caused the movement, etc
runfuncs is 1 if this is the first time we've predicted this command.  If so, sounds and effects should play, otherwise, they should
be ignored
=====================
*/
void DLLEXPORT HUD_PostRunCmd( local_state_t *from, local_state_t *forward, usercmd_t *cmd, int runfuncs, double time, unsigned int random_seed )
{
//	RecClPostRunCmd(from, to, cmd, runfuncs, time, random_seed);

	g_runfuncs = runfuncs != 0;

#if defined( CLIENT_WEAPONS )
	if ( from->client.iuser1 == OBS_NONE && cl_lw && cl_lw->value )
	{
		switch ( from->client.m_iId )
		{
		case WEAPON_GLOCK:
		case WEAPON_PYTHON:
		case WEAPON_MP5:
		case WEAPON_CROSSBOW:
		case WEAPON_SHOTGUN:
		case WEAPON_RPG:
		case WEAPON_GAUSS:
		case WEAPON_HORNETGUN:
		case WEAPON_HANDGRENADE:
		case WEAPON_SATCHEL:
		case WEAPON_UZI:
		case WEAPON_MINIGUN:
		case WEAPON_SNIPERRIFLE:
		case WEAPON_M249:
		case WEAPON_M16:
		case WEAPON_SPORELAUNCHER:
		case WEAPON_EAGLE:
		case WEAPON_SHOCKRIFLE:
		case WEAPON_DISPLACER:
			HUD_WeaponsPostThink( from, forward, cmd, time, random_seed );
			break;
		}
	}
#endif

	// Observer weapon state comes from the server, including the in-eye viewmodel
	if ( from->client.iuser1 != OBS_NONE && from->client.iuser1 != OBS_IN_EYE )
		forward->client.viewmodel = 0;

	forward->client.fov = g_lastFOV;

	if ( g_runfuncs )
	{
		Game_SetSequence( forward->playerstate.sequence, forward->playerstate.gaitsequence );
		Game_SetOrientation( forward->playerstate.origin, cmd->viewangles );
	}

	// All games can use FOV state
	g_lastFOV = forward->client.fov;

	cl_entity_t *pViewModel = gEngfuncs.GetViewModel();

	if ( !pViewModel )
		return;

	if ( TE_CheckElectroFX( gEngfuncs.GetLocalPlayer()->index ) )
	{
		pViewModel->curstate.renderfx = kRenderFxGlowShell;
		pViewModel->curstate.rendermode = kRenderNormal;
		pViewModel->curstate.renderamt = 0;
		pViewModel->curstate.rendercolor.r = 50;
		pViewModel->curstate.rendercolor.g = 75;
		pViewModel->curstate.rendercolor.b = 220;
	}
	else
	{
		pViewModel->curstate.renderfx = from->playerstate.renderfx;
		pViewModel->curstate.rendermode = from->playerstate.rendermode;
		pViewModel->curstate.renderamt = from->playerstate.renderamt;
		pViewModel->curstate.rendercolor = from->playerstate.rendercolor;
	}
}
