#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "trigger_entity_iterator.h"

CBaseEntity *IteratorHandleEntity( EHANDLE &handle )
{
	edict_t *entity = handle.Get();

	if ( !entity || entity->free )
		return NULL;

	return (CBaseEntity *) GET_PRIVATE( entity );
}

void IteratorSetHandle( EHANDLE &handle, CBaseEntity *entity )
{
	handle = EHANDLE();

	if ( entity && entity->pev && entity->pev->pContainingEntity && !entity->pev->pContainingEntity->free )
		handle.Set( entity->pev->pContainingEntity );
}

bool IteratorEntityValid( CBaseEntity *entity )
{
	return entity && entity->pev && entity->pev->pContainingEntity &&
	       !entity->pev->pContainingEntity->free && !FNullEnt( entity->pev->pContainingEntity );
}

bool IteratorPlayerConnected( CBaseEntity *entity )
{
	return !FClassnameIs( entity->pev, "player" ) || ( (CBasePlayer *) entity )->IsConnected();
}

LINK_ENTITY_TO_CLASS( trigger_entity_iterator, CTriggerEntityIterator );

void CTriggerEntityIterator::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "triggerstate" ) )
	{
		int state = atoi( pkvd->szValue );

		m_triggerState = state == 0 ? USE_OFF : state == 1 ? USE_ON : USE_TOGGLE;
	}
	else if ( FStrEq( pkvd->szKeyName, "run_mode" ) )
	{
		int mode = atoi( pkvd->szValue );

		m_iRunMode = mode < 0 ? 0 : mode >= 2 ? 2 : mode;
	}
	else if ( FStrEq( pkvd->szKeyName, "delay_between_triggers" ) )
	{
		double delay = atof( pkvd->szValue );

		m_flDelayBetweenTriggers = (float) ( delay < 0 ? 0 : delay );
	}
	else if ( FStrEq( pkvd->szKeyName, "delay_between_runs" ) )
	{
		double delay = atof( pkvd->szValue );

		m_flDelayBetweenRuns = (float) ( delay < 0 ? 0 : delay );
	}
	else if ( FStrEq( pkvd->szKeyName, "name_filter" ) )
		m_iszNameFilter = AllocStringLocal( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "classname_filter" ) )
		m_iszClassnameFilter = AllocStringLocal( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "status_filter" ) )
	{
		int filter = atoi( pkvd->szValue );

		m_iStatusFilter = filter == 1 || filter == 2 ? filter : 0;
	}
	else if ( FStrEq( pkvd->szKeyName, "maximum_runs" ) )
	{
		int runs = atoi( pkvd->szValue );

		m_iMaximumRuns = runs < 0 ? 0 : runs;
	}
	else if ( FStrEq( pkvd->szKeyName, "trigger_after_run" ) )
		m_iszTriggerAfterRun = AllocStringLocal( pkvd->szValue );
	else
	{
		CBaseEntity::KeyValue( pkvd );
		return;
	}

	pkvd->fHandled = TRUE;
}

void *CTriggerEntityIterator::GetKeyValue( const char *name, entvartype_s &type )
{
	if ( FStrEq( name, "triggerstate" ) )
	{
		type = VAR_INTEGER;
		return &m_triggerState;
	}

	if ( FStrEq( name, "run_mode" ) )
	{
		type = VAR_INTEGER;
		return &m_iRunMode;
	}

	if ( FStrEq( name, "delay_between_triggers" ) )
	{
		type = VAR_FLOAT;
		return &m_flDelayBetweenTriggers;
	}

	if ( FStrEq( name, "delay_between_runs" ) )
	{
		type = VAR_FLOAT;
		return &m_flDelayBetweenRuns;
	}

	if ( FStrEq( name, "name_filter" ) )
	{
		type = VAR_STRING;
		return &m_iszNameFilter;
	}

	if ( FStrEq( name, "classname_filter" ) )
	{
		type = VAR_STRING;
		return &m_iszClassnameFilter;
	}

	if ( FStrEq( name, "status_filter" ) )
	{
		type = VAR_INTEGER;
		return &m_iStatusFilter;
	}

	if ( FStrEq( name, "maximum_runs" ) )
	{
		type = VAR_INTEGER;
		return &m_iMaximumRuns;
	}

	if ( FStrEq( name, "trigger_after_run" ) )
	{
		type = VAR_STRING;
		return &m_iszTriggerAfterRun;
	}

	return CBaseEntity::GetKeyValue( name, type );
}

void CTriggerEntityIterator::Stop( void )
{
	m_iEntityCount = 0;
	SetThink( NULL );
	m_bActive = false;
}

void CTriggerEntityIterator::Spawn( void )
{
	Stop();
	m_iRunCount = 0;
}

void CTriggerEntityIterator::ClearEntityList( void )
{
	m_iEntityCount = 0;
	m_iEntityCapacity = 0;

	if ( m_pEntities )
	{
		free( m_pEntities );
		m_pEntities = NULL;
	}
}

void CTriggerEntityIterator::OnDestroy( void )
{
	Stop();
	ClearEntityList();
	CBaseEntity::OnDestroy();
}

CTriggerEntityIterator *CTriggerEntityIterator::MakeCloneForThreading( void )
{
	edict_t *edict = CREATE_NAMED_ENTITY( pev->classname );

	if ( !edict || edict->free || !GET_PRIVATE( edict ) )
		return NULL;

	CTriggerEntityIterator *clone = (CTriggerEntityIterator *) GET_PRIVATE( edict );

	clone->m_flDelayBetweenTriggers = m_flDelayBetweenTriggers;
	clone->m_flDelayBetweenRuns = m_flDelayBetweenRuns;
	clone->m_iszNameFilter = m_iszNameFilter;
	clone->m_iszClassnameFilter = m_iszClassnameFilter;
	clone->pev->target = pev->target;
	clone->m_triggerState = m_triggerState;
	clone->m_iStatusFilter = m_iStatusFilter;
	clone->m_iszTriggerAfterRun = m_iszTriggerAfterRun;
	clone->m_iRunMode = 0;
	clone->m_iMaximumRuns = 1;
	DispatchSpawn( clone->edict() );

	return clone;
}

void CTriggerEntityIterator::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( useType == USE_OFF )
	{
		Stop();
		return;
	}

	if ( useType == USE_ON )
		m_bActive = true;
	else
	{
		m_bActive = !m_bActive;

		if ( !m_bActive )
		{
			Stop();
			return;
		}
	}

	IteratorSetHandle( m_hActivator, activator );

	if ( m_iRunMode == 1 )
	{
		CTriggerEntityIterator *clone = MakeCloneForThreading();

		if ( clone )
		{
			clone->Use( IteratorHandleEntity( m_hActivator ), this, USE_ON, 0 );
			m_iRunCount++;

			if ( m_iMaximumRuns > 0 && m_iRunCount >= m_iMaximumRuns )
			{
				Stop();
				SUB_Remove();
			}
		}
		else
			ALERT( at_console, "Trigger_entity_iterator \"%s\": Failed to create clone for threading.",
			       STRING( pev->targetname ) );

		m_bActive = false;
		return;
	}

	m_iCurrentEntity = 0;
	m_iEntityCount = 0;
	ProcessIteration();
}

const char *CTriggerEntityIterator::NameFilter( void )
{
	return m_iszNameFilter ? STRING( m_iszNameFilter ) : NULL;
}

const char *CTriggerEntityIterator::ClassnameFilter( void )
{
	return m_iszClassnameFilter ? STRING( m_iszClassnameFilter ) : NULL;
}

bool CTriggerEntityIterator::HasFilters( void )
{
	if ( NameFilter() || ClassnameFilter() )
		return true;

	ALERT( at_console, "Trigger_entity_iterator \"%s\": No criteria for finding entities to iterate over is specified. Disabling.\n",
	       STRING( pev->targetname ) );
	return false;
}

CBaseEntity *CTriggerEntityIterator::FindNext( bool useName, bool useClassname )
{
	for (;;)
	{
		m_pSearchCursor = UTIL_FindEntityByString( m_pSearchCursor, useName ? "targetname" : "classname",
		                                         useName ? NameFilter() : ClassnameFilter() );

		if ( !m_pSearchCursor || !useName || !useClassname ||
		     FStrEq( ClassnameFilter(), STRING( m_pSearchCursor->pev->classname ) ) )
			return m_pSearchCursor;
	}
}

void CTriggerEntityIterator::AddEntity( CBaseEntity *entity )
{
	if ( !IteratorPlayerConnected( entity ) )
		return;

	if ( m_iEntityCount == 0 )
	{
		m_iEntityCapacity = 8;
		m_pEntities = (EHANDLE *) malloc( sizeof( EHANDLE ) * 8 );
	}
	else if ( m_iEntityCount >= m_iEntityCapacity )
	{
		int capacity = m_iEntityCount + 8;
		EHANDLE *entities = (EHANDLE *) realloc( m_pEntities, sizeof( EHANDLE ) * capacity );

		if ( !entities )
			return;

		m_iEntityCapacity = capacity;
		m_pEntities = entities;
	}

	IteratorSetHandle( m_pEntities[m_iEntityCount], entity );
	m_iEntityCount++;
}

bool CTriggerEntityIterator::BuildEntityList( void )
{
	m_iEntityCount = 0;
	m_pSearchCursor = NULL;

	if ( !HasFilters() )
		return false;

	bool useName = NameFilter() != NULL;
	bool useClassname = ClassnameFilter() != NULL;

	while ( FindNext( useName, useClassname ) )
		AddEntity( m_pSearchCursor );

	return true;
}

void CTriggerEntityIterator::TriggerEntity( CBaseEntity *entity )
{
	if ( !IteratorPlayerConnected( entity ) )
		return;

	if ( m_iStatusFilter == 1 )
	{
		if ( entity->pev->deadflag != DEAD_NO )
			return;
	}
	else if ( m_iStatusFilter == 2 )
	{
		if ( entity->pev->deadflag == DEAD_NO )
			return;
	}
	else if ( m_iStatusFilter != 0 )
		return;

	IteratorHandleEntity( m_hTarget )->Use( entity, this, m_triggerState, 0 );
}

void CTriggerEntityIterator::IterateAll( void )
{
	m_pSearchCursor = NULL;

	if ( !HasFilters() )
		return;

	bool useName = NameFilter() != NULL;
	bool useClassname = ClassnameFilter() != NULL;

	while ( FindNext( useName, useClassname ) )
		TriggerEntity( m_pSearchCursor );
}

void CTriggerEntityIterator::FinishRun( bool triggerAfterRun )
{
	m_iRunCount++;

	if ( triggerAfterRun && m_iszTriggerAfterRun && STRING( m_iszTriggerAfterRun ) )
	{
		CBaseEntity *activator = IteratorHandleEntity( m_hActivator );

		FireTargets( STRING( m_iszTriggerAfterRun ), activator ? activator : this, this, USE_TOGGLE, 0 );
	}

	if ( m_iMaximumRuns > 0 && m_iRunCount >= m_iMaximumRuns )
	{
		Stop();
		SUB_Remove();
	}
}

void CTriggerEntityIterator::ProcessIteration( void )
{
	if ( !pev->target || !STRING( pev->target ) )
	{
		ALERT( at_console, "Trigger_entity_iterator \"%s\": No entity to carry the iteration specified. Disabling.\n",
		       STRING( pev->targetname ) );
		Stop();
		return;
	}

	CBaseEntity *target = IteratorHandleEntity( m_hTarget );

	if ( !IteratorEntityValid( target ) || !FStrEq( STRING( target->pev->targetname ), STRING( pev->target ) ) )
	{
		IteratorSetHandle( m_hTarget, UTIL_FindEntityByString( NULL, "targetname", STRING( pev->target ) ) );
		target = IteratorHandleEntity( m_hTarget );
	}

	if ( !IteratorEntityValid( target ) )
	{
		ALERT( at_console, "Trigger_entity_iterator \"%s\": No entity by the name \"%s\" to carry the iteration could be found. Disabling.\n",
		       STRING( pev->targetname ), STRING( pev->target ) );
		Stop();
		return;
	}

	if ( !( m_flDelayBetweenTriggers > 0 ) )
	{
		IterateAll();
		ClearEntityList();
	}
	else
	{
		bool listed = true;

		if ( m_iCurrentEntity == m_iEntityCount )
		{
			listed = BuildEntityList();
			m_iCurrentEntity = 0;
		}

		if ( m_iEntityCount > 0 )
		{
			if ( !listed )
				Stop();
			else
			{
				CBaseEntity *entity = IteratorHandleEntity( m_pEntities[m_iCurrentEntity] );

				if ( entity )
					TriggerEntity( entity );

				m_iCurrentEntity++;
				pev->nextthink = gpGlobals->time + m_flDelayBetweenTriggers;
			}
		}
	}

	if ( !m_bActive )
		return;

	if ( m_iRunMode == 2 )
	{
		SetThink( &CTriggerEntityIterator::ProcessIteration );

		if ( m_iCurrentEntity == m_iEntityCount )
		{
			pev->nextthink = gpGlobals->time + m_flDelayBetweenRuns;
			FinishRun( true );
		}
	}
	else if ( m_iCurrentEntity == m_iEntityCount )
	{
		Stop();
		FinishRun( true );
	}
	else
		SetThink( &CTriggerEntityIterator::ProcessIteration );
}
