#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "lights.h"
#include "global_light_control.h"

LINK_ENTITY_TO_CLASS( global_light_control, CGlobalLightControl );

TYPEDESCRIPTION CGlobalLightControl::m_SaveData[] =
{
	DEFINE_FIELD( CGlobalLightControl, m_iszPattern, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CGlobalLightControl, CPointEntity );

void CGlobalLightControl::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "pattern" ) )
	{
		m_iszPattern = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "target_entity" ) )
	{
		m_iszTargetEntity = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CGlobalLightControl::Spawn( void )
{
	if ( !pev->targetname )
	{
		Use( NULL, NULL, USE_TOGGLE, 0 );
		edict_t *entity = NULL;
		if ( pev )
			entity = pev->pContainingEntity;
		REMOVE_ENTITY( entity );
	}
}

void CGlobalLightControl::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	int style = 0;

	if ( m_iszTargetEntity )
	{
		CLight *light = dynamic_cast<CLight *>( UTIL_FindEntityByTargetname( NULL, STRING( m_iszTargetEntity ) ) );

		if ( !light )
		{
			ALERT( at_console, "global_light_control: Couldn't find %s", STRING( m_iszTargetEntity ) );
			return;
		}

		style = light->m_iStyle;
	}

	LIGHT_STYLE( style, (char *) STRING( m_iszPattern ) );
	SUB_UseTargets( activator, useType, value );
}
