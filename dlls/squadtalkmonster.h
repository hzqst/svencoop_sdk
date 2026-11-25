#ifndef SQUADTALKMONSTER_H
#define SQUADTALKMONSTER_H

#include "talkmonster.h"
#include "squadmonster.h"

#define SQUAD_TLK_CFRIENDS 8

enum
{
	TASK_OP4_MEDIC_SAY_HEAL = LAST_TALKMONSTER_TASK,
	TASK_OP4_MEDIC_RESET_HEAL,
};

class CSquadTalkMonster : public CSquadMonster
{
public:
	virtual void TalkInit( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	int IRelationship( CBaseEntity *pTarget, bool ignoreNoTarget = false );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Touch( CBaseEntity *pOther );
	void Killed( entvars_t *pevAttacker, int iGib );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void PrescheduleThink( void );
	void SetActivity( Activity newActivity );
	Schedule_t *GetScheduleOfType( int Type );
	void StartTask( Task_t *pTask );
	void RunTask( Task_t *pTask );
	int CanPlaySentence( BOOL fDisregardState );
	void PlaySentence( const char *pszSentence, float duration, float volume, float attenuation );
	void PlayScriptedSentence( const char *pszSentence, float duration, float volume,
	                           float attenuation, BOOL bConcurrent, CBaseEntity *pListener );

	bool HealMe( CBaseEntity *pTarget );
	bool ReviveMe( CBaseEntity *pTarget );

	CBaseEntity *FindNearestFriend( BOOL fPlayer );
	CBaseEntity *EnumFriends( CBaseEntity *pPrevious, int listNumber, BOOL bTrace );
	float TargetDistance( void );
	void IdleRespond( void );
	int FIdleSpeak( void );
	int FIdleStare( void );
	int FIdleHello( void );
	void IdleHeadTurn( Vector &vecFriend );
	void TrySmellTalk( void );
	int GetVoicePitch( void );
	BOOL IsTalking( void );
	int FOkToSpeak( void );
	void Talk( float flDuration );
	void ShutUpFriends( void );
	void AlertFriends( void );
	void UseSound( void );
	void UnUseSound( void );
	void StopTalking( void ) { SentenceStop(); }
	void StartPlayerFollowing( CBaseEntity *pLeader, bool fSilent = false );
	void LimitFollowers( CBaseEntity *pPlayer, int maxFollowers );
	void EXPORT FollowerPlayerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual CSquadTalkMonster *MySquadTalkMonsterPointer( void ) { return this; }
	virtual void SetAnswerQuestion( CBaseMonster *pSpeaker );
	virtual int FriendNumber( int arrayNumber ) { return arrayNumber; }

	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	CUSTOM_SCHEDULES;

	static const char *m_szFriends[SQUAD_TLK_CFRIENDS];
	static float      g_talkWaitTime;
	static float      g_talkWaitTimeEvil;

	int         m_bitsSaid;
	int         m_nSpeak;
	int         m_voicePitch;
	const char *m_szGrp[TLK_CGROUPS];

	// Squad-talk monsters have their own use timer in addition to the base one
	float m_useTime;
	int   m_iszUse;
	int   m_iszUnUse;

	float   m_flLastSaidSmelled;
	float   m_flStopTalkTime;
	EHANDLE m_hTalkTarget;

	float m_flHealGiveUpTime;	// a medic drops its patient after this time
};

#endif
