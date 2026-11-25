#include "extdll.h"
#include "util.h"
#include "cbase.h"

#define SF_RANDOM_START_ON 1
#define SF_RANDOM_ONCE 2
#define SF_RANDOM_REUSABLE 4
#define SF_RANDOM_TIMED 8
#define SF_RANDOM_UNIQUE 16

class CTriggerRandom : public CBaseDelay
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value );
	int ObjectCaps( void ) { return 0; }
	int Save( CSave &save );
	int Restore( CRestore &restore );

	float RandomDelay( void );
	void InitUnique( void );
	void Fire( CBaseEntity *activator );
	void EXPORT RandomThink( void );

	static TYPEDESCRIPTION m_SaveData[];

private:
	unsigned int	m_uiTargetsUse;
	string_t		m_iszTargets[32];
	bool			m_bActive;
	float			m_flMinDelay;
	float			m_flMaxDelay;

	unsigned int	m_uiTargetsUsed;
	bool			m_bTargetUsed[32];
};

LINK_ENTITY_TO_CLASS( trigger_random, CTriggerRandom );
LINK_ENTITY_TO_CLASS( trigger_random_time, CTriggerRandom );
LINK_ENTITY_TO_CLASS( trigger_random_unique, CTriggerRandom );

TYPEDESCRIPTION CTriggerRandom::m_SaveData[] = {
	DEFINE_FIELD( CTriggerRandom, m_uiTargetsUse, FIELD_INTEGER ),
	DEFINE_ARRAY( CTriggerRandom, m_iszTargets, FIELD_STRING, 32 ),
	DEFINE_FIELD( CTriggerRandom, m_bActive, FIELD_CHARACTER ),
	DEFINE_FIELD( CTriggerRandom, m_flMinDelay, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerRandom, m_flMaxDelay, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CTriggerRandom, CBaseDelay );

void CTriggerRandom::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "target_count" ) )
	{
		m_uiTargetsUse = strtoul( pkvd->szValue, NULL, 10 );

		if ( m_uiTargetsUse >= 32 )
			m_uiTargetsUse = 32;

		pkvd->fHandled = TRUE;
	}

	if ( FStrEq( pkvd->szKeyName, "min_delay" ) )
	{
		m_flMinDelay = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;

		if ( m_flMaxDelay > 0 && m_flMinDelay > m_flMaxDelay )
		{
			float temporary = m_flMinDelay;

			m_flMinDelay = m_flMaxDelay;
			m_flMaxDelay = temporary;
		}
	}
	else if ( FStrEq( pkvd->szKeyName, "max_delay" ) )
	{
		m_flMaxDelay = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;

		if ( m_flMinDelay > 0 && m_flMinDelay > m_flMaxDelay )
		{
			float temporary = m_flMinDelay;

			m_flMinDelay = m_flMaxDelay;
			m_flMaxDelay = temporary;
		}
	}
	else if ( !strncmp( pkvd->szKeyName, "target", 6 ) )
	{
		char *end = NULL;
		unsigned long index = strtoul( pkvd->szKeyName + 6, &end, 10 );

		if ( index >= 1 && index <= 32 && end && !*end )
		{
			m_iszTargets[index - 1] = AllocStringLocal( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else
			CBaseDelay::KeyValue( pkvd );
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

float CTriggerRandom::RandomDelay( void )
{
	return RANDOM_FLOAT( m_flMinDelay, m_flMaxDelay );
}

void CTriggerRandom::InitUnique( void )
{
	m_uiTargetsUsed = 0;
	memset( m_bTargetUsed, 0, sizeof( m_bTargetUsed ) );
}

void CTriggerRandom::Spawn( void )
{
	m_bActive = ( pev->spawnflags & SF_RANDOM_START_ON ) != 0;

	if ( FClassnameIs( pev, "trigger_random_time" ) )
		pev->spawnflags |= SF_RANDOM_TIMED;
	else if ( FClassnameIs( pev, "trigger_random_unique" ) )
	{
		pev->spawnflags |= SF_RANDOM_UNIQUE;

		if ( pev->spawnflags & SF_RANDOM_START_ON )
		{
			pev->spawnflags &= ~SF_RANDOM_START_ON;
			pev->spawnflags |= SF_RANDOM_REUSABLE;
		}
	}

	if ( pev->spawnflags & SF_RANDOM_TIMED )
	{
		pev->nextthink = gpGlobals->time + RandomDelay();
		SetThink( &CTriggerRandom::RandomThink );
	}

	if ( pev->spawnflags & SF_RANDOM_UNIQUE )
		InitUnique();
}

void CTriggerRandom::Fire( CBaseEntity *activator )
{
	string_t target = 0;

	if ( pev->spawnflags & SF_RANDOM_UNIQUE )
	{
		if ( m_uiTargetsUsed >= m_uiTargetsUse )
		{
			if ( !( pev->spawnflags & SF_RANDOM_REUSABLE ) )
				return;

			InitUnique();
			ALERT( at_aiconsole, "%s(%s): all targets fired; reusable mode on; resetting state\n",
			       pev ? STRING( pev->classname ) : "", pev ? STRING( pev->targetname ) : "" );
		}

		unsigned int attempts = 0;
		int index;

		do
		{
			index = RANDOM_LONG( 0, m_uiTargetsUse - 1 );

			if ( !m_bTargetUsed[index] )
				break;

			attempts++;
		} while ( attempts < 256 );

		if ( attempts == 256 )
		{
			ALERT( at_warning, "%s(%s): random selection failed, selecting first free target\n",
			       pev ? STRING( pev->classname ) : "", pev ? STRING( pev->targetname ) : "" );

			for ( unsigned int i = 0; i < m_uiTargetsUse; i++ )
			{
				if ( !m_bTargetUsed[i] )
				{
					index = i;
					break;
				}
			}
		}

		target = m_iszTargets[index];
		m_bTargetUsed[index] = true;
		m_uiTargetsUsed++;
	}
	else
	{
		for ( unsigned int attempts = 0; attempts < 10; attempts++ )
		{
			int index = RANDOM_LONG( 0, m_uiTargetsUse - 1 );

			target = m_iszTargets[index];

			if ( target )
				break;
		}
	}

	FireTargets( STRING( target ), activator, this, USE_TOGGLE, 0 );
}

void CTriggerRandom::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( pev->spawnflags & SF_RANDOM_TIMED )
	{
		m_bActive = !m_bActive;
		SetThink( &CTriggerRandom::RandomThink );
		pev->nextthink = gpGlobals->time + RandomDelay();
	}
	else
		Fire( activator );
}

void CTriggerRandom::RandomThink( void )
{
	if ( m_bActive )
	{
		Fire( this );

		if ( pev->spawnflags & SF_RANDOM_ONCE )
			m_bActive = false;
	}

	if ( m_flMinDelay > 0 && m_flMaxDelay > 0 )
	{
		pev->nextthink = gpGlobals->time + RandomDelay();
	}
	else
		m_bActive = false;
}
