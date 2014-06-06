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

class UHoudiniAssetInstance;

class IHoudiniTaskCookAssetInstanceCallback
{
public:

	/** Called by FHoudiniTaskCookAsset to notify manager about error during asset instance cooking. **/
	virtual void NotifyAssetInstanceCookingFailed(UHoudiniAssetInstance* HoudiniAssetInstance, HAPI_Result Result) = 0;

	/** Called by FHoudiniTaskCookAsset to notify manager that asset cooking has been completed. **/
	virtual void NotifyAssetInstanceCookingFinished(UHoudiniAssetInstance* HoudiniAssetInstance, HAPI_AssetId AssetId, const std::string& AssetInternalName) = 0;
};
