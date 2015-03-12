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


UHoudiniAssetHandle::UHoudiniAssetHandle(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{

}


UHoudiniAssetHandle::~UHoudiniAssetHandle()
{

}


UHoudiniAssetHandle*
UHoudiniAssetHandle::Create(UHoudiniAssetComponent* InHoudiniAssetComponent, const HAPI_HandleInfo& HandleInfo,
	int32 HandleIdx)
{
	UHoudiniAssetHandle* HoudiniAssetHandle = nullptr;

	TArray<HAPI_HandleBindingInfo> BindingInfos;
	BindingInfos.SetNumZeroed(HandleInfo.bindingsCount);

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetHandleBindingInfo(InHoudiniAssetComponent->GetAssetId(), HandleIdx,
		&BindingInfos[0], 0, HandleInfo.bindingsCount))
	{
		return HoudiniAssetHandle;
	}

	TArray<HAPI_ParmId> ParameterIds;
	TArray<FString> ParameterNames;

	for(int32 HandleBindingIdx = 0; HandleBindingIdx < HandleInfo.bindingsCount; ++HandleBindingIdx)
	{
		const HAPI_HandleBindingInfo& HandleBindingInfo = BindingInfos[HandleBindingIdx];

		// Store parameter id.
		ParameterIds.Add(HandleBindingInfo.assetParmId);

		FString ParmName;
		if(!FHoudiniEngineUtils::GetHoudiniString(HandleBindingInfo.assetParmNameSH, ParmName))
		{
			return HoudiniAssetHandle;
		}

		ParameterNames.Add(ParmName);
	}

	HoudiniAssetHandle = NewObject<UHoudiniAssetHandle>(InHoudiniAssetComponent);

	// Store handle index.
	HoudiniAssetHandle->HandleIdx = HandleIdx;

	// Set component and other information.
	HoudiniAssetHandle->HoudiniAssetComponent = InHoudiniAssetComponent;

	// Store controlled parameter ids and names.
	HoudiniAssetHandle->ParameterIds = ParameterIds;
	HoudiniAssetHandle->ParameterNames = ParameterNames;

	return HoudiniAssetHandle;
}
