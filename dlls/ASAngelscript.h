#ifndef ASANGELSCRIPT_H
#define ASANGELSCRIPT_H

#include "CASServerScriptGlobal.h"
#include "CString.h"

class CASDocumentation;

class CAngelscript : public CASServerScriptGlobal<CAngelscript>
{
public:
	virtual ~CAngelscript();

	CString GetAngelscriptDescription(void) const;
	CString GetAngelscriptVersionString(void) const;
	unsigned int GetAngelscriptVersion(void) const;
	unsigned int GetMajorVersion(void) const;
	unsigned int GetMinorVersion(void) const;

private:
	friend class CASBaseScriptGlobal<IASServerScriptGlobal, CAngelscript>;

	CAngelscript();
};

void RegisterSCScriptAngelscript(CASDocumentation *documentation);

#endif
