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
#include "HoudiniEngineUtils.h"
#include "HoudiniAssetHandle.h"
#include "HoudiniApi.h"


UHoudiniAssetHandle::UHoudiniAssetHandle(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	HandleName(TEXT("")),
	HandleType(TEXT("")),
	HandleIdx(-1)
{

}


UHoudiniAssetHandle::~UHoudiniAssetHandle()
{

}


UHoudiniAssetHandle*
UHoudiniAssetHandle::Create(UHoudiniAssetComponent* InHoudiniAssetComponent, const HAPI_HandleInfo& HandleInfo,
	int32 HandleIdx, const FString& HandleName)
{
	UHoudiniAssetHandle* HoudiniAssetHandle = nullptr;

	TArray<HAPI_HandleBindingInfo> BindingInfos;
	BindingInfos.SetNumZeroed(HandleInfo.bindingsCount);

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetHandleBindingInfo(nullptr, InHoudiniAssetComponent->GetAssetId(), HandleIdx,
		&BindingInfos[0], 0, HandleInfo.bindingsCount))
	{
		return HoudiniAssetHandle;
	}

	// Retrieve type of this handle.
	FString HandleType;
	if(!FHoudiniEngineUtils::GetHoudiniString(HandleInfo.typeNameSH, HandleType))
	{
		return HoudiniAssetHandle;
	}

	TArray<HAPI_ParmId> AssetParameterIds;
	TArray<FString> AssetParameterNames;
	TArray<FString> HandleParameterNames;

	for(int32 HandleBindingIdx = 0; HandleBindingIdx < HandleInfo.bindingsCount; ++HandleBindingIdx)
	{
		const HAPI_HandleBindingInfo& HandleBindingInfo = BindingInfos[HandleBindingIdx];

		// Store parameter id.
		AssetParameterIds.Add(HandleBindingInfo.assetParmId);

		FString AssetParmName;
		if(!FHoudiniEngineUtils::GetHoudiniString(HandleBindingInfo.assetParmNameSH, AssetParmName))
		{
			return HoudiniAssetHandle;
		}

		FString AssetHandleName;
		if(!FHoudiniEngineUtils::GetHoudiniString(HandleBindingInfo.handleParmNameSH, AssetHandleName))
		{
			return HoudiniAssetHandle;
		}

		// Store asset parameter name.
		AssetParameterNames.Add(AssetParmName);

		// Store handle parameter name.
		HandleParameterNames.Add(AssetHandleName);
	}

	HoudiniAssetHandle = NewObject<UHoudiniAssetHandle>(InHoudiniAssetComponent,
		UHoudiniAssetHandle::StaticClass(), NAME_None, RF_Public);

	// Store handle index and name and type.
	HoudiniAssetHandle->HandleIdx = HandleIdx;
	HoudiniAssetHandle->HandleName = HandleName;
	HoudiniAssetHandle->HandleType = HandleType;

	// Set component and other information.
	HoudiniAssetHandle->HoudiniAssetComponent = InHoudiniAssetComponent;

	// Store controlled parameter ids and names.
	HoudiniAssetHandle->AssetParameterIds = AssetParameterIds;
	HoudiniAssetHandle->AssetParameterNames = AssetParameterNames;
	HoudiniAssetHandle->HandleParameterNames = HandleParameterNames;

	return HoudiniAssetHandle;
}


bool
UHoudiniAssetHandle::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	// This implementation is not a true parameter. This method should not be called.
	check(false);
	return false;
}


void
UHoudiniAssetHandle::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetHandle* HoudiniAssetHandle = Cast<UHoudiniAssetHandle>(InThis);
	if(HoudiniAssetHandle && !HoudiniAssetHandle->IsPendingKill())
	{

	}

	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}
