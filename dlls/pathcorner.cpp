/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// ========================== PATH_CORNER ===========================
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "trains.h"
#include "saverestore.h"

class CPathCorner : public CPointEntity
{
public:
	void Spawn( );
	void KeyValue( KeyValueData* pkvd );
	float GetDelay( void ) { return m_flWait; }
//	void Touch( CBaseEntity *pOther );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];

private:
	float	m_flWait;
};

LINK_ENTITY_TO_CLASS( path_corner, CPathCorner );

// Global Savedata for Delay
TYPEDESCRIPTION	CPathCorner::m_SaveData[] = 
{
	DEFINE_FIELD( CPathCorner, m_flWait, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CPathCorner, CPointEntity );

//
// Cache user-entity-field values until spawn is called.
//
void CPathCorner :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "wait"))
	{
		m_flWait = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else 
		CPointEntity::KeyValue( pkvd );
}


void CPathCorner :: Spawn( )
{
	if ( FStringNull( pev->targetname ) )
		ALERT( at_console, "path_corner: No targetname (X%d Y%d Z%d)\n", (int) pev->origin.x, (int) pev->origin.y, (int) pev->origin.z );
}

#if 0
void CPathCorner :: Touch( CBaseEntity *pOther )
{
	entvars_t*		pevToucher = pOther->pev;
		
	if ( FBitSet ( pevToucher->flags, FL_MONSTER ) )
	{// monsters don't navigate path corners based on touch anymore
		return;
	}

	// If OTHER isn't explicitly looking for this path_corner, bail out
	if ( pOther->m_hGoalEnt != this )
	{
		return;
	}

	// If OTHER has an enemy, this touch is incidental, ignore
	if ( !FNullEnt(pevToucher->enemy) )
	{
		return;		// fighting, not following a path
	}
	
	// UNDONE: support non-zero flWait
	/*
	if (m_flWait != 0)
		ALERT(at_warning, "Non-zero path-cornder waits NYI");
	*/

	// Find the next "stop" on the path, make it the goal of the "toucher".
	if (FStringNull(pev->target))
	{
		ALERT(at_warning, "PathCornerTouch: no next stop specified");
	}

	pOther->m_hGoalEnt = CBaseEntity::Instance( FIND_ENTITY_BY_TARGETNAME ( NULL, STRING(pev->target) ) );

	// If "next spot" was not found (does not exist - level design error)
	if ( !pOther->m_hGoalEnt )
	{
		ALERT(at_console, "PathCornerTouch--%s couldn't find next stop in path: %s", STRING(pev->classname), STRING(pev->target));
		return;
	}

	// Turn towards the next stop in the path.
	pevToucher->ideal_yaw = UTIL_VecToYaw ( pOther->m_hGoalEnt->pev->origin - pevToucher->origin );
}
#endif

TYPEDESCRIPTION CPathTrack::m_SaveData[] =
{
	DEFINE_FIELD( CPathTrack, m_length, FIELD_FLOAT ),
	DEFINE_FIELD( CPathTrack, m_hNext, FIELD_EHANDLE ),
	DEFINE_FIELD( CPathTrack, m_hAltpath, FIELD_EHANDLE ),
	DEFINE_FIELD( CPathTrack, m_hPrevious, FIELD_EHANDLE ),
	DEFINE_FIELD( CPathTrack, m_altName, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CPathTrack, CBaseEntity );
LINK_ENTITY_TO_CLASS( path_track, CPathTrack );

void CPathTrack::OnCreate()
{
	CPointEntity::OnCreate();
	m_flNewSpeed = -1.0f;
	m_flMaxSpeed = -1.0f;
}

//
// Cache user-entity-field values until spawn is called.
//
void CPathTrack :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "altpath"))
	{
		m_altName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "newspeed" ) )
	{
		m_flNewSpeed = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "maxspeed" ) )
	{
		m_flMaxSpeed = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CPathTrack :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int on;

	// Use toggles between two paths
	if ( m_hAltpath )
	{
		on = !FBitSet( pev->spawnflags, SF_PATH_ALTERNATE );
		if ( ShouldToggle( useType, on ) )
		{
			if ( on )
				SetBits( pev->spawnflags, SF_PATH_ALTERNATE );
			else
				ClearBits( pev->spawnflags, SF_PATH_ALTERNATE );
		}
	}
	else	// Use toggles between enabled/disabled
	{
		on = !FBitSet( pev->spawnflags, SF_PATH_DISABLED );

		if ( ShouldToggle( useType, on ) )
		{
			if ( on )
				SetBits( pev->spawnflags, SF_PATH_DISABLED );
			else
				ClearBits( pev->spawnflags, SF_PATH_DISABLED );
		}
	}
}


void CPathTrack :: Link( void  )
{
	edict_t *pentTarget;

	if ( !FStringNull(pev->target) )
	{
		pentTarget = FIND_ENTITY_BY_TARGETNAME( NULL, STRING(pev->target) );
		if ( pentTarget && !pentTarget->free && !FNullEnt(pentTarget) )
		{
			m_hNext = CPathTrack::Instance( pentTarget );

			if ( m_hNext ) // If no next pointer, this is the end of a path
			{
				m_hNext->SetPrevious( this );
			}
		}
		else
			ALERT( at_console, "Dead end link %s\n", STRING(pev->target) );
	}

	// Find "alternate" path
	if ( m_altName )
	{
		pentTarget = FIND_ENTITY_BY_TARGETNAME( NULL, STRING(m_altName) );
		if ( pentTarget && !pentTarget->free && !FNullEnt(pentTarget) )
		{
			m_hAltpath = CPathTrack::Instance( pentTarget );

			if ( m_hAltpath ) // If no next pointer, this is the end of a path
			{
				m_hAltpath->SetPrevious( this );
			}
		}
	}
}


void CPathTrack :: Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));

	m_hNext = NULL;
	m_hPrevious = NULL;
	m_bVisited = false;
// DEBUGGING CODE
#if PATH_SPARKLE_DEBUG
	SetThink( Sparkle );
	pev->nextthink = gpGlobals->time + 0.5;
#endif
}


void CPathTrack::Activate( void )
{
	if ( !FStringNull( pev->targetname ) )		// Link to next, and back-link
		Link();
}

CPathTrack	*CPathTrack :: ValidPath( CPathTrack	*ppath, int testFlag )
{
	if ( !ppath )
		return NULL;

	if ( testFlag && FBitSet( ppath->pev->spawnflags, SF_PATH_DISABLED ) )
		return NULL;

	return ppath;
}


void CPathTrack :: Project( CPathTrack *pstart, CPathTrack *pend, Vector *origin, float dist )
{
	if ( pstart && pend )
	{
		Vector dir = (pend->pev->origin - pstart->pev->origin);
		dir = dir.Normalize();
		*origin = pend->pev->origin + dir * dist;
	}
}

CPathTrack *CPathTrack::GetNext( void )
{
	if ( m_hAltpath && FBitSet( pev->spawnflags, SF_PATH_ALTERNATE ) && !FBitSet( pev->spawnflags, SF_PATH_ALTREVERSE ) )
		return m_hAltpath;

	return m_hNext;
}



CPathTrack *CPathTrack::GetPrevious( void )
{
	if ( m_hAltpath && FBitSet( pev->spawnflags, SF_PATH_ALTERNATE ) && FBitSet( pev->spawnflags, SF_PATH_ALTREVERSE ) )
		return m_hAltpath;

	return m_hPrevious;
}



void CPathTrack::SetPrevious( CPathTrack *pprev )
{
	// Only set previous if this isn't my alternate path
	if ( pprev && !FStrEq( STRING(pprev->pev->targetname), STRING(m_altName) ) )
		m_hPrevious = pprev;
}


// Assumes this is ALWAYS enabled
CPathTrack *CPathTrack :: LookAhead( Vector *origin, float dist, int move )
{
	CPathTrack *pcurrent;
	float originalDist = dist;
	
	pcurrent = this;
	Vector currentPos = *origin;

	if ( dist < 0 )		// Travelling backwards through path
	{
		dist = -dist;
		while ( dist > 0 )
		{
			Vector dir = pcurrent->pev->origin - currentPos;
			float length = dir.Length();
			if ( !length )
			{
				if ( !ValidPath(pcurrent->GetPrevious(), move) ) 	// If there is no previous node, or it's disabled, return now.
				{
					if ( !move )
						Project( pcurrent->GetNext(), pcurrent, origin, dist );
					return NULL;
				}
				pcurrent = pcurrent->GetPrevious();
			}
			else if ( length > dist )	// enough left in this path to move
			{
				*origin = currentPos + (dir * (dist / length));
				return pcurrent;
			}
			else
			{
				dist -= length;
				currentPos = pcurrent->pev->origin;
				*origin = currentPos;
				if ( !ValidPath(pcurrent->GetPrevious(), move) )	// If there is no previous node, or it's disabled, return now.
					return NULL;

				pcurrent = pcurrent->GetPrevious();
			}
		}
		*origin = currentPos;
		return pcurrent;
	}
	else 
	{
		while ( dist > 0 )
		{
			if ( !ValidPath(pcurrent->GetNext(), move) )	// If there is no next node, or it's disabled, return now.
			{
				if ( !move )
					Project( pcurrent->GetPrevious(), pcurrent, origin, dist );
				return NULL;
			}
			Vector dir = pcurrent->GetNext()->pev->origin - currentPos;
			float length = dir.Length();
			if ( !length  && !ValidPath( pcurrent->GetNext()->GetNext(), move ) )
			{
				if ( dist == originalDist ) // HACK -- up against a dead end
					return NULL;
				return pcurrent;
			}
			if ( length > dist )	// enough left in this path to move
			{
				*origin = currentPos + (dir * (dist / length));
				return pcurrent;
			}
			else
			{
				dist -= length;
				currentPos = pcurrent->GetNext()->pev->origin;
				pcurrent = pcurrent->GetNext();
				*origin = currentPos;
			}
		}
		*origin = currentPos;
	}

	return pcurrent;
}

	
// Assumes this is ALWAYS enabled
CPathTrack *CPathTrack :: Nearest( Vector origin )
{
	Vector delta = origin - pev->origin;
	delta.z = 0;

	float minDist = delta.Length();
	CPathTrack *nearest = this;
	CPathTrack *ppath = GetNext();
	struct VisitedNode
	{
		VisitedNode               *next;
		CEntityHandle<CPathTrack>  path;
	};

	VisitedNode *head = new VisitedNode();
	VisitedNode *tail = head;
	int count = 0;

	head->path = this;
	m_bVisited = true;
	while ( ppath && ppath != this && !ppath->m_bVisited )
	{
		VisitedNode *entry = new VisitedNode();

		tail->next = entry;
		tail = entry;
		entry->path = ppath;
		ppath->m_bVisited = true;
		if ( ++count > 9999 )
		{
			ALERT( at_error, "Bad sequence of path_tracks from %s", STRING(pev->targetname) );
			break;
		}
		delta = origin - ppath->pev->origin;
		delta.z = 0;

		float distance = delta.Length();
		if ( distance < minDist )
		{
			minDist = distance;
			nearest = ppath;
		}
		ppath = ppath->GetNext();
	}

	while ( head )
	{
		VisitedNode *next = head->next;
		CPathTrack *entry = head->path;

		if ( entry )
			entry->m_bVisited = false;

		delete head;
		head = next;
	}

	return nearest;
}

int CPathTrack::GetGoalDirection( int goalName, CBaseEntity *train )
{
	if ( !train )
		return 2;

	float distanceToCurrent = ( train->pev->origin - pev->origin ).Length();
	float forwardDistance = 0;
	float backwardDistance = 0;
	bool forwardFailed = true;

	if ( FStrEq( STRING( pev->targetname ), STRING( goalName ) ) )
		return 1;

	CPathTrack *next = GetNext();

	if ( next )
	{
		CPathTrack *path = distanceToCurrent > ( train->pev->origin - next->pev->origin ).Length() ? next : this;
		int count = 0;

		while ( path && ( path != this || count == 0 ) )
		{
			next = path->GetNext();

			if ( next )
				forwardDistance += ( path->pev->origin - next->pev->origin ).Length();

			if ( FStrEq( STRING( path->pev->targetname ), STRING( goalName ) ) )
			{
				forwardFailed = false;
				break;
			}

			if ( !next )
			{
				forwardDistance = 999999.0f;
				break;
			}

			path = next;
			++count;
		}

		if ( count != 0 && path == this )
			return 2;
	}
	else
		forwardDistance = 999999.0f;

	CPathTrack *previous = GetPrevious();

	if ( !previous )
		return forwardFailed ? 2 : 0;

	CPathTrack *path = distanceToCurrent > ( train->pev->origin - previous->pev->origin ).Length() ? previous : this;
	int count = 0;

	while ( path && ( path != this || count == 0 ) )
	{
		previous = path->GetPrevious();

		if ( !previous )
		{
			backwardDistance = 999999.0f;
			break;
		}

		backwardDistance += ( path->pev->origin - previous->pev->origin ).Length();

		if ( FStrEq( STRING( path->pev->targetname ), STRING( goalName ) ) )
			break;

		path = previous;
		++count;
	}

	return forwardDistance > backwardDistance;
}

CPathTrack *CPathTrack::Instance( edict_t *pent )
{ 
	if ( FClassnameIs( pent, "path_track" ) )
		return (CPathTrack *)GET_PRIVATE(pent);
	return NULL;
}


	// DEBUGGING CODE
#if PATH_SPARKLE_DEBUG
void CPathTrack :: Sparkle( void )
{

	pev->nextthink = gpGlobals->time + 0.2;
	if ( FBitSet( pev->spawnflags, SF_PATH_DISABLED ) )
		UTIL_ParticleEffect(pev->origin, Vector(0,0,100), 210, 10);
	else
		UTIL_ParticleEffect(pev->origin, Vector(0,0,100), 84, 10);
}
#endif

