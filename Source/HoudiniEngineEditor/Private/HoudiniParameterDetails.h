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

#include "DetailLayoutBuilder.h"
#include "UObject/Object.h"
#include "HoudiniParameter.h"
#include "HoudiniParameterButtonStrip.h"
#include "HoudiniParameterFile.h"
#include "HoudiniParameterInt.h"
#include "HoudiniParameterOperatorPath.h"
#include "HoudiniParameterRamp.h"
#include "HoudiniParameterString.h"
#include "HoudiniParameterToggle.h"
#include <HoudiniParameterButton.h>
#include "HoudiniParameterLabel.h"
#include "HoudiniParameterFolder.h"
#include "Widgets/Layout/SSplitter.h"
#include "HoudiniParameterDetails.generated.h"

struct FHoudiniParameterView;
extern float HoudiniIndentColorScale;

struct FParameterLayout
{
	int IndentLevel = 0;
	float ColorScale = 0.0f;
};

USTRUCT()
struct HOUDINIENGINEEDITOR_API FHoudiniParameterView
{
	// This class is used to wrap each UHoudiniParameter to create Slate Widgets. Its arranged hierarchically
	// so that a parent contains its children.
	GENERATED_BODY()

public:
	FHoudiniParameterView();

	void CreateDetails(IDetailCategoryBuilder& HouParameterCategory, IDetailLayoutBuilder& DetailBuilder);

	static TSharedRef<SWidget> CreateNameWidget(const UHoudiniParameter* Parameter);

	void SetIndent(int Level);
	void PrintOut(int Spacing);
	UHoudiniParameter* GetMainParameter() const;
	EHoudiniParameterType GetParameterType() const;
	EHoudiniFolderParameterType GetFolderType() const;
	const FString& GetParameterLabel() const;

	TArray<TSharedPtr<FHoudiniParameterView>> Children;
	TSharedPtr<FHoudiniParameterView> Parent;
	TArray<TWeakObjectPtr<UHoudiniParameter>> LinkedParameters;
	FString Name;
	TSharedPtr<FParameterLayout> LayoutData;
	TSharedPtr<FHoudiniParameterView> NextJoined;
	bool bIsJoinedToPrevious = false;
	bool bShowMultiParms = false;
	TWeakObjectPtr<UHoudiniCookable> Cookable;
	TSharedPtr<SSplitter> Splitter;

private:
	void SetDividerExpansion(float Value) const;
	float GetDividerExpansion() const;

	FString GetDividerLayoutKey() const;

	static FString LayoutSection;

	void CreateJoinedDetails(IDetailCategoryBuilder& HouParameterCategory, IDetailLayoutBuilder& DetailBuilder);

	void CreateSimpleFolderRow(
		IDetailCategoryBuilder& HouParameterCategory,
		IDetailLayoutBuilder& DetailBuilder,
		const TSharedPtr<SWidget>&  MultiParmAddRemoveButtons);

	void CreateTabbedFolderRow(
		IDetailCategoryBuilder& HouParameterCategory,
		IDetailLayoutBuilder& DetailBuilder,
		TArray<TSharedPtr<FHoudiniParameterView>> FoldersParams,
		const TSharedPtr<SWidget>& MultiParmAddRemoveButtons);

	void CreateRadioFolderRow(
		IDetailCategoryBuilder& HouParameterCategory,
		IDetailLayoutBuilder& DetailBuilder,
		TArray<TSharedPtr<FHoudiniParameterView>> FoldersParams,
		const TSharedPtr<SWidget>& MultiParmAddRemoveButtons);

	void CreateCollapsableFolderRow(
		IDetailCategoryBuilder& HouParameterCategory,
		IDetailLayoutBuilder& DetailBuilder,
		const TSharedPtr<SWidget>& MultiParmAddRemoveButtons);

	template<class DerivedClass>
	DerivedClass* GetTypedMainParameter() { return Cast<DerivedClass>(GetMainParameter()); }

	static TSharedRef<SWidget> CreateWidgetColorRamp(
		const TArray<TWeakObjectPtr<UHoudiniParameterRampColor>>& InJoinedParams,
		const TSharedPtr<SWidget>& ExtraWidgets);

	static TSharedRef<SWidget> CreateWidgetFloatRamp(
		const TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>>& InJoinedParams,
		const TSharedPtr<SWidget>& ExtraWidgets);

	static TSharedRef<SWidget> CreateWidgetOperatorPath(
		IDetailLayoutBuilder& DetailBuilder,
		IDetailCategoryBuilder& HouInputCategory,
		const TArray<TWeakObjectPtr<UHoudiniParameterOperatorPath>>& InJoinedParams,
		const TSharedPtr<SWidget>& ExtraWidgets);

	static TSharedRef<SWidget> CreateWidgetMultiParm(
		IDetailLayoutBuilder& DetailBuilder,
		TArray<TWeakObjectPtr<UHoudiniParameterMultiParm>>& MultiParmParams,
		const TSharedPtr<SWidget>& ExtraWidgets);

	static TSharedRef<SWidget> CreateWidgetChoice(
		const TArray<TWeakObjectPtr<UHoudiniParameterChoice>>& InParams,
		const TSharedPtr<SWidget>& ExtraWidgets);

	static TSharedRef<SWidget> CreateWidgetSeparator(
		const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams,
		const TSharedPtr<SWidget>& ExtraWidgets);

	static TSharedRef<SWidget> CreateWidgetInt(
		const TArray<TWeakObjectPtr<UHoudiniParameterInt>>& IntParams,
		const TSharedPtr<SWidget>& ExtraWidgets);

	static TSharedRef<SWidget> CreateWidgetFloat(
		const TArray<TWeakObjectPtr<UHoudiniParameterFloat>>& InParams,
		const TSharedPtr<SWidget>& ExtraWidgets);

	static TSharedRef<SWidget> CreateWidgetString(
		IDetailCategoryBuilder& HouParameterCategory,
		const TArray<TWeakObjectPtr<UHoudiniParameterString>>& InParams,
		const TSharedPtr<SWidget>& ExtraWidgets);

	static TSharedRef<SWidget> CreateWidgetColor(
		const TArray<TWeakObjectPtr<UHoudiniParameterColor>>& InParams,
		const TSharedPtr<SWidget>& ExtraWidgets);

	static TSharedRef<SWidget> CreateWidgetButton(const TArray<TWeakObjectPtr<UHoudiniParameterButton>>& InParams,
		const TSharedPtr<SWidget>& ExtraWidgets);

	static TSharedRef<SWidget> CreateWidgetFile(
		const TArray<TWeakObjectPtr<UHoudiniParameterFile>>& InParams,
		const TSharedPtr<SWidget>& ExtraWidgets);

	static TSharedRef<SWidget> CreateWidgetButtonStrip(
		const TArray<TWeakObjectPtr<UHoudiniParameterButtonStrip>>& InParams,
		const TSharedPtr<SWidget>& ExtraWidgets);

	static TSharedRef<SWidget> CreateWidgetLabel(
		const TArray<TWeakObjectPtr<UHoudiniParameterLabel>>& LabelParams,
		const TSharedPtr<SWidget>& ExtraWidgets);

	static TSharedRef<SWidget> CreateWidgetToggle(
		const TArray<TWeakObjectPtr<UHoudiniParameterToggle>>& InParams,
		const TSharedPtr<SWidget>& ExtraWidgets);

	static void AddMultiParamWidgetsToBox(const TSharedRef<SHorizontalBox>& Box, const TSharedPtr<SWidget>& ExtraWidgets);

	static FText GetParameterTooltip(
		const UHoudiniParameter* InParam);

	static FString GetParameterTypeString(const EHoudiniParameterType InType, const int32 InTupleSize);

	static TSharedPtr<SWidget> CreateMultiParmWidgets(
		IDetailLayoutBuilder& DetailBuilder,
		const TArray<TWeakObjectPtr<UHoudiniParameterMultiParm>>& ParentMultiParams,
		int InstanceIndex);

	template< class T >
	static TArray<TWeakObjectPtr<T>> CastParameters(const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams);

	FDetailWidgetRow& CreatePropertyRow(
		IDetailCategoryBuilder& HouParameterCategory,
		const TSharedRef<SWidget>& NameWidget,
		const TSharedRef<SWidget>& ValueWidget);

	FDetailWidgetRow& CreateIndentedWholeRow(
		IDetailCategoryBuilder& HouParameterCategory,
		const TSharedRef<SWidget>& Widget);

	static TSharedRef<SWidget> Indent(const FParameterLayout* LayoutData, TSharedRef<SWidget> Widget);
	static FString GetHoudiniParameterTypeString(EHoudiniParameterType Type);
};

USTRUCT()
struct HOUDINIENGINEEDITOR_API FHoudiniParameterDetails
{
	// Main class used to create the details panel entries for parameters.

	GENERATED_BODY()

public:
	void CreateDetails(
		IDetailCategoryBuilder& HouParameterCategory,
		IDetailLayoutBuilder& DetailBuilder,
		const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables);

protected:
	void Construct(const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables);
	void Construct(UHoudiniCookable* HC, const TArray<TObjectPtr<UHoudiniParameter>>& Parameters);

	void AddParameterResetButton(IDetailCategoryBuilder& HouParameterCategory, const TArray<TWeakObjectPtr<UHoudiniCookable>>& InHCs);

	static void SetMultiParmWidgets(FHoudiniParameterView* ParameterView);

	TSharedPtr<FHoudiniParameterView> Root = nullptr;
	TArray<TSharedPtr<FHoudiniParameterView>> ParameterViews;

};

