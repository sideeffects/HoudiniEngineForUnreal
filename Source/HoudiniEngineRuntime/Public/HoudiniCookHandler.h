
#pragma once

#include "CoreGlobals.h"
#include "UnrealString.h"

/** Used to control behavior of package baking helper functions */
enum class EBakeMode
{
    Intermediate,
    CreateNewAssets,
    ReplaceExisitingAssets,
    CookToTemp
};

class IHoudiniCookHandler
{
public:
    virtual FString GetBakingBaseName( const struct FHoudiniGeoPartObject& GeoPartObject ) const = 0;
    virtual void SetStaticMeshGenerationParameters( class UStaticMesh* StaticMesh ) const = 0;
    virtual class UMaterialInterface * GetAssignmentMaterial( const FString& MaterialName ) = 0;
    virtual void ClearAssignmentMaterials() = 0;
    virtual void AddAssignmentMaterial( const FString& MaterialName, class UMaterialInterface* MaterialInterface ) = 0;
    virtual class UMaterialInterface * GetReplacementMaterial( const struct FHoudiniGeoPartObject& GeoPartObject, const FString& MaterialName) = 0;
};

struct FHoudiniCookParams
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

    // When cooking in temp mode - folder to create assets in
    FText TempCookFolder;
    // When cooking in bake mode - folder to create assets in
    FText BakeFolder;
    // When cooking in intermediate mode - uobject to use as outer
    class UObject* IntermediateOuter = nullptr;

    int32 GeneratedDistanceFieldResolutionScale = 0;
};
