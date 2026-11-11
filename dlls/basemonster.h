/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/

#ifndef BASEMONSTER_H
#define BASEMONSTER_H

#ifndef CLIENT_DLL
#include "TypedEntityHandle.h"
#include "EntityHandle.h"
#endif

// Ospreys are targeted at whichever of these bones is closer to the shooter
#define OSPREY_FIRST_TARGET_BONE	4
#define OSPREY_SECOND_TARGET_BONE	8

struct CInventoryItemNode;
class CFuncTank;
class CPathCondition;
class CPathConditionController;
class CPathWaypoint;

// m_afMoveShootCap: attacks that can play over the movement gait
#define bits_MOVESHOOTCAP_MELEE_ATTACK1	( 1 << 0 )
#define bits_MOVESHOOTCAP_MELEE_ATTACK2	( 1 << 1 )
#define bits_MOVESHOOTCAP_RANGE_ATTACK1	( 1 << 2 )
#define bits_MOVESHOOTCAP_RANGE_ATTACK2	( 1 << 3 )

// pev->gaitsequence when no gait plays
#define GAIT_SEQUENCE_NONE			255

// A player moving at least this fast (x + y) pushes a player ally aside
#define PLAYER_ALLY_PUSH_SPEED		50

//
// generic Monster
//
class CBaseMonster : public CBaseToggle
{
private:
		int					m_afConditions;

public:
		typedef enum
		{
			SCRIPT_PLAYING = 0,		// Playing the sequence
			SCRIPT_WAIT,				// Waiting on everyone in the script to be ready
			SCRIPT_CLEANUP,					// Cancelling the script / cleaning up
			SCRIPT_WALK_TO_MARK,
			SCRIPT_RUN_TO_MARK,
		} SCRIPTSTATE;


	
		// these fields have been added in the process of reworking the state machine. (sjb)
		EHANDLE				m_hEnemy;		 // the entity that the monster is fighting.
		EHANDLE				m_hTargetEnt;	 // the entity that the monster is trying to reach
		EHANDLE				m_hHealTarget;	 // the entity a medic is healing or reviving

#ifndef CLIENT_DLL
		EHANDLE				m_hSelectedTank;
#endif

		EHANDLE				m_hOldEnemy[ MAX_OLD_ENEMIES ];
		Vector				m_vecOldEnemy[ MAX_OLD_ENEMIES ];

		float				m_flFieldOfView;// width of monster's field of view ( dot product )
		float				m_flWaitFinished;// if we're told to wait, this is the time that the wait will be over.
		float				m_flMoveWaitFinished;

		Activity			m_Activity;// what the monster is doing (animation)
		Activity			m_IdealActivity;// monster should switch to this activity
		Activity			m_GaitActivity;	// legs play this activity under the main one
		
		int					m_LastHitGroup; // the last body region that took damage
		
		MONSTERSTATE		m_MonsterState;// monster's current state
		MONSTERSTATE		m_IdealMonsterState;// monster should change to this state
	
		int					m_iTaskStatus;
		Schedule_t			*m_pSchedule;
#ifndef CLIENT_DLL
		Schedule_t			*m_pScheduleSaved;
#endif
		int					m_iScheduleIndex;

		WayPoint_t			m_Route[ ROUTE_SIZE ];	// Positions of movement
		int					m_movementGoal;			// Goal that defines route
		int					m_iRouteIndex;			// index into m_Route[]
		float				m_moveWaitTime;			// How long I should wait for something to move
		float				m_moveradius;			// a blocked move within this of m_vecMoveGoal counts as arrived

		Vector				m_vecMoveGoal; // kept around for node graph moves, so we know our ultimate goal
#ifndef CLIENT_DLL
		Vector				m_vecLadderEnd;
#endif
		Activity			m_movementActivity;	// When moving, set this activity
		bool				m_fTriangulateFlip;	// mirrors the triangulation bias after a failed search

		int					m_iAudibleList; // first index of a linked list of sounds that the monster can hear.
		int					m_afSoundTypes;

		Vector				m_vecLastPosition;// monster sometimes wants to return to where it started after an operation.

		int					m_iHintNode; // this is the hint node that the monster is moving towards or performing active idle on.

		int					m_afMemory;

		int					m_iMaxHealth;// keeps track of monster's maximum health value (for re-healing, etc)

	Vector				m_vecEnemyLKP;// last known position of enemy. (enemy's origin)

	int					m_cAmmoLoaded;		// how much ammo is in the weapon (used to trigger reload anim sequences)

	int					m_afCapability;// tells us what a monster can/can't do.
	int					m_afMoveShootCap;// tells us what a monster can/can't do, while moving

	float				m_flNextAttack;		// cannot attack again until this time

	int					m_bitsDamageType;	// what types of damage has monster (player) taken
	BYTE				m_rgbTimeBasedDamage[CDMG_TIMEBASED];

	int					m_lastDamageAmount;// how much damage did monster (player) last take
	float				m_tbdPrev;			// time-based damage was last checked at this time
	entvars_t			*m_pevPoisoner;		// Entity credited with poison damage
											// time based damage counters, decr. 1 per 2 seconds
	int					m_bloodColor;		// color of blood particless

	int					m_failSchedule;				// Schedule type to choose if current schedule fails

	float				m_flHungryTime;// set this is a future time to stop the monster from eating for a while. 

	float				m_flDistTooFar;	// if enemy farther away than this, bits_COND_ENEMY_TOOFAR set in CheckEnemy
	float				m_flDistLook;	// distance monster sees (Default 2048)

	int					m_iTriggerCondition;// for scripted AI, this is the condition that will cause the activation of the monster's TriggerTarget
	string_t			m_iszTriggerTarget;// name of target that should be fired. 

	Vector				m_HackedGunPos;	// HACK until we can query end of gun
	bool				m_fReviving;		// m_hHealTarget was dead when the medic took it

// Scripted sequence Info
	SCRIPTSTATE			m_scriptState;		// internal cinematic state
#ifndef CLIENT_DLL
	CEntityHandle<CCineMonster> m_pCine;
#else
	CCineMonster		*m_pCine;
#endif
	EHANDLE				m_hCineBlocker;		// a player in the way inside the sequence's moveto_radius
	float				m_flMoveToRadius;	// copied from the sequence that possesses us
	bool				m_fInAIScript;		// possessed by an aiscripted_sequence

	float				m_useTime;			// Don't allow +USE until this time
	bool				m_fCanPlayerFollow;	// players may +USE this monster to lead it

	float				m_flFallVelocity;	// downward speed while airborne
	float				m_flFallDamageTime;	// no fall damage before this time
	bool				m_fFallDamageReady;	// has stood on the ground since spawning

	Vector				m_vecAlignOrigin;	// where the corpse was last aligned to the ground,
	Vector				m_vecAlignAngles;	// ...its angles then,
	Vector				m_vecAlignNormal;	// ...and the ground's normal

#ifndef CLIENT_DLL
	string_t			m_FormattedName;
	bool				m_fFreeRoamMode2;
	bool				m_fFreeRoamMode1;

	bool				m_fIsNotRevivable;
	float				m_flReviveTime;
	EHANDLE				m_hHealer;				// the medic reviving this corpse
	float				m_flNextMedicCallTime;	// no medic is asked for help before this time

	bool				m_fShockGlow;
	float				m_flShockGlowTime;	// when the effect expires

	float				m_flHeavyDamageTime;	// when the accumulated damage is forgotten
	float				m_flHeavyDamageTotal;
	bool				m_fKilledByBlast;

	EntityHandle<CPathConditionController>	m_hPathConditionController;
	EntityHandle<CPathCondition>			m_hPathCondition;
	EntityHandle<CPathWaypoint>			m_hPathWaypoint;
	string_t			m_iszPathName;
	string_t			m_iszPreviousPathName;
	float				m_flNextPathEvaluation;

	float				m_flAutomaticAttackTime;

	// Initialization and AI timing state
	bool				m_fDeathKnockback;
	bool				m_fBodyDropEvent;

	CSound				*m_pDangerSound;		// what TASK_FIND_COVER_FROM_BEST_SOUND is fleeing
	float				m_flGrenadeMoveTime;	// when TASK_WAIT_FOR_MOVEMENT_GRENADE gives up
	float				m_flEnemySeenTime;		// until then, a lost enemy still competes with a new one
	float				m_flNextTankSearchTime;
	int					m_iBloodColorOverride;
	float				m_flThinkInterval;
	float				m_flNextLookTime;
	float				m_flFrameTime;
	float				m_flLastThinkTime;

	CInventoryItemNode *m_pInventory;
	Vector				m_vecEffectGlowColor;
	int					m_iEffectBlockWeapons;
	int					m_iEffectInvulnerable;
	int					m_iEffectInvisible;
	int					m_iEffectNonSolid;
	float				m_flEffectRespiration;
	float				m_flEffectGravity;
	float				m_flEffectFriction;
#endif

	float				m_flEffectSpeed;

#ifndef CLIENT_DLL
	float				m_flEffectDamage;

	// Saved gravity and friction multipliers
	float				m_flOriginalGravity;
	float				m_flOriginalFriction;
#endif

	bool				m_fCheckCreatureDanger;	// flees creatures that grab their victims
	bool				m_fIgnoreCreatureDanger;	// ...but never interrupts a schedule for it

	EHANDLE				m_hGuardEnt;		// followed, and players can't take the monster away from it
	string_t			m_iszGuardEntName;	// "guard_ent" keyvalue

	virtual int		Save( CSave &save ); 
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];

	void KeyValue( KeyValueData *pkvd );

// monster use function
	void EXPORT			MonsterUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT			CorpseUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

// overrideable Monster member functions
	
	virtual int	 BloodColor( void ) { return m_bloodColor; }

	virtual CBaseMonster *MyMonsterPointer( void ) { return this; }
	virtual bool IsMonster( void ) { return true; }
	virtual bool IsGibbed( void ) { return !IsAlive() && ( pev->effects & EF_NODRAW ); }
	virtual void Look ( int iDistance );// basic sight function for monsters
	virtual void RunAI ( void );// core ai function!	
	void Listen ( void );

	virtual BOOL	IsAlive( void ) { return (pev->deadflag != DEAD_DEAD); }
	virtual BOOL	ShouldFadeOnDeath( void );

// Basic Monster AI functions
	virtual float ChangeYaw ( int speed );
	virtual void SetupFriendly( void ) { }
	float VecToYaw( Vector vecDir );
	float FlYawDiff ( void ); 

	float DamageForce( float damage );
	float DamageForce( float damage, int bitsDamageType );

// stuff written for new state machine
		virtual void MonsterThink( void );
		void EXPORT	CallMonsterThink( void ) { this->MonsterThink(); }
		virtual void MonsterInit ( void );
		virtual void MonsterInitDead( void );	// Call after animation/pose is set up

		void ApplyEffects( void );
		virtual void EndRevive( float flTimeUntilRevive );
		void EXPORT ReviveThink( void );

#ifndef CLIENT_DLL
		void OnCreate( void );
		void OnDestroy( void );
		void ResetEffects( void );
		void Precache( void );
		bool AreBaseReviveConditionsMet( void );
		virtual bool IsRevivable( void );
		virtual void BeginRevive( float flTimeUntilRevive );
		virtual void Revive( void );
		virtual void CheckRevival( void );
		void ShockGlowEffect( bool fEnable );
		bool FindBestPath( void );
		void EvaluatePaths( void );
		bool StartPathWaypointTask( Task_t *pTask );
		bool RunPathWaypointTask( Task_t *pTask );
#endif

		virtual void BecomeDead( float flSavedHealth );
		void AlignToGround( void );
		void FloatInWater( void );
		void EXPORT CorpseFallThink( void );

		void EXPORT MonsterInitThink ( void );
		virtual void StartMonster ( void );
		virtual CBaseEntity* BestVisibleEnemy ( void );// finds best visible enemy for attack
		virtual BOOL FInViewCone ( CBaseEntity *pEntity );// see if pEntity is in monster's view cone
		virtual BOOL FInViewCone ( Vector *pOrigin );// see if given location is in monster's view cone
		virtual void HandleAnimEvent( MonsterEvent_t *pEvent );

		virtual int CheckLocalMove ( const Vector &vecStart, const Vector &vecEnd, CBaseEntity *pTarget, float *pflDist, Vector *pvecEndPosition = NULL, bool fWalkMoveCheck = false );// check validity of a straight move through space
		virtual void Move( float flInterval = 0.1 );
		virtual void MoveExecute( CBaseEntity *pTargetEnt, const Vector &vecDir, float flInterval );
		virtual BOOL ShouldAdvanceRoute( float flWaypointDist );

		virtual Activity GetStoppedActivity( void ) { return ACT_IDLE; }
		// Clear the gait and switch to the stopped activity
		virtual void Stop( void )
		{
			pev->gaitsequence = GAIT_SEQUENCE_NONE;
			m_GaitActivity = ACT_RESET;
			m_flGroundSpeed = 0;
			ResetGaitSequenceInfo();

			if ( m_Activity == m_movementActivity || m_IdealActivity == m_movementActivity )
				m_IdealActivity = GetStoppedActivity();
		}

		virtual void SetGaitActivity( Activity NewActivity );

		// This will stop animation until you call ResetSequenceInfo() at some point in the future
		inline void StopAnimation( void ) { pev->framerate = 0; }

		// these functions will survey conditions and set appropriate conditions bits for attack types.
		virtual BOOL CheckRangeAttack1( float flDot, float flDist );
		virtual BOOL CheckRangeAttack1_Move( float flDot, float flDist );
		virtual BOOL CheckRangeAttack2( float flDot, float flDist );
		virtual BOOL CheckRangeAttack2_Move( float flDot, float flDist );
		virtual BOOL CheckMeleeAttack1( float flDot, float flDist );
		virtual BOOL CheckMeleeAttack1_Move( float flDot, float flDist );
		virtual BOOL CheckMeleeAttack2( float flDot, float flDist );
		virtual BOOL CheckMeleeAttack2_Move( float flDot, float flDist );

		BOOL FHaveSchedule( void );
		BOOL FScheduleValid ( void );
		void ClearSchedule( void );
		BOOL FScheduleDone ( void );
		void ChangeSchedule ( Schedule_t *pNewSchedule );
		void NextScheduledTask ( void );
		Schedule_t *ScheduleInList( const char *pName, Schedule_t **pList, int listCount );

		virtual Schedule_t *ScheduleFromName( const char *pName );
		static Schedule_t *m_scheduleList[];
		
		void MaintainSchedule ( void );
		virtual void StartTask ( Task_t *pTask );
		virtual void RunTask ( Task_t *pTask );
		virtual Schedule_t *GetScheduleOfType( int Type );
		virtual Schedule_t *GetSchedule( void );
		virtual void ScheduleChange( void ) {}
		// virtual int CanPlaySequence( void ) { return ((m_pCine == NULL) && (m_MonsterState == MONSTERSTATE_NONE || m_MonsterState == MONSTERSTATE_IDLE || m_IdealMonsterState == MONSTERSTATE_IDLE)); }
		virtual int CanPlaySequence( BOOL fDisregardState, int interruptLevel );
		virtual int CanPlaySentence( BOOL fDisregardState ) { return IsAlive(); }
		virtual void PlaySentence( const char *pszSentence, float duration, float volume, float attenuation );
		virtual void PlayScriptedSentence( const char *pszSentence, float duration, float volume, float attenuation, BOOL bConcurrent, CBaseEntity *pListener );

		virtual void SentenceStop( void );

		Task_t *GetTask ( void );
		virtual MONSTERSTATE GetIdealState ( void );
		void FaceSound( CSound *pSound, bool fMayFollowLook );
		virtual void SetActivity ( Activity NewActivity );
		void SetSequenceByName ( const char *szSequence );
		void SetState ( MONSTERSTATE State );
		virtual void ReportAIState( void );

		void CheckAttacks ( CBaseEntity *pTarget, float flDist );
		void CheckAttacks_Move ( CBaseEntity *pTarget, float flDist );
		void ClearEnemyList( void );
		bool IsFriendly( CBaseEntity *pOther );
		virtual int CheckEnemy ( CBaseEntity *pEnemy );
		void PushEnemy( CBaseEntity *pEnemy, Vector &vecLastKnownPos );
		BOOL PopEnemy( void );

		BOOL FGetNodeRoute ( Vector vecDest );
		
		inline void TaskComplete( void ) { if ( !HasConditions(bits_COND_TASK_FAILED) ) m_iTaskStatus = TASKSTATUS_COMPLETE; }
		void MovementComplete( void );
		inline void TaskFail( void ) { SetConditions(bits_COND_TASK_FAILED); }
		inline void TaskBegin( void ) { m_iTaskStatus = TASKSTATUS_RUNNING; }
		int TaskIsRunning( void );
		inline int TaskIsComplete( void ) { return (m_iTaskStatus == TASKSTATUS_COMPLETE); }
		inline int MovementIsComplete( void ) { return (m_movementGoal == MOVEGOAL_NONE); }

		int IScheduleFlags ( void );
		BOOL FRefreshRoute( float flMoveRadius = 0 );
		BOOL FRouteClear ( void );
		void RouteSimplify( CBaseEntity *pTargetEnt );
		void AdvanceRoute ( float distance );
#ifndef CLIENT_DLL
		void LadderMoveDone( void );
#endif
		virtual BOOL FTriangulate ( const Vector &vecStart , const Vector &vecEnd, float flDist, CBaseEntity *pTargetEnt, Vector *pApex, Vector *pApex2 = NULL, bool fNoExtension = false );
		virtual BOOL FTriangulateExtension ( const Vector &vecStart, const Vector &vecEnd, float flDist, CBaseEntity *pTargetEnt, Vector *pApex );
		void MakeIdealYaw( Vector vecTarget );
		virtual void SetYawSpeed ( void ) { return; };// allows different yaw_speeds for each activity
		BOOL BuildRoute ( const Vector &vecGoal, int iMoveFlag, CBaseEntity *pTarget, bool fNoExtension = false );
		virtual BOOL BuildNearestRoute ( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist );
		int RouteClassify( int iMoveFlag );
		void InsertWaypoint ( Vector vecLocation, int afMoveFlags );
		
		BOOL FindLateralCover ( const Vector &vecThreat, const Vector &vecViewOffset );
		BOOL FindLateralSafety ( const Vector &vecThreat, float flMinDist, float flDist );
		BOOL FindLateralSafetyGrenade ( const Vector &vecThreat );
		BOOL FindLateralAttackPoint ( const Vector &vecThreat, const Vector &vecViewOffset );
		BOOL FindLateralAttackPoint ( const Vector &vecThreat, const Vector &vecViewOffset, float flDist );
		BOOL CheckAttacksFromPosition ( CBaseEntity *pTarget, float flDist, Vector vecPosition );
		int FindRoamNode ( void );
		BOOL TestBuildRoute ( const Vector &vecGoal, int iMoveFlag, CBaseEntity *pTarget );
		BOOL TestFGetNodeRoute ( Vector vecDest );
		virtual BOOL FindCover ( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist );
		virtual BOOL FindCoverGrenade ( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist );
		virtual BOOL FindCoverDistance ( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist );
		virtual BOOL FindAttackPoint ( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist );
		virtual BOOL FValidateCover ( const Vector &vecCoverLocation ) { return TRUE; };
		virtual float CoverRadius( void ) { return 784; } // Default cover radius

		virtual BOOL FCanCheckAttacks ( void );
		virtual void CheckAmmo( void ) { return; };
		virtual int IgnoreConditions ( void );
		
		inline int GetConditions( void ) { return m_afConditions; }
		inline void	SetConditions( int iConditions ) { m_afConditions |= iConditions; }
		inline void	ClearConditions( int iConditions ) { m_afConditions &= ~iConditions; }
		inline BOOL HasConditions( int iConditions ) { if ( m_afConditions & iConditions ) return TRUE; return FALSE; }
		inline BOOL HasAllConditions( int iConditions ) { if ( (m_afConditions & iConditions) == iConditions ) return TRUE; return FALSE; }

		virtual BOOL FValidateHintType( short sHint );
		int FindHintNode ( void );
		virtual BOOL FCanActiveIdle ( void );
		void SetTurnActivity ( void );
		float FLSoundVolume ( CSound *pSound );

		BOOL MoveToNode( Activity movementAct, float waitTime, const Vector &goal, float flMoveRadius = 0 );
		BOOL MoveToTarget( Activity movementAct, float waitTime, float flMoveRadius = 0 );
		BOOL MoveToLocation( Activity movementAct, float waitTime, const Vector &goal, float flMoveRadius = 0 );
		BOOL MoveToCover( Activity movementAct, float waitTime, const Vector &goal, float flMoveRadius );
		BOOL MoveToEnemy( Activity movementAct, float waitTime, float flMoveRadius = 0 );

		// Returns the time when the door will be open
		float	OpenDoorAndWait( entvars_t *pevDoor );

		virtual int ISoundMask( void );
		virtual CSound* PBestSound ( void );
		virtual CSound* PBestScent ( void );
		virtual float HearingSensitivity( void ) { return 1.0; };

		bool FBecomeProne ( CBaseEntity *pOther );
		virtual void BarnacleVictimBitten( entvars_t *pevBarnacle, float flDamage );
		virtual void BarnacleVictimReleased( void );

		void SetEyePosition ( void );

		BOOL FShouldEat( void );// see if a monster is 'hungry'
		void Eat ( float flFullDuration );// make the monster 'full' for a while.

		CBaseEntity *CheckTraceHullAttack( float flDist, int iDamage, int iDmgType );
		BOOL FacingIdeal( void );

		BOOL FCheckAITrigger( void );// checks and, if necessary, fires the monster's trigger target. 
		// friendly fire checks for squad monsters, against the enemy, a target or a position
		virtual bool NoFriendlyFire( void ) { return NoFriendlyFire( pev->origin ); }
		virtual bool NoFriendlyFire( Vector vecPos ) { return NoFriendlyFire( vecPos, m_hEnemy ); }
		virtual bool NoFriendlyFire( Vector vecPos, CBaseEntity *pTarget );
		virtual bool NoFriendlyFireToPos( Vector vecPos );
		virtual bool FVisibleGunPos( CBaseEntity *pEntity, const Vector &vecSrc );
		virtual bool FInBulletCone( CBaseEntity *pEntity, const Vector &vecSrc );

		BOOL BBoxFlat( void );

		// PrescheduleThink 
		virtual void PrescheduleThink( void ) { return; };

		BOOL GetEnemy ( void );
		void MakeDamageBloodDecal ( int cCount, float flNoise, TraceResult *ptr, const Vector &vecDir );
		void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);

	// combat functions
	float UpdateTarget ( entvars_t *pevTarget );
	virtual Activity GetDeathActivity ( void );
	Activity GetSmallFlinchActivity( void );
	virtual void Killed( entvars_t *pevAttacker, int iGib );
	virtual void GibMonster( void );
	BOOL		 ShouldGibMonster( int iGib );
	virtual void CallGibMonster( void );
	void		 OnKeyValueUpdate( const char *szKeyName ) { CBaseEntity::OnKeyValueUpdate( szKeyName ); }
	virtual BOOL	HasHumanGibs( void );
	virtual BOOL	HasAlienGibs( void );
	virtual void	FadeMonster( void );	// Called instead of GibMonster() when gibs are disabled

	Vector ShootAtEnemy( const Vector &shootOrigin );
	virtual Vector BodyTarget( const Vector &posSrc )		// position to shoot at
	{
		if ( FClassnameIs( pev, "monster_osprey" ) || FClassnameIs( pev, "monster_blkop_osprey" ) )
		{
			Vector vecFirst, vecSecond, vecAngles;

			GetBonePosition( OSPREY_FIRST_TARGET_BONE, vecFirst, vecAngles );
			GetBonePosition( OSPREY_SECOND_TARGET_BONE, vecSecond, vecAngles );

			if ( ( vecFirst - posSrc ).Length2D() > ( vecSecond - posSrc ).Length2D() )
				return vecSecond;

			return vecFirst;
		}

		return Center() * 0.75 + EyePosition() * 0.25;
	}

	virtual	Vector  GetGunPosition( void );

	virtual void SetClassification( int cl )
	{
		CBaseEntity::SetClassification( cl );

		if ( m_iClassSelection == CLASS_PLAYER
			|| m_iClassSelection == CLASS_HUMAN_PASSIVE
			|| m_iClassSelection == CLASS_PLAYER_ALLY
			|| m_iClassSelection == CLASS_PLAYER_BIOWEAPON )
		{
			SetPlayerAlly( true );
		}
	}

	virtual bool TakeHealth( float flHealth, int bitsDamageType, int health_cap = 0 );
	virtual bool TakeArmor( float flArmor, int bitsDamageType, int armor_cap = 0 );
	virtual int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	int			DeadTakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	void RadiusDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType );
	void RadiusDamage(Vector vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType );
	virtual int		IsMoving( void ) const { return m_movementGoal != MOVEGOAL_NONE || m_flGroundSpeed > 0; }

	void RouteClear( void );
	void RouteNew( void );
	
	virtual void DeathSound ( void ) { return; };
	virtual void AlertSound ( void ) { return; };
	virtual void IdleSound ( void ) { return; };
	virtual void PainSound ( void ) { return; };
	virtual void MedicCallSound( float flDistance = 0.0f ) { PainSound(); }

	// following a player (m_hTargetEnt is the leader)
	virtual void EXPORT FollowerPlayerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT CallFollowerPlayerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) { FollowerPlayerUse( pActivator, pCaller, useType, value ); }
	virtual bool IsPlayerFollowing( void ) { return m_hTargetEnt != NULL && m_hTargetEnt->IsPlayer(); }
	virtual bool CanPlayerFollow( void );
	virtual bool ShouldStopPlayerFollow( void );
	virtual void LimitFollowers( CBaseEntity *pPlayer, int maxFollowers ) { }
	virtual void StartPlayerFollowing( CBaseEntity *pLeader, bool fSilent = false );
	virtual void StopPlayerFollowing( bool clearSchedule, bool fSilent = false );
	virtual void DeclineFollowing( void ) { }
	virtual void UseSound( void ) { }
	virtual void UnUseSound( void ) { }

	// carried along by a monster we stand on
	virtual void RideMonster( CBaseEntity *pOther ) { }

	virtual void CheckAndApplyGenericAttacks( void );
	virtual bool CheckScared( void ) { return false; }
	virtual void CheckCreatureDanger( void );
	virtual void CheckFallDamage( void );
	virtual void CheckTimeBasedDamage( void );
	virtual void ClearTimeBasedDamage( void );
	virtual bool CheckAttacker( CBaseEntity *pAttacker );

	// player allies can be pushed out of the way, then the entity's own touch runs
	virtual void Touch( CBaseEntity *pOther )
	{
		RideMonster( pOther );

		if ( pOther->IsPlayer() && IsPlayerAlly() )
		{
			float speed = fabs( pOther->pev->velocity.x ) + fabs( pOther->pev->velocity.y );

			if ( speed > PLAYER_ALLY_PUSH_SPEED )
			{
				SetConditions( bits_COND_CLIENT_PUSH );
				MakeIdealYaw( pOther->pev->origin );
			}
		}

		CBaseEntity::Touch( pOther );
	}

	inline void	Remember( int iMemory ) { m_afMemory |= iMemory; }
	inline void	Forget( int iMemory ) { m_afMemory &= ~iMemory; }
	inline BOOL HasMemory( int iMemory ) { if ( m_afMemory & iMemory ) return TRUE; return FALSE; }
	inline BOOL HasAllMemories( int iMemory ) { if ( (m_afMemory & iMemory) == iMemory ) return TRUE; return FALSE; }

	BOOL ExitScriptedSequence( );
	BOOL CineCleanup( );

	CBaseEntity* DropItem ( const char *pszItemName, const Vector &vecPos, const Vector &vecAng );// drop an item.
	CBaseEntity *DropWeapon( const char *name, bool force );

#ifndef CLIENT_DLL
	CBaseEntity *DropItem( CBaseEntity *item, const Vector &position, const Vector &angles );

	void CheckSelectedTank( void );
	virtual bool CheckTankUsage( void );
	CFuncTank *FindTank( void );

	virtual void HandleStep( void );
#endif

	virtual void PlayStepSound( int step, float volume );

#ifndef CLIENT_DLL
	int  m_iStepLeft;
	char m_szTextureName[16];
	char m_chTextureType;
#endif
};



#endif // BASEMONSTER_H
