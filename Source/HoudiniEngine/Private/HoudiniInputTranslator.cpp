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

#include "HoudiniInput.h"
#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineString.h"
#include "HoudiniParameter.h"
#include "HoudiniParameterOperatorPath.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniInputObject.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniSplineTranslator.h"
#include "HoudiniAssetActor.h"
#include "HoudiniOutputTranslator.h"
#include "UnrealBrushTranslator.h"
#include "UnrealSplineTranslator.h"
#include "UnrealMeshTranslator.h"
#include "UnrealInstanceTranslator.h"
#include "UnrealLandscapeTranslator.h"
#include "UnrealFoliageTypeTranslator.h"
#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputManager.h"

#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Landscape.h"
#include "Engine/Brush.h"
#include "Engine/DataTable.h"
#include "Camera/CameraComponent.h"
#include "FoliageType_InstancedStaticMesh.h"

#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

#if WITH_EDITOR
	#include "Editor.h"
	#include "Editor/UnrealEdEngine.h"
	#include "UnrealEdGlobals.h"
#endif

#include "HCsgUtils.h"
#include "LandscapeInfo.h"
#include "UnrealGeometryCollectionTranslator.h"

#include "Async/Async.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionObject.h"

#include "UObject/TextProperty.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

#if WITH_EDITOR

namespace
{
	template<typename T, typename P>
	TArray<T>
	PopulateTArray(TMap<FName, uint8*>::TConstIterator It,
		uint32 NumRows,
		uint32 NumComponents,
		uint32 Offset,
		uint32 ComponentSize,
		const TArray<uint32>& Order)
	{
		TArray<T> Values;
		Values.Reserve(NumRows * NumComponents);
		for (; It; ++It)
		{
			const uint8* Data = It.Value();
			for (uint32 Idx = 0; Idx < NumComponents; ++Idx)
			{
				Values.Add(P::GetPropertyValue(Data + Offset + Order[Idx] * ComponentSize));
			}
			
		}
		return Values;
	}

	template<typename T, typename P>
	TArray<T>
	PopulateTArray(TMap<FName, uint8*>::TConstIterator It,
		uint32 NumRows,
		uint32 NumComponents,
		uint32 Offset,
		uint32 ComponentSize)
	{
		TArray<uint32> Order;
		Order.Reserve(NumComponents);
		for (uint32 Idx = 0; Idx < NumComponents; ++Idx)
		{
			Order.Add(Idx);
		}
		return PopulateTArray<T, P>(It, NumRows, NumComponents, Offset, ComponentSize, Order);
	}

	TArray<int8>
	PopulateBoolArray(TMap<FName, uint8*>::TConstIterator It,
		const FBoolProperty& Prop,
		uint32 Count,
		uint32 Offset)
	{
		TArray<int8> Values;
		Values.Reserve(Count);
		for (; It; ++It)
		{
			const uint8* Data = It.Value();
			Values.Add(Prop.GetPropertyValue(Data + Offset));
		}
		return Values;
	}
};

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
	if (!IsValid(HAC))
		return false;

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
	// Ensure the asset has a valid node ID
	if (AssetId < 0)
	{
		return false;
	}

	// Start by getting the asset's info
	HAPI_AssetInfo AssetInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(
		FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo), false);

	// Get the number of geo (SOP) inputs
	int32 InputCount = AssetInfo.geoInputCount;
	/*
	// It's best to update the input count even if the hda hasnt cooked
	// as it can cause loaded geo inputs to disappear upon loading the level
	if ( AssetInfo.hasEverCooked )
	{
		InputCount = AssetInfo.geoInputCount;
	}
	*/
	// Also look for object path parameters inputs

	// Helper map to get the parameter index, given the parameter name
	TMap<FString, int32> ParameterNameToIndexMap;
	
	TArray<TWeakObjectPtr<UHoudiniParameter>> InputParameters;
	for (auto Param : Parameters)
	{
		if (!Param)
			continue;
		
		if (Param->GetParameterType() == EHoudiniParameterType::Input)
		{
			int InsertionIndex = InputParameters.Num();
			ParameterNameToIndexMap.Add(Param->GetParameterName(), InsertionIndex);
			InputParameters.Add(Param);
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
		// TODO: Properly clean up the input object + created nodes?
		for (int32 InputIdx = Inputs.Num() - 1; InputIdx >= InputCount; InputIdx--)
		{
			UHoudiniInput* CurrentInput = Inputs[InputIdx];
			if (!IsValid(CurrentInput))
				continue;

			FHoudiniInputTranslator::DisconnectAndDestroyInput(CurrentInput, CurrentInput->GetInputType());
			
			// DO NOT MANUALLY DESTROY THE OLD/DANGLING INPUTS!
			// This messes up unreal's Garbage collection and would cause crashes on duplication
			//CurrentInput->ConditionalBeginDestroy();
			//CurrentInput = nullptr;
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

			if (ParameterNameToIndexMap.Contains(CurrentInput->GetName()))
			{
				const int32 ParameterIndex = ParameterNameToIndexMap[CurrentInput->GetName()];
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
			case EHoudiniInputType::Landscape:
				//FUnrealLandscapeTranslator::UpdateHoudiniInputLandscapes(CurrentInput);
				break;
			case EHoudiniInputType::Asset:
				break;
			case EHoudiniInputType::Geometry:
				break;
			case EHoudiniInputType::Skeletal:
				break;
			case EHoudiniInputType::World:
				break;
			case EHoudiniInputType::GeometryCollection:
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

	if (InputType == EHoudiniInputType::Asset)
	{
		// TODO:
		// If we're an asset input, just remove us from the downstream connection on the input HDA
		// then reset this input's flag

		// TODO: Check this? Clean our DS assets?? why?? likely uneeded
		UHoudiniAssetComponent* OuterHAC = Cast<UHoudiniAssetComponent>(InputToDestroy->GetOuter());
		if (OuterHAC)
			OuterHAC->ClearDownstreamHoudiniAsset();

		InputToDestroy->SetInputNodeId(-1);
	}

	return true;
}

bool
FHoudiniInputTranslator::DestroyInputNodes(UHoudiniInput* InputToDestroy, const EHoudiniInputType& InputType)
{
	if (!IsValid(InputToDestroy))
		return false;

	if (!InputToDestroy->CanDeleteHoudiniNodes())
		return false;

	// If we're destroying an asset input, don't destroy anything as we don't want to destroy the input HDA
	// a simple disconnect is sufficient
	if (InputType == EHoudiniInputType::Asset)
		return true;

	const bool bUseRefCountedInputSystem = FHoudiniEngineRuntimeUtils::IsRefCountedInputSystemEnabled();
	IUnrealObjectInputManager const* const Manager = bUseRefCountedInputSystem ? FUnrealObjectInputManager::Get() : nullptr;
	
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

			if (bUseRefCountedInputSystem && Manager && CurInputObject->InputNodeHandle.IsValid())
			{
				// If the ref counted system is being used, and we have a valid input handle, remove the HAPI nodes
				// associated with the handle from CreatedInputDataAssetIds so that we don't delete those nodes here:
				// the manager will take care of their deletion when necessary.
				ManagedHAPINodeIds.Reset();
				if (Manager->GetHAPINodeIds(CurInputObject->InputNodeHandle.GetIdentifier(), ManagedHAPINodeIds)
						&& !ManagedHAPINodeIds.IsEmpty())
				{
					for (const int32& NodeId : ManagedHAPINodeIds)
					{
						CreatedInputDataAssetIds.Remove(NodeId);

						if (NodeId == CurInputObject->InputNodeId)
							CurInputObject->InputNodeId = -1;
						if (NodeId == CurInputObject->InputObjectNodeId)
							CurInputObject->InputObjectNodeId = -1;
					}
				}
			}

			if (CurInputObject->Type == EHoudiniInputObjectType::HoudiniAssetComponent)
			{
				// Houdini Asset Input, we don't want to destroy / invalidate the input HDA!
				// Just remove this input object's node Id from the CreatedInputDataAssetIds array
				// to avoid its deletion further down
				CreatedInputDataAssetIds.Remove(CurInputObject->InputNodeId);
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
							CreatedInputDataAssetIds.Remove(CurComponent->InputNodeId);
							CreatedInputDataAssetIds.Remove(CurComponent->InputObjectNodeId);
							continue;
						}
						
						if (bUseRefCountedInputSystem && Manager && CurComponent->InputNodeHandle.IsValid())
						{
							// If the ref counted system is being used, and we have a valid input handle, remove the HAPI nodes
							// associated with the handle from CreatedInputDataAssetIds so that we don't delete those nodes here:
							// the manager will take care of their deletion when necessary.
							ManagedHAPINodeIds.Reset();
							if (Manager->GetHAPINodeIds(CurComponent->InputNodeHandle.GetIdentifier(), ManagedHAPINodeIds)
									&& !ManagedHAPINodeIds.IsEmpty())
							{
								for (const int32& NodeId : ManagedHAPINodeIds)
								{
									CreatedInputDataAssetIds.Remove(NodeId);

									if (NodeId == CurComponent->InputNodeId)
										CurComponent->InputNodeId = -1;
									if (NodeId == CurComponent->InputObjectNodeId)
										CurComponent->InputObjectNodeId = -1;
								}
							}
						}
						
						// No need to delete the nodes created for an asset component manually here,
						// As they will be deleted when we clean up the CreateNodeIds array
						CurComponent->InputNodeId = -1;
					}
				}
			}
			// No need to delete the nodes created for an asset component manually here,
			// As they will be deleted when we clean up the CreateNodeIds array

			if (CurInputObject->InputNodeId >= 0)
			{
				FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), CurInputObject->InputNodeId);
				CurInputObject->InputNodeId = -1;
			}

			if(CurInputObject->InputObjectNodeId >= 0)
			{
				FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), CurInputObject->InputObjectNodeId);
				CurInputObject->InputObjectNodeId = -1;

				// TODO: CHECK ME!
				HAPI_NodeId ParentNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(CurInputObject->InputObjectNodeId);

				// Delete its parent node as well
				if (FHoudiniEngineUtils::IsHoudiniNodeValid(ParentNodeId))
					FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), ParentNodeId);
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
		if (AssetNodeId < 0)
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
		InputType = EHoudiniInputType::Landscape;

	else if ((InputName.Contains(worldPrefix, ESearchCase::IgnoreCase))
		|| (InputName.Contains(worldPrefix2, ESearchCase::IgnoreCase)))
		InputType = EHoudiniInputType::World;

	else if ((InputName.Contains(assetPrefix, ESearchCase::IgnoreCase))
		|| (InputName.Contains(assetPrefix2, ESearchCase::IgnoreCase)))
		InputType = EHoudiniInputType::Asset;

	return InputType;
}

bool
FHoudiniInputTranslator::ChangeInputType(UHoudiniInput* InInput, const bool& bForce)
{
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

		if (FoundActor->IsA<UHoudiniAssetComponent>())
		{
			// Select the HDA in the asset input
			Input->SetInputObjectAt(EHoudiniInputType::Asset, HDAIdx++, FoundActor);
		}
		else if (FoundActor->IsA<ALandscapeProxy>())
		{
			// Select the landscape in the landscape input
			Input->SetInputObjectAt(EHoudiniInputType::Landscape, LandscapedIdx++, FoundActor);
		}

		// Remove the Found Token
		Tokens.RemoveAt(FoundIdx);
	}

	// See if we should change the default input type
	if (Input->GetInputType() == EHoudiniInputType::Geometry && WorldIdx > 0 && GeoIdx == 0)
	{		
		if (LandscapedIdx == WorldIdx)
		{
			// We've only selected landscapes, set to landscape IN
			Input->SetInputType(EHoudiniInputType::Landscape, bOutBlueprintStructureModified);
		}		
		else if (HDAIdx == WorldIdx)
		{
			// We've only selected Houdini Assets, set to Asset IN
			Input->SetInputType(EHoudiniInputType::Asset, bOutBlueprintStructureModified);
		}			
		else
		{
			// Set to world input
			Input->SetInputType(EHoudiniInputType::World, bOutBlueprintStructureModified);
		}			
	}

	return true;
}

bool
FHoudiniInputTranslator::UploadChangedInputs(UHoudiniAssetComponent * HAC)
{
	if (!IsValid(HAC))
		return false;

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

		if (CurrentInput->GetInputType() == EHoudiniInputType::Landscape && CurrentInput->HasLandscapeExportTypeChanged()) 
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
	if ((ParentNodeId >= 0) && (InputType != EHoudiniInputType::Geometry) && (InputType != EHoudiniInputType::Asset) && (InputType != EHoudiniInputType::GeometryCollection))
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
	if (InputType != EHoudiniInputType::Geometry && InputType != EHoudiniInputType::GeometryCollection)
	{
		// Nothing to change
		return true;
	}

	// Get the input objects
	TArray<UHoudiniInputObject*>* InputObjectsArray = InInput->GetHoudiniInputObjectArray(InInput->GetInputType());
	if (!ensure(InputObjectsArray))
		return false;
	
	// Update each object's transform offset
	for (int32 ObjIdx = 0; ObjIdx < InputObjectsArray->Num(); ObjIdx++)
	{
		UHoudiniInputObject* CurrentInputObject = (*InputObjectsArray)[ObjIdx];
		if (!IsValid(CurrentInputObject))
			continue;

		// If the Input mesh has a Transform offset
		FTransform TransformOffset = CurrentInputObject->Transform;
		if (!TransformOffset.Equals(FTransform::Identity))
		{
			// Updating the Transform
			HAPI_TransformEuler HapiTransform;
			FHoudiniApi::TransformEuler_Init(&HapiTransform);
			FHoudiniEngineUtils::TranslateUnrealTransform(TransformOffset, HapiTransform);

			// Set the transform on the OBJ parent
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
				FHoudiniEngine::Get().GetSession(), CurrentInputObject->InputObjectNodeId, &HapiTransform), false);
		}
	}

	return true;
}

bool
FHoudiniInputTranslator::UploadInputData(UHoudiniInput* InInput, const FTransform & InActorTransform)
{
	if (!IsValid(InInput))
		return false;

	EHoudiniInputType InputType = InInput->GetInputType();
	TArray<UHoudiniInputObject*>* InputObjectsArray = InInput->GetHoudiniInputObjectArray(InInput->GetInputType());
	if (!ensure(InputObjectsArray))
		return false;

	// Iterate on all the input objects and see if they need to be uploaded
	bool bSuccess = true;
	TArray<int32> CreatedNodeIds;
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
			if (!UploadHoudiniInputObject(InInput, ChangedInputObject, InActorTransform, CreatedNodeIds))
				bSuccess = false;
		}
	}

	// If we haven't created any input, invalidate our input node id
	if (CreatedNodeIds.Num() == 0)
	{
		if (!InInput->HasInputTypeChanged())
		{
			int32 InputNodeId = InInput->GetInputNodeId();
			TArray<int32> PreviousInputObjectNodeIds = InInput->GetCreatedDataNodeIds();

			if (InInput->GetInputType() == EHoudiniInputType::Asset)
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
			if (InInput->GetInputType() != EHoudiniInputType::Asset)
				HOUDINI_CHECK_ERROR(FHoudiniApi::QueryNodeInput(
					FHoudiniEngine::Get().GetSession(), InputNodeId, Idx, &InputObjectMergeId));

			// Disconnect the two nodes
			HOUDINI_CHECK_ERROR(FHoudiniApi::DisconnectNodeInput(
				FHoudiniEngine::Get().GetSession(), InputNodeId, Idx));

			// Destroy the object merge node, do not destroy other HDA (Asset input type)
			if (InInput->GetInputType() != EHoudiniInputType::Asset)
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

		int32& CurrentInputObjectNodeId = CurrentInputObject->InputObjectNodeId;
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
		std::string ParamNameString = TCHAR_TO_UTF8(*(InInput->GetName()));

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
	const bool& bInputNodesCanBeDeleted)
{
	if (!InInput || !InInputObject)
		return false;

	FString ObjBaseName = InInput->GetNodeBaseName();

	bool bSuccess = true;
	switch (InInputObject->Type)
	{
		case EHoudiniInputObjectType::Object:
		{
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForObject(ObjBaseName, InInputObject);

			if (bSuccess)
				OutCreatedNodeIds.Add(InInputObject->InputObjectNodeId);

			break;
		}

		case EHoudiniInputObjectType::StaticMesh:
		{
			UHoudiniInputStaticMesh* InputSM = Cast<UHoudiniInputStaticMesh>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForStaticMesh(
				ObjBaseName,
				InputSM,
				InInput->GetExportLODs(),
				InInput->GetExportSockets(),
				InInput->GetExportColliders(),
				InInput->GetImportAsReference(),
				InInput->GetImportAsReferenceRotScaleEnabled(),
				InInput->GetImportAsReferenceBboxEnabled(),
				InInput->GetImportAsReferenceMaterialEnabled(),
				bInputNodesCanBeDeleted,
				InInput->GetPreferNaniteFallbackMesh(),
				InInput->GetExportMaterialParameters());

			if (bSuccess)
			{
				OutCreatedNodeIds.Add(InInputObject->InputObjectNodeId);
			}

			break;
		}

		case EHoudiniInputObjectType::SkeletalMesh:
		{
			UHoudiniInputSkeletalMesh* InputSkelMesh = Cast<UHoudiniInputSkeletalMesh>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForSkeletalMesh(
				ObjBaseName,
				InputSkelMesh,
				InInput->GetImportAsReference(),
				InInput->GetImportAsReferenceRotScaleEnabled(),
				InInput->GetImportAsReferenceBboxEnabled(),
				InInput->GetImportAsReferenceMaterialEnabled(),
				bInputNodesCanBeDeleted);

			if (bSuccess)
				OutCreatedNodeIds.Add(InInputObject->InputObjectNodeId);

			break;
		}

		case EHoudiniInputObjectType::SkeletalMeshComponent:
		{
			UHoudiniInputSkeletalMeshComponent* InputSKC = Cast<UHoudiniInputSkeletalMeshComponent>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForSkeletalMeshComponent(
				ObjBaseName,
				InputSKC,
				InInput->GetKeepWorldTransform(),
				InInput->GetImportAsReference(),
				InInput->GetImportAsReferenceRotScaleEnabled(),
				InInput->GetImportAsReferenceBboxEnabled(),
				InInput->GetImportAsReferenceMaterialEnabled(),
				InActorTransform,
				bInputNodesCanBeDeleted);

			if (bSuccess)
				OutCreatedNodeIds.Add(InInputObject->InputObjectNodeId);

			break;
		}
		case EHoudiniInputObjectType::GeometryCollection:
		{	
			UHoudiniInputGeometryCollection* InputGeometryCollection = Cast<UHoudiniInputGeometryCollection>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForGeometryCollection(
				ObjBaseName,
				InputGeometryCollection,
				InInput->GetImportAsReference(),
				InInput->GetImportAsReferenceRotScaleEnabled(),
				InInput->GetImportAsReferenceBboxEnabled(),
				InInput->GetImportAsReferenceMaterialEnabled(),
				InInput->GetExportMaterialParameters(),
				bInputNodesCanBeDeleted);

			if (bSuccess)
				OutCreatedNodeIds.Add(InInputObject->InputObjectNodeId);

			break;
		}
		case EHoudiniInputObjectType::GeometryCollectionComponent:
		{	
			UHoudiniInputGeometryCollectionComponent* InputGeometryCollection = Cast<UHoudiniInputGeometryCollectionComponent>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForGeometryCollectionComponent(
				ObjBaseName,
				InputGeometryCollection,
				InInput->GetKeepWorldTransform(),
				InInput->GetImportAsReference(),
				InInput->GetImportAsReferenceRotScaleEnabled(),
				InInput->GetImportAsReferenceBboxEnabled(),
				InInput->GetImportAsReferenceMaterialEnabled(),
				InInput->GetExportMaterialParameters(),
				InActorTransform,
				bInputNodesCanBeDeleted);

			if (bSuccess)
				OutCreatedNodeIds.Add(InInputObject->InputObjectNodeId);

			break;
		}
		case EHoudiniInputObjectType::SceneComponent:
		{
			UHoudiniInputSceneComponent* InputSceneComp = Cast<UHoudiniInputSceneComponent>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForSceneComponent(
				ObjBaseName,
				InputSceneComp,
				bInputNodesCanBeDeleted);

			if (bSuccess)
				OutCreatedNodeIds.Add(InInputObject->InputObjectNodeId);

			break;
		}

		case EHoudiniInputObjectType::StaticMeshComponent:
		{
			UHoudiniInputMeshComponent* InputSMC = Cast<UHoudiniInputMeshComponent>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForStaticMeshComponent(
				ObjBaseName,
				InputSMC,
				InInput->GetExportLODs(),
				InInput->GetExportSockets(),
				InInput->GetExportColliders(),
				InInput->GetKeepWorldTransform(),
				InInput->GetImportAsReference(),
				InInput->GetImportAsReferenceRotScaleEnabled(),
				InInput->GetImportAsReferenceBboxEnabled(),
				InInput->GetImportAsReferenceMaterialEnabled(),
				InActorTransform,
				bInputNodesCanBeDeleted,
				InInput->GetPreferNaniteFallbackMesh(),
				InInput->GetExportMaterialParameters());

			if (bSuccess)
				OutCreatedNodeIds.Add(InInputObject->InputObjectNodeId);

			break;
		}

		case EHoudiniInputObjectType::InstancedStaticMeshComponent:
		{
			UHoudiniInputInstancedMeshComponent* InputISMC = Cast<UHoudiniInputInstancedMeshComponent>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForInstancedStaticMeshComponent(
				ObjBaseName,
				InputISMC,
				InInput->GetExportLODs(),
				InInput->GetExportSockets(),
				InInput->GetExportColliders(),
				InInput->GetExportMaterialParameters(),
				bInputNodesCanBeDeleted);

			if (bSuccess)
				OutCreatedNodeIds.Add(InInputObject->InputObjectNodeId);

			break;
		}

		case EHoudiniInputObjectType::SplineComponent:
		{
			UHoudiniInputSplineComponent* InputSpline = Cast<UHoudiniInputSplineComponent>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForSplineComponent(
				ObjBaseName,
				InputSpline,
				InInput->GetUnrealSplineResolution(),
				InInput->IsUseLegacyInputCurvesEnabled(),
				bInputNodesCanBeDeleted);

			if (bSuccess)
				OutCreatedNodeIds.Add(InInputObject->InputObjectNodeId);

			break;
		}

		case EHoudiniInputObjectType::HoudiniSplineComponent:
		{
			UHoudiniInputHoudiniSplineComponent* InputCurve = Cast<UHoudiniInputHoudiniSplineComponent>(InInputObject);

			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForHoudiniSplineComponent(
				ObjBaseName,
				InputCurve,
				InInput->IsAddRotAndScaleAttributesEnabled(),
				InInput->IsUseLegacyInputCurvesEnabled());

			if (bSuccess)
				OutCreatedNodeIds.Add(InInputObject->InputObjectNodeId);

			break;
		}

		case EHoudiniInputObjectType::HoudiniAssetActor:
		case EHoudiniInputObjectType::HoudiniAssetComponent:
		{
			UHoudiniInputHoudiniAsset* InputHAC = Cast<UHoudiniInputHoudiniAsset>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForHoudiniAssetComponent(
				ObjBaseName,
				InputHAC,
				InInput->GetKeepWorldTransform(),
				InInput->GetImportAsReference(),
				InInput->GetImportAsReferenceRotScaleEnabled());

			if (bSuccess)
				OutCreatedNodeIds.Add(InInputObject->InputObjectNodeId);

			break;
		}

		case EHoudiniInputObjectType::Actor:
		case EHoudiniInputObjectType::GeometryCollectionActor_Deprecated:
		{			
			UHoudiniInputActor* InputActor = Cast<UHoudiniInputActor>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForActor(
				InInput,
				InputActor,
				InActorTransform,
				OutCreatedNodeIds,
				bInputNodesCanBeDeleted);

			break;
		}

		case EHoudiniInputObjectType::Landscape:
		{
			UHoudiniInputLandscape* InputLandscape = Cast<UHoudiniInputLandscape>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForLandscape(
				ObjBaseName,
				InputLandscape,
				InInput);

			if (bSuccess)
				OutCreatedNodeIds.Add(InInputObject->InputObjectNodeId);

			break;
		}

		case EHoudiniInputObjectType::Brush:
		{
			UHoudiniInputBrush* InputBrush = Cast<UHoudiniInputBrush>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForBrush(
				ObjBaseName,
				InputBrush,
				InInput->GetBoundSelectorObjectArray(),
				InInput->GetExportMaterialParameters());

			if (bSuccess)
				OutCreatedNodeIds.Add(InInputObject->InputObjectNodeId);

			break;
		}

		case EHoudiniInputObjectType::CameraComponent:
		{
			UHoudiniInputCameraComponent* InputCamera = Cast<UHoudiniInputCameraComponent>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForCamera(
				ObjBaseName, InputCamera);

			if (bSuccess)
				OutCreatedNodeIds.Add(InInputObject->InputObjectNodeId);

			break;
		}

		case EHoudiniInputObjectType::DataTable:
		{
			UHoudiniInputDataTable* InputDT = Cast<UHoudiniInputDataTable>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForDataTable(
				ObjBaseName, InputDT);

			if (bSuccess)
				OutCreatedNodeIds.Add(InInputObject->InputObjectNodeId);

			break;
		}

		case EHoudiniInputObjectType::FoliageType_InstancedStaticMesh:
		{
			UHoudiniInputFoliageType_InstancedStaticMesh* const InputFoliageTypeSM = Cast<UHoudiniInputFoliageType_InstancedStaticMesh>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForFoliageType_InstancedStaticMesh(
				ObjBaseName,
				InputFoliageTypeSM,
				InInput->GetExportLODs(),
				InInput->GetExportSockets(),
				InInput->GetExportColliders(),
				InInput->GetExportMaterialParameters(),
				InInput->GetImportAsReference(),
				InInput->GetImportAsReferenceRotScaleEnabled(),
				InInput->GetImportAsReferenceBboxEnabled(),
				InInput->GetImportAsReferenceMaterialEnabled(),
				bInputNodesCanBeDeleted);

			if (bSuccess)
				OutCreatedNodeIds.Add(InInputObject->InputObjectNodeId);

			break;
		}

		case EHoudiniInputObjectType::Blueprint:
		{
			UHoudiniInputBlueprint* InputBP = Cast<UHoudiniInputBlueprint>(InInputObject);
			bSuccess = FHoudiniInputTranslator::HapiCreateInputNodeForBP(
				InInput, InputBP, OutCreatedNodeIds, bInputNodesCanBeDeleted);
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
	const bool bUseRefCountedInputSystem = FHoudiniEngineRuntimeUtils::IsRefCountedInputSystemEnabled();
	
	bool bSuccess = true;
	switch (InInputObject->Type)
	{
		case EHoudiniInputObjectType::StaticMesh:
		{
			// Simply update the Input mesh's Transform offset
			if (!UpdateTransform(InInputObject->Transform, InInputObject->InputObjectNodeId))
				bSuccess = false;

			break;
		}

		
		case EHoudiniInputObjectType::StaticMeshComponent:
		case EHoudiniInputObjectType::SplineComponent:
		{
			// Default behaviour for components derived from SceneComponent.

			// Update using the component's transform
			UHoudiniInputSceneComponent* InComponent = Cast<UHoudiniInputSceneComponent>(InInputObject);
			if (!IsValid(InComponent))
			{
				bSuccess = false;
				break;
			}

			const USceneComponent* SceneComponent = InComponent->GetSceneComponent();
			const FTransform NewTransform = IsValid(SceneComponent) ? SceneComponent->GetComponentTransform() : InInputObject->Transform;
			if(!UpdateTransform(NewTransform, InInputObject->InputObjectNodeId))
				bSuccess = false;

			// Update the InputObject's transform
			InInputObject->Transform = NewTransform;

			break;
		}

		case EHoudiniInputObjectType::InstancedStaticMeshComponent:
		{
			// Update instanced static mesh components in the same way as static mesh component / scene components
			// Update using the component's transform
			UHoudiniInputSceneComponent* const InComponent = Cast<UHoudiniInputSceneComponent>(InInputObject);
			if (!IsValid(InComponent))
			{
				bSuccess = false;
				break;
			}

			USceneComponent const* const SceneComponent = InComponent->GetSceneComponent();
			const FTransform NewTransform = IsValid(SceneComponent) ? SceneComponent->GetComponentTransform() : InInputObject->Transform;
			if(!UpdateTransform(NewTransform, InInputObject->InputObjectNodeId))
				bSuccess = false;

			// Update the InputObject's transform
			InInputObject->Transform = NewTransform;
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
				InputActor->Transform = InputActor->GetActor()->GetTransform();

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
	
		case EHoudiniInputObjectType::SceneComponent:
		{
			UHoudiniInputSceneComponent* InputSceneComp = Cast<UHoudiniInputSceneComponent>(InInputObject);
			if (!IsValid(InputSceneComp))
			{
				bSuccess = false;
				break;
			}

			// Update the component transform to avoid further updates
			if (InputSceneComp->GetSceneComponent())
				InputSceneComp->Transform = InputSceneComp->GetSceneComponent()->GetComponentTransform();

			break;
		}

		case EHoudiniInputObjectType::Landscape:
		{
			//
			UHoudiniInputLandscape* InputLandscape = Cast<UHoudiniInputLandscape>(InInputObject);
			if (!IsValid(InputLandscape))
			{
				bSuccess = false;
				break;
			}

			// 
			ALandscapeProxy* Landscape = InputLandscape->GetLandscapeProxy();
			if (!IsValid(Landscape))
			{
				bSuccess = false;
				break;
			}

			// // Only apply diff for landscape since the HF's transform is used for value conversion as well
			// FTransform CurrentTransform = InputLandscape->Transform;
			// const FTransform NewTransform = Landscape->ActorToWorld();
			
			const FTransform NewTransform = FHoudiniEngineRuntimeUtils::CalculateHoudiniLandscapeTransform(Landscape);

			// Convert back to a HAPI Transform and update the HF's transform
			HAPI_TransformEuler NewHAPITransform;
			FHoudiniApi::TransformEuler_Init(&NewHAPITransform);
			// FHoudiniEngineUtils::TranslateUnrealTransform(HFTransform, NewHAPITransform);
			FHoudiniEngineUtils::TranslateUnrealTransform(
				FTransform(NewTransform.GetRotation(), NewTransform.GetTranslation(), FVector::OneVector), NewHAPITransform);
			// NewHAPITransform.position[1] = 0.0f;
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::SetObjectTransform(
				FHoudiniEngine::Get().GetSession(),
				InputLandscape->InputObjectNodeId, &NewHAPITransform))
			{
				bSuccess = false;
				break;
			}

			// Update the cached transform
			InputLandscape->Transform = NewTransform;
		}

		case EHoudiniInputObjectType::Brush:
		{
			// TODO: Update the Brush's transform
			break;
		}

		case EHoudiniInputObjectType::FoliageType_InstancedStaticMesh:
		{
			// Simply update the Input mesh's Transform offset
			if (!UpdateTransform(InInputObject->Transform, InInputObject->InputObjectNodeId))
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
		case EHoudiniInputObjectType::GeometryCollectionComponent:
		{
			// Simply update the Input mesh's Transform offset
			if (!UpdateTransform(InInputObject->Transform, InInputObject->InputObjectNodeId))
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
	if (!InObject)
		return false;
	
	UObject* Object = InObject->GetObject();
	if (!IsValid(Object))
		return true;

	FString NodeName = InObjNodeName + TEXT("_") + Object->GetName();

	// For UObjects we can't upload much, but can still create an input node
	// with a single point, with an attribute pointing to the input object's path
	HAPI_NodeId InputNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateInputNode(
		FHoudiniEngine::Get().GetSession(), &InputNodeId, TCHAR_TO_UTF8(*NodeName)), false);

	// Update this input object's NodeId and ObjectNodeId
	InObject->InputNodeId = (int32)InputNodeId;
	InObject->InputObjectNodeId = (int32)FHoudiniEngineUtils::HapiGetParentNodeId(InputNodeId);
	
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
		FVector3f ObjectPosition = (FVector3f)InObject->Transform.GetLocation();
		TArray<float> Position =
		{
			ObjectPosition.X * HAPI_UNREAL_SCALE_FACTOR_POSITION,
			ObjectPosition.Z * HAPI_UNREAL_SCALE_FACTOR_POSITION,
			ObjectPosition.Y * HAPI_UNREAL_SCALE_FACTOR_POSITION
		};

		// Now that we have raw positions, we can upload them for our attribute.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			Position, InputNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, AttributeInfoPoint), false);
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
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
			ObjectPathName, InputNodeId, 0, HAPI_UNREAL_ATTRIB_OBJECT_PATH, AttributeInfoPoint), false);
	}

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
		FHoudiniEngine::Get().GetSession(), InputNodeId), false);

	return true;
}

bool
FHoudiniInputTranslator::HapiSetGeoObjectTransform(const HAPI_NodeId& InObjectNodeId, const FTransform& InTransform)
{
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
	// Check that InOutObjectMergeNodeId is valid
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(InOutObjectMergeNodeId))
	{
		if (!bInCreateIfMissingInvalid)
			return false;

		// Create objmerge SOP in InOutGeoObjectNodeId
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniEngineUtils::CreateNode(InOutGeoObjectNodeId, TEXT("object_merge"), InObjNodeName, bCookOnCreation, &InOutObjectMergeNodeId), false);
	}

	// Set the objpath1 on the object merge
	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmNodeValue(Session, InOutObjectMergeNodeId, TCHAR_TO_UTF8(TEXT("objpath1")), InNodeToObjectMerge), false);
	/*
	// We shoudnt use WorldOrigin here, as it causes transform issues when merging into an input!
	const bool bUseRefCountedInputSystem = FHoudiniEngineRuntimeUtils::IsRefCountedInputSystemEnabled();
	if (bUseRefCountedInputSystem)
	{
		IUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get();
		if (!Manager)
		{
			HOUDINI_LOG_WARNING(TEXT("[FHoudiniInputTranslator::HapiCreateOrUpdateGeoObjectMergeAndSetTransform] Could not find input object manager."));
			return false;
		}
		const HAPI_NodeId WorldOriginNodeId = Manager->GetWorldOriginHAPINodeId();
		if (WorldOriginNodeId < 0)
		{
			HOUDINI_LOG_WARNING(TEXT("[FHoudiniInputTranslator::HapiCreateOrUpdateGeoObjectMergeAndSetTransform] Could not find/create world origin null."));
			return false;
		}
		// Set the transform value to "Into Specified Object"
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::SetParmIntValue(Session, InOutObjectMergeNodeId, TCHAR_TO_UTF8(TEXT("xformtype")), 0, 2), false);
		// Set the transform object to the world origin null from the manager
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::SetParmNodeValue(Session, InOutObjectMergeNodeId, TCHAR_TO_UTF8(TEXT("xformpath")), WorldOriginNodeId), false);
	}
	*/

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
	const bool& bExportLODs,
	const bool& bExportSockets,
	const bool& bExportColliders,
	const bool& bImportAsReference,
	const bool& bImportAsReferenceRotScaleEnabled,
	const bool& bImportAsReferenceBboxEnabled,
	const bool& bImportAsReferenceMaterialEnabled,
	const bool& bInputNodesCanBeDeleted,
	const bool& bPreferNaniteFallbackMesh,
	bool bExportMaterialParameters)
{
	if (!IsValid(InObject))
		return false;

	// Get the StaticMesh
	UStaticMesh* SM = InObject->GetStaticMesh();
	if (!IsValid(SM))
		return true;

	FString SMName = InObjNodeName + TEXT("_") + SM->GetName();

	// Marshall the Static Mesh to Houdini
	const bool bUseRefCountedInputSystem = FHoudiniEngineRuntimeUtils::IsRefCountedInputSystemEnabled();
	FUnrealObjectInputHandle InputNodeHandle;
	HAPI_NodeId CreatedNodeId = InObject->InputNodeId;
	
	bool bSuccess = true;
	if (bImportAsReference) 
	{
		FBox InBbox = bImportAsReferenceBboxEnabled ?
			SM->GetBoundingBox() :
			FBox(EForceInit::ForceInit);

		const TArray<FString>& MaterialReferences = bImportAsReferenceMaterialEnabled ?
			InObject->GetMaterialReferences() :
			TArray<FString>();

		bSuccess = FHoudiniInputTranslator::CreateInputNodeForReference(
			CreatedNodeId,
			SM,
			SMName,
			InObject->Transform,
			bImportAsReferenceRotScaleEnabled,
			bUseRefCountedInputSystem,
			InputNodeHandle,
			bInputNodesCanBeDeleted,
			bImportAsReferenceBboxEnabled,
			InBbox,
			bImportAsReferenceMaterialEnabled,
			MaterialReferences);
	}
	else 
	{	
		bSuccess = FUnrealMeshTranslator::HapiCreateInputNodeForStaticMesh(
			SM,
			CreatedNodeId, 
			SMName,
			InputNodeHandle,
			nullptr,
			bExportLODs,
			bExportSockets,
			bExportColliders,
			true,
			bInputNodesCanBeDeleted,
			bPreferNaniteFallbackMesh,
			bExportMaterialParameters);
	}

	InObject->SetImportAsReference(bImportAsReference);
	InObject->SetImportAsReferenceRotScaleEnabled(bImportAsReferenceRotScaleEnabled);
	InObject->SetImportAsReferenceBboxEnabled(bImportAsReferenceBboxEnabled);
	InObject->SetImportAsReferenceMaterialEnabled(bImportAsReferenceMaterialEnabled);

	InObject->InputNodeHandle = InputNodeHandle;
	if (bUseRefCountedInputSystem)
	{
		constexpr HAPI_NodeId ParentNodeId = -1;
		constexpr bool bCreateIfMissingInvalid = true;
		HAPI_NodeId SMNodeId = -1;
		if (!FHoudiniEngineUtils::GetHAPINodeId(InputNodeHandle, SMNodeId))
			return false;

		if (!HapiCreateOrUpdateGeoObjectMergeAndSetTransform(
				ParentNodeId, 
				SMNodeId,
				SMName,
				InObject->InputNodeId,
				InObject->InputObjectNodeId,
				bCreateIfMissingInvalid,
				InObject->Transform))
		{
			return false;
		}
	}
	else
	{
		// Update this input object's OBJ NodeId
		InObject->InputNodeId = CreatedNodeId;
		InObject->InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(InObject->InputNodeId);
		if (!HapiSetGeoObjectTransform(InObject->InputObjectNodeId, InObject->Transform))
			return false;
	}

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
FHoudiniInputTranslator::HapiCreateInputNodeForSkeletalMesh(
	const FString& InObjNodeName,
	UHoudiniInputSkeletalMesh* InObject,
	const bool& bImportAsReference,
	const bool& bImportAsReferenceRotScaleEnabled,
	const bool& bImportAsReferenceBboxEnabled,
	const bool& bImportAsReferenceMaterialEnabled,
	const bool& bInputNodesCanBeDeleted)
{
	if (!IsValid(InObject))
		return false;

	USkeletalMesh* SkelMesh = InObject->GetSkeletalMesh();
	if (!IsValid(SkelMesh))
		return true;

	FString SKName = InObjNodeName + TEXT("_") + SkelMesh->GetName();

	// Get the SM's transform offset
	FTransform TransformOffset = InObject->Transform;

	const bool bUseRefCountedInputSystem = FHoudiniEngineRuntimeUtils::IsRefCountedInputSystemEnabled();
	FUnrealObjectInputHandle InputNodeHandle;
	HAPI_NodeId CreatedNodeId = InObject->InputNodeId;

	// Marshall the SkeletalMesh to Houdini
	bool bSuccess = true;
	if (bImportAsReference)
	{
		// Get the SM's bbox
		FBox InBbox = bImportAsReferenceBboxEnabled ?
			SkelMesh->GetBounds().GetBox() :
			FBox(EForceInit::ForceInit);

		const TArray<FString>& MaterialReferences = bImportAsReferenceMaterialEnabled ?
			InObject->GetMaterialReferences() :
			TArray<FString>();

		bSuccess = FHoudiniInputTranslator::CreateInputNodeForReference(
			CreatedNodeId,
			SkelMesh,
			SKName,
			InObject->Transform,
			bImportAsReferenceRotScaleEnabled,
			bUseRefCountedInputSystem,
			InputNodeHandle,
			bInputNodesCanBeDeleted,
			bImportAsReferenceBboxEnabled,
			InBbox,
			bImportAsReferenceMaterialEnabled,
			MaterialReferences);
	}
	else
	{
		bSuccess = FUnrealMeshTranslator::HapiCreateInputNodeForSkeletalMesh(
			SkelMesh, CreatedNodeId, SKName, InputNodeHandle, nullptr, false, false, false, bInputNodesCanBeDeleted );
		if(!bSuccess)
		{
			return false;
		}

	}

	InObject->SetImportAsReference(bImportAsReference);
	InObject->SetImportAsReferenceRotScaleEnabled(bImportAsReferenceRotScaleEnabled);
	InObject->SetImportAsReferenceBboxEnabled(bImportAsReferenceBboxEnabled);
	InObject->SetImportAsReferenceMaterialEnabled(bImportAsReferenceMaterialEnabled);

	InObject->InputNodeHandle = InputNodeHandle;
	if (bUseRefCountedInputSystem)
	{
		constexpr HAPI_NodeId ParentNodeId = -1;
		constexpr bool bCreateIfMissingInvalid = true;
		HAPI_NodeId SKNodeId = -1;
		if (!FHoudiniEngineUtils::GetHAPINodeId(InputNodeHandle, SKNodeId))
			return false;

		if (!HapiCreateOrUpdateGeoObjectMergeAndSetTransform(
			ParentNodeId,
			SKNodeId,
			SKName,
			InObject->InputNodeId,
			InObject->InputObjectNodeId,
			bCreateIfMissingInvalid,
			InObject->Transform))
		{
			return false;
		}
	}
	else
	{
		// Update this input object's OBJ NodeId
		InObject->InputNodeId = CreatedNodeId;
		InObject->InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(InObject->InputNodeId);
		if (!HapiSetGeoObjectTransform(InObject->InputObjectNodeId, InObject->Transform))
			return false;
	}

	return bSuccess;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForSkeletalMeshComponent(
	const FString& InObjNodeName,
	UHoudiniInputSkeletalMeshComponent* InObject,
	const bool& bKeepWorldTransform,
	const bool& bImportAsReference,
	const bool& bImportAsReferenceRotScaleEnabled,
	const bool& bImportAsReferenceBboxEnabled,
	const bool& bImportAsReferenceMaterialEnabled,
	const FTransform& InActorTransform,
	const bool& bInputNodesCanBeDeleted)
{
	if (!IsValid(InObject))
		return false;

	USkeletalMeshComponent* SKC = InObject->GetSkeletalMeshComponent();
	if (!IsValid(SKC))
		return true;

	// Get the component's Skeletal Mesh
	USkeletalMesh* SK = InObject->GetSkeletalMesh();
	if (!IsValid(SK))
		return true;

	bool bUseRefCountedInputSystem = FHoudiniEngineRuntimeUtils::IsRefCountedInputSystemEnabled();
	HAPI_NodeId CreatedNodeId = InObject->InputNodeId;

	// Marshall the Skeletal Mesh to Houdini
	FString SKCName = InObjNodeName + TEXT("_") + SKC->GetName();

	FUnrealObjectInputHandle InputNodeHandle;
	bool bSuccess = true;
	if (bImportAsReference)
	{
		FTransform ImportAsReferenceTransform = InObject->Transform;
		
		// Previously, ImportAsReferenceTransform was multiplied by
		// InActorTransform.Inverse() if bKeepWorldTransform was true,
		// but this created a double transform issue.
		ImportAsReferenceTransform.SetLocation(FVector::ZeroVector);

		// Get the SM's bbox
		FBox InBbox = bImportAsReferenceBboxEnabled ?
			SK->GetBounds().GetBox() :
			FBox(EForceInit::ForceInit);

		const TArray<FString>& MaterialReferences = bImportAsReferenceMaterialEnabled ?
			InObject->GetMaterialReferences() :
			TArray<FString>();

		bSuccess = FHoudiniInputTranslator::CreateInputNodeForReference(
			CreatedNodeId,
			SK,
			SKCName,
			ImportAsReferenceTransform,
			bImportAsReferenceRotScaleEnabled,
			bUseRefCountedInputSystem,
			InputNodeHandle,
			bInputNodesCanBeDeleted,
			bImportAsReferenceBboxEnabled,
			InBbox,
			bImportAsReferenceMaterialEnabled,
			MaterialReferences);
	}
	else
	{
		bSuccess = FUnrealMeshTranslator::HapiCreateInputNodeForSkeletalMesh(SK, CreatedNodeId, SKCName, InputNodeHandle, SKC);
	}

	InObject->SetImportAsReference(bImportAsReference);
	InObject->SetImportAsReferenceRotScaleEnabled(bImportAsReferenceRotScaleEnabled);
	InObject->SetImportAsReferenceBboxEnabled(bImportAsReferenceBboxEnabled);
	InObject->SetImportAsReferenceMaterialEnabled(bImportAsReferenceMaterialEnabled);

	// Create/update the node in the input manager
	if (bUseRefCountedInputSystem)
	{
		FUnrealObjectInputOptions Options(
			bImportAsReference, bImportAsReferenceRotScaleEnabled, false, false, false);
		constexpr bool bIsLeaf = false;
		FUnrealObjectInputIdentifier SKCIdentifier(SKC, Options, bIsLeaf);
		FUnrealObjectInputHandle Handle;
		if (!FHoudiniEngineUtils::CreateOrUpdateReferenceInputMergeNode(SKCIdentifier, { InputNodeHandle }, Handle, true, bInputNodesCanBeDeleted))
			return false;

		FHoudiniEngineUtils::GetHAPINodeId(Handle, CreatedNodeId);
		InObject->InputNodeHandle = Handle;
	}

	// Update this input object's OBJ NodeId
	InObject->InputNodeId = CreatedNodeId;
	InObject->InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(CreatedNodeId);

	// Update this input object's cache data
	InObject->Update(SKC);

	// Update the component's transform
	FTransform ComponentTransform = InObject->Transform * SKC->GetComponentTransform();
	if (!ComponentTransform.Equals(FTransform::Identity))
	{
		// convert to HAPI_Transform
		HAPI_TransformEuler HapiTransform;
		FHoudiniApi::TransformEuler_Init(&HapiTransform);
		FHoudiniEngineUtils::TranslateUnrealTransform(ComponentTransform, HapiTransform);

		// Set the transform on the OBJ parent
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
			FHoudiniEngine::Get().GetSession(), InObject->InputObjectNodeId, &HapiTransform), false);
	}

	return bSuccess;
}

bool 
FHoudiniInputTranslator::HapiCreateInputNodeForGeometryCollection(
	const FString& InObjNodeName,
	UHoudiniInputGeometryCollection* InObject, 
	const bool& bImportAsReference,
	const bool& bImportAsReferenceRotScaleEnabled,
	const bool& bImportAsReferenceBboxEnabled,
	const bool& bImportAsReferenceMaterialEnabled,
	bool bExportMaterialParameters,
	const bool& bInputNodesCanBeDeleted)
{
	if (!IsValid(InObject))
		return false;

	UGeometryCollection* GeometryCollection = InObject->GetGeometryCollection();
	if (!IsValid(GeometryCollection))
		return false;

	FString GCName = InObjNodeName + TEXT("_") + GeometryCollection->GetName();

	// TODO: Add support for the new input sytem!
	const bool bUseRefCountedInputSystem = FHoudiniEngineRuntimeUtils::IsRefCountedInputSystemEnabled();
	FUnrealObjectInputHandle InputNodeHandle;
	HAPI_NodeId CreatedNodeId = InObject->InputNodeId;

	// Marshall the GeometryCollection to Houdini
	bool bSuccess = true;
	if (bImportAsReference) 
	{
		TManagedArray<FBox>& BboxArray = GeometryCollection->GetGeometryCollection()->BoundingBox;
		FBox InBbox = FBox(EForceInit::ForceInitToZero);
		if (bImportAsReferenceBboxEnabled)
		{
			for (FBox& Bbox : BboxArray)
				InBbox += Bbox;
		}

		const TArray<FString>& MaterialReferences = bImportAsReferenceMaterialEnabled ?
			InObject->GetMaterialReferences() :
			TArray<FString>();

		bSuccess = FHoudiniInputTranslator::CreateInputNodeForReference(
			CreatedNodeId,
			GeometryCollection,
			GCName,
			InObject->Transform,
			bImportAsReferenceRotScaleEnabled,
			bUseRefCountedInputSystem,
			InputNodeHandle,
			bInputNodesCanBeDeleted,
			bImportAsReferenceBboxEnabled,
			InBbox,
			bImportAsReferenceMaterialEnabled,
			MaterialReferences);
	}
	else 
	{
		bSuccess = FUnrealGeometryCollectionTranslator::HapiCreateInputNodeForGeometryCollection(
			GeometryCollection,
			CreatedNodeId,
			GCName,
			InputNodeHandle,
			bExportMaterialParameters,
			nullptr,
			bInputNodesCanBeDeleted);
	}
	
	InObject->InputNodeHandle = InputNodeHandle;
	InObject->SetImportAsReference(bImportAsReference);
	InObject->SetImportAsReferenceRotScaleEnabled(bImportAsReferenceRotScaleEnabled);
	InObject->SetImportAsReferenceBboxEnabled(bImportAsReferenceBboxEnabled);
	InObject->SetImportAsReferenceMaterialEnabled(bImportAsReferenceMaterialEnabled);

	if (bUseRefCountedInputSystem)
	{
		constexpr HAPI_NodeId ParentNodeId = -1;
		constexpr bool bCreateIfMissingInvalid = true;
		HAPI_NodeId GCNodeId = -1;
		if (!FHoudiniEngineUtils::GetHAPINodeId(InputNodeHandle, GCNodeId))
			return false;

		if (!HapiCreateOrUpdateGeoObjectMergeAndSetTransform(
			ParentNodeId, 
			GCNodeId,
			GCName,
			InObject->InputNodeId,
			InObject->InputObjectNodeId,
			bCreateIfMissingInvalid,
			InObject->Transform))
		{
			return false;
		}
	}
	else
	{
		// Update this input object's OBJ NodeId
		InObject->InputNodeId = CreatedNodeId;
		InObject->InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(InObject->InputNodeId);
		if (!HapiSetGeoObjectTransform(InObject->InputObjectNodeId, InObject->Transform))
			return false;
	}

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
	const bool& bKeepWorldTransform,
	const bool& bImportAsReference,
	const bool& bImportAsReferenceRotScaleEnabled,
	const bool& bImportAsReferenceBboxEnabled,
	const bool& bImportAsReferenceMaterialEnabled,
	bool bExportMaterialParameters,
	const FTransform& InActorTransform,
	const bool& bInputNodesCanBeDeleted)
{
	if (!IsValid(InObject))
		return false;

	UGeometryCollectionComponent* GCC = InObject->GetGeometryCollectionComponent();
	if (!IsValid(GCC))
		return true;

	// Get the component's GeometryCollection
	UGeometryCollection* GC = InObject->GetGeometryCollection();
	if (!IsValid(GC))
		return true;

	bool bUseRefCountedInputSystem = FHoudiniEngineRuntimeUtils::IsRefCountedInputSystemEnabled();
	HAPI_NodeId CreatedNodeId = InObject->InputNodeId;
	
	// Marshall the GeometryCollection to Houdini
	FString GCCName = InObjNodeName + TEXT("_") + GCC->GetName();

	FUnrealObjectInputHandle InputNodeHandle;
	bool bSuccess = true;
	if (bImportAsReference) 
	{
		FTransform ImportAsReferenceTransform = InObject->Transform;
		
		// Previously, ImportAsReferenceTransform was multiplied by
		// InActorTransform.Inverse() if bKeepWorldTransform was true,
		// but this created a double transform issue.
		ImportAsReferenceTransform.SetLocation(FVector::ZeroVector);

		TManagedArray<FBox>& BboxArray = GC->GetGeometryCollection()->BoundingBox;
		FBox InBbox = FBox(EForceInit::ForceInitToZero);
		if (bImportAsReferenceBboxEnabled)
		{
			for (FBox& Bbox : BboxArray)
				InBbox += Bbox;
		}

		const TArray<FString>& MaterialReferences = bImportAsReferenceMaterialEnabled ?
			InObject->GetMaterialReferences() :
			TArray<FString>();

		bSuccess = FHoudiniInputTranslator::CreateInputNodeForReference(
			CreatedNodeId,
			GC,
			GCCName,
			ImportAsReferenceTransform,
			bImportAsReferenceRotScaleEnabled,
			bUseRefCountedInputSystem,
			InputNodeHandle,
			bInputNodesCanBeDeleted,
			bImportAsReferenceBboxEnabled,
			InBbox,
			bImportAsReferenceMaterialEnabled,
			MaterialReferences);
	}
	else 
	{
		bSuccess = FUnrealGeometryCollectionTranslator::HapiCreateInputNodeForGeometryCollection(
			GC,
			CreatedNodeId,
			GCCName,
			InputNodeHandle,
			bExportMaterialParameters,
			GCC,
			bInputNodesCanBeDeleted);
	}
	
	InObject->SetImportAsReference(bImportAsReference);
	InObject->SetImportAsReferenceRotScaleEnabled(bImportAsReferenceRotScaleEnabled);
	InObject->SetImportAsReferenceBboxEnabled(bImportAsReferenceBboxEnabled);
	InObject->SetImportAsReferenceMaterialEnabled(bImportAsReferenceMaterialEnabled);

	// Create/update the node in the input manager
	if (bUseRefCountedInputSystem)
	{
		FUnrealObjectInputOptions Options(
			bImportAsReference, bImportAsReferenceRotScaleEnabled, false, false, false);
		constexpr bool bIsLeaf = false;
		FUnrealObjectInputIdentifier GCCIdentifier(GCC, Options, bIsLeaf);
		FUnrealObjectInputHandle Handle;
		if (!FHoudiniEngineUtils::CreateOrUpdateReferenceInputMergeNode(GCCIdentifier, { InputNodeHandle }, Handle, true, bInputNodesCanBeDeleted))
			return false;

		FHoudiniEngineUtils::GetHAPINodeId(Handle, CreatedNodeId);
		InObject->InputNodeHandle = Handle;
	}
	
	// Update this input object's OBJ NodeId
	InObject->InputNodeId = CreatedNodeId;
	InObject->InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(CreatedNodeId);

	// Update this input object's cache data
	InObject->Update(GCC);

	// Update the component's transform
	FTransform ComponentTransform = InObject->Transform * GCC->GetComponentTransform();
	if (!ComponentTransform.Equals(FTransform::Identity))
	{
		// convert to HAPI_Transform
		HAPI_TransformEuler HapiTransform;
		FHoudiniApi::TransformEuler_Init(&HapiTransform);
		FHoudiniEngineUtils::TranslateUnrealTransform(ComponentTransform, HapiTransform);

		// Set the transform on the OBJ parent
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
			FHoudiniEngine::Get().GetSession(), InObject->InputObjectNodeId, &HapiTransform), false);
	}

	return bSuccess;
}


bool
FHoudiniInputTranslator::HapiCreateInputNodeForSceneComponent(
	const FString& InObjNodeName,
	UHoudiniInputSceneComponent* InObject,
	const bool& bInputNodesCanBeDeleted)
{
	if (!IsValid(InObject))
		return false;

	USceneComponent* SceneComp = InObject->GetSceneComponent();
	if (!IsValid(SceneComp))
		return true;

	// Get the Scene Component's transform
	FTransform TransformOffset = InObject->Transform;

	// Get the parent Actor's transform
	FTransform ParentTransform = InObject->ActorTransform;

	// Dont do that!
	return false;

	// TODO
	// Support this type of input object
	return HapiCreateInputNodeForObject(InObjNodeName, InObject);
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForStaticMeshComponent(
	const FString& InObjNodeName, 
	UHoudiniInputMeshComponent* InObject,
	const bool& bExportLODs,
	const bool& bExportSockets,
	const bool& bExportColliders,
	const bool& bKeepWorldTransform,
	const bool& bImportAsReference,
	const bool& bImportAsReferenceRotScaleEnabled,
	const bool& bImportAsReferenceBboxEnabled,
	const bool& bImportAsReferenceMaterialEnabled,
	const FTransform& InActorTransform,
	const bool& bInputNodesCanBeDeleted,
	const bool& bPreferNaniteFallbackMesh,
	bool bExportMaterialParameters)
{
	if (!IsValid(InObject))
		return false;

	UStaticMeshComponent* SMC = InObject->GetStaticMeshComponent();

	if (!IsValid(SMC))
		return true;

	// Get the component's Static Mesh
	UStaticMesh* SM = SMC->GetStaticMesh();
	if (!IsValid(SM))
		return true;

	const bool bUseRefCountedInputSystem = FHoudiniEngineRuntimeUtils::IsRefCountedInputSystemEnabled();
	HAPI_NodeId CreatedNodeId = InObject->InputNodeId;

	// Marshall the Static Mesh to Houdini
	FString SMCName = InObjNodeName + TEXT("_") + SMC->GetName();

	FUnrealObjectInputHandle InputNodeHandle;
	bool bSuccess = true;
	if (bImportAsReference) 
	{
		FTransform ImportAsReferenceTransform = InObject->Transform;

		// Previously, ImportAsReferenceTransform was multiplied by
		// InActorTransform.Inverse() if bKeepWorldTransform was true,
		// but this created a double transform issue.
		ImportAsReferenceTransform.SetLocation(FVector::ZeroVector);

		FBox InBbox = bImportAsReferenceBboxEnabled ?
			SM->GetBoundingBox() :
			FBox(EForceInit::ForceInit);

		const TArray<FString>& MaterialReferences = bImportAsReferenceMaterialEnabled ?
			InObject->GetMaterialReferences() :
			TArray<FString>();

		bSuccess = FHoudiniInputTranslator::CreateInputNodeForReference(
			CreatedNodeId,
			SM,
			SMCName,
			ImportAsReferenceTransform,
			bImportAsReferenceRotScaleEnabled,
			bUseRefCountedInputSystem,
			InputNodeHandle,
			true,
			bImportAsReferenceBboxEnabled,
			InBbox,
			bImportAsReferenceMaterialEnabled,
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
			bExportLODs, 
			bExportSockets, 
			bExportColliders,
			true, 
			bInputNodesCanBeDeleted, 
			bPreferNaniteFallbackMesh,
			bExportMaterialParameters);
	}

	InObject->SetImportAsReference(bImportAsReference);
	InObject->SetImportAsReferenceRotScaleEnabled(bImportAsReferenceRotScaleEnabled);
	InObject->SetImportAsReferenceBboxEnabled(bImportAsReferenceBboxEnabled);
	InObject->SetImportAsReferenceMaterialEnabled(bImportAsReferenceMaterialEnabled);

	// Create/update the node in the input manager
	if (bUseRefCountedInputSystem)
	{
		FUnrealObjectInputOptions Options(
			bImportAsReference, bImportAsReferenceRotScaleEnabled, bExportLODs, bExportSockets, bExportColliders);
		constexpr bool bIsLeaf = false;
		FUnrealObjectInputIdentifier SMCIdentifier(SMC, Options, bIsLeaf);
		FUnrealObjectInputHandle Handle;
		if (!FHoudiniEngineUtils::CreateOrUpdateReferenceInputMergeNode(SMCIdentifier, {InputNodeHandle}, Handle, true, bInputNodesCanBeDeleted))
			return false;
		
		FHoudiniEngineUtils::GetHAPINodeId(Handle, CreatedNodeId);
		InObject->InputNodeHandle = Handle;
	}
	
	// Update this input object's OBJ NodeId
	InObject->InputNodeId = CreatedNodeId;
	InObject->InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(InObject->InputNodeId);

	// Update this input object's cache data
	InObject->Update(SMC);

	// Update the component's transform
	FTransform ComponentTransform = InObject->Transform;
	if (!ComponentTransform.Equals(FTransform::Identity))
	{
		// convert to HAPI_Transform
		HAPI_TransformEuler HapiTransform;
		FHoudiniApi::TransformEuler_Init(&HapiTransform);
		FHoudiniEngineUtils::TranslateUnrealTransform(ComponentTransform, HapiTransform);

		// Set the transform on the OBJ parent
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
			FHoudiniEngine::Get().GetSession(), InObject->InputObjectNodeId, &HapiTransform), false);
	}

	return bSuccess;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForInstancedStaticMeshComponent(
	const FString& InObjNodeName,
	UHoudiniInputInstancedMeshComponent* InObject,
	const bool& bExportLODs,
	const bool& bExportSockets,
	const bool& bExportColliders,
	bool bExportMaterialParameters,
	const bool& bInputNodesCanBeDeleted)
{
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
		bExportLODs,
		bExportSockets,
		bExportColliders,
		false,
		bExportMaterialParameters,
		bInputNodesCanBeDeleted))
		return false;

	// Update this input object's node IDs
	InObject->InputNodeId = NewNodeId;
	InObject->InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(NewNodeId);
	InObject->InputNodeHandle = InputNodeHandle;

	// Update the component's cached instances
	InObject->Update(ISMC);

	// Update the component's transform
	const FTransform ComponentTransform = InObject->Transform;
	if (!ComponentTransform.Equals(FTransform::Identity))
	{
		// convert to HAPI_Transform
		HAPI_TransformEuler HapiTransform;
		FHoudiniApi::TransformEuler_Init(&HapiTransform);
		FHoudiniEngineUtils::TranslateUnrealTransform(ComponentTransform, HapiTransform);

		// Set the transform on the OBJ parent
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
			FHoudiniEngine::Get().GetSession(), InObject->InputObjectNodeId, &HapiTransform), false);
	}
	
	return true;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForSplineComponent(
	const FString& InObjNodeName,
	UHoudiniInputSplineComponent* InObject,
	const float& SplineResolution, 
	const bool& bInUseLegacyInputCurves,
	const bool& bInputNodesCanBeDeleted)
{
	if (!IsValid(InObject))
		return false;

	USplineComponent* Spline = InObject->GetSplineComponent();
	if (!IsValid(Spline))
		return true;

	int32 NumberOfSplineControlPoints = InObject->NumberOfSplineControlPoints;
	TArray<FTransform> SplineControlPoints = InObject->SplineControlPoints;

	FString SplineName = InObjNodeName + TEXT("_") + InObject->GetName();

	const bool bUseRefCountedInputSystem = FHoudiniEngineRuntimeUtils::IsRefCountedInputSystemEnabled();
	FUnrealObjectInputHandle InputNodeHandle;
	HAPI_NodeId CreatedNodeId = InObject->InputNodeId;

	if (!FUnrealSplineTranslator::CreateInputNodeForSplineComponent(
		Spline, CreatedNodeId, InputNodeHandle, SplineResolution, SplineName, bInUseLegacyInputCurves, bInputNodesCanBeDeleted))
		return false;

	// Cache the exported curve's data to the input object
	InObject->InputNodeId = CreatedNodeId;
	InObject->InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(CreatedNodeId);

	InObject->MarkChanged(true);

	// Update the component's cached data
	InObject->Update(Spline);

	// Update the component's transform
	if (bUseRefCountedInputSystem)
	{
		constexpr HAPI_NodeId ParentNodeId = -1;
		constexpr bool bCreateIfMissingInvalid = true;
		HAPI_NodeId SplineNodeId = -1;
		if (!FHoudiniEngineUtils::GetHAPINodeId(InputNodeHandle, SplineNodeId))
			return false;

		if (!HapiCreateOrUpdateGeoObjectMergeAndSetTransform(
			ParentNodeId,
			SplineNodeId,
			SplineName,
			InObject->InputNodeId,
			InObject->InputObjectNodeId,
			bCreateIfMissingInvalid,
			InObject->Transform))
		{
			return false;
		}
	}
	else
	{
		// Update this input object's OBJ NodeId
		InObject->InputNodeId = CreatedNodeId;
		InObject->InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(InObject->InputNodeId);
		if (!HapiSetGeoObjectTransform(InObject->InputObjectNodeId, InObject->Transform))
			return false;
	}

	return true;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForHoudiniSplineComponent(
	const FString& InObjNodeName,
	UHoudiniInputHoudiniSplineComponent* InObject, 
	const bool& bInAddRotAndScaleAttributes, 
	const bool& bInUseLegacyInputCurves)
{
	if (!IsValid(InObject))
		return false;

	UHoudiniSplineComponent* Curve = InObject->GetCurveComponent();
	if (!IsValid(Curve))
		return true;

	Curve->SetIsLegacyInputCurve(bInUseLegacyInputCurves);

	if (!FHoudiniSplineTranslator::HapiUpdateNodeForHoudiniSplineComponent(Curve, bInAddRotAndScaleAttributes))
		return false;

	// See if the component needs it node Id invalidated
	//if (InObject->InputNodeId < 0)
	//	Curve->SetNodeId(InObject->InputNodeId);

	// Cache the exported curve's data to the input object
	InObject->InputNodeId = Curve->GetNodeId();
	InObject->InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(InObject->InputNodeId);

	//InObject->CurveType = Curve->GetCurveType();
	//InObject->CurveMethod = Curve->GetCurveMethod();
	//InObject->Reversed = Curve->IsReversed();
	InObject->Update(Curve);

	InObject->MarkChanged(true);

	return true;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForHoudiniAssetComponent(
		const FString& InObjNodeName,
		UHoudiniInputHoudiniAsset* InObject,
		const bool& bKeepWorldTransform,
		const bool& bImportAsReference,
		const bool& bImportAsReferenceRotScaleEnabled)
{
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
	if (InObject->InputNodeId > -1 && InObject->GetImportAsReference())
	{
		int32 PreviousInputNodeId = InObject->InputNodeId;
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

	InObject->SetImportAsReference(bImportAsReference);
	InObject->SetImportAsReferenceBboxEnabled(bImportAsReferenceRotScaleEnabled);

	// If this object is in an Asset input, we need to set the InputNodeId directly
	// to avoid creating extra merge nodes. World inputs should not do that!
	bool bIsAssetInput = HoudiniInput->GetInputType() == EHoudiniInputType::Asset;

	if (bImportAsReference) 
	{
		InObject->InputNodeId = -1;
		InObject->InputObjectNodeId = -1;

		if(bIsAssetInput)
			HoudiniInput->SetInputNodeId(-1);

		constexpr bool bUseRefCountedInputSystem = false;
		FUnrealObjectInputHandle InputNodeHandle;
		if (!FHoudiniInputTranslator::CreateInputNodeForReference(
				InObject->InputNodeId,
				InputHAC,
				InObject->GetName(),
				InObject->Transform,
				bImportAsReferenceRotScaleEnabled,
				bUseRefCountedInputSystem,
				InputNodeHandle)) // do not delete previous node if it was HAC
			return false;

		if (bIsAssetInput)
			HoudiniInput->SetInputNodeId(InObject->InputNodeId);
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

	if (!bImportAsReference)
	{
		if (bIsAssetInput)
			HoudiniInput->SetInputNodeId(InputHAC->GetAssetId());

		InObject->InputNodeId = InputHAC->GetAssetId();
	}

	InObject->InputObjectNodeId = InObject->InputNodeId;
	
	bool bReturn = InObject->InputNodeId > -1;

	if(bIsAssetInput)
		bReturn = FHoudiniInputTranslator::ConnectInputNode(HoudiniInput);

	return bReturn;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForActor(
	UHoudiniInput* InInput, 
	UHoudiniInputActor* InObject, 
	const FTransform & InActorTransform, 
	TArray<int32>& OutCreatedNodeIds, 
	const bool& bInputNodesCanBeDeleted)
{
	if (!IsValid(InInput))
		return false;

	if (!IsValid(InObject))
		return false;

	AActor* Actor = InObject->GetActor();
	if (!IsValid(Actor))
		return true;

	// Check if this is a world input and if this is a HoudiniAssetActor
	// If so we need to build static meshes for any proxy meshes
	if (InInput->GetInputType() == EHoudiniInputType::World && Actor->IsA<AHoudiniAssetActor>())
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
						InObject->Update(InputObject);
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
					InObject->Update(InputObject);
					TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
				}
			}
		}
	}

	// Now, commit all of this actor's component
	int32 ComponentIdx = 0;
	for (UHoudiniInputSceneComponent* CurComponent : InObject->GetActorComponents())
	{
		if(UploadHoudiniInputObject(InInput, CurComponent, InActorTransform, OutCreatedNodeIds, bInputNodesCanBeDeleted))
			ComponentIdx++;

		// If we're importing the actor as ref, add the level path / actor path attribute to the created nodes
		if (InInput->GetImportAsReference())
		{
			bool bNeedCommit = false;
			if (FHoudiniEngineUtils::AddLevelPathAttribute(CurComponent->InputNodeId, 0, Actor->GetLevel(), 1, HAPI_ATTROWNER_POINT))
				bNeedCommit = true;

			if(FHoudiniEngineUtils::AddActorPathAttribute(CurComponent->InputNodeId, 0, Actor, 1, HAPI_ATTROWNER_POINT))
				bNeedCommit = true;

			// Commit the geo if needed
			if(bNeedCommit)
				FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), CurComponent->InputNodeId);
		}
	}

	// Cache our transformn
	InObject->Transform = Actor->GetTransform();

	/*
	// TODO
	// Support this type of input object
	FString ObjNodeName = InInput->GetNodeBaseName();
	return HapiCreateInputNodeForObject(ObjNodeName, InObject);	
	*/

	//TODO? Check
	// return true if we have at least uploaded one component
	// return (ComponentIdx > 0);

	return true;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForBP(
	UHoudiniInput* InInput,
	UHoudiniInputBlueprint* InObject,
	TArray<int32>& OutCreatedNodeIds,
	const bool& bInputNodesCanBeDeleted)
{
	if (!IsValid(InInput))
		return false;

	if (!IsValid(InObject))
		return false;

	UBlueprint* BP = InObject->GetBlueprint();
	if (!IsValid(BP))
		return true;

	// Now, commit all of this BP's component
	int32 ComponentIdx = 0;
	for (UHoudiniInputSceneComponent* CurComponent : InObject->GetComponents())
	{
		if (UploadHoudiniInputObject(InInput, CurComponent, FTransform::Identity, OutCreatedNodeIds, bInputNodesCanBeDeleted))
			ComponentIdx++;
	}

	return true;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForLandscape(
	const FString& InObjNodeName,
	UHoudiniInputLandscape* InObject,
	UHoudiniInput* InInput)
{
	if (!IsValid(InObject))
		return false;

	if (!IsValid(InInput))
		return false;

	ALandscapeProxy* Landscape = InObject->GetLandscapeProxy();
	if (!IsValid(Landscape))
		return true;

	EHoudiniLandscapeExportType ExportType = InInput->GetLandscapeExportType();

	// Get selected components if bLandscapeExportSelectionOnly or bLandscapeAutoSelectComponent is true
	bool bExportSelectionOnly = InInput->bLandscapeExportSelectionOnly;
	bool bLandscapeAutoSelectComponent = InInput->bLandscapeAutoSelectComponent;

	TSet<ULandscapeComponent *> SelectedComponents = InInput->GetLandscapeSelectedComponents();
	if (bExportSelectionOnly && SelectedComponents.Num() == 0)
	{
		InInput->UpdateLandscapeInputSelection();
		SelectedComponents = InInput->GetLandscapeSelectedComponents();
	}
	
	bool bSucess = false;
	if (ExportType == EHoudiniLandscapeExportType::Heightfield)
	{
		// Ensure we destroy any (Houdini) input nodes before clobbering this object with a new heightfield.
		//DestroyInputNodes(InInput, InInput->GetInputType());
		
		int32 NumComponents = Landscape->LandscapeComponents.Num();
		if ( !bExportSelectionOnly || ( SelectedComponents.Num() == NumComponents ) )
		{
			// Export the whole landscape and its layer as a single heightfield node
			bSucess = FUnrealLandscapeTranslator::CreateHeightfieldFromLandscape(Landscape, InObject->InputNodeId, InObjNodeName);
		}
		else
		{
			// Each selected landscape component will be exported as separate volumes in a single heightfield
			bSucess = FUnrealLandscapeTranslator::CreateHeightfieldFromLandscapeComponentArray( Landscape, SelectedComponents, InObject->InputNodeId, InObjNodeName );
		}
	}
	else
	{
		bool bExportLighting = InInput->bLandscapeExportLighting;
		bool bExportMaterials = InInput->bLandscapeExportMaterials;
		bool bExportNormalizedUVs = InInput->bLandscapeExportNormalizedUVs;
		bool bExportTileUVs = InInput->bLandscapeExportTileUVs;
		bool bExportAsMesh = InInput->LandscapeExportType == EHoudiniLandscapeExportType::Mesh;

		bSucess = FUnrealLandscapeTranslator::CreateMeshOrPointsFromLandscape(
			Landscape, InObject->InputNodeId, InObjNodeName,
			bExportAsMesh, bExportTileUVs, bExportNormalizedUVs, bExportLighting, bExportMaterials);
	}

	// Update this input object's OBJ NodeId
	InObject->InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(InObject->InputNodeId);
	InObject->Update(Landscape);

	return bSucess;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForBrush(
	const FString& InObjNodeName,
	UHoudiniInputBrush* InObject,
	TArray<AActor*>* ExcludeActors,
	bool bExportMaterialParameters)
{
	if (!IsValid(InObject))
		return false;

	ABrush* BrushActor = InObject->GetBrush();
	if (!IsValid(BrushActor))
		return true;

	if (!FUnrealBrushTranslator::CreateInputNodeForBrush(InObject, BrushActor, ExcludeActors, InObject->InputNodeId, InObjNodeName, bExportMaterialParameters))
		return false;

	InObject->InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(InObject->InputNodeId);
	InObject->Update(BrushActor);

	return true;
}



bool
FHoudiniInputTranslator::HapiCreateInputNodeForCamera(const FString& InNodeName, UHoudiniInputCameraComponent* InInputObject)
{
	if (!IsValid(InInputObject))
		return false;

	UCameraComponent* Camera = InInputObject->GetCameraComponent();
	if (!IsValid(Camera))
		return true;

	FString NodeName = InNodeName + TEXT("_") + Camera->GetName();

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

	// Set the transform
	HAPI_TransformEuler H_Transform;
	FHoudiniApi::TransformEuler_Init(&H_Transform);
	FHoudiniEngineUtils::TranslateUnrealTransform(Camera->GetComponentTransform(), H_Transform);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
		FHoudiniEngine::Get().GetSession(), CameraNodeId, &H_Transform), false);
	
	// Update the component's transform
	FTransform ComponentTransform = InInputObject->Transform;
	if (!ComponentTransform.Equals(FTransform::Identity))
	{
		// convert to HAPI_Transform
		HAPI_TransformEuler HapiTransform;
		FHoudiniApi::TransformEuler_Init(&HapiTransform);
		FHoudiniEngineUtils::TranslateUnrealTransform(ComponentTransform, HapiTransform);

		// Camera orientation need to be adjusted
		HapiTransform.rotationEuler[1] += -90.0f;

		// Set the transform on the OBJ parent
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
			FHoudiniEngine::Get().GetSession(), CameraNodeId, &HapiTransform), false);
	}

	// Update this input's NodeId and ObjectNodeId
	InInputObject->InputNodeId = -1;// (int32)CameraNodeId;
	InInputObject->InputObjectNodeId = (int32)CameraNodeId;

	// Update this input object's cache data
	InInputObject->Update(Camera);

	return true;
}

bool
FHoudiniInputTranslator::UpdateLoadedInputs(UHoudiniAssetComponent* HAC)
{
	if (!IsValid(HAC))
		return false;

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

	if (InInput->GetInputType() != EHoudiniInputType::World)
		return false;

	TArray<UHoudiniInputObject*>* InputObjectsPtr = InInput->GetHoudiniInputObjectArray(EHoudiniInputType::World);
	if (!InputObjectsPtr)
		return false;

	bool bHasChanged = false;
	if (InInput->IsWorldInputBoundSelector() && InInput->GetWorldInputBoundSelectorAutoUpdates())
	{
		// If the input is in bound selector mode, and auto-update is enabled
		// update the actors selected by the bounds first
		bHasChanged = InInput->UpdateWorldSelectionFromBoundSelectors();
	}

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
			if ((ActorObject->InputNodeId > 0) || (ActorObject->InputObjectNodeId > 0))
			{
				ActorObject->InvalidateData();
				// We only need to update the input if the actors nodes were created in Houdini
				bHasChanged = true;
			}
			
			// Delete the Actor object
			ObjectToDeleteIndices.Add(InputObjIdx);
			continue;
		}

		// We'll keep track of whether the actor transform changed so that
		// we can mark all the components as having changed transforms -- everything
		// needs to be updated.
		bool bActorTransformChanged = false;
		if (ActorObject->HasActorTransformChanged())
		{
			ActorObject->MarkTransformChanged(true);
			bHasChanged = true;
			bActorTransformChanged = true;
		}	

		if (ActorObject->HasContentChanged())
		{
			ActorObject->MarkChanged(true);
			bHasChanged = true;
		}

		// Ensure we are aware of all the components of the actor
		ActorObject->Update(Actor);

		// Check if any components have content or transform changes
		for (auto CurActorComp : ActorObject->GetActorComponents())
		{
			if (bActorTransformChanged || CurActorComp->HasComponentTransformChanged())
			{
				CurActorComp->MarkTransformChanged(true);
				bHasChanged = true;
			}

			if (CurActorComp->HasComponentChanged())
			{
				CurActorComp->MarkChanged(true);
				bHasChanged = true;
			}

			USceneComponent* Component = CurActorComp->GetSceneComponent();
			if (IsValid(Component))
			{
				CurActorComp->Update(Component);
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

	// Mark the input as changed if need so it will trigger an upload
	if (bHasChanged)
		InInput->MarkChanged(true);

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
			FHoudiniEngine::Get().GetSession(), &NewNodeId, TCHAR_TO_UTF8(*InputNodeName)), false);
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
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			Position, NewNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, AttributeInfoPoint), false);
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
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			InputRotations, NewNodeId, 0, HAPI_UNREAL_ATTRIB_ROTATION, AttributeInfoRotation), false);

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
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			InputScales, NewNodeId, 0, HAPI_UNREAL_ATTRIB_SCALE, AttributeInfoScale), false);
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

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			BboxMin, NewNodeId, 0, HAPI_UNREAL_ATTRIB_BBOX_MIN, AttributeInfoBboxPoint), false);

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

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			BboxMax, NewNodeId, 0, HAPI_UNREAL_ATTRIB_BBOX_MAX, AttributeInfoBboxPoint), false);
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

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
				MaterialReferences[i], NewNodeId, 0, TCHAR_TO_ANSI(*AttributeName), AttributeInfoPoint), false);
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

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
			InRef, NewNodeId, 0, HAPI_UNREAL_ATTRIB_INSTANCE_OVERRIDE, AttributeInfoPoint), false);
	}

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
		FHoudiniEngine::Get().GetSession(), NewNodeId), false);

	InputNodeId = NewNodeId;
	return true;
}

bool FHoudiniInputTranslator::CreateInputNodeForReference(
	HAPI_NodeId& InputNodeId,
	UObject const* const InObjectToRef,
	const FString& InputNodeName,
	const FTransform& InTransform,
	const bool& bImportAsReferenceRotScaleEnabled,
	const bool bInUseRefCountedInputSystem,
	FUnrealObjectInputHandle& OutHandle,
	const bool& bInputNodesCanBeDeleted,
	const bool& bImportAsReferenceBboxEnabled,
	const FBox& InBbox,
	const bool& bImportAsReferenceMaterialEnabled,
	const TArray<FString>& MaterialReferences)
{
	// The identifier to the node in the input system
	FString FinalInputNodeName = InputNodeName;
	HAPI_NodeId ParentNodeId = -1;
	FUnrealObjectInputHandle ParentHandle;
	FUnrealObjectInputIdentifier Identifier;
	if (bInUseRefCountedInputSystem)
	{
		// Build the identifier for the entry in the manager
		constexpr bool bImportAsReference = true;
		constexpr bool bIsLeaf = true;
		FUnrealObjectInputOptions Options(bImportAsReference, bImportAsReferenceRotScaleEnabled);
		Identifier = FUnrealObjectInputIdentifier(InObjectToRef, Options, bIsLeaf);

		// If the entry exists in the manager, the associated HAPI nodes are valid, and it is not marked as dirty, then
		// return the existing entry
		FUnrealObjectInputHandle Handle;
		if (FHoudiniEngineUtils::NodeExistsAndIsNotDirty(Identifier, Handle))
		{
			HAPI_NodeId NodeId = -1;
			if (FHoudiniEngineUtils::GetHAPINodeId(Handle, NodeId))
			{
				// Make sure the node cant be deleted if needed
				if (!bInputNodesCanBeDeleted)
					FHoudiniEngineUtils::UpdateInputNodeCanBeDeleted(Handle, bInputNodesCanBeDeleted);

				OutHandle = Handle;
				InputNodeId = NodeId;
				return true;
			}
		}
		// If the entry does not exist, or is invalid, then we need create it
		FHoudiniEngineUtils::GetDefaultInputNodeName(Identifier, FinalInputNodeName);
		// Create any parent/container nodes that we would need, and get the node id of the immediate parent
		if (FHoudiniEngineUtils::EnsureParentsExist(Identifier, ParentHandle, bInputNodesCanBeDeleted) && ParentHandle.IsValid())
			FHoudiniEngineUtils::GetHAPINodeId(ParentHandle, ParentNodeId);
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
	
	if (bInUseRefCountedInputSystem)
	{
		// Record the node in the manager
		const HAPI_NodeId ObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(InputNodeId);
		FUnrealObjectInputHandle Handle;
		if (FHoudiniEngineUtils::AddNodeOrUpdateNode(Identifier, InputNodeId, Handle, ObjectNodeId, nullptr, bInputNodesCanBeDeleted))
			OutHandle = Handle;
	}

	return bSuccess;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForDataTable(const FString& InNodeName, UHoudiniInputDataTable* InInputObject)
{
	if (!IsValid(InInputObject))
		return false;

	UDataTable* DataTable = InInputObject->GetDataTable();
	if (!IsValid(DataTable))
		return true;
	
	const UScriptStruct* ScriptStruct = DataTable->GetRowStruct();
	if (!IsValid(ScriptStruct))
		return true;

	TArray<FName> RowNames = DataTable->GetRowNames();
	TArray<FString> ColTitles = DataTable->GetUniqueColumnTitles();
	TArray<FString> NiceNames = DataTable->GetColumnTitles();

	int32 NumRows = RowNames.Num();
	int32 NumAttributes = ColTitles.Num();
	if (NumRows <= 0 || NumAttributes <= 0)
		return true;

	// Create the input node
	FString NodeName = InNodeName + TEXT("_") + DataTable->GetName();
	HAPI_NodeId InputNodeId = -1;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateInputNode(
		FHoudiniEngine::Get().GetSession(), &InputNodeId, TCHAR_TO_UTF8(*NodeName)), false);

	// Update this input object's NodeId and ObjectNodeId
	InInputObject->InputNodeId = (int32)InputNodeId;
	InInputObject->InputObjectNodeId = (int32)FHoudiniEngineUtils::HapiGetParentNodeId(InputNodeId);

	// Create a part
	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);
	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = NumAttributes;
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.vertexCount = 0;
	Part.faceCount = 0;
	Part.pointCount = NumRows;
	Part.type = HAPI_PARTTYPE_MESH;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
		FHoudiniEngine::Get().GetSession(), InputNodeId, 0, &Part), false);

	{
		// Create point attribute info for P.
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = NumRows;
		AttributeInfoPoint.tupleSize = 3;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
			HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint), false);

		// Set the point's position
		TArray<float> Positions;
		Positions.SetNum(NumRows * 3);
		for (int32 RowIdx = 0; RowIdx < NumRows; RowIdx++)
		{
			Positions[RowIdx * 3] = 0.0f;
			Positions[RowIdx * 3 + 1] = (float)RowIdx;
			Positions[RowIdx * 3 + 2] = 0.0f;
		}

		// Now that we have raw positions, we can upload them for our attribute.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			Positions, InputNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, AttributeInfoPoint), false);
	}

	{
		// Create point attribute info for the path.
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = NumRows;
		AttributeInfoPoint.tupleSize = 1;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
			HAPI_UNREAL_ATTRIB_OBJECT_PATH, &AttributeInfoPoint), false);

		// Get the object path
		FString ObjectPathName = DataTable->GetPathName();

		// Set the point's path attribute
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
			ObjectPathName, InputNodeId, 0, HAPI_UNREAL_ATTRIB_OBJECT_PATH, AttributeInfoPoint), false);
	}

	{
		// Create point attribute info for data table RowTable class name
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = NumRows;
		AttributeInfoPoint.tupleSize = 1;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
			HAPI_UNREAL_ATTRIB_DATA_TABLE_ROWSTRUCT, &AttributeInfoPoint), false);

		// Get the object path
#if ENGINE_MINOR_VERSION < 1
		FString RowStructName = DataTable->RowStruct->GetPathName();
#else
		FString RowStructName = DataTable->GetRowStructPathName().ToString();
#endif

		// Set the point's path attribute
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
			RowStructName, InputNodeId, 0,
			HAPI_UNREAL_ATTRIB_DATA_TABLE_ROWSTRUCT, AttributeInfoPoint), false);
	}

	{
		// Manually set the attribute for the unique RowName.
		HAPI_AttributeInfo AttributeInfoPoint;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
		AttributeInfoPoint.count = NumRows;
		AttributeInfoPoint.tupleSize = 1;
		AttributeInfoPoint.exists = true;
		AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
		AttributeInfoPoint.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
			HAPI_UNREAL_ATTRIB_DATA_TABLE_ROWNAME, &AttributeInfoPoint), false);

		TArray<FString> Names;
		Names.Reserve(NumRows);
		for (auto KV : DataTable->GetRowMap())
		{
			Names.Add(KV.Key.ToString());
		}

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
			Names, InputNodeId, 0, HAPI_UNREAL_ATTRIB_DATA_TABLE_ROWNAME, AttributeInfoPoint), false);
	}

	// Now set the attributes values for each "point" of the data table
	uint32 ColIdx = 0;
	auto ColIter(ColTitles.CreateConstIterator());
	// Skip the name column.
	++ColIter;
	for (; ColIter; ++ColIter)
	{
		++ColIdx;
		FString ColName = *ColIter;
		FString NiceName = NiceNames[ColIdx];

		// Validate struct variable names
		NiceName = NiceName.Replace(TEXT(" "), TEXT("_"));
		NiceName = NiceName.Replace(TEXT(":"), TEXT("_"));

		FString CurAttrName = TEXT(HAPI_UNREAL_ATTRIB_DATA_TABLE_PREFIX) + FString::FromInt(ColIdx) + TEXT("_") + NiceName;
		if (FHoudiniEngineUtils::SanitizeHAPIVariableName(CurAttrName))
		{
			FString OldName = TEXT(HAPI_UNREAL_ATTRIB_DATA_TABLE_PREFIX) + FString::FromInt(ColIdx) + TEXT("_") + NiceName;
			HOUDINI_LOG_WARNING(TEXT("[HapiCreateInputNodeForDataTable]: Invalid row name (%s) renamed to %s."), *OldName, *CurAttrName);
		}

		FProperty* ColumnProp = ScriptStruct->FindPropertyByName(FName(*ColName));
		if (!ColumnProp)
		{
			HOUDINI_LOG_WARNING(TEXT("[HapiCreateInputNodeForDataTable]: Could not find property %s in struct %s."), *ColName, *ScriptStruct->GetFName().ToString());
			continue;
		}
		// Create a point attribute info
		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		AttributeInfo.count = NumRows;
		// Set tuple size depending on prop type.
		AttributeInfo.exists = true;
		AttributeInfo.owner = HAPI_ATTROWNER_POINT;
		// Set the storage type depending on prop type.
		AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;
		AttributeInfo.typeInfo = HAPI_ATTRIBUTE_TYPE_NONE;

		uint32 Offset = ColumnProp->GetOffset_ForInternal();
		TMap<FName, uint8*>::TConstIterator It = DataTable->GetRowMap().CreateConstIterator();

		// Identify the type of the Property
		bool bIsBoolProperty = ColumnProp->IsA<FBoolProperty>();
		bool bIsTextProperty = ColumnProp->IsA<FTextProperty>();

		// For numeric properties, we want to treat enum separately to send them as string and not ints
		FNumericProperty* NumProp = CastField<FNumericProperty>(ColumnProp);
		bool bIsNumericProperty = NumProp != nullptr ? !NumProp->IsEnum() : false;
		bool bIsEnumProperty = NumProp != nullptr ? NumProp->IsEnum() : false;

		// Struct props 
		FStructProperty* StructProp = CastField<FStructProperty>(ColumnProp);
		bool bIsStructProperty = StructProp != nullptr;

		// We need to get all values for that attribute
		if (bIsBoolProperty)
		{
			AttributeInfo.tupleSize = 1;
			AttributeInfo.storage = HAPI_STORAGETYPE_INT8;
			TArray<int8> Col = PopulateBoolArray(It, *CastField<FBoolProperty>(ColumnProp), NumRows, Offset);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
				TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeInt8Data(
				Col, InputNodeId, 0,
				CurAttrName, AttributeInfo), false);
		}
		// Text treated separately because the method used for string fallback
		// doesn't cleanly convert this
		else if (bIsTextProperty)
		{
			AttributeInfo.tupleSize = 1;
			AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
			TArray<FString> Col;
			Col.Reserve(NumRows);
			for (; It; ++It)
			{
				FText* TextValue = ColumnProp->ContainerPtrToValuePtr<FText>(It.Value());
				if (!TextValue)
				{
					HOUDINI_LOG_WARNING(TEXT("[HapiCreateInputNodeForDataTable]: Invalid transform value for property %s."), *ColumnProp->GetName());
					continue;
				}
				Col.Add(TextValue->ToString());
			}

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
				TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
				Col, InputNodeId, 0,
				CurAttrName, AttributeInfo), false);
		}
		else if (bIsNumericProperty && !bIsEnumProperty)
		{
			AttributeInfo.tupleSize = 1;
			if (NumProp->IsInteger())
			{
				if (NumProp->IsA<FByteProperty>())
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_UINT8;
					TArray<uint8> Col = PopulateTArray<uint8, FByteProperty>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeUInt8Data(
						Col, InputNodeId, 0,
						CurAttrName, AttributeInfo), false);
				}
				else if (NumProp->IsA<FInt16Property>())
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_INT16;
					TArray<int16> Col = PopulateTArray<int16, FInt16Property>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeInt16Data(
						Col, InputNodeId, 0,
						CurAttrName, AttributeInfo), false);
				}
				else if (NumProp->IsA<FInt8Property>())
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_INT8;
					TArray<int8> Col = PopulateTArray<int8, FInt8Property>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeInt8Data(
						Col, InputNodeId, 0,
						CurAttrName, AttributeInfo), false);
				}
				else if (NumProp->IsA<FIntProperty>())
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_INT;
					TArray<int32> Col = PopulateTArray<int32, FIntProperty>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeIntData(
						Col, InputNodeId, 0,
						CurAttrName, AttributeInfo), false);
				}
				// There is no uint16 datatype in Houdini, convert to int32
				else if (NumProp->IsA<FUInt16Property>())
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_INT;
					TArray<int32> Col = PopulateTArray<int32, FUInt16Property>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeUInt16Data(
						Col, InputNodeId, 0,
						CurAttrName, AttributeInfo), false);
				}
				// There is no uint32 datatype in Houdini, convert to int64
				else if (NumProp->IsA<FUInt32Property>())
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_INT64;
					TArray<int64> Col = PopulateTArray<int64, FUInt32Property>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeUIntData(
						Col, InputNodeId, 0,
						CurAttrName, AttributeInfo), false);
				}
				// There is no uint64 datatype in Houdini, since there is
				// no int128, just force a conversion to signed int64
				else if (NumProp->IsA<FUInt64Property>())
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_INT64;
					TArray<int64> Col = PopulateTArray<int64, FUInt64Property>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeUInt64Data(
						Col, InputNodeId, 0,
						CurAttrName, AttributeInfo), false);
				}
				// Default to signed int64.
				else
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_INT64;
					TArray<int64> Col = PopulateTArray<int64, FInt64Property>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeInt64Data(
						Col, InputNodeId, 0,
						CurAttrName, AttributeInfo), false);
				}
			}
			else if (NumProp->IsFloatingPoint())
			{
				if (NumProp->IsA<FFloatProperty>())
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT;
					TArray<float> Col = PopulateTArray<float, FFloatProperty>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
						Col, InputNodeId, 0,
						CurAttrName, AttributeInfo), false);
				}
				// Default to double precision floating point.
				else
				{
					AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT64;
					TArray<double> Col = PopulateTArray<double, FDoubleProperty>(It, NumRows, 1, Offset, ColumnProp->GetSize());

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeDoubleData(
						Col, InputNodeId, 0,
						CurAttrName, AttributeInfo), false);
				}
			}
			else
			{
				FString ext;
				FString typ = ColumnProp->GetCPPMacroType(ext);
				HOUDINI_LOG_WARNING(TEXT("[HapiCreateInputNodeForDataTable]: Unrecognized child of FNumericProperty: %s %s"), *typ, *ext);
				continue;
			}
		}
		else
		{
			if (bIsStructProperty)
			{
				FName StructName = StructProp->Struct->GetFName();

				int32 NumComponents = -1;
				HAPI_StorageType Storage = HAPI_STORAGETYPE_INVALID;
				TArray<uint32> Order;
				if (StructName == NAME_Vector4f || StructName == NAME_LinearColor)
				{
					NumComponents = 4;
					Storage = HAPI_STORAGETYPE_FLOAT;
				}
				else if (StructName == NAME_Vector4d || StructName == NAME_Vector4)
				{
					NumComponents = 4;
					Storage = HAPI_STORAGETYPE_FLOAT64;
				}
				else if (StructName == NAME_Vector3f)
				{
					NumComponents = 3;
					Storage = HAPI_STORAGETYPE_FLOAT;
				}
				else if (StructName == NAME_Rotator3f)
				{
					NumComponents = 3;
					Storage = HAPI_STORAGETYPE_FLOAT;
					// Roll, Pitch, Yaw
					Order = { 1, 0, 2 };
				}
				else if (StructName == NAME_Vector || StructName == NAME_Vector3d)
				{
					NumComponents = 3;
					Storage = HAPI_STORAGETYPE_FLOAT64;
				}
				else if (StructName == NAME_Rotator || StructName == NAME_Rotator3d)
				{
					NumComponents = 3;
					Storage = HAPI_STORAGETYPE_FLOAT64;
					// Roll, Pitch, Yaw
					Order = { 1, 0, 2 };
				}
				else if (StructName == NAME_Vector2f)
				{
					NumComponents = 2;
					Storage = HAPI_STORAGETYPE_FLOAT;
				}
				else if (StructName == NAME_Vector2d || StructName == NAME_Vector2D)
				{
					NumComponents = 2;
					Storage = HAPI_STORAGETYPE_FLOAT64;
				}
				else if (StructName == NAME_Color)
				{
					NumComponents = 4;
					Storage = HAPI_STORAGETYPE_UINT8;
					// RGBA
					Order = { 2, 1, 0, 3 };
				}
				else if (StructName == NAME_Transform || StructName == NAME_Transform3d)
				{
					FTransform3d* Transform;
					TArray<double> RotValues;
					TArray<double> ScaleValues;
					TArray<double> TransValues;
					RotValues.Reserve(4 * NumRows);
					ScaleValues.Reserve(3 * NumRows);
					TransValues.Reserve(3 * NumRows);
					FString RotName = CurAttrName + TEXT("_rotation");
					FString ScaleName = CurAttrName + TEXT("_scale");
					FString TransName = CurAttrName + TEXT("_translate");
					for (; It; ++It)
					{
						Transform = ColumnProp->ContainerPtrToValuePtr<FTransform3d>(It.Value());
						if (!Transform)
						{
							HOUDINI_LOG_WARNING(TEXT("[HapiCreateInputNodeForDataTable]: Invalid transform value for property %s."), *ColumnProp->GetName());
							continue;
						}
						const FQuat4d& Rot = Transform->GetRotation();
						RotValues.Add(Rot.X);
						RotValues.Add(Rot.Y);
						RotValues.Add(Rot.Z);
						RotValues.Add(Rot.W);
						const FVector& Scale = Transform->GetScale3D();
						ScaleValues.Add(Scale.X);
						ScaleValues.Add(Scale.Y);
						ScaleValues.Add(Scale.Z);
						const FVector& Trans = Transform->GetTranslation();
						TransValues.Add(Trans.X);
						TransValues.Add(Trans.Y);
						TransValues.Add(Trans.Z);
					}

					AttributeInfo.tupleSize = 4;
					AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT64;
					// Rotation
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*RotName), &AttributeInfo), false);

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeDoubleData(
						RotValues, InputNodeId, 0,
						RotName, AttributeInfo), false);

					AttributeInfo.tupleSize = 3;
					// Scale
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*ScaleName), &AttributeInfo), false);

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeDoubleData(
						ScaleValues, InputNodeId, 0,
						ScaleName, AttributeInfo), false);
					// Translate
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*TransName), &AttributeInfo), false);

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeDoubleData(
						TransValues, InputNodeId, 0,
						TransName, AttributeInfo), false);

					continue;
				}
				else if (StructName == NAME_Transform3f)
				{
					FTransform3f* Transform;
					TArray<float> RotValues;
					TArray<float> ScaleValues;
					TArray<float> TransValues;
					RotValues.Reserve(4 * NumRows);
					ScaleValues.Reserve(3 * NumRows);
					TransValues.Reserve(3 * NumRows);
					for (; It; ++It)
					{
						Transform = ColumnProp->ContainerPtrToValuePtr<FTransform3f>(It.Value());
						if (!Transform)
						{
							HOUDINI_LOG_WARNING(TEXT("[HapiCreateInputNodeForDataTable]: Invalid transform value for property %s."), *ColumnProp->GetName());
							continue;
						}
						const FQuat4f& Rot = Transform->GetRotation();
						RotValues.Add(Rot.X);
						RotValues.Add(Rot.Y);
						RotValues.Add(Rot.Z);
						RotValues.Add(Rot.W);
						const FVector3f& Scale = Transform->GetScale3D();
						ScaleValues.Add(Scale.X);
						ScaleValues.Add(Scale.Y);
						ScaleValues.Add(Scale.Z);
						const FVector3f& Trans = Transform->GetTranslation();
						TransValues.Add(Trans.X);
						TransValues.Add(Trans.Y);
						TransValues.Add(Trans.Z);
					}

					AttributeInfo.tupleSize = 4;
					AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT;
					// Rotation
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName + *TEXT("_rotation")), &AttributeInfo), false);

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
						RotValues, InputNodeId, 0,
						CurAttrName + TEXT("_rotation"), AttributeInfo), false);

					AttributeInfo.tupleSize = 3;
					// Scale
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName + *TEXT("_scale")), &AttributeInfo), false);

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
						ScaleValues, InputNodeId, 0,
						CurAttrName + TEXT("_scale"), AttributeInfo), false);
					// Translate
					HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
						FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
						TCHAR_TO_ANSI(*CurAttrName + *TEXT("_translate")), &AttributeInfo), false);

					HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
						TransValues, InputNodeId, 0,
						CurAttrName + TEXT("_translate"), AttributeInfo), false);

					continue;
				}

				if (NumComponents != -1 && Storage != HAPI_STORAGETYPE_INVALID)
				{
					AttributeInfo.tupleSize = NumComponents;
					AttributeInfo.storage = Storage;
					// Bytes
					if (Storage == HAPI_STORAGETYPE_UINT8)
					{
						TArray<uint8> Col = Order.Num() == 0 ? PopulateTArray<uint8, FByteProperty>(It, NumRows, NumComponents, Offset, 1) : PopulateTArray<uint8, FByteProperty>(It, NumRows, NumComponents, Offset, 1, Order);
						HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
							FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
							TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

						HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeUInt8Data(
							Col, InputNodeId, 0,
							CurAttrName, AttributeInfo), false);
						continue;
					}
					// Floats
					else if (Storage == HAPI_STORAGETYPE_FLOAT)
					{
						TArray<float> Col = Order.Num() == 0 ? PopulateTArray<float, FFloatProperty>(It, NumRows, NumComponents, Offset, 4) : PopulateTArray<float, FFloatProperty>(It, NumRows, NumComponents, Offset, 4, Order);
						HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
							FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
							TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

						HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
							Col, InputNodeId, 0,
							CurAttrName, AttributeInfo), false);
						continue;
					}
					// Doubles
					else if (Storage == HAPI_STORAGETYPE_FLOAT64)
					{
						TArray<double> Col = Order.Num() == 0 ? PopulateTArray<double, FDoubleProperty>(It, NumRows, NumComponents, Offset, 8) : PopulateTArray<double, FDoubleProperty>(It, NumRows, NumComponents, Offset, 8, Order);
						HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
							FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
							TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

						HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeDoubleData(
							Col, InputNodeId, 0,
							CurAttrName, AttributeInfo), false);
						continue;
					}
					// Something else
					else
					{
						HOUDINI_LOG_WARNING(TEXT("[HapiCreateInputNodeForDataTable]: Invalid component size %d for property %s."), ColumnProp->GetSize(), *ColumnProp->GetName());
						continue;
					}
				}
			}
			// Fallback to string if no match
			AttributeInfo.tupleSize = 1;
			AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
			TArray<FString> Col;
			Col.Reserve(NumRows);
			for (; It; ++It)
			{
				Col.Add(DataTableUtils::GetPropertyValueAsString(ColumnProp, It.Value(), EDataTableExportFlags()));
			}
			
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(), InputNodeId, 0,
				TCHAR_TO_ANSI(*CurAttrName), &AttributeInfo), false);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(Col, InputNodeId, 0,
				CurAttrName, AttributeInfo), false);
		}
	}

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
		FHoudiniEngine::Get().GetSession(), InputNodeId), false);

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
		FHoudiniEngine::Get().GetSession(), InputNodeId, nullptr), false);

	return true;
}

bool
FHoudiniInputTranslator::HapiCreateInputNodeForFoliageType_InstancedStaticMesh(
	const FString& InObjNodeName,
	UHoudiniInputFoliageType_InstancedStaticMesh* InObject,
	const bool& bExportLODs,
	const bool& bExportSockets,
	const bool& bExportColliders,
	bool bExportMaterialParameters,
	const bool& bImportAsReference,
	const bool& bImportAsReferenceRotScaleEnabled,
	const bool& bImportAsReferenceBboxEnabled,
	const bool& bImportAsReferenceMaterialEnabled,
	const bool& bInputNodesCanBeDeleted)
{
	if (!IsValid(InObject))
		return false;

	FString FTName = InObjNodeName + TEXT("_");

	UFoliageType_InstancedStaticMesh* FoliageType = Cast<UFoliageType_InstancedStaticMesh>(InObject->GetObject());
	if (!IsValid(FoliageType))
		return true;
	
	UStaticMesh* const SM = FoliageType->GetStaticMesh();
	if (!IsValid(SM))
		return true;

	FTName += FoliageType->GetName();

	// Marshall the Static Mesh to Houdini
	FUnrealObjectInputHandle InputNodeHandle;
	constexpr bool bUseRefCountedInputSystem = false;
	bool bSuccess = true;

	if (bImportAsReference) 
	{
		FBox InBbox = bImportAsReferenceBboxEnabled ? SM->GetBoundingBox() : FBox(EForceInit::ForceInit);

		const TArray<FString>& MaterialReferences = bImportAsReferenceMaterialEnabled ?
			InObject->GetMaterialReferences() :
			TArray<FString>();

		bSuccess = FUnrealFoliageTypeTranslator::CreateInputNodeForReference(
			InObject->InputNodeId,
			FoliageType,
			FTName,
			InObject->Transform,
			bImportAsReferenceRotScaleEnabled,
			bUseRefCountedInputSystem,
			InputNodeHandle,
			bInputNodesCanBeDeleted,
			bImportAsReferenceBboxEnabled,
			InBbox,
			bImportAsReferenceMaterialEnabled,
			MaterialReferences);
	}
	else 
	{
		bSuccess = FUnrealFoliageTypeTranslator::HapiCreateInputNodeForFoliageType_InstancedStaticMesh(
			FoliageType,
			InObject->InputNodeId,
			FTName,
			InputNodeHandle,
			bExportLODs,
			bExportSockets,
			bExportColliders,
			bExportMaterialParameters);
	}

	InObject->SetImportAsReference(bImportAsReference);
	InObject->SetImportAsReferenceRotScaleEnabled(bImportAsReferenceRotScaleEnabled);
	InObject->SetImportAsReferenceBboxEnabled(bImportAsReferenceBboxEnabled);
	InObject->SetImportAsReferenceMaterialEnabled(bImportAsReferenceMaterialEnabled);

	// Update this input object's OBJ NodeId
	InObject->InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(InObject->InputNodeId);
	InObject->InputNodeHandle = InputNodeHandle;

	// If the Input mesh has a Transform offset
	const FTransform TransformOffset = InObject->Transform;
	if (!TransformOffset.Equals(FTransform::Identity))
	{
		// Updating the Transform
		HAPI_TransformEuler HapiTransform;
		FHoudiniApi::TransformEuler_Init(&HapiTransform);
		FHoudiniEngineUtils::TranslateUnrealTransform(TransformOffset, HapiTransform);

		// Set the transform on the OBJ parent
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
			FHoudiniEngine::Get().GetSession(), InObject->InputObjectNodeId, &HapiTransform), false);
	}

	return bSuccess;
}

#undef LOCTEXT_NAMESPACE
