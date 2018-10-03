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

#pragma once
#include "IHoudiniEngineEditor.h"
#include "ComponentVisualizer.h"
#include "Styling/SlateStyle.h"
#include "EditorUndoClient.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "HoudiniRuntimeSettings.h"
#include "Framework/Commands/Commands.h"


class IAssetTools;
class IAssetTypeActions;
class IComponentAssetBroker;
class UHoudiniAssetComponent;
class FMenuBuilder;

struct FSlateBrush;
struct FHoudiniToolDescription;

struct FHoudiniTool
{
    FHoudiniTool(
        TSoftObjectPtr < class UHoudiniAsset > InHoudiniAsset, const FText& InName,
        const EHoudiniToolType& InType, const EHoudiniToolSelectionType& InSelType,
        const FText& InToolTipText, const FSlateBrush* InIcon, const FString& InHelpURL,
        const bool& isDefault )
        : HoudiniAsset( InHoudiniAsset )
        , Name( InName )
        , ToolTipText( InToolTipText )
        , Icon( InIcon )
        , HelpURL( InHelpURL )
        , Type( InType )
        , DefaultTool( isDefault )
        , SelectionType( InSelType )
    {
    }
    TSoftObjectPtr < class UHoudiniAsset > HoudiniAsset;

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
    bool DefaultTool;

    /** Indicate what the tool should consider for selection **/
    EHoudiniToolSelectionType SelectionType;

    /** Path to the Asset used **/
    FString AssetPath;
};

class FHoudiniEngineStyle
{
public:
    static void Initialize();
    static void Shutdown();
    static TSharedPtr<class ISlateStyle> Get();
    static FName GetStyleSetName();

private:
    //static FString InContent(const FString &RelativePath, const ANSICHAR *Extension);

    static TSharedPtr<class FSlateStyleSet> StyleSet;
};

class FHoudiniEngineEditor : public IHoudiniEngineEditor, public FEditorUndoClient
{
    public:
        FHoudiniEngineEditor();

    /** IModuleInterface methods. **/
    public:

        virtual void StartupModule() override;
        virtual void ShutdownModule() override;

    /** IHoudiniEngineEditor methods. **/
    public:

        virtual void RegisterComponentVisualizers() override;
        virtual void UnregisterComponentVisualizers() override;
        virtual void RegisterDetails() override;
        virtual void UnregisterDetails() override;
        virtual void RegisterAssetTypeActions() override;
        virtual void UnregisterAssetTypeActions() override;
        virtual void RegisterAssetBrokers() override;
        virtual void UnregisterAssetBrokers() override;
        virtual void RegisterActorFactories() override;
        virtual void ExtendMenu() override;
        virtual void RegisterThumbnails() override;
        virtual void UnregisterThumbnails() override;
        virtual void RegisterForUndo() override;
        virtual void UnregisterForUndo() override;
        virtual void RegisterModes() override;
        virtual void UnregisterModes() override;
        virtual void RegisterPlacementModeExtensions() override;
        virtual void UnregisterPlacementModeExtensions() override;

    /** FEditorUndoClient methods. **/
    public:

        virtual bool MatchesContext( const FString & InContext, UObject * PrimaryObject ) const override;
        virtual void PostUndo( bool bSuccess ) override;
        virtual void PostRedo( bool bSuccess ) override;

    public:

        /** App identifier string. **/
        static const FName HoudiniEngineEditorAppIdentifier;

        /** Selected Houdini Tool Dir **/
        int32 CurrentHoudiniToolDirIndex;

    public:

        /** Return singleton instance of Houdini Engine Editor, used internally. **/
        static FHoudiniEngineEditor & Get();

        /** Return true if singleton instance has been created. **/
        static bool IsInitialized();

    public:

        /** Menu action called to save a HIP file. **/
        void SaveHIPFile();

        /** Helper delegate used to determine if HIP file save can be executed. **/
        bool CanSaveHIPFile() const;

        /** Menu action called to report a bug. **/
        void ReportBug();

        /** Helper delegate used to determine if report a bug can be executed. **/
        bool CanReportBug() const;

        /** Menu action called to open the current scene in Houdini. **/
        void OpenInHoudini();

        /** Helper delegate used to determine if open in Houdini can be executed. **/
        bool CanOpenInHoudini() const; 

        /** Menu action called to clean up all unused files in the cook temp folder **/
        void CleanUpTempFolder();

        /** Helper delegate used to determine if Clean up temp can be executed. **/
        bool CanCleanUpTempFolder() const;

        /** Menu action to bake/replace all current Houdini Assets with blueprints **/
        void BakeAllAssets();

        /** Helper function for baking/replacing the current select Houdini Assets with blueprints **/
        void BakeSelection();

        /** Helper delegate used to determine if BakeAllAssets can be executed. **/
        bool CanBakeAllAssets() const;

        /** Helper function for restarting the current Houdini Engine session. **/
        void RestartSession();

        /** Returns the Default Icon to be used by Houdini Tools**/
        FString GetDefaultHoudiniToolIcon() const;

        /** Returns the HoudiniTools currently available for the shelf **/
        const TArray< TSharedPtr<FHoudiniTool> >& GetHoudiniTools() { return HoudiniTools; }

        /** Reads the Houdini Tool Description from a JSON file **/
        bool GetHoudiniToolDescriptionFromJSON(
            const FString& JsonFilePath, FHoudiniToolDescription& HoudiniToolDesc );

        bool WriteJSONFromHoudiniTool(const FHoudiniTool& Tool);

        /** Returns the HoudiniTools array  **/
        TArray< TSharedPtr<FHoudiniTool> >* GetHoudiniToolsForWrite() { return &HoudiniTools; }

        /** Adds a new Houdini Tools to the array **/
        void AddHoudiniTool( const FHoudiniTool& NewTool );

        /** Menu action to pause cooking for all Houdini Assets  **/
        void PauseAssetCooking();

        /** Helper delegate used to determine if PauseAssetCooking can be executed. **/
        bool CanPauseAssetCooking();

        /** Helper delegate used to get the current state of PauseAssetCooking. **/
        bool IsAssetCookingPaused();

        /** Helper function for recooking all assets in the current level **/
        void RecookAllAssets();

        /** Helper function for rebuilding all assets in the current level **/
        void RebuildAllAssets();

        /** Helper function for recooking selected assets **/
        void RecookSelection();

        /** Helper function for rebuilding selected assets **/
        void RebuildSelection();

        /** Helper function for accessing the current CB selection **/
        static int32 GetContentBrowserSelection( TArray< UObject* >& ContentBrowserSelection );

        /** Helper function for accessing the current world selection **/
        static int32 GetWorldSelection( TArray< UObject* >& WorldSelection, bool bHoudiniAssetActorsOnly = false );

        /** Helper function for retrieving an HoudiniTool in the Editor list **/
        bool FindHoudiniTool( const FHoudiniTool& Tool, int32& FoundIndex, bool& IsDefault );

        /** Helper function for retrieving an HoudiniTool in the Houdini Runtime Settings list **/
        bool FindHoudiniToolInHoudiniSettings( const FHoudiniTool& Tool, int32& FoundIndex );

        /** Rebuild the editor's Houdini Tool list **/
        void UpdateHoudiniToolList(int32 SelectedDir = -1);

        /** Rebuild the editor's Houdini Tool list for a directory **/
        void UpdateHoudiniToolList(const FHoudiniToolDirectory& HoudiniToolsDirectory, const bool& isDefault );

        /** Return the directories where we should look for houdini tools**/
        void GetHoudiniToolDirectories(TArray<FHoudiniToolDirectory>& HoudiniToolsDirectoryArray) const;

    protected:

        /** Register AssetType action. **/
        void RegisterAssetTypeAction( IAssetTools & AssetTools, TSharedRef< IAssetTypeActions > Action );

        /** Binds the menu extension's UICommands to their corresponding functions **/
        void BindMenuCommands();

        /** Add menu extension for our module. **/
        void AddHoudiniMenuExtension( FMenuBuilder & MenuBuilder );

        /** Add the default Houdini Tools to the Houdini Engine Shelft tool **/
        void AddDefaultHoudiniToolToArray( TArray< FHoudiniToolDescription >& ToolArray );

        /** Adds the custom Houdini Engine console commands **/
        void RegisterConsoleCommands();

        /** Adds the custom Houdini Engine commands to the world outliner context menu **/
        void AddLevelViewportMenuExtender();

        /** Removes the custom Houdini Engine commands from the world outliner context menu **/
        void RemoveLevelViewportMenuExtender();

        /** Return all the custom Houdini Engine commands for the world outliner context menu **/
        TSharedRef<FExtender> GetLevelViewportContextMenuExtender(
            const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> InActors );

    private:

        /** Singleton instance of Houdini Engine Editor. **/
        static FHoudiniEngineEditor * HoudiniEngineEditorInstance;

    private:

        /** AssetType actions associated with Houdini asset. **/
        TArray< TSharedPtr< IAssetTypeActions > > AssetTypeActions;

        /** Visualizer for our spline component. **/
        TSharedPtr< FComponentVisualizer > HandleComponentVisualizer;

        /** Visualizer for our spline component. **/
        TSharedPtr< FComponentVisualizer > SplineComponentVisualizer;

        /** Broker associated with Houdini asset. **/
        TSharedPtr< IComponentAssetBroker > HoudiniAssetBroker;

        /** The extender to pass to the level editor to extend it's window menu. **/
        TSharedPtr< FExtender > MainMenuExtender;

        /** Stored last used Houdini component which was involved in undo. **/
        mutable UHoudiniAssetComponent * LastHoudiniAssetComponentUndoObject;

        TArray< TSharedPtr<FHoudiniTool> > HoudiniTools;

        TSharedPtr<class FUICommandList> HEngineCommands;

        FDelegateHandle LevelViewportExtenderHandle;	
};


/**
* Class containing commands for Houdini Engine actions
*/
class FHoudiniEngineCommands : public TCommands<FHoudiniEngineCommands>
{
public:
    FHoudiniEngineCommands()
        : TCommands<FHoudiniEngineCommands>
        (
            TEXT("HoudiniEngine"), // Context name for fast lookup
            NSLOCTEXT("Contexts", "HoudiniEngine", "Houdini Engine Plugin"), // Localized context name for displaying
            NAME_None, // Parent context name. 
            FHoudiniEngineStyle::GetStyleSetName() // Icon Style Set
        )
    {
    }

    // TCommand<> interface
    virtual void RegisterCommands() override;

    /** Menu action called to save a HIP file. **/
    TSharedPtr<FUICommandInfo> SaveHIPFile;

    /** Menu action called to report a bug. **/
    TSharedPtr<FUICommandInfo> ReportBug;

    /** Menu action called to open the current scene in Houdini. **/
    TSharedPtr<FUICommandInfo> OpenInHoudini;

    /** Menu action called to clean up all unused files in the cook temp folder **/
    TSharedPtr<FUICommandInfo> CleanUpTempFolder;

    /** Menu action to bake/replace all current Houdini Assets with blueprints **/
    TSharedPtr<FUICommandInfo> BakeAllAssets;

    /** Menu action to pause cooking for all Houdini Assets  **/
    TSharedPtr<FUICommandInfo> PauseAssetCooking;

    /** UI Action to recook the current world selection  **/
    TSharedPtr<FUICommandInfo> CookSelec;

    /** UI Action to rebuild the current world selection  **/
    TSharedPtr<FUICommandInfo> RebuildSelec;

    /** UI Action to bake and replace the current world selection  **/
    TSharedPtr<FUICommandInfo> BakeSelec;

};

