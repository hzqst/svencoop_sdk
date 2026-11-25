#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "item_inventory.h"
#include "InventoryRules.h"

CBaseEntity *Inventory_ResolveHandle( EHANDLE &handle )
{
	edict_t *edict = handle.Get();

	return edict && !edict->free ? CBaseEntity::Instance( edict ) : NULL;
}

CInventoryRules::CInventoryRules( CBaseEntity *owner )
{
	m_hEntity = NULL;

	if ( owner && owner->pev )
	{
		edict_t *edict = owner->pev->pContainingEntity;

		if ( edict && !edict->free )
			m_hEntity.Set( edict );
	}

	m_szRequiresItemName = 0;
	m_szRequiresItemGroup = 0;
	m_iRequiresItemGroupNum = 0;
	m_szCantHaveItemName = 0;
	m_szCantHaveItemGroup = 0;
	m_iCantHaveItemGroupNum = 0;

	m_fIgnoreUseTriggers = false;
	m_szDropItemNameOnUse = 0;
	m_szDropItemGroupOnUse = 0;
	m_fIgnoreDropTriggers = false;
	m_szReturnItemNameOnUse = 0;
	m_szReturnItemGroupOnUse = 0;
	m_fIgnoreReturnTriggers = false;
	m_szDestroyItemNameOnUse = 0;
	m_szDestroyItemGroupOnUse = 0;
	m_fIgnoreDestroyTriggers = false;

	m_szTargetOnFail = 0;
}

bool CInventoryRules::KeyValue( KeyValueData *data )
{
	if ( FStrEq( data->szKeyName, "item_name_required" ) )
		m_szRequiresItemName = AllocStringLocal( data->szValue );
	else if ( FStrEq( data->szKeyName, "item_group_required" ) )
		m_szRequiresItemGroup = AllocStringLocal( data->szValue );
	else if ( FStrEq( data->szKeyName, "item_group_required_num" ) )
		m_iRequiresItemGroupNum = atoi( data->szValue );
	else if ( FStrEq( data->szKeyName, "item_name_canthave" ) )
		m_szCantHaveItemName = AllocStringLocal( data->szValue );
	else if ( FStrEq( data->szKeyName, "item_group_canthave" ) )
		m_szCantHaveItemGroup = AllocStringLocal( data->szValue );
	else if ( FStrEq( data->szKeyName, "item_group_canthave_num" ) )
		m_iCantHaveItemGroupNum = atoi( data->szValue );
	else if ( FStrEq( data->szKeyName, "pass_ignore_use_triggers" ) )
		m_fIgnoreUseTriggers = atoi( data->szValue ) >= 1;
	else if ( FStrEq( data->szKeyName, "pass_drop_item_name" ) )
		m_szDropItemNameOnUse = AllocStringLocal( data->szValue );
	else if ( FStrEq( data->szKeyName, "pass_drop_item_group" ) )
		m_szDropItemGroupOnUse = AllocStringLocal( data->szValue );
	else if ( FStrEq( data->szKeyName, "pass_ignore_drop_triggers" ) )
		m_fIgnoreDropTriggers = atoi( data->szValue ) >= 1;
	else if ( FStrEq( data->szKeyName, "pass_return_item_name" ) )
		m_szReturnItemNameOnUse = AllocStringLocal( data->szValue );
	else if ( FStrEq( data->szKeyName, "pass_return_item_group" ) )
		m_szReturnItemGroupOnUse = AllocStringLocal( data->szValue );
	else if ( FStrEq( data->szKeyName, "pass_ignore_return_triggers" ) )
		m_fIgnoreReturnTriggers = atoi( data->szValue ) >= 1;
	else if ( FStrEq( data->szKeyName, "pass_destroy_item_name" ) )
		m_szDestroyItemNameOnUse = AllocStringLocal( data->szValue );
	else if ( FStrEq( data->szKeyName, "pass_destroy_item_group" ) )
		m_szDestroyItemGroupOnUse = AllocStringLocal( data->szValue );
	else if ( FStrEq( data->szKeyName, "pass_ignore_destroy_triggers" ) )
		m_fIgnoreDestroyTriggers = atoi( data->szValue ) >= 1;
	else if ( FStrEq( data->szKeyName, "target_on_fail" ) )
		m_szTargetOnFail = AllocStringLocal( data->szValue );
	else
		return false;

	data->fHandled = TRUE;
	return true;
}

bool Inventory_CheckTokenStringRule( const char *rule, const char *value )
{
	if ( !rule )
		return value == NULL;

	if ( !value )
		return false;

	if ( !*rule )
		return !*value;

	if ( !*value )
		return false;

	if ( !strchr( rule, ';' ) )
		return !stricmp( value, rule );

	char *buffer = NULL;
	char *context = NULL;
	char *token;

	while ( (token = UTIL_StringTokens( rule, ";", &buffer, &context )) != NULL )
	{
		if ( !stricmp( value, token ) )
			return true;
	}

	return false;
}

bool IsValidInventoryHolder( CBaseEntity *entity )
{
	if ( !entity || entity->pev->solid == SOLID_NOT || !entity->IsPlayer() )
		return false;

	return !entity->IsPlayer() || !(((CBasePlayer *) entity)->m_afPhysicsFlags & PFLAG_OBSERVER);
}

int Inventory_ExistsInGroup( const char *group )
{
	if ( !group || !*group )
		return 0;

	int count = 0;
	CBaseEntity *entity = NULL;

	while ( (entity = UTIL_FindEntityByClassname( entity, "item_inventory" )) != NULL )
	{
		CItemInventory *item = dynamic_cast<CItemInventory *>( entity );

		if ( item && !stricmp( STRING( item->m_szItemGroup ), group ) )
			count++;
	}

	return count;
}

int Inventory_HaveNumFromGroup( CBaseMonster *holder, const char *group )
{
	if ( !IsValidInventoryHolder( holder ) || !group || !*group )
		return 0;

	int count = 0;

	for ( CInventoryItemNode *node = holder->m_pInventory; node; node = node->m_pNext )
	{
		CItemInventory *item = (CItemInventory *) Inventory_ResolveHandle( node->m_hItem );

		if ( item && !stricmp( STRING( item->m_szItemGroup ), group ) )
			count++;
	}

	return count;
}

bool Inventory_HasAllInGroup( CBaseMonster *holder, const char *group )
{
	if ( !IsValidInventoryHolder( holder ) )
		return false;

	int total = Inventory_ExistsInGroup( group );
	int held = Inventory_HaveNumFromGroup( holder, group );

	return held >= total;
}

bool CInventoryRules::CanUseTrigger( CBaseEntity *pActivator, bool fContinueByDefault )
{
	CBaseEntity *owner = Inventory_ResolveHandle( m_hEntity );

	if ( !owner || owner->IsLockedByMaster() )
		return false;

	if ( !(m_szRequiresItemName || m_szRequiresItemGroup || m_szCantHaveItemName || m_szCantHaveItemGroup)
		|| !pActivator || !IsValidInventoryHolder( pActivator ) )
		return !fContinueByDefault;

	CBaseMonster *holder = pActivator->IsMonster() ? (CBaseMonster *) pActivator : NULL;

	BOOL isPlayer = pActivator->IsPlayer();

	if ( !holder )
		return !fContinueByDefault;

	bool passed = true;

	if ( m_szCantHaveItemName )
	{
		for ( CInventoryItemNode *node = holder->m_pInventory; node; node = node->m_pNext )
		{
			CItemInventory *item = (CItemInventory *) Inventory_ResolveHandle( node->m_hItem );

			if ( item && Inventory_CheckTokenStringRule( STRING( m_szCantHaveItemName ), STRING( item->m_szItemName ) ) )
			{
				passed = false;
				break;
			}
		}
	}

	if ( m_szCantHaveItemGroup && passed )
	{
		const char *groups = STRING( m_szCantHaveItemGroup );
		char *buffer = NULL;
		char *context = NULL;
		char *group;

		while ( (group = UTIL_StringTokens( groups, ";", &buffer, &context )) != NULL )
		{
			int total = Inventory_ExistsInGroup( group );

			if ( total || m_iRequiresItemGroupNum >= 1 )
			{
				int held = Inventory_HaveNumFromGroup( holder, group );

				if ( (!m_iCantHaveItemGroupNum && held >= total) || held >= m_iCantHaveItemGroupNum )
					passed = false;
			}
		}
	}

	if ( m_szRequiresItemName && passed )
	{
		int required = 1;
		int held = 0;

		for ( const char *name = STRING( m_szRequiresItemName ); *name; name++ )
		{
			if ( *name == ';' )
				required++;
		}

		for ( CInventoryItemNode *node = holder->m_pInventory; node; node = node->m_pNext )
		{
			CItemInventory *item = (CItemInventory *) Inventory_ResolveHandle( node->m_hItem );

			if ( item && Inventory_CheckTokenStringRule( STRING( m_szRequiresItemName ), STRING( item->m_szItemName ) ) )
				held++;
		}

		if ( held < required )
			passed = false;
	}

	if ( m_szRequiresItemGroup && passed )
	{
		const char *groups = STRING( m_szRequiresItemGroup );
		char *buffer = NULL;
		char *context = NULL;
		char *group;

		while ( (group = UTIL_StringTokens( groups, ";", &buffer, &context )) != NULL )
		{
			int total = Inventory_ExistsInGroup( group );

			if ( total || m_iRequiresItemGroupNum >= 1 )
			{
				int held = Inventory_HaveNumFromGroup( holder, group );

				if ( (!m_iRequiresItemGroupNum && held < total) || held < m_iRequiresItemGroupNum )
					passed = false;
			}
		}
	}

	if ( !passed )
	{
		if ( m_szTargetOnFail )
			FireTargets( STRING( m_szTargetOnFail ), pActivator, Inventory_ResolveHandle( m_hEntity ), USE_TOGGLE, 0 );

		return false;
	}

	CInventoryItemNode *node = holder->m_pInventory;

	while ( node )
	{
		CItemInventory *item = (CItemInventory *) Inventory_ResolveHandle( node->m_hItem );

		node = node->m_pNext;

		if ( !item )
			continue;

		if ( !m_fIgnoreUseTriggers
			&& ((m_szRequiresItemName && item->m_szItemName
				 && Inventory_CheckTokenStringRule( STRING( m_szRequiresItemName ), STRING( item->m_szItemName ) ))
				|| (m_szRequiresItemGroup && item->m_szItemGroup
					&& Inventory_CheckTokenStringRule( STRING( m_szRequiresItemGroup ), STRING( item->m_szItemGroup ) ))) )
		{
			item->HandleTriggers( pActivator, item->m_szTriggerOnUseSelf, item->m_szTriggerOnUseTeam, item->m_szTriggerOnUseOther );
		}

		if ( m_szDestroyItemNameOnUse
			&& Inventory_CheckTokenStringRule( STRING( m_szDestroyItemNameOnUse ), STRING( item->m_szItemName ) ) )
			item->Destroy( m_fIgnoreDestroyTriggers );

		if ( m_szDestroyItemGroupOnUse
			&& Inventory_CheckTokenStringRule( STRING( m_szDestroyItemGroupOnUse ), STRING( item->m_szItemGroup ) ) )
			item->Destroy( m_fIgnoreDestroyTriggers );

		if ( m_szReturnItemNameOnUse
			&& Inventory_CheckTokenStringRule( STRING( m_szReturnItemNameOnUse ), STRING( item->m_szItemName ) ) )
			item->Return( m_fIgnoreReturnTriggers );

		if ( m_szReturnItemGroupOnUse
			&& Inventory_CheckTokenStringRule( STRING( m_szReturnItemGroupOnUse ), STRING( item->m_szItemGroup ) ) )
			item->Return( m_fIgnoreReturnTriggers );

		if ( m_szDropItemNameOnUse
			&& Inventory_CheckTokenStringRule( STRING( m_szDropItemNameOnUse ), STRING( item->m_szItemName ) ) )
			item->Drop( true, Inventory_ResolveHandle( m_hEntity ), m_fIgnoreDropTriggers );

		if ( m_szDropItemGroupOnUse
			&& Inventory_CheckTokenStringRule( STRING( m_szDropItemGroupOnUse ), STRING( item->m_szItemGroup ) ) )
			item->Drop( true, Inventory_ResolveHandle( m_hEntity ), m_fIgnoreDropTriggers );
	}

	return passed;
}
