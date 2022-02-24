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

#include "HoudiniParameterFloat.generated.h"

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniParameterFloat : public UHoudiniParameter
{
	GENERATED_UCLASS_BODY()

	friend class FHoudiniEditorEquivalenceUtils;

public:

	// Create instance of this class.
	static UHoudiniParameterFloat * Create(
		UObject* Outer,
		const FString& ParamName);

	// Accessors
	FString GetUnit() const { return Unit; };
	bool GetNoSwap() const { return bNoSwap; };

	bool HasMin() const { return bHasMin; };
	bool HasMax() const { return bHasMax; };
	bool HasUIMin() const { return bHasUIMin; };
	bool HasUIMax() const { return bHasUIMax; };
	bool IsLogarithmic() const { return bIsLogarithmic; };

	float GetMin() const { return Min; };
	float GetMax() const { return Max; };
	float GetUIMin() const { return UIMin; };
	float GetUIMax() const { return UIMax; };

	bool IsChildOfRamp() const { return bIsChildOfRamp; };

	// Check if current value at Index is the default 
	bool IsDefaultValueAtIndex(const int32& Idx) const;

	bool IsDefault() const override;

	// Get value of this property
	TOptional< float > GetValue(int32 Idx) const;
	bool GetValueAt(const int32& AtIndex, float& OutValue) const;

	// Write access to the value array
	float* GetValuesPtr() { return Values.Num() > 0 ? &Values[0] : nullptr; };

	int32 GetNumberOfValues() { return Values.Num(); };

	// Mutators
	void SetHasMin(const bool& InHasMin) { bHasMin = InHasMin; };
	void SetHasMax(const bool& InHasMax) { bHasMax = InHasMax; };
	void SetHasUIMin(const bool& InHasUIMin) { bHasUIMin = InHasUIMin; };
	void SetHasUIMax(const bool& InHasUIMax) { bHasUIMax = InHasUIMax; };
	void SetIsLogarithmic(const bool& bInLog) { bIsLogarithmic = bInLog; };

	void SetMin(const float& InMin) { Min = InMin; };
	void SetMax(const float& InMax) { Max = InMax; };
	void SetUIMin(const float& InUIMin) { UIMin = InUIMin; };
	void SetUIMax(const float& InUIMax) { UIMax = InUIMax; };

	void SetUnit(const FString& InUnit) { Unit = InUnit; };
	void SetNoSwap(const bool& InNoSwap) { bNoSwap = InNoSwap; };

	void SetDefaultValues();

	void SetNumberOfValues(const uint32& InNumValues) { Values.SetNum(InNumValues); };
	bool SetValueAt(const float& InValue, const int32& AtIndex);

	void SetIsChildOfRamp() { bIsChildOfRamp = true; };

	/** Set value of this property, used by Slate. **/
	void SetValue(float InValue, int32 Idx);

	void RevertToDefault() override;
	void RevertToDefault(const int32& TupleIndex) override;

#if WITH_EDITOR
	void SwitchUniformLock() { bUniformLocked = !bUniformLocked; };
	bool IsUniformLocked() const { return bUniformLocked; };
#endif

protected:

	// Float Values
	UPROPERTY()
	TArray<float> Values;

	// Default float values, assigned at creating the parameter
	UPROPERTY()
	TArray<float> DefaultValues;

	// Unit for this property
	UPROPERTY()
	FString Unit;

	// Do we have the noswap tag?
	UPROPERTY()
	bool bNoSwap;

	// Indicates we have a min/max value
	UPROPERTY()
	bool bHasMin;
	// 
	UPROPERTY()
	bool bHasMax;

	// Indicates we have a UI min/max
	UPROPERTY()
	bool bHasUIMin;
	// 
	UPROPERTY()
	bool bHasUIMax;

	UPROPERTY()
	bool bIsLogarithmic;

	// Min and Max values for this property.
	UPROPERTY()
	float Min;
	// 
	UPROPERTY()
	float Max;

	// Min and Max values of this property for slider UI
	UPROPERTY()
	float UIMin;
	// 
	UPROPERTY()
	float UIMax;

	UPROPERTY()
	bool bIsChildOfRamp;

#if WITH_EDITORONLY_DATA
	// Indicates whether the float VEC change value uniformly
	UPROPERTY(Transient, DuplicateTransient, NonTransactional)
	bool bUniformLocked;
#endif
};