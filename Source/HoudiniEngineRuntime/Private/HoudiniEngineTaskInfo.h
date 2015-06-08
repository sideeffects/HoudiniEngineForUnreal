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

#include "HoudiniEngineTask.h"


namespace EHoudiniEngineTaskState
{
	enum Type
	{
		None,

		Processing,
		FinishedInstantiation,
		FinishedInstantiationWithErrors,
		FinishedCooking,
		FinishedCookingWithErrors,
		Aborted
	};
}

struct FHoudiniEngineTaskInfo
{
	/** Constructors. **/
	FHoudiniEngineTaskInfo();
	FHoudiniEngineTaskInfo(HAPI_Result InResult, HAPI_AssetId InAssetId, EHoudiniEngineTaskType::Type InTaskType,
		EHoudiniEngineTaskState::Type InTaskState);

	/** Current HAPI result. **/
	HAPI_Result Result;

	/** Current Asset Id. **/
	HAPI_AssetId AssetId;

	/** Type of task. **/
	EHoudiniEngineTaskType::Type TaskType;

	/** Current status. **/
	EHoudiniEngineTaskState::Type TaskState;

	/** String used for status / progress bar. **/
	FText StatusText;

	/** Is set to true if corresponding task was issued for loaded component. **/
	bool bLoadedComponent;
};
