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
#include "NaniteSceneProxy.h"
#include "HAPI/HAPI_Common.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "HoudiniPackageParams.h"
#include "DataLayer/DataLayerEditorSubsystem.h"

#if WITH_EDITOR
//#include "DataLayer/DataLayerEditorSubsystem.h"
#endif

void FHoudiniDataLayerUtils::ApplyDataLayersToActor(const FHoudiniPackageParams& Params, AActor* Actor, const TArray<FHoudiniDataLayer>& Layers)
{
#if HOUDINI_ENABLE_DATA_LAYERS
    UWorld* World = Actor->GetWorld();

	if (Layers.Num() == 0)
		return;

    AWorldDataLayers* WorldDataLayers = World->GetWorldDataLayers();
    if (!WorldDataLayers)
    {
        HOUDINI_LOG_ERROR(TEXT("Unable to apply Data Layer because this map is not world partitioned."));
        return;
    }

    for (auto& Layer : Layers)
    {
        AddActorToLayer(Params, WorldDataLayers, Actor, Layer);
    }
#endif
}

#if HOUDINI_ENABLE_DATA_LAYERS
void FHoudiniDataLayerUtils::AddActorToLayer(
	const FHoudiniPackageParams& Params,
	AWorldDataLayers* WorldDataLayers,
	AActor* Actor,
	const FHoudiniDataLayer& Layer)
{
	// Find the Data Layer Instance for this actor.
	UDataLayerInstance* TargetDataLayerInstance = nullptr;
	WorldDataLayers->ForEachDataLayer([&](UDataLayerInstance* DataLayer)
		{
			FString DataLayerName = DataLayer->GetDataLayerShortName();
			if (DataLayerName != Layer.Name)
				return true;

			TargetDataLayerInstance = DataLayer;
			return false;
		});

	if (!TargetDataLayerInstance)
	{
		if (Layer.bCreateIfNeeded)
		{
			UDataLayerAsset * DataLayerAsset = CreateDataLayerAsset(Params, Layer.Name);

			FDataLayerCreationParameters CreationParams;
			CreationParams.DataLayerAsset = DataLayerAsset;
			CreationParams.WorldDataLayers = WorldDataLayers;
			TargetDataLayerInstance = UDataLayerEditorSubsystem::Get()->CreateDataLayerInstance(CreationParams);

			if (!TargetDataLayerInstance)
			{
				HOUDINI_LOG_ERROR(TEXT("Could not create Data Layer: %s"), *Layer.Name);
				return;
			}
		}
		else
		{
			HOUDINI_LOG_WARNING(TEXT("Could not find Data Layer: %s. Set " HAPI_UNREAL_ATTRIB_CREATE_DATA_LAYERS
								" to create a default data layer asset."), *Layer.Name);
			return;
		}
	}

	TargetDataLayerInstance->AddActor(Actor);
}
#endif

TArray<FHoudiniDataLayer>
FHoudiniDataLayerUtils::GetDataLayers(HAPI_NodeId NodeId, HAPI_PartId PartId)
{
	TArray<FHoudiniDataLayer> Results;
#if HOUDINI_ENABLE_DATA_LAYERS
	// Get a list of all groups this part MAY be a member of.
	TArray<FString> RawGroupNames;
	bool bResult = FHoudiniEngineUtils::HapiGetGroupNames(
		NodeId, PartId,
		HAPI_GROUPTYPE_PRIM,
		false,
		RawGroupNames);

	// Check each group to see if we're a member.
	FString NamePrefix = TEXT(HOUDINI_DATA_LAYER_PREFIX);
	for (FString DataLayerName : RawGroupNames)
	{
		// Is a group that specifies an unreal data layer?
		if (!DataLayerName.StartsWith(NamePrefix))
			continue;

		// Is a member of the group?
		int32 PointGroupMembership = 0;

		FHoudiniEngineUtils::HapiGetGroupMembership(
			NodeId, PartId, HAPI_GROUPTYPE_PRIM,
			DataLayerName, PointGroupMembership);

		if (PointGroupMembership == 0)
			continue;

		FHoudiniDataLayer Layer;

		// Get name, removing the prefix.
		DataLayerName.RemoveFromStart(NamePrefix);
		Layer.Name = DataLayerName;

		// Create flag that indicates if we should create missing data layers.
		int32 CreateFlags = 0;
		FHoudiniEngineUtils::HapiGetFirstAttributeValueAsInteger(
				NodeId, PartId, 
				HAPI_UNREAL_ATTRIB_CREATE_DATA_LAYERS,
				HAPI_ATTROWNER_PRIM,
				CreateFlags);

		Layer.bCreateIfNeeded = (CreateFlags == 1);

		Results.Add(Layer);
	}
#endif
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

FHoudiniUnrealDataLayersCache FHoudiniUnrealDataLayersCache::MakeCache(UWorld* World)
{
	FHoudiniUnrealDataLayersCache Cache;
#if HOUDINI_ENABLE_DATA_LAYERS
	AWorldDataLayers* WorldDataLayers = World->GetWorldDataLayers();
	if (!IsValid(WorldDataLayers))
		return Cache;

	WorldDataLayers->ForEachDataLayer([&](class UDataLayerInstance* DataLayer)
		{
			FHoudiniUnrealDataLayerInfo Info;
			Info.Name = DataLayer->GetDataLayerShortName();

			int Id = Cache.DataLayerInfos.Num();
			Cache.DataLayerInfos.Add(Info);

			auto Actors = UDataLayerEditorSubsystem::Get()->GetActorsFromDataLayer(DataLayer);

			for (auto Actor : Actors)
			{
				if (!Cache.ActorDataLayers.Contains(Actor))
				{
					Cache.ActorDataLayers.Add(Actor, TArray<int>());
				}
				Cache.ActorDataLayers[Actor].Add(Id);
			}

			return true;
		});
#endif
	return Cache;

}


bool FHoudiniUnrealDataLayersCache::CreateHapiGroups(AActor* Actor, HAPI_NodeId NodeId, HAPI_PartId PartId)
{
	if (!ActorDataLayers.Contains(Actor))
		return true;

	TArray<FName> GroupNames;
	for (int LayerNumber : ActorDataLayers[Actor])
	{
		FString PrefixedName = FString(HOUDINI_DATA_LAYER_PREFIX) + FString("_") + DataLayerInfos[LayerNumber].Name;

		GroupNames.Add(FName(PrefixedName));
	}
	bool bSuccess = FHoudiniEngineUtils::CreateGroupsFromTags(NodeId, PartId, GroupNames);
	return bSuccess;
}
