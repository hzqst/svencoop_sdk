#include "extdll.h"
#include "SvenVersion.h"
#include "util.h"
#include "CPlayerDatabase.h"
#ifdef _WIN32
#include "../external/curl-win32/curl/curl.h"
#else
#include <curl/curl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exception>
#include <new>
#include <stdexcept>

static const char g_szPlayerDbUrl[] = "https://gameinfo.svencoop.com:443/public/playerextras.php";

#define SERVICE_ERROR_LENGTH 128
static bool g_bPlayerDbInitialized;

CPlayerDatabase::CPlayerDatabase( const char *authId, const char *ipAddress, long long timestamp,
                                  float queryTime, const char *url )
    : m_bStarted( false ), m_bFinished( false ), m_bCancelled( false ),
      m_iTimestamp( timestamp ), m_flQueryTime( queryTime ), m_pResponse( NULL ), m_uiResponseLength( 0 ),
      m_pQueryResult( NULL ), m_iRequestCode( CURLE_OK )
{
#ifdef _WIN32
	m_hThread = NULL;
#else
	memset( &m_Thread, 0, sizeof( m_Thread ) );
#endif
	memset( &m_Result, 0, sizeof( m_Result ) );

	m_Result.iLevel = EXTRAS_INIT;
	m_Result.flLastQuery = -1.0f;
	m_szError[0] = 0;

	snprintf( m_szAuthId, sizeof( m_szAuthId ), "%s", authId ? authId : "" );
	snprintf( m_szIpAddress, sizeof( m_szIpAddress ), "%s", ipAddress ? ipAddress : "" );
	snprintf( m_szUrl, sizeof( m_szUrl ), "%s", url ? url : g_szPlayerDbUrl );
	snprintf( m_Result.szAuthId, sizeof( m_Result.szAuthId ), "%s", m_szAuthId );
}

CPlayerDatabase::~CPlayerDatabase()
{
	Cancel();
	Join();
	free( m_pResponse );
}

class CCurlInit
{
public:
	CCurlInit() : m_bInitialized( curl_global_init( CURL_GLOBAL_DEFAULT ) == CURLE_OK ) {}

	~CCurlInit()
	{
		if ( m_bInitialized )
			curl_global_cleanup();
	}

	bool m_bInitialized;
};

static CCurlInit *g_pCurlInit = NULL;

bool CPlayerDatabase::Initialize()
{
	if ( !g_pCurlInit )
		g_pCurlInit = new CCurlInit;

	InitDonidCryptKey();

	g_bPlayerDbInitialized = g_pCurlInit->m_bInitialized;
	return g_bPlayerDbInitialized;
}

void CPlayerDatabase::Shutdown()
{
	delete g_pCurlInit;
	g_pCurlInit = NULL;

	EraseDonidCryptKey();
	g_bPlayerDbInitialized = false;
}

bool CPlayerDatabase::Start()
{
	if ( m_bStarted )
		return false;

#ifdef _WIN32
	m_hThread = CreateThread( NULL, 0, ThreadEntry, this, 0, NULL );
	m_bStarted = m_hThread != NULL;
#else
	m_bStarted = pthread_create( &m_Thread, NULL, ThreadEntry, this ) == 0;
#endif
	return m_bStarted;
}

void CPlayerDatabase::Cancel()
{
	m_Mutex.Lock();
	m_bCancelled = true;
	m_Mutex.Unlock();
}

bool CPlayerDatabase::IsCancelled()
{
	m_Mutex.Lock();
	bool cancelled = m_bCancelled;

	m_Mutex.Unlock();
	return cancelled;
}

bool CPlayerDatabase::IsFinished()
{
	m_Mutex.Lock();
	bool finished = m_bFinished;

	m_Mutex.Unlock();
	return finished;
}

void CPlayerDatabase::Join()
{
	if ( !m_bStarted )
		return;

#ifdef _WIN32
	WaitForSingleObject( m_hThread, INFINITE );
	CloseHandle( m_hThread );
	m_hThread = NULL;
#else
	pthread_join( m_Thread, NULL );
#endif
	m_bStarted = false;
}

const extrasinfo_s &CPlayerDatabase::GetResult() const
{
	return m_Result;
}

const char *CPlayerDatabase::GetError() const
{
	return m_szError;
}

#ifdef _WIN32
DWORD WINAPI CPlayerDatabase::ThreadEntry( void *context )
#else
void *CPlayerDatabase::ThreadEntry( void *context )
#endif
{
	CPlayerDatabase *database = (CPlayerDatabase *) context;

	try
	{
		if ( !database->IsCancelled() )
			database->GetPlayerExtras( &database->m_Result );
	}
	catch ( const std::exception &error )
	{
		database->m_Result.iLevel = EXTRAS_ERROR;
		snprintf( database->m_szError, sizeof( database->m_szError ), "%s", error.what() );
	}

	database->m_Mutex.Lock();
	database->m_bFinished = true;
	database->m_Mutex.Unlock();

	return 0;
}

size_t CPlayerDatabase::OnData( char *data, size_t size, size_t count, void *context )
{
	CPlayerDatabase *database = (CPlayerDatabase *) context;

	if ( ( size && count > (size_t) -1 / size ) || database->IsCancelled() )
		return 0;

	size_t length = size * count;

	if ( length >= (size_t) -1 - database->m_uiResponseLength )
		return 0;

	char *response = (char *) realloc( database->m_pResponse, database->m_uiResponseLength + length + 1 );

	if ( !response )
		return 0;

	database->m_pResponse = response;
	memcpy( response + database->m_uiResponseLength, data, length );
	database->m_uiResponseLength += length;
	response[database->m_uiResponseLength] = 0;

	return length;
}

bool CPlayerDatabase::PerformHttpRequest()
{
	if ( !m_szAuthId[0] || !m_iTimestamp || !m_szIpAddress[0] )
	{
		ALERT( at_warning, "CPlayerDatabase: Query preperation failed.\n" );
		free( m_pResponse );
		m_pResponse = NULL;
		m_uiResponseLength = 0;
		return false;
	}

	char plain[512];
	unsigned char encrypted[512];
	char key[DONID_CRYPT_KEY_LENGTH + 1];
	int length = snprintf( plain, sizeof( plain ), "ipaddr=%s\ntimestamp=%ld\nauthid=%s\n",
	                       m_szIpAddress, (long) m_iTimestamp, m_szAuthId );

	GetDonidCryptKey( key );
	DonidCryptBuffer( (const unsigned char *) plain, encrypted, length, key );

	m_iRequestCode = CURLE_OK;
	m_pQueryResult->flLastQuery = m_flQueryTime;

	free( m_pResponse );
	m_pResponse = NULL;
	m_uiResponseLength = 0;
	m_szError[0] = 0;

	try
	{
		CURL *curl = curl_easy_init();

		if ( !curl )
			throw std::runtime_error( "CPlayerDatabase: cURL initialisation failed." );

		char contentLength[512];
		char referer[512];
		curl_slist *headers = NULL;

		snprintf( contentLength, sizeof( contentLength ), "Content-Length: %d", length );
		snprintf( referer, sizeof( referer ), "Referer: %s", m_szIpAddress );
		headers = curl_slist_append( headers, "Accept: text/plain" );
		headers = curl_slist_append( headers, "Accept-Encoding: utf-8" );
		headers = curl_slist_append( headers, "Cache-Control: no-cache" );
		headers = curl_slist_append( headers, "Connection: close" );
		headers = curl_slist_append( headers, contentLength );
		headers = curl_slist_append( headers, "Content-Type: application/x-www-form-urlencoded" );
		headers = curl_slist_append( headers, referer );
		headers = curl_slist_append( headers, "User-Agent: Sven Co-op/" SVEN_GAME_VERSION_STRING );
		curl_easy_setopt( curl, CURLOPT_URL, m_szUrl );
		curl_easy_setopt( curl, CURLOPT_CUSTOMREQUEST, "PUT" );
		curl_easy_setopt( curl, CURLOPT_POSTFIELDS, encrypted );
		curl_easy_setopt( curl, CURLOPT_POSTFIELDSIZE, (long) length );
		curl_easy_setopt( curl, CURLOPT_HTTPHEADER, headers );
		curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, OnData );
		curl_easy_setopt( curl, CURLOPT_WRITEDATA, this );
		m_iRequestCode = curl_easy_perform( curl );
		curl_slist_free_all( headers );
		curl_easy_cleanup( curl );

		if ( !m_pResponse )
		{
			m_pResponse = (char *) malloc( 1 );

			if ( !m_pResponse )
				throw std::bad_alloc();

			m_pResponse[0] = 0;
		}

		return true;
	}
	catch ( const std::exception &error )
	{
		ALERT( at_warning, "CPlayerDatabase: Request error. (%s)\n", error.what() );
		free( m_pResponse );
		m_pResponse = NULL;
		m_uiResponseLength = 0;
		return false;
	}
}

void CPlayerDatabase::DecodeServerResponse()
{
	if ( m_iRequestCode != CURLE_OK )
	{
		m_pQueryResult->iLevel = EXTRAS_ERROR;
		return;
	}

	if ( strstr( m_pResponse, "[Critical Error]" ) )
	{
		char serviceError[SERVICE_ERROR_LENGTH + 1] = { 0 };

		if ( sscanf( m_pResponse, "[Critical Error] %[^\t\n]", serviceError ) == 1 )
		{
			serviceError[SERVICE_ERROR_LENGTH] = 0;
			ALERT( at_warning, "CPlayerDatabase: Service error. (%s)\n", serviceError );
		}
		else
			ALERT( at_warning, "CPlayerDatabase: Unknown service error.\n" );

		m_pQueryResult->iLevel = EXTRAS_ERROR;
		return;
	}

	char *decoded = NULL;
	char *fields = NULL;

	try
	{
		char key[DONID_CRYPT_KEY_LENGTH + 1];

		decoded = new char[m_uiResponseLength + 1];
		memset( decoded, 0, m_uiResponseLength + 1 );

		GetDonidCryptKey( key );
		DonidCryptBuffer( (const unsigned char *) m_pResponse, (unsigned char *) decoded, m_uiResponseLength, key );
		fields = new char[m_uiResponseLength + 1];
		memset( fields, 0, m_uiResponseLength + 1 );

		size_t readPosition = 0;
		size_t writePosition = 0;

		while ( readPosition < m_uiResponseLength && decoded[readPosition] )
		{
			while ( readPosition < m_uiResponseLength && decoded[readPosition] == '|' )
				readPosition++;

			if ( readPosition == m_uiResponseLength || !decoded[readPosition] )
				break;

			while ( readPosition < m_uiResponseLength && decoded[readPosition] && decoded[readPosition] != '|' )
				fields[writePosition++] = decoded[readPosition++];

			if ( writePosition < m_uiResponseLength )
				fields[writePosition++] = ' ';
		}

		delete[] decoded;
		decoded = NULL;

		char returnedAuthId[EXTRAS_AUTHID_LENGTH + 1] = { 0 };
		char returnedIp[EXTRAS_RESPONSE_IP_LENGTH + 1] = { 0 };
		char returnedTimeZone[EXTRAS_RESPONSE_TIMEZONE_LENGTH + 1] = { 0 };
		char name[EXTRAS_NAME_LENGTH + 1] = { 0 };
		char description[EXTRAS_DESCRIPTION_LENGTH + 1] = { 0 };
		long returnedTimestamp = 0;
		int level = EXTRAS_INIT;
		int count = sscanf( fields, "%44s %ld %32s %7s %*[\"]%32[^\"]%*[\"] %*[\"]%128[^\"]%*[\"] %d",
		                    returnedAuthId, &returnedTimestamp, returnedIp, returnedTimeZone, name, description, &level );

		delete[] fields;
		fields = NULL;

		if ( count < EXTRAS_RESPONSE_FIELD_COUNT )
		{
			ALERT( at_warning, "CPlayerDatabase: Malformed look-up response.\n" );
			m_pQueryResult->iLevel = EXTRAS_ERROR;
			return;
		}

		if ( level == EXTRAS_INIT || level == EXTRAS_ERROR || !returnedAuthId[0] || !returnedTimestamp
		     || !returnedIp[0] || !returnedTimeZone[0] || !name[0] || !description[0] )
		{
			ALERT( at_warning, "CPlayerDatabase: Malformed look-up data.\n" );
			m_pQueryResult->iLevel = EXTRAS_ERROR;
			return;
		}

		if ( level < EXTRAS_BANNED || level > EXTRAS_DEVELOPER || strcmp( returnedAuthId, m_szAuthId )
		     || returnedTimestamp != m_iTimestamp || strcmp( returnedIp, m_szIpAddress ) )
		{
			ALERT( at_warning, "CPlayerDatabase: Mismatched query response.\n" );
			m_pQueryResult->iLevel = EXTRAS_ERROR;
			return;
		}

		if ( level == EXTRAS_BANNED )
			ALERT( at_aiconsole, "CPlayerDatabase: Downloaded, %s is shadow banned.\n", returnedAuthId );
		else if ( level == EXTRAS_NONE )
			ALERT( at_aiconsole, "CPlayerDatabase: Downloaded, %s has no extras.\n", returnedAuthId );
		else if ( description[0] )
			ALERT( at_aiconsole, "CPlayerDatabase: Downloaded, %s (%s: %s) is level %d.\n", returnedAuthId, name, description, level );
		else
			ALERT( at_aiconsole, "CPlayerDatabase: Downloaded, %s (%s) is level %d.\n", returnedAuthId, name, level );

		strcpy( m_pQueryResult->szAuthId, returnedAuthId );
		strcpy( m_pQueryResult->szName, name );
		strcpy( m_pQueryResult->szDescription, description );

		m_pQueryResult->iLevel = level;
	}
	catch ( const std::bad_alloc &error )
	{
		delete[] decoded;
		delete[] fields;

		ALERT( at_warning, "CPlayerDatabase: Response error. (Memory allocation failed: %s)\n", error.what() );
		m_pQueryResult->iLevel = EXTRAS_ERROR;
	}
}

void CPlayerDatabase::GetPlayerExtras( extrasinfo_s *extras )
{
	m_pQueryResult = extras;

	if ( !extras || !extras->szAuthId[0] )
		return;

	if ( extras->flLastQuery > 0.0f && extras->flLastQuery + 10.0f > m_flQueryTime )
		return;

	snprintf( m_szAuthId, sizeof( m_szAuthId ), "%s", extras->szAuthId );

	if ( !g_bPlayerDbInitialized )
	{
		ALERT( at_warning, "CPlayerDatabase: cURL failed to initialize. Can't request extras.\n" );
		extras->iLevel = EXTRAS_ERROR;
		return;
	}

	if ( PerformHttpRequest() )
	{
		extras->iSource = EXTRAS_SOURCE_ONLINE;

		if ( m_pResponse && m_uiResponseLength >= 1 && m_iRequestCode == CURLE_OK )
			DecodeServerResponse();
		else
		{
			ALERT( at_warning, "CPlayerDatabase: Response is empty, or HTTP request is not OK. (%d)\n", m_iRequestCode );
			extras->iLevel = EXTRAS_ERROR;
		}
	}
	else
	{
		ALERT( at_warning, "CPlayerDatabase: Request failed.\n" );
		extras->iLevel = EXTRAS_ERROR;
	}

	free( m_pResponse );
	m_pResponse = NULL;
	m_uiResponseLength = 0;
}
