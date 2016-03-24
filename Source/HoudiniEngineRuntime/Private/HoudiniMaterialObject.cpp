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
#include "HoudiniMaterialObject.h"
#include "HoudiniParameterObject.h"
#include "HoudiniApi.h"
#include "HoudiniEngineString.h"


FHoudiniMaterialObject::FHoudiniMaterialObject() :
	AssetId(-1),
	NodeId(-1)
{

}


FHoudiniMaterialObject::FHoudiniMaterialObject(const HAPI_MaterialInfo& MaterialInfo) :
	AssetId(MaterialInfo.assetId),
	NodeId(MaterialInfo.nodeId)
{

}


FHoudiniMaterialObject::FHoudiniMaterialObject(HAPI_AssetId InAssetId, HAPI_NodeId InNodeId) :
	AssetId(InAssetId),
	NodeId(InNodeId)
{

}


bool
FHoudiniMaterialObject::HapiGetNodeInfo(HAPI_NodeInfo& NodeInfo) const
{
	if(-1 == NodeId)
	{
		return false;
	}

	FMemory::Memset<HAPI_NodeInfo>(NodeInfo, 0);

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo))
	{
		return false;
	}

	return true;
}


bool
FHoudiniMaterialObject::HapiGetParameterObjects(TArray<FHoudiniParameterObject>& ParameterObjects) const
{
	ParameterObjects.Empty();

	HAPI_NodeInfo NodeInfo;
	if(!HapiGetNodeInfo(NodeInfo))
	{
		return false;
	}

	TArray<HAPI_ParmInfo> ParmInfos;

	if(NodeInfo.parmCount > 0)
	{
		ParmInfos.SetNumUninitialized(NodeInfo.parmCount);
		if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParameters(FHoudiniEngine::Get().GetSession(), NodeId, &ParmInfos[0],
			0, NodeInfo.parmCount))
		{
			return false;
		}
	}

	for(int32 Idx = 0, Num = ParmInfos.Num(); Idx < Num; ++Idx)
	{
		const HAPI_ParmInfo& ParmInfo = ParmInfos[Idx];
		FHoudiniParameterObject HoudiniParameterObject(NodeId, ParmInfo);
		ParameterObjects.Add(HoudiniParameterObject);
	}

	return true;
}


bool
FHoudiniMaterialObject::HapiLocateParameterByName(const FString& Name,
	FHoudiniParameterObject& ResultHoudiniParameterObject) const
{
	TArray<FHoudiniParameterObject> HoudiniParameterObjects;
	if(!HapiGetParameterObjects(HoudiniParameterObjects))
	{
		return false;
	}

	for(int32 Idx = 0, Num = HoudiniParameterObjects.Num(); Idx < Num; ++Idx)
	{
		const FHoudiniParameterObject& HoudiniParameterObject = HoudiniParameterObjects[Idx];
		if(HoudiniParameterObject.HapiIsNameEqual(Name))
		{
			ResultHoudiniParameterObject = HoudiniParameterObject;
			return true;
		}
	}

	return false;
}


bool
FHoudiniMaterialObject::HapiLocateParameterByLabel(const FString& Label,
	FHoudiniParameterObject& ResultHoudiniParameterObject) const
{
	TArray<FHoudiniParameterObject> HoudiniParameterObjects;
	if(!HapiGetParameterObjects(HoudiniParameterObjects))
	{
		return false;
	}

	for(int32 Idx = 0, Num = HoudiniParameterObjects.Num(); Idx < Num; ++Idx)
	{
		const FHoudiniParameterObject& HoudiniParameterObject = HoudiniParameterObjects[Idx];
		if(HoudiniParameterObject.HapiIsLabelEqual(Label))
		{
			ResultHoudiniParameterObject = HoudiniParameterObject;
			return true;
		}
	}

	return false;
}


bool
FHoudiniMaterialObject::IsSubstance() const
{
	FHoudiniParameterObject HoudiniParameterObject;
	if(!HapiLocateParameterByLabel(HAPI_UNREAL_PARAM_SUBSTANCE_FILENAME, HoudiniParameterObject))
	{
		return false;
	}

	return true;
}
