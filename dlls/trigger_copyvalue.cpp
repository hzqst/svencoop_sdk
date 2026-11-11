#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "trigger_copyvalue.h"
#include "trigger_changevalue.h"
#include "server_entvars.h"
#include "server_support.h"
#include "CCustomKeyvalues.h"
#include <float.h>
#include <limits.h>

#define SF_COPYVALUE_CONSTANT 8
#define SF_COPYVALUE_START_ON 16
#define SF_COPYVALUE_MULTIPLE_DESTINATIONS 128

CBaseEntity *CopyValueHandleEntity( EHANDLE &handle )
{
	edict_t *entity = handle.Get();

	if ( !entity || entity->free )
		return NULL;

	return (CBaseEntity *) GET_PRIVATE( entity );
}

bool CopyValueEntityValid( CBaseEntity *entity )
{
	return entity && entity->pev && entity->pev->pContainingEntity && !entity->pev->pContainingEntity->free && ENTINDEX( entity->pev->pContainingEntity ) != 0;
}

void CopyValueSetHandle( EHANDLE &handle, CBaseEntity *entity )
{
	handle = EHANDLE();

	if ( entity && entity->pev && entity->pev->pContainingEntity && !entity->pev->pContainingEntity->free )
		handle.Set( entity->pev->pContainingEntity );
}

LINK_ENTITY_TO_CLASS( trigger_copyvalue, CTriggerCopyValue );

TYPEDESCRIPTION CTriggerCopyValue::m_SaveData[] = {
	DEFINE_FIELD( CTriggerCopyValue, m_iszDstValueName, FIELD_STRING ),
	DEFINE_FIELD( CTriggerCopyValue, m_iszSrcValueName, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CTriggerCopyValue, CBaseDelay );

CTriggerCopyValue::CTriggerCopyValue()
    : m_hDestination(), m_hSource(), m_hActivator(), m_hCaller()
{
}

void CTriggerCopyValue::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszDstValueName" ) )
		m_iszDstValueName = AllocStringLocal( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iszSrcValueName" ) )
		m_iszSrcValueName = AllocStringLocal( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iszValueType" ) )
		m_iszValueType = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_trigonometricBehaviour" ) )
		m_trigonometricBehaviour = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iAppendSpaces" ) )
		m_iAppendSpaces = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iFloatConversion" ) )
		m_iFloatConversion = atoi( pkvd->szValue );
	else
	{
		CBaseDelay::KeyValue( pkvd );
		return;
	}

	pkvd->fHandled = TRUE;
}

void *CTriggerCopyValue::GetKeyValue( const char *name, entvartype_s &type )
{
	if ( FStrEq( name, "m_iszDstValueName" ) )
	{
		type = VAR_STRING;
		return &m_iszDstValueName;
	}

	if ( FStrEq( name, "m_iszSrcValueName" ) )
	{
		type = VAR_STRING;
		return &m_iszSrcValueName;
	}

	if ( !stricmp( name, "m_iszValueType" ) )
	{
		type = VAR_INTEGER;
		return &m_iszValueType;
	}

	return CBaseDelay::GetKeyValue( name, type );
}

void CTriggerCopyValue::Reset( void )
{
	m_bActive = false;
	m_bUseKeyValue = false;
	m_hDestination = EHANDLE();
	m_hSource = EHANDLE();
	m_DestinationType = VAR_INVALID;
	m_SourceType = VAR_INVALID;
	m_hActivator = EHANDLE();
	m_hCaller = EHANDLE();
	m_pDstValue = NULL;
	m_pSrcValue = NULL;
	m_pNextTarget = NULL;
}

void CTriggerCopyValue::Spawn( void )
{
	Reset();

	if ( pev->spawnflags & SF_COPYVALUE_START_ON )
	{
		SetThink( &CTriggerCopyValue::AutoStart );
		pev->nextthink = gpGlobals->time;
	}
}

void CTriggerCopyValue::AutoStart( void )
{
	SetThink( NULL );
	Use( NULL, NULL, USE_ON, 0 );
}

void CTriggerCopyValue::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( caller == this )
	{
		ALERT( at_console, "Trigger_copyvalue \"%s\": Tried to trigger itself. Aborting.\n", STRING( pev->targetname ) );
		Reset();
		SetThink( NULL );
		return;
	}

	m_hDestination = EHANDLE();
	m_hSource = EHANDLE();
	CopyValueSetHandle( m_hActivator, activator );
	CopyValueSetHandle( m_hCaller, caller );

	if ( pev->spawnflags & SF_COPYVALUE_CONSTANT )
	{
		if ( ShouldToggle( useType, m_bActive ) )
			m_bActive = !m_bActive;
	}
	else
		m_bActive = useType != USE_OFF;

	if ( !m_bActive )
	{
		Reset();
		SetThink( NULL );
		return;
	}

	CopyValue();
}

void CTriggerCopyValue::GetEntityPointers( CBaseEntity *activator, CBaseEntity *caller )
{
	if ( !CopyValueHandleEntity( m_hDestination ) )
	{
		if ( FStrEq( STRING( pev->target ), "!activator" ) && CopyValueEntityValid( activator ) )
			CopyValueSetHandle( m_hDestination, activator );
		else if ( FStrEq( STRING( pev->target ), "!caller" ) && CopyValueEntityValid( caller ) )
			CopyValueSetHandle( m_hDestination, caller );
		else
			CopyValueSetHandle( m_hDestination, UTIL_FindEntityByTargetname( m_pNextTarget, STRING( pev->target ) ) );
	}

	if ( !CopyValueHandleEntity( m_hSource ) )
	{
		if ( FStrEq( STRING( pev->netname ), "!activator" ) && CopyValueEntityValid( activator ) )
			CopyValueSetHandle( m_hSource, activator );
		else if ( FStrEq( STRING( pev->netname ), "!caller" ) && CopyValueEntityValid( caller ) )
			CopyValueSetHandle( m_hSource, caller );
		else
			CopyValueSetHandle( m_hSource, UTIL_FindEntityByTargetname( NULL, STRING( pev->netname ) ) );
	}

	CBaseEntity *destination = CopyValueHandleEntity( m_hDestination );
	CBaseEntity *source = CopyValueHandleEntity( m_hSource );

	if ( !destination || !source )
		return;

	int unusedIndex;

	m_DestinationType = destination->GetEntvar( STRING( m_iszDstValueName ), &m_pDstValue, m_iDstValueIndex );
	m_SourceType = source->GetEntvar( STRING( m_iszSrcValueName ), &m_pSrcValue, unusedIndex );
	m_bUseKeyValue = false;

	if ( m_DestinationType == VAR_INVALID && ( m_iszValueType != VALUE_OP_REPLACE || CCustomKeyvalues::IsCustomKeyvalue( STRING( m_iszDstValueName ) ) ) )
	{
		ALERT( at_console, "Trigger_copyvalue \"%s\": Destination keyvalue \"%s\" is a non-existent, impossible custom or an unsupported entity-specific keyvalue. If it is an unsupported entity-specific keyvalue, only \"Replace\"-operation is possible.\n",
		       STRING( pev->targetname ), STRING( m_iszDstValueName ) );
		Reset();
		SetThink( NULL );
		return;
	}

	if ( m_SourceType == VAR_INVALID && m_DestinationType != VAR_EDICT )
	{
		ALERT( at_console, "Trigger_copyvalue \"%s\": Source keyvalue \"%s\" is a non-existent or an unsupported entity-specific keyvalue. Unsupported entity-specific keyvalues cannot be read, only written.\n",
		       STRING( pev->targetname ), STRING( m_iszSrcValueName ) );
		Reset();
		SetThink( NULL );
	}
}

bool CTriggerCopyValue::TryKeyValue( char *value )
{
	CBaseEntity *destination = CopyValueHandleEntity( m_hDestination );

	if ( !destination )
		return false;

	if ( m_iszValueType != VALUE_OP_REPLACE )
	{
		ALERT( at_console, "Trigger_copyvalue \"%s\": The keyvalue by the name \"%s\" either does not exist or is an unsupported entity-specific keyvalue of class %s, and hence can only be replaced on your destination entity \"%s\".\n",
		       STRING( pev->targetname ), STRING( m_iszDstValueName ), STRING( destination->pev->classname ), STRING( destination->pev->targetname ) );
		return false;
	}

	KeyValueData *key = new KeyValueData;

	memset( key, 0, sizeof( *key ) );
	key->szKeyName = (char *) STRING( m_iszDstValueName );
	key->szValue = value;
	destination->KeyValue( key );

	bool handled = key->fHandled != FALSE;

	delete key;

	if ( !handled )
	{
		destination = CopyValueHandleEntity( m_hDestination );

		if ( destination )
			ALERT( at_console, "Trigger_copyvalue \"%s\": No keyvalue by the name \"%s\" does exist on %s \"%s\".\n",
			       STRING( pev->targetname ), STRING( m_iszDstValueName ), STRING( destination->pev->classname ), STRING( destination->pev->targetname ) );

		Reset();
		SetThink( NULL );
	}

	return handled;
}

int CTriggerCopyValue::WriteFloatToString( char *buffer, int size, float value, int conversion )
{
	if ( (unsigned int) conversion - 1 <= 14 )
		conversion -= ( conversion - 1 ) % 3;

	switch ( conversion )
	{
		case 1:
			return UTIL_FormatString( buffer, size, "%.5f", value );
		case 4:
			return UTIL_FormatString( buffer, size, "%.4f", value );
		case 7:
			return UTIL_FormatString( buffer, size, "%.3f", value );
		case 10:
			return UTIL_FormatString( buffer, size, "%.2f", value );
		case 13:
			return UTIL_FormatString( buffer, size, "%.1f", value );
		case 16:
			return UTIL_FormatString( buffer, size, "%i", (int) ( value + 0.5 ) );
		case 17:
		{
			float rounded = ceil( value );

			return UTIL_FormatString( buffer, size, "%i", (int) ( rounded + 0.5 ) );
		}
		case 18:
			return UTIL_FormatString( buffer, size, "%i", (int) value );
		default:
			return UTIL_FormatString( buffer, size, "%f", value );
	}
}

float CTriggerCopyValue::MakeVecLength( Vector *value )
{
	int components = 3;

	for ( int index = 0; index < 3; index++ )
	{
		if ( pev->spawnflags & ( 1 << index ) )
		{
			( *value )[index] = 0;
			components--;
		}
	}

	if ( components == 1 )
		return ( value->y + value->x ) + value->z;

	float squared = ( value->y * value->y + value->x * value->x ) + value->z * value->z;

	return (float) sqrt( (double) squared );
}

void CTriggerCopyValue::SpecialValueByteArray( int *value, const char *source, CBaseEntity *destination )
{
	const char *previous = STRING( *value );
	int length = strlen( source );
	int previousLength = strlen( previous );
	char buffer[VALUE_MAX_STRING + 1];

	if ( m_iAppendSpaces < 0 || m_iAppendSpaces > VALUE_MAX_STRING - length || ( m_iszValueType == VALUE_OP_CONCAT && previousLength > VALUE_MAX_STRING - length - m_iAppendSpaces ) )
	{
		int requested = (int) ( (unsigned int) length + (unsigned int) m_iAppendSpaces );

		if ( m_iAppendSpaces >= 0 && m_iAppendSpaces <= VALUE_MAX_STRING - length && m_iszValueType == VALUE_OP_CONCAT )
			requested += previousLength;

		ALERT( at_console, "Trigger_copyvalue \"%s\": Too long string (%i > %i) used. Not performing operation.\n", STRING( pev->targetname ), requested, VALUE_MAX_STRING );
		return;
	}

	int position = 0;

	if ( m_iszValueType == VALUE_OP_CONCAT )
	{
		memcpy( buffer, previous, previousLength );
		position = previousLength;
	}
	else if ( m_iszValueType != VALUE_OP_REPLACE )
		ALERT( at_console, "Trigger_copyvalue \"%s\": Tried using unknown or impossible operation %s on STRING keyvalue \"%s\".\n",
		       STRING( pev->targetname ), ChangeValueTypeStr( m_iszValueType ), STRING( m_iszSrcValueName ) );

	memcpy( buffer + position, source, length );
	position += length;
	memset( buffer + position, ' ', m_iAppendSpaces );
	buffer[position + m_iAppendSpaces] = 0;
	*value = AllocStringLocal( buffer );
}

void CTriggerCopyValue::SpecialValueFloat( float *value, float source )
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
				ALERT( at_console, "Trigger_copyvalue \"%s\": Tried using unknown or impossible operation %s on FLOAT keyvalue \"%s\".\n",
				       STRING( pev->targetname ), ChangeValueTypeStr( m_iszValueType ), STRING( m_iszSrcValueName ) );
			}
			break;
	}
}

void CTriggerCopyValue::SpecialValueInt( int *value, int source )
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
			ALERT( at_console, "Trigger_copyvalue \"%s\": Tried using unknown or impossible operation %s on INTEGER keyvalue \"%s\".\n",
			       STRING( pev->targetname ), ChangeValueTypeStr( m_iszValueType ), STRING( m_iszSrcValueName ) );
			break;
	}
}

void CTriggerCopyValue::CopyValue( void )
{
	static string_t emptyValue;

	m_pNextTarget = NULL;

	for ( ;; )
	{
		if ( !CopyValueHandleEntity( m_hSource ) || !CopyValueHandleEntity( m_hDestination ) || ( pev->spawnflags & SF_COPYVALUE_MULTIPLE_DESTINATIONS ) )
		{
			CBaseEntity *previous = m_pNextTarget;

			m_hSource = EHANDLE();
			m_hDestination = EHANDLE();
			GetEntityPointers( CopyValueHandleEntity( m_hActivator ), CopyValueHandleEntity( m_hCaller ) );
			m_pNextTarget = CopyValueHandleEntity( m_hDestination );

			if ( !CopyValueHandleEntity( m_hSource ) || !CopyValueHandleEntity( m_hDestination ) )
				break;

			// Special target names keep returning the same entity
			if ( ( pev->spawnflags & SF_COPYVALUE_MULTIPLE_DESTINATIONS ) && previous && m_pNextTarget == previous )
				break;
		}

		if ( m_DestinationType == VAR_INVALID )
		{
			m_DestinationType = VAR_STRING;
			m_pDstValue = &emptyValue;
			m_bUseKeyValue = true;
		}

		CBaseEntity *destination = CopyValueHandleEntity( m_hDestination );
		char buffer[100];
		bool stringResult = false;

		switch ( m_SourceType )
		{
			case VAR_VECTOR:
				if ( m_DestinationType == VAR_FLOAT || m_DestinationType == VAR_INTEGER )
				{
					Vector value = *(Vector *) m_pSrcValue;
					float length = MakeVecLength( &value );

					if ( m_DestinationType == VAR_FLOAT )
						SpecialValueFloat( (float *) m_pDstValue, length );
					else
						SpecialValueInt( (int *) m_pDstValue, (int) ( (double) length + 0.5 ) );
				}
				else if ( m_DestinationType == VAR_VECTOR )
				{
					Vector value = *(Vector *) m_pSrcValue;

					if ( m_iszValueType == VALUE_OP_DIR_TO_ANGLES )
					{
						Vector angles;

						VEC_TO_ANGLES( value, angles );
						// Keep the fixed-size result automatic

						for ( int index = 0; index < 3; index++ )
							if ( !( pev->spawnflags & ( 1 << index ) ) )
								( (float *) m_pDstValue )[index] = angles[index];
					}
					else if ( m_iszValueType == VALUE_OP_ANGLES_TO_DIR )
					{
						UTIL_MakeVectors( value );

						for ( int index = 0; index < 3; index++ )
							if ( !( pev->spawnflags & ( 1 << index ) ) )
								( (float *) m_pDstValue )[index] = gpGlobals->v_forward[index];
					}
					else
					{
						for ( int index = 0; index < 3; index++ )
							if ( !( pev->spawnflags & ( 1 << index ) ) )
								SpecialValueFloat( (float *) m_pDstValue + index, ( (float *) m_pSrcValue )[index] );
					}
				}
				else if ( m_DestinationType == VAR_STRING )
				{
					char components[3][32];

					for ( int index = 0; index < 3; index++ )
						WriteFloatToString( components[index], sizeof( components[index] ),
						                    ( pev->spawnflags & ( 1 << index ) ) ? 0 : ( (float *) m_pSrcValue )[index], m_iFloatConversion );

					UTIL_FormatString( buffer, 96, "%s %s %s", components[0], components[1], components[2] );
					stringResult = true;
				}
				break;
			case VAR_FLOAT:
			case VAR_INTEGER:
			{
				float value = m_SourceType == VAR_FLOAT ? *(float *) m_pSrcValue : (float) *(int *) m_pSrcValue;

				if ( m_DestinationType == VAR_VECTOR )
				{
					for ( int index = 0; index < 3; index++ )
						if ( !( pev->spawnflags & ( 1 << index ) ) )
							SpecialValueFloat( (float *) m_pDstValue + index,
							                   m_SourceType == VAR_FLOAT ? *(float *) m_pSrcValue : (float) *(int *) m_pSrcValue );
				}
				else if ( m_DestinationType == VAR_FLOAT )
					SpecialValueFloat( (float *) m_pDstValue, value );
				else if ( m_DestinationType == VAR_INTEGER )
				{
					if ( m_SourceType == VAR_INTEGER )
						SpecialValueInt( (int *) m_pDstValue, *(int *) m_pSrcValue );
					else if ( m_iFloatConversion == 17 )
						SpecialValueInt( (int *) m_pDstValue, (int) (float) ceil( value ) );
					else if ( m_iFloatConversion == 16 )
						SpecialValueInt( (int *) m_pDstValue, (int) ( value + 0.5 ) );
					else
						SpecialValueInt( (int *) m_pDstValue, (int) value );
				}
				else if ( m_DestinationType == VAR_STRING )
				{
					if ( m_SourceType == VAR_INTEGER )
						UTIL_FormatString( buffer, 12, "%i", *(int *) m_pSrcValue );
					else
						WriteFloatToString( buffer, 32, value, m_iFloatConversion );

					stringResult = true;
				}
				break;
			}
			case VAR_STRING:
			{
				const char *value = STRING( *(string_t *) m_pSrcValue );

				if ( m_DestinationType == VAR_VECTOR )
				{
					Vector vector;

					UTIL_StringToVector( vector, value, ' ' );

					if ( m_iszValueType == VALUE_OP_DIR_TO_ANGLES )
					{
						Vector angles;

						VEC_TO_ANGLES( vector, angles );
						vector = angles;
					}
					else if ( m_iszValueType == VALUE_OP_ANGLES_TO_DIR )
					{
						UTIL_MakeVectors( vector );
						vector = gpGlobals->v_forward;
					}

					for ( int index = 0; index < 3; index++ )
					{
						if ( pev->spawnflags & ( 1 << index ) )
							continue;

						if ( m_iszValueType == VALUE_OP_DIR_TO_ANGLES || m_iszValueType == VALUE_OP_ANGLES_TO_DIR )
							( (float *) m_pDstValue )[index] = vector[index];
						else
							SpecialValueFloat( (float *) m_pDstValue + index, vector[index] );
					}
				}
				else if ( m_DestinationType == VAR_FLOAT )
					SpecialValueFloat( (float *) m_pDstValue, (float) atof( value ) );
				else if ( m_DestinationType == VAR_INTEGER )
				{
					if ( *value >= 'A' && *value <= 'Z' )
					{
						bool valid = false;
						int integer = ValueForPEVFlag( value, m_iDstValueIndex, valid );

						if ( valid )
							SpecialValueInt( (int *) m_pDstValue, integer );
						else
							ALERT( at_console, "Trigger_copyvalue \"%s\": Unknown value-identifier \"%s\" for target keyvalue \"%s\".\n",
							       STRING( pev->targetname ), value, STRING( m_iszDstValueName ) );
					}
					else
						SpecialValueInt( (int *) m_pDstValue, atoi( value ) );
				}
				else if ( m_DestinationType == VAR_STRING )
				{
					if ( m_bUseKeyValue )
					{
						if ( !TryKeyValue( (char *) value ) )
							return;
					}
					else
						SpecialValueByteArray( (int *) m_pDstValue, value, destination );
				}
				else if ( m_DestinationType == VAR_EDICT && m_pDstValue )
				{
					CBaseEntity *entity = UTIL_FindEntityByTargetname( NULL, value );

					*(edict_t **) m_pDstValue = entity && entity->pev ? entity->edict() : NULL;
				}
				break;
			}
			case VAR_EDICT:
				if ( m_DestinationType == VAR_STRING && m_pSrcValue )
				{
					edict_t *entity = *(edict_t **) m_pSrcValue;

					if ( entity && !entity->free && ENTINDEX( entity ) != 0 )
					{
						const char *value = STRING( entity->v.targetname );

						if ( m_bUseKeyValue )
						{
							if ( !TryKeyValue( (char *) value ) )
								return;
						}
						else
							SpecialValueByteArray( (int *) m_pDstValue, value, destination );
					}
				}
				break;
			default:
				if ( m_SourceType == VAR_INVALID && m_DestinationType == VAR_EDICT )
				{
					if ( m_iszValueType != VALUE_OP_REPLACE )
						ALERT( at_console, "Trigger_copyvalue \"%s\": Tried to edit edict keyvalue \"%s\" with non-REPLACE edit-type %s\n",
						       STRING( pev->targetname ), STRING( m_iszDstValueName ), ChangeValueTypeStr( m_iszValueType ) );
					else if ( m_pDstValue )
					{
						CBaseEntity *source = CopyValueHandleEntity( m_hSource );

						*(edict_t **) m_pDstValue = source && source->pev ? source->edict() : NULL;
					}
				}
				break;
		}

		if ( stringResult )
		{
			if ( m_bUseKeyValue )
			{
				if ( !TryKeyValue( buffer ) )
					return;
			}
			else
				SpecialValueByteArray( (int *) m_pDstValue, buffer, destination );
		}

		destination = CopyValueHandleEntity( m_hDestination );

		if ( !destination )
		{
			Reset();
			SetThink( NULL );
			return;
		}

		if ( m_iDstValueIndex == 2 )
			UTIL_SetOrigin( destination->pev, destination->pev->origin );

		destination->OnKeyValueUpdate( STRING( m_iszDstValueName ) );

		if ( !( pev->spawnflags & SF_COPYVALUE_MULTIPLE_DESTINATIONS ) )
			goto complete;
	}

	if ( !( pev->spawnflags & SF_COPYVALUE_MULTIPLE_DESTINATIONS ) || !CopyValueHandleEntity( m_hSource ) )
	{
		Reset();
		SetThink( NULL );
		return;
	}

	m_hSource = EHANDLE();
	m_hDestination = EHANDLE();

complete:
	if ( pev->spawnflags & SF_COPYVALUE_CONSTANT )
	{
		m_bActive = true;
		pev->nextthink = pev->dmg < 0 ? gpGlobals->time : gpGlobals->time + pev->dmg;
		SetThink( &CTriggerCopyValue::CopyValue );
	}
	else
	{
		m_bActive = false;
		SetThink( NULL );
	}

	if ( pev->message )
		FireTargets( STRING( pev->message ), CopyValueHandleEntity( m_hActivator ), this, USE_TOGGLE, 0 );
}
