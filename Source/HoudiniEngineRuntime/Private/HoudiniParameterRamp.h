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

#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "HoudiniParameterMultiParm.h"
#include "HoudiniParameterRamp.generated.h"

class UHoudiniParameterRampFloat;
class UHoudiniParameterRampColor;
class UHoudiniParameter;
class UHoudiniParameterFloat;
class UHoudiniParameterChoice;
class UHoudiniParameterColor;

UENUM()
enum class EHoudiniRampPointConstructStatus : uint8
{
	None,

	INITIALIZED,
	POSITION_INSERTED,
	VALUE_INSERTED,
	INTERPTYPE_INSERTED
};

UCLASS(DefaultToInstanced)
class HOUDINIENGINERUNTIME_API UHoudiniParameterRampModificationEvent : public UObject 
{
	GENERATED_BODY()
public:
	FORCEINLINE
	void SetInsertEvent() { bIsInsertEvent = true; };

	FORCEINLINE
	void SetDeleteEvent() { bIsInsertEvent = false; };

	FORCEINLINE
	void SetFloatRampEvent() { bIsFloatRamp = true; };

	FORCEINLINE
	void SetColorRampEvent() { bIsFloatRamp = false; };

	FORCEINLINE
	bool IsInsertEvent() const { return bIsInsertEvent; };

	FORCEINLINE
	bool IsDeleteEvent() const { return !bIsInsertEvent; };

	FORCEINLINE
	bool IsFloatRampEvent() { return bIsFloatRamp; };

	FORCEINLINE
	bool IsColorRampEvent() { return !bIsFloatRamp; };

	FORCEINLINE
	void SetPosition(float Position) { InsertPosition = Position; }

	FORCEINLINE
	void SetValue(float Value) { InsertFloat = Value; }

	FORCEINLINE
	void SetValue(FLinearColor Value) { InsertColor = Value; }

	FORCEINLINE
	void SetInterpolation(EHoudiniRampInterpolationType Interpolation) { InsertInterpolation = Interpolation; }

private:
	UPROPERTY()
	bool bIsInsertEvent = false;

	UPROPERTY()
	bool bIsFloatRamp = false;

public:
	UPROPERTY()
	int32 DeleteInstanceIndex = -1;

	UPROPERTY()
	float InsertPosition;

	UPROPERTY()
	float InsertFloat;

	UPROPERTY()
	FLinearColor InsertColor;

	UPROPERTY()
	EHoudiniRampInterpolationType InsertInterpolation;
};

UCLASS(DefaultToInstanced)
class HOUDINIENGINERUNTIME_API UHoudiniParameterRampFloatPoint : public UObject
{
	GENERATED_BODY()

	friend class FHoudiniEditorEquivalenceUtils;

public:

	UPROPERTY()
	float Position;

	UPROPERTY()
	float Value;

	UPROPERTY()
	EHoudiniRampInterpolationType Interpolation = EHoudiniRampInterpolationType::InValid;

	UPROPERTY()
	int32 InstanceIndex = -1;

	UPROPERTY()
	TObjectPtr<UHoudiniParameterFloat> PositionParentParm = nullptr;

	UPROPERTY()
	TObjectPtr<UHoudiniParameterFloat> ValueParentParm = nullptr;

	UPROPERTY()
	TObjectPtr<UHoudiniParameterChoice> InterpolationParentParm = nullptr;

	FORCEINLINE
	float GetPosition() const { return Position; };
	
	void SetPosition(const float InPosition);

	FORCEINLINE
	float GetValue() const { return Value; };
	
	void SetValue(const float InValue);

	FORCEINLINE
	EHoudiniRampInterpolationType GetInterpolation() const { return Interpolation; };

	void SetInterpolation(const EHoudiniRampInterpolationType InInterpolation);

	UHoudiniParameterRampFloatPoint* DuplicateAndCopyState(UObject* DestOuter, EObjectFlags ClearFlags=RF_NoFlags, EObjectFlags SetFlags=RF_NoFlags);
	
	void CopyStateFrom(UHoudiniParameterRampFloatPoint* InParameter, bool bCopyAllProperties, EObjectFlags ClearFlags=RF_NoFlags, EObjectFlags SetFlags=RF_NoFlags);

	void RemapParameters(const TMap<UHoudiniParameter*, UHoudiniParameter*>& ParameterMapping);
		
};

UCLASS(DefaultToInstanced)
class HOUDINIENGINERUNTIME_API UHoudiniParameterRampColorPoint : public UObject
{
	GENERATED_BODY()

	friend class FHoudiniEditorEquivalenceUtils;

public:

	UPROPERTY()
	float Position;

	UPROPERTY()
	FLinearColor Value;

	UPROPERTY()
	EHoudiniRampInterpolationType Interpolation = EHoudiniRampInterpolationType::InValid;

	UPROPERTY()
	int32 InstanceIndex = -1;

	UPROPERTY()
	TObjectPtr<UHoudiniParameterFloat>  PositionParentParm = nullptr;

	UPROPERTY()
	TObjectPtr<UHoudiniParameterColor> ValueParentParm = nullptr;

	UPROPERTY()
	TObjectPtr<UHoudiniParameterChoice> InterpolationParentParm = nullptr;

	FORCEINLINE
	float GetPosition() const { return Position; };

	void SetPosition(const float InPosition);
	FORCEINLINE
	FLinearColor GetValue() const { return Value; };

	void SetValue(const FLinearColor InValue);

	FORCEINLINE
	EHoudiniRampInterpolationType GetInterpolation() const { return Interpolation; };

	void SetInterpolation(const EHoudiniRampInterpolationType InInterpolation);

	UHoudiniParameterRampColorPoint* DuplicateAndCopyState(UObject* DestOuter, EObjectFlags InClearFlags=RF_NoFlags, EObjectFlags InSetFlags=RF_NoFlags);
	
	void CopyStateFrom(UHoudiniParameterRampColorPoint* InParameter, bool bCopyAllProperties, EObjectFlags InClearFlags=RF_NoFlags, EObjectFlags InSetFlags=RF_NoFlags);

	void RemapParameters(const TMap<UHoudiniParameter*, UHoudiniParameter*>& ParameterMapping);
};

/**
 * A note on Houdini Ramp Parameter processes
 *
 * The Ramp Parameters changes are applied through HAPI by way of Modification Events.
 * The FHoudiniParameterTranslator::UploadRampParameter method will process all the pending modification events on a
 * ramp parameter and execute the desired HAPI calls to send these modifications through to Houdini. When the HDA
 * has finished cooking, the values from the ramp parameters on the Houdini side will be copied back onto the HDA in
 * Unreal (the `points` member of the respective ramp parameter will be overwritten).
 *
 * It is for this reason that we cannot simply store our desired state in the `Points` member; anything stored here will
 * get clobbered after the HDA has been cooked. So we record any modifications as events instead, and apply those
 * modification events on the next cook.
 *
 * We can store our desired state in the CachedPoints member, as is the case when auto-update is disabled for a
 * Ramp parameter or HoudiniEngine cooking is disabled. Once the HDA is ready to cook, the data in CachedPoints is
 * "synced" (SyncCachedPoints). This will generate Modification Events based on the differences in data between
 * `Points` and `CachedPoints`. Once the modification events have been generated, the HDA can be cooked as usual.
 */


/**
 * Houdini Ramp Parameter (float)
 */
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniParameterRampFloat : public UHoudiniParameterMultiParm
{
	GENERATED_UCLASS_BODY()

public:

	virtual void OnPreCook() override;

	// Create instance of this class.
	static UHoudiniParameterRampFloat * Create(
		UObject* Outer,
		const FString& ParamName);

	FORCEINLINE
	bool IsCaching() const { return bCaching; };

	FORCEINLINE
	void SetCaching(const bool bInCaching) { bCaching = bInCaching; };

	virtual void CopyStateFrom(UHoudiniParameter* InParameter, bool bCopyAllProperties, EObjectFlags InClearFlags=RF_NoFlags, EObjectFlags InSetFlags=RF_NoFlags) override;

	virtual void RemapParameters(const TMap<UHoudiniParameter*, UHoudiniParameter*>& ParameterMapping) override;

	void SyncCachedPoints();

	void CreateInsertEvent(const float& InPosition, const float& InValue, const EHoudiniRampInterpolationType &InInterp);

	void CreateDeleteEvent(const int32 &InDeleteIndex);

	// Resize the number of cached points contained in this parameter 
	void SetNumCachedPoints(const int32 NumPoints);

	// Set the cached point at the current index. If no point object exist at the current index, create it. 
	bool SetCachedPointAtIndex(const int32 InIndex, const float InPosition, const float InValue, const EHoudiniRampInterpolationType InInterpolation);

	/**
	 * Update/populates the Points array from InParameters.
	 * @param InParameters An array of parameters containing this ramp multiparm's instances (the parameters for each
	 * of its points).
	 * @param InStartParamIndex The index in InParameters where this ramp multiparm's child parameters start.
	 * @return true if we found enough parameters to build a number of points == NumInstances().
	 */
	bool UpdatePointsArray(const TArray<UHoudiniParameter*>& InParameters, const int32 InStartParamIndex);
	
	UPROPERTY()
	TArray<TObjectPtr<UHoudiniParameterRampFloatPoint>> Points;

	UPROPERTY()
	TArray<TObjectPtr<UHoudiniParameterRampFloatPoint>> CachedPoints;

	UPROPERTY()
	TArray<float> DefaultPositions;

	UPROPERTY()
	TArray<float> DefaultValues;

	UPROPERTY()
	TArray<int32> DefaultChoices;

	UPROPERTY()
	int32 NumDefaultPoints;

	// If this is true, the cached points will be synced prior to cooking.
	UPROPERTY()
	bool bCaching;

	UPROPERTY()
	TArray<TObjectPtr<UHoudiniParameterRampModificationEvent>> ModificationEvents;

	bool IsDefault() const override;

	void SetDefaultValues();

};


/**
 * Houdini Ramp Parameter (FLinearColor)
 */
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniParameterRampColor : public UHoudiniParameterMultiParm
{
	GENERATED_UCLASS_BODY()

	friend class FHoudiniEditorEquivalenceUtils;

public:
	
	virtual void OnPreCook() override;

	// Create instance of this class.
	static UHoudiniParameterRampColor * Create(
		UObject* Outer,
		const FString& ParamName);

	FORCEINLINE
	bool IsCaching() const { return bCaching; };

	FORCEINLINE
	void SetCaching(const bool bInCaching) { bCaching = bInCaching; };

	virtual void CopyStateFrom(UHoudiniParameter* InParameter, bool bCopyAllProperties, EObjectFlags InClearFlags=RF_NoFlags, EObjectFlags InSetFlags=RF_NoFlags) override;

	virtual void RemapParameters(const TMap<UHoudiniParameter*, UHoudiniParameter*>& ParameterMapping) override;

	/**
	 * Update/populates the Points array from InParameters.
	 * @param InParameters An array of parameters containing this ramp multiparm's instances (the parameters for each
	 * of its points).
	 * @param InStartParamIndex The index in InParameters where this ramp multiparm's child parameters start.
	 * @return true if we found enough parameters to build a number of points == NumInstances().
	 */
	bool UpdatePointsArray(const TArray<UHoudiniParameter*>& InParameters, const int32 InStartParamIndex);

	void SyncCachedPoints();

	void CreateInsertEvent(const float& InPosition, const FLinearColor& InValue, const EHoudiniRampInterpolationType &InInterp);

	void CreateDeleteEvent(const int32 &InDeleteIndex);

	// Resize the number of cached points contained in this parameter 
	void SetNumCachedPoints(const int32 NumPoints);

	// Set the cached point at the current index. If no point object exist at the current index, create it. 
	bool SetCachedPointAtIndex(const int32 InIndex, const float InPosition, const FLinearColor& InValue, const EHoudiniRampInterpolationType InInterpolation);

	UPROPERTY(Instanced)
	TArray<TObjectPtr<UHoudiniParameterRampColorPoint>> Points;

	UPROPERTY()
	bool bCaching;

	UPROPERTY(Instanced)
	TArray<TObjectPtr<UHoudiniParameterRampColorPoint>> CachedPoints;

	UPROPERTY()
	TArray<float> DefaultPositions;

	UPROPERTY()
	TArray<FLinearColor> DefaultValues;

	UPROPERTY()
	TArray<int32> DefaultChoices;

	UPROPERTY()
	int32 NumDefaultPoints;

	UPROPERTY()
	TArray<TObjectPtr<UHoudiniParameterRampModificationEvent>> ModificationEvents;

	bool IsDefault() const override;

	void SetDefaultValues();

};
