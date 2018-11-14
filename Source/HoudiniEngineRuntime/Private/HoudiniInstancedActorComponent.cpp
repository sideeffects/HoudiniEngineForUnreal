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
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniInstancedActorComponent.h"
#include "HoudiniMeshSplitInstancerComponent.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#if WITH_EDITOR
#include "LevelEditorViewport.h"
#endif

#include "Internationalization/Internationalization.h"
#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

UHoudiniInstancedActorComponent::UHoudiniInstancedActorComponent( const FObjectInitializer& ObjectInitializer )
: Super( ObjectInitializer )
, InstancedAsset( nullptr )
{
}


void UHoudiniInstancedActorComponent::OnComponentDestroyed( bool bDestroyingHierarchy )
{
    ClearInstances();
    Super::OnComponentDestroyed( bDestroyingHierarchy );
}

void
UHoudiniInstancedActorComponent::Serialize( FArchive & Ar )
{
    Super::Serialize( Ar );
    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    Ar << InstancedAsset;
    Ar << Instances;
}

void 
UHoudiniInstancedActorComponent::AddReferencedObjects( UObject * InThis, FReferenceCollector & Collector )
{
    UHoudiniInstancedActorComponent * ThisHIAC = Cast< UHoudiniInstancedActorComponent >(InThis);
    if ( ThisHIAC && !ThisHIAC->IsPendingKill() )
    {
        if ( ThisHIAC->InstancedAsset && !ThisHIAC->InstancedAsset->IsPendingKill() )
            Collector.AddReferencedObject( ThisHIAC->InstancedAsset, ThisHIAC );

        Collector.AddReferencedObjects(ThisHIAC->Instances, ThisHIAC );
    }
}

void 
UHoudiniInstancedActorComponent::SetInstances( const TArray<FTransform>& InstanceTransforms )
{
#if WITH_EDITOR
    if ( Instances.Num() || InstanceTransforms.Num() )
    {
        const FScopedTransaction Transaction( LOCTEXT( "UpdateInstances", "Update Instances" ) );
        GetOwner()->Modify();
        ClearInstances();

        if( InstancedAsset && !InstancedAsset->IsPendingKill() )
        {
            for( const FTransform& InstanceTransform : InstanceTransforms )
            {
                AddInstance( InstanceTransform );
            }
        }
        else
        {
            HOUDINI_LOG_ERROR( TEXT( "%s: Null InstancedAsset for instanced actor override" ), *GetOwner()->GetName() );
        }
    }
#endif
}

int32 
UHoudiniInstancedActorComponent::AddInstance( const FTransform& InstanceTransform )
{
    AActor * NewActor = SpawnInstancedActor(InstanceTransform);
    if ( NewActor && !NewActor->IsPendingKill() )
    {
        NewActor->AttachToComponent( this, FAttachmentTransformRules::KeepRelativeTransform );
        NewActor->SetActorRelativeTransform( InstanceTransform );
        return Instances.Add( NewActor );
    }
    return -1;
}

AActor*
UHoudiniInstancedActorComponent::SpawnInstancedActor( const FTransform& InstancedTransform ) const
{
#if WITH_EDITOR
    if (InstancedAsset && !InstancedAsset->IsPendingKill())
    {
        GEditor->ClickLocation = InstancedTransform.GetTranslation();
        GEditor->ClickPlane = FPlane(GEditor->ClickLocation, FVector::UpVector);
        TArray<AActor*> NewActors = FLevelEditorViewportClient::TryPlacingActorFromObject(GetOwner()->GetLevel(), InstancedAsset, false, RF_Transactional, nullptr);
        if (NewActors.Num() > 0)
        {
            if ( NewActors[0] && !NewActors[0]->IsPendingKill() )
                return NewActors[0];
        }
    }
#endif
    return nullptr;
}

void 
UHoudiniInstancedActorComponent::ClearInstances()
{
    for ( AActor* Instance : Instances )
    {
        if ( Instance && !Instance->IsPendingKill() )
            Instance->Destroy();
    }
    Instances.Empty();
}


void 
UHoudiniInstancedActorComponent::OnComponentCreated()
{
    Super::OnComponentCreated();

    // If our instances are parented to another actor we should duplicate them
    bool bNeedDuplicate = false;
    for (auto CurrentInstance : Instances)
    {
        if ( !CurrentInstance || CurrentInstance->IsPendingKill() )
            continue;

        if ( CurrentInstance->GetAttachParentActor() != GetOwner() )
            bNeedDuplicate = true;
    }

    if ( !bNeedDuplicate )
        return;

    // We need to duplicate our instances
    TArray< AActor* > SourceInstances = Instances;
    Instances.Empty();
    for ( AActor* CurrentInstance : SourceInstances )
    {
        if ( !CurrentInstance || CurrentInstance->IsPendingKill() )
            continue;

        FTransform InstanceTransform;
        if ( CurrentInstance->GetRootComponent() )
            InstanceTransform = CurrentInstance->GetRootComponent()->GetRelativeTransform();

        AddInstance( InstanceTransform );
    }
}

void UHoudiniInstancedActorComponent::UpdateInstancerComponentInstances(
    USceneComponent * Component,
    const TArray< FTransform > & InstancedTransforms, const TArray<FLinearColor> & InstancedColors,
    const FRotator & RotationOffset, const FVector & ScaleOffset)
{
    UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>( Component );
    UHoudiniInstancedActorComponent* IAC = Cast<UHoudiniInstancedActorComponent>( Component );
    UHoudiniMeshSplitInstancerComponent* MSIC = Cast<UHoudiniMeshSplitInstancerComponent>( Component );

    if(!ISMC && !IAC && !MSIC)
        return;

    auto ProcessOffsets = [&]()
    {
        TArray<FTransform> ProcessedTransforms;
        ProcessedTransforms.Reserve( InstancedTransforms.Num() );

        for( int32 InstanceIdx = 0; InstanceIdx < InstancedTransforms.Num(); ++InstanceIdx )
        {
            FTransform Transform = InstancedTransforms[ InstanceIdx ];

            // Compute new rotation and scale.
            FQuat TransformRotation = Transform.GetRotation() * RotationOffset.Quaternion();
            FVector TransformScale3D = Transform.GetScale3D() * ScaleOffset;

            // Make sure inverse matrix exists - seems to be a bug in Unreal when submitting instances.
            // Happens in blueprint as well.
            // We want to make sure the scale is not too small, but keep negative values! (Bug 90876)
            if( FMath::Abs( TransformScale3D.X ) < HAPI_UNREAL_SCALE_SMALL_VALUE )
                TransformScale3D.X = ( TransformScale3D.X > 0 ) ? HAPI_UNREAL_SCALE_SMALL_VALUE : -HAPI_UNREAL_SCALE_SMALL_VALUE;

            if ( FMath::Abs( TransformScale3D.Y ) < HAPI_UNREAL_SCALE_SMALL_VALUE )
                TransformScale3D.Y = ( TransformScale3D.Y > 0 ) ? HAPI_UNREAL_SCALE_SMALL_VALUE : -HAPI_UNREAL_SCALE_SMALL_VALUE;

            if ( FMath::Abs( TransformScale3D.Z ) < HAPI_UNREAL_SCALE_SMALL_VALUE )
                TransformScale3D.Z = ( TransformScale3D.Z > 0 ) ? HAPI_UNREAL_SCALE_SMALL_VALUE : -HAPI_UNREAL_SCALE_SMALL_VALUE;

            Transform.SetRotation( TransformRotation );
            Transform.SetScale3D( TransformScale3D );

            ProcessedTransforms.Add( Transform );
        }
        return ProcessedTransforms;
    };

    if( ISMC && !ISMC->IsPendingKill() )
    {
        ISMC->ClearInstances();
        for( const auto& Transform : ProcessOffsets() )
        {
            ISMC->AddInstance( Transform );
        }
    }
    else if( IAC && !IAC->IsPendingKill() )
    {
        IAC->SetInstances( ProcessOffsets() );
    }
    else if( MSIC && !MSIC->IsPendingKill() )
    {
        MSIC->SetInstances( ProcessOffsets(), InstancedColors );
    }
}

#undef LOCTEXT_NAMESPACE