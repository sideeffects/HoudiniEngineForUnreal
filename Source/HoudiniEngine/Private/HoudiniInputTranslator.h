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

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "CoreMinimal.h"

class AActor;

class UHoudiniInput;
class UHoudiniParameter;
class UHoudiniAssetComponent;

class UHoudiniInputObject;
class UHoudiniInputStaticMesh;
class UHoudiniInputSkeletalMesh;
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

	// Upload data for an input's InputObject
	static bool UploadHoudiniInputObject(
		UHoudiniInput* InInput, UHoudiniInputObject* InInputObject, const FTransform& InActorTransform, TArray<int32>& OutCreatedNodeIds);
	
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
	
	static bool	HapiCreateInputNodeForStaticMesh(
		const FString& InObjNodeName,
		UHoudiniInputStaticMesh* InObject,
		const bool& bExportLODs,
		const bool& bExportSockets,
		const bool& bExportColliders,
		const bool& bImportAsReference = false,
		const bool& bImportAsReferenceRotScaleEnabled = false);

	static bool	HapiCreateInputNodeForHoudiniSplineComponent(
		const FString& InObjNodeName,
		UHoudiniInputHoudiniSplineComponent* InObject,
		bool bInSetRotAndScaleAttributes,
		bool bInUseLegacyInputCurves);

	static bool	HapiCreateInputNodeForLandscape(
		const FString& InObjNodeName,
		UHoudiniInputLandscape* InObject,
		UHoudiniInput* InInput);

	static bool HapiCreateInputNodeForSkeletalMesh(
		const FString& InObjNodeName, UHoudiniInputSkeletalMesh* InObject);
	static bool HapiCreateInputNodeForGeometryCollection(
		const FString& InObjNodeName, UHoudiniInputGeometryCollection* InObject, const bool& bImportAsReference);

	static bool HapiCreateInputNodeForGeometryCollectionComponent(
		const FString& InObjNodeName, UHoudiniInputGeometryCollectionComponent* InObject, const bool& bImportAsReference);

	static bool HapiCreateInputNodeForGeometryCollectionActor(
		const FString& InObjNodeName, UHoudiniInputGeometryCollectionActor* InObject, const bool& bImportAsReference);
	
	static bool	HapiCreateInputNodeForSceneComponent(
		const FString& InObjNodeName, UHoudiniInputSceneComponent* InObject);

	static bool	HapiCreateInputNodeForStaticMeshComponent(
		const FString& InObjNodeName,
		UHoudiniInputMeshComponent* InObject,
		const bool& bExportLODs,
		const bool& bExportSockets,
		const bool& bExportColliders,
		const bool& bKeepWorldTransform,
		const bool& bImportAsReference,
		const bool& bImportAsReferenceRotScaleEnabled = false,
		const FTransform& InActorTransform = FTransform::Identity);

	static bool	HapiCreateInputNodeForInstancedStaticMeshComponent(
		const FString& InObjNodeName,
		UHoudiniInputInstancedMeshComponent* InObject,
		const bool& bExportLODs,
		const bool& bExportSockets,
		const bool& bExportColliders);

	static bool	HapiCreateInputNodeForSplineComponent(
		const FString& InObjNodeName, UHoudiniInputSplineComponent* InObject, const float& SplineResolution);

	static bool	HapiCreateInputNodeForHoudiniAssetComponent(
		const FString& InObjNodeName, UHoudiniInputHoudiniAsset* InObject, const bool bKeepWorldTransform, const bool& bImportAsReference, const bool& bImportAsReferenceRotScaleEnabled);

	static bool	HapiCreateInputNodeForActor(
		UHoudiniInput* InInput, UHoudiniInputActor* InObject, const FTransform & InActorTransform, TArray<int32>& OutCreatedNodeIds);

	static bool HapiCreateInputNodeForCamera(
		const FString& InObjNodeName, UHoudiniInputCameraComponent* InObject);
	
	// Create input node for Brush. Optionally exclude actors when combining
	// brush with other intersecting brushes. This is typically used to 
	// exclude Selector objects.
	static bool	HapiCreateInputNodeForBrush(
		const FString& InObjNodeName, 
		UHoudiniInputBrush* InObject, 
		TArray<AActor*>* ExcludeActors
	);

	static bool HapiCreateInputNodeForDataTable(
		const FString& InNodeName, UHoudiniInputDataTable* InInputObject);

	static bool	HapiCreateInputNodeForFoliageType_InstancedStaticMesh(
		const FString& InObjNodeName,
		UHoudiniInputFoliageType_InstancedStaticMesh* InObject,
		const bool& bExportLODs,
		const bool& bExportSockets,
		const bool& bExportColliders,
		const bool& bImportAsReference = false,
		const bool& bImportAsReferenceRotScaleEnabled = false);

	// HAPI: Create an input node for reference
	static bool CreateInputNodeForReference(
		HAPI_NodeId& InputNodeId,
		const FString & InRef,
		const FString & InputNodeName,
		const FTransform & InTransform,
		const bool& bImportAsReferenceRotScaleEnabled);

	//static bool HapiUpdateInputNodeTransform(const HAPI_NodeId InputNodeId, const FTransform& Transform);

};