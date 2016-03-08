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
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniApi.h"
#include "HoudiniEngineString.h"


HAPI_RSTOrder
UHoudiniHandleComponent::GetHapiRSTOrder(const TSharedPtr<FString>& StrPtr)
{
	if ( StrPtr.Get() )
	{
		FString& Str = *StrPtr;

		if (Str == "trs")	return HAPI_TRS;
		else if (Str == "tsr")	return HAPI_TSR;
		else if (Str == "rts")	return HAPI_RTS;
		else if (Str == "rst")	return HAPI_RST;
		else if (Str == "str")	return HAPI_STR;
		else if (Str == "srt")	return HAPI_SRT;
	}

	return HAPI_SRT;
}

HAPI_XYZOrder
UHoudiniHandleComponent::GetHapiXYZOrder(const TSharedPtr<FString>& StrPtr)
{
	if ( StrPtr.Get() )
	{
		FString& Str = *StrPtr;

		if (Str == "xyz")	return HAPI_XYZ;
		else if (Str == "xzy")	return HAPI_XZY;
		else if (Str == "yxz")	return HAPI_YXZ;
		else if (Str == "yzx")	return HAPI_YZX;
		else if (Str == "zxy")	return HAPI_ZXY;
		else if (Str == "zyx")	return HAPI_ZYX;
	}

	return HAPI_XYZ;
}

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

	HAPI_TransformEuler HapiEulerXform;
	HapiEulerXform.position[0] = HapiEulerXform.position[1] = HapiEulerXform.position[2] = 0.0f;
	HapiEulerXform.rotationEuler[0] = HapiEulerXform.rotationEuler[1] = HapiEulerXform.rotationEuler[2] = 0.0f;
	HapiEulerXform.scale[0] = HapiEulerXform.scale[1] = HapiEulerXform.scale[2] = 1.0f;

	TSharedPtr<FString> RSTOrderStrPtr, XYZOrderStrPtr;
	
	for ( const auto& BindingInfo : BindingInfos )
	{
		FString HandleParmName = TEXT("");
		FHoudiniEngineString HoudiniEngineString(BindingInfo.handleParmNameSH);
		HoudiniEngineString.ToFString(HandleParmName);

		const HAPI_AssetId AssetParmId = BindingInfo.assetParmId;

		(void)(	XformParms[EXformParameter::TX].Bind( HapiEulerXform.position[0], "tx", 0, HandleParmName, AssetParmId, Parameters )
			||	XformParms[EXformParameter::TY].Bind( HapiEulerXform.position[1], "ty", 1, HandleParmName, AssetParmId, Parameters )
			||	XformParms[EXformParameter::TZ].Bind( HapiEulerXform.position[2], "tz", 2, HandleParmName, AssetParmId, Parameters )

			||	XformParms[EXformParameter::RX].Bind( HapiEulerXform.rotationEuler[0], "rx", 0, HandleParmName, AssetParmId, Parameters )
			||	XformParms[EXformParameter::RY].Bind( HapiEulerXform.rotationEuler[1], "ry", 1, HandleParmName, AssetParmId, Parameters )
			||	XformParms[EXformParameter::RZ].Bind( HapiEulerXform.rotationEuler[2], "rz", 2, HandleParmName, AssetParmId, Parameters )

			||	XformParms[EXformParameter::SX].Bind( HapiEulerXform.scale[0], "sx", 0, HandleParmName, AssetParmId, Parameters )
			||	XformParms[EXformParameter::SY].Bind( HapiEulerXform.scale[1], "sy", 1, HandleParmName, AssetParmId, Parameters )
			||	XformParms[EXformParameter::SZ].Bind( HapiEulerXform.scale[2], "sz", 2, HandleParmName, AssetParmId, Parameters )

			||	RSTParm.Bind( RSTOrderStrPtr, "trs_order", 0, HandleParmName, AssetParmId, Parameters )
			||	RotOrderParm.Bind( XYZOrderStrPtr, "xyz_order", 0, HandleParmName, AssetParmId, Parameters )
		);
	}

	HapiEulerXform.rstOrder = GetHapiRSTOrder(RSTOrderStrPtr);
	HapiEulerXform.rotationOrder = GetHapiXYZOrder(XYZOrderStrPtr);

	FTransform UnrealXform;
	FHoudiniEngineUtils::TranslateHapiTransform(HapiEulerXform, UnrealXform);

	SetRelativeTransform(UnrealXform);
	return true;
}

void
UHoudiniHandleComponent::ResolveDuplicatedParameters(const TMap<HAPI_ParmId, UHoudiniAssetParameter*>& NewParameters)
{
	for ( size_t i = 0; i < EXformParameter::COUNT; ++i )
	{
		XformParms[i].ResolveDuplicated(NewParameters);
	}

	RSTParm.ResolveDuplicated(NewParameters);
	RotOrderParm.ResolveDuplicated(NewParameters);
}

void
UHoudiniHandleComponent::UpdateTransformParameters()
{
	HAPI_Transform HapiXform;
	FHoudiniEngineUtils::TranslateUnrealTransform(GetRelativeTransform(), HapiXform);

	const HAPI_Session* Session = FHoudiniEngine::Get().GetSession();

	float HapiMatrix[16];
	FHoudiniApi::ConvertTransformQuatToMatrix(Session, &HapiXform, HapiMatrix);

	HAPI_TransformEuler HapiEulerXform;
	FHoudiniApi::ConvertMatrixToEuler(
		Session,
		HapiMatrix,
		GetHapiRSTOrder( RSTParm.Get(TSharedPtr<FString>()) ),
		GetHapiXYZOrder( RotOrderParm.Get(TSharedPtr<FString>()) ),
		&HapiEulerXform
	);

	XformParms[EXformParameter::TX] = HapiEulerXform.position[0];
	XformParms[EXformParameter::TY] = HapiEulerXform.position[1];
	XformParms[EXformParameter::TZ] = HapiEulerXform.position[2];

	XformParms[EXformParameter::RX] = HapiEulerXform.rotationEuler[0];
	XformParms[EXformParameter::RY] = HapiEulerXform.rotationEuler[1];
	XformParms[EXformParameter::RZ] = HapiEulerXform.rotationEuler[2];

	XformParms[EXformParameter::SX] = HapiEulerXform.scale[0];
	XformParms[EXformParameter::SY] = HapiEulerXform.scale[1];
	XformParms[EXformParameter::SZ] = HapiEulerXform.scale[2];
}

void
UHoudiniHandleComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniHandleComponent* This = Cast<UHoudiniHandleComponent>(InThis);

	if ( This && !This->IsPendingKill() )
	{
		for ( size_t i = 0; i < EXformParameter::COUNT; ++i )
		{
			This->XformParms[i].AddReferencedObject(Collector, InThis);
		}

		This->RSTParm.AddReferencedObject(Collector, InThis);
		This->RotOrderParm.AddReferencedObject(Collector, InThis);
	}
}

void
UHoudiniHandleComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	for ( size_t i = 0; i < EXformParameter::COUNT; ++i )
	{
		Ar << XformParms[i];
	}

	Ar << RSTParm;
	Ar << RotOrderParm;
}

#if WITH_EDITOR

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

#endif

