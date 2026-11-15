#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "saverestore.h"
#include "op4mortar.h"

#define SF_MORTAR_ACTIVE 1
#define SF_MORTAR_LINE_OF_SIGHT 16
#define SF_MORTAR_CONTROLLABLE 32

LINK_ENTITY_TO_CLASS( op4mortar, COp4Mortar );
LINK_ENTITY_TO_CLASS( func_op4mortarcontroller, COp4MortarController );

TYPEDESCRIPTION COp4Mortar::m_SaveData[] =
{
	DEFINE_FIELD( COp4Mortar, m_flDelay, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( COp4Mortar, CBaseMonster );

CBaseEntity *Op4MortarHandleEntity( EHANDLE &handle )
{
	edict_t *entity = handle.Get();

	return entity && !entity->free ? (CBaseEntity *) GET_PRIVATE( entity ) : NULL;
}

void Op4MortarSetHandle( EHANDLE &handle, CBaseEntity *entity )
{
	handle = EHANDLE();

	if ( entity && entity->pev && entity->pev->pContainingEntity && !entity->pev->pContainingEntity->free )
		handle.Set( entity->pev->pContainingEntity );
}

Vector VecCheckMortar( entvars_t *pev, const Vector &start, Vector target, float gravityScale )
{
	float gravity = CVAR_GET_FLOAT( "sv_gravity" ) * gravityScale;

	if ( target.z - start.z > 2000 )
		return g_vecZero;

	UTIL_MakeVectors( pev->angles );

	float spread = RANDOM_FLOAT( -16, 16 );

	spread += RANDOM_FLOAT( -8, 8 );
	target = target + gpGlobals->v_right * spread;

	spread = RANDOM_FLOAT( -16, 16 );
	spread += RANDOM_FLOAT( -8, 8 );
	target = target + gpGlobals->v_forward * spread;

	float height = (float) ( (double) ( target - start ).Length() * 0.5 );
	Vector midpoint = start + ( target - start ) * 0.5f;
	Vector originalMidpoint = midpoint;
	TraceResult trace;

	UTIL_TraceLine( midpoint, midpoint + Vector( 0, 0, height ), ignore_monsters, ENT( pev ), &trace );
	midpoint = trace.vecEndPos;

	int contents = UTIL_PointContents( midpoint );

	while ( contents != CONTENTS_EMPTY && midpoint.z > originalMidpoint.z )
	{
		midpoint.z -= 128;
		contents = UTIL_PointContents( midpoint );
	}

	midpoint.z -= 32;

	if ( start.z > midpoint.z || target.z > midpoint.z )
		return g_vecZero;

	double halfGravity = (double) gravity * 0.5;
	float riseTime = (float) sqrt( (double) ( midpoint.z - start.z ) / halfGravity );

	if ( (double) riseTime < 0.1 )
		return g_vecZero;

	float fallTime = (float) sqrt( (double) ( midpoint.z - target.z ) / halfGravity );
	float flightTime = riseTime + fallTime;
	Vector velocity( ( target.x - start.x ) / flightTime, ( target.y - start.y ) / flightTime,
	                 gravity * riseTime );
	Vector apex( start.x + velocity.x * riseTime, start.y + velocity.y * riseTime, midpoint.z );

	UTIL_TraceLine( start, apex, ignore_monsters, ENT( pev ), &trace );

	if ( trace.flFraction != 1 )
		return g_vecZero;

	UTIL_TraceLine( target, apex, ignore_monsters, ENT( pev ), &trace );

	if ( trace.flFraction != 1 )
		return g_vecZero;

	return velocity;
}

COp4Mortar::COp4Mortar()
    : m_flHorizontalCenter( 0 ), m_flHorizontalMax( 0 ), m_flHorizontalMin( 0 ),
      m_flMinDist( 0 ), m_flMaxDist( 0 ), m_flMortarVelocity( 0 ), m_flDelay( 0 ), m_bActive( false ),
      m_vecVelocity( 0, 0, 0 ), m_vecController( 0, 0, 0 ), m_flNextFire( 0 ), m_vecIdealAngles( 0, 0, 0 )
{
}

COp4Mortar::~COp4Mortar()
{
}

int COp4Mortar::Classify( void )
{
	return GetClassification( CLASS_HUMAN_MILITARY );
}

void COp4Mortar::KeyValue( KeyValueData *data )
{
	if ( FStrEq( data->szKeyName, "mindist" ) )
		m_flMinDist = atof( data->szValue );
	else if ( FStrEq( data->szKeyName, "maxdist" ) )
		m_flMaxDist = atof( data->szValue );
	else if ( FStrEq( data->szKeyName, "mortar_velocity" ) )
		m_flMortarVelocity = atof( data->szValue );
	else if ( FStrEq( data->szKeyName, "firedelay" ) )
		m_flDelay = atof( data->szValue );
	else if ( FStrEq( data->szKeyName, "h_min" ) )
		m_flHorizontalMin = atof( data->szValue );
	else if ( FStrEq( data->szKeyName, "h_max" ) )
		m_flHorizontalMax = atof( data->szValue );
	else
	{
		CBaseMonster::KeyValue( data );
		return;
	}

	data->fHandled = TRUE;
}

void COp4Mortar::Precache( void )
{
	CBaseEntity::Precache();
	PrecacheSound_Intercept( this, "weapons/mortar.wav" );
	PrecacheSound_Intercept( this, "weapons/mortarhit.wav" );
	PrecacheSound_Intercept( this, "weapons/ofmortar.wav" );
	PrecacheModel_Intercept( this, "models/mortar.mdl" );
	PrecacheModel_Intercept( this, "models/mortarshell.mdl" );
}

void COp4Mortar::Spawn( void )
{
	m_bActive = ( pev->spawnflags & SF_MORTAR_ACTIVE ) || !pev->targetname;

	Precache();
	SetModel_Intercept( edict(), "models/mortar.mdl" );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->framerate = 1;
	pev->view_ofs = Vector( 0, 0, 96 );
	m_flFieldOfView = -1;
	pev->flags |= FL_MONSTER;

	m_flHorizontalCenter = m_flHorizontalMax + m_flHorizontalMin;
	m_vecController = Vector( 0, m_flHorizontalCenter, 0 );
	m_flHorizontalMax = m_flHorizontalMax < 90 ? m_flHorizontalMax : 90;
	m_flHorizontalMin = m_flHorizontalMin > -90 ? m_flHorizontalMin : -90;
	SetBoneController( 0, 0 );
	SetBoneController( 1, m_vecController.y );

	if ( m_flMaxDist == 0 )
		m_flMaxDist = 8192;

	if ( m_flMinDist == 0 )
		m_flMinDist = 256;

	if ( m_flDelay < 3 )
		m_flDelay = 3;

	m_flNextFire = gpGlobals->time + m_flDelay;
	SetThink( &COp4Mortar::TrackTarget );
	SetUse( &COp4Mortar::FireCannonUse );
	pev->nextthink = gpGlobals->time + 0.1;

	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( "Mortar Shell" );
}

bool COp4Mortar::InRange( float distance )
{
	return !( m_flMinDist > distance || distance > m_flMaxDist );
}

void COp4Mortar::Scan( int distance )
{
	m_pLink = NULL;
	m_pSightLink = NULL;

	CBaseEntity *entities[384];
	Vector delta( distance, distance, distance );
	int count = UTIL_EntitiesInBox( entities, ARRAYSIZE( entities ), pev->origin - delta,
	                                pev->origin + delta, FL_CLIENT | FL_MONSTER );

	for ( int i = 0; i < count; i++ )
	{
		CBaseEntity *target = entities[i];

		if ( target == this || ( target->pev->spawnflags & SF_MONSTER_PRISONER ) || !( target->pev->health > 0 ) )
			continue;

		if ( IRelationship( target, false ) <= R_NO || !FInViewCone( target ) || !FVisible( target, true ) )
			continue;

		target->m_pLink = m_pLink;
		Op4MortarSetHandle( m_pLink, target );
	}
}

CBaseEntity *COp4Mortar::FindTarget( void )
{
	m_pLink = NULL;
	Scan( (int) m_flMaxDist );

	CBaseEntity *best = NULL;
	int bestDistance = 65536;
	int bestRelationship = R_NO;

	for ( CBaseEntity *target = Op4MortarHandleEntity( m_pLink ); target;
	      target = Op4MortarHandleEntity( target->m_pLink ) )
	{
		if ( !target->IsAlive() )
			continue;

		Vector targetCenter = target->Center();
		Vector center = Center();

		if ( !InRange( ( targetCenter - center ).Length() ) )
			continue;

		if ( IRelationship( target, false ) > bestRelationship )
		{
			bestRelationship = IRelationship( target, false );
			bestDistance = (int) ( target->pev->origin - pev->origin ).Length();
			best = target;
		}
		else if ( IRelationship( target, false ) == bestRelationship )
		{
			int distance = (int) ( target->pev->origin - pev->origin ).Length();

			if ( distance <= bestDistance )
			{
				bestRelationship = IRelationship( target, false );
				bestDistance = distance;
				best = target;
			}
		}
	}

	return best;
}

void COp4Mortar::FireCannon( CBaseEntity *activator )
{
	if ( !activator )
		return;

	Vector start( 0, 0, 0 );
	Vector end( 0, 0, 0 );
	Vector angles( 0, 0, 0 );

	GetBonePosition( 5, start, angles );
	GetBonePosition( 4, end, angles );
	angles = UTIL_VecToAngles( end - start );
	UTIL_MakeVectors( angles );
	start = start + gpGlobals->v_right * 10;

	if ( !( gpGlobals->time >= m_flNextFire ) )
		return;

	SetSequenceByName( "fire" );
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "weapons/mortarhit.wav", 1, ATTN_NONE, 0, 100, 0, false, g_vecZero );
	UTIL_ScreenShake( pev->origin, 10, 3, 1.5f, 750 );

	angles = pev->angles + m_vecController;

	if ( m_vecController.x < 45 )
		angles = angles + Vector( 10, 0, 0 );

	UTIL_MakeVectors( angles );
	CGrenade::ShootMortar( activator->pev, start, -gpGlobals->v_forward * 1500 );
	m_flNextFire = gpGlobals->time + m_flDelay;
}

void COp4Mortar::FireCannonUse( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( pev->spawnflags & SF_MORTAR_CONTROLLABLE )
	{
		if ( m_bActive )
			m_bActive = false;
		else
			FireCannon( activator );
	}
	else if ( m_bActive )
		m_bActive = false;
	else
	{
		SetThink( &COp4Mortar::TrackTarget );
		pev->nextthink = gpGlobals->time;
		m_bActive = true;
	}
}

void COp4Mortar::TrackTarget( void )
{
	if ( !m_bActive )
		return;

	pev->nextthink = gpGlobals->time + 0.1;

	CBaseEntity *previousEnemy = Op4MortarHandleEntity( m_hEnemy.m_Handle );

	Op4MortarSetHandle( m_hEnemy.m_Handle, FindTarget() );

	CBaseEntity *enemy = Op4MortarHandleEntity( m_hEnemy.m_Handle );

	if ( !enemy || !enemy->IsAlive() )
	{
		Op4MortarSetHandle( m_hEnemy.m_Handle, previousEnemy );
		enemy = Op4MortarHandleEntity( m_hEnemy.m_Handle );
	}

	if ( !enemy || !enemy->IsAlive() )
	{
		m_vecIdealAngles = Vector( 0, m_flHorizontalMin, 0 );
		UpdateControllers();
		return;
	}

	Vector targetPosition = enemy->pev->origin + enemy->pev->view_ofs;
	Vector center = Center();

	if ( !InRange( ( targetPosition - center ).Length() ) )
	{
		m_vecIdealAngles = Vector( 0, m_flHorizontalMin, 0 );
		UpdateControllers();
		return;
	}

	Vector start( 0, 0, 0 );
	Vector end( 0, 0, 0 );
	Vector angles( 0, 0, 0 );

	GetBonePosition( 5, start, angles );
	GetBonePosition( 4, end, angles );
	angles = UTIL_VecToAngles( end - start );
	UTIL_MakeVectors( angles );
	start = start + gpGlobals->v_right * 10;
	UTIL_MakeVectors( Vector( 0, pev->angles.y, 0 ) );

	Vector direction = ( enemy->Center() - pev->origin ).Normalize();
	Vector localDirection( DotProduct( direction, gpGlobals->v_forward ),
	                       -DotProduct( direction, gpGlobals->v_right ), DotProduct( direction, gpGlobals->v_up ) );

	m_vecIdealAngles = UTIL_VecToAngles( localDirection );

	float distance = ( end - enemy->Center() ).Length2D();

	UTIL_MakeVectors( Vector( 0, pev->angles.y + m_vecController.y, 0 ) );

	float projectedX = end.x - distance * gpGlobals->v_forward.x;
	float projectedY = end.y - distance * gpGlobals->v_forward.y;
	float projectedZ = enemy->Center().z;
	Vector projectedTarget( projectedX, projectedY, projectedZ );

	m_vecVelocity = VecCheckMortar( pev, end, projectedTarget, 1 );
	m_vecIdealAngles.x = UTIL_VecToAngles( m_vecVelocity ).x;

	TraceResult trace;

	UTIL_TraceLine( end, targetPosition, ignore_monsters, edict(), &trace );

	bool visible = trace.flFraction == 1;

	UpdateControllers();

	bool canFire = !( pev->spawnflags & SF_MORTAR_LINE_OF_SIGHT ) || visible;
	float pitchError = UTIL_AngleDistance( m_vecIdealAngles.x, m_vecController.x );
	float yawError = UTIL_AngleDistance( m_vecIdealAngles.y, m_vecController.y );

	if ( !( fabs( pitchError ) < 16 && fabs( yawError ) < 16 ) || !canFire )
		return;

	if ( !( gpGlobals->time >= m_flNextFire ) || m_vecVelocity == g_vecZero )
		return;

	SetSequenceByName( "fire" );
	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "weapons/mortarhit.wav", 1, 0.7f, 0, 100, 0, false, g_vecZero );
	UTIL_ScreenShake( pev->origin, 10, 3, 1.5f, 750 );
	CGrenade::ShootMortar( pev, start, m_vecVelocity );
	m_flNextFire = gpGlobals->time + m_flDelay;
}

void COp4Mortar::UpdateControllers( void )
{
	m_vecIdealAngles.y = UTIL_AngleDistance( m_vecIdealAngles.y + 180, 0 );
	m_vecIdealAngles.x = UTIL_AngleDistance( m_vecIdealAngles.x, 0 );

	if ( m_vecIdealAngles.y > 90 )
		m_vecIdealAngles.y = 90;
	else if ( m_vecIdealAngles.y < -90 )
		m_vecIdealAngles.y = -90;

	if ( m_vecIdealAngles.x > 90 )
		m_vecIdealAngles.x = 90;
	else if ( m_vecIdealAngles.x < 0 )
		m_vecIdealAngles.x = 0;

	if ( m_vecIdealAngles.x > m_vecController.x )
		m_vecController.x = V_min( m_vecIdealAngles.x, m_vecController.x + 4 );

	if ( m_vecController.x > m_vecIdealAngles.x )
		m_vecController.x = V_max( m_vecIdealAngles.x, m_vecController.x - 4 );

	if ( m_vecIdealAngles.y > m_vecController.y )
		m_vecController.y = V_min( m_vecIdealAngles.y, m_vecController.y + 4 );

	if ( m_vecController.y > m_vecIdealAngles.y )
		m_vecController.y = V_max( m_vecIdealAngles.y, m_vecController.y - 4 );

	SetBoneController( 1, m_vecController.y );
	SetBoneController( 0, m_vecController.x );

	if ( m_vecIdealAngles.y != m_vecController.y )
		EMIT_SOUND_DYN( edict(), CHAN_VOICE, "player/pl_grate1.wav", 1, 0.55f, 0, 90, 0, false, g_vecZero );
}

COp4MortarController::COp4MortarController()
    : m_pMortar( NULL ), m_bStopped( false ), m_flStopTime( 0 ), m_iDirection( 0 ), m_flNextMove( 0 ), m_iAxis( 0 )
{
}

COp4MortarController::~COp4MortarController()
{
}

int COp4MortarController::ObjectCaps( void )
{
	return FCAP_CONTINUOUS_USE;
}

void COp4MortarController::KeyValue( KeyValueData *data )
{
	if ( FStrEq( data->szKeyName, "mortar_axis" ) )
	{
		m_iAxis = atoi( data->szValue );
		data->fHandled = TRUE;
	}
}

void COp4MortarController::Spawn( void )
{
	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	UTIL_SetOrigin( pev, pev->origin );
	SetModel_Intercept( edict(), STRING( pev->model ) );
	m_pMortar = NULL;
	m_iDirection = 4;
	m_bStopped = false;
	SetThink( &COp4MortarController::FollowThink );
	SetUse( &COp4MortarController::ControllerUse );
	pev->nextthink = pev->ltime + 0.1;
	m_flStopTime = pev->nextthink;
	m_flNextMove = pev->nextthink;
}

void COp4MortarController::FollowThink( void )
{
	if ( gpGlobals->time > m_flStopTime )
		m_bStopped = true;

	if ( !m_pMortar )
	{
		CBaseEntity *target = NULL;

		while ( ( target = UTIL_FindEntityByTargetname( target, STRING( pev->target ) ) ) != NULL )
		{
			if ( FClassnameIs( target->pev, "op4mortar" ) )
			{
				m_pMortar = dynamic_cast<COp4Mortar *>( target );
				break;
			}
		}
	}

	pev->nextthink = pev->ltime + 0.1f;
}

void COp4MortarController::ControllerUse( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( m_bStopped )
	{
		m_bStopped = false;
		m_iDirection = -m_iDirection;
		return;
	}

	if ( m_pMortar && gpGlobals->time > m_flNextMove )
	{
		float angle;
		int controller;

		if ( m_iAxis )
		{
			angle = m_pMortar->m_vecController.y + m_iDirection;
			m_pMortar->m_vecController.y = angle;

			if ( angle > 90 )
			{
				angle = 90;
				m_iDirection = -m_iDirection;
			}
			else if ( angle < -90 )
			{
				angle = -90;
				m_iDirection = -m_iDirection;
			}

			EMIT_SOUND_DYN( m_pMortar->edict(), CHAN_VOICE, "player/pl_grate1.wav", 1, 0.55f, 0, 90, 0, false, g_vecZero );
			m_pMortar->m_vecController.y = angle;
			controller = 1;
		}
		else
		{
			angle = m_pMortar->m_vecController.x + m_iDirection;
			m_pMortar->m_vecController.x = angle;

			if ( angle > 90 )
			{
				angle = 90;
				m_iDirection = -m_iDirection;
			}
			else if ( angle <= 0 )
			{
				angle = 0;
				m_iDirection = -m_iDirection;
			}

			EMIT_SOUND_DYN( m_pMortar->edict(), CHAN_VOICE, "player/pl_grate1.wav", 1, 0.55f, 0, 90, 0, false, g_vecZero );
			m_pMortar->m_vecController.x = angle;
			controller = 0;
		}

		m_pMortar->SetBoneController( controller, angle );
		m_flNextMove = gpGlobals->time + 0.1f;
	}

	m_flStopTime = gpGlobals->time + 0.5f;
}
