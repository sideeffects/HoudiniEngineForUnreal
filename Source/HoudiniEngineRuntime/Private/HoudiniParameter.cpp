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

#include "HoudiniParameter.h"

UHoudiniParameter::UHoudiniParameter(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
	, ParmType(EHoudiniParameterType::Invalid)
	, ChoiceListType(EHoudiniParameterChoiceListType::None)
	, TupleSize(0)
	, NodeId(-1)
	, ParmId(-1)
	, ParentParmId(-1)
	, ChildIndex(-1)
	, bIsVisible(true)
	, bIsParentFolderVisible(true)
	, bIsDisabled(false)
	, bHasChanged(false)
	, bNeedsToTriggerUpdate(true)
	, bIsDefault(false)
	, bIsSpare(false)
	, bJoinNext(false)
	, bIsLabelVisible(true)
	, bIsChildOfMultiParm(false)
	, bIsDirectChildOfMultiParm(false)
	, bPendingRevertToDefault(false)
	, TagCount(0)
	, ValueIndex(-1)
	, bHasExpression(false)
	, bShowExpression(false)
{
	Name = TEXT("");
	Label = TEXT("");
	Help = TEXT("");

}

UHoudiniParameter *
UHoudiniParameter::Create( UObject* InOuter, const FString& InParamName)
{
	FString ParamNameStr = "HoudiniParameter_" + InParamName;
	FName ParamName = MakeUniqueObjectName(InOuter, UHoudiniParameter::StaticClass(), *ParamNameStr);

	// We need to create a new parameter
	UHoudiniParameter * HoudiniAssetParameter = NewObject< UHoudiniParameter >(
		InOuter, UHoudiniParameter::StaticClass(), ParamName, RF_Public | RF_Transactional);

	return HoudiniAssetParameter;
}

bool
UHoudiniParameter::IsChildParameter() const
{
	return ParentParmId >= 0;
}

void
UHoudiniParameter::RevertToDefault()
{
	bPendingRevertToDefault = true;
	TuplePendingRevertToDefault.Empty();
	TuplePendingRevertToDefault.Add(-1);

	MarkChanged(true);
}

void
UHoudiniParameter::RevertToDefault(const int32& InAtTupleIndex)
{
	bPendingRevertToDefault = true;
	TuplePendingRevertToDefault.AddUnique(InAtTupleIndex);

	MarkChanged(true);	
}

void
UHoudiniParameter::MarkDefault(const bool& bInDefault)
{
	bIsDefault = bInDefault;

	if (bInDefault)
	{
		// No need to revert default parameter
		bPendingRevertToDefault = false;
		TuplePendingRevertToDefault.Empty();
	}
}

EHoudiniRampInterpolationType UHoudiniParameter::GetHoudiniInterpMethodFromInt(int32 InInt)
{
	EHoudiniRampInterpolationType Result = EHoudiniRampInterpolationType::InValid;
	switch (InInt)
	{
	case 0:
		Result = EHoudiniRampInterpolationType::CONSTANT;
		break;
	case 1:
		Result = EHoudiniRampInterpolationType::LINEAR;
		break;
	case 2:
		Result = EHoudiniRampInterpolationType::CATMULL_ROM;
		break;
	case 3:
		Result = EHoudiniRampInterpolationType::MONOTONE_CUBIC;
		break;
	case 4:
		Result = EHoudiniRampInterpolationType::BEZIER;
		break;
	case 5:
		Result = EHoudiniRampInterpolationType::BSPLINE;
		break;
	case 6:
		Result = EHoudiniRampInterpolationType::HERMITE;
		break;
	}

	return Result;
}

FString
UHoudiniParameter::GetStringFromHoudiniInterpMethod(EHoudiniRampInterpolationType InType)
{
	FString Result = FString("InValid");
	switch (InType)
	{
	case EHoudiniRampInterpolationType::CONSTANT:
		Result = FString("Constant");
		break;
	case EHoudiniRampInterpolationType::LINEAR:
		Result = FString("Linear");
		break;
	case EHoudiniRampInterpolationType::BEZIER:
		Result = FString("Bezier");
		break;
	case EHoudiniRampInterpolationType::BSPLINE:
		Result = FString("B-Spline");
		break;
	case EHoudiniRampInterpolationType::MONOTONE_CUBIC:
		Result = FString("Monotone Cubic");
		break;
	case EHoudiniRampInterpolationType::CATMULL_ROM:
		Result = FString("Catmull Rom");
		break;
	case EHoudiniRampInterpolationType::HERMITE:
		Result = FString("Hermite");
		break;
	}

	return Result;
}
EHoudiniRampInterpolationType 
UHoudiniParameter::GetHoudiniInterpMethodFromString(const FString& InString) 
{
	if (InString.StartsWith(TEXT("Constant")))
		return EHoudiniRampInterpolationType::CONSTANT;

	else if (InString.StartsWith("Linear"))
		return EHoudiniRampInterpolationType::LINEAR;

	else if (InString.StartsWith("Bezier"))
		return EHoudiniRampInterpolationType::BEZIER;

	else if (InString.StartsWith("B-Spline"))
		return EHoudiniRampInterpolationType::BSPLINE;

	else if (InString.StartsWith("Monotone Cubic"))
		return EHoudiniRampInterpolationType::MONOTONE_CUBIC;

	else if (InString.StartsWith("Catmull Rom"))
		return EHoudiniRampInterpolationType::CATMULL_ROM;

	else if (InString.StartsWith("Hermite"))
		return EHoudiniRampInterpolationType::HERMITE;

	return EHoudiniRampInterpolationType::InValid;
}



ERichCurveInterpMode 
UHoudiniParameter::EHoudiniRampInterpolationTypeToERichCurveInterpMode(EHoudiniRampInterpolationType InType)
{
	switch (InType)
	{
	case EHoudiniRampInterpolationType::CONSTANT:
		return ERichCurveInterpMode::RCIM_Constant;

	case EHoudiniRampInterpolationType::LINEAR:
		return ERichCurveInterpMode::RCIM_Linear;

	default:
		break;
	}

	return ERichCurveInterpMode::RCIM_Cubic;
}

UHoudiniParameter*
UHoudiniParameter::DuplicateAndCopyState(UObject* DestOuter, EObjectFlags ClearFlags, EObjectFlags SetFlags)
{
	UHoudiniParameter* NewParameter = Cast<UHoudiniParameter>(StaticDuplicateObject(this, DestOuter));
	
	NewParameter->CopyStateFrom(this, false);

	return NewParameter;
}

void
UHoudiniParameter::CopyStateFrom(UHoudiniParameter * InParameter, bool bCopyAllProperties, EObjectFlags InClearFlags, EObjectFlags InSetFlags)
{
	if (bCopyAllProperties)
	{
		UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
		Params.bDoDelta = false; // Perform a deep copy
		Params.bClearReferences = false; // References will be replaced afterwards.
		UEngine::CopyPropertiesForUnrelatedObjects(InParameter, this, Params);
	}

	if (InSetFlags != RF_NoFlags)
		SetFlags(InSetFlags);
	if (InClearFlags != RF_NoFlags)
		ClearFlags( InClearFlags );

	NodeId = InParameter->NodeId;
	ParmId = InParameter->ParmId;
	ParentParmId = InParameter->ParentParmId;
	bIsDefault = InParameter->bIsDefault;
	bPendingRevertToDefault = InParameter->bPendingRevertToDefault;
	TuplePendingRevertToDefault = InParameter->TuplePendingRevertToDefault;
	bShowExpression = InParameter->bShowExpression;
}

void
UHoudiniParameter::InvalidateData()
{

}

void
UHoudiniParameter::OnSessionConnected()
{
	NodeId = INDEX_NONE;

	// Dont invalidate existing parmId as this will prevent the Parm UI
	// From properly showing up.
	//ParmId = INDEX_NONE;
	//ParentParmId = INDEX_NONE;
}


