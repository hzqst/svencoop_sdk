#ifndef CASSCHEDULER_H
#define CASSCHEDULER_H

#include "CASBaseClass.h"

class asIScriptGeneric;
class CASDocumentation;
class CASModule;
class CASBaseCallable;
class CASArguments;
class CString;

class CASScheduler : public CASBaseClass
{
public:
	class CScheduledFunction : public CASRefCountedBaseClass
	{
	public:
		~CScheduledFunction();

		void Release(void) const;
		float GetNextCallTime(void) const;
		void SetNextCallTime(float nextCallTime);
		float GetRepeatTime(void) const;
		void SetRepeatTime(float repeatTime);
		int GetRepeatCount(void) const;
		void SetRepeatCount(int repeatCount);
		bool IsInfiniteRepeat(void) const;
		void MakeInfiniteRepeat(void);
		bool HasBeenRemoved(void) const;
		void Remove(void);

	private:
		friend class CASScheduler;

		CScheduledFunction(CASBaseCallable *function, float nextCallTime, float repeatTime,
		                   int repeatCount, void *object, int typeId, CASArguments *arguments);

		CScheduledFunction *m_pNext;
		CASBaseCallable    *m_pFunction;
		float               m_flNextCallTime;
		float               m_flRepeatTime;
		int                 m_iRepeatCount;
		void               *m_pObject;
		int                 m_iTypeId;
		CASArguments       *m_pArguments;
		bool                m_bRemoved;
	};

	CASScheduler(CASModule *module);
	~CASScheduler();

	void Think(void);
	void AdjustTime(float adjustment);
	void RemoveTimer(CScheduledFunction *function);
	CScheduledFunction *GetCurrentFunction(void) const;
	void ClearTimerList(void);

	void SetTimeout(asIScriptGeneric *generic);
	void SetInterval(asIScriptGeneric *generic);
	void SetTimeoutObj(asIScriptGeneric *generic);
	void SetIntervalObj(asIScriptGeneric *generic);

	const int REPEAT_INFINITE_TIMES;

private:
	friend void CASScheduler_SetInterval_NoArgs(asIScriptGeneric *generic);
	friend void CASScheduler_SetIntervalObj_NoArgs(asIScriptGeneric *generic);

	void RemoveFunction(CScheduledFunction *previous, CScheduledFunction *function);
	void SetInterval(const CString &functionName, float repeatTime, int repeatCount,
	                 unsigned int firstArgument, asIScriptGeneric *generic);
	void SetInterval(void *object, int typeId, const CString &functionName,
	                 float repeatTime, int repeatCount, unsigned int firstArgument, asIScriptGeneric *generic);

	CASModule           *m_pModule;
	CScheduledFunction  *m_pFunctions;
	float                m_flLastThinkTime;
	CScheduledFunction  *m_pPendingFunctions;
	bool                 m_bThinking;
	CScheduledFunction  *m_pCurrentFunction;
	bool                 m_bRemoveCurrentFunction;
};

void RegisterSCScriptScheduler(CASDocumentation *documentation);

#endif