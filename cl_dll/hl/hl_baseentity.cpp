/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
==========================
This file contains "stubs" of class member implementations so that we can predict certain
 weapons client side.  From time to time you might find that you need to implement part of the
 these functions.  If so, cut it from here, paste it in hl_weapons.cpp or somewhere else and
 add in the functionality you need.
==========================
*/
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"weapons.h"
#include	"nodes.h"
#include	"soundent.h"
#include	"skill.h"

// Globals used by game logic
const Vector g_vecZero = Vector( 0, 0, 0 );
int gmsgWeapPickup = 0;
enginefuncs_t g_engfuncs;
globalvars_t  *gpGlobals;

ItemInfo CBasePlayerItem::ItemInfoArray[MAX_WEAPONS];

void EMIT_SOUND_DYN(edict_t *entity, int channel, const char *sample, float volume, float attenuation, int flags, int pitch) { }
void EMIT_SOUND_DYN( edict_t *entity, int channel, const char *sample, float volume, float attenuation, int flags, int pitch, int targetPlayer, bool useOrigin, const Vector &origin ) { }
int AllocStringLocal( const char *text ) { return 0; }

// CBaseEntity Stubs
CBaseEntity::CBaseEntity() { }
CBaseEntity::~CBaseEntity() { }
bool CBaseEntity::SetupModel( void ) { return false; }
void CBaseEntity::OnCreate( void ) { }
void CBaseEntity::OnDestroy( void ) { }
void CBaseEntity::PreSpawn( void ) { }
void CBaseEntity::Precache( void ) { }
void CBaseEntity::UpdateOnRemove( void ) { }
int CBaseEntity::GetClassification( int cl ) { return CLASS_FORCE_NONE; }
const char *CBaseEntity::GetClassificationTag( void ) { return ""; }
const char *CBaseEntity::GetClassificationName( void ) { return ""; }
bool CBaseEntity::SetCustomKeyvalue( const char *szKeyName, const void *pValue, entvartype_s type, Ownership ownership ) { return false; }
void *CBaseEntity::GetCustomKeyvalue( const char *szKeyName, entvartype_s &type ) { return NULL; }
bool CBaseEntity::RemoveCustomKeyvalue( const char *szKeyName ) { return false; }
bool CBaseEntity::HasCustomKeyvalue( const char *szKeyName ) { return false; }
void *CBaseEntity::InitializeCustomKeyvalueWithDefault( const char *szKeyName, entvartype_s &type ) { return NULL; }
bool CBaseEntity :: TakeHealth( float flHealth, int bitsDamageType, int health_cap ) { return true; }
bool CBaseEntity :: TakeArmor( float flArmor, int bitsDamageType, int armor_cap ) { return true; }

bool CBaseEntity::FVisibleFromPos( const Vector &start, const Vector &end )
{
	return false;
}

bool CBaseEntity::IsFacing( entvars_t *target, float fieldOfView )
{
	return false;
}

void CBaseEntity::GetDamagePoints( entvars_t *inflictor, entvars_t *attacker, float damage )
{
}

void CBaseEntity::SUB_UseTargets( CBaseEntity *activator, USE_TYPE useType, float value )
{
}

void CBaseEntity :: Teleport( Vector vecOrigin, Vector vecAngles ) { }
bool CBaseEntity :: IsPointEnt( void ) { return false; }
int CBaseEntity :: FindMonstersInWorld( CBaseEntity **pList, int listMax, int flagMask ) { return 0; }
bool CBaseEntity :: BlockedByEntity( CBaseEntity *pOther, float flDamage ) { return false; }
int CBaseEntity :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType ) { return 1; }
CBaseEntity *CBaseEntity::GetNextTarget( void ) { return NULL; }
int CBaseEntity::Save( CSave &save ) { return 1; }
int CBaseEntity::Restore( CRestore &restore ) { return 1; }
void CBaseEntity::SetObjectCollisionBox( void ) { }
int	CBaseEntity :: Intersects( CBaseEntity *pOther ) { return 0; }
void CBaseEntity :: MakeDormant( void ) { }
int CBaseEntity :: IsDormant( void ) { return 0; }
BOOL CBaseEntity :: IsInWorld( void ) { return TRUE; }
int CBaseEntity::ShouldToggle( USE_TYPE useType, BOOL currentState ) { return 0; }
int	CBaseEntity :: DamageDecal( int bitsDamageType ) { return -1; }
CBaseEntity * CBaseEntity::Create( const char *szName, const Vector &vecOrigin, const Vector &vecAngles, bool fCreateAndDontSpawn, edict_t *pentOwner ) { return NULL; }
void CBaseEntity::SUB_Remove( void ) { }

// CBaseDelay Stubs
void CBaseDelay :: KeyValue( KeyValueData * ) { }
int CBaseDelay::Restore( class CRestore & ) { return 1; }
int CBaseDelay::Save( class CSave & ) { return 1; }

void CBaseDelay::SUB_UseTargets( CBaseEntity *activator, USE_TYPE useType, float value )
{
}

// CBaseAnimating Stubs
int CBaseAnimating::Restore( class CRestore & ) { return 1; }
int CBaseAnimating::Save( class CSave & ) { return 1; }

// DEBUG Stubs
edict_t *DBG_EntOfVars( const entvars_t *pev ) { return NULL; }
void DBG_AssertFunction(BOOL fExpr,	const char*	szExpr,	const char*	szFile,	int szLine,	const char*	szMessage) { }

// UTIL_* Stubs
int UTIL_PrecacheModel( CBaseEntity *entity, const char *model ) { return 0; }
void UTIL_PrecacheOther( const char *szClassname ) { }
void UTIL_BloodDrips( const Vector &origin, const Vector &direction, int color, int amount ) { }
void UTIL_DecalTrace( TraceResult *pTrace, int decalNumber ) { }
void UTIL_GunshotDecalTrace( TraceResult *pTrace, int decalNumber ) { }
void UTIL_MakeVectors( const Vector &vecAngles ) { }
BOOL UTIL_IsValidEntity( edict_t *pent ) { return TRUE; }
void UTIL_SetOrigin( entvars_t *, const Vector &org ) { }
BOOL UTIL_GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon ) { return TRUE; }
void UTIL_LogPrintf(char *,...) { }
void UTIL_ClientPrintAll( int,char const *,char const *,char const *,char const *,char const *) { }
void ClientPrint( entvars_t *client, int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 ) { }

// CBaseToggle Stubs
void CBaseToggle::OnCreate( void ) { }
void CBaseToggle::OnDestroy( void ) { }
BOOL CBaseToggle::IsLockedByMaster( void ) { return FALSE; }
int CBaseToggle::Restore( class CRestore & ) { return 1; }
int CBaseToggle::Save( class CSave & ) { return 1; }
void CBaseToggle :: KeyValue( KeyValueData * ) { }


void UTIL_Remove( CBaseEntity *pEntity ){ }
struct skilldata_t  gSkillData;
void UTIL_SetSize( entvars_t *pev, const Vector &vecMin, const Vector &vecMax ){ }
CBaseEntity *UTIL_FindEntityInSphere( CBaseEntity *pStartEntity, const Vector &vecCenter, float flRadius ){ return 0;}

Vector UTIL_VecToAngles( const Vector &vec ){ return 0; }


CBaseEntity* CBaseMonster :: CheckTraceHullAttack( float flDist, int iDamage, int iDmgType ) { return NULL; }
void CBaseMonster :: Eat ( float flFullDuration ) { }
BOOL CBaseMonster :: FShouldEat ( void ) { return TRUE; }
void CBaseMonster :: BarnacleVictimBitten ( entvars_t *pevBarnacle, float flDamage ) { }
void CBaseMonster :: BarnacleVictimReleased ( void ) { }
void CBaseMonster :: Listen ( void ) { }
float CBaseMonster :: FLSoundVolume ( CSound *pSound ) { return 0.0; }
BOOL CBaseMonster :: FValidateHintType ( short sHint ) { return FALSE; }
void CBaseMonster :: Look ( int iDistance ) { }
int CBaseMonster :: ISoundMask ( void ) { return 0; }
CSound* CBaseMonster :: PBestSound ( void ) { return NULL; }
CSound* CBaseMonster :: PBestScent ( void ) { return NULL; } 
float CBaseAnimating :: StudioFrameAdvance ( float flInterval ) { return 0.0; }
void CBaseMonster :: MonsterThink ( void ) { }
void CBaseMonster :: MonsterUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) { }
int CBaseMonster :: IgnoreConditions ( void ) { return 0; }
void CBaseMonster :: RouteClear ( void ) { }
void CBaseMonster :: RouteNew ( void ) { }
BOOL CBaseMonster :: FRouteClear ( void ) { return FALSE; }
BOOL CBaseMonster :: FRefreshRoute ( float flMoveRadius ) { return 0; }
BOOL CBaseMonster::MoveToEnemy( Activity movementAct, float waitTime, float flMoveRadius ) { return FALSE; }
BOOL CBaseMonster::MoveToLocation( Activity movementAct, float waitTime, const Vector &goal, float flMoveRadius ) { return FALSE; }
BOOL CBaseMonster::MoveToCover( Activity movementAct, float waitTime, const Vector &goal, float flMoveRadius ) { return FALSE; }
BOOL CBaseMonster::MoveToTarget( Activity movementAct, float waitTime, float flMoveRadius ) { return FALSE; }
BOOL CBaseMonster::MoveToNode( Activity movementAct, float waitTime, const Vector &goal, float flMoveRadius ) { return FALSE; }
int ShouldSimplify( int routeType ) { return TRUE; }
void CBaseMonster :: RouteSimplify( CBaseEntity *pTargetEnt ) { }
bool CBaseMonster :: FBecomeProne ( CBaseEntity *pOther ) { return TRUE; }
BOOL CBaseMonster :: CheckRangeAttack1 ( float flDot, float flDist ) { return FALSE; }
BOOL CBaseMonster :: CheckRangeAttack2 ( float flDot, float flDist ) { return FALSE; }
BOOL CBaseMonster :: CheckMeleeAttack1 ( float flDot, float flDist ) { return FALSE; }
BOOL CBaseMonster :: CheckMeleeAttack2 ( float flDot, float flDist ) { return FALSE; }
void CBaseMonster :: CheckAttacks ( CBaseEntity *pTarget, float flDist ) { }
BOOL CBaseMonster :: FCanCheckAttacks ( void ) { return FALSE; }
int CBaseMonster :: CheckEnemy ( CBaseEntity *pEnemy ) { return 0; }
void CBaseMonster :: PushEnemy( CBaseEntity *pEnemy, Vector &vecLastKnownPos ) { }
BOOL CBaseMonster :: PopEnemy( ) { return FALSE; }
void CBaseMonster :: SetActivity ( Activity NewActivity ) { }
void CBaseMonster :: SetSequenceByName ( const char *szSequence ) { }
int CBaseMonster :: CheckLocalMove ( const Vector &vecStart, const Vector &vecEnd, CBaseEntity *pTarget, float *pflDist, Vector *pvecEndPosition, bool fWalkMoveCheck ) { return 0; }
float CBaseMonster :: OpenDoorAndWait( entvars_t *pevDoor ) { return 0.0; }
void CBaseMonster :: AdvanceRoute ( float distance ) { }
int CBaseMonster :: RouteClassify( int iMoveFlag ) { return 0; }
BOOL CBaseMonster :: BuildRoute ( const Vector &vecGoal, int iMoveFlag, CBaseEntity *pTarget, bool fNoExtension ) { return FALSE; }
void CBaseMonster :: InsertWaypoint ( Vector vecLocation, int afMoveFlags ) { }
BOOL CBaseMonster :: FTriangulate ( const Vector &vecStart , const Vector &vecEnd, float flDist, CBaseEntity *pTargetEnt, Vector *pApex, Vector *pApex2, bool fNoExtension ) { return FALSE; }
BOOL CBaseMonster :: FTriangulateExtension ( const Vector &vecStart, const Vector &vecEnd, float flDist, CBaseEntity *pTargetEnt, Vector *pApex ) { return FALSE; }
void CBaseMonster :: Move ( float flInterval ) { }
BOOL CBaseMonster:: ShouldAdvanceRoute( float flWaypointDist ) { return FALSE; }
void CBaseMonster::MoveExecute( CBaseEntity *pTargetEnt, const Vector &vecDir, float flInterval ) { }
void CBaseMonster :: MonsterInit ( void ) { }
void CBaseMonster :: MonsterInitThink ( void ) { }
void CBaseMonster :: StartMonster ( void ) { }
void CBaseMonster :: MovementComplete( void ) { }
int CBaseMonster::TaskIsRunning( void ) { return 0; }
int CBaseEntity::IRelationship( CBaseEntity *other, bool ignoreNoTarget ) { return 0; }
BOOL CBaseMonster :: FindCover ( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist ) { return FALSE; }
BOOL CBaseMonster :: FindCoverGrenade ( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist ) { return FALSE; }
void CBaseMonster :: FollowerPlayerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) { }
bool CBaseMonster :: CanPlayerFollow( void ) { return false; }
void CBaseMonster :: CheckFallDamage( void ) { }
void CBaseMonster :: CheckAndApplyGenericAttacks( void ) { }
void CBaseMonster :: CheckCreatureDanger( void ) { }
BOOL CBaseMonster :: CheckRangeAttack1_Move ( float flDot, float flDist ) { return FALSE; }
BOOL CBaseMonster :: CheckRangeAttack2_Move ( float flDot, float flDist ) { return FALSE; }
BOOL CBaseMonster :: CheckMeleeAttack1_Move ( float flDot, float flDist ) { return FALSE; }
BOOL CBaseMonster :: CheckMeleeAttack2_Move ( float flDot, float flDist ) { return FALSE; }
void CBaseMonster :: CheckAttacks_Move ( CBaseEntity *pTarget, float flDist ) { }
void CBaseMonster :: ClearEnemyList( void ) { }
bool CBaseMonster :: IsFriendly( CBaseEntity *pOther ) { return false; }
void CBaseMonster :: CheckTimeBasedDamage( void ) { }
void CBaseMonster :: ClearTimeBasedDamage( void ) { }
bool CBaseMonster :: CheckAttacker( CBaseEntity *pAttacker ) { return false; }
BOOL CBaseMonster :: FindAttackPoint ( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist ) { return FALSE; }
BOOL CBaseMonster :: FindLateralSafety ( const Vector &vecThreat, float flMinDist, float flDist ) { return FALSE; }
BOOL CBaseMonster :: FindLateralSafetyGrenade ( const Vector &vecThreat ) { return FALSE; }
BOOL CBaseMonster :: FindCoverDistance ( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist ) { return FALSE; }
bool CBaseMonster :: ShouldStopPlayerFollow( void ) { return false; }
void CBaseMonster :: StartPlayerFollowing( CBaseEntity *pLeader, bool fSilent ) { }
void CBaseMonster :: StopPlayerFollowing( bool clearSchedule, bool fSilent ) { }
BOOL CBaseMonster :: BuildNearestRoute ( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist ) { return FALSE; }
CBaseEntity *CBaseMonster :: BestVisibleEnemy ( void ) { return NULL; }
BOOL CBaseMonster :: FInViewCone ( CBaseEntity *pEntity ) { return FALSE; }
BOOL CBaseMonster :: FInViewCone ( Vector *pOrigin ) { return FALSE; }
bool CBaseEntity :: FVisible ( CBaseEntity *pEntity, bool fIgnoreGlass ) { return false; }
BOOL CBaseEntity :: FVisible ( const Vector &vecOrigin ) { return FALSE; }
void CBaseMonster :: MakeIdealYaw( Vector vecTarget ) { }
float	CBaseMonster::FlYawDiff ( void ) { return 0.0; }
float CBaseMonster::ChangeYaw ( int yawSpeed ) { return 0; }
float	CBaseMonster::VecToYaw ( Vector vecDir ) { return 0.0; }
int CBaseAnimating :: LookupActivity ( int activity ) { return 0; }
int CBaseAnimating :: LookupActivityHeaviest ( int activity ) { return 0; }
void CBaseMonster :: SetEyePosition ( void ) { }
int CBaseAnimating :: LookupSequence ( const char *label ) { return 0; }
void CBaseAnimating :: ResetSequenceInfo ( ) { }
BOOL CBaseAnimating :: GetSequenceFlags( ) { return FALSE; }
void CBaseAnimating :: DispatchAnimEvents ( float flInterval ) { }
void CBaseMonster :: HandleAnimEvent( MonsterEvent_t *pEvent ) { }
float CBaseAnimating :: SetBoneController ( int iController, float flValue ) { return 0.0; }
void CBaseAnimating :: InitBoneControllers ( void ) { }
float CBaseAnimating :: SetBlending ( int iBlender, float flValue ) { return 0; }
void CBaseAnimating :: GetBonePosition ( int iBone, Vector &origin, Vector &angles ) { }
void CBaseAnimating :: GetAttachment ( int iAttachment, Vector &origin, Vector &angles ) { }
int CBaseAnimating :: FindTransition( int iEndingSequence, int iGoalSequence, int *piDir ) { return -1; }
void CBaseAnimating :: GetAutomovement( Vector &origin, Vector &angles, float flInterval ) { }
void CBaseAnimating :: SetBodygroup( int iGroup, int iValue ) { }
int CBaseAnimating :: GetBodygroup( int iGroup ) { return 0; }
Vector CBaseMonster :: GetGunPosition( void ) { return g_vecZero; }
void CBaseEntity::TraceAttack(entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType) { }
void CBaseEntity::FireBullets(ULONG cShots, Vector vecSrc, Vector vecDirShooting, Vector vecSpread, float flDistance, int iBulletType, int iTracerFreq, int iDamage, entvars_t *pevAttacker, FireBulletsDrawMode fDraw ) { }
void CBaseEntity :: TraceBleed( float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType ) { }
void CBaseMonster :: MakeDamageBloodDecal ( int cCount, float flNoise, TraceResult *ptr, const Vector &vecDir ) { }
BOOL CBaseMonster :: FGetNodeRoute ( Vector vecDest ) { return TRUE; }
int CBaseMonster :: FindHintNode ( void ) { return NO_NODE; }
void CBaseMonster::ReportAIState( void ) { }
void CBaseMonster :: KeyValue( KeyValueData *pkvd ) { }
BOOL CBaseMonster :: FCheckAITrigger ( void ) { return FALSE; }
int CBaseMonster :: CanPlaySequence( BOOL fDisregardMonsterState, int interruptLevel ) { return FALSE; }
BOOL CBaseMonster :: FindLateralCover ( const Vector &vecThreat, const Vector &vecViewOffset ) { return FALSE; }
Vector CBaseMonster :: ShootAtEnemy( const Vector &shootOrigin ) { return g_vecZero; }
BOOL CBaseMonster :: FacingIdeal( void ) { return FALSE; }
BOOL CBaseMonster :: FCanActiveIdle ( void ) { return FALSE; }
void CBaseMonster::PlaySentence( const char *pszSentence, float duration, float volume, float attenuation ) { }
void CBaseMonster::PlayScriptedSentence( const char *pszSentence, float duration, float volume, float attenuation, BOOL bConcurrent, CBaseEntity *pListener ) { }
void CBaseMonster::SentenceStop( void ) { }
void CBaseMonster::CorpseFallThink( void ) { }
void CBaseMonster :: MonsterInitDead( void ) { }
BOOL CBaseMonster :: BBoxFlat ( void ) { return TRUE; }
BOOL CBaseMonster :: GetEnemy ( void ) { return FALSE; }
void CBaseMonster :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType) { }
CBaseEntity* CBaseMonster :: DropItem ( const char *pszItemName, const Vector &vecPos, const Vector &vecAng ) { return NULL; }
BOOL CBaseMonster :: ShouldFadeOnDeath( void ) { return FALSE; }
void CBaseMonster :: RadiusDamage(entvars_t* pevInflictor, entvars_t*	pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType ) { }
void CBaseMonster :: RadiusDamage( Vector vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType ) { }
void CBaseMonster::FadeMonster( void ) { }
void CBaseMonster :: GibMonster( void ) { }
BOOL CBaseMonster :: HasHumanGibs( void ) { return FALSE; }
BOOL CBaseMonster :: HasAlienGibs( void ) { return FALSE; }
Activity CBaseMonster :: GetDeathActivity ( void ) { return ACT_DIE_HEADSHOT; }
MONSTERSTATE CBaseMonster :: GetIdealState ( void ) { return MONSTERSTATE_ALERT; }
Schedule_t* CBaseMonster :: GetScheduleOfType ( int Type ) { return NULL; }
Schedule_t *CBaseMonster :: GetSchedule ( void ) { return NULL; }
void CBaseMonster :: RunTask ( Task_t *pTask ) { }
void CBaseMonster :: StartTask ( Task_t *pTask ) { }
Schedule_t *CBaseMonster::ScheduleFromName( const char *pName ) { return NULL;}
void CBaseMonster::BecomeDead( float flSavedHealth ) {}
void CBaseMonster :: RunAI ( void ) {}
void CBaseMonster :: Killed( entvars_t *pevAttacker, int iGib ) {}
bool CBaseMonster :: TakeHealth (float flHealth, int bitsDamageType, int health_cap) { return false; }
bool CBaseMonster :: TakeArmor( float flArmor, int bitsDamageType, int armor_cap ) { return false; }
void CBaseMonster :: SetGaitActivity( Activity NewActivity ) { }
void CBaseAnimating :: ResetGaitSequenceInfo( void ) { }
int CBaseAnimating :: GetSequenceActivityName( int iSequence ) { return 0; }
bool CBaseMonster :: NoFriendlyFire( Vector vecPos, CBaseEntity *pTarget ) { return true; }
bool CBaseMonster :: NoFriendlyFireToPos( Vector vecPos ) { return true; }
bool CBaseMonster :: FVisibleGunPos( CBaseEntity *pEntity, const Vector &vecSrc ) { return false; }
bool CBaseMonster :: FInBulletCone( CBaseEntity *pEntity, const Vector &vecSrc ) { return false; }
int CBaseMonster :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType ) { return 0; }
int CBaseMonster::Restore( class CRestore & ) { return 1; }
int CBaseMonster::Save( class CSave & ) { return 1; }
void CBaseMonster::CallGibMonster( void ) { }
void CBaseMonster::ApplyEffects( void ) { }
void CBaseMonster::EndRevive( float flTimeUntilRevive ) { }
void CBaseMonster::PlayStepSound( int step, float volume ) { }

int TrainSpeed(int iSpeed, int iMax) { 	return 0; }
void CBasePlayer :: DeathSound( void ) { }
bool CBasePlayer :: TakeHealth( float flHealth, int bitsDamageType, int health_cap ) { return false; }
void CBasePlayer :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType) { }
int CBasePlayer :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType ) { return 0; }
void CBasePlayer::PackDeadPlayerItems( void ) { }
void CBasePlayer::RemoveAllItems( bool removeSuit )
{ }
void CBasePlayer::SetAnimation( PLAYER_ANIM playerAnim, int iParam ) { }
void CBasePlayer::WaterMove() { }
BOOL CBasePlayer::IsOnLadder( void ) { return FALSE; }
void CBasePlayer::PlayerDeathThink(void) { }
void CBasePlayer::StartDeathCam( void ) { }
void CBasePlayer::StartObserver( Vector vecPosition, Vector vecViewAngle ) { }
void CBasePlayer::PlayerUse ( void ) { }
void CBasePlayer::Jump() { }
void CBasePlayer::Duck( ) { }
int  CBasePlayer::Classify ( void ) { return 0; }
void CBasePlayer::PreThink(void) { }
void CBasePlayer::CheckTimeBasedDamage()  { }
void CBasePlayer :: UpdateGeigerCounter( void ) { }
void CBasePlayer::CheckSuitUpdate() { }
void CBasePlayer::SetSuitUpdate(const char *name, int fgroup, int iNoRepeatTime) { }
void CBasePlayer :: UpdatePlayerSound ( void ) { }
void CBasePlayer::PostThink() { }
void CBasePlayer :: Precache( void ) { }
int CBasePlayer::Save( CSave &save ) { return 0; }
void CBasePlayer::RenewItems(void) { }
int CBasePlayer::Restore( CRestore &restore ) { return 0; }
void CBasePlayer::SelectNextItem( int iItem ) { }
BOOL CBasePlayer::HasWeapons( void ) { return FALSE; }
void CBasePlayer::SelectPrevItem( int iItem ) { }
CBaseEntity *FindEntityForward( CBaseEntity *pMe ) { return NULL; }
BOOL CBasePlayer :: FlashlightIsOn( void ) { return FALSE; }
void CBasePlayer :: FlashlightTurnOn( void ) { }
void CBasePlayer :: FlashlightTurnOff( void ) { }
void CBasePlayer :: ForceClientDllUpdate( void ) { }
void CBasePlayer::ImpulseCommands( ) { }
void CBasePlayer::CheatImpulseCommands( int iImpulse ) { }
int CBasePlayer::AddPlayerItem( CBasePlayerItem *pItem ) { return FALSE; }
bool CBasePlayer::RemovePlayerItem( CBasePlayerItem *pItem ) { return FALSE; }
void CBasePlayer::ItemPreFrame() { }
void CBasePlayer::ItemPostFrame() { }
int CBasePlayer::AmmoInventory( int iAmmoIndex ) { return -1; }
int CBasePlayer::GetAmmoIndex(const char *psz) { return -1; }
void CBasePlayer::SendAmmoUpdate(void) { }
void CBasePlayer :: UpdateClientData( void ) { }
bool CBasePlayer :: FBecomeProne ( CBaseEntity *pOther ) { return TRUE; }
void CBasePlayer :: BarnacleVictimBitten ( entvars_t *pevBarnacle, float flDamage ) { }
void CBasePlayer :: BarnacleVictimReleased ( void ) { }
int CBasePlayer :: Illumination( void ) { return 0; }
void CBasePlayer :: EnableControl(BOOL fControl) { }
Vector CBasePlayer :: GetAutoaimVector( float flDelta ) { return g_vecZero; }
Vector CBasePlayer :: AutoaimDeflection( Vector &vecSrc, float flDist, float flDelta  ) { return g_vecZero; }
void CBasePlayer :: ResetAutoaim( ) { }
void CBasePlayer :: SetCustomDecalFrames( int nFrames ) { }
int CBasePlayer :: GetCustomDecalFrames( void ) { return -1; }
void CBasePlayer::DropPlayerItem ( char *pszItemName ) { }
BOOL CBasePlayer::HasPlayerItem( CBasePlayerItem *pCheckItem ) { return FALSE; }
BOOL CBasePlayer :: SwitchWeapon( CBasePlayerItem *pWeapon )  { return FALSE; }
Vector CBasePlayer :: GetGunPosition( void ) { return g_vecZero; }
const char *CBasePlayer::TeamID( void ) { return ""; }
int CBasePlayer :: GiveAmmo( int iCount, const char *szName, int iMax, bool fFromWeapon, int *piAmountGiven ) { return 0; }
void CBasePlayer::AddPoints( int score, bool bAllowNegativeScore ) { }
void CBasePlayer::AddPointsToTeam( int score, bool bAllowNegativeScore ) { }
bool CBasePlayer::IsMoving( void ) { return pev->velocity != g_vecZero; }
float CBasePlayer::GetLogFrequency( void ) { return 30.0f; }
float CBasePlayer::GetPointsForDamage( float flDamage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	return 0 * flDamage;
}
void CBasePlayer::ResetOverriddenPlayerModel( bool restorePreviousModel, bool forceRestore ) { }
bool CBasePlayer::CanStartNextVote( VOTETYPE type ) { return true; }
void CBasePlayer::Vote( VOTEINPUT input, const char *text ) { }
VOTEINPUT CBasePlayer::LastVoteInput() { return VOTE_YES; }
float CBasePlayer::TimeToStartNextVote() { return 0.0f; }

CToEntityCollision::CToEntityCollision() : m_flNotSolidUntil( 0 ) { }
CBaseEntity *CToEntityCollision::GetEnt( void ) { return NULL; }
CToPlayerCollisions::CToPlayerCollisions() { }
CToPlayerCollisions::~CToPlayerCollisions() { }

void ClearMultiDamage(void) { }
void ApplyMultiDamage(entvars_t *pevInflictor, entvars_t *pevAttacker ) { }
void AddMultiDamage( entvars_t *pevInflictor, CBaseEntity *pEntity, float flDamage, int bitsDamageType) { }
void SpawnBlood(Vector vecSpot, int bloodColor, float flDamage) { }
int DamageDecal( CBaseEntity *pEntity, int bitsDamageType ) { return 0; }
void DecalGunshot( TraceResult *pTrace, int iBulletType ) { }
void EjectBrass ( const Vector &vecOrigin, const Vector &vecVelocity, float rotation, int model, int soundtype ) { }
void AddAmmoNameToAmmoRegistry( const char *szAmmoname )
{ }
void CSoundEnt::InsertSound ( int iType, const Vector &vecOrigin, int iVolume, float flDuration, CBaseEntity *pOwner ) {}
void RadiusDamage( Vector vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, float flRadius, int iClassIgnore, int bitsDamageType ){}
