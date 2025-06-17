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

#include "HoudiniAssetComponent.h"

#include "CoreMinimal.h"

#include "HoudiniParameterTranslator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"

class UHoudiniAssetComponent;
class UHoudiniParameter;
class UHoudiniParameterFloat; 
class UHoudiniParameterInt;
class UHoudiniParameterString;
class UHoudiniParameterColor;
class UHoudiniParameterButton;
class UHoudiniParameterButtonStrip;
class UHoudiniParameterLabel;
class UHoudiniParameterToggle;
class UHoudiniParameterFile;
class UHoudiniParameterChoice;
class UHoudiniParameterFolder;
class UHoudiniParameterFolderList;
class UHoudiniParameterMultiParm;
class UHoudiniParameterRampFloat;
class UHoudiniParameterRampColor;
class UHoudiniParameterOperatorPath;

class IDetailCategoryBuilder;
class FDetailWidgetRow;
class SHorizontalBox;
class SHoudiniAssetParameterRampCurveEditor;

enum class EHoudiniRampInterpolationType : int8;

class SCustomizedButton : public SButton 
{
public:
	bool bChosen;

	bool bIsRadioButton;

public:
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Construct the circles for all radio buttons. Initialize at first use
	void ConstructRadioButtonCircles() const;

	void DrawRadioButton(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const bool& bSelected) const;
};

class SCustomizedBox : public SHorizontalBox
{
public:
	bool bIsTabFolderListRow;

	bool bIsSeparator;

	TArray<float> DividerLinePositions;

	TArray<float> EndingDividerLinePositions;

	float MarginHeight;

public:
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Add indentation to current row, computed by tracing the directory hierarchy,
	// return the indentation width of this parameter row.
	float AddIndentation(const TWeakObjectPtr<UHoudiniParameter>& InParam, const TMap<int32, TWeakObjectPtr<UHoudiniParameterMultiParm>>& InAllMultiParms, const TMap<int32, TWeakObjectPtr<UHoudiniParameter>>& InAllFoldersAndFolderLists);

	void SetHoudiniParameter(const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);
};

/**
 * Widget used to wrap parameter controls, optionally with content attached to the name slot.
 * 
 * This is used to support displaying a label next to a parameter widget when horizontally joining.
 * We use it because using the name content slot provided by Unreal only allows us to place one name
 * widget and one content widget in the details view.
 */
class SHoudiniLabelledParameter : public SHorizontalBox
{
public:

	SLATE_BEGIN_ARGS(SHoudiniLabelledParameter)
		: _Content()
		{}

		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_NAMED_SLOT(FArguments, NameContent)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetContent(TSharedRef<SWidget> InContent);
	void SetNameContent(TSharedRef<SWidget> InNameContent);

private:
	// Controls the padding on the content slot.
	// We only want padding if there is a non-null widget is attached to the Name slot.
	bool bEnableContentPadding;

	// Current padding used on the content slot.
	TAttribute<FMargin> ContentPadding;
};

//class FHoudiniParameterDetails : public TSharedFromThis<FHoudiniParameterDetails>, public TNumericUnitTypeInterface<float>, public TNumericUnitTypeInterface<int32>
class FHoudiniParameterDetails : public TSharedFromThis<FHoudiniParameterDetails, ESPMode::NotThreadSafe>
{
	public:
		/**
		 * @param InJoinedParams Array of horizontally joined parameters, where each element is an
		 *                       array of linked parameters. Not all widgets support being joined
		 *                       horizontally. Use @ref ShouldJoinNext to determine if a widget can
		 *                       be joined.
		 */
		void CreateWidget(
			IDetailCategoryBuilder & HouParameterCategory,
			const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams);

		void CreateJoinableWidget(
			IDetailCategoryBuilder& HouParameterCategory,
			const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams,
			TArray<FDetailWidgetRow*>& OutRows);

		void CreateWidgetInt(
			const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
			const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);

		void CreateWidgetFloat(
			const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
			const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);

		void CreateWidgetString(
			IDetailCategoryBuilder& HouParameterCategory,
			const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
			const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);

		void CreateWidgetColor(
			const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
			const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);

		void CreateWidgetButton(
			const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
			const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);

		void CreateWidgetButtonStrip(
			const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
			const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);

		void CreateWidgetLabel(
			const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
			const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);

		void CreateWidgetToggle(
			const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
			const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);

		void CreateWidgetFile(
			const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
			const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);

		void CreateWidgetChoice(
			const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
			const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);

		void CreateWidgetSeparator(
			const TSharedRef<SHoudiniLabelledParameter> LabelledParameter,
			const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);

		void CreateWidgetFolderList(
			IDetailCategoryBuilder& HouParameterCategory,
			const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams,
			TArray<FDetailWidgetRow*>& OutRows);

		void CreateWidgetFolder(
			IDetailCategoryBuilder& HouParameterCategory,
			const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams,
			TArray<FDetailWidgetRow*>& OutRows);

		void CreateWidgetMultiParm(
			IDetailCategoryBuilder& HouParameterCategory,
			const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams,
			TArray<FDetailWidgetRow*>& OutRows);

		void CreateWidgetOperatorPath(
			IDetailCategoryBuilder& HouParameterCategory,
			const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams,
			TArray<FDetailWidgetRow*>& OutRows);

		void CreateWidgetFloatRamp(
			IDetailCategoryBuilder& HouParameterCategory,
			const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams,
			TArray<FDetailWidgetRow*>& OutRows);

		void CreateWidgetColorRamp(
			IDetailCategoryBuilder& HouParameterCategory,
			const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams,
			TArray<FDetailWidgetRow*>& OutRows);

		void CreateTabEndingRow(IDetailCategoryBuilder & HouParameterCategory);
		

		void HandleUnsupportedParmType(
			IDetailCategoryBuilder & HouParameterCategory,
			const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams);

		static FText GetParameterTooltip(const TWeakObjectPtr<UHoudiniParameter>& InParam);

		static FString GetParameterTypeString(const EHoudiniParameterType& InType, const int32& InTupleSize);

		/** Determines if @ref CreateWidget expects this parameter to be joined. */
		static bool ShouldJoinNext(const UHoudiniParameter& InParam);

	private:

		void Debug();

		template< class T >
		static bool CastParameters(
			const TArray<UHoudiniParameter*>& InParams, TArray<T*>& OutCastedParams);

		template< class T >
		static bool CastParameters(
			const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams, TArray<TWeakObjectPtr<T>>& OutCastedParams);

		//
		// Private helper functions for widget creation
		//


		TSharedPtr<SCustomizedBox> CreateCustomizedBox(
			const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);

		TSharedPtr<STextBlock> CreateNameTextBlock(
			const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);

		// Creates the default name widget, the parameter will then fill the value after
		void CreateNameWidget(FDetailWidgetRow* Row, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams, bool WithLabel);

		// Creates the default name widget, with an extra checkbox for disabling the the parameter update
		void CreateNameWidgetWithAutoUpdate(FDetailWidgetRow* Row, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams, bool WithLabel);

		// Needs to be called for all parameters, not just when we need a row.
		// This is because we adjust folder stack here.
		// In the future, folder structure should really be seperated from details customization.
		FDetailWidgetRow* CreateNestedRow(
			IDetailCategoryBuilder& HouParameterCategory,
			const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams,
			bool bDecreaseChildCount = true);

		void CreateFolderHeaderUI(IDetailCategoryBuilder& HouParameterCategory, FDetailWidgetRow* HeaderRow, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams); //

		void CreateWidgetTab(
			IDetailCategoryBuilder& HouParameterCategory,
			const TWeakObjectPtr<UHoudiniParameterFolder>& InParam,
			const bool bIsShown,
			TArray<FDetailWidgetRow*>& OutRows);
		void CreateWidgetTabUIElements(
			IDetailCategoryBuilder& HouParameterCategory,
			const TWeakObjectPtr<UHoudiniParameterFolder>& InParam,
			TArray<FDetailWidgetRow*>& OutRows);

		void CreateWidgetMultiParmObjectButtons(TSharedPtr<SHorizontalBox> HorizontalBox, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams); //
	
		FDetailWidgetRow* CreateWidgetRamp(
			IDetailCategoryBuilder& HouParameterCategory,
			const TArray<TArray<TWeakObjectPtr<UHoudiniParameter>>>& InJoinedParams);

		void PruneStack();

		void RemoveTabDividers(IDetailCategoryBuilder& HouParameterCategory, const TWeakObjectPtr<UHoudiniParameter>& InParam);

		static bool IsLabelVisible(const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);
		static bool UsesWholeRow(const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);

		/** 
		 * In Houdini, some widgets try to occupy all available space on a row (i.e. separators)
		 * while other types (i.e. toggles) use only the minimum space they require.
		 * @returns true if the widget should occupy all available space, false otherwise.
		 */
		static bool ShouldWidgetFill(EHoudiniParameterType ParameterType);

		static void AddMetaDataToAllDescendants(
			const TSharedRef<SWidget> AncestorWidget,
			const FString& UniqueName, 
			uint32& Index);

	private:
		// The parameter directory is flattened with BFS inside of DFS.
		// When a folderlist is encountered, it goes 'one step' of DFS, otherwise BFS.
		// So that use a Stack<Queue> structure to reconstruct the tree.
		TArray<TArray<UHoudiniParameterFolder*>> FolderStack;

		// Float Ramp currently being processed
		UHoudiniParameterRampFloat* CurrentRampFloat;

		// Color Ramp currently being processed
		UHoudiniParameterRampColor* CurrentRampColor;

		/* Variables for keeping expansion state after adding multiparm instance*/
		TMap<int32, TWeakObjectPtr<UHoudiniParameterMultiParm>> AllMultiParms;

		// Cached the map of parameter id and folders/folder lists 
		TMap<int32, TWeakObjectPtr<UHoudiniParameter>> AllFoldersAndFolderLists;

		/* Variables for keeping expansion state after adding multiparm instance*/

		TMap<int32, int32> MultiParmInstanceIndices;

		// Number of remaining folders for current folder list
		int32 CurrentFolderListSize = 0;

		// The folder list currently being processed
		UHoudiniParameterFolderList* CurrentFolderList;

		// Cached child folders of current tabs
		TArray<UHoudiniParameterFolder*> CurrentTabs;

		TArray<float> DividerLinePositions;

		SCustomizedBox* CurrentTabEndingRow;

};
