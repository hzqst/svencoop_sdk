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
#ifndef TRAINS_H
#define TRAINS_H

#include "TypedEntityHandle.h"

// Tracktrain spawn flags
#define SF_TRACKTRAIN_NOPITCH		0x0001
#define SF_TRACKTRAIN_NOCONTROL		0x0002
#define SF_TRACKTRAIN_FORWARDONLY	0x0004
#define SF_TRACKTRAIN_PASSABLE		0x0008
#define SF_TRACKTRAIN_NOYAW			0x0010

// Spawnflag for CPathTrack
#define SF_PATH_DISABLED		0x00000001
#define SF_PATH_FIREONCE		0x00000002
#define SF_PATH_ALTREVERSE		0x00000004
#define SF_PATH_DISABLE_TRAIN	0x00000008
#define SF_PATH_ALTERNATE		0x00008000

// Spawnflags of CPathCorner
#define SF_CORNER_WAITFORTRIG	0x001
#define SF_CORNER_TELEPORT		0x002
#define SF_CORNER_FIREONCE		0x004
#define SF_CORNER_RANDOM_TARGET	0x008

//#define PATH_SPARKLE_DEBUG		1	// This makes a particle effect around path_track entities for debugging
class CPathTrack : public CPointEntity
{
public:
	CPathTrack() : m_hNext(), m_hPrevious(), m_hAltpath() {}

	void OnCreate() override;
	void		Spawn( void );
	void		Activate( void );
	void		KeyValue( KeyValueData* pkvd);
	
	void		SetPrevious( CPathTrack *pprevious );
	void		Link( void );
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	CPathTrack	*ValidPath( CPathTrack *ppath, int testFlag );		// Returns ppath if enabled, NULL otherwise
	void		Project( CPathTrack *pstart, CPathTrack *pend, Vector *origin, float dist );

	static CPathTrack *Instance( edict_t *pent );

	CPathTrack	*LookAhead( Vector *origin, float dist, int move );
	CPathTrack	*Nearest( Vector origin );

	CPathTrack	*GetNext( void );
	CPathTrack	*GetPrevious( void );
	int GetGoalDirection( int goalName, CBaseEntity *train );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];
#if PATH_SPARKLE_DEBUG
	void EXPORT Sparkle(void);
#endif

	float		m_length;
	string_t	m_altName;

	float	m_flNewSpeed;
	float	m_flMaxSpeed;

	CEntityHandle<CPathTrack>	m_hNext;
	CEntityHandle<CPathTrack>	m_hPrevious;
	CEntityHandle<CPathTrack>	m_hAltpath;

	bool m_bVisited;
};


class CFuncTrackTrain : public CBaseEntity
{
public:
	CFuncTrackTrain() : m_hPath(), m_Unresolved150( 0 ), m_Unresolved154( 0 ), m_hDeadEndPath() {}

	void Spawn( void );
	void Precache( void );

	void Blocked( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData* pkvd );

	void EXPORT Next( void );
	void EXPORT Find( void );
	void EXPORT NearestPath( void );
	void EXPORT DeadEnd( void );
	CPathTrack *FindDeadEnd();
	void SetMaxSpeed( float speed );

	void		NextThink( float thinkTime, BOOL alwaysThink );

	void SetTrack( CPathTrack *track ) { m_hPath = track->Nearest( pev->origin ); }
	void SetControls( entvars_t *pevControls );
	BOOL OnControls( entvars_t *pev );
	virtual bool ShouldDisablePlayerCollisions( void ) const;

	void StopSound ( void );
	void UpdateSound ( void );
	
	static CFuncTrackTrain *Instance( edict_t *pent );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];
	virtual int	ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_DIRECTIONAL_USE; }

	virtual void	OverrideReset( void );

	string_t					m_iszGoal;
	CEntityHandle<CPathTrack>	m_hPath;

	unsigned int	m_Unresolved150;
	unsigned int	m_Unresolved154;

	CEntityHandle<CPathTrack> m_hDeadEndPath;

	float		m_length;
	float		m_height;
	float		m_flMaxSpeed;
	float		m_dir;
	float		m_startSpeed;
	Vector		m_controlMins;
	Vector		m_controlMaxs;
	int			m_soundPlaying;
	int			m_sounds;
	float		m_flVolume;
	float		m_flBank;
	float		m_oldSpeed;

private:
	float			m_flLastSoundSpeed;
	unsigned short	m_usAdjustPitch;
};

#endif
