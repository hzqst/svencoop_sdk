#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "portal.h"
#include "portal_trace.h"
#include "user_messages.h"
#include "pm_defs.h"
#include "pm_shared.h"
#include "teleportdestination.h"
#include <math.h>

struct glpoly_s
{
	glpoly_s	*next;
	glpoly_s	*chain;
	int			numverts;
	int			flags;
	float	verts[4][7];
};

PortalSet globalPortals;

LINK_ENTITY_TO_CLASS( func_portal, CPortal );
LINK_ENTITY_TO_CLASS( func_mirror, CPortal );
LINK_ENTITY_TO_CLASS( func_monitor, CPortal );

CPortal::CPortal()
    : m_iszModel( 0 ), m_bActive( true ), m_iszTarget( 0 ), m_bSourceChanged( false ),
      m_iPortalMode( PORTAL_MIRROR ), m_flZoom( 1 ), m_iFPS( 0 ), m_iMinDistance( 0 ),
      m_iMaxDistance( 0 ), m_iTextureMode( PORTAL_TEXTURE_TRANSPARENT ), m_iszTextureName( 0 ),
      m_uiWidth( 512 ), m_uiHeight( 512 ), m_bOldNoPlanes( false ), m_bOldShootThrough( false ),
      m_bOldHalfFrame( false ), m_bOldActive( true ), m_vecOldOrigin( 0, 0, 0 ),
      m_vecOldAngles( 0, 0, 0 ), m_iOldPortalMode( PORTAL_MIRROR ),
      m_iOldTextureMode( PORTAL_TEXTURE_TRANSPARENT ), m_flOldZoom( 1 ), m_bOldSourceInvalid( true ),
      m_vecOldSourceOrigin( 0, 0, 0 ), m_vecOldSourceAngles( 0, 0, 0 ), m_uiOldWidth( 512 ),
      m_uiOldHeight( 512 ), m_iOldFPS( 0 ), m_iOldMinDistance( 0 ), m_iOldMaxDistance( 0 )
{
}

CPortal::~CPortal()
{
	globalPortals.erase( EntityHandle<CPortal>( this ) );
}

void CPortal::Spawn( void )
{
	pev->flags |= FL_WORLDBRUSH;

	if ( pev->spawnflags & SF_PORTAL_MOVABLE )
	{
		pev->movetype = MOVETYPE_PUSH;
		pev->solid = SOLID_BSP;
	}
	else
	{
		pev->movetype = MOVETYPE_NOCLIP;
		pev->solid = SOLID_TRIGGER;
		SetTouch( NULL );
	}

	if ( FClassnameIs( pev, "func_portal" ) )
	{
		m_iPortalMode = PORTAL_PORTAL;
		pev->spawnflags |= SF_PORTAL_SHOOT_THROUGH;
	}
	else if ( FClassnameIs( pev, "func_mirror" ) )
		m_iPortalMode = PORTAL_MIRROR;
	else if ( FClassnameIs( pev, "func_monitor" ) )
		m_iPortalMode = PORTAL_MONITOR;

	pev->rendermode = kRenderTransAlpha;
	pev->renderamt = 255;
	UTIL_SetOrigin( pev, pev->origin );
	SetModel_Intercept( edict(), STRING( pev->model ) );
	globalPortals.insert( EntityHandle<CPortal>( this ) );

	m_bActive = !( pev->spawnflags & SF_PORTAL_START_OFF );
	SetThink( &CPortal::PortalThink );
	pev->nextthink = pev->ltime + RANDOM_FLOAT( 0.1f, 1.1f );
}

void CPortal::KeyValue( KeyValueData *data )
{
	if ( FStrEq( data->szKeyName, "textureMode" ) )
	{
		int mode = atoi( data->szValue );

		if ( mode >= PORTAL_TEXTURE_TRANSPARENT && mode <= PORTAL_TEXTURE_NAMED )
			m_iTextureMode = mode;
	}
	else if ( FStrEq( data->szKeyName, "textureName" ) )
		m_iszTextureName = AllocStringLocal( data->szValue );
	else if ( FStrEq( data->szKeyName, "zoom" ) )
	{
		double zoom = fmin( 1000.0, atof( data->szValue ) );

		m_flZoom = zoom >= (double) 0.001f ? (float) zoom : 0.001f;
	}
	else if ( FStrEq( data->szKeyName, "width" ) )
		m_uiWidth = atoi( data->szValue );
	else if ( FStrEq( data->szKeyName, "height" ) )
		m_uiHeight = atoi( data->szValue );
	else if ( FStrEq( data->szKeyName, "fps" ) )
		m_iFPS = atoi( data->szValue );
	else if ( FStrEq( data->szKeyName, "mindist" ) )
		m_iMinDistance = V_max( atoi( data->szValue ), 0 );
	else if ( FStrEq( data->szKeyName, "maxdist" ) )
		m_iMaxDistance = V_max( atoi( data->szValue ), 0 );
	else
	{
		CBaseEntity::KeyValue( data );
		return;
	}

	data->fHandled = TRUE;
}

void CPortal::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_bActive ) )
		return;

	m_bActive = !m_bActive;
	PortalThink();
	pev->nextthink = pev->ltime + RANDOM_FLOAT( 0.1f, 1.1f );
}

void CPortal::Touch( CBaseEntity *other )
{
	CBaseEntity *source = m_hSource;

	if ( source && m_iPortalMode == PORTAL_PORTAL && ( pev->spawnflags & SF_PORTAL_TELEPORT ) )
		UTIL_TeleportEntity( other, source, SF_TELEPORT_RELATIVE | SF_TELEPORT_KEEP_ANGLES | SF_TELEPORT_KEEP_VELOCITY | SF_TELEPORT_ROTATE,
			pev->origin );
}

void CPortal::PortalThink( void )
{
	if ( m_bActive )
	{
		if ( pev->model != m_iszModel )
			InitModel();

		CBaseEntity *source = m_hSource;

		if ( m_iPortalMode != PORTAL_MIRROR )
		{
			if ( !source || pev->target != m_iszTarget )
			{
				m_hSource = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );
				m_iszTarget = pev->target;
				m_bSourceChanged = true;
			}
		}
		else if ( !source )
			m_hSource = this;
	}

	SendClientUpdate();
	SetThink( &CPortal::PortalThink );
	pev->nextthink = pev->ltime + 1.0f;
}

EntityHandle<CBaseEntity> CPortal::GetSource( void )
{
	return (CBaseEntity *) m_hSource ? m_hSource : EntityHandle<CBaseEntity>( this );
}

const Vector &CPortal::GetSourceOrigin( void )
{
	CBaseEntity *source = GetSource();

	return source->pev->origin;
}

const Vector &CPortal::GetSourceAngles( void )
{
	CBaseEntity *source = GetSource();

	return source->pev->angles;
}

bool CPortal::IsPortalTexture( texture_s *texture )
{
	switch ( m_iTextureMode )
	{
		case PORTAL_TEXTURE_TRANSPARENT:
			return texture->name[0] == '{';

		case PORTAL_TEXTURE_ALL:
			return true;

		case PORTAL_TEXTURE_NAMED:
			return stricmp( texture->name, STRING( m_iszTextureName ) ) == 0;
	}

	return false;
}

void CPortal::InitModel( void )
{
	if ( pev->model == m_iszModel )
		return;

	playermove_t *move = PM_GetPlayerMove();

	if ( !move || !move->physents[0].model )
		return;

	const char *name = STRING( pev->model );

	if ( !name || name[0] != '*' )
		return;

	int index = atoi( name + 1 );

	if ( index <= 0 )
		return;

	model_t *model = move->physents[0].model + index;

	if ( strcmp( model->name, name ) != 0 )
		return;

	m_PortalSurfaces.clear();

	for ( int i = 0; i < model->nummodelsurfaces; i++ )
	{
		msurface_t *surface = model->surfaces + model->firstmodelsurface + i;

		if ( !surface->texinfo || !surface->texinfo->texture )
			continue;

		if ( IsPortalTexture( surface->texinfo->texture ) )
		{
			m_PortalSurfaces.insert( surface );
		}
	}

	m_iszModel = pev->model;
}

void CPortal::SendClientUpdate( void )
{
	if ( CheckValuesHaveChanged() )
		m_ClientUpdates.clear();

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if ( !player )
			continue;

		EntityHandle<CBasePlayer> handle( player );
		ClientUpdateMap::iterator previous = m_ClientUpdates.find( handle );
		float epoch = player->m_flLastClientPutInServerTime;

		if ( previous != m_ClientUpdates.end() && previous->second == epoch )
			continue;

		if ( gpGlobals->time < epoch + 1.0f )
			continue;

		m_ClientUpdates[handle] = epoch;
		MESSAGE_BEGIN( MSG_ONE, gmsgPortalUpdate, NULL, player->edict() );
		WRITE_LONG( entindex() );
		WRITE_BYTE( m_bActive );

		if ( m_bActive )
		{
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_COORD( pev->angles.x );
			WRITE_COORD( pev->angles.y );
			WRITE_COORD( pev->angles.z );
			WRITE_BYTE( m_iPortalMode );
			WRITE_BYTE( m_iTextureMode );

			unsigned char zoomBytes[sizeof( m_flZoom )];

			memcpy( zoomBytes, &m_flZoom, sizeof( zoomBytes ) );

			for ( unsigned int j = 0; j < sizeof( zoomBytes ); j++ )
				WRITE_BYTE( zoomBytes[j] );

			WRITE_BYTE( m_iFPS );
			WRITE_LONG( m_iMinDistance );
			WRITE_LONG( m_iMaxDistance );
			WRITE_BYTE( ( pev->spawnflags & SF_PORTAL_HALF_FRAME ) != 0 );

			CBaseEntity *source = m_hSource;

			if ( m_iPortalMode != PORTAL_MIRROR )
			{
				WRITE_BYTE( source != NULL );

				if ( source )
				{
					WRITE_LONG( source->entindex() );
					WRITE_COORD( source->pev->origin.x );
					WRITE_COORD( source->pev->origin.y );
					WRITE_COORD( source->pev->origin.z );
					WRITE_ANGLE( source->pev->angles.x );
					WRITE_ANGLE( source->pev->angles.y );
					WRITE_ANGLE( source->pev->angles.z );
				}
			}

			if ( m_iPortalMode == PORTAL_MONITOR )
			{
				bool customSize = (int) m_uiWidth > 0 && (int) m_uiHeight > 0;

				WRITE_BYTE( customSize );

				if ( customSize )
				{
					WRITE_LONG( m_uiWidth );
					WRITE_LONG( m_uiHeight );
				}
			}
			else if ( m_iPortalMode == PORTAL_PORTAL )
			{
				WRITE_BYTE( ( pev->spawnflags & SF_PORTAL_NO_PLANES ) != 0 );
				WRITE_BYTE( ( pev->spawnflags & SF_PORTAL_SHOOT_THROUGH ) != 0 );
			}

			if ( m_iTextureMode == PORTAL_TEXTURE_NAMED )
				WRITE_STRING( STRING( m_iszTextureName ) );
		}

		MESSAGE_END();
	}

	BackupValues();
}

bool CPortal::CheckValuesHaveChanged( void )
{
	if ( m_iOldTextureMode != m_iTextureMode )
	{
		m_iszModel = 0;
		InitModel();
		return true;
	}

	if ( m_bOldActive != m_bActive || m_iOldPortalMode != m_iPortalMode || m_flOldZoom != m_flZoom ||
	     m_iOldFPS != m_iFPS || m_iOldMinDistance != m_iMinDistance || m_iOldMaxDistance != m_iMaxDistance )
	{
		return true;
	}

	if ( m_bOldHalfFrame != ( ( pev->spawnflags & SF_PORTAL_HALF_FRAME ) != 0 ) ||
	     pev->origin != m_vecOldOrigin || pev->angles != m_vecOldAngles )
	{
		return true;
	}

	if ( m_iPortalMode != PORTAL_MIRROR )
	{
		CBaseEntity *oldSource = m_hOldSource;
		CBaseEntity *source = m_hSource;

		if ( m_bOldSourceInvalid != ( oldSource == NULL ) || source != oldSource )
			return true;

		if ( source && ( source->pev->origin != m_vecOldSourceOrigin || source->pev->angles != m_vecOldSourceAngles ) )
			return true;

		if ( m_iPortalMode == PORTAL_MONITOR && ( m_uiOldWidth != m_uiWidth || m_uiOldHeight != m_uiHeight ) )
			return true;

		if ( m_iPortalMode == PORTAL_PORTAL &&
		     ( m_bOldNoPlanes != ( ( pev->spawnflags & SF_PORTAL_NO_PLANES ) != 0 ) ||
		       m_bOldShootThrough != ( ( pev->spawnflags & SF_PORTAL_SHOOT_THROUGH ) != 0 ) ) )
		{
			return true;
		}
	}

	return m_bSourceChanged;
}

void CPortal::BackupValues( void )
{
	m_bOldActive = m_bActive;
	m_vecOldOrigin = pev->origin;
	m_vecOldAngles = pev->angles;
	m_iOldPortalMode = m_iPortalMode;
	m_iOldTextureMode = m_iTextureMode;
	m_flOldZoom = m_flZoom;
	m_iOldFPS = m_iFPS;
	m_iOldMinDistance = m_iMinDistance;
	m_iOldMaxDistance = m_iMaxDistance;
	m_hOldSource = m_hSource;

	CBaseEntity *source = m_hSource;

	m_bOldSourceInvalid = source == NULL;

	if ( source )
	{
		m_vecOldSourceOrigin = source->pev->origin;
		m_vecOldSourceAngles = source->pev->angles;
	}

	m_uiOldWidth = m_uiWidth;
	m_uiOldHeight = m_uiHeight;
	m_bOldNoPlanes = ( pev->spawnflags & SF_PORTAL_NO_PLANES ) != 0;
	m_bOldShootThrough = ( pev->spawnflags & SF_PORTAL_SHOOT_THROUGH ) != 0;
	m_bOldHalfFrame = ( pev->spawnflags & SF_PORTAL_HALF_FRAME ) != 0;
	m_bSourceChanged = false;
}

bool PointInPolygon( const Vector &point, const glpoly_s *poly, const Vector &normal )
{
	int firstAxis;
	int secondAxis;

	if ( fabs( normal.x ) > fabs( normal.y ) && fabs( normal.x ) > fabs( normal.z ) )
	{
		firstAxis = 1;
		secondAxis = 2;
	}
	else
	{
		firstAxis = 0;
		secondAxis = fabs( normal.y ) > fabs( normal.z ) ? 2 : 1;
	}

	bool inside = false;

	for ( int i = 0, previous = poly->numverts - 1; i < poly->numverts; previous = i++ )
	{
		const float *current = poly->verts[i];
		const float *last = poly->verts[previous];

		if ( ( current[secondAxis] > point[secondAxis] ) != ( last[secondAxis] > point[secondAxis] ) &&
		     ( last[firstAxis] - current[firstAxis] ) * ( point[secondAxis] - current[secondAxis] ) /
		                 ( last[secondAxis] - current[secondAxis] ) +
		             current[firstAxis] >
		         point[firstAxis] )
		{
			inside = !inside;
		}
	}

	return inside;
}

void P_TraceSurface( edict_t *entity, CPortal *portal, const Vector &start, const Vector &direction,
                     float &distance, PortalSurfaceTrace &trace )
{
	Vector localStart = start - portal->GetOrigin();
	Vector localDirection = direction;
	Vector rotation = -portal->GetAngles();
	Vector hitPosition( 0, 0, 0 );
	Vector hitNormal( 0, 0, 0 );
	msurface_t *hitSurface = NULL;

	UTIL_RotateVector( &localStart, rotation, g_vecZero, true );
	UTIL_RotateVector( &localDirection, rotation, g_vecZero, true );

	const CPortal::SurfaceSet &surfaces = portal->GetPortalSurfaces();

	for ( CPortal::SurfaceSet::const_iterator current = surfaces.begin(); current != surfaces.end(); current++ )
	{
		msurface_t *surface = *current;
		glpoly_s *poly = surface->polys;
		Vector normal( 0, 0, 0 );
		Vector first( poly->verts[0] );

		for ( int i = 2; i < poly->numverts; i++ )
		{
			Vector third( poly->verts[i] );

			normal = CrossProduct( third - Vector( poly->verts[1] ), third - first );

			if ( normal.Length() > 0.01f )
			{
				normal = normal.Normalize();
				break;
			}

			normal = Vector( 0, 0, 0 );
		}

		float facing = DotProduct( normal, localDirection );

		if ( facing >= -0.01f )
			continue;

		float intersection = DotProduct( first - localStart, normal ) / facing;

		if ( !( intersection > 0.01f ) || !( distance + 1.0f >= intersection ) )
			continue;

		Vector position = localStart + localDirection * intersection;

		if ( PointInPolygon( position, poly, normal ) )
		{
			distance = intersection;
			hitSurface = surface;
			hitPosition = position;
			hitNormal = normal;
		}
	}

	if ( !hitSurface )
		return;

	UTIL_RotateVector( &hitPosition, portal->GetAngles(), g_vecZero, false );
	UTIL_RotateVector( &hitNormal, portal->GetAngles(), g_vecZero, false );
	trace.m_bHit = true;
	trace.m_pPortal = portal;
	trace.m_flScale = portal->GetZoom();
	trace.m_vecPosition = hitPosition + portal->GetOrigin();
	trace.m_vecNormal = -hitNormal;
}
