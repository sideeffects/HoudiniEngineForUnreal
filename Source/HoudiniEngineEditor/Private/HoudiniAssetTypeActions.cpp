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
#include "HoudiniAssetTypeActions.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngine.h"

FText
FHoudiniAssetTypeActions::GetName() const
{
    return LOCTEXT( "HoudiniAssetTypeActions", "HoudiniAsset" );
}

FColor
FHoudiniAssetTypeActions::GetTypeColor() const
{
    return FColor( 255, 165, 0 );
}

UClass *
FHoudiniAssetTypeActions::GetSupportedClass() const
{
    return UHoudiniAsset::StaticClass();
}

uint32
FHoudiniAssetTypeActions::GetCategories()
{
    return EAssetTypeCategories::Misc;
}

UThumbnailInfo *
FHoudiniAssetTypeActions::GetThumbnailInfo( UObject * Asset ) const
{
    UHoudiniAsset * HoudiniAsset = CastChecked< UHoudiniAsset >( Asset );
    UThumbnailInfo * ThumbnailInfo = HoudiniAsset->ThumbnailInfo;
    if ( !ThumbnailInfo )
    {
        // If we have no thumbnail information, construct it.
        ThumbnailInfo = NewObject< USceneThumbnailInfo >( HoudiniAsset, USceneThumbnailInfo::StaticClass() );
        HoudiniAsset->ThumbnailInfo = ThumbnailInfo;
    }

    return ThumbnailInfo;
}

bool
FHoudiniAssetTypeActions::HasActions( const TArray< UObject * > & InObjects ) const
{
    return true;
}

void
FHoudiniAssetTypeActions::GetActions( const TArray< UObject * > & InObjects, class FMenuBuilder & MenuBuilder )
{
    auto HoudiniAssets = GetTypedWeakObjectPtrs< UHoudiniAsset >( InObjects );

    FHoudiniEngineEditor & HoudiniEngineEditor = FHoudiniEngineEditor::Get();
    TSharedPtr< ISlateStyle > StyleSet = HoudiniEngineEditor.GetSlateStyle();

    MenuBuilder.AddMenuEntry(
        NSLOCTEXT( "HoudiniAssetTypeActions", "HoudiniAsset_Reimport", "Reimport" ),
        NSLOCTEXT( "HoudiniAssetTypeActions", "HoudiniAsset_ReimportTooltip", "Reimport selected Houdini Assets." ),
        FSlateIcon( StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
        FUIAction(
            FExecuteAction::CreateSP( this, &FHoudiniAssetTypeActions::ExecuteReimport, HoudiniAssets ),
            FCanExecuteAction()
        )
    );

    MenuBuilder.AddMenuEntry(
        NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_RebuildAll", "Rebuild All Instances"),
        NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_RebuildAllTooltip", "Reimports and rebuild all instances of the selected Houdini Assets."),
        FSlateIcon(StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo"),
        FUIAction(
            FExecuteAction::CreateSP(this, &FHoudiniAssetTypeActions::ExecuteRebuildAllInstances, HoudiniAssets),
            FCanExecuteAction()
        )
    );

    MenuBuilder.AddMenuEntry(
        NSLOCTEXT( "HoudiniAssetTypeActions", "HoudiniAsset_FindInExplorer", "Find Source"),
        NSLOCTEXT(
            "HoudiniAssetTypeActions", "HoudiniAsset_FindInExplorerTooltip",
            "Opens explorer at the location of this asset." ),
        FSlateIcon( StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo" ),
        FUIAction(
            FExecuteAction::CreateSP( this, &FHoudiniAssetTypeActions::ExecuteFindInExplorer, HoudiniAssets ),
            FCanExecuteAction()
        )
    );

    MenuBuilder.AddMenuEntry(
        NSLOCTEXT("HoudiniAssetTypeActions", "HoudiniAsset_OpenInHoudini", "Open in Houdini"),
        NSLOCTEXT(
            "HoudiniAssetTypeActions", "HoudiniAsset_OpenInHoudiniTooltip",
            "Opens the selected asset in Houdini."),
        FSlateIcon(StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo"),
        FUIAction(
            FExecuteAction::CreateSP(this, &FHoudiniAssetTypeActions::ExecuteOpenInHoudini, HoudiniAssets),
            FCanExecuteAction::CreateLambda( [=] { return ( HoudiniAssets.Num() == 1 ); } )
        )
    );
}

void
FHoudiniAssetTypeActions::ExecuteReimport( TArray< TWeakObjectPtr< UHoudiniAsset > > HoudiniAssets )
{
    for ( auto ObjIt = HoudiniAssets.CreateConstIterator(); ObjIt; ++ObjIt )
    {
        UHoudiniAsset * HoudiniAsset = ( *ObjIt ).Get();
        if ( HoudiniAsset )
            FReimportManager::Instance()->Reimport( HoudiniAsset, true );
    }
}

void
FHoudiniAssetTypeActions::ExecuteRebuildAllInstances(TArray< TWeakObjectPtr< UHoudiniAsset > > HoudiniAssets)
{
    // Reimports and then rebuild all instances of the asset
    for (auto ObjIt = HoudiniAssets.CreateConstIterator(); ObjIt; ++ObjIt)
    {
        UHoudiniAsset * HoudiniAsset = ( *ObjIt ).Get();
        if ( !HoudiniAsset )
            continue;

        // Reimports the asset
        FReimportManager::Instance()->Reimport( HoudiniAsset, true );

        // Rebuilds all instances of that asset in the scene
        for ( TObjectIterator<UHoudiniAssetComponent> Itr; Itr; ++Itr )
        {
            UHoudiniAssetComponent * Component = *Itr;
            if ( Component && ( Component->GetHoudiniAsset() == HoudiniAsset ) )
            {
                Component->StartTaskAssetRebuildManual();
            }
        }
    }
}

void
FHoudiniAssetTypeActions::ExecuteFindInExplorer( TArray< TWeakObjectPtr< UHoudiniAsset > > HoudiniAssets )
{
    for ( auto ObjIt = HoudiniAssets.CreateConstIterator(); ObjIt; ++ObjIt )
    {
        UHoudiniAsset * HoudiniAsset = ( *ObjIt ).Get();
        if ( HoudiniAsset && HoudiniAsset->AssetImportData )
        {
            const FString SourceFilePath = HoudiniAsset->AssetImportData->GetFirstFilename();
            if ( SourceFilePath.Len() && IFileManager::Get().FileSize( *SourceFilePath ) != INDEX_NONE )
                FPlatformProcess::ExploreFolder( *SourceFilePath );
        }
    }
}

void
FHoudiniAssetTypeActions::ExecuteOpenInHoudini( TArray< TWeakObjectPtr< UHoudiniAsset > > HoudiniAssets )
{
    if ( !FHoudiniEngine::IsInitialized() )
        return;
    
    if ( HoudiniAssets.Num() != 1 )
        return;
   
    UHoudiniAsset * HoudiniAsset = HoudiniAssets[0].Get();
    if ( !HoudiniAsset || !( HoudiniAsset->AssetImportData ) )
        return;

    const FString SourceFilePath = HoudiniAsset->AssetImportData->GetFirstFilename();
    if ( !SourceFilePath.Len() || IFileManager::Get().FileSize(*SourceFilePath) == INDEX_NONE )
        return;
    
    if ( !FPaths::FileExists( SourceFilePath ) )
        return;

    // Then open the HDA file in Houdini
    FString LibHAPILocation = FHoudiniEngine::Get().GetLibHAPILocation();
    FString HoudiniLocation = LibHAPILocation + "/hview";

    FPlatformProcess::CreateProc(
        HoudiniLocation.GetCharArray().GetData(),
        SourceFilePath.GetCharArray().GetData(),
        true, false, false,
        nullptr, 0,
        FPlatformProcess::UserTempDir(),
        nullptr, nullptr );
}
