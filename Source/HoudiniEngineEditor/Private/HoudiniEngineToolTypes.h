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

#pragma once

#include "CoreMinimal.h"
#include "HoudiniToolData.h"
#include "HoudiniToolTypes.h"

#include "HoudiniEngineToolTypes.generated.h"


class IAssetTools;
class IAssetTypeActions;
class IComponentAssetBroker;
class UHoudiniAssetComponent;
class FMenuBuilder;

struct FSlateBrush;
struct FHoudiniToolDescription;

USTRUCT(BlueprintType)
struct FHoudiniToolDirectory
{
    GENERATED_BODY()

    /** Name of the tool directory */
    UPROPERTY(GlobalConfig, Category = Tool, EditAnywhere)
    FString Name;

    /** Path of the tool directory */
    UPROPERTY(GlobalConfig, Category = Tool, EditAnywhere)
    FDirectoryPath Path;

    /** Unique generated ID used to store the imported uasset for the tools */
    UPROPERTY(GlobalConfig, Category = Tool, VisibleDefaultsOnly)
    FString ContentDirID;

    FORCEINLINE bool operator==(const FHoudiniToolDirectory& Other)const
    {
        return Name == Other.Name && Path.Path == Other.Path.Path;
    }

    FORCEINLINE bool operator!=(const FHoudiniToolDirectory& Other)const
    {
        return !(*this == Other);
    }
};

struct FHoudiniTool
{
    FHoudiniTool()
        : HoudiniAsset( nullptr)
        , Name()
        , ToolTipText()
        , Icon()
        , HelpURL()
        , Type(EHoudiniToolType::HTOOLTYPE_OPERATOR_SINGLE)
        , DefaultTool(false)
        , SelectionType(EHoudiniToolSelectionType::HTOOL_SELECTION_ALL)
        , SourceAssetPath()
        , ToolDirectory()
        , JSONFile()
    {
    }

    FHoudiniTool(
        const TSoftObjectPtr < class UHoudiniAsset >& InHoudiniAsset, const FText& InName,
        const EHoudiniToolType& InType, const EHoudiniToolSelectionType& InSelType,
        const FText& InToolTipText, const FSlateBrush* InIcon, const FString& InHelpURL,
        const bool& isDefault, const FFilePath& InAssetPath, const FHoudiniToolDirectory& InToolDirectory,
        const FString& InJSONFile )
        : HoudiniAsset( InHoudiniAsset )
        , Name( InName )
        , ToolTipText( InToolTipText )
        , Icon( InIcon )
        , HelpURL( InHelpURL )
        , Type( InType )
        , DefaultTool( isDefault )
        , SelectionType( InSelType )
        , SourceAssetPath( InAssetPath )
        , ToolDirectory( InToolDirectory )
        , JSONFile( InJSONFile )
    {
    }

    /** The Houdini Asset used by the tool **/ 
    TSoftObjectPtr < class UHoudiniAsset > HoudiniAsset;
    TSoftObjectPtr < class UHoudiniToolsPackageAsset > ToolsPackage;

    /** The name to be displayed */
    FText Name;

    /** The name to be displayed */
    FText ToolTipText;

    /** The icon to be displayed */
    const FSlateBrush* Icon;

    /** The help URL for this tool */
    FString HelpURL;

    /** The type of tool, this will change how the asset handles the current selection **/
    EHoudiniToolType Type;

    /** Indicate this is one of the default tools **/
    // TODO: DEPRECATE. We won't be hiding tools based on this default field. Rather, we'll be hiding
    //       anything in the "default" category in the Tools panel.
    bool DefaultTool;

    /** Indicate what the tool should consider for selection **/
    EHoudiniToolSelectionType SelectionType;

    /** Path to the Asset used **/
    // TODO: DEPRECATE (this should be retrieved / derived from HoudiniAsset).
    FFilePath SourceAssetPath;

    /** Directory containing the tool **/
    // TODO: DEPRECATE (this should be retrieved / derived from HoudiniAsset).
    FHoudiniToolDirectory ToolDirectory;

    /** Name of the JSON containing the tool's description **/
    // TODO: DEPRECATE (this should be retrieved / derived from HoudiniAsset -- JSON filename matches the external source filename).
    FString JSONFile;

    /** Returns the file path to the JSOn file containing the tool's description **/
    // TODO: Resolve the JSON file path making use of the HoudiniAsset's source file name.
    FString GetJSonFilePath() { return ToolDirectory.Path.Path / JSONFile; };
};

struct FHoudiniToolList
{
    TArray<TSharedPtr<FHoudiniTool>> Tools;
};





