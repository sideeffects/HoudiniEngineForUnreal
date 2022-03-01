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

#include "Widgets/SBoxPanel.h"
#include "IDetailsView.h"

class UHoudiniInput;
class UHoudiniSplineComponent;

class IDetailCategoryBuilder;
class FDetailWidgetRow;
class FMenuBuilder;
class SVerticalBox;
class IDetailsView;
class FReply;
class FAssetThumbnailPool;

class FHoudiniInputDetails : public TSharedFromThis<FHoudiniInputDetails, ESPMode::NotThreadSafe>
{
	public:
		static void CreateWidget(
			IDetailCategoryBuilder& HouInputCategoryBuilder,
			const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs, FDetailWidgetRow* InputRow = nullptr);

		static void CreateNameWidget(
			const TWeakObjectPtr<UHoudiniInput>& InParam,
			FDetailWidgetRow & Row,
			bool bLabel,
			int32 InInputCount);

		static FText GetInputTooltip( const TWeakObjectPtr<UHoudiniInput>& InInput );

		// ComboBox :  Input Type
		static void AddInputTypeComboBox(
			IDetailCategoryBuilder& CategoryBuilder,
			TSharedRef<SVerticalBox> InVerticalBox,
			const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
			const IDetailsView* InDetailsView);

		// Checkbox : Keep World Transform
		static void AddKeepWorldTransformCheckBox(
			TSharedRef<SVerticalBox> InVerticalBox,
			const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs);

		static void AddCurveInputCookOnChangeCheckBox(
				TSharedRef< SVerticalBox > VerticalBox,
				const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs);

		// Checkbox : Pack before merging
		static void AddPackBeforeMergeCheckbox(
			TSharedRef<SVerticalBox> InVerticalBox,
			const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs);

		static void AddImportAsReferenceCheckbox(
			TSharedRef< SVerticalBox > VerticalBox,
			const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs);

		// Checkboxes : Export LODs / Sockets / Collisions
		static void AddExportCheckboxes(
			TSharedRef<SVerticalBox> InVerticalBox,
			const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs);

		// Add Geometry Inputs UI Widgets
		static void AddGeometryInputUI(
			IDetailCategoryBuilder& CategoryBuilder,
			TSharedRef<SVerticalBox> InVerticalBox,
			const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs, 
			TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool);

		// Create a single geometry widget for the given input object
		static void Helper_CreateGeometryWidget(
			IDetailCategoryBuilder& CategoryBuilder,
			const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
			const FPlatformTypes::int32& InGeometryObjectIdx,
			TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool, TSharedRef<SVerticalBox> VerticalBox);

		static void Helper_CreateGeometryCollectionWidget(
			IDetailCategoryBuilder& CategoryBuilder,
			const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
			const FPlatformTypes::int32& InGeometryCollectionObjectIdx,
			TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool, TSharedRef<SVerticalBox> VerticalBox);

		static void Helper_CreateCurveWidget(
			IDetailCategoryBuilder& CategoryBuilder,
			const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
			const int32& InCurveObjectIdx,
			TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool,
			TSharedRef< SVerticalBox > VerticalBox,
			TSharedPtr<class FHoudiniSplineComponentVisualizer> HouSplineComponentVisualizer);

		// Add Asset Inputs UI Widgets
		static void AddAssetInputUI(
			TSharedRef<SVerticalBox> VerticalBox,
			const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs);

		// Add Curve Inputs UI Widgets
		static void AddCurveInputUI(
			IDetailCategoryBuilder& CategoryBuilder,
			TSharedRef<SVerticalBox> VerticalBox,
			const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
			TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool);

		// Add Landscape Inputs UI Widgets
		static void AddLandscapeInputUI(
			TSharedRef<SVerticalBox> VerticalBox,
			const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs);

		// Add World Inputs UI Widgets
		static void AddWorldInputUI(
			IDetailCategoryBuilder& CategoryBuilder,
			TSharedRef<SVerticalBox> VerticalBox,
			const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
			const IDetailsView* InDetailsView);

		// Add Skeletal Inputs UI Widgets
		static void AddSkeletalInputUI(
			TSharedRef<SVerticalBox> VerticalBox,
			const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
			TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool);

		// Add GeometryCollection Inputs UI Widgets
		static void AddGeometryCollectionInputUI(
			IDetailCategoryBuilder& CategoryBuilder,
			TSharedRef<SVerticalBox> VerticalBox,
			const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
			TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool);
	
		/*
		static FMenuBuilder Helper_CreateCustomActorPickerWidget(
			UHoudiniInput* InParam,
			const TAttribute<FText>& HeadingText,
			const bool& bShowCurrentSelectionSection)
			*/

		static FMenuBuilder Helper_CreateHoudiniAssetPickerWidget(const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs);

		static FMenuBuilder Helper_CreateLandscapePickerWidget(const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs);

		static FMenuBuilder Helper_CreateWorldActorPickerWidget(const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs);

		static FMenuBuilder Helper_CreateBoundSelectorPickerWidget(const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs);

		static FReply Helper_OnButtonClickSelectActors(IDetailCategoryBuilder& CategoryBuilder, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs, const FName& DetailsPanelName);

		static FReply Helper_OnButtonClickUseSelectionAsBoundSelector(IDetailCategoryBuilder& CategoryBuilder, const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs, const FName& DetailsPanelName);

		static FReply Helper_OnButtonClickSelectActors(
			IDetailCategoryBuilder& CategoryBuilder,
			const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs,
			const FName& InDetailsPanelName,
			const bool& bUseWorldInAsWorldSelector);

		static bool Helper_CancelWorldSelection(
			const TArray<TWeakObjectPtr<UHoudiniInput>>& InInputs, const FName& DetailsPanelName);
};