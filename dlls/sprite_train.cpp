#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "effects.h"
#include "sprite_train.h"
#include "saverestore.h"

LINK_ENTITY_TO_CLASS( env_spritetrain, CSpriteTrain );

TYPEDESCRIPTION CSpriteTrain::m_SaveData[] =
{
	DEFINE_FIELD( CSpriteTrain, m_hCurrentTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( CSpriteTrain, m_activated, FIELD_CHARACTER ),
};

IMPLEMENT_SAVERESTORE( CSpriteTrain, CBasePlatTrain );

void CSpriteTrain::Spawn( void )
{
	Precache();
	SetModel_Intercept( edict(), STRING( pev->model ) );

	if ( FStringNull( pev->target ) )
		ALERT( at_console, "Sprite train with no target (X%f Y%f Z%f)\n",
		       (double) pev->origin.x, (double) pev->origin.y, (double) pev->origin.z );

	if ( pev->dmg == 0 )
		pev->dmg = 2;

	if ( pev->framerate == 0 )
		pev->framerate = 10;

	if ( pev->speed == 0 )
		pev->speed = 64;

	pev->movetype = MOVETYPE_PUSH;
	pev->solid = SOLID_NOT;

	if ( pev->rendermode == kRenderNormal )
		pev->rendermode = kRenderGlow;

	if ( pev->renderamt == 0 )
		pev->renderamt = 200;

	if ( pev->renderfx == kRenderFxNone )
		pev->renderfx = kRenderFxNone;

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( pev, pev->origin );
	m_activated = FALSE;

	if ( m_volume == 0 )
		m_volume = 0.85f;

	m_hSprite = CSprite::SpriteCreate( STRING( pev->model ), pev->origin, true, 0.0f );
	m_hSprite->SetTransparency( pev->rendermode,
	                            (int) pev->rendercolor.x, (int) pev->rendercolor.y, (int) pev->rendercolor.z,
	                            (int) pev->renderamt, pev->renderfx );
	m_hSprite->SetAttachment( edict(), 0 );
	m_hSprite->SetScale( pev->scale );
	m_hSprite->pev->framerate = pev->framerate;
	m_hSprite->TurnOn();

	pev->renderamt = 0;
	pev->renderfx = kRenderFxNone;
}

void CSpriteTrain::Precache( void )
{
	PrecacheModel_Intercept( this, STRING( pev->model ) );
	CBasePlatTrain::Precache();
}

void CSpriteTrain::UpdateOnRemove( void )
{
	CFuncTrain::UpdateOnRemove();

	if ( m_hSprite )
	{
		UTIL_Remove( m_hSprite );
		m_hSprite = NULL;
	}
}
