/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
*
* Produced by:
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniHandleComponent.h"
#include "HoudiniApi.h"


UHoudiniHandleComponent::UHoudiniHandleComponent(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)

{
}

UHoudiniHandleComponent::~UHoudiniHandleComponent()
{

}

bool
UHoudiniHandleComponent::Construct(const FString& HandleName, const HAPI_HandleInfo& Info)
{
	return true;
}

void
UHoudiniHandleComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	//int32 Version = 0; // Placeholder until we need to use it.
	//Ar << Version;

	//Ar << HoudiniGeoPartObject;

	//Ar << CurvePoints;
	//Ar << CurveDisplayPoints;

	//SerializeEnumeration<EHoudiniHandleComponentType::Enum>(Ar, CurveType);
	//SerializeEnumeration<EHoudiniHandleComponentMethod::Enum>(Ar, CurveMethod);
	//Ar << bClosedCurve;
}

void
UHoudiniHandleComponent::PostEditUndo()
{
	Super::PostEditUndo();

	UHoudiniAssetComponent* AttachComponent = Cast<UHoudiniAssetComponent>(AttachParent);
	if(AttachComponent)
	{
		//UploadControlPoints();
		AttachComponent->StartTaskAssetCooking(true);
	}
}

//void
//UHoudiniHandleComponent::NotifyHoudiniInputCurveChanged()
//{
//	if(HoudiniAssetInput)
//	{
//		HoudiniAssetInput->OnInputCurveChanged();
//	}
//}
