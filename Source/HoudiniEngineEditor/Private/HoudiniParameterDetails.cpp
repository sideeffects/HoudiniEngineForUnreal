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

#include "HoudiniParameterDetails.h"

#include "HoudiniCookable.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "HoudiniParameterFolder.h"
#include "DetailCategoryBuilder.h"
#include "DetailColumnSizeData.h"
#include "EditorDirectories.h"
#include "FoliageType.h"
#include "HoudiniInputDetails.h"
#include "HoudiniParameterFolderList.h"
#include "HoudiniParameterTranslator.h"
#include "SAssetDropTarget.h"
#include "SHoudiniColorRamp.h"
#include "SHoudiniFloatRamp.h"
#include "SNewFilePathPicker.h"
#include "Layout/SeparatorBuilder.h"
#include "Math/UnitConversion.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/ConfigCacheIni.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "Widgets/Input/NumericUnitTypeInterface.inl"
#include "IPropertyUtilities.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

float HoudiniIndentColorScale = 0.11f;

FString FHoudiniParameterView::LayoutSection = TEXT("HoudiniEngine.Layout");

#define MULTIPARM_TOOLTIP "Changes the number of multiparms.\nThis requires the HDA to be cooked in an active Houdini session."
#define MULTIPARM_APPEND_TOOLTIP "Appends a new multiparm instance.\nThis requires the HDA to be cooked in an active Houdini session."
#define MULTIPARM_REMOVE_LAST_TOOLTIP "Removes the last  multiparm instance.\nThis requires the HDA to be cooked in an active Houdini session."
#define MULTIPARM_REMOVE_ALL_TOOLTIP "Removes all instances in the multiparm.\nThis requires the HDA to be cooked in an active Houdini session."

FDetailWidgetRow& FHoudiniParameterView::CreateIndentedWholeRow(
	IDetailCategoryBuilder& HouParameterCategory,
	const TSharedRef<SWidget>& WholeRowWidget)
{
	const FString& Label = this->GetParameterLabel();

	FLinearColor Color = FLinearColor::Black;

	TSharedPtr<SWidget> CustomRowContent = SNew(SBorder)
		.Padding(0.0f, 0.0f, 0.0f, 0.0f)
		.BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
		[
			SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
				.BorderBackgroundColor(Color)
				.Padding(0.0f)
				[
					WholeRowWidget
				]
		];

	TSharedPtr<SWidget> BoxedWidget = FHoudiniParameterView::Indent(LayoutData.Get(), CustomRowContent.ToSharedRef());

	FDetailWidgetRow& Row = HouParameterCategory.AddCustomRow(FText::FromString(Label));

	Row.WholeRowContent()
		[
			BoxedWidget.ToSharedRef()
		];

	return Row;
}

FDetailWidgetRow& FHoudiniParameterView::CreatePropertyRow(
	IDetailCategoryBuilder& HouParameterCategory,
	const TSharedRef<SWidget>& NameWidget,
	const TSharedRef<SWidget>& ValueWidget)
{
	FLinearColor Color = FLinearColor::Black;

	const FString& Label = this->GetParameterLabel();

	TSharedPtr<SWidget> CustomRowContent = SNew(SBorder)
		.Padding(0.0f, 0.0f, 0.0f, 0.0f)
		.BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
		[
			SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
				.BorderBackgroundColor(Color)
				.Padding(0.0f)
				[
					SAssignNew(Splitter, SSplitter)
						.Orientation(Orient_Horizontal)
						// Style to match Details panel splitter
						.Style(&FAppStyle::Get().GetWidgetStyle<FSplitterStyle>("DetailsView.Splitter"))
						.PhysicalSplitterHandleSize(1.0f)
						.HitDetectionSplitterHandleSize(5.0f)

						// Left: text/label
						+ SSplitter::Slot()
						.SizeRule(SSplitter::FractionOfParent)
						.Value_Lambda([this]() { return GetDividerExpansion(); })
						.OnSlotResized_Lambda([this](float NewValue)
						{
							SetDividerExpansion(NewValue);

							if (this->Parent)
							{
								// Update sibilings so they all stay in sink.
								for(auto Child : Parent->Children)
								{
									if(Child->Splitter.IsValid() && Child->Splitter->GetChildren()->Num() > 2)
									{
										SSplitter::FSlot* LeftSlot = (SSplitter::FSlot*)&Child->Splitter->GetChildren()->GetSlotAt(0);
										SSplitter::FSlot* RightSlot = (SSplitter::FSlot*)&Child->Splitter->GetChildren()->GetSlotAt(1);
										
										if(LeftSlot && RightSlot)
										{
											LeftSlot->SetSizeValue(NewValue);
											RightSlot->SetSizeValue(1.0 - NewValue);
										}

										Child->Splitter->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
									}
								}
							}
						})
						[
							SNew(SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
								.Padding(FMargin(4.f, 8.f, 4.f, 0.f))
								[
									NameWidget
								]
						]

					// Right: value widget
					+ SSplitter::Slot()
						.SizeRule(SSplitter::FractionOfParent)
						.Value_Lambda([this]() { return 1.0f - GetDividerExpansion(); })
						[
							SNew(SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
								.BorderBackgroundColor(Color)
								.Clipping(EWidgetClipping::ClipToBounds)
								.Padding(FMargin(5.0f, 0.0f))
							[
								ValueWidget
							]

						]
				]
		];


	TSharedPtr<SWidget> BoxedWidget = Indent(LayoutData.Get(), CustomRowContent.ToSharedRef());

	FDetailWidgetRow& Row = HouParameterCategory.AddCustomRow(FText::FromString(Label));

	Row.WholeRowContent()
		[
			BoxedWidget.ToSharedRef()
		];

	return Row;
}

TSharedRef<SWidget> FHoudiniParameterView::Indent(const FParameterLayout* LayoutData, TSharedRef<SWidget> Widget)
{
	TSharedPtr<SWidget> Root;
	TSharedPtr<SBorder> Last;

	for(int Indent = 0; Indent <= LayoutData->IndentLevel; Indent++)
	{
		FLinearColor Color = FLinearColor(0.f, 0.0f, 0.0f, 0.0f);
		if(Indent == LayoutData->IndentLevel)
		{
			Color = FLinearColor(LayoutData->ColorScale, LayoutData->ColorScale, LayoutData->ColorScale, LayoutData->ColorScale);
		}
		else if(Indent > 0)
		{
			Color = FLinearColor(HoudiniIndentColorScale, HoudiniIndentColorScale, HoudiniIndentColorScale, HoudiniIndentColorScale);
		}


		float Padding = (Indent > 0) ? 20.0f : 0.0f;

		TSharedPtr<SBorder> NewBorder = SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(Color)
			.Padding(Padding, 0.0f, 0.0f, 0.0f);

		if(Last.IsValid())
		{
			Last->SetContent(NewBorder.ToSharedRef());
		}
		else
		{
			Root = NewBorder;
		}
		Last = NewBorder;
	}

	if(Root.IsValid())
	{
		Last->SetContent(Widget);
	}
	else
	{
		Root = Widget;
	}
	return Root.ToSharedRef();
}

FHoudiniParameterView::FHoudiniParameterView() :
	Parent(nullptr)
{
	LayoutData = MakeShared<FParameterLayout>();
}

void 
FHoudiniParameterDetails::AddParameterResetButton(
	IDetailCategoryBuilder& HouParameterCategory,
	const TArray<TWeakObjectPtr<UHoudiniCookable>>& InHCs)
{
	auto ShouldEnableResetParametersButtonLambda = [InHCs]()
		{
			for(auto& NextHC : InHCs)
			{
				if(!IsValidWeakPointer(NextHC))
					continue;

				// Reset parameters to default values?
				for(int32 n = 0; n < NextHC->GetNumParameters(); ++n)
				{
					UHoudiniParameter* NextParm = NextHC->GetParameterAt(n);
					if(IsValid(NextParm) && !NextParm->IsDefault())
						return true;
				}
			}

			return false;
		};

	auto OnResetParametersClickedLambda = [InHCs]()
		{
			for(auto& NextHC : InHCs)
			{
				if(!IsValidWeakPointer(NextHC))
					continue;

				// Reset parameters to default values?
				for(int32 n = 0; n < NextHC->GetNumParameters(); ++n)
				{
					UHoudiniParameter* NextParm = NextHC->GetParameterAt(n);
					if(IsValid(NextParm) && !NextParm->IsDefault())
					{
						NextParm->RevertToDefault();
					}
				}
			}
			return FReply::Handled();
		};

	TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineUIResetParametersIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIResetParametersIconBrush();

	TSharedPtr<SButton> ResetParametersButton;
	TSharedPtr<SHorizontalBox> ResetParametersButtonHorizontalBox;

	TSharedPtr<SHorizontalBox> ButtonHorizontalBox = SNew(SHorizontalBox);

	ButtonHorizontalBox->AddSlot()
		//.MaxWidth(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
		//.Padding(2.0f, 0.0f, 0.0f, 2.0f)
		.HAlign(HAlign_Center)
		[
			SNew(SBox)
				.WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
				.HAlign(HAlign_Center)
				[
					SAssignNew(ResetParametersButton, SButton)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.ToolTipText(LOCTEXT("HoudiniAssetDetailsResetParametersAssetButton", "Reset the selected Houdini Asset's parameters to their default values."))
						//.Text(FText::FromString("Reset Parameters"))
						.IsEnabled_Lambda(ShouldEnableResetParametersButtonLambda)
						.Visibility(EVisibility::Visible)
						.OnClicked_Lambda(OnResetParametersClickedLambda)
						.Content()
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.HAlign(HAlign_Center)
								[
									SAssignNew(ResetParametersButtonHorizontalBox, SHorizontalBox)
								]
						]
				]
		];

	if(HoudiniEngineUIResetParametersIconBrush.IsValid())
	{
		TSharedPtr<SImage> ResetParametersImage;
		ResetParametersButtonHorizontalBox->AddSlot()
			.MaxWidth(16.0f)
			//.Padding(0.0f, 0.0f, 3.0f, 0.0f)
			[
				SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					[
						SAssignNew(ResetParametersImage, SImage)
							//.ColorAndOpacity(FSlateColor::UseForeground())
					]
			];

		ResetParametersImage->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda([HoudiniEngineUIResetParametersIconBrush]()
					{
						return HoudiniEngineUIResetParametersIconBrush.Get();
					})
			)
		);


		FDetailWidgetRow& DetailsRow = HouParameterCategory.AddCustomRow(LOCTEXT("ResetParameters", "ResetParameters"));

		DetailsRow.WholeRowWidget.Widget = ButtonHorizontalBox.ToSharedRef();

	}

	ResetParametersButtonHorizontalBox->AddSlot()
		.Padding(5.0, 0.0, 0.0, 0.0)
		//.FillWidth(4.2f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
				//.MinDesiredWidth(160.f)
				.Text(FText::FromString("Reset Parameters"))
		];
}

void
FHoudiniParameterDetails::CreateDetails(
	IDetailCategoryBuilder& HouParameterCategory,
	IDetailLayoutBuilder& DetailBuilder,
	const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniParameterDetails::CreateDetails);

	Construct(Cookables);

	if(Root.IsValid() && !Root->Children.IsEmpty())
	{
		AddParameterResetButton(HouParameterCategory, Cookables);

		bool bMultiParmsEditable = true;
		for(auto Param : Cookables[0]->GetParameters())
		{
			UHoudiniParameterMultiParm* MultiParm = Cast<UHoudiniParameterMultiParm>(Param);
			if(MultiParm && !MultiParm->CanModifyMultiParm())
			{
				bMultiParmsEditable = false;
				break;
			}
		}
		if (!bMultiParmsEditable)
		{
			HouParameterCategory.AddCustomRow(FText::FromString("MP Warning"))
				[
					SNew(SBox)
					.Padding(0.0f,10.0f)
					[
						SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.0f)))
							.Justification(ETextJustify::Center)
							.Text(FText::FromString("This HDA contains multiparms which will only be editable after cooking."))
					]
				];
		}


		Root->CreateDetails(HouParameterCategory, DetailBuilder);
	}
	else
	{
		HouParameterCategory.AddCustomRow(LOCTEXT("NoParameters", "NoParameters"))
			.WholeRowContent()
			[
				SNew(SBox)
				.Padding(5.0f, 5.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
							.Text(FText::FromString("HDA contains no parameters"))
							.Font(_GetEditorStyle().GetFontStyle(HOUDINI_DETAILS_FONT))
					]
			];
	}

	// Uncomment for debugging. Do not delete.
	//Root->PrintOut(0);

}

void FHoudiniParameterView::SetIndent(int Indent)
{
	UHoudiniParameter* Param = GetMainParameter();
	EHoudiniParameterType ParameterType = Param ? Param->GetParameterType() : EHoudiniParameterType::Invalid;

	switch(ParameterType)
	{
	case EHoudiniParameterType::Folder:
		this->LayoutData->IndentLevel = Indent;
		if(Indent > 0)
			this->LayoutData->ColorScale = HoudiniIndentColorScale;
		Indent++;
		break;

	case EHoudiniParameterType::MultiParm:
		LayoutData->IndentLevel = Indent;
		LayoutData->ColorScale = HoudiniIndentColorScale;
		Indent++;
		break;

	default:
		this->LayoutData->IndentLevel = Indent;
		if(Indent > 0)
			this->LayoutData->ColorScale = HoudiniIndentColorScale;
		break;
	}

	for(auto Child : Children)
	{
		Child->SetIndent(Indent);
	}
}

void
FHoudiniParameterDetails::Construct(const TArray<TWeakObjectPtr<UHoudiniCookable>>& Cookables)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniParameterDetails::CreateDetails);
	ParameterViews.Empty();

	if(!Cookables.IsEmpty() && Cookables[0].IsValid())
		Construct(Cookables[0].Get(), Cookables[0]->GetParameters());

	// Match up linked parameters.
	for(int32 ParamIdx = 0; ParamIdx < Cookables[0]->GetNumParameters(); ParamIdx++)
	{
		UHoudiniParameter* CurrentParam = Cookables[0]->GetParameterAt(ParamIdx);
		if(!IsValid(CurrentParam))
			continue;

		for(int LinkedIdx = 1; LinkedIdx < Cookables.Num(); LinkedIdx++)
		{
			UHoudiniParameter* LinkedParam = Cookables[LinkedIdx]->GetParameterAt(ParamIdx);
			if(!IsValid(LinkedParam))
				continue;

			// Linked params should match the main param! If not try to find one that matches
			if(!LinkedParam->Matches(*CurrentParam))
			{
				LinkedParam = Cookables[0]->FindMatchingParameter(CurrentParam);
				if(!IsValid(LinkedParam) || LinkedParam->IsChildParameter())
					continue;
			}

			ParameterViews[ParamIdx]->LinkedParameters.Add(LinkedParam);
		}
	}
}

void 
FHoudiniParameterDetails::SetMultiParmWidgets(FHoudiniParameterView* ParameterView)
{
	EHoudiniParameterType Type = ParameterView->GetParameterType();
	if(Type == EHoudiniParameterType::MultiParm)
	{
		int LastInstanceNum = -1;

		for (auto& Child : ParameterView->Children)
		{
			if(Child->GetParameterType() == EHoudiniParameterType::Invalid)
				continue;

			if (LastInstanceNum != Child->GetMainParameter()->GetMultiParmInstanceNumber())
			{
				if (Child->GetParameterType() == EHoudiniParameterType::FolderList)
				{
					// If the folder list is not tabs, but the multiparm on the folder.
					if (Child->GetFolderType() != EHoudiniFolderParameterType::Tabs)
					{
						FHoudiniParameterView* Folder = Child->Children.Num() > 0 ? Child->Children[0].Get() : nullptr;
						if(Folder)
							Folder->bShowMultiParms = true;
					}
					else
					{
						Child->bShowMultiParms = true;
					}

				}
				else
				{
					Child->bShowMultiParms = true;
				}


				LastInstanceNum = Child->GetMainParameter()->GetMultiParmInstanceNumber();
			}
		}
		
	}
	for (auto Child : ParameterView->Children)
	{
		SetMultiParmWidgets(Child.Get());
	}
}

void
FHoudiniParameterDetails::Construct(UHoudiniCookable* HC, const TArray<TObjectPtr<UHoudiniParameter>>& Parameters)
{

	// Create a parameter view for each parameter

	for(auto& Parameter : Parameters)
	{
		TSharedPtr<FHoudiniParameterView> View = MakeShareable(new FHoudiniParameterView());
		View->Name = Parameter->GetName();
		ParameterViews.Add(View);
		View->LinkedParameters.Add(Parameter);
	}

	// Keep track of joined parameters
	int ParamIndex = 0;
	while(ParamIndex < Parameters.Num())
	{
		int PrevParamIndex = ParamIndex;
		ParamIndex++;
		while(ParamIndex < Parameters.Num() &&  Parameters[PrevParamIndex]->GetJoinNext())
		{
			ParameterViews[PrevParamIndex]->NextJoined = ParameterViews[ParamIndex];
			ParameterViews[ParamIndex]->bIsJoinedToPrevious = true;
			PrevParamIndex++;
			ParamIndex++;
		}
	}

	// Map all Ids to parameters for quick look up
	TMap<UHoudiniParameter*, int> ParameterToIndex;
	for(int Index = 0; Index < Parameters.Num(); Index++)
	{
		ParameterToIndex.Add(Parameters[Index], Index);
	}


	// Construct tree.
	Root = MakeShareable(new FHoudiniParameterView());

	for(int Index = 0; Index < Parameters.Num(); Index++)
	{
		TSharedPtr<FHoudiniParameterView> ParentView = Root;
		if(Parameters[Index]->GetParent())
		{
			int ParentIndex = ParameterToIndex[Parameters[Index]->GetParent()];
			ParentView = ParameterViews[ParentIndex];
		}

		ParentView->Children.Add(ParameterViews[Index]);
		ParameterViews[Index]->Parent = ParentView;
		ParameterViews[Index]->Cookable = HC;
	}

	// Set multiparm widgets
	SetMultiParmWidgets(Root.Get());

	Root.Get()->SetIndent(0);
}

TSharedRef<SWidget> 
FHoudiniParameterView::CreateNameWidget(const UHoudiniParameter* Parameter)
{
	bool bShowLabel = Parameter->IsLabelVisible();

	TSharedRef<SWidget> Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0)
		.Padding(0.0f, 0.0f)
		[
			SNew(STextBlock)
				.Clipping(EWidgetClipping::ClipToBoundsAlways)
				.Text(FText::FromString(bShowLabel ? Parameter->GetParameterLabel() : FString(TEXT(""))))
				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				.ToolTipText(GetParameterTooltip(Parameter))

		];
	return Widget;
}

FString FHoudiniParameterView::GetDividerLayoutKey() const
{
	if(!this->Cookable.IsValid())
	{
		// Sometimes Unreal will try to recreate the details panel after the cookable has been destroyed.
		return TEXT("");
	}

	FString Result = this->Cookable->GetHoudiniAsset()->GetPathName();

	if (this->Parent && this->Parent->GetMainParameter())
	{
		Result += TEXT("/") / this->Parent->GetMainParameter()->GetParameterName();
	}

	Result += TEXT("_split");
	return Result;
}

void FHoudiniParameterView::SetDividerExpansion(float Value) const
{
	FString Key = GetDividerLayoutKey();
	GConfig->SetFloat(*LayoutSection, *Key, Value, GEditorPerProjectIni);
//	GConfig->Flush(false, GEditorPerProjectIni);
}

float FHoudiniParameterView::GetDividerExpansion() const
{
	FString Key = GetDividerLayoutKey();
	float Value;
	bool bExists = GConfig->GetFloat(*LayoutSection, *Key, Value, GEditorPerProjectIni);
	if(!bExists)
	{
		SetDividerExpansion(0.3f);
	}

	return Value;
}

void 
FHoudiniParameterView::CreateJoinedDetails(IDetailCategoryBuilder& HouParameterCategory, IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TSharedRef<SWidget>> WidgetNames;
	TArray<TSharedRef<SWidget>> WidgetValues;

	for (FHoudiniParameterView* Parameter = this; Parameter != nullptr; Parameter = Parameter->NextJoined.Get())
	{
		UHoudiniParameter* Param = Parameter->GetMainParameter();

		if(IsValid(Param) && !Param->IsVisible())
			continue;

		EHoudiniParameterType ParameterType = Param ? Param->GetParameterType() : EHoudiniParameterType::Invalid;

		switch(ParameterType)
		{
		case EHoudiniParameterType::Int:
		{
			TArray<TWeakObjectPtr<UHoudiniParameterInt>> IntParams = CastParameters<UHoudiniParameterInt>(Parameter->LinkedParameters);
			TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
			TSharedRef<SWidget> ValueWidget = CreateWidgetInt(IntParams, nullptr);
			WidgetNames.Add(NameWidget);
			WidgetValues.Add(ValueWidget);
			break;
		}
		case EHoudiniParameterType::Float:
		{
			TArray<TWeakObjectPtr<UHoudiniParameterFloat>> FloatParams = CastParameters<UHoudiniParameterFloat>(Parameter->LinkedParameters);

			TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
			TSharedRef<SWidget> ValueWidget = CreateWidgetFloat(FloatParams, nullptr);
			WidgetNames.Add(NameWidget);
			WidgetValues.Add(ValueWidget);
			break;
		}
		case EHoudiniParameterType::String:
		case EHoudiniParameterType::StringAssetRef:
		{
			TArray<TWeakObjectPtr<UHoudiniParameterString>> StringParams = CastParameters<UHoudiniParameterString>(Parameter->LinkedParameters);
			TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
			TSharedRef<SWidget> ValueWidget = CreateWidgetString(HouParameterCategory, StringParams, nullptr);
			WidgetNames.Add(NameWidget);
			WidgetValues.Add(ValueWidget);
			break;
		}
		case EHoudiniParameterType::IntChoice:
		{
			TArray<TWeakObjectPtr<UHoudiniParameterChoice>> TypedParams = CastParameters<UHoudiniParameterChoice>(Parameter->LinkedParameters);
			TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
			TSharedRef<SWidget> ValueWidget = CreateWidgetChoice(TypedParams, nullptr);
			WidgetNames.Add(NameWidget);
			WidgetValues.Add(ValueWidget);
			break;
		}
		case EHoudiniParameterType::StringChoice:
		{
			TArray<TWeakObjectPtr<UHoudiniParameterChoice>> TypedParams = CastParameters<UHoudiniParameterChoice>(Parameter->LinkedParameters);
			TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
			TSharedRef<SWidget> ValueWidget = CreateWidgetChoice(TypedParams, nullptr);
			WidgetNames.Add(NameWidget);
			WidgetValues.Add(ValueWidget);
			break;
		}

		case EHoudiniParameterType::Color:
		{
			TArray<TWeakObjectPtr<UHoudiniParameterColor>> TypedParams = CastParameters<UHoudiniParameterColor>(Parameter->LinkedParameters);
			TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
			TSharedRef<SWidget> ValueWidget = CreateWidgetColor(TypedParams, nullptr);
			WidgetNames.Add(NameWidget);
			WidgetValues.Add(ValueWidget);
			break;
		}
		case EHoudiniParameterType::Button:
		{
			TArray<TWeakObjectPtr<UHoudiniParameterButton>> TypedParams = CastParameters<UHoudiniParameterButton>(Parameter->LinkedParameters);
			TSharedRef<SWidget> NameWidget = SNullWidget::NullWidget;
			TSharedRef<SWidget> ValueWidget = CreateWidgetButton(TypedParams, nullptr);
			WidgetNames.Add(NameWidget);
			WidgetValues.Add(ValueWidget);
			break;
		}

		case EHoudiniParameterType::Label:
		{
			TArray<TWeakObjectPtr<UHoudiniParameterLabel>> TypedParams = CastParameters<UHoudiniParameterLabel>(Parameter->LinkedParameters);
			TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
			TSharedRef<SWidget> ValueWidget = CreateWidgetLabel(TypedParams, nullptr);
			WidgetNames.Add(NameWidget);
			WidgetValues.Add(ValueWidget);
			break;
		}

		case EHoudiniParameterType::Toggle:
		{
			TArray<TWeakObjectPtr<UHoudiniParameterToggle>> TypedParams = CastParameters<UHoudiniParameterToggle>(Parameter->LinkedParameters);
			TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
			TSharedRef<SWidget> ValueWidget = CreateWidgetToggle(TypedParams, nullptr);
			WidgetNames.Add(NameWidget);
			WidgetValues.Add(ValueWidget);
			break;
		}
		
		default:
			break;
		}
	}


	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	for (int Index = 0; Index < WidgetNames.Num(); Index++)
	{
		HorizontalBox->AddSlot()
			.Padding(0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					[
						SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
							.Padding(FMargin(4.f, 8.f, 4.f, 0.f))
							[
								WidgetNames[Index]
							]

					]
					+ SHorizontalBox::Slot()
					[
						WidgetValues[Index]
					]
			];
	}

	CreateIndentedWholeRow(
		HouParameterCategory,
		HorizontalBox);

}
void 
FHoudiniParameterView::CreateDetails(IDetailCategoryBuilder& HouParameterCategory, IDetailLayoutBuilder& DetailBuilder)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniParameterView::CreateDetails);

	UHoudiniParameter* Param = (!this->LinkedParameters.IsEmpty() && LinkedParameters[0].IsValid()) ?
		this->LinkedParameters[0].Get() : nullptr;

	if(IsValid(Param) && !Param->IsVisible())
		return;

	EHoudiniParameterType ParameterType = Param ? Param->GetParameterType() : EHoudiniParameterType::Invalid;

	UHoudiniParameter* ParentParm = nullptr;
	if(this->Parent)
	{
		ParentParm = Parent->GetMainParameter();
	}

	TSharedPtr<SWidget> MultiParmButtons;

	if(this->bShowMultiParms)
	{
		FHoudiniParameterView* MultiParm = Parent.Get();
		while (MultiParm != nullptr && MultiParm->GetParameterType() != EHoudiniParameterType::MultiParm)
		{
			MultiParm = MultiParm->Parent.Get();
		}

		if(MultiParm)
		{
			TArray<TWeakObjectPtr<UHoudiniParameterMultiParm>> TypedParentParams = CastParameters<UHoudiniParameterMultiParm>(MultiParm->LinkedParameters);

			int Index = Param->GetMultiParmInstanceNumber();
			MultiParmButtons = CreateMultiParmWidgets(DetailBuilder, TypedParentParams, Index);
		}
	}

	bool bDisplayChildren = true;

	if(this->bIsJoinedToPrevious)
	{
		return;
	}

	if (this->NextJoined.IsValid())
	{
		CreateJoinedDetails(HouParameterCategory, DetailBuilder);
		return;
	}
	
	switch(ParameterType)
	{
	case EHoudiniParameterType::FolderList:
	{
		TArray<TSharedPtr<FHoudiniParameterView>> FoldersParams;

		EHoudiniFolderParameterType FolderType = EHoudiniFolderParameterType::Invalid;

		for(auto Child : Children)
		{
			if(Child->LinkedParameters[0]->GetParameterType() == EHoudiniParameterType::Folder)
			{
				FoldersParams.Add(Child);
				UHoudiniParameterFolder* FolderParam = Cast<UHoudiniParameterFolder>(Child->LinkedParameters[0]);
				FolderType = FolderParam->GetFolderType();
			}
		}

		switch(FolderType)
		{
		case EHoudiniFolderParameterType::Radio: {
			CreateRadioFolderRow(HouParameterCategory, DetailBuilder, FoldersParams, MultiParmButtons);
			break;
		}
		case EHoudiniFolderParameterType::Tabs:
		{
			CreateTabbedFolderRow(HouParameterCategory, DetailBuilder, FoldersParams, MultiParmButtons);
			break;
		}
		case EHoudiniFolderParameterType::Simple:
			break;

		case EHoudiniFolderParameterType::Collapsible:
			break;
		default:
			break;
		}

		break;
	}

	case EHoudiniParameterType::Folder:
	{
		TArray<TSharedPtr<FHoudiniParameterView>> FoldersParams;
		UHoudiniParameterFolder* FolderParam = GetTypedMainParameter<UHoudiniParameterFolder>();
		EHoudiniFolderParameterType FolderType = FolderParam->GetFolderType();

		switch(FolderType)
		{
		case EHoudiniFolderParameterType::Simple:
		{
			CreateSimpleFolderRow(HouParameterCategory, DetailBuilder, MultiParmButtons);
			break;
		}
		case EHoudiniFolderParameterType::Collapsible:
		{
			CreateCollapsableFolderRow(HouParameterCategory, DetailBuilder, MultiParmButtons);
			bDisplayChildren = FolderParam->IsExpanded();
			break;
		}
		case EHoudiniFolderParameterType::Tabs:
		{
			bDisplayChildren = FolderParam->IsChosen();
			break;
		}
		case EHoudiniFolderParameterType::Radio:
		{
			bDisplayChildren = FolderParam->IsChosen();
			break;
		}
		default:
			break;
		}
		break;
	}

	case EHoudiniParameterType::MultiParm:
	{
		TArray<TWeakObjectPtr<UHoudiniParameterMultiParm>> TypedParams = CastParameters<UHoudiniParameterMultiParm>(LinkedParameters);
		TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
		TSharedRef<SWidget> ValueWidget = CreateWidgetMultiParm(DetailBuilder, TypedParams, MultiParmButtons);

		FDetailWidgetRow& Row = CreatePropertyRow(
			HouParameterCategory,
			NameWidget,
			ValueWidget);

		break;
	}

	case EHoudiniParameterType::Int:
	{
		TArray<TWeakObjectPtr<UHoudiniParameterInt>> IntParams = CastParameters<UHoudiniParameterInt>(LinkedParameters);
		TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
		TSharedRef<SWidget> ValueWidget = CreateWidgetInt(IntParams, MultiParmButtons);

		FDetailWidgetRow& Row = CreatePropertyRow(
			HouParameterCategory,
			NameWidget,
			ValueWidget);

		break;
	}
	case EHoudiniParameterType::Float:
	{
		TArray<TWeakObjectPtr<UHoudiniParameterFloat>> FloatParams = CastParameters<UHoudiniParameterFloat>(LinkedParameters);

		TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
		TSharedRef<SWidget> ValueWidget = CreateWidgetFloat(FloatParams, MultiParmButtons);

		FDetailWidgetRow& Row = CreatePropertyRow(
			HouParameterCategory,
			NameWidget,
			ValueWidget);

		break;
	}
	case EHoudiniParameterType::String:
	case EHoudiniParameterType::StringAssetRef:
	{
		TArray<TWeakObjectPtr<UHoudiniParameterString>> StringParams = CastParameters<UHoudiniParameterString>(LinkedParameters);
		TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);

		TSharedRef<SWidget> ValueWidget = CreateWidgetString(HouParameterCategory, StringParams, MultiParmButtons);

		FDetailWidgetRow& Row = CreatePropertyRow(
			HouParameterCategory,
			NameWidget,
			ValueWidget);

		break;
	}
	case EHoudiniParameterType::IntChoice:
	{
		TArray<TWeakObjectPtr<UHoudiniParameterChoice>> TypedParams = CastParameters<UHoudiniParameterChoice>(LinkedParameters);
		TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
		TSharedRef<SWidget> ValueWidget = CreateWidgetChoice(TypedParams, MultiParmButtons);

		FDetailWidgetRow& Row = CreatePropertyRow(
			HouParameterCategory,
			NameWidget,
			ValueWidget);

		break;
	}
	case EHoudiniParameterType::StringChoice:
	{
		TArray<TWeakObjectPtr<UHoudiniParameterChoice>> TypedParams = CastParameters<UHoudiniParameterChoice>(LinkedParameters);
		TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
		TSharedRef<SWidget> ValueWidget = CreateWidgetChoice(TypedParams, MultiParmButtons);

		FDetailWidgetRow& Row = CreatePropertyRow(
			HouParameterCategory,
			NameWidget,
			ValueWidget);

		break;
	}

	case EHoudiniParameterType::Separator:
	{
		TArray<TWeakObjectPtr<UHoudiniParameter>> TypedParams = CastParameters<UHoudiniParameter>(LinkedParameters);
		TSharedRef<SWidget> Widget = CreateWidgetSeparator(TypedParams, MultiParmButtons);

		FDetailWidgetRow& Row = CreateIndentedWholeRow(
			HouParameterCategory,
			Widget);
		break;
	}

	case EHoudiniParameterType::Color:
	{
		TArray<TWeakObjectPtr<UHoudiniParameterColor>> TypedParams = CastParameters<UHoudiniParameterColor>(LinkedParameters);
		TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
		TSharedRef<SWidget> ValueWidget = CreateWidgetColor(TypedParams, MultiParmButtons);

		FDetailWidgetRow& Row = CreatePropertyRow(
			HouParameterCategory,
			NameWidget,
			ValueWidget);
		break;
	}

	case EHoudiniParameterType::ColorRamp:
	{
		bDisplayChildren = false;

		TArray<TWeakObjectPtr<UHoudiniParameterRampColor>> TypedParams = CastParameters<UHoudiniParameterRampColor>(LinkedParameters);
		TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
		TSharedRef<SWidget> ValueWidget = CreateWidgetColorRamp(TypedParams, MultiParmButtons);

		FDetailWidgetRow& Row = CreatePropertyRow(
			HouParameterCategory,
			NameWidget,
			ValueWidget);
		break;
	}

	case EHoudiniParameterType::FloatRamp:
	{
		bDisplayChildren = false;

		TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>> TypedParams = CastParameters<UHoudiniParameterRampFloat>(LinkedParameters);
		TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
		TSharedRef<SWidget> ValueWidget = CreateWidgetFloatRamp(TypedParams, MultiParmButtons);

		FDetailWidgetRow& Row = CreatePropertyRow(
			HouParameterCategory,
			NameWidget,
			ValueWidget);
		break;
	}

	case EHoudiniParameterType::Button:
	{
		TArray<TWeakObjectPtr<UHoudiniParameterButton>> TypedParams = CastParameters<UHoudiniParameterButton>(LinkedParameters);
		TSharedRef<SWidget> NameWidget = SNullWidget::NullWidget;
		TSharedRef<SWidget> ValueWidget = CreateWidgetButton(TypedParams, MultiParmButtons);

		FDetailWidgetRow& Row = CreatePropertyRow(
			HouParameterCategory,
			NameWidget,
			ValueWidget);
		break;
	}

	case EHoudiniParameterType::File:
	case EHoudiniParameterType::FileDir:
	case EHoudiniParameterType::FileGeo:
	case EHoudiniParameterType::FileImage:
	{
		TArray<TWeakObjectPtr<UHoudiniParameterFile>> TypedParams = CastParameters<UHoudiniParameterFile>(LinkedParameters);
		TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
		TSharedRef<SWidget> ValueWidget = CreateWidgetFile(TypedParams, MultiParmButtons);

		FDetailWidgetRow& Row = CreatePropertyRow(
			HouParameterCategory,
			NameWidget,
			ValueWidget);
		break;
	}
	case EHoudiniParameterType::ButtonStrip:
	{
		TArray<TWeakObjectPtr<UHoudiniParameterButtonStrip>> TypedParams = CastParameters<UHoudiniParameterButtonStrip>(LinkedParameters);
		TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
		TSharedRef<SWidget> ValueWidget = CreateWidgetButtonStrip(TypedParams, MultiParmButtons);

		FDetailWidgetRow& Row = CreatePropertyRow(
			HouParameterCategory,
			NameWidget,
			ValueWidget);
		break;
	}

	case EHoudiniParameterType::Input:
	{
		TArray<TWeakObjectPtr<UHoudiniParameterOperatorPath>> TypedParams = CastParameters<UHoudiniParameterOperatorPath>(LinkedParameters);
		TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
		TSharedRef<SWidget> ValueWidget = CreateWidgetOperatorPath(DetailBuilder, HouParameterCategory, TypedParams, MultiParmButtons);

		FDetailWidgetRow& Row = CreatePropertyRow(
			HouParameterCategory,
			NameWidget,
			ValueWidget);

		break;
	}
	case EHoudiniParameterType::Label:
	{
		TArray<TWeakObjectPtr<UHoudiniParameterLabel>> TypedParams = CastParameters<UHoudiniParameterLabel>(LinkedParameters);
		TSharedRef<SWidget> NameWidget = CreateNameWidget(Param); 
		TSharedRef<SWidget> ValueWidget = CreateWidgetLabel(TypedParams, MultiParmButtons);

		FDetailWidgetRow& Row = CreatePropertyRow(
			HouParameterCategory,
			NameWidget,
			ValueWidget);
		break;
	}

	case EHoudiniParameterType::Toggle:
	{
		TArray<TWeakObjectPtr<UHoudiniParameterToggle>> TypedParams = CastParameters<UHoudiniParameterToggle>(LinkedParameters);
		TSharedRef<SWidget> NameWidget = CreateNameWidget(Param);
		TSharedRef<SWidget> ValueWidget = CreateWidgetToggle(TypedParams, MultiParmButtons);

		FDetailWidgetRow& Row = CreatePropertyRow(
			HouParameterCategory,
			NameWidget,
			ValueWidget);
		break;
	}
	case EHoudiniParameterType::Invalid:
		break;
	}

	if(bDisplayChildren)
	{
		// Create Details for children

		for(auto& Child : Children)
		{
			Child->CreateDetails(HouParameterCategory, DetailBuilder);
		}
	}
}

UHoudiniParameter* 
FHoudiniParameterView::GetMainParameter() const
{
	if(LinkedParameters.IsEmpty())
		return nullptr;
	else
		return LinkedParameters[0].IsValid() ? LinkedParameters[0].Get() : nullptr;
}

EHoudiniFolderParameterType 
FHoudiniParameterView::GetFolderType() const
{
	// Returns the type of folder. Input can be a folder or folder list.

	UHoudiniParameterFolder* Folder = Cast<UHoudiniParameterFolder>(GetMainParameter());
	if(Folder)
		return Folder->GetFolderType();

	UHoudiniParameterFolderList* FolderList = Cast<UHoudiniParameterFolderList>(GetMainParameter());
	if(!FolderList)
		return EHoudiniFolderParameterType::Invalid;

	FHoudiniParameterView* Child = this->Children.IsEmpty() ? nullptr : this->Children[0].Get();
	if(!Child)
		return EHoudiniFolderParameterType::Invalid;

	Folder = Cast<UHoudiniParameterFolder>(Child->GetMainParameter());

	if (Folder)
		return Folder->GetFolderType();
	else
		return EHoudiniFolderParameterType::Invalid;
}


EHoudiniParameterType 
FHoudiniParameterView::GetParameterType() const
{
	UHoudiniParameter* Parameter = GetMainParameter();
	if(Parameter)
		return Parameter->GetParameterType();
	else
		return EHoudiniParameterType::Invalid;

}

const FString& 
FHoudiniParameterView::GetParameterLabel() const
{
	UHoudiniParameter* Parameter = GetMainParameter();
	if(Parameter)
		return Parameter->GetParameterLabel();
	else
	{
		static const FString EmptyParameterString = TEXT("");
		return EmptyParameterString;
	}
}


void 
FHoudiniParameterView::CreateTabbedFolderRow(
	IDetailCategoryBuilder& HouParameterCategory,
	IDetailLayoutBuilder& DetailBuilder,
	TArray<TSharedPtr<FHoudiniParameterView>> FoldersParams,
	const TSharedPtr<SWidget>& MultiParmAddRemoveButtons)
{
	UHoudiniParameter* Parameter = GetMainParameter();

	TSharedPtr<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	for(int32 Index = 0; Index < FoldersParams.Num(); Index++)
	{
		TSharedPtr<FHoudiniParameterView> Folder = FoldersParams[Index];

		if(!Folder->GetMainParameter()->IsVisible())
			continue;

		HorizontalBox->AddSlot()
			.Padding(0.0f, 10.0f, 2.0f, 0.0f)
			.AutoWidth()
			[
				SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor_Lambda([FoldersParams, Index]()
						{
							bool bChosen = false;
							if(FoldersParams[Index].IsValid())
							{
								UHoudiniParameterFolder* ParameterFolder = Cast<UHoudiniParameterFolder>(FoldersParams[Index]->LinkedParameters[0].Get());
								bChosen = ParameterFolder->IsChosen();
							}

							return bChosen ? FSlateColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.2f)) : FSlateColor(FLinearColor(0.015f, 0.015f, 0.015f, 0.5f));
						})
					[
						SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "NoBorder")
							.ButtonColorAndOpacity_Lambda([FoldersParams, Index]()
								{
									return FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
								})
							.OnClicked_Lambda([this, FoldersParams, Index, &DetailBuilder]()
								{
									for(int It = 0; It < FoldersParams.Num(); It++)
									{
										UHoudiniParameterFolder* ParameterFolder = Cast<UHoudiniParameterFolder>(FoldersParams[It]->LinkedParameters[0].Get());
										ParameterFolder->SetChosen(It == Index);
										ParameterFolder->MarkChanged(true);
									}

									DetailBuilder.ForceRefreshDetails();

									return FReply::Handled();
								})
							[
								SNew(STextBlock)
									.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
									.Text(FText::FromString(Folder->LinkedParameters[0]->GetParameterLabel()))
							]
					]
			];
	}
	if(MultiParmAddRemoveButtons.IsValid())
	{
		HorizontalBox->AddSlot()
			.Padding(0.0f, 10.0f, 2.0f, 0.0f)
			[
				MultiParmAddRemoveButtons.ToSharedRef()
			];

	}

	FDetailWidgetRow& TabRow = HouParameterCategory.AddCustomRow(FText::FromString(Parameter->GetParameterLabel()));
	TabRow.WholeRowContent()
		[
			Indent(this->LayoutData.Get(), HorizontalBox.ToSharedRef())
		];

}

void 
FHoudiniParameterView::CreateRadioFolderRow(
	IDetailCategoryBuilder& HouParameterCategory,
	IDetailLayoutBuilder& DetailBuilder,
	TArray<TSharedPtr<FHoudiniParameterView>> FoldersParams,
	const TSharedPtr<SWidget>& MultiParmAddRemoveButtons)
{
	UHoudiniParameter* Parameter = GetMainParameter();

	TSharedPtr<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	for(int32 Index = 0; Index < FoldersParams.Num(); Index++)
	{
		TSharedPtr<FHoudiniParameterView> Folder = FoldersParams[Index];

		if(!Folder->GetMainParameter()->IsVisible())
			continue;

		HorizontalBox->AddSlot()
			.Padding(0.0f, 10.0f, 2.0f, 0.0f)
			.AutoWidth()
			[
				SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor_Lambda([FoldersParams, Index]()
						{
							bool bChosen = false;
							if(FoldersParams[Index].IsValid())
							{
								UHoudiniParameterFolder* ParameterFolder = Cast<UHoudiniParameterFolder>(FoldersParams[Index]->LinkedParameters[0].Get());
								bChosen = ParameterFolder->IsChosen();
							}

							return bChosen ? FSlateColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.2f)) : FSlateColor(FLinearColor(0.015f, 0.015f, 0.015f, 0.5f));
						})
					[
						SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "NoBorder")
							.ButtonColorAndOpacity_Lambda([FoldersParams, Index]()
								{
									return FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
								})
							.OnClicked_Lambda([this, FoldersParams, Index, &DetailBuilder]()
								{
									for(int It = 0; It < FoldersParams.Num(); It++)
									{
										UHoudiniParameterFolder* ParameterFolder = Cast<UHoudiniParameterFolder>(FoldersParams[It]->LinkedParameters[0].Get());
										ParameterFolder->SetChosen(It == Index);
										ParameterFolder->MarkChanged(true);
									}

									DetailBuilder.ForceRefreshDetails();

									return FReply::Handled();
								})
							[
								SNew(STextBlock)
									.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
									.Text_Lambda([FoldersParams, Index]()
										{
											FString Text;

											if(!FoldersParams.IsEmpty())
											{
												UHoudiniParameterFolder* ParameterFolder = Cast<UHoudiniParameterFolder>(FoldersParams[Index]->GetMainParameter());
												if(ParameterFolder->IsChosen())
												{
													Text = FString::Printf(TEXT("\u25C9 %s"), *ParameterFolder->GetParameterLabel());
												}
												else
												{
													Text = FString::Printf(TEXT("\u25EF %s"), *ParameterFolder->GetParameterLabel());
												}
											}
											return FText::FromString(Text);
										})
							]
					]
			];
	}
	if(MultiParmAddRemoveButtons.IsValid())
	{
		HorizontalBox->AddSlot()
			.Padding(0.0f, 10.0f, 2.0f, 0.0f)
			[
				MultiParmAddRemoveButtons.ToSharedRef()
			];

	}

	FDetailWidgetRow& TabRow = HouParameterCategory.AddCustomRow(FText::FromString(Parameter->GetParameterLabel()));
	TabRow.WholeRowContent()
		[
			Indent(this->LayoutData.Get(), HorizontalBox.ToSharedRef())
		];

}

void 
FHoudiniParameterView::CreateCollapsableFolderRow(
	IDetailCategoryBuilder& HouParameterCategory,
	IDetailLayoutBuilder& DetailBuilder,
	const TSharedPtr<SWidget>& MultiParmAddRemoveButtons)
{
	UHoudiniParameterFolder* FolderParam = GetTypedMainParameter<UHoudiniParameterFolder>();
	if(!FolderParam)
		return;

	TSharedPtr<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	TSharedRef<SVerticalBox> ExportOptions_VerticalBox = SNew(SVerticalBox);

	FString Title = FolderParam->GetParameterLabel();

	TSharedRef<SWidget> ExportOptions_Expandable = SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.2f))
		[
			SNew(SExpandableArea)
				.BorderBackgroundColor(FLinearColor::Transparent)
				.AreaTitle_Lambda([Title]() { return FText::FromString(Title);})
				.InitiallyCollapsed(!FolderParam->IsExpanded())
				.OnAreaExpansionChanged_Lambda([this, &DetailBuilder](bool bIsExpanded)
					{
						auto Parameter = this->GetTypedMainParameter<UHoudiniParameterFolder>();
						if(Parameter)
						{
							Parameter->SetExpanded(bIsExpanded);
							DetailBuilder.ForceRefreshDetails();
						}
					})
		];

	HorizontalBox->AddSlot()
		[
			ExportOptions_Expandable
		];

	if(MultiParmAddRemoveButtons.IsValid())
	{
		HorizontalBox->AddSlot()
			.Padding(10.0f, 1.0f, 0.0f, 0.0f)
			[
				MultiParmAddRemoveButtons.ToSharedRef()
			];
	}

	FDetailWidgetRow& TabRow = HouParameterCategory.AddCustomRow(FText::FromString(FolderParam->GetParameterLabel()));
	TabRow.WholeRowContent()
		[
			Indent(this->Parent->LayoutData.Get(), HorizontalBox.ToSharedRef())
		];
}


void 
FHoudiniParameterView::CreateSimpleFolderRow(
	IDetailCategoryBuilder& HouParameterCategory,
	IDetailLayoutBuilder& DetailBuilder,
	const TSharedPtr<SWidget>& MultiParmAddRemoveButtons)
{
	UHoudiniParameterFolder* FolderParam = GetTypedMainParameter<UHoudiniParameterFolder>();
	if(!FolderParam)
		return;

	EHoudiniFolderParameterType FolderType = FolderParam->GetFolderType();

	TSharedPtr<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	HorizontalBox->AddSlot()
		.FillWidth(1.0f)
		[
			SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.2f))
				[
					SNew(SBorder)
						.Padding(FMargin(4.0f, 4.0f, 0, 0))   // left offset
						.BorderImage(FStyleDefaults::GetNoBrush())
						[
							SNew(STextBlock)
								.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
								.Text(FText::FromString(FolderParam->GetParameterLabel()))
						]
				]
		];

	if(MultiParmAddRemoveButtons.IsValid())
	{
		HorizontalBox->AddSlot()
			.Padding(10.0f, 1.0f, 2.0f, 0.0f)
			[
				MultiParmAddRemoveButtons.ToSharedRef()
			];
	}

	FDetailWidgetRow& TabRow = HouParameterCategory.AddCustomRow(FText::FromString(FolderParam->GetParameterLabel()));
	TabRow.WholeRowContent()
		[
			Indent(this->Parent->LayoutData.Get(), HorizontalBox.ToSharedRef())
		];
}

FString
FHoudiniParameterView::GetHoudiniParameterTypeString(EHoudiniParameterType Type)
{
	switch(Type)
	{
	case EHoudiniParameterType::Invalid:
		return FString(TEXT("Invalid"));
	case EHoudiniParameterType::Button:
		return FString(TEXT("Button"));
	case EHoudiniParameterType::ButtonStrip:
		return FString(TEXT("ButtonStrip"));
	case EHoudiniParameterType::Color:
		return FString(TEXT("Color"));
	case EHoudiniParameterType::ColorRamp:
		return FString(TEXT("ColorRamp"));
	case EHoudiniParameterType::File:
		return FString(TEXT("File"));
	case EHoudiniParameterType::FileDir:
		return FString(TEXT("FileDir"));
	case EHoudiniParameterType::FileGeo:
		return FString(TEXT("FileGeo"));
	case EHoudiniParameterType::FileImage:
		return FString(TEXT("FileImage"));
	case EHoudiniParameterType::Float:
		return FString(TEXT("Float"));
	case EHoudiniParameterType::FloatRamp:
		return FString(TEXT("FloatRamp"));
	case EHoudiniParameterType::Folder:
		return FString(TEXT("Folder"));
	case EHoudiniParameterType::FolderList:
		return FString(TEXT("FolderList"));
	case EHoudiniParameterType::Input:
		return FString(TEXT("Input"));
	case EHoudiniParameterType::Int:
		return FString(TEXT("Int"));
	case EHoudiniParameterType::IntChoice:
		return FString(TEXT("IntChoice"));
	case EHoudiniParameterType::Label:
		return FString(TEXT("Label"));
	case EHoudiniParameterType::MultiParm:
		return FString(TEXT("MultiParm"));
	case EHoudiniParameterType::Separator:
		return FString(TEXT("Separator"));
	case EHoudiniParameterType::String:
		return FString(TEXT("String"));
	case EHoudiniParameterType::StringChoice:
		return FString(TEXT("StringChoice"));
	case EHoudiniParameterType::StringAssetRef:
		return FString(TEXT("StringAssetRef"));
	case EHoudiniParameterType::Toggle:
		return FString(TEXT("Toggle"));
	default:
		return FString(TEXT("Unknown"));
	}
};

void 
FHoudiniParameterView::PrintOut(int Spacing)
{
	FString Spaces = FString::ChrN(Spacing * 4, ' ');

	FString Label;
	if(!this->LinkedParameters.IsEmpty())
		Label = *this->LinkedParameters[0]->GetParameterLabel();

	HOUDINI_LOG_MESSAGE(TEXT("%sName: %s"), *Spaces, *Label);

	FString ParameterTypeName = TEXT("None");
	if(!this->LinkedParameters.IsEmpty())
		ParameterTypeName = GetHoudiniParameterTypeString(this->LinkedParameters[0]->GetParameterType());

	HOUDINI_LOG_MESSAGE(TEXT("%sType: %s"), *Spaces, *ParameterTypeName);
	HOUDINI_LOG_MESSAGE(TEXT("%sIndent: %d"), *Spaces, this->LayoutData->IndentLevel);

	for(auto Child : Children)
	{
		Child->PrintOut(Spacing + 1);
	}
}


TSharedRef<SWidget>
FHoudiniParameterView::CreateWidgetFloatRamp(
	const TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>>& FloatRampParameters,
	const TSharedPtr<SWidget>& ExtraWidgets)
{
	TSharedPtr<SWidget> Widget = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(FMargin(0.0f, 20.0f))
		.AutoHeight()
		[
			SNew(SHoudiniFloatRamp)
				.RampParameters(FloatRampParameters)
				.OnValueCommitted_Lambda(
					[]() { FHoudiniEngineUtils::UpdateEditorProperties(true); })
		];

	return Widget.ToSharedRef();
}

TSharedRef<SWidget>
FHoudiniParameterView::CreateWidgetColorRamp(
	const TArray<TWeakObjectPtr<UHoudiniParameterRampColor>>& ColorRampParameters,
	const TSharedPtr<SWidget>& ExtraWidgets)
{
	TSharedPtr<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(FMargin(0.0f, 20.0f))
				.AutoHeight()
				[
					SNew(SHoudiniColorRamp)
						.RampParameters(ColorRampParameters)
						.OnValueCommitted_Lambda(
							[]() { FHoudiniEngineUtils::UpdateEditorProperties(true); })
				]
		];

	AddMultiParamWidgetsToBox(HorizontalBox.ToSharedRef(), ExtraWidgets);


	return HorizontalBox.ToSharedRef();
}

TSharedRef<SWidget>
FHoudiniParameterView::CreateWidgetOperatorPath(
	IDetailLayoutBuilder& DetailBuilder,
	IDetailCategoryBuilder& HouInputCategory,
	const TArray<TWeakObjectPtr<UHoudiniParameterOperatorPath>>& OperatorPathParams,
	const TSharedPtr<SWidget>& ExtraWidgets)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniParameterView::FHoudiniParameterView::CreateWidgetOperatorPath);

	auto& MainParam = OperatorPathParams[0];

	if(!MainParam.IsValid())
		return SNullWidget::NullWidget;

	const TWeakObjectPtr<UHoudiniInput>& MainInput = MainParam->HoudiniInput;

	// Build an array of edited inputs for multi edition
	TArray<TWeakObjectPtr<UHoudiniInput>> EditedInputs;
	EditedInputs.Add(MainInput);

	// Add the corresponding inputs found in the other HAC
	for(int LinkedIdx = 1; LinkedIdx < OperatorPathParams.Num(); LinkedIdx++)
	{
		UHoudiniInput* LinkedInput = OperatorPathParams[LinkedIdx]->HoudiniInput.Get();
		if(!IsValid(LinkedInput))
			continue;

		// Linked params should match the main param! If not try to find one that matches
		if(!LinkedInput->Matches(*MainInput))
			continue;

		EditedInputs.Add(LinkedInput);
	}

	TSharedRef<SWidget> Widget = FHoudiniInputDetails::CreateInputValueWidget(DetailBuilder, HouInputCategory, EditedInputs);

	TSharedPtr<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			Widget
		];

	AddMultiParamWidgetsToBox(HorizontalBox.ToSharedRef(), ExtraWidgets); 

	return HorizontalBox.ToSharedRef();
}

TSharedRef<SWidget>
FHoudiniParameterView::CreateWidgetMultiParm(
	IDetailLayoutBuilder& DetailBuilder,
	TArray<TWeakObjectPtr<UHoudiniParameterMultiParm>>& MultiParmParams,
	const TSharedPtr<SWidget>& ExtraWidgets)
{
	const TWeakObjectPtr<UHoudiniParameterMultiParm>& MainParam = MultiParmParams[0];

	auto OnInstanceValueChangedLambda = [MultiParmParams, &DetailBuilder](int32 InValue, ETextCommit::Type CommitType)
		{
			if(CommitType != ETextCommit::Type::OnEnter && CommitType != ETextCommit::Type::OnUserMovedFocus)
				return;

			if(InValue < 0)
				return;
			
			for(auto& Param : MultiParmParams)
			{
				FHoudiniParameterTranslator::SetNumMultiParmElements(Param.Get(), InValue);
			}
			DetailBuilder.ForceRefreshDetails();
		};

	auto CanEditMultiParms = [MultiParmParams]()
		{
			for(auto& Param : MultiParmParams)
			{
				if(!Param.IsValid())
					return false;

				if(!Param->CanModifyMultiParm())
					return false;
			}
			return true;
		};

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	TSharedPtr<SNumericEntryBox<int32>> NumericEntryBox;

	HorizontalBox->AddSlot().Padding(2, 2, 5, 2)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		.MinWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_MIN_WIDTH)
		.MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		.FillWidth(1.0f)

#else
		.AutoWidth()
#endif
		[
			SAssignNew(NumericEntryBox, SNumericEntryBox< int32 >)
				.MinDesiredValueWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_MIN_WIDTH)

				.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.AllowSpin(true)
				.IsEnabled_Lambda(CanEditMultiParms)
				.OnValueCommitted(SNumericEntryBox<int32>::FOnValueCommitted::CreateLambda([OnInstanceValueChangedLambda](int32 InValue, ETextCommit::Type CommitType) {
				OnInstanceValueChangedLambda(InValue, CommitType);
					}))
				.Value(MainParam->GetInstanceCount())
#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 6)
				.ToolTipTextFormat(FTextFormat::FromString(MULTIPARM_TOOLTIP))
#endif
		];

	HorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
		[
			SNew(SBox)
				.IsEnabled_Lambda(CanEditMultiParms)
				[

				PropertyCustomizationHelpers::MakeAddButton(FSimpleDelegate::CreateLambda([MainParam, MultiParmParams, &DetailBuilder]()
					{
						for(auto& Param : MultiParmParams)
						{
							if(!IsValidWeakPointer(Param))
								continue;

							FHoudiniParameterTranslator::SetNumMultiParmElements(Param.Get(), Param->GetInstanceCount() + 1);
						}
						DetailBuilder.ForceRefreshDetails();
					}),
					LOCTEXT("AddMultiparmInstanceToolTipAddLastInstance", MULTIPARM_APPEND_TOOLTIP), true)
				]
		];

	HorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
		[
			SNew(SBox)
				.IsEnabled_Lambda(CanEditMultiParms)
				[
					// Remove the last multiparm instance
					PropertyCustomizationHelpers::MakeRemoveButton(FSimpleDelegate::CreateLambda([MainParam, MultiParmParams, &DetailBuilder]()
						{

							for(auto& Param : MultiParmParams)
							{
								FHoudiniParameterTranslator::RemoveMultiParmInstance(Param.Get(), Param->GetInstanceCount() - 1);
							}
							DetailBuilder.ForceRefreshDetails();
						}),
						LOCTEXT("RemoveLastMultiParamLastToolTipRemoveLastInstance", MULTIPARM_REMOVE_LAST_TOOLTIP), true)
				]
		];

	HorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
		[
			SNew(SBox)
				.IsEnabled_Lambda(CanEditMultiParms)
				[
					PropertyCustomizationHelpers::MakeEmptyButton(FSimpleDelegate::CreateLambda([MainParam, MultiParmParams, &DetailBuilder]()
						{
							for(auto& Param : MultiParmParams)
							{
								FHoudiniParameterTranslator::SetNumMultiParmElements(Param.Get(), 0);
							}
							DetailBuilder.ForceRefreshDetails();
						}),
						LOCTEXT("HoudiniParameterRemoveAllMultiparmInstancesToolTip", MULTIPARM_REMOVE_ALL_TOOLTIP), true)
				]
		];

	AddMultiParamWidgetsToBox(HorizontalBox, ExtraWidgets);

	return HorizontalBox;
}

TSharedRef<SWidget>
FHoudiniParameterView::CreateWidgetSeparator(
	const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams,
	const TSharedPtr<SWidget>& ExtraWidgets)
{
	TSharedPtr<SWidget> Widget = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(FMargin(0.0f, 20.0f))
		.AutoHeight()
		[
			SNew(SSeparator)
				.SeparatorImage(FAppStyle::GetBrush("Menu.Separator"))
				.Thickness(1.0f)
				.Orientation(Orient_Horizontal)
				.ColorAndOpacity(FLinearColor::White)
		];

	return Widget.ToSharedRef();
}

TSharedRef<SWidget>
FHoudiniParameterView::CreateWidgetChoice(
	const TArray<TWeakObjectPtr<UHoudiniParameterChoice>>& ChoiceParams,
	const TSharedPtr<SWidget>& ExtraWidgets)
{
	const TWeakObjectPtr<UHoudiniParameterChoice>& MainParam = ChoiceParams[0];

	// Lambda for changing the parameter value
	auto ChangeSelectionLambda = [ChoiceParams](TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType)
		{
			if(!NewChoice.IsValid())
				return;

			const int32 NewSelection = ChoiceParams[0]->GetLabels().Find(*NewChoice);
			if (NewSelection == INDEX_NONE)
				return;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniParameterChoiceChange", "Houdini Parameter Choice: Changing selection"),
				ChoiceParams[0]->GetOuter());

			bool bChanged = false;
			for(int Idx = 0; Idx < ChoiceParams.Num(); Idx++)
			{
				if(!IsValidWeakPointer(ChoiceParams[Idx]))
					continue;

				ChoiceParams[Idx]->Modify();
				if(ChoiceParams[Idx]->SetChoiceSelection(NewSelection))
				{
					bChanged = true;
					ChoiceParams[Idx]->MarkChanged(true);
				}
			}

			if(!bChanged)
			{
				// Cancel the transaction if no parameter was changed
				Transaction.Cancel();
			}
		};

	// 
	auto ChoiceLabels = MainParam->GetSharedLabelsList();

	TSharedPtr<FString> InitialSelection;
	if(ChoiceLabels->IsValidIndex(MainParam->GetChoiceSelection()))
	{
		InitialSelection = (*ChoiceLabels)[MainParam->GetChoiceSelection()];
	}

	TSharedRef< SHorizontalBox > HorizontalBox = SNew(SHorizontalBox);
	TSharedPtr< SComboBox< TSharedPtr< FString > > > ComboBox;
	HorizontalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SAssignNew(ComboBox, SComboBox< TSharedPtr< FString > >)
				.OptionsSource(ChoiceLabels)
				.InitiallySelectedItem(InitialSelection)
				.OnGenerateWidget_Lambda(
					[](TSharedPtr< FString > InItem)
					{
						return SNew(STextBlock).Text(FText::FromString(*InItem));
					})
				.OnSelectionChanged_Lambda(
					[ChangeSelectionLambda](TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType)
					{
						ChangeSelectionLambda(NewChoice, SelectType);
					})
				[
					SNew(STextBlock)
						.Text_Lambda([MainParam]() { return FText::FromString(MainParam->GetSelectedItemLabel()); })
						.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
		];

	AddMultiParamWidgetsToBox(HorizontalBox, ExtraWidgets);

	return HorizontalBox;
}

TSharedRef<SWidget>
FHoudiniParameterView::CreateWidgetInt(
	const TArray<TWeakObjectPtr<UHoudiniParameterInt>>& IntParams,
	const TSharedPtr<SWidget>& ExtraWidgets)
{
	const TWeakObjectPtr<UHoudiniParameterInt>& MainParam = IntParams[0];

	// Helper function to find a unit from a string (name or abbreviation) 
	auto ParmUnit = FUnitConversion::UnitFromString(*(MainParam->GetUnit()));
	EUnit Unit = EUnit::Unspecified;
	if(FUnitConversion::Settings().ShouldDisplayUnits() && ParmUnit.IsSet())
		Unit = ParmUnit.GetValue();

	TSharedPtr<INumericTypeInterface<int32>> ParamTypeInterface = MakeShareable(new TNumericUnitTypeInterface<int32>(Unit));

	// Lambda for slider begin
	auto SliderBegin = [](const TArray<TWeakObjectPtr<UHoudiniParameterInt>>& IntParams)
		{
			if(IntParams.Num() == 0)
				return;

			if(!IsValidWeakPointer(IntParams[0]))
				return;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniParameterIntChange", "Houdini Parameter Int: Changing a value"),
				IntParams[0]->GetOuter());

			for(int Idx = 0; Idx < IntParams.Num(); Idx++)
			{
				if(!IsValidWeakPointer(IntParams[Idx]))
					continue;

				IntParams[Idx]->Modify();
			}
		};

	// Lambda for slider end
	auto SliderEnd = [](const TArray<TWeakObjectPtr<UHoudiniParameterInt>>& IntParams)
		{
			// Mark the value as changed to trigger an update
			for(int Idx = 0; Idx < IntParams.Num(); Idx++)
			{
				if(!IsValidWeakPointer(IntParams[Idx]))
					continue;

				IntParams[Idx]->MarkChanged(true);
			}
		};

	// Lambda for changing the parameter value
	auto ChangeIntValueAt = [](const int32& Value, const int32& Index, const bool& DoChange, const TArray<TWeakObjectPtr<UHoudiniParameterInt>>& IntParams)
		{
			if(IntParams.Num() == 0)
				return;

			if(!IsValidWeakPointer(IntParams[0]))
				return;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniParameterIntChange", "Houdini Parameter Int: Changing a value"),
				IntParams[0]->GetOuter());

			bool bChanged = false;
			for(int Idx = 0; Idx < IntParams.Num(); Idx++)
			{
				if(!IsValidWeakPointer(IntParams[Idx]))
					continue;

				IntParams[Idx]->Modify();
				if(IntParams[Idx]->SetValueAt(Value, Index))
				{
					// Only mark the param has changed if DoChange is true!!!
					if(DoChange)
						IntParams[Idx]->MarkChanged(true);
					bChanged = true;
				}
			}

			if(!bChanged || !DoChange)
			{
				// Cancel the transaction if there is no param has actually been changed
				Transaction.Cancel();
			}
		};

	auto RevertToDefault = [](const int32& TupleIndex, const TArray<TWeakObjectPtr<UHoudiniParameterInt>>& IntParams)
		{
			for(int32 Idx = 0; Idx < IntParams.Num(); Idx++)
			{
				if(!IsValidWeakPointer(IntParams[Idx]))
					continue;

				if(IntParams[Idx]->IsDefaultValueAtIndex(TupleIndex))
					continue;

				IntParams[Idx]->RevertToDefault(TupleIndex);
			}

			return FReply::Handled();
		};

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	for(int32 Idx = 0; Idx < MainParam->GetTupleSize(); ++Idx)
	{
		TSharedPtr<SNumericEntryBox<int32>> NumericEntryBox;
		VerticalBox->AddSlot().Padding(2, 2, 5, 2)
			[
				SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
					.FillWidth(1.0)
					[
						SAssignNew(NumericEntryBox, SNumericEntryBox<int32>)
							.MinDesiredValueWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
							.AllowSpin(true)
							.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							.MinValue(MainParam->GetMin())
							.MaxValue(MainParam->GetMax())
							.MinSliderValue(MainParam->GetUIMin())
							.MaxSliderValue(MainParam->GetUIMax())

							.Value(TAttribute<TOptional<int32>>::Create(TAttribute<TOptional<int32>>::FGetter::CreateUObject(MainParam.Get(), &UHoudiniParameterInt::GetValue, Idx)))
							.OnValueChanged_Lambda([=](int32 Val) { ChangeIntValueAt(Val, Idx, false, IntParams); })
							.OnValueCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType) { ChangeIntValueAt(Val, Idx, true, IntParams); })
							.OnBeginSliderMovement_Lambda([=]() { SliderBegin(IntParams); })
							.OnEndSliderMovement_Lambda([=](const float NewValue) { SliderEnd(IntParams); })
							.SliderExponent(MainParam->IsLogarithmic() ? 8.0f : 1.0f)
							.TypeInterface(ParamTypeInterface)
					]
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
							.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
							.ButtonStyle(_GetEditorStyle(), "NoBorder")
							.ContentPadding(0)
							.Visibility_Lambda([Idx, IntParams]()
								{
									for(auto& NextSelectedParam : IntParams)
									{
										if(!IsValidWeakPointer(NextSelectedParam))
											continue;

										if(!NextSelectedParam->IsDefaultValueAtIndex(Idx))
											return EVisibility::Visible;
									}

									return EVisibility::Hidden;
								})
							.OnClicked_Lambda([Idx, IntParams, RevertToDefault]() { return RevertToDefault(Idx, IntParams); })
							[
								SNew(SImage)
									.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
							]
					]
			];
	}

	TSharedRef<SHorizontalBox> Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		.MinWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_MIN_WIDTH)
		.MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		.FillWidth(1.0f)
#else
		.AutoWidth()
#endif
		.MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		.FillWidth(1.0f)
		.Padding(0.0f, 0.0f)
		[
			VerticalBox
		];


	AddMultiParamWidgetsToBox(Widget, ExtraWidgets);

	return Widget;
}

TSharedRef<SWidget>
FHoudiniParameterView::CreateWidgetFloat(
	const TArray<TWeakObjectPtr<UHoudiniParameterFloat>>& FloatParams,
	const TSharedPtr<SWidget>& ExtraWidgets)
{
	const TWeakObjectPtr<UHoudiniParameterFloat>& MainParam = FloatParams[0];

	// Helper function to find a unit from a string (name or abbreviation) 
	auto ParmUnit = FUnitConversion::UnitFromString(*(MainParam->GetUnit()));
	EUnit Unit = EUnit::Unspecified;
	if(FUnitConversion::Settings().ShouldDisplayUnits() && ParmUnit.IsSet())
		Unit = ParmUnit.GetValue();

	TSharedPtr<INumericTypeInterface<float>> ParamTypeInterface = MakeShareable(new TNumericUnitTypeInterface<float>(Unit));

	// Lambdas for slider begin
	auto SliderBegin = [](const TArray<TWeakObjectPtr<UHoudiniParameterFloat>>& FloatParams)
		{
			if(FloatParams.Num() == 0)
				return;

			if(!IsValidWeakPointer(FloatParams[0]))
				return;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniParameterFloatChange", "Houdini Parameter Float: Changing a value"),
				FloatParams[0]->GetOuter());

			for(int Idx = 0; Idx < FloatParams.Num(); Idx++)
			{
				if(!IsValidWeakPointer(FloatParams[Idx]))
					continue;

				FloatParams[Idx]->Modify();
			}
		};

	// Lambdas for slider end
	auto SliderEnd = [](const TArray<TWeakObjectPtr<UHoudiniParameterFloat>>& FloatParams)
		{
			// Mark the value as changed to trigger an update
			for(int Idx = 0; Idx < FloatParams.Num(); Idx++)
			{
				if(!IsValidWeakPointer(FloatParams[Idx]))
					continue;

				FloatParams[Idx]->MarkChanged(true);
			}
		};

	// Lambdas for changing the parameter value
	auto ChangeFloatValueAt = [](const float& Value, const int32& Index, const bool& DoChange, const TArray<TWeakObjectPtr<UHoudiniParameterFloat>>& FloatParams)
		{
			if(FloatParams.Num() == 0)
				return;

			if(!IsValidWeakPointer(FloatParams[0]))
				return;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniParameterFloatChange", "Houdini Parameter Float: Changing a value"),
				FloatParams[0]->GetOuter());

			bool bChanged = false;
			for(int Idx = 0; Idx < FloatParams.Num(); Idx++)
			{
				if(!IsValidWeakPointer(FloatParams[Idx]))
					continue;

				FloatParams[Idx]->Modify();
				if(FloatParams[Idx]->SetValueAt(Value, Index))
				{
					// Only mark the param has changed if DoChange is true!!!
					if(DoChange)
						FloatParams[Idx]->MarkChanged(true);
					bChanged = true;
				}
			}

			if(!bChanged || !DoChange)
			{
				// Cancel the transaction if no parameter's value has actually been changed
				Transaction.Cancel();
			}
		};

	auto RevertToDefault = [](const int32& TupleIndex, const TArray<TWeakObjectPtr<UHoudiniParameterFloat>>& FloatParams)
		{
			if(FloatParams.Num() == 0)
				return FReply::Handled();

			if(!IsValidWeakPointer(FloatParams[0]))
				return FReply::Handled();

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniParameterFloatChange", "Houdini Parameter Float: Revert to default value"),
				FloatParams[0]->GetOuter());

			if(TupleIndex < 0)
			{
				for(int32 Idx = 0; Idx < FloatParams.Num(); Idx++)
				{
					if(!IsValidWeakPointer(FloatParams[Idx]))
						continue;

					if(FloatParams[Idx]->IsDefault())
						continue;

					FloatParams[Idx]->RevertToDefault(-1);
				}
			}
			else
			{
				for(int32 Idx = 0; Idx < FloatParams.Num(); Idx++)
				{
					if(!IsValidWeakPointer(FloatParams[Idx]))
						continue;

					if(FloatParams[Idx]->IsDefaultValueAtIndex(TupleIndex))
						continue;

					FloatParams[Idx]->RevertToDefault(TupleIndex);
				}
			}
			return FReply::Handled();
		};


	TSharedPtr<SWidget> Result;

	if(MainParam->GetTupleSize() == 3)
	{
		TSharedPtr<SVerticalBox> VerticalBox = SNew(SVerticalBox);

		// Should we swap Y and Z fields (only relevant for Vector3)
		// Ignore the swapping if that parameter has the noswap tag
		bool SwapVector3 = !MainParam->GetNoSwap();

		auto ChangeFloatValueUniformly = [FloatParams, ChangeFloatValueAt](const float& Val, const bool& bDoChange)
			{
				ChangeFloatValueAt(Val, 0, bDoChange, FloatParams);
				ChangeFloatValueAt(Val, 1, bDoChange, FloatParams);
				ChangeFloatValueAt(Val, 2, bDoChange, FloatParams);
			};

		VerticalBox->AddSlot()
			.Padding(2, 2, 5, 2)
			[
				SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SVectorInputBox)
							.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							.bColorAxisLabels(true)
							.AllowSpin(true)
							.X(TAttribute<TOptional<float>>::Create(TAttribute<TOptional<float>>::FGetter::CreateUObject(MainParam.Get(), &UHoudiniParameterFloat::GetValue, 0)))
							.Y(TAttribute<TOptional<float>>::Create(TAttribute<TOptional<float>>::FGetter::CreateUObject(MainParam.Get(), &UHoudiniParameterFloat::GetValue, SwapVector3 ? 2 : 1)))
							.Z(TAttribute<TOptional<float>>::Create(TAttribute<TOptional<float>>::FGetter::CreateUObject(MainParam.Get(), &UHoudiniParameterFloat::GetValue, SwapVector3 ? 1 : 2)))
							.OnXCommitted_Lambda([ChangeFloatValueAt, ChangeFloatValueUniformly, FloatParams, MainParam, SwapVector3](float Val, ETextCommit::Type TextCommitType)
								{
									if (!IsValidWeakPointer(MainParam))
										return;

									if(MainParam->IsUniformLocked())
										ChangeFloatValueUniformly(Val, true);
									else
										ChangeFloatValueAt(Val, 0, true, FloatParams);
								})
							.OnYCommitted_Lambda([ChangeFloatValueAt, ChangeFloatValueUniformly, FloatParams, MainParam, SwapVector3](float Val, ETextCommit::Type TextCommitType)
								{
									if (!IsValidWeakPointer(MainParam))
										return;

									if(MainParam->IsUniformLocked())
										ChangeFloatValueUniformly(Val, true);
									else
										ChangeFloatValueAt(Val, SwapVector3 ? 2 : 1, true, FloatParams);
								})
							.OnZCommitted_Lambda([ChangeFloatValueAt, ChangeFloatValueUniformly, FloatParams, MainParam, SwapVector3](float Val, ETextCommit::Type TextCommitType)
								{
									if (!IsValidWeakPointer(MainParam))
										return;

									if(MainParam->IsUniformLocked())
										ChangeFloatValueUniformly(Val, true);
									else
										ChangeFloatValueAt(Val, SwapVector3 ? 1 : 2, true, FloatParams);
								})
							.OnXChanged_Lambda([ChangeFloatValueAt, ChangeFloatValueUniformly, FloatParams, MainParam, SwapVector3](float Val)
								{
									if (!IsValidWeakPointer(MainParam))
										return;

									if(MainParam->IsUniformLocked())
										ChangeFloatValueUniformly(Val, false);
									else
										ChangeFloatValueAt(Val, 0, false, FloatParams);
								})
							.OnYChanged_Lambda([ChangeFloatValueAt, ChangeFloatValueUniformly, FloatParams, MainParam, SwapVector3](float Val)
								{
									if (!IsValidWeakPointer(MainParam))
										return;

									if(MainParam->IsUniformLocked())
										ChangeFloatValueUniformly(Val, false);
									else
										ChangeFloatValueAt(Val, SwapVector3 ? 2 : 1, false, FloatParams);
								})
							.OnZChanged_Lambda([ChangeFloatValueAt, ChangeFloatValueUniformly, FloatParams, MainParam, SwapVector3](float Val)
								{
									if (!IsValidWeakPointer(MainParam))
										return;

									if(MainParam->IsUniformLocked())
										ChangeFloatValueUniformly(Val, false);
									else
										ChangeFloatValueAt(Val, SwapVector3 ? 1 : 2, false, FloatParams);
								})
							.OnBeginSliderMovement_Lambda([SliderBegin, FloatParams]() { SliderBegin(FloatParams); })
							.OnEndSliderMovement_Lambda([SliderEnd, FloatParams](const float NewValue) { SliderEnd(FloatParams); })
							.TypeInterface(ParamTypeInterface)
					]
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Center)
							[
								SNew(SButton)
									.ButtonStyle(_GetEditorStyle(), "NoBorder")
									.ClickMethod(EButtonClickMethod::MouseDown)
									.ToolTipText(LOCTEXT("FloatParameterLockButtonToolTip", "When locked, change the vector value uniformly."))
									.Visibility(EVisibility::Visible)
									[
										SNew(SImage)
											.Image(MainParam->IsUniformLocked() ? _GetEditorStyle().GetBrush("Icons.Lock") : _GetEditorStyle().GetBrush("Icons.Unlock"))
									]
									.OnClicked_Lambda([FloatParams, MainParam]()
										{
											if(!IsValidWeakPointer(MainParam))
												return FReply::Handled();

											for(auto& CurParam : FloatParams)
											{
												if(!IsValidWeakPointer(CurParam))
													continue;

												CurParam->SwitchUniformLock();
											}

											FHoudiniEngineUtils::UpdateEditorProperties(true);

											return FReply::Handled();
										})
							]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							[
								SNew(SButton)
									.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
									.ButtonStyle(_GetEditorStyle(), "NoBorder")
									.ContentPadding(0)
									.Visibility_Lambda([FloatParams]()
										{
											for(auto& SelectedParam : FloatParams)
											{
												if(!IsValidWeakPointer(SelectedParam))
													continue;

												if(!SelectedParam->IsDefault())
													return EVisibility::Visible;
											}

											return EVisibility::Hidden;
										})
									.OnClicked_Lambda([FloatParams, RevertToDefault]() { return RevertToDefault(-1, FloatParams); })
									[
										SNew(SImage)
											.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
									]
							]
					]
			];

		Result = VerticalBox;
	}
	else if(MainParam->GetTupleSize() == 2)
	{
		TSharedPtr<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

		for(int32 Idx = 0; Idx < MainParam->GetTupleSize(); ++Idx)
		{
			TSharedPtr<SNumericEntryBox<float>> NumericEntryBox;
			HorizontalBox->AddSlot()
				.Padding(2, 2, 5, 2)
				[
					SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
						.FillWidth(1.0)
						[

							SAssignNew(NumericEntryBox, SNumericEntryBox< float >)
								.MinDesiredValueWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
								.AllowSpin(true)
								.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
								.MinValue(MainParam->GetMin())
								.MaxValue(MainParam->GetMax())
								.MinSliderValue(MainParam->GetUIMin())
								.MaxSliderValue(MainParam->GetUIMax())
								.Value(TAttribute<TOptional<float>>::Create(TAttribute<TOptional<float>>::FGetter::CreateUObject(MainParam.Get(), &UHoudiniParameterFloat::GetValue, Idx)))
								.OnValueChanged_Lambda([ChangeFloatValueAt, Idx, FloatParams](float Val) { ChangeFloatValueAt(Val, Idx, false, FloatParams); })
								.OnValueCommitted_Lambda([ChangeFloatValueAt, Idx, FloatParams](float Val, ETextCommit::Type TextCommitType) {	ChangeFloatValueAt(Val, Idx, true, FloatParams); })
								.OnBeginSliderMovement_Lambda([SliderBegin, FloatParams]() { SliderBegin(FloatParams); })
								.OnEndSliderMovement_Lambda([SliderEnd, FloatParams](const float NewValue) { SliderEnd(FloatParams); })
								.SliderExponent(MainParam->IsLogarithmic() ? 8.0f : 1.0f)
								.TypeInterface(ParamTypeInterface)
						]
					+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2.0f, 0.0f)
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
								.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
								.ButtonStyle(_GetEditorStyle(), "NoBorder")
								.ContentPadding(0)
								.OnClicked_Lambda([Idx, FloatParams, RevertToDefault]() { return RevertToDefault(Idx, FloatParams); })
								.Visibility_Lambda([Idx, FloatParams]()
									{
										for(auto& SelectedParam : FloatParams)
										{
											if(!IsValidWeakPointer(SelectedParam))
												continue;

											if(!SelectedParam->IsDefaultValueAtIndex(Idx))
												return EVisibility::Visible;
										}

										return EVisibility::Hidden;
									})
								[
									SNew(SImage)
										.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
								]
						]
				];
		}
		Result = HorizontalBox;
	}
	else
	{
		TSharedPtr<SVerticalBox> VerticalBox = SNew(SVerticalBox);

		for(int32 Idx = 0; Idx < MainParam->GetTupleSize(); ++Idx)
		{
			TSharedPtr<SNumericEntryBox<float>> NumericEntryBox;
			VerticalBox->AddSlot()
				.Padding(2, 2, 5, 2)
				[
					SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
						.MinWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_MIN_WIDTH)
						.MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
						.FillWidth(1.0f)
#else
						.AutoWidth()
#endif
						.MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
						.FillWidth(1.0f)
						[

							SAssignNew(NumericEntryBox, SNumericEntryBox< float >)
								.MinDesiredValueWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_MIN_WIDTH)
								.AllowSpin(true)
								.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
								.MinValue(MainParam->GetMin())
								.MaxValue(MainParam->GetMax())
								.MinSliderValue(MainParam->GetUIMin())
								.MaxSliderValue(MainParam->GetUIMax())
								.Value(TAttribute<TOptional<float>>::Create(TAttribute<TOptional<float>>::FGetter::CreateUObject(MainParam.Get(), &UHoudiniParameterFloat::GetValue, Idx)))
								.OnValueChanged_Lambda([ChangeFloatValueAt, Idx, FloatParams](float Val) { ChangeFloatValueAt(Val, Idx, false, FloatParams); })
								.OnValueCommitted_Lambda([ChangeFloatValueAt, Idx, FloatParams](float Val, ETextCommit::Type TextCommitType) {	ChangeFloatValueAt(Val, Idx, true, FloatParams); })
								.OnBeginSliderMovement_Lambda([SliderBegin, FloatParams]() { SliderBegin(FloatParams); })
								.OnEndSliderMovement_Lambda([SliderEnd, FloatParams](const float NewValue) { SliderEnd(FloatParams); })
								.SliderExponent(MainParam->IsLogarithmic() ? 8.0f : 1.0f)
								.TypeInterface(ParamTypeInterface)
						]
					+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2.0f, 0.0f)
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
								.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
								.ButtonStyle(_GetEditorStyle(), "NoBorder")
								.ContentPadding(0)
								.OnClicked_Lambda([Idx, FloatParams, RevertToDefault]() { return RevertToDefault(Idx, FloatParams); })
								.Visibility_Lambda([Idx, FloatParams]()
									{
										for(auto& SelectedParam : FloatParams)
										{
											if(!IsValidWeakPointer(SelectedParam))
												continue;

											if(!SelectedParam->IsDefaultValueAtIndex(Idx))
												return EVisibility::Visible;
										}

										return EVisibility::Hidden;
									})
								[
									SNew(SImage)
										.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
								]
						]
				];
		}
		Result = VerticalBox;
	}

	TSharedRef<SHorizontalBox> Widget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		.MinWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_MIN_WIDTH)
		.MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		.FillWidth(1.0f)
#else
		.AutoWidth()
#endif
		.MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		.FillWidth(1.0f)
		.Padding(0.0f, 0.0f)
		[
			Result.ToSharedRef()
		];

	AddMultiParamWidgetsToBox(Widget, ExtraWidgets);

	return Widget;
}

FText
FHoudiniParameterView::GetParameterTooltip(const UHoudiniParameter* InParam)
{
	// Tooltip starts with Label (name)
	FString Tooltip = InParam->GetParameterLabel() + TEXT(" (") + InParam->GetParameterName() + TEXT(")");

	// Append the parameter type
	FString ParmTypeStr = GetParameterTypeString(InParam->GetParameterType(), InParam->GetTupleSize());
	if(!ParmTypeStr.IsEmpty())
		Tooltip += TEXT("\n") + ParmTypeStr;

	// If the parameter has some help, append it
	FString Help = InParam->GetParameterHelp();
	if(!Help.IsEmpty())
		Tooltip += TEXT("\n") + Help;

	// If the parameter has an expression, append it
	if(InParam->HasExpression())
	{
		FString Expr = InParam->GetExpression();
		if(!Expr.IsEmpty())
			Tooltip += TEXT("\nExpression: ") + Expr;
	}

	return FText::FromString(Tooltip);
}

FString
FHoudiniParameterView::GetParameterTypeString(
	const EHoudiniParameterType InType,
	const int32 InTupleSize)
{
	FString ParamStr;

	switch(InType)
	{
	case EHoudiniParameterType::Button:
		ParamStr = TEXT("Button");
		break;

	case EHoudiniParameterType::ButtonStrip:
		ParamStr = TEXT("Button Strip");
		break;

	case EHoudiniParameterType::Color:
	{
		if(InTupleSize == 4)
			ParamStr = TEXT("Color with Alpha");
		else
			ParamStr = TEXT("Color");
	}
	break;

	case EHoudiniParameterType::ColorRamp:
		ParamStr = TEXT("Color Ramp");
		break;

	case EHoudiniParameterType::File:
		ParamStr = TEXT("File (") + FString::FromInt(InTupleSize) + TEXT(" tuple)");
		break;

	case EHoudiniParameterType::FileDir:
		ParamStr = TEXT("File Dir (") + FString::FromInt(InTupleSize) + TEXT(" tuple)");
		break;

	case EHoudiniParameterType::FileGeo:
		ParamStr = TEXT("File Geo (") + FString::FromInt(InTupleSize) + TEXT(" tuple)");
		break;

	case EHoudiniParameterType::FileImage:
		ParamStr = TEXT("File Image (") + FString::FromInt(InTupleSize) + TEXT(" tuple)");
		break;

	case EHoudiniParameterType::Float:
		ParamStr = TEXT("Float (VEC") + FString::FromInt(InTupleSize) + TEXT(")");
		break;

	case EHoudiniParameterType::FloatRamp:
		ParamStr = TEXT("Float Ramp");
		break;

	case EHoudiniParameterType::Folder:
	case EHoudiniParameterType::FolderList:
		break;

	case EHoudiniParameterType::Input:
		ParamStr = TEXT("Opearator Path");
		break;

	case EHoudiniParameterType::Int:
		ParamStr = TEXT("Integer (VEC") + FString::FromInt(InTupleSize) + TEXT(")");
		break;

	case EHoudiniParameterType::IntChoice:
		ParamStr = TEXT("Int Choice");
		break;

	case EHoudiniParameterType::Label:
		ParamStr = TEXT("Label (") + FString::FromInt(InTupleSize) + TEXT(" tuple)");
		break;

	case EHoudiniParameterType::MultiParm:
		ParamStr = TEXT("MultiParm");
		break;

	case EHoudiniParameterType::Separator:
		break;

	case EHoudiniParameterType::String:
		ParamStr = TEXT("String (") + FString::FromInt(InTupleSize) + TEXT(" tuple)");
		break;

	case EHoudiniParameterType::StringAssetRef:
		ParamStr = TEXT("String Asset Ref (") + FString::FromInt(InTupleSize) + TEXT(" tuple)");
		break;

	case EHoudiniParameterType::StringChoice:
		ParamStr = TEXT("String Choice");
		break;

	case EHoudiniParameterType::Toggle:
		ParamStr = TEXT("Toggle (") + FString::FromInt(InTupleSize) + TEXT(" tuple)");
		break;

	default:
		ParamStr = TEXT("invalid parameter type");
		break;
	}

	return ParamStr;
}

TSharedPtr<SWidget> FHoudiniParameterView::CreateMultiParmWidgets(
	IDetailLayoutBuilder& DetailBuilder,
	const TArray<TWeakObjectPtr<UHoudiniParameterMultiParm>>& ParentMultiParams,
	int InstanceIndex)
{
	if(InstanceIndex == INDEX_NONE)
		return SNullWidget::NullWidget;

	auto CanEditMultiParms = [ParentMultiParams]()
		{
			for(auto& Param : ParentMultiParams)
			{
				if(!Param.IsValid())
					return false;

				if(!Param->CanModifyMultiParm())
					return false;
			}
			return true;
		};

	TSharedRef<SWidget> AddButton = PropertyCustomizationHelpers::MakeAddButton(FSimpleDelegate::CreateLambda([ParentMultiParams, InstanceIndex, &DetailBuilder]()
		{
			for(auto& ParentParam : ParentMultiParams)
			{
				if(!IsValidWeakPointer(ParentParam))
					continue;

				FHoudiniParameterTranslator::InsertMultiParmInstance(ParentParam.Get(), InstanceIndex);
			}
			DetailBuilder.ForceRefreshDetails();

		}),
		LOCTEXT("HoudiniParameterMultiParamAddBeforeCurrentInstanceToolTip", "Insert an instance before this instance.\nThis requires the HDA to be cooked in an active Houdini session."));

	TSharedRef<SWidget> RemoveButton = PropertyCustomizationHelpers::MakeRemoveButton(FSimpleDelegate::CreateLambda([ParentMultiParams, InstanceIndex, &DetailBuilder]()
		{
			for(auto& ParentParam : ParentMultiParams)
			{
				if(!IsValidWeakPointer(ParentParam))
					continue;
				FHoudiniParameterTranslator::RemoveMultiParmInstance(ParentParam.Get(), InstanceIndex);
			}
			DetailBuilder.ForceRefreshDetails();

		}),
		LOCTEXT("HoudiniParameterMultiParamDeleteCurrentInstanceToolTip", "Remove an instance.\nThis requires the HDA to be cooked in an active Houdini session."), true);

	TSharedRef<SWidget> IndentText = SNew(STextBlock)
		.TextStyle(FAppStyle::Get(), "SmallText")
		.Text(FText::FromString(FString::Printf(TEXT("(%d)"), InstanceIndex)));

	TSharedPtr<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f)
		[
			IndentText
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f)
		[
			SNew(SBox)
				.IsEnabled_Lambda(CanEditMultiParms)
				[
					AddButton
				]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f)
		[
			SNew(SBox)
				.IsEnabled_Lambda(CanEditMultiParms)
				[
					RemoveButton
				]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SNew(SSpacer)
		];

	return HorizontalBox;
}

TSharedRef<SWidget> FHoudiniParameterView::CreateWidgetToggle(
	const TArray<TWeakObjectPtr<UHoudiniParameterToggle>>& ToggleParams,
	const TSharedPtr<SWidget>& ExtraWidgets)
{
	const TWeakObjectPtr<UHoudiniParameterToggle>& MainParam = ToggleParams[0];

	FText ParameterLabelText = FText::FromString(MainParam->GetParameterLabel());

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	auto IsToggleCheckedLambda = [MainParam](int32 Index)
		{
			if(!IsValidWeakPointer(MainParam))
				return ECheckBoxState::Unchecked;

			if(Index >= MainParam->GetNumValues())
				return ECheckBoxState::Unchecked;

			if(MainParam->GetValueAt(Index))
				return ECheckBoxState::Checked;

			return ECheckBoxState::Unchecked;
		};

	auto OnToggleCheckStateChanged = [MainParam, ToggleParams](ECheckBoxState NewState, int32 Index)
		{
			if(!IsValidWeakPointer(MainParam))
				return;

			if(Index >= MainParam->GetNumValues())
				return;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniParameterToggleChange", "Houdini Parameter Toggle: Changing value"),
				MainParam->GetOuter(), true);

			bool bState = (NewState == ECheckBoxState::Checked);

			bool bChanged = false;
			for(auto& Param : ToggleParams)
			{
				if(!IsValidWeakPointer(Param))
					continue;

				Param->Modify();
				if(Param->SetValueAt(bState, Index))
				{
					bChanged = true;
					Param->MarkChanged(true);
				}
			}

			// Cancel the transaction if no parameter has actually been changed
			if(!bChanged)
			{
				Transaction.Cancel();
			}
		};

	for(int32 Index = 0; Index < MainParam->GetTupleSize(); ++Index)
	{
		TSharedPtr< SCheckBox > CheckBox;
		VerticalBox->AddSlot()
			.Padding(2, 2, 5, 2)
			[
				SAssignNew(CheckBox, SCheckBox)
					.OnCheckStateChanged_Lambda([OnToggleCheckStateChanged, Index](ECheckBoxState NewState) {
					OnToggleCheckStateChanged(NewState, Index);

						})
					.IsChecked_Lambda([IsToggleCheckedLambda, Index]() {
					return IsToggleCheckedLambda(Index);
						})
			];
	}

	TSharedRef<SHorizontalBox> HorizontalBox =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			VerticalBox
		];

	AddMultiParamWidgetsToBox(HorizontalBox, ExtraWidgets);

	return HorizontalBox;
}

TSharedRef<SWidget> FHoudiniParameterView::CreateWidgetColor(
	const TArray<TWeakObjectPtr<UHoudiniParameterColor>>& ColorParams,
	const TSharedPtr<SWidget>& ExtraWidgets)
{
	const TWeakObjectPtr<UHoudiniParameterColor>& MainParam = ColorParams[0];

	bool bHasAlpha = (MainParam->GetTupleSize() == 4);

	// Add color picker UI.
	TSharedPtr<SColorBlock> ColorBlock;
	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	VerticalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SAssignNew(ColorBlock, SColorBlock)
				.Color(MainParam->GetColorValue())
				.ShowBackgroundForAlpha(bHasAlpha)
				.OnMouseButtonDown_Lambda([ColorParams, MainParam, bHasAlpha](const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
					{
						if (!IsValidWeakPointer(MainParam))
							return FReply::Handled();

						FColorPickerArgs PickerArgs;
						PickerArgs.ParentWidget = FSlateApplication::Get().GetActiveTopLevelWindow();
						PickerArgs.bUseAlpha = bHasAlpha;
						PickerArgs.DisplayGamma = TAttribute< float >::Create(
							TAttribute< float >::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
						PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([MainParam, ColorParams](FLinearColor InColor)
							{
								if (!IsValidWeakPointer(MainParam))
									return;

								FScopedTransaction Transaction(
									TEXT(HOUDINI_MODULE_RUNTIME),
									LOCTEXT("HoudiniParameterColorChange", "Houdini Parameter Color: Changing value"),
									MainParam->GetOuter(), true);

								bool bChanged = false;
								for(auto& Param : ColorParams)
								{
									if(!IsValidWeakPointer(Param))
										continue;

									Param->Modify();
									if(Param->SetColorValue(InColor))
									{
										Param->MarkChanged(true);
										bChanged = true;
									}
								}

								// cancel the transaction if there is actually no value changed
								if(!bChanged)
								{
									Transaction.Cancel();
								}
							});
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
						PickerArgs.InitialColor = MainParam->GetColorValue();
#else
						PickerArgs.InitialColorOverride = MainParam->GetColorValue();
#endif
						PickerArgs.bOnlyRefreshOnOk = true;
						OpenColorPicker(PickerArgs);
						return FReply::Handled();
					})
		];

	TSharedRef<SHorizontalBox> HorizontalBox =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			VerticalBox
		];

	AddMultiParamWidgetsToBox(HorizontalBox, ExtraWidgets);

	return HorizontalBox;
}

template< class T >
TArray<TWeakObjectPtr<T>> FHoudiniParameterView::CastParameters(const TArray<TWeakObjectPtr<UHoudiniParameter>>& InParams)
{
	TArray<TWeakObjectPtr<T>> TypedParams;

	for(const auto& CurrentParam : InParams)
	{
		if(!IsValidWeakPointer(CurrentParam))
			continue;

		T* CastedParam = Cast<T>(CurrentParam.Get());
		if(IsValid(CastedParam))
			TypedParams.Add(CastedParam);
	}

	return TypedParams;
}


TSharedRef<SWidget>
FHoudiniParameterView::CreateWidgetString(
	IDetailCategoryBuilder& HouParameterCategory,
	const TArray<TWeakObjectPtr<UHoudiniParameterString>>& StringParams,
	const TSharedPtr<SWidget>& ExtraWidgets)
{
	bool bIsMultiLine = false;
	bool bIsUnrealRef = false;
	UClass* UnrealRefClass = UObject::StaticClass();

	TSharedRef< SVerticalBox > VerticalBox = SNew(SVerticalBox);
	const TWeakObjectPtr<UHoudiniParameterString>& MainParam = StringParams[0];

	TMap<FString, FString>& Tags = MainParam->GetTags();
	if(Tags.Contains(HOUDINI_PARAMETER_STRING_REF_TAG) && FCString::Atoi(*Tags[HOUDINI_PARAMETER_STRING_REF_TAG]) == 1)
	{
		bIsUnrealRef = true;

		if(Tags.Contains(HOUDINI_PARAMETER_STRING_REF_CLASS_TAG))
		{
			UClass* FoundClass = FHoudiniEngineRuntimeUtils::GetClassByName(Tags[HOUDINI_PARAMETER_STRING_REF_CLASS_TAG]);
			if(FoundClass != nullptr)
			{
				UnrealRefClass = FoundClass;
			}
		}
	}

	if(Tags.Contains(HOUDINI_PARAMETER_STRING_MULTILINE_TAG))
	{
		bIsMultiLine = true;
	}

	for(int32 Idx = 0; Idx < MainParam->GetTupleSize(); ++Idx)
	{
		// Lambda for changing the parameter value
		auto ChangeStringValueAt = [](const FString& Value, UObject* ChosenObj, const int32& Index, const bool& DoChange, const TArray<TWeakObjectPtr<UHoudiniParameterString>>& StringParams)
			{
				if(StringParams.Num() == 0)
					return;

				if(!IsValidWeakPointer(StringParams[0]))
					return;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_RUNTIME),
					LOCTEXT("HoudiniParameterSrtingChange", "Houdini Parameter String: Changing a value"),
					StringParams[0]->GetOuter());

				bool bChanged = false;
				for(int Idx = 0; Idx < StringParams.Num(); Idx++)
				{
					if(!IsValidWeakPointer(StringParams[Idx]))
						continue;

					StringParams[Idx]->Modify();
					if(StringParams[Idx]->SetValueAt(Value, Index))
					{
						StringParams[Idx]->MarkChanged(true);
						bChanged = true;
					}

					StringParams[Idx]->SetAssetAt(ChosenObj, Index);
				}

				if(!bChanged || !DoChange)
				{
					// Cancel the transaction if there is no param actually has been changed
					Transaction.Cancel();
				}

				FHoudiniEngineUtils::UpdateEditorProperties(false);
			};

		auto RevertToDefault = [](const int32& TupleIndex, const TArray<TWeakObjectPtr<UHoudiniParameterString>>& StringParams)
			{
				for(int32 Idx = 0; Idx < StringParams.Num(); Idx++)
				{
					if(!IsValidWeakPointer(StringParams[Idx]))
						continue;

					if(StringParams[Idx]->IsDefaultValueAtIndex(TupleIndex))
						continue;

					StringParams[Idx]->RevertToDefault(TupleIndex);
				}

				return FReply::Handled();
			};

		if(bIsUnrealRef)
		{
			TArray<const UClass*> AllowedClasses;
			if(UnrealRefClass != UObject::StaticClass())
			{
				// Use the class specified by the user
				AllowedClasses.Add(UnrealRefClass);
			}
			else
			{
				// Using UObject would list way too many assets, and take a long time to open the menu,
				// so we need to reestrict the classes a bit
				AllowedClasses.Add(UStaticMesh::StaticClass());
				AllowedClasses.Add(UHoudiniAsset::StaticClass());
				AllowedClasses.Add(USkeletalMesh::StaticClass());
				AllowedClasses.Add(UBlueprint::StaticClass());
				AllowedClasses.Add(UMaterialInterface::StaticClass());
				AllowedClasses.Add(UTexture::StaticClass());
				AllowedClasses.Add(ULevel::StaticClass());
				AllowedClasses.Add(UStreamableRenderAsset::StaticClass());
				AllowedClasses.Add(USoundBase::StaticClass());
				AllowedClasses.Add(UParticleSystem::StaticClass());
				AllowedClasses.Add(UFoliageType::StaticClass());
			}

			TSharedPtr<SEditableTextBox> EditableTextBox;
			TSharedPtr<SHorizontalBox> HorizontalBox;
			VerticalBox->AddSlot()
				.Padding(2, 2, 5, 2)
				.AutoHeight()
				[
					SNew(SAssetDropTarget)
						.OnAreAssetsAcceptableForDrop_Lambda([UnrealRefClass](TArrayView<FAssetData> InAssets)
							{
								return InAssets[0].GetAsset()->IsA(UnrealRefClass);
							})
						.OnAssetsDropped_Lambda([=](const FDragDropEvent&, TArrayView<FAssetData> InAssets)
							{
								// Get the asset reference string for this object
								UObject* InObject = InAssets[0].GetAsset();
								FString ReferenceStr = UHoudiniParameterString::GetAssetReference(InObject);
								ChangeStringValueAt(ReferenceStr, InObject, Idx, true, StringParams);
							})
						[
							SAssignNew(HorizontalBox, SHorizontalBox)
						]
				];

			// Thumbnail
			// Get thumbnail pool for this builder.
			TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = HouParameterCategory.GetParentLayout().GetThumbnailPool();

			// Create a thumbnail for the selected object / class
			UObject* EditObject = nullptr;
			const FString AssetPath = MainParam->GetValueAt(Idx);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
			EditObject = StaticFindObject(nullptr, nullptr, *AssetPath, EFindObjectFlags::ExactClass);
#else
			EditObject = StaticFindObject(nullptr, nullptr, *AssetPath, true);
#endif

			FAssetData AssetData;
			if(IsValid(EditObject))
			{
				AssetData = FAssetData(EditObject);
			}
			else
			{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
				AssetData.AssetClassPath = UnrealRefClass->GetClassPathName();
#else
				AssetData.AssetClass = UnrealRefClass->GetFName();
#endif
			}

			constexpr int32 ThumbnailSize = 46;

			TSharedPtr< FAssetThumbnail > StaticMeshThumbnail = MakeShareable(
				new FAssetThumbnail(AssetData, ThumbnailSize, ThumbnailSize, AssetThumbnailPool));

			TSharedPtr<SBorder> ThumbnailBorder;
			HorizontalBox->AddSlot()
				.Padding(0, 3, 5, 0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
						.Visibility(EVisibility::SelfHitTestInvisible)
						.Padding(FMargin(0, 0, 4, 4))
						.BorderImage(FAppStyle::Get().GetBrush("PropertyEditor.AssetTileItem.DropShadow"))
						[
							SNew(SOverlay)
								+ SOverlay::Slot()
								.Padding(1)
								[
									SAssignNew(ThumbnailBorder, SBorder)
										.Padding(0)
										.OnMouseDoubleClick_Lambda(
											[EditObject, Idx](const FGeometry&, const FPointerEvent&)
											{
												if(EditObject && GEditor)
													GEditor->EditObject(EditObject);

												return FReply::Handled();
											})
										[
											SNew(SBox)
												.WidthOverride(ThumbnailSize)
												.HeightOverride(ThumbnailSize)
												[
													StaticMeshThumbnail->MakeThumbnailWidget()
												]
										]
								]
							+ SOverlay::Slot()
								[
									SNew(SImage)
										.Image(TAttribute<const FSlateBrush*>::Create(
											TAttribute<const FSlateBrush*>::FGetter::CreateLambda(
												[WeakThumbnailBorder = TWeakPtr<SBorder>(ThumbnailBorder)]()
												{
													TSharedPtr<SBorder> ThumbnailBorderPtr = WeakThumbnailBorder.Pin();
													if(ThumbnailBorderPtr.IsValid() && ThumbnailBorderPtr->IsHovered())
														return _GetEditorStyle().GetBrush(
															"PropertyEditor.AssetThumbnailBorderHovered");
													else
														return _GetEditorStyle().GetBrush(
															"PropertyEditor.AssetThumbnailBorder");
												}
											)
										))
										.Visibility(EVisibility::SelfHitTestInvisible)
								]
						]
				];

			FText MeshNameText = FText::GetEmpty();
			if(EditObject)
				MeshNameText = FText::FromString(EditObject->GetName());

			TSharedPtr<SComboButton> StaticMeshComboButton;

			TSharedPtr<SHorizontalBox> ButtonBox;
			HorizontalBox->AddSlot()
				.Padding(0.0f, 4.0f, 4.0f, 4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoHeight()
						[
							SAssignNew(StaticMeshComboButton, SComboButton)
								.ButtonContent()
								[
									SNew(STextBlock)
										.Font(_GetEditorStyle().GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
										.Text(FText::FromName(AssetData.AssetName))
										.ToolTipText(FText::FromString(MainParam->GetValueAt(Idx)))
								]
						]
					+ SVerticalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoHeight()
						[
							SAssignNew(ButtonBox, SHorizontalBox)
						]
				];

			// Create tooltip.
			FFormatNamedArguments Args;
			Args.Add(TEXT("Asset"), MeshNameText);
			FText StaticMeshTooltip = FText::Format(
				LOCTEXT(
					"BrowseToSpecificAssetInContentBrowser",
					"Browse to '{Asset}' in the content browser."),
				Args);

			// Button : Use selected in content browser
			ButtonBox->AddSlot()
				.AutoWidth()
				.Padding(1, 0, 3, 0)
				.VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeUseSelectedButton(
						FSimpleDelegate::CreateLambda(
							[AllowedClasses, ChangeStringValueAt, Idx, StringParams]()
							{
								if(GEditor)
								{
									TArray<FAssetData> CBSelections;
									GEditor->GetContentBrowserSelections(CBSelections);

									if(CBSelections.IsEmpty())
									{
										return;
									}

									UObject* Object = CBSelections[0].GetAsset();

									if(!IsValid(Object))
									{
										return;
									}

									FString ReferenceStr = UHoudiniParameterString::GetAssetReference(Object);

									ChangeStringValueAt(ReferenceStr, Object, Idx, true, StringParams);
								}
							}),
						TAttribute<FText>(LOCTEXT(
							"GeometryInputUseSelectedAssetFromCB",
							"Use the currently selected asset from the content browser.")))
				];

			// Button : Browse Static Mesh
			ButtonBox->AddSlot()
				.AutoWidth()
				.Padding(1, 0, 3, 0)
				.VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeBrowseButton(
						FSimpleDelegate::CreateLambda([EditObject]()
							{
								if(GEditor && EditObject)
								{
									TArray<UObject*> Objects;
									Objects.Add(EditObject);
									GEditor->SyncBrowserToObjects(Objects);
								}
							}),
						TAttribute<FText>(StaticMeshTooltip))
				];
			TWeakPtr<SComboButton> WeakStaticMeshComboButton(StaticMeshComboButton);
			StaticMeshComboButton->SetOnGetMenuContent(FOnGetContent::CreateLambda(
				[AllowedClasses, WeakStaticMeshComboButton, ChangeStringValueAt, Idx, StringParams]()
				{
					TArray<UFactory*> NewAssetFactories;
					return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
						FAssetData(nullptr),
						true,
						AllowedClasses,
						NewAssetFactories,
						FOnShouldFilterAsset(),
						FOnAssetSelected::CreateLambda(
							[WeakStaticMeshComboButton, ChangeStringValueAt, Idx, StringParams](const FAssetData& AssetData)
							{
								TSharedPtr<SComboButton> StaticMeshComboButtonPtr = WeakStaticMeshComboButton.Pin();
								if(StaticMeshComboButtonPtr.IsValid())
								{
									StaticMeshComboButtonPtr->SetIsOpen(false);

									UObject* Object = AssetData.GetAsset();
									// Get the asset reference string for this object
									// !! Accept null objects to allow clearing the asset picker !!
									FString ReferenceStr = UHoudiniParameterString::GetAssetReference(Object);

									ChangeStringValueAt(ReferenceStr, Object, Idx, true, StringParams);
								}
							}
						),
						FSimpleDelegate::CreateLambda([]() {}));
				})
			);
		}
		else if(bIsMultiLine)
		{
			TSharedPtr< SMultiLineEditableTextBox > MultiLineEditableTextBox;
			VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
				[
					SNew(SAssetDropTarget)
						.OnAreAssetsAcceptableForDrop_Lambda([](TArrayView<FAssetData> InAssets)
							{return true;})
						.OnAssetsDropped_Lambda([=](const FDragDropEvent&, TArrayView<FAssetData> InAssets)
							{
								// Get the asset reference string for this object
								FString ReferenceStr = UHoudiniParameterString::GetAssetReference(InAssets[0].GetAsset());

								FString NewString = ReferenceStr;
								if(StringParams[0]->GetValueAt(Idx).Len() > 0)
									NewString = StringParams[0]->GetValueAt(Idx) + "\n" + NewString;

								ChangeStringValueAt(NewString, nullptr, Idx, true, StringParams);
							})
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top).MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
								[
									SAssignNew(MultiLineEditableTextBox, SMultiLineEditableTextBox)
										.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
										.Text(FText::FromString(MainParam->GetValueAt(Idx)))
										.OnTextCommitted_Lambda([=](const FText& Val, ETextCommit::Type TextCommitType) { ChangeStringValueAt(Val.ToString(), nullptr, Idx, true, StringParams); })
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(2.0f, 0.0f)
								.VAlign(VAlign_Center)
								[
									SNew(SButton)
										.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
										.ButtonStyle(_GetEditorStyle(), "NoBorder")
										.ContentPadding(0)
										.Visibility_Lambda([Idx, StringParams]()
											{
												for(auto& NextSelectedParam : StringParams)
												{
													if(!IsValidWeakPointer(NextSelectedParam))
														continue;

													if(!NextSelectedParam->IsDefaultValueAtIndex(Idx))
														return EVisibility::Visible;
												}

												return EVisibility::Hidden;
											})
										.OnClicked_Lambda([Idx, StringParams, RevertToDefault]() { return RevertToDefault(Idx, StringParams); })
										[
											SNew(SImage)
												.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
										]
								]
						]
				];
		}
		else
		{
			TSharedPtr<SEditableTextBox> EditableTextBox;


			TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
				.MinWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_MIN_WIDTH)
				.MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
				.FillWidth(1.0f)
#else
				.AutoWidth()
#endif
				.MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
				.FillWidth(1.0f)
				[
					SAssignNew(EditableTextBox, SEditableTextBox)
						.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
						.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.Text(FText::FromString(MainParam->GetValueAt(Idx)))
						.OnTextCommitted_Lambda([=](const FText& Val, ETextCommit::Type TextCommitType)
							{
								ChangeStringValueAt(Val.ToString(), nullptr, Idx, true, StringParams);
							})
				]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
						.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
						.ButtonStyle(_GetEditorStyle(), "NoBorder")
						.ContentPadding(0)
						.Visibility_Lambda([Idx, StringParams]()
							{
								for(auto& NextSelectedParam : StringParams)
								{
									if(!IsValidWeakPointer(NextSelectedParam))
										continue;

									if(!NextSelectedParam->IsDefaultValueAtIndex(Idx))
										return EVisibility::Visible;
								}

								return EVisibility::Hidden;
							})
						.OnClicked_Lambda([Idx, StringParams, RevertToDefault]()
							{ return RevertToDefault(Idx, StringParams); })
						[
							SNew(SImage)
								.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
						]
				];

			AddMultiParamWidgetsToBox(HorizontalBox, ExtraWidgets);

			VerticalBox->AddSlot().Padding(2, 2, 5, 2)
				[
					SNew(SAssetDropTarget)
						.OnAreAssetsAcceptableForDrop_Lambda([](TArrayView<FAssetData> InAssets)
							{return true;})
						.OnAssetsDropped_Lambda([=](const FDragDropEvent&, TArrayView<FAssetData> InAssets)
							{
								// Get the asset reference string for this object
								FString ReferenceStr = UHoudiniParameterString::GetAssetReference(InAssets[0].GetAsset());

								ChangeStringValueAt(ReferenceStr, nullptr, Idx, true, StringParams);
							})
						[
							HorizontalBox
						]
				];
		}

	}

	return VerticalBox;
}



TSharedRef<SWidget>
FHoudiniParameterView::CreateWidgetButton(
	const TArray<TWeakObjectPtr<UHoudiniParameterButton>>& ButtonParams,
	const TSharedPtr<SWidget>& ExtraWidgets)
{
	const TWeakObjectPtr<UHoudiniParameterButton>& MainParam = ButtonParams[0];

	FText ParameterLabelText = FText::FromString(MainParam->GetParameterLabel());
	FText ParameterTooltip = GetParameterTooltip(MainParam.Get());

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	TSharedPtr<SButton> Button;

	// Add button UI.
	HorizontalBox->AddSlot().Padding(1, 2, 4, 2)
		[
			SAssignNew(Button, SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Text(ParameterLabelText)
				.ToolTipText(ParameterTooltip)
				.OnClicked(FOnClicked::CreateLambda([MainParam, ButtonParams]()
					{
						for(auto& Param : ButtonParams)
						{
							if(!IsValidWeakPointer(Param))
								continue;

							// There is no undo redo operation for button
							Param->MarkChanged(true);
						}

						return FReply::Handled();
					}))
		];

	AddMultiParamWidgetsToBox(HorizontalBox, ExtraWidgets);

	return HorizontalBox;
}


TSharedRef<SWidget>
FHoudiniParameterView::CreateWidgetFile(
	const TArray<TWeakObjectPtr<UHoudiniParameterFile>>& FileParams,
	const TSharedPtr<SWidget>& ExtraWidgets)
{
	const TWeakObjectPtr<UHoudiniParameterFile>& MainParam = FileParams[0];

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	FString FileTypeWidgetFilter = TEXT("All files (*.*)|*.*");
	if(!MainParam->GetFileFilters().IsEmpty())
		FileTypeWidgetFilter = FString::Printf(TEXT("%s files (%s)|%s"), *MainParam->GetFileFilters(), *MainParam->GetFileFilters(), *MainParam->GetFileFilters());

	FString BrowseWidgetDirectory = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN);

	TMap<FString, FString>& Tags = MainParam->GetTags();
	if(Tags.Contains(HAPI_PARAM_TAG_DEFAULT_DIR))
	{
		if(!Tags[HAPI_PARAM_TAG_DEFAULT_DIR].IsEmpty())
		{
			FString DefaultDir = Tags[HAPI_PARAM_TAG_DEFAULT_DIR];
			if(FPaths::DirectoryExists(DefaultDir))
				BrowseWidgetDirectory = DefaultDir;
		}
	}

	auto UpdateCheckRelativePath = [MainParam](const FString& PickedPath)
		{
			if (!IsValidWeakPointer(MainParam))
				return PickedPath;

			UHoudiniAssetComponent* HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(MainParam->GetOuter());
			if(MainParam->GetOuter() && !PickedPath.IsEmpty() && FPaths::IsRelative(PickedPath))
			{
				// Check if the path is relative to the UE4 project
				FString AbsolutePath = FPaths::ConvertRelativePathToFull(PickedPath);
				if(FPaths::FileExists(AbsolutePath))
				{
					return AbsolutePath;
				}

				// Check if the path is relative to the asset
				if(IsValid(HoudiniAssetComponent))
				{
					if(IsValid(HoudiniAssetComponent->GetHoudiniAsset()))
					{
						FString AssetFilePath = FPaths::GetPath(HoudiniAssetComponent->GetHoudiniAsset()->AssetFileName);
						if(FPaths::FileExists(AssetFilePath))
						{
							FString UpdatedFileWidgetPath = FPaths::Combine(*AssetFilePath, *PickedPath);
							if(FPaths::FileExists(UpdatedFileWidgetPath))
							{
								return UpdatedFileWidgetPath;
							}
						}
					}
				}
			}

			return PickedPath;
		};

	for(int32 Idx = 0; Idx < MainParam->GetTupleSize(); ++Idx)
	{
		FString FileWidgetPath = MainParam->GetValueAt(Idx);
		FString FileWidgetBrowsePath = BrowseWidgetDirectory;

		if(!FileWidgetPath.IsEmpty())
		{
			FString FileWidgetDirPath = FPaths::GetPath(FileWidgetPath);
			if(!FileWidgetDirPath.IsEmpty())
				FileWidgetBrowsePath = FileWidgetDirPath;
		}

		bool IsDirectoryPicker = MainParam->GetParameterType() == EHoudiniParameterType::FileDir;
		bool bIsNewFile = !MainParam->IsReadOnly();

		FText BrowseTooltip = LOCTEXT("FileButtonToolTipText", "Choose a file from this computer");
		if(IsDirectoryPicker)
			BrowseTooltip = LOCTEXT("DirButtonToolTipText", "Choose a directory from this computer");

		VerticalBox->AddSlot().Padding(2, 2, 5, 2)
			[
				SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1.0f).MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
					[
						SNew(SNewFilePathPicker)
							.BrowseButtonImage(_GetEditorStyle().GetBrush("PropertyWindow.Button_Ellipsis"))
							.BrowseButtonStyle(_GetEditorStyle(), "HoverHintOnly")
							.BrowseButtonToolTip(BrowseTooltip)
							.BrowseDirectory(FileWidgetBrowsePath)
							.BrowseTitle(LOCTEXT("PropertyEditorTitle", "File picker..."))
							.FilePath(FileWidgetPath)
							.FileTypeFilter(FileTypeWidgetFilter)
							.IsNewFile(bIsNewFile)
							.IsDirectoryPicker(IsDirectoryPicker)
							.ToolTipText_Lambda([MainParam]()
								{
									// return the current param value as a tooltip
									FString FileValue = MainParam.IsValid() ? MainParam->GetValueAt(0) : FString();
									return FText::FromString(FileValue);
								})
							.OnPathPicked(FOnPathPicked::CreateLambda([MainParam, FileParams, UpdateCheckRelativePath, Idx](const FString& PickedPath)
								{
									if (!IsValidWeakPointer(MainParam))
										return;

									if(MainParam->GetNumValues() <= Idx)
										return;

									FScopedTransaction Transaction(
										TEXT(HOUDINI_MODULE_RUNTIME),
										LOCTEXT("HoudiniParameterFileChange", "Houdini Parameter File: Changing a file path"),
										MainParam->GetOuter(), true);

									bool bChanged = false;

									for(auto& Param : FileParams)
									{
										if(!IsValidWeakPointer(Param))
											continue;

										Param->Modify();
										if(Param->SetValueAt(UpdateCheckRelativePath(PickedPath), Idx))
										{
											bChanged = true;
											Param->MarkChanged(true);
										}
									}

									// Cancel the transaction if no value has actually been changed
									if(!bChanged)
									{
										Transaction.Cancel();
									}
								}))
					]
			];

	}

	TSharedRef<SHorizontalBox> HorizontalBox =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			VerticalBox
		];

	AddMultiParamWidgetsToBox(HorizontalBox, ExtraWidgets);

	return HorizontalBox;
}


TSharedRef<SWidget> FHoudiniParameterView::CreateWidgetButtonStrip(
	const TArray<TWeakObjectPtr<UHoudiniParameterButtonStrip>>& ButtonStripParams,
	const TSharedPtr<SWidget>& ExtraWidgets)
{
	const TWeakObjectPtr<UHoudiniParameterButtonStrip>& MainParam = ButtonStripParams[0];

	auto OnButtonStateChanged = [MainParam, ButtonStripParams](ECheckBoxState NewState, int32 Idx)
		{
			bool bChanged = false;

			for(auto& NextParam : ButtonStripParams)
			{
				if(!IsValidWeakPointer(NextParam))
					continue;

				if(NextParam->SetValueAt(NewState == ECheckBoxState::Checked, Idx))
				{
					NextParam->MarkChanged(true);
					bChanged = true;
				}
			}
		};


	FText ParameterLabelText = FText::FromString(MainParam->GetParameterLabel());
	FText ParameterTooltip = GetParameterTooltip(MainParam.Get());

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	FLinearColor BgColor(0.53f, 0.81f, 0.82f, 1.0f);   // Sky Blue Backgroud color

	for(uint32 Idx = 0; Idx < MainParam->GetNumValues(); ++Idx)
	{
		const FString* LabelString = MainParam->GetStringLabelAt(Idx);
		FText LabelText = LabelString ? FText::FromString(*LabelString) : FText();

		TSharedPtr<SCheckBox> Button;

		HorizontalBox->AddSlot().Padding(3.0f, 3.0f, 3.0f, 3.0f).FillWidth(1.0f)
			[
				SAssignNew(Button, SCheckBox)
					.Style(_GetEditorStyle(), "Property.ToggleButton.Middle")
					.IsChecked(TAttribute<ECheckBoxState>::CreateLambda(
						[MainParam, Idx]() -> ECheckBoxState
						{
							if(!IsValidWeakPointer(MainParam))
							{
								return ECheckBoxState::Undetermined;
							}

							return MainParam->GetValueAt(Idx)
								? ECheckBoxState::Checked
								: ECheckBoxState::Unchecked;
						}))
					.OnCheckStateChanged_Lambda([OnButtonStateChanged, Idx](ECheckBoxState NewState)
						{
							OnButtonStateChanged(NewState, Idx);
						})
					.Content()
					[
						SNew(SBox)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
									.Text(LabelText)
									.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							]
					]
			];

		Button->SetColorAndOpacity(BgColor);
	}

	AddMultiParamWidgetsToBox(HorizontalBox, ExtraWidgets);

	return HorizontalBox;
}


TSharedRef<SWidget>
FHoudiniParameterView::CreateWidgetLabel(
	const TArray<TWeakObjectPtr<UHoudiniParameterLabel>>& LabelParams,
	const TSharedPtr<SWidget>& ExtraWidgets)
{
	const TWeakObjectPtr<UHoudiniParameterLabel>& MainParam = LabelParams[0];

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	for(int32 Index = 0; Index < MainParam->GetTupleSize(); ++Index)
	{
		FString NextLabelString;
		if (MainParam->LabelStrings.IsValidIndex(Index))
			NextLabelString = MainParam->LabelStrings[Index];
		FText ParameterLabelText = FText::FromString(NextLabelString);

		FString Expression;
		if (MainParam->ExpressionStrings.IsValidIndex(Index))
		{
			Expression = MainParam->ExpressionStrings[Index];
		}
		else
		{
			Expression = NextLabelString;
		}

		FString Tooltip = FString::Printf(TEXT("Column %d: %s"), Index, *Expression);
		FText ParamTooltipText = FText::FromString(Tooltip);

		TSharedPtr<STextBlock> TextBlock;

		HorizontalBox->AddSlot()
			.Padding(4, 8)
			.FillWidth(1.0)
			[
				SAssignNew(TextBlock, STextBlock)
					.Text(ParameterLabelText)
					.ToolTipText(ParamTooltipText)
					.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			];
	}

	AddMultiParamWidgetsToBox(HorizontalBox, ExtraWidgets);

	return HorizontalBox;
}

void FHoudiniParameterView::AddMultiParamWidgetsToBox(
	const TSharedRef<SHorizontalBox>& Box,
	const TSharedPtr<SWidget>& ExtraWidgets)
{
	Box->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			ExtraWidgets.IsValid() ? ExtraWidgets.ToSharedRef() : SNullWidget::NullWidget
		];
}

#undef LOCTEXT_NAMESPACE
