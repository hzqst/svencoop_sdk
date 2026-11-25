#include "extdll.h"
#include "enginecallback.h"
#include "server_filesystem.h"
#include "CKeyvaluesLexer.h"
#include <ctype.h>

CKeyvaluesLexer::CKeyvaluesLexer(const CKeyvaluesLexerSettings &settings)
	: m_Input(0), m_pszReadPosition(NULL), m_TokenType(TOKEN_NONE), m_Settings(settings)
{
}

CKeyvaluesLexer::CKeyvaluesLexer(CMemory<unsigned int> &input,
	const CKeyvaluesLexerSettings &settings)
	: m_Input(0), m_TokenType(TOKEN_NONE), m_Settings(settings)
{
	m_Input.Swap(input);
	m_pszReadPosition = m_Input.Data();
}

CKeyvaluesLexer::CKeyvaluesLexer(const char *fileName,
	const CKeyvaluesLexerSettings &settings)
	: m_Input(0)
{
	// The single-path constructor constructs a separate temporary lexer
	CKeyvaluesLexer(fileName, "GAME", settings);
}

CKeyvaluesLexer::CKeyvaluesLexer(const char *fileName, const char *pathID,
	const CKeyvaluesLexerSettings &settings)
	: m_Input(0), m_pszReadPosition(NULL), m_TokenType(TOKEN_NONE), m_Settings(settings)
{
	if (!fileName || !*fileName)
		return;

	FileHandle_t file = g_pFileSystem->Open(fileName, "rb", pathID);

	if (!file)
		return;

	unsigned int size = g_pFileSystem->Size(file);
	CMemory<unsigned int> input(size);
	int bytesRead = g_pFileSystem->Read(input.Data(), size, file);

	g_pFileSystem->Close(file);

	if (bytesRead == size)
	{
		m_Input.Swap(input);
		m_pszReadPosition = m_Input.Data();
	}
}

CKeyvaluesLexer::~CKeyvaluesLexer()
{
}

bool CKeyvaluesLexer::HasInputData(void) const
{
	return m_Input.Data() != NULL;
}

unsigned int CKeyvaluesLexer::GetReadOffset(void) const
{
	return m_pszReadPosition ? m_pszReadPosition - m_Input.Data() : 0;
}

void CKeyvaluesLexer::Reset(void)
{
	m_pszReadPosition = m_Input.Data();
	m_TokenType = TOKEN_NONE;
	m_Token = "";
}

void CKeyvaluesLexer::Swap(CKeyvaluesLexer &lexer)
{
	if (this == &lexer)
		return;

	m_Input.Swap(lexer.m_Input);

	const char *position = m_pszReadPosition;
	TokenType type = m_TokenType;
	CString token(m_Token);
	CKeyvaluesLexerSettings settings = m_Settings;

	m_pszReadPosition = lexer.m_pszReadPosition;
	m_TokenType = lexer.m_TokenType;
	m_Token = lexer.m_Token;
	m_Settings = lexer.m_Settings;
	lexer.m_pszReadPosition = position;
	lexer.m_TokenType = type;
	lexer.m_Token = token;
	lexer.m_Settings = settings;
}

bool CKeyvaluesLexer::IsValidReadPosition(void)
{
	return (unsigned int)(m_pszReadPosition - m_Input.Data()) < m_Input.Size();
}

void CKeyvaluesLexer::SkipWhitespace(void)
{
	while ( IsValidReadPosition() && isspace( (unsigned char) *m_pszReadPosition ) )
		m_pszReadPosition++;
}

bool CKeyvaluesLexer::SkipCommentLine(void)
{
	SkipWhitespace();

	if (!IsValidReadPosition())
		return false;

	if ( m_pszReadPosition[0] != '/' || m_pszReadPosition[1] != '/' )
		return false;

	m_pszReadPosition += 2;

	while (m_pszReadPosition[-1] != '\n' && IsValidReadPosition())
		m_pszReadPosition++;

	return true;
}

bool CKeyvaluesLexer::SkipComments(void)
{
	bool skipped = SkipCommentLine();

	if (skipped)
	{
		while (SkipCommentLine())
			;
	}

	return skipped;
}

CKeyvaluesLexer::ReadResult CKeyvaluesLexer::ReadNext(
	const char *&start, const char *&end, bool &quoted)
{
	switch (*m_pszReadPosition)
	{
	case '{':
	case '}':
		start = m_pszReadPosition++;
		end = m_pszReadPosition;
		return READ_SUCCESS;

	case '"':
		quoted = true;
		m_pszReadPosition++;
		start = m_pszReadPosition;

		while ( IsValidReadPosition() && *m_pszReadPosition != '"' && *m_pszReadPosition != '\n' )
			m_pszReadPosition++;

		end = m_pszReadPosition;

		if (!IsValidReadPosition())
			return READ_END_OF_DATA;

		if (*m_pszReadPosition != '"' && m_Settings.m_bReportUnclosedQuotes)
			ALERT(at_error, "CKeyvaluesLexer::ReadNext: unclosed quote!\n");

		m_pszReadPosition++;
		return READ_SUCCESS;

	default:
		quoted = false;
		start = m_pszReadPosition;

		while ( IsValidReadPosition() && !isspace( (unsigned char) *m_pszReadPosition ) )
			m_pszReadPosition++;

		end = m_pszReadPosition;
		return READ_SUCCESS;
	}
}

CKeyvaluesLexer::ReadResult CKeyvaluesLexer::ReadNextToken(void)
{
	if (!IsValidReadPosition())
		return READ_END_OF_DATA;

	SkipComments();

	if (!IsValidReadPosition())
		return READ_END_OF_DATA;

	const char *start;
	const char *end;
	bool quoted;
	ReadResult result = ReadNext(start, end, quoted);

	if (result != READ_SUCCESS)
		return result;

	unsigned int length = end - start;

	if (!strncmp(start, "{", length))
	{
		if (m_TokenType == TOKEN_KEY || m_Settings.m_bAllowAnonymousBlocks)
		{
			m_Token = "{";
			m_TokenType = TOKEN_BLOCK_OPEN;
			return READ_SUCCESS;
		}

		if (m_Settings.m_bReportBlockErrors)
			ALERT(at_error, "CKeyvaluesLexer::ReadNextToken: illegal block open '%c'!\n", '{');
	}
	else if (!strncmp(start, "}", length))
	{
		if (m_TokenType == TOKEN_VALUE || m_TokenType == TOKEN_BLOCK_OPEN || m_TokenType == TOKEN_BLOCK_CLOSE)
		{
			m_Token = "}";
			m_TokenType = TOKEN_BLOCK_CLOSE;
			return READ_SUCCESS;
		}

		if (m_Settings.m_bReportBlockErrors)
			ALERT(at_error, "CKeyvaluesLexer::ReadNextToken: illegal block close '%c'!\n", '}');
	}
	else
	{
		m_Token.Assign(start, 0, length);
		m_TokenType = m_TokenType == TOKEN_KEY ? TOKEN_VALUE : TOKEN_KEY;
		return READ_SUCCESS;
	}

	m_Token = "";
	m_TokenType = TOKEN_NONE;
	return READ_FORMAT_ERROR;
}

CKeyvaluesLexer::ReadResult CKeyvaluesLexer::Read(void)
{
	ReadResult result = ReadNextToken();

	if (result == READ_END_OF_DATA)
		m_TokenType = TOKEN_NONE;

	return result;
}
