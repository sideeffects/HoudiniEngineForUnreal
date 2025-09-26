/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "HoudiniDataLayerUtils.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "LevelEditorMenuContext.h"
//#include "NaniteSceneProxy.h"
#include "HoudiniEngineAttributes.h"
#include "HAPI/HAPI_Common.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "HoudiniPackageParams.h"
#include "DataLayer/DataLayerEditorSubsystem.h"
#include "Landscape.h"
#include "LandscapeStreamingProxy.h"

#if HOUDINI_ENABLE_DATA_LAYERS
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#endif

#if WITH_EDITOR
//#include "DataLayer/DataLayerEditorSubsystem.h"
#endif

void FHoudiniDataLayerUtils::ApplyDataLayersToActor(AActor* Actor, TArray<FHoudiniDataLayer> & DataLayers, TMap<FString, UDataLayerInstance*>& DataLayerLookup)
{
#if HOUDINI_ENABLE_DATA_LAYERS

    for (auto& Layer : DataLayers)
    {
		UDataLayerInstance** DataLayerInstance = DataLayerLookup.Find(Layer.Name);
		if(DataLayerInstance)
			(*DataLayerInstance)->AddActor(Actor);
    }

	if (ALandscape* Landscape = Cast<ALandscape>(Actor))
	{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7
		TArray<TWeakObjectPtr<ALandscapeStreamingProxy>> Proxies = Landscape->GetLandscapeInfo()->GetSortedStreamingProxies();
#elif ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
		TArray<TWeakObjectPtr<ALandscapeStreamingProxy>> Proxies = Landscape->GetLandscapeInfo()->StreamingProxies;
#else
		TArray<TObjectPtr<ALandscapeStreamingProxy>> Proxies = Proxy->GetLandscapeInfo()->Proxies;
#endif

		for(TWeakObjectPtr<ALandscapeStreamingProxy> Child : Proxies)
		{
			ALandscapeStreamingProxy* LandscapeProxy = Child.Get();
			if (LandscapeProxy)
			{
				for(auto& Layer : DataLayers)
				{
					UDataLayerInstance** DataLayerInstance = DataLayerLookup.Find(Layer.Name);
					if(DataLayerInstance)
						(*DataLayerInstance)->AddActor(LandscapeProxy);
				}
			}
		}
	}
#endif
}

#if HOUDINI_ENABLE_DATA_LAYERS
UDataLayerInstance * FHoudiniDataLayerUtils::FindOrCreateDataLayerInstance(
	const FHoudiniPackageParams& Params, 
	AWorldDataLayers* WorldDataLayers, 
	const FHoudiniDataLayer& Layer)
{
	// Find the Data Layer Instance for this actor.
	UDataLayerInstance* TargetDataLayerInstance = nullptr;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
	WorldDataLayers->ForEachDataLayerInstance([&](UDataLayerInstance* DataLayer)
#else
	WorldDataLayers->ForEachDataLayer([&](UDataLayerInstance* DataLayer)
#endif
		{
			FString DataLayerName = DataLayer->GetDataLayerShortName();
			if(DataLayerName != Layer.Name)
				return true;

			TargetDataLayerInstance = DataLayer;
			return false;
		});

	if(!TargetDataLayerInstance)
	{
		if(Layer.bCreateIfNeeded)
		{
			UDataLayerAsset* DataLayerAsset = CreateDataLayerAsset(Params, Layer.Name);

			FDataLayerCreationParameters CreationParams;
			CreationParams.DataLayerAsset = DataLayerAsset;
			CreationParams.WorldDataLayers = WorldDataLayers;
			TargetDataLayerInstance = UDataLayerEditorSubsystem::Get()->CreateDataLayerInstance(CreationParams);

			if(!TargetDataLayerInstance)
			{
				HOUDINI_LOG_ERROR(TEXT("Could not create Data Layer: %s"), *Layer.Name);
				return nullptr;
			}
		}
		else
		{
			HOUDINI_LOG_WARNING(TEXT("Could not find Data Layer: %s. Set " HAPI_UNREAL_ATTRIB_CREATE_DATA_LAYERS
				" to create a default data layer asset."), *Layer.Name);
			return nullptr;
		}
	}
	return TargetDataLayerInstance;
}
#endif

#if HOUDINI_ENABLE_DATA_LAYERS
void FHoudiniDataLayerUtils::AddActorToLayer(
	const FHoudiniPackageParams& Params,
	AWorldDataLayers* WorldDataLayers,
	AActor* Actor,
	const FHoudiniDataLayer& Layer)
{
	UDataLayerInstance* DataLayerInstance = FindOrCreateDataLayerInstance(Params, WorldDataLayers, Layer);
	if (DataLayerInstance)
		DataLayerInstance->AddActor(Actor);
}
#endif

TArray<FHoudiniDataLayer>
FHoudiniDataLayerUtils::GetDataLayers(HAPI_NodeId NodeId, HAPI_PartId PartId, HAPI_GroupType GroupType, int Index)
{
	TArray<FHoudiniDataLayer> Results;
#if HOUDINI_ENABLE_DATA_LAYERS
	// Get a list of all groups this part MAY be a member of.
	TArray<FString> RawGroupNames;
	bool bResult = FHoudiniEngineUtils::HapiGetGroupNames(NodeId, PartId, GroupType, false, RawGroupNames);

	// Check each group to see if we're a member.
	FString NamePrefix = TEXT(HOUDINI_DATA_LAYER_PREFIX);
	for(FString DataLayerName : RawGroupNames)
	{
		// Is a group that specifies an unreal data layer?
		if(!DataLayerName.StartsWith(NamePrefix))
			continue;

		// Is a member of the group?
		int32 GroupMembership = 0;

		FHoudiniEngineUtils::HapiGetGroupMembership(NodeId, PartId, GroupType, DataLayerName, GroupMembership, Index, 1);

		if(GroupMembership == 0)
			continue;

		FHoudiniDataLayer Layer;

		// Get name, removing the prefix.
		DataLayerName.RemoveFromStart(NamePrefix);
		Layer.Name = DataLayerName;

		// Create flag that indicates if we should create missing data layers.
		int32 CreateFlags = 0;

		FHoudiniHapiAccessor Accessor;
		Accessor.Init(NodeId, PartId, HAPI_UNREAL_ATTRIB_CREATE_DATA_LAYERS);
		bool bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, CreateFlags);

		Layer.bCreateIfNeeded = (CreateFlags == 1);

		Results.Add(Layer);
	}
#endif
	return Results;
}

TArray<FHoudiniDataLayer>
FHoudiniDataLayerUtils::GetDataLayers(HAPI_NodeId NodeId, HAPI_PartId PartId)
{
	TArray<FHoudiniDataLayer> Results = GetDataLayers(NodeId, PartId, HAPI_GroupType::HAPI_GROUPTYPE_PRIM, 0);
	if (Results.IsEmpty())
		Results = GetDataLayers(NodeId, PartId, HAPI_GroupType::HAPI_GROUPTYPE_POINT, 0);
	return Results;
}

#if HOUDINI_ENABLE_DATA_LAYERS
UDataLayerAsset*
FHoudiniDataLayerUtils::CreateDataLayerAsset(const FHoudiniPackageParams& Params, const FString & LayerName)
{
	// When creating a new data layer asset the name is taken from the asset package name, so we shouldn't
	// append the HDA name of node/part labels. Also, we want the data layer to be available to other outputs
	// in the same, or even other, HDA names, so keeping the name simple makes sense.
	//
	// Really we're just Package Params to get the back folder.

	FHoudiniPackageParams DataLayerParams = Params;
	DataLayerParams.ObjectName = LayerName;
	UDataLayerAsset* Result = DataLayerParams.CreateObjectAndPackage<UDataLayerAsset>();
	return Result;

}
#endif


TArray<FHoudiniUnrealDataLayerInfo>
FHoudiniDataLayerUtils::GetDataLayerInfoForActor(AActor* Actor)
{
	TArray<FHoudiniUnrealDataLayerInfo> Results;

#if HOUDINI_ENABLE_DATA_LAYERS
	TArray<const UDataLayerInstance*> DataLayerInstances =  Actor->GetDataLayerInstances();

	for(const UDataLayerInstance * DataLayerInstance : DataLayerInstances)
	{
		FHoudiniUnrealDataLayerInfo Info = {};
		Info.Name = DataLayerInstance->GetDataLayerShortName();
		Results.Add(Info);
	}
#endif

	return Results;

}

HAPI_NodeId
FHoudiniDataLayerUtils::AddGroupsFromDataLayers(AActor* Actor, HAPI_NodeId ParentNodeId, HAPI_NodeId InputNodeId)
{
	HAPI_NodeId VexNodeId;

	// Create a group node.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(),
			ParentNodeId,
			"attribwrangle",
			"data_layers",
			false,
			&VexNodeId),
		-1);

	// Hook the new node up to the input node.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(), VexNodeId, 0, InputNodeId, 0), false);

	SetVexCode(VexNodeId, Actor);

	return VexNodeId;
}

bool
FHoudiniDataLayerUtils::SetVexCode(HAPI_NodeId VexNodeId, AActor* Actor)
{
	auto DataLayers = FHoudiniDataLayerUtils::GetDataLayerInfoForActor(Actor);

	FString VexCode;

	for (auto& DataLayer : DataLayers)
	{
		FString PrefixedName = FString(HOUDINI_DATA_LAYER_PREFIX) + DataLayer.Name;

		const FString VexLine = FString::Format(TEXT("setprimgroup(0,\"{0}\", @primnum,1);\n"), { PrefixedName });
		VexCode += VexLine;
	}
	// Set the wrangle's class to prims
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), VexNodeId, "class", 0, 1), false);

	// Set the snippet parameter to the VEXpression.
	HAPI_ParmInfo ParmInfo;
	HAPI_ParmId ParmId = FHoudiniEngineUtils::HapiFindParameterByName(VexNodeId, "snippet", ParmInfo);
	if (ParmId != -1)
	{
		FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), VexNodeId, H_TCHAR_TO_UTF8(*VexCode), ParmId, 0);
	}
	else
	{
		HOUDINI_LOG_WARNING(TEXT("Invalid Parameter: %s"), *FHoudiniEngineUtils::GetErrorDescription());
	}
	return true;
}
