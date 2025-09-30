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

#pragma once

#include "HAPI/HAPI_Common.h"
#include "HoudiniEngineRuntimeUtils.h"

#include "CoreMinimal.h"
#include "UnrealObjectInputRuntimeTypes.h"
#include "Components/MeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Misc/Optional.h"
#include "UObject/ObjectMacros.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

class UStaticMesh;
class UAnimSequence;
class UStaticMeshComponent;
class UStaticMeshSocket;

class UMaterialInterface;
class FUnrealObjectInputHandle;
class FHoudiniEngineIndexedStringMap;
class FStaticMeshConstAttributes;
struct FStaticMeshSourceModel;
struct FStaticMeshLODResources;
struct FMeshDescription;
struct FKConvexElem;


struct HOUDINIENGINE_API FUnrealMeshExportOptions
{
	bool bLODs = true;
	bool bSockets = true;
	bool bColliders = true;
	bool bMainMesh = true;
	bool bMaterialParameters = false;
	bool bPreferNaniteFallbackMesh = false;
};


struct HOUDINIENGINE_API FUnrealMaterialInfo
{
	FString MaterialPath;
	TMap<FString, float> ScalarParameters;
	TMap<FString, FLinearColor> VectorParameters;
	TMap<FString, FString> TextureParameters;
	TMap<FString, int8> BoolParameters;
};


TArray<HAPI_NodeId> GetHapiNodeIds(const TArray<FUnrealObjectInputIdentifier> & Identifiers);
HAPI_NodeId GetHapiNodeId(FUnrealObjectInputIdentifier Identifier);
TArray<HAPI_NodeId> GetHapiNodeIds(const TArray<FUnrealObjectInputHandle>& Handles);
HAPI_NodeId GetHapiNodeId(FUnrealObjectInputHandle Handle);

struct HOUDINIENGINE_API FUnrealMeshExportData
{
	// This struct is created on each invocation of the Unreal Mesh Translator. The Unreal Mesh Translator
	// keeps track of multiple Geo (Object) Nodes which keep track of different parts of the mesh, eg.
	// lod0 and lod1 would contain the geometry for the lods, and all_lods contains all lods merged together.
	//
	
	FUnrealMeshExportData(const UObject* Parent, bool bCanDelete);

	// This function creates a Geo Construction node if it doesn't exist. If the Geo node does exist then
	// the function will ensure handles and identifiers exist. 
	HAPI_NodeId GetOrCreateConstructionGeoNode(
		bool& bCreated,
		const FString& Label, 
		const EUnrealObjectInputNodeType NodeType);

	// This function must  be called if GetOrCreateConstructionGeoNode() returned with bCreated == true.
	// See implementation for more details. NodeId must be a Hapi Node internal to the created Geo.
	HAPI_NodeId RegisterConstructionNode(
		const FString& Label,
		const HAPI_NodeId NodeId,
		const TSet<FUnrealObjectInputHandle>* ReferencedNodes = nullptr);

	// Accessors to constructor nodes. Really all you need is the first function, GetConstructionHandles(),
	// but the others just provide convenient shortcuts to reduce code verbosity.
	const TMap<FString, FUnrealObjectInputHandle>& GetConstructionHandles();
	bool Contains(const FString& Label);
	HAPI_NodeId GetHapiNodeId(const FString& Label);
	FUnrealObjectInputHandle GetNodeHandle(const FString& Label);

private:
	HAPI_NodeId GetConstructionSubnetNodeId() const { return ConstructionSubnetNodeId; }
	void EnsureConstructionSubnetExists();
	bool ScanForExistingNodesInHoudini();
	FUnrealObjectInputIdentifier MakeNodeIdentifier(const FString& Label, EUnrealObjectInputNodeType NodeType);
	static FString CleanInputPath(const FString& ObjectPath);

	// For each Label keep track of the data associated with it.
	TMap<FString, FUnrealObjectInputHandle> RegisteredHandles;
	TMap<FString, FUnrealObjectInputIdentifier> RegisteredIdentifiers;
	TMap<FString, HAPI_NodeId> RegisteredGeoNodes;
	TMap<FString, HAPI_NodeId> ExistingUnassignedHAPINodes; 

	// ConstructionSubnetHandle is a handle to the node where all the construction happens. Keep a cached copy of the
	// Hapi node, since we access it a lot.
	FUnrealObjectInputHandle ConstructionSubnetHandle;
	HAPI_NodeId ConstructionSubnetNodeId = INDEX_NONE;
	FString ConstructionSubnetPath;

	bool bCanDelete = true;
};

enum HOUDINIENGINE_API EHoudiniMeshSource
{
	MeshDescription,
	LODResource,
	HiResMeshDescription
};

struct HOUDINIENGINE_API FUnrealMeshTranslator
{
public:
	// A Word To The Wise:
	//
	// This code has been significantly refactored to fix issues with the reference input system.
	//	The old code is still largely in place in case we come across any major problems.
	//
	//	The major problem we had before was that if a Mesh was shared between two inputs with different
	//	settings, you'd most likely get the wrong settings. This wasn't very common, but still an issue.
	//	There were also performance issues.
	//
	//	The new system will use merge nodes to combine different geo nodes to get the desired result.
	//	For example, if the user specifies an input node as a mesh, and requires 2 lods + colliders,
	//	the plugin now:
	//
	//	1. creates two nodes, one for each lod (eg. lod0, lod1) 
	//	2. create merge node (a "geometry") that merges these lods
	//  3. creates a merge node with the geometry node from step 2, the "render mesh"
	//  4. creates the collision geo node
	//  5. creates the final static mesh by merging the collision and render mesh nodes.
	//
	//		lod0 ---
	//		       |- geometry --- render_mesh ---
	//      lod1 ---                             |- output
	//	                             collision ---
	//
	//	Now if a second input uses the same static mesh as an input, but without LODs or collision,
	//	we can just re-use the existing nodes:
	//
	//		 lod0 --- geometry --- render_mesh --- output
	//
	//	The naming of the intermediate nodes is actually constructed from the export options, so is
	//	unique to each combination. Leaf nodes always use the same name (eg. lod0, collisions).
	//	In the same way, if a user is using an input and then enables "lods", only missing LODs will
	//	need to be uploaded, providing a performance improvement.
	//
	//	The render mesh may often be a merge node with single inputs, but it exists because it
	//	is used to add material parameters. This uses a "material table" to upload each material
	//	parameter only once and then performs an attrib_copy to apply the material parameters to
	//	each face in the geometry.
	//
	//	Also the old code had a lot of logic to avoid empty merge nodes, but this has been intentionally
	//	avoided code complication vs a very very minor memory overhead in Houdini.
	//
	//	The best way to see all this in action is to look in SessionSync.
	//


	// HAPI : Marshaling, extract geometry and create input asset for it - return true on success
	static bool CreateInputNodeForStaticMesh(
		HAPI_NodeId& InputObjectNodeId,
		FUnrealObjectInputHandle& OutHandle,
		const UStaticMesh* Mesh,
		const UStaticMeshComponent* StaticMeshComponent,
		const FString& InputNodeName,
		const FUnrealMeshExportOptions& ExportOptions,
		const bool bInputNodesCanBeDeleted,
		const bool bForceReferenceInputNodeCreation);

	static bool CreateInputNodeForStaticMeshNew(
		HAPI_NodeId& InputObjectNodeId,
		FUnrealObjectInputHandle& OutHandle,
		const UStaticMesh* Mesh,
		const FString& InputNodeName,
		const FUnrealMeshExportOptions& ExportOptions,
		const bool bInputNodesCanBeDeleted);

	static bool CreateInputNodeForStaticMeshNew(
		HAPI_NodeId& InputObjectNodeId,
		FUnrealObjectInputHandle& OutHandle,
		const UStaticMesh* Mesh,
		const UStaticMeshComponent* StaticMeshComponent,
		const FString& InputNodeName,
		const FUnrealMeshExportOptions& ExportOptions,
		const bool bInputNodesCanBeDeleted);

	static bool CreateInputNodeForStaticMeshComponentNew(
		HAPI_NodeId& InputObjectNodeId,
		FUnrealObjectInputHandle& OutHandle,
		const FUnrealObjectInputHandle& StaticMeshHandle,
		const UStaticMeshComponent* StaticMeshComponent,
		const FString& InputNodeName,
		const FUnrealMeshExportOptions& ExportOptions,
		const bool bInputNodesCanBeDeleted);

	static bool CreateInputNodeForSplineMeshComponentNew(
		HAPI_NodeId& InputObjectNodeId,
		FUnrealObjectInputHandle& OutHandle,
		const USplineMeshComponent* StaticMeshComponent,
		const FUnrealMeshExportOptions& ExportOptions,
		const bool bInputNodesCanBeDeleted);

	// Convert the Mesh using FStaticMeshLODResources
	static bool CreateInputNodeForStaticMeshLODResources(
		const HAPI_NodeId NodeId,
		const FStaticMeshLODResources& LODResources,
		const int32 LODIndex,
		const bool DoExportLODs,
		const bool bInExportMaterialParametersAsAttributes,
		const UStaticMesh* StaticMesh,
		const UStaticMeshComponent* StaticMeshComponent);

	// Helper for converting mesh assets using FMeshDescription
	static bool CreateAndPopulateMeshPartFromMeshDescription(
		const HAPI_NodeId& NodeId,
		const FMeshDescription& MeshDescription,
		const FStaticMeshConstAttributes& MeshDescriptionAttributes,
		const int32 InLODIndex,
		const bool bAddLODGroups,
		const bool bInExportMaterialParametersAsAttributes,
		const UObject* Mesh,
		const UMeshComponent* MeshComponent,
		const TArray<UMaterialInterface*>& MeshMaterials,
		const TArray<uint16>& SectionMaterialIndices,
		const FVector3f& BuildScaleVector,
		const FString& PhysicalMaterialPath,
		bool bExportVertexColors,
		const TOptional<int32> LightMapResolution,
		const TOptional<float> LODScreenSize,
		const TOptional<FMeshNaniteSettings> NaniteSettings,
		const UAssetImportData* ImportData,
		bool bCommitGeo,
		HAPI_PartInfo& OutPartInfo);

	// Convert the Mesh using FMeshDescription
	static bool CreateInputNodeForMeshDescription(
		const HAPI_NodeId& NodeId,
		const FMeshDescription& MeshDescription,
		const int32 InLODIndex,
		bool bAddLODGroups,
		bool bInExportMaterialParametersAsAttributes,
		const UStaticMesh* StaticMesh,
		const UStaticMeshComponent* StaticMeshComponent);

	static bool CreateInputNodeForBox(
		HAPI_NodeId& OutBoxNodeId,
		const HAPI_NodeId InParentNodeID,
		const int32 ColliderIndex,
		const FVector& BoxCenter,
		const FVector& BoxExtent,
		const FRotator& BoxRotation);

	static bool CreateInputNodeForSphere(
		HAPI_NodeId& OutSphereNodeId,
		const HAPI_NodeId InParentNodeID,
		const int32 ColliderIndex,
		const FVector& SphereCenter,
		const float SphereRadius);

	static bool CreateInputNodeForSphyl(
		HAPI_NodeId& OutNodeId,
		const HAPI_NodeId InParentNodeID,
		const int32 ColliderIndex,
		const FVector& SphylCenter,
		const FRotator& SphylRotation,
		const float SphylRadius,
		const float SphereLength);

	static bool CreateInputNodeForConvex(
		HAPI_NodeId& OutNodeId,
		const HAPI_NodeId InParentNodeID,
		const int32 ColliderIndex,
		const FKConvexElem& ConvexCollider);

	static bool CreateInputNodeForCollider(
		HAPI_NodeId& OutNodeId,
		const HAPI_NodeId InParentNodeID,
		const int32 ColliderIndex,
		const FString& ColliderName,
		const TArray<float>& ColliderVertices,
		const TArray<int32>& ColliderIndices);

	static bool CreateInputNodeForMeshSockets(
		const TArray<UStaticMeshSocket*>& InMeshSocket,
		const HAPI_NodeId InParentNodeId,
		HAPI_NodeId& OutSocketsNodeId);

	// Helper function to extract the array of material names used by a given mesh
	// This is used for marshalling static mesh's materials.
	// Memory allocated by this function needs to be cleared by DeleteFaceMaterialArray()
	static void CreateFaceMaterialArray(
		const TArray<UMaterialInterface* >& Materials,
		const TArray<int32>& FaceMaterialIndices,
		FHoudiniEngineIndexedStringMap& OutStaticMeshFaceMaterials);

	// Helper function to extract the array of material names used by a given mesh
	// Also extracts all scalar/vector/texture parameter in the materials 
	// This is used for marshalling static mesh's materials.
	// Memory allocated by this function needs to be cleared by DeleteFaceMaterialArray()
	// The texture parameter array also needs to be cleared.
	static void CreateFaceMaterialArray(
		const TArray<UMaterialInterface*>& Materials,
		const TArray<int32>& FaceMaterialIndices,
		FHoudiniEngineIndexedStringMap& OutStaticMeshFaceMaterials,
		TMap<FString, TArray<float>>& OutScalarMaterialParameters,
		TMap<FString, TArray<float>>& OutVectorMaterialParameters,
		TMap<FString, FHoudiniEngineIndexedStringMap>& OutTextureMaterialParameters,
		TMap<FString, TArray<int8>>& OutBoolMaterialParameters);

	// Create and set mesh material attribute and material (scalar, vector and texture) parameters attributes
	static bool CreateHoudiniMeshAttributes(
		const int32 NodeId,
		const int32 PartId,
		const int32 Count,
		const FHoudiniEngineIndexedStringMap& TriangleMaterials,
		const TArray<int>& MaterialSlotIndices,
		const TMap<FString, TArray<float>>& ScalarMaterialParameters,
		const TMap<FString, TArray<float>>& VectorMaterialParameters,
		const TMap<FString, FHoudiniEngineIndexedStringMap>& TextureMaterialParameters,
		const TMap<FString, TArray<int8>>& BoolMaterialParameters,
		const TOptional<FString> PhysicalMaterial = TOptional<FString>(),
		const TOptional<FMeshNaniteSettings> InNaniteSettings = TOptional<FMeshNaniteSettings>());

	// Gets the simple physical Material path for the mesh component overrides or,
	// if not set, from the body setup
	static FString GetSimplePhysicalMaterialPath(UBodySetup const* BodySetup);

	static bool GetOrCreateExportStaticMeshLOD(
		FUnrealMeshExportData& ExportData,
		const int LODIndex,
		const bool bAddLODGroups,
		const UStaticMesh* Mesh,
		const EHoudiniMeshSource MeshSource);

	static bool GetOrCreateStaticMeshLODGeometries(
		FUnrealMeshExportData& ExportData,
		const UStaticMesh* StaticMesh,
		const FUnrealMeshExportOptions& ExportOptions,
		EHoudiniMeshSource MeshSource);

	static bool GetOrConstructStaticMeshGeometryNode(
		FString& GeometryLabel,
		FUnrealMeshExportData& ExportData,
		const FUnrealMeshExportOptions& ExportOptions,
		const UStaticMesh* Mesh);

	static bool GetOrConstructStaticMeshRenderNode(
		FString & RenderMeshLabel,
		FUnrealMeshExportData& ExportData,
		const FUnrealMeshExportOptions& ExportOptions,
		const UStaticMesh* Mesh);

	static bool GetOrConstructCollisions(
		FString& CollisionsLabel,
		FUnrealMeshExportData& ExportData,
		const FUnrealMeshExportOptions& ExportOptions,
		const UStaticMesh* Mesh);

	static bool GetOrConstructStaticMesh(
		FString& MeshLabel,
		FUnrealMeshExportData& ExportData,
		const FUnrealMeshExportOptions& ExportOptions,
		const UStaticMesh* StaticMesh);

	static FString MakeUniqueExportName(const FUnrealMeshExportOptions& ExportOptions);

	static TArray<UMaterialInterface*> GetMaterials(const UStaticMesh* Mesh);

	static bool CreateMergeNode(
		HAPI_NodeId & NodeId, 
		const FString & NodeLabel, 
		const HAPI_NodeId ParentNodeId, 
		const TArray<HAPI_NodeId> & Inputs);

	static bool GetOrConstructSplineMeshRenderNode(
		FString& RenderMeshLabel,
		FUnrealMeshExportData& ExportData,
		const FUnrealMeshExportOptions& ExportOptions,
		const USplineMeshComponent* Mesh);

	static bool GetOrConstructSplineMeshGeometryNode(
		FString& GeometryLabel,
		FUnrealMeshExportData& ExportData,
		const FUnrealMeshExportOptions& ExportOptions,
		const USplineMeshComponent* Mesh);


	static bool GetOrConstructSplineMeshComponent(
		FString& MeshLabel,
		FUnrealMeshExportData& ExportData,
		const FUnrealMeshExportOptions& ExportOptions,
		const USplineMeshComponent* SplineMeshComponent);

	static bool GetOrCreateSplineMeshLODGeometries(
		FUnrealMeshExportData& ExportData,
		const USplineMeshComponent* SplineMeshComponent,
		const FUnrealMeshExportOptions& ExportOptions);

	static bool GetOrCreateExportSplineMeshLOD(
		FUnrealMeshExportData& ExportData,
		const int LODIndex,
		const USplineMeshComponent* Mesh);

	static FString MakeLODName(int LODIndex, EHoudiniMeshSource Source);

	static FString MakeMeshSourceStr(EHoudiniMeshSource Source);

	static EHoudiniMeshSource DetermineMeshSource(const FUnrealMeshExportOptions& ExportOptions, const UStaticMesh * StaticMesh);

	static bool ExportCollisions(
		int32& NextMergeIndex,
		const UStaticMesh* StaticMesh,
		const HAPI_NodeId MergeNodeId,
		const HAPI_NodeId InputObjectNodeId,
		const FKAggregateGeom& SimpleColliders);


	static bool GetOrConstructSockets(
		FString& SocketsLabel,
		FUnrealMeshExportData& ExportData,
		const FUnrealMeshExportOptions& ExportOptions,
		const UStaticMesh* Mesh);

	static bool GetMaterialInfo(
		const TArray<UMaterialInterface*>& Materials,
		TArray<FUnrealMaterialInfo>& OutMaterialInfos);

	static bool GetOrCreateMaterialTableNode(
		FUnrealMeshExportData& MeshNodes,
		const TArray<FUnrealMaterialInfo>& MaterialInfos);

	static bool GetOrCreateMaterialZipNode(
		HAPI_NodeId& NodeId,
		const HAPI_NodeId ParentNodeId,
		const HAPI_NodeId MeshNode,
		const HAPI_NodeId MaterialTableNode,
		const TArray<FUnrealMaterialInfo>& MaterialInfos);

	static const FString LODPrefix;
	static const FString HiResMeshName;
	static const FString MTLParams;
	static const FString CombinePrefix;
	static const FString MaterialTableName;

	static  bool bUseNewMeshPath;

};
