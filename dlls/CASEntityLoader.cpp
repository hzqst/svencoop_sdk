#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "CASEntityLoader.h"
#include "CASDocumentation.h"
#include "CASLog.h"
#include "CASModule.h"
#include "CASModuleInfo.h"
#include "CKeyvalues.h"
#include "CKeyvaluesParser.h"
#include "server_filesystem.h"
#include "server_support.h"

CASEntityLoader::CASEntityLoader()
{
}

CASEntityLoader::~CASEntityLoader()
{
}

void CASEntityLoader::HandleKeyValues( const CString &className, edict_s *entity, CKvBlockNode &block ) const
{
	KeyValueData data;
	const CKvBlockNode::Children &children = block.GetChildren();

	data.szClassName = className.Data();

	CKvBlockNode::Children::const_iterator end = children.end();

	for ( CKvBlockNode::Children::const_iterator entry = children.begin(); entry != end; entry++ )
	{
		CKeyvalueNode *child = entry->get();

		if ( child->GetType() == KEYVALUE_NODE_VALUE )
		{
			CKeyvalue *value = (CKeyvalue *) child;

			data.szKeyName = child->GetKey().Data();
			data.szValue = value->GetValue().Data();
			data.fHandled = FALSE;
			DispatchKeyValue( entity, &data );
		}
		else
		{
			CASLog::GetInstance()->Warning( AS_LOG_DEFAULT,
			                                "CEntityLoader::HandleKeyValues: ignoring block '%s' in entity keyvalue block\n",
			                                child->GetKey().Data() );
		}
	}
}

bool CASEntityLoader::InstanceEntity( CKvBlockNode &block, const Vector &offset ) const
{
	CString className( block.FindFirstKeyvalue( "classname" ).Data() );

	if ( className.IsEmpty() )
	{
		CASLog::GetInstance()->Warning( AS_LOG_DEFAULT,
		                                "CEntityLoader::LoadFromFile: no classname found for entity; skipping!\n" );
		return false;
	}

	edict_t *entity = CREATE_NAMED_ENTITY( MAKE_STRING( className.Data() ) );

	if ( !entity )
	{
		CASLog::GetInstance()->Warning( AS_LOG_DEFAULT,
		                                "CEntityLoader::LoadFromFile: no entity with name '%s'; skipping!\n",
		                                className.Data() );
		return false;
	}

	HandleKeyValues( className, entity, block );

	CBaseEntity *actor = entity->free ? NULL : (CBaseEntity *) entity->pvPrivateData;

	if ( !actor )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT,
		                              "CEntityLoader::LoadFromFile: an error occured; entity with classname '%s' could not be created\n",
		                              className.Data() );
		REMOVE_ENTITY( entity );
		return false;
	}

	entity->v.origin = entity->v.origin + offset;

	if ( DispatchSpawn( entity ) == -1 )
		REMOVE_ENTITY( entity );
	else
		actor->Activate();

	return true;
}

bool CASEntityLoader::LoadFromFile( const CString &fileName, const Vector &offset )
{
	if ( fileName.IsEmpty() )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "CEntityLoader::LoadFromFile: filename is empty!\n" );
		return false;
	}

	CASModule *module = CASServerManager::GetInstance()->m_pActiveModule;

	if ( !module )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "CEntityLoader::LoadFromFile: no active module!\n" );
		return false;
	}

	char path[512 + 4]; // path +ext
	int result = UTIL_FormatString( path, sizeof( path ), "%s/%s", module->m_pModuleInfo->m_pszDirectory, fileName.Data() );

	if ( result == -1 )
		return false;

	FileHandle_t file = g_pFileSystem->Open( path, "r", "GAME" );
	char *buffer = NULL;
	unsigned int bytesRead = 0;
	bool read = false;

	if ( file )
	{
		unsigned int position = g_pFileSystem->Tell( file );

		g_pFileSystem->Seek( file, 0, FILESYSTEM_SEEK_TAIL );

		unsigned int size = g_pFileSystem->Tell( file );

		g_pFileSystem->Seek( file, position, FILESYSTEM_SEEK_HEAD );
		buffer = new char[size + 1];
		bytesRead = g_pFileSystem->Read( buffer, size, file );
		read = bytesRead != 0 || size == 0;
		g_pFileSystem->Close( file );
	}

	if ( !read )
	{
		delete[] buffer;
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT,
		                              "CEntityLoader::LoadFromFile: file '%s' does not exist!\n", fileName.Data() );
		return false;
	}

	CMemory<unsigned int> input( buffer, bytesRead, true );
	CKeyvaluesParser parser( input, CKeyvaluesParserSettings( true, true, true, false ) );

	if ( parser.Parse() == CBaseKeyvaluesParser::PARSE_SUCCESS )
	{
		std::shared_ptr<CKeyvalues> keyvalues = parser.GetKeyvalues();
		std::shared_ptr<CKvBlockNode> root = keyvalues->GetRoot();
		const CKvBlockNode::Children &children = root->GetChildren();

		for ( CKvBlockNode::Children::const_iterator entry = children.begin(); entry != children.end(); entry++ )
		{
			CKeyvalueNode *child = entry->get();

			if ( child->GetType() == KEYVALUE_NODE_BLOCK )
				InstanceEntity( *(CKvBlockNode *) child, offset );
			else
				CASLog::GetInstance()->Warning( AS_LOG_DEFAULT, "CEntityLoader::LoadFromFile: keyvalue found in global scope\n" );
		}
	}

	return true;
}

void RegisterSCScriptEntityLoader( CASDocumentation *documentation )
{
	asDWORD previousAccess = documentation->SetDefaultAccessMask( AS_ACCESS_MAP );

	documentation->RegisterObjectType(
	    "Entity loader.\nLoads entity keyvalue data and instances the entities.\nFile format:\n\"Entity\"{\t\"classname\" \"info_player_deathmatch\"\t<additional keyvalues>}...",
	    "CEntityLoader", 0, asOBJ_REF | asOBJ_NOCOUNT );
	documentation->RegisterObjectMethod( "Loads entities from a file.", "CEntityLoader",
	                                     "bool LoadFromFile(const string& in szFileName, const Vector& in vecOffset = g_vecZero)",
	                                     asMETHOD( CASEntityLoader, LoadFromFile ), asCALL_THISCALL );
	documentation->RegisterGlobalProperty( "Entity loader global instance", "CEntityLoader g_EntityLoader", CASEntityLoader::GetInstance() );
	documentation->SetDefaultAccessMask( previousAccess );
}
