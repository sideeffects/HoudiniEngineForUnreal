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

#include "HoudiniApi.h"
#include "HoudiniEngineMaterialUtils.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniEngineString.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/MetaData.h"
#if WITH_EDITOR
    #include "Materials/Material.h"
    #include "Materials/MaterialInstance.h"
    #include "Materials/MaterialExpressionTextureSample.h"
    #include "Materials/MaterialExpressionTextureCoordinate.h"
    #include "Materials/MaterialExpressionConstant4Vector.h"
    #include "Materials/MaterialExpressionConstant.h"
    #include "Materials/MaterialExpressionMultiply.h"
    #include "Materials/MaterialExpressionVertexColor.h"
    #include "Materials/MaterialExpressionTextureSampleParameter2D.h"
    #include "Materials/MaterialExpressionVectorParameter.h"
    #include "Materials/MaterialExpressionScalarParameter.h"
    #include "Factories/MaterialFactoryNew.h"
    #include "Factories/MaterialInstanceConstantFactoryNew.h"
#endif

const int32
FHoudiniEngineMaterialUtils::MaterialExpressionNodeX = -400;

const int32
FHoudiniEngineMaterialUtils::MaterialExpressionNodeY = -150;

const int32
FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepX = 220;

const int32
FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepY = 220;

void
FHoudiniEngineMaterialUtils::HapiCreateMaterials(
    HAPI_NodeId AssetId,
    FHoudiniCookParams& HoudiniCookParams,
    const HAPI_AssetInfo & AssetInfo,
    const TSet< HAPI_NodeId > & UniqueMaterialIds,
    const TSet< HAPI_NodeId > & UniqueInstancerMaterialIds,
    TMap< FString, UMaterialInterface * > & Materials,
    const bool& bForceRecookAll )
{
#if WITH_EDITOR

    // Empty returned materials.
    Materials.Empty();

    if ( UniqueMaterialIds.Num() == 0 )
        return;

    // Update context for generated materials (will trigger when object goes out of scope).
    FMaterialUpdateContext MaterialUpdateContext;

    // Default Houdini material.
    UMaterial * DefaultMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get();
    Materials.Add( HAPI_UNREAL_DEFAULT_MATERIAL_NAME, DefaultMaterial );

    // Factory to create materials.
    UMaterialFactoryNew * MaterialFactory = NewObject< UMaterialFactoryNew >();
    MaterialFactory->AddToRoot();

    for ( TSet< HAPI_NodeId >::TConstIterator IterMaterialId( UniqueMaterialIds ); IterMaterialId; ++IterMaterialId )
    {
        HAPI_NodeId MaterialId = *IterMaterialId;

        // Get material information.
        HAPI_MaterialInfo MaterialInfo;
        if ( FHoudiniApi::GetMaterialInfo(
            FHoudiniEngine::Get().GetSession(),
            MaterialId, &MaterialInfo ) != HAPI_RESULT_SUCCESS )
        {
            continue;
        }

        // Get node information.
        HAPI_NodeInfo NodeInfo;
        if ( FHoudiniApi::GetNodeInfo(
            FHoudiniEngine::Get().GetSession(), MaterialInfo.nodeId, &NodeInfo ) != HAPI_RESULT_SUCCESS )
        {
            continue;
        }

        if ( MaterialInfo.exists )
        {
            FString MaterialShopName = TEXT( "" );
            if ( !FHoudiniEngineMaterialUtils::GetUniqueMaterialShopName( AssetId, MaterialId, MaterialShopName ) )
                continue;
            
            bool bCreatedNewMaterial = false;
            UMaterial * Material = HoudiniCookParams.HoudiniCookManager ? Cast< UMaterial >( HoudiniCookParams.HoudiniCookManager->GetAssignmentMaterial( MaterialShopName ) ) : nullptr;
            if ( Material && !Material->IsPendingKill() )
            {
                // If cached material exists and has not changed, we can reuse it.
                if ( !MaterialInfo.hasChanged && !bForceRecookAll )
                {
                    // We found cached material, we can reuse it.
                    Materials.Add( MaterialShopName, Material );
                    continue;
                }
            }
            else
            {
                // Material was not found, we need to create it.
                FString MaterialName = TEXT( "" );
                EObjectFlags ObjFlags = ( HoudiniCookParams.MaterialAndTextureBakeMode == EBakeMode::Intermediate ) ? RF_Transactional : RF_Public | RF_Standalone;

                // Create material package and get material name.
                UPackage * MaterialPackage = FHoudiniEngineBakeUtils::BakeCreateMaterialPackageForComponent(
                    HoudiniCookParams, MaterialInfo, MaterialName );

                // Create new material.
                Material = (UMaterial *) MaterialFactory->FactoryCreateNew(
                    UMaterial::StaticClass(), MaterialPackage,
                    *MaterialName, ObjFlags, NULL, GWarn );

                // Add meta information to this package.
                FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
                    MaterialPackage, Material, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT( "true" ) );
                FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
                    MaterialPackage, Material, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MaterialName );

                bCreatedNewMaterial = true;
            }

            if ( !Material || Material->IsPendingKill() )
                continue;

            // If this is an instancer material, enable the instancing flag.
            if ( UniqueInstancerMaterialIds.Contains( MaterialId ) )
                Material->bUsedWithInstancedStaticMeshes = true;

            // Reset material expressions.
            Material->Expressions.Empty();

            // Generate various components for this material.
            bool bMaterialComponentCreated = false;
            int32 MaterialNodeY = FHoudiniEngineMaterialUtils::MaterialExpressionNodeY;

            // By default we mark material as opaque. Some of component creators can change this.
            Material->BlendMode = BLEND_Opaque;

            // Extract diffuse plane.
            bMaterialComponentCreated |= FHoudiniEngineMaterialUtils::CreateMaterialComponentDiffuse(
                HoudiniCookParams, AssetId, Material, MaterialInfo, NodeInfo, MaterialNodeY );

            // Extract opacity plane.
            bMaterialComponentCreated |= FHoudiniEngineMaterialUtils::CreateMaterialComponentOpacity(
                HoudiniCookParams, AssetId, Material, MaterialInfo, NodeInfo, MaterialNodeY );

            // Extract opacity mask plane.
            bMaterialComponentCreated |= FHoudiniEngineMaterialUtils::CreateMaterialComponentOpacityMask(
                HoudiniCookParams, AssetId, Material, MaterialInfo, NodeInfo, MaterialNodeY );

            // Extract normal plane.
            bMaterialComponentCreated |= FHoudiniEngineMaterialUtils::CreateMaterialComponentNormal(
                HoudiniCookParams, AssetId, Material, MaterialInfo, NodeInfo, MaterialNodeY );

            // Extract specular plane.
            bMaterialComponentCreated |= FHoudiniEngineMaterialUtils::CreateMaterialComponentSpecular(
                HoudiniCookParams, AssetId, Material, MaterialInfo, NodeInfo, MaterialNodeY );

            // Extract roughness plane.
            bMaterialComponentCreated |= FHoudiniEngineMaterialUtils::CreateMaterialComponentRoughness(
                HoudiniCookParams, AssetId, Material, MaterialInfo, NodeInfo, MaterialNodeY );

            // Extract metallic plane.
            bMaterialComponentCreated |= FHoudiniEngineMaterialUtils::CreateMaterialComponentMetallic(
                HoudiniCookParams, AssetId, Material, MaterialInfo, NodeInfo, MaterialNodeY );

            // Extract emissive plane.
            bMaterialComponentCreated |= FHoudiniEngineMaterialUtils::CreateMaterialComponentEmissive(
                HoudiniCookParams, AssetId, Material, MaterialInfo, NodeInfo, MaterialNodeY );

            // Set other material properties.
            Material->TwoSided = true;
            Material->SetShadingModel( MSM_DefaultLit );

            // Schedule this material for update.
            MaterialUpdateContext.AddMaterial( Material );

            // Cache material.
            Materials.Add( MaterialShopName, Material );

            // Propagate and trigger material updates.
            if ( bCreatedNewMaterial )
                FAssetRegistryModule::AssetCreated( Material );

            Material->PreEditChange( nullptr );
            Material->PostEditChange();
            Material->MarkPackageDirty();
        }
        else
        {
            // Material does not exist, we will use default Houdini material in this case.
        }
    }

    MaterialFactory->RemoveFromRoot();

#endif
}


bool
FHoudiniEngineMaterialUtils::HapiExtractImage(
    HAPI_ParmId NodeParmId, const HAPI_MaterialInfo & MaterialInfo,
    TArray< char > & ImageBuffer, const char * PlaneType, HAPI_ImageDataFormat ImageDataFormat,
    HAPI_ImagePacking ImagePacking, bool bRenderToImage )
{
    if ( bRenderToImage )
    {
        if ( FHoudiniApi::RenderTextureToImage(
            FHoudiniEngine::Get().GetSession(),
            MaterialInfo.nodeId, NodeParmId ) != HAPI_RESULT_SUCCESS )
        {
            return false;
        }
    }

    HAPI_ImageInfo ImageInfo;
    if ( FHoudiniApi::GetImageInfo(
        FHoudiniEngine::Get().GetSession(),
        MaterialInfo.nodeId, &ImageInfo ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    ImageInfo.dataFormat = ImageDataFormat;
    ImageInfo.interleaved = true;
    ImageInfo.packing = ImagePacking;

    if ( FHoudiniApi::SetImageInfo(
        FHoudiniEngine::Get().GetSession(),
        MaterialInfo.nodeId, &ImageInfo) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    int32 ImageBufferSize = 0;
    if ( FHoudiniApi::ExtractImageToMemory(
        FHoudiniEngine::Get().GetSession(),
        MaterialInfo.nodeId, HAPI_RAW_FORMAT_NAME,
        PlaneType, &ImageBufferSize ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    if ( !ImageBufferSize )
        return false;

    ImageBuffer.SetNumUninitialized( ImageBufferSize );

    if ( FHoudiniApi::GetImageMemoryBuffer(
        FHoudiniEngine::Get().GetSession(),
        MaterialInfo.nodeId, &ImageBuffer[ 0 ],
        ImageBufferSize ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    return true;
}

bool
FHoudiniEngineMaterialUtils::HapiGetImagePlanes(
    HAPI_ParmId NodeParmId, const HAPI_MaterialInfo & MaterialInfo,
    TArray< FString > & ImagePlanes )
{
    ImagePlanes.Empty();
    int32 ImagePlaneCount = 0;

    if ( FHoudiniApi::RenderTextureToImage(
        FHoudiniEngine::Get().GetSession(),
        MaterialInfo.nodeId, NodeParmId ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    if ( FHoudiniApi::GetImagePlaneCount(
        FHoudiniEngine::Get().GetSession(),
        MaterialInfo.nodeId, &ImagePlaneCount ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    if ( !ImagePlaneCount )
        return true;

    TArray< HAPI_StringHandle > ImagePlaneStringHandles;
    ImagePlaneStringHandles.SetNumUninitialized( ImagePlaneCount );

    if ( FHoudiniApi::GetImagePlanes(
        FHoudiniEngine::Get().GetSession(),
        MaterialInfo.nodeId, &ImagePlaneStringHandles[ 0 ], ImagePlaneCount ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    for ( int32 IdxPlane = 0, IdxPlaneMax = ImagePlaneStringHandles.Num(); IdxPlane < IdxPlaneMax; ++IdxPlane )
    {
        FString ValueString = TEXT( "" );
        FHoudiniEngineString FHoudiniEngineString( ImagePlaneStringHandles[ IdxPlane ] );
        FHoudiniEngineString.ToFString( ValueString );
        ImagePlanes.Add( ValueString );
    }

    return true;
}

#if WITH_EDITOR

bool
FHoudiniEngineMaterialUtils::CreateMaterialComponentDiffuse(
    FHoudiniCookParams& HoudiniCookParams, const HAPI_NodeId& AssetId,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo,
    const HAPI_NodeInfo & NodeInfo, int32 & MaterialNodeY )
{
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    EBakeMode BakeMode = HoudiniCookParams.MaterialAndTextureBakeMode;
    EObjectFlags ObjectFlag = ( BakeMode == EBakeMode::CookToTemp ) ? RF_NoFlags : RF_Standalone;

    // Names of generating Houdini parameters.
    FString GeneratingParameterNameDiffuseTexture = TEXT( "" );
    FString GeneratingParameterNameUniformColor = TEXT( "" );
    FString GeneratingParameterNameVertexColor = TEXT( HAPI_UNREAL_ATTRIB_COLOR );

    // Diffuse texture creation parameters.
    FCreateTexture2DParameters CreateTexture2DParameters;
    CreateTexture2DParameters.SourceGuidHash = FGuid();
    CreateTexture2DParameters.bUseAlpha = false;
    CreateTexture2DParameters.CompressionSettings = TC_Default;
    CreateTexture2DParameters.bDeferCompression = true;
    CreateTexture2DParameters.bSRGB = true;

    // Attempt to look up previously created expressions.
    UMaterialExpression * MaterialExpression = Material->BaseColor.Expression;

    // Locate sampling expression.
    UMaterialExpressionTextureSampleParameter2D * ExpressionTextureSample =
        Cast< UMaterialExpressionTextureSampleParameter2D >( FHoudiniEngineMaterialUtils::MaterialLocateExpression(
            MaterialExpression, UMaterialExpressionTextureSampleParameter2D::StaticClass() ) );

    // If texture sampling expression does exist, attempt to look up corresponding texture.
    UTexture2D * TextureDiffuse = nullptr;
    if ( ExpressionTextureSample && !ExpressionTextureSample->IsPendingKill() )
        TextureDiffuse = Cast< UTexture2D >( ExpressionTextureSample->Texture );

    // Locate uniform color expression.
    UMaterialExpressionVectorParameter * ExpressionConstant4Vector =
        Cast< UMaterialExpressionVectorParameter >(FHoudiniEngineMaterialUtils::MaterialLocateExpression(
            MaterialExpression, UMaterialExpressionVectorParameter::StaticClass() ) );

    // If uniform color expression does not exist, create it.
    if ( !ExpressionConstant4Vector || ExpressionConstant4Vector->IsPendingKill() )
    {
        ExpressionConstant4Vector = NewObject< UMaterialExpressionVectorParameter >(
            Material, UMaterialExpressionVectorParameter::StaticClass(), NAME_None, ObjectFlag );
        ExpressionConstant4Vector->DefaultValue = FLinearColor::White;
    }

    // Add expression.
    Material->Expressions.Add( ExpressionConstant4Vector );

    // Locate vertex color expression.
    UMaterialExpressionVertexColor * ExpressionVertexColor =
        Cast< UMaterialExpressionVertexColor >(FHoudiniEngineMaterialUtils::MaterialLocateExpression(
            MaterialExpression, UMaterialExpressionVertexColor::StaticClass() ) );

    // If vertex color expression does not exist, create it.
    if ( !ExpressionVertexColor || ExpressionVertexColor->IsPendingKill() )
    {
        ExpressionVertexColor = NewObject< UMaterialExpressionVertexColor >(
            Material, UMaterialExpressionVertexColor::StaticClass(), NAME_None, ObjectFlag );
        ExpressionVertexColor->Desc = GeneratingParameterNameVertexColor;
    }

    // Add expression.
    Material->Expressions.Add( ExpressionVertexColor );

    // Material should have at least one multiply expression.
    UMaterialExpressionMultiply * MaterialExpressionMultiply = Cast< UMaterialExpressionMultiply >( MaterialExpression );
    if ( !MaterialExpressionMultiply || MaterialExpressionMultiply->IsPendingKill() )
        MaterialExpressionMultiply = NewObject< UMaterialExpressionMultiply >(
            Material, UMaterialExpressionMultiply::StaticClass(), NAME_None, ObjectFlag );

    // Add expression.
    Material->Expressions.Add( MaterialExpressionMultiply );

    // See if primary multiplication has secondary multiplication as A input.
    UMaterialExpressionMultiply * MaterialExpressionMultiplySecondary = nullptr;
    if ( MaterialExpressionMultiply->A.Expression )
        MaterialExpressionMultiplySecondary =
            Cast< UMaterialExpressionMultiply >( MaterialExpressionMultiply->A.Expression );

    // See if a diffuse texture is available.
    HAPI_ParmId ParmDiffuseTextureId =
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_MAP_DIFFUSE_0 );

    if ( ParmDiffuseTextureId >= 0 )
    {
        GeneratingParameterNameDiffuseTexture = TEXT( HAPI_UNREAL_PARAM_MAP_DIFFUSE_0 );
    }
    else
    {
        ParmDiffuseTextureId =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_MAP_DIFFUSE_1 );

        if ( ParmDiffuseTextureId >= 0 )
            GeneratingParameterNameDiffuseTexture = TEXT( HAPI_UNREAL_PARAM_MAP_DIFFUSE_1 );
    }

    // See if uniform color is available.
    HAPI_ParmInfo ParmInfoDiffuseColor;
    HAPI_ParmId ParmDiffuseColorId =
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_COLOR_DIFFUSE_0, ParmInfoDiffuseColor );

    if ( ParmDiffuseColorId >= 0 )
    {
        GeneratingParameterNameUniformColor = TEXT( HAPI_UNREAL_PARAM_COLOR_DIFFUSE_0 );
    }
    else
    {
        ParmDiffuseColorId =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_COLOR_DIFFUSE_1, ParmInfoDiffuseColor );

        if ( ParmDiffuseColorId >= 0 )
            GeneratingParameterNameUniformColor = TEXT( HAPI_UNREAL_PARAM_COLOR_DIFFUSE_1 );
    }

    // If we have diffuse texture parameter.
    if ( ParmDiffuseTextureId >= 0 )
    {
        TArray< char > ImageBuffer;

        // Get image planes of diffuse map.
        TArray< FString > DiffuseImagePlanes;
        bool bFoundImagePlanes = FHoudiniEngineMaterialUtils::HapiGetImagePlanes(
            ParmDiffuseTextureId, MaterialInfo, DiffuseImagePlanes );

        HAPI_ImagePacking ImagePacking = HAPI_IMAGE_PACKING_UNKNOWN;
        const char * PlaneType = "";

        if ( bFoundImagePlanes && DiffuseImagePlanes.Contains( TEXT( HAPI_UNREAL_MATERIAL_TEXTURE_COLOR ) ) )
        {
            if ( DiffuseImagePlanes.Contains( TEXT( HAPI_UNREAL_MATERIAL_TEXTURE_ALPHA ) ) )
            {
                ImagePacking = HAPI_IMAGE_PACKING_RGBA;
                PlaneType = HAPI_UNREAL_MATERIAL_TEXTURE_COLOR_ALPHA;

                // Material does use alpha.
                CreateTexture2DParameters.bUseAlpha = true;
            }
            else
            {
                // We still need to have the Alpha plane, just not the CreateTexture2DParameters
                // alpha option. This is because all texture data from Houdini Engine contains
                // the alpha plane by default.
                ImagePacking = HAPI_IMAGE_PACKING_RGBA;
                PlaneType = HAPI_UNREAL_MATERIAL_TEXTURE_COLOR_ALPHA;
            }
        }
        else
        {
            bFoundImagePlanes = false;
        }

        // Retrieve color plane.
        if ( bFoundImagePlanes && FHoudiniEngineMaterialUtils::HapiExtractImage(
            ParmDiffuseTextureId, MaterialInfo, ImageBuffer, PlaneType,
            HAPI_IMAGE_DATA_INT8, ImagePacking, false ) )
        {
            UPackage * TextureDiffusePackage = nullptr;
            if ( TextureDiffuse && !TextureDiffuse->IsPendingKill() )
                TextureDiffusePackage = Cast< UPackage >( TextureDiffuse->GetOuter() );

            HAPI_ImageInfo ImageInfo;
            Result = FHoudiniApi::GetImageInfo(
                FHoudiniEngine::Get().GetSession(),
                MaterialInfo.nodeId, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureDiffuseName;
                bool bCreatedNewTextureDiffuse = false;

                // Create diffuse texture package, if this is a new diffuse texture.
                if ( !TextureDiffusePackage )
                {
                    TextureDiffusePackage = FHoudiniEngineBakeUtils::BakeCreateTexturePackageForComponent(
                        HoudiniCookParams,
                        MaterialInfo,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_DIFFUSE,
                        TextureDiffuseName );
                }

                // Create diffuse texture, if we need to create one.
                if ( !TextureDiffuse || TextureDiffuse->IsPendingKill() )
                    bCreatedNewTextureDiffuse = true;

                // Get the node path to add it to the meta data
                FString NodePath;
                GetUniqueMaterialShopName( AssetId, MaterialInfo.nodeId, NodePath );

                // Reuse existing diffuse texture, or create new one.
                TextureDiffuse = FHoudiniEngineMaterialUtils::CreateUnrealTexture(
                    TextureDiffuse, ImageInfo,
                    TextureDiffusePackage, TextureDiffuseName, ImageBuffer,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_DIFFUSE,
                    CreateTexture2DParameters, TEXTUREGROUP_World, NodePath );

                if ( BakeMode == EBakeMode::CookToTemp )
                    TextureDiffuse->SetFlags( RF_Public | RF_Standalone );

                // Create diffuse sampling expression, if needed.
                if ( !ExpressionTextureSample )
                {
                    ExpressionTextureSample = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag );
                }

                // Record generating parameter.
                ExpressionTextureSample->Desc = GeneratingParameterNameDiffuseTexture;
                ExpressionTextureSample->ParameterName = *GeneratingParameterNameDiffuseTexture;
                ExpressionTextureSample->Texture = TextureDiffuse;
                ExpressionTextureSample->SamplerType = SAMPLERTYPE_Color;

                // Add expression.
                Material->Expressions.Add( ExpressionTextureSample );

                // Propagate and trigger diffuse texture updates.
                if ( bCreatedNewTextureDiffuse )
                    FAssetRegistryModule::AssetCreated( TextureDiffuse );

                TextureDiffuse->PreEditChange( nullptr );
                TextureDiffuse->PostEditChange();
                TextureDiffuse->MarkPackageDirty();
            }
        }
    }

    // If we have uniform color parameter.
    if ( ParmDiffuseColorId >= 0 )
    {
        FLinearColor Color = FLinearColor::White;

        if ( FHoudiniApi::GetParmFloatValues(
            FHoudiniEngine::Get().GetSession(), NodeInfo.id, (float *) &Color.R,
            ParmInfoDiffuseColor.floatValuesIndex, ParmInfoDiffuseColor.size ) == HAPI_RESULT_SUCCESS )
        {
            if ( ParmInfoDiffuseColor.size == 3 )
                Color.A = 1.0f;

            // Record generating parameter.
            ExpressionConstant4Vector->Desc = GeneratingParameterNameUniformColor;
            ExpressionConstant4Vector->ParameterName = *GeneratingParameterNameUniformColor;
            ExpressionConstant4Vector->DefaultValue = Color;
        }
    }

    // If we have have texture sample expression present, we need a secondary multiplication expression.
    if ( ExpressionTextureSample )
    {
        if ( !MaterialExpressionMultiplySecondary )
        {
            MaterialExpressionMultiplySecondary = NewObject< UMaterialExpressionMultiply >(
                Material, UMaterialExpressionMultiply::StaticClass(), NAME_None, ObjectFlag );

            // Add expression.
            Material->Expressions.Add( MaterialExpressionMultiplySecondary );
        }
    }
    else
    {
        // If secondary multiplication exists, but we have no sampling, we can free it.
        if ( MaterialExpressionMultiplySecondary )
        {
            MaterialExpressionMultiplySecondary->A.Expression = nullptr;
            MaterialExpressionMultiplySecondary->B.Expression = nullptr;
            MaterialExpressionMultiplySecondary->ConditionalBeginDestroy();
        }
    }

    float SecondaryExpressionScale = 1.0f;
    if ( MaterialExpressionMultiplySecondary )
        SecondaryExpressionScale = 1.5f;

    // Create multiplication expression which has uniform color and vertex color.
    MaterialExpressionMultiply->A.Expression = ExpressionConstant4Vector;
    MaterialExpressionMultiply->B.Expression = ExpressionVertexColor;

    ExpressionConstant4Vector->MaterialExpressionEditorX =
        FHoudiniEngineMaterialUtils::MaterialExpressionNodeX -
        FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepX * SecondaryExpressionScale;
    ExpressionConstant4Vector->MaterialExpressionEditorY = MaterialNodeY;
    MaterialNodeY += FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepY;

    ExpressionVertexColor->MaterialExpressionEditorX =
        FHoudiniEngineMaterialUtils::MaterialExpressionNodeX -
        FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepX * SecondaryExpressionScale;
    ExpressionVertexColor->MaterialExpressionEditorY = MaterialNodeY;
    MaterialNodeY += FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepY;

    MaterialExpressionMultiply->MaterialExpressionEditorX = FHoudiniEngineMaterialUtils::MaterialExpressionNodeX;
    MaterialExpressionMultiply->MaterialExpressionEditorY =
        ( ExpressionVertexColor->MaterialExpressionEditorY - ExpressionConstant4Vector->MaterialExpressionEditorY ) / 2;

    // Hook up secondary multiplication expression to first one.
    if ( MaterialExpressionMultiplySecondary )
    {
        MaterialExpressionMultiplySecondary->A.Expression = MaterialExpressionMultiply;
        MaterialExpressionMultiplySecondary->B.Expression = ExpressionTextureSample;

        ExpressionTextureSample->MaterialExpressionEditorX =
            FHoudiniEngineMaterialUtils::MaterialExpressionNodeX -
            FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepX * SecondaryExpressionScale;
        ExpressionTextureSample->MaterialExpressionEditorY = MaterialNodeY;
        MaterialNodeY += FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepY;

        MaterialExpressionMultiplySecondary->MaterialExpressionEditorX = FHoudiniEngineMaterialUtils::MaterialExpressionNodeX;
        MaterialExpressionMultiplySecondary->MaterialExpressionEditorY =
            MaterialExpressionMultiply->MaterialExpressionEditorY + FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepY;

        // Assign expression.
        Material->BaseColor.Expression = MaterialExpressionMultiplySecondary;
    }
    else
    {
        // Assign expression.
        Material->BaseColor.Expression = MaterialExpressionMultiply;

        MaterialExpressionMultiply->MaterialExpressionEditorX = FHoudiniEngineMaterialUtils::MaterialExpressionNodeX;
        MaterialExpressionMultiply->MaterialExpressionEditorY =
            ( ExpressionVertexColor->MaterialExpressionEditorY -
                ExpressionConstant4Vector->MaterialExpressionEditorY ) / 2;
    }

    return true;
}

bool
FHoudiniEngineMaterialUtils::CreateMaterialComponentOpacityMask(
    FHoudiniCookParams& HoudiniCookParams, const HAPI_NodeId& AssetId,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo,
    const HAPI_NodeInfo & NodeInfo, int32 & MaterialNodeY )
{
    bool bExpressionCreated = false;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    // Name of generating Houdini parameters.
    FString GeneratingParameterNameTexture = TEXT( "" );

    UMaterialExpression * MaterialExpression = Material->OpacityMask.Expression;

    EBakeMode BakeMode = HoudiniCookParams.MaterialAndTextureBakeMode;
    EObjectFlags ObjectFlag = ( BakeMode == EBakeMode::CookToTemp ) ? RF_NoFlags : RF_Standalone;

    // Opacity expressions.
    UMaterialExpressionTextureSampleParameter2D * ExpressionTextureOpacitySample = nullptr;
    UTexture2D * TextureOpacity = nullptr;

    // Opacity texture creation parameters.
    FCreateTexture2DParameters CreateTexture2DParameters;
    CreateTexture2DParameters.SourceGuidHash = FGuid();
    CreateTexture2DParameters.bUseAlpha = false;
    CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
    CreateTexture2DParameters.bDeferCompression = true;
    CreateTexture2DParameters.bSRGB = true;

    // See if opacity texture is available.
    HAPI_ParmId ParmOpacityTextureId =
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_MAP_OPACITY_0 );

    if ( ParmOpacityTextureId >= 0 )
    {
        GeneratingParameterNameTexture = TEXT( HAPI_UNREAL_PARAM_MAP_OPACITY_0 );
    }
    else
    {
        ParmOpacityTextureId =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_MAP_OPACITY_1 );

        if ( ParmOpacityTextureId >= 0 )
            GeneratingParameterNameTexture = TEXT( HAPI_UNREAL_PARAM_MAP_OPACITY_1 );
    }

    // If we have opacity texture parameter.
    if ( ParmOpacityTextureId >= 0 )
    {
        TArray< char > ImageBuffer;

        // Get image planes of opacity map.
        TArray< FString > OpacityImagePlanes;
        bool bFoundImagePlanes = FHoudiniEngineMaterialUtils::HapiGetImagePlanes(
            ParmOpacityTextureId, MaterialInfo, OpacityImagePlanes );

        HAPI_ImagePacking ImagePacking = HAPI_IMAGE_PACKING_UNKNOWN;
        const char * PlaneType = "";

        bool bColorAlphaFound = ( OpacityImagePlanes.Contains( TEXT( HAPI_UNREAL_MATERIAL_TEXTURE_ALPHA ) ) && OpacityImagePlanes.Contains( TEXT( HAPI_UNREAL_MATERIAL_TEXTURE_COLOR ) ) );

        if ( bFoundImagePlanes && bColorAlphaFound )
        {
            ImagePacking = HAPI_IMAGE_PACKING_RGBA;
            PlaneType = HAPI_UNREAL_MATERIAL_TEXTURE_COLOR_ALPHA;
            CreateTexture2DParameters.bUseAlpha = true;
        }
        else
        {
            bFoundImagePlanes = false;
        }

        if ( bFoundImagePlanes && FHoudiniEngineMaterialUtils::HapiExtractImage(
            ParmOpacityTextureId, MaterialInfo, ImageBuffer, PlaneType,
            HAPI_IMAGE_DATA_INT8, ImagePacking, false ) )
        {
            // Locate sampling expression.
            ExpressionTextureOpacitySample = Cast< UMaterialExpressionTextureSampleParameter2D >(
                FHoudiniEngineMaterialUtils::MaterialLocateExpression(
                    MaterialExpression, UMaterialExpressionTextureSampleParameter2D::StaticClass() ) );

            // Locate opacity texture, if valid.
            if ( ExpressionTextureOpacitySample )
                TextureOpacity = Cast< UTexture2D >( ExpressionTextureOpacitySample->Texture );

            UPackage * TextureOpacityPackage = nullptr;
            if ( TextureOpacity )
                TextureOpacityPackage = Cast< UPackage >( TextureOpacity->GetOuter() );

            HAPI_ImageInfo ImageInfo;
            Result = FHoudiniApi::GetImageInfo(
                FHoudiniEngine::Get().GetSession(),
                MaterialInfo.nodeId, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureOpacityName;
                bool bCreatedNewTextureOpacity = false;

                // Create opacity texture package, if this is a new opacity texture.
                if ( !TextureOpacityPackage )
                {
                    TextureOpacityPackage = FHoudiniEngineBakeUtils::BakeCreateTexturePackageForComponent(
                        HoudiniCookParams,
                        MaterialInfo,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_OPACITY_MASK,
                        TextureOpacityName );
                }

                // Create opacity texture, if we need to create one.
                if ( !TextureOpacity )
                    bCreatedNewTextureOpacity = true;

                // Get the node path to add it to the meta data
                FString NodePath;
                GetUniqueMaterialShopName( AssetId, MaterialInfo.nodeId, NodePath );

                // Reuse existing opacity texture, or create new one.
                TextureOpacity = FHoudiniEngineMaterialUtils::CreateUnrealTexture(
                    TextureOpacity, ImageInfo,
                    TextureOpacityPackage, TextureOpacityName, ImageBuffer,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_OPACITY_MASK,
                    CreateTexture2DParameters,
                    TEXTUREGROUP_World, NodePath );

                if ( BakeMode == EBakeMode::CookToTemp )
                    TextureOpacity->SetFlags(RF_Public | RF_Standalone);

                // Create opacity sampling expression, if needed.
                if ( !ExpressionTextureOpacitySample )
                {
                    ExpressionTextureOpacitySample = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag );
                }

                // Record generating parameter.
                ExpressionTextureOpacitySample->Desc = GeneratingParameterNameTexture;
                ExpressionTextureOpacitySample->ParameterName = *GeneratingParameterNameTexture;
                ExpressionTextureOpacitySample->Texture = TextureOpacity;
                ExpressionTextureOpacitySample->SamplerType = SAMPLERTYPE_Grayscale;

                // Offset node placement.
                ExpressionTextureOpacitySample->MaterialExpressionEditorX =
                    FHoudiniEngineMaterialUtils::MaterialExpressionNodeX;
                ExpressionTextureOpacitySample->MaterialExpressionEditorY = MaterialNodeY;
                MaterialNodeY += FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepY;

                // Add expression.
                Material->Expressions.Add( ExpressionTextureOpacitySample );

                // We need to set material type to masked.
                TArray< FExpressionOutput > ExpressionOutputs = ExpressionTextureOpacitySample->GetOutputs();
                FExpressionOutput* ExpressionOutput = ExpressionOutputs.GetData();

                Material->OpacityMask.Expression = ExpressionTextureOpacitySample;
                Material->BlendMode = BLEND_Masked;

                Material->OpacityMask.Mask = ExpressionOutput->Mask;
                Material->OpacityMask.MaskR = 1;
                Material->OpacityMask.MaskG = 0;
                Material->OpacityMask.MaskB = 0;
                Material->OpacityMask.MaskA = 0;

                // Propagate and trigger opacity texture updates.
                if ( bCreatedNewTextureOpacity )
                    FAssetRegistryModule::AssetCreated( TextureOpacity );

                TextureOpacity->PreEditChange( nullptr );
                TextureOpacity->PostEditChange();
                TextureOpacity->MarkPackageDirty();

                bExpressionCreated = true;
            }
        }
    }

    return bExpressionCreated;
}

bool
FHoudiniEngineMaterialUtils::CreateMaterialComponentOpacity(
    FHoudiniCookParams& HoudiniCookParams, const HAPI_NodeId& AssetId,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo,
    const HAPI_NodeInfo & NodeInfo, int32 & MaterialNodeY )
{
    bool bExpressionCreated = false;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;
    float OpacityValue = 1.0f;
    bool bNeedsTranslucency = false;

    EBakeMode BakeMode = HoudiniCookParams.MaterialAndTextureBakeMode;
    EObjectFlags ObjectFlag = ( BakeMode == EBakeMode::CookToTemp ) ? RF_NoFlags : RF_Standalone;

    // Name of generating Houdini parameters.
    FString GeneratingParameterNameScalar = TEXT( "" );
    FString GeneratingParameterNameTexture = TEXT( "" );

    UMaterialExpression * MaterialExpression = Material->Opacity.Expression;

    // Opacity expressions.
    UMaterialExpressionTextureSampleParameter2D * ExpressionTextureOpacitySample = nullptr;
    UMaterialExpressionScalarParameter * ExpressionScalarOpacity = nullptr;
    UTexture2D * TextureOpacity = nullptr;

    // Opacity texture creation parameters.
    FCreateTexture2DParameters CreateTexture2DParameters;
    CreateTexture2DParameters.SourceGuidHash = FGuid();
    CreateTexture2DParameters.bUseAlpha = false;
    CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
    CreateTexture2DParameters.bDeferCompression = true;
    CreateTexture2DParameters.bSRGB = true;

    // If opacity sampling expression was not created, check if diffuse contains an alpha plane.
    if ( !ExpressionTextureOpacitySample )
    {
        UMaterialExpression * MaterialExpressionDiffuse = Material->BaseColor.Expression;
        if ( MaterialExpressionDiffuse )
        {
            // Locate diffuse sampling expression.
            UMaterialExpressionTextureSampleParameter2D * ExpressionTextureDiffuseSample =
                Cast< UMaterialExpressionTextureSampleParameter2D >(
                    FHoudiniEngineMaterialUtils::MaterialLocateExpression(
                        MaterialExpressionDiffuse,
                        UMaterialExpressionTextureSampleParameter2D::StaticClass() ) );

            // See if there's an alpha plane in this expression's texture.
            if ( ExpressionTextureDiffuseSample )
            {
                UTexture2D * DiffuseTexture = Cast< UTexture2D >( ExpressionTextureDiffuseSample->Texture );
                if ( DiffuseTexture && !DiffuseTexture->CompressionNoAlpha )
                {
                    // The diffuse texture has an alpha channel (that wasn't discarded), so we can use it
                    ExpressionTextureOpacitySample = ExpressionTextureDiffuseSample;
                    bNeedsTranslucency = true;
                }
            }
        }
    }

    // Retrieve opacity uniform parameter.
    HAPI_ParmInfo ParmInfoOpacityValue;
    HAPI_ParmId ParmOpacityValueId =
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_ALPHA_0, ParmInfoOpacityValue );

    if ( ParmOpacityValueId >= 0 )
    {
        GeneratingParameterNameScalar = TEXT( HAPI_UNREAL_PARAM_ALPHA_0 );
    }
    else
    {
        ParmOpacityValueId =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_ALPHA_1, ParmInfoOpacityValue );

        if ( ParmOpacityValueId >= 0 )
            GeneratingParameterNameScalar = TEXT( HAPI_UNREAL_PARAM_ALPHA_1 );
    }

    if ( ParmOpacityValueId >= 0 )
    {
        if (ParmInfoOpacityValue.size > 0 && ParmInfoOpacityValue.floatValuesIndex >= 0 )
        {
            float OpacityValueRetrieved = 1.0f;
            if ( FHoudiniApi::GetParmFloatValues(
                FHoudiniEngine::Get().GetSession(), NodeInfo.id,
                (float *) &OpacityValue, ParmInfoOpacityValue.floatValuesIndex, 1 ) == HAPI_RESULT_SUCCESS )
            {
                if ( !ExpressionScalarOpacity )
                {
                    ExpressionScalarOpacity = NewObject< UMaterialExpressionScalarParameter >(
                        Material, UMaterialExpressionScalarParameter::StaticClass(), NAME_None, ObjectFlag );
                }

                // Clamp retrieved value.
                OpacityValueRetrieved = FMath::Clamp< float >( OpacityValueRetrieved, 0.0f, 1.0f );
                OpacityValue = OpacityValueRetrieved;

                // Set expression fields.
                ExpressionScalarOpacity->DefaultValue = OpacityValue;
                ExpressionScalarOpacity->SliderMin = 0.0f;
                ExpressionScalarOpacity->SliderMax = 1.0f;
                ExpressionScalarOpacity->Desc = GeneratingParameterNameScalar;
                ExpressionScalarOpacity->ParameterName = *GeneratingParameterNameScalar;

                // Add expression.
                Material->Expressions.Add( ExpressionScalarOpacity );

                // If alpha is less than 1, we need translucency.
                bNeedsTranslucency |= ( OpacityValue != 1.0f );
            }
        }
    }

    if ( bNeedsTranslucency )
        Material->BlendMode = BLEND_Translucent;

    if ( ExpressionScalarOpacity && ExpressionTextureOpacitySample )
    {
        // We have both alpha and alpha uniform, attempt to locate multiply expression.
        UMaterialExpressionMultiply * ExpressionMultiply =
            Cast< UMaterialExpressionMultiply >(
                FHoudiniEngineMaterialUtils::MaterialLocateExpression(
                    MaterialExpression,
                    UMaterialExpressionMultiply::StaticClass() ) );

        if ( !ExpressionMultiply )
            ExpressionMultiply = NewObject< UMaterialExpressionMultiply >(
                Material, UMaterialExpressionMultiply::StaticClass(), NAME_None, ObjectFlag );

        Material->Expressions.Add( ExpressionMultiply );

        TArray< FExpressionOutput > ExpressionOutputs = ExpressionTextureOpacitySample->GetOutputs();
        FExpressionOutput * ExpressionOutput = ExpressionOutputs.GetData();

        ExpressionMultiply->A.Expression = ExpressionTextureOpacitySample;
        ExpressionMultiply->B.Expression = ExpressionScalarOpacity;

        Material->Opacity.Expression = ExpressionMultiply;
        Material->Opacity.Mask = ExpressionOutput->Mask;
        Material->Opacity.MaskR = 0;
        Material->Opacity.MaskG = 0;
        Material->Opacity.MaskB = 0;
        Material->Opacity.MaskA = 1;

        ExpressionMultiply->MaterialExpressionEditorX = FHoudiniEngineMaterialUtils::MaterialExpressionNodeX;
        ExpressionMultiply->MaterialExpressionEditorY = MaterialNodeY;

        ExpressionScalarOpacity->MaterialExpressionEditorX =
            FHoudiniEngineMaterialUtils::MaterialExpressionNodeX - FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepX;
        ExpressionScalarOpacity->MaterialExpressionEditorY = MaterialNodeY;
        MaterialNodeY += FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepY;

        bExpressionCreated = true;
    }
    else if ( ExpressionScalarOpacity )
    {
        Material->Opacity.Expression = ExpressionScalarOpacity;

        ExpressionScalarOpacity->MaterialExpressionEditorX = FHoudiniEngineMaterialUtils::MaterialExpressionNodeX;
        ExpressionScalarOpacity->MaterialExpressionEditorY = MaterialNodeY;
        MaterialNodeY += FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepY;

        bExpressionCreated = true;
    }
    else if ( ExpressionTextureOpacitySample )
    {
        TArray<FExpressionOutput> ExpressionOutputs = ExpressionTextureOpacitySample->GetOutputs();
        FExpressionOutput * ExpressionOutput = ExpressionOutputs.GetData();

        Material->Opacity.Expression = ExpressionTextureOpacitySample;
        Material->Opacity.Mask = ExpressionOutput->Mask;
        Material->Opacity.MaskR = 0;
        Material->Opacity.MaskG = 0;
        Material->Opacity.MaskB = 0;
        Material->Opacity.MaskA = 1;

        bExpressionCreated = true;
    }

    return bExpressionCreated;
}

bool
FHoudiniEngineMaterialUtils::CreateMaterialComponentNormal(
    FHoudiniCookParams& HoudiniCookParams, const HAPI_NodeId& AssetId,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo,
    const HAPI_NodeInfo & NodeInfo, int32 & MaterialNodeY )
{
    bool bExpressionCreated = false;
    bool bTangentSpaceNormal = true;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    EBakeMode BakeMode = HoudiniCookParams.MaterialAndTextureBakeMode;
    EObjectFlags ObjectFlag = ( BakeMode == EBakeMode::CookToTemp ) ? RF_NoFlags : RF_Standalone;

    // Name of generating Houdini parameter.
    FString GeneratingParameterName = TEXT( "" );

    // Normal texture creation parameters.
    FCreateTexture2DParameters CreateTexture2DParameters;
    CreateTexture2DParameters.SourceGuidHash = FGuid();
    CreateTexture2DParameters.bUseAlpha = false;
    CreateTexture2DParameters.CompressionSettings = TC_Normalmap;
    CreateTexture2DParameters.bDeferCompression = true;
    CreateTexture2DParameters.bSRGB = false;

    // See if separate normal texture is available.
    HAPI_ParmId ParmNameNormalId =
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_MAP_NORMAL_0 );

    if ( ParmNameNormalId >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_NORMAL_0 );
    }
    else
    {
        ParmNameNormalId =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_MAP_NORMAL_1 );

        if ( ParmNameNormalId >= 0 )
            GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_NORMAL_1 );
    }

    if ( ParmNameNormalId >= 0 )
    {
        // Retrieve space for this normal texture.
        HAPI_ParmInfo ParmInfoNormalType;
        int32 ParmNormalTypeId =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_MAP_NORMAL_TYPE, ParmInfoNormalType );

        // Retrieve value for normal type choice list (if exists).

        if ( ParmNormalTypeId >= 0 )
        {
            FString NormalType = TEXT( HAPI_UNREAL_PARAM_MAP_NORMAL_TYPE_TANGENT );

            if ( ParmInfoNormalType.size > 0 && ParmInfoNormalType.stringValuesIndex >= 0 )
            {
                HAPI_StringHandle StringHandle;
                if ( FHoudiniApi::GetParmStringValues(
                    FHoudiniEngine::Get().GetSession(),
                    NodeInfo.id, false, &StringHandle, ParmInfoNormalType.stringValuesIndex, ParmInfoNormalType.size ) == HAPI_RESULT_SUCCESS )
                {
                    // Get the actual string value.
                    FString NormalTypeString = TEXT( "" );
                    FHoudiniEngineString HoudiniEngineString( StringHandle );
                    if ( HoudiniEngineString.ToFString( NormalTypeString ) )
                        NormalType = NormalTypeString;
                }
            }

            // Check if we require world space normals.
            if ( NormalType.Equals( TEXT( HAPI_UNREAL_PARAM_MAP_NORMAL_TYPE_WORLD ), ESearchCase::IgnoreCase ) )
                bTangentSpaceNormal = false;
        }

        TArray< char > ImageBuffer;

        // Retrieve color plane.
        if (FHoudiniEngineMaterialUtils::HapiExtractImage(
            ParmNameNormalId, MaterialInfo, ImageBuffer,
            HAPI_UNREAL_MATERIAL_TEXTURE_COLOR, HAPI_IMAGE_DATA_INT8, HAPI_IMAGE_PACKING_RGBA, true ) )
        {
            UMaterialExpressionTextureSampleParameter2D * ExpressionNormal =
                Cast< UMaterialExpressionTextureSampleParameter2D >( Material->Normal.Expression );

            UTexture2D * TextureNormal = nullptr;
            if ( ExpressionNormal )
            {
                TextureNormal = Cast< UTexture2D >( ExpressionNormal->Texture );
            }
            else
            {
                // Otherwise new expression is of a different type.
                if ( Material->Normal.Expression )
                {
                    Material->Normal.Expression->ConditionalBeginDestroy();
                    Material->Normal.Expression = nullptr;
                }
            }

            UPackage * TextureNormalPackage = nullptr;
            if ( TextureNormal )
                TextureNormalPackage = Cast< UPackage >( TextureNormal->GetOuter() );

            HAPI_ImageInfo ImageInfo;
            Result = FHoudiniApi::GetImageInfo(
                FHoudiniEngine::Get().GetSession(),
                MaterialInfo.nodeId, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureNormalName;
                bool bCreatedNewTextureNormal = false;

                // Create normal texture package, if this is a new normal texture.
                if ( !TextureNormalPackage )
                {
                    TextureNormalPackage =
                        FHoudiniEngineBakeUtils::BakeCreateTexturePackageForComponent(
                            HoudiniCookParams,
                            MaterialInfo,
                            HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL,
                            TextureNormalName );
                }

                // Create normal texture, if we need to create one.
                if ( !TextureNormal )
                    bCreatedNewTextureNormal = true;

                // Get the node path to add it to the meta data
                FString NodePath;
                GetUniqueMaterialShopName( AssetId, MaterialInfo.nodeId, NodePath );

                // Reuse existing normal texture, or create new one.
                TextureNormal = FHoudiniEngineMaterialUtils::CreateUnrealTexture(
                    TextureNormal, ImageInfo,
                    TextureNormalPackage, TextureNormalName, ImageBuffer,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL,
                    CreateTexture2DParameters,
                    TEXTUREGROUP_WorldNormalMap,
                    NodePath );

                if ( BakeMode == EBakeMode::CookToTemp )
                    TextureNormal->SetFlags(RF_Public | RF_Standalone);

                // Create normal sampling expression, if needed.
                if ( !ExpressionNormal )
                    ExpressionNormal = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag );

                // Record generating parameter.
                ExpressionNormal->Desc = GeneratingParameterName;
                ExpressionNormal->ParameterName = *GeneratingParameterName;

                ExpressionNormal->Texture = TextureNormal;
                ExpressionNormal->SamplerType = SAMPLERTYPE_Normal;

                // Offset node placement.
                ExpressionNormal->MaterialExpressionEditorX = FHoudiniEngineMaterialUtils::MaterialExpressionNodeX;
                ExpressionNormal->MaterialExpressionEditorY = MaterialNodeY;
                MaterialNodeY += FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepY;

                // Set normal space.
                Material->bTangentSpaceNormal = bTangentSpaceNormal;

                // Assign expression to material.
                Material->Expressions.Add(ExpressionNormal);
                Material->Normal.Expression = ExpressionNormal;

                bExpressionCreated = true;

                // Propagate and trigger normal texture updates.
                if (bCreatedNewTextureNormal)
                    FAssetRegistryModule::AssetCreated(TextureNormal);

                TextureNormal->PreEditChange(nullptr);
                TextureNormal->PostEditChange();
                TextureNormal->MarkPackageDirty();
            }
        }
    }

    // If separate normal map was not found, see if normal plane exists in diffuse map.
    if ( !bExpressionCreated )
    {
        // See if diffuse texture is available.
        HAPI_ParmId ParmNameBaseId =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_MAP_DIFFUSE_0 );

        if ( ParmNameBaseId >= 0 )
        {
            GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_DIFFUSE_0 );
        }
        else
        {
            ParmNameBaseId =
                FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_MAP_DIFFUSE_1 );

            if ( ParmNameBaseId >= 0 )
                GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_DIFFUSE_1 );
        }

        if ( ParmNameBaseId >= 0 )
        {
            // Normal plane is available in diffuse map.

            TArray< char > ImageBuffer;

            // Retrieve color plane - this will contain normal data.
            if ( FHoudiniEngineMaterialUtils::HapiExtractImage(
                ParmNameBaseId, MaterialInfo, ImageBuffer,
                HAPI_UNREAL_MATERIAL_TEXTURE_NORMAL, HAPI_IMAGE_DATA_INT8, HAPI_IMAGE_PACKING_RGB, true ) )
            {
                UMaterialExpressionTextureSampleParameter2D * ExpressionNormal =
                    Cast< UMaterialExpressionTextureSampleParameter2D >( Material->Normal.Expression );

                UTexture2D * TextureNormal = nullptr;
                if ( ExpressionNormal )
                {
                    TextureNormal = Cast< UTexture2D >( ExpressionNormal->Texture );
                }
                else
                {
                    // Otherwise new expression is of a different type.
                    if ( Material->Normal.Expression )
                    {
                        Material->Normal.Expression->ConditionalBeginDestroy();
                        Material->Normal.Expression = nullptr;
                    }
                }

                UPackage * TextureNormalPackage = nullptr;
                if ( TextureNormal )
                    TextureNormalPackage = Cast< UPackage >( TextureNormal->GetOuter() );

                HAPI_ImageInfo ImageInfo;
                Result = FHoudiniApi::GetImageInfo(
                    FHoudiniEngine::Get().GetSession(),
                    MaterialInfo.nodeId, &ImageInfo );

                if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
                {
                    // Create texture.
                    FString TextureNormalName;
                    bool bCreatedNewTextureNormal = false;

                    // Create normal texture package, if this is a new normal texture.
                    if ( !TextureNormalPackage )
                    {
                        TextureNormalPackage = FHoudiniEngineBakeUtils::BakeCreateTexturePackageForComponent(
                            HoudiniCookParams,
                            MaterialInfo,
                            HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL,
                            TextureNormalName );
                    }

                    // Create normal texture, if we need to create one.
                    if ( !TextureNormal )
                        bCreatedNewTextureNormal = true;

                    // Get the node path to add it to the meta data
                    FString NodePath;
                    GetUniqueMaterialShopName( AssetId, MaterialInfo.nodeId, NodePath );

                    // Reuse existing normal texture, or create new one.
                    TextureNormal = FHoudiniEngineMaterialUtils::CreateUnrealTexture(
                        TextureNormal, ImageInfo,
                        TextureNormalPackage, TextureNormalName, ImageBuffer,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL, CreateTexture2DParameters,
                        TEXTUREGROUP_WorldNormalMap, NodePath );

                    if ( BakeMode == EBakeMode::CookToTemp )
                        TextureNormal->SetFlags( RF_Public | RF_Standalone );

                    // Create normal sampling expression, if needed.
                    if ( !ExpressionNormal )
                        ExpressionNormal = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                            Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag );

                    // Record generating parameter.
                    ExpressionNormal->Desc = GeneratingParameterName;
                    ExpressionNormal->ParameterName = *GeneratingParameterName;

                    ExpressionNormal->Texture = TextureNormal;
                    ExpressionNormal->SamplerType = SAMPLERTYPE_Normal;

                    // Offset node placement.
                    ExpressionNormal->MaterialExpressionEditorX = FHoudiniEngineMaterialUtils::MaterialExpressionNodeX;
                    ExpressionNormal->MaterialExpressionEditorY = MaterialNodeY;
                    MaterialNodeY += FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepY;

                    // Set normal space.
                    Material->bTangentSpaceNormal = bTangentSpaceNormal;

                    // Assign expression to material.
                    Material->Expressions.Add( ExpressionNormal );
                    Material->Normal.Expression = ExpressionNormal;

                    // Propagate and trigger diffuse texture updates.
                    if ( bCreatedNewTextureNormal )
                        FAssetRegistryModule::AssetCreated( TextureNormal );

                    TextureNormal->PreEditChange( nullptr );
                    TextureNormal->PostEditChange();
                    TextureNormal->MarkPackageDirty();

                    bExpressionCreated = true;
                }
            }
        }
    }

    return bExpressionCreated;
}

bool
FHoudiniEngineMaterialUtils::CreateMaterialComponentSpecular(
    FHoudiniCookParams& HoudiniCookParams, const HAPI_NodeId& AssetId,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo,
    const HAPI_NodeInfo & NodeInfo, int32 & MaterialNodeY )
{
    bool bExpressionCreated = false;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    EBakeMode BakeMode = HoudiniCookParams.MaterialAndTextureBakeMode;
    EObjectFlags ObjectFlag = ( BakeMode == EBakeMode::CookToTemp ) ? RF_NoFlags : RF_Standalone;

    // Name of generating Houdini parameter.
    FString GeneratingParameterName = TEXT( "" );

    // Specular texture creation parameters.
    FCreateTexture2DParameters CreateTexture2DParameters;
    CreateTexture2DParameters.SourceGuidHash = FGuid();
    CreateTexture2DParameters.bUseAlpha = false;
    CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
    CreateTexture2DParameters.bDeferCompression = true;
    CreateTexture2DParameters.bSRGB = false;

    // See if specular texture is available.
    HAPI_ParmId ParmNameSpecularId =
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_MAP_SPECULAR_0 );

    if ( ParmNameSpecularId >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_SPECULAR_0 );
    }
    else
    {
        ParmNameSpecularId =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_MAP_SPECULAR_1 );

        if ( ParmNameSpecularId >= 0 )
            GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_SPECULAR_1 );
    }

    if ( ParmNameSpecularId >= 0 )
    {
        TArray< char > ImageBuffer;

        // Retrieve color plane.
        if ( FHoudiniEngineMaterialUtils::HapiExtractImage(
            ParmNameSpecularId, MaterialInfo, ImageBuffer,
            HAPI_UNREAL_MATERIAL_TEXTURE_COLOR, HAPI_IMAGE_DATA_INT8, HAPI_IMAGE_PACKING_RGBA, true ) )
        {
            UMaterialExpressionTextureSampleParameter2D * ExpressionSpecular =
                Cast< UMaterialExpressionTextureSampleParameter2D >( Material->Specular.Expression );

            UTexture2D * TextureSpecular = nullptr;
            if ( ExpressionSpecular )
            {
                TextureSpecular = Cast< UTexture2D >( ExpressionSpecular->Texture );
            }
            else
            {
                // Otherwise new expression is of a different type.
                if ( Material->Specular.Expression )
                {
                    Material->Specular.Expression->ConditionalBeginDestroy();
                    Material->Specular.Expression = nullptr;
                }
            }

            UPackage * TextureSpecularPackage = nullptr;
            if ( TextureSpecular )
                TextureSpecularPackage = Cast< UPackage >( TextureSpecular->GetOuter() );

            HAPI_ImageInfo ImageInfo;
            Result = FHoudiniApi::GetImageInfo(
                FHoudiniEngine::Get().GetSession(),
                MaterialInfo.nodeId, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureSpecularName;
                bool bCreatedNewTextureSpecular = false;

                // Create specular texture package, if this is a new specular texture.
                if ( !TextureSpecularPackage )
                {
                    TextureSpecularPackage = FHoudiniEngineBakeUtils::BakeCreateTexturePackageForComponent(
                        HoudiniCookParams,
                        MaterialInfo,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_SPECULAR,
                        TextureSpecularName );
                }

                // Create specular texture, if we need to create one.
                if ( !TextureSpecular )
                    bCreatedNewTextureSpecular = true;

                // Get the node path to add it to the meta data
                FString NodePath;
                GetUniqueMaterialShopName( AssetId, MaterialInfo.nodeId, NodePath );

                // Reuse existing specular texture, or create new one.
                TextureSpecular = FHoudiniEngineMaterialUtils::CreateUnrealTexture(
                    TextureSpecular, ImageInfo,
                    TextureSpecularPackage, TextureSpecularName, ImageBuffer,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_SPECULAR,
                    CreateTexture2DParameters,
                    TEXTUREGROUP_World, NodePath );

                if ( BakeMode == EBakeMode::CookToTemp )
                    TextureSpecular->SetFlags( RF_Public | RF_Standalone );

                // Create specular sampling expression, if needed.
                if ( !ExpressionSpecular )
                {
                    ExpressionSpecular = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag );
                }

                // Record generating parameter.
                ExpressionSpecular->Desc = GeneratingParameterName;
                ExpressionSpecular->ParameterName = *GeneratingParameterName;

                ExpressionSpecular->Texture = TextureSpecular;
                ExpressionSpecular->SamplerType = SAMPLERTYPE_LinearGrayscale;

                // Offset node placement.
                ExpressionSpecular->MaterialExpressionEditorX = FHoudiniEngineMaterialUtils::MaterialExpressionNodeX;
                ExpressionSpecular->MaterialExpressionEditorY = MaterialNodeY;
                MaterialNodeY += FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepY;

                // Assign expression to material.
                Material->Expressions.Add( ExpressionSpecular );
                Material->Specular.Expression = ExpressionSpecular;

                bExpressionCreated = true;

                // Propagate and trigger specular texture updates.
                if (bCreatedNewTextureSpecular)
                    FAssetRegistryModule::AssetCreated(TextureSpecular);

                TextureSpecular->PreEditChange(nullptr);
                TextureSpecular->PostEditChange();
                TextureSpecular->MarkPackageDirty();
            }
        }
    }

    HAPI_ParmInfo ParmInfoSpecularColor;
    HAPI_ParmId ParmNameSpecularColorId =
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_COLOR_SPECULAR_0, ParmInfoSpecularColor );

    if( ParmNameSpecularColorId >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_COLOR_SPECULAR_0 );
    }
    else
    {
        ParmNameSpecularColorId =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_COLOR_SPECULAR_1, ParmInfoSpecularColor );

        if ( ParmNameSpecularColorId >= 0 )
            GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_COLOR_SPECULAR_1 );
    }

    if ( !bExpressionCreated && ParmNameSpecularColorId >= 0 )
    {
        // Specular color is available.

        FLinearColor Color = FLinearColor::White;

        if ( FHoudiniApi::GetParmFloatValues(
            FHoudiniEngine::Get().GetSession(), NodeInfo.id, (float*) &Color.R,
            ParmInfoSpecularColor.floatValuesIndex, ParmInfoSpecularColor.size ) == HAPI_RESULT_SUCCESS )
        {
            if (ParmInfoSpecularColor.size == 3 )
                Color.A = 1.0f;

            UMaterialExpressionVectorParameter * ExpressionSpecularColor =
                Cast< UMaterialExpressionVectorParameter >( Material->Specular.Expression );

            // Create color const expression and add it to material, if we don't have one.
            if ( !ExpressionSpecularColor )
            {
                // Otherwise new expression is of a different type.
                if ( Material->Specular.Expression )
                {
                    Material->Specular.Expression->ConditionalBeginDestroy();
                    Material->Specular.Expression = nullptr;
                }

                ExpressionSpecularColor = NewObject< UMaterialExpressionVectorParameter >(
                    Material, UMaterialExpressionVectorParameter::StaticClass(), NAME_None, ObjectFlag );
            }

            // Record generating parameter.
            ExpressionSpecularColor->Desc = GeneratingParameterName;
            ExpressionSpecularColor->ParameterName = *GeneratingParameterName;

            ExpressionSpecularColor->DefaultValue = Color;

            // Offset node placement.
            ExpressionSpecularColor->MaterialExpressionEditorX = FHoudiniEngineMaterialUtils::MaterialExpressionNodeX;
            ExpressionSpecularColor->MaterialExpressionEditorY = MaterialNodeY;
            MaterialNodeY += FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepY;

            // Assign expression to material.
            Material->Expressions.Add( ExpressionSpecularColor );
            Material->Specular.Expression = ExpressionSpecularColor;

            bExpressionCreated = true;
        }
    }

    return bExpressionCreated;
}

bool
FHoudiniEngineMaterialUtils::CreateMaterialComponentRoughness(
    FHoudiniCookParams& HoudiniCookParams, const HAPI_NodeId& AssetId,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo,
    const HAPI_NodeInfo & NodeInfo, int32 & MaterialNodeY )
{
    bool bExpressionCreated = false;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    EBakeMode BakeMode = HoudiniCookParams.MaterialAndTextureBakeMode;
    EObjectFlags ObjectFlag = ( BakeMode == EBakeMode::CookToTemp ) ? RF_NoFlags : RF_Standalone;

    // Name of generating Houdini parameter.
    FString GeneratingParameterName = TEXT( "" );

    // Roughness texture creation parameters.
    FCreateTexture2DParameters CreateTexture2DParameters;
    CreateTexture2DParameters.SourceGuidHash = FGuid();
    CreateTexture2DParameters.bUseAlpha = false;
    CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
    CreateTexture2DParameters.bDeferCompression = true;
    CreateTexture2DParameters.bSRGB = false;

    // See if roughness texture is available.
    HAPI_ParmId ParmNameRoughnessId =
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_MAP_ROUGHNESS_0 );

    if ( ParmNameRoughnessId >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_ROUGHNESS_0 );
    }
    else
    {
        ParmNameRoughnessId =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_MAP_ROUGHNESS_1 );

        if ( ParmNameRoughnessId >= 0 )
            GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_ROUGHNESS_1 );
    }

    if ( ParmNameRoughnessId >= 0 )
    {
        TArray< char > ImageBuffer;

        // Retrieve color plane.
        if ( FHoudiniEngineMaterialUtils::HapiExtractImage(
            ParmNameRoughnessId, MaterialInfo, ImageBuffer,
            HAPI_UNREAL_MATERIAL_TEXTURE_COLOR, HAPI_IMAGE_DATA_INT8, HAPI_IMAGE_PACKING_RGBA, true ) )
        {
            UMaterialExpressionTextureSampleParameter2D* ExpressionRoughness =
                Cast< UMaterialExpressionTextureSampleParameter2D >( Material->Roughness.Expression );

            UTexture2D* TextureRoughness = nullptr;
            if ( ExpressionRoughness )
            {
                TextureRoughness = Cast< UTexture2D >( ExpressionRoughness->Texture );
            }
            else
            {
                // Otherwise new expression is of a different type.
                if ( Material->Roughness.Expression )
                {
                    Material->Roughness.Expression->ConditionalBeginDestroy();
                    Material->Roughness.Expression = nullptr;
                }
            }

            UPackage * TextureRoughnessPackage = nullptr;
            if ( TextureRoughness )
                TextureRoughnessPackage = Cast< UPackage >( TextureRoughness->GetOuter() );

            HAPI_ImageInfo ImageInfo;
            Result = FHoudiniApi::GetImageInfo(
                FHoudiniEngine::Get().GetSession(),
                MaterialInfo.nodeId, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureRoughnessName;
                bool bCreatedNewTextureRoughness = false;

                // Create roughness texture package, if this is a new roughness texture.
                if ( !TextureRoughnessPackage )
                {
                    TextureRoughnessPackage = FHoudiniEngineBakeUtils::BakeCreateTexturePackageForComponent(
                        HoudiniCookParams,
                        MaterialInfo,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_ROUGHNESS,
                        TextureRoughnessName );
                }

                // Create roughness texture, if we need to create one.
                if ( !TextureRoughness )
                    bCreatedNewTextureRoughness = true;

                // Get the node path to add it to the meta data
                FString NodePath;
                GetUniqueMaterialShopName( AssetId, MaterialInfo.nodeId, NodePath );

                // Reuse existing roughness texture, or create new one.
                TextureRoughness = FHoudiniEngineMaterialUtils::CreateUnrealTexture(
                    TextureRoughness, ImageInfo,
                    TextureRoughnessPackage, TextureRoughnessName, ImageBuffer,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_ROUGHNESS,
                    CreateTexture2DParameters,
                    TEXTUREGROUP_World, NodePath );

                if ( BakeMode == EBakeMode::CookToTemp )
                    TextureRoughness->SetFlags( RF_Public | RF_Standalone );

                // Create roughness sampling expression, if needed.
                if ( !ExpressionRoughness )
                    ExpressionRoughness = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag );

                // Record generating parameter.
                ExpressionRoughness->Desc = GeneratingParameterName;
                ExpressionRoughness->ParameterName = *GeneratingParameterName;

                ExpressionRoughness->Texture = TextureRoughness;
                ExpressionRoughness->SamplerType = SAMPLERTYPE_LinearGrayscale;

                // Offset node placement.
                ExpressionRoughness->MaterialExpressionEditorX = FHoudiniEngineMaterialUtils::MaterialExpressionNodeX;
                ExpressionRoughness->MaterialExpressionEditorY = MaterialNodeY;
                MaterialNodeY += FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepY;

                // Assign expression to material.
                Material->Expressions.Add( ExpressionRoughness );
                Material->Roughness.Expression = ExpressionRoughness;

                bExpressionCreated = true;

                // Propagate and trigger roughness texture updates.
                if (bCreatedNewTextureRoughness)
                    FAssetRegistryModule::AssetCreated(TextureRoughness);

                TextureRoughness->PreEditChange(nullptr);
                TextureRoughness->PostEditChange();
                TextureRoughness->MarkPackageDirty();
            }
        }
    }

    HAPI_ParmInfo ParmInfoRoughnessValue;
    HAPI_ParmId ParmNameRoughnessValueId =
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_VALUE_ROUGHNESS_0, ParmInfoRoughnessValue );

    if ( ParmNameRoughnessValueId >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_VALUE_ROUGHNESS_0 );
    }
    else
    {
        ParmNameRoughnessValueId =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_VALUE_ROUGHNESS_1, ParmInfoRoughnessValue );

        if ( ParmNameRoughnessValueId >= 0 )
            GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_VALUE_ROUGHNESS_1 );
    }

    if ( !bExpressionCreated && ParmNameRoughnessValueId >= 0 )
    {
        // Roughness value is available.

        float RoughnessValue = 0.0f;

        if ( FHoudiniApi::GetParmFloatValues(
            FHoudiniEngine::Get().GetSession(), NodeInfo.id, (float *) &RoughnessValue,
            ParmInfoRoughnessValue.floatValuesIndex, 1 ) == HAPI_RESULT_SUCCESS )
        {
            UMaterialExpressionScalarParameter * ExpressionRoughnessValue =
                Cast< UMaterialExpressionScalarParameter >( Material->Roughness.Expression );

            // Clamp retrieved value.
            RoughnessValue = FMath::Clamp< float >( RoughnessValue, 0.0f, 1.0f );

            // Create color const expression and add it to material, if we don't have one.
            if ( !ExpressionRoughnessValue )
            {
                // Otherwise new expression is of a different type.
                if ( Material->Roughness.Expression )
                {
                    Material->Roughness.Expression->ConditionalBeginDestroy();
                    Material->Roughness.Expression = nullptr;
                }

                ExpressionRoughnessValue = NewObject< UMaterialExpressionScalarParameter >(
                    Material, UMaterialExpressionScalarParameter::StaticClass(), NAME_None, ObjectFlag );
            }

            // Record generating parameter.
            ExpressionRoughnessValue->Desc = GeneratingParameterName;
            ExpressionRoughnessValue->ParameterName = *GeneratingParameterName;

            ExpressionRoughnessValue->DefaultValue = RoughnessValue;
            ExpressionRoughnessValue->SliderMin = 0.0f;
            ExpressionRoughnessValue->SliderMax = 1.0f;

            // Offset node placement.
            ExpressionRoughnessValue->MaterialExpressionEditorX = FHoudiniEngineMaterialUtils::MaterialExpressionNodeX;
            ExpressionRoughnessValue->MaterialExpressionEditorY = MaterialNodeY;
            MaterialNodeY += FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepY;

            // Assign expression to material.
            Material->Expressions.Add( ExpressionRoughnessValue );
            Material->Roughness.Expression = ExpressionRoughnessValue;

            bExpressionCreated = true;
        }
    }

    return bExpressionCreated;
}

bool
FHoudiniEngineMaterialUtils::CreateMaterialComponentMetallic(
    FHoudiniCookParams& HoudiniCookParams, const HAPI_NodeId& AssetId,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo,
    const HAPI_NodeInfo & NodeInfo, int32 & MaterialNodeY )
{
    bool bExpressionCreated = false;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    EBakeMode BakeMode = HoudiniCookParams.MaterialAndTextureBakeMode;
    EObjectFlags ObjectFlag = ( BakeMode == EBakeMode::CookToTemp ) ? RF_NoFlags : RF_Standalone;

    // Name of generating Houdini parameter.
    FString GeneratingParameterName = TEXT( "" );

    // Metallic texture creation parameters.
    FCreateTexture2DParameters CreateTexture2DParameters;
    CreateTexture2DParameters.SourceGuidHash = FGuid();
    CreateTexture2DParameters.bUseAlpha = false;
    CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
    CreateTexture2DParameters.bDeferCompression = true;
    CreateTexture2DParameters.bSRGB = false;

    // See if metallic texture is available.
    HAPI_ParmId ParmNameMetallicId =
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_MAP_METALLIC );

    if ( ParmNameMetallicId >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_METALLIC );
    }

    if ( ParmNameMetallicId >= 0 )
    {
        TArray< char > ImageBuffer;

        // Retrieve color plane.
        if ( FHoudiniEngineMaterialUtils::HapiExtractImage(
            ParmNameMetallicId, MaterialInfo, ImageBuffer,
            HAPI_UNREAL_MATERIAL_TEXTURE_COLOR, HAPI_IMAGE_DATA_INT8, HAPI_IMAGE_PACKING_RGBA, true ) )
        {
            UMaterialExpressionTextureSampleParameter2D * ExpressionMetallic =
                Cast< UMaterialExpressionTextureSampleParameter2D >( Material->Metallic.Expression );

            UTexture2D * TextureMetallic = nullptr;
            if ( ExpressionMetallic )
            {
                TextureMetallic = Cast< UTexture2D >( ExpressionMetallic->Texture );
            }
            else
            {
                // Otherwise new expression is of a different type.
                if ( Material->Metallic.Expression )
                {
                    Material->Metallic.Expression->ConditionalBeginDestroy();
                    Material->Metallic.Expression = nullptr;
                }
            }

            UPackage * TextureMetallicPackage = nullptr;
            if ( TextureMetallic )
                TextureMetallicPackage = Cast< UPackage >( TextureMetallic->GetOuter() );

            HAPI_ImageInfo ImageInfo;
            Result = FHoudiniApi::GetImageInfo(
                FHoudiniEngine::Get().GetSession(),
                MaterialInfo.nodeId, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureMetallicName;
                bool bCreatedNewTextureMetallic = false;

                // Create metallic texture package, if this is a new metallic texture.
                if ( !TextureMetallicPackage )
                {
                    TextureMetallicPackage = FHoudiniEngineBakeUtils::BakeCreateTexturePackageForComponent(
                        HoudiniCookParams,
                        MaterialInfo,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_METALLIC,
                        TextureMetallicName );
                }

                // Create metallic texture, if we need to create one.
                if ( !TextureMetallic )
                    bCreatedNewTextureMetallic = true;

                // Get the node path to add it to the meta data
                FString NodePath;
                GetUniqueMaterialShopName( AssetId, MaterialInfo.nodeId, NodePath );

                // Reuse existing metallic texture, or create new one.
                TextureMetallic = FHoudiniEngineMaterialUtils::CreateUnrealTexture(
                    TextureMetallic, ImageInfo,
                    TextureMetallicPackage, TextureMetallicName, ImageBuffer,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_METALLIC,
                    CreateTexture2DParameters,
                    TEXTUREGROUP_World, NodePath );

                if ( BakeMode == EBakeMode::CookToTemp )
                    TextureMetallic->SetFlags( RF_Public | RF_Standalone );

                // Create metallic sampling expression, if needed.
                if ( !ExpressionMetallic )
                    ExpressionMetallic = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag );

                // Record generating parameter.
                ExpressionMetallic->Desc = GeneratingParameterName;
                ExpressionMetallic->ParameterName = *GeneratingParameterName;

                ExpressionMetallic->Texture = TextureMetallic;
                ExpressionMetallic->SamplerType = SAMPLERTYPE_LinearGrayscale;

                // Offset node placement.
                ExpressionMetallic->MaterialExpressionEditorX = FHoudiniEngineMaterialUtils::MaterialExpressionNodeX;
                ExpressionMetallic->MaterialExpressionEditorY = MaterialNodeY;
                MaterialNodeY += FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepY;

                // Assign expression to material.
                Material->Expressions.Add( ExpressionMetallic );
                Material->Metallic.Expression = ExpressionMetallic;

                bExpressionCreated = true;

                // Propagate and trigger metallic texture updates.
                if (bCreatedNewTextureMetallic)
                    FAssetRegistryModule::AssetCreated(TextureMetallic);

                TextureMetallic->PreEditChange(nullptr);
                TextureMetallic->PostEditChange();
                TextureMetallic->MarkPackageDirty();
            }
        }
    }

    HAPI_ParmInfo ParmInfoMetallic;
    HAPI_ParmId ParmNameMetallicValueIdx =
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_VALUE_METALLIC, ParmInfoMetallic );

    if ( ParmNameMetallicValueIdx >= 0 )
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_VALUE_METALLIC );

    if ( !bExpressionCreated && ParmNameMetallicValueIdx >= 0 )
    {
        // Metallic value is available.

        float MetallicValue = 0.0f;

        if ( FHoudiniApi::GetParmFloatValues(
            FHoudiniEngine::Get().GetSession(), NodeInfo.id, (float *) &MetallicValue,
            ParmInfoMetallic.floatValuesIndex, 1 ) == HAPI_RESULT_SUCCESS )
        {
            UMaterialExpressionScalarParameter * ExpressionMetallicValue =
                Cast< UMaterialExpressionScalarParameter >( Material->Metallic.Expression );

            // Clamp retrieved value.
            MetallicValue = FMath::Clamp< float >( MetallicValue, 0.0f, 1.0f );

            // Create color const expression and add it to material, if we don't have one.
            if ( !ExpressionMetallicValue )
            {
                // Otherwise new expression is of a different type.
                if ( Material->Metallic.Expression )
                {
                    Material->Metallic.Expression->ConditionalBeginDestroy();
                    Material->Metallic.Expression = nullptr;
                }

                ExpressionMetallicValue = NewObject< UMaterialExpressionScalarParameter >(
                    Material, UMaterialExpressionScalarParameter::StaticClass(), NAME_None, ObjectFlag );
            }

            // Record generating parameter.
            ExpressionMetallicValue->Desc = GeneratingParameterName;
            ExpressionMetallicValue->ParameterName = *GeneratingParameterName;

            ExpressionMetallicValue->DefaultValue = MetallicValue;
            ExpressionMetallicValue->SliderMin = 0.0f;
            ExpressionMetallicValue->SliderMax = 1.0f;

            // Offset node placement.
            ExpressionMetallicValue->MaterialExpressionEditorX = FHoudiniEngineMaterialUtils::MaterialExpressionNodeX;
            ExpressionMetallicValue->MaterialExpressionEditorY = MaterialNodeY;
            MaterialNodeY += FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepY;

            // Assign expression to material.
            Material->Expressions.Add( ExpressionMetallicValue );
            Material->Metallic.Expression = ExpressionMetallicValue;

            bExpressionCreated = true;
        }
    }

    return bExpressionCreated;
}


bool
FHoudiniEngineMaterialUtils::CreateMaterialComponentEmissive(
    FHoudiniCookParams& HoudiniCookParams, const HAPI_NodeId& AssetId,
    UMaterial * Material, const HAPI_MaterialInfo & MaterialInfo,
    const HAPI_NodeInfo & NodeInfo, int32 & MaterialNodeY )
{
    bool bExpressionCreated = false;
    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    EBakeMode BakeMode = HoudiniCookParams.MaterialAndTextureBakeMode;
    EObjectFlags ObjectFlag = ( BakeMode == EBakeMode::CookToTemp ) ? RF_NoFlags : RF_Standalone;

    // Name of generating Houdini parameter.
    FString GeneratingParameterName = TEXT("");

    // Emissive texture creation parameters.
    FCreateTexture2DParameters CreateTexture2DParameters;
    CreateTexture2DParameters.SourceGuidHash = FGuid();
    CreateTexture2DParameters.bUseAlpha = false;
    CreateTexture2DParameters.CompressionSettings = TC_Grayscale;
    CreateTexture2DParameters.bDeferCompression = true;
    CreateTexture2DParameters.bSRGB = false;

    // See if emissive texture is available.
    HAPI_ParmId ParmNameEmissiveId =
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_MAP_EMISSIVE );

    if ( ParmNameEmissiveId >= 0 )
    {
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_MAP_EMISSIVE );
    }

    if ( ParmNameEmissiveId >= 0 )
    {
        TArray< char > ImageBuffer;

        // Retrieve color plane.
        if ( FHoudiniEngineMaterialUtils::HapiExtractImage(
            ParmNameEmissiveId, MaterialInfo, ImageBuffer,
            HAPI_UNREAL_MATERIAL_TEXTURE_COLOR, HAPI_IMAGE_DATA_INT8, HAPI_IMAGE_PACKING_RGBA, true ) )
        {
            UMaterialExpressionTextureSampleParameter2D * ExpressionEmissive =
                Cast< UMaterialExpressionTextureSampleParameter2D >( Material->EmissiveColor.Expression );

            UTexture2D * TextureEmissive = nullptr;
            if ( ExpressionEmissive )
            {
                TextureEmissive = Cast< UTexture2D >( ExpressionEmissive->Texture );
            }
            else
            {
                // Otherwise new expression is of a different type.
                if ( Material->EmissiveColor.Expression )
                {
                    Material->EmissiveColor.Expression->ConditionalBeginDestroy();
                    Material->EmissiveColor.Expression = nullptr;
                }
            }

            UPackage * TextureEmissivePackage = nullptr;
            if ( TextureEmissive )
                TextureEmissivePackage = Cast< UPackage >( TextureEmissive->GetOuter() );

            HAPI_ImageInfo ImageInfo;
            Result = FHoudiniApi::GetImageInfo(
                FHoudiniEngine::Get().GetSession(),
                MaterialInfo.nodeId, &ImageInfo );

            if ( Result == HAPI_RESULT_SUCCESS && ImageInfo.xRes > 0 && ImageInfo.yRes > 0 )
            {
                // Create texture.
                FString TextureEmissiveName;
                bool bCreatedNewTextureEmissive = false;

                // Create emissive texture package, if this is a new emissive texture.
                if ( !TextureEmissivePackage )
                {
                    TextureEmissivePackage = FHoudiniEngineBakeUtils::BakeCreateTexturePackageForComponent(
                        HoudiniCookParams,
                        MaterialInfo,
                        HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_EMISSIVE,
                        TextureEmissiveName );
                }

                // Create emissive texture, if we need to create one.
                if ( !TextureEmissive )
                    bCreatedNewTextureEmissive = true;

                // Get the node path to add it to the meta data
                FString NodePath;
                GetUniqueMaterialShopName( AssetId, MaterialInfo.nodeId, NodePath );

                // Reuse existing emissive texture, or create new one.
                TextureEmissive = FHoudiniEngineMaterialUtils::CreateUnrealTexture(
                    TextureEmissive, ImageInfo,
                    TextureEmissivePackage, TextureEmissiveName, ImageBuffer,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_EMISSIVE,
                    CreateTexture2DParameters,
                    TEXTUREGROUP_World, NodePath );

                if ( BakeMode == EBakeMode::CookToTemp )
                    TextureEmissive->SetFlags( RF_Public | RF_Standalone );

                // Create emissive sampling expression, if needed.
                if ( !ExpressionEmissive )
                    ExpressionEmissive = NewObject< UMaterialExpressionTextureSampleParameter2D >(
                        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), NAME_None, ObjectFlag );

                // Record generating parameter.
                ExpressionEmissive->Desc = GeneratingParameterName;
                ExpressionEmissive->ParameterName = *GeneratingParameterName;

                ExpressionEmissive->Texture = TextureEmissive;
                ExpressionEmissive->SamplerType = SAMPLERTYPE_LinearGrayscale;

                // Offset node placement.
                ExpressionEmissive->MaterialExpressionEditorX = FHoudiniEngineMaterialUtils::MaterialExpressionNodeX;
                ExpressionEmissive->MaterialExpressionEditorY = MaterialNodeY;
                MaterialNodeY += FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepY;

                // Assign expression to material.
                Material->Expressions.Add( ExpressionEmissive );
                Material->EmissiveColor.Expression = ExpressionEmissive;

                bExpressionCreated = true;

                // Propagate and trigger metallic texture updates.
                if (bCreatedNewTextureEmissive)
                    FAssetRegistryModule::AssetCreated(TextureEmissive);

                TextureEmissive->PreEditChange(nullptr);
                TextureEmissive->PostEditChange();
                TextureEmissive->MarkPackageDirty();
            }
        }
    }

    HAPI_ParmInfo ParmInfoEmissive;
    HAPI_ParmId ParmNameEmissiveValueId =
        FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_VALUE_EMISSIVE_0, ParmInfoEmissive );

    if ( ParmNameEmissiveValueId >= 0 )
        GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_VALUE_EMISSIVE_0 );
    else
    {
        ParmNameEmissiveValueId =
            FHoudiniEngineUtils::HapiFindParameterByNameOrTag( NodeInfo.id, HAPI_UNREAL_PARAM_VALUE_EMISSIVE_1, ParmInfoEmissive );

        if ( ParmNameEmissiveValueId >= 0 )
            GeneratingParameterName = TEXT( HAPI_UNREAL_PARAM_VALUE_EMISSIVE_1 );
    }

    if ( !bExpressionCreated && ParmNameEmissiveValueId >= 0 )
    {
        // Emissive color is available.

        FLinearColor Color = FLinearColor::White;

        if ( FHoudiniApi::GetParmFloatValues(
            FHoudiniEngine::Get().GetSession(), NodeInfo.id, (float*)&Color.R,
            ParmInfoEmissive.floatValuesIndex, ParmInfoEmissive.size ) == HAPI_RESULT_SUCCESS )
        {
            if ( ParmInfoEmissive.size == 3 )
                Color.A = 1.0f;

            UMaterialExpressionConstant4Vector * ExpressionEmissiveColor =
                Cast< UMaterialExpressionConstant4Vector >( Material->EmissiveColor.Expression );

            // Create color const expression and add it to material, if we don't have one.
            if ( !ExpressionEmissiveColor )
            {
                // Otherwise new expression is of a different type.
                if ( Material->EmissiveColor.Expression )
                {
                    Material->EmissiveColor.Expression->ConditionalBeginDestroy();
                    Material->EmissiveColor.Expression = nullptr;
                }

                ExpressionEmissiveColor = NewObject< UMaterialExpressionConstant4Vector >(
                    Material, UMaterialExpressionConstant4Vector::StaticClass(), NAME_None, ObjectFlag );
            }

            // Record generating parameter.
            ExpressionEmissiveColor->Desc = GeneratingParameterName;
            if ( ExpressionEmissiveColor->CanRenameNode() )
                ExpressionEmissiveColor->SetEditableName( *GeneratingParameterName );

            ExpressionEmissiveColor->Constant = Color;

            // Offset node placement.
            ExpressionEmissiveColor->MaterialExpressionEditorX = FHoudiniEngineMaterialUtils::MaterialExpressionNodeX;
            ExpressionEmissiveColor->MaterialExpressionEditorY = MaterialNodeY;
            MaterialNodeY += FHoudiniEngineMaterialUtils::MaterialExpressionNodeStepY;

            // Assign expression to material.
            Material->Expressions.Add( ExpressionEmissiveColor );
            Material->EmissiveColor.Expression = ExpressionEmissiveColor;

            bExpressionCreated = true;
        }
    }

    return bExpressionCreated;
}

UTexture2D *
FHoudiniEngineMaterialUtils::CreateUnrealTexture(
    UTexture2D * ExistingTexture, const HAPI_ImageInfo & ImageInfo,
    UPackage * Package, const FString & TextureName,
    const TArray< char > & ImageBuffer, const FString & TextureType,
    const FCreateTexture2DParameters & TextureParameters, TextureGroup LODGroup, const FString& NodePath )
{
    UTexture2D * Texture = nullptr;
    if ( ExistingTexture )
    {
        Texture = ExistingTexture;
    }
    else
    {
        // Create new texture object.
        Texture = NewObject< UTexture2D >(
            Package, UTexture2D::StaticClass(), *TextureName,
            RF_Transactional );

        // Assign texture group.
        Texture->LODGroup = LODGroup;
    }

    // Add/Update meta information to package.
    FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
        Package, Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT( "true" ) );
    FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
        Package, Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *TextureName );
    FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
        Package, Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE, *TextureType );
    FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
        Package, Texture, HAPI_UNREAL_PACKAGE_META_NODE_PATH, *NodePath );

    // Initialize texture source.
    Texture->Source.Init( ImageInfo.xRes, ImageInfo.yRes, 1, 1, TSF_BGRA8 );

    // Lock the texture.
    uint8 * MipData = Texture->Source.LockMip( 0 );

    // Create base map.
    uint8* DestPtr = nullptr;
    uint32 SrcWidth = ImageInfo.xRes;
    uint32 SrcHeight = ImageInfo.yRes;
    const char * SrcData = &ImageBuffer[ 0 ];

    for ( uint32 y = 0; y < SrcHeight; y++ )
    {
        DestPtr = &MipData[ ( SrcHeight - 1 - y ) * SrcWidth * sizeof( FColor ) ];

        for ( uint32 x = 0; x < SrcWidth; x++ )
        {
            uint32 DataOffset = y * SrcWidth * 4 + x * 4;

            *DestPtr++ = *(uint8*)( SrcData + DataOffset + 2 ); // B
            *DestPtr++ = *(uint8*)( SrcData + DataOffset + 1 ); // G
            *DestPtr++ = *(uint8*)( SrcData + DataOffset + 0 ); // R

            if ( TextureParameters.bUseAlpha )
                *DestPtr++ = *(uint8*)( SrcData + DataOffset + 3 ); // A
            else
                *DestPtr++ = 0xFF;
        }
    }

    bool bHasAlphaValue = false;
    if ( TextureParameters.bUseAlpha )
    {
        // See if there is an actual alpha value in the texture or if we can ignore the texture alpha
        for ( uint32 y = 0; y < SrcHeight; y++ )
        {
            for ( uint32 x = 0; x < SrcWidth; x++ )
            {
                uint32 DataOffset = y * SrcWidth * 4 + x * 4;
                if (*(uint8*)(SrcData + DataOffset + 3) != 0xFF)
                {
                    bHasAlphaValue = true;
                    break;
                }
            }

            if ( bHasAlphaValue )
                break;
        }
    }

    // Unlock the texture.
    Texture->Source.UnlockMip( 0 );

    // Texture creation parameters.
    Texture->SRGB = TextureParameters.bSRGB;
    Texture->CompressionSettings = TextureParameters.CompressionSettings;
    Texture->CompressionNoAlpha = !bHasAlphaValue;
    Texture->DeferCompression = TextureParameters.bDeferCompression;

    // Set the Source Guid/Hash if specified.
    /*
    if ( TextureParameters.SourceGuidHash.IsValid() )
    {
        Texture->Source.SetId( TextureParameters.SourceGuidHash, true );
    }
    */

    Texture->PostEditChange();

    return Texture;
}

#endif

bool
FHoudiniEngineMaterialUtils::GetUniqueMaterialShopName( HAPI_NodeId AssetId, HAPI_NodeId MaterialId, FString & Name )
{
    if ( AssetId < 0 || MaterialId < 0 )
        return false;

    HAPI_AssetInfo AssetInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAssetInfo(
        FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo ), false );

    HAPI_MaterialInfo MaterialInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetMaterialInfo(
        FHoudiniEngine::Get().GetSession(), MaterialId,
        &MaterialInfo ), false );

    HAPI_NodeInfo AssetNodeInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetNodeInfo(
        FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId,
        &AssetNodeInfo ), false );

    HAPI_NodeInfo MaterialNodeInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetNodeInfo(
        FHoudiniEngine::Get().GetSession(), MaterialInfo.nodeId,
        &MaterialNodeInfo ), false );

    FString AssetNodeName = TEXT( "" );
    FString MaterialNodeName = TEXT( "" );

    {
        FHoudiniEngineString HoudiniEngineString( AssetNodeInfo.internalNodePathSH );
        if ( !HoudiniEngineString.ToFString( AssetNodeName ) )
            return false;
    }

    {
        FHoudiniEngineString HoudiniEngineString( MaterialNodeInfo.internalNodePathSH );
        if ( !HoudiniEngineString.ToFString( MaterialNodeName ) )
            return false;
    }

    if ( AssetNodeName.Len() > 0 && MaterialNodeName.Len() > 0 )
    {
        // Remove AssetNodeName part from MaterialNodeName. Extra position is for separator.
        Name = MaterialNodeName.Mid( AssetNodeName.Len() + 1 );
        return true;
    }

    return false;
}

UMaterialExpression *
FHoudiniEngineMaterialUtils::MaterialLocateExpression( UMaterialExpression * Expression, UClass * ExpressionClass )
{
    if ( !Expression )
        return nullptr;
#if WITH_EDITOR
    if ( ExpressionClass == Expression->GetClass() )
        return Expression;


    // If this is a channel multiply expression, we can recurse.
    UMaterialExpressionMultiply * MaterialExpressionMultiply = Cast< UMaterialExpressionMultiply >( Expression );
    if ( MaterialExpressionMultiply )
    {
        {
            UMaterialExpression * MaterialExpression = MaterialExpressionMultiply->A.Expression;
            if ( MaterialExpression )
            {
                if ( MaterialExpression->GetClass() == ExpressionClass )
                    return MaterialExpression;

                MaterialExpression = FHoudiniEngineMaterialUtils::MaterialLocateExpression(
                    Cast< UMaterialExpressionMultiply >( MaterialExpression ), ExpressionClass );

                if ( MaterialExpression )
                    return MaterialExpression;
            }
        }

        {
            UMaterialExpression * MaterialExpression = MaterialExpressionMultiply->B.Expression;
            if ( MaterialExpression )
            {
                if ( MaterialExpression->GetClass() == ExpressionClass )
                    return MaterialExpression;

                MaterialExpression = FHoudiniEngineMaterialUtils::MaterialLocateExpression(
                    Cast< UMaterialExpressionMultiply >( MaterialExpression ), ExpressionClass );

                if ( MaterialExpression )
                    return MaterialExpression;
            }
        }
    }
#endif

    return nullptr;
}


bool
FHoudiniEngineMaterialUtils::CreateMaterialInstances(
    const FHoudiniGeoPartObject& HoudiniGeoPartObject, FHoudiniCookParams& CookParams,
    UMaterialInstance*& CreatedMaterialInstance, UMaterialInterface*& OriginalMaterialInterface,
    std::string AttributeName, int32 MaterialIndex)
{
#if WITH_EDITOR
    if ( !HoudiniGeoPartObject.IsValid() )
        return false;

    // First, make sure this geopartObj has a material instance attribute
    if ( !FHoudiniEngineUtils::HapiCheckAttributeExists(
        HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId,
        HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId,
        AttributeName.c_str() ) )
        return false;

    // Get the material instance attribute info
    HAPI_AttributeInfo AttribMaterialInstances;
    FMemory::Memzero< HAPI_AttributeInfo >( AttribMaterialInstances );

    TArray< FString > MaterialInstances;
    FHoudiniEngineUtils::HapiGetAttributeDataAsString(
        HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId,
        HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId,
        AttributeName.c_str(),
        AttribMaterialInstances, MaterialInstances );

    // No material instance attribute
    if ( !AttribMaterialInstances.exists || MaterialInstances.Num() <= 0 )
        return false;

    // Get the material name from the material_instance attribute 
    // Since the material instance attribute can be set per primitive, it's going to be very difficult to know 
    // exactly where to look for the nth material instance, so we'll have to iterate on them to find it.
    // In order for the material slot to be created, the material instance attribute had to be different, 
    // so we'll use this to hopefully fetch the right value.
    // This is pretty hacky and we should probably require an extra material_instance_index attribute instead.
    // but still a better solution than ignore all the material slots but the first
    int32 MaterialIndexToAttributeIndex = 0;
    if ( MaterialIndex > 0 && AttribMaterialInstances.owner == HAPI_ATTROWNER_PRIM )
    {
        int32 CurrentMaterialIndex = 0;
        FString CurrentMatName = MaterialInstances[ 0 ];
        for ( int32 n = 0; n < MaterialInstances.Num(); n++ )
        {
            if ( MaterialInstances[ n ].Equals( CurrentMatName ) )
                continue;

            CurrentMatName = MaterialInstances[ n ];
            CurrentMaterialIndex++;

            if ( CurrentMaterialIndex == MaterialIndex )
            {
                MaterialIndexToAttributeIndex = n;
                break;
            }
        }
    }

    const FString & MaterialName = MaterialInstances[ MaterialIndexToAttributeIndex ];
    if ( MaterialName.IsEmpty() )
        return false;

    // Trying to find the material we want to create an instance of
    OriginalMaterialInterface = Cast< UMaterialInterface >(
        StaticLoadObject( UMaterialInterface::StaticClass(), nullptr, *MaterialName, nullptr, LOAD_NoWarn, nullptr ) );

    // The source material wasn't found
    if ( !OriginalMaterialInterface )
        return false;

    UMaterial* ParentMaterial = OriginalMaterialInterface->GetMaterial();
    if ( !ParentMaterial )
        return false;

    // Create/Retrieve the package for the MI
    FString MaterialInstanceName;
    FString MaterialInstanceNamePrefix = PackageTools::SanitizePackageName( ParentMaterial->GetName() + TEXT("_instance_") + FString::FromInt(MaterialIndex) );
    
    // See if we can find the package in the cooked temp package cache
    UPackage * MaterialInstancePackage = nullptr;
    TWeakObjectPtr< UPackage > * FoundPointer = CookParams.CookedTemporaryPackages->Find( MaterialInstanceNamePrefix );
    if ( FoundPointer && (*FoundPointer).IsValid() )
    {
        // We found an already existing package for the M_I
        MaterialInstancePackage = (*FoundPointer).Get();
        MaterialInstanceName = MaterialInstancePackage->GetName();
    }
    else 
    {
        // We Couldnt find the corresponding M_I package, so create a new one
        MaterialInstancePackage = FHoudiniEngineBakeUtils::BakeCreateTextureOrMaterialPackageForComponent(
            CookParams, MaterialInstanceNamePrefix, MaterialInstanceName );
    }

    // Couldn't create a package for the Material Instance
    if ( !MaterialInstancePackage )
        return false;

    //UMaterialInterface * MaterialInstanceInterface = Cast< UMaterialInterface >(
    //    StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialInstanceNameString, nullptr, LOAD_NoWarn, nullptr));
    // Trying to load the material instance from the package
    bool bNewMaterialCreated = false;
    UMaterialInstanceConstant* NewMaterialInstance = LoadObject<UMaterialInstanceConstant>( MaterialInstancePackage, *MaterialInstanceName, nullptr, LOAD_None, nullptr );
    if ( !NewMaterialInstance )
    {
        // Factory to create materials.
        UMaterialInstanceConstantFactoryNew* MaterialInstanceFactory = NewObject< UMaterialInstanceConstantFactoryNew >();
        if ( !MaterialInstanceFactory )
            return false;

        // Create the new material instance
        MaterialInstanceFactory->AddToRoot();
        MaterialInstanceFactory->InitialParent = ParentMaterial;
        NewMaterialInstance = ( UMaterialInstanceConstant* )MaterialInstanceFactory->FactoryCreateNew(
            UMaterialInstanceConstant::StaticClass(), MaterialInstancePackage, FName( *MaterialInstanceName ),
            RF_Public | RF_Standalone, NULL, GWarn );

        if ( NewMaterialInstance )
            bNewMaterialCreated = true;

        MaterialInstanceFactory->RemoveFromRoot();

        /*
        // Creating a new material instance
        NewMaterialInstance = NewObject< UMaterialInstanceConstant >( MaterialInstancePackage, FName( *MaterialInstanceName ), RF_Public );
        checkf( NewMaterialInstance, TEXT( "Failed to create instanced material" ) );
        NewMaterialInstance->Parent = Material;

        // Notify registry that we have created a new duplicate material.
        FAssetRegistryModule::AssetCreated( NewMaterialInstance );

        // Dirty the material package.
        NewMaterialInstance->MarkPackageDirty();

        // Reset any derived state
        //MaterialInstance->ForceRecompileForRendering();
        */
    }

    if ( !OriginalMaterialInterface || !NewMaterialInstance )
        return false;

    // Update context for generated materials (will trigger when object goes out of scope).
    FMaterialUpdateContext MaterialUpdateContext;

    bool bModifiedMaterialParameters = false;
    // See if we need to override some of the material instance's parameters
    TArray< UGenericAttribute > AllMatParams;
    // Get the detail material parameters
    int ParamCount = FHoudiniEngineUtils::GetGenericAttributeList( HoudiniGeoPartObject, HAPI_UNREAL_ATTRIB_GENERIC_MAT_PARAM_PREFIX, AllMatParams, HAPI_ATTROWNER_DETAIL );
    // Then the primitive material parameters
    ParamCount += FHoudiniEngineUtils::GetGenericAttributeList( HoudiniGeoPartObject, HAPI_UNREAL_ATTRIB_GENERIC_MAT_PARAM_PREFIX, AllMatParams, HAPI_ATTROWNER_PRIM, MaterialIndexToAttributeIndex );
    for ( int32 ParamIdx = 0; ParamIdx < AllMatParams.Num(); ParamIdx++ )
    {
        // Try to update the material instance parameter corresponding to the attribute
        if ( UpdateMaterialInstanceParameter( AllMatParams[ ParamIdx ], NewMaterialInstance, CookParams ) )
            bModifiedMaterialParameters = true;
    }

    // Schedule this material for update if needed.
    if ( bNewMaterialCreated || bModifiedMaterialParameters )
        MaterialUpdateContext.AddMaterialInstance( NewMaterialInstance );

    if ( bNewMaterialCreated )
    {
        // Add meta information to this package.
        FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
            MaterialInstancePackage, NewMaterialInstance, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT( "true" ) );
        FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
            MaterialInstancePackage, NewMaterialInstance, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MaterialInstanceName );

        // Notify registry that we have created a new duplicate material.
        FAssetRegistryModule::AssetCreated( NewMaterialInstance );
    }

    if ( bNewMaterialCreated || bModifiedMaterialParameters )
    {
        // Dirty the material
        NewMaterialInstance->MarkPackageDirty();

        // Update the material instance
        NewMaterialInstance->InitStaticPermutation();
        NewMaterialInstance->PreEditChange(nullptr);
        NewMaterialInstance->PostEditChange();

        // Automatically save the package to avoid further issue
        MaterialInstancePackage->SetDirtyFlag( true );
        MaterialInstancePackage->FullyLoad();
        UPackage::SavePackage(
            MaterialInstancePackage, nullptr, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone,
            *FPackageName::LongPackageNameToFilename( MaterialInstancePackage->GetName(), FPackageName::GetAssetPackageExtension() ) );
    }

    // Update the return pointers
    CreatedMaterialInstance = NewMaterialInstance;

    return true;
#else
    return false;
#endif
}

bool
FHoudiniEngineMaterialUtils::UpdateMaterialInstanceParameter( UGenericAttribute MaterialParameter, UMaterialInstanceConstant* MaterialInstance, FHoudiniCookParams& CookParams )
{
#if WITH_EDITOR
    if ( !MaterialInstance )
        return false;

    if ( MaterialParameter.AttributeName.IsEmpty() )
        return false;

    // The default material instance parameters needs to be handled manually as they cant be changed via generic SetParameters functions
    if ( MaterialParameter.AttributeName.Compare( "CastShadowAsMasked", ESearchCase::IgnoreCase ) == 0 )
    {
        bool Value = MaterialParameter.GetBoolValue();

        // Update the parameter value only if necessary
        if ( MaterialInstance->GetOverrideCastShadowAsMasked() && ( MaterialInstance->GetCastShadowAsMasked() == Value ) )
            return false;

        MaterialInstance->SetOverrideCastShadowAsMasked( true );
        MaterialInstance->SetCastShadowAsMasked( Value );
        return true;
    }
    else  if ( MaterialParameter.AttributeName.Compare( "EmissiveBoost", ESearchCase::IgnoreCase ) == 0 )
    {
        float Value = (float)MaterialParameter.GetDoubleValue();

        // Update the parameter value only if necessary
        if ( MaterialInstance->GetOverrideEmissiveBoost() && ( MaterialInstance->GetEmissiveBoost() == Value ) )
            return false;

        MaterialInstance->SetOverrideEmissiveBoost( true );
        MaterialInstance->SetEmissiveBoost( Value );
        return true;
    }
    else if ( MaterialParameter.AttributeName.Compare( "DiffuseBoost", ESearchCase::IgnoreCase ) == 0 )
    {
        float Value = (float)MaterialParameter.GetDoubleValue();

        // Update the parameter value only if necessary
        if ( MaterialInstance->GetOverrideDiffuseBoost() && ( MaterialInstance->GetDiffuseBoost() == Value ) )
            return false;

        MaterialInstance->SetOverrideDiffuseBoost( true );
        MaterialInstance->SetDiffuseBoost( Value );
        return true;
    }
    else if ( MaterialParameter.AttributeName.Compare( "ExportResolutionScale", ESearchCase::IgnoreCase ) == 0 )
    {
        float Value = (float)MaterialParameter.GetDoubleValue();

        // Update the parameter value only if necessary
        if (MaterialInstance->GetOverrideExportResolutionScale() && ( MaterialInstance->GetExportResolutionScale() == Value ) )
            return false;

        MaterialInstance->SetOverrideExportResolutionScale( true );
        MaterialInstance->SetExportResolutionScale( Value );
        return true;
    }
    else if ( MaterialParameter.AttributeName.Compare( "OpacityMaskClipValue", ESearchCase::IgnoreCase ) == 0 )
    {
        float Value = (float)MaterialParameter.GetDoubleValue();

        // Update the parameter value only if necessary
        if ( MaterialInstance->BasePropertyOverrides.bOverride_OpacityMaskClipValue && ( MaterialInstance->BasePropertyOverrides.OpacityMaskClipValue == Value ) )
            return false;

        MaterialInstance->BasePropertyOverrides.bOverride_OpacityMaskClipValue = true;
        MaterialInstance->BasePropertyOverrides.OpacityMaskClipValue = Value;
        return true;
    }
    else if ( MaterialParameter.AttributeName.Compare( "BlendMode", ESearchCase::IgnoreCase ) == 0 )
    {
        EBlendMode EnumValue = (EBlendMode)MaterialParameter.GetIntValue();
        if ( MaterialParameter.AttributeType == HAPI_STORAGETYPE_STRING )
        {
            FString StringValue = MaterialParameter.GetStringValue();
            if ( StringValue.Compare( "Opaque", ESearchCase::IgnoreCase ) == 0 )
                EnumValue = EBlendMode::BLEND_Opaque;
            else if ( StringValue.Compare( "Masked", ESearchCase::IgnoreCase ) == 0 )
                EnumValue = EBlendMode::BLEND_Masked;
            else if ( StringValue.Compare( "Translucent", ESearchCase::IgnoreCase ) == 0 )
                EnumValue = EBlendMode::BLEND_Translucent;
            else if ( StringValue.Compare( "Additive", ESearchCase::IgnoreCase ) == 0 )
                EnumValue = EBlendMode::BLEND_Additive;
            else if ( StringValue.Compare( "Modulate", ESearchCase::IgnoreCase ) == 0 )
                EnumValue = EBlendMode::BLEND_Modulate;
            else if ( StringValue.StartsWith( "Alpha", ESearchCase::IgnoreCase ) )
                EnumValue = EBlendMode::BLEND_AlphaComposite;
        }

        // Update the parameter value only if necessary
        if ( MaterialInstance->BasePropertyOverrides.bOverride_BlendMode && ( MaterialInstance->BasePropertyOverrides.BlendMode == EnumValue ) )
            return false;

        MaterialInstance->BasePropertyOverrides.bOverride_BlendMode = true;
        MaterialInstance->BasePropertyOverrides.BlendMode = EnumValue;
        return true;
    }
    else if ( MaterialParameter.AttributeName.Compare( "ShadingModel", ESearchCase::IgnoreCase ) == 0 )
    {
        EMaterialShadingModel EnumValue = (EMaterialShadingModel)MaterialParameter.GetIntValue();
        if ( MaterialParameter.AttributeType == HAPI_STORAGETYPE_STRING )
        {
            FString StringValue = MaterialParameter.GetStringValue();
            if ( StringValue.Compare( "Unlit", ESearchCase::IgnoreCase ) == 0 )
                EnumValue = EMaterialShadingModel::MSM_Unlit;
            else if ( StringValue.StartsWith( "Default", ESearchCase::IgnoreCase ) )
                EnumValue = EMaterialShadingModel::MSM_DefaultLit;
            else if ( StringValue.Compare( "Subsurface", ESearchCase::IgnoreCase ) == 0 )
                EnumValue = EMaterialShadingModel::MSM_Subsurface;
            else if ( StringValue.StartsWith( "Preintegrated", ESearchCase::IgnoreCase ) )
                EnumValue = EMaterialShadingModel::MSM_PreintegratedSkin;
            else if ( StringValue.StartsWith( "Clear", ESearchCase::IgnoreCase ) )
                EnumValue = EMaterialShadingModel::MSM_ClearCoat;
            else if ( StringValue.Compare( "SubsurfaceProfile", ESearchCase::IgnoreCase ) == 0 )
                EnumValue = EMaterialShadingModel::MSM_SubsurfaceProfile;
            else if ( StringValue.Compare( "TwoSidedFoliage", ESearchCase::IgnoreCase ) == 0 )
                EnumValue = EMaterialShadingModel::MSM_TwoSidedFoliage;
            else if ( StringValue.Compare( "Hair", ESearchCase::IgnoreCase ) == 0 )
                EnumValue = EMaterialShadingModel::MSM_Hair;
            else if ( StringValue.Compare( "Cloth", ESearchCase::IgnoreCase ) == 0 )
                EnumValue = EMaterialShadingModel::MSM_Cloth;
            else if ( StringValue.Compare( "Eye", ESearchCase::IgnoreCase ) == 0 )
                EnumValue = EMaterialShadingModel::MSM_Eye;
        }

        // Update the parameter value only if necessary
        if ( MaterialInstance->BasePropertyOverrides.bOverride_ShadingModel && ( MaterialInstance->BasePropertyOverrides.ShadingModel == EnumValue ) )
            return false;

        MaterialInstance->BasePropertyOverrides.bOverride_ShadingModel = true;
        MaterialInstance->BasePropertyOverrides.ShadingModel = EnumValue;
        return true;
    }
    else if ( MaterialParameter.AttributeName.Compare( "TwoSided", ESearchCase::IgnoreCase ) == 0 )
    {
        bool Value = MaterialParameter.GetBoolValue();

        // Update the parameter value only if necessary
        if ( MaterialInstance->BasePropertyOverrides.bOverride_TwoSided && ( MaterialInstance->BasePropertyOverrides.TwoSided == Value ) )
            return false;

        MaterialInstance->BasePropertyOverrides.bOverride_TwoSided = true;
        MaterialInstance->BasePropertyOverrides.TwoSided = Value;
        return true;
    }
    else if ( MaterialParameter.AttributeName.Compare( "DitheredLODTransition", ESearchCase::IgnoreCase ) == 0 )
    {
        bool Value = MaterialParameter.GetBoolValue();

        // Update the parameter value only if necessary
        if ( MaterialInstance->BasePropertyOverrides.bOverride_DitheredLODTransition && ( MaterialInstance->BasePropertyOverrides.DitheredLODTransition == Value ) )
            return false;

        MaterialInstance->BasePropertyOverrides.bOverride_DitheredLODTransition = true;
        MaterialInstance->BasePropertyOverrides.DitheredLODTransition = Value;
        return true;
    }
    else if ( MaterialParameter.AttributeName.Compare( "PhysMaterial", ESearchCase::IgnoreCase ) == 0 )
    {
        // Try to load a Material corresponding to the parameter value
        FString ParamValue = MaterialParameter.GetStringValue();
        UPhysicalMaterial* FoundPhysMaterial = Cast< UPhysicalMaterial >(
            StaticLoadObject( UPhysicalMaterial::StaticClass(), nullptr, *ParamValue, nullptr, LOAD_NoWarn, nullptr ) );

        // Update the parameter value if necessary
        if ( FoundPhysMaterial && ( MaterialInstance->PhysMaterial != FoundPhysMaterial ) )
        {
            MaterialInstance->PhysMaterial = FoundPhysMaterial;
            return true;
        }

        return false;
    }

    // Handling custom parameters
    FName CurrentMatParamName = FName( *MaterialParameter.AttributeName );
    if ( MaterialParameter.AttributeType == HAPI_STORAGETYPE_STRING )
    {
        // String attributes are used for textures parameters
        // We need to find the texture corresponding to the param
        UTexture* FoundTexture = nullptr;
        FString ParamValue = MaterialParameter.GetStringValue();

        // Texture can either be already existing texture assets in UE4, or a newly generated textures by this asset
        // Try to find the texture corresponding to the param value in the existing assets first.
        FoundTexture = Cast< UTexture >(
            StaticLoadObject( UTexture::StaticClass(), nullptr, *ParamValue, nullptr, LOAD_NoWarn, nullptr ) );

        if ( !FoundTexture )
        {
            // We couldn't find a texture corresponding to the parameter in the existing UE4 assets
            // Try to find the corresponding texture in the cooked temporary package we just generated
            FoundTexture = FHoudiniEngineMaterialUtils::FindGeneratedTexture( ParamValue, CookParams );
        }

        // Do not update if unnecessary

        if ( FoundTexture )
        {
            // Do not update if unnecessary
            UTexture* OldTexture = nullptr;
            bool FoundOldParam = MaterialInstance->GetTextureParameterValue( CurrentMatParamName, OldTexture );
            if ( FoundOldParam && ( OldTexture == FoundTexture ) )
                return false;

            MaterialInstance->SetTextureParameterValueEditorOnly( CurrentMatParamName, FoundTexture );
            return true;
        }
    }
    else if ( MaterialParameter.AttributeTupleSize == 1 )
    {
        // Single attributes are for scalar parameters
        float NewValue = (float)MaterialParameter.GetDoubleValue();

        // Do not update if unnecessary
        float OldValue;
        bool FoundOldParam = MaterialInstance->GetScalarParameterValue( CurrentMatParamName, OldValue );
        if ( FoundOldParam && ( OldValue == NewValue ) )
            return false;

        MaterialInstance->SetScalarParameterValueEditorOnly( CurrentMatParamName, NewValue );
        return true;
    }
    else
    {
        // Tuple attributes are for vector parameters
        FLinearColor NewLinearColor;
        // if the attribute is stored in an int, we'll have to convert a color to a linear color
        if ( MaterialParameter.AttributeType == HAPI_STORAGETYPE_INT || MaterialParameter.AttributeType == HAPI_STORAGETYPE_INT64 )
        {
            FColor IntColor;
            IntColor.R = (int8)MaterialParameter.GetIntValue( 0 );
            IntColor.G = (int8)MaterialParameter.GetIntValue( 1 );
            IntColor.B = (int8)MaterialParameter.GetIntValue( 2 );
            if ( MaterialParameter.AttributeTupleSize >= 4 )
                IntColor.A = (int8)MaterialParameter.GetIntValue( 3 );
            else
                IntColor.A = 1;

            NewLinearColor = FLinearColor( IntColor );
        }
        else
        {
            NewLinearColor.R = (float)MaterialParameter.GetDoubleValue( 0 );
            NewLinearColor.G = (float)MaterialParameter.GetDoubleValue( 1 );
            NewLinearColor.B = (float)MaterialParameter.GetDoubleValue( 2 );
            if ( MaterialParameter.AttributeTupleSize >= 4 )
                NewLinearColor.A = (float)MaterialParameter.GetDoubleValue( 3 );
        }

        // Do not update if unnecessary
        FLinearColor OldValue;
        bool FoundOldParam = MaterialInstance->GetVectorParameterValue( CurrentMatParamName, OldValue );
        if ( FoundOldParam && ( OldValue == NewLinearColor ) )
            return false;

        MaterialInstance->SetVectorParameterValueEditorOnly( CurrentMatParamName, NewLinearColor );
        return true;
    }
#endif

    return false;
}

UTexture*
FHoudiniEngineMaterialUtils::FindGeneratedTexture( const FString& TextureString, FHoudiniCookParams& CookParams )
{    
    if ( TextureString.IsEmpty() )
        return nullptr;

    // Try to find the corresponding texture in the cooked temporary package generated by an HDA
    UTexture* FoundTexture = nullptr;
    for ( TMap<FString, TWeakObjectPtr< UPackage > >::TIterator IterPackage( *(CookParams.CookedTemporaryPackages) ); IterPackage; ++IterPackage )
    {
        // Iterate through the cooked packages
        UPackage * CurrentPackage = IterPackage.Value().Get();
        if ( !CurrentPackage )
            continue;

        // First, check if the package contains a texture
        FString CurrentPackageName = CurrentPackage->GetName();
        UTexture* PackageTexture = LoadObject< UTexture >( CurrentPackage, *CurrentPackageName, nullptr, LOAD_None, nullptr );
        if ( !PackageTexture )
            continue;

        // Then check if the package's metadata match what we're looking for
        // Make sure this texture was generated by Houdini Engine
        UMetaData * MetaData = CurrentPackage->GetMetaData();
        if ( !MetaData || !MetaData->HasValue( PackageTexture, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT ) )
            continue;

        // Get the texture type from the meta data
        // Texture type store has meta data will be C_A, N, S, R etc..
        const FString TextureTypeString = MetaData->GetValue( PackageTexture, HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE );
        if ( TextureTypeString.Compare( TextureString, ESearchCase::IgnoreCase ) == 0 )
        {
            FoundTexture = PackageTexture;
            break;
        }

        // Convert the texture type to a "friendly" version
        // C_A to diffuse, N to Normal, S to Specular etc...
        FString TextureTypeFriendlyString = TextureTypeString;
        if ( TextureTypeString.Compare( HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_DIFFUSE, ESearchCase::IgnoreCase ) == 0 )
            TextureTypeFriendlyString = TEXT( "diffuse" );
        else if ( TextureTypeString.Compare( HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_NORMAL, ESearchCase::IgnoreCase) == 0 )
            TextureTypeFriendlyString = TEXT( "normal" );
        else if ( TextureTypeString.Compare( HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_EMISSIVE, ESearchCase::IgnoreCase) == 0 )
            TextureTypeFriendlyString = TEXT( "emissive" );
        else if ( TextureTypeString.Compare( HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_SPECULAR, ESearchCase::IgnoreCase) == 0 )
            TextureTypeFriendlyString = TEXT( "specular" );
        else if ( TextureTypeString.Compare( HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_ROUGHNESS, ESearchCase::IgnoreCase) == 0 )
            TextureTypeFriendlyString = TEXT( "roughness" );
        else if ( TextureTypeString.Compare( HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_METALLIC, ESearchCase::IgnoreCase) == 0 )
            TextureTypeFriendlyString = TEXT( "metallic" );
        else if ( TextureTypeString.Compare( HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_OPACITY_MASK, ESearchCase::IgnoreCase) == 0 )
            TextureTypeFriendlyString = TEXT( "opacity" );

        // See if we have a match between the texture string and the friendly name
        if ( TextureTypeFriendlyString.Compare( TextureString, ESearchCase::IgnoreCase ) == 0 )
        {
            FoundTexture = PackageTexture;
            break;
        }

        // Get the node path from the meta data
        const FString NodePath = MetaData->GetValue( PackageTexture, HAPI_UNREAL_PACKAGE_META_NODE_PATH );
        if ( NodePath.IsEmpty() )
            continue;

        // See if we have a match with the path and texture type
        FString PathAndType = NodePath + TEXT("/") + TextureTypeString;
        if ( PathAndType.Compare( TextureString, ESearchCase::IgnoreCase ) == 0 )
        {
            FoundTexture = PackageTexture;
            break;
        }

        // See if we have a match with the friendly path and texture type
        FString PathAndFriendlyType = NodePath + TEXT("/") + TextureTypeFriendlyString;
        if ( PathAndFriendlyType.Compare( TextureString, ESearchCase::IgnoreCase ) == 0 )
        {
            FoundTexture = PackageTexture;
            break;
        }
    }

    return FoundTexture;
}
