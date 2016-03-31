/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
*
* Produced by:
*      Mykola Konyk
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetInstance.h"
#include "HoudiniAssetInstanceVersion.h"
#include "HoudiniAsset.h"
#include "HoudiniEngineString.h"


UHoudiniAssetInstance::UHoudiniAssetInstance(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	HoudiniAsset(nullptr),
	InstantiatedAssetName(TEXT("")),
	AssetId(-1),
	AssetCookCount(0),
	bIsAssetBeingAsyncInstantiatedOrCooked(false),
	Transform(FTransform::Identity),
	HoudiniAssetInstanceFlagsPacked(0u),
	HoudiniAssetInstanceVersion(VER_HOUDINI_ENGINE_ASSETINSTANCE_BASE)
{

}


UHoudiniAssetInstance::~UHoudiniAssetInstance()
{

}


UHoudiniAssetInstance*
UHoudiniAssetInstance::CreateAssetInstance(UObject* Outer, UHoudiniAsset* HoudiniAsset)
{
	if(!HoudiniAsset)
	{
		return nullptr;
	}

	UHoudiniAssetInstance* HoudiniAssetInstance = NewObject<UHoudiniAssetInstance>(Outer,
		UHoudiniAssetInstance::StaticClass(), NAME_None, RF_Public | RF_Transactional);

	HoudiniAssetInstance->HoudiniAsset = HoudiniAsset;
	return HoudiniAssetInstance;
}


UHoudiniAsset*
UHoudiniAssetInstance::GetHoudiniAsset() const
{
	return HoudiniAsset;
}


bool
UHoudiniAssetInstance::IsValidAssetInstance() const
{
	return FHoudiniEngineUtils::IsValidAssetId(AssetId);
}


int32
UHoudiniAssetInstance::GetAssetCookCount() const
{
	return AssetCookCount;
}


const FTransform&
UHoudiniAssetInstance::GetAssetTransform() const
{
	return Transform;
}


void
UHoudiniAssetInstance::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetInstance* HoudiniAssetInstance = Cast<UHoudiniAssetInstance>(InThis);
	if(HoudiniAssetInstance && !HoudiniAssetInstance->IsPendingKill())
	{
		if(HoudiniAssetInstance->HoudiniAsset)
		{
			Collector.AddReferencedObject(HoudiniAssetInstance->HoudiniAsset, InThis);
		}
	}

	Super::AddReferencedObjects(InThis, Collector);
}


void
UHoudiniAssetInstance::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	HoudiniAssetInstanceVersion = VER_HOUDINI_ENGINE_ASSETINSTANCE_AUTOMATIC_VERSION;
	Ar << HoudiniAssetInstanceVersion;

	Ar << HoudiniAssetInstanceFlagsPacked;

	Ar << HoudiniAsset;
	Ar << DefaultPresetBuffer;
	Ar << Transform;

	HAPI_AssetId AssetIdTemp = AssetId;
	Ar << AssetIdTemp;

	if(Ar.IsLoading())
	{
		AssetId = AssetIdTemp;
	}
}


void
UHoudiniAssetInstance::FinishDestroy()
{
	Super::FinishDestroy();
	DeleteAsset();
}


bool
UHoudiniAssetInstance::InstantiateAsset(bool* bInstantiatedWithErrors)
{
	return InstantiateAsset(FHoudiniEngineString(), bInstantiatedWithErrors);
}


bool
UHoudiniAssetInstance::InstantiateAsset(const FHoudiniEngineString& AssetNameToInstantiate,
	bool* bInstantiatedWithErrors)
{
	HOUDINI_LOG_MESSAGE(TEXT("HAPI Synchronous Instantiation Started. HoudiniAsset = 0x%x, "), HoudiniAsset);

	if(bInstantiatedWithErrors)
	{
		*bInstantiatedWithErrors = false;
	}

	if(!HoudiniAsset)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error instantiating asset, asset is null!"));
		return false;
	}

	if(!FHoudiniEngineUtils::IsInitialized())
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error instantiating asset, HAPI is not initialized!"));
		return false;
	}

	if(IsValidAssetInstance())
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error instantiating asset, asset is already instantiated!"));
		return false;
	}

	FHoudiniEngineString AssetName(AssetNameToInstantiate);

	HAPI_AssetLibraryId AssetLibraryId = -1;
	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	if(!AssetName.HasValidId())
	{
		// No asset was specified, retrieve assets.

		TArray<HAPI_StringHandle> AssetNames;
		if(!FHoudiniEngineUtils::GetAssetNames(HoudiniAsset, AssetLibraryId, AssetNames))
		{
			HOUDINI_LOG_MESSAGE(TEXT("Error instantiating the asset, error retrieving asset names from HDA."));
			return false;
		}

		if(!AssetNames.Num())
		{
			HOUDINI_LOG_MESSAGE(TEXT("Error instantiating the asset, HDA does not contain assets."));
			return false;
		}

		AssetName = FHoudiniEngineString(AssetNames[0]);
		if(!AssetName.HasValidId())
		{
			HOUDINI_LOG_MESSAGE(TEXT("Error instantiating the asset, HDA specifies invalid asset."));
			return false;
		}
	}

	std::string AssetNameString;
	if(!AssetName.ToStdString(AssetNameString))
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error instantiating the asset, error translating the asset name."));
		return false;
	}

	FString AssetNameUnreal = ANSI_TO_TCHAR(AssetNameString.c_str());
	double TimingStart = FPlatformTime::Seconds();

	// We instantiate without cooking.
	HAPI_AssetId AssetIdNew = -1;
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::InstantiateAsset(FHoudiniEngine::Get().GetSession(), &AssetNameString[0],
		false, &AssetIdNew))
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error instantiating the asset, %s failed InstantiateAsset API call."),
			*AssetNameUnreal);
		return false;
	}

	AssetId = AssetIdNew;
	AssetCookCount = 0;
	bool bResultSuccess = false;

	while(true)
	{
		int Status = HAPI_STATE_STARTING_COOK;
		if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetStatus(FHoudiniEngine::Get().GetSession(), HAPI_STATUS_COOK_STATE,
			&Status))
		{
			HOUDINI_LOG_MESSAGE(TEXT("Error instantiating the asset, %s failed GetStatus API call."),
				*AssetNameUnreal);
			return false;
		}

		if(HAPI_STATE_READY == Status)
		{
			HOUDINI_LOG_MESSAGE(TEXT("Instantiated asset %s successfully."), *AssetNameUnreal);
			bResultSuccess = true;
			break;
		}
		else if(HAPI_STATE_READY_WITH_COOK_ERRORS == Status)
		{
			if(bInstantiatedWithErrors)
			{
				*bInstantiatedWithErrors = true;
			}

			HOUDINI_LOG_MESSAGE(TEXT("Instantiated asset %s with errors."), *AssetNameUnreal);
			bResultSuccess = true;
			break;
		}
		else if(HAPI_STATE_READY_WITH_FATAL_ERRORS == Status)
		{
			if(bInstantiatedWithErrors)
			{
				*bInstantiatedWithErrors = true;
			}

			HOUDINI_LOG_MESSAGE(TEXT("Failed to instantiate the asset %s ."), *AssetNameUnreal);
			bResultSuccess = false;
			break;
		}

		FPlatformProcess::Sleep(0.0f);
	}

	if(bResultSuccess)
	{
		double AssetOperationTiming = FPlatformTime::Seconds() - TimingStart;
		HOUDINI_LOG_MESSAGE(TEXT("Instantiation of asset %s took %f seconds."), *AssetNameUnreal,
			AssetOperationTiming);

		InstantiatedAssetName = AssetNameUnreal;

		PostInstantiateAsset();
	}

	return bResultSuccess;
}


bool
UHoudiniAssetInstance::CookAsset(bool* bCookedWithErrors)
{
	HOUDINI_LOG_MESSAGE(TEXT("HAPI Synchronous Cooking of %s Started. HoudiniAsset = 0x%x, "),
		*InstantiatedAssetName, HoudiniAsset);

	if(bCookedWithErrors)
	{
		*bCookedWithErrors = false;
	}

	if(!HoudiniAsset)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error cooking the %s asset, asset is null!"),
			*InstantiatedAssetName);
		return false;
	}

	if(!FHoudiniEngineUtils::IsInitialized())
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error cooking the %s asset, HAPI is not initialized!"),
			*InstantiatedAssetName);
		return false;
	}

	if(!IsValidAssetInstance())
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error cooking the %s asset, asset has not been instantiated!"),
			*InstantiatedAssetName);
		return false;
	}

	double TimingStart = FPlatformTime::Seconds();

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::CookAsset(FHoudiniEngine::Get().GetSession(), AssetId, nullptr))
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error cooking the %s asset, failed CookAsset API call."),
			*InstantiatedAssetName);
		return false;
	}

	bool bResultSuccess = false;

	while(true)
	{
		int32 Status = HAPI_STATE_STARTING_COOK;
		if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetStatus(FHoudiniEngine::Get().GetSession(), HAPI_STATUS_COOK_STATE,
			&Status))
		{
			HOUDINI_LOG_MESSAGE(TEXT("Error cooking the %s asset, failed GetStatus API call."),
				*InstantiatedAssetName);
			return false;
		}

		if(HAPI_STATE_READY == Status)
		{
			HOUDINI_LOG_MESSAGE(TEXT("Cooked asset %s successfully."), *InstantiatedAssetName);
			bResultSuccess = true;
			break;
		}
		else if(HAPI_STATE_READY_WITH_COOK_ERRORS == Status)
		{
			HOUDINI_LOG_MESSAGE(TEXT("Cooked asset %s with errors."), *InstantiatedAssetName);

			if(bCookedWithErrors)
			{
				*bCookedWithErrors = true;
			}

			bResultSuccess = true;
			break;
		}
		else if(HAPI_STATE_READY_WITH_FATAL_ERRORS == Status)
		{
			HOUDINI_LOG_MESSAGE(TEXT("Failed to cook the asset %s ."), *InstantiatedAssetName);

			if(bCookedWithErrors)
			{
				*bCookedWithErrors = true;
			}

			bResultSuccess = false;
			break;
		}

		FPlatformProcess::Sleep(0.0f);
	}

	AssetCookCount++;

	if(bResultSuccess)
	{
		double AssetOperationTiming = FPlatformTime::Seconds() - TimingStart;
		HOUDINI_LOG_MESSAGE(TEXT("Cooking of asset %s took %f seconds."), *InstantiatedAssetName,
			AssetOperationTiming);

		PostCookAsset();
	}

	return bResultSuccess;
}


bool
UHoudiniAssetInstance::DeleteAsset()
{
	HOUDINI_LOG_MESSAGE(TEXT("HAPI Synchronous Deletion of %s Started. HoudiniAsset = 0x%x, "),
		*InstantiatedAssetName, HoudiniAsset);

	if(FHoudiniEngineUtils::IsInitialized() && IsValidAssetInstance())
	{
		FHoudiniEngineUtils::DestroyHoudiniAsset(AssetId);
	}

	AssetId = -1;
	return true;
}


bool
UHoudiniAssetInstance::InstantiateAssetAsync()
{
	check(0);

	FPlatformAtomics::InterlockedExchange(&bIsAssetBeingAsyncInstantiatedOrCooked, 1);

	return false;
}


bool
UHoudiniAssetInstance::CookAssetAsync()
{
	check(0);

	FPlatformAtomics::InterlockedExchange(&bIsAssetBeingAsyncInstantiatedOrCooked, 1);

	return false;
}


bool
UHoudiniAssetInstance::DeleteAssetAsync()
{
	check(0);
	return false;
}


bool
UHoudiniAssetInstance::IsAssetBeingAsyncInstantiatedOrCooked() const
{
	return 1 == bIsAssetBeingAsyncInstantiatedOrCooked;
}


bool
UHoudiniAssetInstance::HasAssetFinishedAsyncInstantiation(bool* bInstantiatedWithErrors) const
{
	check(0);
	return false;
}


bool
UHoudiniAssetInstance::HasAssetFinishedAsyncCooking(bool* bCookedWithErrors) const
{
	check(0);
	return false;
}


bool
UHoudiniAssetInstance::GetGeoPartObjects(TArray<FHoudiniGeoPartObject>& InGeoPartObjects) const
{
	InGeoPartObjects.Empty();

	if(!IsValidAssetInstance())
	{
		return false;
	}

	TArray<HAPI_ObjectInfo> ObjectInfos;
	if(!HapiGetObjectInfos(ObjectInfos))
	{
		return false;
	}

	TArray<FTransform> ObjectTransforms;
	if(!HapiGetObjectTransforms(ObjectTransforms))
	{
		return false;
	}

	check(ObjectInfos.Num() == ObjectTransforms.Num());

	if(!ObjectInfos.Num())
	{
		return true;
	}

	for(int32 ObjectIdx = 0, ObjectNum = ObjectInfos.Num(); ObjectIdx < ObjectNum; ++ObjectIdx)
	{
		const HAPI_ObjectInfo& ObjectInfo = ObjectInfos[ObjectIdx];
		const FTransform& ObjectTransform = ObjectTransforms[ObjectIdx];

		FString ObjectName = TEXT("");
		FHoudiniEngineString HoudiniEngineStringObjectName(ObjectInfo.nameSH);
		HoudiniEngineStringObjectName.ToFString(ObjectName);

		for(int32 GeoIdx = 0; GeoIdx < ObjectInfo.geoCount; ++GeoIdx)
		{
			HAPI_GeoInfo GeoInfo;
			if(!HapiGetGeoInfo(ObjectInfo.id, GeoIdx, GeoInfo))
			{
				continue;
			}

			if(!GeoInfo.isDisplayGeo)
			{
				continue;
			}

			for(int32 PartIdx = 0; PartIdx < GeoInfo.partCount; ++PartIdx)
			{
				HAPI_PartInfo PartInfo;
				if(!HapiGetPartInfo(ObjectInfo.id, GeoInfo.id, PartIdx, PartInfo))
				{
					continue;
				}

				FString PartName = TEXT("");
				FHoudiniEngineString HoudiniEngineStringPartName(PartInfo.nameSH);
				HoudiniEngineStringPartName.ToFString(PartName);

				InGeoPartObjects.Add(FHoudiniGeoPartObject(ObjectTransform, ObjectName, PartName, AssetId,
					ObjectInfo.id, GeoInfo.id, PartInfo.id));
			}
		}
	}

	return true;
}


bool
UHoudiniAssetInstance::GetParameterObjects(TMap<FString, FHoudiniParameterObject>& InParameterObjects) const
{
	TArray<HAPI_ParmInfo> ParmInfos;
	if(!HapiGetParmInfos(ParmInfos))
	{
		return false;
	}

	HAPI_NodeId NodeId = HapiGetNodeId();
	if(NodeId < 0)
	{
		return false;
	}

	for(int32 ParmIdx = 0, ParmNum = ParmInfos.Num(); ParmIdx < ParmNum; ++ParmIdx)
	{
		FString ParameterName = TEXT("");
		FHoudiniParameterObject HoudiniParameterObject(NodeId, ParmInfos[ParmIdx]);
		if(HoudiniParameterObject.HapiGetName(ParameterName))
		{
			InParameterObjects.Add(ParameterName, HoudiniParameterObject);
		}
	}

	return true;
}


bool
UHoudiniAssetInstance::GetInputObjects(TArray<FHoudiniInputObject>& InInputObjects) const
{
	InInputObjects.Empty();

	HAPI_AssetInfo AssetInfo;
	if(!HapiGetAssetInfo(AssetInfo))
	{
		return false;
	}

	if(AssetInfo.geoInputCount > 0)
	{
		InInputObjects.SetNumUninitialized(AssetInfo.geoInputCount);
		for(int32 InputIdx = 0; InputIdx < AssetInfo.geoInputCount; ++InputIdx)
		{
			InInputObjects[InputIdx] = FHoudiniInputObject(InputIdx);
		}
	}

	return true;

}


HAPI_NodeId
UHoudiniAssetInstance::HapiGetNodeId() const
{
	HAPI_AssetInfo AssetInfo;

	if(!HapiGetAssetInfo(AssetInfo))
	{
		return -1;
	}

	return AssetInfo.nodeId;
}


bool
UHoudiniAssetInstance::HapiGetNodeInfo(HAPI_NodeInfo& NodeInfo) const
{
	FMemory::Memset<HAPI_NodeInfo>(NodeInfo, 0);

	HAPI_NodeId NodeId = HapiGetNodeId();
	if(NodeId < 0)
	{
		return false;
	}

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo))
	{
		return false;
	}

	return true;
}


bool
UHoudiniAssetInstance::HapiGetAssetInfo(HAPI_AssetInfo& AssetInfo) const
{
	FMemory::Memset<HAPI_AssetInfo>(AssetInfo, 0);

	if(!IsValidAssetInstance())
	{
		return false;
	}

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo))
	{
		return false;
	}

	return true;
}


bool
UHoudiniAssetInstance::HapiGetObjectInfos(TArray<HAPI_ObjectInfo>& ObjectInfos) const
{
	ObjectInfos.Empty();

	if(!IsValidAssetInstance())
	{
		return false;
	}

	HAPI_AssetInfo AssetInfo;
	if(!HapiGetAssetInfo(AssetInfo))
	{
		return false;
	}

	if(AssetInfo.objectCount > 0)
	{
		ObjectInfos.SetNumUninitialized(AssetInfo.objectCount);
		if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetObjects(FHoudiniEngine::Get().GetSession(), AssetId, &ObjectInfos[0],
			0, AssetInfo.objectCount))
		{
			return false;
		}
	}

	return true;
}


bool
UHoudiniAssetInstance::HapiGetObjectTransforms(TArray<FTransform>& ObjectTransforms) const
{
	ObjectTransforms.Empty();

	if(!IsValidAssetInstance())
	{
		return false;
	}

	HAPI_AssetInfo AssetInfo;
	if(!HapiGetAssetInfo(AssetInfo))
	{
		return false;
	}

	if(AssetInfo.objectCount > 0)
	{
		TArray<HAPI_Transform> HapiObjectTransforms;
		HapiObjectTransforms.SetNumUninitialized(AssetInfo.objectCount);
		ObjectTransforms.SetNumUninitialized(AssetInfo.objectCount);

		if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetObjectTransforms(FHoudiniEngine::Get().GetSession(), AssetId,
			HAPI_SRT, &HapiObjectTransforms[0], 0, AssetInfo.objectCount))
		{
			return false;
		}

		for(int32 Idx = 0; Idx < AssetInfo.objectCount; ++Idx)
		{
			FHoudiniEngineUtils::TranslateHapiTransform(HapiObjectTransforms[Idx], ObjectTransforms[Idx]);
		}
	}

	return true;
}


bool
UHoudiniAssetInstance::HapiGetAssetTransform(FTransform& InTransform) const
{
	InTransform.SetIdentity();

	if(!IsValidAssetInstance())
	{
		return false;
	}

	HAPI_TransformEuler AssetEulerTransform;
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetAssetTransform(FHoudiniEngine::Get().GetSession(), AssetId,
		HAPI_SRT, HAPI_XYZ, &AssetEulerTransform))
	{
		return false;
	}

	// Convert HAPI Euler transform to Unreal one.
	FHoudiniEngineUtils::TranslateHapiTransform(AssetEulerTransform, InTransform);
	return true;
}


bool
UHoudiniAssetInstance::HapiGetGeoInfo(HAPI_ObjectId ObjectId, int32 GeoIdx, HAPI_GeoInfo& GeoInfo) const
{
	FMemory::Memset<HAPI_GeoInfo>(GeoInfo, 0);

	if(!IsValidAssetInstance())
	{
		return false;
	}

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetGeoInfo(FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoIdx,
		&GeoInfo))
	{
		return false;
	}

	return true;
}


bool
UHoudiniAssetInstance::HapiGetPartInfo(HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, int32 PartIdx,
	HAPI_PartInfo& PartInfo) const
{
	FMemory::Memset<HAPI_PartInfo>(PartInfo, 0);

	if(!IsValidAssetInstance())
	{
		return false;
	}

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId,
		PartIdx, &PartInfo))
	{
		return false;
	}

	return true;
}


bool
UHoudiniAssetInstance::HapiGetParmInfos(TArray<HAPI_ParmInfo>& ParmInfos) const
{
	ParmInfos.Empty();

	if(!IsValidAssetInstance())
	{
		return false;
	}

	HAPI_NodeInfo NodeInfo;
	if(!HapiGetNodeInfo(NodeInfo))
	{
		return false;
	}

	if(NodeInfo.parmCount > 0)
	{
		if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParameters(FHoudiniEngine::Get().GetSession(), NodeInfo.id,
			&ParmInfos[0], 0, NodeInfo.parmCount))
		{
			return false;
		}
	}

	return true;
}


bool
UHoudiniAssetInstance::HapiGetAssetPreset(TArray<char>& PresetBuffer) const
{
	PresetBuffer.Empty();

	HAPI_NodeId NodeId = HapiGetNodeId();
	if(NodeId < 0)
	{
		return false;
	}

	int32 BufferLength = 0;
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetPresetBufLength(FHoudiniEngine::Get().GetSession(), NodeId,
		HAPI_PRESETTYPE_BINARY, nullptr, &BufferLength))
	{
		return false;
	}

	PresetBuffer.SetNumZeroed(BufferLength);
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetPreset(FHoudiniEngine::Get().GetSession(), NodeId, &PresetBuffer[0],
		BufferLength))
	{
		PresetBuffer.Empty();
		return false;
	}

	return true;
}


bool
UHoudiniAssetInstance::HapiSetAssetPreset(const TArray<char>& PresetBuffer) const
{
	if(!PresetBuffer.Num())
	{
		return false;
	}

	HAPI_NodeId NodeId = HapiGetNodeId();
	if(NodeId < 0)
	{
		return false;
	}

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::SetPreset(FHoudiniEngine::Get().GetSession(), NodeId,
		HAPI_PRESETTYPE_BINARY, nullptr, &PresetBuffer[0], PresetBuffer.Num()))
	{
		return false;
	}

	return true;
}


bool
UHoudiniAssetInstance::HapiSetDefaultPreset() const
{
	return HapiSetAssetPreset(DefaultPresetBuffer);
}


bool
UHoudiniAssetInstance::PostInstantiateAsset()
{
	HapiGetAssetPreset(DefaultPresetBuffer);

	HapiGetAssetTransform(Transform);
	GetParameterObjects(ParameterObjects);
	GetInputObjects(InputObjects);

	return true;
}


bool
UHoudiniAssetInstance::PostCookAsset()
{
	HapiGetAssetTransform(Transform);
	GetParameterObjects(ParameterObjects);
	GetGeoPartObjects(GeoPartObjects);
	GetInputObjects(InputObjects);

	return true;
}
