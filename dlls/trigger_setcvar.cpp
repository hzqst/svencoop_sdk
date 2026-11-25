#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "CCVarSystem.h"
#include "server_support.h"

class CTriggerSetCVar : public CBaseEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value );
	int ObjectCaps( void ) { return 0; }
	void SetCVar( const char *name, const char *value );

private:
	string_t				m_iszCVarToChange;
	string_t				m_iszNewValue;
	bool					m_bUseMessage;
	CCVarSystem::SetType_t	m_setType;
};

LINK_ENTITY_TO_CLASS( trigger_setcvar, CTriggerSetCVar );

void CTriggerSetCVar::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszCVarToChange" ) )
	{
		m_iszCVarToChange = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszNewValue" ) )
	{
		m_iszNewValue = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( !stricmp( pkvd->szKeyName, "SetType" ) )
	{
		m_setType = (CCVarSystem::SetType_t) atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CTriggerSetCVar::Spawn( void )
{
	m_bUseMessage = !m_iszNewValue || pev->message;

	if ( !m_setType )
		m_setType = CCVarSystem::SET_ALL_CVARS;
}

void CTriggerSetCVar::SetCVar( const char *name, const char *value )
{
	int result = CCVarSystem::GetInstance()->SetCVar( name, value, m_setType );

	switch ( result )
	{
		case -1:
			ALERT( at_error, "Trigger_setcvar \"%s\": Unknown error occurred at %f %f %f.\n",
			       STRING( pev->targetname ), pev->origin.x, pev->origin.y, pev->origin.z );
			break;
		case 1:
			ALERT( at_error, "Trigger_setcvar \"%s\": No CVAR defined at %f %f %f.\n",
			       STRING( pev->targetname ), pev->origin.x, pev->origin.y, pev->origin.z );
			break;
		case 2:
			ALERT( at_error, "Trigger_setcvar \"%s\": CVAR too long (> 60 chars) at %f %f %f.\n",
			       STRING( pev->targetname ), pev->origin.x, pev->origin.y, pev->origin.z );
			break;
		case 3:
			ALERT( at_error, "Trigger_setcvar \"%s\": Disallowed or unknown CVAR \"%s\" specified at %f %f %f.\n",
			       STRING( pev->targetname ), name, pev->origin.x, pev->origin.y, pev->origin.z );
			break;
		case 4:
			ALERT( at_error, "Trigger_setcvar \"%s\": Unknown skill CVAR \"%s\" at %f %f %f.\n",
			       STRING( pev->targetname ), name, pev->origin.x, pev->origin.y, pev->origin.z );
			break;
	}
}

void CTriggerSetCVar::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( useType == USE_OFF )
		return;

	if ( useType == USE_SET )
	{
		char text[64];

		UTIL_FormatString( text, sizeof( text ), "%f", value );
		SetCVar( STRING( m_iszCVarToChange ), text );
	}
	else
		SetCVar( STRING( m_iszCVarToChange ), STRING( m_bUseMessage ? pev->message : m_iszNewValue ) );

	if ( pev->netname )
		FireTargets( STRING( pev->netname ), activator, this, USE_TOGGLE, value );
}
