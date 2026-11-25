/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "items.h"
#include "gamerules.h"
#include "CClassicMode.h"
#include "charger.h"

extern int gmsgItemPickup;
extern cvar_t sk_healthkit;
extern cvar_t sk_healthcharger;

class CHealthKit : public CItem
{
public:
	void KeyValue( KeyValueData *data )
	{
		if ( FStrEq( data->szKeyName, "healthcap" ) )
		{
			m_iHealthCap = atoi( data->szValue );
			data->fHandled = TRUE;
		}
		else
			CItem::KeyValue( data );
	}

	void Spawn( void );
	void Precache( void );
	BOOL MyTouch( CBasePlayer *pPlayer );

private:
	int m_iHealthCap;

};


LINK_ENTITY_TO_CLASS( item_healthkit, CHealthKit );

void CHealthKit :: Spawn( void )
{
	Precache( );

	const char *model = SetupModel() ? STRING( pev->model )
	    : ( CClassicMode::GetInstance()->IsEnabled() ? "models/hlclassic/w_medkit.mdl" : "models/w_medkit.mdl" );

	SET_MODEL( ENT( pev ), model );
	Item_SetDefaultSize( pev, Vector( -11.9f, -11.9f, 0 ), Vector( 11.9f, 11.9f, 7.9f ) );

	CItem::Spawn();
}

void CHealthKit::Precache( void )
{
	CBaseEntity::Precache();

	const char *model = pev->model ? STRING( pev->model )
	    : ( CClassicMode::GetInstance()->IsEnabled() ? "models/hlclassic/w_medkit.mdl" : "models/w_medkit.mdl");

	UTIL_PrecacheModel( this, model );
	PrecacheSound_Intercept( this, "items/smallmedkit1.wav");
}

BOOL CHealthKit::MyTouch( CBasePlayer *pPlayer )
{
	if ( m_iHealthCap <= 0 && pPlayer->pev->health == pPlayer->pev->max_health )
		return FALSE;

	float amount;

	if ( pev->health > 0.0f )
		amount = pev->health;
	else
		amount = sk_healthkit.value;

	if ( pPlayer->TakeHealth( amount, DMG_GENERIC, m_iHealthCap ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgItemPickup, NULL, ENT( pPlayer->pev ) );
			WRITE_STRING( STRING(pev->classname) );
		MESSAGE_END();

		EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/smallmedkit1.wav", 1, ATTN_NORM);

		if ( g_pGameRules->ItemShouldRespawn( this ) )
		{
			Respawn();
		}
		else
		{
			UTIL_Remove(this);	
		}

		return TRUE;
	}

	return FALSE;
}

LINK_ENTITY_TO_CLASS( func_healthcharger, CFuncHealthCharger );

bool CFuncHealthCharger::ChargeDenied( CBasePlayer *player )
{
	if ( m_iJuice > 0 )
	{
		if ( ( player->pev->weapons & ( 1U << WEAPON_SUIT ) )
			&& player->pev->health < player->pev->max_health )
		{
			return false;
		}

		if ( player->pev->health >= player->pev->max_health
			&& player->HasNamedPlayerItem( "weapon_medkit" )
			&& g_pGameRules->CanHaveAmmo( player, "health", 100 ) )
		{
			return false;
		}
	}

	if ( m_flSoundTime <= gpGlobals->time )
	{
		m_flSoundTime = gpGlobals->time + 0.62f;
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, m_pszDeniedSound, 1.0f, ATTN_NORM,
		                0, PITCH_NORM, 0, false, g_vecZero );
	}

	return true;
}

void CFuncHealthCharger::Charge( CBasePlayer *player )
{
	if ( player->TakeHealth( 1, DMG_GENERIC, 0 ) )
		--m_iJuice;
}

int CFuncHealthCharger::GetDefaultCapacity( void ) const
{
	return (int) sk_healthcharger.value;
}

float CFuncHealthCharger::GetRechargeTime( void ) const
{
	return g_pGameRules->FlHealthChargerRechargeTime();
}

ChargerSounds CFuncHealthCharger::GetChargerSounds( void ) const
{
	ChargerSounds sounds = { "items/medshotno1.wav", "items/medshot4.wav", "items/medcharge4.wav" };

	return sounds;
}
