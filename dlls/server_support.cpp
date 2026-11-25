#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "server_support.h"
#include "CHashStringPool.h"
#include "entvars_types.h"
#include "RGBA.h"
#include "CCommand.h"
#include "CConCommand.h"
#include <errno.h>
#include <stdarg.h>

#ifdef _WIN32
#include <winsock.h>
#else
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

CHashStringPool g_StringPool;

namespace
{
enum
{
	ENTITY_LOG_BUFFER_COUNT = 5,
	ENTITY_LOG_BUFFER_SIZE = 512
};

char g_EntityLogBuffers[ENTITY_LOG_BUFFER_COUNT][ENTITY_LOG_BUFFER_SIZE];
int g_iEntityLogBuffer;
}

void UTIL_GetCircularGaussianSpread( float &x, float &y )
{
	do
	{
		float ( *randomFloat )( float, float ) = g_engfuncs.pfnRandomFloat;
		float first = randomFloat( -0.5f, 0.5f );

		x = randomFloat( -0.5f, 0.5f ) + first;

		randomFloat = g_engfuncs.pfnRandomFloat;
		first = randomFloat( -0.5f, 0.5f );
		y = randomFloat( -0.5f, 0.5f ) + first;
	} while ( x * x + y * y > 1.0f );
}

char *EntityLogString( const char *name, const char *userID, const char *authID, const char *team )
{
	g_iEntityLogBuffer++;

	if ( g_iEntityLogBuffer >= ENTITY_LOG_BUFFER_COUNT )
		g_iEntityLogBuffer = 0;

	char *buffer = g_EntityLogBuffers[g_iEntityLogBuffer];

	if ( !name || !*name )
	{
		UTIL_CopyString( buffer, "invalid_ent", ENTITY_LOG_BUFFER_SIZE );
		return buffer;
	}

	UTIL_FormatString( buffer, ENTITY_LOG_BUFFER_SIZE, "%s<%s><%s><%s>", name,
	                   userID && *userID ? userID : "?", authID && *authID ? authID : "?",
	                   team && *team ? team : "?" );

	return buffer;
}

char *PlayerLogString( const char *name, const char *userID, const char *authID )
{
	return EntityLogString( name, userID, authID, "players" );
}

char *PlayerLog( edict_t *entity )
{
	char name[32] = { 0 };
	char userId[12] = { 0 };
	char authId[44] = { 0 };
	char className[64] = { 0 };

	UTIL_CopyString( name, STRING( entity->v.classname ), sizeof( name ) );
	UTIL_CopyString( userId, "generic_entity", sizeof( userId ) );
	UTIL_CopyString( authId, "NoAuthID", sizeof( authId ) );

	CBaseEntity *base = entity->free ? NULL : CBaseEntity::Instance( entity );

	if ( base )
	{
		UTIL_CopyString( className, base->GetClassificationTag(), sizeof( className ) );

		CBaseMonster *monster = base->MyMonsterPointer();
		CBasePlayer *player = base->IsPlayer() ? dynamic_cast<CBasePlayer *>( base ) : NULL;

		if ( player )
		{
			UTIL_CopyString( name, STRING( player->pev->netname ), sizeof( name ) );
			sprintf( userId, "%i", GETPLAYERUSERID( entity ) );
			UTIL_CopyString( authId, GETPLAYERAUTHID( entity ), sizeof( authId ) );
		}
		else if ( monster )
		{
			UTIL_CopyString( name, STRING( monster->pev->classname ), sizeof( name ) );
			UTIL_CopyString( userId, "monster", sizeof( userId ) );
		}
	}

	return EntityLogString( name, userId, authId, className );
}

bool UTIL_IsPlayerConnected( CBasePlayer *player )
{
	return player && player->IsConnected();
}

CBasePlayer *UTIL_CBasePlayerByName( const char *name, bool caseSensitive )
{
	if ( !name )
		return NULL;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		edict_t *entity = INDEXENT( i );

		if ( !entity || entity->free || !OFFSET( entity ) || entity->free )
			continue;

		CBaseEntity *base = CBaseEntity::Instance( entity );

		if ( !base || !base->IsPlayer() )
			continue;

		CBasePlayer *player = dynamic_cast<CBasePlayer *>( base );

		if ( !player || !player->IsConnected() )
			continue;

		const char *playerName = STRING( player->pev->netname );

		if ( !( caseSensitive ? strcmp( playerName, name ) : stricmp( playerName, name ) ) )
			return player;
	}

	return NULL;
}

void WRITE_RGBA(const RGBA &color)
{
	WRITE_BYTE(color.r);
	WRITE_BYTE(color.g);
	WRITE_BYTE(color.b);
	WRITE_BYTE(color.a);
}

bool UTIL_StringToRGBA(RGBA &color, const char *text, char separator)
{
	enum { RGB_COMPONENT_COUNT = 3, RGBA_COMPONENT_COUNT = 4, OPAQUE_ALPHA = 255 };
	unsigned int separators = 0;

	if (!*text)
		return false;

	for (const char *cursor = text; *cursor; cursor++)
	{
		if (*cursor == separator)
			separators++;
	}

	if (separators != RGB_COMPONENT_COUNT - 1 && separators != RGBA_COMPONENT_COUNT - 1)
		return false;

	float values[RGBA_COMPONENT_COUNT] = { 0, 0, 0, OPAQUE_ALPHA };

	if (!UTIL_StringToVectorND(values, separators + 1, text, separator))
		return false;

	color.r = RGBAComponentFromFloat(values[0]);
	color.g = RGBAComponentFromFloat(values[1]);
	color.b = RGBAComponentFromFloat(values[2]);
	color.a = RGBAComponentFromFloat(values[3]);
	return true;
}

int UTIL_FormatString(char *destination, unsigned int size, const char *format, ...)
{
	if (!format || !destination)
		return 0;

	unsigned int formatLength = strlen(format);

	if (!size)
		return 0;

	memset(destination, 0, size);

	if (!formatLength)
		return 0;

	va_list arguments;

	va_start(arguments, format);
	int result = vsnprintf(destination, size, format, arguments);
	va_end(arguments);

	destination[size - 1] = 0;
	return result < 0 ? -1 : result;
}

char *UTIL_FixSlashes(char *path)
{
	if (!path)
		return NULL;

	for (char *cursor = path; *cursor; cursor++)
	{
		if (*cursor == '\\')
			*cursor = '/';
	}

	return path;
}

char *UTIL_CopyString(char *destination, const char *source, unsigned int size)
{
	if (!source || !destination || !size)
		return NULL;

	memset(destination, 0, size);

	unsigned int length = strlen(source);

	if (!length)
		return destination;

	return strncpy(destination, source, length < size ? size : size - 1);
}

char *UTIL_AppendString(char *destination, const char *source, unsigned int size)
{
	if (!source || !destination || !size)
		return destination;

	unsigned int length = strlen(destination);
	unsigned int remaining = size - length - 1;
	unsigned int sourceLength = strlen(source);

	if (!remaining)
		return destination;

	memset(destination + length, 0, size - length);

	if (!sourceLength)
		return destination;

	return strncat(destination, source, sourceLength <= remaining ? remaining : remaining - 1);
}

char *UTIL_GetMapName(void)
{
	return (char *)STRING(gpGlobals->mapname);
}

void UTIL_HostError(const char *message, int status)
{
	unsigned int length = strlen(message);
	char *text = new char[length + 2];

	snprintf(text, length + 1, "%s\n", message);
	ALERT(at_error, text);

#ifdef _WIN32
	MessageBoxA(NULL, text, "Critical Error", MB_SETFOREGROUND | MB_ICONERROR);
#else
	errno = status;
	perror(text);
#endif

	delete[] text;

	if (status)
	{
		if (IS_DEDICATED_SERVER())
			exit(status);

		SERVER_COMMAND("shutdownserver\n");
		SERVER_EXECUTE();
	}
}

int AllocStringLocal(const char *text)
{
	const char *string = g_StringPool.Allocate(text, NULL);

	if (!string)
	{
		g_StringPool.DebugPrint();
		UTIL_HostError("String pool ran out of memory, shutting down!\n", -1);
	}

	return string - STRING(0);
}

const char *GetIpAddr(void)
{
	char name[64];
#ifdef _WIN32
	WSADATA data;

	if (WSAStartup(MAKEWORD(1, 1), &data) != 0)
	{
		WSACleanup();
		return "NULL";
	}
#endif
	if (gethostname(name, sizeof(name)) != 0)
	{
#ifdef _WIN32
		WSACleanup();
#endif
		return "NULL";
	}

	name[sizeof(name) - 1] = 0;

	hostent *host = gethostbyname(name);

	if (!host)
	{
#ifdef _WIN32
		WSACleanup();
#endif
		return "NULL";
	}

	const char *address = NULL;

	for (int i = 0; host->h_addr_list[i]; i++)
	{
		address = inet_ntoa(*(in_addr *)host->h_addr_list[i]);

		if (strncmp(address, "127.", 4) && strcmp(address, "::1"))
			break;
	}

#ifdef _WIN32
	WSACleanup();
#endif
	return address;
}

void string_memusage_Callback( const CCommand &command )
{
	g_StringPool.DebugPrint();
}

CConCommand string_memusage( "string_memusage", string_memusage_Callback, ConCommandFlag::AdminOnly, "Outputs the game's string pool memory usage to the console" );

void Message_Begin_Intercept( int msg_dest, int msg_type, const float *origin, edict_t *entity )
{
}

void CVAR_SetStringSilent( const char *name, const char *value )
{
}

void ClassFileParser( void )
{
}

class CASInstanceTracker
{
public:
	CASInstanceTracker() : m_iCount( 0 ) {}

	void OutputReport( const char *name ) const {}

	int m_iCount;
};
