#ifndef AGRUNT_H
#define AGRUNT_H

#include "squadmonster.h"

class CAGrunt : public CSquadMonster
{
public:
	float GetPointsForDamage( float flDamage );
	int ObjectCaps( void );
	void UseSound( void );
	void UnUseSound( void );

	void Spawn( void );
	void DeathNotice( entvars_t *child );
	void Precache( void );
	void SetupFriendly( void );
	int TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType );
	void Killed( entvars_t *attacker, int gib );
	void SetYawSpeed ( void );
	int  Classify ( void );
	int  ISoundMask ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void SetObjectCollisionBox( void )
	{
		pev->absmin = pev->origin + Vector( -32, -32, 0 );
		pev->absmax = pev->origin + Vector( 32, 32, 85 );
	}

	Schedule_t* GetSchedule ( void );
	Schedule_t* GetScheduleOfType ( int Type );
	BOOL FCanCheckAttacks ( void );
	BOOL CheckMeleeAttack1( float flDot, float flDist );
	BOOL CheckMeleeAttack1_Move( float flDot, float flDist );
	BOOL CheckRangeAttack1( float flDot, float flDist );
	BOOL CheckRangeAttack1_Move( float flDot, float flDist );
	BOOL CheckRangeAttack2( float flDot, float flDist );
	void StartTask( Task_t *pTask );
	void RunTask( Task_t *pTask );
	void AlertSound( void );
	void DeathSound ( void );
	void PainSound ( void );
	void AttackSound ( void );
	void PrescheduleThink ( void );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	int IRelationship( CBaseEntity *pTarget, bool ignoreNoTarget = false );
	void StopTalking ( void );
	BOOL ShouldSpeak( void );
	CUSTOM_SCHEDULES;

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	static const char *pAttackHitSounds[];
	static const char *pAttackMissSounds[];
	static const char *pAttackSounds[];
	static const char *pDieSounds[];
	static const char *pPainSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];

	bool	m_fCanHornetAttack;
	float	m_flNextHornetAttackCheck;

	float m_flNextPainTime;

	int m_iMuzzleFlash;

	// three hacky fields for speech stuff. These don't really need to be saved.
	float	m_flNextSpeakTime;
	float	m_flNextWordTime;
	int		m_iLastWord;

	int m_iHideAttackAttempts;

	float  m_flNextGrenadeCheck;
	Vector m_vecTossVelocity;
	bool   m_fThrowGrenade;
	int    m_iNumSnarks;
};

#endif
