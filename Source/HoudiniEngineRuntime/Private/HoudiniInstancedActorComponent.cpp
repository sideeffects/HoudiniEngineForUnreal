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
*      Chris Grebeldinger
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniInstancedActorComponent.h"
#if WITH_EDITOR
#include "LevelEditorViewport.h"
#endif

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
    if ( UHoudiniInstancedActorComponent * This = Cast< UHoudiniInstancedActorComponent >( InThis ) )
    {
        Collector.AddReferencedObject( This->InstancedAsset, This );
        Collector.AddReferencedObjects( This->Instances, This );
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

        if( InstancedAsset )
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
    if ( AActor * NewActor = SpawnInstancedActor( InstanceTransform ) )
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
    GEditor->ClickLocation = InstancedTransform.GetTranslation();
    GEditor->ClickPlane = FPlane( GEditor->ClickLocation, FVector::UpVector );
    TArray<AActor*> NewActors = FLevelEditorViewportClient::TryPlacingActorFromObject( GetOwner()->GetLevel(), InstancedAsset, false, RF_Transactional, nullptr );
    if ( NewActors.Num() > 0 )
        return NewActors[ 0 ];
#endif
    return nullptr;
}

void 
UHoudiniInstancedActorComponent::ClearInstances()
{
    for ( AActor* Instance : Instances )
    {
        if ( Instance )
        {
            Instance->Destroy();
        }
    }
    Instances.Empty();
}


void 
UHoudiniInstancedActorComponent::OnComponentCreated()
{
    Super::OnComponentCreated();

    // If our instances are parented to another actor we should duplicate them 
    if ( Instances.Num() > 0 && Instances[ 0 ] && Instances[ 0 ]->GetAttachParentActor() != GetOwner() )
    {
        TArray< AActor* > SourceInstances = Instances;
        Instances.Empty();
        for ( AActor* Instance : SourceInstances )
        {
            if ( Instance )
            {
                AddInstance( Instance->GetRootComponent()->GetRelativeTransform() );
            }
        }
    }
}
