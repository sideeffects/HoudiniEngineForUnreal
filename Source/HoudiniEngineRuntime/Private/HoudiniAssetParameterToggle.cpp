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

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetParameterToggle.h"
#include "HoudiniApi.h"


UHoudiniAssetParameterToggle::UHoudiniAssetParameterToggle(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	// Parameter will have at least one value.
	Values.AddZeroed(1);
}


UHoudiniAssetParameterToggle::~UHoudiniAssetParameterToggle()
{

}


UHoudiniAssetParameterToggle*
UHoudiniAssetParameterToggle::Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
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

	UHoudiniAssetParameterToggle* HoudiniAssetParameterToggle = NewObject<UHoudiniAssetParameterToggle>(Outer,
		UHoudiniAssetParameterToggle::StaticClass(), NAME_None, RF_Public | RF_Transactional);

	HoudiniAssetParameterToggle->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);
	return HoudiniAssetParameterToggle;
}


bool
UHoudiniAssetParameterToggle::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo))
	{
		return false;
	}

	// We can only handle toggle type.
	if(HAPI_PARMTYPE_TOGGLE != ParmInfo.type)
	{
		return false;
	}

	// Assign internal Hapi values index.
	SetValuesIndex(ParmInfo.intValuesIndex);

	// Get the actual value for this property.
	Values.SetNumZeroed(TupleSize);
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmIntValues(
		FHoudiniEngine::Get().GetSession(), InNodeId, &Values[0], ValuesIndex, TupleSize))
	{
		return false;
	}

	// Min and max make no sense for this type of parameter.
	return true;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterToggle::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	Super::CreateWidget(DetailCategoryBuilder);

	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());
	FText ParameterLabelText = FText::FromString(GetParameterLabel());

	Row.NameWidget.Widget = SNew(STextBlock)
							.Text(FText::GetEmpty())
							.ToolTipText(ParameterLabelText)
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	for(int32 Idx = 0; Idx < TupleSize; ++Idx)
	{
		TSharedPtr<SCheckBox> CheckBox;

		VerticalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SAssignNew(CheckBox, SCheckBox)
			.OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this,
				&UHoudiniAssetParameterToggle::CheckStateChanged, Idx))
			.IsChecked(TAttribute<ECheckBoxState>::Create(
				TAttribute<ECheckBoxState>::FGetter::CreateUObject(this,
					&UHoudiniAssetParameterToggle::IsChecked, Idx)))
			.Content()
			[
				SNew(STextBlock)
				.Text(ParameterLabelText)
				.ToolTipText(ParameterLabelText)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];

		if(CheckBox.IsValid())
		{
			CheckBox->SetEnabled(!bIsDisabled);
		}
	}

	Row.ValueWidget.Widget = VerticalBox;
	Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
}


void
UHoudiniAssetParameterToggle::CreateWidget(TSharedPtr<SVerticalBox> VerticalBox)
{
	Super::CreateWidget(VerticalBox);
	FText ParameterLabelText = FText::FromString(GetParameterLabel());

	for(int32 Idx = 0; Idx < TupleSize; ++Idx)
	{
		VerticalBox->AddSlot().Padding(0, 2, 0, 2)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().MaxWidth(8)
			[
				SNew(STextBlock)
				.Text(FText::GetEmpty())
				.ToolTipText(ParameterLabelText)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			+SHorizontalBox::Slot()
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this,
					&UHoudiniAssetParameterToggle::CheckStateChanged, Idx))
				.IsChecked(TAttribute<ECheckBoxState>::Create(
					TAttribute<ECheckBoxState>::FGetter::CreateUObject(this,
						&UHoudiniAssetParameterToggle::IsChecked, Idx)))
				.Content()
				[
					SNew(STextBlock)
					.Text(ParameterLabelText)
					.ToolTipText(ParameterLabelText)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
			]
		];
	}
}

#endif


bool
UHoudiniAssetParameterToggle::UploadParameterValue()
{
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmIntValues(
		FHoudiniEngine::Get().GetSession(), NodeId, &Values[0], ValuesIndex, TupleSize))
	{
		return false;
	}

	return Super::UploadParameterValue();
}


void
UHoudiniAssetParameterToggle::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar << Values;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterToggle::CheckStateChanged(ECheckBoxState NewState, int32 Idx)
{
	int32 bState = (ECheckBoxState::Checked == NewState);

	if(Values[Idx] != bState)
	{
		MarkPreChanged();

		Values[Idx] = bState;

		// Mark this parameter as changed.
		MarkChanged();

		// We want to record undo information when toggle changes state.
		RecordUndoState();
	}
}


ECheckBoxState
UHoudiniAssetParameterToggle::IsChecked(int32 Idx) const
{
	if(Values[Idx])
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}

#endif
