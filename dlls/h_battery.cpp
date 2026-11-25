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
/*

===== h_battery.cpp ========================================================

  battery-related code

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "charger.h"

extern cvar_t sk_suitcharger;

LINK_ENTITY_TO_CLASS( func_recharge, CFuncCharger );

bool CFuncCharger::ChargeDenied( CBasePlayer *player )
{
	if ( m_iJuice > 0 && ( player->pev->weapons & ( 1U << WEAPON_SUIT ) )
		&& player->pev->armorvalue < player->pev->armortype )
	{
		return false;
	}

	if ( m_flSoundTime <= gpGlobals->time )
	{
		m_flSoundTime = gpGlobals->time + 0.62f;
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, m_pszDeniedSound, 1.0f, ATTN_NORM,
		                0, PITCH_NORM, 0, false, g_vecZero );
	}

	return true;
}

void CFuncCharger::Charge( CBasePlayer *player )
{
	if ( player->TakeArmor( 1, DMG_GENERIC, 0 ) )
		--m_iJuice;
}

int CFuncCharger::GetDefaultCapacity( void ) const
{
	return (int) sk_suitcharger.value;
}

float CFuncCharger::GetRechargeTime( void ) const
{
	return g_pGameRules->FlHEVChargerRechargeTime();
}

ChargerSounds CFuncCharger::GetChargerSounds( void ) const
{
	ChargerSounds sounds = { "items/suitchargeno1.wav", "items/suitchargeok1.wav", "items/suitcharge1.wav" };

	return sounds;
}
