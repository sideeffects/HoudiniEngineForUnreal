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

namespace EHoudiniEngineTaskType
{
	enum Type
	{
		None,

		AssetInstantiation,
		AssetCooking
	};
}

class UHoudiniAssetInstance;

struct FHoudiniEngineTask
{
	/** Constructors. **/
	FHoudiniEngineTask();
	FHoudiniEngineTask(EHoudiniEngineTaskType::Type InTaskType, FGuid InHapiGUID, TWeakObjectPtr<UHoudiniAsset> InAsset);

	/** GUID of this request. **/
	FGuid HapiGUID;

	/** Type of this task. **/
	EHoudiniEngineTaskType::Type TaskType;

	/** Corresponding Houdini asset. **/
	TWeakObjectPtr<UHoudiniAsset> Asset;

	/** Name of the actor requesting this task. **/
	FString ActorName;
};
