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
#include "effects.h"
#include "weapons.h"
#include "explode.h"
#include "CCustomEntity.h"

#include "player.h"
#include "func_tank.h"
#include "ASBaseTank.h"


//			Custom damage
//			env_laser (duration is 0.5 rate of fire)
//			rockets
//			explosion?



TYPEDESCRIPTION	CFuncTank::m_SaveData[] = 
{
	DEFINE_FIELD( CFuncTank, m_yawCenter, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_yawRate, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_yawRange, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_yawTolerance, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_pitchCenter, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_pitchRate, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_pitchRange, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_pitchTolerance, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_fireLast, FIELD_TIME ),
	DEFINE_FIELD( CFuncTank, m_fireRate, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_lastSightTime, FIELD_TIME ),
	DEFINE_FIELD( CFuncTank, m_persist, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_minRange, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_maxRange, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_barrelPos, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncTank, m_spriteScale, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_iszSpriteSmoke, FIELD_STRING ),
	DEFINE_FIELD( CFuncTank, m_iszSpriteFlash, FIELD_STRING ),
	DEFINE_FIELD( CFuncTank, m_bulletType, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncTank, m_iBulletDamage, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncTank, m_sightOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncTank, m_spread, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncTank, m_hController, FIELD_EHANDLE ),
	DEFINE_FIELD( CFuncTank, m_vecControllerUsePos, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncTank, m_flNextAttack, FIELD_TIME ),
	DEFINE_FIELD( CFuncTank, m_iszMaster, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CFuncTank, CBaseEntity );

CBaseEntity *TankHandle( EHANDLE &handle )
{
	edict_t *entity = handle.Get();

	if ( !entity || entity->free || !entity->pvPrivateData )
		return NULL;

	return (CBaseEntity *) entity->pvPrivateData;
}

void TankSetHandle( EHANDLE &handle, CBaseEntity *entity )
{
	handle = NULL;

	if ( !entity || !entity->pev )
		return;

	edict_t *edict = entity->edict();

	if ( edict && !edict->free )
		handle = entity;
}

static Vector gTankSpread[] =
{
	Vector( 0, 0, 0 ),		// perfect
	Vector( 0.025, 0.025, 0.025 ),	// small cone
	Vector( 0.05, 0.05, 0.05 ),  // medium cone
	Vector( 0.1, 0.1, 0.1 ),	// large cone
	Vector( 0.25, 0.25, 0.25 ),	// extra-large cone
};
#define MAX_FIRING_SPREADS ARRAYSIZE(gTankSpread)


void CFuncTank :: Spawn( void )
{
	Precache();
	m_hUseProxy = NULL;
	pev->takedamage = DAMAGE_YES;

	pev->movetype	= MOVETYPE_PUSH;  // so it doesn't get pushed by anything
	pev->solid		= SOLID_BSP;
	UTIL_SetOrigin( pev, pev->origin );
	SET_MODEL( edict(), STRING(pev->model) );
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	pev->v_angle = Vector( pev->angles.x, pev->angles.y, 0 );

	if ( IsActive() )
		pev->nextthink = pev->ltime + 1.0;

	m_sightOrigin = BarrelPosition() - pev->origin;
	pev->view_ofs = m_sightOrigin; // Point at the end of the barrel

	if ( m_fireRate <= 0 )
		m_fireRate = 1;
	if ( (unsigned int) m_spread > MAX_FIRING_SPREADS )
		m_spread = 0;

	pev->oldorigin = pev->origin;
	m_hTarget = NULL;
	m_hMonsterController = NULL;
	m_bMonsterControl = false;

	Vector forward;

	UTIL_MakeVectorsPrivate( pev->angles, forward, NULL, NULL );
	m_flUseRadius = ( ( pev->maxs - pev->mins ) * 0.5f ).Length();
}


void CFuncTank :: Precache( void )
{
	if ( m_iszSpriteSmoke )
		UTIL_PrecacheModel( this, STRING(m_iszSpriteSmoke) );
	if ( m_iszSpriteFlash )
		UTIL_PrecacheModel( this, STRING(m_iszSpriteFlash) );

	if ( pev->noise )
		PrecacheSound_Intercept( this, STRING(pev->noise) );
}


void CFuncTank :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "yawrate"))
	{
		m_yawRate = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "yawrange"))
	{
		m_yawRange = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "yawtolerance"))
	{
		m_yawTolerance = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "pitchrange"))
	{
		m_pitchRange = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "pitchrate"))
	{
		m_pitchRate = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "pitchtolerance"))
	{
		m_pitchTolerance = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firerate"))
	{
		m_fireRate = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "barrel"))
	{
		m_barrelPos.x = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "barrely"))
	{
		m_barrelPos.y = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "barrelz"))
	{
		m_barrelPos.z = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "spritescale"))
	{
		m_spriteScale = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "spritesmoke"))
	{
		m_iszSpriteSmoke = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "spriteflash"))
	{
		m_iszSpriteFlash = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "rotatesound"))
	{
		pev->noise = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "persistence"))
	{
		m_persist = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "bullet"))
	{
		m_bulletType = (TANKBULLET)atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "bullet_damage" )) 
	{
		m_iBulletDamage = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firespread"))
	{
		m_spread = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "minRange"))
	{
		m_minRange = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "maxRange"))
	{
		m_maxRange = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "master"))
	{
		m_iszMaster = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
	{
		static const char *relationshipKeys[] = {
			"relation_none", "relation_machine", "relation_player", "relation_human_passive",
			"relation_human_militar", "relation_alien_militar", "relation_alien_passive",
			"relation_alien_monster", "relation_alien_prey", "relation_alien_predator",
			"relation_insect", "relation_player_ally", "relation_player_bioweapon",
			"relation_monster_bioweapon"
		};

		for ( int i = 0; i < ARRAYSIZE( relationshipKeys ); i++ )
		{
			if ( !FStrEq( pkvd->szKeyName, relationshipKeys[i] ) )
				continue;

			if ( i != 0 )
			{
				int relation = atoi( pkvd->szValue );

				if ( relation < -2 )
					relation = i == 7 ? 2 : -2;
				else if ( relation > 3 )
					relation = 3;

				m_iRelationships[i] = relation;
			}

			pkvd->fHandled = TRUE;
			return;
		}

		CBaseEntity::KeyValue( pkvd );
}
}

////////////// START NEW STUFF //////////////

//==================================================================================
// TANK CONTROLLING
BOOL CFuncTank :: OnControls( entvars_t *pevTest )
{
	if ( !(pev->spawnflags & SF_TANK_CANCONTROL) )
		return FALSE;

	Vector offset = pevTest->origin - pev->origin;

	if ( (m_vecControllerUsePos - pevTest->origin).Length() < 30 )
		return TRUE;

	return FALSE;
}

bool CFuncTank :: StartControl( CBasePlayer *pController )
{
	if ( !pController || TankHandle( m_hController ) || TankHandle( m_hMonsterController )
	     || TankHandle( m_hUseProxy ) )
		return FALSE;

	// Team only or disabled?
	if ( m_iszMaster && !UTIL_IsMasterTriggered( m_iszMaster, pController ) )
			return FALSE;

	if ( pController->m_pActiveItem && pController->IsExclusiveHoldItemActive() )
		return false;

	TankSetHandle( m_hController, pController );
	pController->BlockWeapons( this );
	TankSetHandle( pController->m_hTank.m_Handle, this );
	m_vecControllerUsePos = TankHandle( m_hController )->pev->origin;

	pev->nextthink = (float) ( (double) pev->ltime + 0.1 );

	if ( pController->pev )
		pev->owner = pController->edict();
	else
		pev->owner = NULL;


	return TRUE;
}

void CFuncTank :: StopControl( void )
{
	CBasePlayer *controller = (CBasePlayer *) TankHandle( m_hController );

	if ( !controller )
		return;

	controller->m_hTank = NULL;
	controller->UnblockWeapons( this );
	m_hController = NULL;
	pev->owner = NULL;

	pev->nextthink = 0;

	if ( IsActive() )
		pev->nextthink = pev->ltime + 1.0;
}

void CFuncTank::MonsterStartup( CBaseMonster *controller )
{
	if ( !controller || m_bMonsterControl )
		return;

	m_bMonsterControl = true;
	pev->owner = controller->pev ? controller->edict() : NULL;
	TankSetHandle( m_hMonsterController, controller );
	pev->spawnflags |= SF_TANK_ACTIVE;
	pev->nextthink = (float) ( (double) pev->ltime + 0.1 );
	m_fireLast = 0;
}

const CBasePlayer *CFuncTank::GetController( void )
{
	return (CBasePlayer *) TankHandle( m_hController );
}

void CFuncTank::MonsterStopControl( void )
{
	m_hTarget = NULL;
	m_hController = NULL;
	m_bMonsterControl = false;

	CBaseMonster *controller = (CBaseMonster *) TankHandle( m_hMonsterController );

	if ( controller )
		controller->m_hSelectedTank = NULL;

	m_hMonsterController = NULL;
	pev->owner = NULL;
	pev->nextthink = 0;
	pev->spawnflags &= ~SF_TANK_ACTIVE;
	m_fireLast = 0;
	StopRotSound();
}

void CFuncTank::UpdateOnRemove( void )
{
	StopControl();

	if ( pev->noise )
		EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING( pev->noise ), 0, 0,
		                SND_STOP, PITCH_NORM, 0, false, g_vecZero );

	CBaseEntity::UpdateOnRemove();
}

// Called each frame by the player's ItemPostFrame
void CFuncTank :: ControllerPostFrame( void )
{
	CBasePlayer *controller = (CBasePlayer *) TankHandle( m_hController );

	if ( !controller || TankHandle( m_hMonsterController ) || !( m_flNextAttack <= gpGlobals->time ) )
		return;

	if ( m_iszMaster && !UTIL_IsMasterTriggered( m_iszMaster, controller ) )
	{
		StopControl();
		return;
	}

	if ( !( pev->spawnflags & SF_TANK_PLAYERCANTFIRE ) && ( controller->pev->button & IN_ATTACK ) )
	{
		Vector vecForward;
		UTIL_MakeVectorsPrivate( pev->angles, vecForward, NULL, NULL );

		m_fireLast = gpGlobals->time - ( 1.0f /m_fireRate) - 0.01;  // to make sure the gun doesn't fire too many bullets

		Fire( BarrelPosition(), vecForward, controller->pev );

		// HACKHACK -- make some noise (that the AI can hear)
		if ( TankHandle( m_hController ) )
			((CBasePlayer *) TankHandle( m_hController ) )->m_iWeaponVolume = LOUD_GUN_VOLUME;

		m_flNextAttack = 1.0f / m_fireRate + gpGlobals->time;
	}
}
////////////// END NEW STUFF //////////////


void CFuncTank :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *proxy = TankHandle( m_hUseProxy );

	if ( proxy )
	{
		proxy->Use( pActivator, pCaller, useType, value );
		return;
	}

	if ( pev->spawnflags & SF_TANK_CANCONTROL )
	{  // player controlled turret

		if ( !pActivator || !pActivator->IsPlayer() )
			return;

		if ( value == 2 && useType == USE_SET )
			ControllerPostFrame();
		else if ( !TankHandle( m_hMonsterController ) && !TankHandle( m_hController ) && useType != USE_OFF )
			StartControl( (CBasePlayer*)pActivator );
		else
			StopControl();
	}
	else
	{
		if ( !ShouldToggle( useType, IsActive() ) )
			return;

		if ( IsActive() )
			TankDeactivate();
		else
			TankActivate();
	}
}


void CFuncTank::Look( int distance )
{
	m_pSightLink = NULL;

	Vector range( distance, distance, distance );
	CBaseEntity *entities[100];

	int count = UTIL_EntitiesInBox( entities, ARRAYSIZE( entities ), pev->origin - range,
	                                pev->origin + range, FL_CLIENT | FL_MONSTER );

	for ( int i = 0; i < count; i++ )
	{
		CBaseEntity *entity = entities[i];

		if ( !entity || entity == this || ( entity->pev->spawnflags & SF_MONSTER_PRISONER ) )
			continue;

		if ( !( entity->pev->health > 0 ) || IRelationship( entity, false ) <= R_NO )
			continue;

		if ( !FVisible( entity, true ) )
			continue;

		entity->m_pSightLink = m_pSightLink;
		TankSetHandle( m_pSightLink, entity );
		IRelationship( entity, false );
	}
}

CBaseEntity *CFuncTank :: FindTarget( void )
{
	CBaseMonster *controller = (CBaseMonster *) TankHandle( m_hMonsterController );

	if ( controller && controller->IsAlive() )
		return TankHandle( controller->m_hEnemy.m_Handle );

	if ( m_bMonsterControl )
	{
		MonsterStopControl();
		return NULL;
	}

	m_pSightLink = NULL;
	Look( m_maxRange > 0 ? (int) m_maxRange : 2048 );

	CBaseEntity *best = NULL;
	int bestRelation = R_NO;
	int bestDistance = 65536;

	// The range threshold is the barrel's X offset
	for ( CBaseEntity *entity = TankHandle( m_pSightLink ); entity;
	      entity = TankHandle( entity->m_pSightLink ) )
	{
		if ( !entity->IsAlive() )
			continue;

		if ( IRelationship( entity, false ) > bestRelation )
		{
			int distance = (int) ( entity->pev->origin - pev->origin ).Length();

			if ( (float) distance > m_barrelPos.x )
			{
				bestDistance = distance;
				bestRelation = IRelationship( entity, false );
				best = entity;
			}
		}
		else if ( IRelationship( entity, false ) == bestRelation )
		{
			int distance = (int) ( entity->pev->origin - pev->origin ).Length();

			if ( distance <= bestDistance && (float) distance > m_barrelPos.x )
			{
				bestDistance = distance;
				bestRelation = IRelationship( entity, false );
				best = entity;
			}
		}
	}

	return best;
}



bool CFuncTank :: InRange( float range )
{
	if ( TankHandle( m_hMonsterController ) )
		return true;

	if ( !( m_minRange <= range ) )
		return FALSE;
	if ( m_maxRange > 0 )
		return range <= m_maxRange;

	return TRUE;
}


void CFuncTank :: Think( void )
{
	pev->avelocity = g_vecZero;
	TrackTarget();

	if ( fabs(pev->avelocity.x) > 1 || fabs(pev->avelocity.y) > 1 )
		StartRotSound();
	else
		StopRotSound();
}

void CFuncTank::TrackTarget( void )
{
	CBaseMonster *monster = (CBaseMonster *) TankHandle( m_hMonsterController );

	if ( m_bMonsterControl
	     && ( !monster || ( !monster->IsAlive() && !TankHandle( monster->m_hSelectedTank ) ) ) )
	{
		m_hTarget = NULL;
		MonsterStopControl();
		return;
	}

	TraceResult tr;
	Vector angles = pev->angles;
	Vector direction, barrelEnd;
	bool updateTime = FALSE;
	CBaseEntity *controller = TankHandle( m_hController );

	// Get a position to aim for
	if ( controller )
	{
		// Tanks attempt to mirror the player's angles
		angles = controller->pev->v_angle;
		angles.x = -angles.x;
		pev->nextthink = (float) ( (double) pev->ltime + 0.05 );
	}
	else
	{
		if ( !IsActive() )
			return;

		pev->nextthink = (float) ( (double) pev->ltime + 0.05 );

		// Calculate angle needed to aim at target
		barrelEnd = BarrelPosition();

		CBaseEntity *pTarget = FindTarget();

		TankSetHandle( m_hTarget, pTarget );
		pTarget = TankHandle( m_hTarget );

		if ( pTarget )
		{
			Vector targetPosition = pTarget->pev->origin + pTarget->pev->view_ofs;
		float range = (targetPosition - barrelEnd).Length();

		if ( !InRange( range ) )
			return;

		UTIL_TraceLine( barrelEnd, targetPosition, dont_ignore_monsters, edict(), &tr );

			if ( ( tr.flFraction == 1.0 || tr.pHit == pTarget->edict() ) && InRange( range ) && pTarget->IsAlive() )
			{
				updateTime = TRUE;
				m_sightOrigin = UpdateTargetPosition( pTarget );
			}

		// Track sight origin

// !!! I'm not sure what i changed
		direction = m_sightOrigin - pev->origin;
//		direction = m_sightOrigin - barrelEnd;
		angles = UTIL_VecToAngles( direction );

		// Calculate the additional rotation to point the end of the barrel at the target (not the gun's center)
		AdjustAnglesForBarrel( angles, direction.Length() );
	}
		else
		{
			m_fireLast = 0;

			if ( gpGlobals->time - m_lastSightTime > m_persist + m_persist )
				angles = pev->v_angle;
		}

		pev->view_ofs = BarrelPosition() - pev->origin;
	}

	angles.y = pev->v_angle.y + UTIL_AngleDistance( angles.y, pev->v_angle.y );
	angles.x = pev->v_angle.x + UTIL_AngleDistance( -angles.x, pev->v_angle.x );

	// Limit against range in y
	if ( angles.y > pev->v_angle.y + m_yawRange )
		angles.y = pev->v_angle.y + m_yawRange;
	else if ( angles.y < pev->v_angle.y - m_yawRange)
		angles.y = pev->v_angle.y - m_yawRange;
	else if ( updateTime )
		m_lastSightTime = gpGlobals->time;

	// Move toward target at rate or less
	float distY = UTIL_AngleDistance( angles.y, pev->angles.y );
	pev->avelocity.y = distY * 10.0f;
	if ( pev->avelocity.y > m_yawRate )
		pev->avelocity.y = m_yawRate;
	else if ( pev->avelocity.y < -m_yawRate )
		pev->avelocity.y = -m_yawRate;

	// Limit against range in x
	if ( angles.x > pev->v_angle.x + m_pitchRange )
		angles.x = pev->v_angle.x + m_pitchRange;
	else if ( angles.x < pev->v_angle.x - m_pitchRange )
		angles.x = pev->v_angle.x - m_pitchRange;

	// Move toward target at rate or less
	float distX = UTIL_AngleDistance( angles.x, pev->angles.x );
	pev->avelocity.x = distX  * 10.0f;

	if ( pev->avelocity.x > m_pitchRate )
		pev->avelocity.x = m_pitchRate;
	else if ( pev->avelocity.x < -m_pitchRate )
		pev->avelocity.x = -m_pitchRate;

	if ( TankHandle( m_hController ) )
		return;

	CBaseEntity *pTarget = TankHandle( m_hTarget );

	if ( pTarget && CanFire() && ( (fabs(distX) < m_pitchTolerance && fabs(distY) < m_yawTolerance) || (pev->spawnflags & SF_TANK_LINEOFSIGHT) ) )
	{
		Vector forward;
		UTIL_MakeVectorsPrivate( pev->angles, forward, NULL, NULL );

		if ( pev->spawnflags & SF_TANK_LINEOFSIGHT )
		{
			UTIL_TraceLine( barrelEnd, barrelEnd + forward * direction.Length(), dont_ignore_monsters, edict(), &tr );
			if ( tr.pHit != pTarget->edict() )
			{
				m_fireLast = 0;
				return;
			}
		}

		Fire( BarrelPosition(), forward, pev );
		}
		else
			m_fireLast = 0;
	}


// If barrel is offset, add in additional rotation
void CFuncTank::AdjustAnglesForBarrel( Vector &angles, float distance )
{
	if ( m_barrelPos.y == 0 && m_barrelPos.z == 0 )
		return;

	distance -= m_barrelPos.z;

	float distanceSquared = distance * distance;
		if ( m_barrelPos.y != 0 )
	{
		float radiusSquared = m_barrelPos.y * m_barrelPos.y;
		float adjacent = (float) sqrt( (double) ( distanceSquared - radiusSquared ) );
		float angle = (float) atan2( (double) m_barrelPos.y, (double) adjacent );

		angles.y = (float) ( (double) angle * (180.0 / M_PI) + (double) angles.y );
		}
		if ( m_barrelPos.z != 0 )
	{
		float radiusSquared = m_barrelPos.z * m_barrelPos.z;
		float adjacent = (float) sqrt( (double) ( distanceSquared - radiusSquared ) );
		float angle = (float) atan2( (double) -m_barrelPos.z, (double) adjacent );

		angles.x = (float) ( (double) angle * (180.0 / M_PI) + (double) angles.x );
		}
	}

bool CFuncTank::PointAt( const Vector &destination )
{
	Vector direction = destination - pev->origin;

	direction.z = -direction.z;

	Vector angles = UTIL_VecToAngles( direction );

	AdjustAnglesForBarrel( angles, direction.Length() );
	pev->angles = angles;

	// The script binding reads AL, left as the low byte of roll
	unsigned char roll;

	memcpy( &roll, &angles.z, sizeof( roll ) );
	return roll != 0;
}

void CFuncTank::Fire( void )
{
	Vector forward;

	UTIL_MakeVectorsPrivate( pev->angles, forward, NULL, NULL );
	Fire( BarrelPosition(), forward, pev );
}

int CFuncTank::IRelationship( CBaseEntity *target, bool )
{
	if ( !target )
		return R_NO;

	if ( target->IsPlayer() )
		return ( pev->spawnflags & SF_TANK_USERELATIONS ) ? m_iRelationships[CLASS_PLAYER] : R_HT;

	const int *relation = m_iRelationships;
	int relationships[14][14] = {
		{ 0, relation[0], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ relation[0], relation[1], relation[2], relation[3], relation[4], relation[5], relation[6],
		  relation[7], relation[8], relation[9], relation[10], relation[11], relation[12], relation[13] },
		{ 0, relation[2], 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1 },
		{ 0, relation[3], -2, -2, 2, 4, 0, 2, 1, 4, 0, -2, 0, 0 },
		{ 0, relation[4], 2, 1, 0, 2, 1, 1, 1, 1, 0, 2, 0, 0 },
		{ 0, relation[5], 2, 1, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0 },
		{ 0, relation[6], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, relation[7], 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0 },
		{ 0, relation[8], 1, 1, 1, 0, 0, 0, 0, 4, 0, 1, 0, 0 },
		{ 0, relation[9], 1, 1, 1, 0, 0, 0, 2, 1, 0, 1, 0, 0 },
		{ 4, relation[10], 4, 4, 4, 0, 4, 4, 4, 4, 0, 4, 0, 0 },
		{ 0, relation[11], -2, -2, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
		{ 0, relation[12], 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1 },
		{ 0, relation[13], 1, 1, 1, -2, 0, 1, 1, 0, 0, 1, 1, 0 }
	};

	int ownClass = Classify();

	return relationships[ownClass][target->Classify()];
}

int CFuncTank::Classify( void )
{
	return CLASS_MACHINE;
}

int CFuncTank::BulletCount( void )
{
	return (int) ( ( gpGlobals->time - m_fireLast ) * m_fireRate );
}

// Fire targets and spawn sprites
void CFuncTank::Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	if ( m_fireLast != 0 )
	{
		if ( m_iszSpriteSmoke )
		{
			CSprite *pSprite = CSprite::SpriteCreate( STRING(m_iszSpriteSmoke), barrelEnd, TRUE );
			pSprite->AnimateAndDie( RANDOM_FLOAT( 15.0, 20.0 ) );
			pSprite->SetTransparency( kRenderTransAlpha, pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z, 255, kRenderFxNone );
			pSprite->pev->velocity.z = RANDOM_FLOAT(40, 80);
			pSprite->SetScale( m_spriteScale );
		}
		if ( m_iszSpriteFlash )
		{
			CSprite *pSprite = CSprite::SpriteCreate( STRING(m_iszSpriteFlash), barrelEnd, TRUE );
			pSprite->AnimateAndDie( 60 );
			pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
			pSprite->SetScale( m_spriteScale );

			// Hack Hack, make it stick around for at least 100 ms.
			pSprite->pev->nextthink += 0.1;
		}
		SUB_UseTargets( this, USE_TOGGLE, 0 );
	}
	m_fireLast = gpGlobals->time;
}


void CFuncTank::TankTrace( const Vector &vecStart, const Vector &vecForward, const Vector &vecSpread, TraceResult &tr )
{
	// get circular gaussian spread
	float x, y, z;
	do {
		x = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
		y = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
		z = x*x+y*y;
	} while (z > 1);
	Vector vecDir = vecForward +
		x * vecSpread.x * gpGlobals->v_right +
		y * vecSpread.y * gpGlobals->v_up;
	Vector vecEnd;
	
	vecEnd = vecStart + vecDir * 4096;
	UTIL_TraceLine( vecStart, vecEnd, dont_ignore_monsters, edict(), &tr );
}

	
void CFuncTank::StartRotSound( void )
{
	if ( !pev->noise || (pev->spawnflags & SF_TANK_SOUNDON) )
		return;
	pev->spawnflags |= SF_TANK_SOUNDON;
	EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING(pev->noise), 0.85, ATTN_NORM,
	                0, PITCH_NORM, 0, false, g_vecZero );
}


void CFuncTank::StopRotSound( void )
{
	if ( pev->spawnflags & SF_TANK_SOUNDON )
		EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING(pev->noise), 0, 0,
		                SND_STOP, PITCH_NORM, 0, false, g_vecZero );
	pev->spawnflags &= ~SF_TANK_SOUNDON;
}

CBaseEntity *Create_CCustomTankEntity( entvars_t *pev )
{
	return GetClassPtr( (CBaseCustomTankEntity<CFuncTank> *) pev );
}

class CFuncTankGun : public CFuncTank
{
public:
	void Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
};
LINK_ENTITY_TO_CLASS( func_tank, CFuncTankGun );

void CFuncTankGun::Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	int i;

	if ( m_fireLast != 0 )
	{
		// FireBullets needs gpGlobals->v_up, etc.
		UTIL_MakeAimVectors(pev->angles);

		int bulletCount = BulletCount();
		if ( bulletCount > 0 )
		{
			for ( i = 0; i < bulletCount; i++ )
			{
				switch( m_bulletType )
				{
				case TANK_BULLET_9MM:
					FireBullets( 1, barrelEnd, forward, gTankSpread[m_spread], 4096, BULLET_MONSTER_9MM, 1, m_iBulletDamage, pevAttacker );
					break;

				case TANK_BULLET_MP5:
					FireBullets( 1, barrelEnd, forward, gTankSpread[m_spread], 4096, BULLET_MONSTER_MP5, 1, m_iBulletDamage, pevAttacker );
					break;

				case TANK_BULLET_12MM:
					FireBullets( 1, barrelEnd, forward, gTankSpread[m_spread], 4096, BULLET_MONSTER_12MM, 1, m_iBulletDamage, pevAttacker );
					break;

				default:
				case TANK_BULLET_NONE:
					break;
				}
			}
			CFuncTank::Fire( barrelEnd, forward, pevAttacker );
		}
	}
	else
		CFuncTank::Fire( barrelEnd, forward, pevAttacker );
}



class CFuncTankLaser : public CFuncTank
{
public:
	void	Activate( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
	void	Think( void );
	CLaser *GetLaser( void );

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

private:
	EHANDLE m_hLaser;
	float   m_laserTime;
};
LINK_ENTITY_TO_CLASS( func_tanklaser, CFuncTankLaser );
LINK_ENTITY_TO_CLASS( func_tanklaser_of, CFuncTankLaser );

TYPEDESCRIPTION	CFuncTankLaser::m_SaveData[] = 
{
	DEFINE_FIELD( CFuncTankLaser, m_hLaser, FIELD_EHANDLE ),
	DEFINE_FIELD( CFuncTankLaser, m_laserTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CFuncTankLaser, CFuncTank );

void CFuncTankLaser::Activate( void )
{
	CLaser *pLaser = GetLaser();

	if ( !pLaser )
	{
		UTIL_Remove( this );
		ALERT( at_error, "Laser tank with no env_laser!\n" );
	}
	else
	{
		pLaser->TurnOff();
	}
}


void CFuncTankLaser::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "laserentity"))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CFuncTank::KeyValue( pkvd );
}


CLaser *CFuncTankLaser::GetLaser( void )
{
	CLaser *pLaser = (CLaser *) TankHandle( m_hLaser );

	if ( pLaser )
		return pLaser;

	edict_t	*pentLaser;

	pentLaser = FIND_ENTITY_BY_TARGETNAME( NULL, STRING(pev->message) );
	while ( !FNullEnt( pentLaser ) )
	{
		// Found the landmark
		if ( FClassnameIs( pentLaser, "env_laser" ) )
		{
			TankSetHandle( m_hLaser, CBaseEntity::Instance( pentLaser ) );
			break;
		}
		else
			pentLaser = FIND_ENTITY_BY_TARGETNAME( pentLaser, STRING(pev->message) );
	}

	return (CLaser *) TankHandle( m_hLaser );
}


void CFuncTankLaser::Think( void )
{
	CLaser *pLaser = (CLaser *) TankHandle( m_hLaser );

	if ( pLaser && gpGlobals->time > m_laserTime )
		pLaser->TurnOff();

	CFuncTank::Think();
}


void CFuncTankLaser::Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	int i;
	TraceResult tr;

	if ( m_fireLast != 0 && GetLaser() )
	{
		// TankTrace needs gpGlobals->v_up, etc.
		UTIL_MakeAimVectors(pev->angles);

		int bulletCount = BulletCount();
		if ( bulletCount )
		{
			for ( i = 0; i < bulletCount; i++ )
			{
				( (CLaser *) TankHandle( m_hLaser ) )->pev->origin = barrelEnd;
				TankTrace( barrelEnd, forward, gTankSpread[m_spread], tr );
				
				m_laserTime = gpGlobals->time;
				( (CLaser *) TankHandle( m_hLaser ) )->TurnOn();
				( (CLaser *) TankHandle( m_hLaser ) )->pev->dmgtime = gpGlobals->time - 1.0;

				if ( pevAttacker )
					( (CLaser *) TankHandle( m_hLaser ) )->pev->owner = ENT( pevAttacker );

				( (CLaser *) TankHandle( m_hLaser ) )->FireAtPoint( tr );
				( (CLaser *) TankHandle( m_hLaser ) )->pev->nextthink = 0;
			}
			CFuncTank::Fire( barrelEnd, forward, pev );
		}
	}
	else
	{
		CFuncTank::Fire( barrelEnd, forward, pev );
	}
}

class CFuncTankRocket : public CFuncTank
{
public:
	void Precache( void );
	void Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
};
LINK_ENTITY_TO_CLASS( func_tankrocket, CFuncTankRocket );

void CFuncTankRocket::Precache( void )
{
	UTIL_PrecacheOther( "rpg_rocket" );
	CFuncTank::Precache();
}



void CFuncTankRocket::Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	int i;

	if ( m_fireLast != 0 )
	{
		int bulletCount = BulletCount();
		if ( bulletCount > 0 )
		{
			for ( i = 0; i < bulletCount; i++ )
			{
				CBaseEntity *pRocket = CBaseEntity::Create( "rpg_rocket", barrelEnd, pev->angles, false, pevAttacker ? ENT( pevAttacker ) : NULL );
			}
			CFuncTank::Fire( barrelEnd, forward, pev );
		}
	}
	else
		CFuncTank::Fire( barrelEnd, forward, pev );
}


class CFuncTankMortar : public CFuncTank
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
};
LINK_ENTITY_TO_CLASS( func_tankmortar, CFuncTankMortar );


void CFuncTankMortar::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "iMagnitude"))
	{
		pev->impulse = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CFuncTank::KeyValue( pkvd );
}


void CFuncTankMortar::Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	if ( m_fireLast != 0 )
	{
		int bulletCount = BulletCount();
		// Only create 1 explosion
		if ( bulletCount > 0 )
		{
			TraceResult tr;

			// TankTrace needs gpGlobals->v_up, etc.
			UTIL_MakeAimVectors(pev->angles);

			TankTrace( barrelEnd, forward, gTankSpread[m_spread], tr );

			ExplosionCreate( tr.vecEndPos, pev->angles, edict(), pev->impulse, TRUE );

			CFuncTank::Fire( barrelEnd, forward, pev );
		}
	}
	else
		CFuncTank::Fire( barrelEnd, forward, pev );
}



//============================================================================
// FUNC TANK CONTROLS
//============================================================================
class CFuncTankControls : public CBaseEntity
{
public:
	virtual int	ObjectCaps( void );
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Think( void );

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	EHANDLE m_hTank;
};
LINK_ENTITY_TO_CLASS( func_tankcontrols, CFuncTankControls );

TYPEDESCRIPTION	CFuncTankControls::m_SaveData[] = 
{
	DEFINE_FIELD( CFuncTankControls, m_hTank, FIELD_EHANDLE ),
};

IMPLEMENT_SAVERESTORE( CFuncTankControls, CBaseEntity );

int	CFuncTankControls :: ObjectCaps( void ) 
{ 
	return (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IMPULSE_USE; 
}


void CFuncTankControls :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *tank = TankHandle( m_hTank );

	if ( tank )
		tank->Use( pActivator, pCaller, useType, value );
}


void CFuncTankControls :: Think( void )
{
	edict_t *pTarget = NULL;

	do
	{
		pTarget = FIND_ENTITY_BY_TARGETNAME( pTarget, STRING(pev->target) );
	} while ( pTarget && !pTarget->free && !FNullEnt(pTarget) && strncmp( STRING(pTarget->v.classname), "func_tank", 9 ) );

	if ( !pTarget || pTarget->free || FNullEnt( pTarget ) )
	{
		ALERT( at_console, "No tank %s\n", STRING(pev->target) );
		return;
	}

	m_hTank = NULL;

	if ( !pTarget->free )
	{
		CBaseEntity *tank = (CBaseEntity *) pTarget->pvPrivateData;

		if ( tank )
			TankSetHandle( m_hTank, tank );
}
}

void CFuncTankControls::Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;
	SET_MODEL( ENT(pev), STRING(pev->model) );

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( pev, pev->origin );

	pev->nextthink = gpGlobals->time + 0.3; // After all the func_tank's have spawned
}
