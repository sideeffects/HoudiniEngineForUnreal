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
	bIsBeingAsyncInstantiatedOrCooked(false),
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
UHoudiniAssetInstance::HasValidAssetId() const
{
	return FHoudiniEngineUtils::IsValidAssetId(AssetId);
}


int32
UHoudiniAssetInstance::GetAssetCookCount() const
{
	return AssetCookCount;
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
}


bool
UHoudiniAssetInstance::Instantiate(bool* bInstantiatedWithErrors)
{
	return Instantiate(FHoudiniEngineString(), bInstantiatedWithErrors);
}


bool
UHoudiniAssetInstance::Instantiate(const FHoudiniEngineString& AssetNameToInstantiate,
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
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::InstantiateAsset(FHoudiniEngine::Get().GetSession(), &AssetNameString[0],
		false, &AssetId))
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error instantiating the asset, %s failed InstantiateAsset API call."), *AssetNameUnreal);
		return false;
	}

	bool bResultSuccess = false;

	while(true)
	{
		int Status = HAPI_STATE_STARTING_COOK;
		if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetStatus(FHoudiniEngine::Get().GetSession(), HAPI_STATUS_COOK_STATE,
			&Status))
		{
			HOUDINI_LOG_MESSAGE(TEXT("Error instantiating the asset, %s failed GetStatus API call."), *AssetNameUnreal);
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
	}

	return bResultSuccess;
}


bool
UHoudiniAssetInstance::Cook(bool* bCookedWithErrors)
{
	HOUDINI_LOG_MESSAGE(TEXT("HAPI Synchronous Cooking of %s Started. HoudiniAsset = 0x%x, "), *InstantiatedAssetName,
		HoudiniAsset);

	if(bCookedWithErrors)
	{
		*bCookedWithErrors = false;
	}

	if(!HoudiniAsset)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error cooking the %s asset, asset is null!"), *InstantiatedAssetName);
		return false;
	}

	if(!FHoudiniEngineUtils::IsInitialized())
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error cooking the %s asset, HAPI is not initialized!"), *InstantiatedAssetName);
		return false;
	}

	if(!HasValidAssetId())
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error cooking the %s asset, asset has not been instantiated!"), *InstantiatedAssetName);
		return false;
	}

	double TimingStart = FPlatformTime::Seconds();

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::CookAsset(FHoudiniEngine::Get().GetSession(), AssetId, nullptr))
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error cooking the %s asset, failed CookAsset API call."), *InstantiatedAssetName);
		return false;
	}

	bool bResultSuccess = false;

	while(true)
	{
		int32 Status = HAPI_STATE_STARTING_COOK;
		if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetStatus(FHoudiniEngine::Get().GetSession(), HAPI_STATUS_COOK_STATE,
			&Status))
		{
			HOUDINI_LOG_MESSAGE(TEXT("Error cooking the %s asset, failed GetStatus API call."), *InstantiatedAssetName);
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

	if(bResultSuccess)
	{
		double AssetOperationTiming = FPlatformTime::Seconds() - TimingStart;
		HOUDINI_LOG_MESSAGE(TEXT("Cooking of asset %s took %f seconds."), *InstantiatedAssetName,
			AssetOperationTiming);
	}

	return bResultSuccess;
}


bool
UHoudiniAssetInstance::InstantiateAsync()
{
	check(0);

	FPlatformAtomics::InterlockedExchange(&bIsBeingAsyncInstantiatedOrCooked, 1);

	return false;
}


bool
UHoudiniAssetInstance::CookAsync()
{
	check(0);

	FPlatformAtomics::InterlockedExchange(&bIsBeingAsyncInstantiatedOrCooked, 1);

	return false;
}


int32
UHoudiniAssetInstance::IsBeingAsyncInstantiatedOrCooked() const
{
	return bIsBeingAsyncInstantiatedOrCooked;
}


bool
UHoudiniAssetInstance::IsFinishedAsyncInstantiation(bool* bInstantiatedWithErrors) const
{
	check(0);
	return false;
}


bool
UHoudiniAssetInstance::IsFinishedAsyncCooking(bool* bCookedWithErrors) const
{
	check(0);
	return false;
}


bool
UHoudiniAssetInstance::GetGeoPartObjects(TArray<FHoudiniGeoPartObject>& GeoPartObjects) const
{
	check(0);
	return false;
}
