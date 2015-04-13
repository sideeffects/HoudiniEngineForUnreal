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

#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniAssetParameterChoice.h"


UHoudiniAssetParameterChoice::UHoudiniAssetParameterChoice(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	CurrentValue(0),
	bStringChoiceList(false)
{
	StringValue = TEXT("");
}


UHoudiniAssetParameterChoice::~UHoudiniAssetParameterChoice()
{

}


UHoudiniAssetParameterChoice*
UHoudiniAssetParameterChoice::Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	UObject* Outer = InHoudiniAssetComponent;
	if(!Outer)
	{
		Outer = InParentParameter;
		if(!Outer)
		{
			// Must have either component or parent not null.
			check(false);
		}
	}

	UHoudiniAssetParameterChoice* HoudiniAssetParameterChoice = NewObject<UHoudiniAssetParameterChoice>(Outer);

	HoudiniAssetParameterChoice->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);
	return HoudiniAssetParameterChoice;
}


bool
UHoudiniAssetParameterChoice::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo))
	{
		return false;
	}

	// Choice lists can be only integer or string.
	if(HAPI_PARMTYPE_INT != ParmInfo.type && HAPI_PARMTYPE_STRING != ParmInfo.type)
	{
		return false;
	}

	// Get the actual value for this property.
	CurrentValue = 0;

	if(HAPI_PARMTYPE_INT == ParmInfo.type)
	{
		// This is an integer choice list.
		bStringChoiceList = false;

		// Assign internal Hapi values index.
		SetValuesIndex(ParmInfo.intValuesIndex);

		if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmIntValues(NodeId, &CurrentValue, ValuesIndex, TupleSize))
		{
			return false;
		}
	}
	else if(HAPI_PARMTYPE_STRING == ParmInfo.type)
	{
		// This is a string choice list.
		bStringChoiceList = true;

		// Assign internal Hapi values index.
		SetValuesIndex(ParmInfo.stringValuesIndex);

		HAPI_StringHandle StringHandle;
		if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmStringValues(NodeId, false, &StringHandle, ValuesIndex, TupleSize))
		{
			return false;
		}

		// Get the actual string value.
		if(!FHoudiniEngineUtils::GetHoudiniString(StringHandle, StringValue))
		{
			return false;
		}
	}

	// Get choice descriptors.
	TArray<HAPI_ParmChoiceInfo> ParmChoices;
	ParmChoices.SetNumZeroed(ParmInfo.choiceCount);
	if(HAPI_RESULT_SUCCESS !=
		FHoudiniApi::GetParmChoiceLists(NodeId, &ParmChoices[0], ParmInfo.choiceIndex, ParmInfo.choiceCount))
	{
		return false;
	}

	// Get string values for all available choices.
	StringChoiceValues.Empty();
	StringChoiceLabels.Empty();

	bool bMatchedSelectionLabel = false;
	for(int32 ChoiceIdx = 0; ChoiceIdx < ParmChoices.Num(); ++ChoiceIdx)
	{
		FString* ChoiceValue = new FString();
		FString* ChoiceLabel = new FString();

		if(!FHoudiniEngineUtils::GetHoudiniString(ParmChoices[ChoiceIdx].valueSH, *ChoiceValue))
		{
			return false;
		}

		StringChoiceValues.Add(TSharedPtr<FString>(ChoiceValue));

		if(!FHoudiniEngineUtils::GetHoudiniString(ParmChoices[ChoiceIdx].labelSH, *ChoiceLabel))
		{
			*ChoiceLabel = *ChoiceValue;
		}

		StringChoiceLabels.Add(TSharedPtr<FString>(ChoiceLabel));

		// If this is a string choice list, we need to match name with corresponding selection label.
		if(bStringChoiceList && !bMatchedSelectionLabel && ChoiceValue->Equals(StringValue))
		{
			StringValue = *ChoiceLabel;
			bMatchedSelectionLabel = true;
			CurrentValue = ChoiceIdx;
		}
		else if(!bStringChoiceList && ChoiceIdx == CurrentValue)
		{
			StringValue = *ChoiceLabel;
		}
	}

	return true;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterChoice::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	Super::CreateWidget(DetailCategoryBuilder);

	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());

	Row.NameWidget.Widget = SNew(STextBlock)
							.Text(GetParameterLabel())
							.ToolTipText(GetParameterLabel())
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	TSharedPtr<SComboBox<TSharedPtr<FString> > > ComboBox;

	HorizontalBox->AddSlot().Padding(2, 2, 5, 2)
	[
		SAssignNew(ComboBox, SComboBox<TSharedPtr<FString> >)
		.OptionsSource(&StringChoiceLabels)
		.InitiallySelectedItem(StringChoiceLabels[CurrentValue])
		.OnGenerateWidget(SComboBox<TSharedPtr<FString> >::FOnGenerateWidget::CreateUObject(this,
			&UHoudiniAssetParameterChoice::CreateChoiceEntryWidget))
		.OnSelectionChanged(SComboBox<TSharedPtr<FString> >::FOnSelectionChanged::CreateUObject(this,
			&UHoudiniAssetParameterChoice::OnChoiceChange))
		[
			SNew(STextBlock)
			.Text(TAttribute<FString>::Create(TAttribute<FString>::FGetter::CreateUObject(this,
				&UHoudiniAssetParameterChoice::HandleChoiceContentText)))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
	];

	if(ComboBox.IsValid())
	{
		ComboBox->SetEnabled(!bIsDisabled);
	}

	Row.ValueWidget.Widget = HorizontalBox;
	Row.ValueWidget.MinDesiredWidth(FHoudiniAssetComponentDetails::RowValueWidgetDesiredWidth);
}


void
UHoudiniAssetParameterChoice::CreateWidget(TSharedPtr<SVerticalBox> VerticalBox)
{
	Super::CreateWidget(VerticalBox);

	VerticalBox->AddSlot().Padding(2, 2, 2, 2)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot().MaxWidth(80).Padding(7, 1, 0, 0).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(GetParameterLabel())
			.ToolTipText(GetParameterLabel())
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		+SHorizontalBox::Slot()
		[
			SNew(SComboBox<TSharedPtr<FString> >)
			.OptionsSource(&StringChoiceLabels)
			.InitiallySelectedItem(StringChoiceLabels[CurrentValue])
			.OnGenerateWidget(SComboBox<TSharedPtr<FString> >::FOnGenerateWidget::CreateUObject(this,
				&UHoudiniAssetParameterChoice::CreateChoiceEntryWidget))
			.OnSelectionChanged(SComboBox<TSharedPtr<FString> >::FOnSelectionChanged::CreateUObject(this,
				&UHoudiniAssetParameterChoice::OnChoiceChange))
			[
				SNew(STextBlock)
				.Text(TAttribute<FString>::Create(TAttribute<FString>::FGetter::CreateUObject(this,
					&UHoudiniAssetParameterChoice::HandleChoiceContentText)))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
	];
}

#endif


bool
UHoudiniAssetParameterChoice::UploadParameterValue()
{
	if(bStringChoiceList)
	{
		// Get corresponding value.
		FString* ChoiceValue = StringChoiceValues[CurrentValue].Get();
		std::string String = TCHAR_TO_UTF8(*(*ChoiceValue));
		FHoudiniApi::SetParmStringValue(NodeId, String.c_str(), ParmId, 0);
	}
	else
	{
		// This is an int choice list.
		FHoudiniApi::SetParmIntValues(NodeId, &CurrentValue, ValuesIndex, TupleSize);
	}

	return Super::UploadParameterValue();
}


void
UHoudiniAssetParameterChoice::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	if(Ar.IsLoading())
	{
		StringChoiceValues.Empty();
		StringChoiceLabels.Empty();
	}

	int32 NumChoices = StringChoiceValues.Num();
	Ar << NumChoices;

	int32 NumLabels = StringChoiceLabels.Num();
	Ar << NumLabels;

	if(Ar.IsSaving())
	{
		for(int32 ChoiceIdx = 0; ChoiceIdx < StringChoiceValues.Num(); ++ChoiceIdx)
		{
			Ar << *(StringChoiceValues[ChoiceIdx].Get());
		}

		for(int32 LabelIdx = 0; LabelIdx < StringChoiceLabels.Num(); ++LabelIdx)
		{
			Ar << *(StringChoiceLabels[LabelIdx].Get());
		}
	}
	else if(Ar.IsLoading())
	{
		FString Temp;

		for(int32 ChoiceIdx = 0; ChoiceIdx < NumChoices; ++ChoiceIdx)
		{
			Ar << Temp;
			FString* ChoiceName = new FString(Temp);
			StringChoiceValues.Add(TSharedPtr<FString>(ChoiceName));
		}

		for(int32 LabelIdx = 0; LabelIdx < NumLabels; ++LabelIdx)
		{
			Ar << Temp;
			FString* ChoiceLabel = new FString(Temp);
			StringChoiceLabels.Add(TSharedPtr<FString>(ChoiceLabel));
		}
	}

	Ar << StringValue;
	Ar << CurrentValue;

	Ar << bStringChoiceList;
}


#if WITH_EDITOR

TSharedRef<SWidget>
UHoudiniAssetParameterChoice::CreateChoiceEntryWidget(TSharedPtr<FString> ChoiceEntry)
{
	return SNew(STextBlock)
		   .Text(*ChoiceEntry)
		   .ToolTipText(*ChoiceEntry)
		   .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
}

#endif


void
UHoudiniAssetParameterChoice::OnChoiceChange(TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
{
	if(!NewChoice.IsValid())
	{
		return;
	}

	bool bChanged = false;
	StringValue = *(NewChoice.Get());

	// We need to match selection based on label.
	int32 LabelIdx = 0;
	for(; LabelIdx < StringChoiceLabels.Num(); ++LabelIdx)
	{
		FString* ChoiceLabel = StringChoiceLabels[LabelIdx].Get();

		if(ChoiceLabel->Equals(StringValue))
		{
			bChanged = true;
			break;
		}
	}

	if(bChanged)
	{
		MarkPreChanged();

		CurrentValue = LabelIdx;

		// Mark this property as changed.
		MarkChanged();
	}
}


FString
UHoudiniAssetParameterChoice::HandleChoiceContentText() const
{
	return StringValue;
}
