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
/*

===== combat.cpp ========================================================

  functions dealing with damage infliction & death

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "soundent.h"
#include "decals.h"
#include "animation.h"
#include "weapons.h"
#include "game.h"
#include "func_break.h"
#include "user_messages.h"
#include "CASHook.h"
#include "DamageInfo.h"
#include "CCVarSystem.h"
#include "func_tank.h"
#include "gamerules.h"
#include "server_support.h"
#include "portal_trace.h"

#define GIB_MESSAGE_HUMAN_RANDOM	1
#define GIB_MESSAGE_ALIEN_RANDOM	2

extern DLL_GLOBAL Vector		g_vecAttackDir;
extern DLL_GLOBAL int			g_iSkillLevel;

extern Vector VecBModelOrigin( entvars_t* pevBModel );
extern entvars_t *g_pevLastInflictor;

#define GERMAN_GIB_COUNT		4
#define	HUMAN_GIB_COUNT			6
#define ALIEN_GIB_COUNT			4


// HACKHACK -- The gib velocity equations don't work
void CGib :: LimitVelocity( void )
{
	float length = pev->velocity.Length();

	// ceiling at 1500.  The gib velocity equation is not bounded properly.  Rather than tune it
	// in 3 separate places again, I'll just limit it here.
	if ( length > 1500.0 )
		pev->velocity = pev->velocity.Normalize() * 1500;		// This should really be sv_maxvelocity * 0.75 or something

	if ( pev->waterlevel > 0 )
		pev->velocity = pev->velocity * 0.33;

	pev->movetype = MOVETYPE_TOSS;
	pev->nextthink = gpGlobals->time + 1;
}


void CGib :: SpawnStickyGibs( entvars_t *pevVictim, Vector vecOrigin, int cGibs )
{
	int i;

	if ( g_Language == LANGUAGE_GERMAN )
	{
		// no sticky gibs in germany right now!
		return; 
	}

	for ( i = 0 ; i < cGibs ; i++ )
	{
		CGib *pGib = GetClassPtr( (CGib *)NULL );

		pGib->Spawn( "models/stickygib.mdl" );
		pGib->pev->body = RANDOM_LONG(0,2);

		if ( pevVictim )
		{
			pGib->pev->origin.x = vecOrigin.x + RANDOM_FLOAT( -3, 3 );
			pGib->pev->origin.y = vecOrigin.y + RANDOM_FLOAT( -3, 3 );
			pGib->pev->origin.z = vecOrigin.z + RANDOM_FLOAT( -3, 3 );

			/*
			pGib->pev->origin.x = pevVictim->absmin.x + pevVictim->size.x * (RANDOM_FLOAT ( 0 , 1 ) );
			pGib->pev->origin.y = pevVictim->absmin.y + pevVictim->size.y * (RANDOM_FLOAT ( 0 , 1 ) );
			pGib->pev->origin.z = pevVictim->absmin.z + pevVictim->size.z * (RANDOM_FLOAT ( 0 , 1 ) );
			*/

			// make the gib fly away from the attack vector
			pGib->pev->velocity = g_vecAttackDir * -1;

			// mix in some noise
			pGib->pev->velocity.x += RANDOM_FLOAT ( -0.15, 0.15 );
			pGib->pev->velocity.y += RANDOM_FLOAT ( -0.15, 0.15 );
			pGib->pev->velocity.z += RANDOM_FLOAT ( -0.15, 0.15 );

			pGib->pev->velocity = pGib->pev->velocity * 900;

			pGib->pev->avelocity.x = RANDOM_FLOAT ( 250, 400 );
			pGib->pev->avelocity.y = RANDOM_FLOAT ( 250, 400 );

			// copy owner's blood color
			pGib->m_bloodColor = (CBaseEntity::Instance(pevVictim))->BloodColor();
		
			if ( pevVictim->health > -50)
			{
				pGib->pev->velocity = pGib->pev->velocity * 0.7;
			}
			else if ( pevVictim->health > -200)
			{
				pGib->pev->velocity = pGib->pev->velocity * 2;
			}
			else
			{
				pGib->pev->velocity = pGib->pev->velocity * 4;
			}

			
			pGib->pev->movetype = MOVETYPE_TOSS;
			pGib->pev->solid = SOLID_BBOX;
			UTIL_SetSize ( pGib->pev, Vector ( 0, 0 ,0 ), Vector ( 0, 0, 0 ) );
			pGib->SetTouch ( &CGib::StickyGibTouch );
			pGib->SetThink ( &CGib::LimitVelocity );
		}
		pGib->LimitVelocity();
	}
}

void CGib :: SpawnHeadGib( entvars_t *pevVictim )
{
	CGib *pGib = GetClassPtr( (CGib *)NULL );

	if ( g_Language == LANGUAGE_GERMAN )
	{
		pGib->Spawn( "models/germangibs.mdl" );// throw one head
		pGib->pev->body = 0;
	}
	else
	{
		pGib->Spawn( "models/hgibs.mdl" );// throw one head
		pGib->pev->body = 0;
	}

	if ( pevVictim )
	{
		pGib->pev->origin = pevVictim->origin + pevVictim->view_ofs;
		
		edict_t		*pentPlayer = FIND_CLIENT_IN_PVS( pGib->edict() );
		
		if ( RANDOM_LONG ( 0, 100 ) <= 5 && pentPlayer )
		{
			// 5% chance head will be thrown at player's face.
			entvars_t	*pevPlayer;

			pevPlayer = VARS( pentPlayer );
			pGib->pev->velocity = ( ( pevPlayer->origin + pevPlayer->view_ofs ) - pGib->pev->origin ).Normalize() * 300;
			pGib->pev->velocity.z += 100;
		}
		else
		{
			pGib->pev->velocity = Vector (RANDOM_FLOAT(-100,100), RANDOM_FLOAT(-100,100), RANDOM_FLOAT(200,320));
		}


		pGib->pev->avelocity.x = RANDOM_FLOAT ( 100, 200 );
		pGib->pev->avelocity.y = RANDOM_FLOAT ( 100, 320 );

		// copy owner's blood color
		pGib->m_bloodColor = (CBaseEntity::Instance(pevVictim))->BloodColor();
	
		if ( pevVictim->health > -50)
		{
			pGib->pev->velocity = pGib->pev->velocity * 0.7;
		}
		else if ( pevVictim->health > -200)
		{
			pGib->pev->velocity = pGib->pev->velocity * 2;
		}
		else
		{
			pGib->pev->velocity = pGib->pev->velocity * 4;
		}
	}
	pGib->LimitVelocity();
}

void CGib :: SpawnRandomGibs( entvars_t *pevVictim, int cGibs, int human )
{
	if ( !pevVictim )
		return;

	Vector origin = pevVictim->absmin + pevVictim->size * 0.5;

	// make the gibs fly away from the attack vector
	Vector velocity = g_vecAttackDir * -1;

	// mix in some noise
	velocity.x += RANDOM_FLOAT ( -0.25, 0.25 );
	velocity.y += RANDOM_FLOAT ( -0.25, 0.25 );
	velocity.z += RANDOM_FLOAT ( -0.25, 0.25 );

	velocity = velocity * RANDOM_FLOAT ( 320, 400 );

			if ( pevVictim->health > -50)
			{
		velocity = velocity * 0.7;
			}
			else if ( pevVictim->health > -200)
			{
		velocity = velocity * 2;
			}
			else
			{
		velocity = velocity * 4;
			}

	if ( velocity.Length() > 600 )
		velocity = velocity.Normalize() * 600;

	( *g_engfuncs.pfnMessageBegin )( MSG_BROADCAST, gmsgGib, NULL, NULL );
		WRITE_BYTE( human ? GIB_MESSAGE_HUMAN_RANDOM : GIB_MESSAGE_ALIEN_RANDOM );
		WRITE_COORD( origin.x );
		WRITE_COORD( origin.y );
		WRITE_COORD( origin.z );
		WRITE_COORD( velocity.x );
		WRITE_COORD( velocity.y );
		WRITE_COORD( velocity.z );
	MESSAGE_END();
	}


BOOL CBaseMonster :: HasHumanGibs( void )
{
	return BloodColor() == BLOOD_COLOR_RED;
}


BOOL CBaseMonster :: HasAlienGibs( void )
{
	return BloodColor() == BLOOD_COLOR_YELLOW;
}


void CBaseMonster::FadeMonster( void )
{
	StopAnimation();
	pev->velocity = g_vecZero;
	pev->movetype = MOVETYPE_NONE;
	pev->avelocity = g_vecZero;
	pev->animtime = gpGlobals->time;
	pev->effects |= EF_NOINTERP;
	SUB_StartFadeOut();
}

//=========================================================
// GibMonster - create some gore and get rid of a monster's
// model.
//=========================================================
void CBaseMonster :: GibMonster( void )
{
	TraceResult	tr;
	BOOL		gibbed = FALSE;

	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "common/bodysplat.wav", 1, ATTN_NORM);		

	// only humans throw skulls !!!UNDONE - eventually monsters will have their own sets of gibs
	if ( HasHumanGibs() )
	{
		if ( CVAR_GET_FLOAT("violence_hgibs") != 0 )	// Only the player will ever get here
		{
			CGib::SpawnRandomGibs( pev, 4, 1 );	// throw some human gibs.
		}
		gibbed = TRUE;
	}
	else if ( HasAlienGibs() )
	{
		if ( CVAR_GET_FLOAT("violence_agibs") != 0 )	// Should never get here, but someone might call it directly
		{
			CGib::SpawnRandomGibs( pev, 4, 0 );	// Throw alien gibs
		}
		gibbed = TRUE;
	}

	if ( !IsPlayer() )
	{
		if ( gibbed )
		{
			// don't remove players!
			SetThink ( &CBaseMonster::SUB_Remove );
			pev->nextthink = gpGlobals->time;
		}
		else
		{
			FadeMonster();
		}
	}
}

//=========================================================
// GetDeathActivity - determines the best type of death
// anim to play.
//=========================================================
Activity CBaseMonster :: GetDeathActivity ( void )
{
	Activity	deathActivity;
	BOOL		fTriedDirection;
	float		flDot;
	TraceResult	tr;
	Vector		vecSrc;

	vecSrc = Center();

	fTriedDirection = FALSE;
	deathActivity = ACT_DIESIMPLE;// in case we can't find any special deaths to do.

	UTIL_MakeVectors ( pev->angles );
	flDot = DotProduct ( gpGlobals->v_forward, g_vecAttackDir * -1 );

	switch ( m_LastHitGroup )
	{
		// try to pick a region-specific death.
	case HITGROUP_HEAD:
		deathActivity = ACT_DIE_HEADSHOT;
		break;

	case HITGROUP_STOMACH:
		deathActivity = ACT_DIE_GUTSHOT;
		break;

	case HITGROUP_GENERIC:
		// try to pick a death based on attack direction
		fTriedDirection = TRUE;

		if ( flDot > 0.3 )
		{
			deathActivity = ACT_DIEFORWARD;
		}
		else if ( flDot <= -0.3 )
		{
			deathActivity = ACT_DIEBACKWARD;
		}
		break;

	default:
		// try to pick a death based on attack direction
		fTriedDirection = TRUE;

		if ( flDot > 0.3 )
		{
			deathActivity = ACT_DIEFORWARD;
		}
		else if ( flDot <= -0.3 )
		{
			deathActivity = ACT_DIEBACKWARD;
		}
		break;
	}


	// dying in deep water or thrown by a blast: always pick a directional death
	if ( pev->waterlevel > 1 || m_fDeathKnockback )
	{
		fTriedDirection = TRUE;

		if ( flDot > 0.3 )
		{
			deathActivity = ACT_DIEFORWARD;
		}
		else if ( flDot <= -0.3 )
		{
			deathActivity = ACT_DIEBACKWARD;
		}
	}

	// can we perform the prescribed death?
	if ( LookupActivity ( deathActivity ) == ACTIVITY_NOT_AVAILABLE )
	{
		// no! did we fail to perform a directional death? 
		if ( fTriedDirection )
		{
			// if yes, we're out of options. Go simple.
			deathActivity = ACT_DIESIMPLE;
		}
		else
		{
			// cannot perform the ideal region-specific death, so try a direction.
			if ( flDot > 0.3 )
			{
				deathActivity = ACT_DIEFORWARD;
			}
			else if ( flDot <= -0.3 )
			{
				deathActivity = ACT_DIEBACKWARD;
			}
		}
	}

	if ( LookupActivity ( deathActivity ) == ACTIVITY_NOT_AVAILABLE )
	{
		// if we're still invalid, simple is our only option.
		deathActivity = ACT_DIESIMPLE;
	}

	// No room checks for underwater or blast deaths
	if ( pev->waterlevel > 1 || m_fDeathKnockback )
		return deathActivity;

	if ( deathActivity == ACT_DIEFORWARD )
	{
			// make sure there's room to fall forward
			UTIL_TraceHull ( vecSrc, vecSrc + gpGlobals->v_forward * 64, dont_ignore_monsters, head_hull, edict(), &tr );

			if ( tr.flFraction != 1.0 )
			{
				deathActivity = ACT_DIESIMPLE;
			}
	}

	if ( deathActivity == ACT_DIEBACKWARD )
	{
			// make sure there's room to fall backward
			UTIL_TraceHull ( vecSrc, vecSrc - gpGlobals->v_forward * 64, dont_ignore_monsters, head_hull, edict(), &tr );

			if ( tr.flFraction != 1.0 )
			{
				deathActivity = ACT_DIESIMPLE;
			}
	}

	return deathActivity;
}

//=========================================================
// GetSmallFlinchActivity - determines the best type of flinch
// anim to play.
//=========================================================
Activity CBaseMonster :: GetSmallFlinchActivity ( void )
{
	Activity	flinchActivity;
	BOOL		fTriedDirection;
	float		flDot;

	fTriedDirection = FALSE;
	UTIL_MakeVectors ( pev->angles );
	flDot = DotProduct ( gpGlobals->v_forward, g_vecAttackDir * -1 );
	
	switch ( m_LastHitGroup )
	{
		// pick a region-specific flinch
	case HITGROUP_HEAD:
		flinchActivity = ACT_FLINCH_HEAD;
		break;
	case HITGROUP_STOMACH:
		flinchActivity = ACT_FLINCH_STOMACH;
		break;
	case HITGROUP_LEFTARM:
		flinchActivity = ACT_FLINCH_LEFTARM;
		break;
	case HITGROUP_RIGHTARM:
		flinchActivity = ACT_FLINCH_RIGHTARM;
		break;
	case HITGROUP_LEFTLEG:
		flinchActivity = ACT_FLINCH_LEFTLEG;
		break;
	case HITGROUP_RIGHTLEG:
		flinchActivity = ACT_FLINCH_RIGHTLEG;
		break;
	case HITGROUP_GENERIC:
	default:
		// just get a generic flinch.
		flinchActivity = ACT_SMALL_FLINCH;
		break;
	}


	// do we have a sequence for the ideal activity?
	if ( LookupActivity ( flinchActivity ) == ACTIVITY_NOT_AVAILABLE )
	{
		flinchActivity = ACT_SMALL_FLINCH;
	}

	return flinchActivity;
}


// Keep walking corpses aligned to the ground
// Fading corpses keep 1 health
void CBaseMonster::BecomeDead( float flSavedHealth )
{
	pev->takedamage = DAMAGE_YES;// don't let autoaim aim at corpses.

	// fliers and prone monsters drop
	if ( ( pev->movetype == MOVETYPE_FLY && ( pev->flags & FL_FLY ) )
		|| m_MonsterState == MONSTERSTATE_PRONE || m_IdealMonsterState == MONSTERSTATE_PRONE )
	{
		pev->movetype = MOVETYPE_TOSS;
	}

	if ( ShouldFadeOnDeath() )
		pev->health = 1;
	else
		pev->health = pev->max_health / 2;

	AlignToGround();
}

//=========================================================
// AlignToGround - tilts a corpse to the slope under it
// Nothing is done while it hasn't moved, on flat ground,
// or on anything but brushes
//=========================================================
#define ALIGN_MOVED_DIST		8	// moved this far, a corpse still sliding isn't tossed
#define ALIGN_PROBE_DIST		8	// ahead and to the right, where the slope is measured
#define ALIGN_TRACE_HEIGHT		32

void CBaseMonster::AlignToGround( void )
{
	if ( pev->movetype == MOVETYPE_FLY )
		return;

	if ( pev->origin == m_vecAlignOrigin && pev->angles == m_vecAlignAngles )
		return;

	bool fMovedFar = fabs( pev->origin.x - m_vecAlignOrigin.x ) >= ALIGN_MOVED_DIST
		|| fabs( pev->origin.y - m_vecAlignOrigin.y ) >= ALIGN_MOVED_DIST;

	// a corpse knocked up from the ground falls again
	if ( !fMovedFar || ( pev->velocity.x == 0 && pev->velocity.y == 0 ) )
	{
		if ( pev->velocity.z > 1 && ( pev->flags & FL_ONGROUND ) )
			pev->movetype = MOVETYPE_TOSS;
	}

	UTIL_MakeVectors( Vector( 0, pev->angles.y, 0 ) );

	TraceResult tr;

	UTIL_TraceLine( pev->origin + Vector( 0, 0, ALIGN_PROBE_DIST ), pev->origin - Vector( 0, 0, ALIGN_TRACE_HEIGHT ), ignore_monsters, ENT( pev ), &tr );

	if ( tr.vecPlaneNormal == Vector( 0, 0, 1 ) && pev->angles.x == 0 && pev->angles.z == 0 )
		return;

	if ( tr.vecPlaneNormal == m_vecAlignNormal )
		return;

	TraceResult trFront, trRight;
	Vector vecFront = pev->origin + gpGlobals->v_forward * ALIGN_PROBE_DIST + Vector( 0, 0, ALIGN_TRACE_HEIGHT );
	Vector vecRight = pev->origin + gpGlobals->v_right * ALIGN_PROBE_DIST + Vector( 0, 0, ALIGN_TRACE_HEIGHT );

	UTIL_TraceLine( vecFront, vecFront - Vector( 0, 0, ALIGN_TRACE_HEIGHT * 2 ), ignore_monsters, ENT( pev ), &trFront );
	UTIL_TraceLine( vecRight, vecRight - Vector( 0, 0, ALIGN_TRACE_HEIGHT * 2 ), ignore_monsters, ENT( pev ), &trRight );

	CBaseEntity *pFront = CBaseEntity::Instance( trFront.pHit ? trFront.pHit : INDEXENT( 0 ) );
	CBaseEntity *pRight = CBaseEntity::Instance( trRight.pHit ? trRight.pHit : INDEXENT( 0 ) );

	if ( !pFront || !pRight || !pFront->IsBSPModel() || !pRight->IsBSPModel() )
		return;

	if ( !( pev->flags & FL_ONGROUND ) || trFront.flFraction == 1.0 || trRight.flFraction == 1.0 )
		return;

	pev->angles.x = UTIL_VecToAngles( trFront.vecEndPos - tr.vecEndPos ).x;
	pev->angles.z = -UTIL_VecToAngles( trRight.vecEndPos - tr.vecEndPos ).x;

	m_vecAlignAngles = pev->angles;
	m_vecAlignOrigin = pev->origin;
	m_vecAlignNormal = tr.vecPlaneNormal;
}

//=========================================================
// FloatInWater - move corpses toward the water surface
//=========================================================
#define CORPSE_WATER_DRAG		0.7
#define CORPSE_SPIN_SPEED		10
#define CORPSE_FLOAT_SPEED		8

void CBaseMonster::FloatInWater( void )
{
	if ( CVAR_GET_FLOAT( "sv_gravity" ) == 0 )
		return;

	if ( pev->waterlevel > 0 )
	{
		if ( pev->movetype == MOVETYPE_NONE )
			return;

		pev->flags &= ~FL_ONGROUND;
		pev->movetype = MOVETYPE_FLY;
		pev->velocity = pev->velocity * CORPSE_WATER_DRAG;
		pev->avelocity.z = RANDOM_LONG( -CORPSE_SPIN_SPEED, CORPSE_SPIN_SPEED );
		pev->avelocity.y = RANDOM_LONG( -CORPSE_SPIN_SPEED, CORPSE_SPIN_SPEED );
		pev->avelocity.x = RANDOM_LONG( -CORPSE_SPIN_SPEED, CORPSE_SPIN_SPEED );
		pev->velocity.z += CORPSE_FLOAT_SPEED;
		pev->angles.x = 0;
		pev->angles.z = 0;
	}
	else if ( !( pev->flags & FL_ONGROUND ) && pev->movetype != MOVETYPE_NONE )
	{
		pev->velocity.z -= CORPSE_FLOAT_SPEED;
	}
}

//=========================================================
// CheckTimeBasedDamage - every 2 seconds, counts down the
// time-based damage the monster took. Only poison hurts:
// 0.15 of a crossbow bolt for its poisoner, or 2 if there
// is none
//=========================================================
#define MONSTER_TBD_INTERVAL		2
#define MONSTER_TBD_DURATION		2
#define MONSTER_POISON_DURATION		5
#define MONSTER_POISON_SCALE		0.15
#define MONSTER_POISON_SELF_DAMAGE	2

void CBaseMonster::CheckTimeBasedDamage( void )
{
	if ( fabs( gpGlobals->time - m_tbdPrev ) < MONSTER_TBD_INTERVAL )
		return;

	m_tbdPrev = gpGlobals->time;

	for ( int i = 0; i < CDMG_TIMEBASED; i++ )
	{
		int bitDamage = DMG_PARALYZE << i;

		if ( !( m_bitsDamageType & bitDamage ) )
			continue;

		BYTE bDuration;

		switch ( i )
		{
		case itbd_Poison:
			if ( m_pevPoisoner )
			{
				float flDamage = MONSTER_POISON_SCALE * sk_plr_xbow_bolt_monster.value;

				if ( BloodColor() != DONT_BLEED )
					SpawnBlood( pev->origin + pev->view_ofs, BloodColor(), flDamage );

				TakeDamage( m_pevPoisoner, m_pevPoisoner, flDamage, DMG_GENERIC );
			}
			else
			{
				TakeDamage( pev, pev, MONSTER_POISON_SELF_DAMAGE, DMG_GENERIC );
			}

			bDuration = MONSTER_POISON_DURATION;
			break;

		case itbd_DrownRecover:
			bDuration = 0;
			break;

		default:
			bDuration = MONSTER_TBD_DURATION;
			break;
		}

		if ( m_rgbTimeBasedDamage[i] )
		{
			if ( --m_rgbTimeBasedDamage[i] == 0 )
			{
				m_bitsDamageType &= ~bitDamage;
				m_pevPoisoner = NULL;
			}
		}
		else
		{
			m_rgbTimeBasedDamage[i] = bDuration;
		}
	}
}

void CBaseMonster::ClearTimeBasedDamage( void )
{
	for ( int i = 0; i < CDMG_TIMEBASED; i++ )
		m_rgbTimeBasedDamage[i] = 0;
}

//=========================================================
// CheckAttacker - true when mp_npckill protects us from
// pAttacker: 0 protects passive humans and player allies
// from everyone, 2 only from their allies, 1 nobody
//=========================================================
#define NPCKILL_ALLOW		1
#define NPCKILL_ALLIES_SAFE	0
#define NPCKILL_SAME_SIDE	2

bool CBaseMonster::CheckAttacker( CBaseEntity *pAttacker )
{
	int iMode = (int)npckilling.value;

	if ( iMode == NPCKILL_ALLOW )
		return false;

	int iClass = Classify();

	if ( iMode == NPCKILL_ALLIES_SAFE )
		return iClass == CLASS_PLAYER_ALLY || iClass == CLASS_HUMAN_PASSIVE;

	if ( iMode != NPCKILL_SAME_SIDE || pAttacker == NULL )
		return false;

	if ( iClass != CLASS_HUMAN_PASSIVE && iClass != CLASS_PLAYER_ALLY && ( iClass < CLASS_TEAM1 || iClass > CLASS_TEAM4 ) )
		return false;

	return IRelationshipByClass( pAttacker->Classify() ) == R_AL;
}


BOOL CBaseMonster::ShouldGibMonster( int iGib )
{
	if ( ( iGib == GIB_NORMAL && pev->health < GIB_HEALTH_VALUE ) || ( iGib == GIB_ALWAYS ) )
	{
		if ( m_fDeathKnockback && CVAR_GET_FLOAT( "mp_noblastgibs" ) == 1.0f && iGib != GIB_ALWAYS )
			return FALSE;

		return TRUE;
	}

	return FALSE;
}


void CBaseMonster::CallGibMonster( void )
{
	BOOL fade = FALSE;

	if ( HasHumanGibs() )
	{
		if ( CVAR_GET_FLOAT("violence_hgibs") == 0 )
			fade = TRUE;
	}
	else if ( HasAlienGibs() )
	{
		if ( CVAR_GET_FLOAT("violence_agibs") == 0 )
			fade = TRUE;
	}

	pev->takedamage = DAMAGE_NO;
	pev->solid = SOLID_NOT;// do something with the body. while monster blows up
	pev->effects = EF_NODRAW; // make the model invisible.

	if ( !fade )
	{
		GibMonster();
	}

	pev->deadflag = DEAD_DEAD;
	FCheckAITrigger();

	// don't let the status bar glitch for players.with <0 health.
	if (pev->health < -99)
	{
		pev->health = 0;
	}
	
	if ( ShouldFadeOnDeath() )
		UTIL_Remove(this);
}


/*
============
Killed
============
*/
void CBaseMonster :: Killed( entvars_t *pevAttacker, int iGib )
{
	unsigned int	cCount = 0;
	BOOL			fDone = FALSE;

	if ( pev->deadflag != DEAD_DEAD && m_IdealMonsterState != MONSTERSTATE_DEAD )
		g_MonsterKilledHook.Call( 0, this, pevAttacker ? CBaseEntity::Instance( pevAttacker ) : NULL, iGib );

	pev->gaitsequence = 255;
	m_LastHitGroup = HITGROUP_GENERIC;

	if ( HasMemory( bits_MEMORY_KILLED ) )
	{
		if ( ShouldGibMonster( iGib ) )
		{
			CallGibMonster();
			return;
		}
	}
	else
	{
		CFuncTank *tank = static_cast<CFuncTank *>( (CBaseEntity *) m_hSelectedTank );

		if ( tank )
		{
			if ( (CBaseEntity *) tank->m_hMonsterController == this )
				tank->MonsterStopControl();

			m_hSelectedTank = NULL;
		}

		CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 1.0, this );
	}

	Remember( bits_MEMORY_KILLED );

	// clear the deceased's sound channels.(may have been firing or reloading when killed)
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "common/null.wav", 1, ATTN_NORM);
	m_IdealMonsterState = MONSTERSTATE_DEAD;

	if ( pev->deadflag < DEAD_DYING )
		pev->deadflag = DEAD_DYING;

	// Make sure this condition is fired too (TakeDamage breaks out before this happens on death)
	SetConditions( bits_COND_LIGHT_DAMAGE );

	if	( ShouldGibMonster( iGib ) )
	{
		CallGibMonster();
		return;
	}
	else if ( pev->flags & FL_MONSTER )
	{
		SetTouch( NULL );
		BecomeDead( pev->health );
	}
	
	// don't let the status bar glitch for players.with <0 health.
	if ( pev->health < 0 )
	{
		pev->health = 0;
	}
	
	if ( pevAttacker && !FNullEnt( pevAttacker ) )
		pev->enemy = ENT( pevAttacker );

	m_IdealMonsterState = MONSTERSTATE_DEAD;

	// NPC death logging uses sv_log_monster_deaths
	// Kills by other monsters use sv_log_monster_monster_kills
	float flLogDeaths = CVAR_GET_FLOAT( "sv_log_monster_deaths" );

	if ( pevAttacker )
	{
		if ( ( pevAttacker->flags & FL_CLIENT
				&& flLogDeaths != 0.0f )
			|| ( !( pevAttacker->flags & FL_CLIENT )
				&& CVAR_GET_FLOAT( "sv_log_monster_monster_kills" ) != 0.0f ) )
		{
			if ( HasMemory( bits_MEMORY_KILLED ) )
				UTIL_LogPrintf( "\"%s\" has been killed by \"%s\"\n", PlayerLog( ENT( pev ) ), PlayerLog( ENT( pevAttacker ) ) );
			else
				ALERT( at_aiconsole, "\"%s\" has been hurt by \"%s\"\n", PlayerLog( ENT( pev ) ), PlayerLog( ENT( pevAttacker ) ) );
		}
	}
	else if ( ( pev->flags & FL_CLIENT ) || flLogDeaths != 0.0f )
	{
		if ( HasMemory( bits_MEMORY_KILLED ) )
			UTIL_LogPrintf( "\"%s\" died\n", PlayerLog( ENT( pev ) ) );
		else
			ALERT( at_aiconsole, "\"%s\" hurt\n", PlayerLog( ENT( pev ) ) );
	}
}

//
// fade out - slowly fades a entity out, then removes it.
//
// DON'T USE ME FOR GIBS AND STUFF IN MULTIPLAYER! 
// SET A FUTURE THINK AND A RENDERMODE!!
void CBaseEntity :: SUB_StartFadeOut ( void )
{
	pev->avelocity = g_vecZero;
	pev->solid = SOLID_NOT;
	UTIL_SetOrigin( pev, pev->origin );

	if ( pev->flags & FL_MONSTER )
	{
		CBaseMonster *monster = (CBaseMonster *) this;

		pev->movetype = MOVETYPE_TOSS;
		monster->FloatInWater();
		monster->AlignToGround();
		monster->m_fDeathKnockback = false;
	}

	if ( !m_fCanFadeStart
		&& ( ( ( pev->flags & FL_ONGROUND ) && pev->velocity == g_vecZero && pev->waterlevel < 2 )
			|| gpGlobals->time > m_flMaximumFadeWait ) )
	{
		m_fCanFadeStart = true;
		m_flMaximumFadeWaitB = gpGlobals->time + 15.0f;
	}

	if ( ( !m_fCanFadeStart || !( gpGlobals->time > m_flMaximumFadeWaitB ) )
		&& ( pev->flags & FL_MONSTER ) )
	{
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	if (pev->rendermode == kRenderNormal)
	{
		pev->renderamt = 255;
		pev->rendermode = kRenderTransTexture;
	}

	pev->nextthink = gpGlobals->time + 0.1;
	SetThink ( &CBaseEntity::SUB_FadeOut );
}

void CBaseEntity :: SUB_FadeOut ( void  )
{
	if ( pev->flags & FL_MONSTER )
		( (CBaseMonster *) this )->FloatInWater();

	if ( pev->renderamt > 7 )
	{
		pev->renderamt -= 7;
		pev->nextthink = gpGlobals->time + 0.1;
	}
	else
	{
		pev->renderamt = 0;
		pev->nextthink = gpGlobals->time + 0.2;
		SetThink ( &CBaseEntity::SUB_Remove );
	}
}

//=========================================================
// WaitTillLand - in order to emit their meaty scent from
// the proper location, gibs should wait until they stop 
// bouncing to emit their scent. That's what this function
// does.
//=========================================================
void CGib :: WaitTillLand ( void )
{
	if (!IsInWorld())
	{
		UTIL_Remove( this );
		return;
	}

	if ( pev->velocity == g_vecZero )
	{
		SetThink (&CGib::SUB_StartFadeOut);
		pev->nextthink = gpGlobals->time + m_lifeTime;

		// If you bleed, you stink!
		if ( m_bloodColor != DONT_BLEED )
		{
			// ok, start stinkin!
			CSoundEnt::InsertSound ( bits_SOUND_MEAT, pev->origin, 384, 25, this );
		}
	}
	else
	{
		// wait and check again in another half second.
		pev->nextthink = gpGlobals->time + 0.5;
	}
}

//
// Gib bounces on the ground or wall, sponges some blood down, too!
//
void CGib :: BounceGibTouch ( CBaseEntity *pOther )
{
	Vector	vecSpot;
	TraceResult	tr;
	
	//if ( RANDOM_LONG(0,1) )
	//	return;// don't bleed everytime

	if (pev->flags & FL_ONGROUND)
	{
		pev->velocity = pev->velocity * 0.9;
		pev->angles.x = 0;
		pev->angles.z = 0;
		pev->avelocity.x = 0;
		pev->avelocity.z = 0;
	}
	else
	{
		if ( g_Language != LANGUAGE_GERMAN && m_cBloodDecals > 0 && m_bloodColor != DONT_BLEED )
		{
			vecSpot = pev->origin + Vector ( 0 , 0 , 8 );//move up a bit, and trace down.
			UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -24 ),  ignore_monsters, ENT(pev), & tr);

			UTIL_BloodDecalTrace( &tr, m_bloodColor );

			m_cBloodDecals--; 
		}

		if ( m_material != matNone && RANDOM_LONG(0,2) == 0 )
		{
			float volume;
			float zvel = fabs(pev->velocity.z);
		
			volume = 0.8 * V_min(1.0, ((float)zvel) / 450.0);

			CBreakable::MaterialSoundRandom( edict(), (Materials)m_material, volume );
		}
	}
}

//
// Sticky gib puts blood on the wall and stays put. 
//
void CGib :: StickyGibTouch ( CBaseEntity *pOther )
{
	Vector	vecSpot;
	TraceResult	tr;
	
	SetThink ( &CGib::SUB_Remove );
	pev->nextthink = gpGlobals->time + 10;

	if ( !FClassnameIs( pOther->pev, "worldspawn" ) )
	{
		pev->nextthink = gpGlobals->time;
		return;
	}

	UTIL_TraceLine ( pev->origin, pev->origin + pev->velocity * 32,  ignore_monsters, ENT(pev), & tr);

	UTIL_BloodDecalTrace( &tr, m_bloodColor );

	pev->velocity = tr.vecPlaneNormal * -1;
	pev->angles = UTIL_VecToAngles ( pev->velocity );
	pev->velocity = g_vecZero; 
	pev->avelocity = g_vecZero;
	pev->movetype = MOVETYPE_NONE;
}

//
// Throw a chunk
//
void CGib :: Spawn( const char *szGibModel )
{
	pev->movetype = MOVETYPE_BOUNCE;
	pev->friction = 0.55; // deading the bounce a bit
	
	// sometimes an entity inherits the edict from a former piece of glass,
	// and will spawn using the same render FX or rendermode! bad!
	pev->renderamt = 255;
	pev->rendermode = kRenderNormal;
	pev->renderfx = kRenderFxNone;
	pev->solid = SOLID_SLIDEBOX;/// hopefully this will fix the VELOCITY TOO LOW crap
	pev->classname = MAKE_STRING("gib");

	SET_MODEL(ENT(pev), szGibModel);
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));

	pev->nextthink = gpGlobals->time + 4;
	m_lifeTime = 7;
	SetThink ( &CGib::WaitTillLand );
	SetTouch ( &CGib::BounceGibTouch );

	m_material = matNone;
	m_cBloodDecals = 5;// how many blood decals this gib can place (1 per bounce until none remain). 
}

// take health
bool CBaseMonster :: TakeHealth (float flHealth, int bitsDamageType, int health_cap)
{
	if (!pev->takedamage && !IsPlayer() )
		return false;

	if ( !CBaseEntity::TakeHealth( flHealth, bitsDamageType, health_cap ) )
		return false;

	// clear out any damage types we healed.
	// UNDONE: generic health should not heal any
	// UNDONE: time-based damage

	m_bitsDamageType &= ~(bitsDamageType & ~DMG_TIMEBASED);

	return true;
}

bool CBaseMonster :: TakeArmor( float flArmor, int bitsDamageType, int armor_cap )
{
	if ( !pev->takedamage )
		return false;

	return CBaseEntity::TakeArmor( flArmor, bitsDamageType, armor_cap );
}

/*
============
TakeDamage

The damage is coming from inflictor, but get mad at attacker
This should be the only function that ever reduces health.
bitsDamageType indicates the type of damage sustained, ie: DMG_SHOCK

Time-based damage: only occurs while the monster is within the trigger_hurt.
When a monster is poisoned via an arrow etc it takes all the poison damage at once.



GLOBALS ASSUMED SET:  g_iSkillLevel
============
*/
int CBaseMonster :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	float	flTake;
	Vector	vecDir;

	if ( ( bitsDamageType & DMG_SHOCK_GLOW ) && !m_fShockGlow )
	{
		CBaseMonster::ShockGlowEffect( true );
		m_flShockGlowTime = (float) ( gpGlobals->time + 0.65 );
	}

	if ( !pev->takedamage || ( pev->flags & FL_GODMODE ) )
		return 0;

	CBaseEntity *pInflictor = CBaseEntity::Instance( pevInflictor );
	CBaseEntity *pAttacker = CBaseEntity::Instance( pevAttacker );
	CBaseEntity *pVictim = this;
	DamageInfo info;

	SetDamageInfo( &info, this, pInflictor, pAttacker, flDamage, bitsDamageType );
	g_MonsterTakeDamageHook.Call( 0, &info );
	GetDamageInfo( &info, &pVictim, &pInflictor, &pAttacker, &flDamage, &bitsDamageType );

	pevInflictor = pInflictor ? pInflictor->pev : NULL;
	pevAttacker = pAttacker ? pAttacker->pev : NULL;

	if ( pevAttacker && !FNullEnt( pevAttacker ) && pAttacker->IsMonster() )
		flDamage *= pAttacker->MyMonsterPointer()->m_flEffectDamage;

	if ( pev->deadflag == DEAD_DEAD )
		return DeadTakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );

	if ( pev->deadflag == DEAD_NO && flDamage > 0 )
	{
		// no pain sound during death animation.
		if ( !pevAttacker || !FClassnameIs( pevAttacker, "monster_barnacle" ) )
			PainSound();// "Ouch!"
	}

	if ( !pevAttacker )
		return 0;

	if ( CheckAttacker( pAttacker ) )
		return 0;

	if ( pevAttacker->health >= 0 )
		GetDamagePoints( pevAttacker, pevInflictor, flDamage );

	if ( pevAttacker->flags & FL_CLIENT )
	{
		if ( pev->spawnflags & SF_MONSTER_WAIT_TILL_SEEN )
		{
			pev->spawnflags &= ~SF_MONSTER_WAIT_TILL_SEEN;
			ChangeSchedule( GetScheduleOfType( SCHED_ALERT_FACE ) );
		}

		if ( pevInflictor && flDamage > 0 )
			flDamage *= g_plCountBalance;
	}

	//!!!LATER - make armor consideration here!
	flTake = flDamage;

	// set damage type sustained
	m_bitsDamageType |= bitsDamageType;

	// poison damage over time is credited to the poisoner
	if ( ( m_bitsDamageType & DMG_TIMEBASED ) && pevInflictor )
		m_pevPoisoner = pevInflictor;

	// grab the vector of the incoming attack. ( pretend that the inflictor is a little lower than it really is, so the body will tend to fly upward a bit).
	vecDir = Vector( 0, 0, 0 );
	if ( pInflictor )
	{
		vecDir = ( pInflictor->Center() - Vector( 0, 0, 24 ) - Center() ).Normalize();
		vecDir = g_vecAttackDir = vecDir.Normalize();
	}

	if ( pevInflictor )
		pev->dmg_inflictor = ENT( pevInflictor );

	// do the damage
	pev->health -= flTake;
	pev->dmg_take += flTake;

	if ( IsPlayer() )
		pev->health = floor( pev->health );

	// HACKHACK Don't kill monsters in a script.  Let them break their scripts first
	if ( m_MonsterState == MONSTERSTATE_SCRIPT )
	{
		SetConditions( bits_COND_LIGHT_DAMAGE );
		return 0;
	}

	if ( pev->health <= 30 )
	{
		SetConditions( bits_COND_HEAVY_DAMAGE );
	}
	else if ( m_flHeavyDamageTime <= gpGlobals->time )
	{
		m_flHeavyDamageTime = gpGlobals->time + 5;
		m_flHeavyDamageTotal = 0;
	}
	else
	{
		m_flHeavyDamageTotal += flTake;

		if ( m_flHeavyDamageTotal > pev->max_health * 0.3 )
		{
			SetConditions( bits_COND_HEAVY_DAMAGE );
			m_flHeavyDamageTotal = 0;
		}
	}

	if ( pev->health <= 0 )
	{
		if ( !( bitsDamageType & ( DMG_BLAST | DMG_BUCKSHOT ) ) || pev->movetype == MOVETYPE_FLY || pev->waterlevel != 0 )
		{
			m_fDeathKnockback = false;
		}
		else if ( !FClassnameIs( pev, "monster_gargantua" )
			&& !FClassnameIs( pev, "monster_bigmomma" )
			&& !FClassnameIs( pev, "monster_babygarg" )
			&& !FClassnameIs( pev, "monster_alien_voltigore" )
			&& !FClassnameIs( pev, "monster_robogrunt" )
			&& !FClassnameIs( pev, "monster_shocktrooper" )
			&& !FClassnameIs( pev, "monster_barnacle" ) )
		{
			float flForce = DamageForce( flDamage, bitsDamageType );

			if ( bitsDamageType & DMG_BLAST )
				flForce = V_max( 256.0f, flForce );
			else
				flForce = V_min( 256.0f, flForce );

			pev->velocity = vecDir * -1 * flForce;

			if ( ( pev->flags & FL_ONGROUND ) || ( bitsDamageType & DMG_BLAST ) )
				pev->velocity.z = V_max( pev->velocity.z, fabs( flDamage ) + 100.0f );

			pev->groundentity = NULL;
			pev->flags &= ~FL_ONGROUND;
			m_fDeathKnockback = true;
		}

		g_pevLastInflictor = pevInflictor;

		if ( bitsDamageType & DMG_ALWAYSGIB )
		{
			Killed( pevAttacker, GIB_ALWAYS );
		}
		else if ( bitsDamageType & DMG_NEVERGIB )
		{
			Killed( pevAttacker, GIB_NEVER );
		}
		else
		{
			Killed( pevAttacker, GIB_NORMAL );
		}

		g_pevLastInflictor = NULL;

		return 0;
	}

	// if this is a player, move him around!
	if ( !FNullEnt( pevInflictor ) && ( pev->movetype == MOVETYPE_WALK || pev->movetype == MOVETYPE_STEP ) && pevAttacker->solid != SOLID_TRIGGER )
	{
		if ( ( bitsDamageType & ( DMG_BLAST | DMG_BUCKSHOT ) ) && !IsPlayer() )
		{
			float flForce = DamageForce( flDamage, bitsDamageType );

			if ( bitsDamageType & DMG_ENERGYBEAM )
				flForce = V_min( 256.0f, flForce );
			else if ( bitsDamageType & DMG_BUCKSHOT )
				flForce = V_min( 256.0f, flForce );
			else
				flForce = V_max( 256.0f, flForce );

			pev->velocity = pev->velocity - vecDir * flForce;
		}
		else if ( IsPlayer() )
		{
			pev->velocity = pev->velocity + vecDir * -DamageForce( flDamage, bitsDamageType );
		}

		pev->flags &= ~FL_ONGROUND;
	}

	// react to the damage (get mad)
	if ( ( pev->flags & FL_MONSTER ) && !FNullEnt( pevAttacker ) )
	{
		if ( pevAttacker->flags & ( FL_MONSTER | FL_CLIENT ) )
		{// only if the attack was a monster or client!

			// enemy's last known position is somewhere down the vector that the attack came from.
			if ( pevInflictor )
			{
				if ( m_hEnemy == NULL || pevInflictor == m_hEnemy->pev || !HasConditions( bits_COND_SEE_ENEMY ) )
				{
					m_vecEnemyLKP = pevInflictor->origin;
				}
			}
			else
			{
				m_vecEnemyLKP = pev->origin + ( g_vecAttackDir * 64 );
			}

			// add pain to the conditions
			// !!!HACKHACK - fudged for now. Do we want to have a virtual function to determine what is light and
			// heavy damage per monster class?
			if ( flDamage > 0 )
			{
				SetConditions( bits_COND_LIGHT_DAMAGE );
			}

			if ( flDamage >= 20 )
			{
				SetConditions( bits_COND_HEAVY_DAMAGE );
			}
		}
	}

	return 1;
}

//=========================================================
// DeadTakeDamage - takedamage function called when a monster's
// corpse is damaged.
//=========================================================
int CBaseMonster :: DeadTakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	Vector			vecDir;

	// grab the vector of the incoming attack. ( pretend that the inflictor is a little lower than it really is, so the body will tend to fly upward a bit).
	vecDir = Vector( 0, 0, 0 );
	if (!FNullEnt( pevInflictor ))
	{
		CBaseEntity *pInflictor = CBaseEntity :: Instance( pevInflictor );
		if (pInflictor)
		{
			vecDir = ( pInflictor->Center() - Vector ( 0, 0, 10 ) - Center() ).Normalize();
			vecDir = g_vecAttackDir = vecDir.Normalize();
		}
	}

#if 0// turn this back on when the bounding box issues are resolved.

	pev->flags &= ~FL_ONGROUND;
	pev->origin.z += 1;
	
	// let the damage scoot the corpse around a bit.
	if ( !FNullEnt(pevInflictor) && (pevAttacker->solid != SOLID_TRIGGER) )
	{
		pev->velocity = pev->velocity + vecDir * -DamageForce( flDamage );
	}

#endif

	// kill the corpse if enough damage was done to destroy the corpse and the damage is of a type that is allowed to destroy the corpse.
	if ( bitsDamageType & DMG_GIB_CORPSE )
	{
		if ( pev->health <= flDamage )
		{
			pev->health = -50;
			Killed( pevAttacker, GIB_ALWAYS );
			return 0;
		}
		// Accumulate corpse gibbing damage, so you can gib with multiple hits
		pev->health -= flDamage * 0.1;
	}
	
	return 1;
}

#define DAMAGE_FORCE_SCALE			4.0f
#define DAMAGE_FORCE_SCALE_BLAST	5.0f
#define DAMAGE_FORCE_MAX			512.0f

float CBaseMonster :: DamageForce( float damage )
{
	return DamageForce( damage, DMG_GENERIC );
}

float CBaseMonster::DamageForce( float damage, int bitsDamageType )
{
	float scale = ( bitsDamageType & DMG_BLAST ) ? DAMAGE_FORCE_SCALE_BLAST : DAMAGE_FORCE_SCALE;
	float force = damage * ( ( 32 * 32 * 72.0f ) / ( pev->size.x * pev->size.y * pev->size.z ) ) * scale;

	if ( force > DAMAGE_FORCE_MAX )
		force = DAMAGE_FORCE_MAX;

	return force;
}

//
// RadiusDamage - this entity is exploding, or otherwise needs to inflict damage upon entities within a certain range.
//
// only damage ents that can clearly be seen by the explosion!

#define VOLTIGORE_BLAST_MONSTER_SCALE	0.2

void RadiusDamage( Vector vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, float flRadius, int iClassIgnore, int bitsDamageType )
{
	CBaseEntity *pEntity = NULL;
	TraceResult	tr;
	float		flAdjustedDamage, falloff;
	Vector		vecSpot;

	if ( flRadius )
		falloff = flDamage / flRadius;
	else
		falloff = 1.0;

	int bInWater = (UTIL_PointContents ( vecSrc ) == CONTENTS_WATER);

	vecSrc.z += 1;// in case grenade is lying on the ground

	if ( !pevAttacker )
		pevAttacker = pevInflictor;

	CBaseEntity *pAttacker = CBaseEntity::Instance( pevAttacker );

	// iterate on all entities in the vicinity.
	while ((pEntity = UTIL_FindEntityInSphere( pEntity, vecSrc, flRadius, NULL, "targetname" )) != NULL)
	{
		if ( pEntity->pev->takedamage != DAMAGE_NO )
		{
			if ( pAttacker && pEntity->IsPlayer() && !g_pGameRules->FPlayerCanTakeDamage( (CBasePlayer *) pEntity, pAttacker ) )
				continue;

			// UNDONE: this should check a damage mask, not an ignore
			if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
			{// houndeyes don't hurt other houndeyes with their attack
				continue;
			}

			// blast's don't tavel into or out of water
			if (bInWater && pEntity->pev->waterlevel == 0)
				continue;
			if (!bInWater && pEntity->pev->waterlevel == 3)
				continue;

			vecSpot = pEntity->BodyTarget( vecSrc );
			
			UTIL_TraceLine ( vecSrc, vecSpot, dont_ignore_monsters, ENT(pevInflictor), &tr );

			if ( tr.flFraction == 1.0 || tr.pHit == pEntity->edict() )
			{// the explosion can 'see' this entity, so hurt them!
				if (tr.fStartSolid)
				{
					// if we're stuck inside them, fixup the position and distance
					tr.vecEndPos = vecSrc;
					tr.flFraction = 0.0;
				}
				
				// decrease damage for an ent that's farther from the bomb.
				flAdjustedDamage = ( vecSrc - tr.vecEndPos ).Length() * falloff;
				flAdjustedDamage = flDamage - flAdjustedDamage;
			
				if ( flAdjustedDamage < 0 )
				{
					flAdjustedDamage = 0;
				}

				// voltigore blasts barely hurt other monsters
				if ( FClassnameIs( pevAttacker, "monster_alien_voltigore" ) && ( pEntity->pev->flags & FL_MONSTER )
					&& !FClassnameIs( pEntity->pev, "monster_apache" ) )
				{
					flAdjustedDamage *= VOLTIGORE_BLAST_MONSTER_SCALE;
				}

				// ALERT( at_console, "hit %s\n", STRING( pEntity->pev->classname ) );
				if (tr.flFraction != 1.0)
				{
					ClearMultiDamage( );

					if ( flAdjustedDamage > 0 )
					{
						pEntity->TraceAttack( pevAttacker, flAdjustedDamage, (tr.vecEndPos - vecSrc).Normalize( ), &tr, bitsDamageType );
						ApplyMultiDamage( pevInflictor, pevAttacker );
					}
				}
				else if ( flAdjustedDamage > 0 )
				{
					pEntity->TakeDamage ( pevInflictor, pevAttacker, flAdjustedDamage, bitsDamageType );
				}
			}
		}
	}
}


void CBaseMonster :: RadiusDamage(entvars_t* pevInflictor, entvars_t*	pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType )
{
	::RadiusDamage( pev->origin, pevInflictor, pevAttacker, flDamage, flDamage * 2.5, iClassIgnore, bitsDamageType );
}


void CBaseMonster :: RadiusDamage( Vector vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType )
{
	::RadiusDamage( vecSrc, pevInflictor, pevAttacker, flDamage, flDamage * 2.5, iClassIgnore, bitsDamageType );
}


//=========================================================
// CheckTraceHullAttack - expects a length to trace, amount 
// of damage to do, and damage type. Returns a pointer to
// the damaged entity in case the monster wishes to do
// other stuff to the victim (punchangle, etc)
//
// Used for many contact-range melee attacks. Bites, claws, etc.
//=========================================================
CBaseEntity* CBaseMonster :: CheckTraceHullAttack( float flDist, int iDamage, int iDmgType )
{
	TraceResult tr;

	if (IsPlayer())
		UTIL_MakeVectors( pev->angles );
	else
		UTIL_MakeAimVectors( pev->angles );

	Vector vecStart = pev->origin;
	vecStart.z += pev->size.z * 0.5;
	Vector vecEnd = vecStart + (gpGlobals->v_forward * flDist );

	// Attack the current enemy directly, without the hull
	// trace, so large or solid enemies that the head hull can miss still
	// take contact damage
	CBaseEntity *pEnemy = m_hEnemy;

	BOOL bTraceHit = FALSE;

	if ( pEnemy )
	{
		if ( pEnemy->IsBSPModel() )
		{
			UTIL_TraceLine( vecStart, m_vecEnemyLKP, dont_ignore_monsters, ENT(pev), &tr );

			if ( tr.pHit )
				bTraceHit = TRUE;
		}

		if ( bTraceHit
			|| ( pEnemy->pev->groundentity == pev->pContainingEntity
				&& ( pEnemy->pev->flags & FL_ONGROUND )
				&& pEnemy->pev->velocity.z < 10.0 ) )
		{
			if ( iDamage > 0 )
			{
				pEnemy->TakeDamage( pev, pev, iDamage, iDmgType );
			}

			return pEnemy;
		}

		if ( flDist > ( vecStart - pEnemy->Center() ).Length() )
		{
			if ( iDamage > 0 )
			{
				pEnemy->TakeDamage( pev, pev, iDamage, iDmgType );
			}

			return pEnemy;
		}
	}

	UTIL_TraceHull( vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr );

	if ( tr.pHit )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if ( iDamage > 0 )
		{
			pEntity->TakeDamage( pev, pev, iDamage, iDmgType );
		}

		return pEntity;
	}

	return NULL;
}


//=========================================================
// FInViewCone - returns true is the passed ent is in
// the caller's forward view cone. The dot product is performed
// in 2d, making the view cone infinitely tall. 
//=========================================================
BOOL CBaseMonster :: FInViewCone ( CBaseEntity *pEntity )
{
	Vector2D	vec2LOS;
	float	flDot;

	UTIL_MakeVectors ( pev->angles );
	
	vec2LOS = ( pEntity->pev->origin - pev->origin ).Make2D();
	vec2LOS = vec2LOS.Normalize();

	flDot = DotProduct (vec2LOS , gpGlobals->v_forward.Make2D() );

	if ( flDot > m_flFieldOfView )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

//=========================================================
// FInViewCone - returns true is the passed vector is in
// the caller's forward view cone. The dot product is performed
// in 2d, making the view cone infinitely tall. 
//=========================================================
BOOL CBaseMonster :: FInViewCone ( Vector *pOrigin )
{
	Vector2D	vec2LOS;
	float		flDot;

	UTIL_MakeVectors ( pev->angles );
	
	vec2LOS = ( *pOrigin - pev->origin ).Make2D();
	vec2LOS = vec2LOS.Normalize();

	flDot = DotProduct (vec2LOS , gpGlobals->v_forward.Make2D() );

	if ( flDot > m_flFieldOfView )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

//=========================================================
// FVisible - returns true if a line can be traced from
// the caller's eyes to the target
//=========================================================
bool CBaseEntity :: FVisible ( CBaseEntity *pEntity, bool fIgnoreGlass )
{
	TraceResult tr;
	Vector		vecLookerOrigin;
	Vector		vecTargetOrigin;
	
	if (FBitSet( pEntity->pev->flags, FL_NOTARGET ))
		return FALSE;

	// don't look through water
	if ((pev->waterlevel != 3 && pEntity->pev->waterlevel == 3) 
		|| (pev->waterlevel == 3 && pEntity->pev->waterlevel == 0))
		return FALSE;

	vecLookerOrigin = pev->origin + pev->view_ofs;//look through the caller's 'eyes'
	vecTargetOrigin = pEntity->EyePosition();

	// a monster walking up to man a tank sees through the tank's eye
	if ( pev->flags & FL_MONSTER )
	{
		CBaseMonster *pMonster = (CBaseMonster *) this;

		if ( pMonster->m_pSchedule && pMonster->m_pSchedule == pMonster->GetScheduleOfType( SCHED_MOVE_TO_TANK ) )
		{
			edict_t *tankEntity = pMonster->m_hSelectedTank.Get();

			if ( tankEntity && !tankEntity->free && tankEntity->pvPrivateData )
			{
				if ( ((CBaseEntity *) tankEntity->pvPrivateData)->FVisible( vecTargetOrigin ) )
					return TRUE;
			}
		}
	}

	UTIL_TraceLine(vecLookerOrigin, vecTargetOrigin, ignore_monsters, fIgnoreGlass ? ignore_glass : dont_ignore_glass, ENT(pev)/*pentIgnore*/, &tr);
	
	if (tr.flFraction != 1.0)
	{
		// ...unless the line stopped on the target itself
		if ( pEntity->pev )
			return tr.pHit == pEntity->pev->pContainingEntity;

		return tr.pHit == NULL;
	}

	return TRUE;// line of sight is valid.
}

//=========================================================
// FVisible - returns true if a line can be traced from
// the caller's eyes to the target vector
//=========================================================
BOOL CBaseEntity :: FVisible ( const Vector &vecTargetOrigin )
{
	TraceResult tr;
	Vector		vecLookerOrigin;
	
	vecLookerOrigin = EyePosition();

	UTIL_TraceLine(vecLookerOrigin, vecTargetOrigin, ignore_monsters, ignore_glass, ENT(pev)/*pentIgnore*/, &tr);
	
	if (tr.flFraction != 1.0)
	{
		return FALSE;// Line of sight is not established
	}
	else
	{
		return TRUE;// line of sight is valid.
	}
}

/*
================
TraceAttack
================
*/
void CBaseEntity::TraceAttack(entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	Vector vecOrigin = ptr->vecEndPos - vecDir * 4;

	if ( pev->takedamage )
	{
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );

		int blood = BloodColor();
		
		if ( blood != DONT_BLEED )
		{
			SpawnBlood(vecOrigin, blood, flDamage);// a little surface blood.
			TraceBleed( flDamage, vecDir, ptr, bitsDamageType );
		}
	}
}


/*
//=========================================================
// TraceAttack
//=========================================================
void CBaseMonster::TraceAttack(entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	Vector vecOrigin = ptr->vecEndPos - vecDir * 4;

	ALERT ( at_console, "%d\n", ptr->iHitgroup );


	if ( pev->takedamage )
	{
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );

		int blood = BloodColor();
		
		if ( blood != DONT_BLEED )
		{
			SpawnBlood(vecOrigin, blood, flDamage);// a little surface blood.
		}
	}
}
*/

//=========================================================
// TraceAttack
//=========================================================
void CBaseMonster :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	if ( pev->takedamage )
	{
		m_LastHitGroup = ptr->iHitgroup;

		switch ( ptr->iHitgroup )
		{
		case HITGROUP_GENERIC:
			break;
		case HITGROUP_HEAD:
			flDamage *= gSkillData.monHead;
			break;
		case HITGROUP_CHEST:
			flDamage *= gSkillData.monChest;
			break;
		case HITGROUP_STOMACH:
			flDamage *= gSkillData.monStomach;
			break;
		case HITGROUP_LEFTARM:
		case HITGROUP_RIGHTARM:
			flDamage *= gSkillData.monArm;
			break;
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
			flDamage *= gSkillData.monLeg;
			break;
		default:
			break;
		}

		if ( bitsDamageType & DMG_SNIPER )
		{
			int iBloodColor = BloodColor();

			if ( iBloodColor == BLOOD_COLOR_RED )
				iBloodColor = 72;

			UTIL_BloodStream( ptr->vecEndPos, ptr->vecPlaneNormal, iBloodColor, (int) ( flDamage * 3.0f ) );
		}

		if ( bitsDamageType & DMG_BULLET )
		{
			RANDOM_LONG( 0, 1 );
			EMIT_SOUND( ENT(pev), CHAN_BODY, "weapons/bullet_hit1.wav", 1, ATTN_NORM );
		}

		SpawnBlood(ptr->vecEndPos, BloodColor(), flDamage);// a little surface blood.
		TraceBleed( flDamage, vecDir, ptr, bitsDamageType );
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
	}
}

/*
================
FireBullets

Go to the trouble of combining multiple pellets into a single damage call.

This version is used by Monsters.
================
*/
extern cvar_t sk_plr_9mm_bullet;
extern cvar_t sk_plr_9mmAR_bullet;
extern cvar_t sk_plr_762_bullet;
extern cvar_t sk_plr_357_bullet;
extern cvar_t sk_plr_buckshot;
extern cvar_t sk_9mm_bullet;
extern cvar_t sk_9mmAR_bullet;
extern cvar_t sk_556_bullet;
extern cvar_t sk_12mm_bullet;
extern cvar_t sk_otis_bullet;
extern cvar_t sk_massassin_sniper;
extern cvar_t sk_grunt_buckshot;

void TraceBulletImpact( CBaseEntity *entity, entvars_t *attacker, int bulletType, int damage,
                               const Vector &direction, TraceResult *trace, const Vector &source, const Vector &end, int tracer )
{
	if ( damage > 0 && bulletType != BULLET_PLAYER_CUSTOMDAMAGE )
	{
		int damageType;

		if ( bulletType == BULLET_NONE )
		damageType = DMG_CLUB;
		else if ( bulletType == BULLET_PLAYER_SNIPER || bulletType == BULLET_MONSTER_SNIPER )
		damageType = DMG_SNIPER;
		else
		damageType = DMG_BULLET;

		if ( bulletType == BULLET_PLAYER_SAW || bulletType == BULLET_PLAYER_SNIPER || bulletType == BULLET_PLAYER_BUCKSHOT
			 || bulletType == BULLET_MONSTER_SAW || bulletType == BULLET_MONSTER_BUCKSHOT )
			damageType |= DMG_BUCKSHOT;

		if ( entity->IsMonster() )
			damageType |= damage < entity->MyMonsterPointer()->m_iMaxHealth ? DMG_NEVERGIB : DMG_ALWAYSGIB;
		else
			damageType |= DMG_NEVERGIB;

		entity->TraceAttack( attacker, (float) damage, direction, trace, damageType );
		TEXTURETYPE_PlaySound( trace, source, end, bulletType );

		if ( bulletType != BULLET_NONE )
			return;
	}
	else
	{
		switch ( bulletType )
		{
		case BULLET_NONE:
			entity->TraceAttack( attacker, 50, direction, trace, DMG_CLUB );
			TEXTURETYPE_PlaySound( trace, source, end, BULLET_NONE );
			break;

		case BULLET_PLAYER_MP5:
			entity->TraceAttack( attacker, sk_plr_9mmAR_bullet.value, direction, trace, DMG_BULLET );
			return;

		case BULLET_PLAYER_SAW:
			entity->TraceAttack( attacker, sk_556_bullet.value, direction, trace, DMG_BULLET );
			return;

		case BULLET_PLAYER_SNIPER:
			entity->TraceAttack( attacker, sk_plr_762_bullet.value, direction, trace, DMG_SNIPER );
			TEXTURETYPE_PlaySound( trace, source, end, BULLET_PLAYER_SNIPER );
			return;

		case BULLET_PLAYER_357:
			entity->TraceAttack( attacker, sk_plr_357_bullet.value, direction, trace, DMG_BULLET );
			return;

		case BULLET_PLAYER_EAGLE:
			entity->TraceAttack( attacker, sk_plr_357_bullet.value / 3.0f * 2.0f, direction, trace, DMG_BULLET );
			return;

		case BULLET_PLAYER_BUCKSHOT:
			entity->TraceAttack( attacker, sk_plr_buckshot.value, direction, trace, DMG_BULLET | DMG_BUCKSHOT );
			return;

		case BULLET_PLAYER_CUSTOMDAMAGE:
			entity->TraceAttack( attacker, (float) damage, direction, trace, DMG_BULLET );
			return;

		case BULLET_MONSTER_9MM:
			entity->TraceAttack( attacker, sk_9mm_bullet.value, direction, trace, DMG_BULLET );
			TEXTURETYPE_PlaySound( trace, source, end, BULLET_MONSTER_9MM );
			return;

		case BULLET_MONSTER_MP5:
			entity->TraceAttack( attacker, sk_9mmAR_bullet.value, direction, trace, DMG_BULLET );
			return;

		case BULLET_MONSTER_SAW:
			TEXTURETYPE_PlaySound( trace, source, end, BULLET_MONSTER_SAW );
			entity->TraceAttack( attacker, sk_556_bullet.value, direction, trace, DMG_BULLET );
			return;

		case BULLET_MONSTER_12MM:
			entity->TraceAttack( attacker, sk_12mm_bullet.value, direction, trace, DMG_BULLET );

			if ( !tracer )
				TEXTURETYPE_PlaySound( trace, source, end, BULLET_MONSTER_12MM );

			return;

		case BULLET_MONSTER_EAGLE:
			entity->TraceAttack( attacker, sk_otis_bullet.value, direction, trace, DMG_BULLET );
			TEXTURETYPE_PlaySound( trace, source, end, BULLET_MONSTER_EAGLE );
			return;

		case BULLET_MONSTER_SNIPER:
			entity->TraceAttack( attacker, sk_massassin_sniper.value, direction, trace, DMG_SNIPER );
			TEXTURETYPE_PlaySound( trace, source, end, BULLET_MONSTER_SNIPER );
			return;

		case BULLET_MONSTER_BUCKSHOT:
			entity->TraceAttack( attacker, sk_grunt_buckshot.value, direction, trace, DMG_BULLET | DMG_BUCKSHOT );
			return;

		default:
			entity->TraceAttack( attacker, sk_plr_9mm_bullet.value, direction, trace, DMG_BULLET );
			return;
		}
	}

	if ( !FNullEnt( trace->pHit ) && VARS( trace->pHit )->rendermode != kRenderNormal )
		UTIL_DecalTrace( trace, DECAL_GLASSBREAK1 + RANDOM_LONG( 0, 2 ) );
}

void CBaseEntity::FireBullets(ULONG cShots, Vector vecTracerSrc, Vector vecDirShooting, Vector vecSpread, float flDistance, int iBulletType, int iTracerFreq, int iDamage, entvars_t *pevAttacker, FireBulletsDrawMode fDraw )
{
	static int tracerCount;
	Vector vecRight = gpGlobals->v_right;
	Vector vecUp = gpGlobals->v_up;

	bool bPlayerShot = IsPlayer();

	if ( pevAttacker == NULL )
		pevAttacker = pev;

	if ( pevAttacker->groundentity == NULL )
		vecSpread = vecSpread + Vector( 0.05f, 0.05f, 0.05f );

	if ( vecSpread.x > 0 )
		vecSpread.x = vecSpread.x;
	else
		vecSpread.x = 0;

	if ( vecSpread.y > 0 )
		vecSpread.y = vecSpread.y;
	else
		vecSpread.y = 0;

	if ( vecSpread.z > 0 )
		vecSpread.z = vecSpread.z;
	else
		vecSpread.z = 0;

	ClearMultiDamage();
	gMultiDamage.type = DMG_BULLET | DMG_NEVERGIB;

	for (ULONG iShot = 0; iShot < cShots; ++iShot )
	{
		// get circular gaussian spread
		float x, y;
		do {
			x = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
			y = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
		} while ( x*x+y*y > 1);

		Vector vecDir = vecDirShooting +
						x * vecSpread.x * vecRight +
						y * vecSpread.y * vecUp;
		PortalTraceResult tr;

		UTIL_TracePortal( vecTracerSrc, vecDir, flDistance, dont_ignore_monsters, dont_ignore_glass,
						  ENT( pev ), bPlayerShot ? (CBasePlayer *)this : NULL, &tr );

		bool DrawTracer = iTracerFreq != 0 && tracerCount++ % iTracerFreq == 0;
		bool DrawDecal = false;
		int tracer = DrawTracer && iTracerFreq != 1;

		if ( !bPlayerShot )
		{
			for ( unsigned int i = 0; i < tr.m_Steps.size(); i++ )
			{
				PortalTraceStep *step = tr.m_Steps[ i ];

				( *g_engfuncs.pfnMessageBegin )( MSG_PVS, gmsgTracerDecal, step->m_vecStart, NULL );
					WRITE_COORD( step->m_vecStart.x );
					WRITE_COORD( step->m_vecStart.y );
					WRITE_COORD( step->m_vecStart.z );
					WRITE_COORD( step->m_vecEnd.x );
					WRITE_COORD( step->m_vecEnd.y );
					WRITE_COORD( step->m_vecEnd.z );
				WRITE_BYTE( DrawTracer ); //Flags: 1 = DrawTracer ----  0 = Don't draw tracer
				WRITE_BYTE( DrawDecal ); //Flags: 1 = DrawDecal ----  0 = Don't draw decal
				MESSAGE_END();
			}

			CSoundEnt::InsertSound( bits_SOUND_BULLETHIT, tr.vecEndPos, 128, 1.0f, this );
		}

		// do damage, paint decals
		if ( tr.flFraction != 1.0 )
		{
			PortalTraceStep *step = tr.m_Steps.back();
			CBaseEntity *pEntity = CBaseEntity::Instance( FNullEnt( tr.pHit ) ? g_engfuncs.pfnPEntityOfEntIndex( 0 ) : tr.pHit );

			TraceBulletImpact( pEntity, pevAttacker, iBulletType, iDamage, tr.m_vecDirection, &tr,
							   step->m_vecStart, tr.vecEndPos, tracer );
		}

		for ( unsigned int i = 0; i < tr.m_Steps.size(); i++ )
		{
			PortalTraceStep *step = tr.m_Steps[ i ];

			UTIL_BubbleTrail( step->m_vecStart, step->m_vecEnd, (int)( step->m_flDistance * 0.015625f ) );
		}
	}
	ApplyMultiDamage(pev, pevAttacker);
}

void CBaseEntity :: TraceBleed( float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if (BloodColor() == DONT_BLEED)
		return;
	
	if (flDamage == 0)
		return;

	if (! (bitsDamageType & (DMG_CRUSH | DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_CLUB | DMG_MORTAR)))
		return;
	
	// make blood decal on the wall! 
	TraceResult Bloodtr;
	Vector vecTraceDir; 
	float flNoise;
	int cCount;
	int i;

/*
	if ( !IsAlive() )
	{
		// dealing with a dead monster. 
		if ( pev->max_health <= 0 )
		{
			// no blood decal for a monster that has already decalled its limit.
			return; 
		}
		else
		{
			pev->max_health--;
		}
	}
*/

	if (flDamage < 10)
	{
		flNoise = 0.1;
		cCount = 1;
	}
	else if (flDamage < 25)
	{
		flNoise = 0.2;
		cCount = 2;
	}
	else
	{
		flNoise = 0.3;
		cCount = 4;
	}

	for ( i = 0 ; i < cCount ; i++ )
	{
		vecTraceDir = vecDir * -1;// trace in the opposite direction the shot came from (the direction the shot is going)

		vecTraceDir.x += RANDOM_FLOAT( -flNoise, flNoise );
		vecTraceDir.y += RANDOM_FLOAT( -flNoise, flNoise );
		vecTraceDir.z += RANDOM_FLOAT( -flNoise, flNoise );

		UTIL_TraceLine( ptr->vecEndPos, ptr->vecEndPos + vecTraceDir * -172, ignore_monsters, ENT(pev), &Bloodtr);

		if ( Bloodtr.flFraction != 1.0 )
		{
			UTIL_BloodDecalTrace( &Bloodtr, BloodColor() );
		}
	}
}

//=========================================================
//=========================================================
void CBaseMonster :: MakeDamageBloodDecal ( int cCount, float flNoise, TraceResult *ptr, const Vector &vecDir )
{
	// make blood decal on the wall! 
	TraceResult Bloodtr;
	Vector vecTraceDir; 
	int i;

	if ( !IsAlive() )
	{
		// dealing with a dead monster. 
		if ( pev->max_health <= 0 )
		{
			// no blood decal for a monster that has already decalled its limit.
			return; 
		}
		else
		{
			pev->max_health--;
		}
	}

	for ( i = 0 ; i < cCount ; i++ )
	{
		vecTraceDir = vecDir;

		vecTraceDir.x += RANDOM_FLOAT( -flNoise, flNoise );
		vecTraceDir.y += RANDOM_FLOAT( -flNoise, flNoise );
		vecTraceDir.z += RANDOM_FLOAT( -flNoise, flNoise );

		UTIL_TraceLine( ptr->vecEndPos, ptr->vecEndPos + vecTraceDir * 172, ignore_monsters, ENT(pev), &Bloodtr);

/*
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_SHOWLINE);
			WRITE_COORD( ptr->vecEndPos.x );
			WRITE_COORD( ptr->vecEndPos.y );
			WRITE_COORD( ptr->vecEndPos.z );
			
			WRITE_COORD( Bloodtr.vecEndPos.x );
			WRITE_COORD( Bloodtr.vecEndPos.y );
			WRITE_COORD( Bloodtr.vecEndPos.z );
		MESSAGE_END();
*/

		if ( Bloodtr.flFraction != 1.0 )
		{
			UTIL_BloodDecalTrace( &Bloodtr, BloodColor() );
		}
	}
}
