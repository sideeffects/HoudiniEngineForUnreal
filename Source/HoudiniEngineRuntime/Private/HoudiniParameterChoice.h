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

#include "HoudiniParameter.h"

#include "HoudiniParameterChoice.generated.h"

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniParameterChoice : public UHoudiniParameter
{
	GENERATED_UCLASS_BODY()

	friend class FHoudiniEditorEquivalenceUtils;

public:

	// Create an instance of this class.
	static UHoudiniParameterChoice * Create(
		UObject* Outer,
		const FString& ParamName,
		const EHoudiniParameterType& ParmType);

	virtual void BeginDestroy() override;

	//------------------------------------------------------------------------------------------------
	// Accessors
	//------------------------------------------------------------------------------------------------

	const int32		GetIntValueIndex() const		{ return IntValue; };
	const FString	GetStringValue() const	{ return StringValue; };	
	const int32		GetNumChoices() const	{ return StringChoiceLabels.Num(); };
	const FString	GetLabel() const		{ return StringChoiceLabels.IsValidIndex(IntValue) ? StringChoiceLabels[IntValue] : FString(); };
	const bool		IsStringChoice() const	{ return ParmType == EHoudiniParameterType::StringChoice; };
	const int32		GetIntValue(int32 InIntValueIndex)	{ return IntValuesArray.IsValidIndex(InIntValueIndex) ? IntValuesArray[InIntValueIndex] : IntValue; }
	void			SetIntValueArray(int32 Index, int32 Value) { if (IntValuesArray.IsValidIndex(Index)) IntValuesArray[Index] = Value; }

	bool		    IsDefault() const override;

	TOptional<TSharedPtr<FString>> GetValue(int32 Idx) const;
	
	const int32		GetIntValueFromLabel(const FString& InSelectedLabel) const;	

	FString*		GetStringChoiceValueAt(const int32& InAtIndex);
	FString*		GetStringChoiceLabelAt(const int32& InAtIndex);

	bool			IsChildOfRamp() const { return bIsChildOfRamp; };
	
	// Returns the ChoiceLabel SharedPtr array, used for UI only	
	TArray<TSharedPtr<FString>>* GetChoiceLabelsPtr() { return &ChoiceLabelsPtr; };

	//------------------------------------------------------------------------------------------------
	// Mutators
	//------------------------------------------------------------------------------------------------
	bool			SetIntValue(const int32& InIntValue);
	bool			SetStringValue(const FString& InStringValue);
	void			SetNumChoices(const int32& InNumChoices);

	// For string choices only, update the int values from the string value
	bool			UpdateIntValueFromString();
	// For int choices only, update the string value from the int value
	bool			UpdateStringValueFromInt();
	// Update the pointers to the ChoiceLabels
	bool			UpdateChoiceLabelsPtr();

	void			SetDefaultIntValue() { DefaultIntValue = IntValue; };
	void			SetDefaultStringValue() { DefaultStringValue = StringValue; };

	void			SetIsChildOfRamp() { bIsChildOfRamp = true; };

	void			RevertToDefault() override;

	int32 GetIndexFromValueArray(int32 Index) const;

protected:

	// Current int value for this property.
	// More of an index to IntValuesArray
	UPROPERTY()
	int32 IntValue;

	// Default int value for this property, assigned at creating the parameter.
	UPROPERTY()
	int32 DefaultIntValue;

	// Current string value for this property
	UPROPERTY()
	FString StringValue;

	// Default string value for this property, assigned at creating the parameter.
	UPROPERTY()
	FString DefaultStringValue;

	// Used only for StringChoices!
	// All the possible string values for this parameter's choices
	UPROPERTY()
	TArray<FString> StringChoiceValues;

	// Labels corresponding to this parameter's choices.
	UPROPERTY()
	TArray<FString> StringChoiceLabels;

	// Array of SharedPtr pointing to this parameter's label, used for UI only
	TArray<TSharedPtr<FString>> ChoiceLabelsPtr;

	UPROPERTY()
	bool bIsChildOfRamp;

	// An array containing the values of all choices
	// IntValues[i] should be i unless UseMenuItemTokenAsValue is enabled.
	UPROPERTY()
	TArray<int32> IntValuesArray;
	

};
