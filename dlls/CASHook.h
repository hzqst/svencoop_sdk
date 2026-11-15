#ifndef CASHOOK_H
#define CASHOOK_H

#include <stdarg.h>
#include "CASHookArguments.h"

class asIScriptFunction;
class CASDocumentation;
class CASHookManager;
class CASModuleHookManager;
class CBasePlayer;

enum HookReturnCode_t : int
{
	HOOK_CONTINUE = 0,
	HOOK_HANDLED = 1
};

class CASHook
{
public:
	CASHook(unsigned char dllTypes, unsigned char moduleTypes, const char *nameSpace,
	        const char *name, const char *arguments, const CASHookArguments &hookArguments);

	bool VCall(CBasePlayer *player, int argumentCount, va_list arguments);
	bool Call(CBasePlayer *player, int argumentCount, ...);
	bool Call(int argumentCount, ...);
	static void OutputHooksToFile(const char *fileName);

	static const char *HOOK_RETURN_TYPE;

private:
	friend class CASHookManager;
	friend class CASModuleHookManager;
	friend class CASPluginManager;
	friend class CASServerManager;
	friend void RegisterSCScriptModuleHookManager(CASDocumentation *documentation);

	static CASHook         *m_pHead;

	CASHook                *m_pNext;
	unsigned char           m_uiDLLTypes;
	unsigned char           m_uiModuleTypes;
	const char             *m_pszNamespace;
	const char             *m_pszName;
	const char             *m_pszArguments;
	const char             *m_pszDescription;
	HookStopMode::Type      m_StopMode;
	HookFlag::Type          m_Flags;
	unsigned int            m_uiCode;
	asIScriptFunction      *m_pFunctionType;
};

void RegisterSCScriptModuleHookReturnCode(CASDocumentation *documentation);

extern CASHook g_CanCollectHook;
extern CASHook g_MaterializeHook;
extern CASHook g_CollectedHook;
extern CASHook g_WeaponTertiaryAttackHook;
extern CASHook g_WeaponReloadHook;
extern CASHook g_PlayerEnteredObserverHook;
extern CASHook g_PlayerLeftObserverHook;
extern CASHook g_GetPlayerSpawnSpotHook;
extern CASHook g_ClientDisconnectHook;
extern CASHook g_ClientPutInServerHook;
extern CASHook g_CanPlayerUseReservedSlotHook;
extern CASHook g_ClientConnectedHook;
extern CASHook g_ClientSayHook;
extern CASHook g_PlayerSpawnHook;
extern CASHook g_PlayerRevivedHook;
extern CASHook g_MonsterKilledHook;
extern CASHook g_MonsterTakeDamageHook;
extern CASHook g_PlayerTakeDamageHook;
extern CASHook g_PlayerUseHook;
extern CASHook g_PlayerPreThinkHook;
extern CASHook g_PlayerPostThinkHook;

class CBaseEntity;

void DispatchEntityCreatedHook(CBaseEntity *entity);

#endif
