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

#include "UnrealBrushTranslator.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniInputObject.h"

#include "HoudiniGeoPartObject.h"
#include "Model.h"
#include "Engine/Polys.h"

#include "HoudiniEngineRuntimeUtils.h"

// Includes for Brush building code. Remove when the code is in the correct place.
#include "HCsgUtils.h"
#include "ActorEditorUtils.h"
#include "Misc/ScopedSlowTask.h"

#include "Engine/Level.h"

// TODO: Fix this
// This is currently being included to get access to the CreateFaceMaterialArray / DeleteFaceMaterialArray methods.
#include "UnrealMeshTranslator.h"

DEFINE_LOG_CATEGORY_STATIC(LogBrushTranslator, Log, All);

bool FUnrealBrushTranslator::CreateInputNodeForBrush(
	UHoudiniInputBrush* InputBrushObject, 
	ABrush* BrushActor, 
	const TArray<AActor*>* ExcludeActors, 
	HAPI_NodeId &CreatedNodeId, 
	const FString& NodeName
)
{
	if (!IsValid(BrushActor))
		return false;

	if (!IsValid(BrushActor->Brush))
		return false;

	if (InputBrushObject->ShouldIgnoreThisInput())
		return true;

	//--------------------------------------------------------------------------------------------------
	// Create an input node
	//--------------------------------------------------------------------------------------------------

	HAPI_NodeId ParentNodeId = -1;
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(CreatedNodeId))
	{
		HAPI_NodeId InputNodeId = -1;
		//FString BrushNodeName = NodeName + TEXT("_") + BrushActor->GetName();
		FString BrushNodeName = BrushActor->GetName();
		// Create Brush SOP node
		std::string NodeNameRawString;
		FHoudiniEngineUtils::ConvertUnrealString(BrushNodeName, NodeNameRawString);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateInputNode(
			FHoudiniEngine::Get().GetSession(), &InputNodeId, NodeNameRawString.c_str()), false);

		// Check if we have a valid id for this new input asset.
		if (!FHoudiniEngineUtils::IsHoudiniNodeValid(InputNodeId))
			return false;

		// We now have a valid id.
		CreatedNodeId = InputNodeId;
		ParentNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(CreatedNodeId);	

		// Add a clean node
		HAPI_NodeId CleanNodeId;
		HOUDINI_CHECK_ERROR_RETURN( FHoudiniEngineUtils::CreateNode( 
			ParentNodeId, TEXT("clean"), TEXT("clean"), true, &CleanNodeId), false);

		// Connect input node to the clean node
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(), CleanNodeId, 0, CreatedNodeId, 0), false);

		// Set display flag on the clean node
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetNodeDisplay(
				FHoudiniEngine::Get().GetSession(), CleanNodeId, 1), false);
	}
	else
	{
		ParentNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(CreatedNodeId);	
	}

	
	// Transform for positions
	const FTransform ActorTransform = BrushActor->GetActorTransform();
	const FTransform ActorTransformInverse = ActorTransform.Inverse();
	// Precompute matrices for Normal transformations (see FPlane::TransformBy)
	const FMatrix NmlInvXform = ActorTransformInverse.ToMatrixWithScale().TransposeAdjoint();
	float NScale = ActorTransformInverse.GetDeterminant() < 0 ? -1.f : 1.f;

	//--------------------------------------------------------------------------------------------------
	// Find actors that intersect with the given brush.
	//--------------------------------------------------------------------------------------------------
	TArray<ABrush*> BrushActors;
	UHoudiniInputBrush::FindIntersectingSubtractiveBrushes(InputBrushObject, BrushActors);
	
	UModel* BrushModel = UHCsgUtils::BuildModelFromBrushes(BrushActors);
	InputBrushObject->UpdateCachedData(BrushModel, BrushActors);
	
	// DEBUG: Upload the level model (baked by UE) to Houdini
	// ULevel* Level = BrushActor->GetTypedOuter<ULevel>();
	// BrushModel = Level->Model;


	int NumPoints = BrushModel->Points.Num();
	if (NumPoints == 0)
	{
		// The content has changed and now we don't have geo to output.
		// Be sure to clean up existing nodes in Houdini.
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
				FHoudiniEngine::Get().GetSession(), ParentNodeId))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input OBJ node for %s."), *(BrushActor->GetName()));
		}
		CreatedNodeId = -1;
		return true;
	}

	//--------------------------------------------------------------------------------------------------
	// Construct the face count buffer. Also count the vertex indices in required to define the Part.
	//--------------------------------------------------------------------------------------------------

	int NumIndices = 0;
	TArray<int32> FaceCountBuffer;

	{
		// Calculate the size of the vertex buffer and the base vertex index of each node.
		TArray<FBspNode>& Nodes = BrushModel->Nodes;
		//TArray<FBspSurf>& Surfs = BrushModel->Surfs;
		TArray<FVert>& Verts = BrushModel->Verts;
		
		int32 NumNodes = Nodes.Num();

		FaceCountBuffer.SetNumUninitialized(NumNodes);
		// Build the face counts buffer by iterating over the BSP nodes.
		for(int32 NodeIndex = 0; NodeIndex < NumNodes; NodeIndex++)
		{
			FBspNode& Node = Nodes[NodeIndex];
			FaceCountBuffer[NodeIndex] = Node.NumVertices;
			NumIndices += Node.NumVertices;
		}
	}

	//--------------------------------------------------------------------------------------------------
	// Apply actor transform
	//--------------------------------------------------------------------------------------------------

	 if (!ActorTransform.Equals(FTransform::Identity))
	 {

	 	// convert to HAPI_Transform
	 	HAPI_TransformEuler HapiTransform;
	 	FHoudiniApi::TransformEuler_Init(&HapiTransform);
	 	FHoudiniEngineUtils::TranslateUnrealTransform(ActorTransform, HapiTransform);

	 	// Set the transform on the OBJ parent
	 	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
	 		FHoudiniEngine::Get().GetSession(), ParentNodeId, &HapiTransform), false);
	 }

	//--------------------------------------------------------------------------------------------------
	// Start processing the geo and add it to the input node
	//--------------------------------------------------------------------------------------------------

	// Create part.
	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);

	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.vertexCount = NumIndices;
	Part.faceCount = FaceCountBuffer.Num();
	Part.pointCount = NumPoints;
	Part.type = HAPI_PARTTYPE_MESH;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
		FHoudiniEngine::Get().GetSession(), CreatedNodeId, 0, &Part), false);	

	// -----------------------------
	// Vector - Point Attribute Info
	// -----------------------------
	HAPI_AttributeInfo AttributeInfoPointVector;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointVector);
	AttributeInfoPointVector.count = NumPoints;
	AttributeInfoPointVector.tupleSize = 3;
	AttributeInfoPointVector.exists = true;
	AttributeInfoPointVector.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPointVector.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPointVector.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), CreatedNodeId, 0,
		HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPointVector), false);

	// -----------------------------
	// Vector - Vertex Attribute Info
	// -----------------------------
	HAPI_AttributeInfo AttributeInfoVertexVector;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoVertexVector);
	AttributeInfoVertexVector.count = NumIndices;
	AttributeInfoVertexVector.tupleSize = 3;
	AttributeInfoVertexVector.exists = true;
	AttributeInfoVertexVector.owner = HAPI_ATTROWNER_VERTEX;
	AttributeInfoVertexVector.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoVertexVector.originalOwner = HAPI_ATTROWNER_INVALID;

	// -----------------------------
	// POSITION (P)
	// -----------------------------

	{
		TArray<FVector> OutPosition;
		FVector Scale = FVector(1.f, 1.f, 1.f); // TODO: Extract from actor transform.
		OutPosition.SetNumUninitialized(NumPoints);

		for (int32 PosIndex = 0; PosIndex < NumPoints; ++PosIndex)
		{
			FVector Point = BrushModel->Points[PosIndex];
			Point = ActorTransform.InverseTransformPosition(Point);
			FVector Pos(Point.X, Point.Z, Point.Y);
			OutPosition[PosIndex] = Pos/HAPI_UNREAL_SCALE_FACTOR_POSITION;
		}

		// Upload point positions.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			(const float*)OutPosition.GetData(), CreatedNodeId, 0, HAPI_UNREAL_ATTRIB_POSITION, AttributeInfoPointVector), false);
	}

	//--------------------------------------------------------------------------------------------------------------------- 
	// INDICES (VertexList), NORMALS, UVS
	//---------------------------------------------------------------------------------------------------------------------
	// Vertex buffer processing logic based on UModel::BuildVertexBuffers().
	{
		// Calculate the size of the vertex buffer and the base vertex index of each node.
		TArray<FVector3f>& Positions = BrushModel->Points;
		TArray<FBspNode>& Nodes = BrushModel->Nodes;
		TArray<FBspSurf>& Surfs = BrushModel->Surfs;
		TArray<FVert>& Verts = BrushModel->Verts;
		TArray<FVector3f>& Vectors = BrushModel->Vectors;
		
		int32 NumNodes = Nodes.Num();
		TArray<int32> Indices;
		TArray<FVector> OutNormals;
		TArray<FVector> OutUV;
		TArray<int32> MaterialIndices;
		TMap<UMaterialInterface*, int32> MaterialMap;
		
		Indices.SetNumUninitialized(NumIndices);
		OutNormals.SetNumUninitialized(NumIndices);
		OutUV.SetNumUninitialized(NumIndices);

		MaterialIndices.SetNumUninitialized(NumNodes);

		// Populate the vertex index buffer.
		int32 iVertex = 0;
		for(int32 NodeIndex = 0; NodeIndex < NumNodes; NodeIndex++)
		{
			FBspNode& Node = Nodes[NodeIndex];
			FBspSurf& Surf = Surfs[Node.iSurf];
			for (int32 NodeVertexIndex = 0; NodeVertexIndex < Node.NumVertices; ++NodeVertexIndex)
			{
				// Vertex Index
				Indices[iVertex] = Verts[Node.iVertPool + NodeVertexIndex].pVertex;
				// Normal
				FVector N = Vectors[Surf.vNormal];			
				N = NmlInvXform.TransformVector(N).GetSafeNormal();

				OutNormals[iVertex] = FVector(N.X, N.Z, N.Y);
				// UVs
				FVector3f& vU = Vectors[Surf.vTextureU];
				FVector3f& vV = Vectors[Surf.vTextureV];
				FVector3f deltaVtx = (Positions[Indices[iVertex]] - Positions[Surf.pBase]);
				float U = FVector::DotProduct(deltaVtx, vU) / UModel::GetGlobalBSPTexelScale();
				float V = -FVector::DotProduct(deltaVtx, vV) / UModel::GetGlobalBSPTexelScale();
				OutUV[iVertex] = FVector3f(U, V, 0.f);
				++iVertex;
			}
			// Face Material
			// Construct a material index array for the faces
			int32 MaterialIndex = MaterialMap.FindOrAdd(Surf.Material, MaterialMap.Num());
			MaterialIndices[NodeIndex] = MaterialIndex;
		}

		// Set the vertex index buffer
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetVertexList(
			Indices, CreatedNodeId, 0), false);

		// Set the face counts as per the BSP nodes.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetFaceCounts(
			FaceCountBuffer, CreatedNodeId,	0), false);

		// -----------------------------
		// Normal attribute
		// -----------------------------
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), CreatedNodeId, 0,
			HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoVertexVector), false);
		
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			(const float*)OutNormals.GetData(), CreatedNodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL, AttributeInfoVertexVector), false);

		// -----------------------------
		// UV attribute
		// -----------------------------
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), CreatedNodeId, 0,
			HAPI_UNREAL_ATTRIB_UV, &AttributeInfoVertexVector), false);
		
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetAttributeFloatData(
			(const float*)OutUV.GetData(), CreatedNodeId, 0, HAPI_UNREAL_ATTRIB_UV, AttributeInfoVertexVector), false);

		// -----------------------------
		// Material attribute
		// -----------------------------

		TArray<UMaterialInterface*> Materials;

		if (MaterialMap.Num() > 0)
		{
			// Sort the output material in the correct order (by material index)
			MaterialMap.ValueSort([](int32 A, int32 B) { return A < B; });

			// Set the value in the correct order
			// Do not reduce the array of materials, this could cause crahses in some weird cases..
			/*if (MaterialMap.Num() > MaterialInterfaces.Num())
				MaterialInterfaces.SetNumZeroed(MaterialMap.Num());*/
			Materials.SetNumUninitialized(MaterialMap.Num());

			int32 MaterialIndex = 0;
			for (auto Kvp : MaterialMap)
			{
				Materials[MaterialIndex++] = Kvp.Key;
			}
		}

		// List of materials, one for each face.
		TArray<FString> OutMaterials;

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
				Materials, MaterialIndices, OutMaterials,
				ScalarMaterialParameters, VectorMaterialParameters, TextureMaterialParameters);

			// Create attribute for materials and all attributes for material parameters
			bAttributeSuccess = FUnrealMeshTranslator::CreateHoudiniMeshAttributes(
				CreatedNodeId,
				0,
				NumNodes,
				OutMaterials,
				ScalarMaterialParameters,
				VectorMaterialParameters,
				TextureMaterialParameters);
		}
		else
		{
			// Create attributes only for the materials
			// Only get the material attribute data
			FUnrealMeshTranslator::CreateFaceMaterialArray(
				Materials, MaterialIndices, OutMaterials);

			// Create attribute for materials
			bAttributeSuccess = FUnrealMeshTranslator::CreateHoudiniMeshAttributes(
				CreatedNodeId,
				0,
				NumNodes,
				OutMaterials,
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

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), CreatedNodeId), false );


	return true;
}

