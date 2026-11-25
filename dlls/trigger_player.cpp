#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

class CTriggerRenamePlayer : public CBaseDelay
{
public:
	void Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value ) override;
};

class CTriggerChangeClass : public CBaseDelay
{
public:
	void KeyValue( KeyValueData *pkvd ) override;
	void Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value ) override;
	int ObjectCaps( void ) override { return 0; }

private:
	void ChangeClass( CBaseEntity *entity, USE_TYPE useType );

	int	m_iNewClass;
};

LINK_ENTITY_TO_CLASS( trigger_renameplayer, CTriggerRenamePlayer );
LINK_ENTITY_TO_CLASS( trigger_change_class, CTriggerChangeClass );

void CTriggerRenamePlayer::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	CBaseEntity *player = activator;

	if ( !player || !player->IsPlayer() )
	{
		if ( !caller || !caller->IsPlayer() )
			return;

		player = caller;
	}

	player->pev->targetname = pev->netname;

	if ( pev->spawnflags == 0 )
		UTIL_Remove( this );
}

void CTriggerChangeClass::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "classify" ) )
	{
		int classification = atoi( pkvd->szValue );

		if ( classification == CLASS_NONE )
			m_iNewClass = -1;
		else if ( classification < CLASS_NONE || classification > CLASS_TEAM4 )
			m_iNewClass = CLASS_NONE;
		else
			m_iNewClass = classification;

		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CTriggerChangeClass::ChangeClass( CBaseEntity *entity, USE_TYPE useType )
{
	if ( !entity )
		return;

	bool hadOverride = entity->m_fOverrideClass;
	int oldClass = entity->m_iClassSelection;

	switch ( useType )
	{
		case USE_OFF:
			entity->m_fOverrideClass = false;
			break;
		case USE_ON:
			if ( m_iNewClass == -1 )
				entity->m_fOverrideClass = false;
			else
			{
				entity->m_fOverrideClass = true;
				entity->m_iClassSelection = m_iNewClass;
			}
			break;
		case USE_TOGGLE:
		{
			const int comparisonClass = m_iNewClass;

			if ( entity->Classify() == comparisonClass || comparisonClass == -1 )
				entity->m_fOverrideClass = false;
			else
			{
				entity->m_fOverrideClass = true;
				entity->m_iClassSelection = m_iNewClass;
			}
			break;
		}
	}

	if ( hadOverride != entity->m_fOverrideClass || oldClass != entity->m_iClassSelection )
	{
		CBasePlayer *player = dynamic_cast<CBasePlayer *>( entity );

		if ( player )
			player->SendScoreInfo( NULL );
	}
}

void CTriggerChangeClass::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( FStrEq( STRING( pev->target ), "!activator" ) )
		ChangeClass( activator, useType );
	else if ( FStrEq( STRING( pev->target ), "!caller" ) )
		ChangeClass( caller, useType );
	else
	{
		CBaseEntity *entity = NULL;

		while ( ( entity = UTIL_FindEntityByTargetname( entity, STRING( pev->target ) ) ) != NULL )
			ChangeClass( entity, useType );
	}
}
