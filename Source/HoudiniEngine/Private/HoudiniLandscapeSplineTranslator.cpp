/*
* Copyright (c) <2023> Side Effects Software Inc.
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

#include "HoudiniLandscapeSplineTranslator.h"

#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniLandscapeRuntimeUtils.h"
#include "HoudiniLandscapeUtils.h"
#include "HoudiniPackageParams.h"
#include "HoudiniSplineTranslator.h"

#include "Landscape.h"
#include "LandscapeSplineActor.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplineSegment.h"
#include "LandscapeSplinesComponent.h"
#include "Materials/Material.h"
#include "WorldPartition/WorldPartition.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	#include "LandscapeEditLayer.h"
#endif


FVector 
FHoudiniLandscapeSplineTranslator::ConvertPositionToVector(const float* InPosition)
{
	// Swap Y/Z and convert meters to centimeters
	return {
		static_cast<double>(InPosition[0] * HAPI_UNREAL_SCALE_FACTOR_POSITION),
		static_cast<double>(InPosition[2] * HAPI_UNREAL_SCALE_FACTOR_POSITION),
		static_cast<double>(InPosition[1] * HAPI_UNREAL_SCALE_FACTOR_POSITION)
	};
}


bool
FHoudiniLandscapeSplineTranslator::ProcessLandscapeSplineOutput(
	UHoudiniOutput* InOutput,
	const TArray<ALandscapeProxy*>& InAllInputLandscapes,
	UWorld* InWorld,
	const FHoudiniPackageParams& InPackageParams,
	TMap<ALandscape*, TSet<FName>>& InClearedLayers)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniLandscapeSplineTranslator::ProcessLandscapeSplineOutput);
	if (!IsValid(InOutput))
		return false;

	if (!IsValid(InWorld))
		return false;

	// Only run on landscape spline inputs
	if (InOutput->GetType() != EHoudiniOutputType::LandscapeSpline)
		return false;

	UHoudiniAssetComponent* HAC = FHoudiniEngineUtils::GetOuterHoudiniAssetComponent(InOutput);

	// Delete any temporary landscape layers created during the last cook
	DeleteTempLandscapeLayers(InOutput);

	// If we have a valid HAC, look for the first valid output landscape to use as a fallback if the spline does
	// not specify a landscape target
	ALandscapeProxy* FallbackLandscape = nullptr;
	if (IsValid(HAC))
	{
		TArray<UHoudiniOutput*> Outputs;
		HAC->GetOutputs(Outputs);
		for (UHoudiniOutput const* const Output : Outputs)
		{
			if (!IsValid(Output) || Output->GetType() == EHoudiniOutputType::Landscape)
				continue;

			for (const auto& Entry : Output->GetOutputObjects())
			{
				const FHoudiniOutputObject& OutputObject = Entry.Value;
				if (!IsValid(OutputObject.OutputObject))
					continue;
				ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(OutputObject.OutputObject);
				if (IsValid(Proxy))
				{
					FallbackLandscape = Proxy;
					break;
				}
			}

			if (FallbackLandscape)
				break;
		}
	}

	// Keep track of segments we need to apply to edit layers per landscape. We apply after processing all HGPO for
	// this output.
	TMap<TTuple<ALandscape*, FName>, FHoudiniLandscapeSplineApplyLayerData> SegmentsToApplyToLayers;
	
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> NewOutputObjects;

	// Iterate on all the output's HGPOs
	for (const FHoudiniGeoPartObject& CurHGPO : InOutput->GetHoudiniGeoPartObjects())
	{
		// Skip any HGPO that is not a landscape spline
		if (CurHGPO.Type != EHoudiniPartType::LandscapeSpline)
			continue;

		// Create / update landscape splines from this HGPO
		static constexpr bool bForceRebuild = false;
		CreateOutputLandscapeSpline(
			CurHGPO,
			InOutput,
			InAllInputLandscapes,
			InWorld,
			InPackageParams,
			FallbackLandscape,
			InClearedLayers,
			SegmentsToApplyToLayers,
			NewOutputObjects,
			HAC);
	}

	// Apply splines to user specified edit layers and reserved spline layers
	FHoudiniLandscapeUtils::ApplySegmentsToLandscapeEditLayers(SegmentsToApplyToLayers);

	InOutput->SetOutputObjects(NewOutputObjects);
	// Mark the output as dirty when we update it with the new output objects. This ensures that the outer, the Actor
	// in the case of OFPA/World Partition, is marked as dirty and the output objects will then be saved when the user
	// saves the level.
	InOutput->MarkPackageDirty();

#if WITH_EDITORONLY_DATA
	if(IsValid(HAC) && HAC->IsOwnerSelected())
		HAC->bNeedToUpdateEditorProperties = true;
#endif

	return true;
}

void
FHoudiniLandscapeSplineTranslator::UpdateNonReservedEditLayers(
	const FHoudiniLandscapeSplineInfo& InSplineInfo,
	TMap<ALandscape*, TSet<FName>>& InClearedLayers,
	TMap<TTuple<ALandscape*, FName>, FHoudiniLandscapeSplineApplyLayerData>& InSegmentsToApplyToLayers)
{
	if (!IsValid(InSplineInfo.Landscape))
		return;

	if (!InSplineInfo.Landscape->CanHaveLayersContent())
		return;

	TSet<FName>& ClearedLayersForLandscape = InClearedLayers.FindOrAdd(InSplineInfo.Landscape);

	// If the landscape has a reserved splines layer, then we don't have track the segments to apply. Just record the
	// landscape with its reserved layer.	
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	const FLandscapeLayer* ReservedSplinesLayer = InSplineInfo.Landscape->FindLayerOfType(ULandscapeEditLayerSplines::StaticClass());
#else
	FLandscapeLayer* const ReservedSplinesLayer = InSplineInfo.Landscape->GetLandscapeSplinesReservedLayer();
#endif
	if (ReservedSplinesLayer)
	{
		FHoudiniLandscapeSplineApplyLayerData& LayerData = InSegmentsToApplyToLayers.FindOrAdd({ InSplineInfo.Landscape, ReservedSplinesLayer->Name});
		LayerData.Landscape = InSplineInfo.Landscape;
		LayerData.EditLayerName = ReservedSplinesLayer->Name;
		LayerData.bIsReservedSplineLayer = true;
		return;
	}

	if (!IsValid(InSplineInfo.SplinesOutputObject))
		return;

	for (const auto& Entry : InSplineInfo.SplinesOutputObject->GetLayerOutputs())
	{
		if (Entry.Key == NAME_None)
			continue;

		UHoudiniLandscapeSplineTargetLayerOutput* const LayerOutput = Entry.Value;
		if (!IsValid(LayerOutput))
			continue;

		const FName CookedEditLayer = *LayerOutput->CookedEditLayer;

		// Create layer if it does not exist
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		const FLandscapeLayer* const UnrealEditLayer = FHoudiniLandscapeUtils::GetOrCreateEditLayer(InSplineInfo.Landscape, CookedEditLayer);
#else
		FLandscapeLayer* const UnrealEditLayer = FHoudiniLandscapeUtils::GetOrCreateEditLayer(InSplineInfo.Landscape, CookedEditLayer);
#endif
		if (!UnrealEditLayer)
		{
			HOUDINI_LOG_ERROR(TEXT("Could not find edit layer %s and failed to create it: %s"), *CookedEditLayer.ToString(), *(InSplineInfo.Landscape->GetActorLabel()));
			continue;
		}

		// Re-order layers
		if (LayerOutput->AfterEditLayer != NAME_None)
			FHoudiniLandscapeUtils::MoveEditLayerAfter(InSplineInfo.Landscape, CookedEditLayer, LayerOutput->AfterEditLayer);

		// Clear layer if requested and not yet cleared
		if (LayerOutput->bClearLayer && !ClearedLayersForLandscape.Contains(CookedEditLayer))
		{
			InSplineInfo.Landscape->ClearLayer(UnrealEditLayer->Guid, nullptr, ELandscapeClearMode::Clear_Heightmap);
			ClearedLayersForLandscape.Add(CookedEditLayer);
		}

		// Record segments to be applied to layer
		FHoudiniLandscapeSplineApplyLayerData& LayerData = InSegmentsToApplyToLayers.FindOrAdd({ InSplineInfo.Landscape, CookedEditLayer });
		LayerData.bIsReservedSplineLayer = false;
		LayerData.Landscape = InSplineInfo.Landscape;
		LayerData.EditLayerName = CookedEditLayer;
		LayerData.SegmentsToApply.Append(LayerOutput->Segments);
	}
}

void 
FHoudiniLandscapeSplineTranslator::DeleteTempLandscapeLayers(UHoudiniOutput* const InOutput)
{
	if (!IsValid(InOutput))
		return;

	// Loop over the output objects and delete all temporary layers
	TMap<ALandscape*, TSet<FName>> DeletedLandscapeLayers;
	for (const auto& OutputObjectEntry : InOutput->GetOutputObjects())
	{
		const FHoudiniOutputObject& ObjectEntry = OutputObjectEntry.Value;
		if (!IsValid(ObjectEntry.OutputObject))
			continue;

		UHoudiniLandscapeSplinesOutput* const OutputObject = Cast<UHoudiniLandscapeSplinesOutput>(ObjectEntry.OutputObject);
		if (!OutputObject)
			continue;

		ALandscape* const Landscape = OutputObject->GetLandscape();
		if (!IsValid(Landscape))
			continue;

		TSet<FName>& DeletedLayers = DeletedLandscapeLayers.FindOrAdd(Landscape);
		for (const auto& LayerEntry : OutputObject->GetLayerOutputs())
		{
			UHoudiniLandscapeSplineTargetLayerOutput const* const LayerOutput = LayerEntry.Value;
			// Temp layers have a different EditLayerName from their BakedLayerName
			if (LayerOutput->CookedEditLayer.IsEmpty()
					|| LayerOutput->BakedEditLayer == LayerOutput->CookedEditLayer)
			{
				continue;
			}

			const FName CookedEditLayer = *LayerOutput->CookedEditLayer;
			if (DeletedLayers.Contains(CookedEditLayer))
				continue;

			FHoudiniLandscapeRuntimeUtils::DeleteEditLayer(Landscape, CookedEditLayer);
			DeletedLayers.Add(CookedEditLayer);
		}
	}
}

void
FHoudiniLandscapeSplineTranslator::AddSegmentToOutputObject(
	ULandscapeSplineSegment* InSegment,
	const FHoudiniLandscapeSplineData& InSplineData,
	int InVertexIndex,
	UHoudiniAssetComponent* InHAC,
	const FHoudiniPackageParams& InPackageParams,
	UHoudiniLandscapeSplinesOutput& InOutputObject)
{
	if (!IsValid(InSegment))
		return;

	InOutputObject.GetSegments().Add(InSegment);

	// Check for edit layer attributes, for each check vertex first, then prim

	FName EditLayerName = NAME_None;
	// Edit layer name
	if (InSplineData.SegmentEditLayers.IsValidIndex(InVertexIndex))
	{
		EditLayerName = *InSplineData.SegmentEditLayers[InVertexIndex];
	}
	else 
	{
		EditLayerName = *InSplineData.DefaultEditLayer;
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Set Layer names. The Baked layer name is always what the user specifies; If we are modifying an existing landscape,
	// use temporary names if specified or user name baked names.
	// -----------------------------------------------------------------------------------------------------------------

	const FName BakedLayerName = EditLayerName;

	// For the cooked name, but the layer name first so it is easier to read in the Landscape Editor UI.
	if (IsValid(InHAC) && InHAC->bLandscapeUseTempLayers)
	{
		EditLayerName = *(
			EditLayerName.ToString() + FString(" : ")
			+ InPackageParams.GetPackageName() + InHAC->GetComponentGUID().ToString());
	}

	// Now that we have the final cooked / temp edit layer name, find or create the Layer Output object for this layer
	TMap<FName, UHoudiniLandscapeSplineTargetLayerOutput*>& LayerOutputs = InOutputObject.GetLayerOutputs();
	UHoudiniLandscapeSplineTargetLayerOutput* LayerOutput = nullptr;
	if (!LayerOutputs.Contains(EditLayerName))
	{
		// Create the layer output
		LayerOutput = NewObject<UHoudiniLandscapeSplineTargetLayerOutput>(&InOutputObject);
		LayerOutputs.Add(EditLayerName, LayerOutput);

		// Set the properties on the newly created layer
		LayerOutput->Landscape = InOutputObject.GetLandscape();
		LayerOutput->LandscapeProxy = InOutputObject.GetLandscapeProxy();
		LayerOutput->CookedEditLayer = EditLayerName.ToString();
		LayerOutput->BakedEditLayer = BakedLayerName.ToString();
		LayerOutput->bCreatedLandscape = false;

		LayerOutput->bCookedLayerRequiresBaking = (BakedLayerName != EditLayerName);

		// Edit layer clear
		if (InSplineData.SegmentEditLayersClear.IsValidIndex(InVertexIndex))
		{
			LayerOutput->bClearLayer = InSplineData.SegmentEditLayersClear[InVertexIndex] != 0;
		}
		else 
		{
			LayerOutput->bClearLayer = InSplineData.DefaultEditLayerClear;
		}

		// Edit layer create after
		if (InSplineData.SegmentEditLayersAfter.IsValidIndex(InVertexIndex))
		{
			LayerOutput->AfterEditLayer = *InSplineData.SegmentEditLayersAfter[InVertexIndex];
		}
		else 
		{
			LayerOutput->AfterEditLayer = *InSplineData.DefaultEditLayerAfter;
		}
	}
	else
	{
		// Layer entry already exists, just fetch it, don't reset properties
		LayerOutput = LayerOutputs[EditLayerName];
	}

	// Add the segment to the layer output
	LayerOutput->Segments.Add(InSegment);
}

bool
FHoudiniLandscapeSplineTranslator::CreateOutputLandscapeSpline(
	const FHoudiniGeoPartObject& InHGPO,
	UHoudiniOutput* InOutput,
	const TArray<ALandscapeProxy*>& InAllInputLandscapes,
	UWorld* InWorld,
	const FHoudiniPackageParams& InPackageParams,
	ALandscapeProxy* InFallbackLandscape,
	TMap<ALandscape*, TSet<FName>>& InClearedLayers,
	TMap<TTuple<ALandscape*, FName>, FHoudiniLandscapeSplineApplyLayerData>& SegmentsToApplyToLayers,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects,
	UHoudiniAssetComponent* InHAC)
{
#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
	HOUDINI_LOG_WARNING(TEXT("Landscape Spline Output is only supported in UE5.1+"));
	return false;
#else

	FTransform HACTransform = IsValid(InHAC) ? InHAC->GetComponentTransform() : FTransform::Identity;

	// Find the fallback landscape to use, either InFallbackLandscape if valid, otherwise the first one we find in the
	// world
	bool bIsUsingWorldPartition = IsValid(InWorld->GetWorldPartition());
	ALandscapeProxy* FallbackLandscape = InFallbackLandscape;
	if (!IsValid(FallbackLandscape))
	{
		TActorIterator<ALandscapeProxy> LandscapeIt(InWorld, ALandscape::StaticClass());
		if (LandscapeIt)
			FallbackLandscape = *LandscapeIt;
	}

	const HAPI_Session * Session = FHoudiniEngine::Get().GetSession();
	if (!Session)
		return false;

	// Get the curve info from HAPI
	HAPI_CurveInfo CurveInfo;
	FHoudiniApi::CurveInfo_Init(&CurveInfo);
	FHoudiniApi::GetCurveInfo(Session, InHGPO.GeoId, InHGPO.PartId, &CurveInfo);

	// Get the point/vertex count for each curve primitive
	int NumCurves = CurveInfo.curveCount;
	TArray<int> CurvePointCounts;
	CurvePointCounts.SetNumZeroed(NumCurves);
	FHoudiniApi::GetCurveCounts(Session, InHGPO.GeoId, InHGPO.PartId, CurvePointCounts.GetData(), 0, NumCurves);

	// Bug #134941: Unreal may crash when there are a large number of control points. At least output a warning.
	for(int Index = 0; Index <  CurvePointCounts.Num(); Index++)
	{
		int NumPoints = CurvePointCounts[Index];

		if (NumPoints > 1000)
		{
			HOUDINI_LOG_ERROR(TEXT(
						"A landscape spline contains more than 1000 control points. "
						"This may lead to instability when saving levels in Unreal, limiting the number of points to 1000. "
						"Consider splitting splines inside Houdini."));
			CurvePointCounts[Index] = 1000;
		}
	}
	

	// Extract all target landscapes refs as prim attributes
	TArray<FString> LandscapeRefs;

	FHoudiniHapiAccessor Accessor(InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_TARGET_LANDSCAPE);
	Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, 1, LandscapeRefs);

	// Extract all custom output name as prim attributes (used for landscape spline actor names in WP, not applicable to non-WP).
	TArray<FString> OutputNames;
	Accessor.Init(InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2);
	Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, 1, OutputNames);

	//--------------------------------------------------------------------------------------------------------------------------------------------------
	// Iterate over curves first, use prim attributes to find the landscape that the splines should be attached to,
	// and for world partition look at unreal_output_name to determine the landscape spline actor name.
	//--------------------------------------------------------------------------------------------------------------------------------------------------

	TMap<FString, FHoudiniLandscapeSplineInfo> LandscapeSplineInfos;
	LandscapeSplineInfos.Reserve(NumCurves);
	for (int CurveIdx = 0, NextCurveStartPointIdx = 0; CurveIdx < NumCurves; ++CurveIdx)
	{
		int NumPointsInCurve = CurvePointCounts[CurveIdx];
		int CurveFirstPointIndex = NextCurveStartPointIdx;
		NextCurveStartPointIdx += NumPointsInCurve;

		//
		// Determine the name (or NAME_None in non-WP)
		//
		FName OutputName = NAME_None;
		if (bIsUsingWorldPartition && OutputNames.IsValidIndex(CurveIdx))
		{
			OutputName = *OutputNames[CurveIdx];
		}

		//
		// Use the landscape specified with the landscape target attribute
		//

		FString LandscapeRef;
		ALandscapeProxy* TargetLandscape = nullptr;
		if (LandscapeRefs.IsValidIndex(CurveIdx))
		{
			LandscapeRef = LandscapeRefs[CurveIdx];
			TargetLandscape = FHoudiniLandscapeUtils::FindTargetLandscapeProxy(LandscapeRef, InWorld, InAllInputLandscapes);
		}

		if (!IsValid(TargetLandscape))
			TargetLandscape = FallbackLandscape;

		// If at this point we don't have a valid target landscape then we cannot proceed, return false
		if (!IsValid(TargetLandscape))
		{
			HOUDINI_LOG_ERROR(TEXT("Could not find target landscape: '%s', and also could not find a "
					 "fallback landscape from the HAC or World."), *LandscapeRef);
			return false;
		}

		FString IdentifierName = FString::Printf(TEXT("%s-%s-%s"), *InHGPO.PartName, *TargetLandscape->GetFName().ToString(), *OutputName.ToString());

		FHoudiniOutputObjectIdentifier Identifier(InHGPO.ObjectId, InHGPO.GeoId, InHGPO.PartId, IdentifierName);

		// Get/create the FHoudiniLandscapeSplineInfo entry that we use to manage the data for each
		// ULandscapeSplinesComponent / ALandscapeSplineActor that we will output to

		if (!LandscapeSplineInfos.Find(IdentifierName))
		{
			FHoudiniLandscapeSplineInfo SplineInfo;

			// Initialize NextControlPointId to 0. For each curve primitive added to this SplineInfo we will increase
			// NextControlPointId based on the control point ids of the curve primitive, so that NextControlPointId
			// is greater than all of the control point ids of all curves in the SplineInfo

			SplineInfo.Identifier = Identifier;
			SplineInfo.OutputName = OutputName;
			SplineInfo.NextControlPointId = 0;

			SplineInfo.LayerPackageParams = InPackageParams;
			SplineInfo.LayerPackageParams.ObjectId = Identifier.ObjectId;
			SplineInfo.LayerPackageParams.GeoId = Identifier.GeoId;
			SplineInfo.LayerPackageParams.PartId = Identifier.PartId;
			
			SplineInfo.SplineActorPackageParams = SplineInfo.LayerPackageParams;
			SplineInfo.SplineActorPackageParams.SplitStr = OutputName.ToString();

			SplineInfo.LandscapeProxy = TargetLandscape;
			SplineInfo.Landscape = SplineInfo.LandscapeProxy->GetLandscapeActor();

			// Validation
			ULandscapeInfo* LandscapeInfo = SplineInfo.LandscapeProxy->GetLandscapeInfo();
			if (!IsValid(LandscapeInfo))
			{
				HOUDINI_LOG_ERROR(TEXT("landscape: has no information!"));
				return false;
			}

			// If the world is using world partition we need to create a landscape spline actor, or manipulate
			// the landscape splines component on the landscape directly (non-world partition)
			if (bIsUsingWorldPartition)
			{
				// In world partition, create a new spline actor and name it to OutputName via PackageParams.
				if (!SplineInfo.LandscapeSplineActor)
				{
					SplineInfo.LandscapeSplineActor = LandscapeInfo->CreateSplineActor(FVector::ZeroVector);
					if (!IsValid(SplineInfo.LandscapeSplineActor))
						return false;

					FHoudiniEngineUtils::SafeRenameActor(SplineInfo.LandscapeSplineActor, SplineInfo.SplineActorPackageParams.GetPackageName());
				}
				
				SplineInfo.LandscapeSplineActor->SetActorTransform(HACTransform);
				SplineInfo.SplinesComponent = SplineInfo.LandscapeSplineActor->GetSplinesComponent();
			}
			else 
			{
				SplineInfo.SplinesComponent = SplineInfo.LandscapeProxy->GetSplinesComponent();
				if (!IsValid(SplineInfo.SplinesComponent))
				{
					SplineInfo.LandscapeProxy->CreateSplineComponent();
					SplineInfo.SplinesComponent = SplineInfo.LandscapeProxy->GetSplinesComponent();
				}
			}

			if (!SplineInfo.SplinesComponent)
			{
				HOUDINI_LOG_ERROR(TEXT("landscape: failed to create a spline component!"));
				return false;
			}
			// Update the objects on the SplinesOutputObject to match the SplineInfo
			SplineInfo.SplinesOutputObject = NewObject<UHoudiniLandscapeSplinesOutput>(InOutput);
			SplineInfo.SplinesOutputObject->SetLandscapeProxy(SplineInfo.LandscapeProxy);
			SplineInfo.SplinesOutputObject->SetLandscape(SplineInfo.Landscape);
			SplineInfo.SplinesOutputObject->SetLandscapeSplinesComponent(SplineInfo.SplinesComponent);
			SplineInfo.SplinesOutputObject->SetLandscapeSplineActor(SplineInfo.LandscapeSplineActor);

			FHoudiniOutputObject OutputObject;
			OutputObject.OutputObject = SplineInfo.SplinesOutputObject;

			OutputObjects.Add(SplineInfo.Identifier, OutputObject);

			LandscapeSplineInfos.Add(IdentifierName, SplineInfo);
		}

		FHoudiniLandscapeSplineInfo* SplineInfo = LandscapeSplineInfos.Find(IdentifierName);

		// Add the primitive and point indices of this curve to the SplineInfo 
		SplineInfo->CurveIndices.Add(CurveIdx);
		SplineInfo->PerCurvePointCount.Add(CurvePointCounts[CurveIdx]);
		SplineInfo->PerCurveFirstPointIndex.Add(CurveFirstPointIndex);

		// Copy the attributes for this curve primitive from Houdini / HAPI

		SplineInfo->SplineData.AddDefaulted_GetRef() = GetSplineDataFromAttributes(
				InHGPO.GeoId,
				InHGPO.PartId,
				CurveIdx,
				CurveFirstPointIndex,
				NumPointsInCurve);

		// Ensure that NextControlPointId is greater than all of the PointIds from this curve
		FHoudiniLandscapeSplineData& SplineData = SplineInfo->SplineData.Last();
		for (int ControlPointId : SplineData.PointIds)
		{
			if (ControlPointId >= SplineInfo->NextControlPointId)
				SplineInfo->NextControlPointId = ControlPointId + 1;
		}
	}
	
	// Fetch generic attributes
	TArray<FHoudiniGenericAttribute> GenericPointAttributes;
	FHoudiniEngineUtils::GetGenericAttributeList(InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, GenericPointAttributes, HAPI_ATTROWNER_POINT);
	TArray<FHoudiniGenericAttribute> GenericPrimAttributes;
	FHoudiniEngineUtils::GetGenericAttributeList(InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, GenericPrimAttributes, HAPI_ATTROWNER_PRIM);

	// Process each SplineInfo entry 
	for (auto& Entry : LandscapeSplineInfos)
	{
		FHoudiniLandscapeSplineInfo& SplineInfo = Entry.Value;

		// When not using world partition, we have to transform the splines: apply the HAC's world transform and then
		// the inverse of the LandscapeSplinesComponent's transform.
		// For world partition, we set the LandscapeSplineActor's transform to the HAC's transform
		const FTransform TransformToApply = !bIsUsingWorldPartition
			? HACTransform.GetRelativeTransform(SplineInfo.SplinesComponent->GetComponentTransform()) 
			: FTransform::Identity;

		TArray<TObjectPtr<ULandscapeSplineControlPoint>>& ControlPoints = SplineInfo.SplinesComponent->GetControlPoints();
		TArray<TObjectPtr<ULandscapeSplineSegment>>& Segments = SplineInfo.SplinesComponent->GetSegments();

		// Process each curve primitive recorded in SplineInfo. Each curve primitive will be at least one segment (with
		// at least the first and last points of the primitive being control points).
		int NumCurvesInSpline = SplineInfo.PerCurveFirstPointIndex.Num();
		for (int CurveEntryIdx = 0; CurveEntryIdx < NumCurvesInSpline; ++CurveEntryIdx)
		{
			const FHoudiniLandscapeSplineData& HoudiniCurveData = SplineInfo.SplineData[CurveEntryIdx];
			ULandscapeSplineControlPoint* PreviousControlPoint = nullptr;
			int PreviousControlPointArrayIdx = INDEX_NONE;

			int NumPointsInCurve = SplineInfo.PerCurvePointCount[CurveEntryIdx];
			for (int CurvePointArrayIdx = 0; CurvePointArrayIdx < NumPointsInCurve; ++CurvePointArrayIdx)
			{
				int HGPOPointIndex = SplineInfo.PerCurveFirstPointIndex[CurveEntryIdx] + CurvePointArrayIdx;
				
				// Check if this is a control point: it has a control point id attribute >= 0, or is the first or last
				// point of the curve prim.
				int ControlPointId = INDEX_NONE;
				if (HoudiniCurveData.PointIds.IsValidIndex(CurvePointArrayIdx))
				{
					ControlPointId = HoudiniCurveData.PointIds[CurvePointArrayIdx];
					if (ControlPointId < 0)
						ControlPointId = INDEX_NONE;
				}

				bool bControlPointCreated = false;
				ULandscapeSplineControlPoint* ThisControlPoint = nullptr;
				// A point is a control point if:
				// 1. It is the first or last point of the curve, or
				// 2. It has a valid (>=0) control point id, or
				// 3. The control point id attribute does not exist
				if (!PreviousControlPoint || CurvePointArrayIdx == NumPointsInCurve - 1 || ControlPointId >= 0 || HoudiniCurveData.PointIds.IsEmpty())
					ThisControlPoint = GetOrCreateControlPoint(SplineInfo, ControlPointId, bControlPointCreated);

				if (bControlPointCreated && IsValid(ThisControlPoint))
				{
					SplineInfo.SplinesOutputObject->GetControlPoints().Add(ThisControlPoint);
					ControlPoints.Add(ThisControlPoint);
					ThisControlPoint->Location = TransformToApply.TransformPosition(ConvertPositionToVector(&HoudiniCurveData.PointPositions[CurvePointArrayIdx * 3]));

					// Update generic properties attributes on the control point
					FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(ThisControlPoint, GenericPointAttributes, HGPOPointIndex);

					// Apply point attributes
					SetControlPointData(ThisControlPoint, HoudiniCurveData, TransformToApply, CurvePointArrayIdx);
				}

				// If we have two control points, create a segment
				if (PreviousControlPoint && ThisControlPoint)
				{
					// Create the segment
					ULandscapeSplineSegment* Segment = NewObject<ULandscapeSplineSegment>(SplineInfo.SplinesComponent, ULandscapeSplineSegment::StaticClass());
					Segment->Connections[0].ControlPoint = PreviousControlPoint;
					Segment->Connections[1].ControlPoint = ThisControlPoint;

					// Update generic properties attributes on the segment
					FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(Segment, GenericPointAttributes, SplineInfo.PerCurveFirstPointIndex[CurveEntryIdx]);
					FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(Segment, GenericPrimAttributes, SplineInfo.CurveIndices[CurveEntryIdx]);

					// Apply attributes to segment
					SetSegmentData(Segment, HoudiniCurveData, CurvePointArrayIdx);

					// Apply attributes for connections
					SetConnectionData(Segment->Connections[0], 0, HoudiniCurveData, PreviousControlPointArrayIdx);
					SetConnectionData(Segment->Connections[1], 1, HoudiniCurveData, CurvePointArrayIdx);
					
					FVector StartLocation;
					FRotator StartRotation;
					PreviousControlPoint->GetConnectionLocationAndRotation(Segment->Connections[0].SocketName, StartLocation, StartRotation);
					FVector EndLocation;
					FRotator EndRotation;
					ThisControlPoint->GetConnectionLocationAndRotation(Segment->Connections[1].SocketName, EndLocation, EndRotation);

					// Set up tangent lengths if not in vertex/prim connection attributes
					if (!(HoudiniCurveData.SegmentConnectionTangentLengths[0].IsValidIndex(PreviousControlPointArrayIdx))
						|| !(HoudiniCurveData.DefaultConnectionTangentLengths[0]))
					{
						Segment->Connections[0].TangentLen = (EndLocation - StartLocation).Size();
					}
					if (!(HoudiniCurveData.SegmentConnectionTangentLengths[1].IsValidIndex(CurvePointArrayIdx))
						|| !(HoudiniCurveData.DefaultConnectionTangentLengths[1]))
					{
						Segment->Connections[1].TangentLen = Segment->Connections[0].TangentLen;
					}

					Segment->AutoFlipTangents();
					
					PreviousControlPoint->ConnectedSegments.Add(FLandscapeSplineConnection(Segment, 0));
					ThisControlPoint->ConnectedSegments.Add(FLandscapeSplineConnection(Segment, 1));

					// Auto-calculate rotation if we didn't receive rotation attributes
					if (!HoudiniCurveData.PointRotations.IsValidIndex(PreviousControlPointArrayIdx))
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
						PreviousControlPoint->AutoCalcRotation(true);
#else
						PreviousControlPoint->AutoCalcRotation();
#endif
					if (!HoudiniCurveData.PointRotations.IsValidIndex(CurvePointArrayIdx)) 
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
						ThisControlPoint->AutoCalcRotation(true);
#else
						ThisControlPoint->AutoCalcRotation();
#endif

					// Add the segment to the appropriate LayerOutput. Will create the LayerOutput if necessary.
					AddSegmentToOutputObject(Segment, HoudiniCurveData, CurvePointArrayIdx, InHAC, SplineInfo.LayerPackageParams, *SplineInfo.SplinesOutputObject);
					Segments.Add(Segment);
				}

				// If we created a control point in this iteration, record that as the previous control point for the
				// next iteration
				if (ThisControlPoint)
				{
					PreviousControlPoint = ThisControlPoint;
					PreviousControlPointArrayIdx = CurvePointArrayIdx;
				}
			}
		}

		SplineInfo.SplinesComponent->RebuildAllSplines();
		
		FHoudiniOutputObject* OutputObject = OutputObjects.Find(SplineInfo.Identifier);

		GetCachedAttributes(OutputObject, InHGPO, SplineInfo);

		// Handle user specified landscape layers for these segments
		UpdateNonReservedEditLayers(SplineInfo, InClearedLayers, SegmentsToApplyToLayers);
	}

	return true;
#endif
}

void FHoudiniLandscapeSplineTranslator::GetCachedAttributes(FHoudiniOutputObject * OutputObject, const FHoudiniGeoPartObject& InHGPO, const FHoudiniLandscapeSplineInfo& SplineInfo)
{
	// Cache commonly supported Houdini attributes on the OutputAttributes
	TArray<FString> LevelPaths;
	FHoudiniEngineUtils::GetLevelPathAttribute(InHGPO.GeoId, InHGPO.PartId, LevelPaths, HAPI_ATTROWNER_INVALID, 0, 1);
	if (LevelPaths.Num() > 0 && !LevelPaths[0].IsEmpty())
	{
		// cache the level path attribute on the output object
		OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_LEVEL_PATH, LevelPaths[0]);
	}

	// cache the output name attribute on the output object
	OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2, SplineInfo.OutputName.ToString());

	int FirstCurvePrimIndex = SplineInfo.CurveIndices.Num() > 0 ? SplineInfo.CurveIndices[0] : INDEX_NONE;

	if (FirstCurvePrimIndex != INDEX_NONE)
	{
		TArray<FString> BakeNames;
		FHoudiniEngineUtils::GetBakeNameAttribute(InHGPO.GeoId, InHGPO.PartId, BakeNames, HAPI_ATTROWNER_PRIM, FirstCurvePrimIndex, 1);

		if (BakeNames.Num() > 0 && !BakeNames[0].IsEmpty())
		{
			// cache the output name attribute on the output object
			OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_NAME, BakeNames[0]);
		}

		TArray<FString> BakeOutputActorNames;
		FHoudiniEngineUtils::GetBakeActorAttribute(InHGPO.GeoId, InHGPO.PartId, BakeOutputActorNames, HAPI_ATTROWNER_PRIM, FirstCurvePrimIndex, 1);
		if (BakeOutputActorNames.Num() > 0 && !BakeOutputActorNames[0].IsEmpty())
		{
			// cache the bake actor attribute on the output object
			OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_ACTOR, BakeOutputActorNames[0]);
		}

		TArray<FString> BakeOutputActorClassNames;
		if (FHoudiniEngineUtils::GetBakeActorClassAttribute(InHGPO.GeoId, InHGPO.PartId, BakeOutputActorClassNames, HAPI_ATTROWNER_PRIM, FirstCurvePrimIndex, 1))
			if (BakeOutputActorClassNames.Num() > 0 && !BakeOutputActorClassNames[0].IsEmpty())
			{
				// cache the bake actor attribute on the output object
				OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_ACTOR_CLASS, BakeOutputActorClassNames[0]);
			}

		TArray<FString> BakeFolders;
		FHoudiniEngineUtils::GetBakeFolderAttribute(InHGPO.GeoId, BakeFolders, InHGPO.PartId, 0, 1);
		if (BakeFolders.Num() > 0 && !BakeFolders[0].IsEmpty())
		{
			// cache the unreal_bake_folder attribute on the output object
			OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_FOLDER, BakeFolders[0]);
		}

		TArray<FString> BakeOutlinerFolders;
		FHoudiniEngineUtils::GetBakeOutlinerFolderAttribute(InHGPO.GeoId, InHGPO.PartId, BakeOutlinerFolders, HAPI_ATTROWNER_PRIM, FirstCurvePrimIndex, 1);
		if (BakeOutlinerFolders.Num() > 0 && !BakeOutlinerFolders[0].IsEmpty())
		{
			// cache the bake actor attribute on the output object
			OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_OUTLINER_FOLDER, BakeOutlinerFolders[0]);
		}
	}
}

ULandscapeSplineControlPoint*
FHoudiniLandscapeSplineTranslator::GetOrCreateControlPoint(FHoudiniLandscapeSplineInfo& SplineInfo, const int InControlPointId, bool& bOutCreated)
{
	ULandscapeSplineControlPoint* ControlPoint = nullptr;
	if (InControlPointId >= 0 && SplineInfo.ControlPointMap.Contains(InControlPointId))
		ControlPoint = SplineInfo.ControlPointMap[InControlPointId];
	if (!IsValid(ControlPoint))
	{
		// Point is null/invalid or has not yet been created, so create it
		ControlPoint = NewObject<ULandscapeSplineControlPoint>(SplineInfo.SplinesComponent, ULandscapeSplineControlPoint::StaticClass());

		// Assign a control point Id to the new point
		//	InControlPointId if its valid
		//	otherwise, generate a new Id from SplineInfo.NextControlPointId 
		int ControlPointId = InControlPointId;
		if (ControlPointId < 0)
		{
			ControlPointId = SplineInfo.NextControlPointId;
			SplineInfo.NextControlPointId++;
		}
		
		SplineInfo.ControlPointMap.Add(ControlPointId, ControlPoint);
		bOutCreated = true;
	}
	else
	{
		// Found the previously created valid point, just return it
		bOutCreated = false;
	}

	return ControlPoint;
}


bool
FHoudiniLandscapeSplineTranslator::CopySegmentMeshAttributesFromHoudini(
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	HAPI_AttributeOwner InAttrOwner,
	int InStartIndex,
	int InCount,
	TArray<FHoudiniLandscapeSplineSegmentMeshData>& AllSegmentMeshData)
{
	AllSegmentMeshData.SetNum(InCount);
	int MeshIndex = 0;

	while (true)
	{
		// If MeshIndex == 0 then don't add the numeric suffix
		const FString AttrNamePrefix = MeshIndex > 0
			? FString::Printf(TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_MESH "%d"), MeshIndex)
			: FString(TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_MESH));
		TArray<FString> MeshReferences;
		FHoudiniHapiAccessor Accessor;
		Accessor.Init(InNodeId, InPartId, TCHAR_TO_ANSI(*AttrNamePrefix));
		Accessor.GetAttributeData(InAttrOwner, 1, MeshReferences, InStartIndex, InCount);

		// mesh scale
		FString MeshScaleAttrName = FString::Printf(TEXT("%s%s"), *AttrNamePrefix, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_SCALE_SUFFIX));
		TArray<float> MeshScales;
		Accessor.Init(InNodeId, InPartId, TCHAR_TO_ANSI(*MeshScaleAttrName));
		Accessor.GetAttributeData(InAttrOwner, 3, MeshScales, InStartIndex, InCount);

		// center adjust
		TArray<float>  CenterAdjust;
		FString MeshCenterAdjustAttrName = FString::Printf(TEXT("%s%s"), *AttrNamePrefix, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_CENTER_ADJUST_SUFFIX));
		Accessor.Init(InNodeId, InPartId, TCHAR_TO_ANSI(*MeshCenterAdjustAttrName));
		Accessor.GetAttributeData(InAttrOwner, 2, CenterAdjust, InStartIndex, InCount);

		// material overrides
		TArray<TArray<FString>> MeterialOverideList;
		FString MaterialAttrNamePrefix = AttrNamePrefix + TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_MATERIAL_OVERRIDE_SUFFIX);

		// The same as with the MeshIndex above, loop until the first iteration where we cannot find a material
		// override attribute
		int MaterialOverrideIdx = 0;
		while (true)
		{
			TArray<FString> MaterialOverrides;

			// Add the MaterialOverrideIdx as a suffix to the attribute name when > 0
			FString MaterialOverrideAttrName = MaterialOverrideIdx > 0
				? MaterialAttrNamePrefix + FString::Printf(TEXT("%d"), MaterialOverrideIdx)
				: MaterialAttrNamePrefix;

			Accessor.Init(InNodeId, InPartId, TCHAR_TO_ANSI(*MaterialOverrideAttrName));
			bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1, MaterialOverrides);

			if (!bSuccess)
			{
				break;
			}

			MeterialOverideList.Emplace(MoveTemp(MaterialOverrides));
			MaterialOverrideIdx++;
		}

		bool bNoData = MeshReferences.IsEmpty() && MeshScales.IsEmpty() && CenterAdjust.IsEmpty() && MeterialOverideList.IsEmpty();
		if (bNoData)
			break;

		for(int Index = 0; Index < InCount; Index++)
		{
			FHoudiniLandscapeSplineMesh SegmentMeshData;

			bool bSetOne =  false;

			if (MeshReferences.IsValidIndex(Index))
			{
				SegmentMeshData.MeshRef = MeshReferences[Index];
				bSetOne = true;
			}
			if (MeshScales.IsValidIndex(Index * 3))
			{
				SegmentMeshData.MeshScale = FVector(
					MeshScales[Index * 3 + 0],
					MeshScales[Index * 3 + 2],
					MeshScales[Index * 3 + 1]);
				bSetOne = true;
			}
			if (CenterAdjust.IsValidIndex(Index * 2))
			{
				SegmentMeshData.CenterAdjust = FVector2d(
					CenterAdjust[Index * 2 + 0],
					CenterAdjust[Index * 2 + 1]);
				bSetOne = true;
			}
			if (MeterialOverideList.IsValidIndex(Index))
			{
				SegmentMeshData.MaterialOverrideRef = MeterialOverideList[Index];
				bSetOne = true;
			}

			if (bSetOne)
				AllSegmentMeshData[Index].Meshes.Add(SegmentMeshData);

		}

		MeshIndex++;
	}

	AllSegmentMeshData.Shrink();
	return true;
}

FHoudiniLandscapeSplineData
FHoudiniLandscapeSplineTranslator::GetSplineDataFromAttributes(
	HAPI_NodeId InNodeId,
	HAPI_PartId InPartId,
	int InPrimIndex,
	int InFirstPointIndex,
	int InNumPoints)
{
	FHoudiniLandscapeSplineData SplineData;

	// point positions
	FHoudiniHapiAccessor Accessor(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_POSITION);
	Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 3, SplineData.PointPositions, InFirstPointIndex, InNumPoints);

	// rot attribute (quaternion) -- control point rotations
	Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_ROTATION);
	bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 4, SplineData.PointRotations, InFirstPointIndex, InNumPoints);

	Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_PAINT_LAYER_NAME);
	Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, SplineData.PointPaintLayerNames, InFirstPointIndex, InNumPoints);

	Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_RAISE_TERRAIN);
	Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, SplineData.PointRaiseTerrains, InFirstPointIndex, InNumPoints);

	Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_LOWER_TERRAIN);
	Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, SplineData.PointLowerTerrains, InFirstPointIndex, InNumPoints);

	Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_MESH);
	Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, SplineData.PointMeshRefs, InFirstPointIndex, InNumPoints);

	// control point material overrides
	SplineData.PerMaterialOverridePointRefs.Reset();
	const FString ControlPointMaterialOverrideAttrNamePrefix = TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_MATERIAL_OVERRIDE_SUFFIX);

	// Loop until the first iteration where we don't find any material override attributes.
	int MaterialOverrideIdx = 0;
	while (true)
	{
		TArray<FString> MaterialOverrides;

		// If the index > 0 add it as a suffix to the attribute name
		const FString AttrName = MaterialOverrideIdx > 0
			? ControlPointMaterialOverrideAttrNamePrefix + FString::Printf(TEXT("%d"), MaterialOverrideIdx)
			: ControlPointMaterialOverrideAttrNamePrefix;

		Accessor.Init(InNodeId, InPartId, TCHAR_TO_ANSI(*AttrName));
		bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, MaterialOverrides);
		if (!bSuccess)
			break;
		
		SplineData.PerMaterialOverridePointRefs.Emplace(MoveTemp(MaterialOverrides));
		MaterialOverrideIdx++;
	}

	// control point mesh scales

	Accessor.Init(InNodeId,InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_MESH HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_SCALE_SUFFIX);
	Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 3, SplineData.PointMeshScales, InFirstPointIndex, InNumPoints);

	Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_ID);
	Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, SplineData.PointIds, InFirstPointIndex, InNumPoints);

	Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_HALF_WIDTH);
	Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, SplineData.PointHalfWidths, InFirstPointIndex, InNumPoints);

	Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SIDE_FALLOFF);
	Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, SplineData.PointSideFalloffs, InFirstPointIndex, InNumPoints);

	Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_END_FALLOFF);
	Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, SplineData.PointEndFalloffs, InFirstPointIndex, InNumPoints);

	// Connection attributes -- there are separate attributes for the two ends of the connection
	static const char* ConnectionMeshSocketNameAttrNames[]
	{
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONNECTION0_MESH_SOCKET_NAME,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONNECTION1_MESH_SOCKET_NAME
	};
	static const char* ConnectionTangentLengthAttrNames[]
	{
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONNECTION0_TANGENT_LENGTH,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONNECTION1_TANGENT_LENGTH
	};
	for (int ConnectionIndex = 0; ConnectionIndex < 2; ++ConnectionIndex)
	{
		// segment connection[ConnectionIndex] socket names -- vertex/point attribute
		Accessor.Init(InNodeId, InPartId, ConnectionMeshSocketNameAttrNames[ConnectionIndex]);
		Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, SplineData.SegmentConnectionSocketNames[ConnectionIndex]);

		Accessor.Init(InNodeId, InPartId, ConnectionTangentLengthAttrNames[ConnectionIndex]);
		Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, SplineData.SegmentConnectionTangentLengths[ConnectionIndex], InFirstPointIndex, InNumPoints);
	}

	// segment paint layer name -- vertex/point
	
	Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_PAINT_LAYER_NAME);
	bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, SplineData.SegmentPaintLayerNames, InFirstPointIndex, InNumPoints);

	// segment raise terrains -- vertex/point

	Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_RAISE_TERRAIN);
	bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1,  SplineData.SegmentRaiseTerrains, InFirstPointIndex, InNumPoints);

	Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_LOWER_TERRAIN);
	bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, SplineData.SegmentLowerTerrains, InFirstPointIndex, InNumPoints);

	Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_NAME);
	bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, SplineData.SegmentEditLayers, InFirstPointIndex, InNumPoints);

	Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_CLEAR);
	bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, SplineData.SegmentEditLayersClear, InFirstPointIndex, InNumPoints);

	Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_AFTER);
	bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, 1, SplineData.SegmentEditLayersAfter, InFirstPointIndex, InNumPoints);

	// segment paint layer name
	if (SplineData.SegmentPaintLayerNames.IsEmpty())
	{
		TArray<FString> SegmentPaintLayerName;

		Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_PAINT_LAYER_NAME);
		bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, 1, SegmentPaintLayerName, InPrimIndex, 1);

		if (bSuccess && SegmentPaintLayerName.Num() > 0)
		{
			SplineData.DefaultPaintLayerName = SegmentPaintLayerName[0];
		}
	}

	// segment raise terrains
	if (SplineData.SegmentRaiseTerrains.IsEmpty())
	{
		TArray<int> RaiseTerrains;
		HAPI_AttributeInfo PrimRaiseTerrainAttrInfo;

		Accessor.Init(InNodeId,InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_RAISE_TERRAIN);
		Accessor.GetInfo(PrimRaiseTerrainAttrInfo, HAPI_ATTROWNER_INVALID);
		PrimRaiseTerrainAttrInfo.tupleSize = 1;
		bSuccess = Accessor.GetAttributeData(PrimRaiseTerrainAttrInfo, RaiseTerrains, InPrimIndex, 1);

		if (bSuccess && RaiseTerrains.Num() > 0)
		{
			SplineData.DefaultRaiseTerrain = RaiseTerrains[0] != 0;
		}
	}
	// segment lower terrains
	if (SplineData.SegmentLowerTerrains.IsEmpty())
	{
		TArray<int> LowerTerrains;

		HAPI_AttributeInfo PrimLowerTerrainAttrInfo;
		Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_LOWER_TERRAIN);
		Accessor.GetInfo(PrimLowerTerrainAttrInfo, HAPI_ATTROWNER_PRIM);
		bSuccess = Accessor.GetAttributeData(PrimLowerTerrainAttrInfo, LowerTerrains);

		if (bSuccess && LowerTerrains.Num() > 0)
		{
			SplineData.DefaultLowerTerrain = LowerTerrains[0];
		}
	}

	// segment edit layer -- prim
	if (SplineData.SegmentEditLayers.IsEmpty())
	{
		TArray<FString> EditLayers;
		Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_NAME);
		bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, 1, EditLayers);

		if (bSuccess && EditLayers.Num() > 0)
		{
			SplineData.DefaultEditLayer = EditLayers[0];
		}
	}

	// segment edit layer clear -- prim
	if (SplineData.SegmentEditLayersClear.IsEmpty())
	{
		TArray<int> EditLayersClear;
		HAPI_AttributeInfo PrimEditLayerClearAttrInfo;

		Accessor.Init(InNodeId,InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_CLEAR);
		bSuccess = Accessor.GetAttributeData(PrimEditLayerClearAttrInfo, EditLayersClear, InPrimIndex, 1);

		if (bSuccess && EditLayersClear.Num() > 0)
		{
			SplineData.DefaultEditLayerClear = static_cast<bool>(EditLayersClear[0]);
		}
	}

	// segment edit layer after -- prim
	if (SplineData.SegmentEditLayersAfter.IsEmpty())
	{
		TArray<FString> EditLayersAfter;
		Accessor.Init(InNodeId, InPartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_AFTER);
		bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, 1, EditLayersAfter, InPrimIndex, 1);

		if (bSuccess && EditLayersAfter.Num() > 0)
		{
			SplineData.DefaultEditLayerAfter = EditLayersAfter[0];
		}
	}

	// Copy segment mesh attributes from Houdini -- vertex/point attributes
	if (!CopySegmentMeshAttributesFromHoudini(InNodeId, InPartId, HAPI_ATTROWNER_POINT, InFirstPointIndex, InNumPoints, SplineData.SegmentMeshData))
	{
		return SplineData;
	}

	// Copy segment mesh attributes from Houdini -- prim attributes
	if (!CopySegmentMeshAttributesFromHoudini(InNodeId, InPartId, HAPI_ATTROWNER_PRIM, InPrimIndex, 1, SplineData.DefaultMeshSegmentData))
	{
		return SplineData;
	}

	return SplineData;
}

bool
FHoudiniLandscapeSplineTranslator::SetControlPointData(
		ULandscapeSplineControlPoint*  InPoint,
		const FHoudiniLandscapeSplineData& InSplineData,
		const FTransform& InTransformToApply,
		int InPointIndex)
{
	if (!IsValid(InPoint))
		return false;

	// Apply the attributes from Houdini (SplineData) to the control point InPoint

	// Rotation
	if (InSplineData.PointRotations.IsValidIndex(InPointIndex * 4) && InSplineData.PointRotations.IsValidIndex(InPointIndex * 4 + 3))
	{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0)
		static constexpr float HalfPI = UE_HALF_PI;
#else
		static constexpr float HalfPI = HALF_PI;
#endif
		// Convert Houdini Y-up to UE Z-up and also Houdini Z-forward to UE X-forward
		InPoint->Rotation = (InTransformToApply.TransformRotation({
			InSplineData.PointRotations[InPointIndex * 4 + 0],
			InSplineData.PointRotations[InPointIndex * 4 + 2],
			InSplineData.PointRotations[InPointIndex * 4 + 1],
			-InSplineData.PointRotations[InPointIndex * 4 + 3]
		}) * FQuat(FVector::UpVector, HalfPI)).Rotator();
	}

	// (Paint) layer name
	if (InSplineData.PointPaintLayerNames.IsValidIndex(InPointIndex))
	{
		InPoint->LayerName = *InSplineData.PointPaintLayerNames[InPointIndex];
	}

	// bRaiseTerrain
	if (InSplineData.PointRaiseTerrains.IsValidIndex(InPointIndex))
	{
		InPoint->bRaiseTerrain = InSplineData.PointRaiseTerrains[InPointIndex];
	}

	// bLowerTerrain
	if (InSplineData.PointLowerTerrains.IsValidIndex(InPointIndex))
	{
		InPoint->bLowerTerrain = InSplineData.PointLowerTerrains[InPointIndex];
	}

	// Control point static mesh
	if (InSplineData.PointMeshRefs.IsValidIndex(InPointIndex))
	{
		if (!InSplineData.PointMeshRefs[InPointIndex].IsEmpty())
		{
			UObject* Mesh = StaticFindObject(UStaticMesh::StaticClass(), nullptr, *InSplineData.PointMeshRefs[InPointIndex]);
			if (!Mesh)
				Mesh = StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *InSplineData.PointMeshRefs[InPointIndex]);
			UStaticMesh* const SM = Mesh ? Cast<UStaticMesh>(Mesh) : nullptr;
			if (IsValid(SM))
				InPoint->Mesh = Cast<UStaticMesh>(Mesh);
			else
				InPoint->Mesh = nullptr;
		}
	}

	// Control point static mesh material overrides
	if (InSplineData.PerMaterialOverridePointRefs.Num() > 0)
	{
		InPoint->MaterialOverrides.Reset(InSplineData.PerMaterialOverridePointRefs.Num());
		for (const TArray<FString>& PerPointMaterialOverrideX : InSplineData.PerMaterialOverridePointRefs)
		{
			if (!PerPointMaterialOverrideX.IsValidIndex(InPointIndex))
				continue;
			
			const FString& MaterialRef = PerPointMaterialOverrideX[InPointIndex];
			
			UObject* Material = StaticFindObject(UMaterialInterface::StaticClass(), nullptr, *MaterialRef);
			if (!Material)
				Material = StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialRef);
			UMaterialInterface* MaterialInterface = Material ? Cast<UMaterialInterface>(Material) : nullptr;
			if (IsValid(MaterialInterface))
				InPoint->MaterialOverrides.Add(MaterialInterface);
			else
				InPoint->MaterialOverrides.Add(nullptr);
		}
		InPoint->MaterialOverrides.Shrink();
	}

	// Control point mesh scale
	if (InSplineData.PointMeshScales.IsValidIndex(InPointIndex * 3) && InSplineData.PointMeshScales.IsValidIndex(InPointIndex * 3 + 2))
	{
		InPoint->MeshScale = FVector(
			InSplineData.PointMeshScales[InPointIndex * 3 + 0],
			InSplineData.PointMeshScales[InPointIndex * 3 + 2],
			InSplineData.PointMeshScales[InPointIndex * 3 + 1]);
	}

	// Control point half-width
	if (InSplineData.PointHalfWidths.IsValidIndex(InPointIndex))
	{
		// Convert from Houdini units to UE units
		InPoint->Width = InSplineData.PointHalfWidths[InPointIndex] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
	}

	// Control point side-falloff
	if (InSplineData.PointSideFalloffs.IsValidIndex(InPointIndex))
	{
		// Convert from Houdini units to UE units
		InPoint->SideFalloff = InSplineData.PointSideFalloffs[InPointIndex] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
	}

	// Control point end-falloff
	if (InSplineData.PointEndFalloffs.IsValidIndex(InPointIndex))
	{
		// Convert from Houdini units to UE units
		InPoint->EndFalloff = InSplineData.PointEndFalloffs[InPointIndex] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
	}

	return true;
}

bool
FHoudiniLandscapeSplineTranslator::SetSegmentData(
	ULandscapeSplineSegment* InSegment, 
	const FHoudiniLandscapeSplineData& InSplineData, 
	int InVertexIndex)
{
	if (!IsValid(InSegment))
		return false;

	// Update the segment (InSegment) with the attributes copied from Houdini (InSplineData)

	// (Paint) layer name
	InSegment->LayerName = *InSplineData.DefaultPaintLayerName;
	if (InSplineData.SegmentPaintLayerNames.IsValidIndex(InVertexIndex))
		InSegment->LayerName = *InSplineData.SegmentPaintLayerNames[InVertexIndex];

	// bRaiseTerrain
	InSegment->bRaiseTerrain = InSplineData.DefaultRaiseTerrain;
	if (InSplineData.SegmentRaiseTerrains.IsValidIndex(InVertexIndex))
		InSegment->bRaiseTerrain = InSplineData.SegmentRaiseTerrains[InVertexIndex];

	// bLowerTerrain
	InSegment->bLowerTerrain = InSplineData.DefaultLowerTerrain;
	if (InSplineData.SegmentLowerTerrains.IsValidIndex(InVertexIndex))
		InSegment->bLowerTerrain = InSplineData.SegmentLowerTerrains[InVertexIndex];

	// Segment static meshes

	const TArray<FHoudiniLandscapeSplineMesh> * Meshes = nullptr;
	if (InSplineData.SegmentMeshData.IsValidIndex(InVertexIndex))
	{
		if (!InSplineData.SegmentMeshData.IsEmpty())
			Meshes = &InSplineData.SegmentMeshData[InVertexIndex].Meshes;
	}
	else if (!InSplineData.DefaultMeshSegmentData.IsEmpty())
	{
		Meshes = &InSplineData.DefaultMeshSegmentData[0].Meshes;
	}

	int NumMeshes = Meshes != nullptr ? Meshes->Num() : 0;
	for (int MeshIdx = 0; MeshIdx < NumMeshes; ++MeshIdx)
	{
		const FHoudiniLandscapeSplineMesh & InputMesh = (*Meshes)[MeshIdx];
		FLandscapeSplineMeshEntry SplineMeshEntry;
		if (!InputMesh.MeshRef.IsEmpty())
		{
			SplineMeshEntry.Mesh = Cast<UStaticMesh>(StaticFindObject(UStaticMesh::StaticClass(), nullptr, *InputMesh.MeshRef));
			if (!SplineMeshEntry.Mesh)
				SplineMeshEntry.Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *InputMesh.MeshRef));
		}

		SplineMeshEntry.Scale = InputMesh.MeshScale;
		SplineMeshEntry.CenterAdjust = InputMesh.CenterAdjust;

		for(int MaterialIdx = 0; MaterialIdx < SplineMeshEntry.MaterialOverrides.Num(); MaterialIdx++)
		{
			// Found a material override at this index, try to find / load it

			const FString& MaterialOverride = *InputMesh.MaterialOverrideRef[MaterialIdx];
			UObject* Material = StaticFindObject(UMaterialInterface::StaticClass(), nullptr, *MaterialOverride);
			if (!Material)
				Material = StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialOverride);

			UMaterialInterface* MaterialInterface = Material ? Cast<UMaterialInterface>(Material) : nullptr;
			SplineMeshEntry.MaterialOverrides.Add(MaterialInterface);
		}

		InSegment->SplineMeshes.Emplace(SplineMeshEntry);

	}

	return true;
}

bool
FHoudiniLandscapeSplineTranslator::SetConnectionData(
	FLandscapeSplineSegmentConnection& InConnection,
	int InConnectionIndex,
	const FHoudiniLandscapeSplineData& InSplineData,
	int InPointIndex)
{
	// Update the InConnection's properties from the attributes copied from Houdini.
	// Check the vertex/point attribute first, if that is not set, use the prim attribute.

	// socket name
	if (InSplineData.SegmentConnectionSocketNames[InConnectionIndex].IsValidIndex(InPointIndex))
	{
		InConnection.SocketName = *InSplineData.SegmentConnectionSocketNames[InConnectionIndex][InPointIndex];
	}
	else
	{
		InConnection.SocketName = *InSplineData.DefaultConnectionSocketNames[InConnectionIndex];
	}

	// tangent length
	if (InSplineData.SegmentConnectionTangentLengths[InConnectionIndex].IsValidIndex(InPointIndex))
	{
		InConnection.TangentLen = InSplineData.SegmentConnectionTangentLengths[InConnectionIndex][InPointIndex];
	}
	else
	{
		InConnection.TangentLen = InSplineData.DefaultConnectionTangentLengths[InConnectionIndex];
	}

	return true;
}
