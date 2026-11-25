#include "extdll.h"
#include "enginecallback.h"
#include "CASCVar.h"
#include "CASConCommandSystem.h"
#include "CASFunction.h"
#include "CASClassRegistration.h"
#include <stdlib.h>

CASCVar::CASCVar(const CString &name, const CString &defaultValue, const CString &helpInfo,
                 ConCommandFlags_t flags, asIScriptFunction *callback)
    : CASBaseConCommand(ConCommandKind::CVar, name, flags, helpInfo, NULL), m_DefaultValue(defaultValue)
{
	SetString(defaultValue);
	SetCallback(callback);
}

CASCVar::CASCVar(const CString &name, float defaultValue, const CString &helpInfo,
                 ConCommandFlags_t flags, asIScriptFunction *callback)
    : CASBaseConCommand(ConCommandKind::CVar, name, flags, helpInfo, NULL), m_DefaultValue(defaultValue)
{
	SetFloat(defaultValue);
	SetCallback(callback);
}

CASCVar::~CASCVar()
{
}

CString CASCVar::GetAdditionalInfo(void) const
{
	CString info("Value is \"");

	info += GetString();
	info += "\"\n";
	return info;
}

void CASCVar::Visit(CASConCommandSystem &system, void *context)
{
	system.Visit(*this, context);
}

const CString &CASCVar::GetDefaultValue(void) const
{
	return m_DefaultValue;
}

const CString &CASCVar::GetString(void) const
{
	return m_Value;
}

float CASCVar::GetFloat(void) const
{
	return m_flValue;
}

int CASCVar::GetInt(void) const
{
	return (int)m_flValue;
}

bool CASCVar::GetBool(void) const
{
	return m_flValue != 0;
}

bool CASCVar::SetStringInternal(const CString &value, bool notify)
{
	CString oldValue(GetString());
	float oldFloat = GetFloat();

	m_Value = value;
	m_flValue = (float)atof(value.Data());

	if (notify)
	{
		CASConCommandSystem *system = CASConCommandSystem::GetInstance();

		if (!system->m_bInCVarCallback && m_pCallback)
		{
			CASConCommandSystem::GetInstance()->m_bInCVarCallback = true;

			bool success = m_pCallback->Call(0, this, &oldValue, oldFloat);

			CASConCommandSystem::GetInstance()->m_bInCVarCallback = false;

			if (!success)
				return false;
		}
	}

	if (oldValue != m_Value)
		ALERT(at_console, "\"%s\" changed to \"%s\"\n", m_FullyQualifiedName.Data(), m_Value.Data());

	return true;
}

void CASCVar::SetString(const CString &value)
{
	SetStringInternal(value, true);
}

void CASCVar::SetFloat(float value)
{
	CString text(value);

	SetString(text);
}

void CASCVar::SetInt(int value)
{
	SetFloat((float)value);
}

void CASCVar::SetBool(bool value)
{
	CString text(value ? "1" : "0");

	SetString(text);
}

void CASCVar::RevertToDefault(void)
{
	SetStringInternal(m_DefaultValue, true);
}

CASCVar *CreateCVar(const CString &name, const CString &value, const CString &helpInfo, unsigned int flags, asIScriptFunction *callback)
{
	return ASNotifyGarbageCollector("CCVar", new CASCVar(name, value, helpInfo, flags, callback));
}

CASCVar *CreateCVar(const CString &name, float value, const CString &helpInfo, unsigned int flags, asIScriptFunction *callback)
{
	return ASNotifyGarbageCollector("CCVar", new CASCVar(name, value, helpInfo, flags, callback));
}

void RegisterSCScriptCVar(CASDocumentation *documentation)
{
	documentation->RegisterObjectType("CVar class", "CCVar", 0, asOBJ_REF | asOBJ_GC);
	RegisterSCScriptGCRefCountedBaseClass<CASCVar>(documentation, "CCVar");
	documentation->RegisterObjectMethod("Gets the name of this command.", "CCVar",
	                                    "const string& GetName() const", asMETHOD(CASBaseConCommand, GetName), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Gets the fully qualified name of this command. This is the name that the command is referred by when called.", "CCVar",
	                                    "const string& GetFullyQualifiedName() const", asMETHOD(CASBaseConCommand, GetFullyQualifiedName), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Gets the help info describing this command.", "CCVar",
	                                    "const string& GetHelpInfo() const", asMETHOD(CASBaseConCommand, GetHelpInfo), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Gets the type of this console command.", "CCVar",
	                                    "ConCommandKind::Type GetKind() const", asMETHOD(CASBaseConCommand, GetKind), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Gets the name of the module that owns (created) this command.", "CCVar",
	                                    "const string& GetOwningModuleName() const", asMETHOD(CASBaseConCommand, GetOwningModuleName), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Whether this command was added to the list of commands.\nMust be added to be usable from the console.", "CCVar",
	                                    "bool HasBeenAdded() const", asMETHOD(CASBaseConCommand, HasBeenAdded), asCALL_THISCALL);
	documentation->RegisterFuncDef("Callback for cvars.", "void CVarCallback(CCVar@ cvar, const string& in szOldValue, float flOldValue)");
	documentation->RegisterObjectBehaviour("Constructor", "CCVar", asBEHAVE_FACTORY,
	                                       "CCVar@ CCVar(const string& in szName, float flValue = 0, const string& in szHelpInfo = \"\", const ConCommandFlags_t flags = ConCommandFlag::None, CVarCallback@ pCallback = null)",
	                                       asFUNCTIONPR(CreateCVar, (const CString &, float, const CString &, unsigned int, asIScriptFunction *), CASCVar *), asCALL_CDECL);
	documentation->RegisterObjectBehaviour("Constructor", "CCVar", asBEHAVE_FACTORY,
	                                       "CCVar@ CCVar(const string& in szName, const string& in szValue, const string& in szHelpInfo = \"\", const ConCommandFlags_t flags = ConCommandFlag::None, CVarCallback@ pCallback = null)",
	                                       asFUNCTIONPR(CreateCVar, (const CString &, const CString &, const CString &, unsigned int, asIScriptFunction *), CASCVar *), asCALL_CDECL);
	documentation->RegisterObjectMethod("Gets the default value for this cvar.", "CCVar", "const string& GetDefaultValue() const", asMETHOD(CASCVar, GetDefaultValue), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Converts the value to a string.", "CCVar", "const string& GetString() const", asMETHOD(CASCVar, GetString), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Converts the value to a float.", "CCVar", "float GetFloat() const", asMETHOD(CASCVar, GetFloat), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Converts the value to an integer.", "CCVar", "int GetInt() const", asMETHOD(CASCVar, GetInt), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Converts the value to a boolean.", "CCVar", "bool GetBool() const", asMETHOD(CASCVar, GetBool), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Sets the given string as the value.", "CCVar", "void SetString(const string& in szValue)", asMETHOD(CASCVar, SetString), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Sets the given float as the value.", "CCVar", "void SetFloat(const float flValue)", asMETHOD(CASCVar, SetFloat), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Sets the given integer as the value.", "CCVar", "void SetInt(const int iValue)", asMETHOD(CASCVar, SetInt), asCALL_THISCALL);
	documentation->RegisterObjectMethod("Sets the given boolean as the value.", "CCVar", "void SetBool(const bool bValue)", asMETHOD(CASCVar, SetBool), asCALL_THISCALL);
}
