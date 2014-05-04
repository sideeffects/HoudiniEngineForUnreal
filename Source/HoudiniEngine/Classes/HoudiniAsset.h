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
#include "HoudiniAsset.generated.h"

UCLASS(DependsOn = UEngineTypes, BlueprintType)
class HOUDINIENGINE_API UHoudiniAsset : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	UHoudiniAsset(const FPostConstructInitializeProperties& PCIP, const char* InAssetName, HAPI_AssetId InAssetId);

protected:

	/** Holds this asset's name. **/
	FString AssetName;

	/** Holds this asset's handle. **/
	HAPI_AssetId AssetId;
};
