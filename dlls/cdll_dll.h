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
//  cdll_dll.h

// this file is included by both the game-dll and the client-dll,

#ifndef CDLL_DLL_H
#define CDLL_DLL_H

#define MAX_WEAPONS		256

#define MAX_WEAPON_SLOTS		10	// hud item selection slots
#ifdef CLIENT_DLL
#define MAX_ITEM_TYPES			6	// hud item selection slots
#else
#define MAX_ITEM_TYPES			10
#endif

#define MAX_ITEMS				6	// hard coded item types

#define	HIDEHUD_WEAPONS		( 1<<0 )
#define	HIDEHUD_FLASHLIGHT	( 1<<1 )
#define	HIDEHUD_ALL			( 1<<2 )
#define 	HIDEHUD_HEALTH		( 1<<3 )
#define HIDEHUD_WEAPONSELECT	( 1<<4 )
#define HIDEHUD_ARMOR		( 1<<5 )
#define HIDEHUD_CUSTOM_MAPS		( 1<<6 )
#define HIDEHUD_CUSTOM_PLUGINS	( 1<<7 )
#define HIDEHUD_CROSSHAIR		( 1<<8 )
#define HIDEHUD_AMMO			( 1<<9 )

#define	MAX_AMMO_TYPES	64
#define MAX_AMMO_SLOTS	64

#define HUD_PRINTNOTIFY		1
#define HUD_PRINTCONSOLE	2
#define HUD_PRINTTALK		3
#define HUD_PRINTCENTER		4

// Custom HUD elements, matching the AngelScript HUD_* enums
#define HUD_EFFECT_NONE			0
#define HUD_EFFECT_RAMP_UP		1
#define HUD_EFFECT_RAMP_DOWN	2
#define HUD_EFFECT_TRIANGLE		3
#define HUD_EFFECT_COSINE_UP	4
#define HUD_EFFECT_COSINE_DOWN	5
#define HUD_EFFECT_COSINE		6
#define HUD_EFFECT_TOGGLE		7
#define HUD_EFFECT_SINE_PULSE	8

#define HUD_ELEM_ABSOLUTE_X		( 1<<0 )
#define HUD_ELEM_ABSOLUTE_Y		( 1<<1 )
#define HUD_ELEM_SCR_CENTER_X	( 1<<2 )
#define HUD_ELEM_SCR_CENTER_Y	( 1<<3 )
#define HUD_ELEM_NO_BORDER		( 1<<4 )
#define HUD_ELEM_HIDDEN			( 1<<5 )
#define HUD_ELEM_EFFECT_ONCE	( 1<<6 )
#define HUD_ELEM_DEFAULT_ALPHA	( 1<<7 )
#define HUD_ELEM_DYNAMIC_ALPHA	( 1<<8 )

#define HUD_SPR_OPAQUE				( 1<<16 )
#define HUD_SPR_MASKED				( 1<<17 )
#define HUD_SPR_PLAY_ONCE			( 1<<18 )
#define HUD_SPR_HIDE_WHEN_STOPPED	( 1<<19 )

#define HUD_NUM_RIGHT_ALIGN			( 1<<16 )
#define HUD_NUM_SEPARATOR			( 1<<17 )
#define HUD_NUM_DONT_DRAW_ZERO		( 1<<18 )
#define HUD_NUM_LEADING_ZEROS		( 1<<19 )
#define HUD_NUM_NEGATIVE_NUMBERS	( 1<<20 )
#define HUD_NUM_PLUS_SIGN			( 1<<21 )

#define HUD_TIME_RIGHT_ALIGN	( 1<<16 )
#define HUD_TIME_HOURS			( 1<<17 )
#define HUD_TIME_MINUTES		( 1<<18 )
#define HUD_TIME_SECONDS		( 1<<19 )
#define HUD_TIME_MILLISECONDS	( 1<<20 )
#define HUD_TIME_ZERO_HOURS		( 1<<21 )
#define HUD_TIME_FREEZE			( 1<<22 )
#define HUD_TIME_COUNT_DOWN		( 1<<23 )


#define WEAPON_SUIT			31

#endif
