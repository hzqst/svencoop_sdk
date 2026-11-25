#include "extdll.h"
#include "util.h"
#include "CASWaypointManager.h"
#include "CASDocumentation.h"
#include "CASFunction.h"
#include "CASLog.h"
#include "CASServerManager.h"
#include "server_support.h"
#include <string.h>

CASWaypointManager::CASWaypointManager()
	: m_pFirst( NULL )
{
}

CASWaypointManager::~CASWaypointManager()
{
}

bool CASWaypointManager::IsValidName( const char *name )
{
	return name != NULL && name[0] != 0;
}

PathConditionPair_t *CASWaypointManager::Find( const char *name, PathConditionPair_t **found ) const
{
	PathConditionPair_t *previous = NULL;

	if ( IsValidName( name ) && m_pFirst )
	{
		for ( PathConditionPair_t *pair = m_pFirst; pair; pair = pair->m_pNext )
		{
			if ( strcmp( pair->m_szName, name ) == 0 )
			{
				break;
			}
			previous = pair;
		}
	}

	if ( found )
		*found = previous;

	return NULL;
}

PathConditionPair_t *CASWaypointManager::GetConditions( const char *name ) const
{
	return Find( name, NULL );
}

void CASWaypointManager::InsertPair( PathConditionPair_t *pair )
{
	if ( !pair )
		return;

	pair->m_pNext = m_pFirst;
	m_pFirst = pair;
}

bool CASWaypointManager::RegisterConditions( const CString &name, asIScriptFunction *startFunction, asIScriptFunction *stopFunction )
{
	const char *nameData = name.Data();
	const unsigned int nameLength = name.Length();

	if ( !IsValidName( nameData ) )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT,
			"CWaypointManager::RegisterConditions: pair name is invalid!\n" );
	}
	else if ( !startFunction || !stopFunction )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT,
			"CWaypointManager::RegisterConditions: both functions must be valid!\n" );
	}
	else if ( Find( nameData, NULL ) )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT,
			"CWaypointManager::RegisterConditions: pair name \"%s\" is already in use!\n", nameData );
	}
	else if ( nameLength > 0x80 )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT,
			"CWaypointManager::RegisterConditions: pair name \"%s\" length is too long! (max length: %u)\n",
			nameData, nameLength );
	}
	else
	{
		PathConditionPair_t *pair = new PathConditionPair_t;

		pair->m_pNext = NULL;
		UTIL_CopyString( pair->m_szName, nameData, 0x81 );
		pair->m_pStartFunction = new CASFunction( startFunction, CASServerManager::GetInstance()->m_pActiveModule );
		pair->m_pStopFunction = new CASFunction( stopFunction, CASServerManager::GetInstance()->m_pActiveModule );

		InsertPair( pair );
	}

	if ( startFunction )
		startFunction->Release();

	if ( stopFunction )
		stopFunction->Release();

	return false;
}

void CASWaypointManager::FreePair( PathConditionPair_t *pair )
{
	if ( !pair )
		return;

	pair->m_pStartFunction->Release();
	pair->m_pStopFunction->Release();

	delete pair;
}

void CASWaypointManager::RemovePair( PathConditionPair_t *pair, PathConditionPair_t *previous )
{
	if ( !pair )
		return;

	if ( previous )
		previous->m_pNext = pair->m_pNext;

	FreePair( pair );
}

void CASWaypointManager::Clear( void )
{
	PathConditionPair_t *pair = m_pFirst;

	while ( pair )
	{
		PathConditionPair_t *next = pair->m_pNext;

		FreePair( pair );

		pair = next;
	}

	m_pFirst = NULL;
}

void CASWaypointManager::Shutdown( void )
{
	Clear();
}

void RegisterSCScriptWaypointManager( CASDocumentation *documentation )
{
	unsigned int defaultAccessMask = documentation->SetDefaultAccessMask( 1 );

	documentation->RegisterFuncDef( "Waypoint start function",
		"bool WaypointStartFunction(CPathCondition@ pCondition, CBaseMonster@ pMonster)" );
	documentation->RegisterFuncDef( "Waypoint stop function",
		"bool WaypointStopFunction(CPathCondition@ pCondition, CBaseMonster@ pMonster)" );

	documentation->RegisterObjectType( "Waypoint manager class", "CWaypointManager", 0, 0x40001 );

	documentation->RegisterObjectMethod( "Registers a pair of functions with a name", "CWaypointManager",
		"bool RegisterConditions(const string& in szName, WaypointStartFunction@ pStartFunc, WaypointStopFunction@ pStopFunc)",
		asMETHODPR( CASWaypointManager, RegisterConditions, (const CString &, asIScriptFunction *, asIScriptFunction *), bool ), asCALL_THISCALL );

	documentation->RegisterGlobalProperty( "Waypoint manager class instance", "CWaypointManager g_WaypointManager",
		CASWaypointManager::GetInstance() );

	documentation->SetDefaultAccessMask( defaultAccessMask );
}
