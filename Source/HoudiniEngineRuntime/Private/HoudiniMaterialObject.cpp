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
#include "HoudiniMaterialObjectVersion.h"
#include "HoudiniParameterObject.h"
#include "HoudiniApi.h"
#include "HoudiniEngineString.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetComponent.h"


FHoudiniMaterialObject::FHoudiniMaterialObject() :
	AssetId(-1),
	NodeId(-1),
	MaterialId(-1),
	HoudiniMaterialObjectFlagsPacked(0u),
	HoudiniMaterialObjectVersion(VER_HOUDINI_ENGINE_MATERIALOBJECT_BASE)
{

}


FHoudiniMaterialObject::FHoudiniMaterialObject(const HAPI_MaterialInfo& MaterialInfo) :
	AssetId(MaterialInfo.assetId),
	NodeId(MaterialInfo.nodeId),
	MaterialId(MaterialInfo.id),
	HoudiniMaterialObjectFlagsPacked(0u),
	HoudiniMaterialObjectVersion(VER_HOUDINI_ENGINE_MATERIALOBJECT_BASE)
{

}


FHoudiniMaterialObject::FHoudiniMaterialObject(HAPI_AssetId InAssetId, HAPI_NodeId InNodeId,
	HAPI_MaterialId InMaterialId) :
	AssetId(InAssetId),
	NodeId(InNodeId),
	MaterialId(InMaterialId),
	HoudiniMaterialObjectFlagsPacked(0u),
	HoudiniMaterialObjectVersion(VER_HOUDINI_ENGINE_MATERIALOBJECT_BASE)
{

}


FHoudiniMaterialObject::FHoudiniMaterialObject(const FHoudiniMaterialObject& HoudiniMaterialObject) :
	AssetId(HoudiniMaterialObject.AssetId),
	NodeId(HoudiniMaterialObject.NodeId),
	MaterialId(HoudiniMaterialObject.MaterialId),
	HoudiniMaterialObjectFlagsPacked(0u),
	HoudiniMaterialObjectVersion(HoudiniMaterialObject.HoudiniMaterialObjectVersion)
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


bool
FHoudiniMaterialObject::HapiIsMaterialTransparent() const
{
	FHoudiniParameterObject ResultHoudiniParameterObject;
	if(!HapiLocateParameterByName(TEXT(HAPI_UNREAL_PARAM_ALPHA), ResultHoudiniParameterObject))
	{
		return false;
	}

	float Alpha = 1.0f;
	if(!ResultHoudiniParameterObject.HapiGetValue(Alpha))
	{
		return false;
	}

	return Alpha < HAPI_UNREAL_ALPHA_THRESHOLD;
}


UPackage*
FHoudiniMaterialObject::CreateMaterialPackage(UHoudiniAssetComponent* HoudiniAssetComponent, FString& MaterialName,
	bool bBake)
{
	UPackage* PackageNew = nullptr;

#if WITH_EDITOR

	if(!HoudiniAssetComponent)
	{
		return nullptr;
	}

	UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
	if(!HoudiniAsset)
	{
		return nullptr;
	}

	HAPI_MaterialInfo MaterialInfo;
	if(!HapiGetMaterialInfo(MaterialInfo))
	{
		return nullptr;
	}

	FString MaterialDescriptor = TEXT("");
	if(bBake)
	{
		MaterialDescriptor = HoudiniAsset->GetName() + TEXT("_material_") + FString::FromInt(MaterialInfo.id)
			+ TEXT("_");
	}
	else
	{
		MaterialDescriptor = HoudiniAsset->GetName() + TEXT("_") + FString::FromInt(MaterialInfo.id) + TEXT("_");
	}

	FGuid BakeGUID = FGuid::NewGuid();
	const FGuid& ComponentGUID = HoudiniAssetComponent->GetComponentGuid();
	FString PackageName;

	// We only want half of generated guid string.
	FString ComponentGUIDString = ComponentGUID.ToString().Left(12);

	while(true)
	{
		if(!BakeGUID.IsValid())
		{
			BakeGUID = FGuid::NewGuid();
		}

		// We only want half of generated guid string.
		FString BakeGUIDString = BakeGUID.ToString().Left(12);

		// Generate material name.
		MaterialName = MaterialDescriptor + BakeGUIDString;

		if(bBake)
		{
			// Generate unique package name.
			PackageName = FPackageName::GetLongPackagePath(HoudiniAsset->GetOutermost()->GetName()) + TEXT("/")
				+ MaterialName;
		}
		else
		{
			// Generate unique package name.
			PackageName = FPackageName::GetLongPackagePath(HoudiniAsset->GetOuter()->GetName()) + TEXT("/") +
				HoudiniAsset->GetName() + TEXT("_") + ComponentGUIDString + TEXT("/") + MaterialName;
		}

		// Sanitize package name.
		PackageName = PackageTools::SanitizePackageName(PackageName);

		UObject* OuterPackage = nullptr;

		if(!bBake)
		{
			// If we are not baking, then use outermost package, since objects within our package need to be visible
			// to external operations, such as copy paste.
			OuterPackage = HoudiniAssetComponent->GetOutermost();
		}

		// See if package exists, if it does, we need to regenerate the name.
		UPackage* Package = FindPackage(OuterPackage, *PackageName);

		if(Package)
		{
			if(!bBake)
			{
				// Package does exist, there's a collision, we need to generate a new name.
				BakeGUID.Invalidate();
			}
		}
		else
		{
			// Create actual package.
			PackageNew = CreatePackage(OuterPackage, *PackageName);
			break;
		}
	}

#endif

	return PackageNew;
}
