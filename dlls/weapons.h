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
#ifndef WEAPONS_H
#define WEAPONS_H

#include "weapon_animations.h"

#include "weapon_iteminfo.h"

#if defined( CLIENT_DLL )
#include "hl/hl_weapons.h"
#else
#include "effects.h"
#include "CBasePickupObject.h"
#include "TypedEntityHandle.h"

class CBasePlayer;
struct weapon_data_s;
extern int gmsgWeapPickup;

void DeactivateSatchels( CBasePlayer *pOwner );
void Weapon_Init( void );
void Weapon_Precache( void );

// Contact Grenade / Timed grenade / Satchel Charge
class CGrenade : public CBaseMonster
{
public:
	void Spawn( void );

	typedef enum { SATCHEL_DETONATE = 0, SATCHEL_RELEASE } SATCHELCODE;

	static CGrenade *ShootTimed( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, float time );
	static CGrenade *ShootBananaCluster( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity );
	static CGrenade *ShootMortar( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity );
	static CGrenade *ShootContact( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity );
	static CGrenade *ShootSatchelCharge( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity );
	static void UseSatchelCharges( entvars_t *pevOwner, SATCHELCODE code );

	void Explode( Vector vecSrc, Vector vecAim );
	void Explode( TraceResult *pTrace, int bitsDamageType );
	void EXPORT Smoke( void );

	void EXPORT BounceTouch( CBaseEntity *pOther );
	void EXPORT SlideTouch( CBaseEntity *pOther );
	void EXPORT ExplodeTouch( CBaseEntity *pOther );
	void EXPORT DangerSoundThink( void );
	void EXPORT PreDetonate( void );
#ifdef CLIENT_DLL
	void EXPORT Detonate( void );
#else
	virtual void EXPORT Detonate( void );
	void EXPORT CallDetonate( void ) { Detonate(); }
#endif
	void EXPORT DetonateUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT TumbleThink( void );
	void EXPORT ClusterThink( void );

	virtual void BounceSound( void );
	virtual int	BloodColor( void ) { return DONT_BLEED; }
	virtual void Killed( entvars_t *pevAttacker, int iGib );

	int    m_iGrenadeType;
	float  m_flSpawnTime;
	float  m_flLastBounceSound;
	bool   m_bExploded;
	float  m_flNextFriction;
	Vector m_vecLastBounceOrigin;
	BOOL m_fRegisteredSound;// whether or not this grenade has issued its DANGER sound to the world sound list yet.
};


// constant items
#define ITEM_HEALTHKIT		1
#define ITEM_ANTIDOTE		2
#define ITEM_SECURITY		3
#define ITEM_BATTERY		4

#define WEAPON_NONE				0
#define WEAPON_CROWBAR			1
#define	WEAPON_GLOCK			2
#define WEAPON_PYTHON			3
#define WEAPON_MP5				4
#define WEAPON_CHAINGUN			5
#define WEAPON_CROSSBOW			6
#define WEAPON_SHOTGUN			7
#define WEAPON_RPG				8
#define WEAPON_GAUSS			9
#define WEAPON_EGON				10
#define WEAPON_HORNETGUN		11
#define WEAPON_HANDGRENADE		12
#define WEAPON_TRIPMINE			13
#define	WEAPON_SATCHEL			14
#define	WEAPON_SNARK			15
#define WEAPON_SNIPERRIFLE		23
#define WEAPON_M249				24
#define WEAPON_SHOCKRIFLE		28

#define WEAPON_UZI		17
#define WEAPON_MEDKIT		18
#define WEAPON_PIPEWRENCH		20
#define WEAPON_MINIGUN		21
#define WEAPON_GRAPPLE		22
#define WEAPON_M16		25
#define WEAPON_SPORELAUNCHER		26
#define WEAPON_EAGLE		27
#define WEAPON_DISPLACER		29

#define WEAPON_ALLWEAPONS		(~(1<<WEAPON_SUIT))

#define WEAPON_SUIT				31	// ?????

#define MAX_WEAPONS			256


#define MAX_NORMAL_BATTERY	100


// weapon weight factors (for auto-switching)   (-1 = noswitch)
#define CROWBAR_WEIGHT		0
#define GLOCK_WEIGHT		10
#define PYTHON_WEIGHT		15
#define MP5_WEIGHT			15
#define SHOTGUN_WEIGHT		15
#define CROSSBOW_WEIGHT		10
#define RPG_WEIGHT			20
#define GAUSS_WEIGHT		20
#define EGON_WEIGHT			20
#define HORNETGUN_WEIGHT	10
#define HANDGRENADE_WEIGHT	5
#define SNARK_WEIGHT		5
#define SATCHEL_WEIGHT		-10
#define TRIPMINE_WEIGHT		-10
#define SNIPERRIFLE_WEIGHT		10
#define M249_WEIGHT				20
#define SHOCKRIFLE_WEIGHT		25


// weapon clip/carry ammo capacities
#define URANIUM_MAX_CARRY		100
#define	_9MM_MAX_CARRY			250
#define _357_MAX_CARRY			36
#define BUCKSHOT_MAX_CARRY		125
#define BOLT_MAX_CARRY			50
#define ROCKET_MAX_CARRY		5
#define HANDGRENADE_MAX_CARRY	10
#define SATCHEL_MAX_CARRY		5
#define TRIPMINE_MAX_CARRY		5
#define SNARK_MAX_CARRY			15
#define HORNET_MAX_CARRY		100
#define M203_GRENADE_MAX_CARRY	10
#define _556_MAX_CARRY			600
#define _762_MAX_CARRY			15
#define SHOCK_MAX_CARRY			100

// the maximum amount of ammo each weapon's clip can hold
#define WEAPON_NOCLIP			-1

//#define CROWBAR_MAX_CLIP		WEAPON_NOCLIP
#define GLOCK_MAX_CLIP			17
#define PYTHON_MAX_CLIP			6
#define MP5_MAX_CLIP			30
#define MP5_DEFAULT_AMMO		25
#define SHOTGUN_MAX_CLIP		8
#define CROSSBOW_MAX_CLIP		5
#define RPG_MAX_CLIP			1
#define GAUSS_MAX_CLIP			WEAPON_NOCLIP
#define EGON_MAX_CLIP			WEAPON_NOCLIP
#define HORNETGUN_MAX_CLIP		WEAPON_NOCLIP
#define HANDGRENADE_MAX_CLIP	WEAPON_NOCLIP
#define SATCHEL_MAX_CLIP		WEAPON_NOCLIP
#define TRIPMINE_MAX_CLIP		WEAPON_NOCLIP
#define SNARK_MAX_CLIP			WEAPON_NOCLIP
#define SNIPERRIFLE_MAX_CLIP		5
#define M249_MAX_CLIP			200
#define SHOCKRIFLE_MAX_CLIP		WEAPON_NOCLIP


// the default amount of ammo that comes with each gun when it spawns
#define GLOCK_DEFAULT_GIVE			17
#define PYTHON_DEFAULT_GIVE			6
#define MP5_DEFAULT_GIVE			60
#define MP5_CLASSIC_DEFAULT_GIVE	75
#define MP5_CLASSIC_MAX_CLIP		50
#define MP5_ZOOM_FOV				40
#define MP5_DEFAULT_AMMO			25
#define MP5_M203_DEFAULT_GIVE		0
#define SHOTGUN_DEFAULT_GIVE		12
#define CROSSBOW_DEFAULT_GIVE		5
#define RPG_DEFAULT_GIVE			1
#define GAUSS_DEFAULT_GIVE			20
#define EGON_DEFAULT_GIVE			20
#define HANDGRENADE_DEFAULT_GIVE	5
#define SATCHEL_DEFAULT_GIVE		1
#define TRIPMINE_DEFAULT_GIVE		1
#define SNARK_DEFAULT_GIVE			5
#define HIVEHAND_DEFAULT_GIVE		100
#define SNIPERRIFLE_DEFAULT_GIVE	5
#define M249_DEFAULT_GIVE			200
#define SHOCKRIFLE_DEFAULT_GIVE	100

#define AMMO_DROP_NONE			-1
#define SNIPERRIFLE_AMMO1_DROP	5
#define M249_AMMO1_DROP			200

// The amount of ammo given to a player by an ammo item.
#define AMMO_URANIUMBOX_GIVE	20
#define AMMO_GLOCKCLIP_GIVE		GLOCK_MAX_CLIP
#define AMMO_357BOX_GIVE		PYTHON_MAX_CLIP
#define PYTHON_AMMO1_DROP	AMMO_357BOX_GIVE
#define AMMO_MP5CLIP_GIVE		MP5_MAX_CLIP
#define AMMO_CHAINBOX_GIVE		200
#define AMMO_M203BOX_GIVE		2
#define AMMO_BUCKSHOTBOX_GIVE	12
#define AMMO_CROSSBOWCLIP_GIVE	CROSSBOW_MAX_CLIP
#define AMMO_RPGCLIP_GIVE		RPG_MAX_CLIP
#define AMMO_URANIUMBOX_GIVE	20
#define AMMO_SNARKBOX_GIVE		5
#define AMMO_556_GIVE				100
#define AMMO_762_GIVE				5

// bullet types
typedef	enum
{
	BULLET_NONE = 0,
	BULLET_PLAYER_9MM = 1,
	BULLET_PLAYER_MP5 = 2,
	BULLET_PLAYER_SAW = 3,
	BULLET_PLAYER_SNIPER = 4,
	BULLET_PLAYER_357 = 5,
	BULLET_PLAYER_EAGLE = 6,
	BULLET_PLAYER_BUCKSHOT = 7,
	BULLET_PLAYER_CROWBAR = 8,
	BULLET_PLAYER_CUSTOMDAMAGE = 9,
	BULLET_MONSTER_9MM = 10,
	BULLET_MONSTER_MP5 = 11,
	BULLET_MONSTER_SAW = 12,
	BULLET_MONSTER_12MM = 13,
	BULLET_MONSTER_EAGLE = 14,
	BULLET_MONSTER_SNIPER = 15,
	BULLET_MONSTER_BUCKSHOT = 16,
	DECAL_SCORCH_MARK = 17,
} Bullet;


#define SPORELAUNCHER_DEFAULT_GIVE		8
#define SPORELAUNCHER_MAX_CARRY		30
#define SPORELAUNCHER_MAX_CLIP		8
#define SPORELAUNCHER_AMMO1_DROP		1
#define SPORELAUNCHER_WEIGHT		15

#define UZI_DEFAULT_GIVE			32
#define UZI_MAX_CLIP			32
#define UZI_WEIGHT				15
#define AMMO_UZICLIP_GIVE		UZI_MAX_CLIP
#define UZI_AMMO1_DROP			AMMO_UZICLIP_GIVE
#define MINIGUN_DEFAULT_GIVE		400
#define MINIGUN_MAX_CLIP		WEAPON_NOCLIP
#define MINIGUN_WEIGHT			25
#define MINIGUN_AMMO1_DROP		AMMO_556_GIVE

#define UZI_AKIMBO_WEIGHT		20
#define M16_WEIGHT				15
#define EAGLE_WEIGHT			15
#define DISPLACER_WEIGHT		20
#define SPORE_MAX_CARRY			30
#define M16_MAX_CLIP			30
#define EAGLE_MAX_CLIP			7
#define DISPLACER_MAX_CLIP		WEAPON_NOCLIP
#define M16_DEFAULT_GIVE			60
#define DISPLACER_DEFAULT_GIVE		20
#define EAGLE_DEFAULT_GIVE			7
#define AMMO_556CLIP_GIVE		M16_MAX_CLIP
#define M16_AMMO1_DROP			AMMO_556CLIP_GIVE
#define M16_AMMO2_DROP			1
#define EAGLE_AMMO1_DROP		AMMO_357BOX_GIVE
#define DISPLACER_AMMO1_DROP	AMMO_URANIUMBOX_GIVE

#define ITEM_FLAG_SELECTONEMPTY		1
#define ITEM_FLAG_NOAUTORELOAD		2
#define ITEM_FLAG_NOAUTOSWITCHEMPTY	4
#define ITEM_FLAG_LIMITINWORLD		8
#define ITEM_FLAG_EXHAUSTIBLE		16 // A player can totally exhaust their ammo supply and lose this weapon
#define ITEM_FLAG_DUALWIELD		32
#define ITEM_FLAG_IGNOREWEAPONSTAY	64
#define ITEM_FLAG_ESSENTIAL		128

#define BOUNCE_MATERIAL_FLESH	2

#define AMMO_SPORE_GIVE			1
#define WEAPONCUSTOM_SND_SPLAUNCHER_FIRE		23
#define WEAPONCUSTOM_SND_SPLAUNCHER_ALTFIRE		24

#define WEAPON_IS_ONTARGET 0x03	// current and on target, as the client reads CurWeapon

#define WEAPONCUSTOM_SOUND			2
#define WEAPONCUSTOM_SND_SNIPER_ZOOM	20

extern unsigned int g_evWeaponCustom;
extern unsigned int g_EvCheckClient;
extern int g_iARGrenadesHack;

// Items that the player has in their inventory that they can use
enum AddPlayerItemResult
{
	APIR_NotAdded = 0,
	APIR_DuplicateAdd = 2,
	APIR_Added = 3
};

class CBasePlayerItem : public CBasePickupObject
{
public:
	CBasePlayerItem() : m_bIsBeingRemoved( false ), m_bIsNotAmmoItem( false ), m_bExclusiveHold( false ), m_iszCustomSpriteDir( 0 ) {}

#ifndef CLIENT_DLL
	int ObjectCaps( void );
	void UpdateOnRemove( void );
#endif

	void OnCreate( void );
	void PostSpawn( void );
	void KeyValue( KeyValueData *data );
	bool CanCollect( CBaseEntity *other, COLLECT_TYPE collectType );
	void Collect( CBaseEntity *other );
	void Collect( CBaseEntity *other, COLLECT_TYPE collectType );
	void ItemUse( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value );
	virtual const char *GetPickupSound( void );
	virtual BOOL AddAmmoFromItem( CBasePlayerItem *pItem ) { return FALSE; }
	virtual bool CanHaveDuplicates( void ) { return false; }
	void UpdateCustomSpriteDir( CBasePlayer *player );
	void LoadSprites( CBasePlayer *player, const char *name );

	virtual void SetObjectCollisionBox( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];

	virtual bool AddToPlayer( CBasePlayer *pPlayer );	// return TRUE if the item you want the item added to the player inventory
	virtual bool AddDuplicate( CBasePlayerItem *pItem ) { return FALSE; }	// return TRUE if you want your duplicate removed from world
	void EXPORT DestroyItem( void );
	void EXPORT FallThink ( void );// when an item is first spawned, this think is run to determine when the object has hit the ground.
	void EXPORT Materialize( void );// make a weapon visible and tangible
	void EXPORT AttemptToMaterialize( void );  // the weapon desires to become visible and tangible, if the game rules allow for it
	CBaseEntity* Respawn ( void );// copy a weapon
	void FallInit( void );
	void CheckRespawn( void );
	virtual float GetRespawnTime( void ) { return 20; }
	virtual bool GetItemInfo(ItemInfo *p) { return 0; };	// returns 0 if struct not filled out
	virtual bool CanDeploy( void ) { return TRUE; };
	virtual bool Deploy( )								// returns is deploy was successful
		 { return TRUE; };

	virtual bool CanHolster( void ) { return TRUE; };// can this weapon be put away right now?
	virtual void Holster( int skiplocal = 0 );
	virtual void UpdateItemInfo( void ) { return; };

	virtual void ItemPreFrame( void )	{ return; }		// called each frame by the player PreThink
	virtual void ItemPostFrame( void ) { return; }		// called each frame by the player PostThink
	virtual void InactiveItemPreFrame( void ) {}
	virtual void InactiveItemPostFrame( void ) {}

	virtual void Drop( void );
	virtual CBasePlayerItem *DropItem( void );
	virtual void Kill( void );
	virtual void AttachToPlayer ( CBasePlayer *pPlayer );
	bool Unhook( void );
#ifndef CLIENT_DLL
	virtual void DetachFromPlayer( void );
#else
	void DetachFromPlayer( void );
#endif

	virtual int PrimaryAmmoIndex() { return -1; };
	virtual int SecondaryAmmoIndex() { return -1; };

	virtual bool UpdateClientData( CBasePlayer *pPlayer ) { return false; }

	virtual CBasePlayerItem *GetWeaponPtr( void ) { return NULL; };
	virtual CBasePlayerItem *MyItemPointer( void ) { return this; }

	static void AddAmmoNameToAmmoRegistry( const char *name, int maxCarry, const char *weaponName );

	static ItemInfo ItemInfoArray[ MAX_WEAPONS ];
	static AmmoInfo AmmoInfoArray[ MAX_AMMO_SLOTS ];

	CEntityHandle<CBasePlayer>     m_hPlayer;
	CEntityHandle<CBasePlayerItem> m_pNext;

	int             m_iId;												// WEAPON_???
	bool            m_bIsBeingRemoved;
	bool            m_bIsNotAmmoItem;
	bool            m_bExclusiveHold;
	string_t        m_iszCustomSpriteDir;
	string_t        m_iszWorldModel;

#ifndef CLIENT_DLL
	float           m_flLastBounce;
#endif

	virtual int iItemSlot( void ) { return 0; }			// return 0 to MAX_ITEMS_SLOTS, used in hud

	int			iItemPosition( void ) { return ItemInfoArray[ m_iId ].iPosition; }
	const char	*pszAmmo1( void )	{ return ItemInfoArray[ m_iId ].pszAmmo1; }
	int			iMaxAmmo1( void )	{ return ItemInfoArray[ m_iId ].iMaxAmmo1; }
	const char	*pszAmmo2( void )	{ return ItemInfoArray[ m_iId ].pszAmmo2; }
	int			iMaxAmmo2( void )	{ return ItemInfoArray[ m_iId ].iMaxAmmo2; }
	const char	*pszName( void )	{ return ItemInfoArray[ m_iId ].pszName; }
	int			iMaxClip( void )	{ return ItemInfoArray[ m_iId ].iMaxClip; }
	int			iWeight( void )		{ return ItemInfoArray[ m_iId ].iWeight; }
	int			iFlags( void )		{ return ItemInfoArray[ m_iId ].iFlags; }

	// int		m_iIdPrimary;										// Unique Id for primary ammo
	// int		m_iIdSecondary;										// Unique Id for secondary ammo
};


// inventory items that 
class CBasePlayerWeapon : public CBasePlayerItem
{
public:
	void OnCreate( void );
	void KeyValue( KeyValueData *data );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];

	// generic weapon versions of CBasePlayerItem calls
	virtual bool AddToPlayer( CBasePlayer *pPlayer );
	virtual bool AddDuplicate( CBasePlayerItem *pItem );
	BOOL AddAmmoFromItem( CBasePlayerItem *pItem );
	virtual BOOL ExtractAmmoFromItem( CBasePlayerItem *pItem );

	virtual int ExtractAmmo( CBasePlayerWeapon *pWeapon ); //{ return TRUE; };			// Return TRUE if you can add ammo to yourself when picked up
	virtual int ExtractClipAmmo( CBasePlayerWeapon *pWeapon );// { return TRUE; };			// Return TRUE if you can add ammo to yourself when picked up

	virtual int AddWeapon( void ) { ExtractAmmo( this ); return TRUE; };	// Return TRUE if you want to add yourself to the player

	virtual int GetAmmo1Drop( void );
	virtual int GetAmmo2Drop( void );

	// generic "shared" ammo handlers
	int AddPrimaryAmmo( int iCount, char *szName, int iMaxClip, int iMaxCarry, int *amountGiven = NULL );
	int AddSecondaryAmmo( int iCount, char *szName, int iMaxCarry, int *amountGiven = NULL );

	virtual void UpdateItemInfo( void ) {};	// updates HUD state

	int  m_iBody;
	bool m_bPlayEmptySound;
	int  m_fFireOnEmpty;		// True when the gun is empty and the player is still holding down the
							// attack key(s)
	virtual bool PlayEmptySound( void );
	virtual void ResetEmptySound( void );

	virtual void SendWeaponAnim( int iAnim, int skiplocal = 1, int body = 0 );  // skiplocal is 1 if client is predicting weapon animations
	virtual Vector BulletAccuracy( const Vector &vecMoving, const Vector &vecStanding, const Vector &vecCrouched );

	virtual bool CanDeploy( void );
	virtual bool IsUseable( void );
	bool DefaultDeploy(const char *szViewModel, const char *szWeaponModel, int iAnim, const char *szAnimExt, int skiplocal = 0, int body = 0 );
	bool DefaultReload( int iClipSize, int iAnim, float fDelay, int body = 0 );
	bool DefaultReload( int iClipSize, int iAnim, float fDelay, int body, int currentClip, int ammoIndex );
	void ReloadClip( int iClipSize, int &iClip, int iAmmoType );

	virtual void ItemPostFrame( void );	// called each frame by the player PostThink
	// called by CBasePlayerWeapons ItemPostFrame()
	virtual void PrimaryAttack( void ) { return; }				// do "+ATTACK"
	virtual void SecondaryAttack( void ) { return; }			// do "+ATTACK2"
	virtual void TertiaryAttack( void ) {}
	virtual void Reload( void );								// do "+RELOAD"
	virtual void FinishReload( void );
	virtual bool ShouldReload( void );
	virtual void WeaponIdle( void ) { return; }					// called when no buttons pressed
	virtual bool UpdateClientData( CBasePlayer *pPlayer );      // sends hud info to client dll, if things have changed
	virtual void RetireWeapon( void );
	virtual bool ShouldWeaponIdle( void ) {return FALSE; };
	virtual void Holster( int skiplocal = 0 );
	virtual bool UseDecrement( void ) { return FALSE; };
	virtual void BurstSupplement( void );

	int	PrimaryAmmoIndex(); 
	int	SecondaryAmmoIndex(); 

	void PrintState( void );

	virtual CBasePlayerItem *GetWeaponPtr( void ) { return (CBasePlayerItem *)this; };
	float GetNextAttackDelay( float delay );

	virtual const char *GetP_Model( const char *model = NULL );
	virtual const char *GetW_Model( const char *model = NULL );
	virtual const char *GetV_Model( const char *model = NULL );
	virtual void PrecacheCustomModels( void );

	virtual void GetWeaponData( weapon_data_s *data ) {}
	virtual void SetWeaponData( weapon_data_s *data ) {}
	virtual bool IsMultiplayer( void );
	virtual bool FRunfuncs( void );
	virtual void SetFOV( int fov );
	virtual bool FCanRun( void );
	virtual void CustomDecrement( float flDecrement ) {}
	virtual void SetV_Model( const char *model );
	virtual void SetP_Model( const char *model );
	virtual void ChangeWeaponSkin( short skin );
	void AddToPlayerMessage( void );

	float m_flPumpTime;
	int		m_fInSpecialReload;									// Are we in the middle of a reload for the shotguns
	float	m_flNextPrimaryAttack;								// soonest time ItemPostFrame will call PrimaryAttack
	float	m_flNextSecondaryAttack;							// soonest time ItemPostFrame will call SecondaryAttack
	float	m_flNextTertiaryAttack;
	float	m_flTimeWeaponIdle;									// soonest time ItemPostFrame will call WeaponIdle
	int		m_iPrimaryAmmoType;									// "primary" ammo index into players m_rgAmmo[]
	int		m_iSecondaryAmmoType;								// "secondary" ammo index into players m_rgAmmo[]
	int		m_iClip;											// number of shots left in the primary weapon clip, -1 it not used
	int		m_iClip2;
	int		m_iClientClip;										// the last version of m_iClip sent to hud dll
	int		m_iClientClip2;
	int		m_iClientWeaponState;								// the last version of the weapon state sent to hud dll (is current weapon, is on target)
	int		m_fInReload;										// Are we in the middle of a reload;

	int m_iDefaultAmmo;// how much ammo you get when you pick up this weapon as placed by a level designer.
	int m_iDefaultSecAmmo;
	
	// hle time creep vars
	float	m_flPrevPrimaryAttack;
	float	m_flLastFireTime;			

	float m_flCustomDmg;
	bool  m_fIsAkimbo;
	bool  m_fInZoom;

	union
	{
		int m_iSpecialState;
		int m_iGrenadeReloadAnim;
	};

	unsigned short m_usBurstFire;

#ifndef CLIENT_DLL
	float m_flNextBurstRound;
#endif

};


class CBasePlayerAmmo : public CBasePickupObject
{
public:
	void OnCreate( void );
	void Collect( CBaseEntity *other, COLLECT_TYPE collectType );

	virtual void Spawn( void );
	void EXPORT DefaultTouch( CBaseEntity *pOther ); // default weapon touch
	virtual BOOL AddAmmo( CBaseEntity *pOther ) { return TRUE; };

	CBaseEntity* Respawn( void );
	void Dematerialize( void );
};


extern DLL_GLOBAL	short	g_sModelIndexLaser;// holds the index for the laser beam
extern DLL_GLOBAL	const char *g_pModelNameLaser;

extern DLL_GLOBAL	short	g_sModelIndexLaserDot;// holds the index for the laser beam dot
extern DLL_GLOBAL	short	g_sModelIndexFireball;// holds the index for the fireball
extern DLL_GLOBAL	short	g_sModelIndexSmoke;// holds the index for the smoke cloud
extern DLL_GLOBAL short g_sModelIndexWeaponSmoke;
extern DLL_GLOBAL	short	g_sModelIndexWExplosion;// holds the index for the underwater explosion
extern DLL_GLOBAL	short	g_sModelIndexBubbles;// holds the index for the bubbles model
extern DLL_GLOBAL	short	g_sModelIndexBloodDrop;// holds the sprite index for blood drops
extern DLL_GLOBAL	short	g_sModelIndexBloodSpray;// holds the sprite index for blood spray (bigger)

extern void ClearMultiDamage(void);
extern void ApplyMultiDamage(entvars_t* pevInflictor, entvars_t* pevAttacker );
extern void AddMultiDamage( entvars_t *pevInflictor, CBaseEntity *pEntity, float flDamage, int bitsDamageType);

extern void DecalGunshot( TraceResult *pTrace, int iBulletType );
extern void SpawnBlood(Vector vecSpot, int bloodColor, float flDamage);
extern int DamageDecal( CBaseEntity *pEntity, int bitsDamageType );
extern void RadiusDamage( Vector vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, float flRadius, int iClassIgnore, int bitsDamageType );

typedef struct 
{
	CBaseEntity		*pEntity;
	float			amount;
	int				type;
} MULTIDAMAGE;

extern MULTIDAMAGE gMultiDamage;


#define LOUD_GUN_VOLUME			1536
#define NORMAL_GUN_VOLUME		1024
#define QUIET_GUN_VOLUME		200

#define	BRIGHT_GUN_FLASH		512
#define NORMAL_GUN_FLASH		256
#define	DIM_GUN_FLASH			128

#define BIG_EXPLOSION_VOLUME	2048
#define NORMAL_EXPLOSION_VOLUME	1024
#define SMALL_EXPLOSION_VOLUME	512

#define	WEAPON_ACTIVITY_VOLUME	64

#define VECTOR_CONE_1DEGREES	Vector( 0.00873, 0.00873, 0.00873 )
#define VECTOR_CONE_2DEGREES	Vector( 0.01745, 0.01745, 0.01745 )
#define VECTOR_CONE_3DEGREES	Vector( 0.02618, 0.02618, 0.02618 )
#define VECTOR_CONE_4DEGREES	Vector( 0.03490, 0.03490, 0.03490 )
#define VECTOR_CONE_5DEGREES	Vector( 0.04362, 0.04362, 0.04362 )
#define VECTOR_CONE_6DEGREES	Vector( 0.05234, 0.05234, 0.05234 )
#define VECTOR_CONE_7DEGREES	Vector( 0.06105, 0.06105, 0.06105 )
#define VECTOR_CONE_8DEGREES	Vector( 0.06976, 0.06976, 0.06976 )
#define VECTOR_CONE_9DEGREES	Vector( 0.07846, 0.07846, 0.07846 )
#define VECTOR_CONE_10DEGREES	Vector( 0.08716, 0.08716, 0.08716 )
#define VECTOR_CONE_15DEGREES	Vector( 0.13053, 0.13053, 0.13053 )
#define VECTOR_CONE_20DEGREES	Vector( 0.17365, 0.17365, 0.17365 )

//=========================================================
// CWeaponBox - a single entity that can store weapons
// and ammo. 
//=========================================================
class CWeaponBox : public CBasePickupObject
{
	void Precache( void );
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	bool CanCollect( CBaseEntity *other, COLLECT_TYPE collectType );
	void Collect( CBaseEntity *other, COLLECT_TYPE collectType );
	void Dematerialize( void );
	bool IsEmpty( void );
	int  GiveAmmo( int iCount, const char *szName, int iMax, int *pIndex = NULL );
	void SetObjectCollisionBox( void );
	int ObjectCaps( void ) { return FCAP_IMPULSE_USE; }
	bool CriticalRemove( void );

public:
	void OnCreate( void );
	void EXPORT Kill ( void );
	int		Save( CSave &save );
	int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	static CWeaponBox *CreateWeaponBox( const Vector &origin, const Vector &angles, const Vector &direction, edict_t *owner );
	bool HasWeapon( const char *classname );
	bool PackWeapon( CBasePlayerItem *pWeapon );
	bool PackAmmo( int iszName, int iCount );
	
	CEntityHandle<CBasePlayerItem> m_rgpPlayerItems[MAX_ITEM_TYPES];// one slot for each
	CEntityHandle<CBasePlayerItem> m_hExclusiveHoldItem;

	int	m_rgiszAmmo[MAX_AMMO_SLOTS];// ammo names
	int	m_rgAmmo[MAX_AMMO_SLOTS];// ammo quantities

	int m_cAmmoTypes;// how many ammo types packed into this box (if packed by a level designer)
};

void PackWeapons( CBasePlayer *player, CWeaponBox *box );
void PackAmmo( CBasePlayer *player, CWeaponBox *box );

#ifdef CLIENT_DLL
bool bIsMultiplayer ( void );
void LoadVModel ( const char *szViewModel, CBasePlayer *m_hPlayer );
#endif

class CGlock : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 2; }
	bool GetItemInfo( ItemInfo *p );
	bool AddToPlayer( CBasePlayer *player );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void GlockFire( float flSpread, float flCycleTime, bool fUseAutoAim );
	bool Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );

	void GetWeaponData( weapon_data_s *data );
	void SetWeaponData( weapon_data_s *data );

	bool UseDecrement( void ) { return true; }

private:
	int m_iShell;

	unsigned short m_usFireGlock1;
	unsigned short m_usFireGlock2;
};


class CCrowbar : public CBasePlayerWeapon
{
public:
	float GetRespawnTime( void ) { return 3; }
	void OnCreate( void );
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 1; }
	void EXPORT SwingAgain( void );
	void EXPORT Smack( void );
	bool GetItemInfo(ItemInfo *p);

	bool AddToPlayer( CBasePlayer *player );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void TertiaryAttack( void );
	void WeaponIdle( void );
	void ItemPreFrame( void );
	void TurnElectricOn( void );
	void TurnElectricOff( void );
	void ToggleElectric( void );
	void LaunchCrowbar( void );
	void EXPORT ElectricSmack( void );
	void EXPORT ThrownThink( void );
	void EXPORT ThrownTouch( CBaseEntity *other );
	bool Swing( bool fFirst );
	bool Deploy( void );
	void Holster( int skiplocal = 0 );

	int         m_iSwing;
	TraceResult m_trHit;

private:
	friend class CBaseDoor;
	friend class CBreakable;

	bool  m_fElectric;
	bool  m_fThrowPending;
	float m_flNextThrowSound;
	bool  m_fThrowActive;
};

class CPython : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 2; }
	bool GetItemInfo(ItemInfo *p);
	bool AddToPlayer( CBasePlayer *pPlayer );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	bool Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );

	void GetWeaponData( weapon_data_s *data );
	void SetWeaponData( weapon_data_s *data );

	bool UseDecrement( void ) { return true; }

private:
	unsigned short m_usFirePython;
};

class CMP5 : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 3; }
	bool GetItemInfo(ItemInfo *p);
	bool AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void ClassicPrimaryAttack( void );
	void SecondaryAttack( void );
	void ClassicSecondaryAttack( void );
	int SecondaryAmmoIndex( void );
	bool Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );
	void GetWeaponData( weapon_data_s *data );
	void SetWeaponData( weapon_data_s *data );

	bool UseDecrement( void ) { return true; }

	float m_flNextAnimTime;
	int   m_iShell;

private:
	unsigned short m_usMP5;
};

class CCrossbow : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( ) { return 3; }
	bool GetItemInfo(ItemInfo *p);

	void FireBolt( void );
	void FireSniperBolt( void );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	bool AddToPlayer( CBasePlayer *pPlayer );
	bool Deploy( );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );

	void GetWeaponData( weapon_data_s *data );
	void SetWeaponData( weapon_data_s *data );

	bool UseDecrement( void ) { return true; }

};

#include "shotgun.h"

class CRPGSpot : public CBaseEntity
{
	void Spawn( void );
	void Precache( void );

	int	ObjectCaps( void ) { return FCAP_DONT_SAVE; }

public:
	void Suspend( float flSuspendTime );
	void EXPORT Revive( void );
	
	static CRPGSpot *CreateSpot( void );
};

class CRpg : public CBasePlayerWeapon
{
public:

#ifndef CLIENT_DLL
	int		Save( CSave &save );
	int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
#endif

	void Spawn( void );
	void Precache( void );
	void Reload( void );
	int iItemSlot( void ) { return 4; }
	bool GetItemInfo(ItemInfo *p);
	bool AddToPlayer( CBasePlayer *pPlayer );

	bool Deploy( void );
	bool CanHolster( void );
	void Holster( int skiplocal = 0 );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void WeaponIdle( void );

	void UpdateSpot( void );
	bool ShouldWeaponIdle( void ) { return TRUE; };

	void GetWeaponData( weapon_data_s *data );
	void SetWeaponData( weapon_data_s *data );

	CEntityHandle<CRPGSpot> m_hSpot;
	int m_fSpotActive;
	int m_cActiveRockets;// how many missiles in flight from this launcher right now?

	bool UseDecrement( void ) { return true; }


};

class CRpgRocket : public CGrenade
{
public:
	int		Save( CSave &save );
	int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
	void Spawn( void );
	void Precache( void );
	void EXPORT FollowThink( void );
	void EXPORT IgniteThink( void );
	void EXPORT RocketTouch( CBaseEntity *pOther );
	static CRpgRocket *CreateRpgRocket( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, CRpg *pLauncher );

	int		m_iTrail;
	float	m_flIgniteTime;
#ifdef CLIENT_DLL
	CRpg	*m_pLauncher;
#else
	EHANDLE	m_hLauncher;
#endif
	float m_flCustomDmg;
};

#include "gauss.h"

#include "egon.h"

#include "hornetgun.h"



class CHandGrenade : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 5; }
	bool GetItemInfo( ItemInfo *p );
	bool CanHaveDuplicates( void ) { return true; }
	const char *GetBounceSound( void ) { return "weapons/grenade_hit3.wav"; }

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void TertiaryAttack( void );
	bool Deploy( void );
	bool CanHolster( void );
	void Holster( int skiplocal = 0 );
	void WeaponIdle( void );
	void CustomDecrement( float flDecrement );

	void GetWeaponData( weapon_data_s *data );
	void SetWeaponData( weapon_data_s *data );

	bool UseDecrement( void ) { return true; }

	bool  m_fStartThrowActive;
	float m_flStartThrow;
	bool  m_fReleaseThrowActive;
	float m_flReleaseThrow;
	bool  m_fThrowAll; // throw all ten grenades at once
};

class CSatchel : public CBasePlayerWeapon
{
public:

#ifndef CLIENT_DLL
	int		Save( CSave &save );
	int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
#endif

	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 5; }
	bool GetItemInfo( ItemInfo *p );
	bool CanHaveDuplicates( void ) { return true; }
	bool AddToPlayer( CBasePlayer *pPlayer );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	bool AddDuplicate( CBasePlayerItem *pOriginal );
	bool CanDeploy( void );
	bool Deploy( void );
	bool IsUseable( void );
	
	void Holster( int skiplocal = 0 );
	void WeaponIdle( void );
	void Throw( void );
	void GetWeaponData( weapon_data_s *data );
	void SetWeaponData( weapon_data_s *data );
	const char *GetBounceSound( void ) { return "weapons/g_bounce1.wav"; }

	bool UseDecrement( void ) { return true; }

	int m_chargeReady;
};


class CTripmine : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 5; }
	bool GetItemInfo( ItemInfo *p );
	bool CanHaveDuplicates( void ) { return true; }
	void SetObjectCollisionBox( void )
	{
		pev->absmin = pev->origin + Vector( -16, -16, -5 );
		pev->absmax = pev->origin + Vector( 16, 16, 28 );
	}
	void PrimaryAttack( void );
	bool Deploy( void );
	void Holster( int skiplocal = 0 );
	void WeaponIdle( void );

};

class CSqueak : public CBasePlayerWeapon
{
public:
	void OnCreate( void );
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 5; }
	bool GetItemInfo( ItemInfo *p );
	bool CanHaveDuplicates( void ) { return true; }

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	bool Deploy( void );
	void Holster( int skiplocal = 0 );
	void WeaponIdle( void );
	int m_fJustThrown;


};


#include "displacer.h"
#include "sporelauncher.h"
#include "minigun.h"
#include "uzi.h"
#include "m16.h"
#include "desert_eagle.h"
#include "pipewrench.h"
#include "medkit.h"

#endif // CLIENT_DLL
#endif // WEAPONS_H
