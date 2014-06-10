/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#include "HoudiniEnginePrivatePCH.h"


const FName FHoudiniEngine::HoudiniEngineAppIdentifier = FName(TEXT("HoudiniEngineApp"));

IMPLEMENT_MODULE(FHoudiniEngine, HoudiniEngine);
DEFINE_LOG_CATEGORY(LogHoudiniEngine);


FHoudiniEngine* 
FHoudiniEngine::HoudiniEngineInstance = nullptr;


TSharedPtr<FSlateDynamicImageBrush> 
FHoudiniEngine::GetHoudiniLogoBrush() const
{
	return HoudiniLogoBrush;
}


FHoudiniEngine& 
FHoudiniEngine::Get()
{
	check(FHoudiniEngine::HoudiniEngineInstance);
	return *FHoudiniEngine::HoudiniEngineInstance;
}


void
FHoudiniEngine::StartupModule()
{
	HOUDINI_LOG_MESSAGE(TEXT("Starting the Houdini Engine module."));

	// Store the instance.
	FHoudiniEngine::HoudiniEngineInstance = this;

	// Create and register asset type actions for Houdini asset.
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FHoudiniAssetTypeActions()));

	// Create and register broker for Houdini asset.
	HoudiniAssetBroker = MakeShareable(new FHoudiniAssetBroker);
	FComponentAssetBrokerage::RegisterBroker(HoudiniAssetBroker, UHoudiniAssetComponent::StaticClass(), true, true);

	// Register thumbnail renderer for Houdini asset.
	UThumbnailManager::Get().RegisterCustomRenderer(UHoudiniAsset::StaticClass(), UHoudiniAssetThumbnailRenderer::StaticClass());

	// Create Houdini logo brush.
	const TArray<FPluginStatus> Plugins = IPluginManager::Get().QueryStatusForAllPlugins();
	for(auto PluginIt(Plugins.CreateConstIterator()); PluginIt; ++PluginIt)
	{
		const FPluginStatus& PluginStatus = *PluginIt;
		if(PluginStatus.Name == TEXT("HoudiniEngine"))
		{
			if(FPlatformFileManager::Get().GetPlatformFile().FileExists(*PluginStatus.Icon128FilePath))
			{
				const FName BrushName(*PluginStatus.Icon128FilePath);
				const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);

				if(Size.X > 0 && Size.Y > 0)
				{
					HoudiniLogoBrush = MakeShareable(new FSlateDynamicImageBrush(BrushName, FVector2D( Size.X, Size.Y)));
				}
			}

			break;
		}
	}

	// Perform HAPI initialization.
	HAPI_CookOptions CookOptions = HAPI_CookOptions_Create();
	CookOptions.maxVerticesPerPrimitive = 3;
	HAPI_Result Result = HAPI_Initialize("", "", CookOptions, true, -1);

	if(HAPI_RESULT_SUCCESS == Result)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Successfully intialized the Houdini Engine API module."));
	}
	else
	{
		HOUDINI_LOG_MESSAGE(TEXT("Starting up the Houdini Engine API module failed: %s"), *FHoudiniEngineUtils::GetErrorDescription(Result));
	}
}


void
FHoudiniEngine::ShutdownModule()
{
	HOUDINI_LOG_MESSAGE(TEXT("Shutting down the Houdini Engine module."));

	if(UObjectInitialized())
	{
		// Unregister broker.
		FComponentAssetBrokerage::UnregisterBroker(HoudiniAssetBroker);

		// Unregister thumbnail renderer.
		UThumbnailManager::Get().UnregisterCustomRenderer(UHoudiniAsset::StaticClass());
	}

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

	// Perform HAPI finalization.
	HAPI_Cleanup();
}


void
FHoudiniEngine::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	AssetTypeActions.Add(Action);
}
