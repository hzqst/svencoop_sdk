#include "CASBaseConCommand.h"
#include "CASConCommandSystem.h"
#include "CASServerManager.h"
#include "CASModule.h"
#include "CASFunction.h"
#include "CASLog.h"

CASBaseConCommand::CASBaseConCommand(ConCommandKind::Type kind, const CString &name,
                                     ConCommandFlags_t flags, const CString &helpInfo, asIScriptFunction *callback)
    : m_Kind(kind), m_Flags(flags), m_Name(name), m_HelpInfo(helpInfo), m_pCallback(NULL), m_bAdded(false)
{
	CASModule *module = CASServerManager::GetInstance()->m_pActiveModule;

	if (module)
	{
		m_OwningModuleName = module->GetModuleName();
		SetCallback(callback);
		m_bAdded = CASConCommandSystem::GetInstance()->AddCommand(this);
	}
	else
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT, "Console command created without an active module to own it!\n");
}

CASBaseConCommand::~CASBaseConCommand()
{
	CASConCommandSystem::GetInstance()->RemoveCommand(this);

	if (m_pCallback)
		m_pCallback->Release();

	m_pCallback = NULL;
}

CString CASBaseConCommand::GetAdditionalInfo(void) const
{
	return CString("");
}

void CASBaseConCommand::Release(void) const
{
	m_bGCFlag = false;

	if (InternalRelease())
		delete this;
}

void CASBaseConCommand::EnumReferences(asIScriptEngine *engine)
{
	if (m_pCallback)
		engine->GCEnumCallback(m_pCallback->GetFunction());
}

void CASBaseConCommand::ReleaseReferences(asIScriptEngine *engine)
{
	if (m_pCallback)
		m_pCallback->Release();

	m_pCallback = NULL;
}

void CASBaseConCommand::SetCallback(CASFunction *callback)
{
	if (m_pCallback)
		m_pCallback->Release();

	m_pCallback = callback;

	if (callback)
		callback->AddRef();
}

void CASBaseConCommand::SetCallback(asIScriptFunction *callback)
{
	if (!callback)
	{
		SetCallback((CASFunction *)NULL);
		return;
	}

	CASFunction *function = CASFunction::Create(callback, CASServerManager::GetInstance()->m_pActiveModule, true);

	SetCallback(function);
	function->Release();
}

ConCommandKind::Type CASBaseConCommand::GetKind(void) const
{
	return m_Kind;
}

const CString &CASBaseConCommand::GetName(void) const
{
	return m_Name;
}

const CString &CASBaseConCommand::GetFullyQualifiedName(void) const
{
	return m_FullyQualifiedName;
}

const CString &CASBaseConCommand::GetHelpInfo(void) const
{
	return m_HelpInfo;
}

const CString &CASBaseConCommand::GetOwningModuleName(void) const
{
	return m_OwningModuleName;
}

bool CASBaseConCommand::HasBeenAdded(void) const
{
	return m_bAdded;
}
