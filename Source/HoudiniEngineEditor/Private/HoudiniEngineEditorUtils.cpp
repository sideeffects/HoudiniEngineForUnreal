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

#include "HoudiniEngineEditorUtils.h"

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniAsset.h"
#include "HoudiniOutput.h"
#include "HoudiniTool.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Engine/Selection.h"
#include "AssetRegistryModule.h"
#include "EditorViewportClient.h"
#include "ActorFactories/ActorFactory.h"
#include "FileHelpers.h"
#include "PropertyPathHelpers.h"
#include "Components/SceneComponent.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

int32
FHoudiniEngineEditorUtils::GetContentBrowserSelection(TArray< UObject* >& ContentBrowserSelection)
{
	ContentBrowserSelection.Empty();

	// Get the current Content browser selection
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked< FContentBrowserModule >("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	for (int32 n = 0; n < SelectedAssets.Num(); n++)
	{
		// Get the current object
		UObject * Object = SelectedAssets[n].GetAsset();
		if (!IsValid(Object))
			continue;

		// Only static meshes are supported
		if (Object->GetClass() != UStaticMesh::StaticClass())
			continue;

		ContentBrowserSelection.Add(Object);
	}

	return ContentBrowserSelection.Num();
}

int32
FHoudiniEngineEditorUtils::GetWorldSelection(TArray<UObject*>& WorldSelection, bool bHoudiniAssetActorsOnly)
{
	WorldSelection.Empty();

	// Get the current editor selection
	if (GEditor)
	{
		USelection* SelectedActors = GEditor->GetSelectedActors();
		if (IsValid(SelectedActors))
		{
			for (FSelectionIterator It(*SelectedActors); It; ++It)
			{
				AActor * Actor = Cast<AActor>(*It);
				if (!IsValid(Actor))
					continue;

				// Ignore the SkySphere?
				FString ClassName = Actor->GetClass() ? Actor->GetClass()->GetName() : FString();
				if (ClassName == TEXT("BP_Sky_Sphere_C"))
					continue;

				// We're normally only selecting actors with StaticMeshComponents and SplineComponents
				// Heightfields? Filter here or later? also allow HoudiniAssets?
				WorldSelection.Add(Actor);
			}
		}
	}

	// If we only want Houdini Actors...
	if (bHoudiniAssetActorsOnly)
	{
		// ... remove all but them
		for (int32 Idx = WorldSelection.Num() - 1; Idx >= 0; Idx--)
		{
			AHoudiniAssetActor * HoudiniAssetActor = Cast<AHoudiniAssetActor>(WorldSelection[Idx]);
			if (!IsValid(HoudiniAssetActor))
				WorldSelection.RemoveAt(Idx);
		}
	}

	return WorldSelection.Num();
}


FString
FHoudiniEngineEditorUtils::HoudiniCurveTypeToString(const EHoudiniCurveType& HoudiniCurveType)
{
	FString HoudiniCurveTypeStr;
	switch (HoudiniCurveType)
	{
	case EHoudiniCurveType::Invalid:
	{
		HoudiniCurveTypeStr = TEXT("Invalid");
	}
	break;

	case EHoudiniCurveType::Polygon:
	{
		HoudiniCurveTypeStr = TEXT("Polygon");
	}
	break;

	case EHoudiniCurveType::Nurbs:
	{
		HoudiniCurveTypeStr = TEXT("Nurbs");
	}
	break;

	case EHoudiniCurveType::Bezier:
	{
		HoudiniCurveTypeStr = TEXT("Bezier");
	}
	break;

	case EHoudiniCurveType::Points:
	{
		HoudiniCurveTypeStr = TEXT("Points");
	}
	break;
	}

	return HoudiniCurveTypeStr;
}

FString
FHoudiniEngineEditorUtils::HoudiniCurveMethodToString(const EHoudiniCurveMethod& CurveMethod)
{
	FString HoudiniCurveMethodStr;
	switch (CurveMethod)
	{
	case EHoudiniCurveMethod::Invalid:
	{
		HoudiniCurveMethodStr = TEXT("Invalid");
	}
	break;
	case EHoudiniCurveMethod::CVs:
	{
		HoudiniCurveMethodStr = TEXT("CVs");
	}
	break;
	case EHoudiniCurveMethod::Breakpoints:
	{
		HoudiniCurveMethodStr = TEXT("Breakpoints");
	}
	break;
	case EHoudiniCurveMethod::Freehand:
	{
		HoudiniCurveMethodStr = TEXT("Freehand");
	}
	break;
	}

	return HoudiniCurveMethodStr;
}

FString FHoudiniEngineEditorUtils::HoudiniCurveBreakpointParameterizationToString(
	const EHoudiniCurveBreakpointParameterization& HoudiniCurveBreakpointParameterization)
{
	FString HoudiniCurveBreakpointParameterizationStr;
	switch (HoudiniCurveBreakpointParameterization)
	{
	case EHoudiniCurveBreakpointParameterization::Invalid:
	{
		HoudiniCurveBreakpointParameterizationStr = TEXT("Invalid");
	}
	break;
	case EHoudiniCurveBreakpointParameterization::Uniform:
	{
		HoudiniCurveBreakpointParameterizationStr = TEXT("Uniform");
	}
	break;
	case EHoudiniCurveBreakpointParameterization::Chord:
	{
		HoudiniCurveBreakpointParameterizationStr = TEXT("Chord");
	}
	break;
	case EHoudiniCurveBreakpointParameterization::Centripetal:
	{
		HoudiniCurveBreakpointParameterizationStr = TEXT("Centripetal");
	}
	break;
	}

	return HoudiniCurveBreakpointParameterizationStr;
}


FString
FHoudiniEngineEditorUtils::HoudiniCurveMethodToUnrealCurveTypeString(const EHoudiniCurveType& HoudiniCurveType)
{
	// Temporary,  we need to figure out a way to access the output curve's info later
	FString UnrealCurveType;
	switch (HoudiniCurveType)
	{
	case EHoudiniCurveType::Polygon:
	case EHoudiniCurveType::Points:
	{
		UnrealCurveType = TEXT("Linear");
	}
	break;

	case EHoudiniCurveType::Nurbs:
	case EHoudiniCurveType::Bezier:
	{
		UnrealCurveType = TEXT("Curve");
	}
	break;
	}

	return UnrealCurveType;
}

FString
FHoudiniEngineEditorUtils::HoudiniLandscapeOutputBakeTypeToString(const EHoudiniLandscapeOutputBakeType& LandscapeBakeType)
{
	FString LandscapeBakeTypeString;
	switch (LandscapeBakeType)
	{
	case EHoudiniLandscapeOutputBakeType::Detachment:
		LandscapeBakeTypeString = "To Current Level";
		break;

	case EHoudiniLandscapeOutputBakeType::BakeToImage:
		LandscapeBakeTypeString = "To Image";
		break;

	case EHoudiniLandscapeOutputBakeType::BakeToWorld:
		LandscapeBakeTypeString = "To New World";
		break;

	}

	return LandscapeBakeTypeString;
}


FTransform
FHoudiniEngineEditorUtils::GetDefaulAssetSpawnTransform()
{
	FTransform SpawnTransform = FTransform::Identity;

	// Get the editor viewport LookAt position to spawn the new objects
	if (GEditor && GEditor->GetActiveViewport())
	{
		FEditorViewportClient* ViewportClient = (FEditorViewportClient*)GEditor->GetActiveViewport()->GetClient();
		if (ViewportClient)
		{
			// We need to toggle the orbit camera on to get the proper LookAtLocation to spawn our asset
			ViewportClient->ToggleOrbitCamera(true);
			SpawnTransform.SetLocation(ViewportClient->GetLookAtLocation());
			ViewportClient->ToggleOrbitCamera(false);
		}
	}

	return SpawnTransform;
}

FTransform
FHoudiniEngineEditorUtils::GetMeanWorldSelectionTransform()
{
	FTransform SpawnTransform = GetDefaulAssetSpawnTransform();

	if (GEditor && (GEditor->GetSelectedActorCount() > 0))
	{
		// Get the current Level Editor Selection
		USelection* SelectedActors = GEditor->GetSelectedActors();

		int NumAppliedTransform = 0;
		for (FSelectionIterator It(*SelectedActors); It; ++It)
		{
			AActor * Actor = Cast< AActor >(*It);
			if (!Actor)
				continue;

			// Just Ignore the SkySphere...
			FString ClassName = Actor->GetClass() ? Actor->GetClass()->GetName() : FString();
			if (ClassName == TEXT("BP_Sky_Sphere_C"))
				continue;

			FTransform CurrentTransform = Actor->GetTransform();

			ALandscapeProxy* Landscape = Cast< ALandscapeProxy >(Actor);
			if (Landscape)
			{
				// We need to offset Landscape's transform in X/Y to center them properly
				FVector Origin, Extent;
				Actor->GetActorBounds(false, Origin, Extent);

				// Use the origin's XY Position
				FVector Location = CurrentTransform.GetLocation();
				Location.X = Origin.X;
				Location.Y = Origin.Y;
				CurrentTransform.SetLocation(Location);
			}

			// Accumulate all the actor transforms...
			if (NumAppliedTransform == 0)
				SpawnTransform = CurrentTransform;
			else
				SpawnTransform.Accumulate(CurrentTransform);

			NumAppliedTransform++;
		}

		if (NumAppliedTransform > 0)
		{
			// "Mean" all the accumulated Transform
			SpawnTransform.SetScale3D(FVector::OneVector);
			SpawnTransform.NormalizeRotation();

			if (NumAppliedTransform > 1)
				SpawnTransform.SetLocation(SpawnTransform.GetLocation() / (float)NumAppliedTransform);
		}
	}

	return SpawnTransform;
}

void
FHoudiniEngineEditorUtils::InstantiateHoudiniAsset(UHoudiniAsset* InHoudiniAsset, const EHoudiniToolType& InType, const EHoudiniToolSelectionType& InSelectionType)
{
	if (!InHoudiniAsset)
		return;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	// Load the asset
	UObject* AssetObj = Cast<UObject>(InHoudiniAsset);//InHoudiniAsset->LoadSynchronous();
	if (!AssetObj)
		return;

	// Get the asset Factory
	UActorFactory* Factory = GEditor->FindActorFactoryForActorClass(AHoudiniAssetActor::StaticClass());
	if (!Factory)
		return;

	// Get the current Level Editor Selection
	TArray<UObject * > WorldSelection;
	int32 WorldSelectionCount = FHoudiniEngineEditorUtils::GetWorldSelection(WorldSelection);

	// Get the current Content browser selection
	TArray<UObject *> ContentBrowserSelection;
	int32 ContentBrowserSelectionCount = FHoudiniEngineEditorUtils::GetContentBrowserSelection(ContentBrowserSelection);

	// By default, Content browser selection has a priority over the world selection
	bool UseCBSelection = ContentBrowserSelectionCount > 0;
	if (InSelectionType == EHoudiniToolSelectionType::HTOOL_SELECTION_CB_ONLY)
		UseCBSelection = true;
	else if (InSelectionType == EHoudiniToolSelectionType::HTOOL_SELECTION_WORLD_ONLY)
		UseCBSelection = false;

	// Modify the created actor's position from the current editor world selection
	FTransform SpawnTransform = GetDefaulAssetSpawnTransform();
	if (WorldSelectionCount > 0)
	{
		// Get the "mean" transform of all the selected actors
		SpawnTransform = GetMeanWorldSelectionTransform();
	}

	// If the current tool is a batch one, we'll need to create multiple instances of the HDA
	if (InType == EHoudiniToolType::HTOOLTYPE_OPERATOR_BATCH)
	{
		// Unselect the current selection to select the created actor after
		if (GEditor)
			GEditor->SelectNone(true, true, false);

		// An instance of the asset will be created for each selected object
		for (int32 SelecIndex = 0; SelecIndex < (UseCBSelection ? ContentBrowserSelectionCount : WorldSelectionCount); SelecIndex++)
		{
			// Get the current object
			UObject* CurrentSelectedObject = nullptr;
			if (UseCBSelection && ContentBrowserSelection.IsValidIndex(SelecIndex))
				CurrentSelectedObject = ContentBrowserSelection[SelecIndex];

			if (!UseCBSelection && WorldSelection.IsValidIndex(SelecIndex))
				CurrentSelectedObject = WorldSelection[SelecIndex];

			if (!CurrentSelectedObject)
				continue;

			// If it's an actor, use its Transform to spawn the HDA
			AActor* CurrentSelectedActor = Cast<AActor>(CurrentSelectedObject);
			if (CurrentSelectedActor)
				SpawnTransform = CurrentSelectedActor->GetTransform();
			else
				SpawnTransform = GetDefaulAssetSpawnTransform();

			// Create the actor for the HDA
			AActor* CreatedActor = Factory->CreateActor(AssetObj, GEditor->GetEditorWorldContext().World()->GetCurrentLevel(), SpawnTransform);
			if (!CreatedActor)
				continue;

			// Get the HoudiniAssetActor / HoudiniAssetComponent we just created
			AHoudiniAssetActor* HoudiniAssetActor = (AHoudiniAssetActor*)CreatedActor;
			if (!HoudiniAssetActor)
				continue;

			UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
			if (!HoudiniAssetComponent)
				continue;

			// Create and set the input preset for this HDA and selected Object
			TMap<UObject*, int32> InputPreset;
			InputPreset.Add(CurrentSelectedObject, 0);
			HoudiniAssetComponent->SetInputPresets(InputPreset);

			// Select the Actor we just created
			if (GEditor && GEditor->CanSelectActor(CreatedActor, true, false))
				GEditor->SelectActor(CreatedActor, true, true, true);
		}
	}
	else
	{
		// We only need to create a single instance of the asset, regarding of the selection
		AActor* CreatedActor = Factory->CreateActor(AssetObj, GEditor->GetEditorWorldContext().World()->GetCurrentLevel(), SpawnTransform);
		if (!CreatedActor)
			return;

		// Generator tools don't need to preset their input
		if (InType != EHoudiniToolType::HTOOLTYPE_GENERATOR)
		{
			TMap<UObject*, int32> InputPresets;
			AHoudiniAssetActor* HoudiniAssetActor = (AHoudiniAssetActor*)CreatedActor;
			UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetActor ? HoudiniAssetActor->GetHoudiniAssetComponent() : nullptr;
			if (HoudiniAssetComponent)
			{
				// Build the preset map
				int InputIndex = 0;
				for (auto CurrentObject : (UseCBSelection ? ContentBrowserSelection : WorldSelection))
				{
					if (!CurrentObject)
						continue;

					if (InType == EHoudiniToolType::HTOOLTYPE_OPERATOR_MULTI)
					{
						// The selection will be applied individually to multiple inputs
						// (first object to first input, second object to second input etc...)
						InputPresets.Add(CurrentObject, InputIndex++);
					}
					else
					{
						// All the selection will be applied to the asset's first input
						InputPresets.Add(CurrentObject, 0);
					}
				}

				// Set the input preset on the HoudiniAssetComponent
				if (InputPresets.Num() > 0)
					HoudiniAssetComponent->SetInputPresets(InputPresets);
			}
		}

		// Select the Actor we just created
		if (GEditor->CanSelectActor(CreatedActor, true, true))
		{
			GEditor->SelectNone(true, true, false);
			GEditor->SelectActor(CreatedActor, true, true, true);
		}
	}
}

AActor*
FHoudiniEngineEditorUtils::InstantiateHoudiniAssetAt(UHoudiniAsset* InHoudiniAsset, const FTransform& InTransform, UWorld* InSpawnInWorld, ULevel* InSpawnInLevelOverride)
{
	if (!InHoudiniAsset)
		return nullptr;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	// Load the asset
	UObject* AssetObj = Cast<UObject>(InHoudiniAsset);//InHoudiniAsset->LoadSynchronous();
	if (!AssetObj)
		return nullptr;

	// Get the asset Factory
	UActorFactory* Factory = GEditor->FindActorFactoryForActorClass(AHoudiniAssetActor::StaticClass());
	if (!Factory)
		return nullptr;

	// Determine the level to spawn in from the supplied parameters
	// InSpawnInLevelOverride if valid, else if InSpawnInWorld is valid its current level
	// lastly, get the editor world's current level
	ULevel* LevelToSpawnIn = InSpawnInLevelOverride;
	if (!IsValid(LevelToSpawnIn))
	{
		if (IsValid(InSpawnInWorld))
			LevelToSpawnIn = InSpawnInWorld->GetCurrentLevel();
		else
			LevelToSpawnIn = GEditor->GetEditorWorldContext().World()->GetCurrentLevel();
	}

	// We only need to create a single instance of the asset, regarding of the selection
	AActor* CreatedActor = Factory->CreateActor(AssetObj, LevelToSpawnIn, InTransform);
	if (!CreatedActor)
		return nullptr;

	// Select the Actor we just created
	if (GEditor->CanSelectActor(CreatedActor, true, true))
	{
		GEditor->SelectNone(true, true, false);
		GEditor->SelectActor(CreatedActor, true, true, true);
	}

	return CreatedActor;
}


void
FHoudiniEngineEditorUtils::SaveAllHoudiniTemporaryCookData(UWorld *InSaveWorld)
{
	// Add a slate notification
	FString Notification = TEXT("Saving all Houdini temporary cook data...");
	// FHoudiniEngineUtils::CreateSlateNotification(Notification);

	TArray<UPackage*> PackagesToSave;
	for (TObjectIterator<UHoudiniAssetComponent> Itr; Itr; ++Itr)
	{
		UHoudiniAssetComponent * HAC = *Itr;
		if (!IsValid(HAC))
			continue;

		if (InSaveWorld && InSaveWorld != HAC->GetWorld())
			continue;

		const int32 NumOutputs = HAC->GetNumOutputs();
		for (int32 Index = 0; Index < NumOutputs; ++Index)
		{
			UHoudiniOutput *Output = HAC->GetOutputAt(Index);
			if (!IsValid(Output))
				continue;

			// TODO: Also save landscape layer info objects?
			if (Output->GetType() != EHoudiniOutputType::Mesh)
				continue;

			for (auto &OutputObjectPair : Output->GetOutputObjects())
			{
				UObject *Obj = OutputObjectPair.Value.OutputObject;
				if (!IsValid(Obj))
					continue;

				UStaticMesh *SM = Cast<UStaticMesh>(Obj);
				if (!SM)
					continue;

				UPackage *Package = SM->GetOutermost();
				if (!IsValid(Package))
					continue;

				if (Package->IsDirty() && Package->IsFullyLoaded() && Package != GetTransientPackage())
				{
					PackagesToSave.Add(Package);
				}
			}

			for (auto& MaterialAssignementPair : Output->GetAssignementMaterials())
			{
				UMaterialInterface* MatInterface = MaterialAssignementPair.Value;
				if (!IsValid(MatInterface))
					continue;

				UPackage *Package = MatInterface->GetOutermost();
				if (!IsValid(Package))
					continue;

				if (Package->IsDirty() && Package->IsFullyLoaded() && Package != GetTransientPackage())
				{
					PackagesToSave.Add(Package);
				}
			}
		}
	}

	UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true);
}

void
FHoudiniEngineEditorUtils::ReselectSelectedActors()
{
	// TODO: Duplicate with FHoudiniEngineUtils::UpdateEditorProperties ??
	USelection* Selection = GEditor->GetSelectedActors();
	TArray<AActor*> SelectedActors;
	SelectedActors.SetNum(GEditor->GetSelectedActorCount());
	Selection->GetSelectedObjects(SelectedActors);

	GEditor->SelectNone(false, false, false);

	for (AActor* NextSelected : SelectedActors)
	{
		GEditor->SelectActor(NextSelected, true, true, true, true);
	}
}

void
FHoudiniEngineEditorUtils::ReselectActorIfSelected(AActor* const InActor)
{
	if (IsValid(InActor) && InActor->IsSelected())
	{
		GEditor->SelectActor(InActor, false, false, false, false);
		GEditor->SelectActor(InActor, true, true, true, true);
	}
}

void
FHoudiniEngineEditorUtils::ReselectComponentOwnerIfSelected(USceneComponent* const InComponent)
{
	if (!IsValid(InComponent))
		return;
	AActor* const Actor = InComponent->GetOwner();
	if (!IsValid(Actor))
		return;
	ReselectActorIfSelected(Actor);
}

FString
FHoudiniEngineEditorUtils::GetNodeNamePaddedByPathDepth(const FString& InNodeName, const FString& InNodePath, const uint8 Padding, const TCHAR PathSep)
{
	int32 Depth = 0;
	for (const TCHAR Char : InNodePath)
	{
		if (Char == PathSep)
			Depth++;
	}
	FString Trimmed = InNodeName;
	Trimmed.TrimStartInline();
	return Trimmed.LeftPad(Trimmed.Len() + (Depth * Padding));
}

void
FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty(FName InPropertyPath, UObject* InRootObject)
{
	if (!IsValid(InRootObject))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineEditorUtils::NotifyPostEditChangeProperty]: InRootObject is null."));
		return;
	}
	
	const FCachedPropertyPath CachedPath(InPropertyPath.ToString());
	if (CachedPath.Resolve(InRootObject))
	{
		// Notify that we have changed the property
		// FPropertyChangedEvent Evt = CachedPath.ToPropertyChangedEvent(EPropertyChangeType::Unspecified);
		// Construct FPropertyChangedEvent from the cached property path
		const int32 NumSegments = CachedPath.GetNumSegments();
		FPropertyChangedEvent Evt(
			CastFieldChecked<FProperty>(CachedPath.GetLastSegment().GetField().ToField()),
			EPropertyChangeType::Unspecified,
			{ InRootObject });
		
		if(NumSegments > 1)
		{
			Evt.SetActiveMemberProperty(CastFieldChecked<FProperty>(CachedPath.GetSegment(NumSegments - 2).GetField().ToField()));
		}

		// Set the array of indices to the changed property
		TArray<TMap<FString,int32>> ArrayIndicesPerObject;
		ArrayIndicesPerObject.AddDefaulted(1);
		for (int32 SegmentIdx = 0; SegmentIdx < NumSegments; ++SegmentIdx)
		{
			const FPropertyPathSegment& Segment = CachedPath.GetSegment(SegmentIdx);
			const int32 ArrayIndex = Segment.GetArrayIndex();
			if (ArrayIndex != INDEX_NONE)
			{
				ArrayIndicesPerObject[0].Add(Segment.GetName().ToString(), ArrayIndex);
			}
		}
		Evt.SetArrayIndexPerObject(ArrayIndicesPerObject);
		
		FEditPropertyChain Chain;
		CachedPath.ToEditPropertyChain(Chain);
		FPropertyChangedChainEvent ChainEvent(Chain, Evt);
		ChainEvent.ObjectIteratorIndex = 0;
		InRootObject->PostEditChangeChainProperty(ChainEvent);
	}
	else
	{
		HOUDINI_LOG_WARNING(TEXT("Could not resolve property path '%s' on %s."), *InPropertyPath.ToString(), *(InRootObject->GetFullName()));
	}
}

#undef LOCTEXT_NAMESPACE
