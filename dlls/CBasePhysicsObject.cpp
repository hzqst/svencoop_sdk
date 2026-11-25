#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "CBasePhysicsObject.h"
#include "CClassicMode.h"

#define BOUNCE_MATERIAL_NONE		0
#define BOUNCE_MATERIAL_METAL		1
#define BOUNCE_MATERIAL_FLESH		2
#define BOUNCE_MATERIAL_GLASS		3
#define BOUNCE_MATERIAL_WOOD		4
#define BOUNCE_MATERIAL_WEAPON		5

#define MAX_BOUNCE_COUNT			15
#define BOUNCE_SOUND_MIN_SPEED		32.0
#define BOUNCE_SOUND_SPEED_RANGE	500.0

extern bool gEvilImpulse101;

void CBasePhysicsObject::Spawn( void )
{
	if ( pev->solid == -1 )
		pev->solid = SOLID_NOT;

	if ( pev->movetype == -1 )
		pev->movetype = MOVETYPE_NONE;

	WorldInit();
}

void CBasePhysicsObject::OnCreate( void )
{
	CBaseEntity::OnCreate();
}

void CBasePhysicsObject::WorldInit( void )
{
	pev->frame = 0;
	pev->framerate = 1.0f;

	int sequence = LookupActivity( ACT_IDLE );

	pev->sequence = sequence == -1 ? 0 : sequence;
	ResetSequenceInfo();
	FallInit();
}

void CBasePhysicsObject::Materialize( void )
{
	SetThink( NULL );
}

void CBasePhysicsObject::TouchedWorld( bool firstTouch )
{
}

void CBasePhysicsObject::Kill( void )
{
	UTIL_Remove( this );
}

const char *CBasePhysicsObject::GetBounceSound( void )
{
	switch ( m_iBounceMaterial )
	{
	case BOUNCE_MATERIAL_METAL:
		return "debris/metal2.wav";

	case BOUNCE_MATERIAL_FLESH:
		return "debris/flesh5.wav";

	case BOUNCE_MATERIAL_GLASS:
		return "debris/glass1.wav";

	case BOUNCE_MATERIAL_WOOD:
		return "debris/wood2.wav";

	case BOUNCE_MATERIAL_WEAPON:
		return CClassicMode::GetInstance()->IsEnabled()
		    ? "hlclassic/items/weapondrop1.wav" : "items/weapondrop1.wav";
	}

	return NULL;
}

void CBasePhysicsObject::FallInit( void )
{
	if ( pev->movetype != -1 && pev->movetype != MOVETYPE_FLY && pev->movetype != MOVETYPE_NOCLIP )
		pev->movetype = ( pev->spawnflags & SF_CREATEDWEAPON ) ? MOVETYPE_BOUNCE : MOVETYPE_TOSS;

	pev->solid = SOLID_TRIGGER;
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	SetTouch( &CBasePhysicsObject::DefaultTouch );
	SetThink( &CBasePhysicsObject::FallThink );

	if ( ( pev->spawnflags & SF_CREATEDWEAPON ) && !gEvilImpulse101 )
	{
		float z = RANDOM_FLOAT( 0, 100 );
		float y = RANDOM_FLOAT( 100, 150 );
		float x = RANDOM_FLOAT( 100, 200 );

		pev->avelocity = Vector( x, y, z );
	}

	pev->friction = 0.3f;
	m_flNextBounce = gpGlobals->time;
	m_iBounceCount = 0;
	m_flWaterSpeed = 160.0f;
	m_fTouchedWorld = false;
	pev->nextthink = gpGlobals->time + 0.1;
}

void CBasePhysicsObject::FallThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	if ( pev->movetype == MOVETYPE_NONE || pev->movetype == MOVETYPE_FLY || pev->movetype == MOVETYPE_NOCLIP )
	{
		Materialize();
		return;
	}

	if ( pev->waterlevel == 3 )
	{
		if ( pev->velocity.Length() > m_flWaterSpeed )
			pev->velocity = pev->velocity.Normalize() * m_flWaterSpeed;

		pev->avelocity = pev->avelocity * 0.9f;
	}

	if ( pev->flags & FL_ONGROUND )
	{
		AlignToGroundStatic( this, 110.0f );

		if ( pev->velocity == g_vecZero )
			Materialize();

		pev->solid = SOLID_TRIGGER;
		UTIL_SetOrigin( pev, pev->origin );
	}
}

void CBasePhysicsObject::DefaultTouch( CBaseEntity *other )
{
	if ( !other->IsBSPModel() )
		return;

	if ( pev->movetype == MOVETYPE_NONE || pev->movetype == MOVETYPE_FLY || pev->movetype == MOVETYPE_NOCLIP )
		return;

	AlignToGroundStatic( this, 190.0f );

	if ( m_iBounceCount >= MAX_BOUNCE_COUNT )
	{
		DROP_TO_FLOOR( ENT( pev ) );
		pev->velocity = g_vecZero;
		pev->avelocity = g_vecZero;
		m_iBounceCount = 0;
	}
	else
	{
		pev->avelocity = -pev->avelocity;
		pev->avelocity.x *= 0.7;
		pev->avelocity.z *= 0.7;
		pev->avelocity.y *= 0.4;

		if ( gpGlobals->time >= m_flNextBounce )
		{
			const char *sound = GetBounceSound();
			float speed = pev->velocity.Length();

			if ( speed > BOUNCE_SOUND_MIN_SPEED && !( pev->flags & FL_ONGROUND ) && pev->waterlevel == 0 && sound && *sound )
			{
				float scale = (float) ( V_min( V_max( (double) speed - BOUNCE_SOUND_MIN_SPEED, 0.0 ), BOUNCE_SOUND_SPEED_RANGE )
				                       / BOUNCE_SOUND_SPEED_RANGE );
				float volume = scale * 0.4 + 0.15;
				int pitch = (int) ( (float) ( RANDOM_LONG( 0, 20 ) + 90 ) + scale * 25.0f );

				EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, sound, volume, 1.0f, 0, pitch );
			}

			float scale;

			if ( pev->waterlevel != 0 )
				scale = 0.3f;
			else if ( pev->angles.x >= -5.0f && pev->angles.x <= 5.0f
			          && ( m_iBounceMaterial == BOUNCE_MATERIAL_NONE || m_iBounceMaterial == BOUNCE_MATERIAL_WEAPON ) )
				scale = 0.5f;
			else
				scale = 0.7f;

			pev->velocity = pev->velocity * scale;
			m_iBounceCount++;
			m_flNextBounce = gpGlobals->time + 0.1;
		}
	}

	TouchedWorld( !m_fTouchedWorld );
	m_fTouchedWorld = true;
}
