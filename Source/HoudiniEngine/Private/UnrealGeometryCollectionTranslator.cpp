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

#include "UnrealGeometryCollectionTranslator.h"
#include "UnrealMeshTranslator.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEnginePrivatePCH.h"
#include "Animation/AttributesContainer.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/SkeletalMesh.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"

#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionComponent.h"
//#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionDebugDrawComponent.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionActor.h"
#include "Materials/Material.h"

bool 
FUnrealGeometryCollectionTranslator::HapiCreateInputNodeForGeometryCollection(
	UGeometryCollection* GeometryCollection, HAPI_NodeId& InputNodeId, const FString& InputNodeName,
	UGeometryCollectionComponent* GeometryCollectionComponent)
{
	// If we don't have a static mesh there's nothing to do.
	if (!IsValid(GeometryCollection))
		return false;

	// Node ID for the newly created node
	HAPI_NodeId PackNodeId = -1;
	FString PackName = FString::Printf(TEXT("%s_pack"), *InputNodeName);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(
		-1, TEXT("SOP/pack"), InputNodeName, true, &PackNodeId), false);

	// Check if we have a valid id for this new input asset.
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(PackNodeId))
		return false;

	HAPI_NodeId PreviousInputNodeId = InputNodeId;

	// Update our input NodeId
	// Get our parent OBJ NodeID
	HAPI_NodeId InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(PackNodeId);

	// We have now created a valid new input node, delete the previous one
	if (PreviousInputNodeId >= 0)
	{
		// Get the parent OBJ node ID before deleting!
		HAPI_NodeId PreviousInputOBJNode = FHoudiniEngineUtils::HapiGetParentNodeId(PreviousInputNodeId);

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
			FHoudiniEngine::Get().GetSession(), PreviousInputNodeId))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input node for %s."), *InputNodeName);
		}

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
			FHoudiniEngine::Get().GetSession(), PreviousInputOBJNode))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input OBJ node for %s."), *InputNodeName);
		}		
	}


	HAPI_NodeId MergeNodeId = -1;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(
                InputObjectNodeId, TEXT("merge"), InputNodeName, true, &MergeNodeId), false);

	// Check if we have a valid id for this new input asset.
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(MergeNodeId))
		return false;
	
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
                FHoudiniEngine::Get().GetSession(),
                PackNodeId, 0, MergeNodeId, 0), false);

	UploadGeometryCollection(GeometryCollection, InputObjectNodeId, InputNodeName, MergeNodeId, GeometryCollectionComponent);

	// Setup the pack node to create packed primitives.
	{
		const char * PackByNameOption = HAPI_UNREAL_PARAM_PACK_BY_NAME;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(
                        FHoudiniEngine::Get().GetSession(), PackNodeId,
                        PackByNameOption, 0, 1), false);
	}

	{
		const char * PackedFragmentsOption = HAPI_UNREAL_PARAM_PACKED_FRAGMENTS;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(
                        FHoudiniEngine::Get().GetSession(), PackNodeId,
                        PackedFragmentsOption, 0, 0), false);
	}
	
	if (!FHoudiniEngineUtils::HapiCookNode(PackNodeId, nullptr, true))
		return false;
	
	InputNodeId = PackNodeId;
	
	return true;
}

bool 
FUnrealGeometryCollectionTranslator::SetGeometryCollectionAttributesForPart(
	UGeometryCollection* InGeometryCollection, int32 InGeometryIndex, HAPI_NodeId InNodeId, FString InName)
{
	const HAPI_PartId PartId = 0;
	
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	HOUDINI_CHECK_ERROR_RETURN(	FHoudiniApi::GetPartInfo(
                FHoudiniEngine::Get().GetSession(), InNodeId, PartId, &PartInfo), false);

	// Name attribute
	{
		// Create point attribute info.
		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		AttributeInfo.count = PartInfo.faceCount;
		AttributeInfo.tupleSize = 1;
		AttributeInfo.exists = true;
		AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
                        FHoudiniEngine::Get().GetSession(), InNodeId, PartId,
                        HAPI_ATTRIB_NAME, &AttributeInfo), false);

		// Now that we have raw positions, we can upload them for our attribute.

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
			InName, InNodeId, 0, HAPI_ATTRIB_NAME, AttributeInfo), false);
	}

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
                FHoudiniEngine::Get().GetSession(), InNodeId), false);
	
	return true;
}

bool 
FUnrealGeometryCollectionTranslator::UploadGeometryCollection(UGeometryCollection* GeometryCollectionObject,
	HAPI_NodeId InParentNodeId, FString InName, HAPI_NodeId InMergeNodeId, UGeometryCollectionComponent * GeometryCollectionComponent)
{
	if (!IsValid(GeometryCollectionObject))
	{
		return false;
	}

	// Level -> ( ParentId -> ClusterLevel )
	TMap<int32, TMap<int32, int32>> LevelToClusterArray;
	TMap<int32, int32> LevelToNewClusterIndex;

	TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
	FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get();
	check(GeometryCollection);

	// vertex information
	TManagedArray<FVector3f>& Vertex = GeometryCollection->Vertex;
	TManagedArray<FVector3f>& TangentU = GeometryCollection->TangentU;
	TManagedArray<FVector3f>& TangentV = GeometryCollection->TangentV;
	TManagedArray<FVector3f>& Normal = GeometryCollection->Normal;
	TArray<FVector2f>& UV = GeometryCollection->UVs[0];
	TManagedArray<FLinearColor>& Color = GeometryCollection->Color;
	TManagedArray<int32>& BoneMap = GeometryCollection->BoneMap;
	TManagedArray<FLinearColor>& BoneColor = GeometryCollection->BoneColor;
	TManagedArray<FString>& BoneName = GeometryCollection->BoneName;

	TManagedArray<FIntVector>& Indices = GeometryCollection->Indices;
	TManagedArray<bool>& Visible = GeometryCollection->Visible;
	TManagedArray<int32>& MaterialID = GeometryCollection->MaterialID;
	TManagedArray<int32>& MaterialIndex = GeometryCollection->MaterialIndex;

	TManagedArray<FTransform>& Transform = GeometryCollection->Transform;
	TManagedArray<int32>& Parent = GeometryCollection->Parent;
	TManagedArray<int32>& SimulationType = GeometryCollection->SimulationType;
	
	TManagedArray<int32>& TransformIndex = GeometryCollection->TransformIndex;
	TManagedArray<FBox>& BoundingBox = GeometryCollection->BoundingBox;
	TManagedArray<float>& InnerRadius = GeometryCollection->InnerRadius;
	TManagedArray<float>& OuterRadius = GeometryCollection->OuterRadius;
	TManagedArray<int32>& VertexStartArray = GeometryCollection->VertexStart;
	TManagedArray<int32>& VertexCountArray = GeometryCollection->VertexCount;
	TManagedArray<int32>& FaceStartArray = GeometryCollection->FaceStart;
	TManagedArray<int32>& FaceCountArray = GeometryCollection->FaceCount;

	TManagedArray<int32>& TransformToGeometryIndexArray = GeometryCollection->TransformToGeometryIndex;

	TManagedArray<FGeometryCollectionSection>& Sections = GeometryCollection->Sections;

	// Need to update hierarchy level otherwise sometimes level would be out of date!
	FGeometryCollectionClusteringUtility::UpdateHierarchyLevelOfChildren(GeometryCollection, -1);
	
	if (Sections.Num() == 0)
	{
		HOUDINI_LOG_ERROR(TEXT("No triangles in mesh"));
		return false;
	}

	// See: FUnrealMeshTranslator::CreateInputNodeForRawMesh for reference.
	for (int32 ObjectIndex = 0; ObjectIndex < GeometryCollection->TransformToGeometryIndex.Num(); ObjectIndex++)
	{
		int32& GeometryIndex = TransformToGeometryIndexArray[ObjectIndex];
		if (GeometryIndex == -1)
		{
			continue;
		}
		
		int32& VertexStart = VertexStartArray[GeometryIndex];
		int32& VertexCount = VertexCountArray[GeometryIndex];
		int32& FaceStart = FaceStartArray[GeometryIndex];
		int32& FaceCount = FaceCountArray[GeometryIndex];


		HAPI_NodeId GeometryNodeId = -1;
		FString OutputName = FString::Printf( TEXT("gc_piece_%d"), ObjectIndex);
		// Create the node in this input object's OBJ node
		HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode(
                        InParentNodeId, TEXT("null"), OutputName, false, &GeometryNodeId), false);
		
		HAPI_PartInfo Part;
		FHoudiniApi::PartInfo_Init(&Part);

		Part.id = 0;
		Part.nameSH = 0;
		Part.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
		Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
		Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
		Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;

		Part.vertexCount = FaceCount * 3; // In GC, "FaceCount" == number of indices.
		Part.faceCount = FaceCount; //1; ///FaceCountArray // FaceCount; //FaceCount / 3; // TODO GC: In GC indices count == faces count. This is not the same in Regular meshes. Double check this.
		Part.pointCount = VertexCount;
		Part.type = HAPI_PARTTYPE_MESH;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
                        FHoudiniEngine::Get().GetSession(), GeometryNodeId, 0, &Part), false);

		//--------------------------------------------------------------------------------------------------------------------- 
		// POSITION (P)
		//--------------------------------------------------------------------------------------------------------------------- 
		if (VertexCount > 3)
		{

			HAPI_AttributeInfo AttributeInfoPoint;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfoPoint);
			AttributeInfoPoint.count = VertexCount;
			AttributeInfoPoint.tupleSize = 3;
			AttributeInfoPoint.exists = true;
			AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
			AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
                                FHoudiniEngine::Get().GetSession(), GeometryNodeId, 0,
                                HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPoint), false);
			
			TArray<float> StaticMeshVertices;
			StaticMeshVertices.SetNumZeroed(VertexCount * 3);
			for (int32 VertexIdx = 0; VertexIdx < VertexCount; ++VertexIdx)
			{
				int32 IndexInArray = VertexStart + VertexIdx;
				const FVector & PositionVector = Vertex[IndexInArray];

				// Convert Unreal to Houdini
				StaticMeshVertices[VertexIdx * 3 + 0] = PositionVector.X / HAPI_UNREAL_SCALE_FACTOR_POSITION;
				StaticMeshVertices[VertexIdx * 3 + 1] = PositionVector.Z / HAPI_UNREAL_SCALE_FACTOR_POSITION;
				StaticMeshVertices[VertexIdx * 3 + 2] = PositionVector.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			}

			// Now that we have raw positions, we can upload them for our attribute.
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				StaticMeshVertices, GeometryNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, AttributeInfoPoint), false);
		}

		TArray<UMaterialInterface*> MaterialInterfaces;
		TArray<int32> TriangleMaterialIndices;

		int32 UEDefaultMaterialIndex = INDEX_NONE;
		
		const int32 NumMaterials = GeometryCollectionObject->Materials.Num();

		if (NumMaterials > 0)
		{
			for (int32 Index = 0; Index < NumMaterials; ++Index)
			{
				UMaterialInterface* CurrMaterial = GeometryCollectionObject->Materials[Index];

				// Possible we have a null entry - replace with default
				if (!IsValid(CurrMaterial))
				{
					CurrMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
					UEDefaultMaterialIndex = Index;
				}

				MaterialInterfaces.Add(CurrMaterial);
			}

			TriangleMaterialIndices.Reserve(Part.faceCount);
		}
 
		
		// Setup for vertex instance attributes
		int32 HoudiniVertexIdx = 0;

		const bool HasUVs = UV.Num() == Vertex.Num();
		TArray<float> UVs;

		if (HasUVs)
			UVs.SetNumZeroed(Part.vertexCount * 3);

		
		const bool HasNormals = Normal.Num() == Vertex.Num();
		TArray<float> Normals;

		if (HasNormals)
			Normals.SetNumZeroed(Part.vertexCount * 3);


		const bool HasTangentU = TangentU.Num() == Vertex.Num();
		TArray<float> Tangents;

		if (HasTangentU)
			Tangents.SetNumZeroed(Part.vertexCount * 3);


		const bool HasTangentV = TangentV.Num() == Vertex.Num();
		TArray<float> Binormals;

		if (HasTangentV)
			Binormals.SetNumZeroed(Part.vertexCount * 3);

		TArray<float> RGBColors;
		TArray<float> Alphas;
		const bool HasColor = Color.Num() == Vertex.Num();
		if (HasColor)
		{
			RGBColors.SetNumZeroed(Part.vertexCount * 3);
			Alphas.SetNumZeroed(Part.vertexCount);
		}

		TArray<int32> MeshTriangleVertexIndices;
		MeshTriangleVertexIndices.SetNumUninitialized(Part.vertexCount);
		
		// For each face:
		for (int32 i = 0; i < FaceCount; i++)
		{
			const FIntVector FaceIndices = Indices[FaceStart + i];

			for (int32 TriangleVertexIndex = 0; TriangleVertexIndex < 3; TriangleVertexIndex++)
			{
				// Swap second and third vertices due to winding
				const int32 WindingIdx = (3 - TriangleVertexIndex) % 3;
				int32 VertexIndex = FaceIndices[WindingIdx];

				// Calculate the index of the first component of a vertex instance's value in an inline float array 
				// representing vectors (3 float) per vertex instance
				const int32 Float3Index = HoudiniVertexIdx * 3;

				//--------------------------------------------------------------------------------------------------------------------- 
				// UVS (uvX)
				//--------------------------------------------------------------------------------------------------------------------- 
				if (HasUVs)
				{
					UVs[Float3Index + 0] = UV[VertexIndex].X;
					UVs[Float3Index + 1] = 1 - UV[VertexIndex].Y;
					UVs[Float3Index + 2] = 0;
				}
				
				//--------------------------------------------------------------------------------------------------------------------- 
				// NORMALS (N)
				//---------------------------------------------------------------------------------------------------------------------

				// Swapped in Z and Y due to axis conversion
				if (HasNormals)
				{
					Normals[Float3Index + 0] = Normal[VertexIndex].X;
					Normals[Float3Index + 1] = Normal[VertexIndex].Z;
					Normals[Float3Index + 2] = Normal[VertexIndex].Y;
				}

				//--------------------------------------------------------------------------------------------------------------------- 
				// TANGENT (tangentu)
				//---------------------------------------------------------------------------------------------------------------------
				if (HasTangentU)
				{
					Tangents[Float3Index + 0] = TangentU[VertexIndex].X;
					Tangents[Float3Index + 1] = TangentU[VertexIndex].Z;
					Tangents[Float3Index + 2] = TangentU[VertexIndex].Y;
				}

				//--------------------------------------------------------------------------------------------------------------------- 
				// BINORMAL (tangentv)
				//---------------------------------------------------------------------------------------------------------------------
				if (HasTangentV)
				{
					Binormals[Float3Index + 0] = TangentV[VertexIndex].X;
					Binormals[Float3Index + 1] = TangentV[VertexIndex].Z;
					Binormals[Float3Index + 2] = TangentV[VertexIndex].Y;
				}

				//--------------------------------------------------------------------------------------------------------------------- 
				// COLORS (Cd)
				//---------------------------------------------------------------------------------------------------------------------
				if (HasColor)
				{
					RGBColors[Float3Index + 0] = Color[VertexIndex].R;
					RGBColors[Float3Index + 1] = Color[VertexIndex].G;
					RGBColors[Float3Index + 2] = Color[VertexIndex].B;
					Alphas[HoudiniVertexIdx] = Color[VertexIndex].A;
				}

				//--------------------------------------------------------------------------------------------------------------------- 
				// TRIANGLE/FACE VERTEX INDICES
				//---------------------------------------------------------------------------------------------------------------------
				MeshTriangleVertexIndices[HoudiniVertexIdx] = VertexIndex - VertexStart;

				HoudiniVertexIdx++;
			}

			//--------------------------------------------------------------------------------------------------------------------- 
			// TRIANGLE MATERIAL ASSIGNMENT
			//---------------------------------------------------------------------------------------------------------------------
			const int32 MatIndex = MaterialID[FaceStart + i];
			if (MaterialInterfaces.IsValidIndex(MatIndex))
			{
				TriangleMaterialIndices.Add(MatIndex);
			}
			else
			{
				TriangleMaterialIndices.Add(UEDefaultMaterialIndex);
			}
			
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// UVS (uvX)
		//---------------------------------------------------------------------------------------------------------------------
		if (HasUVs)
		{
			// Create attribute for uvs.
			HAPI_AttributeInfo AttributeInfoVertex;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

			AttributeInfoVertex.tupleSize = 3;
			AttributeInfoVertex.count = UVs.Num() / AttributeInfoVertex.tupleSize;
			AttributeInfoVertex.exists = true;
			AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				GeometryNodeId, 0, HAPI_UNREAL_ATTRIB_UV, &AttributeInfoVertex), false);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				UVs, GeometryNodeId, 0, HAPI_UNREAL_ATTRIB_UV, AttributeInfoVertex), false);
		}
		
		//--------------------------------------------------------------------------------------------------------------------- 
		// NORMALS (N)
		//---------------------------------------------------------------------------------------------------------------------
		if (HasNormals)
		{
			// Create attribute for normals.
			HAPI_AttributeInfo AttributeInfoVertex;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

			AttributeInfoVertex.tupleSize = 3;
			AttributeInfoVertex.count = Normals.Num() / AttributeInfoVertex.tupleSize;
			AttributeInfoVertex.exists = true;
			AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				GeometryNodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoVertex), false);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				Normals, GeometryNodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL, AttributeInfoVertex), false);
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// TANGENT (tangentu)
		//---------------------------------------------------------------------------------------------------------------------
		if (HasTangentU)
		{
			// Create attribute for tangentu.
			HAPI_AttributeInfo AttributeInfoVertex;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

			AttributeInfoVertex.tupleSize = 3;
			AttributeInfoVertex.count = Tangents.Num() / AttributeInfoVertex.tupleSize;
			AttributeInfoVertex.exists = true;
			AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				GeometryNodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTU, &AttributeInfoVertex), false);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				Tangents, GeometryNodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTU, AttributeInfoVertex), false);
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// BINORMAL (tangentv)
		//---------------------------------------------------------------------------------------------------------------------
		if (HasTangentV)
		{
			// Create attribute for normals.
			HAPI_AttributeInfo AttributeInfoVertex;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

			AttributeInfoVertex.tupleSize = 3;
			AttributeInfoVertex.count = Binormals.Num() / AttributeInfoVertex.tupleSize;
			AttributeInfoVertex.exists = true;
			AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
                                FHoudiniEngine::Get().GetSession(),
                                GeometryNodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTV, &AttributeInfoVertex), false);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				Binormals, GeometryNodeId, 0, HAPI_UNREAL_ATTRIB_TANGENTV, AttributeInfoVertex), false);
		}

		//--------------------------------------------------------------------------------------------------------------------- 
		// COLORS (Cd)
		//---------------------------------------------------------------------------------------------------------------------
		if (HasColor)
		{
			// Create attribute for colors.
			HAPI_AttributeInfo AttributeInfoVertex;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);

			AttributeInfoVertex.tupleSize = 3;
			AttributeInfoVertex.count = RGBColors.Num() / AttributeInfoVertex.tupleSize;
			AttributeInfoVertex.exists = true;
			AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				GeometryNodeId, 0, HAPI_UNREAL_ATTRIB_COLOR, &AttributeInfoVertex), false);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				RGBColors, GeometryNodeId, 0, HAPI_UNREAL_ATTRIB_COLOR, AttributeInfoVertex), false);

			FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertex);
			AttributeInfoVertex.tupleSize = 1;
			AttributeInfoVertex.count = Alphas.Num();
			AttributeInfoVertex.exists = true;
			AttributeInfoVertex.owner = HAPI_ATTROWNER_VERTEX;
			AttributeInfoVertex.storage = HAPI_STORAGETYPE_FLOAT;
			AttributeInfoVertex.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				GeometryNodeId, 0, HAPI_UNREAL_ATTRIB_ALPHA, &AttributeInfoVertex), false);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
				Alphas, GeometryNodeId, 0, HAPI_UNREAL_ATTRIB_ALPHA, AttributeInfoVertex), false);
		}
			
		//--------------------------------------------------------------------------------------------------------------------- 
		// INDICES (VertexList)
		//---------------------------------------------------------------------------------------------------------------------
		if (FaceCount > 0)
		{
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetVertexList(
				MeshTriangleVertexIndices, GeometryNodeId, 0), false);
			
			// We need to generate array of face counts.
			TArray<int32> StaticMeshFaceCounts;
			StaticMeshFaceCounts.SetNumUninitialized(Part.faceCount);
			for (int32 n = 0; n < Part.faceCount; n++)
				StaticMeshFaceCounts[n] = 3;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetFaceCounts(
				StaticMeshFaceCounts, GeometryNodeId, 0), false);
		}

		// Materials - Reuse code from FHoudiniMeshTranslator
		if (NumMaterials > 0)
		{
			// List of materials, one for each face.
			TArray<FString> TriangleMaterials;

			//Lists of material parameters
			TMap<FString, TArray<float>> ScalarMaterialParameters;
			TMap<FString, TArray<float>> VectorMaterialParameters;
			TMap<FString, TArray<FString>> TextureMaterialParameters;

			bool bAttributeSuccess = false;
			bool bAddMaterialParametersAsAttributes = false;

			if (bAddMaterialParametersAsAttributes)
			{
				// Create attributes for the material and all its parameters
				// Get material attribute data, and all material parameters data
				FUnrealMeshTranslator::CreateFaceMaterialArray(
                                        MaterialInterfaces, TriangleMaterialIndices, TriangleMaterials,
                                        ScalarMaterialParameters, VectorMaterialParameters, TextureMaterialParameters);

				// Create attribute for materials and all attributes for material parameters
				bAttributeSuccess = FUnrealMeshTranslator::CreateHoudiniMeshAttributes(
					GeometryNodeId,
					0,
					TriangleMaterials.Num(),
					TriangleMaterials,
					ScalarMaterialParameters,
					VectorMaterialParameters,
					TextureMaterialParameters);
			}
			else
			{
				// Create attributes only for the materials
				// Only get the material attribute data
				FUnrealMeshTranslator::CreateFaceMaterialArray(
					MaterialInterfaces, TriangleMaterialIndices, TriangleMaterials);

				// Create attribute for materials
				bAttributeSuccess = FUnrealMeshTranslator::CreateHoudiniMeshAttributes(
					GeometryNodeId,
					0,
					TriangleMaterials.Num(),
					TriangleMaterials,
					ScalarMaterialParameters,
					VectorMaterialParameters,
					TextureMaterialParameters);
			}

			if (!bAttributeSuccess)
			{
				check(0);
				return false;
			}
		}

		// Geometry collection input attributes
		//--------------------------------------------------------------------------------------------------------------------- 
		// name (required for packing)
		//---------------------------------------------------------------------------------------------------------------------
		{
			// Create point attribute info.
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
			AttributeInfo.count = Part.faceCount;
			AttributeInfo.tupleSize = 1;
			AttributeInfo.exists = true;
			AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
			AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
			AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(), GeometryNodeId, Part.id,
				HAPI_ATTRIB_NAME, &AttributeInfo), false);

			// Upload them for our attribute.
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
				OutputName, GeometryNodeId, Part.id, HAPI_ATTRIB_NAME, AttributeInfo), false);
		}


		// Geometry collection input attributes
		//--------------------------------------------------------------------------------------------------------------------- 
		// unreal_gc_piece (required for packing)
		//---------------------------------------------------------------------------------------------------------------------
		int32 Level = 1;
	
		if (GeometryCollection->HasAttribute("Level", FGeometryCollection::TransformGroup))
		{
			TManagedArray<int32>& Levels = GeometryCollection->GetAttribute<int32>("Level", FGeometryCollection::TransformGroup);
			Level = Levels[GeometryIndex];
		}

		// If simulation type is none, then disable it
		if (SimulationType[GeometryIndex] == FGeometryCollection::ESimulationTypes::FST_None)
		{
			Level = 0;
		}

		{
			TArray<int32> GCPieceAttribute;
			GCPieceAttribute.SetNumUninitialized(Part.faceCount);
			for (int32 n = 0; n < GCPieceAttribute.Num(); n++)
				GCPieceAttribute[n] = Level;
			
			HAPI_AttributeInfo AttributeInfoPrim;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfoPrim);

			AttributeInfoPrim.count = Part.faceCount;
			AttributeInfoPrim.tupleSize = 1;
			AttributeInfoPrim.exists = true;
			AttributeInfoPrim.owner = HAPI_ATTROWNER_PRIM;
			AttributeInfoPrim.storage = HAPI_STORAGETYPE_INT;
			AttributeInfoPrim.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(), 
				GeometryNodeId,	0, HAPI_UNREAL_ATTRIB_GC_PIECE, &AttributeInfoPrim), false);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeIntData(
				GCPieceAttribute, GeometryNodeId, 0, HAPI_UNREAL_ATTRIB_GC_PIECE, AttributeInfoPrim), false);
		}

		// Identify the cluster level using the parent indices:
	
		int32 ClusterIndex = -1;
		TMap<int32, int32> & ClusterMap = LevelToClusterArray.FindOrAdd(Level);
		int32 ParentIndex = Parent[GeometryIndex];
		if (ParentIndex != FGeometryCollection::Invalid)
		{
			if (ClusterMap.Contains(ParentIndex))
			{
				ClusterIndex = ClusterMap[ParentIndex];
			}
			else
			{
				if (LevelToNewClusterIndex.Contains(Level))
				{
					LevelToNewClusterIndex[Level]++;
					ClusterIndex = LevelToNewClusterIndex[Level];
				}
				else
				{
					ClusterIndex = 0;
					LevelToNewClusterIndex.Add(Level, ClusterIndex);
				}

				ClusterMap.Add(ParentIndex, ClusterIndex);
			}
		}

		// Add the unreal_gc_cluster attribute
		{
			TArray<int32> GCClusterAttribute;
			GCClusterAttribute.SetNumUninitialized(Part.faceCount);
			for (int32 n = 0; n < GCClusterAttribute.Num(); n++)
				GCClusterAttribute[n] = ClusterIndex;
			
			HAPI_AttributeInfo AttributeInfoPrim;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfoPrim);

			AttributeInfoPrim.count = Part.faceCount;
			AttributeInfoPrim.tupleSize = 1;
			AttributeInfoPrim.exists = true;
			AttributeInfoPrim.owner = HAPI_ATTROWNER_PRIM;
			AttributeInfoPrim.storage = HAPI_STORAGETYPE_INT;
			AttributeInfoPrim.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(), 
				GeometryNodeId,	0, HAPI_UNREAL_ATTRIB_GC_CLUSTER_PIECE, &AttributeInfoPrim), false);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeIntData(
				GCClusterAttribute, GeometryNodeId, 0, HAPI_UNREAL_ATTRIB_GC_CLUSTER_PIECE, AttributeInfoPrim), false);
		}
		

		AddGeometryCollectionDetailAttributes(GeometryCollectionObject, GeometryNodeId, Part.id, Part, InName, GeometryCollectionComponent);

		// Commit the geo.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
                        FHoudiniEngine::Get().GetSession(), GeometryNodeId), false);

		// Connect the LOD node to the merge node.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
			FHoudiniEngine::Get().GetSession(),
			InMergeNodeId, GeometryIndex, GeometryNodeId, 0), false);
		
	}

	return true;
}

bool FUnrealGeometryCollectionTranslator::AddGeometryCollectionDetailAttributes(
	UGeometryCollection* GeometryCollectionObject, HAPI_NodeId GeoId, HAPI_PartId PartId, HAPI_PartInfo & Part, const FString& InName, UGeometryCollectionComponent * GeometryCollectionComponent)
{

	// unreal_gc_name
	//---------------------------------------------------------------------------------------------------------------------
	{
		// Create primitive attribute with mesh asset path
		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		AttributeInfo.count = Part.faceCount;
		AttributeInfo.tupleSize = 1;
		AttributeInfo.exists = true;
		AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			GeoId, 0, HAPI_UNREAL_ATTRIB_GC_NAME, &AttributeInfo), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
			InName, GeoId, 0, HAPI_UNREAL_ATTRIB_GC_NAME, AttributeInfo), false);
	}
	
	// Clustering - Damage thresholds
	// Damage thresholds are not yet available in vanilla 4.26.
	if (GeometryCollectionObject->DamageThreshold.Num() > 0)
	{
		TArray<float> AttributeData(GeometryCollectionObject->DamageThreshold);

		// We have one array with DamageThreshold.Num() elements.
		TArray<int32> AttributeDataSizes;
		AttributeDataSizes.SetNumUninitialized(GeometryCollectionObject->DamageThreshold.Num());
		for (int32 n = 0; n < AttributeDataSizes.Num(); n++)
			AttributeDataSizes[n] = 0;

		AttributeDataSizes[0] = GeometryCollectionObject->DamageThreshold.Num();
		
		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		AttributeInfo.count = 1;
		AttributeInfo.tupleSize = 1;
		AttributeInfo.totalArrayElements = GeometryCollectionObject->DamageThreshold.Num();
		AttributeInfo.exists = true;
		AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT_ARRAY;
		AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_CLUSTERING_DAMAGE_THRESHOLD;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId, AttributeName, &AttributeInfo), false);
		
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatArrayData(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId, AttributeName, &AttributeInfo,
			(const float*)AttributeData.GetData(), AttributeInfo.totalArrayElements, (const int*)AttributeDataSizes.GetData(), 0, AttributeInfo.count), false);
	}

	// Clustering - Cluster connection type
	{
		int32 ClusterConnectionTypeValue = 0;
		EClusterConnectionTypeEnum ClusterConnectionType = GeometryCollectionObject->ClusterConnectionType;
		if (ClusterConnectionType == EClusterConnectionTypeEnum::Chaos_PointImplicit)
		{
			ClusterConnectionTypeValue = 0;
		}
		else if (ClusterConnectionType == EClusterConnectionTypeEnum::Chaos_DelaunayTriangulation)
		{
			ClusterConnectionTypeValue = 0; // Chaos_DelaunayTriangulation not supported
		}
		else
		{
			ClusterConnectionTypeValue = (int32)ClusterConnectionType - 1;
		}
		TArray< int32 > AttributeData { ClusterConnectionTypeValue };

		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		AttributeInfo.count = 1;
		AttributeInfo.tupleSize = 1;
		AttributeInfo.exists = true;
		AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfo.storage = HAPI_STORAGETYPE_INT;
		AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_CLUSTERING_CLUSTER_CONNECTION_TYPE;
		
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId, AttributeName, &AttributeInfo), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeIntData(
			AttributeData, GeoId, PartId, AttributeName, AttributeInfo), false);
	}

	// Collisions - Mass as density
	{
		TArray< int32 > AttributeData { GeometryCollectionObject->bMassAsDensity == true ? 1 : 0 };
		
		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		AttributeInfo.count = 1;
		AttributeInfo.tupleSize = 1;
		AttributeInfo.exists = true;
		AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfo.storage = HAPI_STORAGETYPE_INT;
		AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_COLLISIONS_MASS_AS_DENSITY;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			 FHoudiniEngine::Get().GetSession(),
			 GeoId, PartId, AttributeName, &AttributeInfo), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeIntData(
			AttributeData, GeoId, PartId, AttributeName, AttributeInfo), false);
	}

	// Collisions - Mass
	{
		TArray< float > AttributeData { GeometryCollectionObject->Mass };
		
		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		AttributeInfo.count = 1;
		AttributeInfo.tupleSize = 1;
		AttributeInfo.exists = true;
		AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_COLLISIONS_MASS;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId, AttributeName, &AttributeInfo), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			AttributeData, GeoId, PartId, AttributeName, AttributeInfo), false);
	}

	// Collisions - Minimum Mass Clamp
	{
		TArray< float > AttributeData { GeometryCollectionObject->MinimumMassClamp };
		
		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		AttributeInfo.count = 1;
		AttributeInfo.tupleSize = 1;
		AttributeInfo.exists = true;
		AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT;
		AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_COLLISIONS_MINIMUM_MASS_CLAMP;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId, AttributeName, &AttributeInfo), false);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			AttributeData, GeoId, PartId, AttributeName, AttributeInfo), false);
	}
	
	const TArray<FGeometryCollectionSizeSpecificData> & GCSizeSpecDatas = GeometryCollectionObject->SizeSpecificData;

	// Size specific data input:

	// Collisions - Size specific data - Max size
	if (GCSizeSpecDatas.Num() > 0)
	{
		TArray<float> AttributeData;
		AttributeData.SetNum(GCSizeSpecDatas.Num());
		for (int32 i = 0; i < AttributeData.Num(); i++)
		{
			AttributeData[i] = GCSizeSpecDatas[i].MaxSize;
		}

		// We have one array with DamageThreshold.Num() elements.
		TArray< int32 > AttributeDataSizes;
		AttributeDataSizes.Init(0, AttributeData.Num());
		AttributeDataSizes[0] = AttributeData.Num();

		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		AttributeInfo.count = 1;
		AttributeInfo.tupleSize = 1;
		AttributeInfo.totalArrayElements = AttributeData.Num();
		AttributeInfo.exists = true;
		AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT_ARRAY;
		AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_COLLISIONS_MAX_SIZE;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId, AttributeName, &AttributeInfo), false);

		FHoudiniApi::SetAttributeFloatArrayData(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId, AttributeName, &AttributeInfo,
			(const float *)AttributeData.GetData(), AttributeInfo.totalArrayElements, (const int *)AttributeDataSizes.GetData(), 0,  AttributeInfo.count);
	}

	// Collisions - Size specific data - Damage threshold
	if (GCSizeSpecDatas.Num() > 0)
	{
		TArray<int32> AttributeData;
		AttributeData.SetNum(GCSizeSpecDatas.Num());
		for (int32 i = 0; i < AttributeData.Num(); i++)
		{
			AttributeData[i] = GCSizeSpecDatas[i].DamageThreshold;
		}

		// We have one array with DamageThreshold.Num() elements.
		TArray< int32 > AttributeDataSizes;
		AttributeDataSizes.Init(0, AttributeData.Num());
		AttributeDataSizes[0] = AttributeData.Num();

		
		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		AttributeInfo.count = 1;
		AttributeInfo.tupleSize = 1;
		AttributeInfo.totalArrayElements = AttributeData.Num();
		AttributeInfo.exists = true;
		AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
		AttributeInfo.storage = HAPI_STORAGETYPE_INT_ARRAY;
		AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_COLLISIONS_DAMAGE_THRESHOLD;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId, AttributeName, &AttributeInfo), false);

		FHoudiniApi::SetAttributeIntArrayData(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId, AttributeName, &AttributeInfo,
			(const int *)AttributeData.GetData(), AttributeInfo.totalArrayElements, (const int *)AttributeDataSizes.GetData(), 0,  AttributeInfo.count);
	}

	for (int32 GCSizeSpecIdx = 0; GCSizeSpecIdx < GCSizeSpecDatas.Num(); GCSizeSpecIdx++)
	{
		const FGeometryCollectionSizeSpecificData & GCSizeSpecData = GCSizeSpecDatas[GCSizeSpecIdx];
		
		// Collisions - Collision Type
		if(GCSizeSpecData.CollisionShapes.Num() > 0)
		{
			TArray<int32> AttributeData;
			AttributeData.SetNum(GCSizeSpecData.CollisionShapes.Num());
			for (int32 i = 0; i < AttributeData.Num(); i++)
			{
				AttributeData[i] = static_cast<int32>(GCSizeSpecData.CollisionShapes[i].CollisionType);
			}
	
			TArray< int32 > AttributeDataSizes;
			AttributeDataSizes.Init(0, AttributeData.Num());
			AttributeDataSizes[0] = AttributeData.Num();
			
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
			AttributeInfo.count = 1;
			AttributeInfo.tupleSize = 1;
			AttributeInfo.totalArrayElements = AttributeData.Num();
			AttributeInfo.exists = true;
			AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
			AttributeInfo.storage = HAPI_STORAGETYPE_INT_ARRAY;
			AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

			const FString AttributeName = FString::Printf(TEXT("%s_%d"), TEXT(HAPI_UNREAL_ATTRIB_GC_COLLISIONS_COLLISION_TYPE), GCSizeSpecIdx);
			const char * AttributeNameCStr = TCHAR_TO_UTF8(*AttributeName);
	
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo), false);
			
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntArrayData(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo,
				(const int32 *)AttributeData.GetData(), AttributeInfo.totalArrayElements, (const int *)AttributeDataSizes.GetData(), 0, AttributeInfo.count), false);
		}
	
		// Collisions - Size specific data - Implicit Type
		if (GCSizeSpecData.CollisionShapes.Num() > 0)
		{
			TArray<int32> AttributeData;
			AttributeData.SetNum(GCSizeSpecData.CollisionShapes.Num());
			for (int32 i = 0; i < AttributeData.Num(); i++)
			{
				AttributeData[i] = (int32)GCSizeSpecData.CollisionShapes[i].CollisionType;
				if (GCSizeSpecData.CollisionShapes[i].ImplicitType == EImplicitTypeEnum::Chaos_Implicit_None)
				{
					AttributeData[i] = 0;
				}
				else if (GCSizeSpecData.CollisionShapes[i].ImplicitType > EImplicitTypeEnum::Chaos_Implicit_None)
				{
					AttributeData[i] = static_cast<int32>(GCSizeSpecData.CollisionShapes[i].ImplicitType); // If past none, keep same indexing
				}
				else
				{
					AttributeData[i] = static_cast<int32>(GCSizeSpecData.CollisionShapes[i].ImplicitType) + 1; // + 1 because 0 = None.
				}
			}
	
			TArray< int32 > AttributeDataSizes;
			AttributeDataSizes.Init(0, AttributeData.Num());
			AttributeDataSizes[0] = AttributeData.Num();
			
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
			AttributeInfo.count = 1;
			AttributeInfo.tupleSize = 1;
			AttributeInfo.totalArrayElements = AttributeData.Num();
			AttributeInfo.exists = true;
			AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
			AttributeInfo.storage = HAPI_STORAGETYPE_INT_ARRAY;
			AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

			const FString AttributeName = FString::Printf(TEXT("%s_%d"), TEXT(HAPI_UNREAL_ATTRIB_GC_COLLISIONS_IMPLICIT_TYPE), GCSizeSpecIdx);
			const char * AttributeNameCStr = TCHAR_TO_UTF8(*AttributeName);
			
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo), false);
			
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntArrayData(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo,
				(const int32 *)AttributeData.GetData(), AttributeInfo.totalArrayElements, (const int *)AttributeDataSizes.GetData(), 0, AttributeInfo.count), false);
		}
	
		// Collisions - Size specific data - Min Level Set Resolution
		if (GCSizeSpecData.CollisionShapes.Num() > 0)
		{
			TArray<int32> AttributeData;
			AttributeData.SetNum(GCSizeSpecData.CollisionShapes.Num());
			for (int32 i = 0; i < AttributeData.Num(); i++)
			{
				AttributeData[i] = GCSizeSpecData.CollisionShapes[i].LevelSet.MinLevelSetResolution;
			}
	
			TArray< int32 > AttributeDataSizes;
			AttributeDataSizes.Init(0, AttributeData.Num());
			AttributeDataSizes[0] = AttributeData.Num();
			
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
			AttributeInfo.count = 1;
			AttributeInfo.tupleSize = 1;
			AttributeInfo.totalArrayElements = AttributeData.Num();
			AttributeInfo.exists = true;
			AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
			AttributeInfo.storage = HAPI_STORAGETYPE_INT_ARRAY;
			AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

			const FString AttributeName = FString::Printf(TEXT("%s_%d"), TEXT(HAPI_UNREAL_ATTRIB_GC_COLLISIONS_MIN_LEVEL_SET_RESOLUTION), GCSizeSpecIdx);
			const char * AttributeNameCStr = TCHAR_TO_UTF8(*AttributeName);
			
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo), false);
			
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntArrayData(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo,
				(const int32 *)AttributeData.GetData(), AttributeInfo.totalArrayElements, (const int *)AttributeDataSizes.GetData(), 0, AttributeInfo.count), false);
		}

		// Collisions - Size specific data - Max Level Set Resolution
		if (GCSizeSpecData.CollisionShapes.Num() > 0)
		{
			TArray<int32> AttributeData;
			AttributeData.SetNum(GCSizeSpecData.CollisionShapes.Num());
			for (int32 i = 0; i < AttributeData.Num(); i++)
			{
				AttributeData[i] = GCSizeSpecData.CollisionShapes[i].LevelSet.MaxLevelSetResolution;
			}
	
			TArray< int32 > AttributeDataSizes;
			AttributeDataSizes.Init(0, AttributeData.Num());
			AttributeDataSizes[0] = AttributeData.Num();
			
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
			AttributeInfo.count = 1;
			AttributeInfo.tupleSize = 1;
			AttributeInfo.totalArrayElements = AttributeData.Num();
			AttributeInfo.exists = true;
			AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
			AttributeInfo.storage = HAPI_STORAGETYPE_INT_ARRAY;
			AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;
	
			const FString AttributeName = FString::Printf(TEXT("%s_%d"), TEXT(HAPI_UNREAL_ATTRIB_GC_COLLISIONS_MAX_LEVEL_SET_RESOLUTION), GCSizeSpecIdx);
			const char * AttributeNameCStr = TCHAR_TO_UTF8(*AttributeName);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo), false);
			
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntArrayData(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo,
				(const int32 *)AttributeData.GetData(), AttributeInfo.totalArrayElements, (const int *)AttributeDataSizes.GetData(), 0, AttributeInfo.count), false);
		}
		
		// Collisions - Size specific data - Min cluster level set resolution
		if (GCSizeSpecData.CollisionShapes.Num() > 0)
		{
			TArray<int32> AttributeData;
			AttributeData.SetNum(GCSizeSpecData.CollisionShapes.Num());
			for (int32 i = 0; i < AttributeData.Num(); i++)
			{
				AttributeData[i] = GCSizeSpecData.CollisionShapes[i].LevelSet.MinClusterLevelSetResolution;
			}
	
			TArray< int32 > AttributeDataSizes;
			AttributeDataSizes.Init(0, AttributeData.Num());
			AttributeDataSizes[0] = AttributeData.Num();
			
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
			AttributeInfo.count = 1;
			AttributeInfo.tupleSize = 1;
			AttributeInfo.totalArrayElements = AttributeData.Num();
			AttributeInfo.exists = true;
			AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
			AttributeInfo.storage = HAPI_STORAGETYPE_INT_ARRAY;
			AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;
	
			const FString AttributeName = FString::Printf(TEXT("%s_%d"), TEXT(HAPI_UNREAL_ATTRIB_GC_COLLISIONS_MIN_CLUSTER_LEVEL_SET_RESOLUTION), GCSizeSpecIdx);
			const char * AttributeNameCStr = TCHAR_TO_UTF8(*AttributeName);
	
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo), false);
			
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntArrayData(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo,
				(const int32 *)AttributeData.GetData(), AttributeInfo.totalArrayElements, (const int *)AttributeDataSizes.GetData(), 0, AttributeInfo.count), false);
		}
		
		// Collisions - Size specific data - Max cluster level set resolution
		if (GCSizeSpecData.CollisionShapes.Num() > 0)
		{
			TArray<int32> AttributeData;
			AttributeData.SetNum(GCSizeSpecData.CollisionShapes.Num());
			for (int32 i = 0; i < AttributeData.Num(); i++)
			{
				AttributeData[i] = GCSizeSpecData.CollisionShapes[i].LevelSet.MaxClusterLevelSetResolution;
			}
	
			TArray< int32 > AttributeDataSizes;
			AttributeDataSizes.Init(0, AttributeData.Num());
			AttributeDataSizes[0] = AttributeData.Num();
			
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
			AttributeInfo.count = 1;
			AttributeInfo.tupleSize = 1;
			AttributeInfo.totalArrayElements = AttributeData.Num();
			AttributeInfo.exists = true;
			AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
			AttributeInfo.storage = HAPI_STORAGETYPE_INT_ARRAY;
			AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;
	
			const FString AttributeName = FString::Printf(TEXT("%s_%d"), TEXT(HAPI_UNREAL_ATTRIB_GC_COLLISIONS_MAX_CLUSTER_LEVEL_SET_RESOLUTION), GCSizeSpecIdx);
			const char * AttributeNameCStr = TCHAR_TO_UTF8(*AttributeName);
			
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo), false);
			
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntArrayData(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo,
				(const int32 *)AttributeData.GetData(), AttributeInfo.totalArrayElements, (const int *)AttributeDataSizes.GetData(), 0, AttributeInfo.count), false);
		}
	
		// Collisions - Size specific data - Object reduction percentage
		if (GCSizeSpecData.CollisionShapes.Num() > 0)
		{
			TArray<float> AttributeData;
			AttributeData.SetNum(GCSizeSpecData.CollisionShapes.Num());
			for (int32 i = 0; i < AttributeData.Num(); i++)
			{
				AttributeData[i] = GCSizeSpecData.CollisionShapes[i].CollisionObjectReductionPercentage;
			}
	
			TArray< int32 > AttributeDataSizes;
			AttributeDataSizes.Init(0, AttributeData.Num());
			AttributeDataSizes[0] = AttributeData.Num();
			
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
			AttributeInfo.count = 1;
			AttributeInfo.tupleSize = 1;
			AttributeInfo.totalArrayElements = AttributeData.Num();
			AttributeInfo.exists = true;
			AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
			AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT_ARRAY;
			AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;
	
			const FString AttributeName = FString::Printf(TEXT("%s_%d"), TEXT(HAPI_UNREAL_ATTRIB_GC_COLLISIONS_COLLISION_OBJECT_REDUCTION_PERCENTAGE), GCSizeSpecIdx);
			const char * AttributeNameCStr = TCHAR_TO_UTF8(*AttributeName);
	
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo), false);
			
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatArrayData(
					FHoudiniEngine::Get().GetSession(),
					GeoId, PartId, AttributeNameCStr, &AttributeInfo,
					(const float *)AttributeData.GetData(), AttributeInfo.totalArrayElements, (const int *)AttributeDataSizes.GetData(), 0, AttributeInfo.count), false);
		}

		// Collisions - Size specific data - Size specific data - Collision margin fraction
		if (GCSizeSpecData.CollisionShapes.Num() > 0)
		{
			TArray<float> AttributeData;
			AttributeData.SetNum(GCSizeSpecData.CollisionShapes.Num());
			for (int32 i = 0; i < AttributeData.Num(); i++)
			{
				AttributeData[i] = GCSizeSpecData.CollisionShapes[i].CollisionMarginFraction;
			}
	
			TArray< int32 > AttributeDataSizes;
			AttributeDataSizes.Init(0, AttributeData.Num());
			AttributeDataSizes[0] = AttributeData.Num();
			
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
			AttributeInfo.count = 1;
			AttributeInfo.tupleSize = 1;
			AttributeInfo.totalArrayElements = AttributeData.Num();
			AttributeInfo.exists = true;
			AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
			AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT_ARRAY;
			AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;
	
			const FString AttributeName = FString::Printf(TEXT("%s_%d"), TEXT(HAPI_UNREAL_ATTRIB_GC_COLLISIONS_COLLISION_MARGIN_FRACTION), GCSizeSpecIdx);
			const char * AttributeNameCStr = TCHAR_TO_UTF8(*AttributeName);
	
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo), false);
			
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatArrayData(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo,
				(const float *)AttributeData.GetData(), AttributeInfo.totalArrayElements, (const int *)AttributeDataSizes.GetData(), 0, AttributeInfo.count), false);
		}
		
		// Collisions - Size specific data - Collision particles fraction
		{
			TArray<float> AttributeData;
			AttributeData.SetNum(GCSizeSpecData.CollisionShapes.Num());
			for (int32 i = 0; i < AttributeData.Num(); i++)
			{
				AttributeData[i] = GCSizeSpecData.CollisionShapes[i].CollisionParticles.CollisionParticlesFraction;
			}
	
			TArray< int32 > AttributeDataSizes;
			AttributeDataSizes.Init(0, AttributeData.Num());
			AttributeDataSizes[0] = AttributeData.Num();
			
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
			AttributeInfo.count = 1;
			AttributeInfo.tupleSize = 1;
			AttributeInfo.totalArrayElements = AttributeData.Num();
			AttributeInfo.exists = true;
			AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
			AttributeInfo.storage = HAPI_STORAGETYPE_FLOAT_ARRAY;
			AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;
	
			const FString AttributeName = FString::Printf(TEXT("%s_%d"), TEXT(HAPI_UNREAL_ATTRIB_GC_COLLISIONS_COLLISION_PARTICLES_FRACTION), GCSizeSpecIdx);
			const char * AttributeNameCStr = TCHAR_TO_UTF8(*AttributeName);
			
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo), false);
			
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatArrayData(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo,
				(const float *)AttributeData.GetData(), AttributeInfo.totalArrayElements, (const int *)AttributeDataSizes.GetData(), 0, AttributeInfo.count), false);
		}
	
		// Collisions - Size specific data - Maximum collision particles
		{
			TArray<int32> AttributeData;
			AttributeData.SetNum(GCSizeSpecData.CollisionShapes.Num());
			for (int32 i = 0; i < AttributeData.Num(); i++)
			{
				AttributeData[i] = GCSizeSpecData.CollisionShapes[i].CollisionParticles.MaximumCollisionParticles;
			}
	
			TArray< int32 > AttributeDataSizes;
			AttributeDataSizes.Init(0, AttributeData.Num());
			AttributeDataSizes[0] = AttributeData.Num();
			
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
			AttributeInfo.count = 1;
			AttributeInfo.tupleSize = 1;
			AttributeInfo.totalArrayElements = AttributeData.Num();
			AttributeInfo.exists = true;
			AttributeInfo.owner = HAPI_ATTROWNER_DETAIL;
			AttributeInfo.storage = HAPI_STORAGETYPE_INT_ARRAY;
			AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;
	
			const FString AttributeName = FString::Printf(TEXT("%s_%d"), TEXT(HAPI_UNREAL_ATTRIB_GC_COLLISIONS_MAXIMUM_COLLISION_PARTICLES), GCSizeSpecIdx);
			const char * AttributeNameCStr = TCHAR_TO_UTF8(*AttributeName);
	
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo), false);
			
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntArrayData(
				FHoudiniEngine::Get().GetSession(),
				GeoId, PartId, AttributeNameCStr, &AttributeInfo,
				(const int32 *)AttributeData.GetData(), AttributeInfo.totalArrayElements, (const int *)AttributeDataSizes.GetData(), 0, AttributeInfo.count), false);
		}
	}
	
	// Non-Essential attributes- Similar to Mesh input: unreal_input_mesh_name, unreal_input_source_file, component and actor tags
	// INPUT GC NAME
	//---------------------------------------------------------------------------------------------------------------------
	{
		// Create primitive attribute with mesh asset path
		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		AttributeInfo.count = Part.faceCount;
		AttributeInfo.tupleSize = 1;
		AttributeInfo.exists = true;
		AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			GeoId, 0, HAPI_UNREAL_ATTRIB_INPUT_GC_NAME, &AttributeInfo), false);

		const FString AssetPath = GeometryCollectionObject->GetPathName();
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeStringData(
			AssetPath, GeoId, 0, HAPI_UNREAL_ATTRIB_INPUT_GC_NAME, AttributeInfo), false);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// COMPONENT AND ACTOR TAGS
	//---------------------------------------------------------------------------------------------------------------------
	if (IsValid(GeometryCollectionComponent))
	{
		// Try to create groups for the static mesh component's tags
		if (GeometryCollectionComponent->ComponentTags.Num() > 0
			&& !FHoudiniEngineUtils::CreateGroupsFromTags(GeoId, PartId, GeometryCollectionComponent->ComponentTags))
				HOUDINI_LOG_WARNING(TEXT("Could not create groups from the Static Mesh Component's tags!"));

		AActor* ParentActor = GeometryCollectionComponent->GetOwner();
		if (IsValid(ParentActor))
		{
			// Try to create groups for the parent Actor's tags
			if (ParentActor->Tags.Num() > 0
				&& !FHoudiniEngineUtils::CreateGroupsFromTags(GeoId, PartId, ParentActor->Tags))
				HOUDINI_LOG_WARNING(TEXT("Could not create groups from the Static Mesh Component's parent actor tags!"));

			// Add the unreal_actor_path attribute
			FHoudiniEngineUtils::AddActorPathAttribute(GeoId, PartId, ParentActor, Part.faceCount);

			// Add the unreal_level_path attribute
			FHoudiniEngineUtils::AddLevelPathAttribute(GeoId, PartId, ParentActor->GetLevel(), Part.faceCount);
		}
	}
	
	return true;
}

