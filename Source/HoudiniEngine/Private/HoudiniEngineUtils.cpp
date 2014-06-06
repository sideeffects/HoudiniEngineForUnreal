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
		switch (Result)
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
				ASSUME(0);
			}
		};
	}
}


const FString 
FHoudiniEngineUtils::GetErrorDescription()
{
	int StatusBufferLength = 0;
	HAPI_GetStatusStringBufLength(HAPI_STATUS_RESULT, &StatusBufferLength);
	
	std::vector<char> StatusStringBuffer(StatusBufferLength, 0);
	HAPI_GetStatusString(HAPI_STATUS_RESULT, &StatusStringBuffer[0]);
	
	return FString(ANSI_TO_TCHAR(&StatusStringBuffer[0]));
}


bool
FHoudiniEngineUtils::IsInitialized()
{
	return(HAPI_RESULT_SUCCESS == HAPI_IsInitialized());
}


bool
FHoudiniEngineUtils::GetAssetName(int AssetName, std::string& AssetNameString)
{
	if(AssetName < 0)
	{
		return false;
	}

	// For now we will load first asset only.
	int AssetNameLength = 0;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetStringBufLength(AssetName, &AssetNameLength), false);

	if(AssetNameLength)
	{
		// Retrieve name of first asset.
		std::vector<char> AssetNameBuffer(AssetNameLength, '\0');
		HOUDINI_CHECK_ERROR_RETURN(HAPI_GetString(AssetName, &AssetNameBuffer[0], AssetNameLength), false);

		// Create and return string.
		AssetNameString = std::string(AssetNameBuffer.begin(), AssetNameBuffer.end());
	}

	return true;
}


bool
FHoudiniEngineUtils::GetAssetName(int AssetName, FString& AssetNameString)
{
	std::string AssetNamePlain;

	if(FHoudiniEngineUtils::GetAssetName(AssetName, AssetNamePlain))
	{
		AssetNameString = ANSI_TO_TCHAR(AssetNamePlain.c_str());
		return true;
	}

	return false;
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

	// Retrieve texture coordinate information. (points too, prim)
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAttributeInfo(AssetId, 0, 0, 0, "uv", HAPI_ATTROWNER_VERTEX, &AttribInfo), false);
	UVs.resize(AttribInfo.count * AttribInfo.tupleSize);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAttributeFloatData(AssetId, 0, 0, 0, "uv", &AttribInfo, &UVs[0], 0, AttribInfo.count), false);

	/*
	// Retrieve normals information. (points $ prims)
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAttributeInfo(AssetId, 0, 0, 0, "N", HAPI_ATTROWNER_VERTEX, &AttribInfo), false);
	Normals.resize(AttribInfo.count * AttribInfo.tupleSize);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAttributeFloatData(AssetId, 0, 0, 0, "N", &AttribInfo, &Normals[0], 0, AttribInfo.count), false);
	*/

	// Transfer data.
	for(int TriangleIdx = 0; TriangleIdx < PartInfo.vertexCount / 3; ++TriangleIdx)
	{
		FHoudiniMeshTriangle Triangle;

		// Process position information.
		// Need to flip the Y with the Z since UE4 is Z-up.
		// Need to flip winding order also.

		Triangle.Vertex0.X = Positions[VertexList[TriangleIdx * 3 + 0] * 3 + 0] * 100.0f;
		Triangle.Vertex0.Z = Positions[VertexList[TriangleIdx * 3 + 0] * 3 + 1] * 100.0f;
		Triangle.Vertex0.Y = Positions[VertexList[TriangleIdx * 3 + 0] * 3 + 2] * 100.0f;
		UpdateBoundingVolumeExtent(Triangle.Vertex0, ExtentMin, ExtentMax);

		Triangle.Vertex2.X = Positions[VertexList[TriangleIdx * 3 + 1] * 3 + 0] * 100.0f;
		Triangle.Vertex2.Z = Positions[VertexList[TriangleIdx * 3 + 1] * 3 + 1] * 100.0f;
		Triangle.Vertex2.Y = Positions[VertexList[TriangleIdx * 3 + 1] * 3 + 2] * 100.0f;
		UpdateBoundingVolumeExtent(Triangle.Vertex2, ExtentMin, ExtentMax);

		Triangle.Vertex1.X = Positions[VertexList[TriangleIdx * 3 + 2] * 3 + 0] * 100.0f;
		Triangle.Vertex1.Z = Positions[VertexList[TriangleIdx * 3 + 2] * 3 + 1] * 100.0f;
		Triangle.Vertex1.Y = Positions[VertexList[TriangleIdx * 3 + 2] * 3 + 2] * 100.0f;	
		UpdateBoundingVolumeExtent(Triangle.Vertex1, ExtentMin, ExtentMax);

		// Process texture information.
		// Need to flip the U coordinate.
		Triangle.TextureCoordinate0.X = UVs[TriangleIdx * 9 + 0];
		Triangle.TextureCoordinate0.Y = 1.0f - UVs[TriangleIdx * 9 + 1];

		Triangle.TextureCoordinate2.X = UVs[TriangleIdx * 9 + 3];
		Triangle.TextureCoordinate2.Y = 1.0f - UVs[TriangleIdx * 9 + 4];

		Triangle.TextureCoordinate1.X = UVs[TriangleIdx * 9 + 6];
		Triangle.TextureCoordinate1.Y = 1.0f - UVs[TriangleIdx * 9 + 7];

		/*
		// Process normals.
		Triangle.Normal0.X = Normals[VertexList[TriangleIdx * 3 + 0] * 3 + 0];
		Triangle.Normal0.Z = Normals[VertexList[TriangleIdx * 3 + 0] * 3 + 1];
		Triangle.Normal0.Y = Normals[VertexList[TriangleIdx * 3 + 0] * 3 + 2];

		Triangle.Normal2.X = Normals[VertexList[TriangleIdx * 3 + 1] * 3 + 0];
		Triangle.Normal2.Z = Normals[VertexList[TriangleIdx * 3 + 1] * 3 + 1];
		Triangle.Normal2.Y = Normals[VertexList[TriangleIdx * 3 + 1] * 3 + 2];

		Triangle.Normal1.X = Normals[VertexList[TriangleIdx * 3 + 2] * 3 + 0];
		Triangle.Normal1.Z = Normals[VertexList[TriangleIdx * 3 + 2] * 3 + 1];
		Triangle.Normal1.Y = Normals[VertexList[TriangleIdx * 3 + 2] * 3 + 2];
		*/

		// Store the extracted triangle.
		Geometry.Push(Triangle);
	}

	// Create bounding volume information.
	SphereBounds = FBoxSphereBounds(FBox(ExtentMin, ExtentMax));

	return true;
}
