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

	return FString(ANSI_TO_TCHAR(&StatusStringBuffer[0]));
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
		NameString = ANSI_TO_TCHAR(NamePlain.c_str());
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
	std::vector<float> StaticMeshVertices;
	for(int VertexIdx = 0; VertexIdx < RawMesh.VertexPositions.Num(); ++VertexIdx)
	{
		// Grab vertex at this index (we also need to swap Z and Y).
		const FVector& PositionVector = RawMesh.VertexPositions[VertexIdx];
		StaticMeshVertices.push_back(PositionVector.X / FHoudiniEngineUtils::ScaleFactorPosition);
		StaticMeshVertices.push_back(PositionVector.Z / FHoudiniEngineUtils::ScaleFactorPosition);
		StaticMeshVertices.push_back(PositionVector.Y / FHoudiniEngineUtils::ScaleFactorPosition);
	}

	// Now that we have raw positions, we can upload them for our attribute.
	HOUDINI_CHECK_ERROR_RETURN(HAPI_SetAttributeFloatData(ConnectedAssetId, 0, 0, HAPI_ATTRIB_POSITION, &AttributeInfoPoint, 
														  &StaticMeshVertices[0], 0, AttributeInfoPoint.count), false);

	// See if we have texture coordinates to upload.
	for(int32 MeshTexCoordIdx = 0; MeshTexCoordIdx < MAX_STATIC_TEXCOORDS; ++MeshTexCoordIdx)
	{
		int32 StaticMeshUVCount = RawMesh.WedgeTexCoords[MeshTexCoordIdx].Num();

		if(StaticMeshUVCount > 0)
		{
			const TArray<FVector2D>& RawMeshUVs = RawMesh.WedgeTexCoords[MeshTexCoordIdx];
			std::vector<float> StaticMeshUVs(RawMeshUVs.Num() * 2);
			for(int32 UVIdx = 0; UVIdx < StaticMeshUVCount; ++UVIdx)
			{
				StaticMeshUVs[UVIdx * 2 + 0] = RawMeshUVs[UVIdx].X;
				StaticMeshUVs[UVIdx * 2 + 1] = 1.0f - RawMeshUVs[UVIdx].Y;
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

			// Upload UV data.
			HOUDINI_CHECK_ERROR_RETURN(HAPI_SetAttributeFloatData(ConnectedAssetId, 0, 0, UVAttributeNameString, &AttributeInfoVertex, 
																  &StaticMeshUVs[0], 0, AttributeInfoVertex.count), false);
		}
	}

	// Upload Unreal specific packed tangents.
	if(RawMesh.WedgeTangentX.Num() && RawMesh.WedgeTangentY.Num())
	{
		// Get raw tangent data.
		FVector* RawMeshTangentsX = RawMesh.WedgeTangentX.GetData();
		FVector* RawMeshTangentsY = RawMesh.WedgeTangentY.GetData();

		// We need to upload them as integers (they are unsigned ints).

		{
			int RawMeshTangentsXDataCount = RawMesh.WedgeTangentX.Num();
			std::vector<int> RawMeshTangentsXData(RawMeshTangentsXDataCount * 4);
			for(int TangentXIdx = 0; TangentXIdx < RawMeshTangentsXDataCount; ++TangentXIdx)
			{
				FPackedNormal PackedNormal(RawMeshTangentsX[TangentXIdx]);
				RawMeshTangentsXData[TangentXIdx * 4 + 0] = PackedNormal.Vector.X;
				RawMeshTangentsXData[TangentXIdx * 4 + 1] = PackedNormal.Vector.Y;
				RawMeshTangentsXData[TangentXIdx * 4 + 2] = PackedNormal.Vector.Z;
				RawMeshTangentsXData[TangentXIdx * 4 + 3] = PackedNormal.Vector.W;
			}

			// Create attribute for packed tangent.
			HAPI_AttributeInfo AttributeInfoPackedTangent = HAPI_AttributeInfo_Create();
			AttributeInfoPackedTangent.count = RawMeshTangentsXDataCount;
			AttributeInfoPackedTangent.tupleSize = 4; 
			AttributeInfoPackedTangent.exists = true;
			AttributeInfoPackedTangent.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoPackedTangent.storage = HAPI_STORAGETYPE_INT;
			HOUDINI_CHECK_ERROR_RETURN(HAPI_AddAttribute(ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_PACKED_TANGENT, &AttributeInfoPackedTangent), false);

			// Upload normal data.
			HOUDINI_CHECK_ERROR_RETURN(HAPI_SetAttributeIntData(ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_PACKED_TANGENT, &AttributeInfoPackedTangent, 
															  (int*) &RawMeshTangentsXData[0], 0, AttributeInfoPackedTangent.count), false);
		}

		{
			int RawMeshTangentsYDataCount = RawMesh.WedgeTangentY.Num();
			std::vector<int> RawMeshTangentsYData(RawMeshTangentsYDataCount * 4);
			for(int TangentYIdx = 0; TangentYIdx < RawMeshTangentsYDataCount; ++TangentYIdx)
			{
				FPackedNormal PackedNormal(RawMeshTangentsX[TangentYIdx]);
				RawMeshTangentsYData[TangentYIdx * 4 + 0] = PackedNormal.Vector.X;
				RawMeshTangentsYData[TangentYIdx * 4 + 1] = PackedNormal.Vector.Y;
				RawMeshTangentsYData[TangentYIdx * 4 + 2] = PackedNormal.Vector.Z;
				RawMeshTangentsYData[TangentYIdx * 4 + 3] = PackedNormal.Vector.W;
			}

			// Create attribute for packed tangent.
			HAPI_AttributeInfo AttributeInfoPackedTangent2 = HAPI_AttributeInfo_Create();
			AttributeInfoPackedTangent2.count = RawMeshTangentsYDataCount;
			AttributeInfoPackedTangent2.tupleSize = 4; 
			AttributeInfoPackedTangent2.exists = true;
			AttributeInfoPackedTangent2.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoPackedTangent2.storage = HAPI_STORAGETYPE_INT;
			HOUDINI_CHECK_ERROR_RETURN(HAPI_AddAttribute(ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_PACKED_TANGENT2, &AttributeInfoPackedTangent2), false);

			// Upload normal data.
			HOUDINI_CHECK_ERROR_RETURN(HAPI_SetAttributeIntData(ConnectedAssetId, 0, 0, HAPI_UNREAL_ATTRIB_PACKED_TANGENT2, &AttributeInfoPackedTangent2, 
															  (int*) &RawMeshTangentsYData[0], 0, AttributeInfoPackedTangent2.count), false);
		}
	}

	// See if we have normals to upload.
	if(RawMesh.WedgeTangentZ.Num())
	{
		// Get raw normal data.
		FVector* RawMeshNormals = RawMesh.WedgeTangentZ.GetData();

		// Create attribute for normals
		HAPI_AttributeInfo AttributeInfoVertex = HAPI_AttributeInfo_Create();
		AttributeInfoVertex.count = RawMesh.WedgeTangentZ.Num();
		AttributeInfoVertex.tupleSize = 3; 
		AttributeInfoVertex.exists = true;
		AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
		AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
		HOUDINI_CHECK_ERROR_RETURN(HAPI_AddAttribute(ConnectedAssetId, 0, 0, HAPI_ATTRIB_NORMAL, &AttributeInfoVertex), false);

		// Upload normal data.
		HOUDINI_CHECK_ERROR_RETURN(HAPI_SetAttributeFloatData(ConnectedAssetId, 0, 0, HAPI_ATTRIB_NORMAL, &AttributeInfoVertex, 
														  (float*) RawMeshNormals, 0, AttributeInfoVertex.count), false);
	}

	// Extract indices from static mesh.
	if(RawMesh.WedgeIndices.Num())
	{
		std::vector<int> StaticMeshIndices;
		for(int IndexIdx = 0; IndexIdx < RawMesh.WedgeIndices.Num(); IndexIdx += 3)
		{
			// Swap indices to fix winding order.
			StaticMeshIndices.push_back(RawMesh.WedgeIndices[IndexIdx + 0]);
			StaticMeshIndices.push_back(RawMesh.WedgeIndices[IndexIdx + 2]);
			StaticMeshIndices.push_back(RawMesh.WedgeIndices[IndexIdx + 1]);
		}

		// We can now set vertex list.
		HOUDINI_CHECK_ERROR_RETURN(HAPI_SetVertexList(ConnectedAssetId, 0, 0, &StaticMeshIndices[0], 0, StaticMeshIndices.size()), false);

		// We need to generate array of face counts.
		std::vector<int> StaticMeshFaceCounts(Part.faceCount, 3);
		HOUDINI_CHECK_ERROR_RETURN(HAPI_SetFaceCounts(ConnectedAssetId, 0, 0, &StaticMeshFaceCounts[0], 0, StaticMeshFaceCounts.size()), false);
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

	// Set it to use textured lightmaps. We use coordinate 1 for UVs.
	StaticMesh->LightMapResolution = 32;
	StaticMesh->LightMapCoordinateIndex = 1;

	// Calculate number of triangles.
	int32 TriangleCount = WedgeCount / 3;

	// Reserve space for attributes.
	RawMesh.FaceMaterialIndices.AddZeroed(TriangleCount);
	RawMesh.FaceSmoothingMasks.AddZeroed(TriangleCount);
	RawMesh.VertexPositions.Reserve(VertexCount);
	RawMesh.WedgeIndices.Reserve(WedgeCount);
	RawMesh.WedgeColors.AddZeroed(WedgeCount);
	RawMesh.WedgeTexCoords[0].AddZeroed(WedgeCount);

	//RawMesh.WedgeTangentX.AddZeroed(WedgeCount);
	//RawMesh.WedgeTangentY.AddZeroed(WedgeCount);
	//RawMesh.WedgeTangentZ.AddZeroed(WedgeCount);

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
	StaticMesh->Build(false);

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

				// There are no vertices.
				if(PartInfo.vertexCount <= 0)
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

				// Attempt to locate static mesh from previous instantiation.
				FHoudiniGeoPartObject HoudiniGeoPartObject(TransformMatrix, ObjectInfo.id, GeoInfo.id, PartInfo.id);
				UStaticMesh* const* FoundStaticMesh = StaticMeshesIn.Find(HoudiniGeoPartObject);

				// See if geometry has changed for this part.
				if(!GeoInfo.hasGeoChanged)
				{
					// If geometry has not changed, look up static mesh from previous instantiation / cooking and use it.
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

				if(bMaterialFound)
				{
					if(MaterialInfo.hasChanged)
					{
						// Attempt to create material.
						UMaterial* Material = FHoudiniEngineUtils::HapiCreateMaterial(MaterialInfo, Package, MeshName);

						if(Material)
						{
							// We have a valid created material.
							StaticMesh->Materials.Add(Material);
						}
						else
						{
							// Otherwise, we just use default material.
							UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
							StaticMesh->Materials.Add(DefaultMaterial);
						}
					}
					else
					{
						// Material hasn't changed, we do not need to change anything.
						check(StaticMesh->Materials.Num() > 0);
					}
				}
				else
				{
					// Locate default material and add it to mesh, if we do not have it set.
					if(0 == StaticMesh->Materials.Num())
					{
						UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
						StaticMesh->Materials.Add(DefaultMaterial);
					}
				}

				// Make sure static mesh has a new lighting guid.
				StaticMesh->LightingGuid = FGuid::NewGuid();

				// Set it to use textured light maps. We use coordinate 1 for UVs.
				StaticMesh->LightMapResolution = 32;
				StaticMesh->LightMapCoordinateIndex = 1;

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

				// Retrieve color data.
				HAPI_AttributeInfo AttribInfoColors;
				FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
					HAPI_ATTRIB_COLOR, AttribInfoColors, Colors);

				// Retrieve normal data.
				HAPI_AttributeInfo AttribInfoNormals;
				FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id,
					HAPI_ATTRIB_NORMAL, AttribInfoNormals, Normals);

				// Retrieve UVs.
				for(int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx)
				{
					std::string UVAttributeName = HAPI_ATTRIB_UV;

					if(TexCoordIdx > 0)
					{
						UVAttributeName += std::to_string(TexCoordIdx + 1);
					}

					const char* UVAttributeNameString = UVAttributeName.c_str();

					HAPI_AttributeInfo AttribInfoUVs;
					FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id, UVAttributeNameString,
																	 AttribInfoUVs, TextureCoordinates[TexCoordIdx], 2);
				}

				// We can transfer attributes to raw mesh.

				// Compute number of faces.
				int32 FaceCount = PartInfo.vertexCount / 3;

				// Set face specific information.
				RawMesh.FaceMaterialIndices.SetNumZeroed(FaceCount);
				RawMesh.FaceSmoothingMasks.SetNumZeroed(FaceCount);

				// Transfer UVs.
				bool bFoundUVs = false;
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

							bFoundUVs = true;
						}
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
					WedgeTangentZ.Z = Normals[WedgeTangentZIdx * 3 + 1];
					WedgeTangentZ.Y = Normals[WedgeTangentZIdx * 3 + 2];

					RawMesh.WedgeTangentZ[WedgeTangentZIdx] = WedgeTangentZ;
				}

				// Create empty tangents.
				RawMesh.WedgeTangentX.SetNumZeroed(VertexList.Num());
				RawMesh.WedgeTangentY.SetNumZeroed(VertexList.Num());

				// We have to have at least one UV channel. If there's none, create one with zero data.
				if(!bFoundUVs)
				{
					RawMesh.WedgeTexCoords[0].SetNumZeroed(VertexList.Num());
				}

				// Some mesh generation settings.
				SrcModel->BuildSettings.bRemoveDegenerates = true;
				SrcModel->BuildSettings.bRecomputeNormals = true;
				SrcModel->BuildSettings.bRecomputeTangents = true;

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

				StaticMesh->Build(true);
				StaticMeshesOut.Add(HoudiniGeoPartObject, StaticMesh);
			}
		}
	}

	return true;
}


void
FHoudiniEngineUtils::SaveRawStaticMesh(UStaticMesh* StaticMesh, FArchive& Ar)
{
	if(Ar.IsSaving())
	{
		FRawMesh RawMesh;

		FStaticMeshSourceModel* SrcModel = &StaticMesh->SourceModels[0];
		FRawMeshBulkData* RawMeshBulkData = SrcModel->RawMeshBulkData;
		RawMeshBulkData->LoadRawMesh(RawMesh);

		// Store raw data bytes.
		FHoudiniEngineUtils::Serialize(RawMesh, Ar);
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
	FHoudiniEngineUtils::Serialize(RawMesh, Ar);

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
FHoudiniEngineUtils::Serialize(FRawMesh& RawMesh, FArchive& Ar)
{
	// Serialize material index.
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
		Material = (UHoudiniAssetMaterial*) MaterialFactory->FactoryCreateNew(UMaterial::StaticClass(), Package, *MaterialName, RF_Transient | RF_Public, NULL, GWarn);
	
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
