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
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniAsset.h"
#include "HoudiniEngineString.h"
#include "HoudiniInstancedActorComponent.h"
#include "HoudiniMeshSplitInstancerComponent.h"

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/Material.h"

#if WITH_EDITOR
    #include "ActorFactories/ActorFactory.h"
    #include "Editor.h"
    #include "Factories/MaterialFactoryNew.h"
    #include "ActorFactories/ActorFactoryStaticMesh.h"
    #include "Interfaces/ITargetPlatform.h"
    #include "Interfaces/ITargetPlatformManagerModule.h"
    #include "FileHelpers.h"
    #include "Editor/UnrealEd/Public/Layers/ILayers.h"
    #include "Materials/Material.h"
    #include "Materials/MaterialInstance.h"
    #include "Materials/MaterialExpressionTextureSample.h"
    #include "Materials/MaterialExpressionTextureCoordinate.h"
    #include "StaticMeshResources.h"
#endif
#include "EngineUtils.h"
#include "UObject/MetaData.h"
#include "PhysicsEngine/BodySetup.h"
#include "Components/InstancedStaticMeshComponent.h"

#if PLATFORM_WINDOWS
    #include "Windows/WindowsHWrapper.h"

    // Of course, Windows defines its own GetGeoInfo,
    // So we need to undefine that before including HoudiniApi.h to avoid collision...
    #ifdef GetGeoInfo
        #undef GetGeoInfo
    #endif
#endif

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

UPackage *
FHoudiniEngineBakeUtils::BakeCreateBlueprintPackageForComponent(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    FString & BlueprintName )
{
    UPackage * Package = nullptr;

#if WITH_EDITOR

    FString HoudiniAssetName;
    if ( HoudiniAssetComponent->HoudiniAsset )
        HoudiniAssetName = HoudiniAssetComponent->HoudiniAsset->GetName();
    else if ( HoudiniAssetComponent->GetOuter() )
        HoudiniAssetName = HoudiniAssetComponent->GetOuter()->GetName();
    else
        HoudiniAssetName = HoudiniAssetComponent->GetName();

    FGuid BakeGUID = FGuid::NewGuid();

    if( !BakeGUID.IsValid() )
        BakeGUID = FGuid::NewGuid();

    // We only want half of generated guid string.
    FString BakeGUIDString = BakeGUID.ToString().Left( FHoudiniEngineUtils::PackageGUIDItemNameLength );

    // Generate Blueprint name.
    BlueprintName = HoudiniAssetName + TEXT( "_" ) + BakeGUIDString;

    // Generate unique package name.
    FString PackageName = HoudiniAssetComponent->GetBakeFolder().ToString() + TEXT( "/" ) + BlueprintName;

    PackageName = PackageTools::SanitizePackageName( PackageName );

    // See if package exists, if it does, we need to regenerate the name.
    Package = FindPackage( nullptr, *PackageName );

    if( Package )
    {
        // Package does exist, there's a collision, we need to generate a new name.
        BakeGUID.Invalidate();
    }
    else
    {
        // Create actual package.
        Package = CreatePackage( nullptr, *PackageName );
    }
#endif

    return Package;
}

UStaticMesh *
FHoudiniEngineBakeUtils::BakeStaticMesh(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    const FHoudiniGeoPartObject & HoudiniGeoPartObject,
    UStaticMesh * InStaticMesh )
{
    UStaticMesh * StaticMesh = nullptr;

#if WITH_EDITOR

    UHoudiniAsset * HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
    check( HoudiniAsset );

    // We cannot bake curves.
    if( HoudiniGeoPartObject.IsCurve() )
        return nullptr;

    if( HoudiniGeoPartObject.IsInstancer() )
    {
        HOUDINI_LOG_MESSAGE( TEXT( "Baking of instanced static meshes is not supported at the moment." ) );
        return nullptr;
    }

    // Get platform manager LOD specific information.
    ITargetPlatform * CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
    check( CurrentPlatform );
    const FStaticMeshLODGroup & LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup( NAME_None );
    int32 NumLODs = LODGroup.GetDefaultNumLODs();

    // Get runtime settings.
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    check( HoudiniRuntimeSettings );

    FHoudiniCookParams HoudiniCookParams( HoudiniAssetComponent );
    HoudiniCookParams.StaticMeshBakeMode = EBakeMode::CreateNewAssets;
    FString MeshName;
    FGuid BakeGUID;
    UPackage * Package = FHoudiniEngineBakeUtils::BakeCreateStaticMeshPackageForComponent(
        HoudiniCookParams, HoudiniGeoPartObject, MeshName, BakeGUID );

    if( !Package )
    {
        return nullptr;
    }

    // Create static mesh.
    StaticMesh = NewObject< UStaticMesh >( Package, FName( *MeshName ), RF_Public | RF_Transactional );

    // Add meta information to this package.
    FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
        Package, StaticMesh, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT( "true" ) );
    FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
        Package, StaticMesh, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MeshName );

    // Notify registry that we created a new asset.
    FAssetRegistryModule::AssetCreated( StaticMesh );

    // Copy materials.
    StaticMesh->StaticMaterials = InStaticMesh->StaticMaterials;

    // Create new source model for current static mesh.
    if( !StaticMesh->SourceModels.Num() )
        new ( StaticMesh->SourceModels ) FStaticMeshSourceModel();

    FStaticMeshSourceModel * SrcModel = &StaticMesh->SourceModels[0];
    FRawMeshBulkData * RawMeshBulkData = SrcModel->RawMeshBulkData;

    // Load raw data bytes.
    FRawMesh RawMesh;
    FStaticMeshSourceModel * InSrcModel = &InStaticMesh->SourceModels[0];
    FRawMeshBulkData * InRawMeshBulkData = InSrcModel->RawMeshBulkData;
    InRawMeshBulkData->LoadRawMesh( RawMesh );

    // Some mesh generation settings.
    HoudiniRuntimeSettings->SetMeshBuildSettings( SrcModel->BuildSettings, RawMesh );

    // Setting the DistanceField resolution
    SrcModel->BuildSettings.DistanceFieldResolutionScale = HoudiniAssetComponent->GeneratedDistanceFieldResolutionScale;

    // We need to check light map uv set for correctness. Unreal seems to have occasional issues with
    // zero UV sets when building lightmaps.
    if( SrcModel->BuildSettings.bGenerateLightmapUVs )
    {
        // See if we need to disable lightmap generation because of bad UVs.
        if( FHoudiniEngineUtils::ContainsInvalidLightmapFaces( RawMesh, StaticMesh->LightMapCoordinateIndex ) )
        {
            SrcModel->BuildSettings.bGenerateLightmapUVs = false;

            HOUDINI_LOG_MESSAGE(
                TEXT( "Skipping Lightmap Generation: Object %s " )
                TEXT( "- skipping." ),
                *MeshName );
        }
    }

    // Store the new raw mesh.
    RawMeshBulkData->SaveRawMesh( RawMesh );

    while( StaticMesh->SourceModels.Num() < NumLODs )
        new ( StaticMesh->SourceModels ) FStaticMeshSourceModel();

    for( int32 ModelLODIndex = 0; ModelLODIndex < NumLODs; ++ModelLODIndex )
    {
        StaticMesh->SourceModels[ModelLODIndex].ReductionSettings = LODGroup.GetDefaultSettings( ModelLODIndex );

        for( int32 MaterialIndex = 0; MaterialIndex < StaticMesh->StaticMaterials.Num(); ++MaterialIndex )
        {
            FMeshSectionInfo Info = StaticMesh->SectionInfoMap.Get( ModelLODIndex, MaterialIndex );
            Info.MaterialIndex = MaterialIndex;
            Info.bEnableCollision = true;
            Info.bCastShadow = true;
            StaticMesh->SectionInfoMap.Set( ModelLODIndex, MaterialIndex, Info );
        }
    }

    // Assign generation parameters for this static mesh.
    HoudiniAssetComponent->SetStaticMeshGenerationParameters( StaticMesh );

    // Copy custom lightmap resolution if it is set.
    if( InStaticMesh->LightMapResolution != StaticMesh->LightMapResolution )
        StaticMesh->LightMapResolution = InStaticMesh->LightMapResolution;

    if( HoudiniGeoPartObject.IsCollidable() || HoudiniGeoPartObject.IsRenderCollidable() )
    {
        UBodySetup * BodySetup = StaticMesh->BodySetup;
        check( BodySetup );

        // Enable collisions for this static mesh.
        BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
    }

    FHoudiniScopedGlobalSilence ScopedGlobalSilence;
    StaticMesh->Build( true );
    StaticMesh->MarkPackageDirty();

#endif

    return StaticMesh;
}

UBlueprint *
FHoudiniEngineBakeUtils::BakeBlueprint( UHoudiniAssetComponent * HoudiniAssetComponent )
{
    UBlueprint * Blueprint = nullptr;

#if WITH_EDITOR

    // Create package for our Blueprint.
    FString BlueprintName = TEXT( "" );
    UPackage * Package = FHoudiniEngineBakeUtils::BakeCreateBlueprintPackageForComponent(
        HoudiniAssetComponent, BlueprintName );

    if( Package )
    {
        AActor * Actor = HoudiniAssetComponent->CloneComponentsAndCreateActor();
        if( Actor )
        {
            Blueprint = FKismetEditorUtilities::CreateBlueprintFromActor( *BlueprintName, Package, Actor, false );

            // If actor is rooted, unroot it. We can also delete intermediate actor.
            Actor->RemoveFromRoot();
            Actor->ConditionalBeginDestroy();

            if( Blueprint )
                FAssetRegistryModule::AssetCreated( Blueprint );
        }
    }

#endif

    return Blueprint;
}

AActor *
FHoudiniEngineBakeUtils::ReplaceHoudiniActorWithBlueprint( UHoudiniAssetComponent * HoudiniAssetComponent )
{
    AActor * Actor = nullptr;

#if WITH_EDITOR

    // Create package for our Blueprint.
    FString BlueprintName = TEXT( "" );
    UPackage * Package = FHoudiniEngineBakeUtils::BakeCreateBlueprintPackageForComponent( HoudiniAssetComponent, BlueprintName );

    if( Package )
    {
        //Bake the asset's landscape
        BakeLandscape( HoudiniAssetComponent );

        AActor * ClonedActor = HoudiniAssetComponent->CloneComponentsAndCreateActor();
        if( ClonedActor )
        {
            UBlueprint * Blueprint = FKismetEditorUtilities::CreateBlueprint(
                ClonedActor->GetClass(), Package, *BlueprintName,
                EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(),
                UBlueprintGeneratedClass::StaticClass(), FName( "CreateFromActor" ) );

            if( Blueprint )
            {
                Package->MarkPackageDirty();

                if( ClonedActor->GetInstanceComponents().Num() > 0 )
                    FKismetEditorUtilities::AddComponentsToBlueprint( Blueprint, ClonedActor->GetInstanceComponents() );

                if( Blueprint->GeneratedClass )
                {
                    AActor * CDO = CastChecked< AActor >( Blueprint->GeneratedClass->GetDefaultObject() );

                    const auto CopyOptions = ( EditorUtilities::ECopyOptions::Type )
                        ( EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties |
                            EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances );

                    EditorUtilities::CopyActorProperties( ClonedActor, CDO, CopyOptions );

                    USceneComponent * Scene = CDO->GetRootComponent();
                    if( Scene )
                    {
                        Scene->RelativeLocation = FVector::ZeroVector;
                        Scene->RelativeRotation = FRotator::ZeroRotator;

                        // Clear out the attachment info after having copied the properties from the source actor
                        Scene->DetachFromComponent( FDetachmentTransformRules::KeepRelativeTransform );
                        while( true )
                        {
                            const int32 ChildCount = Scene->GetAttachChildren().Num();
                            if( ChildCount <= 0 )
                                break;

                            USceneComponent * Component = Scene->GetAttachChildren()[ChildCount - 1];
                            Component->DetachFromComponent( FDetachmentTransformRules::KeepRelativeTransform );
                        }
                        check( Scene->GetAttachChildren().Num() == 0 );

                        // Ensure the light mass information is cleaned up
                        Scene->InvalidateLightingCache();
                    }
                }

                // Compile our blueprint and notify asset system about blueprint.
                FKismetEditorUtilities::CompileBlueprint( Blueprint );
                FAssetRegistryModule::AssetCreated( Blueprint );

                // Retrieve actor transform.
                FVector Location = ClonedActor->GetActorLocation();
                FRotator Rotator = ClonedActor->GetActorRotation();

                // Replace cloned actor with Blueprint instance.
                {
                    TArray< AActor * > Actors;
                    Actors.Add( ClonedActor );

                    ClonedActor->RemoveFromRoot();
                    Actor = FKismetEditorUtilities::CreateBlueprintInstanceFromSelection( Blueprint, Actors, Location, Rotator );
                }

                // We can initiate Houdini actor deletion.
                AHoudiniAssetActor * HoudiniAssetActor = HoudiniAssetComponent->GetHoudiniAssetActorOwner();

                // Remove Houdini actor from active selection in editor and delete it.
                if( GEditor )
                {
                    GEditor->SelectActor( HoudiniAssetActor, false, false );
                    GEditor->Layers->DisassociateActorFromLayers( HoudiniAssetActor );
                }

                UWorld * World = HoudiniAssetActor->GetWorld();
                if ( !World )
                    World = GWorld;

                World->EditorDestroyActor( HoudiniAssetActor, false );
            }
            else
            {
                ClonedActor->RemoveFromRoot();
                ClonedActor->ConditionalBeginDestroy();
            }
        }
    }

#endif

    return Actor;
}

UStaticMesh *
FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackage(
    const UStaticMesh * StaticMesh, UHoudiniAssetComponent * Component,
    const FHoudiniGeoPartObject & HoudiniGeoPartObject, EBakeMode BakeMode )
{
    UStaticMesh * DuplicatedStaticMesh = nullptr;
#if WITH_EDITOR
    if( !HoudiniGeoPartObject.IsCurve() && !HoudiniGeoPartObject.IsInstancer() && !HoudiniGeoPartObject.IsPackedPrimitiveInstancer() && !HoudiniGeoPartObject.IsVolume() )
    {
        // Create package for this duplicated mesh.
        FHoudiniCookParams HoudiniCookParams( Component );
        // Transferring the CookMode to the materials
        // We're either using the default one, or a custom one
        HoudiniCookParams.StaticMeshBakeMode = BakeMode;
        if( BakeMode == FHoudiniCookParams::GetDefaultStaticMeshesCookMode() )
            HoudiniCookParams.MaterialAndTextureBakeMode = FHoudiniCookParams::GetDefaultMaterialAndTextureCookMode();
        else
            HoudiniCookParams.MaterialAndTextureBakeMode = BakeMode;

        FString MeshName;
        FGuid MeshGuid;

        UPackage * MeshPackage = FHoudiniEngineBakeUtils::BakeCreateStaticMeshPackageForComponent(
            HoudiniCookParams, HoudiniGeoPartObject, MeshName, MeshGuid );
        if( !MeshPackage )
            return nullptr;

        // Duplicate mesh for this new copied component.
        DuplicatedStaticMesh = DuplicateObject< UStaticMesh >( StaticMesh, MeshPackage, *MeshName );

        if( BakeMode != EBakeMode::Intermediate )
            DuplicatedStaticMesh->SetFlags( RF_Public | RF_Standalone );

        // Add meta information.
        FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
            MeshPackage, DuplicatedStaticMesh,
            HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT( "true" ) );
        FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
            MeshPackage, DuplicatedStaticMesh,
            HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MeshName );

        // See if we need to duplicate materials and textures.
        TArray< FStaticMaterial > DuplicatedMaterials;
        TArray< FStaticMaterial > & Materials = DuplicatedStaticMesh->StaticMaterials;

        for( int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); ++MaterialIdx )
        {
            UMaterialInterface* MaterialInterface = Materials[MaterialIdx].MaterialInterface;
            if( MaterialInterface )
            {
                UPackage * MaterialPackage = Cast< UPackage >( MaterialInterface->GetOuter() );
                if( MaterialPackage )
                {
                    FString MaterialName;
                    if( FHoudiniEngineBakeUtils::GetHoudiniGeneratedNameFromMetaInformation(
                        MaterialPackage, MaterialInterface, MaterialName ) )
                    {
                        // We only deal with materials.
                        UMaterial * Material = Cast< UMaterial >( MaterialInterface );
                        if( Material )
                        {
                            // Duplicate material resource.
                            UMaterial * DuplicatedMaterial = FHoudiniEngineBakeUtils::DuplicateMaterialAndCreatePackage(
                                Material, HoudiniCookParams, MaterialName );

                            if( !DuplicatedMaterial )
                                continue;

                            // Store duplicated material.
                            FStaticMaterial DupeStaticMaterial = Materials[MaterialIdx];
                            DupeStaticMaterial.MaterialInterface = DuplicatedMaterial;
                            DuplicatedMaterials.Add( DupeStaticMaterial );
                            continue;
                        }
                    }
                }
            }

            DuplicatedMaterials.Add( Materials[MaterialIdx] );
        }

        // Assign duplicated materials.
        DuplicatedStaticMesh->StaticMaterials = DuplicatedMaterials;

        // Notify registry that we have created a new duplicate mesh.
        FAssetRegistryModule::AssetCreated( DuplicatedStaticMesh );

        // Dirty the static mesh package.
        DuplicatedStaticMesh->MarkPackageDirty();
    }
#endif
    return DuplicatedStaticMesh;
}

void
FHoudiniEngineBakeUtils::BakeHoudiniActorToActors( UHoudiniAssetComponent * HoudiniAssetComponent, bool SelectNewActors )
{
#if WITH_EDITOR
    const FScopedTransaction Transaction( LOCTEXT( "BakeToActors", "Bake To Actors" ) );

    auto SMComponentToPart = HoudiniAssetComponent->CollectAllStaticMeshComponents();
    TArray< AActor* > NewActors = BakeHoudiniActorToActors_StaticMeshes( HoudiniAssetComponent, SMComponentToPart );

    auto IAComponentToPart = HoudiniAssetComponent->CollectAllInstancedActorComponents();
    NewActors.Append( BakeHoudiniActorToActors_InstancedActors( HoudiniAssetComponent, IAComponentToPart ) );

    auto SplitMeshInstancerComponentToPart = HoudiniAssetComponent->CollectAllMeshSplitInstancerComponents();
    NewActors.Append( BakeHoudiniActorToActors_SplitMeshInstancers( HoudiniAssetComponent, SplitMeshInstancerComponentToPart ) );

    if( SelectNewActors && NewActors.Num() )
    {
        GEditor->SelectNone( false, true );
        for( AActor* NewActor : NewActors )
        {
            GEditor->SelectActor( NewActor, true, false );
        }
        GEditor->NoteSelectionChange();
    }
#endif
}

TArray< AActor* >
FHoudiniEngineBakeUtils::BakeHoudiniActorToActors_InstancedActors(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    TMap< const UHoudiniInstancedActorComponent*, FHoudiniGeoPartObject >& ComponentToPart )
{
    TArray< AActor* > NewActors;
#if WITH_EDITOR
    ULevel* DesiredLevel = GWorld->GetCurrentLevel();
    FName BaseName( *( HoudiniAssetComponent->GetOwner()->GetName() + TEXT( "_Baked" ) ) );

    for( const auto& Iter : ComponentToPart )
    {
        const UHoudiniInstancedActorComponent * OtherSMC = Iter.Key;
        for( AActor* InstActor : OtherSMC->Instances )
        {
            FName NewName = MakeUniqueObjectName( DesiredLevel, OtherSMC->InstancedAsset->StaticClass(), BaseName );
            FString NewNameStr = NewName.ToString();

            FTransform CurrentTransform = InstActor->GetTransform();
            if( AActor* NewActor = OtherSMC->SpawnInstancedActor( CurrentTransform ) )
            {
                NewActor->SetActorLabel( NewNameStr );
                NewActor->SetFolderPath( BaseName );
                NewActor->SetActorTransform( CurrentTransform );
            }
        }
    }
#endif
    return NewActors;
}

void
FHoudiniEngineBakeUtils::CheckedBakeStaticMesh(
    UHoudiniAssetComponent* HoudiniAssetComponent,
    TMap< const UStaticMesh*, UStaticMesh* >& OriginalToBakedMesh,
    const FHoudiniGeoPartObject & HoudiniGeoPartObject, UStaticMesh* OriginalSM)
{
    UStaticMesh* BakedSM = nullptr;
    if( UStaticMesh ** FoundMeshPtr = OriginalToBakedMesh.Find(OriginalSM) )
    {
        // We've already baked this mesh, use it
        BakedSM = *FoundMeshPtr;
    }
    else
    {
        if( FHoudiniEngineBakeUtils::StaticMeshRequiresBake(OriginalSM) )
        {
            // Bake the found mesh into the project
            BakedSM = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackage(
                OriginalSM, HoudiniAssetComponent, HoudiniGeoPartObject, EBakeMode::CreateNewAssets);

            if( ensure(BakedSM) )
            {
                FAssetRegistryModule::AssetCreated(BakedSM);
            }
        }
        else
        {
            // We didn't bake this mesh, but it's already baked so we will just use it as is
            BakedSM = OriginalSM;
        }
        OriginalToBakedMesh.Add(OriginalSM, BakedSM);
    }
}

TArray< AActor* >
FHoudiniEngineBakeUtils::BakeHoudiniActorToActors_StaticMeshes(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    TMap< const UStaticMeshComponent*, FHoudiniGeoPartObject >& SMComponentToPart )
{
    TMap< const UStaticMesh*, UStaticMesh* > OriginalToBakedMesh;

    // Loop over all comps, bake static mesh if not already baked, and create an actor for every one of them
    TArray< AActor* > NewActors;
#if WITH_EDITOR
    for( const auto& Iter : SMComponentToPart )
    {
        const FHoudiniGeoPartObject & HoudiniGeoPartObject = Iter.Value;
        const UStaticMeshComponent * OtherSMC = Iter.Key;

        UStaticMesh * OtherSM = OtherSMC->GetStaticMesh();

        if( !ensure( OtherSM ) )
            continue;

        CheckedBakeStaticMesh(HoudiniAssetComponent, OriginalToBakedMesh, HoudiniGeoPartObject, OtherSM);
    }

    // Finished baking, now spawn the actors

    for( const auto& Iter : SMComponentToPart )
    {
        const UStaticMeshComponent * OtherSMC = Iter.Key;
        const FHoudiniGeoPartObject & HoudiniGeoPartObject = Iter.Value;
        UStaticMesh* BakedSM = OriginalToBakedMesh[OtherSMC->GetStaticMesh()];

        if( ensure( BakedSM ) )
        {
            ULevel* DesiredLevel = GWorld->GetCurrentLevel();
            FName BaseName( *( HoudiniAssetComponent->GetOwner()->GetName() + TEXT( "_Baked" ) ) );
            UActorFactory* Factory = GEditor->FindActorFactoryByClass( UActorFactoryStaticMesh::StaticClass() );

            auto PrepNewStaticMeshActor = [&]( AActor* NewActor )
            {
                // The default name will be based on the static mesh package, we would prefer it to be based on the Houdini asset
                FName NewName = MakeUniqueObjectName( DesiredLevel, Factory->NewActorClass, BaseName );
                FString NewNameStr = NewName.ToString();
                NewActor->Rename( *NewNameStr );
                NewActor->SetActorLabel( NewNameStr );
                NewActor->SetFolderPath( BaseName );

                // Copy properties to new actor
                if( AStaticMeshActor* SMActor = Cast< AStaticMeshActor>( NewActor ) )
                {
                    if( UStaticMeshComponent* SMC = SMActor->GetStaticMeshComponent() )
                    {
                        UStaticMeshComponent* OtherSMC_NonConst = const_cast<UStaticMeshComponent*>( OtherSMC );

                        SMC->SetCollisionProfileName( OtherSMC_NonConst->GetCollisionProfileName() );
                        SMC->SetCollisionEnabled( OtherSMC->GetCollisionEnabled() );
                        SMC->LightmassSettings = OtherSMC->LightmassSettings;
                        SMC->CastShadow = OtherSMC->CastShadow;
                        SMC->SetMobility( OtherSMC->Mobility );

                        if (OtherSMC_NonConst->GetBodySetup() && SMC->GetBodySetup())
                        {
                            // Copy the BodySetup
                            SMC->GetBodySetup()->CopyBodyPropertiesFrom(OtherSMC_NonConst->GetBodySetup());

                            // We need to recreate the physics mesh for the new body setup
                            SMC->GetBodySetup()->InvalidatePhysicsData();
                            SMC->GetBodySetup()->CreatePhysicsMeshes();

                            // Only copy the physical material if it's different from the default one,
                            // As this was causing crashes on BakeToActors in some cases
                            if (GEngine != NULL && OtherSMC_NonConst->GetBodySetup()->GetPhysMaterial() != GEngine->DefaultPhysMaterial)
                                SMC->SetPhysMaterialOverride(OtherSMC_NonConst->GetBodySetup()->GetPhysMaterial());
                        }
                        SMActor->SetActorHiddenInGame( OtherSMC->bHiddenInGame );
                        SMC->SetVisibility( OtherSMC->IsVisible() );

                        // Reapply the uproperties modified by attributes on the new component
                        FHoudiniEngineUtils::UpdateUPropertyAttributesOnObject( SMC, HoudiniGeoPartObject );

                        SMC->PostEditChange();
                    }
                }
            };

            if( const UInstancedStaticMeshComponent* OtherISMC = Cast< const UInstancedStaticMeshComponent>( OtherSMC ) )
            {
#ifdef BAKE_TO_INSTANCEDSTATICMESHCOMPONENT_ACTORS
                // This is an instanced static mesh component - we will create a generic AActor with a UInstancedStaticMeshComponent root
                FActorSpawnParameters SpawnInfo;
                SpawnInfo.OverrideLevel = DesiredLevel;
                SpawnInfo.ObjectFlags = RF_Transactional;
                SpawnInfo.Name = MakeUniqueObjectName( DesiredLevel, AActor::StaticClass(), BaseName );
                SpawnInfo.bDeferConstruction = true;

                if( AActor* NewActor = DesiredLevel->OwningWorld->SpawnActor<AActor>( SpawnInfo ) )
                {
                    NewActor->SetActorLabel( NewActor->GetName() );
                    NewActor->SetActorHiddenInGame( OtherISMC->bHiddenInGame );

                    // Do we need to create a HISMC?
                    const UHierarchicalInstancedStaticMeshComponent* OtherHISMC = Cast< const UHierarchicalInstancedStaticMeshComponent>( OtherSMC );		    
                    UInstancedStaticMeshComponent* NewISMC = nullptr;
                    if ( OtherHISMC )
                        NewISMC = DuplicateObject< UHierarchicalInstancedStaticMeshComponent >(OtherHISMC, NewActor, *OtherHISMC->GetName() ) );
                    else
                        NewISMC = DuplicateObject< UInstancedStaticMeshComponent >( OtherISMC, NewActor, *OtherISMC->GetName() ) );

                    if( NewISMC )
                    {
                        NewISMC->SetupAttachment( nullptr );
                        NewISMC->SetStaticMesh( BakedSM );
                        NewActor->AddInstanceComponent( NewISMC );
                        NewActor->SetRootComponent( NewISMC );
                        NewISMC->SetWorldTransform( OtherISMC->GetComponentTransform() );
                        NewISMC->RegisterComponent();

                        NewActor->SetFolderPath( BaseName );
                        NewActor->FinishSpawning( OtherISMC->GetComponentTransform() );

                        NewActors.Add( NewActor );
                        NewActor->InvalidateLightingCache();
                        NewActor->PostEditMove( true );
                        NewActor->MarkPackageDirty();
                    }
                }
#else
                // This is an instanced static mesh component - we will split it up into StaticMeshActors
                for( int32 InstanceIx = 0; InstanceIx < OtherISMC->GetInstanceCount(); ++InstanceIx )
                {
                    FTransform InstanceTransform;
                    OtherISMC->GetInstanceTransform( InstanceIx, InstanceTransform, true );
                    if( AActor* NewActor = Factory->CreateActor( BakedSM, DesiredLevel, InstanceTransform, RF_Transactional ) )
                    {
                        PrepNewStaticMeshActor( NewActor );

                        // We need to set the modified uproperty on the created actor
                        if ( AStaticMeshActor* SMActor = Cast< AStaticMeshActor>(NewActor) )
                        {
                            if ( UStaticMeshComponent* SMC = SMActor->GetStaticMeshComponent() )
                            {
                                FHoudiniGeoPartObject GeoPartObject = HoudiniAssetComponent->LocateGeoPartObject( OtherSMC->GetStaticMesh() );

                                // Set the part id to 0 so we can access the instancer
                                GeoPartObject.PartId = 0;
                                FHoudiniEngineUtils::UpdateUPropertyAttributesOnObject( SMC, GeoPartObject );
                            }
                        }
                    }
                }
#endif
            }
            else
            {
                if( AActor* NewActor = Factory->CreateActor( BakedSM, DesiredLevel, OtherSMC->GetComponentTransform(), RF_Transactional ) )
                {
                    PrepNewStaticMeshActor( NewActor );
                }
            }
        }
    }
#endif
    return NewActors;
}

TArray<AActor*>
FHoudiniEngineBakeUtils::BakeHoudiniActorToActors_SplitMeshInstancers(UHoudiniAssetComponent * HoudiniAssetComponent,
    TMap<const UHoudiniMeshSplitInstancerComponent *, FHoudiniGeoPartObject> SplitMeshInstancerComponentToPart)
{
    TArray< AActor* > NewActors;
#if WITH_EDITOR
    TMap< const UStaticMesh*, UStaticMesh* > OriginalToBakedMesh;

    for( auto&& Iter : SplitMeshInstancerComponentToPart )
    {
        auto&& HoudiniGeoPartObject = Iter.Value;
        auto&& OtherMSIC = Iter.Key;

        UStaticMesh * OtherSM = OtherMSIC->GetStaticMesh();

        if( !ensure(OtherSM) )
            continue;

        CheckedBakeStaticMesh(HoudiniAssetComponent, OriginalToBakedMesh, HoudiniGeoPartObject, OtherSM);
    }
    // Done baking, now spawn the actors
    for( auto&& Iter : SplitMeshInstancerComponentToPart )
    {
        auto&& OtherMSIC = Iter.Key;
        UStaticMesh* BakedSM = OriginalToBakedMesh[OtherMSIC->GetStaticMesh()];

        if( ensure(BakedSM) )
        {
            ULevel* DesiredLevel = GWorld->GetCurrentLevel();
            FName BaseName(*( HoudiniAssetComponent->GetOwner()->GetName() + TEXT("_Baked") ));

            // This is a split mesh instancer component - we will create a generic AActor with a bunch of SMC
            FActorSpawnParameters SpawnInfo;
            SpawnInfo.OverrideLevel = DesiredLevel;
            SpawnInfo.ObjectFlags = RF_Transactional;
            SpawnInfo.Name = MakeUniqueObjectName(DesiredLevel, AActor::StaticClass(), BaseName);
            SpawnInfo.bDeferConstruction = true;

            if( AActor* NewActor = DesiredLevel->OwningWorld->SpawnActor<AActor>(SpawnInfo) )
            {
                NewActor->SetActorLabel(NewActor->GetName());
                NewActor->SetActorHiddenInGame(OtherMSIC->bHiddenInGame);

                for( UStaticMeshComponent* OtherSMC : OtherMSIC->GetInstances() )
                {
                    if( UStaticMeshComponent* NewSMC = DuplicateObject< UStaticMeshComponent >(OtherSMC, NewActor, *OtherSMC->GetName()) )
                    {
                        NewSMC->SetupAttachment(nullptr);
                        NewSMC->SetStaticMesh(BakedSM);
                        NewActor->AddInstanceComponent(NewSMC);
                        NewSMC->SetWorldTransform(OtherSMC->GetComponentTransform());
                        NewSMC->RegisterComponent();
                    }
                }
                NewActor->SetFolderPath(BaseName);
                NewActor->FinishSpawning(OtherMSIC->GetComponentTransform());

                NewActors.Add(NewActor);
                NewActor->InvalidateLightingCache();
                NewActor->PostEditMove(true);
                NewActor->MarkPackageDirty();
            }
        }
    }
#endif
    return NewActors;
}

UHoudiniAssetInput*
FHoudiniEngineBakeUtils::GetInputForBakeHoudiniActorToOutlinerInput( const UHoudiniAssetComponent * HoudiniAssetComponent )
{
    UHoudiniAssetInput *FirstInput = nullptr, *Result = nullptr;
#if WITH_EDITOR
    if( HoudiniAssetComponent->GetInputs().Num() && HoudiniAssetComponent->GetInputs()[0] )
    {
        FirstInput = HoudiniAssetComponent->GetInputs()[0];
    }
    else
    {
        TMap< FString, UHoudiniAssetParameter* > InputParams;
        HoudiniAssetComponent->CollectAllParametersOfType( UHoudiniAssetInput::StaticClass(), InputParams );
        TArray< UHoudiniAssetParameter* > InputParamsArray;
        InputParams.GenerateValueArray( InputParamsArray );
        if( InputParamsArray.Num() > 0 )
        {
            FirstInput = Cast<UHoudiniAssetInput>( InputParamsArray[0] );
        }
    }

    if( FirstInput )
    {
        if( FirstInput->GetChoiceIndex() == EHoudiniAssetInputType::WorldInput && FirstInput->GetWorldOutlinerInputs().Num() == 1 )
        {
            const FHoudiniAssetInputOutlinerMesh& InputOutlinerMesh = FirstInput->GetWorldOutlinerInputs()[0];
            if( InputOutlinerMesh.ActorPtr.IsValid() && InputOutlinerMesh.StaticMeshComponent )
            {
                Result = FirstInput;
            }
        }
    }
#endif
    return Result;
}

bool
FHoudiniEngineBakeUtils::GetCanComponentBakeToOutlinerInput( const UHoudiniAssetComponent * HoudiniAssetComponent )
{
#if WITH_EDITOR
    // TODO: Cache this
    auto SMComponentToPart = HoudiniAssetComponent->CollectAllStaticMeshComponents();
    if( SMComponentToPart.Num() == 1 )
    {
        return nullptr != GetInputForBakeHoudiniActorToOutlinerInput( HoudiniAssetComponent );
    }
#endif
    return false;
}

void
FHoudiniEngineBakeUtils::BakeHoudiniActorToOutlinerInput( UHoudiniAssetComponent * HoudiniAssetComponent )
{
#if WITH_EDITOR
    TMap< const UStaticMesh*, UStaticMesh* > OriginalToBakedMesh;
    TMap< const UStaticMeshComponent*, FHoudiniGeoPartObject > SMComponentToPart = HoudiniAssetComponent->CollectAllStaticMeshComponents();

    for( const auto& Iter : SMComponentToPart )
    {
        const FHoudiniGeoPartObject & HoudiniGeoPartObject = Iter.Value;
        const UStaticMeshComponent * OtherSMC = Iter.Key;

        if( !ensure( OtherSMC->GetStaticMesh() ) )
            continue;

        UStaticMesh* BakedSM = nullptr;
        if( UStaticMesh ** FoundMeshPtr = OriginalToBakedMesh.Find( OtherSMC->GetStaticMesh() ) )
        {
            // We've already baked this mesh, use it
            BakedSM = *FoundMeshPtr;
        }
        else
        {
            // Bake the found mesh into the project
            BakedSM = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackage(
                OtherSMC->GetStaticMesh(), HoudiniAssetComponent, HoudiniGeoPartObject, EBakeMode::CreateNewAssets );

            if( BakedSM )
            {
                OriginalToBakedMesh.Add( OtherSMC->GetStaticMesh(), BakedSM );
                FAssetRegistryModule::AssetCreated( BakedSM );
            }
        }
    }

    {
        const FScopedTransaction Transaction( LOCTEXT( "BakeToInput", "Bake To Input" ) );

        for( auto Iter : OriginalToBakedMesh )
        {
            // Get the first outliner input
            if( UHoudiniAssetInput* FirstInput = GetInputForBakeHoudiniActorToOutlinerInput( HoudiniAssetComponent ) )
            {
                const FHoudiniAssetInputOutlinerMesh& InputOutlinerMesh = FirstInput->GetWorldOutlinerInputs()[0];
                if( InputOutlinerMesh.ActorPtr.IsValid() && InputOutlinerMesh.StaticMeshComponent )
                {
                    UStaticMeshComponent* InOutSMC = InputOutlinerMesh.StaticMeshComponent;
                    InputOutlinerMesh.ActorPtr->Modify();
                    InOutSMC->SetStaticMesh( Iter.Value );
                    InOutSMC->InvalidateLightingCache();
                    InOutSMC->MarkPackageDirty();

                    // Disconnect the input from the asset - InputOutlinerMesh now garbage
                    FirstInput->RemoveWorldOutlinerInput( 0 );
                }
            }
            // Only handle the first Baked Mesh
            break;
        }
    }
#endif
}

bool
FHoudiniEngineBakeUtils::StaticMeshRequiresBake( const UStaticMesh * StaticMesh )
{
#if WITH_EDITOR
    check( StaticMesh );
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( "AssetRegistry" );

    FAssetData BackingAssetData = AssetRegistryModule.Get().GetAssetByObjectPath( *StaticMesh->GetPathName() );
    if( !BackingAssetData.IsUAsset() )
        return true;

    for( const auto& StaticMaterial : StaticMesh->StaticMaterials )
    {
        if( StaticMaterial.MaterialInterface )
        {
            BackingAssetData = AssetRegistryModule.Get().GetAssetByObjectPath( *StaticMaterial.MaterialInterface->GetPathName() );
            if( !BackingAssetData.IsUAsset() )
                return true;
        }
    }
#endif
    return false;
}

UMaterial *
FHoudiniEngineBakeUtils::DuplicateMaterialAndCreatePackage(
    UMaterial * Material, FHoudiniCookParams& HoudiniCookParams, const FString & SubMaterialName )
{
    UMaterial * DuplicatedMaterial = nullptr;
#if WITH_EDITOR
    // Create material package.
    FString MaterialName;
    UPackage * MaterialPackage = FHoudiniEngineBakeUtils::BakeCreateTextureOrMaterialPackageForComponent(
        HoudiniCookParams, SubMaterialName, MaterialName );

    if( !MaterialPackage )
        return nullptr;

    // Clone material.
    DuplicatedMaterial = DuplicateObject< UMaterial >( Material, MaterialPackage, *MaterialName );

    if( HoudiniCookParams.MaterialAndTextureBakeMode != EBakeMode::Intermediate )
        DuplicatedMaterial->SetFlags( RF_Public | RF_Standalone );

    // Add meta information.
    FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
        MaterialPackage, DuplicatedMaterial,
        HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT( "true" ) );
    FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
        MaterialPackage, DuplicatedMaterial,
        HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *MaterialName );

    // Retrieve and check various sampling expressions. If they contain textures, duplicate (and bake) them.

    for( auto& Expression : DuplicatedMaterial->Expressions )
    {
        FHoudiniEngineBakeUtils::ReplaceDuplicatedMaterialTextureSample(
            Expression, HoudiniCookParams );
    }

    // Notify registry that we have created a new duplicate material.
    FAssetRegistryModule::AssetCreated( DuplicatedMaterial );

    // Dirty the material package.
    DuplicatedMaterial->MarkPackageDirty();

    // Reset any derived state
    DuplicatedMaterial->ForceRecompileForRendering();
#endif
    return DuplicatedMaterial;
}

void
FHoudiniEngineBakeUtils::ReplaceDuplicatedMaterialTextureSample(
    UMaterialExpression * MaterialExpression, FHoudiniCookParams& HoudiniCookParams )
{
#if WITH_EDITOR
    UMaterialExpressionTextureSample * TextureSample = Cast< UMaterialExpressionTextureSample >( MaterialExpression );
    if( TextureSample )
    {
        UTexture2D * Texture = Cast< UTexture2D >( TextureSample->Texture );
        if( Texture )
        {
            UPackage * TexturePackage = Cast< UPackage >( Texture->GetOuter() );
            if( TexturePackage )
            {
                FString GeneratedTextureName;
                if( FHoudiniEngineBakeUtils::GetHoudiniGeneratedNameFromMetaInformation(
                    TexturePackage, Texture, GeneratedTextureName ) )
                {
                    // Duplicate texture.
                    UTexture2D * DuplicatedTexture = FHoudiniEngineBakeUtils::DuplicateTextureAndCreatePackage(
                        Texture, HoudiniCookParams, GeneratedTextureName );

                    // Re-assign generated texture.
                    TextureSample->Texture = DuplicatedTexture;
                }
            }
        }
    }
#endif
}

UTexture2D *
FHoudiniEngineBakeUtils::DuplicateTextureAndCreatePackage(
    UTexture2D * Texture, FHoudiniCookParams& HoudiniCookParams, const FString & SubTextureName )
{
    UTexture2D* DuplicatedTexture = nullptr;
#if WITH_EDITOR
    // Retrieve original package of this texture.
    UPackage * TexturePackage = Cast< UPackage >( Texture->GetOuter() );
    if( TexturePackage )
    {
        FString GeneratedTextureName;
        if( FHoudiniEngineBakeUtils::GetHoudiniGeneratedNameFromMetaInformation( TexturePackage, Texture, GeneratedTextureName ) )
        {
            UMetaData * MetaData = TexturePackage->GetMetaData();
            if( MetaData )
            {
                // Retrieve texture type.
                const FString & TextureType =
                    MetaData->GetValue( Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE );

                // Create texture package.
                FString TextureName;
                UPackage * NewTexturePackage = FHoudiniEngineBakeUtils::BakeCreateTextureOrMaterialPackageForComponent(
                    HoudiniCookParams, SubTextureName, TextureName );

                if( !NewTexturePackage )
                    return nullptr;

                // Clone texture.
                DuplicatedTexture = DuplicateObject< UTexture2D >( Texture, NewTexturePackage, *TextureName );

                if( HoudiniCookParams.MaterialAndTextureBakeMode != EBakeMode::Intermediate )
                    DuplicatedTexture->SetFlags( RF_Public | RF_Standalone );

                // Add meta information.
                FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
                    NewTexturePackage, DuplicatedTexture,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT( "true" ) );
                FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
                    NewTexturePackage, DuplicatedTexture,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *TextureName );
                FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
                    NewTexturePackage, DuplicatedTexture,
                    HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE, *TextureType );

                // Notify registry that we have created a new duplicate texture.
                FAssetRegistryModule::AssetCreated( DuplicatedTexture );

                // Dirty the texture package.
                DuplicatedTexture->MarkPackageDirty();
            }
        }
    }
#endif
    return DuplicatedTexture;
}

FHoudiniCookParams::FHoudiniCookParams( UHoudiniAssetComponent* HoudiniAssetComponent )
{
#if WITH_EDITOR
    HoudiniAsset = HoudiniAssetComponent->GetHoudiniAsset();
    HoudiniCookManager = HoudiniAssetComponent;
    PackageGUID = HoudiniAssetComponent->GetComponentGuid();
    BakedStaticMeshPackagesForParts = &HoudiniAssetComponent->BakedStaticMeshPackagesForParts;
    BakedMaterialPackagesForIds = &HoudiniAssetComponent->BakedMaterialPackagesForIds;
    CookedTemporaryStaticMeshPackages = &HoudiniAssetComponent->CookedTemporaryStaticMeshPackages;
    CookedTemporaryPackages = &HoudiniAssetComponent->CookedTemporaryPackages;
    CookedTemporaryLandscapeLayers = &HoudiniAssetComponent->CookedTemporaryLandscapeLayers;
    TempCookFolder = HoudiniAssetComponent->GetTempCookFolder();
    BakeFolder = HoudiniAssetComponent->GetBakeFolder();
    IntermediateOuter = HoudiniAssetComponent->GetComponentLevel();
    GeneratedDistanceFieldResolutionScale = HoudiniAssetComponent->GeneratedDistanceFieldResolutionScale;
#endif
}

bool
FHoudiniEngineBakeUtils::BakeLandscape( UHoudiniAssetComponent* HoudiniAssetComponent, ALandscape * OnlyBakeThisLandscape )
{
#if WITH_EDITOR
    if ( !HoudiniAssetComponent )
        return false;

    if ( !HoudiniAssetComponent->HasLandscape() )
        return false;

    TMap< FHoudiniGeoPartObject, ALandscape * > * LandscapeComponentsPtr = HoudiniAssetComponent->GetLandscapeComponents();
    if ( !LandscapeComponentsPtr )
        return false;

    TArray<UPackage *> LayerPackages;
    bool bNeedToUpdateProperties = false;
    for ( TMap< FHoudiniGeoPartObject, ALandscape * >::TIterator Iter(* LandscapeComponentsPtr ); Iter; ++Iter)
    {
        ALandscape * CurrentLandscape = Iter.Value();
        if ( !CurrentLandscape )
            continue;

        // If we only want to bake a single landscape
        if ( OnlyBakeThisLandscape && CurrentLandscape != OnlyBakeThisLandscape )
            continue;

        // Simply remove the landscape from the map
        FHoudiniGeoPartObject & HoudiniGeoPartObject = Iter.Key();
        LandscapeComponentsPtr->Remove( HoudiniGeoPartObject );

        CurrentLandscape->DetachFromActor( FDetachmentTransformRules::KeepWorldTransform );

        // And save its layers to prevent them from being removed
        for ( TMap< TWeakObjectPtr< UPackage >, FHoudiniGeoPartObject > ::TIterator IterPackage( HoudiniAssetComponent->CookedTemporaryLandscapeLayers ); IterPackage; ++IterPackage )
        {
            if ( !( HoudiniGeoPartObject == IterPackage.Value() ) )
                continue;

            UPackage * Package = IterPackage.Key().Get();
            if ( Package )
                LayerPackages.Add( Package );
        }

        bNeedToUpdateProperties = true;

        // If we only wanted to bake a single landscape, we're done
        if ( OnlyBakeThisLandscape )
            break;
    }

    if ( LayerPackages.Num() > 0 )
    {
        // Save the layer info's package
        FEditorFileUtils::PromptForCheckoutAndSave( LayerPackages, true, false );

        // Remove the packages from the asset component, or the asset component might 
        // destroy them when it is being destroyed
        for ( int32 n = 0; n < LayerPackages.Num(); n++ )
            HoudiniAssetComponent->CookedTemporaryLandscapeLayers.Remove( LayerPackages[n] );
    }

    return bNeedToUpdateProperties;
#else
    return false;
#endif
}

UPackage *
FHoudiniEngineBakeUtils::BakeCreateMaterialPackageForComponent(
    FHoudiniCookParams& HoudiniCookParams,
    const HAPI_MaterialInfo & MaterialInfo, FString & MaterialName )
{
    UHoudiniAsset * HoudiniAsset = HoudiniCookParams.HoudiniAsset;
    FString MaterialDescriptor;

    if( HoudiniCookParams.MaterialAndTextureBakeMode != EBakeMode::Intermediate )
        MaterialDescriptor = HoudiniAsset->GetName() + TEXT( "_material_" ) + FString::FromInt( MaterialInfo.nodeId ) + TEXT( "_" );
    else
        MaterialDescriptor = HoudiniAsset->GetName() + TEXT( "_" ) + FString::FromInt( MaterialInfo.nodeId ) + TEXT( "_" );

    return FHoudiniEngineBakeUtils::BakeCreateTextureOrMaterialPackageForComponent(
        HoudiniCookParams, MaterialDescriptor,
        MaterialName );
}

UPackage *
FHoudiniEngineBakeUtils::BakeCreateTexturePackageForComponent(
    FHoudiniCookParams& HoudiniCookParams,
    const HAPI_MaterialInfo & MaterialInfo, const FString & TextureType,
    FString & TextureName )
{
    UHoudiniAsset * HoudiniAsset = HoudiniCookParams.HoudiniAsset;
    FString TextureInfoDescriptor;

    if ( HoudiniCookParams.MaterialAndTextureBakeMode != EBakeMode::Intermediate )
    {
        TextureInfoDescriptor = HoudiniAsset->GetName() + TEXT( "_texture_" ) + FString::FromInt( MaterialInfo.nodeId ) +
            TEXT( "_" ) + TextureType + TEXT( "_" );
    }
    else
    {
        TextureInfoDescriptor = HoudiniAsset->GetName() + TEXT( "_" ) + FString::FromInt( MaterialInfo.nodeId ) + TEXT( "_" ) +
            TextureType + TEXT( "_" );
    }

    return FHoudiniEngineBakeUtils::BakeCreateTextureOrMaterialPackageForComponent(
        HoudiniCookParams, TextureInfoDescriptor, TextureName );
}

UPackage *
FHoudiniEngineBakeUtils::BakeCreateTextureOrMaterialPackageForComponent(
    FHoudiniCookParams& HoudiniCookParams,
    const FString & MaterialInfoDescriptor,
    FString & MaterialName )
{
    UPackage * PackageNew = nullptr;

#if WITH_EDITOR
    EBakeMode BakeMode = HoudiniCookParams.MaterialAndTextureBakeMode;
    UHoudiniAsset * HoudiniAsset = HoudiniCookParams.HoudiniAsset;
    FGuid BakeGUID;
    FString PackageName;

    const FGuid & ComponentGUID = HoudiniCookParams.PackageGUID;
    FString ComponentGUIDString = ComponentGUID.ToString().Left( FHoudiniEngineUtils::PackageGUIDComponentNameLength );

    if ( ( BakeMode == EBakeMode::ReplaceExisitingAssets ) || ( BakeMode == EBakeMode::CookToTemp ) )
    {
        bool bRemovePackageFromCache = false;

        UPackage* FoundPackage = nullptr;
        if (BakeMode == EBakeMode::ReplaceExisitingAssets)
        {
            TWeakObjectPtr< UPackage > * FoundPointer = HoudiniCookParams.BakedMaterialPackagesForIds->Find(MaterialInfoDescriptor);
            if ( FoundPointer )
            {
                if ( (*FoundPointer).IsValid() )
                    FoundPackage = (*FoundPointer).Get();
            }
            else
            {
                bRemovePackageFromCache = true;
            }
        }
        else
        {
            TWeakObjectPtr< UPackage > * FoundPointer = HoudiniCookParams.CookedTemporaryPackages->Find(MaterialInfoDescriptor);
            if (FoundPointer)
            {
                if ( (*FoundPointer).IsValid() )
                    FoundPackage = (*FoundPointer).Get();
            }
            else
            {
                bRemovePackageFromCache = true;
            }
        }

        // Find a previously baked / cooked asset
        if ( FoundPackage )
        {
            if ( UPackage::IsEmptyPackage( FoundPackage ) )
            {
                // This happens when the prior baked output gets renamed, we can delete this 
                // orphaned package so that we can re-use the name
                FoundPackage->ClearFlags( RF_Standalone );
                FoundPackage->ConditionalBeginDestroy();

                bRemovePackageFromCache = true;
            }
            else
            {
                if ( CheckPackageSafeForBake( FoundPackage, MaterialName ) && !MaterialName.IsEmpty() )
                {
                    return FoundPackage;
                }
                else
                {
                    // Found the package but we can't update it.  We already issued an error, but should popup the standard reference error dialog
                    //::ErrorPopup( TEXT( "Baking Failed: Could not overwrite %s, because it is being referenced" ), *(*FoundPackage)->GetPathName() );

                    // If we're cooking, we'll create a new package, if baking, fail
                    if ( BakeMode != EBakeMode::CookToTemp )
                        return nullptr;
                }
            }

            bRemovePackageFromCache = true;
        }

        if ( bRemovePackageFromCache )
        {
            // Package is either invalid / not found so we need to remove it from the cache
            if ( BakeMode == EBakeMode::ReplaceExisitingAssets )
                HoudiniCookParams.BakedMaterialPackagesForIds->Remove( MaterialInfoDescriptor );
            else
                HoudiniCookParams.CookedTemporaryPackages->Remove( MaterialInfoDescriptor );
        }
    }

    while ( true )
    {
        if ( !BakeGUID.IsValid() )
            BakeGUID = FGuid::NewGuid();

        // We only want half of generated guid string.
        FString BakeGUIDString = BakeGUID.ToString().Left( FHoudiniEngineUtils::PackageGUIDItemNameLength );

        // Generate material name.
        MaterialName = MaterialInfoDescriptor;
        MaterialName += BakeGUIDString;

        switch (BakeMode)
        {
            case EBakeMode::Intermediate:
            {
                // Generate unique package name.
                PackageName = FPackageName::GetLongPackagePath( HoudiniAsset->GetOuter()->GetName() ) +
                    TEXT("/") +
                    HoudiniAsset->GetName() +
                    TEXT("_") +
                    ComponentGUIDString +
                    TEXT("/") +
                    MaterialName;
            }
            break;

            case EBakeMode::CookToTemp:
            {
                PackageName = HoudiniCookParams.TempCookFolder.ToString() + TEXT("/") + MaterialName;
            }
            break;

            default:
            {
                // Generate unique package name.
                PackageName = HoudiniCookParams.BakeFolder.ToString() + TEXT("/") + MaterialName;
            }
            break;
        }

        // Sanitize package name.
        PackageName = PackageTools::SanitizePackageName( PackageName );

        UObject * OuterPackage = nullptr;
        if ( BakeMode == EBakeMode::Intermediate )
        {
            // If we are not baking, then use outermost package, since objects within our package need to be visible
            // to external operations, such as copy paste.
            OuterPackage = HoudiniCookParams.IntermediateOuter;
        }

        // See if package exists, if it does, we need to regenerate the name.
        UPackage * Package = FindPackage( OuterPackage, *PackageName );

        if ( Package )
        {
            // Package does exist, there's a collision, we need to generate a new name.
            BakeGUID.Invalidate();
        }
        else
        {
            // Create actual package.
            PackageNew = CreatePackage( OuterPackage, *PackageName );
            break;
        }
    }

    if( PackageNew && ( ( BakeMode == EBakeMode::ReplaceExisitingAssets ) || ( BakeMode == EBakeMode::CookToTemp ) ) )
    {
        // Add the new package to the cache
        if ( BakeMode == EBakeMode::ReplaceExisitingAssets )
            HoudiniCookParams.BakedMaterialPackagesForIds->Add( MaterialInfoDescriptor, PackageNew );
        else
            HoudiniCookParams.CookedTemporaryPackages->Add( MaterialInfoDescriptor, PackageNew );
    }
#endif
    return PackageNew;
}

bool 
FHoudiniEngineBakeUtils::CheckPackageSafeForBake( UPackage* Package, FString& FoundAssetName )
{
    if( Package )
    {
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( "AssetRegistry" );
        TArray<FAssetData> AssetsInPackage;
        AssetRegistryModule.Get().GetAssetsByPackageName( Package->GetFName(), AssetsInPackage );
        for( const auto& AssetInfo : AssetsInPackage )
        {
            if( AssetInfo.GetAsset() )
            {
                // Check and see whether we are referenced by any objects that won't be garbage collected (*including* the undo buffer)
                FReferencerInformationList ReferencesIncludingUndo;
                UObject* AssetInPackage = AssetInfo.GetAsset();
                bool bReferencedInMemoryOrUndoStack = IsReferenced( AssetInPackage, GARBAGE_COLLECTION_KEEPFLAGS, EInternalObjectFlags::GarbageCollectionKeepFlags, true, &ReferencesIncludingUndo );
                if( bReferencedInMemoryOrUndoStack )
                {
                    // warn
                    HOUDINI_LOG_ERROR( TEXT( "Could not bake to %s because it is being referenced" ), *Package->GetPathName() );
                    return false;
                }
                FoundAssetName = AssetInfo.AssetName.ToString();
            }
        }
    }
    else
    {
        return false;
    }
    return true;
}

UPackage *
FHoudiniEngineBakeUtils::BakeCreateStaticMeshPackageForComponent(
    FHoudiniCookParams& HoudiniCookParams,
    const FHoudiniGeoPartObject & HoudiniGeoPartObject,
    FString & MeshName, FGuid & BakeGUID )
{
    UPackage * PackageNew = nullptr;

#if WITH_EDITOR
    EBakeMode BakeMode = HoudiniCookParams.StaticMeshBakeMode;
    FString PackageName;
    int32 BakeCount = 0;
    const FGuid & ComponentGUID = HoudiniCookParams.PackageGUID;
    FString ComponentGUIDString = ComponentGUID.ToString().Left(
        FHoudiniEngineUtils::PackageGUIDComponentNameLength );

    while ( true )
    {
        if( ( BakeMode == EBakeMode::ReplaceExisitingAssets ) || ( BakeMode == EBakeMode::CookToTemp ) )
        {
            bool bRemovePackageFromCache = false;

            UPackage* FoundPackage = nullptr;
            if (BakeMode == EBakeMode::ReplaceExisitingAssets)
            {
                TWeakObjectPtr< UPackage > * FoundPointer = HoudiniCookParams.BakedStaticMeshPackagesForParts->Find( HoudiniGeoPartObject );
                if ( FoundPointer )
                {
                    if ( ( *FoundPointer ).IsValid() )
                        FoundPackage = ( *FoundPointer ).Get();
                }
                else
                {
                    bRemovePackageFromCache = true;
                }
            }
            else
            {
                TWeakObjectPtr< UPackage > * FoundPointer = HoudiniCookParams.CookedTemporaryStaticMeshPackages->Find( HoudiniGeoPartObject );
                if ( FoundPointer )
                {
                    if ( ( *FoundPointer ).IsValid() )
                        FoundPackage = ( *FoundPointer ).Get();
                }
                else
                {
                    bRemovePackageFromCache = true;
                }
            }

            // Find a previously baked / cooked asset
            if ( FoundPackage )
            {
                if ( UPackage::IsEmptyPackage( FoundPackage ) )
                {
                    // This happens when the prior baked output gets renamed, we can delete this 
                    // orphaned package so that we can re-use the name
                    FoundPackage->ClearFlags( RF_Standalone );
                    FoundPackage->ConditionalBeginDestroy();

                    bRemovePackageFromCache = true;
                }
                else
                {
                    if ( FHoudiniEngineBakeUtils::CheckPackageSafeForBake( FoundPackage, MeshName ) && !MeshName.IsEmpty() )
                    {
                        return FoundPackage;
                    }
                    else
                    {
                        // Found the package but we can't update it.  We already issued an error, but should popup the standard reference error dialog
                        //::ErrorPopup( TEXT( "Baking Failed: Could not overwrite %s, because it is being referenced" ), *(*FoundPackage)->GetPathName() );

                        // If we're cooking, we'll create a new package, if baking, fail
                        if ( BakeMode != EBakeMode::CookToTemp )
                            return nullptr;
                    }
                }

                bRemovePackageFromCache = true;
            }

            if ( bRemovePackageFromCache )
            {
                // Package is either invalid / not found so we need to remove it from the cache
                if ( BakeMode == EBakeMode::ReplaceExisitingAssets )
                    HoudiniCookParams.BakedStaticMeshPackagesForParts->Remove( HoudiniGeoPartObject );
                else
                    HoudiniCookParams.CookedTemporaryStaticMeshPackages->Remove( HoudiniGeoPartObject );
            }
        }

        if ( !BakeGUID.IsValid() )
            BakeGUID = FGuid::NewGuid();

        MeshName = HoudiniCookParams.HoudiniCookManager->GetBakingBaseName( HoudiniGeoPartObject );

        if( BakeCount > 0 )
        {
            MeshName += FString::Printf( TEXT( "_%02d" ), BakeCount );
        }

        switch ( BakeMode )
        {
            case EBakeMode::Intermediate:
            {
                // We only want half of generated guid string.
                FString BakeGUIDString = BakeGUID.ToString().Left( FHoudiniEngineUtils::PackageGUIDItemNameLength );

                MeshName += TEXT("_") +
                    FString::FromInt(HoudiniGeoPartObject.ObjectId) + TEXT("_") +
                    FString::FromInt(HoudiniGeoPartObject.GeoId) + TEXT("_") +
                    FString::FromInt(HoudiniGeoPartObject.PartId) + TEXT("_") +
                    FString::FromInt(HoudiniGeoPartObject.SplitId) + TEXT("_") +
                    HoudiniGeoPartObject.SplitName + TEXT("_") +
                    BakeGUIDString;

                PackageName = FPackageName::GetLongPackagePath( HoudiniCookParams.HoudiniAsset->GetOuter()->GetName() ) +
                    TEXT("/") +
                    HoudiniCookParams.HoudiniAsset->GetName() +
                    TEXT("_") +
                    ComponentGUIDString +
                    TEXT("/") +
                    MeshName;
            }
            break;

            case EBakeMode::CookToTemp:
            {
                PackageName = HoudiniCookParams.TempCookFolder.ToString() + TEXT("/") + MeshName;
            }
            break;

            default:
            {
                PackageName = HoudiniCookParams.BakeFolder.ToString() + TEXT("/") + MeshName;
            }
            break;
        }

        // Santize package name.
        PackageName = PackageTools::SanitizePackageName( PackageName );

        UObject * OuterPackage = nullptr;

        if ( BakeMode == EBakeMode::Intermediate )
        {
            // If we are not baking, then use outermost package, since objects within our package need to be visible
            // to external operations, such as copy paste.
            OuterPackage = HoudiniCookParams.IntermediateOuter;
        }

        // See if package exists, if it does, we need to regenerate the name.
        UPackage * Package = FindPackage( OuterPackage, *PackageName );

        if ( Package )
        {
            if ( BakeMode != EBakeMode::Intermediate )
            {
                // Increment bake counter
                BakeCount++;
            }
            else
            {
                // Package does exist, there's a collision, we need to generate a new name.
                BakeGUID.Invalidate();
            }
        }
        else
        {
            // Create actual package.
            PackageNew = CreatePackage( OuterPackage, *PackageName );
            break;
        }
    }

    if ( PackageNew && ( ( BakeMode == EBakeMode::ReplaceExisitingAssets ) || ( BakeMode == EBakeMode::CookToTemp ) ) )
    {
        // Add the new package to the cache
        if ( BakeMode == EBakeMode::ReplaceExisitingAssets )
            HoudiniCookParams.BakedStaticMeshPackagesForParts->Add( HoudiniGeoPartObject, PackageNew );
        else
            HoudiniCookParams.CookedTemporaryStaticMeshPackages->Add( HoudiniGeoPartObject, PackageNew );
    }
#endif
    return PackageNew;
}


void
FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
    UPackage * Package, UObject * Object, const TCHAR * Key,
    const TCHAR * Value)
{
    UMetaData * MetaData = Package->GetMetaData();
    MetaData->SetValue( Object, Key, Value );
}

bool
FHoudiniEngineBakeUtils::GetHoudiniGeneratedNameFromMetaInformation(
    UPackage * Package, UObject * Object, FString & HoudiniName )
{
    UMetaData * MetaData = Package->GetMetaData();
    if ( MetaData && MetaData->HasValue( Object, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT ) )
    {
        // Retrieve name used for package generation.
        const FString NameFull = MetaData->GetValue( Object, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME );
        HoudiniName = NameFull.Left( NameFull.Len() - FHoudiniEngineUtils::PackageGUIDItemNameLength );

        return true;
    }

    return false;
}