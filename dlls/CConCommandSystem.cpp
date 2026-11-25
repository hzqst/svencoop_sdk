#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "CConCommandSystem.h"
#include "CBaseConCommand.h"
#include "CConCommand.h"
#include "CClientCommand.h"
#include "CCVar.h"
#include "CCommand.h"
#include "ConCommandDefs.h"
#include "ConCommandUtils.h"

enum
{
	COMMAND_SYSTEM_STOPPED,
	COMMAND_SYSTEM_STARTING,
	COMMAND_SYSTEM_RUNNING,
	COMMAND_SYSTEM_STOPPING
};

namespace
{
struct CommandContext
{
	const CCommand *m_pCommand;
	bool	m_bHandled;
};
}

void GenericConCommandHandler(void)
{
	CCommand command;

	if (CConCommandSystem::GetInstance()->Execute(command, NULL, true) == 2)
		ALERT(at_console, "Unknown command: '%s'\n", command.Arg(0));
}

void log_commands_Callback(CCVar &variable, const CString &oldValue, float oldFloat)
{
	ALERT(at_console, "Command logging now %s\n", (int)variable.m_flValue ? "enabled" : "disabled");
}

CCVar log_commands("log_commands", CCVarArguments{ConCommandFlag::AdminOnly,
                                                  "If set, every command is logged", "0", {log_commands_Callback}, false});

void find_Callback(const CCommand &command)
{
	CBasePlayer *player = CConCommandSystem::GetInstance()->GetCurrentPlayer();

	if (command.ArgC() == 2)
		CConCommandSystem::GetInstance()->FindCommands(command.Arg(1), player);
	else
		g_engfuncs.pfnClientPrintf(player->edict(), print_console, "find usage: find <substring>\n");
}

CClientCommand g_find("find", find_Callback, ConCommandFlag::None, "Find commands using a given substring");

CConCommandSystem::CConCommandSystem() : m_iState(COMMAND_SYSTEM_STOPPED), m_pUnhandledCommandHandler(NULL)
{
}

CConCommandSystem::~CConCommandSystem()
{
}

CConCommandSystem *CConCommandSystem::GetInstance(void)
{
	static CConCommandSystem *pInstance;

	if (!pInstance)
		pInstance = new CConCommandSystem;

	return pInstance;
}

void CConCommandSystem::Initialize(void)
{
	if (m_iState != COMMAND_SYSTEM_STOPPED)
		return;

	m_iState = COMMAND_SYSTEM_STARTING;

	for (CBaseConCommand *command = CBaseConCommand::GetHead(); command; command = command->GetNext())
	{
		const char *name = command->GetName();

		if (!name || !*name)
		{
			ALERT(at_warning, "Console: Could not add command at address %p: null or empty name\n", command);
			continue;
		}

		CString validName(name);

		for (const char *character = " \t\n"; *character; character++)
			validName.Trim(*character);

		if (validName.IsEmpty() || validName != name)
		{
			// This diagnostic supplies only the name pointer for these two conversions
			ALERT(at_warning, "Console: Could not add command at address %p: invalid name '%s'\n", name);
			continue;
		}

		if (m_Commands.find(validName) != m_Commands.end())
		{
			ALERT(at_warning, "Console: Could not add command '%s': command already exists\n", validName.Data());
			continue;
		}

		if (!m_Commands.insert(std::make_pair(validName, command)).second)
		{
			ALERT(at_warning, "Console: Could not add command '%s': error adding command to list\n", validName.Data());
			continue;
		}

		if (command->GetKind() == ConCommandKind::ConCommand || command->GetKind() == ConCommandKind::CVar)
			g_engfuncs.pfnAddServerCommand((char *)command->GetName(), GenericConCommandHandler);
	}

	m_iState = COMMAND_SYSTEM_RUNNING;
}

void CConCommandSystem::Shutdown(void)
{
	m_iState = COMMAND_SYSTEM_STOPPING;
	m_Commands.clear();
	m_iState = COMMAND_SYSTEM_STOPPED;
}

void CConCommandSystem::SetUnhandledCommandHandler(IConCommandSystem *handler)
{
	m_pUnhandledCommandHandler = handler;
}

CBasePlayer *CConCommandSystem::GetCurrentPlayer(void) const
{
	return m_Players.empty() ? NULL : m_Players.top();
}

int CConCommandSystem::Execute(const CCommand &command, CBasePlayer *player, bool consoleCommand)
{
	if (command.ArgC() <= 0)
		return 2;

	const char *name = command.Arg(0);

	if (!name || !*name)
		return 2;

	std::map<CString, CBaseConCommand *, CStringCaseInsensitiveLess>::iterator it = m_Commands.find(name);

	return Execute(it != m_Commands.end() ? it->second : NULL, command, player, consoleCommand);
}

int CConCommandSystem::Execute(CBaseConCommand *target, const CCommand &command,
                               CBasePlayer *player, bool consoleCommand)
{
	if (command.ArgC() <= 0)
		return 1;

	if (!target)
		return m_pUnhandledCommandHandler ? m_pUnhandledCommandHandler->Execute(command, player, consoleCommand) : 2;

	int result = ValidateCommandAccess(command, player, consoleCommand,
	                                   target->GetName(), target->GetKind(), target->GetFlags());

	if (result)
		return result;

	m_Players.push(player);

	if ((int)log_commands.m_flValue)
	{
		if (player)
			ALERT(at_logged, "Player '%s' sent command '%s'\n", STRING(player->pev->netname), command.GetCommandString());
		else
			ALERT(at_logged, "Console sent command '%s'\n", command.GetCommandString());
	}

	CommandContext context = { &command, false };

	target->Visit(*this, &context);
	m_Players.pop();
	return !context.m_bHandled;
}

void CConCommandSystem::Visit(CConCommand &command, void *data)
{
	CCommand arguments;

	command.Dispatch(arguments);
	((CommandContext *)data)->m_bHandled = true;
}

void CConCommandSystem::Visit(CClientCommand &command, void *data)
{
	CommandContext *context = (CommandContext *)data;

	if (GetCurrentPlayer())
	{
		command.Dispatch(*context->m_pCommand);
		context->m_bHandled = true;
	}
	else
	{
		// This diagnostic has no argument for its string conversion
		ALERT(at_warning, "Client command '%s' executed with null player!\n");
		context->m_bHandled = false;
	}
}

void CConCommandSystem::Visit(CCVar &variable, void *data)
{
	CommandContext *context = (CommandContext *)data;
	const CCommand *command = context->m_pCommand;
	CBasePlayer *player = GetCurrentPlayer();
	edict_t *entity = player ? player->edict() : NULL;

	if (command->ArgC() == 1)
		ConsoleOutput(entity, "\"%s\" is \"%s\"\n", variable.GetName(), variable.m_Value.Data());
	else if (command->ArgC() == 2 && variable.m_Value != command->Arg(1))
		variable.SetString(command->Arg(1));

	context->m_bHandled = true;
}

void CConCommandSystem::CVarChanged(const CCVar &variable, const CString &oldValue, float oldFloat)
{
	if (m_iState == COMMAND_SYSTEM_RUNNING)
		ALERT(at_console, "\"%s\" changed to \"%s\"\n", variable.GetName(), variable.m_Value.Data());
}

void GetCVarValues(void *object, const char *&defaultValue, const char *&currentValue)
{
	CCVar *variable = (CCVar *)object;

	defaultValue = variable->m_pszDefaultValue;
	currentValue = variable->m_Value.Data();
}

void CConCommandSystem::FindCommands(const char *prefix, CBasePlayer *player) const
{
	if (!prefix || !*prefix || !player)
		return;

	edict_t *entity = player->edict();

	for (std::map<CString, CBaseConCommand *, CStringCaseInsensitiveLess>::const_iterator it = m_Commands.begin(); it != m_Commands.end(); it++)
	{
		OutputFoundCommand(entity, prefix, it->second, it->second->GetName(), it->second->GetHelpInfo(),
		                   it->second->GetKind(), GetCVarValues);
	}
}
