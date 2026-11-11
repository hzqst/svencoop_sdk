#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "CServer.h"
#include "server_commands.h"
#include "CServerCrypto.h"
#include "CPlayerDatabase.h"
#include "CPlayerExtrasList.h"
#include "server_support.h"
#include "user_messages.h"
#include "CVoteManager.h"
#include "game.h"
#include "CASServerManager.h"
#include "CASHook.h"
#include "CString.h"
#include <time.h>

#define TE_CUSTOM_GILDED_UZI	3
#define EXTRAS_GILDED_UZI_LEVEL	2
#define EXTRAS_REFRESH_MIN_TIME	180.0f
#define EXTRAS_REFRESH_MAX_TIME	900.0f
#define EXTRAS_BAN_TIME_SCALE	10.0f

enum
{
	SERVER_PLAYER_SLOTS = 32 + 1
};

struct ServerPlayerData
{
	extrasinfo_s   info;
	char           authId[EXTRAS_AUTHID_LENGTH + 1];
	unsigned char  key[SERVER_CRYPTO_DIGEST_SIZE];
	unsigned int   generation;
	int            serial;
	int            userId;
	float          nextRefresh;
	float          lastAttempt;
	bool           used;
	bool           keyReady;
	bool           dirty;
};

struct ServerPlayerQuery
{
	ServerPlayerQuery( const char *authId, const char *ip, float now )
	    : database( authId, ip, (long long) time( NULL ), now ), next( NULL )
	{
	}

	CPlayerDatabase    database;
	unsigned int       slot;
	unsigned int       generation;
	int                serial;
	int                userId;
	ServerPlayerQuery  *next;
};

// Player storage
class CServerState
{
public:
	CServerState();
	~CServerState();
	CBasePlayer *Player( unsigned int slot ) const;
	void ResetPlayer( unsigned int slot );
	void SendIdentity( unsigned int slot );
	void SendGildedUzis( unsigned int slot );
	void SendExtras( unsigned int source, unsigned int recipient );
	void ApplyExtras( unsigned int slot, const extrasinfo_s &info );
	void FinishQueries();
	bool HasQuery( unsigned int slot ) const;

	unsigned int       playerCount;
	unsigned int       reservedSlots;
	CVoteManager       votes;
	ServerPlayerData   players[SERVER_PLAYER_SLOTS];
	CPlayerExtrasList  local;
	CServerCrypto      crypto;
	ServerPlayerQuery  *queries;
	bool               curlReady;
	char               ipAddress[EXTRAS_IP_ADDRESS_LENGTH + 1];
};

static CServer *g_pServer;

CServerState::CServerState() : playerCount( 0 ), reservedSlots( 0 ), queries( NULL ), curlReady( false )
{
	memset( players, 0, sizeof( players ) );
	ipAddress[0] = 0;

	for ( unsigned int i = 0; i < SERVER_PLAYER_SLOTS; i++ )
		ResetPlayer( i );

	if ( !local.Load() )
		ALERT( at_error, "CServer: Unable to load the embedded player extras list.\n" );

	curlReady = CPlayerDatabase::Initialize();

	if ( !curlReady )
		ALERT( at_error, "CPlayerDatabase: cURL failed to initialize. Can't request extras.\n" );
}

CServerState::~CServerState()
{
	for ( ServerPlayerQuery *query = queries; query; query = query->next )
		query->database.Cancel();

	while ( queries )
	{
		ServerPlayerQuery *query = queries;

		queries = query->next;
		delete query;
	}

	if ( curlReady )
		CPlayerDatabase::Shutdown();
}

CBasePlayer *CServerState::Player( unsigned int slot ) const
{
	if ( !slot || slot >= SERVER_PLAYER_SLOTS || slot > (unsigned int) gpGlobals->maxClients
	     || !players[slot].used )
		return NULL;

	edict_t *entity = INDEXENT( slot );

	if ( !entity || entity->free || entity->serialnumber != players[slot].serial
	     || GETPLAYERUSERID( entity ) != players[slot].userId )
		return NULL;

	CBasePlayer *player = (CBasePlayer *) CBaseEntity::Instance( entity );

	return player && player->IsPlayer() && player->m_fConnected ? player : NULL;
}

void CServerState::ResetPlayer( unsigned int slot )
{
	ServerPlayerData &state = players[slot];
	unsigned int generation = state.generation + 1;

	memset( &state, 0, sizeof( state ) );
	state.generation = generation;
	state.info.iLevel = EXTRAS_INIT;
	state.info.flLastQuery = -1.0f;
	state.nextRefresh = -1.0f;
	state.lastAttempt = -1.0f;
}

void CServerState::SendIdentity( unsigned int slot )
{
	CBasePlayer *player = Player( slot );

	if ( !player || ( player->pev->flags & FL_FAKECLIENT ) )
		return;

	ServerPlayerData &state = players[slot];
	const char *authId = GETPLAYERAUTHID( player->edict() );
	const char *hostname = CVAR_GET_STRING( "hostname" );
	const char *map = STRING( gpGlobals->mapname );
	char plain[2048];

	if ( !authId || !*authId || !hostname || !*hostname || !map || !*map || state.userId < 1 )
		return;

	UTIL_CopyString( state.authId, authId, sizeof( state.authId ) );
	snprintf( plain, sizeof( plain ), "%u\n%s\n%u\n%.31s\n%s", slot, hostname,
	          state.userId, map, authId );
	crypto.Digest( state.key, (const unsigned char *) plain, (unsigned int) strlen( plain ) );
	state.keyReady = true;

	MESSAGE_BEGIN( MSG_ONE, gmsgServerName, NULL, player->edict() );
	WRITE_STRING( hostname );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_ONE, gmsgClServerInfo, NULL, player->edict() );
	WRITE_BYTE( slot );
	WRITE_LONG( state.userId );
	WRITE_STRING( authId );
	MESSAGE_END();
}

void CServerState::SendGildedUzis( unsigned int slot )
{
	CBasePlayer *player = Player( slot );

	if ( !player || ( player->pev->flags & FL_FAKECLIENT ) )
		return;

	MESSAGE_BEGIN( MSG_ONE, TE_CUSTOM, NULL, player->edict() );
	WRITE_BYTE( TE_CUSTOM_GILDED_UZI );
	WRITE_BYTE( players[slot].info.iLevel >= EXTRAS_GILDED_UZI_LEVEL );
	MESSAGE_END();
}

void CServerState::SendExtras( unsigned int source, unsigned int recipient )
{
	CBasePlayer *target = Player( recipient );
	const ServerPlayerData &sender = players[source];
	const ServerPlayerData &receiver = players[recipient];

	if ( !target || !receiver.keyReady || !sender.used || sender.info.iLevel < EXTRAS_BANNED
	     || !sender.info.szAuthId[0] || ( target->pev->flags & FL_FAKECLIENT ) )
		return;

	char plain[220];
	servercryptdata_t data = { 0 };

	snprintf( plain, sizeof( plain ), "%u\n%s\n\"%s\"\n\"%s\"\n%d", source, sender.info.szAuthId,
	          sender.info.szName[0] ? sender.info.szName : "Player",
	          sender.info.szDescription[0] ? sender.info.szDescription : "-", sender.info.iLevel );

	if ( crypto.Encrypt( data, (const unsigned char *) plain, (unsigned int) strlen( plain ), receiver.key, sizeof( receiver.key ) ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgClExtrasInfo, NULL, target->edict() );
		crypto.WriteCryptData( data );
		MESSAGE_END();
	}

	crypto.ClearCryptData( data );
}

void CServerState::ApplyExtras( unsigned int slot, const extrasinfo_s &info )
{
	if ( !info.szAuthId[0] || info.iLevel < EXTRAS_BANNED || info.iLevel > EXTRAS_DEVELOPER || !Player( slot ) )
		return;

	ServerPlayerData &state = players[slot];
	bool changed = state.info != info;

	state.info = info;
	state.nextRefresh = gpGlobals->time + RANDOM_FLOAT( EXTRAS_REFRESH_MIN_TIME, EXTRAS_REFRESH_MAX_TIME );
	state.dirty = state.dirty || changed;

	if ( info.iLevel == EXTRAS_BANNED && changed )
		CServer::GetInstance()->BanPlayer( Player( slot ), state.authId, RANDOM_FLOAT( EXTRAS_REFRESH_MIN_TIME, EXTRAS_REFRESH_MAX_TIME ) * EXTRAS_BAN_TIME_SCALE );
}

bool CServerState::HasQuery( unsigned int slot ) const
{
	for ( const ServerPlayerQuery *query = queries; query; query = query->next )
	{
		if ( query->slot == slot && query->generation == players[slot].generation )
			return true;
	}

	return false;
}

void CServerState::FinishQueries()
{
	ServerPlayerQuery **link = &queries;

	while ( *link )
	{
		ServerPlayerQuery *query = *link;

		if ( !query->database.IsFinished() )
		{
			link = &query->next;
			continue;
		}

		query->database.Join();
		*link = query->next;
		ServerPlayerData &state = players[query->slot];

		if ( state.generation == query->generation && state.userId == query->userId
		     && state.serial == query->serial && Player( query->slot ) )
		{
			const extrasinfo_s &result = query->database.GetResult();
			const char *currentAuthId = GETPLAYERAUTHID( Player( query->slot )->edict() );

			if ( currentAuthId && !strcmp( currentAuthId, result.szAuthId ) )
			{
				if ( result.iLevel >= EXTRAS_BANNED )
					ApplyExtras( query->slot, result );
				else if ( query->database.GetError()[0] )
					ALERT( at_error, "CPlayerDatabase: %s\n", query->database.GetError() );
			}
		}

		delete query;
	}
}

CServer::CServer() : m_pState( new CServerState ), m_flRestartTime( 0 )
{
}

CServer::~CServer()
{
	delete m_pState;
}

CServer *CServer::GetInstance()
{
	if ( !g_pServer )
		g_pServer = new CServer;

	return g_pServer;
}

void CServer::DestroyInstance()
{
	delete g_pServer;
	g_pServer = NULL;
}

void CServer::MapInit()
{
	MapEnd();
	m_flRestartTime = 0;
	m_pState->playerCount = 0;
	m_pState->votes.MapInit();
	m_pState->ipAddress[0] = 0;
}

void CServer::MapEnd()
{
	for ( ServerPlayerQuery *query = m_pState->queries; query; query = query->next )
		query->database.Cancel();

	for ( unsigned int i = 1; i < SERVER_PLAYER_SLOTS; i++ )
		m_pState->ResetPlayer( i );
}

bool CServer::ClientConnected( edict_s *client, const char *name, const char *address, char *rejectReason )
{
	if ( !CASServerManager::GetInstance()->IsScriptLifecycleEntered() )
		return true;

	CString playerName( name );
	CString playerAddress( address );
	int available = gpGlobals->maxClients - (int) m_pState->playerCount;

	if ( available <= (int) m_pState->reservedSlots )
	{
		bool allow = false;

		g_CanPlayerUseReservedSlotHook.Call( 0, client, &playerName, &playerAddress, &allow );

		if ( !allow )
		{
			UTIL_CopyString( rejectReason, "Kicked due to reserved slot", 128 );
			return false;
		}
	}

	return true;
}

void CServer::ClientPutInServer( CBasePlayer *player )
{
	unsigned int slot = player ? player->entindex() : 0;

	if ( !slot || slot >= SERVER_PLAYER_SLOTS )
		return;

	for ( ServerPlayerQuery *query = m_pState->queries; query; query = query->next )
	{
		if ( query->slot == slot )
			query->database.Cancel();
	}

	m_pState->ResetPlayer( slot );

	const char *authId = GETPLAYERAUTHID( player->edict() );

	if ( authId && !strncmp( authId, "STEAM_666:", 10 ) )
	{
		BanPlayer( player, authId, RANDOM_FLOAT( EXTRAS_REFRESH_MIN_TIME, EXTRAS_REFRESH_MAX_TIME ) * EXTRAS_BAN_TIME_SCALE );
		return;
	}

	ServerPlayerData &state = m_pState->players[slot];

	m_pState->playerCount++;
	state.used = true;
	state.serial = player->edict()->serialnumber;
	state.userId = GETPLAYERUSERID( player->edict() );
	UTIL_CopyString( state.authId, authId ? authId : "", sizeof( state.authId ) );
	m_pState->SendIdentity( slot );
}

void CServer::ClientDisconnected( edict_t *client )
{
	unsigned int slot = client ? ENTINDEX( client ) : 0;

	if ( !slot || slot >= SERVER_PLAYER_SLOTS )
		return;

	for ( ServerPlayerQuery *query = m_pState->queries; query; query = query->next )
	{
		if ( query->slot == slot )
			query->database.Cancel();
	}

	m_pState->ResetPlayer( slot );

	if ( m_pState->playerCount )
		m_pState->playerCount--;
	else
		ALERT( at_error, "More players left than have joined!\n" );
}

int CServer::UpdatePlayerExtras( CBasePlayer *player, bool force )
{
	unsigned int slot = player ? player->entindex() : 0;

	if ( !slot || slot >= SERVER_PLAYER_SLOTS || !m_pState->Player( slot ) )
		return EXTRAS_INIT;

	ServerPlayerData &state = m_pState->players[slot];
	const char *authId = GETPLAYERAUTHID( player->edict() );

	if ( !authId || !*authId || !strcmp( authId, "STEAM_ID_PENDING" ) || !strcmp( authId, "VALVE_ID_PENDING" ) )
		return EXTRAS_INIT;

	if ( strcmp( state.authId, authId ) )
	{
		for ( ServerPlayerQuery *query = m_pState->queries; query; query = query->next )
		{
			if ( query->slot == slot )
				query->database.Cancel();
		}

		state.generation++;
		state.lastAttempt = -1.0f;
		UTIL_CopyString( state.authId, authId, sizeof( state.authId ) );
		m_pState->SendIdentity( slot );
		force = true;
	}

	bool lan = CVAR_GET_FLOAT( "sv_lan" ) != 0.0f || !strcmp( authId, "4294967295" )
	           || !strcmp( authId, "VALVE_ID_LAN" ) || !strcmp( authId, "STEAM_ID_LAN" )
	           || !strcmp( authId, "VALVE_ID_LOOPBACK" ) || !strcmp( authId, "STEAM_ID_LOOPBACK" );

	if ( lan )
	{
		extrasinfo_s info = { 0 };

		UTIL_CopyString( info.szAuthId, "VALVE_ID_LAN", sizeof( info.szAuthId ) );
		UTIL_CopyString( info.szName, "LAN player", sizeof( info.szName ) );
		info.iLevel = EXTRAS_GOLD_UZI;
		info.flLastQuery = -1.0f;

		if ( state.info.iLevel == EXTRAS_INIT || force )
			m_pState->ApplyExtras( slot, info );

		return state.info.iLevel;
	}

	if ( state.info.iLevel == EXTRAS_INIT || force )
	{
		const extrasinfo_s *local = m_pState->local.Find( authId );

		if ( local )
			m_pState->ApplyExtras( slot, *local );
		else
		{
			extrasinfo_s info = { 0 };

			UTIL_CopyString( info.szAuthId, authId, sizeof( info.szAuthId ) );
			UTIL_CopyString( info.szName, "Player", sizeof( info.szName ) );
			info.iLevel = EXTRAS_NONE;
			info.flLastQuery = -1.0f;
			m_pState->ApplyExtras( slot, info );
			state.nextRefresh = gpGlobals->time + RANDOM_FLOAT( 10.0f, 30.0f );
		}

		if ( m_pState->curlReady && !( player->pev->flags & FL_FAKECLIENT )
		     && CVAR_GET_FLOAT( "sv_disable_online_api" ) == 0.0f && !m_pState->HasQuery( slot )
		     && ( state.lastAttempt < 0.0f || state.lastAttempt + 10.0f <= gpGlobals->time ) )
		{
			if ( !m_pState->ipAddress[0] )
			{
				const char *ip = GetIpAddr();

				UTIL_CopyString( m_pState->ipAddress, ip ? ip : "NULL", sizeof( m_pState->ipAddress ) );
			}

			ServerPlayerQuery *query = new ServerPlayerQuery( authId, m_pState->ipAddress, gpGlobals->time );

			query->slot = slot;
			query->generation = state.generation;
			query->serial = state.serial;
			query->userId = state.userId;
			state.lastAttempt = gpGlobals->time;

			if ( query->database.Start() )
			{
				query->next = m_pState->queries;
				m_pState->queries = query;
			}
			else
			{
				ALERT( at_error, "CPlayerDatabase: Unable to start lookup thread.\n" );
				delete query;
			}
		}
	}

	return state.info.iLevel;
}

const extrasinfo_s *CServer::GetPlayerExtras( const CBasePlayer *player ) const
{
	unsigned int slot = player ? ENTINDEX( ENT( player->pev ) ) : 0;

	return slot && slot < SERVER_PLAYER_SLOTS && m_pState->Player( slot ) ? &m_pState->players[slot].info : NULL;
}

void CServer::UpdateClientData( CBasePlayer *player, bool initializeHud )
{
	unsigned int slot = player ? player->entindex() : 0;

	if ( !slot || slot >= SERVER_PLAYER_SLOTS || !m_pState->Player( slot ) )
		return;

	ServerPlayerData &state = m_pState->players[slot];

	if ( !state.keyReady || initializeHud )
	{
		m_pState->SendIdentity( slot );
		m_pState->SendGildedUzis( slot );

		for ( unsigned int i = 1; i < SERVER_PLAYER_SLOTS; i++ )
			m_pState->SendExtras( i, slot );
	}

	UpdatePlayerExtras( player );

	if ( state.dirty )
	{
		m_pState->SendGildedUzis( slot );

		for ( unsigned int i = 1; i < SERVER_PLAYER_SLOTS; i++ )
			m_pState->SendExtras( slot, i );

		state.dirty = false;
	}
}

void CServer::Think()
{
	if ( m_flRestartTime != 0 && gpGlobals->time > m_flRestartTime + 5 )
	{
		CHANGE_LEVEL( (char *) STRING( gpGlobals->mapname ), NULL );
		return;
	}

	m_pState->votes.Think();
	m_pState->FinishQueries();
}

CVoteManager &CServer::GetVoteManager()
{
	return m_pState->votes;
}

unsigned int CServer::GetPlayerCount() const
{
	return m_pState->playerCount;
}

unsigned int CServer::GetReservedSlots() const
{
	return m_pState->reservedSlots;
}

void CServer::SetReservedSlots( unsigned int slots )
{
	unsigned int maximum = (unsigned int) gpGlobals->maxClients;

	if ( slots > maximum )
		slots = maximum;

	m_pState->reservedSlots = slots;

	unsigned int remaining = maximum - slots;

	// This converts an unsigned -1 when every slot is reserved
	CVAR_SET_FLOAT( "sv_visiblemaxplayers", (float) ( remaining ? remaining : ~0u ) );
}

bool CServer::ClientCommand( CBasePlayer *player, const char *command )
{
	return m_pState->votes.ClientCommand( player, command );
}

void CServer::KillPlayer( CBasePlayer *player, float respawnDelay )
{
	if ( !player )
		return;

	player->m_flRespawnDelayTime = respawnDelay < 0.0f ? mp_votekill_respawndelay.value : respawnDelay;

	float damage = V_max( 65535.0f, player->pev->health + 1.0f );
	entvars_t *inflictor = VARS( INDEXENT( 0 ) );
	entvars_t *attacker = VARS( INDEXENT( 0 ) );

	player->TakeDamage( attacker, inflictor, damage, DMG_ALWAYSGIB );

	if ( !player->IsAlive() )
	{
		player->Killed( VARS( INDEXENT( 0 ) ), GIB_ALWAYS );
		player->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
	}
}

bool CServer::KickPlayer( CBasePlayer *player, const char *authId, float minutes )
{
	return KickBanPlayer( player, authId, minutes == -1.0f ? mp_kickbantime.value : ( 0.0f > minutes ? 0.0f : minutes ) );
}

bool CServer::BanPlayer( CBasePlayer *player, const char *authId, float minutes )
{
	return KickBanPlayer( player, authId, minutes == -1.0f ? mp_bantime.value : ( 0.0f > minutes ? 0.0f : minutes ) );
}

bool CServer::KickBanPlayer( CBasePlayer *player, const char *authId, float minutes )
{
	if ( !authId || !*authId )
	{
		if ( !UTIL_IsPlayerConnected( player ) )
			return false;

		authId = GETPLAYERAUTHID( player->edict() );

		if ( !authId || !*authId )
			return false;
	}

	if ( UTIL_IsPlayerConnected( player ) )
		SERVER_COMMAND( UTIL_VarArgs( "kick \"#%d\"\n", GETPLAYERUSERID( player->edict() ) ) );

	float seconds = minutes == -1.0f ? -1.0f : minutes * 60.0f;

	AddPlayerBan( authId, seconds );
	SERVER_COMMAND( UTIL_VarArgs( "banid %0.1f %s\n", minutes, authId ) );
	UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs( "banid %0.1f %s\n", minutes, authId ) );
	return true;
}
