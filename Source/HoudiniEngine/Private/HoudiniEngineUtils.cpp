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
#include "RawMesh.h"
#include "TargetPlatform.h"
#include "StaticMeshResources.h"


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
FHoudiniEngineUtils::ScaleFactorPosition = 75.0f;


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
	int StatusBufferLength = 0;
	HAPI_GetStatusStringBufLength(
		HAPI_STATUS_CALL_RESULT, HAPI_STATUSVERBOSITY_ERRORS, &StatusBufferLength);

	std::vector<char> StatusStringBuffer(StatusBufferLength, 0);
	HAPI_GetStatusString(HAPI_STATUS_CALL_RESULT, &StatusStringBuffer[0]);

	return FString(UTF8_TO_TCHAR(&StatusStringBuffer[0]));
}


bool
FHoudiniEngineUtils::IsInitialized()
{
	return(HAPI_RESULT_SUCCESS == HAPI_IsInitialized());
}


bool
FHoudiniEngineUtils::IsHoudiniAssetValid(HAPI_AssetId AssetId)
{
	HAPI_AssetInfo AssetInfo;
	int ValidationAnswer = 0;

	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAssetInfo(AssetId, &AssetInfo), false);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_IsAssetValid(AssetId, AssetInfo.validationId, &ValidationAnswer), false);

	return (0 != ValidationAnswer);
}


bool
FHoudiniEngineUtils::DestroyHoudiniAsset(HAPI_AssetId AssetId)
{
	return(HAPI_RESULT_SUCCESS == HAPI_DestroyAsset(AssetId));
}


bool
FHoudiniEngineUtils::GetHoudiniString(int Name, FString& NameString)
{
	std::string NamePlain;

	if(FHoudiniEngineUtils::HapiGetString(Name, NamePlain))
	{
		NameString = UTF8_TO_TCHAR(NamePlain.c_str());
		return true;
	}

	return false;
}


bool
FHoudiniEngineUtils::GetHoudiniAssetName(HAPI_AssetId AssetId, FString& NameString)
{
	HAPI_AssetInfo AssetInfo;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAssetInfo(AssetId, &AssetInfo), false);

	return(FHoudiniEngineUtils::GetHoudiniString(AssetInfo.nameSH, NameString));
}


int
FHoudiniEngineUtils::ConvertHoudiniColorRGB(const float* HoudiniColorRGB, FColor& UnrealColor)
{
	UnrealColor.B = FPlatformMath::FloorToInt(HoudiniColorRGB[2] == 1.0f ? 255 : HoudiniColorRGB[2] * 256.0f);
	UnrealColor.G = FPlatformMath::FloorToInt(HoudiniColorRGB[1] == 1.0f ? 255 : HoudiniColorRGB[1] * 256.0f);
	UnrealColor.R = FPlatformMath::FloorToInt(HoudiniColorRGB[0] == 1.0f ? 255 : HoudiniColorRGB[0] * 256.0f);
	UnrealColor.A = 255;

	return 3;
}


int
FHoudiniEngineUtils::ConvertHoudiniColorRGBA(const float* HoudiniColorRGBA, FColor& UnrealColor)
{
	UnrealColor.B = FPlatformMath::FloorToInt(HoudiniColorRGBA[2] == 1.0f ? 255 : HoudiniColorRGBA[2] * 256.0f);
	UnrealColor.G = FPlatformMath::FloorToInt(HoudiniColorRGBA[1] == 1.0f ? 255 : HoudiniColorRGBA[1] * 256.0f);
	UnrealColor.R = FPlatformMath::FloorToInt(HoudiniColorRGBA[0] == 1.0f ? 255 : HoudiniColorRGBA[0] * 256.0f);
	UnrealColor.A = FPlatformMath::FloorToInt(HoudiniColorRGBA[3] == 1.0f ? 255 : HoudiniColorRGBA[3] * 256.0f);

	return 4;
}


int
FHoudiniEngineUtils::ConvertUnrealColorRGB(const FColor& UnrealColor, float* HoudiniColorRGB)
{
	HoudiniColorRGB[0] = UnrealColor.R / 255.0f;
	HoudiniColorRGB[1] = UnrealColor.G / 255.0f;
	HoudiniColorRGB[2] = UnrealColor.B / 255.0f;

	return 3;
}


int
FHoudiniEngineUtils::ConvertUnrealColorRGBA(const FColor& UnrealColor, float* HoudiniColorRGBA)
{
	HoudiniColorRGBA[0] = UnrealColor.R / 255.0f;
	HoudiniColorRGBA[1] = UnrealColor.G / 255.0f;
	HoudiniColorRGBA[2] = UnrealColor.B / 255.0f;
	HoudiniColorRGBA[3] = UnrealColor.A / 255.0f;

	return 4;
}


bool
FHoudiniEngineUtils::HapiGetString(int Name, std::string& NameString)
{
	if(Name < 0)
	{
		return false;
	}

	// For now we will load first asset only.
	int NameLength = 0;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetStringBufLength(Name, &NameLength), false);

	if(NameLength)
	{
		std::vector<char> NameBuffer(NameLength, '\0');
		HOUDINI_CHECK_ERROR_RETURN(HAPI_GetString(Name, &NameBuffer[0], NameLength), false);

		// Create and return string.
		NameString = std::string(NameBuffer.begin(), NameBuffer.end());
	}

	return true;
}


int
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


int 
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
FHoudiniEngineUtils::HapiGetGroupNames(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_GroupType GroupType, 
									   std::vector<std::string>& GroupNames)
{
	HAPI_GeoInfo GeoInfo;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetGeoInfo(AssetId, ObjectId, GeoId, &GeoInfo), false);

	int GroupCount = FHoudiniEngineUtils::HapiGetGroupCountByType(GroupType, GeoInfo);	

	std::vector<int> GroupNameHandles(GroupCount, 0);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetGroupNames(AssetId, ObjectId, GeoId, GroupType, &GroupNameHandles[0], GroupCount), false);

	for(int NameIdx = 0; NameIdx < GroupCount; ++NameIdx)
	{
		std::string GroupName;
		if(FHoudiniEngineUtils::HapiGetString(GroupNameHandles[NameIdx], GroupName))
		{
			GroupNames.push_back(GroupName);
		}
	}

	return true;
}


bool
FHoudiniEngineUtils::HapiGetGroupMembership(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId, 
											HAPI_GroupType GroupType, std::string GroupName, std::vector<int>& GroupMembership)
{
	HAPI_PartInfo PartInfo;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetPartInfo(AssetId, ObjectId, GeoId, PartId, &PartInfo), false);

	int ElementCount = FHoudiniEngineUtils::HapiGetElementCountByGroupType(GroupType, PartInfo);

	GroupMembership.resize(ElementCount);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetGroupMembership(AssetId, ObjectId, GeoId, PartId, GroupType, GroupName.c_str(), 
													   &GroupMembership[0], 0, ElementCount), false);

	return true;
}


int
FHoudiniEngineUtils::HapiCheckGroupMembership(const std::vector<int>& GroupMembership)
{
	int GroupMembershipCount = 0;
	for(int Idx = 0; Idx < GroupMembership.size(); ++Idx)
	{
		if(GroupMembership[Idx] > 0)
		{
			++GroupMembershipCount;
		}
	}

	return GroupMembershipCount;
}


void
FHoudiniEngineUtils::HapiRetrieveParameterNames(const std::vector<HAPI_ParmInfo>& ParmInfos, std::vector<std::string>& Names)
{
	static const std::string InvalidParameterName("Invalid Parameter Name");

	Names.resize(ParmInfos.size());

	for(int ParmIdx = 0; ParmIdx < ParmInfos.size(); ++ParmIdx)
	{
		const HAPI_ParmInfo& NodeParmInfo = ParmInfos[ParmIdx];
		HAPI_StringHandle NodeParmHandle = NodeParmInfo.nameSH;

		int NodeParmNameLength = 0;
		HAPI_GetStringBufLength(NodeParmHandle, &NodeParmNameLength);

		if(NodeParmNameLength)
		{
			std::vector<char> NodeParmName(NodeParmNameLength, '\0');

			HAPI_Result Result = HAPI_GetString(NodeParmHandle, &NodeParmName[0], NodeParmNameLength);
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
FHoudiniEngineUtils::HapiCheckAttributeExists(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId,
											  HAPI_PartId PartId, const char* Name, HAPI_AttributeOwner Owner)
{
	HAPI_AttributeInfo AttribInfo;
	if(HAPI_RESULT_SUCCESS != HAPI_GetAttributeInfo(AssetId, ObjectId, GeoId, PartId, Name, Owner, &AttribInfo))
	{
		return false;
	}

	return AttribInfo.exists;
}


bool
FHoudiniEngineUtils::HapiCheckAttributeExists(const FHoudiniGeoPartObject& HoudiniGeoPartObject, const char* Name, HAPI_AttributeOwner Owner)
{
	return FHoudiniEngineUtils::HapiCheckAttributeExists(HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId,
														 HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId,
														 Name, Owner);
}


int
FHoudiniEngineUtils::HapiFindParameterByName(const std::string& ParmName, const std::vector<std::string>& Names)
{
	for(int Idx = 0; Idx < Names.size(); ++Idx)
	{
		if(!ParmName.compare(0, ParmName.length(), Names[Idx]))
		{
			return Idx;
		}
	}

	return -1;
}


bool
FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId,
												 const char* Name, HAPI_AttributeInfo& ResultAttributeInfo,
												 TArray<float>& Data, int TupleSize)
{
	ResultAttributeInfo.exists = false;

	// Reset container size.
	Data.SetNumUninitialized(0);

	int OriginalTupleSize = TupleSize;
	HAPI_AttributeInfo AttributeInfo;
	for(int AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx)
	{
		HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAttributeInfo(AssetId, ObjectId, GeoId, PartId, Name, (HAPI_AttributeOwner) AttrIdx, &AttributeInfo), false);

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

	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAttributeFloatData(AssetId, ObjectId, GeoId, PartId, Name, &AttributeInfo,
		&Data[0], 0, AttributeInfo.count), false);

	// Store the retrieved attribute information.
	ResultAttributeInfo = AttributeInfo;
	return true;
}


bool
FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(const FHoudiniGeoPartObject& HoudiniGeoPartObject, const char* Name,
											HAPI_AttributeInfo& ResultAttributeInfo, TArray<float>& Data, int TupleSize)
{
	return FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId,
															HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, Name,
															ResultAttributeInfo, Data, TupleSize);
}


bool
FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId,
											const char* Name, HAPI_AttributeInfo& ResultAttributeInfo,
											TArray<int>& Data, int TupleSize)
{
	ResultAttributeInfo.exists = false;

	// Reset container size.
	Data.SetNumUninitialized(0);

	int OriginalTupleSize = TupleSize;
	HAPI_AttributeInfo AttributeInfo;
	for(int AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx)
	{
		HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAttributeInfo(AssetId, ObjectId, GeoId, PartId, Name, (HAPI_AttributeOwner) AttrIdx, &AttributeInfo), false);

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

	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAttributeIntData(AssetId, ObjectId, GeoId, PartId, Name, &AttributeInfo,
		&Data[0], 0, AttributeInfo.count), false);

	// Store the retrieved attribute information.
	ResultAttributeInfo = AttributeInfo;
	return true;
}


bool
FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(const FHoudiniGeoPartObject& HoudiniGeoPartObject, const char* Name,
												   HAPI_AttributeInfo& ResultAttributeInfo, TArray<int>& Data, int TupleSize)
{

	return FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId,
															  HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, Name,
															  ResultAttributeInfo, Data, TupleSize);
}


bool
FHoudiniEngineUtils::HapiGetAttributeDataAsString(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId,
												  const char* Name, HAPI_AttributeInfo& ResultAttributeInfo,
												  TArray<FString>& Data, int TupleSize)
{
	ResultAttributeInfo.exists = false;

	// Reset container size.
	Data.Empty();

	int OriginalTupleSize = TupleSize;
	HAPI_AttributeInfo AttributeInfo;
	for(int AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx)
	{
		HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAttributeInfo(AssetId, ObjectId, GeoId, PartId, Name, (HAPI_AttributeOwner) AttrIdx, &AttributeInfo), false);

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
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAttributeStringData(AssetId, ObjectId, GeoId, PartId, Name, &AttributeInfo, &StringHandles[0], 0, AttributeInfo.count), false);

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
FHoudiniEngineUtils::HapiGetAttributeDataAsString(const FHoudiniGeoPartObject& HoudiniGeoPartObject, const char* Name,
												  HAPI_AttributeInfo& ResultAttributeInfo, TArray<FString>& Data, int TupleSize)
{
	return FHoudiniEngineUtils::HapiGetAttributeDataAsString(HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId,
															 HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, Name,
															 ResultAttributeInfo, Data, TupleSize);
}


bool
FHoudiniEngineUtils::HapiGetInstanceTransforms(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId, TArray<FTransform>& Transforms)
{
	Transforms.Empty();

	// Number of instances is number of points.
	HAPI_PartInfo PartInfo;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetPartInfo(AssetId, ObjectId, GeoId, PartId, &PartInfo), false);

	if(0 == PartInfo.pointCount)
	{
		return false;
	}

	TArray<HAPI_Transform> InstanceTransforms;
	InstanceTransforms.SetNumUninitialized(PartInfo.pointCount);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetInstanceTransforms(AssetId, ObjectId, GeoId, HAPI_SRT, &InstanceTransforms[0], 0, PartInfo.pointCount), false);

	for(int32 Idx = 0; Idx < PartInfo.pointCount; ++Idx)
	{
		const HAPI_Transform& HapiInstanceTransform = InstanceTransforms[Idx];

		FQuat Rotation(HapiInstanceTransform.rotationQuaternion[0], HapiInstanceTransform.rotationQuaternion[1],
					   HapiInstanceTransform.rotationQuaternion[2], HapiInstanceTransform.rotationQuaternion[3]);
		FVector Translation(HapiInstanceTransform.position[0] * ScaleFactorTranslate, HapiInstanceTransform.position[2] * ScaleFactorTranslate,
							HapiInstanceTransform.position[1] * ScaleFactorTranslate);
		FVector Scale3D(HapiInstanceTransform.scale[0], HapiInstanceTransform.scale[1], HapiInstanceTransform.scale[2]);

		Transforms.Add(FTransform(Rotation, Translation, Scale3D));
	}

	return true;
}


bool
FHoudiniEngineUtils::HapiGetInstanceTransforms(const FHoudiniGeoPartObject& HoudiniGeoPartObject, TArray<FTransform>& Transforms)
{
	return FHoudiniEngineUtils::HapiGetInstanceTransforms(HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId, HoudiniGeoPartObject.GeoId, 
														  HoudiniGeoPartObject.PartId, Transforms);
}


bool
FHoudiniEngineUtils::HapiExtractImage(HAPI_ParmId NodeParmId, const HAPI_MaterialInfo& MaterialInfo, std::vector<char>& ImageBuffer, const std::string Type)
{
	HAPI_Result Result = HAPI_RenderTextureToImage(MaterialInfo.assetId, MaterialInfo.id, NodeParmId);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		return false;
	}

	HAPI_ImageInfo ImageInfo;
	Result = HAPI_GetImageInfo(MaterialInfo.assetId, MaterialInfo.id, &ImageInfo);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		return false;
	}

	ImageInfo.dataFormat = HAPI_IMAGE_DATA_INT8;
	ImageInfo.interleaved = true;
	ImageInfo.packing = HAPI_IMAGE_PACKING_RGBA;

	Result = HAPI_SetImageInfo(MaterialInfo.assetId, MaterialInfo.id, &ImageInfo);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		return false;
	}

	int ImageBufferSize = 0;
	Result = HAPI_ExtractImageToMemory(MaterialInfo.assetId, MaterialInfo.id, HAPI_RAW_FORMAT_NAME, Type.c_str(), &ImageBufferSize);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		return false;
	}

	if(!ImageBufferSize)
	{
		return false;
	}

	ImageBuffer.resize(ImageBufferSize);
	Result = HAPI_GetImageMemoryBuffer(MaterialInfo.assetId, MaterialInfo.id, &ImageBuffer[0], ImageBufferSize);
	if(HAPI_RESULT_SUCCESS != Result)
	{
		return false;
	}

	return true;
}


UTexture2D*
FHoudiniEngineUtils::CreateUnrealTexture(const HAPI_ImageInfo& ImageInfo, EPixelFormat PixelFormat, const std::vector<char>& ImageBuffer)
{
	UTexture2D* Texture = UTexture2D::CreateTransient(ImageInfo.xRes, ImageInfo.yRes, PixelFormat);

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

	return Texture;
}


float
FHoudiniEngineUtils::HapiGetParameterDataAsFloat(HAPI_NodeId NodeId, const std::string ParmName, float DefaultValue)
{
	float Value = DefaultValue;

	HAPI_NodeInfo NodeInfo;
	HAPI_GetNodeInfo(NodeId, &NodeInfo);

	std::vector<HAPI_ParmInfo> NodeParams;
	NodeParams.resize(NodeInfo.parmCount);
	HAPI_GetParameters(NodeInfo.id, &NodeParams[0], 0, NodeInfo.parmCount);

	// Get names of parameters.
	std::vector<std::string> NodeParamNames;
	NodeParamNames.resize(NodeInfo.parmCount);
	FHoudiniEngineUtils::HapiRetrieveParameterNames(NodeParams, NodeParamNames);

	// See if parameter is present.
	int ParmNameIdx = FHoudiniEngineUtils::HapiFindParameterByName(ParmName, NodeParamNames);

	if(-1 != ParmNameIdx)
	{
		HAPI_ParmInfo& ParmInfo = NodeParams[ParmNameIdx];
		HAPI_Result Result = HAPI_GetParmFloatValues(NodeId, &Value, ParmInfo.floatValuesIndex, 1);
	}

	return Value;
}


bool
FHoudiniEngineUtils::HapiIsMaterialTransparent(const HAPI_MaterialInfo& MaterialInfo)
{
	float Alpha = FHoudiniEngineUtils::HapiGetParameterDataAsFloat(MaterialInfo.nodeId, "ogl_alpha", 1.0f);
	return Alpha < 0.95f;
}


bool
FHoudiniEngineUtils::IsValidAssetId(HAPI_AssetId AssetId)
{
	return -1 != AssetId;
}


bool
FHoudiniEngineUtils::HapiCreateAndConnectAsset(HAPI_AssetId HostAssetId, int InputIndex, UStaticMesh* StaticMesh, HAPI_AssetId& ConnectedAssetId)
{
	// We return invalid asset input id by default.
	ConnectedAssetId = -1;

	// If we don't have a static mesh, or host asset is invalid, there's nothing to do.
	if(!StaticMesh || !FHoudiniEngineUtils::IsHoudiniAssetValid(HostAssetId))
	{
		return false;
	}

	HAPI_AssetId AssetId = -1;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_CreateInputAsset(&AssetId, nullptr), false);

	// Check if we have a valid id for this new input asset.
	if(!FHoudiniEngineUtils::IsHoudiniAssetValid(AssetId))
	{
		return false;
	}

	// We now have a valid id.
	ConnectedAssetId = AssetId;

	HOUDINI_CHECK_ERROR_RETURN(HAPI_CookAsset(AssetId, nullptr), false);
	
	// Grab base LOD level.
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[0];

	// Load the existing raw mesh.
	FRawMesh RawMesh;
	SrcModel.RawMeshBulkData->LoadRawMesh(RawMesh);

	// Create part.
	HAPI_PartInfo Part = HAPI_PartInfo_Create();
	Part.vertexCount = RawMesh.WedgeIndices.Num();
	Part.faceCount =  RawMesh.WedgeIndices.Num() / 3;
	Part.pointCount = RawMesh.VertexPositions.Num();
	Part.isCurve = false;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_SetPartInfo(ConnectedAssetId, 0, 0, &Part), false);

	// Create point attribute info.
	HAPI_AttributeInfo AttributeInfoPoint = HAPI_AttributeInfo_Create();
	AttributeInfoPoint.count = RawMesh.VertexPositions.Num();
	AttributeInfoPoint.tupleSize = 3; 
	AttributeInfoPoint.exists = true;
	AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_AddAttribute(ConnectedAssetId, 0, 0, HAPI_ATTRIB_POSITION, &AttributeInfoPoint), false);

	// Extract vertices from static mesh.
	TArray<float> StaticMeshVertices;
	StaticMeshVertices.SetNumZeroed(RawMesh.VertexPositions.Num() * 3);
	for(int VertexIdx = 0; VertexIdx < RawMesh.VertexPositions.Num(); ++VertexIdx)
	{
		// Grab vertex at this index (we also need to swap Z and Y).
		const FVector& PositionVector = RawMesh.VertexPositions[VertexIdx];
		StaticMeshVertices[VertexIdx * 3 + 0] = PositionVector.X / FHoudiniEngineUtils::ScaleFactorPosition;
		StaticMeshVertices[VertexIdx * 3 + 1] = PositionVector.Z / FHoudiniEngineUtils::ScaleFactorPosition;
		StaticMeshVertices[VertexIdx * 3 + 2] = PositionVector.Y / FHoudiniEngineUtils::ScaleFactorPosition;
	}

	// Now that we have raw positions, we can upload them for our attribute.
	HOUDINI_CHECK_ERROR_RETURN(HAPI_SetAttributeFloatData(ConnectedAssetId, 0, 0, HAPI_ATTRIB_POSITION, &AttributeInfoPoint, 
														  StaticMeshVertices.GetData(), 0, AttributeInfoPoint.count), false);

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

			// We need to re-index UVs for wedges we swapped (due to winding differences).
			for(int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeIndices.Num(); WedgeIdx += 3)
			{
				// We do not touch wedge 0 of this triangle.

				FVector2D WedgeUV1 = StaticMeshUVs[WedgeIdx + 1];
				FVector2D WedgeUV2 = StaticMeshUVs[WedgeIdx + 2];

				StaticMeshUVs[WedgeIdx + 1] = WedgeUV2;
				StaticMeshUVs[WedgeIdx + 2] = WedgeUV1;
			}

			// Construct attribute name for this index.
			std::string UVAttributeName = HAPI_ATTRIB_UV;

			if(MeshTexCoordIdx > 0)
			{
				UVAttributeName += std::to_string(MeshTexCoordIdx + 1);
			}

			const char* UVAttributeNameString = UVAttributeName.c_str();

			// Create attribute for UVs
			HAPI_AttributeInfo AttributeInfoVertex = HAPI_AttributeInfo_Create();
			AttributeInfoVertex.count = StaticMeshUVCount;
			AttributeInfoVertex.tupleSize = 2; 
			AttributeInfoVertex.exists = true;
			AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			HOUDINI_CHECK_ERROR_RETURN(HAPI_AddAttribute(ConnectedAssetId, 0, 0, UVAttributeNameString, &AttributeInfoVertex), false);
			HOUDINI_CHECK_ERROR_RETURN(HAPI_SetAttributeFloatData(ConnectedAssetId, 0, 0, UVAttributeNameString, &AttributeInfoVertex, 
																  (float*) StaticMeshUVs.GetData(), 0, AttributeInfoVertex.count), false);
		}
	}

	// See if we have normals to upload.
	if(RawMesh.WedgeTangentZ.Num())
	{
		// Get raw normal data.
		FVector* RawMeshNormals = RawMesh.WedgeTangentZ.GetData();

		// Create attribute for normals.
		HAPI_AttributeInfo AttributeInfoVertex = HAPI_AttributeInfo_Create();
		AttributeInfoVertex.count = RawMesh.WedgeTangentZ.Num();
		AttributeInfoVertex.tupleSize = 3; 
		AttributeInfoVertex.exists = true;
		AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
		AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
		HOUDINI_CHECK_ERROR_RETURN(HAPI_AddAttribute(ConnectedAssetId, 0, 0, HAPI_ATTRIB_NORMAL, &AttributeInfoVertex), false);
		HOUDINI_CHECK_ERROR_RETURN(HAPI_SetAttributeFloatData(ConnectedAssetId, 0, 0, HAPI_ATTRIB_NORMAL, &AttributeInfoVertex, 
														  (float*) RawMeshNormals, 0, AttributeInfoVertex.count), false);
	}

	// Extract indices from static mesh.
	if(RawMesh.WedgeIndices.Num())
	{
		TArray<int> StaticMeshIndices;
		StaticMeshIndices.SetNumUninitialized(RawMesh.WedgeIndices.Num());
		for(int IndexIdx = 0; IndexIdx < RawMesh.WedgeIndices.Num(); IndexIdx += 3)
		{
			// Swap indices to fix winding order.
			StaticMeshIndices[IndexIdx + 0] = (RawMesh.WedgeIndices[IndexIdx + 0]);
			StaticMeshIndices[IndexIdx + 1] = (RawMesh.WedgeIndices[IndexIdx + 2]);
			StaticMeshIndices[IndexIdx + 2] = (RawMesh.WedgeIndices[IndexIdx + 1]);
		}

		// We can now set vertex list.
		HOUDINI_CHECK_ERROR_RETURN(HAPI_SetVertexList(ConnectedAssetId, 0, 0, StaticMeshIndices.GetData(), 0, StaticMeshIndices.Num()), false);

		// We need to generate array of face counts.
		TArray<int> StaticMeshFaceCounts;
		StaticMeshFaceCounts.Init(3, Part.faceCount);
		HOUDINI_CHECK_ERROR_RETURN(HAPI_SetFaceCounts(ConnectedAssetId, 0, 0, StaticMeshFaceCounts.GetData(), 0, StaticMeshFaceCounts.Num()), false);
	}

	// Marshall face material indices.
	if(RawMesh.FaceMaterialIndices.Num())
	{
		// Create list of materials, one for each face.
		TArray<char*> StaticMeshFaceMaterials;
		FHoudiniEngineUtils::CreateFaceMaterialArray(StaticMesh->Materials, RawMesh.FaceMaterialIndices, StaticMeshFaceMaterials);

		// Create attribute for materials.
		HAPI_AttributeInfo AttributeInfoMaterial = HAPI_AttributeInfo_Create();
		AttributeInfoMaterial.count = RawMesh.FaceMaterialIndices.Num();
		AttributeInfoMaterial.tupleSize = 1;
		AttributeInfoMaterial.exists = true;
		AttributeInfoMaterial.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoMaterial.storage = HAPI_STORAGETYPE_STRING;
		HOUDINI_CHECK_ERROR_RETURN(HAPI_AddAttribute(ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_MATERIAL, &AttributeInfoMaterial), false);
		HOUDINI_CHECK_ERROR_RETURN(HAPI_SetAttributeStringData(ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_MATERIAL, &AttributeInfoMaterial,
															   (const char**) StaticMeshFaceMaterials.GetData(), 0, StaticMeshFaceMaterials.Num()), false);

		// Delete material names.
		FHoudiniEngineUtils::DeleteFaceMaterialArray(StaticMeshFaceMaterials);
	}

	// Marshall face smoothing masks.
	if(RawMesh.FaceSmoothingMasks.Num())
	{
		HAPI_AttributeInfo AttributeInfoSmoothingMasks = HAPI_AttributeInfo_Create();
		AttributeInfoSmoothingMasks.count = RawMesh.FaceSmoothingMasks.Num();
		AttributeInfoSmoothingMasks.tupleSize = 1;
		AttributeInfoSmoothingMasks.exists = true;
		AttributeInfoSmoothingMasks.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoSmoothingMasks.storage = HAPI_STORAGETYPE_INT;
		HOUDINI_CHECK_ERROR_RETURN(HAPI_AddAttribute(ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK, &AttributeInfoSmoothingMasks), false);
		HOUDINI_CHECK_ERROR_RETURN(HAPI_SetAttributeIntData(ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK, &AttributeInfoSmoothingMasks,
															   (int*) RawMesh.FaceSmoothingMasks.GetData(), 0, RawMesh.FaceSmoothingMasks.Num()), false);
	}

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(HAPI_CommitGeo(ConnectedAssetId, 0, 0), false);

	// Now we can connect assets together.
	HOUDINI_CHECK_ERROR_RETURN(HAPI_ConnectAssetGeometry(ConnectedAssetId, 0, HostAssetId, InputIndex), false);

	return true;
}


bool
FHoudiniEngineUtils::HapiDisconnectAsset(HAPI_AssetId HostAssetId, int InputIndex)
{
	HOUDINI_CHECK_ERROR_RETURN(HAPI_DisconnectAssetGeometry(HostAssetId, InputIndex), false);
	return true;
}


UPackage*
FHoudiniEngineUtils::BakeCreatePackageForStaticMesh(UHoudiniAsset* HoudiniAsset, UPackage* Package, FString& MeshName, FGuid& BakeGUID, int32 ObjectIdx)
{
	FString PackageName;

	while(true)
	{
		if(!BakeGUID.IsValid())
		{
			BakeGUID = FGuid::NewGuid();
		}

		FString BakeGUIDString = BakeGUID.ToString();
		
		if(ObjectIdx != -1)
		{
			MeshName = HoudiniAsset->GetName() + TEXT("_") + FString::FromInt(ObjectIdx) + TEXT("_") + BakeGUIDString;
		}
		else
		{
			MeshName = HoudiniAsset->GetName() + TEXT("_") + BakeGUIDString;
		}

		if(!Package)
		{
			Package = HoudiniAsset->GetOutermost();
		}

		PackageName = FPackageName::GetLongPackagePath(Package->GetName()) + TEXT("/") + MeshName;
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

	return Package;
}


UStaticMesh*
FHoudiniEngineUtils::CreateStaticMeshHoudiniLogo()
{
	// Geometry scale factor.
	static const float ScaleFactor = 25.0f;
	static const FColor VertexColor(255, 165, 0);

	static const FString MeshName = TEXT("HoudiniLogo");
	static const FString HoudiniLogoStaticMeshPath(TEXT("/Engine/HoudiniLogo.HoudiniLogo"));

	UStaticMesh* StaticMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, *HoudiniLogoStaticMeshPath));
	if(StaticMesh)
	{
		return StaticMesh;
	}
	
	// Get platform manager LOD specific information.
	ITargetPlatform* CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	check(CurrentPlatform);
	const FStaticMeshLODGroup& LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(NAME_None);
	int32 NumLODs = LODGroup.GetDefaultNumLODs();

	FString PackageName = FPackageName::GetLongPackagePath(GetTransientPackage()->GetName()) + TEXT("/") + MeshName;
	PackageName = PackageTools::SanitizePackageName(PackageName);

	// See if package exists.
	UPackage* Package = FindPackage(nullptr, *PackageName);

	if(!Package)
	{
		// Create actual package.
		Package = CreatePackage(nullptr, *PackageName);
	}

	// Create new static mesh.
	StaticMesh = new(Package, FName(*MeshName), RF_Public) UStaticMesh(FPostConstructInitializeProperties());

	// Create one LOD level.
	new(StaticMesh->SourceModels) FStaticMeshSourceModel();

	// Grab base LOD level.
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[0];

	// Locate default material and add it to mesh.
	UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	StaticMesh->Materials.Add(DefaultMaterial);

	// Load the existing raw mesh.
	FRawMesh RawMesh;
	SrcModel.RawMeshBulkData->LoadRawMesh(RawMesh);
	int32 VertexOffset = RawMesh.VertexPositions.Num();
	int32 WedgeOffset = RawMesh.WedgeIndices.Num();
	int32 TriangleOffset = RawMesh.FaceMaterialIndices.Num();
	int32 WedgeCount = sizeof(FHoudiniLogo::VertexIndices) / sizeof(int);
	int32 VertexCount = sizeof(FHoudiniLogo::Vertices) / sizeof(float);

	// Make sure static mesh has a new lighting guid.
	StaticMesh->LightingGuid = FGuid::NewGuid();

	// Set it to use textured lightmaps.
	StaticMesh->LightMapResolution = 32;
	StaticMesh->LightMapCoordinateIndex = 0;

	// Calculate number of triangles.
	int32 TriangleCount = WedgeCount / 3;

	// Reserve space for attributes.
	RawMesh.FaceMaterialIndices.AddZeroed(TriangleCount);
	RawMesh.FaceSmoothingMasks.AddZeroed(TriangleCount);
	RawMesh.VertexPositions.Reserve(VertexCount);
	RawMesh.WedgeIndices.Reserve(WedgeCount);
	//RawMesh.WedgeColors.AddZeroed(WedgeCount);
	RawMesh.WedgeTexCoords[0].AddZeroed(WedgeCount);

	// Copy vertex positions.
	for(int VertexIdx = 0; VertexIdx < VertexCount; VertexIdx += 3)
	{
		FVector VertexPosition;
		VertexPosition.X = FHoudiniLogo::Vertices[VertexIdx + 0] * ScaleFactor;
		VertexPosition.Z = FHoudiniLogo::Vertices[VertexIdx + 1] * ScaleFactor;
		VertexPosition.Y = FHoudiniLogo::Vertices[VertexIdx + 2] * ScaleFactor;
		RawMesh.VertexPositions.Add(VertexPosition);
	}

	// Copy indices.
	for(int IndexIdx = 0; IndexIdx < WedgeCount; ++IndexIdx)
	{
		RawMesh.WedgeIndices.Add(FHoudiniLogo::VertexIndices[IndexIdx] - 1);
	}

	// Some mesh generation settings.
	SrcModel.BuildSettings.bRemoveDegenerates = true;
	SrcModel.BuildSettings.bRecomputeNormals = true;
	SrcModel.BuildSettings.bRecomputeTangents = true;

	// Store the new raw mesh.
	SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);

	while(StaticMesh->SourceModels.Num() < NumLODs)
	{
		new(StaticMesh->SourceModels) FStaticMeshSourceModel();
	}
	for(int32 ModelLODIndex = 0; ModelLODIndex < NumLODs; ++ModelLODIndex)
	{
		StaticMesh->SourceModels[ModelLODIndex].ReductionSettings = LODGroup.GetDefaultSettings(ModelLODIndex);
	}

	StaticMesh->LightMapResolution = LODGroup.GetDefaultLightMapResolution();

	// Build the static mesh - this will generate necessary data and create necessary rendering resources.
	StaticMesh->LODGroup = NAME_None;
	StaticMesh->Build(true);

	return StaticMesh;
}


bool
FHoudiniEngineUtils::CreateStaticMeshesFromHoudiniAsset(HAPI_AssetId AssetId, UHoudiniAsset* HoudiniAsset, UPackage* Package, 
												   const TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshesIn,
												   TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshesOut)
{
	if(!FHoudiniEngineUtils::IsHoudiniAssetValid(AssetId))
	{
		return false;
	}

	// Get platform manager LOD specific information.
	ITargetPlatform* CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	check(CurrentPlatform);
	const FStaticMeshLODGroup& LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(NAME_None);
	int32 NumLODs = LODGroup.GetDefaultNumLODs();

	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	HAPI_AssetInfo AssetInfo;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAssetInfo(AssetId, &AssetInfo), false);

	// Retrieve information about each object contained within our asset.
	TArray<HAPI_ObjectInfo> ObjectInfos;
	ObjectInfos.SetNumUninitialized(AssetInfo.objectCount);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetObjects(AssetId, &ObjectInfos[0], 0, AssetInfo.objectCount), false);

	// Retrieve transforms for each object in this asset.
	TArray<HAPI_Transform> ObjectTransforms;
	ObjectTransforms.SetNumUninitialized(AssetInfo.objectCount);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetObjectTransforms(AssetId, HAPI_SRT, &ObjectTransforms[0], 0, AssetInfo.objectCount), false);

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
	int32 MeshCounter = 0;

	// Iterate through all objects.
	for(int32 ObjectIdx = 0; ObjectIdx < ObjectInfos.Num(); ++ObjectIdx)
	{
		// Retrieve object at this index.
		const HAPI_ObjectInfo& ObjectInfo = ObjectInfos[ObjectIdx];

		// Retrieve object name.
		FString ObjectName;
		FHoudiniEngineUtils::GetHoudiniString(ObjectInfo.nameSH, ObjectName);

		// Convert HAPI transform to a matrix form.
		FMatrix TransformMatrix;
		HOUDINI_CHECK_ERROR_RETURN(HAPI_ConvertTransformQuatToMatrix(&ObjectTransforms[ObjectIdx], &TransformMatrix.M[0][0]), false);
		TransformMatrix.ScaleTranslation(FVector(FHoudiniEngineUtils::ScaleFactorTranslate, FHoudiniEngineUtils::ScaleFactorTranslate, 
												 FHoudiniEngineUtils::ScaleFactorTranslate));

		// We need to swap transforms for Z and Y.
		float TransformSwapY = TransformMatrix.M[3][1];
		float TransformSwapZ = TransformMatrix.M[3][2];
		TransformMatrix.M[3][1] = TransformSwapZ;
		TransformMatrix.M[3][2] = TransformSwapY;

		// Iterate through all Geo informations within this object.
		for(int32 GeoIdx = 0; GeoIdx < ObjectInfo.geoCount; ++GeoIdx)
		{
			// Get Geo information.
			HAPI_GeoInfo GeoInfo;
			if(HAPI_RESULT_SUCCESS != HAPI_GetGeoInfo(AssetId, ObjectInfo.id, GeoIdx, &GeoInfo))
			{
				continue;
			}

			// Right now only care about display SOPs.
			if(!GeoInfo.isDisplayGeo)
			{
				continue;
			}

			bool bGeoError = false;

			for(int32 PartIdx = 0; PartIdx < GeoInfo.partCount; ++PartIdx)
			{
				// Get part information.
				HAPI_PartInfo PartInfo;

				if(HAPI_RESULT_SUCCESS != HAPI_GetPartInfo(AssetId, ObjectInfo.id, GeoInfo.id, PartIdx, &PartInfo))
				{
					// Error retrieving part info.
					bGeoError = true;
					break;
				}

				// There are no vertices and no points.
				if(PartInfo.vertexCount <= 0 && PartInfo.pointCount <= 0)
				{
					bGeoError = true;
					break;
				}

				// Retrieve material information.
				HAPI_MaterialInfo MaterialInfo;
				bool bMaterialFound = false;
				if((HAPI_RESULT_SUCCESS == HAPI_GetMaterialOnPart(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id, &MaterialInfo)) && MaterialInfo.exists)
				{
					bMaterialFound = true;
				}

				// Retrieve part name.
				FString PartName;
				FHoudiniEngineUtils::GetHoudiniString(PartInfo.nameSH, PartName);

				// Create geo part object identifier.
				FHoudiniGeoPartObject HoudiniGeoPartObject(TransformMatrix, ObjectName, PartName, AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id, ObjectInfo.isVisible, ObjectInfo.isInstancer);

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
						bGeoError = true;
						break;
					}
				}
				else
				{
					if(PartInfo.vertexCount <= 0)
					{
						// This is not an instancer, but we do not have vertices, skip.
						bGeoError = true;
						break;
					}
				}

				// Attempt to locate static mesh from previous instantiation.
				UStaticMesh* const* FoundStaticMesh = StaticMeshesIn.Find(HoudiniGeoPartObject);

				// See if geometry has changed for this part.
				if(!GeoInfo.hasGeoChanged)
				{
					// If geometry has not changed.
					if(FoundStaticMesh)
					{
						StaticMesh = *FoundStaticMesh;
						if(bMaterialFound && MaterialInfo.hasChanged)
						{
							// Even though geometry did not change, material requires update.
							MeshName = StaticMesh->GetName();
							UMaterial* Material = FHoudiniEngineUtils::HapiCreateMaterial(MaterialInfo, Package, MeshName);

							// Remove previous materials.
							StaticMesh->Materials.Empty();
							StaticMesh->Materials.Add(Material);
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
						break;
					}
				}

				// If static mesh was not located, we need to create one.
				if(!FoundStaticMesh)
				{
					MeshGuid.Invalidate();

					UPackage* MeshPackage = FHoudiniEngineUtils::BakeCreatePackageForStaticMesh(HoudiniAsset, Package, MeshName, MeshGuid, MeshCounter);
					StaticMesh = new(MeshPackage, FName(*MeshName), RF_Public) UStaticMesh(FPostConstructInitializeProperties());
				}
				else
				{
					// If it was located, we will just reuse it.
					StaticMesh = *FoundStaticMesh;
				}

				// Make sure static mesh has a new lighting guid.
				StaticMesh->LightingGuid = FGuid::NewGuid();

				// Set it to use textured light maps. We use coordinate 1 for UVs.
				StaticMesh->LightMapResolution = 32;

				StaticMesh->LightMapResolution = LODGroup.GetDefaultLightMapResolution();
				StaticMesh->LODGroup = NAME_None;

				MeshCounter++;

				// Create new source model for current static mesh.
				if(!StaticMesh->SourceModels.Num())
				{
					new(StaticMesh->SourceModels) FStaticMeshSourceModel();
				}

				// Grab current source model.
				FStaticMeshSourceModel* SrcModel = &StaticMesh->SourceModels[0];

				// Load existing raw model. This will be empty as we are constructing a new mesh.
				FRawMesh RawMesh;
				SrcModel->RawMeshBulkData->LoadRawMesh(RawMesh);

				// Some mesh generation settings.
				SrcModel->BuildSettings.bRemoveDegenerates = true;
				SrcModel->BuildSettings.bRecomputeTangents = true;
				SrcModel->BuildSettings.bRecomputeNormals = true;

				// Retrieve vertex information for this part.
				VertexList.SetNumUninitialized(PartInfo.vertexCount);
				if(HAPI_RESULT_SUCCESS != HAPI_GetVertexList(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id, &VertexList[0], 0, PartInfo.vertexCount))
				{
					// Error getting the vertex list.
					bGeoError = true;
					break;
				}

				// Retrieve position data.
				HAPI_AttributeInfo AttribInfoPositions;
				if(!FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
					HAPI_ATTRIB_POSITION, AttribInfoPositions, Positions))
				{
					// Error retrieving positions.
					bGeoError = true;
					break;
				}

				HAPI_AttributeInfo AttribFaceMaterials;
				FHoudiniEngineUtils::HapiGetAttributeDataAsString(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
												  HAPI_UNREAL_ATTRIB_MATERIAL, AttribFaceMaterials, FaceMaterials);

				// Retrieve color data.
				HAPI_AttributeInfo AttribInfoColors;
				FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
					HAPI_ATTRIB_COLOR, AttribInfoColors, Colors);

				// See if we need to transfer color point attributes to vertex attributes.
				FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(VertexList, AttribInfoColors, Colors);

				// Retrieve normal data.
				HAPI_AttributeInfo AttribInfoNormals;
				FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
					HAPI_ATTRIB_NORMAL, AttribInfoNormals, Normals);

				// Retrieve face smoothing data.
				HAPI_AttributeInfo AttribInfoFaceSmoothingMasks;
				FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
					HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK, AttribInfoFaceSmoothingMasks, FaceSmoothingMasks);

				// See if we need to transfer normal point attributes to vertex attributes.
				FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(VertexList, AttribInfoNormals, Normals);

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
					FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id, UVAttributeNameString,
																	 AttribInfoUVs[TexCoordIdx], TextureCoordinates[TexCoordIdx], 2);

					// See if we need to transfer uv point attributes to vertex attributes.
					FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(VertexList, AttribInfoUVs[TexCoordIdx], TextureCoordinates[TexCoordIdx]);
				}

				// We can transfer attributes to raw mesh.

				// Compute number of faces.
				int32 FaceCount = PartInfo.vertexCount / 3;

				// Set face smoothing masks.
				RawMesh.FaceSmoothingMasks.SetNumZeroed(FaceCount);

				if(FaceSmoothingMasks.Num())
				{
					for(int32 FaceSmoothingMaskIdx = 0; FaceSmoothingMaskIdx < FaceSmoothingMasks.Num(); ++FaceSmoothingMaskIdx)
					{
						RawMesh.FaceSmoothingMasks[FaceSmoothingMaskIdx] = FaceSmoothingMasks[FaceSmoothingMaskIdx];
					}
				}

				// Set face specific information and materials.
				if(bMaterialFound)
				{
					if(MaterialInfo.hasChanged || (!MaterialInfo.hasChanged && (0 == StaticMesh->Materials.Num())))
					{
						// Material requires update.
						MeshName = StaticMesh->GetName();
						UMaterial* Material = FHoudiniEngineUtils::HapiCreateMaterial(MaterialInfo, Package, MeshName);

						// Remove previous materials.
						StaticMesh->Materials.Empty();
						StaticMesh->Materials.Add(Material);

						RawMesh.FaceMaterialIndices.SetNumZeroed(FaceCount);
					}
				}
				else
				{
					RawMesh.FaceMaterialIndices.SetNumZeroed(FaceCount);
					StaticMesh->Materials.Empty();

					if(FaceMaterials.Num())
					{
						TSet<FString> UniqueFaceMaterials(FaceMaterials);
						TMap<FString, int32> UniqueFaceMaterialMap;

						int32 UniqueFaceMaterialsIdx = 0;
						for(TSet<FString>::TIterator Iter = UniqueFaceMaterials.CreateIterator(); Iter; ++Iter)
						{
							const FString& MaterialName = *Iter;
							UniqueFaceMaterialMap.Add(MaterialName, UniqueFaceMaterialsIdx);

							// Attempt to load this material.
							UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialName, nullptr, LOAD_NoWarn, nullptr));
							
							if(!MaterialInterface)
							{
								// Material does not exist, use default material.
								MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
							}

							StaticMesh->Materials.Add(MaterialInterface);
							UniqueFaceMaterialsIdx++;
						}

						for(int32 FaceMaterialIdx = 0; FaceMaterialIdx < FaceMaterials.Num(); ++FaceMaterialIdx)
						{
							const FString& MaterialName = FaceMaterials[FaceMaterialIdx];
							RawMesh.FaceMaterialIndices[FaceMaterialIdx] = UniqueFaceMaterialMap[MaterialName];
						}
					}
					else
					{
						// We just use default material.
						UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
						StaticMesh->Materials.Add(DefaultMaterial);
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
						RawMesh.WedgeTexCoords[0].SetNumZeroed(VertexList.Num());
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
						StaticMesh->LightMapCoordinateIndex = 1;
						break;
					}
				}

				// Transfer indices.
				RawMesh.WedgeIndices.SetNumZeroed(VertexList.Num());
				for(int32 VertexIdx = 0; VertexIdx < VertexList.Num(); VertexIdx += 3)
				{
					int32 WedgeIndices[3] = { VertexList[VertexIdx + 0], VertexList[VertexIdx + 1], VertexList[VertexIdx + 2] };

					// Flip wedge indices to fix winding order.
					RawMesh.WedgeIndices[VertexIdx + 0] = WedgeIndices[0];
					RawMesh.WedgeIndices[VertexIdx + 1] = WedgeIndices[2];
					RawMesh.WedgeIndices[VertexIdx + 2] = WedgeIndices[1];

					// Check if we need to patch UVs.
					for(int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx)
					{
						if(RawMesh.WedgeTexCoords[TexCoordIdx].Num() > 0)
						{
							FVector2D WedgeUV1 = RawMesh.WedgeTexCoords[TexCoordIdx][VertexIdx + 1];
							FVector2D WedgeUV2 = RawMesh.WedgeTexCoords[TexCoordIdx][VertexIdx + 2];

							RawMesh.WedgeTexCoords[TexCoordIdx][VertexIdx + 1] = WedgeUV2;
							RawMesh.WedgeTexCoords[TexCoordIdx][VertexIdx + 2] = WedgeUV1;
						}
					}
				}

				// Transfer vertex positions.
				int32 VertexPositionsCount = Positions.Num() / 3;
				RawMesh.VertexPositions.SetNumZeroed(VertexPositionsCount);
				for(int32 VertexPositionIdx = 0; VertexPositionIdx < VertexPositionsCount; ++VertexPositionIdx)
				{
					FVector VertexPosition;
					VertexPosition.X = Positions[VertexPositionIdx * 3 + 0] * FHoudiniEngineUtils::ScaleFactorPosition;
					VertexPosition.Z = Positions[VertexPositionIdx * 3 + 1] * FHoudiniEngineUtils::ScaleFactorPosition;
					VertexPosition.Y = Positions[VertexPositionIdx * 3 + 2] * FHoudiniEngineUtils::ScaleFactorPosition;

					RawMesh.VertexPositions[VertexPositionIdx] = VertexPosition;
				}

				// We need to check if this mesh contains only degenerate triangles.
				if(FHoudiniEngineUtils::CountDegenerateTriangles(RawMesh) == FaceCount)
				{
					// This mesh contains only degenerate triangles, there's nothing we can do.
					StaticMesh->MarkPendingKill();
					continue;
				}

				// Transfer colors.
				if(AttribInfoColors.exists && AttribInfoColors.tupleSize)
				{
					int32 WedgeColorsCount = Colors.Num() / AttribInfoColors.tupleSize;
					RawMesh.WedgeColors.SetNumZeroed(WedgeColorsCount);
					for(int32 WedgeColorIdx = 0; WedgeColorIdx < WedgeColorsCount; ++WedgeColorIdx)
					{
						FLinearColor WedgeColor;

						WedgeColor.R = Colors[WedgeColorIdx * AttribInfoColors.tupleSize + 0];
						WedgeColor.G = Colors[WedgeColorIdx * AttribInfoColors.tupleSize + 1];
						WedgeColor.B = Colors[WedgeColorIdx * AttribInfoColors.tupleSize + 2];

						if(4 == AttribInfoColors.tupleSize)
						{
							// We have alpha.
							WedgeColor.A = Colors[WedgeColorIdx * AttribInfoColors.tupleSize + 3];
						}
						else
						{
							WedgeColor.A = 1.0f;
						}

						// Convert linear color to fixed color (no sRGB conversion).
						RawMesh.WedgeColors[WedgeColorIdx] = WedgeColor.ToFColor(false);
					}
				}

				// Transfer normals.
				int32 WedgeNormalCount = Normals.Num() / 3;
				RawMesh.WedgeTangentZ.SetNumZeroed(WedgeNormalCount);
				for(int32 WedgeTangentZIdx = 0; WedgeTangentZIdx < WedgeNormalCount; ++WedgeTangentZIdx)
				{
					FVector WedgeTangentZ;

					WedgeTangentZ.X = Normals[WedgeTangentZIdx * 3 + 0];
					WedgeTangentZ.Y = Normals[WedgeTangentZIdx * 3 + 1];
					WedgeTangentZ.Z = Normals[WedgeTangentZIdx * 3 + 2];

					RawMesh.WedgeTangentZ[WedgeTangentZIdx] = WedgeTangentZ;
				}

				// Store the new raw mesh.
				SrcModel->RawMeshBulkData->SaveRawMesh(RawMesh);

				while(StaticMesh->SourceModels.Num() < NumLODs)
				{
					new(StaticMesh->SourceModels) FStaticMeshSourceModel();
				}

				for(int32 ModelLODIndex = 0; ModelLODIndex < NumLODs; ++ModelLODIndex)
				{
					StaticMesh->SourceModels[ModelLODIndex].ReductionSettings = LODGroup.GetDefaultSettings(ModelLODIndex);
				}

				//StaticMesh->PreEditChange(nullptr);
				StaticMesh->Build(true);
				//StaticMesh->PostEditChange();

				StaticMeshesOut.Add(HoudiniGeoPartObject, StaticMesh);
			}
		}
	}

	return true;
}


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
	int DegenerateTriangleCount = 0;
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


void
FHoudiniEngineUtils::TransferRegularPointAttributesToVertices(const TArray<int32>& VertexList, const HAPI_AttributeInfo& AttribInfo, TArray<float>& Data)
{
	if(AttribInfo.exists && AttribInfo.tupleSize)
	{
		if(HAPI_ATTROWNER_POINT == AttribInfo.owner)
		{
			int32 WedgeCount = VertexList.Num();
			TArray<float> VertexData;
			VertexData.SetNumZeroed(WedgeCount * AttribInfo.tupleSize);

			for(int32 WedgeIdx = 0; WedgeIdx < WedgeCount; ++WedgeIdx)
			{
				int32 VertexId = VertexList[WedgeIdx];

				for(int32 AttributeIndexIdx = 0; AttributeIndexIdx < AttribInfo.tupleSize; ++AttributeIndexIdx)
				{
					VertexData[WedgeIdx * AttribInfo.tupleSize + AttributeIndexIdx] = Data[VertexId * AttribInfo.tupleSize + AttributeIndexIdx];
				}
			}

			Data = VertexData;
		}
		else if(HAPI_ATTROWNER_PRIM == AttribInfo.owner)
		{
			// Handle case when data is on primitive.
			check(false);
		}
	}
}


void
FHoudiniEngineUtils::SaveRawStaticMesh(UStaticMesh* StaticMesh, FArchive& Ar)
{
	if(StaticMesh && Ar.IsSaving())
	{
		FRawMesh RawMesh;

		FStaticMeshSourceModel* SrcModel = &StaticMesh->SourceModels[0];
		FRawMeshBulkData* RawMeshBulkData = SrcModel->RawMeshBulkData;
		RawMeshBulkData->LoadRawMesh(RawMesh);

		// Store raw data bytes.
		FHoudiniEngineUtils::Serialize(RawMesh, StaticMesh->Materials, Ar);
	}
}


UStaticMesh*
FHoudiniEngineUtils::LoadRawStaticMesh(UHoudiniAsset* HoudiniAsset, UPackage* Package, int32 MeshCounter, FArchive& Ar)
{
	UStaticMesh* StaticMesh = nullptr;

	if(!Ar.IsLoading())
	{
		return StaticMesh;
	}

	// If we have no package, we will use transient package.
	if(!Package)
	{
		Package = GetTransientPackage();
	}

	// Get platform manager LOD specific information.
	ITargetPlatform* CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	check(CurrentPlatform);
	const FStaticMeshLODGroup& LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(NAME_None);
	int32 NumLODs = LODGroup.GetDefaultNumLODs();

	FGuid MeshGuid;
	FString MeshName;
	UPackage* MeshPackage = FHoudiniEngineUtils::BakeCreatePackageForStaticMesh(HoudiniAsset, Package, MeshName, MeshGuid, MeshCounter);
	StaticMesh = new(Package, FName(*MeshName), RF_Public) UStaticMesh(FPostConstructInitializeProperties());

	// Create new source model for current static mesh.
	if(!StaticMesh->SourceModels.Num())
	{
		new(StaticMesh->SourceModels) FStaticMeshSourceModel();
	}

	FStaticMeshSourceModel* SrcModel = &StaticMesh->SourceModels[0];
	FRawMeshBulkData* RawMeshBulkData = SrcModel->RawMeshBulkData;

	// Load raw data bytes.
	FRawMesh RawMesh;
	FHoudiniEngineUtils::Serialize(RawMesh, StaticMesh->Materials, Ar);

	// Make sure static mesh has a new lighting guid.
	StaticMesh->LightingGuid = FGuid::NewGuid();

	// Set it to use textured light maps. We use coordinate 1 for UVs.
	StaticMesh->LightMapResolution = 32;
	StaticMesh->LightMapCoordinateIndex = 1;

	StaticMesh->LightMapResolution = LODGroup.GetDefaultLightMapResolution();
	StaticMesh->LODGroup = NAME_None;
	RawMeshBulkData->SaveRawMesh(RawMesh);

	// Some mesh generation settings.
	SrcModel->BuildSettings.bRemoveDegenerates = true;
	SrcModel->BuildSettings.bRecomputeNormals = true;
	SrcModel->BuildSettings.bRecomputeTangents = true;

	// Store the new raw mesh.
	RawMeshBulkData->SaveRawMesh(RawMesh);

	while(StaticMesh->SourceModels.Num() < NumLODs)
	{
		new(StaticMesh->SourceModels) FStaticMeshSourceModel();
	}

	for(int32 ModelLODIndex = 0; ModelLODIndex < NumLODs; ++ModelLODIndex)
	{
		StaticMesh->SourceModels[ModelLODIndex].ReductionSettings = LODGroup.GetDefaultSettings(ModelLODIndex);
	}

	StaticMesh->Build(true);
	return StaticMesh;
}


void
FHoudiniEngineUtils::Serialize(FRawMesh& RawMesh, TArray<UMaterialInterface*>& Materials, FArchive& Ar)
{
	Ar << RawMesh.FaceMaterialIndices;
	Ar << RawMesh.FaceSmoothingMasks;
	Ar << RawMesh.VertexPositions;
	Ar << RawMesh.WedgeIndices;
	Ar << RawMesh.WedgeTangentX;
	Ar << RawMesh.WedgeTangentY;
	Ar << RawMesh.WedgeTangentZ;

	for(int TexCoordIdx = 0; TexCoordIdx < MAX_MESH_TEXTURE_COORDS; ++TexCoordIdx)
	{
		Ar << RawMesh.WedgeTexCoords[TexCoordIdx];
	}

	Ar << RawMesh.WedgeColors;
	Ar << RawMesh.MaterialIndexToImportIndex;

	// We also need to store / restore materials.
	if(Ar.IsLoading())
	{
		Materials.Empty();
	}

	int32 MaterialCount = Materials.Num();
	Ar << MaterialCount;

	for(int32 MaterialIdx = 0; MaterialIdx < MaterialCount; ++MaterialIdx)
	{
		UMaterialInterface* MaterialInterface = nullptr;
		FString MaterialPathName;

		if(Ar.IsSaving())
		{
			MaterialInterface = Materials[MaterialIdx];

			// If interface is null, use default material.
			if(!MaterialInterface)
			{
				MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
			}

			MaterialPathName = MaterialInterface->GetPathName();
		}

		Ar << MaterialPathName;

		if(Ar.IsLoading())
		{
			// Attempt to load this material.
			MaterialInterface = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialPathName, nullptr, LOAD_NoWarn, nullptr));
							
			if(!MaterialInterface)
			{
				// Material does not exist, use default material.
				MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
			}

			Materials.Add(MaterialInterface);
		}
	}
}


UStaticMesh*
FHoudiniEngineUtils::BakeStaticMesh(UHoudiniAsset* HoudiniAsset, UStaticMesh* InStaticMesh, int32 MeshCounter)
{
	// Get platform manager LOD specific information.
	ITargetPlatform* CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	check(CurrentPlatform);
	const FStaticMeshLODGroup& LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(NAME_None);
	int32 NumLODs = LODGroup.GetDefaultNumLODs();

	FString MeshName;
	FGuid BakeGUID;
	UPackage* Package = BakeCreatePackageForStaticMesh(HoudiniAsset, nullptr, MeshName, BakeGUID, MeshCounter);

	// Create static mesh.
	UStaticMesh* StaticMesh = new(Package, FName(*MeshName), RF_Standalone | RF_Public) UStaticMesh(FPostConstructInitializeProperties());

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

	// Make sure static mesh has a new lighting guid.
	StaticMesh->LightingGuid = FGuid::NewGuid();

	// Set it to use textured light maps. We use coordinate 1 for UVs.
	StaticMesh->LightMapResolution = 32;
	StaticMesh->LightMapCoordinateIndex = 1;

	StaticMesh->LightMapResolution = LODGroup.GetDefaultLightMapResolution();
	StaticMesh->LODGroup = NAME_None;
	RawMeshBulkData->SaveRawMesh(RawMesh);

	// Some mesh generation settings.
	SrcModel->BuildSettings.bRemoveDegenerates = true;
	SrcModel->BuildSettings.bRecomputeNormals = true;
	SrcModel->BuildSettings.bRecomputeTangents = true;

	// Store the new raw mesh.
	RawMeshBulkData->SaveRawMesh(RawMesh);

	while(StaticMesh->SourceModels.Num() < NumLODs)
	{
		new(StaticMesh->SourceModels) FStaticMeshSourceModel();
	}

	for(int32 ModelLODIndex = 0; ModelLODIndex < NumLODs; ++ModelLODIndex)
	{
		StaticMesh->SourceModels[ModelLODIndex].ReductionSettings = LODGroup.GetDefaultSettings(ModelLODIndex);
	}

	StaticMesh->Build(true);
	return StaticMesh;
}


UMaterial*
FHoudiniEngineUtils::HapiCreateMaterial(const HAPI_MaterialInfo& MaterialInfo, UPackage* Package, const FString& MeshName)
{
	UMaterial* Material = nullptr;
	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	// Get node information.
	HAPI_NodeInfo NodeInfo;
	HAPI_GetNodeInfo(MaterialInfo.nodeId, &NodeInfo);

	// Get node parameters.
	std::vector<HAPI_ParmInfo> NodeParams;
	NodeParams.resize(NodeInfo.parmCount);
	HAPI_GetParameters(NodeInfo.id, &NodeParams[0], 0, NodeInfo.parmCount);

	// Get names of parameters.
	std::vector<std::string> NodeParamNames;
	NodeParamNames.resize(NodeInfo.parmCount);
	FHoudiniEngineUtils::HapiRetrieveParameterNames(NodeParams, NodeParamNames);

	// See if texture is available.
	int ParmNameIdx = FHoudiniEngineUtils::HapiFindParameterByName("ogl_tex1", NodeParamNames);

	if(-1 == ParmNameIdx)
	{
		ParmNameIdx = FHoudiniEngineUtils::HapiFindParameterByName("baseColorMap", NodeParamNames);
	}

	if(-1 == ParmNameIdx)
	{
		ParmNameIdx = FHoudiniEngineUtils::HapiFindParameterByName("map", NodeParamNames);
	}

	if(ParmNameIdx >= 0)
	{
		// Update context for generated materials (will trigger when object goes out of scope).
		FMaterialUpdateContext MaterialUpdateContext;

		UMaterialFactoryNew* MaterialFactory = new UMaterialFactoryNew(FPostConstructInitializeProperties());
		FString MaterialName = FString::Printf(TEXT("%s_material"), *MeshName);
		Material = (UMaterial*) MaterialFactory->FactoryCreateNew(UMaterial::StaticClass(), Package, *MaterialName, RF_Transient | RF_Public, NULL, GWarn);
	
		std::vector<char> ImageBuffer;

		// Retrieve color data.
		if(FHoudiniEngineUtils::HapiExtractImage(NodeParams[ParmNameIdx].id, MaterialInfo, ImageBuffer, "C A"))
		{
			HAPI_ImageInfo ImageInfo;
			Result = HAPI_GetImageInfo(MaterialInfo.assetId, MaterialInfo.id, &ImageInfo);
							
			if(ImageInfo.xRes > 0 && ImageInfo.yRes > 0)
			{
				// Create texture.
				UTexture2D* Texture = FHoudiniEngineUtils::CreateUnrealTexture(ImageInfo, PF_R8G8B8A8, ImageBuffer);

				// Create sampling expression and add it to material.
				UMaterialExpressionTextureSample* Expression = ConstructObject<UMaterialExpressionTextureSample>(UMaterialExpressionTextureSample::StaticClass(), Material);
				Expression->Texture = Texture;
				Expression->SamplerType = SAMPLERTYPE_Color;

				Material->Expressions.Add(Expression);

				Material->BaseColor.Expression = Expression;

				if(FHoudiniEngineUtils::HapiIsMaterialTransparent(MaterialInfo))
				{
					// This material contains transparency.
					Material->BlendMode = BLEND_Masked;

					TArray<FExpressionOutput> Outputs = Expression->GetOutputs();
					FExpressionOutput* Output = Outputs.GetTypedData();

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

				// Set other material properties.
				Material->TwoSided = true;
				Material->SetShadingModel(MSM_DefaultLit);

				// Propagate and trigger material updates.
				Material->PreEditChange(nullptr);
				Material->PostEditChange();
				Material->MarkPackageDirty();

				// Schedule this material for update.
				MaterialUpdateContext.AddMaterial(Material);
			}
		}
	}

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
FHoudiniEngineUtils::CreateFaceMaterialArray(const TArray<UMaterialInterface*>& Materials, const TArray<int32>& FaceMaterialIndices,
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
