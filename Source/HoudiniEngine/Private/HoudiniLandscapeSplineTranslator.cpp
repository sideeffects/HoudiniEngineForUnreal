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

/** Per mesh attribute data for a landscape spline segment. */
struct FLandscapeSplineSegmentMeshAttributes
{
	/** Mesh ref */
	bool bHasMeshRefAttribute = false;
	TArray<FString> MeshRef;
	
	/** Mesh material override, the outer index is material 0, 1, 2 ... */
	TArray<TArray<FString>> MeshMaterialOverrideRefs;

	/** Mesh scale. */
	bool bHasMeshScaleAttribute = false;
	TArray<float> MeshScale;

	/** Center Adjust. */
	bool bHasMeshCenterAdjustAttribute = false;
	TArray<float> CenterAdjust;
};


/** Attribute data extracted from curve points and prims. */
struct FLandscapeSplineCurveAttributes
{
	//
	// Point Attributes
	//
	
	/** Resampled point positions. */
	TArray<float> PointPositions;
	
	/** Point rotations. */
	bool bHasPointRotationAttribute = false;
	TArray<float> PointRotations;
	
	/** Point paint layer names */
	bool bHasPointPaintLayerNameAttribute = false;
	TArray<FString> PointPaintLayerNames;
	
	/** Point bRaiseTerrain */
	bool bHasPointRaiseTerrainAttribute = false;
	TArray<int32> PointRaiseTerrains;
	
	/** Point bLowerTerrain */
	bool bHasPointLowerTerrainAttribute = false;
	TArray<int32> PointLowerTerrains;

	/** The StaticMesh ref per point. */
	bool bHasPointMeshRefAttribute = false;
	TArray<FString> PointMeshRefs;

	/**
	 * The Material Override refs of each point. The outer index material override index, the inner index
	 * is point index.
	 */
	TArray<TArray<FString>> PerMaterialOverridePointRefs;

	/** The static mesh scale of each point. */
	bool bHasPointMeshScaleAttribute = false;
	TArray<float> PointMeshScales;

	/** The ids of the control points. */
	bool bHasPointIdAttribute = false;
	TArray<int32> PointIds;

	/** The point half-width. */
	bool bHasPointHalfWidthAttribute = false;
	TArray<float> PointHalfWidths;

	/** The point side-falloff. */
	bool bHasPointSideFalloffAttribute = false;
	TArray<float> PointSideFalloffs;

	/** The point end-falloff. */
	bool bHasPointEndFalloffAttribute = false;
	TArray<float> PointEndFalloffs;

	//
	// Although the following properties are named Vertex... they are point attributes in HAPI (but are intended to be
	// authored as vertex attributes in Houdini). When curves are extracted via HAPI points and vertices are the same.
	//
	
	/**
	 * The mesh socket names on the splines' vertices. The outer index is the near side (0) and far side (1) of the
	 * segment connection. The inner index is a vertex index.
	 */
	bool bHasVertexConnectionSocketNameAttribute[2] { false, false };
	TArray<FString> VertexConnectionSocketNames[2];

	/**
	 * Tangent length point attribute, for segment connections. The outer index is the near side (0) and far side (1)
	 * of the segment connection. The inner index is a vertex index.
	 */
	bool bHasVertexConnectionTangentLengthAttribute[2] { false, false };
	TArray<float> VertexConnectionTangentLengths[2];

	/** Vertex/segment paint layer name */
	bool bHasVertexPaintLayerNameAttribute = false;
	TArray<FString> VertexPaintLayerNames;

	/** Vertex/segment bRaiseTerrain */
	bool bHasVertexRaiseTerrainAttribute = false;
	TArray<int32> VertexRaiseTerrains;

	/** Vertex/segment bLowerTerrain */
	bool bHasVertexLowerTerrainAttribute = false;
	TArray<int32> VertexLowerTerrains;

	/** Edit layer name */
	bool bHasVertexEditLayerAttribute = false;
	TArray<FString> VertexEditLayers;

	/** Edit layer: clear */
	bool bHasVertexEditLayerClearAttribute = false;
	TArray<int32> VertexEditLayersClear;

	/** Edit layer: add after */
	bool bHasVertexEditLayerAfterAttribute = false;
	TArray<FString> VertexEditLayersAfter;

	/** Static mesh attributes on vertices. Outer index is mesh 0, 1, 2 ... */
	TArray<FLandscapeSplineSegmentMeshAttributes> VertexPerMeshSegmentData;

	//
	// Primitive attributes
	//
	
	/**
	 * The mesh socket names on the splines' prims. The index is the near side (0) and far side (1) of the
	 * segment connection.
	 */
	bool bHasPrimConnectionSocketNameAttribute[2] { false, false };
	FString PrimConnectionSocketNames[2];

	/**
	 * Tangent length point attribute, for segment connections. The index is the near side (0) and far side (1)
	 * of the segment connection.
	 */
	bool bHasPrimConnectionTangentLengthAttribute[2] { false, false };
	float PrimConnectionTangentLengths[2];

	/** Prim/segment paint layer name */
	bool bHasPrimPaintLayerNameAttribute = false;
	FString PrimPaintLayerName;

	/** Prim/segment bRaiseTerrain */
	bool bHasPrimRaiseTerrainAttribute = false;
	int32 bPrimRaiseTerrain;

	/** Prim/segment bLowerTerrain */
	bool bHasPrimLowerTerrainAttribute = false;
	int32 bPrimLowerTerrain;

	/** Edit layer name */
	bool bHasPrimEditLayerAttribute = false;
	FString PrimEditLayer;

	/** Edit layer: clear */
	bool bHasPrimEditLayerClearAttribute = false;
	bool bPrimEditLayerClear;

	/** Edit layer: add after */
	bool bHasPrimEditLayerAfterAttribute = false;
	FString PrimEditLayerAfter;

	/** Static mesh attribute from primitives, the index is mesh 0, 1, 2 ... */
	TArray<FLandscapeSplineSegmentMeshAttributes> PrimPerMeshSegmentData;
};

/**
 * Transient/transactional struct for processing landscape spline output. Used in
 * CreateOutputLandscapeSplinesFromHoudiniGeoPartObject(). This is not a UStruct / does n0t use UProperties. We do not
 * intend to store this struct anywhere, its entire lifecycle is during calls to
 * CreateOutputLandscapeSplinesFromHoudiniGeoPartObject().
 */
struct FLandscapeSplineInfo
{
	/**
	 * True for valid entries. Invalid entries would be those where Landscape, LandscapeInfo or SplinesComponent is
	 * null / invalid.
	 */
	bool bIsValid = false;

	/** Output object identifier for this landscape splines component / actor. */
	FHoudiniOutputObjectIdentifier Identifier;

	/** True if we are going to reuse a previously create landscape spline component / actor. */
	bool bReusedPreviousOutput = false;
	
	/** The landscape proxy that owns the spline. */
	ALandscapeProxy* LandscapeProxy = nullptr;

	/** The landscape actor that owns the spline. */
	ALandscape* Landscape = nullptr;

	/** The landscape info for the landscape */
	ULandscapeInfo* LandscapeInfo = nullptr;

	/** Custom output name, if applicable (WP only). */
	FName OutputName = NAME_None;

	/** Package params for temp layers. */
	FHoudiniPackageParams LayerPackageParams;

	/** Package params for spline actor. */
	FHoudiniPackageParams SplineActorPackageParams;
	
	/** The landscape spline actor, if applicable (WP only). */
	ALandscapeSplineActor* LandscapeSplineActor = nullptr;

	/** The splines component. */
	ULandscapeSplinesComponent* SplinesComponent = nullptr;

	/**
	 * Array of curve indices in the HGPO that will be used to create segments for this landscape spline. There can
	 * be more than one segment per curve.
	 */
	TArray<int32> CurveIndices;

	/**
	 * An array per-curve that stores the index of the first point (corresponding to the P attribute) for the curve
	 * info in the HGPO.
	 */
	TArray<int32> PerCurveFirstPointIndex;

	/** An array per-curve that stores the number of points for the curve in the HGPO. */
	TArray<int32> PerCurvePointCount;

	/** Curve prim and point attributes read from Houdini to apply to ULandscapeSplineControlPoint/Segment. */
	TArray<FLandscapeSplineCurveAttributes> CurveAttributes;

	/** Control points mapped by id that have been created for this splines component. */
	TMap<int32, ULandscapeSplineControlPoint*> ControlPointMap;

	/** The next control point ID (largest ID seen + 1) */
	int32 NextControlPointId = 0;

	/** Object used to keep track of segments/control points that we create for the FHoudiniOutputObject. */
	UHoudiniLandscapeSplinesOutput* SplinesOutputObject = nullptr;
};


FVector
ConvertPositionToVector(const float* InPosition)
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
	UHoudiniOutput* const InOutput,
	const TArray<ALandscapeProxy*>& InAllInputLandscapes,
	UWorld* const InWorld,
	const FHoudiniPackageParams& InPackageParams,
	TMap<ALandscape*, TSet<FName>>& ClearedLayers)
{
	if (!IsValid(InOutput))
		return false;

	if (!IsValid(InWorld))
		return false;

	// Only run on landscape spline inputs
	if (InOutput->GetType() != EHoudiniOutputType::LandscapeSpline)
		return false;

	UHoudiniAssetComponent* const HAC = FHoudiniEngineUtils::GetOuterHoudiniAssetComponent(InOutput);

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
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OldOutputObjects = InOutput->GetOutputObjects();
	// Iterate on all the output's HGPOs
	for (const FHoudiniGeoPartObject& CurHGPO : InOutput->GetHoudiniGeoPartObjects())
	{
		// Skip any HGPO that is not a landscape spline
		if (CurHGPO.Type != EHoudiniPartType::LandscapeSpline)
			continue;

		// Create / update landscape splines from this HGPO
		static constexpr bool bForceRebuild = false;
		CreateOutputLandscapeSplinesFromHoudiniGeoPartObject(
			CurHGPO,
			InOutput,
			InAllInputLandscapes,
			InWorld,
			InPackageParams,
			OldOutputObjects,
			bForceRebuild,
			FallbackLandscape,
			ClearedLayers,
			SegmentsToApplyToLayers,
			NewOutputObjects,
			HAC);
	}

	// Apply splines to user specified edit layers and reserved spline layers
	FHoudiniLandscapeUtils::ApplySegmentsToLandscapeEditLayers(SegmentsToApplyToLayers);

	// The old map now only contains unused/stale output landscape splines: destroy them
	for (auto& OldPair : OldOutputObjects)
	{
		FHoudiniOutputObject& OldOutputObject = OldPair.Value;

		// Destroy any segments that we previously created
		UHoudiniLandscapeSplinesOutput* const SplinesOutputObject = Cast<UHoudiniLandscapeSplinesOutput>(OldOutputObject.OutputObject);
		if (IsValid(SplinesOutputObject))
		{
			// We already cleared all temp layers from this output at the beginning of this function
			static constexpr bool bClearTempLayers = false;
			SplinesOutputObject->Clear(bClearTempLayers);
		}

		OldOutputObject.OutputActors.Empty();
	}
	OldOutputObjects.Empty();

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
	const FLandscapeSplineInfo& InSplineInfo,
	TMap<ALandscape*, TSet<FName>>& ClearedLayers,
	TMap<TTuple<ALandscape*, FName>, FHoudiniLandscapeSplineApplyLayerData>& SegmentsToApplyToLayers)
{
	if (!IsValid(InSplineInfo.Landscape))
		return;

	if (!InSplineInfo.Landscape->CanHaveLayersContent())
		return;

	TSet<FName>& ClearedLayersForLandscape = ClearedLayers.FindOrAdd(InSplineInfo.Landscape);

	// If the landscape has a reserved splines layer, then we don't have track the segments to apply. Just record the
	// landscape with its reserved layer.	
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	const FLandscapeLayer* ReservedSplinesLayer = InSplineInfo.Landscape->FindLayerOfType(ULandscapeEditLayerSplines::StaticClass());
#else
	FLandscapeLayer* const ReservedSplinesLayer = InSplineInfo.Landscape->GetLandscapeSplinesReservedLayer();
#endif
	if (ReservedSplinesLayer)
	{
		FHoudiniLandscapeSplineApplyLayerData& LayerData = SegmentsToApplyToLayers.FindOrAdd({ InSplineInfo.Landscape, ReservedSplinesLayer->Name});
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
		FHoudiniLandscapeSplineApplyLayerData& LayerData = SegmentsToApplyToLayers.FindOrAdd({ InSplineInfo.Landscape, CookedEditLayer });
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
	ULandscapeSplineSegment* const InSegment,
	const FLandscapeSplineCurveAttributes& InAttributes,
	const int32 InVertexIndex,
	UHoudiniAssetComponent* const InHAC,
	const FHoudiniPackageParams& InPackageParams,
	UHoudiniLandscapeSplinesOutput& InOutputObject)
{
	if (!IsValid(InSegment))
		return;

	InOutputObject.GetSegments().Add(InSegment);

	// Check for edit layer attributes, for each check vertex first, then prim

	FName EditLayerName = NAME_None;
	// Edit layer name
	if (InAttributes.bHasVertexEditLayerAttribute && InAttributes.VertexEditLayers.IsValidIndex(InVertexIndex))
	{
		EditLayerName = *InAttributes.VertexEditLayers[InVertexIndex];
	}
	else if (InAttributes.bHasPrimEditLayerAttribute)
	{
		EditLayerName = *InAttributes.PrimEditLayer;
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
		if (InAttributes.bHasVertexEditLayerClearAttribute && InAttributes.VertexEditLayersClear.IsValidIndex(InVertexIndex))
		{
			LayerOutput->bClearLayer = static_cast<bool>(InAttributes.VertexEditLayersClear[InVertexIndex]);
		}
		else if (InAttributes.bHasPrimEditLayerClearAttribute)
		{
			LayerOutput->bClearLayer = InAttributes.bPrimEditLayerClear;
		}
		else
		{
			LayerOutput->bClearLayer = false;
		}

		// Edit layer create after
		if (InAttributes.bHasVertexEditLayerAfterAttribute && InAttributes.VertexEditLayersAfter.IsValidIndex(InVertexIndex))
		{
			LayerOutput->AfterEditLayer = *InAttributes.VertexEditLayersAfter[InVertexIndex];
		}
		else if (InAttributes.bHasPrimEditLayerAfterAttribute)
		{
			LayerOutput->AfterEditLayer = *InAttributes.PrimEditLayerAfter;
		}
		else
		{
			LayerOutput->AfterEditLayer = NAME_None;
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
FHoudiniLandscapeSplineTranslator::CreateOutputLandscapeSplinesFromHoudiniGeoPartObject(
	const FHoudiniGeoPartObject& InHGPO,
	UHoudiniOutput* const InOutput,
	const TArray<ALandscapeProxy*>& InAllInputLandscapes,
	UWorld* const InWorld,
	const FHoudiniPackageParams& InPackageParams,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InCurrentSplines,
	const bool bInForceRebuild,
	ALandscapeProxy* InFallbackLandscape,
	TMap<ALandscape*, TSet<FName>>& ClearedLayers,
	TMap<TTuple<ALandscape*, FName>, FHoudiniLandscapeSplineApplyLayerData>& SegmentsToApplyToLayers,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputSplines,
	UHoudiniAssetComponent* const InHAC)
{
#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
	HOUDINI_LOG_WARNING(TEXT("Landscape Spline Output is only supported in UE5.1+"));
	return false;
#else

	if (!IsValid(InWorld))
		return false;

	const int32 CurveNodeId = InHGPO.GeoId;
	const int32 CurvePartId = InHGPO.PartId;
	if (CurveNodeId < 0 || CurvePartId < 0)
		return false;

	const FTransform HACTransform = IsValid(InHAC) ? InHAC->GetComponentTransform() : FTransform::Identity;

	// Find the fallback landscape to use, either InFallbackLandscape if valid, otherwise the first one we find in the
	// world
	const bool bIsUsingWorldPartition = IsValid(InWorld->GetWorldPartition());
	ALandscapeProxy* FallbackLandscape = InFallbackLandscape;
	if (!IsValid(FallbackLandscape))
	{
		TActorIterator<ALandscapeProxy> LandscapeIt(InWorld, ALandscapeProxy::StaticClass());
		if (LandscapeIt)
			FallbackLandscape = *LandscapeIt;
	}

	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	if (!Session)
		return false;

	// Get the curve info from HAPI
	HAPI_CurveInfo CurveInfo;
	FHoudiniApi::CurveInfo_Init(&CurveInfo);
	FHoudiniApi::GetCurveInfo(Session, CurveNodeId, CurvePartId, &CurveInfo);

	// Get the point/vertex count for each curve primitive
	int32 NumCurves = CurveInfo.curveCount;
	TArray<int32> CurvePointCounts;
	CurvePointCounts.SetNumZeroed(NumCurves);
	FHoudiniApi::GetCurveCounts(Session, CurveNodeId, CurvePartId, CurvePointCounts.GetData(), 0, NumCurves);

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
	HAPI_AttributeInfo AttrLandscapeRefs;
	FHoudiniApi::AttributeInfo_Init(&AttrLandscapeRefs);
	FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		CurveNodeId, CurvePartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_TARGET_LANDSCAPE, AttrLandscapeRefs, LandscapeRefs, 1, HAPI_ATTROWNER_PRIM);

	// Extract all custom output name as prim attributes (used for landscape spline actor names in WP, not applicable to non-WP).
	TArray<FString> OutputNames;
	if (bIsUsingWorldPartition)
	{
		HAPI_AttributeInfo AttrOutputNames;
		FHoudiniApi::AttributeInfo_Init(&AttrOutputNames);
		FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			CurveNodeId, CurvePartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2, AttrOutputNames, OutputNames, 1, HAPI_ATTROWNER_PRIM);
	}

	// Iterate over curves first, use prim attributes to find the landscape that the splines should be attached to,
	// and for world partition look at unreal_output_name to determine the landscape spline actor name.
	TMap<FString, FLandscapeSplineInfo> LandscapeSplineInfos;
	LandscapeSplineInfos.Reserve(NumCurves);
	for (int32 CurveIdx = 0, NextCurveStartPointIdx = 0; CurveIdx < NumCurves; ++CurveIdx)
	{
		const int32 NumPointsInCurve = CurvePointCounts[CurveIdx];
		NextCurveStartPointIdx += NumPointsInCurve;
		
		// Determine the name (or NAME_None in non-WP)
		FName OutputName = NAME_None;
		if (bIsUsingWorldPartition && OutputNames.IsValidIndex(CurveIdx))
		{
			OutputName = *OutputNames[CurveIdx];
		}

		// Use the landscape specified with the landscape target attribute
		FString LandscapeRef;
		ALandscapeProxy* TargetLandscape = nullptr;
		if (LandscapeRefs.IsValidIndex(CurveIdx))
		{
			LandscapeRef = LandscapeRefs[CurveIdx];
			TargetLandscape = FHoudiniLandscapeUtils::FindTargetLandscapeProxy(LandscapeRef, InWorld, InAllInputLandscapes);
		}

		// Use fallback landscape if TargetLandscape could not be found / is invalid
		if (!IsValid(TargetLandscape))
			TargetLandscape = FallbackLandscape;

		// If at this point we don't have a valid target landscape then we cannot proceed, return false
		if (!IsValid(TargetLandscape))
		{
			HOUDINI_LOG_ERROR(
				TEXT("Could not find target landscape: '%s', and also could not find a "
					 "fallback landscape from the HAC or World."), *LandscapeRef);
			return false;
		}

		const FString IdentifierName = FString::Printf(
			TEXT("%s-%s-%s"), *InHGPO.PartName, *TargetLandscape->GetFName().ToString(), *OutputName.ToString());
		FHoudiniOutputObjectIdentifier Identifier(InHGPO.ObjectId, InHGPO.GeoId, InHGPO.PartId, IdentifierName);

		// Get/create the FLandscapeSplineInfo entry that we use to manage the data for each
		// ULandscapeSplinesComponent / ALandscapeSplineActor that we will output to
		FLandscapeSplineInfo* SplineInfo = LandscapeSplineInfos.Find(IdentifierName);
		if (!SplineInfo)
		{
			SplineInfo = &LandscapeSplineInfos.Add(IdentifierName);
			SplineInfo->bIsValid = false;
			SplineInfo->Identifier = Identifier;
			SplineInfo->OutputName = OutputName;

			// Initialize NextControlPointId to 0. For each curve primitive added to this SplineInfo we will increase
			// NextControlPointId based on the control point ids of the curve primitive, so that NextControlPointId
			// is greater than all of the control point ids of all curves in the SplineInfo
			SplineInfo->NextControlPointId = 0;

			// Configure package params for spline actors and temp layers
			SplineInfo->LayerPackageParams = InPackageParams;
			SplineInfo->LayerPackageParams.ObjectId = Identifier.ObjectId;
			SplineInfo->LayerPackageParams.GeoId = Identifier.GeoId;
			SplineInfo->LayerPackageParams.PartId = Identifier.PartId;
			
			SplineInfo->SplineActorPackageParams = SplineInfo->LayerPackageParams;
			SplineInfo->SplineActorPackageParams.SplitStr = OutputName.ToString();

			FHoudiniOutputObject* FoundOutputObject = InCurrentSplines.Find(Identifier);

			SplineInfo->LandscapeProxy = TargetLandscape;

			// Get the landscape info from our target landscape (if valid)
			const bool bIsLandscapeProxyValid = IsValid(SplineInfo->LandscapeProxy);
			bool bIsLandscapeInfoValid = false;
			if (bIsLandscapeProxyValid)
			{
				SplineInfo->LandscapeInfo = SplineInfo->LandscapeProxy->GetLandscapeInfo();
				bIsLandscapeInfoValid = IsValid(SplineInfo->LandscapeInfo);
				SplineInfo->Landscape = SplineInfo->LandscapeProxy->GetLandscapeActor();
			}

			// Depending if the world is using world partition we need to create a landscape spline actor, or manipulate
			// the landscape splines component on the landscape directly (non-world partition)
			if (bIsUsingWorldPartition)
			{
				if (bIsLandscapeInfoValid)
				{
					// Check if we found a matching Output Object, and check if it already has a landscape spline actor.
					// That actor must belong to SplineInfo->Landscape/Info. If it does not then we won't reuse that
					// output object
					if (FoundOutputObject && IsValid(FoundOutputObject->OutputObject)
							&& FoundOutputObject->OutputObject->IsA<UHoudiniLandscapeSplinesOutput>())
					{
						UHoudiniLandscapeSplinesOutput* const SplinesOutputObject = Cast<UHoudiniLandscapeSplinesOutput>(
							FoundOutputObject->OutputObject);
						ALandscapeSplineActor* const CurrentActor = SplinesOutputObject->GetLandscapeSplineActor();
						if (IsValid(CurrentActor) && CurrentActor->GetLandscapeInfo() == SplineInfo->LandscapeInfo)
						{
							SplineInfo->LandscapeSplineActor = CurrentActor;
							SplineInfo->bReusedPreviousOutput = true;
						}
					}
					
					if (!SplineInfo->LandscapeSplineActor)
					{
						SplineInfo->LandscapeSplineActor = SplineInfo->LandscapeInfo->CreateSplineActor(FVector::ZeroVector);
						// Name the actor according to OutputName via PackageParams.
						FHoudiniEngineUtils::SafeRenameActor(SplineInfo->LandscapeSplineActor, SplineInfo->SplineActorPackageParams.GetPackageName());
					}
					
					if (IsValid(SplineInfo->LandscapeSplineActor))
					{
						// Set / update the transform of the landscape spline actor
						SplineInfo->LandscapeSplineActor->SetActorTransform(HACTransform);
						SplineInfo->SplinesComponent = SplineInfo->LandscapeSplineActor->GetSplinesComponent();
					}
				}
			}
			else if (bIsLandscapeProxyValid)
			{
				SplineInfo->SplinesComponent = SplineInfo->LandscapeProxy->GetSplinesComponent();
				if (!IsValid(SplineInfo->SplinesComponent))
				{
					SplineInfo->LandscapeProxy->CreateSplineComponent();
					SplineInfo->SplinesComponent = SplineInfo->LandscapeProxy->GetSplinesComponent();
				}
				else
				{
					// Check if we are re-using the splines component 
					if (FoundOutputObject && IsValid(FoundOutputObject->OutputObject)
						&& FoundOutputObject->OutputObject->IsA<UHoudiniLandscapeSplinesOutput>()) 
					{
						UHoudiniLandscapeSplinesOutput* const SplinesOutputObject = Cast<UHoudiniLandscapeSplinesOutput>(
							FoundOutputObject->OutputObject);
						if (SplinesOutputObject->GetLandscapeSplinesComponent() == SplineInfo->SplinesComponent)
							SplineInfo->bReusedPreviousOutput = true;
					}
				}
			}

			SplineInfo->bIsValid = IsValid(SplineInfo->SplinesComponent);

			if (!SplineInfo->bReusedPreviousOutput || !FoundOutputObject)
			{
				// If we are not re-using the previous output object with this identifier, record / create it as a new one.

				FHoudiniOutputObject OutputObject;

				// Set the output object in the SplineInfo so that we can record segments and control points as we
				// create them
				SplineInfo->SplinesOutputObject = NewObject<UHoudiniLandscapeSplinesOutput>(InOutput);
				OutputObject.OutputObject = SplineInfo->SplinesOutputObject;

				OutputSplines.Add(SplineInfo->Identifier, OutputObject);
			}
			else
			{
				// Set the output object in the SplineInfo so that we can cleanup old segments / control points and
				// record segments and control points as we create them
				SplineInfo->SplinesOutputObject = Cast<UHoudiniLandscapeSplinesOutput>(FoundOutputObject->OutputObject);
				// If the SplinesOutputObject is not valid, create one
				if (!IsValid(SplineInfo->SplinesOutputObject))
				{
					SplineInfo->SplinesOutputObject = NewObject<UHoudiniLandscapeSplinesOutput>(
						InOutput, UHoudiniLandscapeSplinesOutput::StaticClass());
					FoundOutputObject->OutputObject = SplineInfo->SplinesOutputObject;
				}

				// Re-use the FoundOutputObject
				OutputSplines.Add(SplineInfo->Identifier, *FoundOutputObject);
			}

			// Update the objects on the SplinesOutputObject to match the SplineInfo 
			SplineInfo->SplinesOutputObject->SetLandscapeProxy(SplineInfo->LandscapeProxy);
			SplineInfo->SplinesOutputObject->SetLandscape(SplineInfo->Landscape);
			SplineInfo->SplinesOutputObject->SetLandscapeSplinesComponent(SplineInfo->SplinesComponent);
			SplineInfo->SplinesOutputObject->SetLandscapeSplineActor(
				bIsUsingWorldPartition ? SplineInfo->LandscapeSplineActor : nullptr);
		}

		if (!SplineInfo->bIsValid)
			continue;

		// Add the primitive and point indices of this curve to the SplineInfo 
		SplineInfo->CurveIndices.Add(CurveIdx);
		SplineInfo->PerCurvePointCount.Add(CurvePointCounts[CurveIdx]);
		const int32 CurveFirstPointIndex = NextCurveStartPointIdx - NumPointsInCurve;
		SplineInfo->PerCurveFirstPointIndex.Add(CurveFirstPointIndex);

		// Copy the attributes for this curve primitive from Houdini / HAPI
		CopyCurveAttributesFromHoudini(
			CurveNodeId,
			CurvePartId,
			CurveIdx,
			CurveFirstPointIndex,
			NumPointsInCurve,
			SplineInfo->CurveAttributes.AddDefaulted_GetRef());

		// Ensure that NextControlPointId is greater than all of the PointIds from this curve
		FLandscapeSplineCurveAttributes& CurveAttributes = SplineInfo->CurveAttributes.Last();
		for (const int32 ControlPointId : CurveAttributes.PointIds)
		{
			if (ControlPointId >= SplineInfo->NextControlPointId)
				SplineInfo->NextControlPointId = ControlPointId + 1;
		}
	}
	
	// Fetch generic attributes
	TArray<FHoudiniGenericAttribute> GenericPointAttributes;
	const bool bHasGenericPointAttributes = (FHoudiniEngineUtils::GetGenericAttributeList(
		InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, GenericPointAttributes, HAPI_ATTROWNER_POINT) > 0);
	TArray<FHoudiniGenericAttribute> GenericPrimAttributes;
	const bool bHasGenericPrimAttributes = (FHoudiniEngineUtils::GetGenericAttributeList(
		InHGPO.GeoId, InHGPO.PartId, HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, GenericPrimAttributes, HAPI_ATTROWNER_PRIM) > 0);

	// Process each SplineInfo entry 
	for (auto& Entry : LandscapeSplineInfos)
	{
		FLandscapeSplineInfo& SplineInfo = Entry.Value;
		if (!SplineInfo.bIsValid)
			continue;

		// If we are reusing the spline component, clear all segments and control points we created first
		if (SplineInfo.bReusedPreviousOutput)
		{
			FHoudiniLandscapeRuntimeUtils::DestroyLandscapeSplinesSegmentsAndControlPoints(SplineInfo.SplinesOutputObject);
			// Now that we destroyed the segments and control points, we can remove our layer outputs
			// and recreate based on the attributes found on the curve geo
			SplineInfo.SplinesOutputObject->GetLayerOutputs().Empty();
		}

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
		const int32 NumCurvesInSpline = SplineInfo.PerCurveFirstPointIndex.Num();
		for (int32 CurveEntryIdx = 0; CurveEntryIdx < NumCurvesInSpline; ++CurveEntryIdx)
		{
			const FLandscapeSplineCurveAttributes& Attributes = SplineInfo.CurveAttributes[CurveEntryIdx];
			ULandscapeSplineControlPoint* PreviousControlPoint = nullptr;
			int32 PreviousControlPointArrayIdx = INDEX_NONE;

			const int32 NumPointsInCurve = SplineInfo.PerCurvePointCount[CurveEntryIdx];
			for (int32 CurvePointArrayIdx = 0; CurvePointArrayIdx < NumPointsInCurve; ++CurvePointArrayIdx)
			{
				const int32 HGPOPointIndex = SplineInfo.PerCurveFirstPointIndex[CurveEntryIdx] + CurvePointArrayIdx;
				
				// Check if this is a control point: it has a control point id attribute >= 0, or is the first or last
				// point of the curve prim.
				int32 ControlPointId = INDEX_NONE;
				if (Attributes.bHasPointIdAttribute && Attributes.PointIds.IsValidIndex(CurvePointArrayIdx))
				{
					ControlPointId = Attributes.PointIds[CurvePointArrayIdx];
					if (ControlPointId < 0)
						ControlPointId = INDEX_NONE;
				}

				bool bControlPointCreated = false;
				ULandscapeSplineControlPoint* ThisControlPoint = nullptr;
				// A point is a control point if:
				// 1. It is the first or last point of the curve, or
				// 2. It has a valid (>=0) control point id, or
				// 3. The control point id attribute does not exist
				if (!PreviousControlPoint || CurvePointArrayIdx == NumPointsInCurve - 1 || ControlPointId >= 0 || !Attributes.bHasPointIdAttribute)
					ThisControlPoint = GetOrCreateControlPoint(SplineInfo, ControlPointId, bControlPointCreated);

				if (bControlPointCreated && IsValid(ThisControlPoint))
				{
					SplineInfo.SplinesOutputObject->GetControlPoints().Add(ThisControlPoint);
					ControlPoints.Add(ThisControlPoint);
					ThisControlPoint->Location = TransformToApply.TransformPosition(
						ConvertPositionToVector(&Attributes.PointPositions[CurvePointArrayIdx * 3]));

					// Update generic properties attributes on the control point
					if (bHasGenericPointAttributes)
						FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(ThisControlPoint, GenericPointAttributes, HGPOPointIndex);

					// Apply point attributes
					UpdateControlPointFromAttributes(ThisControlPoint, Attributes, TransformToApply, CurvePointArrayIdx);
				}

				// If we have two control points, create a segment
				if (PreviousControlPoint && ThisControlPoint)
				{
					// Create the segment
					ULandscapeSplineSegment* Segment = NewObject<ULandscapeSplineSegment>(
						SplineInfo.SplinesComponent, ULandscapeSplineSegment::StaticClass());
					Segment->Connections[0].ControlPoint = PreviousControlPoint;
					Segment->Connections[1].ControlPoint = ThisControlPoint;

					// Update generic properties attributes on the segment
					if (bHasGenericPointAttributes)
						FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(Segment, GenericPointAttributes, SplineInfo.PerCurveFirstPointIndex[CurveEntryIdx]);
					if (bHasGenericPrimAttributes)
						FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(Segment, GenericPrimAttributes, SplineInfo.CurveIndices[CurveEntryIdx]);

					// Apply attributes to segment
					UpdateSegmentFromAttributes(Segment, Attributes, CurvePointArrayIdx);

					// Apply attributes for connections
					UpdateConnectionFromAttributes(Segment->Connections[0], 0, Attributes, PreviousControlPointArrayIdx);
					UpdateConnectionFromAttributes(Segment->Connections[1], 1, Attributes, CurvePointArrayIdx);
					
					FVector StartLocation; FRotator StartRotation;
					PreviousControlPoint->GetConnectionLocationAndRotation(Segment->Connections[0].SocketName, StartLocation, StartRotation);
					FVector EndLocation; FRotator EndRotation;
					ThisControlPoint->GetConnectionLocationAndRotation(Segment->Connections[1].SocketName, EndLocation, EndRotation);

					// Set up tangent lengths if not in vertex/prim connection attributes
					if (!(Attributes.bHasVertexConnectionTangentLengthAttribute[0] && Attributes.VertexConnectionTangentLengths[0].IsValidIndex(PreviousControlPointArrayIdx))
						|| !(Attributes.bHasPrimConnectionTangentLengthAttribute[0] && Attributes.PrimConnectionTangentLengths[0]))
					{
						Segment->Connections[0].TangentLen = (EndLocation - StartLocation).Size();
					}
					if (!(Attributes.bHasVertexConnectionTangentLengthAttribute[1] && Attributes.VertexConnectionTangentLengths[1].IsValidIndex(CurvePointArrayIdx))
						|| !(Attributes.bHasPrimConnectionTangentLengthAttribute[1] && Attributes.PrimConnectionTangentLengths[1]))
					{
						Segment->Connections[1].TangentLen = Segment->Connections[0].TangentLen;
					}

					Segment->AutoFlipTangents();
					
					PreviousControlPoint->ConnectedSegments.Add(FLandscapeSplineConnection(Segment, 0));
					ThisControlPoint->ConnectedSegments.Add(FLandscapeSplineConnection(Segment, 1));

					// Auto-calculate rotation if we didn't receive rotation attributes
					if (!Attributes.bHasPointRotationAttribute || !Attributes.PointRotations.IsValidIndex(PreviousControlPointArrayIdx))
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
						PreviousControlPoint->AutoCalcRotation(true);
#else
						PreviousControlPoint->AutoCalcRotation();
#endif
					if (!Attributes.bHasPointRotationAttribute || !Attributes.PointRotations.IsValidIndex(CurvePointArrayIdx)) 
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
						ThisControlPoint->AutoCalcRotation(true);
#else
						ThisControlPoint->AutoCalcRotation();
#endif

					// Add the segment to the appropriate LayerOutput. Will create create the LayerOutput if necessary.
					AddSegmentToOutputObject(Segment, Attributes, CurvePointArrayIdx, InHAC, SplineInfo.LayerPackageParams, *SplineInfo.SplinesOutputObject);
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
		
		FHoudiniOutputObject* const OutputObject = OutputSplines.Find(SplineInfo.Identifier);
		
		// Cache commonly supported Houdini attributes on the OutputAttributes
		TArray<FString> LevelPaths;
		if (OutputObject && FHoudiniEngineUtils::GetLevelPathAttribute(
			InHGPO.GeoId, InHGPO.PartId, LevelPaths, HAPI_ATTROWNER_INVALID, 0, 1))
		{
			if (LevelPaths.Num() > 0 && !LevelPaths[0].IsEmpty())
			{
				// cache the level path attribute on the output object
				OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_LEVEL_PATH, LevelPaths[0]);
			}
		}

		// cache the output name attribute on the output object
		OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2, SplineInfo.OutputName.ToString());

		const int32 FirstCurvePrimIndex = SplineInfo.CurveIndices.Num() > 0 ? SplineInfo.CurveIndices[0] : INDEX_NONE;
		
		TArray<FString> BakeNames;
		if (OutputObject && FirstCurvePrimIndex != INDEX_NONE && FHoudiniEngineUtils::GetBakeNameAttribute(
			InHGPO.GeoId, InHGPO.PartId, BakeNames, HAPI_ATTROWNER_PRIM, FirstCurvePrimIndex, 1))
		{
			if (BakeNames.Num() > 0 && !BakeNames[0].IsEmpty())
			{
				// cache the output name attribute on the output object
				OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_NAME, BakeNames[0]);
			}
		}

		TArray<FString> BakeOutputActorNames;
		if (OutputObject && FirstCurvePrimIndex != INDEX_NONE && FHoudiniEngineUtils::GetBakeActorAttribute(
			InHGPO.GeoId, InHGPO.PartId, BakeOutputActorNames, HAPI_ATTROWNER_PRIM, FirstCurvePrimIndex, 1))
		{
			if (BakeOutputActorNames.Num() > 0 && !BakeOutputActorNames[0].IsEmpty())
			{
				// cache the bake actor attribute on the output object
				OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_ACTOR, BakeOutputActorNames[0]);
			}
		}

		TArray<FString> BakeOutputActorClassNames;
		if (OutputObject && FirstCurvePrimIndex != INDEX_NONE && FHoudiniEngineUtils::GetBakeActorClassAttribute(
			InHGPO.GeoId, InHGPO.PartId, BakeOutputActorClassNames, HAPI_ATTROWNER_PRIM, FirstCurvePrimIndex, 1))
		{
			if (BakeOutputActorClassNames.Num() > 0 && !BakeOutputActorClassNames[0].IsEmpty())
			{
				// cache the bake actor attribute on the output object
				OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_ACTOR_CLASS, BakeOutputActorClassNames[0]);
			}
		}

		TArray<FString> BakeFolders;
		if (OutputObject && FHoudiniEngineUtils::GetBakeFolderAttribute(
			InHGPO.GeoId, BakeFolders, InHGPO.PartId, 0, 1))
		{
			if (BakeFolders.Num() > 0 && !BakeFolders[0].IsEmpty())
			{
				// cache the unreal_bake_folder attribute on the output object
				OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_FOLDER, BakeFolders[0]);
			}
		}

		TArray<FString> BakeOutlinerFolders;
		if (OutputObject && FirstCurvePrimIndex != INDEX_NONE && FHoudiniEngineUtils::GetBakeOutlinerFolderAttribute(
			InHGPO.GeoId, InHGPO.PartId, BakeOutlinerFolders, HAPI_ATTROWNER_PRIM, FirstCurvePrimIndex, 1))
		{
			if (BakeOutlinerFolders.Num() > 0 && !BakeOutlinerFolders[0].IsEmpty())
			{
				// cache the bake actor attribute on the output object
				OutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_OUTLINER_FOLDER, BakeOutlinerFolders[0]);
			}
		}

		// Handle user specified landscape layers for these segments
		UpdateNonReservedEditLayers(SplineInfo, ClearedLayers, SegmentsToApplyToLayers);

		if (SplineInfo.bReusedPreviousOutput)
		{
			// Remove the reused output object from the old map to avoid its deletion
			InCurrentSplines.Remove(SplineInfo.Identifier);
		}
	}

	return true;
#endif
}

ULandscapeSplineControlPoint*
FHoudiniLandscapeSplineTranslator::GetOrCreateControlPoint(FLandscapeSplineInfo& SplineInfo, const int32 InControlPointId, bool& bOutCreated)
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
		int32 ControlPointId = InControlPointId;
		if (ControlPointId < 0)
		{
			ControlPointId = SplineInfo.NextControlPointId;
			SplineInfo.NextControlPointId += 1;
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
	const HAPI_NodeId InNodeId,
	const HAPI_PartId InPartId,
	const HAPI_AttributeOwner InAttrOwner,
	const int32 InStartIndex,
	const int32 InCount,
	TArray<FLandscapeSplineSegmentMeshAttributes>& OutAttributes)
{
	OutAttributes.Reset();

	// Loop look for segment mesh attributes with MeshIndex as a suffix (when > 0). Break out of the loop as soon as
	// we cannot find any segment mesh attribute for the given MeshIndex.
	int32 MeshIndex = 0;
	while (true)
	{
		// If MeshIndex == 0 then don't add the numeric suffix
		const FString AttrNamePrefix = MeshIndex > 0
			? FString::Printf(TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_MESH "%d"), MeshIndex)
			: FString(TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_MESH));

		bool bFoundDataForMeshIndex = false;
		
		FLandscapeSplineSegmentMeshAttributes SegmentAttributes;
		
		// mesh ref
		static constexpr int32 TupleSizeOne = 1;
		HAPI_AttributeInfo MeshRefAttrInfo;
		SegmentAttributes.bHasMeshRefAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			InNodeId,
			InPartId,
			TCHAR_TO_ANSI(*AttrNamePrefix),
			MeshRefAttrInfo,
			SegmentAttributes.MeshRef,
			TupleSizeOne,
			InAttrOwner,
			InStartIndex,
			InCount);
		if (SegmentAttributes.bHasMeshRefAttribute)
			bFoundDataForMeshIndex = true;

		// mesh scale
		static constexpr int32 MeshScaleTupleSize = 3;
		const FString MeshScaleAttrName = FString::Printf(
			TEXT("%s%s"), *AttrNamePrefix, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_SCALE_SUFFIX));

		HAPI_AttributeInfo MeshScaleAttrInfo;
		SegmentAttributes.bHasMeshScaleAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
			InNodeId,
			InPartId,
			TCHAR_TO_ANSI(*MeshScaleAttrName),
			MeshScaleAttrInfo,
			SegmentAttributes.MeshScale,
			MeshScaleTupleSize,
			InAttrOwner,
			InStartIndex,
			InCount);
		if (SegmentAttributes.bHasMeshScaleAttribute)
			bFoundDataForMeshIndex = true;

		// center adjust
		static constexpr int32 MeshCenterAdjustTupleSize = 2;
		const FString MeshCenterAdjustAttrName = FString::Printf(
			TEXT("%s%s"), *AttrNamePrefix, TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_CENTER_ADJUST_SUFFIX));
		
		HAPI_AttributeInfo MeshCenterAdjustAttrInfo;
		SegmentAttributes.bHasMeshCenterAdjustAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
			InNodeId,
			InPartId,
			TCHAR_TO_ANSI(*MeshCenterAdjustAttrName),
			MeshCenterAdjustAttrInfo,
			SegmentAttributes.CenterAdjust,
			MeshCenterAdjustTupleSize,
			InAttrOwner,
			InStartIndex,
			InCount);
		if (SegmentAttributes.bHasMeshCenterAdjustAttribute)
			bFoundDataForMeshIndex = true;

		// material overrides
		const FString MaterialAttrNamePrefix = AttrNamePrefix + TEXT(HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_MATERIAL_OVERRIDE_SUFFIX);
		SegmentAttributes.MeshMaterialOverrideRefs.Reset();

		// The same as with the MeshIndex above, loop until the first iteration where we cannot find a material
		// override attribute
		int32 MaterialOverrideIdx = 0;
		while (true)
		{
			TArray<FString> MaterialOverrides;

			// Add the MaterialOverrideIdx as a suffix to the attribute name when > 0
			const FString MaterialOverrideAttrName = MaterialOverrideIdx > 0
				? MaterialAttrNamePrefix + FString::Printf(TEXT("%d"), MaterialOverrideIdx)
				: MaterialAttrNamePrefix;

			HAPI_AttributeInfo MaterialOverrideAttrInfo;
			if (!FHoudiniEngineUtils::HapiGetAttributeDataAsString(
					InNodeId,
					InPartId,
					TCHAR_TO_ANSI(*MaterialOverrideAttrName),
					MaterialOverrideAttrInfo,
					MaterialOverrides,
					TupleSizeOne,
					InAttrOwner,
					InStartIndex,
					InCount))
			{
				break;
			}
			
			SegmentAttributes.MeshMaterialOverrideRefs.Emplace(MoveTemp(MaterialOverrides));
			bFoundDataForMeshIndex = true;
			MaterialOverrideIdx++;
		}
		SegmentAttributes.MeshMaterialOverrideRefs.Shrink();

		if (!bFoundDataForMeshIndex)
			break;

		OutAttributes.Emplace(MoveTemp(SegmentAttributes));

		MeshIndex++;
	}
	OutAttributes.Shrink();

	return true;
}

bool
FHoudiniLandscapeSplineTranslator::CopyCurveAttributesFromHoudini(
	const HAPI_NodeId InNodeId,
	const HAPI_PartId InPartId,
	const int32 InPrimIndex,
	const int32 InFirstPointIndex,
	const int32 InNumPoints,
	FLandscapeSplineCurveAttributes& OutCurveAttributes)
{
	// Some constants that are reused
	// Tuple size of 1
	static constexpr int32 TupleSizeOne = 1;
	// Prim attribute data count of 1
	static constexpr int32 NumPrimsOne = 1;

	// point positions
	static constexpr int32 PositionTupleSize = 3;
	HAPI_AttributeInfo PositionAttrInfo;
	FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_POSITION,
		PositionAttrInfo,
		OutCurveAttributes.PointPositions,
		PositionTupleSize,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// rot attribute (quaternion) -- control point rotations
	static constexpr int32 RotationTupleSize = 4;
	HAPI_AttributeInfo RotationAttrInfo;
	OutCurveAttributes.bHasPointRotationAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_ROTATION,
		RotationAttrInfo,
		OutCurveAttributes.PointRotations,
		RotationTupleSize,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// control point paint layer names
	HAPI_AttributeInfo LayerNameAttrInfo;
	OutCurveAttributes.bHasPointPaintLayerNameAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_PAINT_LAYER_NAME,
		LayerNameAttrInfo,
		OutCurveAttributes.PointPaintLayerNames,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// control point raise terrains
	HAPI_AttributeInfo RaiseTerrainAttrInfo;
	OutCurveAttributes.bHasPointRaiseTerrainAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_RAISE_TERRAIN,
		RaiseTerrainAttrInfo,
		OutCurveAttributes.PointRaiseTerrains,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// control point lower terrains
	HAPI_AttributeInfo LowerTerrainAttrInfo;
	OutCurveAttributes.bHasPointLowerTerrainAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_LOWER_TERRAIN,
		LowerTerrainAttrInfo,
		OutCurveAttributes.PointLowerTerrains,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// control point meshes
	HAPI_AttributeInfo ControlPointMeshAttrInfo;
	OutCurveAttributes.bHasPointMeshRefAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_MESH,
		ControlPointMeshAttrInfo,
		OutCurveAttributes.PointMeshRefs,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// control point material overrides
	OutCurveAttributes.PerMaterialOverridePointRefs.Reset();
	const FString ControlPointMaterialOverrideAttrNamePrefix = TEXT(
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_MATERIAL_OVERRIDE_SUFFIX);

	// Loop until the first iteration where we don't find any material override attributes.
	int32 MaterialOverrideIdx = 0;
	while (true)
	{
		TArray<FString> MaterialOverrides;

		// If the index > 0 add it as a suffix to the attribute name
		const FString AttrName = MaterialOverrideIdx > 0
			? ControlPointMaterialOverrideAttrNamePrefix + FString::Printf(TEXT("%d"), MaterialOverrideIdx)
			: ControlPointMaterialOverrideAttrNamePrefix;

		HAPI_AttributeInfo AttrInfo;
		if (!FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			InNodeId,
			InPartId,
			TCHAR_TO_ANSI(*AttrName),
			AttrInfo,
			MaterialOverrides,
			TupleSizeOne,
			HAPI_ATTROWNER_POINT,
			InFirstPointIndex,
			InNumPoints))
		{
			break;
		}
		
		OutCurveAttributes.PerMaterialOverridePointRefs.Emplace(MoveTemp(MaterialOverrides));
		MaterialOverrideIdx++;
	}

	// control point mesh scales
	static constexpr int32 MeshScaleTupleSize = 3;
	HAPI_AttributeInfo MeshScaleAttrInfo;
	OutCurveAttributes.bHasPointMeshScaleAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_MESH HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_MESH_SCALE_SUFFIX,
		MeshScaleAttrInfo,
		OutCurveAttributes.PointMeshScales,
		MeshScaleTupleSize,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// control point ids
	HAPI_AttributeInfo ControlPointNameAttrInfo;
	OutCurveAttributes.bHasPointIdAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_CONTROL_POINT_ID,
		ControlPointNameAttrInfo,
		OutCurveAttributes.PointIds,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// point half-widths
	HAPI_AttributeInfo HalfWidthAttrInfo;
	OutCurveAttributes.bHasPointHalfWidthAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_HALF_WIDTH,
		HalfWidthAttrInfo,
		OutCurveAttributes.PointHalfWidths,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// point side-falloff
	HAPI_AttributeInfo SideFalloffAttrInfo;
	OutCurveAttributes.bHasPointSideFalloffAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SIDE_FALLOFF,
		SideFalloffAttrInfo,
		OutCurveAttributes.PointSideFalloffs,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// point end-falloff
	HAPI_AttributeInfo EndFalloffAttrInfo;
	OutCurveAttributes.bHasPointEndFalloffAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_END_FALLOFF,
		EndFalloffAttrInfo,
		OutCurveAttributes.PointEndFalloffs,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

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
	for (int32 ConnectionIndex = 0; ConnectionIndex < 2; ++ConnectionIndex)
	{
		// segment connection[ConnectionIndex] socket names -- vertex/point attribute
		HAPI_AttributeInfo MeshSocketNameAttrInfo;
		OutCurveAttributes.bHasVertexConnectionSocketNameAttribute[ConnectionIndex] = FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			InNodeId,
			InPartId,
			ConnectionMeshSocketNameAttrNames[ConnectionIndex],
			MeshSocketNameAttrInfo,
			OutCurveAttributes.VertexConnectionSocketNames[ConnectionIndex],
			TupleSizeOne,
			HAPI_ATTROWNER_POINT,
			InFirstPointIndex,
			InNumPoints);

		// segment connection[ConnectionIndex] tangents -- vertex/point attribute
		HAPI_AttributeInfo TangentLengthAttrInfo;
		OutCurveAttributes.bHasVertexConnectionTangentLengthAttribute[ConnectionIndex] = FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
			InNodeId,
			InPartId,
			ConnectionTangentLengthAttrNames[ConnectionIndex],
			TangentLengthAttrInfo,
			OutCurveAttributes.VertexConnectionTangentLengths[ConnectionIndex],
			TupleSizeOne,
			HAPI_ATTROWNER_POINT,
			InFirstPointIndex,
			InNumPoints);

		// segment connection[ConnectionIndex] socket names -- prim attribute
		if (!OutCurveAttributes.bHasVertexConnectionSocketNameAttribute[ConnectionIndex])
		{
			TArray<FString> SocketNames;
			HAPI_AttributeInfo PrimMeshSocketNameAttrInfo;
			OutCurveAttributes.bHasPrimConnectionSocketNameAttribute[ConnectionIndex] = FHoudiniEngineUtils::HapiGetAttributeDataAsString(
				InNodeId,
				InPartId,
				ConnectionMeshSocketNameAttrNames[ConnectionIndex],
				PrimMeshSocketNameAttrInfo,
				SocketNames,
				TupleSizeOne,
				HAPI_ATTROWNER_PRIM,
				InPrimIndex,
				NumPrimsOne);
			if (OutCurveAttributes.bHasPrimConnectionSocketNameAttribute[ConnectionIndex] && SocketNames.Num() > 0)
			{
				OutCurveAttributes.PrimConnectionSocketNames[ConnectionIndex] = SocketNames[0];
			}
		}
		else
		{
			OutCurveAttributes.bHasPrimConnectionSocketNameAttribute[ConnectionIndex] = false;
		}

		// segment connection[ConnectionIndex] tangents -- prim attribute
		if (!OutCurveAttributes.bHasVertexConnectionTangentLengthAttribute[ConnectionIndex])
		{
			TArray<float> Tangents;
			HAPI_AttributeInfo PrimTangentLengthAttrInfo;
			OutCurveAttributes.bHasPrimConnectionTangentLengthAttribute[ConnectionIndex] = FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
				InNodeId,
				InPartId,
				ConnectionTangentLengthAttrNames[ConnectionIndex],
				PrimTangentLengthAttrInfo,
				Tangents,
				TupleSizeOne,
				HAPI_ATTROWNER_PRIM,
				InPrimIndex,
				NumPrimsOne);
			if (OutCurveAttributes.bHasPrimConnectionTangentLengthAttribute[ConnectionIndex] && Tangents.Num() > 0)
			{
				OutCurveAttributes.PrimConnectionTangentLengths[ConnectionIndex] = Tangents[0];
			}
		}
		else
		{
			OutCurveAttributes.bHasPrimConnectionTangentLengthAttribute[ConnectionIndex] = false;
		}
	}

	// segment paint layer name -- vertex/point
	HAPI_AttributeInfo VertexLayerNameAttrInfo;
	OutCurveAttributes.bHasVertexPaintLayerNameAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_PAINT_LAYER_NAME,
		VertexLayerNameAttrInfo,
		OutCurveAttributes.VertexPaintLayerNames,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// segment raise terrains -- vertex/point
	HAPI_AttributeInfo VertexRaiseTerrainAttrInfo;
	OutCurveAttributes.bHasVertexRaiseTerrainAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_RAISE_TERRAIN,
		VertexRaiseTerrainAttrInfo,
		OutCurveAttributes.VertexRaiseTerrains,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// segment lower terrains -- vertex/point
	HAPI_AttributeInfo VertexLowerTerrainAttrInfo;
	OutCurveAttributes.bHasVertexLowerTerrainAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_LOWER_TERRAIN,
		VertexLowerTerrainAttrInfo,
		OutCurveAttributes.VertexLowerTerrains,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);
	
	// segment edit layer -- vertex/point
	HAPI_AttributeInfo VertexEditLayerAttrInfo;
	OutCurveAttributes.bHasVertexEditLayerAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_NAME,
		VertexEditLayerAttrInfo,
		OutCurveAttributes.VertexEditLayers,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// segment clear edit layer -- vertex/point
	HAPI_AttributeInfo VertexEditLayerClearAttrInfo;
	OutCurveAttributes.bHasVertexEditLayerClearAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_CLEAR,
		VertexEditLayerClearAttrInfo,
		OutCurveAttributes.VertexEditLayersClear,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// segment clear edit layer -- vertex/point
	HAPI_AttributeInfo VertexEditLayerAfterAttrInfo;
	OutCurveAttributes.bHasVertexEditLayerAfterAttribute = FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		InNodeId,
		InPartId,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_AFTER,
		VertexEditLayerAfterAttrInfo,
		OutCurveAttributes.VertexEditLayersAfter,
		TupleSizeOne,
		HAPI_ATTROWNER_POINT,
		InFirstPointIndex,
		InNumPoints);

	// segment paint layer name
	if (!OutCurveAttributes.bHasVertexPaintLayerNameAttribute)
	{
		TArray<FString> SegmentPaintLayerName;
		HAPI_AttributeInfo PrimLayerNameAttrInfo;
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
				InNodeId,
				InPartId,
				HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_PAINT_LAYER_NAME,
				PrimLayerNameAttrInfo,
				SegmentPaintLayerName,
				TupleSizeOne,
				HAPI_ATTROWNER_PRIM,
				InPrimIndex,
				NumPrimsOne) && SegmentPaintLayerName.Num() > 0)
		{
			OutCurveAttributes.PrimPaintLayerName = SegmentPaintLayerName[0];
			OutCurveAttributes.bHasPrimPaintLayerNameAttribute = true;
		}
		else
		{
			OutCurveAttributes.PrimPaintLayerName = FString();
			OutCurveAttributes.bHasPrimPaintLayerNameAttribute = false;
		}
	}
	else
	{
		OutCurveAttributes.bHasPrimPaintLayerNameAttribute = false;
	}

	// segment raise terrains
	if (!OutCurveAttributes.bHasVertexRaiseTerrainAttribute)
	{
		TArray<int32> RaiseTerrains;
		HAPI_AttributeInfo PrimRaiseTerrainAttrInfo;
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
				InNodeId,
				InPartId,
				HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_RAISE_TERRAIN,
				PrimRaiseTerrainAttrInfo,
				RaiseTerrains,
				TupleSizeOne,
				HAPI_ATTROWNER_PRIM,
				InPrimIndex,
				NumPrimsOne) && RaiseTerrains.Num() > 0)
		{
			OutCurveAttributes.bPrimRaiseTerrain = RaiseTerrains[0];
			OutCurveAttributes.bHasPrimRaiseTerrainAttribute = true;
		}
		else
		{
			OutCurveAttributes.bPrimRaiseTerrain = false;
			OutCurveAttributes.bHasPrimRaiseTerrainAttribute = false;
		}
	}
	else
	{
		OutCurveAttributes.bHasPrimRaiseTerrainAttribute = false;
	}

	// segment lower terrains
	if (!OutCurveAttributes.bHasVertexLowerTerrainAttribute)
	{
		TArray<int32> LowerTerrains;
		HAPI_AttributeInfo PrimLowerTerrainAttrInfo;
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
				InNodeId,
				InPartId,
				HAPI_UNREAL_ATTRIB_LANDSCAPE_SPLINE_SEGMENT_LOWER_TERRAIN,
				PrimLowerTerrainAttrInfo,
				LowerTerrains,
				TupleSizeOne,
				HAPI_ATTROWNER_PRIM,
				InPrimIndex,
				NumPrimsOne) && LowerTerrains.Num() > 0)
		{
			OutCurveAttributes.bPrimLowerTerrain = LowerTerrains[0];
			OutCurveAttributes.bHasPrimLowerTerrainAttribute = true;
		}
		else
		{
			OutCurveAttributes.bPrimLowerTerrain = false;
			OutCurveAttributes.bHasPrimLowerTerrainAttribute = false;
		}
	}
	else
	{
		OutCurveAttributes.bHasPrimLowerTerrainAttribute = false;
	}

	// segment edit layer -- prim
	if (!OutCurveAttributes.bHasVertexEditLayerAttribute)
	{
		TArray<FString> EditLayers;
		HAPI_AttributeInfo PrimEditLayerAttrInfo;
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
				InNodeId,
				InPartId,
				HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_NAME,
				PrimEditLayerAttrInfo,
				EditLayers,
				TupleSizeOne,
				HAPI_ATTROWNER_PRIM,
				InPrimIndex,
				NumPrimsOne) && EditLayers.Num() > 0)
		{
			OutCurveAttributes.PrimEditLayer = EditLayers[0];
			OutCurveAttributes.bHasPrimEditLayerAttribute = true;
		}
		else
		{
			OutCurveAttributes.bHasPrimEditLayerAttribute = false;
		}
	}
	else
	{
		OutCurveAttributes.bHasPrimEditLayerAttribute = false;
	}

	// segment edit layer clear -- prim
	if (!OutCurveAttributes.bHasVertexEditLayerClearAttribute)
	{
		TArray<int32> EditLayersClear;
		HAPI_AttributeInfo PrimEditLayerClearAttrInfo;
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
				InNodeId,
				InPartId,
				HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_CLEAR,
				PrimEditLayerClearAttrInfo,
				EditLayersClear,
				TupleSizeOne,
				HAPI_ATTROWNER_PRIM,
				InPrimIndex,
				NumPrimsOne) && EditLayersClear.Num() > 0)
		{
			OutCurveAttributes.bPrimEditLayerClear = static_cast<bool>(EditLayersClear[0]);
			OutCurveAttributes.bHasPrimEditLayerClearAttribute = true;
		}
		else
		{
			OutCurveAttributes.bHasPrimEditLayerClearAttribute = false;
		}
	}
	else
	{
		OutCurveAttributes.bHasPrimEditLayerClearAttribute = false;
	}

	// segment edit layer after -- prim
	if (!OutCurveAttributes.bHasVertexEditLayerAfterAttribute)
	{
		TArray<FString> EditLayersAfter;
		HAPI_AttributeInfo PrimEditLayerAfterAttrInfo;
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
				InNodeId,
				InPartId,
				HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_AFTER,
				PrimEditLayerAfterAttrInfo,
				EditLayersAfter,
				TupleSizeOne,
				HAPI_ATTROWNER_PRIM,
				InPrimIndex,
				NumPrimsOne) && EditLayersAfter.Num() > 0)
		{
			OutCurveAttributes.PrimEditLayerAfter = EditLayersAfter[0];
			OutCurveAttributes.bHasPrimEditLayerAfterAttribute = true;
		}
		else
		{
			OutCurveAttributes.bHasPrimEditLayerAfterAttribute = false;
		}
	}
	else
	{
		OutCurveAttributes.bHasPrimEditLayerAfterAttribute = false;
	}

	// Copy segment mesh attributes from Houdini -- vertex/point attributes
	if (!CopySegmentMeshAttributesFromHoudini(
			InNodeId, InPartId, HAPI_ATTROWNER_POINT, InFirstPointIndex, InNumPoints, OutCurveAttributes.VertexPerMeshSegmentData))
	{
		return false;
	}

	// Copy segment mesh attributes from Houdini -- prim attributes
	if (!CopySegmentMeshAttributesFromHoudini(
			InNodeId, InPartId, HAPI_ATTROWNER_PRIM, InPrimIndex, 1, OutCurveAttributes.PrimPerMeshSegmentData))
	{
		return false;
	}

	return true;
}

bool
FHoudiniLandscapeSplineTranslator::UpdateControlPointFromAttributes(
		ULandscapeSplineControlPoint* const InPoint,
		const FLandscapeSplineCurveAttributes& InAttributes,
		const FTransform& InTransformToApply,
		const int32 InPointIndex)
{
	if (!IsValid(InPoint))
		return false;

	// Apply the attributes from Houdini (InAttributes) to the control point InPoint

	// Rotation
	if (InAttributes.bHasPointRotationAttribute
			&& InAttributes.PointRotations.IsValidIndex(InPointIndex * 4) && InAttributes.PointRotations.IsValidIndex(InPointIndex * 4 + 3))
	{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0)
		static constexpr float HalfPI = UE_HALF_PI;
#else
		static constexpr float HalfPI = HALF_PI;
#endif
		// Convert Houdini Y-up to UE Z-up and also Houdini Z-forward to UE X-forward
		InPoint->Rotation = (InTransformToApply.TransformRotation({
			InAttributes.PointRotations[InPointIndex * 4 + 0],
			InAttributes.PointRotations[InPointIndex * 4 + 2],
			InAttributes.PointRotations[InPointIndex * 4 + 1],
			-InAttributes.PointRotations[InPointIndex * 4 + 3]
		}) * FQuat(FVector::UpVector, HalfPI)).Rotator();
	}

	// (Paint) layer name
	if (InAttributes.bHasPointPaintLayerNameAttribute && InAttributes.PointPaintLayerNames.IsValidIndex(InPointIndex))
	{
		InPoint->LayerName = *InAttributes.PointPaintLayerNames[InPointIndex];
	}

	// bRaiseTerrain
	if (InAttributes.bHasPointRaiseTerrainAttribute && InAttributes.PointRaiseTerrains.IsValidIndex(InPointIndex))
	{
		InPoint->bRaiseTerrain = InAttributes.PointRaiseTerrains[InPointIndex];
	}

	// bLowerTerrain
	if (InAttributes.bHasPointLowerTerrainAttribute && InAttributes.PointLowerTerrains.IsValidIndex(InPointIndex))
	{
		InPoint->bLowerTerrain = InAttributes.PointLowerTerrains[InPointIndex];
	}

	// Control point static mesh
	if (InAttributes.bHasPointMeshRefAttribute && InAttributes.PointMeshRefs.IsValidIndex(InPointIndex))
	{
		UObject* Mesh = StaticFindObject(UStaticMesh::StaticClass(), nullptr, *InAttributes.PointMeshRefs[InPointIndex]);
		if (!Mesh)
			Mesh = StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *InAttributes.PointMeshRefs[InPointIndex]);
		UStaticMesh* const SM = Mesh ? Cast<UStaticMesh>(Mesh) : nullptr;
		if (IsValid(SM))
			InPoint->Mesh = Cast<UStaticMesh>(Mesh);
		else
			InPoint->Mesh = nullptr;
	}

	// Control point static mesh material overrides
	if (InAttributes.PerMaterialOverridePointRefs.Num() > 0)
	{
		InPoint->MaterialOverrides.Reset(InAttributes.PerMaterialOverridePointRefs.Num());
		for (const TArray<FString>& PerPointMaterialOverrideX : InAttributes.PerMaterialOverridePointRefs)
		{
			if (!PerPointMaterialOverrideX.IsValidIndex(InPointIndex))
				continue;
			
			const FString& MaterialRef = PerPointMaterialOverrideX[InPointIndex];
			
			UObject* Material = StaticFindObject(UMaterialInterface::StaticClass(), nullptr, *MaterialRef);
			if (!Material)
				Material = StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialRef);
			UMaterialInterface* const MaterialInterface = Material ? Cast<UMaterialInterface>(Material) : nullptr;
			if (IsValid(MaterialInterface))
				InPoint->MaterialOverrides.Add(MaterialInterface);
			else
				InPoint->MaterialOverrides.Add(nullptr);
		}
		InPoint->MaterialOverrides.Shrink();
	}

	// Control point mesh scale
	if (InAttributes.bHasPointMeshScaleAttribute
			&& InAttributes.PointMeshScales.IsValidIndex(InPointIndex * 3) && InAttributes.PointMeshScales.IsValidIndex(InPointIndex * 3 + 2))
	{
		InPoint->MeshScale = FVector(
			InAttributes.PointMeshScales[InPointIndex * 3 + 0],
			InAttributes.PointMeshScales[InPointIndex * 3 + 2],
			InAttributes.PointMeshScales[InPointIndex * 3 + 1]);
	}

	// Control point half-width
	if (InAttributes.bHasPointHalfWidthAttribute && InAttributes.PointHalfWidths.IsValidIndex(InPointIndex))
	{
		// Convert from Houdini units to UE units
		InPoint->Width = InAttributes.PointHalfWidths[InPointIndex] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
	}

	// Control point side-falloff
	if (InAttributes.bHasPointSideFalloffAttribute && InAttributes.PointSideFalloffs.IsValidIndex(InPointIndex))
	{
		// Convert from Houdini units to UE units
		InPoint->SideFalloff = InAttributes.PointSideFalloffs[InPointIndex] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
	}

	// Control point end-falloff
	if (InAttributes.bHasPointEndFalloffAttribute && InAttributes.PointEndFalloffs.IsValidIndex(InPointIndex))
	{
		// Convert from Houdini units to UE units
		InPoint->EndFalloff = InAttributes.PointEndFalloffs[InPointIndex] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
	}

	return true;
}

bool
FHoudiniLandscapeSplineTranslator::UpdateSegmentFromAttributes(
	ULandscapeSplineSegment* const InSegment, const FLandscapeSplineCurveAttributes& InAttributes, const int32 InVertexIndex)
{
	if (!IsValid(InSegment))
		return false;

	// Update the segment (InSegment) with the attributes copied from Houdini (InAttributes)

	// Check the vertex/point attribute first, and if that is missing, try the prim attribute

	// (Paint) layer name
	if (InAttributes.bHasVertexPaintLayerNameAttribute && InAttributes.VertexPaintLayerNames.IsValidIndex(InVertexIndex))
	{
		InSegment->LayerName = *InAttributes.VertexPaintLayerNames[InVertexIndex];
	}
	else if (InAttributes.bHasPrimPaintLayerNameAttribute)
	{
		InSegment->LayerName = *InAttributes.PrimPaintLayerName;
	}

	// bRaiseTerrain
	if (InAttributes.bHasVertexRaiseTerrainAttribute && InAttributes.VertexRaiseTerrains.IsValidIndex(InVertexIndex))
	{
		InSegment->bRaiseTerrain = InAttributes.VertexRaiseTerrains[InVertexIndex];
	}
	else if (InAttributes.bHasPrimRaiseTerrainAttribute)
	{
		InSegment->bRaiseTerrain = InAttributes.bPrimRaiseTerrain;
	}

	// bLowerTerrain
	if (InAttributes.bHasVertexLowerTerrainAttribute && InAttributes.VertexLowerTerrains.IsValidIndex(InVertexIndex))
	{
		InSegment->bLowerTerrain = InAttributes.VertexLowerTerrains[InVertexIndex];
	}
	else if (InAttributes.bHasPrimLowerTerrainAttribute)
	{
		InSegment->bLowerTerrain = InAttributes.bPrimLowerTerrain;
	}

	// Segment static meshes
	const int32 MaxNumMeshAttrs = FMath::Max(InAttributes.VertexPerMeshSegmentData.Num(), InAttributes.PrimPerMeshSegmentData.Num());
	InSegment->SplineMeshes.Reset(MaxNumMeshAttrs);
	for (int32 MeshIdx = 0; MeshIdx < MaxNumMeshAttrs; ++MeshIdx)
	{
		FLandscapeSplineMeshEntry SplineMeshEntry;

		// For each index check the vertex/point attribute first, and if not set, check the prim attribute
		
		const FLandscapeSplineSegmentMeshAttributes* PerVertexAttributes = InAttributes.VertexPerMeshSegmentData.IsValidIndex(MeshIdx)
			? &InAttributes.VertexPerMeshSegmentData[MeshIdx] : nullptr;
		const FLandscapeSplineSegmentMeshAttributes* PerPrimAttributes = InAttributes.PrimPerMeshSegmentData.IsValidIndex(MeshIdx)
			? &InAttributes.PrimPerMeshSegmentData[MeshIdx] : nullptr;

		bool bHasMeshRef = false;
		FString MeshRef; 
		if (PerVertexAttributes && PerVertexAttributes->bHasMeshRefAttribute && PerVertexAttributes->MeshRef.IsValidIndex(InVertexIndex))
		{
			bHasMeshRef = true;
			MeshRef = PerVertexAttributes->MeshRef[InVertexIndex];
		}
		else if (PerPrimAttributes && PerPrimAttributes->bHasMeshRefAttribute && PerPrimAttributes->MeshRef.Num() > 0)
		{
			bHasMeshRef = true;
			MeshRef = PerPrimAttributes->MeshRef[0];
		}
		
		if (bHasMeshRef)
		{
			// We have a static mesh at this index, try to find / load it
			UObject* Mesh = StaticFindObject(UStaticMesh::StaticClass(), nullptr, *MeshRef);
			if (!Mesh)
				Mesh = StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *MeshRef);
			UStaticMesh* const SM = Mesh ? Cast<UStaticMesh>(Mesh) : nullptr;
			if (IsValid(SM))
				SplineMeshEntry.Mesh = SM;
			else
				SplineMeshEntry.Mesh = nullptr;
		}

		// mesh scale
		if (PerVertexAttributes && PerVertexAttributes->bHasMeshScaleAttribute
				&& PerVertexAttributes->MeshScale.IsValidIndex(InVertexIndex * 3)
				&& PerVertexAttributes->MeshScale.IsValidIndex(InVertexIndex * 3 + 2))
		{
			const int32 ValueIdx = InVertexIndex * 3;
			SplineMeshEntry.Scale = FVector(
				PerVertexAttributes->MeshScale[ValueIdx + 0],
				PerVertexAttributes->MeshScale[ValueIdx + 2],
				PerVertexAttributes->MeshScale[ValueIdx + 1]);
		}
		else if (PerPrimAttributes && PerPrimAttributes->bHasMeshScaleAttribute && PerPrimAttributes->MeshScale.IsValidIndex(3))
		{
			SplineMeshEntry.Scale = FVector(
				PerVertexAttributes->MeshScale[0],
				PerVertexAttributes->MeshScale[2],
				PerVertexAttributes->MeshScale[1]);
		}

		// Center Adjust
		if (PerVertexAttributes && PerVertexAttributes->bHasMeshCenterAdjustAttribute
			&& PerVertexAttributes->CenterAdjust.IsValidIndex(InVertexIndex * 2)
			&& PerVertexAttributes->CenterAdjust.IsValidIndex(InVertexIndex * 2 + 1))
		{
			const int32 ValueIdx = InVertexIndex * 2;
			SplineMeshEntry.CenterAdjust = FVector2D(
				PerVertexAttributes->CenterAdjust[ValueIdx + 0],
				PerVertexAttributes->CenterAdjust[ValueIdx + 1]);
		}
		else if (PerPrimAttributes && PerPrimAttributes->bHasMeshCenterAdjustAttribute && PerPrimAttributes->CenterAdjust.IsValidIndex(2))
		{
			SplineMeshEntry.CenterAdjust = FVector2D(
				PerVertexAttributes->CenterAdjust[0],
				PerVertexAttributes->CenterAdjust[1]);
		}

		// Each segment static mesh can have multiple material overrides
		// Determine the max from the number of vertex/point material override attributes and the number of prim
		// material override attributes
		const int32 MaxNumMaterialOverrides = FMath::Max(
			PerVertexAttributes ? PerVertexAttributes->MeshMaterialOverrideRefs.Num() : 0,
			PerPrimAttributes ? PerPrimAttributes->MeshMaterialOverrideRefs.Num() : 0);
		SplineMeshEntry.MaterialOverrides.Reset(MaxNumMaterialOverrides);
		for (int32 MaterialOverrideIdx = 0; MaterialOverrideIdx < MaxNumMaterialOverrides; ++MaterialOverrideIdx)
		{
			bool bHasMaterialRef = false;
			FString MaterialRef;

			// Check vertex/prim attribute first, if that is not set check the prim attribute.

			if (PerVertexAttributes && PerVertexAttributes->MeshMaterialOverrideRefs.IsValidIndex(MaterialOverrideIdx))
			{
				const TArray<FString>& PerVertexMaterialOverrides = PerVertexAttributes->MeshMaterialOverrideRefs[MaterialOverrideIdx];
				if (PerVertexMaterialOverrides.IsValidIndex(InVertexIndex))
				{
					bHasMaterialRef = true;
					MaterialRef = PerVertexMaterialOverrides[InVertexIndex];
				}
			}

			if (!bHasMaterialRef && PerPrimAttributes && PerPrimAttributes->MeshMaterialOverrideRefs.IsValidIndex(MaterialOverrideIdx))
			{
				const TArray<FString>& PerPrimMaterialOverrides = PerPrimAttributes->MeshMaterialOverrideRefs[MaterialOverrideIdx];
				if (PerPrimMaterialOverrides.Num() > 0)
				{
					bHasMaterialRef = true;
					MaterialRef = PerPrimMaterialOverrides[0];
				}
			}

			if (!bHasMaterialRef)
			{
				// No material override at this index, set it to null
				SplineMeshEntry.MaterialOverrides.Add(nullptr);
				continue;
			}

			// Found a material override at this index, try to find / load the it
			UObject* Material = StaticFindObject(UMaterialInterface::StaticClass(), nullptr, *MaterialRef);
			if (!Material)
				Material = StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialRef);
			UMaterialInterface* const MaterialInterface = Material ? Cast<UMaterialInterface>(Material) : nullptr;
			if (IsValid(MaterialInterface))
				SplineMeshEntry.MaterialOverrides.Add(MaterialInterface);
			else
				SplineMeshEntry.MaterialOverrides.Add(nullptr);
		}
		SplineMeshEntry.MaterialOverrides.Shrink();

		InSegment->SplineMeshes.Emplace(SplineMeshEntry);
	}
	InSegment->SplineMeshes.Shrink();

	return true;
}

bool
FHoudiniLandscapeSplineTranslator::UpdateConnectionFromAttributes(
	FLandscapeSplineSegmentConnection& InConnection,
	const int32 InConnectionIndex,
	const FLandscapeSplineCurveAttributes& InAttributes,
	const int32 InPointIndex)
{
	// Update the InConnection's properties from the attributes copied from Houdini.
	// Check the vertex/point attribute first, if that is not set, use the prim attribute.

	// socket name
	if (InAttributes.bHasVertexConnectionSocketNameAttribute[InConnectionIndex] && InAttributes.VertexConnectionSocketNames[InConnectionIndex].IsValidIndex(InPointIndex))
	{
		InConnection.SocketName = *InAttributes.VertexConnectionSocketNames[InConnectionIndex][InPointIndex];
	}
	else if (InAttributes.bHasPrimConnectionSocketNameAttribute[InConnectionIndex])
	{
		InConnection.SocketName = *InAttributes.PrimConnectionSocketNames[InConnectionIndex];
	}

	// tangent length
	if (InAttributes.bHasVertexConnectionTangentLengthAttribute[InConnectionIndex] && InAttributes.VertexConnectionTangentLengths[InConnectionIndex].IsValidIndex(InPointIndex))
	{
		InConnection.TangentLen = InAttributes.VertexConnectionTangentLengths[InConnectionIndex][InPointIndex];
	}
	else if (InAttributes.bHasPrimConnectionTangentLengthAttribute[InConnectionIndex])
	{
		InConnection.TangentLen = InAttributes.PrimConnectionTangentLengths[InConnectionIndex];
	}

	return true;
}
