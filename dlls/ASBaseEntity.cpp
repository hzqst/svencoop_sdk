#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "ASBaseEntity.h"
#include "CASServerManager.h"
#include "CASLog.h"
#include "../external/angelscript/sdk/add_on/scriptarray/scriptarray.h"

namespace
{
const Vector &Entity_GetOrigin( CBaseEntity *entity )
{
	return entity->pev->origin;
}

void Entity_SetOrigin( const Vector &origin, CBaseEntity *entity )
{
	g_engfuncs.pfnSetOrigin( entity->pev ? entity->pev->pContainingEntity : NULL, origin );
}

bool Entity_KeyValue( const CString &key, const CString &value, CBaseEntity *entity )
{
	KeyValueData keyValue = { entity->pev ? STRING( entity->pev->classname ) : "", key.Data(), value.Data(), FALSE };

	entity->KeyValue( &keyValue );
	return keyValue.fHandled != FALSE;
}

void Entity_TraceAttack( entvars_t *attacker, float damage, const Vector &direction,
                         TraceResult &trace, int damageType, CBaseEntity *entity )
{
	entity->TraceAttack( attacker, damage, direction, &trace, damageType );
}

void Entity_TraceBleed( float damage, const Vector &direction, TraceResult &trace, int damageType, CBaseEntity *entity )
{
	entity->TraceBleed( damage, direction, &trace, damageType );
}

int Entity_GiveAmmo( int amount, const CString &name, int maximum, bool fromWeapon, CBaseEntity *entity )
{
	return entity->GiveAmmo( amount, name.Data(), maximum, fromWeapon, NULL );
}

bool Entity_HasTarget( const CString &target, CBaseEntity *entity )
{
	return entity->HasTarget( MAKE_STRING( target.Data() ) ) != FALSE;
}

CString Entity_TeamID( CBaseEntity *entity )
{
	return CString( entity->TeamID() );
}

CString Entity_GetClassname( CBaseEntity *entity )
{
	return CString( entity->pev ? STRING( entity->pev->classname ) : "" );
}

CString Entity_GetClassificationTag( CBaseEntity *entity )
{
	return CString( entity->GetClassificationTag() );
}

CString Entity_GetClassificationName( CBaseEntity *entity )
{
	return CString( entity->GetClassificationName() );
}

CString Entity_GetTargetname( CBaseEntity *entity )
{
	return CString( entity->pev ? STRING( entity->pev->targetname ) : "" );
}

CString Entity_SOUNDREPLACEMENT_Find( const CString &filename, CBaseEntity *entity )
{
	return CString( entity->SOUNDREPLACEMENT_Find( filename.Data() ) );
}

int Entity_FindMonstersInWorld( CScriptArray *array, int flagMask, CBaseEntity *entity )
{
	asITypeInfo *expectedType = CASServerManager::GetInstance()->GetTypeInfoByName( "CBaseEntity" );
	asIScriptEngine *engine = array->GetArrayObjectType()->GetEngine();

	if ( expectedType != engine->GetTypeInfoById( array->GetElementTypeId() ) )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "CBaseEntity::FindMonstersInWorld: invalid element type for array! " );
		return 0;
	}

	// Don't release the input array handle
	return entity->FindMonstersInWorld( (CBaseEntity **) array->GetBuffer(), array->GetSize(), flagMask );
}

bool Entity_Equals( CBaseEntity *other, CBaseEntity *entity )
{
	return other == entity;
}
}

void RegisterSCScriptEntityInternal( CASDocumentation *documentation, const char *name )
{
	documentation->RegisterObjectProperty(
	    "Entity variables", name,
	    "entvars_t@ pev", asOFFSET( CBaseEntity, pev ) );

	documentation->RegisterObjectMethod(
	    "Gets the entity's (absolute) origin.", name,
	    "const Vector& GetOrigin() const",
	    asFUNCTION( Entity_GetOrigin ), asCALL_CDECL_OBJLAST );

	documentation->RegisterObjectMethod(
	    "Sets this entity's (absolute) origin", name,
	    "void SetOrigin(const Vector& in vecOrigin)",
	    asFUNCTION( Entity_SetOrigin ), asCALL_CDECL_OBJLAST );

	documentation->RegisterObjectMethod(
	    "Sets keyvalue data on this entity", name,
	    "bool KeyValue(const string& in szKeyName, const string& in szValue)",
	    asFUNCTION( Entity_KeyValue ), asCALL_CDECL_OBJLAST );

	documentation->RegisterObjectMethod(
	    "Traces an attack by the given entity to this entity.", name,
	    "void TraceAttack(entvars_t@ pevAttacker, float flDamage, const Vector& in vecDir, TraceResult& in traceResult, "
	    "int bitsDamageType)",
	    asFUNCTION( Entity_TraceAttack ), asCALL_CDECL_OBJLAST );

	documentation->RegisterObjectMethod(
	    "Traces entity bleed event.", name,
	    "void TraceBleed(float flDamage, const Vector& in vecDir, TraceResult& in traceResult, int bitsDamageType)",
	    asFUNCTION( Entity_TraceBleed ), asCALL_CDECL_OBJLAST );

	documentation->RegisterObjectMethod(
	    "Gives ammo to this entity.", name,
	    "int GiveAmmo(int iAmount, const string& in szName, int iMax, const bool fFromWeapon = true)",
	    asFUNCTION( Entity_GiveAmmo ), asCALL_CDECL_OBJLAST );

	documentation->RegisterObjectMethod(
	    "Returns whether this entity has the given target.", name,
	    "bool HasTarget(const string& in szTarget)",
	    asFUNCTION( Entity_HasTarget ), asCALL_CDECL_OBJLAST );

	documentation->RegisterObjectMethod(
	    "Returns this entity's team ID.", name,
	    "string TeamID()",
	    asFUNCTION( Entity_TeamID ), asCALL_CDECL_OBJLAST );

	documentation->RegisterObjectMethod(
	    "Returns this entity's class name.", name,
	    "string GetClassname() const",
	    asFUNCTION( Entity_GetClassname ), asCALL_CDECL_OBJLAST );

	documentation->RegisterObjectMethod(
	    "Returns this entity's classification tag.", name,
	    "string GetClassificationTag() const",
	    asFUNCTION( Entity_GetClassificationTag ), asCALL_CDECL_OBJLAST );

	documentation->RegisterObjectMethod(
	    "Returns this entity's classification name.", name,
	    "string GetClassificationName() const",
	    asFUNCTION( Entity_GetClassificationName ), asCALL_CDECL_OBJLAST );

	documentation->RegisterObjectMethod(
	    "Returns this entity's target name.", name,
	    "string GetTargetname() const",
	    asFUNCTION( Entity_GetTargetname ), asCALL_CDECL_OBJLAST );

	documentation->RegisterObjectMethod(
	    "Returns whether this entity is visible from the given position.", name,
	    "bool FVisibleFromPos(const Vector& in vecTarget, const Vector& in vecStart)",
	    asMETHOD( CBaseEntity, FVisibleFromPos ), asCALL_THISCALL );

	documentation->RegisterObjectMethod(
	    "Find sound replacement for the given file.", name,
	    "string SOUNDREPLACEMENT_Find(const string& in szFilename)",
	    asFUNCTION( Entity_SOUNDREPLACEMENT_Find ), asCALL_CDECL_OBJLAST );

	documentation->RegisterObjectMethod(
	    "Finds monsters in the world", name,
	    "int FindMonstersInWorld(array<CBaseEntity@>@ pArray, int flagMask)",
	    asFUNCTION( Entity_FindMonstersInWorld ), asCALL_CDECL_OBJLAST );

	documentation->RegisterObjectProperty(
	    "Whether this entity overrides the classification.", name,
	    "bool m_fOverrideClass", asOFFSET( CBaseEntity, m_fOverrideClass ) );

	documentation->RegisterObjectProperty(
	    "The overridden classification.", name,
	    "int m_iClassSelection", asOFFSET( CBaseEntity, m_iClassSelection ) );

	documentation->RegisterObjectMethod(
	    "Returns whether this entity is allied to players.", name,
	    "bool IsPlayerAlly() const",
	    asMETHOD( CBaseEntity, IsPlayerAlly ), asCALL_THISCALL );

	documentation->RegisterObjectMethod(
	    "Sets this entity's player ally status.", name,
	    "void SetPlayerAlly(bool fState)",
	    asMETHOD( CBaseEntity, SetPlayerAlly ), asCALL_THISCALL );

	documentation->RegisterObjectMethod(
	    "Sets this entity's player ally status.\nUse this only if you need the setting to be exactly what you give it "
	    "(e.g. when copying from another monster)",
	    name,
	    "void SetPlayerAllyDirect(bool fState)",
	    asMETHOD( CBaseEntity, SetPlayerAllyDirect ), asCALL_THISCALL );

	documentation->RegisterObjectProperty(
	    "Maximum fade wait time.", name,
	    "float m_flMaximumFadeWait", asOFFSET( CBaseEntity, m_flMaximumFadeWait ) );

	documentation->RegisterObjectProperty(
	    "Maximum fade wait time B.", name,
	    "float m_flMaximumFadeWaitB", asOFFSET( CBaseEntity, m_flMaximumFadeWaitB ) );

	documentation->RegisterObjectProperty(
	    "Whether fading can start.", name,
	    "bool m_fCanFadeStart", asOFFSET( CBaseEntity, m_fCanFadeStart ) );

	documentation->RegisterObjectProperty(
	    "Whether a custom model is used.", name,
	    "bool m_fCustomModel", asOFFSET( CBaseEntity, m_fCustomModel ) );

	documentation->RegisterObjectProperty(
	    "Last origin vector", name,
	    "Vector m_vecLastOrigin", asOFFSET( CBaseEntity, m_vecLastOrigin ) );

	documentation->RegisterObjectProperty(
	    "Target name out filter type.", name,
	    "string_t targetnameOutFilterType", asOFFSET( CBaseEntity, targetnameOutFilterType ) );

	documentation->RegisterObjectProperty(
	    "Class name out filter type.", name,
	    "string_t classnameOutFilterType", asOFFSET( CBaseEntity, classnameOutFilterType ) );

	documentation->RegisterObjectProperty(
	    "Target name in filter type.", name,
	    "string_t targetnameInFilterType", asOFFSET( CBaseEntity, targetnameInFilterType ) );

	documentation->RegisterObjectProperty(
	    "Class name in filter type.", name,
	    "string_t classnameInFilterType", asOFFSET( CBaseEntity, classnameInFilterType ) );

	documentation->RegisterObjectProperty(
	    "Original render model.", name,
	    "int m_iOriginalRenderMode", asOFFSET( CBaseEntity, m_iOriginalRenderMode ) );

	documentation->RegisterObjectProperty(
	    "Original render FX.", name,
	    "int m_iOriginalRenderFX", asOFFSET( CBaseEntity, m_iOriginalRenderFX ) );

	documentation->RegisterObjectProperty(
	    "Original render amount.", name,
	    "float m_flOriginalRenderAmount", asOFFSET( CBaseEntity, m_flOriginalRenderAmount ) );

	documentation->RegisterObjectProperty(
	    "Original render color.", name,
	    "Vector m_vecOriginalRenderColor", asOFFSET( CBaseEntity, m_vecOriginalRenderColor ) );

	documentation->RegisterObjectMethod(
	    "Compares 2 entities for equality.", name,
	    "bool opEquals(CBaseEntity@ pOther)",
	    asFUNCTION( Entity_Equals ), asCALL_CDECL_OBJLAST );
}
