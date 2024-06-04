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

#include "HoudiniPublicAPIInputTypes.h"

#include "HoudiniPublicAPIAssetWrapper.h"

#include "HoudiniInput.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"

UHoudiniPublicAPIInput::UHoudiniPublicAPIInput()
{
	bKeepWorldTransform = false;
	bImportAsReference = false;
	bImportAsReferenceRotScaleEnabled = true;
	bImportAsReferenceBboxEnabled = true;
	bImportAsReferenceMaterialEnabled = true;
	bExportMaterialParameters = false;
}

bool
UHoudiniPublicAPIInput::IsAcceptableObjectForInput_Implementation(UObject* InObject) const
{
	return UHoudiniInput::IsObjectAcceptable(GetInputType(), InObject); 
}

bool
UHoudiniPublicAPIInput::SetInputObjects_Implementation(const TArray<UObject*>& InObjects)
{
	bool bHasFailures = false;
	InputObjects.Empty(InObjects.Num());
	for (UObject* const Object : InObjects)
	{
		if (!IsValid(Object))
		{
			SetErrorMessage(FString::Printf(TEXT("An input object is null or invalid.")));
			bHasFailures = true;
			continue;
		}
		else if (!IsAcceptableObjectForInput(Object))
		{
			SetErrorMessage(FString::Printf(
				TEXT("Object '%s' is not of an acceptable type for inputs of class %s."),
				*(Object->GetName()), *(GetClass()->GetName())));
			bHasFailures = true;
			continue;
		}

		InputObjects.Add(Object);
	}

	return !bHasFailures;
}

bool
UHoudiniPublicAPIInput::GetInputObjects_Implementation(TArray<UObject*>& OutObjects)
{
	OutObjects = InputObjects;

	return true;
}

bool
UHoudiniPublicAPIInput::PopulateFromHoudiniInput(UHoudiniInput const* const InInput)
{
	const EHoudiniInputType InputType = GetInputType();
	if (!IsValid(InInput))
	{
		SetErrorMessage(TEXT("InInput is invalid."));
		return false;
	}

	if (InInput->GetInputType() != InputType)
	{
		SetErrorMessage(FString::Printf(
			TEXT("Incompatible input types %d vs %d"), InInput->GetInputType(), InputType));
		return false;
	}

	bKeepWorldTransform = InInput->GetKeepWorldTransform();
	bImportAsReference = InInput->GetImportAsReference();
	bImportAsReferenceRotScaleEnabled = InInput->GetImportAsReferenceRotScaleEnabled();
	bImportAsReferenceBboxEnabled = InInput->GetImportAsReferenceBboxEnabled();
	bImportAsReferenceMaterialEnabled = InInput->GetImportAsReferenceMaterialEnabled();
	bExportMaterialParameters = InInput->GetExportMaterialParameters();

	const TArray<UHoudiniInputObject*>* SrcInputObjectsPtr = InInput->GetHoudiniInputObjectArray(InputType);
	if (SrcInputObjectsPtr && SrcInputObjectsPtr->Num() > 0)
	{
		InputObjects.Empty(SrcInputObjectsPtr->Num()); 
		for (UHoudiniInputObject const* const SrcInputObject : *SrcInputObjectsPtr)
		{
			if (!IsValid(SrcInputObject))
				continue;

			UObject* NewInputObject = ConvertInternalInputObject(SrcInputObject->GetObject());

			if (NewInputObject && !IsValid(NewInputObject))
			{
				SetErrorMessage(FString::Printf(
					TEXT("One of the input objects is non-null but pending kill/invalid.")));
				return false;
			}
			
			InputObjects.Add(NewInputObject);

			CopyHoudiniInputObjectPropertiesToInputObject(SrcInputObject, InputObjects.Num() - 1);
		}
	}

	return true;
}

bool
UHoudiniPublicAPIInput::UpdateHoudiniInput(UHoudiniInput* const InInput) const
{
	if (!IsValid(InInput))
	{
		SetErrorMessage(TEXT("InInput is invalid."));
		return false;
	}

	bool bAnyChanges = false;

	// If the input type didn't change, but the new/incoming InputObjects array is now smaller than the current input
	// objects array on the input, delete the surplus objects
	const EHoudiniInputType InputType = GetInputType();
	const int32 NumInputObjects = InputObjects.Num();
	if (InputType == InInput->GetInputType())
	{
		const int32 OldNumInputObjects = InInput->GetNumberOfInputObjects();
		if (NumInputObjects < OldNumInputObjects)
		{
			for (int32 Index = OldNumInputObjects - 1; Index >= NumInputObjects; --Index)
			{
				InInput->DeleteInputObjectAt(Index);
			}
			bAnyChanges = true;
		}
	}
	else
	{
		// Set / change the input type
		bool bBlueprintStructureModified = false;
		InInput->SetInputType(InputType, bBlueprintStructureModified);
	}

	// Set any general settings
	if (InInput->GetKeepWorldTransform() != bKeepWorldTransform)
	{
		InInput->SetKeepWorldTransform(bKeepWorldTransform);
		bAnyChanges = true;
	}
	if (InInput->GetImportAsReference() != bImportAsReference)
	{
		InInput->SetImportAsReference(bImportAsReference);
		bAnyChanges = true;
	}
	if (InInput->GetImportAsReferenceRotScaleEnabled() != bImportAsReferenceRotScaleEnabled)
	{
		InInput->SetImportAsReferenceRotScaleEnabled(bImportAsReferenceRotScaleEnabled);
		bAnyChanges = true;
	}
	if (InInput->GetImportAsReferenceBboxEnabled() != bImportAsReferenceBboxEnabled)
	{
		InInput->SetImportAsReferenceBboxEnabled(bImportAsReferenceBboxEnabled);
		bAnyChanges = true;
	}
	if (InInput->GetImportAsReferenceMaterialEnabled() != bImportAsReferenceMaterialEnabled)
	{
		InInput->SetImportAsReferenceMaterialEnabled(bImportAsReferenceMaterialEnabled);
		bAnyChanges = true;
	}
	if (InInput->GetExportMaterialParameters() != bExportMaterialParameters)
	{
		InInput->SetExportMaterialParameters(bExportMaterialParameters);
		bAnyChanges = true;
	}

	// Copy / set the input objects on the Houdini Input
	InInput->SetInputObjectsNumber(InputType, NumInputObjects);
	for (int32 Index = 0; Index < NumInputObjects; ++Index)
	{
		UObject* const InputObject = InputObjects[Index];
		UObject const* CurrentInputObject = InInput->GetInputObjectAt(Index);

		if (!IsValid(InputObject))
		{
			// Delete existing input object, but leave its space in the array, we'll set that to nullptr
			if (CurrentInputObject)
			{
				const bool bRemoveIndexFromArray = false;
				InInput->DeleteInputObjectAt(Index, bRemoveIndexFromArray);
			}
			InInput->SetInputObjectAt(Index, nullptr);

			if (!bAnyChanges && CurrentInputObject)
				bAnyChanges = true;
		}
		else
		{
			UObject const* const NewInputObject = ConvertAPIInputObjectAndAssignToInput(InputObject, InInput, Index);
			UHoudiniInputObject *DstHoudiniInputObject = InInput->GetHoudiniInputObjectAt(Index);
			if (DstHoudiniInputObject)
				CopyInputObjectPropertiesToHoudiniInputObject(Index, DstHoudiniInputObject);

			if (!bAnyChanges && NewInputObject != CurrentInputObject)
				bAnyChanges = true;
		}
	}

	if (bAnyChanges)
	{
		InInput->MarkChanged(true);
	}

	return true;
}

bool
UHoudiniPublicAPIInput::CopyHoudiniInputObjectPropertiesToInputObject(UHoudiniInputObject const* const InHoudiniInputObject, const int32 InInputObjectIndex)
{
	if (!IsValid(InHoudiniInputObject) || !InputObjects.IsValidIndex(InInputObjectIndex))
		return false;

	return true;
}

bool
UHoudiniPublicAPIInput::CopyInputObjectPropertiesToHoudiniInputObject(const int32 InInputObjectIndex, UHoudiniInputObject* const InHoudiniInputObject) const
{
	if (!InputObjects.IsValidIndex(InInputObjectIndex) || !IsValid(InHoudiniInputObject))
		return false;

	if (InHoudiniInputObject->GetImportAsReference() != bImportAsReference)
	{
		// InHoudiniInputObject->SetImportAsReference(bImportAsReference);
		InHoudiniInputObject->MarkChanged(true);
	}
	
	return true;
}

UObject*
UHoudiniPublicAPIInput::ConvertAPIInputObjectAndAssignToInput(UObject* InAPIInputObject, UHoudiniInput* InHoudiniInput, const int32 InInputIndex) const
{
	if (!IsValid(InHoudiniInput))
		return nullptr;

	UObject const* const CurrentInputObject = InHoudiniInput->GetInputObjectAt(InInputIndex);
	
	UObject* const ObjectToSet = (IsValid(InAPIInputObject)) ? InAPIInputObject : nullptr;

	// Delete the existing input object if it is invalid or differs from ObjectToSet
	if (CurrentInputObject && (!IsValid(CurrentInputObject) || CurrentInputObject != ObjectToSet))
	{
		// Keep the space/index in the array, we're going to set the new input object at the same index
		const bool bRemoveIndexFromArray = false;
		InHoudiniInput->DeleteInputObjectAt(InInputIndex, bRemoveIndexFromArray);
		InHoudiniInput->MarkChanged(true);
	}

	InHoudiniInput->SetInputObjectAt(InInputIndex, ObjectToSet);

	return ObjectToSet;
}

UHoudiniPublicAPIGeoInput::UHoudiniPublicAPIGeoInput()
{
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	bKeepWorldTransform = false;
	bPackBeforeMerge = false;
	bExportLODs = false;
	bExportSockets = false;
	bExportColliders = false;
	bPreferNaniteFallbackMesh = HoudiniRuntimeSettings ? HoudiniRuntimeSettings->bPreferNaniteFallbackMesh : false;
}

bool
UHoudiniPublicAPIGeoInput::SetInputObjects_Implementation(const TArray<UObject*>& InObjects)
{
	const bool bSuccess = Super::SetInputObjects_Implementation(InObjects);

	// Keep the transforms at the valid indices, resize the array to match InputObjects length. Set identity transform
	// in new slots.
	const int32 NumInputObjects = InputObjects.Num();
	const int32 NumTransforms = InputObjectTransformOffsetArray.Num();
	if (NumTransforms > NumInputObjects)
	{
		InputObjectTransformOffsetArray.SetNum(NumInputObjects);
	}
	else if (NumTransforms < NumInputObjects)
	{
		InputObjectTransformOffsetArray.Reserve(NumInputObjects);
		for (int32 Index = NumTransforms; Index < NumInputObjects; ++Index)
		{
			InputObjectTransformOffsetArray.Emplace(FTransform::Identity);
		}
	}

	return bSuccess;
}

bool
UHoudiniPublicAPIGeoInput::PopulateFromHoudiniInput(UHoudiniInput const* const InInput)
{
	if (!Super::PopulateFromHoudiniInput(InInput))
		return false;

	bPackBeforeMerge = InInput->GetPackBeforeMerge();
	bExportLODs = InInput->GetExportLODs();
	bExportSockets = InInput->GetExportSockets();
	bExportColliders = InInput->GetExportColliders();

	return true;
}

bool
UHoudiniPublicAPIGeoInput::UpdateHoudiniInput(UHoudiniInput* const InInput) const
{
	if (!Super::UpdateHoudiniInput(InInput))
		return false;

	bool bAnyChanges = false;
	if (InInput->GetPackBeforeMerge() != bPackBeforeMerge)
	{
		InInput->SetPackBeforeMerge(bPackBeforeMerge);
		bAnyChanges = true;
	}
	if (InInput->GetExportLODs() != bExportLODs)
	{
		InInput->SetExportLODs(bExportLODs);
		bAnyChanges = true;
	}
	if (InInput->GetExportSockets() != bExportSockets)
	{
		InInput->SetExportSockets(bExportSockets);
		bAnyChanges = true;
	}
	if (InInput->GetPreferNaniteFallbackMesh() != bPreferNaniteFallbackMesh)
	{
		InInput->SetPreferNaniteFallbackMesh(bPreferNaniteFallbackMesh);
		bAnyChanges = true;
	}
	if (InInput->GetExportColliders() != bExportColliders)
	{
		InInput->SetExportColliders(bExportColliders);
		bAnyChanges = true;
	}

	if (bAnyChanges)
	{
		InInput->MarkChanged(true);
	}

	return true;
}

void
UHoudiniPublicAPIGeoInput::PostLoad()
{
	Super::PostLoad();
}

bool
UHoudiniPublicAPIGeoInput::CopyHoudiniInputObjectPropertiesToInputObject(UHoudiniInputObject const* const InHoudiniInputObject, const int32 InInputObjectIndex)
{
	if (!Super::CopyHoudiniInputObjectPropertiesToInputObject(InHoudiniInputObject, InInputObjectIndex))
		return false;
	
	if (!IsValid(InHoudiniInputObject) || !InputObjects.IsValidIndex(InInputObjectIndex))
		return false;

	// Copy the transform offset
	if (SupportsTransformOffset())
	{
		SetInputObjectTransformOffset(InInputObjectIndex, InHoudiniInputObject->GetTransform());
	}

	return true;
}

bool
UHoudiniPublicAPIGeoInput::CopyInputObjectPropertiesToHoudiniInputObject(const int32 InInputObjectIndex, UHoudiniInputObject* const InHoudiniInputObject) const
{
	if (!Super::CopyInputObjectPropertiesToHoudiniInputObject(InInputObjectIndex, InHoudiniInputObject))
		return false;

	if (!InputObjects.IsValidIndex(InInputObjectIndex) || !IsValid(InHoudiniInputObject))
		return false;

	// Copy the transform offset
	if (SupportsTransformOffset())
	{
		FTransform Transform;
		if (!GetInputObjectTransformOffset(InInputObjectIndex, Transform))
			Transform = FTransform::Identity;

		if (!InHoudiniInputObject->GetTransform().Equals(Transform))
		{
			InHoudiniInputObject->SetTransform(Transform);
			InHoudiniInputObject->MarkChanged(true);
		}
	}

	return true;
}


bool
UHoudiniPublicAPIGeoInput::SetInputObjectTransformOffset_Implementation(
	const int32 InInputObjectIndex, const FTransform& InTransform)
{
	if (!SupportsTransformOffset())
	{
		SetErrorMessage(FString::Printf(
			TEXT("%s inputs do not support transform offsets."), *UEnum::GetValueAsString(GetInputType())));
		return false;
	}

	if (!InputObjects.IsValidIndex(InInputObjectIndex))
	{
		SetErrorMessage(TEXT("InInputObjectIndex is out of range."));
		return false;
	}

	if (!InputObjectTransformOffsetArray.IsValidIndex(InInputObjectIndex))
	{
		const int32 NumTransforms = InputObjectTransformOffsetArray.Num();
		InputObjectTransformOffsetArray.SetNum(InInputObjectIndex + 1);
		for (int32 TransformIndex = NumTransforms; TransformIndex < InInputObjectIndex; ++TransformIndex)
		{
			InputObjectTransformOffsetArray[TransformIndex] = FTransform::Identity;
		}
	}
	InputObjectTransformOffsetArray[InInputObjectIndex] = InTransform;

	return true;
}

bool
UHoudiniPublicAPIGeoInput::GetInputObjectTransformOffset_Implementation(
	const int32 InInputObjectIndex, FTransform& OutTransform) const
{
	if (!SupportsTransformOffset())
	{
		SetErrorMessage(FString::Printf(
			TEXT("%s inputs do not support transform offsets."), *UEnum::GetValueAsString(GetInputType())));
		return false;
	}

	if (!InputObjects.IsValidIndex(InInputObjectIndex))
	{
		SetErrorMessage(TEXT("InInputObjectIndex is out of range."));
		return false;
	}

	if (!InputObjectTransformOffsetArray.IsValidIndex(InInputObjectIndex))
	{
		SetErrorMessage(FString::Printf(
			TEXT("Input object at index '%d' does not have a transform offset set."), InInputObjectIndex));
		return false;
	}

	OutTransform = InputObjectTransformOffsetArray[InInputObjectIndex];
	return true;
}

bool
UHoudiniPublicAPIGeoInput::GetInputObjectTransformOffsetArray_Implementation(TArray<FTransform>& OutInputObjectTransformOffsetArray) const
{
	if (!SupportsTransformOffset())
	{
		SetErrorMessage(FString::Printf(
			TEXT("%s inputs do not support transform offsets."), *UEnum::GetValueAsString(GetInputType())));
		return false;
	}
	
	OutInputObjectTransformOffsetArray = InputObjectTransformOffsetArray;

	return true;
}


UHoudiniPublicAPICurveInputObject::UHoudiniPublicAPICurveInputObject()
	: bClosed(false)
	, bReversed(false)
	, CurveType(EHoudiniPublicAPICurveType::Polygon)
	, CurveMethod(EHoudiniPublicAPICurveMethod::CVs)
	, CurveBreakpointParameterization(EHoudiniPublicAPICurveBreakpointParameterization::Uniform)
{
	
}


void
UHoudiniPublicAPICurveInputObject::PopulateFromHoudiniSplineComponent(UHoudiniSplineComponent const* const InSpline)
{
	if (!IsValid(InSpline))
		return;

	bClosed = InSpline->IsClosedCurve();
	bReversed = InSpline->IsReversed();
	CurveType = ToHoudiniPublicAPICurveType(InSpline->GetCurveType());
	CurveMethod = ToHoudiniPublicAPICurveMethod(InSpline->GetCurveMethod());
	CurveBreakpointParameterization = ToHoudiniPublicAPICurveBreakpointParameterization(InSpline->GetCurveBreakpointParameterization());
	CurvePoints = InSpline->CurvePoints;
}

void
UHoudiniPublicAPICurveInputObject::CopyToHoudiniSplineComponent(UHoudiniSplineComponent* const InSpline) const
{
	if (!IsValid(InSpline))
		return;

	bool bAnyChanges = false;
	if (bClosed != InSpline->IsClosedCurve())
	{
		InSpline->SetClosedCurve(bClosed);
		bAnyChanges = true;
	}
	if (bReversed != InSpline->IsReversed())
	{
		InSpline->SetReversed(bReversed);
		bAnyChanges = true;
	}
	const EHoudiniCurveType HoudiniCurveType = ToHoudiniCurveType(CurveType);
	if (HoudiniCurveType != InSpline->GetCurveType())
	{
		InSpline->SetCurveType(HoudiniCurveType);
		bAnyChanges = true;
	}
	const EHoudiniCurveMethod HoudiniCurveMethod = ToHoudiniCurveMethod(CurveMethod);
	if (HoudiniCurveMethod != InSpline->GetCurveMethod())
	{
		InSpline->SetCurveMethod(HoudiniCurveMethod);
		bAnyChanges = true;
	}
	const EHoudiniCurveBreakpointParameterization HoudiniCurveBreakpointParameterization = ToHoudiniCurveBreakpointParamterization(CurveBreakpointParameterization);
	if (HoudiniCurveBreakpointParameterization != InSpline->GetCurveBreakpointParameterization())
	{
		InSpline->SetCurveBreakpointParameterization(HoudiniCurveBreakpointParameterization);
		bAnyChanges = true;
	}

	// Check if there are curve point differences
	bool bUpdatePoints = false;
	if (CurvePoints.Num() == InSpline->CurvePoints.Num())
	{
		const int32 NumPoints = CurvePoints.Num();
		for (int32 Index = 0; Index < NumPoints; ++Index)
		{
			const FTransform& A = CurvePoints[Index];
			const FTransform& B = InSpline->CurvePoints[Index];

			if (!A.Equals(B, 0.0f))
			{
				bUpdatePoints = true;
				break;
			}
		}
	}
	else
	{
		bUpdatePoints = true;
	}

	// If there are curve point differences, update the points
	if (bUpdatePoints)
	{
		InSpline->ResetCurvePoints();
		InSpline->ResetDisplayPoints();
		InSpline->CurvePoints = CurvePoints;
		bAnyChanges = true;
	}

	if (bAnyChanges)
		InSpline->MarkChanged(true);
}

EHoudiniCurveType
UHoudiniPublicAPICurveInputObject::ToHoudiniCurveType(const EHoudiniPublicAPICurveType InCurveType)
{
	switch (InCurveType)
	{
		case EHoudiniPublicAPICurveType::Invalid:
			return EHoudiniCurveType::Invalid;
		case EHoudiniPublicAPICurveType::Polygon:
			return EHoudiniCurveType::Polygon;
		case EHoudiniPublicAPICurveType::Nurbs:
			return EHoudiniCurveType::Nurbs;
		case EHoudiniPublicAPICurveType::Bezier:
			return EHoudiniCurveType::Bezier;
		case EHoudiniPublicAPICurveType::Points:
			return EHoudiniCurveType::Points;
	}

	return EHoudiniCurveType::Invalid;
}

EHoudiniCurveMethod
UHoudiniPublicAPICurveInputObject::ToHoudiniCurveMethod(const EHoudiniPublicAPICurveMethod InCurveMethod)
{
	switch (InCurveMethod)
	{
		case EHoudiniPublicAPICurveMethod::Invalid:
			return EHoudiniCurveMethod::Invalid;
		case EHoudiniPublicAPICurveMethod::CVs:
			return EHoudiniCurveMethod::CVs;
		case EHoudiniPublicAPICurveMethod::Breakpoints:
			return EHoudiniCurveMethod::Breakpoints;
		case EHoudiniPublicAPICurveMethod::Freehand:
			return EHoudiniCurveMethod::Freehand;
	}

	return EHoudiniCurveMethod::Invalid;
}

EHoudiniCurveBreakpointParameterization UHoudiniPublicAPICurveInputObject::ToHoudiniCurveBreakpointParamterization(
        const EHoudiniPublicAPICurveBreakpointParameterization InCurveBreakpointParameterization)
{
	switch (InCurveBreakpointParameterization)
	{
		case EHoudiniPublicAPICurveBreakpointParameterization::Invalid:
			return EHoudiniCurveBreakpointParameterization::Invalid;
		case EHoudiniPublicAPICurveBreakpointParameterization::Uniform:
			return EHoudiniCurveBreakpointParameterization::Uniform;
		case EHoudiniPublicAPICurveBreakpointParameterization::Chord:
			return EHoudiniCurveBreakpointParameterization::Chord;
		case EHoudiniPublicAPICurveBreakpointParameterization::Centripetal:
			return EHoudiniCurveBreakpointParameterization::Centripetal;
	}

	return EHoudiniCurveBreakpointParameterization::Invalid;
}

EHoudiniPublicAPICurveType
UHoudiniPublicAPICurveInputObject::ToHoudiniPublicAPICurveType(const EHoudiniCurveType InCurveType)
{
	switch (InCurveType)
	{
		case EHoudiniCurveType::Invalid:
			return EHoudiniPublicAPICurveType::Invalid;
		case EHoudiniCurveType::Polygon:
			return EHoudiniPublicAPICurveType::Polygon;
		case EHoudiniCurveType::Nurbs:
			return EHoudiniPublicAPICurveType::Nurbs;
		case EHoudiniCurveType::Bezier:
			return EHoudiniPublicAPICurveType::Bezier;
		case EHoudiniCurveType::Points:
			return EHoudiniPublicAPICurveType::Points;
	}

	return EHoudiniPublicAPICurveType::Invalid;
}

EHoudiniPublicAPICurveMethod
UHoudiniPublicAPICurveInputObject::ToHoudiniPublicAPICurveMethod(const EHoudiniCurveMethod InCurveMethod)
{
	switch (InCurveMethod)
	{
		case EHoudiniCurveMethod::Invalid:
			return EHoudiniPublicAPICurveMethod::Invalid;
		case EHoudiniCurveMethod::CVs:
			return EHoudiniPublicAPICurveMethod::CVs;
		case EHoudiniCurveMethod::Breakpoints:
			return EHoudiniPublicAPICurveMethod::Breakpoints;
		case EHoudiniCurveMethod::Freehand:
			return EHoudiniPublicAPICurveMethod::Freehand;
	}

	return EHoudiniPublicAPICurveMethod::Invalid;
}



EHoudiniPublicAPICurveBreakpointParameterization UHoudiniPublicAPICurveInputObject::
ToHoudiniPublicAPICurveBreakpointParameterization(
        const EHoudiniCurveBreakpointParameterization InCurveBreakpointParameterization)
{
	switch (InCurveBreakpointParameterization)
	{
		case EHoudiniCurveBreakpointParameterization::Invalid:
			return EHoudiniPublicAPICurveBreakpointParameterization::Invalid;
		case EHoudiniCurveBreakpointParameterization::Uniform:
			return EHoudiniPublicAPICurveBreakpointParameterization::Uniform;
		case EHoudiniCurveBreakpointParameterization::Chord:
			return EHoudiniPublicAPICurveBreakpointParameterization::Chord;
		case EHoudiniCurveBreakpointParameterization::Centripetal:
			return EHoudiniPublicAPICurveBreakpointParameterization::Centripetal;
	}

	return EHoudiniPublicAPICurveBreakpointParameterization::Invalid;
}

UHoudiniPublicAPICurveInput::UHoudiniPublicAPICurveInput()
{
	bKeepWorldTransform = false;
	bCookOnCurveChanged = true;
	bAddRotAndScaleAttributesOnCurves = false;
	bUseLegacyInputCurves = false;
}

bool
UHoudiniPublicAPICurveInput::IsAcceptableObjectForInput_Implementation(UObject* InObject) const
{
	if (!IsValid(InObject))
		return false;

	if (InObject->IsA<UHoudiniPublicAPICurveInputObject>())
		return true;

	return Super::IsAcceptableObjectForInput_Implementation(InObject);
}

bool
UHoudiniPublicAPICurveInput::PopulateFromHoudiniInput(UHoudiniInput const* const InInput)
{
	if (!Super::PopulateFromHoudiniInput(InInput))
		return false;

	bCookOnCurveChanged = InInput->GetCookOnCurveChange();
	bAddRotAndScaleAttributesOnCurves = InInput->IsAddRotAndScaleAttributesEnabled();
	bUseLegacyInputCurves = InInput->IsUseLegacyInputCurvesEnabled();

	return true;
}

bool
UHoudiniPublicAPICurveInput::UpdateHoudiniInput(UHoudiniInput* const InInput) const
{
	if (!Super::UpdateHoudiniInput(InInput))
		return false;

	bool bAnyChanges = false;
	if (InInput->GetCookOnCurveChange() != bCookOnCurveChanged)
	{
		InInput->SetCookOnCurveChange(bCookOnCurveChanged);
		bAnyChanges = true;
	}

	if (InInput->IsAddRotAndScaleAttributesEnabled() != bAddRotAndScaleAttributesOnCurves)
	{
		InInput->SetAddRotAndScaleAttributes(bAddRotAndScaleAttributesOnCurves);
		bAnyChanges = true;
	}

	if (InInput->IsUseLegacyInputCurvesEnabled() != bUseLegacyInputCurves)
	{
		InInput->SetUseLegacyInputCurve(bUseLegacyInputCurves);
		bAnyChanges = true;
	}

	if (bAnyChanges)
	{
		InInput->MarkChanged(true);
	}

	return true;
}

UObject*
UHoudiniPublicAPICurveInput::ConvertInternalInputObject(UObject* InInternalInputObject)
{
	UObject* Object = Super::ConvertInternalInputObject(InInternalInputObject);

	// If the input object is a houdini spline component, convert it to an API curve wrapper
	if (IsValid(Object) && Object->IsA<UHoudiniSplineComponent>())
	{
		UHoudiniPublicAPICurveInputObject* const Curve = NewObject<UHoudiniPublicAPICurveInputObject>(
			this, UHoudiniPublicAPICurveInputObject::StaticClass());
		if (IsValid(Curve))
		{
			Curve->PopulateFromHoudiniSplineComponent(Cast<UHoudiniSplineComponent>(Object));
			return Curve;
		}
	}

	return Object;
}

UObject*
UHoudiniPublicAPICurveInput::ConvertAPIInputObjectAndAssignToInput(UObject* InAPIInputObject, UHoudiniInput* InHoudiniInput, const int32 InInputIndex) const
{
	UObject* Object = nullptr;

	// If the input is an API curve wrapper, convert it to a UHoudiniSplineComponent
	if (IsValid(InAPIInputObject) && InAPIInputObject->IsA<UHoudiniPublicAPICurveInputObject>() && IsValid(InHoudiniInput))
	{
		UHoudiniPublicAPICurveInputObject* const InAPICurveInputObject = Cast<UHoudiniPublicAPICurveInputObject>(InAPIInputObject);
		
		// If there is an existing input object at this index, and it is a HoudiniSplineComponent, then just update it
		// otherwise, create a new input object wrapper
		bool bCreateNew = false;
		UHoudiniInputObject const* const CurrentHoudiniInputObject = InHoudiniInput->GetHoudiniInputObjectAt(InInputIndex); 
		UObject* const CurrentInputObject = InHoudiniInput->GetInputObjectAt(InInputIndex);
		if (IsValid(CurrentInputObject) && CurrentInputObject->IsA<UHoudiniSplineComponent>() &&
			IsValid(CurrentHoudiniInputObject) && CurrentHoudiniInputObject->IsA<UHoudiniInputHoudiniSplineComponent>())
		{
			UHoudiniSplineComponent* CurrentSpline = Cast<UHoudiniSplineComponent>(CurrentInputObject);
			if (IsValid(CurrentSpline))
			{
				if (IsValid(InAPICurveInputObject))
				{
					InAPICurveInputObject->CopyToHoudiniSplineComponent(CurrentSpline);
					// Currently the CopyToHoudiniSplineComponent function does not return an indication of if anything
					// actually changed, so we have to assume this is a change
					
					InHoudiniInput->MarkChanged(true);
				}
				Object = CurrentSpline;
			}
			else
			{
				bCreateNew = true;
			}
		}
		else
		{
			bCreateNew = true;
		}

		if (bCreateNew)
		{
			// Replace any object that is already at this index: we remove the current input object first, then
			// we create the new one
			if (CurrentInputObject)
			{
				// Keep the space/index in the array, we're going to set the new input object at the same index
				const bool bRemoveIndexFromArray = false;
				InHoudiniInput->DeleteInputObjectAt(InInputIndex, bRemoveIndexFromArray);
				InHoudiniInput->MarkChanged(true);
			}
			
			UHoudiniInputHoudiniSplineComponent* FromHoudiniSplineInputComponent = nullptr;
			const bool bAttachToParent = true;
			const bool bAppendToInputArray = false;
			bool bBlueprintStructureModified;
			UHoudiniInputHoudiniSplineComponent* const NewHoudiniInputObject = InHoudiniInput->CreateHoudiniSplineInput(
				FromHoudiniSplineInputComponent, bAttachToParent, bAppendToInputArray, bBlueprintStructureModified);
			if (IsValid(NewHoudiniInputObject))
			{
				UHoudiniSplineComponent* HoudiniSplineComponent = NewHoudiniInputObject->GetCurveComponent();
				if (IsValid(HoudiniSplineComponent))
				{
					// Populate the HoudiniSplineComponent from the curve wrapper
					if (IsValid(InAPICurveInputObject))
						InAPICurveInputObject->CopyToHoudiniSplineComponent(HoudiniSplineComponent);
					Object = HoudiniSplineComponent;
				}
			}
			
			TArray<UHoudiniInputObject*>* HoudiniInputObjectArray = InHoudiniInput->GetHoudiniInputObjectArray(InHoudiniInput->GetInputType());
			if (HoudiniInputObjectArray && HoudiniInputObjectArray->IsValidIndex(InInputIndex))
			{
				(*HoudiniInputObjectArray)[InInputIndex] = IsValid(NewHoudiniInputObject) ? NewHoudiniInputObject : nullptr;
				InHoudiniInput->MarkChanged(true);
			}
		}
	}
	else
	{
		Object = Super::ConvertAPIInputObjectAndAssignToInput(InAPIInputObject, InHoudiniInput, InInputIndex);
	}

	return Object;
}

UHoudiniPublicAPIWorldInput::UHoudiniPublicAPIWorldInput()
{
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	bKeepWorldTransform = true;
	bIsWorldInputBoundSelector = false;
	bWorldInputBoundSelectorAutoUpdate = false;
	UnrealSplineResolution = HoudiniRuntimeSettings ? HoudiniRuntimeSettings->MarshallingSplineResolution : 50.0f;
	bPreferNaniteFallbackMesh = HoudiniRuntimeSettings ? HoudiniRuntimeSettings->bPreferNaniteFallbackMesh : false;
	bExportLevelInstanceContent = true;
	bDirectlyConnectHdas = true;
	bExportHeightDataPerEditLayer = true;
	bExportMergedPaintLayers = true;
	bExportPaintLayersPerEditLayer = false;
}

bool
UHoudiniPublicAPIWorldInput::SetInputObjects_Implementation(const TArray<UObject*>& InObjects)
{
	if (bIsWorldInputBoundSelector)
	{
		SetErrorMessage(
			TEXT("This world input is not currently configured as a bound selector (bIsWorldInputBoundSelector == false)"));
		return false;
	}

	return Super::SetInputObjects_Implementation(InObjects);
}

bool
UHoudiniPublicAPIWorldInput::PopulateFromHoudiniInput(UHoudiniInput const* const InInput)
{
	if (!Super::PopulateFromHoudiniInput(InInput))
		return false;

	TArray<AActor*> const* const BoundSelectorObjectArray = InInput->GetBoundSelectorObjectArray();
	if (BoundSelectorObjectArray)
		WorldInputBoundSelectorObjects = *BoundSelectorObjectArray;
	else
		WorldInputBoundSelectorObjects.Empty();

	bIsWorldInputBoundSelector = InInput->IsWorldInputBoundSelector();
	bWorldInputBoundSelectorAutoUpdate = InInput->GetWorldInputBoundSelectorAutoUpdates();
	UnrealSplineResolution = InInput->GetUnrealSplineResolution();
	bExportLevelInstanceContent = InInput->IsExportLevelInstanceContentEnabled();
	bDirectlyConnectHdas = InInput->GetDirectlyConnectHdas();
	bExportHeightDataPerEditLayer = InInput->IsEditLayerHeightExportEnabled();
	bExportPaintLayersPerEditLayer= InInput->IsPaintLayerPerEditLayerExportEnabled();
	bExportMergedPaintLayers = InInput->IsMergedPaintLayerExportEnabled();

	return true;
}

bool
UHoudiniPublicAPIWorldInput::UpdateHoudiniInput(UHoudiniInput* const InInput) const
{
	if (!Super::UpdateHoudiniInput(InInput))
		return false;

	bool bAnyChanges = false;
	TArray<AActor*>* const BoundSelectorObjectArray = InInput->GetBoundSelectorObjectArray();
	if (BoundSelectorObjectArray)
	{
		if (BoundSelectorObjectArray->Num() != WorldInputBoundSelectorObjects.Num())
		{
			InInput->SetBoundSelectorObjectsNumber(WorldInputBoundSelectorObjects.Num());
			bAnyChanges = true;
		}

		bool bNeedToUpdateBoundObjects = false;
		for (int Idx = 0; Idx < WorldInputBoundSelectorObjects.Num(); Idx++)
		{
			if ((*BoundSelectorObjectArray)[Idx] != WorldInputBoundSelectorObjects[Idx])
				bNeedToUpdateBoundObjects = true;
		}

		if (bNeedToUpdateBoundObjects)
		{
			*BoundSelectorObjectArray = WorldInputBoundSelectorObjects;
			bAnyChanges = true;
		}
	}

	if (InInput->IsWorldInputBoundSelector() != bIsWorldInputBoundSelector)
	{
		InInput->SetWorldInputBoundSelector(bIsWorldInputBoundSelector);
		bAnyChanges = true;
	}

	if (InInput->GetWorldInputBoundSelectorAutoUpdates() != bWorldInputBoundSelectorAutoUpdate)
	{
		InInput->SetWorldInputBoundSelectorAutoUpdates(bWorldInputBoundSelectorAutoUpdate);
		bAnyChanges = true;
	}

	if (InInput->GetUnrealSplineResolution() != UnrealSplineResolution)
	{
		InInput->SetUnrealSplineResolution(UnrealSplineResolution);
		bAnyChanges = true;
	}

	if (InInput->IsExportLevelInstanceContentEnabled() != bExportLevelInstanceContent)
	{
		InInput->SetExportLevelInstanceContent(bExportLevelInstanceContent);
		bAnyChanges = true;
	}
	
	if (InInput->GetDirectlyConnectHdas() != bDirectlyConnectHdas)
	{
		InInput->SetDirectlyConnectHdas(bDirectlyConnectHdas);
		bAnyChanges = true;
	}

	if (InInput->IsEditLayerHeightExportEnabled() != bExportHeightDataPerEditLayer)
	{
		InInput->SetExportHeightDataPerEditLayer(bExportHeightDataPerEditLayer);
		bAnyChanges = true;
	}

	if (InInput->IsPaintLayerPerEditLayerExportEnabled() != bExportPaintLayersPerEditLayer)
	{
		InInput->SetExportPaintLayerPerEditLayer(bExportPaintLayersPerEditLayer);
		bAnyChanges = true;
	}


	if (InInput->IsMergedPaintLayerExportEnabled() != bExportMergedPaintLayers)
	{
		InInput->SetExportMergedPaintLayers(bExportMergedPaintLayers);
		bAnyChanges = true;
	}

	if (bAnyChanges)
	{
		InInput->MarkChanged(true);
	}	

	return true;
}
