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

#include "HoudiniGeoPartObject.h"
#include "HoudiniCookHandler.h"

class UHoudiniAssetComponent;
struct FHoudiniCookParams;
class UStaticMesh;
class AActor;
class UStaticMeshComponent;
class UTexture2D;

struct HOUDINIENGINERUNTIME_API FHoudiniEngineBakeUtils
{
public:

    /** Bake static mesh. **/
    static UStaticMesh * BakeStaticMesh(
        UHoudiniAssetComponent * HoudiniAssetComponent,
        const FHoudiniGeoPartObject & HoudiniGeoPartObject,
        UStaticMesh * StaticMesh );

    /** Bake blueprint. **/
    static class UBlueprint * BakeBlueprint( UHoudiniAssetComponent * HoudiniAssetComponent );

    /** Bake blueprint, instantiate and replace Houdini actor. **/
    static AActor * ReplaceHoudiniActorWithBlueprint( UHoudiniAssetComponent * HoudiniAssetComponent );

    /** Create a package for given component for blueprint baking. **/
    static UPackage * BakeCreateBlueprintPackageForComponent(
        UHoudiniAssetComponent * HoudiniAssetComponent, FString & BlueprintName );

    /** Helper for baking to actors */
    static TArray< AActor* > BakeHoudiniActorToActors_StaticMeshes( UHoudiniAssetComponent * HoudiniAssetComponent,
        TMap< const UStaticMeshComponent*, FHoudiniGeoPartObject >& SMComponentToPart );
    /** Helper for baking to actors */
    static TArray< AActor* > BakeHoudiniActorToActors_InstancedActors( UHoudiniAssetComponent * HoudiniAssetComponent,
        TMap< const class UHoudiniInstancedActorComponent*, FHoudiniGeoPartObject >& ComponentToPart );
    static TArray< AActor* > BakeHoudiniActorToActors_SplitMeshInstancers(UHoudiniAssetComponent * HoudiniAssetComponent,
        TMap<const class UHoudiniMeshSplitInstancerComponent *, FHoudiniGeoPartObject> SplitMeshInstancerComponentToPart);
    /** Helper for baking an SM only if necessary */
    static void CheckedBakeStaticMesh(
        class UHoudiniAssetComponent* HoudiniAssetComponent, TMap< const UStaticMesh*, UStaticMesh* >& OriginalToBakedMesh,
        const FHoudiniGeoPartObject & HoudiniGeoPartObject, UStaticMesh* OriginalSM);
    /** Duplicate a given material. This will create a new package for it. This will also create necessary textures **/
    /** and their corresponding packages. **/
    static class UMaterial * DuplicateMaterialAndCreatePackage(
        class UMaterial * Material, FHoudiniCookParams& HoudiniCookParams, const FString & SubMaterialName );

    /** Duplicate a given texture. This will create a new package for it. **/
    static UTexture2D * DuplicateTextureAndCreatePackage(
        UTexture2D * Texture, FHoudiniCookParams& HoudiniCookParams, const FString & SubTextureName );

    /** Replace duplicated texture with a new copy within a given sampling expression. **/
    static void ReplaceDuplicatedMaterialTextureSample(
        class UMaterialExpression * MaterialExpression, FHoudiniCookParams& HoudiniCookParams );

    /** Returns true if the supplied static mesh has unbaked (not backed by a .uasset) mesh or material */
    static bool StaticMeshRequiresBake( const UStaticMesh * StaticMesh );

    /** Duplicate a given static mesh. This will create a new package for it. This will also create necessary       **/
    /** materials and textures and their corresponding packages. **/
    static UStaticMesh * DuplicateStaticMeshAndCreatePackage(
        const UStaticMesh * StaticMesh, UHoudiniAssetComponent * Component,
        const FHoudiniGeoPartObject & HoudiniGeoPartObject, EBakeMode BakeMode );

    /** Bake output meshes and materials to packages and create corresponding actors in the scene */
    static void BakeHoudiniActorToActors( UHoudiniAssetComponent * HoudiniAssetComponent, bool SelectNewActors );

    /** Get a candidate for baking to outliner input workflow */
    static class UHoudiniAssetInput* GetInputForBakeHoudiniActorToOutlinerInput( const UHoudiniAssetComponent * HoudiniAssetComponent );

    /** Returns true if the conditions are met for Bake to Input action ( 1 static mesh output and first input is world outliner with a static mesh) */
    static bool GetCanComponentBakeToOutlinerInput( const UHoudiniAssetComponent * HoudiniAssetComponent );

    /** Bakes output meshes and materials to packages and sets them on an input */
    static void BakeHoudiniActorToOutlinerInput( UHoudiniAssetComponent * HoudiniAssetComponent );

    /** Bakes landscape (detach them from the asset), if OnlyBakeThisLandscape is null, all landscapes will be baked **/
    static bool BakeLandscape( UHoudiniAssetComponent* HoudiniAssetComponent, class ALandscape * OnlyBakeThisLandscape = nullptr );

    /** Create a package for a given component for material. **/
    static UPackage * BakeCreateMaterialPackageForComponent(
        FHoudiniCookParams& HoudiniCookParams,
        const HAPI_MaterialInfo & MaterialInfo, FString & MaterialName );

    /** Create a package for a given component for texture. **/
    static UPackage * BakeCreateTexturePackageForComponent(
        FHoudiniCookParams& HoudiniCookParams,
        const HAPI_MaterialInfo & MaterialInfo, const FString & TextureType,
        FString & TextureName );

    /** Create a package for a given component for either a texture or material **/
    static UPackage * BakeCreateTextureOrMaterialPackageForComponent(
        FHoudiniCookParams& HoudiniCookParams,
        const FString & MaterialInfoDescriptor, FString & MaterialName );

    /** Create a package for given component for static mesh baking. **/
    static UPackage * BakeCreateStaticMeshPackageForComponent(
        FHoudiniCookParams& HoudiniCookParams,
        const FHoudiniGeoPartObject & HoudiniGeoPartObject,
        FString & MeshName, FGuid & BakeGUID );

    /** Checks the package is not referenced or marked for garbage collection **/
    static bool CheckPackageSafeForBake( UPackage* Package, FString& FoundAssetName );
    
    /** Add Houdini meta information to package for a given object. **/
    static void AddHoudiniMetaInformationToPackage(
        UPackage * Package, UObject * Object, const TCHAR * Key, const TCHAR * Value );

    /** Retrieve item name from Houdini meta information. **/
    static bool GetHoudiniGeneratedNameFromMetaInformation(
        UPackage * Package, UObject * Object, FString & HoudiniName );
};