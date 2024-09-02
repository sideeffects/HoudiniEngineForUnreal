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

#include "HoudiniGeoImporter.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniPackageParams.h"
#include "HoudiniOutput.h"
#include "HoudiniOutputTranslator.h"
#include "HoudiniMeshTranslator.h"
#include "HoudiniLandscapeTranslator.h"
#include "HoudiniInstanceTranslator.h"
#include "HoudiniSplineTranslator.h"
#include "HoudiniDataTableTranslator.h"
#include "HoudiniSkeletalMeshTranslator.h"
#include "HoudiniAnimationTranslator.h"
#include "HoudiniGeometryCollectionTranslator.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniEngineRuntimeUtils.h"

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "PackageTools.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Editor.h"

#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"


UHoudiniGeoImporter::UHoudiniGeoImporter(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
	, SourceFilePath()
	, AbsoluteFilePath()
	, AbsoluteFileDirectory()
	, FileName()
	, FileExtension()
	, BakeRootFolder(TEXT("/Game/HoudiniEngine/Bake/"))
{
}

bool
UHoudiniGeoImporter::SetFilePath(const FString& InFilePath)
{
	SourceFilePath = InFilePath;
	if (!FPaths::FileExists(SourceFilePath))
	{
		// Cant find BGEO file
		HOUDINI_LOG_ERROR(TEXT("Houdini GEO Importer: could not find file %s!"), *InFilePath);
		return false;
	}

	// Make sure we're using absolute path!
	AbsoluteFilePath = FPaths::ConvertRelativePathToFull(SourceFilePath);

	// Split the file path
	FPaths::Split(AbsoluteFilePath, AbsoluteFileDirectory, FileName, FileExtension);

	// Handle .bgeo.sc correctly
	if (FileExtension.Equals(TEXT("sc")))
	{
		// append the bgeo to .sc
		FileExtension = FPaths::GetExtension(FileName) +TEXT(".") + FileExtension;
		// update the filename
		FileName = FPaths::GetBaseFilename(FileName);
	}

	if (FileExtension.IsEmpty())
		FileExtension = TEXT("bgeo");

	if (!FileExtension.StartsWith(TEXT("bgeo"), ESearchCase::IgnoreCase))
	{
		// Not a bgeo file!
		HOUDINI_LOG_ERROR(TEXT("Houdini GEO Importer: File %s is not a .bgeo or .bgeo.sc file!"), *SourceFilePath);
		return false;
	}

	//BGEOFilePath = BGEOPath + TEXT("/") + BGEOFileName + TEXT(".") + BGEOExtension;

	// Only use "/" for the output file path
	BakeRootFolder.ReplaceInline(TEXT("\\"), TEXT("/"));
	// Make sure the output folder ends with a "/"
	if (!BakeRootFolder.EndsWith("/"))
		BakeRootFolder += TEXT("/");

	// If we have't specified an outpout file name yet,  use the input file name
	if (OutputFilename.IsEmpty())
		OutputFilename = FileName;

	return true;
}

bool
UHoudiniGeoImporter::AutoStartHoudiniEngineSessionIfNeeded()
{
	if (FHoudiniEngine::Get().GetSession())
		return true;

	// Default first session already attempted to be created ? stop here?
	/*
	if (FHoudiniEngine::Get().GetFirstSessionCreated())
		return false;
	*/

	// Indicates that we've tried to start a session once no matter if it failed or succeed
	FHoudiniEngine::Get().SetFirstSessionCreated(true);
	if (!FHoudiniEngine::Get().RestartSession())
	{
		HOUDINI_LOG_ERROR(TEXT("Houdini GEO Importer: Couldn't start the default HoudiniEngine session!"));
		return false;
	}

	return true;
}

bool
UHoudiniGeoImporter::BuildOutputsForNode(
	const HAPI_NodeId& InNodeId, 
	TArray<UHoudiniOutput*>& InOldOutputs,
	TArray<UHoudiniOutput*>& OutNewOutputs,
	bool bInUseOutputNodes)
{
	FString Notification = TEXT("BGEO Importer: Getting output geos...");
	FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Notification));

	const bool bInAddOutputsToRootSet = true;
	return BuildAllOutputsForNode(InNodeId, this, InOldOutputs, OutNewOutputs, bInAddOutputsToRootSet, bInUseOutputNodes);
}

bool UHoudiniGeoImporter::CreateObjectsFromOutputs(
	TArray<UHoudiniOutput*>& InOutputs,
	FHoudiniPackageParams InPackageParams,
	const FHoudiniStaticMeshGenerationProperties& InStaticMeshGenerationProperties,
	const FMeshBuildSettings& InMeshBuildSettings,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancedOutputPartData>* OutInstancedOutputPartData)
{
	//
	// This isn't ideal but the reason we do this is because previously each 
	// method we call (i.e. CreateCurves) would filter the outputs we pass in.
	// This way, we loop through the array of outputs only once.
	//
	// The reason we cannot have methods for individual outputs is that some
	// output types, like curves, are merged together into a single blueprint.
	//
	TArray<UHoudiniOutput*> MeshOutputs;
	TArray<UHoudiniOutput*> CurveOutputs;
	TArray<UHoudiniOutput*> LandscapeOutputs;
	TArray<UHoudiniOutput*> LandscapeSplineOutputs;
	TArray<UHoudiniOutput*> InstancerOutputs;
	TArray<UHoudiniOutput*> DataTableOutputs;
	TArray<UHoudiniOutput*> SkeletalOutputs;
	TArray<UHoudiniOutput*> AnimSequenceOutputs;

	for (UHoudiniOutput* const Output : InOutputs)
	{
		switch (Output->GetType())
		{
		case EHoudiniOutputType::Mesh:
			MeshOutputs.Add(Output);
			break;
		case EHoudiniOutputType::Curve:
			CurveOutputs.Add(Output);
			break;
		case EHoudiniOutputType::Landscape:
			LandscapeOutputs.Add(Output);
			break;
		case EHoudiniOutputType::LandscapeSpline:
			LandscapeSplineOutputs.Add(Output);
			break;
		case EHoudiniOutputType::Instancer:
			InstancerOutputs.Add(Output);
			break;
		case EHoudiniOutputType::DataTable:
			DataTableOutputs.Add(Output);
			break;
		case EHoudiniOutputType::Skeletal:
			SkeletalOutputs.Add(Output);
			break;
		case EHoudiniOutputType::AnimSequence:
			AnimSequenceOutputs.Add(Output);
			break;
		}
	}

	if (!CreateStaticMeshes(MeshOutputs, InPackageParams, InStaticMeshGenerationProperties, InMeshBuildSettings))
		return false;

	if (!CreateCurves(CurveOutputs, InPackageParams))
		return false;

	if (!CreateLandscapes(LandscapeOutputs, InPackageParams))
		return false;

	if (!CreateLandscapeSplines(LandscapeSplineOutputs, InPackageParams))
		return false;

	if (OutInstancedOutputPartData)
	{
		if (!CreateInstancerOutputPartData(InstancerOutputs, *OutInstancedOutputPartData))
			return false;
	}
	else
	{
		if (!CreateInstancers(InOutputs, InstancerOutputs, InPackageParams))
			return false;
	}

	if (!CreateDataTables(DataTableOutputs, InPackageParams))
		return false;

	if (!CreateSkeletalMeshes(SkeletalOutputs, InPackageParams))
		return false;

	if (!CreateAnimSequences(AnimSequenceOutputs, InPackageParams))
		return false;

	return true;
}

bool
UHoudiniGeoImporter::CreateStaticMeshes(
	const TArray<UHoudiniOutput*>& InOutputs,
	FHoudiniPackageParams InPackageParams,
	const FHoudiniStaticMeshGenerationProperties& InStaticMeshGenerationProperties,
	const FMeshBuildSettings& InMeshBuildSettings)
{
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*> AllOutputMaterials;
	for (UHoudiniOutput* const CurOutput : InOutputs)
	{
		if (CurOutput->GetType() != EHoudiniOutputType::Mesh)
			continue;

		FString Notification = TEXT("BGEO Importer: Creating Static Meshes...");
		FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Notification));

		//FHoudiniMeshTranslator::CreateAllMeshesFromHoudiniOutput(CurOutput, OuterPackage, OuterComponent, OuterAsset);

		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> NewOutputObjects;
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> OldOutputObjects = CurOutput->GetOutputObjects();
		TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& AssignementMaterials = CurOutput->GetAssignementMaterials();
		TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& ReplacementMaterials = CurOutput->GetReplacementMaterials();

		// Iterate on all of the output's HGPO, creating meshes as we go
		for (const FHoudiniGeoPartObject& CurHGPO : CurOutput->GetHoudiniGeoPartObjects())
		{
			// Not a mesh, skip
			if (CurHGPO.Type != EHoudiniPartType::Mesh)
				continue;

			FHoudiniPackageParams PackageParams = GetPackageParamsForHGPO(CurHGPO, InPackageParams);

			UObject* const OuterComponent = nullptr;
			constexpr bool bForceRebuild = true;
			constexpr bool bSplitMeshSupport = false;
			FHoudiniMeshTranslator::CreateStaticMeshFromHoudiniGeoPartObject(
				CurHGPO,
				PackageParams,
				OldOutputObjects,
				NewOutputObjects,
				AssignementMaterials,
				ReplacementMaterials,
				AllOutputMaterials,
				OuterComponent,
				bForceRebuild,
				EHoudiniStaticMeshMethod::FMeshDescription,
				bSplitMeshSupport,
				InStaticMeshGenerationProperties,
				InMeshBuildSettings);

			for (auto& CurMat : AssignementMaterials)
			{
				// Adds the newly generated materials to the output materials array
				// This is to avoid recreating those same materials again
				if (!AllOutputMaterials.Contains(CurMat.Key))
					AllOutputMaterials.Add(CurMat);
			}
		}

		// Add all output objects and materials
		for (auto CurOutputPair : NewOutputObjects)
		{
			UObject* CurObj = CurOutputPair.Value.OutputObject;
			if (!IsValid(CurObj))
				continue;

			OutputObjects.Add(CurObj);
		}

		// Do the same for materials
		for (auto CurAssignmentMatPair : AssignementMaterials)
		{
			UObject* CurObj = CurAssignmentMatPair.Value;
			if (!IsValid(CurObj))
				continue;

			OutputObjects.Add(CurObj);
		}

		// Also assign to the output objects map as we may need the meshes to create instancers later
		CurOutput->SetOutputObjects(NewOutputObjects);
	}

	return true;
}

bool
UHoudiniGeoImporter::CreateCurves(const TArray<UHoudiniOutput*>& InOutputs, FHoudiniPackageParams InPackageParams)
{
	if (InOutputs.IsEmpty())
	{
		return true;
	}

	FString Notification = TEXT("BGEO Importer: Creating Curves...");
	FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Notification));

	FHoudiniPackageParams PackageParams = GetPackageParamsForType(
		InOutputs,
		InPackageParams,
		EHoudiniOutputType::Curve,
		EHoudiniPartType::Curve);

	// Create a Package for the BP
	PackageParams.ObjectName = TEXT("BP_") + PackageParams.ObjectName;
	PackageParams.ReplaceMode = EPackageReplaceMode::CreateNewAssets;

	FString PackageName;
	UPackage* BPPackage = PackageParams.CreatePackageForObject(PackageName);
	check(BPPackage);

	// Create and init a new Blueprint Actor
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), BPPackage, *PackageName, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), FName("HoudiniGeoImporter"));
	if (!Blueprint)
		return false;

	// Create a fake outer component that we'll use as a temporary outer for our curves
	UWorld* TempWorld = UWorld::CreateWorld(EWorldType::Inactive, false, TEXT("BGEOImporterTemp"), GetTransientPackage(), false);
	const FActorSpawnParameters ActorSpawnParameters;
	AActor* OuterActor = TempWorld->SpawnActor<AActor>(ActorSpawnParameters);
	USceneComponent* OuterComponent =
		NewObject<USceneComponent>(OuterActor, USceneComponent::GetDefaultSceneRootVariableName());

	for (UHoudiniOutput* const CurOutput : InOutputs)
	{
		check(CurOutput->GetType() == EHoudiniOutputType::Curve);

		// Output curve
		FHoudiniSplineTranslator::CreateAllSplinesFromHoudiniOutput(CurOutput, OuterComponent);

		// Prepare an ActorComponent array for AddComponentsToBlueprint()
		TArray<UActorComponent*> OutputComp;
		for (auto CurOutputPair : CurOutput->GetOutputObjects())
		{
			for (auto Component : CurOutputPair.Value.OutputComponents)
			{
				UActorComponent* CurObj = Cast<UActorComponent>(Component);
				if (!IsValid(CurObj))
					continue;

				OutputComp.Add(CurObj);
			}
		}

		// Transfer all the instancer components to the BP
		if (OutputComp.Num() > 0)
		{
			FKismetEditorUtilities::FAddComponentsToBlueprintParams Params;
			Params.HarvestMode = FKismetEditorUtilities::EAddComponentToBPHarvestMode::None;
			Params.OptionalNewRootNode = nullptr;
			Params.bKeepMobility = false;
			FKismetEditorUtilities::AddComponentsToBlueprint(Blueprint, OutputComp, Params);
		}
	}

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	// Add it to our output objects
	OutputObjects.Add(Blueprint);

	return true;
}

bool
UHoudiniGeoImporter::CreateLandscapes(const TArray<UHoudiniOutput*>& InOutputs, FHoudiniPackageParams InPackageParams)
{
	if (InOutputs.IsEmpty())
	{
		return true;
	}

	HOUDINI_LOG_WARNING(TEXT("Importing a landscape directly from BGEOs is not currently supported."));
	return false;

	/*
	// Before processing any of the output,
	// we need to get the min/max value for all Height volumes in this output (if any)
	float HoudiniHeightfieldOutputsGlobalMin = 0.f;
	float HoudiniHeightfieldOutputsGlobalMax = 0.f;
	FHoudiniLandscapeTranslator::CalcHeightGlobalZminZMax(InOutputs, HoudiniHeightfieldOutputsGlobalMin, HoudiniHeightfieldOutputsGlobalMax);

	UWorld* PersistentWorld = InParent->GetWorld();
	if(!PersistentWorld)
		PersistentWorld = GEditor ? GEditor->GetEditorWorldContext(false).World() : nullptr;

	if (!PersistentWorld)
		return false;

	for (auto& CurOutput : InOutputs)
	{
		if (CurOutput->GetType() != EHoudiniOutputType::Landscape)
			continue;

		FString Notification = TEXT("BGEO Importer: Creating Landscapes...");
		FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Notification));

		TArray<ALandscapeProxy*> EmptyInputLandscapes;
		UHoudiniAssetComponent* HAC = FHoudiniEngineUtils::GetOuterHoudiniAssetComponent(CurOutput);

		bool bCreatedNewMaps = false;
		ERuntimePackageMode RuntimePackageMode = ERuntimePackageMode::CookToTemp;
		switch(InPackageParams.PackageMode)
		{
			
			case EPackageMode::Bake:
				RuntimePackageMode = ERuntimePackageMode::Bake;
				break;
			case EPackageMode::CookToLevel_Invalid:
			case EPackageMode::CookToTemp:
			default:
				RuntimePackageMode = ERuntimePackageMode::CookToTemp;
				break;
		}
		TArray<TWeakObjectPtr<AActor>> CreatedUntrackedOutputs;
		FHoudiniLandscapeTranslator::CreateLandscape(
			CurOutput,
			CreatedUntrackedOutputs,
			EmptyInputLandscapes,
			EmptyInputLandscapes,
			HAC,
			TEXT("{object_name}_"),
			PersistentWorld,
			HoudiniHeightfieldOutputsGlobalMin, HoudiniHeightfieldOutputsGlobalMax,
			InPackageParams, bCreatedNewMaps,
			RuntimePackageMode);

		// Add all output objects
		for (auto CurOutputPair : CurOutput->GetOutputObjects())
		{
			UObject* CurObj = CurOutputPair.Value.OutputObject;
			if (!IsValid(CurObj))
				continue;

			OutputObjects.Add(CurObj);
		}
	}

	return true;
	*/
}


bool
UHoudiniGeoImporter::CreateLandscapeSplines(const TArray<UHoudiniOutput*>& InOutputs, FHoudiniPackageParams InPackageParams)
{
	if (InOutputs.IsEmpty())
	{
		return true;
	}

	HOUDINI_LOG_WARNING(TEXT("Importing landscape splines directly from BGEOs is not currently supported."));
	return false;
}


bool
UHoudiniGeoImporter::CreateInstancers(
	TArray<UHoudiniOutput*>& InAllOutputs,
	const TArray<UHoudiniOutput*>& InInstancerOutputs,
	FHoudiniPackageParams InPackageParams)
{
	if (InInstancerOutputs.IsEmpty())
	{
		return true;
	}

	FString Notification = TEXT("BGEO Importer: Creating Instancers...");
	FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Notification));

	FHoudiniPackageParams PackageParams = GetPackageParamsForType(
		InInstancerOutputs,
		InPackageParams,
		EHoudiniOutputType::Instancer,
		EHoudiniPartType::Instancer);

	// Create a fake outer component that we'll use as a temporary outer for our instancers
	USceneComponent* OuterComponent = NewObject<USceneComponent>();

	UWorld* const World = nullptr;

	// Now that all meshes have been created, process the instancers
	FHoudiniInstanceTranslator::CreateAllInstancersFromHoudiniOutputs(
		InAllOutputs, OuterComponent, PackageParams);

	bool bHasGeometryCollection = false;
	for (UHoudiniOutput* const CurOutput : InInstancerOutputs)
	{
		if (!bHasGeometryCollection && FHoudiniGeometryCollectionTranslator::IsGeometryCollectionInstancer(CurOutput))
		{
			bHasGeometryCollection = true;
			break;
		}
	}

	if (bHasGeometryCollection)
	{
		FHoudiniGeometryCollectionTranslator::SetupGeometryCollectionComponentFromOutputs(
			InAllOutputs, OuterComponent, PackageParams, World);

		if (InAllOutputs.Last()->GetType() != EHoudiniOutputType::GeometryCollection)
		{
			return false;
		}

		for (auto CurOutputPair : InAllOutputs.Last()->GetOutputObjects())
		{
			UObject* CurObj = CurOutputPair.Value.OutputObject;
			if (!IsValid(CurObj))
				continue;

			OutputObjects.Add(CurObj);
		}

		return true;
	}

	// Create a Package for the BP
	PackageParams.ObjectName = TEXT("BP_") + PackageParams.ObjectName;
	PackageParams.ReplaceMode = EPackageReplaceMode::CreateNewAssets;

	FString PackageName;
	UPackage* BPPackage = PackageParams.CreatePackageForObject(PackageName);
	check(BPPackage);

	// Create and init a new Blueprint Actor
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), BPPackage, *PackageName, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), FName("HoudiniGeoImporter"));
	if (!Blueprint)
		return false;

	for (auto& CurOutput : InInstancerOutputs)
	{
		// Transfer all the instancer components to the BP
		check(CurOutput->GetType() == EHoudiniOutputType::Instancer);

		// Prepare an ActorComponent array for AddComponentsToBlueprint()
		TArray<UActorComponent*> OutputComp;
		for (auto CurOutputPair : CurOutput->GetOutputObjects())
		{
			for (auto Component : CurOutputPair.Value.OutputComponents)
			{
				UActorComponent* CurObj = Cast<UActorComponent>(Component);
				if (!IsValid(CurObj))
					continue;

				OutputComp.Add(CurObj);
			}
		}

		if (OutputComp.Num() > 0)
		{
			FKismetEditorUtilities::FAddComponentsToBlueprintParams Params;
			Params.HarvestMode = FKismetEditorUtilities::EAddComponentToBPHarvestMode::None;
			Params.OptionalNewRootNode = nullptr;
			Params.bKeepMobility = false;
			FKismetEditorUtilities::AddComponentsToBlueprint(Blueprint, OutputComp, Params);
		}
	}

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	// Add it to our output objects
	OutputObjects.Add(Blueprint);

	return true;
}

bool
UHoudiniGeoImporter::CreateDataTables(
	const TArray<UHoudiniOutput*>& InOutputs,
	FHoudiniPackageParams InPackageParams)
{
	if (InOutputs.IsEmpty())
	{
		return true;
	}

	for (UHoudiniOutput* const CurOutput : InOutputs)
	{
		check(CurOutput->GetType() == EHoudiniOutputType::DataTable);

		FString Notification = TEXT("BGEO Importer: Creating Data Tables...");
		FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Notification));

		for (const FHoudiniGeoPartObject& CurHGPO : CurOutput->GetHoudiniGeoPartObjects())
		{
			if (CurHGPO.Type != EHoudiniPartType::DataTable)
			{
				continue;
			}

			// Check for a unreal_output_name if we are in bake mode
			FHoudiniPackageParams PackageParams = GetPackageParamsForHGPO(CurHGPO, InPackageParams);

			if (!FHoudiniDataTableTranslator::BuildDataTable(CurHGPO, CurOutput, PackageParams))
			{
				return false;
			}
		}

		// Add all output objects
		for (auto& CurOutputPair : CurOutput->GetOutputObjects())
		{
			UObject* const CurObj = CurOutputPair.Value.OutputObject;
			if (!IsValid(CurObj))
			{
				continue;
			}

			OutputObjects.Add(CurObj);
		}
	}

	return true;
}

bool
UHoudiniGeoImporter::CreateSkeletalMeshes(
	const TArray<UHoudiniOutput*>& InOutputs,
	FHoudiniPackageParams InPackageParams)
{
	if (InOutputs.IsEmpty())
	{
		return true;
	}

	FString Notification = TEXT("BGEO Importer: Creating Skeletal Meshes...");
	FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Notification));

	FHoudiniPackageParams PackageParams = GetPackageParamsForType(
		InOutputs,
		InPackageParams,
		EHoudiniOutputType::Skeletal,
		EHoudiniPartType::SkeletalMeshShape);

	for (UHoudiniOutput* const CurOutput : InOutputs)
	{
		check(CurOutput->GetType() == EHoudiniOutputType::Skeletal);

		TMap<FHoudiniMaterialIdentifier, UMaterialInterface*> OutputMaterials;
		UObject* const OuterComponent = nullptr;
		if (!FHoudiniSkeletalMeshTranslator::ProcessSkeletalMeshOutputs(CurOutput, PackageParams, OutputMaterials, OuterComponent))
		{
			return false;
		}

		// Add all output objects
		for (auto& CurOutputPair : CurOutput->GetOutputObjects())
		{
			UObject* const CurObj = CurOutputPair.Value.OutputObject;
			if (!IsValid(CurObj))
			{
				continue;
			}

			OutputObjects.Add(CurObj);
		}

		// Do the same for materials
		for (auto& CurOutputPair : OutputMaterials)
		{
			UObject* CurObj = CurOutputPair.Value;
			if (!IsValid(CurObj))
			{
				continue;
			}

			OutputObjects.Add(CurObj);
		}
	}

	return true;
}

bool
UHoudiniGeoImporter::CreateAnimSequences(
	const TArray<UHoudiniOutput*>& InOutputs,
	FHoudiniPackageParams InPackageParams)
{
	if (InOutputs.IsEmpty())
	{
		return true;
	}

	FString Notification = TEXT("BGEO Importer: Creating Animation Sequences...");
	FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Notification));

	// Look for the first unreal_output_name attribute on the animation sequence outputs and use that
	// for ObjectName
	FHoudiniPackageParams PackageParams = GetPackageParamsForType(
		InOutputs,
		InPackageParams,
		EHoudiniOutputType::AnimSequence,
		EHoudiniPartType::MotionClip);

	for (UHoudiniOutput* const CurOutput : InOutputs)
	{
		check(CurOutput->GetType() == EHoudiniOutputType::AnimSequence);

		UObject* const OuterComponent = nullptr;
		if (!FHoudiniAnimationTranslator::CreateAnimSequenceFromOutput(CurOutput, PackageParams, OuterComponent))
		{
			return false;
		}

		// Add all output objects
		for (auto& CurOutputPair : CurOutput->GetOutputObjects())
		{
			UObject* const CurObj = CurOutputPair.Value.OutputObject;
			if (!IsValid(CurObj))
			{
				continue;
			}

			OutputObjects.Add(CurObj);
		}
	}

	return true;
}

bool
UHoudiniGeoImporter::CreateInstancerOutputPartData(
	const TArray<UHoudiniOutput*>& InOutputs,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancedOutputPartData>& OutInstancedOutputPartData)
{
	if (InOutputs.IsEmpty())
	{
		return true;
	}

	for (UHoudiniOutput* const CurOutput : InOutputs)
	{
		check(CurOutput->GetType() == EHoudiniOutputType::Instancer)

		for (auto& HGPO : CurOutput->GetHoudiniGeoPartObjects())
		{
			FHoudiniOutputObjectIdentifier OutputIdentifier;
			OutputIdentifier.ObjectId = HGPO.ObjectId;
			OutputIdentifier.GeoId = HGPO.GeoId;
			OutputIdentifier.PartId = HGPO.PartId;
			OutputIdentifier.PartName = HGPO.PartName;
			
			OutInstancedOutputPartData.Add(OutputIdentifier, FHoudiniInstancedOutputPartData());
			FHoudiniInstancedOutputPartData *InstancedOutputData = OutInstancedOutputPartData.Find(OutputIdentifier); 
			// Create all the instancers and attach them to a fake outer component
			TSet<UObject*> InvisibleObjects; // Not used by this function.
			if (!FHoudiniInstanceTranslator::PopulateInstancedOutputPartData(HGPO, InOutputs, *InstancedOutputData, InvisibleObjects))
				return false;
		}
	}

	return true;
}

bool
UHoudiniGeoImporter::DeleteCreatedNode(const HAPI_NodeId& InNodeId)
{
	if (InNodeId < 0)
		return false;

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), InNodeId))
	{
		// Could not delete the bgeo's file sop !
		HOUDINI_LOG_WARNING(TEXT("Houdini GEO Importer: Could not delete HAPI File SOP."));
		return false;
	}

	return true;
}

bool 
UHoudiniGeoImporter::ImportBGEOFile(
	const FString& InBGEOFile, UObject* InParent, const FHoudiniPackageParams* InPackageParams,
	const FHoudiniStaticMeshGenerationProperties* InStaticMeshGenerationProperties,
	const FMeshBuildSettings* InMeshBuildSettings)
{
	if (InBGEOFile.IsEmpty())
		return false;
	
	// 1. Houdini Engine Session
	// See if we should/can start the default "first" HE session
	if (!AutoStartHoudiniEngineSessionIfNeeded())
		return false;

	// 2. Update the file paths
	if (!SetFilePath(InBGEOFile))
		return false;

	// 3. Load the BGEO file in HAPI
	HAPI_NodeId NodeId;
	if (!LoadBGEOFileInHAPI(NodeId))
		return false;
	
	// 4. Get the output from the file node
	TArray<UHoudiniOutput*> NewOutputs;
	TArray<UHoudiniOutput*> OldOutputs;
	if (!BuildOutputsForNode(NodeId, OldOutputs, NewOutputs, true))
		return false;

	// Failure lambda
	auto CleanUpAndReturn = [&NewOutputs](const bool& bReturnValue)
	{
		// Remove the output objects from the root set before returning false
		for (auto Out : NewOutputs)
			Out->RemoveFromRoot();

		return bReturnValue;
	};

	// Prepare the package used for creating the mesh, landscape and instancer pacakges
	FHoudiniPackageParams PackageParams;
	if (InPackageParams)
	{
		PackageParams = *InPackageParams;
	}
	else
	{
		PackageParams.PackageMode = EPackageMode::Bake;
		PackageParams.ReplaceMode = EPackageReplaceMode::ReplaceExistingAssets;

		PackageParams.BakeFolder = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName());
		PackageParams.TempCookFolder = FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();

		PackageParams.HoudiniAssetName = FString();
		PackageParams.HoudiniAssetActorName = FString();
		PackageParams.ObjectName = FPaths::GetBaseFilename(InParent->GetName());
	}

	if (!PackageParams.OuterPackage)
	{
		PackageParams.OuterPackage = InParent;
	}

	if (!PackageParams.ComponentGUID.IsValid())
	{
		// TODO: will need to reuse the GUID when reimporting?
		PackageParams.ComponentGUID = FGuid::NewGuid();
	}

	// 5. Create the static meshes in the outputs
	const FHoudiniStaticMeshGenerationProperties& StaticMeshGenerationProperties =
		InStaticMeshGenerationProperties?
		*InStaticMeshGenerationProperties :
		FHoudiniEngineRuntimeUtils::GetDefaultStaticMeshGenerationProperties();
	
	const FMeshBuildSettings& MeshBuildSettings =
		InMeshBuildSettings ? *InMeshBuildSettings : FHoudiniEngineRuntimeUtils::GetDefaultMeshBuildSettings();
		
	if (!CreateObjectsFromOutputs(NewOutputs, PackageParams, StaticMeshGenerationProperties, MeshBuildSettings))
		return CleanUpAndReturn(false);

	// Clean up and return true
	return CleanUpAndReturn(true);
}

bool
UHoudiniGeoImporter::OpenBGEOFile(const FString& InBGEOFile, HAPI_NodeId& OutNodeId, bool bInUseWorldComposition)
{
	if (InBGEOFile.IsEmpty())
		return false;
	
	// 1. Houdini Engine Session
	// See if we should/can start the default "first" HE session
	if (!AutoStartHoudiniEngineSessionIfNeeded())
		return false;

	if (!FPaths::FileExists(InBGEOFile))
	{
		// Cant find BGEO file
		HOUDINI_LOG_ERROR(TEXT("Houdini GEO Importer: could not find file %s!"), *InBGEOFile);
		return false;
	}

	// Make sure we're using absolute path!
	const FString AbsoluteFilePath = FPaths::ConvertRelativePathToFull(InBGEOFile);

	if (AbsoluteFilePath.IsEmpty())
		return false;
		
	FString AbsoluteFileDirectory;
	FString FileName;
	FString FileExtension;

	// Split the file path
	FPaths::Split(AbsoluteFilePath, AbsoluteFileDirectory, FileName, FileExtension);

	// Handle .bgeo.sc correctly
	if (FileExtension.Equals(TEXT("sc")))
	{
		// append the bgeo to .sc
		FileExtension = FPaths::GetExtension(FileName) + TEXT(".") + FileExtension;
		// update the filename
		FileName = FPaths::GetBaseFilename(FileName);
	}

	if (FileExtension.IsEmpty())
		FileExtension = TEXT("bgeo");

	if (!FileExtension.StartsWith(TEXT("bgeo"), ESearchCase::IgnoreCase))
	{
		// Not a bgeo file!
		HOUDINI_LOG_ERROR(TEXT("Houdini GEO Importer: File %s is not a .bgeo or .bgeo.sc file!"), *InBGEOFile);
		return false;
	}

	OutNodeId = -1;

	// Check HoudiniEngine / HAPI init?
	if (!FHoudiniEngine::IsInitialized())
	{
		HOUDINI_LOG_ERROR(TEXT("Couldn't initialize HoudiniEngine!"));
		return false;
	}

	FString Notification = TEXT("BGEO Importer: Loading bgeo file...");
	FHoudiniEngine::Get().CreateTaskSlateNotification(FText::FromString(Notification), true);

	// Create a file SOP
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
		-1,	"SOP/file", "bgeo", true, &OutNodeId), false);

	/*
	// Set the file path parameter
	HAPI_ParmId ParmId = -1;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmIdFromName(
		FHoudiniEngine::Get().GetSession(),
		OutNodeId, "file", &ParmId), false);

	const std::string ConvertedString = TCHAR_TO_UTF8(*AbsoluteFilePath);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(
		FHoudiniEngine::Get().GetSession(), OutNodeId, ConvertedString.c_str(), ParmId, 0), false);
	*/

	// Simply use LoadGeoFrom file
	std::string ConvertedString = TCHAR_TO_UTF8(*AbsoluteFilePath);
	FHoudiniApi::LoadGeoFromFile(FHoudiniEngine::Get().GetSession(), OutNodeId, ConvertedString.c_str());

	return true;
}

bool
UHoudiniGeoImporter::MergeGeoFromNode(const FString& InNodePath, HAPI_NodeId& OutNodeId)
{
	if (InNodePath.IsEmpty())
		return false;

	// 1. Houdini Engine Session
	// See if we should/can start the default "first" HE session
	if (!AutoStartHoudiniEngineSessionIfNeeded())
		return false;

	// TODO
	// Check that InNodePath is valid
	OutNodeId = -1;

	// Check HoudiniEngine / HAPI init?
	if (!FHoudiniEngine::IsInitialized())
	{
		HOUDINI_LOG_ERROR(TEXT("Couldn't initialize HoudiniEngine!"));
		return false;
	}

	FString Notification = TEXT("Merging node data...");
	FHoudiniEngine::Get().CreateTaskSlateNotification(FText::FromString(Notification), true);

	// Create an object merge SOP
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(
		-1, "SOP/object_merge", "NodeSyncFetch", true, &OutNodeId), false);

	// Set the object path parameter
	HAPI_ParmId ParmId = -1;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmIdFromName(
		FHoudiniEngine::Get().GetSession(),
		OutNodeId, "objpath1", &ParmId), false);

	const std::string ConvertedString = TCHAR_TO_UTF8(*InNodePath);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(
		FHoudiniEngine::Get().GetSession(), OutNodeId, ConvertedString.c_str(), ParmId, 0), false);

	// Cook the node
	HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
	if(!FHoudiniEngineUtils::HapiCookNode(OutNodeId, &CookOptions, true))
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to cook NodeSyncFetch node!"));
		return false;
	}

	return true;
}


bool
UHoudiniGeoImporter::CloseBGEOFile(const HAPI_NodeId& InNodeId)
{
	// 8. Delete the created  node in Houdini
	if (!DeleteCreatedNode(InNodeId))
		return false;
	
	return true;
}

bool
UHoudiniGeoImporter::LoadBGEOFileInHAPI(HAPI_NodeId& NodeId)
{
	NodeId = -1;

	if (AbsoluteFilePath.IsEmpty())
		return false;

	// Check HoudiniEngine / HAPI init?
	if (!FHoudiniEngine::IsInitialized())
	{
		HOUDINI_LOG_ERROR(TEXT("Couldn't initialize HoudiniEngine!"));
		return false;
	}

	FString Notification = TEXT("BGEO Importer: Loading bgeo file...");
	FHoudiniEngine::Get().CreateTaskSlateNotification(FText::FromString(Notification), true);

	// Create a file SOP
	// We still create a file SOP as we need a Node that LoadGeoFromFile can use to store the data on
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
		-1,	"SOP/file", "bgeo", true, &NodeId), false);

	/*
	// Set the file path parameter
	HAPI_ParmId ParmId = -1;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmIdFromName(
		FHoudiniEngine::Get().GetSession(),
		NodeId, "file", &ParmId), false);

	std::string ConvertedString = TCHAR_TO_UTF8(*AbsoluteFilePath);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(
		FHoudiniEngine::Get().GetSession(), NodeId, ConvertedString.c_str(), ParmId, 0), false);
	*/

	// Simply use LoadGeoFrom file
	std::string ConvertedString = TCHAR_TO_UTF8(*AbsoluteFilePath);
	FHoudiniApi::LoadGeoFromFile(FHoudiniEngine::Get().GetSession(), NodeId, ConvertedString.c_str());

	return CookFileNode(NodeId);
}

bool
UHoudiniGeoImporter::CookFileNode(const HAPI_NodeId& InNodeId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UHoudiniGeoImporter::CookFileNode);

	// Cook the node    
	HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
		FHoudiniEngine::Get().GetSession(), InNodeId, &CookOptions), false);

	// Wait for the cook to finish
	int32 status = HAPI_STATE_MAX_READY_STATE + 1;
	while (status > HAPI_STATE_MAX_READY_STATE)
	{
		// Retrieve the status
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetStatus(
			FHoudiniEngine::Get().GetSession(),
			HAPI_STATUS_COOK_STATE, &status), false);

		HOUDINI_LOG_MESSAGE(TEXT("Still Cooking, current status: %s."), *FHoudiniEngineUtils::GetStatusString(HAPI_STATUS_COOK_STATE, HAPI_STATUSVERBOSITY_ERRORS));

		// Go to bed..
		if (status > HAPI_STATE_MAX_READY_STATE)
			FPlatformProcess::Sleep(0.5f);
	}

	if (status != HAPI_STATE_READY)
	{
		// There was some cook errors
		HOUDINI_LOG_ERROR(TEXT("Finished Cooking with errors!"));
		return false;
	}

	HOUDINI_LOG_MESSAGE(TEXT("Finished Cooking!"));

	return true;
}

bool
UHoudiniGeoImporter::BuildAllOutputsForNode(
	const HAPI_NodeId& InNodeId,
	UObject* InOuter,
	TArray<UHoudiniOutput*>& InOldOutputs,
	TArray<UHoudiniOutput*>& OutNewOutputs,
	bool bInAddOutputsToRootSet,
	bool bInUseOutputNodes,
	bool bGatherEditableCurves)
{
	bool bOutputTemplateGeos = false;

	// Gather output nodes for the HAC
	TArray<int32> OutputNodes;
	FHoudiniEngineUtils::GatherAllAssetOutputs(InNodeId, bInUseOutputNodes, bOutputTemplateGeos, bGatherEditableCurves, OutputNodes);

	// TArray<UHoudiniOutput*> OldOutputs;	
	TMap<HAPI_NodeId, int32> OutputNodeCookCount;
	if (!FHoudiniOutputTranslator::BuildAllOutputs(InNodeId, InOuter, OutputNodes, OutputNodeCookCount, InOldOutputs, OutNewOutputs, bOutputTemplateGeos, bInUseOutputNodes, bGatherEditableCurves))
	{
		// Couldn't create the package
		HOUDINI_LOG_ERROR(TEXT("Houdini GEO Importer: Failed to process the File SOP's outputs!"));
		return false;
	}

	if (bInAddOutputsToRootSet)
	{
		// Add the output objects to the RootSet to prevent them from being GCed
		for (auto& Out : OutNewOutputs)
			Out->AddToRoot();
	}

	return true;
}

FHoudiniPackageParams
UHoudiniGeoImporter::GetPackageParamsForType(
	const TArray<UHoudiniOutput*>& InOutputs,
	FHoudiniPackageParams InPackageParams,
	const EHoudiniOutputType InOutputType,
	const EHoudiniPartType InPartType)
{
	// Look for the first unreal_output_name attribute for the desired output
	// type and use that for ObjectName
	for (const UHoudiniOutput* const CurOutput : InOutputs)
	{
		if (CurOutput->GetType() != InOutputType)
		{
			continue;
		}

		bool bFoundOutputName = false;
		bool bFoundBakeFolder = InPackageParams.PackageMode != EPackageMode::Bake;
		bool bFoundTempFolder = false;
		for (const FHoudiniGeoPartObject& HGPO : CurOutput->GetHoudiniGeoPartObjects())
		{
			if (HGPO.Type != InPartType)
			{
				continue;
			}

			if (!bFoundOutputName)
			{
				FString OutputName;
				if (FHoudiniEngineUtils::GetOutputNameAttribute(HGPO.GeoId, HGPO.PartId, OutputName))
				{
					if (!OutputName.IsEmpty())
					{
						InPackageParams.ObjectName = OutputName;
						bFoundOutputName = true;
					}
				}
			}

			if (!bFoundTempFolder)
			{
				FString TempFolder;
				if (FHoudiniEngineUtils::GetTempFolderAttribute(HGPO.GeoId, TempFolder, HGPO.PartId))
				{
					if (!TempFolder.IsEmpty())
					{
						InPackageParams.TempCookFolder = TempFolder;
						bFoundTempFolder = true;
					}
				}
			}

			if (!bFoundBakeFolder)
			{
				FString BakeFolder;
				if (FHoudiniEngineUtils::GetBakeFolderAttribute(HGPO.GeoId, HGPO.PartId, BakeFolder))
				{
					if (!BakeFolder.IsEmpty())
					{
						InPackageParams.BakeFolder = BakeFolder;
						bFoundBakeFolder = true;
					}
				}
			}

			if (bFoundOutputName && bFoundBakeFolder && bFoundTempFolder)
			{
				return InPackageParams;
			}
		}
	}

	return InPackageParams;
}

FHoudiniPackageParams
UHoudiniGeoImporter::GetPackageParamsForHGPO(
	const FHoudiniGeoPartObject& InHGPO,
	FHoudiniPackageParams InPackageParams)
{
	// Check for a unreal_output_name if we are in bake mode
	if (InPackageParams.PackageMode == EPackageMode::Bake)
	{
		FString OutputName;
		if (FHoudiniEngineUtils::GetOutputNameAttribute(InHGPO.GeoId, InHGPO.PartId, OutputName))
		{
			if (!OutputName.IsEmpty())
			{
				InPackageParams.ObjectName = OutputName;
			}
		}
		// Could have prim attribute unreal_bake_folder override
		FString BakeFolder;
		if (FHoudiniEngineUtils::GetBakeFolderAttribute(InHGPO.GeoId, InHGPO.PartId, BakeFolder))
		{
			if (!BakeFolder.IsEmpty())
			{
				InPackageParams.BakeFolder = BakeFolder;
			}
		}
	}
	return InPackageParams;
}
