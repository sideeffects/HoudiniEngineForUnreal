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

#include "HoudiniInstanceTranslator.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniEngineString.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniGenericAttribute.h"
#include "HoudiniInstancedActorComponent.h"
#include "HoudiniMaterialTranslator.h"
#include "HoudiniOutput.h"
#include "HoudiniStaticMeshComponent.h"
#include "HoudiniStaticMesh.h"
#include "HoudiniFoliageTools.h"

//#include "HAPI/HAPI_Common.h"

#include "Engine/StaticMesh.h"
#include "ComponentReregisterContext.h"
#include "HoudiniMaterialTranslator.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "InstancedFoliageActor.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	#include "GeometryCollection/GeometryCollectionComponent.h"
#else
	#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionComponent.h"
#endif
#include "FoliageEditUtility.h"
#include "LevelInstance/LevelInstanceActor.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "LevelInstance/LevelInstanceComponent.h"
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
	#include "StaticMeshComponentLODInfo.h"
#endif

#if WITH_EDITOR
	//#include "ScopedTransaction.h"
	#include "LevelEditorViewport.h"
	#include "MeshPaintHelpers.h"
#endif
#include "HoudiniEngineAttributes.h"
#include "HoudiniFoliageUtils.h"
#include "HoudiniMeshTranslator.h"
#include "GeometryCollection/GeometryCollectionRenderLevelSetActor.h"
#include <cstdint>

#include "HoudiniHLODLayerUtils.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

//
FHoudiniInstancerPartData
FHoudiniInstanceTranslator::PopulateInstancedOutputPartData(
	const FHoudiniGeoPartObject& InHGPO,
	const TArray<UHoudiniOutput*>& InAllOutputs)
{
	FHoudiniInstancerPartData PartData;
	bool bSuccess = false;

	switch (InHGPO.InstancerType)
	{
	case EHoudiniInstancerType::GeometryCollection:
	case EHoudiniInstancerType::PackedPrimitive:
		bSuccess = GetPackedPrimitiveInstancerPartData(InHGPO, InAllOutputs, PartData);
		break;

	case EHoudiniInstancerType::AttributeInstancer:
		bSuccess = GetAttributeInstancerPartData(InHGPO, PartData);
		break;
	case EHoudiniInstancerType::OldSchoolAttributeInstancer:
		HOUDINI_LOG_ERROR(TEXT("Old School Attribute Instancers are deprecated"));
		break;

	case EHoudiniInstancerType::ObjectInstancer:
		HOUDINI_LOG_ERROR(TEXT("Object Instancers are deprecated"));
		break;
	}

	if (!bSuccess)
		return {};

	GetGenericPropertiesAttributes(InHGPO.GeoId, InHGPO.PartId, PartData.AllPropertyAttributes);
	GetPerInstanceCustomData(InHGPO.GeoId, InHGPO.PartId, PartData);
	GetMaterialOverridesFromAttributes(InHGPO.GeoId, InHGPO.PartId, 0, InHGPO.InstancerType, PartData.MaterialAttributes);

	return PartData;
}

int
FHoudiniInstanceTranslator::CreateAllInstancersFromHoudiniOutputs(
	const TArray<UHoudiniOutput*>& InAllOutputs,
	UObject* InOuterComponent,
	const FHoudiniPackageParams& InPackageParms,
	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancerPartData>* InPreBuiltInstancedOutputPartData)
{
	return CreateAllInstancersFromHoudiniOutputs(
		InAllOutputs,
		InAllOutputs,
		InOuterComponent,
		InPackageParms,
		InPreBuiltInstancedOutputPartData);
}

int
FHoudiniInstanceTranslator::CreateAllInstancersFromHoudiniOutputs(
	const TArray<UHoudiniOutput*>& OutputsToUpdate,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	UObject* InOuterComponent,
	const FHoudiniPackageParams& InPackageParms,
	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancerPartData>* InPreBuiltInstancedOutputPartData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInstanceTranslator::CreateAllInstancersFromHoudiniOutputs);

	USceneComponent* ParentComponent = Cast<USceneComponent>(InOuterComponent);
	if (!ParentComponent)
		return false;

    int InstanceCount = 0;
	for (auto Output : OutputsToUpdate)
	{
		if (Output->GetType() != EHoudiniOutputType::Instancer)
			continue;

		bool bSuccess = FHoudiniInstanceTranslator::CreateAllInstancersFromHoudiniOutput(
			Output,
			InAllOutputs,
			InOuterComponent,
			InPackageParms,
			InPreBuiltInstancedOutputPartData);

		if (bSuccess)
			++InstanceCount;
	}

	bool bOutputFoliage = false;
	for (auto Output : OutputsToUpdate)
	{
		for(auto It : Output->OutputObjects)
		{
			if (It.Value.FoliageType)
			{
				bOutputFoliage = true;
				break;
			}
		}
		if (bOutputFoliage)
			break;

	}
	if (bOutputFoliage)
	{
		FHoudiniEngineUtils::RepopulateFoliageTypeListInUI();
	}
	return InstanceCount;
}


bool
FHoudiniInstanceTranslator::CreateAllInstancersFromHoudiniOutput(
	UHoudiniOutput* InOutput,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	UObject* InOuterComponent,
	const FHoudiniPackageParams& InPackageParams,
	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancerPartData>* InPreBuiltInstancedOutputPartData)
{
	if (!IsValid(InOutput))
		return false;

	if (!IsValid(InOuterComponent))
		return false;

	if (InOutput->Type == EHoudiniOutputType::GeometryCollection)
		return true;

	// Keep track of the previous cook's component to clean them up after
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> OutputObjects;

	USceneComponent* ParentComponent = Cast<USceneComponent>(InOuterComponent);
	if (!ParentComponent)
		return false;

	//------------------------------------------------------------------------------------------------------------------------------
	// If Part Data was passed in, use that. If not fetch part data
	//------------------------------------------------------------------------------------------------------------------------------

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancerPartData> LocalPartData;
	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancerPartData> * Parts = InPreBuiltInstancedOutputPartData;
	if (!Parts)
	{
		for (const FHoudiniGeoPartObject& HGPO : InOutput->HoudiniGeoPartObjects)
		{
			// Not an instancer, skip
			if (HGPO.Type != EHoudiniPartType::Instancer)
				continue;

			// Prepare this output object's output identifier
			FHoudiniOutputObjectIdentifier OutputIdentifier;
			OutputIdentifier.ObjectId = HGPO.ObjectId;
			OutputIdentifier.GeoId = HGPO.GeoId;
			OutputIdentifier.PartId = HGPO.PartId;
			OutputIdentifier.PartName = HGPO.PartName;

			FHoudiniInstancerPartData InstancedOutputPartDataTmp = PopulateInstancedOutputPartData(HGPO, InAllOutputs);
			LocalPartData.Add(OutputIdentifier, InstancedOutputPartDataTmp);
		}
		Parts = &LocalPartData;
	}

	//------------------------------------------------------------------------------------------------------------------------------
	// Iterate on all the outputs' HGPOs, creating instancers as we go
	//------------------------------------------------------------------------------------------------------------------------------

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancedOutput> InstancedOutputs;

	for (auto It : *Parts)
	{
		const FHoudiniInstancerPartData& InstancePartData = It.Value;
	
		// Create the instancer components now
		for (int32 InstanceObjectIdx = 0; InstanceObjectIdx < InstancePartData.Instancers.Num(); InstanceObjectIdx++)
		{
			auto& Instancer = InstancePartData.Instancers[InstanceObjectIdx];

			// Prepare this output object's output identifier
			FHoudiniOutputObjectIdentifier OutputIdentifier = It.Key;
			OutputIdentifier.SplitIdentifier = Instancer.SplitName;

			FHoudiniPackageParams InstancerPackageParams = InPackageParams;
			InstancerPackageParams.ObjectId = OutputIdentifier.ObjectId;
			InstancerPackageParams.GeoId = OutputIdentifier.GeoId;
			InstancerPackageParams.PartId = OutputIdentifier.PartId;

			// Find the matching instance output now
			// Instanced output only use the original object index for their split identifier
			FHoudiniOutputObjectIdentifier InstancedOutputIdentifier = OutputIdentifier;
			InstancedOutputIdentifier.SplitIdentifier = Instancer.SplitName;

			// Get all the materials needed for this object
			// Multiple material slots are supported, as well as creating new material instances if needed
			TArray<UMaterialInterface*> InstanceMaterials;
			InstanceMaterials = GetAllInstancerMaterials(Instancer.AttributeIndices[0], InstancePartData.GeoPartObject, InPackageParams);


			FHoudiniOutputObject OutputObject;
			UObject* InstancedObject;

			bool bSuccess = CreateInstancer(
				InstancedOutputIdentifier,
				OutputObject,
				InstancedObject,
				Instancer,
				InstancePartData,
				InstancerPackageParams,
				ParentComponent,
				InstanceMaterials);

			// Make sure the output is valid, if so add it to the outputs.
			if (bSuccess)
			{
				FHoudiniOutputObject& NewOutputObject = OutputObjects.FindOrAdd(InstancedOutputIdentifier);
				NewOutputObject = MoveTemp(OutputObject);

				FHoudiniInstancedOutput& InstancedOutput = InstancedOutputs.FindOrAdd(InstancedOutputIdentifier);
				InstancedOutput.NumInstances = Instancer.AttributeIndices.Num();
				InstancedOutput.InstancedObject = InstancedObject;
			}
		}
	}

	InOutput->SetInstancedOutputs(InstancedOutputs);
	InOutput->SetOutputObjects(OutputObjects);

	return true;
}

bool
FHoudiniInstanceTranslator::FindInstancedOutputObject(
	const FHoudiniGeoPartObject& HGPO,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	FHoudiniOutputObjectIdentifier & OutId,
	const UHoudiniOutput* & OutOutput)
{
	for (const auto& Output : InAllOutputs)
	{
		if (!Output || Output->Type != EHoudiniOutputType::Mesh)
			continue;

		for (const auto& OutObjPair : Output->OutputObjects)
		{
			if (!OutObjPair.Key.Matches(HGPO))
				continue;

			const FHoudiniOutputObject& CurrentOutputObject = OutObjPair.Value;

			if (CurrentOutputObject.bIsImplicit)
				continue;

			OutOutput = Output;
			OutId = OutObjPair.Key;
			return true;
		}
	}

	return false;
}

void 
FHoudiniInstanceTranslator::SetInstancerObject(
	FHoudiniInstancer & InstancerData,
	const FHoudiniGeoPartObject& HGPO,
	const TArray<UHoudiniOutput*>& InAllOutputs)
{
	FHoudiniOutputObjectIdentifier OutId;
	const UHoudiniOutput* OutOutput;

	bool bFound = FindInstancedOutputObject(HGPO, InAllOutputs, OutId, OutOutput);
	if (!bFound)
		return;

	const FHoudiniOutputObject * OutputObject = OutOutput->OutputObjects.Find(OutId);

	if (InstancerData.AttributeIndices.Num() == 1 && OutputObject->bProxyIsCurrent && IsValid(OutputObject->ProxyObject))
	{
		InstancerData.ObjectPath = OutputObject->ProxyObject->GetPathName();
	}
	else if (IsValid(OutputObject->OutputObject))
	{
		InstancerData.ObjectPath = OutputObject->OutputObject->GetPathName();

		EHoudiniSplitType SplitType = FHoudiniMeshTranslator::GetSplitTypeFromSplitName(OutId.SplitIdentifier);
		if (SplitType == EHoudiniSplitType::InvisibleComplexCollider)
		{
			InstancerData.bVisible = false;
		}
	}
}

TArray<FTransform> FHoudiniInstanceTranslator::GetInstancerTransforms(const FHoudiniGeoPartObject& InHGPO)
{
	TArray<HAPI_Transform> InstancerPartTransforms;
	InstancerPartTransforms.SetNumZeroed(InHGPO.PartInfo.InstanceCount);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetInstancerPartTransforms(FHoudiniEngine::Get().GetSession(),
		InHGPO.GeoId, InHGPO.PartInfo.PartId,
		HAPI_RSTORDER_DEFAULT,
		InstancerPartTransforms.GetData(), 0, InHGPO.PartInfo.InstanceCount), {});

	// Convert the transform to Unreal's coordinate system
	TArray<FTransform> InstancerUnrealTransforms;
	InstancerUnrealTransforms.SetNum(InstancerPartTransforms.Num());
	for (int32 InstanceIdx = 0; InstanceIdx < InstancerPartTransforms.Num(); InstanceIdx++)
	{
		const auto& InstanceTransform = InstancerPartTransforms[InstanceIdx];
		FHoudiniEngineUtils::TranslateHapiTransform(InstanceTransform, InstancerUnrealTransforms[InstanceIdx]);
	}
	return InstancerUnrealTransforms;
}
TTuple<FString, FHoudiniEngineIndexedStringMap> FHoudiniInstanceTranslator::GetSplitData(const FHoudiniGeoPartObject& HGPO, HAPI_AttributeOwner Owner)
{
	//---------------------------------------------------------------------------------------------------------------------------
	// Get split information, if it exists
	//---------------------------------------------------------------------------------------------------------------------------

	FHoudiniHapiAccessor Accessor;
	Accessor.Init(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_SPLIT_ATTR);
	FString SplitAttrName;
	Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, SplitAttrName);
	FHoudiniEngineIndexedStringMap SplitAttributes;
	if (!SplitAttrName.IsEmpty())
	{
		Accessor.Init(HGPO.GeoId, HGPO.PartId, H_TCHAR_TO_UTF8(*SplitAttrName));
		Accessor.GetAttributeStrings(Owner, SplitAttributes);
	}

	return TTuple<FString, FHoudiniEngineIndexedStringMap>(SplitAttrName, SplitAttributes);
}

bool FHoudiniInstanceTranslator::GetPackedPrimitiveInstancerPartData(
	const FHoudiniGeoPartObject& HGPO, 
	const TArray<UHoudiniOutput*>& InAllOutputs, 
	FHoudiniInstancerPartData& PartData)
{
	PartData.GeoPartObject = HGPO;

	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), HGPO.GeoId, HGPO.PartId, &PartInfo), {});

	//---------------------------------------------------------------------------------------------------------------------------
	// Create a unique instancer for each object/split group for each part being instanced in the part instance.
	//---------------------------------------------------------------------------------------------------------------------------

	TTuple<FString, FHoudiniEngineIndexedStringMap> SplitData = GetSplitData(HGPO, HAPI_AttributeOwner::HAPI_ATTROWNER_PRIM);

	TMap<int64_t, int> InstancerMap;
	for (int PartInstance = 0; PartInstance < PartInfo.instancedPartCount; PartInstance++)
	{
		for (int InstanceIndex = 0; InstanceIndex < PartInfo.instanceCount; InstanceIndex++)
		{

			int64_t Id = static_cast<int64_t>(PartInstance) << 32;
			if (!SplitData.Value.Ids.IsEmpty())
			{
				Id += SplitData.Value.Ids[InstanceIndex];
			}
			if (!InstancerMap.Contains(Id))
			{
				InstancerMap.Add(Id, PartData.Instancers.Num());
				PartData.Instancers.Add({});
			}

			FHoudiniInstancer& Instancer = PartData.Instancers[InstancerMap[Id]];
			Instancer.AttributeIndices.Add(InstanceIndex);
		}
	}

	FHoudiniInstancerSettings DefaultSettings = GetDefaultInstancerSettings(HGPO, HAPI_ATTROWNER_DETAIL);

	TArray<int> InstancedPartIds;
	InstancedPartIds.SetNum(PartInfo.instancedPartCount);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetInstancedPartIds(FHoudiniEngine::Get().GetSession(),
		HGPO.GeoId, HGPO.PartInfo.PartId,
		InstancedPartIds.GetData(), 0, PartInfo.instancedPartCount), {});

	for (auto It : InstancerMap)
	{
		int32 PartNumber = It.Key >> 32;
		int32 SplitStringHandle = It.Key & INT_MAX;

		FHoudiniInstancer& Instancer = PartData.Instancers[It.Value];
		Instancer.SplitName = FString::Printf(TEXT("%d"), PartNumber);
		if (!SplitData.Value.Ids.IsEmpty())
		{
			Instancer.SplitName += TEXT("_");
			Instancer.SplitName += SplitData.Value.Strings[SplitStringHandle];
		}

		int FirstIndex = Instancer.AttributeIndices[0];
		Instancer.Settings = GetInstancerSettings(HGPO, HAPI_ATTROWNER_PRIM, FirstIndex, DefaultSettings);

		FHoudiniGeoPartObject InstancedHGPO = HGPO;
		InstancedHGPO.PartId = InstancedPartIds[PartNumber];
		SetInstancerObject(Instancer, InstancedHGPO, InAllOutputs);

	}


	PartData.InstanceTransforms = GetInstancerTransforms(HGPO);

	return true;
}



bool
FHoudiniInstanceTranslator::GetAttributeInstancerPartData(
	const FHoudiniGeoPartObject& HGPO,
	FHoudiniInstancerPartData & PartData)
{
	PartData.GeoPartObject = HGPO;

	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), HGPO.GeoId, HGPO.PartId, &PartInfo), {});

	//---------------------------------------------------------------------------------------------------------------------------
	// Set UnrealInstanceObjects to the names (object refs) of all the objects that will be instantiated. One per point.
	//---------------------------------------------------------------------------------------------------------------------------

	FHoudiniEngineIndexedStringMap UnrealInstanceObjects;
	FHoudiniHapiAccessor Accessor;
	Accessor.Init(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_INSTANCE_OVERRIDE);
	Accessor.GetAttributeStrings(HAPI_ATTROWNER_POINT, UnrealInstanceObjects);
	if (UnrealInstanceObjects.Ids.IsEmpty())
	{
		Accessor.Init(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_INSTANCE);
		Accessor.GetAttributeStrings(HAPI_ATTROWNER_DETAIL, UnrealInstanceObjects);
	}

	if (UnrealInstanceObjects.Ids.IsEmpty())
	{
		Accessor.Init(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_INSTANCE_OVERRIDE);
		FString DetailString;
		Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_DETAIL, DetailString);

		UnrealInstanceObjects.Ids.SetNum(PartInfo.pointCount);
		UnrealInstanceObjects.Strings.Add(DetailString);
	}

	//---------------------------------------------------------------------------------------------------------------------------
	// Create a unique instancer for each object/split group. Combine the split group id and object id into a single int64 for
	// quick look up
	//---------------------------------------------------------------------------------------------------------------------------

	TTuple<FString, FHoudiniEngineIndexedStringMap> SplitData = GetSplitData(HGPO, HAPI_AttributeOwner::HAPI_ATTROWNER_POINT);

	TMap<int64_t, int> InstancerMap;
	for(int PointIndex = 0; PointIndex < PartInfo.pointCount; PointIndex++)
	{
		int StringIndex = UnrealInstanceObjects.Ids[PointIndex];

		int64_t Id = static_cast<int64_t>(UnrealInstanceObjects.Ids[PointIndex]) << 32;
		if (!SplitData.Value.Ids.IsEmpty())
		{
			Id += SplitData.Value.Ids[PointIndex];
		}
		if (!InstancerMap.Contains(Id))
		{
			InstancerMap.Add(Id, PartData.Instancers.Num());
			PartData.Instancers.Add({});
		}

		FHoudiniInstancer & Instancer = PartData.Instancers[InstancerMap[Id]];
		Instancer.AttributeIndices.Add(PointIndex);
	}

	//---------------------------------------------------------------------------------------------------------------------------
	// Pull all instancer data from Houdini
	//---------------------------------------------------------------------------------------------------------------------------

	FHoudiniInstancerSettings DefaultSettings = GetDefaultInstancerSettings(HGPO, HAPI_ATTROWNER_DETAIL);

	for(auto It : InstancerMap)
	{
		int32 ObjectStringHandle = It.Key >> 32;
		int32 SplitStringHandle = It.Key & INT_MAX;

		FHoudiniInstancer & Instancer = PartData.Instancers[It.Value];

		Instancer.ObjectPath = UnrealInstanceObjects.Strings[ObjectStringHandle];
		Instancer.SplitName = FString::Printf(TEXT("%d"), ObjectStringHandle);
		if (!SplitData.Value.Ids.IsEmpty())
		{
			Instancer.SplitName += TEXT("_");
			Instancer.SplitName += SplitData.Value.Strings[SplitStringHandle];
		}

		int FirstIndex = Instancer.AttributeIndices[0];
		Instancer.Settings = GetInstancerSettings(HGPO, HAPI_ATTROWNER_POINT, FirstIndex, DefaultSettings);
	}

	//---------------------------------------------------------------------------------------------------------------------------
	// Pull part data
	//---------------------------------------------------------------------------------------------------------------------------

	HapiGetInstanceTransforms(HGPO, PartData.InstanceTransforms);

	return true;
}

UObject*
FHoudiniInstanceTranslator::LoadInstancedObject(const FString & ObjectPath)
{
	// Load the object using its path. Resolve redirectors if necessary.
	UObject * InstanceObject = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath, nullptr, LOAD_None, nullptr);

	while (UObjectRedirector* Redirector = Cast<UObjectRedirector>(InstanceObject))
		InstanceObject = Redirector->DestinationObject;

	if (IsValid(InstanceObject))
		return InstanceObject;

	// If could not load the actor, try to load it as a class.
	UClass* FoundClass = FHoudiniEngineRuntimeUtils::GetClassByName(ObjectPath);
	if (FoundClass != nullptr)
	{
		// TODO: ensure we'll be able to create an actor from this class! 
		InstanceObject = FoundClass;
	}

	if (IsValid(InstanceObject))
		return InstanceObject;

	// We failed to load anything. Try the default Houdini object if enabled.
	if (GetDefault< UHoudiniRuntimeSettings>()->bShowDefaultMesh)
	{
		HOUDINI_LOG_WARNING(TEXT("Failed to load instanced object '%s', using default instance mesh (hidden in game)."), *(ObjectPath));

		// Couldn't load the referenced object, use the default reference mesh
		UStaticMesh* DefaultReferenceSM = FHoudiniEngine::Get().GetHoudiniDefaultReferenceMesh().Get();
		if (!IsValid(DefaultReferenceSM))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to load the default instance mesh."));
		}
		InstanceObject = DefaultReferenceSM;
	}
	return InstanceObject;
}


bool 
FHoudiniInstanceTranslator::CreateInstancer(
	const FHoudiniOutputObjectIdentifier& Id,
	FHoudiniOutputObject & Output,
	UObject* & InstanceObject,
	const FHoudiniInstancer& Instancers,
	const FHoudiniInstancerPartData& InstancerPartData,
	const FHoudiniPackageParams& PackageParams,
	USceneComponent* ParentComponent,
	const TArray<UMaterialInterface *>& InstancerMaterials)
{
	// See what type of component we want to create
	InstancerComponentType InstancerType = InstancerComponentType::Invalid;

	InstanceObject = LoadInstancedObject(*Instancers.ObjectPath);

	while (UObjectRedirector* Redirector = Cast<UObjectRedirector>(InstanceObject))
		InstanceObject = Redirector->DestinationObject;

	if (!IsValid(InstanceObject))
	{
		HOUDINI_LOG_ERROR(TEXT("Could not find %s"), *Instancers.ObjectPath);
		return {};
	}

	if (InstanceObject->IsA<UFoliageType>() || Instancers.Settings.bIsFoliage)
	{
		// We must test for foliage type first, or FT will be considered as meshes
		InstancerType = Foliage;
	}
	else if(InstanceObject->IsA<UStaticMesh>())
	{
		bool bMustUseInstancerComponent = Instancers.AttributeIndices.Num() > 1 || Instancers.Settings.bForceInstancer;

		// It is recommended to avoid putting Nanite mesh in HISM since they have their own LOD mechanism.
		// Will also improve performance by avoiding access to the render data to fetch the LOD count which could
		// trigger an async mesh wait until it has been computed.

		UStaticMesh* StaticMesh = Cast<UStaticMesh>(InstanceObject);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
		if (!StaticMesh->IsNaniteEnabled() && (Instancers.Settings.bForceHISM || (bMustUseInstancerComponent && StaticMesh->GetNumLODs() > 1)))
#else
		if (!StaticMesh->NaniteSettings.bEnabled && (Instancers.Settings.bForceHISM || (bMustUseInstancerComponent && StaticMesh->GetNumLODs() > 1)))
#endif
			InstancerType = HierarchicalInstancedStaticMeshComponent;
		else if (bMustUseInstancerComponent)
			InstancerType = InstancedStaticMeshComponent;
		else
			InstancerType = StaticMeshComponent;
	}
	else if (InstanceObject->IsA<UHoudiniStaticMesh>())
	{
		if (Instancers.AttributeIndices.Num() == 1)
		{
			InstancerType = HoudiniStaticMeshComponent;
		}
		else 
		{
			HOUDINI_LOG_ERROR(TEXT("More than one instance transform encountered for UHoudiniStaticMesh: %s"), *(InstanceObject->GetPathName()));
			InstancerType = Invalid;
			return {};
		}
	}
	else if (InstanceObject->IsA<UWorld>())
	{
		if (Instancers.Settings.bIsFoliage)
		{
			HOUDINI_LOG_ERROR(TEXT("Cannot use a level instance as foliage"));
			return {};
		}
		InstancerType = LevelInstance;
	}
	else
	{
		InstancerType = HoudiniInstancedActorComponent;
	}

	bool bCheckRenderState = false;
	bool bSuccess = false;

	switch (InstancerType)
	{
		case InstancedStaticMeshComponent:
		case HierarchicalInstancedStaticMeshComponent:
		{
			// Create an Instanced Static Mesh Component
			bSuccess = CreateInstancedStaticMeshInstancer(
				Output,
				Instancers,
				InstanceObject,
				InstancerPartData, 
				ParentComponent, 
				InstancerMaterials);
			bCheckRenderState = true;
		}
		break;

		case HoudiniInstancedActorComponent:
		{
			bSuccess = CreateInstancedActorInstancer(
				Output,
				Instancers, 
				InstanceObject,
				InstancerPartData, 
				ParentComponent);
		}
		break;

		case StaticMeshComponent:
		{
			// Create a Static Mesh Component
			bSuccess = CreateStaticMeshInstancer(
				Output,
				Instancers,
				InstanceObject,
				InstancerPartData, 
				ParentComponent, 
				InstancerMaterials);
			bCheckRenderState = true;
		}
		break;

		case HoudiniStaticMeshComponent:
		{
			// Create a Houdini Static Mesh Component
			bSuccess = CreateHoudiniStaticMeshInstancer(
				Output,
				Instancers,
				InstanceObject,
				InstancerPartData, 
				ParentComponent, 
				InstancerMaterials);
		}
		break;

		case Foliage:
		{
			bSuccess = CreateFoliageInstancer(
				Id,
				Output,
				Instancers,
				InstanceObject,
				InstancerPartData, 
				PackageParams,
				ParentComponent, 
				InstancerMaterials);

		}
		break;
		case LevelInstance:
		{
			// Create a Houdini Static Mesh Component
			bSuccess = CreateLevelInstanceInstancer(
				Output,
				Instancers,
				InstanceObject,
				InstancerPartData, 
				ParentComponent, 
				InstancerMaterials);
		}
		break;
	default:

		break;
	}

	if (!bSuccess)
		return false;

	for(auto Object : Output.OutputComponents)
	{
		USceneComponent * NewComponentToSet = Cast<USceneComponent>(Object);

		// UE5: Make sure we update/recreate the Component's render state
	    // after the update or the mesh component will not be rendered!
	    if (bCheckRenderState)
	    {
		    UMeshComponent* NewMeshComponent = Cast<UMeshComponent>(NewComponentToSet);
		    if (IsValid(NewMeshComponent))
		    {
			    if (NewMeshComponent->IsRenderStateCreated())
			    {
				    // Need to send this to render thread at some point
				    NewMeshComponent->MarkRenderStateDirty();
			    }
			    else if (NewMeshComponent->ShouldCreateRenderState())
			    {
				    // If we didn't have a valid StaticMesh assigned before
				    // our render state might not have been created so
				    // do it now.
				    NewMeshComponent->RecreateRenderState_Concurrent();
			    }
		    }
	    }

		NewComponentToSet->SetMobility(ParentComponent->Mobility);

		if (InstancerType != Foliage && InstancerType != LevelInstance)
		    NewComponentToSet->AttachToComponent(ParentComponent, FAttachmentTransformRules::KeepRelativeTransform);

	    // Only register if we have a valid component
	    if (NewComponentToSet->GetOwner() && NewComponentToSet->GetWorld())
			NewComponentToSet->RegisterComponent();
	}

	if (!Instancers.bVisible)
	{
		for (auto Object : Output.OutputComponents)
		{
			USceneComponent* InstancerComponent = Cast<USceneComponent>(Object);
			InstancerComponent->SetVisibleFlag(false);
		}
	}

	UStaticMesh* DefaultReferenceSM = FHoudiniEngine::Get().GetHoudiniDefaultReferenceMesh().Get();

	for (auto Object : Output.OutputComponents)
	{
		USceneComponent* InstancerComponent = Cast<USceneComponent>(Object);
		SetPerInstanceCustomData(Instancers, InstancerPartData, InstancerComponent);

		// If the instanced object (by ref) wasn't found, hide the component in game
		if (Output.OutputObject == DefaultReferenceSM)
		{
			InstancerComponent->SetHiddenInGame(true);
		}
		else
		{
			// TODO: Revisit why this is need.
			// See if the HiddenInGame property is overriden
			bool bOverridesHiddenInGame = false;
			for (auto& CurPropAttr : InstancerPartData.AllPropertyAttributes)
			{
				if (CurPropAttr.AttributeName.Equals(TEXT("HiddenInGame")) || CurPropAttr.AttributeName.Equals(TEXT("bHiddenInGame")))
				{
					bOverridesHiddenInGame = true;
					break;
				}
			}

			// Don't force the property if it is overriden by generic attributes
			if (!bOverridesHiddenInGame)
				InstancerComponent->SetHiddenInGame(false);
		}
	}

	Output.CachedAttributes.Empty();
	Output.CachedTokens.Empty();

	if (!Instancers.Settings.LevelPath.IsEmpty())
	{
		Output.CachedAttributes.Add(HAPI_UNREAL_ATTRIB_LEVEL_PATH, Instancers.Settings.LevelPath);
	}
	if (!Instancers.Settings.OutputName.IsEmpty())
	{
		Output.CachedAttributes.Add(HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2, Instancers.Settings.OutputName);
	}
	if (!Instancers.Settings.BakeActorClassName.IsEmpty())
	{
		Output.CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_ACTOR_CLASS, Instancers.Settings.BakeActorClassName);
	}
	if (!Instancers.Settings.BakeActorName.IsEmpty())
	{
		Output.CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_ACTOR, Instancers.Settings.BakeActorName);
	}
	if (!Instancers.Settings.BakeOutlinerFolder.IsEmpty())
	{
		Output.CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_OUTLINER_FOLDER, Instancers.Settings.BakeOutlinerFolder);
	}
	if (!Instancers.Settings.BakeFolder.IsEmpty())
	{
		Output.CachedAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_FOLDER, Instancers.Settings.BakeFolder);
	}

	Output.DataLayers = Instancers.Settings.DataLayers;
	Output.HLODLayers = Instancers.Settings.HLODLayers;

	// For Houdini Mesh Proxy - we need to make sure the HSMC is only set on the output's proxy component
	if (InstancerType == HoudiniStaticMeshComponent && Output.ProxyComponent != nullptr)
	{
		Output.OutputComponents.Empty();
	}

	return true;
}

bool
FHoudiniInstanceTranslator::CreateInstancedStaticMeshInstancer(
	FHoudiniOutputObject& Output,
	const FHoudiniInstancer& Instancer,
	UObject* InstanceObject,
	const FHoudiniInstancerPartData& InstancerPartData,
	USceneComponent* ParentComponent,
	const TArray<UMaterialInterface*>& InstancerMaterials)
{

	UStaticMesh* InstancedStaticMesh = Cast<UStaticMesh>(InstanceObject);

	if (!InstancedStaticMesh)
		return false;

	if (!IsValid(ParentComponent))
		return false;

	UObject* ComponentOuter = ParentComponent;
	if (IsValid(ParentComponent->GetOwner()))
		ComponentOuter = ParentComponent->GetOwner();

	UInstancedStaticMeshComponent* InstancedStaticMeshComponent = nullptr;

	// It is recommended to avoid putting Nanite mesh in HISM since they have their own LOD mecanism.
	// Will also improve performance by avoiding access to the render data to fetch the LOD count which could
	// trigger an async mesh wait until it has been computed.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
	if (!InstancedStaticMesh->IsNaniteEnabled() && (InstancedStaticMesh->GetNumLODs() > 1 || Instancer.Settings.bForceHISM))
#else
	if (!InstancedStaticMesh->NaniteSettings.bEnabled && (InstancedStaticMesh->GetNumLODs() > 1 || Instancer.Settings.bForceHISM))
#endif
	{
		// If the mesh has LODs, use Hierarchical ISMC
		InstancedStaticMeshComponent = NewObject<UHierarchicalInstancedStaticMeshComponent>(
			ComponentOuter, UHierarchicalInstancedStaticMeshComponent::StaticClass(), NAME_None, RF_Transactional);
	}
	else
	{
		// If the mesh doesnt have LOD, we can use a regular ISMC
		InstancedStaticMeshComponent = NewObject<UInstancedStaticMeshComponent>(
			ComponentOuter, UInstancedStaticMeshComponent::StaticClass(), NAME_None, RF_Transactional);
	}

	// Change the creation method so the component is listed in the details panels
	if (InstancedStaticMeshComponent)
		FHoudiniEngineRuntimeUtils::AddOrSetAsInstanceComponent(InstancedStaticMeshComponent);
	else
		return false;

	Output.OutputComponents.Add(InstancedStaticMeshComponent);

	FHoudiniEngineUtils::KeepOrClearComponentTags(InstancedStaticMeshComponent, &InstancerPartData.GeoPartObject);

	InstancedStaticMeshComponent->SetStaticMesh(InstancedStaticMesh);

	if (InstancedStaticMeshComponent->GetBodyInstance())
	{
		InstancedStaticMeshComponent->GetBodyInstance()->bAutoWeld = false;
	}

	InstancedStaticMeshComponent->OverrideMaterials.Empty();
	if (InstancerMaterials.Num() > 0)
	{
		int32 MeshMaterialCount = InstancedStaticMesh->GetStaticMaterials().Num();
		for (int32 Idx = 0; Idx < MeshMaterialCount; ++Idx)
		{
			if (InstancerMaterials.IsValidIndex(Idx) && IsValid(InstancerMaterials[Idx]))
				InstancedStaticMeshComponent->SetMaterial(Idx, InstancerMaterials[Idx]);
		}
	}

	// Set the transform of the Component relative to its parents.
	InstancedStaticMeshComponent->SetRelativeTransform(Instancer.Settings.ComponentRelativeTransform);

	// Get the transforms of all instances. In Houdini the origin is relative to the HDA Component in Unreal.
	TArray<FTransform> Transforms = UnpackTransforms(Instancer, InstancerPartData);

	// Offset all transforms relative to the component.
	FTransform InvComponentTransform = Instancer.Settings.ComponentRelativeTransform.Inverse();
	for(FTransform & Transform : Transforms)
	{
		Transform = InvComponentTransform * Transform;
	}

	InstancedStaticMeshComponent->AddInstances(Transforms, false);

	// Apply generic attributes if we have any. Just use attributes on the first point.
	FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(InstancedStaticMeshComponent, InstancerPartData.AllPropertyAttributes, Instancer.AttributeIndices[0]);
	return true;
}

bool
FHoudiniInstanceTranslator::CreateInstancedActorInstancer(
	FHoudiniOutputObject& Output,
	const FHoudiniInstancer& Instancer,
	UObject* InstanceObject,
	const FHoudiniInstancerPartData& InstancerPartData,
	USceneComponent* ParentComponent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInstanceTranslator::CreateInstancedActorInstancer);

	if (!InstanceObject)
		return false;

	if (!IsValid(ParentComponent))
		return false;

	// Get the level where we want to spawn the actors
	ULevel* SpawnLevel = ParentComponent->GetOwner() ? ParentComponent->GetOwner()->GetLevel() : nullptr;
	if (!SpawnLevel)
		return {};

	UObject* ComponentOuter = ParentComponent;
	if (IsValid(ParentComponent->GetOwner()))
		ComponentOuter = ParentComponent->GetOwner();

	UHoudiniInstancedActorComponent* InstancedActorComponent = NewObject<UHoudiniInstancedActorComponent>(
		ComponentOuter, UHoudiniInstancedActorComponent::StaticClass(), NAME_None, RF_Transactional);

	Output.OutputComponents.Add(InstancedActorComponent);

	// Change the creation method so the component is listed in the details panels
	FHoudiniEngineRuntimeUtils::AddOrSetAsInstanceComponent(InstancedActorComponent);

	FHoudiniEngineUtils::KeepOrClearComponentTags(InstancedActorComponent, &InstancerPartData.GeoPartObject);

	InstancedActorComponent->ClearAllInstances();
	InstancedActorComponent->SetInstancedObject(InstanceObject);

	// Set the number of needed instances
	TArray<FTransform> Transforms = UnpackTransforms(Instancer, InstancerPartData);
	InstancedActorComponent->SetNumberOfInstances(Transforms.Num());

	AActor* ReferenceActor = nullptr;
	for (int32 Idx = 0; Idx < Transforms.Num(); Idx++)
	{
		// if we already have an actor, we can reuse it
		const FTransform& CurTransform = Transforms[Idx];

		// Get the current instance
		// If null, we need to create a new one, else we can reuse the actor
		// TODO: ?? we cant reuse previous actors since we clear everything above?
		AActor* CurInstance = InstancedActorComponent->GetInstancedActorAt(Idx);
		if (!IsValid(CurInstance))
		{
			CurInstance = SpawnInstanceActor(CurTransform, SpawnLevel, InstancedActorComponent, ReferenceActor);
			InstancedActorComponent->SetInstanceAt(Idx, CurTransform, CurInstance);
		}
		else
		{
			// We can simply update the actor's transform
			InstancedActorComponent->SetInstanceTransformAt(Idx, CurTransform);	
		}

		if (!ReferenceActor)
			ReferenceActor = CurInstance;

		FHoudiniEngineUtils::KeepOrClearActorTags(CurInstance, true, true, &InstancerPartData.GeoPartObject);

		FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(CurInstance, 
			InstancerPartData.AllPropertyAttributes, Instancer.AttributeIndices[Idx]);
	}

	// Update generic properties for the component managing the instances
	FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(InstancedActorComponent, InstancerPartData.AllPropertyAttributes, Instancer.AttributeIndices[0]);

	// Make sure Post edit change is called on all generated actors
	TArray<AActor*> NewActors = InstancedActorComponent->GetInstancedActors();
	for (auto& CurActor : NewActors)
	{
		if (CurActor)
			CurActor->PostEditChange();
	}

	return true;
}

bool
FHoudiniInstanceTranslator::CreateStaticMeshInstancer(
	FHoudiniOutputObject& Output,
	const FHoudiniInstancer& Instancers,
	UObject* InstanceObject,
	const FHoudiniInstancerPartData& InstancerPartData,
	USceneComponent* ParentComponent,
	const TArray<UMaterialInterface*>& InstancerMaterials)
{
	UStaticMesh* InstancedStaticMesh = Cast<UStaticMesh>(InstanceObject);

	if (!InstancedStaticMesh)
		return false;

	if (!IsValid(ParentComponent))
		return false;

	UObject* ComponentOuter = ParentComponent;
	if (IsValid(ParentComponent->GetOwner()))
		ComponentOuter = ParentComponent->GetOwner();

	UStaticMeshComponent* SMC = NewObject<UStaticMeshComponent>(ComponentOuter, UStaticMeshComponent::StaticClass(), NAME_None, RF_Transactional);

	Output.OutputComponents.Add(SMC);

	// Change the creation method so the component is listed in the details panels
	FHoudiniEngineRuntimeUtils::AddOrSetAsInstanceComponent(SMC);

	if (!SMC)
		return {};

	SMC->SetStaticMesh(InstancedStaticMesh);
	SMC->GetBodyInstance()->bAutoWeld = false;
	
	FHoudiniEngineUtils::KeepOrClearComponentTags(SMC, &InstancerPartData.GeoPartObject);

	SMC->OverrideMaterials.Empty();
	if (InstancerMaterials.Num() > 0)
	{
		int32 MeshMaterialCount = InstancedStaticMesh->GetStaticMaterials().Num();
		for (int32 Idx = 0; Idx < MeshMaterialCount; ++Idx)
		{
			if (InstancerMaterials.IsValidIndex(Idx) && IsValid(InstancerMaterials[Idx]))
				SMC->SetMaterial(Idx, InstancerMaterials[Idx]);
		}
	}

	TArray<FTransform> Transforms = UnpackTransforms(Instancers, InstancerPartData);

	// Now add the instances Transform
	if (Transforms.Num() > 0)
	{
		SMC->SetRelativeTransform(Transforms[0]);
	}

	SetGenericPropertyAttributes(SMC, Instancers, InstancerPartData);

	return true;
}

bool
FHoudiniInstanceTranslator::CreateHoudiniStaticMeshInstancer(
	FHoudiniOutputObject& Output,
	const FHoudiniInstancer& Instancers,
	UObject* InstanceObject,
	const FHoudiniInstancerPartData& InstancerPartData,
	USceneComponent* ParentComponent,
	const TArray<UMaterialInterface*>& InstancerMaterials)
{
 
	UHoudiniStaticMesh * InstancedProxyStaticMesh = Cast<UHoudiniStaticMesh>(InstanceObject);

	if (!InstancedProxyStaticMesh)
		return false;

	if (!IsValid(ParentComponent))
		return false;

	UObject* ComponentOuter = ParentComponent;
	if (IsValid(ParentComponent->GetOwner()))
		ComponentOuter = ParentComponent->GetOwner();

	UHoudiniStaticMeshComponent* HSMC = NewObject<UHoudiniStaticMeshComponent>(ComponentOuter, UHoudiniStaticMeshComponent::StaticClass(), NAME_None, RF_Transactional);
	Output.ProxyComponent = HSMC;
	Output.OutputComponents.Add(HSMC);

	// Change the creation method so the component is listed in the details panels
	FHoudiniEngineRuntimeUtils::AddOrSetAsInstanceComponent(HSMC);

	if (!HSMC)
		return {}; 

	HSMC->SetMesh(InstancedProxyStaticMesh);

	FHoudiniEngineUtils::KeepOrClearComponentTags(HSMC, &InstancerPartData.GeoPartObject);
	
	HSMC->OverrideMaterials.Empty();
	if (InstancerMaterials.Num() > 0)
	{
		int32 MeshMaterialCount = InstancedProxyStaticMesh->GetStaticMaterials().Num();
		for (int32 Idx = 0; Idx < MeshMaterialCount; ++Idx)
		{
			if (InstancerMaterials.IsValidIndex(Idx) && IsValid(InstancerMaterials[Idx]))
				HSMC->SetMaterial(Idx, InstancerMaterials[Idx]);
		}
	}

	// Now add the instances Transform
	TArray<FTransform> Transforms = UnpackTransforms(Instancers, InstancerPartData);
	HSMC->SetRelativeTransform(Transforms[0]);

	// Apply generic attributes if we have any
	SetGenericPropertyAttributes(HSMC, Instancers, InstancerPartData);
	return true;
}


bool
FHoudiniInstanceTranslator::CreateFoliageInstancer(
	const FHoudiniOutputObjectIdentifier& Id,
	FHoudiniOutputObject& Output,
	const FHoudiniInstancer& Instancers,
	UObject* InstanceObject,
	const FHoudiniInstancerPartData& InstancerPartData,
	const FHoudiniPackageParams& InPackageParams,
	USceneComponent* ParentComponent,
	const TArray<UMaterialInterface*>& InstancerMaterials)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(InstanceObject);
	UFoliageType* FoliageType = Cast<UFoliageType>(InstanceObject);

	HOUDINI_CHECK_RETURN(IsValid(StaticMesh) || IsValid(FoliageType), {});
	HOUDINI_CHECK_RETURN(IsValid(ParentComponent), {});

	AActor* OwnerActor = ParentComponent->GetOwner();
	HOUDINI_CHECK_RETURN(IsValid(OwnerActor), {});

	// We want to spawn the foliage in the same level as the parent HDA
	// as spawning in the current level may cause reference issue later on.
	ULevel* DesiredLevel = OwnerActor->GetLevel();
	HOUDINI_CHECK_RETURN(IsValid(DesiredLevel), {});

	Output.World = DesiredLevel->GetWorld();
	HOUDINI_CHECK_RETURN(IsValid(Output.World), {});

    // Previously, (pre 2023) we used to try to find existing foliage types in the current world, but this is dangerous
	// because it can trash the users data if they non-HDA foliage. This can get fairly confusing if there are two HDA
	// in the same level, and doesn't make it clear what is baked where. So always create a custom foliage type.

    FHoudiniPackageParams FoliageTypePackageParams =  InPackageParams;

	if (FoliageType)
	{
		Output.FoliageType = FHoudiniFoliageTools::DuplicateFoliageType(FoliageTypePackageParams, Id, FoliageType);
		Output.UserFoliageType = FoliageType;
	}
	else
	{
		Output.FoliageType = FHoudiniFoliageTools::CreateFoliageType(FoliageTypePackageParams, Id, StaticMesh);
		Output.UserFoliageType = nullptr;
	}

	// Set material overrides on the cooked foliage type
	if (InstancerMaterials.Num() > 0)
	{
		UFoliageType_InstancedStaticMesh* const CookedMeshFoliageType = Cast<UFoliageType_InstancedStaticMesh>(Output.FoliageType);
		if (IsValid(CookedMeshFoliageType))
		{
			UStaticMesh const* const FoliageMesh = CookedMeshFoliageType->GetStaticMesh();
			const int32 MeshMaterialSlotCount = IsValid(FoliageMesh) ? FoliageMesh->GetStaticMaterials().Num() : 0;
			const int32 MaterialOverrideSlotCount = FMath::Min(InstancerMaterials.Num(), MeshMaterialSlotCount);
			for (int32 Idx = 0; Idx < MaterialOverrideSlotCount; ++Idx)
			{
				if (IsValid(InstancerMaterials[Idx]))
				{
					if (!CookedMeshFoliageType->OverrideMaterials.IsValidIndex(Idx))
						CookedMeshFoliageType->OverrideMaterials.SetNum(Idx + 1);
					CookedMeshFoliageType->OverrideMaterials[Idx] = InstancerMaterials[Idx];
				}
				else if (CookedMeshFoliageType->OverrideMaterials.IsValidIndex(Idx) && CookedMeshFoliageType->OverrideMaterials[Idx])
				{
					CookedMeshFoliageType->OverrideMaterials[Idx] = nullptr;
				}
			}
		}
	}
	
	FTransform HoudiniAssetTransform = ParentComponent->GetComponentTransform();

	TArray<FTransform> Transforms = UnpackTransforms(Instancers, InstancerPartData);

	TArray<FFoliageInstance> FoliageInstances;
	FoliageInstances.SetNum(Transforms.Num());

	//for (auto CurrentTransform : InstancedObjectTransforms)
	for(int32 n = 0; n < Transforms.Num(); n++)
	{
		// Instances transforms are relative to the HDA, 
		// But we need world transform for the Foliage Types
		FTransform CurrentTransform = Transforms[n] * HoudiniAssetTransform;

		FoliageInstances[n].Location = CurrentTransform.GetLocation();
		FoliageInstances[n].Rotation = CurrentTransform.GetRotation().Rotator();
		FoliageInstances[n].DrawScale3D = (FVector3f)CurrentTransform.GetScale3D();
	}

	TArray<FFoliageAttachmentInfo> AttachmentTypes = 
		FHoudiniFoliageTools::GetAttachmentInfo(InstancerPartData.GeoPartObject.GeoId, InstancerPartData.GeoPartObject.PartId, FoliageInstances.Num());

	FHoudiniFoliageTools::SpawnFoliageInstances(Output.World, Output.FoliageType, FoliageInstances, AttachmentTypes);

	// Clear the returned component. This should be set, but doesn't make in world partition.
	// In future, this should be an array of components.

	TArray<FFoliageInfo*> FoliageInfos = FHoudiniFoliageTools::GetAllFoliageInfo(DesiredLevel->GetWorld(), Output.FoliageType);
	for(FFoliageInfo * FoliageInfo : FoliageInfos)
	{

	    UHierarchicalInstancedStaticMeshComponent* FoliageHISMC = FoliageInfo->GetComponent();	
	    if (IsValid(FoliageHISMC))
	    {
		    // TODO: This was due to a bug in UE4.22-20, check if still needed! 
		    FoliageHISMC->BuildTreeIfOutdated(true, true);

	    	FHoudiniEngineUtils::KeepOrClearComponentTags(FoliageHISMC, &InstancerPartData.GeoPartObject);

			Output.OutputComponents.Add(FoliageHISMC);

			SetGenericPropertyAttributes(FoliageHISMC, Instancers, InstancerPartData);
	    }
	}

	SetGenericPropertyAttributes(StaticMesh, Instancers, InstancerPartData);
	SetGenericPropertyAttributes(Output.FoliageType, Instancers, InstancerPartData);

	return true;
}

bool
FHoudiniInstanceTranslator::CreateLevelInstanceInstancer(
	FHoudiniOutputObject& Output,
	const FHoudiniInstancer& Instancers,
	UObject* InstanceObject,
	const FHoudiniInstancerPartData& InstancerPartData,
	USceneComponent* ParentComponent,
	const TArray<UMaterialInterface*>& InstancerMaterials)
{
	UWorld* LevelInstanceWorld = Cast<UWorld>(InstanceObject);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	UWorld* SpawnWorld = ParentComponent->GetWorld();

	TArray<FTransform> Transforms = UnpackTransforms(Instancers, InstancerPartData);

	for(int Index = 0; Index < Transforms.Num(); Index++)
	{
		FTransform HoudiniAssetTransform = ParentComponent->GetComponentTransform();
		FTransform CurrentTransform = Transforms[Index] * HoudiniAssetTransform;
		FString Name = FString::Printf(TEXT("%s_%d_%d_%d_%d"),
			*InstancerPartData.GeoPartObject.ObjectName,
			InstancerPartData.GeoPartObject.ObjectId,
			InstancerPartData.GeoPartObject.GeoId,
			InstancerPartData.GeoPartObject.PartId,
			Index);

		FActorSpawnParameters SpawnInfo;
		SpawnInfo.Name = FName(Name);
		SpawnInfo.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		SpawnInfo.Owner = ParentComponent->GetOwner();
		ALevelInstance* LevelInstance = Cast<ALevelInstance>(SpawnWorld->SpawnActor(ALevelInstance::StaticClass(), &CurrentTransform, SpawnInfo));
		LevelInstance->bDefaultOutlinerExpansionState = false;
		LevelInstance->SetWorldAsset(LevelInstanceWorld);
		LevelInstance->LoadLevelInstance();
		LevelInstance->SetActorLabel(Name);
		LevelInstance->AttachToActor(ParentComponent->GetOwner(), FAttachmentTransformRules::KeepWorldTransform);

		SetGenericPropertyAttributes(LevelInstance, Instancers, InstancerPartData);

		Output.OutputActors.Add(LevelInstance);
	}

	return true;
#else
	return false;
#endif
}

bool
FHoudiniInstanceTranslator::HapiGetInstanceTransforms(
	const FHoudiniGeoPartObject& InHGPO, 
	TArray<FTransform>& OutInstancerUnrealTransforms)
{
	// Get the instance transforms	
	int32 PointCount = InHGPO.PartInfo.PointCount;
	if (PointCount <= 0)
		return false;

	TArray<HAPI_Transform> InstanceTransforms;
	InstanceTransforms.SetNum(PointCount);
	for (int32 Idx = 0; Idx < InstanceTransforms.Num(); Idx++)
		FHoudiniApi::Transform_Init(&(InstanceTransforms[Idx]));

	auto Result =  FHoudiniApi::GetInstanceTransformsOnPart(FHoudiniEngine::Get().GetSession(),
		InHGPO.GeoId, InHGPO.PartId, HAPI_SRT, &InstanceTransforms[0], 0, PointCount);

	if(Result == HAPI_RESULT_SUCCESS)
	{
		// Convert the transform to Unreal's coordinate system
		OutInstancerUnrealTransforms.SetNumZeroed(InstanceTransforms.Num());
		for(int32 InstanceIdx = 0; InstanceIdx < InstanceTransforms.Num(); InstanceIdx++)
		{
			const auto& InstanceTransform = InstanceTransforms[InstanceIdx];
			FHoudiniEngineUtils::TranslateHapiTransform(InstanceTransform, OutInstancerUnrealTransforms[InstanceIdx]);
		}
	}
	else
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to fetch instance transforms."));
		OutInstancerUnrealTransforms.SetNum(InstanceTransforms.Num());
		for(int32 InstanceIdx = 0; InstanceIdx < InstanceTransforms.Num(); InstanceIdx++)
		{
			OutInstancerUnrealTransforms[InstanceIdx] = FTransform::Identity;
		}

		return false;
	}



	return true;
}

bool
FHoudiniInstanceTranslator::GetGenericPropertiesAttributes(
	int32 InGeoNodeId, 
	int32 InPartId, 
	TArray<FHoudiniGenericAttribute>& OutPropertyAttributes)
{
	// List all the generic property detail attributes ...
	int32 FoundCount = FHoudiniEngineUtils::GetGenericAttributeList(
		(HAPI_NodeId)InGeoNodeId, (HAPI_PartId)InPartId, HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, OutPropertyAttributes, HAPI_ATTROWNER_DETAIL);

	// .. then get all the values for the primitive property attributes
	FoundCount += FHoudiniEngineUtils::GetGenericAttributeList(
		(HAPI_NodeId)InGeoNodeId, (HAPI_PartId)InPartId, HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, OutPropertyAttributes, HAPI_ATTROWNER_PRIM, -1);

	// .. then finally, all values for point uproperty attributes
	// TODO: !! get the correct Index here?
	FoundCount += FHoudiniEngineUtils::GetGenericAttributeList(
		(HAPI_NodeId)InGeoNodeId, (HAPI_PartId)InPartId, HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, OutPropertyAttributes, HAPI_ATTROWNER_POINT, -1);

	return FoundCount > 0;
}

bool
FHoudiniInstanceTranslator::GetMaterialOverridesFromAttributes(
	int32 InGeoNodeId,
	int32 InPartId,
	int32 InAttributeIndex,
	EHoudiniInstancerType InInstancerType,
	TArray<FHoudiniMaterialInfo>& OutMaterialAttributes)
{	
	const HAPI_AttributeOwner AttribOwner = InInstancerType == EHoudiniInstancerType::AttributeInstancer ? HAPI_ATTROWNER_POINT : HAPI_ATTROWNER_PRIM;

	// Get the part info
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), InGeoNodeId, InPartId, &PartInfo), false);

	// Get all the part's attribute names
	int32 NumAttribs = PartInfo.attributeCounts[AttribOwner];
	TArray<HAPI_StringHandle> AttribNameHandles;
	AttribNameHandles.SetNum(NumAttribs);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeNames(FHoudiniEngine::Get().GetSession(), InGeoNodeId, InPartId, AttribOwner, AttribNameHandles.GetData(), NumAttribs), false);

	// Extract the attribute names' strings
	TArray<FString> AllAttribNames;
	FHoudiniEngineString::SHArrayToFStringArray(AttribNameHandles, AllAttribNames);

	// Remove all unneeded attributes, only keep valid materials attr
	for (int32 Idx = AllAttribNames.Num() - 1; Idx >= 0; Idx--)
	{
		if (AllAttribNames[Idx].StartsWith(HAPI_UNREAL_ATTRIB_MATERIAL_INSTANCE))
			continue;

		if(AllAttribNames[Idx].StartsWith(HAPI_UNREAL_ATTRIB_MATERIAL))
			continue;

		if (AllAttribNames[Idx].StartsWith(HAPI_UNREAL_ATTRIB_MATERIAL_FALLBACK))
			continue;

		// Not a valid mat, remove from the array
		AllAttribNames.RemoveAt(Idx);
	}

	// Now look for different material attributes in the found attributes
	bool bFoundMaterialAttributes = false;
	// TODO: We need to turn this to an array in order to support a mix of material AND material instances
	//OutMaterialOverrideNeedsCreateInstance = false;

	if (AllAttribNames.IsEmpty())
		return false;

	TArray<FString> MaterialInstanceAttributes;
	TArray<FString> MaterialAttributes;

	// Get material instances overrides attributes
	if (GetMaterialOverridesFromAttributes(InGeoNodeId, InPartId, InAttributeIndex, HAPI_UNREAL_ATTRIB_MATERIAL_INSTANCE, AllAttribNames, MaterialInstanceAttributes))
		bFoundMaterialAttributes = true;

	// Get the "main" material override attributes
	if(GetMaterialOverridesFromAttributes(InGeoNodeId, InPartId, InAttributeIndex, HAPI_UNREAL_ATTRIB_MATERIAL, AllAttribNames, MaterialAttributes))
		bFoundMaterialAttributes = true;

	// If we haven't found anything, try the fallback attribute
	if (!bFoundMaterialAttributes && GetMaterialOverridesFromAttributes(InGeoNodeId, InPartId, InAttributeIndex, HAPI_UNREAL_ATTRIB_MATERIAL_FALLBACK, AllAttribNames, MaterialAttributes))
		bFoundMaterialAttributes = true;

	// We couldnt find any mat attribute? early return
	if (!bFoundMaterialAttributes)
	{
		OutMaterialAttributes.Empty();
		return false;
	}

	// Fetch material instance parameters (detail + AttribOwner) specified via attributes
	TArray<FHoudiniGenericAttribute> AllMatParams;
	FHoudiniMaterialTranslator::GetMaterialParameterAttributes(InGeoNodeId, InPartId, AttribOwner, AllMatParams, InAttributeIndex);

	// Consolidate the final material (or material instance) selection into OutMaterialAttributes
	// Use unreal_material if non-empty. If empty, fallback to unreal_material_instance.
	const int32 MaxNumSlots = FMath::Max(MaterialInstanceAttributes.Num(), MaterialAttributes.Num());
	OutMaterialAttributes.Reset(MaxNumSlots);
	for (int32 MatIdx = 0; MatIdx < MaxNumSlots; ++MatIdx)
	{
		FHoudiniMaterialInfo& MatInfo = OutMaterialAttributes.AddDefaulted_GetRef();
		MatInfo.MaterialIndex = MatIdx;
		// unreal_material takes precedence. If it is missing / empty, check unreal_material_instance
		if (MaterialAttributes.IsValidIndex(MatIdx) && !MaterialAttributes[MatIdx].IsEmpty())
		{
			MatInfo.MaterialObjectPath = MaterialAttributes[MatIdx];
		}
		else if (MaterialInstanceAttributes.IsValidIndex(MatIdx) && !MaterialInstanceAttributes[MatIdx].IsEmpty())
		{
			MatInfo.MaterialObjectPath = MaterialInstanceAttributes[MatIdx];
			MatInfo.bMakeMaterialInstance = true;
			// Get any material parameters for the instance, specified via attributes.
			// We use 0 for the index because we only loaded a specific index's attribute values into AllMatParams for
			// AttribOwner. So the underlying FHoudiniGenericAttribute only contains one entry per attribute.
			FHoudiniMaterialTranslator::GetMaterialParameters(MatInfo, AllMatParams, 0);
		}
	}
	
	return true;
}

bool
FHoudiniInstanceTranslator::GetMaterialOverridesFromAttributes(
	int32 InGeoNodeId, 
	int32 InPartId, 
	int32 InAttributeIndex,
	const FString& InAttributeName,
	const TArray<FString>& InAllAttribNames,
	TArray<FString>& OutMaterialAttributes)
{
	// See if the "materialX" attributes were added as zero-based or not by searching for "material0"
	// If they are not (so the attributes starts at unreal_material1). then we'll need to decrement the idx
	FString MatZero = InAttributeName;
	MatZero += "0";
	bool bZeroBased = false;
	if (InAllAttribNames.Contains(MatZero))
		bZeroBased = true;

	bool bFoundMatAttributes = false;
	int32 PrefixLength = InAttributeName.Len();
	TArray<FString> MatName;
	for (const FString& AttribName : InAllAttribNames)
	{
		if (!AttribName.StartsWith(InAttributeName))
		{
			continue;
		}

		FString Fragment = AttribName.Mid(PrefixLength);

		int32 OverrideIdx = -1;
		if (Fragment == "")
		{
			// The attribute is exactly "unreal_material", use it as the default mat (index 0)
			OverrideIdx = 0;
		}
		else if (!Fragment.IsNumeric())
		{
			continue;
		}
		else
		{
			OverrideIdx = FCString::Atoi(*Fragment);
			if (!bZeroBased)
			{
				OverrideIdx--;
			}
		}

		if (OverrideIdx < 0)
		{
			continue;
		}

		// Increase the size of the array with empty materials
		while (OutMaterialAttributes.Num() <= OverrideIdx)
		{
			OutMaterialAttributes.Add("");
		}

		FHoudiniHapiAccessor Accessor(InGeoNodeId, InPartId, TCHAR_TO_ANSI(*AttribName));
		bool Res = Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, MatName, InAttributeIndex, 1);

		if (!Res)
		{
			HOUDINI_LOG_WARNING(TEXT("[FHoudiniInstanceTranslator::GetMaterialOverridesFromAttributes]: Failed to get material override index %d."), OverrideIdx);
			continue;
		}

		OutMaterialAttributes[OverrideIdx] = MatName[0];
		bFoundMatAttributes = true;
	}

	return bFoundMatAttributes;
}

bool
FHoudiniInstanceTranslator::GetInstancerMaterials(
	const TArray<FHoudiniMaterialInfo>& MaterialAttributes, TArray<UMaterialInterface*>& OutInstancerMaterials)
{
	// Use a map to avoid attempting to load the object for each instance
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*> MaterialMap;

	// Non-instanced materials check material attributes one by one
	const int32 NumSlots = MaterialAttributes.Num();
	OutInstancerMaterials.SetNumZeroed(NumSlots);
	for (int32 MatIdx = 0; MatIdx < NumSlots; ++MatIdx)
	{
		const FHoudiniMaterialInfo& CurrentMatInfo = MaterialAttributes[MatIdx];
		// Only process cases where we are not making material instances
		if (CurrentMatInfo.bMakeMaterialInstance)
			continue;

		const FHoudiniMaterialIdentifier MaterialIdentifier = CurrentMatInfo.MakeIdentifier();
		
		UMaterialInterface* CurrentMaterialInterface = nullptr;
		UMaterialInterface** FoundMaterial = MaterialMap.Find(MaterialIdentifier);
		if (!FoundMaterial)
		{
			// See if we can find a material interface that matches the attribute
			CurrentMaterialInterface = Cast<UMaterialInterface>(
				StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *CurrentMatInfo.MaterialObjectPath, nullptr, LOAD_NoWarn, nullptr));

			// Check validity
			if (!IsValid(CurrentMaterialInterface))
				CurrentMaterialInterface = nullptr;

			// Add what we found to the material map to avoid unnecessary loads
			MaterialMap.Add(MaterialIdentifier, CurrentMaterialInterface);
		}
		else
		{
			// Reuse what we previously found
			CurrentMaterialInterface = *FoundMaterial;
		}
		
		OutInstancerMaterials[MatIdx] = CurrentMaterialInterface;
	}

	return true;
}

bool 
FHoudiniInstanceTranslator::GetInstancerMaterialInstances(
	const TArray<FHoudiniMaterialInfo>& MaterialAttribute,
	const FHoudiniGeoPartObject& InHGPO,
	const FHoudiniPackageParams& InPackageParams,
	TArray<UMaterialInterface*>& OutInstancerMaterials)
{
	TMap<FHoudiniMaterialIdentifier, FHoudiniMaterialInfo> MaterialInstanceOverrides;
	TArray<FHoudiniMaterialIdentifier> MaterialIdentifiers;
	for (const FHoudiniMaterialInfo& MatInfo : MaterialAttribute)
	{
		if (!MatInfo.bMakeMaterialInstance)
		{
			MaterialIdentifiers.Add(FHoudiniMaterialIdentifier());
			continue;
		}
		const FHoudiniMaterialIdentifier MaterialIdentifier = MatInfo.MakeIdentifier();
		MaterialIdentifiers.Add(MaterialIdentifier);
		MaterialInstanceOverrides.Add(MaterialIdentifier, MatInfo);
	}

	// We have no material instances to create
	if (MaterialInstanceOverrides.Num() <= 0)
		return true;
	
	TArray<UPackage*> MaterialAndTexturePackages;
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>> InputAssignmentMaterials;
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>> OutputAssignmentMaterials;
	static constexpr bool bForceRecookAll = false;
	bool bSuccess = false;
	if (FHoudiniMaterialTranslator::CreateMaterialInstances(
			InHGPO,
			InPackageParams,
			MaterialInstanceOverrides,
			MaterialAndTexturePackages,
			InputAssignmentMaterials,
			OutputAssignmentMaterials,
			bForceRecookAll))
	{
		bSuccess = true;
		// Make sure that the OutInstancerMaterials array is the correct size
		OutInstancerMaterials.SetNumZeroed(MaterialAttribute.Num());
		const int32 NumSlots = MaterialIdentifiers.Num();
		for (int32 SlotIdx = 0; SlotIdx < NumSlots; ++SlotIdx)
		{
			const FHoudiniMaterialIdentifier& MaterialIdentifier = MaterialIdentifiers[SlotIdx];
			// skip the invalid ids (non material instance)
			if (!MaterialIdentifier.IsValid())
				continue;
			TObjectPtr<UMaterialInterface>* Material = OutputAssignmentMaterials.Find(MaterialIdentifier);
			if (!Material || !IsValid(*Material))
			{
				OutInstancerMaterials[SlotIdx] = nullptr;
				bSuccess = false;
			}
			else
			{
				OutInstancerMaterials[SlotIdx] = *Material;
			}
		}
	}

	return bSuccess;
}

TArray<UMaterialInterface*>
FHoudiniInstanceTranslator::GetAllInstancerMaterials(
	int32 InAttributeIndex,
	const FHoudiniGeoPartObject& InHGPO, 
	const FHoudiniPackageParams& InPackageParams)
{
	TArray<UMaterialInterface*> InstancerMaterials;

	// Get all the material attributes for that variation
	TArray<FHoudiniMaterialInfo> MaterialAttributes;
	GetMaterialOverridesFromAttributes(InHGPO.GeoId, InHGPO.PartId, InAttributeIndex, InHGPO.InstancerType, MaterialAttributes);

	// Get the materials (for which we don't create material instances)
	// OutInstancerMaterials is grown to the same length as MaterialAttributes (# slots). Sets materials in
	// corresponding slots.
	InstancerMaterials.SetNumZeroed(MaterialAttributes.Num());
	bool bSuccess = GetInstancerMaterials(MaterialAttributes, InstancerMaterials);

	// Get/create the material instances (if any were specified, see FHoudiniMaterialInfo.bMakeMaterialInstace
	// OutInstancerMaterials is grown to the same length as MaterialAttributes (# slots). Sets material instances
	// in corresponding slots.
	bSuccess &= GetInstancerMaterialInstances(MaterialAttributes, InHGPO, InPackageParams, InstancerMaterials);

	return InstancerMaterials;
}

AActor*
FHoudiniInstanceTranslator::SpawnInstanceActor(
	const FTransform& InTransform,
	ULevel* InSpawnLevel,
	UHoudiniInstancedActorComponent* InIAC,
	AActor* InReferenceActor,
	const FName Name)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniInstanceTranslator::SpawnInstanceActor);

	if (!IsValid(InIAC))
		return nullptr;

	UObject* InstancedObject = InIAC->GetInstancedObject();
	if (!IsValid(InstancedObject))
		return nullptr;

	AActor* NewActor = nullptr;

	UWorld* SpawnWorld = InSpawnLevel->GetWorld();
	UClass* InstancedActorClass = InIAC->GetInstancedActorClass();
	if (InstancedActorClass == nullptr || SpawnWorld == nullptr)
	{
#if WITH_EDITOR
		// Try to spawn a new actor for the given transform
		GEditor->ClickLocation = InTransform.GetTranslation();
		GEditor->ClickPlane = FPlane(GEditor->ClickLocation, FVector::UpVector);

		// Using this function lets unreal find the appropriate actor class for us
		// We only use it for the first instanced actors just to get the best actor class for that object
		// Once we have that class - it is much faster (~25x) to just use SpawnActor instead
		TArray<AActor*> NewActors = FLevelEditorViewportClient::TryPlacingActorFromObject(InSpawnLevel, InstancedObject, false, RF_Transactional, nullptr, Name);
		if (NewActors.Num() > 0)
		{
			if (IsValid(NewActors[0]))
			{
				NewActor = NewActors[0];
			}
		}

		// Set the instanced actor class on the IAC so we can reuse it
		if(NewActor)
			InIAC->SetInstancedActorClass(NewActor->GetClass());
#endif
	}
	else
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags = RF_Transactional;
		//SpawnParams.Owner = ComponentOuter;
		SpawnParams.OverrideLevel = InSpawnLevel;
		SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		SpawnParams.Template = nullptr;
		SpawnParams.bNoFail = true;
		// We need to use the previously instantiated actor as template when instantiating a decal material.
		SpawnParams.Template = InReferenceActor;

		NewActor = SpawnWorld->SpawnActor(InstancedActorClass, &InTransform, SpawnParams);
	}

	// Make sure that the actor was spawned in the proper level
	FHoudiniEngineUtils::MoveActorToLevel(NewActor, InSpawnLevel);

	return NewActor;
}


FString
FHoudiniInstanceTranslator::GetInstancerTypeFromComponent(UObject* InObject)
{
	USceneComponent* InComponent = Cast<USceneComponent>(InObject);

	FString InstancerType = TEXT("Instancer");
	if (IsValid(InComponent))
	{
		if (InComponent->IsA<UHoudiniInstancedActorComponent>())
		{
			InstancerType = TEXT("(Actor Instancer)");
		}
		else if (InComponent->IsA<UHierarchicalInstancedStaticMeshComponent>())
		{
			if (InComponent->GetOwner() && InComponent->GetOwner()->IsA<AInstancedFoliageActor>())
				InstancerType = TEXT("(Foliage Instancer)");
			else
				InstancerType = TEXT("(Hierarchical Instancer)");
		}
		else if (InComponent->IsA<UInstancedStaticMeshComponent>())
		{
			InstancerType = TEXT("(Mesh Instancer)");
		}
		else if (InComponent->IsA<UStaticMeshComponent>())
		{
			InstancerType = TEXT("(Static Mesh Component)");
		}
	}

	return InstancerType;
}

bool 
FHoudiniInstanceTranslator::IsHISM(HAPI_NodeId GeoId, HAPI_NodeId PartId, HAPI_AttributeOwner Owner, int Index)
{
	TArray<int32> IntData;

	FHoudiniHapiAccessor Accessor(GeoId, PartId, HAPI_UNREAL_ATTRIB_HIERARCHICAL_INSTANCED_SM);
	Accessor.GetAttributeData(Owner, 1,  IntData, Index, 1);
	if (!IntData.IsEmpty())
		return IntData[0] != 0;

	return false;
}

bool 
FHoudiniInstanceTranslator::IsForceInstancer(HAPI_NodeId GeoId, HAPI_NodeId PartId, HAPI_AttributeOwner Owner, int Index)
{
	TArray<int32> IntData;

	FHoudiniHapiAccessor Accessor(GeoId, PartId, HAPI_UNREAL_ATTRIB_FORCE_INSTANCER);
	Accessor.GetAttributeData(Owner, 1, IntData, Index, 1);
	if (!IntData.IsEmpty())
		return IntData[0] != 0;

	return false;
}

void
FHoudiniInstanceTranslator::GetPerInstanceCustomData(
	int32 InGeoNodeId,
	int32 InPartId,
	FHoudiniInstancerPartData& OutInstancedOutputPartData)
{
	TArray<int32> CustomFloatsArray;
	FHoudiniHapiAccessor Accessor(InGeoNodeId, InPartId, HAPI_UNREAL_ATTRIB_INSTANCE_NUM_CUSTOM_FLOATS);
	Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, CustomFloatsArray);
	if (CustomFloatsArray.IsEmpty())
		return;

	int MaxCustomFloats = 0;
	for( auto & Instancer : OutInstancedOutputPartData.Instancers)
	{
		int NumInstances = Instancer.AttributeIndices.Num();
		Instancer.NumCustomFloats = CustomFloatsArray[Instancer.AttributeIndices[0]];
		Instancer.CustomFloats.SetNum(NumInstances * Instancer.NumCustomFloats);
		MaxCustomFloats = FMath::Max(MaxCustomFloats, Instancer.NumCustomFloats);
	}

	for(int CustomFloatIndex = 0; CustomFloatIndex < MaxCustomFloats; CustomFloatIndex++)
	{
		TArray<float> Values;

		FString CurrentAttr = TEXT(HAPI_UNREAL_ATTRIB_INSTANCE_CUSTOM_DATA_PREFIX) + FString::FromInt(CustomFloatIndex);
		Accessor.Init(InGeoNodeId, InPartId, TCHAR_TO_ANSI(*CurrentAttr));
		Accessor.GetAttributeData(HAPI_ATTROWNER_POINT, Values);
		if (Values.IsEmpty())
		{
			HOUDINI_LOG_ERROR(TEXT("Could found attribute "), *CurrentAttr);
			return;
		}

		for (auto& Instancer : OutInstancedOutputPartData.Instancers)
		{
			if (CustomFloatIndex < Instancer.NumCustomFloats)
			{
				for(int Index = 0; Index < Instancer.AttributeIndices.Num(); Index++)
				{
					int Offset = Index * Instancer.NumCustomFloats + CustomFloatIndex;
					Instancer.CustomFloats[Offset] = Values[Instancer.AttributeIndices[Index]];
				}
			}
		}

	}
}


void
FHoudiniInstanceTranslator::SetPerInstanceCustomData(
	const FHoudiniInstancer& Instancers,
	const FHoudiniInstancerPartData& PartData,
	USceneComponent* InComponentToUpdate)
{
	if (Instancers.NumCustomFloats == 0)
		return;

	UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(InComponentToUpdate);
	if (!IsValid(ISMC))
		return;

	ISMC->NumCustomDataFloats = Instancers.NumCustomFloats;

	ISMC->Modify();

	// Clear out and reinit to 0 the PerInstanceCustomData array
	ISMC->PerInstanceSMCustomData = Instancers.CustomFloats;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
	// TODO:5.4 ?? fix me!!
#else
	// Cant call the edit function above because the function is defined in a different cpp file than the .h it is declared in...
	ISMC->InstanceUpdateCmdBuffer.NumEdits++;
#endif
	
	ISMC->MarkRenderStateDirty();
	
	return;
}

void
FHoudiniInstanceTranslator::SetGenericPropertyAttributes(UObject* Object, const FHoudiniInstancer& InstancerData, const FHoudiniInstancerPartData& InstancerPartData)
{
	if (!Object)
		return;

	// Apply generic attributes if we have any
	if (!InstancerPartData.AllPropertyAttributes.IsEmpty())
	{
		FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(
			Object,
			InstancerPartData.AllPropertyAttributes,
			InstancerData.AttributeIndices[0]);
	}
}

FHoudiniInstancerSettings
FHoudiniInstanceTranslator::GetDefaultInstancerSettings(const FHoudiniGeoPartObject& HGPO, HAPI_AttributeOwner Owner)
{
	FHoudiniInstancerSettings DefaultSettings;
	if (HGPO.PartInfo.InstanceCount)
		DefaultSettings.ComponentRelativeTransform = HGPO.TransformMatrix;

	FHoudiniInstancerSettings Result = GetInstancerSettings(HGPO, HAPI_ATTROWNER_DETAIL, 0, DefaultSettings);
	return Result;
}


FHoudiniInstancerSettings
FHoudiniInstanceTranslator::GetInstancerSettings(
	const FHoudiniGeoPartObject& HGPO, 
	HAPI_AttributeOwner AttributeOwner, 
	int AttrIndex, 
	const FHoudiniInstancerSettings& Defaults)
{
	FHoudiniInstancerSettings Result = Defaults;
	FHoudiniHapiAccessor Accessor;
	FHoudiniEngineIndexedStringMap AttributeValues;

	Accessor.Init(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	Accessor.GetAttributeStrings(AttributeOwner, AttributeValues, AttrIndex, 1);
	if (!AttributeValues.Strings.IsEmpty())
		Result.LevelPath = AttributeValues.Strings[0];

	Accessor.Init(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_BAKE_ACTOR);
	Accessor.GetAttributeStrings(AttributeOwner, AttributeValues, AttrIndex, 1);
	if (!AttributeValues.Strings.IsEmpty())
		Result.BakeActorName = AttributeValues.Strings[0];

	Accessor.Init(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_BAKE_ACTOR_CLASS);
	Accessor.GetAttributeStrings(AttributeOwner, AttributeValues, AttrIndex, 1);
	if (!AttributeValues.Strings.IsEmpty())
		Result.BakeActorClassName = AttributeValues.Strings[0];

	Accessor.Init(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_BAKE_FOLDER);
	Accessor.GetAttributeStrings(AttributeOwner, AttributeValues, AttrIndex, 1);
	if (!AttributeValues.Strings.IsEmpty())
		Result.BakeFolder = AttributeValues.Strings[0];

	Accessor.Init(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_BAKE_OUTLINER_FOLDER);
	Accessor.GetAttributeStrings(AttributeOwner, AttributeValues, AttrIndex, 1);
	if (!AttributeValues.Strings.IsEmpty())
		Result.BakeOutlinerFolder = AttributeValues.Strings[0];

	Accessor.Init(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2);
	Accessor.GetAttributeStrings(AttributeOwner, AttributeValues, AttrIndex, 1);
	if (!AttributeValues.Strings.IsEmpty())
		Result.OutputName = AttributeValues.Strings[0];

	int BoolValue = 0;
	Accessor.Init(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_FOLIAGE_INSTANCER);
	Accessor.GetAttributeData(AttributeOwner, &BoolValue, AttrIndex, 1);
	Result.bIsFoliage = BoolValue != 0;

	BoolValue = 0;
	Accessor.Init(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_HIERARCHICAL_INSTANCED_SM);
	Accessor.GetAttributeData(AttributeOwner, &BoolValue, AttrIndex, 1);
	Result.bForceHISM = BoolValue != 0;

	BoolValue = 0;
	Accessor.Init(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATTRIB_FORCE_INSTANCER);
	Accessor.GetAttributeData(AttributeOwner, &BoolValue, AttrIndex, 1);
	Result.bForceInstancer = BoolValue != 0;

	Accessor.Init(HGPO.GeoId, HGPO.PartId, HAPI_UNREAL_ATRTIB_INSTANCE_ORIGIN);
	HAPI_AttributeInfo AttrInfo;
	if (Accessor.GetInfo(AttrInfo, AttributeOwner) && AttrInfo.tupleSize == 3)
	{
		TArray<float> Center;
		Accessor.GetAttributeData(AttrInfo, Center, AttrIndex, 1);
		FVector Location = FVector(Center[0], Center[2], Center[1]);
		Result.ComponentRelativeTransform.SetIdentity();
		Result.ComponentRelativeTransform.SetLocation(Location * 100.0);
	}

	Result.DataLayers = FHoudiniDataLayerUtils::GetDataLayers(HGPO.GeoId, HGPO.PartId, HAPI_GroupType::HAPI_GROUPTYPE_POINT, AttrIndex);
	if (Result.DataLayers.IsEmpty())
		Result.DataLayers = FHoudiniDataLayerUtils::GetDataLayers(HGPO.GeoId, HGPO.PartId, HAPI_GroupType::HAPI_GROUPTYPE_PRIM, 0);

	Result.HLODLayers = FHoudiniHLODLayerUtils::GetHLODLayers(HGPO.GeoId, HGPO.PartId, HAPI_ATTROWNER_POINT, AttrIndex);
	if (Result.HLODLayers.IsEmpty())
		Result.HLODLayers = FHoudiniHLODLayerUtils::GetHLODLayers(HGPO.GeoId, HGPO.PartId, HAPI_ATTROWNER_PRIM, 0);
	if(Result.HLODLayers.IsEmpty())
		Result.HLODLayers = FHoudiniHLODLayerUtils::GetHLODLayers(HGPO.GeoId, HGPO.PartId, HAPI_ATTROWNER_DETAIL, 0);

	return Result;
}

TArray<FTransform> FHoudiniInstanceTranslator::UnpackTransforms(const FHoudiniInstancer& InstanceData, const FHoudiniInstancerPartData& PartData)
{
	// Use the attribute indices to create an array of transforms for this instancer.
	TArray<FTransform> Results;
	Results.SetNumUninitialized(InstanceData.AttributeIndices.Num());

	for(int Index = 0; Index < InstanceData.AttributeIndices.Num(); Index++)
	{
		Results[Index] = PartData.InstanceTransforms[InstanceData.AttributeIndices[Index]];
	}
	return Results;
}

#undef LOCTEXT_NAMESPACE