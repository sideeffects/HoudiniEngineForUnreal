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
#include "HoudiniAssetParameterFile.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniAsset.h"
#include "HoudiniEngineString.h"

#include "Internationalization/Internationalization.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

UHoudiniAssetParameterFile::UHoudiniAssetParameterFile( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
    , Filters( TEXT( "" ) )
    , IsReadOnly( false)
{
    Values.Add( TEXT( "" ) );
}

UHoudiniAssetParameterFile *
UHoudiniAssetParameterFile::Create(
    UObject * InPrimaryObject,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId,
    const HAPI_ParmInfo& ParmInfo )
{
    UObject * Outer = InPrimaryObject;
    if ( !Outer )
    {
        Outer = InParentParameter;
        if ( !Outer )
        {
            // Must have either component or parent not null.
            check( false );
        }
    }

    UHoudiniAssetParameterFile * HoudiniAssetParameterFile = NewObject< UHoudiniAssetParameterFile >(
        Outer, UHoudiniAssetParameterFile::StaticClass(), NAME_None, RF_Public | RF_Transactional );

    HoudiniAssetParameterFile->CreateParameter( InPrimaryObject, InParentParameter, InNodeId, ParmInfo );
    return HoudiniAssetParameterFile;
}

bool
UHoudiniAssetParameterFile::CreateParameter(
    UObject * InPrimaryObject,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId,
    const HAPI_ParmInfo & ParmInfo )
{
    if ( !Super::CreateParameter( InPrimaryObject, InParentParameter, InNodeId, ParmInfo ) )
        return false;

    // Look at the filechooser_mode tag to decide if the file param is read only
    IsReadOnly = false;
    FString FileChooserTag;
    if (FHoudiniEngineUtils::HapiGetParameterTag(InNodeId, ParmId, TEXT("filechooser_mode"), FileChooserTag))
    {
        if (FileChooserTag.Equals(TEXT("read"), ESearchCase::IgnoreCase))
            IsReadOnly = true;
    }

    // We can only handle file types.
    switch ( ParmInfo.type )
    {
        case HAPI_PARMTYPE_PATH_FILE:
        case HAPI_PARMTYPE_PATH_FILE_DIR:
        {
            break;
        }

        case HAPI_PARMTYPE_PATH_FILE_GEO:
        {
            ParameterLabel += TEXT( " (geo)" );
            break;
        }

        case HAPI_PARMTYPE_PATH_FILE_IMAGE:
        {
            ParameterLabel += TEXT( " (img)" );
            break;
        }

        default:
        {
            return false;
        }
    }

    // Assign internal Hapi values index.
    SetValuesIndex( ParmInfo.stringValuesIndex );

    // Get the actual value for this property.
    TArray< HAPI_StringHandle > StringHandles;
    StringHandles.SetNum( TupleSize );
    if ( FHoudiniApi::GetParmStringValues(
        FHoudiniEngine::Get().GetSession(), InNodeId, false,
        &StringHandles[ 0 ], ValuesIndex, TupleSize ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    // Convert HAPI string handles to Unreal strings.
    Values.SetNum( TupleSize );
    for ( int32 Idx = 0; Idx < TupleSize; ++Idx )
    {
        FString ValueString = TEXT( "" );
        FHoudiniEngineString HoudiniEngineString( StringHandles[ Idx ] );
        HoudiniEngineString.ToFString( ValueString );

        // Detect and update relative paths.
        Values[ Idx ] = UpdateCheckRelativePath( ValueString );
    }

    // Retrieve filters for this file.
    if ( ParmInfo.typeInfoSH > 0 )
    {
        FHoudiniEngineString HoudiniEngineString( ParmInfo.typeInfoSH );
        if ( HoudiniEngineString.ToFString( Filters ) )
        {
            if ( !Filters.IsEmpty() )
                ParameterLabel = FString::Printf( TEXT( "%s (%s)" ), *ParameterLabel, *Filters );
        }
    }

    return true;
}

void
UHoudiniAssetParameterFile::Serialize( FArchive & Ar )
{
    // Call base implementation.
    Super::Serialize( Ar );

    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    Ar << Values;
    Ar << Filters;

    if ( HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_FILE_PARAM_READ_ONLY )
        Ar << IsReadOnly;
}

bool
UHoudiniAssetParameterFile::UploadParameterValue()
{
    for ( int32 Idx = 0; Idx < Values.Num(); ++Idx )
    {
        std::string ConvertedString = TCHAR_TO_UTF8( *( Values[ Idx ] ) );
        if ( FHoudiniApi::SetParmStringValue(
            FHoudiniEngine::Get().GetSession(), NodeId, ConvertedString.c_str(), ParmId, Idx) != HAPI_RESULT_SUCCESS )
        {
            return false;
        }
    }

    return Super::UploadParameterValue();
}

bool
UHoudiniAssetParameterFile::SetParameterVariantValue(
    const FVariant & Variant, int32 Idx, bool bTriggerModify,
    bool bRecordUndo )
{
    EVariantTypes VariantType = Variant.GetType();
    if ( EVariantTypes::String == VariantType )
    {
        if ( Idx >= 0 && Idx < Values.Num() )
        {
            FString VariantStringValue = Variant.GetValue< FString >();

#if WITH_EDITOR

            FScopedTransaction Transaction(
                TEXT( HOUDINI_MODULE_RUNTIME ),
                LOCTEXT( "HoudiniAssetParameterFileChange", "Houdini Parameter File: Changing a value" ),
                PrimaryObject );

            if ( !bRecordUndo )
                Transaction.Cancel();

            Modify();

#endif // WITH_EDITOR

            // Detect and fix relative paths.
            VariantStringValue = UpdateCheckRelativePath( VariantStringValue );

            MarkPreChanged( bTriggerModify );
            Values[ Idx ] = VariantStringValue;
            MarkChanged( bTriggerModify );

            return true;
        }
    }

    return false;
}

#if WITH_EDITOR

void
UHoudiniAssetParameterFile::HandleFilePathPickerPathPicked( const FString & PickedPath, int32 Idx )
{
    if ( Values[ Idx ] != PickedPath )
    {
        // Record undo information.
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniAssetParameterFileChange", "Houdini Parameter File: Changing a value" ),
            PrimaryObject );
        Modify();

        MarkPreChanged();

        Values[ Idx ] = UpdateCheckRelativePath( PickedPath );

        // Mark this parameter as changed.
        MarkChanged();
    }
}

#endif

FString
UHoudiniAssetParameterFile::UpdateCheckRelativePath( const FString & PickedPath ) const
{
    if ( PrimaryObject && !PickedPath.IsEmpty() && FPaths::IsRelative( PickedPath ) )
    {
        if ( UHoudiniAssetComponent* Comp = Cast<UHoudiniAssetComponent>(PrimaryObject) )
        {
            if( Comp->HoudiniAsset )
            {
                FString AssetFilePath = FPaths::GetPath( Comp->HoudiniAsset->AssetFileName );
                if( FPaths::FileExists( AssetFilePath ) )
                {
                    FString UpdatedFileWidgetPath = FPaths::Combine( *AssetFilePath, *PickedPath );
                    if( FPaths::FileExists( UpdatedFileWidgetPath ) )
                    {
                        return UpdatedFileWidgetPath;
                    }
                }
            }
        }
    }

    return PickedPath;
}

const FString &
UHoudiniAssetParameterFile::GetParameterValue( int32 Idx, const FString & DefaultValue ) const
{
    if ( Idx < Values.Num() )
        return Values[ Idx ];

    return DefaultValue;
}

void
UHoudiniAssetParameterFile::SetParameterValue( const FString& InValue, int32 Idx, bool bTriggerModify, bool bRecordUndo )
{
    if ( Idx >= Values.Num() )
        return;

    if ( Values[ Idx ] != InValue )
    {

#if WITH_EDITOR

        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniAssetParameterFileChange", "Houdini Parameter File: Changing a value" ),
            PrimaryObject );

        Modify();

        if ( !bRecordUndo )
            Transaction.Cancel();

#endif // WITH_EDITOR

        MarkPreChanged( bTriggerModify );
        Values[ Idx ] = InValue;
        MarkChanged( bTriggerModify );
    }
}

#undef LOCTEXT_NAMESPACE
