#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "trains.h"
#include "trigger_trackgoal.h"

#define SF_TRACKGOAL_ONLY_STOPPED ( 1 << 0 )
#define SF_TRACKGOAL_KEEP_DIRECTION_ON_FAILURE ( 1 << 1 )

LINK_ENTITY_TO_CLASS( trigger_track_goal, CTriggerTrackGoal );

void CTriggerTrackGoal::Spawn()
{
	Precache();
}

void CTriggerTrackGoal::Precache()
{
	if ( m_iszSoundNoPath )
		PrecacheSound_Intercept( this, STRING( m_iszSoundNoPath ) );
}

void CTriggerTrackGoal::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "path_name" ) )
	{
		m_iszPathName = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszSoundNoPath" ) )
	{
		m_iszSoundNoPath = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CTriggerTrackGoal::Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value )
{
	CBaseEntity *entity = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );

	if ( !entity )
		return;

	if ( !FClassnameIs( entity->pev, "func_tracktrain" ) )
	{
		ALERT( at_error, "trigger_track_goal: target must be a func_tracktrain!\n" );
		return;
	}

	CFuncTrackTrain *train = (CFuncTrackTrain *) entity;

	if ( ( pev->spawnflags & SF_TRACKGOAL_ONLY_STOPPED ) && train->pev->speed != 0 )
		return;

	train->m_iszGoal = m_iszPathName;

	int direction = train->m_hPath->GetGoalDirection( m_iszPathName, train );

	switch ( direction )
	{
		case 0:
			train->pev->speed = train->m_flMaxSpeed;
			break;

		case 2:
			ALERT( at_console, "trigger_track_goal: train '%s' cannot reach goal '%s'!\n",
			       STRING( pev->target ), STRING( m_iszPathName ) );

			if ( m_iszSoundNoPath )
				EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING( m_iszSoundNoPath ), 1.0f, ATTN_NORM, 0, PITCH_NORM,
				                0, false, g_vecZero );

			if ( pev->spawnflags & SF_TRACKGOAL_KEEP_DIRECTION_ON_FAILURE )
				break;

			// Fall through
		case 1:
			train->pev->speed = -train->m_flMaxSpeed;
			break;
	}

	train->Next();
	ALERT( at_aiconsole, "TRAIN(%s), speed to %.2f\n", STRING( pev->targetname ), train->m_flMaxSpeed );
}
