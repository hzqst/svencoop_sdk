#include "extdll.h"
#include "CASPluginManager.h"
#include "CASModule.h"
#include "CASModuleBuilder.h"
#include "CASScheduler.h"
#include "CKeyvalues.h"
#include "CKeyvaluesParser.h"
#include "FileSystem.h"
#include "util.h"
#include "CASModuleList.h"
#include "CASServerManager.h"
#include "CASDocumentation.h"
#include "CASModuleHookManager.h"
#include "CASConCommandSystem.h"
#include "CASScriptInfo.h"
#include "CASLog.h"
#include "AdminLevel.h"
#include "../external/angelscript/sdk/add_on/scriptarray/scriptarray.h"

extern bool g_bServerActive;
cvar_t plugin_list_file = { "plugin_list_file", "default_plugins.txt", FCVAR_PROTECTED };

CASPluginManager::PluginListFile::PluginListFile(const CString &name,
	std::shared_ptr<CKvBlockNode> plugins, bool mainList)
{
	m_Name = name;
	m_pPlugins = plugins;
	m_bMainList = mainList;
	m_bRemoved = false;
	m_bLoaded = false;
}

CASPluginManager::CASPluginManager()
	: m_pPlugins(new CASModuleList), m_bPluginsLoaded(false)
{
}

CASPluginManager::~CASPluginManager()
{
	RemoveAllPlugins(true);

	delete m_pPlugins;
	m_pPlugins = NULL;
}

CScriptArray *CASPluginManager::GetPluginListAsScriptArray(void) const
{
	asIScriptEngine *engine = CASServerManager::GetInstance()->m_pEngine;
	int typeId = engine->GetTypeIdByDecl("array<string>");
	CScriptArray *array = CScriptArray::Create(engine->GetTypeInfoById(typeId));

	array->Resize(m_pPlugins->Count());

	for (unsigned int index = 0; index < m_pPlugins->Count(); index++)
	{
		const char *name = m_pPlugins->Get(index)->GetModuleName();

		*(CString *) array->At(index) = name;
	}

	return array;
}

CASModule *CASPluginManager::GetPlugin(const char *name) const
{
	if (!name || !*name)
		return NULL;

	for (unsigned int index = 0; index < m_pPlugins->Count(); index++)
	{
		CASModule *module = m_pPlugins->Get(index);

		if (!stricmp(module->GetModuleName(), name))
			return module;
	}

	return NULL;
}

bool CASPluginManager::IsPluginInList(const char *name) const
{
	return m_pPlugins->Contains(name);
}

bool CASPluginManager::ArePluginsLoaded(void)
{
	return m_bPluginsLoaded;
}

void CASPluginManager::CallMapFunction(const char *declaration)
{
	for (unsigned int index = 0; index < m_pPlugins->Count(); index++)
		m_pPlugins->Get(index)->CallFunction(true, declaration, 0);
}

void CASPluginManager::Think(void)
{
	for (unsigned int index = 0; index < m_pPlugins->Count(); index++)
		m_pPlugins->Get(index)->Think();
}

CASPluginManager::PluginListFile *CASPluginManager::FindPluginList(const char *name)
{
	for (std::vector<PluginListFile>::iterator entry = m_PluginLists.begin();
		entry != m_PluginLists.end(); entry++)
	{
		if (!entry->m_Name.ICompare(name))
			return &*entry;
	}

	return NULL;
}

void RegisterSCScriptPluginManager(CASDocumentation *documentation)
{
	documentation->RegisterObjectType("Plugin manager for handling basic plugin operations",
		"CPluginManager", sizeof(CASPluginManager), asOBJ_REF | asOBJ_NOCOUNT);
	documentation->RegisterObjectMethod("Gets the list of plugins as strings.",
		"CPluginManager", "array<string>@ GetPluginList() const",
		asMETHOD(CASPluginManager, GetPluginListAsScriptArray), asCALL_THISCALL);
	documentation->RegisterGlobalProperty("Plugin manager global instance.",
		"CPluginManager g_PluginManager", CASServerManager::GetInstance()->m_pPluginManager);
}

static const ASLogLevel_t PLUGIN_OPERATION_LOG_LEVEL = (ASLogLevel_t) 1;
static const HookStopMode::Type PLUGIN_HOOK_CALL_ALL = (HookStopMode::Type) 2;
static const int PLUGIN_HOOK_CHECK_PLAYER_ACCESS = 1;

bool CASPluginManager::ValidatePlugin(CASModule *module) const
{
	return module &&
		module->GetScriptInfo()->GetAuthor().FindFirstNotOf(" \t") != CString::INVALID_INDEX &&
		module->GetScriptInfo()->GetContactInfo().FindFirstNotOf(" \t") != CString::INVALID_INDEX;
}

void CASPluginManager::RemovePluginData(CASModule *module)
{
	if (!module)
		return;

	CASConCommandSystem *commands = CASConCommandSystem::GetInstance();
	const char *name = module->GetModuleName();

	commands->RemoveModule(name);
	module->Invalidate();
}

bool CASPluginManager::RemovePluginInt(unsigned int index, bool force)
{
	CASModule *module = m_pPlugins->Get(index);

	if (!force && module->m_bHasCustomEntities)
	{
		module->m_bDelayedRemoval = true;

		CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL,
			"Delayed removal of plugin \"%s\" scheduled\n", module->GetModuleName());
		return false;
	}

	module->CallFunction(true, "void PluginExit()", 0);
	RemovePluginData(module);
	m_pPlugins->Remove(index);
	return true;
}

unsigned int CASPluginManager::RemovePlugins(bool force)
{
	unsigned int removed = 0;

	for (unsigned int index = 0; index < m_pPlugins->Count();)
	{
		if (RemovePluginInt(index, force))
			removed++;
		else
			index++;
	}

	return removed;
}

void CASPluginManager::RemoveAllPlugins(bool force)
{
	unsigned int removed = RemovePlugins(force);

	m_bPluginsLoaded = false;
	CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL, "Removed %u plugin(s)\n", removed);
}

void CASPluginManager::ReloadAllPlugins(void)
{
	CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL, "Reloading plugins\n");
	RemoveAllPlugins(false);

	for (unsigned int index = 0; index < m_pPlugins->Count(); index++)
	{
		CASModule *module = m_pPlugins->Get(index);

		if (!module->m_bDelayedRemoval)
			continue;

		module->m_bDelayedReload = true;

		CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL,
			"Delayed reload of plugin \"%s\" scheduled\n", module->GetModuleName());
	}

	LoadAllPlugins(false, false);
}

void CASPluginManager::CallFunctionOnAllModules(CBasePlayer *player, const CASHook &hook,
	int argumentCount, va_list arguments, HookReturnCode_t &result)
{
	for (unsigned int index = 0; index < m_pPlugins->Count(); index++)
	{
		CASModule *module = m_pPlugins->Get(index);
		CASModuleHookManager *hooks = module->m_pHookManager;

		if (!hooks->HookExists(hook.m_uiCode))
			continue;

		if (player && ((int) hook.m_Flags & PLUGIN_HOOK_CHECK_PLAYER_ACCESS) &&
			AdminLevel(player) < module->GetScriptInfo()->GetMinimumAdminLevel())
		{
			continue;
		}

		if (hooks->CallHook(hook, argumentCount, arguments) != HOOK_HANDLED)
			continue;

		result = HOOK_HANDLED;

		if (hook.m_StopMode != PLUGIN_HOOK_CALL_ALL)
			return;
	}
}

void CASPluginManager::PluginCompStats::Print(const char *name)
{
	if (name && *name)
		CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL, "%u plugin(s) in '%s'\n", m_uiTotal, name);
	else
		CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL, "%u plugin(s) total\n", m_uiTotal);

	if (!m_uiTotal)
	{
		CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL, "No plugins to compile\n");
		return;
	}

	CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL, "%u plugin(s) successfully compiled\n", m_uiSucceeded);

	if (m_uiSkipped)
		CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL, "%u plugin(s) skipped\n", m_uiSkipped);

	if (m_uiFailed)
		CASLog::GetInstance()->Log(AS_LOG_DEFAULT, "%u plugin(s) failed compilation\n", m_uiFailed);
}

void CASPluginManager::LoadPluginLists(void)
{
	CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL, "Loading plugin lists\n");

	const char *fileName = plugin_list_file.string;
	PluginListFile file
	{
		CString(fileName),
		LoadPluginListFile(fileName, "GAME"),
		true
	};

	m_PluginLists.push_back(file);
	ScanExtraPluginLists();
}

CASPluginManager::PluginCompStats CASPluginManager::LoadPluginsFromList(PluginListFile &file, bool force)
{
	PluginCompStats stats = {};

	if (!file.m_pPlugins)
		return stats;

	file.m_bLoaded = true;
	stats.m_uiTotal = file.m_pPlugins->GetChildren().size();
	CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL,
		"Plugin list '%s' - %u plugin(s) listed\n", file.m_Name.Data(), stats.m_uiTotal);

	const CKvBlockNode::Children &children = file.m_pPlugins->GetChildren();

	for (CKvBlockNode::Children::const_iterator entry = children.begin(); entry != children.end(); entry++)
	{
		std::shared_ptr<CKeyvalueNode> child = *entry;

		if (child->GetType() != KEYVALUE_NODE_BLOCK)
			continue;

		switch (AddPlugin(*(CKvBlockNode *) child.get(), force))
		{
		case 0:
			stats.m_uiFailed++;
			break;
		case 1:
			stats.m_uiSucceeded++;
			break;
		case 2:
			stats.m_uiSkipped++;
			break;
		}
	}

	return stats;
}

CASPluginManager::PluginCompStats CASPluginManager::RemovePluginsFromList(PluginListFile &file, bool force)
{
	PluginCompStats stats = {};

	if (!file.m_pPlugins)
		return stats;

	const CKvBlockNode::Children &children = file.m_pPlugins->GetChildren();

	stats.m_uiTotal = children.size();

	for (CKvBlockNode::Children::const_iterator entry = children.begin(); entry != children.end(); entry++)
	{
		std::shared_ptr<CKeyvalueNode> child = *entry;

		if (child->GetType() != KEYVALUE_NODE_BLOCK)
			continue;

		CString name = ((CKvBlockNode *) child.get())->FindFirstKeyvalue("name");

		name.Trim();

		if (name.IsEmpty())
			continue;

		int index = m_pPlugins->IndexOf(name.Data());

		if (index == -1)
			continue;

		if (RemovePluginInt(index, force))
			stats.m_uiSucceeded++;
		else
			stats.m_uiSkipped++;
	}

	return stats;
}

void CASPluginManager::LoadExtraPlugins(bool force)
{
	for (std::vector<PluginListFile>::iterator entry = m_PluginLists.begin(); entry != m_PluginLists.end();)
	{
		if (entry->m_bMainList || !entry->m_bRemoved)
		{
			entry++;
			continue;
		}

		CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL,
			"Removing extra plugin list '%s'\n", entry->m_Name.Data());
		RemovePluginsFromList(*entry, true);
		entry = m_PluginLists.erase(entry);
	}

	PluginCompStats total = {};

	for (std::vector<PluginListFile>::iterator entry = m_PluginLists.begin(); entry != m_PluginLists.end(); entry++)
	{
		if (entry->m_bMainList || entry->m_bLoaded)
			continue;

		PluginCompStats stats = LoadPluginsFromList(*entry, force);

		total.m_uiTotal += stats.m_uiTotal;
		total.m_uiSucceeded += stats.m_uiSucceeded;
		total.m_uiFailed += stats.m_uiFailed;
		total.m_uiSkipped += stats.m_uiSkipped;
	}

	if (total.m_uiTotal)
		total.Print("extra plugins");
}

void CASPluginManager::LoadAllPlugins(bool force, bool scanExtra)
{
	if (m_bPluginsLoaded && scanExtra)
	{
		ScanExtraPluginLists();
		LoadExtraPlugins(force);
		return;
	}

	m_bPluginsLoaded = true;
	m_PluginLists.clear();
	LoadPluginLists();

	PluginCompStats total = {};

	for (std::vector<PluginListFile>::iterator entry = m_PluginLists.begin(); entry != m_PluginLists.end(); entry++)
	{
		PluginCompStats stats = LoadPluginsFromList(*entry, force);

		total.m_uiTotal += stats.m_uiTotal;
		total.m_uiSucceeded += stats.m_uiSucceeded;
		total.m_uiFailed += stats.m_uiFailed;
		total.m_uiSkipped += stats.m_uiSkipped;
	}

	total.Print(NULL);
}

std::shared_ptr<CKvBlockNode> CASPluginManager::LoadPluginListFile(const char *fileName, const char *pathId) const
{
	CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL, "Loading plugin list '%s'\n", fileName);

	CKeyvaluesParser parser(fileName, pathId, CKeyvaluesParserSettings(false, true, true, true));

	if (!parser.m_Lexer.HasInputData())
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
			"Cannot load plugins: failed to read file '%s'\n", fileName);
		return std::shared_ptr<CKvBlockNode>();
	}

	CBaseKeyvaluesParser::ParseResult result = parser.Parse();

	if (result != CBaseKeyvaluesParser::PARSE_SUCCESS)
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
			"Cannot load plugins: Error '%s' encountered while parsing list file\n",
			CBaseKeyvaluesParser::ParseResultToString(result));
		return std::shared_ptr<CKvBlockNode>();
	}

	CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL, "Loading plugins from file '%s'\n", fileName);

	std::shared_ptr<CKvBlockNode> plugins;

	{
		std::shared_ptr<CKeyvalues> keyvalues = parser.GetKeyvalues();
		std::shared_ptr<CKvBlockNode> root = keyvalues->GetRoot();

		plugins = std::static_pointer_cast<CKvBlockNode>(root->FindFirstChild("plugins", KEYVALUE_NODE_BLOCK));
	}

	if (!plugins)
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
			"Cannot load plugins: Missing plugin list contents (block '%s')\n", "plugins");
		return std::shared_ptr<CKvBlockNode>();
	}

	plugins->RemoveAllNotNamed("plugin");
	return plugins;
}

std::shared_ptr<CKvBlockNode> CASPluginManager::FindPluginInList(const char *name) const
{
	for (std::vector<PluginListFile>::const_iterator file = m_PluginLists.begin(); file != m_PluginLists.end(); file++)
	{
		if (!file->m_pPlugins)
			continue;

		const CKvBlockNode::Children &children = file->m_pPlugins->GetChildren();

		for (CKvBlockNode::Children::const_iterator entry = children.begin(); entry != children.end(); entry++)
		{
			std::shared_ptr<CKeyvalueNode> child = *entry;

			if (child->GetType() != KEYVALUE_NODE_BLOCK || child->GetKey() != "plugin")
				continue;

			CString pluginName = ((CKvBlockNode *) child.get())->FindFirstKeyvalue("name");

			pluginName.Trim();

			if (!pluginName.ICompare(name))
				return std::static_pointer_cast<CKvBlockNode>(child);
		}
	}

	return std::shared_ptr<CKvBlockNode>();
}

void CASPluginManager::LoadPlugin(const char *name)
{
	if (!name || !*name)
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT, "CASPluginManager::LoadPlugin: Plugin name is null or empty!\n");
		return;
	}

	std::shared_ptr<CKvBlockNode> plugin = FindPluginInList(name);

	if (plugin)
		AddPlugin(*plugin, false);
	else
		CASLog::GetInstance()->Warning(PLUGIN_OPERATION_LOG_LEVEL, "No such plugin \"%s\", cannot load\n", name);
}

bool CASPluginManager::RemovePlugin(const char *name)
{
	if (!name || !*name)
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT, "CASPluginManager::RemovePlugin: Plugin name is null or empty!\n");
		return false;
	}

	int index = m_pPlugins->IndexOf(name);

	if (index != -1)
	{
		RemovePluginInt(index, false);
		return true;
	}

	CASLog::GetInstance()->Warning(PLUGIN_OPERATION_LOG_LEVEL, "No such plugin \"%s\", cannot remove.\n", name);
	return false;
}

void CASPluginManager::ReloadPlugin(const char *name)
{
	if (!name || !*name)
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT, "CASPluginManager::ReloadPlugin: Plugin name is null or empty!\n");
		return;
	}

	if (!RemovePlugin(name))
	{
		CASLog::GetInstance()->Warning(PLUGIN_OPERATION_LOG_LEVEL,
			"No such plugin \"%s\", cannot reload\nReloading plugin list file to locate plugin data\n", name);
		LoadPluginLists();
	}

	CASModule *module = GetPlugin(name);

	if (module && module->m_bDelayedRemoval)
	{
		module->m_bDelayedReload = true;
		CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL,
			"Delayed reload of plugin \"%s\" scheduled\n", module->GetModuleName());
	}
	else
	{
		LoadPlugin(name);
	}
}

void CASPluginManager::ScanExtraPluginLists(void)
{
	for (std::vector<PluginListFile>::iterator entry = m_PluginLists.begin(); entry != m_PluginLists.end(); entry++)
	{
		if (!entry->m_bMainList)
			entry->m_bRemoved = true;
	}

	IFileSystem_SvEngine *fileSystem = static_cast<IFileSystem_SvEngine *>(g_pFileSystem);
	int length = fileSystem->GetSearchPath("GAME", false, NULL, 0);
	char *paths = (char *) malloc(length + 1);

	fileSystem->GetSearchPath("GAME", false, paths, length);

	for (char *path = strtok(paths, ";"); path; path = strtok(NULL, ";"))
	{
		char relativePath[MAX_PATH];

		if (!fileSystem->FullPathToRelativePathEx(path, "BASE", relativePath, sizeof(relativePath)))
			continue;

		strncat(relativePath, "extra_plugins.txt", sizeof(relativePath) - strlen(relativePath));
		relativePath[sizeof(relativePath) - 1] = '\0';

		if (!strstr(path, "_event_") || !fileSystem->FileExistsEx(relativePath, "BASE"))
			continue;

		PluginListFile *existing = FindPluginList(relativePath);

		if (existing)
		{
			existing->m_bRemoved = false;
			continue;
		}

		PluginListFile file
		{
			CString(relativePath),
			LoadPluginListFile(relativePath, "BASE"),
			false
		};

		m_PluginLists.push_back(file);
	}

	free(paths);
}

bool CASPluginManager::CompilePlugin(CASModuleBuilder &builder, const CKvBlockNode &data,
	CASModule **module, bool force) const
{
	if (!module)
		return false;

	if (!g_bServerActive)
		force = false;

	CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL,
		"Beginning plugin \"%s\" compilation\n", builder.GetModuleName());

	bool succeeded = false;

	if (builder.Build(module))
	{
		CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL, "Plugin script compilation succeeded\n");

		CASModule *plugin = *module;
		CString nameSpace = data.FindFirstKeyvalue("concommandns");

		nameSpace.Trim();

		CASConCommandSystem *commands = CASConCommandSystem::GetInstance();

		if (commands->RegisterModule(plugin->GetModuleName(), nameSpace.Data()))
		{
			{
				CString level = data.FindFirstKeyvalue("adminlevel");

				level.Trim();

				AdminLevel_t minimumLevel = StringToAdminLevel(level.Data());

				plugin->GetScriptInfo()->SetMinimumAdminLevel(minimumLevel < ADMIN_NO ? ADMIN_NO : minimumLevel);
			}

			if (plugin->CallFunction(false, "void PluginInit()", 0))
			{
				if (ValidatePlugin(plugin))
				{
					if (force)
						plugin->CallFunction(true, "void MapInit()", 0);

					succeeded = true;
				}
				else
				{
					CASLog::GetInstance()->Error(AS_LOG_DEFAULT, "Plugin failed validation\n");
				}
			}
			else
			{
				CASLog::GetInstance()->Error(AS_LOG_DEFAULT, "Plugins must implement a PluginInit function!\n");
			}
		}
		else
		{
			CASLog::GetInstance()->Error(AS_LOG_DEFAULT, "Failed to register module with the console command system!\n");
		}
	}
	else
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT, "Plugin script compilation failed\n");
	}

	if (!succeeded && *module)
	{
		CASConCommandSystem *commands = CASConCommandSystem::GetInstance();

		commands->RemoveModule((*module)->GetModuleName());
	}

	return succeeded;
}

bool CASPluginManager::IsPluginAllowedOnMap(CASModule *module, CString mapName) const
{
	const ASLogLevel_t detailLevel = (ASLogLevel_t) 2;

	if (!module->m_IncludedMaps.empty())
	{
		if (!module->m_ExcludedMaps.empty())
		{
			CASLog::GetInstance()->Log(detailLevel,
				"Plugin \"%s\" has both map include/exclude lists defined (ambiguous): Denied on \"%s\".\n",
				module->GetModuleName(), mapName.Data());
			return false;
		}

		for (std::vector<CString>::const_iterator entry = module->m_IncludedMaps.begin();
			entry != module->m_IncludedMaps.end(); entry++)
		{
			CString name = *entry;

			if (name == mapName)
			{
				CASLog::GetInstance()->Log(detailLevel,
					"Plugin \"%s\" found in map include list: Allowed on \"%s\".\n",
					module->GetModuleName(), mapName.Data());
				return true;
			}
		}

		return false;
	}

	if (module->m_ExcludedMaps.empty())
	{
		CASLog::GetInstance()->Log(detailLevel,
			"Plugin \"%s\" has no map include/exclude lists defined: Allowed on \"%s\".\n",
			module->GetModuleName(), mapName.Data());
		return true;
	}

	for (std::vector<CString>::const_iterator entry = module->m_ExcludedMaps.begin();
		entry != module->m_ExcludedMaps.end(); entry++)
	{
		CString name = *entry;

		if (name == mapName)
		{
			CASLog::GetInstance()->Log(detailLevel,
				"Plugin \"%s\" found in map exclude list: Denied on \"%s\".\n",
				module->GetModuleName(), mapName.Data());
			return false;
		}
	}

	return true;
}

void CASPluginManager::OnMapEnd(const char *nextMap)
{
	for (unsigned int index = 0; index < m_pPlugins->Count();)
	{
		CASModule *module = m_pPlugins->Get(index);
		CString name = module->GetModuleName();

		if (nextMap && strlen(nextMap) && !IsPluginAllowedOnMap(module, CString(nextMap)))
		{
			CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL,
				"Removing plugin \"%s\", server does not allow it for the next map \"%s\".\n", name.Data(), nextMap);
			module->m_bDisallowedForNextMap = true;
		}

		bool reload = module->m_bDelayedReload;
		bool removed = false;

		if (module->m_bDelayedRemoval || module->m_bDisallowedForNextMap)
		{
			CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL, "Removing plugin \"%s\" (delayed)\n", name.Data());

			module = m_pPlugins->Get(index);
			module->CallFunction(true, "void PluginExit()", 0);
			RemovePluginData(module);
			m_pPlugins->Remove(index);
			removed = true;
		}

		if (reload)
		{
			CASLog::GetInstance()->Log(PLUGIN_OPERATION_LOG_LEVEL, "Loading plugin \"%s\" (delayed)\n", name.Data());
			LoadPlugin(name.Data());
			removed = false;
		}

		if (!removed)
			index++;
	}

	for (unsigned int index = 0; index < m_pPlugins->Count(); index++)
		m_pPlugins->Get(index)->m_pScheduler->AdjustTime(gpGlobals->time - 1.0f);
}

int CASPluginManager::AddPlugin(const CKvBlockNode &data, bool force)
{
	CString name = data.FindFirstKeyvalue("name");

	name.Trim();

	if (name.IsEmpty())
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT, "Can't add plugin, name is empty!\n");
		return 0;
	}

	if (!stricmp("MapModule", name.Data()))
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT, "Can't add plugin, name can't be \"%s\"!\n", "MapModule");
		return 0;
	}

	if (m_pPlugins->Contains(name.Data()))
	{
		CASModule *module = GetPlugin(name.Data());

		if (module && module->m_bDelayedRemoval)
		{
			CASLog::GetInstance()->Warning(PLUGIN_OPERATION_LOG_LEVEL,
				"Skipping plugin \"%s\", already exists and requires delayed removal.\n", name.Data());
		}
		else
		{
			CASLog::GetInstance()->Warning(PLUGIN_OPERATION_LOG_LEVEL,
				"Skipping plugin \"%s\", already registered.\n", name.Data());
		}

		return 2;
	}

	CString included = data.FindFirstKeyvalue("maps_included");

	included.Trim();

	CString excluded = data.FindFirstKeyvalue("maps_excluded");

	excluded.Trim();

	std::vector<CString> includedMaps;
	std::vector<CString> excludedMaps;
	CString map;

	if (!included.IsEmpty())
	{
		if (!excluded.IsEmpty())
		{
			CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
				"Can't add plugin \"%s\", conflicting map rules! (Must only decide \"%s\" or \"%s\", not both.)\n",
				name.Data(), "maps_included", "maps_excluded");
			return 0;
		}

		while ((map = included.Tokenize(';')) != CString(CString::NO_MORE_TOKENS))
			includedMaps.push_back(map);
	}
	else if (!excluded.IsEmpty())
	{
		while ((map = excluded.Tokenize(';')) != CString(CString::NO_MORE_TOKENS))
			excludedMaps.push_back(map);
	}

	map = "";

	CString script = data.FindFirstKeyvalue("script");

	script.Trim();

	if (script.IsEmpty())
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT, "Can't add plugin \"%s\", no script provided!\n", name.Data());
		return 0;
	}

	CASModuleBuilder builder(ModuleType::PLUGIN, name.Data());
	int result = builder.AddScriptFromFile(script.Data());

	if (result < 0)
	{
		if (result == -2)
		{
			CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
				"Plugin \"%s\": Script '%s' does not exist!\n", name.Data(), script.Data());
		}
		else if (result == -1)
		{
			CASLog::GetInstance()->Error(AS_LOG_DEFAULT, "Failed to add plugin \"%s\"!\n", name.Data());
		}
		else
		{
			CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
				"Failed to add plugin \"%s\" with unknown error '%d'!\n", name.Data(), result);
		}

		return 0;
	}

	CASModule *module = NULL;

	if (!CompilePlugin(builder, data, &module, force))
		return 0;

	// The transfers assign from separate owned copies
	{
		std::vector<CString> maps(includedMaps);

		module->m_IncludedMaps = maps;
	}

	{
		std::vector<CString> maps(excludedMaps);

		module->m_ExcludedMaps = maps;
	}

	if (!IsPluginAllowedOnMap(module, CString(STRING(gpGlobals->mapname))))
	{
		CASLog::GetInstance()->Warning(PLUGIN_OPERATION_LOG_LEVEL,
			"Skipping plugin \"%s\", server does not allow it on this map.\n", name.Data());
		RemovePluginData(module);
		module->Release();
		return 2;
	}

	if (m_pPlugins->Add(module) == -1)
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
			"Failed to add plugin \"%s\" to the plugin list!\n", name.Data());
		RemovePluginData(module);
		module->Release();
		return 0;
	}

	module->Release();
	return 1;
}

void OutputModuleScriptInfo(edict_t *player, CASModule *module)
{
	CString information;
	CASScriptInfo *info = module->GetScriptInfo();

	if (info)
	{
		information.Format("Name: %s\nAuthor: %s\nContact info: %s\n", module->GetModuleName(),
			info->GetAuthor().IsEmpty() ? "<No author info available>" : info->GetAuthor().Data(),
			info->GetContactInfo().IsEmpty() ? "<no contact info available>" : info->GetContactInfo().Data());
	}
	else
	{
		information = "No module information available";
	}

	if (player)
		g_engfuncs.pfnClientPrintf(player, print_console, information.Data());
	else
		printf(information.Data());
}

void CASPluginManager::ListPlugins(edict_t *player) const
{
	CString text;

	text = "Active plugins:\n";

	if (player)
		g_engfuncs.pfnClientPrintf(player, print_console, text.Data());
	else
		printf(text.Data());

	unsigned int count = m_pPlugins->Count();

	if (!count)
	{
		text = "None\n";

		if (player)
			g_engfuncs.pfnClientPrintf(player, print_console, text.Data());
		else
			printf(text.Data());

		return;
	}

	for (unsigned int index = 0; index < count; index++)
	{
		CASModule *module = m_pPlugins->Get(index);
		OutputModuleScriptInfo(player, module);
	}
}
