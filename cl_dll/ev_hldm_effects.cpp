#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "cl_entity.h"
#include "pm_defs.h"
#include "pm_shared.h"
#include "pm_materials.h"
#include "eventscripts.h"
#include "ev_hldm.h"
#include "r_efx.h"
#include "event_api.h"
#include "r_studioint.h"
#include "client_sound.h"
#include "client_support.h"
#include "particle_engine.h"
#include "parsemsg.h"
#include "dlight.h"

#include <stdio.h>
#include <string.h>

extern engine_studio_api_t IEngineStudio;
extern int tracerCount[32];

void VectorAngles(const float *forward, float *angles);

TEMPENTITY *EV_Gib(GIBTYPE type, int bloodColor, float *origin, float *velocity, float life, int body)
{
	char modelName[260];
	int bodyCount = 1;

	switch (type)
	{
	case GIBTYPE_HUMAN:
		UTIL_CopyString(modelName, "models/hgibs.mdl", sizeof(modelName));
		bodyCount = HUMAN_GIB_BODIES;
		break;
	case GIBTYPE_ALIEN:
		UTIL_CopyString(modelName, "models/agibs.mdl", sizeof(modelName));
		bodyCount = ALIEN_GIB_BODIES;
		break;
	case GIBTYPE_GLASS:
		UTIL_CopyString(modelName, "models/glassgibs.mdl", sizeof(modelName));
		bodyCount = GLASS_GIB_BODIES;
		break;
	case GIBTYPE_STICKY:
		UTIL_CopyString(modelName, "models/stickygibpink.mdl", sizeof(modelName));
		break;
	default:
		break;
	}

	int modelIndex = 0;
	struct model_s *model = gEngfuncs.CL_LoadModel(modelName, &modelIndex);

	if (!model)
		return NULL;

	TEMPENTITY *gib = gEngfuncs.pEfxAPI->CL_TentEntAllocCustom(origin, model, 0, EV_TE_GibCallback);

	if (!gib)
		return NULL;

	if (!gib->callback)
		gib->callback = EV_TE_GibCallback;

	if (!gib->hitcallback)
		gib->hitcallback = type == GIBTYPE_STICKY ? EV_TE_StickyGibHitCallback : EV_TE_GibHitCallback;

	for (int i = 0; i < 3; i++)
	{
		if (gEngfuncs.pfnRandomLong(0, 1))
			gib->entity.baseline.angles[i] = gEngfuncs.pfnRandomLong(0, 180);
		else
			gib->entity.baseline.angles[i] = -gEngfuncs.pfnRandomLong(0, 180);
	}

	if (type == GIBTYPE_STICKY)
	{
		for (int i = 0; i < 3; i++)
		{
			if (gEngfuncs.pfnRandomLong(0, 1))
				gib->entity.angles[i] = gEngfuncs.pfnRandomLong(0, 180);
			else
				gib->entity.angles[i] = -gEngfuncs.pfnRandomLong(0, 180);
		}
	}

	VectorCopy(velocity, gib->entity.baseline.origin);
	gib->flags = FTENT_GRAVITY | FTENT_ROTATE | FTENT_HITSOUND | FTENT_COLLIDEALL | FTENT_CLIENTCUSTOM;
	gib->die = gEngfuncs.GetClientTime() + life;

	int randomBody = gEngfuncs.pfnRandomLong(1, bodyCount) - 1;

	if (body == -1)
		body = randomBody;

	gib->entity.baseline.body = body;
	gib->entity.curstate.body = body;
	gib->entity.prevstate.body = body;
	gib->entity.baseline.iuser2 = bloodColor;
	return gib;
}

// Save the gib position and speed for blood decals on impact
void EV_TE_GibCallback(TEMPENTITY *ent, float frametime, float currenttime)
{
	Vector delta = ent->entity.origin - ent->entity.baseline.vuser1;

	ent->entity.baseline.fuser1 = (int)(Length(delta) / frametime);
	ent->entity.baseline.vuser1 = ent->entity.origin;
}

void EV_GibFleshSound(TEMPENTITY *ent, float speed)
{
	static const char *sounds[] =
	{
		"debris/flesh2.wav", "debris/flesh3.wav", "debris/flesh5.wav", "debris/flesh6.wav"
	};

	int sound = gEngfuncs.pfnRandomLong(0, ARRAYSIZE(sounds) - 1);
	CClient_SoundEngine *soundEngine = CClient_SoundEngine::GetSoundEngine();
	int pitch = gEngfuncs.pfnRandomLong(0, 12) + 94;
	float volume = V_min(1.0, speed / 300);

	soundEngine->PlayFMODSound(SOUNDMSG_ORIGIN, ent->entity.index, ent->entity.origin, CHAN_WEAPON,
		sounds[sound], volume, 1, 0, pitch, -1, 0);
}

void EV_TE_GibHitCallback(TEMPENTITY *ent, pmtrace_t *trace)
{
	float speed = fabs(ent->entity.baseline.origin.z);

	if (speed < 100)
		return;

	if (ent->entity.baseline.fuser1 >= 5)
		EV_HLDM_DecalBlood(trace, ent->entity.baseline.iuser2);

	EV_GibFleshSound(ent, speed);
}

void EV_TE_StickyGibHitCallback(TEMPENTITY *ent, pmtrace_t *trace)
{
	float speed = ent->entity.baseline.origin.Length();

	if (speed < 100)
		speed = 100;

	if (ent->entity.baseline.fuser1 >= 5)
		EV_HLDM_DecalBlood(trace, ent->entity.baseline.iuser2);

	EV_GibFleshSound(ent, speed);

	if (trace->ent == 0)
	{
		ent->flags &= ~(FTENT_GRAVITY | FTENT_ROTATE);
		ent->entity.baseline.origin = Vector(0, 0, 0);
		ent->entity.prevstate.origin = trace->endpos;
	}
}

int EV_IsAllyTeam(int iTeam1, int iTeam2)
{
	return 0;
}

int EV_Message_Gib(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int type = READ_BYTE();
	int count = (int)CVAR_GET_FLOAT("cl_gibcount");
	Vector origin(0, 0, 0);
	Vector velocity(0, 0, 0);
	Vector gibVelocity(0, 0, 0);

	switch (type)
	{
	case GIBMSG_HUMAN:
		origin.x = READ_COORD();
		origin.y = READ_COORD();
		origin.z = READ_COORD();
		velocity.x = READ_COORD();
		velocity.y = READ_COORD();
		velocity.z = READ_COORD();

		gibVelocity.x = gEngfuncs.pfnRandomFloat(-1, 1) * Length(velocity);
		gibVelocity.y = gEngfuncs.pfnRandomFloat(-1, 1) * Length(velocity);
		gibVelocity.z = gEngfuncs.pfnRandomFloat(-1, 1) * Length(velocity);
		EV_Gib(GIBTYPE_HUMAN, BLOOD_COLOR_RED, origin, gibVelocity, 15, 0);

		for (int i = 0; i < count; i++)
		{
			int body = gEngfuncs.pfnRandomLong(1, HUMAN_GIB_BODIES - 1);

			gibVelocity.x = gEngfuncs.pfnRandomFloat(-1, 1) * Length(velocity);
			gibVelocity.y = gEngfuncs.pfnRandomFloat(-1, 1) * Length(velocity);
			gibVelocity.z = gEngfuncs.pfnRandomFloat(-1, 1) * Length(velocity);
			EV_Gib(GIBTYPE_HUMAN, BLOOD_COLOR_RED, origin, gibVelocity, 15, body);
		}
		break;
	case GIBMSG_HUMAN_RANDOM:
		origin.x = READ_COORD();
		origin.y = READ_COORD();
		origin.z = READ_COORD();
		velocity.x = READ_COORD();
		velocity.y = READ_COORD();
		velocity.z = READ_COORD();

		for (int i = 0; i < count; i++)
		{
			int body = gEngfuncs.pfnRandomLong(1, 6);

			gibVelocity.x = gEngfuncs.pfnRandomFloat(-1, 1) * Length(velocity);
			gibVelocity.y = gEngfuncs.pfnRandomFloat(-1, 1) * Length(velocity);
			gibVelocity.z = gEngfuncs.pfnRandomFloat(-1, 1) * Length(velocity);
			EV_Gib(GIBTYPE_HUMAN, BLOOD_COLOR_RED, origin, gibVelocity, 15, body);
		}

		// throw the skull upwards
		gibVelocity.x = gEngfuncs.pfnRandomFloat(-0.2f, 0.3f) * Length(velocity);
		gibVelocity.y = gEngfuncs.pfnRandomFloat(-0.2f, 0.3f) * Length(velocity);
		gibVelocity.z = Length(velocity) * 0.8;
		origin.z += 32;
		EV_Gib(GIBTYPE_HUMAN, BLOOD_COLOR_RED, origin, gibVelocity, 15, 0);
		break;
	case GIBMSG_ALIEN_RANDOM:
		origin.x = READ_COORD();
		origin.y = READ_COORD();
		origin.z = READ_COORD();
		velocity.x = READ_COORD();
		velocity.y = READ_COORD();
		velocity.z = READ_COORD();

		for (int i = 0; i < count; i++)
		{
			int body = gEngfuncs.pfnRandomLong(0, ALIEN_GIB_BODIES);

			gibVelocity.x = gEngfuncs.pfnRandomFloat(-1, 1) * Length(velocity);
			gibVelocity.y = gEngfuncs.pfnRandomFloat(-1, 1) * Length(velocity);
			gibVelocity.z = gEngfuncs.pfnRandomFloat(-1, 1) * Length(velocity);
			EV_Gib(GIBTYPE_ALIEN, BLOOD_COLOR_YELLOW, origin, gibVelocity, 15, body);
		}
		break;
	case GIBMSG_STICKY:
		origin.x = READ_COORD();
		origin.y = READ_COORD();
		origin.z = READ_COORD();
		velocity.x = READ_COORD();
		velocity.y = READ_COORD();
		velocity.z = READ_COORD();

		for (int i = 0; i < count * 3; i++)
		{
			gibVelocity.x = gEngfuncs.pfnRandomFloat(-1, 1) * Length(velocity);
			gibVelocity.y = gEngfuncs.pfnRandomFloat(-1, 1) * Length(velocity);
			gibVelocity.z = gEngfuncs.pfnRandomFloat(-1, 1) * Length(velocity);
			EV_Gib(GIBTYPE_STICKY, BLOOD_COLOR_RED, origin, gibVelocity, 15, 0);
		}
		break;
	default:
		break;
	}

	return 1;
}

void EV_HLDM_DecalBlood(pmtrace_t *trace, int bloodColor)
{
	physent_t *entity = gEngfuncs.pEventAPI->EV_GetPhysent(trace->ent);
	char decalName[16];

	UTIL_FormatString(decalName, bloodColor == BLOOD_COLOR_RED ? "{blood%d" : "{yblood%d",
		gEngfuncs.pfnRandomLong(1, 6));

	if (!decalName[0] || !entity || (entity->solid != SOLID_BSP && entity->movetype != MOVETYPE_PUSHSTEP))
		return;

	cl_entity_t *clientEntity = gEngfuncs.GetEntityByIndex(entity->info);

	if (clientEntity && (clientEntity->curstate.effects & EF_NODECALS))
		return;

	int index = gEngfuncs.pEventAPI->EV_IndexFromTrace(trace);
	int decal = gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(decalName);

	gEngfuncs.pEfxAPI->R_DecalShoot(gEngfuncs.pEfxAPI->Draw_DecalIndex(decal), index, 0, trace->endpos, 0);
}

char *EV_HLDM_DamageDecal( physent_t *pe )
{
	static char decalname[ 32 ];

	if ( pe->classnumber == 1 )
		UTIL_FormatString(decalname, "{break%i", gEngfuncs.pfnRandomLong( 0, 2 ) + 1 );
	else if ( pe->rendermode != kRenderNormal )
		UTIL_FormatString( decalname, "{bproof1" );
	else
		UTIL_FormatString(decalname, "{shot%i", gEngfuncs.pfnRandomLong( 0, 4 ) + 1 );

	return decalname;
}

float EV_HLDM_PlayTextureSound( int idx, pmtrace_t *ptr, float *vecSrc, float *vecEnd, int iBulletType )
{
	// hit the world, try to play sound based on texture material type
	char chTextureType = 0;
	float fvol = 0.9f;
	float fvolbar;
	float attenuation = ATTN_NORM;
	const char *rgsz[8];
	int cnt;
	int entity = gEngfuncs.pEventAPI->EV_IndexFromTrace( ptr );

	// Player
	if ( entity >= 1 && entity <= gEngfuncs.GetMaxClients() )
		chTextureType = CHAR_TEX_FLESH;
	else if ( entity == 0 )
	{
		const char *pTextureName = gEngfuncs.pEventAPI->EV_TraceTexture( ptr->ent, vecSrc, vecEnd );

		if ( pTextureName )
		{
			char texname[64], stripped[64];

			UTIL_CopyString( texname, pTextureName, sizeof(texname));
			pTextureName = texname;

			// strip leading '-0' or '+0~' or '{' or '!'
			if (*pTextureName == '-' || *pTextureName == '+')
				pTextureName += 2;

			if (*pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ')
				pTextureName++;

			UTIL_CopyString(stripped, pTextureName, sizeof(stripped));
			stripped[MAX_MATERIAL_NAME - 1 ] = 0;

			// get texture type
			chTextureType = PM_FindTextureType(stripped);
		}
	}

	switch (chTextureType)
	{
	case CHAR_TEX_DIRT:
		fvolbar = 0.1f;
		cnt = 3;
		rgsz[0] = "weapons/ric_grass1.wav";
		rgsz[1] = "weapons/ric_grass2.wav";
		rgsz[2] = "weapons/ric_grass3.wav";
		break;
	case CHAR_TEX_FLESH:
		if (iBulletType == BULLET_PLAYER_CROWBAR)
			return 0;

		fvol = attenuation = 1;
		fvolbar = 0.2f;
		cnt = 2;
		rgsz[0] = "weapons/bullet_hit1.wav";
		rgsz[1] = "weapons/bullet_hit2.wav";
		break;
	case CHAR_TEX_GRATE:
		fvolbar = 0.5f;
		cnt = 2;
		rgsz[0] = "player/pl_grate1.wav";
		rgsz[1] = "player/pl_grate4.wav";
		break;
	case CHAR_TEX_METAL:
		fvolbar = 0.3;
		cnt = 2;
		rgsz[0] = "player/pl_metal1.wav";
		rgsz[1] = "player/pl_metal2.wav";
		break;
	case CHAR_TEX_SNOW:
		fvolbar = 0;
		cnt = 8;
		rgsz[0] = "player/pl_snow1.wav";
		rgsz[1] = "player/pl_snow3.wav";
		rgsz[2] = "player/pl_snow2.wav";
		rgsz[3] = "player/pl_snow4.wav";
		rgsz[4] = "player/pl_snow5.wav";
		rgsz[5] = "player/pl_snow6.wav";
		rgsz[6] = "player/pl_snow7.wav";
		rgsz[7] = "player/pl_snow8.wav";
		break;
	case CHAR_TEX_COMPUTER:
	case CHAR_TEX_GLASS:
		fvol = 0.8f;
		fvolbar = 0.2f;
		cnt = 3;
		rgsz[0] = "debris/glass1.wav";
		rgsz[1] = "debris/glass2.wav";
		rgsz[2] = "debris/glass3.wav";
		break;
	case CHAR_TEX_SLOSH:
		fvolbar = 0;
		cnt = 4;
		rgsz[0] = "player/pl_slosh1.wav";
		rgsz[1] = "player/pl_slosh3.wav";
		rgsz[2] = "player/pl_slosh2.wav";
		rgsz[3] = "player/pl_slosh4.wav";
		break;
	case CHAR_TEX_TILE:	fvol = 0.8; fvolbar = 0.2;
		cnt = 4;
		rgsz[0] = "player/pl_tile1.wav";
		rgsz[1] = "player/pl_tile3.wav";
		rgsz[2] = "player/pl_tile2.wav";
		rgsz[3] = "player/pl_tile4.wav";
		break;
	case CHAR_TEX_VENT: fvol = 0.5f; fvolbar = 0.3f;
		cnt = 2;
		rgsz[0] = "player/pl_duct1.wav";
		rgsz[1] = "player/pl_duct1.wav";
		break;
	case CHAR_TEX_WOOD:
		fvolbar = 0.2;
		cnt = 3;
		rgsz[0] = "debris/wood1.wav";
		rgsz[1] = "debris/wood2.wav";
		rgsz[2] = "debris/wood3.wav";
		break;
	default:
		fvolbar = 0.6f;
		cnt = 8;
		rgsz[0] = "player/pl_step1.wav";
		rgsz[1] = "player/pl_step2.wav";
		rgsz[2] = "player/pl_step3.wav";
		rgsz[3] = "player/pl_step4.wav";
		rgsz[4] = "player/pl_step5.wav";
		rgsz[5] = "player/pl_step6.wav";
		rgsz[6] = "player/pl_step7.wav";
		rgsz[7] = "player/pl_step8.wav";
		break;
	}

	CClient_SoundEngine *soundEngine = CClient_SoundEngine::GetSoundEngine();
	int pitch = gEngfuncs.pfnRandomLong( 0, 15) + 96;
	int sound = gEngfuncs.pfnRandomLong(0,cnt-1);

	soundEngine->PlayFMODSound(SOUNDMSG_ORIGIN, -1, ptr->endpos, CHAN_STATIC, rgsz[sound], fvol, attenuation, 0, pitch, -1, 0);
	return fvolbar;
}

void EV_HLDM_GunshotDecalTrace( pmtrace_t *pTrace, char *decalName )
{
	int iRand = gEngfuncs.pfnRandomLong(0,0x7FFF);
	if ( iRand < (0x7FFF /2) )// not every bullet makes a sound.
	{
		static const char *sounds[] =
		{
			"weapons/ric1.wav", "weapons/ric2.wav", "weapons/ric3.wav",
			"weapons/ric4.wav", "weapons/ric5.wav"
		};

		CClient_SoundEngine::GetSoundEngine()->PlayFMODSound(SOUNDMSG_ORIGIN, -1, pTrace->endpos,
			CHAN_AUTO, sounds[iRand % (int)ARRAYSIZE(sounds)], 1, ATTN_NORM, 0, PITCH_NORM, -1, 0);
	}

	physent_t *pe = gEngfuncs.pEventAPI->EV_GetPhysent( pTrace->ent );

	// Only decal brush models such as the world etc.
	if (CVAR_GET_FLOAT("r_decals") >= 1 && CVAR_GET_FLOAT("mp_decals") >= 1 &&
		decalName && *decalName && pe && ( pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP ) )
	{
		cl_entity_t *clientEntity = gEngfuncs.GetEntityByIndex(pe->info);

		if (!clientEntity || !(clientEntity->curstate.effects & EF_NODECALS) )
		{
			int index = gEngfuncs.pEventAPI->EV_IndexFromTrace(pTrace);
			int decal = gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(decalName);

			gEngfuncs.pEfxAPI->R_DecalShoot(
				gEngfuncs.pEfxAPI->Draw_DecalIndex(decal),
				index, 0, pTrace->endpos, 0 );
		}
	}

	if (!IEngineStudio.IsHardware())
	{
		gEngfuncs.pEfxAPI->R_BulletImpactParticles(pTrace->endpos);
		return;
	}

	if (gEngfuncs.PM_PointContents(pTrace->endpos, NULL) != CONTENTS_WATER)
	{
		CParticleSystem *system = ParticleEngine()->SystemCreate();

		system->m_flDieTime = gEngfuncs.GetClientTime() + 10.0;
		system->m_vecOrigin = pTrace->endpos;
		system->m_vecDirection = pTrace->plane.normal;
		system->SystemStartup(PSYS_GUNSHOT_SMOKE, NULL);
	}

	for (int i = 0; i < 3; i++)
	{
		CParticleSystem *system = ParticleEngine()->SystemCreate();

		system->m_flDieTime = gEngfuncs.GetClientTime() + 4.0f;
		system->m_vecOrigin = pTrace->endpos;

		float z = gEngfuncs.pfnRandomFloat(-0.5f, 0.5f);
		float y = gEngfuncs.pfnRandomFloat(-1, 1);
		float x = gEngfuncs.pfnRandomFloat(-1, 1);

		system->m_vecDirection = pTrace->plane.normal + Vector(x, y, z);
		system->m_pszSprite = "sprites/particles/deb_conc1.spr";
		system->SystemStartup(PSYS_GUNSHOT_DEBRIS, NULL);
	}
}

void EV_HLDM_DecalGunshot( pmtrace_t *pTrace, int iBulletType )
{
	physent_t *pe = gEngfuncs.pEventAPI->EV_GetPhysent( pTrace->ent );

	if (!pe || pe->solid != SOLID_BSP ||
		gEngfuncs.PM_PointContents(pTrace->endpos, NULL) == CONTENTS_SKY)
		return;

	EV_HLDM_GunshotDecalTrace( pTrace, EV_HLDM_DamageDecal( pe ) );

	float scale = gEngfuncs.pfnRandomFloat(4, 10);
	Vector direction = pTrace->plane.normal * scale;
	float speed = gEngfuncs.pfnRandomFloat(4, 10);
	int count = gEngfuncs.pfnRandomLong(5, 10);

	gEngfuncs.pEfxAPI->R_StreakSplash(pTrace->endpos, direction, 5, count, speed, -75, 75);
}

int EV_HLDM_CheckTracer( int idx, float *vecSrc, float *end, float *forward, float *right, int iBulletType, int iTracerFreq, int *tracerCount )
{
	bool player = idx >= 1 && idx <= gEngfuncs.GetMaxClients();

	if (!iTracerFreq || (*tracerCount)++ % iTracerFreq)
		return 0;

	Vector origin(vecSrc);

		if ( player )
		origin = origin + Vector( 0, 0, -4 ) + Vector(right) * 2 + Vector(forward) * -16;

	if (iBulletType != BULLET_PLAYER_CUSTOMDAMAGE && iBulletType != BULLET_MONSTER_SNIPER)
		EV_CreateTracer(origin, end );

	return iTracerFreq != 1;
}

void EV_HLDM_WaterSplash(const Vector &origin, const Vector &direction, bool soundOnly)
{
	if (!soundOnly)
	{
		CParticleSystem *system = ParticleEngine()->SystemCreate();

		system->m_flDieTime = gEngfuncs.GetClientTime() + 10.0;
		system->m_vecOrigin = origin;
		system->m_vecDirection = direction;
		system->SystemStartup(PSYS_WATER_SPLASH, NULL);
	}

	static const char *sounds[] =
	{
		"player/pl_slosh1.wav", "player/pl_slosh3.wav",
		"player/pl_slosh2.wav", "player/pl_slosh4.wav"
	};

	CClient_SoundEngine *soundEngine = CClient_SoundEngine::GetSoundEngine();
	int pitch = gEngfuncs.pfnRandomLong(0, 15) + 96;
	int sound = gEngfuncs.pfnRandomLong(0, 3);

	soundEngine->PlayFMODSound(SOUNDMSG_ORIGIN, -1, origin, CHAN_AUTO, sounds[sound], 0.8f, ATTN_NORM, 0, pitch, -1, 0);
}

void EV_HLDM_FireBullets( int idx, float *forward, float *right, float *up, int cShots, float *vecSrc, float *vecDirShooting, float *spread, float distance, int iBulletType, int iTracerFreq, int *tracerCount)
{
	pmtrace_t tr = {};
	Vector rightVector(right);

	for (int iShot = 0; iShot < cShots; iShot++ )
	{
		float x, y;

		do
		{
			float first = gEngfuncs.pfnRandomFloat(-0.5f, 0.5f);

			x = gEngfuncs.pfnRandomFloat(-0.5,0.5) + first;
			first = gEngfuncs.pfnRandomFloat(-0.5,0.5);
				y = gEngfuncs.pfnRandomFloat(-0.5,0.5) + first;
		} while (x*x+y*y > 1);

		Vector end = Vector(vecSrc) + (rightVector * (x * spread[0]) +
			Vector(vecDirShooting) + Vector(up) * (y * spread[1])) * distance;

		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );

		// Store off the old count
		gEngfuncs.pEventAPI->EV_PushPMStates();

		// Now add in all of the players.
		gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );

		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc, end, PM_NORMAL, -1, &tr );
		EV_HLDM_CheckTracer( idx, vecSrc, tr.endpos, forward, right, iBulletType, iTracerFreq, tracerCount );

		// do damage, paint decals
		if ( tr.fraction != 1)
		{
			if (gEngfuncs.PM_PointContents(tr.endpos, NULL) == CONTENTS_WATER &&
				gEngfuncs.PM_PointContents(vecSrc, NULL) != CONTENTS_WATER)
			{
				Vector origin(vecSrc);
				Vector direction = end - origin;

				VectorNormalize(direction);

				while (gEngfuncs.PM_PointContents(origin, NULL) == CONTENTS_EMPTY)
					origin = origin + direction;

				Vector below = origin - Vector(0, 0, 8);
				int waterEntity = gEngfuncs.PM_WaterEntity(below);

				if (waterEntity >= 0)
				{
					cl_entity_t *entity = gEngfuncs.GetEntityByIndex(waterEntity);

					if (entity && entity->model)
						origin.z += entity->curstate.scale * 24;
				}

				Vector zero(0, 0, 0);

				EV_HLDM_WaterSplash(origin, zero, false);
			}

			if (iBulletType != BULLET_PLAYER_BUCKSHOT)
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, end, iBulletType );
					EV_HLDM_DecalGunshot( &tr, iBulletType );
				}

		gEngfuncs.pEventAPI->EV_PopPMStates();
	}
}

void EV_StandardFireBullets(int idx, float *forward, float *right, float *up, int shots,
	float *src, float *aim, float *spread, float distance, int bulletType, int tracerFrequency)
{
	EV_HLDM_FireBullets(idx, forward, right, up, shots, src, aim, spread, distance,
		bulletType, tracerFrequency, &tracerCount[idx - 1]);
}

void TE_FireBullet(Vector vecSrc, Vector vecDir, int iShots, Vector vecSpread, int iTracerFreq, int iEntity)
{
	if (iShots <= 0)
		return;

	int ignore = iEntity >= 0 ? iEntity : 0;
	Vector angles, forward, right, up;
	Vector dir, end;
	pmtrace_t trace;

	VectorNormalize(vecDir);
	VectorAngles(vecDir, angles);
	AngleVectors(angles, forward, right, up);

	for (int shot = 1; shot <= iShots; shot++)
	{
		float x = gEngfuncs.pfnRandomFloat(-1, 1);
		float y = gEngfuncs.pfnRandomFloat(-1, 1);

		for (int i = 0; i < 3; i++)
			dir[i] = vecSpread[i] * right[i] * x + vecDir[i] + vecSpread[i] * up[i] * y;

		VectorNormalize(dir);
		end = vecSrc + dir * 8192;
		trace = *gEngfuncs.PM_TraceLine(vecSrc, end, PM_STUDIO_IGNORE, 2, ignore);

		if (shot % iTracerFreq)
		{
			if (trace.fraction != 1)
			{
				EV_HLDM_PlayTextureSound(ignore, &trace, vecSrc, end, BULLET_PLAYER_MP5);
				EV_HLDM_DecalGunshot(&trace, BULLET_PLAYER_MP5);
			}
		}
		else
			EV_CreateTracer(vecSrc, trace.endpos);
	}
}

int __MsgFunc_TracerDecal(const char *pszName, int iSize, void *pbuf)
{
	Vector forward(0, 0, 0);
	Vector right(0, 0, 0);
	Vector src, end;

	BEGIN_READ(pbuf, iSize);
	src.x = READ_COORD();
	src.y = READ_COORD();
	src.z = READ_COORD();
	end.x = READ_COORD();
	end.y = READ_COORD();
	end.z = READ_COORD();

	int tracerCount = READ_BYTE();

	READ_BYTE();
	EV_HLDM_CheckTracer(0, src, end, forward, right, BULLET_PLAYER_9MM, tracerCount, &tracerCount);

	if (gEngfuncs.PM_PointContents(end, NULL) == CONTENTS_WATER &&
		gEngfuncs.PM_PointContents(src, NULL) != CONTENTS_WATER)
	{
		Vector origin = src;
		Vector direction = end - src;

		VectorNormalize(direction);

		while (gEngfuncs.PM_PointContents(origin, NULL) == CONTENTS_EMPTY)
			origin = origin + direction;

		Vector below = origin - Vector(0, 0, 8);
		int waterEntity = gEngfuncs.PM_WaterEntity(below);

		if (waterEntity >= 0)
		{
			cl_entity_t *entity = gEngfuncs.GetEntityByIndex(waterEntity);

			if (entity && entity->model)
				origin.z += entity->curstate.scale * 16;
		}

		Vector zero(0, 0, 0);

		EV_HLDM_WaterSplash(origin, zero, false);
	}

	pmtrace_t trace;
	Vector direction = (end - src).Normalize();
	Vector traceEnd = end + direction * 10;

	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(src, traceEnd, PM_WORLD_ONLY, -1, &trace);
	EV_HLDM_DecalGunshot(&trace, BULLET_MONSTER_12MM);

	dlight_t *light = gEngfuncs.pEfxAPI->CL_AllocDlight(0);

	VectorCopy(src, light->origin);
	light->color.r = 255;
	light->color.g = 200;
	light->color.b = 80;
	light->radius = 64;
	light->decay = 0;
	light->die = gEngfuncs.GetClientTime() + 0.1;
	return 1;
}