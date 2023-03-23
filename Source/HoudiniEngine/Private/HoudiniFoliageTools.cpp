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
#include "SSubobjectEditor.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "InstancedFoliageActor.h"
#include "AssetRegistry/AssetRegistryModule.h"

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

void FHoudiniFoliageTools::SpawnFoliageInstance(UWorld* InWorld, UFoliageType* Settings, const TArray<FFoliageInstance>& PlacedInstances, bool InRebuildFoliageTree)
{
	// This code is largely cribbed from SpawnFoliageInstance() in UE5's FoliageEdMode.cpp. It has UI specific functionality removed.

	TMap<AInstancedFoliageActor*, TArray<const FFoliageInstance*>> PerIFAPlacedInstances;
	const bool bSpawnInCurrentLevel = true;
	ULevel* CurrentLevel = InWorld->GetCurrentLevel();
	const bool bCreate = true;
	for (const FFoliageInstance& PlacedInstance : PlacedInstances)
	{
		ULevel* LevelHint = bSpawnInCurrentLevel ? CurrentLevel : PlacedInstance.BaseComponent ? PlacedInstance.BaseComponent->GetComponentLevel() : nullptr;
		if (AInstancedFoliageActor* IFA = AInstancedFoliageActor::Get(InWorld, bCreate, LevelHint, PlacedInstance.Location))
		{
			PerIFAPlacedInstances.FindOrAdd(IFA).Add(&PlacedInstance);
		}
	}

	for (const auto& PlacedLevelInstances : PerIFAPlacedInstances)
	{
		AInstancedFoliageActor* IFA = PlacedLevelInstances.Key;

		FFoliageInfo* Info = nullptr;
		UFoliageType* FoliageSettings = IFA->AddFoliageType(Settings, &Info);

		Info->AddInstances(FoliageSettings, PlacedLevelInstances.Value);

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
	for (TActorIterator<AInstancedFoliageActor> ActorIt(World, AInstancedFoliageActor::StaticClass()); ActorIt; ++ActorIt)
	{
	    AInstancedFoliageActor * IFA = *ActorIt;
        IFA->RemoveFoliageType(&FoliageType, 1);
	}
}


void FHoudiniFoliageTools::RemoveInstancesFromWorld(UWorld* World, UFoliageType* FoliageType)
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

