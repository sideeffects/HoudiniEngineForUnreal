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
UHoudiniHandleComponent::Construct(
	HAPI_AssetId AssetId,
	int32 HandleIdx,
	const FString& HandleName,
	const HAPI_HandleInfo& HandleInfo,
	const TMap<HAPI_ParmId, UHoudiniAssetParameter*>& Parameters
)
{
	TArray<HAPI_HandleBindingInfo> BindingInfos;
	BindingInfos.SetNumZeroed(HandleInfo.bindingsCount);

	if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetHandleBindingInfo(
			FHoudiniEngine::Get().GetSession(),
			AssetId, HandleIdx, &BindingInfos[0], 0, HandleInfo.bindingsCount
		)
	)
	{
		return false;
	}

	/*float Tx = 0, Ty = 0, Tz = 0;
	float Rx = 0, Ry = 0, Rz = 0;
	float Sx = 1, Sy = 1, Sz = 1;

	HAPI_RSTOrder RSTOrder = HAPI_SRT;
	HAPI_XYZOrder XYZOrder = HAPI_XYZ;*/

	HAPI_TransformEuler HapiEulerXform;
	HapiEulerXform.position[0] = HapiEulerXform.position[1] = HapiEulerXform.position[2] = 0.0f;
	HapiEulerXform.rotationEuler[0] = HapiEulerXform.rotationEuler[1] = HapiEulerXform.rotationEuler[2] = 0.0f;
	HapiEulerXform.scale[0] = HapiEulerXform.scale[1] = HapiEulerXform.scale[2] = 1.0f;

	HapiEulerXform.rstOrder = HAPI_SRT;
	HapiEulerXform.rotationOrder = HAPI_XYZ;
	
	for ( const auto& BindingInfo : BindingInfos )
	{
		FString HandleParmName;
		FHoudiniEngineUtils::GetHoudiniString(BindingInfo.handleParmNameSH, HandleParmName);

		const HAPI_AssetId AssetParmId = BindingInfo.assetParmId;

		(void)(	BindHandleParameter<UHoudiniAssetParameterFloat>(HapiEulerXform.position[0], "tx", HandleParmName, AssetParmId, Parameters )
			||	BindHandleParameter<UHoudiniAssetParameterFloat>(HapiEulerXform.position[1], "ty", HandleParmName, AssetParmId, Parameters )
			||	BindHandleParameter<UHoudiniAssetParameterFloat>(HapiEulerXform.position[2], "tz", HandleParmName, AssetParmId, Parameters )

			||	BindHandleParameter<UHoudiniAssetParameterFloat>(HapiEulerXform.rotationEuler[0], "rx", HandleParmName, AssetParmId, Parameters )
			||	BindHandleParameter<UHoudiniAssetParameterFloat>(HapiEulerXform.rotationEuler[1], "ry", HandleParmName, AssetParmId, Parameters )
			||	BindHandleParameter<UHoudiniAssetParameterFloat>(HapiEulerXform.rotationEuler[2], "rz", HandleParmName, AssetParmId, Parameters )

			||	BindHandleParameter<UHoudiniAssetParameterFloat>(HapiEulerXform.scale[0], "sx", HandleParmName, AssetParmId, Parameters )
			||	BindHandleParameter<UHoudiniAssetParameterFloat>(HapiEulerXform.scale[1], "sy", HandleParmName, AssetParmId, Parameters )
			||	BindHandleParameter<UHoudiniAssetParameterFloat>(HapiEulerXform.scale[2], "sz", HandleParmName, AssetParmId, Parameters )

			||	BindHandleParameter<UHoudiniAssetParameterInt>(HapiEulerXform.rstOrder, "trs_order", HandleParmName, AssetParmId, Parameters )
			||	BindHandleParameter<UHoudiniAssetParameterInt>(HapiEulerXform.rotationOrder, "xyz_order", HandleParmName, AssetParmId, Parameters )
		);
	}

	float HapiMatrix[16];
	FHoudiniApi::ConvertTransformEulerToMatrix(FHoudiniEngine::Get().GetSession(), &HapiEulerXform, HapiMatrix);

	HAPI_Transform HapiQuatXform;
	FHoudiniApi::ConvertMatrixToQuat(FHoudiniEngine::Get().GetSession(), HapiMatrix, HAPI_SRT, &HapiQuatXform);

	FTransform UnrealXform;
	FHoudiniEngineUtils::TranslateHapiTransform(HapiQuatXform, UnrealXform);

	SetRelativeTransform(UnrealXform);

	return true;
}

void
UHoudiniHandleComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	//Ar << HoudiniGeoPartObject;
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
