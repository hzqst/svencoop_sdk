#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "effects.h"
#include "customentity.h"
#include "hgrunt_repel.h"

void HumanGruntRepelPrecache( CBaseMonster *owner, const char *className,
                              const char *model, bool invertAlly, int &sprite )
{
	UTIL_PrecacheMonster( className, invertAlly ? !owner->IsPlayerAlly() : owner->IsPlayerAlly() );
	owner->CBaseEntity::Precache();
	UTIL_PrecacheModel( owner, FStringNull( owner->pev->model ) ? model : STRING( owner->pev->model ) );
	sprite = UTIL_PrecacheModel( owner, "sprites/rope.spr" );
}

void HumanGruntRepelUse( CBaseMonster *owner, const char *className, int head )
{
	TraceResult tr;

	UTIL_TraceLine( owner->pev->origin, owner->pev->origin + Vector( 0, 0, -4096 ),
	                dont_ignore_monsters, owner->edict(), &tr );
	CBaseEntity *entity = CBaseEntity::Create( className, owner->pev->origin, owner->pev->angles, true );

	if ( !entity )
		return;

	CBaseMonster *monster = entity->MyMonsterPointer();

	if ( !monster )
	{
		UTIL_Remove( entity );
		return;
	}

	monster->pev->model = owner->pev->model;
	monster->pev->weapons = owner->pev->weapons;
	monster->pev->netname = owner->pev->netname;
	monster->pev->spawnflags = owner->pev->spawnflags;
	monster->pev->health = owner->pev->health;
	monster->m_iClassSelection = owner->m_iClassSelection;
	monster->m_fOverrideClass = owner->m_fOverrideClass;
	monster->SetPlayerAllyDirect( owner->IsPlayerAlly() );

	if ( owner->m_FormattedName )
		monster->m_FormattedName = owner->m_FormattedName;

	monster->m_fCustomModel = true;

	if ( head )
	{
		char value[32];

		sprintf( value, "%d", head - 25 );
		KeyValueData keyValue = { STRING( monster->pev->classname ), "head", value, FALSE };

		monster->KeyValue( &keyValue );
	}

	DispatchSpawn( monster->edict() );

	if ( owner->pev->owner )
	{
		monster->pev->owner = owner->pev->owner;
		owner->pev->owner = NULL;
	}

	monster->pev->movetype = MOVETYPE_FLY;
	monster->pev->velocity = Vector( 0, 0, RANDOM_FLOAT( -196, -128 ) );
	monster->SetActivity( ACT_GLIDE );
	monster->m_vecLastPosition = tr.vecEndPos;
	monster->m_iszTriggerTarget = owner->m_iszTriggerTarget;
	monster->m_iTriggerCondition = owner->m_iTriggerCondition;
	monster->m_fFreeRoamMode2 = owner->m_fFreeRoamMode2;
	monster->m_fFreeRoamMode1 = owner->m_fFreeRoamMode1;

	CBeam *beam = CBeam::BeamCreate( "sprites/rope.spr", 10 );

	if ( beam )
	{
		beam->PointEntInit( owner->pev->origin + Vector( 0, 0, 112 ), monster->entindex() );
		beam->SetFlags( BEAM_FSOLID );
		beam->SetColor( 255, 255, 255 );
		beam->SetThink( &CBeam::SUB_Remove );
		beam->pev->nextthink = gpGlobals->time - 4096 * tr.flFraction / monster->pev->velocity.z + 0.5f;
	}

	UTIL_Remove( owner );
}

extern cvar_t sk_hgrunt_health;

class CHGruntOp4Repel : public CBaseMonster
{
public:
	float GetPointsForDamage( float damage )
	{
		//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
		return pev->max_health / sk_hgrunt_health.value * 5 * ( damage / pev->max_health );
	}

	void Spawn( void );
	void Precache( void );
	void EXPORT RepelUse( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value );
	void SetPlayerAlly( bool state ) { SetPlayerAllyDirect( !state ); }
	void KeyValue( KeyValueData *pkvd );

	int m_iSpriteTexture;
	int m_iHead;
};

LINK_ENTITY_TO_CLASS( monster_grunt_ally_repel, CHGruntOp4Repel );

void CHGruntOp4Repel::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;

	if ( FStringNull( pev->targetname ) )
		RepelUse( NULL, NULL, USE_TOGGLE, 0 );
	else
		SetUse( &CHGruntOp4Repel::RepelUse );
}

void CHGruntOp4Repel::Precache( void )
{
	HumanGruntRepelPrecache( this, "monster_human_grunt_ally",
	                         IsPlayerAlly() ? "models/hgrunt_opforf.mdl" : "models/hgrunt_opfor.mdl", true, m_iSpriteTexture );
}

void CHGruntOp4Repel::RepelUse( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	HumanGruntRepelUse( this, "monster_human_grunt_ally", m_iHead );
}

void CHGruntOp4Repel::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "head" ) )
	{
		m_iHead = atoi( pkvd->szValue ) + 25;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue( pkvd );
}

class CHGruntOp4MedicRepel : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT RepelUse( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value );
	void SetPlayerAlly( bool state ) { SetPlayerAllyDirect( !state ); }
	void KeyValue( KeyValueData *pkvd );

	int m_iSpriteTexture;
	int m_iHead;
};

LINK_ENTITY_TO_CLASS( monster_medic_ally_repel, CHGruntOp4MedicRepel );

void CHGruntOp4MedicRepel::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;

	if ( FStringNull( pev->targetname ) )
		RepelUse( NULL, NULL, USE_TOGGLE, 0 );
	else
		SetUse( &CHGruntOp4MedicRepel::RepelUse );
}

void CHGruntOp4MedicRepel::Precache( void )
{
	HumanGruntRepelPrecache( this, "monster_human_medic_ally",
	                         IsPlayerAlly() ? "models/hgrunt_medicf.mdl" : "models/hgrunt_medic.mdl", true, m_iSpriteTexture );
}

void CHGruntOp4MedicRepel::RepelUse( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	HumanGruntRepelUse( this, "monster_human_medic_ally", m_iHead );
}

void CHGruntOp4MedicRepel::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "head" ) )
	{
		m_iHead = atoi( pkvd->szValue ) + 25;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue( pkvd );
}

class CHGruntOp4TorchRepel : public CBaseMonster
{
public:
	float GetPointsForDamage( float damage )
	{
		//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
		return pev->max_health / sk_hgrunt_health.value * 5 * ( damage / pev->max_health );
	}

	void Spawn( void );
	void Precache( void );
	void EXPORT RepelUse( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value );
	void SetPlayerAlly( bool state ) { SetPlayerAllyDirect( !state ); }

	int m_iSpriteTexture;
};

LINK_ENTITY_TO_CLASS( monster_torch_ally_repel, CHGruntOp4TorchRepel );

void CHGruntOp4TorchRepel::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;

	if ( FStringNull( pev->targetname ) )
		RepelUse( NULL, NULL, USE_TOGGLE, 0 );
	else
		SetUse( &CHGruntOp4TorchRepel::RepelUse );
}

void CHGruntOp4TorchRepel::Precache( void )
{
	HumanGruntRepelPrecache( this, "monster_human_torch_ally",
	                         IsPlayerAlly() ? "models/hgrunt_torchf.mdl" : "models/hgrunt_torch.mdl", true, m_iSpriteTexture );
}

void CHGruntOp4TorchRepel::RepelUse( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	HumanGruntRepelUse( this, "monster_human_torch_ally" );
}

class CHWGruntRepel : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT RepelUse( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value );

	int m_iSpriteTexture;
};

LINK_ENTITY_TO_CLASS( monster_hwgrunt_repel, CHWGruntRepel );

void CHWGruntRepel::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;

	if ( FStringNull( pev->targetname ) )
		RepelUse( NULL, NULL, USE_TOGGLE, 0 );
	else
		SetUse( &CHWGruntRepel::RepelUse );
}

void CHWGruntRepel::Precache( void )
{
	HumanGruntRepelPrecache( this, "monster_hwgrunt",
	                         IsPlayerAlly() ? "models/hwgruntf.mdl" : "models/hwgrunt.mdl", false, m_iSpriteTexture );
}

void CHWGruntRepel::RepelUse( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	HumanGruntRepelUse( this, "monster_hwgrunt" );
}

class CRobotGruntRepel : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT RepelUse( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value );

	int m_iSpriteTexture;
};

LINK_ENTITY_TO_CLASS( monster_robogrunt_repel, CRobotGruntRepel );

void CRobotGruntRepel::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;

	if ( FStringNull( pev->targetname ) )
		RepelUse( NULL, NULL, USE_TOGGLE, 0 );
	else
		SetUse( &CRobotGruntRepel::RepelUse );
}

void CRobotGruntRepel::Precache( void )
{
	HumanGruntRepelPrecache( this, "monster_robogrunt",
	                         IsPlayerAlly() ? "models/rgruntf.mdl" : "models/rgrunt.mdl", false, m_iSpriteTexture );
}

void CRobotGruntRepel::RepelUse( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	HumanGruntRepelUse( this, "monster_robogrunt" );
}

class CMassnRepel : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT RepelUse( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value );

	int m_iSpriteTexture;
};

LINK_ENTITY_TO_CLASS( monster_assassin_repel, CMassnRepel );

void CMassnRepel::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;

	if ( FStringNull( pev->targetname ) )
		RepelUse( NULL, NULL, USE_TOGGLE, 0 );
	else
		SetUse( &CMassnRepel::RepelUse );
}

void CMassnRepel::Precache( void )
{
	HumanGruntRepelPrecache( this, "monster_male_assassin",
	                         IsPlayerAlly() ? "models/massnf.mdl" : "models/massn.mdl", false, m_iSpriteTexture );
}

void CMassnRepel::RepelUse( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	HumanGruntRepelUse( this, "monster_male_assassin" );
}
