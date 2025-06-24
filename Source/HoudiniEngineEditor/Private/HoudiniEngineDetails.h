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

#include "CoreMinimal.h"

#include "HAPI/HAPI_Common.h"
#include "HoudiniEngineDetails.h"
#include "HoudiniEngineRuntimePrivatePCH.h"

#include "Framework/SlateDelegates.h"
#include "Framework/SlateDelegates.h"
#include "Styling/SlateBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SCompoundWidget.h"

class IDetailCategoryBuilder;
class UHoudiniAssetComponent;
class UHoudiniCookable;
class UHoudiniPDGAssetLink;
class FMenuBuilder;
class SBorder;
class SButton;

#define IsValidWeakPointer(InWeakObjectPointer) \
	FHoudiniEngineDetails::IsValidWeakObjectPointer(InWeakObjectPointer, true, TEXT(__FILE__), __LINE__)


class SHoudiniAssetLogWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SHoudiniAssetLogWidget)
		: _LogText(TEXT(""))
	{}

		SLATE_ARGUMENT(FString, LogText)
	SLATE_END_ARGS()

		/** Widget construct. **/
		void Construct(const FArguments & InArgs);
};

struct EHoudiniDetailsFlags 
{
	// Controls the UI for the Houdini Details Planel. The defaults are for for HACs, but its customized
	// for PCG, where some settings should not be used.

	bool bAutoBake = true;
	bool bBakeButton = true;
	bool bDisplayOnOutputLess = false;
	bool bAssetOptions = true;
	bool bGenerateBar = true;
	bool bReplacePreviousBake = true;
	bool bRemoveHDAOutputAfterBake = true;
	bool bTemporaryCookFolderRow = true;
	bool bCookTriggers = true;
	bool bDoNotGenerateOutputs = true;
	bool bPushTransformToHoudini = true;

	static EHoudiniDetailsFlags Defaults;
};

class FHoudiniEngineDetails : public TSharedFromThis<FHoudiniEngineDetails, ESPMode::NotThreadSafe>
{
public:

	// HE ICON
	static void CreateHoudiniEngineIconWidget(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder);

	// HOUDINI ASSET + PRESET MENU
	static void CreateHoudiniAssetDetails(
		IDetailCategoryBuilder& HoudiniAssetCategory,
		TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables);

	// PRESET MENU (FULL ROW - unused)
	static void CreateHoudiniEngineActionWidget(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
		const TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables);

	// GENERATE
	static void CreateGenerateWidgets(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
		const TArray<TWeakObjectPtr<UHoudiniCookable>>& InHCs,
		const EHoudiniDetailsFlags& DetailsFlags);

	// RESET PARAMETERS - used by PCG, this is like GENERATE, but no Rebuild/Recook buttons.
	static void CreateResetParametersOnlyWidgets(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
		const TArray<TWeakObjectPtr<UHoudiniCookable>>& InHCs);

	// BAKE
	static void CreateBakeWidgets(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
		const TArray<TWeakObjectPtr<UHoudiniCookable>>& InHCs,
		const EHoudiniDetailsFlags& DetailsFlags);

	// PDG
	static void CreatePDGBakeWidgets(
		IDetailCategoryBuilder& InPDGCategory,
		UHoudiniPDGAssetLink* InPDGAssetLink);

	// ASSET OPTIONS
	static void CreateAssetOptionsWidgets(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
		const TArray<TWeakObjectPtr<UHoudiniCookable>>& InHCs,
		const EHoudiniDetailsFlags& DetailsFlags);

	// HELP DEBUG
	static void CreateHelpAndDebugWidgets(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
		const TArray<TWeakObjectPtr<UHoudiniCookable>>& InHCs);

	// NODE SYNC
	static void CreateNodeSyncWidgets(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
		const TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables);

	static void CreateInstallInfoWindow();

	static void AddRemovedHDAOutputAfterBakeCheckBox(const TWeakObjectPtr<UHoudiniCookable>& MainHC, 
		const TArray<TWeakObjectPtr<UHoudiniCookable>>& InHCs, 
		TSharedPtr<SVerticalBox>& LeftColumnVerticalBox);

	static void AddRenterBakedActorsCheckbox(const TWeakObjectPtr<UHoudiniCookable>& MainHC, 
		const TArray<TWeakObjectPtr<UHoudiniCookable>>& InHCs, 
		TSharedPtr<SVerticalBox>& LeftColumnVerticalBox);

	static void AddAutoBakeCheckbox(const TWeakObjectPtr<UHoudiniCookable>& MainHC, 
		const TArray<TWeakObjectPtr<UHoudiniCookable>>& InHCs, 
		TSharedPtr<SVerticalBox>& RightColumnVerticalBox);

	static void AddReplaceCheckbox(const TWeakObjectPtr<UHoudiniCookable>& MainHC, 
		const TArray<TWeakObjectPtr<UHoudiniCookable>>& InHCs, 
		TSharedPtr<SVerticalBox>& RightColumnVerticalBox);

	static void AddBakeFolderSelector(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder, 
		const TWeakObjectPtr<UHoudiniCookable>& MainHC, 
		const TArray<TWeakObjectPtr<UHoudiniCookable>>& InHCs);

	static void AddBakeControlBar(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder, 
		const TWeakObjectPtr<UHoudiniCookable>& MainHC, 
		const TArray<TWeakObjectPtr<UHoudiniCookable>>& InHCs, 
		EHoudiniDetailsFlags DetailsFlags);

	static FReply ShowCookLog(const TArray<HAPI_NodeId>& InNodeIds);

	static FReply ShowAssetHelp(HAPI_NodeId InNodeId);

	static FMenuBuilder Helper_CreateHoudiniAssetPicker();

	const FSlateBrush * GetHoudiniAssetThumbnailBorder(TSharedPtr<SBorder> HoudiniAssetThumbnailBorder) const;

	/** Construct drop down menu content for Houdini asset. **/
	//static TSharedRef< SWidget > OnGetHoudiniAssetMenuContent(TArray<UHoudiniAssetComponent*> InHACs);

	static TSharedPtr<SWidget> ConstructActionMenu(
		const TArray<TWeakObjectPtr<UHoudiniCookable>>& InCookables,
		class IDetailLayoutBuilder*);

	static void AddHeaderRowForCookable(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
		const TWeakObjectPtr<UHoudiniCookable>& HoudiniCookable,
		int32 MenuSection);

	static void AddHeaderRowForHoudiniPDGAssetLink(
		IDetailCategoryBuilder& PDGCategoryBuilder,
		const TWeakObjectPtr<UHoudiniPDGAssetLink>& InPDGAssetLink,
		int32 MenuSection);

	static void AddHeaderRow(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder, 
		FOnClicked& InOnExpanderClick,
		TFunction<FText(void)>& InGetText,
		TFunction<const FSlateBrush*(SButton* InExpanderArrow)>& InGetExpanderBrush);

	// Adds a text row that indicate the status of the Houdini Session
	static void AddSessionStatusRow(IDetailCategoryBuilder& InCategory);

	static bool GetSessionStatusAndColor(FString& OutStatusString, FLinearColor& OutStatusColor);

	// Adds a text row indicate we're using a Houdini indie license
	static void AddIndieLicenseRow(IDetailCategoryBuilder& InCategory);

	// Adds a text row indicate we're using a Houdini Edu license
	static void AddEducationLicenseRow(IDetailCategoryBuilder& InCategory);

	// Helper to check if InWeakObjectPointer is valid or not. If not valid, the filepath and line number where the check
	// occurred is logged.
	template <class T>
	static bool IsValidWeakObjectPointer(
		const TWeakObjectPtr<T>& InWeakObjectPointer,
		const bool bInLogInvalid=false,
		const FString& InFilePath=FString(),
		const int32 InLineNumber=INDEX_NONE);

private:

	// Helper function that can be used to set either the bake/tempcook folder path on cookables
	static void SetFolderPath(
		const FText& InPathText,
		const bool& bIsBakePath,
		const TWeakObjectPtr<UHoudiniCookable>& InMainHC,
		const TArray<TWeakObjectPtr<UHoudiniCookable>>& InHCs);


	static void CreateResetParametersButton(const TArray<TWeakObjectPtr<UHoudiniCookable>>& InHCs, TSharedRef<SHorizontalBox> ButtonHorizontalBox);

};


template <class T>
bool
FHoudiniEngineDetails::IsValidWeakObjectPointer(const TWeakObjectPtr<T>& InWeakObjectPointer, const bool bInLogInvalid, const FString& InFilePath, const int32 InLineNumber)
{
	const bool bIsValid = InWeakObjectPointer.IsValid();
	if (!bIsValid && bInLogInvalid)
		HOUDINI_LOG_WARNING(TEXT("Invalid TWeakObjectPtr in Details: %s:%d"), *InFilePath, InLineNumber);
	return bIsValid;
}
