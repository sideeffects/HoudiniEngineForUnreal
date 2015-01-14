/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Brokers define connection between assets (for example in content
 * browser) and actors.
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
#include "HoudiniRuntimeSettings.generated.h"


UCLASS(config = Engine, defaultconfig)
class HOUDINIENGINE_API UHoudiniRuntimeSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniRuntimeSettings();

public:

	// Enables cooking of assets on parameter or input change.
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Cooking)
	bool bEnableCooking;

	// Enables uploading of transformation changes back to Houdini Engine.
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Cooking)
	bool bUploadTransformsToHoudiniEngine;

	// Enables cooking upon transformation changes.
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Cooking)
	bool bTransformChangeTriggersCooks;

public:

#if WITH_EDITORONLY_DATA
#endif

};
