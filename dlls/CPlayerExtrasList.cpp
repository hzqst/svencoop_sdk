#include "CPlayerExtrasList.h"
#include "ConCommandUtils.h"
#include "donid_data.h"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "../external/miniz/miniz.h"
#include <stdlib.h>
#include <string.h>

int Base64Digit( unsigned char character )
{
	if ( character >= 'A' && character <= 'Z' )
		return character - 'A';

	if ( character >= 'a' && character <= 'z' )
		return character - 'a' + 26;

	if ( character >= '0' && character <= '9' )
		return character - '0' + 52;

	if ( character == '+' )
		return 62;

	if ( character == '/' )
		return 63;

	return -1;
}

unsigned int ReadLittleEndian( const unsigned char *bytes )
{
	return bytes[0] | ( (unsigned int) bytes[1] << 8 ) | ( (unsigned int) bytes[2] << 16 ) | ( (unsigned int) bytes[3] << 24 );
}

CPlayerExtrasList::CPlayerExtrasList()
    : m_uiCount( 0 ), m_pFirst( NULL )
{
}

CPlayerExtrasList::~CPlayerExtrasList()
{
	Clear();
}

void CPlayerExtrasList::Clear()
{
	while ( m_pFirst )
	{
		Entry *entry = m_pFirst;

		m_pFirst = entry->next;
		delete entry;
	}

	m_uiCount = 0;
}

#define GZIP_ID1				0x1F
#define GZIP_ID2				0x8B
#define GZIP_METHOD_DEFLATE	8
#define GZIP_HEADER_SIZE	10
#define GZIP_TRAILER_SIZE	8

bool CPlayerExtrasList::Load()
{
	Clear();

	size_t encodedLength = sizeof( g_szPlayerExtrasList ) - 1;
	unsigned char *compressed = new unsigned char[encodedLength];
	unsigned int accumulator = 0;
	unsigned int bits = 0;
	size_t compressedLength = 0;

	for ( size_t position = 0; position < encodedLength; position++ )
	{
		if ( g_szPlayerExtrasList[position] == '=' )
			break;

		int digit = Base64Digit( g_szPlayerExtrasList[position] );

		if ( digit < 0 )
			continue;

		accumulator = ( accumulator << 6 ) | digit;
		bits += 6;

		if ( bits >= 8 )
		{
			bits -= 8;
			compressed[compressedLength++] = (unsigned char) ( accumulator >> bits );
		}
	}

	// The embedded gzip member has a ten-byte header with no optional fields
	if ( compressedLength < GZIP_HEADER_SIZE + GZIP_TRAILER_SIZE || compressed[0] != GZIP_ID1 || compressed[1] != GZIP_ID2
		|| compressed[2] != GZIP_METHOD_DEFLATE || compressed[3] != 0 )
	{
		delete[] compressed;
		return false;
	}

	size_t length = 0;
	char *text = (char *) tinfl_decompress_mem_to_heap( compressed + GZIP_HEADER_SIZE, compressedLength - GZIP_HEADER_SIZE - GZIP_TRAILER_SIZE, &length, 0 );
	const unsigned char *trailer = compressed + compressedLength - GZIP_TRAILER_SIZE;
	bool valid = text && length == ReadLittleEndian( trailer + 4 )
		&& (unsigned int) mz_crc32( 0, (const unsigned char *) text, length ) == ReadLittleEndian( trailer );

	delete[] compressed;

	if ( valid )
		valid = LoadCSV( text, length );

	free( text );
	return valid;
}

bool CPlayerExtrasList::LoadCSV( const char *text, size_t length )
{
	Clear();

	if ( !text || !length || length == (size_t) -1 )
		return false;

	char *field = new char[length + 1];
	size_t position = 0;
	Entry *last = NULL;
	bool valid = true;

	while ( position < length )
	{
		extrasinfo_s info = { 0 };
		unsigned int column = 0;
		bool endOfRow = false;

		while ( !endOfRow )
		{
			size_t fieldLength = 0;
			bool quoted = position < length && text[position] == '"';
			bool closed = !quoted;

			if ( quoted )
				position++;

			while ( position < length )
			{
				char character = text[position++];

				if ( quoted && character == '"' )
				{
					if ( position < length && text[position] == '"' )
					{
						field[fieldLength++] = '"';
						position++;
						continue;
					}

					closed = true;
					break;
				}

				if ( !quoted && ( character == ',' || character == '\r' || character == '\n' ) )
				{
					position--;
					break;
				}

				field[fieldLength++] = character;
			}

			field[fieldLength] = 0;

			if ( !closed || column >= 4 )
			{
				valid = false;
				break;
			}

			char *destination = column == 0 ? info.szAuthId : column == 1 ? info.szName
			                                                              : info.szDescription;
			size_t capacity = column == 0 ? sizeof( info.szAuthId ) : column == 1 ? sizeof( info.szName )
			                                                                      : sizeof( info.szDescription );

			if ( column == 3 )
				info.iLevel = atoi( field );
			else
			{
				size_t copyLength = fieldLength < capacity - 1 ? fieldLength : capacity - 1;

				memcpy( destination, field, copyLength );
				destination[copyLength] = 0;
			}

			column++;

			if ( position < length && text[position] == ',' )
				position++;
			else
			{
				if ( position < length && text[position] == '\r' )
					position++;

				if ( position < length && text[position] == '\n' )
					position++;
				else if ( position < length )
					valid = false;

				endOfRow = true;
			}
		}

		if ( !valid || column != 4 )
		{
			valid = false;
			break;
		}

		if ( !info.szAuthId[0] || info.iLevel < EXTRAS_BANNED || info.iLevel > EXTRAS_DEVELOPER )
			continue;

		info.iSource = EXTRAS_SOURCE_LOCAL;
		info.flLastQuery = -1.0f;
		Entry *entry = new Entry;

		entry->info = info;
		entry->next = NULL;
		entry->previous = last;

		if ( last )
			last->next = entry;
		else
			m_pFirst = entry;

		last = entry;
		m_uiCount++;
	}

	delete[] field;

	if ( !valid )
		Clear();

	return valid;
}

unsigned int CPlayerExtrasList::Count() const
{
	return m_uiCount;
}

const extrasinfo_s *CPlayerExtrasList::Get( unsigned int index ) const
{
	const Entry *entry = m_pFirst;

	while ( entry && index-- )
		entry = entry->next;

	return entry ? &entry->info : NULL;
}

const extrasinfo_s *CPlayerExtrasList::Find( const char *authId ) const
{
	if ( !authId )
		return NULL;

	bool wildcard = strchr( authId, '*' ) != NULL;

	for ( const Entry *entry = m_pFirst; entry; entry = entry->next )
	{
		if ( ( !wildcard && !strcmp( entry->info.szAuthId, authId ) ) || UTIL_StringWildcardSearch( authId, entry->info.szAuthId, false, false ) )
			return &entry->info;
	}

	return NULL;
}
