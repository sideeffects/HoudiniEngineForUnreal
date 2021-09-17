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

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/SlateDelegates.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Layout/SBorder.h"
#include "Framework/SlateDelegates.h"
#include "Widgets/Input/SButton.h"

class IDetailCategoryBuilder;
class UHoudiniAssetComponent;
class UHoudiniPDGAssetLink;
class FMenuBuilder;
class SBorder;
class SButton;

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

class FHoudiniEngineDetails : public TSharedFromThis<FHoudiniEngineDetails, ESPMode::NotThreadSafe>
{
public:
	static void CreateWidget(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
		TArray<UHoudiniAssetComponent*>& InHACs);

	static void CreateHoudiniEngineIconWidget(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
		TArray<UHoudiniAssetComponent*>& InHACs);

	static void CreateGenerateWidgets(
			IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
			TArray<UHoudiniAssetComponent*>& InHACs);

	static void CreateBakeWidgets(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
		TArray<UHoudiniAssetComponent*>& InHACs);

	static void CreatePDGBakeWidgets(
		IDetailCategoryBuilder& InPDGCategory,
		UHoudiniPDGAssetLink* InPDGAssetLink); 

	static void CreateAssetOptionsWidgets(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
		TArray<UHoudiniAssetComponent*>& InHACs);

	static void CreateHelpAndDebugWidgets(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
		TArray<UHoudiniAssetComponent*>& InHACs);

	static FReply ShowCookLog(TArray<UHoudiniAssetComponent *> InHACS);

	static FReply ShowAssetHelp(UHoudiniAssetComponent * InHAC);

	static FMenuBuilder Helper_CreateHoudiniAssetPicker();

	const FSlateBrush * GetHoudiniAssetThumbnailBorder(TSharedPtr< SBorder > HoudiniAssetThumbnailBorder) const;

	/** Construct drop down menu content for Houdini asset. **/
	//static TSharedRef< SWidget > OnGetHoudiniAssetMenuContent(TArray<UHoudiniAssetComponent*> InHACs);

	static void AddHeaderRowForHoudiniAssetComponent(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
		UHoudiniAssetComponent* HoudiniAssetComponent,
		int32 MenuSection);

	static void AddHeaderRowForHoudiniPDGAssetLink(
		IDetailCategoryBuilder& PDGCategoryBuilder,
		UHoudiniPDGAssetLink* InPDGAssetLink,
		int32 MenuSection);

	static void AddHeaderRow(
		IDetailCategoryBuilder& HoudiniEngineCategoryBuilder, 
		FOnClicked& InOnExpanderClick,
		TFunction<FText(void)>& InGetText,
		TFunction<const FSlateBrush*(SButton* InExpanderArrow)>& InGetExpanderBrush);

	// Helper for binding/unbinding the post cook bake delegate
	static void OnBakeAfterCookChangedHelper(bool bInState, UHoudiniAssetComponent* InHAC);
};

