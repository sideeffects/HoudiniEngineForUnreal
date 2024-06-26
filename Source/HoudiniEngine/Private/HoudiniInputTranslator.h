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

#include "HAPI/HAPI_Common.h"
#include "CoreMinimal.h"

#include "HoudiniEnginePrivatePCH.h"

struct FHoudiniUnrealDataLayersCache;
class AActor;

class UHoudiniInput;
class UHoudiniParameter;
class UHoudiniAssetComponent;

class UHoudiniInputObject;
class UHoudiniInputStaticMesh;
class UHoudiniInputSkeletalMesh;
class UHoudiniInputAnimation;
class UHoudiniInputSkeletalMeshComponent;
class UHoudiniInputGeometryCollection;
class UHoudiniInputGeometryCollectionComponent;
class UHoudiniInputGeometryCollectionActor;
class UHoudiniInputSceneComponent;
class UHoudiniInputMeshComponent;
class UHoudiniInputInstancedMeshComponent;
class UHoudiniInputSplineComponent;
class UHoudiniInputHoudiniSplineComponent;
class UHoudiniInputHoudiniAsset;
class UHoudiniInputActor;
class UHoudiniInputLandscape;
class UHoudiniInputBrush;
class UHoudiniSplineComponent;
class UHoudiniInputCameraComponent;
class UHoudiniInputDataTable;
class UHoudiniInputFoliageType_InstancedStaticMesh;
class UHoudiniInputBlueprint;
class UHoudiniInputLandscapeSplinesComponent;
class UHoudiniInputSplineMeshComponent;
class UHoudiniInputLevelInstance;
class UHoudiniInputPackedLevelActor;
class FUnrealObjectInputHandle;
class FUnrealObjectInputIdentifier;

struct FHoudiniInputObjectSettings;

class AActor;

enum class EHoudiniInputType : uint8;
enum class EHoudiniLandscapeExportType : uint8;

struct HOUDINIENGINE_API FHoudiniInputTranslator
{
	// 
	static bool UpdateInputs(UHoudiniAssetComponent* HAC);

	// Update inputs from the asset
	// @AssetId: NodeId of the digital asset
	// @OuterObject: Object to use for transactions and as Outer for new inputs
	// @CurrentInputs: pre: current & post: invalid inputs
	// @NewParameters: pre: empty & post: new inputs
	// On Return: CurrentInputs are the old inputs that are no longer valid,
	// NewInputs are new and re-used inputs.
	static bool BuildAllInputs(
		const HAPI_NodeId& AssetId,
		class UObject* OuterObject,
		TArray<UHoudiniInput*>& Inputs,
		TArray<UHoudiniParameter*>& Parameters);

	// Update loaded inputs and their input objects so they can be uploaded properly
	static bool	UpdateLoadedInputs(UHoudiniAssetComponent * HAC);

	// Update all the inputs that have been marked as change
	static bool UploadChangedInputs(UHoudiniAssetComponent * HAC);

	// Only update simple input properties
	static bool UpdateInputProperties(UHoudiniInput* InInput);

	// Update the KeepWorldTransform / Object merge transform type property
	static bool UpdateTransformType(UHoudiniInput* InInput);

	// Update the pack before merge parameter for World/Geometry inputs
	static bool UpdatePackBeforeMerge(UHoudiniInput* InInput);
	
	// Update the transform offset for geometry inputs
	static bool UpdateTransformOffset(UHoudiniInput* InInput);

	// Upload all the input's data to Houdini
	static bool UploadInputData(UHoudiniInput* InInput, const FTransform & InActorTransform = FTransform::Identity);

	// Upload all the input's transforms to Houdini
	static bool UploadInputTransform(UHoudiniInput* InInput);

	// Upload all the input's data layers to Houdini
	static bool UploadDataLayers(UHoudiniInput* InInput, FHoudiniUnrealDataLayersCache* Cache);

	// Upload data for an input's InputObject
	static bool UploadHoudiniInputObject(
		UHoudiniInput* InInput, 
		UHoudiniInputObject* InInputObject, 
		const FTransform& InActorTransform, 
		TArray<int32>& OutCreatedNodeIds,
		TSet<FUnrealObjectInputHandle>& OutHandles,
		const bool& bInputNodesCanBeDeleted);
	
	// Upload transform for an input's InputObject
	static bool UploadHoudiniInputTransform(
		UHoudiniInput* InInput, UHoudiniInputObject* InInputObject);

	// Updates/ticks world inputs in the given HAC
	static bool UpdateWorldInputs(UHoudiniAssetComponent* HAC);

	// Updates/ticks the given world input
	static bool UpdateWorldInput(UHoudiniInput* InInput);

	// Connect an input's nodes to its linked HDA node
	static bool ConnectInputNode(UHoudiniInput* InInput);

	// Destroys an input
	static bool DisconnectAndDestroyInput(UHoudiniInput* InputToDestroy, const EHoudiniInputType& InputType);
	static bool DisconnectInput(UHoudiniInput* InputToDestroy, const EHoudiniInputType& InputType);
	static bool DestroyInputNodes(UHoudiniInput* InputToDestroy, const EHoudiniInputType& InputType);

	static EHoudiniInputType GetDefaultInputTypeFromLabel(const FString& InputName);

	static bool SetDefaultAssetFromHDA(UHoudiniInput* Input, bool& bOutBlueprintStructureModified);

	static bool ChangeInputType(UHoudiniInput* Input, const bool& bForce);

	static bool HapiCreateInputNodeForObject(
		const FString& InObjNodeName, UHoudiniInputObject* InObject);
	
	static bool HapiSetGeoObjectTransform(const HAPI_NodeId& InObjectNodeId, const FTransform& InTransform);

	static bool HapiCreateOrUpdateGeoObjectMergeAndSetTransform(
		const int32 InParentNodeId,
		const HAPI_NodeId& InNodeToObjectMerge,
		const FString& InObjNodeName,
		HAPI_NodeId& InOutObjectMergeNodeId,
		HAPI_NodeId& InOutGeoObjectNodeId,
		const bool bInCreateIfMissingInvalid=true,
		const FTransform& InTransform=FTransform::Identity,
		const int32& InTransformType=-1);
	
	static bool	HapiCreateInputNodeForStaticMesh(
		const FString& InObjNodeName,
		UHoudiniInputStaticMesh* InObject,
		const FHoudiniInputObjectSettings& InInputSettings,
		const bool& bInputNodesCanBeDeleted);

	static bool	HapiCreateInputNodeForHoudiniSplineComponent(
		const FString& InObjNodeName,
		UHoudiniInputHoudiniSplineComponent* InObject,
		const FHoudiniInputObjectSettings& InInputSettings);

	static bool HapiCreateInputNodeForLandscapeSplinesComponent(
		const FString& InObjNodeName,
		UHoudiniInputLandscapeSplinesComponent* const InObject,
		const FHoudiniInputObjectSettings& InInputSettings,
		TArray<int32>& OutCreatedNodeIds,
		TSet<FUnrealObjectInputHandle>& OutHandles,
		const bool bInInputNodesCanBeDeleted);

	static bool	HapiCreateInputNodeForLandscape(
		const FString& InObjNodeName,
		UHoudiniInputLandscape* InObject,
		UHoudiniInput* InInput,
		TArray<int32>& OutCreatedNodeIds,
		TSet<FUnrealObjectInputHandle>& OutHandles,
		const bool& bInputNodesCanBeDeleted);

	static bool	HapiCreateInputNodeForLevelInstance(
		const FString& InObjNodeName,
		UHoudiniInputLevelInstance* InObject,
		const FHoudiniInputObjectSettings& InInputSettings,
		UHoudiniInput* InInput,
		TArray<int32>& OutCreatedNodeIds,
		TSet<FUnrealObjectInputHandle>& OutHandles,
		const bool& bInputNodesCanBeDeleted);

	static bool HapiCreateInputNodeForPackedLevelActor(
		const FString& InObjNodeName,
		UHoudiniInputPackedLevelActor* InObject,
		const FHoudiniInputObjectSettings& InInputSettings,
		UHoudiniInput* InInput,
		TArray<int32>& OutCreatedNodeIds,
		TSet<FUnrealObjectInputHandle>& OutHandles,
		const bool& bInputNodesCanBeDeleted);

	static bool HapiCreateInputNodeForSkeletalMesh(
		const FString& InObjNodeName,
		UHoudiniInputSkeletalMesh* InObject,
		const FHoudiniInputObjectSettings& InInputSettings,
		const bool& bInputNodesCanBeDeleted);

	static bool HapiCreateInputNodeForAnimation(
		const FString& InObjNodeName,
		UHoudiniInputAnimation* InObject,
		const FHoudiniInputObjectSettings& InInputSettings,
		const bool& bInputNodesCanBeDeleted);

	static bool HapiCreateInputNodeForSkeletalMeshComponent(
		const FString& InObjNodeName,
		UHoudiniInputSkeletalMeshComponent* InObject,
		const FHoudiniInputObjectSettings& InInputSettings,
		const bool& bInputNodesCanBeDeleted);

	static bool HapiCreateInputNodeForGeometryCollection(
		const FString& InObjNodeName,
		UHoudiniInputGeometryCollection* InObject,
		const FHoudiniInputObjectSettings& InInputSettings,
		const bool& bInputNodesCanBeDeleted);

	static bool HapiCreateInputNodeForGeometryCollectionComponent(
		const FString& InObjNodeName,
		UHoudiniInputGeometryCollectionComponent* InObject,
		const FHoudiniInputObjectSettings& InInputSettings,
		const bool& bInputNodesCanBeDeleted);
	
	static bool	HapiCreateInputNodeForSceneComponent(
		const FString& InObjNodeName,
		UHoudiniInputSceneComponent* InObject,
		const FHoudiniInputObjectSettings& InInputSettings,
		const bool& bInputNodesCanBeDeleted);

	static bool	HapiCreateInputNodeForStaticMeshComponent(
		const FString& InObjNodeName,
		UHoudiniInputMeshComponent* InObject,
		const FHoudiniInputObjectSettings& InInputSettings,
		const bool& bInputNodesCanBeDeleted);

	static bool	HapiCreateInputNodeForSplineMeshComponents(
		const FString& InObjNodeName,
		UHoudiniInputActor* InParentActorObject,
		const FHoudiniInputObjectSettings& InInputSettings,
		const bool& bInputNodesCanBeDeleted);
	
	static bool	HapiCreateInputNodeForInstancedStaticMeshComponent(
		const FString& InObjNodeName,
		UHoudiniInputInstancedMeshComponent* InObject,
		const FHoudiniInputObjectSettings& InInputSettings,
		const bool& bInputNodesCanBeDeleted);

	static bool	HapiCreateInputNodeForSplineComponent(
		const FString& InObjNodeName,
		UHoudiniInputSplineComponent* InObject,
		const FHoudiniInputObjectSettings& InInputSettings,
		const bool& bInputNodesCanBeDeleted);

	static bool	HapiCreateInputNodeForHoudiniAssetComponent(
		const FString& InObjNodeName,
		UHoudiniInputHoudiniAsset* InObject,
		const FHoudiniInputObjectSettings& InInputSettings);

	static bool HapiCreateInputNodesForActorComponents(
		UHoudiniInput* const InInput,
		UHoudiniInputActor* const InInputActorObject,
		AActor* const InActor,
		const FTransform& InActorTransform,
		TArray<int32>& OutCreatedNodeIds,
		TSet<FUnrealObjectInputHandle>& OutHandles,
		const bool& bInputNodesCanBeDeleted);

	static bool	HapiCreateInputNodeForActor(
		UHoudiniInput* InInput, 
		UHoudiniInputActor* InObject,
		const FTransform & InActorTransform,
		TArray<int32>& OutCreatedNodeIds,
		TSet<FUnrealObjectInputHandle>& OutHandles,
		const bool& bInputNodesCanBeDeleted);

	static bool HapiCreateInputNodeForBP(
		UHoudiniInput* InInput,
		UHoudiniInputBlueprint* InObject,
		TArray<int32>& OutCreatedNodeIds,
		TSet<FUnrealObjectInputHandle>& OutHandles,
		const bool& bInputNodesCanBeDeleted);

	static bool HapiCreateInputNodeForCamera(
		const FString& InObjNodeName,
		UHoudiniInputCameraComponent* InObject,
		const FHoudiniInputObjectSettings& InInputSettings);

	// Create input node for Brush. Optionally exclude actors when combining
	// brush with other intersecting brushes. This is typically used to 
	// exclude Selector objects.
	static bool	HapiCreateInputNodeForBrush(
		const FString& InObjNodeName, 
		UHoudiniInputBrush* InObject, 
		TArray<AActor*>* ExcludeActors,
		const FHoudiniInputObjectSettings& InInputSettings,
		const bool& bInputNodesCanBeDeleted);

	static bool HapiCreateInputNodeForDataTable(
		const FString& InNodeName,
		UHoudiniInputDataTable* InInputObject,
		const FHoudiniInputObjectSettings& InInputSettings,
		const bool& bInputNodesCanBeDeleted);

	static bool	HapiCreateInputNodeForFoliageType_InstancedStaticMesh(
		const FString& InObjNodeName,
		UHoudiniInputFoliageType_InstancedStaticMesh* InObject,
		const FHoudiniInputObjectSettings& InInputSettings,
		const bool& bInputNodesCanBeDeleted);

	// Import as reference, wrapper function
	static bool HapiCreateInputNodeForReference(
		const FString& InObjNodeName,
		UHoudiniInputObject* InObject,
		const FHoudiniInputObjectSettings& InInputSettings,
		const bool& bInputNodesCanBeDeleted);

	static bool HapiCreateInputNodeForActorReference(
		UHoudiniInputActor* InActorObject,
		const FHoudiniInputObjectSettings& InInputSettings,
		const bool& bInputNodesCanBeDeleted);

	// HAPI: Create an input node for reference
	static bool CreateInputNodeForReference(
		const HAPI_NodeId InParentNodeId,
		HAPI_NodeId& InputNodeId,
		const FString& InRef,
		const FString& InputNodeName,
		const FTransform& InTransform,
		const bool& bImportAsReferenceRotScaleEnabled,
		const bool& bImportAsReferenceBboxEnabled = false,
		const FBox& InBbox = FBox(EForceInit::ForceInit),
		const bool& bImportAsReferenceMaterialEnabled = false,
		const TArray<FString>& MaterialReferences = TArray<FString>());

	// HAPI: Create an input node for reference
	static bool CreateInputNodeForReference(
		HAPI_NodeId& InputNodeId,
		UObject const* const InObjectToRef,
		const FString& InputNodeName,
		const FTransform& InTransform,
		const bool& bImportAsReferenceRotScaleEnabled,
		FUnrealObjectInputHandle& OutHandle,
		const bool& bInputNodesCanBeDeleted,
		const bool& bImportAsReferenceBboxEnabled = false,
		const FBox& InBbox = FBox(EForceInit::ForceInit),
		const bool& bImportAsReferenceMaterialEnabled = false,
		const TArray<FString>& MaterialReferences = TArray<FString>());

	//static bool HapiUpdateInputNodeTransform(const HAPI_NodeId InputNodeId, const FTransform& Transform);

	// Create a Merge SOP. If InInOutMergeNodeId is set to >= 0 then the node will be deleted, if valid, after the
	// merge SOP is created. The inputs of the Merge SOP are set to InNodeIdsToConnect. 
	static bool CreateMergeSOP(
		HAPI_NodeId& InOutMergeNodeId,
		const TArray<HAPI_NodeId>& InNodeIdsToConnect,
		const FString& InMergeNodeName);

	// Set InNodeIdsToConenct as the inputs of the Merge SOP InMergeNodeId.
	static bool SetMergeSOPInputs(const HAPI_NodeId InMergeNodeId, const TArray<HAPI_NodeId>& InNodeIdsToConnect);
};

