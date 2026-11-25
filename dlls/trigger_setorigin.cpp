#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "trigger_setorigin.h"

enum
{
	SF_SETORIGIN_CONTINUOUS = 1,
	SF_SETORIGIN_REMOVE = 4,
	SF_SETORIGIN_ROTATE = 8,
	SF_SETORIGIN_ANGLE_X = 16,
	SF_SETORIGIN_ANGLE_Y = 32,
	SF_SETORIGIN_ANGLE_Z = 64,
	SF_SETORIGIN_ORIGIN_X = 128,
	SF_SETORIGIN_ORIGIN_Y = 256,
	SF_SETORIGIN_ORIGIN_Z = 512,
	SF_SETORIGIN_KEEP_OFFSET = 1024,
	SF_SETORIGIN_AXES = 0x3F0
};

LINK_ENTITY_TO_CLASS( trigger_setorigin, CTriggerSetOrigin );

CBaseEntity *SetOriginHandleEntity( EHANDLE &handle )
{
	edict_t *edict = handle.Get();

	if ( !edict || edict->free )
		return NULL;

	return CBaseEntity::Instance( edict );
}

void SetOriginStoreHandle( EHANDLE &handle, CBaseEntity *entity )
{
	handle = NULL;

	if ( entity && entity->pev )
	{
		edict_t *edict = entity->edict();

		if ( edict && !edict->free )
			handle = entity;
	}
}

bool SetOriginValidEntity( CBaseEntity *entity )
{
	if ( !entity || !entity->pev )
		return false;

	edict_t *edict = entity->edict();

	return edict && !edict->free && CBaseEntity::Instance( edict ) != NULL;
}

CTriggerSetOrigin::CTriggerSetOrigin()
    : m_vecOffset( 0, 0, 0 ), m_vecAngleOffset( 0, 0, 0 ),
      m_bInvertX( false ), m_bInvertY( false ), m_bInvertZ( false ), m_iKnownCount( 0 ),
      m_TriggerState( USE_OFF ), m_iszCopyPointer( iStringNull ), m_bActive( false ), m_bSetupPending( false )
{
	m_hSource = NULL;
	m_hActivator = NULL;
	m_hCaller = NULL;
	m_flDelay = 0;
	m_iszKillTarget = iStringNull;

	for ( int i = 0; i < MAX_ENTITIES; i++ )
	{
		m_hEntities[i] = NULL;
		m_vecRelativeOffsets[i] = g_vecZero;
	}
}

void CTriggerSetOrigin::OnCreate( void )
{
	m_bActive = false;
	m_bSetupPending = false;

	CTriggerSetOriginManager::GetInstance()->Add( this );
}

void CTriggerSetOrigin::OnDestroy( void )
{
	CTriggerSetOriginManager::GetInstance()->Remove( this );
}

void CTriggerSetOrigin::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "triggerstate" ) )
	{
		int state = atoi( pkvd->szValue );

		m_TriggerState = state == 0 ? USE_OFF : state == 2 ? USE_TOGGLE
		                                                   : USE_ON;
	}
	else if ( FStrEq( pkvd->szKeyName, "copypointer" ) )
		m_iszCopyPointer = AllocStringLocal( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "offset" ) )
		UTIL_StringToVector( (float *) &m_vecOffset, pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "angleoffset" ) )
		UTIL_StringToVector( (float *) &m_vecAngleOffset, pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "invert_x" ) )
		m_bInvertX = atoi( pkvd->szValue ) != 0;
	else if ( FStrEq( pkvd->szKeyName, "invert_y" ) )
		m_bInvertY = atoi( pkvd->szValue ) != 0;
	else if ( FStrEq( pkvd->szKeyName, "invert_z" ) )
		m_bInvertZ = atoi( pkvd->szValue ) != 0;
	else if ( FStrEq( pkvd->szKeyName, "m_fRotate" ) )
		pev->spawnflags |= SF_SETORIGIN_ROTATE;
	else
	{
		CBaseDelay::KeyValue( pkvd );
		return;
	}

	pkvd->fHandled = TRUE;
}

void CTriggerSetOrigin::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( pev->spawnflags & SF_SETORIGIN_CONTINUOUS )
	{
		switch ( useType )
		{
			case USE_OFF:
				m_bActive = false;
				break;
			case USE_ON:
				m_bActive = true;
				break;
			case USE_TOGGLE:
				m_bActive = !m_bActive;
				break;
			default:
				break;
		}

		if ( !m_bActive )
			return;
	}

	SetOriginStoreHandle( m_hActivator, activator );
	SetOriginStoreHandle( m_hCaller, caller );
	m_bSetupPending = true;
}

void CTriggerSetOrigin::CopyAngles( CBaseEntity *target, CBaseEntity *source, bool normalize )
{
	const int flags[] = { SF_SETORIGIN_ANGLE_X, SF_SETORIGIN_ANGLE_Y, SF_SETORIGIN_ANGLE_Z };
	const bool invert[] = { m_bInvertX, m_bInvertY, m_bInvertZ };

	for ( int i = 0; i < 3; i++ )
	{
		if ( pev->spawnflags & flags[i] )
		{
			float angle = source->pev->angles[i];

			target->pev->angles[i] = normalize ? UTIL_AngleMod( angle ) : angle;
		}

		if ( invert[i] )
			target->pev->angles[i] = UTIL_AngleMod( -target->pev->angles[i] );
	}
}

void CTriggerSetOrigin::SetupEntities( void )
{
	EHANDLE previousSource = m_hSource;
	const char *copyPointer = STRING( m_iszCopyPointer );
	bool specialSource = false;

	m_bSetupPending = false;
	m_iKnownCount = 0;

	if ( FStrEq( copyPointer, "!activator" ) && SetOriginValidEntity( SetOriginHandleEntity( m_hActivator ) ) )
	{
		m_hSource = m_hActivator;
		specialSource = true;
	}
	else if ( FStrEq( copyPointer, "!caller" ) && SetOriginValidEntity( SetOriginHandleEntity( m_hCaller ) ) )
	{
		m_hSource = m_hCaller;
		specialSource = true;
	}

	if ( !specialSource )
	{
		CBaseEntity *source = UTIL_FindEntityByTargetname( SetOriginHandleEntity( m_hSource ), copyPointer );

		SetOriginStoreHandle( m_hSource, source );
	}

	if ( !SetOriginHandleEntity( m_hSource ) && SetOriginHandleEntity( previousSource ) )
		m_hSource = previousSource;

	CBaseEntity *target = NULL;

	while ( ( target = UTIL_FindEntityByTargetname( target, STRING( pev->target ) ) ) != NULL )
	{
		if ( m_iKnownCount >= MAX_ENTITIES )
			break;

		if ( !SetOriginValidEntity( target ) )
			continue;

		CBaseEntity *source = SetOriginHandleEntity( m_hSource );

		if ( !SetOriginValidEntity( source ) )
		{
			target->Teleport( target->pev->origin + m_vecOffset, target->pev->angles + m_vecAngleOffset );
			target->OnSetOriginByMap();
			continue;
		}

		if ( !( pev->spawnflags & SF_SETORIGIN_KEEP_OFFSET ) )
		{
			int flags = pev->spawnflags;
			int axes = flags & SF_SETORIGIN_AXES;
			Vector origin = target->pev->origin;

			if ( ( flags & SF_SETORIGIN_ORIGIN_X ) || !axes )
				origin.x = source->pev->origin.x;

			if ( ( flags & SF_SETORIGIN_ORIGIN_Y ) || !axes )
				origin.y = source->pev->origin.y;

			if ( ( flags & SF_SETORIGIN_ORIGIN_Z ) || !axes )
				origin.z = source->pev->origin.z;

			CopyAngles( target, source, false );
			target->pev->origin = origin + m_vecOffset;
			target->pev->angles = target->pev->angles + m_vecAngleOffset;
			target->Teleport( target->pev->origin, target->pev->angles );
			target->OnSetOriginByMap();
		}

		SetOriginStoreHandle( m_hEntities[m_iKnownCount], target );

		Vector relative = target->pev->origin - source->pev->origin;
		Vector forward, right, up;

		AngleVectors( source->pev->angles, &forward, &right, &up );

		// Cache the per-component basis sums used by the follow update
		m_vecRelativeOffsets[m_iKnownCount].x = ( ( right.x + forward.x ) + up.x ) * relative.x;
		m_vecRelativeOffsets[m_iKnownCount].y = ( up.y + ( right.y + forward.y ) ) * relative.y;
		m_vecRelativeOffsets[m_iKnownCount].z = ( up.z + ( right.z + forward.z ) ) * relative.z;
		m_iKnownCount++;
	}

	if ( pev->spawnflags & SF_SETORIGIN_REMOVE )
		UTIL_Remove( this );
}

void CTriggerSetOrigin::UpdateKnownEntities( void )
{
	bool updated = false;

	for ( int i = 0; i < MAX_ENTITIES; i++ )
	{
		CBaseEntity *target = SetOriginHandleEntity( m_hEntities[i] );

		if ( !target )
		{
			m_hEntities[i] = NULL;
			continue;
		}

		CBaseEntity *source = SetOriginHandleEntity( m_hSource );

		if ( !source )
		{
			m_bActive = false;
			break;
		}

		int flags = pev->spawnflags;
		int axes = flags & SF_SETORIGIN_AXES;

		if ( flags & SF_SETORIGIN_ROTATE )
		{
			Vector forward, right, up;
			Vector xOffset = g_vecZero;
			Vector yOffset = g_vecZero;
			Vector zOffset = g_vecZero;

			AngleVectors( source->pev->angles, &forward, &right, &up );

			if ( ( flags & SF_SETORIGIN_ORIGIN_X ) || !axes )
				xOffset = forward * m_vecRelativeOffsets[i].x;

			if ( ( flags & SF_SETORIGIN_ORIGIN_Y ) || !axes )
				yOffset = right * m_vecRelativeOffsets[i].y;

			if ( ( flags & SF_SETORIGIN_ORIGIN_Z ) || !axes )
				zOffset = up * m_vecRelativeOffsets[i].z;

			Vector origin = ( ( source->pev->origin + xOffset ) + yOffset ) + zOffset;

			if ( ( flags & SF_SETORIGIN_ORIGIN_X ) || !axes )
				target->pev->origin.x = origin.x;

			if ( ( flags & SF_SETORIGIN_ORIGIN_Y ) || !axes )
				target->pev->origin.y = origin.y;

			if ( ( flags & SF_SETORIGIN_ORIGIN_Z ) || !axes )
				target->pev->origin.z = origin.z;
		}
		else
		{
			Vector origin = target->pev->origin;

			if ( ( flags & SF_SETORIGIN_ORIGIN_X ) || !axes )
				origin.x = source->pev->origin.x;

			if ( ( flags & SF_SETORIGIN_ORIGIN_Y ) || !axes )
				origin.y = source->pev->origin.y;

			if ( ( flags & SF_SETORIGIN_ORIGIN_Z ) || !axes )
				origin.z = source->pev->origin.z;

			target->pev->origin = origin + m_vecOffset;
		}

		CopyAngles( target, source, true );
		target->pev->angles = target->pev->angles + m_vecAngleOffset;
		target->pev->groundentity = NULL;
		target->pev->flags &= ~FL_ONGROUND;
		target->Teleport( target->pev->origin, target->pev->angles );
		target->OnSetOriginByMap();
		updated = true;
	}

	if ( !updated )
		m_bActive = false;
}

void CTriggerSetOrigin::UpdateTick( void )
{
	if ( m_bSetupPending )
		SetupEntities();

	if ( m_bActive )
		UpdateKnownEntities();
}

CTriggerSetOriginManager *CTriggerSetOriginManager::GetInstance( void )
{
	static CTriggerSetOriginManager *manager = NULL;

	if ( !manager )
		manager = new CTriggerSetOriginManager;

	return manager;
}

void CTriggerSetOriginManager::Add( CTriggerSetOrigin *entity )
{
	if ( !entity )
		return;

	Entry *last = NULL;

	for ( Entry *entry = m_pFirst; entry; entry = entry->m_pNext )
	{
		if ( SetOriginHandleEntity( entry->m_hEntity ) == entity )
			return;

		last = entry;
	}

	Entry *entry = new Entry;

	SetOriginStoreHandle( entry->m_hEntity, entity );

	if ( last )
		last->m_pNext = entry;
	else
		m_pFirst = entry;
}

void CTriggerSetOriginManager::Remove( CTriggerSetOrigin *entity )
{
	if ( !entity )
		return;

	Entry *previous = NULL;
	Entry *entry = m_pFirst;

	while ( entry )
	{
		if ( SetOriginHandleEntity( entry->m_hEntity ) == entity )
		{
			Entry *removed = entry;

			entry = entry->m_pNext;

			if ( previous )
				previous->m_pNext = entry;
			else
				m_pFirst = entry;

			delete removed;
		}
		else
		{
			previous = entry;
			entry = entry->m_pNext;
		}
	}
}

void CTriggerSetOriginManager::Update( void )
{
	Entry *previous = NULL;
	Entry *entry = m_pFirst;

	while ( entry )
	{
		CBaseEntity *entity = SetOriginHandleEntity( entry->m_hEntity );

		if ( entity )
		{
			static_cast<CTriggerSetOrigin *>( entity )->UpdateTick();
			previous = entry;
			entry = entry->m_pNext;
		}
		else
		{
			Entry *removed = entry;

			entry = entry->m_pNext;

			if ( previous )
				previous->m_pNext = entry;
			else
				m_pFirst = entry;

			delete removed;
		}
	}
}
