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
#include "CoreMinimal.h"
#include "HoudiniSkeletalMeshUtils.h"
#include "ImportUtils/SkeletalMeshImportUtils.h"
#include "PhysicsEngine/BoxElem.h"
#include "Rendering/SkeletalMeshLODImporterData.h"

class USkeletalMesh;
class USkeleton;

struct FHoudiniSkeletalMesh
{
    TArray<FVector3f> Positions;
    TArray<TArray<float>> UVSets;
    TArray<float> Tangents;
    TArray<FVector3f> Normals;
    TArray<float> Colors;
    TArray<int> Vertices;
    TArray<HAPI_AttributeInfo> AttribInfoUVSets;
    HAPI_AttributeInfo ColorInfo;
    FHoudiniSkeletalMeshMaterialSettings Materials;

};

struct FHoudiniSkeletalMeshParts
{
    const FHoudiniGeoPartObject* HGPOShapeInstancer = nullptr;
	const FHoudiniGeoPartObject* HGPOShapeMesh = nullptr;
	const FHoudiniGeoPartObject* HGPOPoseInstancer = nullptr;
	const FHoudiniGeoPartObject* HGPOPoseMesh = nullptr;
    const FHoudiniGeoPartObject* HGPOPhysAssetInstancer = nullptr;
    const FHoudiniGeoPartObject* HGPOPhysAssetMesh = nullptr;

    const FHoudiniGeoPartObject* GetShapeInstancer() const { return HGPOShapeInstancer; }
    bool HasRestShape() const { return HGPOShapeInstancer && HGPOShapeMesh; }
    bool HasSkeleton() const { return HGPOPoseInstancer && HGPOPoseMesh; }

};


struct FHoudiniSkeletalMeshBuildSettings
{
    FSkeletalMeshImportData SkeletalMeshImportData;
    float ImportScale = 1.0f;
    USkeletalMesh* SKMesh = nullptr;
    UPackage* SKPackage = nullptr;
    USkeleton* Skeleton = nullptr;
    FString CurrentObjectName;
    bool ImportNormals = false;
};


struct HOUDINIENGINE_API FHoudiniSkeletalMeshTranslator
{
public:

    // Check whether the packed primitive is skeleton Rest Geometry
    static bool IsRestShapeInstancer(HAPI_NodeId GeoId, HAPI_PartId PartId, FString& OutBaseName, HAPI_NodeId & MeshPartId);
    static bool IsRestShapeMesh(HAPI_NodeId GeoId, HAPI_PartId PartId);

    // Check whether the packed primitive is skeleton Rest Geometry
    static bool IsCapturePoseInstancer(HAPI_NodeId GeoId, HAPI_PartId PartId, FString& OutBaseName, HAPI_PartId &PoseCurveId);
    static bool IsCapturePoseMesh(HAPI_NodeId GeoId, const HAPI_NodeId PartId);

    // Check whether the packed primitive is a Phys Asset
    static bool IsPhysAssetInstancer(HAPI_NodeId GeoId, HAPI_PartId PartId, FString& OutBaseName, HAPI_PartId& PoseCurveId);
    static bool IsPhysAssetMesh(HAPI_NodeId GeoId, const HAPI_NodeId PartId);

    // Creates all skeletal mesh assets and component for a given HoudiniOutput
    static bool ProcessSkeletalMeshOutputs(
        UHoudiniOutput* InOutput,
        const FHoudiniPackageParams& InPackageParams,
        TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& InAllOutputMaterials,
        UObject* InOuterComponent);

protected:
    // Creates a skeletal mesh assets and component for a given HoudiniOutput
    static bool ProcessSkeletalMeshParts(
        const FHoudiniSkeletalMeshParts& SKParts,
        const FHoudiniPackageParams& InPackageParams,
        UObject* InOuterComponent,
        TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutOutputObjects,
        TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& AssignmentMaterialMap,
        TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& ReplacementMaterialMap,
        const TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& InAllOutputMaterials);

    // Creates SkeletalMesh and Skeleton Assets and Packages, and adds them to OutputObjects
    bool ProcessSkeletalMeshParts();        

    // Builds Skeletal Mesh and Skeleton Assets from FSkeletalMeshImportData
    static void CreateUnrealData(FHoudiniSkeletalMeshBuildSettings& BuildSettings);

	// Creates skeletal mesh import data from intermediate structures
    bool CreateSkeletalMeshImportData(
        FSkeletalMeshImportData & SkeletalMeshImportData,
        const FHoudiniSkeletalMesh & Mesh, 
        const FHoudiniSkeleton & Skeleton, 
        const FHoudiniInfluences & SkinWeights, 
        const FHoudiniPackageParams& InPackageParams);

    bool SetSkeletalMeshImportDataMesh(
        FSkeletalMeshImportData& SkeletalMeshImportData,
        const FHoudiniSkeletalMesh& Mesh,
        const FHoudiniPackageParams& PackageParams);

    bool SetSkeletalMeshImportDataInfluences(
        FSkeletalMeshImportData& SkeletalMeshImportData,
        const FHoudiniInfluences& SkinWeights,
        const FHoudiniPackageParams& PackageParams);

    static bool SetSkeletalMeshImportDataSkeleton(
        FSkeletalMeshImportData& SkeletalMeshImportData,
        const FHoudiniSkeleton& Skeleton,
        const FHoudiniPackageParams& InPackageParams);

    FHoudiniSkeletalMesh GetSkeletalMeshMeshData(HAPI_NodeId ShapeGeoId , HAPI_PartId PartId, bool bImportNormals);

    static float GetSkeletonImportScale(const FHoudiniGeoPartObject& ShapeMeshHGPO);

    static bool FindAttributeOnSkeletalMeshShapeParts(const FHoudiniSkeletalMeshParts& InSKParts, const char* Attribute, HAPI_NodeId& OutGeoId, HAPI_PartId& OutPartId);

    FHoudiniSkeletalMeshMaterialSettings GetMaterials(HAPI_NodeId ShapeGeoId, HAPI_PartId PartId, int NumFaces);

    bool LoadOrCreateMaterials(
        FHoudiniSkeletalMeshMaterialSettings MaterialSettings,
        const FHoudiniPackageParams& InPackageParams,
        TArray<int32>& OutPerFaceUEMaterialIds,
        FSkeletalMeshImportData& OutImportData);

    USkeletalMesh* CreateNewSkeletalMesh(const FString& InSplitIdentifier);
    USkeleton* CreateNewSkeleton(const FString& InSplitIdentifier);
    UPhysicsAsset * CreateNewPhysAsset(const FString& InSplitIdentifier);

    // The HoudiniGeoPartObjects we're working on
    FHoudiniSkeletalMeshParts SKParts;
    // Structure that handles cooking/baking package creation parameters
    FHoudiniPackageParams SkinnedMeshPackageParams;
    FHoudiniPackageParams SkeletonPackageParams;
    FHoudiniPackageParams PhysAssetPackageParams;

    // New Output objects
    TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> OutputObjects;
    // Input Material Map
    TMap<FHoudiniMaterialIdentifier, UMaterialInterface*> InputAssignmentMaterials;
    // Output Material Map
    TMap<FHoudiniMaterialIdentifier, UMaterialInterface*> OutputAssignmentMaterials;
    // Input Replacement Materials maps
    TMap<FHoudiniMaterialIdentifier, UMaterialInterface*> ReplacementMaterials;
    // All the materials that have been generated by this Houdini Asset
    // Used to avoid generating the same houdini material over and over again
    TMap<FHoudiniMaterialIdentifier, UMaterialInterface*> AllOutputMaterials;
    // Outer object for attaching components to
    UObject* OuterComponent = nullptr;

	// Helper to IsRestGeometry* / IsCapturePose* functions
	static HAPI_AttributeInfo GetAttrInfo(HAPI_NodeId GeoId, HAPI_NodeId PartId, const char* AttrName, HAPI_AttributeOwner AttrOwner);

    // Functions to determine if we should create a default physics asset attributes.
    bool GetCreateDefaultPhysicsAssetAttributeSet();
    TOptional<bool> GetCreateDefaultPhysicsAssetAttributeSet(const FHoudiniGeoPartObject* GeoPart);

    // Sets the Physics Assets collision from the HGPO.
    void SetPhysicsAssetFromHGPO(UPhysicsAsset* PhysicsAsset, const FHoudiniSkeleton& Skeleton, const FHoudiniGeoPartObject& HGPO);

    // Gets all points for the given bone name.
    static TArray<FVector> GetPointForPhysicsBone(const FHoudiniSkeleton& Skeleton, const FString& BoneName, const TArray<int> PointIndices, const TArray<float>& Points);

    // Getall point indices for the given collision group. Grouped by Bone
    static TMap<FString, TArray<int>> ExtractBoneGroup(const TArray<FString> & BoneNames, const FHoudiniGeoPartObject& HGPO, const HAPI_PartInfo & PartInfo, const FString & GroupName);

    // Gets (or creates) the UBodySetup for the named bone.
    static UBodySetup* GetBodySetup(UPhysicsAsset* PhysicsAsset, const FString& BoneName);

	// Functions for getting an existing Physics Asset, if specified
    UPhysicsAsset *  GetExistingPhysicsAssetFromParts();
    static FString GetPhysicAssetRef(const FHoudiniGeoPartObject* GeoPart);

};