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

#include "HoudiniApi.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetComponentMaterials.h"
#include "HoudiniEngineRuntimePrivatePCH.h"


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
            for( int32 Ix = 0; Ix < OriginalSM->StaticMaterials.Num(); ++Ix )
            {
                MatReplacements.Add( OriginalSM->StaticMaterials[ Ix ].MaterialInterface,
                    NewSM->StaticMaterials[ Ix ].MaterialInterface);
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