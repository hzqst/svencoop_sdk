#ifndef BLKOPOSPREY_H
#define BLKOPOSPREY_H

class COspreyBlackOps : public CBaseMonster
{
public:
	void                   Spawn( void );
	void                   Precache( void );
	void                   OnDestroy( void );
	void                   UpdateOnRemove( void );
	void                   KeyValue( KeyValueData *pkvd );
	int                    Save( CSave &save );
	int                    Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	static void            PreFindAll( void );
	static void            ResetPrecache( void );

	int    ObjectCaps( void ) { return 0; }
	int    Classify( void ) { return GetClassification( CLASS_MACHINE ); }
	int    BloodColor( void ) { return DONT_BLEED; }
	float  GetPointsForDamage( float flDamage );
	Vector BodyTarget( const Vector &posSrc );
	void   TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	void   Killed( entvars_t *pevAttacker, int iGib );

	void          UpdateGoal( void );
	BOOL          HasDead( void );
	void          Flight( void );
	void          ShowDamage( void );
	CBaseMonster *MakeGrunt( Vector vecSrc );
	void EXPORT   FlyThink( void );
	void EXPORT   FindAllThink( void );
	void EXPORT   DeployThink( void );
	void EXPORT   HoverThink( void );
	void EXPORT   DyingThink( void );
	void EXPORT   HitTouch( CBaseEntity *pOther );
	void EXPORT   CrashTouch( CBaseEntity *pOther );
	void EXPORT   CommandUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

private:
	enum
	{
		MAX_CARRY = 24
	};
	static EHANDLE m_hPreGrunt[MAX_CARRY];
	static EHANDLE m_hPreGrunt_friendly[MAX_CARRY];
	static Vector  m_vecPreOrigin[MAX_CARRY];
	static Vector  m_vecPreOrigin_friendly[MAX_CARRY];
	static int     m_iPreUnits;
	static int     m_iPreUnits_friendly;
	static bool    m_cancheck2;

	EHANDLE m_hGoalEnt;
	Vector  m_vel1;
	Vector  m_vel2;
	Vector  m_pos1;
	Vector  m_pos2;
	Vector  m_ang1;
	Vector  m_ang2;
	float   m_startTime;
	float   m_dTime;
	Vector  m_velocity;
	float   m_flIdealtilt;
	float   m_flRotortilt;
	float   m_flRightHealth;
	float   m_flLeftHealth;

	Vector  m_vecOrigin[MAX_CARRY];
	EHANDLE m_hRepel[4];
	int     m_iSoundState;
	int     m_iSpriteTexture;
	int     m_iPitch;
	int     m_iExplode;
	int     m_iTailGibs;
	int     m_iBodyGibs;
	int     m_iDoLeftSmokePuff;
	int     m_iDoRightSmokePuff;

	string_t m_iszTargetOnDeath;
	bool     m_fWaitingForTrigger;
	bool     m_fMonsterclipChildren;
	int      m_iGruntType;
};

#endif
