#include "extdll.h"
#include "CASModule.h"
#include "CASFunction.h"
#include "CASMethod.h"
#include "CASObjectType.h"
#include "CASObjectInstance.h"
#include "CASModuleInfo.h"
#include "CASModuleHookManager.h"
#include "CASScheduler.h"
#include "CASScriptInfo.h"
#include "CASServerManager.h"
#include "CASDocumentation.h"
#include "ASPrintVariable.h"
#include "server_support.h"
#include "FileSystem.h"
#include "CASLog.h"

#include <angelscript.h>

#define MODULE_USER_DATA_ID 10000

CASModule::CASModule(asIScriptModule *module, CASModuleInfo *moduleInfo)
    : m_pModule(module), m_pModuleInfo(moduleInfo),
      m_pScriptInfo(new CASScriptInfo(moduleInfo->m_pszInitDeclaration)),
      m_pScheduler(NULL), m_pHookManager(NULL), m_bValid(true)
{
	m_pScheduler = new CASScheduler(this);
	m_pHookManager = new CASModuleHookManager(this);
	m_pModule->SetUserData(this, MODULE_USER_DATA_ID);

	m_bHasCustomEntities = false;
	m_bDelayedRemoval = false;
	m_bDelayedReload = false;
	m_bDisallowedForNextMap = false;
}

CASModule::~CASModule()
{
	m_pScriptInfo->Release();

	delete m_pHookManager;
	m_pHookManager = NULL;

	delete m_pScheduler;
	m_pScheduler = NULL;
}

void CASModule::Invalidate(void)
{
	if (!m_bValid)
		return;

	m_bValid = false;
	m_pScheduler->ClearTimerList();

	if (m_pHookManager)
		m_pHookManager->RemoveAllHooks();

	ClearFunctionCache();

	if (m_pModule)
	{
		CASModule *previousModule = CASServerManager::GetInstance()->m_pActiveModule;

		CASServerManager::GetInstance()->m_pActiveModule = this;
		m_pModule->Discard();
		CASServerManager::GetInstance()->m_pActiveModule = previousModule;
		m_pModule = NULL;
	}
}

void CASModule::Think(void)
{
	m_pScheduler->Think();
}

void CASModule::Release(void) const
{
	if (!InternalRelease())
		return;

	CASModule *module = const_cast<CASModule *>(this);

	if (m_bValid)
		module->Invalidate();

	delete module;
}

const char *CASModule::GetModuleName(void) const
{
	return m_pModule->GetName();
}

CASScriptInfo *CASModule::GetScriptInfo(void) const
{
	return m_pScriptInfo;
}

bool CASModule::FunctionExists(const char *declaration) const
{
	if (!declaration || !*declaration)
		return false;

	if (!m_pModule)
	{
		CASLog::GetInstance()->Warning(AS_LOG_DEFAULT,
			"Module not loaded, can't find function %s!\n", declaration);
		return false;
	}

	return m_pModule->GetFunctionByDecl(declaration) != NULL;
}

bool CASModule::ClassExists(const char *name) const
{
	if (!name)
		return false;

	if (!m_pModule)
	{
		CASLog::GetInstance()->Warning(AS_LOG_DEFAULT,
			"Module not loaded, can't find class %s!\n", name);
		return false;
	}

	return m_pModule->GetTypeInfoByName(name) != NULL;
}

bool CASModule::MethodExists(const char *className, const char *methodDeclaration) const
{
	if (!className || !methodDeclaration)
		return false;

	if (!m_pModule)
	{
		CASLog::GetInstance()->Warning(AS_LOG_DEFAULT,
			"Module not loaded, can't find method %s::%s!\n", className, methodDeclaration);
		return false;
	}

	asITypeInfo *type = m_pModule->GetTypeInfoByName(className);

	if (!type)
		return false;

	return type->GetMethodByDecl(methodDeclaration) != NULL;
}

CASFunction *CASModule::GetFunction(const char *declaration)
{
	if (!declaration)
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT, "CASModule::GetFunction: null function signature\n");
		return NULL;
	}

	if (!m_pModule)
	{
		CASLog::GetInstance()->Warning(AS_LOG_DEFAULT,
			"Module not loaded, can't execute function %s!\n", declaration);
		return NULL;
	}

	std::map<std::string, CASFunction *>::iterator entry = m_FunctionCache.find(declaration);

	if (entry != m_FunctionCache.end())
		return entry->second;

	asIScriptFunction *function = m_pModule->GetFunctionByDecl(declaration);
	CASFunction *callable = function ? new CASFunction(function, this) : NULL;

	std::pair<std::map<std::string, CASFunction *>::iterator, bool> result =
		m_FunctionCache.insert(std::make_pair(declaration, callable));

	if (result.first != m_FunctionCache.end())
		return callable;

	CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
		"CASModule::GetFunction: failed to insert function '%s' into function cache!\n", declaration);
	callable->Release();
	return NULL;
}

void CASModule::ClearFunctionCache(void)
{
	for (std::map<std::string, CASFunction *>::iterator entry = m_FunctionCache.begin();
		entry != m_FunctionCache.end(); entry++)
	{
		if (entry->second)
			entry->second->Release();
	}

	m_FunctionCache.clear();
}

CASObjectType *CASModule::GetObjectType(const char *declaration)
{
	if (!declaration)
		return NULL;

	if (!m_pModule)
	{
		CASLog::GetInstance()->Warning(AS_LOG_DEFAULT,
		                               "Module not loaded, can't get class %s!\n", declaration);
		return NULL;
	}

	asITypeInfo *type = m_pModule->GetTypeInfoByName(declaration);

	if (!type)
		return NULL;

	return new CASObjectType(type, this);
}

CASObjectInstance *CASModule::CreateObjectInstance(const char *declaration)
{
	CASObjectType *type = GetObjectType(declaration);

	if (!type)
		return NULL;

	CASObjectInstance *instance = type->CreateInstance();

	type->Release();
	return instance;
}

CASMethod *CASModule::GetMethod(const char *classDeclaration, const char *methodDeclaration)
{
	if (!classDeclaration || !methodDeclaration)
		return NULL;

	if (!m_pModule)
	{
		CASLog::GetInstance()->Warning(AS_LOG_DEFAULT,
			"Module not loaded, can't execute method %s::%s!\n",
			classDeclaration, methodDeclaration);
		return NULL;
	}

	asITypeInfo *type = m_pModule->GetTypeInfoByName(classDeclaration);

	if (!type)
		return NULL;

	asIScriptFunction *function = type->GetMethodByDecl(methodDeclaration);

	if (!function)
		return NULL;

	return new CASMethod(function, this);
}

bool CASModule::CallFunction(void *returnValue, bool optional, const char *declaration,
                             int argumentCount, va_list arguments)
{
	CASFunction *function = GetFunction(declaration);

	if (function)
		return function->Call(returnValue, argumentCount, arguments);

	if (!optional)
		CASLog::GetInstance()->Warning(AS_LOG_DEFAULT,
		                               "Attempted to execute non-existent function \"%s\"!\n", declaration);

	return false;
}

bool CASModule::CallFunction(void *returnValue, bool optional, const char *declaration,
                             int argumentCount, ...)
{
	va_list arguments;

	va_start(arguments, argumentCount);
	bool success = CallFunction(returnValue, optional, declaration, argumentCount, arguments);
	va_end(arguments);

	return success;
}

bool CASModule::CallFunction(bool optional, const char *declaration, int argumentCount, va_list arguments)
{
	CASFunction *function = GetFunction(declaration);

	if (function)
		return function->Call(argumentCount, arguments);

	if (!optional)
		CASLog::GetInstance()->Warning(AS_LOG_DEFAULT,
		                               "Attempted to execute non-existent function \"%s\"!\n", declaration);

	return false;
}

bool CASModule::CallFunction(bool optional, const char *declaration, int argumentCount, ...)
{
	va_list arguments;

	va_start(arguments, argumentCount);
	bool success = CallFunction(optional, declaration, argumentCount, arguments);
	va_end(arguments);

	return success;
}

bool CASModule::CallFunction(void *returnValue, const char *declaration, int argumentCount, va_list arguments)
{
	return CallFunction(returnValue, false, declaration, argumentCount, arguments);
}

bool CASModule::CallFunction(void *returnValue, const char *declaration, int argumentCount, ...)
{
	va_list arguments;

	va_start(arguments, argumentCount);
	bool success = CallFunction(returnValue, false, declaration, argumentCount, arguments);
	va_end(arguments);

	return success;
}

bool CASModule::CallFunction(const char *declaration, int argumentCount, va_list arguments)
{
	return CallFunction(false, declaration, argumentCount, arguments);
}

bool CASModule::CallFunction(const char *declaration, int argumentCount, ...)
{
	va_list arguments;

	va_start(arguments, argumentCount);
	bool success = CallFunction(false, declaration, argumentCount, arguments);
	va_end(arguments);

	return success;
}

bool CASModule::CallMethod(void *returnValue, bool optional, void *object,
                           const char *classDeclaration, const char *methodDeclaration,
                           int argumentCount, va_list arguments)
{
	CASMethod *method = GetMethod(classDeclaration, methodDeclaration);

	if (!method)
	{
		if (!optional)
			CASLog::GetInstance()->Warning(AS_LOG_DEFAULT, "Attempted to execute non-existent method!\n");

		return false;
	}

	method->SetThisPointer(object);

	bool success = method->Call(returnValue, argumentCount, arguments);

	method->Release();
	return success;
}

bool CASModule::CallMethod(void *returnValue, bool optional, void *object,
                           const char *classDeclaration, const char *methodDeclaration, int argumentCount, ...)
{
	va_list arguments;

	va_start(arguments, argumentCount);
	bool success = CallMethod(returnValue, optional, object, classDeclaration,
	                          methodDeclaration, argumentCount, arguments);
	va_end(arguments);

	return success;
}

bool CASModule::CallMethod(bool optional, void *object, const char *classDeclaration,
                           const char *methodDeclaration, int argumentCount, va_list arguments)
{
	CASMethod *method = GetMethod(classDeclaration, methodDeclaration);

	if (!method)
	{
		if (!optional)
			CASLog::GetInstance()->Warning(AS_LOG_DEFAULT, "Attempted to execute non-existent method!\n");

		return false;
	}

	method->SetThisPointer(object);

	bool success = method->Call(argumentCount, arguments);

	method->Release();
	return success;
}

bool CASModule::CallMethod(bool optional, void *object, const char *classDeclaration,
                           const char *methodDeclaration, int argumentCount, ...)
{
	va_list arguments;

	va_start(arguments, argumentCount);
	bool success = CallMethod(optional, object, classDeclaration, methodDeclaration, argumentCount, arguments);
	va_end(arguments);

	return success;
}

bool CASModule::CallMethod(void *returnValue, void *object, const char *classDeclaration,
                           const char *methodDeclaration, int argumentCount, va_list arguments)
{
	return CallMethod(returnValue, false, object, classDeclaration, methodDeclaration, argumentCount, arguments);
}

bool CASModule::CallMethod(void *returnValue, void *object, const char *classDeclaration,
                           const char *methodDeclaration, int argumentCount, ...)
{
	va_list arguments;

	va_start(arguments, argumentCount);
	bool success = CallMethod(returnValue, false, object, classDeclaration,
	                          methodDeclaration, argumentCount, arguments);
	va_end(arguments);

	return success;
}

bool CASModule::CallMethod(void *object, const char *classDeclaration, const char *methodDeclaration,
                           int argumentCount, va_list arguments)
{
	return CallMethod(false, object, classDeclaration, methodDeclaration, argumentCount, arguments);
}

bool CASModule::CallMethod(void *object, const char *classDeclaration, const char *methodDeclaration,
                           int argumentCount, ...)
{
	va_list arguments;

	va_start(arguments, argumentCount);
	bool success = CallMethod(false, object, classDeclaration, methodDeclaration, argumentCount, arguments);
	va_end(arguments);

	return success;
}

void CASModule::PrintGlobals(void *file)
{
	if (!file)
		return;

	g_pFileSystem->FPrintf(file, "%s:\n", m_pModule->GetName());

	unsigned int count = m_pModule->GetGlobalVarCount();

	for (unsigned int index = 0; index < count; index++)
	{
		const char *name = NULL;
		const char *nameSpace = NULL;
		int         typeId;
		bool        isConst;

		if (m_pModule->GetGlobalVar(index, &name, &nameSpace, &typeId, &isConst) < 0)
		{
			CASLog::GetInstance()->Warning(AS_LOG_DEFAULT, "Couldn't get global var information\n");
			continue;
		}

		void *address = m_pModule->GetAddressOfGlobalVar(index);
		char  qualifiedName[256];

		if (strlen(nameSpace) + strlen(name) < sizeof(qualifiedName))
		{
			if (*nameSpace)
				UTIL_FormatString(qualifiedName, sizeof(qualifiedName), "%s::%s", nameSpace, name);
			else
				UTIL_FormatString(qualifiedName, sizeof(qualifiedName), "%s", name);

			ASPrintVariable(file, typeId, address, qualifiedName);
		}
		else
			CASLog::GetInstance()->Warning(AS_LOG_DEFAULT,
			                               "Can't print variable %s::%s information; name is too long!\n", nameSpace, name);
	}
}

CASModule *Module_GetActiveModule( void )
{
	CASModule *module = CASServerManager::GetInstance()->m_pActiveModule;

	if ( !module )
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT,
		                              "Attempted to get current module while outside of a script!\n" );

	return module;
}

CString Module_GetModuleName( CASModule *module )
{
	return module->GetModuleName();
}

void RegisterSCScriptModule(CASDocumentation *documentation)
{
	documentation->RegisterObjectType("Module class.\nRepresents this module.", "CModule",
	                                  sizeof(CASModule), asOBJ_REF | asOBJ_NOCOUNT);

	documentation->RegisterObjectMethod("Gets the name of this module.", "CModule",
	                                    "string GetModuleName() const", asFUNCTION( Module_GetModuleName ), asCALL_CDECL_OBJLAST);
	documentation->RegisterObjectMethod("Gets this module's script info object.", "CModule",
	                                    "CScriptInfo@+ get_ScriptInfo()", asMETHOD(CASModule, GetScriptInfo), asCALL_THISCALL);

	documentation->RegisterGlobalFunction("Gets the current script's module.",
	                                      "CModule@ get_g_Module()", asFUNCTION( Module_GetActiveModule ), asCALL_CDECL);
}
