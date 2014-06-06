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

class IHoudiniTaskCookAssetInstanceCallback;

class FHoudiniTaskCookAssetInstance : public FRunnable
{
public:

	FHoudiniTaskCookAssetInstance(IHoudiniTaskCookAssetInstanceCallback* InHoudiniTaskCookAssetInstanceCallback, UHoudiniAssetInstance* InHoudiniAssetInstance);
	virtual ~FHoudiniTaskCookAssetInstance();

public: /** FRunnable methods. **/
	
	virtual uint32 Run() OVERRIDE;

private:

	/** Helper function used to clean up while running in case of error. **/
	uint32 RunErrorCleanUp(HAPI_Result Result);
		
protected:

	/** Callback for completion / status report. **/
	IHoudiniTaskCookAssetInstanceCallback* HoudiniTaskCookAssetInstanceCallback;

	/** Houdini asset instance we are cooking. **/
	UHoudiniAssetInstance* HoudiniAssetInstance;
};
