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
#ifndef PLAYER_H
#define PLAYER_H


#include "pm_materials.h"
#include "TypedEntityHandle.h"

#ifndef CLIENT_DLL
#include <unordered_map>
#include <unordered_set>
#include "EntityHandle.h"
#include "VoteDefs.h"

enum DROP_TYPE : int;
class CObserver;
class CPortal;
class CEnvRenderIndividual;
class CTriggerCamera;
struct PlayerSlot;
#else
#include "VoteDefs.h"
#include "CToPlayerCollisions.h"
#endif

#define PLAYER_FATAL_FALL_SPEED		1024// approx 60 feet
#define PLAYER_MAX_SAFE_FALL_SPEED	580// approx 20 feet
#define DAMAGE_FOR_FALL_SPEED		(float) 100 / ( PLAYER_FATAL_FALL_SPEED - PLAYER_MAX_SAFE_FALL_SPEED )// damage per unit per second.
#define PLAYER_MIN_BOUNCE_SPEED		200
#define PLAYER_FALL_PUNCH_THRESHHOLD (float)350 // won't punch player's screen/make scrape noise unless player falling at least this fast.
#define PLAYER_BODYTARGET_NEAR_DIST	128 // closer shooters aim at a random height between eyes and center

//
// Player PHYSICS FLAGS bits
//
#define		PFLAG_ONLADDER		( 1<<0 )
#define		PFLAG_ONSWING		( 1<<0 )
#define		PFLAG_ONTRAIN		( 1<<1 )
#define		PFLAG_ONBARNACLE	( 1<<2 )
#define		PFLAG_DUCKING		( 1<<3 )		// In the process of ducking, but totally squatted yet
#define		PFLAG_USING			( 1<<4 )		// Using a continuous entity
#define		PFLAG_OBSERVER		( 1<<5 )		// player is locked in stationary cam mode. Spectators can move, observers can't.
#define		PFLAG_ONGRAPPLE	( 1 << 6 )

#define		PFLAG_CAMERA		( 1<<7 )

// CBasePlayer::SetCheat bits
#define		PLAYER_CHEAT_GODMODE	( 1<<0 )
#define		PLAYER_CHEAT_NOCLIP		( 1<<1 )
#define		PLAYER_CHEAT_NOTARGET	( 1<<2 )
#define		PLAYER_CHEAT_NONSOLID	( 1<<3 )
#define		PLAYER_CHEAT_INVISIBLE	( 1<<4 )

#define		WEAPON_NONE_SELECTED	-1	// CurWeapon id when no weapon is active

//
// generic player
//
//-----------------------------------------------------
//This is Half-Life player entity
//-----------------------------------------------------
#define CSUITPLAYLIST	4		// max of 4 suit sentences queued up at any time

#define SUIT_GROUP			TRUE
#define	SUIT_SENTENCE		FALSE

#define	SUIT_REPEAT_OK		0
#define SUIT_NEXT_IN_30SEC	30
#define SUIT_NEXT_IN_1MIN	60
#define SUIT_NEXT_IN_5MIN	300
#define SUIT_NEXT_IN_10MIN	600
#define SUIT_NEXT_IN_30MIN	1800
#define SUIT_NEXT_IN_1HOUR	3600

#define CSUITNOREPEAT		32

#define	SOUND_FLASHLIGHT_ON		"items/flashlight1.wav"
#define	SOUND_FLASHLIGHT_OFF	"items/flashlight1.wav"

#define TEAM_NAME_LENGTH	16

typedef enum
{
	PLAYER_IDLE,
	PLAYER_WALK,
	PLAYER_JUMP,
	PLAYER_SUPERJUMP,
	PLAYER_DIE,
	PLAYER_ATTACK1,
	PLAYER_RELOAD,
	PLAYER_DEPLOY,
#ifndef CLIENT_DLL
	PLAYER_BARNACLE_HIT,
	PLAYER_BARNACLE_PULL,
	PLAYER_BARNACLE_CHOMP,
	PLAYER_BARNACLE_CHEW,
#endif
} PLAYER_ANIM;

#define MAX_ID_RANGE 2048
#define SBAR_STRING_SIZE 128

enum sbar_data
{
	SBAR_ID_TARGETNAME = 1,
	SBAR_ID_TARGETHEALTH,
	SBAR_ID_TARGETARMOR,
	SBAR_END,
};

#define CHAT_INTERVAL 1.0f

class CBasePlayer : public CBaseMonster
{
#ifndef CLIENT_DLL
private:
	struct HideHUDHandleHash
	{
		size_t operator()( const EHANDLE &handle ) const;
	};

	struct HideHUDHandleEqual
	{
		bool operator()( const EHANDLE &left, const EHANDLE &right ) const;
	};

	typedef std::unordered_map<EHANDLE, int, HideHUDHandleHash, HideHUDHandleEqual> HideHUDMap;
	typedef std::unordered_map<EntityHandle<CBaseEntity>, EntityHandle<CEnvRenderIndividual>,
	                           EntityHandle<CBaseEntity>::Hash, EntityHandle<CBaseEntity>::Equal> EntityRenderOverrideMap;
	HideHUDMap m_ActorHideHUD;

	typedef std::unordered_set<EntityHandle<CPortal>, EntityHandle<CPortal>::Hash, EntityHandle<CPortal>::Equal> PortalSourceSet;
	typedef std::unordered_set<EntityHandle<CBaseEntity>, EntityHandle<CBaseEntity>::Hash, EntityHandle<CBaseEntity>::Equal> PortalEntitySet;

	// Portal visibility caches
	PortalSourceSet	m_VisiblePortalSources;
	PortalEntitySet	m_VisiblePortalEntities;
	unsigned char	*m_pPortalVisibilitySet;
	bool			m_bPortalVisibilityCached;
	unsigned int	m_uiPortalVisibilityGeneration;

	int GetHideHUD( void );
#endif

public:
	void SetOverriddenPlayerModel( const char *model );
	void ResetOverriddenPlayerModel( bool restorePreviousModel, bool forceRestore );

#ifndef CLIENT_DLL
	CBasePlayer();
	void OnCreate( void );
	void OnDestroy( void );
	void OnDisconnect( void );
	void EXPORT ObserverThink( void );
	virtual void EnteredObserver( void );
	virtual void LeftObserver( void );
	bool DropPlayerWeapon( void );
	bool CanSpawn( void );
	void EXPORT ForcedRespawnThink( void );
	void SpawnDead( CBaseEntity *point );
	virtual bool IsRevivable( void );
	virtual void BeginRevive( float flTimeUntilRevive );
	virtual void Revive( void );
	static EntityHandle<CBaseEntity> SelectSpawnPoint( CBaseEntity *player );
	void StartRepelDown( float speed );
	void EndRepelDown( void );
	void RepelDownThink( void );

	float                     m_flRespawnDelayTime;
	float                     m_flTimeOfLastDeath;
	float                     m_flLastMove;
	EHANDLE                   m_hSpawnPoint;
	bool                      m_bDeadItemsPacked;
	bool                      m_bRespawnMessageShown;

	bool	m_bBarnacleMouth;
	EHANDLE	m_hBarnacle;

	float                     m_flLastClientPutInServerTime;
	Vector                    m_vecRepelStart;
	float                     m_flRepelSpeed;
	CEntityHandle<CBaseEntity> m_hRepelBeam;
	bool                      m_fRepelling;

	void Teleport( Vector vecOrigin, Vector vecAngles );
	void ClearVisiblePortalSources( void );
	const PortalSourceSet &GetVisiblePortals( void ) const;
	const PortalEntitySet &GetVisiblePortalSources( void ) const;
	bool HasVisiblePortals( bool sources ) const;
	void AddVisiblePortalSource( EntityHandle<CPortal> portal, EntityHandle<CBaseEntity> source, bool visibilityOnly );
	void SetPortalVisibilitySet( unsigned char *set );
	bool MatchesPortalVisibilitySet( const unsigned char *set ) const;
	unsigned int NextPortalVisibilityGeneration( void );
	unsigned int GetPortalVisibilityGeneration( void ) const;
	CObserver *GetObserver( void ) { return m_pObserver; }

	bool ShouldForcePlayerModel( void );

	void AddEntityRenderOverride( EntityHandle<CBaseEntity> entity, CEnvRenderIndividual *renderer );
	void RemoveEntityRenderOverride( EntityHandle<CBaseEntity> entity );
	bool HasEntityRenderOverride( EntityHandle<CBaseEntity> entity );
	bool HasEntityRenderOverride( edict_t *entity );
	CEnvRenderIndividual *GetEntityRenderOverride( EntityHandle<CBaseEntity> entity );
	CEnvRenderIndividual *GetEntityRenderOverride( edict_t *entity );

	CObserver *m_pObserver;

	int GetCheats( void );
	void KeyValue( KeyValueData *data );
	void OnKeyValueUpdate( const char *key );
	void BlockWeapons( CBaseEntity *actor );
	void UnblockWeapons( CBaseEntity *actor );
	void UpdatePlayerSpeed( void );
	void SetHideHUD( EHANDLE actor, int flags );
	void ClearHideHUD( EHANDLE actor );

	int GetMaxSpeed( void );
	void SetMaxSpeed( int iSpeed );
	int GetMaxSpeedOverride( void );
	void SetMaxSpeedOverride( int iSpeed );
	bool GetCheat( int iCheat );
	bool SetCheat( int iCheat, bool fEnable, bool fNotify );
	bool GetWeaponsBlocked( void );
	bool HasSuit( void );
	void SetHasSuit( bool fHasSuit );
	void ClearTrain( void );
	void ClearGeiger( void );
	bool HasAmmo( void );
	static const char *GetAmmoName( int iAmmoIndex );
	bool CanDropPlayerItem( const char *pszItemName );
	CBaseEntity *CreatePlayerItem( const char *pszItemName );
	void SendCurrentWeapon( int iState, int iId, int iClip, int iClip2 );
	void SendNoCurrentWeapon( void );
	void OpenVGUIMenu( int iMenu );
	void SetViewMode( int iMode );
	void ShowForcedRespawnMessage( int iSeconds );
	virtual void VoteStarted( VOTETYPE type );
	virtual bool CanStartNextVote( VOTETYPE type );
	virtual void Vote( VOTEINPUT input, const char *text );
	virtual bool HasVoted();
	virtual void ResetVote();
	virtual VOTEINPUT LastVoteInput();
	virtual void InitVote();
	virtual float TimeToStartNextVote();

	void CachePVS( unsigned char *pvs ) { m_pCachedPVS = pvs; }
	bool IsPVS( unsigned char *pvs ) const { return pvs && m_pCachedPVS == pvs; }
	void BeginFrame( void ) { ++m_iFrame; }
	int GetFrame( void ) const { return m_iFrame; }

	unsigned char *m_pCachedPVS;
	int       m_iFrame;

	float     m_flNextMiscCommandTime;
	float     m_flNextNpcCommandTime;
	float     m_flNextStuckKillTime;
	bool      m_fDonorExtras;
	float     m_flNextVoteTime;
	VOTEINPUT m_iLastVoteInput;
	bool      m_bHasVoted;

	// Cheat bits maintained by key-value updates
	int		m_iKeyValueCheats;
	int		m_iCheats;
	float	m_flMaxSpeed;
	float	m_flMaxSpeedOverride;
	float	m_flNextClientCommandTime;
	bool	m_fWeaponsBlocked;

#else
	virtual float GetLogFrequency( void );
	virtual bool CanStartNextVote( VOTETYPE type );
	virtual void Vote( VOTEINPUT input, const char *text );
	virtual VOTEINPUT LastVoteInput();
	virtual float TimeToStartNextVote();

	CToPlayerCollisions	m_PlayerCollisions;
#endif

	virtual float GetPointsForDamage( float flDamage );
	virtual bool IsMoving( void );

	// Grapple firing, rappel preference, and release state
	bool	m_fGrappleFired;

#ifndef CLIENT_DLL
	bool	m_fGrappleRappelMode;
	bool	m_fGrappleActive;

	static float PlCountBalanceCalc( void );
	void LogPlayerStats( void );
	void InitScoreInfo( void );
	void SendScoreInfo( edict_t *recipient );
	void SendScoreInfoToOtherPlayer( edict_t *recipient, int playerIndex, const char *classificationTag );

	int	m_iHideXtra;
	int	m_iAdminLevel;
	int	m_iHideAdmin;
#endif

	
	// Spectator camera
	void	Observer_FindNextPlayer( bool bReverse );
	void	Observer_HandleButtons();
	void	Observer_SetMode( int iMode );
	void	Observer_CheckTarget();
	void	Observer_CheckProperties();
	EHANDLE	m_hObserverTarget;
	float	m_flNextObserverInput;
	int		m_iObserverWeapon;	// weapon of current tracked target
	int		m_iObserverLastMode;// last used observer mode
	virtual int IsObserver( void ) { return pev->iuser1 != 0; }

	int					random_seed;    // See that is shared between client & server for shared weapons code

	int					m_iPlayerSound;// the index of the sound list slot reserved for this player
	int					m_iTargetVolume;// ideal sound volume. 
	int					m_iWeaponVolume;// how loud the player's weapon is right now.
	int					m_iExtraSoundTypes;// additional classification for this weapon's sound
	int					m_iWeaponFlash;// brightness of the weapon flash
	float				m_flStopExtraSoundTime;
	
	float				m_flFlashLightTime;	// Time until next battery draw/Recharge
	int					m_iFlashBattery;		// Flashlight Battery Draw

	int					m_afButtonLast;
	int					m_afButtonPressed;
	int					m_afButtonReleased;
	
	edict_t			   *m_pentSndLast;			// last sound entity to modify player room type
	float				m_flSndRoomtype;		// last roomtype set by sound entity
	float				m_flSndRange;			// dist from player to sound entity

	float				m_flFallVelocity;
	
	int					m_rgItems[MAX_ITEMS];
	int					m_fKnownItem;		// True when a new item needs to be added
	int					m_fNewAmmo;			// True when a new item has been added

	unsigned int		m_afPhysicsFlags;	// physics flags - set when 'normal' physics should be revisited or overriden
	float				m_fNextSuicideTime; // the time after which the player can next use the suicide command


// these are time-sensitive things that we keep track of
	float				m_flTimeStepSound;	// when the last stepping sound was made
	float				m_flTimeWeaponIdle; // when to play another weapon idle animation.
	float				m_flSwimTime;		// how long player has been underwater
	float				m_flDuckTime;		// how long we've been ducking
	float				m_flWallJumpTime;	// how long until next walljump

	float				m_flSuitUpdate;					// when to play next suit update
	int					m_rgSuitPlayList[CSUITPLAYLIST];// next sentencenum to play for suit update
	int					m_iSuitPlayNext;				// next sentence slot for queue storage;
	int					m_rgiSuitNoRepeat[CSUITNOREPEAT];		// suit sentence no repeat list
	float				m_rgflSuitNoRepeatTime[CSUITNOREPEAT];	// how long to wait before allowing repeat
	int					m_lastDamageAmount;		// Last damage taken
	float				m_tbdPrev;				// Time-based damage timer

	float				m_flgeigerRange;		// range to nearest radiation source
	float				m_flgeigerDelay;		// delay per update of range msg to client
	int					m_igeigerRangePrev;

#ifdef CLIENT_DLL
	int					m_iStepLeft;			// alternate left/right foot stepping sound
	char				m_szTextureName[CBTEXTURENAMEMAX];	// current texture name we're standing on
	char				m_chTextureType;		// current texture type
#endif

	int					m_idrowndmg;			// track drowning damage taken
	int					m_idrownrestored;		// track drowning damage restored

	int					m_bitsHUDDamage;		// Damage bits for the current fame. These get sent to 
												// the hude via the DAMAGE message
	bool				m_fInitHUD;				// True when deferred HUD restart msg needs to be sent
	BOOL				m_fGameHUDInitialized;
	float				m_flNextTimeEndUpdate;
	int					m_iTrain;				// Train control position
	BOOL				m_fWeapon;				// Set this to FALSE to force a reset of the current weapon HUD info

	CEntityHandle<CBaseEntity> m_hTank; // the tank which the player is currently controlling,  NULL if no tank
#ifndef CLIENT_DLL
	CEntityHandle<CBaseEntity> m_hVehicle;
	EHANDLE                    m_hChumtoad;
	PlayerSlot                *m_pVehicleSlot;
#endif

	float				m_fDeadTime;			// the time at which the player died  (used in PlayerDeathThink())

	BOOL			m_fNoPlayerSound;	// a debugging feature. Player makes no sound if this is true. 
#ifdef CLIENT_DLL
	BOOL			m_fLongJump; // does this player have the longjump module?
#else
	bool			m_fLongJump;
#endif

	float       m_tSneaking;
	int			m_iUpdateTime;		// stores the number of frame ticks before sending HUD update messages
	int			m_iClientHealth;	// the health currently known by the client.  If this changes, send a new
	int			m_iClientBattery;	// the Battery currently known by the client.  If this changes, send a new
	int			m_iClientFrags;		// frags and armor last put on the scoreboard
	int			m_iClientArmor;
	int			m_iHideHUD;		// the players hud weapon info is to be hidden
	int			m_iClientHideHUD;
	int			m_iFOV;			// field of view
	int			m_iClientFOV;	// client's known FOV
	// usable player items 
#ifdef CLIENT_DLL
	CBasePlayerItem	*m_rgpPlayerItems[MAX_ITEM_TYPES];
	CBasePlayerItem *m_pActiveItem;
	CBasePlayerItem *m_pClientActiveItem;  // client version of the active item
	CBasePlayerItem *m_pLastItem;
#else
	CEntityHandle<CBasePlayerItem> m_rgpPlayerItems[MAX_ITEM_TYPES];
	CEntityHandle<CBasePlayerItem> m_pActiveItem;
	CEntityHandle<CBasePlayerItem> m_pClientActiveItem;
	CEntityHandle<CBasePlayerItem> m_pLastItem;

	float m_flNextWeaponPickup[MAX_WEAPONS];	// Per-weapon pickup deadline
#endif

	bool			m_fConnected;		// set when the client is put in the server
	// shared ammo slots
	int	m_rgAmmo[MAX_AMMO_SLOTS];
	int	m_rgAmmoLast[MAX_AMMO_SLOTS];
	int	m_rgMaxAmmo[MAX_AMMO_SLOTS];

	Vector				m_vecAutoAim;
	BOOL				m_fOnTarget;
	int					m_iDeaths;
	float				m_iRespawnFrames;	// used in PlayerDeathThink() to make sure players can always respawn

	int m_lastx, m_lasty;  // These are the previous update's crosshair angles, DON"T SAVE/RESTORE

	int m_nCustomSprayFrames;// Custom clan logo frames for this player
	float	m_flNextDecalTime;// next time this player can spray a decal

	char m_szTeamName[TEAM_NAME_LENGTH];

	virtual void Spawn( void );
#ifndef CLIENT_DLL
	virtual void SpecialSpawn( void );
#endif
	void Pain( void );

//	virtual void Think( void );
	virtual void Jump( void );
	virtual void Duck( void );
	virtual void PreThink( void );
	virtual void PostThink( void );
	virtual Vector GetGunPosition( void );
	virtual bool TakeHealth( float flHealth, int bitsDamageType, int health_cap = 0 );
#ifndef CLIENT_DLL
	virtual bool TakeArmor( float flArmor, int bitsDamageType, int armor_cap = 0 );
#endif
	virtual void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	virtual int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	virtual void	Killed( entvars_t *pevAttacker, int iGib );
	virtual Vector BodyTarget( const Vector &posSrc )		// position to shoot at
	{
		if ( ( Center() - posSrc ).Length2D() <= PLAYER_BODYTARGET_NEAR_DIST )
			return Center() + pev->view_ofs * RANDOM_FLOAT( 0.5, 1.1 );

		return Center() * 0.75 + EyePosition() * 0.25;
	}
	virtual void StartSneaking( void ) { m_tSneaking = gpGlobals->time - 1; }
	virtual void StopSneaking( void ) { m_tSneaking = gpGlobals->time + 30; }
	virtual BOOL IsSneaking( void ) { return m_tSneaking <= gpGlobals->time; }
	virtual BOOL IsAlive( void ) { return (pev->deadflag == DEAD_NO) && pev->health > 0 && IsConnected(); }
	virtual bool IsConnected( void ) { return m_fConnected; }

	void DeployWeapon( void );
	void HolsterWeapon( void );
	virtual BOOL ShouldFadeOnDeath( void ) { return FALSE; }
	virtual	BOOL IsPlayer( void ) { return TRUE; }			// Spectators should return FALSE for this, they aren't "players" as far as game logic is concerned

	virtual BOOL IsNetClient( void ) { return TRUE; }		// Bots should return FALSE for this, they can't receive NET messages
															// Spectators should return TRUE for this
	virtual const char *TeamID( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	void RenewItems(void);
	void PackDeadPlayerItems( void );
#ifdef CLIENT_DLL
	void RemoveAllItems( bool removeSuit );
#else
	void RemoveAllItems( bool removeSuit = false, bool removeLongJump = false );
#endif
	BOOL SwitchWeapon( CBasePlayerItem *pWeapon );

	// JOHN:  sends custom messages if player HUD data has changed  (eg health, ammo)
	virtual void UpdateClientData( void );
	
	static	TYPEDESCRIPTION m_playerSaveData[];

	// Player is moved across the transition by other means
	virtual int		ObjectCaps( void ) { return CBaseMonster :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void	Precache( void );
	BOOL			IsOnLadder( void );
	BOOL			FlashlightIsOn( void );
	void			FlashlightTurnOn( void );
	void			FlashlightTurnOff( void );
	
	void UpdatePlayerSound ( void );
	void DeathSound ( void );

	int Classify ( void );
	void SetAnimation( PLAYER_ANIM playerAnim, int iParam = 0 );
	void SetWeaponAnimType( const char *szExtention );
	char m_szAnimExtention[32];

	// custom player functions
	virtual void ImpulseCommands( void );
	void CheatImpulseCommands( int iImpulse );

	void StartDeathCam( void );
#ifndef CLIENT_DLL
	void StartPlayerDeathView( const Vector &position, const Vector &viewAngle );
#endif
	void StartObserver( Vector vecPosition, Vector vecViewAngle );

	void AddPoints( int score, bool bAllowNegativeScore );
	void AddPointsToTeam( int score, bool bAllowNegativeScore );
	BOOL AddPlayerItem( CBasePlayerItem *pItem );
	bool RemovePlayerItem( CBasePlayerItem *pItem );
#ifdef CLIENT_DLL
	void DropPlayerItem( char *pszItemName );
#else
	CBaseEntity *DropPlayerItem( char *pszItemName );
	CBasePlayerItem *DropPlayerAmmo( const char *itemName, bool giveToPlayer, bool secondary );
	CBasePlayerItem *FindItem( const char *pszItemName );
	CBasePlayerItem *FindPlayerItem( char *pszItemName );
	CBaseEntity *DropItem( CBaseEntity *item, int primaryAmmo, int secondaryAmmo,
		bool throwForward, DROP_TYPE dropType );
	CBasePlayer *MyPlayerPointer( void ) { return this; }
#endif
	BOOL HasPlayerItem( CBasePlayerItem *pCheckItem );
	BOOL HasNamedPlayerItem( const char *pszItemName );
	BOOL HasWeapons( void );// do I have ANY weapons?
	void SelectPrevItem( int iItem );
	void SelectNextItem( int iItem );
	void SelectLastItem(void);
	void SelectItem(const char *pstr);
	void ItemPreFrame( void );
	void ItemPostFrame( void );
	void GiveNamedItem( const char *szName, int iFlags = 0, int iAmmoCount = 0 );
	void GiveItem( CBaseEntity *item );
	bool IsExclusiveHoldItemActive( void );
	void EnableControl(BOOL fControl);

	int  GiveAmmo( int iAmount, const char *szName, int iMax, bool fFromWeapon = true, int *piAmountGiven = NULL );
	void SendAmmoUpdate(void);

	void WaterMove( void );
	void EXPORT PlayerDeathThink( void );
	void PlayerUse( void );

	void CheckSuitUpdate();
	void SetSuitUpdate(const char *name, int fgroup, int iNoRepeat);
	void UpdateGeigerCounter( void );
	void CheckTimeBasedDamage( void );

	bool FBecomeProne ( CBaseEntity *pOther );
	void BarnacleVictimBitten( entvars_t *pevBarnacle, float flDamage );
	void BarnacleVictimReleased ( void );
	static int GetAmmoIndex(const char *psz);
	int AmmoInventory( int iAmmoIndex );
	int GetMaxAmmo( unsigned int index ) const;
	int GetMaxAmmo( const char *name ) const;
	void SetMaxAmmo( unsigned int index, int maxAmmo );
	void SetMaxAmmo( const char *name, int maxAmmo );
	void ResetMaxAmmoToDefault( unsigned int index );
	void ResetMaxAmmoToDefault( const char *name );
	void ResetAllMaxAmmoToDefault( void );
	void RemoveExcessAmmo( unsigned int index );
	void RemoveExcessAmmo( const char *name );
	void RemoveAllExcessAmmo( void );
	int Illumination( void );

	void ResetAutoaim( void );
	Vector GetAutoaimVector( float flDelta  );
	Vector AutoaimDeflection( Vector &vecSrc, float flDist, float flDelta  );

	void ForceClientDllUpdate( void );  // Forces all client .dll specific data to be resent to client.

#ifndef CLIENT_DLL
	int GetPlayerExtras( bool force = false );
	bool HasExtra( int type );
#endif

	void DeathMessage( entvars_t *pevKiller );

	void SetCustomDecalFrames( int nFrames );
	int GetCustomDecalFrames( void );

	void TabulateAmmo( void );

	float m_flStartCharge;
	float m_flAmmoStartCharge;
	float m_flPlayAftershock;
	float m_flNextAmmoBurn;// while charging, when to absorb another unit of player's ammo?
	
	//Player ID
	int   m_izSBarState[ SBAR_END ];
	float m_flNextSBarUpdateTime;
	float m_flStatusBarDisappearDelay;
	char  m_SbarString0[ SBAR_STRING_SIZE ];
	char  m_SbarString1[ SBAR_STRING_SIZE ];
	
	float m_flNextChatTime;

#ifndef CLIENT_DLL
	bool	m_fOverridePlayerModel;
	bool	m_fOverridePlayerModelAcknowledged;
	char	m_szOverriddenPlayerModel[64];

private:
	EntityRenderOverrideMap m_EntityRenderOverrides;

public:
	virtual bool IsValidInfoEntity( CBaseEntity *entity );
	void UpdateMonsterInfo( void );
	bool IsImpostor( const char *name );

	bool m_fIsImpostor;

	CEntityHandle<CTriggerCamera> m_hCamera;
	bool                         m_bCameraMousePanelOpen;

	void SetVModelPos( const Vector &position );
	void ResetVModelPos( void );
	void SendVModelPos( int mode, const Vector &position );
	bool ShowOverheadSprite( const char *spriteName, float zOffset, float lifeTime );
	bool IntervalHUDMessage( int destination, const char *message );
	void ShowMenu( int validSlots, int displayTime, bool needMore, char *text );
	void ShowMenu( int validSlots, int displayTime, char *text, int menuId, unsigned int page );

	int m_iPlayerClass;

	Vector m_vecViewModelPos;
	Vector m_vecClientViewModelPos;
	bool   m_bViewModelPos;
	bool   m_bClientViewModelPos;

	float m_flLastHUDMessageTime;

	bool         m_bTextMenuInputReceived;
	int          m_iTextMenuId;
	unsigned int m_uiTextMenuPage;
	float        m_flTextMenuExpireTime;
	int          m_iTextMenuValidSlots;

	void SetItemPickupTimes( float time );

	int m_iWeaponAutoSwitch;
#endif
};

#define AUTOAIM_2DEGREES  0.0348994967025
#define AUTOAIM_5DEGREES  0.08715574274766
#define AUTOAIM_8DEGREES  0.1391731009601
#define AUTOAIM_10DEGREES 0.1736481776669


extern int	gmsgHudText;
extern BOOL gInitHUD;

#endif // PLAYER_H
