#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "animation.h"
#include "robotgrunt.h"
#include "weapons.h"
#include "effects.h"
#include "soundent.h"
#include "talkmonster.h"
#include "server_sound.h"
#include "player.h"

extern cvar_t sk_hgrunt_health;
extern cvar_t sk_hgrunt_kick;
extern cvar_t sk_hgrunt_pellets;
extern cvar_t sk_hgrunt_gspeed;
extern cvar_t sk_plr_rpg;
extern cvar_t sk_rgrunt_explode;
extern cvar_t sk_grunt_handgrenades;

namespace
{
#define HGRUNT_AE_RELOAD		2
#define HGRUNT_AE_KICK			3
#define HGRUNT_AE_BURST1		4
#define HGRUNT_AE_BURST2		5
#define HGRUNT_AE_BURST3		6
#define HGRUNT_AE_GREN_TOSS		7
#define HGRUNT_AE_GREN_LAUNCH	8
#define HGRUNT_AE_GREN_DROP		9
#define HGRUNT_AE_CAUGHT_ENEMY	10
#define HGRUNT_AE_DROP_GUN		11

#define GUN_GROUP				2
#define GUN_MP5					0
#define GUN_SHOTGUN				1
#define GUN_NONE				2
#define GUN_RPG					3
#define GUN_M16					4
#define GUN_SNIPER				5

#define bits_MOVESHOOTCAP_RPG	( 1 << 4 )

enum
{
	RGRUNT_MP5 = 1,
	RGRUNT_HANDGRENADE = 2,
	RGRUNT_M16 = 4,
	RGRUNT_SHOTGUN = 8,
	RGRUNT_RPG = 64,
	RGRUNT_SNIPER = 128
};

enum
{
	TASK_RGRUNT_FACE_TOSS_DIR = LAST_COMMON_TASK + 1,
	TASK_RGRUNT_SPEAK_SENTENCE,
	TASK_RGRUNT_CHECK_FIRE,
	TASK_RGRUNT_FACE_ROCKET,
	TASK_RGRUNT_WAIT_FACE_ENEMY
};

enum
{
	SCHED_RGRUNT_SUPPRESS = LAST_COMMON_SCHEDULE + 1,
	SCHED_RGRUNT_ESTABLISH_LINE_OF_FIRE,
	SCHED_RGRUNT_HIDE_RELOAD,
	SCHED_RGRUNT_SWEEP,
	SCHED_RGRUNT_FOUND_ENEMY,
	SCHED_RGRUNT_REPEL,
	SCHED_RGRUNT_REPEL_ATTACK,
	SCHED_RGRUNT_REPEL_LAND,
	SCHED_RGRUNT_WAIT_IN_COVER,
	SCHED_RGRUNT_TAKE_COVER_FAILED,
	SCHED_RGRUNT_ESTABLISH_LINE_OF_FIRE_FAILED
};

int g_fRobotGruntQuestion;

const char *g_RobotGruntSentences[] = {
	"RB_GREN", "RB_ALERT", "RB_MONSTER", "RB_COVER",
	"RB_THROW", "RB_CHARGE", "RB_TAUNT"
};

Vector RobotShellVelocity( void )
{
	Vector velocity = gpGlobals->v_right * RANDOM_FLOAT( 40, 90 );

	velocity = velocity + gpGlobals->v_up * RANDOM_FLOAT( 75, 200 );
	return velocity + gpGlobals->v_forward * RANDOM_FLOAT( -40, 40 );
}

bool CanRepairRobo( CBaseEntity *entity )
{
	if ( !entity || !entity->IsPlayer() )
		return false;

	CBasePlayerItem *item = ( (CBasePlayer *) entity )->m_pActiveItem;

	if ( !item )
		return false;

	ItemInfo info;

	memset( &info, 0, sizeof( info ) );
	item->GetItemInfo( &info );

	if ( info.iId != WEAPON_PIPEWRENCH )
		return false;

	CWrench *wrench = dynamic_cast<CWrench *>( item->GetWeaponPtr() );

	return wrench && wrench->m_iChargeBonus > 0;
}

void StartExplode( Vector origin, float unused, float delay, int amount )
{
	CBaseEntity *explosion = CBaseEntity::Create( "env_explosion", origin, g_vecZero, false );
	char magnitude[128];

	sprintf( magnitude, "%3d", amount );

	KeyValueData data;

	data.szClassName = NULL;
	data.szKeyName = "iMagnitude";
	data.szValue = magnitude;
	data.fHandled = FALSE;
	explosion->KeyValue( &data );
	explosion->Spawn();
	explosion->SetThink( &CBaseEntity::SUB_CallUseToggle );
	explosion->pev->nextthink = gpGlobals->time + delay;
}

Task_t tlRobotFail[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 2 }
};

Schedule_t slRobotFail[] = {
	{
		tlRobotFail,
		ARRAYSIZE( tlRobotFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK2 |
		bits_COND_NEW_ENEMY,
		0,
		"Grunt Fail"
	}
};

Task_t tlRobotCombatFail[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 2 }
};

Schedule_t slRobotCombatFail[] = {
	{
		tlRobotCombatFail,
		ARRAYSIZE( tlRobotCombatFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"Grunt Combat Fail"
	}
};

Task_t tlRobotVictoryDance[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_WAIT, 1.5f },
	{ TASK_GET_PATH_TO_ENEMY_CORPSE, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE }
};

Schedule_t slRobotVictoryDance[] = {
	{
		tlRobotVictoryDance,
		ARRAYSIZE( tlRobotVictoryDance ),
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntVictoryDance"
	}
};

Task_t tlRobotEstablishLineOfFire[] = {
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_RGRUNT_ESTABLISH_LINE_OF_FIRE_FAILED },
	{ TASK_GET_PATH_TO_ENEMY, 0 },
	{ TASK_RGRUNT_SPEAK_SENTENCE, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_AP, 0 }
};

Schedule_t slRobotEstablishLineOfFire[] = {
	{
		tlRobotEstablishLineOfFire,
		ARRAYSIZE( tlRobotEstablishLineOfFire ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK2 |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD,
		bits_SOUND_DANGER,
		"GruntEstablishLineOfFire"
	}
};

Task_t tlRobotFoundEnemy[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_SIGNAL1 }
};

Schedule_t slRobotFoundEnemy[] = {
	{
		tlRobotFoundEnemy,
		ARRAYSIZE( tlRobotFoundEnemy ),
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntFoundEnemy"
	}
};

Task_t tlRobotCombatFace[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_WAIT, 1.5f },
	{ TASK_SET_SCHEDULE, (float) SCHED_RGRUNT_SWEEP }
};

Schedule_t slRobotCombatFace[] = {
	{
		tlRobotCombatFace,
		ARRAYSIZE( tlRobotCombatFace ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD,
		0,
		"Combat Face"
	}
};

Task_t tlRobotSignalSuppress[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_SIGNAL2 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 }
};

Schedule_t slRobotSignalSuppress[] = {
	{
		tlRobotSignalSuppress,
		ARRAYSIZE( tlRobotSignalSuppress ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_SPECIAL1,
		bits_SOUND_DANGER,
		"SignalSuppress"
	}
};

Task_t tlRobotSuppress[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 }
};

Schedule_t slRobotSuppress[] = {
	{
		tlRobotSuppress,
		ARRAYSIZE( tlRobotSuppress ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_SPECIAL1,
		bits_SOUND_DANGER,
		"Suppress"
	}
};

Task_t tlRobotWaitInCover[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 1 }
};

Schedule_t slRobotWaitInCover[] = {
	{
		tlRobotWaitInCover,
		ARRAYSIZE( tlRobotWaitInCover ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK2 |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntWaitInCover"
	}
};

Task_t tlRobotTakeCover[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_RGRUNT_TAKE_COVER_FAILED },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RGRUNT_SPEAK_SENTENCE, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_SET_SCHEDULE, (float) SCHED_RGRUNT_WAIT_IN_COVER }
};

Schedule_t slRobotTakeCover[] = {
	{
		tlRobotTakeCover,
		ARRAYSIZE( tlRobotTakeCover ),
		0,
		0,
		"TakeCover"
	}
};

Task_t tlRobotGrenadeCover[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FIND_COVER_FROM_ENEMY, 30 },
	{ TASK_FIND_FAR_NODE_COVER_FROM_ENEMY, 384 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_SPECIAL_ATTACK1 },
	{ TASK_CLEAR_MOVE_WAIT, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_RGRUNT_WAIT_IN_COVER }
};

Schedule_t slRobotGrenadeCover[] = {
	{
		tlRobotGrenadeCover,
		ARRAYSIZE( tlRobotGrenadeCover ),
		0,
		0,
		"GrenadeCover"
	}
};

Task_t tlRobotTossGrenadeCover[] = {
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RANGE_ATTACK2, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_TAKE_COVER_FROM_ENEMY }
};

Schedule_t slRobotTossGrenadeCover[] = {
	{
		tlRobotTossGrenadeCover,
		ARRAYSIZE( tlRobotTossGrenadeCover ),
		0,
		0,
		"TossGrenadeCover"
	}
};

Task_t tlRobotTakeCoverFromBestSound[] = {
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_COWER },
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FIND_COVER_FROM_BEST_SOUND, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_GRENADE, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_TURN_LEFT, 179 }
};

Schedule_t slRobotTakeCoverFromBestSound[] = {
	{
		tlRobotTakeCoverFromBestSound,
		ARRAYSIZE( tlRobotTakeCoverFromBestSound ),
		0,
		bits_SOUND_DANGER,
		"GruntTakeCoverFromBestSound"
	}
};

Task_t tlRobotHideReload[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_RELOAD },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_RELOAD }
};

Schedule_t slRobotHideReload[] = {
	{
		tlRobotHideReload,
		ARRAYSIZE( tlRobotHideReload ),
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntHideReload"
	}
};

Task_t tlRobotSweep[] = {
	{ TASK_TURN_LEFT, 179 },
	{ TASK_WAIT, 1 },
	{ TASK_TURN_LEFT, 179 },
	{ TASK_WAIT, 1 }
};

Schedule_t slRobotSweep[] = {
	{
		tlRobotSweep,
		ARRAYSIZE( tlRobotSweep ),
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_WORLD |
		bits_SOUND_PLAYER |
		bits_SOUND_DANGER,
		"Grunt Sweep"
	}
};

Task_t tlRobotRangeAttack1A[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_WAIT_FACE_ENEMY, 0.1f },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 }
};

Schedule_t slRobotRangeAttack1A[] = {
	{
		tlRobotRangeAttack1A,
		ARRAYSIZE( tlRobotRangeAttack1A ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_SPECIAL1,
		bits_SOUND_DANGER,
		"Range Attack1A"
	}
};

Task_t tlRobotRangeAttack1B[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_WAIT_FACE_ENEMY, 0.1f },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 }
};

Schedule_t slRobotRangeAttack1B[] = {
	{
		tlRobotRangeAttack1B,
		ARRAYSIZE( tlRobotRangeAttack1B ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_SPECIAL1,
		bits_SOUND_DANGER,
		"Range Attack1B"
	}
};

Task_t tlRobotRangeAttack2[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_RGRUNT_FACE_TOSS_DIR, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_RANGE_ATTACK2 },
	{ TASK_SET_SCHEDULE, (float) SCHED_RGRUNT_WAIT_IN_COVER }
};

Schedule_t slRobotRangeAttack2[] = {
	{
		tlRobotRangeAttack2,
		ARRAYSIZE( tlRobotRangeAttack2 ),
		0,
		0,
		"RangeAttack2"
	}
};

Task_t tlRobotRepel[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_GLIDE }
};

Schedule_t slRobotRepel[] = {
	{
		tlRobotRepel,
		ARRAYSIZE( tlRobotRepel ),
		bits_COND_SEE_ENEMY |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_COMBAT |
		bits_SOUND_PLAYER |
		bits_SOUND_DANGER,
		"Repel"
	}
};

Task_t tlRobotRepelAttack[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_FLY }
};

Schedule_t slRobotRepelAttack[] = {
	{
		tlRobotRepelAttack,
		ARRAYSIZE( tlRobotRepelAttack ),
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD,
		0,
		"Repel Attack"
	}
};

Task_t tlRobotRepelLand[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_LAND },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_CLEAR_LASTPOSITION, 0 }
};

Schedule_t slRobotRepelLand[] = {
	{
		tlRobotRepelLand,
		ARRAYSIZE( tlRobotRepelLand ),
		bits_COND_SEE_ENEMY |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_COMBAT |
		bits_SOUND_PLAYER |
		bits_SOUND_DANGER,
		"Repel Land"
	}
};

Task_t tlRobotRangeAttackRPG[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_RGRUNT_FACE_ROCKET, 0 },
	{ TASK_WAIT_FACE_ENEMY, 1 }
};

Schedule_t slRobotRangeAttackRPG[] = {
	{
		tlRobotRangeAttackRPG,
		ARRAYSIZE( tlRobotRangeAttackRPG ),
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_ENEMY_DEAD,
		0,
		"Range Attack1Rocket"
	}
};

Task_t tlRobotRangeAttackSniper[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_RGRUNT_WAIT_FACE_ENEMY, 0.5f },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_RGRUNT_WAIT_IN_COVER }
};

Schedule_t slRobotRangeAttackSniper[] = {
	{
		tlRobotRangeAttackSniper,
		ARRAYSIZE( tlRobotRangeAttackSniper ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_SPECIAL1,
		bits_SOUND_DANGER,
		"Range Attack1C"
	}
};

Task_t tlRobotRangeAttackSniperCrouch[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_RGRUNT_WAIT_FACE_ENEMY, 0 },
	{ TASK_RGRUNT_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_CLEAR_LASTPOSITION, 0 },
	{ TASK_STORE_LASTPOSITION, 0 },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_CLEAR_LASTPOSITION, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_AP, 0 },
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_RGRUNT_WAIT_IN_COVER }
};

Schedule_t slRobotRangeAttackSniperCrouch[] = {
	{
		tlRobotRangeAttackSniperCrouch,
		ARRAYSIZE( tlRobotRangeAttackSniperCrouch ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_SPECIAL1,
		bits_SOUND_DANGER,
		"Range Attack1D"
	}
};
}

LINK_ENTITY_TO_CLASS( monster_robogrunt, CRobotGrunt );

Schedule_t *CRobotGrunt::m_scheduleList[] = {
	slRobotFail,
	slRobotCombatFail,
	slRobotVictoryDance,
	slRobotEstablishLineOfFire,
	slRobotFoundEnemy,
	slRobotCombatFace,
	slRobotSignalSuppress,
	slRobotSuppress,
	slRobotWaitInCover,
	slRobotTakeCover,
	slRobotGrenadeCover,
	slRobotTossGrenadeCover,
	slRobotTakeCoverFromBestSound,
	slRobotHideReload,
	slRobotSweep,
	slRobotRangeAttack1A,
	slRobotRangeAttack1B,
	slRobotRangeAttack2,
	slRobotRepel,
	slRobotRepelAttack,
	slRobotRepelLand
};

void CRobotGrunt::SetupFriendly( void )
{
	UTIL_SetModel( this, IsPlayerAlly() ? "models/rgruntf.mdl" : "models/rgrunt.mdl" );
	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
}

int CRobotGrunt::Classify( void )
{
	return GetClassification( CLASS_MACHINE );
}

int CRobotGrunt::ObjectCaps( void )
{
	return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

int CRobotGrunt::ISoundMask( void )
{
	return bits_SOUND_WORLD | bits_SOUND_COMBAT | bits_SOUND_PLAYER | bits_SOUND_DANGER;
}

int CRobotGrunt::IRelationship( CBaseEntity *target, bool ignoreNoTarget )
{
	if ( !target )
		return R_NO;

	if ( !( ( target->pev->flags & FL_MONSTER ) && target->m_fOverrideClass ) && !IsPlayerAlly()
		 && ( FClassnameIs( target->pev, "monster_alien_grunt" ) || FClassnameIs( target->pev, "monster_gargantua" ) ) )
		return R_NM;

	return CSquadMonster::IRelationship( target, ignoreNoTarget );
}

float CRobotGrunt::GetPointsForDamage( float damage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	float points = pev->max_health / sk_hgrunt_health.value;
	float fraction = damage / pev->max_health;

	points *= 8.0f;
	return points * fraction;
}

void CRobotGrunt::Precache( void )
{
	CSquadMonster::Precache();

	if ( !pev->model )
	{
		UTIL_PrecacheModel( this, IsPlayerAlly() ? "models/rgruntf.mdl" : "models/rgrunt.mdl" );
		m_voicePitch = IsPlayerAlly() ? 130 : 115;
	}

	const char *sounds[] = {
		"turret/tu_die.wav", "turret/tu_die2.wav",
		"weapons/hks1.wav", "weapons/hks2.wav", "weapons/hks3.wav",
		"weapons/m16_3round.wav", "hgrunt/gr_mgun1.wav", "weapons/sbarrel1.wav",
		"hgrunt/gr_reload1.wav", "weapons/glauncher.wav", "weapons/sbarrel1.wav",
		"zombie/claw_miss2.wav", "buttons/button3.wav", "buttons/button2.wav",
		"debris/beamstart14.wav"
	};

	for ( int i = 0; i < ARRAYSIZE( sounds ); i++ )
		PrecacheSound_Intercept( this, sounds[i] );

	m_iBrassShell = UTIL_PrecacheModel( this, "models/shell.mdl" );
	m_iShotgunShell = UTIL_PrecacheModel( this, "models/shotgunshell.mdl" );
	m_iChromeGibs = UTIL_PrecacheModel( this, "models/chromegibs.mdl" );
	m_iComputerGibs = UTIL_PrecacheModel( this, "models/computergibs.mdl" );
}

void CRobotGrunt::Spawn( void )
{
	Precache();

	if ( !SetupModel() )
		SetupFriendly();

	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );
	pev->takedamage = DAMAGE_AIM;
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = DONT_BLEED;
	pev->effects = 0;

	if ( pev->health == 0 )
		pev->health = sk_hgrunt_health.value;

	m_flFieldOfView = 0.2f;
	m_MonsterState = MONSTERSTATE_NONE;
	m_flNextGrenadeCheck = gpGlobals->time + 1;
	m_flNextPainTime = gpGlobals->time;
	m_iSentence = -1;
	m_flSparkTime = gpGlobals->time + 1;
	m_afCapability = bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;
	m_fEnemyEluded = false;
	m_fFirstEncounter = true;
	m_flNextGlow = gpGlobals->time + 1;
	m_HackedGunPos = Vector( 0, 0, 55 );
	m_fShockActive = false;
	m_flShockEndTime = gpGlobals->time;
	m_hRPGEntity = NULL;
	m_hRPGLaserSpot = NULL;
	m_flRPGSafetyRadius = sk_plr_rpg.value * 2.5;
	m_fHasM16 = LookupSequence( "standing_m16a2" ) != -1;

	if ( pev->weapons == 0 )
		pev->weapons = RGRUNT_MP5 | RGRUNT_HANDGRENADE;

	if ( pev->weapons & RGRUNT_SHOTGUN )
	{
		SetBodygroup( GUN_GROUP, GUN_SHOTGUN );
		m_cClipSize = 8;
	}
	else if ( pev->weapons & RGRUNT_RPG )
	{
		m_afMoveShootCap = bits_MOVESHOOTCAP_RPG;
		SetBodygroup( GUN_GROUP, GUN_RPG );
		m_cClipSize = 1;
	}
	else if ( pev->weapons & RGRUNT_SNIPER )
	{
		SetBodygroup( GUN_GROUP, GUN_SNIPER );
		m_cClipSize = 5;
	}
	else
	{
		if ( m_fHasM16 )
			SetBodygroup( GUN_GROUP, GUN_M16 );

		m_cClipSize = 30;
	}

	m_cAmmoLoaded = m_cClipSize;

	if ( pev->weapons & RGRUNT_M16 )
	{
		pev->weapons = RGRUNT_MP5 | RGRUNT_M16;
		SetBodygroup( GUN_GROUP, GUN_MP5 );
	}

	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( "Robo Grunt" );

	MonsterInit();

	if ( IsPlayerAlly() && !( pev->spawnflags & SF_MONSTER_PREDISASTER ) )
	{
		SetUse( &CBaseMonster::CallFollowerPlayerUse );
		m_fCanPlayerFollow = true;
	}
}

void CRobotGrunt::SetYawSpeed( void )
{
	switch ( m_Activity )
	{
		case ACT_TURN_LEFT:
		case ACT_TURN_RIGHT:
			pev->yaw_speed = 180;
			break;
		case ACT_FLY:
		case ACT_GLIDE:
			pev->yaw_speed = 200;
			break;
		default:
			pev->yaw_speed = 270;
			break;
	}
}

BOOL CRobotGrunt::FCanCheckAttacks( void )
{
	if ( !HasConditions( bits_COND_ENEMY_TOOFAR ) )
		return TRUE;

	return HasConditions( bits_COND_SEE_ENEMY ) && ( pev->weapons & ( RGRUNT_RPG | RGRUNT_SNIPER ) );
}

BOOL CRobotGrunt::CheckMeleeAttack1( float dot, float distance )
{
	CBaseEntity *enemy = m_hEnemy;
	CBaseMonster *monster = enemy ? enemy->MyMonsterPointer() : NULL;

	return monster && distance <= 64 && dot >= 0.7
		&& monster->Classify() != CLASS_ALIEN_BIOWEAPON && monster->Classify() != CLASS_PLAYER_BIOWEAPON;
}

void CRobotGrunt::CheckAmmo( void )
{
	if ( m_cAmmoLoaded <= 0 )
		SetConditions( bits_COND_NO_AMMO_LOADED );
}

Vector CRobotGrunt::GetGunPosition( void )
{
	if ( m_fStanding && !( pev->weapons & RGRUNT_RPG ) )
		return pev->origin + Vector( 0, 0, 60 );

	return pev->origin + Vector( 0, 0, ( pev->weapons & RGRUNT_SNIPER ) ? 64 : 48 );
}

void CRobotGrunt::PainSound( void )
{
}

void CRobotGrunt::UseSound( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "buttons/button3.wav", 1, ATTN_NORM, 0, 100 );
}

void CRobotGrunt::UnUseSound( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "buttons/button2.wav", 1, ATTN_NORM, 0, 100 );
}

BOOL CRobotGrunt::CheckRangeAttack1Rocket( float distance, CBaseEntity *enemy )
{
	if ( !enemy || m_flRPGSafetyRadius > distance )
		return FALSE;

	CBaseEntity *entity = NULL;

	while ( ( entity = UTIL_FindEntityInSphere( entity, m_vecEnemyLKP, m_flRPGSafetyRadius, NULL, "targetname" ) ) != NULL )
	{
		if ( ( entity->pev->flags & FL_MONSTER ) && IRelationship( entity ) <= R_NO && entity->IsAlive() && FVisible( entity, false ) )
			return FALSE;
	}

	TraceResult trace;
	Vector source = pev->origin + Vector( 0, 0, 48 );

	UTIL_TraceLine( source, enemy->Center(), missile, ignore_glass, edict(), &trace );

	return trace.flFraction == 1 || trace.pHit == enemy->edict();
}

BOOL CRobotGrunt::CheckRangeAttack1( float dot, float distance )
{
	if ( dot < 0.5f )
		return FALSE;

	CBaseEntity *enemy = m_hEnemy;

	if ( pev->weapons & RGRUNT_RPG )
		return CheckRangeAttack1Rocket( distance, enemy );

	if ( !( pev->weapons & RGRUNT_SNIPER ) && !( distance <= 2048 && NoFriendlyFire() ) )
		return FALSE;

	if ( !enemy->IsPlayer() && distance <= 64 )
		return FALSE;

	if ( RANDOM_LONG( 0, 9 ) == 0 && m_Activity != ACT_RANGE_ATTACK1 )
		m_fStanding = RANDOM_LONG( 0, 1 ) != 0;

	Vector standing = pev->origin + Vector( 0, 0, 60 );
	Vector crouching = pev->origin + Vector( 0, 0, 48 );
	Vector source = m_fStanding ? standing : crouching;
	TraceResult trace;

	UTIL_TraceLine( source, enemy->BodyTarget( source ), ignore_monsters, ignore_glass, edict(), &trace );

	if ( trace.flFraction == 1 || trace.pHit == enemy->edict() )
		return TRUE;

	source = m_fStanding ? crouching : standing;
	UTIL_TraceLine( source, enemy->BodyTarget( source ), ignore_monsters, ignore_glass, edict(), &trace );

	if ( trace.flFraction == 1 || trace.pHit == enemy->edict() )
	{
		m_fStanding = !m_fStanding;
		return TRUE;
	}

	return FALSE;
}

BOOL CRobotGrunt::CheckRangeAttack2( float dot, float distance )
{
	if ( !( pev->weapons & ( RGRUNT_HANDGRENADE | RGRUNT_M16 ) ) )
		return FALSE;

	if ( ( pev->weapons & ( RGRUNT_RPG | RGRUNT_SNIPER ) ) && HasConditions( bits_COND_SEE_ENEMY ) )
	{
		m_fThrowGrenade = false;
		return FALSE;
	}

	if ( m_flNextGrenadeCheck > gpGlobals->time )
		return m_fThrowGrenade;

	CBaseEntity *enemy = m_hEnemy;

	if ( !( enemy->pev->flags & FL_ONGROUND ) && enemy->pev->waterlevel == 0 && m_vecEnemyLKP.z > pev->absmax.z )
	{
		m_fThrowGrenade = false;
		return FALSE;
	}

	Vector target;

	if ( pev->weapons & RGRUNT_HANDGRENADE )
	{
		if ( RANDOM_LONG( 0, 1 ) )
		{
			Vector origin = enemy->IsBSPModel() ? enemy->Center() : enemy->pev->origin;

			target = Vector( origin.x, origin.y, enemy->pev->absmin.z );
		}
		else
			target = m_vecEnemyLKP;
	}
	else
	{
		bool brush = enemy->IsBSPModel() != FALSE;
		Vector bodyTarget = enemy->BodyTarget( pev->origin );
		Vector origin = brush ? enemy->Center() : enemy->pev->origin;

		target = m_vecEnemyLKP + ( bodyTarget - origin );

		if ( HasConditions( bits_COND_SEE_ENEMY ) )
			target = target + ( ( target - pev->origin ).Length() / sk_hgrunt_gspeed.value ) * enemy->pev->velocity;
	}

	if ( SquadMemberInRange( target, 256 ) || ( target - pev->origin ).Length2D() <= 256 )
	{
		m_fThrowGrenade = false;
		m_flNextGrenadeCheck = gpGlobals->time + 1;
		return FALSE;
	}

	Vector velocity;

	if ( pev->weapons & RGRUNT_HANDGRENADE )
		velocity = VecCheckToss( pev, GetGunPosition(), target, 1 );
	else
		velocity = VecCheckThrow( pev, GetGunPosition(), target, sk_hgrunt_gspeed.value, 0.7f );

	if ( velocity != g_vecZero )
	{
		m_vecTossVelocity = velocity;
		m_fThrowGrenade = true;
		m_flNextGrenadeCheck = gpGlobals->time + 0.3;
		return TRUE;
	}

	m_fThrowGrenade = false;
	m_flNextGrenadeCheck = gpGlobals->time + 1;
	return FALSE;
}

CBaseEntity *CRobotGrunt::BestVisibleEnemy( void )
{
	if ( !( pev->weapons & RGRUNT_RPG ) )
		return CSquadMonster::BestVisibleEnemy();

	CBaseEntity *best = NULL;
	int bestRelationship = R_NO;
	int bestDistance = 0x10000;

	for ( CBaseEntity *entity = m_pLink; entity; entity = entity->m_pLink )
	{
		if ( !entity->IsAlive() && !entity->IsBSPModel() )
			continue;

		if ( IRelationship( entity ) > bestRelationship || IRelationship( entity ) == R_FR )
		{
			bestRelationship = IRelationship( entity );
			bestDistance = (int) ( entity->Center() - Center() ).Length();
			best = entity;
		}
		else if ( IRelationship( entity ) == bestRelationship && IRelationship( entity ) > R_NO )
		{
			int distance = (int) ( entity->Center() - Center() ).Length();

			if ( distance < bestDistance || CheckRangeAttack1Rocket( (float) distance, entity ) )
			{
				bestDistance = distance;
				best = entity;
				bestRelationship = IRelationship( entity );
			}
		}
	}

	return best;
}

void CRobotGrunt::SetActivity( Activity activity )
{
	int sequence;

	switch ( activity )
	{
		case ACT_IDLE:
		case ACT_CROUCHIDLE:
		case ACT_IDLE_ANGRY:
			if ( pev->weapons & RGRUNT_RPG )
			{
				sequence = LookupSequence( "rpg_idle" );

				if ( m_MonsterState == MONSTERSTATE_COMBAT )
					sequence = LookupSequence( "rpg_aim" );
			}
			else if ( m_MonsterState == MONSTERSTATE_COMBAT )
			{
				activity = m_fStanding ? ACT_IDLE_ANGRY : ACT_CROUCHIDLE;

				if ( LookupActivity( activity ) == -1 )
					sequence = LookupSequence( "crouching_idle" );
				else
					sequence = LookupActivity( activity );
			}
			else
				sequence = LookupActivity( activity );
			break;
		case ACT_WALK:
			sequence = LookupActivity( pev->health <= 20 ? ACT_WALK_HURT : activity );
			break;
		case ACT_RUN:
			sequence = LookupActivity( ACT_RUN );
			break;
		case ACT_RANGE_ATTACK1:
			if ( pev->weapons & RGRUNT_MP5 )
			{
				sequence = -1;

				if ( pev->weapons & RGRUNT_M16 )
					sequence = LookupSequence( m_fStanding ? "standing_m16a2" : "crouching_m16a2" );

				if ( sequence == -1 )
					sequence = LookupSequence( m_fStanding ? "standing_mp5" : "crouching_mp5" );
			}
			else if ( pev->weapons & RGRUNT_RPG )
				sequence = LookupSequence( "rpg_shoot" );
			else if ( pev->weapons & RGRUNT_SNIPER )
				sequence = LookupSequence( m_fStanding ? "standing_m40a1" : "crouching_m40a1" );
			else
				sequence = LookupSequence( m_fStanding ? "standing_shotgun" : "crouching_shotgun" );
			break;
		case ACT_RANGE_ATTACK2:
			sequence = LookupSequence( ( pev->weapons & RGRUNT_HANDGRENADE ) ? "throwgrenade" : "launchgrenade" );
			break;
		case ACT_RELOAD:
			if ( pev->weapons & RGRUNT_RPG )
				sequence = LookupSequence( "rpg_reload" );
			else if ( pev->weapons & RGRUNT_SNIPER )
			{
				sequence = LookupSequence( "reload_m40a1" );
				EMIT_SOUND( edict(), CHAN_WEAPON, "weapons/sniper_reload_first_seq.wav", 1, ATTN_NORM );
			}
			else
				sequence = LookupActivity( activity );
			break;
		case ACT_SMALL_FLINCH:
			if ( pev->weapons & RGRUNT_RPG )
				sequence = LookupSequence( m_MonsterState == MONSTERSTATE_COMBAT ? "rpg_aim" : "rpg_idle" );
			else
				sequence = LookupActivity( activity );
			break;
		default:
			sequence = LookupActivity( activity );
			break;
	}

	m_Activity = activity;
	m_IdealActivity = activity;

	if ( sequence <= ACTIVITY_NOT_AVAILABLE )
	{
		ALERT( at_aiconsole, "%s has no sequence for act:%d\n", STRING( pev->classname ), activity );
		pev->sequence = 0;
		return;
	}

	if ( pev->sequence != sequence || !m_fSequenceLoops )
		pev->frame = 0;

	pev->sequence = sequence;
	ResetSequenceInfo();
	SetYawSpeed();
}

BOOL CRobotGrunt::FOkToSpeak( void )
{
	if ( !IsAlive() || CTalkMonster::g_talkWaitTime > gpGlobals->time )
		return FALSE;

	return !( pev->spawnflags & SF_MONSTER_GAG ) || m_MonsterState == MONSTERSTATE_COMBAT;
}

void CRobotGrunt::JustSpoke( void )
{
	CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT( 1.5f, 2 );
	m_iSentence = -1;
}

void CRobotGrunt::SpeakSentence( void )
{
	if ( m_iSentence == -1 || !FOkToSpeak() )
		return;

	SENTENCEG_PlayRndSz( edict(), g_RobotGruntSentences[m_iSentence], 1, ATTN_NORM, 0, m_voicePitch );
	JustSpoke();
}

void CRobotGrunt::IdleSound( void )
{
	if ( !FOkToSpeak() || ( !g_fRobotGruntQuestion && !RANDOM_LONG( 0, 1 ) ) )
		return;

	if ( !g_fRobotGruntQuestion )
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
			case 0:
				SENTENCEG_PlayRndSz( edict(), "RB_CHECK", 1, ATTN_NORM, 0, m_voicePitch );
				g_fRobotGruntQuestion = 1;
				break;
			case 1:
				SENTENCEG_PlayRndSz( edict(), "RB_QUEST", 1, ATTN_NORM, 0, m_voicePitch );
				g_fRobotGruntQuestion = 2;
				break;
			case 2:
				SENTENCEG_PlayRndSz( edict(), "RB_IDLE", 1, ATTN_NORM, 0, m_voicePitch );
				break;
		}
	}
	else
	{
		SENTENCEG_PlayRndSz( edict(), g_fRobotGruntQuestion == 1 ? "RB_CLEAR" : "RB_ANSWER", 1, ATTN_NORM, 0, m_voicePitch );
		g_fRobotGruntQuestion = 0;
	}

	JustSpoke();
}

void CRobotGrunt::Shoot( void )
{
	if ( !m_hEnemy )
		return;

	Vector source = GetGunPosition();
	Vector direction = ShootAtEnemy( source );

	UTIL_MakeVectors( pev->angles );
	EjectBrass( source - direction * -24, RobotShellVelocity(), pev->angles.y, m_iBrassShell, TE_BOUNCE_SHELL );
	pev->effects |= EF_MUZZLEFLASH;
	m_cAmmoLoaded--;
	SetBlending( 0, UTIL_VecToAngles( direction ).x );

	if ( pev->weapons & RGRUNT_M16 )
		FireBullets( 1, source, direction, VECTOR_CONE_8DEGREES, 2048, BULLET_MONSTER_SAW, 4, 0, NULL, FBDM_DRAW );
	else
	{
		FireBullets( 1, source, direction, VECTOR_CONE_10DEGREES, 2048, BULLET_MONSTER_MP5, 4, 0, NULL, FBDM_DRAW );

		if ( pev->movetype != MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
		{
			m_fSequenceFinished = TRUE;
			m_Activity = ACT_RESET;
		}
	}
}

void CRobotGrunt::Shotgun( void )
{
	if ( !m_hEnemy )
		return;

	Vector source = GetGunPosition();
	Vector direction = ShootAtEnemy( source );

	UTIL_MakeVectors( pev->angles );
	EjectBrass( source - direction * -24, RobotShellVelocity(), pev->angles.y, m_iShotgunShell, TE_BOUNCE_SHOTSHELL );
	FireBullets( (ULONG) sk_hgrunt_pellets.value, source, direction, VECTOR_CONE_15DEGREES,
	             2048, BULLET_MONSTER_BUCKSHOT, 0, 0, NULL, FBDM_DRAW );
	pev->effects |= EF_MUZZLEFLASH;
	m_cAmmoLoaded--;
	SetBlending( 0, UTIL_VecToAngles( direction ).x );
}

void CRobotGrunt::Rocket( void )
{
	if ( !m_hEnemy )
		return;

	Vector source = GetGunPosition();
	Vector angles = UTIL_VecToAngles( m_hEnemy->Center() - source );

	m_hRPGEntity = CRpgRocket::CreateRpgRocket( source, angles, this, NULL );
	m_cAmmoLoaded--;
	SetBlending( 0, angles.x );
}

void CRobotGrunt::Sniper( void )
{
	if ( !m_hEnemy )
		return;

	UTIL_MakeVectors( pev->angles );

	Vector source = GetGunPosition();
	Vector direction = ShootAtEnemy( source ) - m_hEnemy->pev->velocity / 15000;

	EjectBrass( source - direction * -24, RobotShellVelocity(), pev->angles.y, m_iBrassShell, TE_BOUNCE_SHELL );
	FireBullets( 1, source, direction, g_vecZero, 8192, BULLET_MONSTER_SNIPER, 0, 0, NULL, FBDM_DRAW );
	SetBlending( 0, UTIL_VecToAngles( direction ).x );
	pev->effects |= EF_MUZZLEFLASH;
	m_cAmmoLoaded--;
}

void CRobotGrunt::HandleAnimEvent( MonsterEvent_t *event )
{
	switch ( event->event )
	{
		case HGRUNT_AE_RELOAD:
			EMIT_SOUND( edict(), CHAN_WEAPON, "hgrunt/gr_reload1.wav", 1, ATTN_NORM );
			ClearConditions( bits_COND_NO_AMMO_LOADED );
			m_cAmmoLoaded = m_cClipSize;
			return;
		case HGRUNT_AE_KICK:
		{
			UTIL_MakeVectors( pev->angles );

			Vector source( pev->origin.x, pev->origin.y, pev->origin.z + pev->size.z * 0.5 );
			TraceResult trace;

			UTIL_TraceHull( source, source + gpGlobals->v_forward * 70,
			                dont_ignore_monsters, head_hull, edict(), &trace );

			CBaseEntity *target = CBaseEntity::Instance( trace.pHit );

			if ( !target )
				return;

			if ( ( target->pev->flags & FL_CLIENT
				   || ( target->pev->flags & FL_MONSTER && target->pev->movetype != MOVETYPE_FLY ) )
				 && !FClassnameIs( target->pev, "monster_alien_voltigore" )
				 && !FClassnameIs( target->pev, "monster_gargantua" ) && !FClassnameIs( target->pev, "monster_bigmomma" ) )
			{
				UTIL_MakeVectors( pev->angles );
				target->pev->punchangle.x = 45;
				target->pev->velocity = target->pev->velocity + gpGlobals->v_forward * 300 + gpGlobals->v_up * 50;
			}

			target->TakeDamage( pev, pev, sk_hgrunt_kick.value, DMG_CLUB );
			return;
		}
		case HGRUNT_AE_BURST1:
			if ( !NoFriendlyFire() && m_hEnemy && m_hEnemy->IsAlive() )
				return;

			if ( pev->weapons & RGRUNT_MP5 )
			{
				Shoot();

				if ( pev->weapons & RGRUNT_M16 )
					EMIT_SOUND( edict(), CHAN_AUTO, "weapons/m16_3round.wav", 1, ATTN_NORM );
				else
				{
					const char *sounds[] = { "weapons/hks1.wav", "weapons/hks2.wav", "weapons/hks3.wav" };

					EMIT_SOUND( edict(), CHAN_AUTO, sounds[RANDOM_LONG( 0, 2 )], 1, ATTN_NORM );
				}
			}
			else if ( pev->weapons & RGRUNT_RPG )
			{
				if ( pev->movetype != MOVETYPE_FLY )
					Rocket();
			}
			else if ( pev->weapons & RGRUNT_SNIPER )
			{
				if ( pev->movetype == MOVETYPE_FLY )
					break;

				Sniper();
				EMIT_SOUND( edict(), CHAN_WEAPON, "weapons/sniper_fire.wav", 1, 0.4f );
			}
			else
			{
				Shotgun();
				EMIT_SOUND( edict(), CHAN_AUTO, "weapons/sbarrel1.wav", 1, ATTN_NORM );
			}
			break;
		case HGRUNT_AE_BURST2:
		case HGRUNT_AE_BURST3:
			if ( ( pev->weapons & RGRUNT_M16 ) && ( NoFriendlyFire() || !m_hEnemy || !m_hEnemy->IsAlive() ) )
				Shoot();
			return;
		case HGRUNT_AE_GREN_TOSS:
			UTIL_MakeVectors( pev->angles );
			CGrenade::ShootTimed( pev, GetGunPosition(), m_vecTossVelocity, 3.5f );
			m_fThrowGrenade = false;
			m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 4, 9 );
			return;
		case HGRUNT_AE_GREN_LAUNCH:
			EMIT_SOUND( edict(), CHAN_WEAPON, "weapons/glauncher.wav", 0.8f, ATTN_NORM );
			CGrenade::ShootContact( pev, GetGunPosition(), m_vecTossVelocity );
			m_fThrowGrenade = false;
			m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 2, 8 );
			return;
		case HGRUNT_AE_GREN_DROP:
			UTIL_MakeVectors( pev->angles );
			CGrenade::ShootTimed( pev, pev->origin + gpGlobals->v_forward * 17 - gpGlobals->v_right * 27 + gpGlobals->v_up * 6,
			                      g_vecZero, 3 );
			return;
		case HGRUNT_AE_CAUGHT_ENEMY:
			if ( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz( edict(), "RB_ALERT", 1, ATTN_NORM, 0, m_voicePitch );
				JustSpoke();
			}

			CSquadMonster::HandleAnimEvent( event );
			return;
		case HGRUNT_AE_DROP_GUN:
		{
			Vector position;
			Vector angles;

			GetAttachment( 0, position, angles );
			SetBodygroup( GUN_GROUP, GUN_NONE );
			return;
		}
		default:
			CSquadMonster::HandleAnimEvent( event );
			return;
	}

	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 0.3f, this );
}

void CRobotGrunt::StartTask( Task_t *task )
{
	switch ( task->iTask )
	{
		case TASK_RUN_PATH:
		case TASK_WALK_PATH:
			Forget( bits_MEMORY_INCOVER );
			CSquadMonster::StartTask( task );
			break;
		case TASK_FACE_IDEAL:
		case TASK_FACE_ENEMY:
			CSquadMonster::StartTask( task );

			if ( pev->movetype == MOVETYPE_FLY )
				m_IdealActivity = ACT_GLIDE;

			break;
		case TASK_RANGE_ATTACK1:
			if ( ( pev->weapons & RGRUNT_RPG ) && !m_hRPGLaserSpot )
			{
				m_hRPGLaserSpot = CRPGSpot::CreateSpot();

				if ( m_hRPGLaserSpot )
				{
					m_hRPGLaserSpot->pev->owner = edict();
					m_hRPGLaserSpot->pev->movetype = MOVETYPE_FLY;

					UTIL_MakeVectors( UTIL_VecToAngles( m_vecEnemyLKP - Center() ) );

					Vector source = GetGunPosition();
					TraceResult trace;

					UTIL_TraceLine( source, source + gpGlobals->v_forward * 4096, dont_ignore_monsters, edict(), &trace );
					m_hRPGLaserSpot->pev->origin = trace.vecEndPos;
					m_hRPGLaserSpot->pev->velocity = g_vecZero;
					m_vecLaserAngles = pev->angles;
					UpdateSpot( true );
				}
			}

			CSquadMonster::StartTask( task );
			break;
		case TASK_RELOAD:
			m_IdealActivity = ACT_RELOAD;
			break;
		case TASK_FIND_ATTACKPOINT_TO_ENEMY:
		{
			CBaseEntity *enemy = m_hEnemy;

			if ( !enemy )
			{
				TaskFail();
				break;
			}

			BOOL found;

			if ( pev->weapons & RGRUNT_RPG )
				found = FindLateralAttackPoint( enemy->Center(), enemy->pev->view_ofs, m_flRPGSafetyRadius )
					|| FindAttackPoint( enemy->Center(), enemy->pev->view_ofs, m_flRPGSafetyRadius, 0 );
			else
				found = FindLateralAttackPoint( enemy->Center(), enemy->pev->view_ofs )
					|| FindAttackPoint( enemy->Center(), enemy->pev->view_ofs, 0, CoverRadius() );

			if ( found )
			{
				m_flMoveWaitFinished = gpGlobals->time + task->flData;
				TaskComplete();
			}
			else
				TaskFail();
			break;
		}
		case TASK_RGRUNT_FACE_TOSS_DIR:
			break;
		case TASK_RGRUNT_SPEAK_SENTENCE:
			SpeakSentence();
			TaskComplete();
			break;
		case TASK_RGRUNT_CHECK_FIRE:
		{
			TaskComplete();

			bool clear = NoFriendlyFire();

			if ( pev->weapons & RGRUNT_RPG )
			{
				CBaseEntity *entity = NULL;

				while ( ( entity = UTIL_FindEntityInSphere( entity, m_hEnemy->pev->origin, m_flRPGSafetyRadius, NULL, "targetname" ) ) != NULL )
				{
					if ( ( entity->pev->flags & FL_MONSTER ) && IRelationship( entity ) <= R_NO && entity->IsAlive() && FVisible( entity, true ) )
					{
						clear = false;
						break;
					}
				}
			}

			if ( !clear )
			{
				ChangeSchedule( GetScheduleOfType( SCHED_FIND_ATTACK_POINT ) );
				SetConditions( bits_COND_SPECIAL1 );
			}
			break;
		}
		case TASK_RGRUNT_FACE_ROCKET:
			m_IdealActivity = ACT_SMALL_FLINCH;
			UpdateSpot( false );
			break;
		case TASK_RGRUNT_WAIT_FACE_ENEMY:
			m_IdealActivity = m_fStanding ? ACT_IDLE_ANGRY : ACT_CROUCHIDLE;
			m_flWaitFinished = gpGlobals->time + task->flData;

			if ( ( pev->weapons & RGRUNT_SNIPER ) && m_hEnemy )
			{
				float distance = ( m_hEnemy->Center() - Center() ).Length();

				m_flWaitFinished = gpGlobals->time;

				if ( m_hEnemy->IsMoving() )
					m_flWaitFinished += 0.3f;

				if ( distance < 384 )
					m_flWaitFinished += 0.5f;
				else if ( distance < 1024 )
					m_flWaitFinished += 0.8f;
				else
					m_flWaitFinished += 1.5f;
			}
			break;
		default:
			CSquadMonster::StartTask( task );
			break;
	}
}

void CRobotGrunt::RunTask( Task_t *task )
{
	switch ( task->iTask )
	{
		case TASK_WAIT_FOR_MOVEMENT_AP:
			if ( m_movementGoal == MOVEGOAL_NONE )
			{
				TaskComplete();
				RouteClear();
			}

			if ( ( !( pev->weapons & RGRUNT_RPG ) || !HasConditions( bits_COND_CAN_MELEE_ATTACK1 )
				   || HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
				 && m_hEnemy && NoFriendlyFire() && HasConditions( bits_COND_CAN_ATTACK ) )
			{
				TaskComplete();
				RouteClear();
			}
			break;
		case TASK_RGRUNT_FACE_TOSS_DIR:
			MakeIdealYaw( pev->origin + m_vecTossVelocity * 64 );
			ChangeYaw( (int) pev->yaw_speed );

			if ( FacingIdeal() )
				TaskComplete();
			break;
		case TASK_RGRUNT_FACE_ROCKET:
			if ( !m_hRPGEntity || !m_hEnemy || ( m_hRPGEntity->pev->effects & EF_NODRAW ) )
			{
				KillLaserSpot();
				TaskComplete();
				break;
			}

			UpdateSpot( false );
			MakeIdealYaw( m_vecEnemyLKP );
			ChangeYaw( (int) pev->yaw_speed );
			break;
		case TASK_RGRUNT_WAIT_FACE_ENEMY:
			MakeIdealYaw( m_vecEnemyLKP );
			ChangeYaw( (int) pev->yaw_speed );

			if ( gpGlobals->time >= m_flWaitFinished )
				TaskComplete();
			break;
		default:
			CSquadMonster::RunTask( task );
			break;
	}
}

Schedule_t *CRobotGrunt::ScheduleFromName( const char *name )
{
	Schedule_t *schedule = ScheduleInList( name, m_scheduleList, ARRAYSIZE( m_scheduleList ) );

	return schedule ? schedule : CBaseMonster::ScheduleFromName( name );
}

Schedule_t *CRobotGrunt::GetScheduleOfType( int type )
{
	switch ( type )
	{
		case SCHED_COMBAT_FACE:
			return slRobotCombatFace;
		case SCHED_VICTORY_DANCE:
			return InSquad() && !IsLeader() ? slRobotFail : slRobotVictoryDance;
		case SCHED_TAKE_COVER_FROM_ENEMY:
			if ( InSquad() )
			{
				if ( sk_grunt_handgrenades.value >= 1 && HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
				{
					if ( FOkToSpeak() )
					{
						SENTENCEG_PlayRndSz( edict(), "RB_THROW", 1, ATTN_NORM, 0, m_voicePitch );
						JustSpoke();
					}

					return slRobotTossGrenadeCover;
				}

				return slRobotTakeCover;
			}

			if ( RANDOM_LONG( 0, 1 ) || !m_hEnemy || m_hEnemy->pev->movetype == MOVETYPE_FLY || HasConditions( bits_COND_ENEMY_OCCLUDED ) )
				return slRobotTakeCover;

			if ( ( m_hEnemy->Center() - pev->origin ).Length2D() >= 512 || SquadMemberInRange( pev->origin, 256 ) )
				return slRobotTakeCover;

			return slRobotGrenadeCover;
		case SCHED_TAKE_COVER_FROM_BEST_SOUND:
			return slRobotTakeCoverFromBestSound;
		case SCHED_RANGE_ATTACK1:
			if ( pev->weapons & RGRUNT_RPG )
				return slRobotRangeAttackRPG;

			if ( pev->weapons & RGRUNT_SNIPER )
				return RANDOM_LONG( 0, 9 ) > 0 ? slRobotRangeAttackSniper : slRobotRangeAttackSniperCrouch;

			return m_fStanding ? slRobotRangeAttack1B : slRobotRangeAttack1A;
		case SCHED_RANGE_ATTACK2:
			return slRobotRangeAttack2;
		case SCHED_FAIL:
			return m_hEnemy ? slRobotCombatFail : slRobotFail;
		case SCHED_RGRUNT_SUPPRESS:
			if ( m_hEnemy && m_hEnemy->IsPlayer() && m_fFirstEncounter )
			{
				m_fFirstEncounter = false;
				return ( pev->weapons & ( RGRUNT_RPG | RGRUNT_SNIPER ) ) ? slRobotFoundEnemy : slRobotSignalSuppress;
			}

			return ( pev->weapons & ( RGRUNT_RPG | RGRUNT_SNIPER ) ) ? GetScheduleOfType( SCHED_RANGE_ATTACK1 ) : slRobotSuppress;
		case SCHED_RGRUNT_ESTABLISH_LINE_OF_FIRE:
			return slRobotEstablishLineOfFire;
		case SCHED_RGRUNT_HIDE_RELOAD:
			return slRobotHideReload;
		case SCHED_RGRUNT_SWEEP:
			return slRobotSweep;
		case SCHED_RGRUNT_FOUND_ENEMY:
			return slRobotFoundEnemy;
		case SCHED_RGRUNT_REPEL:
		case SCHED_RGRUNT_REPEL_ATTACK:
			if ( pev->velocity.z > -128 )
				pev->velocity.z -= 32;

			return type == SCHED_RGRUNT_REPEL ? slRobotRepel : slRobotRepelAttack;
		case SCHED_RGRUNT_REPEL_LAND:
			return slRobotRepelLand;
		case SCHED_RGRUNT_WAIT_IN_COVER:
			return slRobotWaitInCover;
		case SCHED_RGRUNT_TAKE_COVER_FAILED:
			return GetScheduleOfType( HasConditions( bits_COND_SEE_ENEMY ) && NoFriendlyFire() ? SCHED_RANGE_ATTACK1 : SCHED_FAIL );
		case SCHED_RGRUNT_ESTABLISH_LINE_OF_FIRE_FAILED:
			return GetScheduleOfType( SCHED_FIND_ATTACK_POINT );
		default:
			return CBaseMonster::GetScheduleOfType( type );
	}
}

Schedule_t *CRobotGrunt::GetSchedule( void )
{
	m_iSentence = -1;
	KillLaserSpot();

	if ( pev->movetype == MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
	{
		if ( ( pev->flags & FL_ONGROUND ) || pev->groundentity || pev->velocity.z > 0 )
		{
			pev->movetype = MOVETYPE_STEP;
			return GetScheduleOfType( SCHED_RGRUNT_REPEL_LAND );
		}

		if ( m_MonsterState == MONSTERSTATE_COMBAT && !( pev->weapons & ( RGRUNT_SHOTGUN | RGRUNT_RPG | RGRUNT_SNIPER ) ) )
			return GetScheduleOfType( SCHED_RGRUNT_REPEL_ATTACK );

		return GetScheduleOfType( SCHED_RGRUNT_REPEL );
	}

	if ( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		CSound *sound = PBestSound();

		if ( sound && ( sound->m_iType & bits_SOUND_DANGER ) )
		{
			if ( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz( edict(), "RB_GREN", 1, ATTN_NORM, 0, m_voicePitch );
				JustSpoke();
			}

			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
		}
	}

	if ( m_MonsterState != MONSTERSTATE_COMBAT || HasConditions( bits_COND_ENEMY_DEAD ) )
		return CSquadMonster::GetSchedule();

	if ( HasConditions( bits_COND_NEW_ENEMY ) )
	{
		AlertSound();

		if ( InSquad() )
		{
			MySquadLeader()->m_fEnemyEluded = FALSE;

			if ( !IsLeader() )
			{
				if ( !m_fFirstEncounter )
					return GetScheduleOfType( SCHED_RGRUNT_ESTABLISH_LINE_OF_FIRE );

				m_fFirstEncounter = false;
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
			}

			if ( FOkToSpeak() )
			{
				if ( m_hEnemy )
				{
					if ( m_hEnemy->IsPlayer() )
						SENTENCEG_PlayRndSz( edict(), "RB_ALERT", 1, ATTN_NORM, 0, m_voicePitch );
					else if ( m_hEnemy->Classify() != CLASS_PLAYER_ALLY && m_hEnemy->Classify() != CLASS_HUMAN_PASSIVE
							  && m_hEnemy->Classify() != CLASS_MACHINE )
						SENTENCEG_PlayRndSz( edict(), "RB_MONST", 1, ATTN_NORM, 0, m_voicePitch );
				}

				JustSpoke();
			}

			return GetScheduleOfType( HasConditions( bits_COND_CAN_RANGE_ATTACK1 )
			                              ? SCHED_RGRUNT_SUPPRESS
			                              : SCHED_RGRUNT_ESTABLISH_LINE_OF_FIRE );
		}
	}
	else
	{
		if ( HasConditions( bits_COND_NO_AMMO_LOADED ) )
			return GetScheduleOfType( SCHED_RGRUNT_HIDE_RELOAD );

		if ( HasConditions( bits_COND_LIGHT_DAMAGE ) )
		{
			if ( RANDOM_LONG( 0, 99 ) > 90 || !m_hEnemy )
				return GetScheduleOfType( SCHED_SMALL_FLINCH );

			if ( FOkToSpeak() )
				m_iSentence = 3;

			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
		}

		if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
			return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

		if ( m_hEnemy && FClassnameIs( m_hEnemy->pev, "monster_gargantua" ) )
		{
			if ( pev->weapons & ( RGRUNT_HANDGRENADE | RGRUNT_M16 ) )
				return GetScheduleOfType( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) ? SCHED_RANGE_ATTACK2 : SCHED_TAKE_COVER_FROM_ENEMY );
		}
		else if ( ( pev->weapons & RGRUNT_M16 ) && HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
			return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
		else if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
		{
			if ( InSquad() && MySquadLeader()->m_fEnemyEluded && !HasConditions( bits_COND_ENEMY_FACING_ME ) )
			{
				MySquadLeader()->m_fEnemyEluded = FALSE;
				return GetScheduleOfType( SCHED_RGRUNT_FOUND_ENEMY );
			}

			if ( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

			if ( ( pev->weapons & RGRUNT_SNIPER ) && HasConditions( bits_COND_SEE_ENEMY ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
		}
		else if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) )
		{
			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
			{
				SENTENCEG_PlayRndSz( edict(), "RB_THROW", 1, ATTN_NORM, 0, m_voicePitch );
				JustSpoke();
				return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
			}

			if ( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
			{
				if ( FOkToSpeak() )
					m_iSentence = 5;

				return GetScheduleOfType( SCHED_RGRUNT_ESTABLISH_LINE_OF_FIRE );
			}

			SENTENCEG_PlayRndSz( edict(), "RB_TAUNT", 1, ATTN_NORM, 0, m_voicePitch );
			JustSpoke();
			return GetScheduleOfType( SCHED_STANDOFF );
		}
	}

	if ( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
	{
		m_iSentence = 6;
		return GetScheduleOfType( SCHED_RGRUNT_ESTABLISH_LINE_OF_FIRE );
	}

	return CSquadMonster::GetSchedule();
}

void CRobotGrunt::GlowEffect( int active )
{
	if ( active == 1 )
	{
		pev->rendermode = kRenderNormal;
		pev->renderfx = kRenderFxGlowShell;
		pev->renderamt = 4;
		pev->rendercolor = Vector( 100, 100, 225 );
		m_flNextGlow = gpGlobals->time + 0.15;
	}
	else if ( active == 0 )
	{
		pev->rendermode = kRenderNormal;
		pev->renderfx = kRenderFxNone;
		pev->renderamt = 255;
		pev->rendercolor = g_vecZero;
	}
}

void CRobotGrunt::UpdateGlow( void )
{
	if ( RANDOM_LONG( 0, 30 ) >= 27 && m_hEnemy )
	{
		if ( m_flSparkTime > gpGlobals->time && !m_fShockActive )
		{
			EMIT_SOUND( edict(), CHAN_STATIC, "debris/beamstart14.wav", 0.8f, ATTN_NORM );
			GlowEffect( 1 );
			m_fShockActive = true;
			m_flShockEndTime = gpGlobals->time + 0.45;
		}
	}
	else if ( RANDOM_LONG( 0, 40 ) == 15 && !m_hEnemy )
	{
		if ( m_flSparkTime > gpGlobals->time && !m_fShockActive )
		{
			EMIT_SOUND( edict(), CHAN_STATIC, "debris/beamstart14.wav", 0.8f, ATTN_NORM );
			GlowEffect( 1 );
			m_fShockActive = true;
			m_flShockEndTime = gpGlobals->time + 0.35;
		}
	}
}

void CRobotGrunt::RunAI( void )
{
	if ( m_fShockActive )
	{
		SetTouch( &CRobotGrunt::ShockTouch );

		if ( gpGlobals->time > m_flShockEndTime )
		{
			GlowEffect( 0 );
			m_fShockActive = false;
		}
	}
	else
		SetTouch( NULL );

	CSquadMonster::RunAI();
}

void CRobotGrunt::ShockTouch( CBaseEntity *other )
{
	if ( other && ( other->pev->flags & ( FL_CLIENT | FL_FAKECLIENT ) ) )
		other->TakeDamage( pev, pev, 25, DMG_SHOCK );
}

void CRobotGrunt::PrescheduleThink( void )
{
	if ( pev->health <= 20 && pev->deadflag != DEAD_DEAD )
	{
		float y = RANDOM_FLOAT( pev->absmin.y, pev->absmax.y );
		float x = RANDOM_FLOAT( pev->absmin.x, pev->absmax.x );
		Vector position( x, y, 0 );

		position.z += RANDOM_FLOAT( pev->absmax.z, pev->origin.z );

		if ( m_flSparkTime <= gpGlobals->time )
		{
			if ( gpGlobals->time - RANDOM_FLOAT( 0.2f, 0.5f ) > m_flSparkTime )
				m_flSparkTime = gpGlobals->time + 0.3;
		}
		else
		{
			UTIL_Sparks( position );

			int pitch = RANDOM_LONG( 0, 10 ) + 95;
			const char *sound = RANDOM_LONG( 1, 2 ) == 1 ? "buttons/spark5.wav" : "buttons/spark6.wav";

			EMIT_SOUND_DYN( edict(), CHAN_BODY, sound, 0.5f, ATTN_NORM, 0, pitch );
		}

		UpdateGlow();
	}

	if ( InSquad() && m_hEnemy )
	{
		if ( HasConditions( bits_COND_SEE_ENEMY ) )
			MySquadLeader()->m_flLastEnemySightTime = gpGlobals->time;
		else if ( gpGlobals->time - MySquadLeader()->m_flLastEnemySightTime > 5 )
			MySquadLeader()->m_fEnemyEluded = TRUE;
	}

	pev->view_ofs = Vector( 0, 0, m_fStanding ? 60 : 48 );

	if ( m_hEnemy )
		UpdateSpot( false );
}

void CRobotGrunt::KillLaserSpot( void )
{
	if ( m_hRPGLaserSpot )
	{
		UTIL_Remove( m_hRPGLaserSpot );
		m_hRPGLaserSpot = NULL;
	}
}

void CRobotGrunt::UpdateSpot( bool create )
{
	if ( !m_hRPGLaserSpot || !m_hEnemy )
		return;

	Vector source = GetGunPosition();

	m_vecLaserTargetAngles = UTIL_VecToAngles( m_hEnemy->Center() - source );

	if ( create )
		m_vecLaserAngles = m_vecLaserTargetAngles;
	else
	{
		for ( int i = 0; i < 3; i++ )
			m_vecLaserAngles[i] = UTIL_ApproachAngle( m_vecLaserTargetAngles[i], m_vecLaserAngles[i], m_flFrameTime * 25 );
	}

	Vector angles = m_vecLaserAngles;

	angles.x = -angles.x;
	UTIL_MakeVectors( angles );

	TraceResult trace;

	UTIL_TraceLine( source, source + gpGlobals->v_forward * 4096, dont_ignore_monsters, edict(), &trace );
	UTIL_SetOrigin( m_hRPGLaserSpot->pev, trace.vecEndPos );
	SetBlending( 0, m_vecLaserAngles.x );
}

void CRobotGrunt::TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType )
{
	CBaseEntity *entity = CBaseEntity::Instance( attacker );

	if ( pev->takedamage == DAMAGE_NO )
	{
		if ( entity && ( damageType & DMG_CLUB ) && CanRepairRobo( entity ) && IRelationship( entity, true ) <= R_NO )
		{
			EMIT_SOUND( edict(), CHAN_BODY, "debris/metal6.wav", 1, 0.6f );
			pev->takedamage = DAMAGE_YES;

			if ( pev->health < 1 )
				pev->health = 1;
		}

		return;
	}

	damageType &= ~( DMG_POISON | DMG_NERVEGAS );
	CSquadMonster::TraceAttack( attacker, damage, direction, trace, damageType );
}

int CRobotGrunt::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	if ( pev->takedamage == DAMAGE_NO || !attacker )
		return 0;

	CBaseEntity *entity = CBaseEntity::Instance( attacker );

	if ( ( damageType & DMG_CLUB ) && entity && CanRepairRobo( entity )
		 && IRelationship( entity, true ) <= R_NO )
	{
		if ( pev->max_health > pev->health )
		{
			EMIT_SOUND( edict(), CHAN_BODY, "debris/metal6.wav", 1, 0.6f );
			TakeHealth( fabs( damage ), damageType, 0 );
			GetDamagePoints( attacker, inflictor, 0.0f - damage );

			if ( pev->health < 1 )
				pev->health = 1;
		}

		return 1;
	}

	if ( damageType & ( DMG_SLASH | DMG_CLUB ) )
	{
		if ( !m_fShockActive )
		{
			if ( pev->health <= 20 && RANDOM_LONG( 0, 2 ) > 1 )
			{
				GlowEffect( 1 );
				m_fShockActive = true;
			}
		}
		else if ( entity && inflictor && !( inflictor->spawnflags & 0x2000 ) )
		{
			entity->TakeDamage( pev, pev, 25, DMG_SHOCK );
			damage = 0.01f;
		}
	}

	Forget( bits_MEMORY_INCOVER );

	TraceResult trace = UTIL_GetGlobalTrace();

	// This OR applies the armor path to every hit group
	if ( trace.iHitgroup != 1 || trace.iHitgroup != 10 )
	{
		if ( damageType & ( DMG_BULLET | DMG_CLUB | DMG_SNIPER ) )
		{
			damage *= 0.15;
			UTIL_Ricochet( trace.vecEndPos, 1 );
		}

		if ( damageType & DMG_SLASH )
		{
			if ( damage >= 5 )
				damage -= 5;

			damage *= 0.08;
		}

		if ( ( damageType & DMG_BLAST ) && pev->health > 10 )
		{
			if ( damage > 15 )
				damage -= 15;

			damage *= 0.2;
		}

		if ( ( damageType & DMG_BURN ) && pev->health > 10 )
			damage *= 0.5;
	}
	else
	{
		if ( damageType & ( DMG_BULLET | DMG_CLUB | DMG_SNIPER ) )
		{
			if ( damage >= 5 )
				damage -= 5;

			damage *= 0.1;
			UTIL_Ricochet( trace.vecEndPos, 1 );
		}

		if ( damageType & DMG_SHOCK )
			damage *= 1.5;
	}

	return CSquadMonster::TakeDamage( inflictor, attacker, damage, damageType );
}

void CRobotGrunt::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszTargetOnDeath" ) )
	{
		m_iszTargetOnDeath = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}

	CBaseMonster::KeyValue( pkvd );
}

void CRobotGrunt::Killed( entvars_t *attacker, int gib )
{
	if ( m_iszTargetOnDeath )
	{
		FireTargets( STRING( m_iszTargetOnDeath ), this, this, USE_TOGGLE, 0 );
		m_iszTargetOnDeath = 0;
	}

	if ( m_iszTriggerTarget )
	{
		FireTargets( STRING( m_iszTriggerTarget ), this, this, USE_TOGGLE, 0 );
		m_iszTriggerTarget = 0;
	}

	CSquadMonster::Killed( attacker, !attacker || FClassnameIs( attacker, "monster_bigmomma" ) ? GIB_NEVER : GIB_NORMAL );
}

void CRobotGrunt::DeathSound( void )
{
	KillLaserSpot();
	GlowEffect( 0 );
	m_fShockActive = false;
	CSoundEnt::InsertSound( bits_SOUND_DANGER, pev->origin, 250, 2.5f, this );

	const char *sound = RANDOM_LONG( 0, 1 ) ? "turret/tu_die2.wav" : "turret/tu_die.wav";

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, sound, 1, 0.5f, 0, 100 );
	UTIL_SetSize( pev, Vector( -4, -4, 0 ), Vector( 4, 4, 1 ) );
	BecomeDead( 0 );
	SetThink( &CRobotGrunt::RobotDeathThink );
	m_flDeathTime = gpGlobals->time + RANDOM_LONG( 3, 7 );
}

void CRobotGrunt::BreakRobot( const Vector &position, const Vector &sparkPosition )
{
	pev->deadflag = DEAD_DEAD;

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, position );
	WRITE_BYTE( TE_BREAKMODEL );
	WRITE_COORD( position.x );
	WRITE_COORD( position.y );
	WRITE_COORD( position.z );
	WRITE_COORD( 200 );
	WRITE_COORD( 200 );
	WRITE_COORD( 65 );
	WRITE_COORD( 10 );
	WRITE_COORD( 20 );
	WRITE_COORD( 80 );
	WRITE_BYTE( 30 );
	WRITE_SHORT( m_iComputerGibs );
	WRITE_BYTE( 15 );
	WRITE_BYTE( 100 );
	WRITE_BYTE( BREAK_METAL );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
	WRITE_BYTE( TE_BREAKMODEL );
	WRITE_COORD( pev->origin.x );
	WRITE_COORD( pev->origin.y );
	WRITE_COORD( pev->origin.z );
	WRITE_COORD( 200 );
	WRITE_COORD( 200 );
	WRITE_COORD( 96 );
	WRITE_COORD( 0 );
	WRITE_COORD( 0 );
	WRITE_COORD( 10 );
	WRITE_BYTE( 30 );
	WRITE_SHORT( m_iChromeGibs );
	WRITE_BYTE( 15 );
	WRITE_BYTE( 100 );
	WRITE_BYTE( BREAK_METAL );
	MESSAGE_END();

	EMIT_SOUND_DYN( edict(), CHAN_BODY, "debris/beamstart14.wav", 1, ATTN_NORM, 0, 100 );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time;

	CBaseEntity *smoker = CBaseEntity::Create( "env_smoker", pev->origin, g_vecZero, false );

	smoker->pev->health = 1;
	smoker->pev->scale = 10;
	smoker->pev->dmg = 0;
	smoker->pev->nextthink = gpGlobals->time + 0.5;
	UTIL_Sparks( sparkPosition );
}

void CRobotGrunt::GibMonster( void )
{
	KillLaserSpot();

	float y = RANDOM_FLOAT( pev->absmin.y, pev->absmax.y );
	float x = RANDOM_FLOAT( pev->absmin.x, pev->absmax.x );
	Vector sparkPosition( x, y, 0 );

	sparkPosition.z += RANDOM_FLOAT( pev->absmin.z, pev->origin.z );

	Vector position;
	Vector angles;

	GetAttachment( 0, position, angles );
	GetAttachment( 0, position, angles );
	StartExplode( pev->origin, 0, 0, (int) sk_rgrunt_explode.value );
	BreakRobot( position, sparkPosition );
}

void CRobotGrunt::RobotDeathThink( void )
{
	StudioFrameAdvance( 0 );
	pev->deadflag = DEAD_DEAD;

	float y = RANDOM_FLOAT( pev->absmin.y, pev->absmax.y );
	float x = RANDOM_FLOAT( pev->absmin.x, pev->absmax.x );
	Vector sparkPosition( x, y, 0 );

	sparkPosition.z += RANDOM_FLOAT( pev->absmin.z, pev->origin.z );

	Vector position;
	Vector angles;

	GetAttachment( 0, position, angles );

	if ( m_flDeathTime <= gpGlobals->time )
	{
		GetAttachment( 0, position, angles );
		pev->solid = SOLID_NOT;
		StartExplode( pev->origin, 0, 0, (int) sk_rgrunt_explode.value );
		BreakRobot( position, sparkPosition );
		return;
	}

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY, pev->origin );
	WRITE_BYTE( TE_SMOKE );
	WRITE_COORD( RANDOM_FLOAT( pev->absmin.x, pev->absmax.x ) );
	WRITE_COORD( RANDOM_FLOAT( pev->absmin.y, pev->absmax.y ) );
	WRITE_COORD( pev->origin.z );
	WRITE_SHORT( g_sModelIndexSmoke );
	WRITE_BYTE( 15 );
	WRITE_BYTE( 10 );
	MESSAGE_END();

	UTIL_Sparks( sparkPosition );
	pev->nextthink = gpGlobals->time + 0.1;
}
