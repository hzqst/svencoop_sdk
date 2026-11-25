#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "game.h"
#include "server_support.h"
#include "CSurvivalMode.h"
#include "CCommand.h"
#include "CConCommand.h"
#include "CServer.h"
#include "CVoteManager.h"

#define SURVIVAL_STATE_INACTIVE		0
#define SURVIVAL_STATE_ACTIVATING	1
#define SURVIVAL_STATE_ACTIVE		2

#define SURVIVAL_RETRIES_UNSET		-2

CSurvivalMode::CSurvivalMode()
	: m_iVoteState( -1 ), m_iRetriesLeft( SURVIVAL_RETRIES_UNSET )
{
	memset( m_szPreviousMap, 0, sizeof( m_szPreviousMap ) );
	memset( m_szCurrentMap, 0, sizeof( m_szCurrentMap ) );
	memset( m_szLastNextMap, 0, sizeof( m_szLastNextMap ) );

	ResetSettings();
}

CSurvivalMode::~CSurvivalMode()
{
}

void CSurvivalMode::Initialize( void )
{
	ResetSettings();
	m_flEndVoteDelay = 10.0f;
	m_flChangeLevelDelay = 10.0f;
}

void CSurvivalMode::Shutdown( void )
{

}

void CSurvivalMode::ResetSettings( void )
{
	m_bEnabled = false;
	m_bStartOn = true;
	m_iState = SURVIVAL_STATE_INACTIVE;
	m_bRoundEnding = false;
	m_flActivationTime = 0;
	m_flUnknownTime = 0;
	m_flEndRoundCheckTime = 0;
	m_flEndVoteTime = 0;
	m_flCountdownTime = 0;
	m_flChangeLevelTime = 0;
}

void CSurvivalMode::PreMapInit( void )
{
	UTIL_CopyString( m_szPreviousMap, m_szCurrentMap, sizeof( m_szPreviousMap ) );
	UTIL_CopyString( m_szCurrentMap, STRING( gpGlobals->mapname ), sizeof( m_szCurrentMap ) );

	m_iState = SURVIVAL_STATE_INACTIVE;
	m_bEnabled = false;
	m_bRoundEnding = false;
}

void CSurvivalMode::PostMapInit( bool success )
{
	if ( stricmp( m_szPreviousMap, m_szCurrentMap ) )
		m_iRetriesLeft = SURVIVAL_RETRIES_UNSET;
}

void CSurvivalMode::PostMapActivate( bool success )
{
	int allowed = GetRetriesAllowed();

	if ( m_iRetriesLeft == SURVIVAL_RETRIES_UNSET || m_iRetriesLeft > allowed || m_iRetriesLeft < 0 )
		m_iRetriesLeft = allowed;

	if ( stricmp( m_szPreviousMap, m_szCurrentMap ) )
	{
		if ( stricmp( m_szLastNextMap, m_szCurrentMap ) )
			ResetVoteState();

		UTIL_CopyString( m_szLastNextMap, CVAR_GET_STRING( mp_survival_nextmap.name ), sizeof( m_szLastNextMap ) );
	}

	if ( mp_survival_starton.value == 0.0f )
		m_bStartOn = false;

	UpdateCheckpoints( false );
	Enable();
}

bool CSurvivalMode::IsEnabled( void ) const
{
	return m_bEnabled;
}

void CSurvivalMode::Enable( bool activateNow )
{
	if ( !MapSupportEnabled() )
		return;

	if ( mp_survival_mode.value == 0.0f )
	{
		if ( m_iVoteState != 1 )
			return;
	}
	else if ( m_iVoteState == 0 )
		return;

	if ( m_bEnabled )
		return;

	m_bEnabled = true;

	UTIL_ClientPrintAll( HUD_PRINTTALK, "Survival mode enabled.\n" );
	CheckIfActivationNeeded( activateNow );
}

void CSurvivalMode::Disable( void )
{
	if ( !m_bEnabled )
		return;

	m_bEnabled = false;

	Reset();
	UTIL_ClientPrintAll( HUD_PRINTTALK, "Survival mode disabled.\n" );
}

void CSurvivalMode::Toggle( void )
{
	if ( m_bEnabled )
		Disable();
	else
		Enable( true );
}

bool CSurvivalMode::IsActive( void ) const
{
	return m_iState == SURVIVAL_STATE_ACTIVE;
}

void CSurvivalMode::Activate( bool activateNow )
{
	if ( m_bStartOn )
		return;

	m_bStartOn = true;
	CheckIfActivationNeeded( activateNow );
}

int CSurvivalMode::GetVoteState( void ) const
{
	return m_iVoteState;
}

void CSurvivalMode::VoteToggle( void )
{
	m_iVoteState = m_bEnabled ? 0 : 1;

	Toggle();
}

void CSurvivalMode::ResetVoteState( void )
{
	m_iVoteState = -1;
}

void CSurvivalMode::EnableMapSupport( void )
{
	CVAR_SET_FLOAT( mp_survival_supported.name, 1 );
}

bool CSurvivalMode::MapSupportEnabled( void ) const
{
	return mp_survival_supported.value != 0.0f;
}

void CSurvivalMode::SetStartOn( bool value )
{
	m_bStartOn = value;
}

bool CSurvivalMode::GetStartOn( void ) const
{
	return m_bStartOn;
}

CString CSurvivalMode::GetNextMap( void ) const
{
	return CString( mp_survival_nextmap.string );
}

void CSurvivalMode::SetNextMap( const CString &mapName )
{
	CVAR_SET_STRING( mp_survival_nextmap.name, mapName.Data() );
}

float CSurvivalMode::GetDelayBeforeStart( void ) const
{
	return mp_survival_startdelay.value;
}

void CSurvivalMode::SetDelayBeforeStart( float delay )
{
	CVAR_SET_FLOAT( mp_survival_startdelay.name, fmaxf( delay, 0.0f ) );
}

int CSurvivalMode::GetMinPlayersRequired( void ) const
{
	return (int)mp_survival_minplayers.value;
}

void CSurvivalMode::SetMinPlayersRequired( int count )
{
	CVAR_SET_FLOAT( mp_survival_minplayers.name, (float) V_max( count, 0 ) );
}

bool CSurvivalMode::HasInfiniteRetries( void ) const
{
	return mp_survival_retries.value == -1.0f;
}

int CSurvivalMode::GetRetriesAllowed( void ) const
{
	// ScriptedSnark: not sure if it's right
	float allowed = HasInfiniteRetries() ? 4294967295.0f : mp_survival_retries.value;

	return (int) allowed;
}

int CSurvivalMode::GetRetriesLeft( void ) const
{
	return m_iRetriesLeft;
}

void CSurvivalMode::Reset( void )
{
	CVAR_SET_FLOAT( mp_observer_mode.name, 0 );
	CVAR_SET_FLOAT( mp_observer_cyclic.name, 0 );

	if ( m_flEndRoundCheckTime > 0.0f )
		m_flEndRoundCheckTime = 0;

	m_iState = SURVIVAL_STATE_INACTIVE;
	m_flActivationTime = 0;

	UTIL_RespawnAllPlayers( false, true );
	UpdateCheckpoints( false );
}

void CSurvivalMode::UpdateCheckpoints( bool active )
{
	CBaseEntity *entity = NULL;

	while ( ( entity = UTIL_FindEntityByClassname( entity, "point_checkpoint" ) ) != NULL )
	{
		if ( active && entity->pev->frags == 0.0f )
			entity->pev->effects &= ~EF_NODRAW;
		else
			entity->pev->effects |= EF_NODRAW;

		entity->pev->health = active ? 1.0f : 0.0f;
	}
}

int CSurvivalMode::GetLivingPlayersCount( void ) const
{
	int living = 0;

	for ( int index = 1; index <= gpGlobals->maxClients; index++ )
	{
		CBaseEntity *player = UTIL_PlayerByIndex( index );

		if ( player && player->IsAlive() )
			living++;
	}

	return living;
}

void CSurvivalMode::StartEndVote( void )
{
	m_EndVote.Start();
}

void CSurvivalMode::VoteBlocked( float time )
{
	m_flEndVoteTime = time;
}

void CSurvivalMode::VoteEnd( bool result, int voters )
{
	ScheduleLevelChange( result );
}

const char *CSurvivalMode::GetRestartMapName( void ) const
{
	if ( m_bRestartLevel )
		return m_szCurrentMap;

	const char *nextMap = CVAR_GET_STRING( mp_survival_nextmap.name );

	if ( !nextMap || !*nextMap )
		nextMap = CVAR_GET_STRING( "mp_nextmap_cycle" );

	return nextMap;
}

void CSurvivalMode::PerformChangeLevel( const char *mapName )
{
	CHANGE_LEVEL( (char *) mapName, NULL );
}

void CSurvivalMode::ScheduleActivation( void )
{
	if ( m_iState != SURVIVAL_STATE_ACTIVATING )
		return;

	float remaining = m_flActivationTime - gpGlobals->time;
	int seconds = (int) ceil( (double) remaining );

	UTIL_ClientPrintAll( HUD_PRINTCENTER, UTIL_VarArgs( "Survival mode starting in %d seconds", seconds ) );
	m_flCountdownTime = V_min( remaining, 1.0f ) + gpGlobals->time;
}

void CSurvivalMode::ScheduleLevelChange( bool restart )
{
	m_bRestartLevel = restart;

	const char *message;
	int delay = (int) m_flChangeLevelDelay;

	if ( restart )
	{
		if ( HasInfiniteRetries() || (int) mp_survival_retries.value <= 0 )
			message = UTIL_VarArgs( "Restarting the map in %d seconds.\n", delay );
		else
			message = UTIL_VarArgs( "Restarting the map in %d seconds. %d %s left.\n",
				delay, m_iRetriesLeft, m_iRetriesLeft == 1 ? "retry" : "retries" );
	}
	else
		message = UTIL_VarArgs( "Changing map to \"%s\" in %d seconds.\n", GetRestartMapName(), delay );

	UTIL_ClientPrintAll( HUD_PRINTTALK, message );
	m_flChangeLevelTime = gpGlobals->time + m_flChangeLevelDelay;
}

bool CSurvivalMode::RetriesDepleted( void ) const
{
	return !HasInfiniteRetries() && m_iRetriesLeft == 0;
}

void CSurvivalMode::EndRound( void )
{
	if ( m_bRoundEnding )
		return;

	m_bRoundEnding = true;

	if ( RetriesDepleted() )
		StartEndVote();
	else
		ScheduleLevelChange( true );
}

void CSurvivalMode::PreEndRoundCheck( void )
{
	m_flEndRoundCheckTime = 0;

	if ( GetLivingPlayersCount() )
	{
		if ( RetriesDepleted() )
			UTIL_ClientPrintAll( HUD_PRINTTALK, "Living player(s) detected. Map vote aborted.\n" );

		return;
	}

	EndRound();
}

void CSurvivalMode::CheckEndConditions( void )
{
	if ( m_iState != SURVIVAL_STATE_ACTIVE || m_bRoundEnding || GetLivingPlayersCount() || m_flEndRoundCheckTime != 0.0f )
		return;

	const char *message = "No living players left.\n";

	if ( RetriesDepleted() )
		message = UTIL_VarArgs( "No living players left. Starting end vote in %d seconds.\n", (int) m_flEndVoteDelay );

	UTIL_ClientPrintAll( HUD_PRINTTALK, message );
	m_flEndRoundCheckTime = gpGlobals->time + m_flEndVoteDelay;
}

void CSurvivalMode::ActivateNow( void )
{
	CVAR_SET_FLOAT( mp_observer_mode.name, 1 );
	CVAR_SET_FLOAT( mp_observer_cyclic.name, 1 );

	m_flActivationTime = 0;
	m_iState = SURVIVAL_STATE_ACTIVE;

	UTIL_ClientPrintAll( HUD_PRINTTALK, "Survival mode now active. No more respawning allowed.\n" );
	CheckEndConditions();
	UpdateCheckpoints( true );
}

bool CSurvivalMode::CheckIfActivationNeeded( bool activateNow )
{
	if ( !m_bEnabled || !m_bStartOn || m_iState != SURVIVAL_STATE_INACTIVE )
		return false;

	if ( mp_survival_minplayers.value > (float) CServer::GetInstance()->GetPlayerCount() || m_flActivationTime > 0.0f )
		return false;

	if ( activateNow || mp_survival_startdelay.value <= 0.0f )
	{
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "Survival mode now active." );
		ActivateNow();
	}
	else
	{
		m_iState = SURVIVAL_STATE_ACTIVATING;
		m_flActivationTime = fmaxf( 0.0f, mp_survival_startdelay.value ) + gpGlobals->time;
		ScheduleActivation();
	}

	return true;
}

void CSurvivalMode::ClientPutInServer( void )
{
	CheckIfActivationNeeded( false );
}

void CSurvivalMode::ClientDisconnect( void )
{
	unsigned int count = CServer::GetInstance()->GetPlayerCount();

	if ( count )
	{
		if ( !( (float) (int) count >= mp_survival_minplayers.value ) )
			Reset();
		else
			CheckEndConditions();
	}
	else if ( m_iState == SURVIVAL_STATE_ACTIVE && !m_bRoundEnding )
	{
		m_bRoundEnding = true;
		ScheduleLevelChange( true );
	}
}

void CSurvivalMode::PlayerKilled( void )
{
	CheckEndConditions();
}

void CSurvivalMode::RetryUsed( void )
{
	if ( m_iRetriesLeft > 0 )
		m_iRetriesLeft--;
}

void CSurvivalMode::PreChangeLevelCheck( void )
{
	if ( GetLivingPlayersCount() )
	{
		m_bRoundEnding = false;
		UTIL_ClientPrintAll( HUD_PRINTTALK, m_bRestartLevel
			? "Living player(s) detected. Map restart aborted.\n"
			: "Living player(s) detected. Map change aborted.\n" );
		return;
	}

	if ( m_bRestartLevel && !RetriesDepleted() )
		RetryUsed();

	PerformChangeLevel( GetRestartMapName() );
}

void CSurvivalMode::Think( void )
{
	if ( !m_bEnabled )
		return;

	if ( m_flActivationTime > 0.0f && gpGlobals->time >= m_flActivationTime )
	{
		m_flActivationTime = 0;
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "" );
		ActivateNow();
	}

	if ( m_flEndRoundCheckTime > 0.0f && gpGlobals->time >= m_flEndRoundCheckTime )
	{
		m_flEndRoundCheckTime = 0;
		PreEndRoundCheck();
	}

	if ( m_flEndVoteTime > 0.0f && gpGlobals->time >= m_flEndVoteTime )
	{
		m_flEndVoteTime = 0;
		StartEndVote();
	}

	if ( m_flCountdownTime > 0.0f && gpGlobals->time >= m_flCountdownTime )
	{
		m_flCountdownTime = 0;
		ScheduleActivation();
	}

	if ( m_flChangeLevelTime > 0.0f && gpGlobals->time >= m_flChangeLevelTime )
	{
		m_flChangeLevelTime = 0;
		PreChangeLevelCheck();
	}
}

void CSurvivalEndVote::Start( void )
{
	CServer::GetInstance()->GetVoteManager().StartCustomVote( this );
}

void CSurvivalEndVote::VoteBlocked( float time )
{
	CSurvivalMode::GetInstance()->VoteBlocked( time );
}

void CSurvivalEndVote::VoteEnd( bool result, int voters )
{
	CSurvivalMode::GetInstance()->VoteEnd( result, voters );
}

const char *CSurvivalEndVote::GetName( void ) const
{
	return "End map vote";
}

float CSurvivalEndVote::GetVoteLength( void ) const
{
	return 15.0f;
}

const char *CSurvivalEndVote::GetVoteMessage( void ) const
{
	return "Choose to restart the map or go to the next one.";
}

float CSurvivalEndVote::GetPercentageNeeded( void ) const
{
	return 50.0f;
}

const char *CSurvivalEndVote::GetYesText( void ) const
{
	return "Restart map";
}

const char *CSurvivalEndVote::GetNoText( void ) const
{
	return "Go to next map";
}

void CSurvivalEndVote::AddRef( void ) const
{
}

void CSurvivalEndVote::Release( void ) const
{
}

void toggle_survival_mode_Callback( const CCommand &command )
{
	CSurvivalMode::GetInstance()->VoteToggle();
}

CConCommand toggle_survival_mode( "toggle_survival_mode", toggle_survival_mode_Callback, ConCommandFlag::AdminOnly, "Toggles Survival Mode" );
