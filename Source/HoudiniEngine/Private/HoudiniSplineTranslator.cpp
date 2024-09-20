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

#include "HoudiniSplineTranslator.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniInput.h"
#include "HoudiniOutput.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineString.h"
#include "HoudiniGenericAttribute.h"
#include "HoudiniGeoPartObject.h"
#include "Components/SplineComponent.h"

#include "EditorViewportClient.h"
#include "HoudiniEngineAttributes.h"
#include "Engine/Selection.h"

#include "HoudiniEnginePrivatePCH.h"
#include "HAPI/HAPI.h"

void
FHoudiniSplineTranslator::ExtractStringPositions(const FString& Positions, TArray<FVector>& OutPositions)
{
	TArray<FString> PointStrings;
	static const TCHAR * PositionSeparators[] =
	{
		TEXT(" "),
		TEXT(","),
	};

	int32 NumCoords = Positions.ParseIntoArray(PointStrings, PositionSeparators, 2);
	OutPositions.SetNum(NumCoords / 3);
	for (int32 OutIndex = 0; OutIndex < OutPositions.Num(); OutIndex++)
	{
		const int32& CoordIndex = OutIndex * 3;
		OutPositions[OutIndex].X = FCString::Atod(*(PointStrings[CoordIndex + 0])) * HAPI_UNREAL_SCALE_FACTOR_POSITION;
		OutPositions[OutIndex].Y = FCString::Atod(*(PointStrings[CoordIndex + 2])) * HAPI_UNREAL_SCALE_FACTOR_POSITION;
		OutPositions[OutIndex].Z = FCString::Atod(*(PointStrings[CoordIndex + 1])) * HAPI_UNREAL_SCALE_FACTOR_POSITION;
	}
}

void
FHoudiniSplineTranslator::ConvertPositionToVectorData(const TArray<float>& InRawData, TArray<TArray<FVector>>& OutVectorData, const TArray<int32>& CurveCounts)
{
	OutVectorData.SetNum(CurveCounts.Num());

	int32 TotalNumPoints = 0;
	for (const int32& NextCount : CurveCounts)
		TotalNumPoints += NextCount;

	// Do not fill the output array, if the total number of points does not match
	if (InRawData.Num() < TotalNumPoints * 3)
		return;

	int32 Itr = 0;
	for (int32 n = 0; n < CurveCounts.Num(); ++n)
	{
		TArray<FVector>& NextVectorDataArray = OutVectorData[n];
		NextVectorDataArray.SetNumZeroed(CurveCounts[n]);

		for (int32 PtIdx = 0; PtIdx < CurveCounts[n]; ++PtIdx)
		{
			if (Itr + 2 >= InRawData.Num())
				return;

			// Swap Y/Z convert meters to centimeters
			NextVectorDataArray[PtIdx].X = (double)(InRawData[Itr] * HAPI_UNREAL_SCALE_FACTOR_POSITION);
			NextVectorDataArray[PtIdx].Y = (double)(InRawData[Itr + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION);
			NextVectorDataArray[PtIdx].Z = (double)(InRawData[Itr + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION);

			Itr += 3;
		}
	}
}

void
FHoudiniSplineTranslator::ConvertScaleToVectorData(const TArray<float>& InRawData, TArray<TArray<FVector>>& OutVectorData, const TArray<int32>& CurveCounts)
{
	OutVectorData.SetNum(CurveCounts.Num());

	int32 TotalNumPoints = 0;
	for (const int32& NextCount : CurveCounts)
		TotalNumPoints += NextCount;

	// Do not fill the output array, if the total number of points does not match
	if (InRawData.Num() < TotalNumPoints * 3)
		return;

	int32 Itr = 0;
	for (int32 n = 0; n < CurveCounts.Num(); ++n)
	{
		TArray<FVector>& NextVectorDataArray = OutVectorData[n];
		NextVectorDataArray.SetNumZeroed(CurveCounts[n]);

		for (int32 PtIdx = 0; PtIdx < CurveCounts[n]; ++PtIdx)
		{
			if (Itr + 2 >= InRawData.Num())
				return;

			// Just Swap Y/Z
			NextVectorDataArray[PtIdx].X = (double)InRawData[Itr];
			NextVectorDataArray[PtIdx].Y = (double)InRawData[Itr + 2];
			NextVectorDataArray[PtIdx].Z = (double)InRawData[Itr + 1];

			Itr += 3;
		}
	}
}

void
FHoudiniSplineTranslator::ConvertEulerRotationToVectorData(const TArray<float>& InRawData, TArray<TArray<FVector>>& OutVectorData, const TArray<int32>& CurveCounts)
{
	OutVectorData.SetNum(CurveCounts.Num());

	int32 TotalNumPoints = 0;
	for (const int32& NextCount : CurveCounts)
		TotalNumPoints += NextCount;

	// Do not fill the output array, if the total number of points does not match
	if (InRawData.Num() < TotalNumPoints * 3)
		return;

	int32 Itr = 0;
	for (int32 n = 0; n < CurveCounts.Num(); ++n)
	{
		TArray<FVector>& NextVectorDataArray = OutVectorData[n];
		NextVectorDataArray.SetNumZeroed(CurveCounts[n]);

		for (int32 PtIdx = 0; PtIdx < CurveCounts[n]; ++PtIdx)
		{
			if (Itr + 2 >= InRawData.Num())
				return;

			// Just Swap Y/Z
			NextVectorDataArray[PtIdx].X = (double)InRawData[Itr];
			NextVectorDataArray[PtIdx].Y = (double)InRawData[Itr + 2];
			NextVectorDataArray[PtIdx].Z = (double)InRawData[Itr + 1];

			Itr += 3;
		}
	}
}

void
FHoudiniSplineTranslator::ConvertQuaternionRotationToVectorData(const TArray<float>& InRawData, TArray<TArray<FVector>>& OutVectorData, const TArray<int32>& CurveCounts)
{
	OutVectorData.SetNum(CurveCounts.Num());

	int32 TotalNumPoints = 0;
	for (const int32& NextCount : CurveCounts)
		TotalNumPoints += NextCount;

	// Do not fill the output array, if the total number of points does not match
	if (InRawData.Num() < TotalNumPoints * 4)
		return;

	int32 Itr = 0;
	for (int32 n = 0; n < CurveCounts.Num(); ++n)
	{
		TArray<FVector>& NextVectorDataArray = OutVectorData[n];
		NextVectorDataArray.SetNumZeroed(CurveCounts[n]);

		for (int32 PtIdx = 0; PtIdx < CurveCounts[n]; ++PtIdx)
		{
			if (Itr + 3 >= InRawData.Num())
				return;

			// Extract a quaternion: Swap Y/Z, invert W
			FQuat ObjectRotation(
				(double)InRawData[Itr + 0],
				(double)InRawData[Itr + 2],
				(double)InRawData[Itr + 1],
				(double)-InRawData[Itr + 3]);

			// Get Euler angles
			NextVectorDataArray[PtIdx] = ObjectRotation.Euler();

			Itr += 4;
		}
	}
}

void
FHoudiniSplineTranslator::UpdateHoudiniInputCurves(UHoudiniAssetComponent* HAC)
{
	for (UHoudiniInput * NextInput : HAC->Inputs)
		UpdateHoudiniInputCurves(NextInput);
}

void
FHoudiniSplineTranslator::UpdateHoudiniInputCurves(UHoudiniInput* Input)
{
	if (!Input || Input->GetInputType() != EHoudiniInputType::Curve)
		return;

	TArray<UHoudiniInputObject*> *InputObjectArray = Input->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
	if (!InputObjectArray)
		return;

	for (UHoudiniInputObject * NextInputObject : *InputObjectArray)
	{
		UHoudiniInputHoudiniSplineComponent * HoudiniSplineInput = Cast<UHoudiniInputHoudiniSplineComponent>(NextInputObject);
		if (!HoudiniSplineInput)
			continue;

		UHoudiniSplineComponent * HoudiniSplineComponent = HoudiniSplineInput->GetCurveComponent();
		FHoudiniSplineTranslator::UpdateHoudiniCurve(HoudiniSplineComponent);
	}
}

bool
FHoudiniSplineTranslator::UpdateHoudiniCurve(UHoudiniSplineComponent* HoudiniSplineComponent)
{
	if (!IsValid(HoudiniSplineComponent))
		return false;

	if (HoudiniSplineComponent->bIsLegacyInputCurve)
	{
		return UpdateHoudiniCurveLegacy(HoudiniSplineComponent);
	}

	int32 CurveNode_id = HoudiniSplineComponent->GetNodeId();
	if (CurveNode_id < 0)
		return false;

	int32 PartId = 0;

	HAPI_InputCurveInfo InputCurveInfo;
	FHoudiniApi::InputCurveInfo_Init(&InputCurveInfo);
	
	if (FHoudiniApi::GetInputCurveInfo(
		FHoudiniEngine::Get().GetSession(), CurveNode_id, PartId, &InputCurveInfo) != HAPI_RESULT_SUCCESS )
	{
		HAPI_NodeId NodeId = -1;
		// Create the curve SOP Node

		if (!FHoudiniSplineTranslator::HapiCreateCurveInputNode(NodeId, -1, HoudiniSplineComponent->GetName(), false))
			return false;

		HoudiniSplineComponent->SetNodeId(NodeId);

		HOUDINI_CHECK_ERROR_RETURN(	FHoudiniApi::GetInputCurveInfo(
		FHoudiniEngine::Get().GetSession(), CurveNode_id, PartId, &InputCurveInfo), false);
	}

	HoudiniSplineComponent->SetCurveType((EHoudiniCurveType)InputCurveInfo.curveType);
	HoudiniSplineComponent->SetCurveMethod((EHoudiniCurveMethod)InputCurveInfo.inputMethod);
	HoudiniSplineComponent->SetClosedCurve(InputCurveInfo.closed);
	HoudiniSplineComponent->SetReversed(InputCurveInfo.reverse);
	HoudiniSplineComponent->SetCurveOrder(InputCurveInfo.order);
	HoudiniSplineComponent->SetCurveBreakpointParameterization((EHoudiniCurveBreakpointParameterization)InputCurveInfo.breakpointParameterization);

	HAPI_AttributeInfo HapiCoordsInfo;
	FHoudiniApi::AttributeInfo_Init(&HapiCoordsInfo);

	// Get and set the input coords positions
	HOUDINI_CHECK_ERROR_RETURN(	FHoudiniApi::GetAttributeInfo(
		FHoudiniEngine::Get().GetSession(), CurveNode_id, PartId, HAPI_UNREAL_ATTRIB_HAPI_INPUT_CURVE_COORDS, HAPI_ATTROWNER_DETAIL, &HapiCoordsInfo), false);
	
	if (HapiCoordsInfo.exists)
	{
		TArray<float> CurveInputPointsFloat;
		CurveInputPointsFloat.SetNum(HapiCoordsInfo.totalArrayElements);
		TArray<int> CurveSizes;
		CurveSizes.SetNum(1); // Should be curve count, but only allow 1 for now.
		
		HOUDINI_CHECK_ERROR_RETURN(	FHoudiniApi::GetAttributeFloatArrayData(
			FHoudiniEngine::Get().GetSession(),
			CurveNode_id,
			PartId,
			HAPI_UNREAL_ATTRIB_HAPI_INPUT_CURVE_COORDS,
			&HapiCoordsInfo,
			CurveInputPointsFloat.GetData(),
			HapiCoordsInfo.totalArrayElements,
			CurveSizes.GetData(),
			0,
			HapiCoordsInfo.count), false);

		TArray<FVector> CurveInputPoints;
		CurveInputPoints.SetNum(CurveInputPointsFloat.Num() / 3);

		// Coordinate conversion
		FHoudiniEngineUtils::ConvertHoudiniPositionToUnrealVector(CurveInputPointsFloat, CurveInputPoints);

		// Build curve points for editable curves.
		if (HoudiniSplineComponent->CurvePoints.Num() != CurveInputPoints.Num()) 
		{
			HoudiniSplineComponent->CurvePoints.SetNum(CurveInputPoints.Num());
			for(int32 Idx = 0; Idx < CurveInputPoints.Num(); Idx++)
			{
				FTransform Transform = FTransform::Identity;
				Transform.SetLocation(CurveInputPoints[Idx]);
				HoudiniSplineComponent->CurvePoints[Idx] = Transform;
			}
		}
	}

	TArray<float> RefinedCurvePositions;
	FHoudiniHapiAccessor Accessor(CurveNode_id, 0, HAPI_UNREAL_ATTRIB_POSITION);
	if (!Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, RefinedCurvePositions))
	{
		return false;
	}

	TArray<FVector> CurveDisplayPoints;
	FHoudiniEngineUtils::ConvertHoudiniPositionToUnrealVector(RefinedCurvePositions, CurveDisplayPoints);

	// Update the display point on the curve
	HoudiniSplineComponent->Construct(CurveDisplayPoints);

	HoudiniSplineComponent->MarkChanged(false);

	return true;
}

bool FHoudiniSplineTranslator::UpdateHoudiniCurveLegacy(UHoudiniSplineComponent* HoudiniSplineComponent)
{
	if (!IsValid(HoudiniSplineComponent))
		return false;

	int32 CurveNode_id = HoudiniSplineComponent->GetNodeId();
	if (CurveNode_id < 0)
		return false;
	
	FString CurvePointsString = FString();
	if (!FHoudiniEngineUtils::HapiGetParameterDataAsString(
		CurveNode_id, HAPI_UNREAL_PARAM_CURVE_COORDS, TEXT(""), CurvePointsString))
	{
		return false;
	}

	int32 CurveTypeValue = (int32)EHoudiniCurveType::Bezier;
	if (!FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
		CurveNode_id, HAPI_UNREAL_PARAM_CURVE_TYPE, 0, CurveTypeValue))
	{
		return false;
	}
	HoudiniSplineComponent->SetCurveType((EHoudiniCurveType)CurveTypeValue);

	int32 CurveMethodValue = (int32)EHoudiniCurveMethod::CVs;
	if (!FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
		CurveNode_id, HAPI_UNREAL_PARAM_CURVE_METHOD, 0, CurveMethodValue))
	{
		return false;
	}
	HoudiniSplineComponent->SetCurveMethod((EHoudiniCurveMethod)CurveMethodValue);

	int32 CurveClosed = 0;
	if (!FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
		CurveNode_id, HAPI_UNREAL_PARAM_CURVE_CLOSED, 0, CurveClosed))
	{
		return false;
	}
	HoudiniSplineComponent->SetClosedCurve(CurveClosed == 1);

	int32 CurveReversed = 0;
	if (!FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
		CurveNode_id, HAPI_UNREAL_PARAM_CURVE_REVERSED, 0, CurveReversed))
	{
		return false;
	}
	HoudiniSplineComponent->SetReversed(CurveReversed == 1);

	// We need to get the NodeInfo to get the parent id
	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);
	HOUDINI_CHECK_ERROR_RETURN(	FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), CurveNode_id, &NodeInfo), false);

	TArray<float> RefinedCurvePositions;
	FHoudiniHapiAccessor Accessor(CurveNode_id, 0 , HAPI_UNREAL_ATTRIB_POSITION);
	if (!Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, RefinedCurvePositions))
	{
		return false;
	}

	// Process coords string and extract positions.
	TArray<FVector> CurvePoints;
	FHoudiniSplineTranslator::ExtractStringPositions(CurvePointsString, CurvePoints);

	TArray<FVector> CurveDisplayPoints;
	FHoudiniEngineUtils::ConvertHoudiniPositionToUnrealVector(RefinedCurvePositions, CurveDisplayPoints);

	// Build curve points for editable curves.
	if (HoudiniSplineComponent->CurvePoints.Num() != CurvePoints.Num()) 
	{
		HoudiniSplineComponent->CurvePoints.SetNum(CurvePoints.Num());
		for(int32 Idx = 0; Idx < CurvePoints.Num(); Idx++)
		{
			FTransform Transform = FTransform::Identity;
			Transform.SetLocation(CurvePoints[Idx]);
			HoudiniSplineComponent->CurvePoints[Idx] = Transform;
		}
	}

	// Update the display point on the curve
	HoudiniSplineComponent->Construct(CurveDisplayPoints);

	HoudiniSplineComponent->MarkChanged(false);

	return true;
}


bool 
FHoudiniSplineTranslator::HapiUpdateNodeForHoudiniSplineComponent(
	UHoudiniSplineComponent* HoudiniSplineComponent,
	bool bInAddRotAndScaleAttributes)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniSplineTranslator::HapiUpdateNodeForHoudiniSplineComponent);

	if (!IsValid(HoudiniSplineComponent))
		return true;

	TArray<FVector> PositionArray;
	TArray<FQuat> RotationArray;
	TArray<FVector> Scales3dArray;
	for (FTransform& CurrentTransform : HoudiniSplineComponent->CurvePoints)
	{
		PositionArray.Add(CurrentTransform.GetLocation());
		if (bInAddRotAndScaleAttributes)
		{
			RotationArray.Add(CurrentTransform.GetRotation());
			Scales3dArray.Add(CurrentTransform.GetScale3D());
		}
	}

	HAPI_NodeId CurveNode_id = HoudiniSplineComponent->GetNodeId();
	FTransform ParentTransform = HoudiniSplineComponent->GetComponentTransform();

	FString InputNodeNameString = HoudiniSplineComponent->GetName();
	UHoudiniInputHoudiniSplineComponent* InputObject = Cast<UHoudiniInputHoudiniSplineComponent>(HoudiniSplineComponent->GetOuter());
	if (InputObject)
	{
		UHoudiniInput* Input = Cast<UHoudiniInput>(InputObject->GetOuter());
		if (Input)
		{
			InputNodeNameString = Input->GetNodeBaseName();
		}
	}
	InputNodeNameString += TEXT("_curve");

	bool Success = FHoudiniSplineTranslator::HapiCreateCurveInputNodeForData(
		CurveNode_id,
		-1,
		InputNodeNameString,
		&PositionArray,
		bInAddRotAndScaleAttributes ? &RotationArray : nullptr,
		bInAddRotAndScaleAttributes ? &Scales3dArray : nullptr,
		HoudiniSplineComponent->GetCurveType(),
		HoudiniSplineComponent->GetCurveMethod(),
		HoudiniSplineComponent->IsClosedCurve(),
		HoudiniSplineComponent->IsReversed(),
		false,
		ParentTransform,
		HoudiniSplineComponent->IsLegacyInputCurve(),
		HoudiniSplineComponent->GetCurveOrder(),
		HoudiniSplineComponent->GetCurveBreakpointParameterization()
	);

	HoudiniSplineComponent->SetNodeId(CurveNode_id);
	Success &= UpdateHoudiniCurve(HoudiniSplineComponent);

	return Success;
}

bool
FHoudiniSplineTranslator::IsCurveInputNodeValid(const HAPI_NodeId& InNodeId, const bool& bLegacyNode)
{
	// Check if connected asset id is valid, if it is not, we need to create an input asset.
	if(InNodeId < 0)
		return false;

	if (bLegacyNode)
	{
		// Legacy Curve::1.0 node
		// Check if the input node has a curve "coords" params.
		// If it doesn't, then the node is not a valid legacy curve1.0 node
		HAPI_ParmId ParmId = -1;
		if (FHoudiniApi::GetParmIdFromName(
			FHoudiniEngine::Get().GetSession(), InNodeId, HAPI_UNREAL_PARAM_CURVE_COORDS, &ParmId) != HAPI_RESULT_SUCCESS
			|| ParmId < 0)
		{
			return false;
		}
	}
	else
	{
		// New input curves
		// First, check that the geo type is a curve
		HAPI_GeoInfo InputGeoInfo;
		FHoudiniApi::GeoInfo_Init(&InputGeoInfo);
		if (FHoudiniApi::GetGeoInfo(FHoudiniEngine::Get().GetSession(), InNodeId, &InputGeoInfo) != HAPI_RESULT_SUCCESS)
			return false;

		if (InputGeoInfo.type != HAPI_GEOTYPE_CURVE)
			return false;

		// Check that the part type is a curve
		HAPI_PartInfo InputPartInfo;
		FHoudiniApi::PartInfo_Init(&InputPartInfo);
		if (FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), InNodeId, 0, &InputPartInfo) != HAPI_RESULT_SUCCESS)
			return false;

		if (InputPartInfo.type != HAPI_PARTTYPE_CURVE)
			return false;

		// Check if the input node has a curve "coords" params.
		// If it does, then the node a legacy curve1.0 node, so we need to recreate the input node!
		HAPI_ParmId ParmId = -1;
		if (FHoudiniApi::GetParmIdFromName(
			FHoudiniEngine::Get().GetSession(), InNodeId, HAPI_UNREAL_PARAM_CURVE_COORDS, &ParmId) == HAPI_RESULT_SUCCESS
			&& ParmId >= 0)
			return false;

		// Previous method. 
		// This caused crashes in HAPI/HARS and forced the user to restart the session manually!
		// We already have an input node, make sure that it's a curve input	
		// if (FHoudiniApi::GetInputCurveInfo(FHoudiniEngine::Get().GetSession(), CurveNodeId, 0, &InputCurveInfo) != HAPI_RESULT_SUCCESS)
		// return false;
	}	

	// We can reuse the node!
	return true;
}


bool
FHoudiniSplineTranslator::HapiCreateCurveInputNodeForData(
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
	const bool& InForceClose,
	const FTransform& ParentTransform,
	const bool & InIsLegacyCurve,
	const int32& InOrder,
	const EHoudiniCurveBreakpointParameterization& InBreakpointParameterization
	 )
{
	if (InIsLegacyCurve)
	{
		return HapiCreateCurveInputNodeForDataLegacy(CurveNodeId, ParentNodeId, InputNodeName, Positions, Rotations, Scales3d, InCurveType, InCurveMethod, InClosed, InReversed, InForceClose, ParentTransform);
	}
	
#if WITH_EDITOR

	// We also need a valid host asset and 2 points to make a curve
	int32 NumberOfCVs = Positions->Num();
	if (NumberOfCVs < 2)
		return false;

	// Check if connected asset id is valid, if it is not, we need to create an input asset.
	bool bNeedToCreateNewCurveInputNode = !IsCurveInputNodeValid(CurveNodeId);

	HAPI_InputCurveInfo InputCurveInfo;
	FHoudiniApi::InputCurveInfo_Init(&InputCurveInfo);
	if (bNeedToCreateNewCurveInputNode)
	{
		// Create a new curve input Node
		HAPI_NodeId NodeId = -1;
		if (!FHoudiniSplineTranslator::HapiCreateCurveInputNode(NodeId, ParentNodeId, InputNodeName, InIsLegacyCurve))
			return false;

		// Check if we have a valid id for this new input asset.
		if (!FHoudiniEngineUtils::IsHoudiniNodeValid(NodeId))
			return false;

		// We now have a valid id.
		HAPI_NodeId PreviousInputNodeId = CurveNodeId;
		CurveNodeId = NodeId;

		// We have now created a valid new input node, delete the previous one
		if (PreviousInputNodeId >= 0)
		{
			// Get the parent OBJ node ID before deleting!
			HAPI_NodeId PreviousInputOBJNode = FHoudiniEngineUtils::HapiGetParentNodeId(PreviousInputNodeId);

			if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
				FHoudiniEngine::Get().GetSession(), PreviousInputNodeId))
			{
				HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input curve node"));
			}

			if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
				FHoudiniEngine::Get().GetSession(), PreviousInputOBJNode))
			{
				HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input curve OBJ node."));
			}
		}
	}

	InputCurveInfo.curveType = (HAPI_CurveType)InCurveType;
	InputCurveInfo.inputMethod = (HAPI_InputCurveMethod)InCurveMethod;
	InputCurveInfo.closed = InClosed;
	InputCurveInfo.reverse = InReversed;
	InputCurveInfo.order = InOrder;
	InputCurveInfo.breakpointParameterization = (HAPI_InputCurveParameterization)InBreakpointParameterization;
	
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetInputCurveInfo(
        FHoudiniEngine::Get().GetSession(), CurveNodeId, 0,
        &InputCurveInfo), false);

	TArray<float> CurvePositions;
	CurvePositions.SetNum(NumberOfCVs * 3);

	for (int32 i = 0; i < Positions->Num(); i++)
	{
		CurvePositions[i*3 + 0] = (float)((*Positions)[i].X) / HAPI_UNREAL_SCALE_FACTOR_POSITION;
		// Swap Y/Z
		CurvePositions[i*3 + 1] = (float)((*Positions)[i].Z) / HAPI_UNREAL_SCALE_FACTOR_POSITION;
		CurvePositions[i*3 + 2] = (float)((*Positions)[i].Y) / HAPI_UNREAL_SCALE_FACTOR_POSITION;
	}

	TArray<float> CurveRotations;
	TArray<float> CurveScales;
	
	bool bAddRotations = (Rotations != nullptr && Rotations->Num() == Positions->Num());
	bool bAddScales3d = (Scales3d != nullptr && Scales3d->Num() == Positions->Num());
	
	if (bAddRotations)
	{
		CurveRotations.SetNum(NumberOfCVs * 4);
		for (int32 Idx = 0; Idx < Rotations->Num(); Idx++)
		{
			// Get current quaternion
			const FQuat& RotationQuaternion = (*Rotations)[Idx];

			CurveRotations[Idx * 4 + 0] = RotationQuaternion.X;
			CurveRotations[Idx * 4 + 1] = RotationQuaternion.Z;
			CurveRotations[Idx * 4 + 2] = RotationQuaternion.Y;
			CurveRotations[Idx * 4 + 3] = -RotationQuaternion.W;
		}
	}

	if (bAddScales3d)
	{
		CurveScales.SetNum(NumberOfCVs * 3);
		for (int32 Idx = 0; Idx < Scales3d->Num(); Idx++)
		{
			// Get current scale
			FVector ScaleVector = (*Scales3d)[Idx];
			CurveScales[Idx * 3 + 0] = ScaleVector.X;
			CurveScales[Idx * 3 + 1] = ScaleVector.Z;
			CurveScales[Idx * 3 + 2] = ScaleVector.Y;
		}
	}

	if (bAddRotations || bAddScales3d)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetInputCurvePositionsRotationsScales(
			FHoudiniEngine::Get().GetSession(),
			CurveNodeId, 0,
			CurvePositions.GetData(), 0, CurvePositions.Num(),
			CurveRotations.GetData(), 0, CurveRotations.Num(),
			CurveScales.GetData(), 0, CurveScales.Num()), false);
	}
	else
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetInputCurvePositions(
			FHoudiniEngine::Get().GetSession(),
			CurveNodeId, 0,
			CurvePositions.GetData(), 0, CurvePositions.Num()), false);
	}	
#endif

	return true;
}

bool
FHoudiniSplineTranslator::HapiCreateCurveInputNodeForDataLegacy(
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
	const bool& InForceClose,
	const FTransform& ParentTransform )
{
#if WITH_EDITOR
	// Positions are required
	if (!Positions)
		return false;

	// We also need a valid host asset and 2 points to make a curve
	int32 NumberOfCVs = Positions->Num();
	if (NumberOfCVs < 2)
		return false;

	// Check if connected asset id is valid, if it is not, we need to create an input asset.
	if (!IsCurveInputNodeValid(CurveNodeId, true))
	{
		HAPI_NodeId NodeId = -1;
		// Create the curve SOP Node
		if (!FHoudiniSplineTranslator::HapiCreateCurveInputNode(NodeId, ParentNodeId, InputNodeName, true))
			return false;

		// Check if we have a valid id for this new input asset.
		if (!FHoudiniEngineUtils::IsHoudiniNodeValid(NodeId))
			return false;

		// We now have a valid id.
		HAPI_NodeId PreviousInputNodeId = CurveNodeId;
		CurveNodeId = NodeId;

		// We have now created a valid new input node, delete the previous one
		if (PreviousInputNodeId >= 0)
		{
			// Get the parent OBJ node ID before deleting!
			HAPI_NodeId PreviousInputOBJNode = FHoudiniEngineUtils::HapiGetParentNodeId(PreviousInputNodeId);

			if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
				FHoudiniEngine::Get().GetSession(), PreviousInputNodeId))
			{
				HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input curve node"));
			}

			if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
				FHoudiniEngine::Get().GetSession(), PreviousInputOBJNode))
			{
				HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input curve OBJ node."));
			}
		}
	}
	else
	{
		// We have to revert the Geo to its original state so we can use the Curve SOP:
		// adding parameters to the Curve SOP locked it, preventing its parameters (type, method, isClosed) from working
		FHoudiniApi::RevertGeo(FHoudiniEngine::Get().GetSession(), CurveNodeId);
	}

	//
	// In order to be able to add rotations and scale attributes to the curve SOP, we need to cook it twice:
	// 
	// - First, we send the positions string to it, and cook it without refinement.
	//   this will allow us to get the proper curve CVs, part attributes and curve info to create the desired curve.
	//
	// - We then need to send back all the info extracted from the curve SOP to it, and add the rotation 
	//   and scale attributes to it. This will lock the curve SOP, and prevent the curve type and method 
	//   parameters from functioning properly (hence why we needed the first cook to set that up)
	//

	// Set the curve type and curve method parameters for the curve node
	int32 CurveTypeValue = (int32)InCurveType;
	FHoudiniApi::SetParmIntValue(
		FHoudiniEngine::Get().GetSession(), CurveNodeId,
		HAPI_UNREAL_PARAM_CURVE_TYPE, 0, CurveTypeValue);

	int32 CurveMethodValue = (int32)InCurveMethod;
	FHoudiniApi::SetParmIntValue(
		FHoudiniEngine::Get().GetSession(), CurveNodeId,
		HAPI_UNREAL_PARAM_CURVE_METHOD, 0, CurveMethodValue);

	int32 CurveClosed = InClosed ? 1 : 0;
	FHoudiniApi::SetParmIntValue(
		FHoudiniEngine::Get().GetSession(), CurveNodeId,
		HAPI_UNREAL_PARAM_CURVE_CLOSED, 0, CurveClosed);

	int32 CurveReversed = InReversed ? 1 : 0;
	FHoudiniApi::SetParmIntValue(
		FHoudiniEngine::Get().GetSession(), CurveNodeId,
		HAPI_UNREAL_PARAM_CURVE_REVERSED, 0, CurveReversed);

	// Reading the curve parameters
	FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
		CurveNodeId, HAPI_UNREAL_PARAM_CURVE_TYPE, 0, CurveTypeValue);
	FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
		CurveNodeId, HAPI_UNREAL_PARAM_CURVE_METHOD, 0, CurveMethodValue);
	FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
		CurveNodeId, HAPI_UNREAL_PARAM_CURVE_CLOSED, 0, CurveClosed);
	FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
		CurveNodeId, HAPI_UNREAL_PARAM_CURVE_REVERSED, 0, CurveReversed);

	if (InForceClose)
	{
		// We need to update the closed parameter
		FHoudiniApi::SetParmIntValue(
			FHoudiniEngine::Get().GetSession(), CurveNodeId, HAPI_UNREAL_PARAM_CURVE_CLOSED, 0, 1);

		CurveClosed = 1;
	}

	// For closed NURBS (CVs and Breakpoints), we have to close the curve manually, by duplicating its last point
	// in order to be able to set the rotations and scales attributes properly.
	bool bCloseCurveManually = false;
	if (CurveClosed && (CurveTypeValue == HAPI_CURVETYPE_NURBS) && (CurveMethodValue != 2))
	{
		// The curve is not closed anymore
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::SetParmIntValue(
			FHoudiniEngine::Get().GetSession(), CurveNodeId,
			HAPI_UNREAL_PARAM_CURVE_CLOSED, 0, 0))
		{
			bCloseCurveManually = true;

			// Duplicating the first point to the end of the curve
			// This needs to be done before sending the position string
			FVector pos = (*Positions)[0];
			Positions->Add(pos);

			CurveClosed = false;
		}
	}

	// Creating the position string
	FString PositionString = TEXT("");
	FHoudiniSplineTranslator::CreatePositionsString(*Positions, PositionString);

	// Get param id for the PositionString and modify it
	HAPI_ParmId ParmId = -1;
	if (FHoudiniApi::GetParmIdFromName(
		FHoudiniEngine::Get().GetSession(), CurveNodeId,
		HAPI_UNREAL_PARAM_CURVE_COORDS, &ParmId) != HAPI_RESULT_SUCCESS)
	{
		return false;
	}

	std::string ConvertedString = TCHAR_TO_UTF8(*PositionString);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(
		FHoudiniEngine::Get().GetSession(), CurveNodeId,
		ConvertedString.c_str(), ParmId, 0), false);

	// If we don't want to add rotations or scale attributes to the curve, 
	// we can just cook the node normally and stop here.
	bool bAddRotations = (Rotations != nullptr);
	bool bAddScales3d = (Scales3d != nullptr);
	if (!bAddRotations && !bAddScales3d)
	{
		/*
		HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
		HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CookNode(
			FHoudiniEngine::Get().GetSession(), CurveNodeId, &CookOptions), false);
		*/

		// Cook the node, no need to wait for completion
		return FHoudiniEngineUtils::HapiCookNode(CurveNodeId, nullptr, false);
	}

	// Setting up the first cook, without the curve refinement
	HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
	CookOptions.maxVerticesPerPrimitive = -1;
	CookOptions.refineCurveToLinear = false;
	if(!FHoudiniEngineUtils::HapiCookNode(CurveNodeId, &CookOptions, true))
		return false;

	//  We can now read back the Part infos from the cooked curve.
	HAPI_PartInfo PartInfos;
	FHoudiniApi::PartInfo_Init(&PartInfos);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(
		FHoudiniEngine::Get().GetSession(), CurveNodeId, 0, &PartInfos), false);

	//
	// Depending on the curve type and method, additionnal control points might have been created.
	// We now have to interpolate the rotations and scale attributes for these.
	//

	// Lambda function that interpolates rotation, scale and uniform scales values
	// between two points using fCoeff as a weight, and insert the interpolated value at nInsertIndex
	auto InterpolateRotScaleUScale = [&](const int32& nIndex1, const int32& nIndex2, const float& fCoeff, const int32& nInsertIndex)
	{
		if (Rotations && Rotations->IsValidIndex(nIndex1) && Rotations->IsValidIndex(nIndex2))
		{
			FQuat interpolation = FQuat::Slerp((*Rotations)[nIndex1], (*Rotations)[nIndex2], fCoeff);
			if (Rotations->IsValidIndex(nInsertIndex))
				Rotations->Insert(interpolation, nInsertIndex);
			else
				Rotations->Add(interpolation);
		}

		if (Scales3d && Scales3d->IsValidIndex(nIndex1) && Scales3d->IsValidIndex(nIndex2))
		{
			FVector interpolation = fCoeff * (*Scales3d)[nIndex1] + (1.0f - fCoeff) * (*Scales3d)[nIndex2];
			if (Scales3d->IsValidIndex(nInsertIndex))
				Scales3d->Insert(interpolation, nInsertIndex);
			else
				Scales3d->Add(interpolation);
		}
	};

	// Lambda function that duplicates rotation and scale values
	// at nIndex and insert/adds it at nInsertIndex
	auto DuplicateRotScale = [&](const int32& nIndex, const int32& nInsertIndex)
	{
		if (Rotations && Rotations->IsValidIndex(nIndex))
		{
			FQuat value = (*Rotations)[nIndex];
			if (Rotations->IsValidIndex(nInsertIndex))
				Rotations->Insert(value, nInsertIndex);
			else
				Rotations->Add(value);
		}

		if (Scales3d && Scales3d->IsValidIndex(nIndex))
		{
			FVector value = (*Scales3d)[nIndex];
			if (Scales3d->IsValidIndex(nInsertIndex))
				Scales3d->Insert(value, nInsertIndex);
			else
				Scales3d->Add(value);
		}
	};

	// Do we want to close the curve by ourselves?
	if (bCloseCurveManually)
	{
		// We need to duplicate the info of the first point to the last
		DuplicateRotScale(0, NumberOfCVs++);

		// We need to update the closed parameter
		FHoudiniApi::SetParmIntValue(
			FHoudiniEngine::Get().GetSession(), CurveNodeId,
			HAPI_UNREAL_PARAM_CURVE_CLOSED, 0, 1);
	}

	// INTERPOLATION
	if (CurveTypeValue == HAPI_CURVETYPE_NURBS)
	{
		// Closed NURBS have additional points reproducing the first ones
		if (InClosed)
		{
			// Only the first one if the method is freehand ... 
			DuplicateRotScale(0, NumberOfCVs++);

			if (CurveMethodValue != 2)
			{
				// ... but also the 2nd and 3rd if the method is CVs or Breakpoints.
				DuplicateRotScale(1, NumberOfCVs++);
				DuplicateRotScale(2, NumberOfCVs++);
			}
		}
		else if (CurveMethodValue == 1)
		{
			// Open NURBS have 2 new points if the method is breakpoint:
			// One between the 1st and 2nd ...
			InterpolateRotScaleUScale(0, 1, 0.5f, 1);

			// ... and one before the last one.
			InterpolateRotScaleUScale(NumberOfCVs, NumberOfCVs - 1, 0.5f, NumberOfCVs);
			NumberOfCVs += 2;
		}
	}
	else if (CurveTypeValue == HAPI_CURVETYPE_BEZIER)
	{
		// Bezier curves requires additional point if the method is Breakpoints
		if (CurveMethodValue == 1)
		{
			// 2 interpolated control points are added per points (except the last one)
			int32 nOffset = 0;
			for (int32 n = 0; n < NumberOfCVs - 1; n++)
			{
				int nIndex1 = n + nOffset;
				int nIndex2 = n + nOffset + 1;

				InterpolateRotScaleUScale(nIndex1, nIndex2, 0.33f, nIndex2);
				nIndex2++;
				InterpolateRotScaleUScale(nIndex1, nIndex2, 0.66f, nIndex2);

				nOffset += 2;
			}
			NumberOfCVs += nOffset;

			if (CurveClosed)
			{
				// If the curve is closed, we need to add 2 points after the last,
				// interpolated between the last and the first one
				int nIndex = NumberOfCVs - 1;
				InterpolateRotScaleUScale(nIndex, 0, 0.33f, NumberOfCVs++);
				InterpolateRotScaleUScale(nIndex, 0, 0.66f, NumberOfCVs++);

				// and finally, the last point is the first..
				DuplicateRotScale(0, NumberOfCVs++);
			}
		}
		else if (CurveClosed)
		{
			// For the other methods, if the bezier curve is closed, the last point is the 1st
			DuplicateRotScale(0, NumberOfCVs++);
		}
	}

	// Even after interpolation, additional points might still be missing:
	// Bezier curves require a certain number of points regarding their order,
	// if points are lacking then HAPI duplicates the last one.
	if (NumberOfCVs < PartInfos.pointCount)
	{
		int nToAdd = PartInfos.pointCount - NumberOfCVs;
		for (int n = 0; n < nToAdd; n++)
		{
			DuplicateRotScale(NumberOfCVs - 1, NumberOfCVs);
			NumberOfCVs++;
		}
	}

	// To avoid crashes, attributes will only be added if we now have the correct number of them
	bAddRotations = bAddRotations && (Rotations->Num() == PartInfos.pointCount);
	bAddScales3d = bAddScales3d && (Scales3d->Num() == PartInfos.pointCount);

	// We need to increase the point attributes count for points in the Part Infos
	HAPI_AttributeOwner NewAttributesOwner = HAPI_ATTROWNER_POINT;
	HAPI_AttributeOwner OriginalAttributesOwner = HAPI_ATTROWNER_POINT;

	int OriginalPointParametersCount = PartInfos.attributeCounts[NewAttributesOwner];
	if (bAddRotations)
		PartInfos.attributeCounts[NewAttributesOwner] += 1;
	if (bAddScales3d)
		PartInfos.attributeCounts[NewAttributesOwner] += 1;

	// Sending the updated PartInfos
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
		FHoudiniEngine::Get().GetSession(),
		CurveNodeId, 0, &PartInfos), false);

	// We need now to reproduce ALL the curves attributes for ALL the Owners..
	for (int nOwner = 0; nOwner < HAPI_ATTROWNER_MAX; nOwner++)
	{
		int nOwnerAttributeCount = nOwner == NewAttributesOwner ? OriginalPointParametersCount : PartInfos.attributeCounts[nOwner];
		if (nOwnerAttributeCount == 0)
			continue;

		TArray<HAPI_StringHandle> AttributeNamesSH;
		AttributeNamesSH.SetNum(nOwnerAttributeCount);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeNames(
			FHoudiniEngine::Get().GetSession(),
			CurveNodeId, 0,	(HAPI_AttributeOwner)nOwner,
			AttributeNamesSH.GetData(), AttributeNamesSH.Num()), false);

		for (int nAttribute = 0; nAttribute < AttributeNamesSH.Num(); nAttribute++)
		{
			const HAPI_StringHandle sh = AttributeNamesSH[nAttribute];
			if (sh == 0)
				continue;

			// Get the attribute name
			std::string attr_name;
			FHoudiniEngineString::ToStdString(sh, attr_name);
			if (strcmp(attr_name.c_str(), "__topology") == 0)
				continue;

			// and the attribute infos
			HAPI_AttributeInfo attr_info;
			FHoudiniApi::AttributeInfo_Init(&attr_info);
			//FMemory::Memzero< HAPI_AttributeInfo >( attr_info );
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
				FHoudiniEngine::Get().GetSession(),
				CurveNodeId, 0,	attr_name.c_str(),
				(HAPI_AttributeOwner)nOwner, &attr_info), false);

			switch (attr_info.storage)
			{
				case HAPI_STORAGETYPE_INT:
				{
					// Storing IntData
					TArray< int > IntData;
					IntData.SetNumUninitialized(attr_info.count * attr_info.tupleSize);

					// GET
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeIntData(
						FHoudiniEngine::Get().GetSession(),
						CurveNodeId, 0,
						attr_name.c_str(), &attr_info, -1,
						IntData.GetData(), 0, attr_info.count), false);

					// ADD
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(),
						CurveNodeId, 0,
						attr_name.c_str(), &attr_info), false);

					// SET
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntData(
						FHoudiniEngine::Get().GetSession(),
						CurveNodeId, 0,
						attr_name.c_str(),
						&attr_info, IntData.GetData(),
						0, attr_info.count), false);
				}
				break;

				case HAPI_STORAGETYPE_INT64:
				{
					// Storing IntData
					TArray<int64_t> Int64Data;
					Int64Data.SetNumUninitialized(attr_info.count * attr_info.tupleSize);

					// GET
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInt64Data(
						FHoudiniEngine::Get().GetSession(),
						CurveNodeId, 0,
						attr_name.c_str(), &attr_info, -1,
						Int64Data.GetData(), 0, attr_info.count), false);

					// ADD
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(),
						CurveNodeId, 0,
						attr_name.c_str(), &attr_info), false);

					// SET
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeInt64Data(
						FHoudiniEngine::Get().GetSession(),
						CurveNodeId, 0,
						attr_name.c_str(),
						&attr_info, Int64Data.GetData(),
						0, attr_info.count), false);
				}
				break;

				case HAPI_STORAGETYPE_FLOAT:
				{
					// Storing Float Data
					TArray< float > FloatData;
					FloatData.SetNumUninitialized(attr_info.count * attr_info.tupleSize);

					// GET
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeFloatData(
						FHoudiniEngine::Get().GetSession(),
						CurveNodeId, 0,
						attr_name.c_str(), &attr_info, -1,
						FloatData.GetData(),
						0, attr_info.count), false);

					// ADD
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(),
						CurveNodeId, 0,
						attr_name.c_str(),
						&attr_info), false);

					// SET
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
						FHoudiniEngine::Get().GetSession(),
						CurveNodeId, 0,
						attr_name.c_str(), &attr_info,
						FloatData.GetData(),
						0, attr_info.count), false);
				}
				break;

				case HAPI_STORAGETYPE_STRING:
				{
					// Storing String Data
					TArray<HAPI_StringHandle> StringHandleData;
					StringHandleData.SetNumUninitialized(attr_info.count * attr_info.tupleSize);

					// GET
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeStringData(
						FHoudiniEngine::Get().GetSession(),
						CurveNodeId, 0,
						attr_name.c_str(), &attr_info,
						StringHandleData.GetData(),
						0, attr_info.count), false);

					// Convert the SH to const char *
					TArray<const char *> StringData;
					StringData.SetNumUninitialized(attr_info.count);
					for (int n = 0; n < StringHandleData.Num(); n++)
					{
						// Converting the string
						std::string strSTD;
						FHoudiniEngineString::ToStdString(sh, strSTD);

						StringData[n] = strSTD.c_str();
					}

					// ADD
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(),
						CurveNodeId, 0,
						attr_name.c_str(),
						&attr_info), false);

					// SET
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeStringData(
						FHoudiniEngine::Get().GetSession(),
						CurveNodeId, 0,
						attr_name.c_str(), &attr_info,
						StringData.GetData(),
						0, attr_info.count), false);
				}
				break;

				default:
				{
					// Unhandled attribute type - warn
					HOUDINI_LOG_WARNING(
						TEXT("HapiCreateCurveInputNodeForData() - Unhandled attribute type - skipping")
						TEXT("- consider disabling additionnal rot/scale if having issues with the HDA"));
					continue;
				}

			}
		}
	}

	// Only GET/SET curve infos if the part is a curve...
	// (Closed linear curves are actually not considered as curves...)
	if (PartInfos.type == HAPI_PARTTYPE_CURVE)
	{
		// We need to read the curve infos ...
		HAPI_CurveInfo CurveInfo;
		FHoudiniApi::CurveInfo_Init(&CurveInfo);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetCurveInfo(
			FHoudiniEngine::Get().GetSession(),
			CurveNodeId, 0,
			&CurveInfo), false);

		// ... the curve counts
		TArray< int > CurveCounts;
		CurveCounts.SetNumUninitialized(CurveInfo.curveCount);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetCurveCounts(
			FHoudiniEngine::Get().GetSession(),
			CurveNodeId, 0,
			CurveCounts.GetData(),
			0, CurveInfo.curveCount), false);

		// .. the curve orders
		TArray< int > CurveOrders;
		CurveOrders.SetNumUninitialized(CurveInfo.curveCount);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetCurveOrders(
			FHoudiniEngine::Get().GetSession(),
			CurveNodeId, 0,
			CurveOrders.GetData(),
			0, CurveInfo.curveCount), false);

		// .. And the Knots if they exist.
		TArray< float > KnotsArray;
		if (CurveInfo.hasKnots)
		{
			KnotsArray.SetNumUninitialized(CurveInfo.knotCount);
			HOUDINI_CHECK_ERROR_RETURN(
				FHoudiniApi::GetCurveKnots(
					FHoudiniEngine::Get().GetSession(),
					CurveNodeId, 0,
					KnotsArray.GetData(),
					0, CurveInfo.knotCount), false);
		}

		// To set them back in HAPI
		// CurveInfo
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetCurveInfo(
			FHoudiniEngine::Get().GetSession(),
			CurveNodeId, 0,
			&CurveInfo), false);

		// CurveCounts
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetCurveCounts(
			FHoudiniEngine::Get().GetSession(),
			CurveNodeId, 0,
			CurveCounts.GetData(),
			0, CurveInfo.curveCount), false);

		// CurveOrders
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetCurveOrders(
			FHoudiniEngine::Get().GetSession(),
			CurveNodeId, 0,
			CurveOrders.GetData(),
			0, CurveInfo.curveCount), false);

		// And Knots if they exist
		if (CurveInfo.hasKnots)
		{
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetCurveKnots(
				FHoudiniEngine::Get().GetSession(),
				CurveNodeId, 0,
				KnotsArray.GetData(),
				0, CurveInfo.knotCount), false);
		}
	}

	if (PartInfos.faceCount > 0)
	{
		// getting the face counts
		TArray< int > FaceCounts;
		FaceCounts.SetNumUninitialized(PartInfos.faceCount);

		if (FHoudiniApi::GetFaceCounts(
			FHoudiniEngine::Get().GetSession(),
			CurveNodeId, 0,
			FaceCounts.GetData(), 0,
			PartInfos.faceCount) == HAPI_RESULT_SUCCESS)
		{
			// Set the face count
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetFaceCounts(
				FHoudiniEngine::Get().GetSession(),
				CurveNodeId, 0,
				FaceCounts.GetData(),
				0, PartInfos.faceCount), false);
		}
	}

	if (PartInfos.vertexCount > 0)
	{
		// the vertex list
		TArray<int> VertexList;
		VertexList.SetNumUninitialized(PartInfos.vertexCount);

		if (FHoudiniApi::GetVertexList(
			FHoudiniEngine::Get().GetSession(),
			CurveNodeId, 0,
			VertexList.GetData(),
			0, PartInfos.vertexCount) == HAPI_RESULT_SUCCESS)
		{
			// setting the vertex list
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetVertexList(
				FHoudiniEngine::Get().GetSession(),
				CurveNodeId, 0,
				VertexList.GetData(),
				0, PartInfos.vertexCount), false);
		}
	}

	// We can add attributes to the curve now that all the curves attributes
	// and properties have been reset.
	if (bAddRotations)
	{
		// Create ROTATION attribute info
		HAPI_AttributeInfo AttributeInfoRotation;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoRotation);
		AttributeInfoRotation.count = NumberOfCVs;
		AttributeInfoRotation.tupleSize = 4;
		AttributeInfoRotation.exists = true;
		AttributeInfoRotation.owner = NewAttributesOwner;
		AttributeInfoRotation.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoRotation.originalOwner = OriginalAttributesOwner;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			CurveNodeId, 0,
			HAPI_UNREAL_ATTRIB_ROTATION,
			&AttributeInfoRotation), false);

		// Convert the rotation infos
		TArray< float > CurveRotations;
		CurveRotations.SetNumZeroed(NumberOfCVs * 4);
		for (int32 Idx = 0; Idx < NumberOfCVs; ++Idx)
		{
			// Get current quaternion
			const FQuat& RotationQuaternion = (*Rotations)[Idx];

			CurveRotations[Idx * 4 + 0] = RotationQuaternion.X;
			CurveRotations[Idx * 4 + 1] = RotationQuaternion.Z;
			CurveRotations[Idx * 4 + 2] = RotationQuaternion.Y;
			CurveRotations[Idx * 4 + 3] = -RotationQuaternion.W;
		}

		//we can now upload them to our attribute.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
			FHoudiniEngine::Get().GetSession(),
			CurveNodeId, 0,
			HAPI_UNREAL_ATTRIB_ROTATION,
			&AttributeInfoRotation,
			CurveRotations.GetData(),
			0, AttributeInfoRotation.count), false);
	}

	// Create SCALE attribute info.
	if (bAddScales3d)
	{
		HAPI_AttributeInfo AttributeInfoScale;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoScale);
		AttributeInfoScale.count = NumberOfCVs;
		AttributeInfoScale.tupleSize = 3;
		AttributeInfoScale.exists = true;
		AttributeInfoScale.owner = NewAttributesOwner;
		AttributeInfoScale.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoScale.originalOwner = OriginalAttributesOwner;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			CurveNodeId, 0,
			HAPI_UNREAL_ATTRIB_SCALE,
			&AttributeInfoScale), false);

		// Convert the scale
		TArray< float > CurveScales;
		CurveScales.SetNumZeroed(NumberOfCVs * 3);
		for (int32 Idx = 0; Idx < NumberOfCVs; ++Idx)
		{
			// Get current scale
			FVector ScaleVector = (*Scales3d)[Idx];
			CurveScales[Idx * 3 + 0] = ScaleVector.X;
			CurveScales[Idx * 3 + 1] = ScaleVector.Z;
			CurveScales[Idx * 3 + 2] = ScaleVector.Y;
		}

		// We can now upload them to our attribute.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
			FHoudiniEngine::Get().GetSession(),
			CurveNodeId, 0,
			HAPI_UNREAL_ATTRIB_SCALE,
			&AttributeInfoScale,
			CurveScales.GetData(),
			0, AttributeInfoScale.count), false);
	}

	// Finally, commit the geo ...
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(CurveNodeId), false);

	// And cook it with refinement enabled
	CookOptions.refineCurveToLinear = true;
	if(!FHoudiniEngineUtils::HapiCookNode(CurveNodeId, &CookOptions, false))
		return false;
#endif

	return true;
}

void
FHoudiniSplineTranslator::CreatePositionsString(const TArray<FVector>& InPositions, FString& OutPositionString)
{
	OutPositionString = TEXT("");
	for (int32 Idx = 0; Idx < InPositions.Num(); ++Idx)
	{
		FVector Position = InPositions[Idx];	
		// Convert to meters
		Position /= HAPI_UNREAL_SCALE_FACTOR_POSITION;
		// Swap Y/Z
		OutPositionString += FString::Printf(TEXT("%f, %f, %f "), Position.X, Position.Z, Position.Y);
	}
}

bool
FHoudiniSplineTranslator::HapiCreateCurveInputNode(
	HAPI_NodeId& OutCurveNodeId, 
	const HAPI_NodeId& ParentNodeId, 
	const FString& InputNodeName, 
	const bool& InIsLegacyCurve)
{
	// Create the curve SOP Node
	HAPI_NodeId NewNodeId = -1;
	if (InIsLegacyCurve)
	{
		// Create the curve SOP Node
		if (ParentNodeId < 0)
		{
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(
				-1, TEXT("SOP/curve::"), InputNodeName, false, &NewNodeId), false);
		}
		else
		{
			// When creating a node inside a parent node (in other words, ParentNodeId is not -1), then we cannot
			// specify the node type category prefix on the node name.
			HAPI_NodeId ObjectNodeId = -1;
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(
				ParentNodeId, TEXT("geo"), InputNodeName, true, &ObjectNodeId), false);
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(
				ObjectNodeId, TEXT("curve::"), InputNodeName, false, &NewNodeId), false);
		}

		OutCurveNodeId = NewNodeId;

		// Submit default points to curve.
		HAPI_ParmId ParmId = -1;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmIdFromName(
			FHoudiniEngine::Get().GetSession(), NewNodeId,
			HAPI_UNREAL_PARAM_CURVE_COORDS, &ParmId), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(
			FHoudiniEngine::Get().GetSession(), NewNodeId,
			HAPI_UNREAL_PARAM_INPUT_CURVE_COORDS_DEFAULT, ParmId, 0), false);

		// Cook the newly created node
		return FHoudiniEngineUtils::HapiCookNode(NewNodeId, nullptr, true);
	}
	else
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateInputCurveNode(
			FHoudiniEngine::Get().GetSession(), -1, &NewNodeId, TCHAR_TO_UTF8(*InputNodeName)), false);

		OutCurveNodeId = NewNodeId;

		TArray<float> DefaultPositions {0,0,3,3,0,3};
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetInputCurvePositions(
			FHoudiniEngine::Get().GetSession(), NewNodeId, 0, DefaultPositions.GetData(), 0, DefaultPositions.Num()), false);

		return true;
	}
}

UHoudiniSplineComponent* 
FHoudiniSplineTranslator::CreateHoudiniSplineComponentFromHoudiniEditableNode(const int32 & GeoId, const FString & PartName, UObject* OuterComponent) 
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniSplineTranslator::CreateHoudiniSplineComponentFromHoudiniEditableNode);
	if (GeoId < 0)
		return nullptr;

	if (!IsValid(OuterComponent))
		return nullptr;

	USceneComponent* const SceneComponent = Cast<USceneComponent>(OuterComponent);
	if (!IsValid(SceneComponent))
		return nullptr;

	// Create a HoudiniSplineComponent for the editable curve.
	UHoudiniSplineComponent* HoudiniSplineComponent = NewObject<UHoudiniSplineComponent>(
		OuterComponent,
		UHoudiniSplineComponent::StaticClass(), NAME_None, RF_Public | RF_Transactional);

	HoudiniSplineComponent->SetNodeId(GeoId);
	HoudiniSplineComponent->SetGeoPartName(PartName);
	bool IsLegacyCurve = false;

	// Check if it is a legacy curve
	int32 ParmId = -1;
	
	if (FHoudiniApi::GetParmIdFromName(
		FHoudiniEngine::Get().GetSession(),
		GeoId, HAPI_UNREAL_PARAM_CURVE_COORDS, &ParmId) == HAPI_RESULT_SUCCESS && ParmId > 0)
	{
		IsLegacyCurve = true;
	}

	HoudiniSplineComponent->SetIsLegacyInputCurve(IsLegacyCurve);
	
	HoudiniSplineComponent->RegisterComponent();
	HoudiniSplineComponent->AttachToComponent(SceneComponent, FAttachmentTransformRules::KeepRelativeTransform);

	// Delete the curve points so that UpdateHoudiniCurves initializes them from HAPI
	HoudiniSplineComponent->CurvePoints.Empty();
	HoudiniSplineComponent->DisplayPoints.Empty();
	UpdateHoudiniCurve(HoudiniSplineComponent);

	ReselectSelectedActors();

	return HoudiniSplineComponent;
	
}

UHoudiniSplineComponent*
FHoudiniSplineTranslator::CreateOutputHoudiniSplineComponent(UHoudiniAssetComponent* OuterHAC, TArray<FVector>& CurvePoints, const TArray<FVector>& CurveRotations, const TArray<FVector>& CurveScales)
{
	if (!IsValid(OuterHAC))
		return nullptr;

	UObject* Outer = nullptr;
	if (IsValid(OuterHAC))
		Outer = OuterHAC->GetOwner() ? OuterHAC->GetOwner() : OuterHAC->GetOuter();

	UHoudiniSplineComponent *NewHoudiniSplineComponent = NewObject<UHoudiniSplineComponent>(Outer, UHoudiniSplineComponent::StaticClass(), NAME_None, RF_Transactional);

	if (!NewHoudiniSplineComponent)
		return nullptr;

	NewHoudiniSplineComponent->Construct(CurvePoints);

	bool bHasRotations = CurveRotations.Num() == CurvePoints.Num();
	bool bHasScales = CurveScales.Num() == CurvePoints.Num();

	TArray<FTransform> Transforms;
	for (int32 n = 0; n < CurvePoints.Num(); ++n) 
	{
		FTransform NextTransform = FTransform::Identity;
		NextTransform.SetLocation(CurvePoints[n]);
		
		if (bHasRotations)
			NextTransform.SetRotation(CurveRotations[n].Rotation().Quaternion());

		if (bHasScales)
			NextTransform.SetScale3D(CurveScales[n]);

		Transforms.Add(NextTransform);
	}

	NewHoudiniSplineComponent->CurveType = EHoudiniCurveType::Polygon;
	NewHoudiniSplineComponent->bIsOutputCurve = true;

	NewHoudiniSplineComponent->AttachToComponent(OuterHAC, FAttachmentTransformRules::KeepRelativeTransform);
	NewHoudiniSplineComponent->RegisterComponent();

	ReselectSelectedActors();
	
	return NewHoudiniSplineComponent;
}

USplineComponent* 
FHoudiniSplineTranslator::CreateOutputUnrealSplineComponent(
	UObject* OuterComponent,
	const TArray<FVector>& CurvePoints,
	const TArray<FVector>& CurveRotations,
	const TArray<FVector>& CurveScales,
	const bool& bIsLinear,
	const bool& bIsClosed)
{
	if (!IsValid(OuterComponent))
		return nullptr;

	USceneComponent* OuterSceneComponent = Cast<USceneComponent>(OuterComponent);
	if (!IsValid(OuterSceneComponent))
		return nullptr;
	
	UObject* Outer = nullptr;
	Outer = OuterSceneComponent->GetOwner() ? OuterSceneComponent->GetOwner() : OuterSceneComponent->GetOuter();

	USplineComponent* NewSplineComponent = NewObject<USplineComponent>(Outer, USplineComponent::StaticClass(), NAME_None, RF_Transactional);

	if (!NewSplineComponent)
		return nullptr;

	// Clear default USplineComponent's points
	NewSplineComponent->ClearSplinePoints();
	NewSplineComponent->bEditableWhenInherited = false;
 
	ESplinePointType::Type SplinePointType = bIsLinear ? ESplinePointType::Linear : ESplinePointType::Curve;
	for (int32 n = 0; n < CurvePoints.Num(); ++n) 
	{
		NewSplineComponent->AddSplinePoint(CurvePoints[n], ESplineCoordinateSpace::Local);
		NewSplineComponent->SetSplinePointType(n, SplinePointType);
	}

	bool bHasScales = CurveScales.Num() == CurvePoints.Num();
	if (bHasScales)
	{
		for (int32 n = 0; n < CurvePoints.Num(); ++n)
		{
			NewSplineComponent->SetScaleAtSplinePoint(n, CurveScales[n], false);
		}
	}

	bool bHasRotations = CurveRotations.Num() == CurvePoints.Num();
	if (bHasRotations)
	{
		for (int32 n = 0; n < CurvePoints.Num(); ++n)
		{
			NewSplineComponent->SetRotationAtSplinePoint(n, FRotator::MakeFromEuler(CurveRotations[n]), ESplineCoordinateSpace::Local, false);
		}
	}

	NewSplineComponent->SetClosedLoop(bIsClosed);

	NewSplineComponent->UpdateSpline();

	NewSplineComponent->AttachToComponent(OuterSceneComponent, FAttachmentTransformRules::KeepRelativeTransform);
	NewSplineComponent->RegisterComponent();
	AActor *OwnerActor = Cast<AActor>(Outer);
	if (IsValid(OwnerActor))
		OwnerActor->AddInstanceComponent(NewSplineComponent);

	ReselectSelectedActors();

	return NewSplineComponent;
}

bool 
FHoudiniSplineTranslator::UpdateOutputUnrealSplineComponent(
	USplineComponent* EditedSplineComponent,
	const TArray<FVector>& CurvePoints,
	const TArray<FVector>& CurveRotations,
	const TArray<FVector>& CurveScales,
	const EHoudiniCurveType& CurveType,
	const bool& bClosed)
{
	if (!IsValid(EditedSplineComponent))
		return false;

	if (CurvePoints.Num() < 2)
		return false;

	EditedSplineComponent->ClearSplinePoints();

	for (int32 Idx = 0; Idx < CurvePoints.Num(); ++Idx) 
	{
		EditedSplineComponent->AddSplinePoint(CurvePoints[Idx], ESplineCoordinateSpace::Local, false);

		if (CurveType == EHoudiniCurveType::Polygon)
			EditedSplineComponent->SetSplinePointType(Idx, ESplinePointType::Linear, false);
		else
			EditedSplineComponent->SetSplinePointType(Idx, ESplinePointType::Curve, false);
	}

	bool bHasScales = CurveScales.Num() == CurvePoints.Num();
	if (bHasScales)
	{
		for (int32 n = 0; n < CurvePoints.Num(); ++n)
		{
			EditedSplineComponent->SetScaleAtSplinePoint(n, CurveScales[n], false);
		}
	}

	bool bHasRotations = CurveRotations.Num() == CurvePoints.Num();
	if (bHasRotations)
	{
		for (int32 n = 0; n < CurvePoints.Num(); ++n)
		{			
			EditedSplineComponent->SetRotationAtSplinePoint(n, FRotator::MakeFromEuler(CurveRotations[n]), ESplineCoordinateSpace::Local, false);
		}
	}

	EditedSplineComponent->SetClosedLoop(bClosed, true);

	return true;
}

bool
FHoudiniSplineTranslator::UpdateOutputHoudiniSplineComponent(const TArray<FVector>& CurvePoints, UHoudiniSplineComponent* EditedHoudiniSplineComponent)
{
	if (!IsValid(EditedHoudiniSplineComponent))
		return false;

	if (CurvePoints.Num() < 2)
		return false;

	int MinCount = FMath::Min(CurvePoints.Num(), EditedHoudiniSplineComponent->CurvePoints.Num());
	
	int Idx = 0;
	// modify existing points
	for (; Idx < MinCount; ++Idx)
	{
		FTransform CurTrans = EditedHoudiniSplineComponent->CurvePoints[Idx];
		if (CurTrans.GetLocation() == CurvePoints[Idx])
			continue;

		CurTrans.SetLocation(CurvePoints[Idx]);
	}

	// remove extra points
	if (Idx < EditedHoudiniSplineComponent->CurvePoints.Num()-1) 
	{
		for (int32 n = EditedHoudiniSplineComponent->CurvePoints.Num() - 1; n >= Idx; --n) 
		{
			EditedHoudiniSplineComponent->RemovePointAtIndex(n);
		}
	}


	// append extra points
	for (; Idx < CurvePoints.Num(); ++Idx)
	{
		FTransform NewPoint = FTransform::Identity;
		NewPoint.SetLocation(CurvePoints[Idx]);
		EditedHoudiniSplineComponent->CurvePoints.Add(NewPoint);
	}

	return true;
}


bool 
FHoudiniSplineTranslator::CreateOutputSplinesFromHoudiniGeoPartObject(
	const FHoudiniGeoPartObject& InHGPO, 
	UObject* InOuterComponent,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InSplines, 
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutSplines,
	const bool& InForceRebuild,
	const EHoudiniCurveOutputType& OutputCurveType,
	const bool& bIsLinear,
	const bool& bIsClosed) 
{
	if (!IsValid(InOuterComponent))
		return false;

	int32 CurveNodeId = InHGPO.GeoId;
	int32 CurvePartId = InHGPO.PartId;
	if (CurveNodeId < 0 || CurvePartId < 0)
		return false;	

	// Extract all curve points from this HGPO

	TArray<float> RefinedCurvePositions;
	FHoudiniHapiAccessor Accessor;
	Accessor.Init(CurveNodeId, CurvePartId, HAPI_UNREAL_ATTRIB_POSITION);
	Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, RefinedCurvePositions);

	TArray<float> RefinedCurveRotations;
	HAPI_AttributeInfo AttributeRefinedCurveRotations;
	Accessor.Init(CurveNodeId, CurvePartId, HAPI_UNREAL_ATTRIB_ROTATION);
	Accessor.GetInfo(AttributeRefinedCurveRotations, HAPI_ATTROWNER_INVALID);
	Accessor.GetAttributeData(AttributeRefinedCurveRotations, RefinedCurveRotations);

	TArray<float> RefinedCurveScales;
	Accessor.Init(CurveNodeId, CurvePartId, HAPI_UNREAL_ATTRIB_SCALE);
	Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, RefinedCurveScales);

	HAPI_CurveInfo CurveInfo;
	FHoudiniApi::CurveInfo_Init(&CurveInfo);
	FHoudiniApi::GetCurveInfo(FHoudiniEngine::Get().GetSession(), CurveNodeId, CurvePartId, &CurveInfo);

	int32 NumOfCurves = CurveInfo.curveCount;
	TArray<int32> CurvePointsCounts;
	CurvePointsCounts.SetNumZeroed(NumOfCurves);
	FHoudiniApi::GetCurveCounts(FHoudiniEngine::Get().GetSession(), CurveNodeId, CurvePartId, CurvePointsCounts.GetData(), 0, NumOfCurves);

	TArray<TArray<FVector>> CurvesDisplayPoints;
	FHoudiniSplineTranslator::ConvertPositionToVectorData(RefinedCurvePositions, CurvesDisplayPoints, CurvePointsCounts);

	TArray<TArray<FVector>> CurvesRotations;
	if (AttributeRefinedCurveRotations.exists && AttributeRefinedCurveRotations.tupleSize == 4)
	{
		FHoudiniSplineTranslator::ConvertQuaternionRotationToVectorData(RefinedCurveRotations, CurvesRotations, CurvePointsCounts);
	}
	else
	{
		FHoudiniSplineTranslator::ConvertEulerRotationToVectorData(RefinedCurveRotations, CurvesRotations, CurvePointsCounts);
	}

	TArray<TArray<FVector>> CurvesScales;
	FHoudiniSplineTranslator::ConvertScaleToVectorData(RefinedCurveScales, CurvesScales, CurvePointsCounts);

	// Extract all curve points from this HGPO
	FString GeoName = InHGPO.PartName;
	int32 CurveIdx = 1;

	// Iterate through all curves found in this HGPO
	for (int32 n = 0; n < CurvesDisplayPoints.Num(); ++n) 
	{
		FString CurveName = FString::Printf(TEXT("%s curve %d"), *GeoName, CurveIdx);
		CurveIdx += 1;

		if (CurvePointsCounts[n] < 2)
		{
			// Invalid vertex count, skip this curve.
			HOUDINI_LOG_WARNING(
				TEXT("Creating Unreal Spline: Object [%d %s], Geo [%d], Part [%d %s], Curve# [%d] invalid vertex count.")
				TEXT("- skipping."),
				InHGPO.ObjectId, *InHGPO.ObjectName, InHGPO.GeoId, InHGPO.PartId, *InHGPO.PartName, CurveIdx);
			continue;
		}

		FHoudiniOutputObjectIdentifier CurveIdentifier(InHGPO.ObjectId, InHGPO.GeoId, InHGPO.PartId, CurveName);		
		FHoudiniOutputObject* FoundOutputObject = InSplines.Find(CurveIdentifier);

		bool bNeedToRebuildSpline = false;
		if (!FoundOutputObject)
			bNeedToRebuildSpline = true;

		USceneComponent* FoundComponent = nullptr;
		if (FoundOutputObject && FoundOutputObject->OutputComponents.Num() > 0)
			FoundComponent = Cast<USceneComponent>(FoundOutputObject->OutputComponents[0]);

	    if (IsValid(FoundComponent))
		{
			// Only support output to Unreal Spline for now...
			//if (FoundComponent->IsA<USplineComponent>() && FoundOutputObject->CurveOutputProperty.CurveOutputType != EHoudiniCurveOutputType::UnrealSpline)
			//	bNeedToRebuildSpline = true;

			//if (FoundComponent->IsA<UHoudiniSplineComponent>() && FoundOutputObject->CurveOutputProperty.CurveOutputType != EHoudiniCurveOutputType::HoudiniSpline)
			//	bNeedToRebuildSpline = true;

			if (InHGPO.bHasGeoChanged || InHGPO.PartInfo.bHasChanged || InForceRebuild)
				bNeedToRebuildSpline = true;
		}
		else
		{
			bNeedToRebuildSpline = true;
		}

		// The curve has not changed, no need to go through the rest
		if (!bNeedToRebuildSpline)
		{
			OutSplines.Add(CurveIdentifier, *FoundOutputObject);
			continue;
		}

		bool bReusedPreviousOutput = false;
		if (!FoundOutputObject) 
		{
			HOUDINI_LOG_WARNING(
				TEXT("Creating Unreal Spline (default): Object [%d %s], Geo [%d], Part [%d %s], Curve# [%d], number of points [%d]."),
				InHGPO.ObjectId, *InHGPO.ObjectName, InHGPO.GeoId, InHGPO.PartId, *InHGPO.PartName, CurveIdx, CurvePointsCounts[n]);
			
			// If not found (at initialize), create an Unreal spline  
			// We only support unreal spline for now..
			// May support Houdini spline too later
			USplineComponent* CreatedSplineComponent = FHoudiniSplineTranslator::CreateOutputUnrealSplineComponent(
				InOuterComponent, CurvesDisplayPoints[n], CurvesRotations[n], CurvesScales[n], bIsLinear, bIsClosed);

			if (!CreatedSplineComponent)
				continue;

			// Create a new output object
			FHoudiniOutputObject NewOutputObject;
			check(NewOutputObject.OutputComponents.Num() < 2); // Multiple components not supported yet.
			NewOutputObject.OutputComponents.Empty();
			NewOutputObject.OutputComponents.Add(CreatedSplineComponent);

			NewOutputObject.CurveOutputProperty.CurveOutputType = OutputCurveType;
			NewOutputObject.CurveOutputProperty.NumPoints = CurvePointsCounts[n];

			// TODO: Need a way to access info of the output curve
			NewOutputObject.CurveOutputProperty.CurveMethod = EHoudiniCurveMethod::Breakpoints;
			NewOutputObject.CurveOutputProperty.CurveType = bIsLinear ? EHoudiniCurveType::Polygon : EHoudiniCurveType::Bezier;
			NewOutputObject.CurveOutputProperty.bClosed = false;
			// Fill in the rest of output curve properties

			OutSplines.Add(CurveIdentifier, NewOutputObject);

			// Update FOundOutputObject so we can cache attributes after
			FoundOutputObject = OutSplines.Find(CurveIdentifier);
		}
		else 
		{
			// If this is not a new output object we have to clear the CachedAttributes and CachedTokens before
			// setting the new values (so that we do not re-use any values from the previous cook)
			FoundOutputObject->CachedAttributes.Empty();
			FoundOutputObject->CachedTokens.Empty();

			// 
			if (FoundOutputObject->CurveOutputProperty.CurveOutputType == EHoudiniCurveOutputType::UnrealSpline)
			{
				// See if we can simply update the previous Spline Component
				bool bCanUpdateUnrealSpline = (!FoundOutputObject->OutputComponents.IsEmpty() &&  FoundOutputObject->OutputComponents[0]->IsA<USplineComponent>());
				if (bCanUpdateUnrealSpline)
				{
					// Update the existing unreal spline component
					bReusedPreviousOutput = true;
					HOUDINI_LOG_WARNING(
						TEXT("Updating Unreal Spline: Object [%d %s], Geo [%d], Part [%d %s], Curve# [%d], number of points [%d]."),
						InHGPO.ObjectId, *InHGPO.ObjectName, InHGPO.GeoId, InHGPO.PartId, *InHGPO.PartName, CurveIdx, CurvePointsCounts[n]);

					USplineComponent* FoundUnrealSpline = FoundOutputObject->OutputComponents.Num() > 0 ?
					        Cast<USplineComponent>(FoundOutputObject->OutputComponents[0]) : nullptr;
					if (!FHoudiniSplineTranslator::UpdateOutputUnrealSplineComponent(
						FoundUnrealSpline, CurvesDisplayPoints[n], CurvesRotations[n], CurvesScales[n], 
						FoundOutputObject->CurveOutputProperty.CurveType, FoundOutputObject->CurveOutputProperty.bClosed))
						continue;
					
					FoundOutputObject = &OutSplines.Add(CurveIdentifier, *FoundOutputObject);
				}
				else
				{
					// Create a new Unreal spline component
					// We support unreal spline only for now...
					bReusedPreviousOutput = false;
					FoundOutputObject->CurveOutputProperty.CurveOutputType = EHoudiniCurveOutputType::UnrealSpline;					
					HOUDINI_LOG_WARNING(
						TEXT("Creating Unreal Spline: Object [%d %s], Geo [%d], Part [%d %s], Curve# [%d], number of points [%d]."),
						InHGPO.ObjectId, *InHGPO.ObjectName, InHGPO.GeoId, InHGPO.PartId, *InHGPO.PartName, CurveIdx, CurvePointsCounts[n]);

					USplineComponent* NewUnrealSpline = FHoudiniSplineTranslator::CreateOutputUnrealSplineComponent(
						InOuterComponent, CurvesDisplayPoints[n], CurvesRotations[n], CurvesScales[n], bIsLinear, bIsClosed);

					if (!NewUnrealSpline)
						continue;

					check(FoundOutputObject->OutputComponents.Num() < 2); // Multiple components not supported yet.
					FoundOutputObject->OutputComponents.Empty();
					FoundOutputObject->OutputComponents.Add(NewUnrealSpline);

					FoundOutputObject = &OutSplines.Add(CurveIdentifier, *FoundOutputObject);
				}
			}
			// We current support Unreal Spline output only...
			/*
			else
			{
				// We want to output a Houdini Spline Component
				// See if we can simply update the previous Houdini Spline Component
				bool bCanUpdateHoudiniSpline = (FoundOutputObject->OutputComponent &&  FoundOutputObject->OutputComponent->IsA<UHoudiniSplineComponent>());
				if (bCanUpdateHoudiniSpline)
				{
					// Update the existing houdini spline component
					bReusedPreviousOutput = true;
					HOUDINI_LOG_WARNING(
						TEXT("Changing Houdini Spline: Object [%d %s], Geo [%d], Part [%d %s], Curve# [%d], number of points [%d]."),
						InHGPO.ObjectId, *InHGPO.ObjectName, InHGPO.GeoId, InHGPO.PartId, *InHGPO.PartName, CurveIdx, CurvePointsCounts[n]);

					UHoudiniSplineComponent* FoundHoudiniSpline = Cast<UHoudiniSplineComponent>(FoundOutputObject->OutputComponent);
					if (!FHoudiniSplineTranslator::UpdateOutputHoudiniSplineComponent(CurvesDisplayPoints[n], FoundHoudiniSpline))
						continue;

					OutSplines.Add(CurveIdentifier, *FoundOutputObject);
				}
				else
				{
					// Create a new Houdini spline component
					bReusedPreviousOutput = false;
					FoundOutputObject->CurveOutputProperty.CurveOutputType = EHoudiniCurveOutputType::HoudiniSpline;
					HOUDINI_LOG_WARNING(
						TEXT("Creating Unreal Spline: Object [%d %s], Geo [%d], Part [%d %s], Curve# [%d], number of points [%d]."),
						InHGPO.ObjectId, *InHGPO.ObjectName, InHGPO.GeoId, InHGPO.PartId, *InHGPO.PartName, CurveIdx, CurvePointsCounts[n]);

					UHoudiniSplineComponent* NewHoudiniSpline = CreateOutputHoudiniSplineComponent(CurvesDisplayPoints[n], CurvesRotations[n], CurvesScales[n], InOuter);
					if (!NewHoudiniSpline)
						continue;

					FoundOutputObject->OutputComponent = NewHoudiniSpline;

					OutSplines.Add(CurveIdentifier, *FoundOutputObject);
				}
			}
			*/
		}

		// Cache commonly supported Houdini attributes on the OutputAttributes
		TArray<FString> LevelPaths;
		if (FoundOutputObject && FHoudiniEngineUtils::GetLevelPathAttribute(
			InHGPO.GeoId, InHGPO.PartId, LevelPaths, HAPI_ATTROWNER_INVALID, 0, 1))
		{
			if (LevelPaths.Num() > 0 && !LevelPaths[0].IsEmpty())
			{
				// cache the level path attribute on the output object
				FoundOutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_LEVEL_PATH, LevelPaths[0]);
			}
		}

		TArray<FString> OutputNames;
		if (FoundOutputObject && FHoudiniEngineUtils::GetOutputNameAttribute(
			InHGPO.GeoId, InHGPO.PartId, OutputNames, 0, 1))
		{
			if (OutputNames.Num() > 0 && !OutputNames[0].IsEmpty())
			{
				// cache the output name attribute on the output object
				FoundOutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2, OutputNames[0]);
			}
		}

		TArray<FString> BakeNames;
		if (FoundOutputObject && FHoudiniEngineUtils::GetBakeNameAttribute(
			InHGPO.GeoId, InHGPO.PartId, BakeNames, HAPI_ATTROWNER_INVALID, 0, 1))
		{
			if (BakeNames.Num() > 0 && !BakeNames[0].IsEmpty())
			{
				// cache the output name attribute on the output object
				FoundOutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_NAME, BakeNames[0]);
			}
		}

		TArray<FString> BakeOutputActorNames;
		if (FoundOutputObject && FHoudiniEngineUtils::GetBakeActorAttribute(
			InHGPO.GeoId, InHGPO.PartId, BakeOutputActorNames, HAPI_ATTROWNER_INVALID, 0, 1))
		{
			if (BakeOutputActorNames.Num() > 0 && !BakeOutputActorNames[0].IsEmpty())
			{
				// cache the bake actor attribute on the output object
				FoundOutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_ACTOR, BakeOutputActorNames[0]);
			}
		}

		TArray<FString> BakeOutputActorClassNames;
		if (FoundOutputObject && FHoudiniEngineUtils::GetBakeActorClassAttribute(
			InHGPO.GeoId, InHGPO.PartId, BakeOutputActorClassNames, HAPI_ATTROWNER_INVALID, 0, 1))
		{
			if (BakeOutputActorClassNames.Num() > 0 && !BakeOutputActorClassNames[0].IsEmpty())
			{
				// cache the bake actor attribute on the output object
				FoundOutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_ACTOR_CLASS, BakeOutputActorClassNames[0]);
			}
		}

		TArray<FString> BakeFolders;
		if (FoundOutputObject && FHoudiniEngineUtils::GetBakeFolderAttribute(
			InHGPO.GeoId, BakeFolders, InHGPO.PartId, 0, 1))
		{
			if (BakeFolders.Num() > 0 && !BakeFolders[0].IsEmpty())
			{
				// cache the unreal_bake_folder attribute on the output object
				FoundOutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_FOLDER, BakeFolders[0]);
			}
		}

		TArray<FString> BakeOutlinerFolders;
		if (FoundOutputObject && FHoudiniEngineUtils::GetBakeOutlinerFolderAttribute(
			InHGPO.GeoId, InHGPO.PartId, BakeOutlinerFolders, HAPI_ATTROWNER_INVALID, 0, 1))
		{
			if (BakeOutlinerFolders.Num() > 0 && !BakeOutlinerFolders[0].IsEmpty())
			{
				// cache the bake actor attribute on the output object
				FoundOutputObject->CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_OUTLINER_FOLDER, BakeOutlinerFolders[0]);
			}
		}

		// Update generic properties attributes on the spline component
		TArray<FHoudiniGenericAttribute> GenericAttributes;
		if (FoundOutputObject && FHoudiniEngineUtils::GetGenericPropertiesAttributes(
			InHGPO.GeoId, InHGPO.PartId, true, 0, 0, 0, GenericAttributes))
		{
			for(auto Component : FoundOutputObject->OutputComponents)
			{
				FHoudiniEngineUtils::KeepOrClearComponentTags(Cast<UActorComponent>(Component), &InHGPO);
				FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(Component, GenericAttributes);
			}
		}
		
		if (bReusedPreviousOutput)
		{
			// Remove the reused output unreal spline from the old map to avoid its deletion
			InSplines.Remove(CurveIdentifier);
		}

		HOUDINI_LOG_WARNING(
			TEXT("Finished Generating Unreal Spline: Object [%d %s], Geo [%d], Part [%d %s], Curve# [%d], number of points [%d]."),
			InHGPO.ObjectId, *InHGPO.ObjectName, InHGPO.GeoId, InHGPO.PartId, *InHGPO.PartName, CurveIdx, CurvePointsCounts[n]);
	}

	return true;
}

bool 
FHoudiniSplineTranslator::CreateAllSplinesFromHoudiniOutput(UHoudiniOutput* InOutput, UObject* InOuterComponent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniSplineTranslator::CreateAllSplinesFromHoudiniOutput);
	if (!IsValid(InOutput))
		return false;

	if (!IsValid(InOuterComponent))
		return false;

	// ONLY DO THIS ON CURVES!!!!
	if (InOutput->GetType() != EHoudiniOutputType::Curve)
		return false;

	// UHoudiniAssetComponent* OuterHAC = Cast<UHoudiniAssetComponent>(InOuterComponent);
	//
	// if (!IsValid(OuterHAC))
	// 	return false;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> NewOutputObjects;
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OldOutputObjects = InOutput->GetOutputObjects();

	// Iterate on all the output's HGPO
	for (const FHoudiniGeoPartObject & CurHGPO : InOutput->GetHoudiniGeoPartObjects())
	{
		// not a curve, skip
		if (CurHGPO.Type != EHoudiniPartType::Curve)
			continue;

		// Check if we want to create a houdini output curve from corresponding attribute
		TArray<int> IntData;
		IntData.Empty();
		FHoudiniHapiAccessor Accessor(CurHGPO.GeoId, CurHGPO.PartId, HAPI_UNREAL_ATTRIB_OUTPUT_UNREAL_CURVE);
		bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, 1, IntData, 0, 1);
		if (!bSuccess || IntData.Num() <= 0 || IntData[0] == 0)
			continue;

		IntData.Empty();
		bool bIsLinear = false;
		Accessor.Init(CurHGPO.GeoId, CurHGPO.PartId, HAPI_UNREAL_ATTRIB_OUTPUT_UNREAL_CURVE_LINEAR);
		if (Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, 1, IntData, 0, 1))
		{
			if (IntData.Num() > 0)
				bIsLinear = IntData[0] != 0;
		}

		IntData.Empty();
		Accessor.Init(CurHGPO.GeoId, CurHGPO.PartId, HAPI_UNREAL_ATTRIB_OUTPUT_UNREAL_CURVE_CLOSED);
		bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, 1, IntData, 0, 1 );
		bool bIsClosed = false;
		if (IntData.Num() > 0)
			bIsClosed = IntData[0] != 0;

		// We output curve to Unreal Spline only for now
		// May support output to Houdini Spline later
		CreateOutputSplinesFromHoudiniGeoPartObject(CurHGPO, InOuterComponent, OldOutputObjects, NewOutputObjects, false, EHoudiniCurveOutputType::UnrealSpline, bIsLinear, bIsClosed);
	}

	// TODO: FIX ME!!! This literally nukes all the output objects, even if they are not curves!

	// The old map now only contains unused/stale output curves destroy them
	for (auto& OldPair : OldOutputObjects)
	{
		for(auto Component : OldPair.Value.OutputComponents)
		{
		    USceneComponent* OldSplineSceneComponent = Cast<USceneComponent>(Component);
		    
		    if (!IsValid(OldSplineSceneComponent))
			    continue;

		    // The output object is supposed to be a spline
		    if (!OldSplineSceneComponent->IsA<USplineComponent>() && !OldSplineSceneComponent->IsA<UHoudiniSplineComponent>())
			    continue;

		    OldSplineSceneComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		    OldSplineSceneComponent->UnregisterComponent();
		    OldSplineSceneComponent->DestroyComponent();
        }
		OldPair.Value.OutputComponents.Empty();
	}
	OldOutputObjects.Empty();

	InOutput->SetOutputObjects(NewOutputObjects);

	// TODO: remove and delay editor propreties update
	FHoudiniEngineUtils::UpdateEditorProperties(true);

	return true;
}

void
FHoudiniSplineTranslator::ReselectSelectedActors()
{
	// TODO: Duplicate with FHoudiniEngineUtils::UpdateEditorProperties ??
	USelection* Selection = GEditor->GetSelectedActors();
	TArray<AActor*> SelectedActors;
	SelectedActors.SetNumUninitialized(GEditor->GetSelectedActorCount());
	Selection->GetSelectedObjects(SelectedActors);

	GEditor->SelectNone(false, false, false);

	for (AActor* NextSelected : SelectedActors)
	{
		GEditor->SelectActor(NextSelected, true, true, true, true);
	}
}
