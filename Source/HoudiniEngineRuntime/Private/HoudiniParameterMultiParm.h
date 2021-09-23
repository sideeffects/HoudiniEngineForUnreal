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

#include "HoudiniParameterMultiParm.generated.h"

UENUM()
enum class EHoudiniMultiParmModificationType : uint8
{
	None,

	Inserted,
	Removed,
	Modified
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniParameterMultiParm : public UHoudiniParameter
{
	GENERATED_UCLASS_BODY()

	friend class FHoudiniEditorEquivalenceUtils;
	

public:

	// Create instance of this class.
	static UHoudiniParameterMultiParm * Create(
		UObject* Outer,
		const FString& ParamName);

	// Accessors
	FORCEINLINE
	int32 GetValue() const { return Value; };
	FORCEINLINE
	int32 GetInstanceCount() const { return MultiParmInstanceCount; };

	// Mutators
	bool SetValue(const int32& InValue);
	FORCEINLINE
	void SetInstanceCount(const int32 InCount) { MultiParmInstanceCount = InCount; };

	FORCEINLINE
	void SetIsShown(const bool InIsShown) { bIsShown = InIsShown; };

	FORCEINLINE
	bool IsShown() const { return bIsShown; };


	/** Increment value, used by Slate. **/
	void InsertElement();

	void InsertElementAt(int32 Index);

	/** Decrement value, used by Slate. **/
	void RemoveElement(int32 Index);

	/** Empty the values, used by Slate. **/
	void EmptyElements();

	/**
	 * Returns the number of multiparm instances there'll be after the next upload to HAPI, after
	 * the current state of MultiParmInstanceLastModifyArray is applied.
	 */
	int32 GetNextInstanceCount() const;

	/**
	 * Helper function to modify MultiParmInstanceLastModifyArray with inserts/removes (at the end) as necessary so
	 * that the multi parm instance count will be equal to InInstanceCount after the next upload to HAPI.
	 * @param InInstanceCount The number of instances the multiparm should have.
	 * @returns True if any changes were made to MultiParmInstanceLastModifyArray. 
	 */
	bool SetNumElements(const int32 InInstanceCount);

	UPROPERTY()
	bool bIsShown;

	// Value of the multiparm
	UPROPERTY()
	int32 Value;

	// 
	UPROPERTY()
	FString TemplateName;

	// Value of this property.
	UPROPERTY()
	int32 MultiparmValue;

	//
	UPROPERTY()
	uint32 MultiParmInstanceNum;

	//
	UPROPERTY()
	uint32 MultiParmInstanceLength;

	//
	UPROPERTY()
	uint32 MultiParmInstanceCount;

	UPROPERTY()
	uint32 InstanceStartOffset;

	// This array records the last modified instance of the multiparm
	UPROPERTY()
	TArray<EHoudiniMultiParmModificationType> MultiParmInstanceLastModifyArray;

	UPROPERTY()
	int32 DefaultInstanceCount;

	bool IsDefault() const override;

	void SetDefaultInstanceCount(int32 InCount);

private:
	void InitializeModifyArray();

};