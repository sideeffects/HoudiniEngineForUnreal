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
#include "HoudiniEngineCommandlet.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManagerGeneric.h"


const FString LocalAutoBakeFolder = TEXT("/HoudiniEngine/AutoBake/");

UHoudiniEngineConvertBgeoCommandlet::UHoudiniEngineConvertBgeoCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UHoudiniEngineConvertBgeoCommandlet::Main( const FString& Params )
{
    // Run me via UE4editor.exe my.uproject -run=HoudiniEngineConvertBgeo BGEO_IN (UASSET_OUT)
    HOUDINI_LOG_MESSAGE( TEXT( "Houdini Engine Convert BGEO Commandlet" ) );

    // Parse the params to a string arrays
    TArray<FString> ArgumentsArray;
    Params.ParseIntoArray(ArgumentsArray, TEXT(" "), true);

    // We're expecting at least one param (the bgeo in) and a maximum of 2 params (bgeo in, uasset out)
    // The first param is the Commandlet name, so ignore that
    if ( ( ArgumentsArray.Num() < 2 ) || ( ArgumentsArray.Num() > 3 ) )
    {
        // Invalid number of arguments, Print usage and error out
        HOUDINI_LOG_MESSAGE( TEXT( "HoudiniEngineConvertBgeoCommandlet" ) );
        HOUDINI_LOG_MESSAGE( TEXT( "Converts a .bgeo file to Static Meshes .uasset files." ) );

        HOUDINI_LOG_MESSAGE( TEXT( "Usage: -run=HoudiniEngineTest BGEO_IN UASSET_OUT" ) );

        HOUDINI_LOG_MESSAGE( TEXT( "BGEO_IN" ) );
        HOUDINI_LOG_MESSAGE( TEXT( "\tPath to the the source .bgeo file to convert." ) );

        HOUDINI_LOG_MESSAGE( TEXT( "UASSET_OUT (optional)" ) );
        HOUDINI_LOG_MESSAGE( TEXT( "\tPath for the converted uasset file. If not present, the directory/name of the bgeo file will be used" ) );

        return 1;
    }

    FString BGEOFilePath = ArgumentsArray[ 1 ];
    FString UASSETFilePath = ArgumentsArray.Num() > 2 ? ArgumentsArray[ 2 ] : FString();

    if ( !FHoudiniCommandletUtils::ConvertBGEOFileToUAsset( BGEOFilePath, UASSETFilePath ) )
        return 1;

    return 0;
}

UHoudiniEngineConvertBgeoDirCommandlet::UHoudiniEngineConvertBgeoDirCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UHoudiniEngineConvertBgeoDirCommandlet::Main(const FString& Params)
{
    // Run me via UE4editor.exe my.uproject -run=HoudiniEngineConvertBgeoDir BGEO_DIR_IN (UASSET_DIR_OUT)
    HOUDINI_LOG_MESSAGE(TEXT("Houdini Engine Convert BGEO directory"));

    // Parse the params to a string arrays
    TArray<FString> ArgumentsArray;
    Params.ParseIntoArray(ArgumentsArray, TEXT(" "), true);

    // We're expecting at least one param (the bgeo dir in) and a maximum of 3 params (bgeo dir in, uasset dir out, timeout)
    // The first param is the Commandlet name, so ignore that
    if ( (ArgumentsArray.Num() < 1 ) || ( ArgumentsArray.Num() > 3 ) )
    {
        // Invalid number of arguments, Print usage and error out
        HOUDINI_LOG_MESSAGE(TEXT("HoudiniEngineTestCommandlet:"));
        HOUDINI_LOG_MESSAGE(TEXT("Converts .bgeo files in directory to Static Meshes .uasset files in a Out directory."));

        HOUDINI_LOG_MESSAGE(TEXT("Usage: -run=HoudiniEngineTest BGEO_DIR_IN UASSET_DIR_OUT TIMEOUT"));

        HOUDINI_LOG_MESSAGE(TEXT("BGEO_DIR_IN"));
        HOUDINI_LOG_MESSAGE(TEXT("\tPath to a directory containing the .bgeo files to convert."));

        HOUDINI_LOG_MESSAGE(TEXT("UASSET_DIR_OUT (optional)"));
        HOUDINI_LOG_MESSAGE(TEXT("\tPath for the converted uasset files."));

        HOUDINI_LOG_MESSAGE(TEXT("TIMEOUT (optional)"));
        HOUDINI_LOG_MESSAGE(TEXT("\tAfter this amount of time of inactivity, the commandlet will exit."));

        return 1;
    }

    FString BGEODirPath = ArgumentsArray[ 0 ];
    FString UASSETDirPath = ArgumentsArray.Num() > 1 ? ArgumentsArray[ 1 ] : BGEODirPath;
    FString InactivityTimeOutStr = ArgumentsArray.Num() > 3 ? ArgumentsArray[ 2 ] : FString();
    float InactivityTimeOut = 1000.0f;
    if ( InactivityTimeOutStr.IsNumeric() )
        InactivityTimeOut = FCString::Atof( *InactivityTimeOutStr );
#if WITH_EDITOR
    // First check the source directory is valid
    if ( !FPaths::DirectoryExists( BGEODirPath ) )
    {
        // Cant find input BGEO dir
        HOUDINI_LOG_ERROR( TEXT( "The source BGEO directory does not exist: %s" ), *BGEODirPath );
        return false;
    }

    // Then the output directory
    if ( !FPaths::DirectoryExists( UASSETDirPath ) )
    {
        // Cant find Output dir
        HOUDINI_LOG_ERROR( TEXT( "The output UASSET directory does not exist: %s" ), *UASSETDirPath );
        return false;
    }
#endif // endif

    HOUDINI_LOG_MESSAGE( TEXT( "Looking for .bgeo files in %s ." ), *BGEODirPath );

    // Sleep time in seconds before listing the files again
    const float SleepTime = 1.0f;
    // Maximum number of conversion for a file
    const int32 NumConvertAttempts = 2;
    // If true, will literally try to erase all files before overwriting them
    // Will also empty the "local" directory
    const bool CrushFiles = true;

    // If true, source bgeo files will be deleted upon conversion
    const bool DeleteFileAfterConversion = false;

    if ( CrushFiles )
    {
        // Nuke everything in our temporary bake folder
        FFileManagerGeneric::Get().DeleteDirectory( *LocalAutoBakeFolder, false, true );
    }

    // Map tracking the number of failures for a given file
    TMap< FString, int32 > FailingFileMap;

    // M<ap listing the files that couldn't/weren't removed
    TArray< FString > UndeletedFileMap;

    bool KeepLookingForFile = true;
    float currentInactivity = 0.0f;    
    while ( KeepLookingForFile )
    {
        // List all the .bgeo files in the directory
        TArray< FString > CurrentFileList;
        FFileManagerGeneric::Get().FindFiles( CurrentFileList, *BGEODirPath, TEXT( ".bgeo" ) );

        // Convert the files we found
        int32 ConversionCount = 0;
        for ( int32 n = CurrentFileList.Num() - 1; n >= 0; n-- )
        {
            FString CurrentFile = CurrentFileList[ n ];

            // Skip undeleted files
            if ( UndeletedFileMap.Contains( CurrentFile ) )
                continue;

            // Skip failing files
            if ( FailingFileMap.Contains(CurrentFile) && FailingFileMap[ CurrentFile ] >= NumConvertAttempts )
                continue;

            // Build the in / out file names
            FString BGEOFile = BGEODirPath + TEXT("/") + CurrentFileList[ n ];
            FString UASSETFile = UASSETDirPath + TEXT("/") + CurrentFileList[ n ].LeftChop(5) + TEXT(".uasset");

            if ( CrushFiles )
            {
                if ( FFileManagerGeneric::Get().FileExists( *UASSETFile ) )
                {
                    // Erase the file if it already exists!
                    FFileManagerGeneric::Get().Delete( *UASSETFile, false, true, true );
                }
            }

            // Attempting to convert the file
            ConversionCount++;
            if ( !FHoudiniCommandletUtils::ConvertBGEOFileToUAsset( BGEOFile, UASSETFile ) )
            {
                if ( FailingFileMap.Contains( CurrentFile ) )
                    FailingFileMap[ CurrentFile ]++;
                else
                    FailingFileMap.Add( CurrentFile, 1 );

                continue;
            }

            HOUDINI_LOG_MESSAGE(TEXT("Successfully converted BGEO file: %s to %s"), *BGEOFile, *UASSETFile );

            // Delete the source BGEO
            if ( !DeleteFileAfterConversion || !FFileManagerGeneric::Get().Delete( *BGEOFile, false, true, true ) )
                UndeletedFileMap.Add( CurrentFile );
        }

        // Update the inactivity counter
        if ( ConversionCount == 0 )
        {
            currentInactivity += SleepTime;
            if ( (InactivityTimeOut > 0.0f ) && ( currentInactivity > InactivityTimeOut ) )
                KeepLookingForFile = false;
        }
        else
        {
            // reset the inactivity counter
            currentInactivity = 0.0f;
        }

        if ( CrushFiles )
        {
            // Nuke everything in our temporary bake folder
            FFileManagerGeneric::Get().DeleteDirectory(*LocalAutoBakeFolder, false, true);
        }

        // Go to bed for a while...
        FPlatformProcess::Sleep( SleepTime );
    }

    return 0;
}


bool FHoudiniCommandletUtils::ConvertBGEOFileToUAsset( const FString& InBGEOFilePath, const FString& OutUAssetFilePath )
{
#if WITH_EDITOR
    //---------------------------------------------------------------------------------------------
    // 1. Handle the BGEO param
    //---------------------------------------------------------------------------------------------

    FString BGEOFilePath = InBGEOFilePath;
    if ( !FPaths::FileExists( BGEOFilePath ) )
    {
        // Cant find BGEO file
        HOUDINI_LOG_ERROR( TEXT( "BGEO file %s could not be found!" ), *BGEOFilePath );
        return false;
    }

    // Make sure we're using absolute path!
    BGEOFilePath = FPaths::ConvertRelativePathToFull( BGEOFilePath );

    // Split the file path
    FString BGEOPath, BGEOFileName, BGEOExtension;
    FPaths::Split( BGEOFilePath, BGEOPath, BGEOFileName, BGEOExtension );
    if ( BGEOExtension.IsEmpty() )
        BGEOExtension = TEXT("bgeo");

    if ( BGEOExtension.Compare( TEXT("bgeo"), ESearchCase::IgnoreCase ) != 0 )
    {
        // Not a bgeo file!
        HOUDINI_LOG_ERROR( TEXT( "First argument %s is not a .bgeo FILE!"), *BGEOFilePath );
        return false;
    }

    BGEOFilePath = BGEOPath + TEXT("/") + BGEOFileName + TEXT(".") + BGEOExtension;

    //---------------------------------------------------------------------------------------------
    // 2. Handle the UASSET param
    //---------------------------------------------------------------------------------------------

    FString UASSETFilePath = OutUAssetFilePath;
    if ( UASSETFilePath.IsEmpty() )
    {
        // No OUT parameter, build it from the bgeo
        UASSETFilePath = BGEOPath + TEXT("/") + BGEOFileName + TEXT(".uasset");
        HOUDINI_LOG_MESSAGE( TEXT( "UASSET_OUT argument missing! Will use %s.") , *UASSETFilePath);
    }

    // Make sure we're using absolute path!
    UASSETFilePath = FPaths::ConvertRelativePathToFull( UASSETFilePath );
    if ( FPaths::FileExists( UASSETFilePath ) )
    {
        // UAsset already exists, overwrite?
        HOUDINI_LOG_MESSAGE( TEXT( "UASSET file : %s already exists, overwriting!"), *UASSETFilePath );
    }

    // Split the file path
    FString UASSETPath, UASSETFileName, UASSETExtension;
    FPaths::Split( UASSETFilePath, UASSETPath, UASSETFileName, UASSETExtension );

    UASSETFilePath = UASSETPath + TEXT("/") + UASSETFileName + TEXT(".") + UASSETExtension;

    HOUDINI_LOG_MESSAGE( TEXT( "Converting BGEO file: %s to  UASSET file : %s" ), *BGEOFilePath, *UASSETFilePath );

    // ERROR Lambda
    // Saves a debug HIP file through HEngine and returns an error (in case of HAPI/HEngine issue).
    auto SaveDebugHipFileAndReturnError = [&]()
    {
        FString HipFileName = BGEOPath + BGEOFileName + TEXT(".hip");
        std::string HipFileNameStr = TCHAR_TO_ANSI(*HipFileName);
        FHoudiniApi::SaveHIPFile(FHoudiniEngine::Get().GetSession(), HipFileNameStr.c_str(), false);

        return false;
    };

    //---------------------------------------------------------------------------------------------
    // 3. Load the bgeo file in HAPI
    //---------------------------------------------------------------------------------------------

    HAPI_NodeId NodeId = -1;
    if ( !FHoudiniCommandletUtils::LoadBGEOFileInHAPI( BGEOFilePath, NodeId) )
        return SaveDebugHipFileAndReturnError();

    //---------------------------------------------------------------------------------------------
    // 4. Create a package for the result
    //---------------------------------------------------------------------------------------------

    FString PackagePath = LocalAutoBakeFolder + UASSETFileName;
    FString PackageFilePath = PackageTools::SanitizePackageName( PackagePath );
    UPackage * Package = FindPackage( nullptr, *PackageFilePath );
    if ( !Package )    
    {
        // Create actual package.
        Package = CreatePackage( nullptr, *PackageFilePath );
        if ( !Package )
        {
            // Couldn't create the package
            HOUDINI_LOG_ERROR( TEXT( "Could not create local Package for the UASSET !!!" ) );
            return false;
        }
    }
    else
    {
        // Package already exists, overwrite?
        HOUDINI_LOG_MESSAGE( TEXT( "Package %s already exists, overwriting!" ), *PackageFilePath );
    }

    // ERROR Lambda
    // Tries to delete the local package and creates an error ( in case of UE4/Package error)
    auto DeleteLocalPackageAndReturnError = [&]()
    {
        FFileManagerGeneric::Get().Delete( *PackageFilePath, false, true, true );
        return SaveDebugHipFileAndReturnError();
    };

    //---------------------------------------------------------------------------------------------
    // 5. Create the Static Meshes from  the result of the bgeo cooking
    //---------------------------------------------------------------------------------------------

    TMap< FHoudiniGeoPartObject, UStaticMesh * > StaticMeshesOut;
    if ( !FHoudiniCommandletUtils::CreateStaticMeshes(
        BGEOFileName, NodeId, Package, StaticMeshesOut ) )
    {
        // There was some cook errors
        HOUDINI_LOG_ERROR(TEXT("Could not create Static Meshes from the bgeo!!!"));
        return SaveDebugHipFileAndReturnError();
    }

    //---------------------------------------------------------------------------------------------
    // 6. Bake the resulting Static Meshes to the package
    //---------------------------------------------------------------------------------------------

    if ( !FHoudiniCommandletUtils::BakeStaticMeshesToPackage( BGEOFileName, StaticMeshesOut, Package ) )
        return SaveDebugHipFileAndReturnError();

    //---------------------------------------------------------------------------------------------
    // 7. Delete the node in Houdini
    //---------------------------------------------------------------------------------------------

    if ( HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode( FHoudiniEngine::Get().GetSession(), NodeId ) )
    {
        // Could not delete the bgeo's file sop !
        HOUDINI_LOG_WARNING( TEXT( "Could not delete the bgeo file sop for %s"), * BGEOFileName );
    }

    //---------------------------------------------------------------------------------------------
    // 8. Save the package
    //---------------------------------------------------------------------------------------------

    Package->SetDirtyFlag( true );
    Package->FullyLoad();

    FString LocalPackageFileName = FPackageName::LongPackageNameToFilename( PackageFilePath, FPackageName::GetAssetPackageExtension() );
    if (!UPackage::SavePackage(Package, NULL, RF_Standalone, *LocalPackageFileName, GError, nullptr, false, true, SAVE_NoError))
    {
        // There was some cook errors
        HOUDINI_LOG_ERROR( TEXT( "Could not save the local package %s"), *PackageFilePath );
        return DeleteLocalPackageAndReturnError();
    }

    //---------------------------------------------------------------------------------------------
    // 9. Move the local package to its final destination
    //---------------------------------------------------------------------------------------------

    LocalPackageFileName = FPaths::ConvertRelativePathToFull( LocalPackageFileName );
    if (!FFileManagerGeneric::Get().Move(*UASSETFilePath, *LocalPackageFileName, true, true, false, false  ) )
    {
        HOUDINI_LOG_ERROR( TEXT("Could not move local package %s to %s"), *LocalPackageFileName, *UASSETFilePath);
        return DeleteLocalPackageAndReturnError();
    }
#endif
    return true;
}


bool FHoudiniCommandletUtils::LoadBGEOFileInHAPI( const FString& InputFilePath, HAPI_NodeId& NodeId )
{
    NodeId = -1;

    // Check HoudiniEngine / HAPI init?
    if ( !FHoudiniEngine::IsInitialized() )
    {
        HOUDINI_LOG_ERROR( TEXT( "Couldn't initialize HoudiniEngine!") );
        return false;
    }

    // Create a file SOP
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CreateNode(
        FHoudiniEngine::Get().GetSession(), -1,
        "SOP/file", "bgeo", true, &NodeId ), false );

    // Set the file path parameter
    HAPI_ParmId ParmId = -1;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmIdFromName(
        FHoudiniEngine::Get().GetSession(),
        NodeId, "file", &ParmId), false );

    std::string ConvertedString = TCHAR_TO_UTF8( *InputFilePath );
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetParmStringValue(
        FHoudiniEngine::Get().GetSession(), NodeId, ConvertedString.c_str(), ParmId, 0 ), false );

    // Cook the node    
    HAPI_CookOptions CookOptions;
    FMemory::Memzero< HAPI_CookOptions >( CookOptions );
    CookOptions.curveRefineLOD = 8.0f;
    CookOptions.clearErrorsAndWarnings = false;
    CookOptions.maxVerticesPerPrimitive = 3;
    CookOptions.splitGeosByGroup = false;
    CookOptions.splitGeosByAttribute = false;
    CookOptions.splitAttrSH = 0;
    CookOptions.refineCurveToLinear = true;
    CookOptions.handleBoxPartTypes = false;
    CookOptions.handleSpherePartTypes = false;
    CookOptions.splitPointsByVertexAttributes = false;
    CookOptions.packedPrimInstancingMode = HAPI_PACKEDPRIM_INSTANCING_MODE_FLAT;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::CookNode(
        FHoudiniEngine::Get().GetSession(), NodeId, &CookOptions ), false );

    // Wait for the cook to finish
    int status = HAPI_STATE_MAX_READY_STATE + 1;
    while ( status > HAPI_STATE_MAX_READY_STATE )
    {
        // Retrieve the status
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetStatus(
            FHoudiniEngine::Get().GetSession(),
            HAPI_STATUS_COOK_STATE, &status ), false );

        FString StatusString = FHoudiniEngineUtils::GetStatusString( HAPI_STATUS_COOK_STATE, HAPI_STATUSVERBOSITY_ERRORS );
        HOUDINI_LOG_MESSAGE( TEXT( "Still Cooking, current status: %s." ), *StatusString );

        // Go to bed..
        if ( status > HAPI_STATE_MAX_READY_STATE )
            FPlatformProcess::Sleep( 0.5f );
    }

    if ( status != HAPI_STATE_READY )
    {
        // There was some cook errors
        HOUDINI_LOG_ERROR( TEXT("Finished Cooking with errors!") );
        return false;
    }

    HOUDINI_LOG_MESSAGE( TEXT( "Finished Cooking!" ) );

    return true;
}

bool FHoudiniCommandletUtils::CreateStaticMeshes(
    const FString& InputName, HAPI_NodeId& NodeId, UPackage* OuterPackage,
    TMap<FHoudiniGeoPartObject, UStaticMesh *>& StaticMeshesOut )
{
	if (!OuterPackage || OuterPackage->IsPendingKill())
		return false;

    // Create a "fake" HoudiniAssetComponent
    FName HACName( *InputName );
    UHoudiniAssetComponent* HoudiniAssetComponent = NewObject< UHoudiniAssetComponent >( OuterPackage, HACName );

    // Create the CookParams
    FHoudiniCookParams HoudiniCookParams( HoudiniAssetComponent );
    HoudiniCookParams.StaticMeshBakeMode = EBakeMode::CookToTemp; // EBakeMode::CreateNewAssets?
    HoudiniCookParams.MaterialAndTextureBakeMode = EBakeMode::CookToTemp; // FHoudiniCookParams::GetDefaultMaterialAndTextureCookMode()?

    FTransform ComponentTransform;
    TMap< FHoudiniGeoPartObject, UStaticMesh * > StaticMeshesIn;
    //TMap< FHoudiniGeoPartObject, UStaticMesh * > StaticMeshesOut;

    // Create the Static Meshes from the cook result
    bool ProcessResult = FHoudiniEngineUtils::CreateStaticMeshesFromHoudiniAsset(
        NodeId, HoudiniCookParams, true, true,
        StaticMeshesIn, StaticMeshesOut, ComponentTransform );

    if ( !ProcessResult || StaticMeshesOut.Num() <= 0 )
    {
        // There was some cook errors
        HOUDINI_LOG_ERROR( TEXT( "Could not create Static Meshes from the bgeo!!!" ) );
        return false;
    }

    return true;
}

bool FHoudiniCommandletUtils::BakeStaticMeshesToPackage(
    const FString& InputName, TMap<FHoudiniGeoPartObject, UStaticMesh *>& StaticMeshes, UPackage* OutPackage )
{
    bool BakedSomething = false;
    for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator Iter( StaticMeshes ); Iter; ++Iter )
    {
        // Get the Static Mesh
        const FHoudiniGeoPartObject HoudiniGeoPartObject = Iter.Key();
        UStaticMesh* CurrentSM = Iter.Value();
        if ( !CurrentSM )
            continue;

        // Build a name for this Static Mesh
        FName StaticMeshName = FName( *( InputName + CurrentSM->GetName() ) );

        // Duplicate the Static MEsh to copy it to the package.
        UStaticMesh* DuplicatedStaticMesh = DuplicateObject< UStaticMesh >( CurrentSM, OutPackage, StaticMeshName );
        if ( !DuplicatedStaticMesh )
            continue;

        // Set the proper flags on the duplicated mesh
        DuplicatedStaticMesh->SetFlags( RF_Public | RF_Standalone );

        // Notify the registry that we have created a new duplicate mesh.
        FAssetRegistryModule::AssetCreated( DuplicatedStaticMesh );

        // Dirty the static mesh package.
        DuplicatedStaticMesh->MarkPackageDirty();
        BakedSomething = true;
    }

    if ( !BakedSomething )
    {
        // There was some cook errors
        HOUDINI_LOG_ERROR( TEXT( "Could not create Static Meshes from the bgeo!!!" ) );
        return false;
    }

    return true;
}
