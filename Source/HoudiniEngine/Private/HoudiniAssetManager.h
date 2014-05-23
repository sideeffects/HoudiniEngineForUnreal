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

#pragma once
#include "HoudiniAssetManager.generated.h"

class UHoudiniAsset;

UCLASS()
class HOUDINIENGINE_API UHoudiniAssetManager : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Called externally to notify manager about asset creation. **/
	void NotifyAssetCreated(UHoudiniAsset* HoudiniAsset);

	/** Called externally to notify manager about asset destruction. **/
	void NotifyAssetDestroyed(UHoudiniAsset* HoudiniAsset);

	/** Called to schedule an asynchronous cooking on a given asset. **/
	void ScheduleAsynchronousCooking(UHoudiniAsset* HoudiniAsset);

public: /** FHoudiniTaskCookAsset callbacks. **/

	/** Called by FHoudiniTaskCookAsset to notify manager about error during asset cooking. **/
	void NotifyAssetCookingFailed(UHoudiniAsset* HoudiniAsset, HAPI_Result Result);

	/** Called by FHoudiniTaskCookAsset to notify manager that asset cooking has been completed. **/
	void NotifyAssetCookingFinished(UHoudiniAsset* HoudiniAsset, HAPI_AssetId AssetId, const std::string& AssetInternalName);

public:

	/** Array containing all managed assets. **/
	TArray<UHoudiniAsset*> HoudiniAssets;
};
