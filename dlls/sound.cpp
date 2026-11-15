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
//=========================================================
// sound.cpp 
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "talkmonster.h"
#include "gamerules.h"
#include "server_sound.h"

#define SF_ENVSOUND_START_OFF 1
#define SF_SENTENCE_ACTIVATOR_ONLY 1

#if !defined ( _WIN32 )
#include <ctype.h>
#endif


static char *memfgets( byte *pMemFile, int fileSize, int &filePos, char *pBuffer, int bufferSize );


const char *FindSoundReplacementSample( CBaseEntity *entity, const char *sample )
{
	if ( !entity )
		return sample;

	if ( !entity->m_iszSoundList )
	{
		if ( !g_szSoundReplacement )
			return sample;

		entity = CWorld::GetInstance();

		if ( !entity )
			return sample;
	}

	return entity->SOUNDREPLACEMENT_Find( sample );
}

const char *FindSoundReplacementSample( edict_t *entity, const char *sample )
{
	if ( !entity || entity->free )
		return sample;

	return FindSoundReplacementSample( (CBaseEntity *) entity->pvPrivateData, sample );
}

void PrecacheSound_Intercept( CBaseEntity *entity, const char *sample )
{
	if ( entity && entity->m_iszSoundList )
	{
		if ( entity->SOUNDREPLACEMENT_Find( sample ) != sample )
			return;
	}
	else if ( g_szSoundReplacement )
	{
		if ( CWorld::GetInstance()->SOUNDREPLACEMENT_Find( sample ) != sample )
			return;
	}

	CServer_SoundEngine::GetSoundEngine()->PrecacheSound( sample );
}


// ==================== GENERIC AMBIENT SOUND ======================================

// runtime pitch shift and volume fadein/out structure

// NOTE: IF YOU CHANGE THIS STRUCT YOU MUST CHANGE THE SAVE/RESTORE VERSION NUMBER
// SEE BELOW (in the typedescription for the class)
typedef struct dynpitchvol
{
	// NOTE: do not change the order of these parameters 
	// NOTE: unless you also change order of rgdpvpreset array elements!
	int preset;

	int pitchrun;		// pitch shift % when sound is running 0 - 255
	int pitchstart;		// pitch shift % when sound stops or starts 0 - 255
	int spinup;			// spinup time 0 - 100
	int spindown;		// spindown time 0 - 100

	int volrun;			// volume change % when sound is running 0 - 10
	int volstart;		// volume change % when sound stops or starts 0 - 10
	int fadein;			// volume fade in time 0 - 100
	int fadeout;		// volume fade out time 0 - 100

						// Low Frequency Oscillator
	int	lfotype;		// 0) off 1) square 2) triangle 3) random 4) sawtooth 5) sine
	int lforate;		// 0 - 1000, how fast lfo osciallates
	
	int lfomodpitch;	// 0-100 mod of current pitch. 0 is off.
	int lfomodvol;		// 0-100 mod of current volume. 0 is off.

	int cspinup;		// each trigger hit increments counter and spinup pitch


	int	cspincount;

	int pitch;			
	int spinupsav;
	int spindownsav;
	int pitchfrac;

	int vol;
	int fadeinsav;
	int fadeoutsav;
	int volfrac;

	int	lfofrac;
	int	lfomult;


} dynpitchvol_t;

#define CDPVPRESETMAX 27

// presets for runtime pitch and vol modulation of ambient sounds

dynpitchvol_t rgdpvpreset[CDPVPRESETMAX] = 
{
// pitch	pstart	spinup	spindwn	volrun	volstrt	fadein	fadeout	lfotype	lforate	modptch modvol	cspnup		
{1,	255,	 75,	95,		95,		10,		1,		50,		95, 	0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0}, 
{2,	255,	 85,	70,		88,		10,		1,		20,		88,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0}, 
{3,	255,	100,	50,		75,		10,		1,		10,		75,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
{4,	100,	100,	0,		0,		10,		1,		90,		90,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
{5,	100,	100,	0,		0,		10,		1,		80,		80,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
{6,	100,	100,	0,		0,		10,		1,		50,		70,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
{7,	100,	100,	0,		0,		 5,		1,		40,		50,		1,		50,		0,		10,		0,		0,0,0,0,0,0,0,0,0,0},
{8,	100,	100,	0,		0,		 5,		1,		40,		50,		1,		150,	0,		10,		0,		0,0,0,0,0,0,0,0,0,0},
{9,	100,	100,	0,		0,		 5,		1,		40,		50,		1,		750,	0,		10,		0,		0,0,0,0,0,0,0,0,0,0},
{10,128,	100,	50,		75,		10,		1,		30,		40,		2,		 8,		20,		0,		0,		0,0,0,0,0,0,0,0,0,0},
{11,128,	100,	50,		75,		10,		1,		30,		40,		2,		25,		20,		0,		0,		0,0,0,0,0,0,0,0,0,0},
{12,128,	100,	50,		75,		10,		1,		30,		40,		2,		70,		20,		0,		0,		0,0,0,0,0,0,0,0,0,0},
{13,50,		 50,	0,		0,		10,		1,		20,		50,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
{14,70,		 70,	0,		0,		10,		1,		20,		50,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
{15,90,		 90,	0,		0,		10,		1,		20,		50,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
{16,120,	120,	0,		0,		10,		1,		20,		50,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
{17,180,	180,	0,		0,		10,		1,		20,		50,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
{18,255,	255,	0,		0,		10,		1,		20,		50,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
{19,200,	 75,	90,		90,		10,		1,		50,		90,		2,		100,	20,		0,		0,		0,0,0,0,0,0,0,0,0,0},
{20,255,	 75,	97,		90,		10,		1,		50,		90,		1,		40,		50,		0,		0,		0,0,0,0,0,0,0,0,0,0},
{21,100,	100,	0,		0,		10,		1,		30,		50,		3,		15,		20,		0,		0,		0,0,0,0,0,0,0,0,0,0},
{22,160,	160,	0,		0,		10,		1,		50,		50,		3,		500,	25,		0,		0,		0,0,0,0,0,0,0,0,0,0},
{23,255,	 75,	88,		0,		10,		1,		40,		0,		0,		0,		0,		0,		5,		0,0,0,0,0,0,0,0,0,0}, 
{24,200,	 20,	95,	    70,		10,		1,		70,		70,		3,		20,		50,		0,		0,		0,0,0,0,0,0,0,0,0,0}, 
{25,180,	100,	50,		60,		10,		1,		40,		60,		2,		90,		100,	100,	0,		0,0,0,0,0,0,0,0,0,0}, 
{26,60,		 60,	0,		0,		10,		1,		40,		70,		3,		80,		20,		50,		0,		0,0,0,0,0,0,0,0,0,0}, 
{27,128,	 90,	10,		10,		10,		1,		20,		40,		1,		5,		10,		20,		0,		0,0,0,0,0,0,0,0,0,0}
};

static const int g_iAmbientLFOSine[512] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
	1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7,
	7, 8, 9, 9, 10, 10, 11, 12, 12, 13, 14, 14, 15, 16, 17, 18,
	18, 19, 20, 21, 22, 23, 24, 24, 25, 26, 27, 28, 29, 30, 31, 32,
	33, 35, 36, 37, 38, 39, 40, 41, 42, 44, 45, 46, 47, 48, 50, 51,
	52, 53, 55, 56, 57, 59, 60, 61, 63, 64, 65, 67, 68, 70, 71, 72,
	74, 75, 77, 78, 80, 81, 83, 84, 85, 87, 88, 90, 91, 93, 95, 96,
	98, 99, 101, 102, 104, 105, 107, 108, 110, 111, 113, 115, 116, 118, 119, 121,
	122, 124, 126, 127, 129, 130, 132, 133, 135, 137, 138, 140, 141, 143, 144, 146,
	147, 149, 151, 152, 154, 155, 157, 158, 160, 161, 163, 164, 166, 167, 169, 170,
	172, 173, 175, 176, 178, 179, 180, 182, 183, 185, 186, 188, 189, 190, 192, 193,
	194, 196, 197, 198, 200, 201, 202, 203, 205, 206, 207, 208, 210, 211, 212, 213,
	214, 215, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230,
	231, 232, 233, 234, 235, 235, 236, 237, 238, 239, 239, 240, 241, 242, 242, 243,
	244, 244, 245, 246, 246, 247, 247, 248, 248, 249, 249, 250, 250, 251, 251, 252,
	252, 252, 253, 253, 253, 254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 254, 254,
	254, 254, 253, 253, 253, 252, 252, 252, 251, 251, 251, 250, 250, 249, 249, 248,
	248, 247, 246, 246, 245, 245, 244, 243, 243, 242, 241, 241, 240, 239, 238, 237,
	237, 236, 235, 234, 233, 232, 231, 231, 230, 229, 228, 227, 226, 225, 224, 223,
	222, 220, 219, 218, 217, 216, 215, 214, 213, 211, 210, 209, 208, 207, 205, 204,
	203, 202, 200, 199, 198, 196, 195, 194, 192, 191, 190, 188, 187, 185, 184, 183,
	181, 180, 178, 177, 175, 174, 172, 171, 170, 168, 167, 165, 164, 162, 160, 159,
	157, 156, 154, 153, 151, 150, 148, 147, 145, 144, 142, 140, 139, 137, 136, 134,
	133, 131, 129, 128, 126, 125, 123, 122, 120, 118, 117, 115, 114, 112, 111, 109,
	108, 106, 104, 103, 101, 100, 98, 97, 95, 94, 92, 91, 89, 88, 86, 85,
	83, 82, 80, 79, 77, 76, 75, 73, 72, 70, 69, 67, 66, 65, 63, 62,
	61, 59, 58, 57, 55, 54, 53, 52, 50, 49, 48, 47, 45, 44, 43, 42,
	41, 40, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25,
	24, 23, 22, 21, 20, 20, 19, 18, 17, 16, 16, 15, 14, 13, 13, 12,
	11, 11, 10, 9, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 4, 3,
	3, 3, 2, 2, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
};

float g_flNextAmbientRefresh = 0;

enum
{
	AMBIENT_SOUND_ACTIVATOR_ONLY = 64,
	AMBIENT_PLAYMODE_FORCE_ONCE = 1,
	AMBIENT_PLAYMODE_FORCE_LOOP = 2,
	AMBIENT_PLAYMODE_LINEAR = 4,
	AMBIENT_REFRESH = 0x200,
	LFO_SAWTOOTH = 4,
	LFO_SINE = 5
};

class CAmbientGeneric : public CBaseEntity
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );
	void Precache( void );
	void EXPORT ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT RampThink( void );
	void InitModulationParms( void );
	void UpdateOnRemove() override;
	void LinearSetup();
	void Refresh();
	void PlaySoundAtClient( CBaseEntity *activator, char *sample, float volume, float attenuation, int flags, int pitch );

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	virtual int ObjectCaps( void ) { return 0; }

	float         m_flAttenuation;
	dynpitchvol_t m_dpv;

	bool m_fActive;
	bool m_fLooping;
	bool m_fRefreshPending;

	int m_iPlayMode;
	int m_iLinearMin;
	int m_iLinearMax;
};

LINK_ENTITY_TO_CLASS( ambient_generic, CAmbientGeneric );
TYPEDESCRIPTION CAmbientGeneric::m_SaveData[] =
{
	DEFINE_FIELD( CAmbientGeneric, m_flAttenuation, FIELD_FLOAT ),
	DEFINE_FIELD( CAmbientGeneric, m_fActive, FIELD_CHARACTER ),
	DEFINE_FIELD( CAmbientGeneric, m_fLooping, FIELD_CHARACTER ),

	// HACKHACK - This is not really in the spirit of the save/restore design, but save this
	// out as a binary data block.  If the dynpitchvol_t is changed, old saved games will NOT
	// load these correctly, so bump the save/restore version if you change the size of the struct
	// The right way to do this is to split the input parms (read in keyvalue) into members and re-init this
	// struct in Precache(), but it's unlikely that the struct will change, so it's not worth the time right now.
	DEFINE_ARRAY( CAmbientGeneric, m_dpv, FIELD_CHARACTER, sizeof( dynpitchvol_t ) ),
};

IMPLEMENT_SAVERESTORE( CAmbientGeneric, CBaseEntity );

void CAmbientGeneric::UpdateOnRemove()
{
	Use( this, this, USE_OFF, 0 );
	CBaseEntity::UpdateOnRemove();
}

//
// ambient_generic - general-purpose user-defined static sound
//
void CAmbientGeneric :: Spawn()
{
/*
		-1 : "Default"
		0  : "Everywhere"
		200 : "Small Radius"
		125 : "Medium Radius"
		80  : "Large Radius"
*/

	if ( pev->spawnflags & AMBIENT_SOUND_EVERYWHERE )
		m_flAttenuation = ATTN_NONE;
	else if ( pev->spawnflags & AMBIENT_SOUND_SMALLRADIUS )
		m_flAttenuation = ATTN_IDLE;
	else if ( pev->spawnflags & AMBIENT_SOUND_MEDIUMRADIUS )
		m_flAttenuation = ATTN_STATIC;
	else if ( pev->spawnflags & AMBIENT_SOUND_LARGERADIUS )
		m_flAttenuation = ATTN_NORM;
	else
		m_flAttenuation = ATTN_STATIC;

	if ( m_iPlayMode & AMBIENT_PLAYMODE_LINEAR )
		LinearSetup();

	if ( FStringNull( pev->message ) || !strlen( STRING( pev->message ) ) )
	{
		ALERT( at_error, "Empty ambient generic '%s' at %.1f %.1f %.1f\n", STRING( pev->targetname ), pev->origin.x, pev->origin.y, pev->origin.z );
		pev->nextthink = gpGlobals->time + 0.1;
		SetThink( &CAmbientGeneric::SUB_Remove );
		return;
	}
    pev->solid		= SOLID_NOT;
    pev->movetype	= MOVETYPE_NONE;

	// Set up think function for dynamic modification
	// of ambient sound's pitch or volume. Don't
	// start thinking yet.

	SetThink(&CAmbientGeneric::RampThink);
	pev->nextthink = 0;

	// allow on/off switching via 'use' function.

	SetUse ( &CAmbientGeneric::ToggleUse );

	m_fActive = FALSE;
	m_fRefreshPending = false;
	m_fLooping = !( pev->spawnflags & AMBIENT_SOUND_NOT_LOOPING );

	if ( m_iPlayMode & AMBIENT_PLAYMODE_FORCE_ONCE )
		m_fLooping = FALSE;
	else if ( m_iPlayMode & AMBIENT_PLAYMODE_FORCE_LOOP )
		m_fLooping = TRUE;
	Precache( );
}

void CAmbientGeneric :: Precache()
{
	char* szSoundFile = (char*) STRING(pev->message);

	if ( !FStringNull( pev->message ) && strlen( szSoundFile ) > 1 && *szSoundFile != '!')
	{
		if ( !m_iPlayMode && g_iMapForGameVersion >= 401 && g_iMapForGameVersion <= 449 &&
		     !( pev->spawnflags & AMBIENT_SOUND_NOT_LOOPING ) )
		{
			const char *extension = strrchr( szSoundFile, '.' );

			if ( extension && strlen( extension ) >= 4 && strnicmp( extension, ".wav", 4 ) )
			{
				m_iPlayMode = AMBIENT_PLAYMODE_FORCE_LOOP;
				m_fLooping = true;
			}
		}

		PrecacheSound_Intercept( this, szSoundFile);
	}
	// init all dynamic modulation parms
	InitModulationParms();

	if ( !(pev->spawnflags & ( AMBIENT_SOUND_START_SILENT | AMBIENT_SOUND_ACTIVATOR_ONLY ) ) && m_fLooping)
			m_fActive = TRUE;

	if ( m_fActive )
	{
		int flags = SND_SPAWNING;

		if ( m_iPlayMode & AMBIENT_PLAYMODE_LINEAR )
			flags |= SOUNDMSG_RADIUS;

		if ( m_iPlayMode & AMBIENT_PLAYMODE_FORCE_LOOP )
			flags |= SOUNDMSG_FORCE_LOOP;

		UTIL_EmitAmbientSound( edict(), pev->origin, szSoundFile, m_dpv.vol * 0.01, m_flAttenuation, flags, m_dpv.pitch );
		float delay = RANDOM_FLOAT( 1.1f, 4.0f );

		pev->nextthink = gpGlobals->time + delay;
	}
}

// RampThink - Think at 5hz if we are dynamically modifying
// pitch or volume of the playing sound.  This function will
// ramp pitch and/or volume up or down, modify pitch/volume
// with lfo if active.

void CAmbientGeneric :: RampThink( void )
{
	char* szSoundFile = (char*) STRING(pev->message);
	int pitch = m_dpv.pitch;
	int vol = m_dpv.vol;
	int flags = 0;
	int fChanged = 0;		// FALSE if pitch and vol remain unchanged this round
	int	prev;

	if (!m_dpv.spinup && !m_dpv.spindown && !m_dpv.fadein && !m_dpv.fadeout && !m_dpv.lfotype)
	{
		Refresh();
		return;
	}

	// ==============
	// pitch envelope
	// ==============
	if (m_dpv.spinup || m_dpv.spindown)
	{
		prev = m_dpv.pitchfrac >> 8;

		if (m_dpv.spinup > 0)
			m_dpv.pitchfrac += m_dpv.spinup;
		else if (m_dpv.spindown > 0)
			m_dpv.pitchfrac -= m_dpv.spindown;

		pitch = m_dpv.pitchfrac >> 8;

		if (pitch > m_dpv.pitchrun)
		{
			pitch = m_dpv.pitchrun;
			m_dpv.spinup = 0;				// done with ramp up
		}

		if (pitch < m_dpv.pitchstart)
		{
			m_dpv.spindown = 0;				// done with ramp down

			// shut sound off
			UTIL_EmitAmbientSound(ENT(pev), pev->origin, szSoundFile,
					0, 0, SND_STOP, 0);

			// return without setting nextthink
			return;
		}

		if (pitch > 255) pitch = 255;
		if (pitch < 1) pitch = 1;

		m_dpv.pitch = pitch;

		fChanged |= (prev != pitch);
		flags |= SND_CHANGE_PITCH;
	}

	// ==================
	// amplitude envelope
	// ==================
	if (m_dpv.fadein || m_dpv.fadeout)
	{
		prev = m_dpv.volfrac >> 8;

		if (m_dpv.fadein > 0)
			m_dpv.volfrac += m_dpv.fadein;
		else if (m_dpv.fadeout > 0)
			m_dpv.volfrac -= m_dpv.fadeout;

		vol = m_dpv.volfrac >> 8;

		if (vol > m_dpv.volrun)
		{
			vol = m_dpv.volrun;
			m_dpv.fadein = 0;				// done with ramp up
		}

		if (vol < m_dpv.volstart)
		{
			m_dpv.fadeout = 0;				// done with ramp down

			// shut sound off
			UTIL_EmitAmbientSound(ENT(pev), pev->origin, szSoundFile,
					0, 0, SND_STOP, 0);

			// return without setting nextthink
			return;
		}

		if (vol > 100) vol = 100;
		if (vol < 1) vol = 1;

		m_dpv.vol = vol;

		fChanged |= (prev != vol);
		flags |= SND_CHANGE_VOL;
	}

	// ===================
	// pitch/amplitude LFO
	// ===================
	if (m_dpv.lfotype)
	{
		int pos;

		if (m_dpv.lfofrac > 0x6fffffff)
			m_dpv.lfofrac = 0;

		// update lfo, lfofrac/255 makes a triangle wave 0-255
		m_dpv.lfofrac += m_dpv.lforate;
		pos = m_dpv.lfofrac >> 8;

		if ( m_dpv.lfotype == LFO_SAWTOOTH || m_dpv.lfotype == LFO_SINE )
		{
			if ( pos > 511 )
			{
				m_dpv.lfofrac = 0;
				m_dpv.lfomult = 0;
			}
			else if ( m_dpv.lfotype == LFO_SAWTOOTH )
				m_dpv.lfomult = m_dpv.lfofrac >> 9;
			else
				m_dpv.lfomult = g_iAmbientLFOSine[pos];
		}
		else
		{
			if (m_dpv.lfofrac < 0)
		{
			m_dpv.lfofrac = 0;
			m_dpv.lforate = abs(m_dpv.lforate);
			pos = 0;
		}
		else if (pos > 255)
		{
			pos = 255;
			m_dpv.lfofrac = (255 << 8);
			m_dpv.lforate = -abs(m_dpv.lforate);
		}

		switch(m_dpv.lfotype)
		{
		case LFO_SQUARE:
			if (pos < 128)
				m_dpv.lfomult = 255;
			else
				m_dpv.lfomult = 0;

			break;
		case LFO_RANDOM:
			if (pos == 255)
				m_dpv.lfomult = RANDOM_LONG(0, 255);
			break;
		case LFO_TRIANGLE:
		default:
			m_dpv.lfomult = pos;
			break;
			}
		}

		if (m_dpv.lfomodpitch)
		{
			prev = pitch;

			// pitch 0-255
			pitch += ((m_dpv.lfomult - 128) * m_dpv.lfomodpitch) / 100;

			if (pitch > 255) pitch = 255;
			if (pitch < 1) pitch = 1;


			fChanged |= (prev != pitch);
			flags |= SND_CHANGE_PITCH;
		}

		if (m_dpv.lfomodvol)
		{
			// vol 0-100
			prev = vol;

			vol += ((m_dpv.lfomult - 128) * m_dpv.lfomodvol) / 100;

			if (vol > 100) vol = 100;
			if (vol < 0) vol = 0;

			fChanged |= (prev != vol);
			flags |= SND_CHANGE_VOL;
		}

	}

	// Send update to playing sound only if we actually changed
	// pitch or volume in this routine.

	if (flags && fChanged)
	{
		if (pitch == PITCH_NORM)
			pitch = PITCH_NORM + 1; // don't send 'no pitch' !

		if ( m_iPlayMode & AMBIENT_PLAYMODE_FORCE_ONCE )
			flags |= SOUNDMSG_FORCE_ONCE;
		else if ( m_iPlayMode & AMBIENT_PLAYMODE_FORCE_LOOP )
			flags |= SOUNDMSG_FORCE_LOOP;

		if ( m_iPlayMode & AMBIENT_PLAYMODE_LINEAR )
			flags |= SOUNDMSG_RADIUS;

		UTIL_EmitAmbientSound(ENT(pev), pev->origin, szSoundFile,
		                       vol * 0.01, m_flAttenuation, flags, pitch);
	}

	// update ramps at 5hz
	pev->nextthink = gpGlobals->time + 0.2;
	return;
}

// Init all ramp params in preparation to
// play a new sound

void CAmbientGeneric :: InitModulationParms(void)
{
	int pitchinc;

	m_dpv.volrun = pev->health * 10;	// 0 - 100
	if (m_dpv.volrun > 100) m_dpv.volrun = 100;
	if (m_dpv.volrun < 0) m_dpv.volrun = 0;

	// get presets
	if (m_dpv.preset != 0 && m_dpv.preset <= CDPVPRESETMAX)
	{
		// load preset values
		m_dpv = rgdpvpreset[m_dpv.preset - 1];

		// fixup preset values, just like
		// fixups in KeyValue routine.
		if (m_dpv.spindown > 0)
			m_dpv.spindown = (101 - m_dpv.spindown) * 64;
		if (m_dpv.spinup > 0)
			m_dpv.spinup = (101 - m_dpv.spinup) * 64;

		m_dpv.volstart *= 10;
		m_dpv.volrun *= 10;

		if (m_dpv.fadein > 0)
			m_dpv.fadein = (101 - m_dpv.fadein) * 64;
		if (m_dpv.fadeout > 0)
			m_dpv.fadeout = (101 - m_dpv.fadeout) * 64;

		m_dpv.lforate *= 256;

		m_dpv.fadeinsav = m_dpv.fadein;
		m_dpv.fadeoutsav = m_dpv.fadeout;
		m_dpv.spinupsav = m_dpv.spinup;
		m_dpv.spindownsav = m_dpv.spindown;
	}

	m_dpv.fadein = m_dpv.fadeinsav;
	m_dpv.fadeout = 0; 
	
	if (m_dpv.fadein)
		m_dpv.vol = m_dpv.volstart;
	else
		m_dpv.vol = m_dpv.volrun;

	m_dpv.spinup = m_dpv.spinupsav;
	m_dpv.spindown = 0; 

	if (m_dpv.spinup)
		m_dpv.pitch = m_dpv.pitchstart;
	else
		m_dpv.pitch = m_dpv.pitchrun;

	if (m_dpv.pitch == 0)
		m_dpv.pitch = PITCH_NORM;

	m_dpv.pitchfrac = m_dpv.pitch << 8;
	m_dpv.volfrac = m_dpv.vol << 8;

	m_dpv.lfofrac = 0;
	m_dpv.lforate = abs(m_dpv.lforate);

	m_dpv.cspincount = 1;
	
	if (m_dpv.cspinup) 
	{
		pitchinc = (255 - m_dpv.pitchstart) / m_dpv.cspinup;

		m_dpv.pitchrun = m_dpv.pitchstart + pitchinc;
		if (m_dpv.pitchrun > 255) m_dpv.pitchrun = 255;
	}

	if ((m_dpv.spinupsav || m_dpv.spindownsav || (m_dpv.lfotype && m_dpv.lfomodpitch))
		&& (m_dpv.pitch == PITCH_NORM))
		m_dpv.pitch = PITCH_NORM + 1; // must never send 'no pitch' as first pitch
									  // if we intend to pitch shift later!
}

//
// ToggleUse - turns an ambient sound on or off.  If the
// ambient is a looping sound, mark sound as active (m_fActive)
// if it's playing, innactive if not.  If the sound is not
// a looping sound, never mark it as active.
//
void CAmbientGeneric :: ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	char* szSoundFile = (char*) STRING(pev->message);
	float fraction;

	if ( useType != USE_TOGGLE )
	{
		if ( (m_fActive && useType == USE_ON) || (!m_fActive && useType == USE_OFF) )
			return;
	}
	// Directly change pitch if arg passed. Only works if sound is already playing.

	if (useType == USE_SET && m_fActive)		// Momentary buttons will pass down a float in here
	{

		fraction = value;

		if ( fraction > 1.0 )
			fraction = 1.0;
		if (fraction < 0.0)
			fraction = 0.01;

		m_dpv.pitch = fraction * 255;

		if ( pev->spawnflags & AMBIENT_SOUND_ACTIVATOR_ONLY )
			PlaySoundAtClient( pActivator, szSoundFile, 0, 0, SND_CHANGE_PITCH, m_dpv.pitch );
		else
			UTIL_EmitAmbientSound( edict(), pev->origin, szSoundFile,
					0, 0, SND_CHANGE_PITCH, m_dpv.pitch);

		return;
	}

	// Toggle

	// m_fActive is TRUE only if a looping sound is playing.

	if ( m_fActive )
	{// turn sound off

		if (m_dpv.cspinup)
		{
			// Don't actually shut off. Each toggle causes
			// incremental spinup to max pitch

			if (m_dpv.cspincount <= m_dpv.cspinup)
			{
				int pitchinc;

				// start a new spinup
				m_dpv.cspincount++;

				pitchinc = (255 - m_dpv.pitchstart) / m_dpv.cspinup;

				m_dpv.spinup = m_dpv.spinupsav;
				m_dpv.spindown = 0;

				m_dpv.pitchrun = m_dpv.pitchstart + pitchinc * m_dpv.cspincount;
				if (m_dpv.pitchrun > 255) m_dpv.pitchrun = 255;

				pev->nextthink = gpGlobals->time + 0.1;
			}

		}
		else
		{
			m_fActive = FALSE;

			// HACKHACK - this makes the code in Precache() work properly after a save/restore
			pev->spawnflags |= AMBIENT_SOUND_START_SILENT;

			if (m_dpv.spindownsav || m_dpv.fadeoutsav)
			{
				// spin it down (or fade it) before shutoff if spindown is set
				m_dpv.spindown = m_dpv.spindownsav;
				m_dpv.spinup = 0;

				m_dpv.fadeout = m_dpv.fadeoutsav;
				m_dpv.fadein = 0;
				pev->nextthink = gpGlobals->time + 0.1;
			}
			else if ( pev->spawnflags & AMBIENT_SOUND_ACTIVATOR_ONLY )
				PlaySoundAtClient( pActivator, szSoundFile, 0, 0, SND_STOP, 0 );
			else
				UTIL_EmitAmbientSound( edict(), pev->origin, szSoundFile,
					0, 0, SND_STOP, 0);
		}
	}
	else
	{// turn sound on

		// only toggle if this is a looping sound.  If not looping, each
		// trigger will cause the sound to play.  If the sound is still
		// playing from a previous trigger press, it will be shut off
		// and then restarted.

		if (m_fLooping)
			m_fActive = TRUE;
		else
			// shut sound off now - may be interrupting a long non-looping sound
			UTIL_EmitAmbientSound(ENT(pev), pev->origin, szSoundFile,
				0, 0, SND_STOP, 0);

		// init all ramp params for startup

		InitModulationParms();

		int flags = 0;

		if ( m_iPlayMode & AMBIENT_PLAYMODE_FORCE_ONCE )
			flags |= SOUNDMSG_FORCE_ONCE;
		else if ( m_iPlayMode & AMBIENT_PLAYMODE_FORCE_LOOP )
			flags |= SOUNDMSG_FORCE_LOOP;

		if ( m_iPlayMode & AMBIENT_PLAYMODE_LINEAR )
			flags |= SOUNDMSG_RADIUS;

		if ( pev->spawnflags & AMBIENT_SOUND_ACTIVATOR_ONLY )
			PlaySoundAtClient( pActivator, szSoundFile, m_dpv.vol * 0.01, m_flAttenuation, flags, m_dpv.pitch );
		else
			UTIL_EmitAmbientSound( edict(), pev->origin, szSoundFile, m_dpv.vol * 0.01, m_flAttenuation, flags, m_dpv.pitch);

		pev->nextthink = gpGlobals->time + 0.1;

	}
}

void CAmbientGeneric::LinearSetup()
{
	if ( m_iLinearMax > 15 )
		m_iLinearMax = 15;
	else if ( m_iLinearMax < 1 )
		m_iLinearMax = 1;

	if ( m_iLinearMin < 0 )
		m_iLinearMin = 0;
	else if ( m_iLinearMin > m_iLinearMax )
		m_iLinearMin = m_iLinearMax;

	m_iLinearMax <<= 4;
	m_flAttenuation = (float) ( m_iLinearMin | m_iLinearMax );
}

void CAmbientGeneric::Refresh()
{
	if ( !m_fLooping || !m_fActive )
		return;

	if ( !( g_flNextAmbientRefresh - 0.01 > gpGlobals->time ) )
	{
		g_flNextAmbientRefresh = gpGlobals->time + 0.3;
		m_fRefreshPending = false;
		pev->nextthink = gpGlobals->time + 12.0f;
	}
	else if ( !m_fRefreshPending )
	{
		m_fRefreshPending = true;
		pev->nextthink = g_flNextAmbientRefresh;
		g_flNextAmbientRefresh += 0.32;
		return;
	}
	else if ( !( g_flNextAmbientRefresh > gpGlobals->time + 12.0f ) )
	{
		m_fRefreshPending = false;
		pev->nextthink = gpGlobals->time + 12.0f;
	}
	else
	{
		m_fRefreshPending = true;
		pev->nextthink = g_flNextAmbientRefresh;
		g_flNextAmbientRefresh += 0.32;
	}

	int flags = SND_CHANGE_VOL | AMBIENT_REFRESH;

	if ( m_iPlayMode & AMBIENT_PLAYMODE_FORCE_ONCE )
		flags |= SOUNDMSG_FORCE_ONCE;
	else if ( m_iPlayMode & AMBIENT_PLAYMODE_FORCE_LOOP )
		flags |= SOUNDMSG_FORCE_LOOP;

	if ( m_iPlayMode & AMBIENT_PLAYMODE_LINEAR )
		flags |= SOUNDMSG_RADIUS;

	UTIL_EmitAmbientSound( edict(), pev->origin, STRING( pev->message ), m_dpv.vol * 0.01,
	                       m_flAttenuation, flags, m_dpv.pitch );
}

void CAmbientGeneric::PlaySoundAtClient( CBaseEntity *activator, char *sample, float volume, float attenuation, int flags, int pitch )
{
	if ( !( pev->spawnflags & AMBIENT_SOUND_EVERYWHERE ) && activator && activator->IsPlayer() )
	{
		EMIT_SOUND_DYN( activator->edict(), CHAN_AUTO, sample, volume, attenuation, flags, pitch,
		                activator->entindex(), false, g_vecZero );
		return;
	}

	for ( int index = 1; index <= gpGlobals->maxClients; ++index )
	{
		CBaseEntity *player = UTIL_PlayerByIndex( index );

		if ( !player )
			continue;

		EMIT_SOUND_DYN( player->edict(), CHAN_AUTO, sample, volume, attenuation, flags, pitch,
		                player->entindex(), false, g_vecZero );
	}
}

// KeyValue - load keyvalue pairs into member data of the
// ambient generic. NOTE: called BEFORE spawn!

void CAmbientGeneric :: KeyValue( KeyValueData *pkvd )
{
	// NOTE: changing any of the modifiers in this code
	// NOTE: also requires changing InitModulationParms code.

	// preset
	if (FStrEq(pkvd->szKeyName, "preset"))
	{
		m_dpv.preset = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}

	// pitchrun
	else if (FStrEq(pkvd->szKeyName, "pitch"))
	{
		m_dpv.pitchrun = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;

		if (m_dpv.pitchrun > 255) m_dpv.pitchrun = 255;
		if (m_dpv.pitchrun < 0) m_dpv.pitchrun = 0;
	}

	// pitchstart
	else if (FStrEq(pkvd->szKeyName, "pitchstart"))
	{
		m_dpv.pitchstart = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;

		if (m_dpv.pitchstart > 255) m_dpv.pitchstart = 255;
		if (m_dpv.pitchstart < 0) m_dpv.pitchstart = 0;
	}

	// spinup
	else if (FStrEq(pkvd->szKeyName, "spinup"))
	{
		m_dpv.spinup = atoi(pkvd->szValue);

		if (m_dpv.spinup > 100) m_dpv.spinup = 100;
		if (m_dpv.spinup < 0) m_dpv.spinup = 0;

		if (m_dpv.spinup > 0)
			m_dpv.spinup = (101 - m_dpv.spinup) * 64;
		m_dpv.spinupsav = m_dpv.spinup;
		pkvd->fHandled = TRUE;
	}

	// spindown
	else if (FStrEq(pkvd->szKeyName, "spindown"))
	{
		m_dpv.spindown = atoi(pkvd->szValue);

		if (m_dpv.spindown > 100) m_dpv.spindown = 100;
		if (m_dpv.spindown < 0) m_dpv.spindown = 0;

		if (m_dpv.spindown > 0)
			m_dpv.spindown = (101 - m_dpv.spindown) * 64;
		m_dpv.spindownsav = m_dpv.spindown;
		pkvd->fHandled = TRUE;
	}

	// volstart
	else if (FStrEq(pkvd->szKeyName, "volstart"))
	{
		m_dpv.volstart = atoi(pkvd->szValue);

		if (m_dpv.volstart > 10) m_dpv.volstart = 10;
		if (m_dpv.volstart < 0) m_dpv.volstart = 0;

		m_dpv.volstart *= 10;	// 0 - 100

		pkvd->fHandled = TRUE;
	}

	// fadein
	else if (FStrEq(pkvd->szKeyName, "fadein"))
	{
		m_dpv.fadein = atoi(pkvd->szValue);

		if (m_dpv.fadein > 100) m_dpv.fadein = 100;
		if (m_dpv.fadein < 0) m_dpv.fadein = 0;

		if (m_dpv.fadein > 0)
			m_dpv.fadein = (101 - m_dpv.fadein) * 64;
		m_dpv.fadeinsav = m_dpv.fadein;
		pkvd->fHandled = TRUE;
	}

	// fadeout
	else if (FStrEq(pkvd->szKeyName, "fadeout"))
	{
		m_dpv.fadeout = atoi(pkvd->szValue);

		if (m_dpv.fadeout > 100) m_dpv.fadeout = 100;
		if (m_dpv.fadeout < 0) m_dpv.fadeout = 0;

		if (m_dpv.fadeout > 0)
			m_dpv.fadeout = (101 - m_dpv.fadeout) * 64;
		m_dpv.fadeoutsav = m_dpv.fadeout;
		pkvd->fHandled = TRUE;
	}

	// lfotype
	else if (FStrEq(pkvd->szKeyName, "lfotype"))
	{
		m_dpv.lfotype = atoi(pkvd->szValue);
		if (m_dpv.lfotype > 5 ) m_dpv.lfotype = LFO_TRIANGLE;
		pkvd->fHandled = TRUE;
	}

	// lforate
	else if (FStrEq(pkvd->szKeyName, "lforate"))
	{
		m_dpv.lforate = atoi(pkvd->szValue);

		if (m_dpv.lforate > 1000) m_dpv.lforate = 1000;
		if (m_dpv.lforate < 0) m_dpv.lforate = 0;

		m_dpv.lforate *= 256;

		pkvd->fHandled = TRUE;
	}
	// lfomodpitch
	else if (FStrEq(pkvd->szKeyName, "lfomodpitch"))
	{
		m_dpv.lfomodpitch = atoi(pkvd->szValue);
		if (m_dpv.lfomodpitch > 100) m_dpv.lfomodpitch = 100;
		if (m_dpv.lfomodpitch < 0) m_dpv.lfomodpitch = 0;


		pkvd->fHandled = TRUE;
	}

	// lfomodvol
	else if (FStrEq(pkvd->szKeyName, "lfomodvol"))
	{
		m_dpv.lfomodvol = atoi(pkvd->szValue);
		if (m_dpv.lfomodvol > 100) m_dpv.lfomodvol = 100;
		if (m_dpv.lfomodvol < 0) m_dpv.lfomodvol = 0;

		pkvd->fHandled = TRUE;
	}

	// cspinup
	else if (FStrEq(pkvd->szKeyName, "cspinup"))
	{
		m_dpv.cspinup = atoi(pkvd->szValue);
		if (m_dpv.cspinup > 100) m_dpv.cspinup = 100;
		if (m_dpv.cspinup < 0) m_dpv.cspinup = 0;

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "playmode" ) )
	{
		m_iPlayMode = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "linearmin" ) )
	{
		m_iLinearMin = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "linearmax" ) )
	{
		m_iLinearMax = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

// =================== ROOM SOUND FX ==========================================

class CEnvSound : public CPointEntity
{
public:
	void KeyValue( KeyValueData* pkvd);
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void Think( void );
	bool InRange( entvars_t *pevTarget, float *pflRange );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	float m_flRadius;
	float m_flRoomtype;
};

LINK_ENTITY_TO_CLASS( env_sound, CEnvSound );
TYPEDESCRIPTION	CEnvSound::m_SaveData[] = 
{
	DEFINE_FIELD( CEnvSound, m_flRadius, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvSound, m_flRoomtype, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CEnvSound, CBaseEntity );


void CEnvSound :: KeyValue( KeyValueData *pkvd )
{
	
	if (FStrEq(pkvd->szKeyName, "radius"))
	{
		m_flRadius = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "roomtype"))
	{
		m_flRoomtype = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

// returns TRUE if the given sound entity (pev) is in range 
// and can see the given player entity (pevTarget)

bool CEnvSound::InRange( entvars_t *pevTarget, float *pflRange )
{
	Vector vecSpot1 = pev->origin + pev->view_ofs;
	Vector vecSpot2 = pevTarget->origin + pevTarget->view_ofs;
	Vector vecRange;
	float flRange;
	TraceResult tr;

	UTIL_TraceLine(vecSpot1, vecSpot2, ignore_monsters, ENT(pev), &tr);
	
	// check if line of sight crosses water boundary, or is blocked

	if ((tr.fInOpen && tr.fInWater) || tr.flFraction != 1)
		return FALSE;

	// calc range from sound entity to player

	vecRange = tr.vecEndPos - vecSpot1;
	flRange = vecRange.Length();

	if ( m_flRadius < flRange )
		return FALSE;
	
	if (pflRange)
		*pflRange = flRange;

	return TRUE;
}

//
// A client that is visible and in range of a sound entity will
// have its room_type set by that sound entity.  If two or more
// sound entities are contending for a client, then the nearest
// sound entity to the client will set the client's room_type.
// A client's room_type will remain set to its prior value until
// a new in-range, visible sound entity resets a new room_type.
//

// CONSIDER: if player in water state, autoset roomtype to 14,15 or 16. 

void CEnvSound :: Think( void )
{
	// get pointer to client if visible; FIND_CLIENT_IN_PVS will
	// cycle through visible clients on consecutive calls.

	edict_t *pentPlayer = FIND_CLIENT_IN_PVS(edict());
	CBasePlayer *pPlayer = NULL;

	if (FNullEnt(pentPlayer))
		goto env_sound_Think_slow; // no player in pvs of sound entity, slow it down
 
	pPlayer = GetClassPtr( (CBasePlayer *)VARS(pentPlayer));
	float flRange;

	// check to see if this is the sound entity that is 
	// currently affecting this player

	if(!FNullEnt(pPlayer->m_pentSndLast) && (pPlayer->m_pentSndLast == ENT(pev))) {

		// this is the entity currently affecting player, check
		// for validity

		if (pPlayer->m_flSndRoomtype != 0 && pPlayer->m_flSndRange != 0) {
		
			// we're looking at a valid sound entity affecting
			// player, make sure it's still valid, update range

			if ( InRange( VARS(pentPlayer), &flRange)) {
				pPlayer->m_flSndRange = flRange;
				goto env_sound_Think_fast;
			} else {
				
				// current sound entity affecting player is no longer valid,
				// flag this state by clearing room_type and range.
				// NOTE: we do not actually change the player's room_type
				// NOTE: until we have a new valid room_type to change it to.

				pPlayer->m_flSndRange = 0;
				pPlayer->m_flSndRoomtype = 0;
				goto env_sound_Think_slow;
			}
		} else {
			// entity is affecting player but is out of range,
			// wait passively for another entity to usurp it...
			goto env_sound_Think_slow;
		}
	}

	// if we got this far, we're looking at an entity that is contending
	// for current player sound. the closest entity to player wins.

	if ( InRange( VARS(pentPlayer), &flRange))
	{
		if (flRange < pPlayer->m_flSndRange || pPlayer->m_flSndRange == 0) 
		{
			// new entity is closer to player, so it wins.
			pPlayer->m_pentSndLast = ENT(pev);
			pPlayer->m_flSndRoomtype = m_flRoomtype;
			pPlayer->m_flSndRange = flRange;
			
			// send room_type command to player's server.
			// this should be a rare event - once per change of room_type
			// only!

			//CLIENT_COMMAND(pentPlayer, "room_type %f", m_flRoomtype);
			
			MESSAGE_BEGIN( MSG_ONE, SVC_ROOMTYPE, NULL, pentPlayer );		// use the magic #1 for "one client"
				WRITE_SHORT( (short)m_flRoomtype );					// sequence number
			MESSAGE_END();

			// crank up nextthink rate for new active sound entity
			// by falling through to think_fast...
		}
		// player is not closer to the contending sound entity,
		// just fall through to think_fast. this effectively
		// cranks up the think_rate of entities near the player.
	} 

	// player is in pvs of sound entity, but either not visible or
	// not in range. do nothing, fall through to think_fast...

env_sound_Think_fast:
	pev->nextthink = gpGlobals->time + 0.1;
	return;

env_sound_Think_slow:
	pev->nextthink = gpGlobals->time + 0.5;
	return;
}

//
// env_sound - spawn a sound entity that will set player roomtype
// when player moves in range and sight.
//
//
void CEnvSound :: Spawn( )
{
	if ( m_flRadius != 0 && !( pev->spawnflags & SF_ENVSOUND_START_OFF ) )
	{
		SetThink( NULL );
		pev->nextthink = gpGlobals->time + 0.1;
	}
	else
	{
		SetThink( NULL );
}
}

void CEnvSound::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator || !pActivator->IsPlayer() )
		return;

	float range;

	if ( m_flRadius != 0 && !InRange( pActivator->pev, &range ) )
		return;

	MESSAGE_BEGIN( MSG_ONE, SVC_ROOMTYPE, NULL, pActivator->edict() );
		WRITE_SHORT( (short) m_flRoomtype );
	MESSAGE_END();
}

class CEnvSentence : public CPointEntity
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

private:
	string_t m_iszSentence;
};

LINK_ENTITY_TO_CLASS( env_sentence, CEnvSentence );

void CEnvSentence::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "_text" ) )
	{
		m_iszSentence = AllocStringLocal( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		CPointEntity::KeyValue( pkvd );
	}
}

void CEnvSentence::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pev->spawnflags & SF_SENTENCE_ACTIVATOR_ONLY )
	{
		if ( pActivator && pActivator->IsPlayer() )
		{
			MESSAGE_BEGIN( MSG_ONE, SVC_STUFFTEXT, NULL, pActivator->edict() );
				WRITE_STRING( UTIL_VarArgs( "spk \"%s\"\n", STRING( m_iszSentence ) ) );
			MESSAGE_END();
		}
	}
	else
	{
		for ( int index = 1; index <= gpGlobals->maxClients; ++index )
		{
			CBaseEntity *player = UTIL_PlayerByIndex( index );

			if ( !player )
				continue;

			MESSAGE_BEGIN( MSG_ONE, SVC_STUFFTEXT, NULL, player->edict() );
				WRITE_STRING( UTIL_VarArgs( "spk \"%s\"\n", STRING( m_iszSentence ) ) );
			MESSAGE_END();
		}
	}

	FireTargets( STRING( pev->target ), pActivator, this, useType, value );
}

// ==================== SENTENCE GROUPS, UTILITY FUNCTIONS  ======================================

#define CSENTENCE_LRU_MAX	32		// max number of elements per sentence group

// group of related sentences

typedef struct sentenceg
{
	char szgroupname[CBSENTENCENAME_MAX];
	int count;
	unsigned char rgblru[CSENTENCE_LRU_MAX];

} SENTENCEG;

#define CSENTENCEG_MAX 200					// max number of sentence groups
// globals

SENTENCEG rgsentenceg[CSENTENCEG_MAX];
int fSentencesInit = FALSE;

char gszallsentencenames[CVOXFILESENTENCEMAX][CBSENTENCENAME_MAX];
int gcallsentences = 0;

// randomize list of sentence name indices

void USENTENCEG_InitLRU(unsigned char *plru, int count)
{
	int i, j, k;
	unsigned char temp;
	
	if (!fSentencesInit)
		return;

	if (count > CSENTENCE_LRU_MAX)
		count = CSENTENCE_LRU_MAX;

	for (i = 0; i < count; i++)
		plru[i] = (unsigned char) i;

	// randomize array
	for (i = 0; i < (count * 4); i++)
	{
		j = RANDOM_LONG(0,count-1);
		k = RANDOM_LONG(0,count-1);
		temp = plru[j];
		plru[j] = plru[k];
		plru[k] = temp;
	}
}

// ignore lru. pick next sentence from sentence group. Go in order until we hit the last sentence, 
// then repeat list if freset is true.  If freset is false, then repeat last sentence.
// ipick is passed in as the requested sentence ordinal.
// ipick 'next' is returned.  
// return of -1 indicates an error.

int USENTENCEG_PickSequential(int isentenceg, char *szfound, int ipick, int freset)
{
	char *szgroupname;
	unsigned char count;
	char sznum[8];
	
	if (!fSentencesInit)
		return -1;

	if (isentenceg < 0)
		return -1;

	szgroupname = rgsentenceg[isentenceg].szgroupname;
	count = rgsentenceg[isentenceg].count;
	
	if (count == 0)
		return -1;

	if (ipick >= count)
		ipick = count-1;

	strcpy(szfound, "!");
	strcat(szfound, szgroupname);
	sprintf(sznum, "%d", ipick);
	strcat(szfound, sznum);
	
	if (ipick >= count)
	{
		if (freset)
			// reset at end of list
			return 0;
		else
			return count;
	}

	return ipick + 1;
}



// pick a random sentence from rootname0 to rootnameX.
// picks from the rgsentenceg[isentenceg] least
// recently used, modifies lru array. returns the sentencename.
// note, lru must be seeded with 0-n randomized sentence numbers, with the
// rest of the lru filled with -1. The first integer in the lru is
// actually the size of the list.  Returns ipick, the ordinal
// of the picked sentence within the group.

int USENTENCEG_Pick(int isentenceg, char *szfound)
{
	char *szgroupname;
	unsigned char *plru;
	unsigned char i;
	unsigned char count;
	char sznum[8];
	unsigned char ipick;
	int ffound = FALSE;
	
	if (!fSentencesInit)
		return -1;

	if (isentenceg < 0)
		return -1;

	szgroupname = rgsentenceg[isentenceg].szgroupname;
	count = rgsentenceg[isentenceg].count;
	plru = rgsentenceg[isentenceg].rgblru;

	while (!ffound)
	{
		for (i = 0; i < count; i++)
			if (plru[i] != 0xFF)
			{
				ipick = plru[i];
				plru[i] = 0xFF;
				ffound = TRUE;
				break;
			}

		if (!ffound)
			USENTENCEG_InitLRU(plru, count);
		else
		{
			strcpy(szfound, "!");
			strcat(szfound, szgroupname);
			sprintf(sznum, "%d", ipick);
			strcat(szfound, sznum);
			return ipick;
		}
	}
	return -1;
}

// ===================== SENTENCE GROUPS, MAIN ROUTINES ========================

// Given sentence group rootname (name without number suffix),
// get sentence group index (isentenceg). Returns -1 if no such name.

int SENTENCEG_GetIndex(const char *szgroupname)
{
	int i;

	if (!fSentencesInit || !szgroupname)
		return -1;

	// search rgsentenceg for match on szgroupname

	i = 0;
	while (rgsentenceg[i].count)
	{
		if (!strcmp(szgroupname, rgsentenceg[i].szgroupname))
			return i;
	i++;
	}

	return -1;
}

// given sentence group index, play random sentence for given entity.
// returns ipick - which sentence was picked to 
// play from the group. Ipick is only needed if you plan on stopping
// the sound before playback is done (see SENTENCEG_Stop).

int SENTENCEG_PlayRndI(edict_t *entity, int isentenceg, 
					  float volume, float attenuation, int flags, int pitch)
{
	char name[64];
	int ipick;

	if (!fSentencesInit)
		return -1;

	name[0] = 0;

	ipick = USENTENCEG_Pick(isentenceg, name);
	if (ipick > 0 && name)
		EMIT_SOUND_DYN(entity, CHAN_VOICE, name, volume, attenuation, flags, pitch);
	return ipick;
}

// same as above, but takes sentence group name instead of index

int SENTENCEG_PlayRndSz(edict_t *entity, const char *szgroupname, 
					  float volume, float attenuation, int flags, int pitch)
{
	return CServer_SoundEngine::GetSoundEngine()->PlaySentenceGroup(
		entity, szgroupname, volume, attenuation, flags, pitch, -1 );
}

// play sentences in sequential order from sentence group.  Reset after last sentence.

int SENTENCEG_PlaySequentialSz(edict_t *entity, const char *szgroupname, 
					  float volume, float attenuation, int flags, int pitch, int ipick, int freset)
{
	char name[64];
	int ipicknext;
	int isentenceg;

	if (!fSentencesInit)
		return -1;

	name[0] = 0;

	isentenceg = SENTENCEG_GetIndex(szgroupname);
	if (isentenceg < 0)
		return -1;

	ipicknext = USENTENCEG_PickSequential(isentenceg, name, ipick, freset);
	if (ipicknext >= 0 && name[0])
		EMIT_SOUND_DYN(entity, CHAN_VOICE, name, volume, attenuation, flags, pitch);
	return ipicknext;
}


// for this entity, for the given sentence within the sentence group, stop
// the sentence.

void SENTENCEG_Stop(edict_t *entity, int isentenceg, int ipick)
{
	char buffer[64];
	char sznum[8];
	
	if (!fSentencesInit)
		return;

	if (isentenceg < 0 || ipick < 0)
		return;
	
	strcpy(buffer, "!");
	strcat(buffer, rgsentenceg[isentenceg].szgroupname);
	sprintf(sznum, "%d", ipick);
	strcat(buffer, sznum);

	STOP_SOUND(entity, CHAN_VOICE, buffer);
}

// open sentences.txt, scan for groups, build rgsentenceg
// Should be called from world spawn, only works on the
// first call and is ignored subsequently.

void SENTENCEG_Init()
{
	char buffer[512];
	char szgroup[64];
	int i, j;
	int isentencegs;

	if (fSentencesInit)
		return;

	memset(gszallsentencenames, 0, CVOXFILESENTENCEMAX * CBSENTENCENAME_MAX);
	gcallsentences = 0;

	memset(rgsentenceg, 0, CSENTENCEG_MAX * sizeof(SENTENCEG));
	memset(buffer, 0, 512);
	memset(szgroup, 0, 64);
	isentencegs = -1;

	
	int filePos = 0, fileSize;
	byte *pMemFile = g_engfuncs.pfnLoadFileForMe( "sound/sentences.txt", &fileSize );
	if ( !pMemFile )
		return;

	// for each line in the file...
	while ( memfgets(pMemFile, fileSize, filePos, buffer, 511) != NULL )
	{
		// skip whitespace
		i = 0;
		while(buffer[i] && buffer[i] == ' ')
			i++;
		
		if (!buffer[i])
			continue;

		if (buffer[i] == '/' || !isalpha(buffer[i]))
			continue;

		// get sentence name
		j = i;
		while (buffer[j] && buffer[j] != ' ')
			j++;

		if (!buffer[j])
			continue;

		if (gcallsentences > CVOXFILESENTENCEMAX)
		{
			ALERT (at_error, "Too many sentences in sentences.txt!\n");
			break;
		}

		// null-terminate name and save in sentences array
		buffer[j] = 0;
		const char *pString = buffer + i;

		if ( strlen( pString ) >= CBSENTENCENAME_MAX )
			ALERT( at_warning, "Sentence %s longer than %d letters\n", pString, CBSENTENCENAME_MAX-1 );

		strcpy( gszallsentencenames[gcallsentences++], pString );

		j--;
		if (j <= i)
			continue;
		if (!isdigit(buffer[j]))
			continue;

		// cut out suffix numbers
		while (j > i && isdigit(buffer[j]))
			j--;

		if (j <= i)
			continue;

		buffer[j+1] = 0;
		
		// if new name doesn't match previous group name, 
		// make a new group.

		if (strcmp(szgroup, &(buffer[i])))
		{
			// name doesn't match with prev name,
			// copy name into group, init count to 1
			isentencegs++;
			if (isentencegs >= CSENTENCEG_MAX)
			{
				ALERT (at_error, "Too many sentence groups in sentences.txt!\n");
				break;
			}

			strcpy(rgsentenceg[isentencegs].szgroupname, &(buffer[i]));
			rgsentenceg[isentencegs].count = 1;

			strcpy(szgroup, &(buffer[i]));

			continue;
		}
		else
		{
			//name matches with previous, increment group count
			if (isentencegs >= 0)
				rgsentenceg[isentencegs].count++;
		}
	}

	g_engfuncs.pfnFreeFile( pMemFile );
	
	fSentencesInit = TRUE;

	// init lru lists

	i = 0;

	while (rgsentenceg[i].count && i < CSENTENCEG_MAX)
	{
		USENTENCEG_InitLRU(&(rgsentenceg[i].rgblru[0]), rgsentenceg[i].count);
		i++;
	}

}

// convert sentence (sample) name to !sentencenum, return !sentencenum

int SENTENCEG_Lookup(const char *sample, char *sentencenum)
{
	char sznum[32];

	int i;
	// this is a sentence name; lookup sentence number
	// and give to engine as string.
	for (i = 0; i < gcallsentences; i++)
		if (!stricmp(gszallsentencenames[i], sample+1))
		{
			if (sentencenum)
			{
				strcpy(sentencenum, "!");
				sprintf(sznum, "%d", i);
				strcat(sentencenum, sznum);
			}
			return i;
		}
	// sentence name not found!
	return -1;
}

void EMIT_SOUND_DYN(edict_t *entity, int channel, const char *sample, float volume, float attenuation,
						   int flags, int pitch)
{
	EMIT_SOUND_DYN(entity, channel, sample, volume, attenuation, flags, pitch, 0, false, g_vecZero );
}

void EMIT_SOUND_DYN( edict_t *entity, int channel, const char *sample, float volume, float attenuation,
                     int flags, int pitch, int targetPlayer, bool useOrigin, const Vector &origin )
{
	if ( !sample )
		return;

	const char *replacement = FindSoundReplacementSample( entity, sample );

	if ( !replacement )
		return;

	if ( stricmp( replacement, sample ) )
		pitch = PITCH_NORM;

	if ( *replacement == '!' && CServer_SoundEngine::GetSoundEngine()->LookupSentenceIndex( (char *) replacement + 1 ) < 0 )
	{
		ALERT( at_aiconsole, "[Sound Engine] Unable to find %s in sentence list.\n", sample );
		return;
	}

	CServer_SoundEngine::GetSoundEngine()->PlaySound( entity, channel, (char *) replacement, volume, attenuation, flags,
	                                                  pitch, targetPlayer, useOrigin, origin, 0 );
}

// play a specific sentence over the HEV suit speaker - just pass player entity, and !sentencename

void EMIT_SOUND_SUIT(edict_t *entity, const char *sample)
{
	float fvol;
	int pitch = PITCH_NORM;

	fvol = CVAR_GET_FLOAT("suitvolume");
	if (RANDOM_LONG(0,1))
		pitch = RANDOM_LONG(0,6) + 98;

	fvol = V_min(1.0f, fvol);

	if (fvol > 0.05)
		EMIT_SOUND_DYN(entity, CHAN_AUTO, sample, fvol, ATTN_NORM, 0, pitch);
}

// play a sentence, randomly selected from the passed in group id, over the HEV suit speaker

void EMIT_GROUPID_SUIT(edict_t *entity, int isentenceg)
{
	float fvol;
	int pitch = PITCH_NORM;

	fvol = CVAR_GET_FLOAT("suitvolume");
	if (RANDOM_LONG(0,1))
		pitch = RANDOM_LONG(0,6) + 98;

	fvol = V_min(1.0f, fvol);

	if (fvol > 0.05)
		CServer_SoundEngine::GetSoundEngine()->PlaySentenceGroup(entity, isentenceg, fvol, ATTN_NORM, 0, pitch, CHAN_AUTO);
}

// play a sentence, randomly selected from the passed in groupname

void EMIT_GROUPNAME_SUIT(edict_t *entity, const char *groupname)
{
	float fvol;
	int pitch = PITCH_NORM;

	fvol = CVAR_GET_FLOAT("suitvolume");
	if (RANDOM_LONG(0,1))
		pitch = RANDOM_LONG(0,6) + 98;

	fvol = V_min(1.0f, fvol);

	if (fvol > 0.05)
		CServer_SoundEngine::GetSoundEngine()->PlaySentenceGroup(entity, groupname, fvol, ATTN_NORM, 0, pitch, CHAN_AUTO);
}

// ===================== MATERIAL TYPE DETECTION, MAIN ROUTINES ========================
// 
// Used to detect the texture the player is standing on, map the
// texture name to a material type.  Play footstep sound based
// on material type.

int fTextureTypeInit = FALSE;

#define CTEXTURESMAX		512			// max number of textures loaded

int gcTextures = 0;
char grgszTextureName[CTEXTURESMAX][CBTEXTURENAMEMAX];	// texture names
char grgchTextureType[CTEXTURESMAX];						// parallel array of texture types

// open materials.txt,  get size, alloc space, 
// save in array.  Only works first time called, 
// ignored on subsequent calls.

static char *memfgets( byte *pMemFile, int fileSize, int &filePos, char *pBuffer, int bufferSize )
{
	// Bullet-proofing
	if ( !pMemFile || !pBuffer )
		return NULL;

	if ( filePos >= fileSize )
		return NULL;

	int i = filePos;
	int last = fileSize;

	// fgets always NULL terminates, so only read bufferSize-1 characters
	if ( last - filePos > (bufferSize-1) )
		last = filePos + (bufferSize-1);

	int stop = 0;

	// Stop at the next newline (inclusive) or end of buffer
	while ( i < last && !stop )
	{
		if ( pMemFile[i] == '\n' )
			stop = 1;
		i++;
	}


	// If we actually advanced the pointer, copy it over
	if ( i != filePos )
	{
		// We read in size bytes
		int size = i - filePos;
		// copy it out
		memcpy( pBuffer, pMemFile + filePos, sizeof(byte)*size );
		
		// If the buffer isn't full, terminate (this is always true)
		if ( size < bufferSize )
			pBuffer[size] = 0;

		// Update file pointer
		filePos = i;
		return pBuffer;
	}

	// No data read, bail
	return NULL;
}


void TEXTURETYPE_Init()
{
	char buffer[512];
	int i, j;
	byte *pMemFile;
	int fileSize, filePos = 0;

	if (fTextureTypeInit)
		return;

	memset(&(grgszTextureName[0][0]), 0, CTEXTURESMAX * CBTEXTURENAMEMAX);
	memset(grgchTextureType, 0, CTEXTURESMAX);

	gcTextures = 0;
	memset(buffer, 0, 512);

	pMemFile = g_engfuncs.pfnLoadFileForMe( "sound/materials.txt", &fileSize );
	if ( !pMemFile )
		return;

	// for each line in the file...
	while (memfgets(pMemFile, fileSize, filePos, buffer, 511) != NULL && (gcTextures < CTEXTURESMAX))
	{
		// skip whitespace
		i = 0;
		while(buffer[i] && isspace(buffer[i]))
			i++;
		
		if (!buffer[i])
			continue;

		// skip comment lines
		if (buffer[i] == '/' || !isalpha(buffer[i]))
			continue;

		// get texture type
		grgchTextureType[gcTextures] = toupper(buffer[i++]);

		// skip whitespace
		while(buffer[i] && isspace(buffer[i]))
			i++;
		
		if (!buffer[i])
			continue;

		// get sentence name
		j = i;
		while (buffer[j] && !isspace(buffer[j]))
			j++;

		if (!buffer[j])
			continue;

		// null-terminate name and save in sentences array
		j = V_min (j, CBTEXTURENAMEMAX-1+i);
		buffer[j] = 0;
		strcpy(&(grgszTextureName[gcTextures++][0]), &(buffer[i]));
	}

	g_engfuncs.pfnFreeFile( pMemFile );
	
	fTextureTypeInit = TRUE;
}

// given texture name, find texture type
// if not found, return type 'concrete'

// NOTE: this routine should ONLY be called if the 
// current texture under the player changes!

char TEXTURETYPE_Find(char *name)
{
	// CONSIDER: pre-sort texture names and perform faster binary search here

	for (int i = 0; i < gcTextures; i++)
	{
		if (!strnicmp(name, &(grgszTextureName[i][0]), CBTEXTURENAMEMAX-1))
			return (grgchTextureType[i]);
	}

	return CHAR_TEX_CONCRETE;
}

// play a strike sound based on the texture that was hit by the attack traceline.  VecSrc/VecEnd are the
// original traceline endpoints used by the attacker, iBulletType is the type of bullet that hit the texture.
// returns volume of strike instrument (crowbar) to play

float TEXTURETYPE_PlaySound(TraceResult *ptr,  Vector vecSrc, Vector vecEnd, int iBulletType)
{
// hit the world, try to play sound based on texture material type
	
	char chTextureType;
	float fvol;
	float fvolbar;
	char szbuffer[64];
	const char *pTextureName;
	float rgfl1[3];
	float rgfl2[3];
	const char *rgsz[8];
	int cnt;
	float fattn = ATTN_NORM;

	CBaseEntity *pEntity = CBaseEntity::Instance(ptr->pHit);

	chTextureType = 0;

	if (pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE)
		// hit body
		chTextureType = CHAR_TEX_FLESH;
	else
	{
		// hit world

		// find texture under strike, get material type

		// copy trace vector into array for trace_texture

		vecSrc.CopyToArray(rgfl1);
		vecEnd.CopyToArray(rgfl2);

		// get texture from entity or world (world is ent(0))
		if (pEntity)
			pTextureName = TRACE_TEXTURE( ENT(pEntity->pev), rgfl1, rgfl2 );
		else
			pTextureName = TRACE_TEXTURE( ENT(0), rgfl1, rgfl2 );
			
		if ( pTextureName )
		{
			// strip leading '-0' or '+0~' or '{' or '!'
			if (*pTextureName == '-' || *pTextureName == '+')
				pTextureName += 2;

			if (*pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ')
				pTextureName++;
			// '}}'
			strcpy(szbuffer, pTextureName);
			szbuffer[CBTEXTURENAMEMAX - 1] = 0;
				
			// ALERT ( at_console, "texture hit: %s\n", szbuffer);

			// get texture type
			chTextureType = TEXTURETYPE_Find(szbuffer);	
		}
	}

	switch (chTextureType)
	{
	default:
	case CHAR_TEX_CONCRETE: fvol = 0.9;	fvolbar = 0.6;
		rgsz[0] = "player/pl_step1.wav";
		rgsz[1] = "player/pl_step2.wav";
		rgsz[2] = "player/pl_step3.wav";
		rgsz[3] = "player/pl_step4.wav";
		rgsz[4] = "player/pl_step5.wav";
		rgsz[5] = "player/pl_step6.wav";
		rgsz[6] = "player/pl_step7.wav";
		rgsz[7] = "player/pl_step8.wav";
		cnt = 8;
		break;
	case CHAR_TEX_METAL: fvol = 0.9; fvolbar = 0.3;
		rgsz[0] = "player/pl_metal1.wav";
		rgsz[1] = "player/pl_metal2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_DIRT:	fvol = 0.9; fvolbar = 0.1;
		rgsz[0] = "weapons/ric_grass1.wav";
		rgsz[1] = "weapons/ric_grass2.wav";
		rgsz[2] = "weapons/ric_grass3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_VENT:	fvol = 0.5; fvolbar = 0.3;
		rgsz[0] = "player/pl_duct1.wav";
		rgsz[1] = "player/pl_duct1.wav";
		cnt = 2;
		break;
	case CHAR_TEX_GRATE: fvol = 0.9; fvolbar = 0.5;
		rgsz[0] = "player/pl_grate1.wav";
		rgsz[1] = "player/pl_grate4.wav";
		cnt = 2;
		break;
	case CHAR_TEX_TILE:	fvol = 0.8; fvolbar = 0.2;
		rgsz[0] = "player/pl_tile1.wav";
		rgsz[1] = "player/pl_tile3.wav";
		rgsz[2] = "player/pl_tile2.wav";
		rgsz[3] = "player/pl_tile4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_SLOSH: fvol = 0.9; fvolbar = 0.0;
		rgsz[0] = "player/pl_slosh1.wav";
		rgsz[1] = "player/pl_slosh3.wav";
		rgsz[2] = "player/pl_slosh2.wav";
		rgsz[3] = "player/pl_slosh4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_WOOD: fvol = 0.9; fvolbar = 0.2;
		rgsz[0] = "debris/wood1.wav";
		rgsz[1] = "debris/wood2.wav";
		rgsz[2] = "debris/wood3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_GLASS:
	case CHAR_TEX_COMPUTER:
		fvol = 0.8; fvolbar = 0.2;
		rgsz[0] = "debris/glass1.wav";
		rgsz[1] = "debris/glass2.wav";
		rgsz[2] = "debris/glass3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_FLESH:
		fvol = 1.0;	fvolbar = 0.2;
		rgsz[0] = "player/pl_organic1.wav";
		rgsz[1] = "player/pl_organic2.wav";
		rgsz[2] = "player/pl_organic3.wav";
		rgsz[3] = "player/pl_organic4.wav";
		fattn = 1.0;
		cnt = 4;
		break;
	}

	// did we hit a breakable?

	if (pEntity && pEntity->IsBreakable())
	{
		// drop volumes, the object will already play a damaged sound
		fvol /= 1.5;
		fvolbar /= 2.0;	
	}
	else if (chTextureType == CHAR_TEX_COMPUTER)
	{
		// play random spark if computer

		if ( ptr->flFraction != 1.0 && RANDOM_LONG(0,1))
		{
			UTIL_Sparks( ptr->vecEndPos );

			float flVolume = RANDOM_FLOAT ( 0.7 , 1.0 );//random volume range
			switch ( RANDOM_LONG(0,1) )
			{
				case 0: UTIL_EmitAmbientSound(ENT(0), ptr->vecEndPos, "buttons/spark5.wav", flVolume, ATTN_NORM, 0, 100); break;
				case 1: UTIL_EmitAmbientSound(ENT(0), ptr->vecEndPos, "buttons/spark6.wav", flVolume, ATTN_NORM, 0, 100); break;
				// case 0: EMIT_SOUND(ENT(pev), CHAN_VOICE, "buttons/spark5.wav", flVolume, ATTN_NORM);	break;
				// case 1: EMIT_SOUND(ENT(pev), CHAN_VOICE, "buttons/spark6.wav", flVolume, ATTN_NORM);	break;
			}
		}
	}

	// play material hit sound
	UTIL_EmitAmbientSound(ENT(0), ptr->vecEndPos, rgsz[RANDOM_LONG(0,cnt-1)], fvol, fattn, 0, 96 + RANDOM_LONG(0,0xf));
	//EMIT_SOUND_DYN( ENT(m_hPlayer->pev), CHAN_WEAPON, rgsz[RANDOM_LONG(0,cnt-1)], fvol, ATTN_NORM, 0, 96 + RANDOM_LONG(0,0xf));
			
	return fvolbar;
}

// ===================================================================================
//
// Speaker class. Used for announcements per level, for door lock/unlock spoken voice. 
//

class CSpeaker : public CBaseEntity
{
public:
	void KeyValue( KeyValueData* pkvd);
	void Spawn( void );
	void Precache( void );
	void EXPORT ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT SpeakerThink( void );
	
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	virtual int	ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }
	
	int	m_preset;			// preset number
};

LINK_ENTITY_TO_CLASS( speaker, CSpeaker );
TYPEDESCRIPTION	CSpeaker::m_SaveData[] = 
{
	DEFINE_FIELD( CSpeaker, m_preset, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CSpeaker, CBaseEntity );

//
// ambient_generic - general-purpose user-defined static sound
//
void CSpeaker :: Spawn( void )
{
	char* szSoundFile = (char*) STRING(pev->message);

	if ( !m_preset && (FStringNull( pev->message ) || strlen( szSoundFile ) < 1 ))
	{
		ALERT( at_error, "SPEAKER with no Level/Sentence! at: %f, %f, %f\n", pev->origin.x, pev->origin.y, pev->origin.z );
		pev->nextthink = gpGlobals->time + 0.1;
		SetThink( &CSpeaker::SUB_Remove );
		return;
	}
    pev->solid		= SOLID_NOT;
    pev->movetype	= MOVETYPE_NONE;

	
	SetThink(&CSpeaker::SpeakerThink);
	pev->nextthink = 0.0;

	// allow on/off switching via 'use' function.

	SetUse ( &CSpeaker::ToggleUse );

	Precache( );
}

#define ANNOUNCE_MINUTES_MIN	0.25	 
#define ANNOUNCE_MINUTES_MAX	2.25

void CSpeaker :: Precache( void )
{
	if ( !FBitSet (pev->spawnflags, SPEAKER_START_SILENT ) )
		// set first announcement time for random n second
		pev->nextthink = gpGlobals->time + RANDOM_FLOAT(5.0, 15.0);
}
void CSpeaker :: SpeakerThink( void )
{
	const char* szSoundFile;
	float flvolume = pev->health * 0.1;
	float flattenuation = 0.3;
	int flags = 0;
	int pitch = 100;


	// Wait for the talkmonster to finish first.
	if (gpGlobals->time <= CTalkMonster::g_talkWaitTime)
	{
		pev->nextthink = CTalkMonster::g_talkWaitTime + RANDOM_FLOAT( 5, 10 );
		return;
	}
	
	if (m_preset)
	{
		// go lookup preset text, assign szSoundFile
		switch (m_preset)
		{
		case 1: szSoundFile =  "C1A0_"; break;
		case 2: szSoundFile =  "C1A1_"; break;
		case 3: szSoundFile =  "C1A2_"; break;
		case 4: szSoundFile =  "C1A3_"; break;
		case 5: szSoundFile =  "C1A4_"; break; 
		case 6: szSoundFile =  "C2A1_"; break;
		case 7: szSoundFile =  "C2A2_"; break;
		case 8: szSoundFile =  "C2A3_"; break;
		case 9: szSoundFile =  "C2A4_"; break;
		case 10: szSoundFile = "C2A5_"; break;
		case 11: szSoundFile = "C3A1_"; break;
		case 12: szSoundFile = "C3A2_"; break;
		}
	} else
		szSoundFile = STRING(pev->message);
	
	if (szSoundFile[0] == '!')
	{
		// play single sentence, one shot
		UTIL_EmitAmbientSound ( ENT(pev), pev->origin, szSoundFile, 
			flvolume, flattenuation, flags, pitch);

		// shut off and reset
		pev->nextthink = 0.0;
	}
	else
	{
		// make random announcement from sentence group

		if (SENTENCEG_PlayRndSz(ENT(pev), szSoundFile, flvolume, flattenuation, flags, pitch) < 0)
			ALERT(at_console, "Level Design Error!\nSPEAKER has bad sentence group name: %s\n",szSoundFile); 

		// set next announcement time for random 5 to 10 minute delay
		pev->nextthink = gpGlobals->time + 
						RANDOM_FLOAT(ANNOUNCE_MINUTES_MIN * 60.0, ANNOUNCE_MINUTES_MAX * 60.0);

		CTalkMonster::g_talkWaitTime = gpGlobals->time + 5;		// time delay until it's ok to speak: used so that two NPCs don't talk at once
	}

	return;
}


//
// ToggleUse - if an announcement is pending, cancel it.  If no announcement is pending, start one.
//
void CSpeaker :: ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int fActive = (pev->nextthink > 0.0);

	// fActive is TRUE only if an announcement is pending
	
	if ( useType != USE_TOGGLE )
	{
		// ignore if we're just turning something on that's already on, or
		// turning something off that's already off.
		if ( (fActive && useType == USE_ON) || (!fActive && useType == USE_OFF) )
			return;
	}

	if ( useType == USE_ON )
	{
		// turn on announcements
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	if ( useType == USE_OFF )
	{
		// turn off announcements
		pev->nextthink = 0.0;
		return;
	
	}

	// Toggle announcements

	
	if ( fActive )
	{
		// turn off announcements
		pev->nextthink = 0.0;
	}
	else 
	{
		// turn on announcements
		pev->nextthink = gpGlobals->time + 0.1;
	} 
}

// KeyValue - load keyvalue pairs into member data
// NOTE: called BEFORE spawn!

void CSpeaker :: KeyValue( KeyValueData *pkvd )
{
	
	// preset
	if (FStrEq(pkvd->szKeyName, "preset"))
	{
		m_preset = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}
