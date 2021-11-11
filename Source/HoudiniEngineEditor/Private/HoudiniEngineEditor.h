/*
* Copyright (c) <2021> Side Effects Software Inc.
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

#include "IHoudiniEngineEditor.h"
#include "HoudiniInputTypes.h"

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Framework/Commands/UICommandList.h"
#include "Brushes/SlateDynamicImageBrush.h"


class FExtender;
class IAssetTools;
class IAssetTypeActions;
class IComponentAssetBroker;
class FComponentVisualizer;
class FMenuBuilder;
class FMenuBarBuilder;
class FUICommandList;
class AActor;

struct IConsoleCommand;
struct FSlateDynamicImageBrush;

enum class EHoudiniCurveType : int8;
enum class EHoudiniCurveMethod: int8;
enum class EHoudiniLandscapeOutputBakeType: uint8;
enum class EHoudiniEngineBakeOption : uint8;
enum class EPDGBakeSelectionOption : uint8;
enum class EPDGBakePackageReplaceModeOption : uint8;
enum class EPackageReplaceMode : int8;

class HOUDINIENGINEEDITOR_API FHoudiniEngineEditor : public IHoudiniEngineEditor
{
	public:
		FHoudiniEngineEditor();

		// IModuleInterface methods.
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;

		// IHoudiniEngineEditor methods
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
		virtual void RegisterForUndo() override;
		virtual void UnregisterForUndo() override;
		virtual void RegisterPlacementModeExtensions() override;
		virtual void UnregisterPlacementModeExtensions() override;

		// Return singleton instance of Houdini Engine Editor, used internally.
		static FHoudiniEngineEditor & Get();

		// Return true if singleton instance has been created.
		static bool IsInitialized();

		// Returns the plugin's directory
		static FString GetHoudiniEnginePluginDir();

		// Initializes Widget resources
		void InitializeWidgetResource();

		// Menu action to pause cooking for all Houdini Assets
		void PauseAssetCooking();

		// Helper delegate used to determine if PauseAssetCooking can be executed.
		bool CanPauseAssetCooking();

		// Helper delegate used to get the current state of PauseAssetCooking.
		bool IsAssetCookingPaused();

		// Returns a pointer to the input choice types
		TArray<TSharedPtr<FString>>* GetInputTypeChoiceLabels() { return &InputTypeChoiceLabels; };
		TArray<TSharedPtr<FString>>* GetBlueprintInputTypeChoiceLabels() { return &BlueprintInputTypeChoiceLabels; };

		// Returns a pointer to the Houdini curve types
		TArray<TSharedPtr<FString>>* GetHoudiniCurveTypeChoiceLabels() { return &HoudiniCurveTypeChoiceLabels; };

		// Returns a pointer to the Houdini curve methods
		TArray<TSharedPtr<FString>>* GetHoudiniCurveMethodChoiceLabels() { return &HoudiniCurveMethodChoiceLabels; };

		// Returns a pointer to the Houdini curve methods
		TArray<TSharedPtr<FString>>* GetHoudiniCurveBreakpointParameterizationChoiceLabels() { return &HoudiniCurveBreakpointParameterizationChoiceLabels; };
	
		// Returns a pointer to the Houdini ramp parameter interpolation methods
		TArray<TSharedPtr<FString>>* GetHoudiniParameterRampInterpolationMethodLabels() {return &HoudiniParameterRampInterpolationLabels;}

		// Returns a pointer to the Houdini curve output export types
		TArray<TSharedPtr<FString>>* GetHoudiniCurveOutputExportTypeLabels() { return &HoudiniCurveOutputExportTypeLabels; };

		TArray<TSharedPtr<FString>>* GetHoudiniLandscapeOutputBakeOptionsLabels() { return &HoudiniLandscapeOutputBakeOptionLabels; };

		// Returns a pointer to the Houdini Engine PDG Bake Type labels
		TArray<TSharedPtr<FString>>* GetHoudiniEnginePDGBakeTypeOptionsLabels() { return &HoudiniEnginePDGBakeTypeOptionLabels; };

		// Returns a pointer to the Houdini Engine Bake Type labels
		TArray<TSharedPtr<FString>>* GetHoudiniEngineBakeTypeOptionsLabels() { return &HoudiniEngineBakeTypeOptionLabels; };

		// Returns a pointer to the Houdini Engine PDG Bake Target labels
		TArray<TSharedPtr<FString>>* GetHoudiniEnginePDGBakeSelectionOptionsLabels() { return &HoudiniEnginePDGBakeSelectionOptionLabels; };

		// Returns a pointer to the Houdini Engine PDG Bake Package Replace Mode labels
		TArray<TSharedPtr<FString>>* GetHoudiniEnginePDGBakePackageReplaceModeOptionsLabels() { return &HoudiniEnginePDGBakePackageReplaceModeOptionLabels; };

		// Returns a shared Ptr to the Houdini logo
		TSharedPtr<FSlateDynamicImageBrush> GetHoudiniLogoBrush() const { return HoudiniLogoBrush; };
		TSharedPtr<FSlateDynamicImageBrush> GetHoudiniEngineLogoBrush() const { return HoudiniEngineLogoBrush; };

		// Functions Return a shared Ptr to the Houdini Engine UI Icon
		TSharedPtr<FSlateDynamicImageBrush> GetHoudiniEngineUIIconBrush() const { return HoudiniEngineUIIconBrush; }
		TSharedPtr<FSlateDynamicImageBrush> GetHoudiniEngineUIRebuildIconBrush() const { return HoudiniEngineUIRebuildIconBrush; }
		TSharedPtr<FSlateDynamicImageBrush> GetHoudiniEngineUIRecookIconBrush() const { return HoudiniEngineUIRecookIconBrush; }
		TSharedPtr<FSlateDynamicImageBrush> GetHoudiniEngineUIResetParametersIconBrush() const { return HoudiniEngineUIResetParametersIconBrush; }

		TSharedPtr<FSlateDynamicImageBrush>	GetHoudiniEngineUIBakeIconBrush() const { return HoudiniEngineUIBakeIconBrush; }
		TSharedPtr<FSlateDynamicImageBrush>	GetHoudiniEngineUICookLogIconBrush() const { return HoudiniEngineUICookLogIconBrush; }
		TSharedPtr<FSlateDynamicImageBrush> GetHoudiniEngineUIAssetHelpIconBrush() const { return HoudiniEngineUIAssetHelpIconBrush; }
		TSharedPtr<FSlateDynamicImageBrush>	GetHoudiniEngineUIPDGIconBrush() const { return HoudiniEngineUIPDGIconBrush; }
		TSharedPtr<FSlateDynamicImageBrush>	GetHoudiniEngineUIPDGCancelIconBrush() const { return HoudiniEngineUIPDGCancelIconBrush; }
		TSharedPtr<FSlateDynamicImageBrush>	GetHoudiniEngineUIPDGDirtyAllIconBrush() const { return HoudiniEngineUIPDGDirtyAllIconBrush; }
		TSharedPtr<FSlateDynamicImageBrush>	GetHoudiniEngineUIPDGDirtyNodeIconBrush() const { return HoudiniEngineUIPDGDirtyNodeIconBrush; }
		TSharedPtr<FSlateDynamicImageBrush>	GetHoudiniEngineUIPDGPauseIconBrush() const { return HoudiniEngineUIPDGPauseIconBrush; }
		TSharedPtr<FSlateDynamicImageBrush>	GetHoudiniEngineUIPDGResetIconBrush() const { return HoudiniEngineUIPDGResetIconBrush; }
		TSharedPtr<FSlateDynamicImageBrush>	GetHoudiniEngineUIPDGRefreshIconBrush() const { return HoudiniEngineUIPDGRefreshIconBrush; }

		// Returns a pointer to Unreal output curve types (for temporary)
		TArray<TSharedPtr<FString>>* GetUnrealOutputCurveTypeLabels() { return &UnrealCurveOutputCurveTypeLabels; };

		// returns string from Houdini Engine Bake Option
		FString GetStringFromHoudiniEngineBakeOption(const EHoudiniEngineBakeOption & BakeOption);

		// returns string from Houdini Engine PDG Bake Target Option
		FString GetStringFromPDGBakeTargetOption(const EPDGBakeSelectionOption& BakeOption);

		// returns string from PDG package replace mode option
		FString GetStringFromPDGBakePackageReplaceModeOption(const EPDGBakePackageReplaceModeOption & InOption);
	
		// Return HoudiniEngineBakeOption from FString
		const EHoudiniEngineBakeOption StringToHoudiniEngineBakeOption(const FString & InString);

		// Return EPDGBakeSelectionOption from FString
		const EPDGBakeSelectionOption StringToPDGBakeSelectionOption(const FString& InString);

		// Return EPDGBakePackageReplaceModeOption from FString
		const EPDGBakePackageReplaceModeOption StringToPDGBakePackageReplaceModeOption(const FString & InString);

		// Convert EPDGBakePackageReplaceModeOption to EPackageReplaceMode
		// TODO: perhaps EPackageReplaceMode can be moved to HoudiniEngineRuntime to avoid having both
		// TODO: EPDGBakePackageReplaceModeOption and EPackageReplaceMode?
		const EPackageReplaceMode PDGBakePackageReplaceModeToPackageReplaceMode(const EPDGBakePackageReplaceModeOption& InReplaceMode);

		// Get the reference of the radio button folder circle point arrays reference
		TArray<FVector2D> & GetHoudiniParameterRadioButtonPointsOuter() { return HoudiniParameterRadioButtonPointsOuter; };
		TArray<FVector2D> & GetHoudiniParameterRadioButtonPointsInner() { return HoudiniParameterRadioButtonPointsInner; };

		// Gets the PostSaveWorldOnceHandle
		FDelegateHandle& GetOnPostSaveWorldOnceHandle() { return PostSaveWorldOnceHandle; }

	protected:

		// Binds the commands used by the menus
		void BindMenuCommands();

		// Register AssetType action. 
		void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef< IAssetTypeActions > Action);

		// Add menu extension for our module.
		void AddHoudiniFileMenuExtension(FMenuBuilder& MenuBuilder);

		// Add the Houdini Engine editor menu
		void AddHoudiniEditorMenu(FMenuBarBuilder& MenuBarBuilder);

		// Add menu extension for our module.
		void AddHoudiniMainMenuExtension(FMenuBuilder & MenuBuilder);

		// Adds the custom Houdini Engine commands to the world outliner context menu
		void AddLevelViewportMenuExtender();

		// Removes the custom Houdini Engine commands to the world outliner context menu
		void RemoveLevelViewportMenuExtender();

		// Returns all the custom Houdini Engine commands for the world outliner context menu
		TSharedRef<FExtender> GetLevelViewportContextMenuExtender(
			const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> InActors);

		// Register all console commands provided by this module
		void RegisterConsoleCommands();

		// Unregister all registered console commands provided by this module
		void UnregisterConsoleCommands();

		// Register for any FEditorDelegates that we are interested in, such as
		// PreSaveWorld and PreBeginPIE, for HoudiniStaticMesh -> UStaticMesh builds
		void RegisterEditorDelegates();

		// Deregister editor delegates
		void UnregisterEditorDelegates();

		// Process the OnDeleteActorsBegin call received from FEditorDelegates.
		// Check if any AHoudiniAssetActors with PDG links are selected for deletion. If so,
		// check if these still have temporary outputs and give the user to option to skip
		// deleting the ones with temporary output.
		void HandleOnDeleteActorsBegin();

		// Re-select AHoudiniAssetActors that were deselected (to avoid deletion) by HandleOnDeleteActorsBegin 
		void HandleOnDeleteActorsEnd();

	private:

		// Singleton instance of Houdini Engine Editor.
		static FHoudiniEngineEditor * HoudiniEngineEditorInstance;

		// AssetType actions associated with Houdini asset.
		TArray<TSharedPtr<IAssetTypeActions>> AssetTypeActions;

		// Broker associated with Houdini asset.
		TSharedPtr<IComponentAssetBroker> HoudiniAssetBroker;

		// Widget resources: Input Type combo box labels
		TArray<TSharedPtr<FString>> InputTypeChoiceLabels;
		TArray<TSharedPtr<FString>> BlueprintInputTypeChoiceLabels;

		// Widget resources: Houdini Curve Type combo box labels
		TArray<TSharedPtr<FString>> HoudiniCurveTypeChoiceLabels;

		// Widget resources: Houdini Curve Method combo box labels
		TArray<TSharedPtr<FString>> HoudiniCurveMethodChoiceLabels;

		// Widget resources: Houdini Curve Method combo box labels
		TArray<TSharedPtr<FString>> HoudiniCurveBreakpointParameterizationChoiceLabels;
	
		// Widget resources: Houdini Ramp Interpolation method combo box labels
		TArray<TSharedPtr<FString>> HoudiniParameterRampInterpolationLabels;

		// Widget resources: Houdini Curve Output type labels
		TArray<TSharedPtr<FString>> HoudiniCurveOutputExportTypeLabels;

		// Widget resources: Unreal Curve type labels (for temporary, we need to figure out a way to access the output curve's info later)
		TArray<TSharedPtr<FString>> UnrealCurveOutputCurveTypeLabels;

		// Widget resources: Landscape output Bake type labels
		TArray<TSharedPtr<FString>> HoudiniLandscapeOutputBakeOptionLabels;

		// Widget resources: PDG Bake type labels
		TArray<TSharedPtr<FString>> HoudiniEnginePDGBakeTypeOptionLabels;

		// Widget resources: Bake type labels
		TArray<TSharedPtr<FString>> HoudiniEngineBakeTypeOptionLabels;

		// Widget resources: PDG Bake target labels
		TArray<TSharedPtr<FString>> HoudiniEnginePDGBakeSelectionOptionLabels;

		// Widget resources: PDG Bake package replace mode labels
		TArray<TSharedPtr<FString>> HoudiniEnginePDGBakePackageReplaceModeOptionLabels;

		// List of UI commands used by the various menus
		TSharedPtr<class FUICommandList> HEngineCommands;

		// Houdini logo brush.
		TSharedPtr<FSlateDynamicImageBrush> HoudiniLogoBrush;
		// Houdini Engine logo brush
		TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineLogoBrush;

		// houdini Engine UI Brushes
		TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineUIIconBrush;
		TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineUIRebuildIconBrush;
		TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineUIRecookIconBrush;
		TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineUIResetParametersIconBrush;

		TSharedPtr<FSlateDynamicImageBrush>	HoudiniEngineUIBakeIconBrush;
		TSharedPtr<FSlateDynamicImageBrush>	HoudiniEngineUICookLogIconBrush;
		TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineUIAssetHelpIconBrush;
		TSharedPtr<FSlateDynamicImageBrush>	HoudiniEngineUIPDGIconBrush;
		TSharedPtr<FSlateDynamicImageBrush>	HoudiniEngineUIPDGCancelIconBrush;
		TSharedPtr<FSlateDynamicImageBrush>	HoudiniEngineUIPDGDirtyAllIconBrush;
		TSharedPtr<FSlateDynamicImageBrush>	HoudiniEngineUIPDGDirtyNodeIconBrush;
		TSharedPtr<FSlateDynamicImageBrush>	HoudiniEngineUIPDGPauseIconBrush;
		TSharedPtr<FSlateDynamicImageBrush>	HoudiniEngineUIPDGResetIconBrush;
		TSharedPtr<FSlateDynamicImageBrush>	HoudiniEngineUIPDGRefreshIconBrush;

		// The extender to pass to the level editor to extend it's File menu.
		TSharedPtr<FExtender> MainMenuExtender;

		// The extender to pass to the level editor to extend it's Main menu.
		//TSharedPtr<FExtender> FileMenuExtender;

		// DelegateHandle for the viewport's context menu extender
		FDelegateHandle LevelViewportExtenderHandle;

		// SplineComponentVisualizer
		TSharedPtr<FComponentVisualizer> SplineComponentVisualizer;

		TSharedPtr<FComponentVisualizer> HandleComponentVisualizer;

		// Array of HoudiniEngine console commands
		TArray<IConsoleCommand*> ConsoleCommands;

		// Delegate handle for the PreSaveWorld editor delegate
		FDelegateHandle PreSaveWorldEditorDelegateHandle;

		// Delegate handle for the PostSaveWorld editor delegate: this
		// is bound on PreSaveWorld with specific captures and then unbound
		// by itself
		FDelegateHandle PostSaveWorldOnceHandle;

		// Delegate handle for the PreBeginPIE editor delegate
		FDelegateHandle PreBeginPIEEditorDelegateHandle;

		// Delegate handle for the EndPIE editor delegate
		FDelegateHandle EndPIEEditorDelegateHandle;

		// Delegate handle for OnDeleteActorsBegin
		FDelegateHandle OnDeleteActorsBegin;

		// Delegate handle for OnDeleteActorsEnd
		FDelegateHandle OnDeleteActorsEnd;

		// List of actors that HandleOnDeleteActorsBegin marked to _not_ be deleted. This
		// is used to re-select these actors in HandleOnDeleteActorsEnd.
		TArray<AActor*> ActorsToReselectOnDeleteActorsEnd;

		// Cache the points of radio button folder circle points to avoid huge amount of repeat computation.
		// (Computing points are time consuming since it uses trigonometric functions)
		TArray<FVector2D> HoudiniParameterRadioButtonPointsOuter;
		TArray<FVector2D> HoudiniParameterRadioButtonPointsInner;
};