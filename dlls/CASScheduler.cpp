#include "extdll.h"
#include "util.h"
#include "CASScheduler.h"
#include "CString.h"
#include "CASLog.h"
#include "CASArguments.h"
#include "CASReflection.h"
#include "CASFunction.h"
#include "CASMethod.h"
#include "CASModule.h"
#include "CASServerManager.h"
#include "CASDocumentation.h"
#include <angelscript.h>
#include <stddef.h>

static const int SCHEDULER_REPEAT_INFINITE_TIMES = -1;
static const int SCHEDULER_ARGUMENT_TYPE_ID_MASK = 0x1FFFFFFF;
static const int SCHEDULER_TIMEOUT_REPEAT_COUNT = 1;
static const unsigned int SCHEDULER_TIMEOUT_FIRST_ARGUMENT = 2;
static const unsigned int SCHEDULER_INTERVAL_FIRST_ARGUMENT = 3;
static const unsigned int SCHEDULER_OBJECT_TIMEOUT_FIRST_ARGUMENT = 3;
static const unsigned int SCHEDULER_OBJECT_INTERVAL_FIRST_ARGUMENT = 4;

CASScheduler::CASScheduler(CASModule *module)
	: REPEAT_INFINITE_TIMES(SCHEDULER_REPEAT_INFINITE_TIMES), m_pModule(module),
	  m_pFunctions(NULL), m_flLastThinkTime(0), m_pPendingFunctions(NULL),
	  m_bThinking(false), m_pCurrentFunction(NULL), m_bRemoveCurrentFunction(false)
{
}

void CASScheduler::SetTimeout(asIScriptGeneric *generic)
{
	CString *functionName = (CString *)generic->GetArgAddress(0);
	float delay = generic->GetArgFloat(1);

	SetInterval(*functionName, delay, SCHEDULER_TIMEOUT_REPEAT_COUNT,
	            SCHEDULER_TIMEOUT_FIRST_ARGUMENT, generic);
}

void CASScheduler::SetInterval(asIScriptGeneric *generic)
{
	CString *functionName = (CString *)generic->GetArgAddress(0);
	float repeatTime = generic->GetArgFloat(1);
	int repeatCount = (int)generic->GetArgDWord(2);

	if (!repeatCount || repeatCount < SCHEDULER_REPEAT_INFINITE_TIMES)
	{
		CASLog::GetInstance()->Warning(AS_LOG_DEFAULT,
		                               "CScheduler::SetInterval: can only add function '%s' if repeat count is positive and non-zero, or REPEAT_INFINITE_TIMES!\n",
		                               functionName->Data());
		generic->SetReturnAddress(NULL);
		return;
	}

	SetInterval(*functionName, repeatTime, repeatCount, SCHEDULER_INTERVAL_FIRST_ARGUMENT, generic);
}

void CASScheduler::SetTimeoutObj(asIScriptGeneric *generic)
{
	void *object = generic->GetArgAddress(0);
	int typeId = generic->GetArgTypeId(0);
	CString *functionName = (CString *)generic->GetArgAddress(1);
	float delay = generic->GetArgFloat(2);

	SetInterval(object, typeId, *functionName, delay, SCHEDULER_TIMEOUT_REPEAT_COUNT,
	            SCHEDULER_OBJECT_TIMEOUT_FIRST_ARGUMENT, generic);
}

void CASScheduler::SetIntervalObj(asIScriptGeneric *generic)
{
	void *object = generic->GetArgAddress(0);
	int typeId = generic->GetArgTypeId(0);
	CString *functionName = (CString *)generic->GetArgAddress(1);
	float repeatTime = generic->GetArgFloat(2);
	int repeatCount = (int)generic->GetArgDWord(3);

	if (!repeatCount || repeatCount < SCHEDULER_REPEAT_INFINITE_TIMES)
	{
		CASLog::GetInstance()->Warning(AS_LOG_DEFAULT,
		                               "CScheduler::SetInterval: can only add function '%s' if repeat count is positive and non-zero, or REPEAT_INFINITE_TIMES!\n",
		                               functionName->Data());
		generic->SetReturnAddress(NULL);
		return;
	}

	SetInterval(object, typeId, *functionName, repeatTime, repeatCount,
	            SCHEDULER_OBJECT_INTERVAL_FIRST_ARGUMENT, generic);
}

void CASScheduler::SetInterval(const CString &functionName, float repeatTime, int repeatCount,
                               unsigned int firstArgument, asIScriptGeneric *generic)
{
	SetInterval(NULL, 0, functionName, repeatTime, repeatCount, firstArgument, generic);
}

CASScheduler::CScheduledFunction::CScheduledFunction(CASBaseCallable *function, float nextCallTime,
	float repeatTime, int repeatCount, void *object, int typeId, CASArguments *arguments)
	: m_pNext(NULL), m_pFunction(function), m_flNextCallTime(nextCallTime),
	  m_flRepeatTime(repeatTime), m_iRepeatCount(repeatCount), m_pObject(object),
	  m_iTypeId(typeId), m_pArguments(arguments), m_bRemoved(false)
{
}

CASScheduler::CScheduledFunction::~CScheduledFunction()
{
	Remove();
}

void CASScheduler::CScheduledFunction::Release(void) const
{
	if (InternalRelease())
		delete this;
}

float CASScheduler::CScheduledFunction::GetNextCallTime(void) const
{
	return m_flNextCallTime;
}

void CASScheduler::CScheduledFunction::SetNextCallTime(float nextCallTime)
{
	m_flNextCallTime = nextCallTime;
}

float CASScheduler::CScheduledFunction::GetRepeatTime(void) const
{
	return m_flRepeatTime;
}

void CASScheduler::CScheduledFunction::SetRepeatTime(float repeatTime)
{
	// This comparison also accepts NaN
	if (!(repeatTime < 0))
		m_flRepeatTime = repeatTime;
}

int CASScheduler::CScheduledFunction::GetRepeatCount(void) const
{
	return m_iRepeatCount;
}

void CASScheduler::CScheduledFunction::SetRepeatCount(int repeatCount)
{
	if (repeatCount >= SCHEDULER_REPEAT_INFINITE_TIMES)
		m_iRepeatCount = repeatCount;
}

bool CASScheduler::CScheduledFunction::IsInfiniteRepeat(void) const
{
	return m_iRepeatCount == SCHEDULER_REPEAT_INFINITE_TIMES;
}

void CASScheduler::CScheduledFunction::MakeInfiniteRepeat(void)
{
	m_iRepeatCount = SCHEDULER_REPEAT_INFINITE_TIMES;
}

bool CASScheduler::CScheduledFunction::HasBeenRemoved(void) const
{
	return m_bRemoved;
}

void CASScheduler::CScheduledFunction::Remove(void)
{
	if (m_bRemoved)
		return;

	m_bRemoved = true;

	if (m_pArguments)
	{
		delete m_pArguments;
		m_pArguments = NULL;
	}

	if (m_pObject)
	{
		asIScriptEngine *engine = CASServerManager::GetInstance()->m_pEngine;
		asITypeInfo *type = engine->GetTypeInfoById(m_iTypeId);

		engine->ReleaseScriptObject(m_pObject, type);
		m_pObject = NULL;
	}

	if (m_pFunction)
		m_pFunction->Release();

	m_pFunction = NULL;
}

CASScheduler::~CASScheduler()
{
	ClearTimerList();
}

CASScheduler::CScheduledFunction *CASScheduler::GetCurrentFunction(void) const
{
	if (m_pCurrentFunction)
		m_pCurrentFunction->AddRef();

	return m_pCurrentFunction;
}

void CASScheduler::RemoveFunction(CScheduledFunction *previous, CScheduledFunction *function)
{
	if (previous)
		previous->m_pNext = function->m_pNext;
	else
		m_pFunctions = function->m_pNext;

	function->Remove();
	function->Release();
}

void CASScheduler::RemoveTimer(CScheduledFunction *function)
{
	if (!function)
		return;

	CScheduledFunction *previous = NULL;

	for (CScheduledFunction *entry = m_pFunctions; entry; entry = entry->m_pNext)
	{
		if (entry != function)
		{
			previous = entry;
			continue;
		}

		if (entry == m_pCurrentFunction)
			m_bRemoveCurrentFunction = true;
		else
			RemoveFunction(previous, entry);

		return;
	}
}

void CASScheduler::ClearTimerList(void)
{
	while (m_pFunctions)
	{
		CScheduledFunction *next = m_pFunctions->m_pNext;

		m_pFunctions->Remove();
		m_pFunctions->Release();
		m_pFunctions = next;
	}
}

void CASScheduler::AdjustTime(float adjustment)
{
	for (CScheduledFunction *function = m_pFunctions; function; function = function->m_pNext)
		function->m_flNextCallTime -= adjustment;
}

void CASScheduler::Think(void)
{
	m_bThinking = true;

	CScheduledFunction *previous = NULL;
	CScheduledFunction *function = m_pFunctions;

	while (function)
	{
		CScheduledFunction *next = function->m_pNext;

		m_pCurrentFunction = function;

		if (!function->IsInfiniteRepeat() && function->m_iRepeatCount <= 0)
		{
			RemoveFunction(previous, function);
			function = next;
			continue;
		}

		if (gpGlobals->time >= function->m_flNextCallTime)
		{
			CASBaseCallable *callable = function->m_pFunction;

			if (function->m_pObject)
				((CASMethod *)callable)->SetThisPointer(function->m_pObject);

			if (!function->IsInfiniteRepeat())
				function->m_iRepeatCount--;

			function->m_flNextCallTime = function->m_flRepeatTime + gpGlobals->time;

			if (callable)
			{
				if (!callable->Call(*function->m_pArguments, (CASReturnValue *)NULL))
				{
					asIScriptFunction *scriptFunction = callable->GetFunction();

					CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
					                             "CScheduler::Think: execution of function %s::%s failed!\n",
					                             scriptFunction->GetNamespace(), scriptFunction->GetName());
				}
			}
			else
			{
				CASLog::GetInstance()->Error(AS_LOG_DEFAULT, "CScheduler::Think: execution of null function!\n");
				m_bRemoveCurrentFunction = true;
			}

			if (m_bRemoveCurrentFunction || (!function->IsInfiniteRepeat() && function->m_iRepeatCount <= 0))
			{
				m_bRemoveCurrentFunction = false;
				RemoveFunction(previous, function);
				function = next;
				continue;
			}
		}

		previous = function;
		function = next;
	}

	m_flLastThinkTime = gpGlobals->time;
	m_pCurrentFunction = NULL;
	m_bThinking = false;

	if (m_pPendingFunctions)
	{
		if (previous)
			previous->m_pNext = m_pPendingFunctions;
		else
			m_pFunctions = m_pPendingFunctions;

		m_pPendingFunctions = NULL;
	}
}

class CASFunctionIterator
{
public:
	explicit CASFunctionIterator(asIScriptModule *module) : m_pModule(module) {}

	unsigned int GetCount(void) const { return m_pModule->GetFunctionCount(); }
	asIScriptFunction *GetFunction(unsigned int index) const { return m_pModule->GetFunctionByIndex(index); }

private:
	asIScriptModule *m_pModule;
};

class CASMethodIterator
{
public:
	explicit CASMethodIterator(asITypeInfo *type) : m_pType(type) {}

	unsigned int GetCount(void) const { return m_pType->GetMethodCount(); }
	asIScriptFunction *GetFunction(unsigned int index) const { return m_pType->GetMethodByIndex(index); }

private:
	asITypeInfo *m_pType;
};

template <class Iterator>
asIScriptFunction *FindFunction(asIScriptEngine *engine, const Iterator &iterator,
                                const CString &name, CASArguments &arguments)
{
	CASArgument *values = arguments.m_pArguments;

	for (unsigned int index = 0; index < iterator.GetCount(); index++)
	{
		asIScriptFunction *function = iterator.GetFunction(index);

		if (strcmp(function->GetName(), name.Data()) || function->GetReturnTypeId() != asTYPEID_VOID ||
		    function->GetParamCount() != arguments.GetCount())
			continue;

		unsigned int count = function->GetParamCount();
		unsigned int argument = 0;

		for (; argument < count; argument++)
		{
			int typeId;
			asDWORD flags;

			if (function->GetParam(argument, &typeId, &flags) < 0)
			{
				CASLog::GetInstance()->Warning(AS_LOG_DEFAULT,
				                               "CScheduler::FindFunction: Failed to retrieve parameter %u for function %s!\n",
				                               argument, function->GetName());
				break;
			}

			asITypeInfo *type = engine->GetTypeInfoById(typeId);
			asDWORD objectFlags = type ? type->GetFlags() : 0;
			ArgumentType_t argumentType = CASReflection::ArgumentTypeFromTypeId(typeId, objectFlags);
			CASArgument &value = values[argument];

			if (value.m_ArgumentType != argumentType &&
			    (value.m_ArgumentType != ARGUMENT_ENUM || argumentType != ARGUMENT_PRIMITIVE))
				break;

			if (((typeId ^ value.m_iTypeId) & SCHEDULER_ARGUMENT_TYPE_ID_MASK) != 0)
			{
				ArgumentValue_t converted;

				converted.qwordValue = 0;

				if (!CASReflection::ConvertEnumToPrimitive(value, typeId, converted) &&
				    !CASReflection::ConvertPrimitiveToEnum(value, converted))
					break;

				if (!value.Set(typeId, value.m_ArgumentType, converted, false))
				{
					CASLog::GetInstance()->Error(AS_LOG_DEFAULT, "CScheduler: Failed to convert enum value!\n");
					break;
				}
			}
		}

		if (argument == count)
			return function;
	}

	return NULL;
}

void CASScheduler::SetInterval(void *object, int typeId, const CString &functionName,
                               float repeatTime, int repeatCount, unsigned int firstArgument, asIScriptGeneric *generic)
{
	if (repeatTime < 0)
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT, "CScheduler: negative repeat time or delay is not allowed!\n");
		generic->SetReturnAddress(NULL);
		return;
	}

	CASArguments *arguments = new CASArguments;

	// Arguments stay allocated on failure
	if (!arguments->SetArguments(generic, firstArgument))
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
		                             "CScheduler: could not add function '%s', failed to parse arguments\n", functionName.Data());
		generic->SetReturnAddress(NULL);
		return;
	}

	asIScriptEngine *engine = CASServerManager::GetInstance()->m_pEngine;
	asITypeInfo *objectType = NULL;
	asIScriptFunction *function = NULL;

	if (object && (typeId & asTYPEID_OBJHANDLE))
		object = *(void **)object;

	if (object)
	{
		objectType = engine->GetTypeInfoById(typeId);

		if (!objectType)
		{
			CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
			                             "CScheduler: could not add function '%s', object type for this pointer not found!\n", functionName.Data());
		}
		else if (!(objectType->GetFlags() & asOBJ_REF))
		{
			CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
			                             "CScheduler: could not add '%s::%s::%s', object type must be a reference!\n",
			                             objectType->GetNamespace(), objectType->GetName(), functionName.Data());
		}
		else
			function = FindFunction(engine, CASMethodIterator(objectType), functionName, *arguments);
	}
	else
		function = FindFunction(engine, CASFunctionIterator(m_pModule->m_pModule), functionName, *arguments);

	if (!function)
	{
		delete arguments;
		CASLog::GetInstance()->Warning(AS_LOG_DEFAULT,
		                               "CScheduler: could not add function '%s', function not found\n", functionName.Data());
		generic->SetReturnAddress(NULL);
		return;
	}

	CScheduledFunction *scheduled = new CScheduledFunction(
	    object ? (CASBaseCallable *)new CASMethod(function, CASServerManager::GetInstance()->m_pActiveModule) : (CASBaseCallable *)new CASFunction(function, CASServerManager::GetInstance()->m_pActiveModule),
	    repeatTime + gpGlobals->time, repeatTime, repeatCount, object, typeId, arguments);

	if (object)
		engine->AddRefScriptObject(object, objectType);

	CScheduledFunction **list = m_bThinking ? &m_pPendingFunctions : &m_pFunctions;

	scheduled->m_pNext = *list;
	*list = scheduled;
	scheduled->AddRef();
	generic->SetReturnAddress(scheduled);
}

void CASScheduler_SetTimeoutHandler(asIScriptGeneric *generic)
{
	((CASScheduler *)generic->GetObject())->SetTimeout(generic);
}

void CASScheduler_SetIntervalHandler(asIScriptGeneric *generic)
{
	((CASScheduler *)generic->GetObject())->SetInterval(generic);
}

void CASScheduler_SetTimeoutObj(asIScriptGeneric *generic)
{
	((CASScheduler *)generic->GetObject())->SetTimeoutObj(generic);
}

void CASScheduler_SetIntervalObj(asIScriptGeneric *generic)
{
	((CASScheduler *)generic->GetObject())->SetIntervalObj(generic);
}

void CASScheduler_SetInterval_NoArgs(asIScriptGeneric *generic)
{
	CASScheduler *scheduler = (CASScheduler *)generic->GetObject();
	CString *functionName = (CString *)generic->GetArgAddress(0);
	float repeatTime = generic->GetArgFloat(1);

	scheduler->SetInterval(*functionName, repeatTime, scheduler->REPEAT_INFINITE_TIMES, 2, generic);
}

void CASScheduler_SetIntervalObj_NoArgs(asIScriptGeneric *generic)
{
	CASScheduler *scheduler = (CASScheduler *)generic->GetObject();
	void *object = generic->GetArgAddress(0);
	int typeId = generic->GetArgTypeId(0);
	CString *functionName = (CString *)generic->GetArgAddress(1);
	float repeatTime = generic->GetArgFloat(2);

	scheduler->SetInterval(object, typeId, *functionName, repeatTime, scheduler->REPEAT_INFINITE_TIMES, 3, generic);
}

CASScheduler *Scheduler_GetActiveScheduler( void )
{
	CASModule *module = CASServerManager::GetInstance()->m_pActiveModule;

	if ( !module )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "Attempted to get current module while outside of a script!\n" );

		return NULL;
	}

	return module->m_pScheduler;
}

void RegisterSCScriptScheduler(CASDocumentation *documentation)
{
	typedef CASScheduler::CScheduledFunction ScheduledFunction;

	documentation->RegisterObjectType("Handle to a scheduled function", "CScheduledFunction", 0, asOBJ_REF);
	documentation->RegisterObjectBehaviour("AddRef", "CScheduledFunction", asBEHAVE_ADDREF,
	                                       "void AddRef()", asMETHOD(ScheduledFunction, AddRef), asCALL_THISCALL);
	documentation->RegisterObjectBehaviour("Release", "CScheduledFunction", asBEHAVE_RELEASE,
	                                       "void Release()", asMETHOD(ScheduledFunction, Release), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Gets the next call time, in seconds", "CScheduledFunction",
	                                    "float GetNextCallTime() const", asMETHOD(ScheduledFunction, GetNextCallTime), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Sets the next call time, in seconds. Is an absolute value (g_Engine.time + RepeatTime by default).Times in the past will result in a call on the next frame.",
	                                    "CScheduledFunction", "void SetNextCallTime(const float flNextCallTime)", asMETHOD(ScheduledFunction, SetNextCallTime), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Gets the repeat time, in seconds", "CScheduledFunction",
	                                    "float GetRepeatTime() const", asMETHOD(ScheduledFunction, GetRepeatTime), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Sets the repeat time, in seconds. Must be a positive value (may be zero)",
	                                    "CScheduledFunction", "void SetRepeatTime(const float flRepeatTime)", asMETHOD(ScheduledFunction, SetRepeatTime), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Gets the repeat count", "CScheduledFunction",
	                                    "int GetRepeatCount() const", asMETHOD(ScheduledFunction, GetRepeatCount), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Returns whether this function repeats infinitely", "CScheduledFunction",
	                                    "bool IsInfiniteRepeat() const", asMETHOD(ScheduledFunction, IsInfiniteRepeat), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Sets the repeat count. Must be a positive value (may be zero) or REPEAT_INFINITE_TIMES",
	                                    "CScheduledFunction", "void SetRepeatCount(const int iRepeatCount)", asMETHOD(ScheduledFunction, SetRepeatCount), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Makes this an infinite repeat function", "CScheduledFunction",
	                                    "void MakeInfiniteRepeat()", asMETHOD(ScheduledFunction, MakeInfiniteRepeat), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Returns whether this function has been removed from the scheduler or not", "CScheduledFunction",
	                                    "bool HasBeenRemoved() const", asMETHOD(ScheduledFunction, HasBeenRemoved), asCALL_THISCALL);

	documentation->RegisterObjectType("Scheduler for calling functions.\nSupports up to 8 parameters for function or method calls.",
	                                  "CScheduler", sizeof(CASScheduler), asOBJ_REF | asOBJ_NOCOUNT);
	documentation->RegisterObjectProperty("If passed to AddTimer, causes the given function to be called infinite times",
	                                      "CScheduler", "const int REPEAT_INFINITE_TIMES", asOFFSET(CASScheduler, REPEAT_INFINITE_TIMES));

	static const char timeoutDescription[] = "Adds a timer that calls the given function after flDelay seconds.";
	static const char objectTimeoutDescription[] = "Adds a timer that calls the given function after flDelay seconds. Calls object methods.";
	static const char intervalDescription[] = "Adds a timer that calls the given function every flRepeatTime seconds,\nfor iRepeatCount times (or infinite times if REPEAT_INFINITE_TIMES).";
	static const char objectIntervalDescription[] = "Adds a timer that calls the given function every flRepeatTime seconds,\nfor iRepeatCount times (or infinite times if REPEAT_INFINITE_TIMES).\nCalls object methods.";
	static const char infiniteDescription[] = "Adds a timer that calls the given function every flRepeatTime seconds,\nfor iRepeatCount times (or infinite times if REPEAT_INFINITE_TIMES).\nThe given function is called an infinite number of times.";
	static const char objectInfiniteDescription[] = "Adds a timer that calls the given function every flRepeatTime seconds,\nfor iRepeatCount times (or infinite times if REPEAT_INFINITE_TIMES).\nThe given function is called an infinite number of times.\nCalls object methods.";

	struct SchedulerMethod
	{
		const char *description;
		const char *declaration;

		void (*function)(asIScriptGeneric *);
	};

	static const SchedulerMethod methods[] = {
		{ timeoutDescription, "CScheduledFunction@ SetTimeout(const string& in szFunction, float flDelay)", CASScheduler_SetTimeoutHandler },
		{ timeoutDescription, "CScheduledFunction@ SetTimeout(const string& in szFunction, float flDelay, ?& in)", CASScheduler_SetTimeoutHandler },
		{ timeoutDescription, "CScheduledFunction@ SetTimeout(const string& in szFunction, float flDelay, ?& in, ?& in)", CASScheduler_SetTimeoutHandler },
		{ timeoutDescription, "CScheduledFunction@ SetTimeout(const string& in szFunction, float flDelay, ?& in, ?& in, ?& in)", CASScheduler_SetTimeoutHandler },
		{ timeoutDescription, "CScheduledFunction@ SetTimeout(const string& in szFunction, float flDelay, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetTimeoutHandler },
		{ timeoutDescription, "CScheduledFunction@ SetTimeout(const string& in szFunction, float flDelay, ?& in, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetTimeoutHandler },
		{ timeoutDescription, "CScheduledFunction@ SetTimeout(const string& in szFunction, float flDelay, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetTimeoutHandler },
		{ timeoutDescription, "CScheduledFunction@ SetTimeout(const string& in szFunction, float flDelay, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetTimeoutHandler },
		{ timeoutDescription, "CScheduledFunction@ SetTimeout(const string& in szFunction, float flDelay, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetTimeoutHandler },
		{ objectTimeoutDescription, "CScheduledFunction@ SetTimeout(?& in thisObject, const string& in szFunction, float flDelay)", CASScheduler_SetTimeoutObj },
		{ objectTimeoutDescription, "CScheduledFunction@ SetTimeout(?& in thisObject, const string& in szFunction, float flDelay, ?& in)", CASScheduler_SetTimeoutObj },
		{ objectTimeoutDescription, "CScheduledFunction@ SetTimeout(?& in thisObject, const string& in szFunction, float flDelay, ?& in, ?& in)", CASScheduler_SetTimeoutObj },
		{ objectTimeoutDescription, "CScheduledFunction@ SetTimeout(?& in thisObject, const string& in szFunction, float flDelay, ?& in, ?& in, ?& in)", CASScheduler_SetTimeoutObj },
		{ objectTimeoutDescription, "CScheduledFunction@ SetTimeout(?& in thisObject, const string& in szFunction, float flDelay, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetTimeoutObj },
		{ objectTimeoutDescription, "CScheduledFunction@ SetTimeout(?& in thisObject, const string& in szFunction, float flDelay, ?& in, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetTimeoutObj },
		{ objectTimeoutDescription, "CScheduledFunction@ SetTimeout(?& in thisObject, const string& in szFunction, float flDelay, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetTimeoutObj },
		{ objectTimeoutDescription, "CScheduledFunction@ SetTimeout(?& in thisObject, const string& in szFunction, float flDelay, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetTimeoutObj },
		{ objectTimeoutDescription, "CScheduledFunction@ SetTimeout(?& in thisObject, const string& in szFunction, float flDelay, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetTimeoutObj },
		{ infiniteDescription, "CScheduledFunction@ SetInterval(const string& in szFunction, float flRepeatTime)", CASScheduler_SetInterval_NoArgs },
		{ objectInfiniteDescription, "CScheduledFunction@ SetInterval(?& in thisObject, const string& in szFunction, float flRepeatTime)", CASScheduler_SetIntervalObj_NoArgs },
		{ intervalDescription, "CScheduledFunction@ SetInterval(const string& in szFunction, float flRepeatTime, int iRepeatCount)", CASScheduler_SetIntervalHandler },
		{ intervalDescription, "CScheduledFunction@ SetInterval(const string& in szFunction, float flRepeatTime, int iRepeatCount, ?& in)", CASScheduler_SetIntervalHandler },
		{ intervalDescription, "CScheduledFunction@ SetInterval(const string& in szFunction, float flRepeatTime, int iRepeatCount, ?& in, ?& in)", CASScheduler_SetIntervalHandler },
		{ intervalDescription, "CScheduledFunction@ SetInterval(const string& in szFunction, float flRepeatTime, int iRepeatCount, ?& in, ?& in, ?& in)", CASScheduler_SetIntervalHandler },
		{ intervalDescription, "CScheduledFunction@ SetInterval(const string& in szFunction, float flRepeatTime, int iRepeatCount, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetIntervalHandler },
		{ intervalDescription, "CScheduledFunction@ SetInterval(const string& in szFunction, float flRepeatTime, int iRepeatCount, ?& in, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetIntervalHandler },
		{ intervalDescription, "CScheduledFunction@ SetInterval(const string& in szFunction, float flRepeatTime, int iRepeatCount, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetIntervalHandler },
		{ intervalDescription, "CScheduledFunction@ SetInterval(const string& in szFunction, float flRepeatTime, int iRepeatCount, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetIntervalHandler },
		{ intervalDescription, "CScheduledFunction@ SetInterval(const string& in szFunction, float flRepeatTime, int iRepeatCount, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetIntervalHandler },
		{ objectIntervalDescription, "CScheduledFunction@ SetInterval(?& in thisObject, const string& in szFunction, float flRepeatTime, int iRepeatCount)", CASScheduler_SetIntervalObj },
		{ objectIntervalDescription, "CScheduledFunction@ SetInterval(?& in thisObject, const string& in szFunction, float flRepeatTime, int iRepeatCount, ?& in)", CASScheduler_SetIntervalObj },
		{ objectIntervalDescription, "CScheduledFunction@ SetInterval(?& in thisObject, const string& in szFunction, float flRepeatTime, int iRepeatCount, ?& in, ?& in)", CASScheduler_SetIntervalObj },
		{ objectIntervalDescription, "CScheduledFunction@ SetInterval(?& in thisObject, const string& in szFunction, float flRepeatTime, int iRepeatCount, ?& in, ?& in, ?& in)", CASScheduler_SetIntervalObj },
		{ objectIntervalDescription, "CScheduledFunction@ SetInterval(?& in thisObject, const string& in szFunction, float flRepeatTime, int iRepeatCount, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetIntervalObj },
		{ objectIntervalDescription, "CScheduledFunction@ SetInterval(?& in thisObject, const string& in szFunction, float flRepeatTime, int iRepeatCount, ?& in, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetIntervalObj },
		{ objectIntervalDescription, "CScheduledFunction@ SetInterval(?& in thisObject, const string& in szFunction, float flRepeatTime, int iRepeatCount, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetIntervalObj },
		{ objectIntervalDescription, "CScheduledFunction@ SetInterval(?& in thisObject, const string& in szFunction, float flRepeatTime, int iRepeatCount, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetIntervalObj },
		{ objectIntervalDescription, "CScheduledFunction@ SetInterval(?& in thisObject, const string& in szFunction, float flRepeatTime, int iRepeatCount, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in, ?& in)", CASScheduler_SetIntervalObj }
	};

	for (unsigned int index = 0; index < sizeof(methods) / sizeof(methods[0]); index++)
	{
		const SchedulerMethod &method = methods[index];

		documentation->RegisterObjectMethod(method.description, "CScheduler", method.declaration,
		                                    asFUNCTION(method.function), asCALL_GENERIC);
	}

	documentation->RegisterObjectMethod("Removes the given function from the list of functions to call.",
	                                    "CScheduler", "void RemoveTimer(CScheduledFunction@ pFunction)", asMETHOD(CASScheduler, RemoveTimer), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Returns the function currently being executed, if any.",
	                                    "CScheduler", "CScheduledFunction@ GetCurrentFunction() const", asMETHOD(CASScheduler, GetCurrentFunction), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Removes all queued functions from the list of functions to call.",
	                                    "CScheduler", "void ClearTimerList()", asMETHOD(CASScheduler, ClearTimerList), asCALL_THISCALL);

	documentation->RegisterGlobalFunction("Gets the current script's scheduler.",
	                                      "CScheduler@ get_g_Scheduler()", asFUNCTION( Scheduler_GetActiveScheduler ), asCALL_CDECL);
}
