#include <fmodex/fmod_errors.h>

#include "hud.h"
#include "cl_util.h"
#include "client_media.h"
#include "client_sound.h"
#include "client_filesystem.h"
#include "vgui_MediaSelect.h"
#include "parsemsg.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

CMediaPlayer MediaPlayer;

CMediaPlayer::CMediaPlayer()
{
	m_iVolume = 8;
	m_bRandom = false;
	m_pSound = NULL;
	m_pChannel = NULL;
	m_bShowHUD = true;
	m_bShowRemainingTime = true;
	Reset();
}

CMediaPlayer::~CMediaPlayer()
{
}

void CMediaPlayer::Reset(void)
{
	m_State = MEDIA_STOPPED;
	m_bPlaylistLoaded = false;
	memset(m_Songs, 0, sizeof(m_Songs));
	StopSong();
	m_iPausePosition = 0;
	m_iSongCount = 0;
	m_iSongIndex = (unsigned int)-1;
}

void CMediaPlayer::Shutdown(void)
{
	if (m_pChannel)
	{
		m_pChannel->stop();
		m_pChannel = NULL;
	}

	if (m_pSound)
	{
		m_pSound->release();
		m_pSound = NULL;
	}
}

const char *CMediaPlayer::GetSongByIndex(unsigned int index) const
{
	if (index > m_iSongCount)
		return NULL;

	return m_Songs[index];
}

void CMediaPlayer::ToggleHUD(void)
{
	m_bShowHUD = !m_bShowHUD;
	gEngfuncs.Con_Printf("Music Player Hud: %s\n", m_bShowHUD ? "ON" : "OFF");
}

void CMediaPlayer::ToggleTimeDisplayMode(void)
{
	m_bShowRemainingTime = !m_bShowRemainingTime;
}

void CMediaPlayer::ToggleRandomMode(void)
{
	m_bRandom = !m_bRandom;
	gViewPort->m_pMediaSelect->Update();
}

void CMediaPlayer::PerformAction(MediaPlayerAction action)
{
	switch (action)
	{
	case MEDIA_NEXT:
		NextSong(true);
		break;
	case MEDIA_PREVIOUS:
		NextSong(false);
		break;
	case MEDIA_STOP:
		StopMedia();
		break;
	case MEDIA_PLAY:
		PlaySong();
		break;
	case MEDIA_PAUSE:
		PauseSong();
		break;
	}
}

void CMediaPlayer::SetVolume(unsigned int volume)
{
	if (volume < 1)
		volume = 1;

	if (volume > 10)
		volume = 10;

	m_iVolume = volume;
	gViewPort->m_pMediaSelect->Update();

	if (m_pChannel)
		m_pChannel->setVolume((double)m_iVolume / 10.0);
}

void CMediaPlayer::ChangeVolume(bool increase)
{
	SetVolume(increase ? m_iVolume + 1 : m_iVolume - 1);
}

void CopyMediaString(char *destination, const char *source, unsigned int size)
{
	memset(destination, 0, size);

	if (source && *source)
		strncpy(destination, source, size - 1);
}

void CMediaPlayer::LoadFromFile(const char *filename)
{
	Reset();

	const char *files[] = { filename, "playlist.txt", "playlist.m3u" };
	FileHandle_t file = NULL;

	for (int i = 0; i < 3; i++)
	{
		if (files[i])
			file = g_pFileSystem->Open(files[i], "r", "GAMECONFIG");

		if (file)
			break;
	}

	if (!file)
	{
		gHUD.m_SayText.Print("* Can't open playlist file 'playlist.txt' or 'playlist.m3u'. Use the Playlist Editor tool to create one.\n", 106);
		return;
	}

	char line[1024];

	while (m_iSongCount < 4096 && g_pFileSystem->ReadLine(line, sizeof(line), file))
	{
		if (line[0] == '\n' || line[0] == '*' || line[0] == '#')
			continue;

		unsigned int length = strlen(line);

		if (line[length - 1] == '\n')
			line[--length] = 0;

		if (length > 3 && stricmp(line + length - 3, "pls") == 0)
		{
			Playlist *playlist = ParsePlaylist(line);

			if (playlist)
			{
				for (int i = 0; i < playlist->count; i++)
					CopyMediaString(m_Songs[m_iSongCount++], playlist->files[i], sizeof(m_Songs[0]));

				FreePlaylist(playlist);
			}
		}
		else if (length)
		{
			m_bPlaylistLoaded = true;
			CopyMediaString(m_Songs[m_iSongCount++], line, sizeof(m_Songs[0]));
		}
	}

	g_pFileSystem->Close(file);
}

void MediaError(FMOD_RESULT result)
{
	char text[512];

	_snprintf(text, sizeof(text), "FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
	text[sizeof(text) - 1] = 0;
	gHUD.m_SayText.Print(text, sizeof(text));
}

bool CMediaPlayer::PlayMedia(const char *filename)
{
	if (!CClient_SoundEngine::GetSoundEngine()->m_bSoundListLoaded)
		return false;

	FMOD_RESULT result = CClient_SoundEngine::GetSoundEngine()->m_pSystem->createStream(filename,
		FMOD_2D | FMOD_CREATECOMPRESSEDSAMPLE, NULL, &m_pSound);

	if (result != FMOD_OK)
		MediaError(result);

	if (!m_pSound)
	{
		StopMedia();
		return false;
	}

	result = CClient_SoundEngine::GetSoundEngine()->m_pSystem->playSound(FMOD_CHANNEL_FREE,
		m_pSound, false, &m_pChannel);

	if (result != FMOD_OK)
		MediaError(result);

	if (!m_pChannel)
		return false;

	char name[1024];
	char text[512];

	m_pSound->getName(name, sizeof(name));
	gViewPort->m_pMediaSelect->SetTitleText(name);
	_snprintf(text, sizeof(text), "* Now playing: %s\n", filename);
	text[sizeof(text) - 1] = 0;
	gHUD.m_SayText.Print(text, sizeof(text));
	m_pChannel->setPriority(0);

	if (m_State == MEDIA_PAUSED)
	{
		m_pChannel->setPosition(m_iPausePosition, FMOD_TIMEUNIT_MS);
		m_iPausePosition = 0;
	}

	m_pChannel->setVolume((double)m_iVolume / 10.0);
	m_State = MEDIA_PLAYING;
	return true;
}

void CMediaPlayer::StopMedia(void)
{
	m_bPlaylistLoaded = false;

	if (m_pChannel)
	{
		m_pChannel->stop();
		m_pChannel = NULL;

		if (m_pSound)
		{
			m_pSound->release();
			m_pSound = NULL;
		}

		m_iPausePosition = 0;
		m_State = MEDIA_STOPPED;
	}
}

void CMediaPlayer::UpdateMedia(void)
{
	if (!CClient_SoundEngine::GetSoundEngine()->m_bSoundListLoaded
		|| !m_bPlaylistLoaded || m_State != MEDIA_PLAYING || !m_pChannel)
		return;

	unsigned int length, position;

	m_pSound->getLength(&length, FMOD_TIMEUNIT_MS);
	m_pChannel->getPosition(&position, FMOD_TIMEUNIT_MS);

	if (length - position < 400 && !NextSong(true))
		m_State = MEDIA_STOPPED;
}

bool CMediaPlayer::PlaySong(void)
{
	if (!CClient_SoundEngine::GetSoundEngine()->m_bSoundListLoaded)
		return false;

	if (!m_bPlaylistLoaded)
	{
		LoadFromFile(NULL);

		if (!m_bPlaylistLoaded)
			return false;
	}

	if (m_iSongIndex >= m_iSongCount)
		m_iSongIndex = 0;

	if (m_State != MEDIA_PAUSED)
	{
		m_iPausePosition = 0;
		m_State = MEDIA_PLAYING;
		StopSong();
	}

	if (PlayMedia(m_Songs[m_iSongIndex]))
		return true;

	char text[512];

	_snprintf(text, sizeof(text), "* Unknown file: %s:\n", m_Songs[0]);
	text[sizeof(text) - 1] = 0;
	gHUD.m_SayText.Print(text, sizeof(text));
	PauseSong();
	return true;
}

bool CMediaPlayer::StopSong(void)
{
	bool stopped = m_pChannel != NULL;

	Shutdown();
	return stopped;
}

void CMediaPlayer::PauseSong(void)
{
	if (m_pChannel && m_pSound)
	{
		if (m_State != MEDIA_PAUSED)
		{
			m_pChannel->getPosition(&m_iPausePosition, FMOD_TIMEUNIT_MS);
			m_State = MEDIA_PAUSED;
			StopSong();
		}
	}
	else
	{
		m_State = MEDIA_PAUSED;
	}
}

void CMediaPlayer::SetSong(unsigned int index)
{
	m_iSongIndex = index;

	if (index != (unsigned int)-1)
		PlaySong();
}

bool CMediaPlayer::NextSong(bool forward)
{
	if (!CClient_SoundEngine::GetSoundEngine()->m_bSoundListLoaded)
		return false;

	if (!m_bPlaylistLoaded)
		LoadFromFile(NULL);

	if (m_bRandom)
	{
		m_iSongIndex = RandomSong(m_iSongIndex);
	}
	else if (forward)
	{
		if (++m_iSongIndex == m_iSongCount)
			m_iSongIndex = 0;
	}
	else
	{
		if (!m_iSongIndex)
			m_iSongIndex = m_iSongCount;

		m_iSongIndex--;
	}

	return PlaySong();
}

unsigned int CMediaPlayer::RandomSong(unsigned int current) const
{
	if (m_iSongCount <= 1)
		return current;

	unsigned int index;

	do
	{
		index = gEngfuncs.pfnRandomLong(0, m_iSongCount - 1);
	} while (index == current);

	return index;
}

void CMediaPlayer::DrawHUDIcon(void)
{
	if (!CClient_SoundEngine::GetSoundEngine()->m_bSoundListLoaded || m_State != MEDIA_PLAYING)
		return;

	unsigned int length, position;
	char text[512];

	m_pSound->getLength(&length, FMOD_TIMEUNIT_MS);
	m_pChannel->getPosition(&position, FMOD_TIMEUNIT_MS);

	if (length == (unsigned int)-1)
	{
		CopyMediaString(text, "Infinite", sizeof(text));
	}
	else
	{
		int seconds = m_bShowRemainingTime ? length / 1000 - position / 1000 : position / 1000;

		_snprintf(text, sizeof(text), "%d:%02d", seconds / 60, seconds % 60);
		text[sizeof(text) - 1] = 0;
	}

	gViewPort->m_pMediaSelect->SetTime(text);

	if (!m_bShowHUD)
		return;

	int y = ScreenHeight >> 1;
	int x = gHUD.GetBorderSize();

	gHUD.DrawString(x, y, ScreenWidth, text, 255, 255, 255, 0);

	if (m_bRandom)
		gHUD.DrawString(x, y + 15, ScreenWidth, "R", 255, 0, 0, 0);

	y -= 7;
	FillRGBA(x - 1, y - 1, 27, 1, 255, 160, 0, 200);
	FillRGBA(x - 1, y + 5, 27, 1, 255, 160, 0, 200);
	FillRGBA(x - 1, y, 1, 5, 255, 160, 0, 200);
	FillRGBA(x + 25, y, 1, 5, 255, 160, 0, 200);

	float progress = fminf(1.0f, (float)(position / 1000) / (float)(length / 1000));

	FillRGBA(x, y, (int)(progress * 25.0f), 5, 0, 160, 0, 255);
	gViewPort->m_pMediaSelect->SetFill(progress);
}

void CMediaPlayer::PlayList(const char *name, int size, void *buffer)
{
	BEGIN_READ(buffer, size);

	const char *filename = READ_STRING();
	char text[512];

	_snprintf(text, sizeof(text), "* Playlist changed to: %s:\n", filename);
	text[sizeof(text) - 1] = 0;
	gHUD.m_SayText.Print(text, sizeof(text));
	StopMedia();
	LoadFromFile(filename);
	gViewPort->m_pMediaSelect->Rebuild();
}

char *stristr(char *string, const char *pattern)
{
	char *lowerPattern = strdup(pattern);
	char *lowerString = strdup(string);
	char *result = NULL;
	int patternLength = strlen(pattern);
	int limit = strlen(string) - patternLength;

	for (char *p = lowerPattern; *p; p++)
		*p = tolower(*p);

	for (char *p = lowerString; *p; p++)
		*p = tolower(*p);

	for (int i = 0; i < limit; i++)
	{
		int match = 0;

		while (match < patternLength && lowerPattern[match] && lowerString[i + match]
			&& lowerPattern[match] == lowerString[i + match])
			match++;

		if (match == patternLength)
		{
			result = string + i;
			break;
		}
	}

	free(lowerPattern);
	free(lowerString);
	return result;
}

Playlist *ParsePlaylist(const char *filename)
{
	IFileSystem_SvEngine *fileSystem = (IFileSystem_SvEngine *)g_pFileSystem;

	if (!filename || stricmp(".pls", filename + strlen(filename) - 4) != 0
		|| !fileSystem->FileExistsEx(filename, "GAMECONFIG"))
		return NULL;

	unsigned int size = fileSystem->SizeEx(filename, "GAMECONFIG");

	if (!size)
		return NULL;

	char *data = (char *)malloc(size);

	if (!data)
		return NULL;

	FileHandle_t file = g_pFileSystem->Open(filename, "rb", "GAMECONFIG");
	int read = 0;

	if (file)
	{
		read = g_pFileSystem->Read(data, size, file);
		g_pFileSystem->Close(file);
	}

	if (!file || read != size)
	{
		free(data);
		return NULL;
	}

	data[size - 1] = 0;

	char *entries = stristr(data, "NumberOfEntries=");
	int count = entries ? atoi(entries + 16) : 0;
	Playlist *playlist = NULL;

	if (count)
	{
		playlist = (Playlist *)calloc(sizeof(Playlist), 1);

		if (playlist)
		{
			playlist->count = count;
			playlist->files = (char **)calloc(count * sizeof(char *), 1);

			if (playlist->files)
				playlist->names = (char **)calloc(count * sizeof(char *), 1);
		}
	}

	if (!playlist || !playlist->files || !playlist->names)
	{
		FreePlaylist(playlist);
		free(data);
		return NULL;
	}

	for (int i = 0; i < count; i++)
	{
		char key[32];
		char *path = NULL;
		char *name = NULL;

		sprintf(key, "File%d=", i + 1);

		char *entry = strstr(data, key);

		if (entry)
		{
			char *start = entry + strlen(key);
			char *end = start;

			while (*end && *end != '\n' && *end != '\r')
				end++;

			if (*end)
			{
				char saved = *end;

				*end = 0;
				path = strdup(start);
				*end = saved;
			}
		}

		if (strncmp(path, "http://", 7) != 0 && strncmp(path, "http:\\\\", 7) != 0)
		{
			char *copy = strdup(path);
			char *base = copy + strlen(copy) - 1;

			while (base > copy && *base != '\\' && *base != '/')
				base--;

			if (*base == '\\' || *base == '/')
				base++;

			name = strdup(base);
			free(copy);
		}

		playlist->files[i] = path;
		playlist->names[i] = name;
	}

	free(data);
	return playlist;
}

bool FreePlaylist(Playlist *playlist)
{
	if (!playlist)
		return false;

	if (playlist->files)
	{
		for (int i = 0; i < playlist->count; i++)
			free(playlist->files[i]);

		free(playlist->files);
	}

	if (playlist->names)
	{
		for (int i = 0; i < playlist->count; i++)
			free(playlist->names[i]);

		free(playlist->names);
	}

	free(playlist);
	return true;
}
