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
#include "HoudiniAssetComponentTickFunction.generated.h"


class UHoudiniAssetComponent;


USTRUCT()
struct FHoudiniAssetComponentTickFunction : public FTickFunction
{
	GENERATED_USTRUCT_BODY()

	/** Houdini asset component this tick function belongs to. **/
	UHoudiniAssetComponent* Target;

	/** Tick function of this component. **/
	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) OVERRIDE;
};
