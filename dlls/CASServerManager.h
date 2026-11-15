#ifndef CASSERVERMANAGER_H
#define CASSERVERMANAGER_H

#include "CASBaseManager.h"

class CASPersistence;
class CASPluginManager;

class CASServerManager : public CASBaseManager
{
public:
	CASServerManager();
	~CASServerManager();
	static CASServerManager *GetInstance(void);

	virtual int Startup(void);
	virtual void Shutdown(void);
	virtual void OnMapBegin(void);
	virtual void OnMapEnd(const char *nextMap);
	virtual void Think(void);
	virtual void SetupFileSystem(CASVirtualFileSystem &fileSystem);
	virtual void PreMapInit(void);
	virtual bool OnMapInit(void);
	virtual void PostMapInit(bool success);
	virtual void PreMapActivate(void);
	virtual bool OnMapActivate(void);
	virtual void PostMapActivate(bool success);
	virtual bool OnMapStart(void);
	virtual bool CallFunctionOnAllModules(CBasePlayer *player, unsigned char moduleTypes,
		const CASHook &hook, int argumentCount, va_list arguments);
	virtual bool CallFunctionOnAllModules(CBasePlayer *player, unsigned char moduleTypes,
		const CASHook &hook, int argumentCount, ...);

	void WorldCreated(void);

private:
	friend void RegisterSCScriptPluginManager(CASDocumentation *documentation);
	friend void RegisterSCScriptPersistence(CASDocumentation *documentation);
	friend CASPersistence *Persistence_GetActivePersistence( void );
	friend class CASConCommandSystem;
	friend void as_listplugins_Callback( const CCommand &command );
	friend void as_plugins_Callback( const CCommand &command );
	friend void as_reloadplugin_Callback( const CCommand &command );
	friend void as_reloadplugins_Callback( const CCommand &command );
	friend void as_removeplugin_Callback( const CCommand &command );
	friend void as_removeplugins_Callback( const CCommand &command );

	CASPersistence		*m_pPersistence;
	CASPluginManager	*m_pPluginManager;
};

#endif
