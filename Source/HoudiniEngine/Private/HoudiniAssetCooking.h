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
#include "HAPI.h"


class FHoudiniAssetCooking : public FRunnable
{
	friend class UHoudiniAssetComponent;

public:

	FHoudiniAssetCooking(UHoudiniAssetComponent* InHoudiniAssetComponent, const UHoudiniAsset* InHoudiniAsset);
	virtual ~FHoudiniAssetCooking();

public: /** FRunnable methods. **/
	
	virtual uint32 Run() OVERRIDE;

protected:

	/** Remove self from the registry, if present. **/
	void RemoveSelfFromRegistryCompleted(HAPI_AssetId AssetId, const char* AssetName);
	void RemoveSelfFromRegistryFailed();

protected:

	/** Map holding all components that have requested cooking. **/
	static TMap<FHoudiniAssetCooking*, UHoudiniAssetComponent*> AssetCookingComponentRegistry;

	/** Synchronization primitive used to control access to the map. **/
	static FCriticalSection CriticalSection;
	
protected:

	/** Houdini asset component which requested this cook. **/
	UHoudiniAssetComponent* HoudiniAssetComponent;

	/** Houdini asset we are cooking from. **/
	const UHoudiniAsset* HoudiniAsset;
};
