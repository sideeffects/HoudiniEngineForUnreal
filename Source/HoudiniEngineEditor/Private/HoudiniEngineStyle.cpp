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

#include "HoudiniEngineStyle.h"

#include "HoudiniEngineEditor.h"
#include "HoudiniEngineUtils.h"

#include "EditorStyleSet.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "SlateOptMacros.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define TTF_FONT( RelativePath, ... ) FSlateFontInfo( StyleSet->RootToContentDir( RelativePath, TEXT(".ttf") ), __VA_ARGS__ )
#define TTF_CORE_FONT( RelativePath, ... ) FSlateFontInfo( StyleSet->RootToCoreContentDir( RelativePath, TEXT(".ttf") ), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) FSlateFontInfo( StyleSet->RootToContentDir( RelativePath, TEXT(".otf") ), __VA_ARGS__ )
#define OTF_CORE_FONT( RelativePath, ... ) FSlateFontInfo( StyleSet->RootToCoreContentDir( RelativePath, TEXT(".otf") ), __VA_ARGS__ )

TSharedPtr<FSlateStyleSet> FHoudiniEngineStyle::StyleSet = nullptr;

TSharedPtr<class ISlateStyle>
FHoudiniEngineStyle::Get()
{
	return StyleSet;
}

FName
FHoudiniEngineStyle::GetStyleSetName()
{
	static FName HoudiniStyleName(TEXT("HoudiniEngineStyle"));
	return HoudiniStyleName;
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void
FHoudiniEngineStyle::Initialize()
{
	// Only register the StyleSet once
	if (StyleSet.IsValid())
		return;

	StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	// Note, these sizes are in Slate Units.
	// Slate Units do NOT have to map to pixels.
	const FVector2D Icon5x16(5.0f, 16.0f);
	const FVector2D Icon8x4(8.0f, 4.0f);
	const FVector2D Icon8x8(8.0f, 8.0f);
	const FVector2D Icon10x10(10.0f, 10.0f);
	const FVector2D Icon12x12(12.0f, 12.0f);
	const FVector2D Icon12x16(12.0f, 16.0f);
	const FVector2D Icon14x14(14.0f, 14.0f);
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon22x22(22.0f, 22.0f);
	const FVector2D Icon24x24(24.0f, 24.0f);
	const FVector2D Icon25x25(25.0f, 25.0f);
	const FVector2D Icon32x32(32.0f, 32.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);
	const FVector2D Icon64x64(64.0f, 64.0f);
	const FVector2D Icon36x24(36.0f, 24.0f);
	const FVector2D Icon128x128(128.0f, 128.0f);

	static FString IconsDir = FHoudiniEngineUtils::GetHoudiniEnginePluginDir() / TEXT("Resources/Icons/");
	StyleSet->Set(
		"HoudiniEngine.HoudiniEngineLogo",
		new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));
	
	StyleSet->Set(
		"ClassIcon.HoudiniAssetActor",
		new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));

	StyleSet->Set(
		"ClassThumbnail.HoudiniAssetActor",
		new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_128.png"), Icon64x64));

	StyleSet->Set(
		"HoudiniEngine.HoudiniEngineLogo40",
		new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_40.png"), Icon40x40));

	StyleSet->Set(
		"ClassIcon.HoudiniAsset",
		new FSlateImageBrush(IconsDir + TEXT("houdini_digital_asset.png"), Icon16x16));

	StyleSet->Set(
		"ClassThumbnail.HoudiniAsset",
		new FSlateImageBrush(IconsDir + TEXT("houdini_digital_asset_128.png"), Icon64x64));

	static FString ResourcesDir = FHoudiniEngineUtils::GetHoudiniEnginePluginDir() / TEXT("Resources/");

	FString AssetHelpIcon = IconsDir + TEXT("asset_help16x16.png");
	FString BakeAllIcon = IconsDir + TEXT("bake_all16x16.png");
	FString BakeSelIcon = IconsDir + TEXT("bake_selected16x16.png");
	FString CleanTempIcon = IconsDir + TEXT("clean_temp16x16.png");
	FString CookAllIcon = IconsDir + TEXT("cook_all16x16.png");
	FString CookLogIcon = IconsDir + TEXT("cook_log16x16.png");
	FString CookSelIcon = IconsDir + TEXT("cook_selected16x16.png");
	FString DigitalAssetIcon = IconsDir + TEXT("digital_asset16x16.png");
	FString OnlineForumIcon = IconsDir + TEXT("online_forum16x16.png");
	FString OnlineHelpIcon = IconsDir + TEXT("online_help16x16.png");
	FString OpenInHIcon = IconsDir + TEXT("open_in_houdini16x16.png");
	FString PauseIcon = IconsDir + TEXT("pause16x16.png");
	FString PDGCancelIcon = IconsDir + TEXT("pdg_cancel16x16.png");
	FString PDGDirtyAllIcon = IconsDir + TEXT("pdg_dirty_all16x16.png");
	FString PDGDirtyNodeIcon = IconsDir + TEXT("pdg_dirty_node16x16.png");
	FString PDGLinkIcon = IconsDir + TEXT("pdg_link16x16.png");
	FString PDGPauseIcon = IconsDir + TEXT("pdg_pause16x16.png");
	FString PDGRefreshIcon = IconsDir + TEXT("pdg_refresh16x16.png");
	FString PDGResetIcon = IconsDir + TEXT("pdg_reset16x16.png");
	FString RebuildAllIcon = IconsDir + TEXT("rebuild_all16x16.png");
	FString RebuildSelIcon = IconsDir + TEXT("rebuild_selected16x16.png");
	FString RefineAllIcon = IconsDir + TEXT("refine_all16x16.png");
	FString RefineSelIcon = IconsDir + TEXT("refine_selected16x16.png");
	FString ReportBugIcon = IconsDir + TEXT("report_bug16x16.png");
	FString ResetIcon = IconsDir + TEXT("reset16x16.png");
	FString ResetParamIcon = IconsDir + TEXT("reset_parameters16x16.png");
	FString SaveToHipIcon = IconsDir + TEXT("save_to_hip16x16.png");
	FString SessionConnectIcon = IconsDir + TEXT("session_connect16x16.png");
	FString SessionCreateIcon = IconsDir + TEXT("session_create16x16.png");
	FString SessionRestartIcon = IconsDir + TEXT("session_restart16x16.png");
	FString SessionStopIcon = IconsDir + TEXT("session_stop16x16.png");
	FString SessionSyncIcon = IconsDir + TEXT("session_sync16x16.png");
	FString SessionSyncStartIcon = IconsDir + TEXT("session_sync_start16x16.png");
	FString SessionSyncStopIcon = IconsDir + TEXT("session_sync_stop16x16.png");
	FString ViewportSyncIcon = IconsDir + TEXT("viewport_sync16x16.png");
	FString ViewportSyncBothIcon = IconsDir + TEXT("viewport_sync_both16x16.png");
	FString ViewportSyncHoudiniIcon = IconsDir + TEXT("viewport_sync_houdini16x16.png");
	FString ViewportSyncOffIcon = IconsDir + TEXT("viewport_sync_off16x16.png");
	FString ViewportSyncUnrealIcon = IconsDir + TEXT("viewport_sync_unreal16x16.png");

	FString InfoIcon = FEditorStyle::GetBrush("Icons.Info")->GetResourceName().ToString();
	FString SettingsIcon = FEditorStyle::GetBrush("Launcher.EditSettings")->GetResourceName().ToString();

	StyleSet->Set("HoudiniEngine._CreateSession", new FSlateImageBrush(SessionCreateIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._ConnectSession", new FSlateImageBrush(SessionConnectIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._StopSession", new FSlateImageBrush(SessionStopIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._RestartSession", new FSlateImageBrush(SessionRestartIcon, Icon16x16));

	StyleSet->Set("HoudiniEngine._SessionSync", new FSlateImageBrush(SessionSyncIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._OpenSessionSync", new FSlateImageBrush(SessionSyncStartIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._CloseSessionSync", new FSlateImageBrush(SessionSyncStopIcon, Icon16x16));

	StyleSet->Set("HoudiniEngine._SyncViewport", new FSlateImageBrush(ViewportSyncIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._ViewportSyncNone", new FSlateImageBrush(ViewportSyncOffIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._ViewportSyncBoth", new FSlateImageBrush(ViewportSyncBothIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._ViewportSyncUnreal", new FSlateImageBrush(ViewportSyncUnrealIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._ViewportSyncHoudini", new FSlateImageBrush(ViewportSyncHoudiniIcon, Icon16x16));

	StyleSet->Set("HoudiniEngine._InstallInfo", new FSlateImageBrush(InfoIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._PluginSettings", new FSlateImageBrush(SettingsIcon, Icon16x16));

	StyleSet->Set("HoudiniEngine._OpenInHoudini", new FSlateImageBrush(OpenInHIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._SaveHIPFile", new FSlateImageBrush(SaveToHipIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._CleanUpTempFolder", new FSlateImageBrush(CleanTempIcon, Icon16x16));

	StyleSet->Set("HoudiniEngine._OnlineDoc", new FSlateImageBrush(OnlineHelpIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._OnlineForum", new FSlateImageBrush(OnlineForumIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._ReportBug", new FSlateImageBrush(ReportBugIcon, Icon16x16));

	StyleSet->Set("HoudiniEngine._CookAll", new FSlateImageBrush(CookAllIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._CookSelected", new FSlateImageBrush(CookSelIcon, Icon16x16));

	StyleSet->Set("HoudiniEngine._BakeSelected", new FSlateImageBrush(BakeSelIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._BakeAll", new FSlateImageBrush(BakeAllIcon, Icon16x16));

	StyleSet->Set("HoudiniEngine._RebuildAll", new FSlateImageBrush(RebuildAllIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._RebuildSelected", new FSlateImageBrush(RebuildSelIcon, Icon16x16));

	StyleSet->Set("HoudiniEngine._RefineAll", new FSlateImageBrush(RefineAllIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._RefineSelected", new FSlateImageBrush(RefineSelIcon, Icon16x16));

	StyleSet->Set("HoudiniEngine._PauseAssetCooking", new FSlateImageBrush(PauseIcon, Icon16x16));

	StyleSet->Set("HoudiniEngine._Reset", new FSlateImageBrush(ResetIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine.DigitalAsset", new FSlateImageBrush(DigitalAssetIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine.PDGLink", new FSlateImageBrush(PDGLinkIcon, Icon16x16));

	/*
	FString StopIcon = FEditorStyle::GetBrush("PropertyWindow.Button_Clear")->GetResourceName().ToString();
	FString RestartIcon = FEditorStyle::GetBrush("Tutorials.Browser.RestartButton")->GetResourceName().ToString();
	FString InfoIcon = FEditorStyle::GetBrush("Icons.Info")->GetResourceName().ToString();
	FString SettingsIcon = FEditorStyle::GetBrush("Launcher.EditSettings")->GetResourceName().ToString();
	FString ClearIcon = FEditorStyle::GetBrush("PropertyWindow.Button_Delete")->GetResourceName().ToString();
	FString HelpIcon = FEditorStyle::GetBrush("Icons.Help")->GetResourceName().ToString();
	FString WarningIcon = FEditorStyle::GetBrush("Icons.Warning")->GetResourceName().ToString();
	FString BPIcon = FEditorStyle::GetBrush("PropertyWindow.Button_CreateNewBlueprint")->GetResourceName().ToString();
	FString PauseIcon = FEditorStyle::GetBrush("Profiler.Pause")->GetResourceName().ToString();

	StyleSet->Set("HoudiniEngine._CreateSession", new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));
	StyleSet->Set("HoudiniEngine._ConnectSession", new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));
	StyleSet->Set("HoudiniEngine._StopSession", new FSlateImageBrush(StopIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._RestartSession", new FSlateImageBrush(RestartIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._OpenSessionSync", new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));
	StyleSet->Set("HoudiniEngine._CloseSessionSync", new FSlateImageBrush(StopIcon, Icon16x16));

	StyleSet->Set("HoudiniEngine._InstallInfo", new FSlateImageBrush(InfoIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._PluginSettings", new FSlateImageBrush(SettingsIcon, Icon16x16));


	StyleSet->Set("HoudiniEngine._OpenInHoudini", new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));
	StyleSet->Set("HoudiniEngine._SaveHIPFile", new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));
	StyleSet->Set("HoudiniEngine._CleanUpTempFolder", new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));

	StyleSet->Set("HoudiniEngine._OnlineDoc", new FSlateImageBrush(HelpIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._OnlineForum", new FSlateImageBrush(InfoIcon, Icon16x16));
	StyleSet->Set("HoudiniEngine._ReportBug", new FSlateImageBrush(WarningIcon, Icon16x16));

	StyleSet->Set("HoudiniEngine._CookAll", new FSlateImageBrush(ResourcesDir + TEXT("hengine_recook_icon.png"), Icon16x16));
	StyleSet->Set("HoudiniEngine._CookSelected", new FSlateImageBrush(ResourcesDir + TEXT("hengine_recook_icon.png"), Icon16x16));

	StyleSet->Set("HoudiniEngine._BakeSelected", new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));
	StyleSet->Set("HoudiniEngine._BakeAll", new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));

	StyleSet->Set("HoudiniEngine._RebuildAll", new FSlateImageBrush(ResourcesDir + TEXT("hengine_reload_icon.png"), Icon16x16));
	StyleSet->Set("HoudiniEngine._RebuildSelected", new FSlateImageBrush(ResourcesDir + TEXT("hengine_reload_icon.png"), Icon16x16));

	StyleSet->Set("HoudiniEngine._RefineAll", new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));
	StyleSet->Set("HoudiniEngine._RefineSelected", new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));

	StyleSet->Set("HoudiniEngine._PauseAssetCooking", new FSlateImageBrush(IconsDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));
	*/

	// We need some colors from Editor Style & this is the only way to do this at the moment
	const FSlateColor DefaultForeground = FEditorStyle::GetSlateColor("DefaultForeground");
	const FSlateColor InvertedForeground = FEditorStyle::GetSlateColor("InvertedForeground");
	const FSlateColor SelectorColor = FEditorStyle::GetSlateColor("SelectorColor");
	const FSlateColor SelectionColor = FEditorStyle::GetSlateColor("SelectionColor");
	const FSlateColor SelectionColor_Inactive = FEditorStyle::GetSlateColor("SelectionColor_Inactive");

	const FTableRowStyle &NormalTableRowStyle = FEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
	StyleSet->Set(
		"HoudiniEngine.TableRow", FTableRowStyle(NormalTableRowStyle)
		.SetEvenRowBackgroundBrush(FSlateNoResource())
		.SetEvenRowBackgroundHoveredBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 0.1f)))
		.SetOddRowBackgroundBrush(FSlateNoResource())
		.SetOddRowBackgroundHoveredBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 0.1f)))
		.SetSelectorFocusedBrush(BORDER_BRUSH("Common/Selector", FMargin(4.f / 16.f), SelectorColor))
		.SetActiveBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, SelectionColor))
		.SetActiveHoveredBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, SelectionColor))
		.SetInactiveBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, SelectionColor_Inactive))
		.SetInactiveHoveredBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, SelectionColor_Inactive))
		.SetTextColor(DefaultForeground)
		.SetSelectedTextColor(InvertedForeground)
	);

	// Normal Text
	const FTextBlockStyle& NormalText = FEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
	StyleSet->Set(
		"HoudiniEngine.ThumbnailText", FTextBlockStyle(NormalText)
		.SetFont(TTF_CORE_FONT("Fonts/Roboto-Regular", 9))
		.SetColorAndOpacity(FSlateColor::UseForeground())
		.SetShadowOffset(FVector2D::ZeroVector)
		.SetShadowColorAndOpacity(FLinearColor::Black)
		.SetHighlightColor(FLinearColor(0.02f, 0.3f, 0.0f))
		.SetHighlightShape(BOX_BRUSH("Common/TextBlockHighlightShape", FMargin(3.f / 8.f)))
	);

	StyleSet->Set("HoudiniEngine.ThumbnailShadow", new BOX_BRUSH("ContentBrowser/ThumbnailShadow", FMargin(4.0f / 64.0f)));
	StyleSet->Set("HoudiniEngine.ThumbnailBackground", new IMAGE_BRUSH("Common/ClassBackground_64x", FVector2D(64.f, 64.f), FLinearColor(0.75f, 0.75f, 0.75f, 1.0f)));

	// Register Slate style.
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
};

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef TTF_CORE_FONT
#undef OTF_FONT
#undef OTF_CORE_FONT

void
FHoudiniEngineStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}

#undef LOCTEXT_NAMESPACE