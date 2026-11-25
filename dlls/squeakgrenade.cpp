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
#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "CClassicMode.h"
#include "game.h"
#include "soundent.h"
#include "gamerules.h"
#include "squeakgrenade.h"

enum w_squeak_e {
	WSQUEAK_IDLE1 = 0,
	WSQUEAK_FIDGET,
	WSQUEAK_JUMP,
	WSQUEAK_RUN,
};

enum squeak_e {
	SQUEAK_IDLE1 = 0,
	SQUEAK_FIDGETFIT,
	SQUEAK_FIDGETNIP,
	SQUEAK_DOWN,
	SQUEAK_UP,
	SQUEAK_THROW
};

#ifndef CLIENT_DLL

float CSqueakGrenade::m_flNextBounceSoundTime = 0;

LINK_ENTITY_TO_CLASS( monster_snark, CSqueakGrenade );
TYPEDESCRIPTION	CSqueakGrenade::m_SaveData[] = 
{
	DEFINE_FIELD( CSqueakGrenade, m_flDie, FIELD_TIME ),
	DEFINE_FIELD( CSqueakGrenade, m_vecTarget, FIELD_VECTOR ),
	DEFINE_FIELD( CSqueakGrenade, m_flNextHunt, FIELD_TIME ),
	DEFINE_FIELD( CSqueakGrenade, m_flNextHit, FIELD_TIME ),
	DEFINE_FIELD( CSqueakGrenade, m_posPrev, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CSqueakGrenade, m_hOwner, FIELD_EHANDLE ),
};

IMPLEMENT_SAVERESTORE( CSqueakGrenade, CGrenade );

#define SQUEEK_DETONATE_DELAY	15.0

int CSqueakGrenade :: Classify ( void )
{
	return GetClassification( 5 );
}

extern cvar_t sk_snark_dmg_bite;
extern cvar_t sk_snark_dmg_pop;
extern cvar_t sk_snark_health;

void CSqueakGrenade :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	if ( !SetupModel() )
	{
		if ( CClassicMode::GetInstance()->IsEnabled() )
			SET_MODEL( ENT(pev), "models/hlclassic/w_squeak.mdl" );
		else
			SET_MODEL(ENT(pev), "models/w_squeak.mdl");
	}
	UTIL_SetSize(pev, Vector( -4, -4, 0), Vector(4, 4, 8));
	UTIL_SetOrigin( pev, pev->origin );

	SetTouch( &CSqueakGrenade::SuperBounceTouch );
	SetThink( &CSqueakGrenade::HuntThink );
	pev->nextthink = gpGlobals->time + 0.1;
	m_flNextHunt = gpGlobals->time + 1E6;

	pev->flags |= FL_MONSTER;
	pev->takedamage		= DAMAGE_AIM;
	pev->health			= sk_snark_health.value;
	pev->gravity		= 0.5;
	pev->friction		= 0.5;

	pev->dmg = sk_snark_dmg_pop.value;

	m_flDie = gpGlobals->time + SQUEEK_DETONATE_DELAY;

	m_flFieldOfView = 0; // 180 degrees

	if ( pev->owner )
		m_hOwner = Instance( pev->owner );

	m_flNextBounceSoundTime = gpGlobals->time;// reset each time a snark is spawned.

	pev->sequence = WSQUEAK_RUN;
	ResetSequenceInfo( );
	m_bNestAlly = false;
}

void CSqueakGrenade::Precache( void )
{
	CBaseEntity::Precache();

	if ( CClassicMode::GetInstance()->IsEnabled() )
		PRECACHE_MODEL( "models/hlclassic/w_squeak.mdl" );
	else
		PRECACHE_MODEL( "models/w_squeak.mdl" );
	PRECACHE_SOUND("squeek/sqk_blast1.wav");
	PRECACHE_SOUND("common/bodysplat.wav");
	PRECACHE_SOUND("squeek/sqk_die1.wav");
	PRECACHE_SOUND("squeek/sqk_hunt1.wav");
	PRECACHE_SOUND("squeek/sqk_hunt2.wav");
	PRECACHE_SOUND("squeek/sqk_hunt3.wav");
	PRECACHE_SOUND("squeek/sqk_deploy1.wav");
}


void CSqueakGrenade :: Killed( entvars_t *pevAttacker, int iGib )
{
	pev->model = iStringNull;// make invisible
	SetThink( &CSqueakGrenade::SUB_Remove );
	SetTouch( NULL );
	pev->nextthink = gpGlobals->time + 0.1;

	// since squeak grenades never leave a body behind, clear out their takedamage now.
	// Squeaks do a bit of radius damage when they pop, and that radius damage will
	// continue to call this function unless we acknowledge the Squeak's death now. (sjb)
	pev->takedamage = DAMAGE_NO;

	// play squeek blast
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "squeek/sqk_blast1.wav", 1, 0.5, 0, PITCH_NORM);	

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, SMALL_EXPLOSION_VOLUME, 3.0 );

	UTIL_BloodDrips( pev->origin, g_vecZero, BloodColor(), 80 );

	if (m_hOwner != NULL)
		RadiusDamage ( pev, m_hOwner->pev, pev->dmg, CLASS_NONE, DMG_BLAST );
	else
		RadiusDamage ( pev, pev, pev->dmg, CLASS_NONE, DMG_BLAST );

	// reset owner so death message happens
	if (m_hOwner != NULL)
		pev->owner = m_hOwner->edict();

	CBaseMonster :: Killed( pevAttacker, GIB_ALWAYS );
}

void CSqueakGrenade :: GibMonster( void )
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "common/bodysplat.wav", 0.75, ATTN_NORM, 0, 200);		
}



void CSqueakGrenade::HuntThink( void )
{
	// ALERT( at_console, "think\n" );

	if (!IsInWorld())
	{
		SetTouch( NULL );
		UTIL_Remove( this );
		return;
	}
	
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	// explode when ready
	if (gpGlobals->time >= m_flDie)
	{
		g_vecAttackDir = pev->velocity.Normalize( );
		pev->health = -1;
		Killed( pev, 0 );
		return;
	}

	// float
	if (pev->waterlevel != 0)
	{
		if (pev->movetype == MOVETYPE_BOUNCE)
		{
			pev->movetype = MOVETYPE_FLY;
		}
		pev->velocity = pev->velocity * 0.9;
		pev->velocity.z += 8.0;
	}
	else if (pev->movetype = MOVETYPE_FLY)
	{
		pev->movetype = MOVETYPE_BOUNCE;
	}

	// return if not time to hunt
	if (m_flNextHunt > gpGlobals->time)
		return;

	m_flNextHunt = gpGlobals->time + 2.0;
	
	CBaseEntity *pOther = NULL;
	Vector vecDir;
	TraceResult tr;

	Vector vecFlat = pev->velocity;
	vecFlat.z = 0;
	vecFlat = vecFlat.Normalize( );

	UTIL_MakeVectors( pev->angles );

	if (m_hEnemy == NULL || !m_hEnemy->IsAlive())
	{
		// find target, bounce a bit towards it.
		Look( 512 );
		m_hEnemy = BestVisibleEnemy( );
	}

	// squeek if it's about time blow up
	if ((m_flDie - gpGlobals->time <= 0.5) && (m_flDie - gpGlobals->time >= 0.3))
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_die1.wav", 1, ATTN_NORM, 0, 100 + RANDOM_LONG(0,0x3F));
		CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, 256, 0.25 );
	}

	// higher pitch as squeeker gets closer to detonation time
	float flpitch = 155.0 - 60.0 * ((m_flDie - gpGlobals->time) / SQUEEK_DETONATE_DELAY);
	if (flpitch < 80)
		flpitch = 80;

	if (m_hEnemy != NULL && ( !m_bNestAlly || !m_hEnemy->IsPlayer() ) )
	{
		if (FVisible( m_hEnemy ))
		{
			vecDir = m_hEnemy->EyePosition() - pev->origin;
			m_vecTarget = vecDir.Normalize( );
		}

		float flVel = pev->velocity.Length();
		float flAdj = 50.0 / (flVel + 10.0);

		if (flAdj > 1.2)
			flAdj = 1.2;
		
		// ALERT( at_console, "think : enemy\n");

		// ALERT( at_console, "%.0f %.2f %.2f %.2f\n", flVel, m_vecTarget.x, m_vecTarget.y, m_vecTarget.z );

		pev->velocity = pev->velocity * flAdj + m_vecTarget * 300;
	}

	if (pev->flags & FL_ONGROUND)
	{
		pev->avelocity = Vector( 0, 0, 0 );
	}
	else
	{
		if (pev->avelocity == Vector( 0, 0, 0))
		{
			pev->avelocity.x = RANDOM_FLOAT( -100, 100 );
			pev->avelocity.z = RANDOM_FLOAT( -100, 100 );
		}
	}

	if ((pev->origin - m_posPrev).Length() < 1.0)
	{
		pev->velocity.x = RANDOM_FLOAT( -100, 100 );
		pev->velocity.y = RANDOM_FLOAT( -100, 100 );
	}
	m_posPrev = pev->origin;

	pev->angles = UTIL_VecToAngles( pev->velocity );
	pev->angles.z = 0;
	pev->angles.x = 0;
}


void CSqueakGrenade::SuperBounceTouch( CBaseEntity *pOther )
{
	float	flpitch;

	TraceResult tr = UTIL_GetGlobalTrace( );

	// don't hit the guy that launched this grenade
	if ( pev->owner && pOther->edict() == pev->owner )
		return;

	// at least until we've bounced once
	pev->owner = NULL;

	pev->angles.x = 0;
	pev->angles.z = 0;

	// avoid bouncing too much
	if (m_flNextHit > gpGlobals->time)
		return;

	// higher pitch as squeeker gets closer to detonation time
	flpitch = 155.0 - 60.0 * ((m_flDie - gpGlobals->time) / SQUEEK_DETONATE_DELAY);

	if ( pOther->pev->takedamage && m_flNextAttack < gpGlobals->time )
	{
		// attack!

		// make sure it's me who has touched them
		if (tr.pHit == pOther->edict())
		{
			// and it's not another squeakgrenade
			if (tr.pHit->v.modelindex != pev->modelindex)
			{
				// ALERT( at_console, "hit enemy\n");
				ClearMultiDamage( );
				pOther->TraceAttack(pev, sk_snark_dmg_bite.value, gpGlobals->v_forward, &tr, DMG_SLASH );
				if (m_hOwner != NULL)
					ApplyMultiDamage( pev, m_hOwner->pev );
				else
					ApplyMultiDamage( pev, pev );

				pev->dmg += sk_snark_dmg_pop.value; // add more explosion damage
				// m_flDie += 2.0; // add more life

				// make bite sound
				EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "squeek/sqk_deploy1.wav", 1.0, ATTN_NORM, 0, (int)flpitch);
				m_flNextAttack = gpGlobals->time + 0.5;
			}
		}
		else
		{
			// ALERT( at_console, "been hit\n");
		}
	}

	m_flNextHit = gpGlobals->time + 0.1;
	m_flNextHunt = gpGlobals->time;

	if ( g_pGameRules->IsMultiplayer() )
	{
		// in multiplayer, we limit how often snarks can make their bounce sounds to prevent overflows.
		if ( gpGlobals->time < m_flNextBounceSoundTime )
		{
			// too soon!
			return;
		}
	}

	if (!(pev->flags & FL_ONGROUND))
	{
		// play bounce sound
		float flRndSound = RANDOM_FLOAT ( 0 , 1 );

		if ( flRndSound <= 0.33 )
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt1.wav", 1, ATTN_NORM, 0, (int)flpitch);		
		else if (flRndSound <= 0.66)
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt2.wav", 1, ATTN_NORM, 0, (int)flpitch);
		else 
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, (int)flpitch);
		CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, 256, 0.25 );
	}
	else
	{
		// skittering sound
		CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, 100, 0.1 );
	}

	m_flNextBounceSoundTime = gpGlobals->time + 0.5;// half second.
}

#endif

LINK_ENTITY_TO_CLASS( weapon_snark, CSqueak );

void CSqueak::Spawn( void )
{
	Precache();

	const char *model;

	if ( CClassicMode::GetInstance()->IsEnabled() )
		model = "models/hlclassic/w_sqknest.mdl";
	else
		model = "models/w_sqknest.mdl";

	SetModel_Intercept( edict(), GetW_Model( model ) );
	CBasePhysicsObject::Spawn( );
	m_iId = WEAPON_SNARK;
	m_iDefaultAmmo = SNARK_DEFAULT_GIVE;
	m_flCustomDmg = pev->dmg;

	pev->sequence = 1;
	pev->animtime = gpGlobals->time;
	pev->framerate = 1.0;
}

void CSqueak::OnCreate( void )
{
	CBasePlayerWeapon::OnCreate();
	m_iBounceMaterial = 2;
}

void CSqueak::Precache( void )
{
	PrecacheCustomModels();

	bool classic = CClassicMode::GetInstance()->IsEnabled();

	UTIL_PrecacheModel( this, classic ? "models/hlclassic/v_squeak.mdl" : "models/v_squeak.mdl" );
	UTIL_PrecacheModel( this, classic ? "models/hlclassic/p_squeak.mdl" : "models/p_squeak.mdl" );
	UTIL_PrecacheModel( this, classic ? "models/hlclassic/w_sqknest.mdl" : "models/w_sqknest.mdl");
	PrecacheSound_Intercept( this, "squeek/sqk_hunt2.wav");
	PrecacheSound_Intercept( this, "squeek/sqk_hunt3.wav");
	UTIL_PrecacheOther("monster_snark");
	UTIL_PrecacheOther( "monster_chumtoad" );
}

bool CSqueak::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Snarks";
	p->iMaxAmmo1 = SNARK_MAX_CARRY;
	p->pszAmmo1Entity = "weapon_snark";
	p->iAmmo1Drop = 1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->pszAmmo2Entity = NULL;
	p->iAmmo2Drop = AMMO_DROP_NONE;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 3;
	p->iId = m_iId = WEAPON_SNARK;
	p->iWeight = SNARK_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE | ITEM_FLAG_NOAUTORELOAD;

	return true;
}

bool CSqueak::Deploy( void )
{
	const char *sample;

	if ( RANDOM_FLOAT ( 0 , 1 ) <= 0.5f )
		sample = "squeek/sqk_hunt2.wav";
	else
		sample = "squeek/sqk_hunt3.wav";

	EMIT_SOUND_DYN( m_hPlayer->edict(), CHAN_WEAPON, sample, 1.0f, ATTN_NORM, 0, 100,
	                m_hPlayer->entindex(), false, g_vecZero );

	m_hPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

	bool classic = CClassicMode::GetInstance()->IsEnabled();

	const char *playerModel = GetP_Model( classic ? "models/hlclassic/p_squeak.mdl" : "models/p_squeak.mdl" );
	const char *viewModel = GetV_Model( classic ? "models/hlclassic/v_squeak.mdl" : "models/v_squeak.mdl" );

	return DefaultDeploy( viewModel, playerModel, SQUEAK_UP, "squeak" );
}

void CSqueak::Holster( int skiplocal /* = 0 */ )
{
	m_fInReload = false;

	m_hPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

	if ( !m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
	{
		SetThink( &CSqueak::DestroyItem );
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	EMIT_SOUND_DYN(m_hPlayer->edict(), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM,
	                0, 100, 0, false, g_vecZero );
	m_hPlayer->pev->viewmodel = 0;
}

void CSqueak::PrimaryAttack( void )
{
	if ( m_hPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] <= 0 )
		return;

	UTIL_MakeVectors( m_hPlayer->pev->v_angle );

	Vector trace_origin = m_hPlayer->pev->origin;
	TraceResult tr;
		if ( m_hPlayer->pev->flags & FL_DUCKING )
		trace_origin = trace_origin - ( VEC_HULL_MIN - VEC_DUCK_HULL_MIN );

	UTIL_TraceLine( trace_origin + gpGlobals->v_forward * 20, trace_origin + gpGlobals->v_forward * 64, dont_ignore_monsters, NULL, &tr );

	if ( tr.fAllSolid || tr.fStartSolid || !( tr.flFraction > 0.25f ) )
		return;

	SendWeaponAnim( SQUEAK_THROW, 0, 0 );
	m_hPlayer->SetAnimation( PLAYER_ATTACK1 );

	CSqueakGrenade *pSqueak = (CSqueakGrenade *) CBaseEntity::Create( "monster_snark", g_vecZero, m_hPlayer->pev->v_angle, false, m_hPlayer->edict() );
			pSqueak->pev->velocity = gpGlobals->v_forward * 200 + m_hPlayer->pev->velocity;
	pSqueak->SetClassification( m_hPlayer->Classify() );
	pSqueak->m_bNestAlly = true;
	pSqueak->pev->spawnflags = 1;

	if ( m_flCustomDmg > 0.0f )
	{
		m_flCustomDmg *= 0.5f;
		pSqueak->pev->dmg = m_flCustomDmg;
	}
	else
	{
		pSqueak->pev->dmg = sk_snark_dmg_pop.value;
	}

	EMIT_SOUND_DYN( m_hPlayer->edict(), CHAN_VOICE,
	                RANDOM_FLOAT ( 0 , 1 ) <= 0.5f ? "squeek/sqk_hunt2.wav" : "squeek/sqk_hunt3.wav",
	                1.0f, ATTN_NORM, 0, 105, 0, false, g_vecZero );
	m_fJustThrown = 1;

			m_hPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	m_flNextPrimaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = gpGlobals->time + 1.0f;

			m_hPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
}

void CSqueak::SecondaryAttack( void )
{
	if ( !CanHaveXtra( XTRA_CHUMTOAD, m_hPlayer ) || m_hPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 10 )
	{
		m_flNextSecondaryAttack = gpGlobals->time + 1.0f;
		return;
	}

	UTIL_MakeVectors( m_hPlayer->pev->v_angle );

	Vector origin = m_hPlayer->pev->origin;
	TraceResult tr;

	if ( m_hPlayer->pev->flags & FL_DUCKING )
		origin = origin - ( VEC_HULL_MIN - VEC_DUCK_HULL_MIN );

	UTIL_TraceLine( origin + gpGlobals->v_forward * 48, origin + gpGlobals->v_forward * 96,
	                dont_ignore_monsters, NULL, &tr );

	if ( tr.fAllSolid || tr.fStartSolid || !( tr.flFraction > 0.75f ) )
		return;

	CBaseEntity *previous = m_hPlayer->m_hChumtoad;

	if ( previous && FClassnameIs( previous->pev, "monster_chumtoad" ) )
	{
		previous->pev->health = -1;
		previous->Killed( m_hPlayer->pev, GIB_ALWAYS );
		m_hPlayer->m_hChumtoad = NULL;
	}

	SendWeaponAnim( SQUEAK_THROW, 0, 0 );
	m_hPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector angles( 0, m_hPlayer->pev->v_angle.y, 0 );
	CBaseEntity *chumtoad = CBaseEntity::Create( "monster_chumtoad", origin + gpGlobals->v_forward * 64,
	                                             angles, true, m_hPlayer->edict() );

	chumtoad->pev->velocity = gpGlobals->v_forward * 200 + m_hPlayer->pev->velocity;
	chumtoad->SetClassification( m_hPlayer->Classify() );
	EMIT_SOUND_DYN( edict(), CHAN_VOICE,
	                RANDOM_FLOAT( 0, 1 ) <= 0.5f ? "squeek/sqk_hunt2.wav" : "squeek/sqk_hunt3.wav",
	                1.0f, ATTN_NORM, 0, 105, 0, false, g_vecZero );
	m_fJustThrown = 1;
	m_hPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	m_flNextPrimaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = gpGlobals->time + 1.0f;
	m_hPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 10;

	if ( CanHaveXtra( XTRA_CHUMTOAD_HEALTH, m_hPlayer ) )
		chumtoad->MyMonsterPointer()->m_iMaxHealth = 333;
	else
		chumtoad->MyMonsterPointer()->m_iMaxHealth = 100;

	m_hPlayer->m_hChumtoad = chumtoad;
	chumtoad->Spawn();
	chumtoad->pev->spawnflags = 1;
	m_flNextSecondaryAttack = gpGlobals->time + 1.0f;
}

void CSqueak::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if (m_fJustThrown)
	{
		m_fJustThrown = 0;

		if ( !m_hPlayer->m_rgAmmo[PrimaryAmmoIndex()] )
		{
			RetireWeapon();
			return;
		}

		SendWeaponAnim( SQUEAK_UP, 0, 0 );
		m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT( 10, 15 );
		return;
	}

	int iAnim;
	float flRand = RANDOM_FLOAT( 0, 1 );
	if (flRand <= 0.75)
	{
		iAnim = SQUEAK_IDLE1;
		m_flTimeWeaponIdle = gpGlobals->time + 30.0 / 16 * (2);
	}
	else if (flRand <= 0.875)
	{
		iAnim = SQUEAK_FIDGETFIT;
		m_flTimeWeaponIdle = gpGlobals->time + 70.0 / 16.0;
	}
	else
	{
		iAnim = SQUEAK_FIDGETNIP;
		m_flTimeWeaponIdle = gpGlobals->time + 80.0 / 16.0;
	}
	SendWeaponAnim( iAnim, 0, 0 );
}

#endif
