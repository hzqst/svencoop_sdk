#ifndef CASBASEMANAGER_H
#define CASBASEMANAGER_H

#include "Platform.h"
#include "CASDirectoryList.h"
#include "CASReflection.h"
#include <map>
#include <stdarg.h>
#include <string>
#include <vector>

class asIScriptEngine;
class asITypeInfo;
struct asSMessageInfo;
class IASImplementationFactory;
class IASScriptGlobal;
class CASDebugger;
class CASDocumentation;
class CASModule;
class CASModuleHookManager;
class CASScheduler;
class CASModuleHandle;
class CASVirtualFileSystem;
class CASFile;
class CString;
class CScriptArray;
class CScriptAny;
class CASReturnValue;
class asIScriptGeneric;
class CASHook;
class CBasePlayer;
class CCommand;
struct entvars_s;

extern "C" DLLEXPORT void custom( entvars_s *pev );

class CASBaseManager
{
public:
	CASBaseManager(IASImplementationFactory *factory);
	~CASBaseManager();

	virtual int Startup(void);
	virtual void Shutdown(void);
	virtual void OnMapBegin(void);
	virtual void LoadModules(void);
	virtual void OnMapEnd(const char *nextMap);
	virtual void Think(void);
	virtual void SetupFileSystem(CASVirtualFileSystem &fileSystem);
	virtual void PreMapInit(void) {}
	virtual bool OnMapInit(void);
	virtual void PostMapInit(bool success) {}
	virtual void PreMapActivate(void) {}
	virtual bool OnMapActivate(void);
	virtual void PostMapActivate(bool success) {}
	virtual void PreMapStart(void) {}
	virtual bool OnMapStart(void);
	virtual void PostMapStart(bool success) {}

	// MSVC reverses this overload group
	// Keep the order to match vtables on both platforms
	virtual bool CallFunctionOnAllModules(CBasePlayer *player, unsigned char moduleTypes,
		const CASHook &hook, int argumentCount, va_list arguments);
	virtual bool CallFunctionOnAllModules(CBasePlayer *player, unsigned char moduleTypes,
		const CASHook &hook, int argumentCount, ...);

	void MessageCallback(const asSMessageInfo *message);
	bool IsScriptLifecycleEntered(void) const;
	CASModule *GetMapModule(void) const;
	bool IsActiveModuleOfType(ModuleType::Type type) const;
	void LoadMapScript(const char *fileName);
	void MapInit(void);
	void MapActivate(void);
	void MapStart(void);
	void Update(void);
	bool SetExceptionOnActiveContext(const char *message, bool logMissingContext);
	void AddScriptGlobal(IASScriptGlobal *global);
	void RemoveScriptGlobal(IASScriptGlobal *global);
	bool IsRegisteredScriptGlobal(IASScriptGlobal *global);
	asITypeInfo *GetTypeInfoByName(const std::string &name);
	void ClearObjectTypeCache(void);

private:
	friend class CASPluginManager;
	friend class CASWaypointManager;
	friend class CASMapCycle;
	friend class CTriggerScript;
	friend class CSquadMaker;
	friend class CASServerManager;
	friend class CCustomEntityHandler;
	friend class CASEntityFuncs;
	friend class CBaseEntity;
	friend bool UserData_CheckModuleAccess( void );
	friend class CClassicMode;
	friend void ::custom( entvars_s *pev );
	template<class T>
	friend T *ASNotifyGarbageCollector(const char *typeName, T *object);

	friend class CASPersistence;
	friend class CASConCommandSystem;
	friend class CASPlayerFuncs;
	friend class CASUtility;
	friend class CASEntityLoader;
	friend class CASBaseConCommand;

	friend CASModule *Module_GetActiveModule( void );
	friend CASModuleHookManager *ModuleHookManager_GetActiveHookManager( void );
	friend CASScheduler *Scheduler_GetActiveScheduler( void );

	friend void RegisterSCScriptModuleHookManager(CASDocumentation *documentation);
	friend void RegisterSCScriptModule(CASDocumentation *documentation);
	friend void RegisterSCScriptScheduler(CASDocumentation *documentation);
	friend void ASPrintVariable(void *file, int typeId, void *address, const char *name);
	friend void ASPrintGlobals( const char *fileName );
	friend void ASOutputConfig( const char *fileName );
	friend void as_fs_dumpfilesystem_Callback( const CCommand &command );
	friend void as_fs_quota_usage_Callback( const CCommand &command );
	friend void as_fs_validate_file_usage_Callback( const CCommand &command );
	friend void as_execute_Callback( const CCommand &command );

	friend class CASArgument;
	friend class CASArguments;
	friend class CASBaseCallable;
	friend class CASContextPool;
	friend class CASScriptInfo;
	friend class CASTextMenu;
	friend class CASVote;
	friend class CASScriptDebugger;
	friend class CASHook;
	friend class CASModuleHookManager;
	friend class CASModule;
	friend class CASModuleBuilder;
	friend class CASScriptEntityPreprocessor;
	friend class CASCommonServerModuleValidation;
	friend class CASObjectType;
	friend class CASObjectInstance;
	friend class CASScheduler;
	friend class CASBaseRegistration;
	friend class CASServerRegistration;
	friend class CASEngineReflectionGroup;
	friend class CASModuleReflectionGroup;
	friend void RegisterSCScriptFileSystem(CASDocumentation *documentation);
	friend void RegisterSCScriptVirtualFileSystem(CASDocumentation *);
	friend CScriptArray *StringSplit(const CString &, const CString &);
	friend CScriptAny *CASReturnValue_ToAny(const CASReturnValue *);
	friend void CASMethod_Call_Varargs(asIScriptGeneric *);
	friend void CASMethod_Call(asIScriptGeneric *);
	friend bool ASSPrintf(char *, unsigned int, const char *, unsigned int, asIScriptGeneric *);
	friend CASFile *CASVirtualFileSystem_OpenFile(CASVirtualFileSystem *, const CString &, unsigned char);
	friend void CASVirtualFileSystem_RemoveFile(CASVirtualFileSystem *, const CString &);
	friend bool CASReflection::GetReturnValue(CASArgument &value, int typeId, asDWORD flags,
		asIScriptContext *context);
	friend bool CASReflection::GetReturnValue(void *value, int typeId, asDWORD flags,
		asIScriptContext *context, CASModule *module);
	friend bool CASReflection::SetArguments(asIScriptFunction *function, asIScriptContext *context,
		CASArguments &arguments);
	friend bool CASReflection::SetArguments(asIScriptFunction *function, asIScriptContext *context,
		va_list arguments);

	enum
	{
		COMPILER_WARNING_LOG_LEVEL = 1,
		COMPILER_INFORMATION_LOG_LEVEL = 2
	};

	static const float GARBAGE_COLLECT_INTERVAL;

	bool										m_bBuildAttempted;
	bool										m_bBuildSucceeded;
	bool										m_bScriptLifecycleEntered;

	IASImplementationFactory					*m_pFactory;
	asIScriptEngine								*m_pEngine;
	CASDebugger									*m_pDebugger;
	CASModuleHandle								*m_pMapModule;
	CASModule									*m_pActiveModule;
	std::vector<IASScriptGlobal *>				m_ScriptGlobals;
	CASVirtualFileSystem						*m_pFileSystem;
	std::map<std::string, asITypeInfo *>		m_ObjectTypeCache;

	float										m_flNextGarbageCollect;
	int											m_iPhase;
};

struct cvar_s;
extern cvar_s as_compiler_treat_warnings_as_errors;

#endif
