//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Functionality for the observer chase camera
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"weapons.h"
#include	"pm_shared.h"
#include	"observer.h"
#include	"CSpawnPoints.h"
#include	"item_inventory.h"
#include	"CAdvFuncVehicle.h"
#include	"user_messages.h"
#include	"game.h"

extern int gmsgCurWeapon;
extern int gmsgSetFOV;

class CBaseDeadPlayer : public CBaseAnimating
{
public:
	CBaseDeadPlayer() : m_hPlayer() {}
	void Initialize( CBasePlayer *player );
	void Spawn( void );
	int Classify( void );
	bool IsRevivable( void ) { return true; }
	void BeginRevive( float delay ) {}
	void EndRevive( float delay );
	void EXPORT Revive( void );
	void EXPORT CorpseFallThink( void );

private:
	CEntityHandle<CBasePlayer> m_hPlayer;
};

void CBaseDeadPlayer::Initialize( CBasePlayer *player )
{
	m_hPlayer = player;

	if ( !player )
	{
		SetThink( NULL );
		pev->effects |= EF_NODRAW;
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
		return;
	}

	pev->model = player->pev->model;
	pev->modelindex = player->pev->modelindex;
	ResetSequenceInfo();

	pev->gaitsequence = 255;
	pev->angles.y = player->pev->angles.y;
	pev->frame = player->pev->frame;
	pev->colormap = player->pev->colormap;
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_TOSS;
	pev->deadflag = player->pev->deadflag;
	pev->renderfx = kRenderFxDeadPlayer;
	pev->renderamt = player->entindex();
	pev->effects = ( player->pev->effects & ~EF_NODRAW ) | EF_NOINTERP;
	pev->sequence = player->pev->sequence;
	pev->animtime = player->pev->animtime;
	pev->deadflag = DEAD_DEAD;
	pev->health = player->pev->health;
	pev->velocity = player->pev->velocity;
	UTIL_SetOrigin( pev, player->pev->origin );
	UTIL_SetSize( pev, player->pev->mins, player->pev->maxs );
	CorpseFallThink();
}

void CBaseDeadPlayer::Spawn( void )
{
	pev->classname = MAKE_STRING( "deadplayer" );
	pev->flags |= FL_NOTARGET;
}

int CBaseDeadPlayer::Classify( void )
{
	CBasePlayer *player = m_hPlayer;

	return player ? player->Classify() : CLASS_NONE;
}

void CBaseDeadPlayer::CorpseFallThink( void )
{
	if ( pev->flags & FL_ONGROUND )
	{
		SetThink( NULL );
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
		UTIL_SetOrigin( pev, pev->origin );
		return;
	}

	SetThink( &CBaseDeadPlayer::CorpseFallThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CBaseDeadPlayer::EndRevive( float delay )
{
	SetThink( &CBaseDeadPlayer::Revive );
	pev->nextthink = delay + gpGlobals->time;
}

void CBaseDeadPlayer::Revive( void )
{
	pev->effects |= EF_NODRAW;

	CBasePlayer *player = m_hPlayer;

	if ( player )
	{
		if ( !player->IsAlive() )
			player->Revive();

		m_hPlayer = NULL;
	}

	UTIL_Remove( this );
}

CObserver::CObserver( CBasePlayer *player )
	: m_hPlayer(), m_hObserverTarget(), m_flNextObserverInput( 0 ), m_hDeadBody(),
	  m_fCanControlObserverMode( true )
{
	m_hPlayer = player;
}

CObserver::~CObserver()
{
	RemoveDeadBody();
}

CBasePlayer *CObserver::GetObserverTarget( void )
{
	return m_hObserverTarget;
}

void CObserver::SetObserverTarget( CBasePlayer *player )
{
	m_hObserverTarget = player;
}

void CObserver::ResetObserverTarget( void )
{
	m_hObserverTarget = NULL;

	int mode = m_hPlayer->pev->iuser1;

	m_hPlayer->pev->iuser1 = -1;
	SetMode( mode );
}

bool CObserver::IsObserver( void )
{
	return m_hPlayer->pev->iuser1 != 0;
}

bool CObserver::HasCorpse( void )
{
	return (CBaseEntity *) m_hDeadBody != NULL;
}

bool CObserver::CanControlObserverMode( void )
{
	return m_fCanControlObserverMode;
}

void CObserver::SetObserverModeControlEnabled( bool enabled )
{
	m_fCanControlObserverMode = enabled;
}

bool CObserver::CanUseObserver( void )
{
	return CVAR_GET_FLOAT( "mp_observer_mode" ) > 0;
}

bool CObserver::IsObserverCyclic( void )
{
	return CVAR_GET_FLOAT( "mp_observer_cyclic" ) != 0;
}

void CObserver::SetObserverCyclic( bool enabled )
{
	CVAR_SET_FLOAT( "mp_observer_cyclic", enabled ? 1.0f : 0.0f );
}

void CObserver::RemoveDeadBody( void )
{
	CBaseEntity *body = m_hDeadBody;

	if ( !body )
		return;

	UTIL_Remove( body );
	m_hDeadBody = NULL;
}

void CObserver::FindNextPlayer( bool reverse )
{
	CBasePlayer *target = m_hObserverTarget;
	int first = target ? target->entindex() : m_hPlayer->entindex();
	int current = first;
	int direction = reverse ? -1 : 1;

	m_hObserverTarget = NULL;

	do
	{
		current += direction;

		if ( current > gpGlobals->maxClients )
			current = 1;

		if ( current < 1 )
			current = gpGlobals->maxClients;

		CBasePlayer *player = (CBasePlayer *) UTIL_PlayerByIndex( current );

		if ( player && player != (CBasePlayer *) m_hPlayer && player->IsAlive()
			&& !player->GetObserver()->IsObserver() )
		{
			m_hObserverTarget = player;
			break;
		}
	} while ( current != first );

	target = m_hObserverTarget;

	if ( !target )
	{
		m_hPlayer->pev->iuser2 = 0;
		ALERT( at_console, "No observer targets.\n" );
		SetMode( OBS_ROAMING );
		return;
	}

	m_hPlayer->pev->iuser2 = target->entindex();
	m_hPlayer->Teleport( target->pev->origin, m_hPlayer->pev->angles );
	ALERT( at_console, "Now Tracking %s\n", STRING( m_hObserverTarget->pev->netname ) );
}

void CObserver::SetMode( int mode )
{
	if ( mode == m_hPlayer->pev->iuser1 )
		return;

	if ( mode != OBS_CHASE_FREE && mode != OBS_CHASE_LOCKED )
	{
		m_hPlayer->pev->iuser1 = OBS_ROAMING;
		m_hPlayer->pev->iuser2 = 0;
		ClientPrint( m_hPlayer->pev, HUD_PRINTCENTER, "#Spec_Mode3" );
		m_hPlayer->pev->maxspeed = 600;
		return;
	}

	if ( !m_hObserverTarget )
		FindNextPlayer( false );

	if ( !m_hObserverTarget )
	{
		ClientPrint( m_hPlayer->pev, HUD_PRINTCENTER, "#Spec_NoTarget" );
		SetMode( OBS_ROAMING );
		return;
	}

	m_hPlayer->pev->iuser1 = mode;
	m_hPlayer->pev->iuser2 = m_hObserverTarget->entindex();
	ClientPrint( m_hPlayer->pev, HUD_PRINTCENTER,
		mode == OBS_CHASE_FREE ? "#Spec_Mode2" : "#Spec_Mode1" );
	m_hPlayer->pev->maxspeed = 0;
}

void CObserver::HandleButtons( void )
{
	if ( m_flNextObserverInput > gpGlobals->time )
		return;

	if ( m_fCanControlObserverMode && ( m_hPlayer->m_afButtonPressed & IN_JUMP ) )
	{
		SetMode( m_hPlayer->pev->iuser1 % 3 + 1 );
		m_flNextObserverInput = gpGlobals->time + 0.2;
	}

	if ( ( m_hPlayer->m_afButtonPressed & IN_ATTACK ) && m_hPlayer->pev->iuser1 != OBS_ROAMING )
	{
		FindNextPlayer( false );
		m_flNextObserverInput = gpGlobals->time + 0.2;
	}

	if ( ( m_hPlayer->m_afButtonPressed & IN_ATTACK2 ) && m_hPlayer->pev->iuser1 != OBS_ROAMING )
	{
		FindNextPlayer( true );
		m_flNextObserverInput = gpGlobals->time + 0.2;
	}
}

void CObserver::CheckTarget( void )
{
	if ( m_hPlayer->pev->iuser1 == OBS_ROAMING )
		return;

	if ( !m_hObserverTarget )
		FindNextPlayer( false );

	CBasePlayer *target = m_hObserverTarget;

	if ( !target )
	{
		int lastMode = m_hPlayer->pev->iuser1;

		SetMode( OBS_ROAMING );
		m_iObserverLastMode = lastMode;
		return;
	}

	if ( !target->IsAlive() && gpGlobals->time > (float) ( (double) target->m_fDeadTime + 2.0 ) )
		FindNextPlayer( false );
}

void CObserver::CheckProperties( void )
{
	if ( m_hPlayer->pev->iuser1 != OBS_IN_EYE || !m_hObserverTarget )
	{
		m_hPlayer->m_iFOV = 0;

		if ( m_iObserverWeapon != 0 )
		{
			m_iObserverWeapon = 0;
			m_hPlayer->SendCurrentWeapon( 1, 0, 0, 0 );
		}

		return;
	}

	CBasePlayerItem *item = m_hObserverTarget->m_pActiveItem;
	int weapon = item ? item->m_iId : 0;

	if ( m_hPlayer->m_iFOV == m_hObserverTarget->m_iFOV && m_iObserverWeapon == weapon )
		return;

	m_hPlayer->m_iFOV = m_hObserverTarget->m_iFOV;
	m_hPlayer->m_iClientFOV = m_hPlayer->m_iFOV;

	MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, m_hPlayer->pev );
		WRITE_BYTE( m_hPlayer->m_iFOV );
	MESSAGE_END();

	m_iObserverWeapon = weapon;
	m_hPlayer->SendCurrentWeapon( 1, weapon, 0, 0 );
}

CBaseEntity *CObserver::FindObserverStartPoint( void )
{
	CBaseEntity *point;

	{
		CSpawnPoints points;

		points.Collect( m_hPlayer );
		point = points.GetRandomSpawnPoint( true );

		if ( !point )
			point = points.GetRandomSpawnPoint( false );
	}

	CBaseEntity *world = CWorld::GetInstance();

	if ( point && point != world && !FClassnameIs( point->pev, "worldspawn" ) )
		return point;

	for ( int index = 0; index < ARRAYSIZE( CSpawnPoints::m_DeathmatchClassNames ); ++index )
	{
		point = UTIL_FindEntityByClassname( NULL, CSpawnPoints::m_DeathmatchClassNames[index] );

		if ( point )
			return point;
	}

	for ( int index = 0; index < ARRAYSIZE( CSpawnPoints::m_CoopClassNames ); ++index )
	{
		point = UTIL_FindEntityByClassname( NULL, CSpawnPoints::m_CoopClassNames[index] );

		if ( point )
			return point;
	}

	point = UTIL_FindEntityByClassname( NULL, "player" );

	if ( point == (CBasePlayer *) m_hPlayer )
		point = UTIL_FindEntityByClassname( point, "player" );

	if ( !point )
		point = UTIL_FindEntityByTargetname( NULL, STRING( gpGlobals->startspot ) );

	return point ? point : world;
}

void CObserver::StartObserver( const Vector &position, const Vector &angles, bool createBody )
{
	if ( m_hPlayer->pev->iuser1 != OBS_NONE )
		return;

	m_hPlayer->pev->iuser1 = -1;

	if ( createBody && !( m_hPlayer->pev->effects & EF_NODRAW ) )
	{
		CBaseDeadPlayer *body = GetClassPtr( (CBaseDeadPlayer *) NULL );

		body->Initialize( m_hPlayer );
		body->Spawn();
		m_hDeadBody = body;
	}

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, m_hPlayer->pev->origin );
		WRITE_BYTE( TE_KILLPLAYERATTACHMENTS );
		WRITE_BYTE( m_hPlayer->entindex() );
	MESSAGE_END();

	if ( m_hPlayer->m_pActiveItem )
		m_hPlayer->m_pActiveItem->Holster();

	if ( m_hPlayer->m_hTank )
	{
		m_hPlayer->m_hTank->Use( m_hPlayer, m_hPlayer, USE_OFF, 0 );
		m_hPlayer->m_hTank = NULL;
	}

	if ( m_hPlayer->m_hVehicle )
		( (CAdvFuncVehicle *) (CBaseEntity *) m_hPlayer->m_hVehicle )->PlayerExit( m_hPlayer, true );

	m_hPlayer->SetSuitUpdate( NULL, FALSE, 0 );

	m_hPlayer->SendNoCurrentWeapon();
	m_hPlayer->m_iClientFOV = 0;
	m_hPlayer->m_iFOV = 0;
	m_hPlayer->pev->fov = (float) m_hPlayer->m_iFOV;

	MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, m_hPlayer->pev );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	m_hPlayer->m_afPhysicsFlags |= PFLAG_OBSERVER;
	m_hPlayer->pev->effects = EF_NODRAW;
	m_hPlayer->pev->view_ofs = g_vecZero;
	m_hPlayer->pev->v_angle = angles;
	m_hPlayer->pev->angles = angles;
	m_hPlayer->pev->angles.z = 0;
	m_hPlayer->pev->fixangle = 1;
	m_hPlayer->pev->solid = SOLID_NOT;
	m_hPlayer->pev->movetype = MOVETYPE_NONE;
	m_hPlayer->pev->health = 1;
	m_hPlayer->pev->takedamage = DAMAGE_NO;
	m_hPlayer->pev->deadflag = DEAD_RESPAWNABLE;
	m_hPlayer->m_afPhysicsFlags &= ~PFLAG_DUCKING;
	m_hPlayer->pev->flags &= ~FL_DUCKING;
	m_hPlayer->ClearTrain();
	m_hPlayer->ClearGeiger();
	m_hPlayer->m_fInitHUD = TRUE;
	m_hPlayer->DropPlayerWeapon();
	Inventory_RemoveAllFromHolder( m_hPlayer, false, false, false );
	m_hPlayer->Teleport( position, m_hPlayer->pev->angles );
	m_flNextObserverInput = 0;
	SetMode( OBS_ROAMING );
	m_fCanControlObserverMode = true;

	MESSAGE_BEGIN( MSG_ALL, gmsgSpectator );
		WRITE_BYTE( m_hPlayer->entindex() );
		WRITE_BYTE( 1 );
	MESSAGE_END();

	m_hPlayer->EnteredObserver();
	m_hPlayer->SetThink( &CBasePlayer::ObserverThink );
	m_hPlayer->pev->nextthink = gpGlobals->time + 0.1;
}

void CObserver::StopObserver( bool respawn )
{
	if ( m_hPlayer->pev->iuser1 == OBS_NONE && m_hPlayer->pev->iuser2 == 0 )
		return;

	MESSAGE_BEGIN( MSG_ALL, gmsgSpectator );
		WRITE_BYTE( m_hPlayer->entindex() );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	m_hPlayer->pev->iuser2 = 0;
	m_hPlayer->pev->iuser1 = OBS_NONE;
	m_hObserverTarget = NULL;
	m_hPlayer->pev->classname = MAKE_STRING( "player" );
	m_hPlayer->pev->takedamage = DAMAGE_YES;
	m_hPlayer->pev->movetype &= ~MOVETYPE_NOCLIP;
	m_hPlayer->pev->flags &= ~FL_NOTARGET;
	m_hPlayer->pev->effects &= ~EF_NODRAW;
	m_hPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;

	if ( HasCorpse() )
		UTIL_SetOrigin( m_hPlayer->pev, m_hDeadBody->pev->origin );

	RemoveDeadBody();

	if ( respawn )
		m_hPlayer->Spawn();

	if ( m_hPlayer->m_pActiveItem && m_hPlayer->m_pActiveItem->CanDeploy() )
		m_hPlayer->m_pActiveItem->Deploy();

	for ( int index = 0; index < MAX_AMMO_SLOTS; ++index )
		m_hPlayer->m_rgAmmoLast[index] = 0;

	m_hPlayer->UpdateClientData();
	m_hPlayer->LeftObserver();
}

void CObserver::Think( void )
{
	if ( ( m_hPlayer->pev->iuser1 == OBS_CHASE_LOCKED || m_hPlayer->pev->iuser1 == OBS_CHASE_FREE )
		&& m_hObserverTarget && m_hObserverTarget->IsAlive() && m_hObserverTarget->IsConnected() )
	{
		Vector distance = m_hObserverTarget->pev->origin - m_hPlayer->pev->origin;

		if ( distance.Length() > 256 )
			m_hPlayer->Teleport( m_hObserverTarget->pev->origin, m_hPlayer->pev->angles );
	}

	if ( ( m_hPlayer->CanSpawn() && !IsObserverCyclic() ) || !( CVAR_GET_FLOAT( "mp_observer_mode" ) > 0 ) )
	{
		m_hPlayer->m_fDeadTime = gpGlobals->time;
		m_hPlayer->m_flRespawnDelayTime = 0;

		float delay = CVAR_GET_FLOAT( "mp_respawndelay" );

		if ( delay < 0.1 )
			delay = 0;

		m_hPlayer->ShowForcedRespawnMessage( (int) delay );
		m_hPlayer->SetThink( &CBasePlayer::ForcedRespawnThink );
	}

	m_hPlayer->pev->nextthink = gpGlobals->time + 0.1;
}

// Find the next client in the game for this player to spectate
void CBasePlayer::Observer_FindNextPlayer( bool bReverse )
{
	m_pObserver->FindNextPlayer( bReverse );



	}

// Handle buttons in observer mode
void CBasePlayer::Observer_HandleButtons()
{
	m_pObserver->HandleButtons();
}

void CBasePlayer::Observer_CheckTarget()
{
	m_pObserver->CheckTarget();
}

void CBasePlayer::Observer_CheckProperties()
{
	m_pObserver->CheckProperties();
		}

// Attempt to change the observer mode
void CBasePlayer::Observer_SetMode( int iMode )
{
	m_pObserver->SetMode( iMode );
}
