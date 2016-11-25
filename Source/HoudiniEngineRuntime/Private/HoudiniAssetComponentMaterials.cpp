/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
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
#include "HoudiniAssetComponentMaterials.h"

UHoudiniAssetComponentMaterials::UHoudiniAssetComponentMaterials( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
    , HoudiniAssetComponentMaterialsFlagsPacked( 0u )
{}

void
UHoudiniAssetComponentMaterials::Serialize( FArchive & Ar )
{
    // Call base implementation.
    Super::Serialize( Ar );

    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    Ar << Assignments;
    Ar << Replacements;
}

void
UHoudiniAssetComponentMaterials::AddReferencedObjects( UObject * InThis, FReferenceCollector & Collector )
{
    UHoudiniAssetComponentMaterials * HoudiniAssetComponentMaterials = Cast< UHoudiniAssetComponentMaterials >( InThis );
    if ( HoudiniAssetComponentMaterials )
    {
        // Add references to all cached materials.
        for ( TMap< FString, UMaterialInterface * >::TIterator
            Iter( HoudiniAssetComponentMaterials->Assignments ); Iter; ++Iter )
        {
            UMaterialInterface * Material = Iter.Value();
            Collector.AddReferencedObject( Material, InThis );
        }

        // Add references for replaced materials.
        for ( TMap< FHoudiniGeoPartObject, TMap< FString, UMaterialInterface * > >::TIterator
            Iter( HoudiniAssetComponentMaterials->Replacements ); Iter; ++Iter )
        {
            TMap< FString, UMaterialInterface * > & MaterialReplacementsValues = Iter.Value();

            for ( TMap< FString, UMaterialInterface * >::TIterator
                IterInterfaces( MaterialReplacementsValues ); IterInterfaces; ++IterInterfaces )
            {
                UMaterialInterface * MaterialInterface = IterInterfaces.Value();
                if ( MaterialInterface )
                    Collector.AddReferencedObject( MaterialInterface, InThis );
            }
        }
    }

    // Call base implementation.
    Super::AddReferencedObjects( InThis, Collector );
}

void
UHoudiniAssetComponentMaterials::ResetMaterialInfo()
{
    Assignments.Empty();
    Replacements.Empty();
}

UHoudiniAssetComponentMaterials* 
UHoudiniAssetComponentMaterials::Duplicate( class UHoudiniAssetComponent* InOuter, TMap<UObject*, UObject*>& InReplacements )
{
    UHoudiniAssetComponentMaterials* ACM = DuplicateObject<UHoudiniAssetComponentMaterials>( this, InOuter );
    
    // Build a map of MI duplications
    TMap<UMaterialInterface*, UMaterialInterface*> MatReplacements;
    for( auto ReplacementPair : InReplacements )
    {
        UStaticMesh* OriginalSM = Cast<UStaticMesh>( ReplacementPair.Key );
        UStaticMesh* NewSM = Cast<UStaticMesh>( ReplacementPair.Value );
        if( OriginalSM && NewSM )
        {
            for( int32 Ix = 0; Ix < OriginalSM->Materials.Num(); ++Ix )
            {
                MatReplacements.Add( OriginalSM->Materials[ Ix ],
                    NewSM->Materials[ Ix ]);
            }
        }
    }

    // Remap MIs
    for( auto& AssignmentPair : ACM->Assignments )
    {
        if( auto NewMI = MatReplacements.Find( AssignmentPair.Value ) )
        {
            AssignmentPair.Value = *NewMI;
        }
    }

    for( auto& ReplacementPair : ACM->Replacements )
    {
        for( auto& MatPair : ReplacementPair.Value )
        {
            if( auto NewMI = MatReplacements.Find( MatPair.Value ) )
            {
                MatPair.Value = *NewMI;
            }
        }
    }
    return ACM;
}