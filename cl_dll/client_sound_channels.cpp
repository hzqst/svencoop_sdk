#include "extdll.h"
#include "hud_iface.h"
#include "client_support.h"
#include "cl_entity.h"
#include "com_model.h"
#include "r_efx.h"
#include "client_sound.h"

extern vec3_t v_origin;
extern "C" int CL_IsThirdPerson(void);

void CClient_SoundEngine::AddWatchedChannel(int entityIndex, int channelIndex,
	FMOD::Channel *channel, FMOD::Sound *sound, bool trackEntity, int sentenceIndex,
	int pitch, float volume, float minDistance, float maxDistance, FMOD_VECTOR origin)
{
	sndChannel_t *entry = new sndChannel_t;

	entry->channelIndex = channelIndex;
	entry->entityIndex = entityIndex;
	entry->volume = volume;
	entry->pitch = pitch;
	entry->minDistance = minDistance;
	entry->maxDistance = maxDistance;
	entry->origin = origin;
	entry->mouthOpen = 0;
	entry->mouthTime = 0;
	entry->endTime = 0;
	entry->looping = false;
	entry->flags = trackEntity ? 0 : SNDCHAN_STATIC;
	entry->channel = channel;
	entry->sound = sound;
	entry->word = NULL;
	entry->next = NULL;

	unsigned int length = 0;

	sound->getLength(&length, FMOD_TIMEUNIT_MS);
	entry->endTime = gEngfuncs.GetClientTime() + (float)(length / 1000);

	if (sentenceIndex < 0)
	{
		FMOD_MODE mode;

		channel->getMode(&mode);
		entry->looping = (mode & FMOD_LOOP_NORMAL) != 0;
	}
	else
	{
		entry->word = m_pSentences[sentenceIndex]->words;
		entry->looping = false;
	}

	entry->next = m_pWatchedChannels;
	m_pWatchedChannels = entry;
}

sndChannel_t *CClient_SoundEngine::FindWatchedChannelByIndex(int entityIndex, int channelIndex)
{
	if (entityIndex == -1)
		return NULL;

	for (sndChannel_t *entry = m_pWatchedChannels; entry; entry = entry->next)
	{
		if (entry->entityIndex == entityIndex && (channelIndex <= 0 || entry->channelIndex == channelIndex))
			return entry;
	}

	return NULL;
}

float CClient_SoundEngine::CalculateMouthOpen(int entityIndex)
{
	sndChannel_t *entry = FindWatchedChannelByIndex(entityIndex, CHAN_VOICE);

	if (!entry || !entry->channel)
		return 0;

	bool playing = false;

	entry->channel->isPlaying(&playing);

	float delta = gEngfuncs.GetClientTime() - entry->mouthTime;
	float target = 0;
	bool update = false;

	if (playing)
	{
		if (delta < 0.04f)
			return entry->mouthOpen;

		float wave[256];

		if (entry->channel->getWaveData(wave, 256, 0) == FMOD_OK)
		{
			float total = 0;
			int count = 0;

			for (int i = 0; i < 256; i++)
			{
				if (wave[i] > 0.1f)
				{
					total += wave[i];
					count++;
				}
			}

			if (count > 0)
				total /= count;

			target = V_min(1.0f, total);
			update = true;
		}
	}

	float output = entry->mouthOpen;
	float step = delta * 3;

	if (target - output > step)
		output += step;
	else if (-step <= target - output)
		output = target;
	else
		output -= step;

	if (update)
	{
		if (m_pSpectrumDebug && m_pSpectrumDebug->value >= 1)
			gEngfuncs.Con_NPrintf(8, "Entity: %d - Level Output: %f", entityIndex, output);

		entry->mouthOpen = output;
		entry->mouthTime = gEngfuncs.GetClientTime();
	}

	return output;
}

void CClient_SoundEngine::UpdateWatchedChannels(void)
{
	sndChannel_t *previous = NULL;
	sndChannel_t *entry = m_pWatchedChannels;
	cvar_t *outline = gEngfuncs.pfnGetCvarPointer("cl_outline_sounds");
	cl_entity_t *local = gEngfuncs.GetLocalPlayer();

	m_iActiveSounds = 0;

	while (entry)
	{
		bool remove = !entry->channel || (entry->flags & SNDCHAN_STOPPED);

		if (!remove)
		{
			bool playing = true;

			entry->channel->isPlaying(&playing);
			if (!playing)
			{
				remove = true;

				if (entry->word)
				{
					entry->word = entry->word->next;

					if (entry->word)
					{
						FMOD::Sound *sound = LookupSoundBySample(entry->word->sample);

						if (sound)
						{
							StatusResult(m_pSystem->playSound(FMOD_CHANNEL_REUSE, sound, true, &entry->channel));
							entry->channel->setChannelGroup(entry->channelIndex == CHAN_MUSIC ? m_pMusicGroup : m_pSfxGroup);

							float frequency = 0;
							unsigned int length = 0;

							sound->getDefaults(&frequency, NULL, NULL, NULL);
							sound->getLength(&length, FMOD_TIMEUNIT_PCM);

							unsigned int start = (unsigned int)(entry->word->start / 100.0f * length);
							unsigned int end = (unsigned int)(entry->word->end / 100.0f * length);

							if (start)
								entry->channel->setPosition(start, FMOD_TIMEUNIT_PCM);

							if (end < length)
							{
								unsigned int hi, lo;

								m_pSystem->getDSPClock(&hi, &lo);

								float clock = (float)(44100U * end) / frequency + (float)lo;

								hi += (float)lo > clock;
								lo = (unsigned int)clock;
								entry->channel->setDelay(FMOD_DELAYTYPE_DSPCLOCK_END, hi, lo);
							}

							float pitch = entry->word->pitch / 100.0f * entry->pitch;

							frequency = 0;
							sound->getDefaults(&frequency, NULL, NULL, NULL);
							entry->channel->setFrequency(pitch / PITCH_NORM * frequency);
							entry->channel->setVolume(entry->word->volume / 100.0f * entry->volume);

							if (entry->channelIndex != CHAN_MUSIC)
							{
								FMOD_VECTOR velocity = { 0, 0, 0 };

								entry->channel->setMode(FMOD_3D_LINEARROLLOFF);
								entry->channel->set3DMinMaxDistance(entry->minDistance, entry->maxDistance);
								entry->channel->set3DAttributes(&entry->origin, &velocity);
							}

							entry->channel->setPaused(false);
							remove = false;
						}
					}
				}
			}

			if (entry->entityIndex >= 0 && !gEngfuncs.GetEntityByIndex(entry->entityIndex))
			{
				if (entry->channel)
					entry->channel->stop();

				remove = true;
			}
		}

		if (remove)
		{
			sndChannel_t *next = entry->next;

			if (previous)
				previous->next = next;
			else
				m_pWatchedChannels = next;

			delete entry;
			entry = next;
			continue;
		}

		if (entry->channel && entry->entityIndex != -1 && !(entry->flags & SNDCHAN_STATIC))
		{
			cl_entity_t *entity = gEngfuncs.GetEntityByIndex(entry->entityIndex);

			if (entity && entity->curstate.messagenum >= local->curstate.messagenum)
			{
				Vector origin;
				bool update = false;

				if (entity == gEngfuncs.GetLocalPlayer())
				{
					Vector angles, forward, right, up;

					gEngfuncs.GetViewAngles(angles);
					gEngfuncs.pfnAngleVectors(angles, forward, right, up);
					origin = entity->origin + forward * 24 + up * 24;

					float distance = (origin - v_origin).Length();

					if (CL_IsThirdPerson() && distance > 96)
						entry->channel->set3DPanLevel((distance + 128) / 96);
					else
						entry->channel->set3DPanLevel(0);

					update = true;
				}
				else if (entity->model)
				{
					origin = (Vector(entity->model->mins) + Vector(entity->model->maxs)) * 0.5f + entity->curstate.origin;

					if (outline->value > 0)
						gEngfuncs.pEfxAPI->R_FlickerParticles(origin);

					update = true;
				}

				if (update)
				{
					FMOD_VECTOR velocity = { 0, 0, 0 };

					entry->origin.x = -origin.x;
					entry->origin.y = origin.y;
					entry->origin.z = origin.z;
					entry->channel->set3DAttributes(&entry->origin, &velocity);
				}
			}
		}

		previous = entry;
		m_iActiveSounds++;
		entry = entry->next;
	}
}
