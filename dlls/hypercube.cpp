#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "effects.h"
#include "hypercube.h"
#include <math.h>

LINK_ENTITY_TO_CLASS( hyper_cube, CHyperCube );

static const float g_HyperCubeVertices[16][4] =
{
	{ -0.5f, -0.5f, -0.5f, -0.5f },
	{ -0.5f, 0.5f, -0.5f, -0.5f },
	{ 0.5f, 0.5f, -0.5f, -0.5f },
	{ 0.5f, -0.5f, -0.5f, -0.5f },
	{ -0.5f, -0.5f, 0.5f, -0.5f },
	{ -0.5f, 0.5f, 0.5f, -0.5f },
	{ 0.5f, 0.5f, 0.5f, -0.5f },
	{ 0.5f, -0.5f, 0.5f, -0.5f },
	{ -0.5f, -0.5f, -0.5f, 0.5f },
	{ -0.5f, 0.5f, -0.5f, 0.5f },
	{ 0.5f, 0.5f, -0.5f, 0.5f },
	{ 0.5f, -0.5f, -0.5f, 0.5f },
	{ -0.5f, -0.5f, 0.5f, 0.5f },
	{ -0.5f, 0.5f, 0.5f, 0.5f },
	{ 0.5f, 0.5f, 0.5f, 0.5f },
	{ 0.5f, -0.5f, 0.5f, 0.5f },
};

CHyperCube::CHyperCube()
    : m_flLastThinkTime( 0 )
{
	memset( m_vecPosition, 0, sizeof( m_vecPosition ) );
	memset( m_vecVelocity, 0, sizeof( m_vecVelocity ) );
	memset( m_flAngles, 0, sizeof( m_flAngles ) );
	memset( m_flAngularVelocity, 0, sizeof( m_flAngularVelocity ) );

	for ( int i = 0; i < 16; i++ )
		m_hSprites[i] = NULL;
}

CHyperCube::~CHyperCube()
{
}

void CHyperCube::CreateBeam( int startEntity, int endEntity, Vector color )
{
	CBeam *beam = CBeam::BeamCreate( "sprites/laserbeam.spr", 4 );

	beam->EntsInit( startEntity, endEntity );
	beam->pev->rendercolor = color;
	beam->SetBrightness( 255 );
}

void CHyperCube::Randomize( void )
{
	for ( int i = 0; i < 6; i++ )
		m_flAngles[i] = RANDOM_FLOAT( 0, 359 );

	for ( int i = 0; i < 6; i++ )
		m_flAngularVelocity[i] = RANDOM_FLOAT( -45, 45 );

	m_flLastThinkTime = gpGlobals->time;
}

void CHyperCube::Spawn( void )
{
	if ( pev->scale <= 0.0f )
		pev->scale = 32.0f;

	for ( int i = 0; i < 3; i++ )
	{
		m_vecPosition[i] = pev->origin[i];
		m_vecVelocity[i] = pev->velocity[i];
	}

	m_vecPosition[3] = 0;
	m_vecVelocity[3] = 0;
	Randomize();

	Vector colors[2] = { Vector( 255, 0, 0 ), Vector( 0, 0, 255 ) };

	for ( int c = 0; c < 2; c++ )
	{
		for ( int vertex = 0; vertex < 8; vertex++ )
		{
			int i = c * 8 + vertex;
			Vector origin( g_HyperCubeVertices[i][0] * pev->scale + pev->origin.x,
			               g_HyperCubeVertices[i][1] * pev->scale + pev->origin.y,
			               g_HyperCubeVertices[i][2] * pev->scale + pev->origin.z );
			CSprite *sprite = CSprite::SpriteCreate( "sprites/glow02.spr", origin, true, 0.0f );

			sprite->pev->rendermode = kRenderTransAdd;
			sprite->pev->rendercolor = colors[c];
			sprite->SetBrightness( 255 );
			sprite->SetScale( 0.1f );
			m_hSprites[i] = sprite;
		}
	}

	for ( int c = 0; c < 2; c++ )
	{
		for ( int vertex = 0; vertex < 8; vertex++ )
		{
			int start = c * 8 + vertex;
			int next = c * 8 + ( vertex + 1 ) % 4;

			if ( vertex < 4 )
			{
				CreateBeam( m_hSprites[start]->entindex(), m_hSprites[next]->entindex(), colors[c] );
				CreateBeam( m_hSprites[start]->entindex(), m_hSprites[start + 4]->entindex(), colors[c] );
			}
			else
			{
				CreateBeam( m_hSprites[start]->entindex(), m_hSprites[next + 4]->entindex(), colors[c] );
			}

			if ( c == 0 )
				CreateBeam( m_hSprites[start]->entindex(), m_hSprites[start + 8]->entindex(), Vector( 255, 255, 255 ) );
		}
	}

	SetUse( &CHyperCube::Use );
	SetThink( &CHyperCube::Think );
	pev->nextthink = gpGlobals->time + 0.01;
}

void CHyperCube::Think( void )
{
	float elapsed = gpGlobals->time - m_flLastThinkTime;

	for ( int i = 0; i < 4; i++ )
		m_vecPosition[i] += m_vecVelocity[i] * elapsed;

	elapsed = gpGlobals->time - m_flLastThinkTime;

	for ( int i = 0; i < 6; i++ )
		m_flAngles[i] += m_flAngularVelocity[i] * elapsed;

	m_flLastThinkTime = gpGlobals->time;

	for ( int i = 0; i < 6; i++ )
	{
		while ( m_flAngles[i] < 0.0f )
			m_flAngles[i] += 360.0f;

		while ( m_flAngles[i] >= 360.0f )
			m_flAngles[i] -= 360.0f;
	}

	float radians[6];
	float vertices[16][4];

	for ( int i = 0; i < 6; i++ )
		radians[i] = m_flAngles[i] * (float) ( M_PI / 180.0 );

	memcpy( vertices, g_HyperCubeVertices, sizeof( vertices ) );

	if ( radians[0] > 0.0f )
	{
		float c = (float) cos( (double) radians[0] );
		float s = (float) sin( (double) radians[0] );

		for ( int i = 0; i < 16; i++ )
		{
			float x = vertices[i][0];
			float y = vertices[i][1];

			vertices[i][0] = x * c - y * s;
			vertices[i][1] = x * s + y * c;
		}
	}

	if ( radians[3] > 0.0f )
	{
		float c = (float) cos( (double) radians[3] );
		float s = (float) sin( (double) radians[3] );

		for ( int i = 0; i < 16; i++ )
		{
			float y = vertices[i][1];
			float z = vertices[i][2];

			vertices[i][1] = y * c + z * s;
			vertices[i][2] = z * c - y * s;
		}
	}

	if ( radians[1] > 0.0f )
	{
		float c = (float) cos( (double) radians[1] );
		float s = (float) sin( (double) radians[1] );

		for ( int i = 0; i < 16; i++ )
		{
			float x = vertices[i][0];
			float z = vertices[i][2];

			vertices[i][0] = x * c - z * s;
			vertices[i][2] = x * s + z * c;
		}
	}

	if ( radians[2] > 0.0f )
	{
		float c = (float) cos( (double) radians[2] );
		float s = (float) sin( (double) radians[2] );

		for ( int i = 0; i < 16; i++ )
		{
			float x = vertices[i][0];
			float w = vertices[i][3];

			vertices[i][0] = x * c + w * s;
			vertices[i][3] = w * c - x * s;
		}
	}

	if ( radians[4] > 0.0f )
	{
		float c = (float) cos( (double) radians[4] );
		float s = (float) sin( (double) radians[4] );

		for ( int i = 0; i < 16; i++ )
		{
			float y = vertices[i][1];
			float w = vertices[i][3];

			vertices[i][1] = y * c - w * s;
			vertices[i][3] = y * s + w * c;
		}
	}

	if ( radians[5] > 0.0f )
	{
		float c = (float) cos( (double) radians[5] );
		float s = (float) sin( (double) radians[5] );

		// ZW step updates only Z
		for ( int i = 0; i < 16; i++ )
			vertices[i][2] = c * vertices[i][2] - s * vertices[i][3];
	}

	// Projection stays relative to pev->origin
	for ( int i = 0; i < 16; i++ )
	{
		m_hSprites[i]->pev->origin.x = vertices[i][0] * pev->scale + pev->origin.x;
		m_hSprites[i]->pev->origin.y = vertices[i][1] * pev->scale + pev->origin.y;
		m_hSprites[i]->pev->origin.z = vertices[i][2] * pev->scale + pev->origin.z;
	}

	SetThink( &CHyperCube::Think );
	pev->nextthink = gpGlobals->time + 0.01;
}

void CHyperCube::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	Randomize();
}
