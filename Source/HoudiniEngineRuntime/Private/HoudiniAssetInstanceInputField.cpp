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
#include "HoudiniEngine.h"
#include "HoudiniAssetInstanceInputField.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniInstancedActorComponent.h"
#include "HoudiniMeshSplitInstancerComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"


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
    UObject * HoudiniAssetComponent,
    UHoudiniAssetInstanceInput * InHoudiniAssetInstanceInput,
    const FHoudiniGeoPartObject & HoudiniGeoPartObject )
{
	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return nullptr;

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
    UObject * InPrimaryObject,
    const UHoudiniAssetInstanceInputField * OtherInputField )
{
    UHoudiniAssetInstanceInputField * InputField = DuplicateObject< UHoudiniAssetInstanceInputField >( OtherInputField, InPrimaryObject );

    InputField->HoudiniAssetComponent = InPrimaryObject;

    InputField->InstancerComponents.Empty();

    // Duplicate the given field's InstancedStaticMesh components
    if( USceneComponent* InRootComp = Cast<USceneComponent>( InPrimaryObject ) )
    {
        for( const USceneComponent* OtherISMC : OtherInputField->InstancerComponents )
        {
            USceneComponent* NewISMC = DuplicateObject< USceneComponent >( OtherISMC, InRootComp );
            NewISMC->RegisterComponent();
            NewISMC->AttachToComponent( InRootComp, FAttachmentTransformRules::KeepRelativeTransform );
            InputField->InstancerComponents.Add( NewISMC );
        }
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

    if ( Ar.IsSaving() || ( Ar.IsLoading() && LinkerVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_INSTANCE_COLORS ) )
    {
        Ar << InstanceColorOverride;
        Ar << VariationInstanceColorOverrideArray;
    }

    Ar << InstancerComponents;
    Ar << InstancedObjects;
    Ar << OriginalObject;
}

void
UHoudiniAssetInstanceInputField::AddReferencedObjects( UObject * InThis, FReferenceCollector & Collector )
{
    UHoudiniAssetInstanceInputField * ThisHAIF = Cast< UHoudiniAssetInstanceInputField >( InThis );
    if ( ThisHAIF && !ThisHAIF->IsPendingKill() )
    {
        if ( ThisHAIF->OriginalObject && !ThisHAIF->OriginalObject->IsPendingKill() )
            Collector.AddReferencedObject(ThisHAIF->OriginalObject, ThisHAIF);

        Collector.AddReferencedObjects(ThisHAIF->InstancedObjects, ThisHAIF);
        Collector.AddReferencedObjects(ThisHAIF->InstancerComponents, ThisHAIF);
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
            UStaticMesh* StaticMesh = Cast<UStaticMesh>(InstancedObjects[Idx]);
            if ( StaticMesh && !StaticMesh->IsPendingKill())
            {
                UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(InstancerComponents[Idx]);
                if ( ISMC && !ISMC->IsPendingKill() )
                {
                    ISMC->SetStaticMesh( StaticMesh );
                }
                else
                {
                    UHoudiniMeshSplitInstancerComponent* MSIC = Cast<UHoudiniMeshSplitInstancerComponent>(InstancerComponents[Idx]);
                    if ( MSIC && !MSIC->IsPendingKill() )
                        MSIC->SetStaticMesh( StaticMesh );
                }
            }
            else
            {
                UHoudiniInstancedActorComponent* IAC = Cast<UHoudiniInstancedActorComponent>(InstancerComponents[Idx]);
                if ( IAC && !IAC->IsPendingKill() )
                {
                    IAC->InstancedAsset = InstancedObjects[ Idx ];
                }
            }
        }
    }

    UpdateInstanceTransforms( true );

    UHoudiniAssetComponent* HAC = Cast<UHoudiniAssetComponent>(HoudiniAssetComponent);
    if (  HAC && !HAC->IsPendingKill() )
        HAC->UpdateEditorProperties( false );

    UpdateInstanceUPropertyAttributes();
}

#endif // WITH_EDITOR

void
UHoudiniAssetInstanceInputField::AddInstanceComponent( int32 VariationIdx )
{
    if ( !InstancedObjects.IsValidIndex( VariationIdx ) )
        return;

    UHoudiniAssetComponent* Comp = Cast<UHoudiniAssetComponent>( HoudiniAssetComponent );
    if( !Comp || Comp->IsPendingKill() )
        return;

    USceneComponent* RootComp = Comp;
	if (!RootComp->GetOwner() || RootComp->GetOwner()->IsPendingKill())
		return;

    // Check if instancer material is available.
    const FHoudiniGeoPartObject & InstancerHoudiniGeoPartObject = HoudiniAssetInstanceInput->HoudiniGeoPartObject;

    UStaticMesh * StaticMesh = Cast<UStaticMesh>(InstancedObjects[VariationIdx]);
    if ( StaticMesh && !StaticMesh->IsPendingKill() )
    {
        UMaterialInterface * InstancerMaterial = nullptr;

        // We check attribute material first.
        if( InstancerHoudiniGeoPartObject.bInstancerAttributeMaterialAvailable )
        {
            InstancerMaterial = Comp->GetAssignmentMaterial(
                InstancerHoudiniGeoPartObject.InstancerAttributeMaterialName);
        }

        // If attribute material was not found, we check for presence of shop instancer material.
        if( !InstancerMaterial && InstancerHoudiniGeoPartObject.bInstancerMaterialAvailable )
            InstancerMaterial = Comp->GetAssignmentMaterial(
                InstancerHoudiniGeoPartObject.InstancerMaterialName);

        USceneComponent* NewComp = nullptr;
        if( HoudiniAssetInstanceInput->Flags.bIsSplitMeshInstancer )
        {
            UHoudiniMeshSplitInstancerComponent* MSIC = NewObject< UHoudiniMeshSplitInstancerComponent >(
                RootComp->GetOwner(), UHoudiniMeshSplitInstancerComponent::StaticClass(),
                NAME_None, RF_Transactional);

            if ( MSIC && !MSIC->IsPendingKill() )
            {
                MSIC->SetStaticMesh(StaticMesh);
                MSIC->SetOverrideMaterial(InstancerMaterial);

                // Check for instance colors
                HAPI_AttributeInfo AttributeInfo = {};
                if ( HAPI_RESULT_SUCCESS == FHoudiniApi::GetAttributeInfo(
                    FHoudiniEngine::Get().GetSession(), InstancerHoudiniGeoPartObject.GeoId, InstancerHoudiniGeoPartObject.PartId,
                    HAPI_UNREAL_ATTRIB_INSTANCE_COLOR, HAPI_AttributeOwner::HAPI_ATTROWNER_PRIM, &AttributeInfo))
                {
                    if ( AttributeInfo.exists )
                    {
                        if ( AttributeInfo.tupleSize == 4 )
                        {
                            // Allocate sufficient buffer for data.
                            InstanceColorOverride.SetNumUninitialized(AttributeInfo.count);

                            if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetAttributeFloatData(
                                FHoudiniEngine::Get().GetSession(), InstancerHoudiniGeoPartObject.GeoId, InstancerHoudiniGeoPartObject.PartId,
                                HAPI_UNREAL_ATTRIB_INSTANCE_COLOR, &AttributeInfo, -1, (float*)InstanceColorOverride.GetData(), 0, AttributeInfo.count))
                            {
                                // got some override colors
                            }
                        }
                        else
                        {
                            HOUDINI_LOG_WARNING(TEXT(HAPI_UNREAL_ATTRIB_INSTANCE_COLOR " must be a float[4] prim attribute"));
                        }
                    }
                }
                NewComp = MSIC;
            }
        }
        else
        {
            UInstancedStaticMeshComponent * InstancedStaticMeshComponent = nullptr;
            if ( StaticMesh->GetNumLODs() > 1 )
            {
                // If the mesh has LODs, use Hierarchical ISMC
                InstancedStaticMeshComponent = NewObject< UHierarchicalInstancedStaticMeshComponent >(
                    RootComp->GetOwner(), UHierarchicalInstancedStaticMeshComponent::StaticClass(), NAME_None, RF_Transactional);
            }
            else
            {
                // If the mesh doesnt have LOD, we can use a regular ISMC
                InstancedStaticMeshComponent = NewObject< UInstancedStaticMeshComponent >(
                    RootComp->GetOwner(),UInstancedStaticMeshComponent::StaticClass(), NAME_None, RF_Transactional );
            }

            if ( InstancedStaticMeshComponent && !InstancedStaticMeshComponent->IsPendingKill() )
            {
                InstancedStaticMeshComponent->SetStaticMesh(StaticMesh);
                InstancedStaticMeshComponent->GetBodyInstance()->bAutoWeld = false;

                // Copy the CollisionTraceFlag from the SM
                if ( InstancedStaticMeshComponent->GetBodySetup() && StaticMesh->BodySetup )
                    InstancedStaticMeshComponent->GetBodySetup()->CollisionTraceFlag = StaticMesh->BodySetup->CollisionTraceFlag;

                if ( InstancerMaterial && !InstancerMaterial->IsPendingKill() )
                {
                    InstancedStaticMeshComponent->OverrideMaterials.Empty();

                    int32 MeshMaterialCount = StaticMesh->StaticMaterials.Num();
                    for (int32 Idx = 0; Idx < MeshMaterialCount; ++Idx)
                        InstancedStaticMeshComponent->SetMaterial(Idx, InstancerMaterial);
                }
                NewComp = InstancedStaticMeshComponent;
            }
        }

        if ( NewComp && !NewComp->IsPendingKill() )
        {
            NewComp->SetMobility(RootComp->Mobility);
            NewComp->AttachToComponent(RootComp, FAttachmentTransformRules::KeepRelativeTransform);
            NewComp->RegisterComponent();
            // We want to make this invisible if it's a collision instancer.
            NewComp->SetVisibility(!HoudiniGeoPartObject.bIsCollidable);

            InstancerComponents.Insert(NewComp, VariationIdx);
            FHoudiniEngineUtils::UpdateUPropertyAttributesOnObject(NewComp, InstancerHoudiniGeoPartObject);
        }
    }
    else
    {
        // Create the actor instancer component
        UHoudiniInstancedActorComponent * InstancedObjectComponent = NewObject< UHoudiniInstancedActorComponent >(
            RootComp->GetOwner(), UHoudiniInstancedActorComponent::StaticClass(), NAME_None, RF_Transactional );

        if  ( InstancedObjectComponent && !InstancedObjectComponent->IsPendingKill() )
        {
            InstancerComponents.Insert(InstancedObjectComponent, VariationIdx);
            InstancedObjectComponent->InstancedAsset = InstancedObjects[VariationIdx];
            InstancedObjectComponent->SetMobility(RootComp->Mobility);
            InstancedObjectComponent->AttachToComponent(RootComp, FAttachmentTransformRules::KeepRelativeTransform);
            InstancedObjectComponent->RegisterComponent();

            FHoudiniEngineUtils::UpdateUPropertyAttributesOnObject(InstancedObjectComponent, HoudiniGeoPartObject);
        }
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
    int32 VariationCount = InstanceVariationCount();

    int nSeed = 1234;
    if ( RecomputeVariationAssignments )
    {
        VariationTransformsArray.Empty();
        VariationTransformsArray.SetNum(VariationCount);

        VariationInstanceColorOverrideArray.Empty();
        VariationInstanceColorOverrideArray.SetNum(VariationCount);

        for ( int32 Idx = 0; Idx < InstancedTransforms.Num(); Idx++ )
        {
            int32 VariationIndex = fastrand(nSeed) % VariationCount;
            if ( VariationTransformsArray.IsValidIndex(VariationIndex) )
                VariationTransformsArray[ VariationIndex ].Add(InstancedTransforms[Idx]);

            if( InstanceColorOverride.Num() > Idx )
            {
                if ( VariationInstanceColorOverrideArray.IsValidIndex( VariationIndex )
                    && InstanceColorOverride.IsValidIndex(Idx) )
                    VariationInstanceColorOverrideArray[VariationIndex].Add(InstanceColorOverride[Idx]);
            }
        }
    }

    for ( int32 Idx = 0; Idx < VariationCount; Idx++ )
    {
        if ( !InstancerComponents.IsValidIndex( Idx )
            || !VariationTransformsArray.IsValidIndex( Idx )
            || !VariationInstanceColorOverrideArray.IsValidIndex( Idx ) )
        {
            // TODO: fix this properly
            continue;
        }

        TArray<FTransform> ProcessedTransform;
        this->GetProcessedTransforms( ProcessedTransform, Idx );

        UHoudiniInstancedActorComponent::UpdateInstancerComponentInstances(
            InstancerComponents[ Idx ],
            ProcessedTransform,
            VariationInstanceColorOverrideArray[ Idx ] );
    }
}

void
UHoudiniAssetInstanceInputField::UpdateRelativeTransform()
{
    int32 VariationCount = InstanceVariationCount();
    for ( int32 Idx = 0; Idx < VariationCount; Idx++ )
        InstancerComponents[ Idx ]->SetRelativeTransform( HoudiniGeoPartObject.TransformMatrix );
}

void
UHoudiniAssetInstanceInputField::UpdateInstanceUPropertyAttributes()
{
    if ( !HoudiniAssetInstanceInput || HoudiniAssetInstanceInput->IsPendingKill() )
        return;

    // Check if instancer material is available.
    const FHoudiniGeoPartObject & InstancerHoudiniGeoPartObject = HoudiniAssetInstanceInput->HoudiniGeoPartObject;

    for ( int32 Idx = 0; Idx < InstancerComponents.Num(); Idx++ )
        FHoudiniEngineUtils::UpdateUPropertyAttributesOnObject(InstancerComponents[ Idx ], InstancerHoudiniGeoPartObject );
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

	if (!InObject || InObject->IsPendingKill())
		return;

	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return;

    InstancedObjects.Insert( InObject, VariationIdx );
    RotationOffsets.Insert( FRotator( 0, 0, 0 ), VariationIdx );
    ScaleOffsets.Insert( FVector( 1, 1, 1 ), VariationIdx );
    bScaleOffsetsLinearlyArray.Insert( true, VariationIdx );

    AddInstanceComponent( VariationIdx );
    UpdateInstanceTransforms( true );
    UpdateInstanceUPropertyAttributes();
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
    if ( InstancerComponents.IsValidIndex( VariationIdx ) )
    {
        if ( USceneComponent* Comp = InstancerComponents[ VariationIdx ] )
        {
            Comp->DestroyComponent();
        }
        InstancerComponents.RemoveAt( VariationIdx );
    }

    UpdateInstanceTransforms( true );
}

void
UHoudiniAssetInstanceInputField::ReplaceInstanceVariation( UObject * InObject, int Index )
{
    if ( !InObject || InObject->IsPendingKill() )
    {
        HOUDINI_LOG_WARNING( TEXT("ReplaceInstanceVariation: Invalid input object") );
        return;
    }

    if ( !InstancedObjects.IsValidIndex(Index) )
    {
        HOUDINI_LOG_WARNING(TEXT("ReplaceInstanceVariation: Input index doesnt match valid Instanced Object"));
        return;
    }

    if ( !InstancerComponents.IsValidIndex( Index ) )
    {
        HOUDINI_LOG_WARNING(TEXT("ReplaceInstanceVariation: Input index doesnt match valid Instanced Component"));
        return;
    }

    if (InstancerComponents.Num() != InstancedObjects.Num())
    {
        HOUDINI_LOG_WARNING(TEXT("ReplaceInstanceVariation: Invalid instanced component and objects"));
        return;
    }

    // Check if the replacing object and the current object are different types (StaticMesh vs. Non)
    // if so we need to swap out the component 
    bool bInIsStaticMesh = InObject->IsA<UStaticMesh>();
    bool bCurrentIsStaticMesh = false;
    if ( InstancedObjects[ Index ] && !InstancedObjects[ Index ]->IsPendingKill() )
        bCurrentIsStaticMesh = InstancedObjects[ Index ]->IsA<UStaticMesh>();

    InstancedObjects[ Index ] = InObject;

    bool bComponentNeedToBeCreated = true;
    if ( bInIsStaticMesh == bCurrentIsStaticMesh )
    {
        // If the in mesh has LODs, we need a Hierarchical ISMC
        UStaticMesh* StaticMesh = Cast< UStaticMesh >( InObject );
        bool bInHasLODs = false;
        if ( StaticMesh && !StaticMesh->IsPendingKill() && ( StaticMesh->GetNumLODs() > 1 ) )
            bInHasLODs = true;

        // We'll try to reuse the InstanceComponent
        if ( UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>( InstancerComponents[ Index ] ) )
        {
            // If we have LODs, make sure we the component is a HISM
            // If we don't, make sure the component is not a HISM
            UHierarchicalInstancedStaticMeshComponent* HISMC = Cast<UHierarchicalInstancedStaticMeshComponent>( InstancerComponents[ Index ] );
            if ( !HISMC && bInHasLODs )
                bComponentNeedToBeCreated = true;
            else if ( HISMC && !bInHasLODs )
                bComponentNeedToBeCreated = true;
            else if ( !ISMC->IsPendingKill() )
            {
                ISMC->SetStaticMesh( Cast<UStaticMesh>( InObject ) ); 
                bComponentNeedToBeCreated = false;
            }
        }
        else if( UHoudiniMeshSplitInstancerComponent* MSPIC = Cast<UHoudiniMeshSplitInstancerComponent>( InstancerComponents[ Index ] ) )
        {
            if( !MSPIC->IsPendingKill() )
            {
                MSPIC->SetStaticMesh( Cast<UStaticMesh>( InObject ) );
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
        FTransform SavedXform = FTransform::Identity;

        if ( InstancerComponents.IsValidIndex( Index ) )
        {
            if ( InstancerComponents[ Index ] )
            {
                SavedXform = InstancerComponents[ Index ]->GetRelativeTransform();
                InstancerComponents[ Index ]->DestroyComponent();
            }
            InstancerComponents.RemoveAt( Index );
        }

        AddInstanceComponent( Index );
        InstancerComponents[ Index ]->SetRelativeTransform( SavedXform );
    }

    UpdateInstanceTransforms( false );
    UpdateInstanceUPropertyAttributes();
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

const FRotator &
UHoudiniAssetInstanceInputField::GetRotationOffset( int32 VariationIdx ) const
{
    if ( RotationOffsets.IsValidIndex( VariationIdx ) )
        return RotationOffsets[ VariationIdx ];
    else
        return FRotator::ZeroRotator;
}

void
UHoudiniAssetInstanceInputField::SetRotationOffset( const FRotator & Rotator, int32 VariationIdx )
{
    if ( RotationOffsets.IsValidIndex( VariationIdx ) )
        RotationOffsets[ VariationIdx ] = Rotator;
}

const FVector &
UHoudiniAssetInstanceInputField::GetScaleOffset( int32 VariationIdx ) const
{
    if (ScaleOffsets.IsValidIndex(VariationIdx))
	return ScaleOffsets[VariationIdx];
    else
	return FVector::ZeroVector;
}

void
UHoudiniAssetInstanceInputField::SetScaleOffset( const FVector & InScale, int32 VariationIdx )
{
    if ( ScaleOffsets.IsValidIndex( VariationIdx ) )
        ScaleOffsets[ VariationIdx ] = InScale;
}

bool
UHoudiniAssetInstanceInputField::AreOffsetsScaledLinearly( int32 VariationIdx ) const
{
    if ( bScaleOffsetsLinearlyArray.IsValidIndex( VariationIdx ) )
        return bScaleOffsetsLinearlyArray[ VariationIdx ];
    else
        return false;
}

void
UHoudiniAssetInstanceInputField::SetLinearOffsetScale( bool bEnabled, int32 VariationIdx )
{
    if ( bScaleOffsetsLinearlyArray.IsValidIndex( VariationIdx ) )
        bScaleOffsetsLinearlyArray[ VariationIdx ] = bEnabled;
}

bool
UHoudiniAssetInstanceInputField::IsOriginalObjectUsed( int32 VariationIdx ) const
{
    if ( !InstancedObjects.IsValidIndex(VariationIdx) )
        return false;

    return OriginalObject == InstancedObjects[ VariationIdx ];
}

USceneComponent *
UHoudiniAssetInstanceInputField::GetInstancedComponent( int32 VariationIdx ) const
{
    if ( !InstancerComponents.IsValidIndex(VariationIdx) )
        return nullptr;

    return InstancerComponents[ VariationIdx ];
}

const TArray< FTransform > &
UHoudiniAssetInstanceInputField::GetInstancedTransforms( int32 VariationIdx ) const
{
    check(VariationIdx >= 0 && VariationIdx < VariationTransformsArray.Num());
    return VariationTransformsArray[VariationIdx];
}

void
UHoudiniAssetInstanceInputField::RecreateRenderState()
{
    check( InstancerComponents.Num() == InstancedObjects.Num() );
    for ( auto Comp : InstancerComponents )
    {
        UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(Comp);
        if ( ISMC && !ISMC->IsPendingKill() )
            ISMC->RecreateRenderState_Concurrent();
    }
}

void
UHoudiniAssetInstanceInputField::RecreatePhysicsState()
{
    check( InstancerComponents.Num() == InstancedObjects.Num() );
    for ( auto Comp : InstancerComponents )
    {
        UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(Comp);
        if ( ISMC && !ISMC->IsPendingKill() )
            ISMC->RecreatePhysicsState();
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
        if ( !StaticMesh || StaticMesh->IsPendingKill() || StaticMesh != OriginalObject )
            continue;

        if (!InstancerComponents.IsValidIndex(Idx))
            continue;

        UInstancedStaticMeshComponent * InstancedStaticMeshComponent = Cast<UInstancedStaticMeshComponent>(InstancerComponents[Idx]);
        if ( !InstancedStaticMeshComponent || InstancedStaticMeshComponent->IsPendingKill() )
            continue;

        const TArray< class UMaterialInterface * > & OverrideMaterials = InstancedStaticMeshComponent->OverrideMaterials;
        for ( int32 MaterialIdx = 0; MaterialIdx < OverrideMaterials.Num(); ++MaterialIdx )
        {
            UMaterialInterface * OverridenMaterial = OverrideMaterials[ MaterialIdx ];
            if ( !OverridenMaterial || OverridenMaterial->IsPendingKill() || OverridenMaterial != Material )
                continue;

            if ( !StaticMesh->StaticMaterials.IsValidIndex( MaterialIdx ) )
                continue;

            MaterialReplacementsMap.Add( StaticMesh, MaterialIdx );
            bResult = true;
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
        if( ReplacementObj && *ReplacementObj && !(*ReplacementObj)->IsPendingKill() )
        {
            InstancedObjects[ Idx ] = *ReplacementObj;
            if( UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>( InstancerComponents[ Idx ] ) )
            {
                if ( !ISMC->IsPendingKill() )
                    ISMC->SetStaticMesh( CastChecked<UStaticMesh>( *ReplacementObj ) );
            }
            else if( UHoudiniMeshSplitInstancerComponent* MSIC = Cast<UHoudiniMeshSplitInstancerComponent>(InstancerComponents[Idx]) )
            {
                if ( !MSIC->IsPendingKill() )
                    MSIC->SetStaticMesh( CastChecked<UStaticMesh>( *ReplacementObj ) );
            }
            else if( UHoudiniInstancedActorComponent* IAC = Cast<UHoudiniInstancedActorComponent>( InstancerComponents[ Idx ] ) )
            {
                if ( !IAC->IsPendingKill() )
                    IAC->InstancedAsset = *ReplacementObj;
            }
        }
    }
}


/** Return the array of processed transforms **/
void
UHoudiniAssetInstanceInputField::GetProcessedTransforms( TArray<FTransform>& ProcessedTransforms, const int32& VariationIdx ) const
{
    ProcessedTransforms.Empty();
    if (!VariationTransformsArray.IsValidIndex(VariationIdx))
        return;

    ProcessedTransforms.Reserve(VariationTransformsArray[ VariationIdx ].Num());

    FTransform CurrentTransform = FTransform::Identity;
    for (int32 InstanceIdx = 0; InstanceIdx < VariationTransformsArray[ VariationIdx ].Num(); ++InstanceIdx)
    {
        CurrentTransform = VariationTransformsArray[VariationIdx][ InstanceIdx ];

        // Compute new rotation and scale.
        FQuat TransformRotation = CurrentTransform.GetRotation() * GetRotationOffset( VariationIdx ).Quaternion();
        FVector TransformScale3D = CurrentTransform.GetScale3D() * GetScaleOffset( VariationIdx );

        // Make sure inverse matrix exists - seems to be a bug in Unreal when submitting instances.
        // Happens in blueprint as well.
        // We want to make sure the scale is not too small, but keep negative values! (Bug 90876)
        if (FMath::Abs(TransformScale3D.X) < HAPI_UNREAL_SCALE_SMALL_VALUE)
            TransformScale3D.X = (TransformScale3D.X > 0) ? HAPI_UNREAL_SCALE_SMALL_VALUE : -HAPI_UNREAL_SCALE_SMALL_VALUE;

        if (FMath::Abs(TransformScale3D.Y) < HAPI_UNREAL_SCALE_SMALL_VALUE)
            TransformScale3D.Y = (TransformScale3D.Y > 0) ? HAPI_UNREAL_SCALE_SMALL_VALUE : -HAPI_UNREAL_SCALE_SMALL_VALUE;

        if (FMath::Abs(TransformScale3D.Z) < HAPI_UNREAL_SCALE_SMALL_VALUE)
            TransformScale3D.Z = (TransformScale3D.Z > 0) ? HAPI_UNREAL_SCALE_SMALL_VALUE : -HAPI_UNREAL_SCALE_SMALL_VALUE;

        CurrentTransform.SetRotation(TransformRotation);
        CurrentTransform.SetScale3D(TransformScale3D);

        if ( CurrentTransform.IsValid() )
            ProcessedTransforms.Add( CurrentTransform );
    }
}

