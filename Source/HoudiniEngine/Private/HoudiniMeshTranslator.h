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

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PhysicsEngine/AggregateGeom.h"

//#include "HoudiniMeshTranslator.generated.h"

class UStaticMesh;
class UStaticMeshSocket;
class UMaterialInterface;
class UMeshComponent;
class UStaticMeshComponent;
class UHoudiniStaticMesh;
class UHoudiniStaticMeshComponent;

struct FKAggregateGeom;
struct FHoudiniGenericAttribute;


UENUM()
enum class EHoudiniSplitType : uint8
{
	Invalid,

	Normal,

	LOD,

	RenderedComplexCollider,
	InvisibleComplexCollider,

	RenderedUCXCollider,
	InvisibleUCXCollider,

	RenderedSimpleCollider,
	InvisibleSimpleCollider
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
			const EHoudiniStaticMeshMethod& InStaticMeshMethod,
			const FHoudiniStaticMeshGenerationProperties& InSMGenerationProperties,
			const FMeshBuildSettings& InMeshBuildSettings,
			const TMap<FString, UMaterialInterface*>& InAllOutputMaterials,
			UObject* InOuterComponent,
			bool bInTreatExistingMaterialsAsUpToDate=false,
			bool bInDestroyProxies=false);
	
		static bool CreateStaticMeshFromHoudiniGeoPartObject(
			const FHoudiniGeoPartObject& InHGPO,
			const FHoudiniPackageParams& InPackageParams,
			const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InOutputObjects,
			TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutOutputObjects,
			TMap<FString, UMaterialInterface*>& InAssignmentMaterialMap,
			TMap<FString, UMaterialInterface*>& InReplacementMaterialMap,
			const TMap<FString, UMaterialInterface*>& InAllOutputMaterials,
			UObject* const InOuterComponent,
			const bool& InForceRebuild,
			const EHoudiniStaticMeshMethod& InStaticMeshMethod,
			const FHoudiniStaticMeshGenerationProperties& InSMGenerationProperties,
			const FMeshBuildSettings& InMeshBuildSettings,
			bool bInTreatExistingMaterialsAsUpToDate = false);

		static bool CreateOrUpdateAllComponents(
			UHoudiniOutput* InOutput,
			UObject* InOuterComponent,
			TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InNewOutputObjects,
			bool bInDestroyProxies=false,
			bool bInApplyGenericProperties=true);


		//-----------------------------------------------------------------------------------------------------------------------------
		// HELPERS
		//-----------------------------------------------------------------------------------------------------------------------------
		static EHoudiniSplitType GetSplitTypeFromSplitName(const FString& InSplitName);

		static FString GetMeshIdentifierFromSplit(const FString& InSplitName, const EHoudiniSplitType& InSplitType);

		// TODO: Rename me! and template me! float/int/string ?
		// TransferPartAttributesToSplitVertices
		static int32 TransferRegularPointAttributesToVertices(
			const TArray<int32>& InVertexList,
			const HAPI_AttributeInfo& InAttribInfo,
			const TArray<float>& InData,
			TArray<float>& OutVertexData);

		template <typename TYPE>
		static int32 TransferPartAttributesToSplit(
			const TArray<int32>& InVertexList,
			const HAPI_AttributeInfo& InAttribInfo,
			const TArray<TYPE>& InData,
			TArray<TYPE>& OutSplitData);

		// Try to find the named InPropertyName property on the source model at InSourceModelIndex on InStaticMesh.
		static bool TryToFindPropertyOnSourceModel(
			UStaticMesh* const InStaticMesh,
			const int32 InSourceModelIndex,
			const FString& InPropertyName,
			FEditPropertyChain& InPropertyChain,
			bool& bOutSkipDefaultIfPropertyNotFound,
			FProperty*& OutFoundProperty,
			UObject*& OutFoundPropertyObject,
			void*& OutContainer);

		// Try to the find the named InPropertyName property on InSourceModel.
		static bool TryToFindPropertyOnSourceModel(
			FStaticMeshSourceModel& InSourceModel,
			const FString& InPropertyName,
			FEditPropertyChain& InPropertyChain,
			FProperty*& OutFoundProperty,
			void*& OutContainer);

		// Update the MeshBuild Settings using the values from the runtime settings/overrides on the HAC
		void UpdateMeshBuildSettings(
			FMeshBuildSettings& OutMeshBuildSettings,
			const bool& bHasNormals,
			const bool& bHasTangents,
			const bool& bHasLightmapUVSet);

		// Update the NaniteSettings for a given Static Mesh using attribute values
		void UpdateStaticMeshNaniteSettings(
		    const int32& GeoId, const int32& PartId, const int32& PrimIndex, UStaticMesh* StaticMesh);

		// Copy supported (non-generic) attributes from the split by point/prim index.
		void CopyAttributesFromHGPOForSplit(
			const int32 InPointIndex,
			const int32 InPrimIndex,
			TMap<FString, FString>& OutAttributes,
			TMap<FString, FString>& OutTokens);
	
		// Copy supported (non-generic) attributes from the split
		void CopyAttributesFromHGPOForSplit(
			const FString& InSplitGroupName, TMap<FString, FString>& OutAttributes, TMap<FString, FString>& OutTokens);

		// Copy supported (non-generic) attributes from the split via outputobjectidentifier
		void CopyAttributesFromHGPOForSplit(
			const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier, TMap<FString, FString>& OutAttributes, TMap<FString, FString>& OutTokens);

		//-----------------------------------------------------------------------------------------------------------------------------
		// ACCESSORS
		//-----------------------------------------------------------------------------------------------------------------------------

		//-----------------------------------------------------------------------------------------------------------------------------
		// MUTATORS
		//-----------------------------------------------------------------------------------------------------------------------------
		void SetHoudiniGeoPartObject(const FHoudiniGeoPartObject& InHGPO) { HGPO = InHGPO; };
		void SetOuterComponent(UObject* InOuter) { OuterComponent = InOuter; };
		void SetPackageParams(const FHoudiniPackageParams& InPackageParams, const bool& bUpdateHGPO = false);

		void SetInputObjects(const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InInputObjects) { InputObjects = InInputObjects; };
		void SetOutputObjects(TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InOutputObjects) { OutputObjects = InOutputObjects; };

		void SetInputAssignmentMaterials(const TMap<FString, UMaterialInterface*>& InInputMaterials) { InputAssignmentMaterials = InInputMaterials; };
		void SetReplacementMaterials(const TMap<FString, UMaterialInterface*>& InReplacementMaterials) { ReplacementMaterials = InReplacementMaterials; };
		void SetAllOutputMaterials(const TMap<FString, UMaterialInterface*>& InAllOutputMaterials) { AllOutputMaterials = InAllOutputMaterials; };

		//void SetInputObjectProperties(const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObjectProperty>& InInputObjectProperties) { InputObjectProperties = InInputObjectProperties; };
		//void SetOutputObjectProperties(TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObjectProperty>& InOutputObjectProperties) { OutputObjectProperties = InOutputObjectProperties; };

		void SetTreatExistingMaterialsAsUpToDate(bool bInTreatExistingMaterialsAsUpToDate) { bTreatExistingMaterialsAsUpToDate = bInTreatExistingMaterialsAsUpToDate; }

		void SetStaticMeshGenerationProperties(const FHoudiniStaticMeshGenerationProperties& InStaticMeshGenerationProperties) { StaticMeshGenerationProperties = InStaticMeshGenerationProperties; };

		void SetStaticMeshBuildSettings(const FMeshBuildSettings& InMBS) { StaticMeshBuildSettings = InMBS; };

	protected:

		// Create a StaticMesh using the MeshDescription format
		bool CreateStaticMesh_MeshDescription();

		// Legacy function using RawMesh for static Mesh creation
		bool CreateStaticMesh_RawMesh();

		// Create a UHoudiniStaticMesh
		bool CreateHoudiniStaticMesh();

		// Helper to make and populate a FHoudiniOutputObjectIdentifier from the current HGPO and the given
		// InSplitGroupName and InSplitType.
		FHoudiniOutputObjectIdentifier MakeOutputObjectIdentifier(const FString& InSplitGroupName, const EHoudiniSplitType InSplitType);

		static void ApplyComplexColliderHelper(
			UStaticMesh* TargetStaticMesh,
			UStaticMesh* ComplexStaticMesh,
			const EHoudiniSplitType SplitType,
			bool& AssignedCustomCollisionMesh,
			FHoudiniOutputObject* OutputObject);

		void ResetPartCache();

		bool UpdatePartVertexList();

		void SortSplitGroups();
				
		bool UpdateSplitsFacesAndIndices();

		// Update this part's position cache if we haven't already
		bool UpdatePartPositionIfNeeded();

		// Update this part's normal cache if we haven't already
		bool UpdatePartNormalsIfNeeded();

		// Update this part's tangent and binormal caches if we haven't already
		bool UpdatePartTangentsIfNeeded();

		// Update this part's color cache if we haven't already
		bool UpdatePartColorsIfNeeded();

		// Update this part's alpha if we haven't already
		bool UpdatePartAlphasIfNeeded();

		// Update this part's face smoothing values if we haven't already
		bool UpdatePartFaceSmoothingIfNeeded();

		// Update this part's UV sets if we haven't already
		bool UpdatePartUVSetsIfNeeded(const bool& bRemoveUnused = false);

		// Update this part;s lightmap resolution cache if we haven't already
		bool UpdatePartLightmapResolutionsIfNeeded();

		// Update this part's lod screensize attribute cache if we haven't already
		bool UpdatePartLODScreensizeIfNeeded();

		// Update th unique materials ids and infos needed for this part using the face materials and overrides
		bool UpdatePartNeededMaterials();

		// Update this part's face material IDs, unique material IDs and material Infos caches if we haven't already
		bool UpdatePartFaceMaterialIDsIfNeeded();

		// Update this part's material overrides cache if we haven't already
		bool UpdatePartFaceMaterialOverridesIfNeeded();

		// Updates and create the material that are needed for this part
		bool CreateNeededMaterials();

		UStaticMesh* CreateNewStaticMesh(const FString& InMeshIdentifierString);

		UStaticMesh* FindExistingStaticMesh(const FHoudiniOutputObjectIdentifier& InIdentifier);

		UHoudiniStaticMesh* CreateNewHoudiniStaticMesh(const FString& InMeshIdentifierString);

		UHoudiniStaticMesh* FindExistingHoudiniStaticMesh(const FHoudiniOutputObjectIdentifier& InIdentifier);

		float GetLODSCreensizeForSplit(const FString& SplitGroupName);

		// Create convex/UCX collider for a split and add to the aggregate
		bool AddConvexCollisionToAggregate(const FString& SplitGroupName, FKAggregateGeom& AggCollisions);
		// Create simple colliders for a split and add to the aggregate
		bool AddSimpleCollisionToAggregate(const FString& SplitGroupName, FKAggregateGeom& AggCollisions);
		
		// Helper functions to generate the simple colliders and add them to the aggregate
		static int32 GenerateBoxAsSimpleCollision(const TArray<FVector>& InPositionArray, FKAggregateGeom& OutAggregateCollisions);
		static int32 GenerateOrientedBoxAsSimpleCollision(const TArray<FVector>& InPositionArray, FKAggregateGeom& OutAggregateCollisions);
		static int32 GenerateSphereAsSimpleCollision(const TArray<FVector>& InPositionArray, FKAggregateGeom& OutAggregateCollisions);
		static int32 GenerateSphylAsSimpleCollision(const TArray<FVector>& InPositionArray, FKAggregateGeom& OutAggregateCollisions);
		static int32 GenerateOrientedSphylAsSimpleCollision(const TArray<FVector>& InPositionArray, FKAggregateGeom& OutAggregateCollisions);
		static int32 GenerateKDopAsSimpleCollision(const TArray<FVector>& InPositionArray, const TArray<FVector> &Dirs, FKAggregateGeom& OutAggregateCollisions);

		// Helper functions for the simple colliders generation
		static void CalcBoundingBox(const TArray<FVector>& PositionArray, FVector& Center, FVector& Extents, FVector& LimitVec);
		static void CalcBoundingSphere(const TArray<FVector>& PositionArray, FSphere& sphere, FVector& LimitVec);
		static void CalcBoundingSphere2(const TArray<FVector>& PositionArray, FSphere& sphere, FVector& LimitVec);
		static void CalcBoundingSphyl(const TArray<FVector>& PositionArray, FSphere& sphere, float& length, FRotator& rotation, FVector& LimitVec);
		
		// Helper functions to remove unused/stale components
		static bool RemoveAndDestroyComponent(UObject* InComponent);

		// Helper to create a new mesh component
		static UMeshComponent* CreateMeshComponent(UObject *InOuterComponent, const TSubclassOf<UMeshComponent>& InComponentType);

		// Helper to update an existing mesh component
		static void UpdateMeshComponent(UMeshComponent *InMeshComponent, UObject *InMesh, const FHoudiniOutputObjectIdentifier &InOutputIdentifier,
			const FHoudiniGeoPartObject *InHGPO, TArray<AActor*> & HoudiniCreatedSocketActors, TArray<AActor*> & HoudiniAttachedSocketActors,
			bool bInApplyGenericProperties=true);

		// Helper to create or update a mesh component for a UStaticMesh or proxy mesh output
		static UMeshComponent* CreateOrUpdateMeshComponent(
			const UHoudiniOutput* InOutput,
			UObject* InOuterComponent,
			const FHoudiniOutputObjectIdentifier& InOutputIdentifier,
			const TSubclassOf<UMeshComponent>& InComponentType,
			FHoudiniOutputObject& OutOutputObject,
			FHoudiniGeoPartObject const *& OutFoundHGPO,
			bool &bCreated);

		// Helper to set or update the mesh on UStaticMeshComponent
		static bool UpdateMeshOnStaticMeshComponent(UStaticMeshComponent *InComponent, UObject *InMesh);

		// Helper to set or update the mesh on UHoudiniStaticMeshComponent
		static bool UpdateMeshOnHoudiniStaticMeshComponent(UHoudiniStaticMeshComponent *InComponent, UObject *InMesh);

		static bool AddActorsToMeshSocket(UStaticMeshSocket * Socket, UStaticMeshComponent * StaticMeshComponent, 
			TArray<AActor*>& HoudiniCreatedSocketActors, TArray<AActor*>& HoudiniAttachedSocketActors);


		static bool HasFracturePieceAttribute(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId);
	protected:

		// Data cache for this translator

		// The HoudiniGeoPartObject we're working on
		FHoudiniGeoPartObject HGPO;

		// Outer object for attaching components to
		UObject* OuterComponent;

		// Structure that handles cooking/baking package creation parameters
		FHoudiniPackageParams PackageParams;


		// Previous output objects
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> InputObjects;

		// New Output objects
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> OutputObjects;


		// Input Material Map
		TMap<FString, UMaterialInterface*> InputAssignmentMaterials;
		// Output Material Map
		TMap<FString, UMaterialInterface*> OutputAssignmentMaterials;
		// Input Replacement Materials maps
		TMap<FString, UMaterialInterface*> ReplacementMaterials;
		// All the materials that have been generated by this Houdini Asset
		// Used to avoid generating the same houdini material over and over again
		TMap<FString, UMaterialInterface*> AllOutputMaterials;

		// Input mesh properties
		//TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObjectProperty> InputObjectProperties;
		// Output mesh properties
		//TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObjectProperty> OutputObjectProperties;

		// Indicates the update is forced
		bool ForceRebuild;

		// The generated simple/UCX colliders
		TMap <FHoudiniOutputObjectIdentifier, FKAggregateGeom> AllAggregateCollisions;

		// Names of the groups used for splitting the geometry
		TArray<FString> AllSplitGroups;

		// Per-split lists of faces
		TMap<FString, TArray<int32>> AllSplitVertexLists;

		// Per-split number of faces
		TMap<FString, int32> AllSplitVertexCounts;

		// Per-split indices arrays
		TMap<FString, TArray<int32>> AllSplitFaceIndices;

		// Per-split first valid vertex index
		TMap<FString, int32> AllSplitFirstValidVertexIndex;

		// Per-split first valid prim index
		TMap<FString, int32> AllSplitFirstValidPrimIndex;

		// Vertex Indices for the part
		TArray<int32> PartVertexList;

		// Positions
		TArray<float> PartPositions;
		HAPI_AttributeInfo AttribInfoPositions;

		// Vertex Normals
		TArray<float> PartNormals;
		HAPI_AttributeInfo AttribInfoNormals;

		// Vertex TangentU
		TArray<float> PartTangentU;
		HAPI_AttributeInfo AttribInfoTangentU;

		// Vertex TangentV
		TArray<float> PartTangentV;
		HAPI_AttributeInfo AttribInfoTangentV;

		// Vertex Colors
		TArray<float> PartColors;
		HAPI_AttributeInfo AttribInfoColors;

		// Vertex Alpha values
		TArray<float> PartAlphas;
		HAPI_AttributeInfo AttribInfoAlpha;

		// Face Smoothing masks
		TArray<int32> PartFaceSmoothingMasks;
		HAPI_AttributeInfo AttribInfoFaceSmoothingMasks;

		// UVs
		TArray<TArray<float>> PartUVSets;
		TArray<HAPI_AttributeInfo> AttribInfoUVSets;

		// Lightmap resolution
		TArray<int32> PartLightMapResolutions;
		HAPI_AttributeInfo AttribInfoLightmapResolution;

		// Material IDs per face
		TArray<int32> PartFaceMaterialIds;
		HAPI_AttributeInfo AttribInfoFaceMaterialIds;
		// Unique material IDs
		TArray<int32> PartUniqueMaterialIds;
		//TSet<int32> PartUniqueMaterialIds;
		// Material infos for each unique Material
		TArray<HAPI_MaterialInfo> PartUniqueMaterialInfos;
		//TSet<HAPI_MaterialInfo> PartUniqueMaterialInfos;
		// Indicates we only have a single face material
		bool bOnlyOneFaceMaterial;

		// Material Overrides per face
		TArray<FString> PartFaceMaterialOverrides;
		HAPI_AttributeInfo AttribInfoFaceMaterialOverrides;
		// Indicates that material overides attributes need an instance to be created
		bool bMaterialOverrideNeedsCreateInstance;

		// LOD Screensize
		TArray<float> PartLODScreensize;
		HAPI_AttributeInfo AttribInfoLODScreensize;

		int32 DefaultMeshSmoothing;

		// When building a mesh, if an associated material already exists, treat
		// it as up to date, regardless of the MaterialInfo.bHasChanged flag
		bool bTreatExistingMaterialsAsUpToDate;

		// Default properties to be used when generating Static Meshes
		FHoudiniStaticMeshGenerationProperties StaticMeshGenerationProperties;

		// Default Mesh Build settings to be used when generating Static Meshes
		FMeshBuildSettings StaticMeshBuildSettings;
};
