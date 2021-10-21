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

#include "HoudiniApi.h"
#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "UObject/WeakObjectPtr.h"

/*
namespace EHoudiniEngineTaskType
{
	enum Type
	{
		None,

		// This type corresponds to Houdini asset instantiation (without cooking).
		AssetInstantiation,

		// This type corresponds to Houdini asset cooking request.
		AssetCooking,

		// This type is used for asynchronous asset deletion.
		AssetDeletion
	};
}
*/

UENUM()
enum class EHoudiniEngineTaskType : uint8
{
	None,

	// This type corresponds to Houdini asset instantiation (without cooking).
	AssetInstantiation,

	// This type corresponds to Houdini asset cooking request.
	AssetCooking,

	// This type is used for asynchronous asset deletion.
	AssetDeletion,

	// This type is used when processing the results of a sucessful cook
	AssetProcess,
};

struct HOUDINIENGINE_API FHoudiniEngineTask
{
	// Constructors.
	FHoudiniEngineTask();
	FHoudiniEngineTask(EHoudiniEngineTaskType InTaskType, FGuid InHapiGUID);

	// GUID of this request.
	FGuid HapiGUID;

	// Type of this task.
	EHoudiniEngineTaskType TaskType;

	// Houdini asset for instantiation.
	TWeakObjectPtr< class UHoudiniAsset > Asset;

	// Name of the actor requesting this task.
	FString ActorName;

	// Asset Id.
	HAPI_NodeId AssetId;

	// Additional Node Id for the task
	// Can be used to apply a task to multiple nodes in the same HDA
	TArray<HAPI_NodeId> OtherNodeIds;
	// Cook results for each output node.
	TMap<HAPI_NodeId, bool> CookResults;

	bool bUseOutputNodes;
	bool bOutputTemplateGeos;

	// Library Id.
	HAPI_AssetLibraryId AssetLibraryId;

	// HAPI name of the asset.
	int32 AssetHapiName;

	// Is set to true if component has been loaded.
	//bool bLoadedComponent;
};
