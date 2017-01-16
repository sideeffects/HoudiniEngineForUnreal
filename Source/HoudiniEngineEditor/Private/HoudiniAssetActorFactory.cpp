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

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniAssetActorFactory.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAsset.h"

UHoudiniAssetActorFactory::UHoudiniAssetActorFactory( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
{
    DisplayName = LOCTEXT( "HoudiniAssetDisplayName", "Houdini Engine Asset" );
    NewActorClass = AHoudiniAssetActor::StaticClass();
}

bool
UHoudiniAssetActorFactory::CanCreateActorFrom( const FAssetData & AssetData, FText & OutErrorMsg )
{
    if ( !AssetData.IsValid() || !AssetData.GetClass()->IsChildOf(UHoudiniAsset::StaticClass() ) )
    {
        OutErrorMsg = NSLOCTEXT( "CanCreateActor", "NoHoudiniAsset", "A valid Houdini Engine asset must be specified." );
        return false;
    }

    return true;
}

UObject *
UHoudiniAssetActorFactory::GetAssetFromActorInstance( AActor * Instance )
{
    check( Instance->IsA( NewActorClass ) );
    AHoudiniAssetActor * HoudiniAssetActor = CastChecked< AHoudiniAssetActor >( Instance );

    check( HoudiniAssetActor->HoudiniAssetComponent );
    return HoudiniAssetActor->GetHoudiniAssetComponent()->HoudiniAsset;
}

void
UHoudiniAssetActorFactory::PostSpawnActor( UObject * Asset, AActor * NewActor )
{
    HOUDINI_LOG_MESSAGE( TEXT( "PostSpawnActor %s, supplied Asset = 0x%0.8p" ), *NewActor->GetName(), Asset );

    UHoudiniAsset * HoudiniAsset = CastChecked<UHoudiniAsset>(Asset);
    if ( HoudiniAsset )
    {
        AHoudiniAssetActor * HoudiniAssetActor = CastChecked< AHoudiniAssetActor >( NewActor );
        UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
        check( HoudiniAssetComponent );

        // Mark this component as native.
        HoudiniAssetComponent->SetNative( true );

        HoudiniAssetComponent->UnregisterComponent();
        HoudiniAssetComponent->SetHoudiniAsset( HoudiniAsset );
        HoudiniAssetComponent->RegisterComponent();
    }
}

void
UHoudiniAssetActorFactory::PostCreateBlueprint( UObject * Asset, AActor * CDO )
{
    HOUDINI_LOG_MESSAGE( TEXT( "PostCreateBlueprint, supplied Asset = 0x%0.8p" ), Asset );

    UHoudiniAsset * HoudiniAsset = CastChecked<UHoudiniAsset>(Asset);
    if ( HoudiniAsset )
    {
        AHoudiniAssetActor * HoudiniAssetActor = CastChecked< AHoudiniAssetActor >( CDO );
        UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
        HoudiniAssetComponent->HoudiniAsset = HoudiniAsset;
    }
}
