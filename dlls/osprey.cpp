/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "CClassicMode.h"
#include "blkoposprey.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "soundent.h"
#include "effects.h"
#include "customentity.h"
#include "sound_message.h"

#include "osprey.h"

#define MASSN_SNIPERRIFLE		( 1 << 3 )
#define HGRUNT_HANDGRENADE		( 1 << 1 )
#define HGRUNT_RPG				( 1 << 6 )

#define SF_WAITFORTRIGGER 0x40

LINK_ENTITY_TO_CLASS( monster_osprey, COsprey );

TYPEDESCRIPTION COsprey::m_SaveData[] =
{
	DEFINE_FIELD( COsprey, m_hGoalEnt, FIELD_EHANDLE ),
	DEFINE_FIELD( COsprey, m_vel1, FIELD_VECTOR ),
	DEFINE_FIELD( COsprey, m_vel2, FIELD_VECTOR ),
	DEFINE_FIELD( COsprey, m_pos1, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( COsprey, m_pos2, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( COsprey, m_ang1, FIELD_VECTOR ),
	DEFINE_FIELD( COsprey, m_ang2, FIELD_VECTOR ),

	DEFINE_FIELD( COsprey, m_startTime, FIELD_TIME ),
	DEFINE_FIELD( COsprey, m_dTime, FIELD_FLOAT ),
	DEFINE_FIELD( COsprey, m_velocity, FIELD_VECTOR ),

	DEFINE_FIELD( COsprey, m_flIdealtilt, FIELD_FLOAT ),
	DEFINE_FIELD( COsprey, m_flRotortilt, FIELD_FLOAT ),

	DEFINE_FIELD( COsprey, m_flRightHealth, FIELD_FLOAT ),
	DEFINE_FIELD( COsprey, m_flLeftHealth, FIELD_FLOAT ),

	DEFINE_ARRAY( COsprey, m_vecOrigin, FIELD_POSITION_VECTOR, MAX_CARRY ),
	DEFINE_ARRAY( COsprey, m_hRepel, FIELD_EHANDLE, 4 ),

	// DEFINE_FIELD( COsprey, m_iSoundState, FIELD_INTEGER ),
	// DEFINE_FIELD( COsprey, m_iSpriteTexture, FIELD_INTEGER ),
	// DEFINE_FIELD( COsprey, m_iPitch, FIELD_INTEGER ),

	DEFINE_FIELD( COsprey, m_iDoLeftSmokePuff, FIELD_INTEGER ),
	DEFINE_FIELD( COsprey, m_iDoRightSmokePuff, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( COsprey, CBaseMonster );

extern cvar_t sk_osprey;

float COsprey::GetPointsForDamage( float flDamage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	return ( pev->max_health / sk_osprey.value ) * 40.0f * ( flDamage / pev->max_health );
}

void COsprey :: Spawn( void )
{
	m_fOverrideClass = false;
	Precache( );
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_SLIDEBOX;

	if ( !SetupModel() )
		UTIL_SetModel( this, CClassicMode::GetInstance()->IsEnabled() ? "models/hlclassic/osprey.mdl" : "models/osprey.mdl");
	UTIL_SetSize(pev, Vector( -400, -400, -100), Vector(400, 400, 32));
	UTIL_SetOrigin( pev, pev->origin );

	if ( pev->health == 0.0f )
		pev->health = sk_osprey.value;

	pev->max_health = pev->health;
	pev->flags |= FL_MONSTER;
	pev->takedamage		= DAMAGE_YES;
	m_flRightHealth		= pev->health * 0.5f;
	m_flLeftHealth		= pev->health * 0.5;
	m_flFieldOfView = 0; // 180 degrees
	pev->deadflag = DEAD_NO;

	pev->sequence = 0;
	ResetSequenceInfo( );
	pev->frame = RANDOM_LONG(0, 255 );

	InitBoneControllers();

	SetThink( &COsprey::FindAllThink );
	SetUse( &COsprey::CommandUse );

	if ( pev->spawnflags & SF_WAITFORTRIGGER)
		m_fWaitingForTrigger = true;

	pev->nextthink = gpGlobals->time + 1.0;
	m_pos2 = pev->origin;
	m_ang2 = pev->angles;
	m_vel2 = pev->velocity;

	ALERT( at_aiconsole, "Osprey spawned\n" );

	if ( FStringNull( m_FormattedName ) )
		m_FormattedName = MAKE_STRING( "Osprey Helicopter" );
}

void COsprey::Precache( void )
{
	CBaseEntity::Precache();
	UTIL_PrecacheMonster( "monster_human_medic_ally", !IsPlayerAlly() );

	if ( m_iGruntType > 1 )
	{
		UTIL_PrecacheMonster( "monster_hwgrunt", IsPlayerAlly() );
		UTIL_PrecacheMonster( "monster_male_assassin", IsPlayerAlly() );
		UTIL_PrecacheMonster( "monster_human_grunt", IsPlayerAlly() );
		UTIL_PrecacheMonster( "monster_human_grunt_ally", !IsPlayerAlly() );
	}

	PrecacheSound_Intercept( this, "apache/ap_rotor4.wav" );
	PrecacheSound_Intercept( this, "weapons/mortarhit.wav" );

	if ( pev->modelindex == 0 )
		UTIL_PrecacheModel( this, CClassicMode::GetInstance()->IsEnabled() ? "models/hlclassic/osprey.mdl" : "models/osprey.mdl");

	UTIL_PrecacheModel( this, "models/dead_osprey.mdl" );

	m_iSpriteTexture = UTIL_PrecacheModel( this, "sprites/rope.spr" );

	m_iExplode	= UTIL_PrecacheModel( this, "sprites/fexplo.spr" );
	m_iTailGibs = UTIL_PrecacheModel( this, "models/osprey_tailgibs.mdl" );
	m_iBodyGibs = UTIL_PrecacheModel( this, "models/osprey_enginegibs.mdl" );
}

void COsprey::CommandUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pActivator && ( FClassnameIs( pActivator->pev, "camera" ) || FClassnameIs( pActivator->pev, "trigger_camera" ) ) )
		return;

	m_fWaitingForTrigger = false;
	pev->nextthink = gpGlobals->time + 0.1;
}

void COsprey :: FindAllThink( void )
{
	if ( m_fWaitingForTrigger )
	{
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}
	SetThink( &COsprey::FlyThink );
	pev->nextthink = gpGlobals->time + 0.1;
	m_startTime = gpGlobals->time;
}

void COsprey :: DeployThink( void )
{
	UTIL_MakeAimVectors( pev->angles );

	Vector vecForward = gpGlobals->v_forward;
	Vector vecRight = gpGlobals->v_right;
	Vector vecUp = gpGlobals->v_up;

	Vector vecSrc;

	TraceResult tr;
	UTIL_TraceLine( pev->origin, pev->origin + Vector( 0, 0, -4096.0), ignore_monsters, ENT(pev), &tr);
	CSoundEnt::InsertSound ( bits_SOUND_DANGER, tr.vecEndPos, 400, 0.3, this );

	vecSrc = pev->origin + vecForward *  32 + vecRight *  100 + vecUp * -96;
	m_hRepel[0] = MakeGrunt( vecSrc );

	vecSrc = pev->origin + vecForward * -64 + vecRight *  100 + vecUp * -96;
	m_hRepel[1] = MakeGrunt( vecSrc );

	vecSrc = pev->origin + vecForward *  32 + vecRight * -100 + vecUp * -96;
	m_hRepel[2] = MakeGrunt( vecSrc );

	vecSrc = pev->origin + vecForward * -64 + vecRight * -100 + vecUp * -96;
	m_hRepel[3] = MakeGrunt( vecSrc );

	SetThink( &COsprey::HoverThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

void COsprey :: HoverThink( void )
{
	int i;
	for (i = 0; i < 4; ++i )
	{
		if (m_hRepel[i] != NULL && m_hRepel[i]->pev->health > 0 && m_hRepel[i]->pev->movetype == MOVETYPE_FLY )
			break;
	}

	if (i == 4)
	{
		m_startTime = gpGlobals->time;
		SetThink( &COsprey::FlyThink );
	}

	pev->nextthink = gpGlobals->time + 0.1;
	UTIL_MakeAimVectors( pev->angles );
	ShowDamage( );
}

void COsprey::UpdateGoal( )
{
	if ( m_hGoalEnt )
	{
		if ( !FStringNull( m_hGoalEnt->pev->message ) )
			FireTargets( STRING( m_hGoalEnt->pev->message ), this, this, USE_TOGGLE, 0 );

		if ( m_hGoalEnt == NULL )
			return;

		m_pos1 = m_pos2;
		m_ang1 = m_ang2;
		m_vel1 = m_vel2;
		m_pos2 = m_hGoalEnt->pev->origin;
		m_ang2 = m_hGoalEnt->pev->angles;
		UTIL_MakeAimVectors( Vector( 0, m_ang2.y, 0 ) );
		m_vel2 = gpGlobals->v_forward * m_hGoalEnt->pev->speed;

		m_startTime = m_startTime + m_dTime;
		m_dTime = 2.0 * (m_pos1 - m_pos2).Length() / (m_vel1.Length() + m_hGoalEnt->pev->speed);

		if (m_ang1.y - m_ang2.y < -180)
		{
			m_ang1.y += 360;
		}
		else if (m_ang1.y - m_ang2.y > 180)
		{
			m_ang1.y -= 360;
		}

		if ( m_hGoalEnt->pev->speed < 400)
			m_flIdealtilt = 0;
		else
			m_flIdealtilt = -90;
	}
	else
	{
		ALERT( at_console, "Osprey missing target!" );
	}
}

void COsprey::FlyThink( void )
{
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	if ( m_hGoalEnt == NULL )
	{
		if ( !FStringNull(pev->target) )// this monster has a target
	{
			m_hGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );
		UpdateGoal( );
	}
		else
		{
			do
			{
				m_hGoalEnt = UTIL_FindEntityByClassname( m_hGoalEnt, "path_corner" );
			} while ( m_hGoalEnt != NULL && m_hGoalEnt->pev->speed != 0 );
		}
	}

	if ( m_hGoalEnt == NULL )
		return;

	if (gpGlobals->time > m_startTime + m_dTime)
	{
		if ( m_hGoalEnt->pev->speed == 0)
			SetThink( &COsprey::DeployThink );

		do
		{
			m_hGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( m_hGoalEnt->pev->target ) );
		} while ( m_hGoalEnt != NULL && m_hGoalEnt->pev->speed < 400 && !HasDead() );

		UpdateGoal( );
	}

	Flight( );
	ShowDamage( );
}

void COsprey::Flight( )
{
	if ( m_fShockGlow && gpGlobals->time > m_flShockGlowTime )
		ShockGlowEffect( false );

	float t = (gpGlobals->time - m_startTime);
	float scale = 1.0 / m_dTime;

	float f = UTIL_SplineFraction( t * scale, 1.0 );

	Vector pos = (m_pos1 + m_vel1 * t) * (1.0 - f) + (m_pos2 - m_vel2 * (m_dTime - t)) * f;
	Vector ang = m_ang1 * (1.0 - f) + m_ang2 * f;
	m_velocity = m_vel1 * (1.0 - f) + m_vel2 * f;

	UTIL_SetOrigin( pev, pos );
	pev->angles = ang;
	UTIL_MakeAimVectors( pev->angles );
	float flSpeed = DotProduct( gpGlobals->v_forward, m_velocity );

	// float flSpeed = DotProduct( gpGlobals->v_forward, pev->velocity );

	float m_flIdealtilt = (160 - flSpeed) / 10.0;

	// ALERT( at_console, "%f %f\n", flSpeed, flIdealtilt );
	if (m_flRotortilt < m_flIdealtilt)
	{
		m_flRotortilt += 0.5;
		if (m_flRotortilt > 0)
			m_flRotortilt = 0;
	}
	if (m_flRotortilt > m_flIdealtilt)
	{
		m_flRotortilt -= 0.5;
		if (m_flRotortilt < -90)
			m_flRotortilt = -90;
	}
	SetBoneController( 0, m_flRotortilt );


	if (m_iSoundState == 0)
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_rotor4.wav", 1.0, 0.4, SOUNDMSG_FORCE_LOOP, 110 );
		// EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_whine1.wav", 0.5, 0.2, 0, 110 );

		m_iSoundState = SND_CHANGE_PITCH; // hack for going through level transitions
	}
	else
	{
		CBaseEntity *pPlayer = NULL;

		pPlayer = UTIL_FindEntityByClassname( NULL, "player" );
		// UNDONE: this needs to send different sounds to every player for multiplayer.
		if (pPlayer)
		{
			float pitch = DotProduct( m_velocity - pPlayer->pev->velocity, (pPlayer->pev->origin - pev->origin).Normalize() );

			pitch = (int)(100 + pitch / 75.0);

			if (pitch > 250)
				pitch = 250;
			if (pitch < 50)
				pitch = 50;

			if (pitch == 100)
				pitch = 101;

			if (pitch != m_iPitch)
			{
				m_iPitch = pitch;
				EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_rotor4.wav", 1.0, 0.3, SOUNDMSG_FORCE_LOOP | SND_CHANGE_PITCH | SND_CHANGE_VOL, pitch);
				// ALERT( at_console, "%.0f\n", pitch );
			}
		}
		// EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_whine1.wav", flVol, 0.2, SND_CHANGE_PITCH | SND_CHANGE_VOL, pitch);
	}

}

void COsprey::HitTouch( CBaseEntity *pOther )
{
	pev->nextthink = gpGlobals->time + 2.0;
}

void COsprey :: Killed( entvars_t *pevAttacker, int iGib )
{
	UTIL_SetModel( this, "models/dead_osprey.mdl" );

	CBaseEntity *owner = Instance( pev->owner );

	if ( owner )
		owner->DeathNotice( pev );

	if ( !FStringNull( m_iszTargetOnDeath ) )
		FireTargets( STRING( m_iszTargetOnDeath ), this, this, USE_TOGGLE, 0 );

	if ( !FStringNull( m_iszTriggerTarget ) )
	{
		FireTargets( STRING( m_iszTriggerTarget ), this, this, USE_TOGGLE, 0 );
		ALERT( at_console, "Osprey::Killed() trigger %s\n", STRING( m_iszTriggerTarget ) );
	}

	pev->movetype = MOVETYPE_BOUNCE;
	pev->friction = 0.1f;
	pev->gravity = 0.3;
	pev->velocity = m_velocity;
	pev->avelocity = Vector( RANDOM_FLOAT( -20, 20 ), 0, RANDOM_FLOAT( -50, 50 ) );
	STOP_SOUND( edict(), CHAN_STATIC, "apache/ap_rotor4.wav" );

	UTIL_SetSize( pev, Vector( -32, -32, -64), Vector( 32, 32, 0) );

	for ( int i = 0; i < 4; ++i )
	{
		if ( m_hRepel[i] != NULL && m_hRepel[i]->pev->health > 0 && !( m_hRepel[i]->pev->flags & FL_ONGROUND ) )
		{
			m_hRepel[i]->pev->flags |= FL_ONGROUND;
			m_hRepel[i]->pev->movetype = MOVETYPE_STEP;
		}
	}

	SetThink( &COsprey::DyingThink );
	SetTouch( &COsprey::CrashTouch );
	pev->nextthink = gpGlobals->time + 0.1;
	pev->health = 0;
	pev->takedamage = DAMAGE_NO;

	m_startTime = gpGlobals->time + 4.0;
}

void COsprey::CrashTouch( CBaseEntity *pOther )
{
	// only crash if we hit something solid
	if ( !( pOther->pev->flags & ( FL_CLIENT | FL_MONSTER ) ) && pOther->pev->solid != SOLID_BSP )
		return;

	SetTouch( NULL );
	SetSequenceByName( "crash" );
	pOther->TakeDamage( pev, pev, 300, DMG_SLASH );
	RadiusDamage( pev->origin, pev, pev, 100, CLASS_NONE, DMG_BLAST );
		m_startTime = gpGlobals->time + 2.0;
		pev->nextthink = gpGlobals->time;
		m_velocity = pev->velocity;
	pev->velocity.z *= 0.9f;
	pev->friction = 0.3f;
	pev->gravity = 1.0f;
	pev->avelocity.y = 100;

	if ( pev->angles.z >= 90 )
		pev->angles.z = 180;
	else
		pev->angles.z = 0;

	if ( pev->angles.x >= 90 )
		pev->angles.x = 180;
	else
		pev->angles.x = 0;

	pev->avelocity.x = 0;

	if ( pev->angles.z >= 180 )
		pev->avelocity.z = -20;
	else
		pev->avelocity.z = 20;
}

void COsprey :: DyingThink( void )
{
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	pev->avelocity = pev->avelocity * 1.02;
	CSoundEnt::InsertSound( bits_SOUND_DANGER, pev->origin + pev->velocity, 1024, 0.2, this );

	// still falling?
	if (m_startTime > gpGlobals->time )
	{
		UTIL_MakeAimVectors( pev->angles );
		ShowDamage( );

		Vector vecSpot = pev->origin + pev->velocity * 0.2;

		// random explosions
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
			WRITE_BYTE( TE_EXPLOSION);		// This just makes a dynamic light now
			WRITE_COORD( vecSpot.x + RANDOM_FLOAT( -150, 150 ));
			WRITE_COORD( vecSpot.y + RANDOM_FLOAT( -150, 150 ));
			WRITE_COORD( vecSpot.z + RANDOM_FLOAT( -150, -50 ));
			WRITE_SHORT( g_sModelIndexFireball );
			WRITE_BYTE( RANDOM_LONG(0,29) + 30  ); // scale * 10
			WRITE_BYTE( 12  ); // framerate
			WRITE_BYTE( TE_EXPLFLAG_NOSOUND );
		MESSAGE_END();

		const char *sound;

		switch ( RANDOM_LONG( 0, 2 ) )
		{
			case 0:
				sound = "weapons/explode3.wav";
				break;
			case 1:
				sound = "weapons/explode4.wav";
				break;
			default:
				sound = "weapons/explode5.wav";
				break;
		}

		UTIL_EmitAmbientSound( NULL, pev->origin, sound, 1.0, 0.3, 0, PITCH_NORM );
		RadiusDamage( pev->origin, pev, pev, 200, CLASS_NONE, DMG_BLAST );

		// lots of smoke
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( vecSpot.x + RANDOM_FLOAT( -150, 150 ));
			WRITE_COORD( vecSpot.y + RANDOM_FLOAT( -150, 150 ));
			WRITE_COORD( vecSpot.z + RANDOM_FLOAT( -150, -50 ));
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 100 ); // scale * 10
			WRITE_BYTE( 10  ); // framerate
		MESSAGE_END();


		vecSpot = pev->origin + (pev->mins + pev->maxs) * 0.5;
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
			WRITE_BYTE( TE_BREAKMODEL);

			// position
			WRITE_COORD( vecSpot.x );
			WRITE_COORD( vecSpot.y );
			WRITE_COORD( vecSpot.z );

			// size
			WRITE_COORD( 800 );
			WRITE_COORD( 800 );
			WRITE_COORD( 132 );

			// velocity
			WRITE_COORD( pev->velocity.x );
			WRITE_COORD( pev->velocity.y );
			WRITE_COORD( pev->velocity.z );

			// randomization
			WRITE_BYTE( 50 );

			// Model
			WRITE_SHORT( m_iTailGibs );	//model id#

			// # of shards
			WRITE_BYTE( 8 );	// let client decide

			// duration
			WRITE_BYTE( 200 );// 10.0 seconds

			// flags

			WRITE_BYTE( BREAK_METAL );
		MESSAGE_END();



		// don't stop it we touch a entity
		pev->flags &= ~FL_ONGROUND;
		pev->nextthink = gpGlobals->time + 0.2;
		return;
	}

	Vector vecSpot = pev->origin + (pev->mins + pev->maxs) * 0.5;

	UTIL_MakeAimVectors( pev->angles );

	Vector forward = gpGlobals->v_forward;
	Vector right = gpGlobals->v_right;
	TraceResult trace;

	UTIL_TraceLine( pev->origin, pev->origin - Vector( 0, 0, 4096 ), ignore_monsters, edict(), &trace );
	m_hRepel[0] = MakeGrunt( pev->origin + forward * 32 + right * 100 + Vector( 0, 0, 96 ) );
	m_hRepel[1] = MakeGrunt( pev->origin - forward * 64 + right * 100 + Vector( 0, 0, 96 ) );
	m_hRepel[2] = MakeGrunt( pev->origin + forward * 32 - right * 100 + Vector( 0, 0, 96 ) );
	m_hRepel[3] = MakeGrunt( pev->origin - forward * 64 - right * 100 + Vector( 0, 0, 96 ) );

	for ( int i = 0; i < 4; ++i )
	{
		if ( m_hRepel[i] == NULL )
			continue;

		CBaseMonster *grunt = m_hRepel[i]->MyMonsterPointer();

		if ( !grunt )
			continue;

		grunt->Killed( NULL, GIB_NEVER );

		int rightJitter = RANDOM_LONG( -10, 10 );
		int forwardJitter = RANDOM_LONG( -10, 10 );

		grunt->pev->velocity = Vector( 0, 0, 400 ) + right * rightJitter + forward * forwardJitter;
		grunt->pev->friction = 0.1f;
		grunt->pev->movetype = MOVETYPE_STEP;
		grunt->pev->flags &= ~FL_ONGROUND;
		grunt->pev->health = 200;
	}

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
			WRITE_BYTE( TE_SPRITE );
			WRITE_COORD( vecSpot.x );
			WRITE_COORD( vecSpot.y );
			WRITE_COORD( vecSpot.z + 512 );
			WRITE_SHORT( m_iExplode );
			WRITE_BYTE( 250 ); // scale * 10
			WRITE_BYTE( 255 ); // brightness
		MESSAGE_END();

		/*
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( vecSpot.x );
			WRITE_COORD( vecSpot.y );
			WRITE_COORD( vecSpot.z + 300 );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 250 ); // scale * 10
			WRITE_BYTE( 6  ); // framerate
		MESSAGE_END();
		*/

		// blast circle
		MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_BEAMCYLINDER );
			WRITE_COORD( pev->origin.x);
			WRITE_COORD( pev->origin.y);
			WRITE_COORD( pev->origin.z);
			WRITE_COORD( pev->origin.x);
			WRITE_COORD( pev->origin.y);
			WRITE_COORD( pev->origin.z + 2000 ); // reach damage radius over .2 seconds
			WRITE_SHORT( m_iSpriteTexture );
			WRITE_BYTE( 0 ); // startframe
			WRITE_BYTE( 0 ); // framerate
			WRITE_BYTE( 4 ); // life
			WRITE_BYTE( 32 );  // width
			WRITE_BYTE( 0 );   // noise
			WRITE_BYTE( 255 );   // r, g, b
			WRITE_BYTE( 255 );   // r, g, b
			WRITE_BYTE( 192 );   // r, g, b
			WRITE_BYTE( 128 ); // brightness
			WRITE_BYTE( 0 );		// speed
		MESSAGE_END();

		EMIT_SOUND( edict(), CHAN_STATIC, "weapons/mortarhit.wav", 1.0, 0.3);

		RadiusDamage( pev->origin, pev, pev, 300, CLASS_NONE, DMG_BLAST );

		// gibs
		vecSpot = pev->origin + (pev->mins + pev->maxs) * 0.5;
		MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, vecSpot );
			WRITE_BYTE( TE_BREAKMODEL);

			// position
			WRITE_COORD( vecSpot.x );
			WRITE_COORD( vecSpot.y );
			WRITE_COORD( vecSpot.z + 64);

			// size
			WRITE_COORD( 800 );
			WRITE_COORD( 800 );
			WRITE_COORD( 128 );

			// velocity
			WRITE_COORD( m_velocity.x );
			WRITE_COORD( m_velocity.y );
			WRITE_COORD( fabs( m_velocity.z ) * 0.25 );

			// randomization
			WRITE_BYTE( 40 );

			// Model
			WRITE_SHORT( m_iBodyGibs );	//model id#

			// # of shards
			WRITE_BYTE( 128 );

			// duration
			WRITE_BYTE( 200 );// 10.0 seconds

			// flags

			WRITE_BYTE( BREAK_METAL );
		MESSAGE_END();

		UTIL_Remove( this );
	}

void COsprey :: ShowDamage( void )
{
	if (m_iDoLeftSmokePuff > 0 || RANDOM_LONG(0,99) > m_flLeftHealth)
	{
		Vector vecSrc, vecAngles;

		GetAttachment( 8, vecSrc, vecAngles );
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( vecSrc.x );
			WRITE_COORD( vecSrc.y );
			WRITE_COORD( vecSrc.z );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( RANDOM_LONG(0,9) + 20 ); // scale * 10
			WRITE_BYTE( 12 ); // framerate
		MESSAGE_END();
		if (m_iDoLeftSmokePuff > 0)
			m_iDoLeftSmokePuff--;
	}
	if (m_iDoRightSmokePuff > 0 || RANDOM_LONG(0,99) > m_flRightHealth)
	{
		Vector vecSrc, vecAngles;

		GetAttachment( 4, vecSrc, vecAngles );
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( vecSrc.x );
			WRITE_COORD( vecSrc.y );
			WRITE_COORD( vecSrc.z );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( RANDOM_LONG(0,9) + 20 ); // scale * 10
			WRITE_BYTE( 12 ); // framerate
		MESSAGE_END();
		if (m_iDoRightSmokePuff > 0)
			m_iDoRightSmokePuff--;
	}
}

void COsprey::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	// ALERT( at_console, "%d %.0f\n", ptr->iHitgroup, flDamage );

	// only so much per engine
	if (ptr->iHitgroup == 2 )
	{
		if (m_flRightHealth < 0)
			return;

		m_flRightHealth -= flDamage;
		m_iDoRightSmokePuff = flDamage / 5.0 + 3.0;
	}

	if (ptr->iHitgroup == 3 )
	{
		if (m_flLeftHealth < 0)
			return;

		m_flLeftHealth -= flDamage;
		m_iDoLeftSmokePuff = flDamage / 5.0 + 3.0;
	}

	// hit hard, hits cockpit, hits engines
	if (flDamage > 50 || ptr->iHitgroup == 1 || ptr->iHitgroup == 2 || ptr->iHitgroup == 3)
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
	else
		UTIL_Sparks( ptr->vecEndPos );
	}

void COsprey::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszTargetOnDeath" ) )
		m_iszTargetOnDeath = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_fMonsterclipChildren" ) )
		m_fMonsterclipChildren = atoi( pkvd->szValue ) != 0;
	else if ( FStrEq( pkvd->szKeyName, "grunttype" ) )
		m_iGruntType = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "num" ) )
		m_iResupplyCount = atoi( pkvd->szValue );
	else
	{
		CBaseMonster::KeyValue( pkvd );
		return;
	}

	pkvd->fHandled = TRUE;
}

void COsprey::OnDestroy( void )
{
	CBaseMonster::OnDestroy();
	STOP_SOUND( edict(), CHAN_STATIC, "apache/ap_rotor4.wav" );
}

Vector COsprey::BodyTarget( const Vector &posSrc )
{
	Vector right, left, angles;

	GetAttachment( 4, right, angles );
	GetAttachment( 8, left, angles );

	return ( right - posSrc ).Length2D() <= ( left - posSrc ).Length2D() ? right : left;
}

void COsprey::UpdateOnRemove( void )
{
	STOP_SOUND( edict(), CHAN_STATIC, "apache/ap_rotor4.wav" );
}

EHANDLE COsprey::m_hPreGrunt[MAX_CARRY];
Vector COsprey::m_vecPreOrigin[MAX_CARRY];
int COsprey::m_iPreUnits;
EHANDLE COsprey::m_hPreGrunt_friendly[MAX_CARRY];
Vector COsprey::m_vecPreOrigin_friendly[MAX_CARRY];
int COsprey::m_iPreUnits_friendly;
EHANDLE COsprey::m_hPreGrunt_op4[MAX_CARRY];
Vector COsprey::m_vecPreOrigin_op4[MAX_CARRY];
int COsprey::m_iPreUnits_op4;
EHANDLE COsprey::m_hPreGrunt_op4friendly[MAX_CARRY];
Vector COsprey::m_vecPreOrigin_op4friendly[MAX_CARRY];
int COsprey::m_iPreUnits_op4friendly;
EHANDLE COsprey::m_hPreGrunt_RandomEvil[MAX_CARRY];
Vector COsprey::m_vecPreOrigin_RandomEvil[MAX_CARRY];
int COsprey::m_iPreUnits_RandomEvil;
EHANDLE COsprey::m_hPreGrunt_RandomFriendly[MAX_CARRY];
Vector COsprey::m_vecPreOrigin_RandomFriendly[MAX_CARRY];
int COsprey::m_iPreUnits_RandomFriendly;
bool COsprey::m_cancheck2 = true;

bool COsprey::SelectGruntPool( EHANDLE *&grunts, Vector *&origins, int *&count )
{
	bool friendly = IsPlayerAlly();

	if ( m_iGruntType == 0 )
	{
		grunts = friendly ? m_hPreGrunt_friendly : m_hPreGrunt;
		origins = friendly ? m_vecPreOrigin_friendly : m_vecPreOrigin;
		count = friendly ? &m_iPreUnits_friendly : &m_iPreUnits;
	}
	else if ( m_iGruntType == 1 )
	{
		grunts = friendly ? m_hPreGrunt_op4friendly : m_hPreGrunt_op4;
		origins = friendly ? m_vecPreOrigin_op4friendly : m_vecPreOrigin_op4;
		count = friendly ? &m_iPreUnits_op4friendly : &m_iPreUnits_op4;
	}
	else if ( m_iGruntType > 1 )
	{
		grunts = friendly ? m_hPreGrunt_RandomFriendly : m_hPreGrunt_RandomEvil;
		origins = friendly ? m_vecPreOrigin_RandomFriendly : m_vecPreOrigin_RandomEvil;
		count = friendly ? &m_iPreUnits_RandomFriendly : &m_iPreUnits_RandomEvil;
	}
	else
		return false;

	return true;
}

BOOL COsprey :: HasDead( void )
{
	EHANDLE *grunts;
	Vector *origins;
	int *count;

	if ( m_iResupplyCount > 0 )
		m_iResupplyCount = V_min( m_iResupplyCount, MAX_CARRY );

	if ( !SelectGruntPool( grunts, origins, count ) )
		return FALSE;

	if ( m_iResupplyCount > 0 )
		*count = m_iResupplyCount;

	for (int i = 0; i < *count; ++i )
	{
		if ( grunts[i] == NULL || !grunts[i]->IsAlive())
		{
			ALERT( at_aiconsole, "Grunt # %d is dead\n", i );
			return TRUE;
		}

		m_vecOrigin[i] = grunts[i]->pev->origin;  // send them to where they died
		}

	return FALSE;
}

CBaseMonster *COsprey :: MakeGrunt( Vector vecSrc )
{
	TraceResult tr;
	UTIL_TraceLine( vecSrc, vecSrc + Vector( 0, 0, -4096 ), dont_ignore_monsters, edict(), &tr);
	if ( tr.pHit && Instance( tr.pHit )->pev->solid != SOLID_BSP)
		return NULL;

	EHANDLE *grunts;
	Vector *origins;
	int *count;

	if ( !SelectGruntPool( grunts, origins, count ) )
		return NULL;

	for (int i = 0; i < *count; ++i )
	{
		if ( grunts[i] != NULL && grunts[i]->IsAlive())
			continue;

		if ( grunts[i] != NULL && grunts[i]->pev->rendermode == kRenderNormal)
			grunts[i]->SUB_StartFadeOut( );

		CBaseEntity *entity = NULL;

		if ( m_iGruntType == 0 )
		{
			entity = Create( "monster_human_grunt", vecSrc, pev->angles, true );

			if ( entity )
			{
				static const int weapons[] = { 1, 3, 5, 8, 10 };

				entity->pev->weapons = weapons[RANDOM_LONG( 1, 5 ) - 1];
			}
		}
		else if ( RANDOM_LONG( 0, 10 ) >= 8 )
		{
			entity = Create( "monster_human_medic_ally", vecSrc, pev->angles, true );

			if ( entity )
				entity->pev->weapons = RANDOM_LONG( 1, 2 );
		}
		else if ( m_iGruntType > 1 && RANDOM_LONG( 0, 10 ) <= 1 )
		{
			switch ( RANDOM_LONG( 0, 2 ) )
			{
				case 0:
					entity = Create( "monster_hwgrunt", vecSrc, pev->angles, true );
					break;
				case 1:
					entity = Create( "monster_male_assassin", vecSrc, pev->angles, true );

					if ( entity )
						entity->pev->weapons = MASSN_SNIPERRIFLE;
					break;
				case 2:
					entity = Create( "monster_human_grunt", vecSrc, pev->angles, true );

					if ( entity )
						entity->pev->weapons = HGRUNT_HANDGRENADE | HGRUNT_RPG;
					break;
			}
		}
		else
		{
			entity = Create( "monster_human_grunt_ally", vecSrc, pev->angles, true );

			if ( entity )
			{
				static const int weapons[] = { 1, 3, 5, 8, 10, 16 };

				entity->pev->weapons = weapons[RANDOM_LONG( 1, 6 ) - 1];
			}
		}

		if ( !entity )
			return NULL;

		if ( m_iGruntType > 1 || ( m_iGruntType == 0 && IsPlayerAlly() ) || ( m_iGruntType == 1 && !IsPlayerAlly() ) )
			entity->SetPlayerAllyDirect( IsPlayerAlly() );

		entity->m_fOverrideClass = m_fOverrideClass;
		entity->m_iClassSelection = m_iClassSelection;
		DispatchSpawn( entity->edict() );

		CBaseMonster *pGrunt = entity->MyMonsterPointer( );

		if ( !pGrunt )
			return NULL;

		if ( m_fMonsterclipChildren )
		{
			pGrunt->pev->spawnflags |= SF_MONSTER_HITMONSTERCLIP;
			pGrunt->pev->flags |= FL_MONSTERCLIP;
		}

		pGrunt->pev->movetype = MOVETYPE_FLY;
			pGrunt->pev->velocity = Vector( 0, 0, RANDOM_FLOAT( -196, -128 ) );
			pGrunt->SetActivity( ACT_GLIDE );

		if ( pev->movetype != MOVETYPE_BOUNCE )
		{
			CBeam *pBeam = CBeam::BeamCreate( "sprites/rope.spr", 10 );

			if ( pBeam )
			{
				pBeam->PointEntInit( vecSrc + Vector(0,0,112), pGrunt->entindex() );
			pBeam->SetFlags( BEAM_FSOLID );
			pBeam->SetColor( 255, 255, 255 );
			pBeam->SetThink( &CBeam::SUB_Remove );
			pBeam->pev->nextthink = gpGlobals->time -4096.0 * tr.flFraction / pGrunt->pev->velocity.z + 0.5;
			}
		}

		pGrunt->m_vecLastPosition = m_vecOrigin[i];
		pGrunt->pev->netname = MAKE_STRING( IsPlayerAlly() && m_iGruntType <= 1 ? "osprey1" : "osprey2" );
		grunts[i] = pGrunt;
			return pGrunt;
		}

	return NULL;
}

void COsprey::InitializePreData( void )
{
	memset( m_hPreGrunt, 0, sizeof( m_hPreGrunt ) );
	memset( m_hPreGrunt_friendly, 0, sizeof( m_hPreGrunt_friendly ) );
	memset( m_hPreGrunt_op4, 0, sizeof( m_hPreGrunt_op4 ) );
	memset( m_hPreGrunt_op4friendly, 0, sizeof( m_hPreGrunt_op4friendly ) );
	memset( m_hPreGrunt_RandomFriendly, 0, sizeof( m_hPreGrunt_RandomFriendly ) );
	memset( m_hPreGrunt_RandomEvil, 0, sizeof( m_hPreGrunt_RandomEvil ) );
}

void COsprey::ResetPrecache( void )
{
	m_iPreUnits = 0;
	m_iPreUnits_friendly = 0;
	m_iPreUnits_op4 = 0;
	m_iPreUnits_op4friendly = 0;
	m_iPreUnits_RandomFriendly = 0;
	m_iPreUnits_RandomEvil = 0;
	m_cancheck2 = true;
}

void COsprey::PreFindAll( void )
{
	if ( !m_cancheck2 )
		return;

	m_cancheck2 = false;
	m_iPreUnits = 0;
	m_iPreUnits_friendly = 0;
	m_iPreUnits_op4 = 0;
	m_iPreUnits_op4friendly = 0;

	CBaseEntity *entity = NULL;
	CBaseEntity *grunt = NULL;

	while ( ( entity = UTIL_FindEntityByClassname( entity, "monster_osprey" ) ) != NULL )
	{
		COsprey *osprey = (COsprey *) entity;
		EHANDLE *grunts;
		Vector *origins;
		int *count;

		if ( !osprey->SelectGruntPool( grunts, origins, count ) )
			continue;

		const char *classes[2] = { "monster_human_grunt", NULL };

		if ( osprey->m_iGruntType == 1 )
		{
			classes[0] = "monster_human_grunt_ally";
			classes[1] = "monster_human_medic_ally";
		}

		for ( int kind = 0; kind < 2 && classes[kind]; ++kind )
		{
			while ( *count < MAX_CARRY && ( grunt = UTIL_FindEntityByClassname( grunt, classes[kind] ) ) != NULL )
			{
				if ( !grunt->IsAlive() || grunt->IsPlayerAlly() != osprey->IsPlayerAlly() )
					continue;

				int index = osprey->m_iGruntType > 1 ? m_iPreUnits : *count;

				// Random pools use the ordinary pool count as their write index
				if ( index < MAX_CARRY )
				{
					grunts[index] = grunt;
					origins[index] = grunt->pev->origin;
				}

				++*count;
			}
		}

		if ( *count == 0 )
		{
			ALERT( at_console, "Osprey Grunt precache error: No Grunts to resupply!\n" );
			return;
		}

		ALERT( at_console, "Osprey found: %d grunts to resupply\n", osprey->m_iGruntType > 1 ? *count : m_iPreUnits );
	}
}

void GlobalOspreyPreFindAll( void )
{
	COsprey::ResetPrecache();
	COsprey::PreFindAll();
}

void GlobalBlackOpsOspreyPreFindAll( void )
{
	COspreyBlackOps::ResetPrecache();
	COspreyBlackOps::PreFindAll();
}

void GlobalOspreyInitialize( void )
{
	GlobalOspreyPreFindAll();
	GlobalBlackOpsOspreyPreFindAll();
}
