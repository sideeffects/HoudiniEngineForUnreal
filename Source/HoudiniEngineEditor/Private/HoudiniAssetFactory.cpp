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

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniAssetFactory.h"
#include "HoudiniAsset.h"

UHoudiniAssetFactory::UHoudiniAssetFactory( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
{
    // This factory is responsible for manufacturing HoudiniEngine assets.
    SupportedClass = UHoudiniAsset::StaticClass();

    // This factory does not manufacture new objects from scratch.
    bCreateNew = false;

    // This factory will open the editor for each new object.
    bEditAfterNew = false;

    // This factory will import objects from files.
    bEditorImport = true;

    // Factory does not import objects from text.
    bText = false;

    // Add supported formats.
    Formats.Add( TEXT( "otl;Houdini Engine Asset" ) );
    Formats.Add( TEXT( "otllc;Houdini Engine Limited Commercial Asset" ) );
    Formats.Add( TEXT( "otlnc;Houdini Engine Non-Commercial Asset" ) );
    Formats.Add( TEXT( "hda;Houdini Engine Asset" ) );
    Formats.Add( TEXT( "hdalc;Houdini Engine Limited Commercial Asset" ) );
    Formats.Add( TEXT( "hdanc;Houdini Engine Non-Commercial Asset" ) );
    Formats.Add(TEXT("list;Houdini Engine Expanded Asset"));
}

bool
UHoudiniAssetFactory::DoesSupportClass( UClass * Class )
{
    return Class == SupportedClass;
}

FText
UHoudiniAssetFactory::GetDisplayName() const
{
    return LOCTEXT( "HoudiniAssetFactoryDescription", "Houdini Engine Asset" );
}

UObject *
UHoudiniAssetFactory::FactoryCreateBinary(
    UClass * InClass, UObject* InParent, FName InName, EObjectFlags Flags,
    UObject * Context, const TCHAR * Type, const uint8 *& Buffer,
    const uint8 * BufferEnd, FFeedbackContext * Warn )
{
    // Broadcast notification that a new asset is being imported.
    FEditorDelegates::OnAssetPreImport.Broadcast( this, InClass, InParent, InName, Type );

    // Create a new asset.
    UHoudiniAsset * HoudiniAsset = NewObject< UHoudiniAsset >( InParent, InName, Flags );
    HoudiniAsset->CreateAsset( Buffer, BufferEnd, UFactory::GetCurrentFilename() );

    // Create reimport information.
    UAssetImportData * AssetImportData = HoudiniAsset->AssetImportData;
    if ( !AssetImportData )
    {
        AssetImportData = NewObject< UAssetImportData >( HoudiniAsset, UAssetImportData::StaticClass() );
        HoudiniAsset->AssetImportData = AssetImportData;
    }

    AssetImportData->Update( UFactory::GetCurrentFilename() );

    // Broadcast notification that the new asset has been imported.
    FEditorDelegates::OnAssetPostImport.Broadcast( this, HoudiniAsset );

    return HoudiniAsset;
}

bool
UHoudiniAssetFactory::CanReimport( UObject * Obj, TArray< FString > & OutFilenames )
{
    UHoudiniAsset * HoudiniAsset = Cast< UHoudiniAsset >( Obj );
    if ( HoudiniAsset )
    {
        UAssetImportData * AssetImportData = HoudiniAsset->AssetImportData;
        if ( AssetImportData )
            OutFilenames.Add( AssetImportData->GetFirstFilename() );
        else
            OutFilenames.Add( TEXT( "" ) );

        return true;
    }

    return false;
}

void
UHoudiniAssetFactory::SetReimportPaths( UObject * Obj, const TArray< FString > & NewReimportPaths )
{
    UHoudiniAsset * HoudiniAsset = Cast< UHoudiniAsset >( Obj );
    if ( HoudiniAsset && ( 1 == NewReimportPaths.Num() ) )
        HoudiniAsset->AssetImportData->UpdateFilenameOnly( NewReimportPaths[ 0 ] );
}

EReimportResult::Type
UHoudiniAssetFactory::Reimport( UObject * Obj )
{
    UHoudiniAsset * HoudiniAsset = Cast< UHoudiniAsset >( Obj );
    if ( HoudiniAsset && HoudiniAsset->AssetImportData )
    {
        // Make sure file is valid and exists.
        const FString & Filename = HoudiniAsset->AssetImportData->GetFirstFilename();

        if ( !Filename.Len() || IFileManager::Get().FileSize( *Filename ) == INDEX_NONE )
            return EReimportResult::Failed;

        if ( UFactory::StaticImportObject(
            HoudiniAsset->GetClass(), HoudiniAsset->GetOuter(), *HoudiniAsset->GetName(),
            RF_Public | RF_Standalone, *Filename, NULL, this ) )
        {
            HOUDINI_LOG_MESSAGE( TEXT( "Houdini Asset reimported successfully." ) );

            if ( HoudiniAsset->GetOuter() )
                HoudiniAsset->GetOuter()->MarkPackageDirty();
            else
                HoudiniAsset->MarkPackageDirty();

            return EReimportResult::Succeeded;
        }
    }

    HOUDINI_LOG_MESSAGE( TEXT( "Houdini Asset reimport has failed." ) );
    return EReimportResult::Failed;
}



UObject*
UHoudiniAssetFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
    // "Sections.list" files (expanded hda / hda folder) need a special treatment,
    // but ".hda" files can be loaded normally
    FString FileExtension = FPaths::GetExtension(Filename);
    if (FileExtension.Compare(TEXT("list"), ESearchCase::IgnoreCase) != 0 )
        return Super::FactoryCreateFile(InClass, InParent, InName, Flags, Filename, Parms, Warn, bOutOperationCanceled);    

    // Make sure the file name is sections.list
    FString NameOfFile = FPaths::GetBaseFilename(Filename);
    if (NameOfFile.Compare(TEXT("sections"), ESearchCase::IgnoreCase) != 0)
    {
        HOUDINI_LOG_ERROR( TEXT("Failed to load file '%s'. File is not a valid extended HDA."), *Filename);
        return nullptr;
    }
    
    // Make sure the proper that the proper .list file is loaded
    FString PathToFile = FPaths::GetPath(Filename);
    if (PathToFile.Find(TEXT(".hda")) != (PathToFile.Len() - 4))
    {
        HOUDINI_LOG_ERROR( TEXT("Failed to load file '%s'. File is not a valid extended HDA."), *Filename);
        return nullptr;
    }

    FString NewFilename = PathToFile;
    FString NewFileNameNoHDA = FPaths::GetBaseFilename(PathToFile); 
    FName NewIname = FName(*NewFileNameNoHDA);
    FString NewFileExtension = FPaths::GetExtension(NewFilename);

    // load as binary
    TArray<uint8> Data;
    if (!FFileHelper::LoadFileToArray(Data, *Filename))
    {
        HOUDINI_LOG_ERROR( TEXT("Failed to load file '%s' to array"), *Filename);
        return nullptr;
    }

    Data.Add(0);
    ParseParms(Parms);
    const uint8* Ptr = &Data[0];

    return FactoryCreateBinary( InClass, InParent, NewIname, Flags, nullptr, *NewFileExtension, Ptr, Ptr + Data.Num() - 1, Warn );
}