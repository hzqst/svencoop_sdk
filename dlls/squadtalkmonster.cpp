#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "squadtalkmonster.h"
#include "scripted.h"
#include "defaultai.h"
#include "scriptevent.h"
#include "soundent.h"

float CSquadTalkMonster::g_talkWaitTime = 0;
float CSquadTalkMonster::g_talkWaitTimeEvil = 0;

const char *CSquadTalkMonster::m_szFriends[SQUAD_TLK_CFRIENDS] = {
	"monster_barney",
	"monster_scientist",
	"monster_sitting_scientist",
	"monster_otis",
	"monster_human_grunt_ally",
	"monster_human_medic_ally",
	"monster_human_torch_ally",
	"monster_cleansuit_scientist"
};

TYPEDESCRIPTION CSquadTalkMonster::m_SaveData[] = {
	DEFINE_FIELD( CSquadTalkMonster, m_bitsSaid, FIELD_INTEGER ),
	DEFINE_FIELD( CSquadTalkMonster, m_nSpeak, FIELD_INTEGER ),
	DEFINE_FIELD( CSquadTalkMonster, m_useTime, FIELD_TIME ),
	DEFINE_FIELD( CSquadTalkMonster, m_iszUse, FIELD_STRING ),
	DEFINE_FIELD( CSquadTalkMonster, m_iszUnUse, FIELD_STRING ),
	DEFINE_FIELD( CSquadTalkMonster, m_flLastSaidSmelled, FIELD_TIME ),
	DEFINE_FIELD( CSquadTalkMonster, m_flStopTalkTime, FIELD_TIME ),
	DEFINE_FIELD( CSquadTalkMonster, m_hTalkTarget, FIELD_EHANDLE )
};

IMPLEMENT_SAVERESTORE( CSquadTalkMonster, CSquadMonster );

static Task_t tlSquadTalkIdleResponse[] = {
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT, (float) 0.5 },
	{ TASK_TLK_EYECONTACT, (float) 0 },
	{ TASK_TLK_RESPOND, (float) 0 },
	{ TASK_TLK_IDEALYAW, (float) 0 },
	{ TASK_FACE_IDEAL, (float) 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_TLK_EYECONTACT, (float) 0 },
};

static Schedule_t slSquadTalkIdleResponse[] = {
	{ tlSquadTalkIdleResponse,
	  ARRAYSIZE( tlSquadTalkIdleResponse ),
	  bits_COND_NEW_ENEMY |
	      bits_COND_LIGHT_DAMAGE |
	      bits_COND_HEAVY_DAMAGE |
	      bits_COND_HEAR_SOUND,
	  bits_SOUND_DANGER,
	  "Idle Response"

	},
};

static Task_t tlSquadTalkIdleSpeak[] = {
	{ TASK_TLK_SPEAK, (float) 0 },
	{ TASK_TLK_IDEALYAW, (float) 0 },
	{ TASK_FACE_IDEAL, (float) 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_TLK_EYECONTACT, (float) 0 },
	{ TASK_WAIT_RANDOM, (float) 0.5 },
};

static Schedule_t slSquadTalkIdleSpeak[] = {
	{ tlSquadTalkIdleSpeak,
	  ARRAYSIZE( tlSquadTalkIdleSpeak ),
	  bits_COND_NEW_ENEMY |
	      bits_COND_CLIENT_PUSH |
	      bits_COND_LIGHT_DAMAGE |
	      bits_COND_HEAVY_DAMAGE |
	      bits_COND_HEAR_SOUND,
	  bits_SOUND_DANGER,
	  "Idle Speak" },
};

static Task_t tlSquadTalkIdleSpeakWait[] = {
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_TLK_SPEAK, (float) 0 },
	{ TASK_TLK_EYECONTACT, (float) 0 },
	{ TASK_WAIT, (float) 2 },
};

static Schedule_t slSquadTalkIdleSpeakWait[] = {
	{ tlSquadTalkIdleSpeakWait,
	  ARRAYSIZE( tlSquadTalkIdleSpeakWait ),
	  bits_COND_NEW_ENEMY |
	      bits_COND_CLIENT_PUSH |
	      bits_COND_LIGHT_DAMAGE |
	      bits_COND_HEAVY_DAMAGE |
	      bits_COND_HEAR_SOUND,
	  bits_SOUND_DANGER,
	  "Idle Speak Wait" },
};

static Task_t tlSquadTalkIdleHello[] = {
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_TLK_HELLO, (float) 0 },
	{ TASK_TLK_EYECONTACT, (float) 0 },
	{ TASK_WAIT, (float) 0.5 },
	{ TASK_TLK_HELLO, (float) 0 },
	{ TASK_TLK_EYECONTACT, (float) 0 },
	{ TASK_WAIT, (float) 0.5 },
	{ TASK_TLK_HELLO, (float) 0 },
	{ TASK_TLK_EYECONTACT, (float) 0 },
	{ TASK_WAIT, (float) 0.5 },
	{ TASK_TLK_HELLO, (float) 0 },
	{ TASK_TLK_EYECONTACT, (float) 0 },
	{ TASK_WAIT, (float) 0.5 },

};

static Schedule_t slSquadTalkIdleHello[] = {
	{ tlSquadTalkIdleHello,
	  ARRAYSIZE( tlSquadTalkIdleHello ),
	  bits_COND_NEW_ENEMY |
	      bits_COND_CLIENT_PUSH |
	      bits_COND_LIGHT_DAMAGE |
	      bits_COND_HEAVY_DAMAGE |
	      bits_COND_HEAR_SOUND,

	  bits_SOUND_COMBAT |
	      bits_SOUND_DANGER,
	  "Idle Hello" },
};

extern Schedule_t slIdleStopShooting[];
extern Schedule_t slMoveAway[];
extern Schedule_t slMoveAwayFollow[];
extern Schedule_t slMoveAwayFail[];
extern Schedule_t slTlkIdleWatchClient[];
extern Schedule_t slTlkIdleEyecontact[];

DEFINE_CUSTOM_SCHEDULES( CSquadTalkMonster ){
	slSquadTalkIdleResponse,
	slSquadTalkIdleSpeak,
	slSquadTalkIdleHello,
	slSquadTalkIdleSpeakWait,
	slIdleStopShooting,
	slMoveAway,
	slMoveAwayFollow,
	slMoveAwayFail,
	slTlkIdleWatchClient,
	&slTlkIdleWatchClient[1],
	slTlkIdleEyecontact,
};

IMPLEMENT_CUSTOM_SCHEDULES( CSquadTalkMonster, CSquadMonster );

void CSquadTalkMonster::TalkInit( void )
{
}

void CSquadTalkMonster::Precache( void )
{
	if ( m_iszUse )
		m_szGrp[TLK_USE] = STRING( m_iszUse );

	if ( m_iszUnUse )
		m_szGrp[TLK_UNUSE] = STRING( m_iszUnUse );

	CBaseEntity::Precache();
}

void CSquadTalkMonster::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "UseSentence" ) )
	{
		m_iszUse = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "UnUseSentence" ) )
	{
		m_iszUnUse = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CSquadMonster::KeyValue( pkvd );
}

int CSquadTalkMonster::IRelationship( CBaseEntity *pTarget, bool ignoreNoTarget )
{
	if ( pTarget && pTarget->IsPlayer() && ( m_afMemory & bits_MEMORY_PROVOKED ) )
		return R_HT;

	return CBaseMonster::IRelationship( pTarget, ignoreNoTarget );
}

int CSquadTalkMonster::GetVoicePitch( void )
{
	return m_voicePitch;
}

BOOL CSquadTalkMonster::IsTalking( void )
{
	return m_flStopTalkTime > gpGlobals->time;
}

void CSquadTalkMonster::Talk( float flDuration )
{
	if ( flDuration <= 0 )
		m_flStopTalkTime = gpGlobals->time + 3;
	else
		m_flStopTalkTime = gpGlobals->time + flDuration;
}

int CSquadTalkMonster::FOkToSpeak( void )
{
	if ( m_MonsterState == MONSTERSTATE_PRONE || m_IdealMonsterState == MONSTERSTATE_PRONE )
		return FALSE;

	if ( pev->deadflag != DEAD_NO )
		return FALSE;

	float waitTime = IsPlayerAlly() ? g_talkWaitTime : g_talkWaitTimeEvil;

	if ( waitTime > gpGlobals->time || ( pev->spawnflags & SF_MONSTER_GAG ) )
		return FALSE;

	if ( !IsAlive() )
		return FALSE;

	edict_t *pClient = FIND_CLIENT_IN_PVS( edict() );

	if ( !pClient || pClient->free || FNullEnt( pClient ) )
		return FALSE;

	if ( FClassnameIs( pev, "monster_human_medic_ally" ) && m_hHealTarget != NULL )
		return FALSE;

	return TRUE;
}

int CSquadTalkMonster::CanPlaySentence( BOOL fDisregardState )
{
	if ( fDisregardState )
		return IsAlive();

	return FOkToSpeak();
}

void CSquadTalkMonster::PlaySentence( const char *pszSentence, float duration, float volume, float attenuation )
{
	if ( !pszSentence )
		return;

	Talk( duration );

	if ( IsPlayerAlly() )
		g_talkWaitTime = gpGlobals->time + duration + 2.0;
	else
		g_talkWaitTimeEvil = gpGlobals->time + duration + 2.0;

	SetBits( m_bitsSaid, bit_saidHelloPlayer );

	if ( pszSentence[0] == '!' || pszSentence[0] == '+' )
	{
		if ( pszSentence[0] == '+' )
			pszSentence++;

		EMIT_SOUND_DYN( edict(), CHAN_VOICE, pszSentence, volume, attenuation, 0, m_voicePitch );
	}
	else
		SENTENCEG_PlayRndSz( edict(), pszSentence, volume, attenuation, 0, m_voicePitch );
}

CBaseEntity *CSquadTalkMonster::EnumFriends( CBaseEntity *pPrevious, int listNumber, BOOL bTrace )
{
	const char *className = m_szFriends[FriendNumber( listNumber )];
	CBaseEntity *pFriend = pPrevious;

	while ( ( pFriend = UTIL_FindEntityByClassname( pFriend, className ) ) != NULL )
	{
		if ( pFriend == this || !pFriend->IsAlive() || pFriend->IsPlayerAlly() != IsPlayerAlly() )
			continue;

		if ( bTrace )
		{
			Vector destination = pFriend->pev->origin;
			TraceResult trace;

			destination.z = pFriend->pev->absmax.z;
			UTIL_TraceLine( pev->origin, destination, ignore_monsters, edict(), &trace );

			if ( trace.flFraction != 1.0f )
				continue;
		}

		return pFriend;
	}

	return NULL;
}

CBaseEntity *CSquadTalkMonster::FindNearestFriend( BOOL fPlayer )
{
	CBaseEntity *pNearest = NULL;
	Vector start = pev->origin;
	float range = 10000000;

	start.z = pev->absmax.z;

	if ( fPlayer )
	{
		for ( int player = 1; player <= gpGlobals->maxClients; player++ )
		{
			CBaseEntity *pFriend = UTIL_PlayerByIndex( player );

			if ( !pFriend )
				continue;

			Vector destination = pFriend->pev->origin;

			destination.z = pFriend->pev->absmax.z;

			float distance = ( start - destination ).Length();

			if ( !( range > distance ) )
				continue;

			TraceResult trace;

			UTIL_TraceLine( start, destination, ignore_monsters, edict(), &trace );

			if ( trace.flFraction == 1.0f && distance < TALKRANGE_MIN )
			{
				pNearest = pFriend;
				range = distance;
			}
		}

		return pNearest;
	}

	for ( int list = SQUAD_TLK_CFRIENDS - 1; list >= 0; list-- )
	{
		const char *className = m_szFriends[FriendNumber( list )];
		CBaseEntity *pFriend = NULL;

		if ( !className )
			continue;

		while ( ( pFriend = UTIL_FindEntityByClassname( pFriend, className ) ) != NULL )
		{
			if ( pFriend == this || !pFriend->IsAlive() || pFriend->IsPlayerAlly() != IsPlayerAlly() )
				continue;

			CBaseMonster *pMonster = pFriend->MyMonsterPointer();

			if ( !pMonster || pMonster->m_MonsterState == MONSTERSTATE_SCRIPT || pMonster->m_MonsterState == MONSTERSTATE_PRONE )
				continue;

			Vector destination = pFriend->pev->origin;

			destination.z = pFriend->pev->absmax.z;

			float distance = ( start - destination ).Length();

			if ( !( range > distance ) )
				continue;

			TraceResult trace;

			UTIL_TraceLine( start, destination, ignore_monsters, edict(), &trace );

			if ( trace.flFraction == 1.0f && distance < TALKRANGE_MIN )
			{
				pNearest = pFriend;
				range = distance;
			}
		}
	}

	return pNearest;
}

void CSquadTalkMonster::ShutUpFriends( void )
{
	for ( int list = 0; list < SQUAD_TLK_CFRIENDS; list++ )
	{
		CBaseEntity *pFriend = NULL;

		while ( ( pFriend = EnumFriends( pFriend, list, TRUE ) ) != NULL )
		{
			CBaseMonster *pMonster = pFriend->MyMonsterPointer();

			if ( pMonster )
				pMonster->SentenceStop();
		}
	}
}

void CSquadTalkMonster::AlertFriends( void )
{
	for ( int list = 0; list < SQUAD_TLK_CFRIENDS; list++ )
	{
		CBaseEntity *pFriend = NULL;

		while ( ( pFriend = EnumFriends( pFriend, list, TRUE ) ) != NULL )
		{
			CBaseMonster *pMonster = pFriend->MyMonsterPointer();

			if ( pMonster->IsAlive() && pMonster->IsPlayerAlly() == IsPlayerAlly() && Classify() == CLASS_PLAYER_ALLY )
				pMonster->Remember( bits_MEMORY_PROVOKED );
		}
	}
}

void CSquadTalkMonster::Killed( entvars_t *pevAttacker, int iGib )
{
	if ( pevAttacker && FBitSet( pevAttacker->flags, FL_CLIENT ) && m_MonsterState != MONSTERSTATE_PRONE )
	{
		if ( !FBitSet( m_afMemory, bits_MEMORY_KILLED ) )
			AlertFriends();

		if ( IsPlayerAlly() )
			LimitFollowers( CBaseEntity::Instance( pevAttacker ), 0 );
	}

	m_hTargetEnt = NULL;
	StopTalking();
	CSquadMonster::Killed( pevAttacker, iGib );
}

void CSquadTalkMonster::PlayScriptedSentence( const char *pszSentence, float duration,
                                              float volume, float attenuation, BOOL bConcurrent,
                                              CBaseEntity *pListener )
{
	if ( !bConcurrent )
		ShutUpFriends();

	ClearConditions( bits_COND_CLIENT_PUSH );
	m_useTime = gpGlobals->time + duration;
	PlaySentence( pszSentence, duration, volume, attenuation );
	m_hTalkTarget = pListener;
}

int CSquadTalkMonster::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker,
                                   float flDamage, int bitsDamageType )
{
	if ( IsAlive() && IsPlayerAlly() && pevAttacker && m_MonsterState != MONSTERSTATE_PRONE && FBitSet( pevAttacker->flags, FL_CLIENT ) )
	{
		CBaseEntity *pFriend = FindNearestFriend( FALSE );

		if ( pFriend && pFriend->IsAlive() && pFriend->IsPlayerAlly() == IsPlayerAlly() )
		{
			extern Schedule_t slIdleStopShooting[];

			pFriend->MyMonsterPointer()->ChangeSchedule( slIdleStopShooting );
		}
	}

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CSquadTalkMonster::Touch( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() || ( m_afMemory & bits_MEMORY_PROVOKED ) || IsTalking() )
		return;

	float speed = fabs( pOther->pev->velocity.x ) + fabs( pOther->pev->velocity.y );

	if ( speed > 50 )
	{
		SetConditions( bits_COND_CLIENT_PUSH );
		MakeIdealYaw( pOther->pev->origin );
	}
}

void CSquadTalkMonster::UseSound( void )
{
	PlaySentence( m_szGrp[TLK_USE], RANDOM_FLOAT( 2.8, 3.2 ), VOL_NORM, ATTN_IDLE );
	m_hTalkTarget = m_hTargetEnt;
}

void CSquadTalkMonster::UnUseSound( void )
{
	if ( m_afMemory & bits_MEMORY_PROVOKED )
		return;

	PlaySentence( m_szGrp[TLK_UNUSE], RANDOM_FLOAT( 2.8, 3.2 ), VOL_NORM, ATTN_IDLE );
	m_hTalkTarget = m_hTargetEnt;
}

float CSquadTalkMonster::TargetDistance( void )
{
	if ( m_hTargetEnt == NULL || !m_hTargetEnt->IsAlive() )
		return 1e6;

	return ( m_hTargetEnt->pev->origin - pev->origin ).Length();
}

void CSquadTalkMonster::PrescheduleThink( void )
{
	if ( !HasConditions( bits_COND_SEE_CLIENT ) )
		SetConditions( bits_COND_CLIENT_UNSEEN );
}

void CSquadTalkMonster::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch ( pEvent->event )
	{
		case SCRIPT_EVENT_SENTENCE_RND1:
			if ( RANDOM_LONG( 0, 99 ) < 75 )
				break;

		case SCRIPT_EVENT_SENTENCE:
			ShutUpFriends();
			PlaySentence( pEvent->options, RANDOM_FLOAT( 2.8, 3.4 ), VOL_NORM, ATTN_IDLE );
			break;

		default:
			CBaseMonster::HandleAnimEvent( pEvent );
			break;
	}
}

void CSquadTalkMonster::IdleRespond( void )
{
	PlaySentence( m_szGrp[TLK_ANSWER], RANDOM_FLOAT( 2.8, 3.2 ), VOL_NORM, ATTN_IDLE );
}

int CSquadTalkMonster::FIdleStare( void )
{
	if ( !FOkToSpeak() )
		return FALSE;

	PlaySentence( m_szGrp[TLK_STARE], RANDOM_FLOAT( 5, 7.5 ), VOL_NORM, ATTN_IDLE );
	m_hTalkTarget = FindNearestFriend( TRUE );
	return TRUE;
}

int CSquadTalkMonster::FIdleHello( void )
{
	if ( !FOkToSpeak() || FBitSet( m_bitsSaid, bit_saidHelloPlayer ) )
		return FALSE;

	CBaseEntity *pPlayer = FindNearestFriend( TRUE );

	if ( !pPlayer || !FInViewCone( pPlayer ) || !FVisible( pPlayer ) )
		return FALSE;

	m_hTalkTarget = pPlayer;
	PlaySentence( m_szGrp[TLK_HELLO], RANDOM_FLOAT( 3, 3.5 ), VOL_NORM, ATTN_IDLE );
	SetBits( m_bitsSaid, bit_saidHelloPlayer );
	return TRUE;
}

void CSquadTalkMonster::IdleHeadTurn( Vector &vecFriend )
{
	if ( !( m_afCapability & bits_CAP_TURN_HEAD ) )
		return;

	float yaw = VecToYaw( vecFriend - pev->origin ) - pev->angles.y;

	if ( yaw > 180 )
		yaw -= 360;

	if ( yaw < -180 )
		yaw += 360;

	SetBoneController( 0, yaw );
}

void CSquadTalkMonster::TrySmellTalk( void )
{
	if ( !FOkToSpeak() )
		return;

	if ( gpGlobals->time > m_flLastSaidSmelled )
		ClearBits( m_bitsSaid, bit_saidSmelled );

	if ( !FBitSet( m_bitsSaid, bit_saidSmelled ) && HasConditions( bits_COND_SMELL ) )
	{
		PlaySentence( m_szGrp[TLK_SMELL], RANDOM_FLOAT( 2.8, 3.2 ), VOL_NORM, ATTN_IDLE );
		m_flLastSaidSmelled = gpGlobals->time + 60;
		SetBits( m_bitsSaid, bit_saidSmelled );
	}
}

int CSquadTalkMonster::FIdleSpeak( void )
{
	if ( !FOkToSpeak() )
		return FALSE;

	const char *szIdleGroup;
	const char *szQuestionGroup;
	float duration;

	if ( FBitSet( pev->spawnflags, SF_MONSTER_PREDISASTER ) )
	{
		szIdleGroup = m_szGrp[TLK_PIDLE];
		szQuestionGroup = m_szGrp[TLK_PQUESTION];
		duration = RANDOM_FLOAT( 4.8, 5.2 );
	}
	else
	{
		szIdleGroup = m_szGrp[TLK_IDLE];
		szQuestionGroup = m_szGrp[TLK_QUESTION];
		duration = RANDOM_FLOAT( 2.8, 3.2 );
	}

	CBaseEntity *pTarget = m_hTargetEnt;

	if ( pTarget && pTarget->IsPlayer() && pTarget->IsAlive() )
	{
		m_hTalkTarget = pTarget;

		if ( !FBitSet( m_bitsSaid, bit_saidDamageHeavy ) && pTarget->pev->health <= pTarget->pev->max_health / 8 )
		{
			PlaySentence( m_szGrp[TLK_PLHURT3], duration, VOL_NORM, ATTN_IDLE );
			SetBits( m_bitsSaid, bit_saidDamageHeavy );
			return TRUE;
		}

		if ( !FBitSet( m_bitsSaid, bit_saidDamageMedium ) && pTarget->pev->health <= pTarget->pev->max_health / 4 )
		{
			PlaySentence( m_szGrp[TLK_PLHURT2], duration, VOL_NORM, ATTN_IDLE );
			SetBits( m_bitsSaid, bit_saidDamageMedium );
			return TRUE;
		}

		if ( !FBitSet( m_bitsSaid, bit_saidDamageLight ) && pTarget->pev->health <= pTarget->pev->max_health / 2 )
		{
			PlaySentence( m_szGrp[TLK_PLHURT1], duration, VOL_NORM, ATTN_IDLE );
			SetBits( m_bitsSaid, bit_saidDamageLight );
			return TRUE;
		}
	}

	CBaseEntity *pFriend = FindNearestFriend( FALSE );

	if ( pFriend && !pFriend->IsMoving() && RANDOM_LONG( 0, 99 ) < 75 )
	{
		PlaySentence( szQuestionGroup, duration, VOL_NORM, ATTN_IDLE );

		if ( FClassnameIs( pFriend->pev, "monster_barney" ) ||
		     FClassnameIs( pFriend->pev, "monster_scientist" ) ||
		     FClassnameIs( pFriend->pev, "monster_sitting_scientist" ) ||
		     FClassnameIs( pFriend->pev, "monster_otis" ) ||
		     FClassnameIs( pFriend->pev, "monster_cleansuit_scientist" ) )
		{
			CTalkMonster *pTalkMonster = (CTalkMonster *) pFriend;

			pTalkMonster->SetAnswerQuestion( this );
			pTalkMonster->m_flStopTalkTime = m_flStopTalkTime;
		}
		else
		{
			CSquadTalkMonster *pTalkMonster = (CSquadTalkMonster *) pFriend;

			pTalkMonster->SetAnswerQuestion( this );
			pTalkMonster->m_flStopTalkTime = m_flStopTalkTime;
		}

		m_hTalkTarget = pFriend;
		m_nSpeak++;
		return TRUE;
	}

	if ( RANDOM_LONG( 0, 1 ) )
	{
		pFriend = FindNearestFriend( TRUE );

		if ( pFriend )
		{
			m_hTalkTarget = pFriend;
			PlaySentence( szIdleGroup, duration, VOL_NORM, ATTN_IDLE );
			m_nSpeak++;
			return TRUE;
		}
	}

	Talk( 0 );
	g_talkWaitTime = 0;
	g_talkWaitTimeEvil = 0;
	return FALSE;
}

void CSquadTalkMonster::SetAnswerQuestion( CBaseMonster *pSpeaker )
{
	if ( m_MonsterState == MONSTERSTATE_PRONE || m_IdealMonsterState == MONSTERSTATE_PRONE ||
	     pev->deadflag != DEAD_NO || FBitSet( pev->spawnflags, SF_MONSTER_GAG ) )
		return;

	if ( m_hEnemy != NULL && FVisible( m_hEnemy ) )
		return;

	if ( FClassnameIs( pev, "monster_human_medic_ally" ) && m_hHealTarget != NULL )
		return;

	if ( !m_pCine )
	{

		ChangeSchedule( slSquadTalkIdleResponse );
	}

	m_hTalkTarget = pSpeaker;
}

void CSquadTalkMonster::StartPlayerFollowing( CBaseEntity *pLeader, bool fSilent )
{
	if ( m_hEnemy != NULL )
		m_IdealMonsterState = MONSTERSTATE_ALERT;

	CBaseMonster::StartPlayerFollowing( pLeader, fSilent );
}

void CSquadTalkMonster::LimitFollowers( CBaseEntity *pPlayer, int maxFollowers )
{
	int count = 0;

	for ( int list = 0; list < SQUAD_TLK_CFRIENDS; list++ )
	{
		CBaseEntity *pFriend = NULL;

		while ( ( pFriend = EnumFriends( pFriend, list, FALSE ) ) != NULL )
		{
			CBaseMonster *pMonster = pFriend->MyMonsterPointer();

			if ( pMonster && pMonster->m_hTargetEnt == pPlayer )
			{
				count++;

				if ( count > maxFollowers )
					pMonster->StopPlayerFollowing( true, false );
			}
		}
	}
}

void CSquadTalkMonster::FollowerPlayerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !IsAlive() || m_useTime > gpGlobals->time || !pCaller || !pCaller->IsPlayer() )
		return;

	if ( m_hGuardEnt != NULL || IRelationship( pCaller, true ) > R_NO )
		return;

	if ( FBitSet( pev->spawnflags, SF_MONSTER_PREDISASTER ) )
	{
		DeclineFollowing();
		return;
	}

	if ( CanPlayerFollow() ||
	     ( m_MonsterState != MONSTERSTATE_SCRIPT && m_hTargetEnt != NULL && pActivator != m_hTargetEnt ) )
	{
		LimitFollowers( pCaller, TLK_MAX_FOLLOWERS );

		if ( FBitSet( m_afMemory, bits_MEMORY_PROVOKED ) )
			ALERT( at_aiconsole, "I'm not following you, you evil person!\n" );
		else
		{
			StartPlayerFollowing( pCaller, false );
			SetBits( m_bitsSaid, bit_saidHelloPlayer );
		}
	}
	else
		StopPlayerFollowing( true, false );
}

Schedule_t *CSquadTalkMonster::GetScheduleOfType( int Type )
{
	switch ( Type )
	{
		case SCHED_MOVE_AWAY:
			return slMoveAway;

		case SCHED_MOVE_AWAY_FOLLOW:
			return slMoveAwayFollow;

		case SCHED_MOVE_AWAY_FAIL:
			return slMoveAwayFail;

		case SCHED_TARGET_FACE:
			return RANDOM_LONG( 0, 99 ) < 2 ? slSquadTalkIdleSpeakWait : slIdleStand;

		case SCHED_IDLE_STAND:
			if ( !FBitSet( m_bitsSaid, bit_saidHelloPlayer ) )
			{
				if ( FCanActiveIdle() )
					return CBaseMonster::GetScheduleOfType( SCHED_IDLE_STAND );

				if ( Classify() == CLASS_PLAYER_ALLY )
					return slSquadTalkIdleHello;
			}

			if ( !FBitSet( m_bitsSaid, bit_saidWoundLight ) && pev->health <= pev->max_health * 0.75 )
			{
				PlaySentence( m_szGrp[TLK_WOUND], RANDOM_FLOAT( 2.8, 3.2 ), VOL_NORM, ATTN_IDLE );
				SetBits( m_bitsSaid, bit_saidWoundLight );
				return slIdleStand;
			}

			if ( !FBitSet( m_bitsSaid, bit_saidWoundHeavy ) && pev->health <= pev->max_health * 0.5 )
			{
				PlaySentence( m_szGrp[TLK_MORTAL], RANDOM_FLOAT( 2.8, 3.2 ), VOL_NORM, ATTN_IDLE );
				SetBits( m_bitsSaid, bit_saidWoundHeavy );
				return slIdleStand;
			}

			if ( FOkToSpeak() && RANDOM_LONG( 0, m_nSpeak * 2 ) == 0 )
				return slSquadTalkIdleSpeak;

			if ( FCanActiveIdle() )
				return CBaseMonster::GetScheduleOfType( SCHED_IDLE_STAND );

			if ( !IsTalking() && HasConditions( bits_COND_SEE_CLIENT ) && RANDOM_LONG( 0, 6 ) == 0 )
			{
				edict_t *pPlayer = g_engfuncs.pfnPEntityOfEntIndex( 1 );

				if ( pPlayer )
				{
					UTIL_MakeVectors( pPlayer->v.angles );

					if ( ( pPlayer->v.origin - pev->origin ).Length2D() < TLK_STARE_DIST &&
					     UTIL_DotPoints( pPlayer->v.origin, pev->origin, gpGlobals->v_forward ) >= m_flFieldOfView )
						return &slTlkIdleWatchClient[1];

					return slTlkIdleWatchClient;
				}
			}
			else
				return IsTalking() ? slTlkIdleEyecontact : slIdleStand;

			break;
	}

	return CBaseMonster::GetScheduleOfType( Type );
}

void CSquadTalkMonster::StartTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_TLK_SPEAK:
			FIdleSpeak();
			TaskComplete();
			break;

		case TASK_TLK_RESPOND:
			IdleRespond();
			TaskComplete();
			break;

		case TASK_TLK_HELLO:
			FIdleHello();
			TaskComplete();
			break;

		case TASK_TLK_STARE:
			FIdleStare();
			TaskComplete();
			break;

		case TASK_FACE_PLAYER:
		case TASK_TLK_LOOK_AT_CLIENT:
		case TASK_TLK_CLIENT_STARE:
			m_flWaitFinished = gpGlobals->time + pTask->flData;
			break;

		case TASK_TLK_EYECONTACT:
			break;

		case TASK_TLK_IDEALYAW:
			if ( m_hTalkTarget != NULL )
			{
				float yaw = VecToYaw( m_hTalkTarget->pev->origin - pev->origin ) - pev->angles.y;

				pev->yaw_speed = 60;

				if ( yaw > 180 )
					yaw -= 360;

				if ( yaw < -180 )
					yaw += 360;

				if ( yaw < 0 )
					pev->ideal_yaw = V_min( yaw + 45, 0 ) + pev->angles.y;
				else
					pev->ideal_yaw = V_max( yaw - 45, 0 ) + pev->angles.y;
			}

			TaskComplete();
			break;

		case TASK_TLK_HEADRESET:
			m_hTalkTarget = NULL;
			TaskComplete();
			break;

		case TASK_TLK_STOPSHOOTING:
			PlaySentence( m_szGrp[TLK_NOSHOOT], RANDOM_FLOAT( 2.8, 3.2 ), VOL_NORM, ATTN_NORM );
			TaskComplete();
			break;

		case TASK_CANT_FOLLOW:
			StopPlayerFollowing( false, false );
			PlaySentence( m_szGrp[TLK_STOP], RANDOM_FLOAT( 2, 2.5 ), VOL_NORM, ATTN_NORM );
			TaskComplete();
			break;

		case TASK_WALK_PATH_FOR_UNITS:
			m_movementActivity = ACT_WALK;
			break;

		case TASK_MOVE_AWAY_PATH:
		{
			Vector angles = pev->angles;
			Vector move;

			angles.y = pev->ideal_yaw + 180;
			UTIL_MakeVectorsPrivate( angles, move, NULL, NULL );

			Vector destination = pev->origin + move * pTask->flData;

			if ( MoveToLocation( ACT_WALK, 2, destination ) )
				TaskComplete();
			else if ( FindCover( pev->origin, pev->view_ofs, 0, CoverRadius() ) )
			{
				m_flMoveWaitFinished = gpGlobals->time + 2;
				TaskComplete();
			}
			else
				TaskFail();
		}
		break;

		case TASK_PLAY_SCRIPT:
			m_hTalkTarget = NULL;
			CBaseMonster::StartTask( pTask );
			break;

		default:
			CBaseMonster::StartTask( pTask );
			break;
	}
}

void CSquadTalkMonster::RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_TLK_CLIENT_STARE:
		case TASK_TLK_LOOK_AT_CLIENT:
		{
			edict_t *pPlayer = NULL;

			if ( m_MonsterState == MONSTERSTATE_IDLE && !IsMoving() && !IsTalking() )
			{
				pPlayer = g_engfuncs.pfnPEntityOfEntIndex( 1 );

				if ( pPlayer )
					IdleHeadTurn( pPlayer->v.origin );
			}
			else
			{
				TaskFail();
				return;
			}

			if ( pTask->iTask == TASK_TLK_CLIENT_STARE )
			{
				if ( ( pPlayer->v.origin - pev->origin ).Length2D() > TLK_STARE_DIST )
					TaskFail();

				UTIL_MakeVectors( pPlayer->v.angles );

				if ( UTIL_DotPoints( pPlayer->v.origin, pev->origin, gpGlobals->v_forward ) < m_flFieldOfView )
					TaskFail();
			}

			if ( gpGlobals->time > m_flWaitFinished )
				TaskComplete();
		}
		break;

		case TASK_FACE_PLAYER:
		{
			edict_t *pPlayer = g_engfuncs.pfnPEntityOfEntIndex( 1 );

			if ( pPlayer )
			{
				MakeIdealYaw( pPlayer->v.origin );
				ChangeYaw( pev->yaw_speed );
				IdleHeadTurn( pPlayer->v.origin );

				if ( gpGlobals->time > m_flWaitFinished && FlYawDiff() < 10 )
					TaskComplete();
			}
			else
				TaskFail();
		}
		break;

		case TASK_TLK_EYECONTACT:
			if ( !IsMoving() && IsTalking() && m_hTalkTarget != NULL )
				IdleHeadTurn( m_hTalkTarget->pev->origin );
			else
				TaskComplete();

			break;

		case TASK_WALK_PATH_FOR_UNITS:
			if ( ( m_vecLastPosition - pev->origin ).Length2D() > pTask->flData || MovementIsComplete() )
			{
				TaskComplete();
				RouteClear();
			}
			break;

		case TASK_WAIT_FOR_MOVEMENT:
			if ( IsTalking() && m_hTalkTarget != NULL )
				IdleHeadTurn( m_hTalkTarget->pev->origin );
			else
			{
				IdleHeadTurn( pev->origin );
				FIdleHello();

				if ( RANDOM_LONG( 0, m_nSpeak * 20 ) == 0 )
					FIdleSpeak();
			}

			CBaseMonster::RunTask( pTask );

			if ( TaskIsComplete() )
				IdleHeadTurn( pev->origin );

			break;

		default:
			if ( IsTalking() && m_hTalkTarget != NULL )
				IdleHeadTurn( m_hTalkTarget->pev->origin );
			else
				SetBoneController( 0, 0 );

			CBaseMonster::RunTask( pTask );
			break;
	}
}
