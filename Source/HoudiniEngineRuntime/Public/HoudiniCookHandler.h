/*
* Copyright (c) <2017> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
*/

#pragma once

#include "CoreGlobals.h"
#include "Containers/UnrealString.h"
#include "Misc/Guid.h"

/** Used to control behavior of package baking helper functions */
enum class EBakeMode
{
    Intermediate,
    CreateNewAssets,
    ReplaceExisitingAssets,
    CookToTemp
};

class HOUDINIENGINERUNTIME_API IHoudiniCookHandler
{
public:
    virtual FString GetBakingBaseName( const struct FHoudiniGeoPartObject& GeoPartObject ) const = 0;
    virtual void SetStaticMeshGenerationParameters( class UStaticMesh* StaticMesh ) const = 0;
    virtual class UMaterialInterface * GetAssignmentMaterial( const FString& MaterialName ) = 0;
    virtual void ClearAssignmentMaterials() = 0;
    virtual void AddAssignmentMaterial( const FString& MaterialName, class UMaterialInterface* MaterialInterface ) = 0;
    virtual class UMaterialInterface * GetReplacementMaterial( const struct FHoudiniGeoPartObject& GeoPartObject, const FString& MaterialName) = 0;
};

struct HOUDINIENGINERUNTIME_API FHoudiniCookParams
{
    FHoudiniCookParams(class UHoudiniAsset* InHoudiniAsset);
    FHoudiniCookParams(class UHoudiniAssetComponent* HoudiniAssetComponent);

    // Helper functions returning the default behavior expected when cooking mesh
    static EBakeMode GetDefaultStaticMeshesCookMode() { return EBakeMode::Intermediate; };
    // Helper functions returning the default behavior expected when cooking materials or textures
    static EBakeMode GetDefaultMaterialAndTextureCookMode() { return EBakeMode::CookToTemp; };


    // Members

    // The attached HoudiniAsset
    class UHoudiniAsset* HoudiniAsset = nullptr;

    // Object that handles material and mesh caches as they are built
    IHoudiniCookHandler* HoudiniCookManager = nullptr;
    EBakeMode StaticMeshBakeMode = GetDefaultStaticMeshesCookMode();
    EBakeMode MaterialAndTextureBakeMode = GetDefaultMaterialAndTextureCookMode();

    // GUID used to decorate package names in intermediate mode
    FGuid PackageGUID;

    // Transient cache of last baked parts
    TMap<FHoudiniGeoPartObject, TWeakObjectPtr<class UPackage> >* BakedStaticMeshPackagesForParts = nullptr;
    // Transient cache of last baked materials and textures
    TMap<FString, TWeakObjectPtr<class UPackage> >* BakedMaterialPackagesForIds = nullptr;
    // Cache of the temp cook content packages created by the asset for its materials/textures
    TMap<FHoudiniGeoPartObject, TWeakObjectPtr<class UPackage> >* CookedTemporaryStaticMeshPackages = nullptr;
    // Cache of the temp cook content packages created by the asset for its materials/textures
    TMap<FString, TWeakObjectPtr<class UPackage> >* CookedTemporaryPackages = nullptr;
    /** Cache of the temp cook content packages created by the asset for its Landscape layers		    **/
    /** As packages are unique their are used as the key (we can have multiple package for the same geopartobj  **/
    TMap< TWeakObjectPtr<class UPackage>, FHoudiniGeoPartObject >* CookedTemporaryLandscapeLayers = nullptr;

    // When cooking in temp mode - folder to create assets in
    FText TempCookFolder;
    // When cooking in bake mode - folder to create assets in
    FText BakeFolder;
    // When cooking in intermediate mode - uobject to use as outer
    class UObject* IntermediateOuter = nullptr;

    int32 GeneratedDistanceFieldResolutionScale = 0;
};
