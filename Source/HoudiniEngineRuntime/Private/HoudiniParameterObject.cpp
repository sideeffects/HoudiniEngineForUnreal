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
#include "HoudiniApi.h"
#include "HoudiniEngineString.h"


FHoudiniParameterObject::FHoudiniParameterObject() :
	NodeId(-1),
	ParmId(-1)
{

}


FHoudiniParameterObject::FHoudiniParameterObject(HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo) :
	NodeId(InNodeId),
	ParmId(ParmInfo.id)
{

}


FHoudiniParameterObject::FHoudiniParameterObject(HAPI_NodeId InNodeId, HAPI_ParmId InParmId) :
	NodeId(InNodeId),
	ParmId(InParmId)
{

}


FHoudiniParameterObject::FHoudiniParameterObject(const FHoudiniParameterObject& HoudiniParameterObject) :
	NodeId(HoudiniParameterObject.NodeId),
	ParmId(HoudiniParameterObject.ParmId)
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
FHoudiniParameterObject::HapiGetValue(int32& Value) const
{
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
