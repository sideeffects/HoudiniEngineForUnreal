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
	NodeId(-1),
	MaterialId(-1)
{

}


FHoudiniMaterialObject::FHoudiniMaterialObject(const HAPI_MaterialInfo& MaterialInfo) :
	AssetId(MaterialInfo.assetId),
	NodeId(MaterialInfo.nodeId),
	MaterialId(MaterialInfo.id)
{

}


FHoudiniMaterialObject::FHoudiniMaterialObject(HAPI_AssetId InAssetId, HAPI_NodeId InNodeId,
	HAPI_MaterialId InMaterialId) :
	AssetId(InAssetId),
	NodeId(InNodeId),
	MaterialId(InMaterialId)
{

}

bool
FHoudiniMaterialObject::HapiGetNodeInfo(HAPI_NodeInfo& NodeInfo) const
{
	FMemory::Memset<HAPI_NodeInfo>(NodeInfo, 0);

	if(-1 == NodeId)
	{
		return false;
	}

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo))
	{
		return false;
	}

	return true;
}


bool
FHoudiniMaterialObject::HapiGetMaterialInfo(HAPI_MaterialInfo& MaterialInfo) const
{
	FMemory::Memset<HAPI_MaterialInfo>(MaterialInfo, 0);
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetMaterialInfo(FHoudiniEngine::Get().GetSession(), AssetId, MaterialId,
		&MaterialInfo))
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


bool
FHoudiniMaterialObject::HapiGetMaterialShopName(FString& ShopName) const
{
	HAPI_AssetInfo AssetInfo;
	FMemory::Memset<HAPI_AssetInfo>(AssetInfo, 0);

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo))
	{
		return false;
	}

	HAPI_NodeInfo AssetNodeInfo;
	FMemory::Memset<HAPI_NodeInfo>(AssetNodeInfo, 0);
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId,
		&AssetNodeInfo))
	{
		return false;
	}

	HAPI_NodeInfo MaterialNodeInfo;
	if(!HapiGetNodeInfo(MaterialNodeInfo))
	{
		return false;
	}

	FString AssetNodeName = TEXT("");
	FString MaterialNodeName = TEXT("");

	{
		FHoudiniEngineString HoudiniEngineString(AssetNodeInfo.internalNodePathSH);
		if(!HoudiniEngineString.ToFString(AssetNodeName))
		{
			return false;
		}
	}

	{
		FHoudiniEngineString HoudiniEngineString(MaterialNodeInfo.internalNodePathSH);
		if(!HoudiniEngineString.ToFString(MaterialNodeName))
		{
			return false;
		}
	}

	if(AssetNodeName.Len() > 0 && MaterialNodeName.Len() > 0)
	{
		// Remove AssetNodeName part from MaterialNodeName. Extra position is for separator.
		ShopName = MaterialNodeName.Mid(AssetNodeName.Len() + 1);
		return true;
	}

	return false;
}


bool
FHoudiniMaterialObject::HapiCheckMaterialExists() const
{
	HAPI_MaterialInfo MaterialInfo;
	if(!HapiGetMaterialInfo(MaterialInfo))
	{
		return false;
	}

	return MaterialInfo.exists;
}


bool
FHoudiniMaterialObject::HapiCheckMaterialChanged() const
{
	HAPI_MaterialInfo MaterialInfo;
	if(!HapiGetMaterialInfo(MaterialInfo))
	{
		return false;
	}

	return MaterialInfo.hasChanged;
}
