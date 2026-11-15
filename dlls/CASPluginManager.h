#ifndef CASPLUGINMANAGER_H
#define CASPLUGINMANAGER_H

#include "CString.h"
#include "CASHook.h"
#include <memory>
#include <vector>

class CASModule;
class CASModuleList;
class CASModuleBuilder;
class CASDocumentation;
class CKvBlockNode;
class CScriptArray;
typedef struct edict_s edict_t;

class CASPluginManager
{
public:
	CASPluginManager();
	~CASPluginManager();

	CScriptArray *GetPluginListAsScriptArray(void) const;
	CASModule *GetPlugin(const char *name) const;
	bool IsPluginInList(const char *name) const;
	void ListPlugins(edict_t *player) const;
	void CallMapFunction(const char *declaration);
	void Think(void);
	void CallFunctionOnAllModules(CBasePlayer *player, const CASHook &hook,
		int argumentCount, va_list arguments, HookReturnCode_t &result);

	void LoadPluginLists(void);
	void LoadAllPlugins(bool force, bool scanExtra);
	void RemoveAllPlugins(bool force);
	void ReloadAllPlugins(void);
	void LoadPlugin(const char *name);
	void ReloadPlugin(const char *name);
	bool RemovePlugin(const char *name);
	void OnMapEnd(const char *nextMap);

private:
	struct PluginCompStats
	{
		void Print(const char *name);

		unsigned int m_uiTotal;
		unsigned int m_uiSucceeded;
		unsigned int m_uiFailed;
		unsigned int m_uiSkipped;
	};

	struct PluginListFile
	{
		PluginListFile(const CString &name, std::shared_ptr<CKvBlockNode> plugins, bool mainList);

		CString                          m_Name;
		std::shared_ptr<CKvBlockNode>    m_pPlugins;
		bool                             m_bMainList;
		bool                             m_bRemoved;
		bool                             m_bLoaded;
	};

	bool IsPluginAllowedOnMap(CASModule *module, CString mapName) const;
	PluginListFile *FindPluginList(const char *name);
	bool ArePluginsLoaded(void);
	void RemovePluginData(CASModule *module);
	bool RemovePluginInt(unsigned int index, bool force);
	unsigned int RemovePlugins(bool force);
	bool ValidatePlugin(CASModule *module) const;
	bool CompilePlugin(CASModuleBuilder &builder, const CKvBlockNode &data, CASModule **module, bool force) const;
	int AddPlugin(const CKvBlockNode &data, bool force);
	PluginCompStats LoadPluginsFromList(PluginListFile &file, bool force);
	PluginCompStats RemovePluginsFromList(PluginListFile &file, bool force);
	void LoadExtraPlugins(bool force);
	std::shared_ptr<CKvBlockNode> LoadPluginListFile(const char *fileName, const char *pathId) const;
	std::shared_ptr<CKvBlockNode> FindPluginInList(const char *name) const;
	void ScanExtraPluginLists(void);

	CASModuleList                  *m_pPlugins;
	bool                            m_bPluginsLoaded;
	std::vector<PluginListFile>     m_PluginLists;
};

struct cvar_s;
extern cvar_s plugin_list_file;

void OutputModuleScriptInfo( edict_t *player, CASModule *module );
void RegisterSCScriptPluginManager(CASDocumentation *documentation);

#endif
