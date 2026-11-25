#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "env_fog.h"
#include "user_messages.h"

#define SF_FOG_START_OFF 1

static int g_iFogID;

LINK_ENTITY_TO_CLASS( env_fog, CEnvFog );

void CEnvFog::OnCreate( void )
{
	CBaseEntity::OnCreate();
	g_iFogID++;
	m_iID = g_iFogID;
}

void CEnvFog::Spawn( void )
{
	m_fActive = false;
	m_fLastActive = false;
	m_vecLastOrigin = pev->origin;
	m_iLastRadius = pev->iuser1;
	m_vecLastColor = pev->rendercolor;
	m_iLastStartDist = pev->iuser2;
	m_iLastEndDist = pev->iuser3;

	if ( !( pev->spawnflags & SF_FOG_START_OFF ) )
		m_fActive = true;

	SetThink( &CEnvFog::Think );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CEnvFog::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "iuser1" ) )
	{
		pev->iuser1 = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "iuser2" ) || FStrEq( pkvd->szKeyName, "startdist" ) )
	{
		pev->iuser2 = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "iuser3" ) || FStrEq( pkvd->szKeyName, "enddist" ) )
	{
		pev->iuser3 = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseEntity::KeyValue( pkvd );
	}
}

void CEnvFog::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	switch ( useType )
	{
	case USE_OFF:
		m_fActive = false;
		break;

	case USE_ON:
		m_fActive = true;
		break;

	case USE_TOGGLE:
		m_fActive = !m_fActive;
		break;
	}
}

void CEnvFog::Think( void )
{
	if ( m_fActive != m_fLastActive
	     || ( m_fActive
	          && ( pev->origin != m_vecLastOrigin || pev->iuser1 != m_iLastRadius
	               || pev->iuser2 != m_iLastStartDist || pev->iuser3 != m_iLastEndDist
	               || pev->rendercolor != m_vecLastColor ) ) )
	{
		m_fLastActive = m_fActive;
		m_vecLastOrigin = pev->origin;
		m_iLastRadius = pev->iuser1;
		m_iLastStartDist = pev->iuser2;
		m_iLastEndDist = pev->iuser3;
		m_vecLastColor = pev->rendercolor;

		MESSAGE_BEGIN( MSG_ALL, gmsgFog );
			WRITE_SHORT( m_iID );
			WRITE_BYTE( m_fActive );
			WRITE_COORD( m_vecLastOrigin.x );
			WRITE_COORD( m_vecLastOrigin.y );
			WRITE_COORD( m_vecLastOrigin.z );
			WRITE_SHORT( m_iLastRadius );
			WRITE_BYTE( (int) m_vecLastColor.x );
			WRITE_BYTE( (int) m_vecLastColor.y );
			WRITE_BYTE( (int) m_vecLastColor.z );
			WRITE_SHORT( m_iLastStartDist );
			WRITE_SHORT( m_iLastEndDist );
		MESSAGE_END();
	}

	SetThink( &CEnvFog::Think );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CEnvFog::UpdatePlayer( CBaseEntity *player )
{
	if ( !player )
		return;

	edict_t *recipient = NULL;
	if ( player->pev )
		recipient = player->edict();

	MESSAGE_BEGIN( MSG_ONE, gmsgFog, NULL, recipient );
		WRITE_SHORT( m_iID );
		WRITE_BYTE( m_fActive );
		WRITE_COORD( m_vecLastOrigin.x );
		WRITE_COORD( m_vecLastOrigin.y );
		WRITE_COORD( m_vecLastOrigin.z );
		WRITE_SHORT( m_iLastRadius );
		WRITE_BYTE( (int) m_vecLastColor.x );
		WRITE_BYTE( (int) m_vecLastColor.y );
		WRITE_BYTE( (int) m_vecLastColor.z );
		WRITE_SHORT( m_iLastStartDist );
		WRITE_SHORT( m_iLastEndDist );
	MESSAGE_END();
}

void CEnvFog::OnDestroy( void )
{
	if ( m_fActive )
	{
		m_fActive = false;

		MESSAGE_BEGIN( MSG_ALL, gmsgFog );
			WRITE_SHORT( m_iID );
			WRITE_BYTE( 0 );
			WRITE_COORD( 0 );
			WRITE_COORD( 0 );
			WRITE_COORD( 0 );
			WRITE_SHORT( 0 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
			WRITE_SHORT( 0 );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}

	CBaseEntity::OnDestroy();
}
