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

===== bmodels.cpp ========================================================

  spawn, think, and use functions for entities that use brush models

*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "func_break.h"
#include "decals.h"
#include "explode.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "pipewrench.h"
#include "CClassicMode.h"
#include "server_sound.h"

extern DLL_GLOBAL Vector		g_vecAttackDir;

CBaseEntity *BreakableAttackerEntity( entvars_t *pev )
{
	edict_t *entity = pev ? pev->pContainingEntity : NULL;

	if ( !entity )
		entity = INDEXENT( 0 );

	return entity && !entity->free ? (CBaseEntity *) GET_PRIVATE( entity ) : NULL;
}

bool BreakableHasChargedWrench( CBaseEntity *attacker )
{
	if ( !attacker || !attacker->IsNetClient() )
		return false;

	CBasePlayer *player = dynamic_cast<CBasePlayer *>( attacker );

	if ( !player )
		return false;

	edict_t *activeEdict = player->m_pActiveItem.Get();
	CBasePlayerItem *active = activeEdict && !activeEdict->free
	                              ? (CBasePlayerItem *) GET_PRIVATE( activeEdict )
	                              : NULL;

	if ( !active )
		return false;

	ItemInfo info = {};

	active->GetItemInfo( &info );

	if ( info.iId != WEAPON_PIPEWRENCH )
		return false;

	CWrench *wrench = dynamic_cast<CWrench *>( active->GetWeaponPtr() );

	return wrench && wrench->m_iChargeBonus > 0;
}

CBreakable::CBreakable()
    : m_Material( matGlass ), m_Explosion( expRandom ), m_idShard( 0 ), m_angle( 0 ),
      m_iszGibModel( 0 ), m_iszSpawnObject( 0 ), m_instantBreakWeapon( 0 ), m_fUseTriggered( false ),
      m_fDead( false ), m_iDamageIndex( 0 )
{
	memset( m_flDamage, 0, sizeof( m_flDamage ) );
	memset( m_flDamageTime, 0, sizeof( m_flDamageTime ) );

	for ( int i = 0; i < 64; ++i )
		m_vecDamageDirection[i] = g_vecZero;
}

CBreakable::~CBreakable()
{
}

CAttackVectors *CBreakable::GetAttackVectors( void ) const
{
	return const_cast<CAttackVectors *>( &m_AttackVectors );
}

void CBreakable::LogDamage( float damage, Vector direction )
{
	if ( damage > 0 )
	{
		m_flDamage[m_iDamageIndex] = damage;
		m_flDamageTime[m_iDamageIndex] = gpGlobals->time;
		m_vecDamageDirection[m_iDamageIndex] = direction;
		m_iDamageIndex = ( m_iDamageIndex + 1 ) % 64;
	}
}

float CBreakable::GetRecentTotalDamage( float &maximum, Vector &direction, float period )
{
	float total = 0;
	float cutoff = gpGlobals->time - period;

	for ( int i = 0; i < 64; ++i )
	{
		if ( m_flDamageTime[i] >= cutoff )
		{
			total += m_flDamage[i];

			if ( m_flDamage[i] > maximum )
			{
				maximum = m_flDamage[i];
				direction = m_vecDamageDirection[i];
			}
		}
	}

	return total;
}

bool CBreakable::CanInstantBreak( CBaseEntity *attacker, int weapon )
{
	if ( !attacker || !attacker->IsNetClient() )
		return false;

	CBasePlayer *player = static_cast<CBasePlayer *>( attacker );
	edict_t *activeEdict = player->m_pActiveItem.Get();
	CBasePlayerItem *active = activeEdict && !activeEdict->free
	                              ? (CBasePlayerItem *) GET_PRIVATE( activeEdict )
	                              : NULL;

	if ( !active )
		return false;

	ItemInfo info = {};

	active->GetItemInfo( &info );

	int weaponId = info.iId;

	if ( weaponId == WEAPON_CROWBAR )
	{
		CCrowbar *crowbar = dynamic_cast<CCrowbar *>( active->GetWeaponPtr() );

		if ( crowbar && crowbar->m_fElectric )
			// The electric crowbar uses a separate breakable selector
			weaponId = 19;
	}

	if ( weaponId == 19 && weapon == WEAPON_CROWBAR )
		return true;

	return weaponId > 0 && weaponId == weapon;
}

bool CBreakable::CheckAttacker( CBaseEntity *attacker )
{
	int classification = Classify();
	int attackerClass = CLASS_NONE;
	int relationship = R_NO;
	bool client = false;

	if ( attacker )
	{
		attackerClass = attacker->Classify();
		relationship = IRelationship( attacker, true );
		client = ( attacker->pev->flags & FL_CLIENT ) != 0;

		if ( client && ( pev->spawnflags & SF_BREAK_IMMUNE_CLIENTS ) )
			return true;
	}

	switch ( classification )
	{
		case CLASS_PLAYER:
		case CLASS_HUMAN_PASSIVE:
		case CLASS_PLAYER_ALLY:
			return client || attackerClass == CLASS_PLAYER_ALLY || attackerClass == CLASS_HUMAN_PASSIVE;

		case CLASS_TEAM1:
		case CLASS_TEAM2:
		case CLASS_TEAM3:
		case CLASS_TEAM4:
			if ( relationship > R_NO )
				return false;

			return client || classification == attackerClass;

		default:
			return false;
	}
}

void *CBreakable::GetKeyValue( const char *name, entvartype_s &type )
{
	if ( FStrEq( name, "explosion" ) )
	{
		type = VAR_INTEGER;
		return &m_Explosion;
	}

	if ( FStrEq( name, "material" ) )
	{
		type = VAR_INTEGER;
		return &m_Material;
	}

	if ( FStrEq( name, "gibmodel" ) )
	{
		type = VAR_STRING;
		return &m_iszGibModel;
	}

	if ( FStrEq( name, "spawnobject" ) )
	{
		type = VAR_STRING;
		return &m_iszSpawnObject;
	}

	if ( FStrEq( name, "explodemagnitude" ) )
	{
		type = VAR_INTEGER;
		return &pev->impulse;
	}

	if ( FStrEq( name, "weapon" ) )
	{
		type = VAR_INTEGER;
		return &m_instantBreakWeapon;
	}

	if ( FStrEq( name, "displayname" ) )
	{
		type = VAR_STRING;
		return &pev->message;
	}

	return CBaseEntity::GetKeyValue( name, type );
}

// =================== FUNC_Breakable ==============================================

// Just add more items to the bottom of this array and they will automagically be supported
// This is done instead of just a classname in the FGD so we can control which entities can
// be spawned, and still remain fairly flexible
const char *CBreakable::pSpawnObjects[] = {
	NULL,
	"item_battery",
	"item_healthkit",
	"weapon_9mmhandgun",
	"ammo_9mmclip",
	"weapon_9mmAR",
	"ammo_9mmAR",
	"ammo_ARgrenades",
	"weapon_shotgun",
	"ammo_buckshot",
	"weapon_crossbow",
	"ammo_crossbow",
	"weapon_357",
	"ammo_357",
	"weapon_rpg",
	"ammo_rpgclip",
	"ammo_gaussclip",
	"weapon_handgrenade",
	"weapon_tripmine",
	"weapon_satchel",
	"weapon_snark",
	"weapon_hornetgun",
	"weapon_crowbar",
	"weapon_pipewrench",
	"weapon_sniperrifle",
	"ammo_762",
	"weapon_m16",
	"weapon_m249",
	"weapon_minigun",
	"ammo_556",
	"weapon_sporelauncher",
	"ammo_sporeclip",
	"ammo_9mmbox",
	"weapon_uzi",
	"weapon_uziakimbo",
	"weapon_eagle",
	"weapon_grapple",
	"weapon_medkit",
	"item_suit",
	"item_antidote",
};

void CBreakable::KeyValue( KeyValueData* pkvd )
{
	// UNDONE_WC: explicitly ignoring these fields, but they shouldn't be in the map file!
	if (FStrEq(pkvd->szKeyName, "explosion"))
		m_Explosion = atoi(pkvd->szValue ) == 1 ? expDirected : expRandom;
	else if (FStrEq(pkvd->szKeyName, "material"))
	{
		int i = atoi( pkvd->szValue);

		if ( (unsigned int) i <= matNone )
			m_Material = (Materials)i;
		else
			m_Material = matWood;
	}
	else if (FStrEq(pkvd->szKeyName, "deadmodel") || FStrEq(pkvd->szKeyName, "shards"))
	{
	}
	else if (FStrEq(pkvd->szKeyName, "gibmodel") )
		m_iszGibModel = AllocStringLocal(pkvd->szValue);
	else if (FStrEq(pkvd->szKeyName, "spawnobject") )
	{
		int object = atoi( pkvd->szValue );
		if ( object > 0 && object < ARRAYSIZE(pSpawnObjects) )
			m_iszSpawnObject = MAKE_STRING( pSpawnObjects[object] );
	}
	else if (FStrEq(pkvd->szKeyName, "explodemagnitude") )
		ExplosionSetMagnitude( atoi( pkvd->szValue ) );
	else if (FStrEq(pkvd->szKeyName, "lip") )
	{
	}
	else if ( FStrEq( pkvd->szKeyName, "weapon" ) )
	{
		int weapon = atoi( pkvd->szValue );

		if ( weapon == 1 || weapon == 19 || weapon == 20 )
			m_instantBreakWeapon = weapon;
		else
			m_instantBreakWeapon = 1;
	}
	else if ( FStrEq( pkvd->szKeyName, "displayname" ) )
		pev->message = AllocStringLocal( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "classify" ) )
		SetClassificationSpecial( atoi( pkvd->szValue ), true );
	else
	{
		CBaseDelay::KeyValue( pkvd );
		return;
	}

	pkvd->fHandled = TRUE;
}

//
// func_breakable - bmodel that breaks into pieces after taking damage
//
LINK_ENTITY_TO_CLASS( func_breakable, CBreakable );
TYPEDESCRIPTION CBreakable::m_SaveData[] = {
	DEFINE_FIELD( CBreakable, m_Material, FIELD_INTEGER ),
	DEFINE_FIELD( CBreakable, m_Explosion, FIELD_INTEGER ),

	// Don't need to save/restore these because we precache after restore
	//	DEFINE_FIELD( CBreakable, m_idShard, FIELD_INTEGER ),

	DEFINE_FIELD( CBreakable, m_angle, FIELD_FLOAT ),
	DEFINE_FIELD( CBreakable, m_iszGibModel, FIELD_STRING ),
	DEFINE_FIELD( CBreakable, m_iszSpawnObject, FIELD_STRING ),
	DEFINE_FIELD( CBreakable, m_instantBreakWeapon, FIELD_INTEGER ),

	// Explosion magnitude is stored in pev->impulse
};

IMPLEMENT_SAVERESTORE( CBreakable, CBaseEntity );

void CBreakable::Spawn( void )
{
    Precache( );

	if ( pev->spawnflags & SF_BREAK_TRIGGER_ONLY )
		pev->takedamage = DAMAGE_NO;
	else
		pev->takedamage = DAMAGE_YES;


	pev->solid		= SOLID_BSP;
    pev->movetype	= MOVETYPE_PUSH;
    m_angle			= pev->angles.y;
	pev->angles.y	= 0;
	pev->max_health = pev->health;

	if ( !pev->message )
		pev->message = MAKE_STRING( "Breakable" );

	if ( ( pev->spawnflags & SF_BREAK_REPAIRABLE ) && m_iClassSelection == CLASS_NONE )
	{
		m_fOverrideClass = true;
		m_iClassSelection = CLASS_PLAYER_ALLY;
	}

	if ( m_Material == matGlass )
		pev->playerclass = 1;

	SetModel_Intercept( edict(), STRING(pev->model) );//set size and link into world.

	SetTouch( &CBreakable::BreakTouch );
	if ( pev->spawnflags & SF_BREAK_TRIGGER_ONLY )		// Only break on trigger
		SetTouch( NULL );

	// Flag unbreakable glass as "worldbrush" so it will block ALL tracelines
	if ( !IsBreakable() && pev->rendermode != kRenderNormal )
		pev->flags |= FL_WORLDBRUSH;

	if ( !m_instantBreakWeapon && ( pev->spawnflags & SF_BREAK_CROWBAR ) )
		m_instantBreakWeapon = WEAPON_CROWBAR;

	m_AttackVectors.CalculateAttackVectors( this );
}

const char *CBreakable::pSoundsWood[] = 
{
	"debris/wood1.wav",
	"debris/wood2.wav",
	"debris/wood3.wav",
};

const char *CBreakable::pSoundsFlesh[] = 
{
	"debris/flesh1.wav",
	"debris/flesh2.wav",
	"debris/flesh3.wav",
	"debris/flesh5.wav",
	"debris/flesh6.wav",
	"debris/flesh7.wav",
};

const char *CBreakable::pSoundsMetal[] = 
{
	"debris/metal1.wav",
	"debris/metal2.wav",
	"debris/metal3.wav",
};

const char *CBreakable::pSoundsConcrete[] = 
{
	"debris/concrete1.wav",
	"debris/concrete2.wav",
	"debris/concrete3.wav",
};


const char *CBreakable::pSoundsGlass[] = 
{
	"debris/glass1.wav",
	"debris/glass2.wav",
	"debris/glass3.wav",
};

const char **CBreakable::MaterialSoundList( Materials precacheMaterial, int &soundCount )
{
	const char	**pSoundList = NULL;

    switch ( precacheMaterial ) 
	{
	case matWood:
		pSoundList = pSoundsWood;
		soundCount = ARRAYSIZE(pSoundsWood);
		break;
	case matFlesh:
		pSoundList = pSoundsFlesh;
		soundCount = ARRAYSIZE(pSoundsFlesh);
		break;
	case matComputer:
	case matUnbreakableGlass:
	case matGlass:
		pSoundList = pSoundsGlass;
		soundCount = ARRAYSIZE(pSoundsGlass);
		break;

	case matMetal:
		pSoundList = pSoundsMetal;
		soundCount = ARRAYSIZE(pSoundsMetal);
		break;

	case matCinderBlock:
	case matRocks:
		pSoundList = pSoundsConcrete;
		soundCount = ARRAYSIZE(pSoundsConcrete);
		break;
	
	
	case matCeilingTile:
	case matNone:
	default:
		soundCount = 0;
		break;
	}

	return pSoundList;
}

void CBreakable::MaterialSoundPrecache( Materials precacheMaterial )
{
	const char	**pSoundList;
	int			i, soundCount = 0;

	pSoundList = MaterialSoundList( precacheMaterial, soundCount );

	for ( i = 0; i < soundCount; i++ )
	{
		CServer_SoundEngine::GetSoundEngine()->PrecacheSound( pSoundList[i] );
	}
}

void CBreakable::MaterialSoundRandom( edict_t *pEdict, Materials soundMaterial, float volume )
{
	const char	**pSoundList;
	int			soundCount = 0;

	pSoundList = MaterialSoundList( soundMaterial, soundCount );

	if ( soundCount )
		EMIT_SOUND_DYN( pEdict, CHAN_BODY, pSoundList[ RANDOM_LONG(0,soundCount-1) ], volume, 1, 0, PITCH_NORM, 0, false, g_vecZero );
}

void CBreakable::Precache( void )
{
	CBaseEntity::Precache();

	const char *pGibName = NULL;

    switch (m_Material)
	{
	case matWood:
		pGibName = "models/woodgibs.mdl";

			PrecacheSound_Intercept( this, "debris/bustcrate1.wav");
			PrecacheSound_Intercept( this, "debris/bustcrate2.wav");
		break;
	case matFlesh:
		pGibName = "models/fleshgibs.mdl";

			PrecacheSound_Intercept( this, "debris/bustflesh1.wav");
			PrecacheSound_Intercept( this, "debris/bustflesh2.wav");
		break;
	case matComputer:
			PrecacheSound_Intercept( this, "buttons/spark5.wav");
			PrecacheSound_Intercept( this, "buttons/spark6.wav");
		pGibName = "models/computergibs.mdl";

			PrecacheSound_Intercept( this, "debris/bustmetal1.wav");
			PrecacheSound_Intercept( this, "debris/bustmetal2.wav");
		break;

	case matUnbreakableGlass:
	case matGlass:
		pGibName = "models/glassgibs.mdl";

			PrecacheSound_Intercept( this, "debris/bustglass1.wav");
			PrecacheSound_Intercept( this, "debris/bustglass2.wav");
		break;
	case matMetal:
		pGibName = "models/metalplategibs.mdl";

			PrecacheSound_Intercept( this, "debris/bustmetal1.wav");
			PrecacheSound_Intercept( this, "debris/bustmetal2.wav");
		break;
	case matCinderBlock:
		pGibName = "models/cindergibs.mdl";

			PrecacheSound_Intercept( this, "debris/bustconcrete1.wav");
			PrecacheSound_Intercept( this, "debris/bustconcrete2.wav");
		break;
	case matRocks:
		pGibName = "models/rockgibs.mdl";

			PrecacheSound_Intercept( this, "debris/bustconcrete1.wav");
			PrecacheSound_Intercept( this, "debris/bustconcrete2.wav");
		break;
	case matCeilingTile:
		pGibName = "models/ceilinggibs.mdl";

			PrecacheSound_Intercept( this, "debris/bustceiling.wav");
		break;
	}
	MaterialSoundPrecache( m_Material );
	if ( m_iszGibModel )
		pGibName = STRING(m_iszGibModel);

	m_idShard = PrecacheModel_Intercept( this, pGibName );

	// Precache the spawn item's data
	if ( m_iszSpawnObject )
		UTIL_PrecacheOther( (char *)STRING( m_iszSpawnObject ) );
}

// play shard sound when func_breakable takes damage.
// the more damage, the louder the shard sound.

void CBreakable::DamageSound( void )
{
	int pitch;
	float fvol;
	const char *rgpsz[6];
	int i = 0;
	int material = m_Material;

//	if (RANDOM_LONG(0,1))
//		return;

	if (RANDOM_LONG(0,2))
		pitch = PITCH_NORM;
	else
		pitch = 95 + RANDOM_LONG(0,34);

	fvol = RANDOM_FLOAT(0.75, 1.0);

	if (material == matComputer && RANDOM_LONG(0,1))
		material = matMetal;

	switch (material)
	{
	case matComputer:
	case matGlass:
	case matUnbreakableGlass:
		rgpsz[0] = "debris/glass1.wav";
		rgpsz[1] = "debris/glass2.wav";
		rgpsz[2] = "debris/glass3.wav";
		i = 3;
		break;

	case matWood:
		rgpsz[0] = "debris/wood1.wav";
		rgpsz[1] = "debris/wood2.wav";
		rgpsz[2] = "debris/wood3.wav";
		i = 3;
		break;

	case matMetal:
		rgpsz[0] = "debris/metal1.wav";
		rgpsz[1] = "debris/metal3.wav";
		rgpsz[2] = "debris/metal2.wav";
		i = 2;
		break;

	case matFlesh:
		rgpsz[0] = "debris/flesh1.wav";
		rgpsz[1] = "debris/flesh2.wav";
		rgpsz[2] = "debris/flesh3.wav";
		rgpsz[3] = "debris/flesh5.wav";
		rgpsz[4] = "debris/flesh6.wav";
		rgpsz[5] = "debris/flesh7.wav";
		i = 6;
		break;

	case matRocks:
	case matCinderBlock:
		rgpsz[0] = "debris/concrete1.wav";
		rgpsz[1] = "debris/concrete2.wav";
		rgpsz[2] = "debris/concrete3.wav";
		i = 3;
		break;

	case matCeilingTile:
		// UNDONE: no ceiling tile shard sound yet
		i = 0;
		break;
	}

	if ( !( pev->spawnflags & SF_BREAK_EXPLOSIVES_ONLY ) && i)
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, rgpsz[RANDOM_LONG(0,i-1)], fvol, ATTN_NORM, 0, pitch, 0, false, g_vecZero );
}

void CBreakable::BreakTouch( CBaseEntity *pOther )
{
	entvars_t*	pevToucher = pOther->pev;

	// only players can break these right now
	if ( CheckAttacker( pOther ) || !pOther->IsNetClient() || !IsBreakable() )
		return;

	if ( pev->spawnflags & SF_BREAK_TOUCH )
	{
		float flDamage = (float) ( pevToucher->velocity.Length() * 0.01 );

		if (flDamage >= pev->health)
		{
			SetTouch( NULL );
			TakeDamage(pevToucher, pevToucher, flDamage, DMG_CRUSH);

			// do a little damage to player if we broke glass or computer
			pOther->TakeDamage( pev, pev, flDamage * 0.25f, DMG_SLASH );
		}
	}

	if ( ( pev->spawnflags & SF_BREAK_PRESSURE ) && pevToucher->absmin.z >= pev->maxs.z - 2 )
	{// can be broken when stood upon

		// play creaking sound here.
		DamageSound();

		SetThink ( &CBreakable::Die );
		SetTouch( NULL );
		SUB_UseTargets( pOther, USE_TOGGLE, 0 );

		if ( m_flDelay == 0 )
			m_flDelay = 0.1;

		pev->nextthink = pev->ltime + m_flDelay;

	}

}

//
// Smash the our breakable object
//

// Break when triggered
void CBreakable::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( IsBreakable() )
	{
		pev->angles.y = m_angle;
		UTIL_MakeVectors(pev->angles);
		g_vecAttackDir = -gpGlobals->v_up;
		pev->health = -50;
		m_fUseTriggered = true;

		Die();
		SUB_UseTargets( pActivator, USE_TOGGLE, 0 );
	}
}

void CBreakable::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	CBaseEntity *attackerEntity = NULL;

	if ( pev->spawnflags & SF_BREAK_REPAIRABLE )
		attackerEntity = BreakableAttackerEntity( pevAttacker );

	if ( ( pev->spawnflags & SF_BREAK_REPAIRABLE ) && attackerEntity && ( bitsDamageType & DMG_CLUB )
	     && BreakableHasChargedWrench( attackerEntity ) && CheckAttacker( attackerEntity ) )
	{
		if ( pev->max_health > pev->health )
		{
			EMIT_SOUND_DYN( edict(), CHAN_AUTO, "buttons/latchunlocked2.wav", 1, 0.6f, 0, 100, 0, false, g_vecZero );
			TakeHealth( fabsf( flDamage ), bitsDamageType, 0 );
			pev->health = V_max( 1.0f, pev->health );
			pev->nextthink = gpGlobals->time;
		}

		return;
	}

	if (RANDOM_LONG(0,1) )
	{
		if ( m_Material == matComputer )
		{
				UTIL_Sparks( ptr->vecEndPos );

				float flVolume = RANDOM_FLOAT ( 0.7 , 1.0 );
			int sound = RANDOM_LONG(0,1);

			if ( sound == 0 )
				EMIT_SOUND_DYN( edict(), CHAN_VOICE, "buttons/spark5.wav", flVolume, 0.8f, 0, 100, 0, false, g_vecZero );
			else if ( sound == 1 )
				EMIT_SOUND_DYN( edict(), CHAN_VOICE, "buttons/spark6.wav", flVolume, 0.8f, 0, 100, 0, false, g_vecZero );
		}
		else if ( m_Material == matUnbreakableGlass )
			UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT(0.5,1.5) );
	}

	CBaseEntity::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

//=========================================================
// Special takedamage for func_breakable. Allows us to make
// exceptions that are breakable-specific
// bitsDamageType indicates the type of damage sustained ie: DMG_CRUSH
//=========================================================
int CBreakable :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	if ( m_fDead )
		return 0;

	// if Attacker == Inflictor, the attack was a melee or other instant-hit attack.
	// (that is, no actual entity projectile was involved in the attack so use the shooter's origin).
	if ( !pevInflictor || !pevInflictor->pContainingEntity )
		// Probes index zero without replacing the inflictor
		INDEXENT( 0 );

	CBaseEntity *attackerEntity = BreakableAttackerEntity( pevAttacker );

	if ( CheckAttacker( attackerEntity ) )
		return 0;

	if ( attackerEntity && attackerEntity->IsMonster() )
		flDamage *= attackerEntity->MyMonsterPointer()->m_flEffectDamage;

	Vector direction( 0, 0, 0 );

	if ( pevInflictor )
	{
		direction = pevInflictor->origin - ( pev->absmin + pev->size * 0.5 );

		// if a client hit the breakable with a crowbar, and breakable is crowbar-sensitive, break it now.
		if ( pevAttacker == pevInflictor && FClassnameIs( pevAttacker, "player" )
		     && ( pev->spawnflags & SF_BREAK_CROWBAR ) && CanInstantBreak( attackerEntity, m_instantBreakWeapon ))
			flDamage = pev->health;
	}

	if (!IsBreakable() || ( pev->spawnflags & SF_BREAK_TRIGGER_ONLY ) )
		return 0;

	g_vecAttackDir = direction.Normalize();

	// Breakables take double damage from the crowbar
	if ( bitsDamageType & DMG_CLUB )
	{
		LogDamage( (float) ( flDamage * 0.2 ), g_vecAttackDir );
		flDamage += flDamage;
	}
	else if ( bitsDamageType & DMG_BULLET )
	{
		if ( bitsDamageType == DMG_BULLET && pevAttacker == pevInflictor )
			LogDamage( flDamage, g_vecAttackDir );
		else if ( m_Material != matGlass )
			LogDamage( (float) ( flDamage * 0.1 ), g_vecAttackDir );
	}
	else
		LogDamage( bitsDamageType & DMG_POISON ? (float) ( flDamage * 0.1 ) : flDamage, g_vecAttackDir );

	// Boxes / glass / etc. don't take much poison damage, just the impact of the dart - consider that 10%
	if ( bitsDamageType & DMG_POISON )
		flDamage = (float) ( flDamage * 0.1 );

	if ( ( pev->spawnflags & SF_BREAK_EXPLOSIVES_ONLY ) && ( !( bitsDamageType & DMG_BLAST ) || FClassnameIs( pevInflictor, "weapon_egon" ) ) )
		flDamage = 0;

	if ( pevInflictor )
		pev->dmg_inflictor = pevInflictor->pContainingEntity;

// do the damage
	pev->health -= flDamage;
	if (pev->health <= 0)
	{
		Killed( pevAttacker, GIB_NORMAL );
		Die();

		if ( pevAttacker )
			SUB_UseTargets( attackerEntity, USE_TOGGLE, 0 );

		return 0;
	}

	// Make a shard noise each time func breakable is hit.
	// Don't play shard noise if cbreakable actually died.

	DamageSound();

	return 1;
}

void CBreakable::Die( void )
{
	if ( m_fDead )
		return;

	m_fDead = true;

	int pitch = 95 + RANDOM_LONG(0,29);

	if (pitch > 97 && pitch < 103)
		pitch = 100;

	float fvol = (float) ( RANDOM_FLOAT(0.85, 1.0) + fabs(pev->health) / 100.0);

	if (fvol > 1 )
		fvol = 1;

	float materialScale = 1;
	int cFlag = 0;
	const char *sound1 = NULL;
	const char *sound2 = NULL;


	switch (m_Material)
	{
	case matGlass:
			sound1 = "debris/bustglass1.wav";
			sound2 = "debris/bustglass2.wav";
			materialScale = 0.6f;
			cFlag = BREAK_GLASS;
		break;

	case matWood:
			sound1 = "debris/bustcrate1.wav";
			sound2 = "debris/bustcrate2.wav";
			materialScale = 0.8f;
			cFlag = BREAK_WOOD;
		break;

	case matComputer:
	case matMetal:
			sound1 = "debris/bustmetal1.wav";
			sound2 = "debris/bustmetal2.wav";
			cFlag = BREAK_METAL;
		break;

	case matFlesh:
			sound1 = "debris/bustflesh1.wav";
			sound2 = "debris/bustflesh2.wav";
			materialScale = 0.9f;
			cFlag = BREAK_FLESH;
		break;

	case matRocks:
	case matCinderBlock:
			sound1 = "debris/bustconcrete1.wav";
			sound2 = "debris/bustconcrete2.wav";
			materialScale = 0.5f;
			cFlag = BREAK_CONCRETE;
		break;

	case matCeilingTile:
			materialScale = 0.8f;
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, "debris/bustceiling.wav", fvol, 0.8f, 0, pitch, 0, false, g_vecZero );
			break;

		default:
			break;
	}


	if ( sound1 )
	{
		int sound = RANDOM_LONG( 0, 1 );

		if ( sound == 0 )
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, sound1, fvol, 0.8f, 0, pitch, 0, false, g_vecZero );
		else if ( sound == 1 )
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, sound2, fvol, 0.8f, 0, pitch, 0, false, g_vecZero );
	}

	float totalDamage = fabsf( pev->health );
	float maximumDamage = totalDamage;
	Vector velocity( 0, 0, 0 );

	if ( m_Explosion == expDirected && !m_fUseTriggered )
	{
		Vector direction( 0, 0, 0 );

		maximumDamage = 0;
		totalDamage = GetRecentTotalDamage( maximumDamage, direction, 0.3f );

		double speed = (double) totalDamage * 2.3 * materialScale;

		if ( speed > 60.0 )
			speed = speed;
		else
			speed = 60.0;

		if ( speed >= 1300.0 )
			speed = 1300.0;

		velocity = direction * (float) -speed;
	}

	double explosionLift = (double) ExplosionMagnitude() * 0.8;

	if ( explosionLift >= 400.0 )
		explosionLift = 400.0;

	double lift = (double) velocity.z + explosionLift;

	velocity.z = (float) ( lift < 1300.0 ? lift : 1300.0 );
	totalDamage += (float) ExplosionMagnitude();
	maximumDamage += (float) ExplosionMagnitude();

	Vector vecSpot = pev->origin + (pev->mins + pev->maxs) * 0.5;
	float size = pev->size.Length();
	float sizeScale;

	if ( !( size < 165 ) )
		sizeScale = 1;
	else if ( size > 80 )
		sizeScale = ( 165.0f - size ) / 56.666668f + 1;
	else
		sizeScale = 2.5f;

	velocity.x *= sizeScale;
	velocity.y *= sizeScale;
	velocity.z *= sizeScale;

	if ( !( velocity.x < 1300 ) )
		velocity.x = 1300;

	if ( !( velocity.y < 1300 ) )
		velocity.y = 1300;

	if ( !( velocity.z < 1300 ) )
		velocity.z = 1300;

	float damageSpeed = (float) sqrt( (float) ( fabsf( totalDamage ) * 250.0f ) );
	float sizeRoot = (float) sqrt( sizeScale );
	float randomization = fabsf( ( damageSpeed + maximumDamage ) * sizeRoot );
	float materialRoot = (float) sqrt( materialScale );

	randomization = ( randomization / 15.0f ) * materialRoot;

	if ( randomization < 6 )
		randomization = 6;
	else if ( !( randomization < 50 ) )
		randomization = 50;

	double duration = (double) velocity.Length() / 20.0 + 15.0;

	if ( !( duration > 25 ) )
		duration = 25;
	else if ( !( duration < 40 ) )
		duration = 40;
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
		WRITE_BYTE( TE_BREAKMODEL);

		// position
		WRITE_COORD( vecSpot.x );
		WRITE_COORD( vecSpot.y );
		WRITE_COORD( vecSpot.z );

		// size
		WRITE_COORD( pev->size.x);
		WRITE_COORD( pev->size.y);
		WRITE_COORD( pev->size.z);

		// velocity
		WRITE_COORD( velocity.x );
		WRITE_COORD( velocity.y );
		WRITE_COORD( velocity.z );

		// randomization
		WRITE_BYTE( (int) randomization );

		// Model
		WRITE_SHORT( m_idShard );	//model id#

		// # of shards
		WRITE_BYTE( 0 );	// let client decide

		// duration
		WRITE_BYTE( (int) duration );// 2.5 seconds

		// flags
		WRITE_BYTE( cFlag );
	MESSAGE_END();

	// !!! HACK  This should work!
	// Build a box above the entity that looks like an 8 pixel high sheet
	Vector mins = pev->absmin;
	Vector maxs = pev->absmax;
	mins.z = pev->absmax.z;
	maxs.z += 8;

	// BUGBUG -- can only find 256 entities on a breakable -- should be enough
	CBaseEntity *pList[256];
	int count = UTIL_EntitiesInBox( pList, ARRAYSIZE( pList ), mins, maxs, FL_ONGROUND );

	for ( int i = 0; i < count; ++i )
	{
		pList[i]->pev->flags &= ~FL_ONGROUND;
			pList[i]->pev->groundentity = NULL;
		}

	pev->targetname = 0;

	pev->solid = SOLID_NOT;

	SetThink( &CBreakable::SUB_Remove );
	pev->nextthink = pev->ltime + 0.1;
	if ( m_iszSpawnObject )
	{
		const CString &mapped = CClassicMode::GetInstance()->FindMappedItem( STRING( m_iszSpawnObject ) );

		if ( mapped.Length() )
			m_iszSpawnObject = AllocStringLocal( mapped.Data() );

		CBaseEntity::Create( STRING(m_iszSpawnObject), VecBModelOrigin(pev), pev->angles, false, edict() );
	}

	if ( Explodable() )
	{
		entvars_t *source = pev;
		int magnitude = ExplosionMagnitude();
		edict_t *owner = source->pContainingEntity;
		Vector center = Center();

		CreateBreakableExplosion( center, source->angles, owner, magnitude );
	}
}

bool CBreakable :: IsBreakable( void )
{ 
	return m_Material != matUnbreakableGlass;
}


int	CBreakable :: DamageDecal( int bitsDamageType )
{
	if ( m_Material == matGlass  )
		return DECAL_GLASSBREAK1 + RANDOM_LONG(0,2);

	if ( m_Material == matUnbreakableGlass )
		return DECAL_BPROOF1;

	return CBaseEntity::DamageDecal( bitsDamageType );
}


CPushable::CPushable()
    : m_vecHoverPosition( 0, 0, 0 )
{
}

TYPEDESCRIPTION CPushable::m_SaveData[] = {
	DEFINE_FIELD( CPushable, m_maxSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CPushable, m_soundTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CPushable, CBreakable );

LINK_ENTITY_TO_CLASS( func_pushable, CPushable );

const char *CPushable::m_soundNames[3] = { "debris/pushbox1.wav", "debris/pushbox2.wav", "debris/pushbox3.wav" };

void CPushable :: Spawn( void )
{
	int solid = pev->solid;

	if ( pev->spawnflags & SF_PUSH_BREAKABLE )
		CBreakable::Spawn();
	else
		Precache( );

	pev->solid = solid;
	pev->movetype	= MOVETYPE_PUSHSTEP;

	if ( pev->solid != SOLID_BSP )
		pev->solid		= SOLID_BBOX;

	SetModel_Intercept( edict(), STRING(pev->model) );

	if ( pev->friction > 399 )
		pev->friction = 399;

	m_maxSpeed = 400 - pev->friction;
	SetBits( pev->flags, FL_FLOAT );
	pev->friction = 0;

	pev->origin.z += 1;	// Pick up off of the floor
	UTIL_SetOrigin( pev, pev->origin );

	if ( !m_fCustomSize )
		UTIL_SetSize( pev, pev->mins, pev->maxs );

	pev->skin = ( pev->skin * (pev->maxs.x - pev->mins.x) * (pev->maxs.y - pev->mins.y) ) * 0.0005;
	m_soundTime = 0;
	m_flPushTime = 0;

	if ( INDEXENT( 0 ) && !( pev->spawnflags & SF_PUSH_START_IN_SOLID ) )
	{
		float width = pev->maxs.x - pev->mins.x;
		float height = pev->maxs.z - pev->mins.z;
		int hull;

		if ( width <= 6 )
			hull = 0;
		else if ( width <= 36 )
			hull = height <= 36 ? 3 : 1;
		else
			hull = 2;

		TraceResult trace;

		UTIL_TraceHull( Center(), Center(), ignore_monsters, hull, edict(), &trace );

		if ( trace.fStartSolid )
			pev->spawnflags |= SF_PUSH_START_IN_SOLID;
	}
}

void CPushable :: Precache( void )
{
	CBaseEntity::Precache();

	for ( int i = 0; i < 3; ++i )
		PrecacheSound_Intercept( this, m_soundNames[i] );

	if ( pev->spawnflags & SF_PUSH_BREAKABLE )
		CBreakable::Precache( );
}

void CPushable :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq(pkvd->szKeyName, "size") )
	{
		m_fCustomSize = true;

		switch ( atoi(pkvd->szValue) )
		{
		case 0:	// Point
			UTIL_SetSize(pev, g_vecZero, g_vecZero );
				break;

			case 1:
				UTIL_SetSize( pev, VEC_HULL_MIN, VEC_HULL_MAX );
			break;

		case 2: // Big Hull!?!?	!!!BUGBUG Figure out what this hull really is
			UTIL_SetSize(pev, VEC_DUCK_HULL_MIN*2, VEC_DUCK_HULL_MAX*2);
			break;

		case 3: // Player duck
			UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
			break;

		default:
				m_fCustomSize = false;
				break;
		}

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "minhullsize" ) )
	{
		Vector size = g_vecZero;

		UTIL_StringToVector( size, pkvd->szValue, ' ' );

		if ( size != g_vecZero )
			UTIL_SetSize(pev, size, pev->maxs );

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "maxhullsize" ) )
	{
		Vector size = g_vecZero;

		UTIL_StringToVector( size, pkvd->szValue, ' ' );

		if ( size != g_vecZero )
			UTIL_SetSize( pev, pev->mins, size );

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "buoyancy") )
	{
		pev->skin = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBreakable::KeyValue( pkvd );
}

void CPushable :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator || !pActivator->IsPlayer() )
	{
		if ( pev->spawnflags & SF_PUSH_BREAKABLE )
			CBreakable::Use( pActivator, pCaller, useType, value );
		return;
	}

	entvars_t *pevActivator = pActivator->pev;

	if ( !pevActivator->groundentity || VARS( pevActivator->groundentity ) == pev )
		return;

	if ( ( pev->spawnflags & SF_PUSH_HOVER ) && !( pevActivator->button & IN_FORWARD )
	     && gpGlobals->time > m_flPushTime && pevActivator->velocity.Length() <= 0 )
	{
		m_flHoverTime = gpGlobals->time + 0.2f;
		UTIL_MakeVectors( pevActivator->v_angle );
		m_vecHoverPosition = pevActivator->origin + Vector( 0, 0, 28 ) + gpGlobals->v_forward * 64;

		if ( m_pfnThink == static_cast<void ( CBaseEntity::* )( void )>( &CPushable::HoverThink ) )
			return;

		SetThink( &CPushable::HoverThink );
		pev->nextthink = pev->ltime + 0.1f;
		return;
	}

	if ( ( pevActivator->button & IN_FORWARD ) && gpGlobals->time > m_flPushTime )
		ReceivePush( pActivator );
	else
		ReceivePull( pActivator );
}

void CPushable :: Touch( CBaseEntity *pOther )
{
	if ( !pOther || FClassnameIs( pOther->pev, "worldspawn" ) )
		return;

	int classification = pOther->Classify();

	if ( classification == CLASS_INSECT || classification == CLASS_ALIEN_BIOWEAPON
	     || classification == CLASS_PLAYER_BIOWEAPON )
		return;

	if ( FClassnameIs( pOther->pev, "monster_roach" ) || FClassnameIs( pOther->pev, "monster_snark" ) )
		return;

	ReceivePush( pOther );
}

void CPushable::ReceivePush( CBaseEntity *pOther )
{
	if ( !pOther )
		return;

	entvars_t *pevToucher = pOther->pev;

	if ( FBitSet( pevToucher->flags, FL_ONGROUND ) && pevToucher->groundentity
	     && VARS( pevToucher->groundentity ) == pev )
	{
		if ( pev->waterlevel > 0 )
			pev->velocity.z += pevToucher->velocity.z * 0.1f;

		return;
	}

	if ( pOther->IsPlayer() )
	{
		if ( !( pevToucher->button & ( IN_FORWARD | IN_USE ) ) )
			return;

		if ( !( pevToucher->flags & FL_ONGROUND ) && pev->waterlevel < 1 )
			return;
	}

	Vector velocity = pevToucher->velocity;
	float speed = velocity.Length2D();

	if ( speed > MaxSpeed() )
	{
		velocity.x *= MaxSpeed() / speed;
		velocity.y *= MaxSpeed() / speed;
	}

	if ( ( pev->spawnflags & SF_PUSH_HOVER ) && pOther->IsPlayer()
	     && ( pevToucher->button & ( IN_FORWARD | IN_USE ) )
	     && gpGlobals->time > m_flPushTime && m_flHoverTime >= gpGlobals->time
	     && pev->gravity != 1 )
	{
		UTIL_MakeVectors( pevToucher->v_angle );
		velocity = gpGlobals->v_forward * 256;
		m_flPushTime = gpGlobals->time + 1.5f;
	}

	Move( velocity );
}

void CPushable::ReceivePull( CBaseEntity *pOther )
{
	if ( !pOther )
		return;

	entvars_t *pevToucher = pOther->pev;

	if ( pOther->IsPlayer() )
	{
		if ( !( pevToucher->button & ( IN_BACK | IN_MOVELEFT | IN_MOVERIGHT ) ) )
			return;

		if ( !( pevToucher->flags & FL_ONGROUND ) && pev->waterlevel < 1 )
			return;
	}

	Vector velocity = pevToucher->velocity;
	Vector distance = pOther->Center() - Center();

	if ( distance.Length2D() > 48 )
		velocity = velocity * 2;

	Move( velocity );

	float speed = pevToucher->velocity.Length2D();

	if ( speed > MaxSpeed() )
	{
		pevToucher->velocity.x *= MaxSpeed() / speed;
		pevToucher->velocity.y *= MaxSpeed() / speed;
	}
}

void CPushable :: Move( const Vector &velocity )
	{
		// Only push if floating
		if ( velocity.Length() <= 0 )
		return;

	pev->velocity.x = velocity.x;
	pev->velocity.y = velocity.y;

	float length = pev->velocity.Length2D();
	if (length > MaxSpeed() )
	{
		pev->velocity.x *= MaxSpeed() / length;
		pev->velocity.y *= MaxSpeed() / length;
	}

	PlaySound();
	}

void CPushable::PlaySound( void )
{
	if ( gpGlobals->time - m_soundTime <= 0.7f )
		return;

	m_soundTime = gpGlobals->time;

	if ( pev->velocity.Length2D() > 0 && FBitSet( pev->flags, FL_ONGROUND ) )
	{
		m_lastSound = RANDOM_LONG( 0, 2 );
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, m_soundNames[m_lastSound], 0.5f, ATTN_NORM, 0, PITCH_NORM, 0, false, g_vecZero );
	}
	else
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, m_soundNames[m_lastSound], 0, 0, SND_STOP, PITCH_NORM, 0, false, g_vecZero );
}

void CPushable::HoverThink( void )
{
	Vector direction = m_vecHoverPosition - Center();
	float distance = direction.Length();

	if ( distance > 256 || gpGlobals->time > m_flHoverTime || m_flPushTime > gpGlobals->time )
	{
		m_vecHoverPosition = g_vecZero;
		pev->gravity = 1;
		SetThink( NULL );
		return;
	}

	float speed = distance * distance;

	if ( distance <= 8 )
		speed = 0;

	pev->velocity = direction.Normalize() * speed;
	pev->gravity = 0.0001f;
	pev->nextthink = pev->ltime + 0.1f;
}

int CPushable::TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	if ( CheckAttacker( BreakableAttackerEntity( pevAttacker ) ) )
		return 0;

	if ( pev->spawnflags & SF_PUSH_BREAKABLE )
	{
		if ( ( pev->spawnflags & SF_BREAK_EXPLOSIVES_ONLY )
		     && ( !( bitsDamageType & DMG_BLAST ) || FClassnameIs( pevInflictor, "weapon_egon" ) ) )
			flDamage = 0;

		return CBreakable::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
	}

	return 1;
}
