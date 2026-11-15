#include <fmodex/fmod_errors.h>

#include "extdll.h"
#include "hud_iface.h"
#include "client_support.h"
#include "client_sound.h"
#include "client_filesystem.h"

void AlertMessage(ALERT_TYPE type, char *format, ...);

const char *CClient_SoundEngine::LookupSoundByIndex(int index)
{
	if ((unsigned int)index >= MAX_SOUNDLIST_SOUNDS)
	{
		AlertMessage(at_aiconsole, "Tried to look up sound by index out of bounds.\n");
		return NULL;
	}

	return m_szSoundNames[index];
}

int CClient_SoundEngine::LookupSoundIndexByName(const char *name)
{
	if (!name || !name[0])
	{
		AlertMessage(at_aiconsole, "Tried to look up sound index by name without specifying a file name.\n");
		return -1;
	}

	for (int i = 0; i < MAX_SOUNDLIST_SOUNDS; i++)
	{
		if (!strcmp(m_szSoundNames[i], name))
			return i;
	}

	return -1;
}

FMOD::Sound *CClient_SoundEngine::LookupSoundBySample(const char *sample)
{
	if (!sample || !sample[0])
	{
		AlertMessage(at_aiconsole, "Tried to look up sound by sample without specifying a file name.\n");
		return NULL;
	}

	int index;

	for (index = 0; index < MAX_SOUNDLIST_SOUNDS; index++)
	{
		if (!m_pSoundCache[index])
			break;

		if (!strcmp(m_pSoundCache[index]->name, sample))
			return m_pSoundCache[index]->sound;
	}

	FMOD::Sound *sound = LoadSound(sample, true);

	if (!sound)
		return NULL;

	if (index == MAX_SOUNDLIST_SOUNDS)
	{
		AlertMessage(at_console, "Can't play %s, not loaded in memory. Sound slots exhausted (max of %i).\n", sample, MAX_SOUNDLIST_SOUNDS);
		sound->release();
		return NULL;
	}

	m_pSoundCache[index] = new sndCache_t;
	m_pSoundCache[index]->sound = sound;
	UTIL_CopyString(m_pSoundCache[index]->name, sample, sizeof(m_pSoundCache[index]->name));
	return sound;
}

void CClient_SoundEngine::OggPcmDataReinitialize(FMOD::Sound **sound, const char *sample)
{
	if (!sound)
		return;

	FMOD::Sound *original = *sound;
	FMOD_SOUND_TYPE type;
	int channels = 0;

	if (original->getFormat(&type, NULL, &channels, NULL) != FMOD_OK || type != FMOD_SOUND_TYPE_OGGVORBIS)
		return;

	unsigned int length = 0;
	unsigned int length1 = 0;
	unsigned int length2 = 0;
	float frequency = 0;
	void *data1 = NULL;
	void *data2 = NULL;

	original->getLength(&length, FMOD_TIMEUNIT_PCMBYTES);
	original->getDefaults(&frequency, NULL, NULL, NULL);
	original->lock(0, length, &data1, &data2, &length1, &length2);

	void *data = malloc(length1);

	memmove(data, data1, length1);
	original->unlock(data1, data2, length1, length2);

	FMOD_CREATESOUNDEXINFO info;
	FMOD::Sound *converted = NULL;

	memset(&info, 0, sizeof(info));
	info.cbsize = sizeof(info);
	info.length = length;
	info.numchannels = channels;
	info.defaultfrequency = (int)frequency;
	info.format = FMOD_SOUND_FORMAT_PCM16;

	FMOD_RESULT result = m_pSystem->createSound((const char *)data,
		FMOD_3D | FMOD_SOFTWARE | FMOD_CREATESAMPLE | FMOD_OPENMEMORY |
		FMOD_OPENRAW | FMOD_3D_WORLDRELATIVE, &info, &converted);

	if (result != FMOD_OK)
	{
		if (sample)
			AlertMessage(at_console, "[Sound Engine] Ogg conversion failed for '%s'.\n", sample);
	}
	else
	{
		original->release();
		*sound = converted;
	}

	free(data);
}

FMOD::Sound *CClient_SoundEngine::LoadSound(const char *sample, bool runtime)
{
	if (!sample || !sample[0])
	{
		AlertMessage(at_aiconsole, "Tried to load a sound without specifying a file name.\n");
		return NULL;
	}

	FMOD::Sound *sound = NULL;
	const char *when = runtime ? " at runtime" : "";
	char name[260];
	char path[260];

	_snprintf(name, sizeof(name), "sound/%s", sample);
	name[sizeof(name) - 1] = 0;

	if (!g_pFileSystem->GetLocalPath(name, path, sizeof(path)))
	{
		AlertMessage(at_console, "[Sound Engine] - File not found%s: %s\n", when, name);
		return NULL;
	}

	FMOD_RESULT result = m_pSystem->createSound(path,
		FMOD_3D | FMOD_SOFTWARE | FMOD_3D_WORLDRELATIVE | FMOD_IGNORETAGS,
		NULL, &sound);

	if (result == FMOD_ERR_FORMAT)
		result = m_pSystem->createSound(path,
			FMOD_3D | FMOD_SOFTWARE | FMOD_MPEGSEARCH | FMOD_3D_WORLDRELATIVE, NULL, &sound);

	if (result != FMOD_OK)
	{
		if (result == FMOD_ERR_FILE_BAD)
			AlertMessage(at_console, "[Sound Engine] - Error loading sound%s: %s\n", when, name);
		else if (result == FMOD_ERR_FILE_NOTFOUND)
			AlertMessage(at_console, "[Sound Engine] - File not found%s: %s\n", when, name);
		else
			AlertMessage(at_console,
				"[Sound Engine] - Error loading sound%s, FMOD Ex error #%d: %s - Filename: %s\n",
				when, result, FMOD_ErrorString(result), name);

		return NULL;
	}

	OggPcmDataReinitialize(&sound, name);

	const char *suffix = strrchr(sample, '_');
	bool once = false;

	if (suffix && strlen(suffix) >= 6)
	{
		if (!strnicmp(suffix, "_loop.", 6))
		{
			StatusResult(sound->setMode(FMOD_LOOP_NORMAL | FMOD_3D | FMOD_SOFTWARE | FMOD_3D_WORLDRELATIVE));
			return sound;
		}

		if (!strnicmp(suffix, "_once.", 6))
			once = true;
	}

	int count = 0;

	sound->getNumSyncPoints(&count);
	if (count > 0 && !once)
	{
		FMOD_SYNCPOINT *point;
		unsigned int start = 0;

		sound->getSyncPoint(0, &point);
		sound->getSyncPointInfo(point, NULL, 0, &start, FMOD_TIMEUNIT_PCMBYTES);

		if (start)
		{
			unsigned int length = 0;

			sound->getLength(&length, FMOD_TIMEUNIT_PCMBYTES);
			StatusResult(sound->setLoopPoints(start, FMOD_TIMEUNIT_PCMBYTES, length, FMOD_TIMEUNIT_PCMBYTES));
		}

		sound->setMode(FMOD_LOOP_NORMAL | FMOD_3D | FMOD_SOFTWARE | FMOD_3D_WORLDRELATIVE);
	}
	else
	{
		sound->setMode(FMOD_LOOP_OFF | FMOD_3D | FMOD_SOFTWARE | FMOD_3D_WORLDRELATIVE);
	}

	return sound;
}
