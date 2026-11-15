#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "gamerules.h"
#include "item_inventory.h"
#include "InventoryRules.h"
#include "PickupUtils.h"
#include "user_messages.h"

#define SF_ITEM_TOUCH_ONLY		128
#define SF_ITEM_USE_WITHOUT_LOS	512
#define SF_ITEM_START_IN_SOLID	( 1 << 24 )

LINK_ENTITY_TO_CLASS( item_inventory, CItemInventory );

CItemInventory::CItemInventory()
    : m_vecSpawnOrigin( g_vecZero ),
      m_vecSpawnAngles( g_vecZero ),
      m_iMoveType( 0 ),
      m_szItemName( 0 ),
      m_szItemGroup( 0 ),
      m_szDisplayName( 0 ),
      m_szDescription( 0 ),
      m_iszIcon( 0 ),
      m_iCollectLimit( 0 ),
      m_iActivateLimit( 0 ),
      m_flWeight( 1 ),
      m_fHiddenWhenCarried( false ),
      m_iIdleSkin( 0 ),
      m_iIdleBody( 0 ),
      m_szIdleSequenceName( 0 ),
      m_iIdleSequence( 0 ),
      m_iCarriedSkin( 0 ),
      m_iCarriedBody( 0 ),
      m_szCarriedSequenceName( 0 ),
      m_iCarriedSequence( 0 ),
      m_szAllowedTargetNames( 0 ),
      m_szAllowedClassNames( 0 ),
      m_szAllowedTeams( 0 ),
      m_iAllowedNpcClassify( 0 ),
      m_szRequiresItemName( 0 ),
      m_szRequiresItemGroup( 0 ),
      m_iRequiresItemGroupNum( 0 ),
      m_szItemNameMoved( 0 ),
      m_szCantHaveItemName( 0 ),
      m_szCantHaveItemGroup( 0 ),
      m_iCantHaveItemGroupNum( 0 ),
      m_szItemNameNotMoved( 0 ),
      m_flMaximumHoldTime( 0 ),
      m_flActivateWaitTime( 0 ),
      m_flWearOutTime( 0 ),
      m_fCanBeActivated( false ),
      m_fHoldTimeWaitUntilActivated( false ),
      m_fCanBeDropped( false ),
      m_flReturnTime( 0 ),
      m_fDelayedRespawn( false ),
      m_fKeepOnDeath( false ),
      m_fKeepOnRespawn( false ),
      m_szTriggerOnCollectSelf( 0 ),
      m_szTriggerOnCollectTeam( 0 ),
      m_szTriggerOnCollectOther( 0 ),
      m_szTriggerOnCantCollectSelf( 0 ),
      m_szTriggerOnCantCollectTeam( 0 ),
      m_szTriggerOnCantCollectOther( 0 ),
      m_szTriggerOnDropSelf( 0 ),
      m_szTriggerOnDropTeam( 0 ),
      m_szTriggerOnDropOther( 0 ),
      m_szTriggerOnCantDropSelf( 0 ),
      m_szTriggerOnCantDropTeam( 0 ),
      m_szTriggerOnCantDropOther( 0 ),
      m_szTriggerOnActivateSelf( 0 ),
      m_szTriggerOnActivateTeam( 0 ),
      m_szTriggerOnActivateOther( 0 ),
      m_szTriggerOnCantActivateSelf( 0 ),
      m_szTriggerOnCantActivateTeam( 0 ),
      m_szTriggerOnCantActivateOther( 0 ),
      m_szTriggerOnUseSelf( 0 ),
      m_szTriggerOnUseTeam( 0 ),
      m_szTriggerOnUseOther( 0 ),
      m_szTriggerOnWearingOutSelf( 0 ),
      m_szTriggerOnWearingOutTeam( 0 ),
      m_szTriggerOnWearingOutOther( 0 ),
      m_szTriggerOnReturnSelf( 0 ),
      m_szTriggerOnReturnTeam( 0 ),
      m_szTriggerOnReturnOther( 0 ),
      m_szTriggerOnMaterialise( 0 ),
      m_szTriggerOnDestroy( 0 ),
      m_fEffectsWaitUntilActivated( false ),
      m_fEffectsPermanent( false ),
      m_vecEffectGlowColor( g_vecZero ),
      m_fEffectBlockWeapons( false ),
      m_fEffectInvulnerable( false ),
      m_fEffectInvisible( false ),
      m_fEffectNonSolid( false ),
      m_flEffectRespiration( 0 ),
      m_flEffectGravity( 1 ),
      m_flEffectFriction( 1 ),
      m_flEffectSpeed( 1 ),
      m_flEffectDamage( 1 ),
      m_iCollectCount( 0 ),
      m_iActivateCount( 0 ),
      m_flTouchedTime( 0 ),
      m_flCollectTime( 0 ),
      m_flDropTime( 0 ),
      m_flActivateTime( 0 ),
      m_fRemovedByForced( false ),
      m_fHandledWearingOut( false )
{
	m_hHolder = NULL;
}

CBaseMonster *CItemInventory::GetHolder( void )
{
	edict_t *entry = m_hHolder.Get();

	return entry && !entry->free ? (CBaseMonster *) GET_PRIVATE( entry ) : NULL;
}

void CItemInventory::OnCreate( void )
{
	CBaseEntity::OnCreate();
	m_RespawnDelay.OnCreate( "mp_item_respawndelay" );
}

void CItemInventory::KeyValue( KeyValueData *data )
{
	if ( FStrEq( data->szKeyName, "minhullsize" ) )
	{
		UTIL_StringToVector( pev->vuser1, data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "maxhullsize" ) )
	{
		UTIL_StringToVector( pev->vuser2, data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "effect_glow" ) )
	{
		UTIL_StringToVector( m_vecEffectGlowColor, data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "item_name" ) )
	{
		m_szItemName = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "item_group" ) )
	{
		m_szItemGroup = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "display_name" ) )
	{
		m_szDisplayName = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "description" ) )
	{
		m_szDescription = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "item_icon" ) )
	{
		m_iszIcon = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "sequencename" ) )
	{
		m_szIdleSequenceName = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "carried_sequencename" ) )
	{
		m_szCarriedSequenceName = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "filter_targetnames" ) )
	{
		m_szAllowedTargetNames = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "filter_classnames" ) )
	{
		m_szAllowedClassNames = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "filter_teams" ) )
	{
		m_szAllowedTeams = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "item_name_required" ) )
	{
		m_szRequiresItemName = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "item_group_required" ) )
	{
		m_szRequiresItemGroup = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "item_name_moved" ) )
	{
		m_szItemNameMoved = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "item_name_canthave" ) )
	{
		m_szCantHaveItemName = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "item_group_canthave" ) )
	{
		m_szCantHaveItemGroup = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "item_name_not_moved" ) )
	{
		m_szItemNameNotMoved = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_collect_self" ) )
	{
		m_szTriggerOnCollectSelf = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_collect_team" ) )
	{
		m_szTriggerOnCollectTeam = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_collect_other" ) )
	{
		m_szTriggerOnCollectOther = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_collect" ) )
	{
		m_szTriggerOnCollectSelf = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_cant_collect_self" ) )
	{
		m_szTriggerOnCantCollectSelf = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_cant_collect_team" ) )
	{
		m_szTriggerOnCantCollectTeam = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_cant_collect_other" ) )
	{
		m_szTriggerOnCantCollectOther = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_cant_collect" ) )
	{
		m_szTriggerOnCantCollectSelf = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_drop_self" ) )
	{
		m_szTriggerOnDropSelf = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_drop_team" ) )
	{
		m_szTriggerOnDropTeam = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_drop_other" ) )
	{
		m_szTriggerOnDropOther = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_drop" ) )
	{
		m_szTriggerOnDropSelf = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_cant_drop_self" ) )
	{
		m_szTriggerOnCantDropSelf = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_cant_drop_team" ) )
	{
		m_szTriggerOnCantDropTeam = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_cant_drop_other" ) )
	{
		m_szTriggerOnCantDropOther = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_cant_drop" ) )
	{
		m_szTriggerOnCantDropSelf = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_activate_self" ) )
	{
		m_szTriggerOnActivateSelf = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_activate_team" ) )
	{
		m_szTriggerOnActivateTeam = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_activate_other" ) )
	{
		m_szTriggerOnActivateOther = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_activate" ) )
	{
		m_szTriggerOnActivateSelf = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_cant_activate_self" ) )
	{
		m_szTriggerOnCantActivateSelf = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_cant_activate_team" ) )
	{
		m_szTriggerOnCantActivateTeam = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_cant_activate_other" ) )
	{
		m_szTriggerOnCantActivateOther = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_cant_activate" ) )
	{
		m_szTriggerOnCantActivateSelf = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_use_self" ) )
	{
		m_szTriggerOnUseSelf = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_use_team" ) )
	{
		m_szTriggerOnUseTeam = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_use_other" ) )
	{
		m_szTriggerOnUseOther = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_use" ) )
	{
		m_szTriggerOnUseSelf = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_wearing_out_self" ) )
	{
		m_szTriggerOnWearingOutSelf = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_wearing_out_team" ) )
	{
		m_szTriggerOnWearingOutTeam = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_wearing_out_other" ) )
	{
		m_szTriggerOnWearingOutOther = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_wearing_out" ) )
	{
		m_szTriggerOnWearingOutSelf = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_return_self" ) )
	{
		m_szTriggerOnReturnSelf = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_return_team" ) )
	{
		m_szTriggerOnReturnTeam = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_return_other" ) )
	{
		m_szTriggerOnReturnOther = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_return" ) )
	{
		m_szTriggerOnReturnSelf = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_materialise" ) )
	{
		m_szTriggerOnMaterialise = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "target_on_destroy" ) )
	{
		m_szTriggerOnDestroy = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "carried_skin" ) )
	{
		m_iCarriedSkin = atoi( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "carried_body" ) )
	{
		m_iCarriedBody = atoi( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "carried_sequence" ) )
	{
		m_iCarriedSequence = atoi( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "filter_npc_classifications" ) )
	{
		m_iAllowedNpcClassify = atoi( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "collect_limit" ) )
	{
		m_iCollectLimit = V_max( atoi( data->szValue ), 0 );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "activate_limit" ) )
	{
		m_iActivateLimit = V_max( atoi( data->szValue ), 0 );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "item_group_required_num" ) )
	{
		m_iRequiresItemGroupNum = V_max( atoi( data->szValue ), 0 );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "item_group_canthave_num" ) )
	{
		m_iCantHaveItemGroupNum = V_max( atoi( data->szValue ), 0 );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "carried_hidden" ) )
	{
		m_fHiddenWhenCarried = atoi( data->szValue ) > 0;
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "holder_can_activate" ) )
	{
		m_fCanBeActivated = atoi( data->szValue ) > 0;
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "holder_timelimit_wait_until_activated" ) )
	{
		m_fHoldTimeWaitUntilActivated = atoi( data->szValue ) > 0;
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "holder_can_drop" ) )
	{
		m_fCanBeDropped = atoi( data->szValue ) > 0;
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "return_delay_respawn" ) )
	{
		m_fDelayedRespawn = atoi( data->szValue ) > 0;
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "holder_keep_on_death" ) )
	{
		m_fKeepOnDeath = atoi( data->szValue ) > 0;
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "holder_keep_on_respawn" ) )
	{
		m_fKeepOnRespawn = atoi( data->szValue ) > 0;
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "effects_wait_until_activated" ) )
	{
		m_fEffectsWaitUntilActivated = atoi( data->szValue ) > 0;
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "effects_permanent" ) )
	{
		m_fEffectsPermanent = atoi( data->szValue ) > 0;
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "effect_block_weapons" ) )
	{
		m_fEffectBlockWeapons = atoi( data->szValue ) > 0;
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "effect_invulnerable" ) )
	{
		m_fEffectInvulnerable = atoi( data->szValue ) > 0;
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "effect_invisible" ) )
	{
		m_fEffectInvisible = atoi( data->szValue ) > 0;
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "effect_nonsolid" ) )
	{
		m_fEffectNonSolid = atoi( data->szValue ) > 0;
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "holder_timelimit" ) )
	{
		m_flMaximumHoldTime = fmax( atof( data->szValue ), 0.0 );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "holder_time_activate_wait" ) )
	{
		m_flActivateWaitTime = fmax( atof( data->szValue ), 0.0 );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "holder_time_wearout" ) )
	{
		m_flWearOutTime = fmax( atof( data->szValue ), 0.0 );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "weight" ) )
	{
		m_flWeight = fmax( fmin( atof( data->szValue ), 100.0 ), 0.0 );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "return_timelimit" ) )
	{
		m_flReturnTime = fmax( atof( data->szValue ), -1.0 );

		if ( m_flReturnTime < 0 )
			m_flReturnTime = -1;
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "effect_respiration" ) )
	{
		m_flEffectRespiration = atof( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "effect_gravity" ) )
	{
		m_flEffectGravity = fmax( atof( data->szValue ) / 100.0, 0.0 );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "effect_friction" ) )
	{
		m_flEffectFriction = atof( data->szValue ) / 100.0;
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "effect_speed" ) )
	{
		m_flEffectSpeed = atof( data->szValue ) / 100.0;
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "effect_damage" ) )
	{
		m_flEffectDamage = atof( data->szValue ) / 100.0;
		data->fHandled = TRUE;
	}
	else if ( !m_RespawnDelay.KeyValue( this, data ) )
		CBaseEntity::KeyValue( data );
}

void CItemInventory::Precache( void )
{
	CBaseEntity::Precache();
	UTIL_PrecacheModel( this, pev->model ? STRING( pev->model ) : "models/key.mdl" );
	PrecacheSound_Intercept( this, "tfc/items/itembk2.wav" );
}

int CItemInventory::ObjectCaps( void )
{
	return pev->spawnflags & SF_ITEM_TOUCH_ONLY ? FCAP_ACROSS_TRANSITION : FCAP_ACROSS_TRANSITION | FCAP_IMPULSE_USE;
}

void CItemInventory::OnSetOriginByMap( void )
{
	if ( GetHolder() == NULL && m_flDropTime == 0 )
	{
		m_vecSpawnOrigin = pev->origin;
		m_vecSpawnAngles = pev->angles;
	}
}

void CItemInventory::Spawn( void )
{
	Precache();
	m_vecSpawnOrigin = pev->origin;
	m_vecSpawnAngles = pev->angles;
	bool customModel = SetupModel();

	UTIL_SetModel( this, customModel ? STRING( pev->model ) : "models/key.mdl" );
	pev->solid = SOLID_TRIGGER;
	Item_SetDefaultSize( pev, Vector( -7.9f, -7.9f, 0 ), Vector( 7.9f, 7.9f, 15.9f ) );
	UTIL_SetOrigin( pev, pev->origin );

	// Allow item to float
	if ( pev->movetype != MOVETYPE_FLY && pev->movetype != MOVETYPE_NOCLIP )
		pev->movetype = MOVETYPE_TOSS;

	m_iMoveType = pev->movetype;
	m_hHolder = NULL;
	pev->owner = NULL;
	m_fRemovedByForced = false;
	m_iIdleSequence = pev->sequence;
	m_iIdleSkin = pev->skin;
	m_iIdleBody = pev->body;
	SetSequenceFromLookup( pev, m_szIdleSequenceName, m_iIdleSequence );

	if ( m_iCarriedSkin == 0 )
		m_iCarriedSkin = m_iIdleSkin;

	if ( m_iCarriedBody == 0 )
		m_iCarriedBody = m_iIdleBody;

	if ( !m_szCarriedSequenceName || !strlen( STRING( m_szCarriedSequenceName ) ) )
		m_szCarriedSequenceName = m_szIdleSequenceName;

	if ( m_iCarriedSequence == 0 )
		m_iCarriedSequence = m_iIdleSequence;

	if ( !( pev->spawnflags & SF_ITEM_USE_ONLY ) )
		SetTouch( NULL );

	SetUse( NULL );

	if ( pev->movetype != MOVETYPE_FLY && pev->movetype != MOVETYPE_NOCLIP )
	{
		edict_t *world = INDEXENT( 0 );

		if ( world )
		{
			Vector position = pev->origin;
			TraceResult trace;

			position.z += 18;
			UTIL_TraceModel( position, position, head_hull, world, &trace );

			if ( trace.fStartSolid )
				pev->spawnflags |= SF_ITEM_START_IN_SOLID;
		}

		if ( !DROP_TO_FLOOR( edict() ) )
		{
			ALERT( at_console, "Item '%s' fell out of level at X%f Y%f Z%f.\n", STRING( pev->classname ), pev->origin.x, pev->origin.y, pev->origin.z );
			UTIL_Remove( this );
		}
	}
}

void CItemInventory::UpdateOnRemove( void )
{
	if ( GetHolder() != NULL || ( pev->owner && !pev->owner->free && GET_PRIVATE( pev->owner ) != NULL ) )
		Drop( true, this, false );

	CBaseEntity::UpdateOnRemove();
}

CBaseEntity *CItemInventory::Respawn( void )
{
	SetTouch( NULL );
	SetUse( NULL );

	if ( m_iCollectLimit > 0 && m_iCollectCount >= m_iCollectLimit )
		Destroy( false );

	pev->effects |= EF_NODRAW;
	UTIL_SetOrigin( pev, m_vecSpawnOrigin );
	pev->angles = m_vecSpawnAngles;
	pev->movetype = m_iMoveType;
	pev->solid = SOLID_TRIGGER;

	if ( pev->movetype != MOVETYPE_FLY && pev->movetype != MOVETYPE_NOCLIP && !DROP_TO_FLOOR( edict() ) )
	{
		ALERT( at_console, "Item '%s' fell out of level at X%f Y%f Z%f.\n", STRING( pev->classname ), pev->origin.x, pev->origin.y, pev->origin.z );
		UTIL_Remove( this );
		return NULL;
	}

	SetThink( &CItemInventory::Materialize );
	pev->nextthink = m_fDelayedRespawn && g_pGameRules ? g_pGameRules->FlInventoryRespawnTime( this ) : gpGlobals->time + 0.01;
	return this;
}

void CItemInventory::Materialize( void )
{
	if ( pev->effects & EF_NODRAW )
	{
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "tfc/items/itembk2.wav", 1, ATTN_NORM, 0, 150 );
		pev->effects &= ~EF_NODRAW;
		pev->effects |= EF_MUZZLEFLASH;

		if ( m_szTriggerOnMaterialise && m_flDropTime > 0 )
			FireTargets( STRING( m_szTriggerOnMaterialise ), this, this, USE_TOGGLE, 0 );
	}

	m_fRemovedByForced = false;
	m_flCollectTime = 0;
	m_flActivateTime = 0;
	m_flDropTime = 0;
	SetUse( NULL );
	SetThink( &CItemInventory::IdleThink );

	if ( !( pev->spawnflags & SF_ITEM_USE_ONLY ) )
		SetTouch( NULL );
}

void CItemInventory::IdleThink( void )
{
	if ( m_flReturnTime == -1 )
	{
		SetThink( NULL );
		return;
	}

	if ( gpGlobals->time - m_flDropTime > m_flReturnTime )
		Return( false );

	pev->nextthink = gpGlobals->time + 0.01;
}

void CItemInventory::HeldThink( void )
{
	if ( GetHolder() != NULL )
	{
		float start = m_fCanBeActivated && m_fHoldTimeWaitUntilActivated ? m_flActivateTime : m_flCollectTime;

		if ( m_flMaximumHoldTime > 0 && start > 0 )
		{
			if ( m_flWearOutTime > 0 && !m_fHandledWearingOut && gpGlobals->time - start > m_flMaximumHoldTime - m_flWearOutTime )
			{
				HandleTriggers( GetHolder(), m_szTriggerOnWearingOutSelf, m_szTriggerOnWearingOutTeam, m_szTriggerOnWearingOutOther );
				m_fHandledWearingOut = true;
			}

			if ( gpGlobals->time - start > m_flMaximumHoldTime )
			{
				Drop( true, this, false );
				m_fRemovedByForced = false;
				return;
			}
		}

		if ( GetHolder() != NULL && !GetHolder()->IsAlive() && !m_fKeepOnDeath )
		{
			Drop( true, this, false );
			m_fRemovedByForced = false;
			return;
		}

		pev->nextthink = gpGlobals->time + 0.01;
	}
	else
		Drop( true, this, false );
}

void CItemInventory::HandleTriggers( CBaseEntity *activator, string_t self, string_t team, string_t others )
{
	if ( !activator || !( activator->IsMonster() || activator->IsPlayer() ) )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *player = UTIL_PlayerByIndex( i );

		if ( !player )
			continue;

		string_t target;

		if ( player == activator )
			target = self;
		else if ( g_pGameRules && g_pGameRules->PlayerRelationship( player, activator ) == GR_TEAMMATE )
		{
			if ( g_iMapForGameVersion >= 524 || team )
				target = team;
			else
				target = others;
		}
		else
			target = others;

		if ( target )
			FireTargets( STRING( target ), player, this, USE_TOGGLE, 0 );
	}
}

bool CItemInventory::Activate( bool ignoreTriggers )
{
	if ( GetHolder() == NULL )
		return false;

	if ( !m_fCanBeActivated || ( m_iActivateLimit > 0 && m_iActivateCount >= m_iActivateLimit )
		|| ( m_flActivateWaitTime > 0 && m_flActivateWaitTime >= gpGlobals->time - m_flActivateTime && m_flActivateTime != 0 ) )
	{
		if ( !ignoreTriggers )
			HandleTriggers( GetHolder(), m_szTriggerOnCantActivateSelf, m_szTriggerOnCantActivateTeam, m_szTriggerOnCantActivateOther );

		return false;
	}

	m_iActivateCount++;
	m_flActivateTime = gpGlobals->time;
	m_fHandledWearingOut = false;

	if ( m_fEffectsWaitUntilActivated )
		AddItemEffects();

	HandleTriggers( GetHolder(), m_szTriggerOnActivateSelf, m_szTriggerOnActivateTeam, m_szTriggerOnActivateOther );
	return true;
}

void CItemInventory::Return( bool ignoreTriggers )
{
	pev->movetype = MOVETYPE_NONE;
	pev->aiment = NULL;
	pev->solid = SOLID_NOT;
	float holderTimeLimit = m_flMaximumHoldTime;
	float returnTimeLimit = m_flReturnTime;

	m_flMaximumHoldTime = -1;
	m_flReturnTime = 0;

	if ( GetHolder() != NULL || ( pev->owner && !pev->owner->free && GET_PRIVATE( pev->owner ) != NULL ) )
	{
		if ( !ignoreTriggers )
			HandleTriggers( GetHolder(), m_szTriggerOnReturnSelf, m_szTriggerOnReturnTeam, m_szTriggerOnReturnOther );

		Drop( true, this, true );
	}

	Respawn();
	m_flMaximumHoldTime = holderTimeLimit;
	m_flReturnTime = returnTimeLimit;
}

void CItemInventory::Destroy( bool ignoreTriggers )
{
	if ( GetHolder() != NULL )
		Return( true );

	if ( !ignoreTriggers && m_szTriggerOnDestroy )
		FireTargets( STRING( m_szTriggerOnDestroy ), this, this, USE_TOGGLE, 0 );

	UTIL_Remove( this );
}

bool CItemInventory::Drop( bool force, CBaseEntity *caller, bool ignoreTriggers )
{
	if ( GetHolder() == NULL && !force )
		return false;

	if ( caller && caller != (CBaseEntity *) GetHolder() && caller != this )
		m_fRemovedByForced = true;

	CBaseMonster *holder = (CBaseMonster *) (CBaseEntity *) GetHolder();
	CBasePlayer *player = holder && holder->IsPlayer() ? dynamic_cast<CBasePlayer *>( holder ) : NULL;

	if ( !m_fCanBeDropped && !force )
	{
		if ( !ignoreTriggers )
			HandleTriggers( GetHolder(), m_szTriggerOnCantDropSelf, m_szTriggerOnCantDropTeam, m_szTriggerOnCantDropOther );

		return false;
	}

	pev->effects &= ~EF_NODRAW;
	pev->skin = m_iIdleSkin;
	pev->body = m_iIdleBody;
	pev->sequence = m_iIdleSequence;
	SetSequenceFromLookup( pev, m_szIdleSequenceName, m_iIdleSequence );
	m_fHandledWearingOut = false;
	m_flActivateTime = 0;

	if ( !ignoreTriggers )
		HandleTriggers( GetHolder(), m_szTriggerOnDropSelf, m_szTriggerOnDropTeam, m_szTriggerOnDropOther );

	RemoveItemEffects();

	if ( player )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgInvRemove, NULL, player->edict() );
		WRITE_LONG( entindex() );
		WRITE_BYTE( 0 );
		MESSAGE_END();
	}

	if ( holder )
	{
		CInventoryItemNode *previous = holder->m_pInventory;
		CInventoryItemNode *node = previous;

		while ( node )
		{
			edict_t *entry = node->m_hItem.Get();

			if ( entry && !entry->free && GET_PRIVATE( entry ) == this )
			{
				if ( node == holder->m_pInventory )
					holder->m_pInventory = node->m_pNext;
				else
					previous->m_pNext = node->m_pNext;

				node->m_hItem = NULL;
				delete node;
				break;
			}

			previous = node;
			node = node->m_pNext;
		}
	}

	if ( m_flReturnTime != 0 && GetHolder() != NULL )
	{
		UTIL_MakeVectors( GetHolder()->pev->v_angle );
		UTIL_SetOrigin( pev, GetHolder()->pev->origin + gpGlobals->v_forward * 10 );
		pev->movetype = MOVETYPE_TOSS;
		pev->aiment = NULL;
		pev->solid = SOLID_TRIGGER;
		pev->velocity = gpGlobals->v_forward * 300 + gpGlobals->v_forward * 100;
		pev->angles.x = 0;
		pev->angles.z = 0;
	}

	m_hHolder = NULL;
	pev->owner = NULL;
	m_flDropTime = gpGlobals->time;
	SetThink( &CItemInventory::IdleThink );
	SetTouch( NULL );
	SetUse( NULL );
	pev->nextthink = gpGlobals->time + 0.01f;

	if ( m_iCollectLimit > 0 && m_iCollectCount >= m_iCollectLimit )
		Destroy( false );

	return true;
}

void CItemInventory::AddItemEffects( void )
{
	if ( GetHolder() == NULL )
		return;

	CBaseMonster *holder = (CBaseMonster *) (CBaseEntity *) GetHolder();

	if ( m_vecEffectGlowColor != g_vecZero )
		holder->m_vecEffectGlowColor = holder->m_vecEffectGlowColor + m_vecEffectGlowColor;

	if ( m_fEffectBlockWeapons )
		holder->m_iEffectBlockWeapons++;

	if ( m_fEffectInvulnerable )
		holder->m_iEffectInvulnerable++;

	if ( m_fEffectInvisible )
		holder->m_iEffectInvisible++;

	if ( m_fEffectNonSolid )
		holder->m_iEffectNonSolid++;

	if ( m_flEffectRespiration != 0 )
		holder->m_flEffectRespiration += m_flEffectRespiration;

	if ( m_flEffectGravity != 1 )
		holder->m_flEffectGravity += m_flEffectGravity - 1;

	if ( m_flEffectFriction != 1 )
		holder->m_flEffectFriction += m_flEffectFriction - 1;

	if ( m_flEffectSpeed != 1 )
		holder->m_flEffectSpeed += m_flEffectSpeed - 1;

	if ( m_flEffectDamage != 1 )
		holder->m_flEffectDamage += m_flEffectDamage - 1;

	holder->ApplyEffects();
}

void CItemInventory::RemoveItemEffects( void )
{
	if ( GetHolder() == NULL )
		return;

	CBaseMonster *holder = (CBaseMonster *) (CBaseEntity *) GetHolder();

	if ( !m_fEffectsPermanent )
	{
		if ( m_vecEffectGlowColor != g_vecZero )
			holder->m_vecEffectGlowColor = holder->m_vecEffectGlowColor - m_vecEffectGlowColor;

		if ( m_fEffectBlockWeapons )
			holder->m_iEffectBlockWeapons--;

		if ( m_fEffectInvulnerable )
			holder->m_iEffectInvulnerable--;

		if ( m_fEffectInvisible )
			holder->m_iEffectInvisible--;

		if ( m_fEffectNonSolid )
			holder->m_iEffectNonSolid--;

		if ( m_flEffectRespiration != 0 )
			holder->m_flEffectRespiration -= m_flEffectRespiration;

		if ( m_flEffectGravity != 1 )
			holder->m_flEffectGravity -= m_flEffectGravity - 1;

		if ( m_flEffectFriction != 1 )
			holder->m_flEffectFriction -= m_flEffectFriction - 1;

		if ( m_flEffectSpeed != 1 )
			holder->m_flEffectSpeed -= m_flEffectSpeed - 1;

		if ( m_flEffectDamage != 1 )
			holder->m_flEffectDamage -= m_flEffectDamage - 1;
	}

	holder->ApplyEffects();
}

bool CItemInventory::CanCollect( CBaseEntity *entity )
{
	if ( !IsValidInventoryHolder( entity ) )
		return false;

	CBaseMonster *holder = entity->MyMonsterPointer();

	if ( !holder )
		return false;

	if ( m_iCollectCount >= m_iCollectLimit && m_iCollectLimit > 0 )
	{
		Destroy( false );
		return false;
	}

	if ( Inventory_GetWeight( holder ) + m_flWeight > 100 )
		return false;

	if ( m_szAllowedTargetNames && entity->pev->targetname && !Inventory_CheckTokenStringRule( STRING( m_szAllowedTargetNames ), STRING( entity->pev->targetname ) ) )
		return false;

	if ( m_szAllowedClassNames && entity->pev->classname && !Inventory_CheckTokenStringRule( STRING( m_szAllowedClassNames ), STRING( entity->pev->classname ) ) )
		return false;

	if ( m_szAllowedTeams && !Inventory_CheckTokenStringRule( STRING( m_szAllowedTeams ), STRING( entity->pev->team ) ) )
		return false;

	if ( m_iAllowedNpcClassify && holder->IsMonster() && holder->Classify() != m_iAllowedNpcClassify )
		return false;

	if ( m_szRequiresItemName )
	{
		int required = 1;
		int found = 0;

		for ( const char *text = STRING( m_szRequiresItemName ); *text; text++ )
		{
			if ( *text == ';' && text[1] != ';' )
				required++;
		}

		for ( CInventoryItemNode *node = holder->m_pInventory; node; node = node->m_pNext )
		{
			edict_t *entry = node->m_hItem.Get();
			CItemInventory *item = entry && !entry->free ? (CItemInventory *) GET_PRIVATE( entry ) : NULL;

			if ( item && Inventory_CheckTokenStringRule( STRING( m_szRequiresItemName ), STRING( item->m_szItemName ) ) )
				found++;
		}

		if ( found < required )
			return false;
	}

	if ( m_szRequiresItemGroup )
	{
		char *buffer = NULL;
		char *context = NULL;
		const char *groups = STRING( m_szRequiresItemGroup );

		for ( char *group = UTIL_StringTokens( groups, ";", &buffer, &context ); group; group = UTIL_StringTokens( groups, ";", &buffer, &context ) )
		{
			int total = Inventory_ExistsInGroup( group );

			if ( total )
			{
				int held = Inventory_HaveNumFromGroup( holder, group );

				if ( ( m_iRequiresItemGroupNum == 0 && held < total ) || held < m_iRequiresItemGroupNum )
					return false;
			}
		}
	}

	if ( m_szItemNameMoved )
	{
		CBaseEntity *entity = NULL;

		while ( ( entity = UTIL_FindEntityByClassname( entity, "item_inventory" ) ) != NULL )
		{
			CItemInventory *item = (CItemInventory *) entity;

			if ( Inventory_CheckTokenStringRule( STRING( m_szItemNameMoved ), STRING( item->m_szItemName ) ) )
			{
				if ( item->GetHolder() != NULL )
					return false;

				if ( item->pev->origin.x == item->m_vecSpawnOrigin.x || item->pev->origin.y == item->m_vecSpawnOrigin.y || item->pev->origin.z == item->m_vecSpawnOrigin.z )
					return false;
			}
		}
	}

	if ( m_szCantHaveItemName )
	{
		for ( CInventoryItemNode *node = holder->m_pInventory; node; node = node->m_pNext )
		{
			edict_t *entry = node->m_hItem.Get();
			CItemInventory *item = entry && !entry->free ? (CItemInventory *) GET_PRIVATE( entry ) : NULL;

			if ( item && Inventory_CheckTokenStringRule( STRING( m_szCantHaveItemName ), STRING( item->m_szItemName ) ) )
				return false;
		}
	}

	if ( m_szCantHaveItemGroup )
	{
		char *buffer = NULL;
		char *context = NULL;
		const char *groups = STRING( m_szCantHaveItemGroup );

		for ( char *group = UTIL_StringTokens( groups, ";", &buffer, &context ); group; group = UTIL_StringTokens( groups, ";", &buffer, &context ) )
		{
			int total = Inventory_ExistsInGroup( group );

			if ( total )
			{
				int held = Inventory_HaveNumFromGroup( holder, group );

				if ( ( m_iCantHaveItemGroupNum == 0 && held >= total ) || held >= m_iCantHaveItemGroupNum )
					return false;
			}
		}
	}

	// The final stationary-item test reads item_name_moved again
	if ( m_szItemNameMoved )
	{
		CBaseEntity *entity = NULL;

		while ( ( entity = UTIL_FindEntityByClassname( entity, "item_inventory" ) ) != NULL )
		{
			CItemInventory *item = (CItemInventory *) entity;

			if ( Inventory_CheckTokenStringRule( STRING( m_szItemNameMoved ), STRING( item->m_szItemName ) )
				&& ( item->GetHolder() != NULL || item->pev->origin != item->m_vecSpawnOrigin ) )
				return false;
		}
	}

	return true;
}

bool CItemInventory::TryCollect( CBaseEntity *entity, bool ignoreTriggers )
{
	if ( ( pev->effects & EF_NODRAW ) || !IsValidInventoryHolder( entity ) )
		return false;

	if ( gpGlobals->time - m_flTouchedTime <= 1 && m_flTouchedTime != 0 )
		return false;

	if ( gpGlobals->time - m_flDropTime <= 3 && m_flDropTime != 0 )
		return false;

	m_flTouchedTime = gpGlobals->time;

	if ( !CanCollect( entity ) )
	{
		if ( !ignoreTriggers )
			HandleTriggers( entity, m_szTriggerOnCantCollectSelf, m_szTriggerOnCantCollectTeam, m_szTriggerOnCantCollectOther );

		return false;
	}

	CBaseMonster *holder = entity->MyMonsterPointer();

	if ( !holder )
		return false;

	m_iCollectCount++;
	m_flCollectTime = gpGlobals->time;
	m_hHolder = holder->edict() && !holder->edict()->free ? holder : NULL;
	pev->owner = holder->edict();
	CInventoryItemNode *last = holder->m_pInventory;

	while ( last && last->m_pNext )
		last = last->m_pNext;

	CInventoryItemNode *node = new CInventoryItemNode;

	node->m_hItem = edict() && !edict()->free ? this : NULL;
	node->m_pNext = NULL;

	if ( last )
		last->m_pNext = node;
	else
		holder->m_pInventory = node;

	if ( holder->IsPlayer() )
	{
		CBasePlayer *player = dynamic_cast<CBasePlayer *>( holder );

		if ( player )
		{
			unsigned int weight;

			memcpy( &weight, &m_flWeight, sizeof( weight ) );
			MESSAGE_BEGIN( MSG_ONE, gmsgInvAdd, NULL, player->edict() );
			WRITE_LONG( entindex() );
			WRITE_BYTE( m_fCanBeActivated );
			WRITE_BYTE( m_fCanBeDropped );
			WRITE_BYTE( m_fKeepOnRespawn );
			WRITE_BYTE( weight );
			WRITE_BYTE( weight >> 8 );
			WRITE_BYTE( weight >> 16 );
			WRITE_BYTE( weight >> 24 );
			WRITE_STRING( STRING( m_szItemName ) );
			WRITE_STRING( STRING( m_szDisplayName ) );
			WRITE_STRING( STRING( m_szDescription ) );
			WRITE_STRING( STRING( pev->model ) );
			WRITE_STRING( STRING( m_iszIcon ) );
			MESSAGE_END();
		}
	}

	pev->movetype = MOVETYPE_NONE;
	pev->aiment = NULL;
	pev->solid = SOLID_NOT;

	if ( m_fHiddenWhenCarried )
		pev->effects |= EF_NODRAW;
	else
	{
		pev->skin = m_iCarriedSkin;
		pev->body = m_iCarriedBody;
		pev->sequence = m_iCarriedSequence;
		SetSequenceFromLookup( pev, m_szCarriedSequenceName, m_iCarriedSequence );
		pev->movetype = MOVETYPE_FOLLOW;
		pev->aiment = GetHolder()->edict();
	}

	if ( !m_fEffectsWaitUntilActivated )
		AddItemEffects();

	if ( !ignoreTriggers )
		HandleTriggers( GetHolder(), m_szTriggerOnCollectSelf, m_szTriggerOnCollectTeam, m_szTriggerOnCollectOther );

	SetTouch( NULL );
	SetUse( NULL );
	SetThink( &CItemInventory::HeldThink );
	pev->nextthink = gpGlobals->time + 0.01;
	return true;
}

void CItemInventory::Touch( CBaseEntity *other )
{
	if ( !( pev->spawnflags & SF_ITEM_USE_ONLY ) )
		TryCollect( other, false );
}

void CItemInventory::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( !activator )
		return;

	if ( useType == USE_SET )
	{
		if ( pev->spawnflags & SF_ITEM_TOUCH_ONLY )
			return;

		if ( !( pev->spawnflags & SF_ITEM_USE_WITHOUT_LOS ) && activator == caller && !UTIL_HasLineOfSight( activator, this, false, false ) )
			return;
	}

	switch ( useType )
	{
		case USE_OFF:
			Drop( true, caller, false );
			break;
		case USE_ON:
			TryCollect( activator, false );
			break;
		case USE_SET:
			if ( GetHolder() == NULL )
				TryCollect( activator, false );
			break;
		case USE_TOGGLE:
			if ( GetHolder() != NULL )
				Drop( true, caller, false );
			else
				TryCollect( activator, false );
			break;
		case USE_KILL:
			Drop( true, caller, false );
			Return( false );
			Destroy( false );
			break;
	}
}

int Inventory_Count( CBaseMonster *holder )
{
	if ( !IsValidInventoryHolder( holder ) )
		return 0;

	int count = 0;

	for ( CInventoryItemNode *node = holder->m_pInventory; node; node = node->m_pNext )
		count++;

	return count;
}

float Inventory_GetWeight( CBaseMonster *holder )
{
	if ( !IsValidInventoryHolder( holder ) )
		return 0;

	float weight = 0;

	for ( CInventoryItemNode *node = holder->m_pInventory; node; node = node->m_pNext )
	{
		edict_t *entry = node->m_hItem.Get();
		CItemInventory *item = entry && !entry->free ? (CItemInventory *) GET_PRIVATE( entry ) : NULL;

		if ( item )
			weight += item->m_flWeight;
	}

	return weight;
}

void Inventory_RemoveAllFromHolder( CBaseMonster *holder, bool drop, bool onlyBlockWeapons, bool keepOnRespawn )
{
	if ( !IsValidInventoryHolder( holder ) )
		return;

	for ( CInventoryItemNode *node = holder->m_pInventory; node; )
	{
		CInventoryItemNode *next = node->m_pNext;
		edict_t *entry = node->m_hItem.Get();
		CItemInventory *item = entry && !entry->free ? (CItemInventory *) GET_PRIVATE( entry ) : NULL;

		if ( item && !( onlyBlockWeapons && !item->m_fEffectBlockWeapons ) && !( keepOnRespawn && item->m_fKeepOnRespawn ) )
		{
			if ( drop )
				item->Drop( false, NULL, false );
			else
				item->Return( false );
		}

		node = next;
	}

	if ( holder->IsPlayer() && !onlyBlockWeapons )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgInvRemove, NULL, holder->edict() );
		WRITE_LONG( 0 );
		WRITE_BYTE( keepOnRespawn );
		MESSAGE_END();
	}

	if ( Inventory_Count( holder ) <= 0 )
		holder->m_pInventory = NULL;
}
