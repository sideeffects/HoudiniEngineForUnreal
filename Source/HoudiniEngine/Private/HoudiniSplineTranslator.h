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

#pragma once

#include "HAPI/HAPI_Common.h"
#include "HoudiniGeoPartObject.h"

#include "CoreMinimal.h"

class UHoudiniInput;
class UHoudiniOutput;
class UHoudiniAssetComponent;
class UHoudiniSplineComponent;
class USceneComponent;
class USplineComponent;

struct FHoudiniGeoPartObject;
struct FHoudiniOutputObjectIdentifier;
struct FHoudiniOutputObject;

enum class EHoudiniCurveOutputType : uint8;

struct HOUDINIENGINE_API FHoudiniSplineTranslator
{
	// Get the cooked Houdini curve.
	static bool UpdateHoudiniCurve(UHoudiniSplineComponent * HoudiniSplineComponent);

	// Get the cooked Houdini curve.
	static bool UpdateHoudiniCurveLegacy(UHoudiniSplineComponent * HoudiniSplineComponent);
	
	// Get all cooked Houdini curves of an input.
	static void UpdateHoudiniInputCurves(UHoudiniInput* Input);

	// Get all cooked Houdini curves of inputs in an HAC.
	static void UpdateHoudiniInputCurves(UHoudiniAssetComponent* HAC);

	// Upload Houdini spline component data to the curve node, and then sync the Houdini Spline Component with the curve node.
	static bool HapiUpdateNodeForHoudiniSplineComponent(UHoudiniSplineComponent* HoudiniSplineComponent, bool bInSetRotAndScaleAttributes);

	// Indicates if the node is a valid curve input node
	static bool IsCurveInputNodeValid(const HAPI_NodeId& InNodeId, const bool& bLegacyNode = false);

	// Update the curve node data, or create a new curve node if the CurveNodeId is valid.
	static bool HapiCreateCurveInputNodeForData(
		HAPI_NodeId& CurveNodeId,
		const HAPI_NodeId& ParentNodeId,
		const FString& InputNodeName,
		TArray<FVector>* Positions,
		TArray<FQuat>* Rotations,
		TArray<FVector>* Scales3d,
		EHoudiniCurveType InCurveType,
		EHoudiniCurveMethod InCurveMethod,
		const bool& InClosed,
		const bool& InReversed,
		const bool& InForceClose = false,
		const FTransform& ParentTransform = FTransform::Identity,
		const bool & InIsLegacyCurve = false,
		// Only used if Legacy curve:
		const int32& InOrder = 2,
		const EHoudiniCurveBreakpointParameterization& InBreakpointParameterization = EHoudiniCurveBreakpointParameterization::Uniform
	);

	// Update curve node data using curve::1.0
	// Update the curve node data, or create a new curve node if the CurveNodeId is valid.
	static bool HapiCreateCurveInputNodeForDataLegacy(
		HAPI_NodeId& CurveNodeId,
		const HAPI_NodeId& ParentNodeId,
		const FString& InputNodeName,
		TArray<FVector>* Positions,
		TArray<FQuat>* Rotations,
		TArray<FVector>* Scales3d,
		const EHoudiniCurveType& InCurveType,
		const EHoudiniCurveMethod& InCurveMethod,
		const bool& InClosed,
		const bool& InReversed,
		const bool& InForceClose = false,
		const FTransform& ParentTransform = FTransform::Identity);

	
	// Create a default curve node.
	static bool HapiCreateCurveInputNode(
		HAPI_NodeId& OutCurveNodeId, 
		const HAPI_NodeId& ParentNodeId, 
		const FString& InputNodeName, 
		const bool& InIsLegacyCurve);

	// Create a Houdini spline component from a given editable node. (Only called once when first build the editable node.)
	static UHoudiniSplineComponent* CreateHoudiniSplineComponentFromHoudiniEditableNode(const int32 & GeoId, const FString & PartName, UObject* OuterComponent);

	// Helper functions.
	static void ExtractStringPositions(const FString& Positions, TArray<FVector>& OutPositions);

	static void ConvertPositionToVectorData(const TArray<float>& InRawData, TArray<TArray<FVector>>& OutVectorData, const TArray<int32>& CurveCounts);
	static void ConvertScaleToVectorData(const TArray<float>& InRawData, TArray<TArray<FVector>>& OutVectorData, const TArray<int32>& CurveCounts);
	static void ConvertEulerRotationToVectorData(const TArray<float>& InRawData, TArray<TArray<FVector>>& OutVectorData, const TArray<int32>& CurveCounts);
	static void ConvertQuaternionRotationToVectorData(const TArray<float>& InRawData, TArray<TArray<FVector>>& OutVectorData, const TArray<int32>& CurveCounts);

	static void CreatePositionsString(const TArray<FVector>& InPositions, FString& OutPositionString);

	static bool CreateOutputSplinesFromHoudiniGeoPartObject(
		const FHoudiniGeoPartObject& InHGPO,
		UObject* InOuterComponent,
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InSplines,
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutSplines,
		const bool& InForceRebuild,
		const EHoudiniCurveOutputType& OutputCurveType,
		const bool& bIsLinear,
		const bool& bIsClosed);

	static bool CreateAllSplinesFromHoudiniOutput(
		UHoudiniOutput* InOutput,
		UObject* InOuterComponent);

	static USplineComponent* CreateOutputUnrealSplineComponent(
		UObject* OuterComponent,
		const TArray<FVector>& CurvePoints,
		const TArray<FVector>& CurveRotations,
		const TArray<FVector>& CurveScales,
		const bool& bIsLinear,
		const bool& bIsClosed);

	static UHoudiniSplineComponent* CreateOutputHoudiniSplineComponent(
		UHoudiniAssetComponent* OuterHAC,
		TArray<FVector>& CurvePoints,
		const TArray<FVector>& CurveRotations,
		const TArray<FVector>& CurveScales);

	static bool UpdateOutputUnrealSplineComponent(
		USplineComponent* EditedSplineComponent,
		const TArray<FVector>& CurvePoints,
		const TArray<FVector>& CurveRotations,
		const TArray<FVector>& CurveScales,
		const EHoudiniCurveType& CurveType,
		const bool& bClosed);

	static bool UpdateOutputHoudiniSplineComponent(
		const TArray<FVector>& CurvePoints, 
		UHoudiniSplineComponent* EditedHoudiniSplineComponent);

	static void ReselectSelectedActors();
};