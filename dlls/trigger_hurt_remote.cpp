#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "trigger_hurt_remote.h"

#define SF_HURTREMOTE_INSTANT 1
#define SF_HURTREMOTE_REPEAT 2
#define SF_HURTREMOTE_START_ON 4
#define SF_HURTREMOTE_ARMOR 8

LINK_ENTITY_TO_CLASS( trigger_hurt_remote, CTriggerHurtRemote );

CBaseEntity *HurtRemoteHandleEntity( EHANDLE &handle )
{
	edict_t *entity = handle.Get();

	return entity && !entity->free ? (CBaseEntity *) GET_PRIVATE( entity ) : NULL;
}

bool HurtRemoteEntityValid( CBaseEntity *entity )
{
	return entity && entity->pev && entity->pev->pContainingEntity &&
	       !entity->pev->pContainingEntity->free && !FNullEnt( entity->pev->pContainingEntity );
}

void HurtRemoteSetHandle( EHANDLE &handle, CBaseEntity *entity )
{
	handle = EHANDLE();

	if ( entity && entity->pev && entity->pev->pContainingEntity && !entity->pev->pContainingEntity->free )
		handle.Set( entity->pev->pContainingEntity );
}

CTriggerHurtRemote::CTriggerHurtRemote()
    : m_bActive( false ), m_flHurtDelay( 0 ), m_flLastSearch( 0 ), m_flArmorDamage( 0 ),
      m_iDamageType( 0 ), m_iszTargetClass( 0 ), m_hActivator(), m_hCaller(), m_pTargets( NULL )
{
}

CTriggerHurtRemote::~CTriggerHurtRemote()
{
}

void CTriggerHurtRemote::Spawn( void )
{
	if ( pev->spawnflags & SF_HURTREMOTE_START_ON )
		m_bActive = true;

	m_pTargets = NULL;
	m_hActivator = EHANDLE();
	m_hCaller = EHANDLE();

	if ( m_bActive )
	{
		SetThink( &CTriggerHurtRemote::Think );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

void CTriggerHurtRemote::KeyValue( KeyValueData *data )
{
	if ( FStrEq( data->szKeyName, "delay" ) )
		m_flHurtDelay = atof( data->szValue );
	else if ( FStrEq( data->szKeyName, "damagetype" ) )
		m_iDamageType = atoi( data->szValue );
	else if ( FStrEq( data->szKeyName, "armordmg" ) )
		m_flArmorDamage = atof( data->szValue );
	else if ( FStrEq( data->szKeyName, "targetclass" ) )
		m_iszTargetClass = AllocStringLocal( data->szValue );
	else
	{
		CBaseDelay::KeyValue( data );
		return;
	}

	data->fHandled = TRUE;
}

void CTriggerHurtRemote::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_bActive ) )
		return;

	m_bActive = !m_bActive;

	if ( m_bActive )
	{
		HurtRemoteSetHandle( m_hActivator, activator );
		HurtRemoteSetHandle( m_hCaller, caller );
		m_flLastSearch = 0;
		Think();
	}
	else
	{
		SetThink( NULL );
		m_flLastSearch = 0;
	}
}

void CTriggerHurtRemote::FindTargets( void )
{
	TargetNode *node = m_pTargets;

	while ( node )
	{
		TargetNode *next = node->next;

		delete node;
		node = next;
	}

	m_pTargets = new TargetNode;
	node = m_pTargets;
	CBaseEntity *target = NULL;

	if ( pev->target )
	{
		if ( FStrEq( STRING( pev->target ), "!activator" ) )
		{
			node->entity = m_hActivator;
			node->next = new TargetNode;
			node = node->next;
		}
		else
		{
			while ( ( target = UTIL_FindEntityByTargetname( target, STRING( pev->target ) ) ) != NULL )
			{
				HurtRemoteSetHandle( node->entity, target );
				node->next = new TargetNode;
				node = node->next;
			}
		}
	}
	else if ( !m_iszTargetClass )
	{
		ALERT( at_console, "trigger_hurt_remote with no target/class name to hurt! (X%f Y%f Z%f)",
		       pev->origin.x, pev->origin.y, pev->origin.z );
		SetThink( NULL );
		m_bActive = false;
		m_flLastSearch = 0;
		return;
	}

	if ( m_iszTargetClass )
	{
		while ( ( target = UTIL_FindEntityByClassname( target, STRING( m_iszTargetClass ) ) ) != NULL )
		{
			HurtRemoteSetHandle( node->entity, target );
			node->next = new TargetNode;
			node = node->next;
		}
	}
}

void CTriggerHurtRemote::Think( void )
{
	if ( m_flLastSearch == 0 || (double) ( gpGlobals->time - m_flLastSearch ) > 0.33 )
	{
		FindTargets();
		m_flLastSearch = gpGlobals->time;
	}

	for ( TargetNode *node = m_pTargets; node; node = node->next )
	{
		CBaseEntity *target = HurtRemoteHandleEntity( node->entity );

		if ( !target )
			break;

		if ( !HurtRemoteEntityValid( target ) )
		{
			m_flLastSearch = 0;
			continue;
		}

		if ( ( target->IsPlayer() || target->IsMonster() ) && !target->IsAlive() )
			continue;

		if ( pev->spawnflags & SF_HURTREMOTE_INSTANT )
		{
			if ( pev->spawnflags & SF_HURTREMOTE_ARMOR )
				target->pev->armorvalue = m_flArmorDamage < 0 ? target->pev->armortype : 0;

			if ( pev->dmg < 0 )
			{
				target->pev->health = target->pev->max_health;
				target->TakeHealth( -pev->dmg, m_iDamageType, 0 );
			}
			else
			{
				target->pev->health = 1;

				CBaseEntity *activator = HurtRemoteHandleEntity( m_hActivator );
				float damage = pev->dmg > 100.0f ? pev->dmg : 100.0f;

				target->TakeDamage( pev, activator ? activator->pev : pev, damage, m_iDamageType );
			}
		}
		else
		{
			if ( pev->spawnflags & SF_HURTREMOTE_ARMOR )
			{
				float armor = target->pev->armorvalue - m_flArmorDamage;

				if ( m_flArmorDamage < 0 )
					armor = armor < target->pev->armortype ? armor : target->pev->armortype;
				else
					armor = armor > 0 ? armor : 0;

				target->pev->armorvalue = armor;
			}

			if ( pev->dmg < 0 )
				target->TakeHealth( -pev->dmg, m_iDamageType, 0 );
			else
			{
				CBaseEntity *activator = HurtRemoteHandleEntity( m_hActivator );

				target->TakeDamage( pev, activator ? activator->pev : pev, pev->dmg, m_iDamageType );
			}
		}
	}

	if ( m_bActive && ( pev->spawnflags & SF_HURTREMOTE_REPEAT ) )
	{
		SetThink( &CTriggerHurtRemote::Think );
		pev->nextthink = gpGlobals->time + m_flHurtDelay;
	}
	else
	{
		SetThink( NULL );
		m_bActive = false;
		m_flLastSearch = 0;
	}
}
