#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "server_sound.h"
#include "ambient_music.h"

#define SF_AMBIENT_MUSIC_START_SILENT 1
#define SF_AMBIENT_MUSIC_LOOPING 2
#define SF_AMBIENT_MUSIC_ACTIVATOR_ONLY 4

LINK_ENTITY_TO_CLASS( ambient_music, CAmbientMusic );

CAmbientMusic::CAmbientMusic()
	: m_flStartTime( -1 ), m_flVolume( 1 ), m_bStartSilent( false ), m_bLooping( false ), m_bActivatorOnly( false )
{
}

void CAmbientMusic::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "volume" ) )
	{
		int volume = atoi( pkvd->szValue );

		if ( volume > 10 )
			volume = 10;
		else if ( volume < 0 )
			volume = 0;

		m_flVolume = volume * 0.1f;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CAmbientMusic::Precache()
{
	CServer_SoundEngine::GetSoundEngine()->PrecacheSound( STRING( pev->message ) );
}

void CAmbientMusic::Spawn()
{
	if ( pev->spawnflags & SF_AMBIENT_MUSIC_START_SILENT )
		m_bStartSilent = true;

	if ( pev->spawnflags & SF_AMBIENT_MUSIC_LOOPING )
		m_bLooping = true;

	if ( pev->spawnflags & SF_AMBIENT_MUSIC_ACTIVATOR_ONLY )
		m_bActivatorOnly = true;

	if ( FStringNull( pev->message ) || !strlen( STRING( pev->message ) ) )
	{
		ALERT( at_error, "Empty ambient music '%s'\n", STRING( pev->targetname ) );
		pev->nextthink = gpGlobals->time + 0.1;
		SetThink( &CAmbientMusic::SUB_Remove );
		return;
	}

	Precache();
	m_Clients.clear();

	if ( !m_bStartSilent && !m_bActivatorOnly )
		Play();
}

void CAmbientMusic::UpdateOnRemove()
{
	Use( this, this, USE_OFF, 0 );
	CBaseEntity::UpdateOnRemove();
}

void CAmbientMusic::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( m_bActivatorOnly )
	{
		switch ( useType )
		{
			case USE_ON:
				PlayForActivator( activator );
				break;
			case USE_OFF:
				StopForActivator( activator );
				break;
			case USE_SET:
				if ( value == 0 )
					StopForActivator( activator );
				else
					PlayForActivator( activator, value );
				break;
			case USE_TOGGLE:
				if ( m_flStartTime >= 0 )
					StopForActivator( activator );
				else
					PlayForActivator( activator );
				break;
			case USE_KILL:
				StopForActivator( activator );
				UTIL_Remove( this );
				break;
		}
	}
	else
	{
		switch ( useType )
		{
			case USE_ON:
				Play();
				break;
			case USE_OFF:
				Stop();
				break;
			case USE_SET:
				if ( value == 0 )
					Stop();
				else
					Play( value );
				break;
			case USE_TOGGLE:
				if ( m_flStartTime >= 0 )
					Stop();
				else
					Play();
				break;
			case USE_KILL:
				Stop();
				UTIL_Remove( this );
				break;
		}
	}
}

void CAmbientMusic::Play( float offset )
{
	if ( m_bActivatorOnly )
		return;

	if ( m_flStartTime >= 0 )
		Stop();

	m_flStartTime = gpGlobals->time;
	m_Clients.clear();
	UpdateClients( offset );
	SetThink( &CAmbientMusic::Think );
	pev->nextthink = pev->ltime + 1.0f;
}

void CAmbientMusic::Stop()
{
	if ( m_bActivatorOnly )
		return;

	m_flStartTime = -1;
	NotifyClient( false, 0, 0 );
	m_Clients.clear();
	SetThink( NULL );
	pev->nextthink = pev->ltime + 0.1f;
}

void CAmbientMusic::PlayForActivator( CBaseEntity *activator, float offset )
{
	if ( !activator || !activator->IsPlayer() )
		return;

	CBasePlayer *player = dynamic_cast<CBasePlayer *>( activator );

	if ( !CheckClient( player ) )
	{
		edict_t *recipient = NULL;
		if ( player->pev )
			recipient = player->edict();

		NotifyClient( true, ENTINDEX( recipient ), offset );
	}
}

void CAmbientMusic::StopForActivator( CBaseEntity *activator )
{
	if ( !activator || !activator->IsPlayer() )
		return;

	CBasePlayer *player = dynamic_cast<CBasePlayer *>( activator );

	if ( !CheckClient( player ) )
	{
		edict_t *recipient = NULL;
		if ( player->pev )
			recipient = player->edict();

		NotifyClient( false, ENTINDEX( recipient ), 0 );
	}
}

void CAmbientMusic::Think()
{
	UpdateClients();
	SetThink( &CAmbientMusic::Think );
	pev->nextthink = pev->ltime + 1.0f;
}

void CAmbientMusic::NotifyClient( bool playing, int client, float offset )
{
	int flags;

	if ( playing )
	{
		flags = SOUNDMSG_NO_REVERB;

		if ( offset != 0 )
			flags |= SOUNDMSG_OFFSET;

		if ( m_bLooping )
			flags |= SOUNDMSG_FORCE_LOOP;
	}
	else
	{
		flags = SOUNDMSG_STOP;
		client = 0;
		offset = 0;
	}

	CServer_SoundEngine::GetSoundEngine()->PlaySound( edict(), CHAN_MUSIC, (char *) STRING( pev->message ),
	                                                  m_flVolume, 0, flags, PITCH_NORM, client, false, g_vecZero, offset );
}

void CAmbientMusic::UpdateClients( float offset )
{
	if ( !( m_flStartTime >= 0 ) )
		return;

	for ( int index = 1; index <= gpGlobals->maxClients; index++ )
	{
		CBasePlayer *player = (CBasePlayer *) UTIL_PlayerByIndex( index );

		if ( !player )
			continue;

		EntityHandle<CBasePlayer> handle( player );
		ClientMap::iterator found = m_Clients.find( handle );

		if ( found != m_Clients.end() && found->second == player->m_flLastClientPutInServerTime )
			continue;

		if ( gpGlobals->time >= player->m_flLastClientPutInServerTime + 2.0f )
		{
			m_Clients[handle] = player->m_flLastClientPutInServerTime;

			float playbackOffset = offset;

			if ( playbackOffset == 0 )
				playbackOffset = gpGlobals->time - m_flStartTime;

			NotifyClient( true, index, playbackOffset );
		}
	}
}

bool CAmbientMusic::CheckClient( CBasePlayer *player )
{
	EntityHandle<CBasePlayer> handle( player );
	ClientMap::iterator found = m_Clients.find( handle );

	if ( found != m_Clients.end() && found->second == player->m_flLastClientPutInServerTime )
		return true;

	m_Clients[handle] = player->m_flLastClientPutInServerTime;
	return false;
}
