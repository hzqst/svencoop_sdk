#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "CAdvFuncVehicle.h"
#include "func_tank.h"

#define SF_ADVVEHICLE_SLIDE_STEER 8
#define SF_ADVVEHICLE_EXPLOSIVES_ONLY 16
#define SF_ADVVEHICLE_ALWAYS_DRIFT 32
#define SF_ADVVEHICLE_NO_REPAIR 256

bool VehicleEdictValid( edict_t *entity )
{
	return entity && !entity->free && OFFSET( entity ) != 0;
}

CBaseEntity *VehicleEdictEntity( edict_t *entity )
{
	return entity && !entity->free ? (CBaseEntity *) GET_PRIVATE( entity ) : NULL;
}

CBaseEntity *VehicleHandleEntity( EHANDLE &handle )
{
	edict_t *entity = handle.Get();

	return VehicleEdictEntity( entity );
}

bool VehicleHasRepairWeapon( CBaseEntity *entity )
{
	if ( !entity || !entity->IsPlayer() )
		return false;

	CBasePlayer *player = static_cast<CBasePlayer *>( entity );
	CBasePlayerItem *weapon = player->m_pActiveItem;

	if ( !weapon )
		return false;

	ItemInfo info;

	weapon->GetItemInfo( &info );
	return info.iId == WEAPON_PIPEWRENCH;
}

class CAdvVehicleThinker : public CBaseEntity
{
public:
	void EXPORT ThinkerThink( void );

	edict_t *m_pVehicle;
};

class CVehicleSlot : public CBaseEntity
{
public:
	void Spawn( void );
	void Think( void );
};

LINK_ENTITY_TO_CLASS( func_adv_vehicle, CAdvFuncVehicle );
LINK_ENTITY_TO_CLASS( adv_vehicle_thinker, CAdvVehicleThinker );
LINK_ENTITY_TO_CLASS( info_adv_vehicle_control, CVehicleSlot );
LINK_ENTITY_TO_CLASS( info_adv_vehicle_seat, CVehicleSlot );
LINK_ENTITY_TO_CLASS( info_adv_vehicle_tire, CVehicleSlot );
LINK_ENTITY_TO_CLASS( info_adv_vehicle_weapon, CVehicleSlot );
LINK_ENTITY_TO_CLASS( info_adv_vehicle_attachement, CVehicleSlot );

PlayerSlot::PlayerSlot()
	: m_pUser( NULL ), m_pData( new Vector( 0, 0, 0 ) ), m_pNext( NULL ),
	  m_flEnterTime( 0 ), m_flExitTime( 0 ), m_pTank( NULL ), m_iControls( 0 ), m_iIndex( 0 )
{
}

TireSlot::TireSlot()
	: m_pEntity( NULL ), m_pData( new Vector( 0, 0, 0 ) ), m_pNext( NULL ),
	  m_Unresolved0C( 0 ), m_Unresolved10( 0 )
{
}

AnythingSlot::AnythingSlot()
	: m_pEntities( NULL ), m_pData( new Vector( 0, 0, 0 ) ), m_pNext( NULL ),
	  m_iFlags( 0 ), m_Unresolved10( 0 ), m_Unresolved14( 0 )
{
}

CAdvFuncVehicle::CAdvFuncVehicle()
	: m_Unresolved19C( 0, 0, 0 ), m_Unresolved1A8( 0, 0, 0 )
{
}

TYPEDESCRIPTION CAdvFuncVehicle::m_SaveData[] =
{
	DEFINE_FIELD( CAdvFuncVehicle, m_flMass, FIELD_FLOAT ),
	DEFINE_FIELD( CAdvFuncVehicle, m_flPower, FIELD_FLOAT ),
	DEFINE_FIELD( CAdvFuncVehicle, m_flBreakPower, FIELD_FLOAT ),
	DEFINE_FIELD( CAdvFuncVehicle, m_flForwardSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CAdvFuncVehicle, m_flBackwardSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CAdvFuncVehicle, m_flSideSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CAdvFuncVehicle, m_flUpSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CAdvFuncVehicle, m_flDownSpeed, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CAdvFuncVehicle, CBaseEntity );

void CAdvFuncVehicle::Spawn( void )
{
	pev->frags = 0;
	m_pTires = NULL;
	m_pPlayerSlots = NULL;
	m_pAttachments = NULL;
	m_flArmor = (float) (int) (float) ( m_flArmor / 10.0 );
	m_iButtons = 0;
	m_Unresolved19C = g_vecZero;
	m_Unresolved1A8 = g_vecZero;
	pev->takedamage = DAMAGE_YES;
	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSHSTEP;
	pev->origin.z += 1;
	UTIL_SetOrigin( pev, pev->origin );
	SetModel_Intercept( edict(), STRING( pev->model ) );
	m_bSpawned = false;
	SetThink( &CAdvFuncVehicle::RealSpawn );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CAdvFuncVehicle::Precache( void )
{
	CBaseEntity::Precache();
}

void CAdvFuncVehicle::KeyValue( KeyValueData *key )
{
	if ( FStrEq( key->szKeyName, "m_flMass" ) )
		m_flMass = atof( key->szValue );
	else if ( FStrEq( key->szKeyName, "m_flPower" ) )
		m_flPower = atof( key->szValue );
	else if ( FStrEq( key->szKeyName, "m_flBreakPower" ) )
		m_flBreakPower = atof( key->szValue );
	else if ( FStrEq( key->szKeyName, "m_flArmor" ) )
		m_flArmor = atof( key->szValue );
	else if ( FStrEq( key->szKeyName, "m_flForwardSpeed" ) )
		m_flForwardSpeed = atof( key->szValue );
	else if ( FStrEq( key->szKeyName, "m_flBackwardSpeed" ) )
		m_flBackwardSpeed = atof( key->szValue );
	else if ( FStrEq( key->szKeyName, "m_flSideSpeed" ) )
		m_flSideSpeed = atof( key->szValue );
	else if ( FStrEq( key->szKeyName, "m_flUpSpeed" ) )
		m_flUpSpeed = atof( key->szValue );
	else if ( FStrEq( key->szKeyName, "m_flDownSpeed" ) )
		m_flDownSpeed = atof( key->szValue );
	else if ( FStrEq( key->szKeyName, "m_flRotSpeed" ) )
		m_flRotSpeed = atof( key->szValue );
	else if ( FStrEq( key->szKeyName, "m_flFloorHeight" ) )
		m_flFloorHeight = atof( key->szValue );
	else if ( FStrEq( key->szKeyName, "m_flFloatHeight" ) )
		m_flFloatHeight = atof( key->szValue );
	else if ( FStrEq( key->szKeyName, "m_flHealth" ) )
		m_flHealth = atof( key->szValue );
	else
	{
		CBaseEntity::KeyValue( key );
		return;
	}

	key->fHandled = TRUE;
}

void CAdvFuncVehicle::Touch( CBaseEntity *other )
{
	ALERT( at_console, "TOUCH: %s\n", STRING( other->pev->classname ) );

	if ( pev->velocity.Length() > 100 )
		Blocked( other );
}

void CAdvFuncVehicle::TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType )
{
	TakeDamage( NULL, attacker, damage, damageType );
}

bool CAdvFuncVehicle::TakeHealth( float health, int damageType, int healthCap )
{
	if ( m_flArmor < 0 )
		health *= -m_flArmor;
	else if ( !( m_flArmor <= 0 ) )
		health /= m_flArmor;

	return CBaseEntity::TakeHealth( health, damageType, healthCap );
}

bool CAdvFuncVehicle::TakeArmor( float armor, int damageType, int armorCap )
{
	return CBaseEntity::TakeArmor( armor, damageType, armorCap );
}

bool CAdvFuncVehicle::CanRepairVehicle( CBaseEntity *entity )
{
	return !( pev->spawnflags & SF_ADVVEHICLE_NO_REPAIR ) && VehicleHasRepairWeapon( entity );
}

int CAdvFuncVehicle::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	ALERT( at_console, "TAKEDAMAGE\n" );

	if ( m_flArmor < 0 )
		damage *= -m_flArmor;
	else if ( !( m_flArmor <= 0 ) )
		damage /= m_flArmor;

	if ( ( pev->spawnflags & SF_ADVVEHICLE_EXPLOSIVES_ONLY ) && !( damageType & ( DMG_BLAST | DMG_BURN ) ) )
		return 0;

	if ( !( damage >= 0 ) )
	{
		edict_t *entity = attacker ? attacker->pContainingEntity : NULL;

		if ( !entity )
			entity = INDEXENT( 0 );

		CBaseEntity *repairer = entity && !entity->free ? (CBaseEntity *) GET_PRIVATE( entity ) : NULL;

		if ( !CanRepairVehicle( repairer ) )
			return 0;
	}

	pev->health -= damage;
	return 1;
}

void CAdvFuncVehicle::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( activator && activator->IsPlayer() && useType == USE_TOGGLE )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( activator );

		if ( !VehicleHandleEntity( player->m_hVehicle.m_Handle ) )
			PlayerEnter( player );
	}
}

void CAdvFuncVehicle::RealSpawn( void )
{
	if ( !m_bSpawned )
	{
		m_bSpawned = true;
		SetThink( &CAdvFuncVehicle::RealSpawn );
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	if ( !m_pPlayerSlots )
	{
		m_pPlayerSlots = new PlayerSlot;
		m_pPlayerSlots->m_iControls = 15;
		m_pPlayerSlots->m_pData = new Vector( 0, 0, 100 );
		m_pPlayerSlots->m_pNext = NULL;
	}

	edict_t *entity = CREATE_NAMED_ENTITY( AllocStringLocal( "adv_vehicle_thinker" ) );

	if ( !VehicleEdictValid( entity ) )
	{
		UTIL_Remove( this );
		return;
	}

	CAdvVehicleThinker *thinker = static_cast<CAdvVehicleThinker *>( VehicleEdictEntity( entity ) );

	thinker->m_pVehicle = edict();
	thinker->ThinkerThink();
	SetThink( NULL );
}

void CAdvFuncVehicle::VehicleThink( void )
{
	PlaceAll();
	ControlMovement();
	ALERT( at_notice, "flForwardSpeed: %f\n", m_flCurrentForwardSpeed );

	Vector forward;
	Vector right;
	Vector up;

	UTIL_MakeVectorsPrivate( pev->angles, forward, right, up );
	pev->velocity = forward.Normalize() * m_flCurrentForwardSpeed;
	pev->avelocity = Vector( 0, m_flCurrentTurnSpeed, 0 );
	ALERT( at_notice, "velocity: %f %f %f\n", pev->velocity.x, pev->velocity.y, pev->velocity.z );
	pev->flags |= FL_ALWAYSTHINK;
	pev->nextthink = gpGlobals->time;
}

void CAdvVehicleThinker::ThinkerThink( void )
{
	if ( !VehicleEdictValid( m_pVehicle ) )
	{
		SetThink( NULL );
		pev->flags &= ~FL_ALWAYSTHINK;
		UTIL_Remove( this );
		return;
	}

	CAdvFuncVehicle *vehicle = static_cast<CAdvFuncVehicle *>( VehicleEdictEntity( m_pVehicle ) );

	vehicle->VehicleThink();
	SetThink( &CAdvVehicleThinker::ThinkerThink );
	pev->nextthink = gpGlobals->time;
	pev->flags |= FL_ALWAYSTHINK;
}

void CVehicleSlot::Spawn( void )
{
	SetThink( NULL );
	pev->nextthink = gpGlobals->time;
}

void CVehicleSlot::Think( void )
{
	CBaseEntity *entity = NULL;

	while ( ( entity = UTIL_FindEntityByTargetname( entity, STRING( pev->target ) ) ) != NULL )
	{
		if ( FClassnameIs( entity->pev, "func_adv_vehicle" ) )
		{
			static_cast<CAdvFuncVehicle *>( entity )->RegisterSlot( pev );
			break;
		}
	}

	SetThink( NULL );
	UTIL_Remove( this );
}

void CAdvFuncVehicle::RegisterSlot( entvars_t *slot )
{
	if ( !slot || !VehicleEdictValid( slot->pContainingEntity ) )
		return;

	const char *classname = STRING( slot->classname );

	// These stack offsets are stored in the persistent slot nodes
	if ( FStrEq( classname, "info_adv_vehicle_control" ) )
	{
		PlayerSlot *position = new PlayerSlot;
		Vector relative = slot->origin - pev->origin;

		position->m_iControls = slot->spawnflags;
		position->m_pUser = NULL;
		position->m_pData = &relative;
		position->m_iIndex = (int) slot->health;
		AddPlayerSlot( position );
		return;
	}

	if ( FStrEq( classname, "info_adv_vehicle_seat" ) )
	{
		PlayerSlot *position = new PlayerSlot;
		Vector relative = slot->origin - pev->origin;

		position->m_pUser = NULL;
		position->m_pData = &relative;
		position->m_iIndex = (int) slot->health;
		AddPlayerSlot( position );
		return;
	}

	if ( FStrEq( classname, "info_adv_vehicle_weapon" ) )
	{
		PlayerSlot *position = new PlayerSlot;
		Vector relative = slot->origin - pev->origin;

		position->m_pTank = NULL;
		position->m_pTank = FIND_ENTITY_BY_STRING( NULL, "targetname", STRING( slot->target ) );

		while ( position->m_pTank && !FClassnameIs( &position->m_pTank->v, "func_tank" ) )
			position->m_pTank = FIND_ENTITY_BY_STRING( position->m_pTank, "targetname", STRING( slot->target ) );

		if ( !VehicleEdictValid( position->m_pTank ) )
		{
			delete position;
			return;
		}

		const char *tankClass = STRING( position->m_pTank->v.classname );

		if ( !FStrEq( tankClass, "func_tank" ) && !FStrEq( tankClass, "func_tanklaser" )
		     && !FStrEq( tankClass, "func_tankrocket" ) && !FStrEq( tankClass, "func_tankmortar" ) )
		{
			delete position;
			return;
		}

		position->m_pUser = NULL;
		position->m_pData = &relative;
		position->m_pTank->v.solid = SOLID_NOT;

		CFuncTank *tank = static_cast<CFuncTank *>( VehicleEdictEntity( position->m_pTank ) );

		tank->m_hUseProxy = this;
		PlaceTank( position );
		position->m_iIndex = (int) slot->health;
		AddPlayerSlot( position );
		return;
	}

	if ( FStrEq( classname, "info_adv_vehicle_tire" ) )
	{
		TireSlot *position = new TireSlot;
		Vector relative = slot->origin - pev->origin;

		position->m_pEntity = NULL;
		position->m_pEntity = FIND_ENTITY_BY_STRING( NULL, "targetname", STRING( slot->target ) );

		while ( position->m_pEntity && !position->m_pEntity->v.model )
			position->m_pEntity = FIND_ENTITY_BY_STRING( position->m_pEntity, "targetname", STRING( slot->target ) );

		if ( !VehicleEdictValid( position->m_pEntity ) || position->m_pEntity->v.model )
		{
			delete position;
			return;
		}

		position->m_pData = &relative;
		PlaceTire( position );
		position->m_pNext = m_pTires;
		m_pTires = position;
		return;
	}

	if ( FStrEq( classname, "info_adv_vehicle_attachement" ) )
	{
		AnythingSlot *position = new AnythingSlot;
		Vector relative = slot->origin - pev->origin;
		edict_t *entity = NULL;

		position->m_pEntities = NULL;

		while ( ( entity = FIND_ENTITY_BY_STRING( entity, "targetname", STRING( slot->target ) ) ) != NULL )
		{
			VehicleAttachmentEntity *attachment = new VehicleAttachmentEntity;
			CBaseEntity *attached = !entity->free ? (CBaseEntity *) GET_PRIVATE( entity ) : NULL;

			attachment->m_hEntity = attached;
			attachment->m_pNext = position->m_pEntities;
			position->m_pEntities = attachment;
		}

		if ( !position->m_pEntities )
		{
			delete position;
			return;
		}

		position->m_iFlags = slot->spawnflags;
		position->m_pData = &relative;
		PlaceAttachements( position );
		position->m_pNext = m_pAttachments;
		m_pAttachments = position;
	}
}

void CAdvFuncVehicle::MakeRelativeWorld( Vector *world, Vector *relative )
{
	*world = pev->origin + *relative;
	UTIL_RotateVector( world, pev->angles, pev->origin, false );
}

bool CAdvFuncVehicle::PlayerNearSlot( Vector *position, Vector *relative )
{
	Vector world = pev->origin + *relative;

	UTIL_RotateVector( &world, pev->angles, pev->origin, false );
	return ( *position - world ).Length() < 512;
}

void CAdvFuncVehicle::PlaceAttachements( AnythingSlot *slot )
{
	for ( VehicleAttachmentEntity *entry = slot->m_pEntities; entry; entry = entry->m_pNext )
	{
		CBaseEntity *entity = VehicleHandleEntity( entry->m_hEntity );

		if ( !entity )
			continue;

		entity->pev->velocity = pev->velocity;
		entity->pev->origin = pev->origin + *slot->m_pData;
		UTIL_RotateVector( &entity->pev->origin, pev->angles, pev->origin, false );

		if ( slot->m_iFlags & 1 )
		{
			entity->pev->avelocity = pev->avelocity;
			entity->pev->angles = pev->angles;
		}
	}
}

void CAdvFuncVehicle::PlaceTire( TireSlot *slot )
{
	if ( VehicleEdictValid( slot->m_pEntity ) )
	{
		slot->m_pEntity->v.velocity = pev->velocity;
		slot->m_pEntity->v.origin = pev->origin + *slot->m_pData;
		UTIL_RotateVector( &slot->m_pEntity->v.origin, pev->angles, pev->origin, false );
	}
}

void CAdvFuncVehicle::PlaceTank( PlayerSlot *slot )
{
	if ( VehicleEdictValid( slot->m_pTank ) )
	{
		slot->m_pTank->v.velocity = pev->velocity;
		slot->m_pTank->v.origin = pev->origin + *slot->m_pData;
		UTIL_RotateVector( &slot->m_pTank->v.origin, pev->angles, pev->origin, false );
	}
	else
		slot->m_pTank = NULL;
}

void CAdvFuncVehicle::PlaceUser( PlayerSlot *slot )
{
	if ( VehicleEdictValid( slot->m_pUser ) )
	{
		slot->m_pUser->v.origin = pev->origin + *slot->m_pData;
		UTIL_RotateVector( &slot->m_pUser->v.origin, pev->angles, pev->origin, false );
	}
	else
		slot->m_pUser = NULL;
}

void CAdvFuncVehicle::PlaceAll( void )
{
	for ( PlayerSlot *slot = m_pPlayerSlots; slot; slot = slot->m_pNext )
	{
		PlaceUser( slot );
		PlaceTank( slot );
	}

	for ( TireSlot *slot = m_pTires; slot; slot = slot->m_pNext )
		PlaceTire( slot );

	for ( AnythingSlot *slot = m_pAttachments; slot; slot = slot->m_pNext )
		PlaceAttachements( slot );
}

void CAdvFuncVehicle::AddPlayerSlot( PlayerSlot *slot )
{
	if ( !slot )
		return;

	PlayerSlot *previous = NULL;
	PlayerSlot *next = m_pPlayerSlots;

	while ( next && next->m_iIndex < slot->m_iIndex )
	{
		previous = next;
		next = next->m_pNext;
	}

	slot->m_pNext = next;

	if ( previous )
		previous->m_pNext = slot;
	else
		m_pPlayerSlots = slot;
}

bool CAdvFuncVehicle::CanExit( PlayerSlot *slot )
{
	return gpGlobals->time - slot->m_flEnterTime > 0.2;
}

bool CAdvFuncVehicle::CanUse( void *data )
{
	PlayerSlot *slot = static_cast<PlayerSlot *>( data );

	return slot && ( VehicleEdictValid( slot->m_pTank ) || slot->m_iControls != 0 );
}

PlayerSlot *CAdvFuncVehicle::FindFreeSlot( CBasePlayer *player )
{
	if ( !player )
		return NULL;

	PlayerSlot *slot = m_pPlayerSlots;

	if ( !slot )
		return NULL;

	while ( VehicleEdictValid( slot->m_pUser ) )
	{
		slot = m_pPlayerSlots->m_pNext;

		if ( !slot )
			return NULL;
	}

	slot->m_flEnterTime = gpGlobals->time;
	slot->m_pUser = player->pev ? ENT( player->pev ) : NULL;
	return slot;
}

void CAdvFuncVehicle::PlayerEnter( CBasePlayer *player )
{
	if ( !player || VehicleHandleEntity( player->m_hVehicle.m_Handle ) || VehicleHandleEntity( player->m_hTank.m_Handle ) )
		return;

	PlayerSlot *slot = FindFreeSlot( player );

	if ( !slot )
		return;

	player->SetSuitUpdate( NULL, FALSE, 0 );
	player->BlockWeapons( this );
	player->pev->effects = EF_NODRAW;
	player->pev->angles = pev->angles;
	player->pev->fixangle = 1;
	player->pev->solid = SOLID_NOT;
	player->pev->takedamage = DAMAGE_NO;
	player->pev->movetype = MOVETYPE_NOCLIP;
	player->pev->flags |= FL_GODMODE | FL_NOTARGET;
	player->m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
	player->pev->flags &= ~FL_DUCKING;
	slot->m_flEnterTime = gpGlobals->time;
	slot->m_pUser = player->pev ? ENT( player->pev ) : NULL;
	player->m_pVehicleSlot = slot;
	player->m_hVehicle = this;
	PlaceUser( slot );
	SendUserSlotMessage( slot );
}

void CAdvFuncVehicle::PlayerExit( CBasePlayer *player, bool force )
{
	if ( !player || !player->m_pVehicleSlot )
		return;

	PlayerSlot *slot = player->m_pVehicleSlot;

	if ( !force && !CanExit( slot ) )
		return;

	slot->m_flExitTime = gpGlobals->time;
	slot->m_pUser = NULL;
	player->m_hVehicle = (CBaseEntity *) NULL;
	player->m_pVehicleSlot = NULL;
	player->pev->origin = pev->origin + Vector( 0, 0, 128 );
	player->pev->solid = SOLID_SLIDEBOX;
	player->pev->takedamage = DAMAGE_YES;
	player->pev->movetype = MOVETYPE_WALK;
	player->pev->flags &= ~( FL_GODMODE | FL_NOTARGET );
	player->pev->effects &= ~EF_NODRAW;
	player->UnblockWeapons( this );

	if ( player->pev )
		SendUserExitMessage( ENT( player->pev ) );
}

void CAdvFuncVehicle::PlayerSwitchSlot( CBasePlayer *player )
{
	if ( !player || !player->m_pVehicleSlot )
		return;

	ALERT( at_console, "SWITCHING SLOTS!\n" );

	PlayerSlot *previous = player->m_pVehicleSlot;
	PlayerSlot *slot = previous;
	edict_t *entity = player->pev ? ENT( player->pev ) : NULL;

	for ( ;; )
	{
		slot = slot->m_pNext;

		if ( !slot )
			slot = m_pPlayerSlots;

		if ( slot == previous || !slot )
			break;

		if ( !VehicleEdictValid( slot->m_pUser ) )
		{
			if ( !VehicleEdictValid( slot->m_pUser ) )
			{
				previous->m_flExitTime = gpGlobals->time;
				previous->m_pUser = NULL;
				slot->m_flEnterTime = gpGlobals->time;
				slot->m_pUser = entity;
				player->m_pVehicleSlot = slot;
				PlaceUser( slot );
				SendUserSlotMessage( slot );
			}

			return;
		}
	}
}

void CAdvFuncVehicle::SendUserExitMessage( edict_t *player )
{
	if ( player && !player->free )
		OFFSET( player );
}

void CAdvFuncVehicle::SendUserSlotMessage( PlayerSlot *slot )
{
	if ( !VehicleEdictValid( slot->m_pUser ) )
		return;

	char *text = new char[128];
	const char *format;

	if ( VehicleEdictValid( slot->m_pTank ) )
		format = "In Vehicle at position %i, using a gun.";
	else if ( slot->m_iControls )
		format = "In Vehicle at position %i, at the wheel.";
	else
		format = "In Vehicle at position %i, sitting around.";

	snprintf( text, 128, format, slot->m_iIndex );

	hudtextparms_t params = {};

	params.x = 0.4f;
	params.y = 0.7f;
	params.r1 = 5;
	params.g1 = 200;
	params.b1 = 10;
	params.r2 = 5;
	params.g2 = 200;
	params.b2 = 10;
	params.a2 = 255;
	params.fadeinTime = 0.075f;
	params.fadeoutTime = 0.075f;
	params.holdTime = 2;
	UTIL_HudMessage( VehicleEdictEntity( slot->m_pUser ), params, text );
	ClientPrint( &slot->m_pUser->v, HUD_PRINTCONSOLE, "%s\n", text );
}

void CAdvFuncVehicle::PlayerUse( CBasePlayer *player, int useType, int buttons )
{
	ALERT( at_console, "PlayerUse1!\n" );

	if ( !player )
		return;

	ALERT( at_console, "PlayerUse2!\n" );

	if ( VehicleHandleEntity( player->m_hVehicle.m_Handle ) != this )
		return;

	ALERT( at_console, "PlayerUse3!\n" );

	PlayerSlot *slot = player->m_pVehicleSlot;

	if ( !CanUse( slot ) )
		return;

	ALERT( at_console, "PlayerUse4!\n" );

	if ( VehicleEdictValid( slot->m_pTank ) )
	{
		if ( buttons & IN_ATTACK )
		{
			ALERT( at_console, "IN_ATTACK!\n" );

			// The firing path treats the stored edict as a tank object
			CFuncTank *tank = reinterpret_cast<CFuncTank *>( slot->m_pTank );

			tank->m_hController = player;
			tank->Fire();
			tank->m_hController = (CBaseEntity *) NULL;
		}

		return;
	}

	if ( slot->m_iControls & 1 )
	{
		if ( buttons & IN_MOVELEFT )
		{
			if ( useType == USE_OFF )
			{
				m_iButtons &= ~IN_MOVELEFT;
				ALERT( at_console, "IN_MOVELEFT USE_OFF: %i!\n", m_iButtons );
			}
			else if ( useType == USE_ON )
			{
				m_iButtons |= IN_MOVELEFT;
				ALERT( at_console, "IN_MOVELEFT USE_ON: %i!\n", m_iButtons );
			}
		}

		if ( ( slot->m_iControls & 1 ) && ( buttons & IN_MOVERIGHT ) )
		{
			if ( useType == USE_OFF )
			{
				m_iButtons &= ~IN_MOVERIGHT;
				ALERT( at_console, "IN_MOVERIGHT USE_OFF: %i!\n", m_iButtons );
			}
			else if ( useType == USE_ON )
			{
				m_iButtons |= IN_MOVERIGHT;
				ALERT( at_console, "IN_MOVERIGHT USE_ON: %i!\n", m_iButtons );
			}
		}
	}

	if ( ( slot->m_iControls & 2 ) && ( buttons & IN_FORWARD ) )
	{
		if ( useType == USE_OFF )
		{
			m_iButtons &= ~IN_FORWARD;
			ALERT( at_console, "IN_FORWARD USE_OFF: %i!\n", m_iButtons );
		}
		else if ( useType == USE_ON )
		{
			m_iButtons |= IN_FORWARD;
			ALERT( at_console, "IN_FORWARD USE_ON: %i!\n", m_iButtons );
		}
	}

	if ( ( slot->m_iControls & 4 ) && ( buttons & IN_BACK ) )
	{
		if ( useType == USE_OFF )
		{
			ALERT( at_console, "IN_BACK USE_OFF!\n" );
			m_iButtons &= ~IN_BACK;
		}
		else if ( useType == USE_ON )
		{
			ALERT( at_console, "IN_BACK USE_ON!\n" );
			m_iButtons |= IN_BACK;
		}
	}

	if ( slot->m_iControls & 8 )
	{
		if ( buttons & IN_JUMP )
		{
			if ( useType == USE_OFF )
			{
				ALERT( at_console, "IN_JUMP USE_OFF!\n" );
				m_iButtons &= ~IN_JUMP;
			}
			else if ( useType == USE_ON )
			{
				ALERT( at_console, "IN_JUMP USE_ON!\n" );
				m_iButtons |= IN_JUMP;
			}
		}

		if ( ( slot->m_iControls & 8 ) && ( buttons & IN_DUCK ) )
		{
			if ( useType == USE_OFF )
			{
				ALERT( at_console, "IN_DUCK USE_OFF!\n" );
				m_iButtons &= ~IN_DUCK;
			}
			else if ( useType == USE_ON )
			{
				ALERT( at_console, "IN_DUCK USE_ON!\n" );
				m_iButtons |= IN_DUCK;
			}
		}
	}
}

float CAdvFuncVehicle::GetAcceleration( void )
{
	return 100.0f / m_flMass * m_flPower;
}

float CAdvFuncVehicle::GetDeceleration( bool braking )
{
	float value = -100.0f / m_flMass;

	if ( braking )
		value *= m_flBreakPower;

	return value;
}

float CAdvFuncVehicle::GetTurnSpeed( bool alternate )
{
	float value = m_flRotSpeed * m_flCurrentForwardSpeed;

	if ( alternate )
		return RANDOM_FLOAT( -1, 1 ) + value * 10;

	return value;
}

void CAdvFuncVehicle::DirectSteer( void )
{
	bool alternate = ( m_iButtons & IN_JUMP ) || ( pev->spawnflags & SF_ADVVEHICLE_ALWAYS_DRIFT );

	if ( m_iButtons & IN_MOVELEFT )
	{
		float value = m_flCurrentForwardSpeed * m_flRotSpeed;

		if ( alternate )
			value = RANDOM_FLOAT( -1, 1 ) + value * 10;

		float turn = m_flRotSpeed;

		if ( turn <= value )
			turn = GetTurnSpeed( alternate );

		m_flCurrentTurnSpeed = turn;
	}
	else if ( m_iButtons & IN_MOVERIGHT )
		m_flCurrentTurnSpeed = -fmaxf( m_flRotSpeed, GetTurnSpeed( alternate ) );
}

void CAdvFuncVehicle::ControlSteer( void )
{
	CBasePlayer *player = NULL;

	while ( ( player = GetDriver( player ) ) != NULL )
	{
		if ( player->m_pVehicleSlot->m_iControls & 1 )
		{
			m_flCurrentTurnSpeed = player->pev->avelocity.y * 0.1;
			player->pev->angles.y = pev->angles.y;
			player->pev->angles.x = player->pev->v_angle.x;
			player->pev->fixangle = 1;
			return;
		}
	}
}

void CAdvFuncVehicle::ControlMovement( void )
{
	bool braking = ( m_iButtons & ( IN_BACK | IN_JUMP ) ) != 0;

	if ( braking )
		m_flCurrentForwardSpeed += GetDeceleration( true );
	else if ( m_iButtons & IN_FORWARD )
		m_flCurrentForwardSpeed += GetAcceleration();
	else
		m_flCurrentForwardSpeed += GetDeceleration( false );

	if ( m_flCurrentForwardSpeed <= m_flForwardSpeed )
	{
		if ( -m_flBackwardSpeed > m_flCurrentForwardSpeed )
			m_flCurrentForwardSpeed = -m_flBackwardSpeed;
	}
	else
		m_flCurrentForwardSpeed = m_flForwardSpeed;

	if ( !( pev->spawnflags & SF_ADVVEHICLE_SLIDE_STEER ) )
	{
		DirectSteer();
		return;
	}

	if ( braking )
		m_flCurrentSideSpeed += GetDeceleration( true );
	else if ( m_iButtons & IN_MOVELEFT )
		m_flCurrentSideSpeed += GetAcceleration();
	else if ( m_iButtons & IN_MOVERIGHT )
		m_flCurrentSideSpeed -= GetAcceleration();
	else
		m_flCurrentSideSpeed += GetDeceleration( false );

	if ( m_flCurrentSideSpeed > m_flSideSpeed )
		m_flCurrentSideSpeed = m_flSideSpeed;
	else if ( -m_flSideSpeed > m_flCurrentSideSpeed )
		m_flCurrentSideSpeed = -m_flSideSpeed;

	ControlSteer();
}

CBasePlayer *CAdvFuncVehicle::GetDriver( CBasePlayer *player )
{
	PlayerSlot *slot = player ? player->m_pVehicleSlot : m_pPlayerSlots;

	if ( !slot || !slot->m_pNext )
		return NULL;

	do
	{
		slot = slot->m_pNext;

		if ( VehicleEdictValid( slot->m_pUser ) )
			return static_cast<CBasePlayer *>( VehicleEdictEntity( slot->m_pUser ) );
	}
	while ( slot->m_pNext );

	return NULL;
}

void CAdvFuncVehicle::Blocked( CBaseEntity *other )
{
	ALERT( at_console, "BLOCKED!\n" );

	if ( !( pev->velocity.Length() >= 100 ) )
		return;

	if ( other->IsPlayer() && ( other->pev->flags & FL_ONGROUND ) && other->pev->groundentity
	     && VARS( other->pev->groundentity ) == pev )
	{
		Vector direction = pev->velocity.Normalize();
		Vector angles = pev->velocity.Normalize() * 90;
		Vector offset;
		Vector right;
		Vector up;

		UTIL_MakeVectorsPrivate( angles, offset, right, up );
		UTIL_SetOrigin( other->pev, other->pev->origin + direction + offset );
		other->pev->velocity = pev->velocity;
		return;
	}

	if ( FClassnameIs( other->pev, "weaponbox" ) || FClassnameIs( other->pev, "monster_satchel" )
	     || FClassnameIs( other->pev, "monster_tripmine" ) )
	{
		other->CriticalRemove();
		return;
	}

	float damage = pev->velocity.Length() * m_flMass;

	if ( FClassnameIs( other->pev, "worldspawn" ) )
	{
		TakeDamage( NULL, NULL, damage, DMG_CRUSH );
		return;
	}

	other->TakeDamage( pev, pev, damage, DMG_CRUSH );

	if ( !( other->pev->health <= 0 ) )
		return;

	CBasePlayer *player = NULL;

	if ( other->IsPlayer() )
	{
		while ( ( player = GetDriver( player ) ) != NULL )
			player->AddPoints( -10, true );
	}
	else if ( other->Classify() == CLASS_PLAYER || other->Classify() == CLASS_PLAYER_ALLY
	          || other->Classify() == CLASS_HUMAN_PASSIVE )
	{
		pev->frags -= 1;

		while ( ( player = GetDriver( player ) ) != NULL )
			player->AddPoints( -1, true );

		if ( pev->frags < 0 && (int) pev->frags % 10 == 0 )
			pev->health -= pev->frags;
	}
	else if ( other->Classify() != CLASS_NONE )
	{
		pev->frags += 1;

		while ( ( player = GetDriver( player ) ) != NULL )
			player->AddPoints( 1, false );
	}
}
