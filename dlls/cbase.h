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

Class Hierachy

CBaseEntity
	CBaseDelay
		CBaseToggle
			CBaseItem
			CBaseMonster
				CBaseCycler
				CBasePlayer
				CBaseGroup
*/

#define		MAX_PATH_SIZE	10 // max number of nodes available for a path.

// These are caps bits to indicate what an object's capabilities (currently used for save/restore and level transitions)
#define		FCAP_CUSTOMSAVE				0x00000001
#define		FCAP_ACROSS_TRANSITION		0x00000002		// should transfer between transitions
#define		FCAP_MUST_SPAWN				0x00000004		// Spawn after restore
#define		FCAP_DONT_SAVE				0x80000000		// Don't save this
#define		FCAP_IMPULSE_USE			0x00000008		// can be used by the player
#define		FCAP_CONTINUOUS_USE			0x00000010		// can be used by the player
#define		FCAP_ONOFF_USE				0x00000020		// can be used by the player
#define		FCAP_DIRECTIONAL_USE		0x00000040		// Player sends +/- 1 when using (currently only tracktrains)
#define		FCAP_MASTER					0x00000080		// Can be used to "master" other entities (like multisource)

// UNDONE: This will ignore transition volumes (trigger_transition), but not the PVS!!!
#define		FCAP_FORCE_TRANSITION		0x00000080		// ALWAYS goes across transitions

#include "archtypes.h"     // DAL
#include "saverestore.h"
#include "schedule.h"

#ifndef MONSTEREVENT_H
#include "monsterevent.h"
#endif

#include "Platform.h"

// C functions for external declarations that call the appropriate C++ methods

#define EXPORT DLLEXPORT

extern "C" DLLEXPORT int GetEntityAPI( DLL_FUNCTIONS *pFunctionTable, int interfaceVersion );
extern "C" DLLEXPORT int GetEntityAPI2( DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion );
extern "C" DLLEXPORT int GetNewDLLFunctions( NEW_DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion );

extern int DispatchSpawn( edict_t *pent );
extern void DispatchKeyValue( edict_t *pentKeyvalue, KeyValueData *pkvd );
extern void DispatchTouch( edict_t *pentTouched, edict_t *pentOther );
extern void DispatchUse( edict_t *pentUsed, edict_t *pentOther );
extern void DispatchThink( edict_t *pent );
extern void DispatchBlocked( edict_t *pentBlocked, edict_t *pentOther );
extern void DispatchSave( edict_t *pent, SAVERESTOREDATA *pSaveData );
extern int  DispatchRestore( edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity );
extern void	DispatchObjectCollsionBox( edict_t *pent );
extern void DispatchOnFreeEntPrivateData( edict_t *pent );
extern void SaveWriteFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount );
extern void SaveReadFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount );
extern void SaveGlobalState( SAVERESTOREDATA *pSaveData );
extern void RestoreGlobalState( SAVERESTOREDATA *pSaveData );
extern void ResetGlobalState( void );

typedef enum
{
	USE_OFF = 0,
	USE_ON = 1,
	USE_SET = 2,
	USE_TOGGLE = 3,
	USE_KILL = 4,
} USE_TYPE;

extern void FireTargets( const char *targetName, CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

typedef void (CBaseEntity::*BASEPTR)(void);
typedef void (CBaseEntity::*ENTITYFUNCPTR)(CBaseEntity *pOther );
typedef void (CBaseEntity::*USEPTR)( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

// For CLASSIFY
#include "EntityClassification.h"
#include "CCustomKeyvalues.h"
#ifndef CLIENT_DLL
#include "CStringMap.h"
#endif

int AllocStringLocal( const char *text );

#define MAX_COLORMAP_COLOR		255
#define COLORMAP_BOTTOM_SHIFT	8

class CBaseEntity;
class CBaseMonster;
class CScriptDictValue;
class CBasePlayer;
class CBasePlayerItem;
class CSquadMonster;
class CAttackVectors;

class ICustomEntity;
class CScriptDictionary;


#define SF_NORESPAWN		( 1 << 10 )
#define SF_DELAYREMOVE		( 1 << 11 )	// a dropped weapon is removed after about a minute
#define SF_CREATEDWEAPON	( 1 << 12 )	// made for a player rather than placed in the map
#define SF_DODAMAGE			( 1 << 13 )	// a thrown weapon hurts what it hits
#define SF_GIVENITEM		( 1 << 14 )
#define SF_NODEFAULTAMMO	( 1 << 15 )

//
// EHANDLE. Safe way to point to CBaseEntities who may die between frames
//
class EHANDLE
{
	friend class CBasePlayer;

private:
	edict_t *m_pent;
	int		m_serialnumber;
public:
	edict_t *Get( void );
	edict_t *Set( edict_t *pent );

	operator CBaseEntity *();

	CBaseEntity * operator = (CBaseEntity *pEntity);
	CBaseEntity * operator ->();
};

#include "TypedEntityHandle.h"

// FireBullets draw mode options
typedef enum
{
	FBDM_DONTDRAW = 0,	// Don't draw impact decal
	FBDM_DRAW = 2		// Draw impact decal
} FireBulletsDrawMode;

//
// Base Entity.  All entity types derive from this
//
class CBaseEntity 
{
public:
	// Constructor.  Set engine to use C/C++ callback functions
	// pointers to engine data
	entvars_t *pev;		// Don't need to save/restore this pointer, the engine resets it

	// path corners
	CEntityHandle<CBaseEntity> m_hGoalEnt; // path corner we are heading towards

#ifndef CLIENT_DLL
	EHANDLE				m_pLink;// used for temporary link-list operations.
#else
	CBaseEntity			*m_pLink;// used for temporary link-list operations.
#endif

	CCustomKeyvalues	*m_pCustomKeyvalues;

#ifndef CLIENT_DLL
	float				m_flCreationTime;
	int					m_iExtraSpawnFlags;
	string_t			m_iszOnDestroy;
	CScriptDictionary	*m_pUserData;
	CStringMap			*m_pSoundReplacementMap;
	bool				m_fCustomModel;

	float				m_flMaximumFadeWait;
	float				m_flMaximumFadeWaitB;
	bool				m_fCanFadeStart;

	unsigned int		m_uiEntityFlags;
	Vector				m_vecLastOrigin;
	unsigned int		m_uiPortalVisibilityGeneration[32 + 1];	// one per client, indexed by entity index

	EHANDLE				m_pSightLink;	// Look's list of everything seen, corpses included
	EHANDLE				m_hHealer;		// medic reserving this entity for healing or revival
#endif

	bool				m_fOverrideClass;
	int					m_iClassSelection;
	bool				m_fPlayerAlly;

	string_t			m_iszSoundList;

	int					targetnameOutFilterType;
	int					classnameOutFilterType;
	int					targetnameInFilterType;
	int					classnameInFilterType;
	string_t			targetnameOutFilter;
	string_t			classnameOutFilter;
	string_t			targetnameInFilter;
	string_t			classnameInFilter;

	int					m_iOriginalRenderMode;
	int					m_iOriginalRenderFX;
	float				m_flOriginalRenderAmount;
	Vector				m_vecOriginalRenderColor;

	int					m_iTopColor;
	int					m_iBottomColor;

	CBaseEntity();

	// initialization functions
	virtual void PreSpawn( void );
	virtual void	Spawn( void ) { return; }
	virtual void	PostSpawn( void ) { }
	virtual void	Precache( void );
	bool SetupModel( void );
#ifndef CLIENT_DLL
	const char *SOUNDREPLACEMENT_Find( const char *sample );
	void AddEntityFlag( int mask );
	void RemoveEntityFlag( int mask );
	bool HasEntityFlag( int mask ) const;
	void ClearEntityFlags( void );
	CScriptDictionary *GetUserData( void );
	CScriptDictValue &GetUserData( const CString &key );
	void ClearUserData( void );
	void ClearUserData( bool checkContext );
	void SetOnDestroyFunction( const char *name );
	int FormatASFunction( char *buffer, unsigned int size, const char *returnType,
	                      const char *name, const char *parameters, bool warn ) const;
#endif
	virtual void	KeyValue( KeyValueData* pkvd);
	virtual void	*GetKeyValue( const char *szKeyName, entvartype_s &type );
	virtual void	OnKeyValueUpdate( const char *szKeyName );

	entvartype_s GetEntvar( const char *szKeyName, void **value, int &index );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	virtual int		ObjectCaps( void ) { return FCAP_ACROSS_TRANSITION; }
	virtual void	Activate( void ) {}

	// Setup the object->object collision box (pev->mins / pev->maxs is the object->world collision box)
	virtual void	SetObjectCollisionBox( void );

	virtual int		GetClassification( int cl );
	virtual const char *GetClassificationTag( void );
	virtual const char *GetClassificationName( void );
	virtual void	SetClassification( int cl ) { SetClassificationSpecial( cl, false ); }
	virtual void	SetClassificationSpecial( int cl, bool fKeepOverride );
	void			SetClassificationFromEntity( CBaseEntity *pEntity );
	void			ClearClassification( void );
	virtual int		IRelationship( CBaseEntity *other, bool ignoreNoTarget = false );

// Classify - returns the type of group (i.e, "houndeye", or "human military" so that monsters with different classnames
// still realize that they are teammates. (overridden for monsters that form groups)
	virtual int Classify ( void ) { return GetClassification( CLASS_NONE ); };
	int IRelationshipByClass( int classification );
	virtual void DeathNotice ( entvars_t *pevChild ) {}// monster maker children use this to tell the monster maker that they have died.


	static	TYPEDESCRIPTION m_SaveData[];

	virtual void	TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	virtual int		TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	virtual bool	TakeHealth( float flHealth, int bitsDamageType, int health_cap = 0 );
	virtual bool	TakeArmor( float flArmor, int bitsDamageType, int armor_cap = 0 );
	virtual void	Killed( entvars_t *pevAttacker, int iGib );
	virtual int		BloodColor( void ) { return DONT_BLEED; }
	virtual void	TraceBleed( float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	virtual BOOL    IsTriggered( CBaseEntity *pActivator ) {return TRUE;}
	virtual CBaseMonster *MyMonsterPointer( void ) { return NULL;}
	virtual CSquadMonster *MySquadMonsterPointer( void ) { return NULL;}
	virtual CBasePlayer *MyPlayerPointer( void ) { return NULL; }
	virtual ICustomEntity *MyCustomPointer( void ) { return NULL; }
	virtual CBasePlayerItem *MyItemPointer( void ) { return NULL; }
	virtual	int		GetToggleState( void ) { return TS_AT_TOP; }
	virtual void	AddPoints( int score, bool bAllowNegativeScore ) {}
	virtual void	AddPointsToTeam( int score, bool bAllowNegativeScore ) {}
	virtual int		AddPlayerItem( CBasePlayerItem *pItem ) { return 0; }
	virtual bool	RemovePlayerItem( CBasePlayerItem *pItem ) { return false; }
	virtual int 	GiveAmmo( int iAmount, const char *szName, int iMax, bool fFromWeapon = true, int *piAmountGiven = NULL ) { return -1; };
	virtual float	GetDelay( void ) { return 0; }
	virtual int		IsMoving( void ) const { return pev->velocity != g_vecZero; }
	virtual void	OverrideReset( void ) {}
	virtual int		DamageDecal( int bitsDamageType );
	// This is ONLY used by the node graph to test movement through a door
	virtual void	SetToggleState( int state ) {}
	virtual void    StartSneaking( void ) {}
	virtual void    StopSneaking( void ) {}
	virtual BOOL	OnControls( entvars_t *pev ) { return FALSE; }
	virtual BOOL    IsSneaking( void ) { return FALSE; }
	virtual BOOL	IsAlive( void ) { return (pev->deadflag == DEAD_NO) && pev->health > 0; }
	virtual bool	IsDead( void ) { return pev->deadflag >= DEAD_DEAD; }
	virtual BOOL	IsBSPModel( void ) { return pev->solid == SOLID_BSP || pev->movetype == MOVETYPE_PUSHSTEP || STRING( pev->model )[0] == '*'; }
	virtual BOOL	ReflectGauss( void ) { return ( IsBSPModel() && !pev->takedamage ); }
	virtual BOOL	HasTarget( string_t targetname ) { return FStrEq(STRING(targetname), STRING(pev->targetname) ); }
	virtual BOOL    IsInWorld( void );
	virtual bool	IsMonster( void ) { return false; }
	virtual	BOOL	IsPlayer( void ) { return FALSE; }
	virtual BOOL	IsNetClient( void ) { return FALSE; }
	virtual bool	IsPointEnt( void );
	virtual bool	IsBreakable( void ) { return false; }
	virtual bool	IsMachine( void );
	virtual const char *TeamID( void ) { return ""; }
	virtual bool	CriticalRemove( void ) { return false; }


//	virtual void	SetActivator( CBaseEntity *pActivator ) {}
	virtual CBaseEntity *GetNextTarget( void );
	
	// fundamental callbacks
	void (CBaseEntity ::*m_pfnThink)(void);
	void (CBaseEntity ::*m_pfnTouch)( CBaseEntity *pOther );
	void (CBaseEntity ::*m_pfnUse)( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void (CBaseEntity ::*m_pfnBlocked)( CBaseEntity *pOther );

	virtual void Think( void ) { if (m_pfnThink) (this->*m_pfnThink)(); };
	virtual void Touch( CBaseEntity *pOther ) { if (m_pfnTouch) (this->*m_pfnTouch)( pOther ); };
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) 
	{ 
		if (m_pfnUse) 
			(this->*m_pfnUse)( pActivator, pCaller, useType, value );
	}
	virtual void Blocked( CBaseEntity *pOther ) { if (m_pfnBlocked) (this->*m_pfnBlocked)( pOther ); };

	// allow engine to allocate instance data
    void *operator new( size_t stAllocateBlock, entvars_t *pev )
	{
		return (void *)ALLOC_PRIVATE(ENT(pev), stAllocateBlock);
	};

	// don't use this.
#if _MSC_VER >= 1200 // only build this code if MSVC++ 6.0 or higher
	void operator delete(void *pMem, entvars_t *pev)
	{
		pev->flags |= FL_KILLME;
	};
#endif

	virtual void UpdateOnRemove( void );

	// common member functions
	void EXPORT SUB_Remove( void );
	void EXPORT SUB_DoNothing( void );
	void EXPORT SUB_StartFadeOut ( void );
	void EXPORT SUB_FadeOut ( void );
	void EXPORT SUB_CallUseToggle( void ) { this->Use( this, this, USE_TOGGLE, 0 ); }
	int			ShouldToggle( USE_TYPE useType, BOOL currentState );
	void FireBullets( ULONG cShots, Vector vecSrc, Vector vecDirShooting, Vector vecSpread,
		float flDistance, int iBulletType, int iTracerFreq = 4, int iDamage = 0,
		entvars_t *pevAttacker = NULL, FireBulletsDrawMode fDraw = FBDM_DRAW );

	virtual CBaseEntity *Respawn( void ) { return NULL; }

	virtual void SUB_UseTargets( CBaseEntity *pActivator, USE_TYPE useType, float value );
	// Do the bounding boxes of these two intersect?
	int		Intersects( CBaseEntity *pOther );
	int		FindMonstersInWorld( CBaseEntity **pList, int listMax, int flagMask );
	void	MakeDormant( void );
	int		IsDormant( void );
	virtual BOOL IsLockedByMaster( void ) { return FALSE; }

	static CBaseEntity *Instance( edict_t *pent )
	{ 
		if ( !pent )
			pent = ENT(0);
		CBaseEntity *pEnt = (CBaseEntity *)GET_PRIVATE(pent); 
		return pEnt; 
	}

	static CBaseEntity *Instance( entvars_t *pev ) { return Instance( ENT( pev ) ); }
	static CBaseEntity *Instance( int eoffset) { return Instance( ENT( eoffset) ); }

	CBaseMonster *GetMonsterPointer( entvars_t *pevMonster ) 
	{ 
		CBaseEntity *pEntity = Instance( pevMonster );
		if ( pEntity )
			return pEntity->MyMonsterPointer();
		return NULL;
	}
	CBaseMonster *GetMonsterPointer( edict_t *pentMonster ) 
	{ 
		CBaseEntity *pEntity = Instance( pentMonster );
		if ( pEntity )
			return pEntity->MyMonsterPointer();
		return NULL;
	}


	// Ugly code to lookup all functions to make sure they are exported when set.
#ifdef _DEBUG
	void FunctionCheck( void *pFunction, char *name ) 
	{ 
		if (pFunction && !NAME_FOR_FUNCTION((uint32)pFunction) )
			ALERT( at_error, "No EXPORT: %s:%s (%08lx)\n", STRING(pev->classname), name, (uint32)pFunction );
	}

	BASEPTR	ThinkSet( BASEPTR func, char *name ) 
	{ 
		m_pfnThink = func; 
		FunctionCheck( (void *)*((int *)((char *)this + ( offsetof(CBaseEntity,m_pfnThink)))), name ); 
		return func;
	}
	ENTITYFUNCPTR TouchSet( ENTITYFUNCPTR func, char *name ) 
	{ 
		m_pfnTouch = func; 
		FunctionCheck( (void *)*((int *)((char *)this + ( offsetof(CBaseEntity,m_pfnTouch)))), name ); 
		return func;
	}
	USEPTR	UseSet( USEPTR func, char *name ) 
	{ 
		m_pfnUse = func; 
		FunctionCheck( (void *)*((int *)((char *)this + ( offsetof(CBaseEntity,m_pfnUse)))), name ); 
		return func;
	}
	ENTITYFUNCPTR	BlockedSet( ENTITYFUNCPTR func, char *name ) 
	{ 
		m_pfnBlocked = func; 
		FunctionCheck( (void *)*((int *)((char *)this + ( offsetof(CBaseEntity,m_pfnBlocked)))), name ); 
		return func;
	}

#endif


	// virtual functions used by a few classes
	
	// used by monsters that are created by the MonsterMaker
	virtual	void UpdateOwner( void ) { return; };


	//
	static CBaseEntity *Create( const char *szName, const Vector &vecOrigin, const Vector &vecAngles, bool fCreateAndDontSpawn, edict_t *pentOwner = NULL );

	virtual bool FBecomeProne( CBaseEntity *pOther ) { return false; }
	edict_t *edict() { return ENT( pev ); };
	EOFFSET eoffset( ) { return OFFSET( pev ); };
	int	  entindex( ) { return ENTINDEX( edict() ); };

	virtual Vector Center( ) { return (pev->absmax + pev->absmin) * 0.5; }; // center point of entity
	virtual Vector EyePosition( );													// position of eyes
	virtual Vector EarPosition( ) { return pev->origin + pev->view_ofs * pev->scale; };	// position of ears
	virtual Vector BodyTarget( const Vector &posSrc ) { return Center( ); };		// position to shoot at

#ifdef CLIENT_DLL
	int Illumination( ) { return GETENTITYILLUM( ENT( pev ) ); };
#else
	virtual int Illumination( ) { return GETENTITYILLUM( ENT( pev ) ); };
#endif

	virtual	bool FVisible ( CBaseEntity *pEntity, bool fIgnoreGlass = true );
	virtual	BOOL FVisible ( const Vector &vecOrigin );
	virtual bool FVisibleFromPos( const Vector &vecStart, const Vector &vecEnd );
	virtual bool IsFacing( entvars_t *target, float fieldOfView );

	bool			BlockedByEntity( CBaseEntity *pOther, float flDamage );
	virtual float GetPointsForDamage( float flDamage )
	{
		//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
		return 0 * flDamage;
	}

	virtual void GetDamagePoints( entvars_t *inflictor, entvars_t *attacker, float damage );

	bool			IsPlayerAlly( void ) { return m_fPlayerAlly; }
	virtual void	SetPlayerAlly( bool fState ) { m_fPlayerAlly = fState; }

	virtual ~CBaseEntity();
	virtual void OnCreate( void );
	virtual void OnDestroy( void );
	virtual void	OnSetOriginByMap( void ) { }
	virtual bool	IsRevivable( void ) { return false; }
	virtual void	BeginRevive( float flTimeUntilRevive ) { }
	virtual void	EndRevive( float flTimeUntilRevive ) { }
	virtual int		ShowHUDInfo( void ) { return 2; }
	virtual bool	ShouldDisablePlayerCollisions( void ) const { return false; }
	virtual CAttackVectors *GetAttackVectors( void ) const { return NULL; }
	virtual void	Teleport( Vector vecOrigin, Vector vecAngles );
	void			SetPlayerAllyDirect( bool fState ) { m_fPlayerAlly = fState; }

	virtual void	SetTopColor( int color );
	virtual void	SetBottomColor( int color );
	virtual void	SetColors( int topColor, int bottomColor ) { SetTopColor( topColor ); SetBottomColor( bottomColor ); }
	virtual int		GetTopColor( void ) { return m_iTopColor; }
	virtual int		GetBottomColor( void ) { return m_iBottomColor; }
	virtual void	UpdateColorMap( void ) { pev->colormap = m_iTopColor + ( m_iBottomColor << COLORMAP_BOTTOM_SHIFT ); }

	bool			SetCustomKeyvalue( const char *szKeyName, const void *pValue, entvartype_s type, Ownership ownership );
	void			*GetCustomKeyvalue( const char *szKeyName, entvartype_s &type );
	bool			RemoveCustomKeyvalue( const char *szKeyName );
	bool			HasCustomKeyvalue( const char *szKeyName );
	void			*InitializeCustomKeyvalueWithDefault( const char *szKeyName, entvartype_s &type );

	//We use this variables to store each ammo count.
	int ammo_9mm;
	int ammo_357;
	int ammo_bolts;
	int ammo_buckshot;
	int ammo_rockets;
	int ammo_uranium;
	int ammo_hornets;
	int ammo_argrens;
	//Special stuff for grenades and satchels.
	float m_flStartThrow;
	float m_flReleaseThrow;
	int m_chargeReady;
	int m_fInAttack;

	enum EGON_FIRESTATE { FIRE_OFF, FIRE_CHARGE };
	int m_fireState;

#ifndef CLIENT_DLL
private:
	void CreateUserData( void );
	bool MandatoryKeyValue( KeyValueData *pkvd );
#endif
};

inline int UTIL_ClampColormapColor( int color )
{
	if ( color > MAX_COLORMAP_COLOR )
		return MAX_COLORMAP_COLOR;

	if ( color < 0 )
		return 0;

	return color;
}

inline void CBaseEntity::KeyValue( KeyValueData *pkvd )
{
	if ( pkvd->szKeyName[0] == '$' )
	{
		SetCustomKeyvalue( pkvd->szKeyName, pkvd->szValue, VAR_CUSTOM, OWNERSHIP_BY_TYPE );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "soundlist" ) )
	{
		m_iszSoundList = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "classify" ) )
	{
		SetClassificationSpecial( strtol( pkvd->szValue, NULL, 10 ), true );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "toutfiltertype" ) )
	{
		targetnameOutFilterType = strtol( pkvd->szValue, NULL, 10 );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "coutfiltertype" ) )
	{
		classnameOutFilterType = strtol( pkvd->szValue, NULL, 10 );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "tinfiltertype" ) )
	{
		targetnameInFilterType = strtol( pkvd->szValue, NULL, 10 );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "cinfiltertype" ) )
	{
		classnameInFilterType = strtol( pkvd->szValue, NULL, 10 );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "toutfilter" ) )
	{
		targetnameOutFilter = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "coutfilter" ) )
	{
		classnameOutFilter = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "tinfilter" ) )
	{
		targetnameInFilter = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "cinfilter" ) )
	{
		classnameInFilter = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "topcolor" ) )
	{
		// Players take their colors from their user info
		if ( !IsPlayer() )
			SetTopColor( strtol( pkvd->szValue, NULL, 10 ) );

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "bottomcolor" ) )
	{
		if ( !IsPlayer() )
			SetBottomColor( strtol( pkvd->szValue, NULL, 10 ) );

		pkvd->fHandled = TRUE;
	}
#ifndef CLIENT_DLL
	else if ( MandatoryKeyValue( pkvd ) )
	{
	}
#endif
	else
	{
		pkvd->fHandled = FALSE;
	}
}

inline void *CBaseEntity::GetKeyValue( const char *szKeyName, entvartype_s &type )
{
	if ( FStrEq( szKeyName, "soundlist" ) )
	{
		type = VAR_STRING;
		return &m_iszSoundList;
	}

	if ( FStrEq( szKeyName, "classify" ) )
	{
		type = VAR_INTEGER;
		return &m_iClassSelection;
	}

	if ( FStrEq( szKeyName, "toutfiltertype" ) )
	{
		type = VAR_INTEGER;
		return &targetnameOutFilterType;
	}

	if ( FStrEq( szKeyName, "coutfiltertype" ) )
	{
		type = VAR_INTEGER;
		return &classnameOutFilterType;
	}

	if ( FStrEq( szKeyName, "tinfiltertype" ) )
	{
		type = VAR_INTEGER;
		return &targetnameInFilterType;
	}

	if ( FStrEq( szKeyName, "cinfiltertype" ) )
	{
		type = VAR_INTEGER;
		return &classnameInFilterType;
	}

	if ( FStrEq( szKeyName, "toutfilter" ) )
	{
		type = VAR_STRING;
		return &targetnameOutFilter;
	}

	if ( FStrEq( szKeyName, "coutfilter" ) )
	{
		type = VAR_STRING;
		return &classnameOutFilter;
	}

	if ( FStrEq( szKeyName, "tinfilter" ) )
	{
		type = VAR_STRING;
		return &targetnameInFilter;
	}

	if ( FStrEq( szKeyName, "cinfilter" ) )
	{
		type = VAR_STRING;
		return &classnameInFilter;
	}

	if ( FStrEq( szKeyName, "topcolor" ) )
	{
		type = VAR_INTEGER;
		return &m_iTopColor;
	}

	if ( FStrEq( szKeyName, "bottomcolor" ) )
	{
		type = VAR_INTEGER;
		return &m_iBottomColor;
	}

	type = VAR_INVALID;
	return NULL;
}

// Called after a value returned by GetKeyValue or an entity variable was changed directly
inline void CBaseEntity::OnKeyValueUpdate( const char *szKeyName )
{
	if ( FStrEq( szKeyName, "classify" ) )
	{
		m_fOverrideClass = true;
		pev->team = MAKE_STRING( GetClassificationTag() );
	}
	else if ( FStrEq( szKeyName, "topcolor" ) || FStrEq( szKeyName, "bottomcolor" ) )
	{
		if ( IsPlayer() )
		{
			m_iTopColor = pev->colormap & MAX_COLORMAP_COLOR;
			m_iBottomColor = ( pev->colormap >> COLORMAP_BOTTOM_SHIFT ) & MAX_COLORMAP_COLOR;
		}
		else
		{
			m_iTopColor = UTIL_ClampColormapColor( m_iTopColor );
			m_iBottomColor = UTIL_ClampColormapColor( m_iBottomColor );

			UpdateColorMap();
		}
	}
	else if ( FStrEq( szKeyName, "rendermode" ) )
	{
		m_iOriginalRenderMode = pev->rendermode;
	}
	else if ( FStrEq( szKeyName, "renderamt" ) )
	{
		m_flOriginalRenderAmount = pev->renderamt;
	}
	else if ( FStrEq( szKeyName, "rendercolor" ) )
	{
		m_vecOriginalRenderColor = pev->rendercolor;
	}
	else if ( FStrEq( szKeyName, "renderfx" ) )
	{
		m_iOriginalRenderFX = pev->renderfx;
	}
}

inline void CBaseEntity::SetClassificationSpecial( int cl, bool fKeepOverride )
{
	if ( ( cl >= CLASS_MACHINE && cl <= CLASS_TEAM4 ) || cl == CLASS_BARNACLE )
	{
		m_fOverrideClass = true;
		m_iClassSelection = cl;
	}
	else if ( cl == CLASS_FORCE_NONE )
	{
		m_fOverrideClass = true;
		m_iClassSelection = CLASS_NONE;
	}
	else if ( !fKeepOverride )
	{
		m_fOverrideClass = false;
		m_iClassSelection = CLASS_NONE;
	}

	pev->team = MAKE_STRING( GetClassificationTag() );
}

inline void CBaseEntity::SetClassificationFromEntity( CBaseEntity *pEntity )
{
	if ( pEntity )
		SetClassification( pEntity->m_iClassSelection );
}

inline void CBaseEntity::ClearClassification( void )
{
	if ( !m_fOverrideClass )
		return;

	m_fOverrideClass = false;
	m_iClassSelection = CLASS_NONE;
	pev->team = 0;
}

inline bool CBaseEntity::IsMachine( void )
{
	const char *pszClassname = STRING( pev->classname );

	return FStrEq( pszClassname, "monster_turret" )
		|| FStrEq( pszClassname, "monster_miniturret" )
		|| FStrEq( pszClassname, "monster_sentry" )
		|| FStrEq( pszClassname, "monster_robogrunt" );
}

inline Vector CBaseEntity::EyePosition( void )
{
	if ( IsBSPModel() )
		return Center();

	return pev->origin + pev->view_ofs * pev->scale;
}

inline void CBaseEntity::SetTopColor( int color )
{
	m_iTopColor = UTIL_ClampColormapColor( color );
	UpdateColorMap();
}

inline void CBaseEntity::SetBottomColor( int color )
{
	m_iBottomColor = UTIL_ClampColormapColor( color );
	UpdateColorMap();
}



// Ugly technique to override base member functions
// Normally it's illegal to cast a pointer to a member function of a derived class to a pointer to a 
// member function of a base class.  static_cast is a sleezy way around that problem.

#ifdef _DEBUG

#define SetThink( a ) ThinkSet( static_cast <void (CBaseEntity::*)(void)> (a), #a )
#define SetTouch( a ) TouchSet( static_cast <void (CBaseEntity::*)(CBaseEntity *)> (a), #a )
#define SetUse( a ) UseSet( static_cast <void (CBaseEntity::*)(	CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )> (a), #a )
#define SetBlocked( a ) BlockedSet( static_cast <void (CBaseEntity::*)(CBaseEntity *)> (a), #a )

#else

#define SetThink( a ) m_pfnThink = static_cast <void (CBaseEntity::*)(void)> (a)
#define SetTouch( a ) m_pfnTouch = static_cast <void (CBaseEntity::*)(CBaseEntity *)> (a)
#define SetUse( a ) m_pfnUse = static_cast <void (CBaseEntity::*)( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )> (a)
#define SetBlocked( a ) m_pfnBlocked = static_cast <void (CBaseEntity::*)(CBaseEntity *)> (a)

#endif


class CPointEntity : public CBaseEntity
{
public:
	void	Spawn( void );
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
private:
};


typedef struct locksounds			// sounds that doors and buttons make when locked/unlocked
{
	string_t	sLockedSound;		// sound a door makes when it's locked
	string_t	sLockedSentence;	// sentence group played when door is locked
	string_t	sUnlockedSound;		// sound a door makes when it's unlocked
	string_t	sUnlockedSentence;	// sentence group played when door is unlocked

	int		iLockedSentence;		// which sentence in sentence group to play next
	int		iUnlockedSentence;		// which sentence in sentence group to play next

	float	flwaitSound;			// time delay between playing consecutive 'locked/unlocked' sounds
	float	flwaitSentence;			// time delay between playing consecutive sentences
	BYTE	bEOFLocked;				// true if hit end of list of locked sentences
	BYTE	bEOFUnlocked;			// true if hit end of list of unlocked sentences
} locksound_t;

struct LockSoundsIndices
{
	BYTE	m_bLockedSound;
	BYTE	m_bLockedSentence;
	BYTE	m_bUnlockedSound;
	BYTE	m_bUnlockedSentence;
};

void PlayLockSounds(entvars_t *pev, locksound_t *pls, int flocked, int fbutton);

//
// MultiSouce
//

#define MAX_MULTI_TARGETS	64 // maximum number of targets a single multi_manager entity may be assigned.
#define MS_MAX_TARGETS 64

class CMultiSource : public CPointEntity
{
public:
	void Spawn( );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int	ObjectCaps( void ) { return (CPointEntity::ObjectCaps() | FCAP_MASTER); }
	BOOL IsTriggered( CBaseEntity *pActivator );
	void EXPORT Register( void );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	EHANDLE		m_rgEntities[MS_MAX_TARGETS];
	int			m_rgTriggered[MS_MAX_TARGETS];

	int			m_iTotal;
	string_t	m_globalstate;
};


//
// generic Delay entity.
//
class CBaseDelay : public CBaseEntity
{
public:
#ifndef CLIENT_DLL
	CBaseDelay();
#endif

	float	m_flDelay;
	int		m_iszKillTarget;
#ifndef CLIENT_DLL
	EHANDLE	m_hDelayActivator;
#endif

	virtual void	KeyValue( KeyValueData* pkvd);
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];
	// common member functions
	void SUB_UseTargets( CBaseEntity *pActivator, USE_TYPE useType, float value );
#ifndef CLIENT_DLL
	virtual void EXPORT DelayThink( void );
	void EXPORT CallDelayThink( void ) { DelayThink(); }
#else
	void EXPORT DelayThink( void );
#endif
};


class CBaseAnimating : public CBaseDelay
{
public:
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	// Basic Monster Animation functions
	float StudioFrameAdvance( float flInterval = 0.0 ); // accumulate animation frame time from last time called until now
	int	 GetSequenceFlags( void );
	int  LookupActivity ( int activity );
	int  LookupActivityHeaviest ( int activity );
	int  LookupSequence ( const char *label );
	void ResetSequenceInfo ( );
	void ResetGaitSequenceInfo( void );
	int GetSequenceActivityName( int iSequence );
	void DispatchAnimEvents ( float flFutureInterval = 0.1 ); // Handle events that have happend since last time called up until X seconds into the future
	virtual void HandleAnimEvent( MonsterEvent_t *pEvent ) { return; };
	float SetBoneController ( int iController, float flValue );
	void InitBoneControllers ( void );
	float SetBlending ( int iBlender, float flValue );
	void GetBonePosition ( int iBone, Vector &origin, Vector &angles );
	void GetAutomovement( Vector &origin, Vector &angles, float flInterval = 0.1 );
	int  FindTransition( int iEndingSequence, int iGoalSequence, int *piDir );
	void GetAttachment ( int iAttachment, Vector &origin, Vector &angles );
	int GetAttachmentCount( void );
	void SetBodygroup( int iGroup, int iValue );
	int GetBodygroup( int iGroup );
	int GetBodygroupSubmodelCount( int iGroup );
	int ExtractBbox( int sequence, float *mins, float *maxs );
	void SetSequenceBox( void );

	// animation needs
	float				m_flFrameRate;		// computed FPS for current sequence
	float				m_flGroundSpeed;	// computed linear movement rate for current sequence
	float				m_flLastEventCheck;	// last time the event list was checked
	float				m_flLastGaitEventCheck;	// last time the gait event list was checked
	BOOL				m_fSequenceFinished;// flag set when StudioAdvanceFrame moves across a frame boundry
	BOOL				m_fSequenceLoops;	// true if the sequence loops
};


//
// generic Toggle entity.
//
#define	SF_ITEM_USE_ONLY	256 //  ITEM_USE_ONLY = BUTTON_USE_ONLY = DOOR_USE_ONLY!!! 

class CInventoryRules;

class CBaseToggle : public CBaseAnimating
{
public:
	void				KeyValue( KeyValueData *pkvd );

	void OnCreate( void );
	void OnDestroy( void );

	TOGGLE_STATE		m_toggle_state;
	float				m_flActivateFinished;//like attack_finished, but for doors
	float				m_flMoveDistance;// how far a door should slide or rotate
	float				m_flWait;
	float				m_flLip;
	float				m_flTWidth;// for plats
	float				m_flTLength;// for plats

	Vector				m_vecPosition1;
	Vector				m_vecPosition2;
	Vector				m_vecAngle1;
	Vector				m_vecAngle2;

	int					m_cTriggersLeft;		// trigger_counter only, # of activations remaining
	float				m_flHeight;
	EHANDLE				m_hActivator;
	void (CBaseToggle::*m_pfnCallWhenMoveDone)(void);
	Vector				m_vecFinalDest;
	Vector				m_vecFinalAngle;

	int					m_bitsDamageInflict;	// DMG_ damage type that the door or tigger does

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	virtual int		GetToggleState( void ) { return m_toggle_state; }
	virtual float	GetDelay( void ) { return m_flWait; }

	virtual void RecalculateVectors( void ) { }

	// common member functions
	void LinearMove( Vector	vecDest, float flSpeed );
	void EXPORT LinearMoveDone( void );
	void AngularMove( Vector vecDestAngle, float flSpeed );
	void EXPORT AngularMoveDone( void );
	virtual BOOL IsLockedByMaster( void );

	static float		AxisValue( int flags, const Vector &angles );
	static void			AxisDir( entvars_t *pev );
	static float		AxisDelta( int flags, const Vector &angle1, const Vector &angle2 );

	string_t m_sMaster;		// If this button has a master switch, this is the targetname.
							// A master switch must be of the multisource type. If all 
							// of the switches in the multisource have been triggered, then
							// the button will be allowed to operate. Otherwise, it will be
							// deactivated.

#ifndef CLIENT_DLL
	CInventoryRules *m_pInventoryRules;
#endif
};
#define SetMoveDone( a ) m_pfnCallWhenMoveDone = static_cast <void (CBaseToggle::*)(void)> (a)


// people gib if their health is <= this at the time of death
#define	GIB_HEALTH_VALUE	-50

#define	ROUTE_SIZE			8 // how many waypoints a monster can store at one time
#define MAX_OLD_ENEMIES		4 // how many old enemies to remember

#define	bits_CAP_DUCK			( 1 << 0 )// crouch
#define	bits_CAP_JUMP			( 1 << 1 )// jump/leap
#define bits_CAP_STRAFE			( 1 << 2 )// strafe ( walk/run sideways)
#define bits_CAP_SQUAD			( 1 << 3 )// can form squads
#define	bits_CAP_SWIM			( 1 << 4 )// proficiently navigate in water
#define bits_CAP_CLIMB			( 1 << 5 )// climb ladders/ropes
#define bits_CAP_USE			( 1 << 6 )// open doors/push buttons/pull levers
#define bits_CAP_HEAR			( 1 << 7 )// can hear forced sounds
#define bits_CAP_AUTO_DOORS		( 1 << 8 )// can trigger auto doors
#define bits_CAP_OPEN_DOORS		( 1 << 9 )// can open manual doors
#define bits_CAP_TURN_HEAD		( 1 << 10)// can turn head, always bone controller 0

#define bits_CAP_RANGE_ATTACK1	( 1 << 11)// can do a range attack 1
#define bits_CAP_RANGE_ATTACK2	( 1 << 12)// can do a range attack 2
#define bits_CAP_MELEE_ATTACK1	( 1 << 13)// can do a melee attack 1
#define bits_CAP_MELEE_ATTACK2	( 1 << 14)// can do a melee attack 2

#define bits_CAP_FLY			( 1 << 15)// can fly, move all around
#define bits_CAP_USE_TANK		( 1 << 16)// can man a func_tank
#define bits_CAP_FALL_DAMAGE	( 1 << 17)// hurt by falls (CBaseMonster::CheckFallDamage)

#define bits_CAP_DOORS_GROUP    (bits_CAP_USE | bits_CAP_AUTO_DOORS | bits_CAP_OPEN_DOORS)

// used by suit voice to indicate damage sustained and repaired type to player

// instant damage

#define DMG_GENERIC			0			// generic damage was done
#define DMG_CRUSH			(1 << 0)	// crushed by falling or moving object
#define DMG_BULLET			(1 << 1)	// shot
#define DMG_SLASH			(1 << 2)	// cut, clawed, stabbed
#define DMG_BURN			(1 << 3)	// heat burned
#define DMG_FREEZE			(1 << 4)	// frozen
#define DMG_FALL			(1 << 5)	// fell too far
#define DMG_BLAST			(1 << 6)	// explosive blast damage
#define DMG_CLUB			(1 << 7)	// crowbar, punch, headbutt
#define DMG_SHOCK			(1 << 8)	// electric shock
#define DMG_SONIC			(1 << 9)	// sound pulse shockwave
#define DMG_ENERGYBEAM		(1 << 10)	// laser or other high energy beam 
#define DMG_NEVERGIB		(1 << 12)	// with this bit OR'd in, no damage type will be able to gib victims upon death
#define DMG_ALWAYSGIB		(1 << 13)	// with this bit OR'd in, any damage type can be made to gib victims upon death.
#define DMG_DROWN			(1 << 14)	// Drowning
// time-based damage
#define DMG_TIMEBASED		(~(0x3fff))	// mask for time-based damage

#define DMG_PARALYZE		(1 << 15)	// slows affected creature down
#define DMG_NERVEGAS		(1 << 16)	// nerve toxins, very bad
#define DMG_POISON			(1 << 17)	// blood poisioning
#define DMG_RADIATION		(1 << 18)	// radiation exposure
#define DMG_DROWNRECOVER	(1 << 19)	// drowning recovery
#define DMG_ACID			(1 << 20)	// toxic chemicals or acid burns
#define DMG_SLOWBURN		(1 << 21)	// in an oven
#define DMG_SLOWFREEZE		(1 << 22)	// in a subzero freezer
#define DMG_MORTAR			(1 << 23)	// Hit by air raid (done to distinguish grenade from mortar)

#define DMG_SNIPER			(1 << 24)
#define DMG_MEDKITHEAL		(1 << 25)
#define DMG_BUCKSHOT			(1 << 26)
#define DMG_LAUNCH			DMG_BUCKSHOT	// the AngelScript name for the same bit
#define DMG_SHOCK_GLOW		( 1 << 27 )

// these are the damage types that are allowed to gib corpses
#define DMG_GIB_CORPSE		( DMG_CRUSH | DMG_FALL | DMG_BLAST | DMG_SONIC | DMG_CLUB | DMG_SHOCK )

// these are the damage types that have client hud art
#define DMG_SHOWNHUD		(DMG_POISON | DMG_ACID | DMG_FREEZE | DMG_SLOWFREEZE | DMG_DROWN | DMG_BURN | DMG_SLOWBURN | DMG_NERVEGAS | DMG_RADIATION | DMG_SHOCK)

// NOTE: tweak these values based on gameplay feedback:

#define PARALYZE_DURATION	2		// number of 2 second intervals to take damage
#define PARALYZE_DAMAGE		1.0		// damage to take each 2 second interval

#define NERVEGAS_DURATION	2
#define NERVEGAS_DAMAGE		5.0

#define POISON_DURATION		5
#define POISON_DAMAGE		2.0

#define RADIATION_DURATION	2
#define RADIATION_DAMAGE	1.0

#define ACID_DURATION		2
#define ACID_DAMAGE			5.0

#define SLOWBURN_DURATION	2
#define SLOWBURN_DAMAGE		1.0

#define SLOWFREEZE_DURATION	2
#define SLOWFREEZE_DAMAGE	1.0


#define	itbd_Paralyze		0		
#define	itbd_NerveGas		1
#define	itbd_Poison			2
#define	itbd_Radiation		3
#define	itbd_DrownRecover	4
#define	itbd_Acid			5
#define	itbd_SlowBurn		6
#define	itbd_SlowFreeze		7
#define CDMG_TIMEBASED		8

// when calling KILLED(), a value that governs gib behavior is expected to be 
// one of these three values
#define GIB_NORMAL			0// gib if entity was overkilled
#define GIB_NEVER			1// never gib, no matter how much death damage is done ( freezing, etc )
#define GIB_ALWAYS			2// always gib ( Houndeye Shock, Barnacle Bite )

class CBaseMonster;
class CCineMonster;
class CSound;

#include "basemonster.h"


const char *ButtonSound( int sound );				// get string of button sound number


//
// Generic Button
//
class CBaseButton : public CBaseToggle
{
public:
	CBaseButton() : m_iUseType( USE_TOGGLE ) {}

	void Spawn( void );
	virtual void Precache( void );
#ifndef CLIENT_DLL
	virtual void OnSetOriginByMap( void );
	virtual void RecalculateVectors( void );
	void UpdateOnRemove( void );
#endif

	void RotSpawn( void );
	virtual void KeyValue( KeyValueData* pkvd);

	void ButtonActivate( );
	void SparkSoundCache( void );

	void EXPORT ButtonShot( void );
	void EXPORT ButtonTouch( CBaseEntity *pOther );
	void EXPORT ButtonSpark ( void );
	void EXPORT TriggerAndWait( void );
	void EXPORT ButtonReturn( void );
	void EXPORT ButtonBackHome( void );
	void EXPORT ButtonUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int		TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	enum BUTTON_CODE { BUTTON_NOTHING, BUTTON_ACTIVATE, BUTTON_RETURN };
	BUTTON_CODE	ButtonResponseToTouch( void );
	
	static	TYPEDESCRIPTION m_SaveData[];
	// Buttons that don't take damage can be IMPULSE used
	virtual int	ObjectCaps( void ) { return (CBaseToggle:: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | (pev->takedamage?0:FCAP_IMPULSE_USE); }

	bool	m_fStayPushed;	// button stays pushed in until touched again?
	bool	m_fRotating;		// a rotating button?  default is a sliding button.

	string_t m_strChangeTarget;	// if this field is not null, this is an index into the engine string array.
							// when this button is touched, it's target entity's TARGET field will be set
							// to the button's ChangeTarget. This allows you to make a func_train switch paths, etc.

	locksound_t m_ls;			// door lock sounds
	
	LockSoundsIndices m_LockSoundsIndices;

	int		m_sounds;
	int		m_iUseType;
};

//
// Weapons 
//

#define	BAD_WEAPON 0x00007FFF

//
// Converts a entvars_t * to a class pointer
// It will allocate the class and entity if necessary
//
template <class T> T * GetClassPtr( T *a )
{
	entvars_t *pev = (entvars_t *)a;

	// allocate entity if necessary
	if (pev == NULL)
		pev = VARS(CREATE_ENTITY());

	// get the private data
	a = (T *)GET_PRIVATE(ENT(pev));

	if (a == NULL) 
	{
		// allocate private data 
		a = new(pev) T;
		a->pev = pev;

#ifndef CLIENT_DLL
		a->OnCreate();
#endif
	}
	return a;
}


/*
bit_PUSHBRUSH_DATA | bit_TOGGLE_DATA
bit_MONSTER_DATA
bit_DELAY_DATA
bit_TOGGLE_DATA | bit_DELAY_DATA | bit_MONSTER_DATA
bit_PLAYER_DATA | bit_MONSTER_DATA
bit_MONSTER_DATA | CYCLER_DATA
bit_LIGHT_DATA
path_corner_data
bit_MONSTER_DATA | wildcard_data
bit_MONSTER_DATA | bit_GROUP_DATA
boid_flock_data
boid_data
CYCLER_DATA
bit_ITEM_DATA
bit_ITEM_DATA | func_hud_data
bit_TOGGLE_DATA | bit_ITEM_DATA
EOFFSET
env_sound_data
env_sound_data
push_trigger_data
*/

#define TRACER_FREQ		4			// Tracers fire every 4 bullets

typedef struct _SelAmmo
{
	BYTE	Ammo1Type;
	BYTE	Ammo1;
	BYTE	Ammo2Type;
	BYTE	Ammo2;
} SelAmmo;


// this moved here from world.cpp, to allow classes to be derived from it
//=======================
// CWorld
//
// This spawns first when each level begins.
//=======================
class CWorld : public CBaseEntity
{
public:
#ifndef CLIENT_DLL
	static CWorld *GetInstance( void );
	void OnCreate( void );
	void OnDestroy( void );
	int ShowHUDInfo( void ) { return 0; }
#endif

	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );

#ifndef CLIENT_DLL
	void SetupEntVars( void );
	const char *MODELREPLACEMENT_Find( const char *model );
	void PrecacheGlobalCustomModels( string_t fileName );
	void PrecacheStandardWeaponModels( void );
	void WeaponModelKey( KeyValueData *pkvd );

	char m_szSentenceFile[256];

	CStringMap m_ModelReplacementMap;

private:
	static CWorld *m_pInstance;
#endif
};
