#include "extdll.h"
#include "util.h"
#include "CASItemRegistry.h"
#include "CASDocumentation.h"
#include "CASLog.h"
#include "server_support.h"
#include "weapon_iteminfo.h"
#include <new>

void ConstructItemInfo( void *memory )
{
	memset( memory, 0, sizeof( ItemInfo ) );
}

void CopyConstructItemInfo( const ItemInfo &value, void *memory )
{
	new ( memory ) ItemInfo( value );
}

void DestructItemInfo( ItemInfo *value )
{
	value->~ItemInfo();
}

ItemInfo &ItemInfo_opAssign( const ItemInfo &value, ItemInfo *item )
{
	return *item = value;
}

CString ItemInfo_GetName( ItemInfo *item )
{
	return CString( item->pszName ? item->pszName : "" );
}

CString ItemInfo_GetPrimaryAmmoName( ItemInfo *item )
{
	return CString( item->pszAmmo1 ? item->pszAmmo1 : "" );
}

CString ItemInfo_GetSecondaryAmmoName( ItemInfo *item )
{
	return CString( item->pszAmmo2 ? item->pszAmmo2 : "" );
}

void RegisterSCScriptPlayerItemEntityDependencies( CASDocumentation *documentation )
{
	documentation->RegisterObjectType( "Item info object.", "ItemInfo", sizeof( ItemInfo ), asOBJ_VALUE | asOBJ_APP_CLASS_CDK );
	documentation->RegisterObjectBehaviour( "Constructor", "ItemInfo", asBEHAVE_CONSTRUCT, "void ItemInfo()", asFUNCTION( ConstructItemInfo ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectBehaviour( "Constructor", "ItemInfo", asBEHAVE_CONSTRUCT, "void ItemInfo(const ItemInfo& in other)", asFUNCTION( CopyConstructItemInfo ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectBehaviour( "Destructor", "ItemInfo", asBEHAVE_DESTRUCT, "void DestructItemInfo()", asFUNCTION( DestructItemInfo ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod( "Assignment operator", "ItemInfo", "ItemInfo& opAssign(const ItemInfo& in other)", asFUNCTION( ItemInfo_opAssign ), asCALL_CDECL_OBJLAST );

	documentation->RegisterObjectProperty( "Item id.", "ItemInfo", "int iId", asOFFSET( ItemInfo, iId ) );
	documentation->RegisterObjectProperty( "Item HUD slot.", "ItemInfo", "int iSlot", asOFFSET( ItemInfo, iSlot ) );
	documentation->RegisterObjectProperty( "Item HUD slot position.", "ItemInfo", "int iPosition", asOFFSET( ItemInfo, iPosition ) );
	documentation->RegisterObjectProperty( "Item flags.", "ItemInfo", "int iFlags", asOFFSET( ItemInfo, iFlags ) );
	documentation->RegisterObjectProperty( "Item weight.\nThis value used to determine this weapon's importance in autoselection.", "ItemInfo", "int iWeight", asOFFSET( ItemInfo, iWeight ) );
	documentation->RegisterObjectProperty( "Item primary max ammo. -1 if this weapon does not use primary ammo.", "ItemInfo", "int iMaxAmmo1", asOFFSET( ItemInfo, iMaxAmmo1 ) );
	documentation->RegisterObjectProperty( "Amount of dropped primary ammo.", "ItemInfo", "int iAmmo1Drop", asOFFSET( ItemInfo, iAmmo1Drop ) );
	documentation->RegisterObjectProperty( "Item secondary max ammo. -1 if this weapon does not use secondary ammo.", "ItemInfo", "int iMaxAmmo2", asOFFSET( ItemInfo, iMaxAmmo2 ) );
	documentation->RegisterObjectProperty( "Amount of dropped secondary ammo.", "ItemInfo", "int iAmmo2Drop", asOFFSET( ItemInfo, iAmmo2Drop ) );
	documentation->RegisterObjectProperty( "Item primary ammo max clip contents.", "ItemInfo", "int iMaxClip", asOFFSET( ItemInfo, iMaxClip ) );

	documentation->RegisterObjectMethod( "Gets the name of this weapon.", "ItemInfo", "string szName() const", asFUNCTION( ItemInfo_GetName ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod( "Gets the primary ammo name of this weapon.", "ItemInfo", "string szAmmo1() const", asFUNCTION( ItemInfo_GetPrimaryAmmoName ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod( "Gets the secondary ammo name of this weapon.", "ItemInfo", "string szAmmo2() const", asFUNCTION( ItemInfo_GetSecondaryAmmoName ), asCALL_CDECL_OBJLAST );

	documentation->RegisterEnum( "Item flags used by ItemInfo::iFlags", "ItemFlag" );
	documentation->RegisterEnumValue( "Allow this weapon to be selected if it has no ammo.",
		"ItemFlag", "ITEM_FLAG_SELECTONEMPTY", 1 );
	documentation->RegisterEnumValue( "Don't automatically reload this weapon.",
		"ItemFlag", "ITEM_FLAG_NOAUTORELOAD", 2 );
	documentation->RegisterEnumValue( "Don't automatically switch away from this weapon when it runs out of ammo.",
		"ItemFlag", "ITEM_FLAG_NOAUTOSWITCHEMPTY", 4 );
	documentation->RegisterEnumValue( "If there are few entity slots left, this item will not respawn.",
		"ItemFlag", "ITEM_FLAG_LIMITINWORLD", 8 );
	documentation->RegisterEnumValue( "A player can totally exhaust their ammo supply and lose this weapon.",
		"ItemFlag", "ITEM_FLAG_EXHAUSTIBLE", 16 );
	documentation->RegisterEnumValue( "Use a special HUD ammo configuration unique to a dual wielded weapons.",
		"ItemFlag", "ITEM_FLAG_DUALWIELD", 32 );
	documentation->RegisterEnumValue( "Ignore weaponstay settings when respawning this weapon.",
		"ItemFlag", "ITEM_FLAG_IGNOREWEAPONSTAY", 64 );
	documentation->RegisterEnumValue(
		"This weapon should not be dropped straight away when the player dies, only if he is gibbed or if he respawned.",
		"ItemFlag", "ITEM_FLAG_ESSENTIAL", 128 );
}

void UTIL_PrecacheOtherWeapon( const char *szClassname );

CASItemRegistry::CASItemRegistry()
{
	memset( m_Items, 0, sizeof( m_Items ) );
	m_iItemCount = 0;
}

CASItemRegistry::~CASItemRegistry()
{
}

void CASItemRegistry::Shutdown( void )
{
	memset( m_Items, 0, sizeof( m_Items ) );
	m_iItemCount = 0;
}

const CASItemRegistry::ItemData_t *CASItemRegistry::GetItemInfoForName( const char *pszName ) const
{
	if ( !pszName || !*pszName )
		return NULL;

	for ( unsigned int i = 0; i < m_iItemCount; i++ )
	{
		if ( !stricmp( pszName, m_Items[i].szName ) )
			return &m_Items[i];
	}

	return NULL;
}

int CASItemRegistry::GetIdForName( const char *pszName ) const
{
	const ItemData_t *pItem = GetItemInfoForName( pszName );

	return pItem ? pItem->iId : 0;
}

int CASItemRegistry::GetIdForName( const CString &name ) const
{
	return GetIdForName( name.Data() );
}

int CASItemRegistry::RegisterItem( const CString &name, const CString &spriteDir, const CString &ammo1, const CString &ammo2,
                                   const CString &ammo1Entity, const CString &ammo2Entity )
{
	if ( !name.Length() )
		return 0;

	if ( m_iItemCount >= MAX_REGISTERED_ITEMS )
	{
		CASLog::GetInstance()->Warning( AS_LOG_DEFAULT, "CItemRegistry::RegisterItem: Cannot register item '%s', all item ids are in use!\n", name.Data() );
		return 0;
	}

	if ( name.Length() > MAX_ITEM_NAME_LENGTH )
	{
		CASLog::GetInstance()->Warning( AS_LOG_DEFAULT, "CItemRegistry::RegisterItem: Cannot register item '%s', item name is too long!\n", name.Data() );
		return 0;
	}

	if ( spriteDir.Length() > MAX_SPRITE_DIR_LENGTH )
	{
		CASLog::GetInstance()->Warning( AS_LOG_DEFAULT, "CItemRegistry::RegisterItem: Cannot register item '%s', sprite directory name is too long!\n", name.Data() );
		return 0;
	}

	int iId = GetIdForName( name );

	if ( iId )
		return iId;

	ItemData_t *pItem = &m_Items[m_iItemCount];

	UTIL_CopyString( pItem->szName, name.Data(), sizeof( pItem->szName ) );
	UTIL_CopyString( pItem->szSpriteDir, spriteDir.Data(), sizeof( pItem->szSpriteDir ) );
	UTIL_CopyString( pItem->szAmmo1, ammo1.Data(), sizeof( pItem->szAmmo1 ) );
	UTIL_CopyString( pItem->szAmmo2, ammo2.Data(), sizeof( pItem->szAmmo2 ) );
	UTIL_CopyString( pItem->szAmmo1Entity, ammo1Entity.Data(), sizeof( pItem->szAmmo1Entity ) );
	UTIL_CopyString( pItem->szAmmo2Entity, ammo2Entity.Data(), sizeof( pItem->szAmmo2Entity ) );
	pItem->iId = FIRST_ITEM_ID + m_iItemCount++;

	UTIL_PrecacheOtherWeapon( STRING( AllocStringLocal( name.Data() ) ) );

	return pItem->iId;
}

void RegisterSCScriptItemRegistry( CASDocumentation *documentation )
{
	asDWORD accessMask = documentation->SetDefaultAccessMask( AS_ACCESS_ALL );

	documentation->RegisterObjectType( "Item registry.", "CItemRegistry", 0, asOBJ_REF | asOBJ_NOCOUNT );
	documentation->RegisterObjectMethod(
	    "Gets the id associated with the given weapon name. Returns WEAPON_NONE if no id is associated with the given name.",
	    "CItemRegistry",
	    "int GetIdForName(const string& in szName) const",
	    asMETHODPR( CASItemRegistry, GetIdForName, ( const CString & ) const, int ),
	    asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "Registers the given entity name as an item and returns the id associated with it.\n"
	    "Returns WEAPON_NONE if no id could be assigned to it.",
	    "CItemRegistry",
	    "int RegisterItem(const string& in szName, const string& in szSpriteDir, const string& in szPrimaryAmmoName = \"\", "
	    "const string& in szSecondaryAmmoName = \"\", const string& in szPrimaryAmmoClass = \"\", const string& in szSecondaryAmmoClass = \"\")",
	    asMETHOD( CASItemRegistry, RegisterItem ),
	    asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "Registers the given entity name as a weapon and returns the id associated with it.\n"
	    "Returns WEAPON_NONE if no id could be assigned to it.",
	    "CItemRegistry",
	    "int RegisterWeapon(const string& in szName, const string& in szSpriteDir, const string& in szPrimaryAmmoName = \"\", "
	    "const string& in szSecondaryAmmoName = \"\", const string& in szPrimaryAmmoClass = \"\", const string& in szSecondaryAmmoClass = \"\")",
	    asMETHOD( CASItemRegistry, RegisterItem ),
	    asCALL_THISCALL );
	documentation->RegisterGlobalProperty( "Item registry global instance.", "CItemRegistry g_ItemRegistry", CASItemRegistry::GetInstance() );

	documentation->SetDefaultAccessMask( accessMask );
}
