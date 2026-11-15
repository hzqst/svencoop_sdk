#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "scientist.h"
#include "game.h"

extern cvar_t sk_scientist_health;

class CScientistCleansuit : public CScientist
{
public:
	float GetPointsForDamage( float flDamage )
	{
		//ScoreIncrement = (Damage / MonsterCustomHealth) * (MonstersDefaultScore * (MonstersCustomHealth / MonstersDefaultHealth) )
		return pev->max_health / sk_scientist_health.value * ( flDamage / pev->max_health );
	}

	void SetPlayerAlly( bool state ) { SetPlayerAllyDirect( !state ); }
	void Spawn( void );
	void Precache( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	BOOL CanHeal( void ) { return FALSE; }
	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

private:
	float m_painTime;
	float m_healTime;
	float m_fearTime;
};

LINK_ENTITY_TO_CLASS( monster_cleansuit_scientist, CScientistCleansuit );

TYPEDESCRIPTION CScientistCleansuit::m_SaveData[] = {
	DEFINE_FIELD( CScientistCleansuit, m_painTime, FIELD_TIME ),
	DEFINE_FIELD( CScientistCleansuit, m_healTime, FIELD_TIME ),
	DEFINE_FIELD( CScientistCleansuit, m_fearTime, FIELD_TIME )
};

IMPLEMENT_SAVERESTORE( CScientistCleansuit, CScientist );

void CScientistCleansuit::Spawn( void )
{
	Precache();

	if ( !SetupModel() )
		UTIL_SetModel( this, "models/cleansuit_scientist.mdl" );

	if ( pev->health == 0 )
		pev->health = gSkillData.scientistHealth;

	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->view_ofs = Vector( 0, 0, 50 );
	m_flFieldOfView = VIEW_FIELD_WIDE;
	m_MonsterState = MONSTERSTATE_NONE;
	m_afCapability = bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP | bits_CAP_USE_TANK;
	m_fCheckCreatureDanger = true;

	if ( pev->body == -1 )
		pev->body = RANDOM_LONG( 0, 3 );

	if ( !m_FormattedName )
		m_FormattedName = MAKE_STRING( "Cleansuit Scientist" );

	MonsterInit();
	SetUse( &CScientistCleansuit::CallFollowerPlayerUse );
}

void CScientistCleansuit::Precache( void )
{
	TalkInit();
	CTalkMonster::Precache();
	UTIL_PrecacheModel( this, "models/cleansuit_scientist.mdl" );

	PrecacheSound_Intercept( this, "scientist/sci_pain1.wav" );
	PrecacheSound_Intercept( this, "scientist/sci_pain2.wav" );
	PrecacheSound_Intercept( this, "scientist/sci_pain3.wav" );
	PrecacheSound_Intercept( this, "scientist/sci_pain4.wav" );
	PrecacheSound_Intercept( this, "scientist/sci_pain5.wav" );
	PrecacheSound_Intercept( this, "scientist/scream01.wav" );
	PrecacheSound_Intercept( this, "scientist/scream02.wav" );
	PrecacheSound_Intercept( this, "scientist/scream22.wav" );
	PrecacheSound_Intercept( this, "scientist/scream05.wav" );
	PrecacheSound_Intercept( this, "scientist/scream07.wav" );
	PrecacheSound_Intercept( this, "scientist/scream14.wav" );
	PrecacheSound_Intercept( this, "scientist/scream21.wav" );
}

int CScientistCleansuit::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker,
                                     float flDamage, int bitsDamageType )
{
	if ( pevInflictor && FClassnameIs( pevInflictor, "squidspit" ) )
	{
		SetConditions( bits_COND_LIGHT_DAMAGE );
		return 0;
	}

	if ( bitsDamageType & ( DMG_DROWN | DMG_NERVEGAS | DMG_POISON | DMG_RADIATION | DMG_ACID ) )
		return 0;

	if ( !pevAttacker || CheckAttacker( CBaseEntity::Instance( pevAttacker ) ) )
		return 0;

	if ( pevInflictor && FBitSet( pevInflictor->flags, FL_CLIENT ) )
	{
		Remember( bits_MEMORY_PROVOKED );
		StopPlayerFollowing( true, false );
	}

	return CTalkMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

class CScientistCleansuitDead : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );

private:
	int m_iPose;
};

LINK_ENTITY_TO_CLASS( monster_cleansuit_scientist_dead, CScientistCleansuitDead );

void CScientistCleansuitDead::Precache( void )
{
	const char *model = pev->model ? STRING( pev->model ) : "models/cleansuit_scientist.mdl";

	UTIL_PrecacheModel( this, model );
	UTIL_SetModel( this, model );
}

void CScientistCleansuitDead::Spawn( void )
{
	static const char *poses[] = {
		"lying_on_back", "lying_on_stomach", "dead_sitting", "dead_hang", "dead_table1",
		"dead_table2", "dead_table3", "scientist_deadpose1", "dead_against_wall"
	};

	Precache();
	pev->health = gSkillData.scientistHealth;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->sequence = LookupSequence( poses[m_iPose] );

	if ( pev->sequence == -1 )
		ALERT( at_console, "monster_cleansuit_scientist_dead at ( %f, %f, %f ) with bad/missing pose!\n",
		       pev->origin.x, pev->origin.y, pev->origin.z );

	MonsterInitDead();
}

void CScientistCleansuitDead::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "pose" ) )
	{
		unsigned long pose = strtoul( pkvd->szValue, NULL, 10 );

		m_iPose = pose <= 8 ? pose : 8;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue( pkvd );
}
