#ifndef CLIENT_MEDIA_H
#define CLIENT_MEDIA_H

#include <fmodex/fmod.hpp>

enum MediaPlayerAction
{
	MEDIA_NEXT = 1,
	MEDIA_PREVIOUS,
	MEDIA_STOP,
	MEDIA_PLAY,
	MEDIA_PAUSE
};

enum MediaPlayerState
{
	MEDIA_STOPPED = 0,
	MEDIA_PLAYING,
	MEDIA_PAUSED
};

struct Playlist
{
	int		count;
	char	**files;
	char	**names;
};

char *stristr(char *string, const char *pattern);
Playlist *ParsePlaylist(const char *filename);
bool FreePlaylist(Playlist *playlist);

class CMediaPlayer
{
public:
	CMediaPlayer();
	~CMediaPlayer();

	void Reset(void);
	void Shutdown(void);
	const char *GetSongByIndex(unsigned int index) const;
	void ToggleHUD(void);
	void ToggleTimeDisplayMode(void);
	void ToggleRandomMode(void);
	void PerformAction(MediaPlayerAction action);
	void SetVolume(unsigned int volume);
	void ChangeVolume(bool increase);
	void LoadFromFile(const char *filename);
	bool PlayMedia(const char *filename);
	void StopMedia(void);
	void UpdateMedia(void);
	bool PlaySong(void);
	bool StopSong(void);
	void PauseSong(void);
	void SetSong(unsigned int index);
	bool NextSong(bool forward);
	unsigned int RandomSong(unsigned int current) const;
	void DrawHUDIcon(void);
	void PlayList(const char *name, int size, void *buffer);

	MediaPlayerState	m_State;
	unsigned int	m_iVolume;
	bool			m_bPlaylistLoaded;
	bool			m_bRandom;
	char			m_Songs[4096][1024];

	FMOD::Sound		*m_pSound;
	FMOD::Channel	*m_pChannel;
	unsigned int	m_iPausePosition;
	unsigned int	m_iSongCount;
	unsigned int	m_iSongIndex;
	bool			m_bShowHUD;
	bool			m_bShowRemainingTime;
};

extern CMediaPlayer MediaPlayer;

#endif
