#ifndef PLATS_H
#define PLATS_H

#include "doors.h"
#include "TypedEntityHandle.h"
#include "sound_message.h"

#define SF_PLAT_TOGGLE		0x0001

class CBasePlatTrain : public CBaseToggle
{
public:
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void KeyValue( KeyValueData* pkvd);
	void Precache( void );

	// This is done to fix spawn flag collisions between this class and a derived class
	virtual BOOL IsTogglePlat( void ) { return (pev->spawnflags & SF_PLAT_TOGGLE) ? TRUE : FALSE; }

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	BYTE	m_bMoveSnd;			// sound a plat makes while moving
	BYTE	m_fMoveSoundLoop;	// force the moving sound to loop
	BYTE	m_bStopSnd;			// sound a plat makes when it stops
	float	m_volume;			// Sound volume

	CStartStopVars m_StartStopVars;

	int		MoveSoundFlags( void )
	{
		if ( m_fMoveSoundLoop )
			return SOUNDMSG_FORCE_LOOP;

		return 0;
	}
};

class CFuncTrain : public CBasePlatTrain
{
public:
	CFuncTrain()
	{
		m_StartStopVars.m_iszFireOnStart = 0;
		m_StartStopVars.m_iFireOnStartTriggerState = USE_TOGGLE;
		m_StartStopVars.m_iszFireOnStop = 0;
		m_StartStopVars.m_iFireOnStopTriggerState = USE_TOGGLE;
	}

	void Spawn( void );
	void Precache( void );
	void Activate( void );
	void OverrideReset( void );
	virtual void UpdateOnRemove( void );

	void Blocked( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData *pkvd );


	void EXPORT Wait( void );
	void EXPORT Next( void );
	void MoveSound( bool start, bool stopped );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	CEntityHandle<CBaseEntity> m_hCurrentTarget;

	bool	m_activated;
	bool	m_bRandomNextTarget;
	int		m_iObeyTriggerMode;
};

#endif
