#ifndef CCUSTOMENTITY_H
#define CCUSTOMENTITY_H

#include "ICustomEntity.h"
#include "CCustomEntityHandler.h"
#include "CString.h"
#include "monsterevent.h"

class CASObjectInstance;
struct Schedule_t;
struct Task_t;

//=========================================================
// Script custom entities. Each method calls the script
// class's method of the same declaration when it has one,
// and otherwise the C++ base. The declarations are what
// CASObjectInstance looks the methods up by
//=========================================================
template <class BASE>
class CBaseCustomBaseEntity : public BASE, public ICustomEntity
{
public:
	virtual void OnCreate( void )
	{
		BASE::OnCreate();
		m_pHandler = new CCustomEntityHandler( this );
	}

	virtual void OnDestroy( void )
	{
		m_pHandler->CallMethod( true, "void OnDestroy()" );
		CCustomEntityHandler::ScheduleForRemoval( m_pHandler );
		m_pHandler = NULL;

		BASE::OnDestroy();
	}

	virtual ICustomEntity *MyCustomPointer( void ) { return this; }

	virtual CCustomEntityHandler *GetHandler( void ) const { return m_pHandler; }
	virtual CASObjectInstance *GetScriptInstance( void ) const { return m_pHandler->GetScriptInstance(); }
	virtual void SetScriptInstance( CASObjectInstance *instance ) { m_pHandler->SetScriptInstance( instance ); }

	virtual void KeyValue( KeyValueData *pkvd )
	{
		// the script file was only needed to find the class
		if ( !strcmp( "m_iszScriptFile", pkvd->szKeyName ) )
		{
			pkvd->fHandled = TRUE;
			return;
		}

		if ( !m_pHandler->MethodExists( "bool KeyValue(const string& in, const string& in)" ) )
		{
			BASE::KeyValue( pkvd );
			return;
		}

		CString keyName( pkvd->szKeyName );
		CString value( pkvd->szValue );
		bool fHandled = false;

		m_pHandler->CallMethod( &fHandled, false, "bool KeyValue(const string& in, const string& in)", &keyName, &value );
		pkvd->fHandled = fHandled;
	}

	virtual void Spawn( void )
	{
		if ( m_pHandler->MethodExists( "void Spawn()" ) )
			m_pHandler->CallMethod( false, "void Spawn()" );
		else
			BASE::Spawn();
	}

	virtual void PostSpawn( void )
	{
		if ( m_pHandler->MethodExists( "void PostSpawn()" ) )
			m_pHandler->CallMethod( false, "void PostSpawn()" );
		else
			BASE::PostSpawn();
	}

	virtual void Activate( void )
	{
		if ( m_pHandler->MethodExists( "void Activate()" ) )
			m_pHandler->CallMethod( false, "void Activate()" );
	}

	virtual void PreSpawn( void )
	{
		if ( m_pHandler->MethodExists( "void PreSpawn()" ) )
			m_pHandler->CallMethod( false, "void PreSpawn()" );
		else
			BASE::PreSpawn();
	}

	virtual void Precache( void )
	{
		if ( m_pHandler->MethodExists( "void Precache()" ) )
			m_pHandler->CallMethod( false, "void Precache()" );
		else
			BASE::Precache();
	}

	virtual void UpdateOnRemove( void )
	{
		if ( m_pHandler->MethodExists( "void UpdateOnRemove()" ) )
			m_pHandler->CallMethod( false, "void UpdateOnRemove()" );
		else
			BASE::UpdateOnRemove();
	}

	virtual int ObjectCaps( void )
	{
		if ( !m_pHandler->MethodExists( "int ObjectCaps()" ) )
			return BASE::ObjectCaps();

		int iCaps = 0;

		m_pHandler->CallMethod( &iCaps, false, "int ObjectCaps()" );
		return iCaps;
	}

	virtual BOOL IsBSPModel( void )
	{
		if ( !m_pHandler->MethodExists( "bool IsBSPModel()" ) )
			return BASE::IsBSPModel();

		bool fResult = false;

		m_pHandler->CallMethod( &fResult, false, "bool IsBSPModel()" );
		return fResult;
	}

	virtual int BloodColor( void )
	{
		if ( !m_pHandler->MethodExists( "int BloodColor()" ) )
			return BASE::BloodColor();

		int iColor = 0;

		m_pHandler->CallMethod( &iColor, false, "int BloodColor()" );
		return iColor;
	}

	virtual CBaseEntity *Respawn( void )
	{
		if ( !m_pHandler->MethodExists( "BASECLASS@ Respawn()" ) )
			return BASE::Respawn();

		CBaseEntity *pEntity = NULL;

		m_pHandler->CallMethod( &pEntity, false, "BASECLASS@ Respawn()" );
		return pEntity;
	}

	virtual bool IsRevivable( void )
	{
		if ( !m_pHandler->MethodExists( "bool IsRevivable()" ) )
			return BASE::IsRevivable();

		bool fResult = false;

		m_pHandler->CallMethod( &fResult, false, "bool IsRevivable()" );
		return fResult;
	}

	virtual void BeginRevive( float flTimeUntilRevive )
	{
		if ( m_pHandler->MethodExists( "void BeginRevive(float)" ) )
			m_pHandler->CallMethod( false, "void BeginRevive(float)", flTimeUntilRevive );
		else
			BASE::BeginRevive( flTimeUntilRevive );
	}

	virtual void EndRevive( float flTimeUntilRevive )
	{
		if ( m_pHandler->MethodExists( "void EndRevive(float)" ) )
			m_pHandler->CallMethod( false, "void EndRevive(float)", flTimeUntilRevive );
		else
			BASE::EndRevive( flTimeUntilRevive );
	}

	virtual void Killed( entvars_t *pevAttacker, int iGib )
	{
		if ( m_pHandler->MethodExists( "void Killed(entvars_t@, int)" ) )
			m_pHandler->CallMethod( false, "void Killed(entvars_t@, int)", pevAttacker, iGib );
		else
			BASE::Killed( pevAttacker, iGib );
	}

	virtual float GetPointsForDamage( float flDamage )
	{
		if ( !m_pHandler->MethodExists( "float GetPointsForDamage(float)" ) )
			return BASE::GetPointsForDamage( flDamage );

		float flPoints = 0;

		m_pHandler->CallMethod( &flPoints, false, "float GetPointsForDamage(float)", flDamage );
		return flPoints;
	}

	virtual BOOL OnControls( entvars_t *pevTest )
	{
		if ( !m_pHandler->MethodExists( "bool OnControls(entvars_t@)" ) )
			return BASE::OnControls( pevTest );

		bool fResult = false;

		m_pHandler->CallMethod( &fResult, false, "bool OnControls(entvars_t@)", pevTest );
		return fResult;
	}

	virtual bool TakeHealth( float flHealth, int bitsDamageType, int health_cap = 0 )
	{
		if ( !m_pHandler->MethodExists( "bool TakeHealth(float, int, int)" ) )
			return BASE::TakeHealth( flHealth, bitsDamageType, health_cap );

		bool fResult = false;

		m_pHandler->CallMethod( &fResult, false, "bool TakeHealth(float, int, int)", flHealth, bitsDamageType, health_cap );
		return fResult;
	}

	virtual bool TakeArmor( float flArmor, int bitsDamageType, int armor_cap = 0 )
	{
		if ( !m_pHandler->MethodExists( "bool TakeArmor(float, int, int)" ) )
			return BASE::TakeArmor( flArmor, bitsDamageType, armor_cap );

		bool fResult = false;

		m_pHandler->CallMethod( &fResult, false, "bool TakeArmor(float, int, int)", flArmor, bitsDamageType, armor_cap );
		return fResult;
	}

	virtual int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
	{
		if ( !m_pHandler->MethodExists( "int TakeDamage(entvars_t@, entvars_t@, float, int)" ) )
			return BASE::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );

		int iResult = 0;

		m_pHandler->CallMethod( &iResult, false, "int TakeDamage(entvars_t@, entvars_t@, float, int)",
		                        pevInflictor, pevAttacker, flDamage, bitsDamageType );
		return iResult;
	}

	virtual void TraceBleed( float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
	{
		if ( m_pHandler->MethodExists( "void TraceBleed(float, const Vector& in, TraceResult& in, int)" ) )
		{
			m_pHandler->CallMethod( false, "void TraceBleed(float, const Vector& in, TraceResult& in, int)",
			                        flDamage, &vecDir, ptr, bitsDamageType );
		}
		else
		{
			BASE::TraceBleed( flDamage, vecDir, ptr, bitsDamageType );
		}
	}

	virtual void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
	{
		if ( m_pHandler->MethodExists( "void TraceAttack(entvars_t@, float, const Vector& in, TraceResult& in, int)" ) )
		{
			m_pHandler->CallMethod( false, "void TraceAttack(entvars_t@, float, const Vector& in, TraceResult& in, int)",
			                        pevAttacker, flDamage, &vecDir, ptr, bitsDamageType );
		}
		else
		{
			BASE::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
		}
	}

	// the script's callbacks, set through the handler, come first
	virtual void Think( void )
	{
		if ( !m_pHandler->CallThink() )
			BASE::Think();
	}

	virtual void Touch( CBaseEntity *pOther )
	{
		if ( !m_pHandler->CallTouch( pOther ) )
			BASE::Touch( pOther );
	}

	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		if ( !m_pHandler->CallUse( pActivator, pCaller, useType, value ) )
			BASE::Use( pActivator, pCaller, useType, value );
	}

	virtual void Blocked( CBaseEntity *pOther )
	{
		if ( !m_pHandler->CallBlocked( pOther ) )
			BASE::Blocked( pOther );
	}

protected:
	CCustomEntityHandler	*m_pHandler;
};

template <class BASE>
class CBaseCustomBaseAnimating : public CBaseCustomBaseEntity<BASE>
{
};

template <class BASE>
class CBaseCustomMonsterEntity : public CBaseCustomBaseEntity<BASE>
{
public:
	virtual void CallGibMonster( void )
	{
		if ( this->m_pHandler->MethodExists( "void CallGibMonster()" ) )
			this->m_pHandler->CallMethod( false, "void CallGibMonster()" );
		else
			BASE::CallGibMonster();
	}

	virtual int Classify( void )
	{
		if ( !this->m_pHandler->MethodExists( "int Classify()" ) )
			return BASE::Classify();

		int iClass = 0;

		this->m_pHandler->CallMethod( &iClass, false, "int Classify()" );
		return iClass;
	}

	virtual void PainSound( void )
	{
		if ( this->m_pHandler->MethodExists( "void PainSound()" ) )
			this->m_pHandler->CallMethod( false, "void PainSound()" );
	}

	virtual void AlertSound( void )
	{
		if ( this->m_pHandler->MethodExists( "void AlertSound()" ) )
			this->m_pHandler->CallMethod( false, "void AlertSound()" );
	}

	virtual void DeathSound( void )
	{
		if ( this->m_pHandler->MethodExists( "void DeathSound()" ) )
			this->m_pHandler->CallMethod( false, "void DeathSound()" );
	}

	virtual void PrescheduleThink( void )
	{
		if ( this->m_pHandler->MethodExists( "void PrescheduleThink()" ) )
			this->m_pHandler->CallMethod( false, "void PrescheduleThink()" );
	}

	virtual void CheckAmmo( void )
	{
		if ( this->m_pHandler->MethodExists( "void CheckAmmo()" ) )
			this->m_pHandler->CallMethod( false, "void CheckAmmo()" );
	}

	virtual void SetYawSpeed( void )
	{
		if ( this->m_pHandler->MethodExists( "void SetYawSpeed()" ) )
			this->m_pHandler->CallMethod( false, "void SetYawSpeed()" );
	}

	virtual void SetupFriendly( void )
	{
		if ( this->m_pHandler->MethodExists( "void SetupFriendly()" ) )
			this->m_pHandler->CallMethod( false, "void SetupFriendly()" );
	}

	virtual void GibMonster( void )
	{
		if ( this->m_pHandler->MethodExists( "void GibMonster()" ) )
			this->m_pHandler->CallMethod( false, "void GibMonster()" );
		else
			BASE::GibMonster();
	}

	virtual int ISoundMask( void )
	{
		if ( !this->m_pHandler->MethodExists( "int ISoundMask()" ) )
			return BASE::ISoundMask();

		int iMask = 0;

		this->m_pHandler->CallMethod( &iMask, false, "int ISoundMask()" );
		return iMask;
	}

	virtual int IgnoreConditions( void )
	{
		if ( !this->m_pHandler->MethodExists( "int IgnoreConditions()" ) )
			return BASE::IgnoreConditions();

		int iConditions = 0;

		this->m_pHandler->CallMethod( &iConditions, false, "int IgnoreConditions()" );
		return iConditions;
	}

	virtual Schedule_t *GetSchedule( void )
	{
		if ( !this->m_pHandler->MethodExists( "Schedule@ GetSchedule()" ) )
			return BASE::GetSchedule();

		Schedule_t *pSchedule = NULL;

		this->m_pHandler->CallMethod( &pSchedule, false, "Schedule@ GetSchedule()" );
		return pSchedule;
	}

	virtual Schedule_t *GetScheduleOfType( int Type )
	{
		if ( !this->m_pHandler->MethodExists( "Schedule@ GetScheduleOfType(int)" ) )
			return BASE::GetScheduleOfType( Type );

		Schedule_t *pSchedule = NULL;

		this->m_pHandler->CallMethod( &pSchedule, false, "Schedule@ GetScheduleOfType(int)", Type );
		return pSchedule;
	}

	virtual Schedule_t *ScheduleFromName( const char *pName )
	{
		if ( !this->m_pHandler->MethodExists( "Schedule@ ScheduleFromName(const string& in szName)" ) )
			return BASE::ScheduleFromName( pName );

		CString name( pName );
		Schedule_t *pSchedule = NULL;

		this->m_pHandler->CallMethod( &pSchedule, false, "Schedule@ ScheduleFromName(const string& in szName)", &name );
		return pSchedule;
	}

	virtual void StartTask( Task_t *pTask )
	{
		if ( this->m_pHandler->MethodExists( "void StartTask(Task@)" ) )
			this->m_pHandler->CallMethod( false, "void StartTask(Task@)", pTask );
		else
			BASE::StartTask( pTask );
	}

	virtual void RunTask( Task_t *pTask )
	{
		if ( this->m_pHandler->MethodExists( "void RunTask(Task@)" ) )
			this->m_pHandler->CallMethod( false, "void RunTask(Task@)", pTask );
		else
			BASE::RunTask( pTask );
	}

	virtual void RunAI( void )
	{
		if ( this->m_pHandler->MethodExists( "void RunAI()" ) )
			this->m_pHandler->CallMethod( false, "void RunAI()" );
		else
			BASE::RunAI();
	}

	virtual void HandleAnimEvent( MonsterEvent_t *pEvent )
	{
		if ( this->m_pHandler->MethodExists( "void HandleAnimEvent(MonsterEvent@)" ) )
			this->m_pHandler->CallMethod( false, "void HandleAnimEvent(MonsterEvent@)", pEvent );
		else
			BASE::HandleAnimEvent( pEvent );
	}

	virtual BOOL CheckRangeAttack1( float flDot, float flDist )
	{
		return CheckAttack( "bool CheckRangeAttack1(float, float)", flDot, flDist, &BASE::CheckRangeAttack1 );
	}

	virtual BOOL CheckRangeAttack1_Move( float flDot, float flDist )
	{
		return CheckAttack( "bool CheckRangeAttack1_Move(float, float)", flDot, flDist, &BASE::CheckRangeAttack1_Move );
	}

	virtual BOOL CheckRangeAttack2( float flDot, float flDist )
	{
		return CheckAttack( "bool CheckRangeAttack2(float, float)", flDot, flDist, &BASE::CheckRangeAttack2 );
	}

	virtual BOOL CheckRangeAttack2_Move( float flDot, float flDist )
	{
		return CheckAttack( "bool CheckRangeAttack2_Move(float, float)", flDot, flDist, &BASE::CheckRangeAttack2_Move );
	}

	virtual BOOL CheckMeleeAttack1( float flDot, float flDist )
	{
		return CheckAttack( "bool CheckMeleeAttack1(float, float)", flDot, flDist, &BASE::CheckMeleeAttack1 );
	}

	virtual BOOL CheckMeleeAttack1_Move( float flDot, float flDist )
	{
		return CheckAttack( "bool CheckMeleeAttack1_Move(float, float)", flDot, flDist, &BASE::CheckMeleeAttack1_Move );
	}

	virtual BOOL CheckMeleeAttack2( float flDot, float flDist )
	{
		return CheckAttack( "bool CheckMeleeAttack2(float, float)", flDot, flDist, &BASE::CheckMeleeAttack2 );
	}

	virtual BOOL CheckMeleeAttack2_Move( float flDot, float flDist )
	{
		return CheckAttack( "bool CheckMeleeAttack2_Move(float, float)", flDot, flDist, &BASE::CheckMeleeAttack2_Move );
	}

private:
	BOOL CheckAttack( const char *pszDeclaration, float flDot, float flDist, BOOL ( BASE::*pfnBase )( float, float ) )
	{
		if ( !this->m_pHandler->MethodExists( pszDeclaration ) )
			return ( this->*pfnBase )( flDot, flDist );

		bool fResult = false;

		this->m_pHandler->CallMethod( &fResult, false, pszDeclaration, flDot, flDist );
		return fResult;
	}
};

template <class BASE>
class CBaseCustomTankEntity : public CBaseCustomBaseEntity<BASE>
{
public:
	virtual void Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
	{
		if ( this->m_pHandler->MethodExists( "void Fire(const Vector& in, const Vector& in, entvars_t@)" ) )
			this->m_pHandler->CallMethod( false, "void Fire(const Vector& in, const Vector& in, entvars_t@)", &barrelEnd, &forward, pevAttacker );
		else
			BASE::Fire( barrelEnd, forward, pevAttacker );
	}

	virtual Vector UpdateTargetPosition( CBaseEntity *pTarget )
	{
		if ( !this->m_pHandler->MethodExists( "Vector UpdateTargetPosition(CBaseEntity@)" ) )
			return BASE::UpdateTargetPosition( pTarget );

		Vector vecPosition = g_vecZero;

		this->m_pHandler->CallMethod( &vecPosition, false, "Vector UpdateTargetPosition(CBaseEntity@)", pTarget );
		return vecPosition;
	}

	virtual int BulletCount( void )
	{
		if ( !this->m_pHandler->MethodExists( "int BulletCount()" ) )
			return BASE::BulletCount();

		int iCount = 0;

		this->m_pHandler->CallMethod( &iCount, false, "int BulletCount()" );
		return iCount;
	}
};

template <class BASE>
class CBaseCustomPlayerItemEntity : public CBaseCustomBaseEntity<BASE>
{
public:
	virtual void Materialize( void )
	{
		if ( this->m_pHandler->MethodExists( "void Materialize()" ) )
			this->m_pHandler->CallMethod( false, "void Materialize()" );
		else
			BASE::Materialize();
	}

	// the item must have been registered with g_ItemRegistry
	virtual bool OnScriptClassReady( void );

	virtual bool GetItemInfo( ItemInfo *p );

	virtual bool AddToPlayer( CBasePlayer *pPlayer )
	{
		if ( !this->m_pHandler->MethodExists( "bool AddToPlayer(CBasePlayer@)" ) )
			return BASE::AddToPlayer( pPlayer );

		bool fResult = false;

		this->m_pHandler->CallMethod( &fResult, false, "bool AddToPlayer(CBasePlayer@)", pPlayer );
		return fResult;
	}

	virtual bool AddDuplicate( CBasePlayerItem *pItem )
	{
		if ( !this->m_pHandler->MethodExists( "bool AddDuplicate(CBasePlayerItem@)" ) )
			return BASE::AddDuplicate( pItem );

		bool fResult = false;

		this->m_pHandler->CallMethod( &fResult, false, "bool AddDuplicate(CBasePlayerItem@)", pItem );
		return fResult;
	}

	virtual bool UpdateClientData( CBasePlayer *pPlayer )
	{
		if ( !this->m_pHandler->MethodExists( "bool UpdateClientData(CBasePlayer@)" ) )
			return BASE::UpdateClientData( pPlayer );

		bool fResult = false;

		this->m_pHandler->CallMethod( &fResult, false, "bool UpdateClientData(CBasePlayer@)", pPlayer );
		return fResult;
	}

	virtual bool CanHaveDuplicates( void )
	{
		if ( !this->m_pHandler->MethodExists( "bool CanHaveDuplicates()" ) )
			return BASE::CanHaveDuplicates();

		bool fResult = false;

		this->m_pHandler->CallMethod( &fResult, false, "bool CanHaveDuplicates()" );
		return fResult;
	}

	virtual bool CanDeploy( void )
	{
		if ( !this->m_pHandler->MethodExists( "bool CanDeploy()" ) )
			return BASE::CanDeploy();

		bool fResult = false;

		this->m_pHandler->CallMethod( &fResult, false, "bool CanDeploy()" );
		return fResult;
	}

	virtual bool Deploy( void )
	{
		if ( !this->m_pHandler->MethodExists( "bool Deploy()" ) )
			return BASE::Deploy();

		bool fResult = false;

		this->m_pHandler->CallMethod( &fResult, false, "bool Deploy()" );
		return fResult;
	}

	virtual bool CanHolster( void )
	{
		if ( !this->m_pHandler->MethodExists( "bool CanHolster()" ) )
			return BASE::CanHolster();

		bool fResult = false;

		this->m_pHandler->CallMethod( &fResult, false, "bool CanHolster()" );
		return fResult;
	}

	virtual void Holster( int skiplocal = 0 )
	{
		if ( this->m_pHandler->MethodExists( "void Holster(int)" ) )
			this->m_pHandler->CallMethod( false, "void Holster(int)", skiplocal );
		else
			BASE::Holster( skiplocal );
	}

	virtual void UpdateItemInfo( void )
	{
		if ( this->m_pHandler->MethodExists( "void UpdateItemInfo()" ) )
			this->m_pHandler->CallMethod( false, "void UpdateItemInfo()" );
	}

	virtual void ItemPreFrame( void )
	{
		if ( this->m_pHandler->MethodExists( "void ItemPreFrame()" ) )
			this->m_pHandler->CallMethod( false, "void ItemPreFrame()" );
	}

	virtual void ItemPostFrame( void )
	{
		if ( this->m_pHandler->MethodExists( "void ItemPostFrame()" ) )
			this->m_pHandler->CallMethod( false, "void ItemPostFrame()" );
		else
			BASE::ItemPostFrame();
	}

	virtual void InactiveItemPreFrame( void )
	{
		if ( this->m_pHandler->MethodExists( "void InactiveItemPreFrame()" ) )
			this->m_pHandler->CallMethod( false, "void InactiveItemPreFrame()" );
	}

	virtual void InactiveItemPostFrame( void )
	{
		if ( this->m_pHandler->MethodExists( "void InactiveItemPostFrame()" ) )
			this->m_pHandler->CallMethod( false, "void InactiveItemPostFrame()" );
	}

	virtual CBasePlayerItem *DropItem( void )
	{
		if ( !this->m_pHandler->MethodExists( "CBasePlayerItem@ DropItem()" ) )
			return BASE::DropItem();

		CBasePlayerItem *pItem = NULL;

		this->m_pHandler->CallMethod( &pItem, false, "CBasePlayerItem@ DropItem()" );
		return pItem;
	}

	virtual void Kill( void )
	{
		if ( this->m_pHandler->MethodExists( "void Kill()" ) )
			this->m_pHandler->CallMethod( false, "void Kill()" );
		else
			BASE::Kill();
	}

	virtual void AttachToPlayer( CBasePlayer *pPlayer )
	{
		if ( this->m_pHandler->MethodExists( "void AttachToPlayer(CBasePlayer@)" ) )
			this->m_pHandler->CallMethod( false, "void AttachToPlayer(CBasePlayer@)", pPlayer );
		else
			BASE::AttachToPlayer( pPlayer );
	}

	virtual int PrimaryAmmoIndex( void )
	{
		if ( !this->m_pHandler->MethodExists( "int PrimaryAmmoIndex()" ) )
			return BASE::PrimaryAmmoIndex();

		int iIndex = 0;

		this->m_pHandler->CallMethod( &iIndex, false, "int PrimaryAmmoIndex()" );
		return iIndex;
	}

	virtual int SecondaryAmmoIndex( void )
	{
		if ( !this->m_pHandler->MethodExists( "int SecondaryAmmoIndex()" ) )
			return BASE::SecondaryAmmoIndex();

		int iIndex = 0;

		this->m_pHandler->CallMethod( &iIndex, false, "int SecondaryAmmoIndex()" );
		return iIndex;
	}

	virtual float GetRespawnTime( void )
	{
		if ( !this->m_pHandler->MethodExists( "float GetRespawnTime()" ) )
			return BASE::GetRespawnTime();

		float flTime = 0;

		this->m_pHandler->CallMethod( &flTime, false, "float GetRespawnTime()" );
		return flTime;
	}
};

template <class BASE>
class CBaseCustomPlayerWeaponEntity : public CBaseCustomPlayerItemEntity<BASE>
{
public:
	virtual int AddWeapon( void )
	{
		if ( !this->m_pHandler->MethodExists( "bool AddWeapon()" ) )
			return BASE::AddWeapon();

		bool fResult = false;

		this->m_pHandler->CallMethod( &fResult, false, "bool AddWeapon()" );
		return fResult;
	}

	virtual int ExtractAmmo( CBasePlayerWeapon *pWeapon )
	{
		if ( !this->m_pHandler->MethodExists( "bool ExtractAmmo(CBasePlayerWeapon@)" ) )
			return BASE::ExtractAmmo( pWeapon );

		bool fResult = false;

		this->m_pHandler->CallMethod( &fResult, false, "bool ExtractAmmo(CBasePlayerWeapon@)", pWeapon );
		return fResult;
	}

	virtual int ExtractClipAmmo( CBasePlayerWeapon *pWeapon )
	{
		if ( !this->m_pHandler->MethodExists( "bool ExtractClipAmmo(CBasePlayerWeapon@)" ) )
			return BASE::ExtractClipAmmo( pWeapon );

		bool fResult = false;

		this->m_pHandler->CallMethod( &fResult, false, "bool ExtractClipAmmo(CBasePlayerWeapon@)", pWeapon );
		return fResult;
	}

	virtual bool PlayEmptySound( void )
	{
		if ( !this->m_pHandler->MethodExists( "bool PlayEmptySound()" ) )
			return BASE::PlayEmptySound();

		bool fResult = false;

		this->m_pHandler->CallMethod( &fResult, false, "bool PlayEmptySound()" );
		return fResult;
	}

	virtual void ResetEmptySound( void )
	{
		if ( this->m_pHandler->MethodExists( "void ResetEmptySound()" ) )
			this->m_pHandler->CallMethod( false, "void ResetEmptySound()" );
		else
			BASE::ResetEmptySound();
	}

	virtual void SendWeaponAnim( int iAnim, int skiplocal = 1, int body = 0 )
	{
		if ( this->m_pHandler->MethodExists( "void SendWeaponAnim(int, int, int)" ) )
			this->m_pHandler->CallMethod( false, "void SendWeaponAnim(int, int, int)", iAnim, skiplocal, body );
		else
			BASE::SendWeaponAnim( iAnim, skiplocal, body );
	}

	virtual Vector BulletAccuracy( const Vector &vecMoving, const Vector &vecStanding, const Vector &vecCrouched )
	{
		if ( !this->m_pHandler->MethodExists( "Vector BulletAccuracy(const Vector& in, const Vector& in, const Vector& in)" ) )
			return BASE::BulletAccuracy( vecMoving, vecStanding, vecCrouched );

		Vector vecAccuracy = g_vecZero;

		this->m_pHandler->CallMethod( &vecAccuracy, false, "Vector BulletAccuracy(const Vector& in, const Vector& in, const Vector& in)",
		                              &vecMoving, &vecStanding, &vecCrouched );
		return vecAccuracy;
	}

	virtual bool IsUseable( void )
	{
		if ( !this->m_pHandler->MethodExists( "bool IsUseable()" ) )
			return BASE::IsUseable();

		bool fResult = false;

		this->m_pHandler->CallMethod( &fResult, false, "bool IsUseable()" );
		return fResult;
	}

	virtual void PrimaryAttack( void )
	{
		if ( this->m_pHandler->MethodExists( "void PrimaryAttack()" ) )
			this->m_pHandler->CallMethod( false, "void PrimaryAttack()" );
	}

	virtual void SecondaryAttack( void )
	{
		if ( this->m_pHandler->MethodExists( "void SecondaryAttack()" ) )
			this->m_pHandler->CallMethod( false, "void SecondaryAttack()" );
	}

	virtual void TertiaryAttack( void )
	{
		if ( this->m_pHandler->MethodExists( "void TertiaryAttack()" ) )
			this->m_pHandler->CallMethod( false, "void TertiaryAttack()" );
	}

	virtual void Reload( void )
	{
		if ( this->m_pHandler->MethodExists( "void Reload()" ) )
			this->m_pHandler->CallMethod( false, "void Reload()" );
		else
			BASE::Reload();
	}

	virtual void FinishReload( void )
	{
		if ( this->m_pHandler->MethodExists( "void FinishReload()" ) )
			this->m_pHandler->CallMethod( false, "void FinishReload()" );
		else
			BASE::FinishReload();
	}

	virtual bool ShouldReload( void )
	{
		if ( !this->m_pHandler->MethodExists( "bool ShouldReload()" ) )
			return BASE::ShouldReload();

		bool fResult = false;

		this->m_pHandler->CallMethod( &fResult, false, "bool ShouldReload()" );
		return fResult;
	}

	virtual void WeaponIdle( void )
	{
		if ( this->m_pHandler->MethodExists( "void WeaponIdle()" ) )
			this->m_pHandler->CallMethod( false, "void WeaponIdle()" );
	}

	virtual void RetireWeapon( void )
	{
		if ( this->m_pHandler->MethodExists( "void RetireWeapon()" ) )
			this->m_pHandler->CallMethod( false, "void RetireWeapon()" );
		else
			BASE::RetireWeapon();
	}

	virtual bool ShouldWeaponIdle( void )
	{
		if ( !this->m_pHandler->MethodExists( "bool ShouldWeaponIdle()" ) )
			return BASE::ShouldWeaponIdle();

		bool fResult = false;

		this->m_pHandler->CallMethod( &fResult, false, "bool ShouldWeaponIdle()" );
		return fResult;
	}

	virtual bool UseDecrement( void )
	{
		if ( !this->m_pHandler->MethodExists( "bool UseDecrement()" ) )
			return BASE::UseDecrement();

		bool fResult = false;

		this->m_pHandler->CallMethod( &fResult, false, "bool UseDecrement()" );
		return fResult;
	}

	virtual void BurstSupplement( void )
	{
		if ( this->m_pHandler->MethodExists( "void BurstSupplement()" ) )
			this->m_pHandler->CallMethod( false, "void BurstSupplement()" );
		else
			BASE::BurstSupplement();
	}
};

template <class BASE>
class CBaseCustomItemEntity : public CBaseCustomBaseEntity<BASE>
{
public:
	virtual BOOL MyTouch( CBasePlayer *pPlayer )
	{
		if ( !this->m_pHandler->MethodExists( "bool MyTouch(CBasePlayer@)" ) )
			return BASE::MyTouch( pPlayer );

		bool fResult = false;

		this->m_pHandler->CallMethod( &fResult, false, "bool MyTouch(CBasePlayer@)", pPlayer );
		return fResult;
	}
};

template <class BASE>
class CBaseCustomPlayerAmmoEntity : public CBaseCustomBaseEntity<BASE>
{
public:
	virtual BOOL AddAmmo( CBaseEntity *pOther )
	{
		if ( !this->m_pHandler->MethodExists( "bool AddAmmo(CBaseEntity@)" ) )
			return BASE::AddAmmo( pOther );

		bool fResult = false;

		this->m_pHandler->CallMethod( &fResult, false, "bool AddAmmo(CBaseEntity@)", pOther );
		return fResult;
	}
};

// Create a script entity on pev
// Return NULL and free pev on failure
CBaseEntity *CreateCustomEntity( const char *pszClassName, entvars_t *pev );
edict_t *ASCreateCustomEntity( const char *pszClassName );
edict_t *ASCreateCustomEntity( string_t iszClassName );

// For func_tank.cpp, which keeps CFuncTank private
CBaseEntity *Create_CCustomTankEntity( entvars_t *pev );

#endif
