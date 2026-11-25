#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "xenmaker.h"
#include "user_messages.h"

LINK_ENTITY_TO_CLASS( env_xenmaker, CXenMaker );

TYPEDESCRIPTION CXenMaker::m_SaveData[] =
{
	DEFINE_FIELD( CXenMaker, m_iszMonsterClassname, FIELD_STRING ),
	DEFINE_FIELD( CXenMaker, m_flScale, FIELD_FLOAT ),
	DEFINE_FIELD( CXenMaker, m_bMakeMonster, FIELD_BOOLEAN ),
	DEFINE_FIELD( CXenMaker, m_bTryOnce, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CXenMaker, CBaseEntity );

CXenMaker::CXenMaker()
    : m_vBeamColor( 0, 0, 0 ), m_vLightColor( 0, 0, 0 ),
      m_vStartSpriteColor( 0, 0, 0 ), m_vEndSpriteColor( 0, 0, 0 )
{
	memset( m_unusedStorage, 0, sizeof( m_unusedStorage ) );
}

void CXenMaker::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;

	if ( pev->spawnflags & SF_XENMAKER_TRY_ONCE )
		m_bTryOnce = true;

	m_bMakeMonster = !( pev->spawnflags & SF_XENMAKER_NO_MONSTER );

	if ( m_iBeamCount <= 0 )
		m_iBeamCount = 8;

	if ( m_vBeamColor == g_vecZero )
		m_vBeamColor = Vector( 217, 226, 146 );

	if ( m_iBeamAlpha <= 0 )
		m_iBeamAlpha = 128;

	if ( !( m_flBeamRadius > 0 ) )
		m_flBeamRadius = 256;

	if ( m_vLightColor == g_vecZero )
		m_vLightColor = Vector( 39, 209, 137 );

	if ( !( m_flLightRadius > 0 ) )
		m_flLightRadius = 160;

	if ( m_vStartSpriteColor == g_vecZero )
		m_vStartSpriteColor = Vector( 65, 209, 61 );

	if ( !( m_flStartSpriteScale > 0 ) )
		m_flStartSpriteScale = 1;

	if ( !( m_flStartSpriteFramerate > 0 ) )
		m_flStartSpriteFramerate = 12;

	if ( m_iStartSpriteAlpha <= 0 )
		m_iStartSpriteAlpha = 255;

	if ( m_vEndSpriteColor == g_vecZero )
		m_vEndSpriteColor = Vector( 159, 240, 214 );

	if ( !( m_flEndSpriteScale > 0 ) )
		m_flEndSpriteScale = 1;

	if ( !( m_flEndSpriteFramerate > 0 ) )
		m_flEndSpriteFramerate = 12;

	if ( m_iEndSpriteAlpha <= 0 )
		m_iEndSpriteAlpha = 255;
}

void CXenMaker::Precache( void )
{
	UTIL_PrecacheOther( STRING( m_iszMonsterClassname ) );
	UTIL_PrecacheModel( this, "sprites/lgtning.spr" );
	PrecacheSound_Intercept( this, "debris/beamstart7.wav" );
	PrecacheSound_Intercept( this, "debris/beamstart2.wav" );
	UTIL_PrecacheModel( this, "sprites/fexplo1.spr" );
	UTIL_PrecacheModel( this, "sprites/xflare1.spr" );
}

void CXenMaker::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "monstertype" ) )
		m_iszMonsterClassname = AllocStringLocal( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_flBeamRadius" ) )
		m_flBeamRadius = atof( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_flEndSpriteFramerate" ) )
		m_flEndSpriteFramerate = atof( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_flEndSpriteScale" ) )
		m_flEndSpriteScale = atof( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_flLightRadius" ) )
		m_flLightRadius = atof( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_flStartSpriteFramerate" ) )
		m_flStartSpriteFramerate = atof( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_flStartSpriteScale" ) )
		m_flStartSpriteScale = atof( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iBeamAlpha" ) )
		m_iBeamAlpha = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iBeamCount" ) )
		m_iBeamCount = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iEndSpriteAlpha" ) )
		m_iEndSpriteAlpha = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iStartSpriteAlpha" ) )
		m_iStartSpriteAlpha = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_vBeamColor" ) )
		UTIL_StringToVector( m_vBeamColor, pkvd->szValue, ' ' );
	else if ( FStrEq( pkvd->szKeyName, "m_vEndSpriteColor" ) )
		UTIL_StringToVector( m_vEndSpriteColor, pkvd->szValue, ' ' );
	else if ( FStrEq( pkvd->szKeyName, "m_vLightColor" ) )
		UTIL_StringToVector( m_vLightColor, pkvd->szValue, ' ' );
	else if ( FStrEq( pkvd->szKeyName, "m_vStartSpriteColor" ) )
		UTIL_StringToVector( m_vStartSpriteColor, pkvd->szValue, ' ' );
	else
	{
		CBaseEntity::KeyValue( pkvd );
		return;
	}

	pkvd->fHandled = TRUE;
}

bool CXenMaker::CheckClear( void )
{
	if ( !m_bMakeMonster )
		return true;

	Vector mins( pev->origin.x - 34, pev->origin.y - 34, m_flGround );
	Vector maxs( pev->origin.x + 34, pev->origin.y + 34, pev->origin.z );
	CBaseEntity *entities[2];

	return UTIL_EntitiesInBox( entities, ARRAYSIZE( entities ), mins, maxs, FL_CLIENT | FL_MONSTER ) == 0;
}

void CXenMaker::CreateEffect( float /*scale*/ )
{
	m_iMaxSpawnTrys = 5;
	m_iSpawnTrys = 0;

	if ( !CheckClear() )
	{
		if ( m_bTryOnce )
			UTIL_Remove( this );
		else
		{
			pev->nextthink = (float) ( (double) gpGlobals->time + 0.2 );
			SetThink( &CXenMaker::TryThink );
		}

		return;
	}

	MESSAGE_BEGIN( MSG_PVS, TE_CUSTOM, pev->origin, (edict_t *) NULL );
	WRITE_BYTE( 2 );
	WRITE_COORD( pev->origin.x );
	WRITE_COORD( pev->origin.y );
	WRITE_COORD( pev->origin.z );
	WRITE_BYTE( m_iBeamCount );
	WRITE_COORD( m_vBeamColor.x );
	WRITE_COORD( m_vBeamColor.y );
	WRITE_COORD( m_vBeamColor.z );
	WRITE_BYTE( m_iBeamAlpha );
	WRITE_COORD( m_flBeamRadius );
	WRITE_COORD( m_vLightColor.x );
	WRITE_COORD( m_vLightColor.y );
	WRITE_COORD( m_vLightColor.z );
	WRITE_COORD( m_flLightRadius );
	WRITE_COORD( m_vStartSpriteColor.x );
	WRITE_COORD( m_vStartSpriteColor.y );
	WRITE_COORD( m_vStartSpriteColor.z );
	WRITE_BYTE( (int) ( m_flStartSpriteScale * 10.0f ) );
	WRITE_BYTE( (int) m_flStartSpriteFramerate );
	WRITE_BYTE( m_iStartSpriteAlpha );
	WRITE_COORD( m_vEndSpriteColor.x );
	WRITE_COORD( m_vEndSpriteColor.y );
	WRITE_COORD( m_vEndSpriteColor.z );
	WRITE_BYTE( (int) ( m_flEndSpriteScale * 10.0f ) );
	WRITE_BYTE( (int) m_flEndSpriteFramerate );
	WRITE_BYTE( m_iEndSpriteAlpha );
	MESSAGE_END();

	if ( m_bMakeMonster )
	{
		SetThink( &CXenMaker::MakeMonster );
		pev->nextthink = (float) ( (double) gpGlobals->time + 0.4 );
	}
	else
	{
		SetThink( &CXenMaker::DestroyEffect );
		pev->nextthink = gpGlobals->time + 1.0f;
	}
}

void CXenMaker::DestroyEffect( void )
{
}

void CXenMaker::MakeMonster( void )
{
	if ( !CheckClear() )
	{
		m_iSpawnTrys++;

		if ( m_iSpawnTrys <= m_iMaxSpawnTrys )
		{
			SetThink( &CXenMaker::MakeMonster );
			pev->nextthink = (float) ( (double) gpGlobals->time + 0.1 );
		}

		return;
	}

	SetThink( &CXenMaker::DestroyEffect );
	pev->nextthink = (float) ( (double) gpGlobals->time + 0.6 );

	edict_t *child = CREATE_NAMED_ENTITY( m_iszMonsterClassname );

	if ( !child || child->free || FNullEnt( child ) )
	{
		ALERT( at_console, "NULL entity in xenmaker '%s' (X%f Y%f Z%f)\n",
		       STRING( pev->targetname ), (double) pev->origin.x, (double) pev->origin.y, (double) pev->origin.z );
		return;
	}

	child->v.origin = pev->origin - Vector( 0, 0, 16 );
	child->v.angles = pev->angles;
	child->v.spawnflags |= SF_MONSTER_FALL_TO_GROUND;
	DispatchSpawn( child );
	child->v.owner = ENT( pev );
}

void CXenMaker::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( m_flScale == 0 )
		m_flScale = 1;

	CreateEffect( m_flScale );
}

void CXenMaker::TryThink( void )
{
	CreateEffect( m_flScale );
}
