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
/*

===== weapons.cpp ========================================================

  functions governing the selection/use of weapons for players

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "soundent.h"
#include "decals.h"
#include "gamerules.h"
#include "user_messages.h"
#include "server_sound.h"
#include "CASHook.h"
#include "WorldModels.h"
#include "server_support.h"

extern bool PM_HACK_GetBarnacleParalyze( void );

#define SF_ITEM_TOUCH_ONLY		128
#define WEAPON_STATE_AKIMBO	4

extern CGraph	WorldGraph;
extern bool gEvilImpulse101;


#define NOT_USED 255

DLL_GLOBAL	short	g_sModelIndexLaser;// holds the index for the laser beam
DLL_GLOBAL  const char *g_pModelNameLaser = "sprites/laserbeam.spr";
DLL_GLOBAL	short	g_sModelIndexLaserDot;// holds the index for the laser beam dot
DLL_GLOBAL	short	g_sModelIndexFireball;// holds the index for the fireball
DLL_GLOBAL	short	g_sModelIndexSmoke;// holds the index for the smoke cloud
DLL_GLOBAL	short	g_sModelIndexWeaponSmoke;	// weapon smoke sprite index
DLL_GLOBAL	short	g_sModelIndexWExplosion;// holds the index for the underwater explosion
DLL_GLOBAL	short	g_sModelIndexBubbles;// holds the index for the bubbles model
DLL_GLOBAL	short	g_sModelIndexBloodDrop;// holds the sprite index for the initial blood
DLL_GLOBAL	short	g_sModelIndexBloodSpray;// holds the sprite index for splattered blood

ItemInfo CBasePlayerItem::ItemInfoArray[MAX_WEAPONS];
AmmoInfo CBasePlayerItem::AmmoInfoArray[MAX_AMMO_SLOTS];

unsigned int g_evWeaponCustom;
unsigned int g_EvCheckClient;

extern int gmsgCurWeapon;

MULTIDAMAGE gMultiDamage;

#define TRACER_FREQ		4			// Tracers fire every fourth bullet


//=========================================================
// MaxAmmoCarry - pass in a name and this function will tell
// you the maximum amount of that type of ammunition that a 
// player can carry.
//=========================================================
int MaxAmmoCarry( int iszName )
{
	for ( int i = 0;  i < MAX_WEAPONS; i++ )
	{
		if ( CBasePlayerItem::ItemInfoArray[i].pszAmmo1 && !strcmp( STRING(iszName), CBasePlayerItem::ItemInfoArray[i].pszAmmo1 ) )
			return CBasePlayerItem::ItemInfoArray[i].iMaxAmmo1;
		if ( CBasePlayerItem::ItemInfoArray[i].pszAmmo2 && !strcmp( STRING(iszName), CBasePlayerItem::ItemInfoArray[i].pszAmmo2 ) )
			return CBasePlayerItem::ItemInfoArray[i].iMaxAmmo2;
	}

	ALERT( at_console, "MaxAmmoCarry() doesn't recognize '%s'!\n", STRING( iszName ) );
	return -1;
}

	
/*
==============================================================================

MULTI-DAMAGE

Collects multiple small damages into a single damage

==============================================================================
*/

//
// ClearMultiDamage - resets the global multi damage accumulator
//
void ClearMultiDamage(void)
{
	gMultiDamage.pEntity = NULL;
	gMultiDamage.amount	= 0;
	gMultiDamage.type = 0;
}


//
// ApplyMultiDamage - inflicts contents of global multi damage register on gMultiDamage.pEntity
//
// GLOBALS USED:
//		gMultiDamage

void ApplyMultiDamage(entvars_t *pevInflictor, entvars_t *pevAttacker )
{
	Vector		vecSpot1;//where blood comes from
	Vector		vecDir;//direction blood should go
	TraceResult	tr;
	
	if ( !gMultiDamage.pEntity )
		return;

	gMultiDamage.pEntity->TakeDamage(pevInflictor, pevAttacker, gMultiDamage.amount, gMultiDamage.type );
}


// GLOBALS USED:
//		gMultiDamage

void AddMultiDamage( entvars_t *pevInflictor, CBaseEntity *pEntity, float flDamage, int bitsDamageType)
{
	if ( !pEntity )
		return;
	
	gMultiDamage.type |= bitsDamageType;

	if ( pEntity != gMultiDamage.pEntity )
	{
		ApplyMultiDamage(pevInflictor,pevInflictor); // UNDONE: wrong attacker!
		gMultiDamage.pEntity	= pEntity;
		gMultiDamage.amount		= 0;
	}

	gMultiDamage.amount += flDamage;
}

/*
================
SpawnBlood
================
*/
void SpawnBlood(Vector vecSpot, int bloodColor, float flDamage)
{
	if ( flDamage > 0.0f )
		UTIL_BloodDrips( vecSpot, g_vecAttackDir, bloodColor, (int)flDamage );
}

int DamageDecal( CBaseEntity *pEntity, int bitsDamageType )
{
	if ( !pEntity )
		return (DECAL_GUNSHOT1 + RANDOM_LONG(0,4));
	
	return pEntity->DamageDecal( bitsDamageType );
}

void DecalGunshot( TraceResult *pTrace, int iBulletType )
{
	// Is the entity valid
	if ( !UTIL_IsValidEntity( pTrace->pHit ) )
		return;

	if ( VARS(pTrace->pHit)->solid == SOLID_BSP || VARS(pTrace->pHit)->movetype == MOVETYPE_PUSHSTEP )
	{
		CBaseEntity *pEntity = NULL;
		// Decal the wall with a gunshot
		if ( !FNullEnt(pTrace->pHit) )
			pEntity = CBaseEntity::Instance(pTrace->pHit);

		switch( iBulletType )
		{
		case BULLET_PLAYER_9MM:
		case BULLET_MONSTER_9MM:
		case BULLET_PLAYER_MP5:
		case BULLET_MONSTER_MP5:
		case BULLET_PLAYER_BUCKSHOT:
		case BULLET_PLAYER_357:
		default:
			// smoke and decal
			UTIL_GunshotDecalTrace( pTrace, DamageDecal( pEntity, DMG_BULLET ) );
			break;
		case BULLET_MONSTER_12MM:
			// smoke and decal
			UTIL_GunshotDecalTrace( pTrace, DamageDecal( pEntity, DMG_BULLET ) );
			break;
		case BULLET_PLAYER_CROWBAR:
		case DECAL_SCORCH_MARK:
			// wall decal
			UTIL_DecalTrace( pTrace, DamageDecal( pEntity, DMG_CLUB ) );
			break;
		}
	}
}



//
// EjectBrass - tosses a brass shell from passed origin at passed velocity
//
void EjectBrass ( const Vector &vecOrigin, const Vector &vecVelocity, float rotation, int model, int soundtype )
{
	// FIX: when the player shoots, their gun isn't in the same position as it is on the model other players see.

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
		WRITE_BYTE( TE_MODEL);
		WRITE_COORD( vecOrigin.x);
		WRITE_COORD( vecOrigin.y);
		WRITE_COORD( vecOrigin.z);
		WRITE_COORD( vecVelocity.x);
		WRITE_COORD( vecVelocity.y);
		WRITE_COORD( vecVelocity.z);
		WRITE_ANGLE( rotation );
		WRITE_SHORT( model );
		WRITE_BYTE ( soundtype);
		WRITE_BYTE ( 25 );// 2.5 seconds
	MESSAGE_END();
}


#if 0
// UNDONE: This is no longer used?
void ExplodeModel( const Vector &vecOrigin, float speed, int model, int count )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
		WRITE_BYTE ( TE_EXPLODEMODEL );
		WRITE_COORD( vecOrigin.x );
		WRITE_COORD( vecOrigin.y );
		WRITE_COORD( vecOrigin.z );
		WRITE_COORD( speed );
		WRITE_SHORT( model );
		WRITE_SHORT( count );
		WRITE_BYTE ( 15 );// 1.5 seconds
	MESSAGE_END();
}
#endif


int giAmmoIndex = 0;
int g_iARGrenadesHack = 0;

// Precaches the ammo and queues the ammo info for sending to clients
void CBasePlayerItem::AddAmmoNameToAmmoRegistry( const char *name, int maxCarry, const char *weaponName )
{
	if ( !name )
		return;

	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		if ( AmmoInfoArray[i].pszName && !stricmp( AmmoInfoArray[i].pszName, name ) )
			return;
	}

	++giAmmoIndex;

	if ( giAmmoIndex >= MAX_AMMO_SLOTS )
	{
		ALERT( at_warning, "AddAmmoNameToAmmoRegistry: Can't add ammo '%s', maximum number of ammo types have been added! (max: %u)\n",
			name, MAX_AMMO_SLOTS );
		return;
	}

	AmmoInfoArray[giAmmoIndex].pszName = name;
	AmmoInfoArray[giAmmoIndex].iId = giAmmoIndex;
	AmmoInfoArray[giAmmoIndex].iMaxCarry = maxCarry;
	AmmoInfoArray[giAmmoIndex].pszWeapon = weaponName;
}


// Precaches the weapon and queues the weapon info for sending to clients
void UTIL_PrecacheOtherWeapon( const char *szClassname )
{
	edict_t	*pent;

	pent = CREATE_NAMED_ENTITY( MAKE_STRING( szClassname ) );
	if ( FNullEnt( pent ) )
	{
		ALERT ( at_console, "NULL Ent in UTIL_PrecacheOtherWeapon\n" );
		return;
	}
	
	CBaseEntity *pEntity = CBaseEntity::Instance (VARS( pent ));

	if (pEntity)
	{
		CBasePlayerItem *item = pEntity->MyItemPointer();

		if ( item )
		{
			ItemInfo II;
		pEntity->Precache( );
		memset( &II, 0, sizeof II );
		if ( item->GetItemInfo( &II ) )
		{
			CBasePlayerItem::ItemInfoArray[II.iId] = II;

				const char *weaponName;

				if ( II.iFlags & ITEM_FLAG_EXHAUSTIBLE )
					weaponName = STRING( item->pev->classname );
				else
					weaponName = NULL;

				if ( II.pszAmmo1 && *II.pszAmmo1 )
					CBasePlayerItem::AddAmmoNameToAmmoRegistry( II.pszAmmo1, II.iMaxAmmo1, weaponName );

				if ( II.pszAmmo2 && *II.pszAmmo2 )
					CBasePlayerItem::AddAmmoNameToAmmoRegistry( II.pszAmmo2, II.iMaxAmmo2, weaponName );
			}
		}
	}

	REMOVE_ENTITY(pent);
}

void Weapon_Init( void )
{
	memset( CBasePlayerItem::ItemInfoArray, 0, sizeof( CBasePlayerItem::ItemInfoArray ) );
	memset( CBasePlayerItem::AmmoInfoArray, 0, sizeof( CBasePlayerItem::AmmoInfoArray ) );
	giAmmoIndex = 0;
}

void Weapon_Precache( void )
{
	UTIL_PrecacheOther( "item_suit" );
	UTIL_PrecacheOther( "item_healthkit" );
	UTIL_PrecacheOther( "item_battery" );
	UTIL_PrecacheOther( "item_antidote" );
	UTIL_PrecacheOther( "item_security" );
	UTIL_PrecacheOther( "item_longjump" );

	UTIL_PrecacheOtherWeapon( "weapon_crowbar" );
	UTIL_PrecacheOtherWeapon( "weapon_pipewrench" );
	UTIL_PrecacheOtherWeapon( "weapon_medkit" );
	UTIL_PrecacheOtherWeapon( "weapon_grapple" );
	UTIL_PrecacheOtherWeapon( "weapon_9mmhandgun" );
	UTIL_PrecacheOther( "ammo_9mmclip" );
	UTIL_PrecacheOtherWeapon( "weapon_357" );
	UTIL_PrecacheOther( "ammo_357" );
	UTIL_PrecacheOtherWeapon( "weapon_eagle" );
	UTIL_PrecacheOtherWeapon( "weapon_uzi" );
	UTIL_PrecacheOtherWeapon( "weapon_9mmAR" );
	UTIL_PrecacheOther( "ammo_9mmAR" );
	UTIL_PrecacheOther( "ammo_ARgrenades" );
	UTIL_PrecacheOtherWeapon( "weapon_shotgun" );
	UTIL_PrecacheOther( "ammo_buckshot" );
	UTIL_PrecacheOtherWeapon( "weapon_crossbow" );
	UTIL_PrecacheOther( "ammo_crossbow" );
	UTIL_PrecacheOtherWeapon( "weapon_m16" );
	UTIL_PrecacheOtherWeapon( "weapon_rpg" );
	UTIL_PrecacheOther( "ammo_rpgclip" );
	UTIL_PrecacheOtherWeapon( "weapon_gauss" );
	UTIL_PrecacheOther( "ammo_gaussclip" );
	UTIL_PrecacheOtherWeapon( "weapon_egon" );
	UTIL_PrecacheOtherWeapon( "weapon_hornetgun" );
	UTIL_PrecacheOtherWeapon( "weapon_handgrenade" );
	UTIL_PrecacheOtherWeapon( "weapon_satchel" );
	UTIL_PrecacheOtherWeapon( "weapon_tripmine" );
	UTIL_PrecacheOtherWeapon( "weapon_snark" );
	UTIL_PrecacheOtherWeapon( "weapon_sniperrifle" );
	UTIL_PrecacheOtherWeapon( "weapon_m249" );
	UTIL_PrecacheOtherWeapon( "weapon_sporelauncher" );
	UTIL_PrecacheOther( "ammo_sporeclip" );
	UTIL_PrecacheOtherWeapon( "weapon_displacer" );
	UTIL_PrecacheOtherWeapon( "weapon_minigun" );
	UTIL_PrecacheOtherWeapon( "weapon_shockrifle" );
	UTIL_PrecacheOther( "weaponbox" );

	g_sModelIndexFireball = UTIL_PrecacheModel( NULL, "sprites/zerogxplode.spr" );
	g_sModelIndexWExplosion = UTIL_PrecacheModel( NULL, "sprites/WXplo1.spr" );
	g_sModelIndexSmoke = UTIL_PrecacheModel( NULL, "sprites/steam1.spr" );
	g_sModelIndexBubbles = UTIL_PrecacheModel( NULL, "sprites/bubble.spr" );
	g_sModelIndexBloodSpray = UTIL_PrecacheModel( NULL, "sprites/bloodspray.spr" );
	g_sModelIndexBloodDrop = UTIL_PrecacheModel( NULL, "sprites/blood.spr" );
	g_sModelIndexWeaponSmoke = UTIL_PrecacheModel( NULL, "sprites/wep_smoke_01.spr" );
	g_sModelIndexLaser = UTIL_PrecacheModel( NULL, g_pModelNameLaser );
	g_sModelIndexLaserDot = UTIL_PrecacheModel( NULL, "sprites/laserdot.spr" );
	UTIL_PrecacheModel( NULL, "models/grenade.mdl" );
	UTIL_PrecacheModel( NULL, "sprites/explode1.spr" );

	CServer_SoundEngine::GetSoundEngine()->PrecacheSound( "weapons/debris1.wav" );
	CServer_SoundEngine::GetSoundEngine()->PrecacheSound( "weapons/debris2.wav" );
	CServer_SoundEngine::GetSoundEngine()->PrecacheSound( "weapons/debris3.wav" );
	CServer_SoundEngine::GetSoundEngine()->PrecacheSound( "weapons/grenade_hit1.wav" );
	CServer_SoundEngine::GetSoundEngine()->PrecacheSound( "weapons/grenade_hit2.wav" );
	CServer_SoundEngine::GetSoundEngine()->PrecacheSound( "weapons/grenade_hit3.wav" );
	CServer_SoundEngine::GetSoundEngine()->PrecacheSound( "weapons/bullet_hit1.wav" );
	CServer_SoundEngine::GetSoundEngine()->PrecacheSound( "weapons/bullet_hit2.wav" );
	CServer_SoundEngine::GetSoundEngine()->PrecacheSound( "items/weapondrop1.wav" );
	CServer_SoundEngine::GetSoundEngine()->PrecacheSound( "hlclassic/items/weapondrop1.wav" );
	g_evWeaponCustom = PRECACHE_EVENT( 1, "events/weapon_custom.sc" );
}



TYPEDESCRIPTION	CBasePlayerItem::m_SaveData[] = 
{
	{ FIELD_EHANDLE, "m_hPlayer", offsetof( CBasePlayerItem, m_hPlayer ), 1, 0 },
	{ FIELD_EHANDLE, "m_hNextItem", offsetof( CBasePlayerItem, m_pNext ), 1, 0 },
	//DEFINE_FIELD( CBasePlayerItem, m_fKnown, FIELD_INTEGER ),Reset to zero on load
	DEFINE_FIELD( CBasePlayerItem, m_iId, FIELD_INTEGER ),
	// DEFINE_FIELD( CBasePlayerItem, m_iIdPrimary, FIELD_INTEGER ),
	// DEFINE_FIELD( CBasePlayerItem, m_iIdSecondary, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CBasePlayerItem, CBaseAnimating );


TYPEDESCRIPTION	CBasePlayerWeapon::m_SaveData[] =
{
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextPrimaryAttack, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextSecondaryAttack, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextTertiaryAttack, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flTimeWeaponIdle, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iPrimaryAmmoType, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iSecondaryAmmoType, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iClip, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iDefaultAmmo, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iDefaultSecAmmo, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flCustomDmg, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CBasePlayerWeapon, CBasePlayerItem );


void CBasePlayerItem :: SetObjectCollisionBox( void ) {
	pev->absmin = pev->origin + Vector(-24, -24, 0);
	pev->absmax = pev->origin + Vector(24, 24, 16); 
}


//=========================================================
// Sets up movetype, size, solidtype for a new weapon. 
//=========================================================
void CBasePlayerItem :: FallInit( void ) {
	CBasePhysicsObject::FallInit();
	m_flWaterSpeed = 80.0f;
	SetTouch( &CBasePickupObject::DefaultTouch );
	SetThink( &CBasePhysicsObject::FallThink );
}

//=========================================================
// FallThink - Items that have just spawned run this think
// to catch them when they hit the ground. Once we're sure
// that the object is grounded, we change its solid type
// to trigger and set it in a large box that helps the
// player get it.
//=========================================================
void CBasePlayerItem::FallThink ( void ) {
	pev->nextthink = gpGlobals->time + 0.1;

	if ( pev->flags & FL_ONGROUND )
	{
		// clatter if we have an owner (i.e., dropped by someone)
		// don't clatter if the gun is waiting to respawn (if it's waiting, it is invisible!)
		if ( !FNullEnt( pev->owner ) )
		{
			int pitch = 95 + RANDOM_LONG(0,29);
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "items/weapondrop1.wav", 1, ATTN_NORM, 0, pitch);	
		}

		// lie flat
		pev->angles.x = 0;
		pev->angles.z = 0;

		Materialize(); 
	}
}

//=========================================================
// Materialize - make a CBasePlayerItem visible and tangible
//=========================================================
void CBasePlayerItem::Materialize( void ) {
	CBasePickupObject::Materialize();
}

//=========================================================
// AttemptToMaterialize - the item is trying to rematerialize,
// should it do so now or wait longer?
//=========================================================
void CBasePlayerItem::AttemptToMaterialize( void ) {
	float time = g_pGameRules->FlWeaponTryRespawn( this );

	if ( time == 0 )
	{
		Materialize();
		return;
	}

	pev->nextthink = gpGlobals->time + time;
}

//=========================================================
// CheckRespawn - a player is taking this weapon, should 
// it respawn?
//=========================================================
void CBasePlayerItem :: CheckRespawn ( void ) {
	switch ( g_pGameRules->WeaponShouldRespawn( this ) )
	{
	case GR_WEAPON_RESPAWN_YES:
		Respawn();
		break;
	case GR_WEAPON_RESPAWN_NO:
		return;
		break;
	}
}

//=========================================================
// Respawn- this item is already in the world, but it is
// invisible and intangible. Make it visible and tangible.
//=========================================================
CBaseEntity* CBasePlayerItem::Respawn( void ) {
	// make a copy of this weapon that is invisible and inaccessible to players (no touch function). The weapon spawn/respawn code
	// will decide when to make the weapon visible and touchable.
	CBaseEntity *pNewWeapon = CBaseEntity::Create( (char *)STRING( pev->classname ), g_pGameRules->VecWeaponRespawnSpot( this ), pev->angles, true, pev->owner );

	if ( pNewWeapon )
	{
		CBasePlayerItem *pNewItem = (CBasePlayerItem *) pNewWeapon;

		pNewWeapon->pev->targetname = pev->targetname;
		pNewWeapon->pev->target = pev->target;
		pNewWeapon->pev->iuser1 = pev->iuser1;
		pNewWeapon->pev->iuser2 = pev->iuser2;
		pNewWeapon->pev->iuser3 = pev->iuser3;
		pNewWeapon->pev->dmg = pev->dmg;
		pNewWeapon->pev->rendermode = pev->rendermode;
		pNewWeapon->pev->renderfx = pev->renderfx;
		pNewWeapon->pev->renderamt = pev->renderamt;
		pNewWeapon->pev->rendercolor = pev->rendercolor;
		pNewWeapon->pev->colormap = pev->colormap;
		pNewWeapon->pev->skin = pev->skin;
		pNewWeapon->pev->body = pev->body;
		pNewWeapon->pev->scale = pev->scale;
		pNewWeapon->m_iszSoundList = m_iszSoundList;
		pNewWeapon->m_fPlayerAlly = m_fPlayerAlly;
		pNewWeapon->pev->movetype = pev->movetype;

		DispatchSpawn( pNewWeapon->edict() );

		pNewWeapon->pev->effects |= EF_NODRAW;// invisible for now
		pNewWeapon->SetTouch( NULL );// no touch
		pNewWeapon->SetUse( NULL );
		pNewWeapon->SetThink( &CBasePlayerItem::AttemptToMaterialize );

		pNewItem->m_RespawnDelay = m_RespawnDelay;
		pNewItem->m_iszCustomSpriteDir = m_iszCustomSpriteDir;
		pNewItem->m_bIsNotAmmoItem = m_bIsNotAmmoItem;
		pNewItem->m_bExclusiveHold = m_bExclusiveHold;

		DROP_TO_FLOOR ( ENT(pev) );

		// not a typo! We want to know when the weapon the player just picked up should respawn! This new entity we created is the replacement,
		// but when it should respawn is based on conditions belonging to the weapon that was taken.
		pNewWeapon->pev->nextthink = g_pGameRules->FlWeaponRespawnTime( this );
	}
	else
	{
		ALERT ( at_console, "Respawn failed to create %s!\n", STRING( pev->classname ) );
	}

	return pNewWeapon;
}

CBasePlayer *ResolveWeaponPlayer( CEntityHandle<CBasePlayer> &handle )
{
	return handle;
}

CBasePlayerItem *CBasePlayerItem::DropItem( void )
{
	return this;
}

int CBasePlayerWeapon::GetAmmo1Drop( void )
{
	int amount = ItemInfoArray[m_iId].iAmmo1Drop;

	if ( amount < 0 )
		return 0;

	if ( amount == 0 )
	{
		int clipSize = iMaxClip();

		return clipSize == WEAPON_NOCLIP ? 1 : clipSize;
	}

	return amount;
}

int CBasePlayerWeapon::GetAmmo2Drop( void )
{
	int amount = ItemInfoArray[m_iId].iAmmo2Drop;

	if ( amount < 0 )
		return 0;

	return amount == 0 ? 1 : amount;
}

void CBasePlayerWeapon::BurstSupplement( void )
{
	m_hPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_hPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_hPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	Vector vecSrc = m_hPlayer->GetGunPosition();
	Vector vecAiming = m_hPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	Vector vecSpread = BulletAccuracy( VECTOR_CONE_4DEGREES, VECTOR_CONE_3DEGREES, VECTOR_CONE_2DEGREES );

	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), m_usBurstFire, 0.0,
	                     (float *) &g_vecZero, (float *) &g_vecZero, 0.0, 0.0, m_iClip, 0, 0, 0 );

	m_hPlayer->FireBullets( 1, vecSrc, vecAiming, vecSpread, 8192,
	                        BULLET_PLAYER_SAW, 2, (int) m_flCustomDmg, NULL, FBDM_DRAW );

	m_flNextSecondaryAttack += 0.06f;

	if ( UTIL_WeaponTimeBase() > m_flNextSecondaryAttack )
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.06f;

	pev->fuser2 -= 1;
	m_iClip--;
}

Vector CBasePlayerWeapon::BulletAccuracy( const Vector &moving, const Vector &standing, const Vector &crouched )
{
	Vector spread = moving;
	CBasePlayer *player = ResolveWeaponPlayer( m_hPlayer );
	float speedSquared = DotProduct( player->pev->velocity, player->pev->velocity );
	float speed = (float) sqrt( (double) speedSquared );

	if ( speed != 0.0f )
		return spread;

	spread = standing;
	player = ResolveWeaponPlayer( m_hPlayer );

	if ( player->pev->flags & FL_DUCKING )
		spread = crouched;

	return spread;
}

void CBasePlayerWeapon::ReloadClip( int iClipSize, int &iClip, int iAmmoType )
{
	CBasePlayer *player = ResolveWeaponPlayer( m_hPlayer );
	int j = V_min( iClipSize - iClip, player->m_rgAmmo[iAmmoType] );

	iClip += j;
	player->m_rgAmmo[iAmmoType] -= j;
}

void CBasePlayerWeapon::FinishReload( void )
{
	if ( !m_fInReload )
		return;

	CBasePlayer *player = ResolveWeaponPlayer( m_hPlayer );

	if ( player->m_flNextAttack <= UTIL_WeaponTimeBase() )
	{
		ReloadClip( iMaxClip(), m_iClip, m_iPrimaryAmmoType );
		m_fInReload = FALSE;
	}
}

bool CBasePlayerWeapon::ShouldReload( void )
{
	if ( m_iClip != 0 || ( iFlags() & ITEM_FLAG_NOAUTORELOAD ) )
		return false;

	return m_flNextPrimaryAttack < ( UseDecrement() ? 0.0 : gpGlobals->time );
}

void CBasePlayerWeapon::Reload( void )
{
	CBasePlayer *player = ResolveWeaponPlayer( m_hPlayer );

	if ( player->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		return;

	player = ResolveWeaponPlayer( m_hPlayer );

	if ( player )
		player->SetAnimation( PLAYER_RELOAD );

	g_WeaponReloadHook.Call( 0, ResolveWeaponPlayer( m_hPlayer ), this );
}

void CBasePlayerWeapon::AddToPlayerMessage( void )
{
	CBasePlayer *player = ResolveWeaponPlayer( m_hPlayer );

	if ( !player )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, player->pev );
		WRITE_SHORT( m_iId );
	MESSAGE_END();
}

bool CBasePlayerWeapon::IsMultiplayer( void )
{
	return g_pGameRules->IsMultiplayer() != FALSE;
}

bool CBasePlayerWeapon::FRunfuncs( void )
{
	return true;
}

void CBasePlayerWeapon::SetFOV( int fov )
{
	ResolveWeaponPlayer( m_hPlayer )->m_iFOV = fov;
}

bool CBasePlayerWeapon::FCanRun( void )
{
	return true;
}

int CBasePlayerItem::ObjectCaps( void )
{
	if ( !m_hPlayer && !( pev->spawnflags & SF_ITEM_TOUCH_ONLY ) )
		return FCAP_ACROSS_TRANSITION | FCAP_IMPULSE_USE;

	return CBasePickupObject::ObjectCaps();
}

void CBasePlayerItem::UpdateOnRemove( void )
{
	CBaseEntity::UpdateOnRemove();

	if ( m_hPlayer )
		m_hPlayer->RemovePlayerItem( this );
}

void CBasePlayerItem::OnCreate( void )
{
	CBasePickupObject::OnCreate();

	m_bIsBeingRemoved = false;

	m_flLastBounce = 0;

	m_vecMinHullSize = Vector( -0.9f, -0.9f, 0 );
	m_vecMaxHullSize = Vector( 0.9f, 0.9f, 1.9f );
	m_iBounceMaterial = 5;
}

void CBasePlayerWeapon::OnCreate( void )
{
	CBasePlayerItem::OnCreate();
	m_iClip = 0;
	m_iClip2 = -1;

	m_flNextBurstRound = 0;
}

void CBasePlayerItem::Collect( CBaseEntity *other )
{
	Collect( other, COLLECT_TOUCH );
}

void CBasePlayerItem::ItemUse( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	CBaseEntity::Use( activator, caller, useType, value );
}

void CBasePlayerWeapon::KeyValue( KeyValueData *data )
{
	if ( FStrEq( data->szKeyName, "iuser1" ) )
	{
		pev->iuser1 = AllocStringLocal( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "iuser2" ) )
	{
		pev->iuser2 = AllocStringLocal( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "iuser3" ) )
	{
		pev->iuser3 = AllocStringLocal( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "wpn_v_model" ) )
	{
		string_t model = AllocStringLocal( data->szValue );

		if ( model && strlen( STRING( model ) ) > 8 )
			pev->iuser3 = model;

		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "wpn_w_model" ) )
	{
		string_t model = AllocStringLocal( data->szValue );

		if ( model && strlen( STRING( model ) ) > 8 )
			pev->iuser2 = model;

		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "wpn_p_model" ) )
	{
		string_t model = AllocStringLocal( data->szValue );

		if ( model && strlen( STRING( model ) ) > 8 )
			pev->iuser1 = model;

		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "dmg" ) )
	{
		// The parser converts the pooled string index to a float
		m_flCustomDmg = (float) AllocStringLocal( data->szValue );
		data->fHandled = TRUE;
	}
	else
		CBasePlayerItem::KeyValue( data );
}

void CBasePlayerItem::PostSpawn( void )
{
	m_iszWorldModel = pev->model;
}

void CBasePlayerItem::KeyValue( KeyValueData *data )
{
	if ( FStrEq( data->szKeyName, "IsNotAmmoItem" ) )
	{
		m_bIsNotAmmoItem = strtol( data->szValue, NULL, 10 ) != 0;
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "CustomSpriteDir" ) )
	{
		m_iszCustomSpriteDir = AllocStringLocal( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "exclusivehold" ) )
	{
		m_bExclusiveHold = strtol( data->szValue, NULL, 10 ) != 0;
		data->fHandled = TRUE;
	}
	else
		CBasePickupObject::KeyValue( data );
}

bool CBasePlayerItem::CanCollect( CBaseEntity *other, COLLECT_TYPE collectType )
{
	if ( !CBasePickupObject::CanCollect( other, collectType ) || !other->IsPlayer() )
		return false;

	CBasePlayer *player = (CBasePlayer *) other;
	CBaseEntity *owner = CBaseEntity::Instance( pev->owner );

	if ( owner && owner->IsPlayer() && m_iUnknown180 == 2 && m_flCreationTime + 30 > gpGlobals->time && owner != player )
	{
		if ( !( (CBasePlayer *) owner )->IsGibbed() )
			return false;
	}

	if ( m_bExclusiveHold && player->IsExclusiveHoldItemActive() )
		return false;

	if ( !g_pGameRules->CanHavePlayerItem( player, this ) )
	{
		if ( gEvilImpulse101 )
			UTIL_Remove( this );

		return false;
	}

	return true;
}

void CBasePlayerItem::Collect( CBaseEntity *other, COLLECT_TYPE collectType )
{
	if ( !other || !other->IsPlayer() )
		return;

	CBasePlayer *player = (CBasePlayer *) other;
	int result = player->AddPlayerItem( this );

	if ( result < APIR_DuplicateAdd )
		return;

	if ( result == APIR_Added )
	{
		SetTouch( NULL );
		AttachToPlayer( player );
	}

	EMIT_SOUND_DYN( player->edict(), CHAN_ITEM, GetPickupSound(), 1, ATTN_NORM, 0, PITCH_NORM );
	Collected( other, collectType );
}

const char *CBasePlayerItem::GetPickupSound( void )
{
	return "items/gunpickup2.wav";
}

void CBasePlayerItem::UpdateCustomSpriteDir( CBasePlayer *player )
{
	if ( !player )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgCustomWeaponDir, NULL, player->pev );
	WRITE_SHORT( m_iId );
	WRITE_STRING( STRING( m_iszCustomSpriteDir ) );
	MESSAGE_END();
}

void CBasePlayerItem::LoadSprites( CBasePlayer *player, const char *name )
{
	if ( !player )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgWeaponSprites, NULL, player->pev );
	WRITE_SHORT( m_iId );
	WRITE_STRING( name );
	MESSAGE_END();
}

BOOL CanAttack( float attack_time, float curtime, BOOL isPredicted )
{
#if defined( CLIENT_WEAPONS )
	if ( !isPredicted )
#else
	if ( 1 )
#endif
	{
		return ( attack_time <= curtime ) ? TRUE : FALSE;
	}
	else
	{
		return ( attack_time <= 0.0 ) ? TRUE : FALSE;
	}
}

void CBasePlayerWeapon::ItemPostFrame( void )
{
	if ( m_hPlayer->m_fWeaponsBlocked || m_bIsBeingRemoved )
		return;

	FinishReload();

	if ((m_hPlayer->pev->button & IN_ATTACK2) && CanAttack( m_flNextSecondaryAttack, gpGlobals->time, UseDecrement() ) )
	{
		if ( pszAmmo2() && !m_hPlayer->m_rgAmmo[SecondaryAmmoIndex()] )
		{
			m_fFireOnEmpty = TRUE;
		}

		m_hPlayer->pev->button &= ~IN_ATTACK2;

		SecondaryAttack();
	}
	else if ((m_hPlayer->pev->button & IN_ATTACK) && CanAttack( m_flNextPrimaryAttack, gpGlobals->time, UseDecrement() ) )
	{
		if ( (m_iClip == 0 && pszAmmo1()) || (iMaxClip() == -1 && !m_hPlayer->m_rgAmmo[PrimaryAmmoIndex()] ) )
		{
			m_fFireOnEmpty = TRUE;
		}

		PrimaryAttack();
	}
	else if ( ( m_hPlayer->pev->button & IN_ALT1 )
		&& CanAttack( m_flNextTertiaryAttack, gpGlobals->time, UseDecrement() ) )
	{
		m_hPlayer->pev->button &= ~IN_ALT1;
		TertiaryAttack();

		g_WeaponTertiaryAttackHook.Call( 0, ResolveWeaponPlayer( m_hPlayer ), this );
	}
	else if ( m_hPlayer->pev->button & IN_RELOAD && iMaxClip() != WEAPON_NOCLIP && !m_fInReload )
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.
		Reload();
	}
	else if ( !(m_hPlayer->pev->button & (IN_ATTACK|IN_ATTACK2 | IN_ALT1 ) ) )
	{
		// no fire buttons down

		m_fFireOnEmpty = FALSE;

		if ( !IsUseable() && m_flNextPrimaryAttack < ( UseDecrement() ? 0.0 : gpGlobals->time ) )
		{
			// weapon isn't useable, switch.
			if ( !( m_hPlayer->pev->flags & FL_FROZEN ) && !(iFlags() & ITEM_FLAG_NOAUTOSWITCHEMPTY) && g_pGameRules->GetNextBestWeapon( m_hPlayer, this ) )
			{
				m_flNextPrimaryAttack = ( UseDecrement() ? 0.0 : gpGlobals->time ) + 0.3;
				return;
			}
		}
		else
		{
			// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
			if ( ShouldReload() )
			{
				Reload();
				return;
			}
		}

		WeaponIdle( );
		return;
	}

	// catch all
	if ( ShouldWeaponIdle() )
	{
		WeaponIdle();
	}
}

void CBasePlayerItem::DestroyItem( void ) {
	if ( m_hPlayer )
	{
		// if attached to a player, remove. 
		m_hPlayer->RemovePlayerItem( this );
	}

	Kill( );
}

bool CBasePlayerItem::AddToPlayer( CBasePlayer *pPlayer ) {
	m_hPlayer = NULL;

	if ( pPlayer && pPlayer->pev )
	{
		edict_t *entity = ENT( pPlayer->pev );

		if ( entity && !entity->free )
			m_hPlayer.Set( entity );
	}

	return TRUE; }

bool CBasePlayerItem::Unhook( void )
{
	CBasePlayer *player = m_hPlayer;

	if ( player && !player->RemovePlayerItem( this ) )
		return false;

	pev->spawnflags |= SF_NORESPAWN;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->modelindex = 0;
	pev->model = iStringNull;
	pev->aiment = NULL;
	pev->owner = NULL;
	m_hPlayer = NULL;
	pev->nextthink = gpGlobals->time + 0.1;
	SetThink( NULL );
	SetTouch( NULL );
	SetUse( NULL );

	return true;
}

void CBasePlayerItem::DetachFromPlayer( void )
{
	if ( !pev->owner || pev->owner->free || !ENTINDEX( pev->owner ) )
		return;

	CBasePlayer *player = m_hPlayer;

	if ( player->m_pActiveItem == this )
		g_pGameRules->GetNextBestWeapon( player, this );

	Unhook();

	SetModel_Intercept( edict(), STRING( m_iszWorldModel ) );
	pev->effects &= ~EF_NODRAW;
}

void CBasePlayerItem::Drop( void ) {
	SetTouch( NULL );
	SetThink(&CBasePlayerItem::SUB_Remove);
	pev->nextthink = gpGlobals->time + .1;
}

void CBasePlayerItem::Kill( void ) {
	m_bIsBeingRemoved = true;

	SetTouch( NULL );

	SetUse( NULL );

	SetThink(&CBasePlayerItem::SUB_Remove);
	pev->nextthink = gpGlobals->time + .1;
}

void CBasePlayerItem::Holster( int skiplocal ) {
	m_hPlayer->pev->viewmodel = 0; 
	m_hPlayer->pev->weaponmodel = 0;
}

void CBasePlayerItem::AttachToPlayer ( CBasePlayer *pPlayer ) {
	pev->movetype = MOVETYPE_FOLLOW;
	pev->solid = SOLID_NOT;
	pev->aiment = pPlayer->edict();
	pev->effects = EF_NODRAW; // ??
	pev->modelindex = 0;// server won't send down to clients if modelindex == 0
	pev->model = iStringNull;
	pev->owner = pPlayer->edict();
	pev->nextthink = gpGlobals->time + .1;
	SetTouch( NULL );
}

// CALLED THROUGH the newly-touched weapon's instance. The existing player weapon is pOriginal
bool CBasePlayerWeapon::AddDuplicate( CBasePlayerItem *pOriginal ) {
	if ( !pOriginal )
		return FALSE;

	if ( ( pev->spawnflags & SF_CREATEDWEAPON ) && m_iUnknown180 != 0 && m_iUnknown180 != 3 )
	{
		if ( !( iFlags() & ITEM_FLAG_EXHAUSTIBLE ) || !pOriginal->CanHaveDuplicates() )
			return FALSE;
	}

	return ExtractAmmoFromItem( pOriginal );
}

BOOL CBasePlayerWeapon::AddAmmoFromItem( CBasePlayerItem *item )
{
	if ( ( pev->spawnflags & SF_CREATEDWEAPON ) && !( iFlags() & ITEM_FLAG_EXHAUSTIBLE ) )
		return ExtractAmmoFromItem( item );

	return FALSE;
}

BOOL CBasePlayerWeapon::ExtractAmmoFromItem( CBasePlayerItem *item )
{
	CBasePlayerWeapon *weapon = item ? (CBasePlayerWeapon *) item->GetWeaponPtr() : NULL;

	if ( m_iDefaultAmmo > 0 || m_iDefaultSecAmmo > 0 )
		return ExtractAmmo( weapon );

	return ExtractClipAmmo( weapon );
}

bool CBasePlayerWeapon::AddToPlayer( CBasePlayer *pPlayer ) {
	CBasePlayerItem::AddToPlayer( pPlayer );

	if ( !m_iPrimaryAmmoType )
	{
		m_iPrimaryAmmoType = pPlayer->GetAmmoIndex( pszAmmo1() );
		m_iSecondaryAmmoType = pPlayer->GetAmmoIndex( pszAmmo2() );
	}

	float nextPickup = 0.0f;

	if ( !( iFlags() & ITEM_FLAG_EXHAUSTIBLE ) )
	{
		double delay = GetRespawnTime();

		nextPickup = (float) ( delay + (double) gpGlobals->time );
	}

	pPlayer->m_flNextWeaponPickup[m_iId] = nextPickup;
	return AddWeapon() != FALSE; }

bool CBasePlayerWeapon::UpdateClientData( CBasePlayer *pPlayer ) {
	bool bSend = false;
	int state = 0;

	if ( pPlayer->m_pActiveItem == this )
	{
		if ( pPlayer->m_fOnTarget )
			state = WEAPON_IS_ONTARGET;
		else
			state = 1;
	}

	if ( m_fIsAkimbo )
		state |= WEAPON_STATE_AKIMBO;

	// Forcing send of all data!
	if ( !pPlayer->m_fWeapon )
	{
		bSend = true;
	}
	
	// This is the current or last weapon, so the state will need to be updated
	if ( this == pPlayer->m_pActiveItem ||
		 this == pPlayer->m_pClientActiveItem )
	{
		if ( pPlayer->m_pActiveItem != pPlayer->m_pClientActiveItem )
		{
			bSend = true;
		}
	}

	// If the ammo, state, or fov has changed, update the weapon
	if ( m_iClip != m_iClientClip ||
	     m_iClip2 != m_iClientClip2 ||
	     state != m_iClientWeaponState ||
	     pPlayer->m_iFOV != pPlayer->m_iClientFOV )
	{
		bSend = true;
	}

	if ( bSend )
	{
		pPlayer->SendCurrentWeapon( state, m_iId, m_iClip, m_iClip2 );

		m_iClientClip = m_iClip;
		m_iClientClip2 = m_iClip2;
		m_iClientWeaponState = state;
		pPlayer->m_fWeapon = TRUE;
	}

	if ( m_pNext )
		m_pNext->UpdateClientData( pPlayer );

	return true;
}


void CBasePlayerWeapon::ChangeWeaponSkin( short skin )
{
	pev->skin = skin;
}

void CBasePlayerWeapon::SendWeaponAnim( int iAnim, int skiplocal, int body )
{
    // H2Whoa: Prevent poorly scripted weapons from crashing the game, 2019/12/11
    if( !m_hPlayer )
        return;

    m_hPlayer->pev->weaponanim = iAnim;
    m_iBody = body;

    //skiplocal = "IS A CLIENT WEAPON"
    //engine canskip returns true if: cl_lw = 1

    if ( skiplocal && ENGINE_CANSKIP( m_hPlayer->edict() ) )
        return;

    // GeckoN: HACK: Extended version of svc_weaponanim (sends "body" as int instead of byte)
    // GeckoN: TODO: Remove this when we have a better way to add protocol extensions
    if ( body > 255 )
    {
        MESSAGE_BEGIN( MSG_ONE, SVC_WEAPONANIMEX, NULL, m_hPlayer->pev );
            WRITE_BYTE( iAnim );                // sequence number
            WRITE_LONG( body );                    // weaponmodel bodygroup. // Changed from pev->body to body -SvenV
        MESSAGE_END();
    }
    else
    {
        MESSAGE_BEGIN( MSG_ONE, SVC_WEAPONANIM, NULL, m_hPlayer->pev );
            WRITE_BYTE( iAnim );                // sequence number
            WRITE_BYTE( body );                    // weaponmodel bodygroup. // Changed from pev->body to body -SvenV
        MESSAGE_END();
    }
}


int CBasePlayerWeapon :: AddPrimaryAmmo( int iCount, char *szName, int iMaxClip, int iMaxCarry, int *amountGiven )
{
	int clipGiven = 0;
	int localAmountGiven = 0;

	if ( !amountGiven )
		amountGiven = &localAmountGiven;

	if ( iMaxClip < 1 )
		m_iClip = WEAPON_NOCLIP;
	else if ( m_iClip == 0 && m_iUnknown180 != 1 && m_iUnknown180 != 2 )
	{
		clipGiven = V_min( iCount, iMaxClip );
		m_iClip = clipGiven;
	}

	int ammoIndex = m_hPlayer->GiveAmmo( iCount - clipGiven, szName, iMaxCarry, true, amountGiven );

	if ( clipGiven > 0 )
	{
		ammoIndex = CBasePlayer::GetAmmoIndex( szName );
		*amountGiven += clipGiven;
	}

	if ( ammoIndex > 0 )
	{
		m_iPrimaryAmmoType = ammoIndex;

		if ( m_hPlayer->HasPlayerItem( this ) )
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM );
	}

	return ammoIndex;
}

int CBasePlayerWeapon :: AddSecondaryAmmo( int iCount, char *szName, int iMax, int *amountGiven )
{
	int ammoIndex = m_hPlayer->GiveAmmo( iCount, szName, iMax, true, amountGiven );

	if ( ammoIndex > 0 )
	{
		m_iSecondaryAmmoType = ammoIndex;
		EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM );
	}

	return ammoIndex;
}

//=========================================================
// IsUseable - this function determines whether or not a 
// weapon is useable by the player in its current state. 
// (does it have ammo loaded? do I have any ammo for the 
// weapon?, etc)
//=========================================================
bool CBasePlayerWeapon :: IsUseable( void ) {
	if ( m_hPlayer->pev->flags & FL_FROZEN )
		return FALSE;

	if ( m_iClip <= 0 && m_iClip2 <= 0 )
	{
		if ( m_hPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] <= 0 && iMaxAmmo1() != -1 )
		{
			// clip is empty (or nonexistant) and the player has no more ammo of this type.
			return m_hPlayer->m_rgAmmo[ SecondaryAmmoIndex() ] > 0 && iMaxAmmo2() != -1;
		}
	}

	return TRUE; }

bool CBasePlayerWeapon :: CanDeploy( void )
{
	BOOL bHasAmmo = 0;

	if ( !m_hPlayer )
		return FALSE;

	if ( ( m_hPlayer->m_afPhysicsFlags & PFLAG_ONBARNACLE ) && PM_HACK_GetBarnacleParalyze() )
	{
		// a paralyzing barnacle hides the view model once the last attack has finished
		if ( UTIL_WeaponTimeBase() >= m_hPlayer->m_flNextAttack )
			m_hPlayer->pev->viewmodel = 0;

		return FALSE;
	}

	if ( !pszAmmo1() )
	{
		// this weapon doesn't use ammo, can always deploy.
		return TRUE;
	}

	if ( pszAmmo1() )
	{
		bHasAmmo |= (m_hPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0);
	}
	if ( pszAmmo2() )
	{
		bHasAmmo |= (m_hPlayer->m_rgAmmo[m_iSecondaryAmmoType] != 0);
	}
	if (m_iClip > 0)
	{
		bHasAmmo |= 1;
	}
	if ( m_iClip2 > 0 )
	{
		bHasAmmo |= 1;
	}
	if (!bHasAmmo)
	{
		return FALSE;
	}

	return TRUE;
}

const char *CBasePlayerWeapon::GetP_Model( const char *model )
{
	if ( pev->iuser1 )
		return STRING( pev->iuser1 );

	if ( g_szModelReplacement && model )
	{
		CWorld *world = (CWorld *) CBaseEntity::Instance( INDEXENT( 0 ) );

		model = world->MODELREPLACEMENT_Find( model );
	}

	return GetWeaponStandardModel( WEAPONMDL_PLAYER, m_iId, model );
}

const char *CBasePlayerWeapon::GetW_Model( const char *model )
{
	if ( pev->iuser2 )
		return STRING( pev->iuser2 );

	if ( g_szModelReplacement && model )
	{
		CWorld *world = (CWorld *) CBaseEntity::Instance( INDEXENT( 0 ) );

		model = world->MODELREPLACEMENT_Find( model );
	}

	return GetWeaponStandardModel( WEAPONMDL_WORLD, m_iId, model );
}

const char *CBasePlayerWeapon::GetV_Model( const char *model )
{
	if ( pev->iuser3 )
		return STRING( pev->iuser3 );

	if ( g_szModelReplacement && model )
	{
		CWorld *world = (CWorld *) CBaseEntity::Instance( INDEXENT( 0 ) );

		model = world->MODELREPLACEMENT_Find( model );
	}

	return GetWeaponStandardModel( WEAPONMDL_VIEW, m_iId, model );
}

void CBasePlayerWeapon::SetV_Model( const char *model )
{
	m_hPlayer->pev->viewmodel = MAKE_STRING( model );
}

void CBasePlayerWeapon::SetP_Model( const char *model )
{
	m_hPlayer->pev->weaponmodel = MAKE_STRING( model );
}

void CBasePlayerWeapon::PrecacheCustomModels( void )
{
	if ( GetP_Model( NULL ) )
		UTIL_PrecacheModel( this, GetP_Model( NULL ) );

	if ( GetW_Model( NULL ) )
		UTIL_PrecacheModel( this, GetW_Model( NULL ) );

	if ( GetV_Model( NULL ) )
		UTIL_PrecacheModel( this, GetV_Model( NULL ) );
}

bool CBasePlayerWeapon :: DefaultDeploy( const char *szViewModel, const char *szWeaponModel, int iAnim, const char *szAnimExt, int skiplocal /* = 0 */, int body )
{
	if (!CanDeploy( ))
		return FALSE;

	CBasePlayer *player = ResolveWeaponPlayer( m_hPlayer );

	if ( !player )
		return FALSE;

	player->m_fGrappleActive = FALSE;

	m_hPlayer->pev->viewmodel = MAKE_STRING(szViewModel);
	m_hPlayer->pev->weaponmodel = MAKE_STRING(szWeaponModel);

	if ( strlen( szAnimExt ) <= sizeof( m_hPlayer->m_szAnimExtention ) )
		UTIL_CopyString( m_hPlayer->m_szAnimExtention, szAnimExt, sizeof( m_hPlayer->m_szAnimExtention ) );

	SendWeaponAnim( iAnim, skiplocal, body );
	m_hPlayer->SetAnimation( PLAYER_DEPLOY );

	m_hPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;

	return TRUE;
}


bool CBasePlayerWeapon::DefaultReload( int cap, int anim, float delay, int body, int currentClip, int ammoIndex )
{
	CBasePlayer *player = ResolveWeaponPlayer( m_hPlayer );

	if ( player->m_rgAmmo[ammoIndex] <= 0 )
		return false;

	int amount = V_min( cap - currentClip, ResolveWeaponPlayer( m_hPlayer )->m_rgAmmo[ammoIndex] );

	if ( amount == 0 )
		return false;

	float nextAttack = (float) ( (double) UTIL_WeaponTimeBase() + (double) delay );

	ResolveWeaponPlayer( m_hPlayer )->m_flNextAttack = nextAttack;
	SendWeaponAnim( anim, UseDecrement() ? 1 : 0, body );
	pev->body = body;
	m_fInReload = true;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3.0f;
	return true;
}

bool CBasePlayerWeapon :: DefaultReload( int iClipSize, int iAnim, float fDelay, int body )
{
	return DefaultReload( iClipSize, iAnim, fDelay, body, m_iClip, m_iPrimaryAmmoType );
}

bool CBasePlayerWeapon :: PlayEmptySound( void )
{
	if (m_bPlayEmptySound)
	{
		if ( UseDecrement() )
		{
			PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_hPlayer->edict(), g_evWeaponCustom, 0.0, (float *) &g_vecZero, (float *) &g_vecZero, 0.0, 0.8, 2, 0, 0, 0 );
		}
		else
		{
			EMIT_SOUND( ENT(m_hPlayer->pev), CHAN_AUTO, "weapons/scock1.wav", 0.8, ATTN_NORM );
		}

		m_bPlayEmptySound = 0;

		return 0;
	}

	return 0;
}

void CBasePlayerWeapon :: ResetEmptySound( void )
{
	m_bPlayEmptySound = 1;
}

//=========================================================
//=========================================================
int CBasePlayerWeapon::PrimaryAmmoIndex( void ) { return m_iPrimaryAmmoType;
}

//=========================================================
//=========================================================
int CBasePlayerWeapon::SecondaryAmmoIndex( void ) {	return -1; }

void CBasePlayerWeapon::Holster( int skiplocal /* = 0 */ )
{ 
	m_fInReload = FALSE; // cancel any reload in progress.
	m_hPlayer->pev->viewmodel = 0; 
	m_hPlayer->pev->weaponmodel = 0;
}

void CBasePlayerAmmo::Spawn( void ) {
	CBasePhysicsObject::Spawn();
	SetTouch( &CBasePlayerAmmo::DefaultTouch );
}

void CBasePlayerAmmo::OnCreate( void )
{
	CBasePickupObject::OnCreate();

	m_vecMinHullSize = Vector( -15.9f, -15.9f, 0 );
	m_vecMaxHullSize = Vector( 15.9f, 15.9f, 15.9f );
	m_iBounceMaterial = 5;
	m_RespawnDelay.OnCreate( "mp_ammo_respawndelay" );
}

void CBasePlayerAmmo::Dematerialize( void )
{
	pev->effects |= EF_NODRAW;
	SetTouch( NULL );
	SetUse( NULL );
}

CBaseEntity* CBasePlayerAmmo::Respawn( void ) {
	Dematerialize();

	UTIL_SetOrigin( pev, g_pGameRules->VecAmmoRespawnSpot( this ) );// move to wherever I'm supposed to repawn.

	SetThink( &CBasePlayerAmmo::CallMaterialize );
	pev->nextthink = g_pGameRules->FlAmmoRespawnTime( this );

	return this; }

void CBasePlayerAmmo :: DefaultTouch( CBaseEntity *pOther ) {
	CBasePickupObject::DefaultTouch( pOther );
}

void CBasePlayerAmmo::Collect( CBaseEntity *other, COLLECT_TYPE collectType )
{
	if ( !other || !other->IsPlayer() )
		return;

	bool remove = false;

	if ( AddAmmo( other ) )
	{
		if ( g_pGameRules->AmmoShouldRespawn( this ) == GR_AMMO_RESPAWN_YES )
			Respawn();
		else
			remove = true;

		Collected( other, collectType );
	}
	else
		remove = gEvilImpulse101;

	if ( remove )
	{
		Dematerialize();
		SetThink( &CBasePlayerAmmo::SUB_Remove );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

//=========================================================
// called by the new item with the existing item as parameter
//
// if we call ExtractAmmo(), it's because the player is picking up this type of weapon for 
// the first time. If it is spawned by the world, m_iDefaultAmmo will have a default ammo amount in it.
// if  this is a weapon dropped by a dying player, has 0 m_iDefaultAmmo, which means only the ammo in 
// the weapon clip comes along. 
//=========================================================
int CBasePlayerWeapon::ExtractAmmo( CBasePlayerWeapon *pWeapon ) {
	int amountGiven = 0;
	int result = FALSE;

	if ( pszAmmo1() && pWeapon->AddPrimaryAmmo( m_iDefaultAmmo, (char *) pszAmmo1(), iMaxClip(), iMaxAmmo1(), &amountGiven ) > 0 )
	{
		m_iDefaultAmmo -= amountGiven;
		result = TRUE;
	}

	if ( pszAmmo2() && pWeapon->AddSecondaryAmmo( m_iDefaultSecAmmo, (char *) pszAmmo2(), iMaxAmmo2(), &amountGiven ) > 0 )
	{
		m_iDefaultSecAmmo -= amountGiven;
		result = TRUE;
	}

	return result;
}

//=========================================================
// called by the new item's class with the existing item as parameter
//=========================================================
int CBasePlayerWeapon::ExtractClipAmmo( CBasePlayerWeapon *pWeapon ) {
	int ammo;

	if ( m_iClip == WEAPON_NOCLIP )
		ammo = 0;
	else
		ammo = m_iClip;

	int amountGiven = 0;

	if ( !pWeapon || !pWeapon->m_hPlayer )
		return FALSE;

	if ( pWeapon->m_hPlayer->GiveAmmo( ammo, pszAmmo1(), iMaxAmmo1(), true, &amountGiven ) <= 0 )
		return FALSE;

	m_iClip -= amountGiven;
	return TRUE;
}
	
//=========================================================
// RetireWeapon - no more ammo for this gun, put it away.
//=========================================================
void CBasePlayerWeapon::RetireWeapon( void ) {
	// first, no viewmodel at all.
	m_hPlayer->pev->viewmodel = iStringNull;
	m_hPlayer->pev->weaponmodel = iStringNull;
	//m_hPlayer->pev->viewmodelindex = NULL;

	if ( !g_pGameRules->GetNextBestWeapon( m_hPlayer, this ) )
		m_hPlayer->m_pActiveItem = NULL;
}

//=========================================================================
// GetNextAttackDelay - An accurate way of calcualting the next attack time.
//=========================================================================
float CBasePlayerWeapon::GetNextAttackDelay( float flTime ) {
	if(m_flLastFireTime == 0 || m_flNextPrimaryAttack == -1) 
	{
		// At this point, we are assuming that the client has stopped firing
		// and we are going to reset our book keeping variables.
		m_flLastFireTime = gpGlobals->time;
		m_flPrevPrimaryAttack = flTime; }
	// calculate the time between this shot and the previous
	float flTimeBetweenFires = gpGlobals->time - m_flLastFireTime;
	float flCreep = 0.0f;
	if(flTimeBetweenFires > 0)
			flCreep = flTimeBetweenFires - m_flPrevPrimaryAttack; // postive or negative
	
	// save the last fire time
	m_flLastFireTime = gpGlobals->time;		
	
	float flNextAttack = UTIL_WeaponTimeBase() + flTime - flCreep;
	// we need to remember what the m_flNextPrimaryAttack time is set to for each shot, 
	// store it as m_flPrevPrimaryAttack.
	m_flPrevPrimaryAttack = flNextAttack - UTIL_WeaponTimeBase();
// 	char szMsg[256];
// 	_snprintf( szMsg, sizeof(szMsg), "next attack time: %0.4f\n", gpGlobals->time + flNextAttack );
// 	OutputDebugString( szMsg );
	return flNextAttack;
}


//*********************************************************
// weaponbox code:
//*********************************************************

LINK_ENTITY_TO_CLASS( weaponbox, CWeaponBox );

TYPEDESCRIPTION	CWeaponBox::m_SaveData[] = 
{
	DEFINE_ARRAY( CWeaponBox, m_rgAmmo, FIELD_INTEGER, MAX_AMMO_SLOTS ),
	DEFINE_ARRAY( CWeaponBox, m_rgiszAmmo, FIELD_STRING, MAX_AMMO_SLOTS ),
	{ FIELD_EHANDLE, "m_rghPlayerItems", offsetof( CWeaponBox, m_rgpPlayerItems ), MAX_ITEM_TYPES, 0 },
	DEFINE_FIELD( CWeaponBox, m_cAmmoTypes, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CWeaponBox, CBaseEntity );

//=========================================================
//
//=========================================================
void CWeaponBox::Precache( void )
{
	CBaseEntity::Precache();
	UTIL_PrecacheModel( this, pev->model ? STRING( pev->model ) : "models/w_weaponbox.mdl");
	PrecacheSound_Intercept( this, "debris/metal2.wav" );
	PrecacheSound_Intercept( this, "debris/flesh5.wav" );
}

void CWeaponBox::OnCreate( void )
{
	CBasePickupObject::OnCreate();

	m_vecMinHullSize = Vector( -16, -16, 0 );
	m_vecMaxHullSize = Vector( 16, 16, 16 );
	m_iBounceMaterial = 5;
}

void CWeaponBox::Spawn( void )
{
	Precache( );

	if ( !SetupModel() )
		SET_MODEL( edict(), pev->model ? STRING( pev->model ) : "models/w_weaponbox.mdl" );

	CBasePhysicsObject::Spawn();

	UTIL_SetSize( pev, m_vecMinHullSize, m_vecMaxHullSize );
	SetUse( &CBasePickupObject::CallDefaultUse );
	SetTouch( &CBasePickupObject::DefaultTouch );
	SetThink( &CBasePhysicsObject::FallThink );
}

void CWeaponBox :: KeyValue( KeyValueData *data )
{
	const char *name = data->szKeyName;
	const char *ammoName = name;
	bool canonical = FStrEq( name, "9mm" ) || FStrEq( name, "357" ) || FStrEq( name, "buckshot" )
	                 || FStrEq( name, "bolts" ) || FStrEq( name, "556" ) || FStrEq( name, "ARgrenades" )
	                 || FStrEq( name, "uranium" ) || FStrEq( name, "Hand Grenade" ) || FStrEq( name, "Satchel Charge" )
	                 || FStrEq( name, "Trip Mine" ) || FStrEq( name, "rockets" ) || FStrEq( name, "Snarks" )
	                 || FStrEq( name, "m40a1" ) || FStrEq( name, "sporeclip" );
	bool alias = FStrEq( name, "bullet9mm" ) || FStrEq( name, "bullet357" ) || FStrEq( name, "bullet556" )
	             || FStrEq( name, "handgrenade" ) || FStrEq( name, "satchelcharge" ) || FStrEq( name, "tripmine" );

	if ( !canonical && !alias )
	{
		CBasePickupObject::KeyValue( data );
		return;
	}

	if ( alias )
{
	if ( FStrEq( name, "bullet9mm" ) )
			ammoName = "9mm";
		else if ( FStrEq( name, "bullet357" ) )
			ammoName = "357";
		else if ( FStrEq( name, "bullet556" ) )
			ammoName = "556";
		else if ( FStrEq( name, "handgrenade" ) )
			ammoName = "Hand Grenade";
		else if ( FStrEq( name, "satchelcharge" ) )
			ammoName = "Satchel Charge";
		else
			ammoName = "Trip Mine";
	}

	if ( m_cAmmoTypes >= MAX_AMMO_SLOTS )
	{
		ALERT( at_console, alias ? "WeaponBox too full! Only %d ammo types allowed (X%f Y%f Z%f)\n"
		                        : "WeaponBox too full! Only %d ammo types allowed at (X%f Y%f Z%f)\n",
		       MAX_AMMO_SLOTS, pev->origin.x, pev->origin.y, pev->origin.z );
		return;
	}

	int count = atoi( data->szValue );

	if ( count > 0 )
	{
		PackAmmo( ALLOC_STRING( ammoName ), count );
		++m_cAmmoTypes;
	}

	data->fHandled = TRUE;
	}

bool CWeaponBox::CanCollect( CBaseEntity *other, COLLECT_TYPE collectType )
{
	if ( !CBasePickupObject::CanCollect( other, collectType ) )
		return false;

	CBasePlayer *player = dynamic_cast<CBasePlayer *>( other );

	if ( !player )
		return false;

	if ( m_hExclusiveHoldItem && m_hExclusiveHoldItem->m_bExclusiveHold && player->IsExclusiveHoldItemActive() )
		return false;

	CBaseEntity *ownerEntity = Instance( pev->owner );
	CBasePlayer *owner = ownerEntity && ownerEntity->IsPlayer() ? dynamic_cast<CBasePlayer *>( ownerEntity ) : NULL;

	if ( owner && m_iUnknown180 == 2 && m_flCreationTime + 30 > gpGlobals->time
	     && owner != player && !owner->IsGibbed() )
		return false;

	if ( player->IsExclusiveHoldItemActive() )
	{
		for ( int slot = 0; slot < MAX_ITEM_TYPES; ++slot )
		{
			CBasePlayerItem *item = m_rgpPlayerItems[slot];

			if ( item && item->m_bExclusiveHold )
				return false;
		}
	}

	return true;
}

void CWeaponBox::Collect( CBaseEntity *other, COLLECT_TYPE collectType )
{
	CBasePlayer *player = dynamic_cast<CBasePlayer *>( other );

	if ( !player )
		return;

	bool collected = false;

	for ( int index = 0; index < MAX_AMMO_SLOTS; ++index )
	{
		if ( m_rgiszAmmo[index] && player->GiveAmmo( m_rgAmmo[index], STRING( m_rgiszAmmo[index] ),
		                                         MaxAmmoCarry( m_rgiszAmmo[index] ), false, NULL ) >= 0 )
			collected = true;
	}

	for ( int slot = 0; slot < MAX_ITEM_TYPES; ++slot )
	{
		CBasePlayerItem *item;

		while ( ( item = m_rgpPlayerItems[slot] ) != NULL )
		{
			m_rgpPlayerItems[slot] = item->m_pNext;

			if ( m_hExclusiveHoldItem == item )
				m_hExclusiveHoldItem = NULL;

			if ( player->AddPlayerItem( item ) != APIR_NotAdded )
			{
				item->AttachToPlayer( player );
				collected = true;
			}
		}
	}

	if ( !collected )
		return;

	EMIT_SOUND( edict(), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM );
	Collected( other, collectType );

	if ( !( pev->spawnflags & ( SF_NORESPAWN | SF_CREATEDWEAPON ) ) )
	{
		Dematerialize();
		SetThink( &CBasePickupObject::CallMaterialize );

		float delay = m_RespawnDelay.GetRespawnTime( 20, false );

		if ( delay < 0 )
		{
			Dematerialize();
			Kill();
			return;
		}

		pev->nextthink = gpGlobals->time + delay;
		return;
	}

	Dematerialize();
	Kill();
}

void CWeaponBox::Dematerialize( void )
{
	pev->effects |= EF_NODRAW;
	SetTouch( NULL );
	SetUse( NULL );
}

void CWeaponBox::Kill( void )
{
	for ( int i = 0 ; i < MAX_ITEM_TYPES ; ++i )
	{
		for ( CBasePlayerItem *pWeapon = m_rgpPlayerItems[ i ]; pWeapon; pWeapon = pWeapon->m_pNext )
		{
			pWeapon->SetThink(&CBasePlayerItem::SUB_Remove);
			pWeapon->pev->nextthink = gpGlobals->time + 0.1;
		}
	}

	SetThink( &CWeaponBox::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1f;
}

bool CWeaponBox::CriticalRemove( void )
{
	SetThink( &CWeaponBox::CallKill );
	pev->nextthink = gpGlobals->time;
	return true;
}

bool CWeaponBox::PackWeapon( CBasePlayerItem *pWeapon )
{
	// is one of these weapons already packed in this box?
	if ( !pWeapon || HasWeapon( STRING( pWeapon->pev->classname ) ) )
		return FALSE;

	pWeapon->Unhook();
	pWeapon->pev->owner = edict();

	int iWeaponSlot = pWeapon->iItemSlot();

	if ( m_rgpPlayerItems[ iWeaponSlot ] )
		pWeapon->m_pNext = m_rgpPlayerItems[ iWeaponSlot ];
	else
		pWeapon->m_pNext = NULL;

	m_rgpPlayerItems[ iWeaponSlot ] = pWeapon;
	return TRUE;
}

bool CWeaponBox::PackAmmo( int iszName, int iCount )
{
	if ( FStringNull( iszName ) )
	{
		// error here
		ALERT ( at_console, "NULL String in PackAmmo!\n" );
		return FALSE;
	}

	int iMaxCarry = MaxAmmoCarry( iszName );

	if ( iMaxCarry == -1 || iCount <= 0 )
		return false;

	GiveAmmo( iCount, STRING( iszName ), iMaxCarry );
		return TRUE;
	}

int CWeaponBox::GiveAmmo( int iCount, const char *szName, int iMax, int *pIndex/* = NULL*/ )
{
	int i = 1;

	for ( ; i < MAX_AMMO_SLOTS && m_rgiszAmmo[i]; ++i )
	{
		if (stricmp( szName, STRING( m_rgiszAmmo[i])) == 0)
		{
			if (pIndex)
				*pIndex = i;

			int iAdd = V_min( iCount, iMax - m_rgAmmo[i]);
			if (iCount != 0 && iAdd <= 0 )
				return -1;

			m_rgAmmo[i] += iAdd;

				return i;
		}
	}
	if (i < MAX_AMMO_SLOTS)
	{
		if (pIndex)
			*pIndex = i;

		m_rgiszAmmo[i] = MAKE_STRING( szName );
		m_rgAmmo[i] = iCount;

		return i;
	}
	ALERT( at_console, "out of named ammo slots\n");
	return i;
}

bool CWeaponBox::HasWeapon( const char *classname )
{
	for ( int slot = 0; slot < MAX_ITEM_TYPES; ++slot )
	{
		for ( CBasePlayerItem *pItem = m_rgpPlayerItems[slot]; pItem; pItem = pItem->m_pNext )
	{
		if (FClassnameIs( pItem->pev, classname) )
				return TRUE;
		}
	}

	return FALSE;
}

bool CWeaponBox::IsEmpty( void )
{
	for ( int i = 0 ; i < MAX_ITEM_TYPES ; ++i )
	{
		if ( m_rgpPlayerItems[ i ] )
			return FALSE;
		}

	for ( int index = 0 ; index < MAX_AMMO_SLOTS ; ++index )
	{
		if ( m_rgiszAmmo[index] )
			return FALSE;
	}

	return TRUE;
}

void CWeaponBox::SetObjectCollisionBox( void )
{
	pev->absmin = pev->origin + Vector(-16, -16, 0);
	pev->absmax = pev->origin + Vector(16, 16, 16);
}

void AddWeaponToPack( CBasePlayerItem *item, CWeaponBox *box )
{
	box->PackWeapon( item );
	item->pev->spawnflags |= SF_CREATEDWEAPON;
}

void PackWeapons( CBasePlayer *player, CWeaponBox *box )
{
	int policy = g_pGameRules->DeadPlayerWeapons( player );

	if ( policy == GR_PLR_DROP_GUN_NO )
		return;

	if ( policy == GR_PLR_DROP_GUN_ACTIVE )
	{
		if ( player->m_pActiveItem )
			AddWeaponToPack( player->m_pActiveItem, box );

		return;
	}

	for ( int slot = 0; slot < MAX_ITEM_TYPES; ++slot )
	{
		CBasePlayerItem *item = player->m_rgpPlayerItems[slot];

		while ( item )
		{
			CBasePlayerItem *next = item->m_pNext;

			AddWeaponToPack( item, box );
			item = next;
		}
	}
}

void AddAmmoToPack( CBasePlayer *player, int index, CWeaponBox *box )
{
	box->PackAmmo( MAKE_STRING( CBasePlayerItem::AmmoInfoArray[index].pszName ), player->m_rgAmmo[index] );
	player->m_rgAmmo[index] = 0;
}

void PackAmmo( CBasePlayer *player, CWeaponBox *box )
{
	int policy = g_pGameRules->DeadPlayerAmmo( player );

	if ( policy == GR_PLR_DROP_AMMO_NO )
		return;

	if ( policy == GR_PLR_DROP_AMMO_ACTIVE )
	{
		if ( !player->m_pActiveItem )
			return;

		int index = player->m_pActiveItem->PrimaryAmmoIndex();

		if ( index >= 0 && player->m_rgAmmo[index] > 0 )
			AddAmmoToPack( player, index, box );

		index = player->m_pActiveItem->SecondaryAmmoIndex();

		if ( index >= 0 && player->m_rgAmmo[index] > 0 )
			AddAmmoToPack( player, index, box );

		return;
	}

	for ( int index = 0; index < MAX_AMMO_SLOTS; ++index )
	{
		if ( player->m_rgAmmo[index] > 0 )
			AddAmmoToPack( player, index, box );
	}
}


void CBasePlayerWeapon::PrintState( void )
{
	ALERT( at_console, "primary:  %f\n", m_flNextPrimaryAttack );
	ALERT( at_console, "idle   :  %f\n", m_flTimeWeaponIdle );

//	ALERT( at_console, "nextrl :  %f\n", m_flNextReload );
//	ALERT( at_console, "nextpum:  %f\n", m_flPumpTime );

//	ALERT( at_console, "m_frt  :  %f\n", m_fReloadTime );
	ALERT( at_console, "m_finre:  %i\n", m_fInReload );
//	ALERT( at_console, "m_finsr:  %i\n", m_fInSpecialReload );

	ALERT( at_console, "m_iclip:  %i\n", m_iClip );
}


TYPEDESCRIPTION	CRpg::m_SaveData[] = 
{
	DEFINE_FIELD( CRpg, m_fSpotActive, FIELD_INTEGER ),
	DEFINE_FIELD( CRpg, m_cActiveRockets, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CRpg, CBasePlayerWeapon );

TYPEDESCRIPTION	CShotgun::m_SaveData[] = 
{
	DEFINE_FIELD( CShotgun, m_flNextReload, FIELD_TIME ),
	DEFINE_FIELD( CShotgun, m_fInSpecialReload, FIELD_INTEGER ),
	DEFINE_FIELD( CShotgun, m_flNextReload, FIELD_TIME ),
	// DEFINE_FIELD( CShotgun, m_iShell, FIELD_INTEGER ),
	DEFINE_FIELD( CShotgun, m_flPumpTime, FIELD_TIME ),
};
IMPLEMENT_SAVERESTORE( CShotgun, CBasePlayerWeapon );

TYPEDESCRIPTION	CGauss::m_SaveData[] = 
{
	DEFINE_FIELD( CGauss, m_fInAttack, FIELD_INTEGER ),
	DEFINE_FIELD( CGauss, m_flStartCharge, FIELD_TIME ),
	DEFINE_FIELD( CGauss, m_flPlayAftershock, FIELD_TIME ),
	DEFINE_FIELD( CGauss, m_flNextAmmoBurn, FIELD_TIME ),
	DEFINE_FIELD( CGauss, m_fPrimaryFire, FIELD_CHARACTER ),
};
IMPLEMENT_SAVERESTORE( CGauss, CBasePlayerWeapon );



TYPEDESCRIPTION	CSatchel::m_SaveData[] = 
{
	DEFINE_FIELD( CSatchel, m_chargeReady, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CSatchel, CBasePlayerWeapon );
