#ifndef DONID_H
#define DONID_H

#include <stddef.h>

#define EXTRAS_AUTHID_LENGTH			44
#define EXTRAS_NAME_LENGTH				32
#define EXTRAS_DESCRIPTION_LENGTH		128
#define EXTRAS_IP_ADDRESS_LENGTH		64
#define EXTRAS_RESPONSE_IP_LENGTH		32
#define EXTRAS_RESPONSE_TIMEZONE_LENGTH	7
#define EXTRAS_RESPONSE_FIELD_COUNT		7
#define DONID_CRYPT_KEY_LENGTH			64

enum ExtrasLevel_t
{
	EXTRAS_INIT = -3,
	EXTRAS_ERROR = -2,
	EXTRAS_BANNED = -1,
	EXTRAS_NONE = 0,
	EXTRAS_DONOR = 1,
	EXTRAS_GOLD_UZI = 2,
	EXTRAS_CHUMTOAD_HEALTH = 3,
	EXTRAS_CONTRIBUTOR = 4,
	EXTRAS_ARTIST = 5,
	EXTRAS_DEVELOPER = 6
};

enum ExtrasSource_t
{
	EXTRAS_SOURCE_NONE = 0,
	EXTRAS_SOURCE_LOCAL,
	EXTRAS_SOURCE_ONLINE
};

struct extrasinfo_s
{
	char	szAuthId[EXTRAS_AUTHID_LENGTH + 1];
	char	szName[EXTRAS_NAME_LENGTH + 1];
	char	szDescription[EXTRAS_DESCRIPTION_LENGTH + 1];
	int		iLevel;
	int		iSource;
	float	flLastQuery;

	bool operator==( const extrasinfo_s &other ) const;
	bool operator!=( const extrasinfo_s &other ) const;
};

const char *ExtrasLevelToString( int level );
const char *ExtrasSourceToString( int source );
void InitDonidCryptKey( void );
void EraseDonidCryptKey( void );
void GetDonidCryptKey( char key[DONID_CRYPT_KEY_LENGTH + 1] );

// The wire format uses a fresh RC4 stream for each request and response
bool DonidCryptBuffer( const unsigned char *input, unsigned char *output, size_t length, const char *key );
bool DecodePlayerDbResponse( extrasinfo_s &extras, const unsigned char *response, size_t length,
                             const char *ipAddress, long long timestamp, const char *authId, const char *key );

#endif
