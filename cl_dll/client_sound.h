#ifndef CLIENT_SOUND_H
#define CLIENT_SOUND_H

#include <fmodex/fmod.hpp>
#include "materials.h"
#include "sound_message.h"

#define CSXROOM		29	// engine room_type presets

// sndChannel_t::flags
#define SNDCHAN_STATIC	( 1<<0 )	// keep the stored origin instead of following the entity
#define SNDCHAN_STOPPED	( 1<<1 )	// replaced by a newer sound, remove on the next update

struct cvar_s;
struct SDL_Window;

struct sndWord_t
{
	float		volume;
	float		pitch;
	float		start;
	float		end;
	float		timeCompress;
	char		sample[260];
	sndWord_t	*next;
};

struct sndSentence_t
{
	char		name[32];
	sndWord_t	*words;
	int			unknown36;
};

struct sndCache_t
{
	char		name[260];
	FMOD::Sound	*sound;
};

struct sndChannel_t
{
	int				channelIndex;
	int				entityIndex;
	float			volume;
	int				pitch;
	float			minDistance;
	float			maxDistance;
	FMOD_VECTOR		origin;
	float			mouthOpen;
	float			mouthTime;
	float			endTime;
	bool			looping;
	int				flags;
	FMOD::Channel	*channel;
	FMOD::Sound		*sound;
	int				unknown64;
	sndWord_t		*word;
	sndChannel_t	*next;
};

class CClient_SoundEngine
{
public:
	CClient_SoundEngine();

	static CClient_SoundEngine *GetSoundEngine(void);
	void Initialize(void);
	void ShutDown(void);
	FMOD_RESULT SetOutputDriver(void);
	void ClearSoundArrays(void);
	void FlushCache(bool all);
	bool IsNewMap(void);
	void MapStarted(void);
	void Update(void);
	void UpdateEnvironmentalEffect(void);
	void UpdateWatchedChannels(void);
	bool LoadSoundList(void);

	const char *LookupSoundByIndex(int index);
	const char *LookupSoundBySentenceIndex(int index);
	int LookupSoundIndexByName(const char *name);
	FMOD::Sound *LookupSoundBySample(const char *sample);
	FMOD::Sound *LoadSound(const char *sample, bool runtime);
	void OggPcmDataReinitialize(FMOD::Sound **sound, const char *sample);

	bool ParseSentenceLine(char *line);
	sndWord_t *ParseSentenceWords(char *words);
	sndWord_t *ParseWordEntry(char *entry, char *directory, bool resetDefaults);
	bool ParseSentenceDirectory(char **words, char *directory, int size);
	bool findWordAudioFile(char *directory, char *word, char *sample, unsigned int size);
	bool AddSentence(sndSentence_t *sentence);

	void PlayFMODSound(int messageFlags, int entityIndex, const float *origin,
		int channelIndex, const char *sample, float volume, float attenuation,
		int flags, int pitch, int sentenceIndex, float offset);
	void Play2DSound(int index, float volume);
	void Play2DSound(const char *sample, float volume);
	void AddWatchedChannel(int entityIndex, int channelIndex, FMOD::Channel *channel,
		FMOD::Sound *sound, bool trackEntity, int sentenceIndex, int pitch,
		float volume, float minDistance, float maxDistance, FMOD_VECTOR origin);
	sndChannel_t *FindWatchedChannelByIndex(int entityIndex, int channelIndex);
	float CalculateMouthOpen(int entityIndex);
	void StatusResult(FMOD_RESULT result);
	bool HasCriticalStatusResult(FMOD_RESULT result, char *message);

	static CClient_SoundEngine	*m_pSoundEngine;
	static const FMOD_REVERB_PROPERTIES eax_presets[CSXROOM];

	sndSentence_t		*m_pSentences[MAX_SOUNDLIST_SENTENCES];
	int					m_iActiveSounds;
	FMOD::System		*m_pSystem;
	CMaterialsList		m_Materials;
	sndCache_t			*m_pSoundCache[MAX_SOUNDLIST_SOUNDS];
	char				m_szSoundNames[MAX_SOUNDLIST_SOUNDS][260];
	sndChannel_t		*m_pWatchedChannels;
	bool				m_bOutputInitialized;
	bool				m_bSoundListLoaded;
	bool				m_bInitialized;
	float				m_flMapTime;
	float				m_flNextEnvironmentUpdate;
	char				m_szMapName[128];
	int					m_iSentenceCount;
	int					m_iSoundCount;
	int					m_iReverbType;
	FMOD::DSP			*m_pLowpassDSP;
	FMOD::ChannelGroup	*m_pSfxGroup;
	FMOD::ChannelGroup	*m_pMusicGroup;
	cvar_s				*m_pVolume;
	cvar_s				*m_pMusicVolume;
	cvar_s				*m_pRoomOff;
	cvar_s				*m_pRoomType;
	cvar_s				*m_pSpectrumDebug;
	cvar_s				*m_pForceDirectSound;
	cvar_s				*m_pMuteUnfocused;
	SDL_Window			*m_pWindow;
};

extern "C" void PM_PlaySoundFX_CLIENT(int playerIndex, float *origin, int channelIndex, char *sample,
	float volume, float attenuation, int flags, int pitch);
extern "C" void PM_UpdateWaterLevel_CLIENT(int waterlevel);
extern "C" int PM_GetMaterials_CLIENT(void);
extern "C" char PM_FindMaterialType_CLIENT(char *name);

void FMOD_StopSound(void);
void FMOD_FlushSoundCache(void);
void FMOD_FlushSoundCacheIncludingLocal(void);

int __MsgFunc_StartSound(const char *name, int size, void *buffer);
int __MsgFunc_Speaksent(const char *name, int size, void *buffer);

#endif
