#ifndef SCIENTIST_H
#define SCIENTIST_H

#include "talkmonster.h"

class CScientist : public CTalkMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void Think( void );
	void SetYawSpeed( void );
	int Classify( void );
	void SetPlayerAlly( bool fState );
	float GetPointsForDamage( float flDamage );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void RunTask( Task_t *pTask );
	void StartTask( Task_t *pTask );
	int ObjectCaps( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	virtual int FriendNumber( int arrayNumber );
	void SetActivity( Activity newActivity );
	Activity GetStoppedActivity( void );
	int ISoundMask( void );
	void DeclineFollowing( void );

	float CoverRadius( void ) { return 1200; }
	BOOL DisregardEnemy( CBaseEntity *pEnemy ) { return !pEnemy->IsAlive() || ( gpGlobals->time - m_fearTime ) > 15; }
	virtual BOOL CanHeal( void );
	void Heal( void );
	void Scream( void );
	void FearScream( void );
	bool ReviveMe( CBaseEntity *pTarget );
	CBaseEntity *BestVisibleDeadAlly( void );

	Schedule_t *GetScheduleOfType( int Type );
	Schedule_t *GetSchedule( void );
	MONSTERSTATE GetIdealState( void );
	void DeathSound( void );
	void PainSound( void );
	void TalkInit( void );
	void Killed( entvars_t *pevAttacker, int iGib );

	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	CUSTOM_SCHEDULES;

private:
	float	m_painTime;
	float	m_healTime;
	float	m_fearTime;
	float	m_flNextFearScream;
};

#endif
