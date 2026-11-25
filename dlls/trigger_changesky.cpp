#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "server_support.h"
#include "user_messages.h"

#define SF_CHANGESKY_ALL_PLAYERS 1
#define SF_CHANGESKY_NO_CLIENT_UPDATE 2
#define SF_CHANGESKY_UPDATE_SERVER 4

class CTriggerChangeSky : public CBaseDelay
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value );
	int ObjectCaps( void ) { return 0; }
	int Save( CSave &save );
	int Restore( CRestore &restore );

	static TYPEDESCRIPTION m_SaveData[];

private:
	bool			m_bChangeSky;
	string_t		m_iszNewSky;
	bool			m_bChangeColor;
	unsigned char	m_iColor[3];
};

LINK_ENTITY_TO_CLASS( trigger_changesky, CTriggerChangeSky );

TYPEDESCRIPTION CTriggerChangeSky::m_SaveData[] = {
	DEFINE_FIELD( CTriggerChangeSky, m_iszNewSky, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CTriggerChangeSky, CBaseDelay );

static bool CanChangeSkyFor( CBaseEntity *entity )
{
	if ( !entity || !entity->pev )
		return false;

	edict_t *edict = entity->edict();

	return edict && !edict->free && !FNullEnt( edict ) && entity->IsPlayer();
}

void CTriggerChangeSky::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "skyname" ) )
	{
		if ( *pkvd->szValue )
		{
			m_bChangeSky = true;
			m_iszNewSky = AllocStringLocal( pkvd->szValue );
		}

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "color" ) )
	{
		if ( *pkvd->szValue )
		{
			int color[3];

			m_bChangeColor = true;
			UTIL_StringToIntArray( color, 3, pkvd->szValue, ' ', NULL, 0 );
			m_iColor[0] = color[0];
			m_iColor[1] = color[1];
			m_iColor[2] = color[2];
		}

		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CTriggerChangeSky::Spawn( void )
{
	if ( m_iszNewSky && *STRING( m_iszNewSky ) )
		UTIL_PrecacheSkymap( STRING( m_iszNewSky ) );
	else
	{
		ALERT( at_error, "trigger_changesky \"%s\": Sky not specified at %f %f %f.\n",
		       STRING( pev->targetname ), pev->origin.x, pev->origin.y, pev->origin.z );
		UTIL_Remove( this );
	}
}

void CTriggerChangeSky::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( !m_bChangeSky && !m_bChangeColor )
	{
		ALERT( at_error, "trigger_changesky \"%s\": Changes not specified at %f %f %f.\n",
		       STRING( pev->targetname ), pev->origin.x, pev->origin.y, pev->origin.z );
		return;
	}

	char skyName[32] = { 0 };

	if ( m_bChangeSky )
	{
		if ( !m_iszNewSky || !*STRING( m_iszNewSky ) )
		{
			ALERT( at_error, "trigger_changesky \"%s\": Sky not specified at %f %f %f.\n",
			       STRING( pev->targetname ), pev->origin.x, pev->origin.y, pev->origin.z );
			return;
		}

		UTIL_CopyString( skyName, STRING( m_iszNewSky ), sizeof( skyName ) );
	}

	float red = -1;
	float green = -1;
	float blue = -1;

	if ( m_bChangeColor )
	{
		red = m_iColor[0];
		green = m_iColor[1];
		blue = m_iColor[2];
	}

	if ( !( pev->spawnflags & SF_CHANGESKY_NO_CLIENT_UPDATE ) )
	{
		if ( pev->spawnflags & SF_CHANGESKY_ALL_PLAYERS )
		{
			ALERT( at_aiconsole, "Changing sky for all players to \"%s\" RGB %d %d %d.\n",
			       skyName, red, green, blue );
			MESSAGE_BEGIN( MSG_ALL, gmsgChangeSky, g_vecZero );
		}
		else
		{
			CBaseEntity *player = NULL;

			if ( CanChangeSkyFor( activator ) )
				player = activator;
			else if ( CanChangeSkyFor( caller ) )
				player = caller;

			if ( !player )
			{
				ALERT( at_console, "Unable to change the sky, are you using a relay without the spawnflag for updating all players?\n" );
				return;
			}

			ALERT( at_aiconsole, "Changing sky for one player to \"%s\" RGB %d %d %d.\n",
			       skyName, red, green, blue );
			MESSAGE_BEGIN( MSG_ONE, gmsgChangeSky, g_vecZero, player->edict() );
		}

		WRITE_STRING( skyName );
		WRITE_COORD( red );
		WRITE_COORD( green );
		WRITE_COORD( blue );
		MESSAGE_END();
	}

	if ( pev->spawnflags & SF_CHANGESKY_UPDATE_SERVER )
	{
		if ( m_bChangeSky )
		{
			ALERT( at_aiconsole, "Changing server sky name to \"%s\".\n", skyName );
			CVAR_SET_STRING( "sv_skyname", skyName );
		}

		if ( m_bChangeColor )
		{
			ALERT( at_aiconsole, "Changing server sky color to RGB %d %d %d.\n", red, green, blue );
			CVAR_SET_FLOAT( "sv_skycolor_r", m_iColor[0] );
			CVAR_SET_FLOAT( "sv_skycolor_g", m_iColor[1] );
			CVAR_SET_FLOAT( "sv_skycolor_b", m_iColor[2] );
		}
	}
}
