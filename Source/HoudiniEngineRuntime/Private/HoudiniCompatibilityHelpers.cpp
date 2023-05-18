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

#include "HoudiniCompatibilityHelpers.h"

#include "HoudiniPluginSerializationVersion.h"

#include "HoudiniInput.h"
#include "HoudiniGeoPartObject.h"

#include "HoudiniParameter.h"
#include "HoudiniParameterButton.h"
#include "HoudiniParameterButtonStrip.h"
#include "HoudiniParameterChoice.h"
#include "HoudiniParameterColor.h"
#include "HoudiniParameterFile.h"
#include "HoudiniParameterFloat.h"
#include "HoudiniParameterFolder.h"
#include "HoudiniParameterFolderList.h"
#include "HoudiniParameterInt.h"
#include "HoudiniParameterLabel.h"
#include "HoudiniParameterMultiParm.h"
#include "HoudiniParameterRamp.h"
#include "HoudiniParameterSeparator.h"
#include "HoudiniParameterString.h"
#include "HoudiniParameterToggle.h"
#include "HoudiniParameterFile.h"
#include "HoudiniParameterOperatorPath.h"

#include "HoudiniAssetComponent.h"
#include "HoudiniInstancedActorComponent.h"
#include "HoudiniMeshSplitInstancerComponent.h"
#include "HoudiniHandleComponent.h"

#include "Engine/StaticMesh.h"
#include "Components/SplineComponent.h"
#include "Landscape.h"
#include "EngineUtils.h" // for TActorIterator<>

#if WITH_EDITOR
	#include "Editor.h"
	#include "Editor/UnrealEdEngine.h"
	#include "UnrealEdGlobals.h"
#endif

// TODO:
// HoudiniInstancedActorComponent ?
// HoudiniMeshSplitInstancerComponent ?

UHoudiniAssetComponent_V1::UHoudiniAssetComponent_V1(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}


void
UHoudiniAssetComponent_V1::Serialize(FArchive & Ar)
{
	if (!Ar.IsLoading())
		return;

	//Super::Serialize(Ar);

	//Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);	

	// Serialize component flags.
	Ar << HoudiniAssetComponentFlagsPacked;
	
	// Serialize format version.
	uint32 HoudiniAssetComponentVersion = 0;//Ar.CustomVer(FHoudiniCustomSerializationVersion::GUID);
	Ar << HoudiniAssetComponentVersion;

	// ComponenState Enum, saved as uint8
	// 0 invalid
	// 1 None
	// 2 Instantiated
	// 3 BeingCooked
	// Serialize component state.
	uint8 ComponentState = 0;
	Ar << ComponentState;

	// Serialize scaling information and import axis.
	Ar << GeneratedGeometryScaleFactor;
	Ar << TransformScaleFactor;

	// ImportAxis Enum, saved as uint8
	// 0 unreal 1 Houdini
	//uint8 ImportAxis = 0;
	Ar << ImportAxis;

	// Serialize generated component GUID.
	Ar << ComponentGUID;

	// If component is in invalid state, we can skip the rest of serialization.
	//if (ComponentState == EHoudiniAssetComponentState_V1::Invalid)
	if (ComponentState == 0)
		return;

	// Serialize Houdini asset.
	Ar << HoudiniAsset;

	// If we are going into playmode, save asset id.
	// NOTE: bIsPlayModeActive_Unused is known to have been mistakenly saved to disk as ON,
	// the following fixes that case - should only happen once when first loading
	if (Ar.IsLoading() && bIsPlayModeActive_Unused)
	{
		//HAPI_NodeId TempId;
		int TempId;
		Ar << TempId;
		bIsPlayModeActive_Unused = false;
	}

	// Serialization of default preset.
	Ar << DefaultPresetBuffer;

	// Serialization of preset.
	{
		bool bPresetSaved = false;
		Ar << bPresetSaved;

		if (bPresetSaved)
		{
			Ar << PresetBuffer;
		}
	}

	// Serialize parameters.
	//SerializeParameters(Ar);
	{
		// We have to make sure that parameter are NOT saaved with an empty name, as this will cause UE to crash on load
		for (TMap<int, UHoudiniAssetParameter * >::TIterator IterParams(Parameters); IterParams; ++IterParams)
		{
			UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
			if (!IsValid(HoudiniAssetParameter))
				continue;

			if (HoudiniAssetParameter->GetFName() != NAME_None)
				continue;

			// Calling Rename with null parameters will make sure the parameter has a unique name
			HoudiniAssetParameter->Rename();
		}

		Ar << Parameters;
	}

	// Serialize parameters name map.
	if (HoudiniAssetComponentVersion >= VER_HOUDINI_ENGINE_COMPONENT_PARAMETER_NAME_MAP)
	{
		Ar << ParameterByName;
	}
	else
	{
		if (Ar.IsLoading())
		{
			ParameterByName.Empty();

			// Otherwise if we are loading an older serialization format, we can reconstruct parameters name map.
			for (TMap<int, UHoudiniAssetParameter *>::TIterator IterParams(Parameters); IterParams; ++IterParams)
			{
				UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
				if (IsValid(HoudiniAssetParameter))
					ParameterByName.Add(HoudiniAssetParameter->ParameterName, HoudiniAssetParameter);
			}
		}
	}

	// Serialize inputs.
	//SerializeInputs(Ar);
	{
		Ar << Inputs;

		/*
		if (Ar.IsLoading())
		{
			for (int32 InputIdx = 0; InputIdx < Inputs.Num(); ++InputIdx)
			{
				UHoudiniAssetInput_V1 * HoudiniAssetInput = Inputs[InputIdx];
				if (IsValid(HoudiniAssetInput))
					Inputs[InputIdx]->SetHoudiniAssetComponent(this);
			}
		}
		*/
	}

	// Serialize material replacements and material assignments.
	Ar << HoudiniAssetComponentMaterials;

	// Serialize geo parts and generated static meshes.
	Ar << StaticMeshes;
	Ar << StaticMeshComponents;

	// Serialize instance inputs (we do this after geometry loading as we might need it).
	//SerializeInstanceInputs(Ar);
	{
		//int32 HoudiniAssetComponentVersion = Ar.CustomVer(FHoudiniCustomSerializationVersion::GUID);
		if (HoudiniAssetComponentVersion > VER_HOUDINI_ENGINE_COMPONENT_PARAMETER_NAME_MAP)
		{
			Ar << InstanceInputs;
		}
		else
		{
			int32 InstanceInputCount = 0;
			Ar << InstanceInputCount;

			InstanceInputs.SetNum(InstanceInputCount);

			for (int32 InstanceInputIdx = 0; InstanceInputIdx < InstanceInputCount; ++InstanceInputIdx)
			{
				//HAPI_NodeId HoudiniInstanceInputKey = -1;
				int HoudiniInstanceInputKey = -1;

				Ar << HoudiniInstanceInputKey;
				Ar << InstanceInputs[InstanceInputIdx];
			}
		}
	}

	// Serialize curves.
	Ar << SplineComponents;

	// Serialize handles.
	Ar << HandleComponents;

	// Serialize downstream asset connections.
	Ar << DownstreamAssetConnections;

	// Serialize Landscape/GeoPart map
	if (HoudiniAssetComponentVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_LANDSCAPES)
	{
		Ar << LandscapeComponents;
	}

	if (HoudiniAssetComponentVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BAKENAME_OVERRIDE)
	{
		Ar << BakeNameOverrides;
	}

	//TArray<UPackage *> DirtyPackages;
	if (HoudiniAssetComponentVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_COOK_TEMP_PACKAGES)
	{
		TMap<FString, FString> SavedPackages;
		Ar << SavedPackages;
	}

	if (HoudiniAssetComponentVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_COOK_TEMP_PACKAGES_MESH_AND_LAYERS)
	{
		// Temporary Mesh Packages
		TMap<FHoudiniGeoPartObject_V1, FString> MeshPackages;
		Ar << MeshPackages;

		// Temporary Landscape Layers Packages
		TMap<FString, FHoudiniGeoPartObject_V1> LayerPackages;
		Ar << LayerPackages;
	}
}

uint32
GetTypeHash(const FHoudiniGeoPartObject_V1& HoudiniGeoPartObject)
{
	return HoudiniGeoPartObject.GetTypeHash();
}

FArchive &
operator<<(FArchive & Ar, FHoudiniGeoPartObject_V1& HoudiniGeoPartObject)
{
	HoudiniGeoPartObject.Serialize(Ar);
	return Ar;
}

uint32
FHoudiniGeoPartObject_V1::GetTypeHash() const
{
	int32 HashBuffer[4] = { ObjectId, GeoId, PartId, SplitId };
	int32 Hash = FCrc::MemCrc32((void *)&HashBuffer[0], sizeof(HashBuffer));
	return FCrc::StrCrc32(*SplitName, Hash);
}

bool
FHoudiniGeoPartObject_V1SortPredicate::operator()(const FHoudiniGeoPartObject_V1& A, const FHoudiniGeoPartObject_V1& B) const
{
	/*if (!A.IsValid() || !B.IsValid())
		return false;*/

	if (A.ObjectId == B.ObjectId)
	{
		if (A.GeoId == B.GeoId)
		{
			if (A.PartId == B.PartId)
				return A.SplitId < B.SplitId;
			else
				return A.PartId < B.PartId;
		}
		else
		{
			return A.GeoId < B.GeoId;
		}
	}

	return A.ObjectId < B.ObjectId;
}

void
FHoudiniGeoPartObject_V1::Serialize(FArchive & Ar)
{
	//Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	HoudiniGeoPartObjectVersion = VER_HOUDINI_PLUGIN_SERIALIZATION_AUTOMATIC_VERSION;
	Ar << HoudiniGeoPartObjectVersion;

	Ar << TransformMatrix;

	Ar << ObjectName;
	Ar << PartName;
	Ar << SplitName;

	// Serialize instancer material.
	if (HoudiniGeoPartObjectVersion >= VER_HOUDINI_ENGINE_GEOPARTOBJECT_INSTANCER_MATERIAL_NAME)
		Ar << InstancerMaterialName;

	// Serialize instancer attribute material.
	if (HoudiniGeoPartObjectVersion >= VER_HOUDINI_ENGINE_GEOPARTOBJECT_INSTANCER_ATTRIBUTE_MATERIAL_NAME)
		Ar << InstancerAttributeMaterialName;

	Ar << AssetId;
	Ar << ObjectId;
	Ar << GeoId;
	Ar << PartId;
	Ar << SplitId;

	Ar << HoudiniGeoPartObjectFlagsPacked;

	if (HoudiniGeoPartObjectVersion < VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BASE)
	{
		// Prior to this version the unused flags space was not zero-initialized, so 
		// zero them out now to prevent misinterpreting any 1s
		HoudiniGeoPartObjectFlagsPacked &= 0x3FFFF;
	}

	if (HoudiniGeoPartObjectVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_GEO_PART_NODE_PATH)
	{
		Ar << NodePath;
	}
}


FHoudiniGeoPartObject
FHoudiniGeoPartObject_V1::ConvertLegacyData()
{
	FHoudiniGeoPartObject NewHGPO;

	NewHGPO.AssetId = AssetId;
	// NewHGPO.AssetName;

	NewHGPO.ObjectId = ObjectId;
	NewHGPO.ObjectName = ObjectName;

	NewHGPO.GeoId = GeoId;

	NewHGPO.PartId = PartId;
	NewHGPO.PartName = PartName;
	NewHGPO.bHasCustomPartName = bHasCustomName;

	NewHGPO.SplitGroups.Add(SplitName);

	NewHGPO.TransformMatrix = TransformMatrix;
	NewHGPO.NodePath = NodePath;

	// NewHGPO.VolumeName;
	// NewHGPO.VolumeTileIndex;

	NewHGPO.bIsVisible = bIsVisible;
	NewHGPO.bIsEditable = bIsEditable;
	// NewHGPO.bIsTemplated;
	// NewHGPO.bIsInstanced;

	NewHGPO.bHasGeoChanged = bHasGeoChanged;
	// NewHGPO.bHasPartChanged;
	// NewHGPO.bHasTransformChanged;
	// NewHGPO.bHasMaterialsChanged;
	NewHGPO.bLoaded = true; //bIsLoaded;

	// Hamdle Part Type
	if (bIsCurve)
	{
		NewHGPO.Type = EHoudiniPartType::Curve;
		NewHGPO.InstancerType = EHoudiniInstancerType::Invalid;
	}
	else if (bIsVolume)
	{
		NewHGPO.Type = EHoudiniPartType::Volume;
		NewHGPO.InstancerType = EHoudiniInstancerType::Invalid;
	}
	else if (bIsInstancer)
	{
		NewHGPO.Type = EHoudiniPartType::Instancer;
		NewHGPO.InstancerType = EHoudiniInstancerType::ObjectInstancer;
	}
	else if (bIsPackedPrimitiveInstancer)
	{
		NewHGPO.Type = EHoudiniPartType::Instancer;
		NewHGPO.InstancerType = EHoudiniInstancerType::PackedPrimitive;
	}
	else
	{
		NewHGPO.Type = EHoudiniPartType::Mesh;
		NewHGPO.InstancerType = EHoudiniInstancerType::Invalid;
	}

	// Instancer specific flags
	if (NewHGPO.Type == EHoudiniPartType::Instancer)
	{
		//bInstancerMaterialAvailable
		//bInstancerAttributeMaterialAvailable
		//InstancerMaterialName
		//InstancerAttributeMaterialName
	}

	// Collision specific flags
	if (NewHGPO.Type == EHoudiniPartType::Mesh)
	{
		//bIsCollidable
		//bIsRenderCollidable
		//bIsUCXCollisionGeo
		//bIsSimpleCollisionGeo
		//bHasCollisionBeenAdded
		//bHasSocketBeenAdded
	}

	//bIsBox
	//bIsSphere

	if (NewHGPO.SplitGroups.Num() <= 0)
	{
		NewHGPO.SplitGroups.Add("main_geo");
	}

	return NewHGPO;
}

UHoudiniAssetInput::UHoudiniAssetInput(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}
void
UHoudiniAssetInput::Serialize(FArchive & Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	// Serialize current choice selection.
	// Enum serialized as uint8
	// 0 GeometryInput
	// 1 AssetInput
	// 2 CurveInput
	// 3 LandscapeInput
	// 4 WorldInput
	// 5 SkeletonInput
	//SerializeEnumeration< EHoudiniAssetInputType::Enum >(Ar, ChoiceIndex);
	Ar << ChoiceIndex;

	Ar << ChoiceStringValue;

	// We need these temporary variables for undo state tracking.
	bool bLocalInputAssetConnectedInHoudini = bInputAssetConnectedInHoudini;
	UHoudiniAssetComponent_V1 * LocalInputAssetComponent = InputAssetComponent;

	Ar << HoudiniAssetInputFlagsPacked;

	// Serialize input index.
	Ar << InputIndex;

	// Serialize input objects (if it's assigned).
	if (HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_MULTI_GEO_INPUT)
	{
		Ar << InputObjects;
	}
	else
	{
		UObject* InputObject = nullptr;
		Ar << InputObject;
		InputObjects.Empty();
		InputObjects.Add(InputObject);
	}

	// Serialize input asset.
	Ar << InputAssetComponent;

	// Serialize curve and curve parameters (if we have those).
	Ar << InputCurve;
	Ar << InputCurveParameters;

	// Serialize landscape used for input.
	if (HoudiniAssetParameterVersion >= VER_HOUDINI_ENGINE_PARAM_LANDSCAPE_INPUT)
	{
		if (HoudiniAssetParameterVersion < VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_INPUT_SOFT_REF)
		{
			ALandscapeProxy* InputLandscapePtr = nullptr;
			Ar << InputLandscapePtr;

			InputLandscapeProxy = InputLandscapePtr;
		}
		else
		{
			Ar << InputLandscapeProxy;
		}

	}

	// Serialize world outliner inputs.
	if (HoudiniAssetParameterVersion >= VER_HOUDINI_ENGINE_PARAM_WORLD_OUTLINER_INPUT)
	{
		Ar << InputOutlinerMeshArray;
	}

	// Create necessary widget resources.
	bLoadedParameter = true;
	// If we're loading for real for the first time we need to reset this
	// flag so we can reconnect when we get our parameters uploaded.
	bInputAssetConnectedInHoudini = false;

	if (HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_UNREAL_SPLINE_RESOLUTION_PER_INPUT)
		Ar << UnrealSplineResolution;

	if (HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_GEOMETRY_INPUT_TRANSFORMS)
	{
		Ar << InputTransforms;
	}
	else
	{
		InputTransforms.SetNum(InputObjects.Num());
		for (int32 n = 0; n < InputTransforms.Num(); n++)
			InputTransforms[n] = FTransform::Identity;
	}

	if ((HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_INPUT_LANDSCAPE_TRANSFORM)
		&& (HoudiniAssetParameterVersion != VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_419_SERIALIZATION_FIX))
		Ar << InputLandscapeTransform;
}

UHoudiniInput*
UHoudiniAssetInput::ConvertLegacyInput(UObject* InOuter)
{
	UHoudiniInput* Input = NewObject<UHoudiniInput>(
		InOuter, UHoudiniInput::StaticClass(), FName(*ParameterLabel), RF_Transactional);

	EHoudiniInputType InputType = EHoudiniInputType::Invalid;
	if (ChoiceIndex == 0)
		InputType = EHoudiniInputType::Geometry;
	else if (ChoiceIndex == 1)
		InputType = EHoudiniInputType::Asset;
	else if (ChoiceIndex == 2)
		InputType = EHoudiniInputType::Curve;
	else if (ChoiceIndex == 3)
		InputType = EHoudiniInputType::Landscape;
	else if (ChoiceIndex == 4)
		InputType = EHoudiniInputType::World;
	else if (ChoiceIndex == 5)
	{
		//InputType = EHoudiniInputType::Skeletal;
		InputType = EHoudiniInputType::Invalid;
	}
	else
	{
		// Invalid
		InputType = EHoudiniInputType::Invalid;
	}

	bool bBlueprintStructureModified = false;
	Input->SetInputType(InputType, bBlueprintStructureModified);

	Input->SetExportColliders(false);
	Input->SetExportLODs(bExportAllLODs);
	Input->SetExportSockets(bExportSockets);
	Input->SetCookOnCurveChange(true);	

	// If KeepWorldTransform is set to 2, use the default value
	if (bKeepWorldTransform == 2)
		Input->SetKeepWorldTransform((bool)Input->GetDefaultXTransformType());
	else
		Input->SetKeepWorldTransform((bool)bKeepWorldTransform);

	Input->SetUnrealSplineResolution(UnrealSplineResolution);
	Input->SetPackBeforeMerge(bPackBeforeMerge);
	if(bIsObjectPathParameter)
		Input->SetObjectPathParameter(ParmId);
	else
		Input->SetSOPInput(InputIndex);

	Input->SetImportAsReference(false);
	Input->SetHelp(ParameterHelp);
	//Input->SetInputNodeId(-1);

	if (bLandscapeExportAsHeightfield)
		Input->SetLandscapeExportType(EHoudiniLandscapeExportType::Heightfield);
	else if (bLandscapeExportAsMesh)
		Input->SetLandscapeExportType(EHoudiniLandscapeExportType::Mesh);
	else
		Input->SetLandscapeExportType(EHoudiniLandscapeExportType::Points);

	Input->SetLabel(ParameterLabel);
	Input->SetName(ParameterName);
	Input->SetUpdateInputLandscape(bUpdateInputLandscape);

	if (InputType == EHoudiniInputType::Geometry)
	{
		// Get the geo input object array
		bool bNeedToEmpty = true;
		TArray<UHoudiniInputObject*>* GeoInputObjectsPtr = Input->GetHoudiniInputObjectArray(InputType);
		if (ensure(GeoInputObjectsPtr))
		{
			// Add the geometry input objects
			for (int32 AtIndex = 0; AtIndex < InputObjects.Num(); AtIndex++)
			{
				// Create a new InputObject wrapper
				UObject* CurObject = InputObjects[AtIndex];
				if (!IsValid(CurObject))
					continue;

				UHoudiniInputObject* NewInputObject = UHoudiniInputObject::CreateTypedInputObject(CurObject, Input, FString::FromInt(AtIndex + 1));
				if (!ensure(NewInputObject))
					continue;

				// Remove the default/null object
				if (bNeedToEmpty)
				{
					GeoInputObjectsPtr->Empty();
					bNeedToEmpty = false;
				}

				// Add to the geo input array
				GeoInputObjectsPtr->Add(NewInputObject);
			}
		}
	}
	else if (InputType == EHoudiniInputType::Asset && IsValid(InputAssetComponent))
	{
		// Get the asset input object array
		TArray<UHoudiniInputObject*>* AssetInputObjectsPtr = Input->GetHoudiniInputObjectArray(InputType);
		if (ensure(AssetInputObjectsPtr))
		{
			// Find the V2 HAC that matches the V1_HAC pointed by InputAssetComponent
			// We can simply use the v1's HAC outer for that
			UHoudiniAssetComponent* InputHAC = Cast<UHoudiniAssetComponent>(InputAssetComponent->GetOuter());
			if (IsValid(InputHAC))
			{
				// Create a new InputObject wrapper
				UHoudiniInputObject* NewInputObject = UHoudiniInputObject::CreateTypedInputObject(InputHAC, Input, FString::FromInt(0));
				if (ensure(NewInputObject))
				{
					// Add to the asset input array
					AssetInputObjectsPtr->Add(NewInputObject);
				}
			}			
		}
	}
	else if (InputType == EHoudiniInputType::Curve)
	{
		// Get the curve input object array
		TArray<UHoudiniInputObject*>* CurveInputObjectsPtr = Input->GetHoudiniInputObjectArray(InputType);
		if (ensure(CurveInputObjectsPtr))
		{
			if (IsValid(InputCurve))
			{
				// Create a new InputObject wrapper
				UHoudiniInputObject* NewInputObject = UHoudiniInputObject::CreateTypedInputObject(InputCurve, Input, FString::FromInt(0));
				if (ensure(NewInputObject))
				{
					// Add to the curve input array
					CurveInputObjectsPtr->Add(NewInputObject);
				}

				// InputCurve->SetInputObject(NewInputObject);

				UHoudiniInputHoudiniSplineComponent* HoudiniSplineInput = Cast<UHoudiniInputHoudiniSplineComponent>(NewInputObject);
				if(HoudiniSplineInput)
					HoudiniSplineInput->Update(InputCurve);
			}
		}

		// TODO ???
		//InputCurveParameters;
	}
	else if (InputType == EHoudiniInputType::Landscape)
	{
		// Get the Landscape input object array
		TArray<UHoudiniInputObject*>* LandscapeInputObjectsPtr = Input->GetHoudiniInputObjectArray(InputType);
		if (ensure(LandscapeInputObjectsPtr))
		{
			// Find the V2 HAC that matches the V1_HAC pointed by InputAssetComponent
			// We can simply use the v1's HAC outer for that
			ALandscapeProxy* InLandscape = InputLandscapeProxy.Get();
			if (IsValid(InLandscape))
			{
				// Create a new InputObject wrapper
				UHoudiniInputObject* NewInputObject = UHoudiniInputObject::CreateTypedInputObject(InLandscape, Input, FString::FromInt(0));
				if (ensure(NewInputObject))
				{
					// Add to the geo input array
					LandscapeInputObjectsPtr->Add(NewInputObject);
				}
			}
		}

		Input->bLandscapeAutoSelectComponent = bLandscapeAutoSelectComponent;
		Input->bLandscapeExportSelectionOnly = bLandscapeExportSelectionOnly;
		Input->bLandscapeExportLighting = bLandscapeExportLighting;
		Input->bLandscapeExportMaterials = bLandscapeExportMaterials;
		Input->bLandscapeExportNormalizedUVs = bLandscapeExportNormalizedUVs;
		Input->bLandscapeExportTileUVs = bLandscapeExportTileUVs;
		
		//bLandscapeExportCurves;		
	}
	else if (InputType == EHoudiniInputType::World)
	{
		// Get the world input object array
		TArray<UHoudiniInputObject*>* WorldInputObjectsPtr = Input->GetHoudiniInputObjectArray(InputType);

		UWorld* MyWorld = InOuter->GetWorld();
		if (ensure(WorldInputObjectsPtr))
		{
			// Add the geometry input objects
			for (int32 AtIndex = 0; AtIndex < InputOutlinerMeshArray.Num(); AtIndex++)
			{
				FHoudiniAssetInputOutlinerMesh_V1& CurWorldInObj = InputOutlinerMeshArray[AtIndex];

				AActor* CurActor = nullptr;
				if (CurWorldInObj.ActorPtr.IsValid())
				{
					CurActor = CurWorldInObj.ActorPtr.Get();
				}
				else
				{
					// Try to update the actor ptr via the pathname
					CurWorldInObj.TryToUpdateActorPtrFromActorPathName(MyWorld);
					if (CurWorldInObj.ActorPtr.IsValid())
					{
						CurActor = CurWorldInObj.ActorPtr.Get();
					}
				}

				if(!IsValid(CurActor))
					continue;

				// Create a new InputObject wrapper for the actor
				UHoudiniInputObject* NewInputObject = UHoudiniInputObject::CreateTypedInputObject(CurActor, Input, FString::FromInt(AtIndex + 1));
				if (!ensure(NewInputObject))
					continue;

				// Add to the geo input array
				WorldInputObjectsPtr->Add(NewInputObject);
			}
		}

		/*
		CurWorldInObj->HoudiniAssetParameterVersion;
		CurWorldInObj->ActorPtr;
		CurWorldInObj->ActorPathName;
		CurWorldInObj->StaticMeshComponent;
		CurWorldInObj->StaticMesh;
		CurWorldInObj->SplineComponent;
		CurWorldInObj->NumberOfSplineControlPoints;
		CurWorldInObj->SplineControlPointsTransform;
		CurWorldInObj->SplineLength;
		CurWorldInObj->SplineResolution;
		CurWorldInObj->ActorTransform;
		CurWorldInObj->ComponentTransform;
		CurWorldInObj->AssetId;
		CurWorldInObj->KeepWorldTransform;
		CurWorldInObj->MeshComponentsMaterials;
		CurWorldInObj->InstanceIndex;
		*/

		//InputOutlinerMeshArray;
	}
	else
	{
		// Invalid
	}

	//ChoiceStringValue;	
	//bStaticMeshChanged;
	//bSwitchedToCurve;
	//bLoadedParameter = true;
	//bInputAssetConnectedInHoudini;
	//InputTransforms;
	//InputLandscapeTransform;

	return Input;
}

FArchive&
operator<<(FArchive& Ar, FHoudiniAssetInputOutlinerMesh_V1& HoudiniAssetInputOutlinerMesh)
{
	HoudiniAssetInputOutlinerMesh.Serialize(Ar);
	return Ar;
}

void
FHoudiniAssetInputOutlinerMesh_V1::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	HoudiniAssetParameterVersion = VER_HOUDINI_PLUGIN_SERIALIZATION_AUTOMATIC_VERSION;
	Ar << HoudiniAssetParameterVersion;

	Ar << ActorPtr;
	if ((HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_OUTLINER_INPUT_SAVE_ACTOR_PATHNAME)
		&& (HoudiniAssetParameterVersion != VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_419_SERIALIZATION_FIX))
	{
		Ar << ActorPathName;
	}

	if (HoudiniAssetParameterVersion < VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_OUTLINER_INPUT_SAVE_ACTOR_ONLY)
	{
		Ar << StaticMeshComponent;
		Ar << StaticMesh;
	}

	Ar << ActorTransform;

	Ar << AssetId;
	if (Ar.IsLoading() && !Ar.IsTransacting())
		AssetId = -1;

	if (HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_ADDED_UNREAL_SPLINE
		&& HoudiniAssetParameterVersion < VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_OUTLINER_INPUT_SAVE_ACTOR_ONLY)
	{
		Ar << SplineComponent;
		Ar << NumberOfSplineControlPoints;
		Ar << SplineLength;
		Ar << SplineResolution;
		Ar << ComponentTransform;
	}

	if (HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_ADDED_KEEP_TRANSFORM)
		Ar << KeepWorldTransform;

	// UE4.19 SERIALIZATION FIX:
	// The component materials serialization (24) was actually missing in the UE4.19 H17.0 / H16.5 plugin.
	// However subsequent serialized changes (25+) were present in those version. This caused crashes when loading
	// a level that was saved with 4.19+16.5/17.0 on a newer version of Unreal or Houdini...
	// If the serialized version is exactly that of the fix, we can ignore the materials paths as well
	if ((HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_OUTLINER_INPUT_SAVE_MAT)
		&& (HoudiniAssetParameterVersion != VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_419_SERIALIZATION_FIX))
		Ar << MeshComponentsMaterials;

	if (HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_OUTLINER_INSTANCE_INDEX)
		Ar << InstanceIndex;
}

bool
FHoudiniAssetInputOutlinerMesh_V1::TryToUpdateActorPtrFromActorPathName(UWorld* InWorld)
{
	// Ensure our current ActorPathName looks valid
	if (ActorPathName.IsEmpty() || ActorPathName.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		return false;

	// We'll try to find the corresponding actor by browsing through all the actors in the world..
	// Get the editor world
	UWorld* World = InWorld;
	if (!World)
		return false;

	// Then try to find the actor corresponding to our path/name
	bool FoundActor = false;
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		if (ActorIt->GetPathName() != ActorPathName)
			continue;

		// We found the actor
		ActorPtr = *ActorIt;
		FoundActor = true;

		break;
	}

	if (FoundActor)
	{
		// We need to invalid our components so they can be updated later
		// from the new actor
		StaticMesh = NULL;
		StaticMeshComponent = NULL;
		SplineComponent = NULL;
	}

	return FoundActor;
}

UHoudiniAssetComponentMaterials_V1::UHoudiniAssetComponentMaterials_V1(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}
void
UHoudiniAssetComponentMaterials_V1::Serialize(FArchive & Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	Ar << Assignments;
	Ar << Replacements;
}

UHoudiniHandleComponent_V1::UHoudiniHandleComponent_V1(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UHoudiniAssetInstanceInput::UHoudiniAssetInstanceInput(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void
UHoudiniAssetInstanceInput::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	Ar << Flags.HoudiniAssetInstanceInputFlagsPacked;
	Ar << HoudiniGeoPartObject;

	Ar << ObjectToInstanceId;
	// Object id is transient
	if (Ar.IsLoading() && !Ar.IsTransacting())
		ObjectToInstanceId = -1;

	// Serialize fields.
	Ar << InstanceInputFields;
}

UHoudiniAssetInstanceInputField::UHoudiniAssetInstanceInputField(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void
UHoudiniAssetInstanceInputField::Serialize(FArchive& Ar)
{
	// Call base implementation first.
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);
	const int32 InstanceInputFieldVersion = Ar.CustomVer(FHoudiniCustomSerializationVersion::GUID);

	Ar << HoudiniAssetInstanceInputFieldFlagsPacked;
	Ar << HoudiniGeoPartObject;

	FString UnusedInstancePathName;
	Ar << UnusedInstancePathName;
	Ar << RotationOffsets;
	Ar << ScaleOffsets;
	Ar << bScaleOffsetsLinearlyArray;

	Ar << InstancedTransforms;
	Ar << VariationTransformsArray;

	if (Ar.IsSaving() || (Ar.IsLoading() && InstanceInputFieldVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_INSTANCE_COLORS))
	{
		Ar << InstanceColorOverride;
		Ar << VariationInstanceColorOverrideArray;
	}

	Ar << InstancerComponents;
	Ar << InstancedObjects;
	Ar << OriginalObject;
}

void
UHoudiniHandleComponent_V1::Serialize(FArchive & Ar)
{
	//Super::Serialize(Ar);

	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	// XFormn Parames is an array of 9 float params + tuple index
	// TX TY TZ
	// RX RY RZ
	// SX SY SZ

	//UHoudiniAssetParameterFloat_V1* XFormParams[9];
	//int32 XFormParamsTupleIndex[9];
	for (int32 i = 0; i < 9; ++i)
	{
		Ar << XFormParams[i];
		Ar << XFormParamsTupleIndex[i];
	}		

	//UHoudiniAssetParameterChoice_V1* RSTParm;
	Ar << RSTParm;
	//int32 RSTParmTupleIdx;
	Ar << RSTParmTupleIdx;

	//UHoudiniAssetParameterChoice_V1* RotOrderParm;
	Ar << RotOrderParm;
	//int32 RotOrderParmTupleIdx;
	Ar << RotOrderParmTupleIdx;
}

/*
UHoudiniHandleComponent*
UHoudiniHandleComponent_V1::ConvertLegacyData(UObject* Outer)
{
	UHoudiniHandleComponent* NewHandle = nullptr;

	return NewHandle;
}
*/

bool
UHoudiniHandleComponent_V1::UpdateFromLegacyData(UHoudiniHandleComponent* NewHC)
{
	if (!IsValid(NewHC))
		return false;

	// TODO
	//NewHC->XformParms;
	//NewHC->RSTParm;
	//NewHC->RotOrderParm;
	//NewHC->HandleType;
	//NewHC->HandleName;

	return true;
}

UHoudiniSplineComponent_V1::UHoudiniSplineComponent_V1(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void
UHoudiniSplineComponent_V1::Serialize(FArchive & Ar)
{
	//Super::Serialize(Ar);

	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	int32 Version = VER_HOUDINI_PLUGIN_SERIALIZATION_AUTOMATIC_VERSION;
	Ar << Version;

	Ar << HoudiniGeoPartObject;

	if (Version < VER_HOUDINI_PLUGIN_SERIALIZATION_HOUDINI_SPLINE_TO_TRANSFORM)
	{
		// Before, curve points where stored as Vectors, not Transforms
		TArray<FVector> OldCurvePoints;
		Ar << OldCurvePoints;

		CurvePoints.SetNum(OldCurvePoints.Num());

		FTransform trans = FTransform::Identity;
		for (int32 n = 0; n < CurvePoints.Num(); n++)
		{
			trans.SetLocation(OldCurvePoints[n]);
			CurvePoints[n] = trans;
		}
	}
	else
	{
		Ar << CurvePoints;
	}

	Ar << CurveDisplayPoints;

	Ar << CurveType;
	Ar << CurveMethod;
	Ar << bClosedCurve;
}

UHoudiniSplineComponent*
UHoudiniSplineComponent_V1::ConvertLegacyData(UObject* Outer)
{
	UHoudiniSplineComponent* NewSpline = NewObject<UHoudiniSplineComponent>(
		GetOuter(), UHoudiniSplineComponent::StaticClass());

	UpdateFromLegacyData(NewSpline);

	return NewSpline;
}

bool
UHoudiniSplineComponent_V1::UpdateFromLegacyData(UHoudiniSplineComponent* NewSpline)
{
	if (!IsValid(NewSpline))
		return false;

	NewSpline->SetFlags(RF_Transactional);

	NewSpline->CurvePoints = CurvePoints;
	NewSpline->DisplayPoints = CurveDisplayPoints;
	//NewSpline->DisplayPointIndexDivider;
	//NewSpline->HoudiniSplineName;
	NewSpline->bClosed = bClosedCurve;
	NewSpline->bReversed = false;
	NewSpline->bIsHoudiniSplineVisible = true;

	//0 Polygon 1 Nurbs 2 Bezier
	if (CurveType == 0)
		NewSpline->CurveType = EHoudiniCurveType::Polygon;
	else if (CurveType == 1)
		NewSpline->CurveType = EHoudiniCurveType::Nurbs;
	else if (CurveType == 2)
		NewSpline->CurveType = EHoudiniCurveType::Bezier;
	else
		NewSpline->CurveType = EHoudiniCurveType::Invalid;

	// 0 CVs, 1 Breakpoints, 2 Freehand
	if (CurveMethod == 0)
		NewSpline->CurveMethod = EHoudiniCurveMethod::CVs;
	else if (CurveMethod == 1)
		NewSpline->CurveMethod = EHoudiniCurveMethod::Breakpoints;
	else if (CurveMethod == 2)
		NewSpline->CurveMethod = EHoudiniCurveMethod::Freehand;
	else
		NewSpline->CurveMethod = EHoudiniCurveMethod::Invalid;
	
	NewSpline->bIsOutputCurve = false;
	
	NewSpline->HoudiniGeoPartObject = HoudiniGeoPartObject.ConvertLegacyData();	

	if (NewSpline->HoudiniGeoPartObject.bIsEditable)
	{
		NewSpline->bIsEditableOutputCurve = true;
		NewSpline->bIsInputCurve = false;
	}
	else
	{
		NewSpline->bIsInputCurve = false;
		NewSpline->bIsEditableOutputCurve = true;		
	}		

	// Create a default Houdini spline input if a null pointer is passed in.
	FName HoudiniSplineName = MakeUniqueObjectName(GetOuter(), UHoudiniSplineComponent::StaticClass(), TEXT("Houdini Spline"));
	NewSpline->SetHoudiniSplineName(HoudiniSplineName.ToString());

	//NewSpline->bHasChanged;
	//NewSpline->bNeedsToTriggerUpdate;
	//NewSpline->InputObject;
	//NewSpline->NodeId;
	//NewSpline->PartName;

	return true;
}

UHoudiniAssetParameter::UHoudiniAssetParameter(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void
UHoudiniAssetParameter::Serialize(FArchive & Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	HoudiniAssetParameterVersion = VER_HOUDINI_PLUGIN_SERIALIZATION_AUTOMATIC_VERSION;
	Ar << HoudiniAssetParameterVersion;

	Ar << HoudiniAssetParameterFlagsPacked;

	if (Ar.IsLoading())
		bChanged = false;

	Ar << ParameterName;
	Ar << ParameterLabel;

	Ar << NodeId;
	if (!Ar.IsTransacting() && Ar.IsLoading())
	{
		// NodeId is invalid after load
		NodeId = -1;
	}
	Ar << ParmId;

	Ar << ParmParentId;
	Ar << ChildIndex;

	Ar << TupleSize;
	Ar << ValuesIndex;
	Ar << MultiparmInstanceIndex;

	if (HoudiniAssetParameterVersion >= VER_HOUDINI_ENGINE_PARAM_ASSET_INSTANCE_MEMBER)
	{
		UObject* Dummy = nullptr;
		Ar << Dummy;
	}

	if (HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_ADDED_PARAM_HELP)
	{
		Ar << ParameterHelp;
	}
	else
	{
		ParameterHelp = TEXT("");
	}
	/*
	if (Ar.IsTransacting())
	{
		Ar << PrimaryObject;
		Ar << ParentParameter;
	}
	*/
}

UHoudiniParameter*
UHoudiniAssetParameter::ConvertLegacyData(UObject* Outer)
{
	return UHoudiniParameter::Create(Outer, ParameterName);
}

void
UHoudiniAssetParameter::CopyLegacyParameterData(UHoudiniParameter* InNewParm)
{
	if (!IsValid(InNewParm))
		return;

	InNewParm->Name = ParameterName;
	InNewParm->Label = ParameterLabel;
	//InNewParm->ParmType;
	InNewParm->TupleSize = TupleSize;
	InNewParm->NodeId = NodeId;
	InNewParm->ParmId = ParmId;
	InNewParm->ParentParmId = ParmParentId;
	InNewParm->ChildIndex = ChildIndex;
	InNewParm->bIsVisible = true;
	InNewParm->bIsDisabled = bIsDisabled;
	InNewParm->bHasChanged = bChanged;
	//InNewParm->bNeedsToTriggerUpdate;
	//InNewParm->bIsDefault;
	InNewParm->bIsSpare = bIsSpare;
	InNewParm->bJoinNext = false;
	InNewParm->bIsChildOfMultiParm = bIsChildOfMultiparm;
	// TODO: MultiparmInstanceIndex ?
	//InNewParm->bIsDirectChildOfMultiParm;
	InNewParm->bPendingRevertToDefault = false;
	//InNewParm->TuplePendingRevertToDefault = false;
	InNewParm->Help = ParameterHelp;
	InNewParm->TagCount = 0;
	InNewParm->ValueIndex = ValuesIndex;
	//InNewParm->bHasExpression;
	//InNewParm->bShowExpression;
	//InNewParm->ParamExpression;
	//InNewParm->Tags;
	InNewParm->bAutoUpdate = true;
}

UHoudiniAssetParameterChoice::UHoudiniAssetParameterChoice(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void
UHoudiniAssetParameterChoice::Serialize(FArchive & Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	if (Ar.IsLoading())
	{
		StringChoiceValues.Empty();
		StringChoiceLabels.Empty();
	}

	int32 NumChoices = StringChoiceValues.Num();
	Ar << NumChoices;

	int32 NumLabels = StringChoiceLabels.Num();
	Ar << NumLabels;

	if (Ar.IsLoading())
	{
		FString Temp;
		for (int32 ChoiceIdx = 0; ChoiceIdx < NumChoices; ++ChoiceIdx)
		{
			Ar << Temp;
			StringChoiceValues.Add(Temp);
		}

		for (int32 LabelIdx = 0; LabelIdx < NumLabels; ++LabelIdx)
		{
			Ar << Temp;
			StringChoiceLabels.Add(Temp);
		}
	}

	Ar << StringValue;
	Ar << CurrentValue;

	Ar << bStringChoiceList;
}

UHoudiniParameter*
UHoudiniAssetParameterChoice::ConvertLegacyData(UObject* Outer)
{
	UHoudiniParameterChoice* Parm = nullptr;
	if (bStringChoiceList)
	{
		Parm = UHoudiniParameterChoice::Create(Outer, ParameterName, EHoudiniParameterType::StringChoice);
	}
	else
	{
		Parm = UHoudiniParameterChoice::Create(Outer, ParameterName, EHoudiniParameterType::IntChoice);
	}

	Parm->SetNumChoices(StringChoiceValues.Num());
	for (int32 Idx = 0; Idx < StringChoiceValues.Num(); Idx++)
	{
		FString * ChoiceValue = Parm->GetStringChoiceValueAt(Idx);
		if (ChoiceValue)
			*ChoiceValue = StringChoiceValues[Idx];
		FString * ChoiceLabel = Parm->GetStringChoiceLabelAt(Idx);
	}

	for (int32 Idx = 0; Idx < StringChoiceLabels.Num(); Idx++)
	{
		FString * ChoiceLabel = Parm->GetStringChoiceLabelAt(Idx);
		if (ChoiceLabel)
			*ChoiceLabel = StringChoiceValues[Idx];			
	}

	Parm->SetStringValue(StringValue);
	Parm->SetIntValue(CurrentValue);
	//Parm->DefaultStringValue = StringValue;
	//Parm->SetDefault();
	//Parm->DefaultIntValue = CurrentValue;
	
	return Parm;
}

UHoudiniAssetParameterButton::UHoudiniAssetParameterButton(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UHoudiniParameter* 
UHoudiniAssetParameterButton::ConvertLegacyData(UObject* Outer)
{
	// Button strips where not supported in v1, just create a normal button
	return UHoudiniParameterButton::Create(Outer, ParameterName);
}

UHoudiniAssetParameterColor::UHoudiniAssetParameterColor(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void
UHoudiniAssetParameterColor::Serialize(FArchive & Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	if (Ar.IsLoading())
		Color = FColor::White;

	Ar << Color;
}

UHoudiniParameter* 
UHoudiniAssetParameterColor::ConvertLegacyData(UObject* Outer)
{
	UHoudiniParameterColor* Parm = UHoudiniParameterColor::Create(Outer, ParameterName);
	Parm->SetColorValue(Color);

	//Parm->DefaultColor = Color;
	Parm->SetDefaultValue();

	//Parm->bIsChildOfRamp = false;

	return Parm;
}

UHoudiniAssetParameterFile::UHoudiniAssetParameterFile(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void
UHoudiniAssetParameterFile::Serialize(FArchive & Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	Ar << Values;
	Ar << Filters;

	if (HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_FILE_PARAM_READ_ONLY)
		Ar << IsReadOnly;
}

UHoudiniParameter*
UHoudiniAssetParameterFile::ConvertLegacyData(UObject* Outer)
{
	UHoudiniParameterFile* Parm = UHoudiniParameterFile::Create(Outer, ParameterName);
	
	Parm->SetNumberOfValues(Values.Num());
	for (int32 Idx = 0; Idx < Values.Num(); Idx++)
		Parm->SetValueAt(Values[Idx], Idx);

	Parm->SetDefaultValues();
	//Parm->DefaultValues = Values;

	Parm->SetFileFilters(Filters);
	Parm->SetReadOnly(IsReadOnly);

	return Parm;
}

UHoudiniAssetParameterFloat::UHoudiniAssetParameterFloat(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void
UHoudiniAssetParameterFloat::Serialize(FArchive & Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	Ar << Values;

	Ar << ValueMin;
	Ar << ValueMax;

	Ar << ValueUIMin;
	Ar << ValueUIMax;

	if (HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_PARAMETERS_UNIT)
		Ar << ValueUnit;

	if (HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_PARAMETERS_NOSWAP)
		Ar << NoSwap;
}

UHoudiniParameter*
UHoudiniAssetParameterFloat::ConvertLegacyData(UObject* Outer)
{
	UHoudiniParameterFloat* Parm = UHoudiniParameterFloat::Create(Outer, ParameterName);

	Parm->SetNumberOfValues(Values.Num());
	for (int32 Idx = 0; Idx < Values.Num(); Idx++)
		Parm->SetValueAt(Values[Idx], Idx);

	Parm->SetDefaultValues();
	//Parm->DefaultValues = Values;

	Parm->SetUnit(ValueUnit);
	Parm->SetHasMin(true);
	Parm->SetHasMax(true);
	Parm->SetHasUIMin(true);
	Parm->SetHasUIMax(true);
	Parm->SetIsLogarithmic(false);
	Parm->SetMin(ValueMin);
	Parm->SetMax(ValueMax);
	Parm->SetUIMin(ValueUIMin);
	Parm->SetUIMax(ValueUIMax);

	//Parm->bIsChildOfRamp = false;

	return Parm;
}

UHoudiniAssetParameterFolder::UHoudiniAssetParameterFolder(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UHoudiniParameter* 
UHoudiniAssetParameterFolder::ConvertLegacyData(UObject* Outer)
{
	return UHoudiniParameterFolder::Create(Outer, ParameterName);
}

UHoudiniAssetParameterFolderList::UHoudiniAssetParameterFolderList(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UHoudiniParameter*
UHoudiniAssetParameterFolderList::ConvertLegacyData(UObject* Outer)
{
	return UHoudiniParameterFolderList::Create(Outer, ParameterName);
}

UHoudiniAssetParameterInt::UHoudiniAssetParameterInt(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void
UHoudiniAssetParameterInt::Serialize(FArchive & Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	Ar << Values;

	Ar << ValueMin;
	Ar << ValueMax;

	Ar << ValueUIMin;
	Ar << ValueUIMax;

	if (HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_PARAMETERS_UNIT)
		Ar << ValueUnit;
}

UHoudiniParameter*
UHoudiniAssetParameterInt::ConvertLegacyData(UObject* Outer)
{
	UHoudiniParameterInt* Parm = UHoudiniParameterInt::Create(Outer, ParameterName);

	Parm->SetNumberOfValues(Values.Num());
	for (int32 Idx = 0; Idx < Values.Num(); Idx++)
		Parm->SetValueAt(Values[Idx], Idx);

	Parm->SetDefaultValues();

	//Parm->DefaultValues = Values;
	Parm->SetUnit(ValueUnit);
	Parm->SetHasMin(true);
	Parm->SetHasMax(true);
	Parm->SetHasUIMin(true);
	Parm->SetHasUIMax(true);
	Parm->SetIsLogarithmic(false);
	Parm->SetMin(ValueMin);
	Parm->SetMax(ValueMax);
	Parm->SetUIMin(ValueUIMin);
	Parm->SetUIMax(ValueUIMax);

	return Parm;
}

UHoudiniAssetParameterLabel::UHoudiniAssetParameterLabel(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UHoudiniParameter*
UHoudiniAssetParameterLabel::ConvertLegacyData(UObject* Outer)
{
	return UHoudiniParameterLabel::Create(Outer, ParameterName);
}

UHoudiniAssetParameterMultiparm::UHoudiniAssetParameterMultiparm(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void
UHoudiniAssetParameterMultiparm::Serialize(FArchive & Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	if (Ar.IsLoading())
		MultiparmValue = 0;

	Ar << MultiparmValue;
}

UHoudiniParameter*
UHoudiniAssetParameterMultiparm::ConvertLegacyData(UObject* Outer)
{
	UHoudiniParameterMultiParm* Parm = UHoudiniParameterMultiParm::Create(Outer, ParameterName);

	//Parm->bIsShown;
	//Parm->Value;
	//Parm->TemplateName;
	Parm->MultiparmValue = MultiparmValue;
	//Parm->MultiParmInstanceNum;
	//Parm->MultiParmInstanceLength;
	//Parm->MultiParmInstanceCount;
	//Parm->InstanceStartOffset;
	//Parm->DefaultInstanceCount;

	// TODO:
	// MultiparmInstanceIndex?

	return Parm;
}

UHoudiniAssetParameterRamp::UHoudiniAssetParameterRamp(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void
UHoudiniAssetParameterRamp::Serialize(FArchive & Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	int32 multiparmvalue = 0;
	Ar << multiparmvalue;

	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	Ar << HoudiniAssetParameterRampCurveFloat;
	Ar << HoudiniAssetParameterRampCurveColor;

	Ar << bIsFloatRamp;
}

UHoudiniParameter*
UHoudiniAssetParameterRamp::ConvertLegacyData(UObject* Outer)
{
	if (bIsFloatRamp)
	{
		UHoudiniParameterRampFloat* Parm = UHoudiniParameterRampFloat::Create(Outer, ParameterName);

		// TODO: 
		// Convert HoudiniAssetParameterRampCurveFloat

		return Parm;
	}
	else
	{
		UHoudiniParameterRampColor* Parm = UHoudiniParameterRampColor::Create(Outer, ParameterName);

		// TODO: 
		// Convert HoudiniAssetParameterRampCurveColor

		return Parm;
	}	
}

UHoudiniAssetParameterSeparator::UHoudiniAssetParameterSeparator(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UHoudiniParameter*
UHoudiniAssetParameterSeparator::ConvertLegacyData(UObject* Outer)
{
	return UHoudiniParameterSeparator::Create(Outer, ParameterName);
}

UHoudiniAssetParameterString::UHoudiniAssetParameterString(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void
UHoudiniAssetParameterString::Serialize(FArchive & Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	Ar << Values;
}

UHoudiniParameter*
UHoudiniAssetParameterString::ConvertLegacyData(UObject* Outer)
{
	UHoudiniParameterString* Parm = UHoudiniParameterString::Create(Outer, ParameterName);

	Parm->SetNumberOfValues(Values.Num());
	for (int32 Idx = 0; Idx < Values.Num(); Idx++)
		Parm->SetValueAt(Values[Idx], Idx);

	Parm->SetIsAssetRef(false);
	Parm->SetDefaultValues();
	
	//Parm->DefaultValues = Values;
	//Parm->ChosenAssets.Empty();
	//Parm->bIsAssetRef = false;

	return Parm;
}

UHoudiniAssetParameterToggle::UHoudiniAssetParameterToggle(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void
UHoudiniAssetParameterToggle::Serialize(FArchive & Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	Ar << Values;
}

UHoudiniParameter* 
UHoudiniAssetParameterToggle::ConvertLegacyData(UObject* Outer)
{
	UHoudiniParameterToggle* Parm = UHoudiniParameterToggle::Create(Outer, ParameterName);

	Parm->SetNumberOfValues(Values.Num());
	for (int32 Idx = 0; Idx < Values.Num(); Idx++)
		Parm->SetValueAt((bool)Values[Idx], Idx);

	Parm->SetDefaultValues();
	//Parm->DefaultValues = Values;

	return Parm;
}

UHoudiniMeshSplitInstancerComponent_V1::UHoudiniMeshSplitInstancerComponent_V1(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void
UHoudiniMeshSplitInstancerComponent_V1::Serialize(FArchive & Ar)
{
	//Super::Serialize(Ar);
	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	Ar << InstancedMesh;
	Ar << OverrideMaterial;
	Ar << Instances;
}

bool
UHoudiniMeshSplitInstancerComponent_V1::UpdateFromLegacyData(UHoudiniMeshSplitInstancerComponent* NewMSIC)
{
	if (!IsValid(NewMSIC))
		return false;

	NewMSIC->Instances = Instances;
	NewMSIC->OverrideMaterials.Add(OverrideMaterial);
	NewMSIC->InstancedMesh = InstancedMesh;

	return true;
}

UHoudiniInstancedActorComponent_V1::UHoudiniInstancedActorComponent_V1(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void
UHoudiniInstancedActorComponent_V1::Serialize(FArchive & Ar)
{
	//Super::Serialize(Ar);
	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	Ar << InstancedAsset;
	Ar << Instances;
}

bool
UHoudiniInstancedActorComponent_V1::UpdateFromLegacyData(UHoudiniInstancedActorComponent* NewIAC)
{
	if (!IsValid(NewIAC))
		return false;

	//NewIAC->SetInstancedObject(InstancedAsset);
	NewIAC->InstancedObject = InstancedAsset;
	NewIAC->InstancedActors = Instances;

	return true;
}