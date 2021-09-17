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

	const TArray<UHoudiniInputObject*>* SrcInputObjectsPtr = InInput->GetHoudiniInputObjectArray(InputType);
	if (SrcInputObjectsPtr && SrcInputObjectsPtr->Num() > 0)
	{
		InputObjects.Empty(SrcInputObjectsPtr->Num()); 
		for (UHoudiniInputObject const* const SrcInputObject : *SrcInputObjectsPtr)
		{
			if (!IsValid(SrcInputObject))
				continue;

			UObject* NewInputObject = ConvertInternalInputObject(SrcInputObject->GetObject());
			if (NewInputObject && NewInputObject->IsPendingKill())
			{
				SetErrorMessage(FString::Printf(
					TEXT("One of the input objects is non-null but pending kill/invalid.")));
				return false;
			}
			
			InputObjects.Add(NewInputObject);

			CopyHoudiniInputObjectProperties(SrcInputObject, NewInputObject);
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

	// Set / change the input type
	const EHoudiniInputType InputType = GetInputType();
	bool bBlueprintStructureModified = false;
	InInput->SetInputType(InputType, bBlueprintStructureModified);

	// Set any general settings
	bool bAnyChanges = false;
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

	// Copy / set the input objects on the Houdini Input
	const int32 NumInputObjects = InputObjects.Num();
	InInput->SetInputObjectsNumber(InputType, NumInputObjects);
	for (int32 Index = 0; Index < NumInputObjects; ++Index)
	{
		UObject* const InputObject = InputObjects[Index];

		if (!IsValid(InputObject))
		{
			InInput->SetInputObjectAt(Index, nullptr);
		}
		else
		{
			ConvertAPIInputObjectAndAssignToInput(InputObject, InInput, Index);
			UHoudiniInputObject *DstInputObject = InInput->GetHoudiniInputObjectAt(Index);
			if (DstInputObject)
				CopyPropertiesToHoudiniInputObject(InputObject, DstInputObject);
		}
	}

	if (bAnyChanges)
	{
		InInput->MarkChanged(true);
	}

	return true;
}

bool
UHoudiniPublicAPIInput::CopyHoudiniInputObjectProperties(UHoudiniInputObject const* const InInputObject, UObject* const InObject)
{
	if (!IsValid(InInputObject) || !IsValid(InObject))
		return false;

	return true;
}

bool
UHoudiniPublicAPIInput::CopyPropertiesToHoudiniInputObject(UObject* const InObject, UHoudiniInputObject* const InInputObject) const
{
	if (!IsValid(InObject) || !IsValid(InInputObject))
		return false;

	// const EHoudiniInputObjectType InputObjectType = InInputObject->Type;

	if (InInputObject->GetImportAsReference() != bImportAsReference)
	{
		InInputObject->SetImportAsReference(bImportAsReference);
		InInputObject->MarkChanged(true);
	}

	// switch (InputObjectType)
	// {
	// 	case EHoudiniInputObjectType::StaticMesh:
	// 	case EHoudiniInputObjectType::FoliageType_InstancedStaticMesh:
	// 		break;
	// 	case EHoudiniInputObjectType::Object:
	// 	case EHoudiniInputObjectType::SkeletalMesh:
	// 	case EHoudiniInputObjectType::HoudiniSplineComponent:
	// 	case EHoudiniInputObjectType::SceneComponent:
	// 	case EHoudiniInputObjectType::StaticMeshComponent:
	// 	case EHoudiniInputObjectType::InstancedStaticMeshComponent:
	// 	case EHoudiniInputObjectType::SplineComponent:
	// 	case EHoudiniInputObjectType::HoudiniAssetComponent:
	// 	case EHoudiniInputObjectType::Actor:
	// 	case EHoudiniInputObjectType::Landscape:
	// 	case EHoudiniInputObjectType::Brush:
	// 	case EHoudiniInputObjectType::CameraComponent:
	// 	case EHoudiniInputObjectType::DataTable:
	// 	case EHoudiniInputObjectType::HoudiniAssetActor:
	// 		break;
	//
	// 	case EHoudiniInputObjectType::Invalid:
	// 		return false;
	// }
	
	return true;
}

UObject*
UHoudiniPublicAPIInput::ConvertAPIInputObjectAndAssignToInput(UObject* InAPIInputObject, UHoudiniInput* InHoudiniInput, const int32 InInputIndex) const
{
	if (!IsValid(InHoudiniInput))
		return nullptr;

	UObject* const ObjectToSet = (InAPIInputObject && !InAPIInputObject->IsPendingKill()) ? InAPIInputObject : nullptr;
	InHoudiniInput->SetInputObjectAt(InInputIndex, ObjectToSet);

	return ObjectToSet;
}

UHoudiniPublicAPIGeoInput::UHoudiniPublicAPIGeoInput()
{
	bKeepWorldTransform = false;
	bPackBeforeMerge = false;
	bExportLODs = false;
	bExportSockets = false;
	bExportColliders = false;
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

bool
UHoudiniPublicAPIGeoInput::CopyHoudiniInputObjectProperties(UHoudiniInputObject const* const InInputObject, UObject* const InObject)
{
	if (!Super::CopyHoudiniInputObjectProperties(InInputObject, InObject))
		return false;
	
	if (!IsValid(InInputObject) || !IsValid(InObject))
		return false;

	// Copy the transform offset
	SetObjectTransformOffset(InObject, InInputObject->Transform);

	return true;
}


bool
UHoudiniPublicAPIGeoInput::CopyPropertiesToHoudiniInputObject(UObject* const InObject, UHoudiniInputObject* const InInputObject) const
{
	if (!Super::CopyPropertiesToHoudiniInputObject(InObject, InInputObject))
		return false;

	if (!IsValid(InObject) || !IsValid(InInputObject))
		return false;

	// Copy the transform offset
	FTransform Transform;
	if (GetObjectTransformOffset(InObject, Transform))
	{
		if (!InInputObject->Transform.Equals(Transform))
		{
			InInputObject->Transform = Transform;
			InInputObject->MarkChanged(true);
		}
	}

	return true;
}

bool
UHoudiniPublicAPIGeoInput::SetObjectTransformOffset_Implementation(UObject* InObject, const FTransform& InTransform)
{
	// Ensure that InObject is valid and has already been added as input object
	if (!IsValid(InObject))
	{
		SetErrorMessage(TEXT("InObject is invalid."));
		return false;
	}

	if (INDEX_NONE == InputObjects.Find(InObject))
	{
		SetErrorMessage(FString::Printf(
			TEXT("InObject '%s' is not currently set as input object on this input."), *(InObject->GetName())));
		return false;
	}

	InputObjectTransformOffsets.Add(InObject, InTransform);

	return true;
}

bool
UHoudiniPublicAPIGeoInput::GetObjectTransformOffset_Implementation(UObject* InObject, FTransform& OutTransform) const
{
	// Ensure that InObject is valid and has already been added as input object
	if (!IsValid(InObject))
	{
		SetErrorMessage(TEXT("InObject is invalid."));
		return false;
	}

	if (INDEX_NONE == InputObjects.Find(InObject))
	{
		SetErrorMessage(FString::Printf(
			TEXT("InObject '%s' is not currently set as input object on this input."), *(InObject->GetName())));
		return false;
	}
	
	FTransform const* const TransformPtr = InputObjectTransformOffsets.Find(InObject);
	if (!TransformPtr)
	{
		SetErrorMessage(FString::Printf(
			TEXT("InObject '%s' does not have a transform offset set."), *(InObject->GetName())));
		return false;
	}
	
	OutTransform = *TransformPtr;
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

	InSpline->SetClosedCurve(bClosed);
	InSpline->SetReversed(bReversed);
	InSpline->SetCurveType(ToHoudiniCurveType(CurveType));
	InSpline->SetCurveMethod(ToHoudiniCurveMethod(CurveMethod));
	InSpline->SetCurveBreakpointParameterization(ToHoudiniCurveBreakpointParamterization(CurveBreakpointParameterization));
	InSpline->ResetCurvePoints();
	InSpline->ResetDisplayPoints();
	InSpline->CurvePoints = CurvePoints;
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
		UHoudiniInputHoudiniSplineComponent* FromHoudiniSplineInputComponent = nullptr;
		const bool bAttachToParent = true;
		const bool bAppendToInputArray = false;
		bool bBlueprintStructureModified;
		UHoudiniInputHoudiniSplineComponent* const NewHoudiniInputObject = InHoudiniInput->CreateHoudiniSplineInput(FromHoudiniSplineInputComponent, bAttachToParent, bAppendToInputArray, bBlueprintStructureModified);
		if (IsValid(NewHoudiniInputObject))
		{
			UHoudiniSplineComponent* HoudiniSplineComponent = NewHoudiniInputObject->GetCurveComponent();
			if (IsValid(HoudiniSplineComponent))
			{
				// Populate the HoudiniSplineComponent from the curve wrapper
				Cast<UHoudiniPublicAPICurveInputObject>(InAPIInputObject)->CopyToHoudiniSplineComponent(HoudiniSplineComponent);
				Object = HoudiniSplineComponent;
			}
		}
		
		TArray<UHoudiniInputObject*>* HoudiniInputObjectArray = InHoudiniInput->GetHoudiniInputObjectArray(InHoudiniInput->GetInputType());
		if (HoudiniInputObjectArray && HoudiniInputObjectArray->IsValidIndex(InInputIndex))
			(*HoudiniInputObjectArray)[InInputIndex] = IsValid(NewHoudiniInputObject) ? NewHoudiniInputObject : nullptr; 
	}
	else
	{
		Object = Super::ConvertAPIInputObjectAndAssignToInput(InAPIInputObject, InHoudiniInput, InInputIndex);
	}

	return Object;
}


UHoudiniPublicAPIAssetInput::UHoudiniPublicAPIAssetInput()
{
	bKeepWorldTransform = true;
}

bool
UHoudiniPublicAPIAssetInput::IsAcceptableObjectForInput_Implementation(UObject* InObject) const
{
	if (IsValid(InObject) && InObject->IsA<UHoudiniPublicAPIAssetWrapper>())
	{
		UHoudiniPublicAPIAssetWrapper* const Wrapper = Cast<UHoudiniPublicAPIAssetWrapper>(InObject);
		AHoudiniAssetActor* const AssetActor = Cast<AHoudiniAssetActor>(Wrapper->GetHoudiniAssetActor());
		if (IsValid(AssetActor) && IsValid(AssetActor->HoudiniAssetComponent))
			return true;
	}

	return Super::IsAcceptableObjectForInput_Implementation(InObject);
}

bool
UHoudiniPublicAPIAssetInput::PopulateFromHoudiniInput(UHoudiniInput const* const InInput)
{
	if (!Super::PopulateFromHoudiniInput(InInput))
		return false;

	return true;
}

bool
UHoudiniPublicAPIAssetInput::UpdateHoudiniInput(UHoudiniInput* const InInput) const
{
	if (!Super::UpdateHoudiniInput(InInput))
		return false;

	return true;
}

UObject*
UHoudiniPublicAPIAssetInput::ConvertInternalInputObject(UObject* InInternalInputObject)
{
	// If InInternalInputObject is a Houdini Asset Component or Houdini Asset Actor, wrap it with the API and return
	// wrapper.
	if (IsValid(InInternalInputObject))
	{
		if ((InInternalInputObject->IsA<AHoudiniAssetActor>() || InInternalInputObject->IsA<UHoudiniAssetComponent>()) &&
				UHoudiniPublicAPIAssetWrapper::CanWrapHoudiniObject(InInternalInputObject))
		{
			return UHoudiniPublicAPIAssetWrapper::CreateWrapper(this, InInternalInputObject);
		}
	}

	return Super::ConvertInternalInputObject(InInternalInputObject);
}

UObject*
UHoudiniPublicAPIAssetInput::ConvertAPIInputObjectAndAssignToInput(UObject* InAPIInputObject, UHoudiniInput* InHoudiniInput, const int32 InInputIndex) const
{
	// If InAPIInputObject is an asset wrapper, extract the underlying HoudiniAssetComponent.
	if (IsValid(InAPIInputObject) && InAPIInputObject->IsA<UHoudiniPublicAPIAssetWrapper>())
	{
		UHoudiniPublicAPIAssetWrapper* const Wrapper = Cast<UHoudiniPublicAPIAssetWrapper>(InAPIInputObject);
		if (Wrapper)
		{
			UHoudiniAssetComponent* const HAC = Wrapper->GetHoudiniAssetComponent();
			if (IsValid(HAC))
			{
				return Super::ConvertAPIInputObjectAndAssignToInput(HAC, InHoudiniInput, InInputIndex);
			}
		}
	}

	return Super::ConvertAPIInputObjectAndAssignToInput(InAPIInputObject, InHoudiniInput, InInputIndex);
}


UHoudiniPublicAPIWorldInput::UHoudiniPublicAPIWorldInput()
{
	bKeepWorldTransform = true;
	bIsWorldInputBoundSelector = false;
	bWorldInputBoundSelectorAutoUpdate = false;
	UnrealSplineResolution = 50.0f;
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

	return true;
}

bool
UHoudiniPublicAPIWorldInput::UpdateHoudiniInput(UHoudiniInput* const InInput) const
{
	if (!Super::UpdateHoudiniInput(InInput))
		return false;
	
	InInput->SetBoundSelectorObjectsNumber(WorldInputBoundSelectorObjects.Num());
	TArray<AActor*>* const BoundSelectorObjectArray = InInput->GetBoundSelectorObjectArray();
	if (BoundSelectorObjectArray)
		*BoundSelectorObjectArray = WorldInputBoundSelectorObjects;
	InInput->SetWorldInputBoundSelector(bIsWorldInputBoundSelector);
	InInput->SetWorldInputBoundSelectorAutoUpdates(bWorldInputBoundSelectorAutoUpdate);
	InInput->SetUnrealSplineResolution(UnrealSplineResolution);
	InInput->MarkChanged(true);

	return true;
}


UHoudiniPublicAPILandscapeInput::UHoudiniPublicAPILandscapeInput()
	: bUpdateInputLandscape(false)
	, LandscapeExportType(EHoudiniLandscapeExportType::Heightfield)
	, bLandscapeExportSelectionOnly(false)
	, bLandscapeAutoSelectComponent(false)
	, bLandscapeExportMaterials(false)
	, bLandscapeExportLighting(false)
	, bLandscapeExportNormalizedUVs(false)
	, bLandscapeExportTileUVs(false)
{
	
}

bool
UHoudiniPublicAPILandscapeInput::PopulateFromHoudiniInput(UHoudiniInput const* const InInput)
{
	if (!Super::PopulateFromHoudiniInput(InInput))
		return false;

	bUpdateInputLandscape = InInput->bUpdateInputLandscape;
	LandscapeExportType = InInput->GetLandscapeExportType();
	bLandscapeExportSelectionOnly = InInput->bLandscapeExportSelectionOnly;
	bLandscapeAutoSelectComponent = InInput->bLandscapeAutoSelectComponent;
	bLandscapeExportMaterials = InInput->bLandscapeExportMaterials;
	bLandscapeExportLighting = InInput->bLandscapeExportLighting;
	bLandscapeExportNormalizedUVs = InInput->bLandscapeExportNormalizedUVs;
	bLandscapeExportTileUVs = InInput->bLandscapeExportTileUVs;

	return true;
}

bool
UHoudiniPublicAPILandscapeInput::UpdateHoudiniInput(UHoudiniInput* const InInput) const
{
	if (!Super::UpdateHoudiniInput(InInput))
		return false;

	bool bAnyChanges = false;
	if (InInput->bUpdateInputLandscape != bUpdateInputLandscape)
	{
		InInput->bUpdateInputLandscape = bUpdateInputLandscape;
		bAnyChanges = true;
	}
	
	if (InInput->GetLandscapeExportType() != LandscapeExportType)
	{
		InInput->SetLandscapeExportType(LandscapeExportType);
		InInput->SetHasLandscapeExportTypeChanged(true);

		// Mark each input object as changed as well
		TArray<UHoudiniInputObject*>* LandscapeInputObjectsArray = InInput->GetHoudiniInputObjectArray(GetInputType());
		if (LandscapeInputObjectsArray)
		{
			for (UHoudiniInputObject *NextInputObj : *LandscapeInputObjectsArray)
			{
				if (!NextInputObj)
					continue;
				NextInputObj->MarkChanged(true);
			}
		}

		bAnyChanges = true;
	}
	
	if (InInput->bLandscapeExportSelectionOnly != bLandscapeExportSelectionOnly)
	{
		InInput->bLandscapeExportSelectionOnly = bLandscapeExportSelectionOnly;
		bAnyChanges = true;
	}
	
	if (InInput->bLandscapeAutoSelectComponent != bLandscapeAutoSelectComponent)
	{
		InInput->bLandscapeAutoSelectComponent = bLandscapeAutoSelectComponent;
		bAnyChanges = true;
	}
	
	if (InInput->bLandscapeExportMaterials != bLandscapeExportMaterials)
	{
		InInput->bLandscapeExportMaterials = bLandscapeExportMaterials;
		bAnyChanges = true;
	}
	
	if (InInput->bLandscapeExportLighting != bLandscapeExportLighting)
	{
		InInput->bLandscapeExportLighting = bLandscapeExportLighting;
		bAnyChanges = true;
	}
	
	if (InInput->bLandscapeExportNormalizedUVs != bLandscapeExportNormalizedUVs)
	{
		InInput->bLandscapeExportNormalizedUVs = bLandscapeExportNormalizedUVs;
		bAnyChanges = true;
	}
	
	if (InInput->bLandscapeExportTileUVs != bLandscapeExportTileUVs)
	{
		InInput->bLandscapeExportTileUVs = bLandscapeExportTileUVs;
		bAnyChanges = true;
	}

	if (bAnyChanges)
	{
		InInput->MarkChanged(true);
	}
	

	return true;
}

UHoudiniPublicAPIGeometryCollectionInput::UHoudiniPublicAPIGeometryCollectionInput()
{
	bKeepWorldTransform = false;
	// TODO: This class is quite similar to UHoudiniPublicAPIGeoInput. Maybe some sort of inheritance would be beneficial?
}

bool UHoudiniPublicAPIGeometryCollectionInput::SetObjectTransformOffset_Implementation(UObject* InObject,
											const FTransform& InTransform)
{
	// Ensure that InObject is valid and has already been added as input object
	if (!IsValid(InObject))
	{
		SetErrorMessage(TEXT("InObject is invalid."));
		return false;
	}

	if (INDEX_NONE == InputObjects.Find(InObject))
	{
		SetErrorMessage(FString::Printf(
			TEXT("InObject '%s' is not currently set as input object on this input."), *(InObject->GetName())));
		return false;
	}

	InputObjectTransformOffsets.Add(InObject, InTransform);

	return true;
}

bool UHoudiniPublicAPIGeometryCollectionInput::GetObjectTransformOffset_Implementation(UObject* InObject,
	FTransform& OutTransform) const
{
	// Ensure that InObject is valid and has already been added as input object
	if (!IsValid(InObject))
	{
		SetErrorMessage(TEXT("InObject is invalid."));
		return false;
	}

	if (INDEX_NONE == InputObjects.Find(InObject))
	{
		SetErrorMessage(FString::Printf(
                        TEXT("InObject '%s' is not currently set as input object on this input."), *(InObject->GetName())));
		return false;
	}
	
	FTransform const* const TransformPtr = InputObjectTransformOffsets.Find(InObject);
	if (!TransformPtr)
	{
		SetErrorMessage(FString::Printf(
                        TEXT("InObject '%s' does not have a transform offset set."), *(InObject->GetName())));
		return false;
	}
	
	OutTransform = *TransformPtr;
	return true;
}

bool UHoudiniPublicAPIGeometryCollectionInput::PopulateFromHoudiniInput(UHoudiniInput const* const InInput)
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

	const TArray<UHoudiniInputObject*>* SrcInputObjectsPtr = InInput->GetHoudiniInputObjectArray(InputType);
	if (SrcInputObjectsPtr && SrcInputObjectsPtr->Num() > 0)
	{
		InputObjects.Empty(SrcInputObjectsPtr->Num()); 
		for (UHoudiniInputObject const* const SrcInputObject : *SrcInputObjectsPtr)
		{
			if (!IsValid(SrcInputObject))
				continue;

			UObject* NewInputObject = ConvertInternalInputObject(SrcInputObject->GetObject());
			if (NewInputObject && NewInputObject->IsPendingKill())
			{
				SetErrorMessage(FString::Printf(
                                        TEXT("One of the input objects is non-null but pending kill/invalid.")));
				return false;
			}
			
			InputObjects.Add(NewInputObject);

			CopyHoudiniInputObjectProperties(SrcInputObject, NewInputObject);
		}
	}

	return true;
}

bool UHoudiniPublicAPIGeometryCollectionInput::UpdateHoudiniInput(UHoudiniInput* const InInput) const
{
	if (!IsValid(InInput))
	{
		SetErrorMessage(TEXT("InInput is invalid."));
		return false;
	}

	// Set / change the input type
	const EHoudiniInputType InputType = GetInputType();
	bool bBlueprintStructureModified = false;
	InInput->SetInputType(InputType, bBlueprintStructureModified);

	// Set any general settings
	bool bAnyChanges = false;
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

	// Copy / set the input objects on the Houdini Input
	const int32 NumInputObjects = InputObjects.Num();
	InInput->SetInputObjectsNumber(InputType, NumInputObjects);
	for (int32 Index = 0; Index < NumInputObjects; ++Index)
	{
		UObject* const InputObject = InputObjects[Index];

		if (!IsValid(InputObject))
		{
			InInput->SetInputObjectAt(Index, nullptr);
		}
		else
		{
			ConvertAPIInputObjectAndAssignToInput(InputObject, InInput, Index);
			UHoudiniInputObject *DstInputObject = InInput->GetHoudiniInputObjectAt(Index);
			if (DstInputObject)
				CopyPropertiesToHoudiniInputObject(InputObject, DstInputObject);
		}
	}

	if (bAnyChanges)
	{
		InInput->MarkChanged(true);
	}

	return true;
}

bool UHoudiniPublicAPIGeometryCollectionInput::CopyHoudiniInputObjectProperties(
	UHoudiniInputObject const* const InInputObject, UObject* const InObject)
{
	if (!Super::CopyHoudiniInputObjectProperties(InInputObject, InObject))
		return false;
	
	if (!IsValid(InInputObject) || !IsValid(InObject))
		return false;

	// Copy the transform offset
	SetObjectTransformOffset(InObject, InInputObject->Transform);

	return true;
}

bool UHoudiniPublicAPIGeometryCollectionInput::CopyPropertiesToHoudiniInputObject(UObject* const InObject,
	UHoudiniInputObject* const InInputObject) const
{
	if (!Super::CopyPropertiesToHoudiniInputObject(InObject, InInputObject))
		return false;

	if (!IsValid(InObject) || !IsValid(InInputObject))
		return false;

	// Copy the transform offset
	FTransform Transform;
	if (GetObjectTransformOffset(InObject, Transform))
	{
		if (!InInputObject->Transform.Equals(Transform))
		{
			InInputObject->Transform = Transform;
			InInputObject->MarkChanged(true);
		}
	}

	return true;
}
