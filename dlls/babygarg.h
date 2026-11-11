#ifndef BABYGARG_H
#define BABYGARG_H

class CBabyGarg : public CSquadMonster
{
public:
	void                   Spawn( void );
	void                   Precache( void );
	void                   OnDestroy( void );
	void                   UpdateOnRemove( void );
	int                    Save( CSave &save );
	int                    Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	int                    Classify( void );
	int                    ObjectCaps( void );
	int                    IRelationship( CBaseEntity *pTarget, bool fIgnoreNoTarget = false );
	float                  GetPointsForDamage( float flDamage );
	void                   SetYawSpeed( void );
	void                   SetObjectCollisionBox( void );
	void                   HandleAnimEvent( MonsterEvent_t *pEvent );
	void                   PrescheduleThink( void );
	void                   Killed( entvars_t *pevAttacker, int iGib );
	int                    TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void                   TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	BOOL                   CheckMeleeAttack1( float flDot, float flDist );
	BOOL                   CheckMeleeAttack2( float flDot, float flDist );
	BOOL                   CheckRangeAttack1( float flDot, float flDist );
	Schedule_t            *GetScheduleOfType( int Type );
	void                   StartTask( Task_t *pTask );
	void                   RunTask( Task_t *pTask );
	CUSTOM_SCHEDULES;

private:
	CSprite     *CreateEye( void );
	void         EyeOn( int level );
	void         EyeOff( void );
	void         EyeUpdate( void );
	void         StompAttack( void );
	void         FlameCreate( void );
	void         FlameControls( float angleX, float angleY );
	void         FlameUpdate( void );
	void         RemoveFlames( void );
	void         FlameDestroy( void );
	void         FlameDamage( Vector vecStart, Vector vecEnd, entvars_t *pevInflictor, entvars_t *pevAttacker,
	                          float flDamage, int iClassIgnore, int bitsDamageType );
	CBaseEntity *FindRunAwayTarget( void );

	static const char *pAttackHitSounds[];
	static const char *pBeamAttackSounds[];
	static const char *pAttackMissSounds[];
	static const char *pFootSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pAttackSounds[];
	static const char *pStompSounds[];
	static const char *pBreatheSounds[];
	static const char *pDieSounds[];

	EHANDLE m_hEyeGlow;
	float   m_flEyeBrightness;
	EHANDLE m_hFlame[4];
	int     m_eyeBrightness;
	float   m_seeTime;
	float   m_flameTime;
	float   m_painSoundTime;
	float   m_streakTime;
	float   m_flameX;
	float   m_flameY;
};

#endif
