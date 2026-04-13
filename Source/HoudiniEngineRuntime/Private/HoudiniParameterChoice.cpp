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
UHoudiniParameterChoice::Create( UObject* InOuter, const FString& InParamName, EHoudiniParameterType InParmType)
{
	FString ParamNameStr = "HoudiniParameterChoice_" + InParamName;
	FName ParamName = MakeUniqueObjectName(InOuter, UHoudiniParameterChoice::StaticClass(), *ParamNameStr);

	// We need to create a new parameter
	UHoudiniParameterChoice * HoudiniAssetParameter = NewObject< UHoudiniParameterChoice >(
		InOuter, UHoudiniParameterChoice::StaticClass(), ParamName, RF_Public | RF_Transactional);

	HoudiniAssetParameter->SetParameterType(InParmType);

	return HoudiniAssetParameter;
}

int32
UHoudiniParameterChoice::GetChoiceSelection() const
{
	return IntValue;
}

FString
UHoudiniParameterChoice::GetSelectedValueAsString() const
{
	FString StringValue;
	if (GetParameterType() == EHoudiniParameterType::IntChoice)
	{
		// IntChoices only have labels
		if (!StringChoiceLabels.IsValidIndex(IntValue))
			return FString();

		StringValue = StringChoiceLabels[IntValue];
	}
	else
	{
		// StringChoices should use values
		if (!StringChoiceValues.IsValidIndex(IntValue))
			return FString();

		StringValue = StringChoiceValues[IntValue];
	}

	return StringValue;
}

int32
UHoudiniParameterChoice::GetNumChoices() const
{
	return StringChoiceLabels.Num();
}

FString
UHoudiniParameterChoice::GetSelectedItemLabel() const
{
	return StringChoiceLabels.IsValidIndex(IntValue) ? StringChoiceLabels[IntValue] : FString();
}

bool
UHoudiniParameterChoice::IsStringChoice() const
{
	return ParmType == EHoudiniParameterType::StringChoice;
}

bool
UHoudiniParameterChoice::IsChildOfRamp() const
{
	return bIsChildOfRamp;
}

void
UHoudiniParameterChoice::SetDefaultValues()
{
	if (IntValuesArray.IsValidIndex(GetChoiceSelection()))
		DefaultIntValue = IntValuesArray[GetChoiceSelection()];

	if (StringChoiceValues.IsValidIndex(GetChoiceSelection()))
		DefaultStringValue = StringChoiceValues[GetChoiceSelection()];
}

void
UHoudiniParameterChoice::SetIsChildOfRamp()
{
	bIsChildOfRamp = true;
}

bool 
UHoudiniParameterChoice::SetChoiceSelection(int32 InIntValue)
{
	if (InIntValue == INDEX_NONE && GetNumChoices() > 0)
	{
		// Should always have a valid selection if possible, so set it to first one.
		InIntValue = 0;
	}

	if (InIntValue >= GetNumChoices())
		InIntValue = 0;

	if (InIntValue == IntValue)
		return false;

	IntValue = InIntValue;

	return true;
}

bool
UHoudiniParameterChoice::IsDefault() const 
{
	if (bIsChildOfRamp)
		return true;

	int Selection = GetChoiceSelection();
	if (Selection == INDEX_NONE || Selection > GetNumChoices())
	{
		if (GetNumChoices() == 0)
			return true;
		else
			return false;
	}


	if (GetParameterType() == EHoudiniParameterType::IntChoice) 
	{

		return  IntValuesArray[Selection] == DefaultIntValue;
	}
	else if (GetParameterType() == EHoudiniParameterType::StringChoice) 
	{
		return  StringChoiceValues[Selection] == DefaultStringValue;
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

const TArray<FString>& 
UHoudiniParameterChoice::GetStringValues() const
{
	return StringChoiceValues;
}

const TArray<FString>&
UHoudiniParameterChoice::GetLabels() const
{
	return StringChoiceLabels;
}

void 
UHoudiniParameterChoice::SetIntChoices(const TArray<FString>& Labels, const TArray<int>& Values)
{
	// Get the value of the previous selection before overwriting the values.
	bool bHasPreviousSelection = this->GetIntValues().IsValidIndex(GetChoiceSelection());
	int PreviousSelectionValue = 0;

	if (bHasPreviousSelection)
	{
		PreviousSelectionValue = this->GetIntValues()[GetChoiceSelection()];
	}
	else
	{
		if (GetNumChoices() == 0)
			this->SetChoiceSelection(INDEX_NONE);
		else
			this->SetChoiceSelection(0);
	}
	
	// Overwrite values;
	this->StringChoiceLabels = Labels;
	this->IntValuesArray = Values;
	this->StringChoiceValues.SetNum(Values.Num());

	// Restore previous selection if possible. If not, set to index 0 (or invalid if no entries).
	if (bHasPreviousSelection)
	{
		SetChoiceSelection(IntValuesArray.Find(PreviousSelectionValue));
		if (this->GetChoiceSelection() == INDEX_NONE)
		{
			if (GetNumChoices() == 0)
				this->SetChoiceSelection(INDEX_NONE);
			else
				this->SetChoiceSelection(0);
		}
	}

	MakeSharedLabelsList();
}

void
UHoudiniParameterChoice::SetStringChoices(const TArray<FString>& Labels, const TArray<FString>& Values)
{
	// Get the value of the previous selection before overwriting the values.
	bool bHasPreviousSelection = this->GetStringValues().IsValidIndex(GetChoiceSelection());
	FString PreviousSelectionValue;

	if (bHasPreviousSelection)
	{
		PreviousSelectionValue = this->GetStringValues()[GetChoiceSelection()];
	}
	else
	{
		if (GetNumChoices() == 0)
			this->SetChoiceSelection(INDEX_NONE);
		else
			this->SetChoiceSelection(0);
	}

	// Overwrite values;
	this->StringChoiceLabels = Labels;
	this->IntValuesArray.SetNum(Values.Num());
	this->StringChoiceValues = Values;

	// Restore previous selection if possible. If not, set to index 0 (or invalid if no entries).
	if (bHasPreviousSelection)
	{
		SetChoiceSelection(StringChoiceValues.Find(PreviousSelectionValue));
		if (this->GetChoiceSelection() == INDEX_NONE)
		{
			if (GetNumChoices() == 0)
				this->SetChoiceSelection(INDEX_NONE);
			else
				this->SetChoiceSelection(0);
		}
	}

	MakeSharedLabelsList();
}

void
UHoudiniParameterChoice::MakeSharedLabelsList() 
{
	SharedLabelsList.Empty();

	auto& Labels = GetLabels();

	for (FString OptionLabel : GetLabels())
	{
		SharedLabelsList.Add(MakeShareable(new FString(OptionLabel)));
	}
}

const TArray<int>& 
UHoudiniParameterChoice::GetIntValues() const
{
	return IntValuesArray;
}

const TArray<TSharedPtr<FString>>* 
UHoudiniParameterChoice::GetSharedLabelsList() const
{
	return &SharedLabelsList;
}

void UHoudiniParameterChoice::PostLoad()
{
	Super::PostLoad();
	MakeSharedLabelsList();
}
