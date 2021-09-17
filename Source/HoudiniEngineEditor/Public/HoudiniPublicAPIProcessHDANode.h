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

#include "Kismet/BlueprintAsyncActionBase.h"

#include "HoudiniPublicAPIAssetWrapper.h"

#include "HoudiniPublicAPIProcessHDANode.generated.h"


class UHoudiniPublicAPIInput;
class UHoudiniAsset;

// Delegate type for output pins on the node.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnProcessHDANodeOutputPinDelegate, UHoudiniPublicAPIAssetWrapper*, AssetWrapper, const bool, bCookSuccess, const bool, bBakeSuccess);

/**
 * A Blueprint async node for instantiating and cooking/processing an HDA, with delegate output pins for the
 * various phases/state changes of the HDA.
 */
UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniPublicAPIProcessHDANode : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
	
public:
	UHoudiniPublicAPIProcessHDANode(const FObjectInitializer& ObjectInitializer);

	/**
	 * Instantiates an HDA in the specified world/level. Sets parameters and inputs supplied in InParameters,
	 * InNodeInputs and InParameterInputs. If bInEnableAutoCook is true, cooks the HDA. If bInEnableAutoBake is
	 * true, bakes the cooked outputs according to the supplied baking parameters.
	 * This all happens asynchronously, with the various output pins firing at the various points in the process:
	 *	- PreInstantiation: before the HDA is instantiated, a good place to set parameter values before the first cook.
	 *	- PostInstantiation: after the HDA is instantiated, a good place to set/configure inputs before the first cook.
	 *	- PostAutoCook: right after a cook
	 *	- PreProcess: after a cook but before output objects have been created/processed
	 *	- PostProcessing: after output objects have been created
	 *	- PostAutoBake: after outputs have been baked
	 *	- Completed: upon successful completion (could be PostInstantiation if auto cook is disabled, PostProcessing
	 *				 if auto bake is disabled, or after PostAutoBake if auto bake is enabled.
	 *	- Failed: If the process failed at any point.
	 * @param InHoudiniAsset The HDA to instantiate.
	 * @param InInstantiateAt The Transform to instantiate the HDA with.
	 * @param InParameters The parameter values to set before cooking the instantiated HDA.
	 * @param InNodeInputs The node inputs to set before cooking the instantiated HDA.
	 * @param InParameterInputs The parameter-based inputs to set before cooking the instantiated HDA.
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
	 * @param bInDeleteInstantiatedAssetOnCompletionOrFailure If true, deletes the instantiated asset actor on
	 * completion or failure. Defaults to false.
	 * @return The blueprint async node.
	 */
	UFUNCTION(BlueprintCallable, meta=(AdvancedDisplay=5,AutoCreateRefTerm="InInstantiateAt,InParameters,InNodeInputs,InParameterInputs",BlueprintInternalUseOnly="true", WorldContext="WorldContextObject"), Category="Houdini|Public API")
	static UHoudiniPublicAPIProcessHDANode* ProcessHDA(
		UHoudiniAsset* InHoudiniAsset,
		const FTransform& InInstantiateAt,
		const TMap<FName, FHoudiniParameterTuple>& InParameters,
		const TMap<int32, UHoudiniPublicAPIInput*>& InNodeInputs,
		const TMap<FName, UHoudiniPublicAPIInput*>& InParameterInputs,
		UObject* InWorldContextObject=nullptr,
		ULevel* InSpawnInLevelOverride=nullptr,
		const bool bInEnableAutoCook=true,
		const bool bInEnableAutoBake=false,
		const FString& InBakeDirectoryPath="",
		const EHoudiniEngineBakeOption InBakeMethod=EHoudiniEngineBakeOption::ToActor,
		const bool bInRemoveOutputAfterBake=false,
		const bool bInRecenterBakedActors=false,
		const bool bInReplacePreviousBake=false,
		const bool bInDeleteInstantiatedAssetOnCompletionOrFailure=false);
	
	virtual void Activate() override;

	/**
	 * Delegate that is broadcast when entering the PreInstantiation state: the HDA's default parameter definitions are
	 * available, but the node has not yet been instantiated in HAPI/Houdini Engine
	 */
	UPROPERTY(BlueprintAssignable, Category="Houdini|Public API")
	FOnProcessHDANodeOutputPinDelegate PreInstantiation;

	/** Delegate that is broadcast after the asset was successfully instantiated */ 
	UPROPERTY(BlueprintAssignable, Category="Houdini|Public API")
	FOnProcessHDANodeOutputPinDelegate PostInstantiation;

	/**
	 * Delegate that is broadcast after a cook completes, but before outputs have been created. This will not be
	 * broadcast from this node if bInAutoCookEnabled is false.
	 */
	UPROPERTY(BlueprintAssignable, Category="Houdini|Public API")
	FOnProcessHDANodeOutputPinDelegate PostAutoCook;

	/**
	 * Delegate that is broadcast just after PostCook (after parameters have been updated) but before creating
	 * output objects. This will not be broadcast from this node if bInAutoCookEnabled is false.
	 */
	UPROPERTY(BlueprintAssignable, Category="Houdini|Public API")
	FOnProcessHDANodeOutputPinDelegate PreProcess;

	/**
	 * Delegate that is broadcast after processing HDA outputs after a cook, the output objects have been created.
	 * This will not be broadcast from this node if bInAutoCookEnabled is false.
	 */
	UPROPERTY(BlueprintAssignable, Category="Houdini|Public API")
	FOnProcessHDANodeOutputPinDelegate PostProcessing;

	/**
	 * Delegate that is broadcast after auto-baking the asset (not called for individual outputs that are baked to the
	 * content browser). This will not be broadcast from this node if bInAutoBake or bInAutoCookEnabled is false.
	 */
	UPROPERTY(BlueprintAssignable, Category="Houdini|Public API")
	FOnProcessHDANodeOutputPinDelegate PostAutoBake;

	/**
	 * Deletate that is broadcast on completion of async processing of the instantiated asset by this node.
	 * After this broadcast, the instantiated asset will be deleted if bInDeleteInstantiatedAssetOnCompletion=true
	 * was set on creation.
	 */
	UPROPERTY(BlueprintAssignable, Category="Houdini|Public API")
	FOnProcessHDANodeOutputPinDelegate Completed;

	/** Deletate that is broadcast if we fail during activation of the node. */
	UPROPERTY(BlueprintAssignable, Category="Houdini|Public API")
	FOnProcessHDANodeOutputPinDelegate Failed;

protected:

	// Output pin data

	/** The asset wrapper for the instantiated HDA processed by this node. */
	UPROPERTY()
	UHoudiniPublicAPIAssetWrapper* AssetWrapper;

	/** True if the last cook was successful. */
	UPROPERTY()
	bool bCookSuccess;

	/** True if the last bake was successful. */
	UPROPERTY()
	bool bBakeSuccess;

	// End: Output pin data

	/** The HDA to instantiate. */
	UPROPERTY()
	UHoudiniAsset* HoudiniAsset;

	/** The transform the instantiate the asset with. */
	UPROPERTY()
	FTransform InstantiateAt;

	/** The parameters to set on #PreInstantiation */
	UPROPERTY()
	TMap<FName, FHoudiniParameterTuple> Parameters;

	/** The node inputs to set on #PostInstantiation */
	UPROPERTY()
	TMap<int32, UHoudiniPublicAPIInput*> NodeInputs;

	/** The object path parameter inputs to set on #PostInstantiation */
	UPROPERTY()
	TMap<FName, UHoudiniPublicAPIInput*> ParameterInputs;

	/** The world context object: spawn in this world if #SpawnInLevelOverride is not set. */ 
	UPROPERTY()
	UObject* WorldContextObject;

	/** The level to spawn in. If both this and #WorldContextObject is not set, spawn in the editor context's level. */ 
	UPROPERTY()
	ULevel* SpawnInLevelOverride;

	/** Whether to set the instantiated asset to auto cook. */
	UPROPERTY()
	bool bEnableAutoCook;

	/** Whether to set the instantiated asset to auto bake after a cook. */
	UPROPERTY()
	bool bEnableAutoBake;
	
	/** Set the fallback bake directory, for if output attributes do not specify it. */
	UPROPERTY()
	FString BakeDirectoryPath;

	/** The bake method/target: for example, to actors vs to blueprints. */
	UPROPERTY()
	EHoudiniEngineBakeOption BakeMethod;

	/** Remove temporary HDA output after a bake. */
	UPROPERTY()
	bool bRemoveOutputAfterBake;

	/** Recenter the baked actors at their bounding box center. */
	UPROPERTY()
	bool bRecenterBakedActors;

	/**
	 * Replace previous bake output on each bake. For the purposes of this node, this would mostly apply to .uassets and
	 * not actors.
	 */
	UPROPERTY()
	bool bReplacePreviousBake;
	
	/** Whether or not to delete the instantiated asset after Complete is called. */
	UPROPERTY()
	bool bDeleteInstantiatedAssetOnCompletionOrFailure;

	/** Unbind all delegates */
	void UnbindDelegates();

	/** Broadcast Failure and removes the node from the root set. */
	UFUNCTION()
	virtual void HandleFailure();

	/** Broadcast Complete and removes the node from the root set. */
	UFUNCTION()
	virtual void HandleComplete();

	/**
	 * Bound to the asset wrapper's pre-instantiation delegate. Sets the HDAs parameters from #Parameters and
	 * broadcasts #PreInstantiation.
	 */
	UFUNCTION()
	virtual void HandlePreInstantiation(UHoudiniPublicAPIAssetWrapper* InAssetWrapper);

	/**
	 * Bound to the asset wrapper's post-instantiation delegate. Sets the HDAs inputs from #NodeInputs and
	 * #ParameterInputs and broadcasts #PostInstantiation.
	 */
	UFUNCTION()
	virtual void HandlePostInstantiation(UHoudiniPublicAPIAssetWrapper* InAssetWrapper);

	/** Bound to the asset wrapper's post-cook delegate. Broadcasts #PostAutoCook. */
	UFUNCTION()
	virtual void HandlePostAutoCook(UHoudiniPublicAPIAssetWrapper* InAssetWrapper, const bool bInCookSuccess);

	/** Bound to the asset wrapper's pre-processing delegate. Broadcasts #PreProcess. */
	UFUNCTION()
	virtual void HandlePreProcess(UHoudiniPublicAPIAssetWrapper* InAssetWrapper);

	/** Bound to the asset wrapper's post-processing delegate. Broadcasts #PostProcessing. */
	UFUNCTION()
	virtual void HandlePostProcessing(UHoudiniPublicAPIAssetWrapper* InAssetWrapper);

	/** Bound to the asset wrapper's post-bake delegate. Broadcasts #PostAutoBake. */
	UFUNCTION()
	virtual void HandlePostAutoBake(UHoudiniPublicAPIAssetWrapper* InAssetWrapper, const bool bInBakeSuccess);
};
