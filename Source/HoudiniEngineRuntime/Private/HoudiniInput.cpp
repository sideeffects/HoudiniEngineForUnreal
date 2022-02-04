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

#include "HoudiniInput.h"

#include "HoudiniEngineRuntime.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniOutput.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniAsset.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetBlueprintComponent.h"

#include "EngineUtils.h"
#include "Engine/Brush.h"
#include "Engine/Engine.h"
#include "Engine/DataTable.h"
#include "Model.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/UObjectGlobals.h"
#include "FoliageType_InstancedStaticMesh.h"

#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionActor.h"

#if WITH_EDITOR

#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

#endif

//
UHoudiniInput::UHoudiniInput()
	: Type(EHoudiniInputType::Invalid)
	, PreviousType(EHoudiniInputType::Invalid)
	, AssetNodeId(-1)
	, InputNodeId(-1)
	, InputIndex(0)
	, ParmId(-1)
	, bIsObjectPathParameter(false)
	, bHasChanged(false)
	, bPackBeforeMerge(false)
	, bExportLODs(false)
	, bExportSockets(false)
	, bExportColliders(false)
	, bCookOnCurveChanged(true)
	, bStaticMeshChanged(false)
	, bInputAssetConnectedInHoudini(false)
	, DefaultCurveOffset(0.f)
	, bAddRotAndScaleAttributesOnCurves(false)
	, bUseLegacyInputCurves(false)
	, bIsWorldInputBoundSelector(false)
	, bWorldInputBoundSelectorAutoUpdate(false)
	, UnrealSplineResolution(0.0f)
	, bUpdateInputLandscape(false)
	, LandscapeExportType(EHoudiniLandscapeExportType::Heightfield)
	, bLandscapeExportSelectionOnly(false)
	, bLandscapeAutoSelectComponent(false)
	, bLandscapeExportMaterials(false)
	, bLandscapeExportLighting(false)
	, bLandscapeExportNormalizedUVs(false)
	, bLandscapeExportTileUVs(false)
	, bCanDeleteHoudiniNodes(true)
{
	Name = TEXT("");
	Label = TEXT("");
	SetFlags(RF_Transactional);

	// Geometry inputs always have one null default object
	GeometryInputObjects.Add(nullptr);
	GeometryCollectionInputObjects.Add(nullptr);
	
	KeepWorldTransform = GetDefaultXTransformType();

	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	UnrealSplineResolution = HoudiniRuntimeSettings ? HoudiniRuntimeSettings->MarshallingSplineResolution : 50.0f;

	bAddRotAndScaleAttributesOnCurves = HoudiniRuntimeSettings ? HoudiniRuntimeSettings->bAddRotAndScaleAttributesOnCurves : false;
	bUseLegacyInputCurves = HoudiniRuntimeSettings ? HoudiniRuntimeSettings->bUseLegacyInputCurves : false;

#if WITH_EDITORONLY_DATA
	bLandscapeUIAdvancedIsExpanded = false;
#endif
	
}

void
UHoudiniInput::BeginDestroy()
{
	InvalidateData();

	// DO NOT MANUALLY DESTROY OUR INPUT OBJECTS!
	// This messes up unreal's Garbage collection and would cause crashes on duplication

	// Mark all our input objects for destruction
	ForAllHoudiniInputObjectArrays([](TArray<UHoudiniInputObject*>& ObjectArray) {
		ObjectArray.Empty();
	});

	Super::BeginDestroy();
}

#if WITH_EDITOR
void UHoudiniInput::PostEditUndo() 
{
	 Super::PostEditUndo();
	 TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(Type);
	 if (!InputObjectsPtr)
		 return;

	 MarkChanged(true);
	 bool bBlueprintStructureChanged = false;

	 if (HasInputTypeChanged()) 
	 {
		 // If the input type has changed on undo, previousType becomes new type
		 /*   This does not work properly, see the corresponding part in FHoudiniInputDetails::AddInputTypeComboBox(...), after Transaction(...
		 )
		 {
			 EHoudiniInputType NewType = PreviousType;
			 SetInputType(NewType);
		 }
		 */
		 EHoudiniInputType Temp = Type;
		 Type = PreviousType;
		 PreviousType = EHoudiniInputType::Invalid;

		 // If the undo action caused input type changing, treat it as a regular type changing
		 // after set up the new and prev types properly 
		 SetInputType(Temp, bBlueprintStructureChanged);
	 }
	 else
	 {
		 if (Type == EHoudiniInputType::Asset)
		 {
			 // Mark the input asset object as changed, since only undo changing asset will get into here.
			 // The input array will be empty when undo adding asset (only support single asset input object in an input now)
			 for (auto & NextAssetInputObj : *InputObjectsPtr)
			 {
				 if (!IsValid(NextAssetInputObj))
					 continue;

				 NextAssetInputObj->MarkChanged(true);
			 }
		 }


		 if (Type == EHoudiniInputType::World)
		 {
			 if (WorldInputObjects.Num() == 0 && InputNodeId >= 0)
			 {
				 for (auto & NextNodeId : CreatedDataNodeIds)
				 {
					 if (bCanDeleteHoudiniNodes)
						FHoudiniEngineRuntime::Get().MarkNodeIdAsPendingDelete(NextNodeId, true);
				 }

				 CreatedDataNodeIds.Empty();

				 if (bCanDeleteHoudiniNodes)
					MarkInputNodeAsPendingDelete();
				 InputNodeId = -1;
			 }
		 }

		 if (Type == EHoudiniInputType::Curve)
		 {
			 if (PreviousType != EHoudiniInputType::Curve)
			 {
				 for (auto& NextInput : *GetHoudiniInputObjectArray(Type))
				 {
					 UHoudiniInputHoudiniSplineComponent* SplineInput = Cast< UHoudiniInputHoudiniSplineComponent>(NextInput);
					 if (!IsValid(SplineInput))
						 continue;

					 UHoudiniSplineComponent * HoudiniSplineComponent = SplineInput->GetCurveComponent();
					 if (!IsValid(HoudiniSplineComponent))
						 continue;

					 USceneComponent* OuterComponent = Cast<USceneComponent>(GetOuter());

					 // Attach the new Houdini spline component to it's owner
					 HoudiniSplineComponent->RegisterComponent();
					 HoudiniSplineComponent->AttachToComponent(OuterComponent, FAttachmentTransformRules::KeepRelativeTransform);
					 HoudiniSplineComponent->SetVisibility(true, true);
					 HoudiniSplineComponent->SetHoudiniSplineVisible(true);
					 HoudiniSplineComponent->SetHiddenInGame(false, true);
					 HoudiniSplineComponent->MarkChanged(true);
				 }
				 return;
			 }
			 bool bUndoDelete = false;
			 bool bUndoInsert = false;
			 bool bUndoDeletedObjArrayEmptied = false;

			 TArray< USceneComponent* > childActor;
			 UHoudiniAssetComponent* OuterHAC = Cast<UHoudiniAssetComponent>(GetOuter());
			 if (IsValid(OuterHAC))
				 childActor = OuterHAC->GetAttachChildren();

			 // Undo delete input objects action
			 for (int Index = 0; Index < GetNumberOfInputObjects(); ++Index)
			 {
				 UHoudiniInputObject* InputObject = (*InputObjectsPtr)[Index];
				 if (!IsValid(InputObject))
					 continue;

				 UHoudiniInputHoudiniSplineComponent * HoudiniSplineInputObject = Cast<UHoudiniInputHoudiniSplineComponent>(InputObject);

				 if (!IsValid(HoudiniSplineInputObject))
					 continue;

				 UHoudiniSplineComponent* SplineComponent = HoudiniSplineInputObject->GetCurveComponent();

				 if (!IsValid(SplineComponent))
					 continue;

				 // If the last change deleted this curve input, recreate this Houdini Spline input.
				 if (!SplineComponent->GetAttachParent())
				 {
					 bUndoDelete = true;

					 if (!bUndoDeletedObjArrayEmptied)  
						 LastUndoDeletedInputs.Empty();

					 bUndoDeletedObjArrayEmptied = true;

					 UHoudiniSplineComponent * ReconstructedSpline = NewObject<UHoudiniSplineComponent>(
						 GetOuter(), UHoudiniSplineComponent::StaticClass());

					 if (!IsValid(ReconstructedSpline))
						 continue;

					 ReconstructedSpline->SetFlags(RF_Transactional);
					 ReconstructedSpline->CopyHoudiniData(SplineComponent);

					 UHoudiniInputObject * ReconstructedInputObject = UHoudiniInputHoudiniSplineComponent::Create(
						 ReconstructedSpline, GetOuter(), ReconstructedSpline->GetHoudiniSplineName());
					 UHoudiniInputHoudiniSplineComponent *ReconstructedHoudiniSplineInput = (UHoudiniInputHoudiniSplineComponent*)ReconstructedInputObject;
					 (*InputObjectsPtr)[Index] = ReconstructedHoudiniSplineInput;

					 ReconstructedSpline->RegisterComponent();
					 ReconstructedSpline->SetFlags(RF_Transactional);

					 CreateHoudiniSplineInput(ReconstructedHoudiniSplineInput, true, true, bBlueprintStructureChanged);

					 // Cast the reconstructed Houdini Spline Input to a generic HoudiniInput object.
					 UHoudiniInputObject * ReconstructedHoudiniInput = Cast<UHoudiniInputObject>(ReconstructedHoudiniSplineInput);

					 LastUndoDeletedInputs.Add(ReconstructedHoudiniInput);
					 // Reset the LastInsertedInputsArray for redoing this undo action.
				 }
			 }

			 if (bUndoDelete)
				 return;

			 // Undo insert input objects action
			 for (int Index = 0; Index < LastInsertedInputs.Num(); ++Index)
			 {
				 bUndoInsert = true;
				 UHoudiniInputHoudiniSplineComponent* SplineInputComponent = LastInsertedInputs[Index];
				 if (!IsValid(SplineInputComponent)) 
					 continue;

				 UHoudiniSplineComponent* HoudiniSplineComponent = SplineInputComponent->GetCurveComponent();
				 if (!IsValid(HoudiniSplineComponent)) 
					 continue;

				 HoudiniSplineComponent->DestroyComponent();
			 }

			 if (bUndoInsert)
				 return;

			 for (int Index = 0; Index < LastUndoDeletedInputs.Num(); ++Index)
			 {
				 UHoudiniInputObject* NextInputObject = LastUndoDeletedInputs[Index];

				 UHoudiniInputHoudiniSplineComponent* SplineInputComponent = Cast<UHoudiniInputHoudiniSplineComponent>(NextInputObject);
				 if (!IsValid(SplineInputComponent))
					 continue;

				 UHoudiniSplineComponent* HoudiniSplineComponent = SplineInputComponent->GetCurveComponent();
				 if (!IsValid(HoudiniSplineComponent))
					 continue;

				 FDetachmentTransformRules DetachTransRules(EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, false);
				 HoudiniSplineComponent->DetachFromComponent(DetachTransRules);

				 HoudiniSplineComponent->DestroyComponent();
			 }
		 }
	 }

	 if (bBlueprintStructureChanged)
	 {
		 UHoudiniAssetComponent* OuterHAC = Cast<UHoudiniAssetComponent>(GetOuter());
		 FHoudiniEngineRuntimeUtils::MarkBlueprintAsStructurallyModified(OuterHAC);
	 }

}
#endif


FBox 
UHoudiniInput::GetBounds() const 
{
	FBox BoxBounds(ForceInitToZero);

	switch (Type)
	{
	case EHoudiniInputType::Curve:
	{
		for (int32 Idx = 0; Idx < CurveInputObjects.Num(); ++Idx)
		{
			const UHoudiniInputHoudiniSplineComponent* CurInCurve = Cast<UHoudiniInputHoudiniSplineComponent>(CurveInputObjects[Idx]);
			if (!IsValid(CurInCurve))
				continue;

			UHoudiniSplineComponent* CurCurve = CurInCurve->GetCurveComponent();
			if (!IsValid(CurCurve))
				continue;

			FBox CurCurveBound(ForceInitToZero);
			for (auto & Trans : CurCurve->CurvePoints)
			{
				CurCurveBound += Trans.GetLocation();
			}

			UHoudiniAssetComponent* OuterHAC = Cast<UHoudiniAssetComponent>(GetOuter());

			if (IsValid(OuterHAC))
				BoxBounds += CurCurveBound.MoveTo(OuterHAC->GetComponentLocation());
		}
	}
	break;

	case EHoudiniInputType::Asset:
	{
		for (int32 Idx = 0; Idx < AssetInputObjects.Num(); ++Idx)
		{
			UHoudiniInputHoudiniAsset* CurInAsset = Cast<UHoudiniInputHoudiniAsset>(AssetInputObjects[Idx]);
			if (!IsValid(CurInAsset))
				continue;

			UHoudiniAssetComponent* CurInHAC = CurInAsset->GetHoudiniAssetComponent();
			if (!IsValid(CurInHAC))
				continue;

			BoxBounds += CurInHAC->GetAssetBounds(nullptr, false);
		}
	}
	break;

	case EHoudiniInputType::World:
	{
		for (int32 Idx = 0; Idx < WorldInputObjects.Num(); ++Idx)
		{
			UHoudiniInputActor* CurInActor = Cast<UHoudiniInputActor>(WorldInputObjects[Idx]);
			if (IsValid(CurInActor))
			{
				AActor* Actor = CurInActor->GetActor();
				if (!IsValid(Actor))
					continue;

				FVector Origin, Extent;
				Actor->GetActorBounds(false, Origin, Extent);

				BoxBounds += FBox::BuildAABB(Origin, Extent);
			}
			else
			{
				// World Input now also support HoudiniAssets
				UHoudiniInputHoudiniAsset* CurInAsset = Cast<UHoudiniInputHoudiniAsset>(WorldInputObjects[Idx]);
				if (IsValid(CurInAsset))
				{
					UHoudiniAssetComponent* CurInHAC = CurInAsset->GetHoudiniAssetComponent();
					if (!IsValid(CurInHAC))
						continue;

					BoxBounds += CurInHAC->GetAssetBounds(nullptr, false);
					continue;
				}
			}
		}
	}
	break;

	case EHoudiniInputType::Landscape:
	{
		for (int32 Idx = 0; Idx < LandscapeInputObjects.Num(); ++Idx)
		{
			UHoudiniInputLandscape* CurInLandscape = Cast<UHoudiniInputLandscape>(LandscapeInputObjects[Idx]);
			if (!IsValid(CurInLandscape))
				continue;

			ALandscapeProxy* CurLandscape = CurInLandscape->GetLandscapeProxy();
			if (!IsValid(CurLandscape))
				continue;

			FVector Origin, Extent;
			CurLandscape->GetActorBounds(false, Origin, Extent);

			BoxBounds += FBox::BuildAABB(Origin, Extent);
		}
	}
	break;
	case EHoudiniInputType::GeometryCollection:
	{
		// TODO: Can probably iterate through the GC objects to properly identify the bounds.
	}
	break;
	case EHoudiniInputType::Skeletal:
	case EHoudiniInputType::Invalid:
	default:
		break;
	}

	return BoxBounds;
}

void UHoudiniInput::UpdateLandscapeInputSelection()
{
	LandscapeSelectedComponents.Reset();
	if (!bLandscapeExportSelectionOnly) return;

#if WITH_EDITOR
	for (UHoudiniInputObject* NextInputObj : LandscapeInputObjects)
	{
		UHoudiniInputLandscape* CurrentInputLandscape = Cast<UHoudiniInputLandscape>(NextInputObj);
		if (!CurrentInputLandscape)
			continue;

		ALandscapeProxy* CurrentInputLandscapeProxy = CurrentInputLandscape->GetLandscapeProxy();
		if (!CurrentInputLandscapeProxy)
			continue;

		// Get selected components if bLandscapeExportSelectionOnly or bLandscapeAutoSelectComponent is true
		FBox Bounds(ForceInitToZero);
		if ( bLandscapeAutoSelectComponent )
		{
			// Get our asset's or our connected input asset's bounds
			UHoudiniAssetComponent* AssetComponent = Cast<UHoudiniAssetComponent>(GetOuter());
			if (IsValid(AssetComponent))
			{
				Bounds = AssetComponent->GetAssetBounds(this, true);
			}
		}
	
		if ( bLandscapeExportSelectionOnly )
		{
			const ULandscapeInfo * LandscapeInfo = CurrentInputLandscapeProxy->GetLandscapeInfo();
			if ( IsValid(LandscapeInfo) )
			{
				// Get the currently selected components
				LandscapeSelectedComponents = LandscapeInfo->GetSelectedComponents();
			}
	
			if ( bLandscapeAutoSelectComponent && LandscapeSelectedComponents.Num() <= 0 && Bounds.IsValid )
			{
				// We'll try to use the asset bounds to automatically "select" the components
				for ( int32 ComponentIdx = 0; ComponentIdx < CurrentInputLandscapeProxy->LandscapeComponents.Num(); ComponentIdx++ )
				{
					ULandscapeComponent * LandscapeComponent = CurrentInputLandscapeProxy->LandscapeComponents[ ComponentIdx ];
					if ( !IsValid(LandscapeComponent) )
						continue;
	
					FBoxSphereBounds WorldBounds = LandscapeComponent->CalcBounds( LandscapeComponent->GetComponentTransform());
	
					if ( Bounds.IntersectXY( WorldBounds.GetBox() ) )
						LandscapeSelectedComponents.Add( LandscapeComponent );
				}
	
				int32 Num = LandscapeSelectedComponents.Num();
				HOUDINI_LOG_MESSAGE( TEXT("Landscape input: automatically selected %d components within the asset's bounds."), Num );
			}
		}
		else
		{
			// Add all the components of the landscape to the selected set
			ULandscapeInfo* LandscapeInfo = CurrentInputLandscapeProxy->GetLandscapeInfo();
			if (LandscapeInfo)
			{
				LandscapeInfo->ForAllLandscapeComponents([&](ULandscapeComponent* Component)
				{
					LandscapeSelectedComponents.Add(Component);
				});
			}
		}

		CurrentInputLandscape->MarkChanged(true);
	
	}
#endif
}

void
UHoudiniInput::MarkInputNodeAsPendingDelete()
{
	if (InputNodeId < 0)
		return;

	InputNodesPendingDelete.Add(InputNodeId);
	InputNodeId = -1;
}

FString
UHoudiniInput::InputTypeToString(const EHoudiniInputType& InInputType)
{
	FString InputTypeStr;
	switch (InInputType)
	{
		case EHoudiniInputType::Geometry:
		{
			InputTypeStr = TEXT("Geometry Input");
		}
		break;

		case EHoudiniInputType::Asset:
		{
			InputTypeStr = TEXT("Asset Input");
		}
		break;

		case EHoudiniInputType::Curve:
		{
			InputTypeStr = TEXT("Curve Input");
		}
		break;

		case EHoudiniInputType::Landscape:
		{
			InputTypeStr = TEXT("Landscape Input");
		}
		break;

		case EHoudiniInputType::World:
		{
			InputTypeStr = TEXT("World Outliner Input");
		}
		break;

		case EHoudiniInputType::Skeletal:
		{
			InputTypeStr = TEXT("Skeletal Mesh Input");
		}
		break;
		case EHoudiniInputType::GeometryCollection:
		{
			InputTypeStr = TEXT("GeometryCollection Input");
		}
		break;
	}

	return InputTypeStr;
}


EHoudiniInputType
UHoudiniInput::StringToInputType(const FString& InInputTypeString)
{
	// Note: Geometry is a prefix of GeometryCollection, so need to make sure to do GeometryCollection first!
	if (InInputTypeString.StartsWith(TEXT("GeometryCollection"), ESearchCase::IgnoreCase))
	{
		return EHoudiniInputType::GeometryCollection;
	}
	else if (InInputTypeString.StartsWith(TEXT("Geometry"), ESearchCase::IgnoreCase))
	{
		return EHoudiniInputType::Geometry;
	}
	else if (InInputTypeString.StartsWith(TEXT("Asset"), ESearchCase::IgnoreCase))
	{
		return EHoudiniInputType::Asset;
	}
	else if (InInputTypeString.StartsWith(TEXT("Curve"), ESearchCase::IgnoreCase))
	{
		return EHoudiniInputType::Curve;
	}
	else if (InInputTypeString.StartsWith(TEXT("Landscape"), ESearchCase::IgnoreCase))
	{
		return EHoudiniInputType::Landscape;
	}
	else if (InInputTypeString.StartsWith(TEXT("World"), ESearchCase::IgnoreCase))
	{
		return EHoudiniInputType::World;
	}
	else if (InInputTypeString.StartsWith(TEXT("Skeletal"), ESearchCase::IgnoreCase))
	{
		return EHoudiniInputType::Skeletal;
	}

	return EHoudiniInputType::Invalid;
}


EHoudiniCurveType UHoudiniInput::StringToHoudiniCurveType(const FString& HoudiniCurveTypeString) 
{
	if (HoudiniCurveTypeString.StartsWith(TEXT("Polygon"), ESearchCase::IgnoreCase))
	{
		return EHoudiniCurveType::Polygon;
	}
	else if (HoudiniCurveTypeString.StartsWith(TEXT("Nurbs"), ESearchCase::IgnoreCase))
	{
		return EHoudiniCurveType::Nurbs;
	}
	else if (HoudiniCurveTypeString.StartsWith(TEXT("Bezier"), ESearchCase::IgnoreCase))
	{
		return EHoudiniCurveType::Bezier;
	}
	else if (HoudiniCurveTypeString.StartsWith(TEXT("Points"), ESearchCase::IgnoreCase))
	{
		return EHoudiniCurveType::Points;
	}
	
	return EHoudiniCurveType::Invalid;
}

EHoudiniCurveMethod UHoudiniInput::StringToHoudiniCurveMethod(const FString& HoudiniCurveMethodString) 
{
	if (HoudiniCurveMethodString.StartsWith(TEXT("CVs"), ESearchCase::IgnoreCase)) 
	{
		return EHoudiniCurveMethod::CVs;
	}
	else if (HoudiniCurveMethodString.StartsWith(TEXT("Breakpoints"), ESearchCase::IgnoreCase)) 
	{
		return EHoudiniCurveMethod::Breakpoints;
	}

	else if (HoudiniCurveMethodString.StartsWith(TEXT("Freehand"), ESearchCase::IgnoreCase))
	{
		return EHoudiniCurveMethod::Freehand;
	}

	return EHoudiniCurveMethod::Invalid;

}

EHoudiniCurveBreakpointParameterization UHoudiniInput::StringToHoudiniCurveBreakpointParameterization(const FString& CurveParameterizationString)
{
	if (CurveParameterizationString.StartsWith(TEXT("Uniform"), ESearchCase::IgnoreCase)) 
	{
		return EHoudiniCurveBreakpointParameterization::Uniform;
	}
	else if (CurveParameterizationString.StartsWith(TEXT("Chord"), ESearchCase::IgnoreCase)) 
	{
		return EHoudiniCurveBreakpointParameterization::Chord;
	}

	else if (CurveParameterizationString.StartsWith(TEXT("Centripetal"), ESearchCase::IgnoreCase))
	{
		return EHoudiniCurveBreakpointParameterization::Centripetal;
	}

	return EHoudiniCurveBreakpointParameterization::Invalid;
}

// 
void 
UHoudiniInput::SetSOPInput(const int32& InInputIndex)
{
	// Set the input index
	InputIndex = InInputIndex;

	// Invalidate objpath parameter
	ParmId = -1;
	bIsObjectPathParameter = false;
}

void
UHoudiniInput::SetObjectPathParameter(const int32& InParmId)
{
	// Set as objpath parameter
	ParmId = InParmId;
	bIsObjectPathParameter = true;

	// Invalidate the geo input
	InputIndex = -1;
}

EHoudiniXformType 
UHoudiniInput::GetDefaultXTransformType() 
{
	switch (Type)
	{
		case EHoudiniInputType::Curve:
		case EHoudiniInputType::Geometry:
		case EHoudiniInputType::Skeletal:
		case EHoudiniInputType::GeometryCollection: // TODO; Double check this.
			return EHoudiniXformType::None;
		case EHoudiniInputType::Asset:
		case EHoudiniInputType::Landscape:
		case EHoudiniInputType::World:
			return EHoudiniXformType::IntoThisObject;
	}

	return EHoudiniXformType::Auto;
}

bool
UHoudiniInput::GetKeepWorldTransform() const
{
	bool bReturn = false;
	switch (KeepWorldTransform)
	{
		case EHoudiniXformType::Auto:
		{
			// Return default values corresponding to the input type:
			if (Type == EHoudiniInputType::Curve
				|| Type == EHoudiniInputType::Geometry
				|| Type == EHoudiniInputType::Skeletal
				|| Type == EHoudiniInputType::GeometryCollection)
			{
				// NONE  for Geo, Curve and skeletal mesh IN
				bReturn = false;
			}
			else
			{
				// INTO THIS OBJECT for Asset, Landscape and World IN
				bReturn = true;
			}
			break;
		}
		
		case EHoudiniXformType::None:
		{
			bReturn = false;
			break;
		}

		case EHoudiniXformType::IntoThisObject:
		{
			bReturn = true;
			break;
		}
	}

	return bReturn;
}

void
UHoudiniInput::SetKeepWorldTransform(const bool& bInKeepWorldTransform)
{
	if (bInKeepWorldTransform)
	{
		KeepWorldTransform = EHoudiniXformType::IntoThisObject;
	}
	else
	{
		KeepWorldTransform = EHoudiniXformType::None;
	}
}

void 
UHoudiniInput::SetInputType(const EHoudiniInputType& InInputType, bool& bOutBlueprintStructureModified)
{ 
	if (InInputType == Type)
		return;

	SetPreviousInputType(Type);

	// Mark this input as changed
	MarkChanged(true);
	bOutBlueprintStructureModified = true;

	// Check previous input type
	switch (PreviousType) 
	{
		case EHoudiniInputType::Asset:
		{
			break;
		}

		case EHoudiniInputType::Curve:
		{
			// detach the input curves from the asset component
			if (GetNumberOfInputObjects() > 0)
			{
				for (UHoudiniInputObject * CurrentInput : *GetHoudiniInputObjectArray(Type))
				{
					UHoudiniInputHoudiniSplineComponent * CurrentInputHoudiniSpline = Cast<UHoudiniInputHoudiniSplineComponent>(CurrentInput);

					if (!IsValid(CurrentInputHoudiniSpline))
						continue;

					UHoudiniSplineComponent * HoudiniSplineComponent = CurrentInputHoudiniSpline->GetCurveComponent();


					if (!IsValid(HoudiniSplineComponent))
						continue;

					HoudiniSplineComponent->Modify();

					const bool bIsArchetype = HoudiniSplineComponent->HasAnyFlags(RF_ArchetypeObject|RF_ClassDefaultObject);

					if (bIsArchetype)
					{
#if WITH_EDITOR
						check(HoudiniSplineComponent->IsTemplate());
						FHoudiniEngineRuntimeUtils::SetTemplatePropertyValue(HoudiniSplineComponent, "bVisible", false);
						FHoudiniEngineRuntimeUtils::SetTemplatePropertyValue(HoudiniSplineComponent, "bHiddenInGame", true);
						FHoudiniEngineRuntimeUtils::SetTemplatePropertyValue(HoudiniSplineComponent, "bIsHoudiniSplineVisible", false);
						FHoudiniEngineRuntimeUtils::SetTemplatePropertyValue(HoudiniSplineComponent, "bHasChanged", true);
						FHoudiniEngineRuntimeUtils::SetTemplatePropertyValue(HoudiniSplineComponent, "bNeedsToTriggerUpdate", true);
#endif
					}
					else
					{
						AActor* OwningActor = HoudiniSplineComponent->GetOwner();
						check(OwningActor);

						FDetachmentTransformRules DetachTransRules(EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, false);
						HoudiniSplineComponent->DetachFromComponent(DetachTransRules);						
				 		HoudiniSplineComponent->SetVisibility(false, true);
						HoudiniSplineComponent->SetHoudiniSplineVisible(false);
						HoudiniSplineComponent->SetHiddenInGame(true, true);
						
						// This NodeId shouldn't be invalidated like this. If a spline component
						// or curve input is no longer valid, the input object should be removed from the HoudinInput
						// to get cleaned up properly.
						// HoudiniSplineComponent->SetNodeId(-1);
						HoudiniSplineComponent->MarkChanged(true);
					}

					bOutBlueprintStructureModified = true;
				}
			}
			break;
		}

		case EHoudiniInputType::Geometry:
		{
			break;
		}

		case EHoudiniInputType::Landscape:
		{
			TArray<UHoudiniInputObject*>* InputObjectsArray = GetHoudiniInputObjectArray(PreviousType);

			if (!InputObjectsArray)
				break;
		
			for (int32 Idx = 0; Idx < InputObjectsArray->Num(); ++Idx)
			{
				UHoudiniInputObject* InputObj = (*InputObjectsArray)[Idx];

				if (!IsValid(InputObj))
					continue;

				UHoudiniInputLandscape* InputLandscape = Cast<UHoudiniInputLandscape>(InputObj);

				if (!IsValid(InputLandscape))
					continue;

				// do something?
			}
		
			break;
		}

		case EHoudiniInputType::Skeletal:
		{
			break;
		}

		case EHoudiniInputType::GeometryCollection:
		{
			break;
		}

		case EHoudiniInputType::World:
		{
			break;
		}

		default:
			break;
	}


	Type = InInputType;

	// TODO: NOPE, not needed
	// Set keep world transform to default w.r.t to new input type.
	//KeepWorldTransform = GetDefaultXTransformType();
	
	// Check current input type
	switch (InInputType) 
	{
		case EHoudiniInputType::World:
		case EHoudiniInputType::Asset:
		{
			UHoudiniAssetComponent* OuterHAC = Cast<UHoudiniAssetComponent>(GetOuter());
			if (OuterHAC && !bImportAsReference) 
			{
				for (auto& CurrentInput : *GetHoudiniInputObjectArray(Type)) 
				{
					UHoudiniInputHoudiniAsset* HoudiniAssetInput = Cast<UHoudiniInputHoudiniAsset>(CurrentInput);
					if (!IsValid(HoudiniAssetInput))
						continue;

					UHoudiniAssetComponent* CurrentHAC = HoudiniAssetInput->GetHoudiniAssetComponent();
					if (!IsValid(CurrentHAC))
						continue;

					CurrentHAC->AddDownstreamHoudiniAsset(OuterHAC);
				}
			}
		}
		break;

		case EHoudiniInputType::Curve:
		{
			if (GetNumberOfInputObjects() == 0)
			{
				CreateNewCurveInputObject(bOutBlueprintStructureModified);
				MarkChanged(true);
			}
			else
			{
				for (auto& CurrentInput : *GetHoudiniInputObjectArray(Type))
				{
					UHoudiniInputHoudiniSplineComponent* SplineInput = Cast< UHoudiniInputHoudiniSplineComponent>(CurrentInput);
					if (!IsValid(SplineInput))
						continue;

					UHoudiniSplineComponent * HoudiniSplineComponent = SplineInput->GetCurveComponent();
					if (!IsValid(HoudiniSplineComponent))
						continue;
				
					HoudiniSplineComponent->Modify();

					const bool bIsArchetype = HoudiniSplineComponent->HasAnyFlags(RF_ArchetypeObject|RF_ClassDefaultObject);

					if (bIsArchetype)
					{
#if WITH_EDITOR
						check(HoudiniSplineComponent->IsTemplate());
						FHoudiniEngineRuntimeUtils::SetTemplatePropertyValue(HoudiniSplineComponent, "bVisible", true);
						FHoudiniEngineRuntimeUtils::SetTemplatePropertyValue(HoudiniSplineComponent, "bHiddenInGame", false);
						FHoudiniEngineRuntimeUtils::SetTemplatePropertyValue(HoudiniSplineComponent, "bIsHoudiniSplineVisible", true);
						FHoudiniEngineRuntimeUtils::SetTemplatePropertyValue(HoudiniSplineComponent, "bHasChanged", true);
						FHoudiniEngineRuntimeUtils::SetTemplatePropertyValue(HoudiniSplineComponent, "bNeedsToTriggerUpdate", true);
#endif
					}
					else
					{
						// Attach the new Houdini spline component to it's owner
						AActor* OwningActor = HoudiniSplineComponent->GetOwner();
						check(OwningActor);
						USceneComponent* OuterComponent = Cast<USceneComponent>(GetOuter());
						HoudiniSplineComponent->RegisterComponent();
						HoudiniSplineComponent->AttachToComponent(OuterComponent, FAttachmentTransformRules::KeepRelativeTransform);
						HoudiniSplineComponent->SetHoudiniSplineVisible(true);
						HoudiniSplineComponent->SetHiddenInGame(false, true);
						HoudiniSplineComponent->SetVisibility(true, true);
						HoudiniSplineComponent->MarkChanged(true);
					
					}

					bOutBlueprintStructureModified = true;
				}
			}
		}
		break;

		case EHoudiniInputType::Geometry:
		{

		}
		break;

		case EHoudiniInputType::Landscape:
		{
			// Need to do anything on select?	
		}
		break;

		case EHoudiniInputType::Skeletal:
		{
		}
		break;
		
		case EHoudiniInputType::GeometryCollection:
		{
		}
		break;
		
		default:
		{
		}
		break;
	}
}

UHoudiniInputObject*
UHoudiniInput::CreateNewCurveInputObject(bool& bOutBlueprintStructureModified)
{
	if (CurveInputObjects.Num() > 0)
		return nullptr;

	UHoudiniInputHoudiniSplineComponent* NewCurveInputObject = CreateHoudiniSplineInput(nullptr, true, false, bOutBlueprintStructureModified);
	if (!IsValid(NewCurveInputObject))
		return nullptr;

	UHoudiniSplineComponent * HoudiniSplineComponent = NewCurveInputObject->GetCurveComponent();
	if (!IsValid(HoudiniSplineComponent))
		return nullptr;

    // Default Houdini spline component input should not be visible at initialization
	HoudiniSplineComponent->SetVisibility(true, true);
	HoudiniSplineComponent->SetHoudiniSplineVisible(true);
	HoudiniSplineComponent->SetHiddenInGame(false, true);

	CurveInputObjects.Add(NewCurveInputObject);
	SetInputObjectsNumber(EHoudiniInputType::Curve, 1);
	CurveInputObjects.SetNum(1);

	return NewCurveInputObject;
}

void
UHoudiniInput::MarkAllInputObjectsChanged(const bool& bInChanged)
{
	MarkDataUploadNeeded(bInChanged);

	// Mark all the objects from this input has changed so they upload themselves

	TSet<EHoudiniInputType> InputTypes;
	InputTypes.Add(Type);
	InputTypes.Add(EHoudiniInputType::Curve);
	
	TArray<UHoudiniInputObject*>* NewInputObjects = GetHoudiniInputObjectArray(Type);
	if (NewInputObjects)
	{
		for (auto CurInputObject : *NewInputObjects)
		{
			if (IsValid(CurInputObject))
				CurInputObject->MarkChanged(bInChanged);
		}
	}
}

UHoudiniInput * UHoudiniInput::DuplicateAndCopyState(UObject * DestOuter, bool bInCanDeleteHoudiniNodes)
{
	UHoudiniInput* NewInput = Cast<UHoudiniInput>(StaticDuplicateObject(this, DestOuter));

	NewInput->CopyStateFrom(this, false, bInCanDeleteHoudiniNodes);

	return NewInput;
}

void UHoudiniInput::CopyStateFrom(UHoudiniInput* InInput, bool bCopyAllProperties, bool bInCanDeleteHoudiniNodes)
{

	// Preserve the current input objects before the copy to ensure we don't lose 
	// access to input objects and have them end up in the garbage.
	
	TMap<EHoudiniInputType, TArray<UHoudiniInputObject*>*> PrevInputObjectsMap;

	for(EHoudiniInputType InputType : HoudiniInputTypeList)
	{
		PrevInputObjectsMap.Add(InputType, GetHoudiniInputObjectArray(InputType));
	}
	
	// TArray<UHoudiniInputObject*> PrevInputObjects;
	// TArray<UHoudiniInputObject*>* OldToInputObjects = GetHoudiniInputObjectArray(Type);
	// if (OldToInputObjects)
	// PrevInputObjects = *OldToInputObjects;

	// Copy the state of this UHoudiniInput object.
	if (bCopyAllProperties)
	{
		UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
		Params.bDoDelta = false; // Perform a deep copy
		Params.bClearReferences = false; // References will be replaced afterwards.
		UEngine::CopyPropertiesForUnrelatedObjects(InInput, this, Params);
	}

	AssetNodeId = InInput->AssetNodeId;
	InputNodeId = InInput->InputNodeId;
	ParmId = InInput->ParmId;
	bCanDeleteHoudiniNodes = bInCanDeleteHoudiniNodes;

	//if (bInCanDeleteHoudiniNodes)
	//{
	//	// Delete stale data nodes before they get overwritten.
	//	TSet<int32> NewNodeIds(InInput->CreatedDataNodeIds);
	//	for (int32 NodeId : CreatedDataNodeIds)
	//	{
	//		if (!NewNodeIds.Contains(NodeId))
	//		{
	//			FHoudiniEngineRuntime::Get().MarkNodeIdAsPendingDelete(NodeId);
	//		}
	//	}
	//}

	CreatedDataNodeIds = InInput->CreatedDataNodeIds;

	// Important note: At this point the new object may still share objects with InInput.
	// The CopyInputs() will properly duplicate inputs where necessary.

	// Copy states of Input Objects that correspond to the current type.

	for(auto& Entry : PrevInputObjectsMap)
	{
		EHoudiniInputType InputType = Entry.Key;
		TArray<UHoudiniInputObject*>* PrevInputObjects = Entry.Value;
		TArray<UHoudiniInputObject*>* ToInputObjects = GetHoudiniInputObjectArray(InputType);
		TArray<UHoudiniInputObject*>* FromInputObjects = InInput->GetHoudiniInputObjectArray(InputType);

		if (ToInputObjects && FromInputObjects)
		{
			*ToInputObjects = *PrevInputObjects;
			CopyInputs(*ToInputObjects, *FromInputObjects, bInCanDeleteHoudiniNodes);
		}
	}

}

void UHoudiniInput::SetCanDeleteHoudiniNodes(bool bInCanDeleteNodes)
{
	bCanDeleteHoudiniNodes = bInCanDeleteNodes;
	for(UHoudiniInputObject* InputObject : GeometryInputObjects)
	{
		if (!InputObject)
			continue;
		InputObject->SetCanDeleteHoudiniNodes(bInCanDeleteNodes);
	}

	for(UHoudiniInputObject* InputObject : AssetInputObjects)
	{
		if (!InputObject)
			continue;
		InputObject->SetCanDeleteHoudiniNodes(bInCanDeleteNodes);
	}

	for(UHoudiniInputObject* InputObject : CurveInputObjects)
	{
		if (!InputObject)
			continue;
		InputObject->SetCanDeleteHoudiniNodes(bInCanDeleteNodes);
	}

	for(UHoudiniInputObject* InputObject : LandscapeInputObjects)
	{
		if (!InputObject)
			continue;
		InputObject->SetCanDeleteHoudiniNodes(bInCanDeleteNodes);
	}

	for(UHoudiniInputObject* InputObject : WorldInputObjects)
	{
		if (!InputObject)
			continue;
		InputObject->SetCanDeleteHoudiniNodes(bInCanDeleteNodes);
	}

	for(UHoudiniInputObject* InputObject : GeometryCollectionInputObjects)
	{
		if (!InputObject)
			continue;
		InputObject->SetCanDeleteHoudiniNodes(bInCanDeleteNodes);
	}

}

void UHoudiniInput::InvalidateData()
{
	// If valid, mark our input node for deletion
	if (InputNodeId >= 0)
	{
		// .. but if we're an asset input, don't delete the node as InputNodeId
		// is set to the input HDA's node ID!
		if (Type != EHoudiniInputType::Asset)
		{
			if (bCanDeleteHoudiniNodes)
				MarkInputNodeAsPendingDelete();
		}
		
		InputNodeId = -1;
	}

	for(UHoudiniInputObject* InputObject : GeometryInputObjects)
	{
		if (!InputObject)
			continue;
		InputObject->InvalidateData();
	}

	for(UHoudiniInputObject* InputObject : AssetInputObjects)
	{
		if (!InputObject)
			continue;
		InputObject->InvalidateData();
	}

	for(UHoudiniInputObject* InputObject : CurveInputObjects)
	{
		if (!InputObject)
			continue;
		InputObject->InvalidateData();
	}

	for(UHoudiniInputObject* InputObject : LandscapeInputObjects)
	{
		if (!InputObject)
			continue;
		InputObject->InvalidateData();
	}

	for(UHoudiniInputObject* InputObject : WorldInputObjects)
	{
		if (!InputObject)
			continue;

		if (InputObject->IsA<UHoudiniInputHoudiniAsset>())
		{
			// When the input object is a HoudiniAssetComponent, 
			// we need to be sure that this HDA node id is not in CreatedDataNodeIds
			// We dont want to delete the input HDA node!
			CreatedDataNodeIds.Remove(InputObject->InputNodeId);
		}

		InputObject->InvalidateData();
	}

	for(UHoudiniInputObject* InputObject : GeometryCollectionInputObjects)
	{
		if (!InputObject)
			continue;
		InputObject->InvalidateData();
	}
	
	if (bCanDeleteHoudiniNodes)
	{
		auto& HoudiniEngineRuntime = FHoudiniEngineRuntime::Get();
		for(int32 NodeId : CreatedDataNodeIds)
		{
			HoudiniEngineRuntime.MarkNodeIdAsPendingDelete(NodeId, true);
		}
	}
	
	CreatedDataNodeIds.Empty();
}

void UHoudiniInput::CopyInputs(TArray<UHoudiniInputObject*>& ToInputObjects, TArray<UHoudiniInputObject*>& FromInputObjects, bool bInCanDeleteHoudiniNodes)
{
	TSet<UHoudiniInputObject*> StaleObjects(ToInputObjects);

	const int32 NumInputs = FromInputObjects.Num();
	UObject* TargetOuter = GetOuter();
	
	ToInputObjects.SetNum(NumInputs);


	for (int i = 0; i < NumInputs; i++)
	{
		UHoudiniInputObject* FromObject = FromInputObjects[i];
		UHoudiniInputObject* ToObject = ToInputObjects[i];

		if (!FromObject)
		{
			ToInputObjects[i] = nullptr;
			continue;
		}

		if (ToObject)
		{
			bool IsValid = true;
			// Is ToInput and FromInput the same or do we have to create a input object?
			IsValid = IsValid && ToObject->Matches(*FromObject);
			IsValid = IsValid && ToObject->GetOuter() == TargetOuter;

			if (!IsValid)
			{
				ToObject = nullptr;
			}
		}

		if (ToObject)
		{
			// We have an existing (matching) object. Copy the
			// state from the incoming input.
			StaleObjects.Remove(ToObject);
			ToObject->CopyStateFrom(FromObject, true);
		}
		else
		{
			// We need to create a new input here.
			ToObject = FromObject->DuplicateAndCopyState(TargetOuter);
			ToInputObjects[i] = ToObject;
		}

		ToObject->SetCanDeleteHoudiniNodes(bInCanDeleteHoudiniNodes);
	}


	for (UHoudiniInputObject* StaleInputObject : StaleObjects)
	{
		if (!StaleInputObject)
			continue;
		if (StaleInputObject->GetOuter() == this)
		{
			StaleInputObject->SetCanDeleteHoudiniNodes(bInCanDeleteHoudiniNodes);
		}
	}
}


UHoudiniInputHoudiniSplineComponent*
UHoudiniInput::CreateHoudiniSplineInput(UHoudiniInputHoudiniSplineComponent * FromHoudiniSplineInputComponent, const bool & bAttachToparent, const bool & bAppendToInputArray, bool& bOutBlueprintStructureModified)
{
	UHoudiniInputHoudiniSplineComponent* HoudiniSplineInput = nullptr;
	UHoudiniSplineComponent* HoudiniSplineComponent = nullptr;

	UObject* OuterObj = GetOuter();
	USceneComponent* OuterComp = Cast<USceneComponent>(GetOuter());
	bool bOuterIsTemplate = (OuterObj && OuterObj->IsTemplate());

	if (!FromHoudiniSplineInputComponent)
	{
		// NOTE: If we're inside the Blueprint editor, the outer here is going to the be HAC component template.
		check(OuterObj)
		
		// Create a default Houdini spline input if a null pointer is passed in.
		FName HoudiniSplineName = MakeUniqueObjectName(OuterComp, UHoudiniSplineComponent::StaticClass(), TEXT("Houdini Spline"));

		// Create a Houdini Input Object.
		UHoudiniInputObject * NewInputObject = UHoudiniInputHoudiniSplineComponent::Create(
			nullptr, OuterObj, HoudiniSplineName.ToString());

		if (!IsValid(NewInputObject))
			return nullptr;

		HoudiniSplineInput = Cast<UHoudiniInputHoudiniSplineComponent>(NewInputObject);
		if (!HoudiniSplineInput)
			return nullptr;

		HoudiniSplineComponent = NewObject<UHoudiniSplineComponent>(
			HoudiniSplineInput,	UHoudiniSplineComponent::StaticClass());

		if (!IsValid(HoudiniSplineComponent))
			return nullptr;

		HoudiniSplineInput->Update(HoudiniSplineComponent);

		HoudiniSplineComponent->SetHoudiniSplineName(HoudiniSplineName.ToString());
		HoudiniSplineComponent->SetFlags(RF_Transactional);

		// Set the default position of curve to avoid overlapping.
		HoudiniSplineComponent->SetOffset(DefaultCurveOffset);
		DefaultCurveOffset += 100.f;

		if (!bOuterIsTemplate)
		{ 
			HoudiniSplineComponent->RegisterComponent();

			// Attach the new Houdini spline component to it's owner.
			if (bAttachToparent)
				HoudiniSplineComponent->AttachToComponent(OuterComp, FAttachmentTransformRules::KeepRelativeTransform);
		}

		//push the new input object to the array for new type.
		if (bAppendToInputArray &&  Type == EHoudiniInputType::Curve)
			GetHoudiniInputObjectArray(Type)->Add(NewInputObject);

#if WITH_EDITOR
		if (bOuterIsTemplate)
		{
			UHoudiniAssetBlueprintComponent* HAB = Cast<UHoudiniAssetBlueprintComponent>(OuterObj);
			if (HAB)
			{
				UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(HAB->GetOuter());
				UBlueprint* Blueprint = Cast<UBlueprint>(BPGC->ClassGeneratedBy);
				if (Blueprint)
				{
					TArray<UActorComponent*> Components;
					Components.Add(HoudiniSplineComponent);

					USCS_Node* HABNode = HAB->FindSCSNodeForTemplateComponentInClassHierarchy(HAB);

					// NOTE: FAddComponentsToBlueprintParams was introduced in 4.26 so for the sake of 
					// backwards compatibility, manually determine which SCSNode was added instead of
					// relying on Params.OutNodes.
					FKismetEditorUtilities::FAddComponentsToBlueprintParams Params;
					Params.OptionalNewRootNode = HABNode;
					const TSet<USCS_Node*> PreviousSCSNodes(Blueprint->SimpleConstructionScript->GetAllNodes());

					FKismetEditorUtilities::AddComponentsToBlueprint(Blueprint, Components, Params);
					USCS_Node* NewNode = nullptr;
					const TSet<USCS_Node*> CurrentSCSNodes(Blueprint->SimpleConstructionScript->GetAllNodes());
					const TSet<USCS_Node*> AddedNodes = CurrentSCSNodes.Difference(PreviousSCSNodes);
					
					if (AddedNodes.Num() > 0)
					{
						// Record Input / SCS node mapping 
						USCS_Node* SCSNode = AddedNodes.Array()[0];
						HAB->AddInputObjectMapping(NewInputObject->GetInputGuid(), SCSNode->VariableGuid);
						SCSNode->ComponentTemplate->SetFlags(RF_Public | RF_ArchetypeObject | RF_DefaultSubObject);
					}

					Blueprint->Modify();
					bOutBlueprintStructureModified = true;
				}
			}
		}
#endif
	}
	else 
	{
		// Otherwise, get the Houdini spline, and Houdini spline input from the argument.
		HoudiniSplineInput = FromHoudiniSplineInputComponent;
		HoudiniSplineComponent = FromHoudiniSplineInputComponent->GetCurveComponent();

		if (!IsValid(HoudiniSplineComponent))
			return nullptr;

		// Attach the new Houdini spline component to it's owner.
		HoudiniSplineComponent->AttachToComponent(OuterComp, FAttachmentTransformRules::KeepRelativeTransform);
	}

	// Mark the created UHoudiniSplineComponent as an input, and set its InputObject.
	HoudiniSplineComponent->SetIsInputCurve(true);

	// HoudiniSplineComponent->SetInputObject(HoudiniSplineInput);

	// Set Houdini Spline Component bHasChanged and bNeedsToTrigerUpdate to true.
	HoudiniSplineComponent->MarkChanged(true);

	

	return HoudiniSplineInput;
}

void
UHoudiniInput::RemoveSplineFromInputObject(
	UHoudiniInputHoudiniSplineComponent* InHoudiniSplineInputObject,
	bool& bOutBlueprintStructureModified) const
{
	if (!InHoudiniSplineInputObject)
		return;

	UObject* OuterObj = GetOuter();
	const bool bOuterIsTemplate = OuterObj && OuterObj->IsTemplate();

	if (bOuterIsTemplate)
	{
#if WITH_EDITOR
		// Find the SCS node that corresponds to this input and remove it.
		UHoudiniAssetBlueprintComponent* HAB = Cast<UHoudiniAssetBlueprintComponent>(OuterObj);
		if (HAB)
		{
			const UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(HAB->GetOuter());
			UBlueprint* Blueprint = Cast<UBlueprint>(BPGC->ClassGeneratedBy);
			if (Blueprint)
			{
				USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
				check(SCS);
				FGuid SCSGuid;
				if (HAB->GetInputObjectSCSVariableGuid(InHoudiniSplineInputObject->Guid, SCSGuid))
				{
					// TODO: Move this SCS variable removal code to a reusable utility function. We're
					// going to need to reuse this in a few other places too.
					USCS_Node* SCSNode = SCS->FindSCSNodeByGuid(SCSGuid);
					if (SCSNode)
					{
						SCS->RemoveNodeAndPromoteChildren(SCSNode);
						SCSNode->SetOnNameChanged(FSCSNodeNameChanged());
						bOutBlueprintStructureModified = true;
						HAB->RemoveInputObjectSCSVariableGuid(InHoudiniSplineInputObject->Guid);

						if (SCSNode->ComponentTemplate != nullptr)
						{
							const FName TemplateName = SCSNode->ComponentTemplate->GetFName();
							const FString RemovedName = SCSNode->GetVariableName().ToString() + TEXT("_REMOVED_") + FGuid::NewGuid().ToString();

							SCSNode->ComponentTemplate->Modify();
							SCSNode->ComponentTemplate->Rename(*RemovedName, /*NewOuter =*/nullptr, REN_DontCreateRedirectors);

							TArray<UObject*> ArchetypeInstances;
							auto DestroyArchetypeInstances = [&ArchetypeInstances, &RemovedName](UActorComponent* ComponentTemplate)
							{
								ComponentTemplate->GetArchetypeInstances(ArchetypeInstances);
								for (UObject* ArchetypeInstance : ArchetypeInstances)
								{
									if (!ArchetypeInstance->HasAllFlags(RF_ArchetypeObject | RF_InheritableComponentTemplate))
									{
										CastChecked<UActorComponent>(ArchetypeInstance)->DestroyComponent();
										ArchetypeInstance->Rename(*RemovedName, nullptr, REN_DontCreateRedirectors);
									}
								}
							};

							DestroyArchetypeInstances(SCSNode->ComponentTemplate);
							
							if (Blueprint)
							{
								// Children need to have their inherited component template instance of the component renamed out of the way as well
								TArray<UClass*> ChildrenOfClass;
								GetDerivedClasses(Blueprint->GeneratedClass, ChildrenOfClass);

								for (UClass* ChildClass : ChildrenOfClass)
								{
									UBlueprintGeneratedClass* BPChildClass = CastChecked<UBlueprintGeneratedClass>(ChildClass);

									if (UActorComponent* Component = (UActorComponent*)FindObjectWithOuter(BPChildClass, UActorComponent::StaticClass(), TemplateName))
									{
										Component->Modify();
										Component->Rename(*RemovedName, /*NewOuter =*/nullptr, REN_DontCreateRedirectors);

										DestroyArchetypeInstances(Component);
									}
								}
							}
						}
					}
				} // if (HAB->GetInputObjectSCSVariableGuid(InHoudiniSplineInputObject->Guid, SCSGuid))
			} // if (Blueprint)
		}
#endif
	} // if (bIsOuterTemplate)
	else
	{
		UHoudiniSplineComponent* HoudiniSplineComponent = InHoudiniSplineInputObject->GetCurveComponent();
		if (HoudiniSplineComponent)
		{
			// detach the input curves from the asset component
			//FDetachmentTransformRules DetachTransRules(EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, false);
			//HoudiniSplineComponent->DetachFromComponent(DetachTransRules);
			// Destroy the Houdini Spline Component
			//InputObjectsPtr->RemoveAt(AtIndex);
			HoudiniSplineComponent->DestroyComponent();
		}
	}
	InHoudiniSplineInputObject->Update(nullptr);
}


TArray<UHoudiniInputObject*>*
UHoudiniInput::GetHoudiniInputObjectArray(const EHoudiniInputType& InType)
{
	switch (InType)
	{
	case EHoudiniInputType::Geometry:
		return &GeometryInputObjects;

	case EHoudiniInputType::Curve:
		return &CurveInputObjects;

	case EHoudiniInputType::Asset:
		return &AssetInputObjects;

	case EHoudiniInputType::Landscape:
		return &LandscapeInputObjects;

	case EHoudiniInputType::World:
		return &WorldInputObjects;

	case EHoudiniInputType::Skeletal:
		return &SkeletalInputObjects;

	case EHoudiniInputType::GeometryCollection:
		return &GeometryCollectionInputObjects;

	default:
	case EHoudiniInputType::Invalid:
		return nullptr;
	}

	return nullptr;
}

TArray<AActor*>*
UHoudiniInput::GetBoundSelectorObjectArray()
{
	return &WorldInputBoundSelectorObjects;
}

const TArray<AActor*>*
UHoudiniInput::GetBoundSelectorObjectArray() const
{
	return &WorldInputBoundSelectorObjects;
}

const TArray<UHoudiniInputObject*>*
UHoudiniInput::GetHoudiniInputObjectArray(const EHoudiniInputType& InType) const
{
	switch (InType)
	{
		case EHoudiniInputType::Geometry:
			return &GeometryInputObjects;

		case EHoudiniInputType::Curve:
			return &CurveInputObjects;

		case EHoudiniInputType::Asset:
			return &AssetInputObjects;

		case EHoudiniInputType::Landscape:
			return &LandscapeInputObjects;

		case EHoudiniInputType::World:
			return &WorldInputObjects;

		case EHoudiniInputType::Skeletal:
			return &SkeletalInputObjects;

		case EHoudiniInputType::GeometryCollection:
			return &GeometryCollectionInputObjects;

		default:
		case EHoudiniInputType::Invalid:
			return nullptr;
	}

	return nullptr;
}

UHoudiniInputObject*
UHoudiniInput::GetHoudiniInputObjectAt(const int32& AtIndex)
{
	return GetHoudiniInputObjectAt(Type, AtIndex);
}

const UHoudiniInputObject*
UHoudiniInput::GetHoudiniInputObjectAt(const int32& AtIndex) const
{
	const TArray<UHoudiniInputObject*>* InputObjectsArray = GetHoudiniInputObjectArray(Type);
	if (!InputObjectsArray || !InputObjectsArray->IsValidIndex(AtIndex))
		return nullptr;

	return (*InputObjectsArray)[AtIndex];
}

UHoudiniInputObject*
UHoudiniInput::GetHoudiniInputObjectAt(const EHoudiniInputType& InType, const int32& AtIndex)
{
	TArray<UHoudiniInputObject*>* InputObjectsArray = GetHoudiniInputObjectArray(InType);
	if (!InputObjectsArray || !InputObjectsArray->IsValidIndex(AtIndex))
		return nullptr;

	return (*InputObjectsArray)[AtIndex];
}

UObject*
UHoudiniInput::GetInputObjectAt(const int32& AtIndex)
{
	return GetInputObjectAt(Type, AtIndex);
}

AActor*
UHoudiniInput::GetBoundSelectorObjectAt(const int32& AtIndex)
{
	if (!WorldInputBoundSelectorObjects.IsValidIndex(AtIndex))
		return nullptr;

	return WorldInputBoundSelectorObjects[AtIndex];
}

UObject*
UHoudiniInput::GetInputObjectAt(const EHoudiniInputType& InType, const int32& AtIndex)
{
	UHoudiniInputObject* HoudiniInputObject = GetHoudiniInputObjectAt(InType, AtIndex);
	if (!IsValid(HoudiniInputObject))
		return nullptr;

	return HoudiniInputObject->GetObject();
}

void
UHoudiniInput::InsertInputObjectAt(const int32& AtIndex)
{
	InsertInputObjectAt(Type, AtIndex);
}

void
UHoudiniInput::InsertInputObjectAt(const EHoudiniInputType& InType, const int32& AtIndex)
{
	TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(InType);
	if (!InputObjectsPtr)
		return;

	InputObjectsPtr->InsertDefaulted(AtIndex, 1);
	MarkChanged(true);
}

void
UHoudiniInput::DeleteInputObjectAt(const int32& AtIndex, const bool bInRemoveIndexFromArray)
{
	DeleteInputObjectAt(Type, AtIndex, bInRemoveIndexFromArray);
}

void
UHoudiniInput::DeleteInputObjectAt(const EHoudiniInputType& InType, const int32& AtIndex, const bool bInRemoveIndexFromArray)
{
	TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(InType);
	if (!InputObjectsPtr)
		return;

	if (!InputObjectsPtr->IsValidIndex(AtIndex))
		return;

	bool bBlueprintStructureModified = false;

	if (Type == EHoudiniInputType::Asset)
	{
		// ... TODO operations for removing asset input type
	}
	else if (Type == EHoudiniInputType::Curve)
	{
		UHoudiniInputHoudiniSplineComponent* HoudiniSplineInputObject = Cast<UHoudiniInputHoudiniSplineComponent>((*InputObjectsPtr)[AtIndex]);
		if (HoudiniSplineInputObject)
		{
			RemoveSplineFromInputObject(HoudiniSplineInputObject, bBlueprintStructureModified);
		}
	}
	else if (Type == EHoudiniInputType::Geometry) 
	{
		// ... TODO operations for removing geometry input type
	}
	else if (Type == EHoudiniInputType::Landscape) 
	{
		// ... TODO operations for removing landscape input type
	}
	else if (Type == EHoudiniInputType::Skeletal) 
	{
		// ... TODO operations for removing skeletal input type
	}
	else if (Type == EHoudiniInputType::GeometryCollection) 
	{
		// ... TODO operations for removing geometrycollection input type
	}
	else if (Type == EHoudiniInputType::World) 
	{
		// ... TODO operations for removing world input type
	}
	else 
	{
		// ... invalid input type
	}

	MarkChanged(true);

	UHoudiniInputObject* InputObjectToDelete = (*InputObjectsPtr)[AtIndex];
	if (IsValid(InputObjectToDelete))
	{		
		// Mark the input object's nodes for deletion
		InputObjectToDelete->InvalidateData();

		// If the deleted object wasnt null, trigger a re upload of the input data
		MarkDataUploadNeeded(true);
	}

	if (bInRemoveIndexFromArray)
	{
		InputObjectsPtr->RemoveAt(AtIndex);

		// Delete the merge node when all the input objects are deleted.
		if (InputObjectsPtr->Num() == 0 && InputNodeId >= 0)
		{
			if (bCanDeleteHoudiniNodes)
				MarkInputNodeAsPendingDelete();
			InputNodeId = -1;
		}
	}
	else
	{
		(*InputObjectsPtr)[AtIndex] = nullptr;
	}

#if WITH_EDITOR
	if (bBlueprintStructureModified)
	{
		UActorComponent* Component = Cast<UActorComponent>(GetOuter());
		FHoudiniEngineRuntimeUtils::MarkBlueprintAsStructurallyModified(Component);
	}
#endif
}

void
UHoudiniInput::DuplicateInputObjectAt(const int32& AtIndex)
{
	DuplicateInputObjectAt(Type, AtIndex);
}

void
UHoudiniInput::DuplicateInputObjectAt(const EHoudiniInputType& InType, const int32& AtIndex)
{
	TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(InType);
	if (!InputObjectsPtr)
		return;

	if (!InputObjectsPtr->IsValidIndex(AtIndex))
		return;

	// If the duplicated object is not null, trigger a re upload of the input data
	bool bTriggerUpload = (*InputObjectsPtr)[AtIndex] != nullptr;

	// TODO: Duplicate the UHoudiniInputObject!!
	UHoudiniInputObject* DuplicateInput = (*InputObjectsPtr)[AtIndex];
	InputObjectsPtr->Insert(DuplicateInput, AtIndex);

	MarkChanged(true);

	if (bTriggerUpload)
		MarkDataUploadNeeded(true);
}

int32
UHoudiniInput::GetNumberOfInputObjects()
{
	return GetNumberOfInputObjects(Type);
}

int32
UHoudiniInput::GetNumberOfInputObjects(const EHoudiniInputType& InType)
{
	TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(InType);
	if (!InputObjectsPtr)
		return 0;

	return InputObjectsPtr->Num();
}

int32
UHoudiniInput::GetNumberOfInputMeshes()
{
	return GetNumberOfInputMeshes(Type);
}

int32
UHoudiniInput::GetNumberOfInputMeshes(const EHoudiniInputType& InType)
{
	TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(InType);
	if (!InputObjectsPtr)
		return 0;

	// TODO?
	// If geometry input, and we only have one null object, return 0
	int32 Num = InputObjectsPtr->Num();

	// TODO: Fix BP properly!
	// Special case for SM in BP:
	// we need to add extra input objects store in BlueprintStaticMeshes
	// Same thing for Actor InputObjects!
	for (auto InputObj : *InputObjectsPtr)
	{
		if (!IsValid(InputObj))
			continue;

		UHoudiniInputStaticMesh* InputSM = Cast<UHoudiniInputStaticMesh>(InputObj);
		if (IsValid(InputSM))
		{
			if (InputSM->BlueprintStaticMeshes.Num() > 0)
			{
				Num += (InputSM->BlueprintStaticMeshes.Num() - 1);
			}
		}

		UHoudiniInputActor* InputActor = Cast<UHoudiniInputActor>(InputObj);
		if (IsValid(InputActor))
		{
			if (InputActor->GetActorComponents().Num() > 0)
			{
				Num += (InputActor->GetActorComponents().Num() - 1);
			}
		}
	}

	return Num;
}


int32
UHoudiniInput::GetNumberOfBoundSelectorObjects() const
{
	return WorldInputBoundSelectorObjects.Num();
}

void
UHoudiniInput::SetInputObjectAt(const int32& AtIndex, UObject* InObject)
{
	return SetInputObjectAt(Type, AtIndex, InObject);
}

void
UHoudiniInput::SetInputObjectAt(const EHoudiniInputType& InType, const int32& AtIndex, UObject* InObject)
{
	// Start by making sure we have the proper number of input objects
	int32 NumIntObject = GetNumberOfInputObjects(InType);
	if (NumIntObject <= AtIndex)
	{
		// We need to resize the array
		SetInputObjectsNumber(InType, AtIndex + 1);
	}

	UObject* CurrentInputObject = GetInputObjectAt(InType, AtIndex);
	if (CurrentInputObject == InObject)
	{
		// Nothing to do
		return;
	}

	UHoudiniInputObject* CurrentInputObjectWrapper = GetHoudiniInputObjectAt(InType, AtIndex);
	if (!InObject)
	{
		// We want to set the input object to null
		TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(InType);		
		if (!ensure(InputObjectsPtr != nullptr && InputObjectsPtr->IsValidIndex(AtIndex)))
			return;

		if (CurrentInputObjectWrapper)
		{
			// TODO: Check this case
			// Do not destroy the input object manually! this messes up GC
			//CurrentInputObjectWrapper->ConditionalBeginDestroy();
			MarkDataUploadNeeded(true);
		}

		(*InputObjectsPtr)[AtIndex] = nullptr;
		return;
	}

	// Get the type of the previous and new input objects
	EHoudiniInputObjectType NewObjectType = InObject ? UHoudiniInputObject::GetInputObjectTypeFromObject(InObject) : EHoudiniInputObjectType::Invalid;
	EHoudiniInputObjectType CurrentObjectType = CurrentInputObjectWrapper ? CurrentInputObjectWrapper->Type : EHoudiniInputObjectType::Invalid;

	// See if we can reuse the existing InputObject		
	if (CurrentObjectType == NewObjectType && NewObjectType != EHoudiniInputObjectType::Invalid)
	{
		// The InputObjectTypes match, we can just update the existing object
		CurrentInputObjectWrapper->Update(InObject);
		CurrentInputObjectWrapper->MarkChanged(true);
		return;
	}

	// Destroy the existing input object
	TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(InType);
	if (!ensure(InputObjectsPtr))
		return;

	UHoudiniInputObject* NewInputObject = UHoudiniInputObject::CreateTypedInputObject(InObject, this, FString::FromInt(AtIndex + 1));
	if (!ensure(NewInputObject))
		return;

	// Mark that input object as changed so we know we need to update it
	NewInputObject->MarkChanged(true);
	if (IsValid(CurrentInputObjectWrapper))
	{
		// TODO:
		// For some input type, we may have to copy some of the previous object's property before deleting it

		// Delete the previous object
		CurrentInputObjectWrapper->MarkAsGarbage();
		(*InputObjectsPtr)[AtIndex] = nullptr;
	}

	// Update the input object array with the newly created input object
	(*InputObjectsPtr)[AtIndex] = NewInputObject;
}

void
UHoudiniInput::SetInputObjectsNumber(const EHoudiniInputType& InType, const int32& InNewCount)
{
	TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(InType);
	if (!InputObjectsPtr)
		return;

	if (InputObjectsPtr->Num() == InNewCount)
	{
		// Nothing to do
		return;
	}

	if (InNewCount > InputObjectsPtr->Num())
	{
		// Simply add new default InputObjects
		InputObjectsPtr->SetNum(InNewCount);
	}
	else
	{
		// TODO: Check this case!
		// Do not destroy the input object themselves manually,
		// destroy the input object's nodes and reduce the array's size  
		for (int32 InObjIdx = InputObjectsPtr->Num() - 1; InObjIdx >= InNewCount; InObjIdx--)
		{
			UHoudiniInputObject* CurrentInputObject = (*InputObjectsPtr)[InObjIdx];
			if (!IsValid(CurrentInputObject))
				continue;

			if (bCanDeleteHoudiniNodes)
				CurrentInputObject->InvalidateData();

			/*/
			//FHoudiniInputTranslator::DestroyInput(Inputs[InputIdx]);
			CurrentObject->ConditionalBeginDestroy();
			(*InputObjectsPtr)[InObjIdx] = nullptr;
			*/
		}
		
		// Decrease the input object array size
		InputObjectsPtr->SetNum(InNewCount);
	}

	// Also delete the input's merge node when all the input objects are deleted.
	if (InNewCount == 0 && InputNodeId >= 0)
	{
		if (bCanDeleteHoudiniNodes)
			MarkInputNodeAsPendingDelete();
		InputNodeId = -1;
	}
}

void
UHoudiniInput::SetBoundSelectorObjectsNumber(const int32& InNewCount)
{
	if (WorldInputBoundSelectorObjects.Num() == InNewCount)
	{
		// Nothing to do
		return;
	}

	if (InNewCount > WorldInputBoundSelectorObjects.Num())
	{
		// Simply add new default InputObjects
		WorldInputBoundSelectorObjects.SetNum(InNewCount);
	}
	else
	{
		/*
		// TODO: Not Needed?
		// Do not destroy the input object themselves manually,
		// destroy the input object's nodes and reduce the array's size  
		for (int32 InObjIdx = WorldInputBoundSelectorObjects.Num() - 1; InObjIdx >= InNewCount; InObjIdx--)
		{
			UHoudiniInputObject* CurrentInputObject = WorldInputBoundSelectorObjects[InObjIdx];
			if (!CurrentInputObject)
				continue;

			CurrentInputObject->MarkInputNodesForDeletion();
		}
		*/

		// Decrease the input object array size
		WorldInputBoundSelectorObjects.SetNum(InNewCount);
	}
}

void
UHoudiniInput::SetBoundSelectorObjectAt(const int32& AtIndex, AActor* InActor)
{
	// Start by making sure we have the proper number of objects
	int32 NumIntObject = GetNumberOfBoundSelectorObjects();
	if (NumIntObject <= AtIndex)
	{
		// We need to resize the array
		SetBoundSelectorObjectsNumber(AtIndex + 1);
	}

	AActor* CurrentActor = GetBoundSelectorObjectAt(AtIndex);
	if (CurrentActor == InActor)
	{
		// Nothing to do
		return;
	}

	// Update the array with the new object
	WorldInputBoundSelectorObjects[AtIndex] = InActor;
}

// Helper function indicating what classes are supported by an input type
TArray<const UClass*>
UHoudiniInput::GetAllowedClasses(const EHoudiniInputType& InInputType)
{
	TArray<const UClass*> AllowedClasses;
	switch (InInputType)
	{
		case EHoudiniInputType::Geometry:
			AllowedClasses.Add(UStaticMesh::StaticClass());
			AllowedClasses.Add(USkeletalMesh::StaticClass());
			AllowedClasses.Add(UBlueprint::StaticClass());
			AllowedClasses.Add(UDataTable::StaticClass());
			AllowedClasses.Add(UFoliageType_InstancedStaticMesh::StaticClass());
			break;

		case EHoudiniInputType::Curve:
			AllowedClasses.Add(USplineComponent::StaticClass());
			AllowedClasses.Add(UHoudiniSplineComponent::StaticClass());
			break;

		case EHoudiniInputType::Asset:
			AllowedClasses.Add(UHoudiniAssetComponent::StaticClass());
			break;

		case EHoudiniInputType::Landscape:
			AllowedClasses.Add(ALandscapeProxy::StaticClass());
			break;

		case EHoudiniInputType::World:
			AllowedClasses.Add(AActor::StaticClass());
			break;

		case EHoudiniInputType::Skeletal:
			AllowedClasses.Add(USkeletalMesh::StaticClass());
			break;
		case EHoudiniInputType::GeometryCollection:
			AllowedClasses.Add(UGeometryCollection::StaticClass());
			AllowedClasses.Add(UGeometryCollectionComponent::StaticClass());
			AllowedClasses.Add(AGeometryCollectionActor::StaticClass());
			break;
		default:
			break;
	}

	return AllowedClasses;
}

// Helper function indicating if an object is supported by an input type
bool 
UHoudiniInput::IsObjectAcceptable(const EHoudiniInputType& InInputType, const UObject* InObject)
{
	TArray<const UClass*> AllowedClasses = GetAllowedClasses(InInputType);
	for (auto CurClass : AllowedClasses)
	{
		if (InObject->IsA(CurClass))
			return true;
	}

	return false;
}

bool
UHoudiniInput::IsDataUploadNeeded()
{
	if (bDataUploadNeeded)
		return true;

	return HasChanged();
}

// Indicates if this input has changed and should be updated
bool 
UHoudiniInput::HasChanged()
{
	if (bHasChanged)
		return true;

	TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(Type);
	if (!ensure(InputObjectsPtr))
		return false;

	for (auto CurrentInputObject : (*InputObjectsPtr))
	{
		if (CurrentInputObject && CurrentInputObject->HasChanged())
			return true;
	}

	return false;
}

bool
UHoudiniInput::IsTransformUploadNeeded()
{
	TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(Type);
	if (!ensure(InputObjectsPtr))
		return false;

	for (auto CurrentInputObject : (*InputObjectsPtr))
	{
		if (CurrentInputObject && CurrentInputObject->HasTransformChanged())
			return true;
	}

	return false;
}

// Indicates if this input needs to trigger an update
bool
UHoudiniInput::NeedsToTriggerUpdate()
{
	if (bNeedsToTriggerUpdate)
		return true;

	const TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(Type);
	if (!ensure(InputObjectsPtr))
		return false;

	for (auto CurrentInputObject : (*InputObjectsPtr))
	{
		if (CurrentInputObject && CurrentInputObject->NeedsToTriggerUpdate())
			return true;
	}

	return false;
}

FString 
UHoudiniInput::GetNodeBaseName() const
{
	UHoudiniAssetComponent* HAC = Cast<UHoudiniAssetComponent>(GetOuter());
	FString NodeBaseName = HAC ? HAC->GetDisplayName() : TEXT("HoudiniAsset");

	// Unfortunately CreateInputNode always prefix with input_...
	if (IsObjectPathParameter())
		NodeBaseName += TEXT("_") + GetName();
	else
		NodeBaseName += TEXT("_input") + FString::FromInt(GetInputIndex());

	return NodeBaseName;
}

void
UHoudiniInput::OnTransformUIExpand(const int32& AtIndex)
{
#if WITH_EDITORONLY_DATA
	if (TransformUIExpanded.IsValidIndex(AtIndex))
	{
		TransformUIExpanded[AtIndex] = !TransformUIExpanded[AtIndex];
	}
	else
	{
		// We need to append values to the expanded array
		for (int32 Index = TransformUIExpanded.Num(); Index <= AtIndex; Index++)
		{
			TransformUIExpanded.Add(Index == AtIndex ? true : false);
		}
	}
#endif
}

bool
UHoudiniInput::IsTransformUIExpanded(const int32& AtIndex)
{
#if WITH_EDITORONLY_DATA
	return TransformUIExpanded.IsValidIndex(AtIndex) ? TransformUIExpanded[AtIndex] : false;
#else
	return false;
#endif
};

FTransform*
UHoudiniInput::GetTransformOffset(const int32& AtIndex) 
{
	UHoudiniInputObject* InObject = GetHoudiniInputObjectAt(AtIndex);
	if (InObject)
		return &(InObject->Transform);

	return nullptr;
}

const FTransform
UHoudiniInput::GetTransformOffset(const int32& AtIndex) const
{
	const UHoudiniInputObject* InObject = GetHoudiniInputObjectAt(AtIndex);
	if (InObject)
		return InObject->Transform;

	return FTransform::Identity;
}

TOptional<float>
UHoudiniInput::GetPositionOffsetX(int32 AtIndex) const
{
	return GetTransformOffset(AtIndex).GetLocation().X;
}

TOptional<float>
UHoudiniInput::GetPositionOffsetY(int32 AtIndex) const
{
	return GetTransformOffset(AtIndex).GetLocation().Y;
}

TOptional<float>
UHoudiniInput::GetPositionOffsetZ(int32 AtIndex) const
{
	return GetTransformOffset(AtIndex).GetLocation().Z;
}

TOptional<float>
UHoudiniInput::GetRotationOffsetRoll(int32 AtIndex) const
{
	return GetTransformOffset(AtIndex).Rotator().Roll;
}

TOptional<float>
UHoudiniInput::GetRotationOffsetPitch(int32 AtIndex) const
{
	return GetTransformOffset(AtIndex).Rotator().Pitch;
}

TOptional<float>
UHoudiniInput::GetRotationOffsetYaw(int32 AtIndex) const
{
	return GetTransformOffset(AtIndex).Rotator().Yaw;
}

TOptional<float>
UHoudiniInput::GetScaleOffsetX(int32 AtIndex) const
{
	return GetTransformOffset(AtIndex).GetScale3D().X;
}

TOptional<float>
UHoudiniInput::GetScaleOffsetY(int32 AtIndex) const
{
	return GetTransformOffset(AtIndex).GetScale3D().Y;
}

TOptional<float>
UHoudiniInput::GetScaleOffsetZ(int32 AtIndex) const
{
	return GetTransformOffset(AtIndex).GetScale3D().Z;
}

bool
UHoudiniInput::SetTransformOffsetAt(const float& Value, const int32& AtIndex, const int32& PosRotScaleIndex, const int32& XYZIndex)
{
	FTransform* Transform = GetTransformOffset(AtIndex);
	if (!Transform)
		return false;

	if (PosRotScaleIndex == 0)
	{
		FVector Position = Transform->GetLocation();
		if (Position[XYZIndex] == Value)
			return false;
		Position[XYZIndex] = Value;
		Transform->SetLocation(Position);
	}
	else if (PosRotScaleIndex == 1)
	{
		FRotator Rotator = Transform->Rotator();
		switch (XYZIndex)
		{
			case 0:
			{
				if (Rotator.Roll == Value)
					return false;
				Rotator.Roll = Value;
				break;
			}

			case 1:
			{
				if (Rotator.Pitch == Value)
					return false;
				Rotator.Pitch = Value;
				break;
			}

			case 2:
			{
				if (Rotator.Yaw == Value)
					return false;
				Rotator.Yaw = Value;
				break;
			}
		}
		Transform->SetRotation(Rotator.Quaternion());
	}
	else if (PosRotScaleIndex == 2)
	{
		FVector Scale = Transform->GetScale3D();
		if (Scale[XYZIndex] == Value)
			return false;

		Scale[XYZIndex] = Value;
		Transform->SetScale3D(Scale);
	}

	MarkChanged(true);
	bStaticMeshChanged = true;

	return true;
}

void
UHoudiniInput::SetAddRotAndScaleAttributes(const bool& InValue)
{
	if (bAddRotAndScaleAttributesOnCurves == InValue)
		return;

	bAddRotAndScaleAttributesOnCurves = InValue;

	// Mark all input obj as changed
	MarkAllInputObjectsChanged(true);
}

void
UHoudiniInput::SetUseLegacyInputCurve(const bool& InValue)
{
	if (bUseLegacyInputCurves == InValue)
		return;

	bUseLegacyInputCurves = InValue;

	// Mark all input obj as changed
	MarkAllInputObjectsChanged(true);
}

#if WITH_EDITOR
FText 
UHoudiniInput::GetCurrentSelectionText() const 
{
	FText CurrentSelectionText;
	switch (Type) 
	{
		case EHoudiniInputType::Landscape :
		{
			if (LandscapeInputObjects.Num() > 0) 
			{
				UHoudiniInputObject* InputObject = LandscapeInputObjects[0];

				UHoudiniInputLandscape* InputLandscape = Cast<UHoudiniInputLandscape>(InputObject);
				if (!IsValid(InputLandscape))
					return CurrentSelectionText;

				ALandscapeProxy* LandscapeProxy = InputLandscape->GetLandscapeProxy();
				if (!IsValid(LandscapeProxy))
					return CurrentSelectionText;

				CurrentSelectionText = FText::FromString(LandscapeProxy->GetActorLabel());
			}	
		}
		break;

		case EHoudiniInputType::Asset :
		{
			if (AssetInputObjects.Num() > 0) 
			{
				UHoudiniInputObject* InputObject = AssetInputObjects[0];

				UHoudiniInputHoudiniAsset* HoudiniAssetInput = Cast<UHoudiniInputHoudiniAsset>(InputObject);
				if (!IsValid(HoudiniAssetInput))
					return CurrentSelectionText;

				UHoudiniAssetComponent* HAC = HoudiniAssetInput->GetHoudiniAssetComponent();
				if (!IsValid(HAC))
					return CurrentSelectionText;

				UHoudiniAsset* HoudiniAsset = HAC->GetHoudiniAsset();
				if (!IsValid(HoudiniAsset))
					return CurrentSelectionText;

				CurrentSelectionText = FText::FromString(HoudiniAsset->GetName());
			}
		}
		break;

		default:
		break;
	}
	
	return CurrentSelectionText;
}
#endif

bool 
UHoudiniInput::HasLandscapeExportTypeChanged () const 
{
	if (Type != EHoudiniInputType::Landscape)
		return false;

	return bLandscapeHasExportTypeChanged;
}

void 
UHoudiniInput::SetHasLandscapeExportTypeChanged(const bool InChanged) 
{
	if (Type != EHoudiniInputType::Landscape)
		return;

	bLandscapeHasExportTypeChanged = InChanged;
}

bool 
UHoudiniInput::GetUpdateInputLandscape() const 
{
	if (Type != EHoudiniInputType::Landscape)
		return false;

	return bUpdateInputLandscape;
}

void 
UHoudiniInput::SetUpdateInputLandscape(const bool bInUpdateInputLandcape)
{
	if (Type != EHoudiniInputType::Landscape)
		return;

	bUpdateInputLandscape = bInUpdateInputLandcape;
}


bool
UHoudiniInput::UpdateWorldSelectionFromBoundSelectors()
{
	// Dont do anything if we're not a World Input
	if (Type != EHoudiniInputType::World)
		return false;

	// Build an array of the current selection's bounds
	TArray<FBox> AllBBox;
	for (auto CurrentActor : WorldInputBoundSelectorObjects)
	{
		if (!IsValid(CurrentActor))
			continue;

		AllBBox.Add(CurrentActor->GetComponentsBoundingBox(true, true));
	}

	//
	// Select all actors in our bound selectors bounding boxes
	//

	// Get our parent component/actor
	USceneComponent* ParentComponent = Cast<USceneComponent>(GetOuter());
	AActor* ParentActor = ParentComponent ? ParentComponent->GetOwner() : nullptr;

	//UWorld* editorWorld = GEditor->GetEditorWorldContext().World();
	UWorld* MyWorld = GetWorld();
	TArray<AActor*> NewSelectedActors;
	for (TActorIterator<AActor> ActorItr(MyWorld); ActorItr; ++ActorItr)
	{
		AActor *CurrentActor = *ActorItr;
		if (!IsValid(CurrentActor))
			continue;

		// Check that actor is currently not selected
		if (WorldInputBoundSelectorObjects.Contains(CurrentActor))
			continue;

		// Ignore the SkySpheres?
		FString ClassName = CurrentActor->GetClass() ? CurrentActor->GetClass()->GetName() : FString();
		if (ClassName.Contains("BP_Sky_Sphere"))
			continue;

		// Don't allow selection of ourselves. Bad things happen if we do.
		if (ParentActor && (CurrentActor == ParentActor))
			continue;

		// For BrushActors, both the actor and its brush must be valid
		ABrush* BrushActor = Cast<ABrush>(CurrentActor);
		if (BrushActor)
		{
			if (!IsValid(BrushActor->Brush))
				continue;
		}

		FBox ActorBounds = CurrentActor->GetComponentsBoundingBox(true);
		for (auto InBounds : AllBBox)
		{
			// Check if both actor's bounds intersects
			if (!ActorBounds.Intersect(InBounds))
				continue;

			NewSelectedActors.Add(CurrentActor);
			break;
		}
	}
	
	return UpdateWorldSelection(NewSelectedActors);
}

bool
UHoudiniInput::UpdateWorldSelection(const TArray<AActor*>& InNewSelection)
{
	TArray<AActor*> NewSelectedActors = InNewSelection;

	// Update our current selection with the new one
	// Keep actors that are still selected, remove the one that are not selected anymore
	bool bHasSelectionChanged = false;
	for (int32 Idx = WorldInputObjects.Num() - 1; Idx >= 0; Idx--)
	{
		UHoudiniInputActor* InputActor = Cast<UHoudiniInputActor>(WorldInputObjects[Idx]);
		AActor* CurActor = InputActor ? InputActor->GetActor() : nullptr;

		if (CurActor && NewSelectedActors.Contains(CurActor))
		{
			// The actor is still selected, remove it from the new selection
			NewSelectedActors.Remove(CurActor);
		}
		else
		{
			// That actor is no longer selected, remove itr from our current selection
			DeleteInputObjectAt(EHoudiniInputType::World, Idx);
			bHasSelectionChanged = true;
		}
	}

	if (NewSelectedActors.Num() > 0)
		bHasSelectionChanged = true;

	// Then add the newly selected Actors
	int32 InputObjectIdx = GetNumberOfInputObjects(EHoudiniInputType::World);
	int32 NewInputObjectNumber = InputObjectIdx + NewSelectedActors.Num();
	SetInputObjectsNumber(EHoudiniInputType::World, NewInputObjectNumber);
	for (const auto& CurActor : NewSelectedActors)
	{
		// Update the input objects from the valid selected actors array
		SetInputObjectAt(InputObjectIdx++, CurActor);
	}

	MarkChanged(bHasSelectionChanged);

	return bHasSelectionChanged;
}


bool
UHoudiniInput::ContainsInputObject(const UObject* InObject, const EHoudiniInputType& InType) const
{
	if (!IsValid(InObject))
		return false;

	// Returns true if the object is one of our input object for the given type
	const TArray<UHoudiniInputObject*>* ObjectArray = GetHoudiniInputObjectArray(InType);
	if (!ObjectArray)
		return false;

	for (auto& CurrentInputObject : (*ObjectArray))
	{
		if (!IsValid(CurrentInputObject))
			continue;

		if (CurrentInputObject->GetObject() == InObject)
			return true;
	}

	return false;
}

void UHoudiniInput::ForAllHoudiniInputObjects(TFunctionRef<void(UHoudiniInputObject*)> Fn, const bool bFilterByInputType) const
{
	auto ShouldIncludeFn = [&](const EHoudiniInputType InInputType) -> bool
	{
		return !bFilterByInputType || (bFilterByInputType && GetInputType() == InInputType);
	};
	
	if (ShouldIncludeFn(EHoudiniInputType::Geometry))
	{
		for(UHoudiniInputObject* InputObject : GeometryInputObjects)
		{
			Fn(InputObject);
		}
	}

	if (ShouldIncludeFn(EHoudiniInputType::Asset))
	{
		for(UHoudiniInputObject* InputObject : AssetInputObjects)
		{
			Fn(InputObject);
		}
	}

	if (ShouldIncludeFn(EHoudiniInputType::Curve))
	{
		for(UHoudiniInputObject* InputObject : CurveInputObjects)
		{
			Fn(InputObject);
		}
	}

	if (ShouldIncludeFn(EHoudiniInputType::Landscape))
	{
		for(UHoudiniInputObject* InputObject : LandscapeInputObjects)
		{
			Fn(InputObject);
		}
	}

	if (ShouldIncludeFn(EHoudiniInputType::World))
	{
		for(UHoudiniInputObject* InputObject : WorldInputObjects)
		{
			Fn(InputObject);
		}
	}

	if (ShouldIncludeFn(EHoudiniInputType::Skeletal))
	{
		for(UHoudiniInputObject* InputObject : SkeletalInputObjects)
		{
			Fn(InputObject);
		}
	}

	if (ShouldIncludeFn(EHoudiniInputType::GeometryCollection))
	{
		for(UHoudiniInputObject* InputObject : GeometryCollectionInputObjects)
		{
			Fn(InputObject);
		}
	}
}

TArray<const TArray<UHoudiniInputObject*>*> UHoudiniInput::GetAllObjectArrays() const
{
	return { &GeometryInputObjects, &CurveInputObjects, &WorldInputObjects, &SkeletalInputObjects, &LandscapeInputObjects, &AssetInputObjects, &GeometryCollectionInputObjects };
}

TArray<TArray<UHoudiniInputObject*>*> UHoudiniInput::GetAllObjectArrays()
{
	return { &GeometryInputObjects, &CurveInputObjects, &WorldInputObjects, &SkeletalInputObjects, &LandscapeInputObjects, &AssetInputObjects, &GeometryCollectionInputObjects };
}

void UHoudiniInput::ForAllHoudiniInputObjectArrays(TFunctionRef<void(const TArray<UHoudiniInputObject*>&)> Fn) const
{
	TArray<const TArray<UHoudiniInputObject*>*> ObjectArrays = GetAllObjectArrays();
	for (const TArray<UHoudiniInputObject*>* ObjectArrayPtr : ObjectArrays)
	{
		if (!ObjectArrayPtr)
			continue;
		Fn(*ObjectArrayPtr);
	}
}

void UHoudiniInput::ForAllHoudiniInputObjectArrays(TFunctionRef<void(TArray<UHoudiniInputObject*>&)> Fn)
{
	TArray<TArray<UHoudiniInputObject*>*> ObjectArrays = GetAllObjectArrays();
	for (TArray<UHoudiniInputObject*>* ObjectArrayPtr : ObjectArrays)
	{
		if (!ObjectArrayPtr)
			continue;
		Fn(*ObjectArrayPtr);
	}
}

void UHoudiniInput::GetAllHoudiniInputObjects(TArray<UHoudiniInputObject*>& OutObjects) const
{
	OutObjects.Empty();
	auto AddInputObject = [&OutObjects](UHoudiniInputObject* InputObject)
	{
		if (InputObject)
			OutObjects.Add(InputObject);
	};
	ForAllHoudiniInputObjects(AddInputObject);
}

void UHoudiniInput::ForAllHoudiniInputSceneComponents(TFunctionRef<void(UHoudiniInputSceneComponent*)> Fn) const
{
	auto ProcessSceneComponent = [Fn](UHoudiniInputObject* InputObject)
	{
		if (!InputObject)
			return;
		UHoudiniInputSceneComponent* SceneComponentInput = Cast<UHoudiniInputSceneComponent>(InputObject);
		if (!SceneComponentInput)
			return;
		Fn(SceneComponentInput);
	};
	ForAllHoudiniInputObjects(ProcessSceneComponent);
}

void UHoudiniInput::GetAllHoudiniInputSceneComponents(TArray<UHoudiniInputSceneComponent*>& OutObjects) const
{
	OutObjects.Empty();
	auto AddSceneComponent = [&OutObjects](UHoudiniInputObject* InputObject)
	{
		if (!InputObject)
			return;
		UHoudiniInputSceneComponent* SceneComponentInput = Cast<UHoudiniInputSceneComponent>(InputObject);
		if (!SceneComponentInput)
			return;
		OutObjects.Add(SceneComponentInput);
	};
	ForAllHoudiniInputObjects(AddSceneComponent);
}

void UHoudiniInput::GetAllHoudiniInputSplineComponents(TArray<UHoudiniInputHoudiniSplineComponent*>& OutObjects) const
{
	OutObjects.Empty();
	auto AddSceneComponent = [&OutObjects](UHoudiniInputObject* InputObject)
	{
		if (!InputObject)
			return;
		UHoudiniInputHoudiniSplineComponent* SceneComponentInput = Cast<UHoudiniInputHoudiniSplineComponent>(InputObject);
		if (!SceneComponentInput)
			return;
		OutObjects.Add(SceneComponentInput);
	};
	ForAllHoudiniInputObjects(AddSceneComponent);
}


void UHoudiniInput::RemoveHoudiniInputObject(UHoudiniInputObject* InInputObject)
{
	if (!InInputObject)
		return;

	ForAllHoudiniInputObjectArrays([InInputObject](TArray<UHoudiniInputObject*>& ObjectArray) {
		ObjectArray.Remove(InInputObject);
	});

	return;
}
