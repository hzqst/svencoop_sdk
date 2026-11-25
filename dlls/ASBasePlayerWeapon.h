#ifndef ASBASEPLAYERWEAPON_H
#define ASBASEPLAYERWEAPON_H

#include "ASBasePlayerItem.h"
#include "ASEntityHeaderSupport.h"
#include "CString.h"
#include "player.h"
#include "server_support.h"
#include "weapons.h"

template <class T>
bool BasePlayerWeaponEntity_ExtractAmmo( CBasePlayerWeapon *weapon, T *entity )
{
	return entity->T::ExtractAmmo( weapon ) != FALSE;
}

template <class T>
bool BasePlayerWeaponEntity_AddWeapon( T *entity )
{
	return entity->T::AddWeapon() != FALSE;
}

template <class T>
bool BasePlayerWeaponEntity_PlayEmptySound( T *entity )
{
	return entity->T::PlayEmptySound();
}

template <class T>
void BasePlayerWeaponEntity_ResetEmptySound( T *entity )
{
	entity->T::ResetEmptySound();
}

template <class T>
void BasePlayerWeaponEntity_SendWeaponAnim( int animation, int skipLocal, int body, T *entity )
{
	entity->T::SendWeaponAnim( animation, skipLocal, body );
}

template <class T>
Vector BasePlayerWeaponEntity_BulletAccuracy( const Vector &moving, const Vector &standing, const Vector &crouched, T *entity )
{
	return entity->T::BulletAccuracy( moving, standing, crouched );
}

template <class T>
bool BasePlayerWeaponEntity_IsUseable( T *entity )
{
	return entity->T::IsUseable();
}

template <class T>
void BasePlayerWeaponEntity_PrimaryAttack( T *entity )
{
	entity->T::PrimaryAttack();
}

template <class T>
void BasePlayerWeaponEntity_SecondaryAttack( T *entity )
{
	entity->T::SecondaryAttack();
}

template <class T>
void BasePlayerWeaponEntity_TertiaryAttack( T *entity )
{
	entity->T::TertiaryAttack();
}

template <class T>
void BasePlayerWeaponEntity_Reload( T *entity )
{
	entity->T::Reload();
}

template <class T>
void BasePlayerWeaponEntity_FinishReload( T *entity )
{
	entity->T::FinishReload();
}

template <class T>
bool BasePlayerWeaponEntity_ShouldReload( T *entity )
{
	return entity->T::ShouldReload();
}

template <class T>
void BasePlayerWeaponEntity_WeaponIdle( T *entity )
{
	entity->T::WeaponIdle();
}

template <class T>
void BasePlayerWeaponEntity_RetireWeapon( T *entity )
{
	entity->T::RetireWeapon();
}

template <class T>
bool BasePlayerWeaponEntity_ShouldWeaponIdle( T *entity )
{
	return entity->T::ShouldWeaponIdle();
}

template <class T>
bool BasePlayerWeaponEntity_UseDecrement( T *entity )
{
	return entity->T::UseDecrement();
}

template <class T>
void BasePlayerWeaponEntity_BurstSupplement( T *entity )
{
	entity->T::BurstSupplement();
}

template <class T>
void RegisterSCScriptBaseBaseClassPlayerWeaponEntity( CASDocumentation *documentation, const char *description, const char *name )
{
	RegisterSCScriptBaseBaseClassPlayerItemEntity<T>( documentation, description, name );
	RegisterASScriptEntityHeaderCasts<CBasePlayerWeapon, T>( documentation, "BasePlayerWeapon", name );

	documentation->RegisterObjectMethod(
	    "Calls the base class implementation of ExtractAmmo", name,
	    "bool ExtractAmmo(CBasePlayerWeapon@ pWeapon)",
	    asFUNCTION( BasePlayerWeaponEntity_ExtractAmmo<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Calls the base class implementation of AddWeapon", name,
	    "bool AddWeapon()",
	    asFUNCTION( BasePlayerWeaponEntity_AddWeapon<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Calls the base class implementation of PlayEmptySound", name,
	    "bool PlayEmptySound()",
	    asFUNCTION( BasePlayerWeaponEntity_PlayEmptySound<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Calls the base class implementation of ResetEmptySound", name,
	    "void ResetEmptySound()",
	    asFUNCTION( BasePlayerWeaponEntity_ResetEmptySound<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Calls the base class implementation of SendWeaponAnim", name,
	    "void SendWeaponAnim(int iAnim, int skiplocal = 0, int body = 0)",
	    asFUNCTION( BasePlayerWeaponEntity_SendWeaponAnim<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Calls the base class implementation of BulletAccuracy", name,
	    "Vector BulletAccuracy(const Vector& in vecMoving, const Vector& in vecStanding, const Vector& in vecCrouched)",
	    asFUNCTION( BasePlayerWeaponEntity_BulletAccuracy<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Calls the base class implementation of IsUseable", name,
	    "bool IsUseable()",
	    asFUNCTION( BasePlayerWeaponEntity_IsUseable<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Calls the base class implementation of PrimaryAttack", name,
	    "void PrimaryAttack()",
	    asFUNCTION( BasePlayerWeaponEntity_PrimaryAttack<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Calls the base class implementation of SecondaryAttack", name,
	    "void SecondaryAttack()",
	    asFUNCTION( BasePlayerWeaponEntity_SecondaryAttack<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Calls the base class implementation of TertiaryAttack", name,
	    "void TertiaryAttack()",
	    asFUNCTION( BasePlayerWeaponEntity_TertiaryAttack<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Calls the base class implementation of Reload", name,
	    "void Reload()",
	    asFUNCTION( BasePlayerWeaponEntity_Reload<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Calls the base class implementation of FinishReload", name,
	    "void FinishReload()",
	    asFUNCTION( BasePlayerWeaponEntity_FinishReload<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Calls the base class implementation of ShouldReload", name,
	    "bool ShouldReload()",
	    asFUNCTION( BasePlayerWeaponEntity_ShouldReload<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Calls the base class implementation of WeaponIdle", name,
	    "void WeaponIdle()",
	    asFUNCTION( BasePlayerWeaponEntity_WeaponIdle<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Calls the base class implementation of RetireWeapon", name,
	    "void RetireWeapon()",
	    asFUNCTION( BasePlayerWeaponEntity_RetireWeapon<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Calls the base class implementation of ShouldWeaponIdle", name,
	    "bool ShouldWeaponIdle()",
	    asFUNCTION( BasePlayerWeaponEntity_ShouldWeaponIdle<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Calls the base class implementation of UseDecrement", name,
	    "bool UseDecrement()",
	    asFUNCTION( BasePlayerWeaponEntity_UseDecrement<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Calls the base class implementation of BurstSupplement", name,
	    "void BurstSupplement()",
	    asFUNCTION( BasePlayerWeaponEntity_BurstSupplement<T> ), asCALL_CDECL_OBJLAST );
}

template <class T>
int PlayerWeaponEntity_AddPrimaryAmmo( int count, const CString &name, int maxClip, int maxCarry, T *entity )
{
	return entity->AddPrimaryAmmo( count, (char *) name.Data(), maxClip, maxCarry );
}

template <class T>
int PlayerWeaponEntity_AddSecondaryAmmo( int count, const CString &name, int maxCarry, T *entity )
{
	return entity->AddSecondaryAmmo( count, (char *) name.Data(), maxCarry );
}

template <class T>
bool PlayerWeaponEntity_DefaultDeploy( const CString &viewModel, const CString &weaponModel, int animation, const CString &extension, int skipLocal, int body, T *entity )
{
	const char *weaponModelName = STRING( AllocStringLocal( weaponModel.Data() ) );
	const char *viewModelName = STRING( AllocStringLocal( viewModel.Data() ) );
	return entity->DefaultDeploy( viewModelName, weaponModelName, animation, extension.Data(), skipLocal, body );
}

template <class T>
CString PlayerWeaponEntity_GetP_Model( T *entity )
{
	return CString( entity->GetP_Model() );
}

template <class T>
CString PlayerWeaponEntity_GetP_Model( const CString &alternative, T *entity )
{
	return CString( entity->GetP_Model( alternative.Data() ) );
}

template <class T>
CString PlayerWeaponEntity_GetW_Model( T *entity )
{
	return CString( entity->GetW_Model() );
}

template <class T>
CString PlayerWeaponEntity_GetW_Model( const CString &alternative, T *entity )
{
	return CString( entity->GetW_Model( alternative.Data() ) );
}

template <class T>
CString PlayerWeaponEntity_GetV_Model( T *entity )
{
	return CString( entity->GetV_Model() );
}

template <class T>
CString PlayerWeaponEntity_GetV_Model( const CString &alternative, T *entity )
{
	return CString( entity->GetV_Model( alternative.Data() ) );
}

template <class T>
void RegisterSCScriptBasePlayerWeapon( CASDocumentation *documentation, const char *name )
{
	RegisterASScriptEntityHeaderCasts<CBasePlayerWeapon, T>( documentation, "CBasePlayerWeapon", name );
	RegisterSCScriptBasePlayerItem<T>( documentation, name );

	documentation->RegisterObjectMethod(
	    "", name,
	    "bool ExtractAmmo(CBasePlayerWeapon@ pWeapon)",
	    asMETHOD( T, ExtractAmmo ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "", name,
	    "bool ExtractClipAmmo(CBasePlayerWeapon@ pWeapon)",
	    asMETHOD( T, ExtractClipAmmo ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "", name,
	    "bool AddWeapon()",
	    asMETHOD( T, AddWeapon ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "", name,
	    "int AddPrimaryAmmo(int iCount, const string& in szName, int iMaxClip, int iMaxCarry)",
	    asFUNCTION( PlayerWeaponEntity_AddPrimaryAmmo<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "", name,
	    "int AddSecondaryAmmo(int iCount, const string& in szName, int iMaxCarry)",
	    asFUNCTION( PlayerWeaponEntity_AddSecondaryAmmo<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectProperty(
	    "Whether to play the empty sound when the player attempts to fire", name,
	    "bool m_bPlayEmptySound", asOFFSET( T, m_bPlayEmptySound ) );
	documentation->RegisterObjectProperty(
	    "True when the gun is empty and the player is still holding down the attack key(s)", name,
	    "bool m_bFireOnEmpty", asOFFSET( T, m_fFireOnEmpty ) );
	documentation->RegisterObjectMethod(
	    "", name,
	    "bool PlayEmptySound()",
	    asMETHOD( T, PlayEmptySound ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "", name,
	    "void ResetEmptySound()",
	    asMETHOD( T, ResetEmptySound ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "skiplocal is 1 if client is predicting weapon animations", name,
	    "void SendWeaponAnim(int iAnim, int skiplocal = 0, int body = 0)",
	    asMETHOD( T, SendWeaponAnim ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "Universal code to determine player bullet accuracy based on speed/stance", name,
	    "Vector BulletAccuracy(const Vector& in vecMoving, const Vector& in vecStanding, const Vector& in vecCrouched)",
	    asMETHOD( T, BulletAccuracy ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "", name,
	    "bool IsUseable()",
	    asMETHOD( T, IsUseable ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "", name,
	    "bool DefaultDeploy(const string& in szViewModel, const string& in szWeaponModel, int iAnim, const string& in "
	    "szAnimExt, int skiplocal = 0, int body = 0)",
	    asFUNCTION( PlayerWeaponEntity_DefaultDeploy<T> ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "", name,
	    "bool DefaultReload(int iClipSize, int iAnim, float flDelay, int iSubModel, int iClip, int iAmmoType)",
	    asMETHODPR( T, DefaultReload, (int, int, float, int, int, int), bool ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "", name,
	    "bool DefaultReload(int iClipSize, int iAnim, float flDelay, int iSubModel = 0)",
	    asMETHODPR( T, DefaultReload, (int, int, float, int), bool ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "do \"+ATTACK\"", name,
	    "void PrimaryAttack()",
	    asMETHOD( T, PrimaryAttack ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "do \"+ATTACK2\"", name,
	    "void SecondaryAttack()",
	    asMETHOD( T, SecondaryAttack ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "do \"+ATTACK3\"", name,
	    "void TertiaryAttack()",
	    asMETHOD( T, TertiaryAttack ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "", name,
	    "void Reload()",
	    asMETHOD( T, Reload ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "Finishes the reload for this weapon.", name,
	    "void FinishReload()",
	    asMETHOD( T, FinishReload ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "Returns whether this weapon should reload automatically.", name,
	    "bool ShouldReload()",
	    asMETHOD( T, ShouldReload ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "called when no buttons pressed", name,
	    "void WeaponIdle()",
	    asMETHOD( T, WeaponIdle ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "no more ammo for this gun, put it away.", name,
	    "void RetireWeapon()",
	    asMETHOD( T, RetireWeapon ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "", name,
	    "bool ShouldWeaponIdle()",
	    asMETHOD( T, ShouldWeaponIdle ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "Whether the client predicts this weapon", name,
	    "bool UseDecrement()",
	    asMETHOD( T, UseDecrement ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "", name,
	    "void BurstSupplement()",
	    asMETHOD( T, BurstSupplement ), asCALL_THISCALL );
	documentation->RegisterObjectProperty(
	    "soonest time ItemPostFrame will call PrimaryAttack", name,
	    "float m_flNextPrimaryAttack", asOFFSET( T, m_flNextPrimaryAttack ) );
	documentation->RegisterObjectProperty(
	    "soonest time ItemPostFrame will call SecondaryAttack", name,
	    "float m_flNextSecondaryAttack", asOFFSET( T, m_flNextSecondaryAttack ) );
	documentation->RegisterObjectProperty(
	    "soonest time ItemPostFrame will call TertiaryAttack", name,
	    "float m_flNextTertiaryAttack", asOFFSET( T, m_flNextTertiaryAttack ) );
	documentation->RegisterObjectProperty(
	    "soonest time ItemPostFrame will call WeaponIdle", name,
	    "float m_flTimeWeaponIdle", asOFFSET( T, m_flTimeWeaponIdle ) );
	documentation->RegisterObjectProperty(
	    "\"primary\" ammo index into players m_rgAmmo[]", name,
	    "int m_iPrimaryAmmoType", asOFFSET( T, m_iPrimaryAmmoType ) );
	documentation->RegisterObjectProperty(
	    "\"secondary\" ammo index into players m_rgAmmo[]", name,
	    "int m_iSecondaryAmmoType", asOFFSET( T, m_iSecondaryAmmoType ) );
	documentation->RegisterObjectProperty(
	    "number of shots left in the primary weapon clip, -1 it not used", name,
	    "int m_iClip", asOFFSET( T, m_iClip ) );
	documentation->RegisterObjectProperty(
	    "number of shots left in the secondary weapon clip, -1 it not used", name,
	    "int m_iClip2", asOFFSET( T, m_iClip2 ) );
	documentation->RegisterObjectProperty(
	    "Are we in the middle of a reload", name,
	    "bool m_fInReload", asOFFSET( T, m_fInReload ) );
	documentation->RegisterObjectProperty(
	    "Per-entity weapon damage", name,
	    "float m_flCustomDmg", asOFFSET( T, m_flCustomDmg ) );
	documentation->RegisterObjectProperty(
	    "Weapon is in 'akimbo' mode", name,
	    "bool m_fIsAkimbo", asOFFSET( T, m_fIsAkimbo ) );
	documentation->RegisterObjectProperty(
	    "Weapon is zoomed in", name,
	    "bool m_fInZoom", asOFFSET( T, m_fInZoom ) );
	documentation->RegisterObjectProperty(
	    "When the next burst round is to be fired", name,
	    "float m_flNextBurstRound", asOFFSET( T, m_flNextBurstRound ) );
	documentation->RegisterObjectProperty(
	    "The amount of primary ammo you receive when you pick up this weapon as placed by a level designer.", name,
	    "int m_iDefaultAmmo", asOFFSET( T, m_iDefaultAmmo ) );
	documentation->RegisterObjectProperty(
	    "The amount of secondary ammo you receive when you pick up this weapon as placed by a level designer.", name,
	    "int m_iDefaultSecAmmo", asOFFSET( T, m_iDefaultSecAmmo ) );
	documentation->RegisterObjectMethod(
	    "Gets the weapon's P model.", name,
	    "string GetP_Model()",
	    asFUNCTIONPR( PlayerWeaponEntity_GetP_Model<T>, (T *), CString ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Gets the weapon's P model.", name,
	    "string GetP_Model(const string& in szAlternative)",
	    asFUNCTIONPR( PlayerWeaponEntity_GetP_Model<T>, (const CString &, T *), CString ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Gets the weapon's W model.", name,
	    "string GetW_Model()",
	    asFUNCTIONPR( PlayerWeaponEntity_GetW_Model<T>, (T *), CString ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Gets the weapon's W model.", name,
	    "string GetW_Model(const string& in szAlternative)",
	    asFUNCTIONPR( PlayerWeaponEntity_GetW_Model<T>, (const CString &, T *), CString ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Gets the weapon's V model.", name,
	    "string GetV_Model()",
	    asFUNCTIONPR( PlayerWeaponEntity_GetV_Model<T>, (T *), CString ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Gets the weapon's V model.", name,
	    "string GetV_Model(const string& in szAlternative)",
	    asFUNCTIONPR( PlayerWeaponEntity_GetV_Model<T>, (const CString &, T *), CString ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod(
	    "Precaches custom models if needed.", name,
	    "void PrecacheCustomModels()",
	    asMETHOD( T, PrecacheCustomModels ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "0 means reset to default fov", name,
	    "void SetFOV(int iFOV)",
	    asMETHOD( T, SetFOV ), asCALL_THISCALL );
	documentation->RegisterObjectMethod(
	    "", name,
	    "void ChangeWeaponSkin(int16 skin)",
	    asMETHOD( T, ChangeWeaponSkin ), asCALL_THISCALL );
}

#endif
