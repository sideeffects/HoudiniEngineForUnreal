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


void
FHoudiniEngineUtils::CreateCachedMaps(const TArray<FHoudiniAssetObjectGeo*>& PreviousObjectGeos,
									  TMap<HAPI_ObjectId, TMap<HAPI_GeoId, TMap<HAPI_PartId, FHoudiniAssetObjectGeo*> > >& CachedObjectMap)
{
	for(int32 Idx = 0; Idx < PreviousObjectGeos.Num(); ++Idx)
	{
		FHoudiniAssetObjectGeo* Geo = PreviousObjectGeos[Idx];

		// We need to add Object -> mapping.
		TMap<HAPI_GeoId, TMap<HAPI_PartId, FHoudiniAssetObjectGeo*> >& ObjectMap = CachedObjectMap.FindOrAdd(Geo->ObjectId);

		// We need to add Geo -> mapping.
		TMap<HAPI_PartId, FHoudiniAssetObjectGeo*>& PartMap = ObjectMap.FindOrAdd(Geo->GeoId);

		// Insert Part -> Houdini geo mapping.
		PartMap.Add(Geo->PartId, Geo);
	}
}


FHoudiniAssetObjectGeo*
FHoudiniEngineUtils::RetrieveCachedHoudiniObjectGeo(HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, HAPI_PartId PartId,
													const TMap<HAPI_ObjectId, TMap<HAPI_GeoId, TMap<HAPI_PartId, FHoudiniAssetObjectGeo*> > >& CachedMap)
{
	FHoudiniAssetObjectGeo* Geo = nullptr;

	if(CachedMap.Contains(ObjectId))
	{
		const TMap<HAPI_GeoId, TMap<HAPI_PartId, FHoudiniAssetObjectGeo*> >& ObjectMap = CachedMap[ObjectId];
		if(ObjectMap.Contains(GeoId))
		{
			const TMap<HAPI_PartId, FHoudiniAssetObjectGeo*>& PartMap = ObjectMap[GeoId];
			if(PartMap.Contains(PartId))
			{
				Geo = PartMap[PartId];
			}
		}
	}

	return Geo;
}


FHoudiniAssetObjectGeo*
FHoudiniEngineUtils::ConstructLogoGeo()
{
	// Create new geo object.
	FHoudiniAssetObjectGeo* ObjectGeo = new FHoudiniAssetObjectGeo();

	// Used for computation of bounding volume.
	FVector ExtentMin(TNumericLimits<float>::Max(), TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
	FVector ExtentMax(TNumericLimits<float>::Min(), TNumericLimits<float>::Min(), TNumericLimits<float>::Min());

	// Bounding volume.
	FBoxSphereBounds SphereBounds(FBox(-FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX, FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX));

	// Compute number of triangles in Houdini logo.
	int VertexIndexCount = sizeof(FHoudiniLogo::VertexIndices) / sizeof(int);
	
	// Geometry scale factor.
	static const float ScaleFactor = 25.0f;
	static const FColor VertexColor(255, 165, 0);

	int VertexIndexLookup;

	TArray<int32> TriangleIndices;
	TriangleIndices.Reserve(VertexIndexCount);

	// Construct triangle data.
	for(int VertexIndex = 0; VertexIndex < VertexIndexCount; VertexIndex += 3)
	{
		FHoudiniMeshTriangle Triangle;

		// Add indices.

		// Set positions.
		VertexIndexLookup = FHoudiniLogo::VertexIndices[VertexIndex + 0] - 1;
		Triangle.Vertex0.X = FHoudiniLogo::Vertices[VertexIndexLookup * 3 + 0] * ScaleFactor;
		Triangle.Vertex0.Z = FHoudiniLogo::Vertices[VertexIndexLookup * 3 + 1] * ScaleFactor;
		Triangle.Vertex0.Y = FHoudiniLogo::Vertices[VertexIndexLookup * 3 + 2] * ScaleFactor;

		TriangleIndices.Add(VertexIndex + 0);
		UpdateBoundingVolumeExtent(Triangle.Vertex0, ExtentMin, ExtentMax);

		VertexIndexLookup = FHoudiniLogo::VertexIndices[VertexIndex + 1] - 1;
		Triangle.Vertex1.X = FHoudiniLogo::Vertices[VertexIndexLookup * 3 + 0] * ScaleFactor;
		Triangle.Vertex1.Z = FHoudiniLogo::Vertices[VertexIndexLookup * 3 + 1] * ScaleFactor;
		Triangle.Vertex1.Y = FHoudiniLogo::Vertices[VertexIndexLookup * 3 + 2] * ScaleFactor;

		TriangleIndices.Add(VertexIndex + 1);
		UpdateBoundingVolumeExtent(Triangle.Vertex1, ExtentMin, ExtentMax);

		VertexIndexLookup = FHoudiniLogo::VertexIndices[VertexIndex + 2] - 1;
		Triangle.Vertex2.X = FHoudiniLogo::Vertices[VertexIndexLookup * 3 + 0] * ScaleFactor;
		Triangle.Vertex2.Z = FHoudiniLogo::Vertices[VertexIndexLookup * 3 + 1] * ScaleFactor;
		Triangle.Vertex2.Y = FHoudiniLogo::Vertices[VertexIndexLookup * 3 + 2] * ScaleFactor;

		TriangleIndices.Add(VertexIndex + 2);
		UpdateBoundingVolumeExtent(Triangle.Vertex2, ExtentMin, ExtentMax);

		// Set colors.
		Triangle.Color0 = VertexColor;
		Triangle.Color1 = VertexColor;
		Triangle.Color2 = VertexColor;

		// Set normals.
		Triangle.Normal0.X = 0.0f;
		Triangle.Normal0.Z = 0.0f;
		Triangle.Normal0.Y = 0.0f;

		Triangle.Normal2.X = 0.0f;
		Triangle.Normal2.Z = 0.0f;
		Triangle.Normal2.Y = 0.0f;

		Triangle.Normal1.X = 0.0f;
		Triangle.Normal1.Z = 0.0f;
		Triangle.Normal1.Y = 0.0f;

		// Set texture coordinates.
		Triangle.TextureCoordinate0.X = 0.0f;
		Triangle.TextureCoordinate0.Y = 0.0f;

		Triangle.TextureCoordinate2.X = 0.0f;
		Triangle.TextureCoordinate2.Y = 0.0f;

		Triangle.TextureCoordinate1.X = 0.0f;
		Triangle.TextureCoordinate1.Y = 0.0f;

		// Set tangents.
		Triangle.Tangent0.X = 0.0f;
		Triangle.Tangent0.Z = 0.0f;
		Triangle.Tangent0.Y = 0.0f;

		Triangle.Tangent2.X = 0.0f;
		Triangle.Tangent2.Z = 0.0f;
		Triangle.Tangent2.Y = 0.0f;

		Triangle.Tangent1.X = 0.0f;
		Triangle.Tangent1.Z = 0.0f;
		Triangle.Tangent1.Y = 0.0f;

		// Add vertices to this geo.
		ObjectGeo->AddTriangleVertices(Triangle);
	}

	// Compute bounding volume.
	SphereBounds = FBoxSphereBounds(FBox(ExtentMin, ExtentMax));

	// Create single geo part.
	FHoudiniAssetObjectGeoPart* ObjectGeoPart = new FHoudiniAssetObjectGeoPart(TriangleIndices, nullptr);
	ObjectGeoPart->SetBoundingVolume(SphereBounds);
	ObjectGeo->AddGeoPart(ObjectGeoPart);

	// Compute aggregate bounding volume for this geo.
	ObjectGeo->ComputeAggregateBoundingVolume();

	return ObjectGeo;
}


bool
FHoudiniEngineUtils::ConstructGeos(HAPI_AssetId AssetId, UPackage* Package, TArray<FHoudiniAssetObjectGeo*>& PreviousObjectGeos, TArray<FHoudiniAssetObjectGeo*>& NewObjectGeos)
{
	// Make sure asset id is valid.
	if(AssetId < 0)
	{
		return false;
	}

	// Caching map.
	TMap<HAPI_ObjectId, TMap<HAPI_GeoId, TMap<HAPI_PartId, FHoudiniAssetObjectGeo*> > > CachedObjectMap;
	FHoudiniEngineUtils::CreateCachedMaps(PreviousObjectGeos, CachedObjectMap);

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

	// Retrieve asset name for material and texture name generation.
	FString AssetName;
	FHoudiniEngineUtils::GetHoudiniAssetName(AssetId, AssetName);

	// Update context for generated materials (will trigger when object goes out of scope).
	FMaterialUpdateContext MaterialUpdateContext;

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

				// Look up Houdini geo from previous cook.
				FHoudiniAssetObjectGeo* HoudiniAssetObjectGeo = FHoudiniEngineUtils::RetrieveCachedHoudiniObjectGeo(ObjectInfo.id, GeoInfo.id, PartInfo.id, CachedObjectMap);;
				UHoudiniAssetMaterial* Material = nullptr;

				if(GeoInfo.hasGeoChanged)
				{
					if(HoudiniAssetObjectGeo && !HoudiniAssetObjectGeo->UsesMultipleMaterials())
					{
						// Geometry has changed, but see if we have material.
						Material = Cast<UHoudiniAssetMaterial>(HoudiniAssetObjectGeo->GetSingleMaterial());
					}

					// If geo has changed, we need to create a new object as we cannot reuse a previous one.
					HoudiniAssetObjectGeo = new FHoudiniAssetObjectGeo(TransformMatrix, ObjectInfo.id, GeoInfo.id, PartInfo.id);
				}
				else
				{
					// Geo has not changed, we can reuse it.

					if(HoudiniAssetObjectGeo)
					{
						PreviousObjectGeos.RemoveSingleSwap(HoudiniAssetObjectGeo);
					}
					else
					{
						// We failed to look up geo from previous cook, this should not happen in theory. Create new one.
						HoudiniAssetObjectGeo = new FHoudiniAssetObjectGeo(TransformMatrix, ObjectInfo.id, GeoInfo.id, PartInfo.id);
						check(false);
					}
				}

				// Add geo to list of output geos.
				NewObjectGeos.Add(HoudiniAssetObjectGeo);

				if(GeoInfo.hasGeoChanged)
				{
					if(PartInfo.vertexCount <= 0)
					{
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

					// Keep track of whether we have UVs. 
					bool bHaveUVs = true;

					// Transfer vertex data into vertex buffer for this geo object.
					for(int TriangleIdx = 0; TriangleIdx < PartInfo.faceCount; ++TriangleIdx)
					{
						FHoudiniMeshTriangle Triangle;

						// Process position information.
						// Need to flip the Y with the Z since UE4 is Z-up.
						// Need to flip winding order also.

						Triangle.Vertex0.X = Positions[VertexList[TriangleIdx * 3 + 0] * 3 + 0] * FHoudiniEngineUtils::ScaleFactorPosition;
						Triangle.Vertex0.Z = Positions[VertexList[TriangleIdx * 3 + 0] * 3 + 1] * FHoudiniEngineUtils::ScaleFactorPosition;
						Triangle.Vertex0.Y = Positions[VertexList[TriangleIdx * 3 + 0] * 3 + 2] * FHoudiniEngineUtils::ScaleFactorPosition;
						//UpdateBoundingVolumeExtent(Triangle.Vertex0, ExtentMin, ExtentMax);

						Triangle.Vertex2.X = Positions[VertexList[TriangleIdx * 3 + 1] * 3 + 0] * FHoudiniEngineUtils::ScaleFactorPosition;
						Triangle.Vertex2.Z = Positions[VertexList[TriangleIdx * 3 + 1] * 3 + 1] * FHoudiniEngineUtils::ScaleFactorPosition;
						Triangle.Vertex2.Y = Positions[VertexList[TriangleIdx * 3 + 1] * 3 + 2] * FHoudiniEngineUtils::ScaleFactorPosition;
						//UpdateBoundingVolumeExtent(Triangle.Vertex2, ExtentMin, ExtentMax);

						Triangle.Vertex1.X = Positions[VertexList[TriangleIdx * 3 + 2] * 3 + 0] * FHoudiniEngineUtils::ScaleFactorPosition;
						Triangle.Vertex1.Z = Positions[VertexList[TriangleIdx * 3 + 2] * 3 + 1] * FHoudiniEngineUtils::ScaleFactorPosition;
						Triangle.Vertex1.Y = Positions[VertexList[TriangleIdx * 3 + 2] * 3 + 2] * FHoudiniEngineUtils::ScaleFactorPosition;
						//UpdateBoundingVolumeExtent(Triangle.Vertex1, ExtentMin, ExtentMax);

						// Process texture information.
						// Need to flip the U coordinate.
						if(AttribInfoUVs.exists)
						{
							switch(AttribInfoUVs.owner)
							{
								case HAPI_ATTROWNER_VERTEX:
								{
									// If the UVs are per vertex just query directly into the UV array we filled above.

									Triangle.TextureCoordinate0.X = UVs[TriangleIdx * 3 * AttribInfoUVs.tupleSize + 0];
									Triangle.TextureCoordinate0.Y = 1.0f - UVs[TriangleIdx * 3 * AttribInfoUVs.tupleSize + 1];

									Triangle.TextureCoordinate2.X = UVs[TriangleIdx * 3 * AttribInfoUVs.tupleSize + AttribInfoUVs.tupleSize];
									Triangle.TextureCoordinate2.Y = 1.0f - UVs[TriangleIdx * 3 * AttribInfoUVs.tupleSize + AttribInfoUVs.tupleSize + 1];

									Triangle.TextureCoordinate1.X = UVs[TriangleIdx * 3 * AttribInfoUVs.tupleSize + AttribInfoUVs.tupleSize * 2];
									Triangle.TextureCoordinate1.Y = 1.0f - UVs[TriangleIdx * 3 * AttribInfoUVs.tupleSize + AttribInfoUVs.tupleSize * 2 + 1];

									break;
								}

								case HAPI_ATTROWNER_POINT:
								{
									// If the UVs are per point use the vertex list array point indices to query into
									// the UV array we filled above.

									Triangle.TextureCoordinate0.X = UVs[VertexList[TriangleIdx * 3 + 0] * AttribInfoUVs.tupleSize + 0];
									Triangle.TextureCoordinate0.Y = 1.0f - UVs[VertexList[TriangleIdx * 3 + 0] * AttribInfoUVs.tupleSize + 1];

									Triangle.TextureCoordinate2.X = UVs[VertexList[TriangleIdx * 3 + 1] * AttribInfoUVs.tupleSize + 0];
									Triangle.TextureCoordinate2.Y = 1.0f - UVs[VertexList[TriangleIdx * 3 + 1] * AttribInfoUVs.tupleSize + 1];

									Triangle.TextureCoordinate1.X = UVs[VertexList[TriangleIdx * 3 + 2] * AttribInfoUVs.tupleSize + 0];
									Triangle.TextureCoordinate1.Y = 1.0f - UVs[VertexList[TriangleIdx * 3 + 2] * AttribInfoUVs.tupleSize + 1];

									break;
								}

								default:
								{
									// UV coords were found on unknown attribute.

									Triangle.TextureCoordinate0.X = 0.0f;
									Triangle.TextureCoordinate0.Y = 0.0f;

									Triangle.TextureCoordinate2.X = 0.0f;
									Triangle.TextureCoordinate2.Y = 0.0f;

									Triangle.TextureCoordinate1.X = 0.0f;
									Triangle.TextureCoordinate1.Y = 0.0f;

									bHaveUVs = false;

									break;
								}
							}
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

						// Bake vertex transformation.
						FHoudiniEngineUtils::TransformPosition(TransformMatrix, Triangle);

						// Add triangle vertices to list of vertices for given geo.
						HoudiniAssetObjectGeo->AddTriangleVertices(Triangle);
					} // for(int TriangleIdx = 0; TriangleIdx < PartInfo.faceCount; ++TriangleIdx)

					// We need to flag presence of UVs for this geo.
					HoudiniAssetObjectGeo->SetUVsPresence(bHaveUVs);
				} // if(GeoInfo.hasGeoChanged)

				// Retrieve material information for this part.
				HAPI_MaterialInfo MaterialInfo;
				Result = HAPI_GetMaterialOnPart(AssetId, ObjectInfo.id, GeoInfo.id, PartInfo.id, &MaterialInfo);
				if(HAPI_RESULT_SUCCESS == Result && (MaterialInfo.hasChanged || !Material))
				{
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
						// Create a new material factory to create our material asset.
						UMaterialFactoryNew* MaterialFactory = new UMaterialFactoryNew(FPostConstructInitializeProperties());
						FString MaterialName = FString::Printf(TEXT("%s_material"), *AssetName);
						Material = (UHoudiniAssetMaterial*) MaterialFactory->FactoryCreateNew(UHoudiniAssetMaterial::StaticClass(), Package, *MaterialName, RF_Transient | RF_Public | RF_Standalone, NULL, GWarn);

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
								Material->AddGeneratedTexture(Texture);

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

						/*
						// Retrieve normal data.
						if(FHoudiniEngineUtils::HapiExtractImage(NodeParams[ParmNameIdx].id, MaterialInfo, ImageBuffer, "N"))
						{
							HAPI_ImageInfo ImageInfo;
							Result = HAPI_GetImageInfo(MaterialInfo.assetId, MaterialInfo.id, &ImageInfo);
						}
						*/
					}
				}

				if(GeoInfo.hasGeoChanged)
				{
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

						// Bounding volume.
						FBoxSphereBounds SphereBounds(FBox(-FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX, FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX));

						// We need to construct a new geo part with indexing information.
						FHoudiniAssetObjectGeoPart* HoudiniAssetObjectGeoPart = new FHoudiniAssetObjectGeoPart(GroupTriangles, Material);
						HoudiniAssetObjectGeoPart->SetBoundingVolume(SphereBounds);
						HoudiniAssetObjectGeo->AddGeoPart(HoudiniAssetObjectGeoPart);
					}

					// Compute aggregate bounding volume for this geo.
					HoudiniAssetObjectGeo->ComputeAggregateBoundingVolume();

					// Compute whether this geo uses multiple materials.
					HoudiniAssetObjectGeo->ComputeMultipleMaterialUsage();
				} // if(GeoInfo.hasGeoChanged)
				else
				{
					// Geometry has not changed. Check if material has changed and needs replacing.
					if(MaterialInfo.hasChanged)
					{
						HoudiniAssetObjectGeo->ReplaceMaterial(Material);
						
						// Compute whether this geo uses multiple materials.
						HoudiniAssetObjectGeo->ComputeMultipleMaterialUsage();
					}
				}
			} // for(int32 PartIdx = 0; PartIdx < GeoInfo.partCount; ++PartIdx)

			// There has been an error, continue onto next Geo.
			if(bGeoError)
			{
				continue;
			}
		}
	}

	return true;
}


void
FHoudiniEngineUtils::TransformPosition(const FMatrix& TransformMatrix, FHoudiniMeshTriangle& Triangle)
{
	FVector4 Position;

	// Transform position of first vertex.
	{
		Position.X = Triangle.Vertex0.X;
		Position.Z = Triangle.Vertex0.Z;
		Position.Y = Triangle.Vertex0.Y;
		Position.W = 1.0f;

		Position = TransformMatrix.TransformFVector4(Position);

		Triangle.Vertex0.X = Position.X;
		Triangle.Vertex0.Z = Position.Z;
		Triangle.Vertex0.Y = Position.Y;
	}

	// Transform position of second vertex.
	{
		Position.X = Triangle.Vertex2.X;
		Position.Z = Triangle.Vertex2.Z;
		Position.Y = Triangle.Vertex2.Y;
		Position.W = 1.0f;

		Position = TransformMatrix.TransformFVector4(Position);

		Triangle.Vertex2.X = Position.X;
		Triangle.Vertex2.Z = Position.Z;
		Triangle.Vertex2.Y = Position.Y;
	}

	// Transform position of third vertex.
	{
		Position.X = Triangle.Vertex1.X;
		Position.Z = Triangle.Vertex1.Z;
		Position.Y = Triangle.Vertex1.Y;
		Position.W = 1.0f;

		Position = TransformMatrix.TransformFVector4(Position);

		Triangle.Vertex1.X = Position.X;
		Triangle.Vertex1.Z = Position.Z;
		Triangle.Vertex1.Y = Position.Y;
	}
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

	// See if we have texture coordinates to upload (for now upload only first channel).
	for(int32 MeshTexCoordIdx = 0; MeshTexCoordIdx < MAX_MESH_TEXTURE_COORDS; ++MeshTexCoordIdx)
	{
		int32 StaticMeshUVCount = RawMesh.WedgeTexCoords[MeshTexCoordIdx].Num();

		if(StaticMeshUVCount > 0)
		{
			const TArray<FVector2D>& RawMeshUVs = RawMesh.WedgeTexCoords[0];
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

	// See if we have normals to upload.
	int32 StaticMeshNormalCount = RawMesh.WedgeTangentZ.Num();
	if(StaticMeshNormalCount)
	{
		const TArray<FVector>& RawMeshNormals = RawMesh.WedgeTangentZ;
		std::vector<float> StaticMeshNormals(RawMeshNormals.Num() * 3);
		for(int32 NormalIdx = 0; NormalIdx < StaticMeshNormalCount; ++NormalIdx)
		{
			StaticMeshNormals[NormalIdx * 3 + 0] = RawMeshNormals[NormalIdx].X;
			StaticMeshNormals[NormalIdx * 3 + 1] = RawMeshNormals[NormalIdx].Y;
			StaticMeshNormals[NormalIdx * 3 + 2] = RawMeshNormals[NormalIdx].Z;
		}

		// Create attribute for normals
		HAPI_AttributeInfo AttributeInfoVertex = HAPI_AttributeInfo_Create();
		AttributeInfoVertex.count = StaticMeshNormalCount;
		AttributeInfoVertex.tupleSize = 3; 
		AttributeInfoVertex.exists = true;
		AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
		AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
		HOUDINI_CHECK_ERROR_RETURN(HAPI_AddAttribute(ConnectedAssetId, 0, 0, HAPI_ATTRIB_NORMAL, &AttributeInfoVertex), false);

		// Upload normal data.
		HOUDINI_CHECK_ERROR_RETURN(HAPI_SetAttributeFloatData(ConnectedAssetId, 0, 0, HAPI_ATTRIB_NORMAL, &AttributeInfoVertex, 
														  &StaticMeshNormals[0], 0, AttributeInfoVertex.count), false);
	}

	// Extract indices from static mesh.
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
FHoudiniEngineUtils::BakeCreatePackageForStaticMesh(UHoudiniAsset* HoudiniAsset, FString& MeshName, FGuid& BakeGUID, int32 ObjectIdx)
{
	UPackage* Package = nullptr;
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

		PackageName = FPackageName::GetLongPackagePath(HoudiniAsset->GetOutermost()->GetName()) + TEXT("/") + MeshName;
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


bool
FHoudiniEngineUtils::BakeCheckUVsPresence(const TArray<FHoudiniAssetObjectGeo*>& ObjectGeos)
{
	for(int32 GeoIdx = 0; GeoIdx < ObjectGeos.Num(); ++GeoIdx)
	{
		FHoudiniAssetObjectGeo* Geo = ObjectGeos[GeoIdx];
		if(!Geo->HasUVs())
		{
			return false;
		}
	}

	return true;
}


void
FHoudiniEngineUtils::CreateStaticMeshes(UHoudiniAsset* HoudiniAsset, const TArray<FHoudiniAssetObjectGeo*>& ObjectGeos, 
										TArray<UStaticMesh*>& StaticMeshes, bool bSplit)
{
	// Array of packages for each static mesh we are creating.
	TArray<UPackage*> Packages;
	FGuid BakeGUID;

	UPackage* Package = nullptr;
	int32 MeshCounter = 0;
	FString MeshName;

	// These maps are only used when we are baking separate meshes.
	TMap<UStaticMesh*, FHoudiniAssetObjectGeo*> StaticMeshGeos;
	TMap<UStaticMesh*, FHoudiniAssetObjectGeoPart*> StaticMeshGeoParts;

	// Get platform manager LOD specific information.
	ITargetPlatform* CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	check(CurrentPlatform);
	const FStaticMeshLODGroup& LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(NAME_None);
	int32 NumLODs = LODGroup.GetDefaultNumLODs();

	if(bSplit)
	{
		for(int32 GeoIdx = 0; GeoIdx < ObjectGeos.Num(); ++GeoIdx)
		{
			FHoudiniAssetObjectGeo* Geo = ObjectGeos[GeoIdx];
			for(int32 GeoPartIdx = 0; GeoPartIdx < Geo->HoudiniAssetObjectGeoParts.Num(); ++GeoPartIdx)
			{
				FHoudiniAssetObjectGeoPart* GeoPart = Geo->HoudiniAssetObjectGeoParts[GeoPartIdx];

				// Generate packages for each geo part - we are saving everything separately.
				Package = BakeCreatePackageForStaticMesh(HoudiniAsset, MeshName, BakeGUID, MeshCounter);
				Packages.Add(Package);
				MeshCounter++;

				// Create static mesh.
				UStaticMesh* StaticMesh = new(Package, FName(*MeshName), RF_Standalone | RF_Public) UStaticMesh(FPostConstructInitializeProperties());
				StaticMeshes.Add(StaticMesh);

				// Store mapping.
				StaticMeshGeos.Add(StaticMesh, Geo);
				StaticMeshGeoParts.Add(StaticMesh, GeoPart);
			}
		}
	}
	else
	{
		// Generate one package for everything - we are saving all geometry inside one mesh.
		Package = BakeCreatePackageForStaticMesh(HoudiniAsset, MeshName, BakeGUID);
		Packages.Add(Package);

		// Create static mesh.
		UStaticMesh* StaticMesh = new(Package, FName(*MeshName), RF_Standalone | RF_Public) UStaticMesh(FPostConstructInitializeProperties());
		StaticMeshes.Add(StaticMesh);
	}

	for(UStaticMesh* StaticMesh : StaticMeshes)
	{
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

		// Get geo and part mappings for each mesh.
		FHoudiniAssetObjectGeo* StaticMeshGeo = (StaticMeshGeos.Num() > 0) ? StaticMeshGeos[StaticMesh] : nullptr;
		FHoudiniAssetObjectGeoPart* StaticMeshGeoPart = (StaticMeshGeoParts.Num() > 0) ? StaticMeshGeoParts[StaticMesh] : nullptr;

		// Calculate number of wedges.
		int32 WedgeCount = 0;

		if(bSplit)
		{
			// If we are splitting into multiple meshes.
			WedgeCount = StaticMeshGeoPart->GetIndexCount();
		}
		else
		{
			// We are not splitting - baking one mesh.
			for(int32 GeoIdx = 0; GeoIdx < ObjectGeos.Num(); ++GeoIdx)
			{
				FHoudiniAssetObjectGeo* Geo = ObjectGeos[GeoIdx];
				for(int32 GeoPartIdx = 0; GeoPartIdx < Geo->HoudiniAssetObjectGeoParts.Num(); ++GeoPartIdx)
				{
					FHoudiniAssetObjectGeoPart* GeoPart = Geo->HoudiniAssetObjectGeoParts[GeoPartIdx];
					WedgeCount += GeoPart->GetIndexCount();
				}
			}
		}

		// Make sure static mesh has a new lighting guid.
		StaticMesh->LightingGuid = FGuid::NewGuid();

		// See if all of geos have UVs.
		bool bHasUVs = (bSplit) ? StaticMeshGeo->HasUVs() : FHoudiniEngineUtils::BakeCheckUVsPresence(ObjectGeos);

		// Set it to use textured lightmaps. We use coordinate 1 for UVs.
		StaticMesh->LightMapResolution = 32;
		StaticMesh->LightMapCoordinateIndex = 1;

		// Calculate number of triangles.
		int32 TriangleCount = WedgeCount / 3;

		// Reserve space for attributes.
		RawMesh.FaceMaterialIndices.AddZeroed(TriangleCount);
		RawMesh.FaceSmoothingMasks.AddZeroed(TriangleCount);

		// We are shifting vertices since if we are flattening vertex information (baking single).
		int32 IndexShift = 0;
		int32 GeosToProcess = (bSplit) ? 1 : ObjectGeos.Num();

		for(int32 GeoIdx = 0; GeoIdx < GeosToProcess; ++GeoIdx)
		{
			FHoudiniAssetObjectGeo* Geo = (bSplit) ? StaticMeshGeo : ObjectGeos[GeoIdx];

			// Get inverse transform for this Geo.
			FMatrix Transform = (bSplit) ? Geo->Transform.InverseSafe() : FMatrix::Identity;
		
			// Get vertices of this geo.
			const TArray<FDynamicMeshVertex>& Vertices = Geo->Vertices;
			for(int32 VertexIdx = 0; VertexIdx < Vertices.Num(); ++VertexIdx)
			{
				// Get vertex information at this index.
				const FDynamicMeshVertex& MeshVertex = Vertices[VertexIdx];

				// Add vertex to raw mesh, sequentially for all geos. We will need to reindex, depending on topology.

				if(bSplit)
				{
					// When generating geometry from HAPI, we bake transforms in, so we need to undo that if we are splitting.
					FVector4 Position(MeshVertex.Position.X, MeshVertex.Position.Y, MeshVertex.Position.Z, 1.0f);
					Position = Transform.TransformFVector4(Position);
					RawMesh.VertexPositions.Add(Position);
				}
				else
				{
					RawMesh.VertexPositions.Add(MeshVertex.Position);
				}

				RawMesh.WedgeColors.Add(MeshVertex.Color);
				RawMesh.WedgeTangentX.Add(MeshVertex.TangentX);
				RawMesh.WedgeTangentY.Add(MeshVertex.TangentZ);

				// First uv channel is used for diffuse/normal.
				RawMesh.WedgeTexCoords[0].Add(MeshVertex.TextureCoordinate);

				// Second uv channel is used for lightmap.
				RawMesh.WedgeTexCoords[1].Add(MeshVertex.TextureCoordinate);
			}

			// Get index information for this geo.
			int32 GeoPartsToProcess = (bSplit) ? 1 : Geo->HoudiniAssetObjectGeoParts.Num();
			for(int32 GeoPartIdx = 0; GeoPartIdx < GeoPartsToProcess; ++GeoPartIdx)
			{
				// Get part at this index.
				FHoudiniAssetObjectGeoPart* GeoPart = (bSplit) ? StaticMeshGeoPart : Geo->HoudiniAssetObjectGeoParts[GeoPartIdx];

				// Get indices for this part.
				const TArray<int32>& Indices = GeoPart->Indices;

				// Add index information to raw mesh.
				for(int32 IndexIdx = 0; IndexIdx < Indices.Num(); ++IndexIdx)
				{
					RawMesh.WedgeIndices.Add(Indices[IndexIdx] + IndexShift);
				}
			}

			// We shift by vertex number.
			IndexShift += Geo->GetVertexCount();
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
	}
}
