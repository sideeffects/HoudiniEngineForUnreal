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

#include "HoudiniEnginePrivatePCH.h"


FHoudiniEngineTaskInfo::FHoudiniEngineTaskInfo() :
	Result(HAPI_RESULT_SUCCESS),
	AssetId(-1),
	TaskType(EHoudiniEngineTaskType::None),
	TaskState(EHoudiniEngineTaskState::None)
{

}


FHoudiniEngineTaskInfo::FHoudiniEngineTaskInfo(HAPI_Result InResult, HAPI_AssetId InAssetId,
												EHoudiniEngineTaskType::Type InTaskType,
												EHoudiniEngineTaskState::Type InTaskState) :
	Result(InResult),
	AssetId(InAssetId),
	TaskType(InTaskType),
	TaskState(InTaskState)
{

}
