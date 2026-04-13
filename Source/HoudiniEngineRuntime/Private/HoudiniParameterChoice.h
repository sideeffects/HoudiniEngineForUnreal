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
		EHoudiniParameterType ParmType);

	virtual void PostLoad();

	bool SetChoiceSelection(int32 InSelectionIndex);
	int GetChoiceSelection() const;

	int32 GetNumChoices() const;

	bool IsChildOfRamp() const;
	void SetIsChildOfRamp();
	bool IsStringChoice() const;

	// Accessors.
	const TArray<int>& GetIntValues() const;
	const TArray<FString>& GetStringValues() const;
	const TArray<FString>& GetLabels() const;
	const TArray<TSharedPtr<FString>>* GetSharedLabelsList() const;

	FString GetSelectedItemLabel() const;
	FString	GetSelectedValueAsString() const;

	// Functions for setting choices. This is done all at once so that the currently selected item
	// can be updated even if the current choice changed position in the array.
	void SetIntChoices(const TArray<FString>&Labels, const TArray<int>& Values);
	void SetStringChoices(const TArray<FString>& Labels, const TArray<FString>& Values);

	// Default handling.
	void SetDefaultValues();
	virtual void RevertToDefault() override;
	virtual bool IsDefault() const override;

protected:
	void MakeSharedLabelsList();

	// IntValue is badly named. It;s the currently Selected Choice Number, an Index NOT a value.
	UPROPERTY()
	int32 IntValue;

	// Default int value for this property, assigned at creating the parameter. Taken from IntValuesArray[] 
	UPROPERTY()
	int32 DefaultIntValue;

	// Default int value for this property, assigned at creating the parameter. Taken from StringChoiceValues[] 
	UPROPERTY()
	FString DefaultStringValue;

	// Used only for StringChoices!
	// All the possible string values for this parameter's choices
	UPROPERTY()
	TArray<FString> StringChoiceValues;

	// Labels corresponding to this parameter's choices.
	UPROPERTY()
	TArray<FString> StringChoiceLabels;

	UPROPERTY()
	bool bIsChildOfRamp;

	// An array containing the values of all choices
	// IntValues[i] should be i unless UseMenuItemTokenAsValue is enabled.
	UPROPERTY()
	TArray<int32> IntValuesArray;
	
	mutable TArray<TSharedPtr<FString>> SharedLabelsList;

};
