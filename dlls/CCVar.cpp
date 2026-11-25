#include "extdll.h"
#include "enginecallback.h"
#include "CCVar.h"
#include "CConCommandSystem.h"
#include <stdlib.h>

CCVar::CCVar(const char *name, const CCVarArguments &arguments)
    : CBaseConCommand(ConCommandKind::CVar, name, arguments.m_Flags, arguments.m_pszHelpInfo),
      m_pszDefaultValue(arguments.m_pszDefaultValue ? arguments.m_pszDefaultValue : ""),
      m_pFunctionCallback(NULL), m_bInterfaceCallback(false)
{
	SetString(m_pszDefaultValue);
	m_pFunctionCallback = arguments.m_pFunctionCallback;
	m_bInterfaceCallback = arguments.m_bInterfaceCallback;
}

CCVar::~CCVar()
{
}

void CCVar::Visit(CConCommandSystem &system, void *context)
{
	system.Visit(*this, context);
}

void CCVar::SetInterfaceCallback(ICVarCallback *callback)
{
	m_pInterfaceCallback = callback;
	m_bInterfaceCallback = true;
}

void CCVar::Dispatch(const CString &oldValue, float oldFloat)
{
	if (!m_pFunctionCallback)
		return;

	if (m_bInterfaceCallback)
		m_pInterfaceCallback->CVarCallback(*this, oldValue, oldFloat);
	else
		m_pFunctionCallback(*this, oldValue, oldFloat);

	if (oldValue != m_Value)
		ALERT(at_console, "\"%s\" changed to \"%s\"\n", GetName(), m_Value.Data());
}

void CCVar::SetStringInternal(const char *value, bool notify)
{
	if (!value)
		return;

	CString oldValue(m_Value);
	float oldFloat = m_flValue;

	m_Value = value;
	m_flValue = (float)atof(value);

	if (notify)
		Dispatch(oldValue, oldFloat);

	CConCommandSystem::GetInstance()->CVarChanged(*this, oldValue, oldFloat);
}

void CCVar::SetString(const char *value)
{
	SetStringInternal(value, true);
}

void CCVar::SetFloat(float value)
{
	CString text(value);

	SetString(text.Data());
}

void CCVar::SetInt(int value)
{
	SetFloat((float)value);
}

void CCVar::SetBool(bool value)
{
	SetFloat((float)value);
}

void CCVar::RevertToDefault(void)
{
	SetStringInternal(m_pszDefaultValue, true);
}
