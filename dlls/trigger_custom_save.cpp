#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "server_support.h"
#include "trigger_custom_save.h"

#include <fstream>

LINK_ENTITY_TO_CLASS( trigger_save, CTriggerCustomSave );
LINK_ENTITY_TO_CLASS( trigger_load, CTriggerCustomSave );

CTriggerCustomSave::~CTriggerCustomSave()
{
}

void CTriggerCustomSave::Spawn( void )
{
}

void CTriggerCustomSave::OnCreate( void )
{
	CBaseDelay::OnCreate();
	m_pszSaveName = new char[512];
	m_pszSaveName[0] = 0;
}

void CTriggerCustomSave::OnDestroy( void )
{
	if ( m_pszSaveName )
		delete[] m_pszSaveName;

	CBaseDelay::OnDestroy();
}

void CTriggerCustomSave::KeyValue( KeyValueData *data )
{
	if ( FStrEq( data->szKeyName, "m_iszTrigger" ) )
	{
		m_iszTrigger = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FClassnameIs( pev, "trigger_load" ) && FStrEq( data->szKeyName, "m_iszLevelName" ) )
	{
		m_iszLevelName = ALLOC_STRING( data->szValue );
		m_pszSaveName[0] = 0;
		data->fHandled = TRUE;
	}
	else
	{
		CBaseDelay::KeyValue( data );
	}
}

void CTriggerCustomSave::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( FClassnameIs( pev, "trigger_save" ) )
		SaveData();
	else if ( FClassnameIs( pev, "trigger_load" ) )
		LoadData();

	if ( m_iszTrigger )
		FireTargets( STRING( m_iszTrigger ), activator, this, USE_ON, 0 );
}

char *CTriggerCustomSave::GetSaveName( void )
{
	if ( !m_pszSaveName[0] )
	{
		char gameDirectory[260];

		GET_GAME_DIR( gameDirectory );

		const char *level = STRING( gpGlobals->mapname );

		if ( FClassnameIs( pev, "trigger_load" ) && m_iszLevelName && *STRING( m_iszLevelName ) )
			level = STRING( m_iszLevelName );

		UTIL_FormatString( m_pszSaveName, 512, "%s/maps/%s.save", gameDirectory, level );
	}

	return m_pszSaveName;
}

void CTriggerCustomSave::SaveData( void )
{
	if ( !pev->netname || !pev->target )
		return;

	const char *label = STRING( pev->netname );

	if ( !label || !*label )
		return;

	CBaseEntity *entity = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );

	if ( !entity )
		return;

	void *data = NULL;
	int unusedIndex = -1;
	entvartype_s type = entity->GetEntvar( STRING( pev->message ), &data, unusedIndex );
	char *text = NULL;

	switch ( type )
	{
		case VAR_STRING:
		{
			const char *string = STRING( *(string_t *) data );
			size_t length = strlen( string ) + 1;

			text = new char[length];
			UTIL_FormatString( text, length, "%s", string );
			break;
		}
		case VAR_VECTOR:
		{
			Vector *vector = (Vector *) data;

			text = new char[192];
			UTIL_FormatString( text, 192, "%f %f %f", vector->x, vector->y, vector->z );
			break;
		}
		case VAR_FLOAT:
			text = new char[64];
			UTIL_FormatString( text, 64, "%f", *(float *) data );
			break;
		case VAR_INTEGER:
			text = new char[64];
			UTIL_FormatString( text, 64, "%i", *(int *) data );
			break;
		case VAR_EDICT:
		{
			edict_t *edict = (edict_t *) data;

			if ( !edict || edict->free || !ENTINDEX( edict ) )
				return;

			text = new char[512];
			UTIL_FormatString( text, 512, "%s", STRING( edict->v.targetname ) );
			break;
		}
		default:
			ALERT( at_console, "Trigger_save \"%s\": Cannot read the non-existent or unsupported entity-specific keyvalue \"%s\" to save it.\n",
			       STRING( pev->targetname ), STRING( pev->message ) );
			return;
	}

	char *contents = NULL;
	std::ifstream input( GetSaveName(), std::ios::in );

	if ( input.is_open() )
	{
		input.seekg( 0, std::ios::end );

		int length = (int) input.tellg();

		contents = new char[length + 1];
		memset( contents, 0, length + 1 );
		input.seekg( 0, std::ios::beg );
		input.read( contents, length );
		input.close();
	}

	if ( contents && *contents )
	{
		char *updated = new char[strlen( contents ) + 2048];
		char *suffix = NULL;
		size_t offset = 0;

		while ( contents[offset] )
		{
			if ( !strncmp( contents + offset, label, strlen( label ) ) )
			{
				size_t end = offset;

				while ( contents[end] && contents[end] != '\n' )
					end++;

				if ( contents[end] == '\n' )
					end++;

				suffix = contents + end;
				break;
			}

			while ( contents[offset] && contents[offset] != '\n' )
				offset++;

			if ( contents[offset] )
				offset++;
		}

		contents[offset] = 0;

		size_t prefixLength = strlen( contents );

		// An empty prefix would read before the buffer
		// Keep the text operation bounded
		if ( prefixLength && contents[prefixLength - 1] == '\n' )
			contents[prefixLength - 1] = 0;

		if ( !suffix )
			suffix = contents + offset;

		UTIL_FormatString( updated, strlen( contents ) + 2048, "%s\n%s:%s\n%s", contents, label, text, suffix );
		delete[] contents;
		contents = updated;
	}
	else
	{
		delete[] contents;
		contents = new char[2048];
		UTIL_FormatString( contents, 2048, "%s:%s\n", label, text );
	}

	std::ofstream output( GetSaveName(), std::ios::out );

	if ( output.is_open() )
	{
		output << contents;
		output.close();
	}
	else
	{
		ALERT( at_error, "Failed writing to file '%s'! Make sure that the file is not read-only.\n", GetSaveName() );
	}

	delete[] contents;
	delete[] text;
}

void CTriggerCustomSave::LoadData( void )
{
	if ( !pev->netname || !pev->target || !pev->message )
		return;

	const char *label = STRING( pev->netname );

	if ( !label || !*label )
		return;

	CBaseEntity *entity = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );

	if ( !entity )
		return;

	void *data = NULL;
	int unusedIndex = -1;
	entvartype_s type = entity->GetEntvar( STRING( pev->message ), &data, unusedIndex );

	if ( type == VAR_INVALID || type == VAR_CUSTOM )
		return;

	std::ifstream input( GetSaveName(), std::ios::in );

	if ( !input.is_open() )
		return;

	input.seekg( 0, std::ios::end );

	int length = (int) input.tellg();
	char *contents = new char[length + 1];

	memset( contents, 0, length + 1 );
	input.seekg( 0, std::ios::beg );
	input.read( contents, length );
	input.close();

	if ( !contents )
		return;

	char *text = NULL;
	size_t offset = 0;

	while ( contents[offset] )
	{
		if ( !strncmp( contents + offset, label, strlen( label ) ) )
		{
			while ( contents[offset] && contents[offset] != ':' && contents[offset] != '\n' )
				offset++;

			if ( contents[offset] == ':' )
			{
				offset++;

				size_t start = offset;

				while ( contents[offset] && contents[offset] != '\n' )
					offset++;

				if ( contents[offset] == '\n' && offset != start )
				{
					contents[offset] = 0;

					size_t textLength = offset - start + 8;

					text = new char[textLength];
					UTIL_FormatString( text, textLength, "%s", contents + start );
				}
			}

			break;
		}

		while ( contents[offset] && contents[offset] != '\n' )
			offset++;

		if ( contents[offset] )
			offset++;
	}

	delete[] contents;

	if ( !text )
	{
		ALERT( at_console, "Trigger_load \"%s\": Could not find label \"%s\" to load from.\n",
		       STRING( pev->targetname ), label );
		return;
	}

	switch ( type )
	{
		case VAR_STRING:
			*(string_t *) data = ALLOC_STRING( text );
			break;
		case VAR_VECTOR:
			UTIL_StringToVectorND( (float *) data, 3, text, ' ' );
			break;
		case VAR_FLOAT:
			*(float *) data = (float) atof( text );
			break;
		case VAR_INTEGER:
			*(int *) data = atoi( text );
			break;
		case VAR_EDICT:
		{
			edict_t *edict = (edict_t *) data;

			// The returned storage is used directly as an edict, without dereferencing it
			if ( edict && !edict->free && ENTINDEX( edict ) )
				edict->v.targetname = ALLOC_STRING( text );

			break;
		}
	}

	delete[] text;
}
