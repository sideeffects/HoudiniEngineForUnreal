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

#include "HoudiniParameterChoice.h"

UHoudiniParameterChoice::UHoudiniParameterChoice(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
	, IntValue(-1)
{
	ParmType = EHoudiniParameterType::IntChoice;
}

UHoudiniParameterChoice *
UHoudiniParameterChoice::Create( UObject* InOuter, const FString& InParamName, const EHoudiniParameterType& InParmType)
{
	FString ParamNameStr = "HoudiniParameterChoice_" + InParamName;
	FName ParamName = MakeUniqueObjectName(InOuter, UHoudiniParameterChoice::StaticClass(), *ParamNameStr);

	// We need to create a new parameter
	UHoudiniParameterChoice * HoudiniAssetParameter = NewObject< UHoudiniParameterChoice >(
		InOuter, UHoudiniParameterChoice::StaticClass(), ParamName, RF_Public | RF_Transactional);

	HoudiniAssetParameter->SetParameterType(InParmType);

	//HoudiniAssetParameter->UpdateFromParmInfo(InParentParameter, InNodeId, ParmInfo);

	return HoudiniAssetParameter;
}

void
UHoudiniParameterChoice::BeginDestroy()
{
	// We need to clean up our arrays
	for (auto& Ptr : ChoiceLabelsPtr)
	{
		Ptr.Reset();
	}
	ChoiceLabelsPtr.Empty();

	// Then the string arrays
	StringChoiceLabels.Empty();
	StringChoiceValues.Empty();

	IntValuesArray.Empty();

	Super::BeginDestroy();
}


FString*
UHoudiniParameterChoice::GetStringChoiceValueAt(const int32& InAtIndex)
{
	if (!StringChoiceValues.IsValidIndex(InAtIndex))
		return nullptr;

	return &(StringChoiceValues[InAtIndex]);
}

FString*
UHoudiniParameterChoice::GetStringChoiceLabelAt(const int32& InAtIndex)
{
	if (!StringChoiceLabels.IsValidIndex(InAtIndex))
		return nullptr;

	return &(StringChoiceLabels[InAtIndex]);
}

void
UHoudiniParameterChoice::SetNumChoices(const int32& InNumChoices)
{
	// Set the array sizes
	StringChoiceValues.SetNumZeroed(InNumChoices); 
	StringChoiceLabels.SetNumZeroed(InNumChoices);

	IntValuesArray.SetNumZeroed(InNumChoices);

	UpdateChoiceLabelsPtr();
}

// Update the pointers to the ChoiceLabels
bool 
UHoudiniParameterChoice::UpdateChoiceLabelsPtr()
{
	/*
	bool bNeedUpdate = false;
	if (StringChoiceLabels.Num() != ChoiceLabelsPtr.Num())
	{
		bNeedUpdate = true;
	}
	else
	{
		for (int32 Idx = 0; Idx < ChoiceLabelsPtr.Num(); Idx++)
		{
			if (ChoiceLabelsPtr[Idx].Get() == &(StringChoiceLabels[Idx]))
				continue;

			bNeedUpdate = true;
			break;
		}
	}

	if (!bNeedUpdate)
		return true;
	*/

	// Updates the Label Ptr array
	ChoiceLabelsPtr.SetNumZeroed(StringChoiceLabels.Num());
	for (int32 Idx = 0; Idx < ChoiceLabelsPtr.Num(); Idx++)
	{
		ChoiceLabelsPtr[Idx] = MakeShared<FString>(StringChoiceLabels[Idx]);
	}

	return true;
}

bool 
UHoudiniParameterChoice::SetIntValue(const int32& InIntValue)
{
	if (InIntValue == IntValue)
		return false;

	IntValue = InIntValue;

	return true;
}

bool
UHoudiniParameterChoice::SetStringValue(const FString& InStringValue)
{
	if (InStringValue.Equals(StringValue))
		return false;

	StringValue = InStringValue;

	return true;	
};

bool 
UHoudiniParameterChoice::UpdateIntValueFromString()
{	
	int32 FoundInt = INDEX_NONE;
	if (GetParameterType() == EHoudiniParameterType::IntChoice)
	{
		// Update the int values from the string value
		FoundInt = StringChoiceLabels.Find(StringValue);
	}
	else
	{
		// Update the int values from the string value
		FoundInt = StringChoiceValues.Find(StringValue);
	}
	
	if (FoundInt == INDEX_NONE)
		return false;

	if (IntValue == FoundInt)
		return false;

	IntValue = FoundInt;

	return true;
}

bool
UHoudiniParameterChoice::UpdateStringValueFromInt()
{
	// Update the string value from the int value
	FString NewStringValue;
	if (GetParameterType() == EHoudiniParameterType::IntChoice)
	{
		// IntChoices only have labels
		if (!StringChoiceLabels.IsValidIndex(IntValue))
			return false;

		NewStringValue = StringChoiceLabels[IntValue];
	}
	else
	{
		// StringChoices should use values
		if (!StringChoiceValues.IsValidIndex(IntValue))
			return false;

		NewStringValue = StringChoiceValues[IntValue];
	}

	if (StringValue.Equals(NewStringValue))
		return false;

	StringValue = NewStringValue;	

	return true;
}

const int32
UHoudiniParameterChoice::GetIntValueFromLabel(const FString& InSelectedLabel) const
{
	return StringChoiceLabels.Find(InSelectedLabel);
}

TOptional< TSharedPtr<FString> >
UHoudiniParameterChoice::GetValue(int32 Idx) const
{
	if (Idx == 0 && StringChoiceValues.IsValidIndex(IntValue))
	{
		return TOptional< TSharedPtr< FString > >(MakeShared<FString>(StringChoiceValues[IntValue]));
	}

	return TOptional< TSharedPtr< FString > >();
}

bool
UHoudiniParameterChoice::IsDefault() const 
{
	if (bIsChildOfRamp)
		return true;

	if (GetParameterType() == EHoudiniParameterType::IntChoice) 
	{
		return IntValue == DefaultIntValue;
	}

	if (GetParameterType() == EHoudiniParameterType::StringChoice) 
	{
		return StringValue == DefaultStringValue;
	}

	return true;
}

void 
UHoudiniParameterChoice::RevertToDefault()
{
	if (!bIsChildOfRamp)
	{
		bPendingRevertToDefault = true;
		TuplePendingRevertToDefault.Empty();
		TuplePendingRevertToDefault.Add(-1);

		MarkChanged(true);
	}
}

int32 UHoudiniParameterChoice::GetIndexFromValueArray(int32 Index) const
{
	return IntValuesArray.Find(Index);
}
