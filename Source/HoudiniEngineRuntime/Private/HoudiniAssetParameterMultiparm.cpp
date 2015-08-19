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
#include "HoudiniAssetParameterMultiparm.h"
#include "HoudiniApi.h"


UHoudiniAssetParameterMultiparm::UHoudiniAssetParameterMultiparm(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	Value(0)
{

}


UHoudiniAssetParameterMultiparm::~UHoudiniAssetParameterMultiparm()
{

}


UHoudiniAssetParameterMultiparm*
UHoudiniAssetParameterMultiparm::Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
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

	UHoudiniAssetParameterMultiparm* HoudiniAssetParameterMultiparm = NewObject<UHoudiniAssetParameterMultiparm>(Outer,
		UHoudiniAssetParameterMultiparm::StaticClass(), NAME_None, RF_Public);

	HoudiniAssetParameterMultiparm->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);
	return HoudiniAssetParameterMultiparm;
}


bool
UHoudiniAssetParameterMultiparm::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo))
	{
		return false;
	}

	if(HAPI_PARMTYPE_MULTIPARMLIST != ParmInfo.type)
	{
		return false;
	}

	// Assign internal Hapi values index.
	SetValuesIndex(ParmInfo.intValuesIndex);

	// Get the actual value for this property.
	Value = 0;
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmIntValues(nullptr, InNodeId, &Value, ValuesIndex, 1))
	{
		return false;
	}

	return true;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterMultiparm::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());
	FText ParameterLabelText = FText::FromString(GetParameterLabel());

	Row.NameWidget.Widget = SNew(STextBlock)
							.Text(ParameterLabelText)
							.ToolTipText(ParameterLabelText)
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	TSharedPtr<SNumericEntryBox<int32> > NumericEntryBox;

	HorizontalBox->AddSlot().Padding(2, 2, 5, 2)
	[
		SAssignNew(NumericEntryBox, SNumericEntryBox<int32>)
		.AllowSpin(true)

		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))

		.Value(TAttribute<TOptional<int32> >::Create(TAttribute<TOptional<int32> >::FGetter::CreateUObject(this,
			&UHoudiniAssetParameterMultiparm::GetValue)))
		.OnValueChanged(SNumericEntryBox<int32>::FOnValueChanged::CreateUObject(this,
			&UHoudiniAssetParameterMultiparm::SetValue))
		.OnValueCommitted(SNumericEntryBox<int32>::FOnValueCommitted::CreateUObject(this,
			&UHoudiniAssetParameterMultiparm::SetValueCommitted))
	];

	HorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
	[
		PropertyCustomizationHelpers::MakeAddButton(
			FSimpleDelegate::CreateUObject(this, &UHoudiniAssetParameterMultiparm::AddElement))
	];

	HorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
	[
		PropertyCustomizationHelpers::MakeRemoveButton(
			FSimpleDelegate::CreateUObject(this, &UHoudiniAssetParameterMultiparm::RemoveElement))
	];

	HorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
	[
		PropertyCustomizationHelpers::MakeEmptyButton(
			FSimpleDelegate::CreateUObject(this, &UHoudiniAssetParameterMultiparm::SetValue, 0))
	];

	if(NumericEntryBox.IsValid())
	{
		NumericEntryBox->SetEnabled(!bIsDisabled);
	}

	Row.ValueWidget.Widget = HorizontalBox;
	Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);

	Super::CreateWidget(DetailCategoryBuilder);
}

#endif

bool
UHoudiniAssetParameterMultiparm::UploadParameterValue()
{
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmIntValues(FHoudiniEngine::Get().GetSession(), NodeId, &Value, ValuesIndex, 1))
	{
		return false;
	}

	return Super::UploadParameterValue();
}

#if WITH_EDITOR

TOptional<int32>
UHoudiniAssetParameterMultiparm::GetValue() const
{
	return TOptional<int32>(Value);
}


void
UHoudiniAssetParameterMultiparm::SetValue(int32 InValue)
{
	if(Value != InValue)
	{
		MarkPreChanged();

		Value = InValue;

		// Mark this parameter as changed.
		MarkChanged();

		// Record undo state.
		RecordUndoState();
	}
}


void
UHoudiniAssetParameterMultiparm::SetValueCommitted(int32 InValue, ETextCommit::Type CommitType)
{

}

void
UHoudiniAssetParameterMultiparm::AddElement()
{
	MarkPreChanged();
	Value++;
	MarkChanged();
	RecordUndoState();
}

void
UHoudiniAssetParameterMultiparm::RemoveElement()
{
	MarkPreChanged();
	Value--;
	MarkChanged();
	RecordUndoState();
}

#endif

void
UHoudiniAssetParameterMultiparm::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	if(Ar.IsLoading())
	{
		Value = 0;
	}

	Ar << Value;
}

