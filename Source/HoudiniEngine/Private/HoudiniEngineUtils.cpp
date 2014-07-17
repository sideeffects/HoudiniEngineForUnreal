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


void
FHoudiniEngineUtils::UpdateBoundingVolumeExtent(const FVector& Vector, FVector& ExtentMin, FVector& ExtentMax)
{
	if(ExtentMin.X > Vector.X)
	{
		ExtentMin.X = Vector.X;
	}

	if(ExtentMin.Y > Vector.Y)
	{
		ExtentMin.Y = Vector.Y;
	}

	if(ExtentMin.Z > Vector.Z)
	{
		ExtentMin.Z = Vector.Z;
	}

	if(ExtentMax.X < Vector.X)
	{
		ExtentMax.X = Vector.X;
	}

	if(ExtentMax.Y < Vector.Y)
	{
		ExtentMax.Y = Vector.Y;
	}

	if(ExtentMax.Z < Vector.Z)
	{
		ExtentMax.Z = Vector.Z;
	}
}


void
FHoudiniEngineUtils::GetBoundingVolume(const TArray<FHoudiniMeshTriangle>& Geometry, FBoxSphereBounds& SphereBounds)
{
	// Used for computation of bounding volume.
	FVector ExtentMin(TNumericLimits<float>::Max(), TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
	FVector ExtentMax(TNumericLimits<float>::Min(), TNumericLimits<float>::Min(), TNumericLimits<float>::Min());

	// Transfer data.
	for(int TriangleIdx = 0; TriangleIdx < Geometry.Num(); ++TriangleIdx)
	{
		const FHoudiniMeshTriangle& Triangle = Geometry[TriangleIdx];

		UpdateBoundingVolumeExtent(Triangle.Vertex0, ExtentMin, ExtentMax);
		UpdateBoundingVolumeExtent(Triangle.Vertex2, ExtentMin, ExtentMax);
		UpdateBoundingVolumeExtent(Triangle.Vertex1, ExtentMin, ExtentMax);
	}

	// Create bounding volume information.
	SphereBounds = FBoxSphereBounds(FBox(ExtentMin, ExtentMax));
}


bool
FHoudiniEngineUtils::GetAssetGeometry(HAPI_AssetId AssetId, TArray<FHoudiniMeshTriangle>& Geometry, FBoxSphereBounds& SphereBounds)
{
	// Reset input geometry.
	Geometry.Reset();

	if(AssetId < 0)
	{
		// We have invalid asset, return generic bounding volume information.
		SphereBounds = FBoxSphereBounds(FBox(-FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX, FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX));
		return false;
	}

	HAPI_Result Result = HAPI_RESULT_SUCCESS;
	HAPI_AssetInfo AssetInfo;
	HAPI_PartInfo PartInfo;
	HAPI_AttributeInfo AttribInfo;

	// Used for computation of bounding volume.
	FVector ExtentMin(TNumericLimits<float>::Max(), TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
	FVector ExtentMax(TNumericLimits<float>::Min(), TNumericLimits<float>::Min(), TNumericLimits<float>::Min());

	// Storage for extracted raw vertex attributes.
	std::vector<int> VertexList;
	std::vector<float> Positions;
	std::vector<float> UVs;
	std::vector<float> Normals;
	
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAssetInfo(AssetId, &AssetInfo), false);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetPartInfo(AssetId, 0, 0, 0, &PartInfo), false);

	VertexList.resize(PartInfo.vertexCount);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetVertexList(AssetId, 0, 0, 0, &VertexList[0], 0, PartInfo.vertexCount), false);

	// Retrieve positions (they are always on a point).
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAttributeInfo(AssetId, 0, 0, 0, "P", HAPI_ATTROWNER_POINT, &AttribInfo), false);
	Positions.resize(AttribInfo.count * AttribInfo.tupleSize);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAttributeFloatData(AssetId, 0, 0, 0, "P", &AttribInfo, &Positions[0], 0, AttribInfo.count), false);

	// Retrieve texture coordinate information (points too, prim).
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAttributeInfo(AssetId, 0, 0, 0, "uv", HAPI_ATTROWNER_VERTEX, &AttribInfo), false);
	if(AttribInfo.exists)
	{
		UVs.resize(AttribInfo.count * AttribInfo.tupleSize);
		HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAttributeFloatData(AssetId, 0, 0, 0, "uv", &AttribInfo, &UVs[0], 0, AttribInfo.count), false);
	}

	// Retrieve normals information (points and prims).
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAttributeInfo(AssetId, 0, 0, 0, "N", HAPI_ATTROWNER_VERTEX, &AttribInfo), false);
	if(AttribInfo.exists)
	{
		Normals.resize(AttribInfo.count * AttribInfo.tupleSize);
		HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAttributeFloatData(AssetId, 0, 0, 0, "N", &AttribInfo, &Normals[0], 0, AttribInfo.count), false);
	}

	// Geometry scale factor.
	static const float ScaleFactor = 100.0f;
	static const FColor VertexColor(255, 255, 255);

	// Transfer data.
	for(int TriangleIdx = 0; TriangleIdx < PartInfo.vertexCount / 3; ++TriangleIdx)
	{
		FHoudiniMeshTriangle Triangle;

		// Process position information.
		// Need to flip the Y with the Z since UE4 is Z-up.
		// Need to flip winding order also.

		Triangle.Vertex0.X = Positions[VertexList[TriangleIdx * 3 + 0] * 3 + 0] * ScaleFactor;
		Triangle.Vertex0.Z = Positions[VertexList[TriangleIdx * 3 + 0] * 3 + 1] * ScaleFactor;
		Triangle.Vertex0.Y = Positions[VertexList[TriangleIdx * 3 + 0] * 3 + 2] * ScaleFactor;
		UpdateBoundingVolumeExtent(Triangle.Vertex0, ExtentMin, ExtentMax);

		Triangle.Vertex2.X = Positions[VertexList[TriangleIdx * 3 + 1] * 3 + 0] * ScaleFactor;
		Triangle.Vertex2.Z = Positions[VertexList[TriangleIdx * 3 + 1] * 3 + 1] * ScaleFactor;
		Triangle.Vertex2.Y = Positions[VertexList[TriangleIdx * 3 + 1] * 3 + 2] * ScaleFactor;
		UpdateBoundingVolumeExtent(Triangle.Vertex2, ExtentMin, ExtentMax);

		Triangle.Vertex1.X = Positions[VertexList[TriangleIdx * 3 + 2] * 3 + 0] * ScaleFactor;
		Triangle.Vertex1.Z = Positions[VertexList[TriangleIdx * 3 + 2] * 3 + 1] * ScaleFactor;
		Triangle.Vertex1.Y = Positions[VertexList[TriangleIdx * 3 + 2] * 3 + 2] * ScaleFactor;	
		UpdateBoundingVolumeExtent(Triangle.Vertex1, ExtentMin, ExtentMax);

		// Process texture information.
		// Need to flip the U coordinate.
		if(UVs.size())
		{
			Triangle.TextureCoordinate0.X = UVs[TriangleIdx * 9 + 0];
			Triangle.TextureCoordinate0.Y = 1.0f - UVs[TriangleIdx * 9 + 1];

			Triangle.TextureCoordinate2.X = UVs[TriangleIdx * 9 + 3];
			Triangle.TextureCoordinate2.Y = 1.0f - UVs[TriangleIdx * 9 + 4];

			Triangle.TextureCoordinate1.X = UVs[TriangleIdx * 9 + 6];
			Triangle.TextureCoordinate1.Y = 1.0f - UVs[TriangleIdx * 9 + 7];
		}

		// Process normals.
		if(Normals.size())
		{
			Triangle.Normal0.X = Normals[VertexList[TriangleIdx * 3 + 0] * 3 + 0];
			Triangle.Normal0.Z = Normals[VertexList[TriangleIdx * 3 + 0] * 3 + 1];
			Triangle.Normal0.Y = Normals[VertexList[TriangleIdx * 3 + 0] * 3 + 2];

			Triangle.Normal2.X = Normals[VertexList[TriangleIdx * 3 + 1] * 3 + 0];
			Triangle.Normal2.Z = Normals[VertexList[TriangleIdx * 3 + 1] * 3 + 1];
			Triangle.Normal2.Y = Normals[VertexList[TriangleIdx * 3 + 1] * 3 + 2];

			Triangle.Normal1.X = Normals[VertexList[TriangleIdx * 3 + 2] * 3 + 0];
			Triangle.Normal1.Z = Normals[VertexList[TriangleIdx * 3 + 2] * 3 + 1];
			Triangle.Normal1.Y = Normals[VertexList[TriangleIdx * 3 + 2] * 3 + 2];
		}

		// Set color.
		Triangle.Color0 = VertexColor;
		Triangle.Color1 = VertexColor;
		Triangle.Color2 = VertexColor;

		// Store the extracted triangle.
		Geometry.Push(Triangle);
	}

	// Create bounding volume information.
	SphereBounds = FBoxSphereBounds(FBox(ExtentMin, ExtentMax));

	return true;
}


void
FHoudiniEngineUtils::GetHoudiniLogoGeometry(TArray<FHoudiniMeshTriangle>& Geometry, FBoxSphereBounds& SphereBounds)
{
	// Reset input geometry.
	Geometry.Reset();

	// Used for computation of bounding volume.
	FVector ExtentMin(TNumericLimits<float>::Max(), TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
	FVector ExtentMax(TNumericLimits<float>::Min(), TNumericLimits<float>::Min(), TNumericLimits<float>::Min());

	// Compute number of triangles in Houdini logo.
	int VertexIndexCount = sizeof(FHoudiniLogo::VertexIndices) / sizeof(int);
	
	// Geometry scale factor.
	static const float ScaleFactor = 12.0f;
	static const FColor VertexColor(255, 165, 0);

	int VertexIndexLookup;

	// Construct triangle data.
	for(int VertexIndex = 0; VertexIndex < VertexIndexCount; VertexIndex += 3)
	{
		FHoudiniMeshTriangle Triangle;

		VertexIndexLookup = FHoudiniLogo::VertexIndices[VertexIndex + 0] - 1;
		Triangle.Vertex0.X = FHoudiniLogo::Vertices[VertexIndexLookup * 3 + 0] * ScaleFactor;
		Triangle.Vertex0.Z = FHoudiniLogo::Vertices[VertexIndexLookup * 3 + 1] * ScaleFactor;
		Triangle.Vertex0.Y = FHoudiniLogo::Vertices[VertexIndexLookup * 3 + 2] * ScaleFactor;
		Triangle.Color0 = VertexColor;
		UpdateBoundingVolumeExtent(Triangle.Vertex0, ExtentMin, ExtentMax);

		VertexIndexLookup = FHoudiniLogo::VertexIndices[VertexIndex + 1] - 1;
		Triangle.Vertex1.X = FHoudiniLogo::Vertices[VertexIndexLookup * 3 + 0] * ScaleFactor;
		Triangle.Vertex1.Z = FHoudiniLogo::Vertices[VertexIndexLookup * 3 + 1] * ScaleFactor;
		Triangle.Vertex1.Y = FHoudiniLogo::Vertices[VertexIndexLookup * 3 + 2] * ScaleFactor;
		Triangle.Color1 = VertexColor;
		UpdateBoundingVolumeExtent(Triangle.Vertex1, ExtentMin, ExtentMax);

		VertexIndexLookup = FHoudiniLogo::VertexIndices[VertexIndex + 2] - 1;
		Triangle.Vertex2.X = FHoudiniLogo::Vertices[VertexIndexLookup * 3 + 0] * ScaleFactor;
		Triangle.Vertex2.Z = FHoudiniLogo::Vertices[VertexIndexLookup * 3 + 1] * ScaleFactor;
		Triangle.Vertex2.Y = FHoudiniLogo::Vertices[VertexIndexLookup * 3 + 2] * ScaleFactor;
		Triangle.Color2 = VertexColor;
		UpdateBoundingVolumeExtent(Triangle.Vertex2, ExtentMin, ExtentMax);

		// Store the extracted triangle.
		Geometry.Push(Triangle);
	}

	// Create bounding volume information.
	SphereBounds = FBoxSphereBounds(FBox(ExtentMin, ExtentMax));
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
FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(HAPI_AssetId AssetId, HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId,
												 const char* Name, HAPI_AttributeInfo& ResultAttributeInfo,
												 std::vector<float>& Data, int TupleSize)
{
	ResultAttributeInfo.exists = false;

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
	Data.resize(AttributeInfo.count * AttributeInfo.tupleSize);
	
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAttributeFloatData(AssetId, ObjectId, GeoId, PartId, Name, &AttributeInfo,
		&Data[0], 0, AttributeInfo.count), false);

	// Store the retrieved attribute information.
	ResultAttributeInfo = AttributeInfo;
	return true;
}


bool
FHoudiniEngineUtils::ConstructGeos(HAPI_AssetId AssetId, TArray<FHoudiniAssetObjectGeo*>& HoudiniAssetObjectGeos)
{
	// Make sure asset id is valid.
	if(AssetId < 0)
	{
		return false;
	}

	HAPI_Result Result = HAPI_RESULT_SUCCESS;
	HAPI_AssetInfo AssetInfo;
	
	std::vector<HAPI_ObjectInfo> ObjectInfos;
	std::vector<HAPI_Transform> ObjectTransforms;

	std::vector<int> VertexList;
	std::vector<int> GroupMembership;

	std::vector<float> Positions;
	std::vector<float> UVs;
	std::vector<float> Normals;
	std::vector<float> Colors;
	std::vector<float> Tangents;

	// Geometry scale factors.
	static const float ScaleFactorPosition = 75.0f;
	static const float ScaleFactorTranslate = 50.0f;

	// Get asset information.
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAssetInfo(AssetId, &AssetInfo), false);

	// Retrieve information about each object contained within our asset.
	ObjectInfos.resize(AssetInfo.objectCount);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetObjects(AssetId, &ObjectInfos[0], 0, AssetInfo.objectCount), false);

	// Retrieve transforms for each object in this asset.
	ObjectTransforms.resize(AssetInfo.objectCount);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetObjectTransforms(AssetId, HAPI_SRT, &ObjectTransforms[0], 0, AssetInfo.objectCount), false);

	// Iterate through all objects.
	for(int32 ObjectIdx = 0; ObjectIdx < ObjectInfos.size(); ++ObjectIdx)
	{
		// Retrieve object at this index.
		const HAPI_ObjectInfo& ObjectInfo = ObjectInfos[ObjectIdx];

		// Convert HAPI transform to a matrix form.
		FMatrix TransformMatrix;
		HOUDINI_CHECK_ERROR_RETURN(HAPI_ConvertTransformQuatToMatrix(&ObjectTransforms[ObjectIdx], &TransformMatrix.M[0][0]), false);
		TransformMatrix.ScaleTranslation(FVector(ScaleFactorTranslate, ScaleFactorTranslate, ScaleFactorTranslate));

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

			// Get group names per Geo.
			std::vector<std::string> GroupNames;
			if(!FHoudiniEngineUtils::HapiGetGroupNames(AssetId, ObjectInfo.id, GeoInfo.id, HAPI_GROUPTYPE_PRIM, GroupNames))
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

				// We need to create a vertex buffer for each part.
				VertexList.resize(PartInfo.vertexCount);
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

				// Retrieve UV data.
				HAPI_AttributeInfo AttribInfoUVs;
				FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id, HAPI_ATTRIB_UV, AttribInfoUVs, UVs, 2);

				// Retrieve normal data.
				HAPI_AttributeInfo AttribInfoNormals;
				FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id, HAPI_ATTRIB_NORMAL, AttribInfoNormals, Normals);

				// Retrieve tangent data.
				HAPI_AttributeInfo AttribInfoTangents;
				FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id, HAPI_ATTRIB_TANGENT, AttribInfoTangents, Tangents);

				// Retrieve color data.
				HAPI_AttributeInfo AttribInfoColors;
				FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id, HAPI_ATTRIB_COLOR, AttribInfoColors, Colors);

				// At this point we can create a new geo object.
				FHoudiniAssetObjectGeo* HoudiniAssetObjectGeo = new FHoudiniAssetObjectGeo(TransformMatrix);
				HoudiniAssetObjectGeos.Add(HoudiniAssetObjectGeo);

				// Transfer vertex data into vertex buffer for this geo object.
				for(int TriangleIdx = 0; TriangleIdx < PartInfo.faceCount; ++TriangleIdx)
				{
					FHoudiniMeshTriangle Triangle;

					// Process position information.
					// Need to flip the Y with the Z since UE4 is Z-up.
					// Need to flip winding order also.

					Triangle.Vertex0.X = Positions[VertexList[TriangleIdx * 3 + 0] * 3 + 0] * ScaleFactorPosition;
					Triangle.Vertex0.Z = Positions[VertexList[TriangleIdx * 3 + 0] * 3 + 1] * ScaleFactorPosition;
					Triangle.Vertex0.Y = Positions[VertexList[TriangleIdx * 3 + 0] * 3 + 2] * ScaleFactorPosition;
					//UpdateBoundingVolumeExtent(Triangle.Vertex0, ExtentMin, ExtentMax);

					Triangle.Vertex2.X = Positions[VertexList[TriangleIdx * 3 + 1] * 3 + 0] * ScaleFactorPosition;
					Triangle.Vertex2.Z = Positions[VertexList[TriangleIdx * 3 + 1] * 3 + 1] * ScaleFactorPosition;
					Triangle.Vertex2.Y = Positions[VertexList[TriangleIdx * 3 + 1] * 3 + 2] * ScaleFactorPosition;
					//UpdateBoundingVolumeExtent(Triangle.Vertex2, ExtentMin, ExtentMax);

					Triangle.Vertex1.X = Positions[VertexList[TriangleIdx * 3 + 2] * 3 + 0] * ScaleFactorPosition;
					Triangle.Vertex1.Z = Positions[VertexList[TriangleIdx * 3 + 2] * 3 + 1] * ScaleFactorPosition;
					Triangle.Vertex1.Y = Positions[VertexList[TriangleIdx * 3 + 2] * 3 + 2] * ScaleFactorPosition;
					//UpdateBoundingVolumeExtent(Triangle.Vertex1, ExtentMin, ExtentMax);

					// Process texture information.
					// Need to flip the U coordinate.
					if(AttribInfoUVs.exists)
					{
						Triangle.TextureCoordinate0.X = UVs[VertexList[TriangleIdx * 3 + 0] * 3 + 0];
						Triangle.TextureCoordinate0.Y = 1.0f - UVs[VertexList[TriangleIdx * 3 + 0] * 3 + 1];

						Triangle.TextureCoordinate2.X = UVs[VertexList[TriangleIdx * 3 + 1] * 3 + 0];
						Triangle.TextureCoordinate2.Y = 1.0f - UVs[VertexList[TriangleIdx * 3 + 1] * 3 + 1];

						Triangle.TextureCoordinate1.X = UVs[VertexList[TriangleIdx * 3 + 2] * 3 + 0];
						Triangle.TextureCoordinate1.Y = 1.0f - UVs[VertexList[TriangleIdx * 3 + 2] * 3 + 1];
					}
					else
					{
						Triangle.TextureCoordinate0.X = 0.0f;
						Triangle.TextureCoordinate0.Y = 0.0f;

						Triangle.TextureCoordinate2.X = 0.0f;
						Triangle.TextureCoordinate2.Y = 0.0f;

						Triangle.TextureCoordinate1.X = 0.0f;
						Triangle.TextureCoordinate1.Y = 0.0f;
					}

					// Process normals.
					if(AttribInfoNormals.exists)
					{
						Triangle.Normal0.X = Normals[VertexList[TriangleIdx * 3 + 0] * 3 + 0];
						Triangle.Normal0.Z = Normals[VertexList[TriangleIdx * 3 + 0] * 3 + 1];
						Triangle.Normal0.Y = Normals[VertexList[TriangleIdx * 3 + 0] * 3 + 2];

						Triangle.Normal2.X = Normals[VertexList[TriangleIdx * 3 + 1] * 3 + 0];
						Triangle.Normal2.Z = Normals[VertexList[TriangleIdx * 3 + 1] * 3 + 1];
						Triangle.Normal2.Y = Normals[VertexList[TriangleIdx * 3 + 1] * 3 + 2];

						Triangle.Normal1.X = Normals[VertexList[TriangleIdx * 3 + 2] * 3 + 0];
						Triangle.Normal1.Z = Normals[VertexList[TriangleIdx * 3 + 2] * 3 + 1];
						Triangle.Normal1.Y = Normals[VertexList[TriangleIdx * 3 + 2] * 3 + 2];
					}
					else
					{
						Triangle.Normal0.X = 0.0f;
						Triangle.Normal0.Z = 0.0f;
						Triangle.Normal0.Y = 0.0f;

						Triangle.Normal2.X = 0.0f;
						Triangle.Normal2.Z = 0.0f;
						Triangle.Normal2.Y = 0.0f;

						Triangle.Normal1.X = 0.0f;
						Triangle.Normal1.Z = 0.0f;
						Triangle.Normal1.Y = 0.0f;
					}

					// Process tangents.
					if(AttribInfoTangents.exists)
					{
						Triangle.Tangent0.X = 0.0f;
						Triangle.Tangent0.Z = 0.0f;
						Triangle.Tangent0.Y = 0.0f;

						Triangle.Tangent2.X = 0.0f;
						Triangle.Tangent2.Z = 0.0f;
						Triangle.Tangent2.Y = 0.0f;

						Triangle.Tangent1.X = 0.0f;
						Triangle.Tangent1.Z = 0.0f;
						Triangle.Tangent1.Y = 0.0f;
					}

					// Process colors.
					if(AttribInfoColors.exists)
					{
						Triangle.Color0.R = Colors[VertexList[TriangleIdx * 3 + 0] * 3 + 0];
						Triangle.Color0.G = Colors[VertexList[TriangleIdx * 3 + 0] * 3 + 1];
						Triangle.Color0.B = Colors[VertexList[TriangleIdx * 3 + 0] * 3 + 2];

						Triangle.Color2.R = Colors[VertexList[TriangleIdx * 3 + 1] * 3 + 0];
						Triangle.Color2.G = Colors[VertexList[TriangleIdx * 3 + 1] * 3 + 1];
						Triangle.Color2.B = Colors[VertexList[TriangleIdx * 3 + 1] * 3 + 2];

						Triangle.Color1.R = Colors[VertexList[TriangleIdx * 3 + 2] * 3 + 0];
						Triangle.Color1.G = Colors[VertexList[TriangleIdx * 3 + 2] * 3 + 1];
						Triangle.Color1.B = Colors[VertexList[TriangleIdx * 3 + 2] * 3 + 2];

						if(4 == AttribInfoColors.tupleSize)
						{
							Triangle.Color0.A = Colors[VertexList[TriangleIdx * 3 + 0] * 3 + 3];
							Triangle.Color2.A = Colors[VertexList[TriangleIdx * 3 + 1] * 3 + 3];
							Triangle.Color1.A = Colors[VertexList[TriangleIdx * 3 + 2] * 3 + 3];
						}
						else
						{
							Triangle.Color0.A = 1.0f;
							Triangle.Color2.A = 1.0f;
							Triangle.Color1.A = 1.0f;
						}
					}
					else
					{
						Triangle.Color0.R = 0.0f;
						Triangle.Color0.G = 0.0f;
						Triangle.Color0.B = 0.0f;
						Triangle.Color0.A = 1.0f;

						Triangle.Color2.R = 0.0f;
						Triangle.Color2.G = 0.0f;
						Triangle.Color2.B = 0.0f;
						Triangle.Color2.A = 1.0f;

						Triangle.Color1.R = 0.0f;
						Triangle.Color1.G = 0.0f;
						Triangle.Color1.B = 0.0f;
						Triangle.Color1.A = 1.0f;
					}

					// Add triangle vertices to list of vertices for given geo.
					HoudiniAssetObjectGeo->AddTriangleVertices(Triangle);
				}

				// Retrieve group memberships for this part.
				for(int GroupIdx = 0; GroupIdx < GroupNames.size(); ++GroupIdx)
				{
					GroupMembership.clear();
					if(!FHoudiniEngineUtils::HapiGetGroupMembership(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id, HAPI_GROUPTYPE_PRIM, 
						GroupNames[GroupIdx], GroupMembership))
					{
						continue;
					}

					int GroupMembershipCount = FHoudiniEngineUtils::HapiCheckGroupMembership(GroupMembership);
					if(!GroupMembershipCount)
					{
						// No group membership, skip to next group.
						continue;
					}

					TArray<int32> GroupTriangles;
					GroupTriangles.Reserve(GroupMembershipCount * 3);

					for(int FaceIdx = 0; FaceIdx < PartInfo.faceCount; ++FaceIdx)
					{
						if(GroupMembership[FaceIdx])
						{
							GroupTriangles.Add(FaceIdx * 3 + 0);
							GroupTriangles.Add(FaceIdx * 3 + 1);
							GroupTriangles.Add(FaceIdx * 3 + 2);
						}
					}

					// We need to construct a new geo part with indexing information.
					FHoudiniAssetObjectGeoPart* HoudiniAssetObjectGeoPart = new FHoudiniAssetObjectGeoPart(GroupTriangles);
					HoudiniAssetObjectGeo->AddGeoPart(HoudiniAssetObjectGeoPart);
				}
			}

			// There has been an error, continue onto next Geo.
			if(bGeoError)
			{
				continue;
			}
		}
	}

	return true;
}
