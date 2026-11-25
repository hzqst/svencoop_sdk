#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "trigger_lookat.h"

#define SF_LOOKAT_START_ON 1
#define SF_LOOKAT_ONCE 2

LINK_ENTITY_TO_CLASS( trigger_lookat, CTriggerLookat );

CBaseEntity *LookatHandleEntity( EHANDLE &handle )
{
	edict_t *entity = handle.Get();

	return entity && !entity->free ? (CBaseEntity *) GET_PRIVATE( entity ) : NULL;
}

void LookatSetHandle( EHANDLE &handle, CBaseEntity *entity )
{
	handle = EHANDLE();

	if ( entity && entity->pev && entity->pev->pContainingEntity && !entity->pev->pContainingEntity->free )
		handle.Set( entity->pev->pContainingEntity );
}

CTriggerLookat::CTriggerLookat()
    : m_bActive( false ), m_flLastSearch( 0 ), m_pTargets( NULL ), m_hLookTarget()
{
}

CTriggerLookat::~CTriggerLookat()
{
}

void CTriggerLookat::Spawn( void )
{
	if ( pev->spawnflags & SF_LOOKAT_START_ON )
		m_bActive = true;

	m_pTargets = NULL;
	m_hLookTarget = EHANDLE();
	m_flLastSearch = 0;

	if ( m_bActive )
	{
		SetThink( &CTriggerLookat::Think );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

void CTriggerLookat::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_bActive ) )
		return;

	m_bActive = !m_bActive;

	if ( m_bActive )
		Think();
	else
	{
		SetThink( NULL );
		m_flLastSearch = 0;
	}
}

void CTriggerLookat::FindTargets( void )
{
	TargetNode *node = m_pTargets;

	while ( node )
	{
		TargetNode *next = node->next;

		delete node;
		node = next;
	}

	TargetNode *targets = NULL;
	CBaseEntity *target = NULL;

	while ( ( target = UTIL_FindEntityByTargetname( target, STRING( pev->target ) ) ) != NULL )
	{
		node = new TargetNode;
		LookatSetHandle( node->entity, target );
		node->next = targets;
		targets = node;
	}

	// Both original versions build this local chain without assigning m_pTargets
}

void CTriggerLookat::Think( void )
{
	CBaseEntity *lookTarget = LookatHandleEntity( m_hLookTarget );

	if ( !lookTarget )
	{
		LookatSetHandle( m_hLookTarget, UTIL_FindEntityByTargetname( NULL, STRING( pev->netname ) ) );
		lookTarget = LookatHandleEntity( m_hLookTarget );
	}

	if ( lookTarget )
	{
		if ( m_flLastSearch == 0 || gpGlobals->time - m_flLastSearch > 10.0f )
		{
			FindTargets();
			m_flLastSearch = gpGlobals->time;
		}

		for ( TargetNode *node = m_pTargets; node; node = node->next )
		{
			CBaseEntity *target = LookatHandleEntity( node->entity );

			if ( target )
				target->pev->angles = UTIL_VecToAngles( lookTarget->pev->origin - target->pev->origin ) + pev->angles;
			else
				m_flLastSearch = 0;
		}
	}

	if ( m_bActive && !( pev->spawnflags & SF_LOOKAT_ONCE ) )
	{
		SetThink( &CTriggerLookat::Think );
		pev->nextthink = gpGlobals->time;
	}
	else
	{
		SetThink( NULL );
		m_flLastSearch = 0;
	}
}
