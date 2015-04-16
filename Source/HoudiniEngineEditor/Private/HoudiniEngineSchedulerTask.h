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

#include "HoudiniEngineSchedulerTaskType.h"


class UHoudiniAsset;
//class UHoudiniAssetComponent;

struct FHoudiniEngineSchedulerTask
{
	/** Constructors. **/
	FHoudiniEngineSchedulerTask();
	FHoudiniEngineSchedulerTask(EHoudiniEngineSchedulerTaskType::Enum InTaskType);

	/** Houdini asset for instantiation. **/
	TWeakObjectPtr<UHoudiniAsset> Asset;

	/** Houdini asset component for cooking. **/
	//TWeakObjectPtr<UHoudiniAssetComponent> AssetComponent;

	/** GUID of this request. **/
	FGuid HapiGUID;

	/** Type of this task. **/
	EHoudiniEngineSchedulerTaskType::Enum TaskType;

	/** Name of the actor requesting this task. **/
	FString ActorName;

	/** Asset Id. **/
	HAPI_AssetId AssetId;

	/** Flags used by Houdini component. **/
	union
	{
		struct
		{
			/** Is set to true if component has been loaded. **/
			uint32 bLoadedComponent : 1;
		};

		uint32 HoudiniEngineSchedulerTaskFlagsPacked;
	};
};
