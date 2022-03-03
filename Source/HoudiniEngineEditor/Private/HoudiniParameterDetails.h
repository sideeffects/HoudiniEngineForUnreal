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

#include "Widgets/Layout/SUniformGridPanel.h"
#include "SCurveEditor.h"
#include "Editor/CurveEditor/Public/CurveEditorSettings.h"
#include "HoudiniParameterTranslator.h"
#include "Curves/CurveFloat.h"
#include "SColorGradientEditor.h"
#include "Curves/CurveLinearColor.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"

#include "HoudiniParameterDetails.generated.h"

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

class UHoudiniParameterRampColorPoint;
class UHoudiniParameterRampFloatPoint;

class UHoudiniColorRampCurve;
class UHoudiniFloatRampCurve;

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

class SHoudiniFloatRampCurveEditor : public SCurveEditor
{
public:
	SLATE_BEGIN_ARGS(SHoudiniFloatRampCurveEditor)
		: _ViewMinInput(0.0f)
		, _ViewMaxInput(10.0f)
		, _ViewMinOutput(0.0f)
		, _ViewMaxOutput(1.0f)
		, _InputSnap(0.1f)
		, _OutputSnap(0.05f)
		, _InputSnappingEnabled(false)
		, _OutputSnappingEnabled(false)
		, _ShowTimeInFrames(false)
		, _TimelineLength(5.0f)
		, _DesiredSize(FVector2D::ZeroVector)
		, _DrawCurve(true)
		, _HideUI(true)
		, _AllowZoomOutput(true)
		, _AlwaysDisplayColorCurves(false)
		, _ZoomToFitVertical(true)
		, _ZoomToFitHorizontal(true)
		, _ShowZoomButtons(true)
		, _XAxisName()
		, _YAxisName()
		, _ShowInputGridNumbers(true)
		, _ShowOutputGridNumbers(true)
		, _ShowCurveSelector(true)
		, _GridColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.3f))
	{
		_Clipping = EWidgetClipping::ClipToBounds;
	}

		SLATE_ATTRIBUTE(float, ViewMinInput)
		SLATE_ATTRIBUTE(float, ViewMaxInput)
		SLATE_ATTRIBUTE(TOptional<float>, DataMinInput)
		SLATE_ATTRIBUTE(TOptional<float>, DataMaxInput)
		SLATE_ATTRIBUTE(float, ViewMinOutput)
		SLATE_ATTRIBUTE(float, ViewMaxOutput)
		SLATE_ATTRIBUTE(float, InputSnap)
		SLATE_ATTRIBUTE(float, OutputSnap)
		SLATE_ATTRIBUTE(bool, InputSnappingEnabled)
		SLATE_ATTRIBUTE(bool, OutputSnappingEnabled)
		SLATE_ATTRIBUTE(bool, ShowTimeInFrames)
		SLATE_ATTRIBUTE(float, TimelineLength)
		SLATE_ATTRIBUTE(FVector2D, DesiredSize)
		SLATE_ATTRIBUTE(bool, AreCurvesVisible)
		SLATE_ARGUMENT(bool, DrawCurve)
		SLATE_ARGUMENT(bool, HideUI)
		SLATE_ARGUMENT(bool, AllowZoomOutput)
		SLATE_ARGUMENT(bool, AlwaysDisplayColorCurves)
		SLATE_ARGUMENT(bool, ZoomToFitVertical)
		SLATE_ARGUMENT(bool, ZoomToFitHorizontal)
		SLATE_ARGUMENT(bool, ShowZoomButtons)
		SLATE_ARGUMENT(TOptional<FString>, XAxisName)
		SLATE_ARGUMENT(TOptional<FString>, YAxisName)
		SLATE_ARGUMENT(bool, ShowInputGridNumbers)
		SLATE_ARGUMENT(bool, ShowOutputGridNumbers)
		SLATE_ARGUMENT(bool, ShowCurveSelector)
		SLATE_ARGUMENT(FLinearColor, GridColor)
		SLATE_EVENT(FOnSetInputViewRange, OnSetInputViewRange)
		SLATE_EVENT(FOnSetOutputViewRange, OnSetOutputViewRange)
		SLATE_EVENT(FOnSetAreCurvesVisible, OnSetAreCurvesVisible)
		SLATE_EVENT(FSimpleDelegate, OnCreateAsset)
		SLATE_END_ARGS()

	public:
		TWeakObjectPtr<UHoudiniFloatRampCurve> HoudiniFloatRampCurve;

		/** Widget construction. **/
		void Construct(const FArguments & InArgs);

		virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

};


class SHoudiniColorRampCurveEditor : public SColorGradientEditor 
{

public:
	SLATE_BEGIN_ARGS(SHoudiniColorRampCurveEditor)
		: _ViewMinInput(0.0f)
		, _ViewMaxInput(10.0f)
		, _InputSnap(0.1f)
		, _OutputSnap(0.05f)
		, _InputSnappingEnabled(false)
		, _OutputSnappingEnabled(false)
		, _ShowTimeInFrames(false)
		, _TimelineLength(5.0f)
		, _DesiredSize(FVector2D::ZeroVector)
		, _DrawCurve(true)
		, _HideUI(true)
		, _AllowZoomOutput(true)
		, _AlwaysDisplayColorCurves(false)
		, _ZoomToFitVertical(true)
		, _ZoomToFitHorizontal(true)
		, _ShowZoomButtons(true)
		, _XAxisName()
		, _YAxisName()
		, _ShowInputGridNumbers(true)
		, _ShowOutputGridNumbers(true)
		, _ShowCurveSelector(true)
		, _GridColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.3f))
	{
		_Clipping = EWidgetClipping::ClipToBounds;
	}

	SLATE_ATTRIBUTE(float, ViewMinInput)
		SLATE_ATTRIBUTE(float, ViewMaxInput)
		SLATE_ATTRIBUTE(TOptional<float>, DataMinInput)
		SLATE_ATTRIBUTE(TOptional<float>, DataMaxInput)
		SLATE_ATTRIBUTE(float, InputSnap)
		SLATE_ATTRIBUTE(float, OutputSnap)
		SLATE_ATTRIBUTE(bool, InputSnappingEnabled)
		SLATE_ATTRIBUTE(bool, OutputSnappingEnabled)
		SLATE_ATTRIBUTE(bool, ShowTimeInFrames)
		SLATE_ATTRIBUTE(float, TimelineLength)
		SLATE_ATTRIBUTE(FVector2D, DesiredSize)
		SLATE_ATTRIBUTE(bool, AreCurvesVisible)
		SLATE_ARGUMENT(bool, DrawCurve)
		SLATE_ARGUMENT(bool, HideUI)
		SLATE_ARGUMENT(bool, AllowZoomOutput)
		SLATE_ARGUMENT(bool, AlwaysDisplayColorCurves)
		SLATE_ARGUMENT(bool, ZoomToFitVertical)
		SLATE_ARGUMENT(bool, ZoomToFitHorizontal)
		SLATE_ARGUMENT(bool, ShowZoomButtons)
		SLATE_ARGUMENT(TOptional<FString>, XAxisName)
		SLATE_ARGUMENT(TOptional<FString>, YAxisName)
		SLATE_ARGUMENT(bool, ShowInputGridNumbers)
		SLATE_ARGUMENT(bool, ShowOutputGridNumbers)
		SLATE_ARGUMENT(bool, ShowCurveSelector)
		SLATE_ARGUMENT(FLinearColor, GridColor)
		SLATE_EVENT(FOnSetInputViewRange, OnSetInputViewRange)
		SLATE_EVENT(FOnSetOutputViewRange, OnSetOutputViewRange)
		SLATE_EVENT(FOnSetAreCurvesVisible, OnSetAreCurvesVisible)
		SLATE_EVENT(FSimpleDelegate, OnCreateAsset)
		SLATE_END_ARGS()

	public:
		/** Widget construction. **/
		void Construct(const FArguments & InArgs);

		TWeakObjectPtr<UHoudiniColorRampCurve> HoudiniColorRampCurve;

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

		virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
};

UCLASS()
class UHoudiniFloatRampCurve : public UCurveFloat 
{
	GENERATED_BODY()

	public:

		TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>> FloatRampParameters;

		virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) override;
};


UCLASS()
class UHoudiniColorRampCurve : public UCurveLinearColor
{
	GENERATED_BODY()

	public:
		bool bEditing = false;

		TArray<TWeakObjectPtr<UHoudiniParameterRampColor>> ColorRampParameters;

		virtual void OnCurveChanged(const TArray< FRichCurveEditInfo > & ChangedCurveEditInfos) override;

		void OnColorRampCurveChanged(bool bModificationOnly = false);

};


//class FHoudiniParameterDetails : public TSharedFromThis<FHoudiniParameterDetails>, public TNumericUnitTypeInterface<float>, public TNumericUnitTypeInterface<int32>
class FHoudiniParameterDetails : public TSharedFromThis<FHoudiniParameterDetails, ESPMode::NotThreadSafe>
{
	public:
		void CreateWidget(
			IDetailCategoryBuilder & HouParameterCategory,
			const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams);

		void CreateWidgetInt(
			IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);
		void CreateWidgetFloat(
			IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);
		void CreateWidgetString(
			IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);
		void CreateWidgetColor(
			IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);
		void CreateWidgetButton(
			IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);
		void CreateWidgetButtonStrip(
			IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);
		void CreateWidgetLabel(
			IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams);
		void CreateWidgetToggle(
			IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams);
		void CreateWidgetFile(
			IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams);
		void CreateWidgetChoice(
			IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);
		void CreateWidgetSeparator(
			IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams, const bool& InIsEnabled);
		void CreateWidgetFolderList(
			IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams);
		void CreateWidgetFolder(
			IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams);
		void CreateWidgetMultiParm(
			IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams);
		void CreateWidgetOperatorPath(
			IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams);
		void CreateWidgetFloatRamp(
			IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams);
		void CreateWidgetColorRamp(
			IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams);

		void CreateTabEndingRow(IDetailCategoryBuilder & HouParameterCategory);
		

		void HandleUnsupportedParmType(
			IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams
		);


		static FText GetParameterTooltip(const TWeakObjectPtr<UHoudiniParameter>& InParam);

		static FString GetParameterTypeString(const EHoudiniParameterType& InType, const int32& InTupleSize);

		static void SyncCachedColorRampPoints(UHoudiniParameterRampColor* ColorRampParameter);

		//static void SyncCachedFloatRampPoints(UHoudiniParameterRampFloat* FloatRampParameter);

		// replace the children parameter values of all (multi-selected) float ramp parameters with the main parameter (weak object pointer version)
		static void ReplaceAllFloatRampParameterPointsWithMainParameter(const TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>>& FloatRampParameters);
		// raw pointer version
		static void ReplaceAllFloatRampParameterPointsWithMainParameter(const TArray<UHoudiniParameterRampFloat*>& FloatRampParameters);
		// helper
		static void ReplaceFloatRampParameterPointsWithMainParameter(UHoudiniParameterRampFloat* Param, UHoudiniParameterRampFloat* MainParam);


		// replace the children parameter values of all (multi-selected) color ramp parameters with the main parameter (weak object pointer version)
		static void ReplaceAllColorRampParameterPointsWithMainParameter(const TArray<TWeakObjectPtr<UHoudiniParameterRampColor>>& ColorRampParameters);
		// raw pointer version
		static void ReplaceAllColorRampParameterPointsWithMainParameter(const TArray<UHoudiniParameterRampColor*>& ColorRampParameters);
		// helper
		static void ReplaceColorRampParameterPointsWithMainParameter(UHoudiniParameterRampColor* Param, UHoudiniParameterRampColor* MainParame);



		// Create an insert event for a float ramp parameter
		static void CreateFloatRampParameterInsertEvent(UHoudiniParameterRampFloat* InParam, 
			const float& InPosition, const float& InValue, const EHoudiniRampInterpolationType &InInterp);

		// Create an insert event for a color ramp parameter
		static void CreateColorRampParameterInsertEvent(UHoudiniParameterRampColor* InParam, 
			const float& InPosition, const FLinearColor& InColor, const EHoudiniRampInterpolationType &InInterp);

		// Create a delete event for a float ramp parameter
		static void CreateFloatRampParameterDeleteEvent(UHoudiniParameterRampFloat* InParam, const int32 &InDeleteIndex);

		// Create a delete event for a color ramp parameter
		static void CreateColorRampParameterDeleteEvent(UHoudiniParameterRampColor* InParam, const int32 &InDeleteIndex);


	private:

		template< class T >
		static bool CastParameters(
			const TArray<UHoudiniParameter*>& InParams, TArray<T*>& OutCastedParams);

		template< class T >
		static bool CastParameters(
			const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams, TArray<TWeakObjectPtr<T>>& OutCastedParams);

		//
		// Private helper functions for widget creation
		//

		// Creates the default name widget, the parameter will then fill the value after
		void CreateNameWidget(FDetailWidgetRow* Row, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams, bool WithLabel);

		// Creates the default name widget, with an extra checkbox for disabling the the parameter update
		void CreateNameWidgetWithAutoUpdate(FDetailWidgetRow* Row, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams, bool WithLabel);

		FDetailWidgetRow* CreateNestedRow(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams, bool bDecreaseChildCount = true); //

		void CreateFolderHeaderUI(FDetailWidgetRow* HeaderRow, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams); //

		void CreateWidgetTab(IDetailCategoryBuilder & HouParameterCategory, const TWeakObjectPtr<UHoudiniParameterFolder>& InParam, const bool& bIsShown);  //

		void CreateWidgetMultiParmObjectButtons(TSharedPtr<SHorizontalBox> HorizontalBox, const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams); //
	
		// Create the UI for ramp's curve editor.
		FDetailWidgetRow* CreateWidgetRampCurveEditor(IDetailCategoryBuilder & HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniParameter>> &InParams); //

		// Create the UI for ramp's stop points.
		void CreateWidgetRampPoints(IDetailCategoryBuilder& CategoryBuilder, FDetailWidgetRow* Row, UHoudiniParameter* InParameter,
								    const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams); //

		void PruneStack();

		void RemoveTabDividers(IDetailCategoryBuilder& HouParameterCategory, const TWeakObjectPtr<UHoudiniParameter>& InParam);

	public:
		// Stores the created ramp curves
		// In order to avoid being grabage collected, curves are added to root, thus need to handle GC manually.
		// These points are for releasing the memory when the detail class are destroyed
		TArray<UHoudiniFloatRampCurve*> CreatedFloatRampCurves;
		TArray<UHoudiniColorRampCurve*> CreatedColorRampCurves;
		// The curve editors reference the UHoudini*Curves as "CurveOwners" as raw (non UObject) pointers, so we have
		// to set their owners to null here before we destroy the Created*RampCuvers
		TArray<TSharedPtr<SHoudiniColorRampCurveEditor>> CreatedColorGradientEditors;
		TArray<TSharedPtr<SHoudiniFloatRampCurveEditor>> CreatedFloatCurveEditors;

	private:
		// The parameter directory is flattened with BFS inside of DFS.
		// When a folderlist is encountered, it goes 'one step' of DFS, otherwise BFS.
		// So that use a Stack<Queue> structure to reconstruct the tree.
		TArray<TArray<UHoudiniParameterFolder*>> FolderStack;

		// Float Ramp currently being processed
		UHoudiniParameterRampFloat* CurrentRampFloat;

		// Color Ramp currently being processed
		UHoudiniParameterRampColor* CurrentRampColor;

		TArray<UHoudiniParameter*> CurrentRampParameterList;

		// Cached curve points of float ramp which being processed
		TArray<UHoudiniParameterRampFloatPoint*> CurrentRampFloatPointsArray;

		// Cached curve points of color ramp which being processed
		TArray<UHoudiniParameterRampColorPoint*> CurrentRampColorPointsArray;

		// Cached color ramp curve which being processed
		UHoudiniColorRampCurve* CurrentRampParameterColorCurve;

		// Cached float ramp curve which being processed
		UHoudiniFloatRampCurve* CurrentRampParameterFloatCurve;

		FDetailWidgetRow * CurrentRampRow;


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
