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

class IHoudiniTaskCookAssetCallback
{
public:

	/** Called to notify receiver about error during asset instance cooking. **/
	virtual void NotifyAssetCookingFailed(UHoudiniAssetInstance* HoudiniAssetInstance, HAPI_Result Result) = 0;

	/** Called to notify receiver that asset cooking has been completed. **/
	virtual void NotifyAssetCookingFinished(UHoudiniAssetInstance* HoudiniAssetInstance, HAPI_AssetId AssetId) = 0;
};
