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

#include "HoudiniEngineInstancerUtils.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniEngineString.h"
#include "HoudiniInstancedActorComponent.h"
#include "HoudiniMeshSplitInstancerComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

#include "HoudiniEngineRuntimePrivatePCH.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

bool
FHoudiniEngineInstancerUtils::CreateAllInstancers(
    FHoudiniCookParams& HoudiniCookParams,
    const HAPI_NodeId& AssetId,
    const TArray< FHoudiniGeoPartObject > & FoundInstancers,
    TMap< FHoudiniGeoPartObject, UStaticMesh * > & StaticMeshes,
    USceneComponent* ParentComponent,
    TMap< FHoudiniGeoPartObject, USceneComponent * >& Instancers,
    TMap< FHoudiniGeoPartObject, USceneComponent * >& NewInstancers )
{
    for ( const FHoudiniGeoPartObject& HoudiniGeoPartObject : FoundInstancers )
    {
        if ( !HoudiniGeoPartObject.IsVisible() )
        continue;

    // Get object to be instanced.
    HAPI_NodeId ObjectToInstance = HoudiniGeoPartObject.HapiObjectGetToInstanceId();

    // We only handle packed prim and attribute overrides instancers for now.
    bool bIsPackedPrimitiveInstancer = HoudiniGeoPartObject.IsPackedPrimitiveInstancer();
    bool bAttributeInstancerOverride = HoudiniGeoPartObject.IsAttributeOverrideInstancer();

    // This is invalid combination, no object to instance and input is not an attribute instancer.
    if ( !bIsPackedPrimitiveInstancer && !bAttributeInstancerOverride && ObjectToInstance == -1 )
        continue;

    // Extract the objects to instance and their transforms
    TArray< FHoudiniGeoPartObject > InstancedGeoParts;
    TArray< TArray< FTransform > > InstancedGeoPartsTransforms;
    TArray< UObject *> InstancedObjects;
    TArray< TArray< FTransform > > InstancedObjectsTransforms;
    if ( !FHoudiniEngineInstancerUtils::GetInstancedObjectsAndTransforms(
        HoudiniGeoPartObject, AssetId,
        InstancedGeoParts, InstancedGeoPartsTransforms,
        InstancedObjects, InstancedObjectsTransforms ) )
        continue;

    // We are instantiating HoudiniGeoPartObjects
    // We need to find their corresponding meshes, and add them and their transform to the instanced Object arrays
    if ( InstancedGeoParts.Num() > 0 && InstancedGeoPartsTransforms.Num() > 0 )
    {
        // Handle Instanced GeoPartObjects here
        for ( int32 InstancedGeoIndex = 0; InstancedGeoIndex < InstancedGeoParts.Num(); InstancedGeoIndex++ )
        {
        const FHoudiniGeoPartObject& InstancedGeoPartObject = InstancedGeoParts[ InstancedGeoIndex ];

        if ( !InstancedGeoPartsTransforms.IsValidIndex( InstancedGeoIndex ) )
            continue;

        const TArray< FTransform >& InstancedGeoPartTransforms = InstancedGeoPartsTransforms[ InstancedGeoIndex ];

        // We need to find the mesh that corresponds to the GeoPartObject
        UStaticMesh** FoundStaticMesh = StaticMeshes.Find( InstancedGeoPartObject );
        if ( FoundStaticMesh && *FoundStaticMesh )
        {
            // We can add the static mesh and its transform
            InstancedObjects.Add( *FoundStaticMesh );
            InstancedObjectsTransforms.Add( InstancedGeoPartTransforms );
        }
        else if ( InstancedGeoPartObject.IsPackedPrimitiveInstancer() )
        {
            // We are instantiating a packed primitive
            TArray< FHoudiniGeoPartObject > AllInstancedPPGeoParts;
            TArray< TArray< FTransform > > AllInstancedPPTransforms;
            if ( FHoudiniEngineInstancerUtils::GetPackedPrimInstancerObjectsAndTransforms(
                InstancedGeoPartObject, AllInstancedPPGeoParts, AllInstancedPPTransforms ) )
            {
            for ( int32 InstancedPPIndex = 0; InstancedPPIndex < AllInstancedPPGeoParts.Num(); InstancedPPIndex++ )
            {
                if ( !AllInstancedPPTransforms.IsValidIndex( InstancedPPIndex ) )
                continue;

                FHoudiniGeoPartObject InstancedPPGeoPart = AllInstancedPPGeoParts[ InstancedPPIndex ];
                TArray< FTransform > InstancedPPTransforms = AllInstancedPPTransforms[ InstancedPPIndex ];

                // Try to find the static mesh corresponding to that PP GeoPart                
                UStaticMesh** FoundPPStaticMesh = StaticMeshes.Find( InstancedPPGeoPart );
                if ( !FoundPPStaticMesh || ( *FoundPPStaticMesh == nullptr ) )
                continue;

                // Build a combined list of all the transforms of the instancer and the packed prim's transform
                TArray< FTransform > CombinedTransforms;
                CombinedTransforms.Empty( InstancedPPTransforms.Num() * InstancedGeoPartTransforms.Num() );
                for ( const FTransform& InstancedTransform : InstancedGeoPartTransforms )                
                {
                for ( const FTransform& PPTransform : InstancedPPTransforms )
                {
                    CombinedTransforms.Add( PPTransform * InstancedTransform );
                }
                }

                // We can add the static mesh and its transform
                InstancedObjects.Add( *FoundPPStaticMesh );
                InstancedObjectsTransforms.Add( CombinedTransforms );
            }
            }
            else
            {
            HOUDINI_LOG_WARNING(
                TEXT( "CreateAllInstancers for Packed Primitive: Could not find static mesh for object [%d %s], geo %d, part %d]" ),
                InstancedGeoPartObject.ObjectId, *InstancedGeoPartObject.ObjectName, InstancedGeoPartObject.GeoId, InstancedGeoPartObject.PartId );
            }
        }
        }
    }

    if ( InstancedObjects.Num() > 0 && InstancedObjectsTransforms.Num() > 0 )
    {
        // Handle Instanced Unreal Objects here
        for ( int32 InstancedObjectIndex = 0; InstancedObjectIndex < InstancedObjects.Num(); InstancedObjectIndex++ )
        {
        UObject* InstancedObject = InstancedObjects[ InstancedObjectIndex ];

        if ( !InstancedObjectsTransforms.IsValidIndex( InstancedObjectIndex ) )
            continue;

        const TArray< FTransform >& InstancedObjectTransforms = InstancedObjectsTransforms[ InstancedObjectIndex ];

        USceneComponent* CreatedSceneComponent = nullptr;
        if ( !CreateInstancerComponent( InstancedObject, InstancedObjectTransforms, HoudiniGeoPartObject, ParentComponent, CreatedSceneComponent ) )
            continue;

        if ( !CreatedSceneComponent )
            continue;

        NewInstancers.Add( HoudiniGeoPartObject, CreatedSceneComponent );
        }
    }
    }

    return true;
}

bool
FHoudiniEngineInstancerUtils::GetInstancedObjectsAndTransforms(
    const FHoudiniGeoPartObject& HoudiniGeoPartObject, const HAPI_NodeId& AssetId,    
    TArray< FHoudiniGeoPartObject >& InstancedGeoParts, TArray< TArray< FTransform > >& InstancedGeoPartsTransforms,
    TArray< UObject *>& InstancedObjects, TArray< TArray< FTransform > >& InstancedObjectsTransforms )
{
    // Get the instancer flags
    bool bIsPackedPrimitiveInstancer = HoudiniGeoPartObject.IsPackedPrimitiveInstancer();
    bool bAttributeInstancerOverride = HoudiniGeoPartObject.IsAttributeOverrideInstancer();

    if ( bIsPackedPrimitiveInstancer )
    {
    return FHoudiniEngineInstancerUtils::GetPackedPrimInstancerObjectsAndTransforms(
        HoudiniGeoPartObject, InstancedGeoParts, InstancedGeoPartsTransforms );
    }
    else if ( bAttributeInstancerOverride )
    {
    return FHoudiniEngineInstancerUtils::GetUnrealAttributeInstancerObjectsAndTransforms(
        HoudiniGeoPartObject, AssetId, InstancedObjects, InstancedObjectsTransforms );
    }

    return false;

}

bool
FHoudiniEngineInstancerUtils::GetPackedPrimInstancerObjectsAndTransforms(
    const FHoudiniGeoPartObject& HoudiniGeoPartObject,    
    TArray< FHoudiniGeoPartObject >& InstancedGeoPart,
    TArray< TArray< FTransform > >& InstancedTransforms )
{
    if ( !HoudiniGeoPartObject.IsPackedPrimitiveInstancer() )
    return false;

    // This is using packed primitives
    HAPI_PartInfo PartInfo;
    FHoudiniApi::PartInfo_Init(&PartInfo);
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetPartInfo(
        FHoudiniEngine::Get().GetSession(), HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, &PartInfo ), false );

    /*// Retrieve part name.
    FString PartName;
    FHoudiniEngineString HoudiniEngineStringPartName( PartInfo.nameSH );
    HoudiniEngineStringPartName.ToFString( PartName );*/

    // Get transforms for each instance
    TArray<HAPI_Transform> InstancerPartTransforms;
    InstancerPartTransforms.SetNumZeroed( PartInfo.instanceCount );

    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetInstancerPartTransforms(
        FHoudiniEngine::Get().GetSession(), HoudiniGeoPartObject.GeoId, PartInfo.id,
        HAPI_RSTORDER_DEFAULT, InstancerPartTransforms.GetData(), 0, PartInfo.instanceCount ), false );

    // Get the part ids for parts being instanced
    TArray<HAPI_PartId> InstancedPartIds;
    InstancedPartIds.SetNumZeroed( PartInfo.instancedPartCount );
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetInstancedPartIds(
        FHoudiniEngine::Get().GetSession(), HoudiniGeoPartObject.GeoId, PartInfo.id,
        InstancedPartIds.GetData(), 0, PartInfo.instancedPartCount ), false );

    // Convert the transform to Unreal's coordinate system
    TArray<FTransform> InstancerUnrealTransforms;
    InstancerUnrealTransforms.SetNumUninitialized( InstancerPartTransforms.Num() );
    for ( int32 InstanceIdx = 0; InstanceIdx < InstancerPartTransforms.Num(); ++InstanceIdx )
    {
        const auto& InstanceTransform = InstancerPartTransforms[InstanceIdx];
        FHoudiniEngineUtils::TranslateHapiTransform( InstanceTransform, InstancerUnrealTransforms[ InstanceIdx ] );
    }

    for ( auto InstancedPartId : InstancedPartIds )
    {
        // Create the GeoPartObject correspondin to the instanced part
        FHoudiniGeoPartObject InstancedPart( HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId, HoudiniGeoPartObject.GeoId, InstancedPartId );
        InstancedPart.TransformMatrix = HoudiniGeoPartObject.TransformMatrix;

        InstancedGeoPart.Add( InstancedPart );
        InstancedTransforms.Add( InstancerUnrealTransforms );
    }

    return true;
}

bool
FHoudiniEngineInstancerUtils::GetUnrealAttributeInstancerObjectsAndTransforms(
    const FHoudiniGeoPartObject& HoudiniGeoPartObject,
    const HAPI_NodeId& AssetId,
    TArray< UObject *>& InstancedObjects,
    TArray< TArray< FTransform > >& InstancedTransforms )
{
    if ( !HoudiniGeoPartObject.IsAttributeOverrideInstancer() )
        return false;

    // This is an attribute override. Unreal mesh is specified through an attribute and we use points.
    std::string MarshallingAttributeInstanceOverride = HAPI_UNREAL_ATTRIB_INSTANCE_OVERRIDE;
    UHoudiniRuntimeSettings::GetSettingsValue( TEXT( "MarshallingAttributeInstanceOverride" ), MarshallingAttributeInstanceOverride );

    HAPI_AttributeInfo ResultAttributeInfo;
    FHoudiniApi:: AttributeInfo_Init(&ResultAttributeInfo);
    //FMemory::Memzero< HAPI_AttributeInfo >( ResultAttributeInfo );
    if ( !HoudiniGeoPartObject.HapiGetAttributeInfo( AssetId, MarshallingAttributeInstanceOverride, ResultAttributeInfo ) )
    {
        // We had an error while retrieving the attribute info.
        return false;
    }

    // Attribute does not exist.
    if ( !ResultAttributeInfo.exists )
        return false;

    // The Attribute can only be on points or details
    if ( ( ResultAttributeInfo.owner != HAPI_ATTROWNER_DETAIL )
        && ( ResultAttributeInfo.owner != HAPI_ATTROWNER_POINT ) )
        return false;

    // Retrieve instance transforms (for each point).
    TArray< FTransform > AllTransforms;
    HoudiniGeoPartObject.HapiGetInstanceTransforms( AssetId, AllTransforms );

    if ( ResultAttributeInfo.owner == HAPI_ATTROWNER_DETAIL )
    {
        // Attribute is on detail, this means it gets applied to all points.
        TArray< FString > DetailInstanceValues;

        if ( !HoudiniGeoPartObject.HapiGetAttributeDataAsString(
            AssetId, MarshallingAttributeInstanceOverride,
            HAPI_ATTROWNER_DETAIL, ResultAttributeInfo, DetailInstanceValues ) )
        {
            // This should not happen - attribute exists, but there was an error retrieving it.
            return false;
        }

        if ( DetailInstanceValues.Num() <= 0 )
        {
            // No values specified.
            return false;
        }

        // Attempt to load specified asset.
        const FString & AssetName = DetailInstanceValues[ 0 ];
        UObject * AttributeObject = StaticLoadObject( UObject::StaticClass(), nullptr, *AssetName, nullptr, LOAD_None, nullptr );

        // Couldnt load the referenced object
        if ( !AttributeObject )
            return false;

        InstancedObjects.Add( AttributeObject );
        InstancedTransforms.Add( AllTransforms );
    }
    else
    {
        // Attribute is on points, so we may have different values for each of them
        TArray< FString > PointInstanceValues;
        if ( !HoudiniGeoPartObject.HapiGetAttributeDataAsString(
            AssetId, MarshallingAttributeInstanceOverride,
            HAPI_ATTROWNER_POINT, ResultAttributeInfo, PointInstanceValues ) )
        {
            // This should not happen - attribute exists, but there was an error retrieving it.
            return false;
        }

        // Attribute is on points, number of points must match number of transforms.
        if ( !ensure( PointInstanceValues.Num() == AllTransforms.Num() ) )
        {
            // This should not happen, we have mismatch between number of instance values and transforms.
            return false;
        }

        // If instance attribute exists on points, we need to get all unique values.
        // This will give us all the unique object we want to instance
        TMap< FString, UObject * > ObjectsToInstance;
        for ( auto Iter : PointInstanceValues )
        {
            const FString & UniqueName = *Iter;

            if ( !ObjectsToInstance.Contains( UniqueName ) )
            {
                // To avoid trying to load an object that fails multiple times, still add it to the array so we skip further attempts
                UObject * AttributeObject = StaticLoadObject( 
                    UObject::StaticClass(), nullptr, *UniqueName, nullptr, LOAD_None, nullptr );

                ObjectsToInstance.Add( UniqueName, AttributeObject );
            }
        }

        // Iterates through all the unique objects and get their corresponding transforms
        bool Success = false;
        for( auto Iter : ObjectsToInstance )
        {
            // Check we managed to load this object
            UObject * AttributeObject = Iter.Value;
            if ( !AttributeObject )
            continue;

            // Extract the transform values that correspond to this object
            const FString & InstancePath = Iter.Key;
            TArray< FTransform > ObjectTransforms;
            for (int32 Idx = 0; Idx < PointInstanceValues.Num(); ++Idx)
            {
                if ( InstancePath.Equals( PointInstanceValues[ Idx ] ) )
                    ObjectTransforms.Add( AllTransforms[ Idx ] );
            }

            InstancedObjects.Add( AttributeObject );
            InstancedTransforms.Add( ObjectTransforms );
            Success = true;
        }

        if ( !Success )
            return false;
    }

    return true;
}
    
bool
FHoudiniEngineInstancerUtils::CreateInstancerComponent(    
    UObject* InstancedObject,
    const TArray< FTransform >& InstancedObjectTransforms,
    const FHoudiniGeoPartObject& InstancerGeoPartObject,
    USceneComponent* ParentComponent,
    USceneComponent*& CreatedInstancedComponent )
{
    check( InstancedObject && ( InstancedObjectTransforms.Num() > 0 ) );

    if ( UStaticMesh * StaticMesh = Cast<UStaticMesh>( InstancedObject) )
    {
        UMaterialInterface * InstancerMaterial = nullptr;
    /*
        // We check attribute material first.
        if(InstancerGeoPartObject.bInstancerAttributeMaterialAvailable )
        {
            InstancerMaterial = Comp->GetAssignmentMaterial(
        InstancerGeoPartObject.InstancerAttributeMaterialName);
        }

        // If attribute material was not found, we check for presence of shop instancer material.
        if( !InstancerMaterial && InstancerHoudiniGeoPartObject.bInstancerMaterialAvailable )
            InstancerMaterial = Comp->GetAssignmentMaterial(
                InstancerHoudiniGeoPartObject.InstancerMaterialName);
    */

    if ( !FHoudiniEngineInstancerUtils::CreateInstancedStaticMeshComponent(
        StaticMesh, InstancedObjectTransforms, InstancerGeoPartObject, ParentComponent, CreatedInstancedComponent, InstancerMaterial ) )
        return false;
       
    }
    else
    {
        // Create the actor instancer component
    if (!FHoudiniEngineInstancerUtils::CreateInstancedActorComponent(
        InstancedObject, InstancedObjectTransforms, InstancerGeoPartObject, ParentComponent, CreatedInstancedComponent ) )
        return false;
    }

    // Update the component's UProperties
    FHoudiniEngineUtils::UpdateUPropertyAttributesOnObject( CreatedInstancedComponent, InstancerGeoPartObject );
    //FHoudiniEngineUtils::UpdateUPropertyAttributesOnObject(CreatedInstancedComponent, HoudiniGeoPartObject);


    // Update the component's relative Transform
    // UpdateRelativeTransform();
    CreatedInstancedComponent->SetRelativeTransform( InstancerGeoPartObject.TransformMatrix );

    return true;
}


bool
FHoudiniEngineInstancerUtils::CreateInstancedStaticMeshComponent(
    UStaticMesh* StaticMesh,
    const TArray< FTransform >& InstancedObjectTransforms,
    const FHoudiniGeoPartObject& InstancerGeoPartObject,
    USceneComponent* ParentComponent,
    USceneComponent*& CreatedInstancedComponent,
    UMaterialInterface * InstancerMaterial /*=nullptr*/ )
{
    if ( !StaticMesh )
        return false;

    if (!ParentComponent || ParentComponent->IsPendingKill())
        return false;

    if (!ParentComponent->GetOwner() || ParentComponent->GetOwner()->IsPendingKill())
        return false;

    UInstancedStaticMeshComponent * InstancedStaticMeshComponent = nullptr;
    if ( StaticMesh->GetNumLODs() > 1 )
    {
        // If the mesh has LODs, use Hierarchical ISMC
        InstancedStaticMeshComponent = NewObject< UHierarchicalInstancedStaticMeshComponent >(
            ParentComponent->GetOwner(), UHierarchicalInstancedStaticMeshComponent::StaticClass(), NAME_None, RF_Transactional);
    }
    else
    {
        // If the mesh doesnt have LOD, we can use a regular ISMC
        InstancedStaticMeshComponent = NewObject< UInstancedStaticMeshComponent >(
            ParentComponent->GetOwner(), UInstancedStaticMeshComponent::StaticClass(), NAME_None, RF_Transactional);
    }

    if ( !InstancedStaticMeshComponent )
    return false;

    InstancedStaticMeshComponent->SetStaticMesh( StaticMesh );
    InstancedStaticMeshComponent->GetBodyInstance()->bAutoWeld = false;
    if (InstancerMaterial)
    {
        InstancedStaticMeshComponent->OverrideMaterials.Empty();

        int32 MeshMaterialCount = StaticMesh->StaticMaterials.Num();
        for (int32 Idx = 0; Idx < MeshMaterialCount; ++Idx)
            InstancedStaticMeshComponent->SetMaterial(Idx, InstancerMaterial);
    }

    // Now add the instances themselves
    // TODO: We should be calling  UHoudiniInstancedActorComponent::UpdateInstancerComponentInstances( ... )
    InstancedStaticMeshComponent->ClearInstances();
    for (const auto& Transform : InstancedObjectTransforms )
    {
        InstancedStaticMeshComponent->AddInstance( Transform );
    }

    // Assign the ISMC / HISMC to the SceneCOmponent we return
    CreatedInstancedComponent = InstancedStaticMeshComponent;

    CreatedInstancedComponent->SetMobility( ParentComponent->Mobility );
    CreatedInstancedComponent->AttachToComponent( ParentComponent, FAttachmentTransformRules::KeepRelativeTransform );
    CreatedInstancedComponent->RegisterComponent();

    // We want to make this invisible if it's a collision instancer.
    CreatedInstancedComponent->SetVisibility( !InstancerGeoPartObject.bIsCollidable );

    /*
    FHoudiniEngineUtils::UpdateUPropertyAttributesOnObject(CreatedInstancedComponent, InstancerGeoPartObject);
    // UpdateRelativeTransform();
    CreatedInstancerComponent->SetRelativeTransform(InstancerGeoPartObject.TransformMatrix);
    */
    return true;
}

bool
FHoudiniEngineInstancerUtils::CreateInstancedActorComponent(
    UObject* InstancedObject,
    const TArray< FTransform >& InstancedObjectTransforms,
    const FHoudiniGeoPartObject& InstancerGeoPartObject,
    USceneComponent* ParentComponent,
    USceneComponent*& CreatedInstancedComponent )
{
    // Create the actor instancer component
    UHoudiniInstancedActorComponent * InstancedObjectComponent = NewObject< UHoudiniInstancedActorComponent >(
        ParentComponent->GetOwner(), UHoudiniInstancedActorComponent::StaticClass(), NAME_None, RF_Transactional);

    if ( !InstancedObjectComponent || InstancedObjectComponent->IsPendingKill() )
        return false;

    InstancedObjectComponent->InstancedAsset = InstancedObject;

    // Now add the instances themselves
    // TODO: We should be calling  UHoudiniInstancedActorComponent::UpdateInstancerComponentInstances( ... )
    InstancedObjectComponent->SetInstances( InstancedObjectTransforms );

    // Assign the HIAC to the SceneCOmponent we return
    CreatedInstancedComponent = InstancedObjectComponent;

    CreatedInstancedComponent->SetMobility(ParentComponent->Mobility);
    CreatedInstancedComponent->AttachToComponent( ParentComponent, FAttachmentTransformRules::KeepRelativeTransform );
    CreatedInstancedComponent->RegisterComponent();

    /*
    FHoudiniEngineUtils::UpdateUPropertyAttributesOnObject( InstancedObjectComponent, HoudiniGeoPartObject );
    UpdateRelativeTransform();
    */

    return true;
}

#undef LOCTEXT_NAMESPACE