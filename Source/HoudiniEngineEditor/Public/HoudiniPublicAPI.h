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
#include "Templates/SubclassOf.h"

#include "HoudiniPublicAPIInputTypes.h"
#include "HoudiniPublicAPIObjectBase.h"
#include "HoudiniEngineRuntimeCommon.h"

#include "HoudiniPublicAPI.generated.h"

// Public API version
// Increased with Houdini compatibility changes
#define HOUDINI_PUBLIC_API_VERSION_MAJOR 1
// Increased for major API changes or changes that break backwards compatibility of the API
#define HOUDINI_PUBLIC_API_VERSION_MINOR 1
// Increased for patches/revisions that don't change the API in a backwards incompatible manner
#define HOUDINI_PUBLIC_API_VERSION_PATCH 0

class ULevel;

class UHoudiniAsset;
class UHoudiniPublicAPIAssetWrapper;
class UHoudiniPublicAPIInput;

/** Public API version of EHoudiniRampInterpolationType: blueprints do not support int8 based enums. */
UENUM(BlueprintType)
enum class EHoudiniPublicAPIRampInterpolationType : uint8
{
	InValid = 0,

	CONSTANT = 1,
	LINEAR = 2,
	CATMULL_ROM = 3,
	MONOTONE_CUBIC = 4,
	BEZIER = 5,
	BSPLINE = 6,
	HERMITE = 7
};

/**
 * The Houdini Engine v2 Plug-in's Public API.
 *
 * The API allows one to manage a Houdini Engine session (Create/Stop/Restart), Pause/Resume asset cooking and
 * instantiate HDA's and interact with it (set/update inputs, parameters, cook, iterate over outputs and bake outputs).
 *
 * Interaction with an instantiated HDA is done via UHoudiniPublicAPIAssetWrapper. 
 *
 */
UCLASS(BlueprintType, Blueprintable, Category="Houdini|Public API")
class HOUDINIENGINEEDITOR_API UHoudiniPublicAPI : public UHoudiniPublicAPIObjectBase
{
	GENERATED_BODY()

public:

	UHoudiniPublicAPI();
	
	// Session
	
	/** Returns true if there is a valid Houdini Engine session running/connected */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool IsSessionValid() const;
	
	/** Start a new Houdini Engine Session if there is no current session */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	void CreateSession();
	
	/** Stops the current session */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	void StopSession();

	/** Stops, then creates a new session */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	void RestartSession();

	// Assets

	/**
	 * Instantiates an HDA in the specified world/level. Returns a wrapper for instantiated asset.
	 * @param InHoudiniAsset The HDA to instantiate.
	 * @param InInstantiateAt The Transform to instantiate the HDA with.
	 * @param InWorldContextObject A world context object for identifying the world to spawn in, if
	 * @InSpawnInLevelOverride is null.
	 * @param InSpawnInLevelOverride If not nullptr, then the AHoudiniAssetActor is spawned in that level. If both
	 * InSpawnInLevelOverride and InWorldContextObject are null, then the actor is spawned in the current editor
	 * context world's current level.
	 * @param bInEnableAutoCook If true (the default) the HDA will cook automatically after instantiation and after
	 * parameter, transform and input changes.
	 * @param bInEnableAutoBake If true, the HDA output is automatically baked after a cook. Defaults to false.
	 * @param InBakeDirectoryPath The directory to bake to if the bake path is not set via attributes on the HDA output.
	 * @param InBakeMethod The bake target (to actor vs blueprint). @see EHoudiniEngineBakeOption.
	 * @param bInRemoveOutputAfterBake If true, HDA temporary outputs are removed after a bake. Defaults to false.
	 * @param bInRecenterBakedActors Recenter the baked actors to their bounding box center. Defaults to false.
	 * @param bInReplacePreviousBake If true, on every bake replace the previous bake's output (assets + actors) with
	 * the new bake's output. Defaults to false.
	 * @return A wrapper for the instantiated asset, or nullptr if InHoudiniAsset or InInstantiateAt is invalid, or
	 * the AHoudiniAssetActor could not be spawned. See UHoudiniPublicAPIAssetWrapper.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API", Meta=(AutoCreateRefTerm="InInstantiateAt"))
	UHoudiniPublicAPIAssetWrapper* InstantiateAsset(
		UHoudiniAsset* InHoudiniAsset,
		const FTransform& InInstantiateAt,
		UObject* InWorldContextObject=nullptr,
		ULevel* InSpawnInLevelOverride=nullptr,
		const bool bInEnableAutoCook=true,
		const bool bInEnableAutoBake=false,
		const FString& InBakeDirectoryPath="",
		const EHoudiniEngineBakeOption InBakeMethod=EHoudiniEngineBakeOption::ToActor,
		const bool bInRemoveOutputAfterBake=false,
		const bool bInRecenterBakedActors=false,
		const bool bInReplacePreviousBake=false);

	/**
	 * Instantiates an HDA in the specified world/level using an existing wrapper.
	 * @param InWrapper The wrapper to instantiate the HDA with.
	 * @param InHoudiniAsset The HDA to instantiate.
	 * @param InInstantiateAt The Transform to instantiate the HDA with.
	 * @param InWorldContextObject A world context object for identifying the world to spawn in, if
	 * InSpawnInLevelOverride is null.
	 * @param InSpawnInLevelOverride If not nullptr, then the AHoudiniAssetActor is spawned in that level. If both
	 * InSpawnInLevelOverride and InWorldContextObject are null, then the actor is spawned in the current editor
	 * context world's current level.
	 * @param bInEnableAutoCook If true (the default) the HDA will cook automatically after instantiation and after
	 * parameter, transform and input changes.
	 * @param bInEnableAutoBake If true, the HDA output is automatically baked after a cook. Defaults to false.
	 * @param InBakeDirectoryPath The directory to bake to if the bake path is not set via attributes on the HDA output.
	 * @param InBakeMethod The bake target (to actor vs blueprint). @see EHoudiniEngineBakeOption.
	 * @param bInRemoveOutputAfterBake If true, HDA temporary outputs are removed after a bake. Defaults to false.
	 * @param bInRecenterBakedActors Recenter the baked actors to their bounding box center. Defaults to false.
	 * @param bInReplacePreviousBake If true, on every bake replace the previous bake's output (assets + actors) with
	 * the new bake's output. Defaults to false.
	 * @return true if InWrapper and InHoudiniAsset is valid and the AHoudiniAssetActor was spawned.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API", Meta=(AutoCreateRefTerm="InInstantiateAt"))
	bool InstantiateAssetWithExistingWrapper(
		UHoudiniPublicAPIAssetWrapper* InWrapper,
		UHoudiniAsset* InHoudiniAsset,
		const FTransform& InInstantiateAt,
		UObject* InWorldContextObject=nullptr,
		ULevel* InSpawnInLevelOverride=nullptr,
		const bool bInEnableAutoCook=true,
		const bool bInEnableAutoBake=false,
		const FString& InBakeDirectoryPath="",
		const EHoudiniEngineBakeOption InBakeMethod=EHoudiniEngineBakeOption::ToActor,
		const bool bInRemoveOutputAfterBake=false,
		const bool bInRecenterBakedActors=false,
		const bool bInReplacePreviousBake=false);

	// Cooking

	/** Returns true if asset cooking is paused. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	bool IsAssetCookingPaused() const;
	
	/** Pause asset cooking (if not already paused) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	void PauseAssetCooking();

	/** Resume asset cooking (if it was paused) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API")
	void ResumeAssetCooking();

	// Inputs

	/**
	 * Create a new empty API input object. The user must populate it and then set it as an input on an asset wrapper.
	 * @param InInputClass The class of the input to create, must be a subclass of UHoudiniPublicAPIInput.
	 * @param InOuter The owner of the input, if nullptr, then this API instance will be set as the outer.
	 * @return The newly created empty input object instance.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini|Public API", Meta=(DeterminesOutputType="InInputClass"))
	UHoudiniPublicAPIInput* CreateEmptyInput(TSubclassOf<UHoudiniPublicAPIInput> InInputClass, UObject* InOuter=nullptr);

	// Helpers -- enum conversions
	
	/**
	 * Helper for converting from EHoudiniRampInterpolationType to EHoudiniPublicAPIRampInterpolationType
	 * @param InInterpolationType The EHoudiniRampInterpolationType to convert.
	 * @return The EHoudiniPublicAPIRampInterpolationType value of InInterpolationType.
	 */
	static EHoudiniPublicAPIRampInterpolationType ToHoudiniPublicAPIRampInterpolationType(const EHoudiniRampInterpolationType InInterpolationType);

	/**
	 * Helper for converting from EHoudiniPublicAPIRampInterpolationType to EHoudiniRampInterpolationType
	 * @param InInterpolationType The EHoudiniPublicAPIRampInterpolationType to convert.
	 * @return The EHoudiniRampInterpolationType value of InInterpolationType.
	 */
	static EHoudiniRampInterpolationType ToHoudiniRampInterpolationType(const EHoudiniPublicAPIRampInterpolationType InInterpolationType);

};
