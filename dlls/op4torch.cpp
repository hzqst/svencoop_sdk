#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "scripted.h"
#include "monsters.h"
#include "schedule.h"
#include "animation.h"
#include "op4torch.h"
#include "weapons.h"
#include "effects.h"
#include "soundent.h"
#include "server_sound.h"

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

#define TORCH_AE_HOLSTER_TORCH		17
#define TORCH_AE_HOLSTER_GUN		18
#define TORCH_AE_HOLSTER_BOTH		19
#define TORCH_AE_ACTIVATE_TORCH		20
#define TORCH_AE_DEACTIVATE_TORCH	21

#define TORCH_DEAGLE				( 1 << 0 )	// pev->weapons

#define TORCH_GUN_GROUP				2
#define TORCH_GUN_DEAGLE			0
#define TORCH_GUN_NONE				1
#define TORCH_GUN_EMPTY				2

#define HITGROUP_TORCH_CANISTER		8
#define HITGROUP_HEADGEAR			11

extern cvar_t sk_hgrunt_health;
extern cvar_t sk_hgrunt_kick;
extern cvar_t sk_grunt_handgrenades;

namespace
{
int g_fTorchQuestion;

enum
{
	TASK_TORCH_FACE_TOSS_DIR = LAST_TALKMONSTER_TASK + 4,
	TASK_TORCH_SPEAK_SENTENCE,
	TASK_TORCH_CHECK_FIRE,
	TASK_TORCH_DISARM
};

enum
{
	SCHED_TORCH_SUPPRESS = LAST_TALKMONSTER_SCHEDULE + 1,
	SCHED_TORCH_ESTABLISH_LINE_OF_FIRE,
	SCHED_TORCH_HIDE_RELOAD,
	SCHED_TORCH_SWEEP,
	SCHED_TORCH_FOUND_ENEMY,
	SCHED_TORCH_REPEL,
	SCHED_TORCH_REPEL_ATTACK,
	SCHED_TORCH_REPEL_LAND,
	SCHED_TORCH_WAIT_IN_COVER,
	SCHED_TORCH_TAKE_COVER_FAILED,
	SCHED_TORCH_ESTABLISH_LINE_OF_FIRE_FAILED,
	SCHED_TORCH_DISARM
};

const char *g_TorchSentences[] = {
	"FG_GREN", "FG_ALERT", "FG_MONSTER", "FG_COVER",
	"FG_THROW", "FG_CHARGE", "FG_TAUNT"
};

Task_t tlTorchDisarm[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_TORCH_DISARM, 0 }
};

Schedule_t slTorchDisarm[] = {
	{
		tlTorchDisarm,
		ARRAYSIZE( tlTorchDisarm ),
		0,
		0,
		"Disarm Torch"
	}
};

Task_t tlTorchFail[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 2 }
};

Schedule_t slTorchFail[] = {
	{
		tlTorchFail,
		ARRAYSIZE( tlTorchFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK2 |
		bits_COND_NEW_ENEMY,
		0,
		"Grunt Fail"
	}
};

Task_t tlTorchCombatFail[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 0.5f }
};

Schedule_t slTorchCombatFail[] = {
	{
		tlTorchCombatFail,
		ARRAYSIZE( tlTorchCombatFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"Grunt Combat Fail"
	}
};

Task_t tlTorchVictoryDance[] = {
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

Schedule_t slTorchVictoryDance[] = {
	{
		tlTorchVictoryDance,
		ARRAYSIZE( tlTorchVictoryDance ),
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntVictoryDance"
	}
};

Task_t tlTorchEstablishLineOfFire[] = {
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_TORCH_ESTABLISH_LINE_OF_FIRE_FAILED },
	{ TASK_GET_PATH_TO_ENEMY, 0 },
	{ TASK_TORCH_SPEAK_SENTENCE, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_AP, 0 }
};

Schedule_t slTorchEstablishLineOfFire[] = {
	{
		tlTorchEstablishLineOfFire,
		ARRAYSIZE( tlTorchEstablishLineOfFire ),
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

Task_t tlTorchFoundEnemy[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_SIGNAL1 }
};

Schedule_t slTorchFoundEnemy[] = {
	{
		tlTorchFoundEnemy,
		ARRAYSIZE( tlTorchFoundEnemy ),
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntFoundEnemy"
	}
};

Task_t tlTorchCombatFace[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_WAIT, 1.5f },
	{ TASK_SET_SCHEDULE, (float) SCHED_TORCH_SWEEP }
};

Schedule_t slTorchCombatFace[] = {
	{
		tlTorchCombatFace,
		ARRAYSIZE( tlTorchCombatFace ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD,
		0,
		"Combat Face"
	}
};

Task_t tlTorchSignalSuppress[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_SIGNAL2 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 }
};

Schedule_t slTorchSignalSuppress[] = {
	{
		tlTorchSignalSuppress,
		ARRAYSIZE( tlTorchSignalSuppress ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_SPECIAL1,
		bits_SOUND_DANGER,
		"SignalSuppress"
	}
};

Task_t tlTorchSuppress[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 }
};

Schedule_t slTorchSuppress[] = {
	{
		tlTorchSuppress,
		ARRAYSIZE( tlTorchSuppress ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_SPECIAL1,
		bits_SOUND_DANGER,
		"Suppress"
	}
};

Task_t tlTorchWaitInCover[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 1 }
};

Schedule_t slTorchWaitInCover[] = {
	{
		tlTorchWaitInCover,
		ARRAYSIZE( tlTorchWaitInCover ),
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

Task_t tlTorchTakeCover[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_TORCH_TAKE_COVER_FAILED },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_TORCH_SPEAK_SENTENCE, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_SET_SCHEDULE, (float) SCHED_TORCH_WAIT_IN_COVER }
};

Schedule_t slTorchTakeCover[] = {
	{
		tlTorchTakeCover,
		ARRAYSIZE( tlTorchTakeCover ),
		0,
		0,
		"TakeCover"
	}
};

Task_t tlTorchGrenadeCover[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FIND_COVER_FROM_ENEMY, 30 },
	{ TASK_FIND_FAR_NODE_COVER_FROM_ENEMY, 384 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_SPECIAL_ATTACK1 },
	{ TASK_CLEAR_MOVE_WAIT, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_TORCH_WAIT_IN_COVER }
};

Schedule_t slTorchGrenadeCover[] = {
	{
		tlTorchGrenadeCover,
		ARRAYSIZE( tlTorchGrenadeCover ),
		0,
		0,
		"GrenadeCover"
	}
};

Task_t tlTorchTossGrenadeCover[] = {
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RANGE_ATTACK2, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_TAKE_COVER_FROM_ENEMY }
};

Schedule_t slTorchTossGrenadeCover[] = {
	{
		tlTorchTossGrenadeCover,
		ARRAYSIZE( tlTorchTossGrenadeCover ),
		0,
		0,
		"TossGrenadeCover"
	}
};

Task_t tlTorchTakeCoverFromBestSound[] = {
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_COWER },
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FIND_COVER_FROM_BEST_SOUND, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_GRENADE, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_TURN_LEFT, 179 }
};

Schedule_t slTorchTakeCoverFromBestSound[] = {
	{
		tlTorchTakeCoverFromBestSound,
		ARRAYSIZE( tlTorchTakeCoverFromBestSound ),
		0,
		bits_SOUND_DANGER,
		"GruntTakeCoverFromBestSound"
	}
};

Task_t tlTorchHideReload[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_RELOAD },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_RELOAD }
};

Schedule_t slTorchHideReload[] = {
	{
		tlTorchHideReload,
		ARRAYSIZE( tlTorchHideReload ),
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntHideReload"
	}
};

Task_t tlTorchSweep[] = {
	{ TASK_TURN_LEFT, 179 },
	{ TASK_WAIT, 1 },
	{ TASK_TURN_LEFT, 179 },
	{ TASK_WAIT, 1 }
};

Schedule_t slTorchSweep[] = {
	{
		tlTorchSweep,
		ARRAYSIZE( tlTorchSweep ),
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

Task_t tlTorchRangeAttack1A[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_CROUCH },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 }
};

Schedule_t slTorchRangeAttack1A[] = {
	{
		tlTorchRangeAttack1A,
		ARRAYSIZE( tlTorchRangeAttack1A ),
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

Task_t tlTorchRangeAttack1B[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_WAIT_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_TORCH_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 }
};

Schedule_t slTorchRangeAttack1B[] = {
	{
		tlTorchRangeAttack1B,
		ARRAYSIZE( tlTorchRangeAttack1B ),
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

Task_t tlTorchRangeAttack2[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_TORCH_FACE_TOSS_DIR, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_RANGE_ATTACK2 },
	{ TASK_SET_SCHEDULE, (float) SCHED_TORCH_WAIT_IN_COVER }
};

Schedule_t slTorchRangeAttack2[] = {
	{
		tlTorchRangeAttack2,
		ARRAYSIZE( tlTorchRangeAttack2 ),
		0,
		0,
		"RangeAttack2"
	}
};

Task_t tlTorchRepel[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_GLIDE }
};

Schedule_t slTorchRepel[] = {
	{
		tlTorchRepel,
		ARRAYSIZE( tlTorchRepel ),
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

Task_t tlTorchRepelAttack[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_FLY }
};

Schedule_t slTorchRepelAttack[] = {
	{
		tlTorchRepelAttack,
		ARRAYSIZE( tlTorchRepelAttack ),
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD,
		0,
		"Repel Attack"
	}
};

Task_t tlTorchRepelLand[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_LAND },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_CLEAR_LASTPOSITION, 0 }
};

Schedule_t slTorchRepelLand[] = {
	{
		tlTorchRepelLand,
		ARRAYSIZE( tlTorchRepelLand ),
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
}

LINK_ENTITY_TO_CLASS( monster_human_torch_ally, CHGruntOp4Torch );

TYPEDESCRIPTION CHGruntOp4Torch::m_SaveData[] = {
	DEFINE_FIELD( CHGruntOp4Torch, m_flNextGrenadeCheck, FIELD_TIME ),
	DEFINE_FIELD( CHGruntOp4Torch, m_flNextPainTime, FIELD_TIME ),
	DEFINE_FIELD( CHGruntOp4Torch, m_vecTossVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( CHGruntOp4Torch, m_fThrowGrenade, FIELD_CHARACTER ),
	DEFINE_FIELD( CHGruntOp4Torch, m_fStanding, FIELD_CHARACTER ),
	DEFINE_FIELD( CHGruntOp4Torch, m_fFirstEncounter, FIELD_CHARACTER ),
	DEFINE_FIELD( CHGruntOp4Torch, m_cClipSize, FIELD_INTEGER ),
	DEFINE_FIELD( CHGruntOp4Torch, m_voicePitch, FIELD_INTEGER ),
	DEFINE_FIELD( CHGruntOp4Torch, m_iSentence, FIELD_INTEGER )
};

IMPLEMENT_SAVERESTORE( CHGruntOp4Torch, CSquadTalkMonster );

Schedule_t *CHGruntOp4Torch::m_scheduleList[] = {
	slTorchDisarm,
	slTorchFail,
	slTorchCombatFail,
	slTorchVictoryDance,
	slTorchEstablishLineOfFire,
	slTorchFoundEnemy,
	slTorchCombatFace,
	slTorchSignalSuppress,
	slTorchSuppress,
	slTorchWaitInCover,
	slTorchTakeCover,
	slTorchGrenadeCover,
	slTorchTossGrenadeCover,
	slTorchTakeCoverFromBestSound,
	slTorchHideReload,
	slTorchSweep,
	slTorchRangeAttack1A,
	slTorchRangeAttack1B,
	slTorchRangeAttack2,
	slTorchRepel,
	slTorchRepelAttack,
	slTorchRepelLand
};

void CHGruntOp4Torch::SetPlayerAlly( bool state )
{
	// The inherited OnCreate passes false to select the allied default
	SetPlayerAllyDirect( !state );
}

int CHGruntOp4Torch::Classify( void )
{
	return GetClassification( CLASS_HUMAN_MILITARY );
}

int CHGruntOp4Torch::ObjectCaps( void )
{
	return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION;
}

int CHGruntOp4Torch::ISoundMask( void )
{
	return bits_SOUND_WORLD | bits_SOUND_COMBAT | bits_SOUND_PLAYER | bits_SOUND_DANGER;
}

float CHGruntOp4Torch::GetPointsForDamage( float damage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	float points = pev->max_health / sk_hgrunt_health.value;
	float fraction = damage / pev->max_health;

	points *= 4;
	return points * fraction;
}

int CHGruntOp4Torch::IRelationship( CBaseEntity *target, bool ignoreNoTarget )
{
	if ( target && !target->m_fOverrideClass && Classify() == CLASS_HUMAN_MILITARY
		 && ( FClassnameIs( target->pev, "monster_alien_grunt" ) || FClassnameIs( target->pev, "monster_gargantua" )
			  || FClassnameIs( target->pev, "monster_bigmomma" ) ) )
		return R_NM;

	return CSquadTalkMonster::IRelationship( target, ignoreNoTarget );
}

void CHGruntOp4Torch::SetupFriendly( void )
{
	UTIL_SetModel( this, IsPlayerAlly() ? "models/hgrunt_torchf.mdl" : "models/hgrunt_torch.mdl" );
	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
}

void CanisterExplodeFX( Vector origin, float unused, float delay, int amount )
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

void CHGruntOp4Torch::KeyValue( KeyValueData *pkvd )
{
	CSquadTalkMonster::KeyValue( pkvd );
}

void CHGruntOp4Torch::TalkInit( void )
{
	CSquadTalkMonster::TalkInit();
	m_szGrp[TLK_ANSWER] = "FG_ANSWER";
	m_szGrp[TLK_QUESTION] = "FG_QUESTION";
	m_szGrp[TLK_IDLE] = "FG_IDLE";
	m_szGrp[TLK_STARE] = "FG_STARE";
	m_szGrp[TLK_USE] = "FG_OK";
	m_szGrp[TLK_UNUSE] = "FG_WAIT";
	m_szGrp[TLK_STOP] = "FG_STOP";
	m_szGrp[TLK_NOSHOOT] = "FG_SHOT";
	m_szGrp[TLK_HELLO] = "FG_HELLO";
	m_szGrp[TLK_PLHURT1] = "!FG_CUREA";
	m_szGrp[TLK_PLHURT2] = "!FG_CUREB";
	m_szGrp[TLK_PLHURT3] = "!FG_CUREC";
	m_szGrp[TLK_PHELLO] = "FG_POK";
	m_szGrp[TLK_PIDLE] = "FG_POK";
	m_szGrp[TLK_PQUESTION] = "FG_POK";
	m_szGrp[TLK_SMELL] = "FG_SMELL";
	m_szGrp[TLK_WOUND] = "FG_WOUND";
	m_szGrp[TLK_MORTAL] = "FG_MORTAL";
}

void CHGruntOp4Torch::Precache( void )
{
	TalkInit();
	CSquadTalkMonster::Precache();

	if ( !pev->model )
		UTIL_PrecacheModel( this, IsPlayerAlly() ? "models/hgrunt_torchf.mdl" : "models/hgrunt_torch.mdl" );

	const char *sounds[] = {
		"otis/ot_attack.wav", "hgrunt/gr_mgun1.wav",
		"fgrunt/death1.wav", "fgrunt/death2.wav", "fgrunt/death3.wav",
		"fgrunt/gr_pain1.wav", "fgrunt/gr_pain2.wav", "fgrunt/gr_pain3.wav",
		"fgrunt/gr_pain4.wav", "fgrunt/gr_pain5.wav", "hgrunt/gr_reload1.wav",
		"weapons/glauncher.wav", "zombie/claw_miss2.wav"
	};

	for ( int i = 0; i < ARRAYSIZE( sounds ); i++ )
		PrecacheSound_Intercept( this, sounds[i] );

	m_voicePitch = RANDOM_LONG( 0, 1 ) ? RANDOM_LONG( 0, 7 ) + 93 : 100;
	m_iBrassShell = UTIL_PrecacheModel( this, "models/shell.mdl" );
	UTIL_PrecacheModel( this, "sprites/lgtning.spr" );
}

void CHGruntOp4Torch::Spawn( void )
{
	Precache();

	if ( !SetupModel() )
		SetupFriendly();

	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->effects = 0;

	if ( pev->health == 0 )
		pev->health = sk_hgrunt_health.value;

	m_flFieldOfView = -0.7f;
	m_MonsterState = MONSTERSTATE_NONE;
	m_flNextGrenadeCheck = gpGlobals->time + 1;
	m_flNextPainTime = gpGlobals->time;
	m_iSentence = -1;
	m_afCapability = bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP | bits_CAP_USE_TANK;
	m_fEnemyEluded = FALSE;
	m_fFirstEncounter = true;
	m_fCheckCreatureDanger = true;
	m_HackedGunPos = Vector( 0, 0, 55 );
	m_afMoveShootCap = 0;
	m_cClipSize = 7;
	SetBodygroup( TORCH_GUN_GROUP, !( pev->weapons & TORCH_DEAGLE ) );
	m_fGunHolstered = !( pev->weapons & TORCH_DEAGLE );
	m_voicePitch = RANDOM_LONG( 0, 1 ) ? RANDOM_LONG( 0, 7 ) + 94 : 100;
	m_iPlayerDamage = 0;
	m_flHealGiveUpTime = gpGlobals->time;
	m_fTorchActive = false;
	m_hTorchBeam = NULL;
	m_fCanisterExploding = false;
	m_cAmmoLoaded = m_cClipSize;

	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( "Torch Grunt" );

	MonsterInit();

	if ( IsPlayerAlly() )
	{
		SetUse( &CBaseMonster::CallFollowerPlayerUse );
		m_fCanPlayerFollow = true;
	}
}

void CHGruntOp4Torch::SetYawSpeed( void )
{
	pev->yaw_speed = m_Activity == ACT_FLY || m_Activity == ACT_GLIDE ? 200 : 270;
}

BOOL CHGruntOp4Torch::FCanCheckAttacks( void )
{
	return !HasConditions( bits_COND_ENEMY_TOOFAR );
}

BOOL CHGruntOp4Torch::CheckMeleeAttack1( float dot, float distance )
{
	CBaseEntity *enemy = m_hEnemy;
	CBaseMonster *monster = enemy ? enemy->MyMonsterPointer() : NULL;

	return monster && distance <= 64 && dot >= 0.7
		&& monster->Classify() != CLASS_ALIEN_BIOWEAPON && monster->Classify() != CLASS_PLAYER_BIOWEAPON;
}

BOOL CHGruntOp4Torch::CheckRangeAttack1( float dot, float distance )
{
	if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) || distance > 2048 || dot < 0.5f || !NoFriendlyFire() )
		return FALSE;

	CBaseEntity *enemy = m_hEnemy;

	if ( !enemy->IsPlayer() && distance <= 64 )
		return FALSE;

	Vector standing = pev->origin + Vector( 0, 0, 60 );
	Vector crouching = pev->origin + Vector( 0, 0, 48 );
	TraceResult trace;

	if ( RANDOM_LONG( 0, 9 ) == 0 && m_Activity != ACT_RANGE_ATTACK1 )
		m_fStanding = RANDOM_LONG( 0, 1 ) != 0;

	Vector source = m_fStanding ? standing : crouching;

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

BOOL CHGruntOp4Torch::CheckRangeAttack2( float dot, float distance )
{
	return FALSE;
}

void CHGruntOp4Torch::CheckAmmo( void )
{
	if ( m_cAmmoLoaded <= 0 )
		SetConditions( bits_COND_NO_AMMO_LOADED );
}

Vector CHGruntOp4Torch::GetGunPosition( void )
{
	return pev->origin + Vector( 0, 0, m_fStanding ? 60 : 48 );
}

void CHGruntOp4Torch::SetActivity( Activity activity )
{
	int sequence;

	switch ( activity )
	{
		case ACT_IDLE:
			if ( m_MonsterState == MONSTERSTATE_COMBAT && !m_fGunHolstered )
				activity = ACT_IDLE_ANGRY;

			sequence = LookupActivity( activity );
			break;
		case ACT_WALK:
			sequence = LookupActivity( pev->health <= 20 ? ACT_WALK_HURT : activity );
			break;
		case ACT_RUN:
			sequence = LookupActivity( pev->health <= 20 ? ACT_RUN_HURT : activity );
			break;
		case ACT_RANGE_ATTACK1:
			sequence = LookupSequence( m_fStanding ? "standing_mp5" : "crouching_mp5" );
			break;
		case ACT_RANGE_ATTACK2:
			sequence = LookupSequence( "throwgrenade" );
			break;
		default:
			sequence = LookupActivity( activity );
			break;
	}

	m_Activity = activity;
	m_IdealActivity = activity;

	if ( sequence <= ACTIVITY_NOT_AVAILABLE )
	{
		pev->sequence = 0;
		return;
	}

	if ( pev->sequence != sequence || !m_fSequenceLoops )
		pev->frame = 0;

	pev->sequence = sequence;
	ResetSequenceInfo();
	SetYawSpeed();
}

BOOL CHGruntOp4Torch::FOkToSpeak( void )
{
	if ( !IsAlive() )
		return FALSE;

	float wait = IsPlayerAlly() ? CSquadTalkMonster::g_talkWaitTime : CSquadTalkMonster::g_talkWaitTimeEvil;

	if ( wait > gpGlobals->time )
		return FALSE;

	return !( pev->spawnflags & SF_MONSTER_GAG ) || m_MonsterState == MONSTERSTATE_COMBAT;
}

void CHGruntOp4Torch::JustSpoke( void )
{
	if ( IsPlayerAlly() )
		CSquadTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT( 2.5f, 3 );
	else
		CSquadTalkMonster::g_talkWaitTimeEvil = gpGlobals->time + RANDOM_FLOAT( 2.5f, 3 );

	m_iSentence = -1;
}

void CHGruntOp4Torch::SpeakSentence( void )
{
	if ( m_iSentence == -1 || !FOkToSpeak() )
		return;

	SENTENCEG_PlayRndSz( edict(), g_TorchSentences[m_iSentence], 1, ATTN_NORM, 0, m_voicePitch );
	JustSpoke();
}

void CHGruntOp4Torch::IdleSound( void )
{
	if ( !FOkToSpeak() )
		return;

	if ( !g_fTorchQuestion )
	{
		if ( !RANDOM_LONG( 0, 1 ) )
			return;

		switch ( RANDOM_LONG( 0, 2 ) )
		{
			case 0:
				SENTENCEG_PlayRndSz( edict(), "FG_CHECK", 1, ATTN_NORM, 0, m_voicePitch );
				g_fTorchQuestion = 1;
				break;
			case 1:
				SENTENCEG_PlayRndSz( edict(), "FG_QUEST", 1, ATTN_NORM, 0, m_voicePitch );
				g_fTorchQuestion = 2;
				break;
			case 2:
				SENTENCEG_PlayRndSz( edict(), "FG_IDLE", 1, ATTN_NORM, 0, m_voicePitch );
				break;
		}
	}
	else
	{
		if ( g_fTorchQuestion == 1 )
			SENTENCEG_PlayRndSz( edict(), "FG_CLEAR", 1, ATTN_NORM, 0, m_voicePitch );
		else if ( g_fTorchQuestion == 2 )
			SENTENCEG_PlayRndSz( edict(), "FG_ANSWER", 1, ATTN_NORM, 0, m_voicePitch );

		g_fTorchQuestion = 0;
	}

	JustSpoke();
}

void CHGruntOp4Torch::AlertSound( void )
{
	if ( FOkToSpeak() && m_hEnemy && m_hEnemy->IsAlive() )
	{
		m_hTalkTarget = m_hEnemy;
		PlaySentence( "FG_ATTACK", RANDOM_FLOAT( 2.8f, 3.2f ), 1, ATTN_NORM );
	}
}

void CHGruntOp4Torch::DeclineFollowing( void )
{
	PlaySentence( "FG_POK", 2, 1, ATTN_NORM );
}

void CHGruntOp4Torch::PainSound( void )
{
	if ( gpGlobals->time <= m_flNextPainTime )
		return;

	int sound = RANDOM_LONG( 0, 6 );
	const char *sounds[] = {
		"fgrunt/gr_pain1.wav", "fgrunt/gr_pain2.wav", "fgrunt/gr_pain3.wav",
		"fgrunt/gr_pain4.wav", "fgrunt/gr_pain5.wav"
	};

	if ( sound < ARRAYSIZE( sounds ) )
		EMIT_SOUND_DYN( edict(), CHAN_VOICE, sounds[sound], 1, ATTN_NORM, 0, m_voicePitch );

	m_flNextPainTime = gpGlobals->time + 1;
}

void CHGruntOp4Torch::DeathSound( void )
{
	const char *sounds[] = { "fgrunt/death1.wav", "fgrunt/death2.wav", "fgrunt/death3.wav" };

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, sounds[RANDOM_LONG( 0, 2 )], 1, ATTN_IDLE, 0, 100 );
}

CBaseEntity *CHGruntOp4Torch::Kick( void )
{
	UTIL_MakeVectors( pev->angles );

	Vector source( pev->origin.x, pev->origin.y, pev->origin.z + pev->size.z * 0.5 );
	TraceResult trace;

	UTIL_TraceHull( source, source + gpGlobals->v_forward * 70, dont_ignore_monsters, head_hull, edict(), &trace );

	if ( !trace.pHit || trace.pHit->free || !GET_PRIVATE( trace.pHit ) )
		return NULL;

	return CBaseEntity::Instance( trace.pHit );
}

void CHGruntOp4Torch::Shoot( void )
{
	if ( !m_hEnemy )
		return;

	Vector source = GetGunPosition();
	Vector direction = ShootAtEnemy( source );

	UTIL_MakeVectors( pev->angles );

	Vector velocity = gpGlobals->v_right * RANDOM_FLOAT( 40, 90 );

	velocity = velocity + gpGlobals->v_up * RANDOM_FLOAT( 75, 200 );
	velocity = velocity + gpGlobals->v_forward * RANDOM_FLOAT( -40, 40 );
	EjectBrass( source - direction * -24, velocity, pev->angles.y, m_iBrassShell, TE_BOUNCE_SHELL );
	FireBullets( 1, source, direction, VECTOR_CONE_2DEGREES, 1024, BULLET_MONSTER_EAGLE, 4, 0, NULL, FBDM_DRAW );
	pev->effects |= EF_MUZZLEFLASH;
	m_cAmmoLoaded--;
	SetBlending( 0, UTIL_VecToAngles( direction ).x );
}

void CHGruntOp4Torch::DropWeapon( void )
{
	if ( pev->weapons == TORCH_DEAGLE )
		CBaseMonster::DropWeapon( "weapon_eagle", false );
}

void CHGruntOp4Torch::GibMonster( void )
{
	if ( GetBodygroup( TORCH_GUN_GROUP ) != TORCH_GUN_EMPTY && pev->weapons == TORCH_DEAGLE )
		CBaseMonster::DropWeapon( "weapon_eagle", false );

	CBaseMonster::GibMonster();
}

void CHGruntOp4Torch::HandleAnimEvent( MonsterEvent_t *event )
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
			CBaseEntity *target = Kick();

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
			if ( NoFriendlyFire() || !m_hEnemy || !m_hEnemy->IsAlive() )
			{
				Shoot();
				EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "otis/ot_attack.wav", 1, ATTN_NORM, 0, 100 );

				if ( FOkToSpeak() && m_hEnemy && m_hEnemy->IsAlive() )
				{
					m_hTalkTarget = m_hEnemy;
					PlaySentence( "FG_ATTACK", RANDOM_FLOAT( 2.8f, 3.2f ), 1, ATTN_NORM );
				}

				CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 0.3f, this );
			}
			return;
		case HGRUNT_AE_GREN_TOSS:
			UTIL_MakeVectors( pev->angles );
			CGrenade::ShootTimed( pev, GetGunPosition(), m_vecTossVelocity, 3.5f );
			m_fThrowGrenade = false;
			m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 5, 10 );
			return;
		case HGRUNT_AE_GREN_LAUNCH:
			EMIT_SOUND( edict(), CHAN_WEAPON, "weapons/glauncher.wav", 0.8f, ATTN_NORM );
			CGrenade::ShootContact( pev, GetGunPosition(), m_vecTossVelocity );
			m_fThrowGrenade = false;
			m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 4, 8 );
			return;
		case HGRUNT_AE_GREN_DROP:
			UTIL_MakeVectors( pev->angles );
			CGrenade::ShootTimed( pev, pev->origin + gpGlobals->v_forward * 17 - gpGlobals->v_right * 27 + gpGlobals->v_up * 6,
			                      g_vecZero, 3 );
			return;
		case HGRUNT_AE_CAUGHT_ENEMY:
			if ( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz( edict(), "FG_ALERT", 1, ATTN_NORM, 0, m_voicePitch );
				JustSpoke();
			}

			CSquadTalkMonster::HandleAnimEvent( event );
			return;
		case HGRUNT_AE_DROP_GUN:
			DropWeapon();
			return;
		case TORCH_AE_HOLSTER_TORCH:
			SetBodygroup( TORCH_GUN_GROUP, TORCH_GUN_DEAGLE );
			m_fGunHolstered = false;
			return;
		case TORCH_AE_HOLSTER_GUN:
		case TORCH_AE_HOLSTER_BOTH:
			m_fGunHolstered = true;
			SetBodygroup( TORCH_GUN_GROUP, TORCH_GUN_NONE );
			return;
		case TORCH_AE_ACTIVATE_TORCH:
		{
			pev->effects |= EF_MUZZLEFLASH;

			Vector position;
			Vector angles;

			GetAttachment( 2, position, angles );
			UTIL_Sparks( position );
			ToggleTorch( 1 );
			return;
		}
		case TORCH_AE_DEACTIVATE_TORCH:
			ToggleTorch( 0 );
			return;
		default:
			CSquadTalkMonster::HandleAnimEvent( event );
			return;
	}
}

void CHGruntOp4Torch::StartTask( Task_t *task )
{
	switch ( task->iTask )
	{
		case TASK_RUN_PATH:
		case TASK_WALK_PATH:
			Forget( bits_MEMORY_INCOVER );
			CSquadTalkMonster::StartTask( task );
			break;
		case TASK_FACE_IDEAL:
		case TASK_FACE_ENEMY:
			CSquadTalkMonster::StartTask( task );

			if ( pev->movetype == MOVETYPE_FLY )
				m_IdealActivity = ACT_GLIDE;

			break;
		case TASK_RELOAD:
			m_IdealActivity = ACT_RELOAD;
			break;
		case TASK_TORCH_FACE_TOSS_DIR:
			break;
		case TASK_TORCH_SPEAK_SENTENCE:
			SpeakSentence();
			TaskComplete();
			break;
		case TASK_TORCH_CHECK_FIRE:
			TaskComplete();

			if ( !NoFriendlyFire() )
			{
				ChangeSchedule( GetScheduleOfType( SCHED_FIND_ATTACK_POINT ) );
				SetConditions( bits_COND_SPECIAL1 );
			}
			break;
		case TASK_TORCH_DISARM:
			m_Activity = ACT_RESET;
			m_IdealActivity = ACT_RESET;
			pev->sequence = LookupSequence( "store_torch" );

			if ( pev->sequence == -1 )
			{
				pev->sequence = 0;
				m_fGunHolstered = false;
				TaskComplete();
				break;
			}

			pev->frame = 0;
			ResetSequenceInfo();
			pev->framerate = 2.5f;
			break;
		default:
			CSquadTalkMonster::StartTask( task );
			break;
	}
}

void CHGruntOp4Torch::RunTask( Task_t *task )
{
	if ( task->iTask == TASK_TORCH_FACE_TOSS_DIR )
	{
		MakeIdealYaw( pev->origin + m_vecTossVelocity * 64 );
		ChangeYaw( (int) pev->yaw_speed );

		if ( FacingIdeal() )
			TaskComplete();
	}
	else if ( task->iTask == TASK_TORCH_DISARM )
	{
		if ( m_fSequenceFinished )
			TaskComplete();
	}
	else
		CSquadTalkMonster::RunTask( task );
}

Schedule_t *CHGruntOp4Torch::ScheduleFromName( const char *name )
{
	Schedule_t *schedule = ScheduleInList( name, m_scheduleList, ARRAYSIZE( m_scheduleList ) );

	return schedule ? schedule : CSquadTalkMonster::ScheduleFromName( name );
}

Schedule_t *CHGruntOp4Torch::GetScheduleOfType( int type )
{
	switch ( type )
	{
		case SCHED_COMBAT_FACE:
			return slTorchCombatFace;
		case SCHED_VICTORY_DANCE:
			return InSquad() && !IsLeader() ? slTorchFail : slTorchVictoryDance;
		case SCHED_TAKE_COVER_FROM_ENEMY:
			if ( !InSquad() )
			{
				// This path still consumes the random draw
				RANDOM_LONG( 0, 1 );
				return slTorchTakeCover;
			}

			if ( sk_grunt_handgrenades.value < 1 || !HasConditions( bits_COND_CAN_RANGE_ATTACK2 )
				 || !OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
				return slTorchTakeCover;

			if ( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz( edict(), "FG_THROW", 1, ATTN_NORM, 0, m_voicePitch );
				JustSpoke();
			}

			return slTorchTossGrenadeCover;
		case SCHED_TAKE_COVER_FROM_BEST_SOUND:
			return slTorchTakeCoverFromBestSound;
		case SCHED_RANGE_ATTACK1:
			if ( m_fGunHolstered )
				return slTorchDisarm;

			if ( RANDOM_LONG( 0, 9 ) == 0 )
				m_fStanding = RANDOM_LONG( 0, 1 ) != 0;

			return m_fStanding ? slTorchRangeAttack1B : slTorchRangeAttack1A;
		case SCHED_RANGE_ATTACK2:
			return slTorchRangeAttack2;
		case SCHED_FAIL:
			if ( !m_hEnemy )
				return slTorchFail;

			if ( HasConditions( bits_COND_SEE_ENEMY ) && NoFriendlyFire() )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			return slTorchCombatFail;
		case SCHED_TORCH_SUPPRESS:
			if ( m_fGunHolstered )
				return slTorchDisarm;

			if ( !m_hEnemy->IsPlayer() || !m_fFirstEncounter )
				return slTorchSuppress;

			m_fFirstEncounter = false;
			return slTorchSignalSuppress;
		case SCHED_TORCH_ESTABLISH_LINE_OF_FIRE:
			return slTorchEstablishLineOfFire;
		case SCHED_TORCH_HIDE_RELOAD:
			return slTorchHideReload;
		case SCHED_TORCH_SWEEP:
			return slTorchSweep;
		case SCHED_TORCH_FOUND_ENEMY:
			return slTorchFoundEnemy;
		case SCHED_TORCH_REPEL:
		case SCHED_TORCH_REPEL_ATTACK:
			if ( pev->velocity.z > -128 )
				pev->velocity.z -= 32;

			return type == SCHED_TORCH_REPEL ? slTorchRepel : slTorchRepelAttack;
		case SCHED_TORCH_REPEL_LAND:
			return slTorchRepelLand;
		case SCHED_TORCH_WAIT_IN_COVER:
			return slTorchWaitInCover;
		case SCHED_TORCH_TAKE_COVER_FAILED:
			return GetScheduleOfType( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) ? SCHED_RANGE_ATTACK1 : SCHED_FAIL );
		case SCHED_TORCH_ESTABLISH_LINE_OF_FIRE_FAILED:
			return GetScheduleOfType( HasConditions( bits_COND_CAN_RANGE_ATTACK1 | bits_COND_SEE_ENEMY ) && NoFriendlyFire()
			                              ? SCHED_RANGE_ATTACK1
			                              : SCHED_FIND_ATTACK_POINT );
		case SCHED_TORCH_DISARM:
			return slTorchDisarm;
		default:
			return CSquadTalkMonster::GetScheduleOfType( type );
	}
}

Schedule_t *CHGruntOp4Torch::GetSchedule( void )
{
	// The selector discards this schedule result
	if ( m_fGunHolstered )
		GetScheduleOfType( SCHED_TORCH_DISARM );

	if ( !HasMemory( bits_MEMORY_SUSPICIOUS ) )
		m_iPlayerDamage = 0;

	m_iSentence = -1;

	if ( pev->movetype == MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
	{
		if ( ( pev->flags & FL_ONGROUND ) || pev->groundentity || pev->velocity.z > 0 )
		{
			pev->movetype = MOVETYPE_STEP;
			return GetScheduleOfType( SCHED_TORCH_REPEL_LAND );
		}

		return GetScheduleOfType( m_MonsterState == MONSTERSTATE_COMBAT ? SCHED_TORCH_REPEL_ATTACK : SCHED_TORCH_REPEL );
	}

	if ( HasConditions( bits_COND_ENEMY_DEAD ) && FOkToSpeak() )
		PlaySentence( "FG_KILL", 4, 1, ATTN_NORM );

	if ( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		CSound *sound = PBestSound();

		if ( sound && ( sound->m_iType & bits_SOUND_DANGER ) )
		{
			if ( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz( edict(), "FG_GREN", 1, ATTN_NORM, 0, m_voicePitch );
				JustSpoke();
			}

			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
		}
	}

	if ( m_MonsterState != MONSTERSTATE_COMBAT )
		return CBaseMonster::GetSchedule();

	if ( m_hGuardEnt )
	{
		if ( !m_hTargetEnt || m_hTargetEnt->edict() != m_hGuardEnt->edict() )
			StartPlayerFollowing( m_hGuardEnt, false );

		if ( m_hTargetEnt && ( Center() - m_hTargetEnt->Center() ).Length2D() > 256 )
			return GetScheduleOfType( SCHED_TARGET_PLAYERCHASE_FORCE );
	}

	if ( HasConditions( bits_COND_ENEMY_DEAD ) )
		return CBaseMonster::GetSchedule();

	if ( HasConditions( bits_COND_NEW_ENEMY ) )
	{
		if ( InSquad() )
		{
			MySquadLeader()->m_fEnemyEluded = FALSE;

			if ( !IsLeader() )
			{
				if ( !m_fFirstEncounter )
					return GetScheduleOfType( SCHED_TORCH_ESTABLISH_LINE_OF_FIRE );

				m_fFirstEncounter = false;
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
			}

			if ( FOkToSpeak() )
			{
				if ( m_hEnemy )
					SENTENCEG_PlayRndSz( edict(), "FG_ALERT", 1, ATTN_NORM, 0, m_voicePitch );
				else if ( m_hEnemy )
				{
					if ( IsPlayerAlly() )
					{
						if ( m_hEnemy->Classify() != CLASS_HUMAN_PASSIVE && m_hEnemy->Classify() != CLASS_MACHINE )
							SENTENCEG_PlayRndSz( edict(), "FG_MONST", 1, ATTN_NORM, 0, m_voicePitch );
					}
					else if ( m_hEnemy->Classify() != CLASS_PLAYER_ALLY && m_hEnemy->Classify() != CLASS_HUMAN_PASSIVE
							  && m_hEnemy->Classify() != CLASS_MACHINE )
						SENTENCEG_PlayRndSz( edict(), "FG_MONST", 1, ATTN_NORM, 0, m_voicePitch );
				}

				JustSpoke();
			}

			return GetScheduleOfType( HasConditions( bits_COND_CAN_RANGE_ATTACK1 )
			                              ? SCHED_TORCH_SUPPRESS
			                              : SCHED_TORCH_ESTABLISH_LINE_OF_FIRE );
		}
	}
	else
	{
		if ( HasConditions( bits_COND_NO_AMMO_LOADED ) )
			return GetScheduleOfType( SCHED_TORCH_HIDE_RELOAD );

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

		if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
		{
			if ( InSquad() && MySquadLeader()->m_fEnemyEluded && !HasConditions( bits_COND_ENEMY_FACING_ME ) )
			{
				MySquadLeader()->m_fEnemyEluded = FALSE;
				return GetScheduleOfType( SCHED_TORCH_FOUND_ENEMY );
			}

			if ( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			return GetScheduleOfType( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE )
			                              ? SCHED_RANGE_ATTACK2
			                              : SCHED_TAKE_COVER_FROM_ENEMY );
		}

		if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) )
		{
			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
			{
				if ( FOkToSpeak() )
				{
					SENTENCEG_PlayRndSz( edict(), "FG_THROW", 1, ATTN_NORM, 0, m_voicePitch );
					JustSpoke();
				}

				return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
			}

			if ( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
			{
				if ( FOkToSpeak() )
					m_iSentence = 5;

				return GetScheduleOfType( SCHED_TORCH_ESTABLISH_LINE_OF_FIRE );
			}

			if ( FOkToSpeak() && RANDOM_LONG( 0, 1 ) )
			{
				SENTENCEG_PlayRndSz( edict(), "FG_TAUNT", 1, ATTN_NORM, 0, m_voicePitch );
				JustSpoke();
			}

			return GetScheduleOfType( SCHED_STANDOFF );
		}
	}

	if ( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
	{
		m_iSentence = 6;
		return GetScheduleOfType( SCHED_TORCH_ESTABLISH_LINE_OF_FIRE );
	}

	return CBaseMonster::GetSchedule();
}

void CHGruntOp4Torch::RunAI( void )
{
	CBaseMonster::RunAI();

	if ( m_hEnemy && ( m_hEnemy->pev->flags & FL_MONSTER ) && ( pev->weapons & TORCH_DEAGLE ) && m_Activity == ACT_RANGE_ATTACK1 )
		pev->framerate = 0.7f;

	if ( !m_fTorchActive )
		return;

	GetAttachment( 2, m_vecTorchSource, m_vecTorchAngles );
	GetAttachment( 3, m_vecTorchEnd, m_vecTorchAngles );
	UTIL_MakeVectors( pev->angles );
	UTIL_TraceLine( m_vecTorchSource - gpGlobals->v_forward * 16, m_vecTorchEnd,
	                ignore_monsters, edict(), &m_TorchTrace );

	if ( m_TorchTrace.flFraction != 1 )
	{
		pev->effects |= EF_MUZZLEFLASH;
		UTIL_DecalTrace( &m_TorchTrace, 17 );
		UTIL_Sparks( m_vecTorchSource );
	}
}

void CHGruntOp4Torch::PrescheduleThink( void )
{
	if ( !InSquad() || !m_hEnemy )
		return;

	if ( HasConditions( bits_COND_SEE_ENEMY ) )
		MySquadLeader()->m_flLastEnemySightTime = gpGlobals->time;
	else if ( gpGlobals->time - MySquadLeader()->m_flLastEnemySightTime > 5 )
		MySquadLeader()->m_fEnemyEluded = TRUE;
}

void CHGruntOp4Torch::ToggleTorch( int active )
{
	if ( m_hTorchBeam )
	{
		UTIL_Remove( m_hTorchBeam );
		m_hTorchBeam = NULL;
	}

	if ( active )
	{
		m_hTorchBeam = CBeam::BeamCreate( "sprites/lgtning.spr", 30 );

		CBeam *beam = (CBeam *) (CBaseEntity *) m_hTorchBeam;

		beam->EntsInit( entindex(), entindex() );
		beam->SetStartAttachment( 3 );
		beam->SetEndAttachment( 4 );
		beam->SetColor( 96, 96, 255 );
		beam->SetBrightness( 255 );
		beam->SetScrollRate( 5 );
		beam->SetNoise( 8 );
		beam->SetFlags( 0x80 );
		m_fGunHolstered = true;
		m_fTorchActive = true;
	}
	else
		m_fTorchActive = false;
}

void CHGruntOp4Torch::CanisterExplode( void )
{
	if ( m_hTorchBeam )
	{
		UTIL_Remove( m_hTorchBeam );
		m_hTorchBeam = NULL;
	}

	m_fTorchActive = false;

	if ( m_fCanisterExploding )
	{
		SetThink( NULL );
		pev->takedamage = DAMAGE_NO;

		CanisterExplodeFX( pev->origin, 0, 0, 100 );
		Killed( pev, GIB_ALWAYS );
	}

	pev->renderfx = kRenderFxExplode;
	m_fCanisterExploding = true;
	pev->nextthink = gpGlobals->time + 0.05;
}

void CHGruntOp4Torch::TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType )
{
	switch ( trace->iHitgroup )
	{
		case HITGROUP_HEAD:
			if ( damageType & ( DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_CLUB | DMG_SNIPER ) )
			{
				damage -= 20;

				if ( damage <= 0 )
				{
					UTIL_Ricochet( trace->vecEndPos, 1 );
					damage = 0.01f;
				}
			}
			break;
		case HITGROUP_CHEST:
		case HITGROUP_STOMACH:
			if ( damageType & ( DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_SNIPER ) )
			{
				if ( damage >= 2 )
					damage -= 2;

				damage *= 0.5;
			}
			break;
		case HITGROUP_TORCH_CANISTER:
			if ( m_pCine )
			{
				UTIL_Ricochet( trace->vecEndPos, 1 );
				damage = 0;
			}
			else
			{
				if ( !attacker )
					return;

				CBaseEntity *entity = CBaseEntity::Instance( attacker );

				if ( CheckAttacker( entity )
					 || ( ( RANDOM_LONG( 0, 3 ) != 3 || ( IsPlayerAlly() && FClassnameIs( attacker, "player" ) ) ) && IsPlayerAlly() ) )
				{
					UTIL_Ricochet( trace->vecEndPos, 1 );
					damage = 0;
				}
				else
				{
					SetThink( &CHGruntOp4Torch::CanisterExplode );
					pev->nextthink = gpGlobals->time;
				}
			}
			break;
		case HITGROUP_HEADGEAR:
			trace->iHitgroup = HITGROUP_HEAD;
			break;
	}

	CSquadTalkMonster::TraceAttack( attacker, damage, direction, trace, damageType );
}

int CHGruntOp4Torch::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	if ( !attacker )
		return 0;

	CBaseEntity *entity = CBaseEntity::Instance( attacker );

	if ( CheckAttacker( entity ) )
		return 0;

	Forget( bits_MEMORY_INCOVER );

	if ( ( damageType & DMG_CLUB ) && entity && entity->IsPlayerAlly() == IsPlayerAlly() && entity->Classify() == Classify() )
		damage = 0;

	int result = CSquadTalkMonster::TakeDamage( inflictor, attacker, damage, damageType );

	if ( ( ( !IsAlive() || pev->deadflag == DEAD_DYING ) && !IsPlayerAlly() )
		 || m_MonsterState == MONSTERSTATE_PRONE || !( attacker->flags & FL_CLIENT ) || !IsPlayerAlly() )
		return result;

	if ( m_hEnemy )
	{
		if ( !m_hEnemy->IsPlayer() && pev->deadflag == DEAD_NO )
			PlaySentence( "FG_SHOT", 4, 1, ATTN_NORM );

		return result;
	}

	if ( pev->deadflag != DEAD_NO )
		return result;

	if ( ( HasMemory( bits_MEMORY_SUSPICIOUS ) || entity->IsFacing( pev, 0.96f ) ) && m_iPlayerDamage >= RANDOM_LONG( 4, 7 ) )
	{
		PlaySentence( "FG_MAD", 4, 1, ATTN_NORM );
		Remember( bits_MEMORY_PROVOKED );
		StopPlayerFollowing( true, false );
	}
	else
	{
		PlaySentence( "FG_SHOT", 4, 1, ATTN_NORM );
		Remember( bits_MEMORY_SUSPICIOUS );
		m_iPlayerDamage++;
	}

	return result;
}
