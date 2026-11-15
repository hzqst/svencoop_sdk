#include "extdll.h"
#include "hud_iface.h"
#include "client_support.h"
#include "cl_entity.h"
#include "com_model.h"
#include "r_studioint.h"
#include "IClient.h"
#include "client_sound.h"

void AlertMessage(ALERT_TYPE type, char *format, ...);
extern engine_studio_api_t IEngineStudio;
extern vec3_t v_origin;
extern "C" int CL_IsThirdPerson(void);

void CClient_SoundEngine::PlayFMODSound(int messageFlags, int entityIndex,
	const float *origin, int channelIndex, const char *sample, float volume,
	float attenuation, int flags, int pitch, int sentenceIndex, float offset)
{
	if (!m_bSoundListLoaded || !m_bInitialized)
		return;

	if (channelIndex == 0)
	{
		for (int i = CHAN_WEAPON; i <= CHAN_STATIC; i++)
		{
			if (!FindWatchedChannelByIndex(entityIndex, i))
			{
				channelIndex = i;
				break;
			}
		}
	}
	else if ((unsigned int)channelIndex > CHAN_MUSIC)
	{
		channelIndex = CHAN_VOICE;
	}

	if (messageFlags & SOUNDMSG_STOP)
	{
		sndChannel_t *entry = FindWatchedChannelByIndex(entityIndex, channelIndex);

		if (entry && entry->channel)
			entry->channel->stop();

		return;
	}

	bool sentence = (messageFlags & SOUNDMSG_SENTENCE) != 0;
	const char *name = sentence ? LookupSoundBySentenceIndex(sentenceIndex) : sample;

	if (!name)
		return;

	FMOD::Sound *sound = LookupSoundBySample(name);

	if (!sound)
		return;

	if ((messageFlags & (SOUNDMSG_SPAWNING | SOUNDMSG_FORCE_LOOP)) == SOUNDMSG_SPAWNING)
	{
		FMOD_MODE mode;

		sound->getMode(&mode);
		if (mode & FMOD_LOOP_OFF)
			return;
	}

	sndChannel_t *previous = FindWatchedChannelByIndex(entityIndex, channelIndex);

	if (previous && previous->channel)
	{
		int changes = flags | messageFlags;

		if (changes & (SOUNDMSG_CHANGE_VOLUME | SOUNDMSG_CHANGE_PITCH))
		{
			if (changes & SOUNDMSG_CHANGE_VOLUME)
				previous->channel->setVolume(volume);

			if (changes & SOUNDMSG_CHANGE_PITCH)
			{
				float frequency = 0;

				previous->sound->getDefaults(&frequency, NULL, NULL, NULL);
				previous->channel->setFrequency((float)pitch / PITCH_NORM * frequency);
			}

			bool playing = false;

			previous->channel->isPlaying(&playing);
			if (previous->sound == sound && playing)
				return;
		}

		previous->channel->stop();
		previous->flags |= SNDCHAN_STOPPED;
	}

	FMOD::Channel *channel;

	StatusResult(m_pSystem->playSound(FMOD_CHANNEL_FREE, sound, true, &channel));
	channel->setChannelGroup(channelIndex == CHAN_MUSIC ? m_pMusicGroup : m_pSfxGroup);

	if (messageFlags & SOUNDMSG_FORCE_ONCE)
		channel->setMode(FMOD_LOOP_OFF);
	else if (messageFlags & SOUNDMSG_FORCE_LOOP)
		channel->setMode(FMOD_LOOP_NORMAL);

	FMOD_VECTOR velocity = { 0, 0, 0 };
	FMOD_VECTOR position = { 0, 0, 0 };
	bool trackEntity = false;
	cl_entity_t *entity = gEngfuncs.GetEntityByIndex(entityIndex);

	if (channelIndex != CHAN_MUSIC)
	{
		bool localSound = entity &&
			(entity == IEngineStudio.GetViewEntity() || entity == gEngfuncs.GetLocalPlayer());

		if (!localSound)
		{
			localSound = attenuation <= 0;
			if (!localSound)
				localSound = (Vector(origin[0], origin[1], origin[2]) - gEngfuncs.GetLocalPlayer()->origin).Length() < 5;
		}

		if (localSound)
		{
			channel->setPriority(48);

			Vector soundOrigin(0, 0, 0);
			Vector angles(0, 0, 0), forward, right, up;

			if (entity)
				soundOrigin = entity->origin;

			gEngfuncs.GetViewAngles(angles);
			gEngfuncs.pfnAngleVectors(angles, forward, right, up);
			soundOrigin = forward * 16 + soundOrigin + up * 16;
			position.x = -soundOrigin.x;
			position.y = soundOrigin.y;
			position.z = soundOrigin.z;
			trackEntity = true;

			float distance = (soundOrigin - v_origin).Length();

			if (CL_IsThirdPerson() && distance > 96 && attenuation != 0)
				channel->set3DPanLevel((distance + 128) / 96);
			else
				channel->set3DPanLevel(0);
		}
		else if ((messageFlags & SOUNDMSG_ORIGIN) || !entity)
		{
			position.x = -origin[0];
			position.y = origin[1];
			position.z = origin[2];
			trackEntity = (messageFlags & SOUNDMSG_FOLLOW_ENTITY) != 0;

			if (entity && !entity->model)
			{
				cl_entity_t *local = gEngfuncs.GetLocalPlayer();

				if (!local)
					trackEntity = false;
				else if (entity->curstate.messagenum < local->curstate.messagenum)
					trackEntity = true;
			}
		}
		else if (entity->model)
		{
			Vector center = (Vector(entity->model->mins) + Vector(entity->model->maxs)) * 0.5f + entity->curstate.origin;

			position.x = -center.x;
			position.y = center.y;
			position.z = center.z;
			trackEntity = true;
		}
		else
		{
			AlertMessage(at_console, "Sound with no origin: %s \n", sample);
		}

		channel->set3DAttributes(&position, &velocity);
	}

	float frequency = 0;
	float sentenceVolume = volume;
	int sentencePitch = pitch;

	sound->getDefaults(&frequency, NULL, NULL, NULL);

	if (sentence)
	{
		sndWord_t *word = m_pSentences[sentenceIndex]->words;

		if (!word)
			return;

		unsigned int length = 0;

		sound->getLength(&length, FMOD_TIMEUNIT_PCM);

		unsigned int start = (unsigned int)(word->start / 100.0f * length);
		unsigned int end = (unsigned int)(word->end / 100.0f * length);

		if (start)
			channel->setPosition(start, FMOD_TIMEUNIT_PCM);

		if (end < length)
		{
			unsigned int hi, lo;

			m_pSystem->getDSPClock(&hi, &lo);

			float clock = (float)(44100U * end) / frequency + (float)lo;

			hi += (float)lo > clock;
			lo = (unsigned int)clock;
			channel->setDelay(FMOD_DELAYTYPE_DSPCLOCK_END, hi, lo);
		}

		pitch = (int)(word->pitch / 100.0f * pitch);
		volume = word->volume / 100.0f * volume;
	}

	if ((messageFlags & SOUNDMSG_OFFSET) && offset != 0)
	{
		FMOD_MODE mode;
		unsigned int length = 0;
		unsigned int start = (unsigned int)(offset * 1000);

		channel->getMode(&mode);
		sound->getLength(&length, FMOD_TIMEUNIT_MS);

		if (start >= length)
		{
			if (!(mode & FMOD_LOOP_NORMAL))
				return;

			start %= length;
		}

		channel->setPosition(start, FMOD_TIMEUNIT_MS);
	}

	if (pitch != PITCH_NORM)
		channel->setFrequency((float)pitch / PITCH_NORM * frequency);

	StatusResult(channel->setVolume(volume));

	float minimum = 64;
	float maximum = 1024;

	if (channelIndex == CHAN_MUSIC)
	{
		AlertMessage(at_console, "Playing music sample '%s', offset %f.\n", sample, offset);
		channel->setMode(FMOD_2D);
	}
	else if (messageFlags & SOUNDMSG_RADIUS)
	{
		minimum = (float)(((int)attenuation & 15) << 8);
		maximum = (float)((((int)attenuation >> 4) << 8) + 32);
		channel->setMode(FMOD_3D_LINEARROLLOFF);
		channel->set3DMinMaxDistance(minimum, maximum);
	}
	else if (attenuation <= 0)
	{
		minimum = 10000;
		maximum = 10000;
		channel->set3DMinMaxDistance(minimum, maximum);
	}
	else
	{
		maximum = 4.0f / attenuation * 256.0f;
		StatusResult(channel->setMode(FMOD_3D_LINEARROLLOFF));
		StatusResult(channel->set3DMinMaxDistance(minimum, maximum));
	}

	if (messageFlags & SOUNDMSG_NO_REVERB)
	{
		FMOD_REVERB_CHANNELPROPERTIES properties = {};
		FMOD_RESULT result = channel->getReverbProperties(&properties);

		if (result != FMOD_OK)
			StatusResult(result);
		else
		{
			properties.Room = -10000;
			channel->setReverbProperties(&properties);
		}
	}

	StatusResult(channel->setPaused(false));

	if (sentence)
		AddWatchedChannel(entityIndex, channelIndex, channel, sound, trackEntity,
			sentenceIndex, sentencePitch, sentenceVolume, minimum, maximum, position);
	else
	{
		FMOD_VECTOR zero = { 0, 0, 0 };

		AddWatchedChannel(entityIndex, channelIndex, channel, sound, trackEntity,
			-1, PITCH_NORM, 1, 64, 1024, zero);
	}
}

void CClient_SoundEngine::Play2DSound(int index, float volume)
{
	float origin[3] = { 0, 0, 0 };

	PlayFMODSound(0, 0, origin, CHAN_AUTO, LookupSoundByIndex(index), volume, 0, 0, PITCH_NORM, -1, 0);
}

void CClient_SoundEngine::Play2DSound(const char *sample, float volume)
{
	float origin[3] = { 0, 0, 0 };

	PlayFMODSound(0, 0, origin, CHAN_AUTO, sample, volume, 0, 0, PITCH_NORM, -1, 0);
}

void CL_Play2DSound(const char *szSound, float vol)
{
	CClient_SoundEngine::GetSoundEngine()->Play2DSound(szSound, vol);
}

extern "C" void PM_PlaySoundFX_CLIENT(int playerIndex, float *origin, int channelIndex, char *sample,
	float volume, float attenuation, int flags, int pitch)
{
	CClient_SoundEngine::GetSoundEngine()->PlayFMODSound(0, playerIndex + 1, origin,
		channelIndex, sample, volume, attenuation, flags, pitch, -1, 0);
	g_pEngineClient->QueueDemoSound(origin, channelIndex, sample, volume, attenuation, flags, pitch);
}
