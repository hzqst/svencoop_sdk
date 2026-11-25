#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "trigger_condition.h"
#include "server_entvars.h"

#define SF_CONDITION_START_OFF 1
#define SF_CONDITION_IGNORE_X 2
#define SF_CONDITION_IGNORE_Y 4
#define SF_CONDITION_IGNORE_Z 8
#define SF_CONDITION_CHECK_ONCE 32
#define SF_CONDITION_KEEP_ACTIVATOR 64
#define SF_CONDITION_IGNORE_INITIAL 128

#define CONDITION_FLOAT_TEXT_SIZE 48
#define CONDITION_INT_TEXT_SIZE 12

enum ConditionCheckType
{
	CONDITION_EQUAL = 0,
	CONDITION_NOT_EQUAL,
	CONDITION_LESS,
	CONDITION_GREATER,
	CONDITION_LESS_EQUAL,
	CONDITION_GREATER_EQUAL,
	CONDITION_BIT_AND,
	CONDITION_LOOKAT_ENTITY,
	CONDITION_SEE_ENTITY,
	CONDITION_LOOKAT_POINT,
	CONDITION_SEE_POINT
};

CBaseEntity *ConditionHandleEntity( EHANDLE &handle )
{
	edict_t *entity = handle.Get();

	if ( !entity || entity->free )
		return NULL;

	return (CBaseEntity *) GET_PRIVATE( entity );
}

bool ConditionEntityValid( CBaseEntity *entity )
{
	return entity && entity->pev && entity->pev->pContainingEntity
		&& !entity->pev->pContainingEntity->free && !FNullEnt( entity->pev->pContainingEntity );
}

void ConditionSetHandle( EHANDLE &handle, CBaseEntity *entity )
{
	handle = EHANDLE();

	if ( entity && entity->pev && entity->pev->pContainingEntity && !entity->pev->pContainingEntity->free )
		handle.Set( entity->pev->pContainingEntity );
}

CBaseEntity *ConditionEdictEntity( edict_t *entity )
{
	if ( !entity )
		entity = ENT( 0 );

	if ( !entity || entity->free )
		return NULL;

	return (CBaseEntity *) GET_PRIVATE( entity );
}

LINK_ENTITY_TO_CLASS( trigger_condition, CTriggerCondition );

TYPEDESCRIPTION CTriggerCondition::m_SaveData[] =
{
	DEFINE_FIELD( CTriggerCondition, m_iszValueName, FIELD_STRING ),
	DEFINE_FIELD( CTriggerCondition, m_iszCheckValue, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CTriggerCondition, CBaseDelay );

CTriggerCondition::CTriggerCondition()
	: m_hTarget(), m_hSource(), m_hActivator(), m_hCaller()
{
}

void CTriggerCondition::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszCheckValue" ) )
	{
		m_iszCheckValue = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszValueName" ) )
	{
		m_iszValueName = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_flDelay" ) )
	{
		m_flDelay = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iCheckType" ) )
	{
		m_iCheckType = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszSourceName" ) )
	{
		m_iszSourceName = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszSourceKey" ) )
	{
		m_iszSourceKey = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_fCheckInterval" ) )
	{
		m_fCheckInterval = atof( pkvd->szValue );

		if ( m_fCheckInterval < 0.001f )
			m_fCheckInterval = 0.001f;

		m_bIntervalSet = true;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iCheckBehaviour" ) )
	{
		m_iCheckBehaviour = atoi( pkvd->szValue );

		if ( m_iCheckBehaviour < 0 )
			m_iCheckBehaviour = (int) ( 0u - (unsigned int) m_iCheckBehaviour );

		if ( m_iCheckBehaviour > 3 )
			m_iCheckBehaviour = 0;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void *CTriggerCondition::GetKeyValue( const char *name, entvartype_s &type )
{
	if ( FStrEq( name, "m_iszCheckValue" ) )
	{
		type = VAR_STRING;
		return &m_iszCheckValue;
	}

	if ( FStrEq( name, "m_iszValueName" ) )
	{
		type = VAR_STRING;
		return &m_iszValueName;
	}

	if ( !stricmp( name, "m_flDelay" ) )
	{
		type = VAR_FLOAT;
		return &m_flDelay;
	}

	if ( FStrEq( name, "m_iCheckType" ) )
	{
		type = VAR_INTEGER;
		return &m_iCheckType;
	}

	if ( FStrEq( name, "m_iszSourceName" ) )
	{
		type = VAR_STRING;
		return &m_iszSourceName;
	}

	if ( FStrEq( name, "m_iszSourceKey" ) )
	{
		type = VAR_STRING;
		return &m_iszSourceKey;
	}

	if ( FStrEq( name, "m_fCheckInterval" ) )
	{
		type = VAR_FLOAT;
		return &m_fCheckInterval;
	}

	if ( FStrEq( name, "m_iCheckBehaviour" ) )
	{
		type = VAR_INTEGER;
		return &m_iCheckBehaviour;
	}

	return CBaseDelay::GetKeyValue( name, type );
}

void CTriggerCondition::Spawn( void )
{
	m_hTarget = EHANDLE();
	m_pValue = NULL;
	m_ppValue = &m_pValue;
	m_ValueType = VAR_INVALID;
	m_SourceType = VAR_INVALID;
	m_bSpecialTarget = false;
	m_bActive = ( pev->spawnflags & ( SF_CONDITION_START_OFF | SF_CONDITION_CHECK_ONCE ) ) == 0;

	if ( !m_bIntervalSet )
		m_fCheckInterval = 0.1f;

	m_iLastResult = ( pev->spawnflags & ( SF_CONDITION_CHECK_ONCE | SF_CONDITION_IGNORE_INITIAL ) ) == SF_CONDITION_IGNORE_INITIAL ? 2 : -1;

	if ( m_bActive )
		Use( NULL, NULL, USE_ON, 0 );
}

void CTriggerCondition::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( pev->spawnflags & SF_CONDITION_CHECK_ONCE )
	{
		if ( useType != USE_OFF )
			m_bActive = true;
	}
	else if ( ShouldToggle( useType, m_bActive ) )
		m_bActive = !m_bActive;

	if ( !m_bActive )
	{
		m_iLastResult = ( pev->spawnflags & ( SF_CONDITION_CHECK_ONCE | SF_CONDITION_IGNORE_INITIAL ) ) == SF_CONDITION_IGNORE_INITIAL ? 2 : -1;
		SetThink( NULL );
		return;
	}

	if ( FStrEq( STRING( pev->target ), "!activator" ) && ConditionEntityValid( activator ) )
	{
		ConditionSetHandle( m_hTarget, activator );
		m_bSpecialTarget = true;
	}
	else if ( FStrEq( STRING( pev->target ), "!caller" ) && ConditionEntityValid( caller ) )
	{
		ConditionSetHandle( m_hTarget, caller );
		m_bSpecialTarget = true;
	}
	else
		m_bSpecialTarget = false;

	ConditionSetHandle( m_hActivator, activator );
	ConditionSetHandle( m_hCaller, caller );

	if ( !( pev->spawnflags & SF_CONDITION_CHECK_ONCE ) )
	{
		SetThink( &CTriggerCondition::CheckValue );
		pev->nextthink = gpGlobals->time + m_fCheckInterval;
	}

	CheckValue();
}

void CTriggerCondition::Result( bool result )
{
	if ( !( pev->spawnflags & SF_CONDITION_KEEP_ACTIVATOR ) || !ConditionEntityValid( ConditionHandleEntity( m_hActivator ) ) )
		ConditionSetHandle( m_hActivator, this );

	if ( m_iLastResult == 2 )
	{
		m_iLastResult = result;
		ALERT( at_aiconsole, " -> %s; Ignoring initial result.\n", result ? "True" : "False" );
		return;
	}

	if ( result )
	{
		if ( m_iLastResult == 1 && ( m_iCheckBehaviour == 0 || m_iCheckBehaviour == 2 ) )
		{
			ALERT( at_aiconsole, " -> True; Waiting for false however.\n" );
			return;
		}

		m_iLastResult = ( pev->spawnflags & SF_CONDITION_CHECK_ONCE ) ? -1 : 1;

		if ( pev->netname )
			ALERT( at_aiconsole, " -> True; Triggering \"%s\".\n", STRING( pev->netname ) );
		else
			ALERT( at_aiconsole, " -> True; Got no true-target to trigger, though.\n" );

		FireTargets( STRING( pev->netname ), ConditionHandleEntity( m_hActivator ), this, USE_TOGGLE, 0 );
	}
	else
	{
		if ( m_iLastResult == 0 && (unsigned int) m_iCheckBehaviour <= 1 )
		{
			ALERT( at_aiconsole, " -> False; Waiting for true however.\n" );
			return;
		}

		m_iLastResult = ( pev->spawnflags & SF_CONDITION_CHECK_ONCE ) ? -1 : 0;

		if ( pev->message )
			ALERT( at_aiconsole, " -> False; Triggering \"%s\".\n", STRING( pev->message ) );
		else
			ALERT( at_aiconsole, " -> False; Got no false-target to trigger, though.\n" );

		FireTargets( STRING( pev->message ), ConditionHandleEntity( m_hActivator ), this, USE_TOGGLE, 0 );
	}
}

void CTriggerCondition::CheckValue( void )
{
	if ( pev->nextthink > gpGlobals->time )
		return;

	bool checkTarget = ConditionEntityValid( ConditionHandleEntity( m_hTarget ) );

	if ( !checkTarget || ( !m_bSpecialTarget && ConditionEntityValid( ConditionHandleEntity( m_hTarget ) )
		&& !FStrEq( STRING( ConditionHandleEntity( m_hTarget )->pev->targetname ), STRING( pev->targetname ) ) ) )
	{
		ConditionSetHandle( m_hTarget, UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) ) );
		checkTarget = ConditionEntityValid( ConditionHandleEntity( m_hTarget ) );
	}

	if ( checkTarget )
	{
		m_bUseSource = m_iszSourceName != 0;

		if ( m_bUseSource )
		{
			if ( !ConditionEntityValid( ConditionHandleEntity( m_hSource ) )
				|| ( ConditionEntityValid( ConditionHandleEntity( m_hSource ) )
				&& !FStrEq( STRING( ConditionHandleEntity( m_hSource )->pev->targetname ), STRING( m_iszSourceName ) ) ) )
			{
				if ( FStrEq( STRING( m_iszSourceName ), "!activator" ) )
					m_hSource = m_hActivator;
				else if ( FStrEq( STRING( m_iszSourceName ), "!caller" ) )
					m_hSource = m_hCaller;
				else
					ConditionSetHandle( m_hSource, UTIL_FindEntityByTargetname( NULL, STRING( m_iszSourceName ) ) );
			}

			if ( !ConditionEntityValid( ConditionHandleEntity( m_hSource ) ) )
			{
				ALERT( at_console, "Trigger_condition \"%s\": Lost sight of or could not find source entity \"%s\". Ceasing action.\n",
				       STRING( pev->targetname ), STRING( m_iszSourceName ) );
				m_bActive = false;
				m_bUseSource = false;
				m_iLastResult = -1;
				SetThink( NULL );
				return;
			}

			int index;

			m_SourceType = ConditionHandleEntity( m_hSource )->GetEntvar( STRING( m_iszSourceKey ), &m_pSourceValue, index );

			if ( m_SourceType == VAR_INVALID )
				m_bUseSource = false;
		}

		if ( m_iCheckType >= CONDITION_LOOKAT_ENTITY && m_iCheckType <= CONDITION_SEE_POINT )
		{
			CBaseEntity *lookEntity = NULL;
			Vector point( 0, 0, 0 );

			if ( m_iCheckType == CONDITION_LOOKAT_ENTITY || m_iCheckType == CONDITION_SEE_ENTITY )
			{
				if ( m_bUseSource && m_SourceType != VAR_STRING )
				{
					ALERT( at_console, "Trigger_condition \"%s\": Cannot identify look-at-entity through keyvalue of type %s; VAR_STRING needed.\n",
					       STRING( pev->targetname ), EntVarStr( m_SourceType ) );
					SetThink( NULL );
					UTIL_Remove( this );
					return;
				}

				lookEntity = UTIL_FindEntityByTargetname( NULL, STRING( m_bUseSource ? *(string_t *) m_pSourceValue : m_iszCheckValue ) );

				if ( lookEntity )
					point = lookEntity->Center();
			}
			else if ( !m_bUseSource || m_SourceType == VAR_STRING )
				UTIL_StringToVector( point, STRING( m_bUseSource ? *(string_t *) m_pSourceValue : m_iszCheckValue ) );
			else if ( m_SourceType == VAR_VECTOR )
				point = *(Vector *) m_pSourceValue;
			else
			{
				ALERT( at_console, "Trigger_condition \"%s\": Cannot identify look-at-point through keyvalue of type %s; VAR_STRING containing a vector or VAR_VECTOR needed.\n",
				       STRING( pev->targetname ), EntVarStr( m_SourceType ) );
				SetThink( NULL );
				UTIL_Remove( this );
				return;
			}

			CBaseEntity *target = ConditionHandleEntity( m_hTarget );

			Vector eye = target->pev->origin + target->pev->view_ofs;
			Vector forward;

			AngleVectors( target->pev->angles, &forward, NULL, NULL );

			Vector delta = point - eye;
			float dot = forward.y * delta.y + forward.x * delta.x;

			dot += forward.z * delta.z;

			float radians = (float) acos( (double) dot );
			float degrees = radians * 180.0f;
			float angle = (float) ( (double) degrees / 3.141592653589793 );
			bool result = target->pev->fov > angle;

			if ( m_iCheckType == CONDITION_SEE_ENTITY || m_iCheckType == CONDITION_SEE_POINT )
			{
				ALERT( at_aiconsole, "Trigger_condition \"%s\": Can \"%s\" see %.2f %.2f %.2f from %.2f %.2f %.2f?",
				       STRING( pev->targetname ), STRING( target->pev->targetname ), point.x, point.y, point.z, eye.x, eye.y, eye.z );

				if ( result )
				{
					TraceResult trace;
					target = ConditionHandleEntity( m_hTarget );

					TRACE_LINE( eye, eye + forward, 257, target->edict(), &trace );
					result = trace.flFraction == 1.0f
						|| ( lookEntity && trace.pHit == ( lookEntity->edict() ) );
				}
			}
			else
				ALERT( at_aiconsole, "Trigger_condition \"%s\": Does \"%s\" look at %.2f %.2f %.2f from %.2f %.2f %.2f?",
				       STRING( pev->targetname ), STRING( target->pev->targetname ), point.x, point.y, point.z, eye.x, eye.y, eye.z );

			Result( result );
		}
		else
		{
			m_ValueType = ConditionHandleEntity( m_hTarget )->GetEntvar( STRING( m_iszValueName ), m_ppValue, m_iValueIndex );
			m_pValue = *m_ppValue;

			switch ( m_ValueType )
			{
				case VAR_INVALID:
					if ( m_bUseSource )
						ALERT( at_console, "Trigger_condition \"%s\": Non-existent or unsupported entity-specific keyvalue \"%s\".\n", STRING( pev->targetname ), STRING( m_iszSourceKey ) );
					else
						ALERT( at_console, "Trigger_condition \"%s\": Non-existent or unsupported entity-specific keyvalue: \"%s\".\n", STRING( pev->targetname ), STRING( m_iszValueName ) );

					SetThink( NULL );
					UTIL_Remove( this );
					return;
				case VAR_STRING:
					CheckString();
					break;
				case VAR_VECTOR:
					CheckVector();
					break;
				case VAR_FLOAT:
					CheckFloat();
					break;
				case VAR_INTEGER:
					CheckInteger();
					break;
				case VAR_EDICT:
					CheckEdict();
					break;
				case VAR_BYTE4:
					CheckBytes( 4 );
					break;
				case VAR_BYTE2:
					CheckBytes( 2 );
					break;
			}
		}
	}

	if ( pev->spawnflags & SF_CONDITION_CHECK_ONCE )
	{
		m_iLastResult = -1;
		m_bActive = false;
		SetThink( NULL );
	}
	else
		pev->nextthink = gpGlobals->time + m_fCheckInterval;
}

float CTriggerCondition::MakeVecLength( Vector *value )
{
	int components = 3;

	if ( pev->spawnflags & SF_CONDITION_IGNORE_X )
	{
		value->x = 0;
		components--;
	}

	if ( pev->spawnflags & SF_CONDITION_IGNORE_Y )
	{
		value->y = 0;
		components--;
	}

	if ( pev->spawnflags & SF_CONDITION_IGNORE_Z )
	{
		value->z = 0;
		components--;
	}

	if ( components == 1 )
		return ( value->y + value->x ) + value->z;

	float squared = value->y * value->y + value->x * value->x;

	squared += value->z * value->z;
	return (float) sqrt( (double) squared );
}

int CTriggerCondition::GetByteCondition( int index )
{
	static const int conditions[] = { 2, 4, 8, 16 };

	return (unsigned int) index < ARRAYSIZE( conditions ) ? conditions[index] : 0;
}

bool ConditionCompare( float left, float right, int operation )
{
	switch ( operation )
	{
		case CONDITION_EQUAL:
			return left == right;
		case CONDITION_NOT_EQUAL:
			return left != right;
		case CONDITION_LESS:
			return left < right;
		case CONDITION_GREATER:
			return left > right;
		case CONDITION_LESS_EQUAL:
			return left <= right;
		case CONDITION_GREATER_EQUAL:
			return left >= right;
		case CONDITION_BIT_AND:
			return ( (int) left & (int) right ) != 0;
	}

	return false;
}

bool ConditionCompare( int left, int right, int operation )
{
	switch ( operation )
	{
		case CONDITION_EQUAL:
			return left == right;
		case CONDITION_NOT_EQUAL:
			return left != right;
		case CONDITION_LESS:
			return left < right;
		case CONDITION_GREATER:
			return left > right;
		case CONDITION_LESS_EQUAL:
			return left <= right;
		case CONDITION_GREATER_EQUAL:
			return left >= right;
		case CONDITION_BIT_AND:
			return ( left & right ) != 0;
	}

	return false;
}

static const char *g_ConditionOperations[] = {
	"equal to", "not equal to", "less than", "greater than",
	"less than or equal to", "greater than or equal to", "bit-wise AND with"
};

void CTriggerCondition::CheckFloat( void )
{
	float left = *(float *) m_pValue;
	float right;

	if ( m_bUseSource )
	{
		switch ( m_SourceType )
		{
			case VAR_STRING:
				right = (float) atof( STRING( *(string_t *) m_pSourceValue ) );
				break;
			case VAR_VECTOR:
			{
				Vector vector = *(Vector *) m_pSourceValue;

				right = MakeVecLength( &vector );
				break;
			}
			case VAR_FLOAT:
				right = *(float *) m_pSourceValue;
				break;
			case VAR_INTEGER:
				right = (float) *(int *) m_pSourceValue;
				break;
			default:
				ALERT( at_console, "Trigger_condition \"%s\": Cannot compare VAR_FLOAT keyvalue \"%s\" to the %s keyvalue \"%s\".\n",
				       STRING( pev->targetname ), STRING( m_iszValueName ), EntVarStr( m_SourceType ), STRING( m_iszSourceKey ) );
				SetThink( NULL );
				UTIL_Remove( this );
				return;
		}
	}
	else
		right = (float) atof( STRING( m_iszCheckValue ) );

	const int operation = m_iCheckType;

	if ( (unsigned int) operation > CONDITION_BIT_AND )
	{
		ALERT( at_console, "Trigger_condition \"%s\": Unknown compare-operation for VAR_FLOAT keyvalue \"%s\" \"%f\".\n",
		       STRING( pev->targetname ), STRING( m_iszValueName ), *(float *) m_pValue );
		SetThink( NULL );
		UTIL_Remove( this );
		return;
	}

	ALERT( at_aiconsole, "Trigger_condition \"%s\": Value %.6f %s %.6f%s", STRING( pev->targetname ),
	       left, g_ConditionOperations[operation], right, operation == CONDITION_BIT_AND ? " not zero?" : "?" );
	Result( ConditionCompare( left, right, operation ) );
}

void CTriggerCondition::CheckInteger( void )
{
	int left = *(int *) m_pValue;
	int right;

	if ( m_bUseSource )
	{
		switch ( m_SourceType )
		{
			case VAR_STRING:
				right = atoi( STRING( *(string_t *) m_pSourceValue ) );
				break;
			case VAR_VECTOR:
			{
				Vector vector = *(Vector *) m_pSourceValue;

				right = (int) MakeVecLength( &vector );
				break;
			}
			case VAR_FLOAT:
				right = (int) *(float *) m_pSourceValue;
				break;
			case VAR_INTEGER:
				right = *(int *) m_pSourceValue;
				break;
			default:
				ALERT( at_console, "Trigger_condition \"%s\": Cannot compare the VAR_INTEGER keyvalue \"%s\" to the %s keyvalue \"%s\".\n",
				       STRING( pev->targetname ), STRING( m_iszValueName ), EntVarStr( m_SourceType ), STRING( m_iszSourceKey ) );
				SetThink( NULL );
				UTIL_Remove( this );
				return;
		}
	}
	else
	{
		const char *text = STRING( m_iszCheckValue );

		if ( *text >= 'A' && *text <= 'Z' )
		{
			bool valid;

			right = ValueForPEVFlag( text, m_iValueIndex, valid );

			if ( !valid )
				ALERT( at_console, "Trigger_condition \"%s\": Unknown value-identifier \"%s\" for monitored keyvalue \"%s\".\n",
				       STRING( pev->targetname ), text, STRING( m_iszValueName ) );
		}
		else
			right = atoi( text );
	}

	const int operation = m_iCheckType;

	if ( (unsigned int) operation > CONDITION_BIT_AND )
	{
		ALERT( at_console, "Trigger_condition \"%s\": Unknown compare-operation for VAR_INTEGER keyvalue \"%s\" \"%i\".\n",
		       STRING( pev->targetname ), STRING( m_iszValueName ), *(int *) m_pValue );
		SetThink( NULL );
		UTIL_Remove( this );
		return;
	}

	ALERT( at_aiconsole, "Trigger_condition \"%s\": Value %i %s %i%s", STRING( pev->targetname ),
	       left, g_ConditionOperations[operation], right, operation == CONDITION_BIT_AND ? " not zero?" : "?" );
	Result( ConditionCompare( left, right, operation ) );
}

void CTriggerCondition::CheckString( void )
{
	const char *left = STRING( *(string_t *) m_pValue );
	const char *right = STRING( m_iszCheckValue );
	char converted[CONDITION_FLOAT_TEXT_SIZE * 3 + 2];

	if ( m_bUseSource )
	{
		switch ( m_SourceType )
		{
			case VAR_STRING:
				right = STRING( *(string_t *) m_pSourceValue );
				break;
			case VAR_VECTOR:
			{
				Vector *vector = (Vector *) m_pSourceValue;

				_snprintf( converted, sizeof( converted ), "%f %f %f", vector->x, vector->y, vector->z );
				right = converted;
				break;
			}
			case VAR_FLOAT:
				_snprintf( converted, CONDITION_FLOAT_TEXT_SIZE, "%f", *(float *) m_pSourceValue );
				right = converted;
				break;
			case VAR_INTEGER:
				_snprintf( converted, CONDITION_INT_TEXT_SIZE, "%i", *(int *) m_pSourceValue );
				right = converted;
				break;
			case VAR_EDICT:
			{
				CBaseEntity *entity = ConditionEdictEntity( *(edict_t **) m_pSourceValue );

				if ( entity )
					right = STRING( entity->pev->targetname );
				else
				{
					const int operation = m_iCheckType;

					if ( operation == CONDITION_EQUAL || operation == CONDITION_NOT_EQUAL )
					{
						ALERT( at_aiconsole, "Trigger_condition \"%s\": String \"%s\" %s null-string?",
						       STRING( pev->targetname ), left, g_ConditionOperations[operation] );
						Result( operation == CONDITION_NOT_EQUAL );
						return;
					}

					right = "";
				}

				break;
			}
			default:
				ALERT( at_console, "Trigger_condition \"%s\": Incomparable types of VAR_STRING keyvalue \"%s\" \"%s\" and %s key \"%s\".\n",
				       STRING( pev->targetname ), STRING( m_iszValueName ), left, EntVarStr( m_SourceType ), STRING( m_iszSourceKey ) );
				SetThink( NULL );
				UTIL_Remove( this );
				return;
		}
	}

	const int operation = m_iCheckType;

	if ( operation == CONDITION_EQUAL || operation == CONDITION_NOT_EQUAL )
	{
		ALERT( at_aiconsole, "Trigger_condition \"%s\": String \"%s\" %s \"%s\"?", STRING( pev->targetname ),
		       left, g_ConditionOperations[operation], right );
		Result( operation == CONDITION_EQUAL ? strcmp( left, right ) == 0 : strcmp( left, right ) != 0 );
		return;
	}

	if ( m_bUseSource )
		ALERT( at_console, "Trigger_condition \"%s\": Can't do arithmetic or logic checks between VAR_STRING keyvalue \"%s\" \"%s\" and %s keyvalue \"%s\" \"%s\".\n",
		       STRING( pev->targetname ), STRING( m_iszValueName ), left, EntVarStr( m_SourceType ), STRING( m_iszSourceKey ), right );
	else
		ALERT( at_console, "Trigger_condition \"%s\": Can't do arithmetic or logic checks between VAR_STRING keyvalue \"%s\" \"%s\" and VAR_STRING value \"%s\".\n",
		       STRING( pev->targetname ), STRING( m_iszValueName ), left, right );

	SetThink( NULL );
	UTIL_Remove( this );
}

void CTriggerCondition::CheckEdict( void )
{
	CBaseEntity *left = m_pValue ? ConditionEdictEntity( *(edict_t **) m_pValue ) : NULL;
	CBaseEntity *right;

	if ( !m_bUseSource || m_SourceType == VAR_STRING )
		right = UTIL_FindEntityByTargetname( NULL, STRING( m_bUseSource ? *(string_t *) m_pSourceValue : m_iszCheckValue ) );
	else if ( m_SourceType == VAR_EDICT )
		right = ConditionEdictEntity( *(edict_t **) m_pSourceValue );
	else
	{
		ALERT( at_console, "Trigger_condition \"%s\": Cannot compare VAR_EDICT keyvalue \"%s\" to the %s keyvalue \"%s\".\n",
		       STRING( pev->targetname ), STRING( m_iszValueName ), EntVarStr( m_SourceType ), STRING( m_iszSourceKey ) );
		SetThink( NULL );
		UTIL_Remove( this );
		return;
	}

	const int operation = m_iCheckType;

	if ( operation == CONDITION_EQUAL || operation == CONDITION_NOT_EQUAL )
	{
		ALERT( at_aiconsole, "Trigger_condition \"%s\": %s \"%s\" %s entity as %s \"%s\"?", STRING( pev->targetname ),
		       left ? STRING( left->pev->classname ) : "Invalid entity", left ? STRING( left->pev->targetname ) : "",
		       operation == CONDITION_EQUAL ? "is them same" : "is not them same",
		       right ? STRING( right->pev->classname ) : "invalid entity", right ? STRING( right->pev->targetname ) : "" );
		Result( operation == CONDITION_EQUAL ? left == right : left != right );
		return;
	}

	if ( left )
		ALERT( at_console, "Trigger_condition \"%s\": Can't perform arithmetic or logical comparisons for VAR_EDICT keyvalue \"%s\" pointing to entity with targetname \"%s\".\n",
		       STRING( pev->targetname ), STRING( m_iszValueName ), STRING( left->pev->targetname ) );
	else
		ALERT( at_console, "Trigger_condition \"%s\": Can't perform arithmetic or logical comparisons for VAR_EDICT keyvalue \"%s\" (also pointing to no valid entity).\n",
		       STRING( pev->targetname ), STRING( m_iszValueName ) );

	SetThink( NULL );
	UTIL_Remove( this );
}

void CTriggerCondition::CheckVector( void )
{
	Vector right;
	float scalar = 0;
	bool scalarComparison = false;

	if ( m_bUseSource )
	{
		switch ( m_SourceType )
		{
			case VAR_STRING:
				UTIL_StringToVector( right, STRING( *(string_t *) m_pSourceValue ) );
				break;
			case VAR_VECTOR:
				right = *(Vector *) m_pSourceValue;

				if ( (unsigned int) m_iCheckType >= CONDITION_LESS && m_iCheckType != CONDITION_BIT_AND )
				{
					scalar = MakeVecLength( &right );
					scalarComparison = true;
				}

				break;
			case VAR_FLOAT:
				scalar = *(float *) m_pSourceValue;
				scalarComparison = true;
				break;
			case VAR_INTEGER:
				scalar = (float) *(int *) m_pSourceValue;
				scalarComparison = true;
				break;
			default:
				ALERT( at_console, "Trigger_condition \"%s\": Cannot compare VAR_VECTOR keyvalue \"%s\" to the %s keyvalue \"%s\".\n",
				       STRING( pev->targetname ), STRING( m_iszValueName ), EntVarStr( m_SourceType ), STRING( m_iszSourceKey ) );
				SetThink( NULL );
				UTIL_Remove( this );
				return;
		}
	}
	else if ( UTIL_IsStringInt( STRING( m_iszCheckValue ) ) || UTIL_IsStringFloat( STRING( m_iszCheckValue ) ) )
	{
		scalar = (float) atof( STRING( m_iszCheckValue ) );
		scalarComparison = true;
	}
	else
		UTIL_StringToVector( right, STRING( m_iszCheckValue ) );

	const int operation = m_iCheckType;

	if ( scalarComparison )
	{
		Vector left = *(Vector *) m_pValue;
		float length = MakeVecLength( &left );

		if ( (unsigned int) operation <= CONDITION_BIT_AND )
		{
			ALERT( at_aiconsole, "Trigger_condition \"%s\": Length of %.2f %.2f %.2f (%.2f) %s %.2f%s", STRING( pev->targetname ),
			       left.x, left.y, left.z, length, g_ConditionOperations[operation], scalar, operation == CONDITION_BIT_AND ? " is non-zero?" : "?" );
			Result( ConditionCompare( length, scalar, operation ) );
			return;
		}
	}
	else if ( operation == CONDITION_EQUAL || operation == CONDITION_NOT_EQUAL || operation == CONDITION_BIT_AND )
	{
		Vector left = *(Vector *) m_pValue;

		for ( int i = 0; i < 3; i++ )
		{
			if ( pev->spawnflags & GetByteCondition( i ) )
				left[i] = operation == CONDITION_EQUAL ? right[i] : (float) ( (double) right[i] + 1.0 );
		}

		if ( operation == CONDITION_BIT_AND )
		{
			ALERT( at_aiconsole, "Trigger_condition \"%s\": %.2f %.2f %.2f bit-wise AND with %.2f %.2f %.2f is not zero?",
			       STRING( pev->targetname ), left.x, left.y, left.z, right.x, right.y, right.z );
			Result( ( (int) left.x & (int) right.x ) != 0
				&& ( (int) left.y & (int) right.y ) != 0 && ( (int) left.z & (int) right.z ) != 0 );
		}
		else
		{
			ALERT( at_aiconsole, "Trigger_condition \"%s\": %.2f %.2f %.2f %s %.2f %.2f %.2f?",
			       STRING( pev->targetname ), left.x, left.y, left.z, g_ConditionOperations[operation], right.x, right.y, right.z );
			bool equal = left.x == right.x && left.y == right.y && left.z == right.z;

			Result( operation == CONDITION_EQUAL ? equal : !equal );
		}

		return;
	}

	Vector *left = (Vector *) m_pValue;

	ALERT( at_console, "Trigger_condition \"%s\": Unknown compare-operation for VAR_VECTOR keyvalue \"%s\" \"%f %f %f\".\n",
	       STRING( pev->targetname ), STRING( m_iszValueName ), left->x, left->y, left->z );
	SetThink( NULL );
	UTIL_Remove( this );
}

void CTriggerCondition::CheckBytes( int count )
{
	if ( !count )
		return;

	// A byte[4] source supplies four components even for a byte[2] comparison
	int *values = new int[count < 4 ? 4 : count];
	byte *left = new byte[count];
	byte *right = new byte[count];

	memset( values, 0, count * sizeof( int ) );
	memset( left, 0, count );
	memset( right, 0, count );

	if ( m_bUseSource )
	{
		switch ( m_SourceType )
		{
			case VAR_STRING:
				UTIL_StringToIntArray( values, count, STRING( *(string_t *) m_pSourceValue ) );
				break;
			case VAR_INTEGER:
				for ( int i = 0; i < count; i++ )
				{
					int shifted = *(int *) m_pSourceValue >> i;
					int magnitude = shifted < 0 ? (int) ( 0u - (unsigned int) shifted ) : shifted;

					values[i] = magnitude % 256;
				}

				break;
			case VAR_BYTE4:
				values[3] = ( (byte *) m_pSourceValue )[3];
				values[2] = ( (byte *) m_pSourceValue )[2];
			case VAR_BYTE2:
				values[1] = ( (byte *) m_pSourceValue )[1];
				values[0] = *(byte *) m_pSourceValue;
				break;
			default:
				ALERT( at_console, "Trigger_condition \"%s\": Cannot compare VAR_BYTE[%i] keyvalue \"%s\" to the %s keyvalue \"%s\".\n",
				       STRING( pev->targetname ), count, STRING( m_iszCheckValue ), EntVarStr( m_SourceType ), STRING( m_iszSourceKey ) );
				SetThink( NULL );
				UTIL_Remove( this );
				delete[] values;
				delete[] left;
				delete[] right;
				return;
		}
	}
	else
		UTIL_StringToIntArray( values, count, STRING( m_iszCheckValue ) );

	char *leftText = (char *) malloc( count * 3 );
	char *rightText = (char *) malloc( count * 3 );

	memset( leftText, 0, count * 3 );
	memset( rightText, 0, count * 3 );

	for ( int i = 0; i < count; i++ )
	{
		right[i] = (byte) values[i];
		left[i] = ( pev->spawnflags & GetByteCondition( i ) ) ? right[i] : ( (byte *) m_pValue )[i];

		char text[4] = { 0 };

		sprintf( text, "%x", left[i] );
		text[2] = i == count - 1 ? 0 : ' ';
		strncat( leftText, text, count * 3 - strlen( leftText ) - 1 );
		sprintf( text, "%x", right[i] );
		text[2] = i == count - 1 ? 0 : ' ';
		strncat( rightText, text, count * 3 - strlen( rightText ) - 1 );
	}

	const int operation = m_iCheckType;

	if ( (unsigned int) operation <= CONDITION_BIT_AND )
	{
		int matches = 0;

		for ( int i = 0; i < count; i++ )
		{
			if ( ConditionCompare( (int) left[i], (int) right[i], operation ) )
				matches++;
		}

		if ( operation == CONDITION_BIT_AND )
			ALERT( at_aiconsole, "Trigger_condition \"%s\": Byte-array %s byte-wise AND with %s leaves all %i bytes non-zero?",
			       STRING( pev->targetname ), leftText, rightText, count );
		else
			ALERT( at_aiconsole, "Trigger_condition \"%s\": Byte-array %s %s%s %s?", STRING( pev->targetname ),
			       leftText, operation >= CONDITION_LESS ? "byte-wise " : "", g_ConditionOperations[operation], rightText );

		Result( operation == CONDITION_NOT_EQUAL ? matches != 0 : matches == count );
	}
	else
	{
		ALERT( at_console, "Trigger_condition \"%s\": Unknown compare-operation for VAR_BYTE[%i] keyvalue \"%s\" \"",
		       STRING( pev->targetname ), count, STRING( m_iszValueName ) );

		for ( int i = 0; i < count - 1; i++ )
			ALERT( at_console, "%i ", left[i] );

		ALERT( at_console, "%i\".\n", left[count - 1] );
		SetThink( NULL );
		UTIL_Remove( this );
	}

	free( leftText );
	free( rightText );
	delete[] values;
	delete[] left;
	delete[] right;
}
