#ifndef ASPLAYERENTITY_H
#define ASPLAYERENTITY_H

#include "ASBaseMonster.h"
#include "ASPlayerDependencies.h"
#include "CString.h"

class CBasePlayer;
class CBasePlayerItem;
class CBaseEntity;

CBasePlayerItem *PlayerEntity_Get_m_rgpPlayerItems( unsigned int index, CBasePlayer *player );
int PlayerEntity_Get_m_rgAmmo( unsigned int index, CBasePlayer *player );
void PlayerEntity_Set_m_rgAmmo( unsigned int index, int value, CBasePlayer *player );
CString CBasePlayer_GetAnimExtension( const CBasePlayer *player );
void CBasePlayer_SetAnimExtension( CBasePlayer *player, const CString &extension );
CBaseEntity *PlayerEntity_DropPlayerItem( const CString &name, CBasePlayer *player );
CBasePlayerItem *PlayerEntity_DropPlayerAmmo( const CString &name, CBasePlayer *player );
CBasePlayerItem *PlayerEntity_HasNamedPlayerItem( const CString &name, CBasePlayer *player );
void PlayerEntity_SelectItem( const CString &name, CBasePlayer *player );
void PlayerEntity_GiveNamedItem( const CString &name, int flags, int ammoCount, CBasePlayer *player );
void PlayerEntity_SetSuitUpdate( const CString &name, bool group, int noRepeat, CBasePlayer *player );
int PlayerEntity_GetMaxAmmo( const CString &name, const CBasePlayer *player );
void PlayerEntity_SetMaxAmmo( const CString &name, int maximum, CBasePlayer *player );
void PlayerEntity_ResetMaxAmmoToDefault( const CString &name, CBasePlayer *player );
void PlayerEntity_RemoveExcessAmmo( const CString &name, CBasePlayer *player );
void PlayerEntity_ShowOverheadSprite( const CString &name, float zOffset, float lifeTime, CBasePlayer *player );

template <class T>
void RegisterSCScriptBasePlayerEntity( CASDocumentation *documentation, const char *name )
{
	RegisterASScriptEntityHeaderCasts<CBasePlayer, T>( documentation, "CBasePlayer", name );
	RegisterSCScriptBaseMonsterEntity<T>( documentation, name );
	documentation->RegisterObjectProperty( "The player's random seed.", name, "const int random_seed",
	                                       asOFFSET( T, random_seed ) );
	documentation->RegisterObjectProperty( "The next time this player can execute a vocal client command", name, "float m_flNextClientCommandTime",
	                                       asOFFSET( T, m_flNextClientCommandTime ) );
	documentation->RegisterObjectProperty( "Time of last death.", name, "float m_flTimeOfLastDeath",
	                                       asOFFSET( T, m_flTimeOfLastDeath ) );
	documentation->RegisterObjectProperty( "Gets added to the standard respawn delay time when killed, reset in spawn to 0.0.", name, "float m_flRespawnDelayTime",
	                                       asOFFSET( T, m_flRespawnDelayTime ) );
	documentation->RegisterObjectProperty( "Pointer for a spawn point to use.", name, "EHandle m_hSpawnPoint",
	                                       asOFFSET( T, m_hSpawnPoint ) );
	documentation->RegisterObjectProperty( "When did this player move or tried to move (with the IN_ keys) ?", name, "float m_flLastMove",
	                                       asOFFSET( T, m_flLastMove ) );
	documentation->RegisterObjectProperty( "How loud the player's weapon is right now.", name, "int m_iWeaponVolume",
	                                       asOFFSET( T, m_iWeaponVolume ) );
	documentation->RegisterObjectProperty( "Additional classification for this weapon's sound.", name, "int m_iExtraSoundTypes",
	                                       asOFFSET( T, m_iExtraSoundTypes ) );
	documentation->RegisterObjectProperty( "Brightness of the weapon flash.", name, "int m_iWeaponFlash",
	                                       asOFFSET( T, m_iWeaponFlash ) );
	documentation->RegisterObjectProperty( "When to stop the m_iExtraSoundTypes sounds.", name, "float m_flStopExtraSoundTime",
	                                       asOFFSET( T, m_flStopExtraSoundTime ) );
	documentation->RegisterObjectProperty( "Player flashlight amount. 0 <= amount <= 100.", name, "int m_iFlashBattery",
	                                       asOFFSET( T, m_iFlashBattery ) );
	documentation->RegisterObjectProperty( "", name, "int m_afButtonLast",
	                                       asOFFSET( T, m_afButtonLast ) );
	documentation->RegisterObjectProperty( "", name, "int m_afButtonPressed",
	                                       asOFFSET( T, m_afButtonPressed ) );
	documentation->RegisterObjectProperty( "", name, "int m_afButtonReleased",
	                                       asOFFSET( T, m_afButtonReleased ) );
	documentation->RegisterObjectProperty( "Player fall velocity.", name, "float m_flPlayerFallVelocity",
	                                       asOFFSET( T, m_flFallVelocity ) );
	documentation->RegisterObjectProperty( "", name, "uint m_afPhysicsFlags",
	                                       asOFFSET( T, m_afPhysicsFlags ) );
	documentation->RegisterObjectProperty( "How long this player has been underwater.", name, "float m_flSwimTime",
	                                       asOFFSET( T, m_flSwimTime ) );
	documentation->RegisterObjectProperty( "Last damage taken.", name, "int m_lastPlayerDamageAmount",
	                                       asOFFSET( T, m_lastDamageAmount ) );
	documentation->RegisterObjectProperty( "Track drowning damage taken.", name, "int m_iDrownDmg",
	                                       asOFFSET( T, m_idrowndmg ) );
	documentation->RegisterObjectProperty( "Track drowning damage restored.", name, "int m_iDrownRestored",
	                                       asOFFSET( T, m_idrownrestored ) );
	documentation->RegisterObjectProperty( "Train control position", name, "int m_iTrain",
	                                       asOFFSET( T, m_iTrain ) );
	documentation->RegisterObjectProperty( "the tank which the player is currently controlling, NULL if no tank", name, "EHandle m_hTank",
	                                       asOFFSET( T, m_hTank ) );
	documentation->RegisterObjectProperty( "the time at which the player died", name, "float m_fDeadTime",
	                                       asOFFSET( T, m_fDeadTime ) );
	documentation->RegisterObjectProperty( "Does this player have the longjump module?", name, "bool m_fLongJump",
	                                       asOFFSET( T, m_fLongJump ) );
	documentation->RegisterObjectProperty( "The players hud weapon info is to be hidden.", name, "int m_iHideHUD",
	                                       asOFFSET( T, m_iHideHUD ) );
	documentation->RegisterObjectProperty( "Field of view.", name, "int m_iFOV",
	                                       asOFFSET( T, m_iFOV ) );
	documentation->RegisterObjectMethod( "Get player weapons. Index must be between 0 and MAX_ITEM_TYPES.", name,
	                                     "CBasePlayerItem@ m_rgpPlayerItems(size_t uiIndex)",
	                                     asFUNCTION( PlayerEntity_Get_m_rgpPlayerItems ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectProperty( "The active item.", name, "EHandle m_hActiveItem",
	                                       asOFFSET( T, m_pActiveItem ) );
	documentation->RegisterObjectMethod( "Get player ammo amount.", name,
	                                     "int m_rgAmmo(size_t uiIndex)",
	                                     asFUNCTION( PlayerEntity_Get_m_rgAmmo ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod( "Set player ammo amount.", name,
	                                     "void m_rgAmmo(size_t uiIndex, int iValue)",
	                                     asFUNCTION( PlayerEntity_Set_m_rgAmmo ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectProperty( "get player death count.", name, "int m_iDeaths",
	                                       asOFFSET( T, m_iDeaths ) );
	documentation->RegisterObjectProperty( "Next time this player can spray a decal.", name, "float m_flNextDecalTime",
	                                       asOFFSET( T, m_flNextDecalTime ) );
	documentation->RegisterObjectMethod( "Makes the player jump.", name,
	                                     "void Jump()",
	                                     asMETHOD( T, Jump ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Makes the player duck.", name,
	                                     "void Duck()",
	                                     asMETHOD( T, Duck ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Returns whether this player is connected.", name,
	                                     "bool IsConnected() const",
	                                     asMETHOD( T, IsConnected ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Packs player weapons into a weapon box if possible.", name,
	                                     "void PackDeadPlayerItems()",
	                                     asMETHOD( T, PackDeadPlayerItems ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Removes all items from the player's inventory.", name,
	                                     "void RemoveAllItems(const bool removeSuit = false, const bool removeLongJump = false)",
	                                     asMETHOD( T, RemoveAllItems ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Switches weapons to the given item.", name,
	                                     "bool SwitchWeapon(CBasePlayerItem@ pWeapon)",
	                                     asMETHOD( T, SwitchWeapon ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Holster weapon.", name,
	                                     "void HolsterWeapon()",
	                                     asMETHOD( T, HolsterWeapon ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Deploy weapon.", name,
	                                     "void DeployWeapon()",
	                                     asMETHOD( T, DeployWeapon ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Hides active weapon and blocks weapon selection.", name,
	                                     "void BlockWeapons(CBaseEntity@ pSetter)",
	                                     asMETHOD( T, BlockWeapons ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Shows active weapon and unblocks weapon selection.", name,
	                                     "void UnblockWeapons(CBaseEntity@ pSetter)",
	                                     asMETHOD( T, UnblockWeapons ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Returns whether the weapons are currently blocked.", name,
	                                     "bool GetWeaponsBlocked()",
	                                     asMETHOD( T, GetWeaponsBlocked ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Sets all item pickup times the given absolute time.", name,
	                                     "void SetItemPickupTimes(float flTime)",
	                                     asMETHOD( T, SetItemPickupTimes ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Returns whether the player is on a ladder.", name,
	                                     "bool IsOnLadder() const",
	                                     asMETHOD( T, IsOnLadder ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Returns whether the flashlight is on.", name,
	                                     "bool FlashlightIsOn() const",
	                                     asMETHOD( T, FlashlightIsOn ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Turns on the flashlight.", name,
	                                     "void FlashlightTurnOn()",
	                                     asMETHOD( T, FlashlightTurnOn ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Turns off the flashlight.", name,
	                                     "void FlashlightTurnOff()",
	                                     asMETHOD( T, FlashlightTurnOff ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Set the player's animation.", name,
	                                     "void SetAnimation(PLAYER_ANIM anim, int mode = 0)",
	                                     asMETHOD( T, SetAnimation ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Gets the current animation extension.", name,
	                                     "string get_m_szAnimExtension() const",
	                                     asFUNCTION( CBasePlayer_GetAnimExtension ), asCALL_CDECL_OBJFIRST );
	documentation->RegisterObjectMethod( "Sets the current animation extension.", name,
	                                     "void set_m_szAnimExtension(const string& in szAnimExtension)",
	                                     asFUNCTION( CBasePlayer_SetAnimExtension ), asCALL_CDECL_OBJFIRST );
	documentation->RegisterObjectMethod( "Triggers the given impulse command.", name,
	                                     "void CheatImpulseCommands(int iImpulse)",
	                                     asMETHOD( T, CheatImpulseCommands ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Starts the death camera.", name,
	                                     "void StartDeathCam()",
	                                     asMETHOD( T, StartDeathCam ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Returns whether this player can spawn.", name,
	                                     "bool CanSpawn()",
	                                     asMETHOD( T, CanSpawn ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Sets the player's view to that of a dead person.", name,
	                                     "void StartPlayerDeathView(const Vector& in vecPosition, const Vector& in vecViewAngle)",
	                                     asMETHOD( T, StartPlayerDeathView ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Shows the forced respawn message.", name,
	                                     "void ShowForcedRespawnMessage(int iSeconds)",
	                                     asMETHOD( T, ShowForcedRespawnMessage ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Gets the Observer instance that represents this player.", name,
	                                     "Observer@ GetObserver()",
	                                     asMETHOD( T, GetObserver ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Drops the given item, or if no name, the current weapon.", name,
	                                     "CBaseEntity@ DropItem(const string& in szItemName)",
	                                     asFUNCTION( PlayerEntity_DropPlayerItem ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod( "Drops ammo used by the named item.", name,
	                                     "CBasePlayerItem@ DropAmmo(const string& in szItemName)",
	                                     asFUNCTION( PlayerEntity_DropPlayerAmmo ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod( "Returns whether this player has the given item.", name,
	                                     "bool HasPlayerItem(CBasePlayerItem@ pCheckItem)",
	                                     asMETHOD( T, HasPlayerItem ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Returns the given item, or null if this player has no item by that name.", name,
	                                     "CBasePlayerItem@ HasNamedPlayerItem(const string& in szItemName)",
	                                     asFUNCTION( PlayerEntity_HasNamedPlayerItem ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod( "Returns whether this player has any weapons.", name,
	                                     "bool HasWeapons() const",
	                                     asMETHOD( T, HasWeapons ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Selects the previous item relative to the given item.", name,
	                                     "void SelectPrevItem(int iItem)",
	                                     asMETHOD( T, SelectPrevItem ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Selects the next item relative to the given item.", name,
	                                     "void SelectNextItem(int iItem)",
	                                     asMETHOD( T, SelectNextItem ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Selects the last active item.", name,
	                                     "void SelectLastItem()",
	                                     asMETHOD( T, SelectLastItem ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Selects the item with the given name.", name,
	                                     "void SelectItem(const string& in szItemName)",
	                                     asFUNCTION( PlayerEntity_SelectItem ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod( "Gives this player the item with the given name.\nFlags are spawnflags passed to the item.", name,
	                                     "void GiveNamedItem(const string& in szName, int iFlags = 0, int iAmmoCount = 0)",
	                                     asFUNCTION( PlayerEntity_GiveNamedItem ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod( "Sets whether the player has control.", name,
	                                     "void EnableControl(const bool bControl)",
	                                     asMETHOD( T, EnableControl ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Triggers the player's use behavior.", name,
	                                     "void PlayerUse()",
	                                     asMETHOD( T, PlayerUse ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Check for suit update messages.", name,
	                                     "void CheckSuitUpdate()",
	                                     asMETHOD( T, CheckSuitUpdate ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Set a suit update.", name,
	                                     "void SetSuitUpdate(const string& in szName, const bool bGroup, int iNoRepeat)",
	                                     asFUNCTION( PlayerEntity_SetSuitUpdate ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod( "Returns the amount of ammo currently in inventory of the given ammo index.", name,
	                                     "int AmmoInventory(int iAmmoIndex)",
	                                     asMETHOD( T, AmmoInventory ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Gets the autoaim vector for the given delta.", name,
	                                     "Vector GetAutoaimVector(float flDelta)",
	                                     asMETHOD( T, GetAutoaimVector ), asCALL_THISCALL );
	documentation->RegisterObjectProperty( "The player's class type.", name, "int m_iPlayerClass",
	                                       asOFFSET( T, m_iPlayerClass ) );
	documentation->RegisterObjectMethod( "Gets the maximum ammo setting for this player for the ammo at the given index. Returns -1 if no ammo setting is present.", name,
	                                     "int GetMaxAmmo(size_t uiIndex) const",
	                                     asMETHODPR( T, GetMaxAmmo, (unsigned int) const, int ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Gets the maximum ammo setting for this player for the given ammo type. Returns -1 if no ammo setting is present.", name,
	                                     "int GetMaxAmmo(const string& in szName) const",
	                                     asFUNCTION( PlayerEntity_GetMaxAmmo ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod( "Sets the maximum ammo setting for this player for the ammo at the given index. Value must be >= 0.", name,
	                                     "void SetMaxAmmo(size_t uiIndex, int iMaxAmmo)",
	                                     asMETHODPR( T, SetMaxAmmo, (unsigned int, int), void ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Sets the maximum ammo setting for this player for the given ammo type. Value must be >= 0.", name,
	                                     "void SetMaxAmmo(const string& in szName, int iMaxAmmo)",
	                                     asFUNCTION( PlayerEntity_SetMaxAmmo ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod( "Resets the ammo setting for this player at the given index to its default value.", name,
	                                     "void ResetMaxAmmoToDefault(size_t uiIndex)",
	                                     asMETHODPR( T, ResetMaxAmmoToDefault, (unsigned int), void ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Resets the ammo setting for this player for the given ammo type to its default value.", name,
	                                     "void ResetMaxAmmoToDefault(const string& in szName)",
	                                     asFUNCTION( PlayerEntity_ResetMaxAmmoToDefault ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod( "Resets the ammo settings for this player for all ammo to their default values.", name,
	                                     "void ResetAllMaxAmmoToDefault()",
	                                     asMETHOD( T, ResetAllMaxAmmoToDefault ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Clamps this player's ammo at the given inex to the range 0 - MAX_AMMO.", name,
	                                     "void RemoveExcessAmmo(size_t uiIndex)",
	                                     asMETHODPR( T, RemoveExcessAmmo, (unsigned int), void ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Clamps this player's ammo of the given type to the range 0 - MAX_AMMO.", name,
	                                     "void RemoveExcessAmmo(const string& in szName)",
	                                     asFUNCTION( PlayerEntity_RemoveExcessAmmo ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod( "Clamps all the player's ammo to their maximum amounts.", name,
	                                     "void RemoveAllExcessAmmo()",
	                                     asMETHOD( T, RemoveAllExcessAmmo ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Returns whether the player has an HEV suit.", name,
	                                     "bool HasSuit()",
	                                     asMETHOD( T, HasSuit ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Sets whether the player has an HEV suit.", name,
	                                     "void SetHasSuit(bool fHasSuit)",
	                                     asMETHOD( T, SetHasSuit ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Sets the player's view mode", name,
	                                     "void SetViewMode(PlayerViewMode viewMode)",
	                                     asMETHOD( T, SetViewMode ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Creates a sprite above the player's head", name,
	                                     "void ShowOverheadSprite(const string& in szSpriteName, const float flZOffset, const float flLifeTime)",
	                                     asFUNCTION( PlayerEntity_ShowOverheadSprite ), asCALL_CDECL_OBJLAST );
	documentation->RegisterObjectMethod( "Set custom viewmodel position.", name,
	                                     "void SetVModelPos( const Vector& in vecPos )",
	                                     asMETHOD( T, SetVModelPos ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Reset viewmodel position.", name,
	                                     "void ResetVModelPos()",
	                                     asMETHOD( T, ResetVModelPos ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Set player persistent max speed. default: sv_maxspeed", name,
	                                     "void SetMaxSpeed(int Speed)",
	                                     asMETHOD( T, SetMaxSpeed ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Override players max speed (temporarily) -1 will restore", name,
	                                     "void SetMaxSpeedOverride(int Speed)",
	                                     asMETHOD( T, SetMaxSpeedOverride ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Get the player persistent max speed. default: sv_maxspeed", name,
	                                     "int GetMaxSpeed()",
	                                     asMETHOD( T, GetMaxSpeed ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Get the player max speed override (-1 = not overriding)", name,
	                                     "int GetMaxSpeedOverride()",
	                                     asMETHOD( T, GetMaxSpeedOverride ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Reset an overriden player model.", name,
	                                     "void ResetOverriddenPlayerModel(const bool bResetToClModel, const bool bForceReset)",
	                                     asMETHOD( T, ResetOverriddenPlayerModel ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Set an overriden player model.", name,
	                                     "void SetOverriddenPlayerModel(const string& in szOverrideModel)",
	                                     asMETHOD( T, SetOverriddenPlayerModel ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Refresh the player's score information with one or all players.", name,
	                                     "void SendScoreInfo(edict_t@ edPlayer = null)",
	                                     asMETHOD( T, SendScoreInfo ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Set a player's cheat bit. (Requires cheat permission. If requested by self a permission message will be shown/logged.)", name,
	                                     "bool SetCheat(int iBit, bool fValue, bool fRequestedBySelf = false)",
	                                     asMETHOD( T, SetCheat ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Get a player's cheat bit.", name,
	                                     "bool GetCheat(int iBit)",
	                                     asMETHOD( T, GetCheat ), asCALL_THISCALL );
	documentation->RegisterObjectMethod( "Get a player's cheat bits.", name,
	                                     "int GetCheats()",
	                                     asMETHOD( T, GetCheats ), asCALL_THISCALL );
}

#endif
