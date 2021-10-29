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

#include "HoudiniHandleTranslator.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineString.h"

#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineRuntimePrivatePCH.h"

#include "HoudiniAssetComponent.h"
#include "HoudiniParameter.h"
#include "HoudiniHandleComponent.h"


bool
FHoudiniHandleTranslator::UpdateHandles(UHoudiniAssetComponent* HAC) 
{
	if (!IsValid(HAC))
		return false;

	TArray<UHoudiniHandleComponent*> NewHandles;

	if (FHoudiniHandleTranslator::BuildAllHandles(HAC->GetAssetId(), HAC, HAC->HandleComponents, NewHandles)) 
	{
		
		HAC->HandleComponents = NewHandles;

	}

	return true;
}

bool 
FHoudiniHandleTranslator::BuildAllHandles(
	const HAPI_NodeId& AssetId,
	UHoudiniAssetComponent* OuterObject,
	TArray<UHoudiniHandleComponent*>& CurrentHandles,
	TArray<UHoudiniHandleComponent*>& NewHandles)
{
	if (AssetId < 0)
		return false;

	HAPI_AssetInfo AssetInfo;
	FHoudiniApi::AssetInfo_Init(&AssetInfo);

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo))
		return false;

	TMap<FString, UHoudiniHandleComponent*> CurrentHandlesByName;

	for (auto& Handle : CurrentHandles)
	{
		if (!Handle)
			continue;

		CurrentHandlesByName.Add(Handle->GetHandleName(), Handle);
	}

	// If there are handles
	if (AssetInfo.handleCount > 0) 
	{
		TArray<HAPI_HandleInfo> HandleInfos;
		HandleInfos.SetNumZeroed(AssetInfo.handleCount);

		for (int32 Idx = 0; Idx < HandleInfos.Num(); Idx++)
		{
			FHoudiniApi::HandleInfo_Init(&(HandleInfos[Idx]));
		}

		if (FHoudiniApi::GetHandleInfo(
			FHoudiniEngine::Get().GetSession(), AssetId,
			&HandleInfos[0], 0, AssetInfo.handleCount) != HAPI_RESULT_SUCCESS)
		{
			return false;
		}


		for (int32 HandleIdx = 0; HandleIdx < AssetInfo.handleCount; ++HandleIdx)
		{
			const HAPI_HandleInfo & HandleInfo = HandleInfos[HandleIdx];

			// If we do not have bindings, we can skip.
			if (HandleInfo.bindingsCount <= 0)
				continue;

			FString TypeName = TEXT("");
			EHoudiniHandleType HandleType = EHoudiniHandleType::Unsupported;
			{
				FHoudiniEngineString HoudiniEngineString(HandleInfo.typeNameSH);
				if (!HoudiniEngineString.ToFString(TypeName))
				{
					continue;
				}

				if (TypeName.Equals(TEXT(HAPI_UNREAL_HANDLE_TRANSFORM)))
					HandleType = EHoudiniHandleType::Xform;
				else if (TypeName.Equals(TEXT(HAPI_UNREAL_HANDLE_BOUNDER)))
					HandleType = EHoudiniHandleType::Bounder;
			}

			FString HandleName = TEXT("");

			{
				FHoudiniEngineString HoudiniEngineString(HandleInfo.nameSH);
				if (!HoudiniEngineString.ToFString(HandleName))
					continue;
			}

			if (HandleType == EHoudiniHandleType::Unsupported)
			{
				HOUDINI_LOG_DISPLAY(TEXT("%s: Unsupported Handle Type %s for handle %s"), 
					OuterObject ? *(OuterObject->GetName()) : TEXT("?"), *TypeName, *HandleName);
				continue;
			}

			UHoudiniHandleComponent* HandleComponent = nullptr;
			UHoudiniHandleComponent** FoundHandleComponent = CurrentHandlesByName.Find(HandleName);
			if (FoundHandleComponent)
			{
				HandleComponent = *FoundHandleComponent;

				CurrentHandles.Remove(*FoundHandleComponent);
				CurrentHandlesByName.Remove(HandleName);
			}
			else
			{
				HandleComponent = NewObject<UHoudiniHandleComponent>(
					OuterObject,
					UHoudiniHandleComponent::StaticClass(),
					NAME_None, RF_Public | RF_Transactional);

				HandleComponent->SetHandleName(HandleName);
				HandleComponent->SetHandleType(HandleType);

				// Change the creation method so the component is listed in the details panels
				HandleComponent->CreationMethod = EComponentCreationMethod::Instance;
			}

			if (!HandleComponent)
				continue;

			// If we have no parent, we need to re-attach.
			if (!HandleComponent->GetAttachParent())
			{
				HandleComponent->AttachToComponent(OuterObject, FAttachmentTransformRules::KeepRelativeTransform);
			}

			HandleComponent->SetVisibility(true);

			// If component is not registered, register it
			if (!HandleComponent->IsRegistered())
				HandleComponent->RegisterComponent();

			// Get handle's bindings
			TArray<HAPI_HandleBindingInfo> BindingInfos;
			BindingInfos.SetNumZeroed(HandleInfo.bindingsCount);
			
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetHandleBindingInfo(
				FHoudiniEngine::Get().GetSession(),
				AssetId, HandleIdx, &BindingInfos[0], 0, HandleInfo.bindingsCount))
				continue;

			HAPI_TransformEuler HapiEulerXform;
			FMemory::Memzero<HAPI_TransformEuler>(HapiEulerXform);
			HapiEulerXform.position[0] = HapiEulerXform.position[1] = HapiEulerXform.position[2] = 0.0f;
			HapiEulerXform.rotationEuler[0] = HapiEulerXform.rotationEuler[1] = HapiEulerXform.rotationEuler[2] = 0.0f;
			HapiEulerXform.scale[0] = HapiEulerXform.scale[1] = HapiEulerXform.scale[2] = 1.0f;

			TSharedPtr<FString> RSTOrderStrPtr, XYZOrderStrPtr;

			for (const auto& BindingInfo : BindingInfos) 
			{
				FString HandleParmName = TEXT("");
				FHoudiniEngineString HoudiniEngineString(BindingInfo.handleParmNameSH);
				HoudiniEngineString.ToFString(HandleParmName);

				const HAPI_ParmId ParamId = BindingInfo.assetParmId;

				TArray<UHoudiniHandleParameter*> &XformParms = HandleComponent->XformParms;

				UHoudiniParameter* FoundParam = nullptr;

				for (auto Param : OuterObject->Parameters) 
				{
					if (Param->GetParmId() == ParamId)
					{
						FoundParam = Param;
						break;
					}
				}
				
				HandleComponent->InitializeHandleParameters();

				if (!HandleComponent->CheckHandleValid())
					continue;

				(void)(XformParms[int32(EXformParameter::TX)]->Bind(HapiEulerXform.position[0], "tx", 0, HandleParmName, FoundParam)
					|| XformParms[int32(EXformParameter::TY)]->Bind(HapiEulerXform.position[1], "ty", 1, HandleParmName, FoundParam)
					|| XformParms[int32(EXformParameter::TZ)]->Bind(HapiEulerXform.position[2], "tz", 2, HandleParmName, FoundParam)

					|| XformParms[int32(EXformParameter::RX)]->Bind(HapiEulerXform.rotationEuler[0], "rx", 0, HandleParmName, FoundParam)
					|| XformParms[int32(EXformParameter::RY)]->Bind(HapiEulerXform.rotationEuler[1], "ry", 1, HandleParmName, FoundParam)
					|| XformParms[int32(EXformParameter::RZ)]->Bind(HapiEulerXform.rotationEuler[2], "rz", 2, HandleParmName, FoundParam)

					|| XformParms[int32(EXformParameter::SX)]->Bind(HapiEulerXform.scale[0], "sx", 0, HandleParmName, FoundParam)
					|| XformParms[int32(EXformParameter::SY)]->Bind(HapiEulerXform.scale[1], "sy", 1, HandleParmName, FoundParam)
					|| XformParms[int32(EXformParameter::SZ)]->Bind(HapiEulerXform.scale[2], "sz", 2, HandleParmName, FoundParam)

					|| HandleComponent->RSTParm->Bind(RSTOrderStrPtr, "trs_order", 0, HandleParmName, FoundParam)
					|| HandleComponent->RotOrderParm->Bind(XYZOrderStrPtr, "xyz_order", 0, HandleParmName, FoundParam)
					);
			}

			HapiEulerXform.rstOrder = GetHapiRSTOrder(RSTOrderStrPtr);
			HapiEulerXform.rotationOrder = GetHapiXYZOrder(XYZOrderStrPtr);

			constexpr float MaxFloat = TNumericLimits<float>::Max();
			constexpr float MinFloat = TNumericLimits<float>::Min();

			HapiEulerXform.scale[0] = FMath::Clamp(HapiEulerXform.scale[0], MinFloat, MaxFloat);
			HapiEulerXform.scale[1] = FMath::Clamp(HapiEulerXform.scale[1], MinFloat, MaxFloat);
			HapiEulerXform.scale[2] = FMath::Clamp(HapiEulerXform.scale[2], MinFloat, MaxFloat);

			FTransform UnrealXform;
			FHoudiniEngineUtils::TranslateHapiTransform(HapiEulerXform, UnrealXform);

			//HandleComponent->SetRelativeTransform(UnrealXform);

			NewHandles.Add(HandleComponent);

		}
	}

	return true;
}


void
FHoudiniHandleTranslator::ClearHandles(UHoudiniAssetComponent* HAC) 
{
	if (!IsValid(HAC))
		return;

	for (auto& HandleComponent : HAC->HandleComponents) 
	{
		if (!HandleComponent)
			continue;

		HandleComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		HandleComponent->UnregisterComponent();
		HandleComponent->DestroyComponent();
	}

	HAC->HandleComponents.Empty();
}

HAPI_RSTOrder 
FHoudiniHandleTranslator::GetHapiRSTOrder(const TSharedPtr<FString> & StrPtr) 
{
	if (StrPtr.Get())
	{
		FString & Str = *StrPtr;

		if (Str == "trs")   return HAPI_TRS;
		else if (Str == "tsr")  return HAPI_TSR;
		else if (Str == "rts")  return HAPI_RTS;
		else if (Str == "rst")  return HAPI_RST;
		else if (Str == "str")  return HAPI_STR;
		else if (Str == "srt")  return HAPI_SRT;
	}

	return HAPI_SRT;
}

HAPI_XYZOrder 
FHoudiniHandleTranslator::GetHapiXYZOrder(const TSharedPtr<FString> & StrPtr) 
{
	if (StrPtr.Get())
	{
		FString & Str = *StrPtr;

		if (Str == "xyz")   return HAPI_XYZ;
		else if (Str == "xzy")  return HAPI_XZY;
		else if (Str == "yxz")  return HAPI_YXZ;
		else if (Str == "yzx")  return HAPI_YZX;
		else if (Str == "zxy")  return HAPI_ZXY;
		else if (Str == "zyx")  return HAPI_ZYX;
	}

	return HAPI_XYZ;
}


void 
FHoudiniHandleTranslator::UpdateTransformParameters(UHoudiniHandleComponent* HandleComponent) 
{
	if (!IsValid(HandleComponent))
		return;

	if (!HandleComponent->CheckHandleValid())
		return;

	TArray<UHoudiniHandleParameter*> &XformParms = HandleComponent->XformParms;

	if (XformParms.Num() < (int32)EXformParameter::COUNT)
		return;

	HAPI_Transform HapiXform;
	FMemory::Memzero< HAPI_Transform >(HapiXform);
	FHoudiniEngineUtils::TranslateUnrealTransform(HandleComponent->GetRelativeTransform(), HapiXform);

	const HAPI_Session * Session = FHoudiniEngine::Get().GetSession();

	float HapiMatrix[16];
	FHoudiniApi::ConvertTransformQuatToMatrix(Session, &HapiXform, HapiMatrix);

	HAPI_TransformEuler HapiEulerXform;
	FMemory::Memzero< HAPI_TransformEuler >(HapiEulerXform);
	FHoudiniApi::ConvertMatrixToEuler(
		Session,
		HapiMatrix,
		GetHapiRSTOrder(HandleComponent->RSTParm->Get(TSharedPtr< FString >())),
		GetHapiXYZOrder(HandleComponent->RotOrderParm->Get(TSharedPtr< FString >())),
		&HapiEulerXform
	);

	*XformParms[int32(EXformParameter::TX)] = HapiEulerXform.position[0];
	*XformParms[int32(EXformParameter::TY)] = HapiEulerXform.position[1];
	*XformParms[int32(EXformParameter::TZ)] = HapiEulerXform.position[2];

	*XformParms[int32(EXformParameter::RX)] = FMath::RadiansToDegrees(HapiEulerXform.rotationEuler[0]);
	*XformParms[int32(EXformParameter::RY)] = FMath::RadiansToDegrees(HapiEulerXform.rotationEuler[1]);
	*XformParms[int32(EXformParameter::RZ)] = FMath::RadiansToDegrees(HapiEulerXform.rotationEuler[2]);

	constexpr float MaxFloat = TNumericLimits<float>::Max();
	constexpr float MinFloat = TNumericLimits<float>::Min();
	HapiEulerXform.scale[0] = FMath::Clamp(HapiEulerXform.scale[0], MinFloat, MaxFloat);
	HapiEulerXform.scale[1] = FMath::Clamp(HapiEulerXform.scale[1], MinFloat, MaxFloat);
	HapiEulerXform.scale[2] = FMath::Clamp(HapiEulerXform.scale[2], MinFloat, MaxFloat);

	*XformParms[int32(EXformParameter::SX)] = HapiEulerXform.scale[0];
	*XformParms[int32(EXformParameter::SY)] = HapiEulerXform.scale[1];
	*XformParms[int32(EXformParameter::SZ)] = HapiEulerXform.scale[2];
}