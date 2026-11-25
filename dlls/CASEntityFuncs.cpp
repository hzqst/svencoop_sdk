#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "CASEntityFuncs.h"
#include "ICustomEntity.h"
#include "CASObjectInstance.h"
#include "CASLog.h"
#include "CScriptDictionary.h"
#include "displacer.h"
#include "explode.h"
#include "../external/angelscript/sdk/add_on/scriptarray/scriptarray.h"
#include <angelscript.h>

void EntvarsKeyvalue( entvars_t *vars, KeyValueData *keyValue );

CASEntityFuncs::CASEntityFuncs()
{
}

CASEntityFuncs::~CASEntityFuncs()
{
}

CBaseEntity *CASEntityFuncs::CreateEntity( const CString &className, CScriptDictionary *dictionary, bool spawn )
{
	return CreateEntity( className, dictionary, spawn, CreateNamedEntity_Intercept );
}

CBaseEntity *CASEntityFuncs::CreateEntity( const CString &className, CScriptDictionary *dictionary, bool spawn,
                                           edict_t *( *factory )( int ) )
{
	edict_t *edict = factory( AllocStringLocal( className.Data() ) );
	CBaseEntity *entity = Instance( edict );
	bool initialized = false;

	if ( entity )
	{
		initialized = InitializeEntity( className, entity, dictionary );

		if ( initialized && spawn )
		{
			::DispatchSpawn( edict );
			entity = Instance( edict );
		}
	}

	if ( dictionary )
		dictionary->Release();

	if ( !initialized && edict )
		REMOVE_ENTITY( edict );

	return initialized ? entity : NULL;
}

bool CASEntityFuncs::InitializeEntity( const CString &className, CBaseEntity *entity, CScriptDictionary *dictionary )
{
	if ( !entity )
		return false;

	edict_t *edict = entity->edict();
	KeyValueData keyValue = { className.Data(), "classname", className.Data(), FALSE };

	::DispatchKeyValue( edict, &keyValue );

	if ( !dictionary )
		return true;

	asITypeInfo *stringType = CASServerManager::GetInstance()->GetTypeInfoByName( "string" );

	if ( !stringType )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT,
		                              "CASEntityFuncs::CreateCustomEntity: failed to get string type!\n" );
		return false;
	}

	if ( dictionary->IsEmpty() )
		return true;

	CString value;
	int stringTypeId = stringType->GetTypeId();

	for ( CScriptDictionary::CIterator iterator = dictionary->begin(); iterator != dictionary->end(); iterator++ )
	{
		if ( iterator.GetValue( &value, stringTypeId ) )
		{
			keyValue.szKeyName = iterator.GetKey().Data();
			keyValue.szValue = value.Data();
			keyValue.fHandled = FALSE;
			::DispatchKeyValue( edict, &keyValue );
		}
		else
		{
			CASLog::GetInstance()->Warning( AS_LOG_DEFAULT,
			                                "CASEntityFuncs::CreateCustomEntity: failed to retrieve value for key '%s' from dictionary!\n",
			                                iterator.GetKey().Data() );
		}
	}

	return true;
}

CBaseEntity *CASEntityFuncs::Create( const CString &className, const Vector &origin, const Vector &angles,
                                     bool createAndDontSpawn, edict_t *owner )
{
	return CBaseEntity::Create( STRING( AllocStringLocal( className.Data() ) ), origin, angles, createAndDontSpawn, owner );
}

void CASEntityFuncs::CreateDecal( const CString &decalName, const Vector &origin, const CString &targetName )
{
	edict_t *edict = CREATE_NAMED_ENTITY( MAKE_STRING( "infodecal" ) );

	if ( !edict )
		return;

	CBaseEntity *entity = Instance( edict );
	KeyValueData keyValue = { "infodecal", "texture", decalName.Data(), FALSE };

	edict->v.origin = origin;
	entity->KeyValue( &keyValue );

	if ( targetName.Length() )
	{
		keyValue.szKeyName = "targetname";
		keyValue.szValue = targetName.Data();
		keyValue.fHandled = FALSE;
		::DispatchKeyValue( edict, &keyValue );
	}

	entity->Spawn();
}

CSprite *CASEntityFuncs::CreateSprite( const CString &spriteName, const Vector &origin, bool animated, float frameRate )
{
	return CSprite::SpriteCreate( STRING( AllocStringLocal( spriteName.Data() ) ), origin, animated, frameRate );
}

CBeam *CASEntityFuncs::CreateBeam( const CString &spriteName, int width )
{
	return CBeam::BeamCreate( STRING( AllocStringLocal( spriteName.Data() ) ), width );
}

void CASEntityFuncs::CreateExplosion( const Vector &center, const Vector &angles, edict_t *owner, int magnitude, bool doDamage )
{
	ExplosionCreate( center, angles, owner, magnitude, doDamage );
}

CBaseEntity *CASEntityFuncs::CreateDisplacerPortal( const Vector &origin, const Vector &velocity, edict_t *owner,
                                                    float damage, float radius )
{
	return CDisplacerPortal::Create( origin, velocity, owner, damage, radius );
}

CBaseEntity *CASEntityFuncs::CreateRPGRocket( const Vector &origin, const Vector &angles, edict_t *owner )
{
	return CBaseEntity::Create( "rpg_rocket", origin, angles, false, owner );
}

CBaseEntity *CASEntityFuncs::CreateRPGRocket( const Vector &origin, const Vector &angles, const Vector &velocity, edict_t *owner )
{
	CBaseEntity *rocket = CBaseEntity::Create( "rpg_rocket", origin, angles, false, owner );

	if ( rocket )
		rocket->pev->velocity = velocity;

	return rocket;
}

CGib *CASEntityFuncs::CreateGib( const Vector &origin, const Vector &angles )
{
	CGib *gib = GetClassPtr( (CGib *) NULL );

	if ( !gib )
		return NULL;

	gib->pev->origin = origin;
	gib->pev->angles = angles;
	return gib;
}

CGrenade *CASEntityFuncs::ShootTimed( entvars_t *owner, const Vector &start, const Vector &velocity, float time )
{
	return CGrenade::ShootTimed( owner, start, velocity, time );
}

CGrenade *CASEntityFuncs::ShootContact( entvars_t *owner, const Vector &start, const Vector &velocity )
{
	return CGrenade::ShootContact( owner, start, velocity );
}

CGrenade *CASEntityFuncs::ShootMortar( entvars_t *owner, const Vector &start, const Vector &velocity )
{
	return CGrenade::ShootMortar( owner, start, velocity );
}

CGrenade *CASEntityFuncs::ShootBananaCluster( entvars_t *owner, const Vector &start, const Vector &velocity )
{
	return CGrenade::ShootBananaCluster( owner, start, velocity );
}

void CASEntityFuncs::UseSatchelCharges( entvars_t *owner, CGrenade::SATCHELCODE code )
{
	CGrenade::UseSatchelCharges( owner, code );
}

void CASEntityFuncs::SpawnHeadGib( entvars_t *victim )
{
	CGib::SpawnHeadGib( victim );
}

void CASEntityFuncs::SpawnRandomGibs( entvars_t *victim, int count, int human )
{
	CGib::SpawnRandomGibs( victim, count, human );
}

void CASEntityFuncs::SpawnStickyGibs( entvars_t *victim, const Vector &origin, int count )
{
	CGib::SpawnStickyGibs( victim, origin, count );
}

void CASEntityFuncs::SetOrigin( CBaseEntity *entity, const Vector &origin )
{
	if ( entity )
		g_engfuncs.pfnSetOrigin( entity->edict(), origin );
}

void CASEntityFuncs::SetModel( CBaseEntity *entity, const CString &model )
{
	if ( entity )
		SetModel_Intercept( entity->edict(), STRING( AllocStringLocal( model.Data() ) ) );
}

void CASEntityFuncs::SetSize( entvars_t *vars, const Vector &mins, const Vector &maxs )
{
	UTIL_SetSize( vars, mins, maxs );
}

CBaseEntity *CASEntityFuncs::FindEntityInSphere( CBaseEntity *start, const Vector &center, float radius,
                                                 const CString &value, const CString &key )
{
	return UTIL_FindEntityInSphere( start, center, radius, value.Data(), key.Data() );
}

CBaseEntity *CASEntityFuncs::FindEntityByString( CBaseEntity *start, const CString &key, const CString &value )
{
	return UTIL_FindEntityByString( start, key.Data(), value.Data() );
}

CBaseEntity *CASEntityFuncs::FindEntityByClassname( CBaseEntity *start, const CString &name )
{
	return UTIL_FindEntityByClassname( start, name.Data() );
}

CBaseEntity *CASEntityFuncs::FindEntityByTargetname( CBaseEntity *start, const CString &name )
{
	return UTIL_FindEntityByTargetname( start, name.Data() );
}

CBaseEntity *CASEntityFuncs::FindEntityGeneric( const CString &name, const Vector &source, float radius )
{
	return UTIL_FindEntityGeneric( name.Data(), const_cast<Vector &>( source ), radius );
}

int CASEntityFuncs::MonstersInSphere( CScriptArray *array, const Vector &center, float radius )
{
	if ( !array )
		return 0;

	CASServerManager *manager = CASServerManager::GetInstance();
	int count = 0;

	if ( manager->m_pEngine->GetTypeInfoById( array->GetElementTypeId() ) != manager->GetTypeInfoByName( "CBaseEntity" ) )
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "CEntityFuncs::MonstersInSphere: array element type must be \"CBaseEntity\"!\n" );
	else
		count = UTIL_MonstersInSphere( (CBaseEntity **) array->GetBuffer(), array->GetSize(), center, radius );

	array->Release();
	return count;
}

int CASEntityFuncs::EntitiesInBox( CScriptArray *array, const Vector &mins, const Vector &maxs, int flagMask )
{
	if ( !array )
		return 0;

	CASServerManager *manager = CASServerManager::GetInstance();
	int count = 0;

	if ( manager->m_pEngine->GetTypeInfoById( array->GetElementTypeId() ) != manager->GetTypeInfoByName( "CBaseEntity" ) )
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "CEntityFuncs::EntitiesInBox: array element type must be \"CBaseEntity\"!\n" );
	else
		count = UTIL_EntitiesInBox( (CBaseEntity **) array->GetBuffer(), array->GetSize(), mins, maxs, flagMask );

	array->Release();
	return count;
}

int CASEntityFuncs::TargetsInBox( CScriptArray *array, const Vector &mins, const Vector &maxs )
{
	if ( !array )
		return 0;

	CASServerManager *manager = CASServerManager::GetInstance();
	int count = 0;

	if ( manager->m_pEngine->GetTypeInfoById( array->GetElementTypeId() ) != manager->GetTypeInfoByName( "CBaseEntity" ) )
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "CEntityFuncs::TargetsInBox: array element type must be \"CBaseEntity\"!\n" );
	else
		count = UTIL_TargetsInBox( (CBaseEntity **) array->GetBuffer(), array->GetSize(), mins, maxs );

	array->Release();
	return count;
}

int CASEntityFuncs::BrushEntsInBox( CScriptArray *array, const Vector &mins, const Vector &maxs )
{
	if ( !array )
		return 0;

	CASServerManager *manager = CASServerManager::GetInstance();
	int count = 0;

	if ( manager->m_pEngine->GetTypeInfoById( array->GetElementTypeId() ) != manager->GetTypeInfoByName( "CBaseEntity" ) )
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "CEntityFuncs::BrushEntsInBox: array element type must be \"CBaseEntity\"!\n" );
	else
		count = UTIL_BrushEntsInBox( (CBaseEntity **) array->GetBuffer(), array->GetSize(), mins, maxs );

	array->Release();
	return count;
}

int CASEntityFuncs::DispatchSpawn( edict_t *entity )
{
	return ::DispatchSpawn( entity );
}

bool CASEntityFuncs::DispatchKeyValue( edict_t *entity, const CString &key, const CString &value )
{
	KeyValueData keyValue = { STRING( entity->v.classname ), key.Data(), value.Data(), FALSE };

	::DispatchKeyValue( entity, &keyValue );
	return keyValue.fHandled != FALSE;
}

bool CASEntityFuncs::EntvarsKeyvalue( edict_t *entity, const CString &key, const CString &value )
{
	KeyValueData keyValue = { STRING( entity->v.classname ), key.Data(), value.Data(), FALSE };

	::EntvarsKeyvalue( VARS( entity ), &keyValue );
	return keyValue.fHandled != FALSE;
}

void CASEntityFuncs::DispatchObjectCollisionBox( edict_t *entity )
{
	DispatchObjectCollsionBox( entity );
}

CBaseEntity *CASEntityFuncs::Instance( edict_t *entity )
{
	if ( !entity || entity->free || !GET_PRIVATE( entity ) )
		return NULL;

	return (CBaseEntity *) GET_PRIVATE( entity );
}

CBaseEntity *CASEntityFuncs::Instance( entvars_t *vars )
{
	return Instance( vars ? vars->pContainingEntity : NULL );
}

CBaseEntity *CASEntityFuncs::Instance( int index )
{
	return Instance( INDEXENT( index ) );
}

void CASEntityFuncs::FireTargets( const CString &targetName, CBaseEntity *activator, CBaseEntity *caller,
                                  USE_TYPE useType, float value, float delay )
{
	if ( delay == 0 )
	{
		::FireTargets( targetName.Data(), activator, caller, useType, value );
		return;
	}

	CBaseDelay *entity = GetClassPtr( (CBaseDelay *) NULL );

	entity->pev->classname = MAKE_STRING( "DelayedUse" );
	entity->pev->nextthink = gpGlobals->time + delay;
	entity->SetThink( &CBaseDelay::CallDelayThink );
	entity->pev->button = useType;
	entity->pev->fuser1 = value;
	entity->m_flDelay = 0;
	entity->pev->target = AllocStringLocal( targetName.Data() );
	edict_t *owner = NULL;
	if ( activator && activator->IsPlayer() && activator->pev )
		owner = activator->pev->pContainingEntity;
	entity->pev->owner = owner;
}

bool CASEntityFuncs::IsMasterTriggered( const CString &master, CBaseEntity *activator )
{
	return UTIL_IsMasterTriggered( MAKE_STRING( master.Data() ), activator ) != FALSE;
}

void CASEntityFuncs::Remove( CBaseEntity *entity )
{
	UTIL_Remove( entity );
}

bool CASEntityFuncs::IsValidEntity( edict_t *entity )
{
	return UTIL_IsValidEntity( entity ) != FALSE;
}

int CASEntityFuncs::EntIndex( edict_t *entity )
{
	return ENTINDEX( entity );
}

edict_t *CASEntityFuncs::IndexEnt( int index )
{
	return INDEXENT( index );
}

void CASEntityFuncs::EjectBrass( const Vector &origin, const Vector &velocity, float rotation, int model, int soundType )
{
	::EjectBrass( origin, velocity, rotation, model, soundType );
}

void *CASEntityFuncs::CastToScriptClass( CBaseEntity *entity )
{
	if ( !entity )
		return NULL;

	ICustomEntity *customEntity = entity->MyCustomPointer();

	if ( !customEntity )
		return NULL;

	CASObjectInstance *instance = customEntity->GetScriptInstance();
	void *object = instance->m_pObject;

	CASServerManager::GetInstance()->m_pEngine->AddRefScriptObject( object, instance->m_pTypeInfo );
	return object;
}

CBaseEntity *CASEntityFuncs::RandomTargetname( const CString &targetName )
{
	return UTIL_RandomTargetname( targetName.Data() );
}

void CASEntityFuncs::PrecacheMaterialSounds( Materials material )
{
	CBreakable::MaterialSoundPrecache( material );
}
