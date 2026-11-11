#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "trigger_changevalue.h"
#include "server_entvars.h"
#include "server_support.h"
#include <float.h>
#include <limits.h>

CBaseEntity *ChangeValueHandleEntity( EHANDLE &handle )
{
	edict_t *entity = handle.Get();

	if ( !entity || entity->free )
		return NULL;

	return (CBaseEntity *) GET_PRIVATE( entity );
}

bool ChangeValueEntityValid( CBaseEntity *entity )
{
	return entity && entity->pev && entity->pev->pContainingEntity && !entity->pev->pContainingEntity->free && ENTINDEX( entity->pev->pContainingEntity ) != 0;
}

LINK_ENTITY_TO_CLASS( trigger_changevalue, CTriggerChangeValue );

TYPEDESCRIPTION CTriggerChangeValue::m_SaveData[] = {
	DEFINE_FIELD( CTriggerChangeValue, m_iszValueType, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerChangeValue, m_iszValueName, FIELD_STRING ),
	DEFINE_FIELD( CTriggerChangeValue, m_iszNewValue, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CTriggerChangeValue, CBaseDelay );

CTriggerChangeValue::CTriggerChangeValue()
    : m_hActivator()
{
}

void CTriggerChangeValue::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszNewValue" ) )
		m_iszNewValue = AllocStringLocal( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iszValueName" ) )
		m_iszValueName = AllocStringLocal( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iszValueType" ) )
		m_iszValueType = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_trigonometricBehaviour" ) )
		m_trigonometricBehaviour = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iAppendSpaces" ) )
		m_iAppendSpaces = atoi( pkvd->szValue );
	else
	{
		CBaseDelay::KeyValue( pkvd );
		return;
	}

	pkvd->fHandled = TRUE;
}

void *CTriggerChangeValue::GetKeyValue( const char *name, entvartype_s &type )
{
	if ( FStrEq( name, "m_iszNewValue" ) )
	{
		type = VAR_STRING;
		return &m_iszNewValue;
	}

	if ( FStrEq( name, "m_iszValueName" ) )
	{
		type = VAR_STRING;
		return &m_iszValueName;
	}

	if ( !stricmp( name, "m_iszValueType" ) )
	{
		type = VAR_INTEGER;
		return &m_iszValueType;
	}

	return CBaseDelay::GetKeyValue( name, type );
}

void CTriggerChangeValue::Spawn( void )
{
	if ( m_iAppendSpaces <= 0 )
		return;

	const char *value = STRING( m_iszNewValue );
	unsigned int length = strlen( value );

	if ( length > INT_MAX - 1 || (unsigned int) m_iAppendSpaces > INT_MAX - 1 - length )
		return;

	unsigned int size = length + m_iAppendSpaces + 1;
	char *buffer = (char *) malloc( size );

	if ( !buffer )
		return;

	memset( buffer, 0, size );
	UTIL_CopyString( buffer, value, size );
	memset( buffer + length, ' ', m_iAppendSpaces );
	buffer[length + m_iAppendSpaces] = 0;
	m_iszNewValue = AllocStringLocal( buffer );
	free( buffer );
}

void CTriggerChangeValue::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( caller == this )
	{
		ALERT( at_console, "Trigger_changevalue \"%s\": Tried to trigger itself. Aborting.\n", STRING( pev->targetname ) );
		return;
	}

	m_hActivator = EHANDLE();

	if ( activator && activator->pev && activator->pev->pContainingEntity && !activator->pev->pContainingEntity->free )
		m_hActivator.Set( activator->pev->pContainingEntity );

	if ( FStrEq( STRING( pev->target ), "!activator" ) && ChangeValueEntityValid( activator ) )
		ChangeValue( activator );
	else if ( FStrEq( STRING( pev->target ), "!caller" ) && ChangeValueEntityValid( caller ) )
		ChangeValue( caller );
	else
	{
		CBaseEntity *entity = NULL;

		while ( ( entity = UTIL_FindEntityByTargetname( entity, STRING( pev->target ) ) ) != NULL )
			ChangeValue( entity );
	}

	if ( pev->message )
		FireTargets( STRING( pev->message ), ChangeValueHandleEntity( m_hActivator ), this, USE_TOGGLE, 0 );
}

void CTriggerChangeValue::ChangeValue( CBaseEntity *entity )
{
	if ( !SpecialValue( entity, STRING( m_iszValueName ), STRING( m_iszNewValue ) ) && ChangeValueEntityValid( entity ) )
	{
		if ( m_iszValueType != VALUE_OP_REPLACE )
		{
			ALERT( at_console, "Trigger_changevalue \"%s\": The keyvalue by the name \"%s\" either does not exist or is an unsupported entity-specific keyvalue of class %s, and hence can only be replaced on your destination entity \"%s\".\n",
			       STRING( pev->targetname ), STRING( m_iszValueName ), STRING( entity->pev->classname ), STRING( entity->pev->targetname ) );
			return;
		}

		if ( !TryKeyValue( entity, STRING( m_iszValueName ), STRING( m_iszNewValue ) ) )
		{
			ALERT( at_console, "Trigger_changevalue \"%s\": No keyvalue by the name \"%s\" does exist on %s \"%s\".\n",
			       STRING( pev->targetname ), STRING( m_iszValueName ), STRING( entity->pev->classname ), STRING( entity->pev->targetname ) );
			return;
		}
	}

	if ( entity )
		entity->OnKeyValueUpdate( STRING( m_iszValueName ) );
}

bool CTriggerChangeValue::TryKeyValue( CBaseEntity *entity, const char *name, const char *value )
{
	KeyValueData *key = new KeyValueData;

	memset( key, 0, sizeof( *key ) );
	key->szKeyName = (char *) name;
	key->szValue = (char *) value;
	entity->KeyValue( key );

	bool handled = key->fHandled != FALSE;

	delete key;
	return handled;
}

float CTriggerChangeValue::MakeVecLength( Vector *value )
{
	for ( int index = 0; index < 3; index++ )
		if ( pev->spawnflags & ( 1 << index ) )
			( *value )[index] = 0;

	float squared = ( value->y * value->y + value->x * value->x ) + value->z * value->z;

	return (float) sqrt( (double) squared );
}

void CTriggerChangeValue::SpecialValueVector( Vector *value, Vector source )
{
	m_iVectorContext = 1;

	for ( int index = 0; index < 3; index++ )
		if ( !( pev->spawnflags & ( 1 << index ) ) )
			SpecialValueFloat( &( *value )[index], source[index] );

	if ( m_iVectorContext == 2 )
		ALERT( at_console, "Trigger_changevalue \"%s\": Tried using unknown or impossible operation %s on VECTOR keyvalue \"%s\".\n",
		       STRING( pev->targetname ), ChangeValueTypeStr( m_iszValueType ), STRING( m_iszValueName ) );

	m_iVectorContext = 0;
}

void CTriggerChangeValue::SpecialValueByteArray( int *value, const char *source, CBaseEntity *destination )
{
	const char *previous = STRING( *value );
	int length = strlen( source );
	int previousLength = strlen( previous );
	char buffer[VALUE_MAX_STRING + 1];

	if ( length > VALUE_MAX_STRING || ( m_iszValueType == VALUE_OP_CONCAT && previousLength > VALUE_MAX_STRING - length ) )
	{
		int requested = length > VALUE_MAX_STRING ? length : previousLength + length;

		ALERT( at_console, "Trigger_changevalue \"%s\": Too long string (%i > %i) used. Not performing operation.\n", STRING( pev->targetname ), requested, VALUE_MAX_STRING );
		return;
	}

	int position = 0;

	if ( m_iszValueType == VALUE_OP_CONCAT )
	{
		memcpy( buffer, previous, previousLength );
		position = previousLength;
	}
	else if ( m_iszValueType != VALUE_OP_REPLACE )
		ALERT( at_console, "Trigger_changevalue \"%s\": Tried using unknown or impossible operation %s on STRING keyvalue \"%s\".\n",
		       STRING( pev->targetname ), ChangeValueTypeStr( m_iszValueType ), STRING( m_iszValueName ) );

	memcpy( buffer + position, source, length );
	buffer[position + length] = 0;
	*value = AllocStringLocal( buffer );
}

bool CTriggerChangeValue::SpecialValue( CBaseEntity *entity, const char *name, const char *source )
{
	if ( !ChangeValueEntityValid( entity ) )
		return false;

	void *value = NULL;
	int index = -1;
	entvartype_s type = entity->GetEntvar( name, &value, index );

	switch ( type )
	{
		case VAR_INVALID:
			if ( FClassnameIs( pev, "trigger_changevalue" ) && m_iszValueType != VALUE_OP_REPLACE )
				ALERT( at_console, "Trigger_changevalue \"%s\": No keyvalue by the name \"%s\" does exist, or it is an unsupported entity-specific keyvalue. (If this is an unsupported entity-specific keyvalue, only \"Replace\"-operation is supported)\n",
				       STRING( pev->targetname ), name );
			return false;
		case VAR_STRING:
			SpecialValueByteArray( (int *) value, source, entity );
			break;
		case VAR_VECTOR:
			if ( UTIL_IsStringInt( source ) )
			{
				int integer = atoi( source );
				int components[3] = { (int) ( (float *) value )[0], (int) ( (float *) value )[1], (int) ( (float *) value )[2] };

				for ( int component = 0; component < 3; component++ )
					if ( !( pev->spawnflags & ( 1 << component ) ) )
						SpecialValueInt( components + component, integer );

				for ( int component = 0; component < 3; component++ )
					( (float *) value )[component] = (float) components[component];
			}
			else if ( UTIL_IsStringFloat( source ) )
			{
				float number = atof( source );
				Vector vector = *(Vector *) value;

				for ( int component = 0; component < 3; component++ )
					if ( !( pev->spawnflags & ( 1 << component ) ) )
						SpecialValueFloat( &vector[component], number );

				*(Vector *) value = vector;
			}
			else
			{
				Vector vector;

				UTIL_StringToVector( vector, source, ' ' );
				SpecialValueVector( (Vector *) value, vector );
			}
			break;
		case VAR_FLOAT:
			if ( UTIL_IsString3DVec( source ) )
			{
				Vector vector;

				UTIL_StringToVector( vector, source, ' ' );
				SpecialValueFloat( (float *) value, MakeVecLength( &vector ) );
			}
			else
				SpecialValueFloat( (float *) value, (float) atof( source ) );
			break;
		case VAR_INTEGER:
			if ( ( *source >= 'A' && *source <= 'Z' ) || *source == '~' )
			{
				bool valid = false;
				int integer = ValueForPEVFlag( source, index, valid );

				if ( valid )
					SpecialValueInt( (int *) value, integer );
				else if ( FClassnameIs( pev, "trigger_changevalue" ) )
					ALERT( at_console, "Trigger_changevalue \"%s\": Unknown value-identifier \"%s\" for target keyvalue \"%s\".\n",
					       STRING( pev->targetname ), source, name );
			}
			else if ( UTIL_IsString3DVec( source ) )
			{
				Vector vector;

				UTIL_StringToVector( vector, source, ' ' );
				SpecialValueInt( (int *) value, (int) ( (double) MakeVecLength( &vector ) + 0.5 ) );
			}
			else
				SpecialValueInt( (int *) value, atoi( source ) );
			break;
		case VAR_EDICT:
			if ( m_iszValueType == VALUE_OP_REPLACE )
			{
				CBaseEntity *target = FStrEq( source, "!activator" ) ? ChangeValueHandleEntity( m_hActivator ) : UTIL_FindEntityByTargetname( NULL, source );

				*(edict_t **) value = target && target->pev ? target->edict() : NULL;
			}
			else if ( FClassnameIs( pev, "trigger_changevalue" ) )
				ALERT( at_console, "Trigger_changevalue \"%s\": Keyvalue \"%s\" of type EDICT cannot be altered with edit type %s. Only REPLACE works.\n",
				       STRING( pev->targetname ), ChangeValueTypeStr( m_iszValueType ), name );
			break;
		default:
			break;
	}

	if ( index == 2 )
		UTIL_SetOrigin( entity->pev, entity->pev->origin );
	else if ( index >= 25 && index <= 29 )
		UTIL_SetSize( entity->pev, entity->pev->mins, entity->pev->maxs );

	return true;
}

void CTriggerChangeValue::SpecialValueFloat( float *value, float source )
{
	int operation = m_iszValueType;

	if ( operation >= VALUE_OP_AND && operation <= VALUE_OP_NOR )
	{
		if ( pev->spawnflags & SF_VALUE_INVERT_TARGET )
			*value = (float) ~(int) *value;

		if ( pev->spawnflags & SF_VALUE_INVERT_SOURCE )
			source = (float) ~(int) source;
	}
	else
	{
		if ( pev->spawnflags & SF_VALUE_INVERT_TARGET )
			*value = -*value;

		if ( pev->spawnflags & SF_VALUE_INVERT_SOURCE )
			source = -source;
	}

	switch ( operation )
	{
		case VALUE_OP_REPLACE:
			*value = source;
			break;
		case VALUE_OP_ADD:
			*value += source;
			break;
		case VALUE_OP_MULTIPLY:
			*value *= source;
			break;
		case VALUE_OP_SUBTRACT:
			*value -= source;
			break;
		case VALUE_OP_DIVIDE:
			if ( fabs( source ) == 0 )
				*value = ( *value < 0 ? -1.0 : 0.0 ) * FLT_MAX * ( source < 0 ? -1.0 : 0.0 );
			else
				*value /= source;
			break;
		case VALUE_OP_AND:
			*value = (float) ( (int) *value & (int) source );
			break;
		case VALUE_OP_OR:
			*value = (float) ( (int) *value | (int) source );
			break;
		case VALUE_OP_NAND:
			*value = (float) ~( (int) *value & (int) source );
			break;
		case VALUE_OP_NOR:
			*value = (float) ~( (int) *value | (int) source );
			break;
		case VALUE_OP_MODULO:
		{
			float divisor = fabs( source );

			if ( divisor != 0 )
			{
				if ( *value >= 0 )
					*value -= (float) (int) ( *value / divisor ) * divisor;
				else
					*value = -( -*value - (float) (int) ( -*value / divisor ) * divisor );
			}
			break;
		}
		case VALUE_OP_XOR:
			*value = (float) ( (int) *value ^ (int) source );
			break;
		case VALUE_OP_XNOR:
			*value = (float) ~( (int) *value ^ (int) source );
			break;
		case VALUE_OP_POWER:
			if ( *value > 0 )
				*value = (float) pow( (double) *value, (double) source );
			else if ( *value < 0 )
				*value = (float) pow( (double) *value, (double) (int) floor( (double) source + 0.5 ) );
			else
				*value = fabs( source ) == 0 ? 1.0f : 0.0f;
			break;
		default:
			if ( pev->spawnflags & SF_VALUE_INVERT_TARGET )
				operation = m_iszValueType;

			if ( operation >= VALUE_OP_SINE && operation <= VALUE_OP_ARCCOTANGENT )
			{
				float unit = m_trigonometricBehaviour == 1 ? 1.0f : 57.29578f;

				if ( operation == VALUE_OP_ARCSINE || operation == VALUE_OP_ARCCOSINE )
					source = (float) ( (float) fmod( fabs( source ), 1.0 ) * ( source >= 0 ? 1.0 : -1.0 ) );

				switch ( operation )
				{
					case VALUE_OP_SINE:
						*value = (float) sin( (double) ( source / unit ) );
						break;
					case VALUE_OP_COSINE:
						*value = (float) cos( (double) ( source / unit ) );
						break;
					case VALUE_OP_TANGENT:
						*value = (float) tan( (double) ( source / unit ) );
						break;
					case VALUE_OP_ARCSINE:
						*value = (float) asin( (double) source ) * unit;
						break;
					case VALUE_OP_ARCCOSINE:
						*value = (float) acos( (double) source ) * unit;
						break;
					case VALUE_OP_ARCTANGENT:
						*value = (float) atan( (double) source ) * unit;
						break;
					case VALUE_OP_COTANGENT:
						*value = 1.0f / (float) tan( (double) ( source / unit ) );
						break;
					case VALUE_OP_ARCCOTANGENT:
						*value = (float) atan( (double) ( 1.0f / source ) ) * unit;
						break;
				}
			}
			else
			{
				*value = source;

				if ( m_iVectorContext )
					m_iVectorContext = 2;
				else
					ALERT( at_console, "Trigger_changevalue \"%s\": Tried using unknown or impossible operation %s on FLOAT keyvalue \"%s\".\n",
					       STRING( pev->targetname ), ChangeValueTypeStr( m_iszValueType ), STRING( m_iszValueName ) );
			}
			break;
	}
}

void CTriggerChangeValue::SpecialValueInt( int *value, int source )
{
	int operation = m_iszValueType;

	if ( operation >= VALUE_OP_AND && operation <= VALUE_OP_NOR )
	{
		if ( pev->spawnflags & SF_VALUE_INVERT_TARGET )
			*value = ~*value;

		if ( pev->spawnflags & SF_VALUE_INVERT_SOURCE )
			source = ~source;
	}
	else
	{
		if ( pev->spawnflags & SF_VALUE_INVERT_TARGET )
			*value = (int) ( 0u - (unsigned int) *value );

		if ( operation != VALUE_OP_MODULO && ( pev->spawnflags & SF_VALUE_INVERT_SOURCE ) )
			source = (int) ( 0u - (unsigned int) source );
	}

	switch ( operation )
	{
		case VALUE_OP_REPLACE:
			*value = source;
			break;
		case VALUE_OP_ADD:
			*value = (int) ( (unsigned int) *value + (unsigned int) source );
			break;
		case VALUE_OP_MULTIPLY:
			*value = (int) ( (unsigned int) *value * (unsigned int) source );
			break;
		case VALUE_OP_SUBTRACT:
			*value = (int) ( (unsigned int) *value - (unsigned int) source );
			break;
		case VALUE_OP_DIVIDE:
			if ( !source )
				*value = *value < 0 ? INT_MIN : INT_MAX;
			else if ( *value == INT_MIN && source == -1 )
				// IDIV traps on this pair
				// Keep the wrapped integer
				*value = INT_MIN;
			else
				*value /= source;
			break;
		case VALUE_OP_AND:
			*value &= source;
			break;
		case VALUE_OP_OR:
			*value |= source;
			break;
		case VALUE_OP_NAND:
			*value = ~( *value & source );
			break;
		case VALUE_OP_NOR:
			*value = ~( *value | source );
			break;
		case VALUE_OP_MODULO:
			if ( source )
			{
				long long divisor = source < 0 ? -(long long) source : source;

				*value = (int) ( (long long) *value % divisor );
			}
			break;
		case VALUE_OP_XOR:
			*value ^= source;
			break;
		case VALUE_OP_XNOR:
			*value = ~( *value ^ source );
			break;
		case VALUE_OP_POWER:
			*value = *value ? (int) pow( (double) *value, (double) source ) : source == 0;
			break;
		default:
			*value = source;
			ALERT( at_console, "Trigger_changevalue \"%s\": Tried using unknown or impossible operation %s on INTEGER keyvalue \"%s\".\n",
			       STRING( pev->targetname ), ChangeValueTypeStr( m_iszValueType ), STRING( m_iszValueName ) );
			break;
	}
}
