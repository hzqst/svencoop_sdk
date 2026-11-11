#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "trigger_numericdisplay.h"
#include "server_entvars.h"
#include "server_support.h"
#include <limits.h>
#include <math.h>

namespace
{
enum
{
	SF_NUMERICDISPLAY_START_ON = 1,
	SF_NUMERICDISPLAY_USE_ONLY = 2,
	SF_NUMERICDISPLAY_ALTERNATE_EMPTY = 4,
	SF_NUMERICDISPLAY_EMPTY_ZERO = 8,
	NUMERICDISPLAY_EFFECTS = 0x300
};

bool NumericDisplayEntityValid( CBaseEntity *entity )
{
	return entity && entity->pev && entity->pev->pContainingEntity &&
	       !entity->pev->pContainingEntity->free && !FNullEnt( entity->pev->pContainingEntity );
}
}

LINK_ENTITY_TO_CLASS( trigger_numericdisplay, CTriggerNumericDisplay );

void CTriggerNumericDisplay::SetState( bool enabled )
{
	m_bEnabled = enabled;

	if ( enabled )
	{
		SetThink( &CTriggerNumericDisplay::UpdateThink );
		pev->nextthink = gpGlobals->time + 0.1f;
	}
	else
		SetThink( NULL );
}

void CTriggerNumericDisplay::Spawn( void )
{
	if ( !( pev->spawnflags & SF_NUMERICDISPLAY_USE_ONLY ) )
		SetState( ( pev->spawnflags & SF_NUMERICDISPLAY_START_ON ) != 0 );
}

void CTriggerNumericDisplay::Clear( void )
{
	Digit_t *digit = m_pDigits;

	while ( digit )
	{
		EntityList_t *entity = digit->entities;

		while ( entity )
		{
			EntityList_t *next = entity->next;

			delete entity;
			entity = next;
		}

		Digit_t *next = digit->next;

		delete digit;
		digit = next;
	}

	m_pDigits = NULL;
}

int CTriggerNumericDisplay::GetUpdatedValue( void )
{
	if ( !pev->target || !pev->message )
		return 0;

	CBaseEntity *source = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );

	if ( !NumericDisplayEntityValid( source ) )
		return 0;

	void *field = NULL;
	int index = INVALID_ENTVAR_INDEX;
	entvartype_s type;

	type = source->GetEntvar( STRING( pev->netname ), &field, index );

	switch ( type )
	{
		case VAR_INVALID:
			return 0;
		case VAR_STRING:
			// Parses the returned field address directly
			return atoi( (const char *) field );
		case VAR_FLOAT:
			return (int) *(float *) field;
		case VAR_INTEGER:
			return *(int *) field;
		default:
			ALERT( at_warning, "point_numericdisplay(%s): keyvalue '%s' is an unsupported type!\n",
			       STRING( pev->targetname ), STRING( pev->netname ) );
			return 0;
	}
}

void CTriggerNumericDisplay::RemoveDigit( Digit_t *digit, Digit_t *previous )
{
	if ( previous )
		previous->next = digit->next;
	else
		m_pDigits = digit->next;

	delete digit;
}

void CTriggerNumericDisplay::RemoveEntityFromDigit( Digit_t *digit, EntityList_t *entity, EntityList_t *previous )
{
	if ( previous )
		previous->next = entity->next;
	else
		digit->entities = entity->next;

	delete entity;
}

void CTriggerNumericDisplay::Update( int value )
{
	unsigned int magnitude = value < 0 ? 0u - (unsigned int) value : (unsigned int) value;
	Digit_t *previousDigit = NULL;
	EntityList_t *previousEntity = NULL;
	Digit_t *digit = m_pDigits;

	while ( digit )
	{
		EntityList_t *entry = digit->entities;

		if ( entry )
		{
			while ( entry )
			{
				edict_t *actor = entry->entity.Get();
				CBaseEntity *entity = actor && !actor->free ? (CBaseEntity *) actor->pvPrivateData : NULL;

				if ( entity )
				{
					unsigned int divisor = (unsigned int) pow( 10.0, (double) ( digit->position - 1 ) );
					int digitValue = -1;

					if ( magnitude >= divisor ||
					     ( !( pev->spawnflags & SF_NUMERICDISPLAY_EMPTY_ZERO ) && (int) magnitude <= 0 && divisor == 1 ) )
						digitValue = (int) ( magnitude / divisor );

					if ( digitValue == -1 )
					{
						entity->pev->frame = ( pev->spawnflags & SF_NUMERICDISPLAY_ALTERNATE_EMPTY ) ? 1.0f : 0.0f;

						if ( divisor == 1 && !( pev->spawnflags & SF_NUMERICDISPLAY_EMPTY_ZERO ) )
							entity->pev->effects |= NUMERICDISPLAY_EFFECTS;
						else if ( pev->spawnflags & SF_NUMERICDISPLAY_ALTERNATE_EMPTY )
							entity->pev->effects &= ~NUMERICDISPLAY_EFFECTS;
					}
					else
					{
						entity->pev->frame = (float) ( digitValue % 10 );
						entity->pev->effects |= NUMERICDISPLAY_EFFECTS;
					}
				}
				else
				{
					RemoveEntityFromDigit( digit, entry, previousEntity );
					entry = previousEntity;
				}

				// Keep the previous entity when moving to the next digit
				previousEntity = entry;
				entry = entry ? entry->next : digit->entities;
			}
		}
		else
		{
			RemoveDigit( digit, previousDigit );
			digit = previousDigit;
		}

		previousDigit = digit;
		digit = digit ? digit->next : m_pDigits;
	}
}

void CTriggerNumericDisplay::UpdateThink( void )
{
	if ( pev->spawnflags & SF_NUMERICDISPLAY_USE_ONLY )
	{
		SetThink( NULL );
		return;
	}

	Update( GetUpdatedValue() );
	pev->nextthink = gpGlobals->time + pev->frags;
}

void CTriggerNumericDisplay::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	if ( pev->spawnflags & SF_NUMERICDISPLAY_USE_ONLY )
	{
		Update( GetUpdatedValue() );
		return;
	}

	if ( !ShouldToggle( useType, m_bEnabled ) )
		return;

	switch ( useType )
	{
		case USE_OFF:
			SetState( false );
			break;
		case USE_ON:
			SetState( true );
			break;
		case USE_TOGGLE:
			SetState( !m_bEnabled );
			break;
		default:
			break;
	}
}

void CTriggerNumericDisplay::FindDigits( void )
{
	Clear();

	if ( !pev->message )
		return;

	unsigned int digitCount = 0;
	Digit_t *previousDigit = NULL;

	for ( int value = INT_MAX; value; value /= 10 )
		digitCount++;

	for ( unsigned int position = 1; position <= digitCount; position++ )
	{
		char targetname[256];
		EntityList_t *entities = NULL;
		CBaseEntity *entity = NULL;

		UTIL_FormatString( targetname, sizeof( targetname ), "%s%u", STRING( pev->message ), position );

		while ( ( entity = UTIL_FindEntityByTargetname( entity, targetname ) ) != NULL )
		{
			if ( pev->spawnflags & SF_NUMERICDISPLAY_ALTERNATE_EMPTY )
				entity->pev->frame = 1.0f;
			else
				entity->pev->effects |= NUMERICDISPLAY_EFFECTS;

			EntityList_t *entry = new EntityList_t;

			entry->entity = EHANDLE();
			entry->entity = entity;
			entry->next = entities;
			entities = entry;
		}

		if ( !entities )
			continue;

		Digit_t *digit = new Digit_t;

		digit->next = NULL;
		digit->entities = entities;
		digit->position = position;

		if ( previousDigit )
			previousDigit->next = digit;
		else
			m_pDigits = digit;

		previousDigit = digit;
	}
}

void CTriggerNumericDisplay::Activate( void )
{
	FindDigits();
}

void CTriggerNumericDisplay::OnDestroy( void )
{
	Clear();
	CBaseEntity::OnDestroy();
}
