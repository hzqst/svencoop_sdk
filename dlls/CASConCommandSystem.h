#ifndef CASCONCOMMANDSYSTEM_H
#define CASCONCOMMANDSYSTEM_H

#include "CASServerScriptGlobal.h"
#include "IConCommandSystem.h"
#include "ConCommandDefs.h"
#include "CASExtensionList.h"
#include <map>
#include <stack>
#include <vector>

class CASBaseConCommand;
class CASConCommand;
class CASClientCommand;
class CASCVar;
class CASCommand;
class CASDocumentation;

class CASConCommandSystem : public CASServerScriptGlobal<CASConCommandSystem>, public IConCommandSystem
{
public:
	typedef void (*CommandQueueCallback_t)(ConCommandExecuteResult::Type result, const char *name, const CASCommand *command);

	CASConCommandSystem();
	virtual ~CASConCommandSystem();
	virtual int Execute(const CCommand &command, CBasePlayer *player, bool consoleCommand);

	int Execute(const CASCommand *command, CBasePlayer *player, bool consoleCommand);
	int Execute(CASBaseConCommand *target, const CCommand &command, CBasePlayer *player, bool consoleCommand);
	int Execute(CASBaseConCommand *target, const CASCommand *command, CBasePlayer *player, bool consoleCommand);
	void Visit(CASConCommand &command, void *context);
	void Visit(CASClientCommand &command, void *context);
	void Visit(CASCVar &variable, void *context);
	bool AddCommand(CASBaseConCommand *command);
	void RemoveCommand(CASBaseConCommand *command);
	bool CommandExists(const char *name) const;
	CASBaseConCommand *GetCommand(const char *name) const;
	bool RegisterModule(const char *moduleName, const char *nameSpace);
	void RemoveModule(const char *moduleName);
	int RemoveAllCommandsOfModule(const char *moduleName);
	const CString &GetCommandNamespace(const CString &moduleName) const;
	CBasePlayer *GetCurrentPlayer(void) const;
	void FindCommands(const char *prefix, CBasePlayer *player) const;
	void DisplayCommandStats(void) const;
	void ClearCommands(void);
	void EnqueueCommand(CASCommand *command, CommandQueueCallback_t callback);
	void ExecuteQueue(void);
	void ClearQueue(void);
	static bool IsValidName(const char *name);
	static bool IsValidNamespace(const char *nameSpace);

private:
	friend class CASCVar;
	friend class CASServerManager;
	friend void as_command_Callback( const CCommand &command );

	struct ModuleData_t
	{
		CString m_Namespace;
	};

	struct Command_t
	{
		CASCommand *m_pCommand;
		CommandQueueCallback_t	m_pCallback;
	};

	typedef std::map<CString, CASBaseConCommand *, CStringCaseInsensitiveLess> CommandMap_t;
	typedef std::map<CString, ModuleData_t, CStringCaseInsensitiveLess> ModuleMap_t;

	CommandMap_t				m_Commands;
	ModuleMap_t					m_Modules;
	std::stack<CBasePlayer *>	m_Players;
	std::vector<Command_t>		m_Queue;
	unsigned char				m_StateByte;
	bool						m_bInCVarCallback;
};

void ServerCommandQueueCallback( ConCommandExecuteResult::Type result, const char *name, const CASCommand *command );
void RegisterSCScriptConCommandSystem(CASDocumentation *documentation);
void RegisterSCScriptConCommand(CASDocumentation *documentation);
void RegisterSCScriptCVarClasses(CASDocumentation *documentation);

#endif
