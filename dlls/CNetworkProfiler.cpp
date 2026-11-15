#include "extdll.h"
#include "enginecallback.h"
#include "FileSystem.h"
#include "CNetworkProfiler.h"
#include <algorithm>
#include <sstream>

#define REPORT_CONSOLE_CHUNK_SIZE 512

CNetworkProfiler::CNetworkProfiler()
{
	Reset();

	for ( unsigned int i = 0; i < MAX_USER_MESSAGES; i++ )
		m_pUserMessages[i] = &m_UserMessages[i];
}

CNetworkProfiler::~CNetworkProfiler()
{
}

bool CNetworkProfiler::UserMsgProfileDataSorter_t::operator()( const UserMsgProfileData_t *left, const UserMsgProfileData_t *right ) const
{
	// Not a strict ordering: equal totals compare true
	return left->m_uiBytes >= right->m_uiBytes;
}

void CNetworkProfiler::Reset()
{
	for ( unsigned int i = 0; i < MAX_USER_MESSAGES; i++ )
	{
		m_UserMessages[i].m_uiIndex = i;
		m_UserMessages[i].m_uiBytes = 0;
	}
}

void CNetworkProfiler::Think()
{
}

void CNetworkProfiler::OutputArrayToString( std::string &output, const UserMsgProfileData_t *const *messages, unsigned int count )
{
	output.clear();

	std::ostringstream stream;

	for ( unsigned int i = 0; i < count; i++ )
	{
		const UserMsgProfileData_t *profile = messages[i];

		if ( !profile->m_uiBytes )
			continue;

		stream << profile->m_uiIndex << ' ';

		const UserMessageData_t *message = CNetworkMessages::GetInstance()->GetUserMessageData( profile->m_uiIndex );

		if ( message )
		{
			if ( message->m_iSize != USER_MESSAGE_UNREGISTERED_INDEX )
				stream << message->m_iSize << ' ';

			if ( message->m_pszName )
				stream << message->m_pszName << ' ';
		}

		stream << profile->m_uiBytes << std::endl;
	}

	output = stream.str();
}

void CNetworkProfiler::GenerateReport( std::string &output, ReportMode_t mode, unsigned int topN )
{
	if ( mode == REPORT_TOP_N )
	{
		if ( !topN || topN >= MAX_USER_MESSAGES )
		{
			ALERT( at_console, "CNetworkProfiler::GenerateReport: top N amount must be valid!\n" );
			return;
		}

		const UserMsgProfileData_t *messages[MAX_USER_MESSAGES];

		memcpy( messages, m_pUserMessages, sizeof( messages ) );
		std::sort( messages, messages + MAX_USER_MESSAGES, UserMsgProfileDataSorter_t() );
		OutputArrayToString( output, messages, topN );
		return;
	}

	OutputArrayToString( output, m_pUserMessages, MAX_USER_MESSAGES );
}

void CNetworkProfiler::OutputToConsole( ReportMode_t mode, unsigned int topN )
{
	std::string output;

	GenerateReport( output, mode, topN );
	ALERT( at_console, "Network message report begin:\n" );

	for ( unsigned int offset = 0; offset < output.size(); offset += REPORT_CONSOLE_CHUNK_SIZE )
		ALERT( at_console, "%s", output.substr( offset, REPORT_CONSOLE_CHUNK_SIZE ).c_str() );

	ALERT( at_console, "Network message report end\n" );
}

void CNetworkProfiler::OutputToFile( const char *fileName, ReportMode_t mode, unsigned int topN )
{
	if ( !fileName )
	{
		ALERT( at_console, "CNetworkProfiler::Output: no file specified!\n" );
		return;
	}

	std::string output;
	char path[MAX_PATH + 1];

	GenerateReport( output, mode, topN );

	if ( (unsigned int) snprintf( path, sizeof( path ), "%s.txt", fileName ) >= sizeof( path ) )
	{
		ALERT( at_console, "CNetworkProfiler::Output: could not format filename!\n" );
		return;
	}

	FileHandle_t file = g_pFileSystem->Open( path, "w", NULL );

	if ( !file )
	{
		ALERT( at_console, "CNetworkProfiler::Output: could not open file for reading!\n" );
		return;
	}

	g_pFileSystem->FPrintf( file, "Network message report begin:\n%sNetwork message report end\n", output.c_str() );
	g_pFileSystem->Close( file );
}

void CNetworkProfiler::OutputUsage( const char *command )
{
	if ( !stricmp( command, "network_generate_report_file" ) )
		ALERT( at_console, "network_generate_report_file usage: network_generate_report_file <filename> <reportMode(list|topN)> <topN(top N report mode only)>\n" );
}

bool CNetworkProfiler::HandleCommand( const char *command )
{
	if ( stricmp( command, "network_generate_report_file" ) )
		return false;

	int argc = CMD_ARGC();

	if ( argc > 2 )
	{
		const char *mode = CMD_ARGV( 2 );
		ReportMode_t reportMode;

		if ( !stricmp( mode, "list" ) )
			reportMode = REPORT_LIST;
		else if ( !stricmp( mode, "topN" ) )
			reportMode = REPORT_TOP_N;
		else
		{
			OutputUsage( command );
			return true;
		}

		unsigned int topN = 10;

		if ( reportMode == REPORT_TOP_N && argc > 3 )
			topN = (unsigned int) atoi( CMD_ARGV( 3 ) );

		OutputToFile( CMD_ARGV( 1 ), reportMode, topN );
		return true;
	}

	OutputUsage( command );
	return true;
}
