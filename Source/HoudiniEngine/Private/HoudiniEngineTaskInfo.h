/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "HoudiniEngineTask.h"

#include "HoudiniEngineTaskInfo.generated.h"

UENUM()
enum class EHoudiniEngineTaskState : uint8
{
	None,

	// Indicates the current task is still running
	Working,

	// Indicates the task has successfully finished
	Success,

	// Indicates the task has finished with non fatal errors
	FinishedWithError,

	// Indicates the task has finished with fatal errors and should be terminated
	FinishedWithFatalError,

	// Indicates the task has been aborted (unused)
	Aborted
};

struct HOUDINIENGINE_API FHoudiniEngineTaskInfo
{
	// Constructors.
	FHoudiniEngineTaskInfo();
	FHoudiniEngineTaskInfo(
		HAPI_Result InResult, HAPI_NodeId InAssetId,
		EHoudiniEngineTaskType InTaskType,
		EHoudiniEngineTaskState InTaskState);

	// Current HAPI result.
	HAPI_Result Result;

	// Current Asset Id.
	HAPI_NodeId AssetId;

	// Type of task.
	EHoudiniEngineTaskType TaskType;

	// Current status.
	EHoudiniEngineTaskState TaskState;

	// String used for status / progress bar.
	FText StatusText;

	// Is set to true if corresponding task was issued for loaded component.
	//bool bLoadedComponent;
};
