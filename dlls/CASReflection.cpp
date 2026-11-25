#include "CASReflection.h"
#include "CASLog.h"
#include "CASArguments.h"
#include "CASMethod.h"
#include "CASServerManager.h"
#include "CString.h"
#include <limits.h>
#include <string.h>

bool CASReflection::SetPrimitiveArgument(ArgumentValue_t &value, int typeId, void *argument, bool &isPrimitive)
{
	if (!argument)
	{
		isPrimitive = false;
		return false;
	}

	isPrimitive = true;

	switch (typeId)
	{
	case asTYPEID_VOID:
		return false;

	case asTYPEID_BOOL:
	case asTYPEID_INT8:
	case asTYPEID_UINT8:
		value.byteValue = *(asBYTE *)argument;
		break;

	case asTYPEID_INT16:
	case asTYPEID_UINT16:
		value.wordValue = *(asWORD *)argument;
		break;

	case asTYPEID_INT32:
	case asTYPEID_UINT32:
	case asTYPEID_FLOAT:
		value.dwordValue = *(asDWORD *)argument;
		break;

	case asTYPEID_INT64:
	case asTYPEID_UINT64:
	case asTYPEID_DOUBLE:
		value.qwordValue = *(asQWORD *)argument;
		break;

	default:
		isPrimitive = false;
		break;
	}

	return true;
}

bool CASReflection::ConvertInputArgToLargest(int typeId, const ArgumentValue_t &value, asINT64 &integerValue, double &realValue)
{
	switch (typeId)
	{
	case asTYPEID_VOID:
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
			"CASCPPReflection::ConvertInputArgToLargest: the impossible happened: a void argument, aborting!\n");
		return false;

	case asTYPEID_BOOL:
	case asTYPEID_INT8:
	case asTYPEID_UINT8:
		integerValue = value.byteValue;
		break;

	case asTYPEID_INT16:
	case asTYPEID_UINT16:
		integerValue = value.wordValue;
		break;

	case asTYPEID_INT64:
	case asTYPEID_UINT64:
		integerValue = (asINT64)value.qwordValue;
		break;

	case asTYPEID_FLOAT:
		realValue = value.floatValue;
		integerValue = (asINT64)realValue;
		return true;

	case asTYPEID_DOUBLE:
		realValue = value.doubleValue;
		integerValue = (asINT64)realValue;
		return true;

	case asTYPEID_INT32:
	case asTYPEID_UINT32:
		integerValue = value.dwordValue;
		break;

	default:
		if (typeId <= asTYPEID_DOUBLE || (typeId & asTYPEID_MASK_OBJECT))
			return false;

		integerValue = value.dwordValue;
		break;
	}

	realValue = (double)integerValue;
	return true;
}

ArgumentType_t CASReflection::ArgumentTypeFromTypeId(int typeId, asDWORD flags)
{
	if (typeId & (asTYPEID_MASK_OBJECT | asTYPEID_OBJHANDLE))
	{
		if (flags & asTM_INREF)
			return ARGUMENT_HANDLE;

		return (flags & asTM_OUTREF) ? ARGUMENT_OBJECT : ARGUMENT_NONE;
	}

	if (typeId >= asTYPEID_BOOL && typeId <= asTYPEID_DOUBLE)
		return ARGUMENT_PRIMITIVE;

	if (typeId > asTYPEID_DOUBLE && !(typeId & asTYPEID_MASK_OBJECT))
		return ARGUMENT_ENUM;

	return ARGUMENT_NONE;
}

bool CASReflection::GetArgumentFromVarargs(ArgumentValue_t &value, int typeId, asDWORD flags, va_list &args,
	asDWORD *argumentFlags, ArgumentType_t *argumentType)
{
	if (argumentType && !argumentFlags)
		return false;

	if (argumentType)
		*argumentType = ARGUMENT_NONE;

	if (typeId & (asTYPEID_MASK_OBJECT | asTYPEID_OBJHANDLE))
	{
		value.pointerValue = va_arg(args, void *);

		if (!argumentType)
			return true;

		if (*argumentFlags & asTM_INREF)
		{
			*argumentType = ARGUMENT_HANDLE;
			return true;
		}

		if (*argumentFlags & asTM_OUTREF)
		{
			*argumentType = ARGUMENT_OBJECT;
			return true;
		}

		CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
			"CASCPPReflection::GetArgumentFromVarargs: unknown object type, cannot convert!\n");
		return false;
	}

	if (typeId >= asTYPEID_BOOL && typeId <= asTYPEID_DOUBLE && (flags & asTM_INOUTREF))
	{
		value.pointerValue = va_arg(args, void *);

		if (argumentType)
			*argumentType = ARGUMENT_PRIMITIVE;

		return true;
	}

	switch (typeId)
	{
	case asTYPEID_VOID:
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
			"CASCPPReflection::SetContextArgument: the impossible happened: a void argument\n");

		if (argumentType)
			*argumentType = ARGUMENT_PRIMITIVE;

		return false;

	case asTYPEID_BOOL:
	case asTYPEID_INT8:
	case asTYPEID_UINT8:
		value.byteValue = (asBYTE)va_arg(args, int);
		break;

	case asTYPEID_INT16:
	case asTYPEID_UINT16:
		value.wordValue = (asWORD)va_arg(args, int);
		break;

	case asTYPEID_INT32:
	case asTYPEID_UINT32:
		value.dwordValue = va_arg(args, asDWORD);
		break;

	case asTYPEID_INT64:
	case asTYPEID_UINT64:
		value.qwordValue = va_arg(args, asQWORD);
		break;

	case asTYPEID_FLOAT:
		value.floatValue = (float)va_arg(args, double);
		break;

	case asTYPEID_DOUBLE:
		value.doubleValue = va_arg(args, double);
		break;

	default:
		if (typeId > asTYPEID_DOUBLE && !(typeId & asTYPEID_MASK_OBJECT))
		{
			if (argumentType)
				*argumentType = ARGUMENT_ENUM;

			value.dwordValue = va_arg(args, asDWORD);
			return true;
		}

		CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
			"CASCPPReflection::SetContextArgument: Attempted to set parameter of unknown type, aborting!\n");
		return false;
	}

	if (argumentType)
		*argumentType = ARGUMENT_PRIMITIVE;

	return true;
}

const char *PrimitiveTypeIdToString(int typeId)
{
	static const char *const names[] = {
		"void", "bool", "int8", "int16", "int32", "int64",
		"uint8", "uint16", "uint32", "uint64", "float", "double"
	};

	if ((unsigned int) typeId > asTYPEID_DOUBLE)
		return NULL;

	return names[typeId];
}

bool CASReflection::SetPrimitiveArgument(CASArgument &argument, int typeId, void *value, bool &isPrimitive)
{
	isPrimitive = false;

	if (!value)
		return false;

	ArgumentValue_t converted;

	converted.qwordValue = 0;

	if (!SetPrimitiveArgument(converted, typeId, value, isPrimitive))
		return false;

	if (isPrimitive && !argument.Set(typeId, ARGUMENT_PRIMITIVE, converted, false))
	{
		isPrimitive = false;
		return false;
	}

	return true;
}

bool CASReflection::SetObjectArgument(asIScriptEngine *engine, CASArgument &argument, int typeId, void *value)
{
	asITypeInfo *type = engine->GetTypeInfoById(typeId);
	ArgumentValue_t converted;
	ArgumentType_t argumentType;

	converted.qwordValue = 0;

	if (type)
	{
		asDWORD flags = type->GetFlags();

		if (flags & asOBJ_VALUE)
		{
			converted.pointerValue = engine->CreateScriptObjectCopy(value, type);
			argumentType = ARGUMENT_OBJECT;
		}
		else if (flags & asOBJ_REF)
		{
			converted.pointerValue = value;

			if (typeId & asTYPEID_OBJHANDLE)
			{
				converted.pointerValue = *(void **) value;
				typeId &= ~asTYPEID_OBJHANDLE;
			}

			if ((flags & asOBJ_FUNCDEF) &&
			    ((asIScriptFunction *) converted.pointerValue)->GetFuncType() == asFUNC_DELEGATE)
				converted.pointerValue = *(void **) converted.pointerValue;

			engine->AddRefScriptObject(converted.pointerValue, type);
			argumentType = ARGUMENT_HANDLE;
		}
		else
		{
			CASLog *log = CASLog::GetInstance();
			const char *name = type->GetName();

			log->Error(AS_LOG_DEFAULT, "CASArguments: unknown type '%s', aborting!\n", name);
			return false;
		}
	}
	else if (typeId > asTYPEID_DOUBLE && !(typeId & asTYPEID_MASK_OBJECT))
	{
		converted.dwordValue = *(asDWORD *) value;
		argumentType = ARGUMENT_ENUM;
	}
	else
	{
		CASLog *log = CASLog::GetInstance();
		const char *declaration = engine->GetTypeDeclaration(typeId, true);

		log->Error(AS_LOG_DEFAULT, "CASArguments: unknown type '%s'(%d), aborting!\n", declaration, typeId);
		return false;
	}

	argument.Set(typeId, argumentType, converted, false);
	return true;
}

bool CASReflection::SetArgument(asIScriptEngine *engine, CASArgument &argument, int typeId, void *value)
{
	if (!engine)
		return false;

	bool isPrimitive = false;

	if (!SetPrimitiveArgument(argument, typeId, value, isPrimitive))
		return false;

	return isPrimitive ? true : SetObjectArgument(engine, argument, typeId, value);
}

bool CASReflection::GetReturnValue(CASArgument &argument, int typeId, asDWORD flags, asIScriptContext *context)
{
	if (!context)
		return false;

	ArgumentValue_t value;
	ArgumentType_t argumentType = ARGUMENT_NONE;
	bool success = true;

	value.qwordValue = 0;

	if (typeId & asTYPEID_OBJHANDLE)
	{
		value.pointerValue = context->GetReturnObject();
		argumentType = ARGUMENT_HANDLE;
	}
	else if (typeId & asTYPEID_MASK_OBJECT)
	{
		asITypeInfo *type = CASServerManager::GetInstance()->m_pEngine->GetTypeInfoById(typeId);

		if (type)
		{
			asDWORD typeFlags = type->GetFlags();

			value.pointerValue = context->GetReturnObject();

			if (typeFlags & asOBJ_REF)
				argumentType = ARGUMENT_HANDLE;
			else if (typeFlags & asOBJ_VALUE)
				argumentType = ARGUMENT_OBJECT;
			else
			{
				CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
				                             "CASCPPReflection::GetReturnValue: unknown object type, cannot convert!\n");
				success = false;
			}
		}
		else
		{
			CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
			                             "CASCPPReflection::GetReturnValue: failed to get object type!\n");
			success = false;
		}
	}
	else if (typeId == asTYPEID_VOID)
		argumentType = ARGUMENT_VOID;
	else
	{
		bool isPrimitive = true;

		if (flags & asTM_INOUTREF)
		{
			if (!SetPrimitiveArgument(value, typeId, context->GetReturnAddress(), isPrimitive))
			{
				CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
				                             "CASCPPReflection::GetReturnValue: Something went wrong while trying to convert the return value to a usable type!\n");
				success = false;
			}
		}
		else
		{
			switch (typeId)
			{
			case asTYPEID_BOOL:
			case asTYPEID_INT8:
			case asTYPEID_UINT8:
				value.byteValue = context->GetReturnByte();
				break;

			case asTYPEID_INT16:
			case asTYPEID_UINT16:
				value.wordValue = context->GetReturnWord();
				break;

			case asTYPEID_INT32:
			case asTYPEID_UINT32:
				value.dwordValue = context->GetReturnDWord();
				break;

			case asTYPEID_INT64:
			case asTYPEID_UINT64:
				value.qwordValue = context->GetReturnQWord();
				break;

			case asTYPEID_FLOAT:
				value.floatValue = context->GetReturnFloat();
				break;

			case asTYPEID_DOUBLE:
				value.doubleValue = context->GetReturnDouble();
				break;

			default:
				isPrimitive = false;
				break;
			}
		}

		if (success)
		{
			if (isPrimitive)
				argumentType = ARGUMENT_PRIMITIVE;
			else if ((typeId & asTYPEID_MASK_SEQNBR) > asTYPEID_DOUBLE)
			{
				value.dwordValue = context->GetReturnDWord();
				argumentType = ARGUMENT_ENUM;
			}
			else
			{
				CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
				                             "CASCPPReflection::GetReturnValue: unknown primitive return type, cannot convert!\n");
				success = false;
			}
		}
	}

	argument.Set(typeId, argumentType, value, true);
	return success;
}

bool CASReflection::GetReturnValue(CASArgument &argument, asIScriptFunction *function,
                                   asIScriptContext *context, asDWORD *flags)
{
	if (!function || !context)
		return false;

	asDWORD returnFlags;
	int typeId = function->GetReturnTypeId(&returnFlags);

	if (flags)
		*flags = returnFlags;

	return GetReturnValue(argument, typeId, returnFlags, context);
}

bool CASReflection::GetReturnValue(void *value, int typeId, asDWORD flags,
                                   asIScriptContext *context, CASModule *module)
{
	if (!context || !module)
		return false;

	if (typeId & asTYPEID_OBJHANDLE)
	{
		*(void **) value = context->GetReturnObject();
		return true;
	}

	if (typeId & asTYPEID_MASK_OBJECT)
	{
		asITypeInfo *type = CASServerManager::GetInstance()->m_pEngine->GetTypeInfoById(typeId);

		if (!type)
		{
			CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
			                             "CASCPPReflection::GetReturnValue: failed to get object type!\n");
			return false;
		}

		void *object = context->GetReturnObject();
		unsigned int methodCount = type->GetMethodCount();

		for (unsigned int index = 0; index < methodCount; index++)
		{
			asIScriptFunction *function = type->GetMethodByIndex(index, true);
			int argumentTypeId;
			asDWORD argumentFlags;

			if (function->GetParamCount() != 1 || function->GetParam(0, &argumentTypeId, &argumentFlags) < 0 ||
			    argumentTypeId != type->GetTypeId() || !(argumentFlags & asTM_INREF))
				continue;

			CASMethod *method = new CASMethod(function, module);

			method->SetThisPointer(value);
			method->Call(0, object);
			method->Release();
			return true;
		}

		CASLog *log = CASLog::GetInstance();
		const char *name = type->GetName();
		const char *nameSpace = type->GetNamespace();

		log->Error(AS_LOG_DEFAULT,
		           "CASCPPReflection::GetReturnValue: no assignment operator found for type '%s::%s', cannot convert!\n",
		           nameSpace, name);
		return false;
	}

	if (typeId == asTYPEID_VOID)
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT, "CASCPPReflection::GetReturnValue: type is null!\n");
		return true;
	}

	if (flags & asTM_INOUTREF)
	{
		*(void **) value = context->GetReturnAddress();
		return true;
	}

	switch (typeId)
	{
	case asTYPEID_BOOL:
		*(bool *) value = context->GetReturnByte() != 0;
		break;

	case asTYPEID_INT8:
	case asTYPEID_UINT8:
		*(asBYTE *) value = context->GetReturnByte();
		break;

	case asTYPEID_INT16:
	case asTYPEID_UINT16:
		*(asWORD *) value = context->GetReturnWord();
		break;

	case asTYPEID_INT32:
	case asTYPEID_UINT32:
		*(asDWORD *) value = context->GetReturnDWord();
		break;

	case asTYPEID_INT64:
	case asTYPEID_UINT64:
		*(asQWORD *) value = context->GetReturnQWord();
		break;

	case asTYPEID_FLOAT:
		*(float *) value = context->GetReturnFloat();
		break;

	case asTYPEID_DOUBLE:
		*(double *) value = context->GetReturnDouble();
		break;

	default:
		if ((typeId & asTYPEID_MASK_SEQNBR) > asTYPEID_DOUBLE)
			*(asDWORD *) value = context->GetReturnDWord();
		else
		{
			CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
			                             "CASCPPReflection::GetReturnValue: unknown primitive return type, cannot convert!\n");
			return false;
		}
		break;
	}

	return true;
}

bool CASReflection::ConvertInputArgToLargest(const CASArgument *argument, asINT64 &integerValue,
                                             double &realValue)
{
	return argument && ConvertInputArgToLargest(argument->m_iTypeId, argument->m_Value, integerValue, realValue);
}

bool CASReflection::ConvertEnumToPrimitive(const CASArgument &argument, int typeId, ArgumentValue_t &value)
{
	if (argument.m_iTypeId <= asTYPEID_DOUBLE)
	{
		if (argument.m_iTypeId != asTYPEID_INT32)
			return false;
	}
	else if (argument.m_iTypeId & asTYPEID_MASK_OBJECT)
		return false;

	switch (typeId)
	{
	case asTYPEID_INT8:
	case asTYPEID_UINT8:
		if (argument.m_Value.dwordValue > UCHAR_MAX)
			CASLog::GetInstance()->Warning(AS_LOG_DEFAULT, "Truncating enum value to 1 byte!\n");

		value.byteValue = (asBYTE) argument.m_Value.dwordValue;
		break;

	case asTYPEID_INT16:
	case asTYPEID_UINT16:
		if (argument.m_Value.dwordValue > USHRT_MAX)
			CASLog::GetInstance()->Warning(AS_LOG_DEFAULT, "Truncating enum value to 2 bytes!\n");

		value.wordValue = (asWORD) argument.m_Value.dwordValue;
		break;

	case asTYPEID_INT32:
	case asTYPEID_UINT32:
		value.dwordValue = argument.m_Value.dwordValue;
		break;

	case asTYPEID_INT64:
	case asTYPEID_UINT64:
		value.qwordValue = argument.m_Value.dwordValue;
		break;

	case asTYPEID_FLOAT:
		value.floatValue = (float) argument.m_Value.dwordValue;
		break;

	case asTYPEID_DOUBLE:
		value.doubleValue = (double) argument.m_Value.dwordValue;
		break;

	default:
		return false;
	}

	return true;
}

bool CASReflection::ConvertPrimitiveToEnum(const CASArgument &argument, ArgumentValue_t &value)
{
	switch (argument.m_iTypeId)
	{
	case asTYPEID_BOOL:
		break;

	case asTYPEID_INT8:
	case asTYPEID_UINT8:
		value.dwordValue = argument.m_Value.byteValue;
		break;

	case asTYPEID_INT16:
	case asTYPEID_UINT16:
		value.dwordValue = argument.m_Value.wordValue;
		break;

	case asTYPEID_INT64:
	case asTYPEID_UINT64:
		if (argument.m_Value.qwordValue >> 32)
			CASLog::GetInstance()->Warning(AS_LOG_DEFAULT, "Truncating 8 byte value to 4 bytes!\n");

	case asTYPEID_INT32:
	case asTYPEID_UINT32:
		value.dwordValue = argument.m_Value.dwordValue;
		break;

	case asTYPEID_FLOAT:
		value.dwordValue = (asDWORD) (asINT64) argument.m_Value.floatValue;
		break;

	case asTYPEID_DOUBLE:
		if (argument.m_Value.doubleValue > (double) UINT_MAX)
			CASLog::GetInstance()->Warning(AS_LOG_DEFAULT, "Truncating 8 byte value to 4 bytes!\n");

		value.dwordValue = (asDWORD) (asINT64) argument.m_Value.doubleValue;
		break;

	default:
		return false;
	}

	return true;
}

bool CASReflection::CreateFunctionSignature(asIScriptEngine *engine, std::stringstream &stream,
                                            const CString &name, CASArguments &arguments, unsigned int firstArgument, asIScriptGeneric *generic)
{
	stream << "void " << name.Data() << '(';

	CASArgument *argument = arguments.m_pArguments;
	unsigned int argumentCount = arguments.m_uiCount;

	for (unsigned int index = 0; index < argumentCount; index++)
	{
		int typeId = generic->GetArgTypeId(firstArgument + index);
		const char *declaration = engine->GetTypeDeclaration(typeId, true);

		if (!declaration)
			return false;

		if (index)
			stream << ", ";

		stream << declaration;

		switch (argument[index].m_ArgumentType)
		{
		case ARGUMENT_PRIMITIVE:
		case ARGUMENT_ENUM:
			break;

		case ARGUMENT_OBJECT:
			stream << "& in";
			break;

		case ARGUMENT_HANDLE:
			if (declaration[strlen(declaration) - 1] != '@')
				stream << '@';
			break;

		default:
			CASLog::GetInstance()->Error(AS_LOG_DEFAULT, "CScheduler: invalid input argument!\n");
			return false;
		}
	}

	stream << ')';
	return true;
}

bool CASReflection::SetContextArgument(asIScriptEngine *engine, asIScriptFunction *function,
                                       asIScriptContext *context, unsigned int index, int typeId, ArgumentValue_t &value, bool varargs)
{
	int parameterTypeId;
	asDWORD parameterFlags;
	int result = function->GetParam(index, &parameterTypeId, &parameterFlags);

	if (result < 0)
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
		                             "CASCPPReflection::SetContextArgument: an error occurred while getting function parameter information, aborting!\n");
		return false;
	}

	if (parameterTypeId & asTYPEID_OBJHANDLE)
	{
		void *object = value.pointerValue;

		if (typeId & asTYPEID_OBJHANDLE)
			object = *(void **) object;

		if (typeId & asTYPEID_MASK_OBJECT)
		{
			asITypeInfo *type = engine->GetTypeInfoById(parameterTypeId);

			if (!type)
			{
				CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
				                             "CASCPPReflection::SetContextArgument: Could not get object type for argument %u, aborting!\n", index);
				return false;
			}

			if (type->GetFlags() & asOBJ_FUNCDEF)
			{
				if (!((asIScriptFunction *) object)->IsCompatibleWithTypeId(parameterTypeId))
				{
					CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
					                             "CASCPPReflection::SetContextArgument: Could not set argument %u, argument function signatures are different, aborting!\n", index);
					return false;
				}

				return context->SetArgObject(index, object) >= 0;
			}

			void *converted = NULL;
			asITypeInfo *sourceType = engine->GetTypeInfoById(typeId);

			if (engine->RefCastObject(object, sourceType, type, &converted) >= 0)
			{
				engine->ReleaseScriptObject(converted, type);
				return context->SetArgObject(index, object) >= 0;
			}
		}

		CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
		                             "CASCPPReflection::SetContextArgument: source argument is incompatible with target, aborting!\n");
		return false;
	}

	if (parameterTypeId & asTYPEID_MASK_OBJECT)
	{
		if (parameterTypeId == typeId)
			return context->SetArgObject(index, value.pointerValue) >= 0;

		return true;
	}

	if (parameterTypeId >= asTYPEID_BOOL && parameterTypeId <= asTYPEID_DOUBLE &&
	    (parameterFlags & asTM_INOUTREF))
		return context->SetArgAddress(index, varargs ? value.pointerValue : &value) >= 0;

	asINT64 integerValue;
	double realValue;

	if (!ConvertInputArgToLargest(typeId, value, integerValue, realValue))
		return false;

	bool success;

	switch (parameterTypeId)
	{
	case asTYPEID_VOID:
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
		                             "CASCPPReflection::SetContextArgument: the impossible happened: a void argument, aborting!\n");
		success = false;
		break;

	case asTYPEID_BOOL:
	case asTYPEID_INT8:
	case asTYPEID_UINT8:
		success = context->SetArgByte(index, (asBYTE) integerValue) >= 0;
		break;

	case asTYPEID_INT16:
	case asTYPEID_UINT16:
		success = context->SetArgWord(index, (asWORD) integerValue) >= 0;
		break;

	case asTYPEID_INT32:
	case asTYPEID_UINT32:
		success = context->SetArgDWord(index, (asDWORD) integerValue) >= 0;
		break;

	case asTYPEID_INT64:
	case asTYPEID_UINT64:
		success = context->SetArgQWord(index, (asQWORD) integerValue) >= 0;
		break;

	case asTYPEID_FLOAT:
		success = context->SetArgFloat(index, (float) realValue) >= 0;
		break;

	case asTYPEID_DOUBLE:
		success = context->SetArgDouble(index, realValue) >= 0;
		break;

	default:
		if ((parameterTypeId & asTYPEID_MASK_SEQNBR) <= asTYPEID_DOUBLE)
		{
			CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
			                             "CASCPPReflection::SetContextArgument: Attempted to set parameter of unknown type, aborting!\n");
			return false;
		}

		if (parameterFlags & asTM_INOUTREF)
			return context->SetArgAddress(index, varargs ? value.pointerValue : &value) >= 0;

		return context->SetArgDWord(index, value.dwordValue) >= 0;
	}

	if (typeId != parameterTypeId)
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
		                             "CASCPPReflection::SetContextArgument: Attempted to set primitive value of type '%s' to value of type '%s', aborting!\n",
		                             PrimitiveTypeIdToString(parameterTypeId), PrimitiveTypeIdToString(typeId));
		return false;
	}

	return success;
}

bool CASReflection::SetContextArgument(asIScriptEngine *engine, asIScriptFunction *function,
                                       asIScriptContext *context, unsigned int index, CASArgument &argument)
{
	return SetContextArgument(engine, function, context, index, argument.m_iTypeId, argument.m_Value, false);
}

bool CASReflection::SetContextArgument(asIScriptEngine *engine, asIScriptFunction *function,
                                       asIScriptContext *context, unsigned int index, va_list &arguments)
{
	int typeId;
	asDWORD flags;
	int result = function->GetParam(index, &typeId, &flags);

	if (result < 0)
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
		                             "CASCPPReflection::SetContextArgument: an error occurred while getting function parameter information, aborting!\n");
		return false;
	}

	ArgumentValue_t value;

	value.qwordValue = 0;

	if (!GetArgumentFromVarargs(value, typeId, flags, arguments, NULL, NULL))
		return false;

	return SetContextArgument(engine, function, context, index, typeId & ~asTYPEID_OBJHANDLE, value, true);
}

bool CASReflection::SetArguments(asIScriptFunction *function, asIScriptContext *context,
                                 CASArguments &arguments)
{
	if (!function || !context)
		return false;

	unsigned int count = function->GetParamCount();

	if (count != arguments.m_uiCount)
	{
		CASLog::GetInstance()->Error(AS_LOG_DEFAULT,
		                             "CASCPPReflection::SetArguments: argument count for function '%s::%s' is incorrect: expected %u, got %u!\n",
		                             function->GetNamespace(), function->GetName(), count, arguments.m_uiCount);
		return false;
	}

	asIScriptEngine *engine = CASServerManager::GetInstance()->m_pEngine;
	CASArgument *argument = arguments.m_pArguments;

	for (unsigned int index = 0; index < count; index++)
	{
		if (!SetContextArgument(engine, function, context, index, argument[index]))
			return false;
	}

	return true;
}

bool CASReflection::SetArguments(asIScriptFunction *function, asIScriptContext *context, va_list arguments)
{
	if (!function || !context || !arguments)
		return false;

	unsigned int count = function->GetParamCount();
	asIScriptEngine *engine = CASServerManager::GetInstance()->m_pEngine;

	for (unsigned int index = 0; index < count; index++)
	{
		if (!SetContextArgument(engine, function, context, index, arguments))
			return false;
	}

	return true;
}
