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

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineTask.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetComponent.h"


FHoudiniEngineTask::FHoudiniEngineTask() :
	TaskType(EHoudiniEngineTaskType::None),
	ActorName(TEXT("")),
	AssetId(-1),
	AssetLibraryId(-1),
	AssetHapiName(-1),
	bLoadedComponent(false)
{
	HapiGUID.Invalidate();
}


FHoudiniEngineTask::FHoudiniEngineTask(EHoudiniEngineTaskType::Type InTaskType, FGuid InHapiGUID) :
	HapiGUID(InHapiGUID),
	TaskType(InTaskType),
	ActorName(TEXT("")),
	AssetId(-1),
	AssetLibraryId(-1),
	AssetHapiName(-1),
	bLoadedComponent(false)
{

}
