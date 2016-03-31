/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
*
* Produced by:
*      Mykola Konyk
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniParameterObject.h"
#include "HoudiniParameterObjectVersion.h"
#include "HoudiniApi.h"
#include "HoudiniEngineString.h"


FHoudiniParameterObject::FHoudiniParameterObject() :
	ParmId(-1),
	NodeId(-1),
	HoudiniParameterObjectFlagsPacked(0u),
	HoudiniParameterObjectVersion(VER_HOUDINI_ENGINE_PARAMETEROBJECT_BASE)
{

}


FHoudiniParameterObject::FHoudiniParameterObject(HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo) :
	ParmId(ParmInfo.id),
	NodeId(InNodeId),
	HoudiniParameterObjectFlagsPacked(0u),
	HoudiniParameterObjectVersion(VER_HOUDINI_ENGINE_PARAMETEROBJECT_BASE)
{

}


FHoudiniParameterObject::FHoudiniParameterObject(HAPI_NodeId InNodeId, HAPI_ParmId InParmId) :
	ParmId(InParmId),
	NodeId(InNodeId),
	HoudiniParameterObjectFlagsPacked(0u),
	HoudiniParameterObjectVersion(VER_HOUDINI_ENGINE_PARAMETEROBJECT_BASE)
{

}


FHoudiniParameterObject::FHoudiniParameterObject(const FHoudiniParameterObject& HoudiniParameterObject) :
	ParmId(HoudiniParameterObject.ParmId),
	NodeId(HoudiniParameterObject.NodeId),
	HoudiniParameterObjectFlagsPacked(0u),
	HoudiniParameterObjectVersion(HoudiniParameterObject.HoudiniParameterObjectVersion)
{

}


bool
FHoudiniParameterObject::HapiGetNodeInfo(HAPI_NodeInfo& NodeInfo) const
{
	FMemory::Memset<HAPI_NodeInfo>(NodeInfo, 0);
	if(HAPI_RESULT_SUCCESS !=
		FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo))
	{
		return false;
	}

	return true;
}


bool
FHoudiniParameterObject::HapiGetParmInfo(HAPI_ParmInfo& ParmInfo) const
{
	HAPI_NodeInfo NodeInfo;
	if(!HapiGetNodeInfo(NodeInfo))
	{
		return false;
	}

	FMemory::Memset<HAPI_ParmInfo>(ParmInfo, 0);
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParameters(FHoudiniEngine::Get().GetSession(), NodeId, &ParmInfo,
		ParmId, 1))
	{
		return false;
	}

	return true;
}


bool
FHoudiniParameterObject::HapiGetName(FString& Name) const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	Name = TEXT("");
	FHoudiniEngineString HoudiniEngineString(ParmInfo.nameSH);
	HoudiniEngineString.ToFString(Name);

	return true;
}


bool
FHoudiniParameterObject::HapiGetLabel(FString& Label) const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	Label = TEXT("");
	FHoudiniEngineString HoudiniEngineString(ParmInfo.labelSH);
	HoudiniEngineString.ToFString(Label);

	return true;
}


bool
FHoudiniParameterObject::HapiIsNameEqual(const FString& Name) const
{
	FString ParameterName = TEXT("");
	if(!HapiGetName(ParameterName))
	{
		return false;
	}

	return ParameterName.Equals(Name);
}


bool
FHoudiniParameterObject::HapiIsLabelEqual(const FString& Label) const
{
	FString ParameterLabel = TEXT("");
	if(!HapiGetLabel(ParameterLabel))
	{
		return false;
	}

	return ParameterLabel.Equals(Label);
}


HAPI_ParmType
FHoudiniParameterObject::HapiGetParmType() const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return HAPI_PARMTYPE_INT;
	}

	return ParmInfo.type;
}


bool
FHoudiniParameterObject::HapiCheckParmType(HAPI_ParmType ParmType) const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	return ParmInfo.type == ParmType;
}


bool
FHoudiniParameterObject::HapiCheckParmCategoryInteger() const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	return ParmInfo.type >= HAPI_PARMTYPE_INT_START && ParmInfo.type <= HAPI_PARMTYPE_INT_END;
}


bool
FHoudiniParameterObject::HapiCheckParmCategoryFloat() const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	return ParmInfo.type >= HAPI_PARMTYPE_FLOAT_START && ParmInfo.type <= HAPI_PARMTYPE_FLOAT_END;
}


bool
FHoudiniParameterObject::HapiCheckParmCategoryString() const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	return ParmInfo.type >= HAPI_PARMTYPE_STRING_START && ParmInfo.type <= HAPI_PARMTYPE_STRING_END;
}


bool
FHoudiniParameterObject::HapiCheckParmCategoryPath() const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	return ParmInfo.type >= HAPI_PARMTYPE_PATH_START && ParmInfo.type <= HAPI_PARMTYPE_PATH_END;
}


bool
FHoudiniParameterObject::HapiCheckParmCategoryContainer() const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	return ParmInfo.type >= HAPI_PARMTYPE_CONTAINER_START && ParmInfo.type <= HAPI_PARMTYPE_CONTAINER_END;
}


bool
FHoudiniParameterObject::HapiCheckParmCategoryNonValue() const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	return ParmInfo.type >= HAPI_PARMTYPE_NONVALUE_START && ParmInfo.type <= HAPI_PARMTYPE_NONVALUE_END;
}


bool
FHoudiniParameterObject::HapiIsArray() const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	return ParmInfo.size > 1;
}


bool
FHoudiniParameterObject::HapiIsSubstance() const
{
	FString ParameterName = TEXT("");
	if(!HapiGetName(ParameterName))
	{
		return false;
	}

	return ParameterName.StartsWith(HAPI_UNREAL_PARAM_SUBSTANCE_PREFIX);
}


bool
FHoudiniParameterObject::HapiGetValue(int32& Value) const
{
	Value = 0;

	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmIntValues(FHoudiniEngine::Get().GetSession(), NodeId, &Value,
		ParmInfo.intValuesIndex, 1))
	{
		return false;
	}

	return true;
}


bool
FHoudiniParameterObject::HapiGetValue(float& Value) const
{
	Value = 0.0f;

	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmFloatValues(FHoudiniEngine::Get().GetSession(), NodeId, &Value,
		ParmInfo.floatValuesIndex, 1))
	{
		return false;
	}

	return true;
}


bool
FHoudiniParameterObject::HapiGetValue(FHoudiniEngineString& Value) const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	HAPI_StringHandle StringHandle = -1;

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmStringValues(FHoudiniEngine::Get().GetSession(), NodeId, false,
		&StringHandle, ParmInfo.stringValuesIndex, 1))
	{
		return false;
	}

	Value = FHoudiniEngineString(StringHandle);
	return true;
}


bool
FHoudiniParameterObject::HapiGetValue(FString& Value) const
{
	Value = TEXT("");

	FHoudiniEngineString HoudiniEngineString;
	if(!HapiGetValue(HoudiniEngineString))
	{
		return false;
	}

	if(!HoudiniEngineString.ToFString(Value))
	{
		return false;
	}

	return true;
}


bool
FHoudiniParameterObject::HapiGetValues(TArray<int32>& Values) const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	Values.Empty();

	if(ParmInfo.size > 0)
	{
		Values.SetNumUninitialized(ParmInfo.size);

		if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmIntValues(FHoudiniEngine::Get().GetSession(), NodeId, &Values[0],
			ParmInfo.intValuesIndex, ParmInfo.size))
		{
			return false;
		}
	}

	return true;
}


bool
FHoudiniParameterObject::HapiGetValues(TArray<float>& Values) const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	Values.Empty();

	if(ParmInfo.size > 0)
	{
		Values.SetNumUninitialized(ParmInfo.size);

		if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmFloatValues(FHoudiniEngine::Get().GetSession(), NodeId,
			&Values[0], ParmInfo.floatValuesIndex, ParmInfo.size))
		{
			return false;
		}
	}

	return true;
}


bool
FHoudiniParameterObject::HapiGetValues(TArray<FHoudiniEngineString>& Values) const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	Values.Empty();

	TArray<HAPI_StringHandle> StringHandles;

	if(ParmInfo.size > 0)
	{
		StringHandles.SetNumUninitialized(ParmInfo.size);

		if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmStringValues(FHoudiniEngine::Get().GetSession(), NodeId, false,
			&StringHandles[0], ParmInfo.stringValuesIndex, ParmInfo.size))
		{
			return false;
		}

		for(int32 Idx = 0, Num = StringHandles.Num(); Idx < Num; ++Idx)
		{
			Values.Add(FHoudiniEngineString(StringHandles[Idx]));
		}
	}

	return true;
}


bool
FHoudiniParameterObject::HapiGetValues(TArray<FString>& Values) const
{
	Values.Empty();

	TArray<FHoudiniEngineString> StringValues;
	if(!HapiGetValues(StringValues))
	{
		return false;
	}

	for(int32 Idx = 0, Num = StringValues.Num(); Idx < Num; ++Idx)
	{
		const FHoudiniEngineString& HoudiniEngineString = StringValues[Idx];
		FString UnrealString = TEXT("");
		HoudiniEngineString.ToFString(UnrealString);
		Values.Add(UnrealString);
	}

	return true;
}


bool
FHoudiniParameterObject::HapiSetValue(int32 Value) const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmIntValues(FHoudiniEngine::Get().GetSession(), NodeId, &Value,
		ParmInfo.intValuesIndex, 1))
	{
		return false;
	}

	return true;
}


bool
FHoudiniParameterObject::HapiSetValue(float Value) const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmFloatValues(FHoudiniEngine::Get().GetSession(), NodeId, &Value,
		ParmInfo.floatValuesIndex, 1))
	{
		return false;
	}

	return true;
}


bool
FHoudiniParameterObject::HapiSetValue(const FHoudiniEngineString& Value) const
{
	if(Value.HasValidId())
	{
		FString UnrealString = TEXT("");
		if(Value.ToFString(UnrealString))
		{
			return HapiSetValue(UnrealString);
		}
	}

	return false;
}


bool
FHoudiniParameterObject::HapiSetValue(const FString& Value) const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	std::string StringValue = TCHAR_TO_UTF8(*Value);
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), NodeId,
		StringValue.c_str(), ParmInfo.id, 0))
	{
		return false;
	}

	return true;
}


bool
FHoudiniParameterObject::HapiSetValues(const TArray<int32>& Values) const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	if(!Values.Num())
	{
		return false;
	}

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmIntValues(FHoudiniEngine::Get().GetSession(), NodeId, &Values[0],
		ParmInfo.intValuesIndex, Values.Num()))
	{
		return false;
	}

	return true;
}


bool
FHoudiniParameterObject::HapiSetValues(const TArray<float>& Values) const
{
	HAPI_ParmInfo ParmInfo;
	if(!HapiGetParmInfo(ParmInfo))
	{
		return false;
	}

	if(!Values.Num())
	{
		return false;
	}

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmFloatValues(FHoudiniEngine::Get().GetSession(), NodeId, &Values[0],
		ParmInfo.floatValuesIndex, Values.Num()))
	{
		return false;
	}

	return true;
}


bool
FHoudiniParameterObject::HapiSetValues(const TArray<FHoudiniEngineString>& Values) const
{
	for(int32 Idx = 0; Idx < Values.Num(); ++Idx)
	{
		if(!HapiSetValue(Values[Idx]))
		{
			return false;
		}
	}

	return true;
}


bool
FHoudiniParameterObject::HapiSetValues(const TArray<FString>& Values) const
{
	for(int32 Idx = 0; Idx < Values.Num(); ++Idx)
	{
		if(!HapiSetValue(Values[Idx]))
		{
			return false;
		}
	}

	return true;
}
