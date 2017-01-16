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
* Produced by:
*      Mykola Konyk
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetInstanceInputField.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniInstancedActorComponent.h"


// Fastrand is a faster alternative to std::rand()
// and doesn't oscillate when looking for 2 values like Unreal's.
inline int fastrand(int& nSeed)
{
    nSeed = (214013 * nSeed + 2531011);
    return (nSeed >> 16) & 0x7FFF;
}

bool
FHoudiniAssetInstanceInputFieldSortPredicate::operator()(
    const UHoudiniAssetInstanceInputField & A,
    const UHoudiniAssetInstanceInputField & B ) const
{
    FHoudiniGeoPartObjectSortPredicate HoudiniGeoPartObjectSortPredicate;
    return HoudiniGeoPartObjectSortPredicate( A.GetHoudiniGeoPartObject(), B.GetHoudiniGeoPartObject() );
}

UHoudiniAssetInstanceInputField::UHoudiniAssetInstanceInputField( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
    , OriginalObject( nullptr )
    , HoudiniAssetComponent( nullptr )
    , HoudiniAssetInstanceInput( nullptr )
    , HoudiniAssetInstanceInputFieldFlagsPacked( 0 )
{}

UHoudiniAssetInstanceInputField *
UHoudiniAssetInstanceInputField::Create(
    UHoudiniAssetComponent * HoudiniAssetComponent,
    UHoudiniAssetInstanceInput * InHoudiniAssetInstanceInput,
    const FHoudiniGeoPartObject & HoudiniGeoPartObject )
{
    UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField =
        NewObject< UHoudiniAssetInstanceInputField >(
            HoudiniAssetComponent,
            UHoudiniAssetInstanceInputField::StaticClass(),
            NAME_None,
            RF_Public | RF_Transactional );

    HoudiniAssetInstanceInputField->HoudiniGeoPartObject = HoudiniGeoPartObject;
    HoudiniAssetInstanceInputField->HoudiniAssetComponent = HoudiniAssetComponent;
    HoudiniAssetInstanceInputField->HoudiniAssetInstanceInput = InHoudiniAssetInstanceInput;

    return HoudiniAssetInstanceInputField;
}

UHoudiniAssetInstanceInputField *
UHoudiniAssetInstanceInputField::Create(
    UHoudiniAssetComponent * InHoudiniAssetComponent,
    const UHoudiniAssetInstanceInputField * OtherInputField )
{
    UHoudiniAssetInstanceInputField * InputField = DuplicateObject< UHoudiniAssetInstanceInputField >( OtherInputField, InHoudiniAssetComponent );

    InputField->HoudiniAssetComponent = InHoudiniAssetComponent;

    InputField->InstancerComponents.Empty();

    // Duplicate the given field's InstancedStaticMesh components
    for ( const USceneComponent* OtherISMC : OtherInputField->InstancerComponents )
    {
        USceneComponent* NewISMC = DuplicateObject< USceneComponent >( OtherISMC, InHoudiniAssetComponent );
        NewISMC->RegisterComponent();
        NewISMC->AttachToComponent( InHoudiniAssetComponent, FAttachmentTransformRules::KeepRelativeTransform );
        InputField->InstancerComponents.Add( NewISMC );
    }

    return InputField;
}

void
UHoudiniAssetInstanceInputField::Serialize( FArchive & Ar )
{
    // Call base implementation first.
    Super::Serialize( Ar );

    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );
    const int32 LinkerVersion = GetLinkerCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    Ar << HoudiniAssetInstanceInputFieldFlagsPacked;
    Ar << HoudiniGeoPartObject;

    FString UnusedInstancePathName;
    Ar << UnusedInstancePathName;
    Ar << RotationOffsets;
    Ar << ScaleOffsets;
    Ar << bScaleOffsetsLinearlyArray;

    Ar << InstancedTransforms;
    Ar << VariationTransformsArray;

    Ar << InstancerComponents;
    Ar << InstancedObjects;
    Ar << OriginalObject;
}

void
UHoudiniAssetInstanceInputField::AddReferencedObjects( UObject * InThis, FReferenceCollector & Collector )
{
    UHoudiniAssetInstanceInputField * This = Cast< UHoudiniAssetInstanceInputField >( InThis );
    if ( This )
    {
        if ( This->OriginalObject )
            Collector.AddReferencedObject( This->OriginalObject, This );

        Collector.AddReferencedObjects( This->InstancedObjects, This );
        Collector.AddReferencedObjects( This->InstancerComponents, This );
    }

    // Call base implementation.
    Super::AddReferencedObjects( InThis, Collector );
}

void
UHoudiniAssetInstanceInputField::BeginDestroy()
{
    for ( USceneComponent* Comp : InstancerComponents )
    {
        if ( Comp )
        {
            Comp->UnregisterComponent();
            Comp->DetachFromComponent( FDetachmentTransformRules::KeepRelativeTransform );
            Comp->DestroyComponent();
        }
    }

    Super::BeginDestroy();
}

#if WITH_EDITOR

void
UHoudiniAssetInstanceInputField::PostEditUndo()
{
    Super::PostEditUndo();

    int32 VariationCount = InstanceVariationCount();
    for ( int32 Idx = 0; Idx < VariationCount; Idx++ )
    {
        if ( ensure( InstancedObjects.IsValidIndex( Idx ) ) && ensure( InstancerComponents.IsValidIndex( Idx ) ) )
        {
            if ( UStaticMesh* StaticMesh = Cast<UStaticMesh>( InstancedObjects[ Idx ] ) )
            {
                if ( UInstancedStaticMeshComponent* Comp = Cast<UInstancedStaticMeshComponent>( InstancerComponents[ Idx ]) )
                {
                    Comp->SetStaticMesh( StaticMesh );
                }
            }
            else
            {
                if ( UHoudiniInstancedActorComponent* Comp = Cast<UHoudiniInstancedActorComponent>( InstancerComponents[ Idx ] ) )
                {
                    Comp->InstancedAsset = InstancedObjects[ Idx ];
                }
            }
        }
    }

    UpdateInstanceTransforms( true );

    if ( HoudiniAssetComponent )
        HoudiniAssetComponent->UpdateEditorProperties( false );
}

#endif // WITH_EDITOR

void
UHoudiniAssetInstanceInputField::AddInstanceComponent( int32 VariationIdx )
{
    check( InstancedObjects.Num() > 0 && ( VariationIdx < InstancedObjects.Num() ) );
    check( HoudiniAssetComponent );

    if ( UStaticMesh * StaticMesh = Cast<UStaticMesh>( InstancedObjects[ VariationIdx ] ) )
    {
        UInstancedStaticMeshComponent * InstancedStaticMeshComponent =
            NewObject< UInstancedStaticMeshComponent >(
                HoudiniAssetComponent->GetOwner(),
                UInstancedStaticMeshComponent::StaticClass(),
                NAME_None, RF_Transactional );

        InstancerComponents.Insert( InstancedStaticMeshComponent, VariationIdx );

        InstancedStaticMeshComponent->SetStaticMesh( StaticMesh );
        InstancedStaticMeshComponent->SetMobility( HoudiniAssetComponent->Mobility );
        InstancedStaticMeshComponent->AttachToComponent(
            HoudiniAssetComponent, FAttachmentTransformRules::KeepRelativeTransform );
        InstancedStaticMeshComponent->RegisterComponent();
        InstancedStaticMeshComponent->GetBodyInstance()->bAutoWeld = false;

        // We want to make this invisible if it's a collision instancer.
        InstancedStaticMeshComponent->SetVisibility( !HoudiniGeoPartObject.bIsCollidable );

        // Check if instancer material is available.
        const FHoudiniGeoPartObject & InstancerHoudiniGeoPartObject = HoudiniAssetInstanceInput->HoudiniGeoPartObject;
        if ( StaticMesh )
        {
            UMaterialInterface * InstancerMaterial = nullptr;

            // We check attribute material first.
            if ( InstancerHoudiniGeoPartObject.bInstancerAttributeMaterialAvailable )
            {
                InstancerMaterial = HoudiniAssetComponent->GetAssignmentMaterial(
                    InstancerHoudiniGeoPartObject.InstancerAttributeMaterialName );
            }

            // If attribute material was not found, we check for presence of shop instancer material.
            if ( !InstancerMaterial && InstancerHoudiniGeoPartObject.bInstancerMaterialAvailable )
                InstancerMaterial = HoudiniAssetComponent->GetAssignmentMaterial(
                    InstancerHoudiniGeoPartObject.InstancerMaterialName );

            if ( InstancerMaterial )
            {
                InstancedStaticMeshComponent->OverrideMaterials.Empty();

                int32 MeshMaterialCount = StaticMesh->StaticMaterials.Num();
                for ( int32 Idx = 0; Idx < MeshMaterialCount; ++Idx )
                    InstancedStaticMeshComponent->SetMaterial( Idx, InstancerMaterial );
            }
        }
    }
    else
    {
        // Create the actor instancer component
        UHoudiniInstancedActorComponent * InstancedObjectComponent =
            NewObject< UHoudiniInstancedActorComponent >(
                HoudiniAssetComponent->GetOwner(),
                UHoudiniInstancedActorComponent::StaticClass(),
                NAME_None, RF_Transactional );

        InstancerComponents.Insert( InstancedObjectComponent, VariationIdx );
        InstancedObjectComponent->InstancedAsset = InstancedObjects[ VariationIdx ];
        InstancedObjectComponent->SetMobility( HoudiniAssetComponent->Mobility );
        InstancedObjectComponent->AttachToComponent(
            HoudiniAssetComponent, FAttachmentTransformRules::KeepRelativeTransform );
        InstancedObjectComponent->RegisterComponent();
    }

    UpdateRelativeTransform();
}

void
UHoudiniAssetInstanceInputField::SetInstanceTransforms( const TArray< FTransform > & ObjectTransforms )
{
    InstancedTransforms = ObjectTransforms;
    UpdateInstanceTransforms( true );
}

void
UHoudiniAssetInstanceInputField::UpdateInstanceTransforms( bool RecomputeVariationAssignments )
{
    int32 NumInstancTransforms = InstancedTransforms.Num();
    int32 VariationCount = InstanceVariationCount();

    int nSeed = 1234;
    if ( RecomputeVariationAssignments )
    {
        //clear the previous cached transform assignments.
        for ( int32 Idx = 0; Idx < VariationTransformsArray.Num(); Idx++ )
        {
            VariationTransformsArray[ Idx ].Empty();
        }

        VariationTransformsArray.Empty();

        for ( int32 Idx = 0; Idx < VariationCount; Idx++ )
        {
            TArray< FTransform > VariationTransforms;
            VariationTransformsArray.Add( VariationTransforms );
        }

        for ( int32 Idx = 0; Idx < NumInstancTransforms; Idx++ )
        {
            FTransform Xform = InstancedTransforms[ Idx ];
            int32 VariationIndex = fastrand(nSeed) % VariationCount;
            VariationTransformsArray[ VariationIndex ].Add( Xform );
        }
    }

    for ( int32 Idx = 0; Idx < VariationCount; Idx++ )
    {
        FHoudiniEngineUtils::UpdateInstancedStaticMeshComponentInstances(
            InstancerComponents[ Idx ],
            VariationTransformsArray[ Idx ],
            RotationOffsets[ Idx ],
            ScaleOffsets[ Idx ] );
    }
}

void
UHoudiniAssetInstanceInputField::UpdateRelativeTransform()
{
    int32 VariationCount = InstanceVariationCount();
    for ( int32 Idx = 0; Idx < VariationCount; Idx++ )
        InstancerComponents[ Idx ]->SetRelativeTransform( HoudiniGeoPartObject.TransformMatrix );
}

const FHoudiniGeoPartObject &
UHoudiniAssetInstanceInputField::GetHoudiniGeoPartObject() const
{
    return HoudiniGeoPartObject;
}

void 
UHoudiniAssetInstanceInputField::SetGeoPartObject( const FHoudiniGeoPartObject & InHoudiniGeoPartObject )
{
    HoudiniGeoPartObject = InHoudiniGeoPartObject;
}

UObject* 
UHoudiniAssetInstanceInputField::GetOriginalObject() const
{
    return OriginalObject;
}

UObject * 
UHoudiniAssetInstanceInputField::GetInstanceVariation( int32 VariationIndex ) const
{
    if ( VariationIndex < 0 || VariationIndex >= InstancedObjects.Num() )
        return nullptr;

    UObject * Obj = InstancedObjects[VariationIndex];
    return Obj;
}

void
UHoudiniAssetInstanceInputField::AddInstanceVariation( UObject * InObject, int32 VariationIdx )
{
    check( InObject );
    check( HoudiniAssetComponent );

    InstancedObjects.Insert( InObject, VariationIdx );
    RotationOffsets.Insert( FRotator( 0, 0, 0 ), VariationIdx );
    ScaleOffsets.Insert( FVector( 1, 1, 1 ), VariationIdx );
    bScaleOffsetsLinearlyArray.Insert( true, VariationIdx );

    AddInstanceComponent( VariationIdx );
    UpdateInstanceTransforms( true );
}

void
UHoudiniAssetInstanceInputField::RemoveInstanceVariation( int32 VariationIdx )
{
    check( VariationIdx >= 0 && VariationIdx < InstanceVariationCount() );

    if ( InstanceVariationCount() == 1 )
        return;

    bool bIsStaticMesh = Cast<UStaticMesh>( InstancedObjects[ VariationIdx ] ) != nullptr;
    InstancedObjects.RemoveAt( VariationIdx );
    RotationOffsets.RemoveAt( VariationIdx );
    ScaleOffsets.RemoveAt( VariationIdx );
    bScaleOffsetsLinearlyArray.RemoveAt( VariationIdx );

    // Remove instanced component.
    if ( USceneComponent* Comp = InstancerComponents[ VariationIdx ] )
    {
        Comp->DestroyComponent();
    }
    InstancerComponents.RemoveAt( VariationIdx );

    UpdateInstanceTransforms( true );
}

void
UHoudiniAssetInstanceInputField::ReplaceInstanceVariation( UObject * InObject, int Index )
{
    check( InObject );
    check( Index >= 0 && Index < InstancedObjects.Num() );
    check( InstancerComponents.Num() == InstancedObjects.Num() );

    // Check if the replacing object and the current object are different types (StaticMesh vs. Non)
    // if so we need to swap out the component 
    bool bInIsStaticMesh = InObject->IsA<UStaticMesh>();
    bool bCurrentIsStaticMesh = InstancedObjects[ Index ]->IsA<UStaticMesh>();
    InstancedObjects[ Index ] = InObject;

    bool bComponentNeedToBeCreated = true;
    if (bInIsStaticMesh == bCurrentIsStaticMesh)
    {
        // We'll try to reuse the InstanceComponent
        if ( UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>( InstancerComponents[ Index ] ) )
        {
            if ( !ISMC->IsPendingKill() )
            {
                ISMC->SetStaticMesh( Cast<UStaticMesh>( InObject ) ); 
                bComponentNeedToBeCreated = false;
            }
        }
        else if ( UHoudiniInstancedActorComponent* IAC = Cast<UHoudiniInstancedActorComponent>( InstancerComponents[ Index ] ) )
        {
            if ( !IAC->IsPendingKill() )
            {
                IAC->InstancedAsset = InObject;
                bComponentNeedToBeCreated = false;
            }
        }
    }

    if ( bComponentNeedToBeCreated )
    {
        // We'll create a new InstanceComponent
        FTransform SavedXform = InstancerComponents[ Index ]->GetRelativeTransform();
        InstancerComponents[ Index ]->DestroyComponent();
        InstancerComponents.RemoveAt( Index );
        AddInstanceComponent( Index );
        InstancerComponents[ Index ]->SetRelativeTransform( SavedXform );
    }

    UpdateInstanceTransforms( false );
}

void
UHoudiniAssetInstanceInputField::FindObjectIndices( UObject * InStaticMesh, TArray< int32 > & Indices )
{
    for ( int32 Idx = 0; Idx < InstancedObjects.Num(); ++Idx )
    {
        if ( InstancedObjects[ Idx ] == InStaticMesh )
            Indices.Add( Idx );
    }
}

int32
UHoudiniAssetInstanceInputField::InstanceVariationCount() const
{
    return InstancedObjects.Num();
}

#if WITH_EDITOR

void
UHoudiniAssetInstanceInputField::AssignThumbnailBorder( TSharedPtr< SBorder > InThumbnailBorder )
{
    ThumbnailBorder = InThumbnailBorder;
}

TSharedPtr< SBorder >
UHoudiniAssetInstanceInputField::GetThumbnailBorder() const
{
    return ThumbnailBorder;
}

void
UHoudiniAssetInstanceInputField::AssignComboButton( TSharedPtr< SComboButton > InComboButton )
{
    StaticMeshComboButton = InComboButton;
}

TSharedPtr< SComboButton >
UHoudiniAssetInstanceInputField::GetComboButton() const
{
    return StaticMeshComboButton;
}

#endif

const FRotator &
UHoudiniAssetInstanceInputField::GetRotationOffset( int32 VariationIdx ) const
{
    return RotationOffsets[ VariationIdx ];
}

void
UHoudiniAssetInstanceInputField::SetRotationOffset( const FRotator & Rotator, int32 VariationIdx )
{
    RotationOffsets[ VariationIdx ] = Rotator;
}

const FVector &
UHoudiniAssetInstanceInputField::GetScaleOffset( int32 VariationIdx ) const
{
    return ScaleOffsets[ VariationIdx ];
}

void
UHoudiniAssetInstanceInputField::SetScaleOffset( const FVector & InScale, int32 VariationIdx )
{
    ScaleOffsets[ VariationIdx ] = InScale;
}

bool
UHoudiniAssetInstanceInputField::AreOffsetsScaledLinearly( int32 VariationIdx ) const
{
    return bScaleOffsetsLinearlyArray[ VariationIdx ];
}

void
UHoudiniAssetInstanceInputField::SetLinearOffsetScale( bool bEnabled, int32 VariationIdx )
{
    bScaleOffsetsLinearlyArray[ VariationIdx ] = bEnabled;
}

bool
UHoudiniAssetInstanceInputField::IsOriginalObjectUsed( int32 VariationIdx ) const
{
    check( VariationIdx >= 0 && VariationIdx < InstancedObjects.Num() );
    return OriginalObject == InstancedObjects[ VariationIdx ];
}

USceneComponent *
UHoudiniAssetInstanceInputField::GetInstancedComponent( int32 VariationIdx ) const
{
    check( VariationIdx >= 0 && VariationIdx < InstancerComponents.Num() );
    return InstancerComponents[ VariationIdx ];
}

const TArray< FTransform > &
UHoudiniAssetInstanceInputField::GetInstancedTransforms( int32 VariationIdx ) const
{
    check( VariationIdx >= 0 && VariationIdx < VariationTransformsArray.Num() );
    return VariationTransformsArray[ VariationIdx ];
}

void
UHoudiniAssetInstanceInputField::RecreateRenderState()
{
    check( InstancerComponents.Num() == InstancedObjects.Num() );
    for ( auto Comp : InstancerComponents )
    {
        if ( UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>( Comp ) )
        {
            ISMC->RecreateRenderState_Concurrent();
        }
    }
}

void
UHoudiniAssetInstanceInputField::RecreatePhysicsState()
{
    check( InstancerComponents.Num() == InstancedObjects.Num() );
    for ( auto Comp : InstancerComponents )
    {
        if ( UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>( Comp ) )
        {
            ISMC->RecreatePhysicsState();
        }
    }
}

bool
UHoudiniAssetInstanceInputField::GetMaterialReplacementMeshes(
    UMaterialInterface * Material,
    TMap< UStaticMesh *, int32 > & MaterialReplacementsMap )
{
    bool bResult = false;

    for ( int32 Idx = 0; Idx < InstancedObjects.Num(); ++Idx )
    {
        UStaticMesh * StaticMesh = Cast<UStaticMesh>(InstancedObjects[ Idx ]);
        if ( StaticMesh && StaticMesh == OriginalObject )
        {
            if ( UInstancedStaticMeshComponent * InstancedStaticMeshComponent = Cast<UInstancedStaticMeshComponent>( InstancerComponents[ Idx ] ) )
            {
                const TArray< class UMaterialInterface * > & OverrideMaterials =
                    InstancedStaticMeshComponent->OverrideMaterials;
                for ( int32 MaterialIdx = 0; MaterialIdx < OverrideMaterials.Num(); ++MaterialIdx )
                {
                    UMaterialInterface * OverridenMaterial = OverrideMaterials[ MaterialIdx ];
                    if ( OverridenMaterial && OverridenMaterial == Material )
                    {
                        if ( MaterialIdx < StaticMesh->StaticMaterials.Num() )
                        {
                            MaterialReplacementsMap.Add( StaticMesh, MaterialIdx );
                            bResult = true;
                        }
                    }
                }
            }
        }
    }

    return bResult;
}

void
UHoudiniAssetInstanceInputField::FixInstancedObjects( const TMap<UObject*, UObject*>& ReplacementMap )
{
    if ( OriginalObject )
    {
        UObject *const *ReplacementObj = ReplacementMap.Find( OriginalObject );
        if( ReplacementObj )
        {
            OriginalObject = *ReplacementObj;
        }
    }

    int32 VariationCount = InstanceVariationCount();
    for( int32 Idx = 0; Idx < VariationCount; Idx++ )
    {
        UObject *const *ReplacementObj = ReplacementMap.Find( InstancedObjects[ Idx ] );
        if( ReplacementObj && *ReplacementObj )
        {
            InstancedObjects[ Idx ] = *ReplacementObj;
            if( UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>( InstancerComponents[ Idx ] ) )
            {
                ISMC->SetStaticMesh( CastChecked<UStaticMesh>( *ReplacementObj ) );
            }
            else if( UHoudiniInstancedActorComponent* IAC = Cast<UHoudiniInstancedActorComponent>( InstancerComponents[ Idx ] ) )
            {
                IAC->InstancedAsset = *ReplacementObj;
            }
        }
    }
}
