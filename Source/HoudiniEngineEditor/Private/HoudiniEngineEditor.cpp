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
#include "HoudiniEngineEditor.h"
#include "HoudiniEngine.h"

#include "HoudiniSplineComponentVisualizer.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniAssetComponentDetails.h"
#include "HoudiniRuntimeSettingsDetails.h"
#include "HoudiniAssetTypeActions.h"


const FName
FHoudiniEngineEditor::HoudiniEngineEditorAppIdentifier = FName(TEXT("HoudiniEngineEditorApp"));


IMPLEMENT_MODULE(FHoudiniEngineEditor, HoudiniEngineEditor);
DEFINE_LOG_CATEGORY(LogHoudiniEngineEditor);


FHoudiniEngineEditor*
FHoudiniEngineEditor::HoudiniEngineEditorInstance = nullptr;


FHoudiniEngineEditor&
FHoudiniEngineEditor::Get()
{
	return *HoudiniEngineEditorInstance;
}


bool
FHoudiniEngineEditor::IsInitialized()
{
	return FHoudiniEngineEditor::HoudiniEngineEditorInstance != nullptr;
}


void
FHoudiniEngineEditor::StartupModule()
{
	HOUDINI_LOG_MESSAGE(TEXT("Starting the Houdini Engine Editor module."));

	// Load Houdini Engine Runtime module.
	//IHoudiniEngine& HoudiniEngine = FModuleManager::LoadModuleChecked<FHoudiniEngine>("HoudiniEngine").Get();

	// Register asset type actions.
	RegisterAssetTypeActions();

	// Register component visualizers.
	RegisterComponentVisualizers();

	// Register detail presenters.
	RegisterDetails();

	// Store the instance.
	FHoudiniEngineEditor::HoudiniEngineEditorInstance = this;
}


void
FHoudiniEngineEditor::ShutdownModule()
{
	HOUDINI_LOG_MESSAGE(TEXT("Shutting down the Houdini Engine Editor module."));

	// Unregister asset type actions.
	UnregisterAssetTypeActions();

	// Unregister detail presenters.
	UnregisterDetails();

	// Unregister our component visualizers.
	UnregisterComponentVisualizers();
}


void
FHoudiniEngineEditor::RegisterComponentVisualizers()
{
	if(!SplineComponentVisualizer.IsValid())
	{
		SplineComponentVisualizer = MakeShareable(new FHoudiniSplineComponentVisualizer);
		GUnrealEd->RegisterComponentVisualizer(UHoudiniSplineComponent::StaticClass()->GetFName(),
			SplineComponentVisualizer);

		SplineComponentVisualizer->OnRegister();
	}
}


void
FHoudiniEngineEditor::UnregisterComponentVisualizers()
{
	if(SplineComponentVisualizer.IsValid())
	{
		GUnrealEd->UnregisterComponentVisualizer(UHoudiniSplineComponent::StaticClass()->GetFName());
	}
}


void
FHoudiniEngineEditor::RegisterDetails()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// Register details presenter for our component type and runtime settings.
	PropertyModule.RegisterCustomClassLayout(TEXT("HoudiniAssetComponent"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniAssetComponentDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(TEXT("HoudiniRuntimeSettings"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniRuntimeSettingsDetails::MakeInstance));
}


void
FHoudiniEngineEditor::UnregisterDetails()
{
	if(FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyModule.UnregisterCustomClassLayout(TEXT("HoudiniAssetComponent"));
		PropertyModule.UnregisterCustomClassLayout(TEXT("HoudiniRuntimeSettings"));
	}
}


void
FHoudiniEngineEditor::RegisterAssetTypeActions()
{
	// Create and register asset type actions for Houdini asset.
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FHoudiniAssetTypeActions()));
}


void
FHoudiniEngineEditor::UnregisterAssetTypeActions()
{
	// Unregister asset type actions we have previously registered.
	if(FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		for(int32 Index = 0; Index < AssetTypeActions.Num(); ++Index)
		{
			AssetTools.UnregisterAssetTypeActions(AssetTypeActions[Index].ToSharedRef());
		}

		AssetTypeActions.Empty();
	}
}


void
FHoudiniEngineEditor::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	AssetTypeActions.Add(Action);
}
