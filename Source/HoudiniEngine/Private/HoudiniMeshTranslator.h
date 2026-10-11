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
#include "HoudiniGeoPartObject.h"
#include "HoudiniOutput.h"
#include "HoudiniPackageParams.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniMaterialTranslator.h"

#include "CoreMinimal.h"
#include "HoudiniEngineAttributes.h"
#include "UObject/ObjectMacros.h"
#include "PhysicsEngine/AggregateGeom.h"

struct FHoudiniGroupedMeshPrimitives;
class UStaticMesh;
class UStaticMeshSocket;
class UMaterialInterface;
class UMeshComponent;
class UStaticMeshComponent;
class UHoudiniStaticMesh;
class UHoudiniStaticMeshComponent;
class FHoudiniProxyMeshTranslator;
class FHoudiniStaticMeshTranslator;

enum class EHoudiniSplitType : uint8;

struct FKAggregateGeom;
struct FHoudiniGenericAttribute;
struct FHoudiniMeshesToBuild;

enum class EHoudiniCollisionType : uint8
{
	None,
	MainMesh,
	CustomComplex,
	Simple,
	SimpleBox,
	SimpleSphere,
	SimpleCapsule,
	Kdop10x,
	Kdop10y,
	Kdop10z,
	Kdop18,
	Kdop26
};


struct FHoudiniGroupedMeshPrimitives
{
	// Groups of primitives to be used for FHoudiniSplitGroupMesh.

	TArray<int32> VertexList;
	TArray<float> Normals;
	TArray<float> TangentU;
	TArray<float> TangentV;
	TArray<uint32> Indices;
	TArray<uint32> PartIndices;
	TArray<TArray<float>> UVSets;
	TArray<float> Colors;
	TArray<float> Alphas;
	TArray<int32> FaceMaterialIndices;
	TArray<int32> FaceSmoothingMasks;
	TArray<int32> NeededVertices;
	FString SplitGroupName;
	int SplitId = 0;
	bool bRendered = false;
	bool bIsLOD = false;
	EHoudiniCollisionType CollisionType;
	FString ComplexCollisionOwner;
	FString StaticMeshName;
};

struct FHoudiniSplitGroupMesh
{
	// This structure defines a static mesh to be built using primitives grouped by Houdini groups.
	// The name of the group in Houdini determines whether the groups are used for the static mesh's
	// main mesh, collisions or lods;

	// All grouped primitives used by this mesh.
	TArray<FHoudiniGroupedMeshPrimitives> SplitMeshData;

	// Index into the group array for LODs. LOD 0 is the main mesh, if present.
	TArray<int> LODRenders;

	// Index into the group array for main meshes.
	TArray<int> SimpleCollisions;

	// If this mesh is to be used a custom complex collider, this is its name.
	FString CustomCollisionOwner;

	bool bIsVisible = true;

	// Static Mesh generated.
	UStaticMesh* UnrealStaticMesh = nullptr;
	UHoudiniStaticMesh * HoudiniStaticMesh = nullptr;
	// Output identifier.
	FHoudiniOutputObjectIdentifier OutputObjectIdentifier;

};

struct FHoudiniMeshToBuild
{
	// All meshes output from a single output node.
	TMap<FString, FHoudiniSplitGroupMesh> Meshes;
};

struct FPreparedHoudiniMeshSplit
{
	TArray<int32> VertexList;
	int32 VertexCount = 0;
	TArray<int32> FaceIndices;
	int32 FirstValidVertexIndex = INDEX_NONE;
	int32 FirstValidPrimIndex = INDEX_NONE;
};

// Shared topology and build plan extracted from one Houdini mesh part. Both static mesh
// representations consume this data, so preparation is performed once before selecting a builder.
struct FHoudiniCachedMeshPart
{
	FHoudiniMeshToBuild MeshesToBuild;

	// Ordered names and topology for the groups that make up this part's meshes.
	TArray<FString> SplitGroups;
	TMap<FString, FPreparedHoudiniMeshSplit> Splits;

	// Vertex indices for the complete Houdini part, used to derive the per-split topology.
	TArray<int32> VertexList;

	FHoudiniCachedMeshPart();

	void ResetCache();

	bool UpdatePartVertexList(const FHoudiniGeoPartObject& InHGPO);

	// Extract the shared topology consumed by every mesh creation path.
	bool ExtractHoudiniMeshPart(const FHoudiniGeoPartObject& InHGPO);

	// Build the split-mesh creation plan after shared topology extraction.
	void PrepareSplitMeshBuildPlan(const FHoudiniGeoPartObject& InHGPO);

	void SortSplitGroups(const FHoudiniGeoPartObject& InHGPO);

	bool UpdateSplitsFacesAndIndices(const FHoudiniGeoPartObject& InHGPO);

	TArray<float>& GetPositions(const FHoudiniGeoPartObject& InHGPO);
	
	TArray<float>& GetNormals(const FHoudiniGeoPartObject& InHGPO, bool bForceRead = false);

	const HAPI_AttributeInfo& GetNormalAttributeInfo(const FHoudiniGeoPartObject& InHGPO);
	
	TArray<float>& GetTangentU(const FHoudiniGeoPartObject& InHGPO);

	const HAPI_AttributeInfo& GetTangentUAttributeInfo(const FHoudiniGeoPartObject& InHGPO);
	
	TArray<float>& GetTangentV(const FHoudiniGeoPartObject& InHGPO);

	const HAPI_AttributeInfo& GetTangentVAttributeInfo(const FHoudiniGeoPartObject& InHGPO);
	
	TArray<float>& GetColors(const FHoudiniGeoPartObject& InHGPO);

	const HAPI_AttributeInfo& GetColorAttributeInfo(const FHoudiniGeoPartObject& InHGPO);
	
	TArray<float>& GetAlphas(const FHoudiniGeoPartObject& InHGPO);

	const HAPI_AttributeInfo& GetAlphaAttributeInfo(const FHoudiniGeoPartObject& InHGPO);
	
	TArray<int32>& GetFaceSmoothingMasks(const FHoudiniGeoPartObject& InHGPO);

	const HAPI_AttributeInfo& GetFaceSmoothingMaskAttributeInfo(const FHoudiniGeoPartObject& InHGPO);
	
	TArray<TArray<float>>& GetUVSets(const FHoudiniGeoPartObject& InHGPO, bool bRemoveUnused = false);

	TArray<HAPI_AttributeInfo>& GetUVSetAttributeInfos(const FHoudiniGeoPartObject& InHGPO, bool bRemoveUnused = false);
	
	TArray<int32>& GetLightmapResolutions(const FHoudiniGeoPartObject& InHGPO);
	
	TArray<int32>& GetFaceMaterialIds(const FHoudiniGeoPartObject& InHGPO);
	
	TArray<int32>& GetUniqueMaterialIds(const FHoudiniGeoPartObject& InHGPO);

	TArray<HAPI_MaterialInfo>& GetUniqueMaterialInfos(const FHoudiniGeoPartObject& InHGPO);
	
	bool IsOnlyOneFaceMaterial(const FHoudiniGeoPartObject& InHGPO);

	TArray<FHoudiniMaterialInfo>& GetFaceMaterialOverrides(const FHoudiniGeoPartObject& InHGPO);
	
	bool HasMaterialOverrides(const FHoudiniGeoPartObject& InHGPO);

	bool HasPrimitiveMaterialOverrides(const FHoudiniGeoPartObject& InHGPO);

	bool NeedsMaterialOverrideInstance(const FHoudiniGeoPartObject& InHGPO);
	
	TArray<float>& GetLODScreensizes(const FHoudiniGeoPartObject& InHGPO);

private:

	// Lazily populated HAPI attribute data shared by every mesh representation. NOTE:
	// Each attribute may be pulled from a different owner, i.e. point, vertex, primitive or detail.
	TArray<float> PartPositions;
	HAPI_AttributeInfo AttribInfoPositions;

	TArray<float> PartNormals;
	HAPI_AttributeInfo AttribInfoNormals;
	
	TArray<float> PartTangentU;
	HAPI_AttributeInfo AttribInfoTangentU;
	
	TArray<float> PartTangentV;
	HAPI_AttributeInfo AttribInfoTangentV;
	
	TArray<float> PartColors;
	HAPI_AttributeInfo AttribInfoColors;
	
	TArray<float> PartAlphas;
	HAPI_AttributeInfo AttribInfoAlpha;
	
	TArray<int32> PartFaceSmoothingMasks;
	HAPI_AttributeInfo AttribInfoFaceSmoothingMasks;

	TArray<TArray<float>> PartUVSets;
	TArray<HAPI_AttributeInfo> AttribInfoUVSets;
	
	TArray<int32> PartLightMapResolutions;
	HAPI_AttributeInfo AttribInfoLightmapResolution;

	TArray<int32> PartFaceMaterialIds;
	HAPI_AttributeInfo AttribInfoFaceMaterialIds;
	
	TArray<int32> PartUniqueMaterialIds;
	TArray<HAPI_MaterialInfo> PartUniqueMaterialInfos;
	
	bool bOnlyOneFaceMaterial;
	TArray<FHoudiniMaterialInfo> PartFaceMaterialOverrides;
	
	bool bHaveMaterialOverrides;
	bool bHavePrimMaterialOverrides;
	bool bMaterialOverrideNeedsCreateInstance;
	
	TArray<float> PartLODScreensize;

	bool CacheTangents(const FHoudiniGeoPartObject& InHGPO);

	bool UpdateNeededMaterials(const FHoudiniGeoPartObject& InHGPO);

};

// Per-translation output and build state. This is created by the entry point and passed to the helpers that consume it.
struct FHoudiniMeshTranslationContext
{
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> InputObjects;
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> OutputObjects;
	TMap<FHoudiniOutputObjectIdentifier, FKAggregateGeom> AllAggregateCollisions;
	bool bTreatExistingMaterialsAsUpToDate = false;
	bool bForceRebuild = false;
	int32 DefaultMeshSmoothing = 1;
	FHoudiniStaticMeshGenerationProperties StaticMeshGenerationProperties;
	FMeshBuildSettings StaticMeshBuildSettings;

	void RemovePreviousOutputs();
};

struct HOUDINIENGINE_API FHoudiniMeshTranslator
{
public:

	//-----------------------------------------------------------------------------------------------------------------------------
	// HOUDINI TO UNREAL
	//-----------------------------------------------------------------------------------------------------------------------------

	// 
	static bool CreateAllMeshesAndComponentsFromHoudiniOutput(
		UHoudiniOutput* InOutput,
		const FHoudiniPackageParams& InPackageParams,
		EHoudiniStaticMeshMethod InStaticMeshMethod,
		bool bSplitMeshSupport,
		const FHoudiniStaticMeshGenerationProperties& InSMGenerationProperties,
		const FMeshBuildSettings& InMeshBuildSettings,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InAllOutputMaterials,
		UObject* InOuterComponent,
		bool bInTreatExistingMaterialsAsUpToDate=false,
		bool bInDestroyProxies=false);

	static bool CreateStaticMeshFromHoudiniGeoPartObject(
		const FHoudiniGeoPartObject& InHGPO,
		const FHoudiniPackageParams& InPackageParams,
		const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InOutputObjects,
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutOutputObjects,
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InAssignmentMaterialMap,
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterialMap,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InAllOutputMaterials,
		UObject* const InOuterComponent,
		bool InForceRebuild,
		EHoudiniStaticMeshMethod InStaticMeshMethod,
		bool bSplitMeshSupport,
		const FHoudiniStaticMeshGenerationProperties& InSMGenerationProperties,
		const FMeshBuildSettings& InMeshBuildSettings,
		bool bInTreatExistingMaterialsAsUpToDate = false);

	static bool CreateOrUpdateAllComponents(
		UHoudiniOutput* InOutput,
		UObject* InOuterComponent,
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InNewOutputObjects,
		bool bInDestroyProxies=false,
		bool bInApplyGenericProperties=true);

};

class HOUDINIENGINE_API FHoudiniProxyMeshTranslator
{
public:
	struct FBuildContext
	{
		FHoudiniGeoPartObject& HGPO;
		FHoudiniCachedMeshPart& CachedMeshPart;
		FHoudiniPackageParams& PackageParams;
		UObject* OuterComponent;
		FHoudiniMeshTranslationContext& TranslationContext;
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& OutputAssignmentMaterials;
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& ReplacementMaterials;
	};

	static bool CreateHoudiniStaticMesh(FBuildContext& InOutBuildContext);

	static bool CreateHoudiniStaticMeshesFromSplitGroups(FBuildContext& InOutBuildContext);

	static bool UpdateMeshOnHoudiniStaticMeshComponent(UHoudiniStaticMeshComponent* InComponent, UObject* InMesh);

private:
	static UHoudiniStaticMesh* CreateNewHoudiniStaticMesh(
		const FHoudiniGeoPartObject& InHGPO,
		FHoudiniPackageParams& InOutPackageParams,
		const FString& InSplitIdentifier);

	static UHoudiniStaticMesh* FindExistingHoudiniStaticMesh(
		const FHoudiniOutputObjectIdentifier& InIdentifier,
		const FHoudiniPackageParams& InPackageParams,
		const FHoudiniMeshTranslationContext& InTranslationContext);

	static bool CreateHoudiniStaticMeshFromSplitGroups(
		const FHoudiniGeoPartObject& InHGPO,
		FHoudiniCachedMeshPart& InOutCachedMeshPart,
		FHoudiniPackageParams& InOutPackageParams,
		UObject* InOuterComponent,
		FHoudiniMeshTranslationContext& InOutTranslationContext,
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials,
		const FString& Name,
		FHoudiniSplitGroupMesh& Mesh,
		TMap<HAPI_NodeId, TObjectPtr<UMaterialInterface>>& MapHoudiniMatIdToUnrealInterface,
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& MapHoudiniMatAttributesToUnrealInterface,
		TMap<UHoudiniStaticMesh*, TMap<TObjectPtr<UMaterialInterface>, int32>>& MapUnrealMaterialInterfaceToUnrealIndexPerMesh);

	static void BuildHoudiniMesh(
		const FHoudiniGeoPartObject& InHGPO,
		FHoudiniCachedMeshPart& InOutCachedMeshPart,
		const FHoudiniMeshTranslationContext& InTranslationContext,
		const FString& SplitGroupName,
		UHoudiniStaticMesh* FoundStaticMesh);

	static void ProcessMaterialsForHSM(
		const FHoudiniGeoPartObject& InHGPO,
		FHoudiniCachedMeshPart& InOutCachedMeshPart,
		const FString& SplitGroupName,
		UHoudiniStaticMesh* FoundStaticMesh,
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials,
		TMap<HAPI_NodeId, TObjectPtr<UMaterialInterface>>& MapHoudiniMatIdToUnrealInterface,
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& MapHoudiniMatAttributesToUnrealInterface,
		TMap<UHoudiniStaticMesh*, TMap<TObjectPtr<UMaterialInterface>, int32>>& MapUnrealMaterialInterfaceToUnrealIndexPerMesh);
};

class HOUDINIENGINE_API FHoudiniStaticMeshTranslator
{
public:
	struct FBuildContext
	{
		FHoudiniGeoPartObject& HGPO;
		FHoudiniCachedMeshPart& CachedMeshPart;
		FHoudiniPackageParams& PackageParams;
		UObject* OuterComponent;
		FHoudiniMeshTranslationContext& TranslationContext;
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& OutputAssignmentMaterials;
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& ReplacementMaterials;
	};

	static bool CreateStaticMesh_MeshDescription(FBuildContext& InOutBuildContext);

	static bool CreateStaticMeshesFromSplitGroups(FBuildContext& InOutBuildContext);

	static FHoudiniMeshToBuild ScanOutputForMeshesToBuild(const FHoudiniGeoPartObject& InHGPO, const TArray<FString>& InSplitGroups);

	static bool UpdateMeshOnStaticMeshComponent(UStaticMeshComponent* InComponent, UObject* InMesh);

	static bool AddActorsToMeshSocket(
		UStaticMeshSocket* Socket,
		UStaticMeshComponent* StaticMeshComponent,
		TArray<TObjectPtr<AActor>>& HoudiniCreatedSocketActors,
		TArray<TObjectPtr<AActor>>& HoudiniAttachedSocketActors);

private:
	static UStaticMesh* CreateNewUnrealStaticMesh(
		const FHoudiniGeoPartObject& InHGPO,
		FHoudiniPackageParams& InOutPackageParams,
		const FString& InMeshIdentifierString);

	static UStaticMesh* FindExistingStaticMesh(
		const FHoudiniOutputObjectIdentifier& InIdentifier,
		const FHoudiniPackageParams& InPackageParams,
		const FHoudiniMeshTranslationContext& InTranslationContext);

	static bool AddConvexCollisionToAggregate(
		const FHoudiniGeoPartObject& InHGPO,
		FHoudiniCachedMeshPart& InOutCachedMeshPart,
		const FString& SplitGroupName,
		FKAggregateGeom& AggCollisions);

	static void ApplyComplexColliderHelper(
		UStaticMesh* TargetStaticMesh,
		UStaticMesh* ComplexStaticMesh,
		EHoudiniSplitType SplitType,
		bool& AssignedCustomCollisionMesh,
		FHoudiniOutputObject* OutputObject);

	static void SetPhysicsMaterialFromHGPO(const FHoudiniGeoPartObject& InHGPO, UBodySetup* BodySetup);

	static void BuildMeshDescription(
		FHoudiniCachedMeshPart& InOutCachedMeshPart,
		const FHoudiniGeoPartObject& InHGPO,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutputAssignmentMaterials,
		int32 InDefaultMeshSmoothing,
		FMeshDescription* MeshDesc,
		FHoudiniGroupedMeshPrimitives& SplitMeshData);

	static void ProcessMaterials(
		const FHoudiniGeoPartObject& InHGPO,
		FHoudiniCachedMeshPart& InOutCachedMeshPart,
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials,
		UStaticMesh* FoundStaticMesh,
		FHoudiniGroupedMeshPrimitives& SplitMeshData);

	static void PullMeshData(
		const FHoudiniGeoPartObject& InHGPO,
		FHoudiniCachedMeshPart& InOutCachedMeshPart,
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials,
		FHoudiniGroupedMeshPrimitives& SplitMeshData,
		UStaticMesh* FoundStaticMesh,
		int LODIndex,
		bool bReadTangents);

	static void ClassifySplitGroup(FHoudiniGroupedMeshPrimitives& SplitMeshData);

	static UStaticMesh* CreateStaticMesh(const FHoudiniGeoPartObject& InHGPO, FHoudiniPackageParams& InOutPackageParams, const FString& Name, int NumLODs);

	static bool CreateStaticMeshFromSplitGroups(
		const FHoudiniGeoPartObject& InHGPO,
		FHoudiniCachedMeshPart& InOutCachedMeshPart,
		FHoudiniPackageParams& InOutPackageParams,
		FHoudiniMeshTranslationContext& InOutTranslationContext,
		TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials,
		const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InReplacementMaterials,
		const FString& Name,
		FHoudiniSplitGroupMesh& Mesh);

	static bool ParseSplitToken(FString& Name, const FString& Token);
};
