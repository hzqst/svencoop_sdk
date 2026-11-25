#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "trigger_createentity.h"
#include "server_entvars.h"
#include "server_support.h"

static edict_t *GetCreateEntityEdict( CBaseEntity *entity )
{
	if ( !entity || !entity->pev )
		return NULL;

	edict_t *edict = entity->pev->pContainingEntity;
	if ( !edict || edict->free || FNullEnt( edict ) )
		return NULL;

	return edict;
}

LINK_ENTITY_TO_CLASS( trigger_createentity, CTriggerCreateEntity );
LINK_ENTITY_TO_CLASS( trigger_create_entity, CTriggerCreateEntity );

void CTriggerCreateEntity::OnCreate( void )
{
	CBaseEntity::OnCreate();
	m_pKeyValues = NULL;
}

void CTriggerCreateEntity::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszCrtEntChildClass" ) )
		m_iszCrtEntChildClass = AllocStringLocal( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iszCrtEntChildName" ) )
		m_iszCrtEntChildName = AllocStringLocal( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iszTriggerAfter" ) )
	{
		m_iszTriggerAfter = AllocStringLocal( pkvd->szValue );
		pev->target = m_iszTriggerAfter;
	}
	else if ( FStrEq( pkvd->szKeyName, "target" ) || FStrEq( pkvd->szKeyName, "delay" ) ||
	          FStrEq( pkvd->szKeyName, "killtarget" ) )
	{
		CBaseDelay::KeyValue( pkvd );
		return;
	}
	else
	{
		KeyValueLink *entry = new KeyValueLink;

		memset( entry, 0, sizeof( *entry ) );
		entry->next = m_pKeyValues;

		if ( pkvd->szKeyName[0] != '+' && pkvd->szKeyName[0] != '-' )
		{
			char name[256] = "-";

			ALERT( at_console, "trigger_createentity \"%s\": Key \"%s\"  with value \"%s\" does not belong directly to trigger_createentity, but also has no leading minus nor plus. Assuming minus (directly set value).\n",
			       STRING( pev->targetname ), pkvd->szKeyName, pkvd->szValue );

			UTIL_AppendString( name, pkvd->szKeyName, sizeof( name ) );
			entry->key.szKeyName = (char *) STRING( AllocStringLocal( name ) );
		}
		else
			entry->key.szKeyName = (char *) STRING( AllocStringLocal( pkvd->szKeyName ) );

		entry->key.szValue = (char *) STRING( AllocStringLocal( pkvd->szValue ) );
		m_pKeyValues = entry;
	}

	pkvd->fHandled = TRUE;
}

void CTriggerCreateEntity::Spawn( void )
{
	m_iszValueType = 0;
	Precache();
}

void CTriggerCreateEntity::Precache( void )
{
	KeyValueLink *entry = m_pKeyValues;

	if ( m_iszCrtEntChildClass )
		UTIL_PrecacheOther( STRING( m_iszCrtEntChildClass ) );

	for ( ; entry; entry = entry->next )
	{
		if ( !stricmp( entry->key.szKeyName, "-model" ) )
			UTIL_PrecacheModel( this, entry->key.szValue );
		else if ( !stricmp( entry->key.szKeyName, "-sound" ) )
			PrecacheSound_Intercept( this, entry->key.szValue );
	}
}

void CTriggerCreateEntity::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( !m_iszCrtEntChildClass )
	{
		ALERT( at_console, "Trigger_createentity \"%s\": No class specified!\n", STRING( pev->targetname ) );
		return;
	}

	CBaseEntity *entity = CBaseEntity::Create( STRING( m_iszCrtEntChildClass ), pev->origin, pev->angles, true, NULL );
	edict_t *edict = GetCreateEntityEdict( entity );

	if ( edict )
		edict = GetCreateEntityEdict( entity );

	if ( !edict )
	{
		ALERT( at_console, "Trigger_createentity \"%s\": Class \"%s\" does not exist!\n",
		       STRING( pev->targetname ), STRING( m_iszCrtEntChildClass ) );
		return;
	}

	if ( m_iszCrtEntChildName )
		entity->pev->targetname = AllocStringLocal( STRING( m_iszCrtEntChildName ) );

	bool originSet = false;
	bool anglesSet = false;
	bool modelFailed = false;

	for ( KeyValueLink *entry = m_pKeyValues; entry; entry = entry->next )
	{
		const char *name = entry->key.szKeyName + 1;
		const char *sourceName = entry->key.szValue;

		if ( entry->key.szKeyName[0] == '+' )
		{
			CBaseEntity *source;

			if ( FStrEq( sourceName, "!activator" ) )
				source = activator;
			else if ( FStrEq( sourceName, "!caller" ) )
				source = caller;
			else
				source = UTIL_FindEntityByTargetname( NULL, sourceName );

			if ( !GetCreateEntityEdict( source ) )
			{
				ALERT( at_console, "Trigger_createentity \"%s\": Could not find source entity \"%s\" for keyvalue \"%s\".\n",
				       STRING( pev->targetname ), sourceName, name );

				if ( FStrEq( name, "model" ) )
				{
					modelFailed = true;
					break;
				}

				continue;
			}

			void *sourceValue = NULL;
			void *destinationValue = NULL;
			int index = INVALID_ENTVAR_INDEX;
			entvartype_s type;

			type = source->GetEntvar( name, &sourceValue, index );

			entity->GetEntvar( name, &destinationValue, index );

			switch ( type )
			{
				case VAR_STRING:
					SpecialValueByteArray( (int *) destinationValue, STRING( *(string_t *) sourceValue ), entity );
					break;
				case VAR_VECTOR:
					SpecialValueVector( (Vector *) destinationValue, *(Vector *) sourceValue );
					break;
				case VAR_FLOAT:
					SpecialValueFloat( (float *) destinationValue, *(float *) sourceValue );
					break;
				case VAR_INTEGER:
					SpecialValueInt( (int *) destinationValue, *(int *) sourceValue );
					break;
				default:
					ALERT( at_console, "Trigger_createentity \"%s\": Cannot copy non-existent or unsupported entity-specific keyvalue \"%s\" from other entity \"%s\".\n",
					       STRING( pev->targetname ), name, sourceName );
					continue;
			}

			entity->OnKeyValueUpdate( name );
		}
		else if ( entry->key.szKeyName[0] == '-' )
		{
			if ( !SpecialValue( entity, name, sourceName ) && !TryKeyValue( entity, name, sourceName ) )
			{
				ALERT( at_console, "Trigger_createentity \"%s\": No keyvalue by the name \"%s\" does exist in class %s of entity to be created.\n",
				       STRING( pev->targetname ), name, STRING( entity->pev->classname ) );
				continue;
			}

			entity->OnKeyValueUpdate( name );
		}

		if ( FStrEq( name, "origin" ) )
			originSet = true;
		else if ( FStrEq( name, "angles" ) )
			anglesSet = true;
	}

	if ( !originSet )
		entity->pev->origin = pev->origin;

	if ( !anglesSet )
		entity->pev->angles = pev->angles;

	if ( modelFailed )
	{
		ALERT( at_console, "Trigger_createentity \"%s\": Not creating %s \"%s\", because an attempt to initialize the crucial keyvalue \"model\" failed. (See above)\n",
		       STRING( pev->targetname ), STRING( m_iszCrtEntChildClass ), STRING( m_iszCrtEntChildName ) );
		UTIL_Remove( entity );
		return;
	}

	DispatchSpawn( edict );
	SUB_UseTargets( activator, USE_TOGGLE, 0 );
}
