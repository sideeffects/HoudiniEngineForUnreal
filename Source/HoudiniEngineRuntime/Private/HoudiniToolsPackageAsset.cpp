/*
* Copyright (c) <2023> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "HoudiniToolsPackageAsset.h"

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniToolsRuntimeUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ObjectSaveContext.h"

// FHoudiniTools::GetHoudiniToolDescriptionFromJSON(const FString& JsonFilePath,
//     FString& OutName, EHoudiniToolType& OutType, EHoudiniToolSelectionType& OutSelectionType,
//     FString& OutToolTip, FFilePath& OutIconPath, FFilePath& OutAssetPath, FString& OutHelpURL )
// bool UHoudiniToolData::PopulateFromJSONData(const FString& JSONData)
// {
//     // Parse it as JSON
//     TSharedPtr<FJsonObject> JSONObject;
//     const TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( JSONData );
//     if (!FJsonSerializer::Deserialize(Reader, JSONObject) || !JSONObject.IsValid())
//     {
//         UE_LOG(LogHoudiniTools, Error, TEXT("Received invalidate json data. Could not populate HoudiniToolData."));
//         return false;
//     }
//
//     // First, check that the tool is compatible with Unreal
//     bool IsCompatible = true;
//     if (JSONObject->HasField(TEXT("target")))
//     {
//         IsCompatible = false;
//         TArray<TSharedPtr<FJsonValue> >TargetArray = JSONObject->GetArrayField("target");
//         for (TSharedPtr<FJsonValue> TargetValue : TargetArray)
//         {
//             if ( !TargetValue.IsValid() )
//                 continue;
//
//             // Check the target array field contains either "all" or "unreal"
//             FString TargetString = TargetValue->AsString();                        
//             if ( TargetString.Equals( TEXT("all"), ESearchCase::IgnoreCase )
//                     || TargetString.Equals(TEXT("unreal"), ESearchCase::IgnoreCase) )
//             {
//                 IsCompatible = true;
//                 break;
//             }
//         }
//     }
//
//     if ( !IsCompatible )
//     {
//         // The tool is not compatible with unreal, skip it
//         UE_LOG(LogHoudiniTools, Log, TEXT("Skipped update from JSON data due to invalid target."));
//         return false;
//     }
//
//     // Then, we need to make sure the json file has a correponding hda
//     // Start by looking at the assetPath field
//     FString HDAFilePath = FString();
//     if ( JSONObject->HasField( TEXT("assetPath") ) )
//     {
//         // If the json has the assetPath field, read it from there
//         HDAFilePath = JSONObject->GetStringField(TEXT("assetPath"));
//         if (!FPaths::FileExists(HDAFilePath))
//             HDAFilePath = FString();
//     }
//
//     if (HDAFilePath.IsEmpty())
//     {
//         // Look for an hda file with the same name as the json file
//         HDAFilePath = JsonFilePath.Replace(TEXT(".json"), TEXT(".hda"));
//         if (!FPaths::FileExists(HDAFilePath))
//         {
//             // Try .hdalc
//             HDAFilePath = JsonFilePath.Replace(TEXT(".json"), TEXT(".hdalc"));
//             if (!FPaths::FileExists(HDAFilePath))
//             {
//                 // Try .hdanc
//                 HDAFilePath = JsonFilePath.Replace(TEXT(".json"), TEXT(".hdanc"));
//                 if (!FPaths::FileExists(HDAFilePath))
//                     HDAFilePath = FString();
//             }
//         }
//     }
//
//     if ( HDAFilePath.IsEmpty() )
//     {
//         HOUDINI_LOG_ERROR(TEXT("Could not find the hda for the Houdini Tool .json file: %s"), *JsonFilePath);
//         return false;
//     }
//
//     // Create a new asset.
//     OutAssetPath = FFilePath{ HDAFilePath };
//
//     // Read the tool name
//     OutName = FPaths::GetBaseFilename(JsonFilePath);
//     if ( JSONObject->HasField(TEXT("name") ) )
//         OutName = JSONObject->GetStringField(TEXT("name"));
//
//     // Read the tool type
//     OutType = EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE;
//     if ( JSONObject->HasField( TEXT("toolType") ) )
//     {
//         FString ToolType = JSONObject->GetStringField(TEXT("toolType"));
//         if ( ToolType.Equals( TEXT("GENERATOR"), ESearchCase::IgnoreCase ) )
//             OutType = EHoudiniToolType::HTOOLTYPE_GENERATOR;
//         else if (ToolType.Equals( TEXT("OPERATOR_SINGLE"), ESearchCase::IgnoreCase ) )
//             OutType = EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE;
//         else if (ToolType.Equals(TEXT("OPERATOR_MULTI"), ESearchCase::IgnoreCase))
//             OutType = EHoudiniToolType::HTOOLTYPE_OPERATOR_MULTI;
//         else if (ToolType.Equals(TEXT("BATCH"), ESearchCase::IgnoreCase))
//             OutType = EHoudiniToolType::HTOOLTYPE_OPERATOR_BATCH;
//     }
//
//     // Read the tooltip
//     OutToolTip = FString();
//     if ( JSONObject->HasField( TEXT("toolTip") ) )
//     {
//         OutToolTip = JSONObject->GetStringField(TEXT("toolTip"));
//     }
//
//     // Read the help url
//     OutHelpURL = FString();
//     if (JSONObject->HasField(TEXT("helpURL")))
//     {
//         OutHelpURL = JSONObject->GetStringField(TEXT("helpURL"));
//     }
//
//     // Read the icon path
//     FString IconPath = FString();
//     if (JSONObject->HasField(TEXT("iconPath")))
//     {
//         // If the json has the iconPath field, read it from there
//         IconPath = JSONObject->GetStringField(TEXT("iconPath"));
//         if (!FPaths::FileExists(IconPath))
//             IconPath = FString();
//     }
//
//     if (IconPath.IsEmpty())
//     {
//         // Look for a png file with the same name as the json file
//         IconPath = JsonFilePath.Replace(TEXT(".json"), TEXT(".png"));
//         if (!FPaths::FileExists(IconPath))
//         {
//             IconPath = GetDefaultHoudiniToolIcon();
//         }
//     }
//
//     OutIconPath = FFilePath{ IconPath };
//
//     // Read the selection types
//     FString SelectionType = TEXT("All");
//     if ( JSONObject->HasField(TEXT("UE_SelectionType")) )
//         SelectionType = JSONObject->GetStringField( TEXT("UE_SelectionType") );
//
//     if ( SelectionType.Equals( TEXT("CB"), ESearchCase::IgnoreCase ) )
//         OutSelectionType = EHoudiniToolSelectionType::HTOOL_SELECTION_CB_ONLY;
//     else if ( SelectionType.Equals( TEXT("World"), ESearchCase::IgnoreCase ) )
//         OutSelectionType = EHoudiniToolSelectionType::HTOOL_SELECTION_WORLD_ONLY;
//     else
//         OutSelectionType = EHoudiniToolSelectionType::HTOOL_SELECTION_ALL;
//
//     // TODO: Handle Tags in the JSON?
//
//     return true;
// }
//
// bool UHoudiniToolData::PopulateFromJSONFile(const FString& JsonFilePath)
// {
//     if ( JsonFilePath.IsEmpty() )
//         return false;
//
//     // Read the file as a string
//     FString FileContents;
//     if ( !FFileHelper::LoadFileToString( FileContents, *JsonFilePath ) )
//     {
//         UE_LOG(LogHoudiniTools, Error, TEXT("Failed to import Houdini Tool .json file: %s"), *JsonFilePath);
//         return false;
//     }
// }

UHoudiniToolsPackageAsset::UHoudiniToolsPackageAsset()
	: bReimportPackageDescription(false)
	, bExportPackageDescription(false)
	, bReimportToolsDescription(false)
	, bExportToolsDescription(false)
{
	
}

#if WITH_EDITOR
void
UHoudiniToolsPackageAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UHoudiniToolsPackageAsset, ExternalPackageDir))
	{
		UpdateProperties();
	}
}
#endif

#if WITH_EDITOR
void
UHoudiniToolsPackageAsset::MakeRelativePackagePath()
{
	const FString ProjectDir = FPaths::ProjectDir();
	FPaths::MakePathRelativeTo(ExternalPackageDir.Path, *ProjectDir);
}
#endif


#if WITH_EDITOR
void
UHoudiniToolsPackageAsset::UpdateProperties()
{
	if (ExternalPackageDir.Path.Len()>0)
	{
		MakeRelativePackagePath();
	}
}
#endif


#if WITH_EDITOR
void
UHoudiniToolsPackageAsset::PreSave(FObjectPreSaveContext SaveContext)
{
	UObject::PreSave(SaveContext);

	// Update our properties to ensure they are correct (relative paths, etc).
	UpdateProperties();
}
#endif

#if WITH_EDITOR 
void
UHoudiniToolsPackageAsset::PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext)
{
	UObject::PostSaveRoot(ObjectSaveContext);

	if (bExportPackageDescription)
	{
		// Export JSON description for this package.
		const FString JSONPath = FHoudiniToolsRuntimeUtils::GetHoudiniToolsPackageJSONPath(this);
		HOUDINI_LOG_DISPLAY(TEXT("[UHoudiniToolsPackageAsset::PostSaveRoot] Export JSON Path: %s"), *JSONPath);
		if (JSONPath.Len())
		{
			HOUDINI_LOG_DISPLAY(TEXT("[UHoudiniToolsPackageAsset::PostSaveRoot] Writing package to JSON.."));
			FHoudiniToolsRuntimeUtils::WriteJSONFromToolPackageAsset(this, JSONPath);
		}
	}
}

#endif

