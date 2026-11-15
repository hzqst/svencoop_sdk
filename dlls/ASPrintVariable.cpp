#include "extdll.h"
#include "ASPrintVariable.h"
#include "CASServerManager.h"
#include "CString.h"
#include "FileSystem.h"
#include <angelscript.h>

void ASPrintVariable(void *file, int typeId, void *address, const char *name)
{
	if (!file)
		return;

	asIScriptEngine *engine = CASServerManager::GetInstance()->m_pEngine;

	if (!engine)
		return;

	asITypeInfo *type = engine->GetTypeInfoById(typeId);

	g_pFileSystem->FPrintf(file, "-");

	if (type)
	{
		if (*type->GetNamespace())
			g_pFileSystem->FPrintf(file, "%s::%s ", type->GetNamespace(), type->GetName());
		else
			g_pFileSystem->FPrintf(file, "%s ", type->GetName());

		if (typeId == engine->GetTypeIdByDecl("string"))
		{
			if (address)
				g_pFileSystem->FPrintf(file, "%s = '%s'\n", name, ((CString *) address)->Data());
			else
				g_pFileSystem->FPrintf(file, "%s = <null>\n", name);
		}
		else if (typeId & (asTYPEID_APPOBJECT | asTYPEID_SCRIPTOBJECT))
			g_pFileSystem->FPrintf(file, address ? "%s = {...}\n" : "%s = <null>\n", name);

		return;
	}

	switch (typeId)
	{
		case asTYPEID_VOID:
			g_pFileSystem->FPrintf(file, "void %s = %u\n", name, (unsigned int) (size_t) address);
			break;
		case asTYPEID_BOOL:
			g_pFileSystem->FPrintf(file, "bool %s = %s\n", name, *(bool *) address ? "true" : "false");
			break;
		case asTYPEID_INT8:
			g_pFileSystem->FPrintf(file, "int8 %s = %hhd\n", name, *(signed char *) address);
			break;
		case asTYPEID_INT16:
			g_pFileSystem->FPrintf(file, "int16 %s = %hd\n", name, *(short *) address);
			break;
		case asTYPEID_INT32:
			g_pFileSystem->FPrintf(file, "int8 %s = %d\n", name, *(int *) address);
			break;
		case asTYPEID_INT64:
			g_pFileSystem->FPrintf(file, "int64 %s = %lld\n", name, *(asINT64 *) address);
			break;
		case asTYPEID_UINT8:
			g_pFileSystem->FPrintf(file, "uint8 %s = %hhu\n", name, *(unsigned char *) address);
			break;
		case asTYPEID_UINT16:
			g_pFileSystem->FPrintf(file, "uint16 %s = %hu\n", name, *(unsigned short *) address);
			break;
		case asTYPEID_UINT32:
			g_pFileSystem->FPrintf(file, "uint8 %s = %u\n", name, *(unsigned int *) address);
			break;
		case asTYPEID_UINT64:
			g_pFileSystem->FPrintf(file, "uint64 %s = %llu\n", name, *(asQWORD *) address);
			break;
		case asTYPEID_FLOAT:
			g_pFileSystem->FPrintf(file, "float %s = %f\n", name, *(float *) address);
			break;
		case asTYPEID_DOUBLE:
			g_pFileSystem->FPrintf(file, "double %s = %f\n", name, *(double *) address);
			break;
		default:
			g_pFileSystem->FPrintf(file, "%s = {...}\n", name);
			break;
	}
}
