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
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "soundent.h"
#include "effects.h"
#include "customentity.h"

#include "blkoposprey.h"

#define MASSN_9MMAR				( 1 << 0 )
#define MASSN_HANDGRENADE		( 1 << 1 )
#define MASSN_GRENADELAUNCHER	( 1 << 2 )
#define MASSN_SNIPERRIFLE		( 1 << 3 )

#define SF_WAITFORTRIGGER 0x40

LINK_ENTITY_TO_CLASS( monster_blkop_osprey, COspreyBlackOps );

TYPEDESCRIPTION COspreyBlackOps::m_SaveData[] =
{
	DEFINE_FIELD( COspreyBlackOps, m_hGoalEnt, FIELD_EHANDLE ),
	DEFINE_FIELD( COspreyBlackOps, m_vel1, FIELD_VECTOR ),
	DEFINE_FIELD( COspreyBlackOps, m_vel2, FIELD_VECTOR ),
	DEFINE_FIELD( COspreyBlackOps, m_pos1, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( COspreyBlackOps, m_pos2, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( COspreyBlackOps, m_ang1, FIELD_VECTOR ),
	DEFINE_FIELD( COspreyBlackOps, m_ang2, FIELD_VECTOR ),

	DEFINE_FIELD( COspreyBlackOps, m_startTime, FIELD_TIME ),
	DEFINE_FIELD( COspreyBlackOps, m_dTime, FIELD_FLOAT ),
	DEFINE_FIELD( COspreyBlackOps, m_velocity, FIELD_VECTOR ),

	DEFINE_FIELD( COspreyBlackOps, m_flIdealtilt, FIELD_FLOAT ),
	DEFINE_FIELD( COspreyBlackOps, m_flRotortilt, FIELD_FLOAT ),

	DEFINE_FIELD( COspreyBlackOps, m_flRightHealth, FIELD_FLOAT ),
	DEFINE_FIELD( COspreyBlackOps, m_flLeftHealth, FIELD_FLOAT ),

	DEFINE_ARRAY( COspreyBlackOps, m_vecOrigin, FIELD_POSITION_VECTOR, MAX_CARRY ),
	DEFINE_ARRAY( COspreyBlackOps, m_hRepel, FIELD_EHANDLE, 4 ),

	// DEFINE_FIELD( COspreyBlackOps, m_iSoundState, FIELD_INTEGER ),
	// DEFINE_FIELD( COspreyBlackOps, m_iSpriteTexture, FIELD_INTEGER ),
	// DEFINE_FIELD( COspreyBlackOps, m_iPitch, FIELD_INTEGER ),

	DEFINE_FIELD( COspreyBlackOps, m_iDoLeftSmokePuff, FIELD_INTEGER ),
	DEFINE_FIELD( COspreyBlackOps, m_iDoRightSmokePuff, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( COspreyBlackOps, CBaseMonster );

extern cvar_t sk_blkopsosprey;

float COspreyBlackOps::GetPointsForDamage( float flDamage )
{
	//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
	return ( pev->max_health / sk_blkopsosprey.value ) * 40.0f * ( flDamage / pev->max_health );
}

void COspreyBlackOps::Spawn( void )
{
	m_fOverrideClass = false;
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	if ( !SetupModel() )
		UTIL_SetModel( this, "models/blkop_osprey.mdl" );

	UTIL_SetSize( pev, Vector( -400, -400, -100 ), Vector( 400, 400, 32 ) );
	UTIL_SetOrigin( pev, pev->origin );

	if ( pev->health == 0.0f )
		pev->health = 600.0f;

	pev->max_health = pev->health;
	pev->flags |= FL_MONSTER;
	pev->takedamage = DAMAGE_YES;
	m_flRightHealth = pev->health * 0.5f;
	m_flLeftHealth = pev->health * 0.5;
	m_flFieldOfView = 0;
	pev->sequence = 0;
	ResetSequenceInfo();
	pev->frame = RANDOM_LONG( 0, 255 );
	InitBoneControllers();

	SetThink( &COspreyBlackOps::FindAllThink );
	SetUse( &COspreyBlackOps::CommandUse );

	if ( pev->spawnflags & SF_WAITFORTRIGGER )
		m_fWaitingForTrigger = true;

	pev->nextthink = gpGlobals->time + 1.0;
	m_pos2 = pev->origin;
	m_ang2 = pev->angles;
	m_vel2 = pev->velocity;

	ALERT( at_aiconsole, "Blackops Osprey spawned\n" );

	if ( FStringNull( m_FormattedName ) )
		m_FormattedName = MAKE_STRING( "Black Ops Osprey" );
}

void COspreyBlackOps::Precache( void )
{
	CBaseEntity::Precache();
	UTIL_PrecacheOther( "monster_male_assassin" );
	PrecacheSound_Intercept( this, "apache/ap_rotor4.wav" );
	PrecacheSound_Intercept( this, "weapons/mortarhit.wav" );

	if ( pev->modelindex == 0 )
		UTIL_PrecacheModel( this, "models/blkop_osprey.mdl" );

	UTIL_PrecacheModel( this, "models/blkop_dead_osprey.mdl" );
	m_iSpriteTexture = UTIL_PrecacheModel( this, "sprites/rope.spr" );
	m_iExplode = UTIL_PrecacheModel( this, "sprites/fexplo.spr" );
	m_iTailGibs = UTIL_PrecacheModel( this, "models/blkop_tailgibs.mdl" );
	m_iBodyGibs = UTIL_PrecacheModel( this, "models/blkop_enginegibs.mdl" );
}

void COspreyBlackOps::CommandUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pActivator && ( FClassnameIs( pActivator->pev, "camera" ) || FClassnameIs( pActivator->pev, "trigger_camera" ) ) )
		return;

	m_fWaitingForTrigger = false;
	pev->nextthink = gpGlobals->time + 0.1;
}

void COspreyBlackOps::FindAllThink( void )
{
	if ( m_fWaitingForTrigger )
	{
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	SetThink( &COspreyBlackOps::FlyThink );
	pev->nextthink = gpGlobals->time + 0.1;
	m_startTime = gpGlobals->time;
}

void COspreyBlackOps::DeployThink( void )
{
	UTIL_MakeAimVectors( pev->angles );

	Vector vecForward = gpGlobals->v_forward;
	Vector vecRight = gpGlobals->v_right;
	Vector vecUp = gpGlobals->v_up;

	Vector vecSrc;

	TraceResult tr;
	UTIL_TraceLine( pev->origin, pev->origin + Vector( 0, 0, -4096.0 ), ignore_monsters, ENT( pev ), &tr );
	CSoundEnt::InsertSound( bits_SOUND_DANGER, tr.vecEndPos, 400, 0.3, this );

	vecSrc = pev->origin + vecForward * 32 + vecRight * 100 + vecUp * -96;
	m_hRepel[0] = MakeGrunt( vecSrc );

	vecSrc = pev->origin + vecForward * -64 + vecRight * 100 + vecUp * -96;
	m_hRepel[1] = MakeGrunt( vecSrc );

	vecSrc = pev->origin + vecForward * 32 + vecRight * -100 + vecUp * -96;
	m_hRepel[2] = MakeGrunt( vecSrc );

	vecSrc = pev->origin + vecForward * -64 + vecRight * -100 + vecUp * -96;
	m_hRepel[3] = MakeGrunt( vecSrc );

	SetThink( &COspreyBlackOps::HoverThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

BOOL COspreyBlackOps::HasDead( void )
{
	EHANDLE *grunts = IsPlayerAlly() ? m_hPreGrunt_friendly : m_hPreGrunt;
	int count = IsPlayerAlly() ? m_iPreUnits_friendly : m_iPreUnits;

	for ( int i = 0; i < count; i++ )
	{
		if ( grunts[i] == NULL || !grunts[i]->IsAlive() )
			return TRUE;

		m_vecOrigin[i] = grunts[i]->pev->origin;
	}

	return FALSE;
}

CBaseMonster *COspreyBlackOps::MakeGrunt( Vector vecSrc )
{
	TraceResult tr;

	UTIL_TraceLine( vecSrc, vecSrc + Vector( 0, 0, -4096 ), dont_ignore_monsters, edict(), &tr );

	if ( tr.pHit && Instance( tr.pHit )->pev->solid != SOLID_BSP )
		return NULL;

	EHANDLE *grunts = IsPlayerAlly() ? m_hPreGrunt_friendly : m_hPreGrunt;
	int count = IsPlayerAlly() ? m_iPreUnits_friendly : m_iPreUnits;

	for ( int i = 0; i < count; i++ )
	{
		if ( grunts[i] != NULL && grunts[i]->IsAlive() )
			continue;

		if ( grunts[i] != NULL && grunts[i]->pev->rendermode == kRenderNormal )
			grunts[i]->SUB_StartFadeOut();

		CBaseEntity *entity = Create( "monster_male_assassin", vecSrc, pev->angles, true );

		if ( !entity )
			return NULL;

		if ( RANDOM_LONG( 0, 50 ) > 40 )
			entity->pev->weapons = MASSN_SNIPERRIFLE | MASSN_HANDGRENADE;
		else
		{
			switch ( RANDOM_LONG( 1, 3 ) )
			{
				case 1:
					entity->pev->weapons = MASSN_9MMAR;
					break;
				case 2:
					entity->pev->weapons = MASSN_9MMAR | MASSN_HANDGRENADE;
					break;
				case 3:
					entity->pev->weapons = MASSN_9MMAR | MASSN_GRENADELAUNCHER;
					break;
			}
		}

		entity->SetPlayerAllyDirect( IsPlayerAlly() );
		entity->m_fOverrideClass = m_fOverrideClass;
		entity->m_iClassSelection = m_iClassSelection;
		DispatchSpawn( entity->edict() );

		CBaseMonster *grunt = entity->MyMonsterPointer();

		if ( !grunt )
			return NULL;

		grunt->SetBodygroup( 1, RANDOM_LONG( 0, 2 ) );

		if ( m_fMonsterclipChildren )
		{
			grunt->pev->spawnflags |= SF_MONSTER_HITMONSTERCLIP;
			grunt->pev->flags |= FL_MONSTERCLIP;
		}

		grunt->pev->movetype = MOVETYPE_FLY;
		grunt->pev->velocity = Vector( 0, 0, RANDOM_FLOAT( -196, -128 ) );
		grunt->SetActivity( ACT_GLIDE );

		CBeam *beam = CBeam::BeamCreate( "sprites/rope.spr", 10 );

		if ( beam )
		{
			beam->PointEntInit( vecSrc + Vector( 0, 0, 112 ), grunt->entindex() );
			beam->SetFlags( BEAM_FSOLID );
			beam->SetColor( 255, 255, 255 );
			beam->SetThink( &CBeam::SUB_Remove );
			beam->pev->nextthink = gpGlobals->time + -4096.0 * tr.flFraction / grunt->pev->velocity.z + 0.5;
		}

		grunt->m_vecLastPosition = m_vecOrigin[i];
		grunts[i] = grunt;
		return grunt;
	}

	return NULL;
}

void COspreyBlackOps::HoverThink( void )
{
	int i;

	for ( i = 0; i < 4; i++ )
	{
		if ( m_hRepel[i] != NULL && m_hRepel[i]->pev->health > 0 && m_hRepel[i]->pev->movetype == MOVETYPE_FLY )
			break;
	}

	if ( i == 4 )
	{
		m_startTime = gpGlobals->time;
		SetThink( &COspreyBlackOps::FlyThink );
	}

	pev->nextthink = gpGlobals->time + 0.1;
	UTIL_MakeAimVectors( pev->angles );
	ShowDamage();
}

void COspreyBlackOps::UpdateGoal()
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
		m_dTime = 2.0 * ( m_pos1 - m_pos2 ).Length() / ( m_vel1.Length() + m_hGoalEnt->pev->speed );

		if ( m_ang1.y - m_ang2.y < -180 )
		{
			m_ang1.y += 360;
		}
		else if ( m_ang1.y - m_ang2.y > 180 )
		{
			m_ang1.y -= 360;
		}

		if ( m_hGoalEnt->pev->speed < 400 )
			m_flIdealtilt = 0;
		else
			m_flIdealtilt = -90;
	}
	else
	{
		ALERT( at_console, "Osprey missing target" );
	}
}

void COspreyBlackOps::FlyThink( void )
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if ( m_hGoalEnt == NULL )
	{
		if ( !FStringNull( pev->target ) )
		{
			m_hGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );
			UpdateGoal();
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

	if ( gpGlobals->time > m_startTime + m_dTime )
	{
		if ( m_hGoalEnt->pev->speed == 0 )
			SetThink( &COspreyBlackOps::DeployThink );

		do
		{
			m_hGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( m_hGoalEnt->pev->target ) );
		} while ( m_hGoalEnt != NULL && m_hGoalEnt->pev->speed < 400 && !HasDead() );

		UpdateGoal();
	}

	Flight();
	ShowDamage();
}

void COspreyBlackOps::Flight()
{
	if ( m_fShockGlow && gpGlobals->time > m_flShockGlowTime )
		ShockGlowEffect( false );

	float t = ( gpGlobals->time - m_startTime );
	float scale = 1.0 / m_dTime;

	float f = UTIL_SplineFraction( t * scale, 1.0 );

	Vector pos = ( m_pos1 + m_vel1 * t ) * ( 1.0 - f ) + ( m_pos2 - m_vel2 * ( m_dTime - t ) ) * f;
	Vector ang = ( m_ang1 ) * ( 1.0 - f ) + (m_ang2) *f;
	m_velocity = m_vel1 * ( 1.0 - f ) + m_vel2 * f;

	UTIL_SetOrigin( pev, pos );
	pev->angles = ang;
	UTIL_MakeAimVectors( pev->angles );
	float flSpeed = DotProduct( gpGlobals->v_forward, m_velocity );

	// float flSpeed = DotProduct( gpGlobals->v_forward, pev->velocity );

	float m_flIdealtilt = ( 160 - flSpeed ) / 10.0;

	// ALERT( at_console, "%f %f\n", flSpeed, flIdealtilt );
	if ( m_flRotortilt < m_flIdealtilt )
	{
		m_flRotortilt += 0.5;
		if ( m_flRotortilt > 0 )
			m_flRotortilt = 0;
	}
	if ( m_flRotortilt > m_flIdealtilt )
	{
		m_flRotortilt -= 0.5;
		if ( m_flRotortilt < -90 )
			m_flRotortilt = -90;
	}
	SetBoneController( 0, m_flRotortilt );

	if ( m_iSoundState == 0 )
	{
		EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, "apache/ap_rotor4.wav", 1.0, 0.4, 0, 110 );
		// EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_whine1.wav", 0.5, 0.2, 0, 110 );

		m_iSoundState = SND_CHANGE_PITCH; // hack for going through level transitions
	}
	else
	{
		CBaseEntity *pPlayer = NULL;

		pPlayer = UTIL_FindEntityByClassname( NULL, "player" );
		// UNDONE: this needs to send different sounds to every player for multiplayer.
		if ( pPlayer )
		{
			float pitch = DotProduct( m_velocity - pPlayer->pev->velocity, ( pPlayer->pev->origin - pev->origin ).Normalize() );

			pitch = (int) ( 100 + pitch / 75.0 );

			if ( pitch > 250 )
				pitch = 250;
			if ( pitch < 50 )
				pitch = 50;

			if ( pitch == 100 )
				pitch = 101;

			if ( pitch != m_iPitch )
			{
				m_iPitch = pitch;
				EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, "apache/ap_rotor4.wav", 1.0, 0.3, SND_CHANGE_PITCH | SND_CHANGE_VOL, pitch );
				// ALERT( at_console, "%.0f\n", pitch );
			}
		}
		// EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_whine1.wav", flVol, 0.2, SND_CHANGE_PITCH | SND_CHANGE_VOL, pitch);
	}
}

void COspreyBlackOps::HitTouch( CBaseEntity *pOther )
{
	pev->nextthink = gpGlobals->time + 2.0;
}

/*
int COspreyBlackOps::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if (m_flRotortilt <= -90)
	{
		m_flRotortilt = 0;
	}
	else
	{
		m_flRotortilt -= 45;
	}
	SetBoneController( 0, m_flRotortilt );
	return 0;
}
*/

void COspreyBlackOps::Killed( entvars_t *pevAttacker, int iGib )
{
	UTIL_SetModel( this, "models/blkop_dead_osprey.mdl" );

	CBaseEntity *owner = Instance( pev->owner );

	if ( owner )
		owner->DeathNotice( pev );

	if ( !FStringNull( m_iszTargetOnDeath ) )
		FireTargets( STRING( m_iszTargetOnDeath ), this, this, USE_TOGGLE, 0 );

	if ( !FStringNull( m_iszTriggerTarget ) )
		FireTargets( STRING( m_iszTriggerTarget ), this, this, USE_TOGGLE, 0 );

	pev->movetype = MOVETYPE_BOUNCE;
	pev->friction = 0.1f;
	pev->gravity = 0.3f;
	pev->velocity = m_velocity;
	pev->avelocity = Vector( RANDOM_FLOAT( -20, 20 ), 0, RANDOM_FLOAT( -50, 50 ) );
	STOP_SOUND( edict(), CHAN_STATIC, "apache/ap_rotor4.wav" );
	UTIL_SetSize( pev, Vector( -32, -32, -64 ), Vector( 32, 32, 0 ) );

	for ( int i = 0; i < 4; i++ )
	{
		if ( m_hRepel[i] != NULL && m_hRepel[i]->pev->health > 0 && !( m_hRepel[i]->pev->flags & FL_ONGROUND ) )
		{
			m_hRepel[i]->pev->flags |= FL_ONGROUND;
			m_hRepel[i]->pev->movetype = MOVETYPE_STEP;
		}
	}

	SetThink( &COspreyBlackOps::DyingThink );
	SetTouch( &COspreyBlackOps::CrashTouch );
	pev->nextthink = gpGlobals->time + 0.1;
	pev->health = 0;
	pev->takedamage = DAMAGE_NO;
	m_startTime = gpGlobals->time + 4.0;
}

void COspreyBlackOps::CrashTouch( CBaseEntity *pOther )
{
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
	pev->angles.z = pev->angles.z >= 90 ? 180 : 0;
	pev->angles.x = pev->angles.x >= 90 ? 180 : 0;
	pev->avelocity.x = 0;
	pev->avelocity.z = pev->angles.z >= 180 ? -20 : 20;
}

void COspreyBlackOps::DyingThink( void )
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;
	pev->avelocity = pev->avelocity * 1.02;
	CSoundEnt::InsertSound( bits_SOUND_DANGER, pev->origin + pev->velocity, 1024, 0.2, this );

	if ( m_startTime > gpGlobals->time )
	{
		UTIL_MakeAimVectors( pev->angles );
		ShowDamage();

		Vector spot = pev->origin + pev->velocity * 0.2;

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, spot );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( spot.x + RANDOM_FLOAT( -150, 150 ) );
		WRITE_COORD( spot.y + RANDOM_FLOAT( -150, 150 ) );
		WRITE_COORD( spot.z + RANDOM_FLOAT( -150, -50 ) );
		WRITE_SHORT( g_sModelIndexFireball );
		WRITE_BYTE( RANDOM_LONG( 0, 29 ) + 30 );
		WRITE_BYTE( 12 );
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

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, spot );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( spot.x + RANDOM_FLOAT( -150, 150 ) );
		WRITE_COORD( spot.y + RANDOM_FLOAT( -150, 150 ) );
		WRITE_COORD( spot.z + RANDOM_FLOAT( -150, -50 ) );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( 100 );
		WRITE_BYTE( 10 );
		MESSAGE_END();

		spot = pev->origin + ( pev->mins + pev->maxs ) * 0.5;

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, spot );
		WRITE_BYTE( TE_BREAKMODEL );
		WRITE_COORD( spot.x );
		WRITE_COORD( spot.y );
		WRITE_COORD( spot.z );
		WRITE_COORD( 800 );
		WRITE_COORD( 800 );
		WRITE_COORD( 132 );
		WRITE_COORD( pev->velocity.x );
		WRITE_COORD( pev->velocity.y );
		WRITE_COORD( pev->velocity.z );
		WRITE_BYTE( 50 );
		WRITE_SHORT( m_iTailGibs );
		WRITE_BYTE( 8 );
		WRITE_BYTE( 200 );
		WRITE_BYTE( BREAK_METAL );
		MESSAGE_END();

		pev->flags &= ~FL_ONGROUND;
		pev->nextthink = gpGlobals->time + 0.2;
		return;
	}

	Vector spot = pev->origin + ( pev->mins + pev->maxs ) * 0.5;

	UTIL_MakeAimVectors( pev->angles );

	Vector forward = gpGlobals->v_forward;
	Vector right = gpGlobals->v_right;
	TraceResult trace;

	UTIL_TraceLine( pev->origin, pev->origin - Vector( 0, 0, 4096 ), ignore_monsters, edict(), &trace );
	m_hRepel[0] = MakeGrunt( pev->origin + forward * 32 + right * 100 + Vector( 0, 0, 96 ) );
	m_hRepel[1] = MakeGrunt( pev->origin - forward * 64 + right * 100 + Vector( 0, 0, 96 ) );
	m_hRepel[2] = MakeGrunt( pev->origin + forward * 32 - right * 100 + Vector( 0, 0, 96 ) );
	m_hRepel[3] = MakeGrunt( pev->origin - forward * 64 - right * 100 + Vector( 0, 0, 96 ) );

	for ( int i = 0; i < 4; i++ )
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

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, spot );
	WRITE_BYTE( TE_SPRITE );
	WRITE_COORD( spot.x );
	WRITE_COORD( spot.y );
	WRITE_COORD( spot.z + 512 );
	WRITE_SHORT( m_iExplode );
	WRITE_BYTE( 250 );
	WRITE_BYTE( 255 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
	WRITE_BYTE( TE_BEAMCYLINDER );
	WRITE_COORD( pev->origin.x );
	WRITE_COORD( pev->origin.y );
	WRITE_COORD( pev->origin.z );
	WRITE_COORD( pev->origin.x );
	WRITE_COORD( pev->origin.y );
	WRITE_COORD( pev->origin.z + 2000 );
	WRITE_SHORT( m_iSpriteTexture );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 4 );
	WRITE_BYTE( 32 );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 255 );
	WRITE_BYTE( 255 );
	WRITE_BYTE( 192 );
	WRITE_BYTE( 128 );
	WRITE_BYTE( 0 );
	MESSAGE_END();

	EMIT_SOUND( edict(), CHAN_STATIC, "weapons/mortarhit.wav", 1.0, 0.3 );
	RadiusDamage( pev->origin, pev, pev, 300, CLASS_NONE, DMG_BLAST );
	spot = pev->origin + ( pev->mins + pev->maxs ) * 0.5;

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, spot );
	WRITE_BYTE( TE_BREAKMODEL );
	WRITE_COORD( spot.x );
	WRITE_COORD( spot.y );
	WRITE_COORD( spot.z + 64 );
	WRITE_COORD( 800 );
	WRITE_COORD( 800 );
	WRITE_COORD( 128 );
	WRITE_COORD( m_velocity.x );
	WRITE_COORD( m_velocity.y );
	WRITE_COORD( fabs( m_velocity.z ) * 0.25 );
	WRITE_BYTE( 40 );
	WRITE_SHORT( m_iBodyGibs );
	WRITE_BYTE( 128 );
	WRITE_BYTE( 200 );
	WRITE_BYTE( BREAK_METAL );
	MESSAGE_END();

	UTIL_Remove( this );
}

void COspreyBlackOps::ShowDamage( void )
{
	if ( m_iDoLeftSmokePuff > 0 || RANDOM_LONG( 0, 99 ) > m_flLeftHealth )
	{
		Vector vecSrc, vecAngles;

		GetAttachment( 8, vecSrc, vecAngles );
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( vecSrc.x );
		WRITE_COORD( vecSrc.y );
		WRITE_COORD( vecSrc.z );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( RANDOM_LONG( 0, 9 ) + 20 ); // scale * 10
		WRITE_BYTE( 12 );                       // framerate
		MESSAGE_END();
		if ( m_iDoLeftSmokePuff > 0 )
			m_iDoLeftSmokePuff--;
	}
	if ( m_iDoRightSmokePuff > 0 || RANDOM_LONG( 0, 99 ) > m_flRightHealth )
	{
		Vector vecSrc, vecAngles;

		GetAttachment( 4, vecSrc, vecAngles );
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( vecSrc.x );
		WRITE_COORD( vecSrc.y );
		WRITE_COORD( vecSrc.z );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( RANDOM_LONG( 0, 9 ) + 20 ); // scale * 10
		WRITE_BYTE( 12 );                       // framerate
		MESSAGE_END();
		if ( m_iDoRightSmokePuff > 0 )
			m_iDoRightSmokePuff--;
	}
}

void COspreyBlackOps::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if ( ptr->iHitgroup == 2 )
	{
		if ( m_flRightHealth < 0 )
			return;

		m_flRightHealth -= flDamage;
		m_iDoRightSmokePuff = flDamage / 5.0 + 3.0;
	}

	if ( ptr->iHitgroup == 3 )
	{
		if ( m_flLeftHealth < 0 )
			return;

		m_flLeftHealth -= flDamage;
		m_iDoLeftSmokePuff = flDamage / 5.0 + 3.0;
	}

	if ( flDamage > 50 || ptr->iHitgroup == 1 || ptr->iHitgroup == 2 || ptr->iHitgroup == 3 )
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
	else
		UTIL_Sparks( ptr->vecEndPos );
}

void COspreyBlackOps::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszTargetOnDeath" ) )
		m_iszTargetOnDeath = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_fMonsterclipChildren" ) )
		m_fMonsterclipChildren = atoi( pkvd->szValue ) != 0;
	else if ( FStrEq( pkvd->szKeyName, "grunttype" ) )
		m_iGruntType = atoi( pkvd->szValue );
	else
	{
		CBaseMonster::KeyValue( pkvd );
		return;
	}

	pkvd->fHandled = TRUE;
}

void COspreyBlackOps::OnDestroy( void )
{
	CBaseMonster::OnDestroy();
	STOP_SOUND( edict(), CHAN_STATIC, "apache/ap_rotor4.wav" );
}

void COspreyBlackOps::UpdateOnRemove( void )
{
	STOP_SOUND( edict(), CHAN_STATIC, "apache/ap_rotor4.wav" );
}

Vector COspreyBlackOps::BodyTarget( const Vector &posSrc )
{
	Vector right, left, angles;

	GetAttachment( 4, right, angles );
	GetAttachment( 8, left, angles );

	return ( right - posSrc ).Length2D() <= ( left - posSrc ).Length2D() ? right : left;
}

EHANDLE COspreyBlackOps::m_hPreGrunt[MAX_CARRY];
EHANDLE COspreyBlackOps::m_hPreGrunt_friendly[MAX_CARRY];
Vector COspreyBlackOps::m_vecPreOrigin[MAX_CARRY];
Vector COspreyBlackOps::m_vecPreOrigin_friendly[MAX_CARRY];
int COspreyBlackOps::m_iPreUnits;
int COspreyBlackOps::m_iPreUnits_friendly;
bool COspreyBlackOps::m_cancheck2 = true;

void COspreyBlackOps::ResetPrecache( void )
{
	m_cancheck2 = true;
}

void COspreyBlackOps::PreFindAll( void )
{
	if ( !m_cancheck2 )
		return;

	m_cancheck2 = false;
	m_iPreUnits = 0;
	m_iPreUnits_friendly = 0;

	CBaseEntity *osprey = NULL;
	CBaseEntity *grunt = NULL;

	while ( ( osprey = UTIL_FindEntityByClassname( osprey, "monster_blkop_osprey" ) ) != NULL )
	{
		bool friendly = osprey->IsPlayerAlly();
		int &count = friendly ? m_iPreUnits_friendly : m_iPreUnits;
		EHANDLE *pool = friendly ? m_hPreGrunt_friendly : m_hPreGrunt;
		Vector *origins = friendly ? m_vecPreOrigin_friendly : m_vecPreOrigin;

		while ( count < MAX_CARRY && ( grunt = UTIL_FindEntityByClassname( grunt, "monster_male_assassin" ) ) != NULL )
		{
			if ( !grunt->IsAlive() || grunt->IsPlayerAlly() != friendly )
				continue;

			pool[count] = grunt;
			origins[count] = grunt->pev->origin;
			count++;
		}

		if ( count == 0 )
		{
			ALERT( at_console, "Osprey Grunt precache error: No Grunts to resupply!\n" );
			return;
		}

		ALERT( at_console, "Osprey found: %d grunts to resupply\n", m_iPreUnits );
	}
}
