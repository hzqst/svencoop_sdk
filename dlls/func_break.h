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
#ifndef FUNC_BREAK_H
#define FUNC_BREAK_H

#include "attackvectors.h"

typedef enum
{
	expRandom,
	expDirected
} Explosions;
typedef enum
{
	matGlass = 0,
	matWood,
	matMetal,
	matFlesh,
	matCinderBlock,
	matCeilingTile,
	matComputer,
	matUnbreakableGlass,
	matRocks,
	matNone,
	matLastMaterial
} Materials;

#define NUM_SHARDS 6 // Number of shards per broken object

#define SF_BREAK_REPAIRABLE			8
#define SF_BREAK_IMMUNE_CLIENTS		64
#define SF_BREAK_EXPLOSIVES_ONLY	512

#define BREAKABLE_DAMAGE_HISTORY	64	// recent hits kept by LogDamage

class CBreakable : public CBaseDelay
{
public:
	CBreakable();
	~CBreakable() override;

	// basic functions
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void *GetKeyValue( const char *name, entvartype_s &type ) override;
	void EXPORT BreakTouch( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void DamageSound( void );
	bool CanInstantBreak( CBaseEntity *attacker, int weapon );
	bool CheckAttacker( CBaseEntity *attacker );
	CAttackVectors *GetAttackVectors( void ) const override;
	void LogDamage( float damage, Vector direction );
	float GetRecentTotalDamage( float &maximum, Vector &direction, float period );

	// breakables use an overridden takedamage
	virtual int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	// To spark when hit
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );

	bool IsBreakable( void );
	BOOL SparkWhenHit( void );

	int DamageDecal( int bitsDamageType );

	void EXPORT Die( void );
	virtual int ObjectCaps( void ) { return ( CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION ); }
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );

	inline BOOL Explodable( void ) { return ExplosionMagnitude() > 0; }
	inline int ExplosionMagnitude( void ) { return pev->impulse; }
	inline void ExplosionSetMagnitude( int magnitude ) { pev->impulse = magnitude; }

	static void MaterialSoundPrecache( Materials precacheMaterial );
	static void MaterialSoundRandom( edict_t *pEdict, Materials soundMaterial, float volume );
	static const char **MaterialSoundList( Materials precacheMaterial, int &soundCount );

	static const char *pSoundsWood[];
	static const char *pSoundsFlesh[];
	static const char *pSoundsGlass[];
	static const char *pSoundsMetal[];
	static const char *pSoundsConcrete[];
	static const char *pSpawnObjects[];

	static TYPEDESCRIPTION m_SaveData[];

	Materials  m_Material;
	Explosions m_Explosion;
	int        m_idShard;
	float      m_angle;
	int        m_iszGibModel;
	int        m_iszSpawnObject;
	int        m_instantBreakWeapon;
	bool       m_fUseTriggered;
	bool       m_fDead;

	float  m_flDamage[BREAKABLE_DAMAGE_HISTORY];
	Vector m_vecDamageDirection[BREAKABLE_DAMAGE_HISTORY];
	float  m_flDamageTime[BREAKABLE_DAMAGE_HISTORY];
	int    m_iDamageIndex;

	CAttackVectors m_AttackVectors;
};

#define SF_PUSH_HOVER 1024
#define SF_PUSH_START_IN_SOLID ( 1 << 25 )

class CPushable : public CBreakable
{
public:
	CPushable();

	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void Touch( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void ReceivePush( CBaseEntity *pOther );
	void ReceivePull( CBaseEntity *pOther );
	void Move( const Vector &velocity );
	void PlaySound( void );
	void EXPORT HoverThink( void );

	int ObjectCaps( void ) { return FCAP_CONTINUOUS_USE; }
	int Save( CSave &save );
	int Restore( CRestore &restore );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	inline float MaxSpeed( void ) { return m_maxSpeed; }

	static TYPEDESCRIPTION m_SaveData[];
	static const char    *m_soundNames[3];

	int    m_lastSound;
	float  m_maxSpeed;
	float  m_soundTime;
	Vector m_vecHoverPosition;
	float  m_flHoverTime;
	float  m_flPushTime;
	bool   m_fCustomSize;
};

#endif // FUNC_BREAK_H
