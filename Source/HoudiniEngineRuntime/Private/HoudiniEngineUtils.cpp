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
#include "HoudiniRuntimeSettings.h"
#include "HoudiniApi.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniAssetComponentMaterials.h"
#include "HoudiniAsset.h"


const FString kResultStringSuccess(TEXT("Success"));
const FString kResultStringFailure(TEXT("Generic Failure"));
const FString kResultStringAlreadyInitialized(TEXT("Already Initialized"));
const FString kResultStringNotInitialized(TEXT("Not Initialized"));
const FString kResultStringCannotLoadFile(TEXT("Unable to Load File"));
const FString kResultStringParmSetFailed(TEXT("Failed Setting Parameter"));
const FString kResultStringInvalidArgument(TEXT("Invalid Argument"));
const FString kResultStringCannotLoadGeo(TEXT("Uneable to Load Geometry"));
const FString kResultStringCannotGeneratePreset(TEXT("Uneable to Generate Preset"));
const FString kResultStringCannotLoadPreset(TEXT("Uneable to Load Preset"));


const float
FHoudiniEngineUtils::ScaleFactorPosition = 50.0f;


const float
FHoudiniEngineUtils::ScaleFactorTranslate = 50.0f;


const float
FHoudiniEngineUtils::ScaleSmallValue = KINDA_SMALL_NUMBER * 2.0f;


const int32
FHoudiniEngineUtils::PackageGUIDComponentNameLength = 12;

const int32
FHoudiniEngineUtils::PackageGUIDItemNameLength = 8;


const FString
FHoudiniEngineUtils::GetErrorDescription(HAPI_Result Result)
{
	if(HAPI_RESULT_SUCCESS == Result)
	{
		return kResultStringSuccess;
	}
	else
	{
		switch(Result)
		{
			case HAPI_RESULT_FAILURE:
			{
				return kResultStringAlreadyInitialized;
			}

			case HAPI_RESULT_ALREADY_INITIALIZED:
			{
				return kResultStringAlreadyInitialized;
			}

			case HAPI_RESULT_NOT_INITIALIZED:
			{
				return kResultStringNotInitialized;
			}

			case HAPI_RESULT_CANT_LOADFILE:
			{
				return kResultStringCannotLoadFile;
			}

			case HAPI_RESULT_PARM_SET_FAILED:
			{
				return kResultStringParmSetFailed;
			}

			case HAPI_RESULT_INVALID_ARGUMENT:
			{
				return kResultStringInvalidArgument;
			}

			case HAPI_RESULT_CANT_LOAD_GEO:
			{
				return kResultStringCannotLoadGeo;
			}

			case HAPI_RESULT_CANT_GENERATE_PRESET:
			{
				return kResultStringCannotGeneratePreset;
			}

			case HAPI_RESULT_CANT_LOAD_PRESET:
			{
				return kResultStringCannotLoadPreset;
			}

			default:
			{
				return kResultStringFailure;
			}
		};
	}
}


const FString
FHoudiniEngineUtils::GetErrorDescription()
{
	return FHoudiniEngineUtils::GetStatusString(HAPI_STATUS_CALL_RESULT, HAPI_STATUSVERBOSITY_ERRORS);
}


const FString
FHoudiniEngineUtils::GetCookState()
{
	return FHoudiniEngineUtils::GetStatusString(HAPI_STATUS_COOK_STATE, HAPI_STATUSVERBOSITY_ERRORS);
}


const FString
FHoudiniEngineUtils::GetCookResult()
{
	return FHoudiniEngineUtils::GetStatusString(HAPI_STATUS_COOK_RESULT, HAPI_STATUSVERBOSITY_MESSAGES);
}


bool
FHoudiniEngineUtils::IsInitialized()
{
	return (FHoudiniApi::IsHAPIInitialized() && HAPI_RESULT_SUCCESS == FHoudiniApi::IsInitialized( FHoudiniEngine::Get().GetSession() ));
}


bool
FHoudiniEngineUtils::ComputeAssetPresetBufferLength(
	HAPI_AssetId AssetId, int32& OutBufferLength)
{
	HAPI_AssetInfo AssetInfo;
	OutBufferLength = 0;

	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo), false);

	int32 BufferLength = 0;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPresetBufLength(
		FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId, HAPI_PRESETTYPE_BINARY, NULL, &BufferLength), false);

	OutBufferLength = BufferLength;
	return true;
}


bool
FHoudiniEngineUtils::SetAssetPreset(
	HAPI_AssetId AssetId, const TArray<char>& PresetBuffer)
{
	if(PresetBuffer.Num() > 0)
	{
		HAPI_AssetInfo AssetInfo;

		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPreset(
			FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId, HAPI_PRESETTYPE_BINARY, NULL, &PresetBuffer[0], PresetBuffer.Num()), false);

		return true;
	}

	return false;
}


bool
FHoudiniEngineUtils::GetAssetPreset(
	HAPI_AssetId AssetId, TArray<char>& PresetBuffer)
{
	PresetBuffer.Empty();

	HAPI_AssetInfo AssetInfo;
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo), false);

	int32 BufferLength = 0;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPresetBufLength(
		FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId, HAPI_PRESETTYPE_BINARY, NULL, &BufferLength), false);

	PresetBuffer.SetNumZeroed(BufferLength);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPreset(
		FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId, &PresetBuffer[0], PresetBuffer.Num()), false);

	return true;
}


bool
FHoudiniEngineUtils::IsHoudiniAssetValid(HAPI_AssetId AssetId)
{
	if(AssetId < 0)
	{
		return false;
	}

	HAPI_AssetInfo AssetInfo;
	int32 ValidationAnswer = 0;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo), false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::IsAssetValid(FHoudiniEngine::Get().GetSession(), AssetId, AssetInfo.validationId, &ValidationAnswer),
		false);

	return (0 != ValidationAnswer);
}


bool
FHoudiniEngineUtils::DestroyHoudiniAsset(HAPI_AssetId AssetId)
{
	return(HAPI_RESULT_SUCCESS == FHoudiniApi::DestroyAsset(FHoudiniEngine::Get().GetSession(), AssetId));
}


void
FHoudiniEngineUtils::ConvertUnrealString(const FString& UnrealString, std::string& String)
{
	String = TCHAR_TO_UTF8(*UnrealString);
}


bool
FHoudiniEngineUtils::GetHoudiniString(int32 Name, FString& NameString)
{
	std::string NamePlain;

	if(FHoudiniEngineUtils::GetHoudiniString(Name, NamePlain))
	{
		NameString = UTF8_TO_TCHAR(NamePlain.c_str());
		return true;
	}

	return false;
}


bool
FHoudiniEngineUtils::GetHoudiniString(int32 Name, std::string& NameString)
{
	if(Name < 0)
	{
		return false;
	}

	// For now we will load first asset only.
	int32 NameLength = 0;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetStringBufLength(FHoudiniEngine::Get().GetSession(), Name, &NameLength), false);

	if(NameLength)
	{
		std::vector<char> NameBuffer(NameLength, '\0');
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetString(FHoudiniEngine::Get().GetSession(), Name, &NameBuffer[0], NameLength), false);

		// Create and return string.
		NameString = std::string(NameBuffer.begin(), NameBuffer.end());
	}

	return true;
}


bool
FHoudiniEngineUtils::GetUniqueMaterialShopName(HAPI_AssetId AssetId, HAPI_MaterialId MaterialId, FString& Name)
{
	HAPI_AssetInfo AssetInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo), false);

	HAPI_MaterialInfo MaterialInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetMaterialInfo(FHoudiniEngine::Get().GetSession(), AssetId, MaterialId, &MaterialInfo), false);

	HAPI_NodeInfo AssetNodeInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId, &AssetNodeInfo), false);

	HAPI_NodeInfo MaterialNodeInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), MaterialInfo.nodeId, &MaterialNodeInfo), false);

	FString AssetNodeName;
	if(!FHoudiniEngineUtils::GetHoudiniString(AssetNodeInfo.internalNodePathSH, AssetNodeName))
	{
		return false;
	}

	FString MaterialNodeName;
	if(!FHoudiniEngineUtils::GetHoudiniString(MaterialNodeInfo.internalNodePathSH, MaterialNodeName))
	{
		return false;
	}

	if(AssetNodeName.Len() > 0 && MaterialNodeName.Len() > 0)
	{
		// Remove AssetNodeName part from MaterialNodeName. Extra position is for separator.
		Name = MaterialNodeName.Mid(AssetNodeName.Len() + 1);
		return true;
	}

	return false;
}


void
FHoudiniEngineUtils::TranslateHapiTransform(const HAPI_Transform& HapiTransform, FTransform& UnrealTransform)
{
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	float TransformScaleFactor = FHoudiniEngineUtils::ScaleFactorTranslate;
	EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

	if(HoudiniRuntimeSettings)
	{
		TransformScaleFactor = HoudiniRuntimeSettings->TransformScaleFactor;
		ImportAxis = HoudiniRuntimeSettings->ImportAxis;
	}

	if(HRSAI_Unreal == ImportAxis)
	{
		FQuat ObjectRotation(-HapiTransform.rotationQuaternion[0], -HapiTransform.rotationQuaternion[1],
							 -HapiTransform.rotationQuaternion[2], HapiTransform.rotationQuaternion[3]);
		Swap(ObjectRotation.Y, ObjectRotation.Z);

		FVector ObjectTranslation(HapiTransform.position[0], HapiTransform.position[1], HapiTransform.position[2]);
		ObjectTranslation *= TransformScaleFactor;
		Swap(ObjectTranslation[2], ObjectTranslation[1]);

		FVector ObjectScale3D(HapiTransform.scale[0], HapiTransform.scale[1], HapiTransform.scale[2]);
		Swap(ObjectScale3D.Y, ObjectScale3D.Z);

		UnrealTransform.SetComponents(ObjectRotation, ObjectTranslation, ObjectScale3D);
	}
	else if(HRSAI_Houdini == ImportAxis)
	{
		FQuat ObjectRotation(HapiTransform.rotationQuaternion[0], HapiTransform.rotationQuaternion[1],
							 HapiTransform.rotationQuaternion[2], HapiTransform.rotationQuaternion[3]);

		FVector ObjectTranslation(HapiTransform.position[0], HapiTransform.position[1], HapiTransform.position[2]);
		ObjectTranslation *= TransformScaleFactor;

		FVector ObjectScale3D(HapiTransform.scale[0], HapiTransform.scale[1], HapiTransform.scale[2]);

		UnrealTransform.SetComponents(ObjectRotation, ObjectTranslation, ObjectScale3D);
	}
	else
	{
		// Not valid enum value.
		check(0);
	}
}


void
FHoudiniEngineUtils::TranslateHapiTransform(const HAPI_TransformEuler& HapiTransformEuler, FTransform& UnrealTransform)
{
	float HapiMatrix[16];
	FHoudiniApi::ConvertTransformEulerToMatrix(FHoudiniEngine::Get().GetSession(), &HapiTransformEuler, HapiMatrix);

	HAPI_Transform HapiTransformQuat;
	FHoudiniApi::ConvertMatrixToQuat(FHoudiniEngine::Get().GetSession(), HapiMatrix, HAPI_SRT, &HapiTransformQuat);

	FHoudiniEngineUtils::TranslateHapiTransform(HapiTransformQuat, UnrealTransform);
}


void
FHoudiniEngineUtils::TranslateUnrealTransform(const FTransform& UnrealTransform, HAPI_Transform& HapiTransform)
{
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	float TransformScaleFactor = FHoudiniEngineUtils::ScaleFactorTranslate;
	EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

	if(HoudiniRuntimeSettings)
	{
		TransformScaleFactor = HoudiniRuntimeSettings->TransformScaleFactor;
		ImportAxis = HoudiniRuntimeSettings->ImportAxis;
	}

	HapiTransform.rstOrder = HAPI_SRT;

	FQuat UnrealRotation = UnrealTransform.GetRotation();
	FVector UnrealTranslation = UnrealTransform.GetTranslation();
	FVector UnrealScale = UnrealTransform.GetScale3D();

	if(HRSAI_Unreal == ImportAxis)
	{
		Swap(UnrealRotation.Y, UnrealRotation.Z);
		HapiTransform.rotationQuaternion[0] = -UnrealRotation.X;
		HapiTransform.rotationQuaternion[1] = -UnrealRotation.Y;
		HapiTransform.rotationQuaternion[2] = -UnrealRotation.Z;
		HapiTransform.rotationQuaternion[3] = UnrealRotation.W;

		UnrealTranslation /= TransformScaleFactor;
		Swap(UnrealTranslation.Y, UnrealTranslation.Z);
		HapiTransform.position[0] = UnrealTranslation.X;
		HapiTransform.position[1] = UnrealTranslation.Y;
		HapiTransform.position[2] = UnrealTranslation.Z;

		Swap(UnrealScale.Y, UnrealScale.Z);
		HapiTransform.scale[0] = UnrealScale.X;
		HapiTransform.scale[1] = UnrealScale.Y;
		HapiTransform.scale[2] = UnrealScale.Z;
	}
	else if(HRSAI_Houdini == ImportAxis)
	{
		HapiTransform.rotationQuaternion[0] = UnrealRotation.X;
		HapiTransform.rotationQuaternion[1] = UnrealRotation.Y;
		HapiTransform.rotationQuaternion[2] = UnrealRotation.Z;
		HapiTransform.rotationQuaternion[3] = UnrealRotation.W;

		HapiTransform.position[0] = UnrealTranslation.X;
		HapiTransform.position[1] = UnrealTranslation.Y;
		HapiTransform.position[2] = UnrealTranslation.Z;

		HapiTransform.scale[0] = UnrealScale.X;
		HapiTransform.scale[1] = UnrealScale.Y;
		HapiTransform.scale[2] = UnrealScale.Z;
	}
	else
	{
		// Not valid enum value.
		check(0);
	}
}


void
FHoudiniEngineUtils::TranslateUnrealTransform(const FTransform& UnrealTransform,
	HAPI_TransformEuler& HapiTransformEuler)
{
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	float TransformScaleFactor = FHoudiniEngineUtils::ScaleFactorTranslate;
	EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

	if(HoudiniRuntimeSettings)
	{
		TransformScaleFactor = HoudiniRuntimeSettings->TransformScaleFactor;
		ImportAxis = HoudiniRuntimeSettings->ImportAxis;
	}

	HapiTransformEuler.rstOrder = HAPI_SRT;
	HapiTransformEuler.rotationOrder = HAPI_XYZ;

	FQuat UnrealRotation = UnrealTransform.GetRotation();
	FRotator Rotator = UnrealRotation.Rotator();

	FVector UnrealTranslation = UnrealTransform.GetTranslation();
	UnrealTranslation /= TransformScaleFactor;

	FVector UnrealScale = UnrealTransform.GetScale3D();

	if(HRSAI_Unreal == ImportAxis)
	{
		HapiTransformEuler.rotationEuler[0] = -Rotator.Roll;
		HapiTransformEuler.rotationEuler[1] = -Rotator.Yaw;
		HapiTransformEuler.rotationEuler[2] = -Rotator.Pitch;

		Swap(UnrealTranslation.Y, UnrealTranslation.Z);
		HapiTransformEuler.position[0] = UnrealTranslation.X;
		HapiTransformEuler.position[1] = UnrealTranslation.Y;
		HapiTransformEuler.position[2] = UnrealTranslation.Z;

		Swap(UnrealScale.Y, UnrealScale.Z);
		HapiTransformEuler.scale[0] = UnrealScale.X;
		HapiTransformEuler.scale[1] = UnrealScale.Y;
		HapiTransformEuler.scale[2] = UnrealScale.Z;
	}
	else if(HRSAI_Houdini == ImportAxis)
	{
		HapiTransformEuler.rotationEuler[0] = Rotator.Roll;
		HapiTransformEuler.rotationEuler[1] = Rotator.Yaw;
		HapiTransformEuler.rotationEuler[2] = Rotator.Pitch;

		HapiTransformEuler.position[0] = UnrealTranslation.X;
		HapiTransformEuler.position[1] = UnrealTranslation.Y;
		HapiTransformEuler.position[2] = UnrealTranslation.Z;

		HapiTransformEuler.scale[0] = UnrealScale.X;
		HapiTransformEuler.scale[1] = UnrealScale.Y;
		HapiTransformEuler.scale[2] = UnrealScale.Z;
	}
	else
	{
		// Not valid enum value.
		check(0);
	}
}


bool
FHoudiniEngineUtils::SetCurrentTime(float CurrentTime)
{
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetTime(FHoudiniEngine::Get().GetSession(), CurrentTime), false);
	return true;
}


bool
FHoudiniEngineUtils::GetHoudiniAssetName(HAPI_AssetId AssetId, FString& NameString)
{
	HAPI_AssetInfo AssetInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo), false);

	return(FHoudiniEngineUtils::GetHoudiniString(AssetInfo.nameSH, NameString));
}


int32
FHoudiniEngineUtils::HapiGetGroupCountByType(HAPI_GroupType GroupType, HAPI_GeoInfo& GeoInfo)
{
	switch(GroupType)
	{
		case HAPI_GROUPTYPE_POINT:
		{
			return GeoInfo.pointGroupCount;
		}

		case HAPI_GROUPTYPE_PRIM:
		{
			return GeoInfo.primitiveGroupCount;
		}

		default:
		{
			break;
		}
	}

	return 0;
}


int32
FHoudiniEngineUtils::HapiGetElementCountByGroupType(HAPI_GroupType GroupType, HAPI_PartInfo& PartInfo)
{
	switch(GroupType)
	{
		case HAPI_GROUPTYPE_POINT:
		{
			return PartInfo.pointCount;
		}

		case HAPI_GROUPTYPE_PRIM:
		{
			return PartInfo.faceCount;
		}

		default:
		{
			break;
		}
	}

	return 0;
}


bool
FHoudiniEngineUtils::HapiGetGroupNames(
	HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_GroupType GroupType,
	TArray<FString>& GroupNames)
{
	HAPI_GeoInfo GeoInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGeoInfo(FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, &GeoInfo), false);

	int32 GroupCount = FHoudiniEngineUtils::HapiGetGroupCountByType(GroupType, GeoInfo);

	if(GroupCount > 0)
	{
		std::vector<int32> GroupNameHandles(GroupCount, 0);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGroupNames(FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, GroupType,
			&GroupNameHandles[0], GroupCount), false);

		for(int32 NameIdx = 0; NameIdx < GroupCount; ++NameIdx)
		{
			FString GroupName;
			FHoudiniEngineUtils::GetHoudiniString(GroupNameHandles[NameIdx], GroupName);
			GroupNames.Add(GroupName);
		}
	}

	return true;
}


bool
FHoudiniEngineUtils::HapiGetGroupMembership(
	HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId, HAPI_GroupType GroupType,
	const FString& GroupName, TArray<int32>& GroupMembership)
{
	HAPI_PartInfo PartInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, PartId, &PartInfo), false);

	int32 ElementCount = FHoudiniEngineUtils::HapiGetElementCountByGroupType(GroupType, PartInfo);
	std::string ConvertedGroupName = TCHAR_TO_UTF8(*GroupName);

	GroupMembership.SetNumUninitialized(ElementCount);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGroupMembership(FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, PartId, GroupType,
		ConvertedGroupName.c_str(), NULL, &GroupMembership[0], 0, ElementCount), false);

	return true;
}


bool
FHoudiniEngineUtils::HapiCheckGroupMembership(const FHoudiniGeoPartObject& HoudiniGeoPartObject, HAPI_GroupType GroupType, const FString& GroupName)
{
	return FHoudiniEngineUtils::HapiCheckGroupMembership(HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId, HoudiniGeoPartObject.GeoId,
		HoudiniGeoPartObject.PartId, GroupType, GroupName);
}


bool
FHoudiniEngineUtils::HapiCheckGroupMembership(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId,
	HAPI_GroupType GroupType, const FString& GroupName)
{
	TArray<int32> GroupMembership;
	if(FHoudiniEngineUtils::HapiGetGroupMembership(AssetId, ObjectId, GeoId, PartId, GroupType, GroupName, GroupMembership))
	{
		int32 GroupSum = 0;
		for(int32 Idx = 0; Idx < GroupMembership.Num(); ++Idx)
		{
			GroupSum += GroupMembership[Idx];
		}

		return GroupSum > 0;
	}

	return false;
}


void
FHoudiniEngineUtils::HapiRetrieveParameterName(const HAPI_ParmInfo& ParmInfo, FString& ParameterName)
{
	HAPI_StringHandle ParmNameHandle = ParmInfo.nameSH;
	if(ParmNameHandle >= 0 && FHoudiniEngineUtils::GetHoudiniString(ParmNameHandle, ParameterName))
	{
		return;
	}

	ParameterName = TEXT("");
}


void
FHoudiniEngineUtils::HapiRetrieveParameterNames(const TArray<HAPI_ParmInfo>& ParmInfos,
	TArray<std::string>& Names)
{
	static const std::string InvalidParameterName("Invalid Parameter Name");

	Names.Empty();

	for(int32 ParmIdx = 0; ParmIdx < ParmInfos.Num(); ++ParmIdx)
	{
		const HAPI_ParmInfo& NodeParmInfo = ParmInfos[ParmIdx];
		HAPI_StringHandle NodeParmHandle = NodeParmInfo.nameSH;

		int32 NodeParmNameLength = 0;
		FHoudiniApi::GetStringBufLength(FHoudiniEngine::Get().GetSession(), NodeParmHandle, &NodeParmNameLength);

		if(NodeParmNameLength)
		{
			std::vector<char> NodeParmName(NodeParmNameLength, '\0');

			HAPI_Result Result = FHoudiniApi::GetString(FHoudiniEngine::Get().GetSession(), NodeParmHandle, &NodeParmName[0], NodeParmNameLength);
			if(HAPI_RESULT_SUCCESS == Result)
			{
				Names.Add(std::string(NodeParmName.begin(), NodeParmName.end() - 1));
			}
			else
			{
				Names.Add(InvalidParameterName);
			}
		}
		else
		{
			Names.Add(InvalidParameterName);
		}
	}
}


bool
FHoudiniEngineUtils::HapiCheckAttributeExists(
	HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId, const char* Name,
	HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttribInfo;
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, PartId, Name, Owner,
		&AttribInfo))
	{
		return false;
	}

	return AttribInfo.exists;
}


bool
FHoudiniEngineUtils::HapiCheckAttributeExists(
	const FHoudiniGeoPartObject& HoudiniGeoPartObject, const char* Name, HAPI_AttributeOwner Owner)
{
	return FHoudiniEngineUtils::HapiCheckAttributeExists(HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId,
		HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, Name, Owner);
}


int32
FHoudiniEngineUtils::HapiFindParameterByName(const std::string& ParmName, const TArray<std::string>& Names)
{
	for(int32 Idx = 0; Idx < Names.Num(); ++Idx)
	{
		if(!ParmName.compare(0, ParmName.length(), Names[Idx]))
		{
			return Idx;
		}
	}

	return -1;
}


bool
FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
	HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId, const char* Name,
	HAPI_AttributeInfo& ResultAttributeInfo, TArray<float>& Data, int32 TupleSize)
{
	ResultAttributeInfo.exists = false;

	// Reset container size.
	Data.SetNumUninitialized(0);

	int32 OriginalTupleSize = TupleSize;
	HAPI_AttributeInfo AttributeInfo;
	for(int32 AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx)
	{
		HOUDINI_CHECK_ERROR_RETURN(
			FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, PartId, Name, (HAPI_AttributeOwner) AttrIdx,
			&AttributeInfo), false);

		if(AttributeInfo.exists)
		{
			break;
		}
	}

	if(!AttributeInfo.exists)
	{
		return false;
	}

	if(OriginalTupleSize > 0)
	{
		AttributeInfo.tupleSize = OriginalTupleSize;
	}

	// Allocate sufficient buffer for data.
	Data.SetNumUninitialized(AttributeInfo.count * AttributeInfo.tupleSize);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeFloatData(
		FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, PartId, Name, &AttributeInfo, -1, &Data[0], 0, AttributeInfo.count), false);

	// Store the retrieved attribute information.
	ResultAttributeInfo = AttributeInfo;
	return true;
}


bool
FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
	const FHoudiniGeoPartObject& HoudiniGeoPartObject, const char* Name, HAPI_AttributeInfo& ResultAttributeInfo,
	TArray<float>& Data, int32 TupleSize)
{
	return FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId,
		HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, Name, ResultAttributeInfo, Data, TupleSize);
}


bool
FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
	HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId, const char* Name,
	HAPI_AttributeInfo& ResultAttributeInfo, TArray<int32>& Data, int32 TupleSize)
{
	ResultAttributeInfo.exists = false;

	// Reset container size.
	Data.SetNumUninitialized(0);

	int32 OriginalTupleSize = TupleSize;
	HAPI_AttributeInfo AttributeInfo;
	for(int32 AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, PartId, Name,
			(HAPI_AttributeOwner) AttrIdx, &AttributeInfo), false);

		if(AttributeInfo.exists)
		{
			break;
		}
	}

	if(!AttributeInfo.exists)
	{
		return false;
	}

	if(OriginalTupleSize > 0)
	{
		AttributeInfo.tupleSize = OriginalTupleSize;
	}

	// Allocate sufficient buffer for data.
	Data.SetNumUninitialized(AttributeInfo.count * AttributeInfo.tupleSize);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeIntData(FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, PartId, Name,
		&AttributeInfo, -1, &Data[0], 0, AttributeInfo.count), false);

	// Store the retrieved attribute information.
	ResultAttributeInfo = AttributeInfo;
	return true;
}


bool
FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
	const FHoudiniGeoPartObject& HoudiniGeoPartObject, const char* Name, HAPI_AttributeInfo& ResultAttributeInfo,
	TArray<int32>& Data, int32 TupleSize)
{

	return FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId,
		HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, Name,
		ResultAttributeInfo, Data, TupleSize);
}


bool
FHoudiniEngineUtils::HapiGetAttributeDataAsString(
	HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId, const char* Name,
	HAPI_AttributeInfo& ResultAttributeInfo, TArray<FString>& Data, int32 TupleSize)
{
	ResultAttributeInfo.exists = false;

	// Reset container size.
	Data.Empty();

	int32 OriginalTupleSize = TupleSize;
	HAPI_AttributeInfo AttributeInfo;
	for(int32 AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, PartId, Name,
			(HAPI_AttributeOwner) AttrIdx, &AttributeInfo), false);

		if(AttributeInfo.exists)
		{
			break;
		}
	}

	if(!AttributeInfo.exists)
	{
		return false;
	}

	if(OriginalTupleSize > 0)
	{
		AttributeInfo.tupleSize = OriginalTupleSize;
	}

	TArray<HAPI_StringHandle> StringHandles;
	StringHandles.Init(-1, AttributeInfo.count * AttributeInfo.tupleSize);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, PartId, Name,
		&AttributeInfo, &StringHandles[0], 0, AttributeInfo.count), false);

	for(int32 Idx = 0; Idx < StringHandles.Num(); ++Idx)
	{
		FString HapiString;
		FHoudiniEngineUtils::GetHoudiniString(StringHandles[Idx], HapiString);
		Data.Add(HapiString);
	}

	// Store the retrieved attribute information.
	ResultAttributeInfo = AttributeInfo;
	return true;
}


bool
FHoudiniEngineUtils::HapiGetAttributeDataAsString(
	const FHoudiniGeoPartObject& HoudiniGeoPartObject, const char* Name, HAPI_AttributeInfo& ResultAttributeInfo,
	TArray<FString>& Data, int32 TupleSize)
{
	return FHoudiniEngineUtils::HapiGetAttributeDataAsString(HoudiniGeoPartObject.AssetId,
		HoudiniGeoPartObject.ObjectId, HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, Name,
		ResultAttributeInfo, Data, TupleSize);
}


bool
FHoudiniEngineUtils::HapiGetInstanceTransforms(
	HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId, TArray<FTransform>& Transforms)
{
	Transforms.Empty();

	// Number of instances is number of points.
	HAPI_PartInfo PartInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, PartId, &PartInfo), false);

	if(0 == PartInfo.pointCount)
	{
		return false;
	}

	TArray<HAPI_Transform> InstanceTransforms;
	InstanceTransforms.SetNumZeroed(PartInfo.pointCount);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetInstanceTransforms(FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, HAPI_SRT,
		&InstanceTransforms[0], 0, PartInfo.pointCount), false);

	for(int32 Idx = 0; Idx < PartInfo.pointCount; ++Idx)
	{
		const HAPI_Transform& HapiInstanceTransform = InstanceTransforms[Idx];
		FTransform TransformMatrix;
		FHoudiniEngineUtils::TranslateHapiTransform(HapiInstanceTransform, TransformMatrix);
		Transforms.Add(TransformMatrix);
	}

	return true;
}


bool
FHoudiniEngineUtils::HapiGetInstanceTransforms(
	const FHoudiniGeoPartObject& HoudiniGeoPartObject, TArray<FTransform>& Transforms)
{
	return FHoudiniEngineUtils::HapiGetInstanceTransforms(HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId,
		HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, Transforms);
}


bool
FHoudiniEngineUtils::HapiExtractImage(
	HAPI_ParmId NodeParmId, const HAPI_MaterialInfo& MaterialInfo, TArray<char>& ImageBuffer, const char* Type)
{
	HAPI_Result Result = FHoudiniApi::RenderTextureToImage(FHoudiniEngine::Get().GetSession(), MaterialInfo.assetId, MaterialInfo.id, NodeParmId);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		return false;
	}

	HAPI_ImageInfo ImageInfo;
	Result = FHoudiniApi::GetImageInfo(FHoudiniEngine::Get().GetSession(), MaterialInfo.assetId, MaterialInfo.id, &ImageInfo);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		return false;
	}

	ImageInfo.dataFormat = HAPI_IMAGE_DATA_INT8;
	ImageInfo.interleaved = true;
	ImageInfo.packing = HAPI_IMAGE_PACKING_RGBA;

	Result = FHoudiniApi::SetImageInfo(FHoudiniEngine::Get().GetSession(), MaterialInfo.assetId, MaterialInfo.id, &ImageInfo);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		return false;
	}

	int32 ImageBufferSize = 0;
	Result = FHoudiniApi::ExtractImageToMemory(FHoudiniEngine::Get().GetSession(), MaterialInfo.assetId, MaterialInfo.id, HAPI_RAW_FORMAT_NAME,
		Type, &ImageBufferSize);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		return false;
	}

	if(!ImageBufferSize)
	{
		return false;
	}

	ImageBuffer.SetNumUninitialized(ImageBufferSize);
	Result = FHoudiniApi::GetImageMemoryBuffer(FHoudiniEngine::Get().GetSession(), MaterialInfo.assetId, MaterialInfo.id, &ImageBuffer[0],
		ImageBufferSize);

	if(HAPI_RESULT_SUCCESS != Result)
	{
		return false;
	}

	return true;
}


#if WITH_EDITOR

UTexture2D*
FHoudiniEngineUtils::CreateUnrealTexture(UTexture2D* ExistingTexture, const HAPI_ImageInfo& ImageInfo, UPackage* Package,
	const FString& TextureName, EPixelFormat PixelFormat, const TArray<char>& ImageBuffer, const FString& TextureType)
{
	UTexture2D* Texture = nullptr;
	if(ExistingTexture)
	{
		Texture = ExistingTexture;
	}
	else
	{
		Texture = NewObject<UTexture2D>(Package, UTexture2D::StaticClass(), *TextureName,
			RF_Public | RF_Standalone | RF_Transactional);

		// Add meta information to package.
		FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(Package, Texture,
			HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
		FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(Package, Texture,
			HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *TextureName);

		FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(Package, Texture,
			HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE, *TextureType);
	}

	Texture->Source.Init(ImageInfo.xRes, ImageInfo.yRes, 1, 1, TSF_BGRA8);

	if(!TextureType.Compare(HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL))
	{
		Texture->CompressionSettings = TC_Normalmap;
		Texture->SRGB = false;
	}

	uint8* MipData = Texture->Source.LockMip(0);

	// Create base map.
	uint8* DestPtr = nullptr;
	uint32 SrcWidth = ImageInfo.xRes;
	uint32 SrcHeight = ImageInfo.yRes;
	const char* SrcData = &ImageBuffer[0];

	for(uint32 y = 0; y < SrcHeight; y++)
	{
		DestPtr = &MipData[(SrcHeight - 1 - y) * SrcWidth * sizeof(FColor)];

		for(uint32 x = 0; x < SrcWidth; x++)
		{
			uint32 DataOffset = y * SrcWidth * 4 + x * 4;

			*DestPtr++ = *(uint8*)(SrcData + DataOffset + 2); //B
			*DestPtr++ = *(uint8*)(SrcData + DataOffset + 1); //G
			*DestPtr++ = *(uint8*)(SrcData + DataOffset + 0); //R
			*DestPtr++ = *(uint8*)(SrcData + DataOffset + 3); //A
		}
	}

	// Unlock the texture.
	Texture->Source.UnlockMip(0);
	Texture->PostEditChange();

	return Texture;
}


void
FHoudiniEngineUtils::ResetRawMesh(FRawMesh& RawMesh)
{
	// Unlike Empty this will not change memory allocations.

	RawMesh.FaceMaterialIndices.Reset();
	RawMesh.FaceSmoothingMasks.Reset();
	RawMesh.VertexPositions.Reset();
	RawMesh.WedgeIndices.Reset();
	RawMesh.WedgeTangentX.Reset();
	RawMesh.WedgeTangentY.Reset();
	RawMesh.WedgeTangentZ.Reset();
	RawMesh.WedgeColors.Reset();

	for(int32 Idx = 0; Idx < MAX_MESH_TEXTURE_COORDS; ++Idx)
	{
		RawMesh.WedgeTexCoords[Idx].Reset();
	}
}

#endif


bool
FHoudiniEngineUtils::HapiGetParameterDataAsFloat(HAPI_NodeId NodeId, const std::string ParmName, float DefaultValue,
	float& OutValue)
{
	float Value = DefaultValue;
	bool bComputed = false;

	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo);

	TArray<HAPI_ParmInfo> NodeParams;
	NodeParams.SetNumUninitialized(NodeInfo.parmCount);
	FHoudiniApi::GetParameters(FHoudiniEngine::Get().GetSession(), NodeInfo.id, &NodeParams[0], 0, NodeInfo.parmCount);

	// Get names of parameters.
	TArray<std::string> NodeParamNames;
	FHoudiniEngineUtils::HapiRetrieveParameterNames(NodeParams, NodeParamNames);

	// See if parameter is present.
	int32 ParmNameIdx = FHoudiniEngineUtils::HapiFindParameterByName(ParmName, NodeParamNames);

	if(-1 != ParmNameIdx)
	{
		HAPI_ParmInfo& ParmInfo = NodeParams[ParmNameIdx];
		if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetParmFloatValues(FHoudiniEngine::Get().GetSession(), NodeId, &Value, ParmInfo.floatValuesIndex, 1))
		{
			bComputed = true;
		}
	}

	OutValue = Value;
	return bComputed;
}


bool
FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
	HAPI_NodeId NodeId, const std::string ParmName, int32 DefaultValue, int32& OutValue)
{
	int32 Value = DefaultValue;
	bool bComputed = false;

	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo);

	TArray<HAPI_ParmInfo> NodeParams;
	NodeParams.SetNumUninitialized(NodeInfo.parmCount);
	FHoudiniApi::GetParameters(FHoudiniEngine::Get().GetSession(), NodeInfo.id, &NodeParams[0], 0, NodeInfo.parmCount);

	// Get names of parameters.
	TArray<std::string> NodeParamNames;
	FHoudiniEngineUtils::HapiRetrieveParameterNames(NodeParams, NodeParamNames);

	// See if parameter is present.
	int32 ParmNameIdx = FHoudiniEngineUtils::HapiFindParameterByName(ParmName, NodeParamNames);

	if(-1 != ParmNameIdx)
	{
		HAPI_ParmInfo& ParmInfo = NodeParams[ParmNameIdx];
		if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetParmIntValues(FHoudiniEngine::Get().GetSession(), NodeId, &Value, ParmInfo.intValuesIndex, 1))
		{
			bComputed = true;
		}
	}

	OutValue = Value;
	return bComputed;
}


bool
FHoudiniEngineUtils::HapiGetParameterDataAsString(
	HAPI_NodeId NodeId, const std::string ParmName, const FString& DefaultValue, FString& OutValue)
{
	FString Value;
	bool bComputed = false;

	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo);

	TArray<HAPI_ParmInfo> NodeParams;
	NodeParams.SetNumUninitialized(NodeInfo.parmCount);
	FHoudiniApi::GetParameters(FHoudiniEngine::Get().GetSession(), NodeInfo.id, &NodeParams[0], 0, NodeInfo.parmCount);

	// Get names of parameters.
	TArray<std::string> NodeParamNames;
	FHoudiniEngineUtils::HapiRetrieveParameterNames(NodeParams, NodeParamNames);

	// See if parameter is present.
	int32 ParmNameIdx = FHoudiniEngineUtils::HapiFindParameterByName(ParmName, NodeParamNames);

	if(-1 != ParmNameIdx)
	{
		HAPI_ParmInfo& ParmInfo = NodeParams[ParmNameIdx];
		HAPI_StringHandle StringHandle;
		if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetParmStringValues(FHoudiniEngine::Get().GetSession(), NodeId, false, &StringHandle,
			ParmInfo.stringValuesIndex, 1) && FHoudiniEngineUtils::GetHoudiniString(StringHandle, Value))
		{
			bComputed = true;
		}
	}

	if(bComputed)
	{
		OutValue = Value;
	}
	else
	{
		OutValue = DefaultValue;
	}

	return bComputed;
}


bool
FHoudiniEngineUtils::HapiIsMaterialTransparent(const HAPI_MaterialInfo& MaterialInfo)
{
	float Alpha;
	FHoudiniEngineUtils::HapiGetParameterDataAsFloat(MaterialInfo.nodeId, "ogl_alpha", 1.0f, Alpha);

	return Alpha < 0.95f;
}


bool
FHoudiniEngineUtils::IsValidAssetId(HAPI_AssetId AssetId)
{
	return -1 != AssetId;
}


bool
FHoudiniEngineUtils::HapiCreateCurve(HAPI_AssetId& CurveAssetId)
{
#if WITH_EDITOR

	HAPI_AssetId AssetId = -1;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateCurve(FHoudiniEngine::Get().GetSession(), &AssetId), false);

	CurveAssetId = AssetId;

	HAPI_NodeId NodeId = -1;
	if(!FHoudiniEngineUtils::HapiGetNodeId(AssetId, 0, 0, NodeId))
	{
		return false;
	}

	// Submit default points to curve.
	HAPI_ParmId ParmId = -1;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmIdFromName(FHoudiniEngine::Get().GetSession(), NodeId, HAPI_UNREAL_PARAM_CURVE_COORDS, &ParmId),
		false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), NodeId,
		HAPI_UNREAL_PARAM_INPUT_CURVE_COORDS_DEFAULT, ParmId, 0), false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookAsset(FHoudiniEngine::Get().GetSession(), AssetId, nullptr), false);

#endif

	return true;
}


bool
FHoudiniEngineUtils::HapiGetNodeId(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_NodeId& NodeId)
{
	if(FHoudiniEngineUtils::IsValidAssetId(AssetId))
	{
		HAPI_GeoInfo GeoInfo;
		if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetGeoInfo(FHoudiniEngine::Get().GetSession(), AssetId, ObjectId, GeoId, &GeoInfo))
		{
			NodeId = GeoInfo.nodeId;
			return true;
		}
	}

	NodeId = -1;
	return false;
}


bool
FHoudiniEngineUtils::HapiCreateAndConnectAsset(HAPI_AssetId HostAssetId, int32 InputIndex,
	ALandscapeProxy* LandscapeProxy, HAPI_AssetId& ConnectedAssetId, bool bExportOnlySelected, bool bExportCurves)
{

#if WITH_EDITOR

	// If we don't have any landscapes or host asset is invalid then there's nothing to do.
	if(!LandscapeProxy || !FHoudiniEngineUtils::IsHoudiniAssetValid(HostAssetId))
	{
		return false;
	}

	// Check if connected asset id is invalid, if it is not, we need to create an input asset.
	if(ConnectedAssetId < 0)
	{
		HAPI_AssetId AssetId = -1;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateInputAsset(FHoudiniEngine::Get().GetSession(), &AssetId,
			nullptr), false);

		// Check if we have a valid id for this new input asset.
		if(!FHoudiniEngineUtils::IsHoudiniAssetValid(AssetId))
		{
			return false;
		}

		// We now have a valid id.
		ConnectedAssetId = AssetId;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookAsset(FHoudiniEngine::Get().GetSession(), AssetId, nullptr),
			false);
	}

	// Get runtime settings.
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	float GeneratedGeometryScaleFactor = FHoudiniEngineUtils::ScaleFactorPosition;
	EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

	if(HoudiniRuntimeSettings)
	{
		GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
		ImportAxis = HoudiniRuntimeSettings->ImportAxis;
	}

	const ULandscapeInfo* LandscapeInfo = LandscapeProxy->GetLandscapeInfo(false);

	TSet<ULandscapeComponent*> SelectedComponents;
	if(bExportOnlySelected && LandscapeInfo)
	{
		SelectedComponents = LandscapeInfo->GetSelectedComponents();
	}

	bExportOnlySelected = bExportOnlySelected && SelectedComponents.Num() > 0;

	int32 MinX = TNumericLimits<int32>::Max();
	int32 MinY = TNumericLimits<int32>::Max();
	int32 MaxX = TNumericLimits<int32>::Lowest();
	int32 MaxY = TNumericLimits<int32>::Lowest();

	for(int32 ComponentIdx = 0, ComponentNum = LandscapeProxy->LandscapeComponents.Num();
		ComponentIdx < ComponentNum; ComponentIdx++)
	{
		ULandscapeComponent* LandscapeComponent = LandscapeProxy->LandscapeComponents[ComponentIdx];

		if(bExportOnlySelected && !SelectedComponents.Contains(LandscapeComponent))
		{
			continue;
		}

		LandscapeComponent->GetComponentExtent(MinX, MinY, MaxX, MaxY);
	}

	const int32 ComponentSizeQuads = ((LandscapeProxy->ComponentSizeQuads + 1) >> LandscapeProxy->ExportLOD) - 1;
	const float ScaleFactor = (float) LandscapeProxy->ComponentSizeQuads / (float) ComponentSizeQuads;
	const int32 NumComponents = bExportOnlySelected ? SelectedComponents.Num() : LandscapeProxy->LandscapeComponents.Num();
	const int32 VertexCountPerComponent = FMath::Square(ComponentSizeQuads + 1);
	const int32 VertexCount = NumComponents * VertexCountPerComponent;
	const int32 TriangleCount = NumComponents * FMath::Square(ComponentSizeQuads) * 2;

	if(!VertexCount)
	{
		return false;
	}

	// Create part.
	HAPI_PartInfo Part;
	FMemory::Memzero<HAPI_PartInfo>(Part);
	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.vertexCount = 0;
	Part.faceCount = 0;
	Part.pointCount = VertexCount;
	Part.type = HAPI_PARTTYPE_MESH;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0,
		&Part), false);

	// Extract point data.

	TArray<float> AllPositions;
	AllPositions.SetNumUninitialized(VertexCount * 3);

	int32 AllPositionsIdx = 0;

	for(int32 ComponentIdx = 0, ComponentNum = LandscapeProxy->LandscapeComponents.Num();
		ComponentIdx < ComponentNum; ComponentIdx++)
	{
		ULandscapeComponent* LandscapeComponent = LandscapeProxy->LandscapeComponents[ComponentIdx];

		if(bExportOnlySelected && !SelectedComponents.Contains(LandscapeComponent))
		{
			continue;
		}

		FLandscapeComponentDataInterface CDI(LandscapeComponent, LandscapeProxy->ExportLOD);

		for(int32 VertexIdx = 0; VertexIdx < VertexCountPerComponent; VertexIdx++)
		{
			int32 VertX, VertY;
			CDI.VertexIndexToXY(VertexIdx, VertX, VertY);

			FVector PositionVector = CDI.GetLocalVertex(VertX, VertY) + LandscapeComponent->RelativeLocation;

			if(HRSAI_Unreal == ImportAxis)
			{
				AllPositions[AllPositionsIdx * 3 + 0] = PositionVector.X / GeneratedGeometryScaleFactor;
				AllPositions[AllPositionsIdx * 3 + 1] = PositionVector.Z / GeneratedGeometryScaleFactor;
				AllPositions[AllPositionsIdx * 3 + 2] = PositionVector.Y / GeneratedGeometryScaleFactor;
			}
			else if(HRSAI_Houdini == ImportAxis)
			{
				AllPositions[AllPositionsIdx * 3 + 0] = PositionVector.X / GeneratedGeometryScaleFactor;
				AllPositions[AllPositionsIdx * 3 + 1] = PositionVector.Y / GeneratedGeometryScaleFactor;
				AllPositions[AllPositionsIdx * 3 + 2] = PositionVector.Z / GeneratedGeometryScaleFactor;
			}
			else
			{
				// Not a valid enum value.
				check(0);
			}

			AllPositionsIdx++;
		}
	}

	// Create point attribute info.
	HAPI_AttributeInfo AttributeInfoPoint;
	FMemory::Memzero<HAPI_AttributeInfo>(AttributeInfoPoint);
	AttributeInfoPoint.count = VertexCount;
	AttributeInfoPoint.tupleSize = 3;
	AttributeInfoPoint.exists = true;
	AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0,
		HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint), false);

	// Now that we have raw positions, we can upload them for our attribute.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(FHoudiniEngine::Get().GetSession(), ConnectedAssetId,
		0, 0, HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint, AllPositions.GetData(), 0,
		AttributeInfoPoint.count), false);

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0),
		false);

	// Now we can connect assets together.
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::ConnectAssetGeometry(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, HostAssetId,
			InputIndex), false);

#endif

	return true;
}


bool
FHoudiniEngineUtils::HapiCreateAndConnectAsset(HAPI_AssetId HostAssetId, int32 InputIndex, UStaticMesh* StaticMesh,
	HAPI_AssetId& ConnectedAssetId)
{
#if WITH_EDITOR

	// If we don't have a static mesh, or host asset is invalid, there's nothing to do.
	if(!StaticMesh || !FHoudiniEngineUtils::IsHoudiniAssetValid(HostAssetId))
	{
		return false;
	}

	// Check if connected asset id is valid, if it is not, we need to create an input asset.
	if(ConnectedAssetId < 0)
	{
		HAPI_AssetId AssetId = -1;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateInputAsset(FHoudiniEngine::Get().GetSession(), &AssetId,
			nullptr), false);

		// Check if we have a valid id for this new input asset.
		if(!FHoudiniEngineUtils::IsHoudiniAssetValid(AssetId))
		{
			return false;
		}

		// We now have a valid id.
		ConnectedAssetId = AssetId;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookAsset(FHoudiniEngine::Get().GetSession(), AssetId, nullptr), false);
	}

	// Get runtime settings.
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	float GeneratedGeometryScaleFactor = FHoudiniEngineUtils::ScaleFactorPosition;
	EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;
	int32 GeneratedLightMapResolution = 32;

	if(HoudiniRuntimeSettings)
	{
		GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
		ImportAxis = HoudiniRuntimeSettings->ImportAxis;
		GeneratedLightMapResolution = HoudiniRuntimeSettings->LightMapResolution;
	}

	// Grab base LOD level.
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[0];

	// Load the existing raw mesh.
	FRawMesh RawMesh;
	SrcModel.RawMeshBulkData->LoadRawMesh(RawMesh);

	// Create part.
	HAPI_PartInfo Part;
	FMemory::Memzero<HAPI_PartInfo>(Part);
	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[ HAPI_ATTROWNER_POINT ] = 0;
	Part.attributeCounts[ HAPI_ATTROWNER_PRIM ] = 0;
	Part.attributeCounts[ HAPI_ATTROWNER_VERTEX ] = 0;
	Part.attributeCounts[ HAPI_ATTROWNER_DETAIL ] = 0;
	Part.vertexCount = RawMesh.WedgeIndices.Num();
	Part.faceCount =  RawMesh.WedgeIndices.Num() / 3;
	Part.pointCount = RawMesh.VertexPositions.Num();
	Part.type = HAPI_PARTTYPE_MESH;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0, &Part), false);

	// Create point attribute info.
	HAPI_AttributeInfo AttributeInfoPoint;
	FMemory::Memzero<HAPI_AttributeInfo>(AttributeInfoPoint);
	AttributeInfoPoint.count = RawMesh.VertexPositions.Num();
	AttributeInfoPoint.tupleSize = 3;
	AttributeInfoPoint.exists = true;
	AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_POSITION,
		&AttributeInfoPoint), false);

	// Extract vertices from static mesh.
	TArray<float> StaticMeshVertices;
	StaticMeshVertices.SetNumZeroed(RawMesh.VertexPositions.Num() * 3);
	for(int32 VertexIdx = 0; VertexIdx < RawMesh.VertexPositions.Num(); ++VertexIdx)
	{
		// Grab vertex at this index.
		const FVector& PositionVector = RawMesh.VertexPositions[VertexIdx];

		if(HRSAI_Unreal == ImportAxis)
		{
			StaticMeshVertices[VertexIdx * 3 + 0] = PositionVector.X / GeneratedGeometryScaleFactor;
			StaticMeshVertices[VertexIdx * 3 + 1] = PositionVector.Z / GeneratedGeometryScaleFactor;
			StaticMeshVertices[VertexIdx * 3 + 2] = PositionVector.Y / GeneratedGeometryScaleFactor;
		}
		else if(HRSAI_Houdini == ImportAxis)
		{
			StaticMeshVertices[VertexIdx * 3 + 0] = PositionVector.X / GeneratedGeometryScaleFactor;
			StaticMeshVertices[VertexIdx * 3 + 1] = PositionVector.Y / GeneratedGeometryScaleFactor;
			StaticMeshVertices[VertexIdx * 3 + 2] = PositionVector.Z / GeneratedGeometryScaleFactor;
		}
		else
		{
			// Not valid enum value.
			check(0);
		}
	}

	// Now that we have raw positions, we can upload them for our attribute.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_POSITION,
		&AttributeInfoPoint, StaticMeshVertices.GetData(), 0, AttributeInfoPoint.count), false);

	// See if we have texture coordinates to upload.
	for(int32 MeshTexCoordIdx = 0; MeshTexCoordIdx < MAX_STATIC_TEXCOORDS; ++MeshTexCoordIdx)
	{
		int32 StaticMeshUVCount = RawMesh.WedgeTexCoords[MeshTexCoordIdx].Num();

		if(StaticMeshUVCount > 0)
		{
			const TArray<FVector2D>& RawMeshUVs = RawMesh.WedgeTexCoords[MeshTexCoordIdx];
			TArray<FVector2D> StaticMeshUVs;
			StaticMeshUVs.SetNumZeroed(RawMeshUVs.Num());

			// Transfer UV data.
			for(int32 UVIdx = 0; UVIdx < StaticMeshUVCount; ++UVIdx)
			{
				StaticMeshUVs[UVIdx] = FVector2D(RawMeshUVs[UVIdx].X, 1.0 - RawMeshUVs[UVIdx].Y);
			}

			if(HRSAI_Unreal == ImportAxis)
			{
				// We need to re-index UVs for wedges we swapped (due to winding differences).
				for(int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeIndices.Num(); WedgeIdx += 3)
				{
					// We do not touch wedge 0 of this triangle.

					FVector2D WedgeUV1 = StaticMeshUVs[WedgeIdx + 1];
					FVector2D WedgeUV2 = StaticMeshUVs[WedgeIdx + 2];

					StaticMeshUVs[WedgeIdx + 1] = WedgeUV2;
					StaticMeshUVs[WedgeIdx + 2] = WedgeUV1;
				}
			}
			else if(HRSAI_Houdini == ImportAxis)
			{
				// Do nothing, data is in proper format.
			}
			else
			{
				// Not valid enum value.
				check(0);
			}

			// Construct attribute name for this index.
			std::string UVAttributeName = HAPI_UNREAL_ATTRIB_UV;

			if(MeshTexCoordIdx > 0)
			{
				UVAttributeName += std::to_string(MeshTexCoordIdx + 1);
			}

			const char* UVAttributeNameString = UVAttributeName.c_str();

			// Create attribute for UVs
			HAPI_AttributeInfo AttributeInfoVertex;
			FMemory::Memzero<HAPI_AttributeInfo>(AttributeInfoVertex);
			AttributeInfoVertex.count = StaticMeshUVCount;
			AttributeInfoVertex.tupleSize = 2;
			AttributeInfoVertex.exists = true;
			AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0, UVAttributeNameString,
				&AttributeInfoVertex), false);
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0,
				UVAttributeNameString, &AttributeInfoVertex, (const float*) StaticMeshUVs.GetData(), 0,
					AttributeInfoVertex.count), false);
		}
	}

	// See if we have normals to upload.
	if(RawMesh.WedgeTangentZ.Num() > 0)
	{
		TArray<FVector> ChangedNormals(RawMesh.WedgeTangentZ);

		if(HRSAI_Unreal == ImportAxis)
		{
			// We need to re-index normals for wedges we swapped (due to winding differences).
			for(int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeIndices.Num(); WedgeIdx += 3)
			{
				FVector TangentZ1 = ChangedNormals[WedgeIdx + 1];
				FVector TangentZ2 = ChangedNormals[WedgeIdx + 2];

				ChangedNormals[WedgeIdx + 1] = TangentZ2;
				ChangedNormals[WedgeIdx + 2] = TangentZ1;
			}

			for(int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeIndices.Num(); ++WedgeIdx)
			{
				Swap(ChangedNormals[WedgeIdx].Y, ChangedNormals[WedgeIdx].Z);
			}
		}
		else if(HRSAI_Houdini == ImportAxis)
		{
			// Do nothing, data is in proper format.
		}
		else
		{
			// Not valid enum value.
			check(0);
		}

		// Create attribute for normals.
		HAPI_AttributeInfo AttributeInfoVertex;
		FMemory::Memzero<HAPI_AttributeInfo>(AttributeInfoVertex);
		AttributeInfoVertex.count = ChangedNormals.Num();
		AttributeInfoVertex.tupleSize = 3;
		AttributeInfoVertex.exists = true;
		AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
		AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_NORMAL,
			&AttributeInfoVertex), false);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_NORMAL,
			&AttributeInfoVertex, (const float*) ChangedNormals.GetData(), 0, AttributeInfoVertex.count), false);
	}

	// See if we have colors to upload.
	if(RawMesh.WedgeColors.Num() > 0)
	{
		TArray<FLinearColor> ChangedColors;
		ChangedColors.SetNumUninitialized(RawMesh.WedgeColors.Num());

		if(HRSAI_Unreal == ImportAxis)
		{
			// We need to re-index colors for wedges we swapped (due to winding differences).
			for(int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeIndices.Num(); WedgeIdx += 3)
			{
				ChangedColors[WedgeIdx + 0] = FLinearColor(RawMesh.WedgeColors[WedgeIdx + 0]);
				ChangedColors[WedgeIdx + 1] = FLinearColor(RawMesh.WedgeColors[WedgeIdx + 2]);
				ChangedColors[WedgeIdx + 2] = FLinearColor(RawMesh.WedgeColors[WedgeIdx + 1]);
			}
		}
		else if(HRSAI_Houdini == ImportAxis)
		{
			ChangedColors = TArray<FLinearColor>(RawMesh.WedgeColors);
		}
		else
		{
			// Not valid enum value.
			check(0);
		}

		// Create attribute for colors.
		HAPI_AttributeInfo AttributeInfoVertex;
		FMemory::Memzero<HAPI_AttributeInfo>(AttributeInfoVertex);
		AttributeInfoVertex.count = ChangedColors.Num();
		AttributeInfoVertex.tupleSize = 4;
		AttributeInfoVertex.exists = true;
		AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
		AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_COLOR,
			&AttributeInfoVertex), false);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_COLOR,
			&AttributeInfoVertex, (const float*) ChangedColors.GetData(), 0, AttributeInfoVertex.count), false);
	}

	// Extract indices from static mesh.
	if(RawMesh.WedgeIndices.Num() > 0)
	{
		TArray<int32> StaticMeshIndices;
		StaticMeshIndices.SetNumUninitialized(RawMesh.WedgeIndices.Num());

		if(HRSAI_Unreal == ImportAxis)
		{
			for(int32 IndexIdx = 0; IndexIdx < RawMesh.WedgeIndices.Num(); IndexIdx += 3)
			{
				// Swap indices to fix winding order.
				StaticMeshIndices[IndexIdx + 0] = (RawMesh.WedgeIndices[IndexIdx + 0]);
				StaticMeshIndices[IndexIdx + 1] = (RawMesh.WedgeIndices[IndexIdx + 2]);
				StaticMeshIndices[IndexIdx + 2] = (RawMesh.WedgeIndices[IndexIdx + 1]);
			}
		}
		else if(HRSAI_Houdini == ImportAxis)
		{
			StaticMeshIndices = TArray<int32>(RawMesh.WedgeIndices);
		}
		else
		{
			// Not valid enum value.
			check(0);
		}

		// We can now set vertex list.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetVertexList(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0,
			StaticMeshIndices.GetData(), 0, StaticMeshIndices.Num()), false);

		// We need to generate array of face counts.
		TArray<int32> StaticMeshFaceCounts;
		StaticMeshFaceCounts.Init(3, Part.faceCount);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetFaceCounts(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0,
			StaticMeshFaceCounts.GetData(), 0, StaticMeshFaceCounts.Num()), false);
	}

	// Marshall face material indices.
	if(RawMesh.FaceMaterialIndices.Num() > 0)
	{
		// Create list of materials, one for each face.
		TArray<char*> StaticMeshFaceMaterials;
		FHoudiniEngineUtils::CreateFaceMaterialArray(StaticMesh->Materials, RawMesh.FaceMaterialIndices,
			StaticMeshFaceMaterials);

		// Get name of attribute used for marshalling materials.
		std::string MarshallingAttributeName = HAPI_UNREAL_ATTRIB_MATERIAL;
		if(HoudiniRuntimeSettings && !HoudiniRuntimeSettings->MarshallingAttributeMaterial.IsEmpty())
		{
			FHoudiniEngineUtils::ConvertUnrealString(HoudiniRuntimeSettings->MarshallingAttributeMaterial,
				MarshallingAttributeName);
		}

		// Create attribute for materials.
		HAPI_AttributeInfo AttributeInfoMaterial;
		FMemory::Memzero<HAPI_AttributeInfo>(AttributeInfoMaterial);
		AttributeInfoMaterial.count = RawMesh.FaceMaterialIndices.Num();
		AttributeInfoMaterial.tupleSize = 1;
		AttributeInfoMaterial.exists = true;
		AttributeInfoMaterial.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoMaterial.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

		bool bAttributeError = false;

		if(HAPI_RESULT_SUCCESS != FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0,
			MarshallingAttributeName.c_str(), &AttributeInfoMaterial))
		{
			bAttributeError = true;
		}

		if(HAPI_RESULT_SUCCESS != FHoudiniApi::SetAttributeStringData(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0,
			MarshallingAttributeName.c_str(), &AttributeInfoMaterial, (const char**) StaticMeshFaceMaterials.GetData(),
			0, StaticMeshFaceMaterials.Num()))
		{
			bAttributeError = true;
		}

		// Delete material names.
		FHoudiniEngineUtils::DeleteFaceMaterialArray(StaticMeshFaceMaterials);

		if(bAttributeError)
		{
			check(0);
			return false;
		}
	}

	// Marshall face smoothing masks.
	if(RawMesh.FaceSmoothingMasks.Num() > 0)
	{
		// Get name of attribute used for marshalling face smoothing masks.
		std::string MarshallingAttributeName = HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK;
		if(HoudiniRuntimeSettings && !HoudiniRuntimeSettings->MarshallingAttributeMaterial.IsEmpty())
		{
			FHoudiniEngineUtils::ConvertUnrealString(HoudiniRuntimeSettings->MarshallingAttributeFaceSmoothingMask,
				MarshallingAttributeName);
		}

		HAPI_AttributeInfo AttributeInfoSmoothingMasks;
		FMemory::Memzero<HAPI_AttributeInfo>(AttributeInfoSmoothingMasks);
		AttributeInfoSmoothingMasks.count = RawMesh.FaceSmoothingMasks.Num();
		AttributeInfoSmoothingMasks.tupleSize = 1;
		AttributeInfoSmoothingMasks.exists = true;
		AttributeInfoSmoothingMasks.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoSmoothingMasks.storage = HAPI_STORAGETYPE_INT;
		AttributeInfoSmoothingMasks.originalOwner = HAPI_ATTROWNER_INVALID;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0,
			MarshallingAttributeName.c_str(), &AttributeInfoSmoothingMasks), false);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntData(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0,
			MarshallingAttributeName.c_str(), &AttributeInfoSmoothingMasks,
			(const int32*) RawMesh.FaceSmoothingMasks.GetData(), 0, RawMesh.FaceSmoothingMasks.Num()), false);
	}

	// Marshall lightmap resolution.
	if(StaticMesh->LightMapResolution != GeneratedLightMapResolution)
	{
		std::string MarshallingAttributeName = HAPI_UNREAL_ATTRIB_LIGHTMAP_RESOLUTION;
		if(HoudiniRuntimeSettings && !HoudiniRuntimeSettings->MarshallingAttributeLightmapResolution.IsEmpty())
		{
			FHoudiniEngineUtils::ConvertUnrealString(HoudiniRuntimeSettings->MarshallingAttributeLightmapResolution,
				MarshallingAttributeName);
		}

		TArray<int32> LightMapResolutions;
		LightMapResolutions.Add(StaticMesh->LightMapResolution);

		HAPI_AttributeInfo AttributeInfoLightMapResolution;
		FMemory::Memzero<HAPI_AttributeInfo>(AttributeInfoLightMapResolution);
		AttributeInfoLightMapResolution.count = LightMapResolutions.Num();
		AttributeInfoLightMapResolution.tupleSize = 1;
		AttributeInfoLightMapResolution.exists = true;
		AttributeInfoLightMapResolution.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfoLightMapResolution.storage = HAPI_STORAGETYPE_INT;
		AttributeInfoLightMapResolution.originalOwner = HAPI_ATTROWNER_INVALID;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0,
			MarshallingAttributeName.c_str(), &AttributeInfoLightMapResolution), false);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntData(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0,
			MarshallingAttributeName.c_str(), &AttributeInfoLightMapResolution,
			(const int32*) LightMapResolutions.GetData(), 0, LightMapResolutions.Num()), false);
	}

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, 0), false);

	// Now we can connect assets together.
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::ConnectAssetGeometry(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 0, HostAssetId, InputIndex), false);

#endif

	return true;
}


bool
FHoudiniEngineUtils::HapiDisconnectAsset(HAPI_AssetId HostAssetId, int32 InputIndex)
{
#if WITH_EDITOR

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::DisconnectAssetGeometry(FHoudiniEngine::Get().GetSession(), HostAssetId, InputIndex), false);

#endif

	return true;
}


bool
FHoudiniEngineUtils::HapiConnectAsset(
	HAPI_AssetId AssetIdFrom, HAPI_ObjectId ObjectIdFrom, HAPI_AssetId AssetIdTo, int32 InputIndex)
{
#if WITH_EDITOR

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectAssetGeometry(FHoudiniEngine::Get().GetSession(), AssetIdFrom, ObjectIdFrom, AssetIdTo,
		InputIndex), false);

#endif

	return true;
}


UPackage*
FHoudiniEngineUtils::BakeCreateStaticMeshPackageForComponent(UHoudiniAssetComponent* HoudiniAssetComponent,
	const FHoudiniGeoPartObject& HoudiniGeoPartObject, FString& MeshName, FGuid& BakeGUID, bool bBake)
{
	UPackage* PackageNew = nullptr;

#if WITH_EDITOR

	FString PackageName;
	int32 BakeCount = 0;
	UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;

	const FGuid& ComponentGUID = HoudiniAssetComponent->GetComponentGuid();
	FString ComponentGUIDString = ComponentGUID.ToString().Left(FHoudiniEngineUtils::PackageGUIDComponentNameLength);

	while(true)
	{
		if(!BakeGUID.IsValid())
		{
			BakeGUID = FGuid::NewGuid();
		}

		// We only want half of generated guid string.
		FString BakeGUIDString = BakeGUID.ToString().Left(FHoudiniEngineUtils::PackageGUIDItemNameLength);
		FString PartName = TEXT("");

		if(HoudiniGeoPartObject.HasCustomName())
		{
			PartName = TEXT("_") + HoudiniGeoPartObject.PartName;
		}

		if(bBake)
		{
			MeshName = HoudiniAsset->GetName() + PartName + FString::Printf(TEXT("_bake%d_"), BakeCount) +
				FString::FromInt(HoudiniGeoPartObject.ObjectId) + TEXT("_") +
				FString::FromInt(HoudiniGeoPartObject.GeoId) + TEXT("_") +
				FString::FromInt(HoudiniGeoPartObject.PartId) + TEXT("_") +
				FString::FromInt(HoudiniGeoPartObject.SplitId) + TEXT("_") +
				HoudiniGeoPartObject.SplitName;

			PackageName = FPackageName::GetLongPackagePath(HoudiniAsset->GetOutermost()->GetName()) +
				TEXT("/") +
				MeshName;
		}
		else
		{
			MeshName = HoudiniAsset->GetName() + PartName + TEXT("_") +
				FString::FromInt(HoudiniGeoPartObject.ObjectId) + TEXT("_") +
				FString::FromInt(HoudiniGeoPartObject.GeoId) + TEXT("_") +
				FString::FromInt(HoudiniGeoPartObject.PartId) + TEXT("_") +
				FString::FromInt(HoudiniGeoPartObject.SplitId) + TEXT("_") +
				HoudiniGeoPartObject.SplitName + TEXT("_") +
				BakeGUIDString;

			PackageName = FPackageName::GetLongPackagePath(HoudiniAsset->GetOuter()->GetName()) +
				TEXT("/") +
				HoudiniAsset->GetName() +
				PartName +
				TEXT("_") +
				ComponentGUIDString +
				TEXT("/") +
				MeshName;
		}

		// Santize package name.
		PackageName = PackageTools::SanitizePackageName(PackageName);

		UObject* OuterPackage = nullptr;

		if(!bBake)
		{
			// If we are not baking, then use outermost package, since objects within our package need to be visible
			// to external operations, such as copy paste.
			OuterPackage = HoudiniAssetComponent->GetOutermost();
		}

		// See if package exists, if it does, we need to regenerate the name.
		UPackage* Package = FindPackage(OuterPackage, *PackageName);

		if(Package)
		{
			if(bBake)
			{
				// Increment bake counter.
				BakeCount++;
			}
			else
			{
				// Package does exist, there's a collision, we need to generate a new name.
				BakeGUID.Invalidate();
			}
		}
		else
		{
			// Create actual package.
			PackageNew = CreatePackage(OuterPackage, *PackageName);
			break;
		}
	}

#endif

	return PackageNew;
}


UPackage*
FHoudiniEngineUtils::BakeCreateBlueprintPackageForComponent(UHoudiniAssetComponent* HoudiniAssetComponent,
	FString& BlueprintName)
{
	UPackage* Package = nullptr;

#if WITH_EDITOR

	UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
	FGuid BakeGUID = FGuid::NewGuid();

	while(true)
	{
		if(!BakeGUID.IsValid())
		{
			BakeGUID = FGuid::NewGuid();
		}

		// We only want half of generated guid string.
		FString BakeGUIDString = BakeGUID.ToString().Left(FHoudiniEngineUtils::PackageGUIDItemNameLength);

		// Generate Blueprint name.
		BlueprintName = HoudiniAsset->GetName() + TEXT("_") + BakeGUIDString;

		// Generate unique package name.=
		FString PackageName = FPackageName::GetLongPackagePath(HoudiniAsset->GetOutermost()->GetName()) +
			TEXT("/") +
			BlueprintName;

		PackageName = PackageTools::SanitizePackageName(PackageName);

		// See if package exists, if it does, we need to regenerate the name.
		Package = FindPackage(nullptr, *PackageName);

		if(Package)
		{
			// Package does exist, there's a collision, we need to generate a new name.
			BakeGUID.Invalidate();
		}
		else
		{
			// Create actual package.
			Package = CreatePackage(nullptr, *PackageName);
			break;
		}
	}

#endif

	return Package;
}


UPackage*
FHoudiniEngineUtils::BakeCreateMaterialPackageForComponent(UHoudiniAssetComponent* HoudiniAssetComponent,
	const HAPI_MaterialInfo& MaterialInfo, FString& MaterialName, bool bBake)
{
	UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
	FString MaterialDescriptor;

	if(bBake)
	{
		MaterialDescriptor = HoudiniAsset->GetName() + TEXT("_material_") + FString::FromInt(MaterialInfo.id) + TEXT("_");
	}
	else
	{
		MaterialDescriptor = HoudiniAsset->GetName() + TEXT("_") + FString::FromInt(MaterialInfo.id) + TEXT("_");
	}

	return FHoudiniEngineUtils::BakeCreateMaterialPackageForComponent(HoudiniAssetComponent, MaterialDescriptor,
		MaterialName, bBake);
}


UPackage*
FHoudiniEngineUtils::BakeCreateMaterialPackageForComponent(UHoudiniAssetComponent* HoudiniAssetComponent,
	const FString& MaterialInfoDescriptor, FString& MaterialName, bool bBake)
{
	UPackage* PackageNew = nullptr;

#if WITH_EDITOR

	UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
	FGuid BakeGUID = FGuid::NewGuid();
	FString PackageName;

	const FGuid& ComponentGUID = HoudiniAssetComponent->GetComponentGuid();
	FString ComponentGUIDString = ComponentGUID.ToString().Left(FHoudiniEngineUtils::PackageGUIDComponentNameLength);

	while(true)
	{
		if(!BakeGUID.IsValid())
		{
			BakeGUID = FGuid::NewGuid();
		}

		// We only want half of generated guid string.
		FString BakeGUIDString = BakeGUID.ToString().Left(FHoudiniEngineUtils::PackageGUIDItemNameLength);

		// Generate material name.
		MaterialName = MaterialInfoDescriptor + BakeGUIDString;

		if(bBake)
		{
			// Generate unique package name.
			PackageName = FPackageName::GetLongPackagePath(HoudiniAsset->GetOutermost()->GetName()) +
				TEXT("/") +
				MaterialName;
		}
		else
		{
			// Generate unique package name.
			PackageName = FPackageName::GetLongPackagePath(HoudiniAsset->GetOuter()->GetName()) +
				TEXT("/") +
				HoudiniAsset->GetName() +
				TEXT("_") +
				ComponentGUIDString +
				TEXT("/") +
				MaterialName;
		}

		// Sanitize package name.
		PackageName = PackageTools::SanitizePackageName(PackageName);

		UObject* OuterPackage = nullptr;

		if(!bBake)
		{
			// If we are not baking, then use outermost package, since objects within our package need to be visible
			// to external operations, such as copy paste.
			OuterPackage = HoudiniAssetComponent->GetOutermost();
		}

		// See if package exists, if it does, we need to regenerate the name.
		UPackage* Package = FindPackage(OuterPackage, *PackageName);

		if(Package)
		{
			if(!bBake)
			{
				// Package does exist, there's a collision, we need to generate a new name.
				BakeGUID.Invalidate();
			}
		}
		else
		{
			// Create actual package.
			PackageNew = CreatePackage(OuterPackage, *PackageName);
			break;
		}
	}

#endif

	return PackageNew;
}


UPackage*
FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(UHoudiniAssetComponent* HoudiniAssetComponent,
	const HAPI_MaterialInfo& MaterialInfo, const FString& TextureType, FString& TextureName, bool bBake)
{
	UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
	FString TextureInfoDescriptor;

	if(bBake)
	{
		TextureInfoDescriptor = HoudiniAsset->GetName() + TEXT("_texture_") + FString::FromInt(MaterialInfo.id) +
			TEXT("_") + TextureType + TEXT("_");
	}
	else
	{
		TextureInfoDescriptor = HoudiniAsset->GetName() + TEXT("_") + FString::FromInt(MaterialInfo.id) + TEXT("_") +
			TextureType + TEXT("_");
	}

	return FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(HoudiniAssetComponent, TextureInfoDescriptor,
		TextureType, TextureName, bBake);
}


UPackage*
FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(UHoudiniAssetComponent* HoudiniAssetComponent,
	const FString& TextureInfoDescriptor, const FString& TextureType, FString& TextureName, bool bBake)
{
	UPackage* PackageNew = nullptr;

#if WITH_EDITOR

	UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
	FGuid BakeGUID = FGuid::NewGuid();
	FString PackageName;

	const FGuid& ComponentGUID = HoudiniAssetComponent->GetComponentGuid();
	FString ComponentGUIDString = ComponentGUID.ToString().Left(FHoudiniEngineUtils::PackageGUIDComponentNameLength);

	while(true)
	{
		if(!BakeGUID.IsValid())
		{
			BakeGUID = FGuid::NewGuid();
		}

		// We only want half of generated guid string.
		FString BakeGUIDString = BakeGUID.ToString().Left(FHoudiniEngineUtils::PackageGUIDItemNameLength);

		// Generate texture name.
		TextureName = TextureInfoDescriptor + BakeGUIDString;

		if(bBake)
		{
			// Generate unique package name.=
			PackageName = FPackageName::GetLongPackagePath(HoudiniAsset->GetOutermost()->GetName()) +
				TEXT("/") +
				TextureName;
		}
		else
		{
			// Generate unique package name.
			PackageName = FPackageName::GetLongPackagePath(HoudiniAsset->GetOuter()->GetName()) +
				TEXT("/") +
				HoudiniAsset->GetName() +
				TEXT("_") +
				ComponentGUIDString +
				TEXT("/") +
				TextureName;
		}

		// Sanitize package name.
		PackageName = PackageTools::SanitizePackageName(PackageName);

		UPackage* OuterPackage = nullptr;

		if(!bBake)
		{
			// If we are not baking, then use outermost package, since objects within our package need to be visible
			// to external operations, such as copy paste.
			OuterPackage = HoudiniAssetComponent->GetOutermost();
		}

		// See if package exists, if it does, we need to regenerate the name.
		UPackage* Package = FindPackage(OuterPackage, *PackageName);

		if(Package)
		{
			if(!bBake)
			{
				// Package does exist, there's a collision, we need to generate a new name.
				BakeGUID.Invalidate();
			}
		}
		else
		{
			// Create actual package.
			PackageNew = CreatePackage(OuterPackage, *PackageName);
			break;
		}
	}

#endif

	return PackageNew;
}


bool
FHoudiniEngineUtils::CreateStaticMeshesFromHoudiniAsset(
	UHoudiniAssetComponent* HoudiniAssetComponent, UPackage* Package,
	const TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshesIn,
	TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshesOut, FTransform& ComponentTransform)
{
#if WITH_EDITOR

	HAPI_AssetId AssetId = HoudiniAssetComponent->GetAssetId();
	UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;

	if(!FHoudiniEngineUtils::IsHoudiniAssetValid(AssetId) || !HoudiniAsset)
	{
		return false;
	}

	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	// Get runtime settings.
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	check(HoudiniRuntimeSettings);

	float GeneratedGeometryScaleFactor = FHoudiniEngineUtils::ScaleFactorPosition;
	EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

	if(HoudiniRuntimeSettings)
	{
		GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
		ImportAxis = HoudiniRuntimeSettings->ImportAxis;
	}

	// Get platform manager LOD specific information.
	ITargetPlatform* CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	check(CurrentPlatform);
	const FStaticMeshLODGroup& LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(NAME_None);
	int32 NumLODs = LODGroup.GetDefaultNumLODs();

	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	HAPI_AssetInfo AssetInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo), false);

	// Retrieve asset transform.
	HAPI_TransformEuler AssetEulerTransform;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetTransform(FHoudiniEngine::Get().GetSession(), AssetId, HAPI_SRT, HAPI_XYZ, &AssetEulerTransform),
		false);

	// Convert HAPI Euler transform to Unreal one.
	FTransform AssetUnrealTransform;
	TranslateHapiTransform(AssetEulerTransform, AssetUnrealTransform);
	ComponentTransform = AssetUnrealTransform;

	// Retrieve information about each object contained within our asset.
	TArray<HAPI_ObjectInfo> ObjectInfos;
	ObjectInfos.SetNumUninitialized(AssetInfo.objectCount);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetObjects(FHoudiniEngine::Get().GetSession(), AssetId, &ObjectInfos[0], 0, AssetInfo.objectCount),
		false);

	// Retrieve transforms for each object in this asset.
	TArray<HAPI_Transform> ObjectTransforms;
	ObjectTransforms.SetNumUninitialized(AssetInfo.objectCount);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetObjectTransforms(FHoudiniEngine::Get().GetSession(), AssetId, HAPI_SRT, &ObjectTransforms[0], 0,
		AssetInfo.objectCount), false);

	// Containers used for raw data extraction.
	TArray<int32> VertexList;
	TArray<float> Positions;
	TArray<float> TextureCoordinates[MAX_STATIC_TEXCOORDS];
	TArray<float> Normals;
	TArray<float> Colors;
	TArray<FString> FaceMaterials;
	TArray<int32> FaceSmoothingMasks;
	TArray<int32> LightMapResolutions;

	// Retrieve all used unique material ids.
	TSet<HAPI_MaterialId> UniqueMaterialIds;
	FHoudiniEngineUtils::ExtractUniqueMaterialIds(AssetInfo, UniqueMaterialIds);

	// Map to hold materials.
	TMap<FString, UMaterial*> Materials;

	// Create materials.
	FHoudiniEngineUtils::HapiCreateMaterials(HoudiniAssetComponent, AssetInfo, UniqueMaterialIds, Materials);

	// Cache all materials inside the component.
	HoudiniAssetComponent->HoudiniAssetComponentMaterials->Assignments = Materials;

	// If we have no package, we will use transient package.
	if(!Package)
	{
		Package = GetTransientPackage();
	}

	UStaticMesh* StaticMesh = nullptr;
	FString MeshName;
	FGuid MeshGuid;

	// Iterate through all objects.
	for(int32 ObjectIdx = 0; ObjectIdx < ObjectInfos.Num(); ++ObjectIdx)
	{
		// Retrieve object at this index.
		const HAPI_ObjectInfo& ObjectInfo = ObjectInfos[ObjectIdx];

		// Retrieve object name.
		FString ObjectName = TEXT("");
		FHoudiniEngineUtils::GetHoudiniString(ObjectInfo.nameSH, ObjectName);

		// Get transformation for this object.
		const HAPI_Transform& ObjectTransform = ObjectTransforms[ObjectIdx];
		FTransform TransformMatrix;
		FHoudiniEngineUtils::TranslateHapiTransform(ObjectTransform, TransformMatrix);

		// Iterate through all Geo informations within this object.
		for(int32 GeoIdx = 0; GeoIdx < ObjectInfo.geoCount; ++GeoIdx)
		{
			// Get Geo information.
			HAPI_GeoInfo GeoInfo;
			if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetGeoInfo(FHoudiniEngine::Get().GetSession(), AssetId, ObjectInfo.id, GeoIdx, &GeoInfo))
			{
				HOUDINI_LOG_MESSAGE(
					TEXT("Creating Static Meshes: Object [%d %s], Geo [%d] unable to retrieve GeoInfo, ")
					TEXT("- skipping."),
					ObjectIdx, *ObjectName, GeoIdx);
				continue;
			}

			if(HAPI_GEOTYPE_CURVE == GeoInfo.type)
			{
				// If this geo is a curve, we skip part processing.
				FHoudiniGeoPartObject HoudiniGeoPartObject(TransformMatrix, ObjectName, ObjectName, AssetId,
					ObjectInfo.id, GeoInfo.id, 0);
				HoudiniGeoPartObject.bIsVisible = ObjectInfo.isVisible;
				HoudiniGeoPartObject.bIsInstancer = false;
				HoudiniGeoPartObject.bIsCurve = true;
				HoudiniGeoPartObject.bIsEditable = GeoInfo.isEditable;
				HoudiniGeoPartObject.bHasGeoChanged = GeoInfo.hasGeoChanged;

				StaticMesh = nullptr;
				StaticMeshesOut.Add(HoudiniGeoPartObject, StaticMesh);
				continue;
			}

			// Right now only care about display SOPs.
			if(!GeoInfo.isDisplayGeo)
			{
				continue;
			}

			bool bGeoError = false;

			// Get object / geo group memberships for primitives.
			TArray<FString> ObjectGeoGroupNames;
			FHoudiniEngineUtils::HapiGetGroupNames(AssetId, ObjectInfo.id, GeoIdx, HAPI_GROUPTYPE_PRIM,
				ObjectGeoGroupNames);

			bool bIsRenderCollidable = false;
			bool bIsCollidable = false;

			if(HoudiniRuntimeSettings)
			{
				// Detect if this object has collision geo or rendered collision geo.
				for(int32 GeoGroupNameIdx = 0; GeoGroupNameIdx < ObjectGeoGroupNames.Num(); ++GeoGroupNameIdx)
				{
					const FString& GroupName = ObjectGeoGroupNames[GeoGroupNameIdx];

					if(!HoudiniRuntimeSettings->RenderedCollisionGroupNamePrefix.IsEmpty() &&
						GroupName.StartsWith(HoudiniRuntimeSettings->RenderedCollisionGroupNamePrefix,
							ESearchCase::IgnoreCase))
					{
						bIsRenderCollidable = true;
					}
					else if(!HoudiniRuntimeSettings->CollisionGroupNamePrefix.IsEmpty() &&
							GroupName.StartsWith(HoudiniRuntimeSettings->CollisionGroupNamePrefix,
								ESearchCase::IgnoreCase))
					{
						bIsCollidable = true;
					}
				}
			}

			for(int32 PartIdx = 0; PartIdx < GeoInfo.partCount; ++PartIdx)
			{
				// Get part information.
				HAPI_PartInfo PartInfo;
				FString PartName = TEXT("");

				if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), AssetId, ObjectInfo.id, GeoInfo.id, PartIdx,
					&PartInfo))
				{
					// Error retrieving part info.
					bGeoError = true;
					HOUDINI_LOG_MESSAGE(
						TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve PartInfo, ")
						TEXT("- skipping."),
						ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName);
					continue;
				}

				// Retrieve part name.
				FHoudiniEngineUtils::GetHoudiniString(PartInfo.nameSH, PartName);

				// Get name of attribute used for marshalling generated mesh name.
				HAPI_AttributeInfo AttribGeneratedMeshName;
				TArray<FString> GeneratedMeshNames;

				{
					std::string MarshallingAttributeName = HAPI_UNREAL_ATTRIB_GENERATED_MESH_NAME;
					if(HoudiniRuntimeSettings)
					{
						FHoudiniEngineUtils::ConvertUnrealString(HoudiniRuntimeSettings->MarshallingAttributeGeneratedMeshName,
							MarshallingAttributeName);
					}

					FHoudiniEngineUtils::HapiGetAttributeDataAsString(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
						MarshallingAttributeName.c_str(), AttribGeneratedMeshName, GeneratedMeshNames);
				}

				// There are no vertices and no points.
				if(PartInfo.vertexCount <= 0 && PartInfo.pointCount <= 0)
				{
					bGeoError = true;
					HOUDINI_LOG_MESSAGE(
						TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] no points or vertices found, ")
						TEXT("- skipping."),
						ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName);
					continue;
				}

				// Retrieve material information for this geo part.
				TArray<HAPI_MaterialId> FaceMaterialIds;
				HAPI_Bool bSingleFaceMaterial = false;
				bool bMaterialsFound = false;
				bool bMaterialsChanged = false;

				if(PartInfo.faceCount > 0)
				{
					FaceMaterialIds.SetNumUninitialized(PartInfo.faceCount);

					if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetMaterialIdsOnFaces(FHoudiniEngine::Get().GetSession(), AssetId, ObjectInfo.id,
						GeoInfo.id, PartInfo.id, &bSingleFaceMaterial, &FaceMaterialIds[0], 0, PartInfo.faceCount))
					{
						// Error retrieving material face assignments.
						bGeoError = true;
						HOUDINI_LOG_MESSAGE(
							TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve material face assignments, ")
							TEXT("- skipping."),
							ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName);
						continue;
					}

					// Set flag if we have materials.
					for(int32 MaterialIdx = 0; MaterialIdx < FaceMaterialIds.Num(); ++MaterialIdx)
					{
						if(-1 != FaceMaterialIds[MaterialIdx])
						{
							bMaterialsFound = true;
							break;
						}
					}

					// Set flag if any of the materials have changed.
					if(bMaterialsFound)
					{
						for(int32 MaterialFaceIdx = 0; MaterialFaceIdx < FaceMaterialIds.Num(); ++MaterialFaceIdx)
						{
							HAPI_MaterialInfo MaterialInfo;

							if(HAPI_RESULT_SUCCESS !=
								FHoudiniApi::GetMaterialInfo(FHoudiniEngine::Get().GetSession(), AssetInfo.id, FaceMaterialIds[MaterialFaceIdx],
									&MaterialInfo))
							{
								continue;
							}

							if(MaterialInfo.hasChanged)
							{
								bMaterialsChanged = true;
							}
						}
					}
				}

				// Create geo part object identifier.
				FHoudiniGeoPartObject HoudiniGeoPartObject(TransformMatrix, ObjectName, PartName, AssetId,
					ObjectInfo.id, GeoInfo.id, PartInfo.id);

				HoudiniGeoPartObject.bIsVisible = ObjectInfo.isVisible;
				HoudiniGeoPartObject.bIsInstancer = ObjectInfo.isInstancer;
				HoudiniGeoPartObject.bIsCurve = (PartInfo.type == HAPI_PARTTYPE_CURVE);
				HoudiniGeoPartObject.bIsEditable = GeoInfo.isEditable;
				HoudiniGeoPartObject.bHasGeoChanged = GeoInfo.hasGeoChanged;
				HoudiniGeoPartObject.bIsBox = (PartInfo.type == HAPI_PARTTYPE_BOX);
				HoudiniGeoPartObject.bIsSphere = (PartInfo.type == HAPI_PARTTYPE_SPHERE);

				if(AttribGeneratedMeshName.exists && GeneratedMeshNames.Num() > 0)
				{
					const FString& CustomPartName = GeneratedMeshNames[0];
					if(!CustomPartName.IsEmpty())
					{
						HoudiniGeoPartObject.SetCustomName(CustomPartName);
					}
				}

				// We do not create mesh for instancers.
				if(ObjectInfo.isInstancer)
				{
					if(PartInfo.pointCount > 0)
					{
						// Instancer objects have no mesh assigned.
						StaticMesh = nullptr;
						StaticMeshesOut.Add(HoudiniGeoPartObject, StaticMesh);
						continue;
					}
					else
					{
						// This is an instancer with no points.
						bGeoError = true;
						HOUDINI_LOG_MESSAGE(
							TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] is instancer but has 0 points ")
							TEXT("skipping."),
							ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName);
						continue;
					}
				}
				else if(PartInfo.type == HAPI_PARTTYPE_CURVE)
				{
					// This is a curve part.
					StaticMesh = nullptr;
					StaticMeshesOut.Add(HoudiniGeoPartObject, StaticMesh);
					continue;
				}
				else if(PartInfo.vertexCount <= 0)
				{
					// This is not an instancer, but we do not have vertices, skip.
					bGeoError = true;
					HOUDINI_LOG_MESSAGE(
						TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] has 0 vertices and non-zero points, ")
						TEXT("but is not an intstancer - skipping."),
						ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName);
					continue;
				}

				// Retrieve all vertex indices.
				VertexList.SetNumUninitialized(PartInfo.vertexCount);

				if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetVertexList(FHoudiniEngine::Get().GetSession(), AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
					&VertexList[0], 0, PartInfo.vertexCount))
				{
					// Error getting the vertex list.
					bGeoError = true;

					HOUDINI_LOG_MESSAGE(
						TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve vertex list ")
						TEXT("- skipping."),
						ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName);

					continue;
				}

				// See if we require splitting.
				TMap<FString, TArray<int32> > GroupSplitFaces;
				TMap<FString, int32> GroupSplitFaceCounts;
				TMap<FString, TArray<int32> > GroupSplitFaceIndices;

				int32 GroupVertexListCount = 0;
				static const FString RemainingGroupName = TEXT(HAPI_UNREAL_GROUP_GEOMETRY_NOT_COLLISION);

				if(bIsRenderCollidable || bIsCollidable)
				{
					// Buffer for all vertex indices used for collision. We need this to figure out all vertex
					// indices that are not part of collision geos.
					TArray<int32> AllCollisionVertexList;
					AllCollisionVertexList.SetNumZeroed(VertexList.Num());

					// Buffer for all face indices used for collision. We need this to figure out all face indices
					// that are not part of collision geos.
					TArray<int32> AllCollisionFaceIndices;
					AllCollisionFaceIndices.SetNumZeroed(FaceMaterialIds.Num());

					for(int32 GeoGroupNameIdx = 0; GeoGroupNameIdx < ObjectGeoGroupNames.Num(); ++GeoGroupNameIdx)
					{
						const FString& GroupName = ObjectGeoGroupNames[GeoGroupNameIdx];

						if((!HoudiniRuntimeSettings->RenderedCollisionGroupNamePrefix.IsEmpty() &&
								GroupName.StartsWith(HoudiniRuntimeSettings->RenderedCollisionGroupNamePrefix,
								ESearchCase::IgnoreCase)) ||
							(!HoudiniRuntimeSettings->CollisionGroupNamePrefix.IsEmpty() &&
								GroupName.StartsWith(HoudiniRuntimeSettings->CollisionGroupNamePrefix,
								ESearchCase::IgnoreCase)))
						{
							// New vertex list just for this group.
							TArray<int32> GroupVertexList;
							TArray<int32> AllFaceList;

							// Extract vertex indices for this split.
							GroupVertexListCount = FHoudiniEngineUtils::HapiGetVertexListForGroup(AssetId,
								ObjectInfo.id, GeoInfo.id, PartInfo.id, GroupName, VertexList, GroupVertexList,
								AllCollisionVertexList, AllFaceList, AllCollisionFaceIndices);

							if(GroupVertexListCount > 0)
							{
								// If list is not empty, we store it for this group - this will define new mesh.
								GroupSplitFaces.Add(GroupName, GroupVertexList);
								GroupSplitFaceCounts.Add(GroupName, GroupVertexListCount);
								GroupSplitFaceIndices.Add(GroupName, AllFaceList);
							}
						}
					}

					// We also need to figure out / construct vertex list for everything that's not collision geometry
					// or rendered collision geometry.
					TArray<int32> GroupSplitFacesRemaining;
					GroupSplitFacesRemaining.Init(-1, VertexList.Num());
					bool bMainSplitGroup = false;
					GroupVertexListCount = 0;

					TArray<int32> GroupSplitFaceIndicesRemaining;

					for(int32 CollisionVertexIdx = 0; CollisionVertexIdx < AllCollisionVertexList.Num();
						++CollisionVertexIdx)
					{
						int32 VertexIndex = AllCollisionVertexList[CollisionVertexIdx];
						if(0 == VertexIndex)
						{
							// This is unused index, we need to add it to unused vertex list.
							//GroupSplitFacesRemaining.Add(VertexList[CollisionVertexIdx]);
							GroupSplitFacesRemaining[CollisionVertexIdx] = VertexList[CollisionVertexIdx];
							bMainSplitGroup = true;
							GroupVertexListCount++;
						}
					}

					for(int32 CollisionFaceIdx = 0; CollisionFaceIdx < AllCollisionFaceIndices.Num(); ++CollisionFaceIdx)
					{
						int32 FaceIndex = AllCollisionFaceIndices[CollisionFaceIdx];
						if(0 == FaceIndex)
						{
							// This is unused face, we need to add it to unused faces list.
							GroupSplitFaceIndicesRemaining.Add(FaceIndex);
						}
					}

					// We store remaining geo vertex list as a special name.
					if(bMainSplitGroup)
					{
						GroupSplitFaces.Add(RemainingGroupName, GroupSplitFacesRemaining);
						GroupSplitFaceCounts.Add(RemainingGroupName, GroupVertexListCount);
						GroupSplitFaceIndices.Add(RemainingGroupName, GroupSplitFaceIndicesRemaining);
					}
				}
				else
				{
					GroupSplitFaces.Add(RemainingGroupName, VertexList);
					GroupSplitFaceCounts.Add(RemainingGroupName, VertexList.Num());

					TArray<int32> AllFaces;
					for(int32 FaceIdx = 0; FaceIdx < PartInfo.faceCount; ++FaceIdx)
					{
						AllFaces.Add(FaceIdx);
					}

					GroupSplitFaceIndices.Add(RemainingGroupName, AllFaces);
				}

				// Keep track of split id.
				int32 SplitId = 0;

				// Iterate through all detected split groups we care about and split geometry.
				for(TMap<FString, TArray<int32> >::TIterator IterGroups(GroupSplitFaces); IterGroups; ++IterGroups)
				{
					// Get split group name and vertex indices.
					const FString& SplitGroupName = IterGroups.Key();
					TArray<int32>& SplitGroupVertexList = IterGroups.Value();

					// Get valid count of vertex indices for this split.
					int32 SplitGroupVertexListCount = GroupSplitFaceCounts[SplitGroupName];

					// Get face indices for this split.
					TArray<int32>& SplitGroupFaceIndices = GroupSplitFaceIndices[SplitGroupName];

					// Record split id in geo part.
					HoudiniGeoPartObject.SplitId = SplitId;

					// Reset collision flags.
					HoudiniGeoPartObject.bIsRenderCollidable = false;
					HoudiniGeoPartObject.bIsCollidable = false;

					// Increment split id.
					SplitId++;

					if(!HoudiniRuntimeSettings->RenderedCollisionGroupNamePrefix.IsEmpty() &&
						SplitGroupName.StartsWith(HoudiniRuntimeSettings->RenderedCollisionGroupNamePrefix,
							ESearchCase::IgnoreCase))
					{
						HoudiniGeoPartObject.bIsRenderCollidable = true;
					}
					else if(!HoudiniRuntimeSettings->CollisionGroupNamePrefix.IsEmpty() &&
							SplitGroupName.StartsWith(HoudiniRuntimeSettings->CollisionGroupNamePrefix,
								ESearchCase::IgnoreCase))
					{
						HoudiniGeoPartObject.bIsCollidable = true;
					}

					// Record split group name.
					HoudiniGeoPartObject.SplitName = SplitGroupName;

					// Attempt to locate static mesh from previous instantiation.
					UStaticMesh* const* FoundStaticMesh = StaticMeshesIn.Find(HoudiniGeoPartObject);

					// See if materials have changed for this geo part object.
					HoudiniAssetComponent->CheckMaterialInformationChanged(HoudiniGeoPartObject);

					// Flag whether we need to rebuild the mesh.
					bool bRebuildStaticMesh = false;

					// See if geometry and scaling factor has not changed. Then we can reuse the corresponding
					// static mesh.
					if(!GeoInfo.hasGeoChanged && HoudiniAssetComponent->CheckGlobalSettingScaleFactors())
					{
						// If geometry has not changed.
						if(FoundStaticMesh)
						{
							StaticMesh = *FoundStaticMesh;

							// If any of the materials on corresponding geo part object have not changed.
							if(!bMaterialsChanged)
							{
								// We can reuse previously created geometry.
								StaticMeshesOut.Add(HoudiniGeoPartObject, StaticMesh);
								continue;
							}
						}
						else
						{
							// No mesh located, this is an error.
							bGeoError = true;
							HOUDINI_LOG_MESSAGE(
								TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] geometry has changed ")
								TEXT("but static mesh does not exist - skipping."),
								ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName);
							continue;
						}
					}
					else
					{
						bRebuildStaticMesh = true;
					}

					// If static mesh was not located, we need to create one.
					bool bStaticMeshCreated = false;
					if(!FoundStaticMesh || *FoundStaticMesh == nullptr)
					{
						MeshGuid.Invalidate();

						UPackage* MeshPackage =
							FHoudiniEngineUtils::BakeCreateStaticMeshPackageForComponent(HoudiniAssetComponent,
								HoudiniGeoPartObject, MeshName, MeshGuid);

						StaticMesh = NewObject<UStaticMesh>(MeshPackage, FName(*MeshName),
							RF_Standalone | RF_Transactional);

						// Add meta information to this package.
						FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(MeshPackage, MeshPackage,
							HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
						FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(MeshPackage, MeshPackage,
							HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MeshName);

						// Notify system that new asset has been created.
						FAssetRegistryModule::AssetCreated(StaticMesh);

						bStaticMeshCreated = true;
					}
					else
					{
						// If it was located, we will just reuse it.
						StaticMesh = *FoundStaticMesh;
					}

					// Create new source model for current static mesh.
					if(!StaticMesh->SourceModels.Num())
					{
						new(StaticMesh->SourceModels) FStaticMeshSourceModel();
					}

					// Grab current source model.
					FStaticMeshSourceModel* SrcModel = &StaticMesh->SourceModels[0];

					// Load existing raw model. This will be empty as we are constructing a new mesh.
					FRawMesh RawMesh;

					// Compute number of faces.
					int32 FaceCount = SplitGroupFaceIndices.Num();

					// Reset Face materials.
					FaceMaterials.Empty();

					// Attributes we are interested in.
					HAPI_AttributeInfo AttribInfoPositions;
					FMemory::Memset<HAPI_AttributeInfo>(AttribInfoPositions, 0);
					HAPI_AttributeInfo AttribLightmapResolution;
					FMemory::Memset<HAPI_AttributeInfo>(AttribLightmapResolution, 0);
					HAPI_AttributeInfo AttribFaceMaterials;
					FMemory::Memset<HAPI_AttributeInfo>(AttribFaceMaterials, 0);
					HAPI_AttributeInfo AttribInfoColors;
					FMemory::Memset<HAPI_AttributeInfo>(AttribInfoColors, 0);
					HAPI_AttributeInfo AttribInfoNormals;
					FMemory::Memset<HAPI_AttributeInfo>(AttribInfoNormals, 0);
					HAPI_AttributeInfo AttribInfoFaceSmoothingMasks;
					FMemory::Memset<HAPI_AttributeInfo>(AttribInfoFaceSmoothingMasks, 0);
					HAPI_AttributeInfo AttribInfoUVs[MAX_STATIC_TEXCOORDS];
					FMemory::Memset(&AttribInfoUVs[0], 0, sizeof(HAPI_AttributeInfo) * MAX_STATIC_TEXCOORDS);

					if(bRebuildStaticMesh)
					{
						// Retrieve position data.
						if(!FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
							HAPI_UNREAL_ATTRIB_POSITION, AttribInfoPositions, Positions))
						{
							// Error retrieving positions.
							bGeoError = true;

							HOUDINI_LOG_MESSAGE(
								TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve position data ")
								TEXT("- skipping."),
								ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName);

							if(bStaticMeshCreated)
							{
								StaticMesh->MarkPendingKill();
							}

							break;
						}

						// Get name of attribute used for marshalling lightmap resolution.
						{
							std::string MarshallingAttributeName = HAPI_UNREAL_ATTRIB_LIGHTMAP_RESOLUTION;
							if(HoudiniRuntimeSettings && !HoudiniRuntimeSettings->MarshallingAttributeLightmapResolution.IsEmpty())
							{
								FHoudiniEngineUtils::ConvertUnrealString(HoudiniRuntimeSettings->MarshallingAttributeLightmapResolution,
									MarshallingAttributeName);
							}

							FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
								MarshallingAttributeName.c_str(), AttribLightmapResolution, LightMapResolutions);
						}

						// Get name of attribute used for marshalling materials.
						{
							std::string MarshallingAttributeName = HAPI_UNREAL_ATTRIB_MATERIAL;
							if(HoudiniRuntimeSettings && !HoudiniRuntimeSettings->MarshallingAttributeMaterial.IsEmpty())
							{
								FHoudiniEngineUtils::ConvertUnrealString(HoudiniRuntimeSettings->MarshallingAttributeMaterial,
									MarshallingAttributeName);
							}

							FHoudiniEngineUtils::HapiGetAttributeDataAsString(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
								MarshallingAttributeName.c_str(), AttribFaceMaterials, FaceMaterials);
						}

						// Retrieve color data.
						FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
							HAPI_UNREAL_ATTRIB_COLOR, AttribInfoColors, Colors);

						// See if we need to transfer color point attributes to vertex attributes.
						FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(SplitGroupVertexList, AttribInfoColors, Colors);

						// Retrieve normal data.
						FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
							HAPI_UNREAL_ATTRIB_NORMAL, AttribInfoNormals, Normals);

						// Retrieve face smoothing data.
						{
							std::string MarshallingAttributeName = HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK;
							if(HoudiniRuntimeSettings && !HoudiniRuntimeSettings->MarshallingAttributeMaterial.IsEmpty())
							{
								FHoudiniEngineUtils::ConvertUnrealString(
									HoudiniRuntimeSettings->MarshallingAttributeFaceSmoothingMask,
									MarshallingAttributeName);
							}

							FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
								AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id, MarshallingAttributeName.c_str(),
								AttribInfoFaceSmoothingMasks, FaceSmoothingMasks);
						}

						// See if we need to transfer normal point attributes to vertex attributes.
						FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(
							SplitGroupVertexList, AttribInfoNormals, Normals);

						// Retrieve UVs.
						for(int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx)
						{
							std::string UVAttributeName = HAPI_UNREAL_ATTRIB_UV;

							if(TexCoordIdx > 0)
							{
								UVAttributeName += std::to_string(TexCoordIdx + 1);
							}

							const char* UVAttributeNameString = UVAttributeName.c_str();
							FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
								AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id, UVAttributeNameString,
								AttribInfoUVs[TexCoordIdx], TextureCoordinates[TexCoordIdx], 2);

							// See if we need to transfer uv point attributes to vertex attributes.
							FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(
								SplitGroupVertexList, AttribInfoUVs[TexCoordIdx], TextureCoordinates[TexCoordIdx]);
						}

						// We can transfer attributes to raw mesh.

						// Set face smoothing masks.
						{
							RawMesh.FaceSmoothingMasks.SetNumZeroed(FaceCount);

							if(FaceSmoothingMasks.Num())
							{
								int32 ValidFaceIdx = 0;
								for(int32 VertexIdx = 0; VertexIdx < SplitGroupVertexList.Num(); VertexIdx += 3)
								{
									int32 WedgeCheck = SplitGroupVertexList[VertexIdx + 0];
									if(-1 == WedgeCheck)
									{
										continue;
									}

									RawMesh.FaceSmoothingMasks[ValidFaceIdx] = FaceSmoothingMasks[VertexIdx / 3];
									ValidFaceIdx++;
								}
							}
						}

						// Transfer UVs.
						int32 UVChannelCount = 0;
						int32 FirstUVChannelIndex = -1;
						for(int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx)
						{
							TArray<float>& TextureCoordinate = TextureCoordinates[TexCoordIdx];
							if(TextureCoordinate.Num() > 0)
							{
								int32 WedgeUVCount = TextureCoordinate.Num() / 2;
								RawMesh.WedgeTexCoords[TexCoordIdx].SetNumZeroed(WedgeUVCount);
								for(int32 WedgeUVIdx = 0; WedgeUVIdx < WedgeUVCount; ++WedgeUVIdx)
								{
									// We need to flip V coordinate when it's coming from HAPI.
									FVector2D WedgeUV;
									WedgeUV.X = TextureCoordinate[WedgeUVIdx * 2 + 0];
									WedgeUV.Y = 1.0f - TextureCoordinate[WedgeUVIdx * 2 + 1];

									RawMesh.WedgeTexCoords[TexCoordIdx][WedgeUVIdx] = WedgeUV;
								}

								UVChannelCount++;
								if(-1 == FirstUVChannelIndex)
								{
									FirstUVChannelIndex = TexCoordIdx;
								}
							}
							else
							{
								RawMesh.WedgeTexCoords[TexCoordIdx].Empty();
							}
						}

						switch(UVChannelCount)
						{
							case 0:
							{
								// We have to have at least one UV channel. If there's none, create one with zero data.
								RawMesh.WedgeTexCoords[0].SetNumZeroed(SplitGroupVertexListCount);
								StaticMesh->LightMapCoordinateIndex = 0;

								break;
							}

							case 1:
							{
								// We have only one UV channel.
								StaticMesh->LightMapCoordinateIndex = FirstUVChannelIndex;

								break;
							}

							default:
							{
								// We have more than one channel, by convention use 2nd set for lightmaps.
								StaticMesh->LightMapCoordinateIndex = 1;

								break;
							}
						}

						// Transfer colors.
						if(AttribInfoColors.exists && AttribInfoColors.tupleSize)
						{
							int32 WedgeColorsCount = Colors.Num() / AttribInfoColors.tupleSize;
							RawMesh.WedgeColors.SetNumZeroed(WedgeColorsCount);
							for(int32 WedgeColorIdx = 0; WedgeColorIdx < WedgeColorsCount; ++WedgeColorIdx)
							{
								FLinearColor WedgeColor;

								WedgeColor.R = FMath::Clamp(Colors[WedgeColorIdx * AttribInfoColors.tupleSize + 0], 0.0f, 1.0f);
								WedgeColor.G = FMath::Clamp(Colors[WedgeColorIdx * AttribInfoColors.tupleSize + 1], 0.0f, 1.0f);
								WedgeColor.B = FMath::Clamp(Colors[WedgeColorIdx * AttribInfoColors.tupleSize + 2], 0.0f, 1.0f);

								if(4 == AttribInfoColors.tupleSize)
								{
									// We have alpha.
									WedgeColor.A = FMath::Clamp(Colors[WedgeColorIdx * AttribInfoColors.tupleSize + 3],
										0.0f, 1.0f);
								}
								else
								{
									WedgeColor.A = 1.0f;
								}

								// Convert linear color to fixed color.
								RawMesh.WedgeColors[WedgeColorIdx] = WedgeColor.ToFColor(false);
							}
						}

						// See if we need to generate tangents, we do this only if normals are present.
						bool bGenerateTangents = (Normals.Num() > 0);

						// Transfer normals.
						int32 WedgeNormalCount = Normals.Num() / 3;
						RawMesh.WedgeTangentZ.SetNumZeroed(WedgeNormalCount);
						for(int32 WedgeTangentZIdx = 0; WedgeTangentZIdx < WedgeNormalCount; ++WedgeTangentZIdx)
						{
							FVector WedgeTangentZ;

							WedgeTangentZ.X = Normals[WedgeTangentZIdx * 3 + 0];
							WedgeTangentZ.Y = Normals[WedgeTangentZIdx * 3 + 1];
							WedgeTangentZ.Z = Normals[WedgeTangentZIdx * 3 + 2];

							if(HRSAI_Unreal == ImportAxis)
							{
								// We need to flip Z and Y coordinate here.
								Swap(WedgeTangentZ.Y, WedgeTangentZ.Z);
								RawMesh.WedgeTangentZ[WedgeTangentZIdx] = WedgeTangentZ;
							}
							else if(HRSAI_Houdini == ImportAxis)
							{
								// Do nothing in this case.
							}
							else
							{
								// Not valid enum value.
								check(0);
							}

							// If we need to generate tangents.
							if(bGenerateTangents)
							{
								FVector TangentX, TangentY;
								WedgeTangentZ.FindBestAxisVectors(TangentX, TangentY);

								RawMesh.WedgeTangentX.Add(TangentX);
								RawMesh.WedgeTangentY.Add(TangentY);
							}
						}

						// Transfer indices.
						RawMesh.WedgeIndices.SetNumZeroed(SplitGroupVertexListCount);
						int32 ValidVertexId = 0;
						for(int32 VertexIdx = 0; VertexIdx < SplitGroupVertexList.Num(); VertexIdx += 3)
						{
							int32 WedgeCheck = SplitGroupVertexList[VertexIdx + 0];
							if(-1 == WedgeCheck)
							{
								continue;
							}

							int32 WedgeIndices[3] = {
								SplitGroupVertexList[VertexIdx + 0],
								SplitGroupVertexList[VertexIdx + 1],
								SplitGroupVertexList[VertexIdx + 2]
							};

							if(HRSAI_Unreal == ImportAxis)
							{
								// Flip wedge indices to fix winding order.
								RawMesh.WedgeIndices[ValidVertexId + 0] = WedgeIndices[0];
								RawMesh.WedgeIndices[ValidVertexId + 1] = WedgeIndices[2];
								RawMesh.WedgeIndices[ValidVertexId + 2] = WedgeIndices[1];

								// Check if we need to patch UVs.
								for(int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx)
								{
									if(RawMesh.WedgeTexCoords[TexCoordIdx].Num() > 0)
									{
										Swap(RawMesh.WedgeTexCoords[TexCoordIdx][ValidVertexId + 1],
											RawMesh.WedgeTexCoords[TexCoordIdx][ValidVertexId + 2]);
									}
								}

								// Check if we need to patch colors.
								if(RawMesh.WedgeColors.Num() > 0)
								{
									Swap(RawMesh.WedgeColors[ValidVertexId + 1], RawMesh.WedgeColors[ValidVertexId + 2]);
								}

								// Check if we need to patch tangents.
								if(RawMesh.WedgeTangentZ.Num() > 0)
								{
									Swap(RawMesh.WedgeTangentZ[ValidVertexId + 1], RawMesh.WedgeTangentZ[ValidVertexId + 2]);
								}
								/*
								if(RawMesh.WedgeTangentX.Num() > 0)
								{
								Swap(RawMesh.WedgeTangentX[ValidVertexId + 1], RawMesh.WedgeTangentX[ValidVertexId + 2]);
								}

								if(RawMesh.WedgeTangentY.Num() > 0)
								{
								Swap(RawMesh.WedgeTangentY[ValidVertexId + 1], RawMesh.WedgeTangentY[ValidVertexId + 2]);
								}
								*/
							}
							else if(HRSAI_Houdini == ImportAxis)
							{
								// Flip wedge indices to fix winding order.
								RawMesh.WedgeIndices[ValidVertexId + 0] = WedgeIndices[0];
								RawMesh.WedgeIndices[ValidVertexId + 1] = WedgeIndices[1];
								RawMesh.WedgeIndices[ValidVertexId + 2] = WedgeIndices[2];
							}
							else
							{
								// Not valid enum value.
								check(0);
							}

							ValidVertexId += 3;
						}

						// Transfer vertex positions.
						int32 VertexPositionsCount = Positions.Num() / 3;
						RawMesh.VertexPositions.SetNumZeroed(VertexPositionsCount);
						for(int32 VertexPositionIdx = 0; VertexPositionIdx < VertexPositionsCount; ++VertexPositionIdx)
						{
							FVector VertexPosition;
							VertexPosition.X = Positions[VertexPositionIdx * 3 + 0] * GeneratedGeometryScaleFactor;
							VertexPosition.Y = Positions[VertexPositionIdx * 3 + 1] * GeneratedGeometryScaleFactor;
							VertexPosition.Z = Positions[VertexPositionIdx * 3 + 2] * GeneratedGeometryScaleFactor;

							if(HRSAI_Unreal == ImportAxis)
							{
								// We need to flip Z and Y coordinate here.
								Swap(VertexPosition.Y, VertexPosition.Z);
							}
							else if(HRSAI_Houdini == ImportAxis)
							{
								// No action required.
							}
							else
							{
								// Not valid enum value.
								check(0);
							}

							RawMesh.VertexPositions[VertexPositionIdx] = VertexPosition;
						}

						// We need to check if this mesh contains only degenerate triangles.
						if(FHoudiniEngineUtils::CountDegenerateTriangles(RawMesh) == FaceCount)
						{
							// This mesh contains only degenerate triangles, there's nothing we can do.
							if(bStaticMeshCreated)
							{
								StaticMesh->MarkPendingKill();
							}

							continue;
						}
					}
					else
					{
						// Otherwise we'll just load old data into Raw mesh and reuse it.
						FRawMeshBulkData* InRawMeshBulkData = SrcModel->RawMeshBulkData;
						InRawMeshBulkData->LoadRawMesh(RawMesh);
					}

					// Process material replacements first.
					bool bMissingReplacement = false;
					bool bMaterialsReplaced = false;

					if(FaceMaterials.Num() > 0)
					{
						// If material name was assigned per detail we replicate it for each primitive.
						if(HAPI_ATTROWNER_DETAIL == AttribFaceMaterials.owner)
						{
							FString SingleFaceMaterial = FaceMaterials[0];
							FaceMaterials.Init(SingleFaceMaterial, SplitGroupVertexList.Num() / 3);
						}

						StaticMesh->Materials.Empty();
						RawMesh.FaceMaterialIndices.SetNumZeroed(FaceCount);

						TMap<FString, int32> FaceMaterialMap;
						int32 UniqueMaterialIdx = 0;
						int32 FaceIdx = 0;

						for(int32 VertexIdx = 0; VertexIdx < SplitGroupVertexList.Num(); VertexIdx += 3)
						{
							int32 WedgeCheck = SplitGroupVertexList[VertexIdx + 0];
							if(-1 == WedgeCheck)
							{
								continue;
							}

							const FString& MaterialName = FaceMaterials[VertexIdx / 3];
							int32 const* FoundFaceMaterialIdx = FaceMaterialMap.Find(MaterialName);
							int32 CurrentFaceMaterialIdx = 0;
							if(FoundFaceMaterialIdx)
							{
								CurrentFaceMaterialIdx = *FoundFaceMaterialIdx;
							}
							else
							{
								// Attempt to load this material.
								UMaterialInterface* MaterialInterface =
									Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(),
										nullptr, *MaterialName, nullptr, LOAD_NoWarn, nullptr));

								if(!MaterialInterface)
								{
									// Material does not exist, use default material.
									MaterialInterface = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();
									bMissingReplacement = true;
								}

								StaticMesh->Materials.Add(MaterialInterface);
								FaceMaterialMap.Add(MaterialName, UniqueMaterialIdx);
								CurrentFaceMaterialIdx = UniqueMaterialIdx;
								UniqueMaterialIdx++;
							}

							RawMesh.FaceMaterialIndices[FaceIdx] = CurrentFaceMaterialIdx;
							FaceIdx++;
						}

						bMaterialsReplaced = true;
					}

					if(bMaterialsReplaced)
					{
						// We want to fallback to supplied material only if replacement occurred and there was an issue in the process.
						if(bMaterialsFound && bMissingReplacement)
						{
							// Get default Houdini material.
							UMaterial* MaterialDefault = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();

							// Extract native Houdini materials.
							TMap<HAPI_MaterialId, UMaterialInterface*> NativeMaterials;

							for(int32 FaceIdx = 0; FaceIdx < SplitGroupFaceIndices.Num(); ++FaceIdx)
							{
								// Get material id for this face.
								HAPI_MaterialId MaterialId = FaceMaterialIds[FaceIdx];
								UMaterialInterface* Material = MaterialDefault;

								FString MaterialShopName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
								FHoudiniEngineUtils::GetUniqueMaterialShopName(AssetId, MaterialId, MaterialShopName);
								UMaterial* const* FoundMaterial = Materials.Find(MaterialShopName);

								if(FoundMaterial)
								{
									UMaterial* AssignedMaterial = *FoundMaterial;

									// If looked up material has a sampling expression or mesh has no vertex colors, use it.
									if(FHoudiniEngineUtils::MaterialHasTextureSampleExpression(AssignedMaterial) ||
										RawMesh.WedgeColors.Num() == 0)
									{
										Material = AssignedMaterial;
									}
								}

								// If we have replacement material for this geo part object and this shop material name.
								UMaterialInterface* ReplacementMaterial =
									HoudiniAssetComponent->GetReplacementMaterial(HoudiniGeoPartObject, MaterialShopName);

								if(ReplacementMaterial)
								{
									Material = ReplacementMaterial;
								}

								// See if this material has been added.
								UMaterialInterface* const* FoundNativeMaterial = NativeMaterials.Find(MaterialId);
								if(!FoundNativeMaterial)
								{
									NativeMaterials.Add(MaterialId, Material);
								}
							}

							for(int32 FaceIdx = 0; FaceIdx < RawMesh.FaceMaterialIndices.Num(); ++FaceIdx)
							{
								int32 MaterialIdx = RawMesh.FaceMaterialIndices[FaceIdx];
								UMaterialInterface* FaceMaterial = StaticMesh->Materials[MaterialIdx];

								if(FaceMaterial == MaterialDefault)
								{
									HAPI_MaterialId MaterialId = FaceMaterialIds[FaceIdx];
									if(MaterialId >= 0)
									{
										UMaterialInterface* const* FoundNativeMaterial = NativeMaterials.Find(MaterialId);
										if(FoundNativeMaterial)
										{
											StaticMesh->Materials[MaterialIdx] = *FoundNativeMaterial;
										}
									}
								}
							}
						}
					}
					else
					{
						if(bMaterialsFound)
						{
							if(bSingleFaceMaterial)
							{
								// Use default Houdini material if no valid material is assigned to any of the faces.
								UMaterialInterface* Material = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();

								// We have only one material.
								RawMesh.FaceMaterialIndices.SetNumZeroed(FaceCount);

								// Get id of this single material.
								FString MaterialShopName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
								FHoudiniEngineUtils::GetUniqueMaterialShopName(AssetId, FaceMaterialIds[0], MaterialShopName);
								UMaterial* const* FoundMaterial = Materials.Find(MaterialShopName);

								if(FoundMaterial)
								{
									UMaterial* AssignedMaterial = *FoundMaterial;

									// If looked up material has a sampling expression or mesh has no vertex colors, use it.
									if(FHoudiniEngineUtils::MaterialHasTextureSampleExpression(AssignedMaterial) ||
										RawMesh.WedgeColors.Num() == 0)
									{
										Material = AssignedMaterial;
									}
								}

								// If we have replacement material for this geo part object and this shop material name.
								UMaterialInterface* ReplacementMaterial =
									HoudiniAssetComponent->GetReplacementMaterial(HoudiniGeoPartObject, MaterialShopName);

								if(ReplacementMaterial)
								{
									Material = ReplacementMaterial;
								}

								StaticMesh->Materials.Empty();
								StaticMesh->Materials.Add(Material);
							}
							else
							{
								// Get default Houdini material.
								UMaterial* MaterialDefault = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();

								// We have multiple materials.
								int32 MaterialIndex = 0;
								TMap<UMaterialInterface*, int32> MappedMaterials;
								TArray<UMaterialInterface*> MappedMaterialsList;

								// Reset Rawmesh material face assignments.
								RawMesh.FaceMaterialIndices.SetNumZeroed(SplitGroupFaceIndices.Num());

								for(int32 FaceIdx = 0; FaceIdx < SplitGroupFaceIndices.Num(); ++FaceIdx)
								{
									// Get material id for this face.
									HAPI_MaterialId MaterialId = FaceMaterialIds[FaceIdx];
									UMaterialInterface* Material = MaterialDefault;

									FString MaterialShopName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
									FHoudiniEngineUtils::GetUniqueMaterialShopName(AssetId, MaterialId, MaterialShopName);
									UMaterial* const* FoundMaterial = Materials.Find(MaterialShopName);

									if(FoundMaterial)
									{
										UMaterial* AssignedMaterial = *FoundMaterial;

										// If looked up material has a sampling expression or mesh has no vertex colors, use it.
										if(FHoudiniEngineUtils::MaterialHasTextureSampleExpression(AssignedMaterial) ||
											RawMesh.WedgeColors.Num() == 0)
										{
											Material = AssignedMaterial;
										}
									}

									// If we have replacement material for this geo part object and this shop material name.
									UMaterialInterface* ReplacementMaterial =
										HoudiniAssetComponent->GetReplacementMaterial(HoudiniGeoPartObject, MaterialShopName);

									if(ReplacementMaterial)
									{
										Material = ReplacementMaterial;
									}

									// See if this material has been added.
									int32 const* FoundMaterialIdx = MappedMaterials.Find(Material);
									if(FoundMaterialIdx)
									{
										// This material has been mapped already.
										RawMesh.FaceMaterialIndices[FaceIdx] = *FoundMaterialIdx;
									}
									else
									{
										// This is the first time we've seen this material.
										MappedMaterials.Add(Material, MaterialIndex);
										MappedMaterialsList.Add(Material);

										RawMesh.FaceMaterialIndices[FaceIdx] = MaterialIndex;

										MaterialIndex++;
									}
								}

								StaticMesh->Materials.Empty();

								for(int32 MaterialIdx = 0; MaterialIdx < MappedMaterialsList.Num(); ++MaterialIdx)
								{
									StaticMesh->Materials.Add(MappedMaterialsList[MaterialIdx]);
								}
							}
						}
						else
						{
							// No materials were found, we need to use default Houdini material.
							RawMesh.FaceMaterialIndices.SetNumZeroed(FaceCount);

							UMaterialInterface* Material = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();
							FString MaterialShopName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;

							// If we have replacement material for this geo part object and this shop material name.
							UMaterialInterface* ReplacementMaterial =
								HoudiniAssetComponent->GetReplacementMaterial(HoudiniGeoPartObject, MaterialShopName);

							if(ReplacementMaterial)
							{
								Material = ReplacementMaterial;
							}

							StaticMesh->Materials.Empty();
							StaticMesh->Materials.Add(Material);
						}
					}

					// Some mesh generation settings.
					HoudiniRuntimeSettings->SetMeshBuildSettings(SrcModel->BuildSettings, RawMesh);

					// We need to check light map uv set for correctness. Unreal seems to have occasional issues with
					// zero UV sets when building lightmaps.
					if(SrcModel->BuildSettings.bGenerateLightmapUVs)
					{
						// See if we need to disable lightmap generation because of bad UVs.
						if(FHoudiniEngineUtils::ContainsInvalidLightmapFaces(RawMesh, StaticMesh->LightMapCoordinateIndex))
						{
							SrcModel->BuildSettings.bGenerateLightmapUVs = false;

							HOUDINI_LOG_MESSAGE(
								TEXT("Skipping Lightmap Generation: Object [%d %s], Geo [%d], Part [%d %s] invalid face detected ")
								TEXT("- skipping."),
								ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName);
						}
					}

					// Store the new raw mesh.
					SrcModel->RawMeshBulkData->SaveRawMesh(RawMesh);

					while(StaticMesh->SourceModels.Num() < NumLODs)
					{
						new(StaticMesh->SourceModels) FStaticMeshSourceModel();
					}

					for(int32 ModelLODIndex = 0; ModelLODIndex < NumLODs; ++ModelLODIndex)
					{
						StaticMesh->SourceModels[ModelLODIndex].ReductionSettings =
							LODGroup.GetDefaultSettings(ModelLODIndex);

						for(int32 MaterialIndex = 0; MaterialIndex < StaticMesh->Materials.Num(); ++MaterialIndex)
						{
							FMeshSectionInfo Info = StaticMesh->SectionInfoMap.Get(ModelLODIndex, MaterialIndex);
							Info.MaterialIndex = MaterialIndex;
							Info.bEnableCollision = true;
							Info.bCastShadow = true;
							StaticMesh->SectionInfoMap.Set(ModelLODIndex, MaterialIndex, Info);
						}
					}

					// Assign generation parameters for this static mesh.
					HoudiniAssetComponent->SetStaticMeshGenerationParameters(StaticMesh);

					// If we have an override for lightmap resolution.
					if(LightMapResolutions.Num() > 0)
					{
						int32 LightMapResolutionOverride = LightMapResolutions[0];
						if(LightMapResolutionOverride > 0)
						{
							StaticMesh->LightMapResolution = LightMapResolutionOverride;
						}
					}

					// See if we need to enable collisions.
					if(HoudiniGeoPartObject.IsCollidable() || HoudiniGeoPartObject.IsRenderCollidable())
					{
						UBodySetup* BodySetup = StaticMesh->BodySetup;
						check(BodySetup);

						// Enable collisions for this static mesh.
						BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
					}

					// Free any RHI resources.
					StaticMesh->PreEditChange(nullptr);

					FHoudiniScopedGlobalSilence ScopedGlobalSilence;

					TArray<FText> BuildErrors;
					StaticMesh->Build(true, &BuildErrors);

					for(int32 BuildErrorIdx = 0; BuildErrorIdx < BuildErrors.Num(); ++BuildErrorIdx)
					{
						const FText& TextError = BuildErrors[BuildErrorIdx];
						HOUDINI_LOG_MESSAGE(
							TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s], Split [%d] build error ")
							TEXT("- %s."),
							ObjectIdx, *ObjectName, GeoIdx, PartIdx, *PartName, SplitId, *(TextError.ToString()));
					}

					StaticMesh->MarkPackageDirty();
					//StaticMesh->PostEditChange();

					StaticMeshesOut.Add(HoudiniGeoPartObject, StaticMesh);
				}
			}
		}
	}

#endif

	return true;
}


#if WITH_EDITOR

bool
FHoudiniEngineUtils::ContainsDegenerateTriangles(const FRawMesh& RawMesh)
{
	int32 WedgeCount = RawMesh.WedgeIndices.Num();
	for(int32 WedgeIdx = 0; WedgeIdx < WedgeCount; WedgeIdx += 3)
	{
		const FVector& Vertex0 = RawMesh.VertexPositions[RawMesh.WedgeIndices[WedgeIdx + 0]];
		const FVector& Vertex1 = RawMesh.VertexPositions[RawMesh.WedgeIndices[WedgeIdx + 1]];
		const FVector& Vertex2 = RawMesh.VertexPositions[RawMesh.WedgeIndices[WedgeIdx + 2]];

		if(Vertex0 == Vertex1 || Vertex0 == Vertex2 || Vertex1 == Vertex2)
		{
			return true;
		}
	}

	return false;
}


int32
FHoudiniEngineUtils::CountDegenerateTriangles(const FRawMesh& RawMesh)
{
	int32 DegenerateTriangleCount = 0;
	int32 WedgeCount = RawMesh.WedgeIndices.Num();
	for(int32 WedgeIdx = 0; WedgeIdx < WedgeCount; WedgeIdx += 3)
	{
		const FVector& Vertex0 = RawMesh.VertexPositions[RawMesh.WedgeIndices[WedgeIdx + 0]];
		const FVector& Vertex1 = RawMesh.VertexPositions[RawMesh.WedgeIndices[WedgeIdx + 1]];
		const FVector& Vertex2 = RawMesh.VertexPositions[RawMesh.WedgeIndices[WedgeIdx + 2]];

		if(Vertex0 == Vertex1 || Vertex0 == Vertex2 || Vertex1 == Vertex2)
		{
			DegenerateTriangleCount++;
		}
	}

	return DegenerateTriangleCount;
}

#endif


int32
FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(
	const TArray<int32>& VertexList, const HAPI_AttributeInfo& AttribInfo, TArray<float>& Data)
{
	int32 ValidWedgeCount = 0;

	if(AttribInfo.exists && AttribInfo.tupleSize)
	{
		// Future optimization - see if we can do direct vertex transfer.

		int32 WedgeCount = VertexList.Num();
		TArray<float> VertexData;
		VertexData.SetNumZeroed(WedgeCount * AttribInfo.tupleSize);

		int32 LastValidWedgeIdx = 0;
		for(int32 WedgeIdx = 0; WedgeIdx < WedgeCount; ++WedgeIdx)
		{
			int32 VertexId = VertexList[WedgeIdx];

			if(-1 == VertexId)
			{
				// This is an index/wedge we are skipping due to split.
				continue;
			}

			// Increment wedge count, since this is a valid wedge.
			ValidWedgeCount++;

			int32 PrimIdx = WedgeIdx / 3;
			int32 SaveIdx = 0;
			float Value = 0.0f;

			for(int32 AttributeIndexIdx = 0; AttributeIndexIdx < AttribInfo.tupleSize; ++AttributeIndexIdx)
			{
				switch(AttribInfo.owner)
				{
					case HAPI_ATTROWNER_POINT:
					{
						Value = Data[VertexId * AttribInfo.tupleSize + AttributeIndexIdx];
						break;
					}

					case HAPI_ATTROWNER_PRIM:
					{
						Value = Data[PrimIdx * AttribInfo.tupleSize + AttributeIndexIdx];
						break;
					}

					case HAPI_ATTROWNER_DETAIL:
					{
						Value = Data[AttributeIndexIdx];
						break;
					}

					case HAPI_ATTROWNER_VERTEX:
					{
						Value = Data[WedgeIdx * AttribInfo.tupleSize + AttributeIndexIdx];
						break;
					}

					default:
					{
						check(false);
						continue;
					}
				}

				SaveIdx = LastValidWedgeIdx * AttribInfo.tupleSize + AttributeIndexIdx;
				VertexData[SaveIdx] = Value;
			}

			// We are re-indexing wedges.
			LastValidWedgeIdx++;
		}

		VertexData.SetNumZeroed(ValidWedgeCount * AttribInfo.tupleSize);
		Data = VertexData;
	}

	return ValidWedgeCount;
}


UStaticMesh*
FHoudiniEngineUtils::BakeStaticMesh(UHoudiniAssetComponent* HoudiniAssetComponent,
	const FHoudiniGeoPartObject& HoudiniGeoPartObject, UStaticMesh* InStaticMesh)
{
	UStaticMesh* StaticMesh = nullptr;

#if WITH_EDITOR

	UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
	check(HoudiniAsset);

	// We cannot bake curves.
	if(HoudiniGeoPartObject.IsCurve())
	{
		return nullptr;
	}

	if(HoudiniGeoPartObject.IsInstancer())
	{
		HOUDINI_LOG_MESSAGE(TEXT("Baking of instanced static meshes is not supported at the moment."));
		return nullptr;
	}

	// Get platform manager LOD specific information.
	ITargetPlatform* CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	check(CurrentPlatform);
	const FStaticMeshLODGroup& LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(NAME_None);
	int32 NumLODs = LODGroup.GetDefaultNumLODs();

	// Get runtime settings.
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	check(HoudiniRuntimeSettings);

	FString MeshName;
	FGuid BakeGUID;
	UPackage* Package =
		BakeCreateStaticMeshPackageForComponent(HoudiniAssetComponent, HoudiniGeoPartObject, MeshName, BakeGUID, true);

	// Create static mesh.
	StaticMesh = NewObject<UStaticMesh>(Package, FName(*MeshName), RF_Standalone | RF_Public | RF_Transactional);

	// Add meta information to this package.
	FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(Package, StaticMesh,
		HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
	FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(Package, StaticMesh,
		HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MeshName);

	// Notify registry that we created a new asset.
	FAssetRegistryModule::AssetCreated(StaticMesh);

	// Copy materials.
	StaticMesh->Materials = InStaticMesh->Materials;

	// Create new source model for current static mesh.
	if(!StaticMesh->SourceModels.Num())
	{
		new(StaticMesh->SourceModels) FStaticMeshSourceModel();
	}

	FStaticMeshSourceModel* SrcModel = &StaticMesh->SourceModels[0];
	FRawMeshBulkData* RawMeshBulkData = SrcModel->RawMeshBulkData;

	// Load raw data bytes.
	FRawMesh RawMesh;
	FStaticMeshSourceModel* InSrcModel = &InStaticMesh->SourceModels[0];
	FRawMeshBulkData* InRawMeshBulkData = InSrcModel->RawMeshBulkData;
	InRawMeshBulkData->LoadRawMesh(RawMesh);

	// Some mesh generation settings.
	HoudiniRuntimeSettings->SetMeshBuildSettings(SrcModel->BuildSettings, RawMesh);

	// We need to check light map uv set for correctness. Unreal seems to have occasional issues with
	// zero UV sets when building lightmaps.
	if(SrcModel->BuildSettings.bGenerateLightmapUVs)
	{
		// See if we need to disable lightmap generation because of bad UVs.
		if(FHoudiniEngineUtils::ContainsInvalidLightmapFaces(RawMesh, StaticMesh->LightMapCoordinateIndex))
		{
			SrcModel->BuildSettings.bGenerateLightmapUVs = false;

			HOUDINI_LOG_MESSAGE(
				TEXT("Skipping Lightmap Generation: Object %s ")
				TEXT("- skipping."),
				*MeshName);
		}
	}

	// Store the new raw mesh.
	RawMeshBulkData->SaveRawMesh(RawMesh);

	while(StaticMesh->SourceModels.Num() < NumLODs)
	{
		new(StaticMesh->SourceModels) FStaticMeshSourceModel();
	}

	for(int32 ModelLODIndex = 0; ModelLODIndex < NumLODs; ++ModelLODIndex)
	{
		StaticMesh->SourceModels[ModelLODIndex].ReductionSettings = LODGroup.GetDefaultSettings(ModelLODIndex);

		for(int32 MaterialIndex = 0; MaterialIndex < StaticMesh->Materials.Num(); ++MaterialIndex)
		{
			FMeshSectionInfo Info = StaticMesh->SectionInfoMap.Get(ModelLODIndex, MaterialIndex);
			Info.MaterialIndex = MaterialIndex;
			Info.bEnableCollision = true;
			Info.bCastShadow = true;
			StaticMesh->SectionInfoMap.Set(ModelLODIndex, MaterialIndex, Info);
		}
	}

	// Assign generation parameters for this static mesh.
	HoudiniAssetComponent->SetStaticMeshGenerationParameters(StaticMesh);

	// Copy custom lightmap resolution if it is set.
	if(InStaticMesh->LightMapResolution != StaticMesh->LightMapResolution)
	{
		StaticMesh->LightMapResolution = InStaticMesh->LightMapResolution;
	}

	if(HoudiniGeoPartObject.IsCollidable() || HoudiniGeoPartObject.IsRenderCollidable())
	{
		UBodySetup* BodySetup = StaticMesh->BodySetup;
		check(BodySetup);

		// Enable collisions for this static mesh.
		BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
	}

	FHoudiniScopedGlobalSilence ScopedGlobalSilence;
	StaticMesh->Build(true);
	StaticMesh->MarkPackageDirty();

#endif

	return StaticMesh;
}


UBlueprint*
FHoudiniEngineUtils::BakeBlueprint(UHoudiniAssetComponent* HoudiniAssetComponent)
{
	UBlueprint* Blueprint = nullptr;

#if WITH_EDITOR

	// Create package for our Blueprint.
	FString BlueprintName = TEXT("");
	UPackage* Package =
		FHoudiniEngineUtils::BakeCreateBlueprintPackageForComponent(HoudiniAssetComponent, BlueprintName);

	if(Package)
	{
		AActor* Actor = HoudiniAssetComponent->CloneComponentsAndCreateActor();
		if(Actor)
		{
			Blueprint = FKismetEditorUtilities::CreateBlueprintFromActor(*BlueprintName, Package, Actor, false);

			// If actor is rooted, unroot it. We can also delete intermediate actor.
			Actor->RemoveFromRoot();
			Actor->ConditionalBeginDestroy();

			if(Blueprint)
			{
				FAssetRegistryModule::AssetCreated(Blueprint);
			}
		}
	}

#endif

	return Blueprint;
}


AActor*
FHoudiniEngineUtils::ReplaceHoudiniActorWithBlueprint(UHoudiniAssetComponent* HoudiniAssetComponent)
{
	AActor* Actor = nullptr;

#if WITH_EDITOR

	// Create package for our Blueprint.
	FString BlueprintName = TEXT("");
	UPackage* Package =
		FHoudiniEngineUtils::BakeCreateBlueprintPackageForComponent(HoudiniAssetComponent, BlueprintName);

	if(Package)
	{
		AActor* ClonedActor = HoudiniAssetComponent->CloneComponentsAndCreateActor();
		if(ClonedActor)
		{
			UBlueprint* Blueprint = 
				FKismetEditorUtilities::CreateBlueprint(ClonedActor->GetClass(), Package, *BlueprintName,
					EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), FName("CreateFromActor"));

			if(Blueprint)
			{
				Package->MarkPackageDirty();

				if(ClonedActor->GetInstanceComponents().Num() > 0)
				{
					FKismetEditorUtilities::AddComponentsToBlueprint(Blueprint, ClonedActor->GetInstanceComponents());
				}

				if(Blueprint->GeneratedClass)
				{
					AActor* CDO = CastChecked<AActor>(Blueprint->GeneratedClass->GetDefaultObject());

					const auto CopyOptions = 
						(EditorUtilities::ECopyOptions::Type)(EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties | 
							EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances);

					EditorUtilities::CopyActorProperties(ClonedActor, CDO, CopyOptions);

					USceneComponent* Scene = CDO->GetRootComponent();
					if(Scene)
					{
						Scene->RelativeLocation = FVector::ZeroVector;
						Scene->RelativeRotation = FRotator::ZeroRotator;

						// Clear out the attachment info after having copied the properties from the source actor
						Scene->AttachParent = nullptr;
						Scene->AttachChildren.Empty();

						// Ensure the light mass information is cleaned up
						Scene->InvalidateLightingCache();
					}
				}

				// Compile our blueprint and notify asset system about blueprint.
				FKismetEditorUtilities::CompileBlueprint(Blueprint);
				FAssetRegistryModule::AssetCreated(Blueprint);

				// Retrieve actor transform.
				FVector Location = ClonedActor->GetActorLocation();
				FRotator Rotator = ClonedActor->GetActorRotation();

				// Replace cloned actor with Blueprint instance.
				{
					TArray<AActor*> Actors;
					Actors.Add(ClonedActor);

					ClonedActor->RemoveFromRoot();
					Actor = FKismetEditorUtilities::CreateBlueprintInstanceFromSelection(Blueprint, Actors, Location, Rotator);
				}

				// We can initiate Houdini actor deletion.
				AHoudiniAssetActor* HoudiniAssetActor = HoudiniAssetComponent->GetHoudiniAssetActorOwner();

				// Remove Houdini actor from active selection in editor and delete it.
				if(GEditor)
				{
					GEditor->SelectActor(HoudiniAssetActor, false, false);
					GEditor->Layers->DisassociateActorFromLayers(HoudiniAssetActor);
				}

				UWorld* World = HoudiniAssetActor->GetWorld();
				World->EditorDestroyActor(HoudiniAssetActor, false);
			}
			else
			{
				ClonedActor->RemoveFromRoot();
				ClonedActor->ConditionalBeginDestroy();
			}
		}
	}

#endif

	return Actor;
}


void
FHoudiniEngineUtils::HapiCreateMaterials(UHoudiniAssetComponent* HoudiniAssetComponent, const HAPI_AssetInfo& AssetInfo,
	const TSet<HAPI_MaterialId>& UniqueMaterialIds, TMap<FString, UMaterial*>& Materials)
{
#if WITH_EDITOR

	// Empty returned materials.
	Materials.Empty();

	if(0 == UniqueMaterialIds.Num())
	{
		return;
	}

	HAPI_AssetId AssetId = HoudiniAssetComponent->GetAssetId();

	const TMap<FString, UMaterial*>& CachedMaterials =
		HoudiniAssetComponent->HoudiniAssetComponentMaterials->Assignments;

	UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;

	// Update context for generated materials (will trigger when object goes out of scope).
	FMaterialUpdateContext MaterialUpdateContext;

	// Default Houdini material.
	UMaterial* DefaultMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();
	Materials.Add(HAPI_UNREAL_DEFAULT_MATERIAL_NAME, DefaultMaterial);

	// Factory to create materials.
	UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
	MaterialFactory->AddToRoot();

	for(TSet<HAPI_MaterialId>::TConstIterator IterMaterialId(UniqueMaterialIds); IterMaterialId; ++IterMaterialId)
	{
		HAPI_MaterialId MaterialId = *IterMaterialId;
		HAPI_MaterialInfo MaterialInfo;

		if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetMaterialInfo(FHoudiniEngine::Get().GetSession(), AssetInfo.id, MaterialId, &MaterialInfo))
		{
			continue;
		}

		if(MaterialInfo.exists)
		{
			FString MaterialShopName;

			if(!FHoudiniEngineUtils::GetUniqueMaterialShopName(AssetId, MaterialId, MaterialShopName))
			{
				continue;
			}

			UMaterial* const* FoundMaterial = CachedMaterials.Find(MaterialShopName);

			UPackage* MaterialPackage = nullptr;
			UPackage* TextureDiffusePackage = nullptr;
			UPackage* TextureNormalPackage = nullptr;

			UMaterial* Material = nullptr;

			UTexture2D* TextureDiffuse = nullptr;
			UTexture2D* TextureNormal = nullptr;

			UMaterialExpressionTextureSample* ExpressionDiffuse = nullptr;
			UMaterialExpressionTextureSample* ExpressionNormal = nullptr;
			UMaterialExpressionConstant4Vector* ExpressionDiffuseColor = nullptr;

			bool bCreatedNewMaterial = false;
			bool bCreatedNewTextureDiffuse = false;
			bool bCreatedNewTextureNormal = false;

			if(FoundMaterial)
			{
				Material = *FoundMaterial;

				// If cached material exists and has not changed, we can reuse it.
				if(!MaterialInfo.hasChanged)
				{
					// We found cached material, we can reuse it.
					Materials.Add(MaterialShopName, Material);
					continue;
				}
				else
				{
					// We do not want to modify default Houdini material.
					if(Material != DefaultMaterial)
					{
						// Material was cached and exists, we need to retrieve necessary resources.

						MaterialPackage = Cast<UPackage>(Material->GetOuter());
						check(MaterialPackage);

						ExpressionDiffuseColor = Cast<UMaterialExpressionConstant4Vector>(Material->BaseColor.Expression);

						ExpressionDiffuse = Cast<UMaterialExpressionTextureSample>(Material->BaseColor.Expression);
						if(ExpressionDiffuse)
						{
							TextureDiffuse = Cast<UTexture2D>(ExpressionDiffuse->Texture);
							if(TextureDiffuse)
							{
								TextureDiffusePackage = Cast<UPackage>(TextureDiffuse->GetOuter());
							}
						}

						ExpressionNormal = Cast<UMaterialExpressionTextureSample>(Material->Normal.Expression);
						if(ExpressionNormal)
						{
							TextureNormal = Cast<UTexture2D>(ExpressionNormal->Texture);
							if(TextureNormal)
							{
								TextureNormalPackage = Cast<UPackage>(TextureNormal->GetOuter());
							}
						}
					}
				}
			}

			// Get node information.
			HAPI_NodeInfo NodeInfo;
			FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), MaterialInfo.nodeId, &NodeInfo);

			// Get node parameters.
			TArray<HAPI_ParmInfo> NodeParams;
			NodeParams.SetNumUninitialized(NodeInfo.parmCount);
			FHoudiniApi::GetParameters(FHoudiniEngine::Get().GetSession(), NodeInfo.id, &NodeParams[0], 0, NodeInfo.parmCount);

			// Get names of parameters.
			TArray<std::string> NodeParamNames;
			FHoudiniEngineUtils::HapiRetrieveParameterNames(NodeParams, NodeParamNames);

			// See if diffuse texture is available.
			int32 ParmNameBaseIdx =
				FHoudiniEngineUtils::HapiFindParameterByName(HAPI_UNREAL_PARAM_MAP_DIFFUSE_0, NodeParamNames);

			if(-1 == ParmNameBaseIdx)
			{
				ParmNameBaseIdx =
					FHoudiniEngineUtils::HapiFindParameterByName(HAPI_UNREAL_PARAM_MAP_DIFFUSE_1, NodeParamNames);
			}

			if(-1 == ParmNameBaseIdx)
			{
				ParmNameBaseIdx =
					FHoudiniEngineUtils::HapiFindParameterByName(HAPI_UNREAL_PARAM_MAP_DIFFUSE_2, NodeParamNames);
			}

			// See if diffuse color is available.
			int32 ParmNameBaseDiffuseColorIdx =
				FHoudiniEngineUtils::HapiFindParameterByName(HAPI_UNREAL_PARAM_COLOR_DIFFUSE, NodeParamNames);

			if(ParmNameBaseIdx >= 0 || ParmNameBaseDiffuseColorIdx >= 0)
			{
				TArray<char> ImageBuffer;

				bool bMaterialGenerated = false;

				// Retrieve color data.
				if(FHoudiniEngineUtils::HapiExtractImage(NodeParams[ParmNameBaseIdx].id, MaterialInfo, ImageBuffer,
					HAPI_UNREAL_MATERIAL_TEXTURE_MAIN))
				{
					FString MaterialName;

					// Create material package, if this is a new material.
					if(!MaterialPackage)
					{
						MaterialPackage =
							FHoudiniEngineUtils::BakeCreateMaterialPackageForComponent(HoudiniAssetComponent,
								MaterialInfo, MaterialName);
					}

					// Create material, if we need to create one.
					if(!Material)
					{
						Material =
							(UMaterial*) MaterialFactory->FactoryCreateNew(UMaterial::StaticClass(), MaterialPackage,
								*MaterialName, RF_Public | RF_Standalone | RF_Transactional, NULL, GWarn);

						bCreatedNewMaterial = true;

						// Add meta information to this package.
						FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(MaterialPackage, Material,
							HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
						FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(MaterialPackage, Material,
							HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MaterialName);
					}

					// Reset material's expresisons.
					Material->Expressions.Empty();

					HAPI_ImageInfo ImageInfo;
					FHoudiniApi::GetImageInfo(FHoudiniEngine::Get().GetSession(), MaterialInfo.assetId, MaterialInfo.id, &ImageInfo);

					if(ImageInfo.xRes > 0 && ImageInfo.yRes > 0)
					{
						// Create texture.
						FString TextureDiffuseName;

						// Create texture package, if this is a new diffuse texture.
						if(!TextureDiffusePackage)
						{
							TextureDiffusePackage =
								FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(HoudiniAssetComponent,
									MaterialInfo, HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_DIFFUSE,
									TextureDiffuseName);
						}

						// Create diffuse texture, if we need to create one.
						if(!TextureDiffuse)
						{
							bCreatedNewTextureDiffuse = true;
						}

						// Reuse existing diffuse texture, or create new one.
						TextureDiffuse =
							FHoudiniEngineUtils::CreateUnrealTexture(TextureDiffuse, ImageInfo,
								TextureDiffusePackage, TextureDiffuseName, PF_R8G8B8A8, ImageBuffer,
								HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_DIFFUSE);

						// Create sampling expression and add it to material, if we don't have one.
						if(!ExpressionDiffuse)
						{
							ExpressionDiffuse = NewObject<UMaterialExpressionTextureSample>(Material,
								UMaterialExpressionTextureSample::StaticClass(), NAME_None, RF_Transactional);
						}

						ExpressionDiffuse->Texture = TextureDiffuse;
						ExpressionDiffuse->SamplerType = SAMPLERTYPE_Color;

						Material->Expressions.Add(ExpressionDiffuse);
						Material->BaseColor.Expression = ExpressionDiffuse;

						// Check if material is transparent. If it is, we need to hook up alpha.
						if(FHoudiniEngineUtils::HapiIsMaterialTransparent(MaterialInfo))
						{
							// This material contains transparency.
							Material->BlendMode = BLEND_Masked;

							TArray<FExpressionOutput> Outputs = ExpressionDiffuse->GetOutputs();
							FExpressionOutput* Output = Outputs.GetData();

							Material->OpacityMask.Expression = ExpressionDiffuse;
							Material->OpacityMask.Mask = Output->Mask;
							Material->OpacityMask.MaskR = 0;
							Material->OpacityMask.MaskG = 0;
							Material->OpacityMask.MaskB = 0;
							Material->OpacityMask.MaskA = 1;
						}
						else
						{
							// Material is opaque.
							Material->BlendMode = BLEND_Opaque;
						}

						// See if material contains normal plane.
						if(FHoudiniEngineUtils::HapiExtractImage(NodeParams[ParmNameBaseIdx].id, MaterialInfo,
							ImageBuffer, HAPI_UNREAL_MATERIAL_TEXTURE_NORMAL))
						{
							// Create texture.
							FString TextureNormalName;

							// Create texture package, if this is a new normal texture.
							if(!TextureNormalPackage)
							{
								TextureNormalPackage =
									FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(HoudiniAssetComponent,
										MaterialInfo, HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL,
										TextureNormalName);
							}

							// Create normal texture, if we need to create one.
							if(!TextureNormal)
							{
								bCreatedNewTextureNormal = true;
							}

							// Reuse existing normal texture, or create new one.
							TextureNormal =
								FHoudiniEngineUtils::CreateUnrealTexture(TextureNormal, ImageInfo,
									TextureNormalPackage, TextureNormalName, PF_R8G8B8A8, ImageBuffer,
									HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL);

							// Create sampling expression and add it to material, if we don't have one.
							if(!ExpressionNormal)
							{
								ExpressionNormal = NewObject<UMaterialExpressionTextureSample>(Material,
									UMaterialExpressionTextureSample::StaticClass(), NAME_None, RF_Transactional);
							}

							ExpressionNormal->Texture = TextureNormal;
							ExpressionNormal->SamplerType = SAMPLERTYPE_Normal;

							Material->Expressions.Add(ExpressionNormal);
							Material->Normal.Expression = ExpressionNormal;
						}
						else
						{
							Material->Normal.Expression = nullptr;
						}

						// Set other material properties.
						Material->TwoSided = true;
						Material->SetShadingModel(MSM_DefaultLit);

						// Schedule this material for update.
						MaterialUpdateContext.AddMaterial(Material);

						// Cache material.
						Materials.Add(MaterialShopName, Material);

						// Propagate and trigger material updates.
						{
							if(bCreatedNewMaterial)
							{
								FAssetRegistryModule::AssetCreated(Material);
							}

							Material->PreEditChange(nullptr);
							Material->PostEditChange();
							Material->MarkPackageDirty();
						}

						// Propagate and trigger diffuse texture updates.
						{
							if(bCreatedNewTextureDiffuse)
							{
								FAssetRegistryModule::AssetCreated(TextureDiffuse);
							}

							TextureDiffuse->PreEditChange(nullptr);
							TextureDiffuse->PostEditChange();
							TextureDiffuse->MarkPackageDirty();

							// Propagate and trigger normal texture updates.
							if(TextureNormal)
							{
								if(bCreatedNewTextureNormal)
								{
									FAssetRegistryModule::AssetCreated(TextureNormal);
								}

								TextureNormal->PreEditChange(nullptr);
								TextureNormal->PostEditChange();
								TextureNormal->MarkPackageDirty();
							}
						}

						bMaterialGenerated = true;
					}
				}

				if(!bMaterialGenerated && ParmNameBaseDiffuseColorIdx >= 0)
				{
					FLinearColor Color = FLinearColor::White;
					HAPI_ParmInfo& ParmInfo = NodeParams[ParmNameBaseDiffuseColorIdx];

					if(HAPI_RESULT_SUCCESS !=
						FHoudiniApi::GetParmFloatValues(FHoudiniEngine::Get().GetSession(), NodeInfo.id, (float*) &Color.R,
							ParmInfo.floatValuesIndex, ParmInfo.size))
					{
						continue;
					}

					if(3 == ParmInfo.size)
					{
						Color.A = 1.0f;
					}

					FString MaterialName;

					// Create material package, if this is a new material.
					if(!MaterialPackage)
					{
						MaterialPackage =
							FHoudiniEngineUtils::BakeCreateMaterialPackageForComponent(HoudiniAssetComponent,
								MaterialInfo, MaterialName);
					}

					// Create material, if we need to create one.
					if(!Material)
					{
						Material =
							(UMaterial*) MaterialFactory->FactoryCreateNew(UMaterial::StaticClass(), MaterialPackage,
								*MaterialName, RF_Public | RF_Standalone | RF_Transactional, NULL, GWarn);

						bCreatedNewMaterial = true;

						// Add meta information to this package.
						FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(MaterialPackage, Material,
							HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
						FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(MaterialPackage, Material,
							HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MaterialName);
					}

					// Reset material's expressions.
					Material->Expressions.Empty();

					// Create color const expression and add it to material, if we don't have one.
					if(!ExpressionDiffuseColor)
					{
						ExpressionDiffuseColor = NewObject<UMaterialExpressionConstant4Vector>(Material,
							UMaterialExpressionConstant4Vector::StaticClass(), NAME_None, RF_Transactional);
					}

					ExpressionDiffuseColor->Constant = Color;

					Material->Expressions.Add(ExpressionDiffuseColor);
					Material->BaseColor.Expression = ExpressionDiffuseColor;

					// Set other material properties.
					Material->TwoSided = true;
					Material->SetShadingModel(MSM_DefaultLit);

					// Schedule this material for update.
					MaterialUpdateContext.AddMaterial(Material);

					// Cache material.
					Materials.Add(MaterialShopName, Material);

					// Propagate and trigger material updates.
					{
						if(bCreatedNewMaterial)
						{
							FAssetRegistryModule::AssetCreated(Material);
						}

						Material->PreEditChange(nullptr);
						Material->PostEditChange();
						Material->MarkPackageDirty();
					}

					bMaterialGenerated = true;
				}
			}
			else
			{
				// Material does not exist, we will use default Houdini material in this case.
			}
		}
		else
		{
			// Material does not exist, we will use default Houdini material in this case.
		}
	}

	MaterialFactory->RemoveFromRoot();

#endif
}


char*
FHoudiniEngineUtils::ExtractMaterialName(UMaterialInterface* MaterialInterface)
{
	UPackage* Package = Cast<UPackage>(MaterialInterface->GetOuter());

	FString FullMaterialName = MaterialInterface->GetPathName();
	std::string ConvertedString = TCHAR_TO_UTF8(*FullMaterialName);

	// Allocate space for unique string.
	int32 UniqueNameBytes = ConvertedString.size() + 1;
	char* UniqueName = static_cast<char*>(FMemory::Malloc(UniqueNameBytes));

	FMemory::Memzero(UniqueName, UniqueNameBytes);
	FMemory::Memcpy(UniqueName, ConvertedString.c_str(), ConvertedString.size());

	return UniqueName;
}


void
FHoudiniEngineUtils::CreateFaceMaterialArray(
	const TArray<UMaterialInterface*>& Materials, const TArray<int32>& FaceMaterialIndices,
	TArray<char*>& OutStaticMeshFaceMaterials)
{
	// We need to create list of unique materials.
	TArray<char*> UniqueMaterialList;
	UMaterialInterface* MaterialInterface;
	char* UniqueName = nullptr;

	if(Materials.Num())
	{
		// We have materials.
		for(int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); ++MaterialIdx)
		{
			UniqueName = nullptr;
			MaterialInterface = Materials[MaterialIdx];

			if(!MaterialInterface)
			{
				// Null material interface found, add default instead.
				MaterialInterface = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();
			}

			UniqueName = FHoudiniEngineUtils::ExtractMaterialName(MaterialInterface);
			UniqueMaterialList.Add(UniqueName);
		}
	}
	else
	{
		// We do not have any materials, add default.
		MaterialInterface = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();
		UniqueName = FHoudiniEngineUtils::ExtractMaterialName(MaterialInterface);
		UniqueMaterialList.Add(UniqueName);
	}

	for(int32 FaceIdx = 0; FaceIdx < FaceMaterialIndices.Num(); ++FaceIdx)
	{
		int32 FaceMaterialIdx = FaceMaterialIndices[FaceIdx];
		check(FaceMaterialIdx < UniqueMaterialList.Num());

		OutStaticMeshFaceMaterials.Add(UniqueMaterialList[FaceMaterialIdx]);
	}
}


void
FHoudiniEngineUtils::DeleteFaceMaterialArray(TArray<char*>& OutStaticMeshFaceMaterials)
{
	TSet<char*> UniqueMaterials(OutStaticMeshFaceMaterials);
	for(TSet<char*>::TIterator Iter = UniqueMaterials.CreateIterator(); Iter; ++Iter)
	{
		char* MaterialName = *Iter;
		FMemory::Free(MaterialName);
	}

	OutStaticMeshFaceMaterials.Empty();
}


void
FHoudiniEngineUtils::ExtractStringPositions(const FString& Positions, TArray<FVector>& OutPositions)
{
	TArray<FString> PointStrings;

	static const TCHAR* PositionSeparators[] =
	{
		TEXT(" "),
		TEXT(","),
	};

	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	float GeneratedGeometryScaleFactor = FHoudiniEngineUtils::ScaleFactorPosition;
	EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

	if(HoudiniRuntimeSettings)
	{
		GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
		ImportAxis = HoudiniRuntimeSettings->ImportAxis;
	}

	int32 NumCoords = Positions.ParseIntoArray(PointStrings, PositionSeparators, 2);
	for(int32 CoordIdx = 0; CoordIdx < NumCoords; CoordIdx += 3)
	{
		FVector Position;

		Position.X = FCString::Atof(*PointStrings[CoordIdx + 0]);
		Position.Y = FCString::Atof(*PointStrings[CoordIdx + 1]);
		Position.Z = FCString::Atof(*PointStrings[CoordIdx + 2]);

		Position *= GeneratedGeometryScaleFactor;

		if(HRSAI_Unreal == ImportAxis)
		{
			Swap(Position.Y, Position.Z);
		}
		else if(HRSAI_Houdini == ImportAxis)
		{
			// No action required.
		}
		else
		{
			// Not valid enum value.
			check(0);
		}

		OutPositions.Add(Position);
	}
}


void
FHoudiniEngineUtils::CreatePositionsString(const TArray<FVector>& Positions, FString& PositionString)
{
	PositionString = TEXT("");

	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	float GeneratedGeometryScaleFactor = FHoudiniEngineUtils::ScaleFactorPosition;
	EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

	if(HoudiniRuntimeSettings)
	{
		GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
		ImportAxis = HoudiniRuntimeSettings->ImportAxis;
	}

	for(int32 Idx = 0; Idx < Positions.Num(); ++Idx)
	{
		FVector Position = Positions[Idx];

		if(HRSAI_Unreal == ImportAxis)
		{
			Swap(Position.Z, Position.Y);
		}
		else if(HRSAI_Houdini == ImportAxis)
		{
			// No action required.
		}
		else
		{
			// Not valid enum value.
			check(0);
		}

		if(GeneratedGeometryScaleFactor != 0.0f)
		{
			Position /= GeneratedGeometryScaleFactor;
		}

		PositionString += FString::Printf(TEXT("%f, %f, %f "), Position.X, Position.Y, Position.Z);
	}
}


void
FHoudiniEngineUtils::ConvertScaleAndFlipVectorData(const TArray<float>& DataRaw, TArray<FVector>& DataOut)
{
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	float GeneratedGeometryScaleFactor = FHoudiniEngineUtils::ScaleFactorPosition;
	EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

	if(HoudiniRuntimeSettings)
	{
		GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
		ImportAxis = HoudiniRuntimeSettings->ImportAxis;
	}

	for(int32 Idx = 0; Idx < DataRaw.Num(); Idx += 3)
	{
		FVector Point(DataRaw[Idx + 0], DataRaw[Idx + 1], DataRaw[Idx + 2]);

		Point *= GeneratedGeometryScaleFactor;

		if(HRSAI_Unreal == ImportAxis)
		{
			Swap(Point.Z, Point.Y);
		}
		else if(HRSAI_Houdini == ImportAxis)
		{
			// No action required.
		}
		else
		{
			// Not valid enum value.
			check(0);
		}

		DataOut.Add(Point);
	}
}


FString
FHoudiniEngineUtils::HoudiniGetLibHAPIName()
{
	static const FString LibHAPIName =

#if PLATFORM_WINDOWS

		HAPI_LIB_OBJECT_WINDOWS;

#elif PLATFORM_MAC

		HAPI_LIB_OBJECT_MAC;

#elif PLATFORM_LINUX

		HAPI_LIB_OBJECT_LINUX;

#else

		TEXT("");

#endif

	return LibHAPIName;
}


#if PLATFORM_WINDOWS

void*
FHoudiniEngineUtils::LocateLibHAPIInRegistry(const FString& HoudiniInstallationType,
	const FString& HoudiniVersionString, FString& StoredLibHAPILocation)
{
	FString HoudiniRegistryLocation =
		FString::Printf(
			TEXT("Software\\Side Effects Software\\%s %s"), *HoudiniInstallationType, *HoudiniVersionString);

	FString HoudiniInstallationPath;

	if(FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *HoudiniRegistryLocation, TEXT("InstallPath"),
		HoudiniInstallationPath))
	{
		HoudiniInstallationPath += FString::Printf(TEXT("/%s"), HAPI_HFS_SUBFOLDER_WINDOWS);

		// Create full path to libHAPI binary.
		FString LibHAPIPath = FString::Printf(TEXT("%s/%s"), *HoudiniInstallationPath, HAPI_LIB_OBJECT_WINDOWS);

		if(FPaths::FileExists(LibHAPIPath))
		{
			FPlatformProcess::PushDllDirectory(*HoudiniInstallationPath);
			void* HAPILibraryHandle = FPlatformProcess::GetDllHandle(HAPI_LIB_OBJECT_WINDOWS);
			FPlatformProcess::PopDllDirectory(*HoudiniInstallationPath);

			if(HAPILibraryHandle)
			{
				HOUDINI_LOG_MESSAGE(
					TEXT("Loaded %s from Registry path %s"), HAPI_LIB_OBJECT_WINDOWS,
					*HoudiniInstallationPath);

				StoredLibHAPILocation = HoudiniInstallationPath;
				return HAPILibraryHandle;
			}
		}
	}

	return nullptr;
}

#endif


void*
FHoudiniEngineUtils::LoadLibHAPI(FString& StoredLibHAPILocation)
{
	FString HFSPath = TEXT("");
	void* HAPILibraryHandle = nullptr;

	// Before doing anything platform specific, check if HFS environment variable is defined.
	TCHAR HFS_ENV_VARIABLE[MAX_PATH];
	FMemory::Memzero(&HFS_ENV_VARIABLE[0], sizeof(TCHAR) * MAX_PATH);

	// Look up HAPI_PATH environment variable; if it is not defined, 0 will stored in HFS_ENV_VARIABLE .
	FPlatformMisc::GetEnvironmentVariable(TEXT("HAPI_PATH"), HFS_ENV_VARIABLE, MAX_PATH);
	if(*HFS_ENV_VARIABLE)
	{
		HFSPath = &HFS_ENV_VARIABLE[0];
	}

	// Look up environment variable; if it is not defined, 0 will stored in HFS_ENV_VARIABLE .
	FPlatformMisc::GetEnvironmentVariable(TEXT("HFS"), HFS_ENV_VARIABLE, MAX_PATH);
	if(*HFS_ENV_VARIABLE)
	{
		HFSPath = &HFS_ENV_VARIABLE[0];
	}

	// Get platform specific name of libHAPI.
	FString LibHAPIName = FHoudiniEngineUtils::HoudiniGetLibHAPIName();

	// We have HFS environment variable defined, attempt to load libHAPI from it.
	if(!HFSPath.IsEmpty())
	{

#if PLATFORM_WINDOWS

		HFSPath += FString::Printf(TEXT("/%s"), HAPI_HFS_SUBFOLDER_WINDOWS);

#elif PLATFORM_MAC

		HFSPath += FString::Printf(TEXT("/%s"), HAPI_HFS_SUBFOLDER_MAC);

#elif PLATFORM_LINUX

		HFSPath += FString::Printf(TEXT("/%s"), HAPI_HFS_SUBFOLDER_LINUX);

#endif

		// Create full path to libHAPI binary.
		FString LibHAPIPath = FString::Printf(TEXT("%s/%s"), *HFSPath, *LibHAPIName);

		if(FPaths::FileExists(LibHAPIPath))
		{
			// libHAPI binary exists at specified location, attempt to load it.
			FPlatformProcess::PushDllDirectory(*HFSPath);

#if PLATFORM_WINDOWS

			HAPILibraryHandle = FPlatformProcess::GetDllHandle(*LibHAPIName);

#elif PLATFORM_MAC || PLATFORM_LINUX

			HAPILibraryHandle = FPlatformProcess::GetDllHandle(*LibHAPIPath);

#endif

			FPlatformProcess::PopDllDirectory(*HFSPath);

			// If library has been loaded successfully we can stop.
			if(HAPILibraryHandle)
			{
				HOUDINI_LOG_MESSAGE(TEXT("Loaded %s from HFS environment path %s"), *LibHAPIName, *HFSPath);
				StoredLibHAPILocation = HFSPath;
				return HAPILibraryHandle;
			}
		}
	}

	// Compute Houdini version string.
	FString HoudiniVersionString = FString::Printf(TEXT("%d.%d.%d"), HAPI_VERSION_HOUDINI_MAJOR,
		HAPI_VERSION_HOUDINI_MINOR, HAPI_VERSION_HOUDINI_BUILD);

	// If we have a patch version, we need to append it.
	if(HAPI_VERSION_HOUDINI_PATCH > 0)
	{
		HoudiniVersionString = FString::Printf(TEXT("%s.%d"), *HoudiniVersionString, HAPI_VERSION_HOUDINI_PATCH);
	}


	// Otherwise, we will attempt to detect Houdini installation.
#if PLATFORM_WINDOWS

	// On Windows, we have also hardcoded HFS path in plugin configuration file; attempt to load from it.
	HFSPath = HOUDINI_ENGINE_HFS_PATH;

	if(!HFSPath.IsEmpty())
	{
		HFSPath += FString::Printf(TEXT("/%s"), HAPI_HFS_SUBFOLDER_WINDOWS);

		// Create full path to libHAPI binary.
		FString LibHAPIPath = FString::Printf(TEXT("%s/%s"), *HFSPath, *LibHAPIName);

		if(FPaths::FileExists(LibHAPIPath))
		{
			FPlatformProcess::PushDllDirectory(*HFSPath);
			HAPILibraryHandle = FPlatformProcess::GetDllHandle(*LibHAPIName);
			FPlatformProcess::PopDllDirectory(*HFSPath);

			if(HAPILibraryHandle)
			{
				HOUDINI_LOG_MESSAGE(TEXT("Loaded %s from Plugin defined HFS path %s"), *LibHAPIName, *HFSPath);
				StoredLibHAPILocation = HFSPath;
				return HAPILibraryHandle;
			}
		}
	}

	// As a second attempt, on Windows, we try to look up location of Houdini Engine in the registry.
	HAPILibraryHandle = FHoudiniEngineUtils::LocateLibHAPIInRegistry(TEXT("Houdini Engine"), HoudiniVersionString,
		StoredLibHAPILocation);
	if(HAPILibraryHandle)
	{
		return HAPILibraryHandle;
	}

	// As a third attempt, we try to look up location of Houdini installation (not Houdini Engine) in the registry.
	HAPILibraryHandle = FHoudiniEngineUtils::LocateLibHAPIInRegistry(TEXT("Houdini"), HoudiniVersionString,
		StoredLibHAPILocation);
	if(HAPILibraryHandle)
	{
		return HAPILibraryHandle;
	}

	// As a fourth attempt, we will try to load from hardcoded program files path.
	FString HoudiniLocation = FString::Printf(
		TEXT("C:\\Program Files\\Side Effects Software\\Houdini %s\\%s"), *HoudiniVersionString,
			HAPI_HFS_SUBFOLDER_WINDOWS);

#else

#	if PLATFORM_MAC

	// Attempt to load from standard Mac OS X installation.
	FString HoudiniLocation = FString::Printf(
		TEXT("/Library/Frameworks/Houdini.framework/Versions/%s/Libraries"), *HoudiniVersionString);

#	elif PLATFORM_LINUX

	// Attempt to load from standard Linux installation.
	FString HoudiniLocation = FString::Printf(
		TEXT("/opt/dev%s/") + HAPI_HFS_SUBFOLDER_LINUX, *HoudiniVersionString);

#	endif

#endif

	// Create full path to libHAPI binary.
	FString LibHAPIPath = FString::Printf(TEXT("%s/%s"), *HoudiniLocation, *LibHAPIName);

	if(FPaths::FileExists(LibHAPIPath))
	{
		FPlatformProcess::PushDllDirectory(*HoudiniLocation);
		HAPILibraryHandle = FPlatformProcess::GetDllHandle(*LibHAPIPath);
		FPlatformProcess::PopDllDirectory(*HoudiniLocation);

		if(HAPILibraryHandle)
		{
			HOUDINI_LOG_MESSAGE(TEXT("Loaded %s from expected installation %s"), *LibHAPIName, *HoudiniLocation);
			StoredLibHAPILocation = HoudiniLocation;
			return HAPILibraryHandle;
		}
	}

	StoredLibHAPILocation = TEXT("");
	return HAPILibraryHandle;
}


int32
FHoudiniEngineUtils::HapiGetVertexListForGroup(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId,
	HAPI_PartId PartId, const FString& GroupName, const TArray<int32>& FullVertexList, TArray<int32>& NewVertexList,
	TArray<int32>& AllVertexList, TArray<int32>& AllFaceList, TArray<int32>& AllCollisionFaceIndices)
{
	NewVertexList.Init(-1, FullVertexList.Num());
	int32 ProcessedWedges = 0;

	AllFaceList.Empty();

	TArray<int32> PartGroupMembership;
	FHoudiniEngineUtils::HapiGetGroupMembership(
		AssetId, ObjectId, GeoId, PartId, HAPI_GROUPTYPE_PRIM, GroupName, PartGroupMembership);

	// Go through all primitives.
	for(int32 FaceIdx = 0; FaceIdx < PartGroupMembership.Num(); ++FaceIdx)
	{
		if(PartGroupMembership[FaceIdx] > 0)
		{
			// Add face.
			AllFaceList.Add(FaceIdx);

			// This face is a member of specified group.
			NewVertexList[FaceIdx * 3 + 0] = FullVertexList[FaceIdx * 3 + 0];
			NewVertexList[FaceIdx * 3 + 1] = FullVertexList[FaceIdx * 3 + 1];
			NewVertexList[FaceIdx * 3 + 2] = FullVertexList[FaceIdx * 3 + 2];

			// Mark these vertex indices as used.
			AllVertexList[FaceIdx * 3 + 0] = 1;
			AllVertexList[FaceIdx * 3 + 1] = 1;
			AllVertexList[FaceIdx * 3 + 2] = 1;

			// Mark this face as used.
			AllCollisionFaceIndices[FaceIdx] = 1;

			ProcessedWedges += 3;
		}
	}

	return ProcessedWedges;
}


#if WITH_EDITOR

bool
FHoudiniEngineUtils::ContainsInvalidLightmapFaces(const FRawMesh& RawMesh, int32 LightmapSourceIdx)
{
	const TArray<FVector2D>& LightmapUVs = RawMesh.WedgeTexCoords[LightmapSourceIdx];
	const TArray<uint32>& Indices = RawMesh.WedgeIndices;

	if(LightmapUVs.Num() != Indices.Num())
	{
		// This is invalid raw mesh; by design we consider that it contains invalid lightmap faces.
		return true;
	}

	for(int32 Idx = 0; Idx < Indices.Num(); Idx += 3)
	{
		const FVector2D& uv0 = LightmapUVs[Idx + 0];
		const FVector2D& uv1 = LightmapUVs[Idx + 1];
		const FVector2D& uv2 = LightmapUVs[Idx + 2];

		if(uv0 == uv1 && uv1 == uv2)
		{
			// Detect invalid lightmap face, can stop.
			return true;
		}
	}

	// Otherwise there are no invalid lightmap faces.
	return false;
}

#endif


int32
FHoudiniEngineUtils::CountUVSets(const FRawMesh& RawMesh)
{
	int32 UVSetCount = 0;

#if WITH_EDITOR

	for(int32 TexCoordIdx = 0; TexCoordIdx < MAX_MESH_TEXTURE_COORDS; ++TexCoordIdx)
	{
		const TArray<FVector2D>& WedgeTexCoords = RawMesh.WedgeTexCoords[TexCoordIdx];
		if(WedgeTexCoords.Num() > 0)
		{
			UVSetCount++;
		}
	}

#endif

	return UVSetCount;
}


const FString
FHoudiniEngineUtils::GetStatusString(HAPI_StatusType status_type, HAPI_StatusVerbosity verbosity)
{
	int32 StatusBufferLength = 0;
	FHoudiniApi::GetStatusStringBufLength(FHoudiniEngine::Get().GetSession(), status_type, verbosity, &StatusBufferLength);

	if(StatusBufferLength > 0)
	{
		TArray<char> StatusStringBuffer;
		StatusStringBuffer.SetNumZeroed(StatusBufferLength);
		FHoudiniApi::GetStatusString(FHoudiniEngine::Get().GetSession(), status_type, &StatusStringBuffer[0], StatusBufferLength);

		return FString(UTF8_TO_TCHAR(&StatusStringBuffer[0]));
	}

	return FString(TEXT(""));
}


bool
FHoudiniEngineUtils::ExtractUniqueMaterialIds(const HAPI_AssetInfo& AssetInfo, TSet<HAPI_MaterialId>& MaterialIds)
{
	// Empty passed material id container.
	MaterialIds.Empty();

	TArray<HAPI_ObjectInfo> ObjectInfos;
	ObjectInfos.SetNumUninitialized(AssetInfo.objectCount);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetObjects(FHoudiniEngine::Get().GetSession(), AssetInfo.id, &ObjectInfos[0], 0, AssetInfo.objectCount), false);

	// Iterate through all objects.
	for(int32 ObjectIdx = 0; ObjectIdx < ObjectInfos.Num(); ++ObjectIdx)
	{
		// Retrieve object at this index.
		const HAPI_ObjectInfo& ObjectInfo = ObjectInfos[ObjectIdx];

		// Iterate through all Geo informations within this object.
		for(int32 GeoIdx = 0; GeoIdx < ObjectInfo.geoCount; ++GeoIdx)
		{
			// Get Geo information.
			HAPI_GeoInfo GeoInfo;
			if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetGeoInfo(FHoudiniEngine::Get().GetSession(), AssetInfo.id, ObjectInfo.id, GeoIdx, &GeoInfo))
			{
				continue;
			}

			for(int32 PartIdx = 0; PartIdx < GeoInfo.partCount; ++PartIdx)
			{
				// Get part information.
				HAPI_PartInfo PartInfo;
				FString PartName = TEXT("");

				if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), AssetInfo.id, ObjectInfo.id, GeoInfo.id, PartIdx,
					&PartInfo))
				{
					continue;
				}

				// Retrieve material information for this geo part.
				TArray<HAPI_MaterialId> FaceMaterialIds;
				HAPI_Bool bSingleFaceMaterial = false;
				bool bMaterialsFound = false;

				if(PartInfo.faceCount > 0)
				{
					FaceMaterialIds.SetNumUninitialized(PartInfo.faceCount);

					if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetMaterialIdsOnFaces(FHoudiniEngine::Get().GetSession(), AssetInfo.id, ObjectInfo.id,
						GeoInfo.id, PartInfo.id, &bSingleFaceMaterial, &FaceMaterialIds[0], 0, PartInfo.faceCount))
					{
						continue;
					}

					MaterialIds.Append(FaceMaterialIds);
				}
			}
		}
	}

	MaterialIds.Remove(-1);

	return true;
}


bool
FHoudiniEngineUtils::MaterialHasTextureSampleExpression(UMaterial* Material)
{
	if(Material)
	{
		if(Cast<UMaterialExpressionTextureSample>(Material->BaseColor.Expression))
		{
			return true;
		}

		if(Cast<UMaterialExpressionTextureSample>(Material->Normal.Expression))
		{
			return true;
		}
	}

	return false;
}


AHoudiniAssetActor*
FHoudiniEngineUtils::LocateClipboardActor(const FString& ClipboardText)
{
	const TCHAR* Paste = nullptr;

	if(ClipboardText.IsEmpty())
	{
		FString PasteString;
		FPlatformMisc::ClipboardPaste(PasteString);
		Paste = *PasteString;
	}
	else
	{
		Paste = *ClipboardText;
	}

	AHoudiniAssetActor* HoudiniAssetActor = nullptr;
	FString ActorName = TEXT("");

	FString StrLine;
	while(FParse::Line(&Paste, StrLine))
	{
		StrLine = StrLine.Trim();

		const TCHAR* Str = *StrLine;
		FString ClassName;

		if(StrLine.StartsWith(TEXT("Begin Actor")) && FParse::Value(Str, TEXT("Class="), ClassName))
		{
			if(ClassName == *AHoudiniAssetActor::StaticClass()->GetFName().ToString())
			{
				if(FParse::Value(Str, TEXT("Name="), ActorName))
				{
					HoudiniAssetActor = Cast<AHoudiniAssetActor>(StaticFindObject(AHoudiniAssetActor::StaticClass(),
						ANY_PACKAGE, *ActorName));

					break;
				}
			}
		}
		else if(StrLine.StartsWith(TEXT("ActorLabel")) && FParse::Value(Str, TEXT("ActorLabel="), ActorName))
		{
			HoudiniAssetActor = Cast<AHoudiniAssetActor>(StaticFindObject(AHoudiniAssetActor::StaticClass(),
				ANY_PACKAGE, *ActorName));

			break;
		}
	}

	return HoudiniAssetActor;
}


void
FHoudiniEngineUtils::UpdateInstancedStaticMeshComponentInstances(UInstancedStaticMeshComponent* Component,
	const TArray<FTransform>& InstancedTransforms, const FRotator& RotationOffset, const FVector& ScaleOffset)
{
	Component->ClearInstances();

	for(int32 InstanceIdx = 0; InstanceIdx < InstancedTransforms.Num(); ++InstanceIdx)
	{
		FTransform Transform = InstancedTransforms[InstanceIdx];

		// Compute new rotation and scale.
		FQuat TransformRotation = Transform.GetRotation() * RotationOffset.Quaternion();
		FVector TransformScale3D = Transform.GetScale3D() * ScaleOffset;

		// Make sure inverse matrix exists - seems to be a bug in Unreal when submitting instances.
		// Happens in blueprint as well.
		if(TransformScale3D.X < FHoudiniEngineUtils::ScaleSmallValue)
		{
			TransformScale3D.X = FHoudiniEngineUtils::ScaleSmallValue;
		}

		if(TransformScale3D.Y < FHoudiniEngineUtils::ScaleSmallValue)
		{
			TransformScale3D.Y = FHoudiniEngineUtils::ScaleSmallValue;
		}

		if(TransformScale3D.Z < FHoudiniEngineUtils::ScaleSmallValue)
		{
			TransformScale3D.Z = FHoudiniEngineUtils::ScaleSmallValue;
		}

		Transform.SetRotation(TransformRotation);
		Transform.SetScale3D(TransformScale3D);

		Component->AddInstance(Transform);
	}
}


bool
FHoudiniEngineUtils::GetAssetNames(UHoudiniAsset* HoudiniAsset, HAPI_AssetLibraryId& OutAssetLibraryId,
	TArray<int32>& OutAssetNames)
{
	OutAssetLibraryId = -1;
	OutAssetNames.Empty();

	if(FHoudiniEngineUtils::IsInitialized() && HoudiniAsset)
	{
		const FString& AssetFileName = HoudiniAsset->GetAssetFileName();
		HAPI_Result Result = HAPI_RESULT_SUCCESS;
		HAPI_AssetLibraryId AssetLibraryId = -1;
		int32 AssetCount = 0;
		TArray<int32> AssetNames;

		if(!AssetFileName.IsEmpty() && FPaths::FileExists(AssetFileName))
		{
			// File does exist, we can load asset from file.
			std::string AssetFileNamePlain;
			FHoudiniEngineUtils::ConvertUnrealString(AssetFileName, AssetFileNamePlain);

			Result = FHoudiniApi::LoadAssetLibraryFromFile(
				FHoudiniEngine::Get().GetSession(), AssetFileNamePlain.c_str(), true, &AssetLibraryId);
		}
		else
		{
			// Otherwise we will try to load from buffer we've cached.
			Result = FHoudiniApi::LoadAssetLibraryFromMemory(FHoudiniEngine::Get().GetSession(),
				reinterpret_cast<const char*>(HoudiniAsset->GetAssetBytes()),
				HoudiniAsset->GetAssetBytesCount(), true, &AssetLibraryId);
		}

		if(HAPI_RESULT_SUCCESS != Result)
		{
			HOUDINI_LOG_MESSAGE(TEXT("Error loading asset library for %s"), *AssetFileName);
			return false;
		}

		Result = FHoudiniApi::GetAvailableAssetCount(FHoudiniEngine::Get().GetSession(), AssetLibraryId, &AssetCount);
		if(HAPI_RESULT_SUCCESS != Result)
		{
			HOUDINI_LOG_MESSAGE(TEXT("Error getting asset count for %s"), *AssetFileName);
			return false;
		}

		AssetNames.SetNumUninitialized(AssetCount);

		Result = FHoudiniApi::GetAvailableAssets(FHoudiniEngine::Get().GetSession(), AssetLibraryId, &AssetNames[0], AssetCount);
		if(HAPI_RESULT_SUCCESS != Result)
		{
			HOUDINI_LOG_MESSAGE(TEXT("Unable to retrieve asset names for %s"), *AssetFileName);
			return false;
		}

		if(!AssetCount)
		{
			HOUDINI_LOG_MESSAGE(TEXT("No assets found within %s"), *AssetFileName);
			return false;
		}

		OutAssetLibraryId = AssetLibraryId;
		OutAssetNames = AssetNames;
	
		return true;
	}

	return false;
}


void
FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(UPackage* Package, UObject* Object, const TCHAR* Key,
	const TCHAR* Value)
{
	UMetaData* MetaData = Package->GetMetaData();
	MetaData->SetValue(Object, Key, Value);
}


bool
FHoudiniEngineUtils::GetHoudiniGeneratedNameFromMetaInformation(UPackage* Package, UObject* Object,
	FString& HoudiniName)
{
	UMetaData* MetaData = Package->GetMetaData();
	if(MetaData && MetaData->HasValue(Object, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT))
	{
		// Retrieve name used for package generation.
		const FString NameFull = MetaData->GetValue(Object, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME);
		HoudiniName = NameFull.Left(NameFull.Len() - FHoudiniEngineUtils::PackageGUIDItemNameLength);

		return true;
	}

	return false;
}


UStaticMesh*
FHoudiniEngineUtils::DuplicateStaticMeshAndCreatePackage(UStaticMesh* StaticMesh, UHoudiniAssetComponent* Component,
	const FHoudiniGeoPartObject& HoudiniGeoPartObject, bool bBake)
{
	UStaticMesh* DuplicatedStaticMesh = nullptr;

	if(!HoudiniGeoPartObject.IsCurve() && !HoudiniGeoPartObject.IsInstancer())
	{
		// Create package for this duplicated mesh.
		FString MeshName;
		FGuid MeshGuid;

		UPackage* MeshPackage =
			FHoudiniEngineUtils::BakeCreateStaticMeshPackageForComponent(Component, HoudiniGeoPartObject,
				MeshName, MeshGuid, bBake);

		// Duplicate mesh for this new copied component.
		DuplicatedStaticMesh = DuplicateObject<UStaticMesh>(StaticMesh, MeshPackage, *MeshName);

		if(bBake)
		{
			DuplicatedStaticMesh->SetFlags(RF_Public);
		}

		// Add meta information.
		FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(MeshPackage, DuplicatedStaticMesh,
			HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
		FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(MeshPackage, DuplicatedStaticMesh,
			HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MeshName);

		// See if we need to duplicate materials and textures.
		TArray<UMaterialInterface*> DuplicatedMaterials;
		TArray<UMaterialInterface*>& Materials = DuplicatedStaticMesh->Materials;

		for(int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); ++MaterialIdx)
		{
			UMaterialInterface* MaterialInterface = Materials[MaterialIdx];
			if(MaterialInterface)
			{
				UPackage* MaterialPackage = Cast<UPackage>(MaterialInterface->GetOuter());
				if(MaterialPackage)
				{
					FString MaterialName;
					if(FHoudiniEngineUtils::GetHoudiniGeneratedNameFromMetaInformation(MaterialPackage, MaterialInterface,
						MaterialName))
					{
						// We only deal with materials.
						UMaterial* Material = Cast<UMaterial>(MaterialInterface);
						if(Material)
						{
							// Duplicate material resource.
							UMaterial* DuplicatedMaterial =
								FHoudiniEngineUtils::DuplicateMaterialAndCreatePackage(Material, Component, MaterialName, bBake);

							// Store duplicated material.
							DuplicatedMaterials.Add(DuplicatedMaterial);
							continue;
						}
					}
				}
			}

			DuplicatedMaterials.Add(MaterialInterface);
		}

		// Assign duplicated materials.
		DuplicatedStaticMesh->Materials = DuplicatedMaterials;

		// Notify registry that we have created a new duplicate mesh.
		FAssetRegistryModule::AssetCreated(DuplicatedStaticMesh);

		// Dirty the static mesh package.
		DuplicatedStaticMesh->MarkPackageDirty();
	}

	return DuplicatedStaticMesh;
}


UMaterial*
FHoudiniEngineUtils::DuplicateMaterialAndCreatePackage(UMaterial* Material, UHoudiniAssetComponent* Component,
	const FString& SubMaterialName, bool bBake)
{
	UMaterial* DuplicatedMaterial = nullptr;

	// Create material package.
	FString MaterialName;
	UPackage* MaterialPackage = FHoudiniEngineUtils::BakeCreateMaterialPackageForComponent(Component, SubMaterialName,
		MaterialName, bBake);

	// Clone material.
	DuplicatedMaterial = DuplicateObject<UMaterial>(Material, MaterialPackage, *MaterialName);

	if(bBake)
	{
		DuplicatedMaterial->SetFlags(RF_Public);
	}

	// Add meta information.
	FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(MaterialPackage, DuplicatedMaterial,
		HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
	FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(MaterialPackage, DuplicatedMaterial,
		HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MaterialName);

	// Retrieve and check various sampling expressions. If they contain textures, duplicate (and bake) them.
	FHoudiniEngineUtils::ReplaceDuplicatedMaterialTextureSample(DuplicatedMaterial->BaseColor.Expression,
		Component, bBake);
	FHoudiniEngineUtils::ReplaceDuplicatedMaterialTextureSample(DuplicatedMaterial->Normal.Expression,
		Component, bBake);
	FHoudiniEngineUtils::ReplaceDuplicatedMaterialTextureSample(DuplicatedMaterial->Metallic.Expression,
		Component, bBake);
	FHoudiniEngineUtils::ReplaceDuplicatedMaterialTextureSample(DuplicatedMaterial->Specular.Expression,
		Component, bBake);
	FHoudiniEngineUtils::ReplaceDuplicatedMaterialTextureSample(DuplicatedMaterial->Roughness.Expression,
		Component, bBake);
	FHoudiniEngineUtils::ReplaceDuplicatedMaterialTextureSample(DuplicatedMaterial->EmissiveColor.Expression,
		Component, bBake);

	// Notify registry that we have created a new duplicate material.
	FAssetRegistryModule::AssetCreated(DuplicatedMaterial);

	// Dirty the material package.
	DuplicatedMaterial->MarkPackageDirty();

	return DuplicatedMaterial;
}


void
FHoudiniEngineUtils::ReplaceDuplicatedMaterialTextureSample(UMaterialExpression* MaterialExpression,
	UHoudiniAssetComponent* Component, bool bBake)
{
	UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>(MaterialExpression);
	if(TextureSample)
	{
		UTexture2D* Texture = Cast<UTexture2D>(TextureSample->Texture);
		if(Texture)
		{
			UPackage* TexturePackage = Cast<UPackage>(Texture->GetOuter());
			if(TexturePackage)
			{
				FString GeneratedTextureName;
				if(FHoudiniEngineUtils::GetHoudiniGeneratedNameFromMetaInformation(TexturePackage, Texture,
					GeneratedTextureName))
				{
					// Duplicate texture.
					UTexture2D* DuplicatedTexture =
						FHoudiniEngineUtils::DuplicateTextureAndCreatePackage(Texture, Component,
							GeneratedTextureName, bBake);

					// Re-assign generated texture.
					TextureSample->Texture = DuplicatedTexture;
				}
			}
		}
	}
}


UTexture2D*
FHoudiniEngineUtils::DuplicateTextureAndCreatePackage(UTexture2D* Texture, UHoudiniAssetComponent* Component,
	const FString& SubTextureName, bool bBake)
{
	UTexture2D* DuplicatedTexture = nullptr;

	// Retrieve original package of this texture.
	UPackage* TexturePackage = Cast<UPackage>(Texture->GetOuter());
	if(TexturePackage)
	{
		FString GeneratedTextureName;
		if(FHoudiniEngineUtils::GetHoudiniGeneratedNameFromMetaInformation(TexturePackage, Texture, GeneratedTextureName))
		{
			UMetaData* MetaData = TexturePackage->GetMetaData();
			if(MetaData)
			{
				// Retrieve texture type.
				const FString& TextureType =
					MetaData->GetValue(Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE);

				// Create texture package.
				FString TextureName;
				UPackage* NewTexturePackage = FHoudiniEngineUtils::BakeCreateTexturePackageForComponent(Component,
					SubTextureName, TextureType, TextureName, bBake);

				// Clone texture.
				DuplicatedTexture = DuplicateObject<UTexture2D>(Texture, NewTexturePackage, *TextureName);

				if(bBake)
				{
					DuplicatedTexture->SetFlags(RF_Public);
				}

				// Add meta information.
				FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(NewTexturePackage, DuplicatedTexture,
					HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
				FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(NewTexturePackage, DuplicatedTexture,
					HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *TextureName);
				FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(NewTexturePackage, DuplicatedTexture,
					HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE, *TextureType);

				// Notify registry that we have created a new duplicate texture.
				FAssetRegistryModule::AssetCreated(DuplicatedTexture);

				// Dirty the texture package.
				DuplicatedTexture->MarkPackageDirty();
			}
		}
	}

	return DuplicatedTexture;
}

