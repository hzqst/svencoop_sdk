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

===== items.cpp ========================================================

  functions governing the selection/use of weapons for players

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "skill.h"
#include "items.h"
#include "gamerules.h"
#include "CASHook.h"
#include "server_support.h"
#include "CClassicMode.h"

#define SF_ITEM_TOUCH_ONLY		128
#define SF_ITEM_USE_WITHOUT_LOS	512
#define SF_ITEM_START_IN_SOLID	( 1 << 24 )

extern int gmsgItemPickup;
extern cvar_t sk_battery;

class CWorldItem : public CBaseEntity
{
public:
	void	KeyValue(KeyValueData *pkvd ); 
	void	Spawn( void );
	int		m_iType;
};

LINK_ENTITY_TO_CLASS(world_items, CWorldItem);

void CWorldItem::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "type"))
	{
		m_iType = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CWorldItem::Spawn( void )
{
	CBaseEntity *pEntity = NULL;

	switch (m_iType) 
	{
	case 44: // ITEM_BATTERY:
		pEntity = CBaseEntity::Create( "item_battery", pev->origin, pev->angles , false);
		break;
	case 42: // ITEM_ANTIDOTE:
		pEntity = CBaseEntity::Create( "item_antidote", pev->origin, pev->angles , false);
		break;
	case 43: // ITEM_SECURITY:
		pEntity = CBaseEntity::Create( "item_security", pev->origin, pev->angles , false);
		break;
	case 45: // ITEM_SUIT:
		pEntity = CBaseEntity::Create( "item_suit", pev->origin, pev->angles , false);
		break;
	}

	if (!pEntity)
	{
		ALERT( at_console, "unable to create world_item %d\n", m_iType );
	}
	else
	{
		pEntity->pev->target = pev->target;
		pEntity->pev->targetname = pev->targetname;
		pEntity->pev->spawnflags = pev->spawnflags;
	}

	REMOVE_ENTITY(edict());
}


void CItem::OnCreate( void )
{
	CBaseEntity::OnCreate();

	m_RespawnDelay.OnCreate( "mp_item_respawndelay" );
}

void CItem::KeyValue( KeyValueData *data )
{
	if ( FStrEq( data->szKeyName, "sequencename" ) )
	{
		m_iszSequence = AllocStringLocal( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "minhullsize" ) )
	{
		UTIL_StringToVector( pev->vuser1, data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "maxhullsize" ) )
	{
		UTIL_StringToVector( pev->vuser2, data->szValue );
		data->fHandled = TRUE;
	}
	else if ( !m_RespawnDelay.KeyValue( this, data ) )
		CBaseEntity::KeyValue( data );
}

int CItem::ObjectCaps( void )
{
	return FCAP_ACROSS_TRANSITION | ( ( pev->spawnflags & SF_ITEM_TOUCH_ONLY ) ? 0 : FCAP_IMPULSE_USE );
}

void CItem::Spawn( void )
{
	if ( pev->solid != -1 && pev->solid != SOLID_TRIGGER && pev->solid != SOLID_BBOX )
		pev->solid = SOLID_TRIGGER;

	if ( pev->solid == -1 )
		pev->solid = SOLID_NOT;

	if ( pev->solid == SOLID_BBOX )
	{
		Vector mins;

		if ( pev->vuser1 == g_vecZero )
			mins = pev->mins;
		else
			mins = pev->vuser1;

		Vector maxs;

		if ( pev->vuser2 == g_vecZero )
			maxs = pev->maxs;
		else
			maxs = pev->vuser2;

		UTIL_SetSize( pev, mins, maxs );
	}

	UTIL_SetOrigin( pev, pev->origin );

	if ( pev->movetype != -1 && pev->movetype != MOVETYPE_FLY && pev->movetype != MOVETYPE_NOCLIP )
		pev->movetype = ( pev->spawnflags & SF_CREATEDWEAPON ) ? MOVETYPE_BOUNCE : MOVETYPE_TOSS;

	if ( pev->movetype == -1 )
		pev->movetype = MOVETYPE_NONE;

	SetSequenceFromLookup(pev, m_iszSequence, pev->sequence );
	SetTouch(&CItem::ItemTouch);
	SetUse( &CBaseEntity::Use );

	if ( ( pev->spawnflags & SF_GIVENITEM ) || pev->movetype == MOVETYPE_NONE
	     || pev->movetype == MOVETYPE_FLY || pev->movetype == MOVETYPE_NOCLIP )
		return;

	edict_t *world = INDEXENT( 0 );

	if ( world )
	{
		Vector origin = pev->origin + Vector( 0, 0, 18 );
		TraceResult trace;

		UTIL_TraceModel( origin, origin, head_hull, world, &trace );

		if ( trace.fStartSolid )
			pev->spawnflags |= SF_ITEM_START_IN_SOLID;
	}

	if ( DROP_TO_FLOOR( edict()) == 0)
	{
		ALERT( at_console, "Item '%s' fell out of level at X%f Y%f Z%f.\n", STRING( pev->classname ), pev->origin.x, pev->origin.y, pev->origin.z );
		UTIL_Remove( this );
	}
}

extern bool gEvilImpulse101;

bool CItem::CanCollect( CBaseEntity *other, COLLECT_TYPE collectType )
{
	if ( pev->flags & FL_KILLME )
		return false;

	if ( collectType == COLLECT_TOUCH )
	{
		if ( ( pev->spawnflags & SF_ITEM_USE_ONLY ) && !( pev->spawnflags & SF_GIVENITEM ) )
			return false;
	}
	else if ( collectType == COLLECT_USE_DIRECT )
	{
		if ( pev->spawnflags & SF_ITEM_TOUCH_ONLY )
			return false;

		if ( !( pev->spawnflags & SF_ITEM_USE_WITHOUT_LOS ) && !UTIL_HasLineOfSight( other, this, false, false ) )
			return false;
	}

	if ( !other || !other->IsPlayer() )
		return false;

	if ( ( pev->effects & EF_NODRAW ) && !( pev->spawnflags & SF_GIVENITEM ) )
		return false;

	CBasePlayer *player = dynamic_cast<CBasePlayer *>( other );

	if ( !player || !g_pGameRules->CanHaveItem( player, this ) )
		return false;

	bool canCollect = true;

	g_CanCollectHook.Call( 3, this, other, &canCollect );
	return canCollect;
}

void CItem::Collect( CBaseEntity *other, COLLECT_TYPE collectType )
{
	CBasePlayer *player = dynamic_cast<CBasePlayer *>( other );

	if ( !player )
		return;

	if ( MyTouch( player ) )
	{
		SetTouch( NULL );
		SetUse( NULL );
		SUB_UseTargets( other, USE_TOGGLE, 0 );
		g_pGameRules->PlayerGotItem( player, this );
		g_CollectedHook.Call( 2, this, other );

		if ( FStrEq( STRING( pev->classname ), "item_security" ) )
		{
			pev->effects |= EF_NODRAW;
			UTIL_Remove( this );
			return;
		}

		if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_YES )
			Respawn();
		else
			UTIL_Remove( this );
	}
	else if ( gEvilImpulse101 )
		UTIL_Remove( this );
}

void CItem::ItemTouch( CBaseEntity *pOther )
{
	// if it's not a player, ignore
	if ( pOther && !pOther->IsBSPModel() && CanCollect( pOther, COLLECT_TOUCH ) )
		Collect( pOther, COLLECT_TOUCH );
		}

void CItem::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( !activator )
		return;

	if ( useType == USE_ON || useType == USE_SET || useType == USE_TOGGLE )
	{
		COLLECT_TYPE collectType = GetUseCollectType( activator, caller, useType );

		if ( CanCollect( activator, collectType ) )
			Collect( activator, collectType );
	}
	else if ( useType == (USE_TYPE) 4 )
		UTIL_Remove( this );
}

CBaseEntity* CItem::Respawn( void )
{
	SetTouch( NULL );
	SetUse( NULL );
	pev->effects |= EF_NODRAW;

	UTIL_SetOrigin( pev, g_pGameRules->VecItemRespawnSpot( this ) );// blip to whereever you should respawn.

	SetThink ( &CItem::CallMaterialize );
	pev->nextthink = g_pGameRules->FlItemRespawnTime( this ); 
	return this;
}

void CItem::Materialize( void )
{
	g_MaterializeHook.Call( 1, this );

	if ( pev->effects & EF_NODRAW )
	{
		// changing from invisible state to visible.
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "items/suitchargeok1.wav", 1, ATTN_NORM, 0, 150 );
		pev->effects &= ~EF_NODRAW;
		pev->effects |= EF_MUZZLEFLASH;
	}

	SetUse( &CBaseEntity::Use );
	SetTouch( &CItem::ItemTouch );
}

#define SF_SUIT_SHORTLOGON		0x0001

class CItemSuit : public CItem
{
	void Spawn( void )
	{ 
		Precache( );

		const char *model = SetupModel() ? STRING( pev->model )
		    : ( CClassicMode::GetInstance()->IsEnabled() ? "models/hlclassic/w_suit.mdl" : "models/w_suit.mdl" );

		SET_MODEL( ENT( pev ), model );
		Item_SetDefaultSize( pev, Vector( -14.9f, -14.9f, 0 ), Vector( 14.9f, 14.9f, 63.9f ) );
		CItem::Spawn( );
	}
	void Precache( void )
	{
		const char *model = pev->model ? STRING( pev->model )
		    : ( CClassicMode::GetInstance()->IsEnabled() ? "models/hlclassic/w_suit.mdl" : "models/w_suit.mdl" );

		UTIL_PrecacheModel( this, model );
		CBaseEntity::Precache();
	}
	BOOL MyTouch( CBasePlayer *pPlayer )
	{
		if ( pPlayer->pev->weapons & (1<<WEAPON_SUIT) )
			return FALSE;

		if ( pev->spawnflags & SF_SUIT_SHORTLOGON )
			EMIT_SOUND_SUIT(pPlayer->edict(), "!HEV_A0");		// short version of suit logon,
		else
			EMIT_SOUND_SUIT(pPlayer->edict(), "!HEV_AAx");	// long version of suit logon

		pPlayer->pev->weapons |= (1<<WEAPON_SUIT);
		pPlayer->SendScoreInfo( NULL );
		return TRUE;
	}
};

LINK_ENTITY_TO_CLASS(item_suit, CItemSuit);



class CItemBattery : public CItem
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

	void Spawn( void )
	{ 
		Precache( );

		const char *model = SetupModel() ? STRING( pev->model )
		    : ( CClassicMode::GetInstance()->IsEnabled() ? "models/hlclassic/w_battery.mdl" : "models/w_battery.mdl" );

		SET_MODEL( ENT( pev ), model );
		Item_SetDefaultSize( pev, Vector( -7.9f, -7.9f, 0 ), Vector( 7.9f, 7.9f, 15.9f ) );
		CItem::Spawn( );
	}
	void Precache( void )
	{
		CBaseEntity::Precache();

		const char *model = pev->model ? STRING( pev->model )
		    : ( CClassicMode::GetInstance()->IsEnabled() ? "models/hlclassic/w_battery.mdl" : "models/w_battery.mdl" );

		UTIL_PrecacheModel( this, model );
		PrecacheSound_Intercept( this, "items/gunpickup2.wav" );
	}
	BOOL MyTouch( CBasePlayer *pPlayer )
	{
		if ( m_iHealthCap <= 0 && pPlayer->pev->armorvalue == pPlayer->pev->armortype )
			return FALSE;

		float amount = pev->health > 0.0f ? pev->health : sk_battery.value;

		if ( pPlayer->TakeArmor( amount, DMG_GENERIC, m_iHealthCap ) )
		{
			int pct;
			char szcharge[64];

			MESSAGE_BEGIN( MSG_ONE, gmsgItemPickup, NULL, ENT( pPlayer->pev ) );
				WRITE_STRING( STRING(pev->classname) );
			MESSAGE_END();

			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM );

			if ( g_pGameRules->ItemShouldRespawn( this ) )
				Respawn();
			else
				UTIL_Remove( this );

			
			// Suit reports new power level
			// For some reason this wasn't working in release build -- round it.
			pct = (int)( (float)(pPlayer->pev->armorvalue * 100.0) * (1.0/MAX_NORMAL_BATTERY) + 0.5);
			pct = (pct / 5);
			if (pct > 0)
				pct--;
		
			sprintf( szcharge,"!HEV_%1dP", pct );
			
			//EMIT_SOUND_SUIT(ENT(pev), szcharge);
			pPlayer->SetSuitUpdate(szcharge, FALSE, SUIT_NEXT_IN_30SEC);
			return TRUE;		
		}
		return FALSE;
	}

private:
	int m_iHealthCap;
};

LINK_ENTITY_TO_CLASS(item_battery, CItemBattery);

class CItemHelmet : public CItemBattery
{
	void Spawn( void )
	{
		Precache();

		const char *model = SetupModel() ? STRING( pev->model ) : "models/barney_helmet.mdl";

		SET_MODEL( ENT( pev ), model );
		Item_SetDefaultSize( pev, Vector( -7.9f, -7.9f, 0 ), Vector( 7.9f, 7.9f, 19.9f ) );
		CItem::Spawn();
	}

	void Precache( void )
	{
		CBaseEntity::Precache();

		const char *model = pev->model ? STRING( pev->model ) : "models/barney_helmet.mdl";

		UTIL_PrecacheModel( this, model );
		PrecacheSound_Intercept( this, "items/gunpickup2.wav" );
	}
};

LINK_ENTITY_TO_CLASS( item_helmet, CItemHelmet );

class CItemArmorVest : public CItemBattery
{
	void Spawn( void )
	{
		Precache();

		const char *model = SetupModel() ? STRING( pev->model ) : "models/barney_vest.mdl";

		SET_MODEL( ENT( pev ), model );
		Item_SetDefaultSize( pev, Vector( -10.9f, -10.9f, 0 ), Vector( 10.9f, 10.9f, 35.9f ) );
		CItem::Spawn();
	}

	void Precache( void )
	{
		CBaseEntity::Precache();

		const char *model = pev->model ? STRING( pev->model ) : "models/barney_vest.mdl";

		UTIL_PrecacheModel( this, model );
		PrecacheSound_Intercept( this, "items/gunpickup2.wav" );
	}
};

LINK_ENTITY_TO_CLASS( item_armorvest, CItemArmorVest );

class CItemGeneric : public CItem
{
	void Spawn( void )
	{
		Precache();

		const char *model = SetupModel() ? STRING( pev->model ) : "models/egg.mdl";

		SET_MODEL( edict(), model );
		Item_SetDefaultSize( pev, g_vecZero, g_vecZero );
		CItem::Spawn();
	}

	void Precache( void )
	{
		const char *model = pev->model ? STRING( pev->model ) : "models/egg.mdl";

		UTIL_PrecacheModel( this, model );
		CBaseEntity::Precache();
	}

	void Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
	{
		SUB_UseTargets( caller, USE_TOGGLE, 0 );
	}
};

LINK_ENTITY_TO_CLASS( item_generic, CItemGeneric );


class CItemAntidote : public CItem
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL( edict(), SetupModel() ? STRING( pev->model ) : "models/w_antidote.mdl" );
		Item_SetDefaultSize( pev, Vector( -3.9f, -3.9f, 0 ), Vector( 3.9f, 3.9f, 15.9f ) );
		CItem::Spawn( );
	}
	void Precache( void )
	{
		UTIL_PrecacheModel( this, pev->model ? STRING( pev->model ) : "models/w_antidote.mdl" );
		CBaseEntity::Precache();
		PrecacheSound_Intercept( this, "items/airtank1.wav" );
	}
	BOOL MyTouch( CBasePlayer *pPlayer )
	{
		if ( !( pPlayer->m_bitsDamageType & DMG_POISON ) )
			return FALSE;

		pPlayer->m_bitsDamageType &= ~DMG_POISON;
		pPlayer->SetSuitUpdate( "!HEV_DET4", FALSE, SUIT_NEXT_IN_1MIN );
		pPlayer->m_rgItems[ITEM_ANTIDOTE] += 1;

		EMIT_SOUND( edict(), CHAN_ITEM, "items/airtank1.wav", 1, ATTN_NORM );

		if ( g_pGameRules->ItemShouldRespawn( this ) )
			Respawn();
		else
			UTIL_Remove( this );

		return TRUE;
	}
};

LINK_ENTITY_TO_CLASS(item_antidote, CItemAntidote);


class CItemSecurity : public CItem
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL( edict(), SetupModel() ? STRING( pev->model ) : "models/w_security.mdl" );
		Item_SetDefaultSize( pev, Vector( -11.9f, -11.9f, 0 ), Vector( 11.9f, 11.9f, 0.9f ) );
		CItem::Spawn( );
	}
	void Precache( void )
	{
		UTIL_PrecacheModel( this, pev->model ? STRING( pev->model ) : "models/w_security.mdl" );
		CBaseEntity::Precache();
	}
	BOOL MyTouch( CBasePlayer *pPlayer )
	{
		pPlayer->m_rgItems[ITEM_SECURITY] += 1;
		return TRUE;
	}
};

LINK_ENTITY_TO_CLASS(item_security, CItemSecurity);

class CItemLongJump : public CItem
{
	void Spawn( void )
	{ 
		Precache( );

		const char *model = SetupModel() ? STRING( pev->model )
		    : ( CClassicMode::GetInstance()->IsEnabled() ? "models/hlclassic/w_longjump.mdl" : "models/w_longjump.mdl" );

		SET_MODEL( edict(), model );
		Item_SetDefaultSize( pev, Vector( -13.9f, -13.9f, 0 ), Vector( 13.9f, 13.9f, 35.9f ) );
		CItem::Spawn( );
	}
	void Precache( void )
	{
		const char *model = pev->model ? STRING( pev->model )
		    : ( CClassicMode::GetInstance()->IsEnabled() ? "models/hlclassic/w_longjump.mdl" : "models/w_longjump.mdl" );

		UTIL_PrecacheModel( this, model );
		CBaseEntity::Precache();
	}
	BOOL MyTouch( CBasePlayer *pPlayer )
	{
		if ( pPlayer->m_fLongJump )
		{
			return FALSE;
		}

		if ( ( pPlayer->pev->weapons & (1<<WEAPON_SUIT) ) )
		{
			pPlayer->m_fLongJump = TRUE;// player now has longjump module

			g_engfuncs.pfnSetPhysicsKeyValue( pPlayer->edict(), "slj", "1" );

			MESSAGE_BEGIN( MSG_ONE, gmsgItemPickup, NULL, pPlayer->pev );
				WRITE_STRING( STRING(pev->classname) );
			MESSAGE_END();

			EMIT_SOUND_SUIT( pPlayer->edict(), "!HEV_A1" );	// Play the longjump sound UNDONE: Kelly? correct sound?
			return TRUE;		
		}
		return FALSE;
	}
};

LINK_ENTITY_TO_CLASS( item_longjump, CItemLongJump );
