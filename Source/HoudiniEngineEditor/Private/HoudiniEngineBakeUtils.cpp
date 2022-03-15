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

#include "HoudiniEngineBakeUtils.h"

#include "HoudiniEngineEditorPrivatePCH.h"

#include "HoudiniEngineUtils.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniOutput.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniPackageParams.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniEngineUtils.h"
#include "UnrealLandscapeTranslator.h"
#include "HoudiniInstanceTranslator.h"
#include "HoudiniInstancedActorComponent.h"
#include "HoudiniMeshSplitInstancerComponent.h"
#include "HoudiniPDGAssetLink.h"
#include "HoudiniStringResolver.h"
#include "HoudiniEngineCommands.h"
#include "HoudiniEngineRuntimeUtils.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "RawMesh.h"
#include "UObject/Package.h"
#include "PackageTools.h"
#include "UObject/MetaData.h"
#include "AssetRegistryModule.h"
#include "AssetSelection.h"
#include "Materials/Material.h"
#include "LandscapeProxy.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeInfo.h"
#include "Factories/WorldFactory.h"
#include "AssetToolsModule.h"
#include "InstancedFoliageActor.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "ActorFactories/ActorFactoryStaticMesh.h"
#include "ActorFactories/ActorFactoryEmptyActor.h"
#include "BusyCursor.h"
#include "Editor.h"
#include "EditorLevelUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "FileHelpers.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngine.h"
#include "HoudiniGeometryCollectionTranslator.h"
#include "HoudiniLandscapeTranslator.h"
#include "HoudiniMeshTranslator.h"
#include "HoudiniOutputTranslator.h"
#include "Editor/EditorEngine.h"
#include "Factories/BlueprintFactory.h"
#include "Engine/SimpleConstructionScript.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "LandscapeEdit.h"
#include "LandscapeInfoMap.h"
#include "ActorFactories/ActorFactoryClass.h"
#include "Containers/UnrealString.h"
#include "Components/AudioComponent.h"
#include "Engine/LevelBounds.h"
#include "Engine/WorldComposition.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MaterialEditor/Public/MaterialEditingLibrary.h"
#include "MaterialGraph/MaterialGraph.h"
#include "Materials/MaterialInstance.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"
#include "Math/Box.h"
#include "Misc/ScopedSlowTask.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "Kismet2/ComponentEditorUtils.h"

#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionActor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "WorldPartition/WorldPartition.h"

HOUDINI_BAKING_DEFINE_LOG_CATEGORY();

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

FHoudiniEngineBakedActor::FHoudiniEngineBakedActor()
	: Actor(nullptr)
	, OutputIndex(INDEX_NONE)
	, OutputObjectIdentifier()
	, ActorBakeName(NAME_None)
	, BakedObject(nullptr)
	, SourceObject(nullptr)
	, BakeFolderPath()
	, bInstancerOutput(false)
	, bPostBakeProcessPostponed(false)
{
}

FHoudiniEngineBakedActor::FHoudiniEngineBakedActor(
	AActor* InActor,
	FName InActorBakeName,
	FName InWorldOutlinerFolder,
	int32 InOutputIndex,
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	UObject* InBakedObject,
	UObject* InSourceObject,
	UObject* InBakedComponent,
	const FString& InBakeFolderPath,
	const FHoudiniPackageParams& InBakedObjectPackageParams)
	: Actor(InActor)
	, OutputIndex(InOutputIndex)
	, OutputObjectIdentifier(InOutputObjectIdentifier)
	, ActorBakeName(InActorBakeName)
	, WorldOutlinerFolder(InWorldOutlinerFolder)
	, BakedObject(InBakedObject)
	, SourceObject(InSourceObject)
	, BakedComponent(InBakedComponent)
	, BakeFolderPath(InBakeFolderPath)
	, BakedObjectPackageParams(InBakedObjectPackageParams)
	, bInstancerOutput(false)
	, bPostBakeProcessPostponed(false)
{
}

bool
FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(
	UHoudiniAssetComponent* InHACToBake,
	bool bInReplacePreviousBake,
	EHoudiniEngineBakeOption InBakeOption,
	bool bInRemoveHACOutputOnSuccess,
	bool bInRecenterBakedActors)
{
	if (!IsValid(InHACToBake))
		return false;

	// Handle proxies: if the output has any current proxies, first refine them
	bool bHACNeedsToReCook;
	if (!CheckForAndRefineHoudiniProxyMesh(InHACToBake, bInReplacePreviousBake, InBakeOption, bInRemoveHACOutputOnSuccess, bInRecenterBakedActors, bHACNeedsToReCook))
	{
		// Either the component is invalid, or needs a recook to refine a proxy mesh
		return false;
	}

	bool bSuccess = false;
	switch (InBakeOption)
	{
	case EHoudiniEngineBakeOption::ToActor:
	{
		bSuccess = FHoudiniEngineBakeUtils::BakeHoudiniActorToActors(InHACToBake, bInReplacePreviousBake, bInReplacePreviousBake, bInRecenterBakedActors);
	}
	break;

	case EHoudiniEngineBakeOption::ToBlueprint:
	{
		bSuccess = FHoudiniEngineBakeUtils::BakeBlueprints(InHACToBake, bInReplacePreviousBake, bInRecenterBakedActors);
	}
	break;

	case EHoudiniEngineBakeOption::ToFoliage:
	{
		bSuccess = FHoudiniEngineBakeUtils::BakeHoudiniActorToFoliage(InHACToBake, bInReplacePreviousBake);
	}
	break;

	case EHoudiniEngineBakeOption::ToWorldOutliner:
	{
		//Todo
		bSuccess = false;
	}
	break;

	}

	if (bSuccess && bInRemoveHACOutputOnSuccess)
	{
		TArray<UHoudiniOutput*> DeferredClearOutputs;
		FHoudiniOutputTranslator::ClearAndRemoveOutputs(InHACToBake, DeferredClearOutputs, true);
	}
	
	return bSuccess;
}

bool 
FHoudiniEngineBakeUtils::BakeHoudiniActorToActors(
	UHoudiniAssetComponent* HoudiniAssetComponent, bool bInReplaceActors, bool bInReplaceAssets, bool bInRecenterBakedActors) 
{
	if (!IsValid(HoudiniAssetComponent))
		return false;

	TArray<FHoudiniEngineBakedActor> NewActors;
	TArray<UPackage*> PackagesToSave;
	FHoudiniEngineOutputStats BakeStats;

	const bool bBakedWithErrors = !FHoudiniEngineBakeUtils::BakeHoudiniActorToActors(
		HoudiniAssetComponent, bInReplaceActors, bInReplaceAssets, NewActors, PackagesToSave, BakeStats);
	if (bBakedWithErrors)
	{
		// TODO ?
		HOUDINI_LOG_WARNING(TEXT("Errors when baking"));
	}

	// Save the created packages
	FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

	// Recenter and select the baked actors
	if (GEditor && NewActors.Num() > 0)
		GEditor->SelectNone(false, true);
	
	for (const FHoudiniEngineBakedActor& Entry : NewActors)
	{
		if (!IsValid(Entry.Actor))
			continue;
		
		if (bInRecenterBakedActors)
			CenterActorToBoundingBoxCenter(Entry.Actor);

		if (GEditor)
			GEditor->SelectActor(Entry.Actor, true, false);
	}
	
	if (GEditor && NewActors.Num() > 0)
		GEditor->NoteSelectionChange();

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakeStats.NumPackagesCreated, BakeStats.NumPackagesUpdated } );
		FHoudiniEngine::Get().FinishTaskSlateNotification( FText::FromString(Msg) );
	}

	// Broadcast that the bake is complete
	HoudiniAssetComponent->HandleOnPostBake(!bBakedWithErrors);

	return true;
}

bool
FHoudiniEngineBakeUtils::BakeHoudiniActorToActors(
	UHoudiniAssetComponent* HoudiniAssetComponent,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	TArray<FHoudiniEngineBakedActor>& OutNewActors, 
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats,
	TArray<EHoudiniOutputType> const* InOutputTypesToBake,
	TArray<EHoudiniInstancerComponentType> const* InInstancerComponentTypesToBake,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	if (!IsValid(HoudiniAssetComponent))
		return false;

	// Get an array of the outputs
	const int32 NumOutputs = HoudiniAssetComponent->GetNumOutputs();
	TArray<UHoudiniOutput*> Outputs;
	Outputs.Reserve(NumOutputs);
	for (int32 OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
	{
		Outputs.Add(HoudiniAssetComponent->GetOutputAt(OutputIdx));
	}

	// Get the previous bake objects and grow/shrink to match asset outputs
	TArray<FHoudiniBakedOutput>& BakedOutputs = HoudiniAssetComponent->GetBakedOutputs();
	// Ensure we have an entry for each output
	if (BakedOutputs.Num() != NumOutputs)
		BakedOutputs.SetNum(NumOutputs);

	const TArray<FHoudiniEngineBakedActor> AllBakedActors;
	return BakeHoudiniOutputsToActors(
		HoudiniAssetComponent,
		Outputs,
		BakedOutputs,
		HoudiniAssetComponent->GetComponentTransform(),
		HoudiniAssetComponent->BakeFolder,
		HoudiniAssetComponent->TemporaryCookFolder,
		bInReplaceActors,
		bInReplaceAssets,
		AllBakedActors,
		OutNewActors,
		OutPackagesToSave,
		OutBakeStats,
		InOutputTypesToBake,
		InInstancerComponentTypesToBake,
		InFallbackActor,
		InFallbackWorldOutlinerFolder);
}

bool
FHoudiniEngineBakeUtils::BakeHoudiniOutputsToActors(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	const TArray<UHoudiniOutput*>& InOutputs,
	TArray<FHoudiniBakedOutput>& InBakedOutputs,
	const FTransform& InParentTransform,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors, 
	TArray<FHoudiniEngineBakedActor>& OutNewActors, 
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats,
	TArray<EHoudiniOutputType> const* InOutputTypesToBake,
	TArray<EHoudiniInstancerComponentType> const* InInstancerComponentTypesToBake,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	const int32 NumOutputs = InOutputs.Num();
	
	const FString MsgTemplate = TEXT("Baking output: {0}/{1}.");
	FString Msg = FString::Format(*MsgTemplate, { 0, NumOutputs });
	FHoudiniEngine::Get().CreateTaskSlateNotification(FText::FromString(Msg));

	TArray<FHoudiniEngineBakedActor> AllBakedActors = InBakedActors;
	TArray<FHoudiniEngineBakedActor> NewBakedActors;

	// Ensure that InBakedOutputs is the same size as InOutputs
	if (InBakedOutputs.Num() != NumOutputs)
		InBakedOutputs.SetNum(NumOutputs);
	
	// First bake everything except instancers, then bake instancers. Since instancers might use meshes in
	// from the other outputs.
	bool bHasAnyInstancers = false;
	int32 NumProcessedOutputs = 0;

	TMap<UMaterialInterface *, UMaterialInterface *> AlreadyBakedMaterialsMap;
	TMap<UStaticMesh*, UStaticMesh*> AlreadyBakedStaticMeshMap;
	TArray<FHoudiniEngineBakedActor> OutputBakedActors;
	for (int32 OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
	{
		UHoudiniOutput* Output = InOutputs[OutputIdx];
		if (!IsValid(Output))
		{
			NumProcessedOutputs++;
			continue;
		}

		Msg = FString::Format(*MsgTemplate, { NumProcessedOutputs + 1, NumOutputs });
		FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Msg));

		const EHoudiniOutputType OutputType = Output->GetType();
		// Check if we should skip this output type
		if (InOutputTypesToBake && InOutputTypesToBake->Find(OutputType) == INDEX_NONE)
		{
			NumProcessedOutputs++;
			continue;
		}

		OutputBakedActors.Reset();
		switch (OutputType)
		{
		case EHoudiniOutputType::Mesh:
		{
			FHoudiniEngineBakeUtils::BakeStaticMeshOutputToActors(
				HoudiniAssetComponent,
				OutputIdx,
				InOutputs,
				InBakedOutputs,
				InBakeFolder,
				InTempCookFolder,
				bInReplaceActors,
				bInReplaceAssets,
				AllBakedActors,
				OutputBakedActors,
				OutPackagesToSave,
				AlreadyBakedStaticMeshMap,
				AlreadyBakedMaterialsMap,
				OutBakeStats,
				InFallbackActor,
				InFallbackWorldOutlinerFolder);
		}
		break;

		case EHoudiniOutputType::Instancer:
		{
			if (!bHasAnyInstancers)
				bHasAnyInstancers = true;
			NumProcessedOutputs--;
		}
		break;

		case EHoudiniOutputType::Landscape:
		{
			const bool bResult = BakeLandscape(
				HoudiniAssetComponent,
				OutputIdx,
				InOutputs,
				InBakedOutputs,
				bInReplaceActors,
				bInReplaceAssets,
				InBakeFolder.Path,
				OutBakeStats);
		}
		break;

		case EHoudiniOutputType::Skeletal:
			break;

		case EHoudiniOutputType::Curve:
		{
			FHoudiniEngineBakeUtils::BakeHoudiniCurveOutputToActors(
				HoudiniAssetComponent,
				OutputIdx,
				InOutputs,
				InBakedOutputs,
				InBakeFolder,
				bInReplaceActors,
				bInReplaceAssets,
				AllBakedActors,
				OutputBakedActors,
				OutBakeStats,
				InFallbackActor,
				InFallbackWorldOutlinerFolder);
		}
		break;
		case EHoudiniOutputType::GeometryCollection:
		{
			BakeGeometryCollectionOutputToActors(
				HoudiniAssetComponent,
				OutputIdx,
				InOutputs,
				InBakedOutputs,
				InBakeFolder,
				InTempCookFolder,
				bInReplaceActors,
				bInReplaceAssets,
				AllBakedActors,
				OutputBakedActors,
				OutPackagesToSave,
				AlreadyBakedStaticMeshMap,
				AlreadyBakedMaterialsMap,
				OutBakeStats,
				InFallbackActor,
				InFallbackWorldOutlinerFolder);
		}
		break;

		case EHoudiniOutputType::Invalid:
			break;
		}

		AllBakedActors.Append(OutputBakedActors);
		NewBakedActors.Append(OutputBakedActors);

		NumProcessedOutputs++;
	}

	if (bHasAnyInstancers)
	{
		for (int32 OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
		{
			UHoudiniOutput* Output = InOutputs[OutputIdx];
			if (!IsValid(Output))
			{
				continue;
			}

			if (Output->GetType() == EHoudiniOutputType::Instancer)
			{
				OutputBakedActors.Reset();
				
				Msg = FString::Format(*MsgTemplate, { NumProcessedOutputs + 1, NumOutputs });
				FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Msg));

				FHoudiniEngineBakeUtils::BakeInstancerOutputToActors(
					HoudiniAssetComponent,
                    OutputIdx,
                    InOutputs,
                    InBakedOutputs,
                    InParentTransform,
                    InBakeFolder,
                    InTempCookFolder,
                    bInReplaceActors,
                    bInReplaceAssets,
                    AllBakedActors,
                    OutputBakedActors,
                    OutPackagesToSave,
                    AlreadyBakedStaticMeshMap,
                    AlreadyBakedMaterialsMap,
                    OutBakeStats,
                    InInstancerComponentTypesToBake,
                    InFallbackActor,
                    InFallbackWorldOutlinerFolder);

				AllBakedActors.Append(OutputBakedActors);
				NewBakedActors.Append(OutputBakedActors);
				
				NumProcessedOutputs++;
			}
		}
	}

	// Only do the post bake post-process once per Actor
	TSet<AActor*> UniqueActors;
	for (FHoudiniEngineBakedActor& BakedActor : NewBakedActors)
	{
		if (BakedActor.bPostBakeProcessPostponed && BakedActor.Actor)
		{
			BakedActor.bPostBakeProcessPostponed = false;
			AActor* Actor = BakedActor.Actor;
			bool bIsAlreadyInSet = false;
			UniqueActors.Add(Actor, &bIsAlreadyInSet);
			if (!bIsAlreadyInSet)
			{
				Actor->InvalidateLightingCache();
				Actor->PostEditMove(true);
				Actor->MarkPackageDirty();
			}
		}
	}

	OutNewActors = MoveTemp(NewBakedActors);

	return true;
}

bool
FHoudiniEngineBakeUtils::BakeInstancerOutputToFoliage(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	FHoudiniBakedOutputObject& InBakedOutputObject,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	FHoudiniEngineBakedActor& OutBakedActorEntry,
	TArray<UPackage*>& OutPackagesToSave,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	FHoudiniEngineOutputStats& OutBakeStats)
{
	UHoudiniOutput* Output = InAllOutputs[InOutputIndex];
	if (!IsValid(Output))
		return false;

	if (Output->GetType() != EHoudiniOutputType::Instancer)
		return false;

	if (!IsValid(InOutputObject.OutputComponent))
		return false;

	UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(InOutputObject.OutputComponent);
	if (!IsValid(SMC))
	{
		HOUDINI_LOG_WARNING(
			TEXT("Unsupported component for foliage: %s"),*(InOutputObject.OutputComponent->GetClass()->GetName()));
		return false;
	}

	UStaticMesh* InstancedStaticMesh = SMC->GetStaticMesh();
	if (!IsValid(InstancedStaticMesh))
	{
		// No mesh, skip this instancer
		return false;
	}

	const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets
		? EPackageReplaceMode::ReplaceExistingAssets
		: EPackageReplaceMode::CreateNewAssets;
	UWorld* DesiredWorld = Output ? Output->GetWorld() : GWorld;

	// Determine if the incoming mesh is temporary by looking for it in the mesh outputs. Populate mesh package params
	// for baking from it.
	// If not temporary set the ObjectName from the its package. (Also use this as a fallback default)
	FString ObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(InstancedStaticMesh);
	UStaticMesh* PreviousStaticMesh = Cast<UStaticMesh>(InBakedOutputObject.GetBakedObjectIfValid());
	UStaticMesh* BakedStaticMesh = nullptr;
	int32 MeshOutputIndex = INDEX_NONE;
	FHoudiniOutputObjectIdentifier MeshIdentifier;
	FHoudiniAttributeResolver MeshResolver;
	FHoudiniPackageParams MeshPackageParams;
	const bool bFoundMeshOutput = FindOutputObject(InstancedStaticMesh, EHoudiniOutputType::Mesh, InAllOutputs, MeshOutputIndex, MeshIdentifier);
	if (bFoundMeshOutput)
	{
		// Found the mesh in the mesh outputs, is temporary
		const FHoudiniOutputObject& MeshOutputObject = InAllOutputs[MeshOutputIndex]->GetOutputObjects().FindChecked(MeshIdentifier);
		FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
			DesiredWorld, HoudiniAssetComponent, MeshIdentifier, MeshOutputObject, ObjectName,
			MeshPackageParams, MeshResolver, InBakeFolder.Path, AssetPackageReplaceMode);
		// Update with resolved object name
		ObjectName = MeshPackageParams.ObjectName;

		// This will bake/duplicate the mesh if temporary, or return the input one if it is not
		BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
			InstancedStaticMesh, PreviousStaticMesh, MeshPackageParams, InAllOutputs, InBakedActors, InTempCookFolder.Path,
			OutPackagesToSave, InOutAlreadyBakedStaticMeshMap, InOutAlreadyBakedMaterialsMap, OutBakeStats);
	}
	else
	{
		BakedStaticMesh = InstancedStaticMesh;
	}

	// Update the baked object
	InBakedOutputObject.BakedObject = FSoftObjectPath(BakedStaticMesh).ToString();

	// const FString InstancerName = FString::Printf(TEXT("%s_foliage_%s"), *ObjectName, *(InOutputObjectIdentifier.SplitIdentifier));
	// Construct PackageParams for the instancer itself. When baking to actor we technically won't create a stand-alone
	// disk package for the instancer, but certain attributes (such as level path) use tokens populated from the
	// package params.
	FHoudiniPackageParams InstancerPackageParams;
	FHoudiniAttributeResolver InstancerResolver;
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
		DesiredWorld, HoudiniAssetComponent, InOutputObjectIdentifier, InOutputObject, ObjectName,
		InstancerPackageParams, InstancerResolver, InBakeFolder.Path, AssetPackageReplaceMode);

	// By default spawn in the current level unless specified via the unreal_level_path attribute
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
	{
		// Get the package path from the unreal_level_path attribute
		FString LevelPackagePath = InstancerResolver.ResolveFullLevelPath();

		bool bCreatedPackage = false;
		if (!FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
			LevelPackagePath,
			DesiredLevel,
			DesiredWorld,
			bCreatedPackage))
		{
			// TODO: LOG ERROR IF NO LEVEL
			HOUDINI_LOG_ERROR(TEXT("Could not find or create a level: %s"), *LevelPackagePath);
			return false;
		}

		// If we have created a new level, add it to the packages to save
		// TODO: ? always add?
		if (bCreatedPackage && DesiredLevel)
		{
			OutBakeStats.NotifyPackageCreated(1);
			OutBakeStats.NotifyObjectsCreated(DesiredLevel->GetClass()->GetName(), 1);
			// We can now save the package again, and unload it.
			OutPackagesToSave.Add(DesiredLevel->GetOutermost());
		}
	}

	if (!DesiredLevel)
		return false;

	// Get foliage actor for the level
	const bool bCreateIfNone = true;
	AInstancedFoliageActor* InstancedFoliageActor = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(DesiredLevel, bCreateIfNone);
	if (!IsValid(InstancedFoliageActor))
	{
		HOUDINI_LOG_ERROR(TEXT("Could not find or create an instanced foliage actor for level %s"), *(DesiredLevel->GetPathName()));
		return false;
	}

	// Get the previous bake data for this instancer
	InBakedOutputObject.BakedObject = FSoftObjectPath(BakedStaticMesh).ToString();

	// Foliage type is replaced in replacement mode if:
	//   the previous baked object is this foliage type
	//   and we haven't bake this foliage type during this bake (BakeResults)
	//   NOTE: foliage type is only recorded as the previous bake object if we created the foliage type
	//   TODO: replacement mode should probably only affect the instances themselves and not the foliage type
	//			since the foliage type is already linked to whatever mesh we are using (which will be replaced
	//			incremented already). To track instances it looks like we would have to use the locations of the
	//			baked instances (likely cannot use the indices, since the user might modify/add/remove instances
	//			after the bake).
	
	// See if we already have a FoliageType for that static mesh
	UFoliageType* FoliageType = InstancedFoliageActor->GetLocalFoliageTypeForSource(BakedStaticMesh);
	if (!IsValid(FoliageType))
	{
		// We need to create a new FoliageType for this Static Mesh
		// TODO: Add foliage default settings
		InstancedFoliageActor->AddMesh(BakedStaticMesh, &FoliageType);
		// Update the previous bake results with the foliage type we created
		InBakedOutputObject.BakedComponent = FSoftObjectPath(FoliageType).ToString();
	}
	else
	{
		const FString FoliageTypePath = FSoftObjectPath(FoliageType).ToString();
		if (bInReplaceAssets && InBakedOutputObject.BakedComponent == FoliageTypePath &&
			!InBakedActors.FindByPredicate([FoliageType](const FHoudiniEngineBakedActor& Entry) { return Entry.BakedComponent == FoliageType; }))
		{
			InstancedFoliageActor->RemoveFoliageType(&FoliageType, 1);
			// Update the previous bake results with the foliage type
			InBakedOutputObject.BakedComponent = FoliageTypePath;
		}
		else
		{
			// If we didn't create the foliage type, don't set the baked component
			InBakedOutputObject.BakedComponent.Empty();
		}
	}

	// Record the foliage bake in the current results
	FHoudiniEngineBakedActor NewResult;
	NewResult.OutputIndex = InOutputIndex;
	NewResult.OutputObjectIdentifier = InOutputObjectIdentifier;
	NewResult.SourceObject = InstancedStaticMesh;
	NewResult.BakedObject = BakedStaticMesh;
	NewResult.BakedComponent = FoliageType;

	OutBakedActorEntry = NewResult;

	// Get the FoliageMeshInfo for this Foliage type so we can add the instance to it
	FFoliageInfo* FoliageInfo = InstancedFoliageActor->FindOrAddMesh(FoliageType);
	if (!FoliageInfo)
		return false;

	int32 CurrentInstanceCount = 0;
	if (SMC->IsA<UInstancedStaticMeshComponent>())
	{
		UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(SMC);
		const int32 NumInstances = ISMC->GetInstanceCount();
		for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; ++InstanceIndex)
		{
			FTransform InstanceTransform;
			const bool bWorldSpace = true;
			if (ISMC->GetInstanceTransform(InstanceIndex, InstanceTransform, bWorldSpace))
			{
				FFoliageInstance FoliageInstance;
				FoliageInstance.Location = InstanceTransform.GetLocation();
				FoliageInstance.Rotation = InstanceTransform.GetRotation().Rotator();
				FoliageInstance.DrawScale3D = InstanceTransform.GetScale3D();

				FoliageInfo->AddInstance(FoliageType, FoliageInstance);
				CurrentInstanceCount++;
			}
		}
	}
	else
	{
		const FTransform ComponentToWorldTransform = SMC->GetComponentToWorld();
		FFoliageInstance FoliageInstance;
		FoliageInstance.Location = ComponentToWorldTransform.GetLocation();
		FoliageInstance.Rotation = ComponentToWorldTransform.GetRotation().Rotator();
		FoliageInstance.DrawScale3D = ComponentToWorldTransform.GetScale3D();

		FoliageInfo->AddInstance(FoliageType, FoliageInstance);

		CurrentInstanceCount++;
	}

	// TODO: This was due to a bug in UE4.22-20, check if still needed! 
	if (FoliageInfo->GetComponent())
		FoliageInfo->GetComponent()->BuildTreeIfOutdated(true, true);

	// Notify the user that we succesfully bake the instances to foliage
	FString Notification = TEXT("Successfully baked ") + FString::FromInt(CurrentInstanceCount) + TEXT(" instances of ") + BakedStaticMesh->GetName() + TEXT(" to Foliage");
	FHoudiniEngineUtils::CreateSlateNotification(Notification);

	InstancedFoliageActor->RegisterAllComponents();

	// Update / repopulate the foliage editor mode's mesh list
	if (CurrentInstanceCount > 0)
		FHoudiniEngineUtils::RepopulateFoliageTypeListInUI();

	return true;
}

bool 
FHoudiniEngineBakeUtils::CanHoudiniAssetComponentBakeToFoliage(UHoudiniAssetComponent* HoudiniAssetComponent) 
{
	if (!IsValid(HoudiniAssetComponent))
		return false;

	for (int32 n = 0; n < HoudiniAssetComponent->GetNumOutputs(); ++n) 
	{
		UHoudiniOutput* Output = HoudiniAssetComponent->GetOutputAt(n);
		if (!IsValid(Output))
			continue;

		if (Output->GetType() != EHoudiniOutputType::Instancer)
			continue;

		// If the output contains a geometry collection, don't allow baking to foliage
		if (FHoudiniGeometryCollectionTranslator::IsGeometryCollectionInstancer(Output))
			return false;
		
		if (Output->GetInstancedOutputs().Num() > 0)
			return true;
		/*
		// TODO: Is this needed? check we have components to bake?
		for (auto& OutputObjectPair : Output->GetOutputObjects())
		{
			if (OutputObjectPair.Value.OutputCompoent!= nullpt)
				return true;
		}
		*/
	}

	return false;
}

bool 
FHoudiniEngineBakeUtils::BakeHoudiniActorToFoliage(UHoudiniAssetComponent* HoudiniAssetComponent, bool bInReplaceAssets) 
{
	if (!IsValid(HoudiniAssetComponent))
		return false;

	TMap<UStaticMesh*, UStaticMesh*> AlreadyBakedStaticMeshMap;
	TMap<UMaterialInterface *, UMaterialInterface *> AlreadyBakedMaterialsMap;
	FHoudiniEngineOutputStats BakeStats;
	TArray<UPackage*> PackagesToSave;

	FTransform HoudiniAssetTransform = HoudiniAssetComponent->GetComponentTransform();

	// Build an array of the outputs so that we can search for meshes/previous baked meshes
	TArray<UHoudiniOutput*> Outputs;
	HoudiniAssetComponent->GetOutputs(Outputs);
	const int32 NumOutputs = HoudiniAssetComponent->GetNumOutputs();
	
	// Get the previous bake outputs and match the output array size
	TArray<FHoudiniBakedOutput>& BakedOutputs = HoudiniAssetComponent->GetBakedOutputs();
	if (BakedOutputs.Num() != NumOutputs)
		BakedOutputs.SetNum(NumOutputs);

	TArray<FHoudiniEngineBakedActor> AllBakedActors;
	
	bool bSuccess = true;
	// Map storing original and baked Static Meshes
	for (int32 OutputIdx = 0; OutputIdx < NumOutputs; OutputIdx++)
	{
		UHoudiniOutput* Output = Outputs[OutputIdx];
		if (!IsValid(Output))
			continue;

		if (Output->GetType() != EHoudiniOutputType::Instancer)
			continue;

		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
		const TMap<FHoudiniBakedOutputObjectIdentifier, FHoudiniBakedOutputObject>& OldBakedOutputObjects = BakedOutputs[OutputIdx].BakedOutputObjects;
		TMap<FHoudiniBakedOutputObjectIdentifier, FHoudiniBakedOutputObject> NewBakedOutputObjects;
		
		for (auto & Pair : OutputObjects)
		{
			const FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;
			FHoudiniOutputObject& OutputObject = Pair.Value;

			FHoudiniBakedOutputObject& BakedOutputObject = NewBakedOutputObjects.Add(Identifier);
			if (OldBakedOutputObjects.Contains(Identifier))
				BakedOutputObject = OldBakedOutputObjects.FindChecked(Identifier);

			FHoudiniEngineBakedActor OutputBakedActorEntry;
			const bool bInReplaceActors = false;
			if (BakeInstancerOutputToFoliage(
					HoudiniAssetComponent,
					OutputIdx,
					Outputs,
					Identifier,
					OutputObject,
					BakedOutputObject,
					HoudiniAssetComponent->BakeFolder,
					HoudiniAssetComponent->TemporaryCookFolder,
					bInReplaceActors,
					bInReplaceAssets,
					AllBakedActors,
					OutputBakedActorEntry,
					PackagesToSave,
					AlreadyBakedStaticMeshMap,
					AlreadyBakedMaterialsMap,
					BakeStats))
			{
				AllBakedActors.Add(OutputBakedActorEntry);
			}
			else
			{
				bSuccess = false;
			}
		}

		// Update the cached baked output data
		BakedOutputs[OutputIdx].BakedOutputObjects = NewBakedOutputObjects;
	}

	if (PackagesToSave.Num() > 0)
	{
		FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);
	}

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakeStats.NumPackagesCreated, BakeStats.NumPackagesUpdated } );
		FHoudiniEngine::Get().FinishTaskSlateNotification( FText::FromString(Msg) );
	}

	// Broadcast that the bake is complete
	HoudiniAssetComponent->HandleOnPostBake(bSuccess);

	return bSuccess;
}


bool
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	TArray<FHoudiniBakedOutput>& InBakedOutputs,
	const FTransform& InTransform,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	TArray<UPackage*>& OutPackagesToSave,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	FHoudiniEngineOutputStats& OutBakeStats,
	TArray<EHoudiniInstancerComponentType> const* InInstancerComponentTypesToBake,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	if (!InAllOutputs.IsValidIndex(InOutputIndex))
		return false;

	UHoudiniOutput* InOutput = InAllOutputs[InOutputIndex];	
	if (!IsValid(InOutput))
		return false;

	// Ensure we have the same number of baked outputs and asset outputs
	if (InBakedOutputs.Num() != InAllOutputs.Num())
		InBakedOutputs.SetNum(InAllOutputs.Num());

	// Geometry collection instancers will be done on the geometry collection output component.
	if (FHoudiniGeometryCollectionTranslator::IsGeometryCollectionInstancer(InOutput))
	{
		return true;
	}

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	const TMap<FHoudiniBakedOutputObjectIdentifier, FHoudiniBakedOutputObject>& OldBakedOutputObjects = InBakedOutputs[InOutputIndex].BakedOutputObjects;
	TMap<FHoudiniBakedOutputObjectIdentifier, FHoudiniBakedOutputObject> NewBakedOutputObjects;

	TArray<FHoudiniEngineBakedActor> AllBakedActors = InBakedActors;
	TArray<FHoudiniEngineBakedActor> NewBakedActors;
	TArray<FHoudiniEngineBakedActor> OutputBakedActors;

	// Iterate on the output objects, baking their object/component as we go
	for (auto& Pair : OutputObjects)
	{
		const FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;
		FHoudiniOutputObject& CurrentOutputObject = Pair.Value;
		
		FHoudiniBakedOutputObject& BakedOutputObject = NewBakedOutputObjects.Add(Identifier);
		if (OldBakedOutputObjects.Contains(Identifier))
			BakedOutputObject = OldBakedOutputObjects.FindChecked(Identifier);

		if (CurrentOutputObject.bProxyIsCurrent)
		{
			// TODO: we need to refine the SM first!
			// ?? 
		}

		if (!IsValid(CurrentOutputObject.OutputComponent))
			continue;

		OutputBakedActors.Reset();

		if (CurrentOutputObject.OutputComponent->IsA<UFoliageInstancedStaticMeshComponent>())
		{
			// Bake foliage as foliage
			if (!InInstancerComponentTypesToBake ||
				InInstancerComponentTypesToBake->Contains(EHoudiniInstancerComponentType::FoliageInstancedStaticMeshComponent))
			{
				FHoudiniEngineBakedActor BakedActorEntry;
				if (BakeInstancerOutputToFoliage(
						HoudiniAssetComponent,
						InOutputIndex,
						InAllOutputs,
						// InBakedOutputs,
						Pair.Key, 
						CurrentOutputObject, 
						BakedOutputObject,
						InBakeFolder,
						InTempCookFolder,
						bInReplaceActors, 
						bInReplaceAssets,
						AllBakedActors,
						BakedActorEntry,
						OutPackagesToSave,
						InOutAlreadyBakedStaticMeshMap,
						InOutAlreadyBakedMaterialsMap,
						OutBakeStats))
				{
					OutputBakedActors.Add(BakedActorEntry);
				}
			}
			else if (!InInstancerComponentTypesToBake ||
				InInstancerComponentTypesToBake->Contains(EHoudiniInstancerComponentType::FoliageAsHierarchicalInstancedStaticMeshComponent))
			{
				BakeInstancerOutputToActors_ISMC(
					HoudiniAssetComponent,
					InOutputIndex,
					InAllOutputs,
					// InBakedOutputs,
					Pair.Key, 
					CurrentOutputObject, 
					BakedOutputObject,
					InTransform,
					InBakeFolder,
					InTempCookFolder,
					bInReplaceActors, 
					bInReplaceAssets,
					AllBakedActors,
					OutputBakedActors,
					OutPackagesToSave,
					InOutAlreadyBakedStaticMeshMap,
					InOutAlreadyBakedMaterialsMap,
					OutBakeStats,
					InFallbackActor,
					InFallbackWorldOutlinerFolder);
			}
		}
		else if (CurrentOutputObject.OutputComponent->IsA<UInstancedStaticMeshComponent>()
			&& (!InInstancerComponentTypesToBake || InInstancerComponentTypesToBake->Contains(EHoudiniInstancerComponentType::InstancedStaticMeshComponent)))
		{
			BakeInstancerOutputToActors_ISMC(
				HoudiniAssetComponent,
				InOutputIndex,
				InAllOutputs,
				// InBakedOutputs,
				Pair.Key, 
				CurrentOutputObject, 
				BakedOutputObject,
				InTransform,
				InBakeFolder,
				InTempCookFolder,
				bInReplaceActors, 
				bInReplaceAssets,
				AllBakedActors,
				OutputBakedActors,
				OutPackagesToSave,
				InOutAlreadyBakedStaticMeshMap,
				InOutAlreadyBakedMaterialsMap,
				OutBakeStats,
				InFallbackActor,
				InFallbackWorldOutlinerFolder);
		}
		else if (CurrentOutputObject.OutputComponent->IsA<UHoudiniInstancedActorComponent>()
				&& (!InInstancerComponentTypesToBake || InInstancerComponentTypesToBake->Contains(EHoudiniInstancerComponentType::InstancedActorComponent)))
		{
			BakeInstancerOutputToActors_IAC(
				HoudiniAssetComponent,
				InOutputIndex,
				Pair.Key, 
				CurrentOutputObject, 
				BakedOutputObject,
				InBakeFolder,
				bInReplaceActors, 
				bInReplaceAssets,
				AllBakedActors,
				OutputBakedActors,
				OutPackagesToSave,
				OutBakeStats);
		}
		else if (CurrentOutputObject.OutputComponent->IsA<UHoudiniMeshSplitInstancerComponent>()
		 		 && (!InInstancerComponentTypesToBake || InInstancerComponentTypesToBake->Contains(EHoudiniInstancerComponentType::MeshSplitInstancerComponent)))
		{
			FHoudiniEngineBakedActor BakedActorEntry;
			if (BakeInstancerOutputToActors_MSIC(
					HoudiniAssetComponent,
					InOutputIndex,
					InAllOutputs,
					// InBakedOutputs,
					Pair.Key, 
					CurrentOutputObject, 
					BakedOutputObject,
					InTransform,
					InBakeFolder,
					InTempCookFolder,
					bInReplaceActors, 
					bInReplaceAssets,
					AllBakedActors,
					BakedActorEntry,
					OutPackagesToSave,
					InOutAlreadyBakedStaticMeshMap,
					InOutAlreadyBakedMaterialsMap,
					OutBakeStats,
					InFallbackActor,
					InFallbackWorldOutlinerFolder))
			{
				OutputBakedActors.Add(BakedActorEntry);
			}
		}
		else if (CurrentOutputObject.OutputComponent->IsA<UStaticMeshComponent>()
	  			 && (!InInstancerComponentTypesToBake || InInstancerComponentTypesToBake->Contains(EHoudiniInstancerComponentType::StaticMeshComponent)))
		{
			FHoudiniEngineBakedActor BakedActorEntry;
			if (BakeInstancerOutputToActors_SMC(
					HoudiniAssetComponent,
					InOutputIndex,
					InAllOutputs,
					// InBakedOutputs,
					Pair.Key, 
					CurrentOutputObject, 
					BakedOutputObject, 
					InBakeFolder,
					InTempCookFolder,
					bInReplaceActors, 
					bInReplaceAssets,
					AllBakedActors,
					BakedActorEntry,
					OutPackagesToSave,
					InOutAlreadyBakedStaticMeshMap,
					InOutAlreadyBakedMaterialsMap,
					OutBakeStats,
					InFallbackActor,
					InFallbackWorldOutlinerFolder))
			{
				OutputBakedActors.Add(BakedActorEntry);
			}
			
		}
		else
		{
			// Unsupported component!
		}

		AllBakedActors.Append(OutputBakedActors);
		NewBakedActors.Append(OutputBakedActors);
	}

	// Update the cached baked output data
	InBakedOutputs[InOutputIndex].BakedOutputObjects = NewBakedOutputObjects;

	OutActors = MoveTemp(NewBakedActors);

	return true;
}

bool
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_ISMC(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	// const TArray<FHoudiniBakedOutput>& InAllBakedOutputs,
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	FHoudiniBakedOutputObject& InBakedOutputObject,
	const FTransform& InTransform,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	TArray<UPackage*>& OutPackagesToSave,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	FHoudiniEngineOutputStats& OutBakeStats,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	UInstancedStaticMeshComponent * InISMC = Cast<UInstancedStaticMeshComponent>(InOutputObject.OutputComponent);
	if (!IsValid(InISMC))
		return false;

	AActor * OwnerActor = InISMC->GetOwner();
	if (!IsValid(OwnerActor))
		return false;

	UStaticMesh * StaticMesh = InISMC->GetStaticMesh();
	if (!IsValid(StaticMesh))
		return false;

	// Certain SMC materials may need to be duplicated if we didn't generate the mesh object.
	TArray<UMaterialInterface *> DuplicatedISMCOverrideMaterials;
	
	const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
		EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
	UWorld* DesiredWorld = OwnerActor ? OwnerActor->GetWorld() : GWorld;

	// Determine if the incoming mesh is temporary by looking for it in the mesh outputs. Populate mesh package params
	// for baking from it.
	// If not temporary set the ObjectName from the its package. (Also use this as a fallback default)
	FString ObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(StaticMesh);
	UStaticMesh* PreviousStaticMesh = Cast<UStaticMesh>(InBakedOutputObject.GetBakedObjectIfValid());
	UStaticMesh* BakedStaticMesh = nullptr;
	int32 MeshOutputIndex = INDEX_NONE;
	FHoudiniOutputObjectIdentifier MeshIdentifier;
	FHoudiniAttributeResolver MeshResolver;
	FHoudiniPackageParams MeshPackageParams;

	// Construct PackageParams for the instancer itself. When baking to actor we technically won't create a stand-alone
	// disk package for the instancer, but certain attributes (such as level path) use tokens populated from the
	// package params.
	FHoudiniPackageParams InstancerPackageParams;
	FHoudiniAttributeResolver InstancerResolver;
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
		DesiredWorld, HoudiniAssetComponent, InOutputObjectIdentifier, InOutputObject, ObjectName,
		InstancerPackageParams, InstancerResolver, InBakeFolder.Path, AssetPackageReplaceMode);
	
	const bool bFoundMeshOutput = FindOutputObject(StaticMesh, EHoudiniOutputType::Mesh, InAllOutputs, MeshOutputIndex, MeshIdentifier);
	if (bFoundMeshOutput)
	{
		// Found the mesh in the mesh outputs, is temporary
		const FHoudiniOutputObject& MeshOutputObject = InAllOutputs[MeshOutputIndex]->GetOutputObjects().FindChecked(MeshIdentifier);
		FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
			DesiredWorld, HoudiniAssetComponent, MeshIdentifier, MeshOutputObject, ObjectName,
			MeshPackageParams, MeshResolver, InBakeFolder.Path, AssetPackageReplaceMode);
		// Update with resolved object name
		ObjectName = MeshPackageParams.ObjectName;

		// This will bake/duplicate the mesh if temporary, or return the input one if it is not
		BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
			StaticMesh, PreviousStaticMesh, MeshPackageParams, InAllOutputs, InBakedActors, InTempCookFolder.Path,
			OutPackagesToSave, InOutAlreadyBakedStaticMeshMap, InOutAlreadyBakedMaterialsMap, OutBakeStats);
	}
	else
	{
		BakedStaticMesh = StaticMesh;


		// We still need to duplicate materials, if they are temporary.
		TArray<UMaterialInterface *> Materials = InISMC->GetMaterials();
		for (int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); ++MaterialIdx)
		{
			UMaterialInterface* MaterialInterface = Materials[MaterialIdx];
			if (!IsValid(MaterialInterface))
				continue;
	
			// Only duplicate the material if it is temporary
			if (IsObjectTemporary(MaterialInterface, EHoudiniOutputType::Invalid, InAllOutputs, InTempCookFolder.Path))
			{
				UMaterialInterface * DuplicatedMaterial = BakeSingleMaterialToPackage(
					MaterialInterface, InstancerPackageParams, OutPackagesToSave, InOutAlreadyBakedMaterialsMap, OutBakeStats);
				DuplicatedISMCOverrideMaterials.Add(DuplicatedMaterial);
			}
		}
	}

	// Update the baked object
	InBakedOutputObject.BakedObject = FSoftObjectPath(BakedStaticMesh).ToString();

	// Instancer name adds the split identifier (INSTANCERNUM_VARIATIONNUM)
	const FString InstancerName = ObjectName + "_instancer_" + InOutputObjectIdentifier.SplitIdentifier;
	const FName WorldOutlinerFolderPath = GetOutlinerFolderPath(
		InOutputObject,
		FName(InFallbackWorldOutlinerFolder.IsEmpty() ? InstancerPackageParams.HoudiniAssetActorName : InFallbackWorldOutlinerFolder));

	// By default spawn in the current level unless specified via the unreal_level_path attribute
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
	{
		// Get the package path from the unreal_level_apth attribute
		FString LevelPackagePath = InstancerResolver.ResolveFullLevelPath();

		bool bCreatedPackage = false;
		if (!FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
			LevelPackagePath,
			DesiredLevel,
			DesiredWorld,
			bCreatedPackage))
		{
			// TODO: LOG ERROR IF NO LEVEL
			return false;
		}

		// If we have created a new level, add it to the packages to save
		// TODO: ? always add?
		if (bCreatedPackage && DesiredLevel)
		{
			OutBakeStats.NotifyPackageCreated(1);
			OutBakeStats.NotifyObjectsCreated(DesiredLevel->GetClass()->GetName(), 1);
			// We can now save the package again, and unload it.
			OutPackagesToSave.Add(DesiredLevel->GetOutermost());
		}
	}

	if(!DesiredLevel)
		return false;

	// Try to find the unreal_bake_actor, if specified, or fallback to the default named actor
	FName BakeActorName;
	AActor* FoundActor = nullptr;
	bool bHasBakeActorName = false;
	if (!FindUnrealBakeActor(InOutputObject, InBakedOutputObject, InBakedActors, DesiredLevel, *InstancerName, bInReplaceActors, InFallbackActor, FoundActor, bHasBakeActorName, BakeActorName))
		return false;

	/*
	// TODO: Get the bake name!
	// Bake override, the output name
	// The bake name override has priority
	FString InstancerName = InOutputObject.BakeName;
	if (InstancerName.IsEmpty())
	{
		// .. then use the output name
		InstancerName = Resolver.ResolveOutputName();
	}
	*/

	// Should we create one actor with an ISMC or multiple actors with one SMC?
	bool bSpawnMultipleSMC = false;
	if (bSpawnMultipleSMC)
	{
		// TODO: Double check, Has a crash here!

		// Get the StaticMesh ActorFactory
		UActorFactory* ActorFactory = nullptr;
		TSubclassOf<AActor> BakeActorClass = nullptr;
		if (!FoundActor)
		{
			ActorFactory = GetActorFactory(NAME_None, BakeActorClass, UActorFactoryEmptyActor::StaticClass(), BakedStaticMesh);

			if (!ActorFactory)
				return false;
		}

		// Split the instances to multiple StaticMeshActors
		for (int32 InstanceIdx = 0; InstanceIdx < InISMC->GetInstanceCount(); InstanceIdx++)
		{
			FTransform InstanceTransform;
			InISMC->GetInstanceTransform(InstanceIdx, InstanceTransform, true);

			if (!FoundActor)
			{
				FoundActor = SpawnBakeActor(ActorFactory, BakedStaticMesh, DesiredLevel, InstanceTransform, HoudiniAssetComponent, BakeActorClass);
				if (!IsValid(FoundActor))
					continue;
			}

			const FString NewNameStr = MakeUniqueObjectNameIfNeeded(DesiredLevel, ActorFactory->NewActorClass, BakeActorName.ToString(), FoundActor);
			RenameAndRelabelActor(FoundActor, NewNameStr, false);

			// The folder is named after the original actor and contains all generated actors
			SetOutlinerFolderPath(FoundActor, InOutputObject, WorldOutlinerFolderPath);

			AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(FoundActor);
			if (!IsValid(SMActor))
				continue;

			// Copy properties from the existing component
			CopyPropertyToNewActorAndComponent(FoundActor, SMActor->GetStaticMeshComponent(), InISMC);

			FHoudiniEngineBakedActor& OutputEntry = OutActors.Add_GetRef(FHoudiniEngineBakedActor(
				FoundActor,
				BakeActorName,
				WorldOutlinerFolderPath,
				InOutputIndex,
				InOutputObjectIdentifier,
				BakedStaticMesh,
				StaticMesh,
				SMActor->GetStaticMeshComponent(),
				bFoundMeshOutput ? MeshPackageParams.BakeFolder : FString(),
				MeshPackageParams));
			OutputEntry.bInstancerOutput = true;
			OutputEntry.InstancerPackageParams = InstancerPackageParams;
		}
	}
	else
	{
		bool bSpawnedActor = false;
		if (!FoundActor)
		{
			// Only create one actor
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.OverrideLevel = DesiredLevel;
			SpawnInfo.ObjectFlags = RF_Transactional;

			if (!DesiredLevel->bUseExternalActors)
			{
				SpawnInfo.Name = FName(MakeUniqueObjectNameIfNeeded(DesiredLevel, AActor::StaticClass(), BakeActorName.ToString()));
			}
			SpawnInfo.bDeferConstruction = true;

			// Spawn the new Actor
			UClass* ActorClass = GetBakeActorClassOverride(InOutputObject);
			if (!ActorClass)
				ActorClass = AActor::StaticClass();
			FoundActor = DesiredLevel->OwningWorld->SpawnActor<AActor>(ActorClass, SpawnInfo);
			if (!IsValid(FoundActor))
				return false;
			bSpawnedActor = true;

			OutBakeStats.NotifyObjectsCreated(FoundActor->GetClass()->GetName(), 1);

			FHoudiniEngineRuntimeUtils::SetActorLabel(FoundActor, DesiredLevel->bUseExternalActors ? BakeActorName.ToString() : FoundActor->GetName());
			FoundActor->SetActorHiddenInGame(InISMC->bHiddenInGame);
		}
		else
		{
			// If there is a previously baked component, and we are in replace mode, remove it
			if (bInReplaceAssets)
			{
				USceneComponent* InPrevComponent = Cast<USceneComponent>(InBakedOutputObject.GetBakedComponentIfValid());
				if (IsValid(InPrevComponent) && InPrevComponent->GetOwner() == FoundActor)
					RemovePreviouslyBakedComponent(InPrevComponent);
			}
			
			const FString UniqueActorNameStr = MakeUniqueObjectNameIfNeeded(DesiredLevel, AActor::StaticClass(), BakeActorName.ToString(), FoundActor);
			RenameAndRelabelActor(FoundActor, UniqueActorNameStr, false);

			OutBakeStats.NotifyObjectsUpdated(FoundActor->GetClass()->GetName(), 1);
		}
		
		// The folder is named after the original actor and contains all generated actors
		SetOutlinerFolderPath(FoundActor, InOutputObject, WorldOutlinerFolderPath);

		// Get/create the actor's root component
		const bool bCreateIfMissing = true;
		USceneComponent* RootComponent = GetActorRootComponent(FoundActor, bCreateIfMissing);
		if (bSpawnedActor && IsValid(RootComponent))
			RootComponent->SetWorldTransform(InTransform);
		
		// Duplicate the instancer component, create a Hierarchical ISMC if needed
		UInstancedStaticMeshComponent* NewISMC = nullptr;
		UHierarchicalInstancedStaticMeshComponent* InHISMC = Cast<UHierarchicalInstancedStaticMeshComponent>(InISMC);
		if (InHISMC)
		{
			// Handle foliage: don't duplicate foliage component, create a new hierarchical one and copy what we can
			// from the foliage component
			if (InHISMC->IsA<UFoliageInstancedStaticMeshComponent>())
			{
				NewISMC = NewObject<UHierarchicalInstancedStaticMeshComponent>(
					FoundActor,
					FName(MakeUniqueObjectNameIfNeeded(FoundActor, InHISMC->GetClass(), InISMC->GetName())));
				CopyPropertyToNewActorAndComponent(FoundActor, NewISMC, InISMC);

				OutBakeStats.NotifyObjectsCreated(UHierarchicalInstancedStaticMeshComponent::StaticClass()->GetName(), 1);
			}
			else
			{
				NewISMC = DuplicateObject<UHierarchicalInstancedStaticMeshComponent>(
					InHISMC,
					FoundActor,
					FName(MakeUniqueObjectNameIfNeeded(FoundActor, InHISMC->GetClass(), InISMC->GetName())));

				OutBakeStats.NotifyObjectsCreated(InHISMC->GetClass()->GetName(), 1);
			}
		}
		else
		{
			NewISMC = DuplicateObject<UInstancedStaticMeshComponent>(
				InISMC,
				FoundActor,
				FName(MakeUniqueObjectNameIfNeeded(FoundActor, InISMC->GetClass(), InISMC->GetName())));

			OutBakeStats.NotifyObjectsCreated(InISMC->GetClass()->GetName(), 1);
		}

		if (!NewISMC)
		{
			//DesiredLevel->OwningWorld->
			return false;
		}

		InBakedOutputObject.BakedComponent = FSoftObjectPath(NewISMC).ToString();

		NewISMC->RegisterComponent();
		// NewISMC->SetupAttachment(nullptr);
		NewISMC->SetStaticMesh(BakedStaticMesh);
		FoundActor->AddInstanceComponent(NewISMC);

		if (DuplicatedISMCOverrideMaterials.Num() > 0)
		{
			UMaterialInterface * InstancerMaterial = DuplicatedISMCOverrideMaterials[0];
			if (InstancerMaterial)
			{
				NewISMC->OverrideMaterials.Empty();
				int32 MeshMaterialCount = BakedStaticMesh->GetStaticMaterials().Num();
				for (int32 Idx = 0; Idx < MeshMaterialCount; ++Idx)
					NewISMC->SetMaterial(Idx, InstancerMaterial);
			}
		}
		
		// NewActor->SetRootComponent(NewISMC);
		if (IsValid(RootComponent))
			NewISMC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		NewISMC->SetWorldTransform(InISMC->GetComponentTransform());

		// TODO: do we need to copy properties here, we duplicated the component
		// // Copy properties from the existing component
		// CopyPropertyToNewActorAndComponent(FoundActor, NewISMC, InISMC);

		if (bSpawnedActor)
			FoundActor->FinishSpawning(InTransform);

		InBakedOutputObject.Actor = FSoftObjectPath(FoundActor).ToString();
		FHoudiniEngineBakedActor& OutputEntry = OutActors.Add_GetRef(FHoudiniEngineBakedActor(
			FoundActor,
			BakeActorName,
			WorldOutlinerFolderPath,
			InOutputIndex,
			InOutputObjectIdentifier,
			BakedStaticMesh,
			StaticMesh,
			NewISMC,
			bFoundMeshOutput ? MeshPackageParams.BakeFolder : FString(),
			MeshPackageParams));
		OutputEntry.bInstancerOutput = true;
		OutputEntry.InstancerPackageParams = InstancerPackageParams;

		// Postpone post-bake calls to do them once per actor
		OutActors.Last().bPostBakeProcessPostponed = true;
	}

	// If we are baking in replace mode, remove previously baked components/instancers
	if (bInReplaceActors && bInReplaceAssets)
	{
		const bool bInDestroyBakedComponent = false;
		const bool bInDestroyBakedInstancedActors = true;
		const bool bInDestroyBakedInstancedComponents = true;
		DestroyPreviousBakeOutput(
			InBakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
	}
	
	return true;
}

bool
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_SMC(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	// const TArray<FHoudiniBakedOutput>& InAllBakedOutputs,
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	FHoudiniBakedOutputObject& InBakedOutputObject,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	FHoudiniEngineBakedActor& OutBakedActorEntry,
	TArray<UPackage*>& OutPackagesToSave,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	FHoudiniEngineOutputStats& OutBakeStats,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	UStaticMeshComponent* InSMC = Cast<UStaticMeshComponent>(InOutputObject.OutputComponent);
	if (!IsValid(InSMC))
		return false;

	AActor* OwnerActor = InSMC->GetOwner();
	if (!IsValid(OwnerActor))
		return false;

	UStaticMesh* StaticMesh = InSMC->GetStaticMesh();
	if (!IsValid(StaticMesh))
		return false;

	UWorld* DesiredWorld = OwnerActor ? OwnerActor->GetWorld() : GWorld;
	const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
		EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;

	// Certain SMC materials may need to be duplicated if we didn't generate the mesh object.
	TArray<UMaterialInterface *> DuplicatedSMCOverrideMaterials;

	// Determine if the incoming mesh is temporary by looking for it in the mesh outputs. Populate mesh package params
	// for baking from it.
	// If not temporary set the ObjectName from the its package. (Also use this as a fallback default)
	FString ObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(StaticMesh);
	UStaticMesh* PreviousStaticMesh = Cast<UStaticMesh>(InBakedOutputObject.GetBakedObjectIfValid());
	UStaticMesh* BakedStaticMesh = nullptr;
	int32 MeshOutputIndex = INDEX_NONE;
	FHoudiniOutputObjectIdentifier MeshIdentifier;
	FHoudiniAttributeResolver MeshResolver;
	FHoudiniPackageParams MeshPackageParams;

	// Package params for the instancer
	// See if the instanced static mesh is still a temporary Houdini created Static Mesh
	// If it is, we need to bake the StaticMesh first
	FHoudiniPackageParams InstancerPackageParams;
	// Configure FHoudiniAttributeResolver and fill the package params with resolved object name and bake folder.
	// The resolver is then also configured with the package params for subsequent resolving (level_path etc)
	FHoudiniAttributeResolver InstancerResolver;
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
	DesiredWorld, HoudiniAssetComponent,  InOutputObjectIdentifier, InOutputObject, ObjectName,
	InstancerPackageParams, InstancerResolver, InBakeFolder.Path, AssetPackageReplaceMode);
	
	const bool bFoundMeshOutput = FindOutputObject(StaticMesh, EHoudiniOutputType::Mesh, InAllOutputs, MeshOutputIndex, MeshIdentifier);
	if (bFoundMeshOutput)
	{
		// Found the mesh in the mesh outputs, is temporary
		const FHoudiniOutputObject& MeshOutputObject = InAllOutputs[MeshOutputIndex]->GetOutputObjects().FindChecked(MeshIdentifier);
		FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
			DesiredWorld, HoudiniAssetComponent, MeshIdentifier, MeshOutputObject, ObjectName,
			MeshPackageParams, MeshResolver, InBakeFolder.Path, AssetPackageReplaceMode);
		// Update with resolved object name
		ObjectName = MeshPackageParams.ObjectName;

		// This will bake/duplicate the mesh if temporary, or return the input one if it is not
		BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
			StaticMesh, PreviousStaticMesh, MeshPackageParams, InAllOutputs, InBakedActors, InTempCookFolder.Path,
			OutPackagesToSave, InOutAlreadyBakedStaticMeshMap, InOutAlreadyBakedMaterialsMap, OutBakeStats);
	}
	else
	{
		BakedStaticMesh = StaticMesh;

		// We still need to duplicate materials, if they are temporary.
		TArray<UMaterialInterface *> Materials = InSMC->GetMaterials();
		for (int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); ++MaterialIdx)
		{
			UMaterialInterface* MaterialInterface = Materials[MaterialIdx];
			if (!IsValid(MaterialInterface))
				continue;
	
			// Only duplicate the material if it is temporary
			if (IsObjectTemporary(MaterialInterface, EHoudiniOutputType::Invalid, InAllOutputs, InTempCookFolder.Path))
			{
				UMaterialInterface * DuplicatedMaterial = BakeSingleMaterialToPackage(
					MaterialInterface, InstancerPackageParams, OutPackagesToSave, InOutAlreadyBakedMaterialsMap, OutBakeStats);
				DuplicatedSMCOverrideMaterials.Add(DuplicatedMaterial);
			}
		}
	}

	// Update the previous baked object
	InBakedOutputObject.BakedObject = FSoftObjectPath(BakedStaticMesh).ToString();

	// Instancer name adds the split identifier (INSTANCERNUM_VARIATIONNUM)
	const FString InstancerName = ObjectName + "_instancer_" + InOutputObjectIdentifier.SplitIdentifier;
	const FName WorldOutlinerFolderPath = GetOutlinerFolderPath(
		InOutputObject, 
		FName(InFallbackWorldOutlinerFolder.IsEmpty() ? InstancerPackageParams.HoudiniAssetActorName : InFallbackWorldOutlinerFolder));



	// By default spawn in the current level unless specified via the unreal_level_path attribute
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
	{
		// Get the package path from the unreal_level_apth attribute
		FString LevelPackagePath = InstancerResolver.ResolveFullLevelPath();

		bool bCreatedPackage = false;
		if (!FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
			LevelPackagePath,
			DesiredLevel,
			DesiredWorld,
			bCreatedPackage))
		{
			// TODO: LOG ERROR IF NO LEVEL
			return false;
		}

		// If we have created a level, add it to the packages to save
		// TODO: ? always add?
		if (bCreatedPackage && DesiredLevel)
		{
			OutBakeStats.NotifyPackageCreated(1);
			OutBakeStats.NotifyObjectsCreated(DesiredLevel->GetClass()->GetName(), 1);
			// We can now save the package again, and unload it.
			OutPackagesToSave.Add(DesiredLevel->GetOutermost());
		}
	}

	if (!DesiredLevel)
		return false;

	// Try to find the unreal_bake_actor, if specified
	FName BakeActorName;
	AActor* FoundActor = nullptr;
	bool bHasBakeActorName = false;
	if (!FindUnrealBakeActor(InOutputObject, InBakedOutputObject, InBakedActors, DesiredLevel, *InstancerName, bInReplaceActors, InFallbackActor, FoundActor, bHasBakeActorName, BakeActorName))
		return false;

	UStaticMeshComponent* StaticMeshComponent = nullptr;
	// Create an actor if we didn't find one
	bool bCreatedNewActor = false;
	if (!FoundActor)
	{
		// Get the actor factory for the unreal_bake_actor_class attribute. If not set, use an empty actor.
		TSubclassOf<AActor> BakeActorClass = nullptr;
		UActorFactory* ActorFactory = GetActorFactory(InOutputObject, BakeActorClass, UActorFactoryEmptyActor::StaticClass(), BakedStaticMesh);
		if (!ActorFactory)
		{
			return false;
		}

		FoundActor = SpawnBakeActor(ActorFactory, BakedStaticMesh, DesiredLevel, InSMC->GetComponentTransform(), HoudiniAssetComponent, BakeActorClass);
		if (!IsValid(FoundActor))
			return false;

		OutBakeStats.NotifyObjectsCreated(FoundActor->GetClass()->GetName(), 1);
		bCreatedNewActor = true;

		// If the factory created a static mesh actor, get its component.
		AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(FoundActor);
		if (IsValid(SMActor))
			StaticMeshComponent = SMActor->GetStaticMeshComponent();
	}

	// We have an existing actor (or we created one without a static mesh component) 
	if (!IsValid(StaticMeshComponent))
	{
		USceneComponent* RootComponent = GetActorRootComponent(FoundActor);
		if (!IsValid(RootComponent))
			return false;

		if (bInReplaceAssets && !bCreatedNewActor)
		{
			// Check if we have a previous bake component and that it belongs to FoundActor, if so, reuse it
			UStaticMeshComponent* PrevSMC = Cast<UStaticMeshComponent>(InBakedOutputObject.GetBakedComponentIfValid());
			if (IsValid(PrevSMC) && (PrevSMC->GetOwner() == FoundActor))
			{
				StaticMeshComponent = PrevSMC;
			}
		}
		
		if (!IsValid(StaticMeshComponent))
		{
			// Create a new static mesh component
			StaticMeshComponent = NewObject<UStaticMeshComponent>(FoundActor, NAME_None, RF_Transactional);

			FoundActor->AddInstanceComponent(StaticMeshComponent);
			StaticMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
			StaticMeshComponent->RegisterComponent();

			OutBakeStats.NotifyObjectsCreated(StaticMeshComponent->GetClass()->GetName(), 1);
		}
	}

	const FString NewNameStr = MakeUniqueObjectNameIfNeeded(DesiredLevel, FoundActor->GetClass(), BakeActorName.ToString(), FoundActor);
	RenameAndRelabelActor(FoundActor, NewNameStr, false);

	// The folder is named after the original actor and contains all generated actors
	SetOutlinerFolderPath(FoundActor, InOutputObject, WorldOutlinerFolderPath);

	// Update the previous baked component
	InBakedOutputObject.BakedComponent = FSoftObjectPath(StaticMeshComponent).ToString();
	
	if (!IsValid(StaticMeshComponent))
		return false;
	
	// Copy properties from the existing component
	const bool bCopyWorldTransform = true;
	CopyPropertyToNewActorAndComponent(FoundActor, StaticMeshComponent, InSMC, bCopyWorldTransform);
	StaticMeshComponent->SetStaticMesh(BakedStaticMesh);

	if (DuplicatedSMCOverrideMaterials.Num() > 0)
	{
		UMaterialInterface * InstancerMaterial = DuplicatedSMCOverrideMaterials[0];
		if (InstancerMaterial)
		{
			StaticMeshComponent->OverrideMaterials.Empty();
			int32 MeshMaterialCount = BakedStaticMesh->GetStaticMaterials().Num();
			for (int32 Idx = 0; Idx < MeshMaterialCount; ++Idx)
				StaticMeshComponent->SetMaterial(Idx, InstancerMaterial);
		}
	}
	
	InBakedOutputObject.Actor = FSoftObjectPath(FoundActor).ToString();
	FHoudiniEngineBakedActor OutputEntry(
		FoundActor,
		BakeActorName,
		WorldOutlinerFolderPath,
		InOutputIndex,
		InOutputObjectIdentifier,
		BakedStaticMesh,
		StaticMesh,
		StaticMeshComponent,
		MeshPackageParams.BakeFolder,
		MeshPackageParams);
	OutputEntry.bInstancerOutput = true;
	OutputEntry.InstancerPackageParams = InstancerPackageParams;

	OutBakedActorEntry = OutputEntry;
 
	// If we are baking in replace mode, remove previously baked components/instancers
	if (bInReplaceActors && bInReplaceAssets)
	{
		const bool bInDestroyBakedComponent = false;
		const bool bInDestroyBakedInstancedActors = true;
		const bool bInDestroyBakedInstancedComponents = true;
		DestroyPreviousBakeOutput(
			InBakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
	}

	return true;
}

bool
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_IAC(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	FHoudiniBakedOutputObject& InBakedOutputObject,
	const FDirectoryPath& InBakeFolder,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats)
{
	UHoudiniInstancedActorComponent* InIAC = Cast<UHoudiniInstancedActorComponent>(InOutputObject.OutputComponent);
	if (!IsValid(InIAC))
		return false;

	AActor* OwnerActor = InIAC->GetOwner();
	if (!IsValid(OwnerActor))
		return false;

	// Get the object instanced by this IAC
	UObject* InstancedObject = InIAC->GetInstancedObject();
	if (!IsValid(InstancedObject))
		return false;

	// Set the default object name to the 
	const FString DefaultObjectName = InstancedObject->GetName();
	
	FHoudiniPackageParams PackageParams;
	const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
		EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
	// Configure FHoudiniAttributeResolver and fill the package params with resolved object name and bake folder.
	// The resolver is then also configured with the package params for subsequent resolving (level_path etc)
	FHoudiniAttributeResolver Resolver;
	UWorld* DesiredWorld = OwnerActor ? OwnerActor->GetWorld() : GWorld;
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
		DesiredWorld, HoudiniAssetComponent, InOutputObjectIdentifier, InOutputObject, DefaultObjectName,
		PackageParams, Resolver, InBakeFolder.Path, AssetPackageReplaceMode);

	// By default spawn in the current level unless specified via the unreal_level_path attribute
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();

	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
	{
		// Get the package path from the unreal_level_apth attribute
		FString LevelPackagePath = Resolver.ResolveFullLevelPath();

		bool bCreatedPackage = false;
		if (!FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
			LevelPackagePath,
			DesiredLevel,
			DesiredWorld,
			bCreatedPackage))
		{
			// TODO: LOG ERROR IF NO LEVEL
			return false;
		}

		// If we have created a level, add it to the packages to save
		// TODO: ? always add?
		if (bCreatedPackage && DesiredLevel)
		{
			OutBakeStats.NotifyPackageCreated(1);
			OutBakeStats.NotifyObjectsCreated(DesiredLevel->GetClass()->GetName(), 1);
			// We can now save the package again, and unload it.
			OutPackagesToSave.Add(DesiredLevel->GetOutermost());
		}
	}

	if (!DesiredLevel)
		return false;

	const FName WorldOutlinerFolderPath = GetOutlinerFolderPath(InOutputObject, *PackageParams.HoudiniAssetActorName);

	// Try to find the unreal_bake_actor, if specified. If we found the actor, we will attach the instanced actors
	// to it. If we did not find an actor, but unreal_bake_actor was set, then we create a new actor with that name
	// and parent the instanced actors to it. Otherwise, we don't attach the instanced actors to anything.
	FName DesiredParentBakeActorName;
	FName ParentBakeActorName;
	AActor* ParentToActor = nullptr;
	bool bHasBakeActorName = false;
	constexpr AActor* FallbackActor = nullptr;
	const FName DefaultBakeActorName = NAME_None;
	if (!FindUnrealBakeActor(InOutputObject, InBakedOutputObject, InBakedActors, DesiredLevel, DefaultBakeActorName, bInReplaceActors, FallbackActor, ParentToActor, bHasBakeActorName, DesiredParentBakeActorName))
	{
		HOUDINI_LOG_ERROR(TEXT("Finding / processing unreal_bake_actor unexpectedly failed during bake."));
		return false;
	}

	OutActors.Reset();

	if (!ParentToActor && bHasBakeActorName)
	{
		// Get the actor factory for the unreal_bake_actor_class attribute. If not set, use an empty actor.
		TSubclassOf<AActor> BakeActorClass = nullptr;
		UActorFactory* const ActorFactory = GetActorFactory(InOutputObject, BakeActorClass, UActorFactoryEmptyActor::StaticClass());
		if (!ActorFactory)
			return false;
		
		constexpr UObject* AssetToSpawn = nullptr;
		constexpr EObjectFlags ObjectFlags = RF_Transactional;
		ParentBakeActorName = *MakeUniqueObjectNameIfNeeded(DesiredLevel, AActor::StaticClass(), DesiredParentBakeActorName.ToString());
		
		FActorSpawnParameters SpawnParam;
		SpawnParam.ObjectFlags = ObjectFlags;
		SpawnParam.Name = ParentBakeActorName;
		ParentToActor = SpawnBakeActor(ActorFactory, AssetToSpawn, DesiredLevel, InIAC->GetComponentTransform(), HoudiniAssetComponent, BakeActorClass, SpawnParam);

		if (!IsValid(ParentToActor))
		{
			ParentToActor = nullptr;
		}
		else
		{
			OutBakeStats.NotifyObjectsCreated(ParentToActor->GetClass()->GetName(), 1);
			
			ParentToActor->SetActorLabel(ParentBakeActorName.ToString());
			OutActors.Emplace(FHoudiniEngineBakedActor(
				ParentToActor,
				DesiredParentBakeActorName,
				WorldOutlinerFolderPath,
				InOutputIndex,
				InOutputObjectIdentifier,
				nullptr,  // InBakedObject
				nullptr,  // InSourceObject
				nullptr,  // InBakedComponent
				PackageParams.BakeFolder,
				PackageParams));
		}
	}

	if (ParentToActor)
	{
		InBakedOutputObject.ActorBakeName = DesiredParentBakeActorName;
		InBakedOutputObject.Actor = FSoftObjectPath(ParentToActor).ToString();
	}
	
	// If we are baking in actor replacement mode, remove any previously baked instanced actors for this output
	if (bInReplaceActors && InBakedOutputObject.InstancedActors.Num() > 0)
	{
		UWorld* LevelWorld = DesiredLevel->GetWorld();
		if (IsValid(LevelWorld))
		{
			for (const FString& ActorPathStr : InBakedOutputObject.InstancedActors)
			{
				const FSoftObjectPath ActorPath(ActorPathStr);

				if (!ActorPath.IsValid())
					continue;
				
				AActor* Actor = Cast<AActor>(ActorPath.TryLoad());
				// Destroy Actor if it is valid and part of DesiredLevel
				if (IsValid(Actor) && Actor->GetLevel() == DesiredLevel)
				{
					// Just before we destroy the actor, rename it with a _DELETE suffix, so that we can re-use
					// its original name before garbage collection
					FHoudiniEngineUtils::SafeRenameActor(Actor, Actor->GetName() + TEXT("_DELETE"));
#if WITH_EDITOR
					LevelWorld->EditorDestroyActor(Actor, true);
#else
					LevelWorld->DestroyActor(Actor);
#endif
				}
			}
		}
	}

	// Empty and reserve enough space for new instanced actors
	InBakedOutputObject.InstancedActors.Empty(InIAC->GetInstancedActors().Num());

	// Iterates on all the instances of the IAC
	for (AActor* CurrentInstancedActor : InIAC->GetInstancedActors())
	{
		if (!IsValid(CurrentInstancedActor))
			continue;

		const FString NewNameStr = MakeUniqueObjectNameIfNeeded(DesiredLevel, CurrentInstancedActor->GetClass(), PackageParams.ObjectName);

		FTransform CurrentTransform = CurrentInstancedActor->GetTransform();
		AActor* NewActor = FHoudiniInstanceTranslator::SpawnInstanceActor(CurrentTransform, DesiredLevel, InIAC);
		if (!IsValid(NewActor))
			continue;

		OutBakeStats.NotifyObjectsCreated(NewActor->GetClass()->GetName(), 1);

		EditorUtilities::CopyActorProperties(CurrentInstancedActor, NewActor);

		FHoudiniEngineUtils::SafeRenameActor(NewActor, NewNameStr);

		SetOutlinerFolderPath(NewActor, InOutputObject, WorldOutlinerFolderPath);
		NewActor->SetActorTransform(CurrentTransform);

		if (ParentToActor)
			NewActor->AttachToActor(ParentToActor, FAttachmentTransformRules::KeepWorldTransform);

		InBakedOutputObject.InstancedActors.Add(FSoftObjectPath(NewActor).ToString());
		
		FHoudiniEngineBakedActor& OutputEntry = OutActors.Add_GetRef(FHoudiniEngineBakedActor(
			NewActor,
			*PackageParams.ObjectName,
			WorldOutlinerFolderPath,
			InOutputIndex,
			InOutputObjectIdentifier,
			nullptr,
			InstancedObject,
			nullptr,
			PackageParams.BakeFolder,
			PackageParams));
		OutputEntry.bInstancerOutput = true;
		OutputEntry.InstancerPackageParams = PackageParams;
	}

	// TODO:
	// Move Actors to DesiredLevel if needed??

	// If we are baking in replace mode, remove previously baked components/instancers
	if (bInReplaceActors && bInReplaceAssets)
	{
		const bool bInDestroyBakedComponent = true;
		const bool bInDestroyBakedInstancedActors = false;
		const bool bInDestroyBakedInstancedComponents = true;
		DestroyPreviousBakeOutput(
			InBakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
	}

	return true;
}

bool
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_MSIC(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	// const TArray<FHoudiniBakedOutput>& InAllBakedOutputs,
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	FHoudiniBakedOutputObject& InBakedOutputObject,
	const FTransform& InTransform,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	FHoudiniEngineBakedActor& OutBakedActorEntry,
	TArray<UPackage*>& OutPackagesToSave,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	FHoudiniEngineOutputStats& OutBakeStats,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	UHoudiniMeshSplitInstancerComponent * InMSIC = Cast<UHoudiniMeshSplitInstancerComponent>(InOutputObject.OutputComponent);
	if (!IsValid(InMSIC))
		return false;

	AActor * OwnerActor = InMSIC->GetOwner();
	if (!IsValid(OwnerActor))
		return false;

	UStaticMesh * StaticMesh = InMSIC->GetStaticMesh();
	if (!IsValid(StaticMesh))
		return false;

	// Certain SMC materials may need to be duplicated if we didn't generate the mesh object.
	TArray<UMaterialInterface *> DuplicatedMSICOverrideMaterials;

	
	UWorld* DesiredWorld = OwnerActor ? OwnerActor->GetWorld() : GWorld;
	const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
		EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;

	// Determine if the incoming mesh is temporary by looking for it in the mesh outputs. Populate mesh package params
	// for baking from it.
	// If not temporary set the ObjectName from the its package. (Also use this as a fallback default)
	FString ObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(StaticMesh);
	UStaticMesh* PreviousStaticMesh = Cast<UStaticMesh>(InBakedOutputObject.GetBakedObjectIfValid());
	UStaticMesh* BakedStaticMesh = nullptr;
	int32 MeshOutputIndex = INDEX_NONE;
	FHoudiniOutputObjectIdentifier MeshIdentifier;
	FHoudiniAttributeResolver MeshResolver;
	FHoudiniPackageParams MeshPackageParams;

	// See if the instanced static mesh is still a temporary Houdini created Static Mesh
	// If it is, we need to bake the StaticMesh first
	FHoudiniPackageParams InstancerPackageParams;
	// Configure FHoudiniAttributeResolver and fill the package params with resolved object name and bake folder.
	// The resolver is then also configured with the package params for subsequent resolving (level_path etc)
	FHoudiniAttributeResolver InstancerResolver;
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
		DesiredWorld, HoudiniAssetComponent, InOutputObjectIdentifier, InOutputObject, ObjectName,
		InstancerPackageParams, InstancerResolver, InBakeFolder.Path, AssetPackageReplaceMode);
	
	const bool bFoundMeshOutput = FindOutputObject(StaticMesh, EHoudiniOutputType::Mesh, InAllOutputs, MeshOutputIndex, MeshIdentifier);
	if (bFoundMeshOutput)
	{
		// Found the mesh in the mesh outputs, is temporary
		const FHoudiniOutputObject& MeshOutputObject = InAllOutputs[MeshOutputIndex]->GetOutputObjects().FindChecked(MeshIdentifier);
		FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
			DesiredWorld, HoudiniAssetComponent, MeshIdentifier, MeshOutputObject, ObjectName,
			MeshPackageParams, MeshResolver, InBakeFolder.Path, AssetPackageReplaceMode);
		// Update with resolved object name
		ObjectName = MeshPackageParams.ObjectName;

		// This will bake/duplicate the mesh if temporary, or return the input one if it is not
		BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
			StaticMesh, PreviousStaticMesh, MeshPackageParams, InAllOutputs, InBakedActors, InTempCookFolder.Path,
			OutPackagesToSave, InOutAlreadyBakedStaticMeshMap, InOutAlreadyBakedMaterialsMap, OutBakeStats);
	}
	else
	{
		BakedStaticMesh = StaticMesh;


		// We still need to duplicate materials, if they are temporary.
		TArray<UMaterialInterface *> Materials = InMSIC->GetOverrideMaterials();
		for (int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); ++MaterialIdx)
		{
			UMaterialInterface* MaterialInterface = Materials[MaterialIdx];
			if (!IsValid(MaterialInterface))
				continue;
	
			// Only duplicate the material if it is temporary
			if (IsObjectTemporary(MaterialInterface, EHoudiniOutputType::Invalid, InAllOutputs, InTempCookFolder.Path))
			{
				UMaterialInterface * DuplicatedMaterial = BakeSingleMaterialToPackage(
					MaterialInterface, InstancerPackageParams, OutPackagesToSave, InOutAlreadyBakedMaterialsMap, OutBakeStats);
				DuplicatedMSICOverrideMaterials.Add(DuplicatedMaterial);
			}
		}
	}

	// Update the baked output
	InBakedOutputObject.BakedObject = FSoftObjectPath(BakedStaticMesh).ToString();

	// Instancer name adds the split identifier (INSTANCERNUM_VARIATIONNUM)
	const FString InstancerName = ObjectName + "_instancer_" + InOutputObjectIdentifier.SplitIdentifier;
	const FName WorldOutlinerFolderPath = GetOutlinerFolderPath(
		InOutputObject, 
		FName(InFallbackWorldOutlinerFolder.IsEmpty() ? InstancerPackageParams.HoudiniAssetActorName : InFallbackWorldOutlinerFolder));



	// By default spawn in the current level unless specified via the unreal_level_path attribute
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();

	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
	{
		// Get the package path from the unreal_level_path attribute
		FString LevelPackagePath = InstancerResolver.ResolveFullLevelPath();

		bool bCreatedPackage = false;
		if (!FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
			LevelPackagePath,
			DesiredLevel,
			DesiredWorld,
			bCreatedPackage))
		{
			// TODO: LOG ERROR IF NO LEVEL
			return false;
		}

		// If we have created a level, add it to the packages to save
		// TODO: ? always add?
		if (bCreatedPackage && DesiredLevel)
		{
			OutBakeStats.NotifyPackageCreated(1);
			OutBakeStats.NotifyObjectsCreated(DesiredLevel->GetClass()->GetName(), 1);
			// We can now save the package again, and unload it.
			OutPackagesToSave.Add(DesiredLevel->GetOutermost());
		}
	}

	if (!DesiredLevel)
		return false;

	// Try to find the unreal_bake_actor, if specified
	FName BakeActorName;
	AActor* FoundActor = nullptr;
	bool bHasBakeActorName = false;
	bool bSpawnedActor = false;
	if (!FindUnrealBakeActor(InOutputObject, InBakedOutputObject, InBakedActors, DesiredLevel, *InstancerName, bInReplaceActors, InFallbackActor, FoundActor, bHasBakeActorName, BakeActorName))
		return false;

	if (!FoundActor)
	{
		// This is a split mesh instancer component - we will create a generic AActor with a bunch of SMC
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.OverrideLevel = DesiredLevel;
		SpawnInfo.ObjectFlags = RF_Transactional;

		if (!DesiredLevel->bUseExternalActors)
		{
			SpawnInfo.Name = FName(MakeUniqueObjectNameIfNeeded(DesiredLevel, AActor::StaticClass(), BakeActorName.ToString()));
		}
		SpawnInfo.bDeferConstruction = true;

		// Spawn the new Actor
		UClass* ActorClass = GetBakeActorClassOverride(InOutputObject);
		if (!ActorClass)
			ActorClass = AActor::StaticClass();
		FoundActor = DesiredLevel->OwningWorld->SpawnActor<AActor>(SpawnInfo);
		if (!IsValid(FoundActor))
			return false;
		bSpawnedActor = true;

		OutBakeStats.NotifyObjectsCreated(FoundActor->GetClass()->GetName(), 1);
		
		FHoudiniEngineRuntimeUtils::SetActorLabel(FoundActor, DesiredLevel->bUseExternalActors ? BakeActorName.ToString() : FoundActor->GetName());

		FoundActor->SetActorHiddenInGame(InMSIC->bHiddenInGame);
	}
	else
	{
		// If we are baking in replacement mode, remove the previous components (if they belong to FoundActor)
		for (const FString& PrevComponentPathStr : InBakedOutputObject.InstancedComponents)
		{
			const FSoftObjectPath PrevComponentPath(PrevComponentPathStr);

			if (!PrevComponentPath.IsValid())
				continue;
			
			UActorComponent* PrevComponent = Cast<UActorComponent>(PrevComponentPath.TryLoad());
			if (!IsValid(PrevComponent) || PrevComponent->GetOwner() != FoundActor)
				continue;

			RemovePreviouslyBakedComponent(PrevComponent);
		}

		const FString UniqueActorNameStr = MakeUniqueObjectNameIfNeeded(DesiredLevel, AActor::StaticClass(), BakeActorName.ToString(), FoundActor);
		RenameAndRelabelActor(FoundActor, UniqueActorNameStr, false);

		OutBakeStats.NotifyObjectsUpdated(FoundActor->GetClass()->GetName(), 1);
	}
	// The folder is named after the original actor and contains all generated actors
	SetOutlinerFolderPath(FoundActor, InOutputObject, WorldOutlinerFolderPath);

	// Get/create the actor's root component
	const bool bCreateIfMissing = true;
	USceneComponent* RootComponent = GetActorRootComponent(FoundActor, bCreateIfMissing);
	if (bSpawnedActor && IsValid(RootComponent))
		RootComponent->SetWorldTransform(InTransform);

	// Empty and reserve enough space in the baked components array for the new components
	InBakedOutputObject.InstancedComponents.Empty(InMSIC->GetInstances().Num());
	
	// Now add s SMC component for each of the SMC's instance
	for (UStaticMeshComponent* CurrentSMC : InMSIC->GetInstances())
	{
		if (!IsValid(CurrentSMC))
			continue;

		UStaticMeshComponent* NewSMC = DuplicateObject<UStaticMeshComponent>(
			CurrentSMC,
			FoundActor,
			FName(MakeUniqueObjectNameIfNeeded(FoundActor, CurrentSMC->GetClass(), CurrentSMC->GetName())));
		if (!IsValid(NewSMC))
			continue;

		OutBakeStats.NotifyObjectsCreated(NewSMC->GetClass()->GetName(), 1);
		
		InBakedOutputObject.InstancedComponents.Add(FSoftObjectPath(NewSMC).ToString());
		
		NewSMC->RegisterComponent();
		// NewSMC->SetupAttachment(nullptr);
		NewSMC->SetStaticMesh(BakedStaticMesh);
		FoundActor->AddInstanceComponent(NewSMC);
		NewSMC->SetWorldTransform(CurrentSMC->GetComponentTransform());

		if (DuplicatedMSICOverrideMaterials.Num() > 0)
		{
			UMaterialInterface * InstancerMaterial = DuplicatedMSICOverrideMaterials[0];
			if (InstancerMaterial)
			{
				NewSMC->OverrideMaterials.Empty();
				int32 MeshMaterialCount = BakedStaticMesh->GetStaticMaterials().Num();
				for (int32 Idx = 0; Idx < MeshMaterialCount; ++Idx)
					NewSMC->SetMaterial(Idx, InstancerMaterial);
			}
		}
		
		if (IsValid(RootComponent))
			NewSMC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

		// TODO: Do we need to copy properties here, we duplicated the component
		// // Copy properties from the existing component
		// CopyPropertyToNewActorAndComponent(FoundActor, NewSMC, CurrentSMC);
	}

	if (bSpawnedActor)
		FoundActor->FinishSpawning(InTransform);

	InBakedOutputObject.Actor = FSoftObjectPath(FoundActor).ToString();
	FHoudiniEngineBakedActor OutputEntry(
		FoundActor,
		BakeActorName,
		WorldOutlinerFolderPath,
		InOutputIndex,
		InOutputObjectIdentifier,
		BakedStaticMesh,
		StaticMesh,
		nullptr,
		MeshPackageParams.BakeFolder,
		MeshPackageParams);
	OutputEntry.bInstancerOutput = true;
	OutputEntry.InstancerPackageParams = InstancerPackageParams;

	// Postpone these calls to do them once per actor
	OutputEntry.bPostBakeProcessPostponed = true;

	OutBakedActorEntry = OutputEntry;

	// If we are baking in replace mode, remove previously baked components/instancers
	if (bInReplaceActors && bInReplaceAssets)
	{
		const bool bInDestroyBakedComponent = true;
		const bool bInDestroyBakedInstancedActors = true;
		const bool bInDestroyBakedInstancedComponents = false;
		DestroyPreviousBakeOutput(
			InBakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
	}

	return true;
}

bool
FHoudiniEngineBakeUtils::FindHGPO(
	const FHoudiniOutputObjectIdentifier& InIdentifier,
	const TArray<FHoudiniGeoPartObject>& InHGPOs,
	FHoudiniGeoPartObject const*& OutHGPO)
{
	// Find the HGPO that matches this output identifier
	const FHoudiniGeoPartObject* FoundHGPO = nullptr;
	for (auto & NextHGPO : InHGPOs) 
	{
		// We use Matches() here as it handles the case where the HDA was loaded,
		// which likely means that the the obj/geo/part ids dont match the output identifier
		if(InIdentifier.Matches(NextHGPO))
		{
			FoundHGPO = &NextHGPO;
			break;
		}
	}

	OutHGPO = FoundHGPO;
	return !OutHGPO;
}

void
FHoudiniEngineBakeUtils::GetTemporaryOutputObjectBakeName(
	const UObject* InObject,
	const FHoudiniOutputObject& InMeshOutputObject,
	FString& OutBakeName)
{
	// The bake name override has priority
	OutBakeName = InMeshOutputObject.BakeName;
	if (OutBakeName.IsEmpty())
	{
		FHoudiniAttributeResolver Resolver;
		Resolver.SetCachedAttributes(InMeshOutputObject.CachedAttributes);
		Resolver.SetTokensFromStringMap(InMeshOutputObject.CachedTokens);
		const FString DefaultObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(InObject);
		// The default output name (if not set via attributes) is {object_name}, which look for an object_name
		// key-value token
		if (!Resolver.GetCachedTokens().Contains(TEXT("object_name")))
			Resolver.SetToken(TEXT("object_name"), DefaultObjectName);
		OutBakeName = Resolver.ResolveOutputName();
		// const TArray<FHoudiniGeoPartObject>& HGPOs = InAllOutputs[MeshOutputIdx]->GetHoudiniGeoPartObjects();
		// const FHoudiniGeoPartObject* FoundHGPO = nullptr;
		// FindHGPO(MeshIdentifier, HGPOs, FoundHGPO);
		// // ... finally the part name
		// if (FoundHGPO && FoundHGPO->bHasCustomPartName)
		// 	OutBakeName = FoundHGPO->PartName;
		if (OutBakeName.IsEmpty())
			OutBakeName = DefaultObjectName;
	}
}

bool
FHoudiniEngineBakeUtils::GetTemporaryOutputObjectBakeName(
	const UObject* InObject,
	EHoudiniOutputType InOutputType,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	FString& OutBakeName)
{
	if (!IsValid(InObject))
		return false;
	
	OutBakeName.Empty();
	
	int32 MeshOutputIdx = INDEX_NONE;
	FHoudiniOutputObjectIdentifier MeshIdentifier;
	if (FindOutputObject(InObject, InOutputType, InAllOutputs, MeshOutputIdx, MeshIdentifier))
	{
		// Found the mesh, get its name
		const FHoudiniOutputObject& MeshOutputObject = InAllOutputs[MeshOutputIdx]->GetOutputObjects().FindChecked(MeshIdentifier);
		GetTemporaryOutputObjectBakeName(InObject, MeshOutputObject, OutBakeName);
		
		return true;
	}

	return false;
}

bool
FHoudiniEngineBakeUtils::BakeStaticMeshOutputObjectToActor(
	const UHoudiniAssetComponent* InHoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	const FHoudiniOutputObjectIdentifier& InIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	const TArray<FHoudiniGeoPartObject>& InHGPOs,
	const TMap<FHoudiniBakedOutputObjectIdentifier, FHoudiniBakedOutputObject>& InOldBakedOutputObjects,
	const FDirectoryPath& InTempCookFolder,
	const FDirectoryPath& InBakeFolder,
	const bool bInReplaceActors,
	const bool bInReplaceAssets,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder,
	const TArray<FHoudiniEngineBakedActor>& InAllBakedActors,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats,
	FHoudiniBakedOutputObject& OutBakedOutputObject,
	bool& bOutBakedToActor,
	FHoudiniEngineBakedActor& OutBakedActorEntry
)
{
	if (!InAllOutputs.IsValidIndex(InOutputIndex))
		return false;
	UHoudiniOutput* const InOutput = InAllOutputs[InOutputIndex];
	
	// Initialize the baked output object entry (use the previous bake's data, if available).
	if (InOldBakedOutputObjects.Contains(InIdentifier))
		OutBakedOutputObject = InOldBakedOutputObjects.FindChecked(InIdentifier);
	else
		OutBakedOutputObject = FHoudiniBakedOutputObject();

	UStaticMesh* StaticMesh = Cast<UStaticMesh>(InOutputObject.OutputObject);
	if (!IsValid(StaticMesh))
		return false;

	UStaticMeshComponent* InSMC = Cast<UStaticMeshComponent>(InOutputObject.OutputComponent);
	const bool bHasOutputSMC = IsValid(InSMC);
	if (!bHasOutputSMC && !InOutputObject.bIsImplicit)
		return false;

	// Find the HGPO that matches this output identifier
	const FHoudiniGeoPartObject* FoundHGPO = nullptr;
	FindHGPO(InIdentifier, InHGPOs, FoundHGPO);

	// We do not bake templated geos
	if (FoundHGPO && FoundHGPO->bIsTemplated)
		return true;

	const FString DefaultObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(StaticMesh);

	UWorld* DesiredWorld = InOutput ? InOutput->GetWorld() : GWorld;
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();

	FHoudiniPackageParams PackageParams;

	if (!ResolvePackageParams(
		InHoudiniAssetComponent,
		InOutput,
		InIdentifier,
		InOutputObject,
		DefaultObjectName,
		InBakeFolder,
		bInReplaceAssets,
		PackageParams,
		OutPackagesToSave))
	{
		return false;
	}
	
	// Bake the static mesh if it is still temporary
	UStaticMesh* BakedSM = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
		StaticMesh,
		Cast<UStaticMesh>(OutBakedOutputObject.GetBakedObjectIfValid()),
		PackageParams,
		InAllOutputs,
		InAllBakedActors,
		InTempCookFolder.Path,
		OutPackagesToSave,
		InOutAlreadyBakedStaticMeshMap,
		InOutAlreadyBakedMaterialsMap,
		OutBakeStats);

	if (!IsValid(BakedSM))
		return false;

	// Record the baked object
	OutBakedOutputObject.BakedObject = FSoftObjectPath(BakedSM).ToString();

	if (bHasOutputSMC)
	{
		const FName WorldOutlinerFolderPath = GetOutlinerFolderPath(
			InOutputObject,
			FName(InFallbackWorldOutlinerFolder.IsEmpty() ? PackageParams.HoudiniAssetActorName : InFallbackWorldOutlinerFolder));

		// Get the actor factory for the unreal_bake_actor_class attribute. If not set, use an empty actor.
		TSubclassOf<AActor> BakeActorClass = nullptr;
		UActorFactory* const Factory = GetActorFactory(InOutputObject, BakeActorClass, UActorFactoryEmptyActor::StaticClass(), BakedSM);

		// If we could not find a factory, we have to skip this output object
		if (!Factory)
			return false;

		// Make sure we have a level to spawn to
		if (!IsValid(DesiredLevel))
			return false;

		// Try to find the unreal_bake_actor, if specified
		FName BakeActorName;
		AActor* FoundActor = nullptr;
		bool bHasBakeActorName = false;
		if (!FindUnrealBakeActor(
				InOutputObject,
				OutBakedOutputObject,
				InAllBakedActors,
				DesiredLevel,
				*(PackageParams.ObjectName),
				bInReplaceActors,
				InFallbackActor,
				FoundActor,
				bHasBakeActorName,
				BakeActorName))
			return false;

		bool bCreatedNewActor = false;
		UStaticMeshComponent* SMC = nullptr;
		if (!FoundActor)
		{
			// Spawn the new actor
			FoundActor = SpawnBakeActor(Factory, BakedSM, DesiredLevel, InSMC->GetComponentTransform(), InHoudiniAssetComponent, BakeActorClass);
			if (!IsValid(FoundActor))
				return false;

			bCreatedNewActor = true;
			
			// Copy properties to new actor
			AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(FoundActor);
			if (IsValid(SMActor))
				SMC = SMActor->GetStaticMeshComponent();
		}
		
		if (!IsValid(SMC))
		{
			if (bInReplaceAssets && !bCreatedNewActor)
			{
				// Check if we have a previous bake component and that it belongs to FoundActor, if so, reuse it
				UStaticMeshComponent* PrevSMC = Cast<UStaticMeshComponent>(OutBakedOutputObject.GetBakedComponentIfValid());
				if (IsValid(PrevSMC) && (PrevSMC->GetOwner() == FoundActor))
				{
					SMC = PrevSMC;
				}
			}

			const bool bCreateIfMissing = true;
			USceneComponent* RootComponent = GetActorRootComponent(FoundActor, bCreateIfMissing);

			if (!IsValid(SMC))
			{
				// Create a new static mesh component on the existing actor
				SMC = NewObject<UStaticMeshComponent>(FoundActor, NAME_None, RF_Transactional);

				FoundActor->AddInstanceComponent(SMC);
				if (IsValid(RootComponent))
					SMC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
				else
					FoundActor->SetRootComponent(SMC);
				SMC->RegisterComponent();
			}
		}

		// We need to make a unique name for the actor, renaming an object on top of another is a fatal error
		const FString NewNameStr = MakeUniqueObjectNameIfNeeded(DesiredLevel, Factory->NewActorClass, BakeActorName.ToString(), FoundActor);
		RenameAndRelabelActor(FoundActor, NewNameStr, false);
		SetOutlinerFolderPath(FoundActor, InOutputObject, WorldOutlinerFolderPath);

		if (IsValid(SMC))
		{
			constexpr bool bCopyWorldTransform = true;
			CopyPropertyToNewActorAndComponent(FoundActor, SMC, InSMC, bCopyWorldTransform);
			SMC->SetStaticMesh(BakedSM);
			OutBakedOutputObject.BakedComponent = FSoftObjectPath(SMC).ToString();
		}
		
		OutBakedOutputObject.Actor = FSoftObjectPath(FoundActor).ToString();
		OutBakedActorEntry = FHoudiniEngineBakedActor(
			FoundActor, BakeActorName, WorldOutlinerFolderPath, InOutputIndex, InIdentifier, BakedSM, StaticMesh, SMC,
			PackageParams.BakeFolder, PackageParams);
		bOutBakedToActor = true;
	}
	else
	{
		// Implicit object, no component and no actor
		OutBakedOutputObject.BakedComponent = nullptr;
		OutBakedOutputObject.Actor = nullptr;
		bOutBakedToActor = false;
	}

	// If we are baking in replace mode, remove previously baked components/instancers
	if (bInReplaceActors && bInReplaceAssets)
	{
		constexpr bool bInDestroyBakedComponent = false;
		constexpr bool bInDestroyBakedInstancedActors = true;
		constexpr bool bInDestroyBakedInstancedComponents = true;
		DestroyPreviousBakeOutput(
			OutBakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
	}

	return true;
}

bool 
FHoudiniEngineBakeUtils::BakeStaticMeshOutputToActors(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	TArray<FHoudiniBakedOutput>& InBakedOutputs,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	TArray<UPackage*>& OutPackagesToSave,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	FHoudiniEngineOutputStats& OutBakeStats,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	// Check that index is not negative
	if (InOutputIndex < 0)
		return false;

	if (!InAllOutputs.IsValidIndex(InOutputIndex))
		return false;

	UHoudiniOutput* InOutput = InAllOutputs[InOutputIndex];
	if (!IsValid(InOutput))
		return false;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	const TArray<FHoudiniGeoPartObject>& HGPOs = InOutput->GetHoudiniGeoPartObjects();

	// Get the previous bake objects
	if (!InBakedOutputs.IsValidIndex(InOutputIndex))
		InBakedOutputs.SetNum(InOutputIndex + 1);
	
	const TMap<FHoudiniBakedOutputObjectIdentifier, FHoudiniBakedOutputObject>& OldBakedOutputObjects = InBakedOutputs[InOutputIndex].BakedOutputObjects;
	TMap<FHoudiniBakedOutputObjectIdentifier, FHoudiniBakedOutputObject> NewBakedOutputObjects;

	TArray<FHoudiniEngineBakedActor> AllBakedActors = InBakedActors;
	TArray<FHoudiniEngineBakedActor> NewBakedActors;

	// We need to bake invisible complex colliders first, since they are static meshes themselves but are referenced
	// by the main static mesh
	for (auto& Pair : OutputObjects)
	{
		const FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;

		const EHoudiniSplitType SplitType = FHoudiniMeshTranslator::GetSplitTypeFromSplitName(Identifier.SplitIdentifier);
		if (SplitType != EHoudiniSplitType::InvisibleComplexCollider)
			continue;
		
		const FHoudiniOutputObject& OutputObject = Pair.Value;

		FHoudiniBakedOutputObject BakedOutputObject;
		bool bBakedToActor = false;
		FHoudiniEngineBakedActor BakedActorEntry;
		if (BakeStaticMeshOutputObjectToActor(
				HoudiniAssetComponent,
				InOutputIndex,
				InAllOutputs,
				Identifier,
				OutputObject,
				HGPOs,
				OldBakedOutputObjects,
				InTempCookFolder,
				InBakeFolder,
				bInReplaceActors,
				bInReplaceAssets,
				InFallbackActor,
				InFallbackWorldOutlinerFolder,
				AllBakedActors,
				InOutAlreadyBakedStaticMeshMap,
				InOutAlreadyBakedMaterialsMap,
				OutPackagesToSave,
				OutBakeStats,
				BakedOutputObject,
				bBakedToActor,
				BakedActorEntry))
		{
			NewBakedOutputObjects.Add(Identifier, BakedOutputObject);
			if (bBakedToActor)
			{
				NewBakedActors.Add(BakedActorEntry);
				AllBakedActors.Add(BakedActorEntry);
			}
		}
	}
	
	// Now bake the other output objects
	for (auto& Pair : OutputObjects)
	{
		const FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;

		const EHoudiniSplitType SplitType = FHoudiniMeshTranslator::GetSplitTypeFromSplitName(Identifier.SplitIdentifier);
		if (SplitType == EHoudiniSplitType::InvisibleComplexCollider)
			continue;

		const FHoudiniOutputObject& OutputObject = Pair.Value;

		FHoudiniBakedOutputObject BakedOutputObject;
		bool bBakedToActor = false;
		FHoudiniEngineBakedActor BakedActorEntry;
		if (BakeStaticMeshOutputObjectToActor(
				HoudiniAssetComponent,
				InOutputIndex,
				InAllOutputs,
				Identifier,
				OutputObject,
				HGPOs,
				OldBakedOutputObjects,
				InTempCookFolder,
				InBakeFolder,
				bInReplaceActors,
				bInReplaceAssets,
				InFallbackActor,
				InFallbackWorldOutlinerFolder,
				AllBakedActors,
				InOutAlreadyBakedStaticMeshMap,
				InOutAlreadyBakedMaterialsMap,
				OutPackagesToSave,
				OutBakeStats,
				BakedOutputObject,
				bBakedToActor,
				BakedActorEntry))
		{
			NewBakedOutputObjects.Add(Identifier, BakedOutputObject);
			if (bBakedToActor)
			{
				NewBakedActors.Add(BakedActorEntry);
				AllBakedActors.Add(BakedActorEntry);
			}
		}
	}

	// Update the cached baked output data
	InBakedOutputs[InOutputIndex].BakedOutputObjects = NewBakedOutputObjects;

	OutActors = MoveTemp(NewBakedActors);

	return true;
}

bool FHoudiniEngineBakeUtils::ResolvePackageParams(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	UHoudiniOutput* InOutput, 
	const FHoudiniOutputObjectIdentifier& Identifier,
	const FHoudiniOutputObject& InOutputObject,
	const FString& DefaultObjectName, 
	const FDirectoryPath& InBakeFolder,
	const bool bInReplaceAssets,
	FHoudiniPackageParams& OutPackageParams,
	TArray<UPackage*>& OutPackagesToSave,
	const FString& InHoudiniAssetName,
	const FString& InHoudiniAssetActorName)
{
	FHoudiniAttributeResolver Resolver;
	
	UWorld* DesiredWorld = InOutput ? InOutput->GetWorld() : GWorld;
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();

	// Set the replace mode based on if we are doing a replacement or incremental asset bake
	const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
		EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
	// Configure FHoudiniAttributeResolver and fill the package params with resolved object name and bake folder.
	// The resolver is then also configured with the package params for subsequent resolving (level_path etc)
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
		DesiredWorld, HoudiniAssetComponent, Identifier, InOutputObject, DefaultObjectName,
		OutPackageParams, Resolver, InBakeFolder.Path, AssetPackageReplaceMode,
		InHoudiniAssetName, InHoudiniAssetActorName);

	// See if this output object has an unreal_level_path attribute specified
	// In which case, we need to create/find the desired level for baking instead of using the current one
	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
	{
		// Get the package path from the unreal_level_path attribute
		FString LevelPackagePath = Resolver.ResolveFullLevelPath();

		bool bCreatedPackage = false;
		if (!FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
			LevelPackagePath,
			DesiredLevel,
			DesiredWorld,
			bCreatedPackage))
		{
			// TODO: LOG ERROR IF NO LEVEL
			return false;
		}

		// If we have created a level, add it to the packages to save
		// TODO: ? always add the level to the packages to save?
		if (bCreatedPackage && DesiredLevel)
		{
			// We can now save the package again, and unload it.
			OutPackagesToSave.Add(DesiredLevel->GetOutermost());
		}
	}

	return true;
}

bool FHoudiniEngineBakeUtils::BakeGeometryCollectionOutputToActors(const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex, const TArray<UHoudiniOutput*>& InAllOutputs, TArray<FHoudiniBakedOutput>& InBakedOutputs,
	const FDirectoryPath& InBakeFolder, const FDirectoryPath& InTempCookFolder,
	bool bInReplaceActors, bool bInReplaceAssets, const TArray<FHoudiniEngineBakedActor>& InBakedActors, TArray<FHoudiniEngineBakedActor>& OutActors,
	TArray<UPackage*>& OutPackagesToSave, TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	FHoudiniEngineOutputStats& OutBakeStats, AActor* InFallbackActor, const FString& InFallbackWorldOutlinerFolder)
{
	// Check that index is not negative
	if (InOutputIndex < 0)
		return false;

	if (!InAllOutputs.IsValidIndex(InOutputIndex))
		return false;

	UHoudiniOutput* InOutput = InAllOutputs[InOutputIndex];
	if (!IsValid(InOutput))
		return false;

	if (!IsValid(HoudiniAssetComponent))
		return false;

	AActor* const OwnerActor = HoudiniAssetComponent->GetOwner();
	const FString HoudiniAssetActorName = IsValid(OwnerActor) ? OwnerActor->GetName() : FString();

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	const TArray<FHoudiniGeoPartObject>& HGPOs = InOutput->GetHoudiniGeoPartObjects();

	// Get the previous bake objects
	if (!InBakedOutputs.IsValidIndex(InOutputIndex))
		InBakedOutputs.SetNum(InOutputIndex + 1);

	const FString GCOutputName = InOutput->GetName();
	
	const TMap<FHoudiniBakedOutputObjectIdentifier, FHoudiniBakedOutputObject>& OldBakedOutputObjects = InBakedOutputs[InOutputIndex].BakedOutputObjects;
	TMap<FHoudiniBakedOutputObjectIdentifier, FHoudiniBakedOutputObject> NewBakedOutputObjects;

	// Map from old static mesh map to new baked static mesh
	TMap<FSoftObjectPath, UStaticMesh*> OldToNewStaticMeshMap;
	TMap<UMaterialInterface*, UMaterialInterface*> OldToNewMaterialMap;

	// Need to make sure that all geometry collection meshes are generated before we generate the geometry collection.
	for (auto & Output : InAllOutputs)
	{
		if (!FHoudiniGeometryCollectionTranslator::IsGeometryCollectionMesh(Output))
		{
			continue;
		}

		for (auto Pair : Output->GetOutputObjects())
		{
			const FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;
			const FHoudiniOutputObject& OutputObject = Pair.Value;

			if (OutputObject.GeometryCollectionPieceName != GCOutputName)
			{
				continue;
			}

			UStaticMesh * StaticMesh = Cast<UStaticMesh>(OutputObject.OutputObject);
			if (!IsValid(StaticMesh))
			{
				continue;
			}

			// Add a new baked output object entry and update it with the previous bake's data, if available
			FHoudiniBakedOutputObject& BakedOutputObject = NewBakedOutputObjects.Add(Identifier);
			if (OldBakedOutputObjects.Contains(Identifier))
				BakedOutputObject = OldBakedOutputObjects.FindChecked(Identifier);
			
			const FString DefaultObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(StaticMesh);

			FHoudiniPackageParams PackageParams;
			
			if (!ResolvePackageParams(
				HoudiniAssetComponent,
				InOutput,
				Identifier,
				OutputObject,
				DefaultObjectName,
				InBakeFolder,
				bInReplaceAssets,
				PackageParams,
				OutPackagesToSave))
			{
				continue;
			}

			UStaticMesh* BakedSM = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
				StaticMesh,
				Cast<UStaticMesh>(BakedOutputObject.GetBakedObjectIfValid()),
				PackageParams,
				InAllOutputs,
				InBakedActors,
				InTempCookFolder.Path,
				OutPackagesToSave,
				InOutAlreadyBakedStaticMeshMap,
				InOutAlreadyBakedMaterialsMap,
				OutBakeStats);

			if (!IsValid(BakedSM))
				continue;

			BakedOutputObject.BakedObject = FSoftObjectPath(BakedSM).ToString();

			// If we are baking in replace mode, remove previously baked components/instancers
			if (bInReplaceActors && bInReplaceAssets)
			{
				const bool bInDestroyBakedComponent = false;
				const bool bInDestroyBakedInstancedActors = true;
				const bool bInDestroyBakedInstancedComponents = true;
				DestroyPreviousBakeOutput(
					BakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
			}

			OldToNewStaticMeshMap.Add(FSoftObjectPath(StaticMesh), BakedSM);

			const TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();
			const TArray<FStaticMaterial>& BakedStaticMaterials = BakedSM->GetStaticMaterials();
			for (int32 i = 0; i < StaticMaterials.Num(); i++)
			{
				if (i >= BakedStaticMaterials.Num())
					continue;

				OldToNewMaterialMap.Add(StaticMaterials[i].MaterialInterface, BakedStaticMaterials[i].MaterialInterface);
			}
		}
	}

	TArray<FHoudiniEngineBakedActor> AllBakedActors = InBakedActors;
	TArray<FHoudiniEngineBakedActor> NewBakedActors;
	for (auto& Pair : OutputObjects)
	{
		const FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;
		const FHoudiniOutputObject& OutputObject = Pair.Value;

		// Add a new baked output object entry and update it with the previous bake's data, if available
		FHoudiniBakedOutputObject& BakedOutputObject = NewBakedOutputObjects.Add(Identifier);
		if (OldBakedOutputObjects.Contains(Identifier))
			BakedOutputObject = OldBakedOutputObjects.FindChecked(Identifier);


		AGeometryCollectionActor* InGeometryCollectionActor = Cast<AGeometryCollectionActor>(OutputObject.OutputObject);

		if (!IsValid(InGeometryCollectionActor))
			return false;

		UGeometryCollectionComponent * InGeometryCollectionComponent = InGeometryCollectionActor->GetGeometryCollectionComponent();
		if (!IsValid(InGeometryCollectionComponent))
			return false;

		FGeometryCollectionEdit GeometryCollectionEdit = InGeometryCollectionActor->GetGeometryCollectionComponent()->EditRestCollection(GeometryCollection::EEditUpdate::RestPhysicsDynamic);
		UGeometryCollection* InGeometryCollection = GeometryCollectionEdit.GetRestCollection();

		if (!IsValid(InGeometryCollection))
			return false;
		
		
		// Find the HGPO that matches this output identifier
		const FHoudiniGeoPartObject* FoundHGPO = nullptr;
		FindHGPO(Identifier, HGPOs, FoundHGPO);

		// We do not bake templated geos
		if (FoundHGPO && FoundHGPO->bIsTemplated)
			continue;

		const FString DefaultObjectName = HoudiniAssetActorName + Identifier.SplitIdentifier;

		UWorld* DesiredWorld = InOutput ? InOutput->GetWorld() : GWorld;
		ULevel* DesiredLevel = DesiredWorld->GetCurrentLevel();

		FHoudiniPackageParams PackageParams;

		if (!ResolvePackageParams(
			HoudiniAssetComponent,
			InOutput,
			Identifier,
			OutputObject,
			DefaultObjectName,
			InBakeFolder,
			bInReplaceAssets,
			PackageParams,
			OutPackagesToSave))
		{
			continue;
		}

		const FName WorldOutlinerFolderPath = GetOutlinerFolderPath(
			OutputObject,
			FName(InFallbackWorldOutlinerFolder.IsEmpty() ? PackageParams.HoudiniAssetActorName : InFallbackWorldOutlinerFolder));
		
		// Bake the static mesh if it is still temporary
		UGeometryCollection* BakedGC = FHoudiniEngineBakeUtils::DuplicateGeometryCollectionAndCreatePackageIfNeeded(
			InGeometryCollection,
			Cast<UGeometryCollection>(BakedOutputObject.GetBakedObjectIfValid()),
			PackageParams,
			InAllOutputs,
			AllBakedActors,
			InTempCookFolder.Path,
			OldToNewStaticMeshMap,
			OldToNewMaterialMap,
			OutPackagesToSave,
			OutBakeStats);

		if (!IsValid(BakedGC))
			continue;

		// Record the baked object
		BakedOutputObject.BakedObject = FSoftObjectPath(BakedGC).ToString();

		// Make sure we have a level to spawn to
		if (!IsValid(DesiredLevel))
			continue;

		// Try to find the unreal_bake_actor, if specified
		FName BakeActorName;
		AActor* FoundActor = nullptr;
		bool bHasBakeActorName = false;
		if (!FindUnrealBakeActor(OutputObject, BakedOutputObject, AllBakedActors, DesiredLevel, *(PackageParams.ObjectName), bInReplaceActors, InFallbackActor, FoundActor, bHasBakeActorName, BakeActorName))
			return false;

		AGeometryCollectionActor* NewGCActor = nullptr;
		UGeometryCollectionComponent* NewGCC = nullptr;
		if (!FoundActor)
		{

			FoundActor = FHoudiniGeometryCollectionTranslator::CreateNewGeometryActor(DesiredWorld, BakeActorName.ToString(), InGeometryCollectionComponent->GetComponentTransform());
			// Spawn the new actor
			if (!IsValid(FoundActor))
				continue;

			OutBakeStats.NotifyObjectsCreated(FoundActor->GetClass()->GetName(), 1);

			// Copy properties to new actor
			NewGCActor = Cast<AGeometryCollectionActor>(FoundActor);
			if (!IsValid(NewGCActor))
				continue;

			NewGCC = NewGCActor->GetGeometryCollectionComponent();
		}
		else
		{
			if (bInReplaceAssets)
			{
				// Check if we have a previous bake component and that it belongs to FoundActor, if so, reuse it
				UGeometryCollectionComponent* PrevGCC = Cast<UGeometryCollectionComponent>(BakedOutputObject.GetBakedComponentIfValid());
				if (IsValid(PrevGCC) && (PrevGCC->GetOwner() == FoundActor))
				{
					NewGCC = PrevGCC;
				}
			}

			const bool bCreateIfMissing = true;
			USceneComponent* RootComponent = GetActorRootComponent(FoundActor, bCreateIfMissing);

			if (!IsValid(NewGCC))
			{
				// Create a new static mesh component on the existing actor
				NewGCC = NewObject<UGeometryCollectionComponent>(FoundActor, NAME_None, RF_Transactional);

				FoundActor->AddInstanceComponent(NewGCC);
				if (IsValid(RootComponent))
					NewGCC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
				else
					FoundActor->SetRootComponent(NewGCC);
				NewGCC->RegisterComponent();
			}

			NewGCActor = Cast<AGeometryCollectionActor>(FoundActor);

			OutBakeStats.NotifyObjectsUpdated(FoundActor->GetClass()->GetName(), 1);
		}


		// We need to make a unique name for the actor, renaming an object on top of another is a fatal error
		const FString NewNameStr = MakeUniqueObjectNameIfNeeded(DesiredLevel, AGeometryCollectionActor::StaticClass(), BakeActorName.ToString(), FoundActor);
		RenameAndRelabelActor(FoundActor, NewNameStr, false);
		SetOutlinerFolderPath(FoundActor, OutputObject, WorldOutlinerFolderPath);

		if (IsValid(NewGCC))
		{
			const bool bCopyWorldTransform = true;
			CopyPropertyToNewGeometryCollectionActorAndComponent(NewGCActor, NewGCC, InGeometryCollectionComponent, bCopyWorldTransform);
			
			NewGCC->SetRestCollection(BakedGC);
			BakedOutputObject.BakedComponent = FSoftObjectPath(NewGCC).ToString();
		}
		
		BakedOutputObject.Actor = FSoftObjectPath(FoundActor).ToString();
		const FHoudiniEngineBakedActor& BakedActorEntry = AllBakedActors.Add_GetRef(FHoudiniEngineBakedActor(
			FoundActor, BakeActorName, WorldOutlinerFolderPath, InOutputIndex, Identifier, BakedGC, InGeometryCollection, InGeometryCollectionComponent,
			PackageParams.BakeFolder, PackageParams));
		NewBakedActors.Add(BakedActorEntry);

		// If we are baking in replace mode, remove previously baked components/instancers
		if (bInReplaceActors && bInReplaceAssets)
		{
			const bool bInDestroyBakedComponent = false;
			const bool bInDestroyBakedInstancedActors = true;
			const bool bInDestroyBakedInstancedComponents = true;
			DestroyPreviousBakeOutput(
				BakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
		}

	}

	// Update the cached baked output data
	InBakedOutputs[InOutputIndex].BakedOutputObjects = NewBakedOutputObjects;

	OutActors = MoveTemp(NewBakedActors);

	return true;
}

bool
FHoudiniEngineBakeUtils::BakeHoudiniCurveOutputToActors(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	TArray<FHoudiniBakedOutput>& InBakedOutputs,
	const FDirectoryPath& InBakeFolder,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	FHoudiniEngineOutputStats& OutBakeStats,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder) 
{
	// Check that index is not negative
	if (InOutputIndex < 0)
		return false;
	
	if (!InAllOutputs.IsValidIndex(InOutputIndex))
		return false;
	
	UHoudiniOutput* const Output = InAllOutputs[InOutputIndex];
	if (!IsValid(Output))
		return false;

	if (!IsValid(HoudiniAssetComponent))
		return false;

	AActor* OwnerActor = HoudiniAssetComponent->GetOwner();
	const FString HoudiniAssetActorName = IsValid(OwnerActor) ? OwnerActor->GetName() : FString();

	TArray<UPackage*> PackagesToSave;
	
	// Find the previous baked output data for this output index. If an entry
	// does not exist, create entries up to and including this output index
	if (!InBakedOutputs.IsValidIndex(InOutputIndex))
		InBakedOutputs.SetNum(InOutputIndex + 1);

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
	FHoudiniBakedOutput& BakedOutput = InBakedOutputs[InOutputIndex];
	const TMap<FHoudiniBakedOutputObjectIdentifier, FHoudiniBakedOutputObject>& OldBakedOutputObjects = BakedOutput.BakedOutputObjects;
	TMap<FHoudiniBakedOutputObjectIdentifier, FHoudiniBakedOutputObject> NewBakedOutputObjects;
	
	const TArray<FHoudiniGeoPartObject> & HGPOs = Output->GetHoudiniGeoPartObjects();

	TArray<FHoudiniEngineBakedActor> AllBakedActors = InBakedActors;
	TArray<FHoudiniEngineBakedActor> NewBakedActors;

	for (auto & Pair : OutputObjects) 
	{
		
		FHoudiniOutputObject& OutputObject = Pair.Value;
		USplineComponent* SplineComponent = Cast<USplineComponent>(OutputObject.OutputComponent);
		if (!IsValid(SplineComponent))
			continue;
		
		const FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;
		FHoudiniBakedOutputObject& BakedOutputObject = NewBakedOutputObjects.Add(Identifier);
		if (OldBakedOutputObjects.Contains(Identifier))
			BakedOutputObject = OldBakedOutputObjects.FindChecked(Identifier);

		// TODO: FIX ME!! May not work 100%
		const FHoudiniGeoPartObject* FoundHGPO = nullptr;
		for (auto & NextHGPO : HGPOs)
		{
			if (Identifier.GeoId == NextHGPO.GeoId &&
				Identifier.ObjectId == NextHGPO.ObjectId &&
				Identifier.PartId == NextHGPO.PartId)
			{
				FoundHGPO = &NextHGPO;
				break;
			}
		}

		if (!FoundHGPO)
			continue;

		const FString DefaultObjectName = HoudiniAssetActorName + "_" + SplineComponent->GetName();

		FHoudiniPackageParams PackageParams;
		// Set the replace mode based on if we are doing a replacement or incremental asset bake
		const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
			EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;

		// Configure FHoudiniAttributeResolver and fill the package params with resolved object name and bake folder.
		// The resolver is then also configured with the package params for subsequent resolving (level_path etc)
		FHoudiniAttributeResolver Resolver;
		UWorld* const DesiredWorld = SplineComponent ? SplineComponent->GetWorld() : GWorld;
		FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
			DesiredWorld, HoudiniAssetComponent, Identifier, OutputObject, DefaultObjectName,
			PackageParams, Resolver, InBakeFolder.Path, AssetPackageReplaceMode);

		FHoudiniEngineBakedActor OutputBakedActor;
		BakeCurve(
			HoudiniAssetComponent, OutputObject, BakedOutputObject, PackageParams, Resolver, bInReplaceActors, bInReplaceAssets,
			AllBakedActors, OutputBakedActor, PackagesToSave, OutBakeStats, InFallbackActor, InFallbackWorldOutlinerFolder);

		OutputBakedActor.OutputIndex = InOutputIndex;
		OutputBakedActor.OutputObjectIdentifier = Identifier;

		AllBakedActors.Add(OutputBakedActor);
		NewBakedActors.Add(OutputBakedActor);
	}

	// Update the cached bake output results
	BakedOutput.BakedOutputObjects = NewBakedOutputObjects;

	OutActors = MoveTemp(NewBakedActors);
	
	SaveBakedPackages(PackagesToSave);

	return true;
}

bool 
FHoudiniEngineBakeUtils::CopyActorContentsToBlueprint(AActor * InActor, UBlueprint * OutBlueprint, bool bInRenameComponentsWithInvalidNames) 
{
	if (!IsValid(InActor))
		return false;

	if (!IsValid(OutBlueprint))
		return false;

	if (bInRenameComponentsWithInvalidNames)
	{
		for (UActorComponent* const Comp : InActor->GetInstanceComponents())
		{
			if (!IsValid(Comp))
				continue;

			// If the component name would not be a valid variable name in the BP, rename it
			if (!FComponentEditorUtils::IsValidVariableNameString(Comp, Comp->GetName()))
			{
				FString NewComponentName = FComponentEditorUtils::GenerateValidVariableName(Comp->GetClass(), Comp->GetOwner());
				NewComponentName = MakeUniqueObjectNameIfNeeded(Comp->GetOuter(), Comp->GetClass(), NewComponentName, Comp);
				if (NewComponentName != Comp->GetName())
					Comp->Rename(*NewComponentName);
			}
		}
	}

	if (InActor->GetInstanceComponents().Num() > 0)
		FKismetEditorUtilities::AddComponentsToBlueprint(
			OutBlueprint,
			InActor->GetInstanceComponents());

	if (OutBlueprint->GeneratedClass)
	{
		AActor * CDO = Cast< AActor >(OutBlueprint->GeneratedClass->GetDefaultObject());
		if (!IsValid(CDO))
			return false;

		const auto CopyOptions = (EditorUtilities::ECopyOptions::Type)
			(EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties |
				EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances);

		EditorUtilities::CopyActorProperties(InActor, CDO, CopyOptions);

		USceneComponent * Scene = CDO->GetRootComponent();
		if (IsValid(Scene))
		{
			Scene->SetRelativeLocation(FVector::ZeroVector);
			Scene->SetRelativeRotation(FRotator::ZeroRotator);

			// Clear out the attachment info after having copied the properties from the source actor
			Scene->SetupAttachment(nullptr);
			while (true)
			{
				const int32 ChildCount = Scene->GetAttachChildren().Num();
				if (ChildCount < 1)
					break;

				USceneComponent * Component = Scene->GetAttachChildren()[ChildCount - 1];
				if (IsValid(Component))
					Component->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			}
			check(Scene->GetAttachChildren().Num() == 0);

			// Ensure the light mass information is cleaned up
			Scene->InvalidateLightingCache();

			// Copy relative scale from source to target.
			if (USceneComponent* SrcSceneRoot = InActor->GetRootComponent())
			{
				Scene->SetRelativeScale3D_Direct(SrcSceneRoot->GetRelativeScale3D());
			}
		}
	}

	// Compile our blueprint and notify asset system about blueprint.
	FBlueprintEditorUtils::MarkBlueprintAsModified(OutBlueprint);
	//FKismetEditorUtilities::CompileBlueprint(OutBlueprint);
	//FAssetRegistryModule::AssetCreated(OutBlueprint);

	return true;
}

bool 
FHoudiniEngineBakeUtils::BakeBlueprints(UHoudiniAssetComponent* HoudiniAssetComponent, bool bInReplaceAssets, bool bInRecenterBakedActors) 
{
	FHoudiniEngineOutputStats BakeStats;
	TArray<UPackage*> PackagesToSave;
	TArray<UBlueprint*> Blueprints;
	const bool bSuccess = BakeBlueprints(HoudiniAssetComponent, bInReplaceAssets, bInRecenterBakedActors, BakeStats, Blueprints, PackagesToSave);
	if (!bSuccess)
	{
		// TODO: ?
		HOUDINI_LOG_WARNING(TEXT("Errors while baking to blueprints."));
	}

	// Compile the new/updated blueprints
	for (UBlueprint* const Blueprint : Blueprints)
	{
		if (!IsValid(Blueprint))
			continue;
		
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	}
	FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

	// Sync the CB to the baked objects
	if(GEditor && Blueprints.Num() > 0)
	{
		TArray<UObject*> Assets;
		Assets.Reserve(Blueprints.Num());
		for (UBlueprint* Blueprint : Blueprints)
		{
			Assets.Add(Blueprint);
		}
		GEditor->SyncBrowserToObjects(Assets);
	}

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakeStats.NumPackagesCreated, BakeStats.NumPackagesUpdated } );
		FHoudiniEngine::Get().FinishTaskSlateNotification( FText::FromString(Msg) );
	}
	
	TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	// Broadcast that the bake is complete
	HoudiniAssetComponent->HandleOnPostBake(bSuccess);

	return bSuccess;
}

bool 
FHoudiniEngineBakeUtils::BakeBlueprints(
	UHoudiniAssetComponent* HoudiniAssetComponent,
	bool bInReplaceAssets,
	bool bInRecenterBakedActors,
	FHoudiniEngineOutputStats& InBakeStats,
	TArray<UBlueprint*>& OutBlueprints,
	TArray<UPackage*>& OutPackagesToSave)
{
	if (!IsValid(HoudiniAssetComponent))
		return false;

	AActor* OwnerActor = HoudiniAssetComponent->GetOwner();
	const bool bIsOwnerActorValid = IsValid(OwnerActor);
	
	TArray<FHoudiniEngineBakedActor> Actors;

	// Don't process outputs that are not supported in blueprints
	TArray<EHoudiniOutputType> OutputsToBake = {
		EHoudiniOutputType::Mesh,
		EHoudiniOutputType::Instancer,
		EHoudiniOutputType::Curve,
		EHoudiniOutputType::GeometryCollection
	};
	TArray<EHoudiniInstancerComponentType> InstancerComponentTypesToBake = {
		EHoudiniInstancerComponentType::StaticMeshComponent,
		EHoudiniInstancerComponentType::InstancedStaticMeshComponent,
		EHoudiniInstancerComponentType::MeshSplitInstancerComponent,
		EHoudiniInstancerComponentType::FoliageAsHierarchicalInstancedStaticMeshComponent,
		EHoudiniInstancerComponentType::GeoemtryCollectionComponent,
	};
	// When baking blueprints we always create new actors since they are deleted from the world once copied into the
	// blueprint
	const bool bReplaceActors = false;
	bool bBakeSuccess = BakeHoudiniActorToActors(
		HoudiniAssetComponent,
		bReplaceActors,
		bInReplaceAssets,
		Actors,
		OutPackagesToSave,
		InBakeStats,
		&OutputsToBake,
		&InstancerComponentTypesToBake);
	if (!bBakeSuccess)
	{
		HOUDINI_LOG_ERROR(TEXT("Could not create output actors for baking to blueprint."));
		return false;
	}

	// Get the previous baked outputs
	TArray<FHoudiniBakedOutput>& BakedOutputs = HoudiniAssetComponent->GetBakedOutputs();

	bBakeSuccess = BakeBlueprintsFromBakedActors(
		Actors,
		bInRecenterBakedActors,
		bInReplaceAssets,
		IsValid(HoudiniAssetComponent->GetHoudiniAsset()) ? HoudiniAssetComponent->GetHoudiniAsset()->GetName() : FString(), 
		bIsOwnerActorValid ? OwnerActor->GetName() : FString(),
		HoudiniAssetComponent->BakeFolder,
		&BakedOutputs,
		nullptr,
		OutBlueprints,
		OutPackagesToSave,
		InBakeStats);

	return bBakeSuccess;
}

UStaticMesh* 
FHoudiniEngineBakeUtils::BakeStaticMesh(
	UStaticMesh * StaticMesh,
	const FHoudiniPackageParams& PackageParams,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	const FDirectoryPath& InTempCookFolder,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	FHoudiniEngineOutputStats& OutBakeStats) 
{
	if (!IsValid(StaticMesh))
		return nullptr;

	TArray<UPackage*> PackagesToSave;
	TArray<UHoudiniOutput*> Outputs;
	const TArray<FHoudiniEngineBakedActor> BakedResults;
	UStaticMesh* BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
		StaticMesh, nullptr, PackageParams, InAllOutputs, BakedResults, InTempCookFolder.Path, PackagesToSave,
		InOutAlreadyBakedStaticMeshMap, InOutAlreadyBakedMaterialsMap, OutBakeStats);

	if (BakedStaticMesh) 
	{
		FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

		// Sync the CB to the baked objects
		if(GEditor)
		{
			TArray<UObject*> Objects;
			Objects.Add(BakedStaticMesh);
			GEditor->SyncBrowserToObjects(Objects);
		}
	}

	return BakedStaticMesh;
}

bool
FHoudiniEngineBakeUtils::BakeLandscape(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	TArray<FHoudiniBakedOutput>& InBakedOutputs,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	const FString& BakePath,
	FHoudiniEngineOutputStats& BakeStats
	)
{
	// Check that index is not negative
	if (InOutputIndex < 0)
		return false;
	
	if (!InAllOutputs.IsValidIndex(InOutputIndex))
		return false;
	
	UHoudiniOutput* const Output = InAllOutputs[InOutputIndex]; 
	if (!IsValid(Output))
		return false;

	// Find the previous baked output data for this output index. If an entry
	// does not exist, create entries up to and including this output index
	if (!InBakedOutputs.IsValidIndex(InOutputIndex))
		InBakedOutputs.SetNum(InOutputIndex + 1);

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
	FHoudiniBakedOutput& BakedOutput = InBakedOutputs[InOutputIndex];
	const TMap<FHoudiniBakedOutputObjectIdentifier, FHoudiniBakedOutputObject>& OldBakedOutputObjects = BakedOutput.BakedOutputObjects;
	TMap<FHoudiniBakedOutputObjectIdentifier, FHoudiniBakedOutputObject> NewBakedOutputObjects;
	TArray<UPackage*> PackagesToSave;
	TArray<UWorld*> LandscapeWorldsToUpdate;

	FHoudiniPackageParams PackageParams;
	
	for (auto& Elem : OutputObjects)
	{
		const FHoudiniOutputObjectIdentifier& ObjectIdentifier = Elem.Key;
		FHoudiniOutputObject& OutputObject = Elem.Value;
		FHoudiniBakedOutputObject& BakedOutputObject = NewBakedOutputObjects.Add(ObjectIdentifier);
		if (OldBakedOutputObjects.Contains(ObjectIdentifier))
			BakedOutputObject = OldBakedOutputObjects.FindChecked(ObjectIdentifier);
		
		// Populate the package params for baking this output object.
		if (!IsValid(OutputObject.OutputObject))
			continue;

		if (!OutputObject.OutputObject->IsA<UHoudiniLandscapePtr>())
			continue;

		UHoudiniLandscapePtr* LandscapePtr = Cast<UHoudiniLandscapePtr>(OutputObject.OutputObject);
		ALandscapeProxy* Landscape = LandscapePtr->GetRawPtr();
		if (!IsValid(Landscape))
			continue;

		FString ObjectName = Landscape->GetName();

		// Set the replace mode based on if we are doing a replacement or incremental asset bake
		const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
			EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;

		// Configure FHoudiniAttributeResolver and fill the package params with resolved object name and bake folder.
		// The resolver is then also configured with the package params for subsequent resolving (level_path etc)
		FHoudiniAttributeResolver Resolver;
		UWorld* const DesiredWorld = Landscape ? Landscape->GetWorld() : GWorld;
		FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
			DesiredWorld, HoudiniAssetComponent, ObjectIdentifier, OutputObject, ObjectName,
			PackageParams, Resolver, BakePath, AssetPackageReplaceMode);

		BakeLandscapeObject(OutputObject, BakedOutputObject, bInReplaceActors, bInReplaceAssets,
			PackageParams, Resolver, LandscapeWorldsToUpdate, PackagesToSave, BakeStats);
	}

	// Update the cached baked output data
	BakedOutput.BakedOutputObjects = NewBakedOutputObjects;
	
	if (PackagesToSave.Num() > 0)
	{
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, true, false);
	}

	for(UWorld* LandscapeWorld : LandscapeWorldsToUpdate)
	{
		if (!LandscapeWorld)
			continue;

		AWorldSettings* WorldSettings = LandscapeWorld->GetWorldSettings();
		if (WorldSettings && WorldSettings->bEnableWorldComposition)
		{
			// We force a PostEditChangeProperty event which will trigger world composition to properly detect
			// newly created maps, if enabled.
			FHoudiniEngineRuntimeUtils::DoPostEditChangeProperty(WorldSettings, "bEnableWorldComposition");
		} 
	}

	if (PackagesToSave.Num() > 0)
	{
		// These packages were either created during the Bake process or they weren't
		// loaded in the first place so be sure to unload them again to preserve their "state".
		
		TArray<UPackage*> PackagesToUnload;
		for (UPackage* Package : PackagesToSave)
		{
			if (!Package->IsDirty())
				PackagesToUnload.Add(Package);
		}
		UPackageTools::UnloadPackages(PackagesToUnload);
	}

#if WITH_EDITOR
	FEditorDelegates::RefreshLevelBrowser.Broadcast();
	FEditorDelegates::RefreshAllBrowsers.Broadcast();
#endif

	return true;
}

bool
FHoudiniEngineBakeUtils::BakeLandscapeObject(
	FHoudiniOutputObject& InOutputObject,
	FHoudiniBakedOutputObject& InBakedOutputObject,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	FHoudiniPackageParams& PackageParams,
	FHoudiniAttributeResolver& InResolver,
	TArray<UWorld*>& WorldsToUpdate,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& BakeStats)
{
	UHoudiniLandscapePtr* LandscapePointer = Cast<UHoudiniLandscapePtr>(InOutputObject.OutputObject);
	if (!LandscapePointer)
		return false;
	
	ALandscapeProxy* TileActor = LandscapePointer->GetRawPtr();
	if (!TileActor)
		return false;

	// Fetch the previous bake's pointer and proxy (if available)
	ALandscapeProxy* PreviousTileActor = Cast<ALandscapeProxy>(InBakedOutputObject.GetBakedObjectIfValid());
	
	UWorld* TileWorld = TileActor->GetWorld();
	ULevel* TileLevel = TileActor->GetLevel();

	ULandscapeInfo::RecreateLandscapeInfo(TileWorld, true);

	// If this actor has a shared landscape, ensure the shared landscape gets detached from the HAC
	// and has the appropriate name.
	ALandscape* SharedLandscapeActor = TileActor->GetLandscapeActor();
	check(SharedLandscapeActor);

	// Fetch the previous bake's shared landscape actor (if available)
	ALandscape* PreviousSharedLandscapeActor = nullptr;
	if (IsValid(PreviousTileActor))
		PreviousSharedLandscapeActor = PreviousTileActor->GetLandscapeActor();
	
	const bool bHasSharedLandscape = SharedLandscapeActor != TileActor;
	bool bHasPreviousSharedLandscape = PreviousSharedLandscapeActor && PreviousSharedLandscapeActor != PreviousTileActor;
	if (bHasPreviousSharedLandscape)
	{
		// Ignore the previous shared landscape if the world's are different
		// Typically in baking we treat completely different asset/output names in a bake as detached from the "previous" bake
		if (PreviousSharedLandscapeActor->GetWorld() != SharedLandscapeActor->GetWorld())
			bHasPreviousSharedLandscape = false;
	}
	
	bool bLandscapeReplaced = false;
	if (bHasSharedLandscape)
	{
		// If we are baking in replace mode and we have a previous shared landscape actor, use the name of that
		// actor
		FString SharedLandscapeName = InResolver.ResolveAttribute(
			HAPI_UNREAL_ATTRIB_LANDSCAPE_SHARED_ACTOR_NAME,
			SharedLandscapeActor->GetName());

		// If the shared landscape is still attached, or it's base name does not match the desired name, "bake" it
		AActor* const AttachedParent = SharedLandscapeActor->GetAttachParentActor();
		if (AttachedParent || SharedLandscapeActor->GetFName().GetPlainNameString() != SharedLandscapeName)
		{
			if (bHasPreviousSharedLandscape && bInReplaceActors &&
					PreviousSharedLandscapeActor->GetFName().GetPlainNameString() == SharedLandscapeName)
			{
				SharedLandscapeName = PreviousSharedLandscapeActor->GetName(); 
			}
			else if (!bInReplaceActors)
			{
				// If we are not baking in replacement mode, create a unique name if the name is already in use
				SharedLandscapeName = MakeUniqueObjectNameIfNeeded(
					SharedLandscapeActor->GetOuter(), SharedLandscapeActor->GetClass(), *SharedLandscapeName, SharedLandscapeActor);
			}
			
			if (SharedLandscapeActor->GetName() != SharedLandscapeName)
			{
				AActor* FoundActor = nullptr;
				ALandscape* ExistingLandscape = FHoudiniEngineUtils::FindOrRenameInvalidActor<ALandscape>(TileWorld, SharedLandscapeName, FoundActor);
				if (ExistingLandscape && bInReplaceActors)
				{
					// Even though we found an existing landscape with the desired type, we're just going to destroy/replace
					// it for now.
					FHoudiniEngineUtils::RenameToUniqueActor(ExistingLandscape, SharedLandscapeName+"_0");
					ExistingLandscape->Destroy();
					bLandscapeReplaced = true;
				}

				// Fix name of shared landscape
				FHoudiniEngineUtils::SafeRenameActor(SharedLandscapeActor, *SharedLandscapeName);
			}
			
			SharedLandscapeActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			WorldsToUpdate.AddUnique(SharedLandscapeActor->GetWorld());
		}
	}

	// Find the world where the landscape tile should be placed.

	TArray<ALandscapeProxy*> ValidLandscapes;

	FString ActorName = InResolver.ResolveOutputName();

	// If the unreal_level_path was not specified, then fallback to the tile world's package
	FString PackagePath = TileWorld->GetPackage() ? TileWorld->GetPackage()->GetPathName() : FString();
	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
		PackagePath = InResolver.ResolveFullLevelPath();
	
	// Get the previous baked actor (if available) name, but only if it is in the
	// same target level, and it's plain name (no numeric suffix) matches ActorName
	// In replacement mode we'll then replace the previous tile actor.
	if (bInReplaceActors && IsValid(PreviousTileActor))
	{
		UPackage* PreviousPackage = PreviousTileActor->GetPackage();
		if (IsValid(PreviousPackage) && PreviousPackage->GetPathName() == PackagePath &&
			PreviousTileActor->GetFName().GetPlainNameString() == ActorName)
		{
			ActorName = PreviousTileActor->GetName();
		}
	}

	bool bCreatedPackage = false;
	UWorld* TargetWorld = nullptr;
	ULevel* TargetLevel = nullptr;
	ALandscapeProxy* TargetActor = FHoudiniLandscapeTranslator::FindExistingLandscapeActor_Bake(
		TileActor->GetWorld(),
		nullptr, //unused in bake mode
		ValidLandscapes,//unused in bake mode
		-1, //unused in bake mode
		-1, //unused in bake mode
		ActorName,
		PackagePath,
		TargetWorld,
		TargetLevel,
		bCreatedPackage
		);

	check(TargetLevel)
	check(TargetWorld)
	
	if (TargetActor && TargetActor != TileActor)
	{
		if (bInReplaceActors && (!PreviousTileActor || PreviousTileActor == TargetActor))
		{
			// We found an target matching the name that we want. For now, rename it and then nuke it, so that
			// at the very least we can spawn a new actor with the desired name. At a later stage we'll implement
			// a content update, if possible.
			FHoudiniEngineUtils::RenameToUniqueActor(TargetActor, ActorName + TEXT("_0"));
			TargetActor->Destroy();
		}
		else
		{
			// incremental, keep existing actor and create a unique name for the new one
			ActorName = MakeUniqueObjectNameIfNeeded(TargetActor->GetOuter(), TargetActor->GetClass(), ActorName, TileActor);
		}
		TargetActor = nullptr;
	}

	if (TargetLevel != TileActor->GetLevel())
	{
		bool bLevelInWorld = TileWorld->ContainsLevel(TargetLevel);
		ALandscape* SharedLandscape = TileActor->GetLandscapeActor();
		ULandscapeInfo* LandscapeInfo = TileActor->GetLandscapeInfo();
		
		check(LandscapeInfo);
		
		// We can now move the current landscape to the new world / level
		if (TileActor->GetClass()->IsChildOf<ALandscapeStreamingProxy>())
		{
			// We can only move streaming proxies to sublevels for now.
			TArray<AActor*> ActorsToMove = {TileActor};

			// NOTE: We can't use the LandscapeInfo's MoveComponentsToLevel directly since there is the assumption
			//       that the target level is already loaded in the current world where the shared landscape exists.
			//       In other words the TargetLevel->GetWorld() and Landscape->GetWorld() should be the same world.
			//       To make matters worse, we can't manually load a newly created level into the editor as a _sublevel_
			//       since the whole WorldBrowser / LevelCollection API is private and the LevelEditor subsystem doesn't
			//       provide any functions to achieve this.
			ALandscapeProxy* NewLandscapeProxy = MoveLandscapeComponentsToLevel(LandscapeInfo, TileActor->LandscapeComponents, TargetLevel);
			
			// We have now moved the landscape components into the new level. We can (hopefully) safely delete the
			// old tile actor.
			TileActor->Destroy();

			TargetLevel->MarkPackageDirty();

			TileActor = NewLandscapeProxy;
		}
		else
		{
			// Move the landscape actor to the target level
			constexpr bool bWarnAboutReferences = false;
			constexpr bool bWarnAboutRenaming = false;
			constexpr bool bMoveAllOrFail = true;
			TArray<AActor*> MovedActors;
			if (UEditorLevelUtils::MoveActorsToLevel({TileActor}, TargetLevel, bWarnAboutReferences, bWarnAboutRenaming, bMoveAllOrFail, &MovedActors) > 0)
			{
				// The function returned > 0 but no actors were moved
				if (MovedActors.Num() <= 0)
					return false;
				// The moved actor isn't a landscape?
				TileActor = Cast<ALandscapeProxy>(MovedActors[0]);
				if (!IsValid(TileActor))
					return false;
			}
			else
			{
				// The move failed
				return false;
			}
		}
		// Ensure target level bounds are up to date
		if (IsValid(TargetLevel) && TargetLevel->LevelBoundsActor.IsValid())
		{
			TargetLevel->LevelBoundsActor->MarkLevelBoundsDirty();
		}
	}
	else
	{
		// Ensure the landscape actor is detached.
		TileActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
	
	UPackage * CreatedPackage = TargetLevel->GetOutermost();
	TMap<UMaterialInterface *, UMaterialInterface *> AlreadyBakedMaterialsMap;
	
	// Replace materials
	if (TileActor->LandscapeMaterial)
	{
		UMaterialInterface * DuplicatedMaterial = BakeSingleMaterialToPackage(
			TileActor->LandscapeMaterial, PackageParams, OutPackagesToSave, AlreadyBakedMaterialsMap, BakeStats);
		TileActor->LandscapeMaterial = DuplicatedMaterial;
	}

	if (TileActor->LandscapeHoleMaterial)
	{
		UMaterialInterface * DuplicatedMaterial = BakeSingleMaterialToPackage(
			TileActor->LandscapeHoleMaterial, PackageParams, OutPackagesToSave, AlreadyBakedMaterialsMap, BakeStats);
		TileActor->LandscapeHoleMaterial = DuplicatedMaterial;
	}

	// Ensure the tile actor has the desired name.
	FHoudiniEngineUtils::SafeRenameActor(TileActor, ActorName);

	if (TileActor->GetClass()->IsChildOf(ALandscape::StaticClass()))
	{
		// This is not a shared landscape. Be sure to update this landscape's world when
		// baking is done.
		WorldsToUpdate.AddUnique(TileActor->GetWorld());
	}

	if (bCreatedPackage)
	{
		// We can now save the package again, and unload it.		
		OutPackagesToSave.Add(TargetLevel->GetOutermost());
	}

	// Record the landscape in the baked output object via a new UHoudiniLandscapePtr
	// UHoudiniLandscapePtr* BakedLandscapePtr = NewObject<UHoudiniLandscapePtr>(LandscapePointer->GetOuter());
	// if (IsValid(BakedLandscapePtr))
	// {
	// 	BakedLandscapePtr->SetSoftPtr(TileActor);
		InBakedOutputObject.BakedObject = FSoftObjectPath(TileActor).ToString();
	// }
	// else
	// {
	// 	InBakedOutputObject.BakedObject = nullptr;
	// }

	// Bake the landscape layer uassets
	ULandscapeInfo* const LandscapeInfo = TileActor->GetLandscapeInfo();
	if (IsValid(LandscapeInfo) && LandscapeInfo->Layers.Num() > 0)
	{
		TSet<ULandscapeLayerInfoObject*> TempLayers;
		const int32 NumLayers = LandscapeInfo->Layers.Num();
		TempLayers.Reserve(NumLayers);
		for (int32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
		{
			const FLandscapeInfoLayerSettings& Layer = LandscapeInfo->Layers[LayerIndex];
			if (!IsValid(Layer.LayerInfoObj))
				continue;
				
			if (!IsObjectInTempFolder(Layer.LayerInfoObj, PackageParams.TempCookFolder))
				continue;
				
			if (!TempLayers.Contains(Layer.LayerInfoObj))
				TempLayers.Add(Layer.LayerInfoObj);
		}

		// Setup package params to duplicate each layer
		FHoudiniPackageParams LayerPackageParams = PackageParams;
		const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
			EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
		LayerPackageParams.ReplaceMode = AssetPackageReplaceMode;

		// Determine the final bake name of the "owning" landscape (shared landscape in tiled mode, or just the
		// landscape actor itself in non-tiled mode
		FString OwningLandscapeActorBakeName;
		if (bHasSharedLandscape && IsValid(SharedLandscapeActor))
		{
			SharedLandscapeActor->GetName(OwningLandscapeActorBakeName);
		}
		else
		{
			TileActor->GetName(OwningLandscapeActorBakeName);
		}

		// Keep track of the landscape layers we are baking this time around, and replace in the baked output object
		// at the end.
		TMap<FName, FString> ThisBakedLandscapeLayers;
		
		// Bake/duplicate temp layers and replace temp layers via LandscapeInfo
		for (ULandscapeLayerInfoObject* const LayerInfo : TempLayers)
		{
			const FString SanitizedLayerName = ObjectTools::SanitizeObjectName(LayerInfo->LayerName.ToString());
			LayerPackageParams.SplitStr = SanitizedLayerName;
			LayerPackageParams.ObjectName = OwningLandscapeActorBakeName + TEXT("_layer_") + SanitizedLayerName;

			// Get the previously baked layer info for this layer, if any
			ULandscapeLayerInfoObject* const PreviousBakedLayerInfo = InBakedOutputObject.GetLandscapeLayerInfoIfValid(
				LayerInfo->LayerName);

			// If our name is the base name (no number) of the previous, then we can fetch the bake counter for
			// replacement / incrementing from it
			int32 BakeCounter = 0;
			if (IsValid(PreviousBakedLayerInfo) && LayerPackageParams.MatchesPackagePathNameExcludingBakeCounter(PreviousBakedLayerInfo))
			{
				// Get the bake counter from the previous bake
				FHoudiniPackageParams::GetBakeCounterFromBakedAsset(PreviousBakedLayerInfo, BakeCounter);
			}

			FString LayerPackageName;
			UPackage* const LayerPackage = LayerPackageParams.CreatePackageForObject(LayerPackageName, BakeCounter);
			if (IsValid(LayerPackage))
			{
				BakeStats.NotifyPackageCreated(1);
				ULandscapeLayerInfoObject* BakedLayer = DuplicateObject<ULandscapeLayerInfoObject>(
					LayerInfo, LayerPackage, *LayerPackageName);
				if (IsValid(BakedLayer))
				{
					BakeStats.NotifyObjectsCreated(BakedLayer->GetClass()->GetName(), 1);
					OutPackagesToSave.Add(LayerPackage);
					
					// Trigger update of the Layer Info
					BakedLayer->PreEditChange(nullptr);
					BakedLayer->PostEditChange();
					BakedLayer->MarkPackageDirty();

					// Mark the package dirty...
					LayerPackage->MarkPackageDirty();

					LandscapeInfo->ReplaceLayer(LayerInfo, BakedLayer);

					// Record as the new baked result for the LayerName
					ThisBakedLandscapeLayers.Add(LayerInfo->LayerName, FSoftObjectPath(BakedLayer).ToString());
				}
			}
		}

		// Update the baked landscape layers in InBakedOutputObject
		InBakedOutputObject.LandscapeLayers = ThisBakedLandscapeLayers;
	}

	// Remove the landscape from the InOutputObject since it should no longer be used/reused/updated by temp cooks
	InOutputObject.OutputObject = nullptr;
	
	DestroyPreviousBakeOutput(InBakedOutputObject, true, true, true);

	// ----------------------------------------------------
	// Collect baking stats
	// ----------------------------------------------------
	if (bLandscapeReplaced)
		BakeStats.NotifyObjectsReplaced(EHoudiniOutputType::Landscape, 1);
	else
		BakeStats.NotifyObjectsCreated(EHoudiniOutputType::Landscape, 1);

	if (bCreatedPackage)
		BakeStats.NotifyPackageCreated(1);
	else
		if (TileLevel != TargetLevel)
			BakeStats.NotifyPackageUpdated(1);

	return true;
}

UStaticMesh * 
FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
	UStaticMesh * InStaticMesh,
	UStaticMesh * InPreviousBakeStaticMesh,
	const FHoudiniPackageParams &PackageParams,
	const TArray<UHoudiniOutput*>& InParentOutputs, 
	const TArray<FHoudiniEngineBakedActor>& InCurrentBakedActors,
	const FString& InTemporaryCookFolder,
	TArray<UPackage*> & OutCreatedPackages,
	TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	FHoudiniEngineOutputStats& OutBakeStats) 
{
	if (!IsValid(InStaticMesh))
		return nullptr;

	const bool bIsTemporaryStaticMesh = IsObjectTemporary(InStaticMesh, EHoudiniOutputType::Mesh, InParentOutputs, InTemporaryCookFolder);
	if (!bIsTemporaryStaticMesh)
	{
		// The Static Mesh is not a temporary one/already baked, we can simply reuse it
		// instead of duplicating it
		return InStaticMesh;
	}

	UStaticMesh** AlreadyBakedSM = InOutAlreadyBakedStaticMeshMap.Find(InStaticMesh);
	if (AlreadyBakedSM && IsValid(*AlreadyBakedSM))
		return *AlreadyBakedSM;

	// Look for InStaticMesh as the SourceObject in InCurrentBakedActors (it could have already been baked along with
	// a previous output: instancers etc)
	for (const FHoudiniEngineBakedActor& BakedActor : InCurrentBakedActors)
	{
		if (BakedActor.SourceObject == InStaticMesh && IsValid(BakedActor.BakedObject)
			&& BakedActor.BakedObject->IsA(InStaticMesh->GetClass()))
		{
			// We have found a bake result where InStaticMesh was the source object and we have a valid BakedObject
			// of a compatible class
			return Cast<UStaticMesh>(BakedActor.BakedObject);
		}
	}

	// InStaticMesh is temporary and we didn't find a baked version of it in our current bake output, we need to bake it
	
	// If we have a previously baked static mesh, get the bake counter from it so that both replace and increment
	// is consistent with the bake counter
	int32 BakeCounter = 0;
	bool bPreviousBakeStaticMeshValid = IsValid(InPreviousBakeStaticMesh);
	TArray<FStaticMaterial> PreviousBakeMaterials;
	if (bPreviousBakeStaticMeshValid)
	{
		bPreviousBakeStaticMeshValid = PackageParams.MatchesPackagePathNameExcludingBakeCounter(InPreviousBakeStaticMesh);
		if (bPreviousBakeStaticMeshValid)
		{
			PackageParams.GetBakeCounterFromBakedAsset(InPreviousBakeStaticMesh, BakeCounter);
			PreviousBakeMaterials = InPreviousBakeStaticMesh->GetStaticMaterials();
		}
	}
	FString CreatedPackageName;
	UPackage* MeshPackage = PackageParams.CreatePackageForObject(CreatedPackageName, BakeCounter);
	if (!IsValid(MeshPackage))
		return nullptr;
	OutBakeStats.NotifyPackageCreated(1);
	OutCreatedPackages.Add(MeshPackage);

	// We need to be sure the package has been fully loaded before calling DuplicateObject
	if (!MeshPackage->IsFullyLoaded())
	{
		FlushAsyncLoading();
		if (!MeshPackage->GetOuter())
		{
			MeshPackage->FullyLoad();
		}
		else
		{
			MeshPackage->GetOutermost()->FullyLoad();
		}
	}

	// If the a UStaticMesh with that name already exists then detach it from all of its components before replacing
	// it so that its render resources can be safely replaced/updated, and then reattach it
	UStaticMesh * DuplicatedStaticMesh = nullptr;
	UStaticMesh* ExistingMesh = FindObject<UStaticMesh>(MeshPackage, *CreatedPackageName);
	bool bFoundExistingMesh = false;
	if (IsValid(ExistingMesh))
	{
		FStaticMeshComponentRecreateRenderStateContext SMRecreateContext(ExistingMesh);	
		DuplicatedStaticMesh = DuplicateObject<UStaticMesh>(InStaticMesh, MeshPackage, *CreatedPackageName);
		bFoundExistingMesh = true;
		OutBakeStats.NotifyObjectsReplaced(UStaticMesh::StaticClass()->GetName(), 1);
	}
	else
	{
		DuplicatedStaticMesh = DuplicateObject<UStaticMesh>(InStaticMesh, MeshPackage, *CreatedPackageName);
		OutBakeStats.NotifyObjectsUpdated(UStaticMesh::StaticClass()->GetName(), 1);
	}
	
	if (!IsValid(DuplicatedStaticMesh))
		return nullptr;

	InOutAlreadyBakedStaticMeshMap.Add(InStaticMesh, DuplicatedStaticMesh);

	// Add meta information.
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MeshPackage, DuplicatedStaticMesh,
		HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MeshPackage, DuplicatedStaticMesh,
		HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *CreatedPackageName);

	// See if we need to duplicate materials and textures.
	TArray<FStaticMaterial>DuplicatedMaterials;
	TArray<FStaticMaterial>& Materials = DuplicatedStaticMesh->GetStaticMaterials();
	for (int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); ++MaterialIdx)
	{
		UMaterialInterface* MaterialInterface = Materials[MaterialIdx].MaterialInterface;
		if (!IsValid(MaterialInterface))
			continue;

		// Only duplicate the material if it is temporary
		if (IsObjectTemporary(MaterialInterface, EHoudiniOutputType::Invalid, InParentOutputs, InTemporaryCookFolder))
		{
			UPackage * MaterialPackage = Cast<UPackage>(MaterialInterface->GetOuter());
			if (IsValid(MaterialPackage))
			{
				FString MaterialName;
				if (FHoudiniEngineBakeUtils::GetHoudiniGeneratedNameFromMetaInformation(
					MeshPackage, DuplicatedStaticMesh, MaterialName))
				{
					MaterialName = MaterialName + "_Material" + FString::FromInt(MaterialIdx + 1);

					// We only deal with materials.
					if (!MaterialInterface->IsA(UMaterial::StaticClass()) && !MaterialInterface->IsA(UMaterialInstance::StaticClass()))
					{
						continue;
					}
					
					UMaterialInterface * Material = MaterialInterface;

					if (IsValid(Material))
					{
						// Look for a previous bake material at this index
						UMaterialInterface* PreviousBakeMaterial = nullptr;
						if (bPreviousBakeStaticMeshValid && PreviousBakeMaterials.IsValidIndex(MaterialIdx))
						{
							PreviousBakeMaterial = Cast<UMaterialInterface>(PreviousBakeMaterials[MaterialIdx].MaterialInterface);
						}
						// Duplicate material resource.
						UMaterialInterface * DuplicatedMaterial = FHoudiniEngineBakeUtils::DuplicateMaterialAndCreatePackage(
							Material, PreviousBakeMaterial, MaterialName, PackageParams, OutCreatedPackages, InOutAlreadyBakedMaterialsMap,
							OutBakeStats);

						if (!IsValid(DuplicatedMaterial))
							continue;

						// Store duplicated material.
						FStaticMaterial DupeStaticMaterial = Materials[MaterialIdx];
						DupeStaticMaterial.MaterialInterface = DuplicatedMaterial;
						DuplicatedMaterials.Add(DupeStaticMaterial);
						continue;
					}
				}
			}
		}
		
		// We can simply reuse the source material
		DuplicatedMaterials.Add(Materials[MaterialIdx]);
	}
		
	// Assign duplicated materials.
	DuplicatedStaticMesh->SetStaticMaterials(DuplicatedMaterials);

	// Check if the complex collision mesh of the SM is a temporary SM, if so try to get its baked version
	if (IsValid(DuplicatedStaticMesh->ComplexCollisionMesh) &&
			IsObjectTemporary(DuplicatedStaticMesh->ComplexCollisionMesh, EHoudiniOutputType::Mesh, InParentOutputs, InTemporaryCookFolder))
	{
		UStaticMesh** BakedComplexCollisionMesh = InOutAlreadyBakedStaticMeshMap.Find(DuplicatedStaticMesh->ComplexCollisionMesh);
		if (BakedComplexCollisionMesh && IsValid(*BakedComplexCollisionMesh))
		{
			DuplicatedStaticMesh->ComplexCollisionMesh = *BakedComplexCollisionMesh;
		}
	}

	// Notify registry that we have created a new duplicate mesh.
	if (!bFoundExistingMesh)
		FAssetRegistryModule::AssetCreated(DuplicatedStaticMesh);

	// Dirty the static mesh package.
	DuplicatedStaticMesh->MarkPackageDirty();

	return DuplicatedStaticMesh;
}

UGeometryCollection* FHoudiniEngineBakeUtils::DuplicateGeometryCollectionAndCreatePackageIfNeeded(
	UGeometryCollection* InGeometryCollection, UGeometryCollection* InPreviousBakeGeometryCollection,
	const FHoudiniPackageParams& PackageParams, const TArray<UHoudiniOutput*>& InParentOutputs,
	const TArray<FHoudiniEngineBakedActor>& InCurrentBakedActors, const FString& InTemporaryCookFolder,
	const TMap<FSoftObjectPath, UStaticMesh*>& InOldToNewStaticMesh,
	const TMap<UMaterialInterface*, UMaterialInterface*>& InOldToNewMaterialMap,
	TArray<UPackage*>& OutCreatedPackages, FHoudiniEngineOutputStats& OutBakeStats)
{
	if (!IsValid(InGeometryCollection))
		return nullptr;

	const bool bIsTemporaryStaticMesh = IsObjectTemporary(InGeometryCollection, EHoudiniOutputType::GeometryCollection, InParentOutputs, InTemporaryCookFolder);
	if (!bIsTemporaryStaticMesh)
	{
		// The output is not a temporary one/already baked, we can simply reuse it
		// instead of duplicating it
		return InGeometryCollection;
	}

	// Look for InGeometryCollection as the SourceObject in InCurrentBakedActors (it could have already been baked along with
	// a previous output: instancers etc)
	for (const FHoudiniEngineBakedActor& BakedActor : InCurrentBakedActors)
	{
		if (BakedActor.SourceObject == InGeometryCollection && IsValid(BakedActor.BakedObject)
			&& BakedActor.BakedObject->IsA(InGeometryCollection->GetClass()))
		{
			// We have found a bake result where InStaticMesh was the source object and we have a valid BakedObject
			// of a compatible class
			return Cast<UGeometryCollection>(BakedActor.BakedObject);
		}
	}

	// InGeometryCollection is temporary and we didn't find a baked version of it in our current bake output, we need to bake it
	
	// If we have a previously baked static mesh, get the bake counter from it so that both replace and increment
	// is consistent with the bake counter

	
	int32 BakeCounter = 0;
	bool bPreviousBakeStaticMeshValid = IsValid(InPreviousBakeGeometryCollection);
	TArray<UMaterialInterface*> PreviousBakeMaterials;
	if (bPreviousBakeStaticMeshValid)
	{
		bPreviousBakeStaticMeshValid = PackageParams.MatchesPackagePathNameExcludingBakeCounter(InPreviousBakeGeometryCollection);
		if (bPreviousBakeStaticMeshValid)
		{
			PackageParams.GetBakeCounterFromBakedAsset(InPreviousBakeGeometryCollection, BakeCounter);
			PreviousBakeMaterials = InPreviousBakeGeometryCollection->Materials;
		}
	}
	FString CreatedPackageName;
	UPackage* MeshPackage = PackageParams.CreatePackageForObject(CreatedPackageName, BakeCounter);
	if (!IsValid(MeshPackage))
		return nullptr;
	OutBakeStats.NotifyPackageCreated(1);
	OutCreatedPackages.Add(MeshPackage);

	// We need to be sure the package has been fully loaded before calling DuplicateObject
	if (!MeshPackage->IsFullyLoaded())
	{
		FlushAsyncLoading();
		if (!MeshPackage->GetOuter())
		{
			MeshPackage->FullyLoad();
		}
		else
		{
			MeshPackage->GetOutermost()->FullyLoad();
		}
	}

	// If the a UGeometryCollection with that name already exists then detach it from all of its components before replacing
	// it so that its render resources can be safely replaced/updated, and then reattach it
	UGeometryCollection * DuplicatedGeometryCollection = nullptr;
	UGeometryCollection * ExistingGeometryCollection = FindObject<UGeometryCollection>(MeshPackage, *CreatedPackageName);
	bool bFoundExistingObject = false;
	if (IsValid(ExistingGeometryCollection))
	{
		DuplicatedGeometryCollection = DuplicateObject<UGeometryCollection>(InGeometryCollection, MeshPackage, *CreatedPackageName);
		bFoundExistingObject = true;
	}
	else
	{
		DuplicatedGeometryCollection = DuplicateObject<UGeometryCollection>(InGeometryCollection, MeshPackage, *CreatedPackageName);
	}
	
	if (!IsValid(DuplicatedGeometryCollection))
		return nullptr;

	OutBakeStats.NotifyObjectsCreated(DuplicatedGeometryCollection->GetClass()->GetName(), 1);

	// Add meta information.
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MeshPackage, DuplicatedGeometryCollection,
		HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MeshPackage, DuplicatedGeometryCollection,
		HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *CreatedPackageName);

	for (FGeometryCollectionSource& Source : DuplicatedGeometryCollection->GeometrySource)
	{
		if (!InOldToNewStaticMesh.Contains(Source.SourceGeometryObject))
		{
			continue;
		}

		UStaticMesh * BakedSM = InOldToNewStaticMesh[Source.SourceGeometryObject];
		
		Source.SourceGeometryObject = FSoftObjectPath(BakedSM);
		Source.SourceMaterial.Empty();
		
		for (const auto& Material : BakedSM->GetStaticMaterials())
		{
			Source.SourceMaterial.Add(Material.MaterialInterface);
		}
	}


	// Duplicate geometry collection materials	
	for (int32 i = 0; i < DuplicatedGeometryCollection->Materials.Num(); i++)
	{
		if (!InOldToNewMaterialMap.Contains(DuplicatedGeometryCollection->Materials[i]))
		{
			continue;
		}

		DuplicatedGeometryCollection->Materials[i] = InOldToNewMaterialMap[DuplicatedGeometryCollection->Materials[i]];
	}
		

	// Notify registry that we have created a new duplicate mesh.
	if (!bFoundExistingObject)
		FAssetRegistryModule::AssetCreated(DuplicatedGeometryCollection);

	// Dirty the static mesh package.
	DuplicatedGeometryCollection->MarkPackageDirty();

	return DuplicatedGeometryCollection;
}

ALandscapeProxy* 
FHoudiniEngineBakeUtils::BakeHeightfield(
	ALandscapeProxy * InLandscapeProxy,
	const FHoudiniPackageParams & PackageParams,
	const EHoudiniLandscapeOutputBakeType & LandscapeOutputBakeType,
	FHoudiniEngineOutputStats& OutBakeStats)
{
	if (!IsValid(InLandscapeProxy))
		return nullptr;

	const FString & BakeFolder = PackageParams.BakeFolder;
	const FString & AssetName = PackageParams.HoudiniAssetName;
	TArray<UPackage*> PackagesToSave;

	switch (LandscapeOutputBakeType) 
	{
		case EHoudiniLandscapeOutputBakeType::Detachment:
		{
			// Detach the landscape from the Houdini Asset Actor
			InLandscapeProxy->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
		}
		break;
		case EHoudiniLandscapeOutputBakeType::BakeToImage:
		{
			// Create heightmap image to the bake folder
			ULandscapeInfo * InLandscapeInfo = InLandscapeProxy->GetLandscapeInfo();
			if (!IsValid(InLandscapeInfo))
				return nullptr;
		
			// bake to image must use absoluate path, 
			// and the file name has a file extension (.png)
			FString BakeFolderInFullPath = BakeFolder;

			// Figure absolute path,
			if (!BakeFolderInFullPath.EndsWith("/"))
				BakeFolderInFullPath += "/";

			if (BakeFolderInFullPath.StartsWith("/Game"))
				BakeFolderInFullPath = BakeFolderInFullPath.Mid(5, BakeFolderInFullPath.Len() - 5);

			if (BakeFolderInFullPath.StartsWith("/"))
				BakeFolderInFullPath = BakeFolderInFullPath.Mid(1, BakeFolderInFullPath.Len() - 1);

			FString FullPath = FPaths::ProjectContentDir() + BakeFolderInFullPath + AssetName + "_" + InLandscapeProxy->GetName() + ".png";

			InLandscapeInfo->ExportHeightmap(FullPath);

			// TODO:
			// We should update this to have an asset/package..
		}
		break;
		case EHoudiniLandscapeOutputBakeType::BakeToWorld:
		{
			ULandscapeInfo * InLandscapeInfo = InLandscapeProxy->GetLandscapeInfo();
			if (!IsValid(InLandscapeInfo))
				return nullptr;

			// 0.  Get Landscape Data //
			
			// Extract landscape height data
			TArray<uint16> InLandscapeHeightData;
			int32 XSize, YSize;
			FVector Min, Max;
			if (!FUnrealLandscapeTranslator::GetLandscapeData(InLandscapeProxy, InLandscapeHeightData, XSize, YSize, Min, Max))
				return nullptr;

			// Extract landscape Layers data
			TArray<FLandscapeImportLayerInfo> InLandscapeImportLayerInfos;
			for (int32 n = 0; n < InLandscapeInfo->Layers.Num(); ++n) 
			{
				TArray<uint8> CurrentLayerIntData;
				FLinearColor LayerUsageDebugColor;
				FString LayerName;
				if (!FUnrealLandscapeTranslator::GetLandscapeLayerData(InLandscapeProxy, InLandscapeInfo, n, CurrentLayerIntData, LayerUsageDebugColor, LayerName))
					continue;

				FLandscapeImportLayerInfo CurrentLayerInfo;
				CurrentLayerInfo.LayerName = FName(LayerName);
				CurrentLayerInfo.LayerInfo = InLandscapeInfo->Layers[n].LayerInfoObj;
				CurrentLayerInfo.LayerData = CurrentLayerIntData;

				CurrentLayerInfo.LayerInfo->LayerUsageDebugColor = LayerUsageDebugColor;

				InLandscapeImportLayerInfos.Add(CurrentLayerInfo);
			}

			// 1. Create package  //

			FString PackagePath = PackageParams.GetPackagePath();
			FString PackageName = PackageParams.GetPackageName();

			UPackage *CreatedPackage = nullptr;
			FString CreatedPackageName;

			CreatedPackage = PackageParams.CreatePackageForObject(CreatedPackageName);

			if (!CreatedPackage)
				return nullptr;

			OutBakeStats.NotifyPackageCreated(1);

			// 2. Create a new world asset with dialog //
			UWorldFactory* Factory = NewObject<UWorldFactory>();
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

			UObject* Asset = AssetToolsModule.Get().CreateAssetWithDialog(
				PackageName, PackagePath,
				UWorld::StaticClass(), Factory, FName("ContentBrowserNewAsset"));


			UWorld* NewWorld = Cast<UWorld>(Asset);
			if (!NewWorld)
				return nullptr;

			OutBakeStats.NotifyObjectsCreated(NewWorld->GetClass()->GetName(), 1);
			NewWorld->SetCurrentLevel(NewWorld->PersistentLevel);

			// 4. Spawn a landscape proxy actor in the created world
			ALandscapeStreamingProxy * BakedLandscapeProxy = NewWorld->SpawnActor<ALandscapeStreamingProxy>();
			if (!BakedLandscapeProxy)
				return nullptr;

			OutBakeStats.NotifyObjectsCreated(BakedLandscapeProxy->GetClass()->GetName(), 1);
			// Create a new GUID
			FGuid currentGUID = FGuid::NewGuid();
			BakedLandscapeProxy->SetLandscapeGuid(currentGUID);

			// Deactivate CastStaticShadow on the landscape to avoid "grid shadow" issue
			BakedLandscapeProxy->bCastStaticShadow = false;
			

			// 5. Import data to the created landscape proxy
			TMap<FGuid, TArray<uint16>> HeightmapDataPerLayers;
			TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayer;

			HeightmapDataPerLayers.Add(FGuid(), InLandscapeHeightData);
			MaterialLayerDataPerLayer.Add(FGuid(), InLandscapeImportLayerInfos);

			ELandscapeImportAlphamapType ImportLayerType = ELandscapeImportAlphamapType::Additive;

			BakedLandscapeProxy->Import(
				currentGUID,
				0, 0, XSize-1, YSize-1,
				InLandscapeInfo->ComponentNumSubsections, InLandscapeInfo->SubsectionSizeQuads,
				HeightmapDataPerLayers, NULL,
				MaterialLayerDataPerLayer, ImportLayerType);

			BakedLandscapeProxy->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((XSize * YSize) / (2048 * 2048) + 1), (uint32)2);

			TMap<UMaterialInterface *, UMaterialInterface *> AlreadyBakedMaterialsMap;

	
			if (BakedLandscapeProxy->LandscapeMaterial)
			{
				UMaterialInterface * DuplicatedMaterial = BakeSingleMaterialToPackage(
					BakedLandscapeProxy->LandscapeMaterial, PackageParams, PackagesToSave, AlreadyBakedMaterialsMap, OutBakeStats);
				BakedLandscapeProxy->LandscapeMaterial = DuplicatedMaterial;
			}

			if (BakedLandscapeProxy->LandscapeHoleMaterial)
			{
				UMaterialInterface * DuplicatedMaterial = BakeSingleMaterialToPackage(
					BakedLandscapeProxy->LandscapeHoleMaterial, PackageParams, PackagesToSave, AlreadyBakedMaterialsMap, OutBakeStats);
				BakedLandscapeProxy->LandscapeHoleMaterial = DuplicatedMaterial;
			}

			// 6. Register all the landscape components, and set landscape actor transform
			BakedLandscapeProxy->RegisterAllComponents();
			BakedLandscapeProxy->SetActorTransform(InLandscapeProxy->GetTransform());

			// 7. Save Package
			PackagesToSave.Add(CreatedPackage);
			FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

			// Sync the CB to the baked objects
			if(GEditor)
			{
				TArray<UObject*> Objects;
				Objects.Add(NewWorld);
				GEditor->SyncBrowserToObjects(Objects);
			}
		}
		break;
	}

	return InLandscapeProxy;
}

bool
FHoudiniEngineBakeUtils::BakeCurve(
	UHoudiniAssetComponent const* const InHoudiniAssetComponent,
	USplineComponent* InSplineComponent,
	ULevel* InLevel,
	const FHoudiniPackageParams &PackageParams,
	const FName& InActorName,
	AActor*& OutActor,
	USplineComponent*& OutSplineComponent,
	FHoudiniEngineOutputStats& OutBakeStats,
	FName InOverrideFolderPath,
	AActor* InActor,
	UActorFactory* InActorFactory)
{
	if (!IsValid(InActor))
	{
		TSubclassOf<AActor> BakeActorClass = nullptr;
		UActorFactory* Factory = nullptr;
		if (IsValid(InActorFactory))
		{
			Factory = InActorFactory;
		}
		else
		{
			Factory = GetActorFactory(NAME_None, BakeActorClass, UActorFactoryEmptyActor::StaticClass());
		}
		if (!Factory)
			return false;

		OutActor = SpawnBakeActor(Factory, nullptr, InLevel, InSplineComponent->GetComponentTransform(), InHoudiniAssetComponent, BakeActorClass);
		if (IsValid(OutActor))
			OutBakeStats.NotifyObjectsCreated(OutActor->GetClass()->GetName(), 1);
	}
	else
	{
		OutActor = InActor;
		if (IsValid(OutActor))
			OutBakeStats.NotifyObjectsUpdated(OutActor->GetClass()->GetName(), 1);
	}

	// Fallback to ObjectName from package params if InActorName is not set
	const FName ActorName = InActorName.IsNone() ? FName(PackageParams.ObjectName) : InActorName;
	const FString NewNameStr = MakeUniqueObjectNameIfNeeded(InLevel, OutActor->GetClass(), InActorName.ToString(), OutActor);
	RenameAndRelabelActor(OutActor, NewNameStr, false);
	OutActor->SetFolderPath(InOverrideFolderPath.IsNone() ? FName(PackageParams.HoudiniAssetActorName) : InOverrideFolderPath);

	USplineComponent* DuplicatedSplineComponent = DuplicateObject<USplineComponent>(
		InSplineComponent,
		OutActor,
		FName(MakeUniqueObjectNameIfNeeded(OutActor, InSplineComponent->GetClass(), PackageParams.ObjectName)));

	if (IsValid(DuplicatedSplineComponent))
		OutBakeStats.NotifyObjectsCreated(DuplicatedSplineComponent->GetClass()->GetName(), 1);
	
	OutActor->AddInstanceComponent(DuplicatedSplineComponent);
	const bool bCreateIfMissing = true;
	USceneComponent* RootComponent = GetActorRootComponent(OutActor, bCreateIfMissing);
	DuplicatedSplineComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	// We duplicated the InSplineComponent, so we don't have to copy all of its properties, but we must set the
	// world transform
	DuplicatedSplineComponent->SetWorldTransform(InSplineComponent->GetComponentTransform());
	
	FAssetRegistryModule::AssetCreated(DuplicatedSplineComponent);
	DuplicatedSplineComponent->RegisterComponent();

	OutSplineComponent = DuplicatedSplineComponent;
	return true;
}

bool 
FHoudiniEngineBakeUtils::BakeCurve(
	UHoudiniAssetComponent const* const InHoudiniAssetComponent,
	const FHoudiniOutputObject& InOutputObject,
	FHoudiniBakedOutputObject& InBakedOutputObject,
	// const TArray<FHoudiniBakedOutput>& InAllBakedOutputs,
	const FHoudiniPackageParams &PackageParams,
	FHoudiniAttributeResolver& InResolver,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	FHoudiniEngineBakedActor& OutBakedActorEntry,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	USplineComponent* SplineComponent = Cast<USplineComponent>(InOutputObject.OutputComponent);
	if (!IsValid(SplineComponent))
		return false;

	// By default spawn in the current level unless specified via the unreal_level_path attribute
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
	{
		UWorld* DesiredWorld = SplineComponent ? SplineComponent->GetWorld() : GWorld;

		// Get the package path from the unreal_level_apth attribute
		FString LevelPackagePath = InResolver.ResolveFullLevelPath();

		bool bCreatedPackage = false;
		if (!FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
			LevelPackagePath,
			DesiredLevel,
			DesiredWorld,
			bCreatedPackage))
		{
			// TODO: LOG ERROR IF NO LEVEL
			return false;
		}

		// If we have created a new level, add it to the packages to save
		// TODO: ? always add?
		if (bCreatedPackage && DesiredLevel)
		{
			OutBakeStats.NotifyPackageCreated(1);
			OutBakeStats.NotifyObjectsCreated(DesiredLevel->GetClass()->GetName(), 1);
			// We can now save the package again, and unload it.
			OutPackagesToSave.Add(DesiredLevel->GetOutermost());
		}
	}

	if(!DesiredLevel)
		return false;

	// Try to find the unreal_bake_actor, if specified, or fallback to the default named actor
	FName BakeActorName;
	AActor* FoundActor = nullptr;
	bool bHasBakeActorName = false;
	if (!FindUnrealBakeActor(InOutputObject, InBakedOutputObject, InBakedActors, DesiredLevel, *(PackageParams.ObjectName), bInReplaceActors, InFallbackActor, FoundActor, bHasBakeActorName, BakeActorName))
		return false;

	// If we are baking in replace mode, remove the previous bake component
	if (bInReplaceAssets && !InBakedOutputObject.BakedComponent.IsEmpty())
	{
		UActorComponent* PrevComponent = Cast<UActorComponent>(InBakedOutputObject.GetBakedComponentIfValid());
		if (PrevComponent && PrevComponent->GetOwner() == FoundActor)
		{
			RemovePreviouslyBakedComponent(PrevComponent);
		}
	}
	
	USplineComponent* NewSplineComponent = nullptr;
	const FName OutlinerFolderPath = GetOutlinerFolderPath(InOutputObject, *(PackageParams.HoudiniAssetActorName));
	if (!BakeCurve(InHoudiniAssetComponent, SplineComponent, DesiredLevel, PackageParams, BakeActorName, FoundActor, NewSplineComponent, OutBakeStats, OutlinerFolderPath, FoundActor))
		return false;

	InBakedOutputObject.Actor = FSoftObjectPath(FoundActor).ToString();
	InBakedOutputObject.BakedComponent = FSoftObjectPath(NewSplineComponent).ToString();

	// If we are baking in replace mode, remove previously baked components/instancers
	if (bInReplaceActors && bInReplaceAssets)
	{
		const bool bInDestroyBakedComponent = false;
		const bool bInDestroyBakedInstancedActors = true;
		const bool bInDestroyBakedInstancedComponents = true;
		DestroyPreviousBakeOutput(
			InBakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
	}

	FHoudiniEngineBakedActor Result(
		FoundActor,
		BakeActorName,
		OutlinerFolderPath.IsNone() ? FName(PackageParams.HoudiniAssetActorName) : OutlinerFolderPath,
		INDEX_NONE,  // Output index
		FHoudiniOutputObjectIdentifier(),
		nullptr,  // InBakedObject
		nullptr,  // InSourceObject
		NewSplineComponent,
		PackageParams.BakeFolder,
		PackageParams);

	OutBakedActorEntry = Result;

	return true;
}

AActor*
FHoudiniEngineBakeUtils::BakeInputHoudiniCurveToActor(
	UHoudiniAssetComponent const* const InHoudiniAssetComponent,
	UHoudiniSplineComponent * InHoudiniSplineComponent,
	const FHoudiniPackageParams & PackageParams,
	UWorld* WorldToSpawn,
	const FTransform & SpawnTransform) 
{
	if (!IsValid(InHoudiniSplineComponent))
		return nullptr;

	TArray<FVector> & DisplayPoints = InHoudiniSplineComponent->DisplayPoints;
	if (DisplayPoints.Num() < 2)
		return nullptr;

	ULevel* DesiredLevel = GWorld->GetCurrentLevel();

	TSubclassOf<AActor> BakeActorClass = nullptr;
	UActorFactory* const Factory = GetActorFactory(NAME_None, BakeActorClass, UActorFactoryEmptyActor::StaticClass());
	if (!Factory)
		return nullptr;

	// Remove the actor if it exists
	for (auto & Actor : DesiredLevel->Actors)
	{
		if (!Actor)
			continue;

		if (Actor->GetName() == PackageParams.ObjectName)
		{
			UWorld* World = Actor->GetWorld();
			if (!World)
				World = GWorld;

			Actor->RemoveFromRoot();
			Actor->ConditionalBeginDestroy();
			World->EditorDestroyActor(Actor, true);

			break;
		}
	}

	AActor* NewActor = SpawnBakeActor(Factory, nullptr, DesiredLevel, InHoudiniSplineComponent->GetComponentTransform(), InHoudiniAssetComponent, BakeActorClass);

	USplineComponent* BakedUnrealSplineComponent = NewObject<USplineComponent>(NewActor);
	if (!BakedUnrealSplineComponent)
		return nullptr;

	// add display points to created unreal spline component
	for (int32 n = 0; n < DisplayPoints.Num(); ++n) 
	{
		FVector & NextPoint = DisplayPoints[n];
		BakedUnrealSplineComponent->AddSplinePoint(NextPoint, ESplineCoordinateSpace::Local);
		// Set the curve point type to be linear, since we are using display points
		BakedUnrealSplineComponent->SetSplinePointType(n, ESplinePointType::Linear);
	}
	NewActor->AddInstanceComponent(BakedUnrealSplineComponent);

	BakedUnrealSplineComponent->AttachToComponent(NewActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

	FAssetRegistryModule::AssetCreated(NewActor);
	FAssetRegistryModule::AssetCreated(BakedUnrealSplineComponent);
	BakedUnrealSplineComponent->RegisterComponent();

	// The default name will be based on the static mesh package, we would prefer it to be based on the Houdini asset
	const FString NewNameStr = MakeUniqueObjectNameIfNeeded(DesiredLevel, Factory->NewActorClass, *(PackageParams.ObjectName), NewActor);
	RenameAndRelabelActor(NewActor, NewNameStr, false);
	NewActor->SetFolderPath(FName(PackageParams.HoudiniAssetName));

	return NewActor;
}

UBlueprint* 
FHoudiniEngineBakeUtils::BakeInputHoudiniCurveToBlueprint(
	UHoudiniAssetComponent const* const InHoudiniAssetComponent,
	UHoudiniSplineComponent * InHoudiniSplineComponent,
	const FHoudiniPackageParams & PackageParams,
	UWorld* WorldToSpawn,
	const FTransform & SpawnTransform) 
{
	if (!IsValid(InHoudiniSplineComponent))
		return nullptr;

	FGuid BakeGUID = FGuid::NewGuid();

	if (!BakeGUID.IsValid())
		BakeGUID = FGuid::NewGuid();

	// We only want half of generated guid string.
	FString BakeGUIDString = BakeGUID.ToString().Left(FHoudiniEngineUtils::PackageGUIDItemNameLength);

	// Generate Blueprint name.
	FString BlueprintName = PackageParams.ObjectName + "_BP";

	// Generate unique package name.
	FString PackageName = PackageParams.BakeFolder + "/" + BlueprintName;
	PackageName = UPackageTools::SanitizePackageName(PackageName);

	// See if package exists, if it does, we need to regenerate the name.
	UPackage * Package = FindPackage(nullptr, *PackageName);

	if (IsValid(Package))
	{
		// Package does exist, there's a collision, we need to generate a new name.
		BakeGUID.Invalidate();
	}
	else
	{
		// Create actual package.
		Package = CreatePackage(*PackageName);
	}

	AActor * CreatedHoudiniSplineActor = FHoudiniEngineBakeUtils::BakeInputHoudiniCurveToActor(
		InHoudiniAssetComponent, InHoudiniSplineComponent, PackageParams, WorldToSpawn, SpawnTransform);

	TArray<UPackage*> PackagesToSave;

	UBlueprint * Blueprint = nullptr;
	if (IsValid(CreatedHoudiniSplineActor))
	{

		UObject* Asset = nullptr;

		Asset = StaticFindObjectFast(UObject::StaticClass(), Package, FName(*BlueprintName));
		if (!Asset)
		{
			UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();

			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

			Asset = AssetToolsModule.Get().CreateAsset(
				BlueprintName, PackageParams.BakeFolder,
				UBlueprint::StaticClass(), Factory, FName("ContentBrowserNewAsset"));
		}

		TArray<UActorComponent*> Components;
		for (auto & Next : CreatedHoudiniSplineActor->GetComponents())
		{
			Components.Add(Next);
		}

		Blueprint = Cast<UBlueprint>(Asset);

		// Clear old Blueprint Node tree
		USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;

		int32 NodeSize = SCS->GetAllNodes().Num();
		for (int32 n = NodeSize - 1; n >= 0; --n)
			SCS->RemoveNode(SCS->GetAllNodes()[n]);

		FKismetEditorUtilities::AddComponentsToBlueprint(Blueprint, Components);

		CreatedHoudiniSplineActor->RemoveFromRoot();
		CreatedHoudiniSplineActor->ConditionalBeginDestroy();

		GWorld->EditorDestroyActor(CreatedHoudiniSplineActor, true);

		Package->MarkPackageDirty();
		PackagesToSave.Add(Package);
	}

	// Compile the new/updated blueprints
	if (!IsValid(Blueprint))
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	// Save the created BP package.
	FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

	return Blueprint;
}


void
FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
	UPackage * Package, UObject * Object, const TCHAR * Key,
	const TCHAR * Value)
{
	if (!IsValid(Package))
		return;

	UMetaData * MetaData = Package->GetMetaData();
	if (IsValid(MetaData))
		MetaData->SetValue(Object, Key, Value);
}


bool
FHoudiniEngineBakeUtils::
GetHoudiniGeneratedNameFromMetaInformation(
	UPackage * Package, UObject * Object, FString & HoudiniName)
{
	if (!IsValid(Package))
		return false;

	UMetaData * MetaData = Package->GetMetaData();
	if (!IsValid(MetaData))
		return false;

	if (MetaData->HasValue(Object, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT))
	{
		// Retrieve name used for package generation.
		const FString NameFull = MetaData->GetValue(Object, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME);

		//HoudiniName = NameFull.Left(FMath::Min(NameFull.Len(), FHoudiniEngineUtils::PackageGUIDItemNameLength));
		HoudiniName = NameFull;
		return true;
	}

	return false;
}

UMaterialInterface *
FHoudiniEngineBakeUtils::DuplicateMaterialAndCreatePackage(
	UMaterialInterface * Material, UMaterialInterface* PreviousBakeMaterial, const FString & MaterialName, const FHoudiniPackageParams& ObjectPackageParams,
	TArray<UPackage*> & OutGeneratedPackages,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	FHoudiniEngineOutputStats& OutBakeStats)
{
	if (InOutAlreadyBakedMaterialsMap.Contains(Material))
	{
		return InOutAlreadyBakedMaterialsMap[Material];
	}
	
	UMaterialInterface * DuplicatedMaterial = nullptr;

	FString CreatedMaterialName;
	// Create material package.  Use the same package params as static mesh, but with the material's name
	FHoudiniPackageParams MaterialPackageParams = ObjectPackageParams;
	MaterialPackageParams.ObjectName = MaterialName;

	// Check if there is a valid previous material. If so, get the bake counter for consistency in
	// replace or iterative package naming
	bool bIsPreviousBakeMaterialValid = IsValid(PreviousBakeMaterial);
	int32 BakeCounter = 0;
	TArray<UMaterialExpression*> PreviousBakeMaterialExpressions;

	
	if (bIsPreviousBakeMaterialValid && PreviousBakeMaterial->IsA(UMaterial::StaticClass()))
	{
		UMaterial * PreviousMaterialCast = Cast<UMaterial>(PreviousBakeMaterial);
		bIsPreviousBakeMaterialValid = MaterialPackageParams.MatchesPackagePathNameExcludingBakeCounter(PreviousBakeMaterial);

		if (bIsPreviousBakeMaterialValid && PreviousMaterialCast)
		{
			MaterialPackageParams.GetBakeCounterFromBakedAsset(PreviousBakeMaterial, BakeCounter);

			PreviousBakeMaterialExpressions = PreviousMaterialCast->Expressions;
		}
	}
	
	UPackage * MaterialPackage = MaterialPackageParams.CreatePackageForObject(CreatedMaterialName, BakeCounter);

	if (!IsValid(MaterialPackage))
		return nullptr;

	OutBakeStats.NotifyPackageCreated(1);
	
	// Clone material.
	DuplicatedMaterial = DuplicateObject< UMaterialInterface >(Material, MaterialPackage, *CreatedMaterialName);
	if (!IsValid(DuplicatedMaterial))
		return nullptr;

	OutBakeStats.NotifyObjectsCreated(DuplicatedMaterial->GetClass()->GetName(), 1);

	// Add meta information.
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MaterialPackage, DuplicatedMaterial,
		HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MaterialPackage, DuplicatedMaterial,
		HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *CreatedMaterialName);

	// Retrieve and check various sampling expressions. If they contain textures, duplicate (and bake) them.
	UMaterial * DuplicatedMaterialCast = Cast<UMaterial>(DuplicatedMaterial);
	if (DuplicatedMaterialCast)
	{
		const int32 NumExpressions = DuplicatedMaterialCast->Expressions.Num();
		for (int32 ExpressionIdx = 0; ExpressionIdx < NumExpressions; ++ExpressionIdx)
		{
			UMaterialExpression* Expression = DuplicatedMaterialCast->Expressions[ExpressionIdx];
			UMaterialExpression* PreviousBakeExpression = nullptr;
			if (bIsPreviousBakeMaterialValid && PreviousBakeMaterialExpressions.IsValidIndex(ExpressionIdx))
			{
				PreviousBakeExpression = PreviousBakeMaterialExpressions[ExpressionIdx];
			}
			FHoudiniEngineBakeUtils::ReplaceDuplicatedMaterialTextureSample(
				Expression, PreviousBakeExpression, MaterialPackageParams, OutGeneratedPackages, OutBakeStats);
		}
	}

	// Notify registry that we have created a new duplicate material.
	FAssetRegistryModule::AssetCreated(DuplicatedMaterial);

	// Dirty the material package.
	DuplicatedMaterial->MarkPackageDirty();

	// Recompile the baked material
	// DuplicatedMaterial->ForceRecompileForRendering();
	// Use UMaterialEditingLibrary::RecompileMaterial since it correctly updates texture references in the material
	// which ForceRecompileForRendering does not do
	if (DuplicatedMaterialCast)
	{
		UMaterialEditingLibrary::RecompileMaterial(DuplicatedMaterialCast);
	}

	OutGeneratedPackages.Add(MaterialPackage);

	InOutAlreadyBakedMaterialsMap.Add(Material, DuplicatedMaterial);

	return DuplicatedMaterial;
}

void
FHoudiniEngineBakeUtils::ReplaceDuplicatedMaterialTextureSample(
	UMaterialExpression * MaterialExpression, UMaterialExpression* PreviousBakeMaterialExpression,
	const FHoudiniPackageParams& PackageParams, TArray<UPackage*> & OutCreatedPackages, FHoudiniEngineOutputStats& OutBakeStats)
{
	UMaterialExpressionTextureSample * TextureSample = Cast< UMaterialExpressionTextureSample >(MaterialExpression);
	if (!IsValid(TextureSample))
		return;

	UTexture2D * Texture = Cast< UTexture2D >(TextureSample->Texture);
	if (!IsValid(Texture))
		return;

	UPackage * TexturePackage = Cast< UPackage >(Texture->GetOuter());
	if (!IsValid(TexturePackage))
		return;

	// Try to get the previous bake's texture
	UTexture2D* PreviousBakeTexture = nullptr;
	if (IsValid(PreviousBakeMaterialExpression))
	{
		UMaterialExpressionTextureSample* PreviousBakeTextureSample = Cast< UMaterialExpressionTextureSample >(PreviousBakeMaterialExpression);
		if (IsValid(PreviousBakeTextureSample))
			PreviousBakeTexture = Cast< UTexture2D >(PreviousBakeTextureSample->Texture);
	}

	FString GeneratedTextureName;
	if (FHoudiniEngineBakeUtils::GetHoudiniGeneratedNameFromMetaInformation(
		TexturePackage, Texture, GeneratedTextureName))
	{
		// Duplicate texture.
		UTexture2D * DuplicatedTexture = FHoudiniEngineBakeUtils::DuplicateTextureAndCreatePackage(
			Texture, PreviousBakeTexture, GeneratedTextureName, PackageParams, OutCreatedPackages, OutBakeStats);

		// Re-assign generated texture.
		TextureSample->Texture = DuplicatedTexture;
	}
}

UTexture2D *
FHoudiniEngineBakeUtils::DuplicateTextureAndCreatePackage(
	UTexture2D * Texture, UTexture2D* PreviousBakeTexture, const FString & SubTextureName, const FHoudiniPackageParams& PackageParams,
	TArray<UPackage*> & OutCreatedPackages, FHoudiniEngineOutputStats& OutBakeStats)
{
	UTexture2D* DuplicatedTexture = nullptr;
#if WITH_EDITOR
	// Retrieve original package of this texture.
	UPackage * TexturePackage = Cast< UPackage >(Texture->GetOuter());
	if (!IsValid(TexturePackage))
		return nullptr;

	FString GeneratedTextureName;
	if (FHoudiniEngineBakeUtils::GetHoudiniGeneratedNameFromMetaInformation(TexturePackage, Texture, GeneratedTextureName))
	{
		UMetaData * MetaData = TexturePackage->GetMetaData();
		if (!IsValid(MetaData))
			return nullptr;

		// Retrieve texture type.
		const FString & TextureType =
			MetaData->GetValue(Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE);

		FString CreatedTextureName;

		// Create texture package. Use the same package params as material's, but with object name appended by generated texture's name
		FHoudiniPackageParams TexturePackageParams = PackageParams;
		TexturePackageParams.ObjectName = TexturePackageParams.ObjectName + "_" + GeneratedTextureName;

		// Determine the bake counter of the previous bake's texture (if exists/valid) for naming consistency when
		// replacing/iterating
		bool bIsPreviousBakeTextureValid = IsValid(PreviousBakeTexture);
		int32 BakeCounter = 0;
		if (bIsPreviousBakeTextureValid)
		{
			bIsPreviousBakeTextureValid = TexturePackageParams.MatchesPackagePathNameExcludingBakeCounter(PreviousBakeTexture);
			if (bIsPreviousBakeTextureValid)
			{
				TexturePackageParams.GetBakeCounterFromBakedAsset(PreviousBakeTexture, BakeCounter);
			}
		}

		UPackage * NewTexturePackage = TexturePackageParams.CreatePackageForObject(CreatedTextureName, BakeCounter);

		if (!IsValid(NewTexturePackage))
			return nullptr;

		OutBakeStats.NotifyPackageCreated(1);
		
		// Clone texture.
		DuplicatedTexture = DuplicateObject< UTexture2D >(Texture, NewTexturePackage, *CreatedTextureName);
		if (!IsValid(DuplicatedTexture))
			return nullptr;

		OutBakeStats.NotifyObjectsCreated(DuplicatedTexture->GetClass()->GetName(), 1);

		// Add meta information.
		FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
			NewTexturePackage, DuplicatedTexture,
			HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
		FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
			NewTexturePackage, DuplicatedTexture,
			HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *CreatedTextureName);
		FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
			NewTexturePackage, DuplicatedTexture,
			HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE, *TextureType);

		// Notify registry that we have created a new duplicate texture.
		FAssetRegistryModule::AssetCreated(DuplicatedTexture);
		
		// Dirty the texture package.
		DuplicatedTexture->MarkPackageDirty();

		OutCreatedPackages.Add(NewTexturePackage);
	}
#endif
	return DuplicatedTexture;
}


bool 
FHoudiniEngineBakeUtils::DeleteBakedHoudiniAssetActor(UHoudiniAssetComponent* HoudiniAssetComponent) 
{
	if (!IsValid(HoudiniAssetComponent))
		return false;

	AActor * ActorOwner = HoudiniAssetComponent->GetOwner();

	if (!IsValid(ActorOwner))
		return false;

	UWorld* World = ActorOwner->GetWorld();
	if (!World)
		World = GWorld;

	World->EditorDestroyActor(ActorOwner, false);

	return true;
}


void 
FHoudiniEngineBakeUtils::SaveBakedPackages(TArray<UPackage*> & PackagesToSave, bool bSaveCurrentWorld) 
{
	UWorld * CurrentWorld = nullptr;
	if (bSaveCurrentWorld && GEditor)
		CurrentWorld = GEditor->GetEditorWorldContext().World();

	if (CurrentWorld)
	{
		// Save the current map
		FString CurrentWorldPath = FPaths::GetBaseFilename(CurrentWorld->GetPathName(), false);
		UPackage* CurrentWorldPackage = CreatePackage(*CurrentWorldPath);

		if (CurrentWorldPackage)
		{
			CurrentWorldPackage->MarkPackageDirty();
			PackagesToSave.Add(CurrentWorldPackage);
		}
	}

	FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, true, false);
}

bool
FHoudiniEngineBakeUtils::FindOutputObject(const UObject* InObjectToFind, EHoudiniOutputType InOutputType, const TArray<UHoudiniOutput*> InOutputs, int32& OutOutputIndex, FHoudiniOutputObjectIdentifier &OutIdentifier)
{
	if (!IsValid(InObjectToFind))
		return false;

	const int32 NumOutputs = InOutputs.Num();
	for (int32 OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
	{
		const UHoudiniOutput* CurOutput = InOutputs[OutputIdx];
		if (!IsValid(CurOutput))
			continue;

		if (CurOutput->GetType() != InOutputType)
			continue;
		
		for (auto& CurOutputObject : CurOutput->GetOutputObjects())
		{
			if (CurOutputObject.Value.OutputObject == InObjectToFind
				|| CurOutputObject.Value.OutputComponent == InObjectToFind
				|| CurOutputObject.Value.ProxyObject == InObjectToFind
				|| CurOutputObject.Value.ProxyComponent == InObjectToFind)
			{
				OutOutputIndex = OutputIdx;
				OutIdentifier = CurOutputObject.Key;
				return true;
			}
		}
	}

	return false;
}

bool
FHoudiniEngineBakeUtils::IsObjectTemporary(UObject* InObject, EHoudiniOutputType InOutputType, UHoudiniAssetComponent* InHAC)
{
	if (!IsValid(InObject))
		return false;

	FString TempPath = FString();

	// TODO: Get the HAC outputs in a better way?
	TArray<UHoudiniOutput*> Outputs;
	if (IsValid(InHAC))
	{
		const int32 NumOutputs = InHAC->GetNumOutputs();
		Outputs.Reserve(NumOutputs);
		for (int32 OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
		{
			Outputs.Add(InHAC->GetOutputAt(OutputIdx));
		}

		TempPath = InHAC->TemporaryCookFolder.Path;
	}

	return IsObjectTemporary(InObject, InOutputType, Outputs, TempPath);
}

bool FHoudiniEngineBakeUtils::IsObjectInTempFolder(UObject* const InObject, const FString& InTemporaryCookFolder)
{
	if (!IsValid(InObject))
		return false;

	// Check the package path for this object
	// If it is in the HAC temp directory, assume it is temporary, and will need to be duplicated
	UPackage* ObjectPackage = InObject->GetOutermost();
	if (IsValid(ObjectPackage))
	{
		const FString PathName = ObjectPackage->GetPathName();
		if (PathName.StartsWith(InTemporaryCookFolder))
			return true;

		// Also check the default temp folder
		const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
		if (PathName.StartsWith(HoudiniRuntimeSettings->DefaultTemporaryCookFolder))
			return true;
	}

	return false;
}

bool FHoudiniEngineBakeUtils::IsObjectTemporary(
	UObject* InObject, EHoudiniOutputType InOutputType, const TArray<UHoudiniOutput*>& InParentOutputs, const FString& InTemporaryCookFolder)
{
	if (!IsValid(InObject))
		return false;

	int32 ParentOutputIndex = -1;
	FHoudiniOutputObjectIdentifier Identifier;
	if (FindOutputObject(InObject, InOutputType, InParentOutputs, ParentOutputIndex, Identifier))
		return true;
	
	// Check the package path for this object
	// If it is in the HAC temp directory, assume it is temporary, and will need to be duplicated
	if (IsObjectInTempFolder(InObject, InTemporaryCookFolder))
		return true;

	/*
	UPackage* ObjectPackage = InObject->GetOutermost();
	if (IsValid(ObjectPackage))
	{
		// TODO: this just indicates that the object was generated by H
		// it could as well have been baked before... 
		// we should probably add a "temp" metadata
		// Look in the meta info as well??
		UMetaData * MetaData = ObjectPackage->GetMetaData();
		if (!IsValid(MetaData))
			return false;

		if (MetaData->HasValue(InObject, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT))
			return true;
	}
	*/

	return false;	
}

void 
FHoudiniEngineBakeUtils::CopyPropertyToNewActorAndComponent(
	AActor* NewActor,
	UStaticMeshComponent* NewSMC,
	UStaticMeshComponent* InSMC,
	bool bInCopyWorldTransform)
{
	if (!IsValid(NewSMC))
		return;

	if (!IsValid(InSMC))
		return;

	// Copy properties to new actor
	//UStaticMeshComponent* OtherSMC_NonConst = const_cast<UStaticMeshComponent*>(StaticMeshComponent);
	NewSMC->SetCollisionProfileName(InSMC->GetCollisionProfileName());
	NewSMC->SetCollisionEnabled(InSMC->GetCollisionEnabled());
	NewSMC->LightmassSettings = InSMC->LightmassSettings;
	NewSMC->CastShadow = InSMC->CastShadow;
	NewSMC->SetMobility(InSMC->Mobility);

	UBodySetup* InBodySetup = InSMC->GetBodySetup();
	UBodySetup* NewBodySetup = NewSMC->GetBodySetup();
	if (InBodySetup && NewBodySetup)
	{
		// Copy the BodySetup
		NewBodySetup->CopyBodyPropertiesFrom(InBodySetup);

		// We need to recreate the physics mesh for the new body setup
		NewBodySetup->InvalidatePhysicsData();
		NewBodySetup->CreatePhysicsMeshes();

		// Only copy the physical material if it's different from the default one,
		// As this was causing crashes on BakeToActors in some cases
		if (GEngine != NULL && NewBodySetup->GetPhysMaterial() != GEngine->DefaultPhysMaterial)
			NewSMC->SetPhysMaterialOverride(InBodySetup->GetPhysMaterial());
	}

	if (IsValid(NewActor))
		NewActor->SetActorHiddenInGame(InSMC->bHiddenInGame);

	NewSMC->SetVisibility(InSMC->IsVisible());

	// TODO:
	// // Reapply the uproperties modified by attributes on the new component
	// FHoudiniEngineUtils::UpdateAllPropertyAttributesOnObject(InSMC, InHGPO);

	// The below code is from EditorUtilities::CopyActorProperties and modified to only copy from one component to another
	UClass* ComponentClass = nullptr;
	if (InSMC->GetClass()->IsChildOf(NewSMC->GetClass()))
	{
		ComponentClass = NewSMC->GetClass();
	}
	else if (NewSMC->GetClass()->IsChildOf(InSMC->GetClass()))
	{
		ComponentClass = InSMC->GetClass();
	}
	else
	{
		HOUDINI_LOG_WARNING(
			TEXT("Incompatible component classes in CopyPropertyToNewActorAndComponent: %s vs %s"),
			*(InSMC->GetName()),
			*(NewSMC->GetClass()->GetName()));

		NewSMC->PostEditChange();
		return;
	}

	TSet<const FProperty*> SourceUCSModifiedProperties;
	InSMC->GetUCSModifiedProperties(SourceUCSModifiedProperties);

	AActor* SourceActor = InSMC->GetOwner();
	if (!IsValid(SourceActor))
	{
		NewSMC->PostEditChange();
		return;
	}

	TArray<UObject*> ModifiedObjects;
	const EditorUtilities::FCopyOptions Options(EditorUtilities::ECopyOptions::CallPostEditChangeProperty);
	// Copy component properties
	for( FProperty* Property = ComponentClass->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext )
	{
		const bool bIsTransient = !!( Property->PropertyFlags & CPF_Transient );
		const bool bIsIdentical = Property->Identical_InContainer( InSMC, NewSMC );
		const bool bIsComponent = !!( Property->PropertyFlags & ( CPF_InstancedReference | CPF_ContainsInstancedReference ) );
		const bool bIsTransform =
			Property->GetFName() == USceneComponent::GetRelativeScale3DPropertyName() ||
			Property->GetFName() == USceneComponent::GetRelativeLocationPropertyName() ||
			Property->GetFName() == USceneComponent::GetRelativeRotationPropertyName();

		// auto SourceComponentIsRoot = [&]()
		// {
		// 	USceneComponent* RootComponent = SourceActor->GetRootComponent();
		// 	if (InSMC == RootComponent)
		// 	{
		// 		return true;
		// 	}
		// 	return false;
		// };

		// if( !bIsTransient && !bIsIdentical && !bIsComponent && !SourceUCSModifiedProperties.Contains(Property)
		// 	&& ( !bIsTransform || !SourceComponentIsRoot() ) )
		if( !bIsTransient && !bIsIdentical && !bIsComponent && !SourceUCSModifiedProperties.Contains(Property)
			&& !bIsTransform )
		{
			// const bool bIsSafeToCopy = (!(Options.Flags & EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties) || (Property->HasAnyPropertyFlags(CPF_Edit | CPF_Interp)))
			// 							&& (!(Options.Flags & EditorUtilities::ECopyOptions::SkipInstanceOnlyProperties) || (!Property->HasAllPropertyFlags(CPF_DisableEditOnTemplate)));
			const bool bIsSafeToCopy = true;
			if( bIsSafeToCopy )
			{
				if (!Options.CanCopyProperty(*Property, *SourceActor))
				{
					continue;
				}
					
				if( !ModifiedObjects.Contains(NewSMC) )
				{
					NewSMC->SetFlags(RF_Transactional);
					NewSMC->Modify();
					ModifiedObjects.Add(NewSMC);
				}

				if (Options.Flags & EditorUtilities::ECopyOptions::CallPostEditChangeProperty)
				{
					// @todo simulate: Should we be calling this on the component instead?
					NewActor->PreEditChange( Property );
				}

				EditorUtilities::CopySingleProperty(InSMC, NewSMC, Property);

				if (Options.Flags & EditorUtilities::ECopyOptions::CallPostEditChangeProperty)
				{
					FPropertyChangedEvent PropertyChangedEvent( Property );
					NewActor->PostEditChangeProperty( PropertyChangedEvent );
				}
			}
		}
	}

	if (bInCopyWorldTransform)
	{
		NewSMC->SetWorldTransform(InSMC->GetComponentTransform());
	}

	NewSMC->PostEditChange();
}

void FHoudiniEngineBakeUtils::CopyPropertyToNewGeometryCollectionActorAndComponent(AGeometryCollectionActor* NewActor,
	UGeometryCollectionComponent* NewGCC, UGeometryCollectionComponent* InGCC, bool bInCopyWorldTransform)
{
	if (!IsValid(NewGCC))
		return;

	if (!IsValid(InGCC))
		return;

	// Copy properties to new actor
	//UStaticMeshComponent* OtherSMC_NonConst = const_cast<UStaticMeshComponent*>(StaticMeshComponent);
	NewGCC->ChaosSolverActor = InGCC->ChaosSolverActor;
	
	NewGCC->InitializationFields = InGCC->InitializationFields;
	//NewGCC->Simulating = InGCC->Simulating;
	NewGCC->InitializationState = InGCC->InitializationState;
	NewGCC->ObjectType= InGCC->ObjectType;
	NewGCC->EnableClustering = InGCC->EnableClustering;
	NewGCC->ClusterGroupIndex = InGCC->ClusterGroupIndex;
	NewGCC->MaxClusterLevel = InGCC->MaxClusterLevel;
	NewGCC->DamageThreshold = InGCC->DamageThreshold;
	// NewGCC->ClusterConnectionType = InGCC->ClusterConnectionType; // DEPRECATED.
	NewGCC->CollisionGroup = InGCC->CollisionGroup;
	NewGCC->CollisionSampleFraction = InGCC->CollisionSampleFraction;
	NewGCC->InitialVelocityType = InGCC->InitialVelocityType;
	NewGCC->InitialLinearVelocity = InGCC->InitialLinearVelocity;
	NewGCC->InitialAngularVelocity = InGCC->InitialAngularVelocity;
//	NewGCC->PhysicalMaterialOverride = InGCC->PhysicalMaterialOverride;
	
	if (IsValid(NewActor))
		NewActor->SetActorHiddenInGame(InGCC->bHiddenInGame);

	NewGCC->SetVisibility(InGCC->IsVisible());

	// The below code is from EditorUtilities::CopyActorProperties and modified to only copy from one component to another
	UClass* ComponentClass = nullptr;
	if (InGCC->GetClass()->IsChildOf(NewGCC->GetClass()))
	{
		ComponentClass = NewGCC->GetClass();
	}
	else if (NewGCC->GetClass()->IsChildOf(InGCC->GetClass()))
	{
		ComponentClass = InGCC->GetClass();
	}
	else
	{
		HOUDINI_LOG_WARNING(
			TEXT("Incompatible component classes in CopyPropertyToNewActorAndComponent: %s vs %s"),
			*(InGCC->GetName()),
			*(NewGCC->GetClass()->GetName()));

		NewGCC->PostEditChange();
		return;
	}

	TSet<const FProperty*> SourceUCSModifiedProperties;
	InGCC->GetUCSModifiedProperties(SourceUCSModifiedProperties);

	AActor* SourceActor = InGCC->GetOwner();
	if (!IsValid(SourceActor))
	{
		NewGCC->PostEditChange();
		return;
	}

	TArray<UObject*> ModifiedObjects;
	const EditorUtilities::FCopyOptions Options(EditorUtilities::ECopyOptions::CallPostEditChangeProperty);
	// Copy component properties
	for( FProperty* Property = ComponentClass->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext )
	{
		const bool bIsTransient = !!( Property->PropertyFlags & CPF_Transient );
		const bool bIsIdentical = Property->Identical_InContainer( InGCC, NewGCC );
		const bool bIsComponent = !!( Property->PropertyFlags & ( CPF_InstancedReference | CPF_ContainsInstancedReference ) );
		const bool bIsTransform =
			Property->GetFName() == USceneComponent::GetRelativeScale3DPropertyName() ||
			Property->GetFName() == USceneComponent::GetRelativeLocationPropertyName() ||
			Property->GetFName() == USceneComponent::GetRelativeRotationPropertyName();

		// auto SourceComponentIsRoot = [&]()
		// {
		// 	USceneComponent* RootComponent = SourceActor->GetRootComponent();
		// 	if (InSMC == RootComponent)
		// 	{
		// 		return true;
		// 	}
		// 	return false;
		// };

		// if( !bIsTransient && !bIsIdentical && !bIsComponent && !SourceUCSModifiedProperties.Contains(Property)
		// 	&& ( !bIsTransform || !SourceComponentIsRoot() ) )
		if( !bIsTransient && !bIsIdentical && !bIsComponent && !SourceUCSModifiedProperties.Contains(Property)
			&& !bIsTransform )
		{
			// const bool bIsSafeToCopy = (!(Options.Flags & EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties) || (Property->HasAnyPropertyFlags(CPF_Edit | CPF_Interp)))
			// 							&& (!(Options.Flags & EditorUtilities::ECopyOptions::SkipInstanceOnlyProperties) || (!Property->HasAllPropertyFlags(CPF_DisableEditOnTemplate)));
			const bool bIsSafeToCopy = true;
			if( bIsSafeToCopy )
			{
				if (!Options.CanCopyProperty(*Property, *SourceActor))
				{
					continue;
				}
					
				if( !ModifiedObjects.Contains(NewGCC) )
				{
					NewGCC->SetFlags(RF_Transactional);
					NewGCC->Modify();
					ModifiedObjects.Add(NewGCC);
				}

				if (Options.Flags & EditorUtilities::ECopyOptions::CallPostEditChangeProperty)
				{
					// @todo simulate: Should we be calling this on the component instead?
					NewActor->PreEditChange( Property );
				}

				EditorUtilities::CopySingleProperty(InGCC, NewGCC, Property);

				if (Options.Flags & EditorUtilities::ECopyOptions::CallPostEditChangeProperty)
				{
					FPropertyChangedEvent PropertyChangedEvent( Property );
					NewActor->PostEditChangeProperty( PropertyChangedEvent );
				}
			}
		}
	}

	if (bInCopyWorldTransform)
	{
		NewGCC->SetWorldTransform(InGCC->GetComponentTransform());
	}

	NewGCC->PostEditChange();
};

bool
FHoudiniEngineBakeUtils::RemovePreviouslyBakedActor(
	AActor* InNewBakedActor,
	ULevel* InLevel,
	const FHoudiniPackageParams& InPackageParams)
{
	// Remove a previous bake actor if it exists
	for (auto & Actor : InLevel->Actors)
	{
		if (!Actor)
			continue;

		if (Actor != InNewBakedActor && Actor->GetName() == InPackageParams.ObjectName)
		{
			UWorld* World = Actor->GetWorld();
			if (!World)
				World = GWorld;

			Actor->RemoveFromRoot();
			Actor->ConditionalBeginDestroy();
			World->EditorDestroyActor(Actor, true);

			return true;
		}
	}

	return false;
}

bool
FHoudiniEngineBakeUtils::RemovePreviouslyBakedComponent(UActorComponent* InComponent)
{
	if (!IsValid(InComponent))
		return false;

	// Remove from its actor first
	if (InComponent->GetOwner())
		InComponent->GetOwner()->RemoveOwnedComponent(InComponent);

	// Detach from its parent component if attached
	USceneComponent* SceneComponent = Cast<USceneComponent>(InComponent);
	if (IsValid(SceneComponent))
		SceneComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	InComponent->UnregisterComponent();
	InComponent->DestroyComponent();

	return true;
}

FName
FHoudiniEngineBakeUtils::GetOutputFolderPath(UObject* InOutputOwner)
{
	// Get an output folder path for PDG outputs generated by InOutputOwner.
	// The folder path is: <InOutputOwner's folder path (if it is an actor)>/<InOutputOwner's name>
	FString FolderName;
	FName FolderDirName;
	AActor* OuterActor = Cast<AActor>(InOutputOwner);
	if (OuterActor)
	{
		FolderName = OuterActor->GetActorLabel();
		FolderDirName = OuterActor->GetFolderPath();	
	}
	else
	{
		FolderName = InOutputOwner->GetName();
	}
	if (!FolderDirName.IsNone())		
		return FName(FString::Printf(TEXT("%s/%s"), *FolderDirName.ToString(), *FolderName));
	else
		return FName(FolderName);
}

void
FHoudiniEngineBakeUtils::RenameAsset(UObject* InAsset, const FString& InNewName, bool bMakeUniqueIfNotUnique)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));

	const FSoftObjectPath OldPath = FSoftObjectPath(InAsset);

	FString NewName;
	if (bMakeUniqueIfNotUnique)
		NewName = MakeUniqueObjectNameIfNeeded(InAsset->GetPackage(), InAsset->GetClass(), InNewName, InAsset);
	else
		NewName = InNewName;

	FHoudiniEngineUtils::RenameObject(InAsset, *NewName);

	const FSoftObjectPath NewPath = FSoftObjectPath(InAsset);
	if (OldPath != NewPath)
	{
		TArray<FAssetRenameData> RenameData;
		RenameData.Add(FAssetRenameData(OldPath, NewPath, true));
		AssetToolsModule.Get().RenameAssets(RenameData);
	}
}

void
FHoudiniEngineBakeUtils::RenameAndRelabelActor(AActor* InActor, const FString& InNewName, bool bMakeUniqueIfNotUnique)
{
	if (!IsValid(InActor))
		return;
	
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));

	const FSoftObjectPath OldPath = FSoftObjectPath(InActor);

	FString NewName;
	if (bMakeUniqueIfNotUnique)
		NewName = MakeUniqueObjectNameIfNeeded(InActor->GetOuter(), InActor->GetClass(), InNewName, InActor);
	else
		NewName = InNewName;

	FHoudiniEngineUtils::RenameObject(InActor, *NewName);
	FHoudiniEngineRuntimeUtils::SetActorLabel(InActor, NewName);

	const FSoftObjectPath NewPath = FSoftObjectPath(InActor);
	if (OldPath != NewPath)
	{
		TArray<FAssetRenameData> RenameData;
		RenameData.Add(FAssetRenameData(OldPath, NewPath, true));
		AssetToolsModule.Get().RenameAssets(RenameData);
	}
}

bool
FHoudiniEngineBakeUtils::DetachAndRenameBakedPDGOutputActor(
	AActor* InActor,
	const FString& InNewName,
	const FName& InFolderPath)
{
	if (!IsValid(InActor))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineUtils::DetachAndRenameBakedPDGOutputActor]: InActor is null."));
		return false;
	}

	if (InNewName.TrimStartAndEnd().IsEmpty())
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineUtils::DetachAndRenameBakedPDGOutputActor]: A valid actor name must be specified."));
		return false;
	}

	// Detach from parent
	InActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	// Rename
	const bool bMakeUniqueIfNotUnique = true;
	RenameAndRelabelActor(InActor, InNewName, bMakeUniqueIfNotUnique);

	InActor->SetFolderPath(InFolderPath);

	return true;
}

bool
FHoudiniEngineBakeUtils::BakePDGWorkResultObject(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNode* InNode,
	int32 InWorkResultArrayIndex,
	int32 InWorkResultObjectArrayIndex,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	bool bInBakeToWorkResultActor,
	bool bInIsAutoBake,
	const TArray<FHoudiniEngineBakedActor>& InBakedActors,
	TArray<FHoudiniEngineBakedActor>& OutBakedActors,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats,
	TArray<EHoudiniOutputType> const* InOutputTypesToBake,
	TArray<EHoudiniInstancerComponentType> const* InInstancerComponentTypesToBake,
	const FString& InFallbackWorldOutlinerFolder)
{
	if (!IsValid(InPDGAssetLink))
		return false;

	if (!IsValid(InNode))
		return false;

	if (!InNode->WorkResult.IsValidIndex(InWorkResultArrayIndex))
		return false;

	FTOPWorkResult& WorkResult = InNode->WorkResult[InWorkResultArrayIndex];
	if (!WorkResult.ResultObjects.IsValidIndex(InWorkResultObjectArrayIndex))
		return false;
	
	FTOPWorkResultObject& WorkResultObject = WorkResult.ResultObjects[InWorkResultObjectArrayIndex];
	TArray<UHoudiniOutput*>& Outputs = WorkResultObject.GetResultOutputs();
	if (Outputs.Num() == 0)
		return true;

	if (WorkResultObject.State != EPDGWorkResultState::Loaded)
	{
		if (bInIsAutoBake && WorkResultObject.AutoBakedSinceLastLoad())
		{
			HOUDINI_LOG_MESSAGE(TEXT("[FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors]: WorkResultObject (%s) is not loaded but was auto-baked since its last load."), *WorkResultObject.Name);
			return true;
		}

		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors]: WorkResultObject (%s) is not loaded, cannot bake it."), *WorkResultObject.Name);
		return false;
	}

	AActor* WorkResultObjectActor = WorkResultObject.GetOutputActorOwner().GetOutputActor();
	if (!IsValid(WorkResultObjectActor))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors]: WorkResultObjectActor (%s) is null (unexpected since # Outputs > 0)"), *WorkResultObject.Name);
		return false;
	}

	// Find the previous bake output for this work result object
	FString Key;
	InNode->GetBakedWorkResultObjectOutputsKey(InWorkResultArrayIndex, InWorkResultObjectArrayIndex, Key);
	FHoudiniPDGWorkResultObjectBakedOutput& BakedOutputContainer = InNode->GetBakedWorkResultObjectsOutputs().FindOrAdd(Key);

	const UHoudiniAssetComponent* HoudiniAssetComponent = FHoudiniEngineUtils::GetOuterHoudiniAssetComponent(InPDGAssetLink);
	check(IsValid(HoudiniAssetComponent));

	TArray<FHoudiniEngineBakedActor> WROBakedActors;
	BakeHoudiniOutputsToActors(
		HoudiniAssetComponent,
		Outputs,
		BakedOutputContainer.BakedOutputs,
		WorkResultObjectActor->GetActorTransform(),
		InPDGAssetLink->BakeFolder,
		InPDGAssetLink->GetTemporaryCookFolder(),
		bInReplaceActors,
		bInReplaceAssets,
		InBakedActors,
		WROBakedActors, 
		OutPackagesToSave,
		OutBakeStats,
		InOutputTypesToBake,
		InInstancerComponentTypesToBake,
		bInBakeToWorkResultActor ? WorkResultObjectActor : nullptr,
		InFallbackWorldOutlinerFolder);

	// Set the PDG indices on the output baked actor entries
	FOutputActorOwner& OutputActorOwner = WorkResultObject.GetOutputActorOwner();
	AActor* const WROActor = OutputActorOwner.GetOutputActor();
	FHoudiniEngineBakedActor const * BakedWROActorEntry = nullptr;
	if (WROBakedActors.Num() > 0)
	{
		for (FHoudiniEngineBakedActor& BakedActorEntry : WROBakedActors)
		{
			BakedActorEntry.PDGWorkResultArrayIndex = InWorkResultArrayIndex;
			BakedActorEntry.PDGWorkItemIndex = WorkResult.WorkItemIndex;
			BakedActorEntry.PDGWorkResultObjectArrayIndex = InWorkResultObjectArrayIndex;

			if (WROActor && BakedActorEntry.Actor == WROActor)
			{
				BakedWROActorEntry = &BakedActorEntry;
			}
		}
	}

	// If anything was baked to WorkResultObjectActor, detach it from its parent
	if (bInBakeToWorkResultActor)
	{
		// if we re-used the temp actor as a bake actor, then remove its temp outputs
		constexpr bool bDeleteOutputActor = false;
		InNode->DeleteWorkResultObjectOutputs(InWorkResultArrayIndex, InWorkResultObjectArrayIndex, bDeleteOutputActor);
		if (WROActor)
		{
			if (BakedWROActorEntry)
			{
				OutputActorOwner.SetOutputActor(nullptr);
				const FString OldActorPath = FSoftObjectPath(WROActor).ToString();
				DetachAndRenameBakedPDGOutputActor(
					WROActor, BakedWROActorEntry->ActorBakeName.ToString(), BakedWROActorEntry->WorldOutlinerFolder);
				const FString NewActorPath = FSoftObjectPath(WROActor).ToString();
				if (OldActorPath != NewActorPath)
				{
					// Fix cached string reference in baked outputs to WROActor
					for (FHoudiniBakedOutput& BakedOutput : BakedOutputContainer.BakedOutputs)
					{
						for (auto& Entry : BakedOutput.BakedOutputObjects)
						{
							if (Entry.Value.Actor == OldActorPath)
								Entry.Value.Actor = NewActorPath;
						}
					}
				}
			}
			else
			{
				OutputActorOwner.DestroyOutputActor();
			}
		}
	}

	if (bInIsAutoBake)
		WorkResultObject.SetAutoBakedSinceLastLoad(true);
	
	OutBakedActors = MoveTemp(WROBakedActors);
	
	return true;
}

void
FHoudiniEngineBakeUtils::CheckPDGAutoBakeAfterResultObjectLoaded(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNode* InNode,
	int32 InWorkItemHAPIIndex,
	int32 InWorkItemResultInfoIndex)
{
	TArray<FHoudiniEngineBakedActor> BakedActors;
	PDGAutoBakeAfterResultObjectLoaded(InPDGAssetLink, InNode, InWorkItemHAPIIndex, InWorkItemResultInfoIndex, BakedActors);
}

void
FHoudiniEngineBakeUtils::PDGAutoBakeAfterResultObjectLoaded(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNode* InNode,
	int32 InWorkItemHAPIIndex,
	int32 InWorkItemResultInfoIndex,
	TArray<FHoudiniEngineBakedActor>& OutBakedActors)
{
	if (!IsValid(InPDGAssetLink))
		return;

	if (!InPDGAssetLink->bBakeAfterAllWorkResultObjectsLoaded)
		return;

	if (!IsValid(InNode))
		return;

	// Check if the node is ready for baking: all work items must be complete
	bool bDoNotBake = false;
	bool bPendingBakeItems = false;
	if (!InNode->AreAllWorkItemsComplete() || InNode->AnyWorkItemsFailed())
		bDoNotBake = true;

	// Check if the node is ready for baking: all work items must be loaded
	if (!bDoNotBake)
	{
		for (const FTOPWorkResult& WorkResult : InNode->WorkResult)
		{
			for (const FTOPWorkResultObject& WRO : WorkResult.ResultObjects)
			{
				if (WRO.State != EPDGWorkResultState::Loaded && !WRO.AutoBakedSinceLastLoad())
				{
					bDoNotBake = true;
					break;
				}
			}
			if (bDoNotBake)
				break;
		}
	}

	if (!bDoNotBake)
	{
		// Check which outputs are selected for baking: selected node, selected network or all
		// And only bake if the node falls within the criteria
		UTOPNetwork const * const SelectedTOPNetwork = InPDGAssetLink->GetSelectedTOPNetwork();
		UTOPNode const * const SelectedTOPNode = InPDGAssetLink->GetSelectedTOPNode();
		switch (InPDGAssetLink->PDGBakeSelectionOption)
		{
			case EPDGBakeSelectionOption::SelectedNetwork:
				if (!IsValid(SelectedTOPNetwork) || !InNode->IsParentTOPNetwork(SelectedTOPNetwork))
				{
					HOUDINI_LOG_WARNING(
						TEXT("Not baking Node %s (Net %s): not in selected network"),
						InNode ? *InNode->GetName() : TEXT(""),
						SelectedTOPNetwork ? *SelectedTOPNetwork->GetName() : TEXT(""));
					bDoNotBake = true;
				}
				break;
			case EPDGBakeSelectionOption::SelectedNode:
				if (InNode != SelectedTOPNode)
				{
					HOUDINI_LOG_WARNING(
						TEXT("Not baking Node %s (Net %s): not the selected node"),
						InNode ? *InNode->GetName() : TEXT(""),
						SelectedTOPNetwork ? *SelectedTOPNetwork->GetName() : TEXT(""));
					bDoNotBake = true;
				}
				break;
			case EPDGBakeSelectionOption::All:
			default:
				break;
		}
	}

	// If there are no nodes left to auto-bake, broadcast the onpostbake delegate
	if (bDoNotBake && !InPDGAssetLink->AnyRemainingAutoBakeNodes())
		InPDGAssetLink->HandleOnPostBake(true);

	if (bDoNotBake)
		return;

	TArray<FHoudiniEngineBakedActor> BakedActors;
	bool bSuccess = false;
	const bool bIsAutoBake = true;
	switch (InPDGAssetLink->HoudiniEngineBakeOption)
	{
		case EHoudiniEngineBakeOption::ToActor:
			bSuccess = FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors(InPDGAssetLink, InNode, bIsAutoBake, InPDGAssetLink->PDGBakePackageReplaceMode, InPDGAssetLink->bRecenterBakedActors, BakedActors);
			break;

		case EHoudiniEngineBakeOption::ToBlueprint:
			bSuccess = FHoudiniEngineBakeUtils::BakePDGTOPNodeBlueprints(InPDGAssetLink, InNode, bIsAutoBake, InPDGAssetLink->PDGBakePackageReplaceMode, InPDGAssetLink->bRecenterBakedActors);
			break;

		default:
			HOUDINI_LOG_WARNING(TEXT("Unsupported HoudiniEngineBakeOption %i"), InPDGAssetLink->HoudiniEngineBakeOption);
	}

	if (bSuccess)
	{
		OutBakedActors = MoveTemp(BakedActors);
	}

	if (!InPDGAssetLink->AnyRemainingAutoBakeNodes())
		InPDGAssetLink->HandleOnPostBake(bSuccess);
}

bool
FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNode* InNode,
	bool bInBakeForBlueprint,
	bool bInIsAutoBake,
	const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode,
	TArray<FHoudiniEngineBakedActor>& OutBakedActors,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats) 
{
	if (!IsValid(InPDGAssetLink))
		return false;

	if (!IsValid(InNode))
		return false;

	// Determine the output world outliner folder path via the PDG asset link's
	// owner's folder path and name
	UObject* PDGOwner = InPDGAssetLink->GetOwnerActor();
	if (!PDGOwner)
		PDGOwner = InPDGAssetLink->GetOuter();
	const FName& FallbackWorldOutlinerFolderPath = GetOutputFolderPath(PDGOwner);

	// Determine the actor/package replacement settings
	const bool bReplaceActors = !bInBakeForBlueprint && InPDGBakePackageReplaceMode == EPDGBakePackageReplaceModeOption::ReplaceExistingAssets;
	const bool bReplaceAssets = InPDGBakePackageReplaceMode == EPDGBakePackageReplaceModeOption::ReplaceExistingAssets;

	// Determine the output types to bake: don't bake landscapes in blueprint baking mode
	TArray<EHoudiniOutputType> OutputTypesToBake;
	TArray<EHoudiniInstancerComponentType> InstancerComponentTypesToBake;
	if (bInBakeForBlueprint)
	{
		OutputTypesToBake.Add(EHoudiniOutputType::Mesh);
		OutputTypesToBake.Add(EHoudiniOutputType::Instancer);
		OutputTypesToBake.Add(EHoudiniOutputType::Curve);

		InstancerComponentTypesToBake.Add(EHoudiniInstancerComponentType::StaticMeshComponent);
		InstancerComponentTypesToBake.Add(EHoudiniInstancerComponentType::InstancedStaticMeshComponent);
		InstancerComponentTypesToBake.Add(EHoudiniInstancerComponentType::MeshSplitInstancerComponent);
		InstancerComponentTypesToBake.Add(EHoudiniInstancerComponentType::FoliageAsHierarchicalInstancedStaticMeshComponent);
		InstancerComponentTypesToBake.Add(EHoudiniInstancerComponentType::GeoemtryCollectionComponent);
	}
	
	const int32 NumWorkResults = InNode->WorkResult.Num();
	FScopedSlowTask Progress(NumWorkResults, FText::FromString(FString::Printf(TEXT("Baking PDG Node Output %s ..."), *InNode->GetName())));
	Progress.MakeDialog();

	TArray<FHoudiniEngineBakedActor> OurBakedActors;
	TArray<FHoudiniEngineBakedActor> WorkResultObjectBakedActors;
	for (int32 WorkResultArrayIdx = 0; WorkResultArrayIdx < NumWorkResults; ++WorkResultArrayIdx)
	{
		FTOPWorkResult& WorkResult = InNode->WorkResult[WorkResultArrayIdx];
		const int32 NumWorkResultObjects = WorkResult.ResultObjects.Num();
		for (int32 WorkResultObjectArrayIdx = 0; WorkResultObjectArrayIdx < NumWorkResultObjects; ++WorkResultObjectArrayIdx)
		{
			WorkResultObjectBakedActors.Reset();
			Progress.EnterProgressFrame(1.0f);

			BakePDGWorkResultObject(
				InPDGAssetLink,
				InNode,
				WorkResultArrayIdx,
				WorkResultObjectArrayIdx,
				bReplaceActors,
				bReplaceAssets,
				!bInBakeForBlueprint,
				bInIsAutoBake,
				OurBakedActors,
				WorkResultObjectBakedActors,
				OutPackagesToSave,
				OutBakeStats,
				OutputTypesToBake.Num() > 0 ? &OutputTypesToBake : nullptr,
				InstancerComponentTypesToBake.Num() > 0 ? &InstancerComponentTypesToBake : nullptr,
				FallbackWorldOutlinerFolderPath.ToString()
			);

			OurBakedActors.Append(WorkResultObjectBakedActors);
		}
	}

	OutBakedActors = MoveTemp(OurBakedActors);

	return true;
}

bool
FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors(
	UHoudiniPDGAssetLink* InPDGAssetLink, 
	UTOPNode* InTOPNode, 
	bool bInIsAutoBake, 
	const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode, 
	bool bInRecenterBakedActors,
	TArray<FHoudiniEngineBakedActor>& OutBakedActors)
{
	TArray<UPackage*> PackagesToSave;
	FHoudiniEngineOutputStats BakeStats;

	const bool bBakeBlueprints = false;

	bool bSuccess = BakePDGTOPNodeOutputsKeepActors(
		InPDGAssetLink, InTOPNode, bBakeBlueprints, bInIsAutoBake, InPDGBakePackageReplaceMode, OutBakedActors, PackagesToSave, BakeStats);

	SaveBakedPackages(PackagesToSave);

	// Recenter and select the baked actors
	if (GEditor && OutBakedActors.Num() > 0)
		GEditor->SelectNone(false, true);
	
	for (const FHoudiniEngineBakedActor& Entry : OutBakedActors)
	{
		if (!IsValid(Entry.Actor))
			continue;
		
		if (bInRecenterBakedActors)
			CenterActorToBoundingBoxCenter(Entry.Actor);

		if (GEditor)
			GEditor->SelectActor(Entry.Actor, true, false);
	}
	
	if (GEditor && OutBakedActors.Num() > 0)
		GEditor->NoteSelectionChange();

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakeStats.NumPackagesCreated, BakeStats.NumPackagesUpdated } );
		FHoudiniEngine::Get().FinishTaskSlateNotification( FText::FromString(Msg) );
	}

	return bSuccess;
}

bool
FHoudiniEngineBakeUtils::BakePDGTOPNetworkOutputsKeepActors(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNetwork* InNetwork,
	bool bInBakeForBlueprint,
	bool bInIsAutoBake,
	const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode,
	TArray<FHoudiniEngineBakedActor>& BakedActors,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats)
{
	if (!IsValid(InPDGAssetLink))
		return false;

	if (!IsValid(InNetwork))
		return false;

	bool bSuccess = true;
	for (UTOPNode* Node : InNetwork->AllTOPNodes)
	{
		if (!IsValid(Node))
			continue;

		bSuccess &= BakePDGTOPNodeOutputsKeepActors(InPDGAssetLink, Node, bInBakeForBlueprint, bInIsAutoBake, InPDGBakePackageReplaceMode, BakedActors, OutPackagesToSave, OutBakeStats);
	}

	return bSuccess;
}

bool
FHoudiniEngineBakeUtils::BakePDGAssetLinkOutputsKeepActors(UHoudiniPDGAssetLink* InPDGAssetLink, const EPDGBakeSelectionOption InBakeSelectionOption, const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode, bool bInRecenterBakedActors)
{
	if (!IsValid(InPDGAssetLink))
		return false;

	TArray<UPackage*> PackagesToSave;
	FHoudiniEngineOutputStats BakeStats;
	TArray<FHoudiniEngineBakedActor> BakedActors;
	const bool bBakeBlueprints = false;
	const bool bIsAutoBake = false;

	bool bSuccess = true;
	switch(InBakeSelectionOption)
	{
		case EPDGBakeSelectionOption::All:
			for (UTOPNetwork* Network : InPDGAssetLink->AllTOPNetworks)
			{
				if (!IsValid(Network))
					continue;
				
				for (UTOPNode* Node : Network->AllTOPNodes)
				{
					if (!IsValid(Node))
						continue;
					
					bSuccess &= BakePDGTOPNodeOutputsKeepActors(InPDGAssetLink, Node, bBakeBlueprints, bIsAutoBake, InPDGBakePackageReplaceMode, BakedActors, PackagesToSave, BakeStats);
				}
			}
			break;
		case EPDGBakeSelectionOption::SelectedNetwork:
			bSuccess = BakePDGTOPNetworkOutputsKeepActors(InPDGAssetLink, InPDGAssetLink->GetSelectedTOPNetwork(), bBakeBlueprints, bIsAutoBake, InPDGBakePackageReplaceMode, BakedActors, PackagesToSave, BakeStats);
		case EPDGBakeSelectionOption::SelectedNode:
			bSuccess = BakePDGTOPNodeOutputsKeepActors(InPDGAssetLink, InPDGAssetLink->GetSelectedTOPNode(), bBakeBlueprints, bIsAutoBake, InPDGBakePackageReplaceMode, BakedActors, PackagesToSave, BakeStats);
	}

	SaveBakedPackages(PackagesToSave);

	// Recenter and select the baked actors
	if (GEditor && BakedActors.Num() > 0)
		GEditor->SelectNone(false, true);
	
	for (const FHoudiniEngineBakedActor& Entry : BakedActors)
	{
		if (!IsValid(Entry.Actor))
			continue;
		
		if (bInRecenterBakedActors)
			CenterActorToBoundingBoxCenter(Entry.Actor);

		if (GEditor)
			GEditor->SelectActor(Entry.Actor, true, false);
	}
	
	if (GEditor && BakedActors.Num() > 0)
		GEditor->NoteSelectionChange();

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakeStats.NumPackagesCreated, BakeStats.NumPackagesUpdated } );
		FHoudiniEngine::Get().FinishTaskSlateNotification( FText::FromString(Msg) );
	}

	// Broadcast that the bake is complete
	InPDGAssetLink->HandleOnPostBake(bSuccess);

	return bSuccess;
}

bool
FHoudiniEngineBakeUtils::BakeBlueprintsFromBakedActors(
	const TArray<FHoudiniEngineBakedActor>& InBakedActors, 
	bool bInRecenterBakedActors,
	bool bInReplaceAssets,
	const FString& InHoudiniAssetName,
	const FString& InHoudiniAssetActorName,
	const FDirectoryPath& InBakeFolder,
	TArray<FHoudiniBakedOutput>* const InNonPDGBakedOutputs,
	TMap<FString, FHoudiniPDGWorkResultObjectBakedOutput>* const InPDGBakedOutputs,
	TArray<UBlueprint*>& OutBlueprints,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats)
{
	// // Clear selection
	// if (GEditor)
	// {
	// 	GEditor->SelectNone(false, true);
	// 	GEditor->NoteSelectionChange();
	// }

	// Iterate over the baked actors. An actor might appear multiple times if multiple OutputComponents were
	// baked to the same actor, so keep track of actors we have already processed in BakedActorSet
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	const bool bIsAssetEditorSubsystemValid = IsValid(AssetEditorSubsystem);
	TArray<UObject*> AssetsToReOpenEditors;
	TMap<AActor*, UBlueprint*> BakedActorMap;

	for (const FHoudiniEngineBakedActor& Entry : InBakedActors)
	{
		AActor *Actor = Entry.Actor;
		
		if (!IsValid(Actor))
			continue;

		// If we have a previously baked a blueprint, get the bake counter from it so that both replace and increment
		// is consistent with the bake counter
		int32 BakeCounter = 0;
		FHoudiniBakedOutputObject* BakedOutputObject = nullptr;
		// Get the baked output object
		if (Entry.PDGWorkResultArrayIndex >= 0 && Entry.PDGWorkItemIndex >= 0 && Entry.PDGWorkResultObjectArrayIndex >= 0 && InPDGBakedOutputs)
		{
			const FString Key = UTOPNode::GetBakedWorkResultObjectOutputsKey(Entry.PDGWorkResultArrayIndex, Entry.PDGWorkResultObjectArrayIndex);
			FHoudiniPDGWorkResultObjectBakedOutput* WorkResultObjectBakedOutput = InPDGBakedOutputs->Find(Key);
			if (WorkResultObjectBakedOutput)
			{
				if (Entry.OutputIndex >= 0 && WorkResultObjectBakedOutput->BakedOutputs.IsValidIndex(Entry.OutputIndex))
				{
					BakedOutputObject = WorkResultObjectBakedOutput->BakedOutputs[Entry.OutputIndex].BakedOutputObjects.Find(Entry.OutputObjectIdentifier);
				}
			}
		}
		else if (Entry.OutputIndex >= 0 && InNonPDGBakedOutputs)
		{
			if (Entry.OutputIndex >= 0 && InNonPDGBakedOutputs->IsValidIndex(Entry.OutputIndex))
			{
				BakedOutputObject = (*InNonPDGBakedOutputs)[Entry.OutputIndex].BakedOutputObjects.Find(Entry.OutputObjectIdentifier);
			}
		}

		if (BakedActorMap.Contains(Actor))
		{
			// Record the blueprint as the previous bake blueprint and clear the info of the temp bake actor/component
			if (BakedOutputObject)
			{
				UBlueprint* const BakedBlueprint = BakedActorMap[Actor];
				if (BakedBlueprint)
					BakedOutputObject->Blueprint = FSoftObjectPath(BakedBlueprint).ToString();
				else
					BakedOutputObject->Blueprint.Empty();
				BakedOutputObject->Actor.Empty();
				// TODO: Set the baked component to the corresponding component in the blueprint?
				BakedOutputObject->BakedComponent.Empty();
			}
			continue;
		}

		// Add a placeholder entry since we've started processing the actor, we'll replace the null with the blueprint
		// if successful and leave it as null if the bake fails (the actor will then be skipped if it appears in the
		// array again).
		BakedActorMap.Add(Actor, nullptr);

		UObject* Asset = nullptr;

		// Recenter the actor to its bounding box center
		if (bInRecenterBakedActors)
			CenterActorToBoundingBoxCenter(Actor);

		// Create package for out Blueprint
		FString BlueprintName;

		// For instancers we determine the bake folder from the instancer,
		// for everything else we use the baked object's bake folder
		// If all of that is blank, we fall back to InBakeFolder.
		FString BakeFolderPath;
		if (Entry.bInstancerOutput)
			BakeFolderPath = Entry.InstancerPackageParams.BakeFolder;
		else
			BakeFolderPath = Entry.BakeFolderPath;
		if (BakeFolderPath.IsEmpty())
			BakeFolderPath = InBakeFolder.Path;
		
		FHoudiniPackageParams PackageParams;
		// Set the replace mode based on if we are doing a replacement or incremental asset bake
		const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
            EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
		FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
            PackageParams,
            FHoudiniOutputObjectIdentifier(),
            BakeFolderPath,
            Entry.ActorBakeName.ToString() + "_BP",
            InHoudiniAssetName,
            InHoudiniAssetActorName,
            AssetPackageReplaceMode);
		
		if (BakedOutputObject)
		{
			UBlueprint* const PreviousBlueprint = BakedOutputObject->GetBlueprintIfValid();
			if (IsValid(PreviousBlueprint))
			{
				if (PackageParams.MatchesPackagePathNameExcludingBakeCounter(PreviousBlueprint))
				{
					PackageParams.GetBakeCounterFromBakedAsset(PreviousBlueprint, BakeCounter);
				}
			}
		}

		UPackage* Package = PackageParams.CreatePackageForObject(BlueprintName, BakeCounter);
		
		if (!IsValid(Package))
		{
			HOUDINI_LOG_WARNING(TEXT("Could not find or create a package for the blueprint of %s"), *(Actor->GetPathName()));
			continue;
		}

		OutBakeStats.NotifyPackageCreated(1);

		if (!Package->IsFullyLoaded())
			Package->FullyLoad();

		//Blueprint = FKismetEditorUtilities::CreateBlueprintFromActor(*BlueprintName, Package, Actor, false);
		// Find existing asset first (only relevant if we are in replacement mode). If the existing asset has a
		// different base class than the incoming actor, we reparent the blueprint to the new base class before
		// clearing the SCS graph and repopulating it from the temp actor.
		Asset = StaticFindObjectFast(UBlueprint::StaticClass(), Package, FName(*BlueprintName));
		if (IsValid(Asset))
		{
			UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
			if (IsValid(Blueprint))
			{
				if (Blueprint->GeneratedClass && Blueprint->GeneratedClass != Actor->GetClass())
				{
					// Close editors opened on existing asset if applicable
					if (Asset && bIsAssetEditorSubsystemValid && AssetEditorSubsystem->FindEditorForAsset(Asset, false) != nullptr)
					{
						AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
						AssetsToReOpenEditors.Add(Asset);
					}

					Blueprint->ParentClass = Actor->GetClass();

					FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
					FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
					FKismetEditorUtilities::CompileBlueprint(Blueprint);
				}
			}
		}
		else if (Asset && !IsValid(Asset))
		{
			// Rename to pending kill so that we can use the desired name
			const FString AssetPendingKillName(BlueprintName + "_PENDING_KILL");
			RenameAsset(Asset, AssetPendingKillName, true);
			Asset = nullptr;
		}

		bool bCreatedNewBlueprint = false;
		if (!Asset)
		{
			UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
			Factory->ParentClass = Actor->GetClass();

			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

			Asset = AssetToolsModule.Get().CreateAsset(
				BlueprintName, PackageParams.GetPackagePath(),
				UBlueprint::StaticClass(), Factory, FName("ContentBrowserNewAsset"));

			if (Asset)
				bCreatedNewBlueprint = true;
		}

		UBlueprint* Blueprint = Cast<UBlueprint>(Asset);

		if (!IsValid(Blueprint))
		{
			HOUDINI_LOG_WARNING(
				TEXT("Found an asset at %s/%s, but it was not a blueprint or was pending kill."),
				*(InBakeFolder.Path), *BlueprintName);
			
			continue;
		}

		if (bCreatedNewBlueprint)
		{
			OutBakeStats.NotifyObjectsCreated(Blueprint->GetClass()->GetName(), 1);
		}
		else
		{
			OutBakeStats.NotifyObjectsUpdated(Blueprint->GetClass()->GetName(), 1);
		}
		
		// Close editors opened on existing asset if applicable
		if (Blueprint && bIsAssetEditorSubsystemValid && AssetEditorSubsystem->FindEditorForAsset(Blueprint, false) != nullptr)
		{
			AssetEditorSubsystem->CloseAllEditorsForAsset(Blueprint);
			AssetsToReOpenEditors.Add(Blueprint);
		}
		
		// Record the blueprint as the previous bake blueprint and clear the info of the temp bake actor/component
		if (BakedOutputObject)
		{
			BakedOutputObject->Blueprint = FSoftObjectPath(Blueprint).ToString();
			BakedOutputObject->Actor.Empty();
			// TODO: Set the baked component to the corresponding component in the blueprint?
			BakedOutputObject->BakedComponent.Empty();
		}
		
		OutBlueprints.Add(Blueprint);
		BakedActorMap[Actor] = Blueprint;

		// Clear old Blueprint Node tree
		{
			USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;

			int32 NodeSize = SCS->GetAllNodes().Num();
			for (int32 n = NodeSize - 1; n >= 0; --n)
				SCS->RemoveNode(SCS->GetAllNodes()[n]);
		}

		constexpr bool bRenameComponents = true;
		FHoudiniEngineBakeUtils::CopyActorContentsToBlueprint(Actor, Blueprint, bRenameComponents);

		// Save the created BP package.
		Package->MarkPackageDirty();
		OutPackagesToSave.Add(Package);
	}

	// Destroy the actors that were baked
	for (const auto& BakedActorEntry : BakedActorMap)
	{
		AActor* const Actor = BakedActorEntry.Key;
		if (!IsValid(Actor))
			continue;

		UWorld* World = Actor->GetWorld();
		if (!World)
			World = GWorld;
		
		if (World)
			World->EditorDestroyActor(Actor, true);
	}

	// Re-open asset editors for updated blueprints that were open in editors
	if (bIsAssetEditorSubsystemValid && AssetsToReOpenEditors.Num() > 0)
	{
		for (UObject* Asset : AssetsToReOpenEditors)
		{
			if (IsValid(Asset))
			{
				AssetEditorSubsystem->OpenEditorForAsset(Asset);
			}
		}
	}

	return true;
}

bool
FHoudiniEngineBakeUtils::BakePDGTOPNodeBlueprints(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNode* InNode,
	bool bInIsAutoBake,
	const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode,
	bool bInRecenterBakedActors,
	TArray<UBlueprint*>& OutBlueprints,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats)
{
	TArray<AActor*> BPActors;

	if (!IsValid(InPDGAssetLink))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::BakePDGBlueprint]: InPDGAssetLink is null"));
		return false;
	}
		
	if (!IsValid(InNode))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::BakePDGBlueprint]: InNode is null"));
		return false;
	}

	const bool bReplaceAssets = InPDGBakePackageReplaceMode == EPDGBakePackageReplaceModeOption::ReplaceExistingAssets; 
	
	// Bake PDG output to new actors
	// bInBakeForBlueprint == true will skip landscapes and instanced actor components
	const bool bInBakeForBlueprint = true;
	TArray<FHoudiniEngineBakedActor> BakedActors;
	bool bSuccess = BakePDGTOPNodeOutputsKeepActors(
		InPDGAssetLink,
		InNode,
		bInBakeForBlueprint,
		bInIsAutoBake,
		InPDGBakePackageReplaceMode,
		BakedActors,
		OutPackagesToSave,
		OutBakeStats
	);

	if (bSuccess)
	{
		AActor* OwnerActor = InPDGAssetLink->GetOwnerActor();
		bSuccess = BakeBlueprintsFromBakedActors(
			BakedActors,
			bInRecenterBakedActors,
			bReplaceAssets,
			InPDGAssetLink->AssetName,
			IsValid(OwnerActor) ? OwnerActor->GetName() : FString(),
			InPDGAssetLink->BakeFolder,
			nullptr,
			&InNode->GetBakedWorkResultObjectsOutputs(),
			OutBlueprints,
			OutPackagesToSave,
			OutBakeStats);
	}
	
	return bSuccess;
}

bool
FHoudiniEngineBakeUtils::BakePDGTOPNodeBlueprints(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode, bool bInIsAutoBake, const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode, bool bInRecenterBakedActors)
{
	TArray<UBlueprint*> Blueprints;
	TArray<UPackage*> PackagesToSave;
	FHoudiniEngineOutputStats BakeStats;
	
	if (!IsValid(InPDGAssetLink))
		return false;

	const bool bSuccess = BakePDGTOPNodeBlueprints(
		InPDGAssetLink,
		InTOPNode,
		bInIsAutoBake,
		InPDGBakePackageReplaceMode,
		bInRecenterBakedActors,
		Blueprints,
		PackagesToSave,
		BakeStats);

	// Compile the new/updated blueprints
	for (UBlueprint* const Blueprint : Blueprints)
	{
		if (!IsValid(Blueprint))
			continue;
		
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	}
	FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

	// Sync the CB to the baked objects
	if(GEditor && Blueprints.Num() > 0)
	{
		TArray<UObject*> Assets;
		Assets.Reserve(Blueprints.Num());
		for (UBlueprint* Blueprint : Blueprints)
		{
			Assets.Add(Blueprint);
		}
		GEditor->SyncBrowserToObjects(Assets);
	}

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakeStats.NumPackagesCreated, BakeStats.NumPackagesUpdated } );
		FHoudiniEngine::Get().FinishTaskSlateNotification( FText::FromString(Msg) );
	}
	
	TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	return bSuccess;
}

bool
FHoudiniEngineBakeUtils::BakePDGTOPNetworkBlueprints(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNetwork* InNetwork,
	const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode,
	bool bInRecenterBakedActors,
	TArray<UBlueprint*>& OutBlueprints,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats)
{
	if (!IsValid(InPDGAssetLink))
		return false;

	if (!IsValid(InNetwork))
		return false;

	const bool bIsAutoBake = false;
	bool bSuccess = true;
	for (UTOPNode* Node : InNetwork->AllTOPNodes)
	{
		if (!IsValid(Node))
			continue;
		
		bSuccess &= BakePDGTOPNodeBlueprints(InPDGAssetLink, Node, bIsAutoBake, InPDGBakePackageReplaceMode, bInRecenterBakedActors, OutBlueprints, OutPackagesToSave, OutBakeStats);
	}

	return bSuccess;
}

bool
FHoudiniEngineBakeUtils::BakePDGAssetLinkBlueprints(UHoudiniPDGAssetLink* InPDGAssetLink, const EPDGBakeSelectionOption InBakeSelectionOption, const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode, bool bInRecenterBakedActors)
{
	TArray<UBlueprint*> Blueprints;
	TArray<UPackage*> PackagesToSave;
	FHoudiniEngineOutputStats BakeStats;
	
	if (!IsValid(InPDGAssetLink))
		return false;

	const bool bIsAutoBake = false;
	bool bSuccess = true;
	switch(InBakeSelectionOption)
	{
		case EPDGBakeSelectionOption::All:
			for (UTOPNetwork* Network : InPDGAssetLink->AllTOPNetworks)
			{
				if (!IsValid(Network))
					continue;
				
				for (UTOPNode* Node : Network->AllTOPNodes)
				{
					if (!IsValid(Node))
						continue;
					
					bSuccess &= BakePDGTOPNodeBlueprints(InPDGAssetLink, Node, bIsAutoBake, InPDGBakePackageReplaceMode, bInRecenterBakedActors, Blueprints, PackagesToSave, BakeStats);
				}
			}
			break;
		case EPDGBakeSelectionOption::SelectedNetwork:
			bSuccess &= BakePDGTOPNetworkBlueprints(
				InPDGAssetLink,
				InPDGAssetLink->GetSelectedTOPNetwork(),
				InPDGBakePackageReplaceMode,
				bInRecenterBakedActors,
				Blueprints,
				PackagesToSave,
				BakeStats);
		case EPDGBakeSelectionOption::SelectedNode:
			bSuccess &= BakePDGTOPNodeBlueprints(
				InPDGAssetLink,
				InPDGAssetLink->GetSelectedTOPNode(),
				bIsAutoBake,
				InPDGBakePackageReplaceMode,
				bInRecenterBakedActors,
				Blueprints,
				PackagesToSave,
				BakeStats);
	}

	// Compile the new/updated blueprints
	for (UBlueprint* const Blueprint : Blueprints)
	{
		if (!IsValid(Blueprint))
			continue;
		
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	}
	FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

	// Sync the CB to the baked objects
	if(GEditor && Blueprints.Num() > 0)
	{
		TArray<UObject*> Assets;
		Assets.Reserve(Blueprints.Num());
		for (UBlueprint* Blueprint : Blueprints)
		{
			Assets.Add(Blueprint);
		}
		GEditor->SyncBrowserToObjects(Assets);
	}

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakeStats.NumPackagesCreated, BakeStats.NumPackagesUpdated } );
		FHoudiniEngine::Get().FinishTaskSlateNotification( FText::FromString(Msg) );
	}
	
	TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	// Broadcast that the bake is complete
	InPDGAssetLink->HandleOnPostBake(bSuccess);
	
	return bSuccess;
}

bool
FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
	const FString& InLevelPath,
	ULevel*& OutDesiredLevel,
	UWorld*& OutDesiredWorld,
	bool& OutCreatedPackage)
{
	OutDesiredLevel = nullptr;
	OutDesiredWorld = nullptr;
	if (InLevelPath.IsEmpty())
	{
		OutDesiredWorld = GWorld;
		OutDesiredLevel = GWorld->GetCurrentLevel();
	}
	else
	{
		OutCreatedPackage = false;

		UWorld* FoundWorld = nullptr;
		ULevel* FoundLevel = nullptr;		
		bool bActorInWorld = false;
		if (FHoudiniEngineUtils::FindWorldAndLevelForSpawning(
			GWorld,
			InLevelPath,
			true,
			FoundWorld,
			FoundLevel,
			OutCreatedPackage,
			bActorInWorld))
		{
			OutDesiredLevel = FoundLevel;
			OutDesiredWorld = FoundWorld;
		}
	}

	return ((OutDesiredWorld != nullptr) && (OutDesiredLevel != nullptr));
}


bool
FHoudiniEngineBakeUtils::FindDesiredBakeActorFromBakeActorName(
	const FString& InBakeActorName,
	const TSubclassOf<AActor>& InBakeActorClass,
	ULevel* InLevel,
	AActor*& OutActor,
	bool bInNoPendingKillActors,
	bool bRenamePendingKillActor)
{
	OutActor = nullptr;
	
	if (!IsValid(InLevel))
		return false;

	UWorld* const World = InLevel->GetWorld();
	if (!IsValid(World))
		return false;

	// Look for an actor with the given name in the world
	const FName BakeActorFName(InBakeActorName);
	const TSubclassOf<AActor> ActorClass = IsValid(InBakeActorClass.Get()) ? InBakeActorClass.Get() : AActor::StaticClass();
	AActor* FoundActor = Cast<AActor>(StaticFindObjectFast(ActorClass.Get(), InLevel, BakeActorFName));
	// for (TActorIterator<AActor> Iter(World, AActor::StaticClass(), EActorIteratorFlags::AllActors); Iter; ++Iter)
	// {
	// 	AActor* const Actor = *Iter;
	// 	if (Actor->GetFName() == BakeActorFName && Actor->GetLevel() == InLevel)
	// 	{
	// 		// Found the actor
	// 		FoundActor = Actor;
	// 		break;
	// 	}
	// }

	// If we found an actor and it is pending kill, rename it and don't use it
	if (FoundActor)
	{
		if (!IsValid(FoundActor))
		{
			if (bRenamePendingKillActor)
			{
				// FoundActor->Rename(
    //                 *MakeUniqueObjectNameIfNeeded(
    //                     FoundActor->GetOuter(),
    //                     FoundActor->GetClass(),
    //                     FName(FoundActor->GetName() + "_Pending_Kill")).ToString());
				RenameAndRelabelActor(
					FoundActor,
                    *MakeUniqueObjectNameIfNeeded(
                        FoundActor->GetOuter(),
                        FoundActor->GetClass(),
                        FoundActor->GetName() + "_Pending_Kill",
                        FoundActor),
                    false);
			}
			if (bInNoPendingKillActors)
				FoundActor = nullptr;
			else
				OutActor = FoundActor;
		}
		else
		{
			OutActor = FoundActor;
		}
	}

	return true;
}

bool FHoudiniEngineBakeUtils::FindUnrealBakeActor(
	const FHoudiniOutputObject& InOutputObject,
	const FHoudiniBakedOutputObject& InBakedOutputObject,
	const TArray<FHoudiniEngineBakedActor>& InAllBakedActors,
	ULevel* InLevel,
	FName InDefaultActorName,
	bool bInReplaceActorBakeMode,
	AActor* InFallbackActor,
	AActor*& OutFoundActor,
	bool& bOutHasBakeActorName,
	FName& OutBakeActorName)
{
	// Determine the desired actor class via unreal_bake_actor_class
	TSubclassOf<AActor> BakeActorClass = GetBakeActorClassOverride(InOutputObject);
	if (!IsValid(BakeActorClass.Get()))
		BakeActorClass = AActor::StaticClass();
	
	// Determine desired actor name via unreal_bake_actor, fallback to InDefaultActorName
	OutBakeActorName = NAME_None;
	OutFoundActor = nullptr;
	bOutHasBakeActorName = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_BAKE_ACTOR);
	if (bOutHasBakeActorName)
	{
		const FString& BakeActorNameStr = InOutputObject.CachedAttributes[HAPI_UNREAL_ATTRIB_BAKE_ACTOR];
		if (BakeActorNameStr.IsEmpty())
		{
			OutBakeActorName = NAME_None;
			bOutHasBakeActorName = false;
		}
		else
		{
			OutBakeActorName = FName(BakeActorNameStr, NAME_NO_NUMBER_INTERNAL);
			// We have a bake actor name, look for the actor
			AActor* BakeNameActor = nullptr;
			if (FindDesiredBakeActorFromBakeActorName(BakeActorNameStr, BakeActorClass, InLevel, BakeNameActor))
			{
				// Found an actor with that name, check that we "own" it (we created in during baking previously)
				AActor* IncrementedBakedActor = nullptr;
				for (const FHoudiniEngineBakedActor& BakedActor : InAllBakedActors)
				{
					if (!IsValid(BakedActor.Actor))
						continue;
					// Ensure the class is of the appropriate type
					if (!BakedActor.Actor->IsA(BakeActorClass.Get()))
						continue;
					if (BakedActor.Actor == BakeNameActor)
					{
						OutFoundActor = BakeNameActor;
						break;
					}
					else if (!IncrementedBakedActor && BakedActor.ActorBakeName == OutBakeActorName)
					{
						// Found an actor we have baked named OutBakeActorName_# (incremental version of our desired name)
						IncrementedBakedActor = BakedActor.Actor;
					}
				}
				if (!OutFoundActor && IncrementedBakedActor)
					OutFoundActor = IncrementedBakedActor;
			}
		}
	}

	// If unreal_bake_actor is not set, or is blank, fallback to InDefaultActorName
	if (!bOutHasBakeActorName || (OutBakeActorName.IsNone() || OutBakeActorName.ToString().TrimStartAndEnd().IsEmpty()))
		OutBakeActorName = InDefaultActorName;

	if (!OutFoundActor)
	{
		// If in replace mode, use previous bake actor if valid and in InLevel
		if (bInReplaceActorBakeMode)
		{
			const FSoftObjectPath PrevActorPath(InBakedOutputObject.Actor);
			const FString ActorPath = PrevActorPath.IsSubobject()
                ? PrevActorPath.GetAssetPathString() + ":" + PrevActorPath.GetSubPathString()
                : PrevActorPath.GetAssetPathString();
			const FString LevelPath = IsValid(InLevel) ? InLevel->GetPathName() : "";
			if (PrevActorPath.IsValid() && (LevelPath.IsEmpty() || ActorPath.StartsWith(LevelPath)))
			{
				AActor* const PrevBakedActor = InBakedOutputObject.GetActorIfValid();
				if (IsValid(PrevBakedActor) && PrevBakedActor->IsA(BakeActorClass.Get()))
					OutFoundActor = PrevBakedActor;
			}
		}

		// Fallback to InFallbackActor if valid and in InLevel
		if (!OutFoundActor && IsValid(InFallbackActor) && (!InLevel || InFallbackActor->GetLevel() == InLevel))
		{
			if (IsValid(InFallbackActor) && InFallbackActor->IsA(BakeActorClass.Get()))
				OutFoundActor = InFallbackActor;
		}
	}

	return true;
}

AActor*
FHoudiniEngineBakeUtils::FindExistingActor_Bake(
	UWorld* InWorld,
	UHoudiniOutput* InOutput,	
	const FString& InActorName,
	const FString& InPackagePath,
	UWorld*& OutWorld,
	ULevel*& OutLevel,
	bool& bCreatedPackage)
{
	bCreatedPackage = false;

	// Try to Locate a previous actor
	AActor* FoundActor = FHoudiniEngineUtils::FindOrRenameInvalidActor<AActor>(InWorld, InActorName, FoundActor);
	if (FoundActor)
		FoundActor->Destroy(); // nuke it!

	if (FoundActor)
	{
		// TODO: make sure that the found is actor is actually assigned to the level defined by package path.
		//       If the found actor is not from that level, it should be moved there.

		OutWorld = FoundActor->GetWorld();
		OutLevel = FoundActor->GetLevel();
	}
	else
	{
		// Actor is not present, BUT target package might be loaded. Find the appropriate World and Level for spawning. 
		bool bActorInWorld = false;
		const bool bResult = FHoudiniEngineUtils::FindWorldAndLevelForSpawning(
			InWorld,
			InPackagePath,
			true,
			OutWorld,
			OutLevel,
			bCreatedPackage,
			bActorInWorld);

		if (!bResult)
		{
			return nullptr;
		}

		if (!bActorInWorld)
		{
			// The OutLevel is not present in the current world which means we might
			// still find the tile actor in OutWorld.
			FoundActor = FHoudiniEngineUtils::FindActorInWorld<AActor>(OutWorld, FName(InActorName));
		}
	}

	return FoundActor;
}

bool
FHoudiniEngineBakeUtils::CheckForAndRefineHoudiniProxyMesh(
	UHoudiniAssetComponent* InHoudiniAssetComponent,
	bool bInReplacePreviousBake,
	EHoudiniEngineBakeOption InBakeOption,
	bool bInRemoveHACOutputOnSuccess,
	bool bInRecenterBakedActors,
	bool& bOutNeedsReCook)
{
	if (!IsValid(InHoudiniAssetComponent))
	{
		return false;
	}
		
	// Handle proxies: if the output has any current proxies, first refine them
	bOutNeedsReCook = false;
	if (InHoudiniAssetComponent->HasAnyCurrentProxyOutput())
	{
		bool bNeedsRebuildOrDelete;
		bool bInvalidState;
		const bool bCookedDataAvailable = InHoudiniAssetComponent->IsHoudiniCookedDataAvailable(bNeedsRebuildOrDelete, bInvalidState);

		if (bCookedDataAvailable)
		{
			// Cook data is available, refine the mesh
			AHoudiniAssetActor* HoudiniActor = Cast<AHoudiniAssetActor>(InHoudiniAssetComponent->GetOwner());
			if (IsValid(HoudiniActor))
			{
				FHoudiniEngineCommands::RefineHoudiniProxyMeshActorArrayToStaticMeshes({ HoudiniActor });
			}
		}
		else if (!bNeedsRebuildOrDelete && !bInvalidState)
		{
			// A cook is needed: request the cook, but with no proxy and with a bake after cook
			InHoudiniAssetComponent->SetNoProxyMeshNextCookRequested(true);
			// Only
			if (!InHoudiniAssetComponent->IsBakeAfterNextCookEnabled() || !InHoudiniAssetComponent->GetOnPostCookBakeDelegate().IsBound())
			{
				InHoudiniAssetComponent->GetOnPostCookBakeDelegate().BindLambda([bInReplacePreviousBake, InBakeOption, bInRemoveHACOutputOnSuccess, bInRecenterBakedActors](UHoudiniAssetComponent* InHAC) {
                    return FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(InHAC, bInReplacePreviousBake, InBakeOption, bInRemoveHACOutputOnSuccess, bInRecenterBakedActors);
                });
			}
			InHoudiniAssetComponent->MarkAsNeedCook();

			bOutNeedsReCook = true;

			// The cook has to complete first (asynchronously) before the bake can happen
			// The SetBakeAfterNextCookEnabled flag will result in a bake after cook
			return false;
		}
		else
		{
			// The HAC is in an unsupported state
			const EHoudiniAssetState AssetState = InHoudiniAssetComponent->GetAssetState();
			HOUDINI_LOG_ERROR(TEXT("Could not refine (in order to bake) %s, the asset is in an unsupported state: %s"), *(InHoudiniAssetComponent->GetPathName()), *(UEnum::GetValueAsString(AssetState)));
			return false;
		}
	}

	return true;
}

void
FHoudiniEngineBakeUtils::CenterActorToBoundingBoxCenter(AActor* InActor)
{
	if (!IsValid(InActor))
		return;

	USceneComponent * const RootComponent = InActor->GetRootComponent();
	if (!IsValid(RootComponent))
		return;

	// If the root component does not have any child components, then there is nothing to recenter
	if (RootComponent->GetNumChildrenComponents() <= 0)
		return;

	const bool bOnlyCollidingComponents = false;
	const bool bIncludeFromChildActors = true;
	FVector Origin;
	FVector BoxExtent;
	// InActor->GetActorBounds(bOnlyCollidingComponents, Origin, BoxExtent, bIncludeFromChildActors);
	FBox Box(ForceInit);

	InActor->ForEachComponent<UPrimitiveComponent>(bIncludeFromChildActors, [&](const UPrimitiveComponent* InPrimComp)
	{
		// Only use non-editor-only components for the bounds calculation (to exclude things like editor only sprite/billboard components)
		if (InPrimComp->IsRegistered() && !InPrimComp->IsEditorOnly() &&
			(!bOnlyCollidingComponents || InPrimComp->IsCollisionEnabled()))
		{
			Box += InPrimComp->Bounds.GetBox();
		}
	});
	Box.GetCenterAndExtents(Origin, BoxExtent);

	const FVector Delta = Origin - RootComponent->GetComponentLocation();
	// Actor->SetActorLocation(Origin);
	RootComponent->SetWorldLocation(Origin);

	for (USceneComponent* SceneComponent : RootComponent->GetAttachChildren())
	{
		if (!IsValid(SceneComponent))
			continue;
		
		SceneComponent->SetWorldLocation(SceneComponent->GetComponentLocation() - Delta);
	}
}

void
FHoudiniEngineBakeUtils::CenterActorsToBoundingBoxCenter(const TArray<AActor*>& InActors)
{
	for (AActor* Actor : InActors)
	{
		if (!IsValid(Actor))
			continue;

		CenterActorToBoundingBoxCenter(Actor);
	}
}

USceneComponent*
FHoudiniEngineBakeUtils::GetActorRootComponent(AActor* InActor, bool bCreateIfMissing, EComponentMobility::Type InMobilityIfCreated)
{
	USceneComponent* RootComponent = InActor->GetRootComponent();
	if (!IsValid(RootComponent))
	{
		RootComponent = NewObject<USceneComponent>(InActor, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional);

		// Change the creation method so the component is listed in the details panels
		InActor->SetRootComponent(RootComponent);
		InActor->AddInstanceComponent(RootComponent);
		RootComponent->RegisterComponent();
		RootComponent->SetMobility(InMobilityIfCreated);
	}

	return RootComponent;
}

FString
FHoudiniEngineBakeUtils::MakeUniqueObjectNameIfNeeded(UObject* InOuter, const UClass* InClass, const FString& InName, UObject* InObjectThatWouldBeRenamed)
{
	if (IsValid(InObjectThatWouldBeRenamed))
	{
		const FName CurrentName = InObjectThatWouldBeRenamed->GetFName();
		if (CurrentName.ToString() == InName)
			return InName;

		// Check if the prefix matches (without counter suffix) the new name
		// In other words, if InName is 'my_actor' and the object is already an increment of it, 'my_actor_5' then
		// don't we can just keep the current name
		if (CurrentName.GetPlainNameString() == InName)
			return CurrentName.ToString();
	}

	UObject* ExistingObject = nullptr;
	FName CandidateName(InName);
	bool bAppendedNumber = false;
	// Do our own loop for generate suffixes as sequentially as possible. If this turns out to be expensive we can
	// revert to MakeUniqueObjectName.
	// return MakeUniqueObjectName(InOuter, InClass, CandidateName).ToString();
	do
	{
		if (InOuter == ANY_PACKAGE)
		{
			ExistingObject = StaticFindObject(nullptr, ANY_PACKAGE, *(CandidateName.ToString()));
		}
		else
		{
			ExistingObject = StaticFindObjectFast(nullptr, InOuter, CandidateName);
		}

		if (ExistingObject)
		{
			// We don't want to create unique names when actors are saved in their own package 
			// because we don't care about the name, we only care about the label.
			AActor* ExistingActor = Cast<AActor>(ExistingObject);
			AActor* RenamedActor = Cast<AActor>(InObjectThatWouldBeRenamed);
			if (ExistingActor && ExistingActor->IsPackageExternal() && RenamedActor && RenamedActor->IsPackageExternal())
			{
				return InName;
			}

			if (!bAppendedNumber)
			{
				const bool bSplitName = false;
				CandidateName = FName(*InName, NAME_EXTERNAL_TO_INTERNAL(1), FNAME_Add, bSplitName);
				bAppendedNumber = true;
			}
			else
			{
				CandidateName.SetNumber(CandidateName.GetNumber() + 1);
			}
			// CandidateName = FString::Printf(TEXT("%s_%d"), *InName, ++Counter);
		}
	} while (ExistingObject);

	return CandidateName.ToString();
}

FName
FHoudiniEngineBakeUtils::GetOutlinerFolderPath(const FHoudiniOutputObject& InOutputObject, FName InDefaultFolder)
{
	const FString* FolderPathPtr = InOutputObject.CachedAttributes.Find(HAPI_UNREAL_ATTRIB_BAKE_OUTLINER_FOLDER);
	if (FolderPathPtr && !FolderPathPtr->IsEmpty())
		return FName(*FolderPathPtr);
	else
		return InDefaultFolder;
}

bool
FHoudiniEngineBakeUtils::SetOutlinerFolderPath(AActor* InActor, const FHoudiniOutputObject& InOutputObject, FName InDefaultFolder)
{
	if (!IsValid(InActor))
		return false;

	InActor->SetFolderPath(GetOutlinerFolderPath(InOutputObject, InDefaultFolder));
	return true;
}

uint32
FHoudiniEngineBakeUtils::DestroyPreviousBakeOutput(
	FHoudiniBakedOutputObject& InBakedOutputObject,
	bool bInDestroyBakedComponent,
	bool bInDestroyBakedInstancedActors,
	bool bInDestroyBakedInstancedComponents)
{
	uint32 NumDeleted = 0;

	if (bInDestroyBakedComponent)
	{
		UActorComponent* Component = Cast<UActorComponent>(InBakedOutputObject.GetBakedComponentIfValid());
		if (Component)
		{
			if (RemovePreviouslyBakedComponent(Component))
			{
				InBakedOutputObject.BakedComponent = nullptr;
				NumDeleted++;
			}
		}
	}

	if (bInDestroyBakedInstancedActors)
	{
		for (const FString& ActorPathStr : InBakedOutputObject.InstancedActors)
		{
			const FSoftObjectPath ActorPath(ActorPathStr);

			if (!ActorPath.IsValid())
				continue;

			AActor* Actor = Cast<AActor>(ActorPath.TryLoad());
			if (IsValid(Actor))
			{
				UWorld* World = Actor->GetWorld();
				if (IsValid(World))
				{
#if WITH_EDITOR
					World->EditorDestroyActor(Actor, true);
#else
					World->DestroyActor(Actor);
#endif
					NumDeleted++;
				}
			}
		}
		InBakedOutputObject.InstancedActors.Empty();
	}

	if (bInDestroyBakedInstancedComponents)
	{
		for (const FString& ComponentPathStr : InBakedOutputObject.InstancedComponents)
		{
			const FSoftObjectPath ComponentPath(ComponentPathStr);

			if (!ComponentPath.IsValid())
				continue;

			UActorComponent* Component = Cast<UActorComponent>(ComponentPath.TryLoad());
			if (IsValid(Component))
			{
				if (RemovePreviouslyBakedComponent(Component))
					NumDeleted++;
			}
		}
		InBakedOutputObject.InstancedComponents.Empty();
	}
	
	return NumDeleted;
}

UMaterialInterface* FHoudiniEngineBakeUtils::BakeSingleMaterialToPackage(UMaterialInterface* InOriginalMaterial,
	const FHoudiniPackageParams & InPackageParams,
	TArray<UPackage*>& OutPackagesToSave,
	TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
	FHoudiniEngineOutputStats& OutBakeStats)
{
	if (!IsValid(InOriginalMaterial))
	{
		return nullptr;
	}

	// We only deal with materials.
	if (!InOriginalMaterial->IsA(UMaterial::StaticClass()) && !InOriginalMaterial->IsA(UMaterialInstance::StaticClass()))
	{
		return nullptr;
	}

	FString MaterialName = InOriginalMaterial->GetName();

	// Duplicate material resource.
	UMaterialInterface * DuplicatedMaterial = FHoudiniEngineBakeUtils::DuplicateMaterialAndCreatePackage(
		InOriginalMaterial, nullptr, MaterialName, InPackageParams,  OutPackagesToSave, InOutAlreadyBakedMaterialsMap,
		OutBakeStats);

	if (!IsValid(DuplicatedMaterial))
		return nullptr;
	
	return DuplicatedMaterial;
}

UClass*
FHoudiniEngineBakeUtils::GetBakeActorClassOverride(const FName& InActorClassName)
{
	if (InActorClassName.IsNone())
		return nullptr;

	// Try to the find the user specified actor class
	const FString ActorClassNameString = *InActorClassName.ToString();
	UClass* FoundClass = LoadClass<AActor>(nullptr, *ActorClassNameString);
	if (!IsValid(FoundClass))
		FoundClass = FindObjectSafe<UClass>(ANY_PACKAGE, *ActorClassNameString);

	if (!IsValid(FoundClass))
		return nullptr;

	// The class must be a child of AActor
	if (!FoundClass->IsChildOf<AActor>())
		return nullptr;

	return FoundClass;
}

UClass*
FHoudiniEngineBakeUtils::GetBakeActorClassOverride(const FHoudiniOutputObject& InOutputObject)
{
	// Find the unreal_bake_actor_class attribute in InOutputObject
	const FName ActorClassName = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_BAKE_ACTOR_CLASS) ?
		FName(InOutputObject.CachedAttributes.FindChecked(HAPI_UNREAL_ATTRIB_BAKE_ACTOR_CLASS)) : NAME_None;
	return GetBakeActorClassOverride(ActorClassName);
}

UActorFactory*
FHoudiniEngineBakeUtils::GetActorFactory(const FName& InActorClassName, TSubclassOf<AActor>& OutActorClass, const TSubclassOf<UActorFactory>& InFactoryClass, UObject* const InAsset)
{
	if (!GEditor)
		return nullptr;

	// If InActorClassName is not blank, try to find an actor factory that spawns actors of this class.
	OutActorClass = nullptr;
	if (!InActorClassName.IsNone())
	{
		UClass* const ActorClass = GetBakeActorClassOverride(InActorClassName);
		if (IsValid(ActorClass))
		{
			OutActorClass = ActorClass;
			UActorFactory* ActorFactory = GEditor->FindActorFactoryForActorClass(ActorClass);
			if (IsValid(ActorFactory))
				return ActorFactory;
			// If we could not find a factory that specifically spawns ActorClass, then fallback to the
			// UActorFactoryClass factory
			ActorFactory = GEditor->FindActorFactoryByClass(UActorFactoryClass::StaticClass());
			if (IsValid(ActorFactory))
				return ActorFactory;
		}
	}

	// If InActorClassName was blank, or we could not find a factory for it, then if InFactoryClass is valid, try to
	// find a factory of that class
	UClass const* const ActorFactoryClass = InFactoryClass.Get();
	if (IsValid(ActorFactoryClass))
	{
		UActorFactory* const ActorFactory = GEditor->FindActorFactoryByClass(ActorFactoryClass);
		if (IsValid(ActorFactory))
			return ActorFactory;
	}

	// If we couldn't find a factory via InActorClassName or InFactoryClass, then if InAsset was specified try to find
	// a factory that spawns actors for InAsset
	if (IsValid(InAsset))
	{
		UActorFactory* const ActorFactory = FActorFactoryAssetProxy::GetFactoryForAssetObject(InAsset);
		if (IsValid(ActorFactory))
			return ActorFactory;
	}

	HOUDINI_LOG_ERROR(
		TEXT("[FHoudiniEngineBakeUtils::GetActorFactory] Could not find actor factory:\n\tunreal_bake_actor_class = %s\n\tfallback actor factory class = %s\n\tasset = %s"),
		InActorClassName.IsNone() ? TEXT("not specified") : *InActorClassName.ToString(),
		IsValid(InFactoryClass.Get()) ? *InFactoryClass->GetName() : TEXT("null"),
		IsValid(InAsset) ? *InAsset->GetFullName() : TEXT("null"));
	
	return nullptr;
}

UActorFactory*
FHoudiniEngineBakeUtils::GetActorFactory(const FHoudiniOutputObject& InOutputObject, TSubclassOf<AActor>& OutActorClass, const TSubclassOf<UActorFactory>& InFactoryClass, UObject* InAsset)
{
	// Find the unreal_bake_actor_class attribute in InOutputObject
	const FName ActorClassName = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_BAKE_ACTOR_CLASS) ?
		FName(InOutputObject.CachedAttributes.FindChecked(HAPI_UNREAL_ATTRIB_BAKE_ACTOR_CLASS)) : NAME_None;
	return GetActorFactory(ActorClassName, OutActorClass, InFactoryClass, InAsset);
}

AActor*
FHoudiniEngineBakeUtils::SpawnBakeActor(UActorFactory* const InActorFactory, UObject* const InAsset, ULevel* const InLevel, const FTransform& InTransform, UHoudiniAssetComponent const* const InHAC, const TSubclassOf<AActor>& InActorClass, const FActorSpawnParameters& InSpawnParams)
{
	if (!IsValid(InActorFactory))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::SpawnBakeActor] Could not spawn an actor, since InActorFactory is nullptr."));
		return nullptr;
	}

	AActor* SpawnedActor = nullptr;
	if (InActorFactory->IsA<UActorFactoryClass>())
	{
		if (!IsValid(InActorClass.Get()))
		{
			HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::SpawnBakeActor] Could not spawn an actor: InActorFactory is a UActorFactoryClass, but InActorClass is nullptr."));
			return nullptr;
		}
		
		// UActorFactoryClass spawns an actor of the specified class (specified via InAsset to the CreateActor function)
		SpawnedActor = InActorFactory->CreateActor(InActorClass.Get(), InLevel, InTransform, InSpawnParams);
	}
	else
	{
		SpawnedActor = InActorFactory->CreateActor(InAsset, InLevel, InTransform, InSpawnParams);
	}

	if (IsValid(SpawnedActor))
	{
		PostSpawnBakeActor(SpawnedActor, InHAC);
	}
	
	return SpawnedActor;
}

void
FHoudiniEngineBakeUtils::PostSpawnBakeActor(AActor* const InSpawnedActor, UHoudiniAssetComponent const* const InHAC)
{
	if (!IsValid(InSpawnedActor))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::PostSpawnBakeActor] InSpawnedActor is null."));
		return;
	}

	if (!IsValid(InHAC))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::PostSpawnBakeActor] InHAC is null."));
		return;
	}

	// Copy the mobility of the HAC (root component of the houdini asset actor) to the root component of the spawned
	// bake actor
	USceneComponent* const BakedRootComponent = InSpawnedActor->GetRootComponent();
	if (IsValid(BakedRootComponent))
	{
		BakedRootComponent->SetMobility(InHAC->Mobility);
	}
}

ALandscapeProxy* FHoudiniEngineBakeUtils::MoveLandscapeComponentsToLevel(ULandscapeInfo* LandscapeInfo,
	const TArray<ULandscapeComponent*>& InComponents, ULevel* TargetLevel, FName NewProxyName)
{
	if (!IsValid(LandscapeInfo))
		return nullptr;
		
	ALandscape* Landscape = LandscapeInfo->LandscapeActor.Get();
	check(Landscape != nullptr);

	// Make sure references are in a different package (should be fixed up before calling this method)
	// Check the Physical Material is same package with Landscape
	if (Landscape->DefaultPhysMaterial && Landscape->DefaultPhysMaterial->GetOutermost() == Landscape->GetOutermost())
	{
		return nullptr;
	}

	// Check the LayerInfoObjects are not in same package as Landscape
	TArray<FLandscapeInfoLayerSettings>& Layers = LandscapeInfo->Layers;
	for (int32 i = 0; i < Layers.Num(); ++i)
	{
		const ULandscapeLayerInfoObject* LayerInfo = Layers[i].LayerInfoObj;
		if (LayerInfo && LayerInfo->GetOutermost() == Landscape->GetOutermost())
		{
			return nullptr;
		}
	}

	// Check the Landscape Materials are not in same package as moved components
	for (const ULandscapeComponent* Component : InComponents)
	{
		UMaterialInterface* LandscapeMaterial = Component->GetLandscapeMaterial();
		if (LandscapeMaterial && LandscapeMaterial->GetOutermost() == Component->GetOutermost())
		{
			return nullptr;
		}
	}
	
	UWorld* TargetWorld = TargetLevel->GetWorld();
	ALandscapeProxy* TargetProxy = nullptr;
	ULandscapeInfoMap& TargetLandscapeInfoMaps = ULandscapeInfoMap::GetLandscapeInfoMap(TargetWorld);

	// NOTE: We can't transfer landscape components between two different landscapes so we have to create a completely
	// new landscape.

	// TODO: Transfer foliage association from previous landscape tile to new landscape tile.

	// ALandscapeProxy* LandscapeProxy = LandscapeInfo->GetLandscapeProxyForLevel(TargetLevel);
	bool bSetPositionAndOffset = false;
	{
		// Create the streaming proxy in the target level, register it with the current landscape and then move
		// the components.
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = NewProxyName;
		SpawnParams.OverrideLevel = TargetLevel;
		ALandscapeStreamingProxy* StreamingProxy = TargetLevel->GetWorld()->SpawnActor<ALandscapeStreamingProxy>(SpawnParams);

		// copy shared properties to this new proxy
		StreamingProxy->SetActorLabel(StreamingProxy->GetName());
		StreamingProxy->GetSharedProperties(Landscape);
		StreamingProxy->LandscapeActor = Landscape;
		StreamingProxy->bHasLayersContent = Landscape->CanHaveLayersContent();
		LandscapeInfo->RegisterActor(StreamingProxy, false);
		bSetPositionAndOffset = true;
		
		TargetProxy = StreamingProxy;
	}

	if (Landscape->CanHaveLayersContent())
	{
		// Ensure the editing layer is cleared otherwise the MoveComponentsToProxy can crash.
		Landscape->SetEditingLayer(FGuid());
	}
	// Move landscape components
	ALandscapeProxy* TileActor = MoveLandscapeComponentsToProxy(LandscapeInfo, InComponents, TargetProxy, bSetPositionAndOffset, TargetLevel);
	
	return TileActor;
}

ALandscapeProxy*
FHoudiniEngineBakeUtils::MoveLandscapeComponentsToProxy(
	ULandscapeInfo* LandscapeInfo,
	const TArray<ULandscapeComponent*>& InComponents,
	ALandscapeProxy* LandscapeProxy,
	bool bSetPositionAndOffset,
	ULevel* TargetLevel)
{
	if (!LandscapeInfo)
		return nullptr;
	
	ALandscape* Landscape = LandscapeInfo->LandscapeActor.Get();
	check(Landscape != nullptr);
	
	struct FCompareULandscapeComponentBySectionBase
	{
		FORCEINLINE bool operator()(const ULandscapeComponent& A, const ULandscapeComponent& B) const
		{
			return (A.GetSectionBase().X == B.GetSectionBase().X) ? (A.GetSectionBase().Y < B.GetSectionBase().Y) : (A.GetSectionBase().X < B.GetSectionBase().X);
		}
	};
	TArray<ULandscapeComponent*> ComponentsToMove(InComponents);
	ComponentsToMove.Sort(FCompareULandscapeComponentBySectionBase());
		
	const int32 ComponentSizeVerts = Landscape->NumSubsections * (Landscape->SubsectionSizeQuads + 1);
	const int32 NeedHeightmapSize = 1 << FMath::CeilLogTwo(ComponentSizeVerts);

	TSet<ALandscapeProxy*> SelectProxies;
	TSet<ULandscapeComponent*> TargetSelectedComponents;
	TArray<ULandscapeHeightfieldCollisionComponent*> TargetSelectedCollisionComponents;
	for (ULandscapeComponent* Component : ComponentsToMove)
	{
		SelectProxies.Add(Component->GetLandscapeProxy());
		if (Component->GetLandscapeProxy() != LandscapeProxy && (!TargetLevel || Component->GetLandscapeProxy()->GetOuter() != TargetLevel))
		{
			TargetSelectedComponents.Add(Component);
		}

		ULandscapeHeightfieldCollisionComponent* CollisionComp = Component->CollisionComponent.Get();
		SelectProxies.Add(CollisionComp->GetLandscapeProxy());
		if (CollisionComp->GetLandscapeProxy() != LandscapeProxy && (!TargetLevel || CollisionComp->GetLandscapeProxy()->GetOuter() != TargetLevel))
		{
			TargetSelectedCollisionComponents.Add(CollisionComp);
		}
	}

	// Check which ones are need for height map change
	TSet<UTexture2D*> OldHeightmapTextures;
	for (ULandscapeComponent* Component : TargetSelectedComponents)
	{
		Component->Modify();
		OldHeightmapTextures.Add(Component->GetHeightmap());
	}

	// Need to split all the component which share Heightmap with selected components
	TMap<ULandscapeComponent*, bool> HeightmapUpdateComponents;
	HeightmapUpdateComponents.Reserve(TargetSelectedComponents.Num() * 4); // worst case
	for (ULandscapeComponent* Component : TargetSelectedComponents)
	{
		// Search neighbor only
		const int32 SearchX = Component->GetHeightmap()->Source.GetSizeX() / NeedHeightmapSize - 1;
		const int32 SearchY = Component->GetHeightmap()->Source.GetSizeY() / NeedHeightmapSize - 1;
		const FIntPoint ComponentBase = Component->GetSectionBase() / Component->ComponentSizeQuads;

		for (int32 Y = -SearchY; Y <= SearchY; ++Y)
		{
			for (int32 X = -SearchX; X <= SearchX; ++X)
			{
				ULandscapeComponent* const Neighbor = LandscapeInfo->XYtoComponentMap.FindRef(ComponentBase + FIntPoint(X, Y));
				if (Neighbor && Neighbor->GetHeightmap() == Component->GetHeightmap() && !HeightmapUpdateComponents.Contains(Neighbor))
				{
					Neighbor->Modify();
					bool bNeedsMoveToCurrentLevel = TargetSelectedComponents.Contains(Neighbor);
					HeightmapUpdateComponents.Add(Neighbor, bNeedsMoveToCurrentLevel);
				}
			}
		}
	}

	// Proxy position/offset needs to be set
	if(bSetPositionAndOffset)
	{
		// set proxy location
		// by default first component location
		ULandscapeComponent* FirstComponent = *TargetSelectedComponents.CreateConstIterator();
		LandscapeProxy->GetRootComponent()->SetWorldLocationAndRotation(FirstComponent->GetComponentLocation(), FirstComponent->GetComponentRotation());
		LandscapeProxy->LandscapeSectionOffset = FirstComponent->GetSectionBase();
	}

	// Hide(unregister) the new landscape if owning level currently in hidden state
	if (LandscapeProxy->GetLevel()->bIsVisible == false)
	{
		LandscapeProxy->UnregisterAllComponents();
	}

	// Changing Heightmap format for selected components
	for (const auto& HeightmapUpdateComponentPair : HeightmapUpdateComponents)
	{
		ALandscape::SplitHeightmap(HeightmapUpdateComponentPair.Key, HeightmapUpdateComponentPair.Value ? LandscapeProxy : nullptr);
	}

	// Delete if it is no referenced textures...
	for (UTexture2D* Texture : OldHeightmapTextures)
	{
		Texture->SetFlags(RF_Transactional);
		Texture->Modify();
		Texture->MarkPackageDirty();
		Texture->ClearFlags(RF_Standalone);
	}

	for (ALandscapeProxy* Proxy : SelectProxies)
	{
		Proxy->Modify();
	}

	LandscapeProxy->Modify();
	LandscapeProxy->MarkPackageDirty();

	// Handle XY-offset textures (these don't need splitting, as they aren't currently shared between components like heightmaps/weightmaps can be)
	for (ULandscapeComponent* Component : TargetSelectedComponents)
	{
		if (Component->XYOffsetmapTexture)
		{
			Component->XYOffsetmapTexture->Modify();
			Component->XYOffsetmapTexture->Rename(nullptr, LandscapeProxy);
		}
	}

	// Change Weight maps...
	{
		FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
		for (ULandscapeComponent* Component : TargetSelectedComponents)
		{
			Component->ReallocateWeightmaps(&LandscapeEdit, false, true, true, LandscapeProxy);
			Component->ForEachLayer([&](const FGuid& LayerGuid, FLandscapeLayerComponentData& LayerData)
			{
				FScopedSetLandscapeEditingLayer Scope(Landscape, LayerGuid);
				Component->ReallocateWeightmaps(&LandscapeEdit, true, true, true, LandscapeProxy);
			});
			Landscape->RequestLayersContentUpdateForceAll();
		}

		// Need to Repacking all the Weight map (to make it packed well...)
		for (ALandscapeProxy* Proxy : SelectProxies)
		{
			Proxy->RemoveInvalidWeightmaps();
		}
	}

	// Move the components to the Proxy actor
	// This does not use the MoveSelectedActorsToCurrentLevel path as there is no support to only move certain components.
	for (ULandscapeComponent* Component : TargetSelectedComponents)
	{
		// Need to move or recreate all related data (Height map, Weight map, maybe collision components, allocation info)
		Component->GetLandscapeProxy()->LandscapeComponents.Remove(Component);
		Component->UnregisterComponent();
		Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Component->InvalidateLightingCache();
		Component->Rename(nullptr, LandscapeProxy);
		LandscapeProxy->LandscapeComponents.Add(Component);
		Component->AttachToComponent(LandscapeProxy->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);

		// clear transient mobile data
		Component->MobileDataSourceHash.Invalidate();
		Component->MobileMaterialInterfaces.Reset();
		Component->MobileWeightmapTextures.Reset();

		Component->UpdateMaterialInstances();
	}
	LandscapeProxy->UpdateCachedHasLayersContent();

	for (ULandscapeHeightfieldCollisionComponent* Component : TargetSelectedCollisionComponents)
	{
		// Need to move or recreate all related data (Height map, Weight map, maybe collision components, allocation info)

		Component->GetLandscapeProxy()->CollisionComponents.Remove(Component);
		Component->UnregisterComponent();
		Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Component->Rename(nullptr, LandscapeProxy);
		LandscapeProxy->CollisionComponents.Add(Component);
		Component->AttachToComponent(LandscapeProxy->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);

		// Move any foliage associated
		AInstancedFoliageActor::MoveInstancesForComponentToLevel(Component, LandscapeProxy->GetLevel());
	}
		
	// Register our new components if destination landscape is registered in scene 
	if (LandscapeProxy->GetRootComponent()->IsRegistered())
	{
		LandscapeProxy->RegisterAllComponents();
	}

	for (ALandscapeProxy* Proxy : SelectProxies)
	{
		if (Proxy->GetRootComponent()->IsRegistered())
		{
			Proxy->RegisterAllComponents();
		}
	}

	return LandscapeProxy;
}

#undef LOCTEXT_NAMESPACE
