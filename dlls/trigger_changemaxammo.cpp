#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "trigger_changemaxammo.h"

// ScriptedSnark: TODO
// pPlayer->SetMaxAmmo( uiAmmoIndex, iMaxAmmo );

enum
{
	SF_CHANGEMAXAMMO_ALL_PLAYERS = 1,
	SF_CHANGEMAXAMMO_REMOVE_EXCESS = 2,
	SF_CHANGEMAXAMMO_FILL = 4
};

LINK_ENTITY_TO_CLASS( trigger_changemaxammo, CTriggerChangeMaxAmmo );

void CTriggerChangeMaxAmmo::KeyValue( KeyValueData *pkvd )
{
	if ( !stricmp( pkvd->szKeyName, "m_iMaxAmmo" ) )
	{
		m_iMaxAmmo = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( !stricmp( pkvd->szKeyName, "m_Mode" ) )
	{
		m_Mode = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CTriggerChangeMaxAmmo::Set_s::operator()( CBasePlayer *player ) const
{
	player->SetMaxAmmo( index, value );
}

void CTriggerChangeMaxAmmo::Reset_s::operator()( CBasePlayer *player ) const
{
	player->ResetMaxAmmoToDefault( index );
}

void CTriggerChangeMaxAmmo::Add_s::operator()( CBasePlayer *player ) const
{
	player->SetMaxAmmo( index, player->GetMaxAmmo( index ) + value );
}

void CTriggerChangeMaxAmmo::Subtract_s::operator()( CBasePlayer *player ) const
{
	player->SetMaxAmmo( index, V_max( player->GetMaxAmmo( index ) - value, 0 ) );
}

template <class Operation>
void CTriggerChangeMaxAmmo::ChangeAmmoForPlayer( CBasePlayer *player, const Operation &operation )
{
	operation( player );

	if ( pev->spawnflags & SF_CHANGEMAXAMMO_REMOVE_EXCESS )
		player->RemoveExcessAmmo( operation.index );

	if ( pev->spawnflags & SF_CHANGEMAXAMMO_FILL )
		player->m_rgAmmo[operation.index] = player->GetMaxAmmo( operation.index );
}

template <class Operation>
void CTriggerChangeMaxAmmo::ChangeAmmo( CBaseEntity *activator, const Operation &operation )
{
	if ( !( pev->spawnflags & SF_CHANGEMAXAMMO_ALL_PLAYERS ) &&
	     ( !activator || !activator->IsPlayer() ) )
		return;

	if ( pev->spawnflags & SF_CHANGEMAXAMMO_ALL_PLAYERS )
	{
		for ( int index = 1; index <= gpGlobals->maxClients; index++ )
		{
			CBasePlayer *player = (CBasePlayer *) UTIL_PlayerByIndex( index );

			if ( player )
				ChangeAmmoForPlayer( player, operation );
		}
	}
	else
		ChangeAmmoForPlayer( (CBasePlayer *) activator, operation );
}

void CTriggerChangeMaxAmmo::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( !( pev->spawnflags & SF_CHANGEMAXAMMO_ALL_PLAYERS ) &&
	     ( !activator || !activator->IsPlayer() ) )
		return;

	int index = CBasePlayer::GetAmmoIndex( STRING( pev->message ) );

	if ( index == -1 )
	{
		ALERT( at_warning, "trigger_changemaxammo: ammo type '%s' does not exist!\n", STRING( pev->message ) );
		return;
	}

	int amount = useType == USE_SET ? (int) value : m_iMaxAmmo;

	switch ( m_Mode )
	{
		case 1:
		{
			Reset_s operation = { (unsigned int) index, 0 };

			ChangeAmmo( activator, operation );
			break;
		}
		case 2:
		{
			Add_s operation = { (unsigned int) index, amount };

			ChangeAmmo( activator, operation );
			break;
		}
		case 3:
		{
			Subtract_s operation = { (unsigned int) index, amount };

			ChangeAmmo( activator, operation );
			break;
		}
		default:
			ALERT( at_warning, "trigger_changemaxammo(%s): unknown mode, defaulting to set!\n", STRING( pev->targetname ) );
		case 0:
		{
			Set_s operation = { (unsigned int) index, amount };

			ChangeAmmo( activator, operation );
			break;
		}
	}

	SUB_UseTargets( activator, USE_TOGGLE, 0 );
}
