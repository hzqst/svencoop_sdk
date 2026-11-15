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
#ifndef DOORS_H
#define DOORS_H

// doors
#define SF_DOOR_ROTATE_Y			0
#define	SF_DOOR_START_OPEN			1
#define SF_DOOR_ROTATE_BACKWARDS	2
#define SF_DOOR_PASSABLE			8
#define SF_DOOR_ONEWAY				16
#define	SF_DOOR_NO_AUTO_RETURN		32
#define SF_DOOR_ROTATE_Z			64
#define SF_DOOR_ROTATE_X			128
#define SF_DOOR_USE_ONLY			256	// door must be opened by player's use button.
#define SF_DOOR_NOMONSTERS			512	// Monster can't open
#define SF_DOOR_SILENT				0x80000000

#include "attackvectors.h"
#include "EntityHandle.h"
#include "func_break.h"

bool HandleLockSounds( locksound_t &sounds, LockSoundsIndices &indices, KeyValueData *pkvd );
void SetLockSounds( CBaseEntity *pEntity, locksound_t &sounds, LockSoundsIndices &indices );
const char *DoorMovingSound( int iSound );
const char *DoorStopSound( int iSound );
const char *LockedSentenceSound( int iSound );
const char *UnlockedSentenceSound( int iSound );

class CStartStopVars
{
public:
	bool KeyValue( KeyValueData *pkvd );
	void TriggerStartedTargets( CBaseEntity *owner, EntityHandle<CBaseEntity> &activator );
	void TriggerStoppedTargets( CBaseEntity *owner, EntityHandle<CBaseEntity> &activator );

	string_t	m_iszFireOnStart;
	int			m_iFireOnStartTriggerState;
	string_t	m_iszFireOnStop;
	int			m_iFireOnStopTriggerState;
};

class COpenClosableVars
{
public:
	void TriggerMoveStartedTargets( CBaseEntity *owner, EntityHandle<CBaseEntity> &activator,
	                                bool movingUp, bool startOpen );
	void TriggerMoveFinishedTargets( CBaseEntity *owner, EntityHandle<CBaseEntity> &activator,
	                                 bool movingUp, bool startOpen );

	string_t	m_iszFireOnOpening;
	int			m_iFireOnOpeningTriggerState;
	string_t	m_iszFireOnClosing;
	int			m_iFireOnClosingTriggerState;
	string_t	m_iszFireOnOpened;
	int			m_iFireOnOpenedTriggerState;
	string_t	m_iszFireOnClosed;
	int			m_iFireOnClosedTriggerState;
};

class CBaseDoor : public CBaseToggle
{
public:
	CBaseDoor();

	void Spawn( void );
	void Precache( void );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void Blocked( CBaseEntity *pOther );
	virtual void OnSetOriginByMap( void );
	virtual void RecalculateVectors( void );
	virtual void UpdateOnRemove( void );
	virtual void *GetKeyValue( const char *name, entvartype_s &type );
	virtual bool IsBreakable( void );
	virtual int ShowHUDInfo( void );
	virtual CAttackVectors *GetAttackVectors( void ) const;
	virtual int DamageDecal( int damageType );
	virtual void TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType );
	virtual int TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType );

	void DamageSound( void );
	bool CheckAttacker( CBaseEntity *attacker );
	bool CanInstantBreak( CBaseEntity *attacker, int weapon );
	void LogDamage( float damage, Vector direction );
	void EXPORT Die( void );
	void CheckDoorFriends( void );
	bool DoorChangeDirection( void );

	virtual int ObjectCaps( void )
	{
		return pev->spawnflags & SF_DOOR_USE_ONLY ? FCAP_IMPULSE_USE : 0;
	}

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );

	static TYPEDESCRIPTION m_SaveData[];

	virtual void SetToggleState( int state );

	// used to selectivly override defaults
	void EXPORT DoorTouch( CBaseEntity *pOther );

	// local functions
	int DoorActivate();
	void EXPORT DoorGoUp( void );
	void EXPORT DoorGoDown( void );
	void EXPORT DoorHitTop( void );
	void EXPORT DoorHitBottom( void );

	BYTE m_bHealthValue; // some doors are medi-kit doors, they give players health

	BYTE	m_bMoveSnd; // sound a door makes while moving
	bool	m_fMoveSoundLoop;
	BYTE	m_bStopSnd; // sound a door makes when it stops

	locksound_t m_ls; // door lock sounds

	LockSoundsIndices m_LockSoundsIndices;

	int			m_iOpenFlags;
	bool		m_fIgnoreTargetname;
	int			m_iObeyTriggerMode;
	string_t	m_iszFireOnBreak;
	string_t	m_iszDisplayName;

	Materials	m_Material;
	Explosions	m_Explosion;
	int			m_idShard;
	float		m_angle;
	string_t	m_iszGibModel;
	string_t	m_iszSpawnObject;
	int			m_instantBreakWeapon;
	bool		m_fBrokenByTrigger;

	float	m_flDamage[BREAKABLE_DAMAGE_HISTORY];
	Vector	m_vecDamageDirection[BREAKABLE_DAMAGE_HISTORY];
	float	m_flDamageTime[BREAKABLE_DAMAGE_HISTORY];
	int		m_iDamageIndex;

	bool	m_fBreakable;
	bool	m_fOnlyTrigger;
	bool	m_fBreakOnTrigger;
	bool	m_fRepairable;
	bool	m_fShowHUDInfo;
	bool	m_fImmuneToClients;
	bool	m_fInstantBreak;
	bool	m_fExplosivesOnly;

	CAttackVectors m_AttackVectors;

	CStartStopVars		m_StartStopVars;
	COpenClosableVars	m_OpenClosableVars;
};


#endif //DOORS_H
