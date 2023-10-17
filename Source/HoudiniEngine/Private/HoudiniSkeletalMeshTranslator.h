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

struct SKBuildSettings
{
    FSkeletalMeshImportData SkeletalMeshImportData;
    bool bIsNewSkeleton = false;
    float ImportScale = 1.0f;
    USkeletalMesh* SKMesh = nullptr;
    UPackage* SKPackage = nullptr;
    USkeleton* Skeleton = nullptr;
    FString CurrentObjectName;
    HAPI_NodeId GeoId = -1;
    HAPI_NodeId PartId = -1;
    bool ImportNormals = false;
    bool OverwriteSkeleton = false;
    FString SkeletonAssetPath = "";
    int NumTexCoords = 1;
};


struct HOUDINIENGINE_API FHoudiniSkeletalMeshTranslator
{
    public:        
    
        // 
        static bool CreateAllSkeletalMeshesAndComponentsFromHoudiniOutput(
            UHoudiniOutput* InOutput,
            const FHoudiniPackageParams& InPackageParams,
            const TMap<FHoudiniMaterialIdentifier, UMaterialInterface*>& InAllOutputMaterials,
            UObject* InOuterComponent);

        //
        static bool CreateSkeletalMeshFromHoudiniGeoPartObject(
            const FHoudiniGeoPartObject& InHGPO,
            const FHoudiniPackageParams& InPackageParams,
            const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InOutputObjects,
            TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutOutputObjects);

        bool CreateSkeletalMesh_SkeletalMeshImportData();        

        static void ExportSkeletalMeshAssets(UHoudiniOutput* InOutput);
        static bool HasSkeletalMeshData(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId);
        static void LoadImportData(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId);
        static void CreateSKAssetAndPackage(SKBuildSettings& BuildSettings, const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId, FString PackageName, int MaxInfluences = 1, bool ImportNormals = false);
        static void BuildSKFromImportData(SKBuildSettings& BuildSettings, TArray<FSkeletalMaterial>& Materials);
        static void SKImportData(SKBuildSettings& BuildSettings);
        static USkeleton* CreateOrUpdateSkeleton(SKBuildSettings& BuildSettings);

        //-----------------------------------------------------------------------------------------------------------------------------
        // MUTATORS
        //-----------------------------------------------------------------------------------------------------------------------------
        void SetHoudiniGeoPartObject(const FHoudiniGeoPartObject& InHGPO) { HGPO = InHGPO; };
        void SetPackageParams(const FHoudiniPackageParams& InPackageParams, const bool& bUpdateHGPO = false);
        void SetOutputObjects(TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InOutputObjects) { OutputObjects = InOutputObjects; };

        // New Output objects
        TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> OutputObjects;

    protected:

        // The HoudiniGeoPartObject we're working on
        FHoudiniGeoPartObject HGPO;        
        // Structure that handles cooking/baking package creation parameters
        FHoudiniPackageParams PackageParams;
        
        // New Output objects
        //TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> OutputObjects;
        

        USkeletalMesh* CreateNewSkeletalMesh(const FString& InSplitIdentifier);
        USkeleton* CreateNewSkeleton(const FString& InSplitIdentifier);
};