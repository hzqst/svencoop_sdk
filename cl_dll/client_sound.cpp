#include <fmodex/fmod_errors.h>

#include "extdll.h"
#include "hud_iface.h"
#include "client_support.h"
#include "client_sound.h"
#include "client_media.h"
#include "client_messagebox.h"
#include "client_filesystem.h"
#include "SDL2/SDL_video.h"

void AlertMessage(ALERT_TYPE type, char *format, ...);

extern vec3_t v_origin;
extern vec3_t g_vVecViewangles;
extern int g_iWaterLevel;

CClient_SoundEngine *CClient_SoundEngine::m_pSoundEngine;

FMOD_RESULT F_CALLBACK FMOD_HandleSystemCallback(FMOD_SYSTEM *system,
	FMOD_SYSTEM_CALLBACKTYPE type, void *command1, void *command2)
{
	return FMOD_OK;
}

CClient_SoundEngine::CClient_SoundEngine()
{
	m_pWindow = NULL;
	memset(m_pSoundCache, 0, sizeof(m_pSoundCache));
	memset(m_pSentences, 0, sizeof(m_pSentences));
	m_iReverbType = 0;
	m_pWatchedChannels = NULL;
	m_bInitialized = false;
	m_pSfxGroup = NULL;
	m_pMusicGroup = NULL;
	ClearSoundArrays();
}

CClient_SoundEngine *CClient_SoundEngine::GetSoundEngine(void)
{
	if (!m_pSoundEngine)
		m_pSoundEngine = new CClient_SoundEngine;

	return m_pSoundEngine;
}

void CClient_SoundEngine::Initialize(void)
{
	FlushCache(false);
	m_Materials.Reset();

	if (HasCriticalStatusResult(FMOD::System_Create(&m_pSystem), NULL))
		return;

	if (HasCriticalStatusResult(m_pSystem->setCallback(FMOD_HandleSystemCallback), NULL))
		return;

	unsigned int version;

	if (HasCriticalStatusResult(m_pSystem->getVersion(&version), NULL))
		return;

	if (version < FMOD_VERSION)
	{
		char message[128];

		_snprintf(message, sizeof(message),
			"You are using an old version of FMOD: \"%08x\".  This program requires \"%08x\"\n\n"
			" Please obtain the correct FMODEx library file or re-install Sven Co-op.", version, FMOD_VERSION);
		message[sizeof(message) - 1] = 0;
		UTIL_HostError(message, 1);
	}

	m_bOutputInitialized = false;

	if (HasCriticalStatusResult(SetOutputDriver(), NULL))
		return;

	if (HasCriticalStatusResult(m_pSystem->set3DSettings(1.0f, 38.0f, 3.0f), NULL))
		return;

	if (HasCriticalStatusResult(m_pSystem->createChannelGroup("Sfx", &m_pSfxGroup), NULL))
		return;

	if (HasCriticalStatusResult(m_pSystem->createChannelGroup("Music", &m_pMusicGroup), NULL))
		return;

	FMOD::ChannelGroup *master = NULL;

	m_pSystem->getMasterChannelGroup(&master);
	if (!master)
		return;

	if (HasCriticalStatusResult(master->addGroup(m_pSfxGroup), NULL))
		return;

	if (HasCriticalStatusResult(master->addGroup(m_pMusicGroup), NULL))
		return;

	StatusResult(m_pSystem->createDSPByType(FMOD_DSP_TYPE_LOWPASS_SIMPLE, &m_pLowpassDSP));
	m_pLowpassDSP->setParameter(0, 500.0f);
	m_pLowpassDSP->setBypass(true);
	m_pSfxGroup->addDSP(m_pLowpassDSP, NULL);
	m_pRoomOff = gEngfuncs.pfnGetCvarPointer("room_off");
	m_pRoomType = gEngfuncs.pfnGetCvarPointer("room_type");
	m_pVolume = gEngfuncs.pfnGetCvarPointer("volume");
	m_pMusicVolume = gEngfuncs.pfnGetCvarPointer("MP3Volume");
	m_pSpectrumDebug = gEngfuncs.pfnGetCvarPointer("cl_spectrum_debug");

	for (unsigned int i = 0; i != (unsigned int)-1; i++)
	{
		m_pWindow = SDL_GetWindowFromID(i);
		if (m_pWindow)
			break;
	}

	m_bInitialized = true;
}

FMOD_RESULT CClient_SoundEngine::SetOutputDriver(void)
{
	if (!m_pSystem)
		return FMOD_ERR_INITIALIZATION;

	int drivers = 0;
	FMOD_OUTPUTTYPE output = FMOD_OUTPUTTYPE_AUTODETECT;

	if (m_bOutputInitialized)
	{
		m_bOutputInitialized = false;
		StatusResult(m_pSystem->close());
		StatusResult(m_pSystem->release());
	}

#ifdef _WIN32
	m_pForceDirectSound = gEngfuncs.pfnGetCvarPointer("s_force_dsound");

	if (m_pForceDirectSound && m_pForceDirectSound->value >= 1)
		output = FMOD_OUTPUTTYPE_DSOUND;
#endif

	FMOD_RESULT result = m_pSystem->getNumDrivers(&drivers);

	if (HasCriticalStatusResult(result, NULL))
		return result;

	if (drivers <= 0)
	{
		output = FMOD_OUTPUTTYPE_NOSOUND;
	}
	else
	{
		unsigned int caps;
		FMOD_SPEAKERMODE mode;
		char name[512];

		StatusResult(m_pSystem->getDriverCaps(0, &caps, NULL, &mode));
		StatusResult(m_pSystem->setSpeakerMode(mode));

		if (caps & FMOD_CAPS_HARDWARE_EMULATED)
			StatusResult(m_pSystem->setDSPBufferSize(1024, 10));

		StatusResult(m_pSystem->getDriverInfo(0, name, sizeof(name), NULL));
		AlertMessage(at_console, "[Sound Engine] Audio Driver: %s\n", name);

		if (strstr(name, "SigmaTel"))
		{
			StatusResult(m_pSystem->setSoftwareFormat(44100, FMOD_SOUND_FORMAT_PCMFLOAT,
				0, 0, FMOD_DSP_RESAMPLER_LINEAR));
			AlertMessage(at_console, "[Sound Engine] SigmaTel audio optimizations applied.\n");
		}
	}

	result = m_pSystem->setOutput(output);
	if (HasCriticalStatusResult(result, NULL))
		return result;

	result = m_pSystem->init(1024, FMOD_INIT_3D_RIGHTHANDED, NULL);
	if (result == FMOD_ERR_OUTPUT_CREATEBUFFER)
	{
		StatusResult(m_pSystem->setSpeakerMode(FMOD_SPEAKERMODE_STEREO));
		result = m_pSystem->init(1024, FMOD_INIT_3D_RIGHTHANDED, NULL);
		StatusResult(result);
	}

	if (!HasCriticalStatusResult(result, NULL))
		m_bOutputInitialized = true;

	return result;
}

void CClient_SoundEngine::ShutDown(void)
{
	ClearSoundArrays();
	FlushCache(false);
	MediaPlayer.Shutdown();

	if (m_pSystem && m_bInitialized)
	{
		m_bInitialized = false;
		m_pSystem->close();
		m_pSystem->release();
	}
}

void CClient_SoundEngine::ClearSoundArrays(void)
{
	memset(m_szSoundNames, 0, sizeof(m_szSoundNames));

	sndChannel_t *channel = m_pWatchedChannels;

	while (channel)
	{
		sndChannel_t *next = channel->next;

		if (channel->channel)
			channel->channel->stop();

		delete channel;
		channel = next;
	}

	m_pWatchedChannels = NULL;

	for (int i = 0; i < MAX_SOUNDLIST_SOUNDS; i++)
	{
		if (m_pSoundCache[i])
		{
			if (m_pSoundCache[i]->sound)
				m_pSoundCache[i]->sound->release();

			delete m_pSoundCache[i];
		}

		m_pSoundCache[i] = NULL;
	}

	for (int i = 0; i < MAX_SOUNDLIST_SENTENCES; i++)
	{
		if (m_pSentences[i])
		{
			sndWord_t *word = m_pSentences[i]->words;

			while (word)
			{
				sndWord_t *next = word->next;

				delete word;
				word = next;
			}

			delete m_pSentences[i];
		}

		m_pSentences[i] = NULL;
	}

	m_iSentenceCount = 0;
	m_iSoundCount = 0;
	m_bSoundListLoaded = false;
	m_flMapTime = gEngfuncs.GetClientTime();
}

bool CClient_SoundEngine::IsNewMap(void)
{
	char mapName[128];
	const char *level = gEngfuncs.pfnGetLevelName();

	StripMapFromPFN(level, mapName);

	return strcmp(m_szMapName, mapName) != 0;
}

void CClient_SoundEngine::MapStarted(void)
{
	if (IsNewMap())
		ClearSoundArrays();

	m_flNextEnvironmentUpdate = gEngfuncs.GetClientTime();
}

void CClient_SoundEngine::Update(void)
{
	if (!m_bInitialized)
		return;

	if (!m_bSoundListLoaded)
		LoadSoundList();

	UpdateWatchedChannels();
	UpdateEnvironmentalEffect();

	Vector angles = g_vVecViewangles;
	Vector forward, up;
	FMOD_VECTOR velocity = { 0, 0, 0 };

	angles.z += 180;
	gEngfuncs.pfnAngleVectors(angles, forward, NULL, up);

	FMOD_VECTOR position = { -v_origin.x, v_origin.y, v_origin.z };
	FMOD_VECTOR direction = { -forward.x, forward.y, forward.z };
	FMOD_VECTOR vertical = { -up.x, up.y, up.z };

	m_pSystem->set3DListenerAttributes(0, &position, &velocity, &direction, &vertical);
	m_pSystem->update();
}

extern "C" void PM_UpdateWaterLevel_CLIENT(int waterlevel)
{
	g_iWaterLevel = waterlevel;
}

extern "C" int PM_GetMaterials_CLIENT(void)
{
	static int i;

	if ((unsigned int)i < MAX_MATERIALS)
		return ++i;

	i = 1;
	return 1;
}

extern "C" char PM_FindMaterialType_CLIENT(char *name)
{
	return CClient_SoundEngine::GetSoundEngine()->m_Materials.FindMaterialType(name);
}

void CClient_SoundEngine::UpdateEnvironmentalEffect(void)
{
	if (!m_bInitialized)
		return;

	int room = 0;
	bool bypass;

	if (g_iWaterLevel >= 3)
	{
		if (m_pRoomOff && (m_pRoomOff->value == 15 || m_pRoomOff->value == 16))
			room = (int)m_pRoomOff->value;
		else
			room = 14;

		bypass = false;
	}
	else
	{
		if (m_pMuteUnfocused && m_pRoomOff->value < 1)
			room = (int)m_pRoomType->value;

		bypass = true;
	}

	m_pLowpassDSP->setBypass(bypass);

	float now = gEngfuncs.GetClientTime();

	if (m_flNextEnvironmentUpdate > now)
		return;

	FMOD::ChannelGroup *master;

	m_pSystem->getMasterChannelGroup(&master);
	if (master)
	{
		m_pMuteUnfocused = gEngfuncs.pfnGetCvarPointer("s_mute_unfocused");

		bool mute = m_pMuteUnfocused && m_pMuteUnfocused->value >= 1;

		mute = m_pWindow && !(SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_INPUT_FOCUS) && mute;
		StatusResult(master->setMute(mute));
	}

	StatusResult(m_pSfxGroup->setVolume(V_min(1.0f, V_max(0.0f, m_pVolume->value))));
	StatusResult(m_pMusicGroup->setVolume(V_min(1.0f, V_max(0.0f, m_pMusicVolume->value))));

	if (m_iReverbType != room)
	{
		room = V_min(CSXROOM - 1, V_max(0, room));

		FMOD_REVERB_PROPERTIES properties = eax_presets[room];

		StatusResult(m_pSystem->setReverbProperties(&properties));
		m_iReverbType = room;
	}

	m_flNextEnvironmentUpdate = gEngfuncs.GetClientTime() + 0.1;
}

void CClient_SoundEngine::StatusResult(FMOD_RESULT result)
{
	if (result != FMOD_OK)
		AlertMessage(at_console, "[Sound Engine] FMOD Ex error #%d: %s\n", result, FMOD_ErrorString(result));
}

bool CClient_SoundEngine::HasCriticalStatusResult(FMOD_RESULT result, char *message)
{
	if (result == FMOD_OK)
		return false;

	char text[512];

	_snprintf(text, sizeof(text),
		"FMOD Ex error code %d:\n\n%s\n\nYou can play Sven Co-op but without sound.\nWould you like to continue?",
		result, FMOD_ErrorString(result));
	text[sizeof(text) - 1] = 0;

#ifdef _WIN32
	int answer = MessageBoxA(NULL, text, "FMOD Ex Critical Error", MB_SETFOREGROUND | MB_ICONWARNING | MB_YESNO);
#else
	int answer = UTIL_MessageBox(text, "FMOD Ex Critical Error", MSGBOX_ICONWARNING, MSGBOX_YESNO);
#endif

	if (answer == MSGBOX_IDNO)
		exit(result);

	return true;
}

void FMOD_StopSound(void)
{
	CClient_SoundEngine *soundEngine = CClient_SoundEngine::GetSoundEngine();

	if (soundEngine->m_pSystem && soundEngine->m_bInitialized)
	{
		FMOD::ChannelGroup *master;

		soundEngine->m_pSystem->getMasterChannelGroup(&master);
		master->stop();
	}
}

void FMOD_FlushSoundCache(void)
{
	CClient_SoundEngine::GetSoundEngine()->FlushCache(false);
}

void FMOD_FlushSoundCacheIncludingLocal(void)
{
	CClient_SoundEngine::GetSoundEngine()->FlushCache(true);
}
