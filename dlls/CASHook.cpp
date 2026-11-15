#include "extdll.h"
#include "util.h"
#include "CASHook.h"
#include "CASDocumentation.h"
#include "CASServerManager.h"
#include "CASLog.h"
#include "DllType.h"
#include "ASFileSystem.h"
#include "CKeyvaluesWriter.h"
#include "server_support.h"

#define HOOK_MODULE_TYPES_ALL ( ( 1 << ModuleType::MAP ) | ( 1 << ModuleType::PLUGIN ) )

CASHook *CASHook::m_pHead;
const char *CASHook::HOOK_RETURN_TYPE = "HookReturnCode";

static const unsigned int HOOK_INVALID_CODE = (unsigned int)-1;

static const CASHookArguments mapChangeHookArguments =
{
	HookStopMode::CALL_ALL,
	HookFlag::NONE,
	"Called when the map changes. This happens when the world is destroyed. There may still be entities that "
	"exist at this point. String will show the next map will be, or empty if the server is shutting down."
};

CASHook g_MapChangeHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Game", "MapChange", "const string& in", mapChangeHookArguments);

// Declaration order determines the hook codes after head insertion
static const CASHookArguments scriptHookArguments[] =
{
	{ HookStopMode::ON_HANDLED, HookFlag::NONE,
	  "Called when a new entity is created. At this point the entity is not spawned yet and may not be fully initialized." },
	{ HookStopMode::CALL_ALL, HookFlag::NONE,
	  "Called when a player disconnects. Note that this is only called if the player was fully connected, "
	  "meaning the player went through ClientPutInServer. This is never called for the local host." },
	{ HookStopMode::CALL_ALL, HookFlag::NONE,
	  "Called when a player has finished connecting and is put into the world. It is safe to send network "
	  "messages to the player at this point." },
	{ HookStopMode::ON_HANDLED, HookFlag::ADMIN_HOOK,
	  "Called when a player says something in game chat. The SayParameters class can be used to manipulate input "
	  "and veto the message." },
	{ HookStopMode::CALL_ALL, HookFlag::NONE,
	  "Called when a monster is killed.This may still be called even when the monster is taking damage while in "
	  "a event (eg.: death animation). Players also appear there as they're technically monsters too." },
	{ HookStopMode::ON_HANDLED, HookFlag::NONE,
	  "Called when a monster takes damage.Note that the victim entity can't be changed at this point. Players "
	  "also appear there as they're technically monsters too." },
	{ HookStopMode::ON_HANDLED, HookFlag::NONE,
	  "Called when a player connects to the server, and the number of slots left on the server is <= the number "
	  "of reserved slots. Set bAllowJoin to true to allow the player to join (default false)." },
	{ HookStopMode::CALL_ALL, HookFlag::NONE,
	  "Called when a player connects to the server. if bDisallowJoin is set to false, the player is "
	  "disconnected. szRejectReason is shown to the player if disconnected. The maximum length of the reject "
	  "reason string is 127 characters." },
	{ HookStopMode::CALL_ALL, HookFlag::NONE,
	  "Called when a player (re)spawns." },
	{ HookStopMode::ON_HANDLED, HookFlag::NONE,
	  "Called when the game wants to know if the player should be able to respawn or not.Set bCanRespawn to "
	  "false to disallow, default true." },
	{ HookStopMode::ON_HANDLED, HookFlag::NONE,
	  "Called when a pickup object is about to be collected by a player. Note that basic checks are done before "
	  "this hook is called." },
	{ HookStopMode::CALL_ALL, HookFlag::NONE,
	  "Called when a pickup object materializes." },
	{ HookStopMode::CALL_ALL, HookFlag::NONE,
	  "Called when a pickup object is collected by a player. " },
	{ HookStopMode::ON_HANDLED, HookFlag::NONE,
	  "Called when a player takes damage. Note that the victim entity can't be changed at this point." },
	{ HookStopMode::CALL_ALL, HookFlag::NONE,
	  "Called when a player is killed." },
	{ HookStopMode::CALL_ALL, HookFlag::NONE,
	  "Called when a player enters observer mode." },
	{ HookStopMode::CALL_ALL, HookFlag::NONE,
	  "Called when a player leaves observer mode." },
	{ HookStopMode::ON_HANDLED, HookFlag::NONE,
	  "Called when the game is processing player use input. Note that this occurs even if the player has not "
	  "pressed their use key." },
	{ HookStopMode::ON_HANDLED, HookFlag::NONE,
	  "Called when the player is processing pre think events." },
	{ HookStopMode::CALL_ALL, HookFlag::NONE,
	  "Called when the player is processing post think events." },
	{ HookStopMode::MODULE_HANDLED, HookFlag::NONE,
	  "Called when a player is about to get a spawn point before (re)spawning." },
	{ HookStopMode::CALL_ALL, HookFlag::NONE,
	  "Called when a player is revived. At this point all the revival process is finished." },
	{ HookStopMode::CALL_ALL, HookFlag::NONE,
	  "Called when a player is spraypainting a decal onto a surface. The given trace result contains the surface "
	  "information. " },
	{ HookStopMode::CALL_ALL, HookFlag::NONE,
	  "Called when a player attempts to spraypaint a decal onto a surface. The given trace result contains the "
	  "surface information. Set bResult to false if the player shouldn't be able to spray. " },
	{ HookStopMode::ON_HANDLED, HookFlag::NONE,
	  "Called when a player fires a weapon's primary attack." },
	{ HookStopMode::ON_HANDLED, HookFlag::NONE,
	  "Called when a player fires a weapon's secondary attack." },
	{ HookStopMode::ON_HANDLED, HookFlag::NONE,
	  "Called when a player fires a weapon's tertiary attack." },
	{ HookStopMode::ON_HANDLED, HookFlag::NONE,
	  "Called when a player reloads a weapon." }
};

static CASHook g_EntityCreatedHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Game", "EntityCreated",
                                   "CBaseEntity@", scriptHookArguments[0]);
CASHook g_ClientDisconnectHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Player", "ClientDisconnect",
                                      "CBasePlayer@", scriptHookArguments[1]);
CASHook g_ClientPutInServerHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Player", "ClientPutInServer",
                                       "CBasePlayer@", scriptHookArguments[2]);
CASHook g_ClientSayHook( 1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Player", "ClientSay",
                         "SayParameters@", scriptHookArguments[3] );
CASHook g_MonsterKilledHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Monster", "MonsterKilled",
                                   "CBaseMonster@, CBaseEntity@, int", scriptHookArguments[4]);
CASHook g_MonsterTakeDamageHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Monster", "MonsterTakeDamage",
                                       "DamageInfo@", scriptHookArguments[5]);
CASHook g_CanPlayerUseReservedSlotHook( 1 << DllType::SERVER, 2, "Player", "CanPlayerUseReservedSlot",
                                        "edict_t@, const string& in, const string& in, bool& out", scriptHookArguments[6] );
CASHook g_ClientConnectedHook( 1 << DllType::SERVER, 2, "Player", "ClientConnected",
                               "edict_t@, const string& in, const string& in, bool& out, string& out", scriptHookArguments[7] );
CASHook g_PlayerSpawnHook( 1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Player", "PlayerSpawn",
                           "CBasePlayer@", scriptHookArguments[8] );
static CASHook g_PlayerCanRespawnHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Player", "PlayerCanRespawn",
                                      "CBasePlayer@, bool& out", scriptHookArguments[9]);
CASHook g_CanCollectHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "PickupObject", "CanCollect",
                                "CBaseEntity@, CBaseEntity@, bool& out", scriptHookArguments[10]);
CASHook g_MaterializeHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "PickupObject", "Materialize",
                                 "CBaseEntity@", scriptHookArguments[11]);
CASHook g_CollectedHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "PickupObject", "Collected",
                               "CBaseEntity@, CBaseEntity@", scriptHookArguments[12]);
CASHook g_PlayerTakeDamageHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Player", "PlayerTakeDamage",
                                      "DamageInfo@", scriptHookArguments[13]);
static CASHook g_PlayerKilledHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Player", "PlayerKilled",
                                  "CBasePlayer@, CBaseEntity@, int", scriptHookArguments[14]);
CASHook g_PlayerEnteredObserverHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Player", "PlayerEnteredObserver",
                                           "CBasePlayer@", scriptHookArguments[15]);
CASHook g_PlayerLeftObserverHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Player", "PlayerLeftObserver",
                                        "CBasePlayer@", scriptHookArguments[16]);
CASHook g_PlayerUseHook( 1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Player", "PlayerUse",
                         "CBasePlayer@, uint& out", scriptHookArguments[17] );
CASHook g_PlayerPreThinkHook( 1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Player", "PlayerPreThink",
                              "CBasePlayer@, uint& out", scriptHookArguments[18] );
CASHook g_PlayerPostThinkHook( 1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Player", "PlayerPostThink",
                               "CBasePlayer@", scriptHookArguments[19] );
CASHook g_GetPlayerSpawnSpotHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Player", "GetPlayerSpawnSpot",
                                        "CBasePlayer@ pPlayer, CBaseEntity@& out ppEntSpawnSpot", scriptHookArguments[20]);
CASHook g_PlayerRevivedHook( 1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Player", "PlayerRevived",
                           "CBasePlayer@ pPlayer", scriptHookArguments[21] );
static CASHook g_PlayerDecalHook(1 << DllType::SERVER, 2, "Player", "PlayerDecal",
                                 "CBasePlayer@, const TraceResult& in", scriptHookArguments[22]);
static CASHook g_PlayerPreDecalHook(1 << DllType::SERVER, 2, "Player", "PlayerPreDecal",
                                    "CBasePlayer@, const TraceResult& in, bool& out bResult", scriptHookArguments[23]);
static CASHook g_WeaponPrimaryAttackHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Weapon", "WeaponPrimaryAttack",
                                         "CBasePlayer@, CBasePlayerWeapon@", scriptHookArguments[24]);
static CASHook g_WeaponSecondaryAttackHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Weapon", "WeaponSecondaryAttack",
                                           "CBasePlayer@, CBasePlayerWeapon@", scriptHookArguments[25]);
CASHook g_WeaponTertiaryAttackHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Weapon", "WeaponTertiaryAttack",
                                  "CBasePlayer@, CBasePlayerWeapon@", scriptHookArguments[26]);
CASHook g_WeaponReloadHook(1 << DllType::SERVER, HOOK_MODULE_TYPES_ALL, "Weapon", "WeaponReload",
                           "CBasePlayer@, CBasePlayerWeapon@", scriptHookArguments[27]);

void DispatchEntityCreatedHook(CBaseEntity *entity)
{
	g_EntityCreatedHook.Call(0, entity);
}

const char *HookStopModeToString(HookStopMode::Type mode)
{
	if (mode == HookStopMode::MODULE_HANDLED)
		return "MODULE_HANDLED";

	if (mode == HookStopMode::CALL_ALL)
		return "CALL_ALL";

	return "ON_HANDLED";
}

HookStopMode::Type StringToHookStopMode(const char *text)
{
	if (!text || !stricmp(text, "ON_HANDLED"))
		return HookStopMode::ON_HANDLED;

	if (!stricmp(text, "MODULE_HANDLED"))
		return HookStopMode::MODULE_HANDLED;

	return !stricmp(text, "CALL_ALL") ? HookStopMode::CALL_ALL : HookStopMode::ON_HANDLED;
}

const char *HookFlagToString(HookFlag::Type flag)
{
	return flag == HookFlag::ADMIN_HOOK ? "ADMIN_HOOK" : "NONE";
}

HookFlag::Type StringToHookFlag(const char *text)
{
	if (!text || !stricmp(text, "NONE"))
		return HookFlag::NONE;

	return !stricmp(text, "ADMIN_HOOK") ? HookFlag::ADMIN_HOOK : HookFlag::NONE;
}

CASHook::CASHook(unsigned char dllTypes, unsigned char moduleTypes, const char *nameSpace,
                 const char *name, const char *arguments, const CASHookArguments &hookArguments)
    : m_pNext(NULL), m_uiDLLTypes(dllTypes), m_uiModuleTypes(moduleTypes), m_pszNamespace(nameSpace),
      m_pszName(name), m_pszArguments(arguments),
      m_pszDescription(hookArguments.m_pszDescription ? hookArguments.m_pszDescription : "Hook function"),
      m_StopMode(hookArguments.m_StopMode), m_Flags(hookArguments.m_Flags),
      m_uiCode(HOOK_INVALID_CODE), m_pFunctionType(NULL)
{
	if (dllTypes & (1 << g_DllType))
	{
		m_pNext = m_pHead;
		m_pHead = this;
	}
}

bool CASHook::VCall(CBasePlayer *player, int argumentCount, va_list arguments)
{
	if (!CASServerManager::GetInstance()->m_bScriptLifecycleEntered)
		return false;

	if (m_uiCode == HOOK_INVALID_CODE)
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
		                             " CASHook::Call: hook '%s' has no id!\n", m_pszName);
		return false;
	}

	return CASServerManager::GetInstance()->CallFunctionOnAllModules(player,
	                                                                 m_uiModuleTypes, *this, argumentCount, arguments);
}

bool CASHook::Call(CBasePlayer *player, int argumentCount, ...)
{
	va_list arguments;

	va_start(arguments, argumentCount);
	bool handled = VCall(player, argumentCount, arguments);
	va_end(arguments);

	return handled;
}

bool CASHook::Call(int argumentCount, ...)
{
	va_list arguments;

	va_start(arguments, argumentCount);
	bool handled = VCall(NULL, argumentCount, arguments);
	va_end(arguments);

	return handled;
}

void CASHook::OutputHooksToFile(const char *fileName)
{
	if (!fileName || !*fileName)
		return;

	char path[MAX_PATH + 1];

	if ((unsigned int)UTIL_FormatString(path, sizeof(path), "%s.txt", fileName) >= sizeof(path))
	{
		ALERT(at_console, "Filename too long!\n");
		return;
	}

	CKeyvaluesLexerSettings settings;
	CKeyvaluesWriter writer(path, settings);

	if (!writer.m_hFile)
	{
		ALERT(at_error, "Could not open file '%s' for hook output\n", path);
		return;
	}

	writer.BeginBlock("Hooks");

	for (CASHook *hook = m_pHead; hook; hook = hook->m_pNext)
	{
		writer.BeginBlock("Hook");
		writer.WriteKeyvalue("namespace", hook->m_pszNamespace);
		writer.WriteKeyvalue("name", hook->m_pszName);
		writer.WriteKeyvalue("description", hook->m_pszDescription);
		writer.WriteKeyvalue("arguments", hook->m_pszArguments);
		writer.WriteKeyvalue("stopmode", HookStopModeToString(hook->m_StopMode));
		writer.WriteKeyvalue("funcdef", hook->m_pFunctionType->GetName());
		ModuleTypesToKeyvaluesBlock(writer, hook->m_uiModuleTypes);
		writer.BeginBlock("dlls");

		for (int type = DllType::CLIENT; type <= DllType::SERVER; type++)
		{
			if (hook->m_uiDLLTypes & (1 << type))
				writer.WriteKeyvalue("dll", DLLToString((DllType::Type)type));
		}

		writer.EndBlock();
		writer.BeginBlock("flags");

		if (hook->m_Flags & HookFlag::ADMIN_HOOK)
			writer.WriteKeyvalue("flag", HookFlagToString(HookFlag::ADMIN_HOOK));

		writer.EndBlock();
		writer.EndBlock();

		if (writer.m_bError)
			break;
	}

	writer.EndBlock();

	if (writer.m_bError)
		ALERT(at_error, "An error occurred while writing hooks to file '%s'\n", path);
	else
		ALERT(at_console, "Written hooks to file '%s'\n", writer.m_szFileName);
}

void RegisterSCScriptModuleHookReturnCode(CASDocumentation *documentation)
{
	const char *typeName = CASHook::HOOK_RETURN_TYPE;

	documentation->RegisterEnum("Hook function return codes.", typeName);
	documentation->RegisterEnumValue("Returned when a hook function has not handled the call",
	                                 typeName, "HOOK_CONTINUE", HOOK_CONTINUE);
	documentation->RegisterEnumValue("Returned when a hook function has handled the call",
	                                 typeName, "HOOK_HANDLED", HOOK_HANDLED);
}
