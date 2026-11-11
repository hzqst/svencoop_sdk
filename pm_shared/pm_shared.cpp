/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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

#include "tier0/platform.h"

#include <assert.h>
// The player movement code is built with Vector, like the rest of the game code
#ifdef CLIENT_DLL
#include "util_vector.h"
#else
#include <math.h>
typedef float vec_t;
#include "vector.h"
#define DID_VEC3_T_DEFINE
#define vec3_t Vector
#endif
#include "mathlib.h"
#include "const.h"
#include "usercmd.h"
#include "pm_defs.h"
#include "pm_shared.h"
#include "pm_movevars.h"
#include "pm_debug.h"
#include "CPlayerMove.h"
#include <stdio.h>  // NULL
#include <math.h>   // sqrt
#include <string.h> // strcpy
#include <stdlib.h> // atoi
#include <ctype.h>  // isspace

#ifdef CLIENT_DLL
#include "client_sound.h"
#include "particle_effects.h"

int iHasNewViewAngles;
int iHasNewViewOrigin;
int iIsSpectator;
float vecNewViewAngles[3];
float vecNewViewOrigin[3];
#else
extern "C" void PM_PlaySoundFX_SERVER(int playerIndex, float *origin, int channel,
	char *sample, float volume, float attenuation, int flags, int pitch);
extern "C" char PM_FindMaterialType_SERVER(char *name);
#endif

#pragma warning( disable : 4305 )

//typedef enum {mod_brush, mod_sprite, mod_alias, mod_studio} modtype_t;

playermove_t *pmove = NULL;
CPlayerMove g_Pmove;

/*typedef struct
{
	int			planenum;
	short		children[2];	// negative numbers are contents
} dclipnode_t;*/

/*typedef struct mplane_s
{
	vec3_t	normal;			// surface normal
	float	dist;			// closest appoach to origin
	byte	type;			// for texture axis selection and fast side tests
	byte	signbits;		// signx + signy<<1 + signz<<1
	byte	pad[2];
} mplane_t;*/

/*typedef struct hull_s
{
	dclipnode_t	*clipnodes;
	mplane_t	*planes;
	int			firstclipnode;
	int			lastclipnode;
	vec3_t		clip_mins;
	vec3_t		clip_maxs;
} hull_t;*/

// Ducking time
#define TIME_TO_DUCK		0.4
#define VEC_DUCK_HULL_MIN	-18
#define VEC_DUCK_HULL_MAX	18
#define VEC_DUCK_VIEW		12
#define PM_DEAD_VIEWHEIGHT	-8
#define MAX_CLIMB_SPEED		150
#define LADDER_FRICTION_SCALE	3.0
#define STUCK_MOVEUP		1
#define STUCK_MOVEDOWN		-1
#define VEC_HULL_MIN		-36
#define VEC_HULL_MAX		36
#define VEC_VIEW			28
#define	DIST_EPSILON		0.03125
#define	FRACTION_EPSILON	0.0001f

// usercmd moves arrive in this range and are remapped to clientmaxspeed
#define CMD_MOVE_RANGE		255

#define CHAR_TEX_CONCRETE	'C'			// texture types
#define CHAR_TEX_METAL		'M'
#define CHAR_TEX_DIRT		'D'
#define CHAR_TEX_VENT		'V'
#define CHAR_TEX_GRATE		'G'
#define CHAR_TEX_TILE		'T'
#define CHAR_TEX_SLOSH		'S'
#define CHAR_TEX_WOOD		'W'
#define CHAR_TEX_COMPUTER	'P'
#define CHAR_TEX_GLASS		'Y'
#define CHAR_TEX_SNOW		'O'
#define CHAR_TEX_FLESH		'F'

#define STEP_CONCRETE	0		// default step sound
#define STEP_METAL		1		// metal floor
#define STEP_DIRT		2		// dirt, sand, rock
#define STEP_VENT		3		// ventillation duct
#define STEP_GRATE		4		// metal grating
#define STEP_TILE		5		// floor tiles
#define STEP_SLOSH		6		// shallow liquid puddle
#define STEP_WADE		7		// wading in liquid
#define STEP_LADDER		8		// climbing ladder
#define STEP_WOOD		9
#define STEP_ORGANIC	10
#define STEP_SNOW		11

#define PLAYER_FATAL_FALL_SPEED		1024// approx 60 feet
#define PLAYER_MAX_SAFE_FALL_SPEED	580// approx 20 feet
#define DAMAGE_FOR_FALL_SPEED		(float) 100 / ( PLAYER_FATAL_FALL_SPEED - PLAYER_MAX_SAFE_FALL_SPEED )// damage per unit per second.
#define PLAYER_MIN_BOUNCE_SPEED		200
#define PLAYER_FALL_PUNCH_THRESHHOLD (float)350 // won't punch player's screen/make scrape noise unless player falling at least this fast.

#define PLAYER_LONGJUMP_SPEED 350 // how fast we longjump

#define PLAYER_DUCKING_MULTIPLIER 0.333

// double to float warning
#pragma warning(disable : 4244)
#define max(a, b)  (((a) > (b)) ? (a) : (b))
#define min(a, b)  (((a) < (b)) ? (a) : (b))
// up / down
#define	PITCH	0
// left / right
#define	YAW		1
// fall over
#define	ROLL	2 

#define MAX_CLIENTS 32

#define	CONTENTS_CURRENT_0		-9
#define	CONTENTS_CURRENT_90		-10
#define	CONTENTS_CURRENT_180	-11
#define	CONTENTS_CURRENT_270	-12
#define	CONTENTS_CURRENT_UP		-13
#define	CONTENTS_CURRENT_DOWN	-14

#define CONTENTS_TRANSLUCENT	-15

int CPlayerMove::PlayerShouldBypassEntity( physent_t *entity )
{
	if ( pmove->iuser1 )
		return true;

	if ( entity->player && entity->iuser1 )
		return true;

	if ( entity->solid == SOLID_BSP )
		return false;

	if ( entity->movetype != MOVETYPE_WALK && entity->movetype != MOVETYPE_STEP )
		return false;

	if ( entity->player == 1 )
	{
		if ( entity->iuser4 == 1 && pmove->iuser4 == 1 )
			return true;
	}
	else if ( entity->player == 0 && entity->movetype == MOVETYPE_STEP && entity->iuser2 > 0 )
	{
		return true;
	}

	for ( int i = 0; i < 3; i++ )
	{
		if ( entity->origin[i] + entity->mins[i] > pmove->origin[i] + pmove->player_maxs[pmove->usehull][i] ||
			 pmove->origin[i] + pmove->player_mins[pmove->usehull][i] > entity->origin[i] + entity->maxs[i] )
			return false;
	}

#ifndef CLIENT_DLL
	rgOverlappingEntity[pmove->player_index] = entity->info;
#endif

	return true;
}

// Use a free function for the engine trace callback
int pfnPlayerShouldBypassEntity( physent_t *entity )
{
	return g_Pmove.PlayerShouldBypassEntity( entity );
}

char PM_FindTextureType( char *name )
{
#ifdef CLIENT_DLL
	return PM_FindMaterialType_CLIENT(name);
#else
	return PM_FindMaterialType_SERVER(name);
#endif
}

void CPlayerMove::PlaySoundFX(int playerIndex, float *origin, int channel, char *sample, float volume, float attenuation, int flags, int pitch)
{
	if (!pmove->runfuncs)
		return;

#ifdef CLIENT_DLL
	PM_PlaySoundFX_CLIENT(playerIndex, origin, channel, sample, volume, attenuation, flags, pitch);
#else
	PM_PlaySoundFX_SERVER(playerIndex + 1, origin, channel, sample, volume, attenuation, flags, pitch);
#endif
}

void CPlayerMove::PlayStepSound(int step, float volume, bool force)
{
	static int iSkipStep;
	static const char *sounds[12][8] =
	{
		{ "player/pl_step1.wav", "player/pl_step3.wav", "player/pl_step2.wav", "player/pl_step4.wav",
		  "player/pl_step5.wav", "player/pl_step6.wav", "player/pl_step7.wav", "player/pl_step8.wav" },
		{ "player/pl_metal1.wav", "player/pl_metal3.wav", "player/pl_metal2.wav", "player/pl_metal4.wav" },
		{ "player/pl_dirt1.wav", "player/pl_dirt3.wav", "player/pl_dirt2.wav", "player/pl_dirt4.wav" },
		{ "player/pl_duct1.wav", "player/pl_duct3.wav", "player/pl_duct2.wav", "player/pl_duct4.wav" },
		{ "player/pl_grate1.wav", "player/pl_grate3.wav", "player/pl_grate2.wav", "player/pl_grate4.wav" },
		{ "player/pl_tile1.wav", "player/pl_tile3.wav", "player/pl_tile2.wav", "player/pl_tile4.wav", "player/pl_tile5.wav" },
		{ "player/pl_slosh1.wav", "player/pl_slosh3.wav", "player/pl_slosh2.wav", "player/pl_slosh4.wav" },
		{ "player/pl_wade1.wav", "player/pl_wade2.wav", "player/pl_wade3.wav", "player/pl_wade4.wav" },
		{ "player/pl_ladder1.wav", "player/pl_ladder3.wav", "player/pl_ladder2.wav", "player/pl_ladder4.wav" },
		{ "player/pl_wood1.wav", "player/pl_wood3.wav", "player/pl_wood2.wav", "player/pl_wood4.wav" },
		{ "player/pl_organic1.wav", "player/pl_organic3.wav", "player/pl_organic2.wav", "player/pl_organic4.wav" },
		{ "player/pl_snow1.wav", "player/pl_snow3.wav", "player/pl_snow2.wav", "player/pl_snow4.wav",
		  "player/pl_snow5.wav", "player/pl_snow6.wav", "player/pl_snow7.wav", "player/pl_snow8.wav" }
	};

	pmove->iStepLeft = !pmove->iStepLeft;

	if (!pmove->runfuncs)
		return;

	int irand = pmove->RandomLong(0, 1) + pmove->iStepLeft * 2;

	if (pmove->multiplayer && !pmove->movevars->footsteps)
		return;

	vec3_t hvel;

	VectorCopy(pmove->velocity, hvel);
	hvel[2] = 0;

	if (pmove->multiplayer && !m_pLadder && !force && Length(hvel) <= 220)
		return;

	if (step < STEP_CONCRETE || step > STEP_SNOW)
		step = STEP_CONCRETE;

	if (step == STEP_CONCRETE || step == STEP_SNOW)
		irand += pmove->RandomLong(0, 1) ? 4 : 0;
	else if (step == STEP_TILE && !pmove->RandomLong(0, 4))
		irand = 4;
	else if (step == STEP_WADE)
	{
		if (iSkipStep == 0)
		{
			iSkipStep = 1;
			return;
		}

		if (iSkipStep++ == 3)
			iSkipStep = 0;
	}

	int channel = CHAN_BODY;

	if (step == STEP_SLOSH || step == STEP_WOOD || step == STEP_ORGANIC || step == STEP_SNOW)
		channel = CHAN_AUTO;
	else if (step == STEP_WADE)
		channel = CHAN_ITEM;

	PlaySoundFX(pmove->player_index, pmove->origin, channel, (char *)sounds[step][irand], volume, ATTN_NORM, 0, PITCH_NORM);
}

#ifdef CLIENT_DLL
void CPlayerMove::GenericSplashSearch(void)
{
	if (Length(pmove->velocity) > 150 && pmove->waterlevel > 0 && pmove->waterlevel != 3)
	{
		vec3_t origin;

		VectorCopy(pmove->origin, origin);
		origin[2] += 36;
		origin[0] += pmove->RandomLong(-8, 8);
		origin[1] += pmove->RandomLong(-8, 8);

		while (pmove->PM_PointContents(origin, NULL) == CONTENTS_EMPTY)
			origin[2] -= 1;

		MakeWaterSplash(origin, 0);
	}
}
#endif

int CPlayerMove::MapTextureTypeStepType(char chTextureType)
{
	switch (chTextureType)
	{
		default:
		case CHAR_TEX_CONCRETE:	return STEP_CONCRETE;	
		case CHAR_TEX_METAL: return STEP_METAL;	
		case CHAR_TEX_DIRT: return STEP_DIRT;	
		case CHAR_TEX_VENT: return STEP_VENT;	
		case CHAR_TEX_GRATE: return STEP_GRATE;	
		case CHAR_TEX_TILE: return STEP_TILE;
		case CHAR_TEX_SLOSH: return STEP_SLOSH;
		case CHAR_TEX_WOOD: return STEP_WOOD;
		case CHAR_TEX_FLESH: return STEP_ORGANIC;
		case CHAR_TEX_SNOW: return STEP_SNOW;
	}
}

/*
====================
CatagorizeTextureType

Determine texture info for the texture we are standing on.
====================
*/
void CPlayerMove::CatagorizeTextureType( void )
{
	vec3_t start, end;
	const char *pTextureName;

	VectorCopy( pmove->origin, start );
	VectorCopy( pmove->origin, end );

	// Straight down
	end[2] -= 64;

	// Fill in default values, just in case.
	pmove->sztexturename[sizeof(pmove->sztexturename) - 1] = '\0';
	pmove->chtexturetype = CHAR_TEX_CONCRETE;

	pTextureName = pmove->PM_TraceTexture( pmove->onground, start, end );
	if ( !pTextureName )
		return;

	// strip leading '-0' or '+0~' or '{' or '!'
	if (*pTextureName == '-' || *pTextureName == '+')
		pTextureName += 2;

	if (*pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ')
		pTextureName++;
	// '}}'
	
	strncpy(pmove->sztexturename, pTextureName, sizeof(pmove->sztexturename));
	pmove->sztexturename[15] = 0;
		
	// get texture type
	pmove->chtexturetype = PM_FindTextureType( pmove->sztexturename );	
}

void CPlayerMove::UpdateStepSound( bool force )
{
	int	fWalking;
	float fvol;
	vec3_t knee;
	vec3_t feet;
	vec3_t center;
	float height;
	float speed;
	float velrun;
	float velwalk;
	float flduck;
	int	fLadder;
	int step;

	if ( !force && pmove->flTimeStepSound > 0 )
		return;

	if ( pmove->flags & FL_FROZEN )
		return;

	if (!force)
		CatagorizeTextureType();

	speed = Length( pmove->velocity );

	// determine if we are on a ladder
	fLadder = ( pmove->movetype == MOVETYPE_FLY );// IsOnLadder();

	// UNDONE: need defined numbers for run, walk, crouch, crouch run velocities!!!!	
	if ( ( pmove->flags & FL_DUCKING) || fLadder )
	{
		velwalk = 60;		// These constants should be based on cl_movespeedkey * cl_forwardspeed somehow
		velrun = 80;		// UNDONE: Move walking to server
		flduck = 100;
	}
	else
	{
		velwalk = 120;
		velrun = 210;
		flduck = 0;
	}

	// If we're on a ladder or on the ground, and we're moving fast enough,
	//  play step sound.  Also, if pmove->flTimeStepSound is zero, get the new
	//  sound right away - we just started moving in new level.
	if ( (fLadder || pmove->onground != -1 || force) &&
		(Length(pmove->velocity) > 0.0 || force) &&
		( speed >= velwalk || !pmove->flTimeStepSound ) )
	{
		fWalking = speed < velrun;		

		VectorCopy( pmove->origin, center );
		VectorCopy( pmove->origin, knee );
		VectorCopy( pmove->origin, feet );

		height = pmove->player_maxs[ pmove->usehull ][ 2 ] - pmove->player_mins[ pmove->usehull ][ 2 ];

		knee[2] = pmove->origin[2] - 0.1 * height;
		feet[2] = pmove->origin[2] - 0.5 * height + 2;

		// find out what we're stepping in or on...
		if (fLadder)
		{
			step = STEP_LADDER;
			fvol = 0.35;
			pmove->flTimeStepSound = 350;
		}
		else if ( pmove->PM_PointContents ( knee, NULL ) == CONTENTS_WATER )
		{
			step = STEP_WADE;
			fvol = 0.65;
			pmove->flTimeStepSound = 600;

#ifdef CLIENT_DLL
			GenericSplashSearch();
#endif
		}
		else if (pmove->PM_PointContents(feet, NULL) == CONTENTS_WATER || pmove->waterlevel)
		{
			step = STEP_SLOSH;
			fvol = fWalking ? 0.2 : 0.5;
			pmove->flTimeStepSound = fWalking ? 400 : 300;

#ifdef CLIENT_DLL
			GenericSplashSearch();
#endif
		}
		else
		{
			// find texture under player, if different from current texture, 
			// get material type
			step = MapTextureTypeStepType( pmove->chtexturetype );

			switch ( pmove->chtexturetype )
			{
			default:
			case CHAR_TEX_CONCRETE:						
				fvol = fWalking ? 0.2 : 0.5;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;

			case CHAR_TEX_METAL:	
				fvol = fWalking ? 0.2 : 0.5;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;

			case CHAR_TEX_DIRT:	
				fvol = fWalking ? 0.25 : 0.55;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;

			case CHAR_TEX_VENT:	
				fvol = fWalking ? 0.4 : 0.7;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;

			case CHAR_TEX_GRATE:
				fvol = fWalking ? 0.2 : 0.5;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;

			case CHAR_TEX_TILE:	
				fvol = fWalking ? 0.2 : 0.5;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;

			case CHAR_TEX_SLOSH:
				fvol = fWalking ? 0.2 : 0.5;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;
			}
		}
		
		pmove->flTimeStepSound += flduck; // slower step time if ducking

		// play the sound
		// 35% volume if ducking
		if ( pmove->flags & FL_DUCKING )
		{
			fvol *= 0.35;
		}

		PlayStepSound(step, fvol, force);
	}
}

/*
================
AddToTouched

Add's the trace result to touch list, if contact is not already in list.
================
*/
qboolean CPlayerMove::AddToTouched(pmtrace_t tr, vec3_t impactvelocity)
{
	int i;

	for (i = 0; i < pmove->numtouch; i++)
	{
		if (pmove->touchindex[i].ent == tr.ent)
			break;
	}
	if (i != pmove->numtouch)  // Already in list.
		return false;

	VectorCopy( impactvelocity, tr.deltavelocity );

	if (pmove->numtouch >= MAX_PHYSENTS)
		pmove->Con_DPrintf("Too many entities were touched!\n");

	pmove->touchindex[pmove->numtouch++] = tr;
	return true;
}

/*
================
CheckVelocity

See if the player has a bogus velocity value.
================
*/
void CPlayerMove::CheckVelocity ()
{
	int		i;

//
// bound velocity
//
	for (i=0 ; i<3 ; i++)
	{
		// See if it's bogus.
		if (IS_NAN(pmove->velocity[i]))
		{
			pmove->Con_Printf ("PM  Got a NaN velocity %i\n", i);
			pmove->velocity[i] = 0;
		}
		if (IS_NAN(pmove->origin[i]))
		{
			pmove->Con_Printf ("PM  Got a NaN origin on %i\n", i);
			pmove->origin[i] = 0;
		}

		// Bound it.
		if (pmove->velocity[i] > pmove->movevars->maxvelocity)
			pmove->velocity[i] = pmove->movevars->maxvelocity;
		else if (pmove->velocity[i] < -pmove->movevars->maxvelocity)
			pmove->velocity[i] = -pmove->movevars->maxvelocity;
	}
}

/*
==================
ClipVelocity

Slide off of the impacting object
returns the blocked flags:
0x01 == floor
0x02 == step / wall
==================
*/
int CPlayerMove::ClipVelocity (const Vector &in, const Vector &normal, Vector &out, float overbounce)
{
	float	backoff;
	float	change;
	float	adjust;
	float angle;
	int		i, blocked;

	angle = normal[ 2 ];

	blocked = 0x00;            // Assume unblocked.
	if (angle > 0)      // If the plane that is blocking us has a positive z component, then assume it's a floor.
		blocked |= 0x01;		// 
	if (!angle)         // If the plane has no Z, it is vertical (wall/step)
		blocked |= 0x02;		// 
	
	// Determine how far along plane to slide based on incoming direction.
	// Scale by overbounce factor.
	backoff = DotProduct (in, normal) * overbounce;

	for (i=0 ; i<3 ; i++)
	{
		change = normal[i]*backoff;
		out[i] = in[i] - change;
	}

	// iterate once to make sure we aren't still moving through the plane
	adjust = DotProduct (out, normal);

	if (adjust < 0)
	{
		adjust = min (adjust, -DIST_EPSILON);

		for (i=0 ; i<3 ; i++)
			out[i] -= normal[i]*adjust;
	}

	// Return blocking flags.
	return blocked;
}

void CPlayerMove::StartGravity ()
{
	float	ent_gravity;

	if (pmove->gravity)
		ent_gravity = pmove->gravity;
	else
		ent_gravity = 1.0;

	// Add gravity so they'll be in the correct position during movement
	// yes, this 0.5 looks wrong, but it's not.  
	pmove->velocity[2] -= (ent_gravity * pmove->movevars->gravity * 0.5 * pmove->frametime );
	pmove->velocity[2] += pmove->basevelocity[2] * pmove->frametime;
	pmove->basevelocity[2] = 0;

	CheckVelocity();
}


void CPlayerMove::FinishGravity ()
{
	float	ent_gravity;

	if ( pmove->waterjumptime )
		return;

	if (pmove->gravity)
		ent_gravity = pmove->gravity;
	else
		ent_gravity = 1.0;

	// Get the correct velocity for the end of the dt 
  	pmove->velocity[2] -= (ent_gravity * pmove->movevars->gravity * pmove->frametime * 0.5 );

	CheckVelocity();
}

/*
============
FlyMove

The basic solid body movement clip that slides along multiple planes
============
*/
int CPlayerMove::FlyMove (void)
{
	int			bumpcount, numbumps;
	vec3_t		dir;
	float		d;
	int			numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity, original_velocity;
	vec3_t      new_velocity;
	int			i, j;
	pmtrace_t	trace;
	vec3_t		end;
	float		time_left, allFraction;
	int			blocked;
		
	numbumps  = 4;           // Bump up to four times
	
	blocked   = 0;           // Assume not blocked
	numplanes = 0;           //  and not sliding along any planes
	VectorCopy (pmove->velocity, original_velocity);  // Store original velocity
	VectorCopy (pmove->velocity, primal_velocity);
	
	allFraction = 0;
	time_left = pmove->frametime;   // Total time for this movement operation.

	for (bumpcount=0 ; bumpcount<numbumps ; bumpcount++)
	{
		if (Length (pmove->velocity) == 0.0)
			break;

		// Assume we can move all the way from the current origin to the
		//  end point.
		for (i=0 ; i<3 ; i++)
			end[i] = pmove->origin[i] + time_left * pmove->velocity[i];

		// See if we can make it from origin to end point.
		trace = pmove->PM_PlayerTraceEx( pmove->origin, end, PM_NORMAL, pfnPlayerShouldBypassEntity );

		if (trace.fraction > 0 && trace.fraction < FRACTION_EPSILON)
			trace.fraction = 0;

		allFraction += trace.fraction;
		// If we started in a solid object, or we were in solid space
		//  the whole way, zero out our velocity and return that we
		//  are blocked by floor and wall.
		if (trace.allsolid)
		{	// entity is trapped in another solid
			VectorCopy (vec3_origin, pmove->velocity);
			//Con_DPrintf("Trapped 4\n");
			return 4;
		}

		// If we moved some portion of the total distance, then
		//  copy the end position into the pmove->origin and 
		//  zero the plane counter.
		if (trace.fraction > DIST_EPSILON)
		{	// actually covered some distance
			VectorCopy (trace.endpos, pmove->origin);
			VectorCopy (original_velocity, pmove->velocity);
			numplanes = 0;
		}

		// If we covered the entire distance, we are done
		//  and can return.
		if (trace.fraction == 1)
			 break;		// moved the entire distance

		//if (!trace.ent)
		//	Sys_Error ("PM_PlayerTrace: !trace.ent");

		// Save entity that blocked us (since fraction was < 1.0)
		//  for contact
		// Add it if it's not already in the list!!!
		AddToTouched(trace, pmove->velocity);

		// If the plane we hit has a high z component in the normal, then
		//  it's probably a floor
		if (trace.plane.normal[2] > 0.7)
		{
			blocked |= 1;		// floor
		}
		// If the plane has a zero z component in the normal, then it's a 
		//  step or wall
		if (!trace.plane.normal[2])
		{
			blocked |= 2;		// step / wall
			//Con_DPrintf("Blocked by %i\n", trace.ent);
		}

		// Reduce amount of pmove->frametime left by total time left * fraction
		//  that we covered.
		time_left -= time_left * trace.fraction;
		
		// Did we run out of planes to clip against?
		if (numplanes >= MAX_CLIP_PLANES)
		{	// this shouldn't really happen
			//  Stop our movement if so.
			VectorCopy (vec3_origin, pmove->velocity);
			//Con_DPrintf("Too many planes 4\n");

			break;
		}

		// Set up next clipping plane
		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;
//

// modify original_velocity so it parallels all of the clip planes
//
		if ( numplanes == 1 &&
			pmove->movetype == MOVETYPE_WALK &&
			pmove->onground <= 0 )	// relfect player velocity
		{
			for ( i = 0; i < numplanes; i++ )
			{
				if ( planes[i][2] > 0.7  )
				{// floor or slope
					ClipVelocity( original_velocity, planes[i], new_velocity, 1 );
					VectorCopy( new_velocity, original_velocity );
				}
				else															
					ClipVelocity( original_velocity, planes[i], new_velocity, 1.0 + pmove->movevars->bounce * (1-pmove->friction) );
			}

			VectorCopy( new_velocity, pmove->velocity );
			VectorCopy( new_velocity, original_velocity );
		}
		else
		{
			for (i=0 ; i<numplanes ; i++)
			{
				ClipVelocity (
					original_velocity,
					planes[i],
					pmove->velocity,
					1);
				for (j=0 ; j<numplanes ; j++)
					if (j != i)
					{
						// Are we now moving against this plane?
						if (DotProduct (pmove->velocity, planes[j]) < 0)
							break;	// not ok
					}
				if (j == numplanes)  // Didn't have to clip, so we're ok
					break;
			}
			
			// Did we go all the way through plane set
			if (i != numplanes)
			{	// go along this plane
				// pmove->velocity is set in clipping call, no need to set again.
				;  
			}
			else
			{	// go along the crease
				if (numplanes != 2)
				{
					//Con_Printf ("clip velocity, numplanes == %i\n",numplanes);
					VectorCopy (vec3_origin, pmove->velocity);
					//Con_DPrintf("Trapped 4\n");

					break;
				}
				CrossProduct (planes[0], planes[1], dir);
				d = DotProduct (dir, pmove->velocity);
				VectorScale (dir, d, pmove->velocity );
			}

	//
	// if original velocity is against the original velocity, stop dead
	// to avoid tiny occilations in sloping corners
	//
			if (DotProduct (pmove->velocity, primal_velocity) <= 0)
			{
				//Con_DPrintf("Back\n");
				VectorCopy (vec3_origin, pmove->velocity);
				break;
			}
		}
	}

	if ( allFraction == 0 )
	{
		VectorCopy (vec3_origin, pmove->velocity);
		//Con_DPrintf( "Don't stick\n" );
	}

	return blocked;
}

/*
==============
Accelerate
==============
*/
void CPlayerMove::Accelerate (vec3_t wishdir, float wishspeed, float accel)
{
	int			i;
	float		addspeed, accelspeed, currentspeed;

	// Dead player's don't accelerate
	if (pmove->dead)
		return;

	// If waterjumping, don't accelerate
	if (pmove->waterjumptime)
		return;

	// See if we are changing direction a bit
	currentspeed = DotProduct (pmove->velocity, wishdir);

	// Reduce wishspeed by the amount of veer.
	addspeed = wishspeed - currentspeed;

	// If not going to add any speed, done.
	if (addspeed <= 0)
		return;

	// Determine amount of accleration.
	accelspeed = accel * pmove->frametime * wishspeed * pmove->friction;
	
	// Cap at addspeed
	if (accelspeed > addspeed)
		accelspeed = addspeed;
	
	// Adjust velocity.
	for (i=0 ; i<3 ; i++)
	{
		pmove->velocity[i] += accelspeed * wishdir[i];	
	}
}

/*
=====================
WalkMove

Only used by players.  Moves along the ground when player is a MOVETYPE_WALK.
======================
*/
void CPlayerMove::WalkMove ()
{
	int			clip;
	int			oldonground;
	int i;

	vec3_t		wishvel;
	float       spd;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;

	vec3_t dest, start;
	vec3_t original, originalvel;
	vec3_t down, downvel;
	float downdist, updist;

	pmtrace_t trace;
	
	// Copy movement amounts
	fmove = pmove->cmd.forwardmove;
	smove = pmove->cmd.sidemove;
	
	// Zero out z components of movement vectors
	pmove->forward[2] = 0;
	pmove->right[2]   = 0;
	
	VectorNormalize (pmove->forward);  // Normalize remainder of vectors.
	VectorNormalize (pmove->right);    // 

	for (i=0 ; i<2 ; i++)       // Determine x and y parts of velocity
		wishvel[i] = pmove->forward[i]*fmove + pmove->right[i]*smove;
	
	wishvel[2] = 0;             // Zero out z part of velocity

	VectorCopy (wishvel, wishdir);   // Determine maginitude of speed of move
	wishspeed = VectorNormalize(wishdir);

//
// Clamp to client defined max speed
//
	if (wishspeed > pmove->clientmaxspeed)
	{
		VectorScale (wishvel, pmove->clientmaxspeed/wishspeed, wishvel);
		wishspeed = pmove->clientmaxspeed;
	}

	// Set pmove velocity
	pmove->velocity[2] = 0;
	Accelerate (wishdir, wishspeed, pmove->movevars->accelerate);
	pmove->velocity[2] = 0;

	// Add in any base velocity to the current velocity.
	VectorAdd (pmove->velocity, pmove->basevelocity, pmove->velocity );

	spd = Length( pmove->velocity );

	if (spd < 1.0f)
	{
		VectorClear( pmove->velocity );
		return;
	}

	// If we are not moving, do nothing
	//if (!pmove->velocity[0] && !pmove->velocity[1] && !pmove->velocity[2])
	//	return;

	oldonground = pmove->onground;

// first try just moving to the destination	
	dest[0] = pmove->origin[0] + pmove->velocity[0]*pmove->frametime;
	dest[1] = pmove->origin[1] + pmove->velocity[1]*pmove->frametime;	
	dest[2] = pmove->origin[2];

	// first try moving directly to the next spot
	VectorCopy (dest, start);
	trace = pmove->PM_PlayerTraceEx( pmove->origin, dest, PM_NORMAL, pfnPlayerShouldBypassEntity );
	// If we made it all the way, then copy trace end
	//  as new player position.
	if (trace.fraction == 1)
	{
		VectorCopy (trace.endpos, pmove->origin);
		return;
	}

	if (oldonground == -1 &&   // Don't walk up stairs if not on ground.
		pmove->waterlevel  == 0)
		return;

	if (pmove->waterjumptime)         // If we are jumping out of water, don't do anything more.
		return;

	// Try sliding forward both on ground and up 16 pixels
	//  take the move that goes farthest
	VectorCopy (pmove->origin, original);       // Save out original pos &
	VectorCopy (pmove->velocity, originalvel);  //  velocity.

	// Slide move
	clip = FlyMove ();

	// Copy the results out
	VectorCopy (pmove->origin  , down);
	VectorCopy (pmove->velocity, downvel);

	// Reset original values.
	VectorCopy (original, pmove->origin);

	VectorCopy (originalvel, pmove->velocity);

	// Start out up one stair height
	VectorCopy (pmove->origin, dest);
	dest[2] += pmove->movevars->stepsize;
	
	trace = pmove->PM_PlayerTraceEx( pmove->origin, dest, PM_NORMAL, pfnPlayerShouldBypassEntity );
	// If we started okay and made it part of the way at least,
	//  copy the results to the movement start position and then
	//  run another move try.
	if (!trace.startsolid && !trace.allsolid)
	{
		VectorCopy (trace.endpos, pmove->origin);
	}

// slide move the rest of the way.
	clip = FlyMove ();

// Now try going back down from the end point
//  press down the stepheight
	VectorCopy (pmove->origin, dest);
	dest[2] -= pmove->movevars->stepsize;
	
	trace = pmove->PM_PlayerTraceEx( pmove->origin, dest, PM_NORMAL, pfnPlayerShouldBypassEntity );

	// If the trace ended up in empty space, copy the end
	//  over to the origin.
	if (!trace.startsolid && !trace.allsolid)
	{
		VectorCopy (trace.endpos, pmove->origin);
	}
	// Copy this origion to up.
	VectorCopy (pmove->origin, pmove->up);

	// decide which one went farther
	downdist = (down[0] - original[0])*(down[0] - original[0])
		     + (down[1] - original[1])*(down[1] - original[1]);
	updist   = (pmove->up[0]   - original[0])*(pmove->up[0]   - original[0])
		     + (pmove->up[1]   - original[1])*(pmove->up[1]   - original[1]);

	if (downdist > updist)
	{
		VectorCopy (down   , pmove->origin);
		VectorCopy (downvel, pmove->velocity);
	} else // copy z value from slide move
		pmove->velocity[2] = downvel[2];

}

/*
==================
Friction

Handles both ground friction and water friction
==================
*/
void CPlayerMove::Friction (void)
{
	float	*vel;
	float	speed, newspeed, control;
	float	friction;
	float	drop;
	vec3_t newvel;
	
	// If we are in water jump cycle, don't apply friction
	if (pmove->waterjumptime)
		return;

	// Get velocity
	vel = pmove->velocity;
	
	// Calculate speed
	speed = sqrt(vel[0]*vel[0] +vel[1]*vel[1] + vel[2]*vel[2]);
	
	// If too slow, return
	if (speed < 0.1f)
	{
		return;
	}

	drop = 0;

// apply ground friction
	if (pmove->onground != -1)  // On an entity that is the ground
	{
		vec3_t start, stop;
		pmtrace_t trace;

		start[0] = stop[0] = pmove->origin[0] + vel[0]/speed*16;
		start[1] = stop[1] = pmove->origin[1] + vel[1]/speed*16;
		start[2] = pmove->origin[2] + pmove->player_mins[pmove->usehull][2];
		stop[2] = start[2] - 34;

		trace = pmove->PM_PlayerTraceEx( start, stop, PM_NORMAL, pfnPlayerShouldBypassEntity );

		if (trace.fraction == 1.0)
			friction = pmove->movevars->friction*pmove->movevars->edgefriction;
		else
			friction = pmove->movevars->friction;
		
		// Grab friction value.
		//friction = pmove->movevars->friction;      

		friction *= pmove->friction;  // player friction?

		// Bleed off some speed, but if we have less than the bleed
		//  threshhold, bleed the theshold amount.
		control = (speed < pmove->movevars->stopspeed) ?
			pmove->movevars->stopspeed : speed;
		// Add the amount to t'he drop amount.
		drop += control*friction*pmove->frametime;
	}

// apply water friction
//	if (pmove->waterlevel)
//		drop += speed * pmove->movevars->waterfriction * waterlevel * pmove->frametime;

// scale the velocity
	newspeed = speed - drop;
	if (newspeed < 0)
		newspeed = 0;

	// Determine proportion of old speed we are using.
	newspeed /= speed;

	// Adjust velocity according to proportion.
	newvel[0] = vel[0] * newspeed;
	newvel[1] = vel[1] * newspeed;
	newvel[2] = vel[2] * newspeed;

	VectorCopy( newvel, pmove->velocity );
}

void CPlayerMove::AirAccelerate (vec3_t wishdir, float wishspeed, float accel)
{
	int			i;
	float		addspeed, accelspeed, currentspeed, wishspd = wishspeed;
		
	if (pmove->dead)
		return;
	if (pmove->waterjumptime)
		return;

	// Cap speed
	//wishspd = VectorNormalize (pmove->wishveloc);
	
	if (wishspd > 30)
		wishspd = 30;
	// Determine veer amount
	currentspeed = DotProduct (pmove->velocity, wishdir);
	// See how much to add
	addspeed = wishspd - currentspeed;
	// If not adding any, done.
	if (addspeed <= 0)
		return;
	// Determine acceleration speed after acceleration

	accelspeed = accel * wishspeed * pmove->frametime * pmove->friction;
	// Cap it
	if (accelspeed > addspeed)
		accelspeed = addspeed;
	
	// Adjust pmove vel.
	for (i=0 ; i<3 ; i++)
	{
		pmove->velocity[i] += accelspeed*wishdir[i];	
	}
}

/*
===================
WaterMove

===================
*/
void CPlayerMove::WaterMove (void)
{
	int		i;
	vec3_t	wishvel;
	float	wishspeed;
	vec3_t	wishdir;
	vec3_t	start, dest;
	vec3_t  temp;
	pmtrace_t	trace;

	float speed, newspeed, addspeed, accelspeed;

//
// user intentions
//
	for (i=0 ; i<3 ; i++)
		wishvel[i] = pmove->forward[i]*pmove->cmd.forwardmove + pmove->right[i]*pmove->cmd.sidemove;

	// Sinking after no other movement occurs
	if (!pmove->cmd.forwardmove && !pmove->cmd.sidemove && !pmove->cmd.upmove)
		wishvel[2] -= 60;		// drift towards bottom
	else  // Go straight up by upmove amount.
		wishvel[2] += pmove->cmd.upmove;

	// Copy it over and determine speed
	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	// Cap speed.
	if (wishspeed > pmove->clientmaxspeed)
	{
		VectorScale (wishvel, pmove->clientmaxspeed/wishspeed, wishvel);
		wishspeed = pmove->clientmaxspeed;
	}
	// Slow us down a bit.
	wishspeed *= 0.8;

	VectorAdd (pmove->velocity, pmove->basevelocity, pmove->velocity);
// Water friction
	VectorCopy(pmove->velocity, temp);
	speed = VectorNormalize(temp);
	if (speed)
	{
		newspeed = speed - pmove->frametime * speed * pmove->movevars->friction * pmove->friction;

		if (newspeed < 0)
			newspeed = 0;
		VectorScale (pmove->velocity, newspeed/speed, pmove->velocity);
	}
	else
		newspeed = 0;

//
// water acceleration
//
	if ( wishspeed < 0.1f )
	{
		return;
	}

	addspeed = wishspeed - newspeed;
	if (addspeed > 0)
	{

		VectorNormalize(wishvel);
		accelspeed = pmove->movevars->accelerate * wishspeed * pmove->frametime * pmove->friction;
		if (accelspeed > addspeed)
			accelspeed = addspeed;

		for (i = 0; i < 3; i++)
			pmove->velocity[i] += accelspeed * wishvel[i];
	}

// Now move
// assume it is a stair or a slope, so press down from stepheight above
	VectorMA (pmove->origin, pmove->frametime, pmove->velocity, dest);
	VectorCopy (dest, start);
	start[2] += pmove->movevars->stepsize + 1;
	trace = pmove->PM_PlayerTraceEx( start, dest, PM_NORMAL, pfnPlayerShouldBypassEntity );
	if (!trace.startsolid && !trace.allsolid)	// FIXME: check steep slope?
	{	// walked up the step, so just keep result and exit
		VectorCopy (trace.endpos, pmove->origin);
		return;
	}
	
	// Try moving straight along out normal path.
	FlyMove ();
}


/*
===================
AirMove

===================
*/
void CPlayerMove::AirMove (void)
{
	int			i;
	vec3_t		wishvel;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;

	// Copy movement amounts
	fmove = pmove->cmd.forwardmove;
	smove = pmove->cmd.sidemove;
	
	// Zero out z components of movement vectors
	pmove->forward[2] = 0;
	pmove->right[2]   = 0;
	// Renormalize
	VectorNormalize (pmove->forward);
	VectorNormalize (pmove->right);

	// Determine x and y parts of velocity
	for (i=0 ; i<2 ; i++)       
	{
		wishvel[i] = pmove->forward[i]*fmove + pmove->right[i]*smove;
	}
	// Zero out z part of velocity
	wishvel[2] = 0;             

	 // Determine maginitude of speed of move
	VectorCopy (wishvel, wishdir);  
	wishspeed = VectorNormalize(wishdir);

	// Clamp to client defined max speed
	if (wishspeed > pmove->clientmaxspeed)
	{
		VectorScale (wishvel, pmove->clientmaxspeed/wishspeed, wishvel);
		wishspeed = pmove->clientmaxspeed;
	}

	AirAccelerate (wishdir, wishspeed, pmove->movevars->airaccelerate);

	// Add in any base velocity to the current velocity.
	VectorAdd (pmove->velocity, pmove->basevelocity, pmove->velocity );

	FlyMove ();
}

qboolean CPlayerMove::InWater( void )
{
	return ( pmove->waterlevel > 1 );
}

/*
=============
CheckWater

Sets pmove->waterlevel and pmove->watertype values.
=============
*/
qboolean CPlayerMove::CheckWater ()
{
	vec3_t	point;
	int		cont;
	int		truecont;
	float     height;
	float		heightover2;

	// Pick a spot just above the players feet.
	point[0] = pmove->origin[0] + (pmove->player_mins[pmove->usehull][0] + pmove->player_maxs[pmove->usehull][0]) * 0.5;
	point[1] = pmove->origin[1] + (pmove->player_mins[pmove->usehull][1] + pmove->player_maxs[pmove->usehull][1]) * 0.5;
	point[2] = pmove->origin[2] + pmove->player_mins[pmove->usehull][2] + 1;
	
	// Assume that we are not in water at all.
	pmove->waterlevel = 0;
	pmove->watertype = CONTENTS_EMPTY;

	// Grab point contents.
	cont = pmove->PM_PointContents (point, &truecont );
	// Are we under water? (not solid and not empty?)
	if (cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
	{
		// Set water type
		pmove->watertype = cont;

		// We are at least at level one
		pmove->waterlevel = 1;

		height = (pmove->player_mins[pmove->usehull][2] + pmove->player_maxs[pmove->usehull][2]);
		heightover2 = height * 0.5;

		// Now check a point that is at the player hull midpoint.
		point[2] = pmove->origin[2] + heightover2;
		cont = pmove->PM_PointContents (point, NULL );
		// If that point is also under water...
		if (cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
		{
			// Set a higher water level.
			pmove->waterlevel = 2;

			// Now check the eye position.  (view_ofs is relative to the origin)
			point[2] = pmove->origin[2] + pmove->view_ofs[2];

			cont = pmove->PM_PointContents (point, NULL );
			if (cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT ) 
				pmove->waterlevel = 3;  // In over our eyes
		}

		// Adjust velocity based on water current, if any.
		if ( ( truecont <= CONTENTS_CURRENT_0 ) &&
			 ( truecont >= CONTENTS_CURRENT_DOWN ) )
		{
			// The deeper we are, the stronger the current.
			static vec3_t current_table[] =
			{
				{1, 0, 0}, {0, 1, 0}, {-1, 0, 0},
				{0, -1, 0}, {0, 0, 1}, {0, 0, -1}
			};

			VectorMA (pmove->basevelocity, 50.0*pmove->waterlevel, current_table[CONTENTS_CURRENT_0 - truecont], pmove->basevelocity);
		}
	}

	return pmove->waterlevel > 1;
}

/*
=============
CategorizePosition
=============
*/
void CPlayerMove::CategorizePosition (void)
{
	vec3_t		point;
	pmtrace_t		tr;

// if the player hull point one unit down is solid, the player
// is on ground

// see if standing on something solid	

	// Doing this before we move may introduce a potential latency in water detection, but
	// doing it after can get us stuck on the bottom in water if the amount we move up
	// is less than the 1 pixel 'threshold' we're about to snap to.	Also, we'll call
	// this several times per frame, so we really need to avoid sticking to the bottom of
	// water on each call, and the converse case will correct itself if called twice.
	CheckWater();

	point[0] = pmove->origin[0];
	point[1] = pmove->origin[1];
	point[2] = pmove->origin[2] - 2;

	if (pmove->movetype == MOVETYPE_NOCLIP ||
		pmove->movetype == MOVETYPE_FLY ||
		pmove->velocity[2] > 180)   // Shooting up really fast.  Definitely not on ground.
	{
		pmove->onground = -1;
	}
	else
	{
		// Try and move down.
		tr = pmove->PM_PlayerTraceEx( pmove->origin, point, PM_NORMAL, pfnPlayerShouldBypassEntity );
		// If we hit a steep plane, we are not on ground
		if ( tr.plane.normal[2] < 0.7)
			pmove->onground = -1;	// too steep
		else
			pmove->onground = tr.ent;  // Otherwise, point to index of ent under us.

		// If we are on something...
		if (pmove->onground != -1)
		{
			// Then we are not in water jump sequence
			pmove->waterjumptime = 0;
			// If we could make the move, drop us down that 1 pixel
			if (pmove->waterlevel < 2 && !tr.startsolid && !tr.allsolid)
				VectorCopy (tr.endpos, pmove->origin);
		}

		// Standing on an entity other than the world
		if (tr.ent > 0)   // So signal that we are touching something.
		{
			AddToTouched(tr, pmove->velocity);
		}
	}
}

/*
=================
GetRandomStuckOffsets

When a player is stuck, it's costly to try and unstick them
Grab a test offset for the player based on a passed in index
=================
*/
int CPlayerMove::GetRandomStuckOffsets(int nIndex, int server, Vector &offset)
{
 // Last time we did a full
	int idx;
	idx = rgStuckLast[nIndex][server]++;

	VectorCopy(rgv3tStuckTable[idx % 54], offset);

	return (idx % 54);
}

void CPlayerMove::ResetStuckOffsets(int nIndex, int server)
{
	rgStuckLast[nIndex][server] = 0;
}

/*
=================
NudgePosition

If pmove->origin is in a solid position,
try nudging slightly on all axis to
allow for the cut precision of the net coordinates
=================
*/
#define PM_CHECKSTUCK_MINTIME 0.05  // Don't check again too quickly.

int CPlayerMove::CheckStuck (void)
{
	vec3_t	base;
	vec3_t  offset;
	vec3_t  test;
	int     hitent;
	int		idx;
	float	fTime;
	int i;
	pmtrace_t traceresult;


	// If position is okay, exit
	hitent = pmove->PM_TestPlayerPositionEx( pmove->origin, &traceresult, pfnPlayerShouldBypassEntity );
	if (hitent == -1 )
	{
		ResetStuckOffsets( pmove->player_index, pmove->server );
		return 0;
	}

	VectorCopy (pmove->origin, base);

	// 
	// Deal with precision error in network.
	// 
	if (!pmove->server)
	{
		// World or BSP model
		if ( ( hitent == 0 ) ||
			 ( pmove->physents[hitent].model != NULL ) )
		{
			int nReps = 0;
			ResetStuckOffsets( pmove->player_index, pmove->server );
			do 
			{
				i = GetRandomStuckOffsets(pmove->player_index, pmove->server, offset);

				VectorAdd(base, offset, test);
				if (pmove->PM_TestPlayerPositionEx( test, &traceresult, pfnPlayerShouldBypassEntity ) == -1)
				{
					ResetStuckOffsets( pmove->player_index, pmove->server );
		
					VectorCopy ( test, pmove->origin );
					return 0;
				}
				nReps++;
			} while (nReps < 54);
		}
	}

	// Only an issue on the client.

	if (pmove->server)
		idx = 0;
	else
		idx = 1;

	fTime = pmove->Sys_FloatTime();
	// Too soon?
	if (rgStuckCheckTime[pmove->player_index][idx] >= 
		( fTime - PM_CHECKSTUCK_MINTIME ) )
	{
		return 1;
	}
	rgStuckCheckTime[pmove->player_index][idx] = fTime;

	pmove->PM_StuckTouch( hitent, &traceresult );

	i = GetRandomStuckOffsets(pmove->player_index, pmove->server, offset);

	VectorAdd(base, offset, test);
	if ( ( hitent = pmove->PM_TestPlayerPositionEx( test, NULL, NULL ) ) == -1 )
	{
		//Con_DPrintf("Nudged\n");

		ResetStuckOffsets( pmove->player_index, pmove->server );

		if (i >= 27)
			VectorCopy ( test, pmove->origin );

		return 0;
	}

	// If player is flailing while stuck in another player ( should never happen ), then see
	//  if we can't "unstick" them forceably.
	if ( pmove->cmd.buttons & ( IN_JUMP | IN_DUCK | IN_ATTACK ) && ( pmove->physents[ hitent ].player != 0 ) )
	{
		float x, y, z;
		float xystep = 8.0;
		float zstep = 18.0;
		float xyminmax = xystep;
		float zminmax = 4 * zstep;
		
		for ( z = 0; z <= zminmax; z += zstep )
		{
			for ( x = -xyminmax; x <= xyminmax; x += xystep )
			{
				for ( y = -xyminmax; y <= xyminmax; y += xystep )
				{
					VectorCopy( base, test );
					test[0] += x;
					test[1] += y;
					test[2] += z;

					if ( pmove->PM_TestPlayerPositionEx( test, NULL, NULL ) == -1 )
					{
						VectorCopy( test, pmove->origin );
						return 0;
					}
				}
			}
		}
	}

	//VectorCopy (base, pmove->origin);

	return 1;
}

/*
===============
GetChaseOrigin
===============
*/
void CPlayerMove::GetChaseOrigin(Vector angles, int target, Vector offset, Vector *origin)
{
	vec3_t forward;
	vec3_t start;
	vec3_t end;

	AngleVectors(angles, forward, NULL, NULL);
	VectorCopy(pmove->physents[target].origin, start);
	start[2] += VEC_VIEW;

	for (int i = 0; i < 3; ++i)
		end[i] = start[i] - (forward[i] * 112 + offset[i]);

	pmtrace_t *trace = pmove->PM_TraceLine(start, end, PM_TRACELINE_PHYSENTSONLY, 2, target);

	for (int i = 0; i < 3; ++i)
		(*origin)[i] = (trace->endpos[i] + trace->plane.normal[i] * 4 - pmove->origin[i])
			* pmove->frametime * 10 + pmove->origin[i];
}

/*
===============
SpectatorMove
===============
*/
void CPlayerMove::SpectatorMove(void)
{
#ifdef CLIENT_DLL
	if (pmove->runfuncs)
		iIsSpectator = 1;
#endif

	if (pmove->iuser2)
	{
		int target;

		for (target = 0; target < pmove->numphysent; ++target)
		{
			if (pmove->physents[target].info == pmove->iuser2)
				break;
		}

		if (target == pmove->numphysent)
			return;

		vec3_t angles = {0, 0, 0};
		vec3_t origin;

		if (pmove->iuser1 == OBS_CHASE_LOCKED || pmove->iuser1 == OBS_IN_EYE)
		{
			angles[1] = pmove->physents[target].angles[1];
			angles[2] = pmove->physents[target].angles[2];

#ifdef CLIENT_DLL
			if (pmove->runfuncs)
				iIsSpectator |= 6;
#endif
		}
		else
		{
			VectorCopy(pmove->angles, angles);
		}

		if (pmove->iuser1 == OBS_CHASE_FREE || pmove->iuser1 == OBS_CHASE_LOCKED)
			GetChaseOrigin(angles, target, vec3_origin, &origin);
		else
		{
			VectorCopy(pmove->physents[target].origin, origin);
		}

		VectorCopy(origin, pmove->origin);
		VectorCopy(angles, pmove->angles);
		VectorCopy(vec3_origin, pmove->velocity);

#ifdef CLIENT_DLL
		if (pmove->runfuncs)
		{
			VectorCopy(pmove->angles, vecNewViewAngles);
			iHasNewViewAngles = 1;
			VectorCopy(pmove->origin, vecNewViewOrigin);
			iHasNewViewOrigin = 1;
		}
#endif
		return;
	}

	float speed = Length(pmove->velocity);

	if (speed < 1)
	{
		VectorCopy(vec3_origin, pmove->velocity);
	}
	else
	{
		float friction = pmove->movevars->friction * 1.5;
		float control = max(pmove->movevars->stopspeed, speed);
		float drop = control * friction * pmove->frametime;
		float newspeed = max(0, speed - drop) / speed;

		VectorScale(pmove->velocity, newspeed, pmove->velocity);
	}

	float fmove = pmove->cmd.forwardmove;
	float smove = pmove->cmd.sidemove;
	vec3_t wishvel;
	vec3_t wishdir;

	VectorNormalize(pmove->forward);
	VectorNormalize(pmove->right);

	for (int i = 0; i < 3; ++i)
		wishvel[i] = pmove->forward[i] * fmove + pmove->right[i] * smove
			+ pmove->up[i] * pmove->cmd.upmove;

	VectorCopy(wishvel, wishdir);

	float wishspeed = VectorNormalize(wishdir);

	if (wishspeed > pmove->movevars->spectatormaxspeed)
	{
		VectorScale(wishvel, pmove->movevars->spectatormaxspeed / wishspeed, wishvel);
		wishspeed = pmove->movevars->spectatormaxspeed;
	}

	float currentspeed = DotProduct(pmove->velocity, wishdir);
	float addspeed = wishspeed - currentspeed;

	if (addspeed <= 0)
		return;

	float accelspeed = min(addspeed, pmove->movevars->accelerate * pmove->frametime * wishspeed);

	for (int i = 0; i < 3; ++i)
		pmove->velocity[i] += wishdir[i] * accelspeed;

	VectorMA(pmove->origin, pmove->frametime, pmove->velocity, pmove->origin);
}

/*
==================
SplineFraction

Use for ease-in, ease-out style interpolation (accel/decel)
Used by ducking code.
==================
*/
float CPlayerMove::SplineFraction( float value, float scale )
{
	float valueSquared;

	value = scale * value;
	valueSquared = value * value;

	// Nice little ease-in, ease-out spline-like curve
	return 3 * valueSquared - 2 * valueSquared * value;
}

void CPlayerMove::FixPlayerCrouchStuck( int direction )
{
	int     hitent;
	int i;
	vec3_t test;

	hitent = pmove->PM_TestPlayerPositionEx( pmove->origin, NULL, pfnPlayerShouldBypassEntity );
	if (hitent == -1 )
		return;
	
	VectorCopy( pmove->origin, test );	
	for ( i = 0; i < 36; i++ )
	{
		pmove->origin[2] += direction;
		hitent = pmove->PM_TestPlayerPositionEx( pmove->origin, NULL, pfnPlayerShouldBypassEntity );
		if (hitent == -1 )
			return;
	}

	VectorCopy( test, pmove->origin ); // Failed
}

void CPlayerMove::UnDuck( void )
{
	int i;
	pmtrace_t trace;
	vec3_t newOrigin;

	VectorCopy( pmove->origin, newOrigin );

	if ( pmove->onground != -1 )
	{
		for ( i = 0; i < 3; i++ )
		{
			newOrigin[i] += ( pmove->player_mins[1][i] - pmove->player_mins[0][i] );
		}
	}
	
	trace = pmove->PM_PlayerTraceEx( newOrigin, newOrigin, PM_NORMAL, NULL );

	if ( !trace.startsolid )
	{
		pmove->usehull = 0;

		// Oh, no, changing hulls stuck us into something, try unsticking downward first.
		trace = pmove->PM_PlayerTraceEx( newOrigin, newOrigin, PM_NORMAL, NULL );
		if ( trace.startsolid )
		{
			// See if we are stuck?  If so, stay ducked with the duck hull until we have a clear spot
			//Con_Printf( "unstick got stuck\n" );
			pmove->usehull = 1;
			return;
		}

		pmove->flags &= ~FL_DUCKING;
		pmove->bInDuck  = false;
		pmove->view_ofs[2] = VEC_VIEW;
		pmove->flDuckTime = 0;
		
		VectorCopy( newOrigin, pmove->origin );

		// Recatagorize position since ducking can change origin
		CategorizePosition();
	}
}

void CPlayerMove::Duck( void )
{
	int i;
	float time;
	float duckFraction;

	int buttonsChanged	= ( pmove->oldbuttons ^ pmove->cmd.buttons );	// These buttons have changed this frame
	int nButtonPressed	=  buttonsChanged & pmove->cmd.buttons;		// The changed ones still down are "pressed"

	int duckchange		= buttonsChanged & IN_DUCK ? 1 : 0;
	int duckpressed		= nButtonPressed & IN_DUCK ? 1 : 0;

	if ( pmove->cmd.buttons & IN_DUCK )
	{
		pmove->oldbuttons |= IN_DUCK;
	}
	else
	{
		pmove->oldbuttons &= ~IN_DUCK;
	}

	// Prevent ducking if the iuser3 variable is set
	if ( pmove->iuser3 || pmove->dead )
	{
		// Try to unduck
		if ( pmove->flags & FL_DUCKING )
		{
			UnDuck();
		}
		return;
	}

	if ( pmove->flags & FL_DUCKING )
	{
		pmove->cmd.forwardmove *= PLAYER_DUCKING_MULTIPLIER;
		pmove->cmd.sidemove    *= PLAYER_DUCKING_MULTIPLIER;
		pmove->cmd.upmove      *= PLAYER_DUCKING_MULTIPLIER;
	}

	if ( ( pmove->cmd.buttons & IN_DUCK ) || ( pmove->bInDuck ) || ( pmove->flags & FL_DUCKING ) )
	{
		if ( pmove->cmd.buttons & IN_DUCK )
		{
			if ( (nButtonPressed & IN_DUCK ) && !( pmove->flags & FL_DUCKING ) )
			{
				// Use 1 second so super long jump will work
				pmove->flDuckTime = 1000;
				pmove->bInDuck    = true;
			}

			time = max( 0.0, ( 1.0 - (float)pmove->flDuckTime / 1000.0 ) );
			
			if ( pmove->bInDuck )
			{
				// Finish ducking immediately if duck time is over or not on ground
				if ( ( (float)pmove->flDuckTime / 1000.0 <= ( 1.0 - TIME_TO_DUCK ) ) ||
					 ( pmove->onground == -1 ) )
				{
					pmove->usehull = 1;
					pmove->view_ofs[2] = VEC_DUCK_VIEW;
					pmove->flags |= FL_DUCKING;
					pmove->bInDuck = false;

					// HACKHACK - Fudge for collision bug - no time to fix this properly
					if ( pmove->onground != -1 )
					{
						for ( i = 0; i < 3; i++ )
						{
							pmove->origin[i] -= ( pmove->player_mins[1][i] - pmove->player_mins[0][i] );
						}
						// See if we are stuck?
						FixPlayerCrouchStuck( STUCK_MOVEUP );

						// Recatagorize position since ducking can change origin
						CategorizePosition();
					}
				}
				else
				{
					float fMore = (VEC_DUCK_HULL_MIN - VEC_HULL_MIN);

					// Calc parametric time
					duckFraction = SplineFraction( time, (1.0/TIME_TO_DUCK) );
					pmove->view_ofs[2] = ((VEC_DUCK_VIEW - fMore ) * duckFraction) + (VEC_VIEW * (1-duckFraction));
				}
			}
		}
		else
		{
			// Try to unduck
			UnDuck();
		}
	}
}

void CPlayerMove::LadderMove( void )
{
	physent_t *pLadder = m_pLadder;
	vec3_t		ladderCenter;
	trace_t		trace;
	qboolean	onFloor;
	vec3_t		floor;
	vec3_t		modelmins, modelmaxs;

	if ( pmove->movetype == MOVETYPE_NOCLIP )
		return;
	
#if defined( _TFC )
	// this is how TFC freezes players, so we don't want them climbing ladders
	if ( pmove->maxspeed <= 1.0 )
		return;
#endif

	pmove->PM_GetModelBounds( pLadder->model, modelmins, modelmaxs );

	VectorAdd( modelmins, modelmaxs, ladderCenter );
	VectorScale( ladderCenter, 0.5, ladderCenter );
	VectorAdd( ladderCenter, pLadder->origin, ladderCenter );

	pmove->movetype = MOVETYPE_FLY;

	// On ladder, convert movement to be relative to the ladder

	VectorCopy( pmove->origin, floor );
	floor[2] += pmove->player_mins[pmove->usehull][2] - 1;

	if ( pmove->PM_PointContents( floor, NULL ) == CONTENTS_SOLID )
		onFloor = true;
	else
		onFloor = false;

	pmove->gravity = 0;
	pmove->PM_TraceModel( pLadder, pmove->origin, ladderCenter, &trace );
	if ( trace.fraction != 1.0 )
	{
		float forward = 0, right = 0;
		vec3_t vpn, v_right;
		float flSpeed = MAX_CLIMB_SPEED;

		// they shouldn't be able to move faster than their maxspeed
		if ( flSpeed > pmove->clientmaxspeed )
		{
			flSpeed = pmove->clientmaxspeed;
		}

		AngleVectors( pmove->angles, vpn, v_right, NULL );

		if ( pmove->cmd.buttons & IN_BACK )
		{
			forward -= flSpeed;
		}
		if ( pmove->cmd.buttons & IN_FORWARD )
		{
			forward += flSpeed;
		}
		if ( pmove->cmd.buttons & IN_MOVELEFT )
		{
			right -= flSpeed;
		}
		if ( pmove->cmd.buttons & IN_MOVERIGHT )
		{
			right += flSpeed;
		}

		if ( pmove->cmd.buttons & IN_JUMP )
		{
			pmove->movetype = MOVETYPE_WALK;
			VectorScale( trace.plane.normal, 270, pmove->velocity );
		}
		else
		{
			if ( forward != 0 || right != 0 )
			{
				vec3_t velocity, perp, cross, lateral, tmp;
				float normal;

				//ALERT(at_console, "pev %.2f %.2f %.2f - ",
				//	pev->velocity.x, pev->velocity.y, pev->velocity.z);
				// Calculate player's intended velocity
				//Vector velocity = (forward * gpGlobals->v_forward) + (right * gpGlobals->v_right);
				VectorScale( vpn, forward, velocity );
				VectorMA( velocity, right, v_right, velocity );

				
				// Perpendicular in the ladder plane
	//					Vector perp = CrossProduct( Vector(0,0,1), trace.vecPlaneNormal );
	//					perp = perp.Normalize();
				VectorClear( tmp );
				tmp[2] = 1;
				CrossProduct( tmp, trace.plane.normal, perp );
				VectorNormalize( perp );


				// decompose velocity into ladder plane
				normal = DotProduct( velocity, trace.plane.normal );
				// This is the velocity into the face of the ladder
				VectorScale( trace.plane.normal, normal, cross );


				// This is the player's additional velocity
				VectorSubtract( velocity, cross, lateral );

				// This turns the velocity into the face of the ladder into velocity that
				// is roughly vertically perpendicular to the face of the ladder.
				// NOTE: It IS possible to face up and move down or face down and move up
				// because the velocity is a sum of the directional velocity and the converted
				// velocity through the face of the ladder -- by design.
				CrossProduct( trace.plane.normal, perp, tmp );
				VectorMA( lateral, -normal, tmp, pmove->velocity );
				if ( onFloor && normal > 0 )	// On ground moving away from the ladder
				{
					VectorMA( pmove->velocity, MAX_CLIMB_SPEED, trace.plane.normal, pmove->velocity );
				}
				//pev->velocity = lateral - (CrossProduct( trace.vecPlaneNormal, perp ) * normal);
			}
			else
			{
				float speed = Length( pmove->velocity );

				if ( speed < 1 )
				{
					VectorCopy( vec3_origin, pmove->velocity );
				}
				else if ( pmove->flFallVelocity >= PLAYER_FALL_PUNCH_THRESHHOLD )
				{
					VectorClear( pmove->velocity );
				}
				else
				{
					float friction = pmove->movevars->friction * LADDER_FRICTION_SCALE;
					float control = max( pmove->movevars->stopspeed, speed );
					float drop = control * friction * pmove->frametime;
					float newspeed = max( 0, speed - drop ) / speed;

					VectorScale( pmove->velocity, newspeed, pmove->velocity );
				}
			}
		}
	}
}

physent_t *CPlayerMove::CheckLadder( void )
{
	int			i;
	physent_t	*pe;
	hull_t		*hull;
	int			num;
	vec3_t		test;

	for ( i = 0; i < pmove->nummoveent; i++ )
	{
		pe = &pmove->moveents[i];
		
		if ( pe->model && (modtype_t)pmove->PM_GetModelType( pe->model ) == mod_brush && pe->skin == CONTENTS_LADDER )
		{

			hull = (hull_t *)pmove->PM_HullForBsp( pe, test );
			num = hull->firstclipnode;

			// Offset the test point appropriately for this hull.
			VectorSubtract ( pmove->origin, test, test);

			// Test the player's hull for intersection with this model
			if ( pmove->PM_HullPointContents (hull, num, test) == CONTENTS_EMPTY)
				continue;
			
			return pe;
		}
	}

	return NULL;
}



void CPlayerMove::WaterJump (void)
{
	if ( pmove->waterjumptime > 10000 )
	{
		pmove->waterjumptime = 10000;
	}

	if ( !pmove->waterjumptime )
		return;

	pmove->waterjumptime -= pmove->cmd.msec;
	if ( pmove->waterjumptime < 0 ||
		 !pmove->waterlevel )
	{
		pmove->waterjumptime = 0;
		pmove->flags &= ~FL_WATERJUMP;
	}

	pmove->velocity[0] = pmove->movedir[0];
	pmove->velocity[1] = pmove->movedir[1];
}

void CPlayerMove::PlayWaterSounds( void )
{
}

/*
============
AddGravity

============
*/
void CPlayerMove::AddGravity ()
{
	float	ent_gravity;

	if (pmove->gravity)
		ent_gravity = pmove->gravity;
	else
		ent_gravity = 1.0;

	// Add gravity incorrectly
	pmove->velocity[2] -= (ent_gravity * pmove->movevars->gravity * pmove->frametime );
	pmove->velocity[2] += pmove->basevelocity[2] * pmove->frametime;
	pmove->basevelocity[2] = 0;
	CheckVelocity();
}
/*
============
PushEntity

Does not change the entities velocity at all
============
*/
pmtrace_t CPlayerMove::PushEntity (vec3_t push)
{
	pmtrace_t	trace;
	vec3_t	end;
		
	VectorAdd (pmove->origin, push, end);

	trace = pmove->PM_PlayerTraceEx( pmove->origin, end, PM_NORMAL, pfnPlayerShouldBypassEntity );
	
	VectorCopy (trace.endpos, pmove->origin);

	// So we can run impact function afterwards.
	if (trace.fraction < 1.0 &&
		!trace.allsolid)
	{
		AddToTouched(trace, pmove->velocity);
	}

	return trace;
}	

/*
============
Physics_Toss()

Dead player flying through air., e.g.
============
*/
void CPlayerMove::Physics_Toss()
{
	pmtrace_t trace;
	vec3_t	move;
	float	backoff;

	CheckWater();

	if (pmove->velocity[2] > 0)
		pmove->onground = -1;

	// If on ground and not moving, return.
	if ( pmove->onground != -1 )
	{
		if (VectorCompare(pmove->basevelocity, vec3_origin) &&
		    VectorCompare(pmove->velocity, vec3_origin))
			return;
	}

	CheckVelocity ();

// add gravity
	if ( pmove->movetype != MOVETYPE_FLY &&
		 pmove->movetype != MOVETYPE_BOUNCEMISSILE &&
		 pmove->movetype != MOVETYPE_FLYMISSILE )
		AddGravity ();

// move origin
	// Base velocity is not properly accounted for since this entity will move again after the bounce without
	// taking it into account
	VectorAdd (pmove->velocity, pmove->basevelocity, pmove->velocity);
	
	CheckVelocity();
	VectorScale (pmove->velocity, pmove->frametime, move);
	VectorSubtract (pmove->velocity, pmove->basevelocity, pmove->velocity);

	trace = PushEntity (move);	// Should this clear basevelocity

	CheckVelocity();

	if (trace.allsolid)
	{	
		// entity is trapped in another solid
		pmove->onground = trace.ent;
		VectorCopy (vec3_origin, pmove->velocity);
		return;
	}
	
	if (trace.fraction == 1)
	{
		CheckWater();
		return;
	}


	if (pmove->movetype == MOVETYPE_BOUNCE)
		backoff = 2.0 - pmove->friction;
	else if (pmove->movetype == MOVETYPE_BOUNCEMISSILE)
		backoff = 2.0;
	else
		backoff = 1;

	ClipVelocity (pmove->velocity, trace.plane.normal, pmove->velocity, backoff);

	// stop if on ground
	if (trace.plane.normal[2] > 0.7)
	{		
		float vel;
		vec3_t base;

		VectorClear( base );
		if (pmove->velocity[2] < pmove->movevars->gravity * pmove->frametime)
		{
			// we're rolling on the ground, add static friction.
			pmove->onground = trace.ent;
			pmove->velocity[2] = 0;
		}

		vel = DotProduct( pmove->velocity, pmove->velocity );

		// Con_DPrintf("%f %f: %.0f %.0f %.0f\n", vel, trace.fraction, ent->velocity[0], ent->velocity[1], ent->velocity[2] );

		if (vel < (30 * 30) || (pmove->movetype != MOVETYPE_BOUNCE && pmove->movetype != MOVETYPE_BOUNCEMISSILE))
		{
			pmove->onground = trace.ent;
			VectorCopy (vec3_origin, pmove->velocity);
		}
		else
		{
			VectorScale (pmove->velocity, (1.0 - trace.fraction) * pmove->frametime * 0.9, move);
			trace = PushEntity (move);
		}
		VectorSubtract( pmove->velocity, base, pmove->velocity )
	}
	
// check for in water
	CheckWater();
}

/*
====================
NoClip

====================
*/
void CPlayerMove::NoClip()
{
	int			i;
	vec3_t		wishvel;
	float		fmove, smove;
//	float		currentspeed, addspeed, accelspeed;

	// Copy movement amounts
	fmove = pmove->cmd.forwardmove;
	smove = pmove->cmd.sidemove;
	
	VectorNormalize ( pmove->forward ); 
	VectorNormalize ( pmove->right );

	for (i=0 ; i<3 ; i++)       // Determine x and y parts of velocity
	{
		wishvel[i] = pmove->forward[i] * fmove + pmove->right[i] * smove
			+ pmove->up[i] * pmove->cmd.upmove;
	}

	VectorMA (pmove->origin, pmove->frametime, wishvel, pmove->origin);
	
	// Zero out the velocity so that we don't accumulate a huge downward velocity from
	//  gravity, etc.
	VectorClear( pmove->velocity );

}

/*
=============
Jump
=============
*/
void CPlayerMove::Jump (void)
{
	int i;
	qboolean tfc = false;

	qboolean cansuperjump = false;
	qboolean nolongjump = false;

	if (pmove->fuser4 >= 1)
		return;

	if (pmove->dead)
	{
		pmove->oldbuttons |= IN_JUMP ;	// don't jump again until released
		return;
	}

	tfc = atoi( pmove->PM_Info_ValueForKey( pmove->physinfo, "tfc" ) ) == 1 ? true : false;

	// Spy that's feigning death cannot jump
	if ( tfc && 
		( pmove->deadflag == ( DEAD_DISCARDBODY + 1 ) ) )
	{
		return;
	}

	// See if we are waterjumping.  If so, decrement count and return.
	if ( pmove->waterjumptime )
	{
		pmove->waterjumptime -= pmove->cmd.msec;
		if (pmove->waterjumptime < 0)
		{
			pmove->waterjumptime = 0;
		}
		return;
	}

	// If we are in the water most of the way...
	if (pmove->waterlevel >= 2)
	{	// swimming, not jumping
		pmove->onground = -1;

		if (pmove->watertype == CONTENTS_WATER)    // We move up a certain amount
			pmove->velocity[2] = 100;
		else if (pmove->watertype == CONTENTS_SLIME)
			pmove->velocity[2] = 80;
		else  // LAVA
			pmove->velocity[2] = 50;

		// play swiming sound
		if ( pmove->flSwimTime <= 0 )
		{
			// Don't play sound again for 1 second
			pmove->flSwimTime = 1000;
			switch ( pmove->RandomLong( 0, 3 ) )
			{ 
			case 0:
				PlaySoundFX( pmove->player_index, pmove->origin, CHAN_ITEM, (char *)"player/pl_wade1.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 1:
				PlaySoundFX( pmove->player_index, pmove->origin, CHAN_ITEM, (char *)"player/pl_wade2.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 2:
				PlaySoundFX( pmove->player_index, pmove->origin, CHAN_ITEM, (char *)"player/pl_wade3.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 3:
				PlaySoundFX( pmove->player_index, pmove->origin, CHAN_ITEM, (char *)"player/pl_wade4.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			}
		}

		return;
	}

	// No more effect
 	if ( pmove->onground == -1 )
	{
		// Flag that we jumped.
		// HACK HACK HACK
		// Remove this when the game .dll no longer does physics code!!!!
		pmove->oldbuttons |= IN_JUMP;	// don't jump again until released
		return;		// in air, so no effect
	}

	if ( pmove->oldbuttons & IN_JUMP )
		return;		// don't pogo stick

	// In the air now.
    pmove->onground = -1;

	if ( tfc )
	{
		PlaySoundFX( pmove->player_index, pmove->origin, CHAN_BODY, (char *)"player/plyrjmp8.wav", 0.5, ATTN_NORM, 0, PITCH_NORM );
	}
	else
	{
		UpdateStepSound(true);
	}

	// See if user can super long jump?
	cansuperjump = atoi( pmove->PM_Info_ValueForKey( pmove->physinfo, "slj" ) ) == 1 ? true : false;
	nolongjump = atoi(pmove->PM_Info_ValueForKey(pmove->physinfo, "nolj")) != 0;

	// Acclerate upward
	// If we are ducking...
	if ( ( pmove->bInDuck ) || ( pmove->flags & FL_DUCKING ) )
	{
		// Adjust for super long jump module
		// UNDONE -- note this should be based on forward angles, not current velocity.
		if ( cansuperjump && !nolongjump &&
			( pmove->cmd.buttons & IN_DUCK ) &&
			( pmove->flDuckTime > 0 ) &&
			Length( pmove->velocity ) > 50 )
		{
			pmove->punchangle[0] = -5;

			for (i =0; i < 2; i++)
			{
				pmove->velocity[i] = pmove->forward[i] * PLAYER_LONGJUMP_SPEED * 1.6;
			}
		
			pmove->velocity[2] = sqrt(2 * 800 * 56.0);
		}
		else
		{
			pmove->velocity[2] = sqrt(2 * 800 * 45.0);
		}
	}
	else
	{
		pmove->velocity[2] = sqrt(2 * 800 * 45.0);
	}

	// Decay it for simulation
	FinishGravity();

	// Flag that we jumped.
	pmove->oldbuttons |= IN_JUMP;	// don't jump again until released
}

/*
=============
CheckWaterJump
=============
*/
#define WJ_HEIGHT 8
void CPlayerMove::CheckWaterJump (void)
{
	vec3_t	vecStart, vecEnd;
	vec3_t	flatforward;
	vec3_t	flatvelocity;
	float curspeed;
	pmtrace_t tr;
	int		savehull;

	// Already water jumping.
	if ( pmove->waterjumptime )
		return;

	// Don't hop out if we just jumped in
	if ( pmove->velocity[2] < -180 )
		return; // only hop out if we are moving up

	// See if we are backing up
	flatvelocity[0] = pmove->velocity[0];
	flatvelocity[1] = pmove->velocity[1];
	flatvelocity[2] = 0;

	// Must be moving
	curspeed = VectorNormalize( flatvelocity );
	
	// see if near an edge
	flatforward[0] = pmove->forward[0];
	flatforward[1] = pmove->forward[1];
	flatforward[2] = 0;
	VectorNormalize (flatforward);

	// Are we backing into water from steps or something?  If so, don't pop forward
	if ( curspeed != 0.0 && ( DotProduct( flatvelocity, flatforward ) < 0.0 ) )
		return;

	VectorCopy( pmove->origin, vecStart );
	vecStart[2] += WJ_HEIGHT;

	VectorMA ( vecStart, 24, flatforward, vecEnd );
	
	// Trace, this trace should use the point sized collision hull
	savehull = pmove->usehull;
	pmove->usehull = 2;
	tr = pmove->PM_PlayerTraceEx( vecStart, vecEnd, PM_NORMAL, pfnPlayerShouldBypassEntity );
	if ( tr.fraction < 1.0 && fabs( tr.plane.normal[2] ) < 0.1f )  // Facing a near vertical wall?
	{
		vecStart[2] += pmove->player_maxs[ savehull ][2] - WJ_HEIGHT;
		VectorMA( vecStart, 24, flatforward, vecEnd );
		VectorMA( vec3_origin, -50, tr.plane.normal, pmove->movedir );

		tr = pmove->PM_PlayerTraceEx( vecStart, vecEnd, PM_NORMAL, pfnPlayerShouldBypassEntity );
		if ( tr.fraction == 1.0 )
		{
			pmove->waterjumptime = 2000;
			pmove->velocity[2] = 225;
			pmove->oldbuttons |= IN_JUMP;
			pmove->flags |= FL_WATERJUMP;
		}
	}

	// Reset the collision hull
	pmove->usehull = savehull;
}

void CPlayerMove::CheckFalling(void)
{
	if (pmove->onground != -1 && !pmove->dead &&
		pmove->flFallVelocity >= PLAYER_FALL_PUNCH_THRESHHOLD)
	{
		bool bLanded = true;

		if (pmove->waterlevel <= 0)
		{
			if (pmove->flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED)
			{
				PlaySoundFX(pmove->player_index, pmove->origin, CHAN_VOICE, (char *)"player/pl_fallpain3.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			}
			else if (pmove->flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED / 2)
			{
				if (atoi(pmove->PM_Info_ValueForKey(pmove->physinfo, "tfc")) == 1)
					PlaySoundFX(pmove->player_index, pmove->origin, CHAN_VOICE, (char *)"player/pl_fallpain3.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			}
			else if (pmove->flFallVelocity < PLAYER_MIN_BOUNCE_SPEED)
			{
				bLanded = false;
			}
		}

		if (bLanded)
		{
			pmove->flTimeStepSound = 0;
			UpdateStepSound();
			pmove->punchangle[PITCH] = pmove->flFallVelocity * 0.013;
		}
	}

	if (pmove->onground != -1)
		pmove->flFallVelocity = 0;
}

/*
===============
CalcRoll

===============
*/
float CPlayerMove::CalcRoll (vec3_t angles, vec3_t velocity, float rollangle, float rollspeed )
{
    float   sign;
    float   side;
    float   value;
	vec3_t  forward, right, up;
    
	AngleVectors (angles, forward, right, up);
    
	side = DotProduct (velocity, right);
    
	sign = side < 0 ? -1 : 1;
    
	side = fabs(side);
    
	value = rollangle;
    
	if (side < rollspeed)
	{
		side = side * value / rollspeed;
	}
    else
	{
		side = value;
	}
  
	return side * sign;
}

/*
=============
DropPunchAngle

=============
*/
void CPlayerMove::DropPunchAngle ( Vector &punchangle )
{
	float	len;
	
	len = VectorNormalize ( punchangle );
	len -= (10.0 + len * 0.5) * pmove->frametime;
	len = max( len, 0.0 );
	VectorScale ( punchangle, len, punchangle);
}

float RemapValClamped( float val, float A, float B, float C, float D )
{
	float cVal;

	if ( A == B )
		return val >= B ? D : C;

	cVal = ( val - A ) / ( B - A );

	if ( cVal < 0.0f )
		cVal = 0.0f;
	else if ( cVal > 1.0f )
		cVal = 1.0f;

	return C + ( D - C ) * cVal;
}

/*
==============
ScaleInput

==============
*/
void CPlayerMove::ScaleInput( float &forwardmove, float &sidemove, float &upmove )
{
	float spd;
	float maxspeed;

	maxspeed = pmove->clientmaxspeed;

	if ( maxspeed <= 0.0 )
	{
		forwardmove = 0;
		sidemove    = 0;
		upmove      = 0;
		return;
	}

	forwardmove = RemapValClamped( forwardmove, -CMD_MOVE_RANGE, CMD_MOVE_RANGE, -maxspeed, maxspeed );
	sidemove    = RemapValClamped( sidemove, -CMD_MOVE_RANGE, CMD_MOVE_RANGE, -maxspeed, maxspeed );
	upmove      = RemapValClamped( upmove, -CMD_MOVE_RANGE, CMD_MOVE_RANGE, -maxspeed, maxspeed );

	spd = sqrt( ( forwardmove * forwardmove ) + ( sidemove * sidemove ) + ( upmove * upmove ) );

	if ( spd > maxspeed )
	{
		float fRatio = maxspeed / spd;

		forwardmove *= fRatio;
		sidemove    *= fRatio;
		upmove      *= fRatio;
	}
}

/*
==============
CheckParameters

==============
*/
void CPlayerMove::CheckParameters( void )
{
	vec3_t	v_angle;

	ScaleInput( pmove->cmd.forwardmove, pmove->cmd.sidemove, pmove->cmd.upmove );

	if ( pmove->flags & FL_FROZEN ||
		 pmove->flags & FL_ONTRAIN || 
		 pmove->dead )
	{
		pmove->cmd.forwardmove = 0;
		pmove->cmd.sidemove    = 0;
		pmove->cmd.upmove      = 0;
	}


	DropPunchAngle( pmove->punchangle );

	// Take angles from command.
	if ( !pmove->dead )
	{
		VectorCopy ( pmove->cmd.viewangles, v_angle );         
		VectorAdd( v_angle, pmove->punchangle, v_angle );

		// Set up view angles.
		pmove->angles[ROLL]	=	CalcRoll ( v_angle, pmove->velocity, 0, 0 )*4;
		pmove->angles[PITCH] =	v_angle[PITCH];
		pmove->angles[YAW]   =	v_angle[YAW];
	}
	else
	{
		VectorCopy( pmove->oldangles, pmove->angles );
	}

	// Set dead player view_offset
	if ( pmove->dead )
	{
		if ( pmove->bInDuck )
		{
			UnDuck();
			pmove->bInDuck = false;
		}

		pmove->view_ofs[2] = PM_DEAD_VIEWHEIGHT;
	}

	// Adjust client view angles to match values used on server.
	if (pmove->angles[YAW] > 180.0f)
	{
		pmove->angles[YAW] -= 360.0f;
	}

}

void CPlayerMove::ReduceTimers( void )
{
	if ( pmove->flTimeStepSound > 0 )
	{
		pmove->flTimeStepSound -= pmove->cmd.msec;
		if ( pmove->flTimeStepSound < 0 )
		{
			pmove->flTimeStepSound = 0;
		}
	}
	if ( pmove->flDuckTime > 0 )
	{
		pmove->flDuckTime -= pmove->cmd.msec;
		if ( pmove->flDuckTime < 0 )
		{
			pmove->flDuckTime = 0;
		}
	}
	if ( pmove->flSwimTime > 0 )
	{
		pmove->flSwimTime -= pmove->cmd.msec;
		if ( pmove->flSwimTime < 0 )
		{
			pmove->flSwimTime = 0;
		}
	}
}

/*
=============
FullWalkMove

=============
*/
void CPlayerMove::FullWalkMove( void )
{
	physent_t *pLadder = m_pLadder;

	if ( pmove->flags & FL_FLY )
	{
		CheckWater();

		if ( pmove->cmd.buttons & IN_JUMP )
		{
			if ( !pLadder )
			{
				Jump ();
			}
		}
		else
		{
			pmove->oldbuttons &= ~IN_JUMP;
		}

		VectorAdd (pmove->velocity, pmove->basevelocity, pmove->velocity);
		FlyMove ();
		VectorSubtract (pmove->velocity, pmove->basevelocity, pmove->velocity);
		return;
	}

	if ( !InWater() )
	{
		StartGravity();
	}

	// If we are leaping out of the water, just update the counters.
	if ( pmove->waterjumptime )
	{
		WaterJump();
		FlyMove();

		// Make sure waterlevel is set correctly
		CheckWater();
		return;
	}

	// If we are swimming in the water, see if we are nudging against a place we can jump up out
	//  of, and, if so, start out jump.  Otherwise, if we are not moving up, then reset jump timer to 0
	if ( pmove->waterlevel >= 2 )
	{
		if ( pmove->waterlevel == 2 )
		{
			CheckWaterJump();
		}

		// If we are falling again, then we must not trying to jump out of water any more.
		if ( pmove->velocity[2] < 0 && pmove->waterjumptime )
		{
			pmove->waterjumptime = 0;
		}

		// Was jump button pressed?
		if (pmove->cmd.buttons & IN_JUMP)
		{
			Jump ();
		}
		else
		{
			pmove->oldbuttons &= ~IN_JUMP;
		}

		// Perform regular water movement
		WaterMove();

		VectorSubtract (pmove->velocity, pmove->basevelocity, pmove->velocity);

		// Get a final position
		CategorizePosition();
	}
	else

	// Not underwater
	{
		// Was jump button pressed?
		if ( pmove->cmd.buttons & IN_JUMP )
		{
			if ( !pLadder )
			{
				Jump ();
			}
		}
		else
		{
			pmove->oldbuttons &= ~IN_JUMP;
		}

		// Fricion is handled before we add in any base velocity. That way, if we are on a conveyor,
		//  we don't slow when standing still, relative to the conveyor.
		if ( pmove->onground != -1 )
		{
			pmove->velocity[2] = 0.0;
			Friction();
		}

		// Make sure velocity is valid.
		CheckVelocity();

		// Are we on ground now
		if ( pmove->onground != -1 )
		{
			WalkMove();
		}
		else
		{
			AirMove();  // Take into account movement when in air.
		}

		// Set final flags.
		CategorizePosition();

		// Now pull the base velocity back out.
		// Base velocity is set if you are on a moving object, like
		//  a conveyor (or maybe another monster?)
		VectorSubtract (pmove->velocity, pmove->basevelocity, pmove->velocity );

		// Make sure velocity is valid.
		CheckVelocity();

		// Add any remaining gravitational component.
		if ( !InWater() )
		{
			FinishGravity();
		}

		// If we are on ground, no downward velocity.
		if ( pmove->onground != -1 )
		{
			pmove->velocity[2] = 0;
		}

		// See if we landed on the ground with enough force to play
		//  a landing sound.
		CheckFalling();
	}

	PlayWaterSounds();
}

/*
=============
PlayerMove

Returns with origin, angles, and velocity modified in place.

Numtouch and touchindex[] will be set if any of the physents
were contacted during the move.
=============
*/
void CPlayerMove::PlayerMove ( bool server )
{
	m_pLadder = NULL;

#ifndef CLIENT_DLL
	rgOverlappingEntity[pmove->player_index] = 0;
#endif

	// Are we running server code?
	pmove->server = server;                

	// Adjust speeds etc.
	CheckParameters();

	// Assume we don't touch anything
	pmove->numtouch = 0;                    

	// # of msec to apply movement
	pmove->frametime = pmove->cmd.msec * 0.001;    

	ReduceTimers();

	// Convert view angles to vectors
	AngleVectors (pmove->angles, pmove->forward, pmove->right, pmove->up);

#ifdef CLIENT_DLL
	if (pmove->runfuncs)
	{
		iIsSpectator = 0;
		iHasNewViewAngles = 0;
		iHasNewViewOrigin = 0;
	}
#endif

	// Special handling for spectator and observers. (iuser1 is set if the player's in observer mode)
	if ( pmove->spectator || pmove->iuser1 > 0 )
	{
		SpectatorMove();
		CategorizePosition();
		return;
	}

	// Always try and unstick us unless we are in NOCLIP mode
	if ( pmove->movetype != MOVETYPE_NOCLIP && pmove->movetype != MOVETYPE_NONE )
	{
		if ( CheckStuck() )
		{
			return;  // Can't move, we're stuck
		}
	}

	// Now that we are "unstuck", see where we are ( waterlevel and type, pmove->onground ).
	CategorizePosition();

#ifndef CLIENT_DLL
	if ( server && rgOverlappingEntity[pmove->player_index] > 0 )
	{
		vec3_t direction;
		physent_t *entity = &pmove->physents[rgOverlappingEntity[pmove->player_index]];

		VectorSubtract( pmove->origin, entity->origin, direction );
		direction[2] = 0;
		VectorNormalize( direction );
		Accelerate( direction, 270, 270 );
	}
#endif

	// Store off the starting water level
	pmove->oldwaterlevel = pmove->waterlevel;

#ifdef CLIENT_DLL
	PM_UpdateWaterLevel_CLIENT( pmove->waterlevel );
#endif

	// If we are not on ground, store off how fast we are moving down
	if ( pmove->onground == -1 )
	{
		pmove->flFallVelocity = -pmove->velocity[2];
	}

	// Don't run ladder code if dead or on a train
	if ( !pmove->dead && !(pmove->flags & FL_ONTRAIN) )
	{
		m_pLadder = CheckLadder();
	}

	UpdateStepSound();

	Duck();
	
	if ( m_pLadder && !pmove->dead && !(pmove->flags & FL_ONTRAIN) )
	{
		LadderMove();
	}
	else if ( m_pLadder || pmove->movetype != MOVETYPE_NOCLIP )
	{
		pmove->movetype = MOVETYPE_WALK;
	}

	// Handle movement
	switch ( pmove->movetype )
	{
	default:
		pmove->Con_DPrintf("Bogus pmove player movetype %i on (%i) 0=cl 1=sv\n", pmove->movetype, pmove->server);
		break;

	case MOVETYPE_NONE:
		break;

	case MOVETYPE_NOCLIP:
		NoClip();
		break;

	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
		Physics_Toss();
		break;

	case MOVETYPE_FLY:
	
		CheckWater();

		// Was jump button pressed?
		// If so, set velocity to 270 away from ladder.  This is currently wrong.
		// Also, set MOVE_TYPE to walk, too.
		if ( pmove->cmd.buttons & IN_JUMP )
		{
			if ( !m_pLadder )
			{
				Jump ();
			}
		}
		else
		{
			pmove->oldbuttons &= ~IN_JUMP;
		}
		
		// Perform the move accounting for any base velocity.
		VectorAdd (pmove->velocity, pmove->basevelocity, pmove->velocity);
		FlyMove ();
		VectorSubtract (pmove->velocity, pmove->basevelocity, pmove->velocity);
		break;

	case MOVETYPE_WALK:
		FullWalkMove();
		break;
	}
}

void CPlayerMove::CreateStuckTable( void )
{
	float x, y, z;
	int idx;
	int i;
	float zi[3];

	memset(rgv3tStuckTable, 0, 54 * sizeof(vec3_t));

	idx = 0;
	// Little Moves.
	x = y = 0;
	// Z moves
	for (z = -0.125 ; z <= 0.125 ; z += 0.125)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		idx++;
	}
	x = z = 0;
	// Y moves
	for (y = -0.125 ; y <= 0.125 ; y += 0.125)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		idx++;
	}
	y = z = 0;
	// X moves
	for (x = -0.125 ; x <= 0.125 ; x += 0.125)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		idx++;
	}

	// Remaining multi axis nudges.
	for ( x = - 0.125; x <= 0.125; x += 0.250 )
	{
		for ( y = - 0.125; y <= 0.125; y += 0.250 )
		{
			for ( z = - 0.125; z <= 0.125; z += 0.250 )
			{
				rgv3tStuckTable[idx][0] = x;
				rgv3tStuckTable[idx][1] = y;
				rgv3tStuckTable[idx][2] = z;
				idx++;
			}
		}
	}

	// Big Moves.
	x = y = 0;
	zi[0] = 0.0f;
	zi[1] = 1.0f;
	zi[2] = 6.0f;

	for (i = 0; i < 3; i++)
	{
		// Z moves
		z = zi[i];
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		idx++;
	}

	x = z = 0;

	// Y moves
	for (y = -2.0f ; y <= 2.0f ; y += 2.0)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		idx++;
	}
	y = z = 0;
	// X moves
	for (x = -2.0f ; x <= 2.0f ; x += 2.0f)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		idx++;
	}

	// Remaining multi axis nudges.
	for (i = 0 ; i < 3; i++)
	{
		z = zi[i];
		
		for (x = -2.0f ; x <= 2.0f ; x += 2.0f)
		{
			for (y = -2.0f ; y <= 2.0f ; y += 2.0)
			{
				rgv3tStuckTable[idx][0] = x;
				rgv3tStuckTable[idx][1] = y;
				rgv3tStuckTable[idx][2] = z;
				idx++;
			}
		}
	}
}



/*
This modume implements the shared player physics code between any particular game and 
the engine.  The same PM_Move routine is built into the game .dll and the client .dll and is
invoked by each side as appropriate.  There should be no distinction, internally, between server
and client.  This will ensure that prediction behaves appropriately.
*/

void CPlayerMove::Move ( playermove_t *ppmove, bool server )
{
	assert( m_bInitialized );

	pmove = ppmove;

	PlayerMove( server );

	if ( pmove->onground != -1 )
	{
		pmove->flags |= FL_ONGROUND;
	}
	else
	{
		pmove->flags &= ~FL_ONGROUND;
	}

	if (pmove->movetype == MOVETYPE_WALK)
	{
		pmove->friction = 1.0f;
	}
}

int CPlayerMove::GetVisEntInfo( int ent )
{
	if ( ent >= 0 && ent <= pmove->numvisent )
	{
		return pmove->visents[ ent ].info;
	}
	return -1;
}

int CPlayerMove::GetPhysEntInfo( int ent )
{
	if ( ent >= 0 && ent <= pmove->numphysent)
	{
		return pmove->physents[ ent ].info;
	}
	return -1;
}

playermove_t *CPlayerMove::GetCurrentPMove( void )
{
	return pmove;
}

void CPlayerMove::Init( playermove_t *ppmove )
{
	assert( !m_bInitialized );

	pmove = ppmove;

	CreateStuckTable();

	m_bInitialized = true;
}

void PM_Move ( playermove_t *ppmove, int server )
{
	pmove = ppmove;
	g_Pmove.Move( ppmove, server != 0 );
}

int PM_GetInfo( int ent )
{
	return g_Pmove.GetVisEntInfo( ent );
}

playermove_t *PM_GetPlayerMove( void )
{
	return g_Pmove.GetCurrentPMove();
}

void PM_Init( playermove_t *ppmove )
{
	pmove = ppmove;
	g_Pmove.Init( ppmove );
}
