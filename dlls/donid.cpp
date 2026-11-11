#include "donid.h"
#include <stdio.h>
#include <string.h>

typedef char ExtrasInfoSizeCheck[sizeof( extrasinfo_s ) == 220 ? 1 : -1];
typedef char ExtrasInfoLevelOffsetCheck[offsetof( extrasinfo_s, iLevel ) == 208 ? 1 : -1];
typedef char ExtrasInfoSourceOffsetCheck[offsetof( extrasinfo_s, iSource ) == 212 ? 1 : -1];
typedef char ExtrasInfoQueryOffsetCheck[offsetof( extrasinfo_s, flLastQuery ) == 216 ? 1 : -1];

bool extrasinfo_s::operator==( const extrasinfo_s &other ) const
{
	return !strcmp( szAuthId, other.szAuthId ) && !strcmp( szName, other.szName ) && !strcmp( szDescription, other.szDescription ) && iLevel == other.iLevel && iSource == other.iSource;
}

bool extrasinfo_s::operator!=( const extrasinfo_s &other ) const
{
	return !( *this == other );
}

const char *ExtrasLevelToString( int level )
{
	switch ( level )
	{
		case EXTRAS_INIT:
			return "Init";
		case EXTRAS_ERROR:
			return "Error";
		case EXTRAS_BANNED:
			return "Barred";
		case EXTRAS_NONE:
			return "None";
		case EXTRAS_DONOR:
			return "Donor (electric crowbar and Chumtoad)";
		case EXTRAS_GOLD_UZI:
			return "Donor (gold Uzi)";
		case EXTRAS_CHUMTOAD_HEALTH:
			return "Donor (increased Chumtoad health)";
		case EXTRAS_CONTRIBUTOR:
			return "Staff (contributor/tester)";
		case EXTRAS_ARTIST:
			return "Staff (artist)";
		case EXTRAS_DEVELOPER:
			return "Staff (leader/developer)";
		default:
			return "Unknown";
	}
}

const char *ExtrasSourceToString( int source )
{
	switch ( source )
	{
		case EXTRAS_SOURCE_NONE:
			return "None";
		case EXTRAS_SOURCE_LOCAL:
			return "Local";
		case EXTRAS_SOURCE_ONLINE:
			return "On-line";
		default:
			return "Unknown";
	}
}

static char *g_szPlayerDbParamSalt = NULL;

void EraseDonidCryptKey( void )
{
	delete[] g_szPlayerDbParamSalt;
	g_szPlayerDbParamSalt = NULL;
}

void InitDonidCryptKey( void )
{
	EraseDonidCryptKey();
	g_szPlayerDbParamSalt = new char[DONID_CRYPT_KEY_LENGTH + 1];

	snprintf( g_szPlayerDbParamSalt, DONID_CRYPT_KEY_LENGTH + 1, "%s%s%s%s%s%s%s%s", "j.6v,A?c", "Pk4)zL[8", "9$~GTBM@", "fmR-(U{;",
		"u&3qY]eh", "/p}sbX#Z", "*75Jd=nE", "%DC_:xiK" );
}

void GetDonidCryptKey( char key[DONID_CRYPT_KEY_LENGTH + 1] )
{
	if ( !g_szPlayerDbParamSalt )
		InitDonidCryptKey();

	memcpy( key, g_szPlayerDbParamSalt, DONID_CRYPT_KEY_LENGTH + 1 );
}

bool DonidCryptBuffer( const unsigned char *input, unsigned char *output, size_t length, const char *key )
{
	if ( !input || !output || !length || !key || !*key )
		return false;

	unsigned int state[256];
	size_t keyLength = strlen( key );
	unsigned int j = 0;

	for ( unsigned int i = 0; i < 256; i++ )
		state[i] = i;

	for ( unsigned int i = 0; i < 256; i++ )
	{
		unsigned int value = state[i];

		j = ( j + value + (unsigned char) key[i % keyLength] ) & 255;
		state[i] = state[j];
		state[j] = value;
	}

	j = 0;
	unsigned int i = 0;

	for ( size_t position = 0; position < length; position++ )
	{
		i = ( i + 1 ) & 255;
		unsigned int value = state[i];

		j = ( j + value ) & 255;
		state[i] = state[j];
		state[j] = value;
		output[position] = input[position] ^ (unsigned char) state[( value + state[i] ) & 255];
	}

	return true;
}

bool DecodePlayerDbResponse( extrasinfo_s &extras, const unsigned char *response, size_t length,
                             const char *ipAddress, long long timestamp, const char *authId, const char *key )
{
	extras.iLevel = EXTRAS_ERROR;

	if ( !response || !length || length == (size_t) -1 || !ipAddress || !authId || !key )
		return false;

	char *decoded = new char[length + 1];

	memcpy( decoded, response, length );
	decoded[length] = 0;

	if ( strstr( decoded, "[Critical Error]" ) )
	{
		delete[] decoded;
		return false;
	}

	if ( !DonidCryptBuffer( response, (unsigned char *) decoded, length, key ) )
	{
		delete[] decoded;
		return false;
	}

	decoded[length] = 0;

	size_t readPosition = 0;
	size_t writePosition = 0;

	while ( readPosition < length && decoded[readPosition] )
	{
		while ( readPosition < length && decoded[readPosition] == '|' )
			readPosition++;

		if ( readPosition == length || !decoded[readPosition] )
			break;

		while ( readPosition < length && decoded[readPosition] && decoded[readPosition] != '|' )
			decoded[writePosition++] = decoded[readPosition++];

		if ( readPosition < length && decoded[readPosition] == '|' )
			readPosition++;

		if ( writePosition < length )
			decoded[writePosition++] = ' ';
	}

	decoded[writePosition] = 0;

	char returnedAuthId[EXTRAS_AUTHID_LENGTH + 1] = { 0 };
	char returnedIp[EXTRAS_RESPONSE_IP_LENGTH + 1] = { 0 };
	char returnedTimeZone[EXTRAS_RESPONSE_TIMEZONE_LENGTH + 1] = { 0 };
	char name[EXTRAS_NAME_LENGTH + 1] = { 0 };
	char description[EXTRAS_DESCRIPTION_LENGTH + 1] = { 0 };
	long returnedTimestamp = 0;
	int level = EXTRAS_INIT;
	int fields = sscanf( decoded, "%44s %ld %32s %7s %*[\"]%32[^\"]%*[\"] %*[\"]%128[^\"]%*[\"] %d",
	                     returnedAuthId, &returnedTimestamp, returnedIp, returnedTimeZone, name, description, &level );

	delete[] decoded;

	if ( fields < EXTRAS_RESPONSE_FIELD_COUNT || !returnedIp[0] || !returnedTimestamp || !returnedAuthId[0] || !returnedTimeZone[0]
		|| !name[0] || !description[0] || level < EXTRAS_BANNED || level > EXTRAS_DEVELOPER )
		return false;

	if ( strcmp( returnedIp, ipAddress ) || returnedTimestamp != timestamp || strcmp( returnedAuthId, authId ) )
		return false;

	strcpy( extras.szAuthId, returnedAuthId );
	strcpy( extras.szName, name );
	strcpy( extras.szDescription, description );
	extras.iLevel = level;
	return true;
}
