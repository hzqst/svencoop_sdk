#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "func_tank.h"

CBaseEntity *MonsterTankHandle( EHANDLE &handle )
{
	edict_t *entity = handle.Get();

	if ( !entity || entity->free || !entity->pvPrivateData )
		return NULL;

	return (CBaseEntity *) entity->pvPrivateData;
}

void CBaseMonster::CheckSelectedTank( void )
{
	if ( m_MonsterState != MONSTERSTATE_COMBAT && !HasConditions( bits_COND_NEW_ENEMY ) )
		return;

	if ( IsMoving() || !MonsterTankHandle( m_hSelectedTank ) )
		return;

	if ( m_pSchedule && m_pSchedule == GetScheduleOfType( SCHED_MOVE_TO_TANK ) )
		return;

	if ( HasConditions( bits_COND_CAN_USE_TANK ) )
		ChangeSchedule( GetScheduleOfType( SCHED_MOVE_TO_TANK ) );
}

bool CBaseMonster::CheckTankUsage( void )
{
	if ( m_pSchedule && m_pSchedule == GetScheduleOfType( SCHED_MOVE_TO_TANK ) )
		return false;

	if ( !( gpGlobals->time >= m_flNextTankSearchTime ) && !HasConditions( bits_COND_NEW_ENEMY ) )
		return false;

	CFuncTank *tank = FindTank();

	m_hSelectedTank = NULL;

	if ( !tank || !tank->pev )
		return false;

	edict_t *entity = tank->edict();

	if ( !entity || entity->free )
		return false;

	m_hSelectedTank = tank;

	if ( !MonsterTankHandle( m_hSelectedTank ) )
		return false;

	m_flNextTankSearchTime = (float) ( (double) gpGlobals->time + 10.0 );
	return true;
}

CFuncTank *CBaseMonster::FindTank( void )
{
	CBaseEntity *entity = NULL;
	CFuncTank *best = NULL;
	float bestDistance = 65536.0f;

	while ( ( entity = UTIL_FindEntityByClassname( entity, "func_tank" ) ) != NULL )
	{
		CFuncTank *tank = dynamic_cast<CFuncTank *>( entity );

		if ( !tank )
			continue;

		float distance = ( tank->Center() - pev->origin ).Length();

		if ( distance > 768.0f && !FVisible( tank, true ) )
			continue;

		if ( best && distance >= bestDistance )
			continue;

		if ( !( tank->pev->spawnflags & SF_TANK_CANCONTROL ) )
			continue;

		if ( MonsterTankHandle( tank->m_hMonsterController ) || MonsterTankHandle( tank->m_hController ) )
			continue;

		if ( tank->pev->spawnflags & SF_TANK_ACTIVE )
			continue;

		CBaseEntity *enemy = MonsterTankHandle( m_hEnemy.m_Handle );

		if ( !enemy )
			return tank;

		if ( !( ( enemy->pev->origin - pev->origin ).Length() > distance ) )
			continue;

		UTIL_MakeVectors( tank->pev->v_angle );

		Vector2D direction = ( enemy->pev->origin - tank->pev->origin ).Make2D().Normalize();

		if ( !( 1.0f - tank->m_yawRange / 100.0f <= DotProduct( direction, gpGlobals->v_forward.Make2D() ) ) )
			continue;

		best = tank;
		bestDistance = ( tank->Center() - pev->origin ).Length();
	}

	return best;
}
