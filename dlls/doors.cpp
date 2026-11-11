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

===== doors.cpp ========================================================

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "doors.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "InventoryRules.h"
#include "decals.h"
#include "server_sound.h"
#include "explode.h"

#define SF_DOOR_NO_LINK_CHECKING 1024

extern void SetMovedir(entvars_t* ev);
extern Vector g_vecAttackDir;

#define noiseMoving noise1
#define noiseArrived noise2

TYPEDESCRIPTION CBaseDoor::m_SaveData[] =
{
	DEFINE_FIELD( CBaseDoor, m_bHealthValue, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseDoor, m_bMoveSnd, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseDoor, m_bStopSnd, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseDoor, m_LockSoundsIndices.m_bLockedSound, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseDoor, m_LockSoundsIndices.m_bLockedSentence, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseDoor, m_LockSoundsIndices.m_bUnlockedSound, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseDoor, m_LockSoundsIndices.m_bUnlockedSentence, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseDoor, m_Material, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseDoor, m_Explosion, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseDoor, m_angle, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseDoor, m_iszGibModel, FIELD_STRING ),
	DEFINE_FIELD( CBaseDoor, m_iszSpawnObject, FIELD_STRING ),
	DEFINE_FIELD( CBaseDoor, m_instantBreakWeapon, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CBaseDoor, CBaseToggle );

const char *DoorMovingSound( int iSound )
{
	switch ( iSound )
	{
	case 1:		return "doors/doormove1.wav";
	case 2:		return "doors/doormove2.wav";
	case 3:		return "doors/doormove3.wav";
	case 4:		return "doors/doormove4.wav";
	case 5:		return "doors/doormove5.wav";
	case 6:		return "doors/doormove6.wav";
	case 7:		return "doors/doormove7.wav";
	case 8:		return "doors/doormove8.wav";
	case 9:		return "doors/doormove9.wav";
	case 10:	return "doors/doormove10.wav";
	default:	return "common/null.wav";
	}
}

const char *DoorStopSound( int iSound )
{
	switch ( iSound )
	{
	case 1:		return "doors/doorstop1.wav";
	case 2:		return "doors/doorstop2.wav";
	case 3:		return "doors/doorstop3.wav";
	case 4:		return "doors/doorstop4.wav";
	case 5:		return "doors/doorstop5.wav";
	case 6:		return "doors/doorstop6.wav";
	case 7:		return "doors/doorstop7.wav";
	case 8:		return "doors/doorstop8.wav";
	default:	return "common/null.wav";
	}
}

const char *LockedSentenceSound( int iSound )
{
	switch ( iSound )
	{
	case 1:		return "NA";	// access denied
	case 2:		return "ND";	// security lockout
	case 3:		return "NF";	// blast door
	case 4:		return "NFIRE";	// fire door
	case 5:		return "NCHEM";	// chemical door
	case 6:		return "NRAD";	// radiation door
	case 7:		return "NCON";	// gen containment
	case 8:		return "NH";	// maintenance door
	case 9:		return "NG";	// broken door
	default:	return NULL;
	}
}

const char *UnlockedSentenceSound( int iSound )
{
	switch ( iSound )
	{
	case 1:		return "EA";	// access granted
	case 2:		return "ED";	// security door
	case 3:		return "EF";	// blast door
	case 4:		return "EFIRE";	// fire door
	case 5:		return "ECHEM";	// chemical door
	case 6:		return "ERAD";	// radiation door
	case 7:		return "ECON";	// gen containment
	case 8:		return "EH";	// maintenance door
	default:	return NULL;
	}
}

// True when the key is handled, here or by the caller already
bool HandleLockSounds( locksound_t &sounds, LockSoundsIndices &indices, KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "locked_sound" ) )
		indices.m_bLockedSound = (BYTE) (int) atof( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "locked_sentence" ) )
		indices.m_bLockedSentence = (BYTE) (int) atof( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "unlocked_sound" ) )
		indices.m_bUnlockedSound = (BYTE) (int) atof( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "unlocked_sentence" ) )
		indices.m_bUnlockedSentence = (BYTE) (int) atof( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "locked_sound_override" ) )
		sounds.sLockedSound = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "locked_sentence_override" ) )
		sounds.sLockedSentence = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "unlocked_sound_override" ) )
		sounds.sUnlockedSound = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "unlocked_sentence_override" ) )
		sounds.sUnlockedSentence = ALLOC_STRING( pkvd->szValue );
	else
		return pkvd->fHandled != 0;

	pkvd->fHandled = TRUE;
	return true;
}

// An override from the map wins over the index
void SetLockSounds( CBaseEntity *pEntity, locksound_t &sounds, LockSoundsIndices &indices )
{
	if ( !sounds.sLockedSound && indices.m_bLockedSound )
	{
		const char *pszSound = ButtonSound( indices.m_bLockedSound );

		sounds.sLockedSound = pszSound ? MAKE_STRING( pszSound ) : 0;
	}

	if ( sounds.sLockedSound )
		PrecacheSound_Intercept( pEntity, STRING( sounds.sLockedSound ) );

	if ( !sounds.sUnlockedSound && indices.m_bUnlockedSound )
	{
		const char *pszSound = ButtonSound( indices.m_bUnlockedSound );

		sounds.sUnlockedSound = pszSound ? MAKE_STRING( pszSound ) : 0;
	}

	if ( sounds.sUnlockedSound )
		PrecacheSound_Intercept( pEntity, STRING( sounds.sUnlockedSound ) );

	if ( !sounds.sLockedSentence )
	{
		const char *pszSentence = LockedSentenceSound( indices.m_bLockedSentence );

		sounds.sLockedSentence = pszSentence ? MAKE_STRING( pszSentence ) : 0;
	}

	if ( !sounds.sUnlockedSentence )
	{
		const char *pszSentence = UnlockedSentenceSound( indices.m_bUnlockedSentence );

		sounds.sUnlockedSentence = pszSentence ? MAKE_STRING( pszSentence ) : 0;
	}
}

void CStartStopVars::TriggerStartedTargets( CBaseEntity *owner, EntityHandle<CBaseEntity> &activator )
{
	if ( m_iszFireOnStart )
		FireTargets( STRING( m_iszFireOnStart ), activator, owner,
		             (USE_TYPE) m_iFireOnStartTriggerState, 0 );
}

void CStartStopVars::TriggerStoppedTargets( CBaseEntity *owner, EntityHandle<CBaseEntity> &activator )
{
	if ( m_iszFireOnStop )
		FireTargets( STRING( m_iszFireOnStop ), activator, owner,
		             (USE_TYPE) m_iFireOnStopTriggerState, 0 );
}

void COpenClosableVars::TriggerMoveStartedTargets( CBaseEntity *owner, EntityHandle<CBaseEntity> &activator,
                                                   bool movingUp, bool startOpen )
{
	if ( m_iszFireOnOpening && movingUp != startOpen )
		FireTargets( STRING( m_iszFireOnOpening ), activator, owner,
		             (USE_TYPE) m_iFireOnOpeningTriggerState, 0 );

	if ( m_iszFireOnClosing && movingUp == startOpen )
		FireTargets( STRING( m_iszFireOnClosing ), activator, owner,
		             (USE_TYPE) m_iFireOnClosingTriggerState, 0 );
}

void COpenClosableVars::TriggerMoveFinishedTargets( CBaseEntity *owner, EntityHandle<CBaseEntity> &activator,
                                                    bool movingUp, bool startOpen )
{
	if ( m_iszFireOnOpened && movingUp != startOpen )
		FireTargets( STRING( m_iszFireOnOpened ), activator, owner,
		             (USE_TYPE) m_iFireOnOpenedTriggerState, 0 );

	if ( owner->pev->netname )
		m_iszFireOnClosed = owner->pev->netname;

	if ( m_iszFireOnClosed && movingUp == startOpen )
		FireTargets( STRING( m_iszFireOnClosed ), activator, owner,
		             (USE_TYPE) m_iFireOnClosedTriggerState, 0 );
}

CBaseDoor::CBaseDoor()
{
	for ( int i = 0; i < ARRAYSIZE( m_vecDamageDirection ); ++i )
		m_vecDamageDirection[i] = g_vecZero;

	m_StartStopVars.m_iszFireOnStart = 0;
	m_StartStopVars.m_iFireOnStartTriggerState = USE_TOGGLE;
	m_StartStopVars.m_iszFireOnStop = 0;
	m_StartStopVars.m_iFireOnStopTriggerState = USE_TOGGLE;
	m_OpenClosableVars.m_iszFireOnOpening = 0;
	m_OpenClosableVars.m_iFireOnOpeningTriggerState = USE_TOGGLE;
	m_OpenClosableVars.m_iszFireOnClosing = 0;
	m_OpenClosableVars.m_iFireOnClosingTriggerState = USE_TOGGLE;
	m_OpenClosableVars.m_iszFireOnOpened = 0;
	m_OpenClosableVars.m_iFireOnOpenedTriggerState = USE_TOGGLE;
	m_OpenClosableVars.m_iszFireOnClosed = 0;
	m_OpenClosableVars.m_iFireOnClosedTriggerState = USE_TOGGLE;
}

void CBaseDoor::UpdateOnRemove( void )
{
	CBaseEntity::UpdateOnRemove();
	EMIT_SOUND_DYN( edict(), CHAN_STATIC, "common/null.wav", 0, 0, SND_STOP, PITCH_NORM, 0, false, g_vecZero );
}

bool CBaseDoor::IsBreakable( void )
{
	return m_fBreakable && m_Material != matUnbreakableGlass;
}

int CBaseDoor::ShowHUDInfo( void )
{
	return m_fShowHUDInfo ? 1 : 0;
}

CAttackVectors *CBaseDoor::GetAttackVectors( void ) const
{
	return const_cast<CAttackVectors *>( &m_AttackVectors );
}

int CBaseDoor::DamageDecal( int damageType )
{
	if ( m_fBreakable )
	{
		if ( m_Material == matGlass )
			return DECAL_GLASSBREAK1 + RANDOM_LONG( 0, 2 );

		if ( m_Material == matUnbreakableGlass )
			return DECAL_BPROOF1;
	}

	return CBaseEntity::DamageDecal( damageType );
}

bool CBaseDoor::CheckAttacker( CBaseEntity *attacker )
{
	int classification = Classify();
	int attackerClass = CLASS_NONE;
	int relationship = 0;
	bool isClient = false;

	if ( attacker )
	{
		attackerClass = attacker->Classify();
		relationship = IRelationship( attacker, true );
		isClient = ( attacker->pev->flags & FL_CLIENT ) != 0;

		if ( isClient && m_fImmuneToClients )
			return true;
	}

	switch ( classification )
	{
		case CLASS_HUMAN_PASSIVE:
		case CLASS_PLAYER_ALLY:
			return isClient || attackerClass == CLASS_PLAYER_ALLY || attackerClass == CLASS_HUMAN_PASSIVE;

		case CLASS_TEAM1:
		case CLASS_TEAM2:
		case CLASS_TEAM3:
		case CLASS_TEAM4:
			if ( relationship > 0 )
				return false;

			return isClient || classification == attackerClass;

		default:
			return false;
	}
}

bool CBaseDoor::CanInstantBreak( CBaseEntity *attacker, int weapon )
{
	if ( !attacker || !attacker->IsNetClient() )
		return false;

	CBasePlayer *player = static_cast<CBasePlayer *>( attacker );
	CBasePlayerItem *active = player->m_pActiveItem;

	if ( !active )
		return false;

	ItemInfo info = {};

	active->GetItemInfo( &info );

	int weaponId = info.iId;

	if ( weaponId == WEAPON_CROWBAR )
	{
		CCrowbar *crowbar = dynamic_cast<CCrowbar *>( active->GetWeaponPtr() );

		// Door activation uses a separate ID for the electric crowbar
		if ( crowbar && crowbar->m_fElectric )
			weaponId = 19;
	}

	if ( weaponId == 19 && weapon == WEAPON_CROWBAR )
		return true;

	return weaponId > 0 && weaponId == weapon;
}

void CBaseDoor::LogDamage( float damage, Vector direction )
{
	if ( damage > 0 )
	{
		m_flDamage[m_iDamageIndex] = damage;
		m_flDamageTime[m_iDamageIndex] = gpGlobals->time;
		m_vecDamageDirection[m_iDamageIndex] = direction;
		m_iDamageIndex = ( m_iDamageIndex + 1 ) % 64;
	}
}

void CBaseDoor::DamageSound( void )
{
	Materials material = m_Material;
	int pitch = RANDOM_LONG( 0, 2 ) ? PITCH_NORM : 95 + RANDOM_LONG( 0, 34 );
	float volume = RANDOM_FLOAT( 0.75f, 1.0f );

	if ( material == matComputer && RANDOM_LONG( 0, 1 ) )
		material = matMetal;

	const char *sounds[6];
	int count;

	switch ( material )
	{
		case matComputer:
		case matGlass:
		case matUnbreakableGlass:
			sounds[0] = "debris/glass1.wav";
			sounds[1] = "debris/glass2.wav";
			sounds[2] = "debris/glass3.wav";
			count = 3;
			break;

		case matWood:
			sounds[0] = "debris/wood1.wav";
			sounds[1] = "debris/wood2.wav";
			sounds[2] = "debris/wood3.wav";
			count = 3;
			break;

		case matMetal:
			sounds[0] = "debris/metal1.wav";
			sounds[1] = "debris/metal3.wav";
			sounds[2] = "debris/metal2.wav";
			count = 2;
			break;

		case matFlesh:
			sounds[0] = "debris/flesh1.wav";
			sounds[1] = "debris/flesh2.wav";
			sounds[2] = "debris/flesh3.wav";
			sounds[3] = "debris/flesh5.wav";
			sounds[4] = "debris/flesh6.wav";
			sounds[5] = "debris/flesh7.wav";
			count = 6;
			break;

		case matCinderBlock:
		case matRocks:
			sounds[0] = "debris/concrete1.wav";
			sounds[1] = "debris/concrete2.wav";
			sounds[2] = "debris/concrete3.wav";
			count = 3;
			break;

		default:
			return;
	}

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, sounds[RANDOM_LONG( 0, count - 1 )], volume, ATTN_NORM,
	                0, pitch, 0, false, g_vecZero );
}

CBaseEntity *DoorDamageAttacker( entvars_t *attacker )
{
	edict_t *entity = attacker ? attacker->pContainingEntity : NULL;

	if ( !entity )
		entity = INDEXENT( 0 );

	return entity && !entity->free ? (CBaseEntity *) GET_PRIVATE( entity ) : NULL;
}

int CBaseDoor::TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType )
{
	if ( !IsBreakable() )
		return 0;

	CBaseEntity *attackerEntity = DoorDamageAttacker( attacker );

	if ( attackerEntity && attackerEntity->IsPlayer() )
		damage *= attackerEntity->MyMonsterPointer()->m_flEffectDamage;

	attackerEntity = DoorDamageAttacker( attacker );

	if ( CheckAttacker( attackerEntity ) )
		return 0;

	Vector direction = inflictor->origin - ( pev->absmin + pev->size * 0.5f );

	if ( attacker == inflictor && FClassnameIs( attacker, "player" ) && m_fInstantBreak
	     && CanInstantBreak( DoorDamageAttacker( attacker ), m_instantBreakWeapon ) )
		damage = pev->health;

	if ( m_fOnlyTrigger )
		return 0;

	g_vecAttackDir = direction.Normalize();

	if ( damageType & DMG_CLUB )
	{
		LogDamage( (float) ( damage * 0.2 ), g_vecAttackDir );
		damage += damage;
	}
	else if ( damageType & DMG_BULLET )
	{
		if ( damageType == DMG_BULLET && attacker == inflictor )
			LogDamage( damage, g_vecAttackDir );
		else if ( m_Material != matGlass )
			LogDamage( (float) ( damage * 0.1 ), g_vecAttackDir );
	}
	else
	{
		LogDamage( damageType & DMG_POISON ? (float) ( damage * 0.1 ) : damage, g_vecAttackDir );
	}

	if ( damageType & DMG_POISON )
		damage = (float) ( damage * 0.1 );

	if ( m_fExplosivesOnly && ( !( damageType & DMG_BLAST ) || FClassnameIs( inflictor, "weapon_egon" ) ) )
		damage = 0;

	pev->health -= damage;

	if ( pev->health <= 0 )
	{
		Killed( attacker, GIB_NORMAL );
		Die();

		if ( attacker )
			FireTargets( STRING( m_iszFireOnBreak ), DoorDamageAttacker( attacker ), this, USE_TOGGLE, 0 );

		return 0;
	}

	DamageSound();
	return 1;
}

void CBaseDoor::TraceAttack( entvars_t *attacker, float damage, Vector direction, TraceResult *trace, int damageType )
{
	CBaseEntity *attackerEntity = m_fRepairable ? DoorDamageAttacker( attacker ) : NULL;
	bool repairing = false;

	if ( attackerEntity && ( damageType & DMG_CLUB ) && attackerEntity->IsNetClient() )
	{
		CBasePlayer *player = dynamic_cast<CBasePlayer *>( attackerEntity );
		CBasePlayerItem *active = player ? (CBasePlayerItem *) player->m_pActiveItem : NULL;

		if ( active )
		{
			ItemInfo info = {};

			active->GetItemInfo( &info );

			if ( info.iId == WEAPON_PIPEWRENCH )
			{
				CWrench *wrench = dynamic_cast<CWrench *>( active->GetWeaponPtr() );

				repairing = wrench && wrench->m_iChargeBonus > 0;
			}
		}
	}

	if ( repairing && CheckAttacker( attackerEntity ) )
	{
		if ( pev->max_health > pev->health )
		{
			EMIT_SOUND_DYN( edict(), CHAN_AUTO, "buttons/latchunlocked2.wav", 1, 0.6f, 0, PITCH_NORM, 0, false, g_vecZero );
			TakeHealth( fabsf( damage ), damageType, 0 );

			if ( pev->health < 1 )
				pev->health = 1;

			pev->health = (float) (int) pev->health;
			pev->max_health = (float) (int) pev->max_health;
			pev->nextthink = gpGlobals->time;
		}

		return;
	}

	if ( RANDOM_LONG( 0, 1 ) )
	{
		if ( m_Material == matUnbreakableGlass )
		{
			UTIL_Ricochet( trace->vecEndPos, RANDOM_FLOAT( 0.5, 1.5 ) );
		}
		else if ( m_Material == matComputer )
		{
			UTIL_Sparks( trace->vecEndPos );

			float volume = RANDOM_FLOAT( 0.7, 1.0 );

			switch ( RANDOM_LONG( 0, 1 ) )
			{
				case 0:
					EMIT_SOUND_DYN( edict(), CHAN_VOICE, "buttons/spark5.wav", volume, ATTN_NORM, 0, PITCH_NORM, 0, false, g_vecZero );
					break;

				case 1:
					EMIT_SOUND_DYN( edict(), CHAN_VOICE, "buttons/spark6.wav", volume, ATTN_NORM, 0, PITCH_NORM, 0, false, g_vecZero );
					break;
			}
		}
	}

	CBaseDelay::TraceAttack( attacker, damage, direction, trace, damageType );
}

#define DOOR_SENTENCEWAIT	6
#define DOOR_SOUNDWAIT		3
#define BUTTON_SOUNDWAIT	0.5

// play door or button locked or unlocked sounds.
// pass in pointer to valid locksound struct.
// if flocked is true, play 'door is locked' sound,
// otherwise play 'door is unlocked' sound
// NOTE: this routine is shared by doors and buttons

void PlayLockSounds(entvars_t *pev, locksound_t *pls, int flocked, int fbutton)
{
	// LOCKED SOUND

	// CONSIDER: consolidate the locksound_t struct (all entries are duplicates for lock/unlock)
	// CONSIDER: and condense this code.
	float flsoundwait;

	if ( fbutton )
		flsoundwait = BUTTON_SOUNDWAIT;
	else
		flsoundwait = DOOR_SOUNDWAIT;

	if (flocked)
	{
		bool fplaysound = pls->sLockedSound && gpGlobals->time > pls->flwaitSound;
		bool fplaysentence = pls->sLockedSentence && !pls->bEOFLocked && gpGlobals->time > pls->flwaitSentence;

		float fvol;

		if ( fplaysound && fplaysentence )
			fvol = 0.25f;
		else
			fvol = 1.0;

		// if there is a locked sound, and we've debounced, play sound
		if (fplaysound)
		{
			EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, STRING(pls->sLockedSound), fvol, ATTN_NORM,
			                0, PITCH_NORM, 0, false, g_vecZero );
			pls->flwaitSound = gpGlobals->time + flsoundwait;
		}

		// if there is a sentence, we've not played all in list, and we've debounced, play sound
		if (fplaysentence)
		{
			// play next 'door locked' sentence in group
			int iprev = pls->iLockedSentence;

			pls->iLockedSentence = CServer_SoundEngine::GetSoundEngine()->PlaySentenceGroupSequential(ENT(pev), STRING(pls->sLockedSentence),
					  0.85, ATTN_NORM, 0, PITCH_NORM, pls->iLockedSentence, FALSE, -1 );
			pls->iUnlockedSentence = 0;

			// make sure we don't keep calling last sentence in list
			pls->bEOFLocked = iprev == pls->iLockedSentence;

			pls->flwaitSentence = gpGlobals->time + DOOR_SENTENCEWAIT;
		}
	}
	else
	{
		bool fplaysound = pls->sUnlockedSound && gpGlobals->time > pls->flwaitSound;
		bool fplaysentence = pls->sUnlockedSentence && !pls->bEOFUnlocked && gpGlobals->time > pls->flwaitSentence;

		float fvol;

		if ( fplaysound && fplaysentence )
			fvol = 0.25f;
		else
			fvol = 1.0;

		// play 'door unlocked' sound if set
		if (fplaysound)
		{
			EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, STRING(pls->sUnlockedSound), fvol, ATTN_NORM,
			                0, PITCH_NORM, 0, false, g_vecZero );
			pls->flwaitSound = gpGlobals->time + flsoundwait;
		}

		// play next 'door unlocked' sentence in group
		if (fplaysentence)
		{
			int iprev = pls->iUnlockedSentence;

			pls->iUnlockedSentence = CServer_SoundEngine::GetSoundEngine()->PlaySentenceGroupSequential(ENT(pev), STRING(pls->sUnlockedSentence),
					  0.85, ATTN_NORM, 0, PITCH_NORM, pls->iUnlockedSentence, FALSE, -1 );
			pls->iLockedSentence = 0;

			// make sure we don't keep calling last sentence in list
			pls->bEOFUnlocked = iprev == pls->iUnlockedSentence;
			pls->flwaitSentence = gpGlobals->time + DOOR_SENTENCEWAIT;
		}
	}
}

//
// Cache user-entity-field values until spawn is called.
//

void CBaseDoor::KeyValue( KeyValueData *pkvd )
{
	if ( HandleLockSounds( m_ls, m_LockSoundsIndices, pkvd ) && pkvd->fHandled )
		return;

	if ( FStrEq( pkvd->szKeyName, "m_iOpenFlags" ) )
	{
		m_iOpenFlags = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_fIgnoreTargetname" ) )
	{
		m_fIgnoreTargetname = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iObeyTriggerMode" ) )
	{
		m_iObeyTriggerMode = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "skin"))//skin is used for content type
	{
		pev->skin = (int) atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "movesnd"))
	{
		m_bMoveSnd = (BYTE) (int) atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "movesnd_loop" ) )
	{
		m_fMoveSoundLoop = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "stopsnd"))
	{
		m_bStopSnd = (BYTE) (int) atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "healthvalue"))
	{
		m_bHealthValue = (BYTE) (int) atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "WaveHeight"))
	{
		pev->scale = (float) ( atof(pkvd->szValue) * 0.125 );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonstart" ) )
	{
		m_StartStopVars.m_iszFireOnStart = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonstart_triggerstate" ) )
	{
		m_StartStopVars.m_iFireOnStartTriggerState = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonstop" ) )
	{
		m_StartStopVars.m_iszFireOnStop = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonstop_triggerstate" ) )
	{
		m_StartStopVars.m_iFireOnStopTriggerState = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonopening" ) )
	{
		m_OpenClosableVars.m_iszFireOnOpening = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonopening_triggerstate" ) )
	{
		m_OpenClosableVars.m_iFireOnOpeningTriggerState = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonclosing" ) )
	{
		m_OpenClosableVars.m_iszFireOnClosing = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonclosing_triggerstate" ) )
	{
		m_OpenClosableVars.m_iFireOnClosingTriggerState = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonopened" ) )
	{
		m_OpenClosableVars.m_iszFireOnOpened = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonopened_triggerstate" ) )
	{
		m_OpenClosableVars.m_iFireOnOpenedTriggerState = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonclosed" ) )
	{
		m_OpenClosableVars.m_iszFireOnClosed = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonclosed_triggerstate" ) )
	{
		m_OpenClosableVars.m_iFireOnClosedTriggerState = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonopen" ) )
	{
		m_OpenClosableVars.m_iszFireOnOpened = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fireonbreak" ) )
	{
		m_iszFireOnBreak = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "explosion" ) )
	{
		if ( atoi( pkvd->szValue ) == 1 )
			m_Explosion = expDirected;
		else
			m_Explosion = expRandom;

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "material" ) )
	{
		int material = atoi( pkvd->szValue );

		if ( material >= matGlass && material <= matNone )
			m_Material = (Materials) material;
		else
			m_Material = matWood;

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "deadmodel" ) )
	{
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "shards" ) )
	{
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gibmodel" ) )
	{
		m_iszGibModel = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "explodemagnitude" ) )
	{
		pev->impulse = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "weapon" ) )
	{
		int weapon = atoi( pkvd->szValue );

		if ( weapon == 1 || weapon == 19 || weapon == 20 )
			m_instantBreakWeapon = weapon;
		else
			m_instantBreakWeapon = 1;

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "displayname" ) )
	{
		m_iszDisplayName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "classify" ) )
	{
		SetClassificationSpecial( atoi( pkvd->szValue ), true );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "breakable" ) )
	{
		m_fBreakable = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "onlytrigger" ) )
	{
		m_fOnlyTrigger = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "breakontrigger" ) )
	{
		m_fBreakOnTrigger = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "repairable" ) )
	{
		m_fRepairable = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "showhudinfo" ) )
	{
		m_fShowHUDInfo = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "immunetoclients" ) )
	{
		m_fImmuneToClients = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "instantbreak" ) )
	{
		m_fInstantBreak = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "explosivesonly" ) )
	{
		m_fExplosivesOnly = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

void *CBaseDoor::GetKeyValue( const char *key, entvartype_s &type )
{
	if ( FStrEq( key, "explosion" ) )
	{
		type = VAR_INTEGER;
		return &m_Explosion;
	}

	if ( FStrEq( key, "material" ) )
	{
		type = VAR_INTEGER;
		return &m_Material;
	}

	if ( FStrEq( key, "gibmodel" ) )
	{
		type = VAR_STRING;
		return &m_iszGibModel;
	}

	if ( FStrEq( key, "explodemagnitude" ) )
	{
		type = VAR_INTEGER;
		return &pev->impulse;
	}

	if ( FStrEq( key, "weapon" ) )
	{
		type = VAR_INTEGER;
		return &m_instantBreakWeapon;
	}

	if ( FStrEq( key, "displayname" ) )
	{
		type = VAR_STRING;
		return &m_iszDisplayName;
	}

	return CBaseEntity::GetKeyValue( key, type );
}

/*QUAKED func_door (0 .5 .8) ? START_OPEN x DOOR_DONT_LINK TOGGLE
if two doors touch, they are assumed to be connected and operate as a unit.

TOGGLE causes the door to wait in both the start and end states for a trigger event.

START_OPEN causes the door to move to its destination when spawned, and operate in reverse.
It is used to temporarily or permanently close off an area when triggered (not usefull for
touch or takedamage doors).

"angle"         determines the opening direction
"targetname"	if set, no touch field will be spawned and a remote button or trigger
				field activates the door.
"health"        if set, door must be shot open
"speed"         movement speed (100 default)
"wait"          wait before returning (3 default, -1 = never return)
"lip"           lip remaining at end of move (8 default)
"dmg"           damage to inflict when blocked (2 default)
"sounds"
0)      no sound
1)      stone
2)      base
3)      stone chain
4)      screechy metal
*/

LINK_ENTITY_TO_CLASS( func_door, CBaseDoor );
//
// func_water - same as a door.
//
LINK_ENTITY_TO_CLASS( func_water, CBaseDoor );

void CBaseDoor::OnSetOriginByMap( void )
{
	RecalculateVectors();
	CBaseEntity::OnSetOriginByMap();
}

void CBaseDoor::RecalculateVectors( void )
{
	float distance = fabsf( pev->movedir.x * pev->size.x )
	                 + fabsf( pev->movedir.y * pev->size.y )
	                 + fabsf( pev->movedir.z * pev->size.z ) - m_flLip;

	m_vecPosition1 = pev->origin;
	m_vecPosition2 = m_vecPosition1 + pev->movedir * distance;

	CBaseToggle::RecalculateVectors();
}

void CBaseDoor::Spawn( )
{
	Precache();
	SetMovedir (pev);

	if ( pev->skin == 0 )
	{//normal door
		if ( FBitSet (pev->spawnflags, SF_DOOR_PASSABLE) )
			pev->solid		= SOLID_NOT;
		else
			pev->solid		= SOLID_BSP;
	}
	else
	{// special contents
		pev->solid		= SOLID_NOT;
		SetBits( pev->spawnflags, SF_DOOR_SILENT );	// water is silent for now
	}

	pev->movetype	= MOVETYPE_PUSH;
	UTIL_SetOrigin(pev, pev->origin);
	SetModel_Intercept( edict(), STRING(pev->model) );

	if (pev->speed == 0)
		pev->speed = 100;

	m_flTWidth = 0;
	RecalculateVectors();
	ASSERTSZ(m_vecPosition1 != m_vecPosition2, "door start/end positions are equal");
	if ( FBitSet (pev->spawnflags, SF_DOOR_START_OPEN) )
	{	// swap pos1 and pos2, put door at pos2
		UTIL_SetOrigin(pev, m_vecPosition2);
		m_vecPosition2 = m_vecPosition1;
		m_vecPosition1 = pev->origin;
	}

	m_toggle_state = TS_AT_BOTTOM;

	// if the door is flagged for USE button activation only, use NULL touch function
	if ( FBitSet ( pev->spawnflags, SF_DOOR_USE_ONLY ) )
	{
		SetTouch ( NULL );
	}
	else // touchable button
		SetTouch( &CBaseDoor::DoorTouch );

	if ( m_fBreakable )
	{
		if ( m_fOnlyTrigger )
			pev->takedamage = DAMAGE_NO;
		else
			pev->takedamage = DAMAGE_YES;

		m_angle = pev->angles.y;
		pev->angles.y = 0;
		pev->max_health = pev->health;

		if ( !m_iszDisplayName )
			m_iszDisplayName = MAKE_STRING( "Breakable Door" );

		if ( m_fRepairable && !m_iClassSelection )
		{
			m_fOverrideClass = true;
			m_iClassSelection = CLASS_PLAYER_ALLY;
		}

		if ( m_Material == matGlass )
			pev->playerclass = 1;

		if ( !IsBreakable() && pev->rendermode != kRenderNormal )
			pev->flags |= FL_WORLDBRUSH;

		if ( !m_instantBreakWeapon && m_fInstantBreak )
			m_instantBreakWeapon = WEAPON_CROWBAR;

		m_AttackVectors.CalculateAttackVectors( this );
	}
}

void CBaseDoor :: SetToggleState( int state )
{
	if ( state == TS_AT_TOP )
		UTIL_SetOrigin( pev, m_vecPosition2 );
	else
		UTIL_SetOrigin( pev, m_vecPosition1 );
}


void CBaseDoor::Precache( void )
{
	CBaseEntity::Precache();

	if ( !pev->noiseMoving )
		pev->noiseMoving = MAKE_STRING( DoorMovingSound( m_bMoveSnd ) );

	PrecacheSound_Intercept( this, STRING( pev->noiseMoving ) );

	if ( !pev->noiseArrived )
		pev->noiseArrived = MAKE_STRING( DoorStopSound( m_bStopSnd ) );

	PrecacheSound_Intercept( this, STRING( pev->noiseArrived ) );

	SetLockSounds( this, m_ls, m_LockSoundsIndices );

	const char *gibModel = NULL;

	switch ( m_Material )
	{
		case matGlass:
		case matUnbreakableGlass:
			gibModel = "models/glassgibs.mdl";
			PrecacheSound_Intercept( this, "debris/bustglass1.wav" );
			PrecacheSound_Intercept( this, "debris/bustglass2.wav" );
		break;
	case matWood:
			gibModel = "models/woodgibs.mdl";
			PrecacheSound_Intercept( this, "debris/bustcrate1.wav" );
			PrecacheSound_Intercept( this, "debris/bustcrate2.wav" );
		break;
	case matMetal:
			gibModel = "models/metalplategibs.mdl";
			PrecacheSound_Intercept( this, "debris/bustmetal1.wav" );
			PrecacheSound_Intercept( this, "debris/bustmetal2.wav" );
		break;
	case matFlesh:
			gibModel = "models/fleshgibs.mdl";
			PrecacheSound_Intercept( this, "debris/bustflesh1.wav" );
			PrecacheSound_Intercept( this, "debris/bustflesh2.wav" );
		break;
	case matCinderBlock:
			gibModel = "models/cindergibs.mdl";
			PrecacheSound_Intercept( this, "debris/bustconcrete1.wav" );
			PrecacheSound_Intercept( this, "debris/bustconcrete2.wav" );
		break;
	case matCeilingTile:
			gibModel = "models/ceilinggibs.mdl";
			PrecacheSound_Intercept( this, "debris/bustceiling.wav" );
		break;
	case matComputer:
			PrecacheSound_Intercept( this, "buttons/spark5.wav" );
			PrecacheSound_Intercept( this, "buttons/spark6.wav" );
			gibModel = "models/computergibs.mdl";
			PrecacheSound_Intercept( this, "debris/bustmetal1.wav" );
			PrecacheSound_Intercept( this, "debris/bustmetal2.wav" );
		break;
	case matRocks:
			gibModel = "models/rockgibs.mdl";
			PrecacheSound_Intercept( this, "debris/bustconcrete1.wav" );
			PrecacheSound_Intercept( this, "debris/bustconcrete2.wav" );
		break;
	default:
			break;
	}

	CBreakable::MaterialSoundPrecache( m_Material );

	if ( m_iszGibModel )
		gibModel = STRING( m_iszGibModel );

	m_idShard = UTIL_PrecacheModel( this, gibModel );
	}

//
// Doors not tied to anything (e.g. button, another door) can be touched, to make them activate.
//
void CBaseDoor::DoorTouch( CBaseEntity *pevToucher )
{
	// AdamR: Inventory filtering rules
	if (!m_pInventoryRules->CanUseTrigger(pevToucher, false ) )
	{
		if ( !UTIL_IsMasterTriggered(m_sMaster, pevToucher ))
		PlayLockSounds(pev, &m_ls, TRUE, TRUE );

		return;
	}

	bool canOpen = ( ( m_iOpenFlags & 1 ) && FClassnameIs( pevToucher->pev, "func_pushable" ) ) || ( !( m_iOpenFlags & 2 ) && pevToucher->IsNetClient() ) || ( ( m_iOpenFlags & 4 ) && !pevToucher->IsNetClient() && !FClassnameIs( pevToucher->pev, "func_pushable" ) );

	// If door is somebody's target, then touching does nothing.
	// You have to activate the owner (e.g. button).

	if (!canOpen )
		return;

	if (pev->targetname && !m_fIgnoreTargetname )
	{
		// play locked sound
		PlayLockSounds(pev, &m_ls, TRUE, FALSE);
		return;
	}

	m_hActivator = pevToucher;// remember who activated the door

	if (DoorActivate( ))
		SetTouch( NULL ); // Temporarily disable the touch function, until movement is finished.
}


//
// Used by SUB_UseTargets, when a door is the target of a button.
//
void CBaseDoor::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// AdamR: Inventory filtering rules
	if ( !m_pInventoryRules->CanUseTrigger(pActivator))
	{
		if ( !UTIL_IsMasterTriggered( m_sMaster, pActivator ) )
			PlayLockSounds( pev, &m_ls, TRUE, TRUE );

		return;
	}

	if ( IsBreakable() && m_fBreakOnTrigger )
	{
		pev->angles.y = m_angle;
		UTIL_MakeVectors( pev->angles );
		g_vecAttackDir = -gpGlobals->v_up;
		pev->health = -50;
		m_fBrokenByTrigger = true;
		Die();
		FireTargets( STRING( m_iszFireOnBreak ), pActivator, this, USE_TOGGLE, 0 );
		return;
	}

	m_hActivator = pActivator;
	// if not ready to be used, ignore "use" command.
	if ( m_iObeyTriggerMode )
	{
		if ( m_toggle_state == TS_AT_BOTTOM )
		{
			if ( useType == USE_OFF )
				return;

			DoorActivate();
			return;
		}

		if ( m_toggle_state == TS_AT_TOP && useType == USE_ON )
			return;
	}

	if ( m_toggle_state == TS_AT_BOTTOM || ( FBitSet(pev->spawnflags, SF_DOOR_NO_AUTO_RETURN) && m_toggle_state == TS_AT_TOP) )
	{
		DoorActivate();
}
	else if ( m_iObeyTriggerMode == 2 && ( ( m_toggle_state == TS_GOING_DOWN && useType == USE_ON ) || ( m_toggle_state == TS_GOING_UP && useType == USE_OFF ) || ( g_iMapForGameVersion >= 517 && useType == USE_TOGGLE ) ) )
	{
		if ( m_toggle_state != TS_AT_TOP )
			DoorChangeDirection();
	}
}

//
// Causes the door to "do its thing", i.e. start moving, and cascade activation.
//
int CBaseDoor::DoorActivate( )
{
	if (!UTIL_IsMasterTriggered(m_sMaster, m_hActivator))
		return 0;

	pev->framerate = 1;

	if (FBitSet(pev->spawnflags, SF_DOOR_NO_AUTO_RETURN) && m_toggle_state == TS_AT_TOP)
	{// door should close
		DoorGoDown();
	}
	else
	{// door should open

		if ( m_hActivator != NULL && m_hActivator->IsNetClient() )
		{// give health if player opened the door (medikit)
		// VARS( m_eoActivator )->health += m_bHealthValue;

			m_hActivator->TakeHealth( m_bHealthValue, DMG_GENERIC );

		}

		// play door unlock sounds
		PlayLockSounds(pev, &m_ls, FALSE, FALSE);

		DoorGoUp();
	}

	return 1;
}

extern Vector VecBModelOrigin( entvars_t* pevBModel );

//
// Starts the door going to its "up" position (simply ToggleData->vecPosition2).
//
void CBaseDoor::DoorGoUp( void )
{
	pev->framerate = 1;

	// emit door moving and stop sounds on CHAN_STATIC so that the multicast doesn't
	// filter them out and leave a client stuck with looping door sounds!
	if ( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ) && ( pev->noiseMoving || m_bMoveSnd ) )
		EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING(pev->noiseMoving), 1, ATTN_NORM,
		                m_fMoveSoundLoop ? 2048 : 0, PITCH_NORM, 0, false, g_vecZero );

	m_toggle_state = TS_GOING_UP;

	SetMoveDone( &CBaseDoor::DoorHitTop );
	if ( FClassnameIs(pev, "func_door_rotating"))		// !!! BUGBUG Triggered doors don't work with this yet
	{
		float	sign = 1;

		if ( m_hActivator != NULL && !FBitSet( pev->spawnflags, SF_DOOR_ONEWAY ) && pev->movedir.y ) 		// Y axis rotation, move away from the player
			{
			entvars_t *pevActivator = m_hActivator->pev;
			Vector vec = pevActivator->origin - pev->origin;
				Vector angles = pevActivator->angles;
				angles.x = 0;
				angles.z = 0;
				UTIL_MakeVectors (angles);

			Vector forward;

			if ( pevActivator->velocity.Length() == 0 )
			{
				UTIL_MakeVectors ( pevActivator->angles );
				forward = gpGlobals->v_forward;
			}
			else
			{
				forward = pevActivator->velocity.Normalize();
			}

			Vector vnext = pevActivator->origin + forward * 10 - pev->origin;
				if (vec.x*vnext.y - vec.y*vnext.x < 0 )
					sign = -1;
		}
		AngularMove(m_vecAngle2*sign, pev->speed);
	}
	else
	{
		LinearMove(m_vecPosition2, pev->speed);
}

	EntityHandle<CBaseEntity> pevActivator( m_hActivator );

	m_StartStopVars.TriggerStartedTargets( this, pevActivator );

	pevActivator = (CBaseEntity *) m_hActivator;
	m_OpenClosableVars.TriggerMoveStartedTargets( this, pevActivator, true,
	                                              FBitSet( pev->spawnflags, SF_DOOR_START_OPEN ) != 0 );
}


//
// The door has reached the "up" position.  Either go back down, or wait for another activation.
//
void CBaseDoor::DoorHitTop( void )
{
	CheckDoorFriends();
	pev->framerate = 0;

	if ( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ) )
	{
		if ( pev->noiseMoving || m_bMoveSnd )
			EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING(pev->noiseMoving), 0, 0, SND_STOP, PITCH_NORM, 0, false, g_vecZero );

		if ( pev->noiseArrived || m_bStopSnd )
			EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING(pev->noiseArrived), 1, ATTN_NORM, 0, PITCH_NORM, 0, false, g_vecZero );
	}

	m_toggle_state = TS_AT_TOP;

	// toggle-doors don't come down automatically, they wait for refire.
	if (FBitSet(pev->spawnflags, SF_DOOR_NO_AUTO_RETURN))
	{
		// Re-instate touch method, movement is complete
		if ( !FBitSet ( pev->spawnflags, SF_DOOR_USE_ONLY ) )
			SetTouch( &CBaseDoor::DoorTouch );
	}
	else
	{
		// In flWait seconds, DoorGoDown will fire, unless wait is -1, then door stays open
		pev->nextthink = pev->ltime + m_flWait;
		SetThink( &CBaseDoor::DoorGoDown );

		if ( m_flWait == -1 )
			pev->nextthink = -1;
		}

	EntityHandle<CBaseEntity> activator( m_hActivator );

	m_StartStopVars.TriggerStoppedTargets( this, activator );

	activator = (CBaseEntity *) m_hActivator;
	m_OpenClosableVars.TriggerMoveFinishedTargets( this, activator, true,
	                                               FBitSet(pev->spawnflags, SF_DOOR_START_OPEN ) != 0 );

	SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 ); // this isn't finished
}


//
// Starts the door going to its "down" position (simply ToggleData->vecPosition1).
//
void CBaseDoor::DoorGoDown( void )
{
	if ( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ) && ( pev->noiseMoving || m_bMoveSnd ) )
		EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING(pev->noiseMoving), 1, ATTN_NORM,
		                0, PITCH_NORM, 0, false, g_vecZero );

	pev->framerate = 1;
	m_toggle_state = TS_GOING_DOWN;

	SetMoveDone( &CBaseDoor::DoorHitBottom );
	if ( FClassnameIs(pev, "func_door_rotating"))//rotating door
		AngularMove( m_vecAngle1, pev->speed);
	else
		LinearMove( m_vecPosition1, pev->speed);

	EntityHandle<CBaseEntity> activator( m_hActivator );

	m_StartStopVars.TriggerStartedTargets( this, activator );

	activator = (CBaseEntity *) m_hActivator;
	m_OpenClosableVars.TriggerMoveStartedTargets( this, activator, false,
	                                              FBitSet( pev->spawnflags, SF_DOOR_START_OPEN ) != 0 );
}

//
// The door has reached the "down" position.  Back to quiescence.
//
void CBaseDoor::DoorHitBottom( void )
{
	CheckDoorFriends();
	pev->framerate = 0;

	if ( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ) )
	{
		if ( pev->noiseMoving || m_bMoveSnd )
			EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING(pev->noiseMoving), 0, 0, SND_STOP, PITCH_NORM, 0, false, g_vecZero );

		if ( pev->noiseArrived || m_bStopSnd )
			EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING(pev->noiseArrived), 1, ATTN_NORM, 0, PITCH_NORM, 0, false, g_vecZero );
	}

	m_toggle_state = TS_AT_BOTTOM;

	// Re-instate touch method, cycle is complete
	if ( FBitSet ( pev->spawnflags, SF_DOOR_USE_ONLY ) )
		SetTouch ( NULL );
	else // touchable door
		SetTouch( &CBaseDoor::DoorTouch );

	SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );

	EntityHandle<CBaseEntity> activator( m_hActivator );

	m_StartStopVars.TriggerStoppedTargets( this, activator );

	activator = (CBaseEntity *) m_hActivator;
	m_OpenClosableVars.TriggerMoveFinishedTargets( this, activator, false,
	                                               FBitSet(pev->spawnflags, SF_DOOR_START_OPEN ) != 0 );
}

void CBaseDoor::Blocked( CBaseEntity *pOther )
{
	if ( !BlockedByEntity( pOther, pev->dmg ) || m_flTWidth > gpGlobals->time )
		return;

	// if a door has a negative wait, it would never come back if blocked,
	// so let it just squash the object to death real fast

	if (m_flWait >= 0)
	{
		m_flTWidth = gpGlobals->time + 0.2f;

		if ( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ) && ( pev->noiseMoving || m_bMoveSnd ) )
			EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING( pev->noiseMoving ), 0, 0, SND_STOP, PITCH_NORM, 0, false, g_vecZero );

		if (m_toggle_state == TS_GOING_DOWN)
			DoorGoUp();
		else
			DoorGoDown();
		}

	if ( !pev->targetname )
		return;

	edict_t *pentTarget = NULL;

	for (;;)
		{
			pentTarget = FIND_ENTITY_BY_STRING( pentTarget, "targetname", STRING(pev->targetname));

			if ( pentTarget && VARS( pentTarget ) == pev )
			continue;

		if ( !pentTarget || pentTarget->free || !ENTINDEX(pentTarget))
					break;

				if ( !FClassnameIs ( pentTarget, "func_door" ) && !FClassnameIs ( pentTarget, "func_door_rotating" ) )
			continue;

		CBaseDoor *pDoor = GetClassPtr( (CBaseDoor *) VARS(pentTarget) );

					if ( pDoor->m_flWait < 0 )
			continue;

		if (pDoor->pev->velocity == pev->velocity && pDoor->pev->avelocity == pev->avelocity )
						{
							// this is the most hacked, evil, bastardized thing I've ever seen. kjb
							if ( FClassnameIs ( pentTarget, "func_door" ) )
							{// set origin to realign normal doors
								pDoor->pev->origin = pev->origin;
								pDoor->pev->velocity = g_vecZero;// stop!
							}
							else
							{// set angles to realign rotating doors
								pDoor->pev->angles = pev->angles;
								pDoor->pev->avelocity = g_vecZero;
							}
						}

						if ( pDoor->m_toggle_state == TS_GOING_DOWN)
							pDoor->DoorGoUp();
						else if ( pDoor->m_toggle_state == TS_GOING_UP )
			pDoor->DoorGoDown();

		pDoor->m_flTWidth = gpGlobals->time + 0.2f;
	}
}

bool CBaseDoor::DoorChangeDirection( void )
{
	if ( (unsigned int) m_toggle_state < 2 )
		return false;

	if ( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ) )
		EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING( pev->noiseMoving ), 0, 0, SND_STOP, PITCH_NORM, 0, false, g_vecZero );

	if ( m_toggle_state == TS_GOING_DOWN )
		DoorGoUp();
	else
		DoorGoDown();

	return true;
}

void CBaseDoor::CheckDoorFriends( void )
{
	if ( pev->spawnflags & SF_DOOR_NO_LINK_CHECKING )
		return;

	edict_t *target = NULL;

	for ( ;; )
	{
		target = FIND_ENTITY_BY_STRING( target, "targetname", STRING( pev->targetname ) );

		if ( target && VARS( target ) == pev )
			continue;

		if ( !target || target->free || !ENTINDEX( target ) )
			break;

		if ( !FClassnameIs( target, "func_door" ) && !FClassnameIs( target, "func_door_rotating" ) )
			continue;

		CBaseDoor *door = GetClassPtr( (CBaseDoor *) VARS( target ) );

		if ( UTIL_IsMasterTriggered( door->m_sMaster, door->m_hActivator ) && door->m_toggle_state != m_toggle_state && !( door->m_toggle_state == TS_AT_BOTTOM && m_toggle_state == TS_GOING_DOWN ) && !( door->m_toggle_state == TS_AT_TOP && m_toggle_state == TS_GOING_UP ) )
		{
			if ( door->m_toggle_state == TS_AT_BOTTOM )
				door->DoorGoUp();
			else
				door->DoorGoDown();
		}
	}
}

void CBaseDoor::Die( void )
{
	int pitch = 95 + RANDOM_LONG( 0, 29 );

	if ( pitch >= 98 && pitch <= 102 )
		pitch = PITCH_NORM;

	float damage = fabsf( pev->health );
	float volume = (float) ( RANDOM_FLOAT( 0.85, 1.0 ) + damage / 100.0 );
	float materialScale = 1;
	int flags = 0;
	const char *sound = NULL;

	volume = fminf( 1, volume );

	switch ( m_Material )
	{
		case matGlass:
			sound = RANDOM_LONG( 0, 1 ) ? "debris/bustglass2.wav" : "debris/bustglass1.wav";
			materialScale = 0.6f;
			flags = BREAK_GLASS;
			break;

		case matWood:
			sound = RANDOM_LONG( 0, 1 ) ? "debris/bustcrate2.wav" : "debris/bustcrate1.wav";
			materialScale = 0.8f;
			flags = BREAK_WOOD;
			break;

		case matComputer:
		case matMetal:
			sound = RANDOM_LONG( 0, 1 ) ? "debris/bustmetal2.wav" : "debris/bustmetal1.wav";
			flags = BREAK_METAL;
			break;

		case matFlesh:
			sound = RANDOM_LONG( 0, 1 ) ? "debris/bustflesh2.wav" : "debris/bustflesh1.wav";
			materialScale = 0.9f;
			flags = BREAK_FLESH;
			break;

		case matCinderBlock:
		case matRocks:
			sound = RANDOM_LONG( 0, 1 ) ? "debris/bustconcrete2.wav" : "debris/bustconcrete1.wav";
			materialScale = 0.5f;
			flags = BREAK_CONCRETE;
			break;

		case matCeilingTile:
			sound = "debris/bustceiling.wav";
			materialScale = 0.8f;
			break;

		default:
			break;
	}

	if ( sound )
		EMIT_SOUND_DYN( edict(), CHAN_VOICE, sound, volume, ATTN_NORM, 0, pitch, 0, false, g_vecZero );

	float totalDamage = damage;
	float largestDamage = damage;
	Vector velocity = g_vecZero;

	if ( m_Explosion == expDirected && !m_fBrokenByTrigger )
	{
		float earliestTime = gpGlobals->time - 0.3f;
		Vector direction = g_vecZero;

		totalDamage = 0;
		largestDamage = 0;

		for ( int i = 0; i < ARRAYSIZE( m_flDamage ); ++i )
		{
			if ( m_flDamageTime[i] >= earliestTime )
			{
				totalDamage += m_flDamage[i];

				if ( m_flDamage[i] > largestDamage )
				{
					largestDamage = m_flDamage[i];
					direction = m_vecDamageDirection[i];
				}
			}
		}

		float magnitude = (float) fmin( fmax( totalDamage * 2.3 * materialScale, 60.0 ), 1300.0 );

		velocity = direction * -magnitude;
	}

	velocity.z = (float) fmin( fmin( pev->impulse * 0.8, 400.0 ) + velocity.z, 1300.0 );
	totalDamage += (float) pev->impulse;
	largestDamage += (float) pev->impulse;

	Vector center = pev->origin + ( pev->mins + pev->maxs ) * 0.5f;
	float diagonal = pev->size.Length();
	float sizeScale = 1;

	if ( diagonal < 165 )
		sizeScale = diagonal > 80 ? ( 165 - diagonal ) / 56.666668f + 1 : 2.5f;

	// Test the scaled component without scaling the saved value
	for ( int i = 0; i < 3; ++i )
	{
		if ( velocity[i] * sizeScale >= 1300 )
			velocity[i] = 1300;
	}

	float randomization = fabsf( ( sqrtf( fabsf( totalDamage ) * 250.0f ) + largestDamage ) * sqrtf( sizeScale ) ) / 15.0f;

	randomization *= sqrtf( materialScale );
	randomization = fminf( fmaxf( 6, randomization ), 50 );

	double lifetime = fmin( fmax( velocity.Length() / 20.0 + 15.0, 25.0 ), 40.0 );

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, center );
	WRITE_BYTE( TE_BREAKMODEL );
	WRITE_COORD( center.x );
	WRITE_COORD( center.y );
	WRITE_COORD( center.z );
	WRITE_COORD( pev->size.x );
	WRITE_COORD( pev->size.y );
	WRITE_COORD( pev->size.z );
	WRITE_COORD( velocity.x );
	WRITE_COORD( velocity.y );
	WRITE_COORD( velocity.z );
	WRITE_BYTE( (int) randomization );
	WRITE_SHORT( m_idShard );
	WRITE_BYTE( 0 );
	WRITE_BYTE( (int) lifetime );
	WRITE_BYTE( flags );
	MESSAGE_END();

	Vector minimum = pev->absmin;
	Vector maximum = pev->absmax;

	minimum.z = pev->absmax.z;
	maximum.z += 8;

	CBaseEntity *entities[256];
	int count = UTIL_EntitiesInBox( entities, ARRAYSIZE( entities ), minimum, maximum, FL_ONGROUND );

	for ( int i = 0; i < count; ++i )
	{
		entities[i]->pev->flags &= ~FL_ONGROUND;
		entities[i]->pev->groundentity = NULL;
	}

	pev->targetname = 0;
	pev->solid = SOLID_NOT;
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = pev->ltime + 0.1f;

	if ( m_iszSpawnObject )
		Create( STRING( m_iszSpawnObject ), VecBModelOrigin( pev ), pev->angles, false, edict() );

	if ( pev->impulse > 0 )
		CreateBreakableExplosion( Center(), pev->angles, edict(), pev->impulse );
}

/*QUAKED FuncRotDoorSpawn (0 .5 .8) ? START_OPEN REVERSE
DOOR_DONT_LINK TOGGLE X_AXIS Y_AXIS
if two doors touch, they are assumed to be connected and operate as
a unit.

TOGGLE causes the door to wait in both the start and end states for
a trigger event.

START_OPEN causes the door to move to its destination when spawned,
and operate in reverse.  It is used to temporarily or permanently
close off an area when triggered (not usefull for touch or
takedamage doors).

You need to have an origin brush as part of this entity.  The
center of that brush will be
the point around which it is rotated. It will rotate around the Z
axis by default.  You can
check either the X_AXIS or Y_AXIS box to change that.

"distance" is how many degrees the door will be rotated.
"speed" determines how fast the door moves; default value is 100.

REVERSE will cause the door to rotate in the opposite direction.

"angle"		determines the opening direction
"targetname" if set, no touch field will be spawned and a remote
button or trigger field activates the door.
"health"	if set, door must be shot open
"speed"		movement speed (100 default)
"wait"		wait before returning (3 default, -1 = never return)
"dmg"		damage to inflict when blocked (2 default)
"sounds"
0)	no sound
1)	stone
2)	base
3)	stone chain
4)	screechy metal
*/
class CRotDoor : public CBaseDoor
{
public:
	void Spawn( void );
	virtual void SetToggleState( int state );
	virtual void OnSetOriginByMap( void );
	virtual void RecalculateVectors( void );
};

LINK_ENTITY_TO_CLASS( func_door_rotating, CRotDoor );

void CRotDoor::OnSetOriginByMap( void )
{
	CBaseEntity::OnSetOriginByMap();
}

void CRotDoor::RecalculateVectors( void )
{
	m_vecAngle1 = pev->angles;
	m_vecAngle2 = pev->angles + pev->movedir * m_flMoveDistance;

	CBaseToggle::RecalculateVectors();
}

void CRotDoor::Spawn( void )
{
	Precache();
	// set the axis of rotation
	CBaseToggle::AxisDir( pev );

	// check for clockwise rotation
	if ( FBitSet (pev->spawnflags, SF_DOOR_ROTATE_BACKWARDS) )
		pev->movedir = pev->movedir * -1;

	//m_flWait			= 2; who the hell did this? (sjb)

	if ( FBitSet (pev->spawnflags, SF_DOOR_PASSABLE) )
		pev->solid		= SOLID_NOT;
	else
		pev->solid		= SOLID_BSP;

	pev->movetype	= MOVETYPE_PUSH;
	UTIL_SetOrigin(pev, pev->origin);
	SetModel_Intercept( edict(), STRING(pev->model) );
	UTIL_SetSize( pev, pev->mins, pev->maxs );

	if (pev->speed == 0)
		pev->speed = 100;

	RecalculateVectors();

	ASSERTSZ( m_vecAngle1 != m_vecAngle2, "rotating door start/end positions are equal" );

// DOOR_START_OPEN is to allow an entity to be lighted in the closed position
// but spawn in the open position
	if ( FBitSet (pev->spawnflags, SF_DOOR_START_OPEN) )
	{	// swap pos1 and pos2, put door at pos2, invert movement direction
		pev->angles = m_vecAngle2;
		SetModel_Intercept( edict(), STRING( pev->model ) );
		UTIL_SetSize( pev, pev->mins, pev->maxs );
		Vector vecSav = m_vecAngle1;
		m_vecAngle2 = m_vecAngle1;
		m_vecAngle1 = vecSav;
		pev->movedir = pev->movedir * -1;
	}

	m_toggle_state = TS_AT_BOTTOM;

	if ( FBitSet ( pev->spawnflags, SF_DOOR_USE_ONLY ) )
	{
		SetTouch ( NULL );
	}
	else // touchable button
		SetTouch( &CRotDoor::DoorTouch );

	if ( m_fBreakable )
	{
		if ( m_fOnlyTrigger )
			pev->takedamage = DAMAGE_NO;
		else
			pev->takedamage = DAMAGE_YES;

		m_angle = pev->angles.y;
		pev->angles.y = 0;
		pev->max_health = pev->health;

		if ( !m_iszDisplayName )
			m_iszDisplayName = MAKE_STRING( "Breakable Door" );

		if ( m_fRepairable && !m_iClassSelection )
		{
			m_fOverrideClass = true;
			m_iClassSelection = CLASS_PLAYER_ALLY;
		}

		if ( m_Material == matGlass )
			pev->playerclass = 1;

		if ( !IsBreakable() && pev->rendermode != kRenderNormal )
			pev->flags |= FL_WORLDBRUSH;

		if ( !m_instantBreakWeapon && m_fInstantBreak )
			m_instantBreakWeapon = WEAPON_CROWBAR;

		m_AttackVectors.CalculateAttackVectors( this );
	}
}


void CRotDoor :: SetToggleState( int state )
{
	if ( state == TS_AT_TOP )
		pev->angles = m_vecAngle2;
	else
		pev->angles = m_vecAngle1;

	UTIL_SetOrigin( pev, pev->origin );
}


class CMomentaryDoor : public CBaseToggle
{
public:
	void	Spawn( void );
	void Precache( void );

	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void OnSetOriginByMap( void );
	virtual void RecalculateVectors( void );
	virtual void UpdateOnRemove( void );
	virtual int ShowHUDInfo( void )
	{
		return 0;
	}

	virtual int	ObjectCaps( void ) { return CBaseToggle :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void EXPORT DoorMoveDone( void );

	BYTE	m_bMoveSnd;			// sound a door makes while moving
	BYTE	m_bStopSnd;			// sound a door makes when it stops
};

LINK_ENTITY_TO_CLASS( momentary_door, CMomentaryDoor );

TYPEDESCRIPTION	CMomentaryDoor::m_SaveData[] = 
{
	DEFINE_FIELD( CMomentaryDoor, m_bMoveSnd, FIELD_CHARACTER ),
	DEFINE_FIELD( CMomentaryDoor, m_bStopSnd, FIELD_CHARACTER ),
};

IMPLEMENT_SAVERESTORE( CMomentaryDoor, CBaseToggle );

void CMomentaryDoor::UpdateOnRemove( void )
{
	CBaseEntity::UpdateOnRemove();

	EMIT_SOUND_DYN( edict(), CHAN_STATIC, "common/null.wav", 0, 0, SND_STOP,
	                PITCH_NORM, 0, false, g_vecZero );
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "common/null.wav", 0, 0, SND_STOP,
	                PITCH_NORM, 0, false, g_vecZero );
}

void CMomentaryDoor::OnSetOriginByMap( void )
{
	RecalculateVectors();
	CBaseEntity::OnSetOriginByMap();
}

void CMomentaryDoor::RecalculateVectors( void )
{
	float distance = fabsf( pev->movedir.x * pev->size.x )
	                 + fabsf( pev->movedir.y * pev->size.y )
	                 + fabsf( pev->movedir.z * pev->size.z ) - m_flLip;

	m_vecPosition1 = pev->origin;
	m_vecPosition2 = m_vecPosition1 + pev->movedir * distance;

	CBaseToggle::RecalculateVectors();
}

void CMomentaryDoor::Spawn( void )
{
	Precache();

	SetMovedir (pev);

	pev->solid		= SOLID_BSP;
	pev->movetype	= MOVETYPE_PUSH;

	UTIL_SetOrigin(pev, pev->origin);
	SET_MODEL( ENT(pev), STRING(pev->model) );
	
	if (pev->speed == 0)
		pev->speed = 100;
	if (pev->dmg == 0)
		pev->dmg = 2;

	RecalculateVectors();
	ASSERTSZ(m_vecPosition1 != m_vecPosition2, "door start/end positions are equal");

	if ( FBitSet (pev->spawnflags, SF_DOOR_START_OPEN) )
	{	// swap pos1 and pos2, put door at pos2
		UTIL_SetOrigin(pev, m_vecPosition2);
		m_vecPosition2 = m_vecPosition1;
		m_vecPosition1 = pev->origin;
	}
	SetTouch( NULL );
}

void CMomentaryDoor::Precache( void )
{
	CBaseEntity::Precache();

// set the door's "in-motion" sound
	if ( !pev->noiseMoving )
		pev->noiseMoving = MAKE_STRING( DoorMovingSound( m_bMoveSnd ) );

	PrecacheSound_Intercept( this, STRING( pev->noiseMoving ) );

	if ( !pev->noiseArrived )
		pev->noiseArrived = MAKE_STRING( DoorStopSound( m_bStopSnd ) );

	PrecacheSound_Intercept( this, STRING( pev->noiseArrived ) );
}

void CMomentaryDoor::KeyValue( KeyValueData *pkvd )
{

	if (FStrEq(pkvd->szKeyName, "movesnd"))
	{
		m_bMoveSnd = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "stopsnd"))
	{
		m_bStopSnd = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "healthvalue"))
	{
//		m_bHealthValue = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

void CMomentaryDoor::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( useType != USE_SET )		// Momentary buttons will pass down a float in here
		return;

	if ( value > 1.0 )
		value = 1.0;

	Vector move = m_vecPosition1 + (value * (m_vecPosition2 - m_vecPosition1));
	
	Vector delta = move - pev->origin;
	//float speed = delta.Length() * 10;
	float speed = delta.Length() / 0.1; // move there in 0.1 sec
	if ( speed == 0 )
		return;

	// This entity only thinks when it moves, so if it's thinking, it's in the process of moving
	// play the sound when it starts moving (not yet thinking)
	if ( pev->velocity.Length() <= 1.0 && ( pev->nextthink < pev->ltime || pev->nextthink == 0 ) )
		EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMoving), 1, ATTN_NORM);

	LinearMove( move, speed );
}

//
// The door has reached needed position.
//
void CMomentaryDoor::DoorMoveDone( void )
{
	STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMoving) );
	EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseArrived), 1, ATTN_NORM);
}
