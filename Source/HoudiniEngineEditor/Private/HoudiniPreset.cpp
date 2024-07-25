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

#include "HoudiniPreset.h"

#include "HoudiniParameterChoice.h"
#include "HoudiniParameterColor.h"
#include "HoudiniParameterFile.h"
#include "HoudiniParameterInt.h"
#include "HoudiniParameterFloat.h"
#include "HoudiniParameterRamp.h"
#include "HoudiniParameterString.h"
#include "HoudiniParameterToggle.h"
#include "HoudiniEngineRuntime.h"


FHoudiniPresetCurveInputObject::FHoudiniPresetCurveInputObject()
	: bClosed(false)
	, bReversed(false)
	, CurveOrder(0)
	, bIsHoudiniSplineVisible(false)
	, CurveType(EHoudiniCurveType::Invalid)
	, CurveMethod(EHoudiniCurveMethod::Invalid)
	, CurveBreakpointParameterization(EHoudiniCurveBreakpointParameterization::Invalid)
	, bIsOutputCurve(false)
	, bCookOnCurveChanged(false)
	, bIsLegacyInputCurve(false)
	, bIsInputCurve(false)
	, bIsEditableOutputCurve(false)
{
}

bool
FHoudiniPresetHelpers::IsSupportedInputType(const EHoudiniInputType InputType)
{
	if (InputType == EHoudiniInputType::Geometry ||
		InputType == EHoudiniInputType::Curve)
	{
		return true;
	}

	return false;
}

bool
FHoudiniPresetHelpers::GetParameterValues(const UHoudiniParameterInt* Param, TMap<FString, FHoudiniPresetIntValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Param))
	{
		return false;
	}

	const int32 NumValues = Param->GetNumberOfValues();
	FHoudiniPresetIntValues Values;
	Values.Values.SetNum(NumValues);
	for (int i = 0; i < NumValues; i++)
	{
		Param->GetValueAt(i, Values.Values[i]);
	}

	OutValues.Add(Param->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	return true;
}


bool
FHoudiniPresetHelpers::GetParameterValues(const UHoudiniParameterChoice* Param, TMap<FString, FHoudiniPresetIntValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Param))
	{
		return false;
	}
	
	FHoudiniPresetIntValues Values;
	Values.Values.Add(Param->GetIntValueIndex() );

	OutValues.Add(Param->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	return true;
}


bool
FHoudiniPresetHelpers::GetParameterValues(const UHoudiniParameterToggle* Param, TMap<FString, FHoudiniPresetIntValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Param))
	{
		return false;
	}

	const int32 NumValues = Param->GetNumValues();
	FHoudiniPresetIntValues Values;
	Values.Values.SetNum(NumValues);
	for (int i = 0; i < NumValues; i++)
	{
		 Values.Values[i] = static_cast<int>(Param->GetValueAt(i));
	}

	OutValues.Add(Param->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	return true;
}


bool
FHoudiniPresetHelpers::GetParameterValues(const UHoudiniParameterFloat* Param, TMap<FString, FHoudiniPresetFloatValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Param))
	{
		return false;
	}

	const int32 NumValues = Param->GetNumberOfValues();
	FHoudiniPresetFloatValues Values;
	Values.Values.SetNum(NumValues);
	for (int i = 0; i < NumValues; i++)
	{
		Param->GetValueAt(i, Values.Values[i]);
	}

	OutValues.Add(Param->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	return true;
}


bool
FHoudiniPresetHelpers::GetParameterValues(const UHoudiniParameterColor* Param, TMap<FString, FHoudiniPresetFloatValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Param))
	{
		return false;
	}

	FHoudiniPresetFloatValues Values;
	Values.Values.SetNum(4);
	
	const FLinearColor Color = Param->GetColorValue();
	Values.Values[0] = Color.R;
	Values.Values[1] = Color.G;
	Values.Values[2] = Color.B;
	Values.Values[3] = Color.A;
	
	OutValues.Add(Param->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	return true;
}


bool
FHoudiniPresetHelpers::GetParameterValues(const UHoudiniParameterString* Param, TMap<FString, FHoudiniPresetStringValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Param))
	{
		return false;
	}

	const int32 NumValues = Param->GetNumberOfValues();
	FHoudiniPresetStringValues Values;
	Values.Values.SetNum(NumValues);
	for (int i = 0; i < NumValues; i++)
	{
		Values.Values[i] = Param->GetValueAt(i);
	}

	OutValues.Add(Param->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	return true;
}


bool
FHoudiniPresetHelpers::GetParameterValues(const UHoudiniParameterFile* Param, TMap<FString, FHoudiniPresetStringValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Param))
	{
		return false;
	}
	
	const int32 NumValues = Param->GetNumValues();
	FHoudiniPresetStringValues Values;
	Values.Values.SetNum(NumValues);
	for (int i = 0; i < NumValues; i++)
	{
		Values.Values[i] = Param->GetValueAt(i);
	}

	OutValues.Add(Param->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	
	return true;
}


bool
FHoudiniPresetHelpers::GetParameterValues(const UHoudiniParameterChoice* Param, TMap<FString, FHoudiniPresetStringValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Param))
	{
		return false;
	}
	
	FHoudiniPresetStringValues Values;
	Values.Values.Add(Param->GetStringValue() );

	OutValues.Add(Param->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	return true;
}


bool
FHoudiniPresetHelpers::GetParameterValues(const UHoudiniParameterRampFloat* Param,
	TMap<FString, FHoudiniPresetRampFloatValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Param))
	{
		return false;
	}

	FHoudiniPresetRampFloatValues Values;
	for (const UHoudiniParameterRampFloatPoint* ParmPoint : Param->Points)
	{
		if (!IsValid(ParmPoint))
		{
			continue;
		}
		FHoudiniPresetRampFloatPoint Point;
		Point.Position = ParmPoint->Position;
		Point.Value = ParmPoint->Value;
		Point.Interpolation = ParmPoint->Interpolation;
		Values.Points.Add(Point);
	}

	OutValues.Add(Param->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	
	return true;
}


bool
FHoudiniPresetHelpers::GetParameterValues(const UHoudiniParameterRampColor* Parm,
	TMap<FString, FHoudiniPresetRampColorValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Parm))
	{
		return false;
	}

	FHoudiniPresetRampColorValues Values;
	for (const UHoudiniParameterRampColorPoint* ParmPoint : Parm->Points)
	{
		if (!IsValid(ParmPoint))
		{
			continue;
		}
		FHoudiniPresetRampColorPoint Point;
		Point.Position = ParmPoint->Position;
		Point.Value = ParmPoint->Value;
		Point.Interpolation = ParmPoint->Interpolation;
		Values.Points.Add(Point);
	}

	OutValues.Add(Parm->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	
	return true;
}

bool
FHoudiniPresetHelpers::GetParameterValues(const UHoudiniParameterMultiParm* Parm, TMap<FString, FHoudiniPresetMultiParmValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Parm))
		return false;

	FHoudiniPresetMultiParmValues Values;
	Values.Count = Parm->GetInstanceCount();

	OutValues.Add(Parm->GetParameterName(), Values);
	OutValueStr = Values.ToString();

	return true;
}

void
FHoudiniPresetHelpers::GetGenericInput(UHoudiniInput* Input,
	bool bIsParameterInput,
	const FString& ParameterName,
	TArray<FHoudiniPresetInputValue>& OutValues)
{
	FHoudiniPresetInputValue Value;
	Value.ParameterName = ParameterName;
	Value.bIsParameterInput = bIsParameterInput;
	Value.InputIndex = Input->GetInputIndex();

	switch( Input->GetInputType() )
	{
		case EHoudiniInputType::Geometry:
			UpdateFromGeometryInput(Value, Input);
			break;
		case EHoudiniInputType::Curve:
			UpdateFromCurveInput(Value, Input);
			break;
		default:
			break;
	}
	
	OutValues.Add(Value);
}

void
FHoudiniPresetHelpers::UpdateGenericInputSettings(FHoudiniPresetInputValue& Value, const UHoudiniInput* Input)
{
	Value.InputType = Input->GetInputType();

	// Copy export settings
	Value.bKeepWorldTransform = Input->GetKeepWorldTransform();
	Value.bPackGeometryBeforeMerging = Input->GetPackBeforeMerge();
	Value.bExportInputAsReference = Input->GetImportAsReference();
	Value.bExportLODs = Input->GetExportLODs();
	Value.bExportSockets = Input->GetExportSockets();
	Value.bExportColliders = Input->GetExportColliders();
	Value.bExportMaterialParameters = Input->GetExportMaterialParameters();
	Value.bMergeSplineMeshComponents = Input->IsMergeSplineMeshComponentsEnabled();
	Value.bPreferNaniteFallbackMesh = Input->GetPreferNaniteFallbackMesh();
}

void
FHoudiniPresetHelpers::UpdateFromGeometryInput(FHoudiniPresetInputValue& Value, const UHoudiniInput* Input)
{
	UpdateGenericInputSettings(Value, Input);
	
	const TArray<UHoudiniInputObject*>* InputObjects = Input->GetHoudiniInputObjectArray(EHoudiniInputType::Geometry);
	if (!InputObjects)
	{
		return;
	}

	const int32 NumObjects = InputObjects->Num();	
	for (int i = 0; i < NumObjects; i++)
	{
		const UHoudiniInputObject* InputObj = (*InputObjects)[i];
		FHoudiniPresetGeometryInputObject PresetObject;
		
		if (IsValid(InputObj))
		{
			PresetObject.InputObject = InputObj->GetObject();
			PresetObject.Transform = InputObj->GetTransform();
		}
		
		Value.GeometryInputObjects.Add(PresetObject);
	}
}

void
FHoudiniPresetHelpers::UpdateFromCurveInput(FHoudiniPresetInputValue& Value, const UHoudiniInput* Input)
{
	UpdateGenericInputSettings(Value, Input);

	const TArray<UHoudiniInputObject*>* InputObjects = Input->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
	if (!InputObjects)
	{
		return;
	}


	const int32 NumObjects = InputObjects->Num();
	Value.CurveInputObjects.SetNum(NumObjects);
	
	for (int i = 0; i < NumObjects; i++)
	{
		const UHoudiniInputHoudiniSplineComponent* InputObj = Cast<UHoudiniInputHoudiniSplineComponent>((*InputObjects)[i]);
		FHoudiniPresetCurveInputObject PresetObject;
		if (IsValid(InputObj))
		{
		 	PresetObject.Transform = InputObj->GetTransform();

			const UHoudiniSplineComponent* CurveComponent = InputObj->GetCurveComponent();
			if (IsValid(CurveComponent))
			{
				PresetObject.CurvePoints = CurveComponent->CurvePoints;
				PresetObject.bClosed = CurveComponent->IsClosedCurve();
				PresetObject.bReversed = CurveComponent->IsReversed();
				PresetObject.CurveOrder = CurveComponent->GetCurveOrder();
				PresetObject.bIsHoudiniSplineVisible = CurveComponent->IsHoudiniSplineVisible();
				PresetObject.CurveType = CurveComponent->GetCurveType();
				PresetObject.CurveMethod = CurveComponent->GetCurveMethod();
				PresetObject.CurveBreakpointParameterization = CurveComponent->GetCurveBreakpointParameterization();
				PresetObject.bIsOutputCurve = CurveComponent->bIsOutputCurve;
				PresetObject.bCookOnCurveChanged = CurveComponent->bCookOnCurveChanged;
				PresetObject.bIsLegacyInputCurve = CurveComponent->IsLegacyInputCurve();
				PresetObject.bIsInputCurve = CurveComponent->IsInputCurve();
				PresetObject.bIsEditableOutputCurve = CurveComponent->IsEditableOutputCurve();
			}
		}
		
		Value.CurveInputObjects[i] = PresetObject;
	}
}

void
FHoudiniPresetHelpers::ApplyPresetParameterValues(const FHoudiniPresetIntValues& Values, UHoudiniParameterInt* Parm)
{
	if (!IsValid(Parm))
	{
		return;
	}
	const int32 NumValues = FMath::Min(Parm->GetNumberOfValues(), Values.Values.Num());
	for (int i = 0; i < NumValues; i++)
	{
		Parm->SetValueAt(Values.Values[i], i);
		Parm->MarkChanged(true);
	}
}


void
FHoudiniPresetHelpers::ApplyPresetParameterValues(const FHoudiniPresetIntValues& Values, UHoudiniParameterChoice* Parm)
{
	if (!IsValid(Parm))
	{
		return;
	}
	
	if (Values.Values.Num() > 0)
	{
		Parm->SetIntValue( Values.Values[0] );
		Parm->MarkChanged(true);
	}
}


void
FHoudiniPresetHelpers::ApplyPresetParameterValues(const FHoudiniPresetIntValues& Values, UHoudiniParameterToggle* Parm)
{
	if (!IsValid(Parm))
	{
		return;
	}
	if (Values.Values.Num() > 0)
	{
		Parm->SetValueAt( static_cast<bool>(Values.Values[0]), 0);
		Parm->MarkChanged(true);
	}
}


void
FHoudiniPresetHelpers::ApplyPresetParameterValues(const FHoudiniPresetFloatValues& Values, UHoudiniParameterFloat* Parm)
{
	if (!IsValid(Parm))
	{
		return;
	}
	const int32 NumValues = FMath::Min(Parm->GetNumberOfValues(), Values.Values.Num());
	for (int i = 0; i < NumValues; i++)
	{
		Parm->SetValueAt(Values.Values[i], i);
		Parm->MarkChanged(true);
	}
}


void
FHoudiniPresetHelpers::ApplyPresetParameterValues(const FHoudiniPresetFloatValues& Values, UHoudiniParameterColor* Parm)
{
	if (!IsValid(Parm))
	{
		return;
	}

	FLinearColor Color(1.f, 1.f, 1.f, 1.f);
	const int32 NumValues = FMath::Min(Values.Values.Num(), 4);
	for (int i = 0; i < NumValues; i++)
	{
		switch(i)
		{
			case 0:
				Color.R = Values.Values[i];
				break;
			case 1:
				Color.G = Values.Values[i];
				break;
			case 2:
				Color.B = Values.Values[i];
				break;
			case 3:
				Color.A = Values.Values[i];
				break;
			default: ;
		}
	}
	
	Parm->SetColorValue(Color);
	Parm->MarkChanged(true);
}


void
FHoudiniPresetHelpers::ApplyPresetParameterValues(const FHoudiniPresetStringValues& Values, UHoudiniParameterString* Parm)
{
	if (!IsValid(Parm))
	{
		return;
	}
	const int32 NumValues = FMath::Min(Parm->GetNumberOfValues(), Values.Values.Num());
	for (int i = 0; i < NumValues; i++)
	{
		Parm->SetValueAt(Values.Values[i], i);
		Parm->MarkChanged(true);
	}
}


void
FHoudiniPresetHelpers::ApplyPresetParameterValues(const FHoudiniPresetStringValues& Values, UHoudiniParameterFile* Parm)
{
	if (!IsValid(Parm))
	{
		return;
	}
	const int32 NumValues = FMath::Min(Parm->GetNumValues(), Values.Values.Num());
	for (int i = 0; i < NumValues; i++)
	{
		Parm->SetValueAt(Values.Values[i], i);
		Parm->MarkChanged(true);
	}
}


void
FHoudiniPresetHelpers::ApplyPresetParameterValues(const FHoudiniPresetStringValues& Values, UHoudiniParameterChoice* Parm)
{
	if (!IsValid(Parm))
	{
		return;
	}
	
	if (Values.Values.Num() > 0)
	{
		Parm->SetStringValue( Values.Values[0] );
		Parm->MarkChanged(true);
	}
}

void
FHoudiniPresetHelpers::ApplyPresetParameterValues(const FHoudiniPresetRampFloatValues& Values,
	UHoudiniParameterRampFloat* Param)
{
	if (!IsValid(Param))
	{
		return;
	}

	// When applying a preset, we ignore the auto-update setting on the ramp parameter since we're likely going to be
	// updating a bunch of other parameters too. The HDA / Houdini Engine's cook settings will determine whether it's
	// going to cook our changes or not.

	const int32 NumPoints = Values.Points.Num();

	// Copy our preset points into the cached points array of the Ramp parameter.
	Param->SetNumCachedPoints(NumPoints);
	for (int i = 0; i < NumPoints; i++)
	{
		const FHoudiniPresetRampFloatPoint& PresetPoint = Values.Points[i];
		Param->SetCachedPointAtIndex(i, PresetPoint.Position, PresetPoint.Value, PresetPoint.Interpolation );
	}
	
	Param->MarkChanged(true);
	
	// Clear the events queue, then force a sync to ensure that we have the correct events in the queue to match our
	// cached points state.
	// The reason we do this is because the HDA cooking might be paused, or the HDA is cooking already, so the user may
	// start performing additional ramp manipulations which will start conflicting with our cached state.
	Param->ModificationEvents.Empty();
	Param->SyncCachedPoints();
	// We have already synced the cached points, so we don't need to process them on the next PreCook event.
	Param->bCaching = false;
}

void
FHoudiniPresetHelpers::ApplyPresetParameterValues(const FHoudiniPresetMultiParmValues& Values, UHoudiniParameterMultiParm* Param)
{
	if (!IsValid(Param))
	{
		return;
	}
	Param->SetNumElements(Values.Count);

	Param->MarkChanged(true);
}


void
FHoudiniPresetHelpers::ApplyPresetParameterValues(const FHoudiniPresetRampColorValues& Values, UHoudiniParameterRampColor* Param)
{
	if (!IsValid(Param))
	{
		return;
	}

	// When applying a preset, we ignore the auto-update setting on the ramp parameter since we're likely going to be
	// updating a bunch of other parameters too. The HDA / Houdini Engine's cook settings will determine whether it's
	// going to cook our changes or not.

	const int32 NumPoints = Values.Points.Num();

	// Copy our preset points into the cached points array of the Ramp parameter.
	Param->SetNumCachedPoints(NumPoints);
	for (int i = 0; i < NumPoints; i++)
	{
		const FHoudiniPresetRampColorPoint& PresetPoint = Values.Points[i];
		Param->SetCachedPointAtIndex(i, PresetPoint.Position, PresetPoint.Value, PresetPoint.Interpolation );
	}
	
	Param->MarkChanged(true);
	
	// Clear the events queue, then force a sync to ensure that we have the correct events in the queue to match our
	// cached points state.
	// The reason we do this is because the HDA cooking might be paused, or the HDA is cooking already, so the user may
	// start performing additional ramp manipulations which will start conflicting with our cached state.
	Param->ModificationEvents.Empty();
	Param->SyncCachedPoints();
	// We have already synced the cached points, so we don't need to process them on the next PreCook event.
	Param->bCaching = false;
}


void
FHoudiniPresetHelpers::ApplyPresetParameterValues(const FHoudiniPresetInputValue& PresetInput, UHoudiniInput* Input)
{
	if (!IsValid(Input))
		return;
	
	Input->Modify();
	
	// Apply Export settings
	Input->SetKeepWorldTransform( PresetInput.bKeepWorldTransform );
	Input->SetPackBeforeMerge( PresetInput.bPackGeometryBeforeMerging );
	Input->SetImportAsReference( PresetInput.bExportInputAsReference );
	Input->SetExportLODs( PresetInput.bExportLODs );
	Input->SetExportSockets( PresetInput.bExportSockets );
	Input->SetExportColliders( PresetInput.bExportColliders );
	Input->SetExportMaterialParameters( PresetInput.bExportMaterialParameters );
	Input->SetMergeSplineMeshComponents( PresetInput.bMergeSplineMeshComponents );
	Input->SetPreferNaniteFallbackMesh( PresetInput.bPreferNaniteFallbackMesh );
	
	// Overwrite this input with the type and input objects that were captured by the preset. 
	bool bBlueprintStructureModified = false;
	Input->SetInputType( PresetInput.InputType, bBlueprintStructureModified );

	switch(PresetInput.InputType)
	{
		case EHoudiniInputType::Geometry:
			ApplyPresetGeometryInput(PresetInput, Input);
			break;
		case EHoudiniInputType::Curve:
			ApplyPresetCurveInput(PresetInput, Input);
			break;
		default:
			break;
	}
	
	Input->MarkChanged(true);
}

void
FHoudiniPresetHelpers::ApplyPresetGeometryInput(const FHoudiniPresetInputValue& PresetInput, UHoudiniInput* Input)
{
	// TODO: Correctly resize geometry array. Delete excess input objects.
	
	Input->SetInputObjectsNumber( PresetInput.InputType, PresetInput.GeometryInputObjects.Num() );
	
	// Apply Geometry Input Objects
	for (int32 i = 0; i < PresetInput.GeometryInputObjects.Num(); i++)
	{
		const FHoudiniPresetGeometryInputObject& PresetObject = PresetInput.GeometryInputObjects[i];
		UObject* PresetInputObject = PresetObject.InputObject.LoadSynchronous();
		if (IsValid(PresetInputObject))
		{
			// Set the valid input object at this index.
			Input->SetInputObjectAt(PresetInput.InputType, i, PresetInputObject);
			// Apply the input object transform
			UHoudiniInputObject* InputObject = Input->GetHoudiniInputObjectAt(PresetInput.InputType, i);
			
			if (InputObject)
			{
				if (!InputObject->GetTransform().Equals(PresetObject.Transform))
				{
					InputObject->SetTransform(PresetObject.Transform);
					InputObject->MarkTransformChanged(true);
				}
			}
		}
		else
		{
			// We don't have a preset input at this index. Ensure this input object index is empty.
			const UHoudiniInputObject* InputObject = Input->GetHoudiniInputObjectAt(PresetInput.InputType, i);
			if (IsValid(InputObject))
			{
				// Delete the existing input object.
				Input->DeleteInputObjectAt(PresetInput.InputType, i, false);
			}
		}
	}
}

void
FHoudiniPresetHelpers::ApplyPresetCurveInput(const FHoudiniPresetInputValue& PresetInput, UHoudiniInput* Input)
{
	TArray<UHoudiniInputObject*>* InputObjects = Input->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
	if (!InputObjects)
	{
		return;
	}

	Input->Modify();

	const int32 NumCurves = PresetInput.CurveInputObjects.Num();
	if (InputObjects->Num() > NumCurves)
	{
		// Delete excess input objects
		for (int i = InputObjects->Num()-1; i < NumCurves; i++ )
		{
			Input->DeleteInputObjectAt(i, false);
		}
		Input->MarkChanged(true);
	}

	Input->SetInputObjectsNumber(EHoudiniInputType::Curve, NumCurves);
	
	// Apply Curve Input Objects
	for (int32 i = 0; i < PresetInput.CurveInputObjects.Num(); i++)
	{
		const FHoudiniPresetCurveInputObject& PresetObject = PresetInput.CurveInputObjects[i];
		
		UHoudiniInputHoudiniSplineComponent* InputObj = nullptr;
		
		if (InputObjects->IsValidIndex(i))
		{
			InputObj = Cast<UHoudiniInputHoudiniSplineComponent>((*InputObjects)[i]); 
		}
		
		if (!IsValid(InputObj))
		{
			bool bBlueprintStructModified = false;
			InputObj = Input->GetOrCreateCurveInputObjectAt(i ,true, bBlueprintStructModified);
		}

		if (!IsValid(InputObj))
		{
			continue;
		}

		InputObj->Modify();
		
		InputObj->SetTransform(PresetObject.Transform);
		InputObj->MarkTransformChanged(true);
		
		UHoudiniSplineComponent* SplineComponent = InputObj->GetCurveComponent();

		if (!IsValid(SplineComponent))
		{
			continue;
		}

		SplineComponent->Modify();
		
		SplineComponent->CurvePoints = PresetObject.CurvePoints;
		SplineComponent->ResetDisplayPoints();
		SplineComponent->SetClosedCurve( PresetObject.bClosed );
		SplineComponent->SetReversed( PresetObject.bReversed );
		SplineComponent->SetCurveOrder( PresetObject.CurveOrder );
		SplineComponent->SetHoudiniSplineVisible( PresetObject.bIsHoudiniSplineVisible );
		SplineComponent->SetCurveType( PresetObject.GetValidCurveType() );
		SplineComponent->SetCurveMethod( PresetObject.GetValidCurveMethod() );
		SplineComponent->SetCurveBreakpointParameterization( PresetObject.GetValidCurveBreakpointParameterization() );
		SplineComponent->bIsOutputCurve = PresetObject.bIsOutputCurve;
		SplineComponent->bCookOnCurveChanged = PresetObject.bCookOnCurveChanged;
		SplineComponent->bIsLegacyInputCurve = PresetObject.bIsLegacyInputCurve;
		SplineComponent->SetIsInputCurve( PresetObject.bIsInputCurve );
		SplineComponent->SetIsEditableOutputCurve( PresetObject.bIsEditableOutputCurve );

		InputObj->MarkChanged(true);
		SplineComponent->MarkChanged(true);
		
	}

}


UHoudiniPreset::UHoudiniPreset()
	: SourceHoudiniAsset(nullptr)
	, bRevertHDAParameters(false)
	, bHidePreset(false)
	, bApplyOnlyToSource(false)
	, bCanInstantiate(false)
{
	
}


#if WITH_EDITOR
void
UHoudiniPreset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);
	if (FModuleManager::Get().IsModuleLoaded("HoudiniEngineRuntime"))
	{
		const FHoudiniEngineRuntime& HoudiniEngineRuntime = FModuleManager::GetModuleChecked<FHoudiniEngineRuntime>("HoudiniEngineRuntime");
		HoudiniEngineRuntime.BroadcastToolOrPackageChanged();
	}
}
#endif
