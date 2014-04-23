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
#include "HoudiniEngineAsset.generated.h"

UCLASS(DependsOn = UEngineTypes, BlueprintType, meta = (DisplayThumbnail = "true"))
class HOUDINIENGINE_API UHoudiniEngineAsset : public UObject//, public IInterface_CollisionDataProvider
{
	GENERATED_UCLASS_BODY()
};
