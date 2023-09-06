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


bool FHoudiniPresetHelpers::IsSupportedInputType(const EHoudiniInputType InputType)
{
	if (InputType == EHoudiniInputType::Geometry)
	{
		return true;
	}

	return false;
}

void
FHoudiniPresetHelpers::PopulateFromParameter(UHoudiniParameterFloat* Parm, FHoudiniPresetFloatValues& Value)
{
	const int32 NumValues = Parm->GetNumberOfValues();
	Value.Values.SetNum(NumValues);
	for (int i = 0; i < NumValues; i++)
	{
		Parm->GetValueAt(i, Value.Values[i]);
	}
}


void
FHoudiniPresetHelpers::PopulateFromParameter(UHoudiniParameterInt* Parm, FHoudiniPresetIntValues& Value)
{
	const int32 NumValues = Parm->GetNumberOfValues();
	Value.Values.SetNum(NumValues);
	for (int i = 0; i < NumValues; i++)
	{
		Parm->GetValueAt(i, Value.Values[i]);
	}
}


void
FHoudiniPresetHelpers::PopulateFromParameter(UHoudiniParameterString* Parm, FHoudiniPresetStringValues& Value)
{
	const int32 NumValues = Parm->GetNumberOfValues();
	Value.Values.SetNum(NumValues);
	for (int i = 0; i < NumValues; i++)
	{
		Value.Values[i] = Parm->GetValueAt(i);
	}
}


bool
FHoudiniPresetHelpers::IngestParameter(UHoudiniParameterInt* Parm,
	TMap<FString, FHoudiniPresetIntValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Parm))
	{
		return false;
	}

	const int32 NumValues = Parm->GetNumberOfValues();
	FHoudiniPresetIntValues Values;
	Values.Values.SetNum(NumValues);
	for (int i = 0; i < NumValues; i++)
	{
		Parm->GetValueAt(i, Values.Values[i]);
	}

	OutValues.Add(Parm->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	return true;
}


bool
FHoudiniPresetHelpers::IngestParameter(UHoudiniParameterChoice* Parm,
	TMap<FString, FHoudiniPresetIntValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Parm))
	{
		return false;
	}
	
	FHoudiniPresetIntValues Values;
	Values.Values.Add( Parm->GetIntValueIndex() );

	OutValues.Add(Parm->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	return true;
}


bool
FHoudiniPresetHelpers::IngestParameter(UHoudiniParameterToggle* Parm,
	TMap<FString, FHoudiniPresetIntValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Parm))
	{
		return false;
	}

	const int32 NumValues = Parm->GetNumValues();
	FHoudiniPresetIntValues Values;
	Values.Values.SetNum(NumValues);
	for (int i = 0; i < NumValues; i++)
	{
		 Values.Values[i] = static_cast<int>(Parm->GetValueAt(i));
	}

	OutValues.Add(Parm->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	return true;
}


bool
FHoudiniPresetHelpers::IngestParameter(UHoudiniParameterFloat* Parm,
	TMap<FString, FHoudiniPresetFloatValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Parm))
	{
		return false;
	}

	const int32 NumValues = Parm->GetNumberOfValues();
	FHoudiniPresetFloatValues Values;
	Values.Values.SetNum(NumValues);
	for (int i = 0; i < NumValues; i++)
	{
		Parm->GetValueAt(i, Values.Values[i]);
	}

	OutValues.Add(Parm->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	return true;
}


bool
FHoudiniPresetHelpers::IngestParameter(UHoudiniParameterColor* Parm,
	TMap<FString, FHoudiniPresetFloatValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Parm))
	{
		return false;
	}

	FHoudiniPresetFloatValues Values;
	Values.Values.SetNum(4);
	
	const FLinearColor Color = Parm->GetColorValue();
	Values.Values[0] = Color.R;
	Values.Values[1] = Color.G;
	Values.Values[2] = Color.B;
	Values.Values[3] = Color.A;
	
	OutValues.Add(Parm->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	return true;
}


bool
FHoudiniPresetHelpers::IngestParameter(UHoudiniParameterString* Parm,
	TMap<FString, FHoudiniPresetStringValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Parm))
	{
		return false;
	}

	const int32 NumValues = Parm->GetNumberOfValues();
	FHoudiniPresetStringValues Values;
	Values.Values.SetNum(NumValues);
	for (int i = 0; i < NumValues; i++)
	{
		Values.Values[i] = Parm->GetValueAt(i);
	}

	OutValues.Add(Parm->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	return true;
}


bool
FHoudiniPresetHelpers::IngestParameter(UHoudiniParameterFile* Parm,
	TMap<FString, FHoudiniPresetStringValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Parm))
	{
		return false;
	}
	
	const int32 NumValues = Parm->GetNumValues();
	FHoudiniPresetStringValues Values;
	Values.Values.SetNum(NumValues);
	for (int i = 0; i < NumValues; i++)
	{
		Values.Values[i] = Parm->GetValueAt(i);
	}

	OutValues.Add(Parm->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	
	return true;
}


bool
FHoudiniPresetHelpers::IngestParameter(UHoudiniParameterChoice* Parm,
	TMap<FString, FHoudiniPresetStringValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Parm))
	{
		return false;
	}
	
	FHoudiniPresetStringValues Values;
	Values.Values.Add( Parm->GetStringValue() );

	OutValues.Add(Parm->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	return true;
}


bool
FHoudiniPresetHelpers::IngestParameter(UHoudiniParameterRampFloat* Parm,
	TMap<FString, FHoudiniPresetRampFloatValues>& OutValues, FString& OutValueStr)
{
	if (!IsValid(Parm))
	{
		return false;
	}

	FHoudiniPresetRampFloatValues Values;
	for (const UHoudiniParameterRampFloatPoint* ParmPoint : Parm->Points)
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

	OutValues.Add(Parm->GetParameterName(), Values);
	OutValueStr = Values.ToString();
	
	return true;
}


bool
FHoudiniPresetHelpers::IngestParameter(UHoudiniParameterRampColor* Parm,
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


void
FHoudiniPresetHelpers::IngestGenericInput(
	UHoudiniInput* Input,
	bool bIsParameterInput,
	const FString& ParameterName,
	TArray<FHoudiniPresetInputValue>& OutValues,
	FString& OutValueStr)
{
	FHoudiniPresetInputValue Value;
	Value.ParameterName = ParameterName;
	Value.bIsParameterInput = bIsParameterInput;
	Value.InputIndex = Input->GetInputIndex();

	UpdateFromInput(Value, Input);
	
	OutValues.Add(Value);
}

void FHoudiniPresetHelpers::UpdateFromInput(FHoudiniPresetInputValue& Value, UHoudiniInput* Input)
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

	const TArray<UHoudiniInputObject*>* InputObjects = Input->GetHoudiniInputObjectArray(EHoudiniInputType::Geometry);
	if (!InputObjects)
	{
		return;
	}

	const int32 NumObjects = InputObjects->Num();	
	for (int i = 0; i < NumObjects; i++)
	{
		const UHoudiniInputObject* InputObj = (*InputObjects)[i];
		FHoudiniPresetInputObject PresetObject;
		
		if (IsValid(InputObj))
		{
			PresetObject.InputObject = InputObj->GetObject();
			PresetObject.Transform = InputObj->Transform;
		}
		
		Value.InputObjects.Add(PresetObject);
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

void FHoudiniPresetHelpers::ApplyPresetParameterValues(const FHoudiniPresetRampFloatValues& Values,
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

void FHoudiniPresetHelpers::ApplyPresetParameterValues(const FHoudiniPresetRampColorValues& Values, UHoudiniParameterRampColor* Param)
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
	Input->SetInputObjectsNumber( PresetInput.InputType, PresetInput.InputObjects.Num() );

	for (int32 i = 0; i < PresetInput.InputObjects.Num(); i++)
	{
		const FHoudiniPresetInputObject& PresetObject = PresetInput.InputObjects[i];
		UObject* PresetInputObject = PresetObject.InputObject.LoadSynchronous();
		if (IsValid(PresetInputObject))
		{
			// Set the valid input object at this index.
			Input->SetInputObjectAt(PresetInput.InputType, i, PresetInputObject);
			// Apply the input object transform
			UHoudiniInputObject* InputObject = Input->GetHoudiniInputObjectAt(PresetInput.InputType, i);
			
			if (InputObject)
			{
				if (!InputObject->Transform.Equals(PresetObject.Transform))
				{
					InputObject->Transform = PresetObject.Transform;
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
	
	Input->MarkChanged(true);
}


UHoudiniPreset::UHoudiniPreset()
	: SourceHoudiniAsset(nullptr)
	, bRevertHDAParameters(false)
	, bApplyOnlyToSource(false)
	, bCanInstantiate(false)
{
	
}
