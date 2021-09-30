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

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameFramework/Actor.h"

#include "HoudiniPublicAPIObjectBase.h"
#include "HoudiniPublicAPI.h"
#include "HoudiniPublicAPIOutputTypes.h"
#include "HoudiniEngineRuntimeCommon.h"

#include "HoudiniPublicAPIAssetWrapper.generated.h"


class UHoudiniPublicAPIInput;
class UHoudiniOutput;
class UHoudiniParameter;
class UHoudiniInput;
class UTOPNode;
class UHoudiniAssetComponent;
class AHoudiniAssetActor;

/**
 * The base class of a struct for Houdini Ramp points.
 */
USTRUCT(BlueprintType, Category="Houdini Engine | Public API")
struct HOUDINIENGINEEDITOR_API FHoudiniPublicAPIRampPoint
{
	GENERATED_BODY();

public:
	FHoudiniPublicAPIRampPoint();

	FHoudiniPublicAPIRampPoint(
		const float InPosition,
		const EHoudiniPublicAPIRampInterpolationType InInterpolation=EHoudiniPublicAPIRampInterpolationType::LINEAR);

	/** The position of the point on the Ramp's x-axis [0,1]. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API")
	float Position;

	/** The interpolation type of the point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API")
	EHoudiniPublicAPIRampInterpolationType Interpolation;
};

/**
 * A struct for Houdini float ramp points.
 */
USTRUCT(BlueprintType, Category="Houdini Engine | Public API")
struct HOUDINIENGINEEDITOR_API FHoudiniPublicAPIFloatRampPoint : public FHoudiniPublicAPIRampPoint
{
	GENERATED_BODY();

public:

	FHoudiniPublicAPIFloatRampPoint();

	FHoudiniPublicAPIFloatRampPoint(
		const float InPosition,
		const float InValue,
		const EHoudiniPublicAPIRampInterpolationType InInterpolation=EHoudiniPublicAPIRampInterpolationType::LINEAR);

	/** The value of the point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API")
	float Value;

};

/**
 * A struct for Houdini color ramp points.
 */
USTRUCT(BlueprintType, Category="Houdini Engine | Public API")
struct HOUDINIENGINEEDITOR_API FHoudiniPublicAPIColorRampPoint : public FHoudiniPublicAPIRampPoint
{
	GENERATED_BODY();

public:

	FHoudiniPublicAPIColorRampPoint();

	FHoudiniPublicAPIColorRampPoint(
		const float InPosition,
		const FLinearColor& InValue,
		const EHoudiniPublicAPIRampInterpolationType InInterpolation=EHoudiniPublicAPIRampInterpolationType::LINEAR);

	/** The value of the point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API")
	FLinearColor Value;

};

/**
 * A struct for storing the values of a Houdini parameter tuple.
 * Currently supports bool, int32, float and FString storage.
 */
USTRUCT(BlueprintType, Category="Houdini Engine | Public API")
struct HOUDINIENGINEEDITOR_API FHoudiniParameterTuple
{
	GENERATED_BODY();

public:
	FHoudiniParameterTuple();

	/**
	 * Wrap a single bool value.
	 * @param InValue The value to wrap.
	 */
	FHoudiniParameterTuple(const bool& InValue);
	/**
	 * Wrap a bool tuple.
	 * @param InValues The tuple to wrap.
	 */
	FHoudiniParameterTuple(const TArray<bool>& InValues);

	/**
	 * Wrap a single int32 value.
	 * @param InValue The value to wrap. 
	 */
	FHoudiniParameterTuple(const int32& InValue);
	/**
	 * Wrap a int32 tuple.
	 * @param InValues The tuple to wrap.
	 */
	FHoudiniParameterTuple(const TArray<int32>& InValues);
	
	/**
	 * Wrap a single float value.
	 * @param InValue The value to wrap.
	 */
	FHoudiniParameterTuple(const float& InValue);
	/**
	 * Wrap a float tuple.
	 * @param InValues The tuple to wrap.
	 */
	FHoudiniParameterTuple(const TArray<float>& InValues);

	/**
	 * Wrap a single FString value.
	 * @param InValue The value to wrap.
	 */
	FHoudiniParameterTuple(const FString& InValue);
	/**
	 * Wrap a FString tuple.
	 * @param InValues The tuple to wrap.
	 */
	FHoudiniParameterTuple(const TArray<FString>& InValues);

	/**
	 * Wrap float ramp points
	 * @param InRampPoints The float ramp points.
	 */
	FHoudiniParameterTuple(const TArray<FHoudiniPublicAPIFloatRampPoint>& InRampPoints);

	/**
	 * Wrap color ramp points
	 * @param InRampPoints The color ramp points.
	 */
	FHoudiniParameterTuple(const TArray<FHoudiniPublicAPIColorRampPoint>& InRampPoints);

	// Parameter tuple storage

	/** For bool compatible parameters, the bool parameter tuple values. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API")
	TArray<bool> BoolValues;

	/** For int32 compatible parameters, the int32 parameter tuple values. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API")
	TArray<int32> Int32Values;

	/** For float compatible parameters, the float parameter tuple values. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API")
	TArray<float> FloatValues;

	/** For string compatible parameters, the string parameter tuple values. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API")
	TArray<FString> StringValues;

	/** For float ramp parameters, the ramp points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API")
	TArray<FHoudiniPublicAPIFloatRampPoint> FloatRampPoints;

	/** For color ramp parameters, the ramp points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API")
	TArray<FHoudiniPublicAPIColorRampPoint> ColorRampPoints;
};

/**
 * A wrapper for spawned/instantiating HDAs.
 *
 * The wrapper/HDA should be instantiated via UHoudiniPublicAPI::InstantiateAsset(). Alternatively an empty
 * wrapper can be created via UHoudiniPublicAPIAssetWrapper::CreateEmptyWrapper() and an HDA later instantiated and
 * assigned to the wrapper via UHoudiniPublicAPI::InstantiateAssetWithExistingWrapper(). 
 *
 * The wrapper provides functionality for interacting/manipulating a AHoudiniAssetActor / UHoudiniAssetComponent:
 *		- Get/Set Inputs
 *		- Get/Set Parameters
 *		- Manually initiate a cook/recook
 *		- Subscribe to delegates:
 *			- #OnPreInstantiationDelegate (good place to set parameter values before the first cook)
 *			- #OnPostInstantiationDelegate (good place to set/configure inputs before the first cook)
 *			- #OnPostCookDelegate
 *			- #OnPreProcessStateExitedDelegate
 *			- #OnPostProcessingDelegate (output objects are available if the cook was successful) 
 *			- #OnPostBakeDelegate
 *			- #OnPostPDGTOPNetworkCookDelegate
 *			- #OnPostPDGBakeDelegate
 *			- #OnProxyMeshesRefinedDelegate
 *		- Iterate over outputs and find the output assets
 *		- Bake outputs
 *		- PDG: Dirty all, cook outputs
 *
 * Important: In the current implementation of the plugin, nodes are cooked asynchronously. That means that cooking
 * (including rebuilding the HDA and auto-cooks triggered from, for example, parameter changes) does not happen
 * immediately. Functions in the API, such as Recook() and Rebuild(), do not block until the cook is complete, but
 * instead immediately return after arranging for the cook to take place. This means that if a cook is triggered
 * (either automatically, via parameter changes, or by calling Recook()) and there is a reliance on data that will only
 * be available after the cook (such as an updated parameter interface, or the output objects of the cook), one of the
 * delegates mentioned above (#OnPostProcessingDelegate or #OnPostCookDelegate, for example) would have to be used to
 * execute the dependent code after the cook.
 */
UCLASS(BlueprintType, Blueprintable, Category="Houdini Engine|Public API")
class HOUDINIENGINEEDITOR_API UHoudiniPublicAPIAssetWrapper : public UHoudiniPublicAPIObjectBase
{
	GENERATED_BODY()
	
public:
	UHoudiniPublicAPIAssetWrapper();

	// Delegate types
	
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHoudiniAssetStateChange, UHoudiniPublicAPIAssetWrapper*, InAssetWrapper);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHoudiniAssetPostCook, UHoudiniPublicAPIAssetWrapper*, InAssetWrapper, const bool, bInCookSuccess);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHoudiniAssetPostBake, UHoudiniPublicAPIAssetWrapper*, InAssetWrapper, const bool, bInBakeSuccess);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHoudiniAssetProxyMeshesRefinedDelegate, UHoudiniPublicAPIAssetWrapper* const, InAssetWrapper, const EHoudiniProxyRefineResult, InResult);

	/**
	 * Factory function for creating new wrapper instances around instantiated assets.
	 * @param InOuter The outer for the new wrapper instance.
	 * @param InHoudiniAssetActorOrComponent The AHoudiniAssetActor or UHoudiniAssetComponent to wrap.
	 * @return The newly instantiated wrapper that wraps InHoudiniAssetActor, or nullptr if the wrapper could not
	 * be created, or if InHoudiniAssetActorOrComponent is invalid or not of a supported type.
	 */
	UFUNCTION(BlueprintCallable, Category="Houdini|Public API")
	static UHoudiniPublicAPIAssetWrapper* CreateWrapper(UObject* InOuter, UObject* InHoudiniAssetActorOrComponent);

	/**
	 * Factory function for creating a new empty wrapper instance.
	 * An instantiated actor can be wrapped using SetHoudiniAssetActor.
	 * @param InOuter the outer for the new wrapper instance.
	 * @return The newly instantiated wrapper.
	 */
	UFUNCTION(BlueprintCallable, Category="Houdini|Public API")
	static UHoudiniPublicAPIAssetWrapper* CreateEmptyWrapper(UObject* InOuter);

	/**
	 * Checks if InObject can be wrapped by instances of UHoudiniPublicAPIAssetWrapper.
	 * @param InObject The object to check for compatiblity.
	 * @return true if InObject can be wrapped by instances of UHoudiniPublicAPIAssetWrapper.
	 */
	UFUNCTION(BlueprintCallable, Category="Houdini|Public API")
	static bool CanWrapHoudiniObject(UObject* InObject);

	/**
	 * Wrap the specified instantiated houdini asset object. Supported objects are: AHoudiniAssetActor,
	 * UHoudiniAssetComponent. This will first unwrap/unbind the currently wrapped instantiated
	 * asset if InHoudiniAssetObjectToWrap is valid and of a supported class.
	 *
	 * If InHoudiniAssetObjectToWrap is nullptr, then this wrapper will unwrap/unbind the currently wrapped
	 * instantiated asset and return true.
	 * 
	 * @param InHoudiniAssetObjectToWrap The object to wrap, or nullptr to unwrap/unbind if currently wrapping an
	 * asset.
	 * @return true if InHoudiniAssetObjectToWrap is valid, of a supported class and was successfully wrapped, or true
	 * if InHoudiniAssetObjectToWrap is nullptr.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool WrapHoudiniAssetObject(UObject* InHoudiniAssetObjectToWrap);

	// Accessors and mutators

	/**
	 * Get the wrapped instantiated houdini asset object.
	 * @return The wrapped instantiated houdini asset object.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	UObject* GetHoudiniAssetObject() const;
	FORCEINLINE
	virtual UObject* GetHoudiniAssetObject_Implementation() const { return HoudiniAssetObject.Get(); }
	
	/**
	 * Helper function for getting the instantiated HDA asset actor, if HoudiniAssetObject is an AHoudiniAssetActor or
	 * a UHoudiniAssetComponent owned by a AHoudiniAssetActor.
	 * @return The instantiated AHoudiniAssetActor, if HoudiniAssetObject is an AHoudiniAssetActor or
	 * a UHoudiniAssetComponent owned by a AHoudiniAssetActor, otherwise nullptr. 
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	AHoudiniAssetActor* GetHoudiniAssetActor() const;

	/**
	 * Helper function for getting the UHoudiniAssetComponent of the HDA, if HoudiniAssetObject is a
	 * UHoudiniAssetComponent or an AHoudiniAssetActor.
	 * @return The instantiated AHoudiniAssetActor, if HoudiniAssetObject is a
	 * UHoudiniAssetComponent or an AHoudiniAssetActor, otherwise nullptr. 
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	UHoudiniAssetComponent* GetHoudiniAssetComponent() const;

	/**
	 * Get the Temp Folder fallback as configured on asset details panel
	 * @param OutDirectoryPath The currently configured fallback temporary cook folder.
	 * @return true if the wrapper is valid and the value was fetched.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetTemporaryCookFolder(FDirectoryPath& OutDirectoryPath) const;

	/**
	 * Set the Temp Folder fallback as configured on asset details panel. Returns true if the value was changed.
	 * @param InDirectoryPath The new temp folder fallback to set.
	 * @return true if the wrapper is valid and the value was set.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetTemporaryCookFolder(const FDirectoryPath& InDirectoryPath) const;

	/**
	 * Get the Bake Folder fallback as configured on asset details panel.
	 * @param OutDirectoryPath The current bake folder fallback.
	 * @return true if the wrapper is valid and the value was fetched.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetBakeFolder(FDirectoryPath& OutDirectoryPath) const;

	/**
	 * Set the Bake Folder fallback as configured on asset details panel. Returns true if the value was changed.
	 * @param InDirectoryPath The new bake folder fallback.
	 * @return true if the wrapper is valid and the value was set.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetBakeFolder(const FDirectoryPath& InDirectoryPath) const;

	// Houdini Engine Actions

	/** Delete the instantiated asset from its level and mark the wrapper for destruction. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool DeleteInstantiatedAsset();

	/**
	 * Marks the HDA as needing to be rebuilt in Houdini Engine and immediately returns. The rebuild happens
	 * asynchronously. If you need to take action after the rebuild and cook completes, one of the wrapper's delegates
	 * can be used, such as: OnPostCookDelegate or OnPostProcessingDelegate.
	 * 
	 * @returns true If the HDA was successfully marked as needing to be rebuilt.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool Rebuild();

	// Cooking

	/**
	 * Marks the HDA as needing to be cooked and immediately returns. The cook happens asynchronously. If you need
	 * to take action after the cook completes, one of the wrapper's delegates can be used, such as:
	 * OnPostCookDelegate or OnPostProcessingDelegate.
	 * 
	 * @returns true If the HDA was successfully marked as needing to be cooked.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool Recook();

	/**
	 * Enable or disable auto cooking of the asset (on parameter changes, input updates and transform changes, for
	 * example)
	 * @param bInSetEnabled Whether or not enable auto cooking.
	 * @return true if the value was changed.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetAutoCookingEnabled(const bool bInSetEnabled);

	/** Returns true if auto cooking is enabled for this instantiated asset. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool IsAutoCookingEnabled() const;

	// Baking

	/**
	 * Bake all outputs of the instantiated asset using the settings configured on the asset.
	 * @return true if the wrapper is valid and the baking process was started.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool BakeAllOutputs();

	/**
	 * Bake all outputs of the instantiated asset using the specified settings.
	 * @param InBakeOption The bake method/target, (to actor vs blueprint, for example).
	 * @param bInReplacePreviousBake If true, replace the previous bake output (assets + actor) with the
	 * new results.
	 * @param bInRemoveTempOutputsOnSuccess If true, the temporary outputs of the wrapper asset are removed
	 * after a successful bake.
	 * @param bInRecenterBakedActors If true, recenter the baked actors to their bounding box center after the bake.
	 * @return true if the wrapper is valid and the baking process was started.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool BakeAllOutputsWithSettings(
		EHoudiniEngineBakeOption InBakeOption,
		bool bInReplacePreviousBake=false,
		bool bInRemoveTempOutputsOnSuccess=false,
		bool bInRecenterBakedActors=false);

	/**
	 * Set whether to automatically bake all outputs after a successful cook.
	 * @return false if the asset/wrapper is invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetAutoBakeEnabled(const bool bInAutoBakeEnabled);

	/** Returns true if auto bake is enabled. See SetAutoBakeEnabled. */ 
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool IsAutoBakeEnabled() const;

	/**
	 * Sets the bake method to use (to actor, blueprint, foliage).
	 * @param InBakeMethod The new bake method to set.
	 * @return false if the asset/wrapper is invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetBakeMethod(const EHoudiniEngineBakeOption InBakeMethod);

	/**
	 * Gets the currently set bake method to use (to actor, blueprint, foliage).
	 * @param OutBakeMethod The current bake method.
	 * @return false if the asset/wrapper is invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetBakeMethod(EHoudiniEngineBakeOption& OutBakeMethod);

	/**
	 * Set the bRemoveOutputAfterBake property, that controls if temporary outputs are removed after a successful bake.
	 * @param bInRemoveOutputAfterBake If true, then after a successful bake, the HACs outputs will be cleared and
	 * removed.
	 * @return false if the asset/wrapper is invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetRemoveOutputAfterBake(const bool bInRemoveOutputAfterBake);

	/**
	 * Get the bRemoveOutputAfterBake property, that controls if temporary outputs are removed after a successful bake.
	 * @return true if bRemoveOutputAfterBake is true.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetRemoveOutputAfterBake() const;

	/**
	 * Set the bRecenterBakedActors property that controls if baked actors are recentered around their bounding box center.
	 * @param bInRecenterBakedActors If true, recenter baked actors to their bounding box center after bake.
	 * @return false if the asset/wrapper is invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetRecenterBakedActors(const bool bInRecenterBakedActors);
	
	/** Gets the bRecenterBakedActors property. If true, recenter baked actors to their bounding box center after bake. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetRecenterBakedActors() const;

	/**
	 * Set the bReplacePreviousBake property, if true, replace the previously baked output (if any) instead of creating
	 * new objects.
	 * @param bInReplacePreviousBake If true, replace the previously baked output (if any) instead of creating new
	 * objects.
	 * @return false if the asset/wrapper is invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetReplacePreviousBake(const bool bInReplacePreviousBake);

	/** Get the bReplacePreviousBake property.
	 * @return The value of bReplacePreviousBake. If true, previous bake output (if any) will be replaced by subsequent
	 * bakes.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetReplacePreviousBake() const;

	// Parameters

	/**
	 * Set the value of a float-based parameter.
	 * Supported parameter types:
	 *	- Float
	 *	- Color
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param InValue The value to set.
	 * @param InAtIndex The index of the parameter in the parameter tuple to set the value at. Defaults to 0.
	 * @param bInMarkChanged If true, the parameter is marked as changed and will be uploaded to Houdini before the
	 * next cook. If auto-cook triggers are enabled, this will also trigger a auto-cook. Defaults to true.
	 * @return true if the value was set or the parameter already had the given value.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetFloatParameterValue(FName InParameterTupleName, float InValue, int32 InAtIndex=0, bool bInMarkChanged=true);

	/**
	 * Get the value of a float parameter. Returns true if the parameter and index was found and the value set in OutValue.
	 * Supported parameter types:
	 *	- Float
	 *	- Color
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param OutValue The value of the parameter that was fetched.
	 * @param InAtIndex The index of the parameter in the parameter tuple to get. Defaults to 0.
	 * @return true if the wrapper is valid and the parameter was found.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetFloatParameterValue(FName InParameterTupleName, float& OutValue, int32 InAtIndex=0) const;

	/**
	 * Set the value of a color parameter.
	 * Supported parameter types:
	 *	- Color
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param InValue The value to set.
	 * @param bInMarkChanged If true, the parameter is marked as changed and will be uploaded to Houdini before the
	 * next cook. If auto-cook triggers are enabled, this will also trigger a auto-cook. Defaults to true.
	 * @return true if the value was set or the parameter already had the given value.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetColorParameterValue(FName InParameterTupleName, const FLinearColor& InValue, bool bInMarkChanged=true);

	/**
	 * Get the value of a color parameter. Returns true if the parameter was found and the value set in OutValue.
	 * Supported parameter types:
	 *	- Color
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param OutValue The value of the parameter that was fetched.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetColorParameterValue(FName InParameterTupleName, FLinearColor& OutValue) const;

	/**
	 * Set the value of a int32 parameter.
	 * Supported parameter types:
	 *	- Int
	 *	- IntChoice
	 *	- MultiParm
	 *	- Toggle
	 *	- Folder (set the folder as currently shown)
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param InValue The value to set.
	 * @param InAtIndex The index of the parameter in the parameter tuple to set the value at. Defaults to 0.
	 * @param bInMarkChanged If true, the parameter is marked as changed and will be uploaded to Houdini before the
	 * next cook. If auto-cook triggers are enabled, this will also trigger a auto-cook. Defaults to true.
	 * @return true if the value was set or the parameter already had the given value.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetIntParameterValue(FName InParameterTupleName, int32 InValue, int32 InAtIndex=0, bool bInMarkChanged=true);

	/**
	 * Get the value of a int32 parameter.
	 * Supported parameter types:
	 *	- Int
	 *	- IntChoice
	 *	- MultiParm
	 *	- Toggle
	 *	- Folder (get if the folder is currently shown)
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param OutValue The value of the parameter that was fetched.
	 * @param InAtIndex The index of the parameter in the parameter tuple to get. Defaults to 0.
	 * @return true if the parameter and index was found and the value set in OutValue. 
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetIntParameterValue(FName InParameterTupleName, int32& OutValue, int32 InAtIndex=0) const;

	/**
	 * Set the value of a bool parameter.
	 * Supported parameter types:
	 *	- Toggle
	 *	- Folder (set the folder as currently shown)
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param InValue The value to set.
	 * @param InAtIndex The index of the parameter in the parameter tuple to set the value at.
	 * @param bInMarkChanged If true, the parameter is marked as changed and will be uploaded to Houdini before the
	 * next cook. If auto-cook triggers are enabled, this will also trigger a auto-cook. Defaults to true.
	 * @return true if the value was set or the parameter already had the given value.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetBoolParameterValue(FName InParameterTupleName, bool InValue, int32 InAtIndex=0, bool bInMarkChanged=true);

	/**
	 * Get the value of a bool parameter.
	 * Supported parameter types:
	 *	- Toggle
	 *	- Folder (get if the folder is currently shown)
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param OutValue The value of the parameter that was fetched.
	 * @param InAtIndex The index of the parameter in the parameter tuple to get. Defaults to 0.
	 * @return true if the parameter and index was found and the value set in OutValue.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetBoolParameterValue(FName InParameterTupleName, bool& OutValue, int32 InAtIndex=0) const;

	/**
	 * Set the value of a String parameter.
	 * Supported parameter types:
	 * 	- String
	 * 	- StringChoice
	 * 	- StringAssetRef
	 * 	- File
	 * 	- FileDir
	 * 	- FileGeo
	 * 	- FileImage
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param InValue The value to set.
	 * @param InAtIndex The index of the parameter in the parameter tuple to set the value at.
	 * @param bInMarkChanged If true, the parameter is marked as changed and will be uploaded to Houdini before the
	 * next cook. If auto-cook triggers are enabled, this will also trigger a auto-cook. Defaults to true.
	 * @return true if the value was set or the parameter already had the given value.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetStringParameterValue(FName InParameterTupleName, const FString& InValue, int32 InAtIndex=0, bool bInMarkChanged=true);

	/**
	 * Get the value of a String parameter.
	 * Supported parameter types:
	 * 	- String
	 * 	- StringChoice
	 * 	- StringAssetRef
	 * 	- File
	 * 	- FileDir
	 * 	- FileGeo
	 * 	- FileImage
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param OutValue The value of the parameter that was fetched.
	 * @param InAtIndex The index of the parameter in the parameter tuple to get. Defaults to 0.
	 * @return true if the parameter was found and the value set in OutValue.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetStringParameterValue(FName InParameterTupleName, FString& OutValue, int32 InAtIndex=0) const;

	/**
	 * Set the value of an AssetRef parameter.
	 * Supported parameter types:
	 *	- StringAssetRef
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param InValue The value to set.
	 * @param InAtIndex The index of the parameter in the parameter tuple to set the value at.
	 * @param bInMarkChanged If true, the parameter is marked as changed and will be uploaded to Houdini before the
	 * next cook. If auto-cook triggers are enabled, this will also trigger a auto-cook. Defaults to true.
	 * @return true if the value was set or the parameter already had the given value.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetAssetRefParameterValue(FName InParameterTupleName, UObject* InValue, int32 InAtIndex=0, bool bInMarkChanged=true);

	/**
	 * Get the value of an AssetRef parameter.
	 * Supported parameter types:
	 *	- StringAssetRef
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param OutValue The value of the parameter that was fetched.
	 * @param InAtIndex The index of the parameter in the parameter tuple to get. Defaults to 0.
	 * @return True if the parameter was found and the value set in OutValue.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetAssetRefParameterValue(FName InParameterTupleName, UObject*& OutValue, int32 InAtIndex=0) const;

	/**
	 * Sets the number of points of the specified ramp parameter. This will insert or remove points from the end
	 * as necessary.
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param InNumPoints The new number of points to set. Must be >= 1.
	 * @return true if the parameter was found and the number of points set, or if the number of points was already InNumPoints.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetRampParameterNumPoints(FName InParameterTupleName, const int32 InNumPoints) const;

	/**
	 * Gets the number of points of the specified ramp parameter.
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param OutNumPoints The number of points the ramp has.
	 * @return true if the parameter was found and the number of points fetched.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetRampParameterNumPoints(FName InParameterTupleName, int32& OutNumPoints) const;

	/**
	 * Set the position, value and interpolation of a point of a FloatRamp parameter.
	 * Supported parameter types:
	 *	- FloatRamp
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param InPointIndex The index of the ramp point to set.
	 * @param InPointPosition The point position to set [0, 1].
	 * @param InPointValue The value to set for the point.
	 * @param InInterpolationType The interpolation to set at the point. Defaults to EHoudiniPublicAPIRampInterpolationType.Linear.
	 * @param bInMarkChanged If true, the parameter is marked as changed and will be uploaded to Houdini before the
	 * next cook. If auto-cook triggers are enabled, this will also trigger a auto-cook. Defaults to true.
	 * @return true if the value was set.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetFloatRampParameterPointValue(
		FName InParameterTupleName,
		const int32 InPointIndex,
		const float InPointPosition,
		const float InPointValue,
		const EHoudiniPublicAPIRampInterpolationType InInterpolationType=EHoudiniPublicAPIRampInterpolationType::LINEAR,
		const bool bInMarkChanged=true);

	/**
	 * Get the position, value and interpolation of a point of a FloatRamp parameter.
	 * Supported parameter types:
	 *	- FloatRamp
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param InPointIndex The index of the ramp point to get.
	 * @param OutPointPosition The point position [0, 1].
	 * @param OutPointValue The value at the point.
	 * @param OutInterpolationType The interpolation of the point.
	 * @return True if the parameter was found and output values set.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetFloatRampParameterPointValue(
		FName InParameterTupleName,
		const int32 InPointIndex,
		float& OutPointPosition,
		float& OutPointValue,
		EHoudiniPublicAPIRampInterpolationType& OutInterpolationType) const;

	/**
	 * Set all of the points (position, value and interpolation) of float ramp.
	 * Supported parameter types:
	 *	- FloatRamp
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param InRampPoints An array of structs to set as the ramp's points. 
	 * @param bInMarkChanged If true, parameters are marked as changed and will be uploaded to Houdini before the
	 * next cook. If auto-cook triggers are enabled, this will also trigger a auto-cook. Defaults to true.
	 * @return true if the values were set.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetFloatRampParameterPoints(
		FName InParameterTupleName,
		const TArray<FHoudiniPublicAPIFloatRampPoint>& InRampPoints,
		const bool bInMarkChanged=true);

	/**
	 * Get the all of the points (position, value and interpolation) of a FloatRamp parameter.
	 * Supported parameter types:
	 *	- FloatRamp
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param OutRampPoints The array to populate with the ramp's points.
	 * @return True if the parameter was found and output values set.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetFloatRampParameterPoints(
		FName InParameterTupleName,
		TArray<FHoudiniPublicAPIFloatRampPoint>& OutRampPoints) const;

	/**
	 * Set the position, value and interpolation of a point of a ColorRamp parameter.
	 * Supported parameter types:
	 *	- ColorRamp
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param InPointIndex The index of the ramp point to set.
	 * @param InPointPosition The point position to set [0, 1].
	 * @param InPointValue The value to set for the point.
	 * @param InInterpolationType The interpolation to set at the point. Defaults to EHoudiniPublicAPIRampInterpolationType.Linear.
	 * @param bInMarkChanged If true, the parameter is marked as changed and will be uploaded to Houdini before the
	 * next cook. If auto-cook triggers are enabled, this will also trigger a auto-cook. Defaults to true.
	 * @return true if the value was set.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetColorRampParameterPointValue(
		FName InParameterTupleName,
		const int32 InPointIndex,
		const float InPointPosition,
		const FLinearColor& InPointValue,
		const EHoudiniPublicAPIRampInterpolationType InInterpolationType=EHoudiniPublicAPIRampInterpolationType::LINEAR,
		const bool bInMarkChanged=true);

	/**
	 * Get the position, value and interpolation of a point of a ColorRamp parameter.
	 * Supported parameter types:
	 *	- ColorRamp
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param InPointIndex The index of the ramp point to get.
	 * @param OutPointPosition The point position [0, 1].
	 * @param OutPointValue The value at the point.
	 * @param OutInterpolationType The interpolation of the point.
	 * @return True if the parameter was found and output values set.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetColorRampParameterPointValue(
		FName InParameterTupleName,
		const int32 InPointIndex,
		float& OutPointPosition,
		FLinearColor& OutPointValue,
		EHoudiniPublicAPIRampInterpolationType& OutInterpolationType) const;

	/**
	 * Set all of the points (position, value and interpolation) of color ramp.
	 * Supported parameter types:
	 *	- ColorRamp
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param InRampPoints An array of structs to set as the ramp's points. 
	 * @param bInMarkChanged If true, parameters are marked as changed and will be uploaded to Houdini before the
	 * next cook. If auto-cook triggers are enabled, this will also trigger a auto-cook. Defaults to true.
	 * @return true if the values were set.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetColorRampParameterPoints(
		FName InParameterTupleName,
		const TArray<FHoudiniPublicAPIColorRampPoint>& InRampPoints,
		const bool bInMarkChanged=true);

	/**
	 * Get the all of the points (position, value and interpolation) of a ColorRamp parameter.
	 * Supported parameter types:
	 *	- ColorRamp
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param OutRampPoints The array to populate with the ramp's points.
	 * @return True if the parameter was found and output values set.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetColorRampParameterPoints(
		FName InParameterTupleName,
		TArray<FHoudiniPublicAPIColorRampPoint>& OutRampPoints) const;

	/**
	 * Trigger / click the specified button parameter.
	 * @return True if the button was found and triggered/clicked, or was already marked to be triggered.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool TriggerButtonParameter(FName InButtonParameterName);

	/**
	 * Gets all parameter tuples (with their values) from this asset and outputs it to OutParameterTuples.
	 * @param OutParameterTuples Populated with all parameter tuples and their values.
	 * @return false if the asset/wrapper is invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetParameterTuples(TMap<FName, FHoudiniParameterTuple>& OutParameterTuples) const;

	/**
	 * Sets all parameter tuple values (matched by name and compatible type) from InParameterTuples on this
	 * instantiated asset.
	 * @param InParameterTuples The parameter tuples to set.
	 * @return false if any entry in InParameterTuples could not be found on the asset or had an incompatible type/size.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetParameterTuples(const TMap<FName, FHoudiniParameterTuple>& InParameterTuples);

	// Inputs

	/**
	 * Creates an empty input wrapper.
	 * @param InInputClass the class of the input to create. See the UHoudiniPublicAPIInput class hierarchy.
	 * @return The newly created input wrapper, or null if the input wrapper could not be created.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API", Meta=(DeterminesOutputType="InInputClass"))
	UHoudiniPublicAPIInput* CreateEmptyInput(TSubclassOf<UHoudiniPublicAPIInput> InInputClass);

	/**
	 * Get the number of node inputs supported by the asset.
	 * @return The number of node inputs (inputs on the HDA node, excluding parameter-based inputs). Returns -1 if the
	 * asset/wrapper is invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	UPARAM(DisplayName="OutNumNodeInputs") int32 GetNumNodeInputs() const;

	/**
	 * Set a node input at the specific index.
	 * @param InNodeInputIndex The index of the node input, starts at 0.
	 * @param InInput The input wrapper to use to set the input.
	 * @return false if the the input could not be set, for example, if the wrapper is invalid, or if the input index
	 * is invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetInputAtIndex(const int32 InNodeInputIndex, const UHoudiniPublicAPIInput* InInput);

	/**
	 * Get the node input at the specific index and sets OutInput. This is a copy of the input structure. Changes to
	 * properties in OutInput won't affect the instantiated HDA until a subsequent call to SetInputAtIndex.
	 * @param InNodeInputIndex The index of the node input to get.
	 * @param OutInput Copy of the input configuration and data for node input index InNodeInputIndex.
	 * @return false if the input could be fetched, for example if the wrapper is invalid or the input index is invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetInputAtIndex(const int32 InNodeInputIndex, UHoudiniPublicAPIInput*& OutInput);

	/**
	 * Set node inputs at the specified indices via a map.
	 * @param InInputs A map of node input index to input wrapper to use to set inputs on the instantiated asset.
	 * @return true if all inputs from InInputs were set successfully.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetInputsAtIndices(const TMap<int32, UHoudiniPublicAPIInput*>& InInputs);

	/**
	 * Get all node inputs.
	 * @param OutInputs All node inputs as a map, with the node input index as key. The input configuration is copied
	 * from the instantiated asset, and changing an input property from the entry in this map will not affect the
	 * instantiated asset until a subsequent SetInputsAtIndices() call or SetInputAtIndex() call.
	 * @return false if the wrapper is invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetInputsAtIndices(TMap<int32, UHoudiniPublicAPIInput*>& OutInputs);

	/**
	 * Set a parameter-based input via parameter name.
	 * @param InParameterName The name of the input parameter.
	 * @param InInput The input wrapper to use to set/configure the input.
	 * @return false if the wrapper is invalid, InParameterName is not a valid input parameter, or if InInput could
	 * not be used to successfully configure/set the input.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API", Meta=(AutoCreateRefTerm="InParameterName"))
	bool SetInputParameter(const FName& InParameterName, const UHoudiniPublicAPIInput* InInput);

	/**
	 * Get a parameter-based input via parameter name. This is a copy of the input structure. Changes to properties in
	 * OutInput won't affect the instantiated HDA until a subsequent call to SetInputParameter.
	 * @param InParameterName The name of the input parameter.
	 * @param OutInput A copy of the input configuration for InParameterName.
	 * @return false if the wrapper is invalid, InParameterName is not a valid input parameter, or the current input
	 * configuration could not be successfully copied to a new UHoudiniPublicAPIInput wrapper.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API", Meta=(AutoCreateRefTerm="InParameterName"))
	bool GetInputParameter(const FName& InParameterName, UHoudiniPublicAPIInput*& OutInput);

	/**
	 * Set a parameter-based inputs via a map,
	 * @param InInputs A map of input parameter names to input wrapper to use to set inputs on the instantiated asset.
	 * @return true if all inputs from InInputs were set successfully.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API", Meta=(AutoCreateRefTerm="InParameterName"))
	bool SetInputParameters(const TMap<FName, UHoudiniPublicAPIInput*>& InInputs);

	/**
	 * Get a parameter-based inputs as a map
	 * @param OutInputs All parameter inputs as a map, with the input parameter name as key. The input configuration is
	 * copied from the instantiated asset, and changing an input property from the entry in this map will not affect the
	 * instantiated asset until a subsequent SetInputParameters() call or SetInputParameter() call.
	 * @return false if the wrapper is invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API", Meta=(AutoCreateRefTerm="InParameterName"))
	bool GetInputParameters(TMap<FName, UHoudiniPublicAPIInput*>& OutInputs);

	// Outputs

	/**
	 * Gets the number of outputs of the instantiated asset.
	 * @return the number of outputs of the instantiated asset. -1 if the asset/wrapper is invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	int32 GetNumOutputs() const;

	/**
	 * Gets the output type of the output at index InIndex.
	 * @param InIndex The output index to get the type for.
	 * @return the output type of the output at index InIndex.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	EHoudiniOutputType GetOutputTypeAt(const int32 InIndex) const;

	/**
	 * Populates OutIdentifiers with the output object identifiers of all the output objects of the output at InIndex.
	 * @param InIndex The output index to get output identifiers for.
	 * @param OutIdentifiers The output identifiers of the output objects at output index InIndex.
	 * @return false if the asset/wrapper is invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetOutputIdentifiersAt(const int32 InIndex, TArray<FHoudiniPublicAPIOutputObjectIdentifier>& OutIdentifiers) const;

	/**
	 * Gets the output object at InIndex identified by InIdentifier.
	 * @param InIndex The output index to get output object from.
	 * @param InIdentifier The output identifier of the output object to get from output index InIndex.
	 * @return nullptr if the index/identifier is invalid or if the asset/wrapper is invalid, otherwise the output
	 * object.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	UObject* GetOutputObjectAt(const int32 InIndex, const FHoudiniPublicAPIOutputObjectIdentifier& InIdentifier) const;

	/**
	 * Gets the output component at InIndex identified by InIdentifier.
	 * @param InIndex The output index to get output component from.
	 * @param InIdentifier The output identifier of the output component to get from output index InIndex.
	 * @return nullptr if the index/identifier is invalid or if the asset/wrapper is invalid, otherwise the output
	 * component.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	UObject* GetOutputComponentAt(const int32 InIndex, const FHoudiniPublicAPIOutputObjectIdentifier& InIdentifier) const;

	/**
	 * Gets the output's fallback BakeName (as configured on the output details panel) for the output at InIndex
	 * identified by InIdentifier.
	 * @param InIndex The output index of the output object to get fallback BakeName for.
	 * @param InIdentifier The output identifier of the output object to get fallback BakeName for.
	 * @param OutBakeName The fallback BakeName configured for the output object identified by InIndex and InIdentifier. 
	 * @return false if the index/identifier is invalid or if the asset/wrapper is invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetOutputBakeNameFallbackAt(const int32 InIndex, const FHoudiniPublicAPIOutputObjectIdentifier& InIdentifier, FString& OutBakeName) const;

	/**
	 * Sets the output's fallback BakeName (as configured on the output details panel) for the output at InIndex
	 * identified by InIdentifier.
	 * @param InIndex The output index of the output object to set fallback BakeName for.
	 * @param InIdentifier The output identifier of the output object to set fallback BakeName for.
	 * @param InBakeName The fallback BakeName to set for the output object identified by InIndex and InIdentifier. 
	 * @return false if the index/identifier is invalid or if the asset/wrapper is invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetOutputBakeNameFallbackAt(const int32 InIndex, const FHoudiniPublicAPIOutputObjectIdentifier& InIdentifier, const FString& InBakeName);

	/**
	 * Bake the specified output object to the content browser.
	 * @param InIndex The output index of the output object to bake.
	 * @param InIdentifier The output identifier of the output object to bake.
	 * @param InBakeName The bake name to bake with.
	 * @param InLandscapeBakeType For landscape assets, the output bake type.
	 * @return true if the output was baked successfully, false if the wrapper/asset is invalid, or the output index
	 * and output identifier combination is invalid, or if baking failed.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool BakeOutputObjectAt(const int32 InIndex, const FHoudiniPublicAPIOutputObjectIdentifier& InIdentifier, const FName InBakeName=NAME_None, const EHoudiniLandscapeOutputBakeType InLandscapeBakeType=EHoudiniLandscapeOutputBakeType::InValid);

	/**
	 * Returns true if the wrapped asset has any proxy output on any outputs.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool HasAnyCurrentProxyOutput() const;

	/**
	 * Returns true if the wrapped asset has any proxy output at output InIndex.
	 * @param InIndex The output index to check for proxies.
	 * @return true if the wrapped asset has any proxy output at output InIndex.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool HasAnyCurrentProxyOutputAt(const int32 InIndex) const;

	/**
	 * Returns true if the output object at output InIndex with identifier InIdentifier is a proxy.
	 * @param InIndex The output index of the output object to check.
	 * @param InIdentifier The output identifier of the output object at output index InIndex to check.
	 * @return true if the output object at output InIndex with identifier InIdentifier is a proxy.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool IsOutputCurrentProxyAt(const int32 InIndex, const FHoudiniPublicAPIOutputObjectIdentifier& InIdentifier) const;

	// Proxy mesh

	/**
	 * Refines all current proxy mesh outputs (if any) to static mesh. This could trigger a cook if the asset is loaded
	 * and has no cook data.
	 * @param bInSilent If true, then slate notifications about the refinement process are not shown.
	 * @return Whether the refinement process is needed, requires a pending asynchronous cook, or was completed
	 * synchronously.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	EHoudiniProxyRefineRequestResult RefineAllCurrentProxyOutputs(const bool bInSilent);

	// PDG

	/**
	 * Returns true if the wrapped asset is valid and has a PDG asset link.
	 * @return true if the wrapped asset is valid and has a PDG asset link.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool HasPDGAssetLink() const;

	/**
	 * Gets the paths (relative to the instantiated asset) of all TOP networks in the HDA.
	 * @param OutTOPNetworkPaths The relative paths of the TOP networks in the HDA.
	 * @return false if the asset/wrapper is invalid, or does not contain any TOP networks.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetPDGTOPNetworkPaths(TArray<FString>& OutTOPNetworkPaths) const;
	
	/**
	 * Gets the paths (relative to the specified TOP network) of all TOP nodes in the network.
	 * @param InNetworkRelativePath The relative path of the network inside the instantiated asset, as returned by
	 * GetPDGTOPNetworkPaths(), to fetch TOP node paths for.
	 * @return false if the asset/wrapper is invalid, or does not contain the specified TOP network.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetPDGTOPNodePaths(const FString& InNetworkRelativePath, TArray<FString>& OutTOPNodePaths) const;

	/**
	 * Dirty all TOP networks in this asset.
	 * @return true if TOP networks were dirtied.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool PDGDirtyAllNetworks();

	/**
	 * Dirty the specified TOP network.
	 * @param InNetworkRelativePath The relative path of the network inside the instantiated asset, as returned by
	 * GetPDGTOPNetworkPaths().
	 * @return true if the TOP network was dirtied.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool PDGDirtyNetwork(const FString& InNetworkRelativePath);

	/**
	 * Dirty the specified TOP node.
	 * @param InNetworkRelativePath The relative path of the network inside the instantiated asset, as returned by
	 * GetPDGTOPNetworkPaths().
	 * @param InNodeRelativePath The relative path of the TOP node inside the specified TOP network.
	 * @return true if TOP nodes were dirtied.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool PDGDirtyNode(const FString& InNetworkRelativePath, const FString& InNodeRelativePath);

	// // Cook all outputs for all TOP networks in this asset.
	// // Returns true if TOP networks were set to cook.
	// UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	// bool PDGCookOutputsForAllNetworks();

	/**
	 * Cook all outputs for the specified TOP network.
	 * @param InNetworkRelativePath The relative path of the network inside the instantiated asset, as returned by
	 * GetPDGTOPNetworkPaths().
	 * @return true if the TOP network was set to cook.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool PDGCookOutputsForNetwork(const FString& InNetworkRelativePath);

	/**
	 * Cook the specified TOP node.
	 * @param InNetworkRelativePath The relative path of the network inside the instantiated asset, as returned by
	 * GetPDGTOPNetworkPaths().
	 * @param InNodeRelativePath The relative path of the TOP node inside the specified TOP network.
	 * @return true if the TOP node was set to cook.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool PDGCookNode(const FString& InNetworkRelativePath, const FString& InNodeRelativePath);

	/**
	 * Bake all outputs of the instantiated asset's PDG contexts using the settings configured on the asset.
	 * @return true if the bake was successful.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool PDGBakeAllOutputs();

	/**
	 * Bake all outputs of the instantiated asset's PDG contexts using the specified settings.
	 * @param InBakeOption The bake option (to actors, blueprints or foliage).
	 * @param InBakeSelection Whether to bake outputs from all networks, the selected network or the selected node.
	 * @param InBakeReplacementMode Whether to replace previous bake results/existing assets with the same name
	 * when baking.
	 * @param bInRecenterBakedActors If true, recenter baked actors to their bounding box center. Defaults to false.
	 * @return true if the bake was successful.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool PDGBakeAllOutputsWithSettings(
		const EHoudiniEngineBakeOption InBakeOption,
		const EPDGBakeSelectionOption InBakeSelection,
		const EPDGBakePackageReplaceModeOption InBakeReplacementMode,
		const bool bInRecenterBakedActors=false);

	/**
	 * Set whether to automatically bake PDG work items after a successfully loading them.
	 * @param bInAutoBakeEnabled If true, automatically bake work items after successfully loading them.
	 * @return false if the asset/wrapper is invalid, or does not contain a TOP network.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetPDGAutoBakeEnabled(const bool bInAutoBakeEnabled);

	/** Returns true if PDG auto bake is enabled. See SetPDGAutoBakeEnabled(). */ 
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool IsPDGAutoBakeEnabled() const;

	/**
	 * Sets the bake method to use for PDG baking (to actor, blueprint, foliage).
	 * @param InBakeMethod The new bake method to set.
	 * @return false if the asset/wrapper is invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetPDGBakeMethod(const EHoudiniEngineBakeOption InBakeMethod);

	/**
	 * Gets the currently set bake method to use for PDG baking (to actor, blueprint, foliage).
	 * @param OutBakeMethod The current bake method.
	 * @return false if the asset/wrapper is invalid or does not contain a TOP network.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetPDGBakeMethod(EHoudiniEngineBakeOption& OutBakeMethod);

	/**
	 * Set which outputs to bake for PDG, for example, all, selected network, selected node
	 * @param InBakeSelection The new bake selection.
	 * @return false if the asset/wrapper is invalid or does not contain a TOP network.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetPDGBakeSelection(const EPDGBakeSelectionOption InBakeSelection);

	/**
	 * Get which outputs to bake for PDG, for example, all, selected network, selected node
	 * @param OutBakeSelection The current bake selection setting.
	 * @return false if the asset/wrapper is invalid or does not contain a TOP network.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetPDGBakeSelection(EPDGBakeSelectionOption& OutBakeSelection);

	/**
	 * Setter for the bRecenterBakedActors property, that determines if baked actors are recentered to their bounding
	 * box center after a PDG bake, on the PDG asset link.
	 * @param bInRecenterBakedActors If true, recenter baked actors to their bounding box center after bake (PDG)
	 * @return false if the asset/wrapper is invalid or does not contain a TOP network.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetPDGRecenterBakedActors(const bool bInRecenterBakedActors);
	
	/**
	 * Getter for the bRecenterBakedActors property, that determines if baked actors are recentered to their bounding
	 * box center after a PDG bake, on the PDG asset link.
	 * @return true if baked actors should be recentered to their bounding box center after bake (PDG)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetPDGRecenterBakedActors() const;

	/**
	 * Set the replacement mode to use for PDG baking (replace previous bake output vs increment)
	 * @param InBakingReplacementMode The new replacement mode to set.
	 * @return false if the asset/wrapper is invalid or does not contain a TOP network.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool SetPDGBakingReplacementMode(const EPDGBakePackageReplaceModeOption InBakingReplacementMode);

	/**
	 * Get the replacement mode to use for PDG baking (replace previous bake output vs increment)
	 * @param OutBakingReplacementMode The current replacement mode.
	 * @return false if the asset/wrapper is invalid or does not contain a TOP network.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool GetPDGBakingReplacementMode(EPDGBakePackageReplaceModeOption& OutBakingReplacementMode) const;

	/**
	 * Getter for the OnPreInstantiationDelegate, broadcast before the HDA is instantiated. The HDA's default parameter
	 * definitions are available, but the node has not yet been instantiated in HAPI/Houdini Engine. Parameter values
	 * can be set at this point.
	 */
	FOnHoudiniAssetStateChange& GetOnPreInstantiationDelegate() { return OnPreInstantiationDelegate; }

	/**
	 * Getter for the OnPostInstantiationDelegate, broadcast after the HDA is instantiated. This is a good place to
	 * set/configure inputs before the first cook.
	 */
	FOnHoudiniAssetStateChange& GetOnPostInstantiationDelegate() { return OnPostInstantiationDelegate; }
	
	/**
	 * Getter for the OnPostCookDelegate, broadcast after the HDA has cooked.
	 */
	FOnHoudiniAssetPostCook& GetOnPostCookDelegate() { return OnPostCookDelegate; }

	/**
	 * Getter for the OnPreProcessStateExitedDelegate, broadcast after the output pre-processing phase of the HDA,
	 * but before it enters the post processing phase. When this delegate is broadcast output objects/assets have not
	 * yet been created.
	 */
	FOnHoudiniAssetStateChange& GetOnPreProcessStateExitedDelegate() { return OnPreProcessStateExitedDelegate; }

	/**
	 * Getter for the OnPostProcessingDelegate, broadcast after the HDA has processed its outputs and created output
	 * objects/assets.
	 */
	FOnHoudiniAssetStateChange& GetOnPostProcessingDelegate() { return OnPostProcessingDelegate; }

	/**
	 * Getter for the OnPostBakeDelegate, broadcast after the HDA has finished baking outputs (not called for
	 * individual outputs that are baked to the content browser).
	 */
	FOnHoudiniAssetPostBake& GetOnPostBakeDelegate() { return OnPostBakeDelegate; }

	/**
	 * Getter for the OnPostPDGTOPNetworkCookDelegate, broadcast after the HDA/PDG has cooked a TOP Network. Work item
	 * results have not necessarily yet been loaded.
	 */
	FOnHoudiniAssetPostCook& GetOnPostPDGTOPNetworkCookDelegate() { return OnPostPDGTOPNetworkCookDelegate; }

	/**
	 * Getter for the OnPostPDGBakeDelegate, broadcast after the HDA/PDG has finished baking outputs (not called for
	 * individual outputs that are baked to the content browser).
	 */
	FOnHoudiniAssetPostBake& GetOnPostPDGBakeDelegate() { return OnPostPDGBakeDelegate; }

	/**
	 * Getter for the OnProxyMeshesRefinedDelegate, broadcast for each proxy mesh of the HDA's outputs that has been
	 * refined to a UStaticMesh.
	 */
	FOnHoudiniAssetProxyMeshesRefinedDelegate& GetOnProxyMeshesRefinedDelegate() { return OnProxyMeshesRefinedDelegate; }

protected:

	/** This will unwrap/unbind the currently wrapped instantiated asset. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	void ClearHoudiniAssetObject();

	/**
	 * Attempt to bind to the asset's PDG asset link, if it has one, and if the wrapper is not already bound to its
	 * events.
	 */
	UFUNCTION()
	bool BindToPDGAssetLink();
	
	/** Handler that is bound to the wrapped HAC's state change delegate. */
	UFUNCTION()
	void HandleOnHoudiniAssetComponentStateChange(UHoudiniAssetComponent* InHAC, const EHoudiniAssetState InFromState, const EHoudiniAssetState InToState);

	/** Handler that is bound to the wrapped HAC's PostCook delegate. */
	UFUNCTION()
	void HandleOnHoudiniAssetComponentPostCook(UHoudiniAssetComponent* InHAC, const bool bInCookSuccess);

	/** Handler that is bound to the wrapped HAC's PostBake delegate. */
	UFUNCTION()
	void HandleOnHoudiniAssetComponentPostBake(UHoudiniAssetComponent* InHAC, const bool bInBakeSuccess);

	/** Handler that is bound to the wrapped PDG asset link's OnPostTOPNetworkCookDelegate delegate. */
	UFUNCTION()
	void HandleOnHoudiniPDGAssetLinkTOPNetPostCook(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNetwork* InTOPNet, const bool bInAnyWorkItemsFailed);

	/** Handler that is bound to the wrapped PDG asset link's OnPostBake delegate. */
	UFUNCTION()
	void HandleOnHoudiniPDGAssetLinkPostBake(UHoudiniPDGAssetLink* InPDGAssetLink, const bool bInBakeSuccess);

	/**
	 * Handler that is bound to FHoudiniEngineCommands::GetOnHoudiniProxyMeshesRefined(). It is called for any HAC
	 * that has its proxy meshes refined. If relevant for this wrapped asset, then
	 * #OnProxyMeshesRefinedDelegate is broadcast.
	 */
	UFUNCTION()
	void HandleOnHoudiniProxyMeshesRefinedGlobal(UHoudiniAssetComponent* InHAC, const EHoudiniProxyRefineResult InResult);

	/**
	 * Helper function for getting the instantiated asset's AHoudiniAssetActor. If there is no valid
	 * AHoudiniAssetActor an error is set with SetErrorMessage() and false is returned.
	 * @param OutActor Set to the AHoudiniAssetActor of the wrapped asset, if found.
	 * @return true if the wrapped asset has/is a valid AHoudiniAssetActor, false otherwise.
	 */
	bool GetValidHoudiniAssetActorWithError(AHoudiniAssetActor*& OutActor) const;
	
	/**
	 * Helper function for getting the instantiated asset's UHoudiniAssetComponent. If there is no valid
	 * HoudiniAssetComponent an error is set with SetErrorMessage() and false is returned.
	 * @param OutHAC Set to the HoudiniAssetComponent of the wrapped asset, if found.
	 * @return true if the wrapped asset has a valid HoudiniAssetComponent, false otherwise.
	 */
	bool GetValidHoudiniAssetComponentWithError(UHoudiniAssetComponent*& OutHAC) const;

	/**
	 * Helper function for getting a valid output at the specified index. If there is no valid
	 * UHoudiniOutput at that index (either the index is out of range or the output is null/invalid) an error is set
	 * with SetErrorMessage() and false is returned.
	 * @param InOutputIndex The output index.
	 * @param OutOutput Set to the valid UHoudiniOutput at InOutputIndex, if found.
	 * @return true if there is a valid UHoudiniOutput at index InOutputIndex.
	 */
	bool GetValidOutputAtWithError(const int32 InOutputIndex, UHoudiniOutput*& OutOutput) const;

	/** Helper function for getting the instantiated asset's PDG asset link. */
	UHoudiniPDGAssetLink* GetHoudiniPDGAssetLink() const;
	
	/**
	 * Helper function for getting the instantiated asset's UHoudiniPDGAssetLink. If there is no valid
	 * UHoudiniPDGAssetLink an error is set with SetErrorMessage() and false is returned.
	 * @param OutAssetLink Set to the UHoudiniPDGAssetLink of the wrapped asset, if found.
	 * @return true if the wrapped asset has a valid UHoudiniPDGAssetLink, false otherwise.
	 */
	bool GetValidHoudiniPDGAssetLinkWithError(UHoudiniPDGAssetLink*& OutAssetLink) const;

	/** Helper function for getting a valid parameter by name */
	UHoudiniParameter* FindValidParameterByName(const FName& InParameterTupleName) const;

	/**
	 * Helper function to find the appropriate array index for a ramp point.
	 * @param InParam The parameter.
	 * @param InIndex The index of the ramp point to get. If INDEX_NONE, all points are fetched.
	 * @param OutPointData Array populated with all fetched points. The bool in the pair is true if the
	 * object is UHoudiniParameterRampFloatPoint or UHoudiniParameterRampColorPoint and false if it is
	 * UHoudiniParameterRampModificationEvent.
	 * @return true if the point was, or all points were, fetched successfully.
	 */
	bool FindRampPointData(
		UHoudiniParameter* const InParam, const int32 InIndex, TArray<TPair<UObject*, bool>>& OutPointData) const;

	/**
	 * Set the position, value and interpolation of a point of a ramp parameter.
	 * Supported parameter types:
	 *	- FloatRamp
	 *	- ColorRamp
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param InPointIndex The index of the ramp point to set.
	 * @param InPosition The point position [0, 1].
	 * @param InFloatValue The float value at the point (if this is a float ramp).
	 * @param InColorValue The color value at the point (if this is a color ramp).
	 * @param InInterpolation The interpolation of the point.
	 * @param bInMarkChanged If true, the parameter is marked as changed and will be uploaded to Houdini before the
	 * next cook. If auto-cook triggers are enabled, this will also trigger a auto-cook. Defaults to true.
	 * @return true if the value was set.
	 */
	bool SetRampParameterPointValue(
		FName InParameterTupleName,
		const int32 InPointIndex,
		const float InPosition,
		const float InFloatValue,
		const FLinearColor& InColorValue,
		const EHoudiniPublicAPIRampInterpolationType InInterpolation,
		const bool bInMarkChanged=true);

	/**
	 * Get the position, value and interpolation of a point of a ramp parameter.
	 * Supported parameter types:
	 *	- FloatRamp
	 *	- ColorRamp
	 * @param InParameterTupleName The name of the parameter tuple.
	 * @param InPointIndex The index of the ramp point to get.
	 * @param OutPosition The point position [0, 1].
	 * @param OutFloatValue The float value at the point (if this is a float ramp).
	 * @param OutColorValue The color value at the point (if this is a color ramp).
	 * @param OutInterpolation The interpolation of the point.
	 * @return True if the parameter was found and output values set.
	 */
	bool GetRampParameterPointValue(
		FName InParameterTupleName,
		const int32 InPointIndex,
		float& OutPosition,
		float& OutFloatValue,
		FLinearColor& OutColorValue,
		EHoudiniPublicAPIRampInterpolationType& OutInterpolation) const;

	/** Helper function for getting an input by node index */
	UHoudiniInput* GetHoudiniNodeInputByIndex(const int32 InNodeInputIndex);
	const UHoudiniInput* GetHoudiniNodeInputByIndex(const int32 InNodeInputIndex) const;

	/** Helper function for getting an input by parameter name */
	UHoudiniInput* FindValidHoudiniNodeInputParameter(const FName& InInputParameterName);
	const UHoudiniInput* FindValidHoudiniNodeInputParameter(const FName& InInputParameterName) const;

	/** Helper function for populating a UHoudiniPublicAPIInput from a UHoudiniInput */
	bool CreateAndPopulateAPIInput(const UHoudiniInput* InHoudiniInput, UHoudiniPublicAPIInput*& OutAPIInput);
	/** Helper function for populating a UHoudiniInput from a UHoudiniPublicAPIInput */
	bool PopulateHoudiniInput(const UHoudiniPublicAPIInput* InAPIInput, UHoudiniInput* InHoudiniInput) const;

	/**
	 * Helper functions for getting a TOP network by path. If the TOP network could not be found, an error is set
	 * with SetErrorMessage, and false is returned.
	 * @param InNetworkRelativePath The relative path to the network inside the asset.
	 * @param OutNetworkIndex The index to the network in the asset link's AllNetworks array.
	 * @param OutNetwork The network that was found at InNetworkRelativePath.
	 * @return true if the network was found and is valid, false otherwise.
	 */
	bool GetValidTOPNetworkByPathWithError(const FString& InNetworkRelativePath, int32& OutNetworkIndex, UTOPNetwork*& OutNetwork) const;

	/**
	 * Helper functions for getting a TOP node by path. If the TOP node could not be found, an error is set
	 * with SetErrorMessage, and false is returned.
	 * @param InNetworkRelativePath The relative path to the network inside the asset.
	 * @param InNodeRelativePath The relative path to the node inside the network.
	 * @param OutNetworkIndex The index to the network in the asset link's AllTOPNetworks array.
	 * @param OutNodeIndex The index to the TOP node in the network's AllTOPNodes array.
	 * @param OutNode The node that was found.
	 * @return true if the node was found and is valid, false otherwise.
	 */
	bool GetValidTOPNodeByPathWithError(
		const FString& InNetworkRelativePath,
		const FString& InNodeRelativePath,
		int32& OutNetworkIndex,
		int32& OutNodeIndex,
		UTOPNode*& OutNode) const;

	/** The wrapped Houdini Asset object (not the uasset, an AHoudiniAssetActor or UHoudiniAssetComponent). */
	UPROPERTY(BlueprintReadOnly, Category="Houdini|Public API")
	TSoftObjectPtr<UObject> HoudiniAssetObject;

	/** The wrapped AHoudiniAssetActor (derived from HoudiniAssetObject when calling WrapHoudiniAssetObject()). */
	UPROPERTY(BlueprintReadOnly, Category="Houdini|Public API")
	TSoftObjectPtr<AHoudiniAssetActor> CachedHoudiniAssetActor;

	/** The wrapped UHoudiniAssetComponent (derived from HoudiniAssetObject when calling WrapHoudiniAssetObject()). */
	UPROPERTY(BlueprintReadOnly, Category="Houdini|Public API")
	TSoftObjectPtr<UHoudiniAssetComponent> CachedHoudiniAssetComponent;

	/**
	 * Delegate that is broadcast when entering the PreInstantiation state: the HDA's default parameter definitions are
	 * available, but the node has not yet been instantiated in HAPI/Houdini Engine. Parameters can be set at this point.
	 */
	UPROPERTY(BlueprintAssignable, Category="Houdini|Public API")
	FOnHoudiniAssetStateChange OnPreInstantiationDelegate;

	/**
	 * Delegate that is broadcast after the asset was successfully instantiated. This is a good place to set/configure
	 * inputs before the first cook.
	 */
	UPROPERTY(BlueprintAssignable, Category="Houdini|Public API")
	FOnHoudiniAssetStateChange OnPostInstantiationDelegate;

	/** Delegate that is broadcast after a cook completes. Output objects/assets have not yet been created/updated. */
	UPROPERTY(BlueprintAssignable, Category="Houdini|Public API")
	FOnHoudiniAssetPostCook OnPostCookDelegate;

	/** Delegate that is broadcast when PreProcess is exited. Output objects/assets have not yet been created/updated. */
	UPROPERTY(BlueprintAssignable, Category="Houdini|Public API")
	FOnHoudiniAssetStateChange OnPreProcessStateExitedDelegate;

	/**
	 * Delegate that is broadcast when the Processing state is exited and the None state is entered. Output objects
	 * assets have been created/updated.
	 */
	UPROPERTY(BlueprintAssignable, Category="Houdini|Public API")
	FOnHoudiniAssetStateChange OnPostProcessingDelegate;

	/**
	 * Delegate that is broadcast after baking the asset (not called for individual outputs that are baked to the
	 * content browser).
	 */
	UPROPERTY(BlueprintAssignable, Category="Houdini|Public API")
	FOnHoudiniAssetPostBake OnPostBakeDelegate;

	/**
	 * Delegate that is broadcast after a cook of a TOP network completes. Work item results have not necessarily yet
	 * been loaded.
	 */
	UPROPERTY(BlueprintAssignable, Category="Houdini|Public API")
	FOnHoudiniAssetPostCook OnPostPDGTOPNetworkCookDelegate;

	/**
	 * Delegate that is broadcast after baking PDG outputs (not called for individual outputs that are baked to the
	 * content browser).
	 */
	UPROPERTY(BlueprintAssignable, Category="Houdini|Public API")
	FOnHoudiniAssetPostBake OnPostPDGBakeDelegate;

	/** Delegate that is broadcast after refining all proxy meshes for this wrapped asset. */
	UPROPERTY(BlueprintAssignable, Category="Houdini|Public API")
	FOnHoudiniAssetProxyMeshesRefinedDelegate OnProxyMeshesRefinedDelegate;

	/**
	 * This starts as false and is set to true in HandleOnHoudiniAssetComponentStateChange during post instantiation,
	 * once we have checked for a PDG asset link and configured the bindings if there is one.
	 */
	UPROPERTY()
	bool bAssetLinkSetupAttemptComplete;

	// Delegate handles

	/** Handle for the binding to the HAC's asset state change delegate */
	FDelegateHandle OnAssetStateChangeDelegateHandle;
	/** Handle for the binding to the HAC's post cook delegate */
	FDelegateHandle OnPostCookDelegateHandle;
	/** Handle for the binding to the HAC's post bake delegate */
	FDelegateHandle OnPostBakeDelegateHandle;
	/** Handle for the binding to the global proxy mesh refined delegate */
	FDelegateHandle OnHoudiniProxyMeshesRefinedDelegateHandle;
	/** Handle for the binding to the HAC's PDG asset link's post TOP Network cook delegate */
	FDelegateHandle OnPDGPostTOPNetworkCookDelegateHandle;
	/** Handle for the binding to the HAC's PDG asset link's post bake delegate */
	FDelegateHandle OnPDGPostBakeDelegateHandle;

};
