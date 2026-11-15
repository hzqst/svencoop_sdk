#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "animation.h"
#include "schedule.h"
#include "squadmonster.h"
#include "squadtalkmonster.h"
#include "weapons.h"
#include "soundent.h"
#include "effects.h"
#include "customentity.h"
#include "hgrunt_variants.h"

extern cvar_t sk_grunt_handgrenades;

// pev->weapons of the grunts
#define HGRUNT_9MMAR			( 1 << 0 )
#define HGRUNT_HANDGRENADE		( 1 << 1 )
#define HGRUNT_GRENADELAUNCHER	( 1 << 2 )
#define HGRUNT_SHOTGUN			( 1 << 3 )
#define HGRUNT_SAW				( 1 << 4 )
#define HGRUNT_UNARMED			( 1 << 5 )
#define HGRUNT_RPG				( 1 << 6 )
#define HGRUNT_SNIPER			( 1 << 7 )
#define HGRUNT_NODROP			( 1 << 8 )

#define MEDIC_DEAGLE			( 1 << 0 )
#define MEDIC_GLOCK				( 1 << 1 )

#define TORCH_DEAGLE			( 1 << 0 )

#define MASSN_SNIPERRIFLE		( 1 << 3 )
#define MASSN_WEAPON_NONE		16	// fights bare-handed after losing the gun
#define MASSN_WEAPON_LOST		32

enum
{
	BG_WEAPON_PISTOL = 1,
	BG_WEAPON_DEAGLE,
	BG_WEAPON_SHOTGUN,
	BG_WEAPON_UZIS,
	BG_WEAPON_MP5,
	BG_WEAPON_SNIPER,
	BG_WEAPON_MINIGUN
};

enum
{
	HWGRUNT_SIDEARM_GLOCK = 1,
	HWGRUNT_SIDEARM_DEAGLE,
	HWGRUNT_SIDEARM_357
};

// Bodygroups
#define HEAD_GROUP				1
#define HEAD_SHOTGUN			2

#define GUN_GROUP				2
#define GUN_MP5					0
#define GUN_SHOTGUN				1
#define GUN_NONE				2
#define GUN_RPG					3
#define GUN_M16					4
#define GUN_SNIPER				5
#define MASSN_GUN_M16			3

#define OP4_HEAD_GASMASK		0
#define OP4_HEAD_MP				5
#define OP4_TORSO_GROUP			2
#define OP4_TORSO_SAW			1
#define OP4_TORSO_SHOTGUN		3
#define OP4_GUN_GROUP			3
#define OP4_GUN_SHOTGUN			1
#define OP4_GUN_SAW				2
#define OP4_GUN_NONE			3

#define MEDIC_HEAD_GROUP		2
#define MEDIC_GUN_DEAGLE		0
#define MEDIC_GUN_GLOCK			1
#define MEDIC_GUN_NEEDLE		2

#define TORCH_GUN_GROUP			2

#define HWGRUNT_GUN_GROUP		1
#define HWGRUNT_GUN_MINIGUN		0
#define HWGRUNT_GUN_NONE		1

// The "head" keyvalue is stored with this added, to tell a set head from none
#define HUMAN_HEAD_KEYVALUE_BIAS	25

// Animation events
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

#define MEDIC_AE_HOLSTER_GUN	15
#define MEDIC_AE_EQUIP_NEEDLE	16
#define MEDIC_AE_HOLSTER_NEEDLE	17
#define MEDIC_AE_EQUIP_GUN		18

#define TORCH_AE_HOLSTER_TORCH		17
#define TORCH_AE_HOLSTER_GUN		18
#define TORCH_AE_HOLSTER_BOTH		19
#define TORCH_AE_ACTIVATE_TORCH		20
#define TORCH_AE_DEACTIVATE_TORCH	21

#define HWGRUNT_AE_SHOOT		1
#define HWGRUNT_AE_RELOAD		2
#define HWGRUNT_AE_DROP_MINIGUN	3

#define BG_AE_SHOOT1			3
#define BG_AE_SHOOT2			4

// Hitgroups of the armored models
#define HITGROUP_HELMET			10
#define HITGROUP_HEADGEAR		11

#define bits_MOVESHOOTCAP_RPG	( 1 << 4 )
#define bits_COND_GRUNT_NOFIRE	( bits_COND_SPECIAL1 )

enum HumanCombatType
{
	HUMAN_OP4,
	HUMAN_MEDIC,
	HUMAN_TORCH,
	HUMAN_HEAVY,
	HUMAN_ROBOT,
	HUMAN_MASSN,
	HUMAN_BODYGUARD
};

struct HumanCombatData
{
	int m_cClipSize;
	int m_iBrassShell;
	int m_iShotgunShell;
	int m_iSawShell;
	int m_iHead;
	int m_iSidearm;
	int m_voicePitch;

	float m_flNextGrenadeCheck;
	float m_flNextPainTime;

	Vector m_vecTossVelocity;

	bool m_fThrowGrenade;
	bool m_fStanding;
	BOOL m_fGunHolstered;
	BOOL m_fTorchActive;

	EHANDLE m_hTorchBeam;
	EHANDLE m_hSavedTarget;

	float m_flHealTime;

	int  m_iSentence;		// spoken by the next speak task
	int  m_iPlayerDamage;	// hits taken from players before turning on them
	bool m_fFirstEncounter;
};

const char *HumanDefaultModel( CBaseMonster *monster, HumanCombatType type )
{
	switch ( type )
	{
		case HUMAN_OP4:
			return monster->IsPlayerAlly() ? "models/hgrunt_opforf.mdl" : "models/hgrunt_opfor.mdl";
		case HUMAN_MEDIC:
			return monster->IsPlayerAlly() ? "models/hgrunt_medicf.mdl" : "models/hgrunt_medic.mdl";
		case HUMAN_TORCH:
			return monster->IsPlayerAlly() ? "models/hgrunt_torchf.mdl" : "models/hgrunt_torch.mdl";
		case HUMAN_HEAVY:
			return monster->IsPlayerAlly() ? "models/hwgruntf.mdl" : "models/hwgrunt.mdl";
		case HUMAN_ROBOT:
			return monster->IsPlayerAlly() ? "models/rgruntf.mdl" : "models/rgrunt.mdl";
		case HUMAN_MASSN:
			return monster->IsPlayerAlly() ? "models/massnf.mdl" : "models/massn.mdl";
		default:
			return "models/bgman.mdl";
	}
}

const char *HumanDisplayName( CBaseMonster *monster, HumanCombatType type )
{
	switch ( type )
	{
		case HUMAN_OP4:
			return "Grunt";
		case HUMAN_MEDIC:
			return "Medic Grunt";
		case HUMAN_TORCH:
			return "Torch Grunt";
		case HUMAN_HEAVY:
			return "Heavy Weapons Grunt";
		case HUMAN_ROBOT:
			return "Robo Grunt";
		case HUMAN_MASSN:
			return "Male Assassin";
		default:
			return monster->IsPlayerAlly() ? "Body Guard" : "Agent";
	}
}

void HumanSetupFriendly( CBaseMonster *monster, HumanCombatType type )
{
	UTIL_SetModel( monster, HumanDefaultModel( monster, type ) );

	if ( type == HUMAN_ROBOT )
	{
		UTIL_SetSize( monster->pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );
		monster->pev->solid = SOLID_SLIDEBOX;
		monster->pev->movetype = MOVETYPE_STEP;
	}
}

void HumanPrecache( CBaseMonster *monster, HumanCombatData &data, HumanCombatType type )
{
	static const char *fgruntSounds[] = {
		"fgrunt/death1.wav", "fgrunt/death2.wav", "fgrunt/death3.wav",
		"fgrunt/gr_pain1.wav", "fgrunt/gr_pain2.wav", "fgrunt/gr_pain3.wav",
		"fgrunt/gr_pain4.wav", "fgrunt/gr_pain5.wav"
	};
	static const char *gruntDeaths[] = { "hgrunt/gr_die1.wav", "hgrunt/gr_die2.wav", "hgrunt/gr_die3.wav" };
	static const char *bodyguardSounds[] = {
		"bodyguard/pain1.ogg", "bodyguard/pain2.ogg", "bodyguard/pain3.ogg",
		"bodyguard/die1.ogg", "bodyguard/die2.ogg", "bodyguard/die3.ogg"
	};
	static const char *gunSounds[] = { "weapons/hks1.wav", "weapons/hks2.wav", "weapons/hks3.wav" };

	if ( FStringNull( monster->pev->model ) )
		UTIL_PrecacheModel( monster, HumanDefaultModel( monster, type ) );

	if ( type == HUMAN_OP4 )
	{
		PrecacheSound_Intercept( monster, "weapons/saw_fire1.wav" );
		for ( int i = 0; i < ARRAYSIZE( gunSounds ); i++ )
			PrecacheSound_Intercept( monster, gunSounds[i] );

		PrecacheSound_Intercept( monster, "weapons/m16_3round.wav" );
	}
	else if ( type == HUMAN_MEDIC || type == HUMAN_TORCH )
	{
		PrecacheSound_Intercept( monster, "otis/ot_attack.wav" );

		if ( type == HUMAN_TORCH )
			PrecacheSound_Intercept( monster, "hgrunt/gr_mgun1.wav" );
	}

	if ( type <= HUMAN_TORCH )
	{
		for ( int i = 0; i < ARRAYSIZE( fgruntSounds ); i++ )
			PrecacheSound_Intercept( monster, fgruntSounds[i] );
	}
	else if ( type == HUMAN_ROBOT )
	{
		PrecacheSound_Intercept( monster, "turret/tu_die.wav" );
		PrecacheSound_Intercept( monster, "turret/tu_die2.wav" );
	}
	else if ( type == HUMAN_BODYGUARD )
	{
		for ( int i = 0; i < ARRAYSIZE( bodyguardSounds ); i++ )
			PrecacheSound_Intercept( monster, bodyguardSounds[i] );
	}
	else
	{
		if ( type == HUMAN_HEAVY )
		{
			static const char *painSounds[] = {
				"hgrunt/gr_pain5.wav", "hgrunt/gr_pain4.wav", "hgrunt/gr_pain3.wav",
				"hgrunt/gr_pain2.wav", "hgrunt/gr_pain1.wav"
			};

			for ( int i = 0; i < ARRAYSIZE( painSounds ); i++ )
				PrecacheSound_Intercept( monster, painSounds[i] );
		}

		for ( int i = 0; i < ARRAYSIZE( gruntDeaths ); i++ )
			PrecacheSound_Intercept( monster, gruntDeaths[i] );
	}

	if ( type == HUMAN_HEAVY || type == HUMAN_BODYGUARD )
	{
		PrecacheSound_Intercept( monster, "hassault/hw_spinup.wav" );
		PrecacheSound_Intercept( monster, "hassault/hw_spindown.wav" );
		PrecacheSound_Intercept( monster, "hassault/hw_spin.wav" );
		PrecacheSound_Intercept( monster, "weapons/pl_gun3.wav" );
		PrecacheSound_Intercept( monster, "otis/ot_attack.wav" );
		PrecacheSound_Intercept( monster, "weapons/357_shot1.wav" );
		PrecacheSound_Intercept( monster, "minigun/hw_shoot1.wav" );
	}

	if ( type == HUMAN_MASSN || type == HUMAN_ROBOT || type == HUMAN_BODYGUARD )
	{
		for ( int i = 0; i < ARRAYSIZE( gunSounds ); i++ )
			PrecacheSound_Intercept( monster, gunSounds[i] );

		PrecacheSound_Intercept( monster, "weapons/m16_3round.wav" );
		PrecacheSound_Intercept( monster, "weapons/sniper_fire.wav" );
		PrecacheSound_Intercept( monster, "weapons/sniper_reload_first_seq.wav" );
		PrecacheSound_Intercept( monster, "weapons/sniper_bolt1.wav" );
	}

	PrecacheSound_Intercept( monster, "hgrunt/gr_reload1.wav" );
	PrecacheSound_Intercept( monster, "zombie/claw_miss2.wav" );

	if ( type != HUMAN_MEDIC )
		PrecacheSound_Intercept( monster, "weapons/glauncher.wav" );

	if ( type != HUMAN_MEDIC && type != HUMAN_TORCH && type != HUMAN_HEAVY )
	{
		PrecacheSound_Intercept( monster, "hgrunt/gr_mgun1.wav" );
		PrecacheSound_Intercept( monster, "weapons/sbarrel1.wav" );
	}

	if ( type == HUMAN_OP4 )
	{
		PrecacheSound_Intercept( monster, "weapons/saw_reload.wav" );
		data.m_iSawShell = UTIL_PrecacheModel( monster, "models/saw_shell.mdl" );
		PrecacheSound_Intercept( monster, "fgrunt/medic.wav" );
	}

	if ( type == HUMAN_ROBOT )
	{
		PrecacheSound_Intercept( monster, "buttons/button3.wav" );
		PrecacheSound_Intercept( monster, "buttons/button2.wav" );
		PrecacheSound_Intercept( monster, "debris/beamstart14.wav" );
		UTIL_PrecacheModel( monster, "models/chromegibs.mdl" );
		UTIL_PrecacheModel( monster, "models/computergibs.mdl" );
	}

	if ( type == HUMAN_MASSN )
	{
		PrecacheSound_Intercept( monster, "barney/teamup2.wav" );
		PrecacheSound_Intercept( monster, "barney/standguard.wav" );
		PrecacheSound_Intercept( monster, "barney/heyfella.wav" );
		PrecacheSound_Intercept( monster, "barney/stop2.wav" );
	}

	if ( type == HUMAN_TORCH )
		UTIL_PrecacheModel( monster, "sprites/lgtning.spr" );

	if ( type == HUMAN_BODYGUARD )
		PrecacheSound_Intercept( monster, "weapons/uzi/fire_both1.wav" );

	data.m_iBrassShell = UTIL_PrecacheModel( monster, "models/shell.mdl" );

	if ( type == HUMAN_OP4 || type == HUMAN_ROBOT || type == HUMAN_BODYGUARD )
		data.m_iShotgunShell = UTIL_PrecacheModel( monster, "models/shotgunshell.mdl" );

	data.m_voicePitch = type == HUMAN_ROBOT ? 100 : RANDOM_LONG( 0, 1 ) ? 93 + RANDOM_LONG( 0, 7 )
	                                                                    : 100;
}

void HumanSpawn( CBaseMonster *monster, HumanCombatData &data, HumanCombatType type, bool headSet = true )
{
	entvars_t *pev = monster->pev;

	if ( !monster->SetupModel() )
		HumanSetupFriendly( monster, type );

	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	monster->m_bloodColor = type == HUMAN_ROBOT ? DONT_BLEED : BLOOD_COLOR_RED;
	pev->effects = 0;

	if ( pev->health == 0 )
		pev->health = type == HUMAN_BODYGUARD ? 100 : CVAR_GET_FLOAT( type == HUMAN_HEAVY ? "sk_hwgrunt_health" : "sk_hgrunt_health" );

	monster->m_flFieldOfView = type == HUMAN_ROBOT ? 0.2f : -0.7f;
	monster->m_MonsterState = MONSTERSTATE_NONE;
	monster->m_afCapability = bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;

	if ( type != HUMAN_BODYGUARD )
		monster->m_afCapability |= bits_CAP_SQUAD;

	if ( type == HUMAN_HEAVY || type == HUMAN_BODYGUARD )
		monster->m_afCapability |= bits_CAP_HEAR;

	if ( type != HUMAN_HEAVY && type != HUMAN_ROBOT )
		monster->m_afCapability |= bits_CAP_USE_TANK;

	monster->m_afMoveShootCap = type == HUMAN_BODYGUARD ? bits_MOVESHOOTCAP_RANGE_ATTACK1 : 0;
	monster->m_fCheckCreatureDanger = type != HUMAN_ROBOT && type != HUMAN_HEAVY;
	monster->m_HackedGunPos = Vector( 0, 0, 55 );
	data.m_flNextGrenadeCheck = gpGlobals->time + 1;
	data.m_flNextPainTime = gpGlobals->time;
	data.m_fStanding = FALSE;
	data.m_fThrowGrenade = FALSE;
	data.m_cClipSize = 30;

	if ( type == HUMAN_BODYGUARD )
	{
		static const int clips[] = { 0, 17, 7, 8, 32, 30, 1, 0 };

		pev->view_ofs = Vector( 0, 0, 50 );
		pev->body = 0;
		if ( RANDOM_LONG( 0, 10 ) > 8 )
			pev->skin = 1;

		if ( !pev->weapons )
			pev->weapons = RANDOM_LONG( BG_WEAPON_PISTOL, BG_WEAPON_MINIGUN );

		if ( pev->weapons >= BG_WEAPON_PISTOL && pev->weapons <= BG_WEAPON_MINIGUN )
			data.m_cClipSize = clips[pev->weapons];

		monster->SetBodygroup( GUN_GROUP, pev->weapons );
		if ( pev->weapons == BG_WEAPON_SNIPER )
			monster->m_flDistLook = 4096;
	}
	else if ( type == HUMAN_HEAVY )
	{
		if ( pev->scale <= 0 )
			pev->scale = 1;

		pev->view_ofs = Vector( 0, 0, 28 );
		if ( pev->weapons == HGRUNT_NODROP )
			pev->weapons = HWGRUNT_SIDEARM_GLOCK;
		else if ( pev->weapons < 0 || pev->weapons > 4 )
			pev->weapons = 0;

		data.m_iSidearm = pev->weapons ? pev->weapons : RANDOM_LONG( HWGRUNT_SIDEARM_GLOCK, HWGRUNT_SIDEARM_357 );
		data.m_cClipSize = data.m_iSidearm == HWGRUNT_SIDEARM_GLOCK ? 17 : data.m_iSidearm == HWGRUNT_SIDEARM_DEAGLE ? 7
		                                                                    : 6;
		monster->SetBodygroup( HWGRUNT_GUN_GROUP, HWGRUNT_GUN_MINIGUN );
	}
	else if ( type == HUMAN_MEDIC )
	{
		if ( !pev->weapons )
			pev->weapons = RANDOM_LONG( 0, 50 ) > 35 ? RANDOM_LONG( MEDIC_DEAGLE, MEDIC_GLOCK ) : MEDIC_GLOCK;

		if ( pev->weapons & MEDIC_DEAGLE )
		{
			monster->SetBodygroup( OP4_GUN_GROUP, MEDIC_GUN_DEAGLE );
			data.m_cClipSize = 7;
		}
		else if ( pev->weapons & MEDIC_GLOCK )
		{
			monster->SetBodygroup( OP4_GUN_GROUP, MEDIC_GUN_GLOCK );
			data.m_cClipSize = 17;
		}

		data.m_iHead -= HUMAN_HEAD_KEYVALUE_BIAS;
		monster->SetBodygroup( MEDIC_HEAD_GROUP, data.m_iHead < 0 ? RANDOM_LONG( 0, 99 ) >= 80 : data.m_iHead );
	}
	else if ( type == HUMAN_TORCH )
	{
		data.m_cClipSize = 7;
		data.m_fGunHolstered = !( pev->weapons & TORCH_DEAGLE );
		monster->SetBodygroup( TORCH_GUN_GROUP, data.m_fGunHolstered );
	}
	else
	{
		if ( type == HUMAN_MASSN )
		{
			monster->SetBodygroup( HEAD_GROUP, data.m_iHead == -1 ? RANDOM_LONG( 0, 2 ) : data.m_iHead );
			if ( pev->weapons == HGRUNT_NODROP )
				pev->weapons |= MASSN_SNIPERRIFLE;
		}

		if ( !pev->weapons )
			pev->weapons = HGRUNT_9MMAR | HGRUNT_HANDGRENADE;

		if ( pev->weapons & HGRUNT_SHOTGUN )
		{
			data.m_cClipSize = type == HUMAN_MASSN ? 5 : 8;
			monster->SetBodygroup( GUN_GROUP, type == HUMAN_OP4 ? OP4_TORSO_SHOTGUN : GUN_SHOTGUN );
			if ( type == HUMAN_OP4 )
				monster->SetBodygroup( OP4_GUN_GROUP, OP4_GUN_SHOTGUN );
		}
		else if ( type == HUMAN_OP4 && ( pev->weapons & HGRUNT_SAW ) )
		{
			data.m_cClipSize = 200;
			monster->SetBodygroup( OP4_TORSO_GROUP, OP4_TORSO_SAW );
			monster->SetBodygroup( OP4_GUN_GROUP, OP4_GUN_SAW );
		}
		else if ( type == HUMAN_ROBOT && ( pev->weapons & HGRUNT_RPG ) )
		{
			data.m_cClipSize = 1;
			monster->m_afMoveShootCap = bits_MOVESHOOTCAP_RPG;
			monster->SetBodygroup( GUN_GROUP, GUN_RPG );
		}
		else if ( type == HUMAN_ROBOT && ( pev->weapons & HGRUNT_SNIPER ) )
		{
			data.m_cClipSize = 5;
			monster->SetBodygroup( GUN_GROUP, GUN_SNIPER );
		}
		else if ( type == HUMAN_OP4 && ( pev->weapons & HGRUNT_UNARMED ) )
			monster->SetBodygroup( OP4_GUN_GROUP, OP4_GUN_NONE );
		else if ( monster->LookupSequence( "standing_m16a2" ) != -1 )
			monster->SetBodygroup( type == HUMAN_OP4 ? OP4_GUN_GROUP : GUN_GROUP, GUN_M16 );

		if ( type == HUMAN_MASSN && ( pev->weapons == MASSN_WEAPON_NONE || pev->weapons == MASSN_WEAPON_LOST ) )
		{
			pev->weapons = MASSN_WEAPON_NONE;
			monster->SetBodygroup( GUN_GROUP, GUN_NONE );
			monster->m_fCheckCreatureDanger = false;
		}
		else if ( pev->weapons & HGRUNT_GRENADELAUNCHER )
		{
			pev->weapons = HGRUNT_9MMAR | HGRUNT_GRENADELAUNCHER;
			monster->SetBodygroup( type == HUMAN_OP4 ? OP4_GUN_GROUP : GUN_GROUP, type == HUMAN_MASSN ? MASSN_GUN_M16 : GUN_MP5 );
		}

		if ( type == HUMAN_OP4 )
		{
			if ( !headSet )
				data.m_iHead = 0;

			data.m_iHead -= HUMAN_HEAD_KEYVALUE_BIAS;
			if ( data.m_iHead >= 0 && data.m_iHead <= 7 )
				monster->SetBodygroup( HEAD_GROUP, data.m_iHead );
			else if ( pev->weapons & HGRUNT_SHOTGUN )
				monster->SetBodygroup( HEAD_GROUP, HEAD_SHOTGUN );
		}
	}

	monster->m_cAmmoLoaded = data.m_cClipSize;

	if ( !monster->m_FormattedName )
		monster->m_FormattedName = MAKE_STRING( HumanDisplayName( monster, type ) );

	monster->MonsterInit();

	if ( type == HUMAN_MASSN && ( pev->weapons & MASSN_SNIPERRIFLE ) )
		monster->m_flDistLook = 4096;

	if ( monster->IsPlayerAlly() && ( type == HUMAN_OP4 || type == HUMAN_TORCH || !( pev->spawnflags & SF_MONSTER_PREDISASTER ) ) )
	{
		monster->SetUse( &CBaseMonster::CallFollowerPlayerUse );
		monster->m_fCanPlayerFollow = true;
	}
}

Vector HumanGunPosition( CBaseMonster *monster, HumanCombatData &data, HumanCombatType type )
{
	if ( type == HUMAN_BODYGUARD )
		return monster->pev->origin + Vector( 0, 0, 55 );

	if ( data.m_fStanding && !( monster->pev->weapons & HGRUNT_RPG ) )
		return monster->pev->origin + Vector( 0, 0, 60 );

	return monster->pev->origin + Vector( 0, 0, ( monster->pev->weapons & HGRUNT_SNIPER ) ? 64 : 48 );
}

BOOL HumanCheckRangeAttack1( CBaseMonster *monster, float flDot, float flDist )
{
	if ( monster->m_hEnemy == NULL || flDot < 0.5f || flDist > monster->m_flDistLook || monster->HasConditions( bits_COND_ENEMY_OCCLUDED ) )
		return FALSE;

	TraceResult tr;
	Vector source = monster->GetGunPosition();
	Vector target = monster->m_hEnemy->BodyTarget( source );

	UTIL_TraceLine( source, target, dont_ignore_monsters, monster->edict(), &tr );

	return tr.flFraction == 1.0f || tr.pHit == monster->m_hEnemy->edict();
}

BOOL HumanCheckMeleeAttack1( CBaseMonster *monster, float flDot, float flDist )
{
	if ( monster->m_hEnemy == NULL )
		return FALSE;

	int classification = monster->m_hEnemy->Classify();

	return flDist <= 64 && flDot >= 0.7f && classification != CLASS_ALIEN_BIOWEAPON && classification != CLASS_PLAYER_BIOWEAPON;
}

BOOL HumanCheckRangeAttack2( CSquadMonster *monster, HumanCombatData &data, float flDot, float flDist )
{
	entvars_t *pev = monster->pev;
	CBaseEntity *enemy = monster->m_hEnemy;

	if ( !( pev->weapons & ( HGRUNT_HANDGRENADE | HGRUNT_GRENADELAUNCHER ) ) )
		return FALSE;

	// if the grunt is moving, don't check
	if ( monster->m_flGroundSpeed != 0 )
	{
		data.m_fThrowGrenade = FALSE;
		return FALSE;
	}

	if ( gpGlobals->time < data.m_flNextGrenadeCheck )
		return data.m_fThrowGrenade;

	if ( !( enemy->pev->flags & FL_ONGROUND ) && enemy->pev->waterlevel == 0 && monster->m_vecEnemyLKP.z > pev->absmax.z )
	{
		data.m_fThrowGrenade = FALSE;
		return FALSE;
	}

	Vector target;

	if ( pev->weapons & HGRUNT_HANDGRENADE )
	{
		if ( RANDOM_LONG( 0, 1 ) )
		{
			Vector feet = enemy->IsBSPModel() ? enemy->Center() : enemy->pev->origin;

			target = Vector( feet.x, feet.y, enemy->pev->absmin.z );
		}
		else
		{
			target = monster->m_vecEnemyLKP;
		}
	}
	else
	{
		Vector base = enemy->IsBSPModel() ? enemy->Center() : enemy->pev->origin;

		target = monster->m_vecEnemyLKP + ( enemy->BodyTarget( pev->origin ) - base );

		if ( monster->HasConditions( bits_COND_SEE_ENEMY ) )
			target = target + ( ( target - pev->origin ).Length() / gSkillData.hgruntGrenadeSpeed ) * enemy->pev->velocity;
	}

	if ( monster->SquadMemberInRange( target, 256 ) || ( target - pev->origin ).Length2D() <= 256 )
	{
		data.m_flNextGrenadeCheck = gpGlobals->time + 1;
		data.m_fThrowGrenade = FALSE;
		return FALSE;
	}

	Vector toss;

	if ( pev->weapons & HGRUNT_HANDGRENADE )
		toss = VecCheckToss( pev, monster->GetGunPosition(), target, 1.0 );
	else
		toss = VecCheckThrow( pev, monster->GetGunPosition(), target, gSkillData.hgruntGrenadeSpeed, 0.7 );

	if ( toss != g_vecZero )
	{
		data.m_vecTossVelocity = toss;
		data.m_fThrowGrenade = TRUE;
		data.m_flNextGrenadeCheck = gpGlobals->time + 0.3;
		return TRUE;
	}

	data.m_fThrowGrenade = FALSE;
	data.m_flNextGrenadeCheck = gpGlobals->time + 1;
	return FALSE;
}

void HumanCheckAmmo( CBaseMonster *monster, HumanCombatData &data, HumanCombatType type )
{
	if ( type == HUMAN_HEAVY && !data.m_fGunHolstered )
		return;

	if ( type == HUMAN_BODYGUARD && monster->pev->weapons == BG_WEAPON_MINIGUN )
		return;

	if ( monster->m_cAmmoLoaded <= 0 )
		monster->SetConditions( bits_COND_NO_AMMO_LOADED );
}

void HumanShoot( CBaseMonster *monster, HumanCombatData &data, HumanCombatType type )
{
	if ( monster->m_hEnemy == NULL )
		return;

	Vector source = monster->GetGunPosition();
	Vector direction = monster->ShootAtEnemy( source );
	int weapon = monster->pev->weapons;
	int bullet = BULLET_MONSTER_MP5;
	int shots = 1;
	int shell = data.m_iBrassShell;
	int bounce = TE_BOUNCE_SHELL;
	float distance = 2048;
	Vector spread = VECTOR_CONE_5DEGREES;
	const char *sound = "hgrunt/gr_mgun1.wav";

	if ( type == HUMAN_BODYGUARD )
	{
		static const char *sounds[] = {
			"", "weapons/pl_gun3.wav", "otis/ot_attack.wav", "weapons/sbarrel1.wav",
			"weapons/uzi/fire_both1.wav", "weapons/hks1.wav", "weapons/sniper_fire.wav", "minigun/hw_shoot1.wav"
		};

		if ( weapon < BG_WEAPON_PISTOL || weapon > BG_WEAPON_MINIGUN )
			return;

		sound = sounds[weapon];
		if ( weapon == BG_WEAPON_PISTOL )
			bullet = BULLET_MONSTER_9MM;
		else if ( weapon == BG_WEAPON_DEAGLE )
			bullet = BULLET_MONSTER_EAGLE;
		else if ( weapon == BG_WEAPON_SHOTGUN )
			bullet = BULLET_MONSTER_BUCKSHOT;
		else if ( weapon == BG_WEAPON_UZIS )
			shots = 2;
		else if ( weapon == BG_WEAPON_SNIPER )
			bullet = BULLET_MONSTER_SNIPER;
		else if ( weapon == BG_WEAPON_MINIGUN )
			bullet = BULLET_MONSTER_12MM;
	}
	else if ( type == HUMAN_MEDIC || type == HUMAN_TORCH )
	{
		bullet = type == HUMAN_TORCH || ( weapon & MEDIC_DEAGLE ) ? BULLET_MONSTER_EAGLE : BULLET_MONSTER_9MM;
		spread = bullet == BULLET_MONSTER_EAGLE && type == HUMAN_MEDIC ? VECTOR_CONE_2DEGREES : VECTOR_CONE_1DEGREES;
		sound = bullet == BULLET_MONSTER_EAGLE ? "otis/ot_attack.wav" : "weapons/pl_gun3.wav";
		if ( type == HUMAN_TORCH )
			distance = 1024;
	}
	else if ( type == HUMAN_HEAVY )
	{
		if ( !data.m_fGunHolstered )
		{
			bullet = BULLET_MONSTER_12MM;
			sound = "minigun/hw_shoot1.wav";
		}
		else
		{
			bullet = data.m_iSidearm == HWGRUNT_SIDEARM_GLOCK ? BULLET_MONSTER_9MM : data.m_iSidearm == HWGRUNT_SIDEARM_DEAGLE ? BULLET_MONSTER_EAGLE : BULLET_PLAYER_357;
			spread = data.m_iSidearm == HWGRUNT_SIDEARM_DEAGLE ? VECTOR_CONE_2DEGREES : VECTOR_CONE_1DEGREES;
			sound = data.m_iSidearm == HWGRUNT_SIDEARM_GLOCK ? "weapons/pl_gun3.wav" : data.m_iSidearm == HWGRUNT_SIDEARM_DEAGLE ? "otis/ot_attack.wav"
			                                                                            : "weapons/357_shot1.wav";
		}
		distance = 4096;
	}
	else if ( type == HUMAN_ROBOT && ( weapon & HGRUNT_RPG ) )
	{
		CBaseEntity *rocket = CBaseEntity::Create( "rpg_rocket", source, UTIL_VecToAngles( direction ), false, monster->edict() );

		if ( rocket )
			rocket->pev->velocity = direction * 600;

		monster->m_cAmmoLoaded--;
		return;
	}
	else if ( type == HUMAN_ROBOT && ( weapon & HGRUNT_SNIPER ) )
	{
		bullet = BULLET_MONSTER_SNIPER;
		spread = VECTOR_CONE_1DEGREES;
		sound = "weapons/sniper_fire.wav";
		distance = 4096;
	}
	else if ( weapon & HGRUNT_SHOTGUN )
		bullet = BULLET_MONSTER_BUCKSHOT;
	else if ( weapon & HGRUNT_GRENADELAUNCHER )
		sound = "weapons/m16_3round.wav";

	if ( bullet == BULLET_MONSTER_BUCKSHOT )
	{
		shots = (int) CVAR_GET_FLOAT( "sk_hgrunt_pellets" );
		spread = VECTOR_CONE_15DEGREES;
		shell = data.m_iShotgunShell;
		bounce = TE_BOUNCE_SHOTSHELL;
		sound = "weapons/sbarrel1.wav";
	}

	UTIL_MakeVectors( monster->pev->angles );
	Vector shellVelocity = gpGlobals->v_right * RANDOM_FLOAT( 40, 90 ) + gpGlobals->v_up * RANDOM_FLOAT( 75, 200 ) + gpGlobals->v_forward * RANDOM_FLOAT( -40, 40 );

	EjectBrass( source - direction * 24, shellVelocity, monster->pev->angles.y, shell, bounce );
	monster->FireBullets( shots, source, direction, spread, distance, bullet );
	EMIT_SOUND( monster->edict(), CHAN_WEAPON, sound, 1, ATTN_NORM );
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, monster->pev->origin, 1024, 0.3 );
	monster->pev->effects |= EF_MUZZLEFLASH;
	monster->m_cAmmoLoaded--;

	Vector aim = UTIL_VecToAngles( direction );

	monster->SetBlending( 0, aim.x );
}

void HumanKick( CBaseMonster *monster )
{
	TraceResult tr;
	Vector source = monster->pev->origin + Vector( 0, 0, monster->pev->size.z * 0.5f );

	UTIL_MakeVectors( monster->pev->angles );
	UTIL_TraceHull( source, source + gpGlobals->v_forward * 70, dont_ignore_monsters, head_hull, monster->edict(), &tr );
	CBaseEntity *target = CBaseEntity::Instance( tr.pHit );

	if ( !target )
		return;

	target->pev->punchangle.x = 45;
	target->pev->velocity = target->pev->velocity + gpGlobals->v_forward * 100 + gpGlobals->v_up * 50;
	target->TakeDamage( monster->pev, monster->pev, CVAR_GET_FLOAT( "sk_hgrunt_kick" ), DMG_CLUB );
}

void HumanTorch( CBaseMonster *monster, HumanCombatData &data, BOOL active )
{
	if ( data.m_hTorchBeam != NULL )
		UTIL_Remove( data.m_hTorchBeam );

	data.m_hTorchBeam = NULL;
	data.m_fTorchActive = active;

	if ( !active )
		return;

	data.m_fGunHolstered = TRUE;

	CBeam *beam = CBeam::BeamCreate( "sprites/lgtning.spr", 30 );

	if ( !beam )
		return;

	beam->EntsInit( monster->entindex(), monster->entindex() );
	beam->SetStartAttachment( 3 );
	beam->SetEndAttachment( 4 );
	beam->SetColor( 96, 96, 255 );
	beam->SetBrightness( 255 );
	beam->SetScrollRate( 5 );
	beam->SetNoise( 8 );
	beam->SetFlags( BEAM_FSHADEOUT );
	data.m_hTorchBeam = beam;
}

const char *HumanDroppedWeapon( HumanCombatType type, int weapons )
{
	if ( type == HUMAN_TORCH )
		return "weapon_eagle";

	if ( type == HUMAN_ROBOT )
	{
		if ( weapons & HGRUNT_RPG )
			return "weapon_rpg";

		if ( weapons & HGRUNT_SNIPER )
			return "weapon_sniperrifle";
	}

	if ( type == HUMAN_MASSN )
	{
		if ( weapons & MASSN_SNIPERRIFLE )
			return "weapon_sniperrifle";

		if ( weapons == MASSN_WEAPON_NONE )
			return "weapon_crowbar";
	}

	return weapons & HGRUNT_SHOTGUN ? "weapon_shotgun" : "weapon_9mmAR";
}

BOOL HumanAnimEvent( CBaseMonster *monster, HumanCombatData &data, HumanCombatType type, MonsterEvent_t *event )
{
	if ( type == HUMAN_BODYGUARD )
	{
		if ( event->event != BG_AE_SHOOT1 && event->event != BG_AE_SHOOT2 )
			return FALSE;

		HumanShoot( monster, data, type );
		return TRUE;
	}

	if ( type == HUMAN_HEAVY )
	{
		if ( event->event == HWGRUNT_AE_SHOOT )
			HumanShoot( monster, data, type );
		else if ( event->event == HWGRUNT_AE_RELOAD )
		{
			monster->m_cAmmoLoaded = data.m_cClipSize;
			monster->ClearConditions( bits_COND_NO_AMMO_LOADED );
		}
		else if ( event->event == HWGRUNT_AE_DROP_MINIGUN && !data.m_fGunHolstered )
		{
			monster->DropItem( "weapon_minigun", monster->GetGunPosition(), monster->pev->angles );
			data.m_fGunHolstered = TRUE;
			monster->m_cAmmoLoaded = data.m_cClipSize;
			monster->SetBodygroup( HWGRUNT_GUN_GROUP, HWGRUNT_GUN_NONE );
		}
		else
			return FALSE;

		return TRUE;
	}

	if ( type == HUMAN_TORCH && event->event >= TORCH_AE_HOLSTER_TORCH && event->event <= TORCH_AE_DEACTIVATE_TORCH )
	{
		if ( event->event >= TORCH_AE_ACTIVATE_TORCH )
			HumanTorch( monster, data, event->event == TORCH_AE_ACTIVATE_TORCH );
		else
		{
			data.m_fGunHolstered = event->event != TORCH_AE_HOLSTER_TORCH;
			monster->SetBodygroup( TORCH_GUN_GROUP, data.m_fGunHolstered );
		}

		return TRUE;
	}

	switch ( event->event )
	{
		case HGRUNT_AE_RELOAD:
			EMIT_SOUND( monster->edict(), CHAN_WEAPON, "hgrunt/gr_reload1.wav", 1, ATTN_NORM );
			monster->m_cAmmoLoaded = data.m_cClipSize;
			monster->ClearConditions( bits_COND_NO_AMMO_LOADED );
			return TRUE;
		case HGRUNT_AE_KICK:
			HumanKick( monster );
			return TRUE;
		case HGRUNT_AE_BURST1:
			HumanShoot( monster, data, type );
			return TRUE;
		case HGRUNT_AE_BURST2:
		case HGRUNT_AE_BURST3:
			if ( ( type == HUMAN_ROBOT || type == HUMAN_MASSN ) && ( monster->pev->weapons & ( HGRUNT_9MMAR | HGRUNT_GRENADELAUNCHER ) ) )
				HumanShoot( monster, data, type );
			return TRUE;
		case HGRUNT_AE_GREN_TOSS:
			CGrenade::ShootTimed( monster->pev, monster->GetGunPosition(), data.m_vecTossVelocity, 3.5 );
			data.m_fThrowGrenade = FALSE;
			data.m_flNextGrenadeCheck = gpGlobals->time + 6;
			return TRUE;
		case HGRUNT_AE_GREN_LAUNCH:
			EMIT_SOUND( monster->edict(), CHAN_WEAPON, "weapons/glauncher.wav", 0.8, ATTN_NORM );
			CGrenade::ShootContact( monster->pev, monster->GetGunPosition(), data.m_vecTossVelocity );
			data.m_fThrowGrenade = FALSE;
			data.m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 5, 10 );
			return TRUE;
		case HGRUNT_AE_GREN_DROP:
			UTIL_MakeVectors( monster->pev->angles );
			CGrenade::ShootTimed( monster->pev, monster->pev->origin + gpGlobals->v_forward * 17 - gpGlobals->v_right * 27 + gpGlobals->v_up * 6, g_vecZero, 3 );
			return TRUE;
		case HGRUNT_AE_DROP_GUN:
		{
			const char *weapon = HumanDroppedWeapon( type, monster->pev->weapons );

			if ( weapon )
				monster->DropItem( weapon, monster->GetGunPosition(), monster->pev->angles );

			monster->SetBodygroup( GUN_GROUP, GUN_NONE );
			return TRUE;
		}
		default:
			return FALSE;
	}
}

enum
{
	TASK_OP4_MEDIC_SAY_HEAL = LAST_TALKMONSTER_TASK,
	TASK_OP4_MEDIC_RESET_HEAL,
	TASK_OP4_MEDIC_HEAL,
	TASK_OP4_FACE_TOSS_DIR = LAST_TALKMONSTER_TASK + 4,
	TASK_OP4_SPEAK_SENTENCE,
	TASK_OP4_CHECK_FIRE,
	TASK_OP4_MEDIC_REVIVE
};

enum
{
	TASK_MASSN_FACE_TOSS_DIR = LAST_COMMON_TASK + 1,
	TASK_MASSN_CHECK_FIRE,
	TASK_MASSN_WAIT_SNIPER = LAST_COMMON_TASK + 4
};

enum
{
	SCHED_OP4_SUPPRESS = LAST_TALKMONSTER_SCHEDULE + 1,
	SCHED_OP4_ESTABLISH_LINE_OF_FIRE,
	SCHED_OP4_COVER_AND_RELOAD,
	SCHED_OP4_SWEEP,
	SCHED_OP4_FOUND_ENEMY,
	SCHED_OP4_REPEL,
	SCHED_OP4_REPEL_ATTACK,
	SCHED_OP4_REPEL_LAND,
	SCHED_OP4_WAIT_IN_COVER,
	SCHED_OP4_TAKECOVER_FAILED,
	SCHED_OP4_ELOF_FAIL,
	SCHED_OP4_MEDIC_NEEDLE_DISARM
};

enum
{
	SCHED_MASSN_SUPPRESS = LAST_COMMON_SCHEDULE + 1,
	SCHED_MASSN_ESTABLISH_LINE_OF_FIRE,
	SCHED_MASSN_COVER_AND_RELOAD,
	SCHED_MASSN_SWEEP,
	SCHED_MASSN_FOUND_ENEMY,
	SCHED_MASSN_REPEL,
	SCHED_MASSN_REPEL_ATTACK,
	SCHED_MASSN_REPEL_LAND,
	SCHED_MASSN_WAIT_FACE_ENEMY,
	SCHED_MASSN_TAKECOVER_FAILED,
	SCHED_MASSN_ELOF_FAIL
};

static Task_t tlOp4Fail[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 1 },
};

static Schedule_t slOp4Fail[] = {
	{
		tlOp4Fail,
		ARRAYSIZE( tlOp4Fail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK2 |
		bits_COND_NEW_ENEMY,
		0,
		"Grunt Fail"
	}
};

static Task_t tlOp4CombatFail[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 0.5f },
};

static Schedule_t slOp4CombatFail[] = {
	{
		tlOp4CombatFail,
		ARRAYSIZE( tlOp4CombatFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"Grunt Combat Fail"
	}
};

static Task_t tlOp4VictoryDance[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_WAIT, 1.5f },
	{ TASK_GET_PATH_TO_ENEMY_CORPSE, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
};

static Schedule_t slOp4VictoryDance[] = {
	{
		tlOp4VictoryDance,
		ARRAYSIZE( tlOp4VictoryDance ),
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntVictoryDance"
	}
};

static Task_t tlOp4EstablishLineOfFire[] = {
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_OP4_ELOF_FAIL },
	{ TASK_GET_PATH_TO_ENEMY, 0 },
	{ TASK_OP4_SPEAK_SENTENCE, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_AP, 0 },
};

static Schedule_t slOp4EstablishLineOfFire[] = {
	{
		tlOp4EstablishLineOfFire,
		ARRAYSIZE( tlOp4EstablishLineOfFire ),
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

static Task_t tlOp4FoundEnemy[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_SIGNAL1 },
};

static Schedule_t slOp4FoundEnemy[] = {
	{
		tlOp4FoundEnemy,
		ARRAYSIZE( tlOp4FoundEnemy ),
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntFoundEnemy"
	}
};

static Task_t tlOp4CombatFace[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_WAIT, 1 },
	{ TASK_SET_SCHEDULE, (float) SCHED_OP4_SWEEP },
};

static Schedule_t slOp4CombatFace[] = {
	{
		tlOp4CombatFace,
		ARRAYSIZE( tlOp4CombatFace ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD,
		0,
		"Combat Face"
	}
};

static Task_t tlOp4SignalSuppress[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_SIGNAL2 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
};

static Schedule_t slOp4SignalSuppress[] = {
	{
		tlOp4SignalSuppress,
		ARRAYSIZE( tlOp4SignalSuppress ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"SignalSuppress"
	}
};

static Task_t tlOp4Suppress[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
};

static Schedule_t slOp4Suppress[] = {
	{
		tlOp4Suppress,
		ARRAYSIZE( tlOp4Suppress ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"Suppress"
	}
};

static Task_t tlOp4WaitInCover[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_WAIT_FACE_ENEMY, 1 },
};

static Schedule_t slOp4WaitInCover[] = {
	{
		tlOp4WaitInCover,
		ARRAYSIZE( tlOp4WaitInCover ),
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

static Task_t tlOp4TakeCover[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_OP4_TAKECOVER_FAILED },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_OP4_SPEAK_SENTENCE, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_SET_SCHEDULE, (float) SCHED_OP4_WAIT_IN_COVER },
};

static Schedule_t slOp4TakeCover[] = {
	{
		tlOp4TakeCover,
		ARRAYSIZE( tlOp4TakeCover ),
		0,
		0,
		"TakeCover"
	}
};

static Task_t tlOp4GrenadeCover[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FIND_COVER_FROM_ENEMY, 30 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_SPECIAL_ATTACK1 },
	{ TASK_CLEAR_MOVE_WAIT, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_OP4_WAIT_IN_COVER },
};

static Schedule_t slOp4GrenadeCover[] = {
	{
		tlOp4GrenadeCover,
		ARRAYSIZE( tlOp4GrenadeCover ),
		0,
		0,
		"GrenadeCover"
	}
};

static Task_t tlOp4TossGrenadeCover[] = {
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RANGE_ATTACK2, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_TAKE_COVER_FROM_ENEMY },
};

static Schedule_t slOp4TossGrenadeCover[] = {
	{
		tlOp4TossGrenadeCover,
		ARRAYSIZE( tlOp4TossGrenadeCover ),
		0,
		0,
		"TossGrenadeCover"
	}
};

static Task_t tlOp4TakeCoverFromBestSound[] = {
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_COWER },
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FIND_COVER_FROM_BEST_SOUND, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_GRENADE, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_TURN_LEFT, 179 },
};

static Schedule_t slOp4TakeCoverFromBestSound[] = {
	{
		tlOp4TakeCoverFromBestSound,
		ARRAYSIZE( tlOp4TakeCoverFromBestSound ),
		0,
		bits_SOUND_DANGER,
		"GruntTakeCoverFromBestSound"
	}
};

static Task_t tlOp4HideReload[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_RELOAD },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_RELOAD },
};

static Schedule_t slOp4HideReload[] = {
	{
		tlOp4HideReload,
		ARRAYSIZE( tlOp4HideReload ),
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntHideReload"
	}
};

static Task_t tlOp4Sweep[] = {
	{ TASK_TURN_LEFT, 179 },
	{ TASK_WAIT, 1 },
	{ TASK_TURN_LEFT, 179 },
	{ TASK_WAIT, 1 },
};

static Schedule_t slOp4Sweep[] = {
	{
		tlOp4Sweep,
		ARRAYSIZE( tlOp4Sweep ),
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

static Task_t tlOp4RangeAttack1A[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_CROUCH },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
};

static Schedule_t slOp4RangeAttack1A[] = {
	{
		tlOp4RangeAttack1A,
		ARRAYSIZE( tlOp4RangeAttack1A ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"Range Attack1A"
	}
};

static Task_t tlOp4RangeAttack1B[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_WAIT_FACE_ENEMY, 0.1f },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
};

static Schedule_t slOp4RangeAttack1B[] = {
	{
		tlOp4RangeAttack1B,
		ARRAYSIZE( tlOp4RangeAttack1B ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"Range Attack1B"
	}
};

static Task_t tlOp4RangeAttack2[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_OP4_FACE_TOSS_DIR, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_RANGE_ATTACK2 },
	{ TASK_SET_SCHEDULE, (float) SCHED_OP4_WAIT_IN_COVER },
};

static Schedule_t slOp4RangeAttack2[] = {
	{
		tlOp4RangeAttack2,
		ARRAYSIZE( tlOp4RangeAttack2 ),
		0,
		0,
		"RangeAttack2"
	}
};

static Task_t tlOp4Repel[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_GLIDE },
};

static Schedule_t slOp4Repel[] = {
	{
		tlOp4Repel,
		ARRAYSIZE( tlOp4Repel ),
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

static Task_t tlOp4RepelAttack[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_FLY },
};

static Schedule_t slOp4RepelAttack[] = {
	{
		tlOp4RepelAttack,
		ARRAYSIZE( tlOp4RepelAttack ),
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_NEW_ENEMY,
		0,
		"Repel Attack"
	}
};

static Task_t tlOp4RepelLand[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_LAND },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_CLEAR_LASTPOSITION, 0 },
};

static Schedule_t slOp4RepelLand[] = {
	{
		tlOp4RepelLand,
		ARRAYSIZE( tlOp4RepelLand ),
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

static Task_t tlOp4MedicFail[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 2 },
};

static Schedule_t slOp4MedicFail[] = {
	{
		tlOp4MedicFail,
		ARRAYSIZE( tlOp4MedicFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK2 |
		bits_COND_NEW_ENEMY,
		0,
		"Grunt Fail"
	}
};

static Task_t tlOp4MedicCombatFail[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 0.5f },
};

static Schedule_t slOp4MedicCombatFail[] = {
	{
		tlOp4MedicCombatFail,
		ARRAYSIZE( tlOp4MedicCombatFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"Grunt Combat Fail"
	}
};

static Task_t tlOp4MedicVictoryDance[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_WAIT, 1.5f },
	{ TASK_GET_PATH_TO_ENEMY_CORPSE, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
};

static Schedule_t slOp4MedicVictoryDance[] = {
	{
		tlOp4MedicVictoryDance,
		ARRAYSIZE( tlOp4MedicVictoryDance ),
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntVictoryDance"
	}
};

static Task_t tlOp4MedicEstablishLineOfFire[] = {
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_OP4_ELOF_FAIL },
	{ TASK_GET_PATH_TO_ENEMY, 0 },
	{ TASK_OP4_SPEAK_SENTENCE, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_AP, 0 },
};

static Schedule_t slOp4MedicEstablishLineOfFire[] = {
	{
		tlOp4MedicEstablishLineOfFire,
		ARRAYSIZE( tlOp4MedicEstablishLineOfFire ),
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

static Task_t tlOp4MedicFoundEnemy[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_SIGNAL1 },
};

static Schedule_t slOp4MedicFoundEnemy[] = {
	{
		tlOp4MedicFoundEnemy,
		ARRAYSIZE( tlOp4MedicFoundEnemy ),
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntFoundEnemy"
	}
};

static Task_t tlOp4MedicCombatFace[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_WAIT, 1.5f },
	{ TASK_SET_SCHEDULE, (float) SCHED_OP4_SWEEP },
};

static Schedule_t slOp4MedicCombatFace[] = {
	{
		tlOp4MedicCombatFace,
		ARRAYSIZE( tlOp4MedicCombatFace ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD,
		0,
		"Combat Face"
	}
};

static Task_t tlOp4MedicSignalSuppress[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_SIGNAL2 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
};

static Schedule_t slOp4MedicSignalSuppress[] = {
	{
		tlOp4MedicSignalSuppress,
		ARRAYSIZE( tlOp4MedicSignalSuppress ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"SignalSuppress"
	}
};

static Task_t tlOp4MedicSuppress[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
};

static Schedule_t slOp4MedicSuppress[] = {
	{
		tlOp4MedicSuppress,
		ARRAYSIZE( tlOp4MedicSuppress ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"Suppress"
	}
};

static Task_t tlOp4MedicWaitInCover[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 1 },
};

static Schedule_t slOp4MedicWaitInCover[] = {
	{
		tlOp4MedicWaitInCover,
		ARRAYSIZE( tlOp4MedicWaitInCover ),
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

static Task_t tlOp4MedicTakeCover[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_OP4_TAKECOVER_FAILED },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_OP4_SPEAK_SENTENCE, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_SET_SCHEDULE, (float) SCHED_OP4_WAIT_IN_COVER },
};

static Schedule_t slOp4MedicTakeCover[] = {
	{
		tlOp4MedicTakeCover,
		ARRAYSIZE( tlOp4MedicTakeCover ),
		0,
		0,
		"TakeCover"
	}
};

static Task_t tlOp4MedicGrenadeCover[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FIND_COVER_FROM_ENEMY, 30 },
	{ TASK_FIND_FAR_NODE_COVER_FROM_ENEMY, 384 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_SPECIAL_ATTACK1 },
	{ TASK_CLEAR_MOVE_WAIT, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_OP4_WAIT_IN_COVER },
};

static Schedule_t slOp4MedicGrenadeCover[] = {
	{
		tlOp4MedicGrenadeCover,
		ARRAYSIZE( tlOp4MedicGrenadeCover ),
		0,
		0,
		"GrenadeCover"
	}
};

static Task_t tlOp4MedicTossGrenadeCover[] = {
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RANGE_ATTACK2, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_TAKE_COVER_FROM_ENEMY },
};

static Schedule_t slOp4MedicTossGrenadeCover[] = {
	{
		tlOp4MedicTossGrenadeCover,
		ARRAYSIZE( tlOp4MedicTossGrenadeCover ),
		0,
		0,
		"TossGrenadeCover"
	}
};

static Task_t tlOp4MedicTakeCoverFromBestSound[] = {
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_COWER },
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FIND_COVER_FROM_BEST_SOUND, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_GRENADE, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_TURN_LEFT, 179 },
};

static Schedule_t slOp4MedicTakeCoverFromBestSound[] = {
	{
		tlOp4MedicTakeCoverFromBestSound,
		ARRAYSIZE( tlOp4MedicTakeCoverFromBestSound ),
		0,
		bits_SOUND_DANGER,
		"GruntTakeCoverFromBestSound"
	}
};

static Task_t tlOp4MedicHideReload[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_RELOAD },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_RELOAD },
};

static Schedule_t slOp4MedicHideReload[] = {
	{
		tlOp4MedicHideReload,
		ARRAYSIZE( tlOp4MedicHideReload ),
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntHideReload"
	}
};

static Task_t tlOp4MedicSweep[] = {
	{ TASK_TURN_LEFT, 179 },
	{ TASK_WAIT, 1 },
	{ TASK_TURN_LEFT, 179 },
	{ TASK_WAIT, 1 },
};

static Schedule_t slOp4MedicSweep[] = {
	{
		tlOp4MedicSweep,
		ARRAYSIZE( tlOp4MedicSweep ),
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

static Task_t tlOp4MedicRangeAttack1A[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_CROUCH },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
};

static Schedule_t slOp4MedicRangeAttack1A[] = {
	{
		tlOp4MedicRangeAttack1A,
		ARRAYSIZE( tlOp4MedicRangeAttack1A ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"Range Attack1A"
	}
};

static Task_t tlOp4MedicRangeAttack1B[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_WAIT_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_OP4_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
};

static Schedule_t slOp4MedicRangeAttack1B[] = {
	{
		tlOp4MedicRangeAttack1B,
		ARRAYSIZE( tlOp4MedicRangeAttack1B ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"Range Attack1B"
	}
};

static Task_t tlOp4MedicRangeAttack2[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_OP4_FACE_TOSS_DIR, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_RANGE_ATTACK2 },
	{ TASK_SET_SCHEDULE, (float) SCHED_OP4_WAIT_IN_COVER },
};

static Schedule_t slOp4MedicRangeAttack2[] = {
	{
		tlOp4MedicRangeAttack2,
		ARRAYSIZE( tlOp4MedicRangeAttack2 ),
		0,
		0,
		"RangeAttack2"
	}
};

static Task_t tlOp4MedicRepel[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_GLIDE },
};

static Schedule_t slOp4MedicRepel[] = {
	{
		tlOp4MedicRepel,
		ARRAYSIZE( tlOp4MedicRepel ),
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

static Task_t tlOp4MedicRepelAttack[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_FLY },
};

static Schedule_t slOp4MedicRepelAttack[] = {
	{
		tlOp4MedicRepelAttack,
		ARRAYSIZE( tlOp4MedicRepelAttack ),
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD,
		0,
		"Repel Attack"
	}
};

static Task_t tlOp4MedicRepelLand[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_LAND },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_CLEAR_LASTPOSITION, 0 },
};

static Schedule_t slOp4MedicRepelLand[] = {
	{
		tlOp4MedicRepelLand,
		ARRAYSIZE( tlOp4MedicRepelLand ),
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


static Task_t tlOp4MedicHeal[] = {
	{ TASK_MOVE_TO_TARGET_RANGE, 55.0f },
	{ TASK_OP4_MEDIC_SAY_HEAL, 0.0f },
	{ TASK_PLAY_SEQUENCE_FACE_TARGET, (float) ACT_ARM },
	{ TASK_OP4_MEDIC_HEAL, 0.0f },
	{ TASK_PLAY_SEQUENCE_FACE_TARGET, (float) ACT_DISARM },
	{ TASK_OP4_MEDIC_RESET_HEAL, 0.0f },
};

static Schedule_t slOp4MedicHeal[] = {
	{
		tlOp4MedicHeal,
		ARRAYSIZE( tlOp4MedicHeal ),
		bits_COND_HEAVY_DAMAGE,
		0,
		"Heal Speak/Heal"
	}
};

#define MEDIC_GIVE_UP_DELAY			10	// a new patient is dropped after this
#define MEDIC_HEAL_GIVE_UP_DELAY	5	// ...or this long after the last heal
#define MEDIC_REVIVE_MIN_RENDERAMT	50.0f
#define MEDIC_HEAL_FOLLOW_DIST		90	// the patient is followed beyond this
#define MEDIC_HEAL_RANGE			100

static Task_t tlOp4MedicContinue[] = {
	{ TASK_MOVE_TO_TARGET_RANGE, 55.0f },
	{ TASK_OP4_MEDIC_HEAL, 0.0f },
	{ TASK_PLAY_SEQUENCE_FACE_TARGET, (float) ACT_DISARM },
	{ TASK_OP4_MEDIC_RESET_HEAL, 0.0f },
};

static Schedule_t slOp4MedicContinue[] = {
	{
		tlOp4MedicContinue,
		ARRAYSIZE( tlOp4MedicContinue ),
		bits_COND_HEAVY_DAMAGE,
		0,
		"Heal Speak Heal Continue"
	}
};

static Task_t tlOp4MedicRevive[] = {
	{ TASK_MOVE_TO_TARGET_RANGE, 25.0f },
	{ TASK_OP4_MEDIC_REVIVE, 0.0f },
	{ TASK_OP4_MEDIC_RESET_HEAL, 0.0f },
};

static Schedule_t slOp4MedicRevive[] = {
	{
		tlOp4MedicRevive,
		ARRAYSIZE( tlOp4MedicRevive ),
		bits_COND_HEAVY_DAMAGE,
		0,
		"Revive Speak/Revive"
	}
};

static Task_t tlOp4MedicNeedleDisarm[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_DISARM },
	{ TASK_OP4_MEDIC_RESET_HEAL, 0 },
};

static Schedule_t slOp4MedicNeedleDisarm[] = {
	{
		tlOp4MedicNeedleDisarm,
		ARRAYSIZE( tlOp4MedicNeedleDisarm ),
		0,
		0,
		"Disarm Needle"
	}
};

static Task_t tlMassnFail[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 2 },
};

static Schedule_t slMassnFail[] = {
	{
		tlMassnFail,
		ARRAYSIZE( tlMassnFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK2 |
		bits_COND_NEW_ENEMY,
		0,
		"Grunt Fail"
	}
};

static Task_t tlMassnCombatFail[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 1 },
	{ TASK_SET_SCHEDULE, (float) SCHED_MASSN_ESTABLISH_LINE_OF_FIRE },
};

static Schedule_t slMassnCombatFail[] = {
	{
		tlMassnCombatFail,
		ARRAYSIZE( tlMassnCombatFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"Male Assassin Combat Fail"
	}
};

static Task_t tlMassnCombatFailB[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 2 },
};

static Schedule_t slMassnCombatFailB[] = {
	{
		tlMassnCombatFailB,
		ARRAYSIZE( tlMassnCombatFailB ),
		bits_COND_SEE_ENEMY |
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"Male Assassin Combat Fail (B)"
	}
};

static Task_t tlMassnVictoryDance[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_CLEAR_LASTPOSITION, 0 },
	{ TASK_STORE_LASTPOSITION, 0 },
	{ TASK_WAIT, 1.5f },
	{ TASK_GET_PATH_TO_ENEMY_CORPSE, 0 },
	{ TASK_WALK_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_VICTORY_DANCE },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
};

static Schedule_t slMassnVictoryDance[] = {
	{
		tlMassnVictoryDance,
		ARRAYSIZE( tlMassnVictoryDance ),
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntVictoryDance"
	}
};

static Task_t tlMassnEstablishLineOfFire[] = {
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_MASSN_ELOF_FAIL },
	{ TASK_GET_PATH_TO_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_AP, 0 },
};

static Schedule_t slMassnEstablishLineOfFire[] = {
	{
		tlMassnEstablishLineOfFire,
		ARRAYSIZE( tlMassnEstablishLineOfFire ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK2 |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD,
		bits_SOUND_DANGER,
		"Male Assassin EstablishLineOfFire"
	}
};

static Task_t tlMassnFoundEnemy[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_SIGNAL1 },
};

static Schedule_t slMassnFoundEnemy[] = {
	{
		tlMassnFoundEnemy,
		ARRAYSIZE( tlMassnFoundEnemy ),
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"Male Assassin FoundEnemy"
	}
};

static Task_t tlMassnCombatFace[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_WAIT_FACE_ENEMY, 1 },
	{ TASK_SET_SCHEDULE, (float) SCHED_MASSN_SWEEP },
};

static Schedule_t slMassnCombatFace[] = {
	{
		tlMassnCombatFace,
		ARRAYSIZE( tlMassnCombatFace ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD,
		0,
		"Combat Face"
	}
};

static Task_t tlMassnSignalSuppress[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_SIGNAL2 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_MASSN_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_MASSN_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_MASSN_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_MASSN_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_MASSN_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
};

static Schedule_t slMassnSignalSuppress[] = {
	{
		tlMassnSignalSuppress,
		ARRAYSIZE( tlMassnSignalSuppress ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"SignalSuppress"
	}
};

static Task_t tlMassnSuppress[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_MASSN_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_MASSN_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_MASSN_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_MASSN_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_MASSN_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
};

static Schedule_t slMassnSuppress[] = {
	{
		tlMassnSuppress,
		ARRAYSIZE( tlMassnSuppress ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"Suppress"
	}
};

static Task_t tlMassnWaitInCover[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_ACTIVITY, (float) ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY, 1 },
	{ TASK_SET_SCHEDULE, (float) SCHED_MASSN_ESTABLISH_LINE_OF_FIRE },
};

static Schedule_t slMassnWaitInCover[] = {
	{
		tlMassnWaitInCover,
		ARRAYSIZE( tlMassnWaitInCover ),
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

static Task_t tlMassnTakeCover[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_MASSN_TAKECOVER_FAILED },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_SET_SCHEDULE, (float) SCHED_MASSN_WAIT_FACE_ENEMY },
};

static Schedule_t slMassnTakeCover[] = {
	{
		tlMassnTakeCover,
		ARRAYSIZE( tlMassnTakeCover ),
		0,
		0,
		"TakeCover"
	}
};

static Task_t tlMassnGrenadeCover[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FIND_COVER_FROM_ENEMY, 30 },
	{ TASK_FIND_FAR_NODE_COVER_FROM_ENEMY, 384 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_SPECIAL_ATTACK1 },
	{ TASK_CLEAR_MOVE_WAIT, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_MASSN_WAIT_FACE_ENEMY },
};

static Schedule_t slMassnGrenadeCover[] = {
	{
		tlMassnGrenadeCover,
		ARRAYSIZE( tlMassnGrenadeCover ),
		0,
		0,
		"GrenadeCover"
	}
};

static Task_t tlMassnTossGrenadeCover[] = {
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_RANGE_ATTACK2, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_TAKE_COVER_FROM_ENEMY },
};

static Schedule_t slMassnTossGrenadeCover[] = {
	{
		tlMassnTossGrenadeCover,
		ARRAYSIZE( tlMassnTossGrenadeCover ),
		0,
		0,
		"TossGrenadeCover"
	}
};

static Task_t tlMassnTakeCoverFromBestSound[] = {
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_COWER },
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FIND_COVER_FROM_BEST_SOUND, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_GRENADE, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_TURN_LEFT, 179 },
};

static Schedule_t slMassnTakeCoverFromBestSound[] = {
	{
		tlMassnTakeCoverFromBestSound,
		ARRAYSIZE( tlMassnTakeCoverFromBestSound ),
		0,
		bits_SOUND_DANGER,
		"GruntTakeCoverFromBestSound"
	}
};

static Task_t tlMassnHideReload[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_RELOAD },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_RELOAD },
	{ TASK_SET_SCHEDULE, (float) SCHED_MASSN_ESTABLISH_LINE_OF_FIRE },
};

static Schedule_t slMassnHideReload[] = {
	{
		tlMassnHideReload,
		ARRAYSIZE( tlMassnHideReload ),
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntHideReload"
	}
};

static Task_t tlMassnHideReloadB[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_RELOAD },
	{ TASK_CLEAR_LASTPOSITION, 0 },
	{ TASK_STORE_LASTPOSITION, 0 },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_REMEMBER, (float) bits_MEMORY_INCOVER },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_RELOAD },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_CLEAR_LASTPOSITION, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_AP, 0 },
};

static Schedule_t slMassnHideReloadB[] = {
	{
		tlMassnHideReloadB,
		ARRAYSIZE( tlMassnHideReloadB ),
		bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"GruntHideReloadB"
	}
};

static Task_t tlMassnSweep[] = {
	{ TASK_TURN_LEFT, 179 },
	{ TASK_WAIT, 1 },
	{ TASK_TURN_LEFT, 179 },
	{ TASK_WAIT, 1 },
};

static Schedule_t slMassnSweep[] = {
	{
		tlMassnSweep,
		ARRAYSIZE( tlMassnSweep ),
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

static Task_t tlMassnRangeAttack1A[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_WAIT_FACE_ENEMY, 0 },
	{ TASK_MASSN_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_IDLE_ANGRY },
	{ TASK_WAIT_FACE_ENEMY, 1 },
};

static Schedule_t slMassnRangeAttack1A[] = {
	{
		tlMassnRangeAttack1A,
		ARRAYSIZE( tlMassnRangeAttack1A ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"Range Attack1A"
	}
};

static Task_t tlMassnRangeAttack1B[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_CROUCH },
	{ TASK_MASSN_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_FIND_COVER_FROM_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT_ENEMY_OCCLUDED, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY, (float) ACT_IDLE_ANGRY },
	{ TASK_WAIT_FACE_ENEMY, 1 },
};

static Schedule_t slMassnRangeAttack1B[] = {
	{
		tlMassnRangeAttack1B,
		ARRAYSIZE( tlMassnRangeAttack1B ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"Range Attack1B"
	}
};

static Task_t tlMassnRangeAttack1C[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_MASSN_WAIT_SNIPER, 0.5f },
	{ TASK_MASSN_CHECK_FIRE, 0 },
	{ TASK_RANGE_ATTACK1, 0 },
	{ TASK_SET_SCHEDULE, (float) SCHED_MASSN_WAIT_FACE_ENEMY },
};

static Schedule_t slMassnRangeAttack1C[] = {
	{
		tlMassnRangeAttack1C,
		ARRAYSIZE( tlMassnRangeAttack1C ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"Range Attack1C"
	}
};

static Task_t tlMassnRangeAttack1D[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_MASSN_WAIT_SNIPER, 0 },
	{ TASK_MASSN_CHECK_FIRE, 0 },
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
	{ TASK_SET_SCHEDULE, (float) SCHED_MASSN_WAIT_FACE_ENEMY },
};

static Schedule_t slMassnRangeAttack1D[] = {
	{
		tlMassnRangeAttack1D,
		ARRAYSIZE( tlMassnRangeAttack1D ),
		bits_COND_NO_AMMO_LOADED |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_GRUNT_NOFIRE,
		bits_SOUND_DANGER,
		"Range Attack1D"
	}
};

static Task_t tlMassnRangeAttack2[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_MASSN_FACE_TOSS_DIR, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_RANGE_ATTACK2 },
	{ TASK_SET_SCHEDULE, (float) SCHED_MASSN_WAIT_FACE_ENEMY },
};

static Schedule_t slMassnRangeAttack2[] = {
	{
		tlMassnRangeAttack2,
		ARRAYSIZE( tlMassnRangeAttack2 ),
		0,
		0,
		"RangeAttack2"
	}
};

static Task_t tlMassnRepel[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_GLIDE },
};

static Schedule_t slMassnRepel[] = {
	{
		tlMassnRepel,
		ARRAYSIZE( tlMassnRepel ),
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

static Task_t tlMassnRepelAttack[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_FLY },
};

static Schedule_t slMassnRepelAttack[] = {
	{
		tlMassnRepelAttack,
		ARRAYSIZE( tlMassnRepelAttack ),
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD,
		0,
		"Repel Attack"
	}
};

static Task_t tlMassnRepelLand[] = {
	{ TASK_STOP_MOVING, 0 },
	{ TASK_PLAY_SEQUENCE, (float) ACT_LAND },
	{ TASK_GET_PATH_TO_LASTPOSITION, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
	{ TASK_CLEAR_LASTPOSITION, 0 },
};

static Schedule_t slMassnRepelLand[] = {
	{
		tlMassnRepelLand,
		ARRAYSIZE( tlMassnRepelLand ),
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

static Task_t tlMassnChaseEnemy[] = {
	{ TASK_SET_FAIL_SCHEDULE, (float) SCHED_FAIL },
	{ TASK_GET_PATH_TO_ENEMY, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 },
};

static Schedule_t slMassnChaseEnemy[] = {
	{
		tlMassnChaseEnemy,
		ARRAYSIZE( tlMassnChaseEnemy ),
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND |
		bits_COND_ENEMY_DEAD |
		bits_COND_TASK_FAILED,
		0,
		"Massn Chase Enemy"
	}
};

extern cvar_t sk_hgrunt_health;
extern cvar_t sk_hgrunt_kick;
extern cvar_t sk_hgrunt_pellets;

class CHGruntOp4 : public CSquadTalkMonster
{
public:
	int ObjectCaps( void ) { return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION; }
	int ISoundMask( void ) { return bits_SOUND_WORLD | bits_SOUND_COMBAT | bits_SOUND_PLAYER | bits_SOUND_DANGER | bits_SOUND_BULLETHIT; }
	int IRelationship( CBaseEntity *target, bool ignoreNoTarget = false );
	float GetPointsForDamage( float damage )
	{
		//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
		return pev->max_health / sk_hgrunt_health.value * 4 * ( damage / pev->max_health );
	}

	void PrescheduleThink( void );
	void IdleSound( void );
	void AlertSound( void );
	void DeclineFollowing( void ) { PlaySentence( "FG_POK", 2, 1, ATTN_NORM ); }
	void GibMonster( void );
	CBaseEntity *DropWeapon( void );
	int TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType );
	void TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType );
	BOOL FOkToSpeak( void );
	void JustSpoke( void );
	void SpeakSentence( void );
	void Spawn( void );
	void Precache( void );
	void SetupFriendly( void ) { HumanSetupFriendly( this, HUMAN_OP4 ); }
	int Classify( void ) { return GetClassification( CLASS_HUMAN_MILITARY ); }
	void SetPlayerAlly( bool state ) { SetPlayerAllyDirect( !state ); }
	void KeyValue( KeyValueData *pkvd );
	BOOL FCanCheckAttacks( void ) { return !HasConditions( bits_COND_ENEMY_TOOFAR ); }
	BOOL CheckRangeAttack1( float flDot, float flDist );
	BOOL CheckRangeAttack2( float flDot, float flDist );
	BOOL CheckMeleeAttack1( float flDot, float flDist ) { return HumanCheckMeleeAttack1( this, flDot, flDist ); }
	void CheckAmmo( void ) { HumanCheckAmmo( this, m_Combat, HUMAN_OP4 ); }
	Vector GetGunPosition( void ) { return HumanGunPosition( this, m_Combat, HUMAN_OP4 ); }
	void HandleAnimEvent( MonsterEvent_t *event );
	CBaseEntity *Kick( void );
	void Shoot( void );
	void Shotgun( void );
	void Saw( void );
	void SetActivity( Activity activity );
	void SetGaitActivity( Activity activity );
	void SetYawSpeed( void ) { pev->yaw_speed = m_Activity == ACT_GLIDE || m_Activity == ACT_FLY ? 200 : 360; }
	void StartTask( Task_t *task );
	void RunTask( Task_t *task );
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int type );
	void PainSound( void );
	void DeathSound( void );
	void Killed( entvars_t *attacker, int gib );
	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	CUSTOM_SCHEDULES;
	void TalkInit( void );

	HumanCombatData m_Combat;
	bool            m_bHeadSet;
	bool            m_bSawRestarts;	// the model fires each SAW shot from frame 0 of its sequence
};

LINK_ENTITY_TO_CLASS( monster_human_grunt_ally, CHGruntOp4 );

void SquadMakerPrepareHumanGrunt( CBaseEntity *entity )
{
	static_cast<CHGruntOp4 *>( entity )->m_bHeadSet = true;
}

TYPEDESCRIPTION CHGruntOp4::m_SaveData[] =
{
	{ FIELD_TIME, "m_flNextGrenadeCheck", offsetof( CHGruntOp4, m_Combat.m_flNextGrenadeCheck ), 1, 0 },
	{ FIELD_TIME, "m_flNextPainTime", offsetof( CHGruntOp4, m_Combat.m_flNextPainTime ), 1, 0 },
	{ FIELD_VECTOR, "m_vecTossVelocity", offsetof( CHGruntOp4, m_Combat.m_vecTossVelocity ), 1, 0 },
	{ FIELD_CHARACTER, "m_fThrowGrenade", offsetof( CHGruntOp4, m_Combat.m_fThrowGrenade ), 1, 0 },
	{ FIELD_CHARACTER, "m_fStanding", offsetof( CHGruntOp4, m_Combat.m_fStanding ), 1, 0 },
	{ FIELD_CHARACTER, "m_fFirstEncounter", offsetof( CHGruntOp4, m_Combat.m_fFirstEncounter ), 1, 0 },
	{ FIELD_INTEGER, "m_cClipSize", offsetof( CHGruntOp4, m_Combat.m_cClipSize ), 1, 0 },
	DEFINE_FIELD( CHGruntOp4, m_voicePitch, FIELD_INTEGER ),
	{ FIELD_INTEGER, "m_iSentence", offsetof( CHGruntOp4, m_Combat.m_iSentence ), 1, 0 },
};

IMPLEMENT_SAVERESTORE( CHGruntOp4, CSquadTalkMonster );

DEFINE_CUSTOM_SCHEDULES( CHGruntOp4 ){
	slOp4Fail,
	slOp4CombatFail,
	slOp4VictoryDance,
	slOp4EstablishLineOfFire,
	slOp4FoundEnemy,
	slOp4CombatFace,
	slOp4SignalSuppress,
	slOp4Suppress,
	slOp4WaitInCover,
	slOp4TakeCover,
	slOp4GrenadeCover,
	slOp4TossGrenadeCover,
	slOp4TakeCoverFromBestSound,
	slOp4HideReload,
	slOp4Sweep,
	slOp4RangeAttack1A,
	slOp4RangeAttack1B,
	slOp4RangeAttack2,
	slOp4Repel,
	slOp4RepelAttack,
	slOp4RepelLand
};

IMPLEMENT_CUSTOM_SCHEDULES( CHGruntOp4, CSquadTalkMonster );

void CHGruntOp4::Spawn( void )
{
	Precache();
	HumanSpawn( this, m_Combat, HUMAN_OP4, m_bHeadSet );
	m_Combat.m_iSentence = -1;
	m_Combat.m_fFirstEncounter = true;
	m_voicePitch = m_Combat.m_voicePitch;
	m_bSawRestarts = false;

	int saw = LookupSequence( "standing_saw" );

	if ( saw != -1 )
	{
		void *model = GET_MODEL_PTR( edict() );

		if ( model && GetSequenceEventFrame( model, saw, HGRUNT_AE_BURST1 ) == 0 )
			m_bSawRestarts = true;
	}
}

void CHGruntOp4::Precache( void )
{
	TalkInit();
	CSquadTalkMonster::Precache();
	HumanPrecache( this, m_Combat, HUMAN_OP4 );
}

void CHGruntOp4::SetGaitActivity( Activity activity )
{
	if ( activity == ACT_WALK || activity == ACT_RUN )
	{
		m_Combat.m_fStanding = TRUE;

		if ( pev->health <= 20 )
			activity = activity == ACT_WALK ? ACT_WALK_HURT : ACT_RUN_HURT;
	}

	CBaseMonster::SetGaitActivity( activity );
}

void CHGruntOp4::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "head" ) )
	{
		m_bHeadSet = true;
		m_Combat.m_iHead = atoi( pkvd->szValue ) + HUMAN_HEAD_KEYVALUE_BIAS;
		pkvd->fHandled = TRUE;
	}
	else
		CSquadTalkMonster::KeyValue( pkvd );
}

BOOL CHGruntOp4::CheckRangeAttack1( float flDot, float flDist )
{
	if ( flDist > 2048 || flDot < 0.5f || !NoFriendlyFire() )
		return FALSE;

	CBaseEntity *enemy = m_hEnemy;

	if ( !enemy->IsPlayer() && flDist <= 64 )
		return FALSE;

	Vector standing = pev->origin + Vector( 0, 0, 60 );
	Vector crouching = pev->origin + Vector( 0, 0, 48 );
	TraceResult tr;

	if ( RANDOM_LONG( 0, 9 ) == 0 && m_Activity != ACT_RANGE_ATTACK1 && IsMoving() )
		m_Combat.m_fStanding = RANDOM_LONG( 0, 1 ) != 0;

	pev->view_ofs = Vector( 0, 0, m_Combat.m_fStanding ? 60 : 48 );

	Vector first = m_Combat.m_fStanding ? standing : crouching;
	Vector second = m_Combat.m_fStanding ? crouching : standing;

	UTIL_TraceLine( first, enemy->BodyTarget( first ), ignore_monsters, ignore_glass, edict(), &tr );

	if ( tr.flFraction == 1.0f || tr.pHit == enemy->edict() )
		return TRUE;

	UTIL_TraceLine( second, enemy->BodyTarget( second ), ignore_monsters, ignore_glass, edict(), &tr );

	if ( tr.flFraction == 1.0f || tr.pHit == enemy->edict() )
	{
		m_Combat.m_fStanding = !m_Combat.m_fStanding;
		pev->view_ofs = Vector( 0, 0, m_Combat.m_fStanding ? 60 : 48 );
		return TRUE;
	}

	return FALSE;
}

BOOL CHGruntOp4::CheckRangeAttack2( float flDot, float flDist )
{
	return HumanCheckRangeAttack2( this, m_Combat, flDot, flDist );
}

void CHGruntOp4::SetActivity( Activity activity )
{
	int sequence;

	switch ( activity )
	{
		case ACT_IDLE:
			if ( m_MonsterState == MONSTERSTATE_COMBAT )
			{
				activity = m_Combat.m_fStanding ? ACT_IDLE_ANGRY : ACT_CROUCHIDLE;
				sequence = LookupActivity( activity );

				if ( sequence == ACTIVITY_NOT_AVAILABLE )
					sequence = LookupSequence( "crouching_idle" );
			}
			else
			{
				sequence = LookupActivity( activity );
			}
			break;
		case ACT_WALK:
			sequence = LookupActivity( pev->health <= 20 ? ACT_WALK_HURT : activity );
			break;
		case ACT_RUN:
			sequence = LookupActivity( pev->health <= 20 ? ACT_RUN_HURT : activity );
			break;
		case ACT_RANGE_ATTACK1:
			if ( pev->weapons & HGRUNT_9MMAR )
			{
				sequence = ACTIVITY_NOT_AVAILABLE;

				if ( pev->weapons & HGRUNT_GRENADELAUNCHER )
					sequence = LookupSequence( m_Combat.m_fStanding ? "standing_m16a2" : "crouching_m16a2" );

				if ( sequence == ACTIVITY_NOT_AVAILABLE )
					sequence = LookupSequence( m_Combat.m_fStanding ? "standing_mp5" : "crouching_mp5" );
			}
			else if ( pev->weapons & HGRUNT_SAW )
			{
				sequence = LookupSequence( m_Combat.m_fStanding ? "standing_saw" : "crouching_saw" );
			}
			else
			{
				sequence = LookupSequence( m_Combat.m_fStanding ? "standing_shotgun" : "crouching_shotgun" );
			}
			break;
		case ACT_RANGE_ATTACK2:
			sequence = LookupSequence( ( pev->weapons & HGRUNT_HANDGRENADE ) ? "throwgrenade" : "launchgrenade" );
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

void CHGruntOp4::StartTask( Task_t *task )
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
		case TASK_OP4_FACE_TOSS_DIR:
			break;
		case TASK_OP4_SPEAK_SENTENCE:
			SpeakSentence();
			TaskComplete();
			break;
		case TASK_OP4_CHECK_FIRE:
			TaskComplete();

			if ( !NoFriendlyFire() )
			{
				ChangeSchedule( GetScheduleOfType( SCHED_FIND_ATTACK_POINT ) );
				SetConditions( bits_COND_GRUNT_NOFIRE );
			}
			break;
		default:
			CSquadTalkMonster::StartTask( task );
			break;
	}
}

void CHGruntOp4::RunTask( Task_t *task )
{
	if ( task->iTask != TASK_OP4_FACE_TOSS_DIR )
	{
		CSquadTalkMonster::RunTask( task );
		return;
	}

	MakeIdealYaw( pev->origin + m_Combat.m_vecTossVelocity * 64 );
	ChangeYaw( pev->yaw_speed );

	if ( FacingIdeal() )
		TaskComplete();
}

static const char *g_HumanSentences[] = {
	"FG_GREN", "FG_ALERT", "FG_MONSTER", "FG_COVER",
	"FG_THROW", "FG_CHARGE", "FG_TAUNT"
};

enum
{
	HUMAN_SENTENCE_COVER = 3,
	HUMAN_SENTENCE_CHARGE = 5,
	HUMAN_SENTENCE_TAUNT = 6
};

static int g_fOp4GruntQuestion;
static int g_fOp4MedicQuestion;

CBaseEntity *HumanKickTrace( CBaseMonster *monster )
{
	UTIL_MakeVectors( monster->pev->angles );

	Vector source( monster->pev->origin.x, monster->pev->origin.y, monster->pev->origin.z + monster->pev->size.z * 0.5f );
	TraceResult trace;

	UTIL_TraceHull( source, source + gpGlobals->v_forward * 70, dont_ignore_monsters, head_hull, monster->edict(), &trace );

	if ( !trace.pHit || trace.pHit->free )
		return NULL;

	return (CBaseEntity *) GET_PRIVATE( trace.pHit );
}

// The big creatures don't move for a kick
void HumanKickHit( CBaseMonster *monster, CBaseEntity *target, float punch )
{
	int flags = target->pev->flags;

	if ( ( ( flags & FL_CLIENT ) || ( ( flags & FL_MONSTER ) && target->pev->movetype != MOVETYPE_FLY ) )
		 && !FClassnameIs( target->pev, "monster_alien_voltigore" )
		 && !FClassnameIs( target->pev, "monster_gargantua" )
		 && !FClassnameIs( target->pev, "monster_bigmomma" ) )
	{
		UTIL_MakeVectors( monster->pev->angles );

		target->pev->punchangle.x = punch;
		target->pev->velocity = target->pev->velocity + gpGlobals->v_forward * 300 + gpGlobals->v_up * 50;
	}

	target->TakeDamage( monster->pev, monster->pev, sk_hgrunt_kick.value, DMG_CLUB );
}

void HumanEjectShell( CBaseMonster *monster, const Vector &source, const Vector &direction, int shell, int bounce = TE_BOUNCE_SHELL )
{
	Vector velocity = gpGlobals->v_right * RANDOM_FLOAT( 40, 90 );

	velocity = velocity + gpGlobals->v_up * RANDOM_FLOAT( 75, 200 );
	velocity = velocity + gpGlobals->v_forward * RANDOM_FLOAT( -40, 40 );
	EjectBrass( source - direction * -24, velocity, monster->pev->angles.y, shell, bounce );
}

// Shot by a player ally: complain, and turn on a repeat offender
template <class T>
void HumanTalkPlayerDamage( T *monster, HumanCombatData &data, CBaseEntity *entity )
{
	if ( monster->pev->deadflag != DEAD_NO )
		return;

	if ( ( monster->HasMemory( bits_MEMORY_SUSPICIOUS ) || entity->IsFacing( monster->pev, 0.96f ) )
		 && data.m_iPlayerDamage >= RANDOM_LONG( 4, 7 ) )
	{
		monster->PlaySentence( "FG_MAD", 4, 1, ATTN_NORM );
		monster->Remember( bits_MEMORY_PROVOKED );
		monster->StopPlayerFollowing( true, false );
	}
	else
	{
		monster->PlaySentence( "FG_SHOT", 4, 1, ATTN_NORM );
		monster->Remember( bits_MEMORY_SUSPICIOUS );
		data.m_iPlayerDamage++;
	}
}

BOOL CHGruntOp4::FOkToSpeak( void )
{
	if ( !IsAlive() )
		return FALSE;

	float wait = IsPlayerAlly() ? CSquadTalkMonster::g_talkWaitTime : CSquadTalkMonster::g_talkWaitTimeEvil;

	if ( wait > gpGlobals->time )
		return FALSE;

	return !( pev->spawnflags & SF_MONSTER_GAG ) || m_MonsterState == MONSTERSTATE_COMBAT;
}

void CHGruntOp4::JustSpoke( void )
{
	if ( IsPlayerAlly() )
		CSquadTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT( 2.5f, 3 );
	else
		CSquadTalkMonster::g_talkWaitTimeEvil = gpGlobals->time + RANDOM_FLOAT( 2.5f, 3 );

	m_Combat.m_iSentence = -1;
}

void CHGruntOp4::SpeakSentence( void )
{
	if ( m_Combat.m_iSentence == -1 || !FOkToSpeak() )
		return;

	SENTENCEG_PlayRndSz( edict(), g_HumanSentences[m_Combat.m_iSentence], 1, ATTN_NORM, 0, m_voicePitch );
	JustSpoke();
}

void CHGruntOp4::IdleSound( void )
{
	if ( !FOkToSpeak() )
		return;

	if ( !g_fOp4GruntQuestion )
	{
		if ( !RANDOM_LONG( 0, 1 ) )
			return;

		switch ( RANDOM_LONG( 0, 2 ) )
		{
			case 0:
				SENTENCEG_PlayRndSz( edict(), "FG_CHECK", 1, ATTN_NORM, 0, m_voicePitch );
				g_fOp4GruntQuestion = 1;
				break;
			case 1:
				SENTENCEG_PlayRndSz( edict(), "FG_QUEST", 1, ATTN_NORM, 0, m_voicePitch );
				g_fOp4GruntQuestion = 2;
				break;
			case 2:
				SENTENCEG_PlayRndSz( edict(), "FG_IDLE", 1, ATTN_NORM, 0, m_voicePitch );
				break;
		}
	}
	else
	{
		if ( g_fOp4GruntQuestion == 1 )
			SENTENCEG_PlayRndSz( edict(), "FG_CLEAR", 1, ATTN_NORM, 0, m_voicePitch );
		else if ( g_fOp4GruntQuestion == 2 )
			SENTENCEG_PlayRndSz( edict(), "FG_ANSWER", 1, ATTN_NORM, 0, m_voicePitch );

		g_fOp4GruntQuestion = 0;
	}

	JustSpoke();
}

void CHGruntOp4::PrescheduleThink( void )
{
	if ( !InSquad() || !m_hEnemy )
		return;

	if ( HasConditions( bits_COND_SEE_ENEMY ) )
		MySquadLeader()->m_flLastEnemySightTime = gpGlobals->time;
	else if ( gpGlobals->time - MySquadLeader()->m_flLastEnemySightTime > 5 )
		MySquadLeader()->m_fEnemyEluded = TRUE;
}

void CHGruntOp4::AlertSound( void )
{
	if ( m_hEnemy && m_hEnemy->IsAlive() )
	{
		m_hTalkTarget = m_hEnemy;
		PlaySentence( "FG_ATTACK", RANDOM_FLOAT( 2.8f, 3.2f ), 1, ATTN_NORM );
	}
}

int CHGruntOp4::IRelationship( CBaseEntity *target, bool ignoreNoTarget )
{
	if ( !target )
		return R_NO;

	if ( ( target->pev->flags & FL_MONSTER ) && target->m_fOverrideClass )
		return CBaseEntity::IRelationship( target, ignoreNoTarget );

	if ( IsPlayerAlly() )
		return CSquadTalkMonster::IRelationship( target, ignoreNoTarget );

	if ( FClassnameIs( target->pev, "monster_bigmomma" ) )
	{
		CBaseMonster *monster = target->MyMonsterPointer();

		if ( monster->m_hEnemy != NULL && monster->m_hEnemy == this )
			return R_NM;
	}

	if ( FClassnameIs( target->pev, "monster_alien_grunt" ) || FClassnameIs( target->pev, "monster_gargantua" ) )
		return R_NM;

	return CSquadTalkMonster::IRelationship( target, ignoreNoTarget );
}

CBaseEntity *CHGruntOp4::DropWeapon( void )
{
	switch ( pev->weapons & ~HGRUNT_HANDGRENADE )
	{
		case HGRUNT_9MMAR:
			return CBaseMonster::DropWeapon( "weapon_9mmAR", false );
		case HGRUNT_GRENADELAUNCHER:
		{
			CBaseEntity *weapon = CBaseMonster::DropWeapon( "weapon_m16", false );

			CBaseMonster::DropWeapon( "ammo_ARgrenades", false );
			return weapon;
		}
		case HGRUNT_SHOTGUN:
			return CBaseMonster::DropWeapon( "weapon_shotgun", false );
		case HGRUNT_SAW:
			return CBaseMonster::DropWeapon( "weapon_saw", false );
	}

	return NULL;
}

CBaseEntity *CHGruntOp4::Kick( void )
{
	return HumanKickTrace( this );
}

void CHGruntOp4::Shoot( void )
{
	if ( !m_hEnemy )
		return;

	Vector source = GetGunPosition();
	Vector direction = ShootAtEnemy( source );

	UTIL_MakeVectors( pev->angles );
	HumanEjectShell( this, source, direction, m_Combat.m_iBrassShell );

	pev->effects |= EF_MUZZLEFLASH;
	m_cAmmoLoaded--;
	SetBlending( 0, UTIL_VecToAngles( direction ).x );

	if ( pev->weapons & HGRUNT_GRENADELAUNCHER )
	{
		FireBullets( 1, source, direction, VECTOR_CONE_8DEGREES, 2048, BULLET_MONSTER_SAW, 4, 0, NULL, FBDM_DRAW );
		return;
	}

	FireBullets( 1, source, direction, VECTOR_CONE_10DEGREES, 2048, BULLET_MONSTER_MP5, 4, 0, NULL, FBDM_DRAW );

	if ( pev->movetype != MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
	{
		m_fSequenceFinished = TRUE;
		m_Activity = ACT_RESET;
	}
}

void CHGruntOp4::Shotgun( void )
{
	if ( !m_hEnemy )
		return;

	Vector source = GetGunPosition();
	Vector direction = ShootAtEnemy( source );

	UTIL_MakeVectors( pev->angles );
	HumanEjectShell( this, source, direction, m_Combat.m_iShotgunShell, TE_BOUNCE_SHOTSHELL );

	pev->effects |= EF_MUZZLEFLASH;
	m_cAmmoLoaded--;
	SetBlending( 0, UTIL_VecToAngles( direction ).x );

	CBaseEntity *enemy = m_hEnemy;
	int shots = (int) sk_hgrunt_pellets.value;

	if ( !enemy || ( enemy->Center() - pev->origin ).Length2D() >= 768 || m_cAmmoLoaded <= 0 )
	{
		FireBullets( shots, source, direction, VECTOR_CONE_15DEGREES, 2048, BULLET_MONSTER_BUCKSHOT, 0, 0, NULL, FBDM_DRAW );
		return;
	}

	FireBullets( shots, source, direction, VECTOR_CONE_20DEGREES, 2048, BULLET_MONSTER_BUCKSHOT, 0, 0, NULL, FBDM_DRAW );

	if ( pev->movetype != MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
		m_flNextAttack = gpGlobals->time + 0.25f;
}

void CHGruntOp4::Saw( void )
{
	if ( !m_hEnemy )
		return;

	Vector source = GetGunPosition();
	Vector direction = ShootAtEnemy( source );

	UTIL_MakeVectors( pev->angles );
	HumanEjectShell( this, source, direction, m_Combat.m_iSawShell );
	FireBullets( 1, source, direction, VECTOR_CONE_10DEGREES, 2048, BULLET_MONSTER_SAW, 4, 0, NULL, FBDM_DRAW );

	pev->effects |= EF_MUZZLEFLASH;
	m_cAmmoLoaded--;
	SetBlending( 0, UTIL_VecToAngles( direction ).x );

	if ( m_bSawRestarts && pev->movetype != MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
	{
		m_Activity = ACT_RESET;
		pev->frame = 0;
	}
}

void CHGruntOp4::HandleAnimEvent( MonsterEvent_t *event )
{
	switch ( event->event )
	{
		case HGRUNT_AE_RELOAD:
			EMIT_SOUND( edict(), CHAN_WEAPON, ( pev->weapons & HGRUNT_SAW ) ? "weapons/saw_reload.wav" : "hgrunt/gr_reload1.wav", 1, ATTN_NORM );
			m_cAmmoLoaded = m_Combat.m_cClipSize;
			ClearConditions( bits_COND_NO_AMMO_LOADED );
			break;
		case HGRUNT_AE_KICK:
		{
			CBaseEntity *target = Kick();

			if ( target )
				HumanKickHit( this, target, 45 );

			break;
		}
		case HGRUNT_AE_BURST1:
			if ( !NoFriendlyFire() && m_hEnemy && m_hEnemy->IsAlive() )
				break;

			if ( pev->weapons & HGRUNT_9MMAR )
			{
				static const char *sounds[] = { "weapons/hks1.wav", "weapons/hks2.wav", "weapons/hks3.wav" };

				Shoot();
				EMIT_SOUND( edict(), CHAN_AUTO, ( pev->weapons & HGRUNT_GRENADELAUNCHER ) ? "weapons/m16_3round.wav" : sounds[RANDOM_LONG( 0, 2 )], 1, 0.8f );
			}
			else if ( pev->weapons & HGRUNT_SAW )
			{
				Saw();
				EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/saw_fire1.wav", 1, 0.8f, 0, 94 );
			}
			else
			{
				Shotgun();
				EMIT_SOUND( edict(), CHAN_AUTO, "weapons/sbarrel1.wav", 1, 0.8f );
			}

			if ( FOkToSpeak() && m_hEnemy && m_hEnemy->IsAlive() )
			{
				m_hTalkTarget = m_hEnemy;
				PlaySentence( "FG_ATTACK", RANDOM_FLOAT( 2.8f, 3.2f ), 1, ATTN_NORM );
			}

			CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 0.3f );
			break;
		case HGRUNT_AE_BURST2:
		case HGRUNT_AE_BURST3:
			if ( !NoFriendlyFire() && m_hEnemy && m_hEnemy->IsAlive() )
				break;

			if ( pev->weapons & HGRUNT_SAW )
			{
				if ( !m_bSawRestarts )
				{
					Saw();
					EMIT_SOUND( edict(), CHAN_WEAPON, "weapons/saw_fire1.wav", 1, 0.6f );
				}
			}
			else if ( pev->weapons & HGRUNT_GRENADELAUNCHER )
				Shoot();

			break;
		case HGRUNT_AE_GREN_TOSS:
			UTIL_MakeVectors( pev->angles );
			CGrenade::ShootTimed( pev, GetGunPosition(), m_Combat.m_vecTossVelocity, 3.5f );
			m_Combat.m_fThrowGrenade = FALSE;
			m_Combat.m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 5, 10 );
			break;
		case HGRUNT_AE_GREN_LAUNCH:
			EMIT_SOUND( edict(), CHAN_WEAPON, "weapons/glauncher.wav", 0.8f, 0.8f );
			CGrenade::ShootContact( pev, GetGunPosition(), m_Combat.m_vecTossVelocity );
			m_Combat.m_fThrowGrenade = FALSE;
			m_Combat.m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 4, 8 );
			break;
		case HGRUNT_AE_GREN_DROP:
			UTIL_MakeVectors( pev->angles );
			CGrenade::ShootTimed( pev, pev->origin + gpGlobals->v_forward * 17 - gpGlobals->v_right * 27 + gpGlobals->v_up * 6, g_vecZero, 3 );
			break;
		case HGRUNT_AE_CAUGHT_ENEMY:
			if ( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz( edict(), "FG_ALERT", 1, ATTN_NORM, 0, m_voicePitch );
				JustSpoke();
			}

			CSquadTalkMonster::HandleAnimEvent( event );
			break;
		case HGRUNT_AE_DROP_GUN:
			DropWeapon();
			break;
		default:
			CSquadTalkMonster::HandleAnimEvent( event );
			break;
	}
}

void CHGruntOp4::GibMonster( void )
{
	if ( GetBodygroup( OP4_GUN_GROUP ) != OP4_GUN_NONE )
		DropWeapon();

	CBaseMonster::GibMonster();
}

void CHGruntOp4::TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType )
{
	if ( trace->iHitgroup == HITGROUP_GENERIC )
	{
		if ( ( GetBodygroup( HEAD_GROUP ) == OP4_HEAD_GASMASK || GetBodygroup( HEAD_GROUP ) == OP4_HEAD_MP ) && ( damageType & ( DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_CLUB | DMG_SNIPER ) ) )
		{
			damage -= 20;

			if ( damage <= 0 )
			{
				UTIL_Ricochet( trace->vecEndPos, 1 );
				damage = 0.01f;
			}
		}
		else
		{
			trace->iHitgroup = HITGROUP_HEAD;
		}
	}
	else if ( ( trace->iHitgroup == HITGROUP_CHEST || trace->iHitgroup == HITGROUP_STOMACH )
			  && ( damageType & ( DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_SNIPER ) ) )
	{
		if ( damage >= 2 )
			damage -= 2;

		damage *= 0.5;
	}

	CBaseMonster::TraceAttack( attacker, damage, direction, trace, damageType );
}

int CHGruntOp4::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	if ( !attacker )
		return 0;

	CBaseEntity *entity = CBaseEntity::Instance( attacker );

	if ( CheckAttacker( entity ) )
		return 0;

	Forget( bits_MEMORY_INCOVER );

	if ( ( damageType & DMG_CLUB ) && IsFriendly( entity ) )
		damage = 0;

	int result = CSquadTalkMonster::TakeDamage( inflictor, attacker, damage, damageType );

	if ( ( ( !IsAlive() || pev->deadflag == DEAD_DYING ) && !IsPlayerAlly() ) || m_MonsterState == MONSTERSTATE_PRONE )
		return result;

	if ( ( attacker->flags & FL_CLIENT ) && IsPlayerAlly() )
	{
		if ( !m_hEnemy )
		{
			HumanTalkPlayerDamage( this, m_Combat, entity );
			return result;
		}

		if ( m_hEnemy->IsPlayer() || pev->deadflag != DEAD_NO )
			return result;
	}
	else if ( pev->deadflag != DEAD_NO || !IsFriendly( entity ) )
	{
		return result;
	}

	PlaySentence( "FG_SHOT", 4, 1, ATTN_NORM );
	return result;
}

Schedule_t *CHGruntOp4::GetSchedule( void )
{
	if ( !IsTalking() )
		m_hTalkTarget = NULL;

	if ( !HasMemory( bits_MEMORY_SUSPICIOUS ) )
		m_Combat.m_iPlayerDamage = 0;

	m_Combat.m_iSentence = -1;

	// flying? If PRONE, barnacle has me. IF not, it's assumed I am rapelling.
	if ( pev->movetype == MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
	{
		if ( ( pev->flags & FL_ONGROUND ) || pev->groundentity || pev->velocity.z > 0 )
		{
			// just landed
			pev->movetype = MOVETYPE_STEP;
			return GetScheduleOfType( SCHED_OP4_REPEL_LAND );
		}

		if ( m_MonsterState != MONSTERSTATE_COMBAT || ( pev->weapons & HGRUNT_SHOTGUN ) )
			return GetScheduleOfType( SCHED_OP4_REPEL );

		return GetScheduleOfType( SCHED_OP4_REPEL_ATTACK );
	}

	if ( HasConditions( bits_COND_ENEMY_DEAD ) && FOkToSpeak() )
		PlaySentence( "FG_KILL", 4, 1, ATTN_NORM );

	if ( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		Listen();

		CSound *sound = PBestSound();

		if ( sound )
		{
			if ( sound->m_iType & bits_SOUND_BULLETHIT )
				return GetScheduleOfType( SCHED_LOSE_ENEMY );

			if ( sound->m_iType & bits_SOUND_DANGER )
			{
				if ( FOkToSpeak() )
				{
					SENTENCEG_PlayRndSz( edict(), "FG_GREN", 1, ATTN_NORM, 0, m_voicePitch );
					JustSpoke();
				}

				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
			}
		}
	}

	if ( m_MonsterState != MONSTERSTATE_COMBAT )
		return CBaseMonster::GetSchedule();

	Listen();

	CSound *sound = PBestSound();

	if ( sound && ( sound->m_iType & bits_SOUND_BULLETHIT ) )
		return GetScheduleOfType( SCHED_LOSE_ENEMY );

	if ( m_hGuardEnt != NULL )
	{
		if ( m_hTargetEnt == NULL || m_hTargetEnt->edict() != m_hGuardEnt->edict() )
			StartPlayerFollowing( m_hGuardEnt, false );

		if ( m_hTargetEnt != NULL && ( Center() - m_hTargetEnt->Center() ).Length2D() > 256 )
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
				if ( !m_Combat.m_fFirstEncounter )
					return GetScheduleOfType( SCHED_OP4_ESTABLISH_LINE_OF_FIRE );

				m_Combat.m_fFirstEncounter = false;
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
			}

			if ( FOkToSpeak() )
			{
				if ( m_hEnemy != NULL )
					SENTENCEG_PlayRndSz( edict(), "FG_ALERT", 1, ATTN_NORM, 0, m_voicePitch );

				JustSpoke();
			}

			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_OP4_SUPPRESS );

			return GetScheduleOfType( SCHED_OP4_ESTABLISH_LINE_OF_FIRE );
		}
	}
	else
	{
		if ( HasConditions( bits_COND_NO_AMMO_LOADED ) )
			return GetScheduleOfType( SCHED_OP4_COVER_AND_RELOAD );

		if ( HasConditions( bits_COND_LIGHT_DAMAGE ) )
		{
			if ( RANDOM_LONG( 0, 99 ) > 90 || m_hEnemy == NULL )
				return GetScheduleOfType( SCHED_SMALL_FLINCH );

			if ( FOkToSpeak() )
				m_Combat.m_iSentence = HUMAN_SENTENCE_COVER;

			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
		}

		if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
			return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

		if ( m_hEnemy != NULL && FClassnameIs( m_hEnemy->pev, "monster_gargantua" ) )
		{
			if ( pev->weapons & ( HGRUNT_HANDGRENADE | HGRUNT_GRENADELAUNCHER ) )
			{
				if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) )
					return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
			}
		}
		else
		{
			if ( ( pev->weapons & HGRUNT_GRENADELAUNCHER ) && HasConditions( bits_COND_CAN_RANGE_ATTACK2 )
				 && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
			{
				if ( InSquad() && MySquadLeader()->m_fEnemyEluded && !HasConditions( bits_COND_ENEMY_FACING_ME ) )
				{
					MySquadLeader()->m_fEnemyEluded = FALSE;
					return GetScheduleOfType( SCHED_OP4_FOUND_ENEMY );
				}

				if ( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
					return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

				if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
					return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
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

				if ( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) && RANDOM_LONG( 0, 1 ) )
				{
					if ( FOkToSpeak() )
						m_Combat.m_iSentence = HUMAN_SENTENCE_CHARGE;

					return GetScheduleOfType( SCHED_OP4_ESTABLISH_LINE_OF_FIRE );
				}

				if ( FOkToSpeak() && RANDOM_LONG( 0, 1 ) )
				{
					SENTENCEG_PlayRndSz( edict(), "FG_TAUNT", 1, ATTN_NORM, 0, m_voicePitch );
					JustSpoke();
				}

				return GetScheduleOfType( SCHED_STANDOFF );
			}
		}
	}

	if ( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
	{
		m_Combat.m_iSentence = HUMAN_SENTENCE_TAUNT;
		return GetScheduleOfType( SCHED_OP4_ESTABLISH_LINE_OF_FIRE );
	}

	return CBaseMonster::GetSchedule();
}

Schedule_t *CHGruntOp4::GetScheduleOfType( int type )
{
	switch ( type )
	{
		case SCHED_COMBAT_FACE:
			return slOp4CombatFace;
		case SCHED_VICTORY_DANCE:
			if ( InSquad() && !IsLeader() )
				return slOp4Fail;

			return slOp4VictoryDance;
		case SCHED_TAKE_COVER_FROM_ENEMY:
			if ( RANDOM_LONG( 0, 1 ) && InSquad() )
			{
				if ( sk_grunt_handgrenades.value >= 1 && HasConditions( bits_COND_CAN_RANGE_ATTACK2 )
					 && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
				{
					if ( FOkToSpeak() )
					{
						SENTENCEG_PlayRndSz( edict(), "FG_THROW", 1, ATTN_NORM, 0, m_voicePitch );
						JustSpoke();
					}

					return slOp4TossGrenadeCover;
				}

				return slOp4TakeCover;
			}

			if ( RANDOM_LONG( 0, 1 ) || m_hEnemy == NULL || m_hEnemy->pev->movetype == MOVETYPE_FLY
				 || HasConditions( bits_COND_ENEMY_OCCLUDED ) )
				return slOp4TakeCover;

			if ( ( m_hEnemy->Center() - pev->origin ).Length2D() >= 512 || SquadMemberInRange( pev->origin, 256 ) )
				return slOp4TakeCover;

			return slOp4GrenadeCover;
		case SCHED_TAKE_COVER_FROM_BEST_SOUND:
			return slOp4TakeCoverFromBestSound;
		case SCHED_RANGE_ATTACK1:
			return m_Combat.m_fStanding ? slOp4RangeAttack1B : slOp4RangeAttack1A;
		case SCHED_RANGE_ATTACK2:
			return slOp4RangeAttack2;
		case SCHED_FAIL:
			if ( m_hEnemy == NULL )
				return slOp4Fail;

			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			return slOp4CombatFail;
		case SCHED_OP4_SUPPRESS:
			if ( !m_hEnemy->IsPlayer() || !m_Combat.m_fFirstEncounter )
				return slOp4Suppress;

			m_Combat.m_fFirstEncounter = false;
			return slOp4SignalSuppress;
		case SCHED_OP4_ESTABLISH_LINE_OF_FIRE:
			return slOp4EstablishLineOfFire;
		case SCHED_OP4_COVER_AND_RELOAD:
			return slOp4HideReload;
		case SCHED_OP4_SWEEP:
			return slOp4Sweep;
		case SCHED_OP4_FOUND_ENEMY:
			return slOp4FoundEnemy;
		case SCHED_OP4_REPEL:
			if ( pev->velocity.z > -128 )
				pev->velocity.z -= 32;

			return slOp4Repel;
		case SCHED_OP4_REPEL_ATTACK:
			if ( pev->velocity.z > -128 )
				pev->velocity.z -= 32;

			return slOp4RepelAttack;
		case SCHED_OP4_REPEL_LAND:
			return slOp4RepelLand;
		case SCHED_OP4_WAIT_IN_COVER:
			return slOp4WaitInCover;
		case SCHED_OP4_TAKECOVER_FAILED:
			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			return GetScheduleOfType( SCHED_FAIL );
		case SCHED_OP4_ELOF_FAIL:
			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 | bits_COND_SEE_ENEMY ) && NoFriendlyFire() )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			return GetScheduleOfType( SCHED_FIND_ATTACK_POINT );
		default:
			return CSquadTalkMonster::GetScheduleOfType( type );
	}
}

void CHGruntOp4::PainSound( void )
{
	if ( gpGlobals->time <= m_Combat.m_flNextPainTime )
		return;

	static const char *sounds[] = {
		"fgrunt/gr_pain1.wav", "fgrunt/gr_pain2.wav", "fgrunt/gr_pain3.wav",
		"fgrunt/gr_pain4.wav", "fgrunt/gr_pain5.wav"
	};

	// two of the seven draws stay silent
	int sound = RANDOM_LONG( 0, 6 );

	if ( sound < ARRAYSIZE( sounds ) )
		EMIT_SOUND_DYN( edict(), CHAN_VOICE, sounds[sound], 1, ATTN_NORM, 0, m_voicePitch );

	m_Combat.m_flNextPainTime = gpGlobals->time + 1;
}

void CHGruntOp4::DeathSound( void )
{
	static const char *sounds[] = { "fgrunt/death1.wav", "fgrunt/death2.wav", "fgrunt/death3.wav" };

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, sounds[RANDOM_LONG( 0, 2 )], 1, ATTN_IDLE, 0, 100 );
}

void CHGruntOp4::Killed( entvars_t *attacker, int gib )
{
	CSquadTalkMonster::Killed( attacker, gib );
}

void CHGruntOp4::TalkInit( void )
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
	m_szGrp[TLK_PHELLO] = "FG_POK";
	m_szGrp[TLK_PIDLE] = "FG_POK";
	m_szGrp[TLK_PQUESTION] = "FG_POK";
	m_szGrp[TLK_SMELL] = "FG_SMELL";
	m_szGrp[TLK_WOUND] = "FG_WOUND";
	m_szGrp[TLK_MORTAL] = "FG_MORTAL";
	m_szGrp[TLK_PLHURT1] = "!FG_CUREA";
	m_szGrp[TLK_PLHURT2] = "!FG_CUREB";
	m_szGrp[TLK_PLHURT3] = "!FG_CUREC";
}

class CHGruntOp4Medic : public CSquadTalkMonster
{
public:
	CBaseEntity *Kick( void );
	void Shoot( void );
	void Heal( void );
	int ObjectCaps( void ) { return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION; }
	int ISoundMask( void ) { return bits_SOUND_WORLD | bits_SOUND_COMBAT | bits_SOUND_PLAYER | bits_SOUND_DANGER | bits_SOUND_BULLETHIT; }
	int IRelationship( CBaseEntity *target, bool ignoreNoTarget = false );
	float GetPointsForDamage( float damage )
	{
		//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
		return pev->max_health / sk_hgrunt_health.value * 4 * ( damage / pev->max_health );
	}

	void PrescheduleThink( void );
	void IdleSound( void );
	void AlertSound( void );
	void DeclineFollowing( void ) { PlaySentence( "FG_POK", 2, 1, ATTN_NORM ); }
	void GibMonster( void );
	CBaseEntity *DropWeapon( void );
	int TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType );
	void TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType );
	BOOL FOkToSpeak( void );
	void JustSpoke( void );
	void SpeakSentence( void );
	void RunAI( void );
	void Spawn( void );
	void Precache( void );
	void SetupFriendly( void ) { HumanSetupFriendly( this, HUMAN_MEDIC ); }
	int Classify( void ) { return GetClassification( CLASS_HUMAN_MILITARY ); }
	void SetPlayerAlly( bool state ) { SetPlayerAllyDirect( !state ); }
	void KeyValue( KeyValueData *pkvd );
	BOOL FCanCheckAttacks( void ) { return !HasConditions( bits_COND_ENEMY_TOOFAR ); }
	bool CheckTankUsage( void );
	BOOL CheckRangeAttack1( float flDot, float flDist );
	BOOL CheckRangeAttack2( float flDot, float flDist );
	BOOL CheckMeleeAttack1( float flDot, float flDist ) { return HumanCheckMeleeAttack1( this, flDot, flDist ); }
	void CheckAmmo( void ) { HumanCheckAmmo( this, m_Combat, HUMAN_MEDIC ); }
	Vector GetGunPosition( void ) { return HumanGunPosition( this, m_Combat, HUMAN_MEDIC ); }
	void HandleAnimEvent( MonsterEvent_t *event );
	void SetActivity( Activity activity );
	void SetYawSpeed( void ) { pev->yaw_speed = m_Activity == ACT_GLIDE || m_Activity == ACT_FLY ? 200 : 360; }
	void StartTask( Task_t *task );
	void RunTask( Task_t *task );
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int type );
	Schedule_t *GetCombatSchedule( void );
	void PainSound( void );
	void DeathSound( void );
	void Killed( entvars_t *attacker, int gib );
	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	CUSTOM_SCHEDULES;
	bool HealMePlayer( CBaseEntity *player );
	CBaseEntity *BestVisibleDeadAlly( void );
	void DropPatient( void );
	void TalkInit( void );

	HumanCombatData m_Combat;
};

LINK_ENTITY_TO_CLASS( monster_human_medic_ally, CHGruntOp4Medic );

TYPEDESCRIPTION CHGruntOp4Medic::m_SaveData[] =
{
	{ FIELD_TIME, "m_flNextGrenadeCheck", offsetof( CHGruntOp4Medic, m_Combat.m_flNextGrenadeCheck ), 1, 0 },
	{ FIELD_TIME, "m_flNextPainTime", offsetof( CHGruntOp4Medic, m_Combat.m_flNextPainTime ), 1, 0 },
	{ FIELD_VECTOR, "m_vecTossVelocity", offsetof( CHGruntOp4Medic, m_Combat.m_vecTossVelocity ), 1, 0 },
	{ FIELD_CHARACTER, "m_fThrowGrenade", offsetof( CHGruntOp4Medic, m_Combat.m_fThrowGrenade ), 1, 0 },
	{ FIELD_CHARACTER, "m_fStanding", offsetof( CHGruntOp4Medic, m_Combat.m_fStanding ), 1, 0 },
	{ FIELD_CHARACTER, "m_fFirstEncounter", offsetof( CHGruntOp4Medic, m_Combat.m_fFirstEncounter ), 1, 0 },
	{ FIELD_INTEGER, "m_cClipSize", offsetof( CHGruntOp4Medic, m_Combat.m_cClipSize ), 1, 0 },
	DEFINE_FIELD( CHGruntOp4Medic, m_voicePitch, FIELD_INTEGER ),
	{ FIELD_INTEGER, "m_iSentence", offsetof( CHGruntOp4Medic, m_Combat.m_iSentence ), 1, 0 },
};

IMPLEMENT_SAVERESTORE( CHGruntOp4Medic, CSquadTalkMonster );

DEFINE_CUSTOM_SCHEDULES( CHGruntOp4Medic ){
	slOp4MedicRevive,
	slOp4MedicHeal,
	slOp4MedicContinue,
	slOp4MedicNeedleDisarm,
	slOp4MedicFail,
	slOp4MedicCombatFail,
	slOp4MedicVictoryDance,
	slOp4MedicEstablishLineOfFire,
	slOp4MedicFoundEnemy,
	slOp4MedicCombatFace,
	slOp4MedicSignalSuppress,
	slOp4MedicSuppress,
	slOp4MedicWaitInCover,
	slOp4MedicTakeCover,
	slOp4MedicGrenadeCover,
	slOp4MedicTossGrenadeCover,
	slOp4MedicTakeCoverFromBestSound,
	slOp4MedicHideReload,
	slOp4MedicSweep,
	slOp4MedicRangeAttack1A,
	slOp4MedicRangeAttack1B,
	slOp4MedicRangeAttack2,
	slOp4MedicRepel,
	slOp4MedicRepelAttack,
	slOp4MedicRepelLand
};

IMPLEMENT_CUSTOM_SCHEDULES( CHGruntOp4Medic, CSquadTalkMonster );

bool CHGruntOp4Medic::CheckTankUsage( void )
{
	edict_t *patient = m_hHealTarget.Get();

	if ( patient && !patient->free && patient->pvPrivateData )
		return false;

	return CBaseMonster::CheckTankUsage();
}

void CHGruntOp4Medic::Spawn( void )
{
	Precache();
	HumanSpawn( this, m_Combat, HUMAN_MEDIC );
	m_Combat.m_iSentence = -1;
	m_Combat.m_fFirstEncounter = true;
	m_voicePitch = m_Combat.m_voicePitch;
}

void CHGruntOp4Medic::Precache( void )
{
	TalkInit();
	CSquadTalkMonster::Precache();
	HumanPrecache( this, m_Combat, HUMAN_MEDIC );
}

void CHGruntOp4Medic::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "head" ) )
	{
		m_Combat.m_iHead = atoi( pkvd->szValue ) + 25;
		pkvd->fHandled = TRUE;
	}
	else
		CSquadTalkMonster::KeyValue( pkvd );
}

BOOL CHGruntOp4Medic::CheckRangeAttack1( float flDot, float flDist )
{
	if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) || flDist > 2048 || flDot < 0.5f || !NoFriendlyFire() )
		return FALSE;

	CBaseEntity *enemy = m_hEnemy;

	if ( !enemy->IsPlayer() && flDist <= 64 )
		return FALSE;

	Vector standing = pev->origin + Vector( 0, 0, 60 );
	Vector crouching = pev->origin + Vector( 0, 0, 48 );
	TraceResult tr;

	if ( RANDOM_LONG( 0, 9 ) == 0 && m_Activity != ACT_RANGE_ATTACK1 )
		m_Combat.m_fStanding = RANDOM_LONG( 0, 1 ) != 0;

	Vector first = m_Combat.m_fStanding ? standing : crouching;
	Vector second = m_Combat.m_fStanding ? crouching : standing;

	UTIL_TraceLine( first, enemy->BodyTarget( first ), ignore_monsters, ignore_glass, edict(), &tr );

	if ( tr.flFraction == 1.0f || tr.pHit == enemy->edict() )
		return TRUE;

	UTIL_TraceLine( second, enemy->BodyTarget( second ), ignore_monsters, ignore_glass, edict(), &tr );

	if ( tr.flFraction == 1.0f || tr.pHit == enemy->edict() )
	{
		m_Combat.m_fStanding = !m_Combat.m_fStanding;
		return TRUE;
	}

	return FALSE;
}

void CHGruntOp4Medic::SetActivity( Activity activity )
{
	int sequence;

	switch ( activity )
	{
		case ACT_IDLE:
			if ( m_MonsterState == MONSTERSTATE_COMBAT && !m_Combat.m_fGunHolstered )
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
			sequence = LookupSequence( m_Combat.m_fStanding ? "standing_mp5" : "crouching_mp5" );
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

BOOL CHGruntOp4Medic::CheckRangeAttack2( float flDot, float flDist )
{
	return FALSE;
}

CBaseEntity *CHGruntOp4Medic::Kick( void )
{
	return HumanKickTrace( this );
}

void CHGruntOp4Medic::Shoot( void )
{
	if ( !m_hEnemy )
		return;

	Vector source = GetGunPosition();
	Vector direction = ShootAtEnemy( source );

	UTIL_MakeVectors( pev->angles );
	HumanEjectShell( this, source, direction, m_Combat.m_iBrassShell );

	if ( pev->weapons & MEDIC_DEAGLE )
		FireBullets( 1, source, direction, VECTOR_CONE_4DEGREES, 2048, BULLET_MONSTER_EAGLE, 4, 0, NULL, FBDM_DRAW );
	else if ( pev->weapons & MEDIC_GLOCK )
		FireBullets( 1, source, direction, VECTOR_CONE_2DEGREES, 2048, BULLET_MONSTER_9MM, 4, 0, NULL, FBDM_DRAW );

	pev->effects |= EF_MUZZLEFLASH;
	m_cAmmoLoaded--;
	SetBlending( 0, UTIL_VecToAngles( direction ).x );

	if ( pev->movetype != MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
		m_flNextAttack = gpGlobals->time + RANDOM_FLOAT( 0.2f, 0.5f );
}

// While in reach, however the patient is, the medic keeps working
void CHGruntOp4Medic::Heal( void )
{
	CBaseEntity *target = m_hHealTarget;

	if ( !target || ( target->pev->origin - pev->origin ).Length() > MEDIC_HEAL_RANGE )
		return;

	if ( gpGlobals->time > m_Combat.m_flHealTime )
	{
		if ( target->IsAlive() )
			target->TakeHealth( target->pev->health > 25 ? 10 : 25, DMG_MEDKITHEAL );

		m_Combat.m_flHealTime = gpGlobals->time + 0.5f;
	}

	m_flHealGiveUpTime = gpGlobals->time + MEDIC_HEAL_GIVE_UP_DELAY;
}

void CHGruntOp4Medic::HandleAnimEvent( MonsterEvent_t *event )
{
	switch ( event->event )
	{
		case HGRUNT_AE_RELOAD:
			EMIT_SOUND( edict(), CHAN_WEAPON, "hgrunt/gr_reload1.wav", 1, ATTN_NORM );
			m_cAmmoLoaded = m_Combat.m_cClipSize;
			ClearConditions( bits_COND_NO_AMMO_LOADED );
			break;
		case HGRUNT_AE_KICK:
		{
			CBaseEntity *target = Kick();

			if ( target )
				HumanKickHit( this, target, 45 );

			break;
		}
		case HGRUNT_AE_BURST1:
			// never into a friend, while the enemy is alive to shoot at
			if ( !NoFriendlyFire() && m_hEnemy && m_hEnemy->IsAlive() )
				break;

			Shoot();

			if ( pev->weapons & MEDIC_DEAGLE )
				EMIT_SOUND( edict(), CHAN_WEAPON, "otis/ot_attack.wav", 1, ATTN_NORM );
			else if ( pev->weapons & MEDIC_GLOCK )
				EMIT_SOUND( edict(), CHAN_WEAPON, "weapons/pl_gun3.wav", 1, ATTN_NORM );

			if ( FOkToSpeak() && m_hEnemy && m_hEnemy->IsAlive() )
			{
				m_hTalkTarget = m_hEnemy;
				PlaySentence( "FG_ATTACK", RANDOM_FLOAT( 2.8f, 3.2f ), 1, ATTN_NORM );
			}

			CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 0.3f );
			break;
		case HGRUNT_AE_GREN_TOSS:
			UTIL_MakeVectors( pev->angles );
			CGrenade::ShootTimed( pev, GetGunPosition(), m_Combat.m_vecTossVelocity, 3.5f );
			m_Combat.m_fThrowGrenade = FALSE;
			m_Combat.m_flNextGrenadeCheck = gpGlobals->time + 6;
			break;
		case HGRUNT_AE_GREN_LAUNCH:
			EMIT_SOUND( edict(), CHAN_WEAPON, "weapons/glauncher.wav", 0.8f, 0.8f );
			CGrenade::ShootContact( pev, GetGunPosition(), m_Combat.m_vecTossVelocity );
			m_Combat.m_fThrowGrenade = FALSE;
			m_Combat.m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 5, 10 );
			break;
		case HGRUNT_AE_GREN_DROP:
			UTIL_MakeVectors( pev->angles );
			CGrenade::ShootTimed( pev, pev->origin + gpGlobals->v_forward * 17 - gpGlobals->v_right * 27 + gpGlobals->v_up * 6, g_vecZero, 3 );
			break;
		case HGRUNT_AE_CAUGHT_ENEMY:
			if ( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz( edict(), "FG_ALERT", 1, ATTN_NORM, 0, m_voicePitch );
				JustSpoke();
			}

			CSquadTalkMonster::HandleAnimEvent( event );
			break;
		case HGRUNT_AE_DROP_GUN:
			DropWeapon();
			break;
		case MEDIC_AE_HOLSTER_GUN:
		case MEDIC_AE_HOLSTER_NEEDLE:
			m_Combat.m_fGunHolstered = TRUE;
			SetBodygroup( OP4_GUN_GROUP, OP4_GUN_NONE );
			break;
		case MEDIC_AE_EQUIP_NEEDLE:
			m_Combat.m_fGunHolstered = TRUE;
			SetBodygroup( OP4_GUN_GROUP, MEDIC_GUN_NEEDLE );
			break;
		case MEDIC_AE_EQUIP_GUN:
			if ( pev->weapons & MEDIC_DEAGLE )
				SetBodygroup( OP4_GUN_GROUP, MEDIC_GUN_DEAGLE );
			else if ( pev->weapons & MEDIC_GLOCK )
				SetBodygroup( OP4_GUN_GROUP, MEDIC_GUN_GLOCK );

			m_Combat.m_fGunHolstered = FALSE;
			break;
		default:
			CSquadTalkMonster::HandleAnimEvent( event );
			break;
	}
}

void CHGruntOp4Medic::StartTask( Task_t *task )
{
	if ( task->iTask == TASK_OP4_MEDIC_SAY_HEAL && m_pSchedule == slOp4MedicHeal )
	{
		TaskComplete();
		return;
	}

	if ( task->iTask == TASK_OP4_MEDIC_RESET_HEAL
		 && ( m_pSchedule == slOp4MedicHeal || m_pSchedule == slOp4MedicContinue || m_pSchedule == slOp4MedicRevive
			  || m_pSchedule == slOp4MedicNeedleDisarm ) )
	{
		m_hHealTarget = NULL;
		m_hTargetEnt = m_Combat.m_hSavedTarget;
		m_Combat.m_hSavedTarget = NULL;
		m_fReviving = false;
		TaskComplete();
		return;
	}

	m_iTaskStatus = TASKSTATUS_RUNNING;

	if ( task->iTask == TASK_OP4_MEDIC_REVIVE )
	{
		CBaseEntity *target = m_hHealTarget;

		if ( !target || target->IsAlive() || IRelationship( target, true ) > R_NO )
		{
			TaskComplete();
			return;
		}

		CBaseMonster *monster = target->MyMonsterPointer();

		if ( !monster || ( target->pev->effects & EF_NODRAW ) || !target->IsRevivable()
		     || ( monster->m_hHealer != NULL && monster->m_hHealer != this ) )
			TaskFail();
		else
			monster->m_hHealer = this;

		SetSequenceByName( "heal_crouch" );
		return;
	}

	if ( task->iTask == TASK_OP4_MEDIC_HEAL && ( m_pSchedule == slOp4MedicHeal || m_pSchedule == slOp4MedicContinue ) )
	{
		m_IdealActivity = ACT_MELEE_ATTACK2;
		return;
	}

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
		case TASK_OP4_FACE_TOSS_DIR:
			break;
		case TASK_OP4_SPEAK_SENTENCE:
			SpeakSentence();
			TaskComplete();
			break;
		case TASK_OP4_CHECK_FIRE:
			TaskComplete();

			if ( !NoFriendlyFire() )
			{
				ChangeSchedule( GetScheduleOfType( SCHED_FIND_ATTACK_POINT ) );
				SetConditions( bits_COND_GRUNT_NOFIRE );
			}
			break;
		default:
			CSquadTalkMonster::StartTask( task );
			break;
	}
}

void CHGruntOp4Medic::RunTask( Task_t *task )
{
	if ( task->iTask == TASK_OP4_MEDIC_HEAL && ( m_pSchedule == slOp4MedicHeal || m_pSchedule == slOp4MedicContinue ) )
	{
		CBaseEntity *target = m_hHealTarget;

		if ( !target || target->pev->health >= target->pev->max_health )
		{
			TaskComplete();
			return;
		}

		if ( TargetDistance() > MEDIC_HEAL_FOLLOW_DIST )
		{
			if ( target->IsAlive() )
			{
				ChangeSchedule( slOp4MedicContinue );
			}
			else
			{
				ChangeSchedule( slOp4MedicRevive );
				m_fReviving = true;
			}
		}

		MakeIdealYaw( target->pev->origin );
		ChangeYaw( pev->yaw_speed );
		Heal();

		return;
	}

	if ( task->iTask == TASK_OP4_MEDIC_REVIVE )
	{
		CBaseEntity *target = m_hHealTarget;

		if ( !target )
		{
			if ( m_fSequenceFinished )
				TaskComplete();

			return;
		}

		CBaseMonster *monster = target->MyMonsterPointer();

		MakeIdealYaw( target->pev->origin );
		ChangeYaw( pev->yaw_speed );

		// a fading corpse stays visible while it is worked on
		target->pev->renderamt = monster ? V_max( monster->m_flOriginalRenderAmount, MEDIC_REVIVE_MIN_RENDERAMT ) : 255;

		if ( !m_fSequenceFinished )
			return;

		if ( monster && !target->IsAlive() && monster->m_hHealer == this && IRelationship( target, true ) <= R_NO
		     && !( target->pev->effects & EF_NODRAW ) && target->IsRevivable() )
		{
			monster->m_flReviveTime = gpGlobals->time;
			target->pev->flags |= FL_FROZEN;
			monster->SetThink( &CBaseMonster::ReviveThink );
			target->pev->nextthink = gpGlobals->time + 0.5f;
		}

		if ( monster )
			monster->m_hHealer = NULL;

		m_hHealTarget = NULL;
		m_fReviving = false;
		TaskComplete();
		return;
	}

	if ( task->iTask == TASK_OP4_FACE_TOSS_DIR )
	{
		MakeIdealYaw( pev->origin + m_Combat.m_vecTossVelocity * 64 );
		ChangeYaw( pev->yaw_speed );

		if ( FacingIdeal() )
			TaskComplete();

		return;
	}

	CSquadTalkMonster::RunTask( task );
}

// The patient, if any, no longer has this medic
void CHGruntOp4Medic::DropPatient( void )
{
	CBaseEntity *patient = m_hHealTarget;

	if ( !patient )
		return;

	CBaseMonster *monster = patient->MyMonsterPointer();

	if ( monster )
		monster->m_hHealer = NULL;

	m_hHealTarget = NULL;
	m_fReviving = false;
}

//=========================================================
// BestVisibleDeadAlly - the nearest corpse, among the
// visible monsters and all players, this medic may revive
//=========================================================
#define MEDIC_CORPSE_MIN_RENDERAMT	10
#define MEDIC_CORPSE_MAX_DIST		65536

CBaseEntity *CHGruntOp4Medic::BestVisibleDeadAlly( void )
{
	CBaseEntity *best = NULL;
	int bestDist = MEDIC_CORPSE_MAX_DIST;

	for ( CBaseEntity *entity = m_pSightLink; entity; entity = entity->m_pSightLink )
	{
		if ( entity->IsAlive() || entity->IsBSPModel() || entity->BloodColor() == DONT_BLEED
		     || entity->pev->renderamt <= MEDIC_CORPSE_MIN_RENDERAMT )
			continue;

		CBaseMonster *monster = entity->MyMonsterPointer();

		if ( !monster || monster->m_hHealer != NULL || ( entity->pev->flags & FL_FROZEN ) || IRelationship( entity, true ) > R_NO )
			continue;

		// a monster corpse that began to fade shows again
		if ( entity->pev->flags & FL_MONSTER )
			entity->pev->renderamt = 255;

		int dist = (int) ( entity->Center() - Center() ).Length();

		if ( dist < bestDist )
		{
			bestDist = dist;
			best = entity;
		}
	}

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *client = UTIL_PlayerByIndex( i );
		CBaseMonster *player = client ? client->MyMonsterPointer() : NULL;

		if ( !player || ( player->m_hHealer != NULL && player->m_hHealer != this ) || ( player->pev->flags & FL_FROZEN )
		     || player->IsAlive() || IRelationship( player, true ) > R_NO )
			continue;

		int dist = (int) ( player->Center() - Center() ).Length();

		if ( dist < bestDist )
		{
			bestDist = dist;
			best = player;
		}
	}

	return best;
}

BOOL CHGruntOp4Medic::FOkToSpeak( void )
{
	if ( !IsAlive() )
		return FALSE;

	float wait = IsPlayerAlly() ? CSquadTalkMonster::g_talkWaitTime : CSquadTalkMonster::g_talkWaitTimeEvil;

	if ( wait > gpGlobals->time )
		return FALSE;

	return !( pev->spawnflags & SF_MONSTER_GAG ) || m_MonsterState == MONSTERSTATE_COMBAT;
}

void CHGruntOp4Medic::JustSpoke( void )
{
	if ( IsPlayerAlly() )
		CSquadTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT( 2.5f, 3 );
	else
		CSquadTalkMonster::g_talkWaitTimeEvil = gpGlobals->time + RANDOM_FLOAT( 2.5f, 3 );

	m_Combat.m_iSentence = -1;
}

void CHGruntOp4Medic::SpeakSentence( void )
{
	if ( m_Combat.m_iSentence == -1 || !FOkToSpeak() )
		return;

	SENTENCEG_PlayRndSz( edict(), g_HumanSentences[m_Combat.m_iSentence], 1, ATTN_NORM, 0, m_voicePitch );
	JustSpoke();
}

void CHGruntOp4Medic::IdleSound( void )
{
	if ( !FOkToSpeak() )
		return;

	if ( !g_fOp4MedicQuestion )
	{
		if ( !RANDOM_LONG( 0, 1 ) )
			return;

		switch ( RANDOM_LONG( 0, 2 ) )
		{
			case 0:
				SENTENCEG_PlayRndSz( edict(), "FG_CHECK", 1, ATTN_NORM, 0, m_voicePitch );
				g_fOp4MedicQuestion = 1;
				break;
			case 1:
				SENTENCEG_PlayRndSz( edict(), "FG_QUEST", 1, ATTN_NORM, 0, m_voicePitch );
				g_fOp4MedicQuestion = 2;
				break;
			case 2:
				SENTENCEG_PlayRndSz( edict(), "FG_IDLE", 1, ATTN_NORM, 0, m_voicePitch );
				break;
		}
	}
	else
	{
		if ( g_fOp4MedicQuestion == 1 )
			SENTENCEG_PlayRndSz( edict(), "FG_CLEAR", 1, ATTN_NORM, 0, m_voicePitch );
		else if ( g_fOp4MedicQuestion == 2 )
			SENTENCEG_PlayRndSz( edict(), "FG_ANSWER", 1, ATTN_NORM, 0, m_voicePitch );

		g_fOp4MedicQuestion = 0;
	}

	JustSpoke();
}

void CHGruntOp4Medic::PrescheduleThink( void )
{
	if ( !InSquad() || !m_hEnemy )
		return;

	if ( HasConditions( bits_COND_SEE_ENEMY ) )
		MySquadLeader()->m_flLastEnemySightTime = gpGlobals->time;
	else if ( gpGlobals->time - MySquadLeader()->m_flLastEnemySightTime > 5 )
		MySquadLeader()->m_fEnemyEluded = TRUE;
}

void CHGruntOp4Medic::AlertSound( void )
{
	if ( FOkToSpeak() )
		PlaySentence( "FG_ATTACK", RANDOM_FLOAT( 2.8f, 3.2f ), 1, ATTN_NORM );
}

int CHGruntOp4Medic::IRelationship( CBaseEntity *target, bool ignoreNoTarget )
{
	if ( target && !target->m_fOverrideClass && Classify() == CLASS_HUMAN_MILITARY
		 && ( FClassnameIs( target->pev, "monster_alien_grunt" ) || FClassnameIs( target->pev, "monster_gargantua" )
			  || FClassnameIs( target->pev, "monster_bigmomma" ) ) )
		return R_NM;

	return CSquadTalkMonster::IRelationship( target, ignoreNoTarget );
}

CBaseEntity *CHGruntOp4Medic::DropWeapon( void )
{
	if ( pev->weapons == MEDIC_DEAGLE )
		return CBaseMonster::DropWeapon( "weapon_eagle", false );

	if ( pev->weapons == MEDIC_GLOCK )
		return CBaseMonster::DropWeapon( "weapon_9mmhandgun", false );

	return NULL;
}

void CHGruntOp4Medic::GibMonster( void )
{
	DropPatient();

	if ( GetBodygroup( OP4_GUN_GROUP ) != OP4_GUN_NONE )
		DropWeapon();

	CBaseMonster::GibMonster();
}

// Drawing and holstering run at double speed while the medic, or its patient, has an enemy
void CHGruntOp4Medic::RunAI( void )
{
	bool fEnemy = m_hEnemy != NULL;

	if ( !fEnemy && m_hHealTarget != NULL )
	{
		CBaseMonster *patient = m_hHealTarget->MyMonsterPointer();

		fEnemy = patient && patient->m_hEnemy != NULL;
	}

	pev->framerate = fEnemy && ( m_Activity == ACT_ARM || m_Activity == ACT_DISARM ) ? 2 : 1;

	CSquadTalkMonster::RunAI();
}

void CHGruntOp4Medic::TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType )
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
				if ( damage >= 5 )
					damage -= 5;

				damage *= 0.5;
			}
			break;
		case HITGROUP_HEADGEAR:
			trace->iHitgroup = HITGROUP_HEAD;
			break;
	}

	CBaseMonster::TraceAttack( attacker, damage, direction, trace, damageType );
}

int CHGruntOp4Medic::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	if ( !attacker )
		return 0;

	CBaseEntity *entity = CBaseEntity::Instance( attacker );

	if ( CheckAttacker( entity ) )
		return 0;

	Forget( bits_MEMORY_INCOVER );

	if ( ( damageType & DMG_CLUB ) && IsFriendly( entity ) )
		damage = 0;

	int result = CSquadTalkMonster::TakeDamage( inflictor, attacker, damage, damageType );

	if ( ( ( !IsAlive() || pev->deadflag == DEAD_DYING ) && !IsPlayerAlly() ) || m_MonsterState == MONSTERSTATE_PRONE
		 || !( attacker->flags & FL_CLIENT ) || !IsPlayerAlly() )
		return result;

	if ( !m_hEnemy )
	{
		HumanTalkPlayerDamage( this, m_Combat, entity );
		return result;
	}

	if ( !m_hEnemy->IsPlayer() && pev->deadflag == DEAD_NO )
		PlaySentence( "FG_SHOT", 4, 1, ATTN_NORM );

	return result;
}

Schedule_t *CHGruntOp4Medic::GetSchedule( void )
{
	if ( !IsAlive() )
		DropPatient();

	if ( m_Combat.m_fGunHolstered )
		return GetScheduleOfType( SCHED_OP4_MEDIC_NEEDLE_DISARM );

	CBaseEntity *dead = BestVisibleDeadAlly();

	if ( dead && ReviveMe( dead ) )
		return slOp4MedicRevive;

	if ( gpGlobals->time > m_flHealGiveUpTime )
		DropPatient();

	if ( m_hHealTarget != NULL && !m_fReviving && m_MonsterState != MONSTERSTATE_COMBAT )
		return slOp4MedicHeal;

	return GetCombatSchedule();
}

// The combat half of the medic's schedule selection
Schedule_t *CHGruntOp4Medic::GetCombatSchedule( void )
{
	if ( !HasMemory( bits_MEMORY_SUSPICIOUS ) )
		m_Combat.m_iPlayerDamage = 0;

	m_Combat.m_iSentence = -1;

	// flying? If PRONE, barnacle has me. IF not, it's assumed I am rapelling.
	if ( pev->movetype == MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
	{
		if ( ( pev->flags & FL_ONGROUND ) || pev->groundentity || pev->velocity.z > 0 )
		{
			// just landed
			pev->movetype = MOVETYPE_STEP;
			return GetScheduleOfType( SCHED_OP4_REPEL_LAND );
		}

		if ( m_MonsterState == MONSTERSTATE_COMBAT )
			return GetScheduleOfType( SCHED_OP4_REPEL_ATTACK );

		return GetScheduleOfType( SCHED_OP4_REPEL );
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

	if ( m_hGuardEnt != NULL )
	{
		if ( m_hTargetEnt == NULL || m_hTargetEnt->edict() != m_hGuardEnt->edict() )
			StartPlayerFollowing( m_hGuardEnt, false );

		if ( m_hTargetEnt != NULL && ( Center() - m_hTargetEnt->Center() ).Length2D() > 256 )
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
				if ( !m_Combat.m_fFirstEncounter )
					return GetScheduleOfType( SCHED_OP4_ESTABLISH_LINE_OF_FIRE );

				m_Combat.m_fFirstEncounter = false;
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
			}

			if ( FOkToSpeak() )
			{
				if ( m_hEnemy != NULL )
					SENTENCEG_PlayRndSz( edict(), "FG_ALERT", 1, ATTN_NORM, 0, m_voicePitch );

				JustSpoke();
			}

			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_OP4_SUPPRESS );

			return GetScheduleOfType( SCHED_OP4_ESTABLISH_LINE_OF_FIRE );
		}
	}
	else
	{
		if ( HasConditions( bits_COND_NO_AMMO_LOADED ) )
			return GetScheduleOfType( SCHED_OP4_COVER_AND_RELOAD );

		if ( HasConditions( bits_COND_LIGHT_DAMAGE ) )
		{
			if ( RANDOM_LONG( 0, 99 ) > 90 || m_hEnemy == NULL )
				return GetScheduleOfType( SCHED_SMALL_FLINCH );

			if ( FOkToSpeak() )
				m_Combat.m_iSentence = HUMAN_SENTENCE_COVER;

			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
		}

		if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
			return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

		if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
		{
			if ( InSquad() && MySquadLeader()->m_fEnemyEluded && !HasConditions( bits_COND_ENEMY_FACING_ME ) )
			{
				MySquadLeader()->m_fEnemyEluded = FALSE;
				return GetScheduleOfType( SCHED_OP4_FOUND_ENEMY );
			}

			if ( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
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
					m_Combat.m_iSentence = HUMAN_SENTENCE_CHARGE;

				return GetScheduleOfType( SCHED_OP4_ESTABLISH_LINE_OF_FIRE );
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
		m_Combat.m_iSentence = HUMAN_SENTENCE_TAUNT;
		return GetScheduleOfType( SCHED_OP4_ESTABLISH_LINE_OF_FIRE );
	}

	return CBaseMonster::GetSchedule();
}

Schedule_t *CHGruntOp4Medic::GetScheduleOfType( int type )
{
	switch ( type )
	{
		case SCHED_COMBAT_FACE:
			return slOp4MedicCombatFace;
		case SCHED_VICTORY_DANCE:
			if ( InSquad() && !IsLeader() )
				return slOp4MedicFail;

			return slOp4MedicVictoryDance;
		case SCHED_TAKE_COVER_FROM_ENEMY:
			if ( !InSquad() )
			{
				// this path still consumes the random draw
				RANDOM_LONG( 0, 1 );
				return slOp4MedicTakeCover;
			}

			if ( sk_grunt_handgrenades.value < 1 || !HasConditions( bits_COND_CAN_RANGE_ATTACK2 )
				 || !OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
				return slOp4MedicTakeCover;

			if ( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz( edict(), "FG_THROW", 1, ATTN_NORM, 0, m_voicePitch );
				JustSpoke();
			}

			return slOp4MedicTossGrenadeCover;
		case SCHED_TAKE_COVER_FROM_BEST_SOUND:
			return slOp4MedicTakeCoverFromBestSound;
		case SCHED_RANGE_ATTACK1:
			if ( m_Combat.m_fGunHolstered )
				return slOp4MedicNeedleDisarm;

			return m_Combat.m_fStanding ? slOp4MedicRangeAttack1B : slOp4MedicRangeAttack1A;
		case SCHED_RANGE_ATTACK2:
			return slOp4MedicRangeAttack2;
		case SCHED_FAIL:
			if ( m_hEnemy != NULL )
				return slOp4MedicCombatFail;

			m_hHealTarget = NULL;
			m_fReviving = false;
			return slOp4MedicFail;
		case SCHED_OP4_SUPPRESS:
			if ( m_Combat.m_fGunHolstered )
				return slOp4MedicNeedleDisarm;

			if ( !m_hEnemy->IsPlayer() || !m_Combat.m_fFirstEncounter )
				return slOp4MedicSuppress;

			m_Combat.m_fFirstEncounter = false;
			return slOp4MedicSignalSuppress;
		case SCHED_OP4_ESTABLISH_LINE_OF_FIRE:
			return slOp4MedicEstablishLineOfFire;
		case SCHED_OP4_COVER_AND_RELOAD:
			return slOp4MedicHideReload;
		case SCHED_OP4_SWEEP:
			return slOp4MedicSweep;
		case SCHED_OP4_FOUND_ENEMY:
			return slOp4MedicFoundEnemy;
		case SCHED_OP4_REPEL:
			if ( pev->velocity.z > -128 )
				pev->velocity.z -= 32;

			return slOp4MedicRepel;
		case SCHED_OP4_REPEL_ATTACK:
			if ( pev->velocity.z > -128 )
				pev->velocity.z -= 32;

			return slOp4MedicRepelAttack;
		case SCHED_OP4_REPEL_LAND:
			return slOp4MedicRepelLand;
		case SCHED_OP4_WAIT_IN_COVER:
			return slOp4MedicWaitInCover;
		case SCHED_OP4_TAKECOVER_FAILED:
			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			return GetScheduleOfType( SCHED_FAIL );
		case SCHED_OP4_ELOF_FAIL:
			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 | bits_COND_SEE_ENEMY ) && NoFriendlyFire() )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			return GetScheduleOfType( SCHED_FIND_ATTACK_POINT );
		case SCHED_OP4_MEDIC_NEEDLE_DISARM:
			return slOp4MedicNeedleDisarm;
		default:
			return CSquadTalkMonster::GetScheduleOfType( type );
	}
}

void CHGruntOp4Medic::PainSound( void )
{
	if ( gpGlobals->time <= m_Combat.m_flNextPainTime )
		return;

	static const char *sounds[] = {
		"fgrunt/gr_pain1.wav", "fgrunt/gr_pain2.wav", "fgrunt/gr_pain3.wav",
		"fgrunt/gr_pain4.wav", "fgrunt/gr_pain5.wav"
	};

	// two of the seven draws stay silent
	int sound = RANDOM_LONG( 0, 6 );

	if ( sound < ARRAYSIZE( sounds ) )
		EMIT_SOUND_DYN( edict(), CHAN_VOICE, sounds[sound], 1, ATTN_NORM, 0, m_voicePitch );

	m_Combat.m_flNextPainTime = gpGlobals->time + 1;
}

void CHGruntOp4Medic::DeathSound( void )
{
	static const char *sounds[] = { "fgrunt/death1.wav", "fgrunt/death2.wav", "fgrunt/death3.wav" };

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, sounds[RANDOM_LONG( 0, 2 )], 1, ATTN_IDLE, 0, 100 );
}

void CHGruntOp4Medic::Killed( entvars_t *attacker, int gib )
{
	CSquadTalkMonster::Killed( attacker, gib );
}

void CHGruntOp4Medic::TalkInit( void )
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
	m_szGrp[TLK_PHELLO] = "FG_POK";
	m_szGrp[TLK_PIDLE] = "FG_POK";
	m_szGrp[TLK_PQUESTION] = "FG_POK";
	m_szGrp[TLK_SMELL] = "FG_SMELL";
	m_szGrp[TLK_WOUND] = "FG_WOUND";
	m_szGrp[TLK_MORTAL] = "FG_MORTAL";
	m_szGrp[TLK_PLHURT1] = "!FG_CUREA";
	m_szGrp[TLK_PLHURT2] = "!FG_CUREB";
	m_szGrp[TLK_PLHURT3] = "!FG_CUREC";
}

bool CHGruntOp4Medic::HealMePlayer( CBaseEntity *player )
{
	if ( !player || IRelationship( player, true ) > R_NO || pev->movetype == MOVETYPE_FLY
	     || m_MonsterState == MONSTERSTATE_SCRIPT || !IsAlive() || m_hCineBlocker != NULL )
		return false;

	if ( m_hHealTarget != NULL )
		return false;

	if ( !player->IsAlive() || player->pev->health >= player->pev->max_health )
		return false;

	m_Combat.m_hSavedTarget = m_hTargetEnt;
	m_flHealGiveUpTime = gpGlobals->time + MEDIC_GIVE_UP_DELAY;
	m_hHealTarget = player;
	m_hTargetEnt = player;
	ChangeSchedule( slOp4MedicHeal );
	return true;
}

//=========================================================
// HealMe / ReviveMe - this medic takes a hurt or dead ally
// as its patient
//=========================================================
#define MEDIC_HEAL_ROUTE_DIST	64

bool CSquadTalkMonster::HealMe( CBaseEntity *pTarget )
{
	if ( IRelationship( pTarget, true ) > R_NO || pTarget == this )
		return false;

	CBaseMonster *pMonster = pTarget->MyMonsterPointer();

	if ( !pMonster || pMonster->m_bloodColor == DONT_BLEED || pTarget->Classify() == CLASS_MACHINE )
		return false;

	if ( pTarget->pev->movetype == MOVETYPE_FLY || pev->movetype == MOVETYPE_FLY || m_MonsterState == MONSTERSTATE_SCRIPT )
		return false;

	// busy with another patient, unless that one is a medic
	CBaseEntity *pPatient = m_hHealTarget;

	if ( pPatient && pPatient != pTarget && !FClassnameIs( pPatient->pev, "monster_human_medic_ally" ) )
		return false;

	if ( !IsAlive() || m_hCineBlocker != NULL )
		return false;

	// the goal is offset by whatever v_forward was made last
	if ( !TestBuildRoute( pTarget->Center() - gpGlobals->v_forward * MEDIC_HEAL_ROUTE_DIST, bits_MF_TO_LOCATION, NULL ) )
		return false;

	m_flHealGiveUpTime = gpGlobals->time + MEDIC_GIVE_UP_DELAY;
	m_hHealTarget = pTarget;
	m_hTargetEnt = m_hHealTarget;
	m_fReviving = false;
	ChangeSchedule( slOp4MedicHeal );

	return true;
}

bool CSquadTalkMonster::ReviveMe( CBaseEntity *pTarget )
{
	if ( IRelationship( pTarget, true ) > R_NO || pTarget == this )
		return false;

	CBaseMonster *pMonster = pTarget->MyMonsterPointer();

	if ( !pMonster || pMonster->m_bloodColor == DONT_BLEED || pTarget->Classify() == CLASS_MACHINE )
		return false;

	if ( pTarget->pev->movetype == MOVETYPE_FLY || ( pTarget->pev->effects & EF_NODRAW ) )
		return false;

	if ( pev->movetype == MOVETYPE_FLY || m_MonsterState == MONSTERSTATE_SCRIPT )
		return false;

	CBaseEntity *pPatient = m_hHealTarget;

	if ( pPatient && pPatient != pTarget && !FClassnameIs( pPatient->pev, "monster_human_medic_ally" ) )
		return false;

	// a corpse that no longer thinks can't come back
	if ( !IsAlive() || m_hCineBlocker != NULL || !pTarget->m_pfnThink )
		return false;

	m_flHealGiveUpTime = gpGlobals->time + MEDIC_GIVE_UP_DELAY;
	m_hHealTarget = pTarget;
	m_hTargetEnt = m_hHealTarget;
	m_fReviving = true;
	ChangeSchedule( slOp4MedicRevive );

	return true;
}

#define MASSN_SNIPER_LEAD	15000

class CMassn : public CSquadMonster
{
public:
	CMassn() { m_Combat.m_iHead = -1; }
	CBaseEntity *Kick( void );
	void Shoot( void );
	void Sniper( void );
	int ObjectCaps( void ) { return IsPlayerAlly() ? FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION; }
	int ISoundMask( void ) { return bits_SOUND_WORLD | bits_SOUND_COMBAT | bits_SOUND_PLAYER | bits_SOUND_DANGER | bits_SOUND_BULLETHIT; }
	int IRelationship( CBaseEntity *target, bool ignoreNoTarget = false );
	float GetPointsForDamage( float damage )
	{
		//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
		return pev->max_health / sk_hgrunt_health.value * 5 * ( damage / pev->max_health );
	}

	void RunAI( void );
	void PrescheduleThink( void );
	void UseSound( void );
	void UnUseSound( void );
	void GibMonster( void );
	CBaseEntity *DropWeapon( void );
	int TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType );
	void TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType );
	void Spawn( void );
	void Precache( void );
	void SetupFriendly( void ) { HumanSetupFriendly( this, HUMAN_MASSN ); }
	int Classify( void ) { return GetClassification( CLASS_HUMAN_MILITARY ); }
	void KeyValue( KeyValueData *pkvd );
	BOOL FCanCheckAttacks( void );
	BOOL CheckRangeAttack1( float flDot, float flDist );
	BOOL CheckRangeAttack2( float flDot, float flDist );
	BOOL CheckMeleeAttack1( float flDot, float flDist ) { return HumanCheckMeleeAttack1( this, flDot, flDist ); }
	void CheckAmmo( void ) { HumanCheckAmmo( this, m_Combat, HUMAN_MASSN ); }
	Vector GetGunPosition( void );
	void HandleAnimEvent( MonsterEvent_t *event );
	void SetActivity( Activity activity );
	void SetYawSpeed( void ) { pev->yaw_speed = m_Activity == ACT_GLIDE || m_Activity == ACT_FLY ? 270 : 360; }
	void StartTask( Task_t *task );
	void RunTask( Task_t *task );
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int type );
	Schedule_t *GetUnarmedSchedule( void );
	void DeathSound( void );
	void Killed( entvars_t *attacker, int gib );
	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	CUSTOM_SCHEDULES;

	HumanCombatData m_Combat;
};

LINK_ENTITY_TO_CLASS( monster_male_assassin, CMassn );

TYPEDESCRIPTION CMassn::m_SaveData[] =
{
	{ FIELD_TIME, "m_flNextGrenadeCheck", offsetof( CMassn, m_Combat.m_flNextGrenadeCheck ), 1, 0 },
	{ FIELD_TIME, "m_flNextPainTime", offsetof( CMassn, m_Combat.m_flNextPainTime ), 1, 0 },
	{ FIELD_VECTOR, "m_vecTossVelocity", offsetof( CMassn, m_Combat.m_vecTossVelocity ), 1, 0 },
	{ FIELD_CHARACTER, "m_fThrowGrenade", offsetof( CMassn, m_Combat.m_fThrowGrenade ), 1, 0 },
	{ FIELD_CHARACTER, "m_fStanding", offsetof( CMassn, m_Combat.m_fStanding ), 1, 0 },
	{ FIELD_CHARACTER, "m_fFirstEncounter", offsetof( CMassn, m_Combat.m_fFirstEncounter ), 1, 0 },
	{ FIELD_INTEGER, "m_cClipSize", offsetof( CMassn, m_Combat.m_cClipSize ), 1, 0 },
	{ FIELD_INTEGER, "m_voicePitch", offsetof( CMassn, m_Combat.m_voicePitch ), 1, 0 },
};

IMPLEMENT_SAVERESTORE( CMassn, CSquadMonster );

DEFINE_CUSTOM_SCHEDULES( CMassn ){
	slMassnFail,
	slMassnCombatFail,
	slMassnCombatFailB,
	slMassnVictoryDance,
	slMassnEstablishLineOfFire,
	slMassnFoundEnemy,
	slMassnCombatFace,
	slMassnSignalSuppress,
	slMassnSuppress,
	slMassnWaitInCover,
	slMassnTakeCover,
	slMassnGrenadeCover,
	slMassnTossGrenadeCover,
	slMassnTakeCoverFromBestSound,
	slMassnHideReload,
	slMassnHideReloadB,
	slMassnSweep,
	slMassnRangeAttack1A,
	slMassnRangeAttack1B,
	slMassnRangeAttack1C,
	slMassnRangeAttack1D,
	slMassnRangeAttack2,
	slMassnRepel,
	slMassnRepelAttack,
	slMassnRepelLand,
	slMassnChaseEnemy
};

IMPLEMENT_CUSTOM_SCHEDULES( CMassn, CSquadMonster );

void CMassn::Spawn( void )
{
	Precache();
	HumanSpawn( this, m_Combat, HUMAN_MASSN );
}

void CMassn::Precache( void )
{
	CSquadMonster::Precache();
	HumanPrecache( this, m_Combat, HUMAN_MASSN );
}

void CMassn::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "head" ) )
	{
		m_Combat.m_iHead = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CSquadMonster::KeyValue( pkvd );
}

CBaseEntity *CMassn::Kick( void )
{
	return HumanKickTrace( this );
}

void CMassn::Shoot( void )
{
	if ( !m_hEnemy )
		return;

	UTIL_MakeVectors( pev->angles );

	Vector source = GetGunPosition();
	Vector direction = ShootAtEnemy( source );

	HumanEjectShell( this, source, direction, m_Combat.m_iBrassShell );
	SetBlending( 0, UTIL_VecToAngles( direction ).x );
	pev->effects |= EF_MUZZLEFLASH;
	m_cAmmoLoaded--;

	if ( pev->weapons & HGRUNT_GRENADELAUNCHER )
	{
		FireBullets( 1, source, direction, VECTOR_CONE_6DEGREES, 2048, BULLET_MONSTER_SAW, 4, 0, NULL, FBDM_DRAW );
		return;
	}

	FireBullets( 1, source, direction, VECTOR_CONE_8DEGREES, 2048, BULLET_MONSTER_MP5, 4, 0, NULL, FBDM_DRAW );

	// the sequence ends with each shot, so the rate of fire follows the event
	if ( pev->movetype != MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
	{
		m_fSequenceFinished = TRUE;
		m_Activity = ACT_RESET;
	}
}

// Dead accurate, and leading the target
void CMassn::Sniper( void )
{
	if ( !m_hEnemy || pev->weapons == MASSN_WEAPON_NONE || pev->weapons == MASSN_WEAPON_LOST )
		return;

	UTIL_MakeVectors( pev->angles );

	Vector source = GetGunPosition();
	Vector direction = ShootAtEnemy( source ) - m_hEnemy->pev->velocity / MASSN_SNIPER_LEAD;

	HumanEjectShell( this, source, direction, m_Combat.m_iBrassShell );
	FireBullets( 1, source, direction, g_vecZero, 8192, BULLET_MONSTER_SNIPER, 0, 0, NULL, FBDM_DRAW );
	SetBlending( 0, UTIL_VecToAngles( direction ).x );
	pev->effects |= EF_MUZZLEFLASH;
	m_cAmmoLoaded--;
}

void CMassn::HandleAnimEvent( MonsterEvent_t *event )
{
	switch ( event->event )
	{
		case HGRUNT_AE_RELOAD:
			EMIT_SOUND( edict(), CHAN_WEAPON, ( pev->weapons & MASSN_SNIPERRIFLE ) ? "weapons/sniper_bolt1.wav" : "hgrunt/gr_reload1.wav", 1, ATTN_NORM );
			m_cAmmoLoaded = m_Combat.m_cClipSize;
			ClearConditions( bits_COND_NO_AMMO_LOADED );
			break;
		case HGRUNT_AE_KICK:
		{
			if ( pev->sequence == LookupSequence( "frontkick_3" ) )
			{
				UTIL_MakeVectors( pev->angles );

				if ( pev->flags & FL_ONGROUND )
					pev->velocity = pev->velocity + gpGlobals->v_up * 80;

				pev->velocity = pev->velocity + gpGlobals->v_forward * 120;
				pev->flags &= ~FL_ONGROUND;
			}

			CBaseEntity *target = Kick();

			if ( !target )
				break;

			static const char *sounds[] = { "weapons/cbar_hitbod1.wav", "weapons/cbar_hitbod2.wav", "weapons/cbar_hitbod3.wav" };

			EMIT_SOUND( edict(), CHAN_VOICE, sounds[RANDOM_LONG( 0, 2 )], 0.9f, 2 );
			HumanKickHit( this, target, -84 );
			break;
		}
		case HGRUNT_AE_BURST1:
			if ( pev->weapons & HGRUNT_9MMAR )
			{
				static const char *sounds[] = { "weapons/hks1.wav", "weapons/hks2.wav", "weapons/hks3.wav" };

				Shoot();
				EMIT_SOUND( edict(), CHAN_WEAPON, ( pev->weapons & HGRUNT_GRENADELAUNCHER ) ? "weapons/m16_3round.wav" : sounds[RANDOM_LONG( 0, 2 )], 1, ATTN_NORM );
			}
			else
			{
				Sniper();
				EMIT_SOUND( edict(), CHAN_WEAPON, "weapons/sniper_fire.wav", 1, 0.4f );
			}

			CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 0.3f );
			break;
		case HGRUNT_AE_BURST2:
		case HGRUNT_AE_BURST3:
			Shoot();
			break;
		case HGRUNT_AE_GREN_TOSS:
			UTIL_MakeVectors( pev->angles );
			CGrenade::ShootTimed( pev, GetGunPosition(), m_Combat.m_vecTossVelocity, 3.5f );
			m_Combat.m_fThrowGrenade = FALSE;
			m_Combat.m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 5, 10 );
			break;
		case HGRUNT_AE_GREN_LAUNCH:
			EMIT_SOUND( edict(), CHAN_WEAPON, "weapons/glauncher.wav", 0.8f, 0.8f );
			CGrenade::ShootContact( pev, GetGunPosition(), m_Combat.m_vecTossVelocity );
			m_Combat.m_fThrowGrenade = FALSE;
			m_Combat.m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 4, 8 );
			break;
		case HGRUNT_AE_GREN_DROP:
			UTIL_MakeVectors( pev->angles );
			CGrenade::ShootTimed( pev, pev->origin + gpGlobals->v_forward * 17 - gpGlobals->v_right * 27 + gpGlobals->v_up * 6, g_vecZero, 3 );
			break;
		case HGRUNT_AE_DROP_GUN:
			if ( DropWeapon() )
				SetBodygroup( GUN_GROUP, GUN_NONE );

			break;
		default:
			CSquadMonster::HandleAnimEvent( event );
			break;
	}
}

BOOL CMassn::FCanCheckAttacks( void )
{
	if ( !HasConditions( bits_COND_ENEMY_TOOFAR ) )
		return TRUE;

	return HasConditions( bits_COND_SEE_ENEMY ) && ( pev->weapons & MASSN_SNIPERRIFLE );
}

Vector CMassn::GetGunPosition( void )
{
	if ( m_Combat.m_fStanding || ( pev->weapons & MASSN_SNIPERRIFLE ) )
		return pev->origin + Vector( 0, 0, 64 );

	return pev->origin + Vector( 0, 0, 48 );
}

BOOL CMassn::CheckRangeAttack1( float flDot, float flDist )
{
	if ( pev->weapons == MASSN_WEAPON_NONE || pev->weapons == MASSN_WEAPON_LOST )
		return FALSE;

	CBaseEntity *enemy = m_hEnemy;
	TraceResult tr;

	if ( pev->weapons & MASSN_SNIPERRIFLE )
	{
		if ( flDist <= 64 )
			return FALSE;

		Vector source = GetGunPosition();

		UTIL_TraceLine( source, enemy->BodyTarget( source ), ignore_monsters, ignore_glass, edict(), &tr );
		return tr.flFraction == 1.0f || tr.pHit == enemy->edict();
	}

	if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) || flDist > 2048 || flDot < 0.5f || !NoFriendlyFire() )
		return FALSE;

	if ( !enemy->IsPlayer() && flDist <= 64 )
		return FALSE;

	Vector standing = pev->origin + Vector( 0, 0, 60 );
	Vector crouching = pev->origin + Vector( 0, 0, 48 );

	if ( RANDOM_LONG( 0, 9 ) == 0 && m_Activity != ACT_RANGE_ATTACK1 )
		m_Combat.m_fStanding = RANDOM_LONG( 0, 1 ) != 0;

	Vector first = m_Combat.m_fStanding ? standing : crouching;
	Vector second = m_Combat.m_fStanding ? crouching : standing;

	UTIL_TraceLine( first, enemy->BodyTarget( first ), ignore_monsters, ignore_glass, edict(), &tr );

	if ( tr.flFraction == 1.0f || tr.pHit == enemy->edict() )
		return TRUE;

	UTIL_TraceLine( second, enemy->BodyTarget( second ), ignore_monsters, ignore_glass, edict(), &tr );

	if ( tr.flFraction == 1.0f || tr.pHit == enemy->edict() )
	{
		m_Combat.m_fStanding = !m_Combat.m_fStanding;
		return TRUE;
	}

	return FALSE;
}

BOOL CMassn::CheckRangeAttack2( float flDot, float flDist )
{
	return HumanCheckRangeAttack2( this, m_Combat, flDot, flDist );
}

void CMassn::SetActivity( Activity activity )
{
	int sequence;

	switch ( activity )
	{
		case ACT_IDLE:
			if ( m_MonsterState == MONSTERSTATE_COMBAT )
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
			if ( pev->weapons & HGRUNT_9MMAR )
			{
				sequence = ACTIVITY_NOT_AVAILABLE;

				if ( pev->weapons & HGRUNT_GRENADELAUNCHER )
					sequence = LookupSequence( m_Combat.m_fStanding ? "standing_m16a2" : "crouching_m16a2" );

				if ( sequence == ACTIVITY_NOT_AVAILABLE )
					sequence = LookupSequence( m_Combat.m_fStanding ? "standing_mp5" : "crouching_mp5" );
			}
			else if ( pev->weapons & MASSN_WEAPON_NONE )
			{
				sequence = LookupSequence( "standing_armswing" );

				if ( sequence == ACTIVITY_NOT_AVAILABLE )
					sequence = LookupSequence( "standing_m40a1 " );
			}
			else
			{
				sequence = LookupSequence( "standing_m40a1" );
			}
			break;
		case ACT_RANGE_ATTACK2:
			sequence = LookupSequence( ( pev->weapons & HGRUNT_HANDGRENADE ) ? "throwgrenade" : "launchgrenade" );
			break;
		case ACT_RELOAD:
			if ( pev->weapons & HGRUNT_9MMAR )
			{
				sequence = LookupSequence( "reload_mp5" );
			}
			else
			{
				EMIT_SOUND( edict(), CHAN_WEAPON, "weapons/sniper_reload_first_seq.wav", 1, ATTN_NORM );
				sequence = LookupSequence( "reload_m40a1" );
			}
			break;
		case ACT_DIESIMPLE:
		case ACT_DIEBACKWARD:
		case ACT_DIEFORWARD:
		case ACT_DIEVIOLENT:
		case ACT_DIE_HEADSHOT:
		case ACT_DIE_CHESTSHOT:
		case ACT_DIE_GUTSHOT:
		case ACT_DIE_BACKSHOT:
			sequence = ACTIVITY_NOT_AVAILABLE;

			// shot while still on the rope
			if ( pev->movetype == MOVETYPE_FLY && pev->velocity.z < 0 && !( pev->flags & FL_ONGROUND ) && !pev->groundentity )
				sequence = LookupSequence( "repel_die" );

			if ( sequence <= ACTIVITY_NOT_AVAILABLE )
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
		ALERT( at_console, "%s has no sequence for act:%d\n", STRING( pev->classname ), activity );
		pev->sequence = 0;
		return;
	}

	if ( pev->sequence != sequence || !m_fSequenceLoops )
		pev->frame = 0;

	pev->sequence = sequence;
	ResetSequenceInfo();
	SetYawSpeed();
}

void CMassn::StartTask( Task_t *task )
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
		case TASK_RELOAD:
			m_IdealActivity = ACT_RELOAD;
			break;
		case TASK_MASSN_FACE_TOSS_DIR:
			break;
		case TASK_MASSN_CHECK_FIRE:
			TaskComplete();

			if ( !NoFriendlyFire() )
			{
				ChangeSchedule( GetScheduleOfType( SCHED_FIND_ATTACK_POINT ) );
				SetConditions( bits_COND_GRUNT_NOFIRE );
			}
			break;
		case TASK_MASSN_WAIT_SNIPER:
		{
			m_IdealActivity = ACT_IDLE_ANGRY;

			if ( !( pev->weapons & MASSN_SNIPERRIFLE ) || m_hEnemy == NULL )
			{
				m_flWaitFinished = gpGlobals->time + task->flData;
				break;
			}

			m_flWaitFinished = gpGlobals->time;

			float distance = ( m_hEnemy->Center() - Center() ).Length();

			if ( m_hEnemy->IsMoving() )
				m_flWaitFinished += 0.3f;

			if ( distance < 384 )
				m_flWaitFinished += 0.5f;
			else if ( distance < 1024 )
				m_flWaitFinished += 0.8f;
			else
				m_flWaitFinished += 1.5f;
			break;
		}
		default:
			CSquadMonster::StartTask( task );
			break;
	}
}

void CMassn::RunTask( Task_t *task )
{
	switch ( task->iTask )
	{
		case TASK_DIE:
			if ( pev->sequence == LookupSequence( "repel_die" ) && pev->waterlevel < 1 && pev->movetype == MOVETYPE_FLY
				 && pev->deadflag == DEAD_DYING )
			{
				bool landed = ( pev->flags & FL_ONGROUND ) || pev->velocity.z > 0 || pev->groundentity;

				if ( ( m_fSequenceFinished && pev->frame >= 255 ) || pev->framerate == 0 || landed )
				{
					pev->framerate = 0;

					if ( landed )
					{
						pev->movetype = MOVETYPE_STEP;
						m_Activity = ACT_RESET;
						m_IdealActivity = ACT_DIEBACKWARD;
						SetActivity( ACT_DIEBACKWARD );
					}
				}
			}

			CSquadMonster::RunTask( task );
			break;
		case TASK_MASSN_FACE_TOSS_DIR:
			MakeIdealYaw( pev->origin + m_Combat.m_vecTossVelocity * 64 );
			ChangeYaw( pev->yaw_speed );

			if ( FacingIdeal() )
				TaskComplete();
			break;
		case TASK_MASSN_WAIT_SNIPER:
			MakeIdealYaw( m_vecEnemyLKP );
			ChangeYaw( pev->yaw_speed );

			if ( gpGlobals->time >= m_flWaitFinished )
				TaskComplete();
			break;
		default:
			CSquadMonster::RunTask( task );
			break;
	}
}

Schedule_t *CMassn::GetSchedule( void )
{
	// flying? If PRONE, barnacle has me. IF not, it's assumed I am rapelling.
	if ( pev->movetype == MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
	{
		if ( ( pev->flags & FL_ONGROUND ) || pev->groundentity || pev->velocity.z > 0 )
		{
			// just landed
			pev->movetype = MOVETYPE_STEP;
			return GetScheduleOfType( SCHED_MASSN_REPEL_LAND );
		}

		if ( m_MonsterState != MONSTERSTATE_COMBAT || ( pev->weapons & MASSN_SNIPERRIFLE ) )
			return GetScheduleOfType( SCHED_MASSN_REPEL );

		return GetScheduleOfType( SCHED_MASSN_REPEL_ATTACK );
	}

	if ( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		CSound *sound = PBestSound();

		if ( sound && ( sound->m_iType & bits_SOUND_DANGER ) )
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
	}

	switch ( m_MonsterState )
	{
		case MONSTERSTATE_IDLE:
		case MONSTERSTATE_ALERT:
			if ( HasConditions( bits_COND_HEAR_SOUND ) && !( pev->spawnflags & SF_MONSTER_PRISONER ) )
			{
				CSound *sound = PBestSound();

				if ( sound && ( sound->m_iType & bits_SOUND_COMBAT ) && !( pev->weapons & MASSN_SNIPERRIFLE ) )
					return GetScheduleOfType( SCHED_INVESTIGATE_SOUND );
			}

			return CSquadMonster::GetSchedule();
		case MONSTERSTATE_COMBAT:
			break;
		default:
			return CSquadMonster::GetSchedule();
	}

	if ( HasConditions( bits_COND_ENEMY_DEAD ) )
		return CSquadMonster::GetSchedule();

	if ( HasConditions( bits_COND_NEW_ENEMY ) )
	{
		if ( InSquad() )
		{
			MySquadLeader()->m_fEnemyEluded = FALSE;

			if ( IsLeader() )
			{
				if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
					return GetScheduleOfType( SCHED_MASSN_SUPPRESS );

				return GetScheduleOfType( SCHED_MASSN_ESTABLISH_LINE_OF_FIRE );
			}

			if ( !m_Combat.m_fFirstEncounter )
				return GetScheduleOfType( SCHED_MASSN_ESTABLISH_LINE_OF_FIRE );

			m_Combat.m_fFirstEncounter = false;
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
		}
	}
	else if ( HasConditions( bits_COND_NO_AMMO_LOADED ) )
	{
		return GetScheduleOfType( SCHED_MASSN_COVER_AND_RELOAD );
	}
	else if ( HasConditions( bits_COND_LIGHT_DAMAGE ) )
	{
		if ( RANDOM_LONG( 0, 99 ) > 90 || m_hEnemy == NULL )
			return GetScheduleOfType( SCHED_SMALL_FLINCH );

		return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
	}
	else if ( HasConditions( bits_COND_SEE_FEAR ) )
	{
		CBaseEntity *enemy = m_hEnemy;

		if ( enemy && ( enemy->pev->flags & FL_MONSTER ) )
		{
			CBaseMonster *monster = enemy->MyMonsterPointer();

			if ( monster->HasConditions( bits_COND_SEE_ENEMY ) && monster->m_hEnemy == this )
				return GetScheduleOfType( SCHED_LOSE_ENEMY );
		}

		if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
			return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

		if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) )
			return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

		if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
			return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

		if ( HasConditions( bits_COND_CAN_MELEE_ATTACK2 ) )
			return GetScheduleOfType( SCHED_MELEE_ATTACK2 );

		return GetScheduleOfType( SCHED_CHASE_ENEMY );
	}
	else
	{
		if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
			return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

		if ( m_hEnemy != NULL && FClassnameIs( m_hEnemy->pev, "monster_gargantua" ) )
		{
			if ( pev->weapons & ( HGRUNT_HANDGRENADE | HGRUNT_GRENADELAUNCHER ) )
			{
				if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) )
					return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
			}
		}
		else
		{
			if ( ( pev->weapons & ( HGRUNT_HANDGRENADE | HGRUNT_GRENADELAUNCHER ) ) && HasConditions( bits_COND_CAN_RANGE_ATTACK2 )
				 && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
			{
				if ( InSquad() && MySquadLeader()->m_fEnemyEluded && !HasConditions( bits_COND_ENEMY_FACING_ME ) )
				{
					MySquadLeader()->m_fEnemyEluded = FALSE;
					return GetScheduleOfType( SCHED_MASSN_FOUND_ENEMY );
				}

				if ( !OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
				{
					if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
						return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

					if ( !( pev->weapons & MASSN_SNIPERRIFLE ) || !HasConditions( bits_COND_SEE_ENEMY ) )
						return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
				}

				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
			}

			if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) )
			{
				if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
					return GetScheduleOfType( SCHED_RANGE_ATTACK2 );

				if ( !OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) || ( ( pev->weapons & MASSN_SNIPERRIFLE ) && RANDOM_LONG( 0, 10 ) > 1 ) )
					return GetScheduleOfType( SCHED_STANDOFF );

				return GetScheduleOfType( SCHED_MASSN_ESTABLISH_LINE_OF_FIRE );
			}
		}
	}

	if ( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
		return GetScheduleOfType( SCHED_MASSN_ESTABLISH_LINE_OF_FIRE );

	return CSquadMonster::GetSchedule();
}

// Bare-handed, the assassin closes in for the kick
Schedule_t *CMassn::GetUnarmedSchedule( void )
{
	if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
		return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

	return slMassnChaseEnemy;
}

Schedule_t *CMassn::GetScheduleOfType( int type )
{
	switch ( type )
	{
		case SCHED_COMBAT_FACE:
			return slMassnCombatFace;
		case SCHED_VICTORY_DANCE:
			if ( InSquad() && !IsLeader() )
				return slMassnFail;

			return slMassnVictoryDance;
		case SCHED_TAKE_COVER_FROM_ENEMY:
			if ( RANDOM_LONG( 0, 1 ) && InSquad() )
			{
				if ( sk_grunt_handgrenades.value >= 1 && HasConditions( bits_COND_CAN_RANGE_ATTACK2 )
					 && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
					return slMassnTossGrenadeCover;

				return slMassnTakeCover;
			}

			if ( pev->weapons == MASSN_WEAPON_NONE )
				return slMassnChaseEnemy;

			if ( m_hEnemy == NULL || m_hEnemy->pev->movetype == MOVETYPE_FLY || HasConditions( bits_COND_ENEMY_OCCLUDED ) )
				return slMassnTakeCover;

			if ( ( m_hEnemy->Center() - pev->origin ).Length2D() >= 512 || SquadMemberInRange( pev->origin, 256 ) )
				return slMassnTakeCover;

			return slMassnGrenadeCover;
		case SCHED_TAKE_COVER_FROM_BEST_SOUND:
			return slMassnTakeCoverFromBestSound;
		case SCHED_RANGE_ATTACK1:
			if ( pev->weapons == MASSN_WEAPON_NONE )
				return GetUnarmedSchedule();

			if ( pev->weapons & MASSN_SNIPERRIFLE )
				return RANDOM_LONG( 0, 9 ) > 0 ? slMassnRangeAttack1C : slMassnRangeAttack1D;

			if ( RANDOM_LONG( 0, 9 ) == 0 )
				m_Combat.m_fStanding = RANDOM_LONG( 0, 1 ) != 0;

			return m_Combat.m_fStanding ? slMassnRangeAttack1A : slMassnRangeAttack1B;
		case SCHED_RANGE_ATTACK2:
			if ( pev->weapons == MASSN_WEAPON_NONE )
				return GetUnarmedSchedule();

			return slMassnRangeAttack2;
		case SCHED_RELOAD:
			CheckAmmo();

			if ( HasConditions( bits_COND_NO_AMMO_LOADED ) )
				return CSquadMonster::GetScheduleOfType( type );

			return GetScheduleOfType( SCHED_MASSN_ESTABLISH_LINE_OF_FIRE );
		case SCHED_FAIL:
			if ( m_hEnemy == NULL )
				return slMassnFail;

			if ( HasConditions( bits_COND_SEE_ENEMY ) && NoFriendlyFire() && pev->weapons != MASSN_WEAPON_NONE )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			if ( pev->weapons == MASSN_WEAPON_NONE )
				return GetUnarmedSchedule();

			return slMassnCombatFail;
		case SCHED_MASSN_SUPPRESS:
			if ( pev->weapons == MASSN_WEAPON_NONE )
				return GetUnarmedSchedule();

			if ( pev->weapons & MASSN_SNIPERRIFLE )
				return slMassnRangeAttack1C;

			if ( !m_hEnemy->IsPlayer() || !m_Combat.m_fFirstEncounter )
				return slMassnSuppress;

			m_Combat.m_fFirstEncounter = false;
			return slMassnSignalSuppress;
		case SCHED_MASSN_ESTABLISH_LINE_OF_FIRE:
			return slMassnEstablishLineOfFire;
		case SCHED_MASSN_COVER_AND_RELOAD:
			if ( pev->weapons & MASSN_SNIPERRIFLE )
				return slMassnHideReloadB;

			return slMassnHideReload;
		case SCHED_MASSN_SWEEP:
			return slMassnSweep;
		case SCHED_MASSN_FOUND_ENEMY:
			return slMassnFoundEnemy;
		case SCHED_MASSN_REPEL:
			if ( pev->velocity.z > -128 )
				pev->velocity.z -= 32;

			return slMassnRepel;
		case SCHED_MASSN_REPEL_ATTACK:
			if ( pev->weapons == MASSN_WEAPON_NONE )
				return slMassnRepelLand;

			if ( pev->velocity.z > -128 )
				pev->velocity.z -= 32;

			return slMassnRepelAttack;
		case SCHED_MASSN_REPEL_LAND:
			return slMassnRepelLand;
		case SCHED_MASSN_WAIT_FACE_ENEMY:
			return slMassnWaitInCover;
		case SCHED_MASSN_TAKECOVER_FAILED:
			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			if ( pev->weapons == MASSN_WEAPON_NONE && HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

			return GetScheduleOfType( SCHED_FAIL );
		case SCHED_MASSN_ELOF_FAIL:
			if ( !( pev->weapons & MASSN_SNIPERRIFLE ) )
			{
				if ( ( pev->weapons & HGRUNT_9MMAR ) && HasConditions( bits_COND_CAN_RANGE_ATTACK1 | bits_COND_SEE_ENEMY ) )
					return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

				return GetScheduleOfType( SCHED_FIND_ATTACK_POINT );
			}

			if ( HasConditions( bits_COND_SEE_ENEMY | bits_COND_CAN_RANGE_ATTACK1 ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );

			if ( RANDOM_LONG( 0, 10 ) > 5 )
				return GetScheduleOfType( SCHED_FIND_ATTACK_POINT );

			return slMassnCombatFailB;
		default:
			return CSquadMonster::GetScheduleOfType( type );
	}
}

void CMassn::DeathSound( void )
{
	static const char *sounds[] = { "hgrunt/gr_die1.wav", "hgrunt/gr_die2.wav", "hgrunt/gr_die3.wav" };

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, sounds[RANDOM_LONG( 0, 2 )], 0.9f, ATTN_IDLE, 0, 100 );
}

int CMassn::IRelationship( CBaseEntity *target, bool ignoreNoTarget )
{
	// unarmed, it sometimes backs off from what it would fight
	if ( pev->weapons == MASSN_WEAPON_NONE && RANDOM_LONG( 0, 10 ) > 8 && CBaseEntity::IRelationship( target, false ) > R_NO )
		return R_FR;

	if ( target && !target->m_fOverrideClass && Classify() == CLASS_HUMAN_MILITARY
		 && ( FClassnameIs( target->pev, "monster_alien_grunt" ) || FClassnameIs( target->pev, "monster_gargantua" )
			  || FClassnameIs( target->pev, "monster_bigmomma" ) ) )
		return R_NM;

	return CBaseEntity::IRelationship( target, ignoreNoTarget );
}

void CMassn::RunAI( void )
{
	bool fEnemy = m_hEnemy != NULL;

	if ( fEnemy && ( m_Activity == ACT_RANGE_ATTACK2 || m_Activity == ACT_MELEE_ATTACK1 || m_Activity == ACT_MELEE_ATTACK2 ) )
	{
		pev->framerate = pev->weapons == MASSN_WEAPON_NONE ? 1.4f : 1.25f;
	}
	else if ( fEnemy && m_Activity == ACT_RUN )
	{
		if ( pev->weapons == MASSN_WEAPON_NONE )
			pev->framerate = 1.2f;
	}
	else if ( m_Activity != ACT_RELOAD )
	{
		pev->framerate = m_Activity == ACT_IDLE_ANGRY ? 2 : 1;
	}

	CSquadMonster::RunAI();
}

void CMassn::PrescheduleThink( void )
{
	if ( pev->weapons == MASSN_WEAPON_LOST || pev->weapons == MASSN_WEAPON_NONE )
		SetBodygroup( GUN_GROUP, GUN_NONE );

	if ( !InSquad() || !m_hEnemy )
		return;

	if ( HasConditions( bits_COND_SEE_ENEMY ) )
		MySquadLeader()->m_flLastEnemySightTime = gpGlobals->time;
	else if ( gpGlobals->time - MySquadLeader()->m_flLastEnemySightTime > 5 )
		MySquadLeader()->m_fEnemyEluded = TRUE;
}

void CMassn::UseSound( void )
{
	if ( !IsPlayerAlly() )
		return;

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, RANDOM_LONG( 0, 1 ) ? "barney/heyfella.wav" : "barney/teamup2.wav", 0.8f, ATTN_NORM, 0, 85 );
}

void CMassn::UnUseSound( void )
{
	if ( !IsPlayerAlly() )
		return;

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, RANDOM_LONG( 0, 1 ) ? "barney/standguard.wav" : "barney/stop2.wav", 0.8f, ATTN_NORM, 0, 85 );
}

// The gun flies from the hand, spinning, and the assassin is left unarmed
CBaseEntity *CMassn::DropWeapon( void )
{
	if ( GetBodygroup( GUN_GROUP ) == GUN_NONE || ( pev->weapons & HGRUNT_NODROP ) )
		return NULL;

	CBaseEntity *weapon;
	CBaseEntity *ammo = NULL;

	if ( pev->weapons & HGRUNT_GRENADELAUNCHER )
	{
		weapon = CBaseMonster::DropWeapon( "weapon_m16", false );
		ammo = CBaseMonster::DropWeapon( "ammo_ARgrenades", false );
	}
	else if ( pev->weapons & HGRUNT_9MMAR )
	{
		weapon = CBaseMonster::DropWeapon( "weapon_9mmAR", false );
	}
	else if ( pev->weapons & MASSN_SNIPERRIFLE )
	{
		weapon = CBaseMonster::DropWeapon( "weapon_sniperrifle", true );
	}
	else
	{
		return NULL;
	}

	if ( !weapon )
		return NULL;

	Vector vecGun = g_vecZero;
	Vector vecGunAngles = g_vecZero;
	TraceResult tr;

	GetAttachment( 1, vecGun, vecGunAngles );
	UTIL_TraceLine( pev->origin, vecGun, dont_ignore_monsters, edict(), &tr );
	UTIL_SetOrigin( weapon->pev, tr.vecEndPos );

	Vector vecThrow = vecGun - m_HackedGunPos;
	float flSpeed = vecThrow.Length() * 10;

	if ( flSpeed > 300 )
		flSpeed = 300;

	weapon->pev->velocity = vecThrow.Normalize() * flSpeed;
	weapon->pev->angles = g_vecZero;
	weapon->pev->angles.y = pev->angles.y;

	float flMaxSpin;

	if ( pev->weapons & HGRUNT_9MMAR )
	{
		weapon->pev->angles.y = UTIL_AngleMod( pev->angles.y + 90 );
		weapon->pev->angles.x = UTIL_AngleMod( weapon->pev->angles.x - 90 );
		flMaxSpin = 400;
	}
	else
	{
		weapon->pev->angles.y = UTIL_AngleMod( pev->angles.y + 180 );
		weapon->pev->angles.z = UTIL_AngleMod( weapon->pev->angles.z - 90 );
		flMaxSpin = 200;
	}

	weapon->pev->avelocity.x = RANDOM_FLOAT( 100, flMaxSpin ) * ( RANDOM_LONG( 0, 1 ) ? -1 : 1 );
	weapon->pev->avelocity.y = RANDOM_FLOAT( 100, flMaxSpin ) * ( RANDOM_LONG( 0, 1 ) ? -1 : 1 );
	weapon->pev->avelocity.z = 0;

	if ( ammo )
	{
		ammo->pev->velocity = weapon->pev->velocity
			+ Vector( RANDOM_LONG( -10, 10 ), RANDOM_LONG( -10, 10 ), RANDOM_LONG( -10, 10 ) );
	}

	pev->weapons = MASSN_WEAPON_NONE;

	return weapon;
}

void CMassn::GibMonster( void )
{
	DropWeapon();
	CBaseMonster::GibMonster();
}

void CMassn::TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType )
{
	switch ( trace->iHitgroup )
	{
		case HITGROUP_HEAD:
		case HITGROUP_HELMET:
			trace->iHitgroup = HITGROUP_HEAD;
			break;
		case HITGROUP_CHEST:
		case HITGROUP_STOMACH:
			if ( damageType & ( DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_SNIPER ) )
			{
				if ( damage >= 2 )
					damage -= 2;

				damage *= 0.6f;
			}
			break;
	}

	CBaseMonster::TraceAttack( attacker, damage, direction, trace, damageType );
}

int CMassn::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	Forget( bits_MEMORY_INCOVER );

	if ( ( damageType & DMG_CLUB ) && IsFriendly( attacker ? CBaseEntity::Instance( attacker ) : NULL ) )
		damage = 0;

	return CBaseMonster::TakeDamage( inflictor, attacker, damage, damageType );
}

void CMassn::Killed( entvars_t *attacker, int gib )
{
	CSquadMonster::Killed( attacker, gib );
}
