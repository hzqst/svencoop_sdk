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
//
// Client side weapon prediction classes. dlls/weapons.h includes
// this file instead of its own contents when CLIENT_DLL is defined
//
#ifndef HL_WEAPONS_H
#define HL_WEAPONS_H

#include "weapon_animations.h"

#include "pmtrace.h"
#include "../ev_hldm.h"
#include "CClassicModeWrapper.h"
#include "weapon_iteminfo.h"

class CBasePlayer;
class CBasePlayerItem;
struct weapon_data_s;

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
#define WEAPON_UZI				17
#define WEAPON_MINIGUN			21
#define WEAPON_SNIPERRIFLE		23
#define WEAPON_M249				24
#define WEAPON_M16				25
#define WEAPON_SPORELAUNCHER	26
#define WEAPON_EAGLE			27
#define WEAPON_SHOCKRIFLE		28
#define WEAPON_DISPLACER		29
#define WEAPON_FLAMETHROWER		30

#define WEAPON_ALLWEAPONS		(~(1<<WEAPON_SUIT))

#define WEAPON_SUIT				31	// ?????

#define MAX_WEAPONS			256


#define MAX_NORMAL_BATTERY	100


// weapon weight factors (for auto-switching)   (-1 = noswitch)
#define GLOCK_WEIGHT			10
#define PYTHON_WEIGHT			15
#define MP5_WEIGHT				15
#define SHOTGUN_WEIGHT			15
#define CROSSBOW_WEIGHT			10
#define RPG_WEIGHT				20
#define GAUSS_WEIGHT			20
#define HORNETGUN_WEIGHT		10
#define HANDGRENADE_WEIGHT		5
#define SATCHEL_WEIGHT			-10
#define UZI_WEIGHT				15
#define UZI_AKIMBO_WEIGHT		20
#define MINIGUN_WEIGHT			25
#define SNIPERRIFLE_WEIGHT		10
#define M249_WEIGHT				20
#define M16_WEIGHT				15
#define SPORELAUNCHER_WEIGHT	15
#define EAGLE_WEIGHT			15
#define SHOCKRIFLE_WEIGHT		25
#define DISPLACER_WEIGHT		20
#define FLAMETHROWER_WEIGHT		20


// weapon clip/carry ammo capacities
#define URANIUM_MAX_CARRY		100
#define	_9MM_MAX_CARRY			250
#define _357_MAX_CARRY			36
#define BUCKSHOT_MAX_CARRY		125
#define BOLT_MAX_CARRY			50
#define ROCKET_MAX_CARRY		5
#define HANDGRENADE_MAX_CARRY	10
#define SATCHEL_MAX_CARRY		5
#define HORNET_MAX_CARRY		100
#define M203_GRENADE_MAX_CARRY	10
#define _556_MAX_CARRY			600
#define _762_MAX_CARRY			15
#define SPORE_MAX_CARRY			30
#define SHOCK_MAX_CARRY			100
#define FUEL_MAX_CARRY			300

// the maximum amount of ammo each weapon's clip can hold
#define WEAPON_NOCLIP			-1

#define GLOCK_MAX_CLIP			17
#define PYTHON_MAX_CLIP			6
#define MP5_MAX_CLIP			30
#define SHOTGUN_MAX_CLIP		8
#define CROSSBOW_MAX_CLIP		5
#define RPG_MAX_CLIP			1
#define GAUSS_MAX_CLIP			WEAPON_NOCLIP
#define HORNETGUN_MAX_CLIP		WEAPON_NOCLIP
#define HANDGRENADE_MAX_CLIP	WEAPON_NOCLIP
#define SATCHEL_MAX_CLIP		WEAPON_NOCLIP
#define UZI_MAX_CLIP			32
#define MINIGUN_MAX_CLIP		WEAPON_NOCLIP
#define SNIPERRIFLE_MAX_CLIP	5
#define M249_MAX_CLIP			200
#define M16_MAX_CLIP			30
#define SPORELAUNCHER_MAX_CLIP	8
#define EAGLE_MAX_CLIP			7
#define SHOCKRIFLE_MAX_CLIP		WEAPON_NOCLIP
#define DISPLACER_MAX_CLIP		WEAPON_NOCLIP
#define FLAMETHROWER_MAX_CLIP	WEAPON_NOCLIP

// the default amount of ammo that comes with each gun when it spawns
#define GLOCK_DEFAULT_GIVE			17
#define PYTHON_DEFAULT_GIVE			6
#define MP5_DEFAULT_GIVE			60
#define MP5_CLASSIC_DEFAULT_GIVE	75
#define CROSSBOW_DEFAULT_GIVE		5
#define SHOTGUN_DEFAULT_GIVE		12
#define RPG_DEFAULT_GIVE			1
#define GAUSS_DEFAULT_GIVE			20
#define HIVEHAND_DEFAULT_GIVE		100
#define SATCHEL_DEFAULT_GIVE		1
#define HANDGRENADE_DEFAULT_GIVE	5
#define UZI_DEFAULT_GIVE			32
#define M16_DEFAULT_GIVE			60
#define M249_DEFAULT_GIVE			200
#define SNIPERRIFLE_DEFAULT_GIVE	5
#define MINIGUN_DEFAULT_GIVE		400
#define SHOCKRIFLE_DEFAULT_GIVE		100
#define SPORELAUNCHER_DEFAULT_GIVE	8
#define DISPLACER_DEFAULT_GIVE		20
#define FLAMETHROWER_DEFAULT_GIVE	100
#define EAGLE_DEFAULT_GIVE			7

// The amount of ammo given to a player by an ammo item.
#define AMMO_URANIUMBOX_GIVE	20
#define AMMO_GLOCKCLIP_GIVE		GLOCK_MAX_CLIP
#define AMMO_357BOX_GIVE		PYTHON_MAX_CLIP
#define AMMO_MP5CLIP_GIVE		MP5_MAX_CLIP
#define AMMO_CHAINBOX_GIVE		200
#define AMMO_M203BOX_GIVE		2
#define AMMO_BUCKSHOTBOX_GIVE	12
#define AMMO_CROSSBOWCLIP_GIVE	CROSSBOW_MAX_CLIP
#define AMMO_RPGCLIP_GIVE		RPG_MAX_CLIP
#define AMMO_UZICLIP_GIVE		UZI_MAX_CLIP
#define AMMO_556CLIP_GIVE		M16_MAX_CLIP
#define AMMO_556_GIVE			100
#define AMMO_762BOX_GIVE		SNIPERRIFLE_MAX_CLIP
#define AMMO_SPORE_GIVE			1

// The amount of ammo a weapon leaves behind when it is dropped (ItemInfo::iAmmo1Drop/iAmmo2Drop)
#define AMMO_DROP_NONE			-1
#define GLOCK_AMMO1_DROP		AMMO_GLOCKCLIP_GIVE
#define PYTHON_AMMO1_DROP		AMMO_357BOX_GIVE
#define MP5_AMMO2_DROP			1
#define SHOTGUN_AMMO1_DROP		SHOTGUN_MAX_CLIP
#define CROSSBOW_AMMO1_DROP		AMMO_CROSSBOWCLIP_GIVE
#define RPG_AMMO1_DROP			AMMO_RPGCLIP_GIVE
#define GAUSS_AMMO1_DROP		AMMO_URANIUMBOX_GIVE
#define HANDGRENADE_AMMO1_DROP	1
#define SATCHEL_AMMO1_DROP		1
#define UZI_AMMO1_DROP			AMMO_UZICLIP_GIVE
#define MINIGUN_AMMO1_DROP		AMMO_556_GIVE
#define SNIPERRIFLE_AMMO1_DROP	AMMO_762BOX_GIVE
#define M249_AMMO1_DROP			M249_MAX_CLIP
#define M16_AMMO1_DROP			AMMO_556CLIP_GIVE
#define M16_AMMO2_DROP			1
#define SPORELAUNCHER_AMMO1_DROP	AMMO_SPORE_GIVE
#define EAGLE_AMMO1_DROP		AMMO_357BOX_GIVE
#define DISPLACER_AMMO1_DROP	AMMO_URANIUMBOX_GIVE
#define FLAMETHROWER_AMMO1_DROP	75

#define ITEM_FLAG_SELECTONEMPTY		1
#define ITEM_FLAG_NOAUTORELOAD		2
#define ITEM_FLAG_NOAUTOSWITCHEMPTY	4
#define ITEM_FLAG_LIMITINWORLD		8
#define ITEM_FLAG_EXHAUSTIBLE		16 // A player can totally exhaust their ammo supply and lose this weapon
#define ITEM_FLAG_DUALWIELD			32 // Use a special HUD ammo configuration unique to a dual wielded weapons
#define ITEM_FLAG_IGNOREWEAPONSTAY	64 // Ignore weaponstay settings when respawning this weapon
#define ITEM_FLAG_ESSENTIAL			128 // Only dropped when the player is gibbed or respawns

#define WEAPON_IS_ONTARGET 0x40

// EV_WeaponCustom iparam1
#define WEAPONCUSTOM_EMPTY_SOUND	1
#define WEAPONCUSTOM_SOUND			2

// EV_WeaponCustom fparam1 for WEAPONCUSTOM_SOUND, indices into its sound table
#define WEAPONCUSTOM_SND_357_COCK1				0
#define WEAPONCUSTOM_SND_ROCKETFIRE1			1
#define WEAPONCUSTOM_SND_GLAUNCHER				2
#define WEAPONCUSTOM_SND_GLAUNCHER2				3
#define WEAPONCUSTOM_SND_357_RELOAD1			4
#define WEAPONCUSTOM_SND_XBOW_FIRE1				5
#define WEAPONCUSTOM_SND_XBOW_RELOAD1			6
#define WEAPONCUSTOM_SND_NULL					7
#define WEAPONCUSTOM_SND_ELECTRO4				8
#define WEAPONCUSTOM_SND_ELECTRO5				9
#define WEAPONCUSTOM_SND_ELECTRO6				10
#define WEAPONCUSTOM_SND_AG_FIRE1				11
#define WEAPONCUSTOM_SND_AG_FIRE2				12
#define WEAPONCUSTOM_SND_AG_FIRE3				13
#define WEAPONCUSTOM_SND_DBARREL1				14
#define WEAPONCUSTOM_SND_SBARREL1				15
#define WEAPONCUSTOM_SND_RELOAD1				16
#define WEAPONCUSTOM_SND_RELOAD3				17
#define WEAPONCUSTOM_SND_BBCOVER				18
#define WEAPONCUSTOM_SND_SCOCK1					19
#define WEAPONCUSTOM_SND_SNIPER_ZOOM			20
#define WEAPONCUSTOM_SND_DESERT_EAGLE_SIGHT		21
#define WEAPONCUSTOM_SND_DESERT_EAGLE_SIGHT2	22
#define WEAPONCUSTOM_SND_SPLAUNCHER_FIRE		23
#define WEAPONCUSTOM_SND_SPLAUNCHER_ALTFIRE		24

// Pickup collection modes
typedef enum
{
	COLLECT_TOUCH = 0,
	COLLECT_USE
} COLLECT_TYPE;

// CBasePhysicsObject::m_iBounceMaterial, picks the bounce sound on the server
#define BOUNCE_MATERIAL_NONE	0
#define BOUNCE_MATERIAL_METAL	1
#define BOUNCE_MATERIAL_FLESH	2
#define BOUNCE_MATERIAL_GLASS	3
#define BOUNCE_MATERIAL_WOOD	4
#define BOUNCE_MATERIAL_WEAPON	5

#define SF_ITEM_TOUCH_ONLY		128

// Base for objects that fall and bounce. The physics run on the server
class CBasePhysicsObject : public CBaseAnimating
{
public:
	virtual void	Materialize( void ) { }
	virtual void	TouchedWorld( bool fFirstTouch ) { }
	virtual const char	*GetBounceSound( void ) { return NULL; }

	int				m_iBounceMaterial;
};

// Base for collectable objects. Collection runs on the server, but this
// still builds these virtuals into the client weapons
class CBasePickupObject : public CBasePhysicsObject
{
public:
	virtual int		ObjectCaps( void );
	virtual void	OnCreate( void ) { }
	virtual void	Dropped( CBaseEntity *pDropper ) { }
	virtual bool	CanCollect( CBaseEntity *pOther, COLLECT_TYPE collectType ) { return true; }
	virtual void	Collect( CBaseEntity *pOther, COLLECT_TYPE collectType ) { }
	virtual void	Collected( CBaseEntity *pOther, COLLECT_TYPE collectType ) { }
};

inline int CBasePickupObject::ObjectCaps( void )
{
	if ( pev->effects & EF_NODRAW )
		return FCAP_ACROSS_TRANSITION;

	if ( pev->spawnflags & SF_ITEM_TOUCH_ONLY )
		return FCAP_ACROSS_TRANSITION;

	return FCAP_ACROSS_TRANSITION | FCAP_IMPULSE_USE;
}

// Items that the player has in their inventory that they can use
class CBasePlayerItem : public CBasePickupObject
{
public:
	virtual bool	AddToPlayer( CBasePlayer *pPlayer ) { return false; }	// return TRUE if the item you want the item added to the player inventory
	virtual bool	AddDuplicate( CBasePlayerItem *pItem ) { return false; }	// return TRUE if you want your duplicate removed from world
	virtual bool	AddAmmoFromItem( CBasePlayerItem *pItem ) { return false; }
	virtual const char	*GetPickupSound( void ) { return NULL; }
	virtual bool	GetItemInfo( ItemInfo *p ) { return false; }	// returns 0 if struct not filled out
	virtual bool	CanDeploy( void ) { return true; }
	virtual bool	Deploy( void ) { return true; }		// returns is deploy was successful
	virtual bool	CanHolster( void ) { return true; }	// can this weapon be put away right now?
	virtual void	Holster( int skiplocal = 0 ) { }
	virtual void	UpdateItemInfo( void ) { }

	virtual void	ItemPreFrame( void ) { }			// called each frame by the player PreThink
	virtual void	ItemPostFrame( void ) { }			// called each frame by the player PostThink
	virtual void	InactiveItemPreFrame( void ) { }
	virtual void	InactiveItemPostFrame( void ) { }

	virtual int		PrimaryAmmoIndex( void ) { return -1; }
	virtual int		SecondaryAmmoIndex( void ) { return -1; }

	virtual bool	UpdateClientData( CBasePlayer *pPlayer ) { return false; }

	virtual CBasePlayerItem *GetWeaponPtr( void ) { return NULL; }
	virtual CBasePlayerItem *MyItemPointer( void ) { return this; }

	virtual int		ObjectCaps( void );
	virtual int		iItemSlot( void ) { return 0; }	// return 0 to MAX_ITEMS_SLOTS, used in hud
	virtual float	GetRespawnTime( void ) { return 20; }
	virtual CBasePlayerItem *DropItem( void ) { return NULL; }
	virtual bool	CanHaveDuplicates( void ) { return false; }

	virtual CBaseEntity *Respawn( void ) { return NULL; }
	virtual bool	CanCollect( CBaseEntity *pOther, COLLECT_TYPE collectType ) { return true; }
	virtual void	Collect( CBaseEntity *pOther, COLLECT_TYPE collectType ) { }
	const char		*CheckModel( const char *szModel ) { return szModel; }
	void			FallInit( void ) { }
	void			DestroyItem( void ) { }

	static ItemInfo	ItemInfoArray[ MAX_WEAPONS ];
	static AmmoInfo	AmmoInfoArray[ MAX_AMMO_SLOTS ];

	CBasePlayer		*m_hPlayer;
	CBasePlayerItem	*m_pNext;
	int				m_iId;					// WEAPON_???
	float			m_flLastBounce;
	string_t		m_iszWorldModel;
	bool			m_bExclusiveHold;		// the player must keep this weapon selected while carrying it

	int			iItemPosition( void ) { return ItemInfoArray[ m_iId ].iPosition; }
	const char	*pszAmmo1( void ) { return ItemInfoArray[ m_iId ].pszAmmo1; }
	int			iMaxAmmo1( void ) { return ItemInfoArray[ m_iId ].iMaxAmmo1; }
	const char	*pszAmmo2( void ) { return ItemInfoArray[ m_iId ].pszAmmo2; }
	int			iMaxAmmo2( void ) { return ItemInfoArray[ m_iId ].iMaxAmmo2; }
	const char	*pszName( void ) { return ItemInfoArray[ m_iId ].pszName; }
	int			iMaxClip( void ) { return ItemInfoArray[ m_iId ].iMaxClip; }
	int			iWeight( void ) { return ItemInfoArray[ m_iId ].iWeight; }
	int			iFlags( void ) { return ItemInfoArray[ m_iId ].iFlags; }
};

// inventory items that
class CBasePlayerWeapon : public CBasePlayerItem
{
public:
	// generic weapon versions of CBasePlayerItem calls
	virtual bool	CanDeploy( void );
	virtual void	Holster( int skiplocal = 0 );
	virtual void	ItemPostFrame( void );	// called each frame by the player PostThink

	virtual CBasePlayerItem *GetWeaponPtr( void ) { return (CBasePlayerItem *)this; }

	virtual bool	ExtractAmmo( CBasePlayerWeapon *pWeapon ) { return false; }		// Return TRUE if you can add ammo to yourself when picked up
	virtual bool	ExtractClipAmmo( CBasePlayerWeapon *pWeapon ) { return false; }	// Return TRUE if you can add ammo to yourself when picked up
	virtual bool	AddWeapon( void ) { ExtractAmmo( this ); return true; }		// Return TRUE if you want to add yourself to the player
	virtual int		GetAmmo1Drop( void ) { return 0; }
	virtual int		GetAmmo2Drop( void ) { return 0; }

	virtual bool	PlayEmptySound( void );
	virtual void	ResetEmptySound( void );

	virtual void	SendWeaponAnim( int iAnim, int skiplocal = 0, int body = 0 );	// skiplocal is 1 if client is predicting weapon animations
	virtual Vector	BulletAccuracy( const Vector &vecMoving, const Vector &vecStanding, const Vector &vecCrouched );

	virtual bool	IsUseable( void ) { return true; }

	// called by CBasePlayerWeapons ItemPostFrame()
	virtual void	PrimaryAttack( void ) { }		// do "+ATTACK"
	virtual void	SecondaryAttack( void ) { }		// do "+ATTACK2"
	virtual void	TertiaryAttack( void ) { }		// do "+ATTACK3"
	virtual void	Reload( void ) { }				// do "+RELOAD"
	virtual void	FinishReload( void );
	virtual bool	ShouldReload( void );
	virtual void	WeaponIdle( void ) { }			// called when no buttons pressed
	virtual void	RetireWeapon( void ) { }		// no more ammo for this gun, put it away
	virtual bool	ShouldWeaponIdle( void ) { return false; }
	virtual bool	UseDecrement( void ) { return false; }	// whether the client predicts this weapon
	virtual void	BurstSupplement( void );

	virtual const char	*GetP_Model( const char *szModel ) { return szModel; }
	virtual const char	*GetW_Model( const char *szModel ) { return szModel; }
	virtual const char	*GetV_Model( const char *szModel ) { return szModel; }
	virtual void	PrecacheCustomModels( void ) { }

	virtual void	GetWeaponData( weapon_data_s *data ) { }
	virtual void	SetWeaponData( weapon_data_s *data ) { }

	virtual bool	IsMultiplayer( void );
	virtual bool	FRunfuncs( void );
	virtual void	SetFOV( int fov );
	virtual bool	FCanRun( void );
	virtual void	CustomDecrement( float flDecrement ) { }
	virtual void	SetV_Model( const char *szViewModel );
	virtual void	SetP_Model( const char *szWeaponModel ) { }
	virtual void	ChangeWeaponSkin( short skin );

	void			AddToPlayerMessage( void ) { }
	bool			DefaultDeploy( const char *szViewModel, const char *szWeaponModel, int iAnim, const char *szAnimExt, int skiplocal = 0, int body = 0 );
	bool			DefaultReload( int iClipSize, int iAnim, float fDelay, int body = 0 );
	bool			DefaultReload( int iClipSize, int iAnim, float fDelay, int body, int iClip, int iAmmoType );
	void			ReloadClip( int iClipSize, int &iClip, int iAmmoType );

	void			PrintState( void );

	bool			m_bPlayEmptySound;
	bool			m_bFireOnEmpty;				// True when the gun is empty and the player is still holding down the attack key(s)

	float			m_flNextPrimaryAttack;		// soonest time ItemPostFrame will call PrimaryAttack
	float			m_flNextSecondaryAttack;	// soonest time ItemPostFrame will call SecondaryAttack
	float			m_flNextTertiaryAttack;		// soonest time ItemPostFrame will call TertiaryAttack
	float			m_flTimeWeaponIdle;			// soonest time ItemPostFrame will call WeaponIdle
	int				m_iPrimaryAmmoType;			// "primary" ammo index into players m_rgAmmo[]
	int				m_iSecondaryAmmoType;		// "secondary" ammo index into players m_rgAmmo[]
	int				m_iClip;					// number of shots left in the primary weapon clip, -1 it not used
	int				m_iClip2;					// number of shots left in the secondary weapon clip, -1 it not used

	bool			m_fInReload;				// Are we in the middle of a reload
	float			m_flCustomDmg;				// Per-entity weapon damage
	bool			m_fIsAkimbo;				// Weapon is in 'akimbo' mode
	bool			m_fInZoom;					// Weapon is zoomed in
	float			m_flNextBurstRound;			// When the next burst round is to be fired
	unsigned short	m_usBurstFire;

	int				m_iDefaultAmmo;				// how much ammo you get when you pick up this weapon as placed by a level designer.
	int				m_iDefaultSecAmmo;
};

class CBasePlayerAmmo : public CBasePickupObject
{
public:
	virtual bool	AddAmmo( CBaseEntity *pOther ) { return true; }
	virtual CBaseEntity *Respawn( void ) { return this; }
	virtual void	Collect( CBaseEntity *pOther, COLLECT_TYPE collectType ) { }
};

// Grenade logic runs on the server
class CGrenade : public CBaseMonster
{
public:
	void			Explode( TraceResult *pTrace, int bitsDamageType ) { }
};

// Ammo item that gives a fixed amount of one ammo type
class CPlayerAmmo : public CBasePlayerAmmo
{
public:
	CPlayerAmmo( int iAmmoGive, const char *pszAmmoName, int iMaxCarry );

	void	Spawn( void );
	void	Precache( void );
	bool	AddAmmo( CBaseEntity *pOther );

	virtual const char	*GetDefaultModel( void ) { return NULL; }
	virtual const char	*GetPickupSound( void );

	int				m_iAmmoGive;
	const char		*m_pszAmmoName;
	int				m_iMaxCarry;
};

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

#include "desert_eagle.h"

// CUzi::Fire modes. The right uzi fires from m_iClip, the left one from m_iClip2
#define UZI_FIRE_NONE		0
#define UZI_FIRE_SINGLE		1
#define UZI_FIRE_RIGHT		2
#define UZI_FIRE_LEFT		3
#define UZI_FIRE_BOTH		4

// CanHaveXtra extras
#define XTRA_GOLDEN_UZI		2

#define UZI_DAMAGE			5
#define UZI_GOLDEN_DAMAGE	8

// v_uzi.mdl skins
#define UZI_SKIN_DEFAULT	0
#define UZI_SKIN_GILDED		1

#include "uzi.h"

// rounds fired by one pull of the M16 trigger, counted down in pev->fuser2
#define M16_BURST_ROUNDS	3

#include "m16.h"

// v_saw.mdl belt submodels: one body per round left near the end of the box
#define M249_BELT_ROUNDS		15
#define M249_LEGACY_BELT_ROUNDS	7

class CM249 : public CBasePlayerWeapon
{
public:
	void	Spawn( void );
	void	Precache( void );
	int		iItemSlot( void ) { return 3; }
	bool	GetItemInfo( ItemInfo *p );
	bool	AddToPlayer( CBasePlayer *pPlayer );

	void	ItemPostFrame( void );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	int		SecondaryAmmoIndex( void );
	bool	Deploy( void );
	void	Holster( int skiplocal = 0 );
	void	Reload( void );
	bool	SpecialReload( int iClipSize, int iAnim, float fDelay, int body );
	void	WeaponIdle( void );

	int		GetBodyFromClip( void );
	bool	CheckForLegacyModel( void );

	void	GetWeaponData( weapon_data_s *data );
	void	SetWeaponData( weapon_data_s *data );

	bool	UseDecrement( void ) { return true; }

	int				m_iReloadEndAnim;	// second reload stage, played by WeaponIdle
	int				m_iShell;
	bool			m_fLegacyModel;
	unsigned short	m_usFireM249Legacy;
	unsigned short	m_usFireM249;
};

#define SNIPERRIFLE_ZOOM_FOV	20

#ifndef PFLAG_CAMERA
#define PFLAG_CAMERA			( 1<<7 )	// Looking through a trigger_camera
#endif

class CSniperRifle : public CBasePlayerWeapon
{
public:
	void	Spawn( void );
	void	Precache( void );
	int		iItemSlot( void ) { return 3; }
	bool	GetItemInfo( ItemInfo *p );
	bool	AddToPlayer( CBasePlayer *pPlayer );

	void	ItemPostFrame( void );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	int		SecondaryAmmoIndex( void );
	bool	Deploy( void );
	void	Holster( int skiplocal = 0 );
	void	Reload( void );
	bool	SpecialReload( int iClipSize, int iAnim, float fDelay, int fTwoStage );
	void	WeaponIdle( void );

	void	GetWeaponData( weapon_data_s *data );
	void	SetWeaponData( weapon_data_s *data );

	bool	UseDecrement( void ) { return true; }

	int				m_iReloadEndAnim;	// second reload stage, played by WeaponIdle
	int				m_iShell;
	unsigned short	m_usSniper;
};

// CMinigun::SetPlayerSpeed states
#define MINIGUN_SPEED_NONE		0
#define MINIGUN_SPEED_FIRE		1
#define MINIGUN_SPEED_IDLE		2
#define MINIGUN_SPEED_SPINUP	3

// minigun_spin.sc iparam1
#define MINIGUN_SPIN_LOOP		0
#define MINIGUN_SPIN_START		1
#define MINIGUN_SPIN_STOP		2

#include "minigun.h"

// Don't allow default ammo to be set
#define SF_NODEFAULTAMMO			0x8000

#define SHOCKRIFLE_PRIMARY_AMMO		2	// charges per bolt
#define SHOCKRIFLE_WATER_BEAM_AMMO	5	// extra charges per beam frame in water
#define SHOCKRIFLE_WATER_DISCHARGE	20	// charges lost when firing under water
#define SHOCKRIFLE_DECAY_AMMO		30	// the battery drains below this charge
#define SHOCKRIFLE_BEAM_VOLUME		450

class CShockRifle : public CBasePlayerWeapon
{
public:
	void	Spawn( void );
	void	Precache( void );
	int		iItemSlot( void ) { return 4; }
	bool	GetItemInfo( ItemInfo *p );
	bool	AddToPlayer( CBasePlayer *pPlayer );
	const char	*GetPickupSound( void );
	CBasePlayerItem *DropItem( void );
	bool	IsUseable( void ) { return true; }
	int		ObjectCaps( void ) { return FCAP_IMPULSE_USE; }
	void	Materialize( void );

	void	ItemPostFrame( void );
	void	InactiveItemPostFrame( void );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	void	TertiaryAttack( void );
	bool	Deploy( void );
	void	Holster( int skiplocal = 0 );
	void	WeaponIdle( void );
	void	CustomDecrement( float flDecrement );

	void	EndBeamAttack( void );
	void	BeamFire( const Vector &vecSrc, const Vector &vecDir );
	void	AmmoDecay( void );
	void	UseAmmo( int iCount );
	void	DetonateInHand( void );

	void	GetWeaponData( weapon_data_s *data );
	void	SetWeaponData( weapon_data_s *data );

	bool	UseDecrement( void ) { return true; }

	float			m_flRechargeTime;
	float			m_flAmmoDecayTime;
	unsigned short	m_usShockRifle;
	unsigned short	m_usShockRifleBeam;
	int				m_iBeam;
	int				m_fInAttack;		// beam attack state
	float			m_flStartAttack;
};

// v_spore_launcher.mdl


// CSporeLauncher::m_ReloadState

#include "sporelauncher.h"

// v_displacer.mdl


// CDisplacer::m_iFireMode (CDisplacer::AttackMode_t)

// CDisplacer::m_TertiaryMode

// defaults for the mapper settable keyvalues


#include "displacer.h"

#define FLAMETHROWER_DAMAGE			10
#define FLAMETHROWER_AMMO_PER_SHOT	3
#define FLAMETHROWER_VOLUME			450

class CWeaponFlamethrower : public CBasePlayerWeapon
{
public:
	void	Spawn( void );
	void	Precache( void );
	bool	GetItemInfo( ItemInfo *p );
	bool	AddToPlayer( CBasePlayer *pPlayer );

	void	PrimaryAttack( void );
	bool	Deploy( void );
	void	WeaponIdle( void );

	void	GetWeaponData( weapon_data_s *data );
	void	SetWeaponData( weapon_data_s *data );

	bool	UseDecrement( void ) { return true; }
};

// v_9mmhandgun.mdl silencer handling
#define GLOCK_REMOVE_SILENCER	( GLOCK_ADD_SILENCER + 1 )

#define GLOCK_BODY_PLAIN		0
#define GLOCK_BODY_SILENCER		1

// CGlock::m_iSilencerState
#define GLOCK_SILENCER_NONE		0
#define GLOCK_SILENCER_ADD		1
#define GLOCK_SILENCER_REMOVE	2

class CGlock : public CBasePlayerWeapon
{
public:
	void	Spawn( void );
	void	Precache( void );
	int		iItemSlot( void ) { return 2; }
	bool	GetItemInfo( ItemInfo *p );
	bool	AddToPlayer( CBasePlayer *pPlayer );

	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	void	GlockFire( float flSpread, float flCycleTime, bool fUseAutoAim );
	bool	Deploy( void );
	void	Holster( int skiplocal = 0 );
	void	Reload( void );
	void	WeaponIdle( void );

	void	GetWeaponData( weapon_data_s *data );
	void	SetWeaponData( weapon_data_s *data );

	bool	UseDecrement( void ) { return true; }

	int				m_iSilencerState;	// silencer animation WeaponIdle still has to play
	int				m_iShell;
	unsigned short	m_usFireGlock1;
	unsigned short	m_usFireGlock2;
};

class CPython : public CBasePlayerWeapon
{
public:
	void	Spawn( void );
	void	Precache( void );
	int		iItemSlot( void ) { return 2; }
	bool	GetItemInfo( ItemInfo *p );
	bool	AddToPlayer( CBasePlayer *pPlayer );

	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	bool	Deploy( void );
	void	Holster( int skiplocal = 0 );
	void	Reload( void );
	void	WeaponIdle( void );

	void	GetWeaponData( weapon_data_s *data );
	void	SetWeaponData( weapon_data_s *data );

	bool	UseDecrement( void ) { return true; }

	unsigned short	m_usFirePython;
};

#define MP5_CLASSIC_MAX_CLIP	50
#define MP5_ZOOM_FOV			40

class CMP5 : public CBasePlayerWeapon
{
public:
	void	Spawn( void );
	void	Precache( void );
	int		iItemSlot( void ) { return 3; }
	bool	GetItemInfo( ItemInfo *p );
	bool	AddToPlayer( CBasePlayer *pPlayer );

	void	PrimaryAttack( void );
	void	ClassicPrimaryAttack( void );
	void	SecondaryAttack( void );
	void	ClassicSecondaryAttack( void );
	int		SecondaryAmmoIndex( void );
	bool	Deploy( void );
	void	Holster( int skiplocal = 0 );
	void	Reload( void );
	void	WeaponIdle( void );

	void	GetWeaponData( weapon_data_s *data );
	void	SetWeaponData( weapon_data_s *data );

	bool	UseDecrement( void ) { return true; }

	int				m_iShell;
	int				m_iSecondaryAmmoCarried;
	unsigned short	m_usMP5;
};

// v_crossbow.mdl
enum crossbow_e
{
	CROSSBOW_IDLE1 = 0,	// full
	CROSSBOW_IDLE2,		// empty
	CROSSBOW_FIDGET1,	// full
	CROSSBOW_FIDGET2,	// empty
	CROSSBOW_FIRE1,		// full
	CROSSBOW_FIRE2,		// reload
	CROSSBOW_FIRE3,		// empty
	CROSSBOW_RELOAD,	// from empty
	CROSSBOW_DRAW1,		// full
	CROSSBOW_DRAW2,		// empty
	CROSSBOW_HOLSTER1,	// full
	CROSSBOW_HOLSTER2,	// empty
};

#define CROSSBOW_ZOOM_FOV	20

class CCrossbow : public CBasePlayerWeapon
{
public:
	void	Spawn( void );
	void	Precache( void );
	int		iItemSlot( void ) { return 3; }
	bool	GetItemInfo( ItemInfo *p );
	bool	AddToPlayer( CBasePlayer *pPlayer );

	void	FireBolt( void );
	void	FireSniperBolt( void );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	bool	Deploy( void );
	void	Holster( int skiplocal = 0 );
	void	Reload( void );
	void	WeaponIdle( void );

	void	GetWeaponData( weapon_data_s *data );
	void	SetWeaponData( weapon_data_s *data );

	bool	UseDecrement( void ) { return true; }
};

#define SHOTGUN_SINGLE_PELLETS	8
#define SHOTGUN_DOUBLE_PELLETS	14

#define VECTOR_CONE_DM_SHOTGUN			Vector( 0.08716, 0.04362, 0.00 )// 10 degrees by 5 degrees
#define VECTOR_CONE_DM_DOUBLESHOTGUN	Vector( 0.17365, 0.04362, 0.00 )// 20 degrees by 5 degrees

#include "shotgun.h"

// v_rpg.mdl
enum rpg_e
{
	RPG_IDLE = 0,
	RPG_FIDGET,
	RPG_RELOAD,		// to reload
	RPG_FIRE2,		// to empty
	RPG_HOLSTER1,	// loaded
	RPG_DRAW1,		// loaded
	RPG_HOLSTER2,	// unloaded
	RPG_DRAW_UL,	// unloaded
	RPG_IDLE_UL,	// unloaded idle
	RPG_FIDGET_UL,	// unloaded fidget
};

class CRpg : public CBasePlayerWeapon
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	Reload( void );
	int		iItemSlot( void ) { return 4; }
	bool	GetItemInfo( ItemInfo *p );
	bool	AddToPlayer( CBasePlayer *pPlayer );

	bool	Deploy( void );
	bool	CanHolster( void );
	void	Holster( int skiplocal = 0 );

	void	PrimaryAttack( void );
	void	SecondaryAttack( void ) { }
	void	WeaponIdle( void );
	bool	ShouldWeaponIdle( void ) { return true; }

	void	UpdateSpot( void ) { }

	void	GetWeaponData( weapon_data_s *data );
	void	SetWeaponData( weapon_data_s *data );

	bool	UseDecrement( void ) { return true; }

	int		m_fSpotActive;
	int		m_cActiveRockets;// how many missiles in flight from this launcher right now?
};

#define GAUSS_PRIMARY_AMMO		1
#define GAUSS_DAMAGE			20		// event fparam1 and recoil of a shot
#define GAUSS_FULL_CHARGE_TIME	3
#define GAUSS_SPIN_START_PITCH	110
#define GAUSS_MAX_PITCH_RISE	150		// spin pitch gained over a full charge

#include "gauss.h"

// v_hgun.mdl
enum hgun_e
{
	HGUN_IDLE1 = 0,
	HGUN_FIDGETSWAY,
	HGUN_FIDGETSHAKE,
	HGUN_DOWN,
	HGUN_UP,
	HGUN_SHOOT
};

#include "hornetgun.h"

// v_satchel.mdl
enum satchel_e
{
	SATCHEL_IDLE1 = 0,
	SATCHEL_FIDGET1,
	SATCHEL_DRAW,
	SATCHEL_DROP
};

// v_satchel_radio.mdl
enum satchel_radio_e
{
	SATCHEL_RADIO_IDLE1 = 0,
	SATCHEL_RADIO_FIDGET1,
	SATCHEL_RADIO_DRAW,
	SATCHEL_RADIO_FIRE,
	SATCHEL_RADIO_HOLSTER
};

// CSatchel::m_chargeReady
enum satchel_state
{
	SATCHEL_IDLE = 0,
	SATCHEL_READY,
	SATCHEL_RELOAD
};

class CSatchel : public CBasePlayerWeapon
{
public:
	void	Spawn( void );
	void	Precache( void );
	int		iItemSlot( void ) { return 5; }
	bool	GetItemInfo( ItemInfo *p );
	bool	AddToPlayer( CBasePlayer *pPlayer );
	bool	AddDuplicate( CBasePlayerItem *pOriginal );
	const char	*GetBounceSound( void ) { return "weapons/g_bounce1.wav"; }
	bool	CanHaveDuplicates( void ) { return true; }
	bool	CanDeploy( void );
	bool	IsUseable( void );

	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	void	Throw( void );
	bool	Deploy( void );
	void	Holster( int skiplocal = 0 );
	void	WeaponIdle( void );

	void	GetWeaponData( weapon_data_s *data );
	void	SetWeaponData( weapon_data_s *data );

	bool	UseDecrement( void ) { return true; }

	int		m_chargeReady;
};

// v_grenade.mdl
enum handgrenade_e
{
	HANDGRENADE_IDLE = 0,
	HANDGRENADE_FIDGET,
	HANDGRENADE_PINPULL,
	HANDGRENADE_THROW1,	// toss
	HANDGRENADE_THROW2,	// medium
	HANDGRENADE_THROW3,	// hard
	HANDGRENADE_HOLSTER,
	HANDGRENADE_DRAW
};

#define HANDGRENADE_DAMAGE		90

class CHandGrenade : public CBasePlayerWeapon
{
public:
	void	Spawn( void );
	void	Precache( void );
	int		iItemSlot( void ) { return 5; }
	bool	GetItemInfo( ItemInfo *p );
	bool	CanHaveDuplicates( void ) { return true; }
	const char	*GetBounceSound( void ) { return "weapons/grenade_hit3.wav"; }

	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	void	TertiaryAttack( void );
	bool	Deploy( void );
	bool	CanHolster( void );
	void	Holster( int skiplocal = 0 );
	void	WeaponIdle( void );
	void	CustomDecrement( float flDecrement );

	void	GetWeaponData( weapon_data_s *data );
	void	SetWeaponData( weapon_data_s *data );

	bool	UseDecrement( void ) { return true; }

	bool	m_fStartThrowActive;
	float	m_flStartThrow;
	bool	m_fReleaseThrowActive;
	float	m_flReleaseThrow;
	bool	m_fThrowAll;			// throw all ten grenades at once
};


extern CBasePlayerWeapon	*g_pWpns[ MAX_WEAPONS ];
extern unsigned int		g_evWeaponCustom;
extern unsigned short		g_EvCheckClient;
extern bool					g_fEventRunned;
extern int					g_fAllowGilded;
extern bool					g_fCanHaveGildedUzis;

bool	CanHaveXtra( int iXtra, CBasePlayer *pPlayer );
void	SetGlobalFOV( int fov );

#endif // HL_WEAPONS_H
