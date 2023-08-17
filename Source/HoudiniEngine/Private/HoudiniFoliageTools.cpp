/*
* Copyright (c) <2022> Side Effects Software Inc.
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

#include "HoudiniFoliageTools.h"
#include "HoudiniEngineUtils.h"
#include "UnrealObjectInputRuntimeTypes.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetActor.h"
#include "HoudiniInput.h"
#include "HoudiniAssetComponent.h"
#include "Editor/UnrealEdEngine.h"
#include "Async/Async.h"
#include "Components/BrushComponent.h"
#include "Components/ModelComponent.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "InstancedFoliageActor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Spatial/PointHashGrid3.h"

#if WITH_EDITOR
#include "EditorModeManager.h"
#include "EditorModes.h"
#endif

UFoliageType*
FHoudiniFoliageTools::CreateFoliageType(const FHoudiniPackageParams& Params, int OutputIndex, UStaticMesh* InstancedStaticMesh)
{
	UFoliageType* FoliageType = nullptr;

	// With world partition, Foliage Types must be assets. Create a package and save it.
	FHoudiniPackageParams FoliageParams = Params;
	FoliageParams.ObjectName = FString::Printf(TEXT("%s_%d_%s"), *FoliageParams.HoudiniAssetName, OutputIndex + 1, TEXT("foliage_type"));
	if (UFoliageType_InstancedStaticMesh* InstancedMeshFoliageType = FoliageParams.CreateObjectAndPackage<UFoliageType_InstancedStaticMesh>())
	{
		InstancedMeshFoliageType->SetStaticMesh(InstancedStaticMesh);
		InstancedMeshFoliageType->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(InstancedMeshFoliageType);
		FoliageType = InstancedMeshFoliageType;
	}
	return FoliageType;

}

// Duplicate foliage asset.
UFoliageType* FHoudiniFoliageTools::DuplicateFoliageType(const FHoudiniPackageParams& Params, int OutputIndex, UFoliageType* OrigFoliageType)
{
	UFoliageType* FoliageType = nullptr;

	FHoudiniPackageParams FoliageParams = Params;
	FoliageParams.ObjectName = FString::Printf(TEXT("%s_%s_%d_%s"), *FoliageParams.HoudiniAssetName, *OrigFoliageType->GetName(), OutputIndex + 1, TEXT("foliage_type"));

	UFoliageType_InstancedStaticMesh * FTISM = Cast<UFoliageType_InstancedStaticMesh>(OrigFoliageType);

	if (UFoliageType_InstancedStaticMesh* InstancedMeshFoliageType = FoliageParams.CreateObjectAndPackage<UFoliageType_InstancedStaticMesh>(FTISM))
	{
		InstancedMeshFoliageType->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(InstancedMeshFoliageType);
		FoliageType = InstancedMeshFoliageType;
	}
	return FoliageType;
}


UFoliageType*
FHoudiniFoliageTools::GetFoliageType(const ULevel* DesiredLevel, const UStaticMesh* InstancedStaticMesh)
{
	TArray<UFoliageType*> FoliageTypes = FHoudiniFoliageTools::GetFoliageTypes(DesiredLevel, InstancedStaticMesh);
	if (FoliageTypes.Num() > 1)
	{
		HOUDINI_LOG_WARNING(TEXT("More than one Folage Type found for instanced static mesh: %s"), *InstancedStaticMesh->GetName()); 
	}

	return (FoliageTypes.Num() > 0) ? FoliageTypes[0] : nullptr;
}

TArray<UFoliageType*> FHoudiniFoliageTools::GetFoliageTypes(AInstancedFoliageActor* IFA)
{
	TArray<UFoliageType*> Results;

	const TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>& FoliageInfos = IFA->GetFoliageInfos();

	for (const auto& FoliageIt : FoliageInfos)
	{
		UFoliageType* FoliageType = FoliageIt.Key;
		Results.Add(FoliageType);
	}
	return Results;
}

TArray<UFoliageType*>
FHoudiniFoliageTools::GetFoliageTypes(const ULevel* DesiredLevel, const UStaticMesh* InstancedStaticMesh)
{
	TArray<UFoliageType*> Results;

	// Iterate through all AInstancedFoliageActor actors in the world and combine their foliage types
	// into a single array.

	for (TActorIterator<AActor> It(DesiredLevel->GetWorld(), AInstancedFoliageActor::StaticClass()); It; ++It)
	{
		AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>(*It);

		TArray<const UFoliageType*> FoliageTypes;
		IFA->GetAllFoliageTypesForSource(InstancedStaticMesh, FoliageTypes);

		// GetAllFoliageTypesForSource() returns an array of const UFoliageTypes while the rest of the Unreal
		// API is pretty const ignorant for foliage types. So remove consting.

		for(auto FoliageType : FoliageTypes)
			Results.Add(const_cast<UFoliageType*>(FoliageType));
	}
	return Results;
}

TArray<FFoliageInfo*> FHoudiniFoliageTools::GetAllFoliageInfo(UWorld * World, UFoliageType* FoliageType)
{
	TArray<FFoliageInfo*> Results;

	for (TActorIterator<AActor> It(World, AInstancedFoliageActor::StaticClass()); It; ++It)
	{
		AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>(*It);

		const TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>& FoliageInfos = IFA->GetFoliageInfos();

		for (const auto& FoliageIt : FoliageInfos)
		{
			if (FoliageIt.Key == FoliageType)
			{
			    const FFoliageInfo& FoliageInfo = *FoliageIt.Value;
			    Results.Add(const_cast<FFoliageInfo*>(&FoliageInfo));
			}

		}
	}
    return Results;
}

TArray<FFoliageInstance> FHoudiniFoliageTools::GetAllFoliageInstances(UWorld* InWorld, UFoliageType* FoliageType)
{
	TArray<FFoliageInstance> Results;

	for (TActorIterator<AActor> It(InWorld, AInstancedFoliageActor::StaticClass()); It; ++It)
	{
		AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>(*It);

        auto InstanceMap = IFA->GetAllInstancesFoliageType();
		for(auto & InstanceIt : InstanceMap)
		{
		    FFoliageInfo * FoliageInfo = InstanceIt.Value;
			if (InstanceIt.Key == FoliageType)
			    Results.Append(FoliageInfo->Instances);

		}
	}
    return Results;
}

void FHoudiniFoliageTools::SpawnFoliageInstances(UWorld* InWorld, UFoliageType* Settings, const TArray<FFoliageInstance>& InstancesToPlace, const TArray<FFoliageAttachmentInfo>& AttachmentInfo)
{
	// This code is largely cribbed from SpawnFoliageInstance() in UE5's FoliageEdMode.cpp. It has UI specific functionality removed.

	TMap<AInstancedFoliageActor*, TArray<int>> PerIFAInstances;
	const bool bSpawnInCurrentLevel = true;
	ULevel* CurrentLevel = InWorld->GetCurrentLevel();
	const bool bCreate = true;
	for (int Index = 0; Index < InstancesToPlace.Num(); Index++)
	{
		const FFoliageInstance& PlacedInstance = InstancesToPlace[Index];
		ULevel* LevelHint = bSpawnInCurrentLevel ? CurrentLevel : PlacedInstance.BaseComponent ? PlacedInstance.BaseComponent->GetComponentLevel() : nullptr;
		if (AInstancedFoliageActor* IFA = AInstancedFoliageActor::Get(InWorld, bCreate, LevelHint, PlacedInstance.Location))
		{
			PerIFAInstances.FindOrAdd(IFA).Add(Index);
		}
	}

	for (const auto& PlacedLevelInstances : PerIFAInstances)
	{
		AInstancedFoliageActor* IFA = PlacedLevelInstances.Key;

		FFoliageInfo* Info = nullptr;
		UFoliageType* FoliageSettings = IFA->AddFoliageType(Settings, &Info);


		for(int InstanceIndex = 0; InstanceIndex < PlacedLevelInstances.Value.Num(); InstanceIndex++)
		{
			FFoliageInstance Instance = InstancesToPlace[PlacedLevelInstances.Value[InstanceIndex]];
			if (AttachmentInfo.Num() > 0)
			{
				SetInstanceAttachment(IFA, Info, FoliageSettings, Instance, AttachmentInfo[InstanceIndex]);
			}
			Info->AddInstance(FoliageSettings, Instance);
		}
		Info->Refresh(false, true);

	}

	TArray<FFoliageInfo*> FoliageInfos = FHoudiniFoliageTools::GetAllFoliageInfo(InWorld, Settings);
	for(auto & FoliageInfo : FoliageInfos)
	{
	    if (FoliageInfo != nullptr)
		{
		    FoliageInfo->Refresh(true, false);
		}
	}
}

void
FHoudiniFoliageTools::RemoveFoliageTypeFromWorld(UWorld* World, UFoliageType* FoliageType)
{
	// Avoid needlessly dirtying every FoliageInstanceActor by calling RemoveFoliageType() with null
	if (!IsValid(FoliageType))
		return;

	for (TActorIterator<AInstancedFoliageActor> ActorIt(World, AInstancedFoliageActor::StaticClass()); ActorIt; ++ActorIt)
	{
	    AInstancedFoliageActor * IFA = *ActorIt;

		// Check to see if the IFA actually contains the FoliageType, then remove it. Do this so we
		// don't dirty IFAs needlessly because Unreal doesn't check for this and always marks the actor as dirty.
		FFoliageInfo* Info = IFA->FindInfo(FoliageType);
		if (Info != nullptr)
		{
			IFA->RemoveFoliageType(&FoliageType, 1);
		}
	}
}


void
FHoudiniFoliageTools::RemoveInstancesFromWorld(UWorld* World, UFoliageType* FoliageType)
{
	TArray<FFoliageInfo*> FoliageInfos = FHoudiniFoliageTools::GetAllFoliageInfo(World, FoliageType);
	for (auto& FoliageInfo : FoliageInfos)
	{
		if (FoliageInfo != nullptr)
		{
			TArray<int> InstancesToRemove;
			InstancesToRemove.SetNum(FoliageInfo->Instances.Num());
			for(int Index = 0; Index < FoliageInfo->Instances.Num(); Index++)
			{
				InstancesToRemove[Index] = Index;
			}
			FoliageInfo->RemoveInstances(InstancesToRemove, true);
		}
	}
}

void
FHoudiniFoliageTools::RemoveFoliageInstances(UWorld* World, UFoliageType* FoliageType, const TArray<FVector3d>& Positions)
{
	const float Spacing = 1.0f;
	// Create a spatial hash for fast look up of positions.
	UE::Geometry::TPointHashGrid3d< int> SpatialHash(Spacing, -1);
	for (int Index = 0; Index < Positions.Num(); ++Index)
		SpatialHash.InsertPoint(Index, Positions[Index]);


	TArray<FFoliageInfo*> FoliageInfos = FHoudiniFoliageTools::GetAllFoliageInfo(World, FoliageType);
	for (auto& FoliageInfo : FoliageInfos)
	{
		if (FoliageInfo == nullptr) continue;

		// Query all instances in the Foliage Type, see if they are close to one in the set we are trying to remove

		TArray<int> InstancesToRemove;
		for (int Index = 0; Index < FoliageInfo->Instances.Num(); Index++)
		{
			TPair<int, double> Result = SpatialHash.FindNearestInRadius(FoliageInfo->Instances[Index].Location, Spacing, [&](int PosIndex)
				{
					return FVector3d::DistSquared(FoliageInfo->Instances[Index].Location, Positions[PosIndex]);
				});

			if (Result.Key != -1)
			{
				// Remove this instance of foliage.
				InstancesToRemove.Add(Index);

				// Remove the matched point from the spatial hash. This means we will only remove one instance
				// for each point.
				SpatialHash.RemovePoint(Result.Value, Positions[Result.Value]);
			}

		}
		FoliageInfo->RemoveInstances(InstancesToRemove, true);
	}
}

void
FHoudiniFoliageTools::SetInstanceAttachment(AInstancedFoliageActor * IFA, FFoliageInfo * FoliageInfo, UFoliageType * FoliageType, FFoliageInstance& FoliageInstance, const FFoliageAttachmentInfo& AttachmentInfo)
{
	UWorld * World = IFA->GetWorld();

	if (AttachmentInfo.Type == EFoliageAttachmentType::None)
	{
		// Use whatever was in the instance - this is used when baking.
		return;
	}

	TFunction<bool(const UPrimitiveComponent*)> FilterFunc = [AttachmentInfo] (const UPrimitiveComponent* Component)
	{
		switch(AttachmentInfo.Type)
		{
		case EFoliageAttachmentType::All:
			return true;

		case EFoliageAttachmentType::LandscapeOnly:
			return Component->IsA<ULandscapeHeightfieldCollisionComponent>();

		default: 
			return false;
		}
	};

	FVector TraceStart = FoliageInstance.Location + FVector(0.f, 0.f, AttachmentInfo.Distance);
	FVector TraceEnd = FoliageInstance.Location - FVector(0.f, 0.f, AttachmentInfo.Distance);
	FHitResult Hit;
	static FName TraceName = FName("HoudiniFoliageTrace");
	if (AInstancedFoliageActor::FoliageTrace(World, Hit, FDesiredFoliageInstance(TraceStart, TraceEnd, FoliageType), TraceName, false, FilterFunc, false))
	{
		UPrimitiveComponent* BaseComponent = Hit.Component.Get();
		if (!IsValid(BaseComponent) || BaseComponent->GetComponentLevel() != IFA->GetLevel())
		{
			return;
		}

		if (UModelComponent* ModelComponent = Cast<UModelComponent>(BaseComponent))
		{
			if (ABrush* BrushActor = ModelComponent->GetModel()->FindBrush((FVector3f)Hit.Location))
			{
				BaseComponent = BrushActor->GetBrushComponent();
			}
		}

		FoliageInstance.BaseId = IFA->InstanceBaseCache.AddInstanceBaseId(FoliageInfo->ShouldAttachToBaseComponent() ? BaseComponent : nullptr);
		if (FoliageInstance.BaseId == FFoliageInstanceBaseCache::InvalidBaseId)
		{
			FoliageInstance.BaseComponent = nullptr;
		}
		else
		{
			FoliageInstance.BaseComponent = BaseComponent;
		}

		FoliageInstance.Location = Hit.Location;
		FoliageInstance.ZOffset = 0.f;

#if 0
		// For now, we do not support this. Possibly this could be controlled by an attribute or setting in the future
		if (FoliageInstance.Flags & FOLIAGE_AlignToNormal)
		{
			FoliageInstance.Rotation = FoliageInstance.PreAlignRotation;
			FoliageInstance.AlignToNormal(Hit.Normal, FoliageType->AlignMaxAngle);
		}
#endif
	}
}

TArray<FFoliageAttachmentInfo> FHoudiniFoliageTools::GetAttachmentInfo(int GeoId, int PartId, int Count)
{
	TArray<FFoliageAttachmentInfo> Result;
	Result.SetNum(Count);

	HAPI_AttributeInfo AttributeInfo;
	TArray<int32> IntData;
	if (!FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_FOLIAGE_ATTACHMENT_TYPE,
		AttributeInfo, IntData, 0, HAPI_ATTROWNER_POINT, 0, Count))
	{
		return Result;
	}

	if (!AttributeInfo.exists || AttributeInfo.count <= 0 || IntData.Num() != Count)
		return Result;

	for(int Index = 0; Index < IntData.Num(); Index++)
	{
		switch (IntData[Index])
		{
		case 0:	Result[Index].Type = EFoliageAttachmentType::None; break;
		case 1:	Result[Index].Type = EFoliageAttachmentType::All; break;
		case 2: Result[Index].Type = EFoliageAttachmentType::LandscapeOnly; break;
		default: break;
		}
	}

	// Get (optional) attachment distance
	TArray<float> FloatData;
	if (!FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_FOLIAGE_ATTACHMENT_DISTANCE,
		AttributeInfo, FloatData, 0, HAPI_ATTROWNER_POINT, 0, Count))
	{
		return Result;
	}

	for (int Index = 0; Index < FloatData.Num(); Index++)
	{
		Result[Index].Distance = FloatData[Index];
	}

	return Result;
}
