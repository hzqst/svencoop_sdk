#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "bmodels.h"
#include "item_inventory.h"
#include "func_clip.h"

enum
{
	SF_CLIP_START_OFF = 1,
	SF_CLIP_DIRECTIONAL = 2,
	SF_CLIP_NO_CLIENTS = 4,
	SF_CLIP_MONSTERS = 8,
	SF_CLIP_PUSHABLES = 16,
	SF_CLIP_OTHERS = 32,
	SF_CLIP_INVENTORY = 64
};

#define SF_CLIP_FILTER_MASK ( SF_CLIP_DIRECTIONAL | SF_CLIP_NO_CLIENTS | SF_CLIP_MONSTERS | SF_CLIP_PUSHABLES | SF_CLIP_OTHERS | SF_CLIP_INVENTORY )

LINK_ENTITY_TO_CLASS( func_clip, CFuncClip );
LINK_ENTITY_TO_CLASS( func_monsterclip, CFuncClip );

float ClipAngle( const Vector &first, const Vector &second )
{
	float radians = (float) acos( (double) ( DotProduct( first, second ) / ( first.Length() * second.Length() ) ) );

	return (float) ( (double) radians * 180.0 / M_PI );
}

void CFuncClip::Spawn( void )
{
	m_bMonsterClip = FClassnameIs( pev, "func_monsterclip" )
		|| ( pev->spawnflags & SF_CLIP_FILTER_MASK ) == ( SF_CLIP_NO_CLIENTS | SF_CLIP_MONSTERS );

	if ( !m_bMonsterClip && ( pev->spawnflags & SF_CLIP_MONSTERS )
		&& !( pev->spawnflags & ( SF_CLIP_DIRECTIONAL | SF_CLIP_NO_CLIENTS ) )
		&& ( pev->spawnflags & SF_CLIP_PUSHABLES ) && ( pev->spawnflags & SF_CLIP_OTHERS )
		&& ( pev->spawnflags & SF_CLIP_INVENTORY ) )
	{
		CFuncWallToggle *wall = dynamic_cast<CFuncWallToggle *>(
		    CBaseEntity::Create( "func_wall_toggle", pev->origin, pev->angles, true, NULL ) );

		if ( wall )
		{
			if ( pev->spawnflags & SF_CLIP_START_OFF )
				wall->pev->spawnflags |= SF_CLIP_START_OFF;

			wall->pev->model = pev->model;
			wall->Spawn();
			wall->pev->effects |= EF_NODRAW;

			SetThink( NULL );
			SetTouch( NULL );
			pev->model = iStringNull;
			UTIL_Remove( this );
			return;
		}
	}

	pev->effects |= EF_NODRAW;
	SetModel_Intercept( edict(), STRING( pev->model ) );
	m_vecCenter = pev->absmin + ( pev->absmax - pev->absmin ) * 0.5f;

	if ( m_bMonsterClip )
	{
		pev->movetype = MOVETYPE_PUSH;
		pev->flags |= FL_WORLDBRUSH;
	}
	else
	{
		pev->movetype = MOVETYPE_NONE;
		pev->flags &= ~FL_WORLDBRUSH;
	}

	if ( pev->spawnflags & SF_CLIP_START_OFF )
		DeActivate();
	else
		EnableClip();
}

void CFuncClip::EnableClip( void )
{
	pev->solid = m_bMonsterClip ? SOLID_BSP : SOLID_TRIGGER;

	if ( m_bMonsterClip || ( pev->spawnflags & SF_CLIP_MONSTERS ) )
		pev->flags |= FL_MONSTERCLIP;

	SetTouch( &CFuncClip::Touch );
	m_bActive = true;
	UTIL_SetOrigin( pev, pev->origin );
}

void CFuncClip::DeActivate( void )
{
	pev->solid = SOLID_NOT;

	if ( m_bMonsterClip || ( pev->spawnflags & SF_CLIP_MONSTERS ) )
		pev->flags &= ~FL_MONSTERCLIP;

	SetTouch( NULL );
	m_bActive = false;
	UTIL_SetOrigin( pev, pev->origin );
}

void CFuncClip::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	switch ( useType )
	{
		case USE_OFF:
			DeActivate();
			break;
		case USE_ON:
			EnableClip();
			break;
		case USE_SET:
			if ( value == 0.0f )
				DeActivate();
			else
				EnableClip();
			break;
		case USE_TOGGLE:
			if ( m_bActive )
				DeActivate();
			else
				EnableClip();
			break;
		case USE_KILL:
			UTIL_Remove( this );
			break;
	}
}

void CFuncClip::Touch( CBaseEntity *other )
{
	if ( FClassnameIs( other->pev, "worldspawn" ) )
		return;

	if ( FClassnameIs( other->pev, "item_inventory" )
		&& GetClassPtr( (CItemInventory *) other->pev )->m_hHolder )
	{
		return;
	}

	if ( !( ( !( pev->spawnflags & SF_CLIP_NO_CLIENTS ) && other->IsPlayer() )
		|| ( ( pev->spawnflags & SF_CLIP_MONSTERS ) && other->IsMonster() )
		|| ( ( pev->spawnflags & SF_CLIP_PUSHABLES ) && FClassnameIs( other->pev, "func_pushable" ) )
		|| ( ( pev->spawnflags & SF_CLIP_OTHERS ) && !other->IsPlayer() && !other->IsMonster()
			&& !FClassnameIs( other->pev, "func_pushable" ) && !FClassnameIs( other->pev, "item_inventory" ) )
		|| ( ( pev->spawnflags & SF_CLIP_INVENTORY ) && FClassnameIs( other->pev, "item_inventory" ) ) ) )
	{
		return;
	}

	if ( pev->spawnflags & SF_CLIP_DIRECTIONAL )
	{
		Vector direction;

		AngleVectors( pev->angles, &direction, NULL, NULL );

		if ( pev->max_health > ClipAngle( direction, other->pev->velocity.Normalize() ) )
			return;
	}

	Vector offset = m_vecCenter - other->pev->origin;
	Vector distance( fabs( offset.x ) - pev->size.x * 0.5f,
	                 fabs( offset.y ) - pev->size.y * 0.5f, fabs( offset.z ) - pev->size.z * 0.5f );
	Vector normal( 0, 0, 0 );
	Vector velocity;
	bool vertical = false;

	if ( distance.x >= distance.y && distance.x >= distance.z )
	{
		normal.x = offset.x < 0.0f ? 1.0f : -1.0f;
	}
	else if ( distance.y >= distance.x && distance.y >= distance.z )
	{
		normal.y = offset.y < 0.0f ? 1.0f : -1.0f;
	}
	else if ( distance.z >= distance.x && distance.z >= distance.y )
	{
		velocity = Vector( other->pev->velocity.x, other->pev->velocity.y, 0 );
		vertical = true;

		if ( offset.z >= 0.0f )
			other->pev->flags |= FL_ONGROUND;
	}

	if ( !vertical )
	{
		if ( pev->max_health > ClipAngle( other->pev->velocity, normal ) )
			return;

		velocity = CrossProduct( normal, CrossProduct( other->pev->velocity, normal ) );
	}

	Vector origin = other->pev->origin - ( other->pev->velocity - velocity ) * gpGlobals->frametime;

	if ( other->pev->velocity.z < 0.0f && !( other->pev->flags & FL_ONGROUND ) )
		origin.z = other->pev->origin.z;

	UTIL_SetOrigin( other->pev, origin );
	other->pev->velocity = velocity;
}
