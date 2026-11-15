#include "extdll.h"
#include "util.h"
#include "server_sound.h"
#include "server_filesystem.h"
#include "server_support.h"
#include <ctype.h>

int CServer_SoundEngine::LookupSentenceIndex(char *name)
{
	if (!name)
		return -1;

	for (int i = 0; i < MAX_SOUNDLIST_SENTENCES && strlen(m_Sentences[i].name); i++)
	{
		if (!stricmp(name, m_Sentences[i].name))
			return i;
	}

	if (m_bSoundListGenerated)
		ALERT(at_console, "[Sound Engine] - Sentence '%s' not found!\n", name);

	return -1;
}

char *CServer_SoundEngine::LookupSentenceName(int index)
{
	if ((unsigned int)index >= MAX_SOUNDLIST_SENTENCES)
		return NULL;

	if (strlen(m_Sentences[index].name))
		return m_Sentences[index].name;

	if (m_bSoundListGenerated)
		ALERT(at_console, "[Sound Engine] - Sentence index %d does not exist!\n", index);

	return NULL;
}

int CServer_SoundEngine::LookupSentenceGroupIndex(const char *name)
{
	if (!name)
		return -1;

	for (int i = 0; i < MAX_SENTENCE_GROUPS && strlen(m_SentenceGroups[i].name); i++)
	{
		if (!stricmp(m_SentenceGroups[i].name, name))
			return i;
	}

	ALERT(at_console, "[Sound Engine] - Sentence group '%s' not found!\n", name);
	return -1;
}

void CServer_SoundEngine::CategorizeSentence(const char *name, int index)
{
	if (!name)
		return;

	unsigned int end = strlen(name) - 1;

	while (name[end] && isspace((unsigned char)name[end]))
		end--;

	if (!isdigit((unsigned char)name[end]))
		return;

	while (name[end] && (isspace((unsigned char)name[end]) || isdigit((unsigned char)name[end])))
		end--;

	char groupName[MAX_SENTENCE_NAME];

	UTIL_CopyString(groupName, name, end + 2);
	groupName[end + 1] = 0;

	for (int i = 0; i < MAX_SENTENCE_GROUPS; i++)
	{
		server_sentencegroup_t &group = m_SentenceGroups[i];

		if (!strlen(group.name))
		{
			UTIL_CopyString(group.name, groupName, sizeof(group.name));
			group.sentences[0] = index;
			group.shuffledSentences[0] = index;
			group.count = 1;
			return;
		}

		if (strcmp(group.name, groupName))
			continue;

		if (group.count >= MAX_GROUP_SENTENCES)
		{
			ALERT(at_error, "[Sound Engine] - Too many sentences in group '%s'! (Limit of %d)\n", groupName, MAX_GROUP_SENTENCES);
			return;
		}

		group.sentences[group.count] = index;
		group.shuffledSentences[group.count] = index;
		group.count++;
		return;
	}

	ALERT(at_error, "[Sound Engine] - Too many sentence groups! (Limit of %d)\n", MAX_SENTENCE_GROUPS);
}

void CServer_SoundEngine::LoadSentenceList(void)
{
	char paths[2][MAX_PATH] = {};
	int count = 0;

	UTIL_CopyString(paths[0], "sound/default_sentences.txt", sizeof(paths[0]));

	if (m_szSentenceFile[0])
		UTIL_CopyString(paths[1], m_szSentenceFile, sizeof(paths[1]));

	for (int fileIndex = 0; fileIndex < 2 && paths[fileIndex][0]; fileIndex++)
	{
		ALERT(at_console, "Loading sentence list \"%s\"...\n", paths[fileIndex]);

		FileHandle_t file = g_pFileSystem->Open(paths[fileIndex], "r", "GAME|DEFAULTGAME");

		if (!file)
		{
			ALERT(at_error, "[Sound Engine] - Failed to open %s sentence file \"%s\"\n",
				fileIndex ? "custom" : "core", paths[fileIndex]);
			continue;
		}

		char line[512];
		int lineNumber = 0;

		while (!g_pFileSystem->EndOfFile(file))
		{
			if (count > MAX_SOUNDLIST_SENTENCES)
				break;

			if (count >= MAX_SOUNDLIST_SENTENCES)
			{
				ALERT(at_warning, "Maximum sentences loaded (%d), aborting.\n", count);
				count--;
				break;
			}

			if (!g_pFileSystem->ReadLine(line, sizeof(line), file))
				continue;

			lineNumber++;

			int start = 0;

			while (line[start] && isspace((unsigned char)line[start]))
				start++;

			char *name = &line[start];

			if (!*name || *name == '/' || !isalpha((unsigned char)*name))
				continue;

			int end = start;

			while (line[end] && !isspace((unsigned char)line[end]))
				end++;

			char *text = &line[end];
			int nameLength = end - start;

			if (nameLength > MAX_SENTENCE_NAME)
			{
				ALERT(at_error, "[Sound Engine] - Sentence name length too long! Greater than %d characters! (Line #%d)\n",
					MAX_SENTENCE_NAME, lineNumber);
				continue;
			}

			while (name[nameLength] && isspace((unsigned char)name[nameLength]) && nameLength > 1)
				nameLength--;

			while (isspace((unsigned char)*text))
				text++;

			int textEnd = strlen(text) - 1;

			while (text[textEnd] && (text[textEnd] == '\n' || text[textEnd] == '\r' ||
				text[textEnd] == '\t' || text[textEnd] == ' ') && textEnd > 1)
				textEnd--;

			text[textEnd + 1] = 0;

			if (strlen(text) >= MAX_SENTENCE_TEXT)
			{
				ALERT(at_error, "[Sound Engine] - Sentence length too long! Greater than %d characters! (Line #%d)\n",
					MAX_SENTENCE_TEXT, lineNumber);
				continue;
			}

			char sentenceName[MAX_SENTENCE_NAME] = {};

			UTIL_CopyString(sentenceName, name, nameLength + 2);

			int index = LookupSentenceIndex(sentenceName);

			if (index <= -1)
			{
				server_sentence_t &sentence = m_Sentences[count];

				UTIL_CopyString(sentence.name, name, nameLength + 2);
				UTIL_CopyString(sentence.text, text, sizeof(sentence.text));
				ALERT(at_aiconsole, "[Sound Engine] - Sentence #%d loaded: \"%s\" as \"%s\"\n",
					count, sentence.name, sentence.text);
				count++;
			}
			else
			{
				server_sentence_t &sentence = m_Sentences[index];

				UTIL_CopyString(sentence.name, name, nameLength + 2);
				UTIL_CopyString(sentence.text, text, sizeof(sentence.text));
				ALERT(at_aiconsole, "[Sound Engine] - Sentence #%d replaced: \"%s\" as \"%s\"\n",
					index, sentence.name, sentence.text);
			}
		}

		g_pFileSystem->Close(file);
		ALERT(at_aiconsole, "[Sound Engine] - %d sentence(s) loaded.\n",
			strlen(m_Sentences[0].name) ? count : 0);
	}

	for (int i = 0; i < count; i++)
		CategorizeSentence(m_Sentences[i].name, i);
}

int CServer_SoundEngine::PlaySentenceGroup(edict_t *entity, const char *name,
	float volume, float attenuation, int flags, int pitch, int channel)
{
	int index = LookupSentenceGroupIndex(name);

	if (index == -1)
		return -1;

	return PlaySentenceGroup(entity, index, volume, attenuation, flags, pitch, channel);
}

int CServer_SoundEngine::PlaySentenceGroup(edict_t *entity, int index,
	float volume, float attenuation, int flags, int pitch, int channel)
{
	index = V_max(0, V_min(MAX_SENTENCE_GROUPS - 1, index));

	server_sentencegroup_t &group = m_SentenceGroups[index];
	int sentenceIndex = group.shuffledSentences[group.nextSentence];
	char sample[MAX_SENTENCE_NAME + 1];

	UTIL_CopyString(sample, "!", sizeof(sample));
	UTIL_AppendString(sample, m_Sentences[sentenceIndex].name, sizeof(sample));

	if (channel <= -1)
		channel = CHAN_VOICE;

	EMIT_SOUND_DYN(entity, channel, sample, volume, attenuation, flags, pitch);
	group.nextSentence++;

	if (group.nextSentence >= group.count)
	{
		for (int i = 0; i < group.count * 4; i++)
		{
			int first = RANDOM_LONG(0, group.count - 1);
			int second = RANDOM_LONG(0, group.count - 1);
			int sentence = group.shuffledSentences[first];

			group.shuffledSentences[first] = group.shuffledSentences[second];
			group.shuffledSentences[second] = sentence;
		}

		group.nextSentence = 0;
	}

	return sentenceIndex;
}

int CServer_SoundEngine::PlaySentenceGroupSequential(edict_t *entity,
	const char *name, float volume, float attenuation, int flags, int pitch,
	int sentenceNumber, bool reset, int channel)
{
	int index = LookupSentenceGroupIndex(name);

	if (index == -1)
		return -1;

	if (channel <= -1)
		channel = CHAN_VOICE;

	index = V_max(0, V_min(MAX_SENTENCE_GROUPS - 1, index));

	server_sentencegroup_t &group = m_SentenceGroups[index];

	if (sentenceNumber >= group.count || sentenceNumber < 0 || reset)
		sentenceNumber = 0;

	sentenceNumber = V_min(MAX_GROUP_SENTENCES - 1, sentenceNumber);

	int sentenceIndex = group.sentences[sentenceNumber];
	char sample[MAX_SENTENCE_NAME + 1];

	UTIL_CopyString(sample, "!", sizeof(sample));
	UTIL_AppendString(sample, m_Sentences[sentenceIndex].name, sizeof(sample));
	EMIT_SOUND_DYN(entity, channel, sample, volume, attenuation, flags, pitch);

	return sentenceNumber + 1;
}
