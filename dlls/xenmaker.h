#ifndef XENMAKER_H
#define XENMAKER_H

#define SF_XENMAKER_TRY_ONCE		1
#define SF_XENMAKER_NO_MONSTER		2

class CXenMaker : public CBaseEntity
{
public:
	CXenMaker();

	void Spawn( void ) override;
	void Precache( void ) override;
	void KeyValue( KeyValueData *pkvd ) override;
	void Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value ) override;
	int Save( CSave &save ) override;
	int Restore( CRestore &restore ) override;

	static TYPEDESCRIPTION m_SaveData[];

private:
	friend class CSquadMaker;

	bool CheckClear( void );
	void CreateEffect( float scale );
	void EXPORT DestroyEffect( void );
	void EXPORT MakeMonster( void );
	void EXPORT TryThink( void );

	unsigned char	m_unusedStorage[24];
	string_t		m_iszMonsterClassname;
	float			m_flScale;
	float			m_flGround;
	bool			m_bTryOnce;
	bool			m_bMakeMonster;
	int				m_iSpawnTrys;
	int				m_iMaxSpawnTrys;

	int			m_iBeamCount;
	Vector		m_vBeamColor;
	int			m_iBeamAlpha;
	float		m_flBeamRadius;

	Vector		m_vLightColor;
	float		m_flLightRadius;

	Vector		m_vStartSpriteColor;
	float		m_flStartSpriteScale;
	float		m_flStartSpriteFramerate;
	int			m_iStartSpriteAlpha;

	Vector		m_vEndSpriteColor;
	float		m_flEndSpriteScale;
	float		m_flEndSpriteFramerate;
	int			m_iEndSpriteAlpha;
};

#endif
