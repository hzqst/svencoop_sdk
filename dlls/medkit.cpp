#include "extdll.h"

#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "weapons.h"
#include "medkit.h"
#include "playerammo.h"
#include "CClassicMode.h"
#include "server_support.h"
#include "game.h"

enum medkit_e
{
	MEDKIT_IDLE = 0,
	MEDKIT_LONGIDLE,
	MEDKIT_LONGUSE,
	MEDKIT_SHORTUSE,
	MEDKIT_HOLSTER,
	MEDKIT_DRAW
};

#define MEDKIT_USE_VOLUME 128

class CMedkitAmmo : public CPlayerAmmo
{
public:
	CMedkitAmmo() : CPlayerAmmo( 10, "health", 100 ) {}
	const char *GetDefaultModel( void );
	float GetFadeDelay( void );
};

LINK_ENTITY_TO_CLASS( weapon_medkit, CMedkit );
LINK_ENTITY_TO_CLASS( ammo_medkit, CMedkitAmmo );

void CMedkit::Spawn( void )
{
	m_iId = WEAPON_MEDKIT;
	Precache();

	const char *model = CClassicMode::GetInstance()->IsEnabled()
		? "models/hlclassic/w_pmedkit.mdl" : "models/w_pmedkit.mdl";

	SetModel_Intercept( edict(), GetW_Model( model ) );
	m_iClip = WEAPON_NOCLIP;
	m_iDefaultAmmo = 50;
	CBasePhysicsObject::Spawn();
}

void CMedkit::Precache( void )
{
	PrecacheCustomModels();

	bool classic = CClassicMode::GetInstance()->IsEnabled();

	PrecacheModel_Intercept( this, classic ? "models/hlclassic/v_medkit.mdl" : "models/v_medkit.mdl" );
	PrecacheModel_Intercept( this, classic ? "models/hlclassic/w_pmedkit.mdl" : "models/w_pmedkit.mdl" );
	PrecacheModel_Intercept( this, classic ? "models/hlclassic/p_medkit.mdl" : "models/p_medkit.mdl" );
	PrecacheModel_Intercept( this, "models/v_medkit.mdl" );
	PrecacheModel_Intercept( this, "models/w_pmedkit.mdl" );
	PrecacheModel_Intercept( this, "models/p_medkit.mdl" );
	PrecacheSound_Intercept( this, "items/medshot5.wav" );
	PrecacheSound_Intercept( this, "weapons/electro4.wav" );
	PrecacheSound_Intercept( this, "items/medshotno1.wav" );
	PrecacheSound_Intercept( this, "items/suitchargeok1.wav" );
	PrecacheSound_Intercept( this, "items/suitchargeno1.wav" );
}

bool CMedkit::GetItemInfo( ItemInfo *info )
{
	info->pszName = STRING( pev->classname );
	info->pszAmmo1 = "health";
	info->iMaxAmmo1 = 100;
	info->pszAmmo1Entity = "ammo_medkit";
	info->iAmmo1Drop = 10;
	info->pszAmmo2 = NULL;
	info->iMaxAmmo2 = -1;
	info->pszAmmo2Entity = NULL;
	info->iAmmo2Drop = -1;
	info->iMaxClip = WEAPON_NOCLIP;
	info->iSlot = 0;
	info->iPosition = 2;
	info->iId = WEAPON_MEDKIT;
	info->iFlags = ITEM_FLAG_ESSENTIAL;
	info->iWeight = -5;
	return true;
}

bool CMedkit::AddToPlayer( CBasePlayer *player )
{
	if ( !CBasePlayerWeapon::AddToPlayer( player ) )
		return false;

	AddToPlayerMessage();
	return true;
}

bool CMedkit::Deploy( void )
{
	pev->body = 0;

	bool classic = CClassicMode::GetInstance()->IsEnabled();
	const char *playerModel = GetP_Model( classic ? "models/hlclassic/p_medkit.mdl" : "models/p_medkit.mdl" );
	const char *viewModel = GetV_Model( classic ? "models/hlclassic/v_medkit.mdl" : "models/v_medkit.mdl" );

	return DefaultDeploy( viewModel, playerModel, MEDKIT_DRAW, "trip", 0, 0 );
}

void CMedkit::Holster( int skiplocal )
{
	m_hPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	m_hPlayer->pev->viewmodel = 0;
}

void CMedkit::WeaponIdle( void )
{
	m_iReviveState = 0;
	pev->body = 0;

	if ( m_flTimeWeaponIdle > gpGlobals->time )
		return;

	float random = RANDOM_FLOAT( 0, 1 );

	m_flTimeWeaponIdle = gpGlobals->time + 2.0f;
	SendWeaponAnim( random <= 0.5 ? MEDKIT_IDLE : MEDKIT_LONGIDLE, 0, 0 );
}

void CMedkit::PrimaryAttack( void )
{
	float healAmount = V_max( 0.0f, sk_plr_HpMedic.value );

	if ( healAmount <= 0.0f )
		return;

	TraceResult trace = {};

	trace.vecEndPos = Vector( 0, 0, 0 );
	trace.vecPlaneNormal = Vector( 0, 0, 0 );
	UTIL_MakeVectors( m_hPlayer->pev->v_angle );

	Vector source = m_hPlayer->GetGunPosition();
	Vector end = source + gpGlobals->v_forward * 32.0f;

	UTIL_TraceLine( source, end, dont_ignore_monsters, m_hPlayer->edict(), &trace );

	if ( trace.flFraction >= 1.0f )
		UTIL_TraceHull( source, end, dont_ignore_monsters, head_hull, m_hPlayer->edict(), &trace );

	m_flNextPrimaryAttack = gpGlobals->time + 0.2;

	if ( trace.flFraction >= 1.0f )
	{
		if ( gpGlobals->time > m_flTimeWeaponIdle )
		{
			SendWeaponAnim( MEDKIT_IDLE, 0, 0 );
			m_flTimeWeaponIdle = gpGlobals->time + 2.0f;
		}

		return;
	}

	edict_t *hit = trace.pHit;

	if ( !hit )
		hit = INDEXENT( 0 );

	if ( !hit || hit->free || !hit->pvPrivateData )
		return;

	CBaseEntity *target = (CBaseEntity *) hit->pvPrivateData;
	bool provoked = target->IsMonster()
		&& ( target->MyMonsterPointer()->m_afMemory & bits_MEMORY_PROVOKED );

	if ( !( !target->IsMachine() && !target->IsBSPModel()
		&& m_hPlayer->IRelationship( target, true ) < R_DL ) && !provoked )
		return;

	if ( !target->IsAlive()
		|| !( target->pev->max_health > target->pev->health || provoked ) )
		return;

	healAmount = V_min( target->pev->max_health - target->pev->health, healAmount );
	healAmount = V_min( healAmount, (float) m_hPlayer->m_rgAmmo[m_iPrimaryAmmoType] );

	ClearMultiDamage();
	UseAmmo( (int) healAmount );
	target->TakeHealth( healAmount, DMG_MEDKITHEAL, 0 );

	if ( m_hPlayer->pev->max_health > m_hPlayer->pev->health )
	{
		m_hPlayer->TakeHealth( V_min( m_hPlayer->pev->max_health - m_hPlayer->pev->health,
			(float) (int) healAmount * 0.25f ), DMG_MEDKITHEAL, 0 );
	}

	if ( CVAR_GET_FLOAT( "mp_disable_medkit_points" ) == 0.0f && healAmount > 0.0f )
		m_hPlayer->pev->frags += healAmount * 0.1f;

	if ( provoked )
	{
		for ( int i = 0; i < MAX_OLD_ENEMIES; i++ )
		{
			edict_t *enemy = target->MyMonsterPointer()->m_hOldEnemy[i].Get();

			if ( enemy && !enemy->free && enemy->pvPrivateData )
				target->MyMonsterPointer()->m_hOldEnemy[i] = NULL;
		}

		target->MyMonsterPointer()->m_afMemory &= ~( bits_MEMORY_PROVOKED | bits_MEMORY_SUSPICIOUS );
		target->MyMonsterPointer()->m_hEnemy = NULL;
	}

	EMIT_SOUND_DYN( m_hPlayer->edict(), CHAN_WEAPON, "items/medshot5.wav", 1.0f, ATTN_NORM, 0, 100 );
	SendWeaponAnim( MEDKIT_SHORTUSE, 0, 0 );
	m_hPlayer->SetAnimation( PLAYER_ATTACK1, 0 );
	m_hPlayer->m_iWeaponVolume = MEDKIT_USE_VOLUME;
	m_flTimeWeaponIdle = gpGlobals->time + 1.0f;
	m_flNextPrimaryAttack = gpGlobals->time + 0.5;
}

void CMedkit::SecondaryAttack( void )
{
	if ( m_hPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 50 )
	{
		EMIT_SOUND_DYN( m_hPlayer->edict(), CHAN_WEAPON, "items/suitchargeno1.wav", 1.0f, ATTN_NORM, 0, 110 );
		m_flNextSecondaryAttack = gpGlobals->time + 1.0f;

		if ( gpGlobals->time > m_flTimeWeaponIdle )
		{
			pev->body = 0;
			SendWeaponAnim( MEDKIT_IDLE, 0, 0 );
			m_flTimeWeaponIdle = gpGlobals->time + 2.0f;
		}

		return;
	}

	if ( m_iReviveState )
	{
		if ( gpGlobals->time > m_flReviveTime )
			ReviveEntity();

		return;
	}

	bool clearAim = false;
	CBaseEntity *target = FindTargetToRevive( &clearAim );

	if ( clearAim && target )
	{
		target->BeginRevive( 2.0f );

		if ( gpGlobals->time > m_flReviveTime )
			EMIT_SOUND_DYN( m_hPlayer->edict(), CHAN_WEAPON, "items/suitchargeok1.wav", 1.0f, ATTN_NORM, 0, 110 );

		pev->body = 1;
		SendWeaponAnim( MEDKIT_LONGUSE, 0, 1 );
		m_iReviveState = 1;
		m_flReviveTime = gpGlobals->time + 2.0f;
		m_flTimeWeaponIdle = gpGlobals->time + 2.0f;
	}

	if ( m_iReviveState != 1 )
	{
		EMIT_SOUND_DYN( m_hPlayer->edict(), CHAN_WEAPON, "items/medshotno1.wav", 1.0f, ATTN_NORM, 0, 110 );
		m_flNextSecondaryAttack = gpGlobals->time + 0.6;

		if ( gpGlobals->time > m_flTimeWeaponIdle )
		{
			pev->body = 0;
			SendWeaponAnim( MEDKIT_IDLE, 0, 0 );
			m_flTimeWeaponIdle = gpGlobals->time + 2.0f;
		}
	}
}

void CMedkit::ReviveEntity( void )
{
	pev->body = 0;
	m_hPlayer->SetAnimation( PLAYER_ATTACK1, 0 );
	m_flTimeWeaponIdle = gpGlobals->time + 1.0f;

	bool clearAim = false;
	CBaseEntity *target = FindTargetToRevive( &clearAim );

	if ( !clearAim || !target )
	{
		m_flNextSecondaryAttack = gpGlobals->time + 0.5;
		return;
	}

	target->EndRevive( 0.5f );
	UseAmmo( 50 );
	EMIT_SOUND_DYN( m_hPlayer->edict(), CHAN_WEAPON, "items/medshot5.wav", 1.0f, ATTN_NORM, 0, 100 );
	EMIT_SOUND_DYN( m_hPlayer->edict(), CHAN_ITEM, "weapons/electro4.wav", 1.0f, ATTN_NORM, 0, 110 );
	SendWeaponAnim( MEDKIT_SHORTUSE, 0, 0 );

	if ( CVAR_GET_FLOAT( "mp_disable_medkit_points" ) == 0.0f && target->IsPlayer() )
		m_hPlayer->pev->frags += 5.0f;

	m_hPlayer->m_iWeaponVolume = MEDKIT_USE_VOLUME;
	m_iReviveState = 0;
	m_flNextSecondaryAttack = gpGlobals->time + 2.0f;
}

CBaseEntity *CMedkit::FindTargetToRevive( bool *clearAim )
{
	TraceResult trace = {};

	trace.vecEndPos = Vector( 0, 0, 0 );
	trace.vecPlaneNormal = Vector( 0, 0, 0 );
	UTIL_MakeVectors( m_hPlayer->pev->v_angle );

	Vector source = m_hPlayer->GetGunPosition();
	Vector end = source + gpGlobals->v_forward * 16.0f;

	UTIL_TraceLine( source, end, dont_ignore_monsters, m_hPlayer->edict(), &trace );

	if ( trace.flFraction >= 1.0f )
	{
		UTIL_TraceHull( source, end, dont_ignore_monsters, head_hull, m_hPlayer->edict(), &trace );

		if ( trace.flFraction < 1.0f )
		{
			edict_t *hit = trace.pHit;

			if ( !hit )
				hit = INDEXENT( 0 );

			CBaseEntity *entity = hit && !hit->free ? (CBaseEntity *) hit->pvPrivateData : NULL;

			if ( !entity || entity->IsBSPModel() )
			{
				UTIL_FindHullIntersection( source, trace, Vector( -16, -16, -18 ),
					Vector( 16, 16, 18 ), m_hPlayer->edict(), 1000000.0f );
			}

			end = trace.vecEndPos;
		}
	}

	bool canSearch = trace.flFraction >= 1.0f;

	if ( !canSearch )
	{
		edict_t *hit = trace.pHit;

		if ( !hit )
			hit = INDEXENT( 0 );

		canSearch = hit && !hit->free && hit->pvPrivateData;
	}

	if ( clearAim )
		*clearAim = canSearch;

	if ( !canSearch )
		return NULL;

	CBaseEntity *target = NULL;

	while ( ( target = UTIL_FindEntityInSphere( target, end, 64.0f, NULL, "targetname" ) ) != NULL )
	{
		if ( m_hPlayer->IRelationship( target, true ) <= R_NO && target->IsRevivable() )
			return target;
	}

	return NULL;
}

int CMedkit::UseAmmo( int amount )
{
	int ammoType = m_iPrimaryAmmoType;

	if ( amount >= m_hPlayer->m_rgAmmo[ammoType] )
		amount = m_hPlayer->m_rgAmmo[ammoType];

	m_hPlayer->m_rgAmmo[ammoType] -= amount;

	if ( m_hPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 0 )
		m_hPlayer->m_rgAmmo[m_iPrimaryAmmoType] = 0;

	return amount;
}

void CMedkit::ItemPostFrame( void )
{
	CBasePlayerWeapon::ItemPostFrame();

	if ( !m_hPlayer || !m_hPlayer->HasPlayerItem( this ) )
	{
		m_flRechargeProgress = 0;
		return;
	}

	UpdateAmmo();
}

void CMedkit::InactiveItemPostFrame( void )
{
	UpdateAmmo();
}

void CMedkit::UpdateAmmo( void )
{
	float rate = m_hPlayer && (CBasePlayerItem *) m_hPlayer->m_pActiveItem == this ? 0.5f : 0.2f;

	m_flRechargeProgress += rate * gpGlobals->frametime;

	if ( m_flRechargeProgress >= 1.5f )
	{
		if ( m_hPlayer )
			m_hPlayer->GiveAmmo( 5, "health", 100, true, NULL );

		m_flRechargeProgress = 0;
	}
}

const char *CMedkitAmmo::GetDefaultModel( void )
{
	return CClassicMode::GetInstance()->IsEnabled()
		? "models/hlclassic/w_pmedkit.mdl" : "models/w_pmedkit.mdl";
}

float CMedkitAmmo::GetFadeDelay( void )
{
	return CBasePickupObject::GetFadeDelay() / 3.0f;
}
