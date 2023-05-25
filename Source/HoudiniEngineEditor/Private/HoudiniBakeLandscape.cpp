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

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniAsset.h"
#include "HoudiniOutput.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniPackageParams.h"
#include "HoudiniEnginePrivatePCH.h"
#include "UnrealLandscapeTranslator.h"
#include "HoudiniStringResolver.h"
#include "HoudiniEngineCommands.h"
#include "HoudiniLandscapeUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "RawMesh.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "GameFramework/Actor.h"
#include "Editor.h"
#include "HoudiniLandscapeTranslator.h"
#include "HoudiniBakeLandscape.h"
#include "HoudiniEngineOutputStats.h"
#include "FileHelpers.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniMeshTranslator.h"
#include "LandscapeEdit.h"
#include "PackageTools.h"
#include "Containers/UnrealString.h"
#include "Components/AudioComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Sound/SoundBase.h"
#include "HoudiniLandscapeRuntimeUtils.h"
#include "WorldPartition/WorldPartition.h"

bool
FHoudiniLandscapeBake::BakeLandscapeLayer(
	FHoudiniPackageParams& PackageParams, 
	UHoudiniLandscapeTargetLayerOutput& LayerOutput, 
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& BakeStats,
	TSet<FString>& ClearedLayers)
{
	ALandscape* OutputLandscape = LayerOutput.Landscape;
	ULandscapeInfo* TargetLandscapeInfo = OutputLandscape->GetLandscapeInfo();
	FHoudiniExtents Extents = LayerOutput.Extents;

	if (!OutputLandscape->bCanHaveLayersContent)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Landscape {0} has no edit layers, so baking does nothing."), *OutputLandscape->GetActorLabel());
		return true;
	}

	//---------------------------------------------------------------------------------------------------------------------------
	// For landscape layers baking is the act of copying cooked data to a baked layer. We do not need to do that if we already
	// wrote directly to the final layer.
	//---------------------------------------------------------------------------------------------------------------------------

	if (!LayerOutput.bCookedLayerRequiresBaking)
		return true;

	FLandscapeLayer* BakedLayer = FHoudiniLandscapeUtils::GetEditLayerForWriting(OutputLandscape, FName(LayerOutput.BakedEditLayer));
	ULandscapeLayerInfoObject* TargetLayerInfo = OutputLandscape->GetLandscapeInfo()->GetLayerInfoByName(FName(LayerOutput.TargetLayer));

	//---------------------------------------------------------------------------------------------------------------------------
	// Clear the layer, but only once per bake.
	//---------------------------------------------------------------------------------------------------------------------------

	bool bIsHeightFieldLayer = LayerOutput.TargetLayer == "height";

	if (OutputLandscape->bHasLayersContent && LayerOutput.bClearLayer && !ClearedLayers.Contains(LayerOutput.BakedEditLayer))
	{
		ClearedLayers.Add(LayerOutput.BakedEditLayer);
		if (bIsHeightFieldLayer)
		{
			OutputLandscape->ClearLayer(BakedLayer->Guid, nullptr, ELandscapeClearMode::Clear_Heightmap);
		}
		else
		{
			HOUDINI_CHECK_RETURN(TargetLayerInfo, false);
			OutputLandscape->ClearPaintLayer(BakedLayer->Guid, TargetLayerInfo);
		}

	}

	//---------------------------------------------------------------------------------------------------------------------------
	// Copy cooked layer to baked layer
	//---------------------------------------------------------------------------------------------------------------------------

	if (!bIsHeightFieldLayer)
	{
		HOUDINI_CHECK_RETURN(TargetLayerInfo, false);

		FScopedSetLandscapeEditingLayer Scope(OutputLandscape, BakedLayer->Guid, [&] { OutputLandscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_All); });

		TArray<uint8_t> Values = FHoudiniLandscapeUtils::GetLayerData(OutputLandscape, Extents, FName(LayerOutput.CookedEditLayer), FName(LayerOutput.TargetLayer));

		if (LayerOutput.TargetLayer == HAPI_UNREAL_VISIBILITY_LAYER_NAME)
		{
			FAlphamapAccessor<false, false> AlphaAccessor(OutputLandscape->GetLandscapeInfo(), ALandscapeProxy::VisibilityLayer);

			AlphaAccessor.SetData(
				Extents.Min.X, Extents.Min.Y, Extents.Max.X, Extents.Max.Y,
				Values.GetData(),
				ELandscapeLayerPaintingRestriction::None);
		}
		else
		{
			FAlphamapAccessor<false, true> AlphaAccessor(OutputLandscape->GetLandscapeInfo(), TargetLayerInfo);

			AlphaAccessor.SetData(
				Extents.Min.X, Extents.Min.Y, Extents.Max.X, Extents.Max.Y,
				Values.GetData(),
				ELandscapeLayerPaintingRestriction::None);
		}
	}
	else
	{
		FLandscapeLayer* EditLayer = FHoudiniLandscapeUtils::GetEditLayerForReading(OutputLandscape, FName(LayerOutput.CookedEditLayer));
		HOUDINI_CHECK_RETURN(EditLayer != nullptr, false);
		TArray<uint16_t> Values = FHoudiniLandscapeUtils::GetHeightData(OutputLandscape, Extents, EditLayer);

		FScopedSetLandscapeEditingLayer Scope(OutputLandscape, BakedLayer->Guid, [&] { OutputLandscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_All); });

		FLandscapeEditDataInterface LandscapeEdit(TargetLandscapeInfo);
		FHeightmapAccessor<false> HeightmapAccessor(TargetLandscapeInfo);
		HeightmapAccessor.SetData(Extents.Min.X, Extents.Min.Y, Extents.Max.X, Extents.Max.Y, Values.GetData());

	}

	// Delete cooked layer.
	FHoudiniLandscapeRuntimeUtils::DeleteEditLayer(OutputLandscape, FName(LayerOutput.CookedEditLayer));

	// Bake all the LayerInfoObjects, and patch up the result inside the landscape.
	for (ULandscapeLayerInfoObject* CookedLayerInfoObject : LayerOutput.LayerInfoObjects)
	{
		CreateBakedLandscapeLayerInfoObject(PackageParams, LayerOutput.Landscape, CookedLayerInfoObject, OutPackagesToSave, BakeStats);
	}

	//---------------------------------------------------------------------------------------------------------------------------
	// Make sure baked layer is visible.
	//---------------------------------------------------------------------------------------------------------------------------
	int EditLayerIndex = OutputLandscape->GetLayerIndex(FName(LayerOutput.BakedEditLayer));
	if (EditLayerIndex != INDEX_NONE)
		OutputLandscape->SetLayerVisibility(EditLayerIndex, true);


	if (!LayerOutput.BakeOutlinerFolder.IsEmpty())
		OutputLandscape->SetFolderPath(FName(LayerOutput.BakeOutlinerFolder));

	return true;
}

bool
FHoudiniLandscapeBake::BakeLandscape(
	const UHoudiniAssetComponent* HoudiniAssetComponent,
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	TArray<FHoudiniBakedOutput>& InBakedOutputs,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	const FString& BakePath,
	FHoudiniEngineOutputStats& BakeStats,
	TSet<FString> & ClearedLandscapeLayers
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

	const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
		EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;

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

		if (OutputObject.OutputObject->IsA<UHoudiniLandscapePtr>())
		{
			HOUDINI_LOG_ERROR(TEXT("Old Landscape Data found, rebuild the HDA Actor"));
			continue;
		}

		UHoudiniLandscapeTargetLayerOutput* LayerOutput = Cast<UHoudiniLandscapeTargetLayerOutput>(OutputObject.OutputObject);
		HOUDINI_CHECK_RETURN(IsValid(LayerOutput), false);

		if (!IsValid(LayerOutput->Landscape))
		{
			HOUDINI_LOG_WARNING(TEXT("Cooked Landscape was not found, so nothing will be baked"));
			continue;
		}

		FHoudiniAttributeResolver Resolver;
		UWorld* const DesiredWorld = LayerOutput->Landscape->GetWorld();
		FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
			DesiredWorld, HoudiniAssetComponent, ObjectIdentifier, OutputObject, LayerOutput->BakedEditLayer,
			PackageParams, Resolver, BakePath, AssetPackageReplaceMode);

		FHoudiniLandscapeBake::BakeLandscapeLayer(PackageParams, *LayerOutput, PackagesToSave, BakeStats, ClearedLandscapeLayers);
	}

	// Update the cached baked output data
	BakedOutput.BakedOutputObjects = NewBakedOutputObjects;

	if (PackagesToSave.Num() > 0)
	{
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, true, false);
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


TArray<FHoudiniEngineBakedActor>
FHoudiniLandscapeBake::MoveCookedToBakedLandscapes(
	const FHoudiniPackageParams& InBakedObjectPackageParams,
	const FName & InFallbackWorldOutlinerFolder,
	const TArray<UHoudiniOutput*>& InOutputs, 
	bool bReplaceExistingActors)
{
	TSet<ALandscape*> ProcessedLandscapes;
	TArray<FHoudiniEngineBakedActor> Results;

	for(int OutputIndex = 0; OutputIndex < InOutputs.Num(); OutputIndex++)
	{
		UHoudiniOutput* HoudiniOutput = InOutputs[OutputIndex];

		// Get a valid output layer from the output.
		if (!IsValid(HoudiniOutput) || HoudiniOutput->GetType() != EHoudiniOutputType::Landscape)
			continue;

		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = HoudiniOutput->GetOutputObjects();

		for (auto& Elem : OutputObjects)
		{
			UHoudiniLandscapeTargetLayerOutput * LayerOutput =  Cast<UHoudiniLandscapeTargetLayerOutput>(Elem.Value.OutputObject);
			if (!LayerOutput)
				continue;

			// If the landscape was not created for this layer, we don't need to do anything, layers have already been renamed.
			if (!LayerOutput->bCreatedLandscape)
				continue;

			// Skip renaming of already deleted/moved actors
			if (!IsValid(LayerOutput->Landscape))
				continue;

			// Only bake each landscape once.
			if (ProcessedLandscapes.Contains(LayerOutput->Landscape))
				continue;

			// Rename the actor, make sure we only do this once.
			AActor* FoundActor = nullptr;
			ALandscape* ExistingLandscape = FHoudiniEngineUtils::FindOrRenameInvalidActor<ALandscape>(
					LayerOutput->Landscape->GetWorld(), LayerOutput->BakedLandscapeName, FoundActor);

			if (ExistingLandscape && bReplaceExistingActors)
			{
				// Even though we found an existing landscape with the desired type, we're just going to destroy/replace
				// it for now.
				FHoudiniEngineUtils::RenameToUniqueActor(ExistingLandscape, LayerOutput->BakedLandscapeName + "_0");
				FHoudiniLandscapeRuntimeUtils::DestroyLandscape(ExistingLandscape);
			}

			// Now rename the actor. Clear the old "cooked" output as it should be treated as destroyed
			FHoudiniEngineUtils::SafeRenameActor(LayerOutput->Landscape, LayerOutput->BakedLandscapeName);

			if (!ProcessedLandscapes.Contains(LayerOutput->Landscape))
			{
				ProcessedLandscapes.Add(LayerOutput->Landscape);

				FName WorldOutlinerFolder =
					!LayerOutput->BakeOutlinerFolder.IsEmpty() ? FName(LayerOutput->BakeOutlinerFolder) : InFallbackWorldOutlinerFolder;

				FHoudiniEngineBakedActor BakeActor(
						LayerOutput->Landscape,
						FName(LayerOutput->Landscape->GetActorLabel()),
						WorldOutlinerFolder,
						OutputIndex,
						Elem.Key,
						nullptr, 
						nullptr, 
						nullptr,
						InBakedObjectPackageParams.BakeFolder,
						InBakedObjectPackageParams);

				Results.Add(BakeActor);

				FHoudiniEngineBakeUtils::SetOutlinerFolderPath(LayerOutput->Landscape, Elem.Value, WorldOutlinerFolder);

			}

			LayerOutput->Landscape = nullptr;
		}
	}

	return Results;
}


ULandscapeLayerInfoObject* 
FHoudiniLandscapeBake::CreateBakedLandscapeLayerInfoObject(const FHoudiniPackageParams& PackageParams, ALandscape* Landscape, ULandscapeLayerInfoObject* LandscapeLayerInfoObject, TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& BakeStats)
{
	FHoudiniPackageParams LayerPackageParams = PackageParams;

	//------------------------------------------------------------------------------------------------------------------------------
	// Create the new package and make sure its fully loaded
	//------------------------------------------------------------------------------------------------------------------------------

	FString CreatedPackageName;
	UPackage* Package = PackageParams.CreatePackageForObject(CreatedPackageName);
	if (!Package->IsFullyLoaded())
	{
		FlushAsyncLoading();
		if (!Package->GetOuter())
		{
			Package->FullyLoad();
		}
		else
		{
			Package->GetOutermost()->FullyLoad();
		}
	}

	OutPackagesToSave.Add(Package);

	//------------------------------------------------------------------------------------------------------------------------------
	// Replace the layer.
	//------------------------------------------------------------------------------------------------------------------------------

	FString ObjectName = LandscapeLayerInfoObject->GetName();
	ULandscapeLayerInfoObject* DuplicatedObject = DuplicateObject<ULandscapeLayerInfoObject>(LandscapeLayerInfoObject, Package, *ObjectName);
	Landscape->GetLandscapeInfo()->ReplaceLayer(LandscapeLayerInfoObject, DuplicatedObject);

	Package->MarkPackageDirty();
	BakeStats.NotifyObjectsCreated(DuplicatedObject->GetClass()->GetName(), 1);

	return DuplicatedObject;

}
