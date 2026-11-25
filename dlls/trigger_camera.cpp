#include "extdll.h"
#include "util.h"
#include "server_support.h"
#include "portal_trace.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "trigger_camera.h"
#include "trains.h"
#include "explode.h"
#include "CASServerManager.h"
#include "CASModule.h"
#include "CASFunction.h"
#include "CASLog.h"

#define SF_CAMERA_PLAYER_POSITION 1
#define SF_CAMERA_PLAYER_TARGET 2
#define SF_CAMERA_PLAYER_TAKECONTROL 4
#define SF_CAMERA_ALL_PLAYERS 8
#define SF_CAMERA_FORCE_VIEW 16
#define SF_CAMERA_SMOOTH_TURN 32
#define SF_CAMERA_INSTANT_MOVE 64
#define SF_CAMERA_MOUSE 128
#define SF_CAMERA_INVULNERABLE 256
#define SF_CAMERA_INFINITE_WAIT 512

#define SF_CAMERATARGET_INVISIBLE 1
#define SF_CAMERATARGET_START_OFF 2

#define PFLAG_CAMERA_FORCE_VIEW ( 1 << 8 )

#define CAMERA_CURSOR_OFF 0
#define CAMERA_CURSOR_DEFAULT 1
#define CAMERA_CURSOR_CUSTOM 2

enum CameraMouseEvent
{
	CAMERA_MOUSE_RELEASED = 0,
	CAMERA_MOUSE_PRESSED,
	CAMERA_MOUSE_DOUBLE_PRESSED,
	CAMERA_MOUSE_DRAGGED,
	CAMERA_MOUSE_MOVED,
	CAMERA_MOUSE_WHEEL
};

enum CameraMouseAction
{
	CAMERA_ACTION_USE_OFF = 1,
	CAMERA_ACTION_USE_ON,
	CAMERA_ACTION_USE_TOGGLE,
	CAMERA_ACTION_REMOVE,
	CAMERA_ACTION_DAMAGE,
	CAMERA_ACTION_HEAL,
	CAMERA_ACTION_CREATE,
	CAMERA_ACTION_PULL_ENTITY,
	CAMERA_ACTION_MOVE_TARGET,
	CAMERA_ACTION_EXPLODE
};

extern int gmsgCameraMouse;

LINK_ENTITY_TO_CLASS( trigger_camera, CTriggerCamera );
LINK_ENTITY_TO_CLASS( trigger_cameratarget, CTriggerCameraTarget );

void ParseCameraIntegers( int *values, int count, const char *text, char separator,
                                 char *remainder = NULL, int remainderSize = 0 )
{
	char buffer[128 + 1];

	strncpy( buffer, text, sizeof( buffer ) );
	buffer[sizeof( buffer ) - 1] = '\0';

	char *cursor = buffer;
	char *value = buffer;
	int index = 0;

	for ( ; index < count; index++ )
	{
		values[index] = atoi( value );

		while ( *cursor )
		{
			char character = *cursor++;

			if ( character == separator )
			{
				value = cursor;
				break;
			}
		}

		if ( cursor != value )
		{
			index++;
			break;
		}
	}

	for ( ; index < count; index++ )
		values[index] = 0;

	if ( remainder )
	{
		strncpy( remainder, value, remainderSize );
		remainder[remainderSize - 1] = '\0';
	}
}

void ParseCameraFloats( float *values, int count, const char *text, char separator )
{
	char buffer[128];

	strncpy( buffer, text, sizeof( buffer ) );
	buffer[sizeof( buffer ) - 1] = '\0';

	char *cursor = buffer;
	int index = 0;

	for ( ; index < count; index++ )
	{
		values[index] = atof( cursor );

		while ( *cursor && *cursor++ != separator )
		{
		}

		if ( !*cursor )
		{
			index++;
			break;
		}
	}

	for ( ; index < count; index++ )
		values[index] = 0;
}

bool CameraMouseKey( KeyValueData *data, int actions[3][2], string_t parameters[3][2],
                            bool blockDrag[3][2], string_t ( *targets )[2] = NULL )
{
	int indices[2];
	int kind;
	int prefixLength;

	if ( !strncmp( data->szKeyName, "mouse_action_", 13 ) )
	{
		kind = 0;
		prefixLength = 13;
	}
	else if ( !strncmp( data->szKeyName, "mouse_param_", 12 ) )
	{
		kind = 1;
		prefixLength = 12;
	}
	else if ( !strncmp( data->szKeyName, "mouse_block_drag_)", 17 ) )
	{
		kind = 2;
		prefixLength = 17;
	}
	else if ( targets && !strncmp( data->szKeyName, "mouse_target_", 13 ) )
	{
		kind = 3;
		prefixLength = 13;
	}
	else
	{
		return false;
	}

	ParseCameraIntegers( indices, 2, data->szKeyName + prefixLength, '_' );

	if ( (unsigned int) indices[0] <= 2 && (unsigned int) indices[1] <= 1 )
	{
		switch ( kind )
		{
			case 0:
				actions[indices[0]][indices[1]] = atoi( data->szValue );
				break;
			case 1:
				parameters[indices[0]][indices[1]] = AllocStringLocal( data->szValue );
				break;
			case 2:
				blockDrag[indices[0]][indices[1]] = atoi( data->szValue ) >= 1;
				break;
			case 3:
				targets[indices[0]][indices[1]] = AllocStringLocal( data->szValue );
				break;
		}

		data->fHandled = TRUE;
	}

	return true;
}

bool CameraClassnameIs( CBaseEntity *entity, const char *classname )
{
	edict_t *edict = NULL;

	if ( entity->pev )
		edict = entity->edict();

	return FClassnameIs( edict, classname );
}

void ResolveCameraCallback( CBaseEntity *camera, CASFunction *&function, string_t &name,
                                   const char *returnType, const char *parameters )
{
	if ( function || !name )
		return;

	CASModule *module = CASServerManager::GetInstance()->GetMapModule();

	if ( module )
	{
		char declaration[1024];

		if ( CASBaseCallable::FormatFunction( declaration, sizeof( declaration ), returnType,
		                                       STRING( name ), parameters ) )
		{
			function = module->GetFunction( declaration );

			if ( function )
			{
				function->AddRef();
				return;
			}

			CASLog::GetInstance()->Error( AS_LOG_DEFAULT,
			                              "%s (targetname: %s): Function with signature '%s' does not exist!\n",
			                              STRING( camera->pev->classname ), STRING( camera->pev->targetname ), declaration );
		}
		else
		{
			CASLog::GetInstance()->Warning( AS_LOG_DEFAULT,
			                                "%s (targetname: %s): Can't format function %s, name is too long (maximum length: %u)!",
			                                STRING( camera->pev->classname ), STRING( camera->pev->targetname ), STRING( name ),
			                                sizeof( declaration ) - CASBaseCallable::ComputeReservedSpace( "bool", parameters ) );
		}
	}
	else
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "%s (targetname: %s): Map module does not exist!\n",
		                              STRING( camera->pev->classname ), STRING( camera->pev->targetname ) );
	}

	name = 0;
}

CTriggerCamera::CTriggerCamera()
	: m_flDigitalActionReset( 0.01f ), m_flNextDigitalAction( 0 ),
	  m_iszASMouseEventCallbackName( 0 ), m_pMouseEventCallback( NULL ),
	  m_iszASKeyInputCallbackName( 0 ), m_pKeyInputCallback( NULL ),
	  m_iszOverridePlayerTargetname( 0 ), m_iszTargetWhenPlayerStartsUsing( 0 ),
	  m_iszTargetWhenPlayerStopsUsing( 0 ), m_iszTurnedOffTarget( 0 ),
	  m_iMaxPlayerCount( 0 ), m_iszMaxPlayerTarget( 0 ),
	  m_vecDragEntityOrigin( g_vecZero ), m_vecDragTraceOrigin( g_vecZero ), m_flDragDistance( 0 ),
	  m_bHideFlashlight( false ), m_bHideWeapons( false ), m_bHideHealth( false ),
	  m_iszCursorSprite( 0 ), m_bCopiedPlayerModel( false ), m_sPath( 0 ),
	  m_flWait( 0 ), m_flReturnTime( 0 ), m_flStopTime( 0 ), m_moveDistance( 0 ),
	  m_targetSpeed( 0 ), m_initialSpeed( 0 ), m_acceleration( 0 ), m_deceleration( 0 ),
	  m_turnspeed( 0 ), m_state( 0 )
{
	m_hDraggedEntity = NULL;
	m_hTarget = NULL;
	m_hPath = NULL;
}

CTriggerCamera::~CTriggerCamera()
{
	if ( m_pKeyInputCallback )
		m_pKeyInputCallback->Release();

	if ( m_pMouseEventCallback )
		m_pMouseEventCallback->Release();
}

void CTriggerCamera::OnCreate( void )
{
	for ( int button = 0; button < 3; button++ )
	{
		for ( int state = 0; state < 2; state++ )
		{
			m_iMouseActions[button][state] = 0;
			m_iszMouseParameters[button][state] = 0;
		}

		m_bMouseButtonDown[button] = false;
	}
}

TYPEDESCRIPTION CTriggerCamera::m_SaveData[] =
{
	DEFINE_FIELD( CTriggerCamera, m_hTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( CTriggerCamera, m_hPath, FIELD_EHANDLE ),
	DEFINE_FIELD( CTriggerCamera, m_sPath, FIELD_STRING ),
	DEFINE_FIELD( CTriggerCamera, m_flWait, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerCamera, m_flReturnTime, FIELD_TIME ),
	DEFINE_FIELD( CTriggerCamera, m_flStopTime, FIELD_TIME ),
	DEFINE_FIELD( CTriggerCamera, m_moveDistance, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerCamera, m_targetSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerCamera, m_initialSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerCamera, m_acceleration, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerCamera, m_deceleration, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerCamera, m_turnspeed, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerCamera, m_state, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CTriggerCamera, CBaseDelay );

void CTriggerCamera::Spawn( void )
{
	pev->movetype = MOVETYPE_NOCLIP;
	pev->solid = SOLID_NOT;							// Remove model & collisions
	pev->renderamt = 0;								// The engine won't draw this model if this is set to 0 and blending is on
	pev->rendermode = kRenderTransTexture;

	m_initialSpeed = pev->speed;
	if ( m_acceleration == 0 )
		m_acceleration = 500;
	if ( m_deceleration == 0 )
		m_deceleration = 500;

	if ( m_turnspeed <= 0 )
		m_turnspeed = 40;

	m_hDraggedEntity = NULL;
	m_vecDragEntityOrigin = g_vecZero;
	m_vecDragTraceOrigin = g_vecZero;
	m_flDragDistance = 0;
	m_bCopiedPlayerModel = false;

	if ( m_iszCursorSprite )
		UTIL_PrecacheModel( this, STRING( m_iszCursorSprite ) );
}

void CTriggerCamera :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "wait"))
		m_flWait = atof(pkvd->szValue);
	else if (FStrEq(pkvd->szKeyName, "moveto"))
		m_sPath = AllocStringLocal( pkvd->szValue );
	else if (FStrEq(pkvd->szKeyName, "acceleration"))
		m_acceleration = atof( pkvd->szValue );
	else if (FStrEq(pkvd->szKeyName, "deceleration"))
		m_deceleration = atof( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "turnspeed" ) )
		m_turnspeed = atof( pkvd->szValue );
	else if ( CameraMouseKey( pkvd, m_iMouseActions, m_iszMouseParameters, m_bMouseBlockDrag ) )
		return;
	else if ( FStrEq( pkvd->szKeyName, "mouse_digital_action_reset" ) )
		m_flDigitalActionReset = fmax( 0.01, atof( pkvd->szValue ) );
	else if ( FStrEq( pkvd->szKeyName, "m_iszASMouseEventCallbackName" ) )
		m_iszASMouseEventCallbackName = AllocStringLocal( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iszASKeyInputCallbackName" ) )
		m_iszASKeyInputCallbackName = AllocStringLocal( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iszOverridePlayerTargetname" ) )
		m_iszOverridePlayerTargetname = AllocStringLocal( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iszTargetWhenPlayerStartsUsing" ) )
		m_iszTargetWhenPlayerStartsUsing = AllocStringLocal( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iszTargetWhenPlayerStopsUsing" ) )
		m_iszTargetWhenPlayerStopsUsing = AllocStringLocal( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iszTurnedOffTarget" ) )
		m_iszTurnedOffTarget = AllocStringLocal( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "max_player_count" ) )
		m_iMaxPlayerCount = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "max_player_target" ) )
		m_iszMaxPlayerTarget = AllocStringLocal( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "hud_flashlight" ) )
		m_bHideFlashlight = atoi( pkvd->szValue ) == 1;
	else if ( FStrEq( pkvd->szKeyName, "hud_weapons" ) )
		m_bHideWeapons = atoi( pkvd->szValue ) == 1;
	else if ( FStrEq( pkvd->szKeyName, "hud_health" ) )
		m_bHideHealth = atoi( pkvd->szValue ) == 1;
	else if ( FStrEq( pkvd->szKeyName, "cursor_sprite" ) )
		m_iszCursorSprite = AllocStringLocal( pkvd->szValue );
	else
	{
		CBaseDelay::KeyValue( pkvd );
		return;
	}

	pkvd->fHandled = TRUE;
	}

void CTriggerCamera::EnableMouse( CBasePlayer *player )
{
	if ( !player || ( player->m_hCamera == this && player->m_bCameraMousePanelOpen ) )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgCameraMouse, NULL, player->edict() );

	if ( m_iszCursorSprite )
	{
		WRITE_BYTE( CAMERA_CURSOR_CUSTOM );
		WRITE_STRING( STRING( m_iszCursorSprite ) );
	}
	else
	{
		WRITE_BYTE( CAMERA_CURSOR_DEFAULT );
	}

	MESSAGE_END();
	player->m_bCameraMousePanelOpen = true;
}

void CTriggerCamera::DisableMouse( CBasePlayer *player )
{
	if ( !player )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgCameraMouse, NULL, player->edict() );
		WRITE_BYTE( CAMERA_CURSOR_OFF );
	MESSAGE_END();
	player->m_bCameraMousePanelOpen = false;
}

void CTriggerCamera::CameraPlayer( CBasePlayer *player )
{
	if ( !player )
		return;

	CTriggerCamera *previous = player->m_hCamera;

	if ( previous && previous != this )
		previous->UnCameraPlayer( player );

	if ( !m_bCopiedPlayerModel && player->pev->model )
	{
		SetModel_Intercept( edict(), STRING( player->pev->model ) );
		m_bCopiedPlayerModel = true;
	}

	if ( pev->spawnflags & SF_CAMERA_PLAYER_TAKECONTROL )
		player->EnableControl( FALSE );

	if ( pev->spawnflags & SF_CAMERA_FORCE_VIEW )
		player->m_afPhysicsFlags |= PFLAG_CAMERA_FORCE_VIEW;

	if ( pev->spawnflags & SF_CAMERA_INVULNERABLE )
		player->pev->takedamage = DAMAGE_NO;

	if ( player->m_iFOV != 0 && player->m_iFOV != 90 && player->m_pActiveItem )
	{
		CBasePlayerWeapon *weapon = (CBasePlayerWeapon *) player->m_pActiveItem->GetWeaponPtr();

		if ( weapon )
		{
			weapon->SecondaryAttack();
			weapon->m_fInZoom = false;
			player->m_iFOV = 0;
		}
	}

	SET_VIEW( player->edict(), edict() );
	player->m_afPhysicsFlags |= PFLAG_CAMERA;

	if ( pev->spawnflags & SF_CAMERA_MOUSE )
		EnableMouse( player );
	else
		DisableMouse( player );

	if ( m_iszOverridePlayerTargetname )
		player->pev->targetname = m_iszOverridePlayerTargetname;

	if ( m_iszTargetWhenPlayerStartsUsing && player->m_hCamera != this )
		FireTargets( STRING( m_iszTargetWhenPlayerStartsUsing ), player, this, USE_TOGGLE, 0 );

	player->m_hCamera = this;
}

void CTriggerCamera::UnCameraPlayer( CBasePlayer *player )
{
	if ( !player || player->m_hCamera != this )
		return;

	player->EnableControl( TRUE );
	player->m_afPhysicsFlags &= ~( PFLAG_CAMERA | PFLAG_CAMERA_FORCE_VIEW );
	SET_VIEW( player->edict(), player->edict() );
	player->pev->takedamage = DAMAGE_YES;
	player->m_hCamera = NULL;
	DisableMouse( player );

	if ( player->pev->targetname == m_iszOverridePlayerTargetname )
		player->pev->targetname = 0;

	if ( m_iszTargetWhenPlayerStopsUsing )
		FireTargets( STRING( m_iszTargetWhenPlayerStopsUsing ), player, this, USE_TOGGLE, 0 );
}

int CTriggerCamera::GetCurrentPlayerCount( void )
{
	int count = 0;

	for ( int index = 1; index <= gpGlobals->maxClients; index++ )
	{
		CBasePlayer *player = (CBasePlayer *) UTIL_PlayerByIndex( index );

		if ( player && player->m_hCamera == this )
			count++;
	}

	return count;
}

void CTriggerCamera::FindAllPlayers( void )
{
	for ( int index = 1; index <= gpGlobals->maxClients; index++ )
		CameraPlayer( (CBasePlayer *) UTIL_PlayerByIndex( index ) );
}

void CTriggerCamera::TurnOff( void )
{
	for ( int index = 1; index <= gpGlobals->maxClients; index++ )
		UnCameraPlayer( (CBasePlayer *) UTIL_PlayerByIndex( index ) );

	pev->avelocity = g_vecZero;
	pev->health = 0;
	m_state = 0;

	if ( m_iszKillTarget || m_iszTurnedOffTarget )
	{
		string_t target = pev->target;

		pev->target = m_iszTurnedOffTarget;
		SUB_UseTargets( this, USE_TOGGLE, 0 );
		pev->target = target;
	}
}

int CTriggerCamera::GetHideHUD( void )
{
	return ( m_bHideHealth ? 0x28 : 0 ) | ( m_bHideWeapons ? 1 : 0 ) | ( m_bHideFlashlight ? 2 : 0 );
}

void CTriggerCamera::Use( CBaseEntity *activator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetThink( &CTriggerCamera::FollowTarget );
	pev->nextthink = gpGlobals->time;

	int oldState = m_state;

	if ( ShouldToggle( useType, m_state != 0 ) )
		m_state = !m_state;

	CBasePlayer *pActivator;

	if ( activator && activator->IsPlayer() )
		pActivator = (CBasePlayer *) activator;
	else
		pActivator = NULL;

	if ( m_state == 1 )
	{
		if ( pev->spawnflags & SF_CAMERA_ALL_PLAYERS )
		{
			FindAllPlayers();
		}
		else if ( pActivator && pActivator->m_hCamera != this )
		{
			if ( m_iMaxPlayerCount <= 0 || GetCurrentPlayerCount() < m_iMaxPlayerCount )
				CameraPlayer( pActivator );
			else if ( m_iszMaxPlayerTarget )
				FireTargets( STRING( m_iszMaxPlayerTarget ), pActivator, this, USE_TOGGLE, 0 );
		}
	}
	else if (m_state == 0
	          && !( ( pev->spawnflags & SF_CAMERA_ALL_PLAYERS ) && ( !pActivator || pActivator->m_fConnected ) )
	          && oldState && pActivator && pActivator->m_hCamera == this )
	{
		UnCameraPlayer( pActivator );
		pActivator = NULL;

		if ( GetCurrentPlayerCount() > 0 )
			m_state = 1;
	}

	if ( !m_state )
	{
		TurnOff();
		return;
	}
	if ( oldState )
		return;

	m_flReturnTime = gpGlobals->time + m_flWait;
	pev->speed = m_initialSpeed;
	m_targetSpeed = m_initialSpeed;

	if ( pev->spawnflags & SF_CAMERA_PLAYER_TARGET )
		m_hTarget = pActivator;
	else if ( !(CBaseEntity *) m_hTarget )
		m_hTarget = GetNextTarget();

	if ( m_sPath )
	{
		m_hPath = UTIL_FindEntityByTargetname( NULL, STRING(m_sPath));
		UTIL_SetOrigin( pev, ( (CBaseEntity *) m_hPath )->Center() );
	}
	else
	{
		m_hPath = NULL;
	}

	m_flStopTime = gpGlobals->time;
	if ( (CBaseEntity *) m_hPath )
	{
		CBaseEntity *path = m_hPath;

		if ( path->pev->speed != 0 )
			m_targetSpeed = path->pev->speed;

		m_flStopTime += path->GetDelay();
	}

	// copy over player information
	if ( (pev->spawnflags & SF_CAMERA_PLAYER_POSITION ) && pActivator )
	{
		UTIL_SetOrigin( pev, pActivator->pev->origin + pActivator->pev->view_ofs );
		pev->angles = Vector( -pActivator->pev->angles.x, pActivator->pev->angles.y, 0 );
		pev->velocity = pActivator->pev->velocity;
	}
	else
	{
		pev->velocity = g_vecZero;

		if ( (CBaseEntity *) m_hTarget )
		{
			pev->angles = UTIL_VecToAngles( ( (CBaseEntity *) m_hTarget )->Center() - pev->origin );
	pev->angles.x = -pev->angles.x;
		}
	}

	m_moveDistance = 0;
	Move();
}

void CTriggerCamera::FollowTarget( void )
{
	if ( !m_state || ( !( pev->spawnflags & SF_CAMERA_INFINITE_WAIT ) && gpGlobals->time > m_flReturnTime ) )
	{
		TurnOff();
		return;
	}

	if ( pev->spawnflags & SF_CAMERA_ALL_PLAYERS )
	{
		FindAllPlayers();
	}
	else
	{
		for ( int index = 1; index <= gpGlobals->maxClients; index++ )
		{
			CBasePlayer *player = (CBasePlayer *) UTIL_PlayerByIndex( index );

	if ( player && player->m_hCamera == this )
	{
		if ( player->IsAlive( ) || ( pev->spawnflags & SF_CAMERA_FORCE_VIEW ) )
					CameraPlayer( player );
				else
					UnCameraPlayer( player );
			}
		}
	}

	if ( !(CBaseEntity *) m_hTarget && pev->target )
		m_hTarget = GetNextTarget();

	if ((CBaseEntity *) m_hTarget )
	{
		CBaseEntity *target = m_hTarget;
		Vector vecGoal = UTIL_VecToAngles( target->pev->origin - pev->origin );
	vecGoal.x = -vecGoal.x;

		if ( pev->spawnflags & SF_CAMERA_SMOOTH_TURN )
		{
			if (pev->angles.y > 360)
		pev->angles.y -= 360;

	if (pev->angles.y < 0)
		pev->angles.y += 360;

	float dx = vecGoal.x - pev->angles.x;
	float dy = vecGoal.y - pev->angles.y;

	if (dx < -180)
		dx += 360;
	if (dx > 180)
		dx -= 360;

	if (dy < -180)
		dy += 360;
	if (dy > 180)
		dy -= 360;

	pev->avelocity.x = dx * m_turnspeed * gpGlobals->frametime;
	pev->avelocity.y = dy * m_turnspeed * gpGlobals->frametime;
		}
		else
		{
		pev->angles = vecGoal;
		}
	}

	pev->nextthink = gpGlobals->time;

	pev->health = 0;

	for ( int index = 1; index <= gpGlobals->maxClients; index++ )
	{
		CBasePlayer *player = (CBasePlayer *) UTIL_PlayerByIndex( index );

		if ( player && player->m_hCamera == this )
		{
			pev->health = 1;
			break;
		}
	}

	Move();
}

void CTriggerCamera::Move( void )
{
	CBaseEntity *path = m_hPath;

	if (!path )
		return;

	// Subtract movement from the previous frame
	m_moveDistance -= pev->speed * gpGlobals->frametime;

	// Have we moved enough to reach the target?
	if ( m_moveDistance <= 0 )
	{
		// Fire the passtarget if there is one
		if ( path->pev->message )
		{
			FireTargets( STRING( path->pev->message), this, this, USE_TOGGLE, 0 );
			path = m_hPath;
			if ( path->pev->spawnflags & SF_CORNER_FIREONCE )
				path->pev->message = 0;
		}

		m_hPath = path->GetNextTarget();
		path = m_hPath;

		if ( !path )
		{
			pev->velocity = g_vecZero;
			return;
		}

		if ( path->pev->speed > 0 )
				m_targetSpeed = path->pev->speed;

			Vector delta = path->pev->origin - pev->origin;
			m_moveDistance = delta.Length();
			pev->movedir = delta.Normalize();
			m_flStopTime = gpGlobals->time + path->GetDelay();
		}

	if ( pev->spawnflags & SF_CAMERA_INSTANT_MOVE )
	{
		if ( m_flStopTime <= gpGlobals->time )
			pev->speed = m_targetSpeed;
		else
			pev->speed = 0;

		pev->velocity = pev->movedir * pev->speed;
	}
	else
	{
		if ( m_flStopTime <= gpGlobals->time )
			pev->speed = UTIL_Approach( m_targetSpeed, pev->speed, m_acceleration * gpGlobals->frametime );
		else
			pev->speed = UTIL_Approach( 0, pev->speed, m_deceleration * gpGlobals->frametime );

		float fraction = 2 * gpGlobals->frametime;
	pev->velocity = pev->movedir * pev->speed * fraction + pev->velocity * (1-fraction);
	}
}

void CTriggerCamera::MouseClicked( CBasePlayer *player, unsigned int event, unsigned int button,
                                   Vector2D position, Vector worldPosition )
{
	if ( !player || player->m_hCamera != this || !( player->m_afPhysicsFlags & PFLAG_CAMERA ) )
		return;

	const char *eventName;

	switch ( event )
	{
		case CAMERA_MOUSE_RELEASED:
			eventName = "released";
			break;
		case CAMERA_MOUSE_PRESSED:
			eventName = "pressed";
			break;
		case CAMERA_MOUSE_DOUBLE_PRESSED:
			eventName = "double pressed";
			break;
		case CAMERA_MOUSE_DRAGGED:
			eventName = "dragged";
			break;
		case CAMERA_MOUSE_MOVED:
			eventName = "moved";
			break;
		case CAMERA_MOUSE_WHEEL:
			eventName = "wheel scrolled";
			break;
		default:
			eventName = "unknown action";
			break;
	}

	char description[128];

	UTIL_FormatString( description, sizeof( description ), "[trigger_camera] Player #%d %s button %d at %.2f %.2f %.2f",
	         player->entindex(), eventName, button, worldPosition.x, worldPosition.y, worldPosition.z );
	ALERT( at_aiconsole, "%s: received.\n", description );

	if ( m_flNextDigitalAction > gpGlobals->time
	     && m_iMouseActions[button][0] != 0 && m_iMouseActions[button][0] != 255 )
	{
		ALERT( at_aiconsole, "%s blocked: Awaiting reset wait time. (%.2f remaining.)\n",
		       description, m_flNextDigitalAction - gpGlobals->time );
		return;
	}

	if ( event > 5 || ( event != 5 && button > 2 ) )
		return;

	int state = event == 2;

	if ( event != 4 && event != 5 && m_iMouseActions[button][state] == 255
	     && !( pev->spawnflags & SF_CAMERA_FORCE_VIEW ) )
	{
		UnCameraPlayer( player );
		return;
	}

	switch ( event )
	{
		case CAMERA_MOUSE_RELEASED:
			if ( !m_bMouseButtonDown[button] )
				return;

			m_hDraggedEntity = NULL;
			m_bMouseButtonDown[button] = false;
			break;
		case CAMERA_MOUSE_PRESSED:
			m_hDraggedEntity = NULL;
			m_bMouseButtonDown[button] = true;
			break;
		case CAMERA_MOUSE_DOUBLE_PRESSED:
			m_hDraggedEntity = NULL;
			m_bMouseButtonDown[button] = false;
			break;
		case CAMERA_MOUSE_MOVED:
			m_hDraggedEntity = NULL;
			m_bMouseButtonDown[0] = false;
			m_bMouseButtonDown[1] = false;
			m_bMouseButtonDown[2] = false;
			break;
	}

	Vector direction;

	if ( worldPosition == pev->origin )
		AngleVectors( pev->angles, &direction, NULL, NULL );
	else
		direction = worldPosition - pev->origin;

	direction = direction.Normalize();

	PortalTraceResult trace = {};

	UTIL_TracePortal( pev->origin, direction, 65536, dont_ignore_monsters, dont_ignore_glass,
	                  NULL, NULL, &trace );

	CBaseEntity *hit = NULL;

	if ( trace.pHit && !trace.pHit->free && GET_PRIVATE( trace.pHit ) )
		hit = CBaseEntity::Instance( trace.pHit );

	ResolveCameraCallback( this, m_pMouseEventCallback, m_iszASMouseEventCallbackName, "bool",
	                       "CBaseEntity@, CBaseEntity@, CBaseEntity@, int, int, float, float, Vector, Vector, Vector, float" );

	if ( m_pMouseEventCallback )
	{
		bool proceed = true;

		if ( !m_pMouseEventCallback->Call( &proceed, 0, this, player, hit, event, button,
		                                   position.x, position.y, &trace.vecEndPos, &trace.m_vecDirection,
		                                   &trace.vecPlaneNormal, trace.m_flScale ) )
		{
			CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "%s (targetname: %s): Failed in call to function '%s'!\n",
			                              STRING( pev->classname ), STRING( pev->targetname ), STRING( m_iszASMouseEventCallbackName ) );
			proceed = true;
		}

		if ( !proceed )
			return;
	}

	if ( event == 1 )
	{
		ALERT( at_aiconsole, "%s aborted: Nothing to do.\n", description );
		return;
	}

	if ( m_iMouseActions[button][0] != 0 && m_iMouseActions[button][0] != 255
	     && ( event == 0 || event == 2 || event == 5
	          || ( ( event == 3 || event == 4 ) && m_iMouseActions[button][0] != 11 ) ) )
		m_flNextDigitalAction = gpGlobals->time + m_flDigitalActionReset;

	if ( event == 4 || event == 5 )
		return;

	if ( event == 3 && ( m_iMouseActions[button][0] == 11 || m_iMouseActions[button][1] == 11 ) )
	{
		CBaseEntity *dragged = m_hDraggedEntity;

		if ( !dragged )
		{
			if ( !hit )
				return;

			m_hDraggedEntity = hit;
			m_flDragDistance = ( trace.m_Steps.at( 0 )->m_vecEnd - pev->origin ).Length();
			m_vecDragEntityOrigin = m_hDraggedEntity->pev->origin;
			m_vecDragTraceOrigin = trace.m_Steps.at( 0 )->m_vecEnd;
		}
		else
		{
			Vector dragDirection = ( trace.m_Steps.at( 0 )->m_vecEnd - pev->origin ).Normalize();

			m_hDraggedEntity->pev->origin = pev->origin + dragDirection * m_flDragDistance
			                       - m_vecDragTraceOrigin + m_vecDragEntityOrigin;
		}

		return;
	}

	if ( hit && CameraClassnameIs( hit, "trigger_cameratarget" ) )
	{
		( (CTriggerCameraTarget *) hit )->MouseClicked( player, event, button, position, worldPosition );
		return;
	}

	bool blocked = !hit || ( hit->IsBSPModel() && ( hit->pev->flags & FL_WORLDBRUSH ) )
	               || CameraClassnameIs( hit, "worldspawn" );

	if ( m_bMouseBlockDrag[button][state] && event == 3 )
	{
		ALERT( at_aiconsole, "%s blocked: Drag repeat action firing denied.\n", description );
		return;
	}

	int action = m_iMouseActions[button][state];

	if ( action == CAMERA_ACTION_CREATE || action == CAMERA_ACTION_PULL_ENTITY )
	{
		Vector zero = g_vecZero;
		CBaseEntity *entity = action == CAMERA_ACTION_PULL_ENTITY ? UTIL_FindEntityGeneric( STRING( m_iszMouseParameters[button][state] ), zero, 65536 ) : NULL;

		if ( action == CAMERA_ACTION_CREATE || entity )
		{
			for ( std::vector<PortalTraceStep *>::reverse_iterator it = trace.m_Steps.rbegin();
			      it != trace.m_Steps.rend(); it++ )
			{
				PortalTraceStep *step = *it;

				if ( step->m_flDistance > 100 )
				{
					Vector origin = step->m_vecEnd - step->m_vecDirection.Normalize() * 100;

					if ( action == CAMERA_ACTION_CREATE )
						CBaseEntity::Create( STRING( m_iszMouseParameters[button][state] ), origin, zero, false, edict() );
					else
					{
						entity->pev->origin = origin;
						entity->pev->velocity = zero;
					}

					break;
				}
			}
		}
	}
	else if ( action == CAMERA_ACTION_EXPLODE )
	{
		float offset = trace.m_Steps.back()->m_flDistance > 10 ? 10 : trace.flFraction * 65536 * 0.5;
		Vector origin = trace.vecEndPos - trace.m_vecDirection.Normalize() * offset;

		ExplosionCreate( origin, g_vecZero, player->edict(), (int) atof( STRING( m_iszMouseParameters[button][state] ) ), TRUE );
	}

	if ( blocked || !hit )
		return;

	switch ( m_iMouseActions[button][state] )
	{
		case CAMERA_ACTION_USE_OFF:
			hit->Use( player, this, USE_OFF, atof( STRING( m_iszMouseParameters[button][state] ) ) );
			break;
		case CAMERA_ACTION_USE_ON:
			hit->Use( player, this, USE_ON, atof( STRING( m_iszMouseParameters[button][state] ) ) );
			break;
		case CAMERA_ACTION_USE_TOGGLE:
			hit->Use( player, this, USE_TOGGLE, atof( STRING( m_iszMouseParameters[button][state] ) ) );
			break;
		case CAMERA_ACTION_REMOVE:
			UTIL_Remove( hit );
			break;
		case CAMERA_ACTION_DAMAGE:
			hit->TakeDamage( pev, player->pev, atof( STRING( m_iszMouseParameters[button][state] ) ), DMG_GENERIC );
			break;
		case CAMERA_ACTION_HEAL:
			hit->TakeHealth( atof( STRING( m_iszMouseParameters[button][state] ) ), DMG_MEDKITHEAL, 0 );
			break;
		case CAMERA_ACTION_MOVE_TARGET:
		{
			Vector origin = g_vecZero;
			CBaseEntity *destination = UTIL_FindEntityGeneric( STRING( m_iszMouseParameters[button][state] ), origin, 65536 );

			if ( destination )
				origin = destination->pev->origin;
			else
				ParseCameraFloats( &origin.x, 3, STRING( m_iszMouseParameters[button][state] ), ' ' );

			hit->pev->origin = origin;
			hit->pev->velocity = g_vecZero;
			break;
		}
	}
}

void CTriggerCamera::KeyInput( CBasePlayer *player, int down, int keynum, const char *binding )
{
	if ( !player || player->m_hCamera != this || !( player->m_afPhysicsFlags & PFLAG_CAMERA ) )
		return;

	ResolveCameraCallback( this, m_pKeyInputCallback, m_iszASKeyInputCallbackName, "void",
	                       "CBaseEntity@, CBasePlayer@, bool, KeyCode" );

	if ( m_pKeyInputCallback && !m_pKeyInputCallback->Call( 0, this, player, down != 0, keynum ) )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "%s (targetname: %s): Failed in call to function '%s'!\n",
		                              STRING( pev->classname ), STRING( pev->targetname ), STRING( m_iszASKeyInputCallbackName ) );
	}
}

CTriggerCameraTarget::CTriggerCameraTarget()
	: m_flDigitalActionReset( 0.01f ), m_flNextDigitalAction( 0 )
{
}

void CTriggerCameraTarget::Spawn( void )
{
	pev->movetype = MOVETYPE_PUSH;
	pev->solid = SOLID_BSP;
	pev->flags |= FL_WORLDBRUSH;
	SetModel_Intercept( edict(), STRING( pev->model ) );

	if ( pev->spawnflags & SF_CAMERATARGET_INVISIBLE )
		pev->effects |= EF_NODRAW;

	if ( !( pev->spawnflags & SF_CAMERATARGET_START_OFF ) )
		m_bActive = true;
}

void CTriggerCameraTarget::KeyValue( KeyValueData *data )
{
	if ( CameraMouseKey( data, m_iMouseActions, m_iszMouseParameters, m_bMouseBlockDrag, m_iszMouseTargets ) )
		return;

	if ( FStrEq( data->szKeyName, "mouse_digital_action_reset" ) )
	{
		m_flDigitalActionReset = fmax( 0.01, atof( data->szValue ) );
		data->fHandled = TRUE;
	}
	else
	{
		CBaseDelay::KeyValue( data );
	}
}

void CTriggerCameraTarget::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( ShouldToggle( useType, m_bActive ) )
		m_bActive = !m_bActive;
}

void CTriggerCameraTarget::MouseClicked( CBasePlayer *player, int event, int button,
                                         Vector2D position, Vector worldPosition )
{
	if ( !( m_flNextDigitalAction <= gpGlobals->time ) || !player || !m_bActive )
		return;

	int state = event == 2;

	if ( m_bMouseBlockDrag[button][state] && ( event == 4 || event == 3 ) )
	{
		ALERT( at_aiconsole, "camera_target MouseClicked blocked: Drag repeat action firing denied.\n" );
		return;
	}

	int action = m_iMouseActions[button][state];

	if ( action >= CAMERA_ACTION_USE_OFF && action <= CAMERA_ACTION_USE_TOGGLE )
	{
		USE_TYPE type = action == CAMERA_ACTION_USE_OFF ? USE_OFF : action == CAMERA_ACTION_USE_ON ? USE_ON : USE_TOGGLE;

		FireTargets( STRING( m_iszMouseTargets[button][state] ), player, this, type, atof( STRING( m_iszMouseParameters[button][state] ) ) );
	}
	else if ( action >= CAMERA_ACTION_REMOVE && action <= CAMERA_ACTION_HEAL )
	{
		CBaseEntity *entity = NULL;

		while ( ( entity = UTIL_FindEntityByTargetname( entity, STRING( m_iszMouseTargets[button][state] ) ) ) != NULL )
		{
			if ( action == CAMERA_ACTION_REMOVE )
				UTIL_Remove( entity );
			else if ( action == CAMERA_ACTION_DAMAGE )
				entity->TakeDamage( pev, player->pev, atof( STRING( m_iszMouseParameters[button][state] ) ), DMG_GENERIC );
			else
				entity->TakeHealth( atof( STRING( m_iszMouseParameters[button][state] ) ), DMG_MEDKITHEAL, 0 );
		}
	}

	m_flNextDigitalAction = gpGlobals->time + m_flDigitalActionReset;
}

void CTriggerCameraTarget::MouseMoved( CBasePlayer *player, Vector2D position, Vector worldPosition )
{
}

void CTriggerCameraTarget::MouseWheeled( CBasePlayer *player, int delta )
{
}

bool HandleCameraCommand( CBasePlayer *player, const char *command )
{
	if ( !strncmp( command, "cam_mouse_", 10 ) )
	{
		if ( player->m_hCamera )
		{
			int values[2];
			float coordinates[5];
			char remainder[128];

			ParseCameraIntegers( values, 2, command + 10, '_', remainder, sizeof( remainder ) );
			ParseCameraFloats( coordinates, 5, remainder, '_' );
			player->m_hCamera->MouseClicked( player, values[0], values[1],
			                                 Vector2D( coordinates[0], coordinates[1] ),
			                                 Vector( coordinates[2], coordinates[3], coordinates[4] ) );
		}

		return true;
	}

	if ( FStrEq( command, "cam_key" ) )
	{
		if ( player->m_hCamera && CMD_ARGC() > 2 )
		{
			int down = atoi( CMD_ARGV( 1 ) );
			int keynum = atoi( CMD_ARGV( 2 ) );
			player->m_hCamera->KeyInput( player, down, keynum, NULL );
		}

		return true;
	}

	return false;
}
