#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "server_sound.h"
#include "sound_message.h"

extern int gmsgStartSound;

CServer_SoundEngine::CServer_SoundEngine()
{
	memset(m_szSentenceFile, 0, sizeof(m_szSentenceFile));
	memset(m_szMaterialsFile, 0, sizeof(m_szMaterialsFile));
	m_iLastSound = 0;
}

CServer_SoundEngine *CServer_SoundEngine::GetSoundEngine(void)
{
	static CServer_SoundEngine *pEngine;

	if (!pEngine)
		pEngine = new CServer_SoundEngine;

	return pEngine;
}

void CServer_SoundEngine::Initialize(void)
{
	m_Materials.Reset();
	memset(m_szMaterialsFile, 0, sizeof(m_szMaterialsFile));
	m_bSoundListGenerated = false;
	memset(m_Sounds, 0, sizeof(m_Sounds));
	memset(m_Sentences, 0, sizeof(m_Sentences));
	memset(m_SentenceGroups, 0, sizeof(m_SentenceGroups));
	memset(m_szSentenceFile, 0, sizeof(m_szSentenceFile));
}

int CServer_SoundEngine::LookupSoundIndex(char *sample)
{
	if (!sample)
		return -1;

	for (int i = 0; i < MAX_SOUNDLIST_SOUNDS && m_Sounds[i]; i++)
	{
		if (!strcmp(sample, m_Sounds[i]))
			return i;
	}

	ALERT(at_console, "[Sound Engine] - %s not precached!\n", sample);
	return -1;
}

void CServer_SoundEngine::PlaySound(edict_t *entity, int channel, char *sample,
	float volume, float attenuation, int flags, int pitch, int targetPlayer,
	bool useOrigin, Vector origin, float offset)
{
	if (!m_bSoundListGenerated)
		return;

	if (volume < 0 || volume > 1)
	{
		ALERT(at_console, "[Sound Engine] - PlaySound(): volume = %f  (range = 0 to 1 float)\n", volume);
		volume = V_min(1.0f, V_max(0.0f, volume));
	}

	if (!(attenuation < 0 || attenuation > 4) || (flags & SOUNDMSG_RADIUS))
	{
		if (attenuation < 0 || attenuation > 255)
		{
			ALERT(at_error, "[Sound Engine] - PlaySound(): LINEAR attenuation = %f  (range = 0 to 255 float)\n", attenuation);
			return;
		}
	}
	else
	{
		ALERT(at_console, "[Sound Engine] - PlaySound(): attenuation = %f  (range = 0 to 4 float)\n", attenuation);
		attenuation = V_min(4.0f, V_max(0.0f, attenuation));
	}

	if ((unsigned int)channel > 15)
	{
		ALERT(at_error, "[Sound Engine] - PlaySound(): channel = %i  (range = 0 to 15 int)\n", channel);
		return;
	}

	if (!sample)
		return;

	if (*sample == '*')
		sample++;

	int soundIndex;

	if (*sample == '!')
	{
		soundIndex = LookupSentenceIndex(sample + 1);
		flags |= SOUNDMSG_SENTENCE;
	}
	else
	{
		soundIndex = LookupSoundIndex(sample);
	}

	if (soundIndex == -1)
		return;

	int entityIndex = entity ? ENTINDEX(entity) : -1;

	if (!useOrigin && entity)
		origin = entity->v.origin + (entity->v.mins + entity->v.maxs) * 0.5f;

	if (targetPlayer)
	{
		CBaseEntity *player = UTIL_PlayerByIndex(targetPlayer);

		if (!player)
			return;

		MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgStartSound, origin, player->edict());
	}
	else if (channel == CHAN_STATIC || channel == CHAN_MUSIC || (flags & SOUNDMSG_STOP))
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgStartSound, origin, (edict_t *)NULL);
	}
	else
	{
		MESSAGE_BEGIN(MSG_PAS, gmsgStartSound, origin, (edict_t *)NULL);
	}

	int messageFlags = flags & (SOUNDMSG_SPAWNING | SOUNDMSG_FORCE_ONCE |
		SOUNDMSG_FORCE_LOOP | SOUNDMSG_STOP | SOUNDMSG_CHANGE_PITCH |
		SOUNDMSG_CHANGE_VOLUME | SOUNDMSG_SENTENCE | SOUNDMSG_NO_REVERB | SOUNDMSG_OFFSET);

	if (flags & SOUNDMSG_RADIUS)
		messageFlags |= SOUNDMSG_RADIUS | SOUNDMSG_ATTENUATION;

	if (entity && !(entity->v.effects & EF_NODRAW))
		messageFlags |= SOUNDMSG_FOLLOW_ENTITY;

	if (volume != 1)
		messageFlags |= SOUNDMSG_VOLUME;

	if (pitch != PITCH_NORM)
		messageFlags |= SOUNDMSG_PITCH;

	if (attenuation != ATTN_NORM)
		messageFlags |= SOUNDMSG_ATTENUATION;

	if (useOrigin || !entity || (entity->v.effects & EF_NODRAW) ||
		(attenuation != 0 && !(flags & SOUNDMSG_SKIP_ORIGIN_USE_ENT)))
		messageFlags |= SOUNDMSG_ORIGIN;

	if (messageFlags & SOUNDMSG_STOP)
	{
		messageFlags &= ~(SOUNDMSG_VOLUME | SOUNDMSG_PITCH | SOUNDMSG_ATTENUATION |
			SOUNDMSG_ORIGIN | SOUNDMSG_SPAWNING | SOUNDMSG_FORCE_ONCE | SOUNDMSG_FORCE_LOOP |
			SOUNDMSG_RADIUS | SOUNDMSG_NO_REVERB | SOUNDMSG_OFFSET);
	}

	WRITE_SHORT(messageFlags);

	if (messageFlags & SOUNDMSG_FOLLOW_ENTITY)
		WRITE_SHORT(entityIndex);

	if (messageFlags & SOUNDMSG_VOLUME)
		WRITE_BYTE((int)(volume * 255));

	if (messageFlags & SOUNDMSG_PITCH)
		WRITE_BYTE(pitch);

	if ((messageFlags & (SOUNDMSG_RADIUS | SOUNDMSG_ATTENUATION)) ==
		(SOUNDMSG_RADIUS | SOUNDMSG_ATTENUATION))
	{
		WRITE_BYTE((int)attenuation);
	}
	else if (messageFlags & SOUNDMSG_ATTENUATION)
	{
		WRITE_BYTE((int)V_min(255.0f, V_max(0.0f, attenuation * 64)));
	}

	if (messageFlags & SOUNDMSG_ORIGIN)
	{
		WRITE_COORD(origin.x);
		WRITE_COORD(origin.y);
		WRITE_COORD(origin.z);
	}

	if (messageFlags & SOUNDMSG_OFFSET)
	{
		unsigned int bits;

		memcpy(&bits, &offset, sizeof(bits));
		WRITE_BYTE(bits & 0xFF);
		WRITE_BYTE((bits >> 8) & 0xFF);
		WRITE_BYTE((bits >> 16) & 0xFF);
		WRITE_BYTE(bits >> 24);
	}

	WRITE_BYTE(channel);
	WRITE_SHORT(soundIndex);
	MESSAGE_END();
}

void PM_PlaySoundFX_SERVER(int playerIndex, float *origin, int channel,
	char *sample, float volume, float attenuation, int flags, int pitch)
{
	CBaseEntity *player = UTIL_PlayerByIndex(playerIndex);

	if (!player)
		return;

	flags |= SOUNDMSG_ORIGIN;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		if (i == playerIndex || !UTIL_PlayerByIndex(i))
			continue;

		Vector soundOrigin = player->pev->origin - Vector(0, 0, 24);

		CServer_SoundEngine::GetSoundEngine()->PlaySound(player->edict(), channel,
			sample, volume, attenuation, flags, pitch, i, true, soundOrigin, 0);
	}
}

char PM_FindMaterialType_SERVER(char *name)
{
	return CServer_SoundEngine::GetSoundEngine()->m_Materials.FindMaterialType(name);
}
