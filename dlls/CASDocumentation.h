#ifndef CASDOCUMENTATION_H
#define CASDOCUMENTATION_H

#include "CASBaseClass.h"
#include "CHashStringPool.h"
#include <angelscript.h>

class CCommand;

class CKeyvaluesWriter;

// Script access mask bits, one for each ModuleType
#define AS_ACCESS_MAP		( 1 << 0 )
#define AS_ACCESS_PLUGIN	( 1 << 1 )
#define AS_ACCESS_ALL		( AS_ACCESS_MAP | AS_ACCESS_PLUGIN )

class CASDocumentation : public CASBaseClass
{
public:
	enum ENUM_TYPE
	{
		ENUM_SIGNED = 0,
		ENUM_UNSIGNED,
		ENUM_CHAR
	};

	CASDocumentation(asIScriptEngine *engine);
	~CASDocumentation();

	const char *AllocString(const char *text);
	const char *GetDefaultNamespace(void) const;
	int SetDefaultNamespace(const char *nameSpace);
	asDWORD SetDefaultAccessMask(asDWORD accessMask);
	void RegisterGlobalFunction(const char *documentation, const char *declaration,
		const asSFuncPtr &function, asDWORD callingConvention, void *auxiliary = NULL);
	void RegisterGlobalProperty(const char *documentation, const char *declaration, void *address);
	void RegisterInterface(const char *documentation, const char *name);
	void RegisterInterfaceMethod(const char *documentation, const char *name, const char *declaration);
	void RegisterObjectType(const char *documentation, const char *name, int byteSize, asDWORD flags);
	void RegisterObjectMethod(const char *documentation, const char *name, const char *declaration,
		const asSFuncPtr &function, asDWORD callingConvention);
	void RegisterObjectBehaviour(const char *documentation, const char *name,
		asEBehaviours behaviour, const char *declaration, const asSFuncPtr &function,
		asDWORD callingConvention, void *auxiliary = NULL);
	void RegisterObjectProperty(const char *documentation, const char *name,
		const char *declaration, int offset);
	void RegisterEnum(const char *documentation, const char *name, ENUM_TYPE type = ENUM_SIGNED);
	void RegisterEnumValue(const char *documentation, const char *name, const char *valueName, int value);
	void RegisterTypedef(const char *documentation, const char *name, const char *type);
	int RegisterFuncDef(const char *documentation, const char *declaration);
	void OutputDocumentationToFile(const char *fileName);
	void OutputGlobalsDocs(const char *fileName);
	void RegistrationComplete(void);

private:
	friend void as_showversioninfo_Callback( const CCommand &command );
	friend class CASBaseRegistration;
	friend class CASServerRegistration;

	friend void RegisterSCScriptModuleHookManager(CASDocumentation *documentation);

	enum
	{
		DOCUMENTATION_VERSION = 2,
		FILE_NAME_LENGTH = 260,
		TYPE_NAME_LENGTH = 512,
		INTEGER_BUFFER_LENGTH = 16,
		CHARACTER_BUFFER_LENGTH = 8,
		FLAGS_BUFFER_LENGTH = 80
	};

	static const char *const GAME_VERSION;
	static const char *const API_VERSION;

	struct MemberInfo_t
	{
		const char		*m_pszDocumentation;
		const char		*m_pszDeclaration;
		MemberInfo_t	*m_pNext;
	};

	struct GlobalInfo_t
	{
		const char		*m_pszDocumentation;
		const char		*m_pszNamespace;
		const char		*m_pszDeclaration;
		GlobalInfo_t	*m_pNext;
	};

	struct InterfaceInfo_t
	{
		const char			*m_pszDocumentation;
		const char			*m_pszNamespace;
		const char			*m_pszName;
		MemberInfo_t		*m_pMethods;
		InterfaceInfo_t		*m_pNext;
	};

	struct ClassInfo_t
	{
		const char		*m_pszDocumentation;
		const char		*m_pszNamespace;
		const char		*m_pszName;
		asDWORD			m_Flags;
		MemberInfo_t	*m_pMethods;
		MemberInfo_t	*m_pProperties;
		ClassInfo_t		*m_pNext;
	};

	struct EnumValueInfo_t
	{
		const char			*m_pszDocumentation;
		const char			*m_pszName;
		int					m_iValue;
		EnumValueInfo_t		*m_pNext;
	};

	struct EnumInfo_t
	{
		const char		*m_pszDocumentation;
		const char		*m_pszNamespace;
		const char		*m_pszName;
		ENUM_TYPE		m_Type;
		EnumValueInfo_t	*m_pValues;
		EnumInfo_t		*m_pNext;
	};

	struct TypedefInfo_t
	{
		const char		*m_pszDocumentation;
		const char		*m_pszNamespace;
		const char		*m_pszType;
		const char		*m_pszName;
		TypedefInfo_t	*m_pNext;
	};

	InterfaceInfo_t *LookupInterface(const char *name);
	ClassInfo_t *LookupClass(const char *name);
	EnumInfo_t *LookupEnum(const char *name);
	const char *FormatInt(int value);
	const char *FormatUnsigned(unsigned int value);
	const char *FormatChar(char value);
	const char *FormatEnumValue(int value, ENUM_TYPE type);
	void OutputDocInfo(CKeyvaluesWriter &writer);
	void OutputInterfaces(CKeyvaluesWriter &writer);
	void OutputClasses(CKeyvaluesWriter &writer);
	void OutputEnums(CKeyvaluesWriter &writer);
	void OutputFunctions(CKeyvaluesWriter &writer);
	void OutputProperties(CKeyvaluesWriter &writer);
	void OutputTypedefs(CKeyvaluesWriter &writer);
	void OutputFuncDefs(CKeyvaluesWriter &writer);

	CHashStringPool	m_Strings;
	bool			m_bOutputDocs;
	asIScriptEngine	*m_pEngine;
	const char		*m_pszNamespace;
	GlobalInfo_t	*m_pFunctions;
	GlobalInfo_t	*m_pProperties;
	InterfaceInfo_t	*m_pInterfaces;
	ClassInfo_t		*m_pClasses;
	EnumInfo_t		*m_pEnums;
	TypedefInfo_t	*m_pTypedefs;
	GlobalInfo_t	*m_pFuncDefs;
};

#endif
