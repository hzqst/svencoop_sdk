#ifndef CKEYVALUESWRITER_H
#define CKEYVALUESWRITER_H

#include "CKeyvaluesLexer.h"
#include "FileSystem.h"

class CEscapeSequences;
class CCommand;
class CKeyvalue;
class CKeyvalueNode;
class CKvBlockNode;

class CKeyvaluesWriter
{
public:
	enum
	{
		FILE_PATH_LENGTH = 260 + 1,
		WRITE_BUFFER_SIZE = 512
	};

	CKeyvaluesWriter(const char *fileName,
		const CKeyvaluesLexerSettings &settings = CKeyvaluesLexerSettings());
	CKeyvaluesWriter(const char *fileName, CEscapeSequences &conversions,
		const CKeyvaluesLexerSettings &settings = CKeyvaluesLexerSettings());
	~CKeyvaluesWriter();

	bool Open(const char *fileName);
	void Close(void);
	void Error(const char *message);
	bool WriteTabs(void);
	bool BeginBlock(const char *key);
	bool EndBlock(void);
	bool WriteToken(const char *text);
	bool WriteKeyvalue(const char *key, const char *value);
	bool WriteKeyvalue(const CKeyvalue &keyvalue);
	bool Write(const CKeyvalueNode &node);
	bool WriteBlock(const CKvBlockNode &block);

private:
	friend class CASDocumentation;
	friend class CASHook;
	friend class CWriteFileSystemToFile;
	friend void as_fs_dumpfilesystem_Callback( const CCommand &command );
	friend bool ModuleTypesToKeyvaluesBlock(CKeyvaluesWriter &writer, unsigned char types,
		const char *blockName, const char *keyName);

	CKeyvaluesLexerSettings	m_Settings;
	CEscapeSequences		*m_pConversions;
	FileHandle_t			m_hFile;
	char					m_szFileName[FILE_PATH_LENGTH];
	unsigned int			m_uiDepth;
	bool					m_bError;
};

#endif
