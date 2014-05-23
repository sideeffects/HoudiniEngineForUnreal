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

#include "HoudiniEnginePrivatePCH.h"


FHoudiniTaskCookAsset::FHoudiniTaskCookAsset(UHoudiniAssetManager* InHoudiniAssetManager, UHoudiniAsset* InHoudiniAsset) :
	HoudiniAssetManager(InHoudiniAssetManager),
	HoudiniAsset(InHoudiniAsset)
{

}


FHoudiniTaskCookAsset::~FHoudiniTaskCookAsset()
{

}


uint32 
FHoudiniTaskCookAsset::RunErrorCleanUp(HAPI_Result Result)
{
	// Notify manager that a certain error occurred during cooking.
	HoudiniAssetManager->NotifyAssetCookingFailed(HoudiniAsset, Result);
	return 0;
}


uint32 
FHoudiniTaskCookAsset::Run()
{
	HOUDINI_LOG_MESSAGE(TEXT("HoudiniTaskCookAsset Asynchronous Cooking Started."));

	if(!FHoudiniEngineUtils::IsInitialized())
	{
		HOUDINI_LOG_ERROR(TEXT("HoudiniTaskCookAsset failed: %s"), *FHoudiniEngineUtils::GetErrorDescription(HAPI_RESULT_NOT_INITIALIZED));
		return RunErrorCleanUp(HAPI_RESULT_NOT_INITIALIZED);
	}

	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	// Load asset library from given buffer.
	HAPI_AssetLibraryId AssetLibraryId = 0;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_LoadAssetLibraryFromMemory(reinterpret_cast<const char*>(HoudiniAsset->GetAssetBytes()), HoudiniAsset->GetAssetBytesCount(), &AssetLibraryId), RunErrorCleanUp(Result));

	// Retrieve number of assets contained in this library.
	int32 AssetCount = 0;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAvailableAssetCount(AssetLibraryId, &AssetCount), RunErrorCleanUp(Result));

	// Retrieve available assets. 
	std::vector<int> AssetNames(AssetCount, 0);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAvailableAssets(AssetLibraryId, &AssetNames[0], AssetCount), RunErrorCleanUp(Result));

	// If we have assets, instantiate first one.
	std::string AssetName;
	if(AssetCount && FHoudiniEngineUtils::GetAssetName(AssetNames[0], AssetName))
	{
		HAPI_AssetId AssetId = -1;
		bool CookOnLoad = true;
		HOUDINI_CHECK_ERROR_RETURN(HAPI_InstantiateAsset(&AssetName[0], CookOnLoad, &AssetId), RunErrorCleanUp(Result));

		// We need to spin until cooking is finished.
		while(true)
		{
			int Status = HAPI_STATE_STARTING_COOK;
			HOUDINI_CHECK_ERROR(HAPI_GetStatus(HAPI_STATUS_STATE, &Status));

			if(HAPI_STATE_READY == Status)
			{
				// Cooking has been successful.
				HoudiniAssetManager->NotifyAssetCookingFinished(HoudiniAsset, AssetId, AssetName);
				break;
			}
			else if(HAPI_STATE_READY_WITH_ERRORS == Status)
			{
				// There was an error while cooking.
				HOUDINI_LOG_ERROR(TEXT("HoudiniTaskCookAsset failed: Asset cooked with errors."));
				HoudiniAssetManager->NotifyAssetCookingFailed(HoudiniAsset, HAPI_RESULT_FAILURE);
				break;
			}

			// We want to yield for a bit.
			FPlatformProcess::Sleep(0.01f);
		}		
	}

	return 0;
}
