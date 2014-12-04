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


UHoudiniAssetParameterToggle::UHoudiniAssetParameterToggle(const FPostConstructInitializeProperties& PCIP) :
	Super(PCIP)
{
	// Parameter will have at least one value.
	Values.AddZeroed(1);
}


UHoudiniAssetParameterToggle::~UHoudiniAssetParameterToggle()
{

}


UHoudiniAssetParameterToggle*
UHoudiniAssetParameterToggle::Create(UHoudiniAssetComponent* InHoudiniAssetComponent, UHoudiniAssetParameter* InParentParameter, 
									 HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
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

	UHoudiniAssetParameterToggle* HoudiniAssetParameterToggle = NewObject<UHoudiniAssetParameterToggle>(Outer);

	HoudiniAssetParameterToggle->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);
	return HoudiniAssetParameterToggle;
}


bool
UHoudiniAssetParameterToggle::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent, UHoudiniAssetParameter* InParentParameter, 
											  HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
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
	if(HAPI_RESULT_SUCCESS != HAPI_GetParmIntValues(InNodeId, &Values[0], ValuesIndex, TupleSize))
	{
		return false;
	}

	// Min and max make no sense for this type of parameter.
	return true;
}


void
UHoudiniAssetParameterToggle::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	Super::CreateWidget(DetailCategoryBuilder);

	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(TEXT(""));

	Row.NameWidget.Widget = SNew(STextBlock)
							.Text(FString(""))
							.ToolTipText(GetParameterLabel())
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	for(int32 Idx = 0; Idx < TupleSize; ++Idx)
	{
		VerticalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SNew(SCheckBox)
				.OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this, &UHoudiniAssetParameterToggle::CheckStateChanged, Idx))
				.IsChecked(TAttribute<ESlateCheckBoxState::Type>::Create(TAttribute<ESlateCheckBoxState::Type>::FGetter::CreateUObject(this, &UHoudiniAssetParameterToggle::IsChecked, Idx)))
				.Content()
				[
					SNew(STextBlock)
					.Text(GetParameterLabel())
					.ToolTipText(GetParameterLabel())
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
		];
	}

	Row.ValueWidget.Widget = VerticalBox;
	Row.ValueWidget.MinDesiredWidth(FHoudiniAssetComponentDetails::RowValueWidgetDesiredWidth);
}


void
UHoudiniAssetParameterToggle::CreateWidget(TSharedPtr<SVerticalBox> VerticalBox)
{
	Super::CreateWidget(VerticalBox);

	for(int32 Idx = 0; Idx < TupleSize; ++Idx)
	{
		VerticalBox->AddSlot().Padding(0, 2, 0, 2)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().MaxWidth(8)
			[
				SNew(STextBlock)
				.Text(FString(""))
				.ToolTipText(GetParameterLabel())
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			+SHorizontalBox::Slot()
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this, &UHoudiniAssetParameterToggle::CheckStateChanged, Idx))
				.IsChecked(TAttribute<ESlateCheckBoxState::Type>::Create(TAttribute<ESlateCheckBoxState::Type>::FGetter::CreateUObject(this, &UHoudiniAssetParameterToggle::IsChecked, Idx)))
				.Content()
				[
					SNew(STextBlock)
					.Text(GetParameterLabel())
					.ToolTipText(GetParameterLabel())
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
			]
		];
	}
}


bool
UHoudiniAssetParameterToggle::UploadParameterValue()
{
	if(HAPI_RESULT_SUCCESS != HAPI_SetParmIntValues(NodeId, &Values[0], ValuesIndex, TupleSize))
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

	if(Ar.IsLoading())
	{
		Values.Empty();
	}

	Ar << Values;
}


void
UHoudiniAssetParameterToggle::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}


void
UHoudiniAssetParameterToggle::CheckStateChanged(ESlateCheckBoxState::Type NewState, int32 Idx)
{
	int32 bState = (ESlateCheckBoxState::Checked == NewState);

	if(Values[Idx] != bState)
	{
		MarkPreChanged();

		Values[Idx] = bState;

		// Mark this parameter as changed.
		MarkChanged();
	}
}


ESlateCheckBoxState::Type
UHoudiniAssetParameterToggle::IsChecked(int32 Idx) const
{
	if(Values[Idx])
	{
		return ESlateCheckBoxState::Checked;
	}
	
	return ESlateCheckBoxState::Unchecked;
}

