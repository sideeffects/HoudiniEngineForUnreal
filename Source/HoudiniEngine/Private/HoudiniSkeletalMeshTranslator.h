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
//#include "HoudiniAssetComponent.h"
//#include "HoudiniMaterialTranslator.h"

#include "CoreMinimal.h"
//#include "UObject/ObjectMacros.h"
#include "ImportUtils/SkeletalMeshImportUtils.h"
#include "Rendering/SkeletalMeshLODImporterData.h"

class USkeletalMesh;
class USkeleton;


struct FHoudiniSkeletalMeshParts
{
    const FHoudiniGeoPartObject* HGPOShapeInstancer = nullptr;
	const FHoudiniGeoPartObject* HGPOShapeMesh = nullptr;
	const FHoudiniGeoPartObject* HGPOPoseInstancer = nullptr;
	const FHoudiniGeoPartObject* HGPOPoseMesh = nullptr;

    const FHoudiniGeoPartObject* GetMainHGPO() const { return HGPOShapeInstancer; }

    bool IsValid() const { return HGPOShapeInstancer && HGPOShapeMesh && HGPOPoseInstancer && HGPOPoseMesh; }
};


struct SKBuildSettings
{
    FSkeletalMeshImportData SkeletalMeshImportData;
    bool bIsNewSkeleton = false;
    float ImportScale = 1.0f;
    USkeletalMesh* SKMesh = nullptr;
    UPackage* SKPackage = nullptr;
    USkeleton* Skeleton = nullptr;
    FString CurrentObjectName;
    FHoudiniSkeletalMeshParts SKParts;
    bool ImportNormals = false;
    bool OverwriteSkeleton = false;
    FString SkeletonAssetPath = "";
    int NumTexCoords = 1;
};


struct HOUDINIENGINE_API FHoudiniSkeletalMeshTranslator
{
    public:
    
        // Check whether the packed primitive is skeleton Rest Geometry
        static bool IsRestGeometryInstancer(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId, FString& OutBaseName);
        static bool IsRestGeometryMesh(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId);

        // Check whether the packed primitive is skeleton Rest Geometry
        static bool IsCapturePoseInstancer(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId, FString& OutBaseName);
        static bool IsCapturePoseMesh(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId);

protected:

        // Helper to IsRestGeometry* / IsCapturePose* functions
        static HAPI_AttributeInfo GetAttrInfo(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId, const char* AttrName, HAPI_AttributeOwner AttrOwner);

public:
        
        // Creates all skeletal mesh assets and component for a given HoudiniOutput
        static bool CreateAllSkeletalMeshesAndComponentsFromHoudiniOutput(
            UHoudiniOutput* InOutput,
            const FHoudiniPackageParams& InPackageParams,
            TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& InAllOutputMaterials,
            UObject* InOuterComponent);

        // Creates a skeletal mesh assets and component for a given HoudiniOutput
        static bool CreateSkeletalMeshFromHoudiniGeoPartObject(
            const FHoudiniSkeletalMeshParts& SKParts,
            const FHoudiniPackageParams& InPackageParams,
            UObject* InOuterComponent,
            const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InOutputObjects, 
            TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutOutputObjects,
            TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& AssignmentMaterialMap,
            TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& ReplacementMaterialMap,
            const TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& InAllOutputMaterials);

        // Creates a skeletal mesh by using FSkeletalMeshImportData
        // Creates SkelatalMesh and Skeleton Assets and Packages, and adds them to OutputObjects
        bool CreateSkeletalMesh_SkeletalMeshImportData();        

        // Builds Skeletal Mesh and Skeleton Assets from FSkeletalMeshImportData
        static void BuildSKFromImportData(SKBuildSettings& BuildSettings);

        // Fills the FSkeletalMeshImportData with data from HAPI
        bool FillSkeletalMeshImportData(SKBuildSettings& BuildSettings, const FHoudiniPackageParams& InPackageParams);

        //
        static void UpdateBuildSettings(SKBuildSettings& BuildSettings);

        static bool FindAttributeOnSkeletalMeshShapeParts(const FHoudiniSkeletalMeshParts& InSKParts, const char* Attribute, HAPI_NodeId& OutGeoId, HAPI_PartId& OutPartId);

        bool CreateSkeletalMeshMaterials(
            const FHoudiniGeoPartObject& InShapeMeshHGPO,
            const HAPI_PartInfo& InShapeMeshPartInfo,
            const FHoudiniPackageParams& InPackageParams,
            TArray<int32>& OutPerFaceUEMaterialIds,
            FSkeletalMeshImportData& OutImportData);

        //-----------------------------------------------------------------------------------------------------------------------------
        // MUTATORS
        //-----------------------------------------------------------------------------------------------------------------------------
        void SetHoudiniSkeletalMeshParts(const FHoudiniSkeletalMeshParts& InSKParts) { SKParts = InSKParts; };
        void SetPackageParams(const FHoudiniPackageParams& InPackageParams, const bool& bUpdateHGPO = false);
        void SetInputObjects(const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InInputObjects) { InputObjects = InInputObjects; };
        void SetOutputObjects(const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InOutputObjects) { OutputObjects = InOutputObjects; };
        void SetOuterComponent(UObject* InOuterComponent) { OuterComponent = InOuterComponent; }
        void SetInputAssignmentMaterials(const TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& InInputMaterials) { InputAssignmentMaterials = InInputMaterials; };
        void SetReplacementMaterials(const TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& InReplacementMaterials) { ReplacementMaterials = InReplacementMaterials; };
        void SetAllOutputMaterials(const TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& InAllOutputMaterials) { AllOutputMaterials = InAllOutputMaterials; };

        // Current / Previous Output objects
        TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> InputObjects;

        // New Output objects
        TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> OutputObjects;

    protected:

        // The HoudiniGeoPartObjects we're working on
        FHoudiniSkeletalMeshParts SKParts;
        // Structure that handles cooking/baking package creation parameters
        FHoudiniPackageParams PackageParams;
        
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
        

        USkeletalMesh* CreateNewSkeletalMesh(const FString& InSplitIdentifier);
        USkeleton* CreateNewSkeleton(const FString& InSplitIdentifier) const;
};