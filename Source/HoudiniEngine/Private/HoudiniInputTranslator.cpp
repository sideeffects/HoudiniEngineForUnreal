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

#include "HoudiniInputTranslator.h"

#include "HCsgUtils.h"
#include "HoudiniApi.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniDataLayerUtils.h"
#include "HoudiniEngine.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineString.h"
#include "HoudiniInput.h"
#include "HoudiniInputObject.h"
#include "HoudiniMeshUtils.h"
#include "HoudiniNodeSyncComponent.h"
#include "HoudiniOutputTranslator.h"
#include "HoudiniParameter.h"
#include "HoudiniParameterOperatorPath.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniSplineTranslator.h"
#include "UnrealAnimationTranslator.h"
#include "UnrealBrushTranslator.h"
#include "UnrealDataTableTranslator.h"
#include "UnrealFoliageTypeTranslator.h"
#include "UnrealGeometryCollectionTranslator.h"
#include "UnrealInstanceTranslator.h"
#include "UnrealLandscapeSplineTranslator.h"
#include "UnrealLandscapeTranslator.h"
#include "UnrealLevelInstanceTranslator.h"
#include "UnrealMeshTranslator.h"
#include "UnrealObjectInputManager.h"
#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputRuntimeUtils.h"
#include "UnrealObjectInputTypes.h"
#include "UnrealObjectInputUtils.h"
#include "UnrealSkeletalMeshTranslator.h"
#include "UnrealSplineTranslator.h"

#include "Animation/AnimSequence.h"
#include "Async/Async.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Brush.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniHLODLayerUtils.h"
#include "GeometryCollection/GeometryCollection.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeSplinesComponent.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "PackedLevelActor/PackedLevelActor.h"
#include "UObject/TextProperty.h"

#if WITH_EDITOR
	#include "Editor.h"
	#include "Editor/UnrealEdEngine.h"
	#include "UnrealEdGlobals.h"
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	#include "MeshMerge/MeshMergingSettings.h"
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	#include "GeometryCollection/GeometryCollectionActor.h"
	#include "GeometryCollection/GeometryCollectionComponent.h"
	#include "GeometryCollection/GeometryCollectionObject.h"
#else
	#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionActor.h"
	#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionComponent.h"
	#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionObject.h"	
#endif

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

#if WITH_EDITOR
// Allows checking of objects currently being dragged around
struct FHoudiniMoveTracker
{
	FHoudiniMoveTracker() : IsObjectMoving(false)
	{		
		GEditor->OnBeginObjectMovement().AddLambda([this](UObject&) { IsObjectMoving = true; });
		GEditor->OnEndObjectMovement().AddLambda([this](UObject&) { IsObjectMoving = false; });

		GEditor->OnActorsMoved().AddLambda([this](TArray<AActor*>&) { IsObjectMoving = false; });

		GEditor->OnBeginCameraMovement().AddLambda([this](UObject&) { IsObjectMoving = false; });
		GEditor->OnEndCameraMovement().AddLambda([this](UObject&) { IsObjectMoving = false; });
	}
	static FHoudiniMoveTracker& Get() { static FHoudiniMoveTracker Instance; return Instance; }

	bool IsObjectMoving;
};
#endif

// 
bool
FHoudiniInputTranslator::UpdateInputs(UHoudiniAssetComponent* HAC)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::UpdateInputs);

	if (!IsValid(HAC))
		return false;

	// Nothing to do for Node Sync Components!
	if (HAC->IsA<UHoudiniNodeSyncComponent>())
		return true;

	if (!FHoudiniInputTranslator::BuildAllInputs(HAC->GetAssetId(), HAC, HAC->Inputs, HAC->Parameters))
	{
		// Failed to create the inputs
		return false;
	}

	return true;
}

bool
FHoudiniInputTranslator::BuildAllInputs(
	const HAPI_NodeId& AssetId,
	class UObject* InOuterObject,
	TArray<UHoudiniInput*>& Inputs,
	TArray<UHoudiniParameter*>& Parameters)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::BuildAllInputs);

	// Ensure the asset has a valid node ID
	if (AssetId < 0)
	{
		return false;
	}

	// Start by getting the asset's info
	HAPI_AssetInfo AssetInfo;
	bool bAssetInfoSuccess = (HAPI_RESULT_SUCCESS == FHoudiniApi::GetAssetInfo(
		FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo));

	// Get the number of geo (SOP) inputs
	// It's best to update the input count even if the hda hasnt cooked
	// as it can cause loaded geo inputs to disappear upon loading the level
	int32 InputCount = bAssetInfoSuccess ? AssetInfo.geoInputCount : 0;

	// Also look for object path parameters inputs
	// Helper map to get the parameter index, given the parameter name
	TMap<FString, int32> ParameterNameToIndexMap;
	TArray<TWeakObjectPtr<UHoudiniParameter>> InputParameters;
	TArray<FString> InputParameterNames;
	for (auto Param : Parameters)
	{
		if (!Param)
			continue;
		
		if (Param->GetParameterType() == EHoudiniParameterType::Input)
		{
			int InsertionIndex = InputParameters.Num();
			ParameterNameToIndexMap.Add(Param->GetParameterName(), InsertionIndex);
			InputParameters.Add(Param);
			InputParameterNames.Add(Param->GetParameterName());
		}
	}

	InputCount += InputParameters.Num();

	// Append new inputs as needed
	if (InputCount > Inputs.Num())
	{
		int32 NumNewInputs = InputCount - Inputs.Num();
		for (int32 InputIdx = Inputs.Num(); InputIdx < InputCount; ++InputIdx)
		{
			FString InputObjectName = TEXT("Input") + FString::FromInt(InputIdx + 1);
			UHoudiniInput * NewInput = NewObject< UHoudiniInput >(
				InOuterObject,
				UHoudiniInput::StaticClass(),
				FName(*InputObjectName),
				RF_Transactional);

			if (!IsValid(NewInput))
			{
				//HOUDINI_LOG_WARNING("Failed to create asset input");
				continue;
			}
			// Create a default curve object here to avoid Transaction issue
			//NewInput->CreateDefaultCurveInputObject();

			Inputs.Add(NewInput);
		}			
	}
	else if (InputCount < Inputs.Num())
	{
		// DO NOT DELETE PARAM INPUTS THAT ARE STILL PRESENT!
		// This can ause issues with some input type when recooking the HDA after removing inputs!
		// Make sure that we only delete inputs that are not present anymore!
		for (int32 InputIdx = Inputs.Num() - 1; InputIdx >= 0; InputIdx--)
		{
			UHoudiniInput* CurrentInput = Inputs[InputIdx];
			if (IsValid(CurrentInput))
			{
				// Do not delete a param input that is still present!
				if (CurrentInput->IsObjectPathParameter()
					&& InputParameterNames.Contains(CurrentInput->GetInputName()))
					continue;

				FHoudiniInputTranslator::DisconnectAndDestroyInput(CurrentInput, CurrentInput->GetInputType());

				// DO NOT MANUALLY DESTROY THE OLD/DANGLING INPUTS!
				// This messes up unreal's Garbage collection and would cause crashes on duplication
				//CurrentInput->ConditionalBeginDestroy();
				//CurrentInput = nullptr;
			}

			Inputs.RemoveAt(InputIdx);

			// Stop deleting inputs once we've removed enough
			if (Inputs.Num() <= InputCount)
				break;
		}

		Inputs.SetNum(InputCount);
	}

	// Input index -> InputParameter index
	// Special values: -1 = SOP input. Ignore completely. -2 = To be determined later
	// Used to preserve inputs after insertion/deletion
	TArray<int32> InputIdxToInputParamIndex;
	InputIdxToInputParamIndex.SetNum(Inputs.Num());

	// Keep a set of used indices, to figure out the unused indices later
	TSet<int32> UsedParameterIndices;
	for (int32 InputIdx = 0; InputIdx < Inputs.Num(); InputIdx++)
	{
		// SOP input -> Parameter map doesn't make sense - ignore this
		if (InputIdx < AssetInfo.geoInputCount)
		{
			// Ignore completely
			InputIdxToInputParamIndex[InputIdx] = -1;
		}
		else
		{
			UHoudiniInput* CurrentInput = Inputs[InputIdx];
			if (!IsValid(CurrentInput))
				continue;

			if (ParameterNameToIndexMap.Contains(CurrentInput->GetInputName()))
			{
				const int32 ParameterIndex = ParameterNameToIndexMap[CurrentInput->GetInputName()];
				InputIdxToInputParamIndex[InputIdx] = ParameterIndex;
				UsedParameterIndices.Add(ParameterIndex);
			}
			else
			{
				// To be determined in the second pass
				InputIdxToInputParamIndex[InputIdx] = -2;
			}
		}
	}

	// Second pass for InputIdxToInputParamIndex
	// Fill in the inputs that could not be mapped onto old inputs. Used when inserting a new element.
	for (int32 NewInputIndex = 0; NewInputIndex < Inputs.Num(); NewInputIndex++)
	{
		if (InputIdxToInputParamIndex[NewInputIndex] == -2)
		{
			// Find the first free index
			for (int32 FreeIdx = 0; FreeIdx < InputParameters.Num(); FreeIdx++)
			{
				if (!UsedParameterIndices.Contains(FreeIdx))
				{
					InputIdxToInputParamIndex[NewInputIndex] = FreeIdx;
					UsedParameterIndices.Add(FreeIdx);
					break;
				}
			}
		}
	}

	// Now, check the inputs in the array match the geo inputs
	//for (int32 GeoInIdx = 0; GeoInIdx < AssetInfo.geoInputCount; GeoInIdx++)
	bool bBlueprintStructureChanged = false;
	for (int32 InputIdx = 0; InputIdx < Inputs.Num(); InputIdx++)
	{
		UHoudiniInput* CurrentInput = Inputs[InputIdx];
		if (!IsValid(CurrentInput))
			continue;

		// Create default Name/Label/Help
		FString CurrentInputName = TEXT("Input") + FString::FromInt(InputIdx + 1);
		FString CurrentInputLabel = CurrentInputName;
		FString CurrentInputHelp;

		// Set the nodeId
		CurrentInput->SetAssetNodeId(AssetId);

		// Is this an object path parameter input?
		bool bIsObjectPath = InputIdx >= AssetInfo.geoInputCount;
		if (!bIsObjectPath)
		{
			// Mark this input as a SOP input
			CurrentInput->SetSOPInput(InputIdx);

			// Get and set the name		
			HAPI_StringHandle InputStringHandle;
			if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetNodeInputName(
				FHoudiniEngine::Get().GetSession(),
				AssetId, InputIdx, &InputStringHandle))
			{
				FHoudiniEngineString HoudiniEngineString(InputStringHandle);
				HoudiniEngineString.ToFString(CurrentInputLabel);
			}
		}
		else
		{
			// Get this input's parameter index in the objpath param array
			int32 CurrentParmIdx = InputIdxToInputParamIndex[InputIdx];
			
			UHoudiniParameter* CurrentParm = nullptr;
			if (InputParameters.IsValidIndex(CurrentParmIdx))
			{
				if (InputParameters[CurrentParmIdx].IsValid())
					CurrentParm = InputParameters[CurrentParmIdx].Get();
			}

			int32 ParmId = -1;
			if (IsValid(CurrentParm))
			{
				ParmId = CurrentParm->GetParmId();
				CurrentInputName = CurrentParm->GetParameterName();
				CurrentInputLabel = CurrentParm->GetParameterLabel();
				CurrentInputHelp = CurrentParm->GetParameterHelp();
			}

			UHoudiniParameterOperatorPath* CurrentObjPathParm = Cast<UHoudiniParameterOperatorPath>(CurrentParm);
			if (IsValid(CurrentObjPathParm))
			{
				CurrentObjPathParm->HoudiniInput = CurrentInput;
			}

			// Mark this input as an object path parameter input
			CurrentInput->SetObjectPathParameter(ParmId);
		}

		CurrentInput->SetName(CurrentInputName);
		CurrentInput->SetLabel(CurrentInputLabel);

		if ( CurrentInputHelp.IsEmpty() )
		{
			CurrentInputHelp = CurrentInputLabel + TEXT("(") + CurrentInputName + TEXT(")");
		}
		CurrentInput->SetHelp(CurrentInputHelp);

		// If the input type is invalid, 
		// We need to initialize its default
		if (CurrentInput->GetInputType() == EHoudiniInputType::Invalid)
		{
			// Initialize it to the default corresponding to its name
			CurrentInput->SetInputType(GetDefaultInputTypeFromLabel(CurrentInputLabel), bBlueprintStructureChanged);

			// Preset the default HDA for objpath input
			SetDefaultAssetFromHDA(CurrentInput, bBlueprintStructureChanged);
		}

		// Update input objects data on UE side for all types of inputs.
		switch (CurrentInput->GetInputType())
		{
			case EHoudiniInputType::Curve:
				FHoudiniSplineTranslator::UpdateHoudiniInputCurves(CurrentInput);
				break;
			case EHoudiniInputType::Geometry:
				break;
			case EHoudiniInputType::World:
				break;
			default:
				break;
		}
	}

	return true;
}

bool
FHoudiniInputTranslator::DisconnectInput(UHoudiniInput* InputToDestroy, const EHoudiniInputType& InputType)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::DisconnectInput);

	if (!IsValid(InputToDestroy))
		return false;

	// Start by disconnecting the input / nullifying the object path parameter
	if (InputToDestroy->IsObjectPathParameter())
	{
		// Just set the objpath parameter to null
		FHoudiniApi::SetParmStringValue(
			FHoudiniEngine::Get().GetSession(),
			InputToDestroy->GetAssetNodeId(), "",
			InputToDestroy->GetParameterId(), 0);
	}
	else
	{
		// Get the asset / created input node ID
		HAPI_NodeId HostAssetId = InputToDestroy->GetAssetNodeId();
		HAPI_NodeId CreatedInputId = InputToDestroy->GetInputNodeId();

		// Only disconnect if both are valid
		if (HostAssetId >= 0 && CreatedInputId >= 0)
		{
			FHoudiniApi::DisconnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				HostAssetId, InputToDestroy->GetInputIndex());
		}
	}

	return true;
}

bool
FHoudiniInputTranslator::DestroyInputNodes(UHoudiniInput* InputToDestroy, const EHoudiniInputType& InputType)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::DestroyInputNodes);

	if (!IsValid(InputToDestroy))
		return false;

	if (!InputToDestroy->CanDeleteHoudiniNodes())
		return false;

	// When using the new input system, get all HAPI NodeIds managed by the system as a set. Do not delete any nodes
	// here if their ids are in the set. The manager will handle deletion of those nodes when needed.
	IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	TSet<int32> ManagedNodeIdSet;
	if (Manager)
	{
		TArray<int32> ManagedNodeIds;
		Manager->GetAllHAPINodeIds(ManagedNodeIds);
		ManagedNodeIdSet.Append(ManagedNodeIds);
	}
	
	// Destroy the nodes created by all the input objects
	TArray<int32> CreatedInputDataAssetIds = InputToDestroy->GetCreatedDataNodeIds();
	TArray<UHoudiniInputObject*>* InputObjectNodes = InputToDestroy->GetHoudiniInputObjectArray(InputType);
	if (InputObjectNodes)
	{
		TArray<int32> ManagedHAPINodeIds;
		for (auto CurInputObject : *InputObjectNodes)
		{
			if (!IsValid(CurInputObject))
				continue;

			if (CurInputObject->Type == EHoudiniInputObjectType::HoudiniAssetComponent)
			{
				// Houdini Asset Input, we don't want to destroy / invalidate the input HDA!
				// Just remove this input object's node Id from the CreatedInputDataAssetIds array
				// to avoid its deletion further down
				const int32 InputNodeId = CurInputObject->GetInputNodeId();
				if (InputNodeId >= 0)
					CreatedInputDataAssetIds.Remove(InputNodeId);
				//CurInputObject->InputNodeId = -1;
				//CurInputObject->InputObjectNodeId = -1;
				continue;
			}

			// For Actor/BP input objects, set the input node id for all component objects to -1,
			if (CurInputObject->Type == EHoudiniInputObjectType::Actor || CurInputObject->Type == EHoudiniInputObjectType::Blueprint)
			{
				UHoudiniInputActor* CurActorInputObject = Cast<UHoudiniInputActor>(CurInputObject);
				UHoudiniInputBlueprint* CurBPInputObject = Cast<UHoudiniInputBlueprint>(CurInputObject);
				if (CurActorInputObject || CurBPInputObject) 
				{
					for (auto & CurComponent : CurActorInputObject ? CurActorInputObject->GetActorComponents() : CurBPInputObject->GetComponents())
					{
						if (!IsValid(CurComponent))
							continue;

						if (!CurComponent->CanDeleteHoudiniNodes())
						{
							const int32 InputNodeId = CurComponent->GetInputNodeId();
							if (InputNodeId >= 0)
								CreatedInputDataAssetIds.Remove(InputNodeId);
							const int32 InputObjectNodeId = CurComponent->GetInputObjectNodeId();
							if (InputObjectNodeId >= 0)
								CreatedInputDataAssetIds.Remove(InputObjectNodeId);
							continue;
						}

						// No need to delete the nodes created for an asset component manually here,
						// As they will be deleted when we clean up the CreateNodeIds array
						CurComponent->SetInputNodeId(-1);
					}
				}
			}
			// No need to delete the nodes created for an asset component manually here,
			// As they will be deleted when we clean up the CreateNodeIds array

			const int32 InputNodeId = CurInputObject->GetInputNodeId();
			if (InputNodeId >= 0 && !ManagedNodeIdSet.Contains(InputNodeId))
			{
				FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), InputNodeId);
				CurInputObject->SetInputNodeId(-1);
			}

			const int32 InputObjectNodeId = CurInputObject->GetInputObjectNodeId();
			if(InputObjectNodeId >= 0 && !ManagedNodeIdSet.Contains(InputObjectNodeId))
			{
				FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), InputObjectNodeId);
				CurInputObject->SetInputObjectNodeId(-1);

				// TODO: CHECK ME!
				//HAPI_NodeId ParentNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(CurInputObject->InputObjectNodeId);

				//// Delete its parent node as well
				//if (FHoudiniEngineUtils::IsHoudiniNodeValid(ParentNodeId))
				//	FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), ParentNodeId);
			}
			// Also directly invalidate HoudiniSplineComponent's node IDs.
			UHoudiniInputHoudiniSplineComponent* HoudiniSplineInputObject = Cast<UHoudiniInputHoudiniSplineComponent>(CurInputObject);
			if (IsValid(HoudiniSplineInputObject) && !IsGarbageCollecting())
			{
				UHoudiniSplineComponent* SplineComponent = HoudiniSplineInputObject->GetCurveComponent();
				if (IsValid(SplineComponent))
				{
					SplineComponent->SetNodeId(-1);
				}
			}

			CurInputObject->MarkChanged(true);
		}
	}

	// Destroy all the input assets
	for (HAPI_NodeId AssetNodeId : CreatedInputDataAssetIds)
	{
		if (AssetNodeId < 0 || ManagedNodeIdSet.Contains(AssetNodeId))
			continue;

		FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), AssetNodeId);
	}
	CreatedInputDataAssetIds.Empty();

	// Then simply destroy the input's parent OBJ node
	if (InputToDestroy->GetInputNodeId() >= 0)
	{
		HAPI_NodeId CreatedInputId = InputToDestroy->GetInputNodeId();
		HAPI_NodeId ParentId = FHoudiniEngineUtils::HapiGetParentNodeId(CreatedInputId);

		if (CreatedInputId >= 0)
		{
			FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), CreatedInputId);
			InputToDestroy->SetInputNodeId(-1);
		}

		if (FHoudiniEngineUtils::IsHoudiniNodeValid(ParentId))
		{
			FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), ParentId);
		}
	}

	return true;
}

bool
FHoudiniInputTranslator::DisconnectAndDestroyInput(UHoudiniInput* InputToDestroy, const EHoudiniInputType& InputType)
{
	// Start by disconnecting the input/object merge
	bool bSuccess = DisconnectInput(InputToDestroy, InputType);

	// Then destroy the created input nodes
	bSuccess &= DestroyInputNodes(InputToDestroy, InputType);

	return bSuccess;
}


EHoudiniInputType
FHoudiniInputTranslator::GetDefaultInputTypeFromLabel(const FString& InputName)
{
	// We'll try to find these magic words to try to detect the default input type
	//FString geoPrefix = TEXT("geo");
	FString curvePrefix = TEXT("curve");

	FString landscapePrefix = TEXT("landscape");
	FString landscapePrefix2 = TEXT("terrain");
	FString landscapePrefix3 = TEXT("heightfield");

	FString worldPrefix = TEXT("world");
	FString worldPrefix2 = TEXT("outliner");

	FString assetPrefix = TEXT("asset");
	FString assetPrefix2 = TEXT("hda");

	// By default, geometry input is chosen.
	EHoudiniInputType InputType = EHoudiniInputType::Geometry;

	if (InputName.Contains(curvePrefix, ESearchCase::IgnoreCase))
		InputType = EHoudiniInputType::Curve;

	else if ((InputName.Contains(landscapePrefix, ESearchCase::IgnoreCase))
		|| (InputName.Contains(landscapePrefix2, ESearchCase::IgnoreCase))
		|| (InputName.Contains(landscapePrefix3, ESearchCase::IgnoreCase)))
		InputType = EHoudiniInputType::World; // Landscape;

	else if ((InputName.Contains(worldPrefix, ESearchCase::IgnoreCase))
		|| (InputName.Contains(worldPrefix2, ESearchCase::IgnoreCase)))
		InputType = EHoudiniInputType::World;

	else if ((InputName.Contains(assetPrefix, ESearchCase::IgnoreCase))
		|| (InputName.Contains(assetPrefix2, ESearchCase::IgnoreCase)))
		InputType = EHoudiniInputType::World; // Asset;

	return InputType;
}

bool
FHoudiniInputTranslator::ChangeInputType(UHoudiniInput* InInput, const bool& bForce)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::ChangeInputType);

	if (!IsValid(InInput))
		return false;
	
	if (!InInput->HasInputTypeChanged() && !bForce)
		return true;

	// - Handle switching AWAY from an input type
	DisconnectAndDestroyInput(InInput, InInput->GetPreviousInputType());

	// Invalidate the previous input type now that we've actually changed
	//InInput->SetPreviousInputType(EHoudiniInputType::Invalid);

	//ChangeInputType(InInput, NewType);

	// TODO:
	// - Handle updating to the new input type
	//  downstream asset connection, static mesh update, curve creation...

	// Mark all the objects from this input has changed so they upload themselves
	InInput->MarkAllInputObjectsChanged(true);

	return true;
}

bool
FHoudiniInputTranslator::SetDefaultAssetFromHDA(UHoudiniInput* Input, bool& bOutBlueprintStructureModified)
{
	// 
	if (!IsValid(Input))
		return false;

	// Make sure we're linked to a valid object path parameter
	if (Input->GetParameterId() < 0)
		return false;

	// Get our ParmInfo
	HAPI_ParmInfo FoundParamInfo;
	FHoudiniApi::ParmInfo_Init(&FoundParamInfo);
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmInfo(
		FHoudiniEngine::Get().GetSession(),
		Input->GetAssetNodeId(), Input->GetParameterId(), &FoundParamInfo))
	{
		return false;
	}

	// Get our string value
	HAPI_StringHandle StringHandle;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmStringValues(
		FHoudiniEngine::Get().GetSession(),
		Input->GetAssetNodeId(),
		false,
		&StringHandle,
		FoundParamInfo.stringValuesIndex,
		1))
	{
		return false;
	}

	FString ParamValue;
	FHoudiniEngineString HoudiniEngineString(StringHandle);
	if (!HoudiniEngineString.ToFString(ParamValue))
	{
		return false;
	}

	if (ParamValue.Len() <= 0)
	{
		return false;
	}

	// Chop the default value using semi-colons as separators
	TArray<FString> Tokens;
	ParamValue.ParseIntoArray(Tokens, TEXT(";"), true);
	
	// Start by setting geometry input objects
	int32 GeoIdx = 0;
	for (auto& CurToken : Tokens)
	{
		if (CurToken.IsEmpty())
			continue;

		// Set default objects on the HDA instance - will override the parameter string
		// and apply the object input local-path thing for the HDA cook.
		UObject * pObject = LoadObject<UObject>(nullptr, *CurToken);
		if (!pObject)
			continue;

		Input->SetInputObjectAt(EHoudiniInputType::Geometry, GeoIdx++, pObject);
	}

	// See if we can preset world objects as well
	int32 WorldIdx = 0;
	int32 LandscapedIdx = 0;
	int32 HDAIdx = 0;
	for (TActorIterator<AActor> ActorIt(Input->GetWorld(), AActor::StaticClass(), EActorIteratorFlags::SkipPendingKill); ActorIt; ++ActorIt)
	{
		AActor* CurActor = *ActorIt;
		if (!CurActor)
			continue;

		AActor* FoundActor = nullptr;
		int32 FoundIdx = Tokens.Find(CurActor->GetFName().ToString());
		if (FoundIdx == INDEX_NONE)
			FoundIdx = Tokens.Find(CurActor->GetActorLabel());

		if(FoundIdx != INDEX_NONE)
			FoundActor = CurActor;

		if (!FoundActor)
			continue;

		// Select the found actor in the world input
		Input->SetInputObjectAt(EHoudiniInputType::World, WorldIdx++, FoundActor);

		// Remove the Found Token
		Tokens.RemoveAt(FoundIdx);
	}

	// See if we should change the default input type
	if (Input->GetInputType() == EHoudiniInputType::Geometry && WorldIdx > 0 && GeoIdx == 0)
	{	
		// Can just set the input type NewWorld Input Type
		Input->SetInputType(EHoudiniInputType::World, bOutBlueprintStructureModified);
	}

	return true;
}

bool
FHoudiniInputTranslator::UploadChangedInputs(UHoudiniAssetComponent * HAC)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::UploadChangedInputs);

	if (!IsValid(HAC))
		return false;

	// Nothing to do for Node Sync Components!
	if (HAC->IsA<UHoudiniNodeSyncComponent>())
		return true;

	// Disabled, this seems to be unused and is fairly costly to run in large levels/worlds
	//HoudiniUnrealDataLayersCache DataLayerCache = FHoudiniUnrealDataLayersCache::MakeCache(HAC->GetWorld());

	//for (auto CurrentInput : HAC->Inputs)
	for(int32 InputIdx = 0; InputIdx < HAC->GetNumInputs(); InputIdx++)
	{
		UHoudiniInput*& CurrentInput = HAC->Inputs[InputIdx];
		if (!IsValid(CurrentInput) || !CurrentInput->HasChanged())
			continue;

		// Delete any previous InputNodeIds of this HoudiniInput that are pending delete
		for (const HAPI_NodeId InputNodeIdPendingDelete : CurrentInput->GetInputNodesPendingDelete())
		{
			if (InputNodeIdPendingDelete < 0)
				continue;

			HAPI_NodeInfo NodeInfo;
			FHoudiniApi::NodeInfo_Init(&NodeInfo);

			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(
				FHoudiniEngine::Get().GetSession(), InputNodeIdPendingDelete, &NodeInfo))
			{
				continue;
			}

			HAPI_NodeId NodeToDelete = InputNodeIdPendingDelete;
			if (NodeInfo.type == HAPI_NODETYPE_SOP)
			{
				// Input nodes are Merge SOPs in a geo object, delete the geo object
				const HAPI_NodeId ParentId = FHoudiniEngineUtils::HapiGetParentNodeId(InputNodeIdPendingDelete);
				NodeToDelete = ParentId != -1 ? ParentId : InputNodeIdPendingDelete;
			}

			HOUDINI_CHECK_ERROR(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), NodeToDelete));
		}
		CurrentInput->ClearInputNodesPendingDelete();

		// First thing, see if we need to change the input type
		if (CurrentInput->HasInputTypeChanged())
		{
			ChangeInputType(CurrentInput, false);
		}

		if ((CurrentInput->IsLandscapeInput())
			&& CurrentInput->HasLandscapeExportTypeChanged()) 
		{
			DisconnectAndDestroyInput(CurrentInput, CurrentInput->GetInputType());
			CurrentInput->MarkAllInputObjectsChanged(true);
			CurrentInput->SetHasLandscapeExportTypeChanged(false);
		}

		bool bSuccess = true;
		if (CurrentInput->IsDataUploadNeeded())
		{
			FTransform OwnerTransform = FTransform::Identity;
			AActor * OwnerActor = HAC->GetOwner();
			if (OwnerActor)
			{
				OwnerTransform = OwnerActor->GetTransform();
			}
			
			bSuccess &= UploadInputData(CurrentInput, OwnerTransform);
			CurrentInput->MarkDataUploadNeeded(!bSuccess);
		}

		if (CurrentInput->IsTransformUploadNeeded())
		{
			bSuccess &= UploadInputTransform(CurrentInput);
		}

		// Update the input properties AFTER eventually uploading it
		bSuccess = UpdateInputProperties(CurrentInput);

		if (bSuccess)
		{
			CurrentInput->MarkChanged(false);
			CurrentInput->MarkAllInputObjectsChanged(false);
		}

		if (CurrentInput->HasInputTypeChanged())
			CurrentInput->SetPreviousInputType(EHoudiniInputType::Invalid);

		// Even if we failed, no need to try updating again.
		CurrentInput->SetNeedsToTriggerUpdate(false);
	}

	return true;
}

bool
FHoudiniInputTranslator::UpdateInputProperties(UHoudiniInput* InInput)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::UpdateInputProperties);

	bool bSucess = UpdateTransformType(InInput);

	bSucess &= UpdatePackBeforeMerge(InInput);

	bSucess &= UpdateTransformOffset(InInput);

	return bSucess;
}

bool
FHoudiniInputTranslator::UpdateTransformType(UHoudiniInput* InInput)
{
	if (!IsValid(InInput))
		return false;

	bool nTransformType = InInput->GetKeepWorldTransform();

	// Geometry inputs are always set to none
	EHoudiniInputType InputType = InInput->GetInputType();
	if (InputType == EHoudiniInputType::Geometry)
		nTransformType = 0;

	// Get the Input node ID from the host ID
	HAPI_NodeId InputNodeId = -1;
	HAPI_NodeId HostAssetId = InInput->GetAssetNodeId();

	bool bSuccess = true;
	const std::string sXformType = "xformtype"; 
	if (InInput->IsObjectPathParameter())
	{
		// Directly change the Parameter xformtype
		// (This will only work if the object merge is editable/unlocked)
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmIntValue(
			FHoudiniEngine::Get().GetSession(),
			HostAssetId, sXformType.c_str(), 0, nTransformType))
			bSuccess = false;
	}
	else
	{
		// Query the object merge's node ID via the input
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::QueryNodeInput(
			FHoudiniEngine::Get().GetSession(),
			HostAssetId, InInput->GetInputIndex(), &InputNodeId))
		{
			// Change its Parameter xformtype
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmIntValue(
				FHoudiniEngine::Get().GetSession(),
				InputNodeId, sXformType.c_str(), 0, nTransformType))
				bSuccess = false;
		}
	}

	// Since our input objects are all plugged into a merge node
	// We want to also update the transform type on the object merge plugged into the merge node
	// TODO: Also do it for Geo IN with multiple objects? or BP?
	HAPI_NodeId ParentNodeId = InInput->GetInputNodeId();
	if ((ParentNodeId >= 0)
		&& (InputType != EHoudiniInputType::Geometry))
	{
		HAPI_NodeId InputObjectNodeId = -1;
		int32 NumberOfInputMeshes = InInput->GetNumberOfInputMeshes(InputType);
		for (int n = 0; n < NumberOfInputMeshes; n++)
		{
			// Get the Input node ID from the host ID
			InputObjectNodeId = -1;
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::QueryNodeInput(
				FHoudiniEngine::Get().GetSession(),
				ParentNodeId, n, &InputObjectNodeId))
				continue;

			if (InputObjectNodeId == -1)
				continue;

			// Change the xformtype parameter on the object merge
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmIntValue(
				FHoudiniEngine::Get().GetSession(), InputObjectNodeId,
				sXformType.c_str(), 0, nTransformType))
				bSuccess = false;
		}
	}

	return bSuccess;
}

bool
FHoudiniInputTranslator::UpdatePackBeforeMerge(UHoudiniInput* InInput)
{
	if (!IsValid(InInput))
		return false;

	// Pack before merge is only available for Geo/World input
	EHoudiniInputType InputType = InInput->GetInputType();
	if (InputType != EHoudiniInputType::World
		&& InputType != EHoudiniInputType::Geometry)
	{
		// Nothing to change
		return true;
	}

	uint32 nPackValue = InInput->GetPackBeforeMerge() ? 1 : 0;

	// Get the Input node ID from the host ID
	HAPI_NodeId HostAssetId = InInput->GetAssetNodeId();

	bool bSuccess = true;
	const std::string sPack = "pack";
	const std::string sPivot = "pivot";

	// We'll be going through each input object plugged in the input's merge node
	// and change the pack parameter there
	HAPI_NodeId ParentNodeId = InInput->GetInputNodeId();
	if (ParentNodeId >= 0)
	{
		HAPI_NodeId InputObjectNodeId = -1;
		int32 NumberOfInputMeshes = InInput->GetNumberOfInputMeshes(InputType);
		for (int n = 0; n < NumberOfInputMeshes; n++)
		{
			// Get the Input node ID from the host ID
			InputObjectNodeId = -1;
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::QueryNodeInput(
				FHoudiniEngine::Get().GetSession(),
				ParentNodeId, n, &InputObjectNodeId))
				continue;

			if (InputObjectNodeId == -1)
				continue;

			// Change the pack parameter on the object merge
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmIntValue(
				FHoudiniEngine::Get().GetSession(), InputObjectNodeId,
				sPack.c_str(), 0, nPackValue))
				bSuccess = false;

			// Change the pivot parameter on the object merge to "origin"
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmIntValue(
				FHoudiniEngine::Get().GetSession(), InputObjectNodeId,
				sPivot.c_str(), 0, 0))
				bSuccess = false;
		}
	}

	return bSuccess;
}

bool
FHoudiniInputTranslator::UpdateTransformOffset(UHoudiniInput* InInput)
{
	if (!IsValid(InInput))
		return false;

	// Transform offsets are only for geometry inputs
	EHoudiniInputType InputType = InInput->GetInputType();
	if (InputType != EHoudiniInputType::Geometry)
		return true;

	// Get the input objects
	TArray<UHoudiniInputObject*>* InputObjectsArray = InInput->GetHoudiniInputObjectArray(InputType);
	if (!ensure(InputObjectsArray))
		return false;

	// Update each object's transform offset
	for (int32 ObjIdx = 0; ObjIdx < InputObjectsArray->Num(); ObjIdx++)
	{
		UHoudiniInputObject* CurrentInputObject = (*InputObjectsArray)[ObjIdx];
		if (!IsValid(CurrentInputObject))
			continue;

		// If the Input mesh has a Transform offset
		FTransform TransformOffset = CurrentInputObject->GetHoudiniObjectTransform();

		// Updating the Transform
		HAPI_TransformEuler HapiTransform;
		FHoudiniApi::TransformEuler_Init(&HapiTransform);
		FHoudiniEngineUtils::TranslateUnrealTransform(TransformOffset, HapiTransform);

		// Set the transform on the OBJ parent
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
			FHoudiniEngine::Get().GetSession(), CurrentInputObject->GetInputObjectNodeId(), &HapiTransform), false);
	}

	return true;
}

bool
FHoudiniInputTranslator::UploadInputData(UHoudiniInput* InInput, const FTransform & InActorTransform)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::UploadInputData);

	if (!IsValid(InInput))
		return false;

	EHoudiniInputType InputType = InInput->GetInputType();
	TArray<UHoudiniInputObject*>* InputObjectsArray = InInput->GetHoudiniInputObjectArray(InInput->GetInputType());
	if (!ensure(InputObjectsArray))
		return false;

	// When using the ref counted input system we use the UpdateScope to record all input nodes in the input
	// manager that are updated/created. Afterwards we look for all reference nodes and update/fix any objmerge
	// paths that are out of date (depending on node creation/deletion order, new nodes could use the same names
	// but now have a numeric suffix). The scope registers itself with the manager on construction and is then notified
	// of each entry in the manager that is created, updated or deleted. On destruction the scope unregisters itself
	// from the manager.
	FUnrealObjectInputUpdateScope UpdateScope;

	// Iterate on all the input objects and see if they need to be uploaded
	bool bSuccess = true;
	TArray<int32> CreatedNodeIds;
	TSet<FUnrealObjectInputHandle> Handles;
	TArray<int32> ValidNodeIds;
	TArray<UHoudiniInputObject*> ChangedInputObjects;
	for (int32 ObjIdx = 0; ObjIdx < InputObjectsArray->Num(); ObjIdx++)
	{
		UHoudiniInputObject* CurrentInputObject = (*InputObjectsArray)[ObjIdx];
		if (!IsValid(CurrentInputObject))
			continue;

		ValidNodeIds.Reset();
		ChangedInputObjects.Reset();
		// The input object could have child objects: GetChangedObjectsAndValidNodes finds if the object itself or
		// any its children has changed, and also returns the NodeIds of those objects that are still valid and
		// unchanged
		CurrentInputObject->GetChangedObjectsAndValidNodes(ChangedInputObjects, ValidNodeIds);

		// Keep track of the node ids for unchanged objects that already exist
		if (ValidNodeIds.Num() > 0)
			CreatedNodeIds.Append(ValidNodeIds);

		// Upload the changed input objects
		for (UHoudiniInputObject* ChangedInputObject : ChangedInputObjects)
		{
			// Upload the current input object to Houdini
			if (!UploadHoudiniInputObject(InInput, ChangedInputObject, InActorTransform, CreatedNodeIds, Handles, ChangedInputObject->CanDeleteHoudiniNodes()))
				bSuccess = false;
		}
	}

	// When using the ref counted input system, update objmerge paths in reference nodes that are potentially out of
	// date after the update above 
	{
		IUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
		if (Manager)
		{
			// Get nodes that were created / update from the UpdateScope
			TSet<FUnrealObjectInputIdentifier> UpdatedNodes(UpdateScope.GetNodesCreatedOrUpdated());
			TSet<FUnrealObjectInputIdentifier> ProcessedNodes(UpdatedNodes);
			TSet<FUnrealObjectInputIdentifier> ReferencedBy;
			for (const FUnrealObjectInputIdentifier& Identifier : UpdatedNodes)
			{
				ReferencedBy.Reset();
				// Look for all reference nodes that reference this node
				Manager->GetReferencedBy(Identifier, ReferencedBy);
				for (const FUnrealObjectInputIdentifier& RefToUpdate : ReferencedBy)
				{
					if (ProcessedNodes.Contains(RefToUpdate))
						continue;
					ProcessedNodes.Add(RefToUpdate);

					if (RefToUpdate.GetNodeType() != EUnrealObjectInputNodeType::Reference)
						continue;

					FUnrealObjectInputUtils::ConnectReferencedNodesToMerge(RefToUpdate);
				}
			}
		}
	}

	// If we haven't created any input, invalidate our input node id
	if (CreatedNodeIds.Num() == 0)
	{
		if (!InInput->HasInputTypeChanged())
		{
			int32 InputNodeId = InInput->GetInputNodeId();
			TArray<int32> PreviousInputObjectNodeIds = InInput->GetCreatedDataNodeIds();
	
			// TODO: CHECK THIS - remive ?
			//if (InInput->GetInputType() == EHoudiniInputType::Asset_DEPRECATED)
			if (InInput->IsAssetInput())
			{
				UHoudiniAssetComponent * OuterHAC = Cast<UHoudiniAssetComponent>(InInput->GetOuter());
				HAPI_NodeId  AssetId = OuterHAC->GetAssetId();

				// Disconnect the asset input
				if (InputNodeId >= 0 && InInput->GetInputIndex() >= 0)
				{
					HOUDINI_CHECK_ERROR(FHoudiniApi::DisconnectNodeInput(
						FHoudiniEngine::Get().GetSession(), AssetId, InInput->GetInputIndex()));
				}
			}
			else if (InInput->GetInputType() == EHoudiniInputType::World)
			{
				// World nodes are handled by InputObjects () (with FHoudiniEngineRuntime::Get().MarkNodeIdAsPendingDelete)
			}
			else
			{
				if (InputNodeId >= 0)
				{
					for (int32 Idx = PreviousInputObjectNodeIds.Num() - 1; Idx >= 0; --Idx)
					{

						// Get the object merge connected to the merge node
						HAPI_NodeId InputObjectMergeId = -1;
						HOUDINI_CHECK_ERROR(FHoudiniApi::QueryNodeInput(
							FHoudiniEngine::Get().GetSession(), InputNodeId, Idx, &InputObjectMergeId));

						// Disconnect the two nodes
						HOUDINI_CHECK_ERROR(FHoudiniApi::DisconnectNodeInput(
							FHoudiniEngine::Get().GetSession(), InputNodeId, Idx));

						// Destroy the object merge node, do not delete other HDA (Asset input type)
						HOUDINI_CHECK_ERROR(FHoudiniApi::DeleteNode(
							FHoudiniEngine::Get().GetSession(), InputObjectMergeId));
					}
				}
			}
		}
		InInput->GetCreatedDataNodeIds().Empty();
		InInput->SetInputNodeId(-1);
		return bSuccess;
	}

	// Get the current input's NodeId
	HAPI_NodeId InputNodeId = InInput->GetInputNodeId();
	// Check that the current input's node ID is still valid
	if (InputNodeId < 0 || !FHoudiniEngineUtils::IsHoudiniNodeValid(InputNodeId))
	{
		// This input doesn't have a valid NodeId yet,
		// we need to create this input's merge node and update this input's node ID
		FString MergeName = InInput->GetNodeBaseName() + TEXT("_Merge");
		HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
			-1,	TEXT("SOP/merge"), MergeName, true, &InputNodeId), false);

		InInput->SetInputNodeId(InputNodeId);
	}

	//TODO:
	// Do we want to update the input's transform?
	if (false)
	{
		FTransform ComponentTransform = FTransform::Identity;
		USceneComponent* OuterComp = Cast<USceneComponent>(InInput->GetOuter());
		if (IsValid(OuterComp))
			ComponentTransform = OuterComp->GetComponentTransform();

		FHoudiniEngineUtils::HapiSetAssetTransform(InputNodeId, ComponentTransform);
		//HapiUpdateInputNodeTransform(InputNodeId, ComponentTransform);
	}

	// Connect all the input objects to the merge node now
	int32 InputIndex = 0;
	for (auto CurrentNodeId : CreatedNodeIds)
	{
		if (CurrentNodeId < 0)
			continue;

		if (InputNodeId == CurrentNodeId)
			continue;

		// Connect the current input object to the merge node
		HOUDINI_CHECK_ERROR(FHoudiniApi::ConnectNodeInput(
			FHoudiniEngine::Get().GetSession(),
			InputNodeId, InputIndex++, CurrentNodeId, 0));
	}

	// Check if we need to disconnect extra input objects nodes from the merge
	// This can be needed when the input had more input objects on the previous cook
	TArray<int32>& PreviousInputObjectNodeIds = InInput->GetCreatedDataNodeIds();
	if (!InInput->HasInputTypeChanged())
	{
		for (int32 Idx = PreviousInputObjectNodeIds.Num() - 1; Idx >= CreatedNodeIds.Num(); --Idx)
		{
			// Get the object merge connected to the merge node
			HAPI_NodeId InputObjectMergeId = -1;
			
			// TODO: CHECK THIS - remove if?!
			//if (InInput->GetInputType() != EHoudiniInputType::Asset_DEPRECATED)
			if (!InInput->IsAssetInput())
				HOUDINI_CHECK_ERROR(FHoudiniApi::QueryNodeInput(
					FHoudiniEngine::Get().GetSession(), InputNodeId, Idx, &InputObjectMergeId));

			// Disconnect the two nodes
			HOUDINI_CHECK_ERROR(FHoudiniApi::DisconnectNodeInput(
				FHoudiniEngine::Get().GetSession(), InputNodeId, Idx));

			// TODO: CHECK THIS - remove if?!
			// Destroy the object merge node, do not destroy other HDA (Asset input type)
			//if (InInput->GetInputType() != EHoudiniInputType::Asset_DEPRECATED)
			if (!InInput->IsAssetInput())
			{
				HOUDINI_CHECK_ERROR(FHoudiniApi::DeleteNode(
					FHoudiniEngine::Get().GetSession(), InputObjectMergeId));
			}
		}
	}

	// Keep track of all the nodes plugged into our input's merge
	PreviousInputObjectNodeIds = CreatedNodeIds;

	// Finally, connect our main input node to the asset
	bSuccess = ConnectInputNode(InInput);

	return bSuccess;
}

bool
FHoudiniInputTranslator::UploadInputTransform(UHoudiniInput* InInput)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::UploadInputTransform);

	if (!IsValid(InInput))
		return false;

	EHoudiniInputType InputType = InInput->GetInputType();
	TArray<UHoudiniInputObject*>* InputObjectsArray = InInput->GetHoudiniInputObjectArray(InInput->GetInputType());
	if (!ensure(InputObjectsArray))
		return false;

	// Iterate on all the input objects and see if their transform needs to be uploaded
	bool bSuccess = true;
	for (int32 ObjIdx = 0; ObjIdx < InputObjectsArray->Num(); ObjIdx++)
	{
		UHoudiniInputObject* CurrentInputObject = (*InputObjectsArray)[ObjIdx];
		if (!IsValid(CurrentInputObject))
			continue;

		if (!CurrentInputObject->HasTransformChanged())
			continue;

		// Upload the current input object's transform to Houdini	
		if (!UploadHoudiniInputTransform(InInput, CurrentInputObject))
		{
			bSuccess = false;
			continue;
		}
	}

	return bSuccess;
}

bool
FHoudiniInputTranslator::ConnectInputNode(UHoudiniInput* InInput)
{
	if (!IsValid(InInput))
		return false;

	HAPI_NodeId AssetNodeId = InInput->GetAssetNodeId();
	if (AssetNodeId < 0)
		return false;

	HAPI_NodeId InputNodeId = InInput->GetInputNodeId();
	if (InputNodeId < 0)
		return false;

	// Helper for connecting our input or setting the object path parameter
	if (InInput->IsObjectPathParameter())
	{
		// Now we can assign the input node path to the parameter
		std::string ParamNameString = TCHAR_TO_UTF8(*(InInput->GetInputName()));

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmNodeValue(
			FHoudiniEngine::Get().GetSession(), AssetNodeId,
			ParamNameString.c_str(), InputNodeId), false);
	}
	else
	{
		// TODO: CHECK ME!
		//if (!FHoudiniEngineUtils::IsHoudiniNodeValid(InputNodeId))
		//	return false;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
			FHoudiniEngine::Get().GetSession(), AssetNodeId,
			InInput->GetInputIndex(), InputNodeId, 0), false);
	}
	
	return true;
}

bool
FHoudiniInputTranslator::UploadHoudiniInputObject(
	UHoudiniInput* InInput, 
	UHoudiniInputObject* InInputObject,
	const FTransform& InActorTransform,
	TArray<int32>& OutCreatedNodeIds,
	TSet<FUnrealObjectInputHandle>& OutHandles,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::UploadHoudiniInputObject);

	if (!InInput || !InInputObject)
		return false;

	FString ObjBaseName = InInput->GetNodeBaseName();

	FHoudiniInputObjectSettings InputSettings(InInput);

	bool bSuccess = true;
	switch (InInputObject->Type)
	{
		case EHoudiniInputObjectType::Object:
		{
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForObject(ObjBaseName, InInputObject);

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->GetInputObjectNodeId());
				OutHandles.Add(InInputObject->InputNodeHandle);
			}

			break;
		}

		case EHoudiniInputObjectType::StaticMesh:
		{
			UHoudiniInputStaticMesh* InputSM = Cast<UHoudiniInputStaticMesh>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForStaticMesh(
				ObjBaseName,
				InputSM,
				InputSettings,
				bInputNodesCanBeDeleted);

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->GetInputObjectNodeId());
				OutHandles.Add(InInputObject->InputNodeHandle);
			}

			break;
		}

		case EHoudiniInputObjectType::SkeletalMesh:
		{
			UHoudiniInputSkeletalMesh* InputSkelMesh = Cast<UHoudiniInputSkeletalMesh>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForSkeletalMesh(
				ObjBaseName,
				InputSkelMesh,
				InputSettings,
				bInputNodesCanBeDeleted);

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->GetInputObjectNodeId());
				OutHandles.Add(InInputObject->InputNodeHandle);
			}

			break;
		}

		case EHoudiniInputObjectType::Animation:
		{
			UHoudiniInputAnimation* InputAnimation = Cast<UHoudiniInputAnimation>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForAnimation(
				ObjBaseName,
				InputAnimation,
				InputSettings,
				bInputNodesCanBeDeleted);

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->GetInputObjectNodeId());
				OutHandles.Add(InInputObject->InputNodeHandle);
			}

			break;
		}

		case EHoudiniInputObjectType::SkeletalMeshComponent:
		{
			UHoudiniInputSkeletalMeshComponent* InputSKC = Cast<UHoudiniInputSkeletalMeshComponent>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForSkeletalMeshComponent(
				ObjBaseName,
				InputSKC,
				InputSettings,
				bInputNodesCanBeDeleted);

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->GetInputObjectNodeId());
				OutHandles.Add(InInputObject->InputNodeHandle);
			}

			break;
		}
		case EHoudiniInputObjectType::GeometryCollection:
		{	
			UHoudiniInputGeometryCollection* InputGeometryCollection = Cast<UHoudiniInputGeometryCollection>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForGeometryCollection(
				ObjBaseName,
				InputGeometryCollection,
				InputSettings,
				bInputNodesCanBeDeleted);

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->GetInputObjectNodeId());
				OutHandles.Add(InInputObject->InputNodeHandle);
			}

			break;
		}
		case EHoudiniInputObjectType::GeometryCollectionComponent:
		{	
			UHoudiniInputGeometryCollectionComponent* InputGeometryCollection = Cast<UHoudiniInputGeometryCollectionComponent>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForGeometryCollectionComponent(
				ObjBaseName,
				InputGeometryCollection,
				InputSettings,
				bInputNodesCanBeDeleted);

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->GetInputObjectNodeId());
				OutHandles.Add(InInputObject->InputNodeHandle);
			}

			break;
		}
		case EHoudiniInputObjectType::SceneComponent:
		{
			UHoudiniInputSceneComponent* InputSceneComp = Cast<UHoudiniInputSceneComponent>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForSceneComponent(
				ObjBaseName,
				InputSceneComp,
				InputSettings,
				bInputNodesCanBeDeleted);

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->GetInputObjectNodeId());
				OutHandles.Add(InInputObject->InputNodeHandle);
			}

			break;
		}

		case EHoudiniInputObjectType::StaticMeshComponent:
		{
			UHoudiniInputMeshComponent* InputSMC = Cast<UHoudiniInputMeshComponent>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForStaticMeshComponent(
				ObjBaseName,
				InputSMC,
				InputSettings,
				bInputNodesCanBeDeleted);

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->GetInputObjectNodeId());
				OutHandles.Add(InInputObject->InputNodeHandle);
			}

			break;
		}

		case EHoudiniInputObjectType::InstancedStaticMeshComponent:
		{
			UHoudiniInputInstancedMeshComponent* InputISMC = Cast<UHoudiniInputInstancedMeshComponent>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForInstancedStaticMeshComponent(
				ObjBaseName,
				InputISMC,
				InputSettings,
				bInputNodesCanBeDeleted);

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->GetInputObjectNodeId());
				OutHandles.Add(InInputObject->InputNodeHandle);
			}

			break;
		}

		case EHoudiniInputObjectType::SplineComponent:
		{
			UHoudiniInputSplineComponent* InputSpline = Cast<UHoudiniInputSplineComponent>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForSplineComponent(
				ObjBaseName,
				InputSpline,
				InputSettings,
				bInputNodesCanBeDeleted);

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->GetInputObjectNodeId());
				OutHandles.Add(InInputObject->InputNodeHandle);
			}

			break;
		}

		case EHoudiniInputObjectType::HoudiniSplineComponent:
		{
			UHoudiniInputHoudiniSplineComponent* InputCurve = Cast<UHoudiniInputHoudiniSplineComponent>(InInputObject);

			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForHoudiniSplineComponent(
				ObjBaseName,
				InputCurve,
				InputSettings);

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->GetInputObjectNodeId());
				OutHandles.Add(InInputObject->InputNodeHandle);
			}

			break;
		}

		case EHoudiniInputObjectType::HoudiniAssetActor:
		case EHoudiniInputObjectType::HoudiniAssetComponent:
		{
			UHoudiniInputHoudiniAsset* InputHAC = Cast<UHoudiniInputHoudiniAsset>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForHoudiniAssetComponent(
				ObjBaseName,
				InputHAC,
				InputSettings);

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->GetInputObjectNodeId());
				OutHandles.Add(InInputObject->InputNodeHandle);
			}

			break;
		}

		case EHoudiniInputObjectType::Actor:
		case EHoudiniInputObjectType::GeometryCollectionActor_Deprecated:
		case EHoudiniInputObjectType::LandscapeSplineActor:
		{			
			UHoudiniInputActor* InputActor = Cast<UHoudiniInputActor>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForActor(
				InInput,
				InputActor,
				InActorTransform,
				OutCreatedNodeIds,
				OutHandles,
				bInputNodesCanBeDeleted);

			break;
		}

		case EHoudiniInputObjectType::Landscape:
		{
			UHoudiniInputLandscape* InputLandscape = Cast<UHoudiniInputLandscape>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForLandscape(
				ObjBaseName,
				InputLandscape,
				InInput,
				OutCreatedNodeIds,
				OutHandles,
				bInputNodesCanBeDeleted);

			break;
		}

		case EHoudiniInputObjectType::LevelInstance:
		{
			UHoudiniInputLevelInstance* InputLevelInstance = Cast<UHoudiniInputLevelInstance>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForLevelInstance(
				ObjBaseName,
				InputLevelInstance,
				InputSettings,
				InInput,
				OutCreatedNodeIds,
				OutHandles,
				bInputNodesCanBeDeleted);

			break;
		}

		case EHoudiniInputObjectType::PackedLevelActor:
		{
			UHoudiniInputPackedLevelActor* const InputPackedLevelActor = Cast<UHoudiniInputPackedLevelActor>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForPackedLevelActor(
				ObjBaseName,
				InputPackedLevelActor,
				InputSettings,
				InInput,
				OutCreatedNodeIds,
				OutHandles,
				bInputNodesCanBeDeleted);

			break;
		}

		case EHoudiniInputObjectType::Brush:
		{
			UHoudiniInputBrush* InputBrush = Cast<UHoudiniInputBrush>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForBrush(
				ObjBaseName,
				InputBrush,
				InInput->GetBoundSelectorObjectArray(),
				InputSettings,
				bInputNodesCanBeDeleted);

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->GetInputObjectNodeId());
				OutHandles.Add(InInputObject->InputNodeHandle);
			}

			break;
		}

		case EHoudiniInputObjectType::CameraComponent:
		{
			UHoudiniInputCameraComponent* InputCamera = Cast<UHoudiniInputCameraComponent>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForCamera(
				ObjBaseName, InputCamera, InputSettings);

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->GetInputObjectNodeId());
				OutHandles.Add(InInputObject->InputNodeHandle);
			}

			break;
		}

		case EHoudiniInputObjectType::DataTable:
		{
			UHoudiniInputDataTable* InputDT = Cast<UHoudiniInputDataTable>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForDataTable(
				ObjBaseName, InputDT, InputSettings, bInputNodesCanBeDeleted);

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->GetInputObjectNodeId());
				OutHandles.Add(InInputObject->InputNodeHandle);
			}

			break;
		}

		case EHoudiniInputObjectType::FoliageType_InstancedStaticMesh:
		{
			UHoudiniInputFoliageType_InstancedStaticMesh* const InputFoliageTypeSM = Cast<UHoudiniInputFoliageType_InstancedStaticMesh>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForFoliageType_InstancedStaticMesh(
				ObjBaseName,
				InputFoliageTypeSM,
				InputSettings,
				bInputNodesCanBeDeleted);

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->GetInputObjectNodeId());
				OutHandles.Add(InInputObject->InputNodeHandle);
			}

			break;
		}

		case EHoudiniInputObjectType::Blueprint:
		{
			UHoudiniInputBlueprint* InputBP = Cast<UHoudiniInputBlueprint>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForBP(
				InInput, InputBP, OutCreatedNodeIds, OutHandles, bInputNodesCanBeDeleted);
			break;
		}

		case EHoudiniInputObjectType::LandscapeSplinesComponent:
		{
			UHoudiniInputLandscapeSplinesComponent* const InputLandscapeSplinesComponent = Cast<UHoudiniInputLandscapeSplinesComponent>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForLandscapeSplinesComponent(
				ObjBaseName,
				InputLandscapeSplinesComponent,
				InputSettings,
				OutCreatedNodeIds,
				OutHandles,
				bInputNodesCanBeDeleted);
			break;
		}

		case EHoudiniInputObjectType::SplineMeshComponent:
		{
			UHoudiniInputMeshComponent* InputSMC = Cast<UHoudiniInputMeshComponent>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForStaticMeshComponent(
				ObjBaseName,
				InputSMC,
				InputSettings,
				bInputNodesCanBeDeleted);

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->GetInputObjectNodeId());
				OutHandles.Add(InInputObject->InputNodeHandle);
			}
			break;
		}
	}

	// Mark that input object as not changed
	if (bSuccess)
	{
		InInputObject->MarkChanged(false);
		InInputObject->SetNeedsToTriggerUpdate(false);
	}
	else
	{
		// We couldn't update/create that input object, keep it changed but prevent it from trigger updates
		InInputObject->SetNeedsToTriggerUpdate(false);
	}

	// Mark the outer package as dirty, to ensure that the changes are saved when using OFPA / World partition
	InInputObject->MarkPackageDirty();

	return bSuccess;
}


// Upload transform for an input's InputObject
bool 
FHoudiniInputTranslator::UploadHoudiniInputTransform(
	UHoudiniInput* InInput, UHoudiniInputObject* InInputObject)
{
	if (!InInput || !InInputObject)
		return false;

	auto UpdateTransform = [](const FTransform& InTransform, const HAPI_NodeId& InNodeId)
	{
		// Translate the Transform to HAPI
		HAPI_TransformEuler HapiTransform;
		FHoudiniApi::TransformEuler_Init(&HapiTransform);
		FHoudiniEngineUtils::TranslateUnrealTransform(InTransform, HapiTransform);

		// Set the transform on the OBJ parent
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
			FHoudiniEngine::Get().GetSession(), InNodeId, &HapiTransform), false);

		return true;
	};

	// Check if the new input system is being used
	bool bSuccess = true;
	switch (InInputObject->Type)
	{
		case EHoudiniInputObjectType::StaticMesh:
		{
			// Simply update the Input mesh's Transform offset
			if (!UpdateTransform(InInputObject->GetHoudiniObjectTransform(), InInputObject->GetInputObjectNodeId()))
				bSuccess = false;

			break;
		}

		
		case EHoudiniInputObjectType::CameraComponent:
		case EHoudiniInputObjectType::GeometryCollectionComponent:
		case EHoudiniInputObjectType::InstancedStaticMeshComponent:
		case EHoudiniInputObjectType::SceneComponent:
		case EHoudiniInputObjectType::SplineComponent:
		case EHoudiniInputObjectType::StaticMeshComponent:
		{
			// Default behaviour for components derived from SceneComponent.

			// Update using the component's transform
			UHoudiniInputSceneComponent* InComponent = Cast<UHoudiniInputSceneComponent>(InInputObject);
			if (!IsValid(InComponent))
			{
				bSuccess = false;
				break;
			}

			// Update the InputObject's transform
			InComponent->UpdateTransform();
			if(!UpdateTransform(InComponent->GetHoudiniObjectTransform(), InInputObject->GetInputObjectNodeId()))
				bSuccess = false;

			break;
		}

		case EHoudiniInputObjectType::HoudiniSplineComponent:
		{
			// TODO: Simply update the curve's transform?
			break;
		}

		case EHoudiniInputObjectType::HoudiniAssetActor:
		case EHoudiniInputObjectType::HoudiniAssetComponent:
		{
			// TODO: Check, nothing to do?
			break;
		}

		case EHoudiniInputObjectType::Actor:
		case EHoudiniInputObjectType::GeometryCollectionActor_Deprecated:
		case EHoudiniInputObjectType::LandscapeSplineActor:
		case EHoudiniInputObjectType::LevelInstance:
		case EHoudiniInputObjectType::PackedLevelActor:
		{
			UHoudiniInputActor* InputActor = Cast<UHoudiniInputActor>(InInputObject);
			if (!IsValid(InputActor))
			{
				bSuccess = false;
				break;
			}

			// Update the actor's transform
			// To avoid further updates
			if (InputActor->GetActor())
				InputActor->SetTransform(InputActor->GetActor()->GetTransform());

			{
				const HAPI_NodeId ObjectNodeId = InputActor->GetInputObjectNodeId();
				if (ObjectNodeId >= 0)
					UpdateTransform(InputActor->GetHoudiniObjectTransform(), ObjectNodeId);
			}

			// Iterate on all the actor input objects and see if their transform needs to be uploaded
			// TODO? Also update the component's actor transform??
			for (auto& CurrentComponent : InputActor->GetActorComponents())
			{
				if (!IsValid(CurrentComponent))
					continue;

				if (!CurrentComponent->HasTransformChanged())
					continue;

				// Upload the current input object's transform to Houdini	
				if (!UploadHoudiniInputTransform(InInput, CurrentComponent))
				{
					bSuccess = false;
					continue;
				}
			}
			break;
		}
	
		case EHoudiniInputObjectType::Landscape:
		{
			const FTransform NewTransform = InInputObject->GetHoudiniObjectTransform();
			if (!UpdateTransform(InInputObject->GetHoudiniObjectTransform(), InInputObject->GetInputObjectNodeId()))
				bSuccess = false;
		}

		case EHoudiniInputObjectType::Brush:
		{
			// TODO: Update the Brush's transform
			break;
		}

		case EHoudiniInputObjectType::FoliageType_InstancedStaticMesh:
		{
			// Simply update the Input mesh's Transform offset
			if (!UpdateTransform(InInputObject->GetHoudiniObjectTransform(), InInputObject->GetInputObjectNodeId()))
				bSuccess = false;

			break;
		}

		// Unsupported
		case EHoudiniInputObjectType::Object:
		case EHoudiniInputObjectType::SkeletalMesh:
		{
			break;
		}
		case EHoudiniInputObjectType::GeometryCollection:
		{
			// Simply update the Input mesh's Transform offset
			if (!UpdateTransform(InInputObject->GetHoudiniObjectTransform(), InInputObject->GetInputObjectNodeId()))
				bSuccess = false;

			break;
		}

		case EHoudiniInputObjectType::Blueprint:
		{
			UHoudiniInputBlueprint* InputBP = Cast<UHoudiniInputBlueprint>(InInputObject);
			if (!IsValid(InputBP))
			{
				bSuccess = false;
				break;
			}
			
			const HAPI_NodeId InputObjNodeId = InputBP->GetInputObjectNodeId();
			if (InputObjNodeId >= 0)
			{
				UpdateTransform(InputBP->GetHoudiniObjectTransform(), InputObjNodeId);
			}
			
			// Iterate on all the BP's input objects and see if their transform needs to be uploaded
			for (auto& CurrentComponent : InputBP->GetComponents())
			{
				if (!IsValid(CurrentComponent))
					continue;

				if (!CurrentComponent->HasTransformChanged())
					continue;

				// Upload the current input object's transform to Houdini	
				if (!UploadHoudiniInputTransform(InInput, CurrentComponent))
				{
					bSuccess = false;
					continue;
				}
			}
			break;
		}
		case EHoudiniInputObjectType::Invalid:
		default:
			break;
	}

	// Mark that input object as not changed
	if (bSuccess)
	{
		InInputObject->MarkTransformChanged(false);
		InInputObject->SetNeedsToTriggerUpdate(false);
	}
	else
	{
		// We couldn't update/create that input object, keep it changed but prevent it from trigger updates
		InInputObject->SetNeedsToTriggerUpdate(false);
	}

	return bSuccess;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForObject(const FString& InObjNodeName, UHoudiniInputObject* InObject)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForObject);

	if (!InObject)
		return false;
	
	UObject* Object = InObject->GetObject();
	if (!IsValid(Object))
		return true;

	FString NodeName = InObjNodeName + TEXT("_") + Object->GetName();
	FHoudiniEngineUtils::SanitizeHAPIVariableName(NodeName);

	// For UObjects we can't upload much, but can still create an input node
	// with a single point, with an attribute pointing to the input object's path
	HAPI_NodeId InputNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateInputNode(
		FHoudiniEngine::Get().GetSession(), -1, &InputNodeId, TCHAR_TO_UTF8(*NodeName)), false);

	// Update this input object's NodeId and ObjectNodeId
	InObject->SetInputNodeId((int32)InputNodeId);
	InObject->SetInputObjectNodeId((int32)FHoudiniEngineUtils::HapiGetParentNodeId(InputNodeId));
	
	// Create a part
	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = 2;
	Part.vertexCount = 0;
	Part.faceCount = 0;
	Part.pointCount = 1;
	Part.type = HAPI_PARTTYPE_MESH;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
		FHoudiniEngine::Get().GetSession(), InputNodeId, 0, &Part), false);

	{
		// Create point attribute info for P.
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = 1;
		AttributeInfoPoint.tupleSize = 3;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
			HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint), false);

		// Set the point's position
		FVector3f ObjectPosition = (FVector3f)InObject->GetHoudiniObjectTransform().GetLocation();
		TArray<float> Position =
		{
			ObjectPosition.X * HAPI_UNREAL_SCALE_FACTOR_POSITION,
			ObjectPosition.Z * HAPI_UNREAL_SCALE_FACTOR_POSITION,
			ObjectPosition.Y * HAPI_UNREAL_SCALE_FACTOR_POSITION
		};

		// Now that we have raw positions, we can upload them for our attribute.
		FHoudiniHapiAccessor Accessor(InputNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoPoint, Position), false);

	}

	{
		// Create point attribute info for the path.
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = 1;
		AttributeInfoPoint.tupleSize = 1;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
			HAPI_UNREAL_ATTRIB_OBJECT_PATH, &AttributeInfoPoint), false);

		// Set the point's path attribute
		FString ObjectPathName = Object->GetPathName();

		FHoudiniHapiAccessor Accessor(InputNodeId, 0, HAPI_UNREAL_ATTRIB_OBJECT_PATH);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfoPoint, ObjectPathName), false);
	}

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(InputNodeId), false);

	return true;
}

bool
FHoudiniInputTranslator::HapiSetGeoObjectTransform(const HAPI_NodeId& InObjectNodeId, const FTransform& InTransform)
{
	if (InObjectNodeId < 0)
		return true;

	// Updating the Transform
	HAPI_TransformEuler HapiTransform;
	FHoudiniApi::TransformEuler_Init(&HapiTransform);

	FHoudiniEngineUtils::TranslateUnrealTransform(InTransform, HapiTransform);

	// Set the transform on the OBJ parent
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
		FHoudiniEngine::Get().GetSession(), InObjectNodeId, &HapiTransform), false);

	return true;
}

bool
FHoudiniInputTranslator::HapiCreateOrUpdateGeoObjectMergeAndSetTransform(
	const int32 InParentNodeId,
	const HAPI_NodeId& InNodeToObjectMerge,
	const FString& InObjNodeName,
	HAPI_NodeId& InOutObjectMergeNodeId,
	HAPI_NodeId& InOutGeoObjectNodeId,
	const bool bInCreateIfMissingInvalid,
	const FTransform& InTransform,
	const int32& InTransformType)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateOrUpdateGeoObjectMergeAndSetTransform);

	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(InNodeToObjectMerge))
		return false;

	bool bCreatedGeoObject = false;
	constexpr bool bCookOnCreation = true;

	// Check that InOutGeoObjectNodeId is valid
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(InOutGeoObjectNodeId))
	{
		if (!bInCreateIfMissingInvalid)
			return false;

		// Create Geo object in InParentNodeId's network
		const FString ObjOperatorName = InParentNodeId >= 0 ? TEXT("geo") : TEXT("Object/geo");
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniEngineUtils::CreateNode(InParentNodeId, ObjOperatorName, InObjNodeName, bCookOnCreation, &InOutGeoObjectNodeId), false);
		bCreatedGeoObject = true;
	}
	
	HAPI_NodeId CurrentGeoNodeId = -1;
	if (InOutObjectMergeNodeId < 0)
	{
		// See if the node already exists
		HAPI_Result result = FHoudiniApi::GetNodeFromPath(
			FHoudiniEngine::Get().GetSession(), InOutGeoObjectNodeId, TCHAR_TO_ANSI(*InObjNodeName), &CurrentGeoNodeId);

		if (CurrentGeoNodeId >= 0)
			InOutObjectMergeNodeId = CurrentGeoNodeId;
	}

	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(InOutObjectMergeNodeId))
	{
		if (!bInCreateIfMissingInvalid)
			return false;

		// Create the objmerge SOP in InOutGeoObjectNodeId if non existent
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniEngineUtils::CreateNode(InOutGeoObjectNodeId, TEXT("object_merge"), InObjNodeName, bCookOnCreation, &InOutObjectMergeNodeId), false);
	}

	// Set the objpath1 on the object merge
	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmNodeValue(Session, InOutObjectMergeNodeId, TCHAR_TO_UTF8(TEXT("objpath1")), InNodeToObjectMerge), false);

	// Set Transform type if needed
	if (InTransformType >= 0 && InTransformType <= 2)
	{
		// 0 None
		// 1 Into this object
		// 2 Into Specified
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::SetParmIntValue(Session, InOutObjectMergeNodeId, TCHAR_TO_UTF8(TEXT("xformtype")), 0, InTransformType), false);
	}
	
	if (!InTransform.Equals(FTransform::Identity) || !bCreatedGeoObject)
	{
		if (!HapiSetGeoObjectTransform(InOutGeoObjectNodeId, InTransform))
			return false;
	}

	return true;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForStaticMesh(
	const FString& InObjNodeName,
	UHoudiniInputStaticMesh* InObject,
	const FHoudiniInputObjectSettings& InInputSettings,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForStaticMesh);

	if (!IsValid(InObject))
		return false;

	// Get the StaticMesh
	UStaticMesh* SM = InObject->GetStaticMesh();
	if (!IsValid(SM))
		return true;

	FString SMName = InObjNodeName + TEXT("_") + SM->GetName();
	FHoudiniEngineUtils::SanitizeHAPIVariableName(SMName);

	// Marshall the Static Mesh to Houdini
	FUnrealObjectInputHandle SMInputNodeHandle;
	HAPI_NodeId CreatedNodeId = -1;

	// Get the existing node id, if any
	
	// For the ref counted system the handle on the input object represents a reference node that has a single node
	// it references: the static mesh. The reference node represents InObject with its Transform (geometry input).
	{
		TSet<FUnrealObjectInputHandle> ReferencedNodes;
		if (FUnrealObjectInputUtils::GetReferencedNodes(InObject->InputNodeHandle, ReferencedNodes) && ReferencedNodes.Num() == 1)
		{
			const FUnrealObjectInputHandle Handle = ReferencedNodes.Array()[0];
			FUnrealObjectInputUtils::GetHAPINodeId(Handle, CreatedNodeId);
		}
	}
	
	bool bSuccess = true;
	if (InInputSettings.bImportAsReference) 
	{
		FBox InBbox = InInputSettings.bImportAsReferenceBboxEnabled ?
			SM->GetBoundingBox() :
			FBox(EForceInit::ForceInit);

		const TArray<FString>& MaterialReferences = InInputSettings.bImportAsReferenceMaterialEnabled ?
			InObject->GetMaterialReferences() :
			TArray<FString>();

		bSuccess = FHoudiniInputTranslator::CreateInputNodeForReference(
			CreatedNodeId,
			SM,
			SMName,
			InObject->GetTransform(),
			InInputSettings.bImportAsReferenceRotScaleEnabled,
			SMInputNodeHandle,
			bInputNodesCanBeDeleted,
			InInputSettings.bImportAsReferenceBboxEnabled,
			InBbox,
			InInputSettings.bImportAsReferenceMaterialEnabled,
			MaterialReferences);
	}
	else 
	{	
		bSuccess = FUnrealMeshTranslator::HapiCreateInputNodeForStaticMesh(
			SM,
			CreatedNodeId,
			SMName,
			SMInputNodeHandle,
			nullptr,
			InInputSettings.bExportLODs,
			InInputSettings.bExportSockets,
			InInputSettings.bExportColliders,
			true,
			bInputNodesCanBeDeleted,
			InInputSettings.bPreferNaniteFallbackMesh,
			InInputSettings.bExportMaterialParameters,
			false);
	}

	{
		// The static mesh can have its own transform (geometry input), so we have to create a reference node that
		// represents InObject in the new input system that references the StaticMesh asset's input node handle
		FUnrealObjectInputOptions Options;
		static constexpr bool bIsLeaf = false;
		FUnrealObjectInputIdentifier GeoInputRefNodeId(InObject, Options, bIsLeaf);
		FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(GeoInputRefNodeId, { SMInputNodeHandle }, InObject->InputNodeHandle, true, bInputNodesCanBeDeleted);
	}

	if (!HapiSetGeoObjectTransform(InObject->GetInputObjectNodeId(), InObject->GetHoudiniObjectTransform()))
		return false;
	
	// Update the cached data and input settings
	InObject->Update(SM, InInputSettings);

	// // If the Input mesh has a Transform offset
	// FTransform TransformOffset = InObject->Transform;
	// if (!TransformOffset.Equals(FTransform::Identity))
	// {
	// 	// Updating the Transform
	// 	HAPI_TransformEuler HapiTransform;
	// 	FHoudiniApi::TransformEuler_Init(&HapiTransform);
	// 	FHoudiniEngineUtils::TranslateUnrealTransform(TransformOffset, HapiTransform);
	//
	// 	// Set the transform on the OBJ parent
	// 	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
	// 		FHoudiniEngine::Get().GetSession(), InObject->InputObjectNodeId, &HapiTransform), false);
	// }

	return bSuccess;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForReference(
        const FString& InObjNodeName,
        UHoudiniInputObject* InObject,
        const FHoudiniInputObjectSettings& InInputSettings,
        const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForReference);

    if (!IsValid(InObject))
        return false;

    // Marshall the Object to Houdini
    FUnrealObjectInputHandle InputNodeHandle;
    HAPI_NodeId CreatedNodeId = -1;
	
	// Get the existing node id, if any
	
	// For the ref counted system the handle on the input object represents a reference node that has a single node
	// it references: the static mesh. The reference node represents InObject with its Transform (geometry input).
	{
		TSet<FUnrealObjectInputHandle> ReferencedNodes;
		if (FUnrealObjectInputUtils::GetReferencedNodes(InObject->InputNodeHandle, ReferencedNodes) && ReferencedNodes.Num() == 1)
		{
			const FUnrealObjectInputHandle Handle = ReferencedNodes.Array()[0];
			FUnrealObjectInputUtils::GetHAPINodeId(Handle, CreatedNodeId);
		}
	}

    bool bSuccess = true;
    FBox InBbox = FBox(EForceInit::ForceInit);

    const TArray<FString>& MaterialReferences
            = InInputSettings.bImportAsReferenceMaterialEnabled ?
                      InObject->GetMaterialReferences() :
                      TArray<FString>();

    bSuccess = FHoudiniInputTranslator::CreateInputNodeForReference(
            CreatedNodeId, InObject->GetObject(), InObjNodeName,
            InObject->GetTransform(), InInputSettings.bImportAsReferenceRotScaleEnabled,
            InputNodeHandle, bInputNodesCanBeDeleted,
            InInputSettings.bImportAsReferenceBboxEnabled, InBbox,
            InInputSettings.bImportAsReferenceMaterialEnabled, MaterialReferences);

	{
		// The input object can have its own transform (geometry input), so we have to create a reference node that
		// represents InObject in the new input system that references the StaticMesh asset's input node handle
		FUnrealObjectInputOptions Options;
		static constexpr bool bIsLeaf = false;
		FUnrealObjectInputIdentifier GeoInputRefNodeId(InObject, Options, bIsLeaf);
		FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(GeoInputRefNodeId, { InputNodeHandle }, InObject->InputNodeHandle, true, bInputNodesCanBeDeleted);
	}

	if (!HapiSetGeoObjectTransform(InObject->GetInputObjectNodeId(), InObject->GetHoudiniObjectTransform()))
		return false;

	// Ensure bImportAsReference is recorded as true
	FHoudiniInputObjectSettings InputSettings(InInputSettings);
	InputSettings.bImportAsReference = true;
	// Update the cached data and input settings
	InObject->Update(InObject->GetObject(), InputSettings);

    return bSuccess;
}


bool
FHoudiniInputTranslator::HapiCreateInputNodeForActorReference(
	UHoudiniInputActor* InActorObject,
	const FHoudiniInputObjectSettings& InInputSettings,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForActorReference);

	if (!IsValid(InActorObject))
		return false;

	// Get the Actor we want to send as reference
	AActor* InActor = InActorObject->GetActor();
	if (!InActor)
		return false;

	// Actors properties
	FTransform ActorTransform = InActor->GetTransform();
	FString ActorPath = InActor->GetPathName();
	FString ActorLevelPath = InActor->GetLevel()->GetPathName();
	{
		// We just want the path up to the first point
		int32 DotIndex;
		if (ActorLevelPath.FindChar('.', DotIndex))
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
			ActorLevelPath.LeftInline(DotIndex, EAllowShrinking::No);
#else
			ActorLevelPath.LeftInline(DotIndex, false);
#endif
	}

	// Component properties
	TArray<FTransform> ComponentTransforms;
	TArray<FBox> ComponentBBoxes;
	TArray<FString> ComponentReferences;
	TArray<TArray<FString>> ComponentMaterials;
	for (UHoudiniInputSceneComponent* CurComponent : InActorObject->GetActorComponents())
	{
		switch (CurComponent->Type)
		{
			case EHoudiniInputObjectType::StaticMeshComponent:
			{
				UHoudiniInputMeshComponent* InputSMC = Cast<UHoudiniInputMeshComponent>(CurComponent);
				if (InputSMC)
				{
					// Ref
					UStaticMesh* SM = InputSMC->GetStaticMesh();
					FString AssetRef = FString();
					if(IsValid(SM))
						AssetRef = UHoudiniInputObject::FormatAssetReference(SM->GetFullName());
					ComponentReferences.Add(AssetRef);

					// Bounding box
					FBox Bbox = InInputSettings.bImportAsReferenceBboxEnabled ?
						SM->GetBoundingBox() :
						FBox(EForceInit::ForceInit);
					ComponentBBoxes.Add(Bbox);

					// Transforms
					// For some reason - object merge seem to ignore point rotations/scale values?
					FTransform ComponentTransform = InputSMC->GetTransform();
					ComponentTransform.SetLocation(InputSMC->GetTransformRelativeToOwner().GetLocation());
					ComponentTransforms.Add(ComponentTransform);

					// Materials
					if (InInputSettings.bImportAsReferenceMaterialEnabled)
						ComponentMaterials.Add(InputSMC->GetMaterialReferences());
					else
						ComponentMaterials.Add(TArray<FString>());					
				}
			}			
			break;

			case EHoudiniInputObjectType::InstancedStaticMeshComponent:
			{
				UHoudiniInputInstancedMeshComponent* InputIMC = Cast<UHoudiniInputInstancedMeshComponent>(CurComponent);
				if (InputIMC)
				{
					// Ref
					UStaticMesh* SM = InputIMC->GetStaticMesh();
					FString AssetRef = FString();
					if (IsValid(SM))
						AssetRef = UHoudiniInputObject::FormatAssetReference(SM->GetFullName());
					ComponentReferences.Add(AssetRef);

					// Bounding box
					FBox Bbox = InInputSettings.bImportAsReferenceBboxEnabled ?
						SM->GetBoundingBox() :
						FBox(EForceInit::ForceInit);
					ComponentBBoxes.Add(Bbox);

					// Transforms
					ComponentTransforms.Add(InputIMC->GetTransformRelativeToOwner());

					// Materials
					if (InInputSettings.bImportAsReferenceMaterialEnabled)
						ComponentMaterials.Add(InputIMC->GetMaterialReferences());
					else
						ComponentMaterials.Add(TArray<FString>());
				}
			}
			break;

			case EHoudiniInputObjectType::SkeletalMeshComponent:
			{
				UHoudiniInputSkeletalMeshComponent* InputSKMC = Cast<UHoudiniInputSkeletalMeshComponent>(CurComponent);
				if (IsValid(InputSKMC))
				{
					// Ref
					USkeletalMesh* SKM = InputSKMC->GetSkeletalMesh();
					FString AssetRef = FString();
					if (IsValid(SKM))
						AssetRef = UHoudiniInputObject::FormatAssetReference(SKM->GetFullName());
					ComponentReferences.Add(AssetRef);

					// Bounding box
					FBox Bbox = InInputSettings.bImportAsReferenceBboxEnabled ?
						SKM->GetBounds().GetBox() :
						FBox(EForceInit::ForceInit);
					ComponentBBoxes.Add(Bbox);

					// Transforms
					ComponentTransforms.Add(InputSKMC->GetTransformRelativeToOwner());

					// Materials
					if (InInputSettings.bImportAsReferenceMaterialEnabled)
						ComponentMaterials.Add(InputSKMC->GetMaterialReferences());
					else
						ComponentMaterials.Add(TArray<FString>());
				}
			}
			break;

			case EHoudiniInputObjectType::GeometryCollectionComponent:
			{
				UHoudiniInputGeometryCollectionComponent* InputGCC = Cast<UHoudiniInputGeometryCollectionComponent>(CurComponent);
				if (IsValid(InputGCC))
				{
					// Ref
					UGeometryCollection* GC = InputGCC->GetGeometryCollection();
					FString AssetRef = FString();
					if (IsValid(GC))
						AssetRef = UHoudiniInputObject::FormatAssetReference(GC->GetFullName());
					ComponentReferences.Add(AssetRef);

					// Bounding box
					TManagedArray<FBox>& BboxArray = GC->GetGeometryCollection()->BoundingBox;
					FBox GCBbox = FBox(EForceInit::ForceInitToZero);
					if (InInputSettings.bImportAsReferenceBboxEnabled)
					{
						for (FBox& Bbox : BboxArray)
							GCBbox += Bbox;
					}
					ComponentBBoxes.Add(GCBbox);

					// Transforms
					ComponentTransforms.Add(InputGCC->GetTransformRelativeToOwner());

					// Materials
					if (InInputSettings.bImportAsReferenceMaterialEnabled)
						ComponentMaterials.Add(InputGCC->GetMaterialReferences());
					else
						ComponentMaterials.Add(TArray<FString>());
				}
			}
			break;

			case EHoudiniInputObjectType::SplineMeshComponent:
			case EHoudiniInputObjectType::SplineComponent:
			case EHoudiniInputObjectType::HoudiniSplineComponent:
			case EHoudiniInputObjectType::CameraComponent:
			case EHoudiniInputObjectType::SceneComponent:
			case EHoudiniInputObjectType::HoudiniAssetComponent:
			case EHoudiniInputObjectType::LandscapeSplinesComponent:
			{
				// Ref
				UObject* Obj = CurComponent->GetObject();
				FString AssetRef = FString();
				if (IsValid(Obj))
					AssetRef = UHoudiniInputObject::FormatAssetReference(Obj->GetFullName());
				ComponentReferences.Add(AssetRef);

				// Bounding box
				USceneComponent* SC = CurComponent->GetSceneComponent();
				FBox SCBbox = FBox(EForceInit::ForceInitToZero);
				if (InInputSettings.bImportAsReferenceBboxEnabled && IsValid(SC))
				{
					SCBbox = SC->GetLocalBounds().GetBox();
				}
				ComponentBBoxes.Add(SCBbox);

				// Transforms
				ComponentTransforms.Add(CurComponent->GetTransformRelativeToOwner());

				// Materials
				if (InInputSettings.bImportAsReferenceMaterialEnabled)
					ComponentMaterials.Add(CurComponent->GetMaterialReferences());
				else
					ComponentMaterials.Add(TArray<FString>());
			}
			break;


			default:
			{
				// Do Nothing
			}
			break;
		}
	}

	// The identifier to the node in the input system
	FString InputNodeName;
	HAPI_NodeId ParentNodeId = -1;
	HAPI_NodeId NodeId = -1;
	FUnrealObjectInputHandle ParentHandle;
	FUnrealObjectInputIdentifier Identifier;
	{
		// Build the identifier for the entry in the manager
		constexpr bool bIsLeaf = true;
		FUnrealObjectInputOptions Options;
		Options.bImportAsReference = true;
		Options.bImportAsReferenceRotScaleEnabled = InInputSettings.bImportAsReferenceRotScaleEnabled;
		Identifier = FUnrealObjectInputIdentifier(InActor, Options, bIsLeaf);

		// If the entry exists in the manager, the associated HAPI nodes are valid, and it is not marked as dirty, then
		// return the existing entry
		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::NodeExistsAndIsNotDirty(Identifier, Handle))
		{
			if (FUnrealObjectInputUtils::GetHAPINodeId(Handle, NodeId))
			{
				// Make sure the node cant be deleted if needed
				if (!bInputNodesCanBeDeleted)
					FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(Handle, bInputNodesCanBeDeleted);

				//OutHandle = Handle;
				//InputNodeId = NodeId;
				return true;
			}
		}

		if (!FUnrealObjectInputUtils::GetHAPINodeId(Handle, NodeId))
			NodeId = -1;

		// If the entry does not exist, or is invalid, then we need create it
		FUnrealObjectInputUtils::GetDefaultInputNodeName(Identifier, InputNodeName);
		// Create any parent/container nodes that we would need, and get the node id of the immediate parent
		if (FUnrealObjectInputUtils::EnsureParentsExist(Identifier, ParentHandle, bInputNodesCanBeDeleted) && ParentHandle.IsValid())
			FUnrealObjectInputUtils::GetHAPINodeId(ParentHandle, ParentNodeId);
	}

	// Create a single input node
	HAPI_NodeId NewNodeId = -1;	
	if (ParentNodeId >= 0)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateInputNode(InputNodeName, NewNodeId, ParentNodeId), false);
	}
	else
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateInputNode(
			FHoudiniEngine::Get().GetSession(), -1, &NewNodeId, TCHAR_TO_UTF8(*InputNodeName)), false);
	}

	// Check if we have a valid id for this new input asset.
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(NewNodeId))
		return false;

	// We have now created a valid new input node, we can delete the previous one
	HAPI_NodeId PreviousInputNodeId = NodeId;
	if (PreviousInputNodeId >= 0)
	{
		// Get the parent OBJ node ID before deleting!
		HAPI_NodeId PreviousInputOBJNode = FHoudiniEngineUtils::HapiGetParentNodeId(PreviousInputNodeId);
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
			FHoudiniEngine::Get().GetSession(), PreviousInputNodeId))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input node for %s."), *InputNodeName);
		}

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
			FHoudiniEngine::Get().GetSession(), PreviousInputOBJNode))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input OBJ node for %s."), *InputNodeName);
		}
	}

	// We will create one point per component
	int32 NumPoints = ComponentReferences.Num();

	// Create and initialize a part with a point attribute
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	PartInfo.attributeCounts[HAPI_ATTROWNER_POINT] = 1;
	PartInfo.vertexCount = 0;
	PartInfo.faceCount = 0;
	PartInfo.pointCount = NumPoints;
	PartInfo.type = HAPI_PARTTYPE_MESH;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0, &PartInfo), false);

	// Point Position Attribute
	{
		// Create point attribute info for P.
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = NumPoints;
		AttributeInfoPoint.tupleSize = 3;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
			HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint), false);

		// Extract/Convert the positions from the Transform
		TArray<float> AllPositions;
		AllPositions.SetNum(3 * NumPoints);
		for (int32 Idx = 0; Idx < ComponentTransforms.Num(); Idx++)
		{
			FVector3f CurPos = (FVector3f)ComponentTransforms[Idx].GetLocation();
			AllPositions[3 * Idx] = CurPos.X / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			AllPositions[3 * Idx + 1] = CurPos.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			AllPositions[3 * Idx + 2] = CurPos.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION;
		}

		// Now that we have raw positions, we can upload them for our attribute.
		FHoudiniHapiAccessor Accessor(NewNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoPoint, AllPositions), false);
	}

	if(InInputSettings.bImportAsReferenceRotScaleEnabled)
	{
		// Extract/Convert the rotation/scale values from the Transform 
		TArray<float> AllRotations;
		AllRotations.SetNum(4 * NumPoints);
		TArray<float> AllScales;
		AllScales.SetNum(3 * NumPoints);
		for (int32 Idx = 0; Idx < ComponentTransforms.Num(); Idx++)
		{
			FQuat InputRotation = ComponentTransforms[Idx].GetRotation();
			AllRotations[4 * Idx] = (float)InputRotation.X;
			AllRotations[4 * Idx + 1] = (float)InputRotation.Z;
			AllRotations[4 * Idx + 2] = (float)InputRotation.Y;
			AllRotations[4 * Idx + 3] = (float)-InputRotation.W;

			FVector3f InputScale = (FVector3f)ComponentTransforms[Idx].GetScale3D();
			AllScales[3 * Idx] = InputScale.X;
			AllScales[3 * Idx + 1] = InputScale.Z;
			AllScales[3 * Idx + 2] = InputScale.Y;
		}

		// Create ROTATION attribute info
		HAPI_AttributeInfo AttributeInfoRotation;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoRotation);
		AttributeInfoRotation.count = NumPoints;
		AttributeInfoRotation.tupleSize = 4;
		AttributeInfoRotation.exists = true;
		AttributeInfoRotation.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoRotation.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoRotation.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NewNodeId, 0,
			HAPI_UNREAL_ATTRIB_ROTATION,
			&AttributeInfoRotation), false);

		//we can now upload to our attribute.
		FHoudiniHapiAccessor Accessor(NewNodeId, 0, HAPI_UNREAL_ATTRIB_ROTATION);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoRotation, AllRotations), false);

		// Create SCALE attribute info
		HAPI_AttributeInfo AttributeInfoScale;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoScale);
		AttributeInfoScale.count = NumPoints;
		AttributeInfoScale.tupleSize = 3;
		AttributeInfoScale.exists = true;
		AttributeInfoScale.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoScale.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoScale.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NewNodeId, 0,
			HAPI_UNREAL_ATTRIB_SCALE,
			&AttributeInfoScale), false);

		//we can now upload to our attribute.
		Accessor.Init(NewNodeId, 0, HAPI_UNREAL_ATTRIB_SCALE);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoScale, AllScales), false);
	}

	if (InInputSettings.bImportAsReferenceBboxEnabled)
	{
		// Extract/Convert the bbox min/max values from the Transform 
		TArray<float> AllBBoxMins;
		AllBBoxMins.SetNum(3 * NumPoints);
		TArray<float> AllBBoxMaxs;
		AllBBoxMaxs.SetNum(3 * NumPoints);
		for (int32 Idx = 0; Idx < ComponentBBoxes.Num(); Idx++)
		{
			FVector3f CurMin = (FVector3f)ComponentBBoxes[Idx].Min;
			FVector3f CurMax = (FVector3f)ComponentBBoxes[Idx].Max;

			AllBBoxMins[3 * Idx] = CurMin.X / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			AllBBoxMins[3 * Idx + 1] = CurMin.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			AllBBoxMins[3 * Idx + 2] = CurMin.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION;

			AllBBoxMaxs[3 * Idx] = CurMax.X / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			AllBBoxMaxs[3 * Idx + 1] = CurMax.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			AllBBoxMaxs[3 * Idx + 2] = CurMax.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION;
		}

		// Create attribute info for both bbox min and bbox max
		HAPI_AttributeInfo AttributeInfoBboxPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoBboxPoint);
		AttributeInfoBboxPoint.count = NumPoints;
		AttributeInfoBboxPoint.tupleSize = 3;
		AttributeInfoBboxPoint.exists = true;
		AttributeInfoBboxPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoBboxPoint.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoBboxPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		// bbox min
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
			HAPI_UNREAL_ATTRIB_BBOX_MIN, &AttributeInfoBboxPoint), false);

		FHoudiniHapiAccessor Accessor(NewNodeId, 0, HAPI_UNREAL_ATTRIB_BBOX_MIN);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoBboxPoint, AllBBoxMins), false);

		// bbox max
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
			HAPI_UNREAL_ATTRIB_BBOX_MAX, &AttributeInfoBboxPoint), false);

		Accessor.Init(NewNodeId, 0, HAPI_UNREAL_ATTRIB_BBOX_MAX);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoBboxPoint, AllBBoxMaxs), false);
	}

	// Material Reference String Array Attribute
	if (InInputSettings.bImportAsReferenceMaterialEnabled)
	{
		// Create point attribute info.
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = NumPoints;
		AttributeInfoPoint.tupleSize = 1;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		int32 MaxNumMaterials = 0;
		for (int32 CompIdx = 0; CompIdx < ComponentMaterials.Num(); CompIdx++)
		{
			if (ComponentMaterials[CompIdx].Num() > MaxNumMaterials)
				MaxNumMaterials = ComponentMaterials[CompIdx].Num();
		}

		// We set it to be multiple string attributes rather than a single string array attribute to not conflict
		// with any existing HDA's that use the attribute name unreal_material
		for (int32 MatIdx = 0; MatIdx < MaxNumMaterials; MatIdx++)
		{
			FString AttributeName = HAPI_UNREAL_ATTRIB_MATERIAL;
			if (MatIdx > 0)
				AttributeName.AppendInt(MatIdx);

			// Create an array for the current Material 
			TArray<FString> CurrentMaterials;
			CurrentMaterials.SetNum(ComponentMaterials.Num());
			for (int32 CompIdx = 0; CompIdx < ComponentMaterials.Num(); CompIdx++)
			{
				if (ComponentMaterials[CompIdx].IsValidIndex(MatIdx))
					CurrentMaterials[CompIdx] = ComponentMaterials[CompIdx][MatIdx];
				else
					CurrentMaterials[CompIdx] = FString();
			}

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
				TCHAR_TO_ANSI(*AttributeName), &AttributeInfoPoint), false);

			FHoudiniHapiAccessor Accessor(NewNodeId, 0, TCHAR_TO_ANSI(*AttributeName));
			HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoPoint, CurrentMaterials), false);
		}
	}

	// Unreal Reference String Attribute
	{
		// Create point attribute info.
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = NumPoints;
		AttributeInfoPoint.tupleSize = 1;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
			HAPI_UNREAL_ATTRIB_INSTANCE_OVERRIDE, &AttributeInfoPoint), false);

		FHoudiniHapiAccessor Accessor(NewNodeId, 0, HAPI_UNREAL_ATTRIB_INSTANCE_OVERRIDE);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoPoint, ComponentReferences), false);
	}

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(NewNodeId), false);

	FUnrealObjectInputHandle OutHandle;
	{
		// Record the node in the manager
		const HAPI_NodeId ObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(NewNodeId);
		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::AddNodeOrUpdateNode(Identifier, NewNodeId, Handle, ObjectNodeId, nullptr, bInputNodesCanBeDeleted))
			OutHandle = Handle;

		InActorObject->SetInputObjectNodeId(ObjectNodeId);
		InActorObject->SetInputNodeId(NewNodeId);
		InActorObject->InputNodeHandle = OutHandle;
	}

	if (!HapiSetGeoObjectTransform(InActorObject->GetInputObjectNodeId(), InActorObject->GetHoudiniObjectTransform()))
		return false;

	/*
	// Ensure bImportAsReference is recorded as true
	FHoudiniInputObjectSettings InputSettings(InInputSettings);
	InputSettings.bImportAsReference = true;
	// Update the cached data and input settings
	InActorObject->Update(InObject->GetObject(), InputSettings);
	*/

	return true;
}


bool
FHoudiniInputTranslator::HapiCreateInputNodeForAnimation(
	const FString& InObjNodeName,
	UHoudiniInputAnimation* InObject,
	const FHoudiniInputObjectSettings& InInputSettings,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForAnimation);

	if (!IsValid(InObject))
		return false;

	UAnimSequence* Animation = InObject->GetAnimation();
	if (!IsValid(Animation))
		return true;

	FString SKName = InObjNodeName + TEXT("_") + Animation->GetName();
	FHoudiniEngineUtils::SanitizeHAPIVariableName(SKName);

	FUnrealObjectInputHandle AnimInputNodeHandle;
	HAPI_NodeId CreatedNodeId = -1;

	// Get the existing node id, if any
	
	// For the ref counted system the handle on the input object represents a reference node that has a single node
	// it references: the animation. The reference node represents InObject with its Transform (geometry input).
	{
		TSet<FUnrealObjectInputHandle> ReferencedNodes;
		if (FUnrealObjectInputUtils::GetReferencedNodes(InObject->InputNodeHandle, ReferencedNodes) && ReferencedNodes.Num() == 1)
		{
			const FUnrealObjectInputHandle Handle = ReferencedNodes.Array()[0];
			FUnrealObjectInputUtils::GetHAPINodeId(Handle, CreatedNodeId);
		}
	}

	// Marshall the SkeletalMesh to Houdini
	bool bSuccess = true;


	if (InInputSettings.bImportAsReference)
	{
		// Get the SM's bbox
		FBox InBbox = FBox(EForceInit::ForceInit);

		//FBox InBbox = bImportAsReferenceBboxEnabled ?
		//	SkelMesh->GetBounds().GetBox() :
		//	FBox(EForceInit::ForceInit);

		const TArray<FString>& MaterialReferences = InInputSettings.bImportAsReferenceMaterialEnabled ?
			InObject->GetMaterialReferences() :
			TArray<FString>();

		bSuccess = FHoudiniInputTranslator::CreateInputNodeForReference(
			CreatedNodeId,
			Animation,
			SKName,
			InObject->GetTransform(),
			InInputSettings.bImportAsReferenceRotScaleEnabled,
			AnimInputNodeHandle,
			bInputNodesCanBeDeleted,
			InInputSettings.bImportAsReferenceBboxEnabled,
			InBbox,
			InInputSettings.bImportAsReferenceMaterialEnabled,
			MaterialReferences);
	}
	else
	{
		bSuccess = FUnrealAnimationTranslator::HapiCreateInputNodeForAnimation(
			Animation, CreatedNodeId, SKName, AnimInputNodeHandle, false, false, false, bInputNodesCanBeDeleted);
		if (!bSuccess)
		{
			return false;
		}

	}

	{
		// The animation can have its own transform (geometry input), so we have to create a reference node that
		// represents InObject in the new input system that references the UAnimSequence asset's input node handle
		FUnrealObjectInputOptions Options;
		static constexpr bool bIsLeaf = false;
		FUnrealObjectInputIdentifier GeoInputRefNodeId(InObject, Options, bIsLeaf);
		FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(GeoInputRefNodeId, { AnimInputNodeHandle }, InObject->InputNodeHandle, true, bInputNodesCanBeDeleted);
	}

	if (!HapiSetGeoObjectTransform(InObject->GetInputObjectNodeId(), InObject->GetHoudiniObjectTransform()))
		return false;

	// Update the cached data and input settings
	InObject->Update(Animation, InInputSettings);




	return bSuccess;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForSkeletalMesh(
	const FString& InObjNodeName,
	UHoudiniInputSkeletalMesh* InObject,
	const FHoudiniInputObjectSettings& InInputSettings,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForSkeletalMesh);

	if (!IsValid(InObject))
		return false;

	USkeletalMesh* SkelMesh = InObject->GetSkeletalMesh();
	if (!IsValid(SkelMesh))
		return true;

	FString SKName = InObjNodeName + TEXT("_") + SkelMesh->GetName();
	FHoudiniEngineUtils::SanitizeHAPIVariableName(SKName);

	FUnrealObjectInputHandle SKMInputNodeHandle;
	HAPI_NodeId CreatedNodeId = -1;

	// Get the existing node id, if any
	
	// For the ref counted system the handle on the input object represents a reference node that has a single node
	// it references: the skeletal mesh. The reference node represents InObject with its Transform (geometry input).
	{
		TSet<FUnrealObjectInputHandle> ReferencedNodes;
		if (FUnrealObjectInputUtils::GetReferencedNodes(InObject->InputNodeHandle, ReferencedNodes) && ReferencedNodes.Num() == 1)
		{
			const FUnrealObjectInputHandle Handle = ReferencedNodes.Array()[0];
			FUnrealObjectInputUtils::GetHAPINodeId(Handle, CreatedNodeId);
		}
	}

	// Marshall the SkeletalMesh to Houdini
	bool bSuccess = true;
	if (InInputSettings.bImportAsReference)
	{
		// Get the SM's bbox
		FBox InBbox = InInputSettings.bImportAsReferenceBboxEnabled ?
			SkelMesh->GetBounds().GetBox() :
			FBox(EForceInit::ForceInit);

		const TArray<FString>& MaterialReferences = InInputSettings.bImportAsReferenceMaterialEnabled ?
			InObject->GetMaterialReferences() :
			TArray<FString>();

		bSuccess = FHoudiniInputTranslator::CreateInputNodeForReference(
			CreatedNodeId,
			SkelMesh,
			SKName,
			InObject->GetTransform(),
			InInputSettings.bImportAsReferenceRotScaleEnabled,
			SKMInputNodeHandle,
			bInputNodesCanBeDeleted,
			InInputSettings.bImportAsReferenceBboxEnabled,
			InBbox,
			InInputSettings.bImportAsReferenceMaterialEnabled,
			MaterialReferences);
	}
	else
	{
		bSuccess = FUnrealSkeletalMeshTranslator::HapiCreateInputNodeForSkeletalMesh(
			SkelMesh, CreatedNodeId, SKName, SKMInputNodeHandle, nullptr, InInputSettings.bExportLODs, InInputSettings.bExportSockets, InInputSettings.bExportColliders, true, bInputNodesCanBeDeleted, InInputSettings.bExportMaterialParameters );

		if(!bSuccess)
		{
			return false;
		}

	}

	{
		// The skeletal mesh can have its own transform (geometry input), so we have to create a reference node that
		// represents InObject in the new input system that references the SkeletalMesh asset's input node handle
		FUnrealObjectInputOptions Options;
		static constexpr bool bIsLeaf = false;
		FUnrealObjectInputIdentifier GeoInputRefNodeId(InObject, Options, bIsLeaf);
		FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(GeoInputRefNodeId, { SKMInputNodeHandle }, InObject->InputNodeHandle, true, bInputNodesCanBeDeleted);
	}

	if (!HapiSetGeoObjectTransform(InObject->GetInputObjectNodeId(), InObject->GetHoudiniObjectTransform()))
		return false;

	// Update the cached data and input settings
	InObject->Update(SkelMesh, InInputSettings);

	return bSuccess;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForSkeletalMeshComponent(
	const FString& InObjNodeName,
	UHoudiniInputSkeletalMeshComponent* InObject,
	const FHoudiniInputObjectSettings& InInputSettings,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForSkeletalMeshComponent);

	if (!IsValid(InObject))
		return false;

	USkeletalMeshComponent* SKC = InObject->GetSkeletalMeshComponent();
	if (!IsValid(SKC))
		return true;

	// Get the component's Skeletal Mesh
	USkeletalMesh* SK = InObject->GetSkeletalMesh();
	if (!IsValid(SK))
		return true;

	HAPI_NodeId CreatedNodeId = InObject->GetInputNodeId();

	// Marshall the Skeletal Mesh to Houdini
	FString SKCName = InObjNodeName + TEXT("_") + SKC->GetName();
	FHoudiniEngineUtils::SanitizeHAPIVariableName(SKCName);

	FUnrealObjectInputHandle InputNodeHandle;
	bool bSuccess = true;
	if (InInputSettings.bImportAsReference)
	{
		FTransform ImportAsReferenceTransform = InObject->GetTransform();
		
		// Previously, ImportAsReferenceTransform was multiplied by
		// InActorTransform.Inverse() if bKeepWorldTransform was true,
		// but this created a double transform issue.
		ImportAsReferenceTransform.SetLocation(FVector::ZeroVector);

		// Get the SM's bbox
		FBox InBbox = InInputSettings.bImportAsReferenceBboxEnabled ?
			SK->GetBounds().GetBox() :
			FBox(EForceInit::ForceInit);

		const TArray<FString>& MaterialReferences = InInputSettings.bImportAsReferenceMaterialEnabled ?
			InObject->GetMaterialReferences() :
			TArray<FString>();

		bSuccess = FHoudiniInputTranslator::CreateInputNodeForReference(
			CreatedNodeId,
			SK,
			SKCName,
			ImportAsReferenceTransform,
			InInputSettings.bImportAsReferenceRotScaleEnabled,
			InputNodeHandle,
			bInputNodesCanBeDeleted,
			InInputSettings.bImportAsReferenceBboxEnabled,
			InBbox,
			InInputSettings.bImportAsReferenceMaterialEnabled,
			MaterialReferences);
	}
	else
	{
		bSuccess = FUnrealSkeletalMeshTranslator::HapiCreateInputNodeForSkeletalMesh(
			SK, CreatedNodeId, SKCName, InputNodeHandle, SKC, InInputSettings.bExportLODs, InInputSettings.bExportSockets, InInputSettings.bExportColliders, true, bInputNodesCanBeDeleted, InInputSettings.bExportMaterialParameters);
	}

	// Create/update the node in the input manager
	{
		FUnrealObjectInputOptions Options = InputNodeHandle.GetIdentifier().GetOptions();
		constexpr bool bIsLeaf = false;
		FUnrealObjectInputIdentifier SKCIdentifier(SKC, Options, bIsLeaf);
		FUnrealObjectInputHandle Handle;
		if (!FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(SKCIdentifier, { InputNodeHandle }, Handle, true, bInputNodesCanBeDeleted))
			return false;

		FUnrealObjectInputUtils::GetHAPINodeId(Handle, CreatedNodeId);
		InObject->InputNodeHandle = Handle;

		// Create the output modifier chain if missing
		const FName ModifierChainName(FUnrealObjectInputNode::OutputChainName);
		if (!FUnrealObjectInputUtils::DoesModifierChainExist(InObject->InputNodeHandle, ModifierChainName))
			FUnrealObjectInputUtils::AddModifierChain(InObject->InputNodeHandle, ModifierChainName, CreatedNodeId);
		else
			FUnrealObjectInputUtils::SetModifierChainNodeToConnectTo(InObject->InputNodeHandle, ModifierChainName, CreatedNodeId);

		// Make sure that material overrides modifier exists and is correctly configured for this component's input node
		if (FUnrealObjectInputModifier* MatOverridesModifier = FUnrealObjectInputUtils::FindFirstModifierOfType(InObject->InputNodeHandle, ModifierChainName, EUnrealObjectInputModifierType::MaterialOverrides))
		{
			FUnrealObjectInputMaterialOverrides* const MatOverrides = static_cast<FUnrealObjectInputMaterialOverrides*>(MatOverridesModifier);
			if (InInputSettings.bImportAsReference)
			{
				if (InInputSettings.bImportAsReferenceMaterialEnabled)
				{
					if (MatOverrides)
						MatOverrides->SetUsePrimWrangle(false);
				}
				else
				{
					FUnrealObjectInputUtils::DestroyModifier(InObject->InputNodeHandle, ModifierChainName, MatOverridesModifier);
				}
			}
			else
			{
				if (MatOverrides)
					MatOverrides->SetUsePrimWrangle(true);
			}
		}
		else
		{
			if (InInputSettings.bImportAsReference)
			{
				if (InInputSettings.bImportAsReferenceMaterialEnabled)
					FUnrealObjectInputUtils::CreateAndAddModifier<FUnrealObjectInputMaterialOverrides>(InObject->InputNodeHandle, ModifierChainName, SKC, false);
			}
			else
			{
				FUnrealObjectInputUtils::CreateAndAddModifier<FUnrealObjectInputMaterialOverrides>(InObject->InputNodeHandle, ModifierChainName, SKC, true);
			}
		}

		// Ensure that the physical material override modifier exists for this component's input node and is correctly configured
		const HAPI_AttributeOwner PhysMatOverrideAttrOwner = InInputSettings.bImportAsReference ? HAPI_ATTROWNER_POINT : HAPI_ATTROWNER_PRIM;
		if (FUnrealObjectInputModifier* PhysMatOverrideModifier = FUnrealObjectInputUtils::FindFirstModifierOfType(InObject->InputNodeHandle, ModifierChainName, EUnrealObjectInputModifierType::PhysicalMaterialOverride))
		{
			if (FUnrealObjectInputPhysicalMaterialOverride* const PhysMatOverride = static_cast<FUnrealObjectInputPhysicalMaterialOverride*>(PhysMatOverrideModifier))
				PhysMatOverride->SetAttributeOwner(PhysMatOverrideAttrOwner);
		}
		else
		{
			FUnrealObjectInputUtils::CreateAndAddModifier<FUnrealObjectInputPhysicalMaterialOverride>(InObject->InputNodeHandle, ModifierChainName, SKC, PhysMatOverrideAttrOwner);
		}

		// Data layer Modifier
		FUnrealObjectInputModifier* DataLayerModifier = FUnrealObjectInputUtils::FindFirstModifierOfType(InObject->InputNodeHandle, ModifierChainName, EUnrealObjectInputModifierType::DataLayerGroups);
		if (!DataLayerModifier)
		{
			FUnrealObjectInputUtils::CreateAndAddModifier<FUnrealObjectInputDataLayer>(InObject->InputNodeHandle, ModifierChainName, SKC->GetOwner());
		}

		// HLODs
		FUnrealObjectInputModifier* HLODModifier = FUnrealObjectInputUtils::FindFirstModifierOfType(Handle, ModifierChainName, EUnrealObjectInputModifierType::HLODAttributes);
		if (!HLODModifier)
		{
			FUnrealObjectInputUtils::CreateAndAddModifier<FUnrealObjectInputHLODAttributes>(Handle, ModifierChainName, SKC->GetOwner());
		}

		// Update all modifiers
		FUnrealObjectInputUtils::UpdateAllModifierChains(InObject->InputNodeHandle);
	}

	// Update this input object's OBJ NodeId
	InObject->SetInputNodeId(CreatedNodeId);
	InObject->SetInputObjectNodeId(FHoudiniEngineUtils::HapiGetParentNodeId(CreatedNodeId));

	// Update this input object's cache data
	InObject->Update(SKC, InInputSettings);

	// Update the component's transform
	FTransform ComponentTransform = InObject->GetHoudiniObjectTransform();

	{
		// convert to HAPI_Transform
		HAPI_TransformEuler HapiTransform;
		FHoudiniApi::TransformEuler_Init(&HapiTransform);
		FHoudiniEngineUtils::TranslateUnrealTransform(ComponentTransform, HapiTransform);

		// Set the transform on the OBJ parent
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
			FHoudiniEngine::Get().GetSession(), InObject->GetInputObjectNodeId(), &HapiTransform), false);
	}

	return bSuccess;
}

bool 
FHoudiniInputTranslator::HapiCreateInputNodeForGeometryCollection(
	const FString& InObjNodeName,
	UHoudiniInputGeometryCollection* InObject,
	const FHoudiniInputObjectSettings& InInputSettings,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForGeometryCollection);

	if (!IsValid(InObject))
		return false;

	UGeometryCollection* GeometryCollection = InObject->GetGeometryCollection();
	if (!IsValid(GeometryCollection))
		return false;

	FString GCName = InObjNodeName + TEXT("_") + GeometryCollection->GetName();
	FHoudiniEngineUtils::SanitizeHAPIVariableName(GCName);

	// TODO: Add support for the new input sytem!
	FUnrealObjectInputHandle GCInputNodeHandle;
	HAPI_NodeId CreatedNodeId = -1;

	// Get the existing node id, if any
	
	// For the ref counted system the handle on the input object represents a reference node that has a single node
	// it references: the geometry collection. The reference node represents InObject with its Transform (geometry input).
	{
		TSet<FUnrealObjectInputHandle> ReferencedNodes;
		if (FUnrealObjectInputUtils::GetReferencedNodes(InObject->InputNodeHandle, ReferencedNodes) && ReferencedNodes.Num() == 1)
		{
			const FUnrealObjectInputHandle Handle = ReferencedNodes.Array()[0];
			FUnrealObjectInputUtils::GetHAPINodeId(Handle, CreatedNodeId);
		}
	}

	// Marshall the GeometryCollection to Houdini
	bool bSuccess = true;
	if (InInputSettings.bImportAsReference) 
	{
		TManagedArray<FBox>& BboxArray = GeometryCollection->GetGeometryCollection()->BoundingBox;
		FBox InBbox = FBox(EForceInit::ForceInitToZero);
		if (InInputSettings.bImportAsReferenceBboxEnabled)
		{
			for (FBox& Bbox : BboxArray)
				InBbox += Bbox;
		}

		const TArray<FString>& MaterialReferences = InInputSettings.bImportAsReferenceMaterialEnabled ?
			InObject->GetMaterialReferences() :
			TArray<FString>();

		bSuccess = FHoudiniInputTranslator::CreateInputNodeForReference(
			CreatedNodeId,
			GeometryCollection,
			GCName,
			InObject->GetTransform(),
			InInputSettings.bImportAsReferenceRotScaleEnabled,
			GCInputNodeHandle,
			bInputNodesCanBeDeleted,
			InInputSettings.bImportAsReferenceBboxEnabled,
			InBbox,
			InInputSettings.bImportAsReferenceMaterialEnabled,
			MaterialReferences);
	}
	else 
	{
		bSuccess = FUnrealGeometryCollectionTranslator::HapiCreateInputNodeForGeometryCollection(
			GeometryCollection,
			CreatedNodeId,
			GCName,
			GCInputNodeHandle,
			InInputSettings.bExportMaterialParameters,
			nullptr,
			bInputNodesCanBeDeleted);
	}
	
	{
		// The geometry collection can have its own transform (geometry input), so we have to create a reference node that
		// represents InObject in the new input system that references the GeometryCollection asset's input node handle
		FUnrealObjectInputOptions Options;
		static constexpr bool bIsLeaf = false;
		FUnrealObjectInputIdentifier GeoInputRefNodeId(InObject, Options, bIsLeaf);
		FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(GeoInputRefNodeId, { GCInputNodeHandle }, InObject->InputNodeHandle, true, bInputNodesCanBeDeleted);
	}

	if (!HapiSetGeoObjectTransform(InObject->GetInputObjectNodeId(), InObject->GetHoudiniObjectTransform()))
		return false;

	// Update the cached data and input settings
	InObject->Update(GeometryCollection, InInputSettings);
	
	// // If the Input mesh has a Transform offset
	// FTransform TransformOffset = InObject->Transform;
	// if (!TransformOffset.Equals(FTransform::Identity))
	// {
	// 	// Updating the Transform
	// 	HAPI_TransformEuler HapiTransform;
	// 	FHoudiniApi::TransformEuler_Init(&HapiTransform);
	// 	FHoudiniEngineUtils::TranslateUnrealTransform(TransformOffset, HapiTransform);
	//
	// 	// Set the transform on the OBJ parent
	// 	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
	// 		FHoudiniEngine::Get().GetSession(), InObject->InputObjectNodeId, &HapiTransform), false);
	// }

	return bSuccess;
}

bool 
FHoudiniInputTranslator::HapiCreateInputNodeForGeometryCollectionComponent(
	const FString& InObjNodeName,
	UHoudiniInputGeometryCollectionComponent* InObject,
	const FHoudiniInputObjectSettings& InInputSettings,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForGeometryCollectionComponent);

	if (!IsValid(InObject))
		return false;

	UGeometryCollectionComponent* GCC = InObject->GetGeometryCollectionComponent();
	if (!IsValid(GCC))
		return true;

	// Get the component's GeometryCollection
	UGeometryCollection* GC = InObject->GetGeometryCollection();
	if (!IsValid(GC))
		return true;

	HAPI_NodeId CreatedNodeId = InObject->GetInputNodeId();
	
	// Marshall the GeometryCollection to Houdini
	FString GCCName = InObjNodeName + TEXT("_") + GCC->GetName();
	FHoudiniEngineUtils::SanitizeHAPIVariableName(GCCName);

	FUnrealObjectInputHandle InputNodeHandle;
	bool bSuccess = true;
	if (InInputSettings.bImportAsReference) 
	{
		FTransform ImportAsReferenceTransform = InObject->GetTransform();
		
		// Previously, ImportAsReferenceTransform was multiplied by
		// InActorTransform.Inverse() if bKeepWorldTransform was true,
		// but this created a double transform issue.
		ImportAsReferenceTransform.SetLocation(FVector::ZeroVector);

		TManagedArray<FBox>& BboxArray = GC->GetGeometryCollection()->BoundingBox;
		FBox InBbox = FBox(EForceInit::ForceInitToZero);
		if (InInputSettings.bImportAsReferenceBboxEnabled)
		{
			for (FBox& Bbox : BboxArray)
				InBbox += Bbox;
		}

		const TArray<FString>& MaterialReferences = InInputSettings.bImportAsReferenceMaterialEnabled ?
			InObject->GetMaterialReferences() :
			TArray<FString>();

		bSuccess = FHoudiniInputTranslator::CreateInputNodeForReference(
			CreatedNodeId,
			GC,
			GCCName,
			ImportAsReferenceTransform,
			InInputSettings.bImportAsReferenceRotScaleEnabled,
			InputNodeHandle,
			bInputNodesCanBeDeleted,
			InInputSettings.bImportAsReferenceBboxEnabled,
			InBbox,
			InInputSettings.bImportAsReferenceMaterialEnabled,
			MaterialReferences);
	}
	else 
	{
		bSuccess = FUnrealGeometryCollectionTranslator::HapiCreateInputNodeForGeometryCollection(
			GC,
			CreatedNodeId,
			GCCName,
			InputNodeHandle,
			InInputSettings.bExportMaterialParameters,
			GCC,
			bInputNodesCanBeDeleted);
	}
	
	// Create/update the node in the input manager
	{
		FUnrealObjectInputOptions Options = InputNodeHandle.GetIdentifier().GetOptions();
		constexpr bool bIsLeaf = false;
		FUnrealObjectInputIdentifier GCCIdentifier(GCC, Options, bIsLeaf);
		FUnrealObjectInputHandle Handle;
		if (!FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(GCCIdentifier, { InputNodeHandle }, Handle, true, bInputNodesCanBeDeleted))
			return false;

		FUnrealObjectInputUtils::GetHAPINodeId(Handle, CreatedNodeId);
		InObject->InputNodeHandle = Handle;
	}
	
	// Update this input object's OBJ NodeId
	InObject->SetInputNodeId(CreatedNodeId);
	InObject->SetInputObjectNodeId(FHoudiniEngineUtils::HapiGetParentNodeId(CreatedNodeId));

	// Update this input object's cache data
	InObject->Update(GCC, InInputSettings);

	// Update the component's transform
	FTransform ComponentTransform = InObject->GetHoudiniObjectTransform();
	{
		// convert to HAPI_Transform
		HAPI_TransformEuler HapiTransform;
		FHoudiniApi::TransformEuler_Init(&HapiTransform);
		FHoudiniEngineUtils::TranslateUnrealTransform(ComponentTransform, HapiTransform);

		// Set the transform on the OBJ parent
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(FHoudiniEngine::Get().GetSession(), InObject->GetInputObjectNodeId(), &HapiTransform), false);
	}

	return bSuccess;
}


bool
FHoudiniInputTranslator::HapiCreateInputNodeForSceneComponent(
	const FString& InObjNodeName,
	UHoudiniInputSceneComponent* InObject,
	const FHoudiniInputObjectSettings& InInputSettings,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForSceneComponent);

	if (!IsValid(InObject))
		return false;

	USceneComponent* SceneComp = InObject->GetSceneComponent();
	if (!IsValid(SceneComp))
		return true;

	// Get the Scene Component's transform
	FTransform TransformOffset = InObject->GetTransform();

	// Get the parent Actor's transform
	FTransform ParentTransform = InObject->ActorTransform;

	// Don't do that!
	return false;
	/*
	// TODO
	// Support this type of input object
	return HapiCreateInputNodeForObject(InObjNodeName, InObject);
	*/
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForStaticMeshComponent(
	const FString& InObjNodeName, 
	UHoudiniInputMeshComponent* InObject,
	const FHoudiniInputObjectSettings& InInputSettings,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForStaticMeshComponent);

	if (!IsValid(InObject))
		return false;

	UStaticMeshComponent* SMC = InObject->GetStaticMeshComponent();

	if (!IsValid(SMC))
		return true;

	// Get the component's Static Mesh
	UStaticMesh* SM = SMC->GetStaticMesh();
	if (!IsValid(SM))
		return true;

	HAPI_NodeId CreatedNodeId = InObject->GetInputNodeId();

	// Marshall the Static Mesh to Houdini
	FString SMCName = InObjNodeName + TEXT("_") + SMC->GetName();
	FHoudiniEngineUtils::SanitizeHAPIVariableName(SMCName);

	// Does the component generate unique to it, or does it use an asset directly? In cases where the component
	// generates its own data (perhaps derived from an asset, such as a static mesh) there will be no separation
	// between the component and the data (asset, for example StaticMesh input) in the ref counted input system. For
	// example StaticMeshComponent uses a StaticMesh, those create separate nodes for the component and the asset (and
	// its variations) in the input system. But a SplineMeshComponent generates a deformed mesh unique to it, so
	// the component's node also acts as the main/reference node for what would be the asset data it uses (although
	// additional nodes can be created for options/variations).
	const bool bComponentGeneratesData = SMC->IsA<USplineMeshComponent>();

	FUnrealObjectInputHandle InputNodeHandle;
	bool bSuccess = true;
	if (InInputSettings.bImportAsReference) 
	{
		FTransform ImportAsReferenceTransform = InObject->GetTransform();

		// Previously, ImportAsReferenceTransform was multiplied by
		// InActorTransform.Inverse() if bKeepWorldTransform was true,
		// but this created a double transform issue.
		ImportAsReferenceTransform.SetLocation(FVector::ZeroVector);

		FBox InBbox = InInputSettings.bImportAsReferenceBboxEnabled ?
			SM->GetBoundingBox() :
			FBox(EForceInit::ForceInit);

		const TArray<FString>& MaterialReferences = InInputSettings.bImportAsReferenceMaterialEnabled ?
			InObject->GetMaterialReferences() :
			TArray<FString>();

		bSuccess = FHoudiniInputTranslator::CreateInputNodeForReference(
			CreatedNodeId,
			SM,
			SMCName,
			ImportAsReferenceTransform,
			InInputSettings.bImportAsReferenceRotScaleEnabled,
			InputNodeHandle,
			true,
			InInputSettings.bImportAsReferenceBboxEnabled,
			InBbox,
			InInputSettings.bImportAsReferenceMaterialEnabled,
			MaterialReferences);
	}
	else 
	{
		bSuccess = FUnrealMeshTranslator::HapiCreateInputNodeForStaticMesh(
			SM, 
			CreatedNodeId, 
			SMCName,
			InputNodeHandle, 
			SMC, 
			InInputSettings.bExportLODs, 
			InInputSettings.bExportSockets, 
			InInputSettings.bExportColliders,
			true, 
			bInputNodesCanBeDeleted, 
			InInputSettings.bPreferNaniteFallbackMesh,
			InInputSettings.bExportMaterialParameters,
			bComponentGeneratesData);
	}

	// Create/update the node in the input manager if the static mesh component uses an asset directly.
	{
		if (!bComponentGeneratesData)
		{
			FUnrealObjectInputOptions Options = InputNodeHandle.GetIdentifier().GetOptions();
			constexpr bool bIsLeaf = false;
			FUnrealObjectInputIdentifier SMCIdentifier(SMC, Options, bIsLeaf);
			FUnrealObjectInputHandle Handle;
			if (!FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(SMCIdentifier, {InputNodeHandle}, Handle, true, bInputNodesCanBeDeleted))
				return false;
			
			FUnrealObjectInputUtils::GetHAPINodeId(Handle, CreatedNodeId);
			InObject->InputNodeHandle = Handle;
		}
		else
		{
			InObject->InputNodeHandle = InputNodeHandle;
		}

		// Create the output modifier chain if missing
		const FName ModifierChainName(FUnrealObjectInputNode::OutputChainName);
		if (!FUnrealObjectInputUtils::DoesModifierChainExist(InObject->InputNodeHandle, ModifierChainName))
			FUnrealObjectInputUtils::AddModifierChain(InObject->InputNodeHandle, ModifierChainName, CreatedNodeId);
		else
			FUnrealObjectInputUtils::SetModifierChainNodeToConnectTo(InObject->InputNodeHandle, ModifierChainName, CreatedNodeId);

		// Make sure that material overrides modifier exists and is correctly configured for this component's input node
		if (FUnrealObjectInputModifier* MatOverridesModifier = FUnrealObjectInputUtils::FindFirstModifierOfType(InObject->InputNodeHandle, ModifierChainName, EUnrealObjectInputModifierType::MaterialOverrides))
		{
			FUnrealObjectInputMaterialOverrides* const MatOverrides = static_cast<FUnrealObjectInputMaterialOverrides*>(MatOverridesModifier);
			if (InInputSettings.bImportAsReference)
			{
				if (InInputSettings.bImportAsReferenceMaterialEnabled)
				{
					if (MatOverrides)
						MatOverrides->SetUsePrimWrangle(false);
				}
				else
				{
					FUnrealObjectInputUtils::DestroyModifier(InObject->InputNodeHandle, ModifierChainName, MatOverridesModifier);
				}
			}
			else
			{
				if (MatOverrides)
					MatOverrides->SetUsePrimWrangle(true);
			}
		}
		else
		{
			if (InInputSettings.bImportAsReference)
			{
				if (InInputSettings.bImportAsReferenceMaterialEnabled)
					FUnrealObjectInputUtils::CreateAndAddModifier<FUnrealObjectInputMaterialOverrides>(InObject->InputNodeHandle, ModifierChainName, SMC, false);
			}
			else
			{
				FUnrealObjectInputUtils::CreateAndAddModifier<FUnrealObjectInputMaterialOverrides>(InObject->InputNodeHandle, ModifierChainName, SMC, true);
			}
		}

		// Ensure that the physical material override modifier exists for this component's input node and is correctly configured
		const HAPI_AttributeOwner PhysMatOverrideAttrOwner = InInputSettings.bImportAsReference ? HAPI_ATTROWNER_POINT : HAPI_ATTROWNER_PRIM;
		if (FUnrealObjectInputModifier* PhysMatOverrideModifier = FUnrealObjectInputUtils::FindFirstModifierOfType(InObject->InputNodeHandle, ModifierChainName, EUnrealObjectInputModifierType::PhysicalMaterialOverride))
		{
			FUnrealObjectInputPhysicalMaterialOverride* const PhysMatOverride = static_cast<FUnrealObjectInputPhysicalMaterialOverride*>(PhysMatOverrideModifier);
			if (PhysMatOverride)
				PhysMatOverride->SetAttributeOwner(PhysMatOverrideAttrOwner);
		}
		else
		{
			FUnrealObjectInputUtils::CreateAndAddModifier<FUnrealObjectInputPhysicalMaterialOverride>(InObject->InputNodeHandle, ModifierChainName, SMC, PhysMatOverrideAttrOwner);
		}

		// Data layer Modifier
		if (FUnrealObjectInputModifier* DataLayerModifier = FUnrealObjectInputUtils::FindFirstModifierOfType(InObject->InputNodeHandle, ModifierChainName, EUnrealObjectInputModifierType::DataLayerGroups))
		{
			// nothing for now
		}
		else
		{
			FUnrealObjectInputUtils::CreateAndAddModifier<FUnrealObjectInputDataLayer>(InObject->InputNodeHandle, ModifierChainName, SMC->GetOwner());
		}

		// HLODs
		FUnrealObjectInputModifier* HLODModifier = FUnrealObjectInputUtils::FindFirstModifierOfType(InObject->InputNodeHandle, ModifierChainName, EUnrealObjectInputModifierType::HLODAttributes);
		if (!HLODModifier)
		{
			FUnrealObjectInputUtils::CreateAndAddModifier<FUnrealObjectInputHLODAttributes>(InObject->InputNodeHandle, ModifierChainName, SMC->GetOwner());
		}

		// Update all modifiers
		FUnrealObjectInputUtils::UpdateAllModifierChains(InObject->InputNodeHandle);
	}
	
	// Update this input object's OBJ NodeId
	InObject->SetInputNodeId(CreatedNodeId);
	InObject->SetInputObjectNodeId(FHoudiniEngineUtils::HapiGetParentNodeId(CreatedNodeId));

	// Update this input object's cache data
	InObject->Update(SMC, InInputSettings);

	// Update the component's transform
	FTransform ComponentTransform = InObject->GetHoudiniObjectTransform();
	{
		// convert to HAPI_Transform
		HAPI_TransformEuler HapiTransform;
		FHoudiniApi::TransformEuler_Init(&HapiTransform);
		FHoudiniEngineUtils::TranslateUnrealTransform(ComponentTransform, HapiTransform);

		// Set the transform on the OBJ parent
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
			FHoudiniEngine::Get().GetSession(), InObject->GetInputObjectNodeId(), &HapiTransform), false);
	}

	return bSuccess;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForSplineMeshComponents(
	const FString& InObjNodeName,
	UHoudiniInputActor* InParentActorObject,
	const FHoudiniInputObjectSettings& InInputSettings,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForSplineMeshComponents);

	if (!IsValid(InParentActorObject))
		return false;

	UHoudiniInputSplineMeshComponent* FirstSMCObject = nullptr;
	TArray<UPrimitiveComponent*> MeshComponents;
	TArray<UHoudiniInputSplineMeshComponent*> SMCObjects;
	for (UHoudiniInputSceneComponent* Component : InParentActorObject->GetActorComponents())
	{
		if (!IsValid(Component))
			continue;

		UHoudiniInputSplineMeshComponent* const SMCObject = Cast<UHoudiniInputSplineMeshComponent>(Component);
		if (!IsValid(SMCObject))
			continue;

		SMCObjects.Add(SMCObject);

		// Since we are going to send this SMC as part of a merged mesh for this input we can invalidate the single
		// mesh case here
		SMCObject->InvalidateData();

		if (!FirstSMCObject)
			FirstSMCObject = SMCObject;

		USplineMeshComponent* const SMC = SMCObject->GetSplineMeshComponent();
		if (!IsValid(SMC))
			continue;

		MeshComponents.Add(SMC);
	}

	if (MeshComponents.IsEmpty())
		return true;

	if (!IsValid(FirstSMCObject))
		return true;
	
	USplineMeshComponent* FirstSMC = FirstSMCObject->GetSplineMeshComponent();
	
	// Generate a static mesh from the spline mesh components
	AActor* const ParentActor = InParentActorObject->GetActor();
	
	FHoudiniPackageParams PackageParams;
	PackageParams.PackageMode = EPackageMode::CookToTemp;
	PackageParams.ReplaceMode = EPackageReplaceMode::ReplaceExistingAssets;
	PackageParams.HoudiniAssetActorName = ParentActor->GetActorNameOrLabel();
	PackageParams.HoudiniAssetName = ParentActor->GetClass()->GetName();
	PackageParams.ObjectName = FirstSMC->GetName();
	PackageParams.ComponentGUID = InParentActorObject->GetSplinesMeshPackageGuid();
	FMeshMergingSettings Settings;
	UStaticMesh* SM = nullptr;

	FVector MergedLocation = FVector::ZeroVector;
	if (!FHoudiniMeshUtils::MergeMeshes(MeshComponents, PackageParams, Settings, SM, MergedLocation))
		return true;

	if (!IsValid(SM))
		return true;

	InParentActorObject->SetGeneratedSplineMesh(SM);
	
	HAPI_NodeId CreatedNodeId = InParentActorObject->SplinesMeshNodeId;

	// Marshall the Static Mesh to Houdini
	FString SMCName = InObjNodeName + TEXT("_") + FirstSMC->GetName();
	FHoudiniEngineUtils::SanitizeHAPIVariableName(SMCName);

	FUnrealObjectInputHandle InputNodeHandle;
	bool bSuccess = FUnrealMeshTranslator::HapiCreateInputNodeForStaticMesh(
		SM, 
		CreatedNodeId, 
		SMCName,
		InputNodeHandle, 
		nullptr, 
		InInputSettings.bExportLODs, 
		InInputSettings.bExportSockets, 
		InInputSettings.bExportColliders,
		true, 
		bInputNodesCanBeDeleted, 
		InInputSettings.bPreferNaniteFallbackMesh,
		InInputSettings.bExportMaterialParameters,
		false);

	// Create/update the node in the input manager
	{
		FUnrealObjectInputOptions Options = InputNodeHandle.GetIdentifier().GetOptions();
		constexpr bool bIsLeaf = false;
		FUnrealObjectInputIdentifier SMCIdentifier(FirstSMC, Options, bIsLeaf);
		FUnrealObjectInputHandle Handle;
		if (!FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(SMCIdentifier, {InputNodeHandle}, Handle, true, bInputNodesCanBeDeleted))
			return false;
		
		FUnrealObjectInputUtils::GetHAPINodeId(Handle, CreatedNodeId);
		InParentActorObject->SplinesMeshInputNodeHandle = Handle;
	}
	
	// Update this input object's OBJ NodeId
	InParentActorObject->SplinesMeshNodeId = CreatedNodeId;
	InParentActorObject->SplinesMeshObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(InParentActorObject->SplinesMeshNodeId);

	for (UHoudiniInputSplineMeshComponent* const SMCObject : SMCObjects)
	{
		if (!IsValid(SMCObject))
			continue;
		
		// Update this input object's cache data
		SMCObject->Update(SMCObject->GetSplineMeshComponent(), InInputSettings);
	}

	// Update the component's transform
	FTransform Transform = FTransform::Identity;
	Transform.SetTranslation(MergedLocation);
	// When using the ref counted system we expected this transform to be relative to actor
	{
		Transform = Transform.GetRelativeTransform(InParentActorObject->GetTransform());
	}
	{
		// convert to HAPI_Transform
		HAPI_TransformEuler HapiTransform;
		FHoudiniApi::TransformEuler_Init(&HapiTransform);
		FHoudiniEngineUtils::TranslateUnrealTransform(Transform, HapiTransform);

		// Set the transform on the OBJ parent
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
			FHoudiniEngine::Get().GetSession(), InParentActorObject->SplinesMeshObjectNodeId, &HapiTransform), false);
	}

	return bSuccess;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForInstancedStaticMeshComponent(
	const FString& InObjNodeName,
	UHoudiniInputInstancedMeshComponent* InObject,
	const FHoudiniInputObjectSettings& InInputSettings,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForInstancedStaticMeshComponent);

	if (!IsValid(InObject))
		return false;

	UObject* Object = InObject->GetObject();
	if (!IsValid(Object))
		return true;

	// Get the ISMC
	UInstancedStaticMeshComponent* ISMC = InObject->GetInstancedStaticMeshComponent();
	if (!IsValid(ISMC))
		return true;

	HAPI_NodeId NewNodeId = -1;
	FUnrealObjectInputHandle InputNodeHandle;
	if (!FUnrealInstanceTranslator::HapiCreateInputNodeForInstancer(
		ISMC,
		InObjNodeName,
		NewNodeId,
		InputNodeHandle,
		InInputSettings.bExportLODs,
		InInputSettings.bExportSockets,
		InInputSettings.bExportColliders,
		false,
		InInputSettings.bPreferNaniteFallbackMesh,
		InInputSettings.bExportMaterialParameters,
		bInputNodesCanBeDeleted))
		return false;

	// Update this input object's node IDs
	InObject->SetInputNodeId(NewNodeId);
	InObject->SetInputObjectNodeId(FHoudiniEngineUtils::HapiGetParentNodeId(NewNodeId));
	InObject->InputNodeHandle = InputNodeHandle;

	// Update the component's cached instances
	InObject->Update(ISMC, InInputSettings);

	// Update the component's transform
	const FTransform ComponentTransform = InObject->GetHoudiniObjectTransform();
	{
		// convert to HAPI_Transform
		HAPI_TransformEuler HapiTransform;
		FHoudiniApi::TransformEuler_Init(&HapiTransform);
		FHoudiniEngineUtils::TranslateUnrealTransform(ComponentTransform, HapiTransform);

		// Set the transform on the OBJ parent
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
			FHoudiniEngine::Get().GetSession(), InObject->GetInputObjectNodeId(), &HapiTransform), false);
	}
	
	return true;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForSplineComponent(
	const FString& InObjNodeName,
	UHoudiniInputSplineComponent* InObject,
	const FHoudiniInputObjectSettings& InInputSettings,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForSplineComponent);

	if (!IsValid(InObject))
		return false;

	USplineComponent* Spline = InObject->GetSplineComponent();
	if (!IsValid(Spline))
		return true;

	int32 NumberOfSplineControlPoints = InObject->NumberOfSplineControlPoints;
	TArray<FTransform> SplineControlPoints = InObject->SplineControlPoints;

	FString SplineName = InObjNodeName + TEXT("_") + InObject->GetName();
	FHoudiniEngineUtils::SanitizeHAPIVariableName(SplineName);

	FUnrealObjectInputHandle InputNodeHandle;
	HAPI_NodeId CreatedNodeId = InObject->GetInputNodeId();

	if (!FUnrealSplineTranslator::CreateInputNodeForSplineComponent(
		Spline, CreatedNodeId, InputNodeHandle, InInputSettings.UnrealSplineResolution, SplineName, InInputSettings.bUseLegacyInputCurves, bInputNodesCanBeDeleted))
		return false;

	// Cache the exported curve's data to the input object
	InObject->InputNodeHandle = InputNodeHandle;
	InObject->SetInputNodeId((int32)CreatedNodeId);
	InObject->SetInputObjectNodeId((int32)FHoudiniEngineUtils::HapiGetParentNodeId(CreatedNodeId));

	InObject->MarkChanged(true);

	// Update the component's cached data
	InObject->Update(Spline, InInputSettings);

	if (!HapiSetGeoObjectTransform(InObject->GetInputObjectNodeId(), InObject->GetHoudiniObjectTransform()))
		return false;

	return true;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForHoudiniSplineComponent(
	const FString& InObjNodeName,
	UHoudiniInputHoudiniSplineComponent* InObject,
	const FHoudiniInputObjectSettings& InInputSettings)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForHoudiniSplineComponent);

	if (!IsValid(InObject))
		return false;

	UHoudiniSplineComponent* Curve = InObject->GetCurveComponent();
	if (!IsValid(Curve))
		return true;

	Curve->SetIsLegacyInputCurve(InInputSettings.bUseLegacyInputCurves);

	if (!FHoudiniSplineTranslator::HapiUpdateNodeForHoudiniSplineComponent(Curve, InInputSettings.bAddRotAndScaleAttributesOnCurves))
		return false;

	// See if the component needs it node Id invalidated
	//if (InObject->InputNodeId < 0)
	//	Curve->SetNodeId(InObject->InputNodeId);

	// Cache the exported curve's data to the input object
	InObject->SetInputNodeId(Curve->GetNodeId());
	InObject->SetInputObjectNodeId(FHoudiniEngineUtils::HapiGetParentNodeId(InObject->GetInputNodeId()));

	//InObject->CurveType = Curve->GetCurveType();
	//InObject->CurveMethod = Curve->GetCurveMethod();
	//InObject->Reversed = Curve->IsReversed();
	InObject->Update(Curve, InInputSettings);

	InObject->MarkChanged(true);

	return true;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForHoudiniAssetComponent(
		const FString& InObjNodeName,
		UHoudiniInputHoudiniAsset* InObject,
		const FHoudiniInputObjectSettings& InInputSettings)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForHoudiniAssetComponent);

	if (!IsValid(InObject))
		return false;

	UHoudiniAssetComponent* InputHAC = InObject->GetHoudiniAssetComponent();
	if (!IsValid(InputHAC))
		return true;

	if (!InputHAC->CanDeleteHoudiniNodes())
		return true;

	UHoudiniInput* HoudiniInput = Cast<UHoudiniInput>(InObject->GetOuter());
	if (!IsValid(HoudiniInput))
		return true;

	UHoudiniAssetComponent* OuterHAC = Cast<UHoudiniAssetComponent>(HoudiniInput->GetOuter());
	if (!IsValid(OuterHAC))
		return true;

	// Do not allow using ourself as an input, terrible things would happen
	if (InputHAC->GetAssetId() == OuterHAC->GetAssetId())
		return false;

	// If previously imported as ref, delete the input node.
	if (InObject->GetInputNodeId() > -1 && InObject->GetImportAsReference())
	{
		int32 PreviousInputNodeId = InObject->GetInputNodeId();
		// Get the parent OBJ node ID before deleting!
		HAPI_NodeId PreviousInputOBJNode = FHoudiniEngineUtils::HapiGetParentNodeId(PreviousInputNodeId);

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
			FHoudiniEngine::Get().GetSession(), PreviousInputNodeId))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input node for %s."), *InObject->GetName());
		}

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
			FHoudiniEngine::Get().GetSession(), PreviousInputOBJNode))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input OBJ node for %s."), *InObject->GetName());
		}
	}

	// If this object is in an Asset input, we need to set the InputNodeId directly
	// to avoid creating extra merge nodes. World inputs should not do that!

	// TODO: CHECK ME!
	//bool bIsAssetInput = HoudiniInput->GetInputType() == EHoudiniInputType::Asset_DEPRECATED;
	bool bIsAssetInput = HoudiniInput->IsAssetInput();

	if (InInputSettings.bImportAsReference) 
	{
		InObject->SetInputNodeId(-1);
		InObject->SetInputObjectNodeId(-1);

		if(bIsAssetInput)
			HoudiniInput->SetInputNodeId(-1);

		FString HAName = InObject->GetName();
		FHoudiniEngineUtils::SanitizeHAPIVariableName(HAName);

		HAPI_NodeId InputNodeId = InObject->GetInputNodeId();
		constexpr bool bUseRefCountedInputSystem = false;
		FUnrealObjectInputHandle InputNodeHandle;
		if (!FHoudiniInputTranslator::CreateInputNodeForReference(
				InputNodeId,
				InputHAC,
				HAName,
				InObject->GetTransform(),
				InInputSettings.bImportAsReferenceRotScaleEnabled,
				InputNodeHandle,
				InObject->CanDeleteHoudiniNodes())) // do not delete previous node if it was HAC
			return false;

		InObject->SetInputNodeId(InputNodeId);

		if (bIsAssetInput)
			HoudiniInput->SetInputNodeId(InObject->GetInputNodeId());
	}

	InputHAC->AddDownstreamHoudiniAsset(OuterHAC);

	//if (HAC->NeedsInitialization())
	//	HAC->MarkAsNeedInstantiation();

	//HoudiniInput->SetAssetNodeId(HAC->GetAssetId());

	// TODO: This might be uneeded as this function should only be called
	// after we're not  wiating on the input asset...
	if (InputHAC->GetAssetState() == EHoudiniAssetState::NeedInstantiation)
	{
		// If the input HAC needs to be instantiated, tell it do so
		InputHAC->SetAssetState(EHoudiniAssetState::PreInstantiation);
		// Mark this object's input as changed so we can properly update after the input HDA's done instantiating/cooking
		HoudiniInput->MarkChanged(true);
	}

	if (InputHAC->NeedsInitialization() || InputHAC->NeedUpdate())
		return false;

	if (!InInputSettings.bImportAsReference)
	{
		if (bIsAssetInput)
			HoudiniInput->SetInputNodeId(InputHAC->GetAssetId());

		InObject->SetInputNodeId(InputHAC->GetAssetId());
	}

	InObject->SetInputObjectNodeId(InObject->GetInputNodeId());
	
	bool bReturn = InObject->GetInputNodeId() > -1;

	if(bIsAssetInput)
		bReturn = FHoudiniInputTranslator::ConnectInputNode(HoudiniInput);

	// Update the cached data and input settings
	InObject->Update(InputHAC, InInputSettings);

	return bReturn;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodesForActorComponents(
	UHoudiniInput* const InInput,
	UHoudiniInputActor* const InInputActorObject,
	AActor* const InActor,
	const FTransform& InActorTransform, 
	TArray<int32>& OutCreatedNodeIds,
	TSet<FUnrealObjectInputHandle>& OutHandles,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodesForActorComponents);

	if (!IsValid(InInput))
		return false;

	if (!IsValid(InInputActorObject))
		return false;

	if (!IsValid(InActor))
		return true;

	const FHoudiniInputObjectSettings InputSettings(InInput);
	
	const bool bMergeSplineMeshes = InputSettings.bMergeSplineMeshComponents && InInputActorObject->GetNumSplineMeshComponents() > 1;
	// If we are not sending a merged mesh, invalidate any previous merge mesh data so that it can be cleaned up
	if (!bMergeSplineMeshes)
	{
		InInputActorObject->InvalidateSplinesMeshData();
	}

	// Now, commit all of this actor's component
	bool bHasSplineMeshComponentsToMerge = false;
	int32 ComponentIdx = 0;
	for (UHoudiniInputSceneComponent* CurComponent : InInputActorObject->GetActorComponents())
	{
		const bool bIsSplineMeshComponent = CurComponent->IsA<UHoudiniInputSplineMeshComponent>();
		if (bMergeSplineMeshes && bIsSplineMeshComponent)
		{
			CurComponent->InvalidateData();
			bHasSplineMeshComponentsToMerge = true;
			continue;
		}
		
		if (UploadHoudiniInputObject(InInput, CurComponent, InActorTransform, OutCreatedNodeIds, OutHandles, bInputNodesCanBeDeleted))
			ComponentIdx++;

		// If we're importing the actor as ref, add the level path / actor path attribute to the created nodes
		if (InInput->GetImportAsReference())
		{
			// When using the ref counted input system: the nodes are created differently so we cannot just add attributes
			// to the input node (it is likely a merge and not an input null). For the new system we add a modifier to
			// the output modifier chain.
			if (!CurComponent->InputNodeHandle.IsValid())
			{
				bool bNeedCommit = false;
				if (FHoudiniEngineUtils::AddLevelPathAttribute(CurComponent->GetInputNodeId(), 0, InActor->GetLevel(), 1, HAPI_ATTROWNER_POINT))
					bNeedCommit = true;

				if(FHoudiniEngineUtils::AddActorPathAttribute(CurComponent->GetInputNodeId(), 0, InActor, 1, HAPI_ATTROWNER_POINT))
					bNeedCommit = true;

				// Commit the geo if needed
				if(bNeedCommit)
					FHoudiniEngineUtils::HapiCommitGeo(CurComponent->GetInputNodeId());
			}
			else
			{
				const FName ChainName(FUnrealObjectInputNode::OutputChainName);
				if (!FUnrealObjectInputUtils::DoesModifierChainExist(CurComponent->InputNodeHandle, ChainName))
					FUnrealObjectInputUtils::AddModifierChain(CurComponent->InputNodeHandle, ChainName, CurComponent->GetInputNodeId());
				if (!FUnrealObjectInputUtils::FindFirstModifierOfType(CurComponent->InputNodeHandle, ChainName, EUnrealObjectInputModifierType::ActorAsReference))
					FUnrealObjectInputUtils::CreateAndAddModifier<FUnrealObjectInputActorAsReference>(CurComponent->InputNodeHandle, ChainName, InActor);

				FUnrealObjectInputUtils::UpdateModifiers(CurComponent->InputNodeHandle, ChainName);
			}
		}
	}

	if (bHasSplineMeshComponentsToMerge)
	{
		InInputActorObject->SetUsedMergeSplinesMeshAtLastTranslate(true);
		if (HapiCreateInputNodeForSplineMeshComponents(
				InInput->GetNodeBaseName(),
				InInputActorObject,
				InputSettings,
				bInputNodesCanBeDeleted))
		{
			OutCreatedNodeIds.Add(InInputActorObject->SplinesMeshObjectNodeId);
			OutHandles.Add(InInputActorObject->SplinesMeshInputNodeHandle);
		}
	}
	else
	{
		InInputActorObject->SetUsedMergeSplinesMeshAtLastTranslate(false);
	}

	return true;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForActor(
	UHoudiniInput* InInput, 
	UHoudiniInputActor* InObject,
	const FTransform & InActorTransform, 
	TArray<int32>& OutCreatedNodeIds,
	TSet<FUnrealObjectInputHandle>& OutHandles,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForActor);

	if (!IsValid(InInput))
		return false;

	if (!IsValid(InObject))
		return false;

	AActor* Actor = InObject->GetActor();
	if (!IsValid(Actor))
		return true;

	const FHoudiniInputObjectSettings InputSettings(InInput);

	// Check if this is a world input and if this is a HoudiniAssetActor
	// If so we need to build static meshes for any proxy meshes
	if ((InInput->GetInputType() == EHoudiniInputType::World)
		&& Actor->IsA<AHoudiniAssetActor>())
	{
		AHoudiniAssetActor *HAA = Cast<AHoudiniAssetActor>(Actor);
		UHoudiniAssetComponent *HAC = HAA->GetHoudiniAssetComponent();
		if (IsValid(HAC))
		{
			if (HAC->HasAnyCurrentProxyOutput())
			{
				bool bPendingDeleteOrRebuild = false;
				bool bInvalidState = false;
				const bool bIsHoudiniCookedDataAvailable = HAC->IsHoudiniCookedDataAvailable(bPendingDeleteOrRebuild, bInvalidState);
				if (bIsHoudiniCookedDataAvailable)
				{
					// Build the static mesh
					FHoudiniOutputTranslator::BuildStaticMeshesOnHoudiniProxyMeshOutputs(HAC);
					// Update the input object since a new StaticMeshComponent could have been created
					UObject *InputObject = InObject->GetObject();
					if (IsValid(InputObject))
					{
						InObject->Update(InputObject, InputSettings);
						TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
					}
				}
				else if (!bPendingDeleteOrRebuild && !bInvalidState)
				{
					// Request a cook with no proxy output
					HAC->MarkAsNeedCook();
					HAC->SetNoProxyMeshNextCookRequested(true);
				}
			}
			else if (InObject->GetActorComponents().Num() == 0 && HAC->HasAnyOutputComponent())
			{
				// The HAC has non-proxy output components, but the InObject does not have any
				// actor components. This can arise after a cook if previously there were only
				// proxies and the input was created when there were only proxies
				// Try to update the input to find new components
				UObject *InputObject = InObject->GetObject();
				if (IsValid(InputObject))
				{
					InObject->Update(InputObject, InputSettings);
					TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
				}
			}
		}
	}

	// Now, commit all of this actor's component
	TArray<int32> CreatedNodeIds;
	TSet<FUnrealObjectInputHandle> Handles;
	bool bSuccess = false;
	if (InputSettings.bImportAsReference)
		bSuccess = HapiCreateInputNodeForActorReference(InObject, InputSettings, bInputNodesCanBeDeleted);
	else
		bSuccess = HapiCreateInputNodesForActorComponents(InInput, InObject, Actor, InActorTransform, CreatedNodeIds, Handles, bInputNodesCanBeDeleted);

	// Cache our transformn
	InObject->SetTransform(Actor->GetTransform());

	InObject->Update(Actor, InputSettings);

	if (!InputSettings.bImportAsReference)
	{
		// Make a reference node for the actor
		const FUnrealObjectInputOptions Options = FUnrealObjectInputOptions::MakeOptionsForGenericActor(InputSettings);
		const FUnrealObjectInputIdentifier ActorInputNodeId(Actor, Options, false);
		FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(ActorInputNodeId, Handles, InObject->InputNodeHandle, true, bInputNodesCanBeDeleted);
		if (!HapiSetGeoObjectTransform(InObject->GetInputObjectNodeId(), InObject->GetHoudiniObjectTransform()))
			return false;

		OutCreatedNodeIds.Add(InObject->GetInputObjectNodeId());
		OutHandles.Add(InObject->InputNodeHandle);
	}
	else
	{
		// We only created one node for the actor as ref
		OutCreatedNodeIds.Add(InObject->GetInputObjectNodeId());
		OutHandles.Add(InObject->InputNodeHandle);
	}

	//TODO? Check
	// return true if we have at least uploaded one component
	// return (ComponentIdx > 0);

	return bSuccess;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForBP(
	UHoudiniInput* InInput,
	UHoudiniInputBlueprint* InObject,
	TArray<int32>& OutCreatedNodeIds,
	TSet<FUnrealObjectInputHandle>& OutHandles,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForBP);

	if (!IsValid(InInput))
		return false;

	if (!IsValid(InObject))
		return false;

	UBlueprint* BP = InObject->GetBlueprint();
	if (!IsValid(BP))
		return true;

	// If importing as reference, we want to send the whole BP, not its components
	if (InInput->GetImportAsReference())
	{
		FString BPName = InInput->GetNodeBaseName() + TEXT("_") + BP->GetName();
		FHoudiniEngineUtils::SanitizeHAPIVariableName(BPName);

		const FHoudiniInputObjectSettings InputSettings(InInput);
		if (!FHoudiniInputTranslator::HapiCreateInputNodeForReference(
			BPName,
			InObject,
			InputSettings,
			bInputNodesCanBeDeleted))
		{
			return false;
		}

		OutCreatedNodeIds.Add(InObject->GetInputObjectNodeId());
		OutHandles.Add(InObject->InputNodeHandle);
	}
	else
	{
		// Now, commit all of this BP's component
		TSet<FUnrealObjectInputHandle> ComponentHandles;
		TArray<int32> CreatedNodeIds;
		TSet<FUnrealObjectInputHandle> Handles;
		for (UHoudiniInputSceneComponent* CurComponent : InObject->GetComponents())
		{
			if (UploadHoudiniInputObject(InInput, CurComponent, FTransform::Identity, CreatedNodeIds, Handles, bInputNodesCanBeDeleted))
			{
				ComponentHandles.Add(CurComponent->InputNodeHandle);
			}
		}

		{
			// The BP can have its own transform (geometry input), so we have to create a reference node that
			// represents InObject in the new input system that references the StaticMesh asset's input node handle
			FUnrealObjectInputOptions Options;
			static constexpr bool bIsLeaf = false;
			FUnrealObjectInputIdentifier GeoInputRefNodeId(InObject, Options, bIsLeaf);
			FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(GeoInputRefNodeId, ComponentHandles, InObject->InputNodeHandle, true, bInputNodesCanBeDeleted);

			// Set the transform on the InputObject's geo object node
			if (!HapiSetGeoObjectTransform(InObject->GetInputObjectNodeId(), InObject->GetHoudiniObjectTransform()))
				return false;

			OutCreatedNodeIds.Add(InObject->GetInputObjectNodeId());
			OutHandles.Add(InObject->InputNodeHandle);
		}
	}

	const FHoudiniInputObjectSettings InputSettings(InInput);
	InObject->Update(BP, InputSettings);

	return true;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForLandscapeSplinesComponent(
	const FString& InObjNodeName,
	UHoudiniInputLandscapeSplinesComponent* const InObject,
	const FHoudiniInputObjectSettings& InInputSettings,
	TArray<int32>& OutCreatedNodeIds,
	TSet<FUnrealObjectInputHandle>& OutHandles,
	const bool bInInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForLandscapeSplinesComponent);

	if (!IsValid(InObject))
		return false;

	ULandscapeSplinesComponent* const SplinesComponent = InObject->GetLandscapeSplinesComponent();

	if (!IsValid(SplinesComponent))
		return true;

	HAPI_NodeId CreatedNodeId = InObject->GetInputNodeId();

	FString SplinesComponentName = InObjNodeName + TEXT("_") + SplinesComponent->GetName();
	FHoudiniEngineUtils::SanitizeHAPIVariableName(SplinesComponentName);

	TMap<TSoftObjectPtr<ULandscapeSplineControlPoint>, int32> ControlPointIdMap(InObject->GetControlPointIdMap());
	int32 NextControlPointId = InObject->GetNextControlPointId();
	
	static constexpr bool bForceReferenceInputNodeCreation = true;
	static constexpr bool bLandscapeSplinesExportCurves = true;
	FUnrealObjectInputHandle CreatedSplinesNodeHandle;
	const bool bSuccess = FUnrealLandscapeSplineTranslator::CreateInputNode(
		SplinesComponent,
		bForceReferenceInputNodeCreation,
		CreatedNodeId,
		CreatedSplinesNodeHandle,
		SplinesComponentName,
		ControlPointIdMap,
		NextControlPointId,
		InInputSettings.UnrealSplineResolution,
		bLandscapeSplinesExportCurves,
		InInputSettings.bLandscapeSplinesExportControlPoints,
		InInputSettings.bLandscapeSplinesExportLeftRightCurves,
		bInInputNodesCanBeDeleted);

	// Update this input object's OBJ NodeId
	InObject->SetInputNodeId(CreatedNodeId);
	InObject->SetInputObjectNodeId(CreatedNodeId >= 0 ? FHoudiniEngineUtils::HapiGetParentNodeId(CreatedNodeId) : -1);
	InObject->InputNodeHandle = CreatedSplinesNodeHandle;

	InObject->SetControlPointIdMap(MoveTemp(ControlPointIdMap));
	InObject->SetNextControlPointId(NextControlPointId);

	// Even if the function failed, some nodes may have been created, so check the node ID
	if (InObject->GetInputObjectNodeId() >= 0)
	{
		OutCreatedNodeIds.Emplace(InObject->GetInputObjectNodeId());
	}
	OutHandles.Add(InObject->InputNodeHandle);
	
	// Update this input object's cache data
	InObject->Update(SplinesComponent, InInputSettings);

	// Update the component's transform
	const FTransform ComponentTransform = InObject->GetHoudiniObjectTransform();
	{
		// convert to HAPI_Transform
		HAPI_TransformEuler HapiTransform;
		FHoudiniApi::TransformEuler_Init(&HapiTransform);
		FHoudiniEngineUtils::TranslateUnrealTransform(ComponentTransform, HapiTransform);

		// Set the transform on the OBJ parent
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
			FHoudiniEngine::Get().GetSession(), InObject->GetInputObjectNodeId(), &HapiTransform), false);
	}

	return bSuccess;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForLevelInstance(
	const FString& InObjNodeName,
	UHoudiniInputLevelInstance* InObject,
	const FHoudiniInputObjectSettings& InInputSettings,
	UHoudiniInput* InInput,
	TArray<int32>& OutCreatedNodeIds,
	TSet<FUnrealObjectInputHandle>& OutHandles,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForLevelInstance);

	if (!IsValid(InObject) || !IsValid(InInput))
		return false;

	ALevelInstance * LevelInstance = InObject->GetLevelInstance();
	if (!IsValid(LevelInstance))
		return true;

	FString LevelInstanceName = InObjNodeName + TEXT("_") + LevelInstance->GetActorLabel();
	FUnrealObjectInputHandle InputNodeHandle;
	HAPI_NodeId InputNodeId = InObject->GetInputNodeId();

	if (InInputSettings.bExportLevelInstanceContent)
	{
		{
			const FUnrealObjectInputOptions LevelInstanceNodeOptions = FUnrealObjectInputOptions::MakeOptionsForLevelInstanceActor(InInputSettings);
			const FUnrealObjectInputIdentifier LevelInstanceId(LevelInstance->GetWorldAsset().LoadSynchronous(), LevelInstanceNodeOptions, false);
			if (!FUnrealObjectInputUtils::NodeExistsAndIsNotDirty(LevelInstanceId, InputNodeHandle))
			{
				// Process each actor in the level instance
				TArray<int32> NodeIds;
				TSet<FUnrealObjectInputHandle> Handles;
				for (auto& Entry : InObject->GetTrackedActorObjects())
				{
					UHoudiniInputObject* const InputObject = Entry.Value;
					if (!IsValid(InputObject))
						continue;

					UploadHoudiniInputObject(InInput, InputObject, FTransform::Identity, NodeIds, Handles, bInputNodesCanBeDeleted);
				}

				// Create/Update the level instance' merge / reference node
				FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(LevelInstanceId, Handles, InputNodeHandle, true, bInputNodesCanBeDeleted);
			}

			// Make a reference node for the actor
			const FUnrealObjectInputIdentifier ActorInputNodeId(LevelInstance, LevelInstanceNodeOptions, false);
			// Create/update the input-specific merge node for this level instance, on which we can apply the actor transform
			FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(ActorInputNodeId, { InputNodeHandle }, InObject->InputNodeHandle, true, bInputNodesCanBeDeleted);

			OutCreatedNodeIds.Add(InObject->GetInputObjectNodeId());
			OutHandles.Add(InObject->InputNodeHandle);
		}
	}
	else
	{
		if (!FUnrealLevelInstanceTranslator::AddLevelInstance(
			LevelInstance, InInput, InputNodeId, LevelInstanceName, InputNodeHandle, bInputNodesCanBeDeleted))
				return false;
		InObject->InputNodeHandle = InputNodeHandle;

		OutCreatedNodeIds.Add(InObject->GetInputObjectNodeId());
		OutHandles.Add(InObject->InputNodeHandle);
	}

	if (!HapiSetGeoObjectTransform(InObject->GetInputObjectNodeId(), InObject->GetHoudiniObjectTransform()))
		return false;

	InObject->Update(LevelInstance, InInputSettings);

	return true;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForPackedLevelActor(
	const FString& InObjNodeName,
	UHoudiniInputPackedLevelActor* InObject,
	const FHoudiniInputObjectSettings& InInputSettings,
	UHoudiniInput* InInput,
	TArray<int32>& OutCreatedNodeIds,
	TSet<FUnrealObjectInputHandle>& OutHandles,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForPackedLevelActor);

	if (!IsValid(InObject) || !IsValid(InInput))
		return false;

	APackedLevelActor* PackedLevelActor = InObject->GetPackedLevelActor();
	if (!IsValid(PackedLevelActor))
		return true;

	FString LevelInstanceName = InObjNodeName + TEXT("_") + PackedLevelActor->GetActorLabel();
	FUnrealObjectInputHandle InputNodeHandle;
	HAPI_NodeId InputNodeId = InObject->GetInputNodeId();

	if (InInputSettings.bExportLevelInstanceContent)
	{
		{
			// Process the underlying BP of the packed level actor
			UHoudiniInputBlueprint* const InputBP = InObject->GetBlueprintInputObject();
			if (!IsValid(InputBP))
				return false;

			TArray<int32> NodeIds;
			TSet<FUnrealObjectInputHandle> Handles;
			// Now, commit all of this BP's component
			TSet<FUnrealObjectInputHandle> ComponentHandles;
			for (UHoudiniInputSceneComponent* CurComponent : InputBP->GetComponents())
			{
				if (UploadHoudiniInputObject(InInput, CurComponent, FTransform::Identity, NodeIds, Handles, bInputNodesCanBeDeleted))
				{
					ComponentHandles.Add(CurComponent->InputNodeHandle);
				}
			}

			// Make a reference node for the BP asset
			const FUnrealObjectInputOptions Options = FUnrealObjectInputOptions::MakeOptionsForPackedLevelActor(InInputSettings);
			const FUnrealObjectInputIdentifier BPAssetNodeId(InputBP->GetBlueprint(), Options, false);
			FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(BPAssetNodeId, Handles, InputBP->InputNodeHandle, true, bInputNodesCanBeDeleted);
		
			// Make a reference node for the actor
			const FUnrealObjectInputIdentifier ActorInputNodeId(PackedLevelActor, Options, false);
			FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(ActorInputNodeId, { InputBP->InputNodeHandle }, InObject->InputNodeHandle, true, bInputNodesCanBeDeleted);
			
			if (!HapiSetGeoObjectTransform(InObject->GetInputObjectNodeId(), InObject->GetHoudiniObjectTransform()))
				return false;

			OutCreatedNodeIds.Add(InObject->GetInputObjectNodeId());
			OutHandles.Add(InObject->InputNodeHandle);
		}
	}
	else
	{
		if (!FUnrealLevelInstanceTranslator::AddLevelInstance(
			PackedLevelActor, InInput, InputNodeId, LevelInstanceName, InputNodeHandle, bInputNodesCanBeDeleted))
				return false;
		InObject->InputNodeHandle = InputNodeHandle;
		
		if (!HapiSetGeoObjectTransform(InObject->GetInputObjectNodeId(), InObject->GetHoudiniObjectTransform()))
			return false;
		
		OutCreatedNodeIds.Add(InObject->GetInputObjectNodeId());
		OutHandles.Add(InObject->InputNodeHandle);
	}

	InObject->Update(PackedLevelActor, InInputSettings);

	return true;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForLandscape(
	const FString& InObjNodeName,
	UHoudiniInputLandscape* InObject,
	UHoudiniInput* InInput,
	TArray<int32>& OutCreatedNodeIds,
	TSet<FUnrealObjectInputHandle>& OutHandles,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForLandscape);

	if (!IsValid(InObject))
		return false;

	if (!IsValid(InInput))
		return false;

	ALandscapeProxy* Landscape = InObject->GetLandscapeProxy();
	if (!IsValid(Landscape))
		return true;

	FString LandscapeName = InObjNodeName + TEXT("_") + Landscape->GetActorLabel();
	FUnrealObjectInputHandle InputNodeHandle;
	HAPI_NodeId InputNodeId = InObject->GetInputNodeId();

	if (!FUnrealLandscapeTranslator::CreateInputNodeForLandscapeObject(
			Landscape, InInput, InputNodeId, LandscapeName, InputNodeHandle, bInputNodesCanBeDeleted))
		return false;
	
	FTransform Transform = InObject->GetHoudiniObjectTransform();
	// Now, commit all of the input components of the landscape
	TArray<int32> CreatedNodeIds;
	TSet<FUnrealObjectInputHandle> Handles;
	bool bSuccess = HapiCreateInputNodesForActorComponents(InInput, InObject, Landscape, Transform, CreatedNodeIds, Handles, bInputNodesCanBeDeleted);

	const FHoudiniInputObjectSettings InputSettings(InInput);
	InObject->Update(Landscape, InputSettings);

	{
		TSet<ULandscapeComponent*> SelectedLandscapeComponents = InInput->GetLandscapeSelectedComponents();
		FUnrealObjectInputOptions Options = FUnrealObjectInputOptions::MakeOptionsForLandscapeActor(InputSettings, &SelectedLandscapeComponents);
		FUnrealObjectInputIdentifier LandscapeInputNodeId(Landscape, Options, false);

		Handles.Add(InputNodeHandle);
		FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(LandscapeInputNodeId, Handles, InObject->InputNodeHandle, true, bInputNodesCanBeDeleted);
		if (!HapiSetGeoObjectTransform(InObject->GetInputObjectNodeId(), Transform))
			return false;

		OutCreatedNodeIds.Add(InObject->GetInputObjectNodeId());
		OutHandles.Add(InObject->InputNodeHandle);
	}

	return bSuccess;
}


bool
FHoudiniInputTranslator::HapiCreateInputNodeForBrush(
	const FString& InObjNodeName,
	UHoudiniInputBrush* InObject,
	TArray<AActor*>* ExcludeActors,
	const FHoudiniInputObjectSettings& InInputSettings,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForBrush);

	if (!IsValid(InObject))
		return false;

	ABrush* BrushActor = InObject->GetBrush();
	if (!IsValid(BrushActor))
		return true;

	FString BrushName = InObjNodeName + TEXT("_") + BrushActor->GetName();
	FHoudiniEngineUtils::SanitizeHAPIVariableName(BrushName);

	FUnrealObjectInputHandle InputNodeHandle;
	
	HAPI_NodeId InputNodeId = InObject->GetInputNodeId();

	if (!FUnrealBrushTranslator::CreateInputNodeForBrush(InObject, BrushActor, ExcludeActors, InputNodeId, BrushName, InInputSettings.bExportMaterialParameters, InputNodeHandle, bInputNodesCanBeDeleted))
		return false;

	InObject->InputNodeHandle = InputNodeHandle;
	InObject->SetInputNodeId((int32)InputNodeId);
	InObject->SetInputObjectNodeId((int32)FHoudiniEngineUtils::HapiGetParentNodeId(InputNodeId));

	// InObject->MarkChanged(true);
	InObject->Update(BrushActor, InInputSettings);

	if (!HapiSetGeoObjectTransform(InObject->GetInputObjectNodeId(), InObject->GetHoudiniObjectTransform()))
		return false;

	return true;
}


bool
FHoudiniInputTranslator::HapiCreateInputNodeForCamera(
	const FString& InNodeName, UHoudiniInputCameraComponent* InInputObject, const FHoudiniInputObjectSettings& InInputSettings)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForCamera);

	if (!IsValid(InInputObject))
		return false;

	UCameraComponent* Camera = InInputObject->GetCameraComponent();
	if (!IsValid(Camera))
		return true;

	FString NodeName = InNodeName + TEXT("_") + Camera->GetName();
	FHoudiniEngineUtils::SanitizeHAPIVariableName(NodeName);

	// Create the camera OBJ.
	int32 CameraNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(
		-1, TEXT("Object/cam"), InNodeName, true, &CameraNodeId), false);

	// set "Pixel Aspect Ratio" (aspect)
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), CameraNodeId, "aspect", 0, InInputObject->AspectRatio), false);

	// set "Projection" (projection) (0 persp, 1 ortho)
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(
		FHoudiniEngine::Get().GetSession(), CameraNodeId, "projection", 0, InInputObject->bIsOrthographic ? 1 : 0), false);

	// set Ortho Width (orthowidth)
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), CameraNodeId, "orthowidth", 0, InInputObject->OrthoWidth), false);

	// set Near Clippin (near)
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), CameraNodeId, "near", 0, InInputObject->OrthoNearClipPlane), false);

	// set far clipping (far)
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmFloatValue(
		FHoudiniEngine::Get().GetSession(), CameraNodeId, "far", 0, InInputObject->OrthoFarClipPlane), false);

	// Set the transform - rotate by 90 degrees to align with where Houdini expects camera to be pointing.
	FTransform RotationTransform(FQuat::MakeFromEuler(FVector(0.0, 0.0, 90.0)));
	FTransform RotatedCamera = RotationTransform * Camera->GetComponentTransform();
	HAPI_TransformEuler H_Transform;
	FHoudiniApi::TransformEuler_Init(&H_Transform);
	FHoudiniEngineUtils::TranslateUnrealTransform(RotatedCamera, H_Transform);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
		FHoudiniEngine::Get().GetSession(), CameraNodeId, &H_Transform), false);
	
	// Update this input's NodeId and ObjectNodeId
	InInputObject->SetInputNodeId(-1);// (int32)CameraNodeId;
	InInputObject->SetInputObjectNodeId((int32)CameraNodeId);

	// Update this input object's cache data
	InInputObject->Update(Camera, InInputSettings);

	return true;
}

bool
FHoudiniInputTranslator::UpdateLoadedInputs(UHoudiniAssetComponent* HAC)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::UpdateLoadedInputs);
	if (!IsValid(HAC))
		return false;

	// Nothing to do for Node Sync Components!
	if (HAC->IsA<UHoudiniNodeSyncComponent>())
		return true;

	// We need to call BuildAllInputs here to update all the inputs,
	// and make sure that the object path parameter inputs' parameter ids are up to date
	if (!FHoudiniInputTranslator::BuildAllInputs(HAC->GetAssetId(), HAC, HAC->Inputs, HAC->Parameters))
		return false;

	// We need to update the AssetID stored on all the inputs
	// and mark all the input objects for this input type as changed
	int32 HACAssetId = HAC->GetAssetId();
	for (auto CurrentInput : HAC->Inputs)
	{
		if (!IsValid(CurrentInput))
			continue;

		//
		CurrentInput->SetAssetNodeId(HACAssetId);

		// We need to delete the nodes created for the input objects if they are valid
		// (since the node IDs are transients, this likely means we're handling a recook/rebuild
		// and therefore expect to recreate the input nodes)
		DestroyInputNodes(CurrentInput, CurrentInput->GetInputType());
	}

	return true;
}



bool
FHoudiniInputTranslator::UpdateWorldInputs(UHoudiniAssetComponent* HAC)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::UpdateWorldInputs);

	if (!IsValid(HAC))
		return false;

	// Only tick/cook when in Editor
	// This prevents PIE cooks or runtime cooks due to inputs moving
	AActor* ActorOwner = HAC->GetOwner();
	if (ActorOwner)
	{
		if (!ActorOwner->GetWorld() || (ActorOwner->GetWorld()->WorldType != EWorldType::Editor))
			return false;
	}

#if WITH_EDITOR
	// Stop outliner objects from causing recooks while input objects are dragged around
	if (FHoudiniMoveTracker::Get().IsObjectMoving)
	{
		//HOUDINI_LOG_MESSAGE(TEXT("Object moving, not updating world inputs!"));
		return false;
	}
#endif

	for (auto CurrentInput : HAC->Inputs)
	{
		if (!CurrentInput)
			continue;
		if (CurrentInput->GetInputType() != EHoudiniInputType::World)
			continue;

		UpdateWorldInput(CurrentInput);
	}

	return true;
}

bool
FHoudiniInputTranslator::UpdateWorldInput(UHoudiniInput* InInput)
{
	if (!IsValid(InInput))
		return false;

	EHoudiniInputType WorldType = InInput->GetInputType();
	if (WorldType != EHoudiniInputType::World)
		return false;

	TArray<UHoudiniInputObject*>* InputObjectsPtr = InInput->GetHoudiniInputObjectArray(WorldType);
	if (!InputObjectsPtr)
		return false;

	bool bHasChanged = false;
	if (InInput->IsWorldInputBoundSelector() && InInput->GetWorldInputBoundSelectorAutoUpdates())
	{
		// If the input is in bound selector mode, and auto-update is enabled
		// update the actors selected by the bounds first
		bHasChanged = InInput->UpdateWorldSelectionFromBoundSelectors();
	}

	const FHoudiniInputObjectSettings InputSettings(InInput);
	
	// See if we need to update the components for this input
	// look for deleted actors/components	
	TArray<int32> ObjectToDeleteIndices;
	for(int32 InputObjIdx = 0; InputObjIdx < InputObjectsPtr->Num(); InputObjIdx++)
	{
		UHoudiniInputActor* ActorObject = Cast<UHoudiniInputActor>((*InputObjectsPtr)[InputObjIdx]);
		if (!IsValid(ActorObject))
			continue;

		// Make sure the actor is still valid
		AActor* const Actor = ActorObject->GetActor();
		bool bValidActorObject = IsValid(Actor);

		// For BrushActors, the brush and actors must be valid as well
		UHoudiniInputBrush* BrushActorObject = Cast<UHoudiniInputBrush>(ActorObject);
		if (bValidActorObject && BrushActorObject)
		{
			ABrush* BrushActor = BrushActorObject->GetBrush();
			if (!IsValid(BrushActor))
				bValidActorObject = false;
			else if (!IsValid(BrushActor->Brush))
				bValidActorObject = false;
		}

		// The actor is no longer valid, mark it for deletion
		if (!bValidActorObject)
		{
			if ((ActorObject->GetInputNodeId() > 0) || (ActorObject->GetInputObjectNodeId() > 0))
			{
				ActorObject->InvalidateData();
				// We only need to update the input if the actors nodes were created in Houdini
				bHasChanged = true;
			}
			
			// Delete the Actor object
			ObjectToDeleteIndices.Add(InputObjIdx);
			continue;
		}

		// If we send our input objects as references, we should recreate the whole input node for 
		// a transform change (as the transform is stored as a point attribute, not as a geo/object transform)
		bool bImportAsRef = InInput->GetImportAsReference();

		// We'll keep track of whether the actor transform changed so that
		// we can mark all the components as having changed transforms -- everything
		// needs to be updated.
		bool bActorTransformChanged = false;
		if (ActorObject->HasActorTransformChanged())
		{
			if (bImportAsRef)
				ActorObject->MarkChanged(true);
			else
				ActorObject->MarkTransformChanged(true);

			bHasChanged = true;
			bActorTransformChanged = true;
		}	

		if (ActorObject->HasContentChanged(InputSettings))
		{
			ActorObject->MarkChanged(true);
			bHasChanged = true;
		}

		// Ensure we are aware of all the components of the actor
		ActorObject->Update(Actor, InputSettings);

		// Check if any components have content or transform changes
		for (auto CurActorComp : ActorObject->GetActorComponents())
		{
			if (bActorTransformChanged || CurActorComp->HasComponentTransformChanged())
			{
				if (bImportAsRef)
					CurActorComp->MarkChanged(true);
				else
					CurActorComp->MarkTransformChanged(true);

				bHasChanged = true;
			}

			if (CurActorComp->HasComponentChanged(InputSettings))
			{
				CurActorComp->MarkChanged(true);
				bHasChanged = true;
			}

			USceneComponent* Component = CurActorComp->GetSceneComponent();
			if (IsValid(Component))
			{
				CurActorComp->Update(Component, InputSettings);
			}
		}

		// Check if we added/removed any components in the call to update
		if (ActorObject->GetLastUpdateNumComponentsAdded() > 0 || ActorObject->GetLastUpdateNumComponentsRemoved() > 0)
		{
			bHasChanged = true;
			if (ActorObject->GetLastUpdateNumComponentsRemoved() > 0)
				TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
	}

	// Delete the actor objects that were marked for deletion
	for (int32 ToDeleteIdx = ObjectToDeleteIndices.Num() - 1; ToDeleteIdx >= 0; ToDeleteIdx--)
	{
		InputObjectsPtr->RemoveAt(ObjectToDeleteIndices[ToDeleteIdx]);
		bHasChanged = true;
	}

	// If not a bound selector and auto select landscape splines is enabled, add all landscape splines of input
	// landscapes to our input objects
	if (!InInput->IsWorldInputBoundSelector() && InInput->IsLandscapeAutoSelectSplinesEnabled())
	{
		InInput->AddAllLandscapeSplineActorsForInputLandscapes();
	}

	// Mark the input as changed if need so it will trigger an upload
	if (bHasChanged)
	{
		InInput->MarkChanged(true);

		// Mark the outer package as dirty, to ensure that the changes are saved when using OFPA / World partition
		//InInput->MarkPackageDirty();

	}

	return true;
}


bool
FHoudiniInputTranslator::CreateInputNodeForReference(
	const HAPI_NodeId InParentNodeId,
	HAPI_NodeId& InputNodeId,
	const FString& InRef,
	const FString& InputNodeName,
	const FTransform& InTransform,
	const bool& bImportAsReferenceRotScaleEnabled,
	const bool& bImportAsReferenceBboxEnabled,
	const FBox& InBbox,
	const bool& bImportAsReferenceMaterialEnabled,
	const TArray<FString>& MaterialReferences)
{
	HAPI_NodeId NewNodeId = -1;

	// Create a single input node
	if (InParentNodeId >= 0)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateInputNode(InputNodeName, NewNodeId, InParentNodeId), false);
	}
	else
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateInputNode(
			FHoudiniEngine::Get().GetSession(), -1, &NewNodeId, TCHAR_TO_UTF8(*InputNodeName)), false);
	}

	/*
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
		FHoudiniEngine::Get().GetSession(), NewNodeId, nullptr), false);
	*/

	// Check if we have a valid id for this new input asset.
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(NewNodeId))
		return false;

	// We have now created a valid new input node, delete the previous one
	HAPI_NodeId PreviousInputNodeId = InputNodeId;
	if (PreviousInputNodeId >= 0)
	{
		// Get the parent OBJ node ID before deleting!
		HAPI_NodeId PreviousInputOBJNode = FHoudiniEngineUtils::HapiGetParentNodeId(PreviousInputNodeId);

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
			FHoudiniEngine::Get().GetSession(), PreviousInputNodeId))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input node for %s."), *InputNodeName);
		}

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
			FHoudiniEngine::Get().GetSession(), PreviousInputOBJNode))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input OBJ node for %s."), *InputNodeName);
		}
	}

	// Create and initialize a part containing one point with a point attribute
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);

	PartInfo.attributeCounts[HAPI_ATTROWNER_POINT] = 1;
	PartInfo.vertexCount = 0;
	PartInfo.faceCount = 0;
	PartInfo.pointCount = 1;
	PartInfo.type = HAPI_PARTTYPE_MESH;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
		FHoudiniEngine::Get().GetSession(), NewNodeId, 0, &PartInfo), false);

	// Point Position Attribute
	{
		// Create point attribute info for P.
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = 1;
		AttributeInfoPoint.tupleSize = 3;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
			HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint), false);

		// Set the point's position
		FVector3f ObjectPosition = (FVector3f)InTransform.GetLocation();
		TArray<float> Position =
		{
			ObjectPosition.X / HAPI_UNREAL_SCALE_FACTOR_POSITION,
			ObjectPosition.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION,
			ObjectPosition.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION
		};

		// Now that we have raw positions, we can upload them for our attribute.
		FHoudiniHapiAccessor Accessor(NewNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoPoint, Position), false);

	}

	if (bImportAsReferenceRotScaleEnabled)
	{
		// Create ROTATION attribute info
		HAPI_AttributeInfo AttributeInfoRotation;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoRotation);
		AttributeInfoRotation.count = 1;
		AttributeInfoRotation.tupleSize = 4;
		AttributeInfoRotation.exists = true;
		AttributeInfoRotation.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoRotation.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoRotation.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NewNodeId, 0,
			HAPI_UNREAL_ATTRIB_ROTATION,
			&AttributeInfoRotation), false);

		TArray<float> InputRotations;
		InputRotations.SetNumZeroed(4);

		FQuat InputRotation = InTransform.GetRotation();
		InputRotations[0] = (float)InputRotation.X;
		InputRotations[1] = (float)InputRotation.Z;
		InputRotations[2] = (float)InputRotation.Y;
		InputRotations[3] = (float)-InputRotation.W;

		//we can now upload them to our attribute.
		FHoudiniHapiAccessor Accessor(NewNodeId, 0, HAPI_UNREAL_ATTRIB_ROTATION);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoRotation, InputRotations), false);

		// Create SCALE attribute info
		HAPI_AttributeInfo AttributeInfoScale;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoScale);
		AttributeInfoScale.count = 1;
		AttributeInfoScale.tupleSize = 3;
		AttributeInfoScale.exists = true;
		AttributeInfoScale.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoScale.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoScale.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NewNodeId, 0,
			HAPI_UNREAL_ATTRIB_SCALE,
			&AttributeInfoScale), false);

		TArray<float> InputScales;
		InputScales.SetNumZeroed(3);

		FVector3f InputScale = (FVector3f)InTransform.GetScale3D();
		InputScales[0] = InputScale.X;
		InputScales[1] = InputScale.Z;
		InputScales[2] = InputScale.Y;

		//we can now upload them to our attribute.
		Accessor.Init(NewNodeId, 0, HAPI_UNREAL_ATTRIB_SCALE);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoScale, InputScales), false);

	}

	if (bImportAsReferenceBboxEnabled)
	{
		// Create attribute info for both bbox min and bbox max
		HAPI_AttributeInfo AttributeInfoBboxPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoBboxPoint);
		AttributeInfoBboxPoint.count = 1;
		AttributeInfoBboxPoint.tupleSize = 3;
		AttributeInfoBboxPoint.exists = true;
		AttributeInfoBboxPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoBboxPoint.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoBboxPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		// bbox min
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
			HAPI_UNREAL_ATTRIB_BBOX_MIN, &AttributeInfoBboxPoint), false);

		FVector3f InBboxMin = (FVector3f)InBbox.Min;
		TArray<float> BboxMin =
		{
			InBboxMin.X / HAPI_UNREAL_SCALE_FACTOR_POSITION,
			InBboxMin.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION,
			InBboxMin.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION
		};

		FHoudiniHapiAccessor Accessor(NewNodeId, 0, HAPI_UNREAL_ATTRIB_BBOX_MIN);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoBboxPoint, BboxMin), false);

		// bbox max
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
			HAPI_UNREAL_ATTRIB_BBOX_MAX, &AttributeInfoBboxPoint), false);

		FVector3f InBboxMax = (FVector3f)InBbox.Max;
		TArray<float> BboxMax =
		{
			InBboxMax.X / HAPI_UNREAL_SCALE_FACTOR_POSITION,
			InBboxMax.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION,
			InBboxMax.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION
		};

		Accessor.Init(NewNodeId, 0, HAPI_UNREAL_ATTRIB_BBOX_MAX);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoBboxPoint, BboxMax), false);
	}

	// Material Reference String Array Attribute
	if (bImportAsReferenceMaterialEnabled)
	{
		// Create point attribute info.
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = 1;
		AttributeInfoPoint.tupleSize = 1;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		// We set it to be multiple string attributes rather than a single string array attribute to not conflict
		// with any existing HDA's that use the attribute name unreal_material
		for (int i = 0; i < MaterialReferences.Num(); ++i)
		{
			FString AttributeName = HAPI_UNREAL_ATTRIB_MATERIAL;
			if (i > 0)
				AttributeName.AppendInt(i);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
				TCHAR_TO_ANSI(*AttributeName), &AttributeInfoPoint), false);

			FHoudiniHapiAccessor Accessor(NewNodeId, 0, TCHAR_TO_ANSI(*AttributeName));
			HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfoPoint, MaterialReferences[i]), false);
		}
	}


	// Unreal Reference String Attribute
	{
		// Create point attribute info.
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = 1;
		AttributeInfoPoint.tupleSize = 1;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), NewNodeId, 0,
			HAPI_UNREAL_ATTRIB_INSTANCE_OVERRIDE, &AttributeInfoPoint), false);

		FHoudiniHapiAccessor Accessor(NewNodeId, 0, HAPI_UNREAL_ATTRIB_INSTANCE_OVERRIDE);
		HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfoPoint, InRef), false);
	}

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(NewNodeId), false);

	InputNodeId = NewNodeId;
	return true;
}

bool FHoudiniInputTranslator::CreateInputNodeForReference(
	HAPI_NodeId& InputNodeId,
	UObject const* const InObjectToRef,
	const FString& InputNodeName,
	const FTransform& InTransform,
	const bool& bImportAsReferenceRotScaleEnabled,
	FUnrealObjectInputHandle& OutHandle,
	const bool& bInputNodesCanBeDeleted,
	const bool& bImportAsReferenceBboxEnabled,
	const FBox& InBbox,
	const bool& bImportAsReferenceMaterialEnabled,
	const TArray<FString>& MaterialReferences)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::CreateInputNodeForReference);

	// The identifier to the node in the input system
	FString FinalInputNodeName = InputNodeName;
	HAPI_NodeId ParentNodeId = -1;
	FUnrealObjectInputHandle ParentHandle;
	FUnrealObjectInputIdentifier Identifier;
	{
		// Build the identifier for the entry in the manager
		constexpr bool bIsLeaf = true;
		FUnrealObjectInputOptions Options;
		Options.bImportAsReference = true;
		Options.bImportAsReferenceRotScaleEnabled = bImportAsReferenceRotScaleEnabled;
		Identifier = FUnrealObjectInputIdentifier(InObjectToRef, Options, bIsLeaf);

		// If the entry exists in the manager, the associated HAPI nodes are valid, and it is not marked as dirty, then
		// return the existing entry
		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::NodeExistsAndIsNotDirty(Identifier, Handle))
		{
			HAPI_NodeId NodeId = -1;
			if (FUnrealObjectInputUtils::GetHAPINodeId(Handle, NodeId))
			{
				// Make sure the node cant be deleted if needed
				if (!bInputNodesCanBeDeleted)
					FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(Handle, bInputNodesCanBeDeleted);

				OutHandle = Handle;
				InputNodeId = NodeId;
				return true;
			}
		}
		// If the entry does not exist, or is invalid, then we need create it
		FUnrealObjectInputUtils::GetDefaultInputNodeName(Identifier, FinalInputNodeName);
		// Create any parent/container nodes that we would need, and get the node id of the immediate parent
		if (FUnrealObjectInputUtils::EnsureParentsExist(Identifier, ParentHandle, bInputNodesCanBeDeleted) && ParentHandle.IsValid())
			FUnrealObjectInputUtils::GetHAPINodeId(ParentHandle, ParentNodeId);
	}

	const FString AssetReference =
		UHoudiniInputObject::FormatAssetReference(InObjectToRef->GetFullName());

	HAPI_NodeId NodeId = -1;
	const bool bSuccess = CreateInputNodeForReference(
		ParentNodeId,
		NodeId,
		AssetReference,
		FinalInputNodeName,
		InTransform,
		bImportAsReferenceRotScaleEnabled,
		bImportAsReferenceBboxEnabled,
		InBbox,
		bImportAsReferenceMaterialEnabled,
		MaterialReferences);

	if (!bSuccess)
		return false;

	InputNodeId = NodeId;
	
	{
		// Record the node in the manager
		const HAPI_NodeId ObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(InputNodeId);
		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::AddNodeOrUpdateNode(Identifier, InputNodeId, Handle, ObjectNodeId, nullptr, bInputNodesCanBeDeleted))
			OutHandle = Handle;
	}

	return bSuccess;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForDataTable(
	const FString& InNodeName,
	UHoudiniInputDataTable* InInputObject,
	const FHoudiniInputObjectSettings& InInputSettings,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForDataTable);

	if (!IsValid(InInputObject))
		return false;

	UDataTable* DataTable = InInputObject->GetDataTable();
	if (!IsValid(DataTable))
		return true;
	
	FString DataTableName = InNodeName + TEXT("_") + DataTable->GetName();
	FHoudiniEngineUtils::SanitizeHAPIVariableName(DataTableName);

	FUnrealObjectInputHandle DTInputNodeHandle;
	HAPI_NodeId InputNodeId = -1;

	// Get the existing node id, if any
	
	// For the ref counted system the handle on the input object represents a reference node that has a single node
	// it references: the data table. The reference node represents InObject with its Transform (geometry input).
	{
		TSet<FUnrealObjectInputHandle> ReferencedNodes;
		if (FUnrealObjectInputUtils::GetReferencedNodes(InInputObject->InputNodeHandle, ReferencedNodes) && ReferencedNodes.Num() == 1)
		{
			const FUnrealObjectInputHandle Handle = ReferencedNodes.Array()[0];
			FUnrealObjectInputUtils::GetHAPINodeId(Handle, InputNodeId);
		}
	}

	if (!FUnrealDataTableTranslator::CreateInputNodeForDataTable(DataTable, InputNodeId, DataTableName, DTInputNodeHandle, bInputNodesCanBeDeleted))
		return false;


	{
		// The data table can have its own transform (geometry input), so we have to create a reference node that
		// represents InInputObject in the new input system that references the DataTable asset's input node handle
		FUnrealObjectInputOptions Options;
		static constexpr bool bIsLeaf = false;
		FUnrealObjectInputIdentifier GeoInputRefNodeId(InInputObject, Options, bIsLeaf);
		FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(GeoInputRefNodeId, { DTInputNodeHandle }, InInputObject->InputNodeHandle, true, bInputNodesCanBeDeleted);
	}

	if (!HapiSetGeoObjectTransform(InInputObject->GetInputObjectNodeId(), InInputObject->GetHoudiniObjectTransform()))
		return false;

	// Update the cached data and input settings
	InInputObject->Update(DataTable, InInputSettings);

	/*
	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
		FHoudiniEngine::Get().GetSession(), InputNodeId), false);

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
		FHoudiniEngine::Get().GetSession(), InputNodeId, nullptr), false);
	*/

	return true;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForFoliageType_InstancedStaticMesh(
	const FString& InObjNodeName,
	UHoudiniInputFoliageType_InstancedStaticMesh* InObject,
	const FHoudiniInputObjectSettings& InInputSettings,
	const bool& bInputNodesCanBeDeleted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInputTranslator::HapiCreateInputNodeForFoliageType_InstancedStaticMesh);

	if (!IsValid(InObject))
		return false;

	UFoliageType_InstancedStaticMesh* FoliageType = Cast<UFoliageType_InstancedStaticMesh>(InObject->GetObject());
	if (!IsValid(FoliageType))
		return true;
	
	UStaticMesh* const SM = FoliageType->GetStaticMesh();
	if (!IsValid(SM))
		return true;

	FString FTName = InObjNodeName + TEXT("_") + FoliageType->GetName();
	FHoudiniEngineUtils::SanitizeHAPIVariableName(FTName);

	// Marshall the Static Mesh to Houdini
	FUnrealObjectInputHandle InputNodeHandle;
	constexpr bool bUseRefCountedInputSystem = false;
	bool bSuccess = true;

	HAPI_NodeId InputNodeId = InObject->GetInputNodeId();
	if (InInputSettings.bImportAsReference) 
	{
		FBox InBbox = InInputSettings.bImportAsReferenceBboxEnabled ? SM->GetBoundingBox() : FBox(EForceInit::ForceInit);

		const TArray<FString>& MaterialReferences = InInputSettings.bImportAsReferenceMaterialEnabled ?
			InObject->GetMaterialReferences() :
			TArray<FString>();

		bSuccess = FUnrealFoliageTypeTranslator::CreateInputNodeForReference(
			InputNodeId,
			FoliageType,
			FTName,
			InObject->GetTransform(),
			InInputSettings.bImportAsReferenceRotScaleEnabled,
			InputNodeHandle,
			bInputNodesCanBeDeleted,
			InInputSettings.bImportAsReferenceBboxEnabled,
			InBbox,
			InInputSettings.bImportAsReferenceMaterialEnabled,
			MaterialReferences);
	}
	else 
	{
		bSuccess = FUnrealFoliageTypeTranslator::HapiCreateInputNodeForFoliageType_InstancedStaticMesh(
			FoliageType,
			InputNodeId,
			FTName,
			InputNodeHandle,
			InInputSettings.bExportLODs,
			InInputSettings.bExportSockets,
			InInputSettings.bExportColliders,
			InInputSettings.bExportMaterialParameters);
	}

	// Update this input object's OBJ NodeId
	InObject->SetInputNodeId(InputNodeId);
	InObject->SetInputObjectNodeId(FHoudiniEngineUtils::HapiGetParentNodeId(InputNodeId));
	InObject->InputNodeHandle = InputNodeHandle;

	// Update the cached data and input settings
	InObject->Update(FoliageType, InInputSettings);
	
	// If the Input mesh has a Transform offset
	const FTransform TransformOffset = InObject->GetHoudiniObjectTransform();
	if (bUseRefCountedInputSystem || !TransformOffset.Equals(FTransform::Identity))
	{
		// Updating the Transform
		HAPI_TransformEuler HapiTransform;
		FHoudiniApi::TransformEuler_Init(&HapiTransform);
		FHoudiniEngineUtils::TranslateUnrealTransform(TransformOffset, HapiTransform);

		// Set the transform on the OBJ parent
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
			FHoudiniEngine::Get().GetSession(), InObject->GetInputObjectNodeId(), &HapiTransform), false);
	}

	return bSuccess;
}

// Upload all the input's data layers to Houdini
bool FHoudiniInputTranslator::UploadDataLayers(UHoudiniInput* InInput, FHoudiniUnrealDataLayersCache* Cache)
{
	return true;
}

bool
FHoudiniInputTranslator::CreateMergeSOP(
	HAPI_NodeId& InOutMergeNodeId,
	const TArray<HAPI_NodeId>& InNodeIdsToConnect,
	const FString& InMergeNodeName)
{
	HAPI_NodeId NodeId = -1;

	// Create the merge node
	if (FHoudiniEngineUtils::CreateNode(-1, TEXT("SOP/merge"), InMergeNodeName, true, &NodeId) != HAPI_RESULT_SUCCESS)
		return false;

	// If the previous node was valid, attempt to delete it
	if (InOutMergeNodeId >= 0 && FHoudiniEngineUtils::IsHoudiniNodeValid(InOutMergeNodeId))
	{
		const HAPI_NodeId ObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(InOutMergeNodeId);
		if (ObjectNodeId >= 0)
			FHoudiniEngineUtils::DeleteHoudiniNode(ObjectNodeId);
		else
			FHoudiniEngineUtils::DeleteHoudiniNode(InOutMergeNodeId);
	}

	InOutMergeNodeId = NodeId;

	if (!SetMergeSOPInputs(InOutMergeNodeId, InNodeIdsToConnect))
		return false;

	return true;
}

bool
FHoudiniInputTranslator::SetMergeSOPInputs(const HAPI_NodeId InMergeNodeId, const TArray<HAPI_NodeId>& InNodeIdsToConnect)
{
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(InMergeNodeId))
		return false;

	const HAPI_Session* const Session = FHoudiniEngine::Get().GetSession();

	// Get currently connected inputs
	TArray<HAPI_NodeId> PrevConnectedNodes;
	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);
	if (FHoudiniApi::GetNodeInfo(Session, InMergeNodeId, &NodeInfo) == HAPI_RESULT_SUCCESS)
	{
		// There is no function in HAPI currently to directly get the number of _connected_ input nodes or to
		// compose a list of the connected input nodes.
		// Nodes with "infinite" inputs, such as the Merge SOP, always have NodeInfo.inputCount == 9999. So we
		// stop iteration at the first disconnected input instead of visiting all 9999 possible indices.
		for (int32 InputIndex = 0; InputIndex < NodeInfo.inputCount; ++InputIndex)
		{
			HAPI_NodeId ConnectedInputNodeId = -1;
			if (FHoudiniApi::QueryNodeInput(Session, InMergeNodeId, InputIndex, &ConnectedInputNodeId) != HAPI_RESULT_SUCCESS || ConnectedInputNodeId < 0)
				break;
			PrevConnectedNodes.Add(ConnectedInputNodeId);
		}
	}

	// Connect referenced nodes
	TSet<HAPI_NodeId> ConnectedNodeSet;
	int32 InputIndex = 0;
	for (const HAPI_NodeId& NodeId : InNodeIdsToConnect)
	{
		if (NodeId < 0)
			continue;

		// Connect the current input object to the merge node
		if (FHoudiniApi::ConnectNodeInput(Session, InMergeNodeId, InputIndex, NodeId, 0) != HAPI_RESULT_SUCCESS)
		{
			HOUDINI_LOG_WARNING(TEXT("[FUnrealObjectInputUtils::ConnectReferencedNodes] Failed to connected node input: %s"), *FHoudiniEngineUtils::GetErrorDescription());
			continue;
		}
		// HAPI will automatically create a object_merge node (we are expecting that NodeId and RefNodeId are never in
		// the same network), we need to set the xformtype to "Into specified object"
		HAPI_NodeId ConnectedNodeId = -1;
		if (FHoudiniApi::QueryNodeInput(Session, InMergeNodeId, InputIndex, &ConnectedNodeId) != HAPI_RESULT_SUCCESS)
		{
			HOUDINI_LOG_WARNING(TEXT("[FUnrealObjectInputUtils::ConnectReferencedNodes] Failed to query connected node input: %s"), *FHoudiniEngineUtils::GetErrorDescription());
			continue;

		}
		InputIndex++;

		if (ConnectedNodeId < 0)
		{
			// No connection was made even though the previous functions were successful!?
			continue;
		}

		ConnectedNodeSet.Add(ConnectedNodeId);
		
		// Set the transform value to "Into Specified Object"
		// Set the transform object to the world origin null from the manager
		FUnrealObjectInputUtils::SetObjectMergeXFormTypeToWorldOrigin(ConnectedNodeId);
	}

	// Disconnect previously connected nodes at indices >= FirstUnusedInputIndex
	// Disconnect in reverse: inputs are consolidated on disconnect on "infinite" input nodes like the Merge SOP
	const int32 FirstUnusedInputIndex = InputIndex; 
	for (int32 InputIndexToDelete = PrevConnectedNodes.Num(); InputIndexToDelete >= FirstUnusedInputIndex; --InputIndexToDelete)
	{
		HOUDINI_CHECK_ERROR(FHoudiniApi::DisconnectNodeInput(Session, InMergeNodeId, InputIndexToDelete));
	}

	// Delete nodes from previous connections that are no longer used (the object merge SOPs automatically created
	// by HAPI) 
	for (const HAPI_NodeId& NodeToDeleteId : PrevConnectedNodes)
	{
		if (ConnectedNodeSet.Contains(NodeToDeleteId))
			continue;
		// Check that the node is valid / still exists before attempting to delete the node
		HAPI_NodeInfo NodeToDeleteInfo;
		FHoudiniApi::NodeInfo_Init(&NodeToDeleteInfo);
		if (FHoudiniApi::GetNodeInfo(Session, NodeToDeleteId, &NodeInfo) != HAPI_RESULT_SUCCESS)
			continue;
		bool bNodeIsValid = false;
		if (FHoudiniApi::IsNodeValid(Session, NodeToDeleteId, NodeInfo.uniqueHoudiniNodeId, &bNodeIsValid) != HAPI_RESULT_SUCCESS || !bNodeIsValid)
			continue;
		HOUDINI_CHECK_ERROR(FHoudiniApi::DeleteNode(Session, NodeToDeleteId));
	}
	
	return true;
}


#undef LOCTEXT_NAMESPACE
