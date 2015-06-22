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
	return (FHoudiniApi::IsHAPIInitialized() && HAPI_RESULT_SUCCESS == FHoudiniApi::IsInitialized());
}


bool
FHoudiniEngineUtils::ComputeAssetPresetBufferLength(
	HAPI_AssetId AssetId, int32& OutBufferLength)
{
	HAPI_AssetInfo AssetInfo;
	OutBufferLength = 0;

	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::GetAssetInfo(AssetId, &AssetInfo), false);

	int32 BufferLength = 0;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPresetBufLength(
		AssetInfo.nodeId, HAPI_PRESETTYPE_BINARY, NULL, &BufferLength), false);

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
			FHoudiniApi::GetAssetInfo(AssetId, &AssetInfo), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPreset(
			AssetInfo.nodeId, HAPI_PRESETTYPE_BINARY, NULL,
			&PresetBuffer[0], PresetBuffer.Num()),
			false);

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
		FHoudiniApi::GetAssetInfo(AssetId, &AssetInfo), false);

	int32 BufferLength = 0;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPresetBufLength(
		AssetInfo.nodeId, HAPI_PRESETTYPE_BINARY, NULL, &BufferLength), false);

	PresetBuffer.SetNumZeroed(BufferLength);
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetPreset(
		AssetInfo.nodeId, &PresetBuffer[0], PresetBuffer.Num()), false);

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

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(AssetId, &AssetInfo), false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::IsAssetValid(AssetId, AssetInfo.validationId, &ValidationAnswer), false);

	return (0 != ValidationAnswer);
}


bool
FHoudiniEngineUtils::DestroyHoudiniAsset(HAPI_AssetId AssetId)
{
	return(HAPI_RESULT_SUCCESS == FHoudiniApi::DestroyAsset(AssetId));
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
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetStringBufLength(Name, &NameLength), false);

	if(NameLength)
	{
		std::vector<char> NameBuffer(NameLength, '\0');
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetString(Name, &NameBuffer[0], NameLength), false);

		// Create and return string.
		NameString = std::string(NameBuffer.begin(), NameBuffer.end());
	}

	return true;
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
FHoudiniEngineUtils::GetHoudiniAssetName(HAPI_AssetId AssetId, FString& NameString)
{
	HAPI_AssetInfo AssetInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(AssetId, &AssetInfo), false);

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
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGeoInfo(AssetId, ObjectId, GeoId, &GeoInfo), false);

	int32 GroupCount = FHoudiniEngineUtils::HapiGetGroupCountByType(GroupType, GeoInfo);

	if(GroupCount > 0)
	{
		std::vector<int32> GroupNameHandles(GroupCount, 0);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGroupNames(AssetId, ObjectId, GeoId, GroupType, &GroupNameHandles[0],
			GroupCount), false);

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
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(AssetId, ObjectId, GeoId, PartId, &PartInfo), false);

	int32 ElementCount = FHoudiniEngineUtils::HapiGetElementCountByGroupType(GroupType, PartInfo);
	std::string ConvertedGroupName = TCHAR_TO_UTF8(*GroupName);

	GroupMembership.SetNumUninitialized(ElementCount);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGroupMembership(AssetId, ObjectId, GeoId, PartId, GroupType,
		ConvertedGroupName.c_str(), &GroupMembership[0], 0, ElementCount), false);

	return true;
}


int32
FHoudiniEngineUtils::HapiCheckGroupMembership(const TArray<int32>& GroupMembership)
{
	int32 GroupMembershipCount = 0;
	for(int32 Idx = 0; Idx < GroupMembership.Num(); ++Idx)
	{
		if(GroupMembership[Idx] > 0)
		{
			++GroupMembershipCount;
		}
	}

	return GroupMembershipCount;
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
FHoudiniEngineUtils::HapiRetrieveParameterNames(
	const std::vector<HAPI_ParmInfo>& ParmInfos, std::vector<std::string>& Names)
{
	static const std::string InvalidParameterName("Invalid Parameter Name");

	Names.resize(ParmInfos.size());

	for(int32 ParmIdx = 0; ParmIdx < ParmInfos.size(); ++ParmIdx)
	{
		const HAPI_ParmInfo& NodeParmInfo = ParmInfos[ParmIdx];
		HAPI_StringHandle NodeParmHandle = NodeParmInfo.nameSH;

		int32 NodeParmNameLength = 0;
		FHoudiniApi::GetStringBufLength(NodeParmHandle, &NodeParmNameLength);

		if(NodeParmNameLength)
		{
			std::vector<char> NodeParmName(NodeParmNameLength, '\0');

			HAPI_Result Result = FHoudiniApi::GetString(NodeParmHandle, &NodeParmName[0], NodeParmNameLength);
			if(HAPI_RESULT_SUCCESS == Result)
			{
				Names[ParmIdx] = std::string(NodeParmName.begin(), NodeParmName.end() - 1);
			}
			else
			{
				Names[ParmIdx] = InvalidParameterName;
			}
		}
		else
		{
			Names[ParmIdx] = InvalidParameterName;
		}
	}
}


bool
FHoudiniEngineUtils::HapiCheckAttributeExists(
	HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId, const char* Name,
	HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttribInfo;
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeInfo(AssetId, ObjectId, GeoId, PartId, Name, Owner, &AttribInfo))
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
FHoudiniEngineUtils::HapiFindParameterByName(const std::string& ParmName, const std::vector<std::string>& Names)
{
	for(int32 Idx = 0; Idx < Names.size(); ++Idx)
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
			FHoudiniApi::GetAttributeInfo(AssetId, ObjectId, GeoId, PartId, Name, (HAPI_AttributeOwner) AttrIdx,
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
		AssetId, ObjectId, GeoId, PartId, Name, &AttributeInfo, &Data[0], 0, AttributeInfo.count), false);

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
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(AssetId, ObjectId, GeoId, PartId, Name,
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

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeIntData(AssetId, ObjectId, GeoId, PartId, Name, &AttributeInfo,
		&Data[0], 0, AttributeInfo.count), false);

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
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(AssetId, ObjectId, GeoId, PartId, Name,
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
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeStringData(AssetId, ObjectId, GeoId, PartId, Name,
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
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(AssetId, ObjectId, GeoId, PartId, &PartInfo), false);

	if(0 == PartInfo.pointCount)
	{
		return false;
	}

	TArray<HAPI_Transform> InstanceTransforms;
	InstanceTransforms.SetNumZeroed(PartInfo.pointCount);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetInstanceTransforms(AssetId, ObjectId, GeoId, HAPI_SRT,
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
	HAPI_Result Result = FHoudiniApi::RenderTextureToImage(MaterialInfo.assetId, MaterialInfo.id, NodeParmId);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		return false;
	}

	HAPI_ImageInfo ImageInfo;
	Result = FHoudiniApi::GetImageInfo(MaterialInfo.assetId, MaterialInfo.id, &ImageInfo);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		return false;
	}

	ImageInfo.dataFormat = HAPI_IMAGE_DATA_INT8;
	ImageInfo.interleaved = true;
	ImageInfo.packing = HAPI_IMAGE_PACKING_RGBA;

	Result = FHoudiniApi::SetImageInfo(MaterialInfo.assetId, MaterialInfo.id, &ImageInfo);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		return false;
	}

	int32 ImageBufferSize = 0;
	Result = FHoudiniApi::ExtractImageToMemory(MaterialInfo.assetId, MaterialInfo.id, HAPI_RAW_FORMAT_NAME, Type,
		&ImageBufferSize);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		return false;
	}

	if(!ImageBufferSize)
	{
		return false;
	}

	ImageBuffer.SetNumUninitialized(ImageBufferSize);
	Result = FHoudiniApi::GetImageMemoryBuffer(MaterialInfo.assetId, MaterialInfo.id, &ImageBuffer[0], ImageBufferSize);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		return false;
	}

	return true;
}


#if WITH_EDITOR

UTexture2D*
FHoudiniEngineUtils::CreateUnrealTexture(
	const HAPI_ImageInfo& ImageInfo, UPackage* Package, const FString& TextureName, EPixelFormat PixelFormat,
	const TArray<char>& ImageBuffer)
{
	UTexture2D* Texture = NewObject<UTexture2D>(Package, UTexture2D::StaticClass(), *TextureName, RF_Public);
	Texture->PlatformData = new FTexturePlatformData();
	Texture->PlatformData->SizeX = ImageInfo.xRes;
	Texture->PlatformData->SizeY = ImageInfo.yRes;
	Texture->PlatformData->PixelFormat = PixelFormat;
	/*
	NewTexture->SRGB = false;
	NewTexture->CompressionNone = true;
	NewTexture->MipGenSettings = TMGS_LeaveExistingMips;
	NewTexture->AddressX = TA_Clamp;
	NewTexture->AddressY = TA_Clamp;
	NewTexture->LODGroup = InLODGroup;
	*/
	// Allocate first mipmap.
	int32 NumBlocksX = ImageInfo.xRes / GPixelFormats[PixelFormat].BlockSizeX;
	int32 NumBlocksY = ImageInfo.yRes / GPixelFormats[PixelFormat].BlockSizeY;
	FTexture2DMipMap* Mip = new(Texture->PlatformData->Mips) FTexture2DMipMap();
	Mip->SizeX = ImageInfo.xRes;
	Mip->SizeY = ImageInfo.yRes;
	Mip->BulkData.Lock(LOCK_READ_WRITE);
	Mip->BulkData.Realloc(NumBlocksX * NumBlocksY * GPixelFormats[PixelFormat].BlockBytes);
	Mip->BulkData.Unlock();

	// Lock texture for modification.
	uint8* MipData = static_cast<uint8*>(Texture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

	// Create base map.
	uint8* DestPtr = NULL;
	const FColor* SrcPtr = NULL;
	uint32 SrcWidth = ImageInfo.xRes;
	uint32 SrcHeight = ImageInfo.yRes;
	const char* SrcData = &ImageBuffer[0];

	for(uint32 y = 0; y < SrcHeight; y++)
	{
		DestPtr = &MipData[(SrcHeight - 1 - y) * SrcWidth * sizeof(FColor)];

		for(uint32 x = 0; x < SrcWidth; x++)
		{
			uint32 DataOffset = y * SrcWidth * 4 + x * 4;

			*DestPtr++ = *(uint8*)(SrcData + DataOffset + 0); //R
			*DestPtr++ = *(uint8*)(SrcData + DataOffset + 1); //G
			*DestPtr++ = *(uint8*)(SrcData + DataOffset + 2); //B
			*DestPtr++ = *(uint8*)(SrcData + DataOffset + 3); //A
		}
	}

	// Unlock the texture.
	Texture->PlatformData->Mips[0].BulkData.Unlock();
	Texture->UpdateResource();
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
	FHoudiniApi::GetNodeInfo(NodeId, &NodeInfo);

	std::vector<HAPI_ParmInfo> NodeParams;
	NodeParams.resize(NodeInfo.parmCount);
	FHoudiniApi::GetParameters(NodeInfo.id, &NodeParams[0], 0, NodeInfo.parmCount);

	// Get names of parameters.
	std::vector<std::string> NodeParamNames;
	NodeParamNames.resize(NodeInfo.parmCount);
	FHoudiniEngineUtils::HapiRetrieveParameterNames(NodeParams, NodeParamNames);

	// See if parameter is present.
	int32 ParmNameIdx = FHoudiniEngineUtils::HapiFindParameterByName(ParmName, NodeParamNames);

	if(-1 != ParmNameIdx)
	{
		HAPI_ParmInfo& ParmInfo = NodeParams[ParmNameIdx];
		if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetParmFloatValues(NodeId, &Value, ParmInfo.floatValuesIndex, 1))
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
	FHoudiniApi::GetNodeInfo(NodeId, &NodeInfo);

	std::vector<HAPI_ParmInfo> NodeParams;
	NodeParams.resize(NodeInfo.parmCount);
	FHoudiniApi::GetParameters(NodeInfo.id, &NodeParams[0], 0, NodeInfo.parmCount);

	// Get names of parameters.
	std::vector<std::string> NodeParamNames;
	NodeParamNames.resize(NodeInfo.parmCount);
	FHoudiniEngineUtils::HapiRetrieveParameterNames(NodeParams, NodeParamNames);

	// See if parameter is present.
	int32 ParmNameIdx = FHoudiniEngineUtils::HapiFindParameterByName(ParmName, NodeParamNames);

	if(-1 != ParmNameIdx)
	{
		HAPI_ParmInfo& ParmInfo = NodeParams[ParmNameIdx];
		if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetParmIntValues(NodeId, &Value, ParmInfo.intValuesIndex, 1))
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
	FHoudiniApi::GetNodeInfo(NodeId, &NodeInfo);

	std::vector<HAPI_ParmInfo> NodeParams;
	NodeParams.resize(NodeInfo.parmCount);
	FHoudiniApi::GetParameters(NodeInfo.id, &NodeParams[0], 0, NodeInfo.parmCount);

	// Get names of parameters.
	std::vector<std::string> NodeParamNames;
	NodeParamNames.resize(NodeInfo.parmCount);
	FHoudiniEngineUtils::HapiRetrieveParameterNames(NodeParams, NodeParamNames);

	// See if parameter is present.
	int32 ParmNameIdx = FHoudiniEngineUtils::HapiFindParameterByName(ParmName, NodeParamNames);

	if(-1 != ParmNameIdx)
	{
		HAPI_ParmInfo& ParmInfo = NodeParams[ParmNameIdx];
		HAPI_StringHandle StringHandle;
		if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetParmStringValues(NodeId, false, &StringHandle,
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
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateCurve(&AssetId), false);

	CurveAssetId = AssetId;

	HAPI_NodeId NodeId = -1;
	if(!FHoudiniEngineUtils::HapiGetNodeId(AssetId, 0, 0, NodeId))
	{
		return false;
	}

	// Submit default points to curve.
	HAPI_ParmId ParmId = -1;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmIdFromName(NodeId, HAPI_UNREAL_PARAM_CURVE_COORDS, &ParmId), false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(NodeId, HAPI_UNREAL_PARAM_INPUT_CURVE_COORDS_DEFAULT,
		ParmId, 0), false);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookAsset(AssetId, nullptr), false);

#endif

	return true;
}


bool
FHoudiniEngineUtils::HapiGetNodeId(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_NodeId& NodeId)
{
	if(FHoudiniEngineUtils::IsValidAssetId(AssetId))
	{
		HAPI_GeoInfo GeoInfo;
		if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetGeoInfo(AssetId, ObjectId, GeoId, &GeoInfo))
		{
			NodeId = GeoInfo.nodeId;
			return true;
		}
	}

	NodeId = -1;
	return false;
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
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateInputAsset(&AssetId, nullptr), false);

		// Check if we have a valid id for this new input asset.
		if(!FHoudiniEngineUtils::IsHoudiniAssetValid(AssetId))
		{
			return false;
		}

		// We now have a valid id.
		ConnectedAssetId = AssetId;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookAsset(AssetId, nullptr), false);
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

	// Grab base LOD level.
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[0];

	// Load the existing raw mesh.
	FRawMesh RawMesh;
	SrcModel.RawMeshBulkData->LoadRawMesh(RawMesh);

	// Create part.
	HAPI_PartInfo Part;
	Part.id = 0;
	Part.nameSH = 0;
	Part.pointAttributeCount = 0;
	Part.faceAttributeCount = 0;
	Part.vertexAttributeCount = 0;
	Part.detailAttributeCount = 0;
	Part.vertexCount = RawMesh.WedgeIndices.Num();
	Part.faceCount =  RawMesh.WedgeIndices.Num() / 3;
	Part.pointCount = RawMesh.VertexPositions.Num();
	Part.hasVolume = false;
	Part.isCurve = false;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(ConnectedAssetId, 0, 0, &Part), false);

	// Create point attribute info.
	HAPI_AttributeInfo AttributeInfoPoint;
	AttributeInfoPoint.count = RawMesh.VertexPositions.Num();
	AttributeInfoPoint.tupleSize = 3;
	AttributeInfoPoint.exists = true;
	AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(ConnectedAssetId, 0, 0, HAPI_ATTRIB_POSITION,
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
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(ConnectedAssetId, 0, 0, HAPI_ATTRIB_POSITION,
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
			std::string UVAttributeName = HAPI_ATTRIB_UV;

			if(MeshTexCoordIdx > 0)
			{
				UVAttributeName += std::to_string(MeshTexCoordIdx + 1);
			}

			const char* UVAttributeNameString = UVAttributeName.c_str();

			// Create attribute for UVs
			HAPI_AttributeInfo AttributeInfoVertex;
			AttributeInfoVertex.count = StaticMeshUVCount;
			AttributeInfoVertex.tupleSize = 2;
			AttributeInfoVertex.exists = true;
			AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(ConnectedAssetId, 0, 0, UVAttributeNameString,
				&AttributeInfoVertex), false);
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(ConnectedAssetId, 0, 0, UVAttributeNameString,
				&AttributeInfoVertex, (const float*) StaticMeshUVs.GetData(), 0, AttributeInfoVertex.count), false);
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
		AttributeInfoVertex.count = ChangedNormals.Num();
		AttributeInfoVertex.tupleSize = 3;
		AttributeInfoVertex.exists = true;
		AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
		AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(ConnectedAssetId, 0, 0, HAPI_ATTRIB_NORMAL,
			&AttributeInfoVertex), false);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(ConnectedAssetId, 0, 0, HAPI_ATTRIB_NORMAL,
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
		AttributeInfoVertex.count = ChangedColors.Num();
		AttributeInfoVertex.tupleSize = 4;
		AttributeInfoVertex.exists = true;
		AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
		AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(ConnectedAssetId, 0, 0, HAPI_ATTRIB_COLOR,
			&AttributeInfoVertex), false);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(ConnectedAssetId, 0, 0, HAPI_ATTRIB_COLOR,
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
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetVertexList(ConnectedAssetId, 0, 0, StaticMeshIndices.GetData(), 0,
			StaticMeshIndices.Num()), false);

		// We need to generate array of face counts.
		TArray<int32> StaticMeshFaceCounts;
		StaticMeshFaceCounts.Init(3, Part.faceCount);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetFaceCounts(ConnectedAssetId, 0, 0, StaticMeshFaceCounts.GetData(), 0,
			StaticMeshFaceCounts.Num()), false);
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
		AttributeInfoMaterial.count = RawMesh.FaceMaterialIndices.Num();
		AttributeInfoMaterial.tupleSize = 1;
		AttributeInfoMaterial.exists = true;
		AttributeInfoMaterial.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoMaterial.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoMaterial.originalOwner = HAPI_ATTROWNER_INVALID;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(ConnectedAssetId, 0, 0, MarshallingAttributeName.c_str(),
			&AttributeInfoMaterial), false);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeStringData(ConnectedAssetId, 0, 0,
			MarshallingAttributeName.c_str(), &AttributeInfoMaterial, (const char**) StaticMeshFaceMaterials.GetData(),
			0, StaticMeshFaceMaterials.Num()), false);

		// Delete material names.
		FHoudiniEngineUtils::DeleteFaceMaterialArray(StaticMeshFaceMaterials);
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
		AttributeInfoSmoothingMasks.count = RawMesh.FaceSmoothingMasks.Num();
		AttributeInfoSmoothingMasks.tupleSize = 1;
		AttributeInfoSmoothingMasks.exists = true;
		AttributeInfoSmoothingMasks.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoSmoothingMasks.storage = HAPI_STORAGETYPE_INT;
		AttributeInfoSmoothingMasks.originalOwner = HAPI_ATTROWNER_INVALID;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(ConnectedAssetId, 0, 0, MarshallingAttributeName.c_str(),
			&AttributeInfoSmoothingMasks), false);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntData(ConnectedAssetId, 0, 0,
			MarshallingAttributeName.c_str(), &AttributeInfoSmoothingMasks,
			(const int32*) RawMesh.FaceSmoothingMasks.GetData(), 0, RawMesh.FaceSmoothingMasks.Num()), false);
	}

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(ConnectedAssetId, 0, 0), false);

	// Now we can connect assets together.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectAssetGeometry(ConnectedAssetId, 0, HostAssetId, InputIndex), false);

#endif

	return true;
}


bool
FHoudiniEngineUtils::HapiDisconnectAsset(HAPI_AssetId HostAssetId, int32 InputIndex)
{
#if WITH_EDITOR

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::DisconnectAssetGeometry(HostAssetId, InputIndex), false);

#endif

	return true;
}


bool
FHoudiniEngineUtils::HapiConnectAsset(
	HAPI_AssetId AssetIdFrom, HAPI_ObjectId ObjectIdFrom, HAPI_AssetId AssetIdTo, int32 InputIndex)
{
#if WITH_EDITOR

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectAssetGeometry(AssetIdFrom, ObjectIdFrom, AssetIdTo, InputIndex),
		false);

#endif

	return true;
}


UPackage*
FHoudiniEngineUtils::BakeCreateStaticMeshPackageForComponent(UHoudiniAssetComponent* HoudiniAssetComponent,
	const FHoudiniGeoPartObject& HoudiniGeoPartObject, UPackage* Package, FString& MeshName, FGuid& BakeGUID, bool bBake)
{
	UPackage* PackageNew = nullptr;

#if WITH_EDITOR

	FString PackageName;
	int32 BakeCount = 0;
	UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;

	while(true)
	{
		if(!BakeGUID.IsValid())
		{
			BakeGUID = FGuid::NewGuid();
		}

		// We only want half of generated guid string.
		FString BakeGUIDString = BakeGUID.ToString().Left(8);

		if(bBake)
		{
			MeshName = HoudiniAsset->GetName() + FString::Printf(TEXT("_bake%d_"), BakeCount) +
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
			MeshName = HoudiniAsset->GetName() + TEXT("_") +
				FString::FromInt(HoudiniGeoPartObject.ObjectId) + TEXT("_") +
				FString::FromInt(HoudiniGeoPartObject.GeoId) + TEXT("_") +
				FString::FromInt(HoudiniGeoPartObject.PartId) + TEXT("_") +
				FString::FromInt(HoudiniGeoPartObject.SplitId) + TEXT("_") +
				HoudiniGeoPartObject.SplitName + TEXT("_") +
				BakeGUIDString;

			PackageName = FPackageName::GetLongPackagePath(HoudiniAsset->GetOuter()->GetName()) +
				TEXT("/") +
				HoudiniAsset->GetName() +
				TEXT("/") +
				MeshName;
		}

		PackageName = PackageTools::SanitizePackageName(PackageName);

		// See if package exists, if it does, we need to regenerate the name.
		Package = FindPackage(nullptr, *PackageName);

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
			PackageNew = CreatePackage(nullptr, *PackageName);
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
		FString BakeGUIDString = BakeGUID.ToString().Left(8);

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
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(AssetId, &AssetInfo), false);

	// Retrieve asset transform.
	HAPI_TransformEuler AssetEulerTransform;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetTransform(AssetId, HAPI_SRT, HAPI_XYZ, &AssetEulerTransform),
		false);

	// Convert HAPI Euler transform to Unreal one.
	FTransform AssetUnrealTransform;
	TranslateHapiTransform(AssetEulerTransform, AssetUnrealTransform);
	ComponentTransform = AssetUnrealTransform;

	// Retrieve information about each object contained within our asset.
	TArray<HAPI_ObjectInfo> ObjectInfos;
	ObjectInfos.SetNumUninitialized(AssetInfo.objectCount);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetObjects(AssetId, &ObjectInfos[0], 0, AssetInfo.objectCount), false);

	// Retrieve transforms for each object in this asset.
	TArray<HAPI_Transform> ObjectTransforms;
	ObjectTransforms.SetNumUninitialized(AssetInfo.objectCount);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetObjectTransforms(AssetId, HAPI_SRT, &ObjectTransforms[0], 0,
		AssetInfo.objectCount), false);

	// Containers used for raw data extraction.
	TArray<int32> VertexList;
	TArray<float> Positions;
	TArray<float> TextureCoordinates[MAX_STATIC_TEXCOORDS];
	TArray<float> Normals;
	TArray<float> Colors;
	TArray<FString> FaceMaterials;
	TArray<int32> FaceSmoothingMasks;

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
			if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetGeoInfo(AssetId, ObjectInfo.id, GeoIdx, &GeoInfo))
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

				if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetPartInfo(AssetId, ObjectInfo.id, GeoInfo.id, PartIdx,
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

				// Retrieve material information.
				HAPI_MaterialInfo MaterialInfo;
				bool bMaterialFound = false;
				if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetMaterialOnPart(AssetId, ObjectInfo.id, GeoInfo.id,
					PartInfo.id, &MaterialInfo))
				{
					if(MaterialInfo.exists)
					{
						bMaterialFound = true;
					}
				}

				// Retrieve part name.
				FHoudiniEngineUtils::GetHoudiniString(PartInfo.nameSH, PartName);

				// Create geo part object identifier.
				FHoudiniGeoPartObject HoudiniGeoPartObject(TransformMatrix, ObjectName, PartName, AssetId,
					ObjectInfo.id, GeoInfo.id, PartInfo.id);

				HoudiniGeoPartObject.bIsVisible = ObjectInfo.isVisible;
				HoudiniGeoPartObject.bIsInstancer = ObjectInfo.isInstancer;
				HoudiniGeoPartObject.bIsCurve = PartInfo.isCurve;
				HoudiniGeoPartObject.bIsEditable = GeoInfo.isEditable;
				HoudiniGeoPartObject.bHasGeoChanged = GeoInfo.hasGeoChanged;

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
				else if(PartInfo.isCurve)
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

				if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetVertexList(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
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
				TMap<FString, int32 > GroupSplitFaceCounts;
				int32 GroupVertexListCount = 0;
				static const FString RemainingGroupName = TEXT(HAPI_UNREAL_GROUP_GEOMETRY_NOT_COLLISION);

				if(bIsRenderCollidable || bIsCollidable)
				{
					// Set holding all vertex indices used for collision. We need this to figure out all vertex
					// indices that are not part of collision geos.
					TArray<int32> AllCollisionVertexList;
					AllCollisionVertexList.SetNumZeroed(VertexList.Num());

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

							// Extract vertex indices for this split.
							GroupVertexListCount = FHoudiniEngineUtils::HapiGetVertexListForGroup(AssetId,
								ObjectInfo.id, GeoInfo.id, PartInfo.id, GroupName, VertexList, GroupVertexList,
								AllCollisionVertexList);

							if(GroupVertexListCount > 0)
							{
								// If list is not empty, we store it for this group - this will define new mesh.
								GroupSplitFaces.Add(GroupName, GroupVertexList);
								GroupSplitFaceCounts.Add(GroupName, GroupVertexListCount);
							}
						}
					}

					// We also need to figure out / construct vertex list for everything that's not collision geometry
					// or rendered collision geometry.
					TArray<int32> GroupSplitFacesRemaining;
					GroupSplitFacesRemaining.Init(-1, VertexList.Num());
					bool bMainSplitGroup = false;
					GroupVertexListCount = 0;

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

					// We store remaining geo vertex list as a special name.
					if(bMainSplitGroup)
					{
						GroupSplitFaces.Add(RemainingGroupName, GroupSplitFacesRemaining);
						GroupSplitFaceCounts.Add(RemainingGroupName, GroupVertexListCount);
					}
				}
				else
				{
					GroupSplitFaces.Add(RemainingGroupName, VertexList);
					GroupSplitFaceCounts.Add(RemainingGroupName, VertexList.Num());
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

					// See if geometry has changed for this part (and global scaling has not changed).
					if(!GeoInfo.hasGeoChanged && !HoudiniAssetComponent->CheckGlobalSettingScaleFactors())
					{
						// If geometry has not changed.
						if(FoundStaticMesh)
						{
							StaticMesh = *FoundStaticMesh;

							// If there's material and material has changed.
							if(bMaterialFound && MaterialInfo.hasChanged)
							{
								// Also make sure material has not been replaced by Unreal material.
								if(!HoudiniGeoPartObject.bHasUnrealMaterialAssigned)
								{
									// Grab current source model and load existing raw model. This will be empty as we are
									// constructing a new mesh.
									FStaticMeshSourceModel* SrcModel = &StaticMesh->SourceModels[0];
									FRawMesh RawMesh;
									SrcModel->RawMeshBulkData->LoadRawMesh(RawMesh);

									// Even though geometry did not change, material requires update.
									MeshName = StaticMesh->GetName();
									UMaterial* Material = FHoudiniEngineUtils::HapiCreateMaterial(MaterialInfo, Package,
										MeshName, RawMesh);

									// Flag that we have Houdini material.
									HoudiniGeoPartObject.bHasNativeHoudiniMaterial = true;

									// Remove previous materials.
									StaticMesh->Materials.Empty();
									StaticMesh->Materials.Add(Material);
								}
							}
							else
							{
								// Material hasn't changed, we do not need to change anything.
								check(StaticMesh->Materials.Num() > 0);
							}

							// We can reuse previously created geometry.
							StaticMeshesOut.Add(HoudiniGeoPartObject, StaticMesh);
							continue;
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

					// If static mesh was not located, we need to create one.
					bool bStaticMeshCreated = false;
					if(!FoundStaticMesh)
					{
						MeshGuid.Invalidate();

						UPackage* MeshPackage = FHoudiniEngineUtils::BakeCreateStaticMeshPackageForComponent(HoudiniAssetComponent,
							HoudiniGeoPartObject, nullptr, MeshName, MeshGuid);
						StaticMesh = NewObject<UStaticMesh>(MeshPackage, FName(*MeshName), RF_Standalone);
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

					// Retrieve position data.
					HAPI_AttributeInfo AttribInfoPositions;
					if(!FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
						HAPI_ATTRIB_POSITION, AttribInfoPositions, Positions))
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

					// Get name of attribute used for marshalling materials.
					HAPI_AttributeInfo AttribFaceMaterials;

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
					HAPI_AttributeInfo AttribInfoColors;
					FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
						HAPI_ATTRIB_COLOR, AttribInfoColors, Colors);

					// See if we need to transfer color point attributes to vertex attributes.
					FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(SplitGroupVertexList, AttribInfoColors, Colors);

					// Retrieve normal data.
					HAPI_AttributeInfo AttribInfoNormals;
					FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
						HAPI_ATTRIB_NORMAL, AttribInfoNormals, Normals);

					// Retrieve face smoothing data.
					HAPI_AttributeInfo AttribInfoFaceSmoothingMasks;

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
					HAPI_AttributeInfo AttribInfoUVs[MAX_STATIC_TEXCOORDS];
					for(int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx)
					{
						std::string UVAttributeName = HAPI_ATTRIB_UV;

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

					// Compute number of faces.
					int32 FaceCount = SplitGroupVertexListCount / 3;

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
							RawMesh.WedgeColors[WedgeColorIdx] = FColor(WedgeColor);
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

					// Set face specific information and materials.
					RawMesh.FaceMaterialIndices.SetNumZeroed(FaceCount);

					if(bMaterialFound)
					{
						if(MaterialInfo.hasChanged || HoudiniGeoPartObject.bNativeHoudiniMaterialRefetch ||
							(!MaterialInfo.hasChanged && (0 == StaticMesh->Materials.Num())))
						{
							// If material has not been replaced by Unreal material.
							if(!HoudiniGeoPartObject.bHasUnrealMaterialAssigned)
							{
								// Material requires update.
								MeshName = StaticMesh->GetName();
								UMaterial* Material = FHoudiniEngineUtils::HapiCreateMaterial(MaterialInfo, Package, MeshName,
									RawMesh);

								// Flag that we have Houdini material.
								HoudiniGeoPartObject.bHasNativeHoudiniMaterial = true;

								// Remove previous materials.
								StaticMesh->Materials.Empty();
								StaticMesh->Materials.Add(Material);

								// Reset refetch flag.
								HoudiniGeoPartObject.bNativeHoudiniMaterialRefetch = false;
							}
						}
					}
					else
					{
						if(FaceMaterials.Num())
						{
							// We regenerate materials.
							StaticMesh->Materials.Empty();

							TSet<FString> UniqueFaceMaterials(FaceMaterials);
							TMap<FString, int32> UniqueFaceMaterialMap;

							int32 UniqueFaceMaterialsIdx = 0;
							for(TSet<FString>::TIterator Iter = UniqueFaceMaterials.CreateIterator(); Iter; ++Iter)
							{
								const FString& MaterialName = *Iter;
								UniqueFaceMaterialMap.Add(MaterialName, UniqueFaceMaterialsIdx);

								// Attempt to load this material.
								UMaterialInterface* MaterialInterface =
									Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(),
										nullptr, *MaterialName, nullptr, LOAD_NoWarn, nullptr));

								if(!MaterialInterface)
								{
									// Material does not exist, use default material.
									MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
								}

								StaticMesh->Materials.Add(MaterialInterface);
								UniqueFaceMaterialsIdx++;
							}

							int32 ValidFaceIdx = 0;
							for(int32 VertexIdx = 0; VertexIdx < SplitGroupVertexList.Num(); VertexIdx += 3)
							{
								int32 WedgeCheck = SplitGroupVertexList[VertexIdx + 0];
								if(-1 == WedgeCheck)
								{
									continue;
								}

								const FString& MaterialName = FaceMaterials[VertexIdx / 3];
								RawMesh.FaceMaterialIndices[ValidFaceIdx] = UniqueFaceMaterialMap[MaterialName];
								ValidFaceIdx++;
							}
						}
						else
						{
							if(0 == StaticMesh->Materials.Num())
							{
								UMaterial* Material = nullptr;

								if(RawMesh.WedgeColors.Num() > 0 && !HoudiniGeoPartObject.bHasUnrealMaterialAssigned)
								{
									// We have colors.
									MeshName = StaticMesh->GetName();
									Material = FHoudiniEngineUtils::HapiCreateMaterial(MaterialInfo, Package, MeshName,
										RawMesh);

									// Flag that we have Houdini material.
									HoudiniGeoPartObject.bHasNativeHoudiniMaterial = true;
								}
								else
								{
									// We just use default material if we do not have any.
									Material = UMaterial::GetDefaultMaterial(MD_Surface);
								}

								StaticMesh->Materials.Add(Material);
							}

							// Otherwise reuse materials from previous mesh.
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

					// See if we need to enable collisions.
					if(HoudiniGeoPartObject.IsCollidable() || HoudiniGeoPartObject.IsRenderCollidable())
					{
						UBodySetup* BodySetup = StaticMesh->BodySetup;
						check(BodySetup);

						// Enable collisions for this static mesh.
						BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
					}

					//StaticMesh->PreEditChange(nullptr);
					FHoudiniScopedGlobalSilence ScopedGlobalSilence;
					StaticMesh->Build(true);
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


void
FHoudiniEngineUtils::Serialize(UMaterialInterface*& MaterialInterface, UPackage* Package, FArchive& Ar)
{
	FString MaterialName;

	if(Ar.IsSaving())
	{
		MaterialName = MaterialInterface->GetName();
	}

	Ar << MaterialName;

	if(Ar.IsLoading())
	{
		MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
	}
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
		BakeCreateStaticMeshPackageForComponent(HoudiniAssetComponent, HoudiniGeoPartObject, nullptr, MeshName, BakeGUID, true);

	// Create static mesh.
	StaticMesh = NewObject<UStaticMesh>(Package, FName(*MeshName), RF_Standalone | RF_Public);
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

	UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;

	// Create package for our Blueprint.
	FString BlueprintName = TEXT("");
	UPackage* Package = 
		FHoudiniEngineUtils::BakeCreateBlueprintPackageForComponent(HoudiniAssetComponent, BlueprintName);

	AActor* Actor = HoudiniAssetComponent->CloneComponentsAndCreateActor();
	Blueprint = FKismetEditorUtilities::CreateBlueprintFromActor(*BlueprintName, Package, Actor, false);

	Actor->RemoveFromRoot();
	Actor->ConditionalBeginDestroy();

#endif

	return Blueprint;
}


UMaterial*
FHoudiniEngineUtils::HapiCreateMaterial(
	const HAPI_MaterialInfo& MaterialInfo, UPackage* Package, const FString& MeshName, const FRawMesh& RawMesh)
{
	//return UMaterial::GetDefaultMaterial(MD_Surface);
	UMaterial* Material = nullptr;

#if WITH_EDITOR

	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	// Get node information.
	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::GetNodeInfo(MaterialInfo.nodeId, &NodeInfo);

	// Get node parameters.
	std::vector<HAPI_ParmInfo> NodeParams;
	NodeParams.resize(NodeInfo.parmCount);
	FHoudiniApi::GetParameters(NodeInfo.id, &NodeParams[0], 0, NodeInfo.parmCount);

	// Get names of parameters.
	std::vector<std::string> NodeParamNames;
	NodeParamNames.resize(NodeInfo.parmCount);
	FHoudiniEngineUtils::HapiRetrieveParameterNames(NodeParams, NodeParamNames);

	// See if texture is available.
	int32 ParmNameIdx = FHoudiniEngineUtils::HapiFindParameterByName("ogl_tex1", NodeParamNames);

	if(-1 == ParmNameIdx)
	{
		ParmNameIdx = FHoudiniEngineUtils::HapiFindParameterByName("baseColorMap", NodeParamNames);
	}

	if(-1 == ParmNameIdx)
	{
		ParmNameIdx = FHoudiniEngineUtils::HapiFindParameterByName("map", NodeParamNames);
	}

	// Update context for generated materials (will trigger when object goes out of scope).
	FMaterialUpdateContext MaterialUpdateContext;

	UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
	FString MaterialName = FString::Printf(TEXT("%s_%s"), *MeshName, HAPI_UNREAL_GENERATED_MATERIAL_SUFFIX);
	Material = (UMaterial*) MaterialFactory->FactoryCreateNew(UMaterial::StaticClass(), Package, *MaterialName,
		RF_Public, NULL, GWarn);

	if(ParmNameIdx >= 0)
	{
		TArray<char> ImageBuffer;

		// Retrieve color data.
		if(FHoudiniEngineUtils::HapiExtractImage(NodeParams[ParmNameIdx].id, MaterialInfo, ImageBuffer,
			HAPI_UNREAL_MATERIAL_TEXTURE_MAIN))
		{
			HAPI_ImageInfo ImageInfo;
			Result = FHoudiniApi::GetImageInfo(MaterialInfo.assetId, MaterialInfo.id, &ImageInfo);

			if(ImageInfo.xRes > 0 && ImageInfo.yRes > 0)
			{
				// Create texture.
				FString TextureName = FString::Printf(TEXT("%s_diffuse_texture"), *MeshName);
				UTexture2D* Texture = FHoudiniEngineUtils::CreateUnrealTexture(ImageInfo, Package, TextureName,
					PF_R8G8B8A8, ImageBuffer);

				// Create sampling expression and add it to material.
				UMaterialExpressionTextureSample* Expression =
					NewObject<UMaterialExpressionTextureSample>(Material,
						UMaterialExpressionTextureSample::StaticClass());

				Expression->Texture = Texture;
				Expression->SamplerType = SAMPLERTYPE_Color;

				Material->Expressions.Add(Expression);
				Material->BaseColor.Expression = Expression;

				if(FHoudiniEngineUtils::HapiIsMaterialTransparent(MaterialInfo))
				{
					// This material contains transparency.
					Material->BlendMode = BLEND_Masked;

					TArray<FExpressionOutput> Outputs = Expression->GetOutputs();
					FExpressionOutput* Output = Outputs.GetData();

					Material->OpacityMask.Expression = Expression;
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
			}
			else
			{
				// This is illegal case but it needs to be handled.
				check(false);
			}
		}
		else
		{
			// If texture extraction failed and we have vertex colors.
			if(RawMesh.WedgeColors.Num() > 0)
			{
				UMaterialExpressionVertexColor* Expression =
					NewObject<UMaterialExpressionVertexColor>(Material,
						UMaterialExpressionVertexColor::StaticClass());

				Material->Expressions.Add(Expression);
				Material->BaseColor.Expression = Expression;
			}

			// Material is opaque.
			Material->BlendMode = BLEND_Opaque;
		}
	}
	else
	{
		if(RawMesh.WedgeColors.Num() > 0)
		{
			UMaterialExpressionVertexColor* Expression =
				NewObject<UMaterialExpressionVertexColor>(Material,
					UMaterialExpressionVertexColor::StaticClass());

			Material->Expressions.Add(Expression);
			Material->BaseColor.Expression = Expression;
		}

		// Material is opaque.
		Material->BlendMode = BLEND_Opaque;
	}

	// Set other material properties.
	Material->TwoSided = true;
	Material->SetShadingModel(MSM_DefaultLit);

	// Propagate and trigger material updates.
	Material->PreEditChange(nullptr);
	Material->PostEditChange();
	Material->MarkPackageDirty();

	// Schedule this material for update.
	MaterialUpdateContext.AddMaterial(Material);

#endif

	return Material;
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
				MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
			}

			UniqueName = FHoudiniEngineUtils::ExtractMaterialName(MaterialInterface);
			UniqueMaterialList.Add(UniqueName);
		}
	}
	else
	{
		// We do not have any materials, add default.
		MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
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

		Position /= GeneratedGeometryScaleFactor;

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

		TEXT("libHAPI.dll");

#elif PLATFORM_MAC

		TEXT("libHAPI.dylib");

#elif PLATFORM_LINUX

		TEXT("libHAPI.so");

#else

		TEXT("");

#endif

	return LibHAPIName;
}


void*
FHoudiniEngineUtils::LoadLibHAPI(FString& StoredLibHAPILocation)
{
	void* HAPILibraryHandle = nullptr;

	// Before doing anything platform specific, check if HFS environment variable is defined.
	TCHAR HFS_ENV_VARIABLE[MAX_PATH];
	FPlatformMisc::GetEnvironmentVariable(TEXT("HFS"), HFS_ENV_VARIABLE, MAX_PATH);
	FString HFSPath = HFS_ENV_VARIABLE;

	// Get platform specific name of libHAPI.
	FString LibHAPIName = FHoudiniEngineUtils::HoudiniGetLibHAPIName();

	// We have HFS environment variable defined, attempt to load libHAPI from it.
	if(!HFSPath.IsEmpty())
	{

#if PLATFORM_WINDOWS

		HFSPath += TEXT("/bin");

#elif PLATFORM_MAC || PLATFORM_LINUX

		HFSPath += TEXT("/dsolib");

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

	// Otherwise, we will attempt to detect Houdini installation.
#if PLATFORM_WINDOWS

	// On Windows, we have also hardcoded HFS path in plugin configuration file; attempt to load from it.
	HFSPath = HOUDINI_ENGINE_HFS_PATH;

	if(!HFSPath.IsEmpty())
	{
		HFSPath += TEXT("/bin");

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

	// Otherwise on Windows, we try to look up location of Houdini Engine in the registry first.
	{
		FString HoudiniRegistryLocation =
			FString::Printf(
				TEXT("Software\\Side Effects Software\\Houdini Engine %d.%d.%d"),
				HAPI_VERSION_HOUDINI_MAJOR, HAPI_VERSION_HOUDINI_MINOR, HAPI_VERSION_HOUDINI_BUILD);

		FString HoudiniInstallationPath;

		if(FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *HoudiniRegistryLocation, TEXT("InstallPath"),
			HoudiniInstallationPath))
		{
			HoudiniInstallationPath += TEXT("/bin");

			// Create full path to libHAPI binary.
			FString LibHAPIPath = FString::Printf(TEXT("%s/%s"), *HoudiniInstallationPath, *LibHAPIName);

			if(FPaths::FileExists(LibHAPIPath))
			{
				FPlatformProcess::PushDllDirectory(*HoudiniInstallationPath);
				HAPILibraryHandle = FPlatformProcess::GetDllHandle(*LibHAPIName);
				FPlatformProcess::PopDllDirectory(*HoudiniInstallationPath);

				if(HAPILibraryHandle)
				{
					HOUDINI_LOG_MESSAGE(
						TEXT("Loaded %s from Registry path %s"), *LibHAPIName,
						*HoudiniInstallationPath);

					StoredLibHAPILocation = HoudiniInstallationPath;
					return HAPILibraryHandle;
				}
			}
		}
	}

	// If Houdini Engine was not found in the registry, attempt to locate installation of Houdini in the registry.
	{
		FString HoudiniRegistryLocation = FString::Printf(
			TEXT("Software\\Side Effects Software\\Houdini %d.%d.%d"),
			HAPI_VERSION_HOUDINI_MAJOR, HAPI_VERSION_HOUDINI_MINOR, HAPI_VERSION_HOUDINI_BUILD);

		FString HoudiniInstallationPath;

		if(FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *HoudiniRegistryLocation, TEXT("InstallPath"),
			HoudiniInstallationPath))
		{
			HoudiniInstallationPath += TEXT("/bin");

			// Create full path to libHAPI binary.
			FString LibHAPIPath = FString::Printf(TEXT("%s/%s"), *HoudiniInstallationPath, *LibHAPIName);

			if(FPaths::FileExists(LibHAPIPath))
			{
				FPlatformProcess::PushDllDirectory(*HoudiniInstallationPath);
				HAPILibraryHandle = FPlatformProcess::GetDllHandle(*LibHAPIName);
				FPlatformProcess::PopDllDirectory(*HoudiniInstallationPath);

				if(HAPILibraryHandle)
				{
					HOUDINI_LOG_MESSAGE(
						TEXT("Loaded %s from Registry path %s"), *LibHAPIName,
						*HoudiniInstallationPath);

					StoredLibHAPILocation = HoudiniInstallationPath;
					return HAPILibraryHandle;
				}
			}
		}
	}

#else

#	if PLATFORM_MAC

	// Attempt to load from standard Mac OS X installation.
	FString HoudiniLocation = FString::Printf(
		TEXT("/Library/Frameworks/Houdini.framework/Versions/%d.%d.%d/Libraries"),
		HAPI_VERSION_HOUDINI_MAJOR, HAPI_VERSION_HOUDINI_MINOR, HAPI_VERSION_HOUDINI_BUILD);

#	elif PLATFORM_LINUX

	// Attempt to load from standard Linux installation.
	FString HoudiniLocation = FString::Printf(
		TEXT("/opt/dev%d.%d.%d/dsolib"),
		HAPI_VERSION_HOUDINI_MAJOR, HAPI_VERSION_HOUDINI_MINOR, HAPI_VERSION_HOUDINI_BUILD);

#	endif

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

#endif

	StoredLibHAPILocation = TEXT("");
	return HAPILibraryHandle;
}


int32
FHoudiniEngineUtils::HapiGetVertexListForGroup(
	HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId, const FString& GroupName,
	const TArray<int32>& FullVertexList, TArray<int32>& NewVertexList, TArray<int32>& AllVertexList)
{
	NewVertexList.Init(-1, FullVertexList.Num());
	int32 ProcessedWedges = 0;

	TArray<int32> PartGroupMembership;
	FHoudiniEngineUtils::HapiGetGroupMembership(
		AssetId, ObjectId, GeoId, PartId, HAPI_GROUPTYPE_PRIM, GroupName, PartGroupMembership);

	// Go through all primitives.
	for(int32 FaceIdx = 0; FaceIdx < PartGroupMembership.Num(); ++FaceIdx)
	{
		if(PartGroupMembership[FaceIdx] > 0)
		{
			// This face is a member of specified group.
			NewVertexList[FaceIdx * 3 + 0] = FullVertexList[FaceIdx * 3 + 0];
			NewVertexList[FaceIdx * 3 + 1] = FullVertexList[FaceIdx * 3 + 1];
			NewVertexList[FaceIdx * 3 + 2] = FullVertexList[FaceIdx * 3 + 2];

			// Mark these vertex indices as used.
			AllVertexList[FaceIdx * 3 + 0] = 1;
			AllVertexList[FaceIdx * 3 + 1] = 1;
			AllVertexList[FaceIdx * 3 + 2] = 1;

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
	FHoudiniApi::GetStatusStringBufLength(
		status_type, verbosity, &StatusBufferLength);

	if(StatusBufferLength > 0)
	{
		TArray<char> StatusStringBuffer;
		StatusStringBuffer.SetNumZeroed(StatusBufferLength);
		FHoudiniApi::GetStatusString(status_type, &StatusStringBuffer[0], StatusBufferLength);

		return FString(UTF8_TO_TCHAR(&StatusStringBuffer[0]));
	}

	return FString(TEXT(""));
}


AHoudiniAssetActor*
FHoudiniEngineUtils::LocateClipboardActor()
{
	FString PasteString;
	FPlatformMisc::ClipboardPaste(PasteString);
	const TCHAR* Paste = *PasteString;

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
					HoudiniAssetActor = Cast<AHoudiniAssetActor>(StaticFindObject(AHoudiniAssetActor::StaticClass(), ANY_PACKAGE,
						*ActorName));
					break;
				}
			}
		}
	}

	return HoudiniAssetActor;
}
